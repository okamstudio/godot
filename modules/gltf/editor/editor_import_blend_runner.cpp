/**************************************************************************/
/*  editor_import_blend_runner.cpp                                        */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#include "editor_import_blend_runner.h"

#ifdef TOOLS_ENABLED

#include "core/io/http_client.h"
#include "editor/editor_file_system.h"
#include "editor/editor_node.h"
#include "editor/editor_settings.h"

static constexpr char PYTHON_SCRIPT_RPC[] = R"(
import bpy, sys, threading
from xmlrpc.server import SimpleXMLRPCServer
req = threading.Condition()
res = threading.Condition()
info = None
export_err = None
def xmlrpc_server():
  server = SimpleXMLRPCServer(('127.0.0.1', %d))
  server.register_function(export_gltf)
  server.serve_forever()
def export_gltf(opts):
  with req:
    global info
    info = ('export_gltf', opts)
    req.notify()
  with res:
    res.wait()
  if export_err:
    raise export_err
  # Important to return a value to prevent the error 'cannot marshal None unless allow_none is enabled'.
  return 'BLENDER_GODOT_EXPORT_SUCCESSFUL'
if bpy.app.version < (3, 0, 0):
  print('Blender 3.0 or higher is required.', file=sys.stderr)
threading.Thread(target=xmlrpc_server).start()
while True:
  with req:
    while info is None:
      req.wait()
  method, opts = info
  if method == 'export_gltf':
    try:
      export_err = None
      bpy.ops.wm.open_mainfile(filepath=opts['path'])
      if opts['unpack_all']:
        bpy.ops.file.unpack_all(method='USE_LOCAL')
      bpy.ops.export_scene.gltf(**opts['gltf_options'])
    except Exception as e:
      export_err = e
  info = None
  with res:
    res.notify()
)";

static constexpr char PYTHON_SCRIPT_DIRECT[] = R"(
import bpy, sys
opts = %s
if bpy.app.version < (3, 0, 0):
  print('Blender 3.0 or higher is required.', file=sys.stderr)
bpy.ops.wm.open_mainfile(filepath=opts['path'])
if opts['unpack_all']:
  bpy.ops.file.unpack_all(method='USE_LOCAL')
bpy.ops.export_scene.gltf(**opts['gltf_options'])
)";

String dict_to_python(const Dictionary &p_dict) {
	String entries;
	List<Variant> dict_keys;
	p_dict.get_key_list(&dict_keys);
	for (const String key : dict_keys) {
		String value;
		Variant raw_value = p_dict[key];

		switch (raw_value.get_type()) {
			case Variant::Type::BOOL: {
				value = raw_value ? "True" : "False";
				break;
			}
			case Variant::Type::STRING:
			case Variant::Type::STRING_NAME: {
				value = raw_value;
				value = vformat("'%s'", value.c_escape());
				break;
			}
			case Variant::Type::DICTIONARY: {
				value = dict_to_python(raw_value);
				break;
			}
			default: {
				ERR_FAIL_V_MSG("", vformat("Unhandled Variant type %s for python dictionary", Variant::get_type_name(raw_value.get_type())));
			}
		}

		entries += vformat("'%s': %s,", key, value);
	}
	return vformat("{%s}", entries);
}

String dict_to_xmlrpc(const Dictionary &p_dict) {
	String members;
	List<Variant> dict_keys;
	p_dict.get_key_list(&dict_keys);
	for (const String key : dict_keys) {
		String value;
		Variant raw_value = p_dict[key];

		switch (raw_value.get_type()) {
			case Variant::Type::BOOL: {
				value = vformat("<boolean>%d</boolean>", raw_value ? 1 : 0);
				break;
			}
			case Variant::Type::STRING:
			case Variant::Type::STRING_NAME: {
				value = raw_value;
				value = vformat("<string>%s</string>", value.xml_escape());
				break;
			}
			case Variant::Type::DICTIONARY: {
				value = dict_to_xmlrpc(raw_value);
				break;
			}
			default: {
				ERR_FAIL_V_MSG("", vformat("Unhandled Variant type %s for XMLRPC", Variant::get_type_name(raw_value.get_type())));
			}
		}

		members += vformat("<member><name>%s</name><value>%s</value></member>", key, value);
	}
	return vformat("<struct>%s</struct>", members);
}

Error EditorImportBlendRunner::start_blender(const String &p_python_script, bool p_blocking) {
	String blender_path = EDITOR_GET("filesystem/import/blender/blender_path");

	List<String> args;
	args.push_back("--background");
	args.push_back("--python-expr");
	args.push_back(p_python_script);

	Error err;
	if (p_blocking) {
		int exitcode = 0;
		err = OS::get_singleton()->execute(blender_path, args, nullptr, &exitcode);
		if (exitcode != 0) {
			return FAILED;
		}
	} else {
		err = OS::get_singleton()->create_process(blender_path, args, &blender_pid);
	}
	return err;
}

Error EditorImportBlendRunner::do_import(const Dictionary &p_options) {
	if (is_using_rpc()) {
		Error err = do_import_rpc(p_options);
		if (err != OK) {
			// Retry without using RPC (slow, but better than the import failing completely).
			if (err == ERR_CONNECTION_ERROR) {
				// Disable RPC if the connection could not be established.
				print_error(vformat("Failed to connect to Blender via RPC, switching to direct imports of .blend files. Check your proxy and firewall settings, then RPC can be re-enabled by changing the editor setting `filesystem/import/blender/rpc_port` to %d.", rpc_port));
				EditorSettings::get_singleton()->set_manually("filesystem/import/blender/rpc_port", 0);
				rpc_port = 0;
			}
			if (err != ERR_QUERY_FAILED) {
				err = do_import_direct(p_options);
			}
		}
		return err;
	} else {
		return do_import_direct(p_options);
	}
}

HTTPClient::Status EditorImportBlendRunner::connect_blender_rpc(const Ref<HTTPClient> &p_client, int p_timeout_usecs) {
	p_client->connect_to_host("127.0.0.1", rpc_port);
	HTTPClient::Status status = p_client->get_status();

	int attempts = 1;
	int wait_usecs = 1000;

	bool done = false;
	while (!done) {
		OS::get_singleton()->delay_usec(wait_usecs);
		status = p_client->get_status();
		switch (status) {
			case HTTPClient::STATUS_RESOLVING:
			case HTTPClient::STATUS_CONNECTING: {
				p_client->poll();
				break;
			}
			case HTTPClient::STATUS_CONNECTED: {
				done = true;
				break;
			}
			default: {
				if (attempts * wait_usecs < p_timeout_usecs) {
					p_client->connect_to_host("127.0.0.1", rpc_port);
				} else {
					return status;
				}
			}
		}
	}

	return status;
}

Error EditorImportBlendRunner::do_import_rpc(const Dictionary &p_options) {
	kill_timer->stop();

	// Start Blender if not already running.
	if (!is_running()) {
		// Start an XML RPC server on the given port.
		String python = vformat(PYTHON_SCRIPT_RPC, rpc_port);
		Error err = start_blender(python, false);
		if (err != OK || blender_pid == 0) {
			return FAILED;
		}
	}

	// Convert options to XML body.
	String xml_options = dict_to_xmlrpc(p_options);
	String xml_body = vformat("<?xml version=\"1.0\"?><methodCall><methodName>export_gltf</methodName><params><param><value>%s</value></param></params></methodCall>", xml_options);

	// Connect to RPC server.
	Ref<HTTPClient> client = HTTPClient::create();
	HTTPClient::Status status = connect_blender_rpc(client, 1000000);
	if (status != HTTPClient::STATUS_CONNECTED) {
		ERR_FAIL_V_MSG(ERR_CONNECTION_ERROR, vformat("Unexpected status during RPC connection: %d", status));
	}

	// Send XML request.
	PackedByteArray xml_buffer = xml_body.to_utf8_buffer();
	Error err = client->request(HTTPClient::METHOD_POST, "/", Vector<String>(), xml_buffer.ptr(), xml_buffer.size());
	if (err != OK) {
		ERR_FAIL_V_MSG(err, vformat("Unable to send RPC request: %d", err));
	}

	// Wait for response.
	bool done = false;
	PackedByteArray response;
	while (!done) {
		status = client->get_status();
		switch (status) {
			case HTTPClient::STATUS_REQUESTING: {
				client->poll();
				break;
			}
			case HTTPClient::STATUS_BODY: {
				client->poll();
				response.append_array(client->read_response_body_chunk());
				break;
			}
			case HTTPClient::STATUS_CONNECTED: {
				done = true;
				break;
			}
			default: {
				ERR_FAIL_V_MSG(ERR_CONNECTION_ERROR, vformat("Unexpected status during RPC response: %d", status));
			}
		}
	}

	String response_text = "No response from Blender.";
	if (response.size() > 0) {
		response_text = String::utf8((const char *)response.ptr(), response.size());
	}

	if (client->get_response_code() != HTTPClient::RESPONSE_OK) {
		ERR_FAIL_V_MSG(ERR_QUERY_FAILED, vformat("Error received from Blender - status code: %s, error: %s", client->get_response_code(), response_text));
	} else if (response_text.find("BLENDER_GODOT_EXPORT_SUCCESSFUL") < 0) {
		// Previous versions of Godot used a Python script where the RPC function did not return
		// a value, causing the error 'cannot marshal None unless allow_none is enabled'.
		// If an older version of Godot is running and has started Blender with this script,
		// we will receive the error, but there's a good chance that the import was successful.
		// We are discarding this error to maintain backward compatibility and prevent situations
		// where the user needs to close the older version of Godot or kill Blender.
		if (response_text.find("cannot marshal None unless allow_none is enabled") < 0) {
			String error_message;
			if (_extract_error_message_xml(response, error_message)) {
				ERR_FAIL_V_MSG(ERR_QUERY_FAILED, vformat("Blender exportation failed: %s", error_message));
			} else {
				ERR_FAIL_V_MSG(ERR_QUERY_FAILED, vformat("Blender exportation failed: %s", response_text));
			}
		}
	}

	return OK;
}

bool EditorImportBlendRunner::_extract_error_message_xml(const Vector<uint8_t> &p_response_data, String &r_error_message) {
	// Based on RPC Xml spec from: https://xmlrpc.com/spec.md
	Ref<XMLParser> parser = memnew(XMLParser);
	Error err = parser->open_buffer(p_response_data);
	if (err) {
		return false;
	}

	r_error_message = String();
	while (parser->read() == OK) {
		if (parser->get_node_type() == XMLParser::NODE_TEXT) {
			if (parser->get_node_data().size()) {
				if (r_error_message.size()) {
					r_error_message += " ";
				}
				r_error_message += parser->get_node_data().trim_suffix("\n");
			}
		}
	}

	return r_error_message.size();
}

Error EditorImportBlendRunner::do_import_direct(const Dictionary &p_options) {
	// Export glTF directly.
	String python = vformat(PYTHON_SCRIPT_DIRECT, dict_to_python(p_options));
	Error err = start_blender(python, true);
	if (err != OK) {
		return err;
	}

	return OK;
}

void EditorImportBlendRunner::_resources_reimported(const PackedStringArray &p_files) {
	if (is_running()) {
		// After a batch of imports is done, wait a few seconds before trying to kill blender,
		// in case of having multiple imports trigger in quick succession.
		kill_timer->start();
	}
}

void EditorImportBlendRunner::_kill_blender() {
	kill_timer->stop();
	if (is_running()) {
		OS::get_singleton()->kill(blender_pid);
	}
	blender_pid = 0;
}

void EditorImportBlendRunner::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_PREDELETE: {
			_kill_blender();
			break;
		}
	}
}

EditorImportBlendRunner *EditorImportBlendRunner::singleton = nullptr;

EditorImportBlendRunner::EditorImportBlendRunner() {
	ERR_FAIL_COND_MSG(singleton != nullptr, "EditorImportBlendRunner already created.");
	singleton = this;

	rpc_port = EDITOR_GET("filesystem/import/blender/rpc_port");

	kill_timer = memnew(Timer);
	add_child(kill_timer);
	kill_timer->set_one_shot(true);
	kill_timer->set_wait_time(EDITOR_GET("filesystem/import/blender/rpc_server_uptime"));
	kill_timer->connect("timeout", callable_mp(this, &EditorImportBlendRunner::_kill_blender));

	EditorFileSystem::get_singleton()->connect("resources_reimported", callable_mp(this, &EditorImportBlendRunner::_resources_reimported));
}

#endif // TOOLS_ENABLED

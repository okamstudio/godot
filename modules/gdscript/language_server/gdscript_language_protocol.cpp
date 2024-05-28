/**************************************************************************/
/*  gdscript_language_protocol.cpp                                        */
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

#include "gdscript_language_protocol.h"

#include "core/config/project_settings.h"
#include "editor/doc_tools.h"
#include "editor/editor_help.h"
#include "editor/editor_log.h"
#include "editor/editor_node.h"
#include "editor/editor_settings.h"

GDScriptLanguageProtocol *GDScriptLanguageProtocol::singleton = nullptr;

Error GDScriptLanguageProtocol::LSPeer::handle_data() {
	int read = 0;
	// Read headers
	if (!has_header) {
		while (true) {
			if (req_pos >= LSP_MAX_BUFFER_SIZE) {
				req_pos = 0;
				ERR_FAIL_V_MSG(ERR_OUT_OF_MEMORY, "Response header too big");
			}
			Error err = connection->get_partial_data(&req_buf[req_pos], 1, read);
			if (err != OK) {
				return FAILED;
			} else if (read != 1) { // Busy, wait until next poll
				return ERR_BUSY;
			}
			char *r = (char *)req_buf;
			int l = req_pos;

			// End of headers
			if (l > 3 && r[l] == '\n' && r[l - 1] == '\r' && r[l - 2] == '\n' && r[l - 3] == '\r') {
				r[l - 3] = '\0'; // Null terminate to read string
				String header;
				header.parse_utf8(r);
				content_length = header.substr(16).to_int();
				has_header = true;
				req_pos = 0;
				break;
			}
			req_pos++;
		}
	}
	if (has_header) {
		while (req_pos < content_length) {
			if (req_pos >= LSP_MAX_BUFFER_SIZE) {
				req_pos = 0;
				has_header = false;
				ERR_FAIL_COND_V_MSG(req_pos >= LSP_MAX_BUFFER_SIZE, ERR_OUT_OF_MEMORY, "Response content too big");
			}
			Error err = connection->get_partial_data(&req_buf[req_pos], 1, read);
			if (err != OK) {
				return FAILED;
			} else if (read != 1) {
				return ERR_BUSY;
			}
			req_pos++;
		}

		// Parse data
		String msg;
		msg.parse_utf8((const char *)req_buf, req_pos);

		// Reset to read again
		req_pos = 0;
		has_header = false;

		// Response
		String output = GDScriptLanguageProtocol::get_singleton()->process_message(msg);
		if (!output.is_empty()) {
			res_queue.push_back(output.utf8());
		}
	}
	return OK;
}

Error GDScriptLanguageProtocol::LSPeer::send_data() {
	int sent = 0;
	while (!res_queue.is_empty()) {
		CharString c_res = res_queue[0];
		if (res_sent < c_res.size()) {
			Error err = connection->put_partial_data((const uint8_t *)c_res.get_data() + res_sent, c_res.size() - res_sent - 1, sent);
			if (err != OK) {
				return err;
			}
			res_sent += sent;
		}
		// Response sent
		if (res_sent >= c_res.size() - 1) {
			res_sent = 0;
			res_queue.remove_at(0);
		}
	}
	return OK;
}

Error GDScriptLanguageProtocol::on_client_connected() {
	Ref<StreamPeerTCP> tcp_peer = server->take_connection();
	ERR_FAIL_COND_V_MSG(clients.size() >= LSP_MAX_CLIENTS, FAILED, "Max client limits reached");
	Ref<LSPeer> peer = memnew(LSPeer);
	peer->connection = tcp_peer;
	clients.insert(next_client_id, peer);
	next_client_id++;
	EditorNode::get_log()->add_message("[LSP] Connection Taken", EditorLog::MSG_TYPE_EDITOR);
	return OK;
}

void GDScriptLanguageProtocol::on_client_disconnected(const int &p_client_id) {
	clients.erase(p_client_id);
	EditorNode::get_log()->add_message("[LSP] Disconnected", EditorLog::MSG_TYPE_EDITOR);
}

String GDScriptLanguageProtocol::process_message(const String &p_text) {
	String ret = process_string(p_text);
	if (ret.is_empty()) {
		return ret;
	} else {
		return format_output(ret);
	}
}

String GDScriptLanguageProtocol::format_output(const String &p_text) {
	String header = "Content-Length: ";
	CharString charstr = p_text.utf8();
	size_t len = charstr.length();
	header += itos(len);
	header += "\r\n\r\n";

	return header + p_text;
}

void GDScriptLanguageProtocol::_bind_methods() {
	ClassDB::bind_method(D_METHOD("initialize", "params"), &GDScriptLanguageProtocol::initialize);
	ClassDB::bind_method(D_METHOD("initialized", "params"), &GDScriptLanguageProtocol::initialized);
	ClassDB::bind_method(D_METHOD("on_client_connected"), &GDScriptLanguageProtocol::on_client_connected);
	ClassDB::bind_method(D_METHOD("on_client_disconnected"), &GDScriptLanguageProtocol::on_client_disconnected);
	ClassDB::bind_method(D_METHOD("notify_client", "method", "params", "client_id"), &GDScriptLanguageProtocol::notify_client, DEFVAL(Variant()), DEFVAL(-1));
	ClassDB::bind_method(D_METHOD("is_smart_resolve_enabled"), &GDScriptLanguageProtocol::is_smart_resolve_enabled);
	ClassDB::bind_method(D_METHOD("get_text_document"), &GDScriptLanguageProtocol::get_text_document);
	ClassDB::bind_method(D_METHOD("get_workspace"), &GDScriptLanguageProtocol::get_workspace);
	ClassDB::bind_method(D_METHOD("is_initialized"), &GDScriptLanguageProtocol::is_initialized);
}

Dictionary GDScriptLanguageProtocol::initialize(const Dictionary &p_params) {
	lsp::InitializeResult ret;

	String root_uri = p_params["rootUri"];
	String root = p_params["rootPath"];
	bool is_same_workspace;
#ifndef WINDOWS_ENABLED
	is_same_workspace = root.to_lower() == workspace->root.to_lower();
#else
	is_same_workspace = root.replace_char('\\', '/').to_lower() == workspace->root.to_lower();
#endif

	if (root_uri.length() && is_same_workspace) {
		workspace->root_uri = root_uri;
	} else {
		String r_root = workspace->root;
		r_root = r_root.lstrip("/");
		workspace->root_uri = "file:///" + r_root;

		Dictionary params;
		params["path"] = workspace->root;
		Dictionary request = make_notification("gdscript_client/changeWorkspace", params);

		ERR_FAIL_COND_V_MSG(!clients.has(latest_client_id), ret.to_json(),
				vformat("GDScriptLanguageProtocol: Can't initialize invalid peer '%d'.", latest_client_id));
		Ref<LSPeer> peer = clients.get(latest_client_id);
		if (peer.is_valid()) {
			String msg = Variant(request).to_json_string();
			msg = format_output(msg);
			(*peer)->res_queue.push_back(msg.utf8());
		}
	}

	if (!_initialized) {
		workspace->initialize();
		text_document->initialize();
		_initialized = true;
	}

	return ret.to_json();
}

void GDScriptLanguageProtocol::initialized(const Variant &p_params) {
	lsp::GodotCapabilities capabilities;

	DocTools *doc = EditorHelp::get_doc_data();
	for (const KeyValue<String, DocData::ClassDoc> &E : doc->class_list) {
		lsp::GodotNativeClassInfo gdclass;
		gdclass.name = E.value.name;
		gdclass.class_doc = &(E.value);
		if (ClassDB::ClassInfo *ptr = ClassDB::classes.getptr(StringName(E.value.name))) {
			gdclass.class_info = ptr;
		}
		capabilities.native_classes.push_back(gdclass);
	}

	notify_client("gdscript/capabilities", capabilities.to_json());
}

void GDScriptLanguageProtocol::poll(int p_limit_usec) {
	uint64_t target_ticks = OS::get_singleton()->get_ticks_usec() + p_limit_usec;

	if (server->is_connection_available()) {
		on_client_connected();
	}

	HashMap<int, Ref<LSPeer>>::Iterator E = clients.begin();
	while (E != clients.end()) {
		Ref<LSPeer> peer = E->value;
		peer->connection->poll();
		StreamPeerTCP::Status status = peer->connection->get_status();
		if (status == StreamPeerTCP::STATUS_NONE || status == StreamPeerTCP::STATUS_ERROR) {
			on_client_disconnected(E->key);
			E = clients.begin();
			continue;
		} else {
			Error err = OK;
			while (peer->connection->get_available_bytes() > 0) {
				latest_client_id = E->key;
				err = peer->handle_data();
				if (err != OK || OS::get_singleton()->get_ticks_usec() >= target_ticks) {
					break;
				}
			}

			if (err != OK && err != ERR_BUSY) {
				on_client_disconnected(E->key);
				E = clients.begin();
				continue;
			}

			err = peer->send_data();
			if (err != OK && err != ERR_BUSY) {
				on_client_disconnected(E->key);
				E = clients.begin();
				continue;
			}
		}
		++E;
	}
}

Error GDScriptLanguageProtocol::start(int p_port, const IPAddress &p_bind_ip) {
	return server->listen(p_port, p_bind_ip);
}

void GDScriptLanguageProtocol::stop() {
	for (const KeyValue<int, Ref<LSPeer>> &E : clients) {
		Ref<LSPeer> peer = clients.get(E.key);
		peer->connection->disconnect_from_host();
	}

	server->stop();
}

void GDScriptLanguageProtocol::notify_client(const String &p_method, const Variant &p_params, int p_client_id) {
#ifdef TESTS_ENABLED
	if (clients.is_empty()) {
		return;
	}
#endif
	if (p_client_id == -1) {
		ERR_FAIL_COND_MSG(latest_client_id == -1,
				"GDScript LSP: Can't notify client as none was connected.");
		p_client_id = latest_client_id;
	}
	ERR_FAIL_COND(!clients.has(p_client_id));
	Ref<LSPeer> peer = clients.get(p_client_id);
	ERR_FAIL_COND(peer.is_null());

	Dictionary message = make_notification(p_method, p_params);
	String msg = Variant(message).to_json_string();
	msg = format_output(msg);
	peer->res_queue.push_back(msg.utf8());
}

void GDScriptLanguageProtocol::request_client(const String &p_method, const Variant &p_params, int p_client_id) {
#ifdef TESTS_ENABLED
	if (clients.is_empty()) {
		return;
	}
#endif
	if (p_client_id == -1) {
		ERR_FAIL_COND_MSG(latest_client_id == -1,
				"GDScript LSP: Can't notify client as none was connected.");
		p_client_id = latest_client_id;
	}
	ERR_FAIL_COND(!clients.has(p_client_id));
	Ref<LSPeer> peer = clients.get(p_client_id);
	ERR_FAIL_COND(peer.is_null());

	Dictionary message = make_request(p_method, p_params, next_server_id);
	next_server_id++;
	String msg = Variant(message).to_json_string();
	msg = format_output(msg);
	peer->res_queue.push_back(msg.utf8());
}

bool GDScriptLanguageProtocol::is_smart_resolve_enabled() const {
	return bool(_EDITOR_GET("network/language_server/enable_smart_resolve"));
}

bool GDScriptLanguageProtocol::is_goto_native_symbols_enabled() const {
	return bool(_EDITOR_GET("network/language_server/show_native_symbols_in_editor"));
}

GDScriptLanguageProtocol::GDScriptLanguageProtocol() {
	server.instantiate();
	singleton = this;
	workspace.instantiate();
	text_document.instantiate();
	set_scope("textDocument", text_document.ptr());
	set_scope("completionItem", text_document.ptr());
	set_scope("workspace", workspace.ptr());
	workspace->root = ProjectSettings::get_singleton()->get_resource_path();
}

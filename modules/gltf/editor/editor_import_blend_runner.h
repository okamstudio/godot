/**************************************************************************/
/*  editor_import_blend_runner.h                                          */
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

#pragma once

#ifdef TOOLS_ENABLED

#include "core/io/http_client.h"
#include "core/os/os.h"
#include "scene/main/node.h"
#include "scene/main/timer.h"

class EditorImportBlendRunner : public Node {
	GDCLASS(EditorImportBlendRunner, Node);

	static EditorImportBlendRunner *singleton;

	Timer *kill_timer;
	void _resources_reimported(const PackedStringArray &p_files);
	void _kill_blender();
	void _notification(int p_what);
	bool _extract_error_message_xml(const Vector<uint8_t> &p_response_data, String &r_error_message);

protected:
	int rpc_port = 0;
	OS::ProcessID blender_pid = 0;
	Error start_blender(const String &p_python_script, bool p_blocking);
	Error do_import_direct(const Dictionary &p_options);
	Error do_import_rpc(const Dictionary &p_options);

public:
	static EditorImportBlendRunner *get_singleton() { return singleton; }

	bool is_running() { return blender_pid != 0 && OS::get_singleton()->is_process_running(blender_pid); }
	bool is_using_rpc() { return rpc_port != 0; }
	Error do_import(const Dictionary &p_options);
	HTTPClient::Status connect_blender_rpc(const Ref<HTTPClient> &p_client, int p_timeout_usecs);

	EditorImportBlendRunner();
};

#endif // TOOLS_ENABLED

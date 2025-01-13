/**************************************************************************/
/*  debug_adapter_server.cpp                                              */
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

#include "debug_adapter_server.h"

#include "editor/editor_log.h"
#include "editor/editor_node.h"
#include "editor/editor_settings.h"

int DebugAdapterServer::port_override = -1;

DebugAdapterServer::DebugAdapterServer() {
	// TODO: Move to editor_settings.cpp
	_EDITOR_DEF("network/debug_adapter/remote_port", remote_port);
	_EDITOR_DEF("network/debug_adapter/request_timeout", protocol._request_timeout);
	_EDITOR_DEF("network/debug_adapter/sync_breakpoints", protocol._sync_breakpoints);
}

void DebugAdapterServer::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			start();
		} break;

		case NOTIFICATION_EXIT_TREE: {
			stop();
		} break;

		case NOTIFICATION_INTERNAL_PROCESS: {
			// The main loop can be run again during request processing, which modifies internal state of the protocol.
			// Thus, "polling" is needed to prevent it from parsing other requests while the current one isn't finished.
			if (started && !polling) {
				polling = true;
				protocol.poll();
				polling = false;
			}
		} break;

		case EditorSettings::NOTIFICATION_EDITOR_SETTINGS_CHANGED: {
			if (!EditorSettings::get_singleton()->check_changed_settings_in_group("network/debug_adapter")) {
				break;
			}
			protocol._request_timeout = EDITOR_GET("network/debug_adapter/request_timeout");
			protocol._sync_breakpoints = EDITOR_GET("network/debug_adapter/sync_breakpoints");
			int port = (DebugAdapterServer::port_override > -1) ? DebugAdapterServer::port_override : (int)_EDITOR_GET("network/debug_adapter/remote_port");
			if (port != remote_port) {
				stop();
				start();
			}
		} break;
	}
}

void DebugAdapterServer::start() {
	remote_port = (DebugAdapterServer::port_override > -1) ? DebugAdapterServer::port_override : (int)_EDITOR_GET("network/debug_adapter/remote_port");
	if (protocol.start(remote_port, IPAddress("127.0.0.1")) == OK) {
		EditorNode::get_log()->add_message("--- Debug adapter server started on port " + itos(remote_port) + " ---", EditorLog::MSG_TYPE_EDITOR);
		set_process_internal(true);
		started = true;
	}
}

void DebugAdapterServer::stop() {
	protocol.stop();
	started = false;
	EditorNode::get_log()->add_message("--- Debug adapter server stopped ---", EditorLog::MSG_TYPE_EDITOR);
}

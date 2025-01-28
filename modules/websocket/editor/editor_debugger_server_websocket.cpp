/**************************************************************************/
/*  editor_debugger_server_websocket.cpp                                  */
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

#include "editor_debugger_server_websocket.h"

#ifdef TOOLS_ENABLED

#include "../remote_debugger_peer_websocket.h"

#include "editor/editor_log.h"
#include "editor/editor_node.h"
#include "editor/editor_settings.h"

void EditorDebuggerServerWebSocket::poll() {
	if (pending_peer.is_null() && tcp_server->is_connection_available()) {
		Ref<WebSocketPeer> peer = Ref<WebSocketPeer>(WebSocketPeer::create());
		ERR_FAIL_COND(peer.is_null()); // Bug.

		Vector<String> ws_protocols;
		ws_protocols.push_back("binary"); // Compatibility for emscripten TCP-to-WebSocket.
		peer->set_supported_protocols(ws_protocols);

		Error err = peer->accept_stream(tcp_server->take_connection());
		if (err == OK) {
			pending_timer = OS::get_singleton()->get_ticks_msec();
			pending_peer = peer;
		}
	}
	if (pending_peer.is_valid() && pending_peer->get_ready_state() != WebSocketPeer::STATE_OPEN) {
		pending_peer->poll();
		WebSocketPeer::State ready_state = pending_peer->get_ready_state();
		if (ready_state != WebSocketPeer::STATE_CONNECTING && ready_state != WebSocketPeer::STATE_OPEN) {
			pending_peer.unref(); // Failed.
		}
		if (ready_state == WebSocketPeer::STATE_CONNECTING && OS::get_singleton()->get_ticks_msec() - pending_timer > 3000) {
			pending_peer.unref(); // Timeout.
		}
	}
}

String EditorDebuggerServerWebSocket::get_uri() const {
	return endpoint;
}

Error EditorDebuggerServerWebSocket::start(const String &p_uri) {
	// Default host and port
	String bind_host = (String)EDITOR_GET("network/debug/remote_host");
	int bind_port = (int)EDITOR_GET("network/debug/remote_port");

	// Optionally override
	if (!p_uri.is_empty() && p_uri != "ws://") {
		String scheme, path, fragment;
		Error err = p_uri.parse_url(scheme, bind_host, bind_port, path, fragment);
		ERR_FAIL_COND_V(err != OK, ERR_INVALID_PARAMETER);
		ERR_FAIL_COND_V(!bind_host.is_valid_ip_address() && bind_host != "*", ERR_INVALID_PARAMETER);
	}

	// Try listening on ports
	const int max_attempts = 5;
	for (int attempt = 1;; ++attempt) {
		const Error err = tcp_server->listen(bind_port, bind_host);
		if (err == OK) {
			break;
		}
		if (attempt >= max_attempts) {
			EditorNode::get_log()->add_message(vformat("Cannot listen on port %d, remote debugging unavailable.", bind_port), EditorLog::MSG_TYPE_ERROR);
			return err;
		}
		int last_port = bind_port++;
		EditorNode::get_log()->add_message(vformat("Cannot listen on port %d, trying %d instead.", last_port, bind_port), EditorLog::MSG_TYPE_WARNING);
	}

	// Endpoint that the client should connect to
	endpoint = vformat("ws://%s:%d", bind_host, bind_port);

	return OK;
}

void EditorDebuggerServerWebSocket::stop() {
	pending_peer.unref();
	tcp_server->stop();
}

bool EditorDebuggerServerWebSocket::is_active() const {
	return tcp_server->is_listening();
}

bool EditorDebuggerServerWebSocket::is_connection_available() const {
	return pending_peer.is_valid() && pending_peer->get_ready_state() == WebSocketPeer::STATE_OPEN;
}

Ref<RemoteDebuggerPeer> EditorDebuggerServerWebSocket::take_connection() {
	ERR_FAIL_COND_V(!is_connection_available(), Ref<RemoteDebuggerPeer>());
	RemoteDebuggerPeer *peer = memnew(RemoteDebuggerPeerWebSocket(pending_peer));
	pending_peer.unref();
	return peer;
}

EditorDebuggerServerWebSocket::EditorDebuggerServerWebSocket() {
	tcp_server.instantiate();
}

EditorDebuggerServerWebSocket::~EditorDebuggerServerWebSocket() {
	stop();
}

EditorDebuggerServer *EditorDebuggerServerWebSocket::create(const String &p_protocol) {
	ERR_FAIL_COND_V(p_protocol != "ws://", nullptr);
	return memnew(EditorDebuggerServerWebSocket);
}

#endif // TOOLS_ENABLED

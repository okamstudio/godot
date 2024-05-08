/**************************************************************************/
/*  remote_debugger_peer_websocket.cpp                                    */
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

#include "remote_debugger_peer_websocket.h"

#include "core/config/project_settings.h"

Error RemoteDebuggerPeerWebSocket::connect_to_host(const String &p_uri) {
	ws_peer = Ref<WebSocketPeer>(WebSocketPeer::create());
	ERR_FAIL_COND_V(ws_peer.is_null(), ERR_BUG);

	Vector<String> protocols;
	protocols.push_back("binary"); // Compatibility for emscripten TCP-to-WebSocket.

	ws_peer->set_supported_protocols(protocols);
	ws_peer->set_max_queued_packets(max_queued_messages);
	ws_peer->set_inbound_buffer_size((1 << 23) - 1);
	ws_peer->set_outbound_buffer_size((1 << 23) - 1);

	Error err = ws_peer->connect_to_url(p_uri);
	ERR_FAIL_COND_V(err != OK, err);

	ws_peer->poll();
	WebSocketPeer::State ready_state = ws_peer->get_ready_state();
	if (ready_state != WebSocketPeer::STATE_CONNECTING && ready_state != WebSocketPeer::STATE_OPEN) {
		ERR_PRINT(vformat("Remote Debugger: Unable to connect. State: %s.", ws_peer->get_ready_state()));
		return FAILED;
	}

	return OK;
}

bool RemoteDebuggerPeerWebSocket::is_peer_connected() {
	return ws_peer.is_valid() && (ws_peer->get_ready_state() == WebSocketPeer::STATE_OPEN || ws_peer->get_ready_state() == WebSocketPeer::STATE_CONNECTING);
}

void RemoteDebuggerPeerWebSocket::poll() {
	ERR_FAIL_COND(ws_peer.is_null());
	ws_peer->poll();

	while (ws_peer->get_ready_state() == WebSocketPeer::STATE_OPEN && ws_peer->get_available_packet_count() > 0 && in_queue.size() < max_queued_messages) {
		Variant var;
		Error err = ws_peer->get_var(var);
		ERR_CONTINUE(err != OK);
		ERR_CONTINUE(var.get_type() != Variant::ARRAY);
		in_queue.push_back(var);
	}

	while (ws_peer->get_ready_state() == WebSocketPeer::STATE_OPEN && out_queue.size() > 0) {
		Array var = out_queue.get_front();
		Error err = ws_peer->put_var(var);
		ERR_BREAK(err != OK); // Peer buffer full?
		out_queue.pop_front();
	}
}

int RemoteDebuggerPeerWebSocket::get_max_message_size() const {
	ERR_FAIL_COND_V(ws_peer.is_null(), 0);
	return ws_peer->get_max_packet_size();
}

bool RemoteDebuggerPeerWebSocket::has_message() {
	return in_queue.size() > 0;
}

Array RemoteDebuggerPeerWebSocket::get_message() {
	ERR_FAIL_COND_V(in_queue.is_empty(), Array());
	Array msg = in_queue.get_front();
	in_queue.pop_front();
	return msg;
}

Error RemoteDebuggerPeerWebSocket::put_message(const Array &p_arr) {
	if (out_queue.size() >= max_queued_messages) {
		return ERR_OUT_OF_MEMORY;
	}
	out_queue.push_back(p_arr);
	return OK;
}

void RemoteDebuggerPeerWebSocket::close() {
	if (ws_peer.is_valid()) {
		ws_peer.unref();
	}
}

bool RemoteDebuggerPeerWebSocket::can_block() const {
#ifdef WEB_ENABLED
	return false;
#else
	return true;
#endif
}

RemoteDebuggerPeerWebSocket::RemoteDebuggerPeerWebSocket(Ref<WebSocketPeer> p_peer) {
	max_queued_messages = (int)GLOBAL_GET("network/limits/debugger/max_queued_messages");
	ws_peer = p_peer;
	if (ws_peer.is_valid()) {
		ws_peer->set_max_queued_packets(max_queued_messages);
		ws_peer->set_inbound_buffer_size((1 << 23) - 1);
		ws_peer->set_outbound_buffer_size((1 << 23) - 1);
	}
}

RemoteDebuggerPeer *RemoteDebuggerPeerWebSocket::create(const String &p_uri) {
	ERR_FAIL_COND_V(!p_uri.begins_with("ws://") && !p_uri.begins_with("wss://"), nullptr);
	RemoteDebuggerPeerWebSocket *peer = memnew(RemoteDebuggerPeerWebSocket);
	Error err = peer->connect_to_host(p_uri);
	if (err != OK) {
		memdelete(peer);
		return nullptr;
	}
	return peer;
}

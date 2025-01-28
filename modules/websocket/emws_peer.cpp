/**************************************************************************/
/*  emws_peer.cpp                                                         */
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

#include "emws_peer.h"

#ifdef WEB_ENABLED

#include "core/io/ip.h"

void EMWSPeer::_esws_on_connect(void *p_obj, char *p_proto) {
	EMWSPeer *peer = static_cast<EMWSPeer *>(p_obj);
	peer->ready_state = STATE_OPEN;
	peer->selected_protocol.parse_utf8(p_proto);
}

void EMWSPeer::_esws_on_message(void *p_obj, const uint8_t *p_data, int p_data_size, int p_is_string) {
	EMWSPeer *peer = static_cast<EMWSPeer *>(p_obj);
	uint8_t is_string = p_is_string ? 1 : 0;
	peer->in_buffer.write_packet(p_data, p_data_size, &is_string);
}

void EMWSPeer::_esws_on_error(void *p_obj) {
	EMWSPeer *peer = static_cast<EMWSPeer *>(p_obj);
	peer->ready_state = STATE_CLOSED;
}

void EMWSPeer::_esws_on_close(void *p_obj, int p_code, const char *p_reason, int p_was_clean) {
	EMWSPeer *peer = static_cast<EMWSPeer *>(p_obj);
	peer->close_code = p_code;
	peer->close_reason.parse_utf8(p_reason);
	peer->ready_state = STATE_CLOSED;
}

Error EMWSPeer::connect_to_url(const String &p_url, Ref<TLSOptions> p_tls_options) {
	ERR_FAIL_COND_V(p_url.is_empty(), ERR_INVALID_PARAMETER);
	ERR_FAIL_COND_V(p_tls_options.is_valid() && p_tls_options->is_server(), ERR_INVALID_PARAMETER);
	ERR_FAIL_COND_V(ready_state != STATE_CLOSED && ready_state != STATE_CLOSING, ERR_ALREADY_IN_USE);

	_clear();

	String host;
	String path;
	String scheme;
	String fragment;
	int port = 0;
	Error err = p_url.parse_url(scheme, host, port, path, fragment);
	ERR_FAIL_COND_V_MSG(err != OK, err, "Invalid URL: " + p_url);

	if (scheme.is_empty()) {
		scheme = "ws://";
	}
	ERR_FAIL_COND_V_MSG(scheme != "ws://" && scheme != "wss://", ERR_INVALID_PARAMETER, vformat("Invalid protocol: \"%s\" (must be either \"ws://\" or \"wss://\").", scheme));

	String proto_string;
	for (int i = 0; i < supported_protocols.size(); i++) {
		if (i != 0) {
			proto_string += ",";
		}
		proto_string += supported_protocols[i];
	}

	if (handshake_headers.size()) {
		WARN_PRINT_ONCE("Custom headers are not supported in Web platform.");
	}

	requested_url = scheme + host;

	if (port && ((scheme == "ws://" && port != 80) || (scheme == "wss://" && port != 443))) {
		requested_url += ":" + String::num_int64(port);
	}

	if (!path.is_empty()) {
		requested_url += path;
	}

	peer_sock = godot_js_websocket_create(this, requested_url.utf8().get_data(), proto_string.utf8().get_data(), &_esws_on_connect, &_esws_on_message, &_esws_on_error, &_esws_on_close);
	if (peer_sock == -1) {
		return FAILED;
	}
	in_buffer.resize(nearest_shift(inbound_buffer_size), max_queued_packets);
	packet_buffer.resize(inbound_buffer_size);
	ready_state = STATE_CONNECTING;
	return OK;
}

Error EMWSPeer::accept_stream(Ref<StreamPeer> p_stream) {
	WARN_PRINT_ONCE("Acting as WebSocket server is not supported in Web platforms.");
	return ERR_UNAVAILABLE;
}

Error EMWSPeer::_send(const uint8_t *p_buffer, int p_buffer_size, bool p_binary) {
	ERR_FAIL_COND_V(outbound_buffer_size > 0 && (get_current_outbound_buffered_amount() + p_buffer_size >= outbound_buffer_size), ERR_OUT_OF_MEMORY);

	if (godot_js_websocket_send(peer_sock, p_buffer, p_buffer_size, p_binary ? 1 : 0) != 0) {
		return FAILED;
	}
	return OK;
}

Error EMWSPeer::send(const uint8_t *p_buffer, int p_buffer_size, WriteMode p_mode) {
	return _send(p_buffer, p_buffer_size, p_mode == WRITE_MODE_BINARY);
}

Error EMWSPeer::put_packet(const uint8_t *p_buffer, int p_buffer_size) {
	return _send(p_buffer, p_buffer_size, true);
}

Error EMWSPeer::get_packet(const uint8_t **r_buffer, int &r_buffer_size) {
	if (in_buffer.packets_left() == 0) {
		return ERR_UNAVAILABLE;
	}

	int read = 0;
	Error err = in_buffer.read_packet(packet_buffer.ptrw(), packet_buffer.size(), &was_string, read);
	ERR_FAIL_COND_V(err != OK, err);

	*r_buffer = packet_buffer.ptr();
	r_buffer_size = read;

	return OK;
}

int EMWSPeer::get_available_packet_count() const {
	return in_buffer.packets_left();
}

int EMWSPeer::get_current_outbound_buffered_amount() const {
	if (peer_sock != -1) {
		return godot_js_websocket_buffered_amount(peer_sock);
	}
	return 0;
}

bool EMWSPeer::was_string_packet() const {
	return was_string;
}

void EMWSPeer::_clear() {
	if (peer_sock != -1) {
		godot_js_websocket_destroy(peer_sock);
		peer_sock = -1;
	}
	ready_state = STATE_CLOSED;
	was_string = 0;
	close_code = -1;
	close_reason.clear();
	selected_protocol.clear();
	requested_url.clear();
	in_buffer.clear();
	packet_buffer.clear();
}

void EMWSPeer::close(int p_code, String p_reason) {
	if (p_code < 0) {
		if (peer_sock != -1) {
			godot_js_websocket_destroy(peer_sock);
			peer_sock = -1;
		}
		ready_state = STATE_CLOSED;
	}
	if (ready_state == STATE_CONNECTING || ready_state == STATE_OPEN) {
		ready_state = STATE_CLOSING;
		if (peer_sock != -1) {
			godot_js_websocket_close(peer_sock, p_code, p_reason.utf8().get_data());
		} else {
			ready_state = STATE_CLOSED;
		}
	}
	in_buffer.clear();
	packet_buffer.clear();
}

void EMWSPeer::poll() {
	// Automatically polled by the navigator.
}

WebSocketPeer::State EMWSPeer::get_ready_state() const {
	return ready_state;
}

int EMWSPeer::get_close_code() const {
	return close_code;
}

String EMWSPeer::get_close_reason() const {
	return close_reason;
}

String EMWSPeer::get_selected_protocol() const {
	return selected_protocol;
}

String EMWSPeer::get_requested_url() const {
	return requested_url;
}

IPAddress EMWSPeer::get_connected_host() const {
	ERR_FAIL_V_MSG(IPAddress(), "Not supported in Web export.");
}

uint16_t EMWSPeer::get_connected_port() const {
	ERR_FAIL_V_MSG(0, "Not supported in Web export.");
}

void EMWSPeer::set_no_delay(bool p_enabled) {
	ERR_FAIL_MSG("'set_no_delay' is not supported in Web export.");
}

EMWSPeer::EMWSPeer() {
}

EMWSPeer::~EMWSPeer() {
	_clear();
}

#endif // WEB_ENABLED

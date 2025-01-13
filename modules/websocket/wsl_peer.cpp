/**************************************************************************/
/*  wsl_peer.cpp                                                          */
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

#include "wsl_peer.h"

#ifndef WEB_ENABLED

#include "core/io/stream_peer_tls.h"

CryptoCore::RandomGenerator *WSLPeer::_static_rng = nullptr;

void WSLPeer::initialize() {
	WebSocketPeer::_create = WSLPeer::_create;
	_static_rng = memnew(CryptoCore::RandomGenerator);
	_static_rng->init();
}

void WSLPeer::deinitialize() {
	if (_static_rng) {
		memdelete(_static_rng);
		_static_rng = nullptr;
	}
}

///
/// Resolver
///
void WSLPeer::Resolver::start(const String &p_host, int p_port) {
	stop();

	port = p_port;
	if (p_host.is_valid_ip_address()) {
		ip_candidates.push_back(IPAddress(p_host));
	} else {
		// Queue hostname for resolution.
		resolver_id = IP::get_singleton()->resolve_hostname_queue_item(p_host);
		ERR_FAIL_COND(resolver_id == IP::RESOLVER_INVALID_ID);
		// Check if it was found in cache.
		IP::ResolverStatus ip_status = IP::get_singleton()->get_resolve_item_status(resolver_id);
		if (ip_status == IP::RESOLVER_STATUS_DONE) {
			ip_candidates = IP::get_singleton()->get_resolve_item_addresses(resolver_id);
			IP::get_singleton()->erase_resolve_item(resolver_id);
			resolver_id = IP::RESOLVER_INVALID_ID;
		}
	}
}

void WSLPeer::Resolver::stop() {
	if (resolver_id != IP::RESOLVER_INVALID_ID) {
		IP::get_singleton()->erase_resolve_item(resolver_id);
		resolver_id = IP::RESOLVER_INVALID_ID;
	}
	port = 0;
}

void WSLPeer::Resolver::try_next_candidate(Ref<StreamPeerTCP> &p_tcp) {
	// Check if we still need resolving.
	if (resolver_id != IP::RESOLVER_INVALID_ID) {
		IP::ResolverStatus ip_status = IP::get_singleton()->get_resolve_item_status(resolver_id);
		if (ip_status == IP::RESOLVER_STATUS_WAITING) {
			return;
		}
		if (ip_status == IP::RESOLVER_STATUS_DONE) {
			ip_candidates = IP::get_singleton()->get_resolve_item_addresses(resolver_id);
		}
		IP::get_singleton()->erase_resolve_item(resolver_id);
		resolver_id = IP::RESOLVER_INVALID_ID;
	}

	// Try the current candidate if we have one.
	if (p_tcp->get_status() != StreamPeerTCP::STATUS_NONE) {
		p_tcp->poll();
		StreamPeerTCP::Status status = p_tcp->get_status();
		if (status == StreamPeerTCP::STATUS_CONNECTED) {
			// On Windows, setting TCP_NODELAY may fail if the socket is still connecting.
			p_tcp->set_no_delay(true);
			ip_candidates.clear();
			return;
		} else if (status == StreamPeerTCP::STATUS_CONNECTING) {
			return; // Keep connecting.
		} else {
			p_tcp->disconnect_from_host();
		}
	}

	// Keep trying next candidate.
	while (ip_candidates.size()) {
		Error err = p_tcp->connect_to_host(ip_candidates.pop_front(), port);
		if (err == OK) {
			return;
		} else {
			p_tcp->disconnect_from_host();
		}
	}
}

///
/// Server functions
///
Error WSLPeer::accept_stream(Ref<StreamPeer> p_stream) {
	ERR_FAIL_COND_V(p_stream.is_null(), ERR_INVALID_PARAMETER);
	ERR_FAIL_COND_V(ready_state != STATE_CLOSED && ready_state != STATE_CLOSING, ERR_ALREADY_IN_USE);

	_clear();

	if (p_stream->is_class_ptr(StreamPeerTCP::get_class_ptr_static())) {
		tcp = p_stream;
		connection = p_stream;
		use_tls = false;
	} else if (p_stream->is_class_ptr(StreamPeerTLS::get_class_ptr_static())) {
		Ref<StreamPeer> base_stream = static_cast<Ref<StreamPeerTLS>>(p_stream)->get_stream();
		ERR_FAIL_COND_V(base_stream.is_null() || !base_stream->is_class_ptr(StreamPeerTCP::get_class_ptr_static()), ERR_INVALID_PARAMETER);
		tcp = static_cast<Ref<StreamPeerTCP>>(base_stream);
		connection = p_stream;
		use_tls = true;
	}
	ERR_FAIL_COND_V(connection.is_null() || tcp.is_null(), ERR_INVALID_PARAMETER);
	is_server = true;
	tcp->set_no_delay(true);
	ready_state = STATE_CONNECTING;
	handshake_buffer->resize(WSL_MAX_HEADER_SIZE);
	handshake_buffer->seek(0);
	return OK;
}

bool WSLPeer::_parse_client_request() {
	Vector<String> psa = String((const char *)handshake_buffer->get_data_array().ptr(), handshake_buffer->get_position() - 4).split("\r\n");
	int len = psa.size();
	ERR_FAIL_COND_V_MSG(len < 4, false, "Not enough response headers, got: " + itos(len) + ", expected >= 4.");

	Vector<String> req = psa[0].split(" ", false);
	ERR_FAIL_COND_V_MSG(req.size() < 2, false, "Invalid protocol or status code.");

	// Wrong protocol
	ERR_FAIL_COND_V_MSG(req[0] != "GET" || req[2] != "HTTP/1.1", false, "Invalid method or HTTP version.");

	HashMap<String, String> headers;
	for (int i = 1; i < len; i++) {
		Vector<String> header = psa[i].split(":", false, 1);
		ERR_FAIL_COND_V_MSG(header.size() != 2, false, "Invalid header -> " + psa[i]);
		String name = header[0].to_lower();
		String value = header[1].strip_edges();
		if (headers.has(name)) {
			headers[name] += "," + value;
		} else {
			headers[name] = value;
		}
	}
	requested_host = headers.has("host") ? headers.get("host") : "";
	requested_url = (use_tls ? "wss://" : "ws://") + requested_host + req[1];
#define WSL_CHECK(NAME, VALUE)                                                          \
	ERR_FAIL_COND_V_MSG(!headers.has(NAME) || headers[NAME].to_lower() != VALUE, false, \
			"Missing or invalid header '" + String(NAME) + "'. Expected value '" + VALUE + "'.");
#define WSL_CHECK_EX(NAME) \
	ERR_FAIL_COND_V_MSG(!headers.has(NAME), false, "Missing header '" + String(NAME) + "'.");
	WSL_CHECK("upgrade", "websocket");
	WSL_CHECK("sec-websocket-version", "13");
	WSL_CHECK_EX("sec-websocket-key");
	WSL_CHECK_EX("connection");
#undef WSL_CHECK_EX
#undef WSL_CHECK
	session_key = headers["sec-websocket-key"];
	if (headers.has("sec-websocket-protocol")) {
		Vector<String> protos = headers["sec-websocket-protocol"].split(",");
		for (int i = 0; i < protos.size(); i++) {
			String proto = protos[i].strip_edges();
			// Check if we have the given protocol
			for (int j = 0; j < supported_protocols.size(); j++) {
				if (proto != supported_protocols[j]) {
					continue;
				}
				selected_protocol = proto;
				break;
			}
			// Found a protocol
			if (!selected_protocol.is_empty()) {
				break;
			}
		}
		if (selected_protocol.is_empty()) { // Invalid protocol(s) requested
			return false;
		}
	} else if (supported_protocols.size() > 0) { // No protocol requested, but we need one
		return false;
	}
	return true;
}

Error WSLPeer::_do_server_handshake() {
	if (use_tls) {
		Ref<StreamPeerTLS> tls = static_cast<Ref<StreamPeerTLS>>(connection);
		if (tls.is_null()) {
			ERR_FAIL_V_MSG(ERR_BUG, "Couldn't get StreamPeerTLS for WebSocket handshake.");
			close(-1);
			return FAILED;
		}
		tls->poll();
		if (tls->get_status() == StreamPeerTLS::STATUS_HANDSHAKING) {
			return OK; // Pending handshake
		} else if (tls->get_status() != StreamPeerTLS::STATUS_CONNECTED) {
			print_verbose(vformat("WebSocket SSL connection error during handshake (StreamPeerTLS status code %d).", tls->get_status()));
			close(-1);
			return FAILED;
		}
	}

	if (pending_request) {
		int read = 0;
		while (true) {
			ERR_FAIL_COND_V_MSG(handshake_buffer->get_available_bytes() < 1, ERR_OUT_OF_MEMORY, "WebSocket response headers are too big.");
			int pos = handshake_buffer->get_position();
			uint8_t byte;
			Error err = connection->get_partial_data(&byte, 1, read);
			if (err != OK) { // Got an error
				print_verbose(vformat("WebSocket error while getting partial data (StreamPeer error code %d).", err));
				close(-1);
				return FAILED;
			} else if (read != 1) { // Busy, wait next poll
				return OK;
			}
			handshake_buffer->put_u8(byte);
			const char *r = (const char *)handshake_buffer->get_data_array().ptr();
			int l = pos;
			if (l > 3 && r[l] == '\n' && r[l - 1] == '\r' && r[l - 2] == '\n' && r[l - 3] == '\r') {
				if (!_parse_client_request()) {
					close(-1);
					return FAILED;
				}
				String s = "HTTP/1.1 101 Switching Protocols\r\n";
				s += "Upgrade: websocket\r\n";
				s += "Connection: Upgrade\r\n";
				s += "Sec-WebSocket-Accept: " + _compute_key_response(session_key) + "\r\n";
				if (!selected_protocol.is_empty()) {
					s += "Sec-WebSocket-Protocol: " + selected_protocol + "\r\n";
				}
				for (int i = 0; i < handshake_headers.size(); i++) {
					s += handshake_headers[i] + "\r\n";
				}
				s += "\r\n";
				CharString cs = s.utf8();
				handshake_buffer->clear();
				handshake_buffer->put_data((const uint8_t *)cs.get_data(), cs.length());
				handshake_buffer->seek(0);
				pending_request = false;
				break;
			}
		}
	}

	if (pending_request) { // Still pending.
		return OK;
	}

	int left = handshake_buffer->get_available_bytes();
	if (left) {
		Vector<uint8_t> data = handshake_buffer->get_data_array();
		int pos = handshake_buffer->get_position();
		int sent = 0;
		Error err = connection->put_partial_data(data.ptr() + pos, left, sent);
		if (err != OK) {
			print_verbose(vformat("WebSocket error while putting partial data (StreamPeer error code %d).", err));
			close(-1);
			return err;
		}
		handshake_buffer->seek(pos + sent);
		left -= sent;
		if (left == 0) {
			resolver.stop();
			// Response sent, initialize wslay context.
			wslay_event_context_server_init(&wsl_ctx, &_wsl_callbacks, this);
			wslay_event_config_set_no_buffering(wsl_ctx, 1);
			wslay_event_config_set_max_recv_msg_length(wsl_ctx, inbound_buffer_size);
			in_buffer.resize(nearest_shift(inbound_buffer_size), max_queued_packets);
			packet_buffer.resize(inbound_buffer_size);
			ready_state = STATE_OPEN;
		}
	}

	return OK;
}

///
/// Client functions
///
void WSLPeer::_do_client_handshake() {
	ERR_FAIL_COND(tcp.is_null());

	// Try to connect to candidates.
	if (resolver.has_more_candidates() || tcp->get_status() == StreamPeerTCP::STATUS_CONNECTING) {
		resolver.try_next_candidate(tcp);
		if (resolver.has_more_candidates()) {
			return; // Still pending.
		}
	}

	tcp->poll();
	if (tcp->get_status() == StreamPeerTCP::STATUS_CONNECTING) {
		return; // Keep connecting.
	} else if (tcp->get_status() != StreamPeerTCP::STATUS_CONNECTED) {
		close(-1); // Failed to connect.
		return;
	}

	if (use_tls) {
		Ref<StreamPeerTLS> tls;
		if (connection == tcp) {
			// Start SSL handshake
			tls = Ref<StreamPeerTLS>(StreamPeerTLS::create());
			ERR_FAIL_COND(tls.is_null());
			if (tls->connect_to_stream(tcp, requested_host, tls_options) != OK) {
				close(-1);
				return; // Error.
			}
			connection = tls;
		} else {
			tls = static_cast<Ref<StreamPeerTLS>>(connection);
			ERR_FAIL_COND(tls.is_null());
			tls->poll();
		}
		if (tls->get_status() == StreamPeerTLS::STATUS_HANDSHAKING) {
			return; // Need more polling.
		} else if (tls->get_status() != StreamPeerTLS::STATUS_CONNECTED) {
			close(-1);
			return; // Error.
		}
	}

	// Do websocket handshake.
	if (pending_request) {
		int left = handshake_buffer->get_available_bytes();
		int pos = handshake_buffer->get_position();
		const Vector<uint8_t> data = handshake_buffer->get_data_array();
		int sent = 0;
		Error err = connection->put_partial_data(data.ptr() + pos, left, sent);
		// Sending handshake failed
		if (err != OK) {
			close(-1);
			return; // Error.
		}
		handshake_buffer->seek(pos + sent);
		if (handshake_buffer->get_available_bytes() == 0) {
			pending_request = false;
			handshake_buffer->clear();
			handshake_buffer->resize(WSL_MAX_HEADER_SIZE);
			handshake_buffer->seek(0);
		}
	} else {
		int read = 0;
		while (true) {
			int left = handshake_buffer->get_available_bytes();
			int pos = handshake_buffer->get_position();
			if (left == 0) {
				// Header is too big
				close(-1);
				ERR_FAIL_MSG("Response headers too big.");
			}

			uint8_t byte;
			Error err = connection->get_partial_data(&byte, 1, read);
			if (err != OK) {
				// Got some error.
				close(-1);
				return;
			} else if (read != 1) {
				// Busy, wait next poll.
				break;
			}
			handshake_buffer->put_u8(byte);

			// Check "\r\n\r\n" header terminator
			const char *r = (const char *)handshake_buffer->get_data_array().ptr();
			int l = pos;
			if (l > 3 && r[l] == '\n' && r[l - 1] == '\r' && r[l - 2] == '\n' && r[l - 3] == '\r') {
				// Response is over, verify headers and initialize wslay context/
				if (!_verify_server_response()) {
					close(-1);
					ERR_FAIL_MSG("Invalid response headers.");
				}
				wslay_event_context_client_init(&wsl_ctx, &_wsl_callbacks, this);
				wslay_event_config_set_no_buffering(wsl_ctx, 1);
				wslay_event_config_set_max_recv_msg_length(wsl_ctx, inbound_buffer_size);
				in_buffer.resize(nearest_shift(inbound_buffer_size), max_queued_packets);
				packet_buffer.resize(inbound_buffer_size);
				ready_state = STATE_OPEN;
				break;
			}
		}
	}
}

bool WSLPeer::_verify_server_response() {
	Vector<String> psa = String((const char *)handshake_buffer->get_data_array().ptr(), handshake_buffer->get_position() - 4).split("\r\n");
	int len = psa.size();
	ERR_FAIL_COND_V_MSG(len < 4, false, "Not enough response headers. Got: " + itos(len) + ", expected >= 4.");

	Vector<String> req = psa[0].split(" ", false);
	ERR_FAIL_COND_V_MSG(req.size() < 2, false, "Invalid protocol or status code. Got '" + psa[0] + "', expected 'HTTP/1.1 101'.");

	// Wrong protocol
	ERR_FAIL_COND_V_MSG(req[0] != "HTTP/1.1", false, "Invalid protocol. Got: '" + req[0] + "', expected 'HTTP/1.1'.");
	ERR_FAIL_COND_V_MSG(req[1] != "101", false, "Invalid status code. Got: '" + req[1] + "', expected '101'.");

	HashMap<String, String> headers;
	for (int i = 1; i < len; i++) {
		Vector<String> header = psa[i].split(":", false, 1);
		ERR_FAIL_COND_V_MSG(header.size() != 2, false, "Invalid header -> " + psa[i] + ".");
		String name = header[0].to_lower();
		String value = header[1].strip_edges();
		if (headers.has(name)) {
			headers[name] += "," + value;
		} else {
			headers[name] = value;
		}
	}

#define WSL_CHECK(NAME, VALUE)                                                          \
	ERR_FAIL_COND_V_MSG(!headers.has(NAME) || headers[NAME].to_lower() != VALUE, false, \
			"Missing or invalid header '" + String(NAME) + "'. Expected value '" + VALUE + "'.");
#define WSL_CHECK_NC(NAME, VALUE)                                            \
	ERR_FAIL_COND_V_MSG(!headers.has(NAME) || headers[NAME] != VALUE, false, \
			"Missing or invalid header '" + String(NAME) + "'. Expected value '" + VALUE + "'.");
	WSL_CHECK("connection", "upgrade");
	WSL_CHECK("upgrade", "websocket");
	WSL_CHECK_NC("sec-websocket-accept", _compute_key_response(session_key));
#undef WSL_CHECK_NC
#undef WSL_CHECK
	if (supported_protocols.size() == 0) {
		// We didn't request a custom protocol
		ERR_FAIL_COND_V_MSG(headers.has("sec-websocket-protocol"), false, "Received unrequested sub-protocol -> " + headers["sec-websocket-protocol"]);
	} else {
		// We requested at least one custom protocol but didn't receive one
		ERR_FAIL_COND_V_MSG(!headers.has("sec-websocket-protocol"), false, "Requested sub-protocol(s) but received none.");
		// Check received sub-protocol was one of those requested.
		selected_protocol = headers["sec-websocket-protocol"];
		bool valid = false;
		for (int i = 0; i < supported_protocols.size(); i++) {
			if (supported_protocols[i] != selected_protocol) {
				continue;
			}
			valid = true;
			break;
		}
		if (!valid) {
			ERR_FAIL_V_MSG(false, "Received unrequested sub-protocol -> " + selected_protocol);
		}
	}
	return true;
}

Error WSLPeer::connect_to_url(const String &p_url, Ref<TLSOptions> p_options) {
	ERR_FAIL_COND_V(p_url.is_empty(), ERR_INVALID_PARAMETER);
	ERR_FAIL_COND_V(p_options.is_valid() && p_options->is_server(), ERR_INVALID_PARAMETER);
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

	use_tls = false;
	if (scheme == "wss://") {
		use_tls = true;
	}
	if (port == 0) {
		port = use_tls ? 443 : 80;
	}
	if (path.is_empty()) {
		path = "/";
	}

	ERR_FAIL_COND_V_MSG(use_tls && !StreamPeerTLS::is_available(), ERR_UNAVAILABLE, "WSS is not available in this build.");

	requested_url = p_url;
	requested_host = host;

	if (p_options.is_valid()) {
		tls_options = p_options;
	} else {
		tls_options = TLSOptions::client();
	}

	tcp.instantiate();

	resolver.start(host, port);
	resolver.try_next_candidate(tcp);

	if (tcp->get_status() != StreamPeerTCP::STATUS_CONNECTING && tcp->get_status() != StreamPeerTCP::STATUS_CONNECTED && !resolver.has_more_candidates()) {
		_clear();
		return FAILED;
	}
	connection = tcp;

	// Prepare handshake request.
	session_key = _generate_key();
	String request = "GET " + path + " HTTP/1.1\r\n";
	String port_string;
	if ((port != 80 && !use_tls) || (port != 443 && use_tls)) {
		port_string = ":" + itos(port);
	}
	request += "Host: " + host + port_string + "\r\n";
	request += "Upgrade: websocket\r\n";
	request += "Connection: Upgrade\r\n";
	request += "Sec-WebSocket-Key: " + session_key + "\r\n";
	request += "Sec-WebSocket-Version: 13\r\n";
	if (supported_protocols.size() > 0) {
		request += "Sec-WebSocket-Protocol: ";
		for (int i = 0; i < supported_protocols.size(); i++) {
			if (i != 0) {
				request += ",";
			}
			request += supported_protocols[i];
		}
		request += "\r\n";
	}
	for (int i = 0; i < handshake_headers.size(); i++) {
		request += handshake_headers[i] + "\r\n";
	}
	request += "\r\n";
	CharString cs = request.utf8();
	handshake_buffer->put_data((const uint8_t *)cs.get_data(), cs.length());
	handshake_buffer->seek(0);
	ready_state = STATE_CONNECTING;
	is_server = false;
	return OK;
}

///
/// Callback functions.
///
ssize_t WSLPeer::_wsl_recv_callback(wslay_event_context_ptr ctx, uint8_t *data, size_t len, int flags, void *user_data) {
	WSLPeer *peer = (WSLPeer *)user_data;
	Ref<StreamPeer> conn = peer->connection;
	if (conn.is_null()) {
		wslay_event_set_error(ctx, WSLAY_ERR_CALLBACK_FAILURE);
		return -1;
	}
	// Make sure we don't read more than what our buffer can hold.
	size_t buffer_limit = MIN(peer->in_buffer.payload_space_left(), peer->in_buffer.packets_space_left() * 2); // The minimum size of a websocket message is 2 bytes.
	size_t to_read = MIN(len, buffer_limit);
	if (to_read == 0) {
		wslay_event_set_error(ctx, WSLAY_ERR_WOULDBLOCK);
		return -1;
	}
	int read = 0;
	Error err = conn->get_partial_data(data, to_read, read);
	if (err != OK) {
		print_verbose("Websocket get data error: " + itos(err) + ", read (should be 0!): " + itos(read));
		wslay_event_set_error(ctx, WSLAY_ERR_CALLBACK_FAILURE);
		return -1;
	}
	if (read == 0) {
		wslay_event_set_error(ctx, WSLAY_ERR_WOULDBLOCK);
		return -1;
	}
	return read;
}

void WSLPeer::_wsl_recv_start_callback(wslay_event_context_ptr ctx, const struct wslay_event_on_frame_recv_start_arg *arg, void *user_data) {
	WSLPeer *peer = (WSLPeer *)user_data;
	uint8_t op = arg->opcode;
	if (op == WSLAY_TEXT_FRAME || op == WSLAY_BINARY_FRAME) {
		// Get ready to process a data package.
		PendingMessage &pm = peer->pending_message;
		pm.opcode = op;
		pm.payload_size = arg->payload_length;
	}
}

void WSLPeer::_wsl_frame_recv_chunk_callback(wslay_event_context_ptr ctx, const struct wslay_event_on_frame_recv_chunk_arg *arg, void *user_data) {
	WSLPeer *peer = (WSLPeer *)user_data;
	PendingMessage &pm = peer->pending_message;
	if (pm.opcode != 0) {
		// Only write the payload.
		peer->in_buffer.write_packet(arg->data, arg->data_length, nullptr);
	}
}

ssize_t WSLPeer::_wsl_send_callback(wslay_event_context_ptr ctx, const uint8_t *data, size_t len, int flags, void *user_data) {
	WSLPeer *peer = (WSLPeer *)user_data;
	Ref<StreamPeer> conn = peer->connection;
	if (conn.is_null()) {
		wslay_event_set_error(ctx, WSLAY_ERR_CALLBACK_FAILURE);
		return -1;
	}
	int sent = 0;
	Error err = conn->put_partial_data(data, len, sent);
	if (err != OK) {
		wslay_event_set_error(ctx, WSLAY_ERR_CALLBACK_FAILURE);
		return -1;
	}
	if (sent == 0) {
		wslay_event_set_error(ctx, WSLAY_ERR_WOULDBLOCK);
		return -1;
	}
	return sent;
}

int WSLPeer::_wsl_genmask_callback(wslay_event_context_ptr ctx, uint8_t *buf, size_t len, void *user_data) {
	ERR_FAIL_NULL_V(_static_rng, WSLAY_ERR_CALLBACK_FAILURE);
	Error err = _static_rng->get_random_bytes(buf, len);
	ERR_FAIL_COND_V(err != OK, WSLAY_ERR_CALLBACK_FAILURE);
	return 0;
}

void WSLPeer::_wsl_msg_recv_callback(wslay_event_context_ptr ctx, const struct wslay_event_on_msg_recv_arg *arg, void *user_data) {
	WSLPeer *peer = (WSLPeer *)user_data;
	uint8_t op = arg->opcode;

	if (op == WSLAY_CONNECTION_CLOSE) {
		// Close request or confirmation.
		peer->close_code = arg->status_code;
		size_t len = arg->msg_length;
		peer->close_reason = "";
		if (len > 2 /* first 2 bytes = close code */) {
			peer->close_reason.parse_utf8((char *)arg->msg + 2, len - 2);
		}
		if (peer->ready_state == STATE_OPEN) {
			peer->ready_state = STATE_CLOSING;
		}
		return;
	}

	if (op == WSLAY_PONG) {
		peer->heartbeat_waiting = false;
	} else if (op == WSLAY_TEXT_FRAME || op == WSLAY_BINARY_FRAME) {
		PendingMessage &pm = peer->pending_message;
		ERR_FAIL_COND(pm.opcode != op);
		// Only write the packet (since it's now completed).
		uint8_t is_string = pm.opcode == WSLAY_TEXT_FRAME ? 1 : 0;
		peer->in_buffer.write_packet(nullptr, pm.payload_size, &is_string);
		pm.clear();
	}
	// Ping.
}

wslay_event_callbacks WSLPeer::_wsl_callbacks = {
	_wsl_recv_callback,
	_wsl_send_callback,
	_wsl_genmask_callback,
	_wsl_recv_start_callback,
	_wsl_frame_recv_chunk_callback,
	nullptr,
	_wsl_msg_recv_callback
};

String WSLPeer::_generate_key() {
	// Random key
	Vector<uint8_t> bkey;
	int len = 16; // 16 bytes, as per RFC
	bkey.resize(len);
	_wsl_genmask_callback(nullptr, bkey.ptrw(), len, nullptr);
	return CryptoCore::b64_encode_str(bkey.ptrw(), len);
}

String WSLPeer::_compute_key_response(String p_key) {
	String key = p_key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"; // Magic UUID as per RFC
	Vector<uint8_t> sha = key.sha1_buffer();
	return CryptoCore::b64_encode_str(sha.ptr(), sha.size());
}

void WSLPeer::poll() {
	// Nothing to do.
	if (ready_state == STATE_CLOSED) {
		return;
	}

	if (ready_state == STATE_CONNECTING) {
		if (is_server) {
			_do_server_handshake();
		} else {
			_do_client_handshake();
		}
	}

	if (ready_state == STATE_OPEN || ready_state == STATE_CLOSING) {
		ERR_FAIL_NULL(wsl_ctx);
		uint64_t ticks = OS::get_singleton()->get_ticks_msec();
		int err = 0;
		if (heartbeat_interval_msec != 0 && ticks - last_heartbeat > heartbeat_interval_msec && ready_state == STATE_OPEN) {
			if (heartbeat_waiting) {
				wslay_event_context_free(wsl_ctx);
				wsl_ctx = nullptr;
				close(-1);
				return;
			}
			heartbeat_waiting = true;
			struct wslay_event_msg msg;
			msg.opcode = WSLAY_PING;
			msg.msg = nullptr;
			msg.msg_length = 0;
			err = wslay_event_queue_msg(wsl_ctx, &msg);
			if (err == 0) {
				last_heartbeat = ticks;
			} else {
				print_verbose("Websocket (wslay) failed to send ping: " + itos(err));
				wslay_event_context_free(wsl_ctx);
				wsl_ctx = nullptr;
				close(-1);
				return;
			}
		}
		if ((err = wslay_event_recv(wsl_ctx)) != 0 || (err = wslay_event_send(wsl_ctx)) != 0) {
			// Error close.
			print_verbose("Websocket (wslay) poll error: " + itos(err));
			wslay_event_context_free(wsl_ctx);
			wsl_ctx = nullptr;
			close(-1);
			return;
		}
		if (wslay_event_get_close_sent(wsl_ctx)) {
			if (wslay_event_get_close_received(wsl_ctx)) {
				// Clean close.
				wslay_event_context_free(wsl_ctx);
				wsl_ctx = nullptr;
				close(-1);
				return;
			} else if (!wslay_event_get_read_enabled(wsl_ctx)) {
				// Some protocol error caused wslay to stop processing incoming events, we'll never receive a close from the other peer.
				close_code = wslay_event_get_status_code_sent(wsl_ctx);
				switch (close_code) {
					case WSLAY_CODE_MESSAGE_TOO_BIG:
						close_reason = "Message too big";
						break;
					case WSLAY_CODE_PROTOCOL_ERROR:
						close_reason = "Protocol error";
						break;
					case WSLAY_CODE_ABNORMAL_CLOSURE:
						close_reason = "Abnormal closure";
						break;
					case WSLAY_CODE_INVALID_FRAME_PAYLOAD_DATA:
						close_reason = "Invalid frame payload data";
						break;
					default:
						close_reason = "Unknown";
				}
				wslay_event_context_free(wsl_ctx);
				wsl_ctx = nullptr;
				close(-1);
				return;
			}
		}
	}
}

Error WSLPeer::_send(const uint8_t *p_buffer, int p_buffer_size, wslay_opcode p_opcode) {
	ERR_FAIL_COND_V(ready_state != STATE_OPEN, FAILED);
	ERR_FAIL_COND_V(wslay_event_get_queued_msg_count(wsl_ctx) >= (uint32_t)max_queued_packets, ERR_OUT_OF_MEMORY);
	ERR_FAIL_COND_V(outbound_buffer_size > 0 && (wslay_event_get_queued_msg_length(wsl_ctx) + p_buffer_size > (uint32_t)outbound_buffer_size), ERR_OUT_OF_MEMORY);

	struct wslay_event_msg msg;
	msg.opcode = p_opcode;
	msg.msg = p_buffer;
	msg.msg_length = p_buffer_size;

	// Queue & send message.
	if (wslay_event_queue_msg(wsl_ctx, &msg) != 0 || wslay_event_send(wsl_ctx) != 0) {
		close(-1);
		return FAILED;
	}
	return OK;
}

Error WSLPeer::send(const uint8_t *p_buffer, int p_buffer_size, WriteMode p_mode) {
	wslay_opcode opcode = p_mode == WRITE_MODE_TEXT ? WSLAY_TEXT_FRAME : WSLAY_BINARY_FRAME;
	return _send(p_buffer, p_buffer_size, opcode);
}

Error WSLPeer::put_packet(const uint8_t *p_buffer, int p_buffer_size) {
	return _send(p_buffer, p_buffer_size, WSLAY_BINARY_FRAME);
}

Error WSLPeer::get_packet(const uint8_t **r_buffer, int &r_buffer_size) {
	r_buffer_size = 0;

	ERR_FAIL_COND_V(ready_state != STATE_OPEN, FAILED);

	if (in_buffer.packets_left() == 0) {
		return ERR_UNAVAILABLE;
	}

	int read = 0;
	uint8_t *rw = packet_buffer.ptrw();
	in_buffer.read_packet(rw, packet_buffer.size(), &was_string, read);

	*r_buffer = rw;
	r_buffer_size = read;

	return OK;
}

int WSLPeer::get_available_packet_count() const {
	if (ready_state != STATE_OPEN) {
		return 0;
	}

	return in_buffer.packets_left();
}

int WSLPeer::get_current_outbound_buffered_amount() const {
	if (ready_state != STATE_OPEN) {
		return 0;
	}

	return wslay_event_get_queued_msg_length(wsl_ctx);
}

void WSLPeer::close(int p_code, String p_reason) {
	if (p_code < 0) {
		// Force immediate close.
		ready_state = STATE_CLOSED;
	}

	if (ready_state == STATE_OPEN && !wslay_event_get_close_sent(wsl_ctx)) {
		CharString cs = p_reason.utf8();
		wslay_event_queue_close(wsl_ctx, p_code, (uint8_t *)cs.ptr(), cs.length());
		wslay_event_send(wsl_ctx);
		ready_state = STATE_CLOSING;
	} else if (ready_state == STATE_CONNECTING || ready_state == STATE_CLOSED) {
		ready_state = STATE_CLOSED;
		connection.unref();
		if (tcp.is_valid()) {
			tcp->disconnect_from_host();
			tcp.unref();
		}
	}

	heartbeat_waiting = false;
	in_buffer.clear();
	packet_buffer.resize(0);
	pending_message.clear();
}

IPAddress WSLPeer::get_connected_host() const {
	ERR_FAIL_COND_V(tcp.is_null(), IPAddress());
	return tcp->get_connected_host();
}

uint16_t WSLPeer::get_connected_port() const {
	ERR_FAIL_COND_V(tcp.is_null(), 0);
	return tcp->get_connected_port();
}

String WSLPeer::get_selected_protocol() const {
	return selected_protocol;
}

String WSLPeer::get_requested_url() const {
	return requested_url;
}

void WSLPeer::set_no_delay(bool p_enabled) {
	ERR_FAIL_COND(tcp.is_null());
	tcp->set_no_delay(p_enabled);
}

void WSLPeer::_clear() {
	// Connection info.
	ready_state = STATE_CLOSED;
	is_server = false;
	connection.unref();
	if (tcp.is_valid()) {
		tcp->disconnect_from_host();
		tcp.unref();
	}
	if (wsl_ctx) {
		wslay_event_context_free(wsl_ctx);
		wsl_ctx = nullptr;
	}

	resolver.stop();
	requested_url.clear();
	requested_host.clear();
	pending_request = true;
	handshake_buffer->clear();
	selected_protocol.clear();
	session_key.clear();

	// Pending packets info.
	was_string = 0;
	in_buffer.clear();
	packet_buffer.clear();

	// Close code info.
	close_code = -1;
	close_reason.clear();
}

WSLPeer::WSLPeer() {
	handshake_buffer.instantiate();
}

WSLPeer::~WSLPeer() {
	close(-1);
}

#endif // WEB_ENABLED

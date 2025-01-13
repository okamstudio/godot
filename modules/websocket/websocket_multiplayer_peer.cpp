/**************************************************************************/
/*  websocket_multiplayer_peer.cpp                                        */
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

#include "websocket_multiplayer_peer.h"

#include "core/io/stream_peer_tls.h"
#include "core/os/os.h"

WebSocketMultiplayerPeer::WebSocketMultiplayerPeer() {
	peer_config = Ref<WebSocketPeer>(WebSocketPeer::create());
}

WebSocketMultiplayerPeer::~WebSocketMultiplayerPeer() {
	_clear();
}

Ref<WebSocketPeer> WebSocketMultiplayerPeer::_create_peer() {
	Ref<WebSocketPeer> peer = Ref<WebSocketPeer>(WebSocketPeer::create());
	peer->set_supported_protocols(get_supported_protocols());
	peer->set_handshake_headers(get_handshake_headers());
	peer->set_inbound_buffer_size(get_inbound_buffer_size());
	peer->set_outbound_buffer_size(get_outbound_buffer_size());
	peer->set_max_queued_packets(get_max_queued_packets());
	return peer;
}

void WebSocketMultiplayerPeer::_clear() {
	connection_status = CONNECTION_DISCONNECTED;
	unique_id = 0;
	peers_map.clear();
	tcp_server.unref();
	pending_peers.clear();
	tls_server_options.unref();
	if (current_packet.data != nullptr) {
		memfree(current_packet.data);
		current_packet.data = nullptr;
	}

	for (Packet &E : incoming_packets) {
		memfree(E.data);
		E.data = nullptr;
	}

	incoming_packets.clear();
}

void WebSocketMultiplayerPeer::_bind_methods() {
	ClassDB::bind_method(D_METHOD("create_client", "url", "tls_client_options"), &WebSocketMultiplayerPeer::create_client, DEFVAL(Ref<TLSOptions>()));
	ClassDB::bind_method(D_METHOD("create_server", "port", "bind_address", "tls_server_options"), &WebSocketMultiplayerPeer::create_server, DEFVAL("*"), DEFVAL(Ref<TLSOptions>()));

	ClassDB::bind_method(D_METHOD("get_peer", "peer_id"), &WebSocketMultiplayerPeer::get_peer);
	ClassDB::bind_method(D_METHOD("get_peer_address", "id"), &WebSocketMultiplayerPeer::get_peer_address);
	ClassDB::bind_method(D_METHOD("get_peer_port", "id"), &WebSocketMultiplayerPeer::get_peer_port);

	ClassDB::bind_method(D_METHOD("get_supported_protocols"), &WebSocketMultiplayerPeer::get_supported_protocols);
	ClassDB::bind_method(D_METHOD("set_supported_protocols", "protocols"), &WebSocketMultiplayerPeer::set_supported_protocols);

	ClassDB::bind_method(D_METHOD("get_handshake_headers"), &WebSocketMultiplayerPeer::get_handshake_headers);
	ClassDB::bind_method(D_METHOD("set_handshake_headers", "protocols"), &WebSocketMultiplayerPeer::set_handshake_headers);

	ClassDB::bind_method(D_METHOD("get_inbound_buffer_size"), &WebSocketMultiplayerPeer::get_inbound_buffer_size);
	ClassDB::bind_method(D_METHOD("set_inbound_buffer_size", "buffer_size"), &WebSocketMultiplayerPeer::set_inbound_buffer_size);

	ClassDB::bind_method(D_METHOD("get_outbound_buffer_size"), &WebSocketMultiplayerPeer::get_outbound_buffer_size);
	ClassDB::bind_method(D_METHOD("set_outbound_buffer_size", "buffer_size"), &WebSocketMultiplayerPeer::set_outbound_buffer_size);

	ClassDB::bind_method(D_METHOD("get_handshake_timeout"), &WebSocketMultiplayerPeer::get_handshake_timeout);
	ClassDB::bind_method(D_METHOD("set_handshake_timeout", "timeout"), &WebSocketMultiplayerPeer::set_handshake_timeout);

	ClassDB::bind_method(D_METHOD("set_max_queued_packets", "max_queued_packets"), &WebSocketMultiplayerPeer::set_max_queued_packets);
	ClassDB::bind_method(D_METHOD("get_max_queued_packets"), &WebSocketMultiplayerPeer::get_max_queued_packets);

	ADD_PROPERTY(PropertyInfo(Variant::PACKED_STRING_ARRAY, "supported_protocols"), "set_supported_protocols", "get_supported_protocols");
	ADD_PROPERTY(PropertyInfo(Variant::PACKED_STRING_ARRAY, "handshake_headers"), "set_handshake_headers", "get_handshake_headers");

	ADD_PROPERTY(PropertyInfo(Variant::INT, "inbound_buffer_size"), "set_inbound_buffer_size", "get_inbound_buffer_size");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "outbound_buffer_size"), "set_outbound_buffer_size", "get_outbound_buffer_size");

	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "handshake_timeout"), "set_handshake_timeout", "get_handshake_timeout");

	ADD_PROPERTY(PropertyInfo(Variant::INT, "max_queued_packets"), "set_max_queued_packets", "get_max_queued_packets");
}

//
// PacketPeer
//
int WebSocketMultiplayerPeer::get_available_packet_count() const {
	return incoming_packets.size();
}

Error WebSocketMultiplayerPeer::get_packet(const uint8_t **r_buffer, int &r_buffer_size) {
	ERR_FAIL_COND_V(get_connection_status() != CONNECTION_CONNECTED, ERR_UNCONFIGURED);

	r_buffer_size = 0;

	if (current_packet.data != nullptr) {
		memfree(current_packet.data);
		current_packet.data = nullptr;
	}

	ERR_FAIL_COND_V(incoming_packets.is_empty(), ERR_UNAVAILABLE);

	current_packet = incoming_packets.front()->get();
	incoming_packets.pop_front();

	*r_buffer = current_packet.data;
	r_buffer_size = current_packet.size;

	return OK;
}

Error WebSocketMultiplayerPeer::put_packet(const uint8_t *p_buffer, int p_buffer_size) {
	ERR_FAIL_COND_V(get_connection_status() != CONNECTION_CONNECTED, ERR_UNCONFIGURED);

	if (is_server()) {
		if (target_peer > 0) {
			ERR_FAIL_COND_V_MSG(!peers_map.has(target_peer), ERR_INVALID_PARAMETER, "Peer not found: " + itos(target_peer));
			get_peer(target_peer)->put_packet(p_buffer, p_buffer_size);
		} else {
			for (KeyValue<int, Ref<WebSocketPeer>> &E : peers_map) {
				if (target_peer && -target_peer == E.key) {
					continue; // Excluded.
				}
				E.value->put_packet(p_buffer, p_buffer_size);
			}
		}
		return OK;
	} else {
		return get_peer(1)->put_packet(p_buffer, p_buffer_size);
	}
}

//
// MultiplayerPeer
//
void WebSocketMultiplayerPeer::set_target_peer(int p_target_peer) {
	target_peer = p_target_peer;
}

int WebSocketMultiplayerPeer::get_packet_peer() const {
	ERR_FAIL_COND_V(incoming_packets.is_empty(), 1);

	return incoming_packets.front()->get().source;
}

int WebSocketMultiplayerPeer::get_unique_id() const {
	return unique_id;
}

int WebSocketMultiplayerPeer::get_max_packet_size() const {
	return get_outbound_buffer_size() - PROTO_SIZE;
}

Error WebSocketMultiplayerPeer::create_server(int p_port, IPAddress p_bind_ip, Ref<TLSOptions> p_options) {
	ERR_FAIL_COND_V(get_connection_status() != CONNECTION_DISCONNECTED, ERR_ALREADY_IN_USE);
	ERR_FAIL_COND_V(p_options.is_valid() && !p_options->is_server(), ERR_INVALID_PARAMETER);
	_clear();
	tcp_server.instantiate();
	Error err = tcp_server->listen(p_port, p_bind_ip);
	if (err != OK) {
		tcp_server.unref();
		return err;
	}
	unique_id = 1;
	connection_status = CONNECTION_CONNECTED;
	tls_server_options = p_options;
	return OK;
}

Error WebSocketMultiplayerPeer::create_client(const String &p_url, Ref<TLSOptions> p_options) {
	ERR_FAIL_COND_V(get_connection_status() != CONNECTION_DISCONNECTED, ERR_ALREADY_IN_USE);
	ERR_FAIL_COND_V(p_options.is_valid() && p_options->is_server(), ERR_INVALID_PARAMETER);
	_clear();
	Ref<WebSocketPeer> peer = _create_peer();
	Error err = peer->connect_to_url(p_url, p_options);
	if (err != OK) {
		return err;
	}
	PendingPeer pending;
	pending.time = OS::get_singleton()->get_ticks_msec();
	pending_peers[1] = pending;
	peers_map[1] = peer;
	connection_status = CONNECTION_CONNECTING;
	return OK;
}

bool WebSocketMultiplayerPeer::is_server() const {
	return tcp_server.is_valid();
}

void WebSocketMultiplayerPeer::_poll_client() {
	ERR_FAIL_COND(connection_status == CONNECTION_DISCONNECTED); // Bug.
	ERR_FAIL_COND(!peers_map.has(1) || peers_map[1].is_null()); // Bug.
	Ref<WebSocketPeer> peer = peers_map[1];
	peer->poll(); // Update state and fetch packets.
	WebSocketPeer::State ready_state = peer->get_ready_state();
	if (ready_state == WebSocketPeer::STATE_OPEN) {
		if (connection_status == CONNECTION_CONNECTING) {
			if (peer->get_available_packet_count() > 0) {
				const uint8_t *in_buffer;
				int size = 0;
				Error err = peer->get_packet(&in_buffer, size);
				if (err != OK || size != 4) {
					peer->close(); // Will cause connection error on next poll.
					ERR_FAIL_MSG("Invalid ID received from server");
				}
				unique_id = *((int32_t *)in_buffer);
				if (unique_id < 2) {
					peer->close(); // Will cause connection error on next poll.
					ERR_FAIL_MSG("Invalid ID received from server");
				}
				connection_status = CONNECTION_CONNECTED;
				emit_signal("peer_connected", 1);
			} else {
				return; // Still waiting for an ID.
			}
		}
		int pkts = peer->get_available_packet_count();
		while (pkts > 0 && peer->get_ready_state() == WebSocketPeer::STATE_OPEN) {
			const uint8_t *in_buffer;
			int size = 0;
			Error err = peer->get_packet(&in_buffer, size);
			ERR_FAIL_COND(err != OK);
			ERR_FAIL_COND(size <= 0);
			Packet packet;
			packet.data = (uint8_t *)memalloc(size);
			memcpy(packet.data, in_buffer, size);
			packet.size = size;
			packet.source = 1;
			incoming_packets.push_back(packet);
			pkts--;
		}
	} else if (peer->get_ready_state() == WebSocketPeer::STATE_CLOSED) {
		if (connection_status == CONNECTION_CONNECTED) {
			emit_signal(SNAME("peer_disconnected"), 1);
		}
		_clear();
		return;
	}
	if (connection_status == CONNECTION_CONNECTING) {
		// Still connecting
		ERR_FAIL_COND(!pending_peers.has(1)); // Bug.
		if (OS::get_singleton()->get_ticks_msec() - pending_peers[1].time > handshake_timeout) {
			print_verbose(vformat("WebSocket handshake timed out after %.3f seconds.", handshake_timeout * 0.001));
			_clear();
			return;
		}
	}
}

void WebSocketMultiplayerPeer::_poll_server() {
	ERR_FAIL_COND(connection_status != CONNECTION_CONNECTED); // Bug.
	ERR_FAIL_COND(tcp_server.is_null() || !tcp_server->is_listening()); // Bug.

	// Accept new connections.
	if (!is_refusing_new_connections() && tcp_server->is_connection_available()) {
		PendingPeer peer;
		peer.time = OS::get_singleton()->get_ticks_msec();
		peer.tcp = tcp_server->take_connection();
		peer.connection = peer.tcp;
		pending_peers[generate_unique_id()] = peer;
	}

	// Process pending peers.
	HashSet<int> to_remove;
	for (KeyValue<int, PendingPeer> &E : pending_peers) {
		PendingPeer &peer = E.value;
		int id = E.key;

		if (OS::get_singleton()->get_ticks_msec() - peer.time > handshake_timeout) {
			print_verbose(vformat("WebSocket handshake timed out after %.3f seconds.", handshake_timeout * 0.001));
			to_remove.insert(id);
			continue;
		}

		if (peer.ws.is_valid()) {
			peer.ws->poll();
			WebSocketPeer::State state = peer.ws->get_ready_state();
			if (state == WebSocketPeer::STATE_OPEN) {
				// Connected.
				to_remove.insert(id);
				if (is_refusing_new_connections()) {
					// The user does not want new connections, dropping it.
					continue;
				}
				int32_t peer_id = id;
				Error err = peer.ws->put_packet((const uint8_t *)&peer_id, sizeof(peer_id));
				if (err == OK) {
					peers_map[id] = peer.ws;
					emit_signal("peer_connected", id);
				} else {
					ERR_PRINT("Failed to send ID to newly connected peer.");
				}
				continue;
			} else if (state == WebSocketPeer::STATE_CONNECTING) {
				continue; // Still connecting.
			}
			to_remove.insert(id); // Error.
			continue;
		}
		if (peer.tcp->get_status() != StreamPeerTCP::STATUS_CONNECTED) {
			to_remove.insert(id); // Error.
			continue;
		}
		if (tls_server_options.is_null()) {
			peer.ws = _create_peer();
			peer.ws->accept_stream(peer.tcp);
			continue;
		} else {
			if (peer.connection == peer.tcp) {
				Ref<StreamPeerTLS> tls = Ref<StreamPeerTLS>(StreamPeerTLS::create());
				Error err = tls->accept_stream(peer.tcp, tls_server_options);
				if (err != OK) {
					to_remove.insert(id);
					continue;
				}
				peer.connection = tls;
			}
			Ref<StreamPeerTLS> tls = static_cast<Ref<StreamPeerTLS>>(peer.connection);
			tls->poll();
			if (tls->get_status() == StreamPeerTLS::STATUS_CONNECTED) {
				peer.ws = _create_peer();
				peer.ws->accept_stream(peer.connection);
				continue;
			} else if (tls->get_status() == StreamPeerTLS::STATUS_HANDSHAKING) {
				// Still connecting.
				continue;
			} else {
				// Error
				to_remove.insert(id);
			}
		}
	}

	// Remove disconnected pending peers.
	for (const int &pid : to_remove) {
		pending_peers.erase(pid);
	}
	to_remove.clear();

	// Process connected peers.
	for (KeyValue<int, Ref<WebSocketPeer>> &E : peers_map) {
		Ref<WebSocketPeer> ws = E.value;
		int id = E.key;
		ws->poll();
		if (ws->get_ready_state() != WebSocketPeer::STATE_OPEN) {
			to_remove.insert(id); // Disconnected.
			continue;
		}
		// Fetch packets
		int pkts = ws->get_available_packet_count();
		while (pkts > 0 && ws->get_ready_state() == WebSocketPeer::STATE_OPEN) {
			const uint8_t *in_buffer;
			int size = 0;
			Error err = ws->get_packet(&in_buffer, size);
			if (err != OK || size <= 0) {
				break;
			}
			Packet packet;
			packet.data = (uint8_t *)memalloc(size);
			memcpy(packet.data, in_buffer, size);
			packet.size = size;
			packet.source = E.key;
			incoming_packets.push_back(packet);
			pkts--;
		}
	}

	// Remove disconnected peers.
	for (const int &pid : to_remove) {
		emit_signal(SNAME("peer_disconnected"), pid);
		peers_map.erase(pid);
	}
}

void WebSocketMultiplayerPeer::poll() {
	if (connection_status == CONNECTION_DISCONNECTED) {
		return;
	}
	if (is_server()) {
		_poll_server();
	} else {
		_poll_client();
	}
}

MultiplayerPeer::ConnectionStatus WebSocketMultiplayerPeer::get_connection_status() const {
	return connection_status;
}

Ref<WebSocketPeer> WebSocketMultiplayerPeer::get_peer(int p_id) const {
	ERR_FAIL_COND_V(!peers_map.has(p_id), Ref<WebSocketPeer>());
	return peers_map[p_id];
}

void WebSocketMultiplayerPeer::set_supported_protocols(const Vector<String> &p_protocols) {
	peer_config->set_supported_protocols(p_protocols);
}

Vector<String> WebSocketMultiplayerPeer::get_supported_protocols() const {
	return peer_config->get_supported_protocols();
}

void WebSocketMultiplayerPeer::set_handshake_headers(const Vector<String> &p_headers) {
	peer_config->set_handshake_headers(p_headers);
}

Vector<String> WebSocketMultiplayerPeer::get_handshake_headers() const {
	return peer_config->get_handshake_headers();
}

void WebSocketMultiplayerPeer::set_outbound_buffer_size(int p_buffer_size) {
	peer_config->set_outbound_buffer_size(p_buffer_size);
}

int WebSocketMultiplayerPeer::get_outbound_buffer_size() const {
	return peer_config->get_outbound_buffer_size();
}

void WebSocketMultiplayerPeer::set_inbound_buffer_size(int p_buffer_size) {
	peer_config->set_inbound_buffer_size(p_buffer_size);
}

int WebSocketMultiplayerPeer::get_inbound_buffer_size() const {
	return peer_config->get_inbound_buffer_size();
}

void WebSocketMultiplayerPeer::set_max_queued_packets(int p_max_queued_packets) {
	peer_config->set_max_queued_packets(p_max_queued_packets);
}

int WebSocketMultiplayerPeer::get_max_queued_packets() const {
	return peer_config->get_max_queued_packets();
}

float WebSocketMultiplayerPeer::get_handshake_timeout() const {
	return handshake_timeout / 1000.0;
}

void WebSocketMultiplayerPeer::set_handshake_timeout(float p_timeout) {
	ERR_FAIL_COND(p_timeout <= 0.0);
	handshake_timeout = p_timeout * 1000;
}

IPAddress WebSocketMultiplayerPeer::get_peer_address(int p_peer_id) const {
	ERR_FAIL_COND_V(!peers_map.has(p_peer_id), IPAddress());
	return peers_map[p_peer_id]->get_connected_host();
}

int WebSocketMultiplayerPeer::get_peer_port(int p_peer_id) const {
	ERR_FAIL_COND_V(!peers_map.has(p_peer_id), 0);
	return peers_map[p_peer_id]->get_connected_port();
}

void WebSocketMultiplayerPeer::disconnect_peer(int p_peer_id, bool p_force) {
	ERR_FAIL_COND(!peers_map.has(p_peer_id));
	peers_map[p_peer_id]->close();
	if (p_force) {
		peers_map.erase(p_peer_id);
		if (!is_server()) {
			_clear();
		}
	}
}

void WebSocketMultiplayerPeer::close() {
	_clear();
}

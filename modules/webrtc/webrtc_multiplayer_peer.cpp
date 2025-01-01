/**************************************************************************/
/*  webrtc_multiplayer_peer.cpp                                           */
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

#include "webrtc_multiplayer_peer.h"

void WebRTCMultiplayerPeer::_bind_methods() {
	ClassDB::bind_method(D_METHOD("create_server", "channels_config"), &WebRTCMultiplayerPeer::create_server, DEFVAL(Array()));
	ClassDB::bind_method(D_METHOD("create_client", "peer_id", "channels_config"), &WebRTCMultiplayerPeer::create_client, DEFVAL(Array()));
	ClassDB::bind_method(D_METHOD("create_mesh", "peer_id", "channels_config"), &WebRTCMultiplayerPeer::create_mesh, DEFVAL(Array()));
	ClassDB::bind_method(D_METHOD("add_peer", "peer", "peer_id", "unreliable_lifetime"), &WebRTCMultiplayerPeer::add_peer, DEFVAL(1));
	ClassDB::bind_method(D_METHOD("remove_peer", "peer_id"), &WebRTCMultiplayerPeer::remove_peer);
	ClassDB::bind_method(D_METHOD("has_peer", "peer_id"), &WebRTCMultiplayerPeer::has_peer);
	ClassDB::bind_method(D_METHOD("get_peer", "peer_id"), &WebRTCMultiplayerPeer::get_peer);
	ClassDB::bind_method(D_METHOD("get_peers"), &WebRTCMultiplayerPeer::get_peers);
}

void WebRTCMultiplayerPeer::set_target_peer(int p_peer_id) {
	target_peer = p_peer_id;
}

/* Returns the ID of the MultiplayerPeer who sent the most recent packet: */
int WebRTCMultiplayerPeer::get_packet_peer() const {
	return next_packet_peer;
}

int WebRTCMultiplayerPeer::get_packet_channel() const {
	return next_packet_channel < CH_RESERVED_MAX ? 0 : next_packet_channel - CH_RESERVED_MAX + 1;
}

MultiplayerPeer::TransferMode WebRTCMultiplayerPeer::get_packet_mode() const {
	ERR_FAIL_INDEX_V(next_packet_channel, channels_modes.size(), TRANSFER_MODE_RELIABLE);
	return channels_modes.get(next_packet_channel);
}

bool WebRTCMultiplayerPeer::is_server() const {
	return unique_id == TARGET_PEER_SERVER;
}

void WebRTCMultiplayerPeer::poll() {
	if (peer_map.size() == 0) {
		return;
	}

	List<int> remove;
	List<int> add;
	for (KeyValue<int, Ref<ConnectedPeer>> &E : peer_map) {
		Ref<ConnectedPeer> peer = E.value;
		peer->connection->poll();
		// Check peer state
		switch (peer->connection->get_connection_state()) {
			case WebRTCPeerConnection::STATE_NEW:
			case WebRTCPeerConnection::STATE_CONNECTING:
				// Go to next peer, not ready yet.
				continue;
			case WebRTCPeerConnection::STATE_CONNECTED:
				// Good to go, go ahead and check channel state.
				break;
			default:
				// Peer is closed or in error state. Got to next peer.
				remove.push_back(E.key);
				continue;
		}
		// Check channels state
		int ready = 0;
		for (List<Ref<WebRTCDataChannel>>::Element *C = peer->channels.front(); C && C->get().is_valid(); C = C->next()) {
			Ref<WebRTCDataChannel> ch = C->get();
			switch (ch->get_ready_state()) {
				case WebRTCDataChannel::STATE_CONNECTING:
					continue;
				case WebRTCDataChannel::STATE_OPEN:
					ready++;
					continue;
				default:
					// Channel was closed or in error state, remove peer id.
					remove.push_back(E.key);
			}
			// We got a closed channel break out, the peer will be removed.
			break;
		}
		// This peer has newly connected, and all channels are now open.
		if (ready == peer->channels.size() && !peer->connected) {
			peer->connected = true;
			add.push_back(E.key);
		}
	}
	// Remove disconnected peers
	for (int &E : remove) {
		remove_peer(E);
		if (next_packet_peer == E) {
			next_packet_peer = 0;
		}
	}
	// Signal newly connected peers
	for (int &E : add) {
		// Already connected to server: simply notify new peer.
		if (network_mode == MODE_CLIENT) {
			ERR_CONTINUE(E != TARGET_PEER_SERVER); // Bug.
			// Server connected.
			connection_status = CONNECTION_CONNECTED;
			emit_signal(SNAME("peer_connected"), TARGET_PEER_SERVER);
		} else {
			emit_signal(SNAME("peer_connected"), E);
		}
	}
	// Fetch next packet
	if (next_packet_peer == 0) {
		_find_next_peer();
	}
}

void WebRTCMultiplayerPeer::_find_next_peer() {
	HashMap<int, Ref<ConnectedPeer>>::Iterator E = peer_map.find(next_packet_peer);
	if (E) {
		++E;
	}
	// After last.
	while (E) {
		if (!E->value->connected) {
			++E;
			continue;
		}
		int idx = 0;
		for (const Ref<WebRTCDataChannel> &F : E->value->channels) {
			if (F->get_available_packet_count()) {
				next_packet_channel = idx;
				next_packet_peer = E->key;
				return;
			}
			idx++;
		}
		++E;
	}
	E = peer_map.begin();
	// Before last
	while (E) {
		if (!E->value->connected) {
			++E;
			continue;
		}
		int idx = 0;
		for (const Ref<WebRTCDataChannel> &F : E->value->channels) {
			if (F->get_available_packet_count()) {
				next_packet_channel = idx;
				next_packet_peer = E->key;
				return;
			}
			idx++;
		}
		if (E->key == (int)next_packet_peer) {
			break;
		}
		++E;
	}
	// No packet found
	next_packet_channel = 0;
	next_packet_peer = 0;
}

MultiplayerPeer::ConnectionStatus WebRTCMultiplayerPeer::get_connection_status() const {
	return connection_status;
}

Error WebRTCMultiplayerPeer::create_server(Array p_channels_config) {
	return _initialize(1, MODE_SERVER, p_channels_config);
}

Error WebRTCMultiplayerPeer::create_client(int p_self_id, Array p_channels_config) {
	ERR_FAIL_COND_V_MSG(p_self_id == 1, ERR_INVALID_PARAMETER, "Clients cannot have ID 1.");
	return _initialize(p_self_id, MODE_CLIENT, p_channels_config);
}

Error WebRTCMultiplayerPeer::create_mesh(int p_self_id, Array p_channels_config) {
	return _initialize(p_self_id, MODE_MESH, p_channels_config);
}

Error WebRTCMultiplayerPeer::_initialize(int p_self_id, NetworkMode p_mode, Array p_channels_config) {
	ERR_FAIL_COND_V(p_self_id < 1 || p_self_id > ~(1 << 31), ERR_INVALID_PARAMETER);
	channels_config.clear();
	channels_modes.clear();
	channels_modes.push_back(TRANSFER_MODE_RELIABLE);
	channels_modes.push_back(TRANSFER_MODE_UNRELIABLE_ORDERED);
	channels_modes.push_back(TRANSFER_MODE_UNRELIABLE);
	for (int i = 0; i < p_channels_config.size(); i++) {
		ERR_FAIL_COND_V_MSG(p_channels_config[i].get_type() != Variant::INT, ERR_INVALID_PARAMETER, "The 'channels_config' array must contain only enum values from 'MultiplayerPeer.TransferMode'");
		int mode = p_channels_config[i].operator int();
		// Initialize data channel configurations.
		Dictionary cfg;
		cfg["id"] = CH_RESERVED_MAX + i + 1;
		cfg["negotiated"] = true;
		cfg["ordered"] = true;

		switch (mode) {
			case TRANSFER_MODE_UNRELIABLE_ORDERED:
				cfg["maxPacketLifetime"] = 1;
				break;
			case TRANSFER_MODE_UNRELIABLE:
				cfg["maxPacketLifetime"] = 1;
				cfg["ordered"] = false;
				break;
			case TRANSFER_MODE_RELIABLE:
				break;
			default:
				ERR_FAIL_V_MSG(ERR_INVALID_PARAMETER, vformat("The 'channels_config' array must contain only enum values from 'MultiplayerPeer.TransferMode'. Got: %d", mode));
		}
		channels_config.push_back(cfg);
		channels_modes.push_back((TransferMode)mode);
	}

	unique_id = p_self_id;
	network_mode = p_mode;

	// Mesh and server are always connected
	if (p_mode != MODE_CLIENT) {
		connection_status = CONNECTION_CONNECTED;
	} else {
		connection_status = CONNECTION_CONNECTING;
	}
	return OK;
}

bool WebRTCMultiplayerPeer::is_server_relay_supported() const {
	return network_mode == MODE_SERVER || network_mode == MODE_CLIENT;
}

int WebRTCMultiplayerPeer::get_unique_id() const {
	ERR_FAIL_COND_V(connection_status == CONNECTION_DISCONNECTED, 1);
	return unique_id;
}

void WebRTCMultiplayerPeer::_peer_to_dict(Ref<ConnectedPeer> p_connected_peer, Dictionary &r_dict) {
	Array channels;
	for (Ref<WebRTCDataChannel> &F : p_connected_peer->channels) {
		channels.push_back(F);
	}
	r_dict["connection"] = p_connected_peer->connection;
	r_dict["connected"] = p_connected_peer->connected;
	r_dict["channels"] = channels;
}

bool WebRTCMultiplayerPeer::has_peer(int p_peer_id) {
	return peer_map.has(p_peer_id);
}

Dictionary WebRTCMultiplayerPeer::get_peer(int p_peer_id) {
	ERR_FAIL_COND_V(!peer_map.has(p_peer_id), Dictionary());
	Dictionary out;
	_peer_to_dict(peer_map[p_peer_id], out);
	return out;
}

Dictionary WebRTCMultiplayerPeer::get_peers() {
	Dictionary out;
	for (const KeyValue<int, Ref<ConnectedPeer>> &E : peer_map) {
		Dictionary d;
		_peer_to_dict(E.value, d);
		out[E.key] = d;
	}
	return out;
}

Error WebRTCMultiplayerPeer::add_peer(Ref<WebRTCPeerConnection> p_peer, int p_peer_id, int p_unreliable_lifetime) {
	ERR_FAIL_COND_V(network_mode == MODE_NONE, ERR_UNCONFIGURED);
	ERR_FAIL_COND_V(network_mode == MODE_CLIENT && p_peer_id != 1, ERR_INVALID_PARAMETER);
	ERR_FAIL_COND_V(network_mode == MODE_SERVER && p_peer_id == 1, ERR_INVALID_PARAMETER);
	ERR_FAIL_COND_V(p_peer_id < 1 || p_peer_id > ~(1 << 31), ERR_INVALID_PARAMETER);
	ERR_FAIL_COND_V(p_unreliable_lifetime < 0, ERR_INVALID_PARAMETER);
	ERR_FAIL_COND_V(is_refusing_new_connections(), ERR_UNAUTHORIZED);
	// Peer must be valid, and in new state (to create data channels)
	ERR_FAIL_COND_V(p_peer.is_null(), ERR_INVALID_PARAMETER);
	ERR_FAIL_COND_V(p_peer->get_connection_state() != WebRTCPeerConnection::STATE_NEW, ERR_INVALID_PARAMETER);

	Ref<ConnectedPeer> peer = memnew(ConnectedPeer);
	peer->connection = p_peer;

	// Initialize data channels
	Dictionary cfg;
	cfg["negotiated"] = true;
	cfg["ordered"] = true;

	cfg["id"] = 1;
	peer->channels.get(CH_RELIABLE) = p_peer->create_data_channel("reliable", cfg);
	ERR_FAIL_COND_V(peer->channels.get(CH_RELIABLE).is_null(), FAILED);

	cfg["id"] = 2;
	cfg["maxPacketLifetime"] = p_unreliable_lifetime;
	peer->channels.get(CH_ORDERED) = p_peer->create_data_channel("ordered", cfg);
	ERR_FAIL_COND_V(peer->channels.get(CH_ORDERED).is_null(), FAILED);

	cfg["id"] = 3;
	cfg["ordered"] = false;
	peer->channels.get(CH_UNRELIABLE) = p_peer->create_data_channel("unreliable", cfg);
	ERR_FAIL_COND_V(peer->channels.get(CH_UNRELIABLE).is_null(), FAILED);

	for (const Dictionary &dict : channels_config) {
		Ref<WebRTCDataChannel> ch = p_peer->create_data_channel(String::num_int64(dict["id"]), dict);
		ERR_FAIL_COND_V(ch.is_null(), FAILED);
		peer->channels.push_back(ch);
	}

	peer_map[p_peer_id] = peer; // add the new peer connection to the peer_map

	return OK;
}

void WebRTCMultiplayerPeer::remove_peer(int p_peer_id) {
	ERR_FAIL_COND(!peer_map.has(p_peer_id));
	Ref<ConnectedPeer> peer = peer_map[p_peer_id];
	peer_map.erase(p_peer_id);
	if (peer->connected) {
		peer->connected = false;
		emit_signal(SNAME("peer_disconnected"), p_peer_id);
		if (network_mode == MODE_CLIENT && p_peer_id == TARGET_PEER_SERVER) {
			connection_status = CONNECTION_DISCONNECTED;
		}
	}
}

void WebRTCMultiplayerPeer::disconnect_peer(int p_peer_id, bool p_force) {
	ERR_FAIL_COND(!peer_map.has(p_peer_id));
	if (p_force) {
		peer_map.erase(p_peer_id);
		if (network_mode == MODE_CLIENT && p_peer_id == TARGET_PEER_SERVER) {
			connection_status = CONNECTION_DISCONNECTED;
		}
	} else {
		peer_map[p_peer_id]->connection->close(); // Will be removed during next poll.
	}
}

Error WebRTCMultiplayerPeer::get_packet(const uint8_t **r_buffer, int &r_buffer_size) {
	// Peer not available
	if (next_packet_peer == 0 || !peer_map.has(next_packet_peer)) {
		_find_next_peer();
		ERR_FAIL_V(ERR_UNAVAILABLE);
	}
	for (Ref<WebRTCDataChannel> &E : peer_map[next_packet_peer]->channels) {
		if (E->get_available_packet_count()) {
			Error err = E->get_packet(r_buffer, r_buffer_size);
			_find_next_peer();
			return err;
		}
	}
	// Channels for that peer were empty. Bug?
	_find_next_peer();
	ERR_FAIL_V(ERR_BUG);
}

Error WebRTCMultiplayerPeer::put_packet(const uint8_t *p_buffer, int p_buffer_size) {
	ERR_FAIL_COND_V(connection_status == CONNECTION_DISCONNECTED, ERR_UNCONFIGURED);

	int ch = get_transfer_channel();
	if (ch == 0) {
		switch (get_transfer_mode()) {
			case TRANSFER_MODE_RELIABLE:
				ch = CH_RELIABLE;
				break;
			case TRANSFER_MODE_UNRELIABLE_ORDERED:
				ch = CH_ORDERED;
				break;
			case TRANSFER_MODE_UNRELIABLE:
				ch = CH_UNRELIABLE;
				break;
		}
	} else {
		ch += CH_RESERVED_MAX - 1;
	}

	if (target_peer > 0) {
		HashMap<int, Ref<ConnectedPeer>>::Iterator E = peer_map.find(target_peer);
		ERR_FAIL_COND_V_MSG(!E, ERR_INVALID_PARAMETER, "Invalid target peer: " + itos(target_peer) + ".");

		ERR_FAIL_COND_V_MSG(E->value->channels.size() <= ch, ERR_INVALID_PARAMETER, vformat("Unable to send packet on channel %d, max channels: %d", ch, E->value->channels.size()));
		ERR_FAIL_COND_V(E->value->channels.get(ch).is_null(), ERR_BUG);
		return E->value->channels.get(ch)->put_packet(p_buffer, p_buffer_size);

	} else {
		int exclude = -target_peer;

		for (KeyValue<int, Ref<ConnectedPeer>> &F : peer_map) {
			// Exclude packet. If target_peer == 0 then don't exclude any packets
			if (target_peer != 0 && F.key == exclude) {
				continue;
			}

			ERR_CONTINUE_MSG(F.value->channels.size() <= ch, vformat("Unable to send packet on channel %d, max channels: %d", ch, F.value->channels.size()));
			ERR_CONTINUE(F.value->channels.get(ch).is_null());
			F.value->channels.get(ch)->put_packet(p_buffer, p_buffer_size);
		}
	}
	return OK;
}

int WebRTCMultiplayerPeer::get_available_packet_count() const {
	if (next_packet_peer == 0) {
		return 0; // To be sure next call to get_packet works if size > 0 .
	}
	int size = 0;
	for (const KeyValue<int, Ref<ConnectedPeer>> &E : peer_map) {
		if (!E.value->connected) {
			continue;
		}
		for (const Ref<WebRTCDataChannel> &F : E.value->channels) {
			size += F->get_available_packet_count();
		}
	}
	return size;
}

int WebRTCMultiplayerPeer::get_max_packet_size() const {
	return 1200;
}

void WebRTCMultiplayerPeer::close() {
	peer_map.clear();
	channels_config.clear();
	unique_id = 0;
	next_packet_peer = 0;
	next_packet_channel = 0;
	target_peer = 0;
	network_mode = MODE_NONE;
	connection_status = CONNECTION_DISCONNECTED;
}

WebRTCMultiplayerPeer::~WebRTCMultiplayerPeer() {
	close();
}

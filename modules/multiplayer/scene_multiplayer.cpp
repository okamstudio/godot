/**************************************************************************/
/*  scene_multiplayer.cpp                                                 */
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

#include "scene_multiplayer.h"

#include "core/debugger/engine_debugger.h"
#include "core/io/marshalls.h"

#ifdef DEBUG_ENABLED
#include "core/os/os.h"
#endif

#include <stdint.h>

#ifdef DEBUG_ENABLED
_FORCE_INLINE_ void SceneMultiplayer::_profile_bandwidth(const String &p_what, int p_value) {
	if (EngineDebugger::is_profiling("multiplayer:bandwidth")) {
		Array values;
		values.push_back(p_what);
		values.push_back(OS::get_singleton()->get_ticks_msec());
		values.push_back(p_value);
		EngineDebugger::profiler_add_frame_data("multiplayer:bandwidth", values);
	}
}
#endif

void SceneMultiplayer::_update_status() {
	MultiplayerPeer::ConnectionStatus status = multiplayer_peer.is_valid() ? multiplayer_peer->get_connection_status() : MultiplayerPeer::CONNECTION_DISCONNECTED;
	if (last_connection_status != status) {
		if (status == MultiplayerPeer::CONNECTION_DISCONNECTED) {
			if (last_connection_status == MultiplayerPeer::CONNECTION_CONNECTING) {
				emit_signal(SNAME("connection_failed"));
			} else {
				emit_signal(SNAME("server_disconnected"));
			}
			clear();
		}
		last_connection_status = status;
	}
}

Error SceneMultiplayer::poll() {
	_update_status();
	if (last_connection_status == MultiplayerPeer::CONNECTION_DISCONNECTED) {
		return OK;
	}

	multiplayer_peer->poll();

	_update_status();
	if (last_connection_status != MultiplayerPeer::CONNECTION_CONNECTED) {
		// We might be still connecting, or polling might have resulted in a disconnection.
		return OK;
	}

	while (multiplayer_peer->get_available_packet_count()) {
		int sender = multiplayer_peer->get_packet_peer();
		const uint8_t *packet;
		int len;

		int channel = multiplayer_peer->get_packet_channel();
		MultiplayerPeer::TransferMode mode = multiplayer_peer->get_packet_mode();

		Error err = multiplayer_peer->get_packet(&packet, len);
		ERR_FAIL_COND_V_MSG(err != OK, err, vformat("Error getting packet! %d", err));

#ifdef DEBUG_ENABLED
		_profile_bandwidth("in", len);
#endif

		if (pending_peers.has(sender)) {
			ERR_CONTINUE(len < 2 || (packet[0] & CMD_MASK) != NETWORK_COMMAND_SYS || packet[1] != SYS_COMMAND_AUTH);
			// Auth message.
			PackedByteArray pba;
			pba.resize(len - 2);
			if (pba.size()) {
				memcpy(pba.ptrw(), &packet[2], len - 2);
				// User callback
				const Variant sv = sender;
				const Variant pbav = pba;
				const Variant *argv[2] = { &sv, &pbav };
				Variant ret;
				Callable::CallError ce;
				auth_callback.callp(argv, 2, ret, ce);
				ERR_CONTINUE_MSG(ce.error != Callable::CallError::CALL_OK, "Failed to call authentication callback");
			} else {
				// Remote complete notification.
				pending_peers[sender].remote = true;
				if (pending_peers[sender].local) {
					pending_peers.erase(sender);
					_admit_peer(sender);
				}
			}
			continue; // Auth in progress.
		}

		ERR_CONTINUE(!connected_peers.has(sender));

		if (len && (packet[0] & CMD_MASK) == NETWORK_COMMAND_SYS) {
			// Sys messages are processed separately since they might call _process_packet themselves.
			if (len > 1 && packet[1] == SYS_COMMAND_AUTH) {
				ERR_CONTINUE(len != 2);
				// If we are here, we already admitted the peer locally, and this is just a confirmation packet.
				continue;
			}

			_process_sys(sender, packet, len, mode, channel);
		} else {
			remote_sender_id = sender;
			_process_packet(sender, packet, len);
			remote_sender_id = 0;
		}

		_update_status();
		if (last_connection_status != MultiplayerPeer::CONNECTION_CONNECTED) { // It's possible that processing a packet might have resulted in a disconnection, so check here.
			return OK;
		}
	}
	if (pending_peers.size() && auth_timeout) {
		HashSet<int> to_drop;
		uint64_t time = OS::get_singleton()->get_ticks_msec();
		for (const KeyValue<int, PendingPeer> &pending : pending_peers) {
			if (pending.value.time + auth_timeout <= time) {
				multiplayer_peer->disconnect_peer(pending.key);
				to_drop.insert(pending.key);
			}
		}
		for (const int &P : to_drop) {
			// Each signal might trigger a disconnection.
			pending_peers.erase(P);
			emit_signal(SNAME("peer_authentication_failed"), P);
		}
	}

	_update_status();
	if (last_connection_status != MultiplayerPeer::CONNECTION_CONNECTED) { // Signals might have triggered disconnection.
		return OK;
	}

	replicator->on_network_process();
	return OK;
}

void SceneMultiplayer::clear() {
	last_connection_status = MultiplayerPeer::CONNECTION_DISCONNECTED;
	pending_peers.clear();
	connected_peers.clear();
	packet_cache.clear();
	replicator->on_reset();
	cache->clear();
	relay_buffer->clear();
}

void SceneMultiplayer::set_root_path(const NodePath &p_path) {
	ERR_FAIL_COND_MSG(!p_path.is_absolute() && !p_path.is_empty(), "SceneMultiplayer root path must be absolute.");
	root_path = p_path;
}

NodePath SceneMultiplayer::get_root_path() const {
	return root_path;
}

void SceneMultiplayer::set_multiplayer_peer(const Ref<MultiplayerPeer> &p_peer) {
	if (p_peer == multiplayer_peer) {
		return; // Nothing to do
	}

	ERR_FAIL_COND_MSG(p_peer.is_valid() && p_peer->get_connection_status() == MultiplayerPeer::CONNECTION_DISCONNECTED,
			"Supplied MultiplayerPeer must be connecting or connected.");

	if (multiplayer_peer.is_valid()) {
		multiplayer_peer->disconnect("peer_connected", callable_mp(this, &SceneMultiplayer::_add_peer));
		multiplayer_peer->disconnect("peer_disconnected", callable_mp(this, &SceneMultiplayer::_del_peer));
		clear();
	}

	multiplayer_peer = p_peer;

	if (multiplayer_peer.is_valid()) {
		multiplayer_peer->connect("peer_connected", callable_mp(this, &SceneMultiplayer::_add_peer));
		multiplayer_peer->connect("peer_disconnected", callable_mp(this, &SceneMultiplayer::_del_peer));
	}
	_update_status();
}

Ref<MultiplayerPeer> SceneMultiplayer::get_multiplayer_peer() {
	return multiplayer_peer;
}

void SceneMultiplayer::_process_packet(int p_from, const uint8_t *p_packet, int p_packet_len) {
	ERR_FAIL_COND_MSG(root_path.is_empty(), "Multiplayer root was not initialized. If you are using custom multiplayer, remember to set the root path via SceneMultiplayer.set_root_path before using it.");
	ERR_FAIL_COND_MSG(p_packet_len < 1, "Invalid packet received. Size too small.");

	// Extract the `packet_type` from the LSB three bits:
	uint8_t packet_type = p_packet[0] & CMD_MASK;

	switch (packet_type) {
		case NETWORK_COMMAND_SIMPLIFY_PATH: {
			cache->process_simplify_path(p_from, p_packet, p_packet_len);
		} break;

		case NETWORK_COMMAND_CONFIRM_PATH: {
			cache->process_confirm_path(p_from, p_packet, p_packet_len);
		} break;

		case NETWORK_COMMAND_REMOTE_CALL: {
			rpc->process_rpc(p_from, p_packet, p_packet_len);
		} break;

		case NETWORK_COMMAND_RAW: {
			_process_raw(p_from, p_packet, p_packet_len);
		} break;
		case NETWORK_COMMAND_SPAWN: {
			replicator->on_spawn_receive(p_from, p_packet, p_packet_len);
		} break;
		case NETWORK_COMMAND_DESPAWN: {
			replicator->on_despawn_receive(p_from, p_packet, p_packet_len);
		} break;
		case NETWORK_COMMAND_SYNC: {
			replicator->on_sync_receive(p_from, p_packet, p_packet_len);
		} break;
		default: {
			ERR_FAIL_MSG("Invalid network command from " + itos(p_from));
		} break;
	}
}

#ifdef DEBUG_ENABLED
_FORCE_INLINE_ Error SceneMultiplayer::_send(const uint8_t *p_packet, int p_packet_len) {
	_profile_bandwidth("out", p_packet_len);
	return multiplayer_peer->put_packet(p_packet, p_packet_len);
}
#endif

Error SceneMultiplayer::send_command(int p_to, const uint8_t *p_packet, int p_packet_len) {
	if (server_relay && get_unique_id() != 1 && p_to != 1 && multiplayer_peer->is_server_relay_supported()) {
		// Send relay packet.
		relay_buffer->seek(0);
		relay_buffer->put_u8(NETWORK_COMMAND_SYS);
		relay_buffer->put_u8(SYS_COMMAND_RELAY);
		relay_buffer->put_32(p_to); // Set the destination.
		relay_buffer->put_data(p_packet, p_packet_len);
		multiplayer_peer->set_target_peer(1);
		const Vector<uint8_t> data = relay_buffer->get_data_array();
		return _send(data.ptr(), relay_buffer->get_position());
	}
	if (p_to > 0) {
		ERR_FAIL_COND_V(!connected_peers.has(p_to), ERR_BUG);
		multiplayer_peer->set_target_peer(p_to);
		return _send(p_packet, p_packet_len);
	} else {
		for (const int &pid : connected_peers) {
			if (p_to && pid == -p_to) {
				continue;
			}
			multiplayer_peer->set_target_peer(pid);
			_send(p_packet, p_packet_len);
		}
		return OK;
	}
}

void SceneMultiplayer::_process_sys(int p_from, const uint8_t *p_packet, int p_packet_len, MultiplayerPeer::TransferMode p_mode, int p_channel) {
	ERR_FAIL_COND_MSG(p_packet_len < SYS_CMD_SIZE, "Invalid packet received. Size too small.");
	uint8_t sys_cmd_type = p_packet[1];
	int32_t peer = int32_t(decode_uint32(&p_packet[2]));
	switch (sys_cmd_type) {
		case SYS_COMMAND_ADD_PEER: {
			ERR_FAIL_COND(!server_relay || !multiplayer_peer->is_server_relay_supported() || get_unique_id() == 1 || p_from != 1);
			_admit_peer(peer); // Relayed peers are automatically accepted.
		} break;
		case SYS_COMMAND_DEL_PEER: {
			ERR_FAIL_COND(!server_relay || !multiplayer_peer->is_server_relay_supported() || get_unique_id() == 1 || p_from != 1);
			_del_peer(peer);
		} break;
		case SYS_COMMAND_RELAY: {
			ERR_FAIL_COND(!server_relay || !multiplayer_peer->is_server_relay_supported());
			ERR_FAIL_COND(p_packet_len < SYS_CMD_SIZE + 1);
			const uint8_t *packet = p_packet + SYS_CMD_SIZE;
			int len = p_packet_len - SYS_CMD_SIZE;
			bool should_process = false;
			if (get_unique_id() == 1) { // I am the server.
				// The requested target might have disconnected while the packet was in transit.
				if (unlikely(peer > 0 && !connected_peers.has(peer))) {
					return;
				}
				// Send relay packet.
				relay_buffer->seek(0);
				relay_buffer->put_u8(NETWORK_COMMAND_SYS);
				relay_buffer->put_u8(SYS_COMMAND_RELAY);
				relay_buffer->put_32(p_from); // Set the source.
				relay_buffer->put_data(packet, len);
				const Vector<uint8_t> data = relay_buffer->get_data_array();
				multiplayer_peer->set_transfer_mode(p_mode);
				multiplayer_peer->set_transfer_channel(p_channel);
				if (peer > 0) {
					// Single destination.
					multiplayer_peer->set_target_peer(peer);
					_send(data.ptr(), relay_buffer->get_position());
				} else {
					// Multiple destinations.
					for (const int &P : connected_peers) {
						// Not to sender, nor excluded.
						if (P == p_from || P == -peer) {
							continue;
						}
						multiplayer_peer->set_target_peer(P);
						_send(data.ptr(), relay_buffer->get_position());
					}
					if (peer != -1) {
						// The server is one of the targets, process the packet with sender as source.
						should_process = true;
						peer = p_from;
					}
				}
			} else {
				ERR_FAIL_COND(p_from != 1); // Bug.
				should_process = true;
			}
			if (should_process) {
				remote_sender_id = peer;
				_process_packet(peer, packet, len);
				remote_sender_id = 0;
			}
		} break;
		default: {
			ERR_FAIL();
		}
	}
}

void SceneMultiplayer::_add_peer(int p_id) {
	if (auth_callback.is_valid()) {
		pending_peers[p_id] = PendingPeer();
		pending_peers[p_id].time = OS::get_singleton()->get_ticks_msec();
		emit_signal(SNAME("peer_authenticating"), p_id);
		return;
	} else {
		_admit_peer(p_id);
	}
}

void SceneMultiplayer::_admit_peer(int p_id) {
	if (server_relay && get_unique_id() == 1 && multiplayer_peer->is_server_relay_supported()) {
		// Notify others of connection, and send connected peers to newly connected one.
		uint8_t buf[SYS_CMD_SIZE];
		buf[0] = NETWORK_COMMAND_SYS;
		buf[1] = SYS_COMMAND_ADD_PEER;
		multiplayer_peer->set_transfer_channel(0);
		multiplayer_peer->set_transfer_mode(MultiplayerPeer::TRANSFER_MODE_RELIABLE);
		for (const int &P : connected_peers) {
			// Send new peer to already connected.
			encode_uint32(p_id, &buf[2]);
			multiplayer_peer->set_target_peer(P);
			_send(buf, sizeof(buf));
			// Send already connected to new peer.
			encode_uint32(P, &buf[2]);
			multiplayer_peer->set_target_peer(p_id);
			_send(buf, sizeof(buf));
		}
	}

	connected_peers.insert(p_id);
	cache->on_peer_change(p_id, true);
	replicator->on_peer_change(p_id, true);
	if (p_id == 1) {
		emit_signal(SNAME("connected_to_server"));
	}
	emit_signal(SNAME("peer_connected"), p_id);
}

void SceneMultiplayer::_del_peer(int p_id) {
	if (pending_peers.has(p_id)) {
		pending_peers.erase(p_id);
		emit_signal(SNAME("peer_authentication_failed"), p_id);
		return;
	} else if (!connected_peers.has(p_id)) {
		return;
	}

	if (server_relay && get_unique_id() == 1 && multiplayer_peer->is_server_relay_supported()) {
		// Notify others of disconnection.
		uint8_t buf[SYS_CMD_SIZE];
		buf[0] = NETWORK_COMMAND_SYS;
		buf[1] = SYS_COMMAND_DEL_PEER;
		multiplayer_peer->set_transfer_channel(0);
		multiplayer_peer->set_transfer_mode(MultiplayerPeer::TRANSFER_MODE_RELIABLE);
		encode_uint32(p_id, &buf[2]);
		for (const int &P : connected_peers) {
			if (P == p_id) {
				continue;
			}
			multiplayer_peer->set_target_peer(P);
			_send(buf, sizeof(buf));
		}
	}

	replicator->on_peer_change(p_id, false);
	cache->on_peer_change(p_id, false);
	connected_peers.erase(p_id);
	emit_signal(SNAME("peer_disconnected"), p_id);
}

void SceneMultiplayer::disconnect_peer(int p_id) {
	ERR_FAIL_COND(multiplayer_peer.is_null() || multiplayer_peer->get_connection_status() != MultiplayerPeer::CONNECTION_CONNECTED);
	// Block signals to avoid emitting peer_disconnected.
	bool blocking = is_blocking_signals();
	set_block_signals(true);
	_del_peer(p_id);
	set_block_signals(blocking);
	multiplayer_peer->disconnect_peer(p_id);
}

Error SceneMultiplayer::send_bytes(Vector<uint8_t> p_data, int p_to, MultiplayerPeer::TransferMode p_mode, int p_channel) {
	ERR_FAIL_COND_V_MSG(p_data.is_empty(), ERR_INVALID_DATA, "Trying to send an empty raw packet.");
	ERR_FAIL_COND_V_MSG(multiplayer_peer.is_null(), ERR_UNCONFIGURED, "Trying to send a raw packet while no multiplayer peer is active.");
	ERR_FAIL_COND_V_MSG(multiplayer_peer->get_connection_status() != MultiplayerPeer::CONNECTION_CONNECTED, ERR_UNCONFIGURED, "Trying to send a raw packet via a multiplayer peer which is not connected.");

	if (packet_cache.size() < p_data.size() + 1) {
		packet_cache.resize(p_data.size() + 1);
	}

	const uint8_t *r = p_data.ptr();
	packet_cache.write[0] = NETWORK_COMMAND_RAW;
	memcpy(&packet_cache.write[1], &r[0], p_data.size());

	multiplayer_peer->set_transfer_channel(p_channel);
	multiplayer_peer->set_transfer_mode(p_mode);
	return send_command(p_to, packet_cache.ptr(), p_data.size() + 1);
}

Error SceneMultiplayer::send_auth(int p_to, Vector<uint8_t> p_data) {
	ERR_FAIL_COND_V(multiplayer_peer.is_null() || multiplayer_peer->get_connection_status() != MultiplayerPeer::CONNECTION_CONNECTED, ERR_UNCONFIGURED);
	ERR_FAIL_COND_V(!pending_peers.has(p_to), ERR_INVALID_PARAMETER);
	ERR_FAIL_COND_V(p_data.is_empty(), ERR_INVALID_PARAMETER);
	ERR_FAIL_COND_V_MSG(pending_peers[p_to].local, ERR_FILE_CANT_WRITE, "The authentication session was previously marked as completed, no more authentication data can be sent.");
	ERR_FAIL_COND_V_MSG(pending_peers[p_to].remote, ERR_FILE_CANT_WRITE, "The remote peer notified that the authentication session was completed, no more authentication data can be sent.");

	if (packet_cache.size() < p_data.size() + 2) {
		packet_cache.resize(p_data.size() + 2);
	}

	packet_cache.write[0] = NETWORK_COMMAND_SYS;
	packet_cache.write[1] = SYS_COMMAND_AUTH;
	memcpy(&packet_cache.write[2], p_data.ptr(), p_data.size());

	multiplayer_peer->set_target_peer(p_to);
	multiplayer_peer->set_transfer_channel(0);
	multiplayer_peer->set_transfer_mode(MultiplayerPeer::TRANSFER_MODE_RELIABLE);
	return _send(packet_cache.ptr(), p_data.size() + 2);
}

Error SceneMultiplayer::complete_auth(int p_peer) {
	ERR_FAIL_COND_V(multiplayer_peer.is_null() || multiplayer_peer->get_connection_status() != MultiplayerPeer::CONNECTION_CONNECTED, ERR_UNCONFIGURED);
	ERR_FAIL_COND_V(!pending_peers.has(p_peer), ERR_INVALID_PARAMETER);
	ERR_FAIL_COND_V_MSG(pending_peers[p_peer].local, ERR_FILE_CANT_WRITE, "The authentication session was already marked as completed.");
	pending_peers[p_peer].local = true;

	// Notify the remote peer that the authentication has completed.
	uint8_t buf[2] = { NETWORK_COMMAND_SYS, SYS_COMMAND_AUTH };
	multiplayer_peer->set_target_peer(p_peer);
	multiplayer_peer->set_transfer_channel(0);
	multiplayer_peer->set_transfer_mode(MultiplayerPeer::TRANSFER_MODE_RELIABLE);
	Error err = _send(buf, 2);

	// The remote peer already reported the authentication as completed, so admit the peer.
	// May generate new packets, so it must happen after sending confirmation.
	if (pending_peers[p_peer].remote) {
		pending_peers.erase(p_peer);
		_admit_peer(p_peer);
	}
	return err;
}

void SceneMultiplayer::set_auth_callback(Callable p_callback) {
	auth_callback = p_callback;
}

Callable SceneMultiplayer::get_auth_callback() const {
	return auth_callback;
}

void SceneMultiplayer::set_auth_timeout(double p_timeout) {
	ERR_FAIL_COND_MSG(p_timeout < 0, "Timeout must be greater or equal to 0 (where 0 means no timeout)");
	auth_timeout = uint64_t(p_timeout * 1000);
}

double SceneMultiplayer::get_auth_timeout() const {
	return double(auth_timeout) / 1000.0;
}

void SceneMultiplayer::_process_raw(int p_from, const uint8_t *p_packet, int p_packet_len) {
	ERR_FAIL_COND_MSG(p_packet_len < 2, "Invalid packet received. Size too small.");

	Vector<uint8_t> out;
	int len = p_packet_len - 1;
	out.resize(len);
	{
		uint8_t *w = out.ptrw();
		memcpy(&w[0], &p_packet[1], len);
	}
	emit_signal(SNAME("peer_packet"), p_from, out);
}

int SceneMultiplayer::get_unique_id() {
	ERR_FAIL_COND_V_MSG(multiplayer_peer.is_null(), 0, "No multiplayer peer is assigned. Unable to get unique ID.");
	return multiplayer_peer->get_unique_id();
}

void SceneMultiplayer::set_refuse_new_connections(bool p_refuse) {
	ERR_FAIL_COND_MSG(multiplayer_peer.is_null(), "No multiplayer peer is assigned. Unable to set 'refuse_new_connections'.");
	multiplayer_peer->set_refuse_new_connections(p_refuse);
}

bool SceneMultiplayer::is_refusing_new_connections() const {
	ERR_FAIL_COND_V_MSG(multiplayer_peer.is_null(), false, "No multiplayer peer is assigned. Unable to get 'refuse_new_connections'.");
	return multiplayer_peer->is_refusing_new_connections();
}

Vector<int> SceneMultiplayer::get_peer_ids() {
	ERR_FAIL_COND_V_MSG(multiplayer_peer.is_null(), Vector<int>(), "No multiplayer peer is assigned. Assume no peers are connected.");

	Vector<int> ret;
	for (const int &E : connected_peers) {
		ret.push_back(E);
	}

	return ret;
}

Vector<int> SceneMultiplayer::get_authenticating_peer_ids() {
	Vector<int> out;
	out.resize(pending_peers.size());
	int idx = 0;
	for (const KeyValue<int, PendingPeer> &E : pending_peers) {
		out.write[idx++] = E.key;
	}
	return out;
}

void SceneMultiplayer::set_allow_object_decoding(bool p_enable) {
	allow_object_decoding = p_enable;
}

bool SceneMultiplayer::is_object_decoding_allowed() const {
	return allow_object_decoding;
}

String SceneMultiplayer::get_rpc_md5(const Object *p_obj) {
	return rpc->get_rpc_md5(p_obj);
}

Error SceneMultiplayer::rpcp(Object *p_obj, int p_peer_id, const StringName &p_method, const Variant **p_arg, int p_argcount) {
	return rpc->rpcp(p_obj, p_peer_id, p_method, p_arg, p_argcount);
}

Error SceneMultiplayer::object_configuration_add(Object *p_obj, Variant p_config) {
	if (p_obj == nullptr && p_config.get_type() == Variant::NODE_PATH) {
		set_root_path(p_config);
		return OK;
	}
	MultiplayerSpawner *spawner = Object::cast_to<MultiplayerSpawner>(p_config.get_validated_object());
	MultiplayerSynchronizer *sync = Object::cast_to<MultiplayerSynchronizer>(p_config.get_validated_object());
	if (spawner) {
		return replicator->on_spawn(p_obj, p_config);
	} else if (sync) {
		return replicator->on_replication_start(p_obj, p_config);
	}
	return ERR_INVALID_PARAMETER;
}

Error SceneMultiplayer::object_configuration_remove(Object *p_obj, Variant p_config) {
	if (p_obj == nullptr && p_config.get_type() == Variant::NODE_PATH) {
		ERR_FAIL_COND_V(root_path != p_config.operator NodePath(), ERR_INVALID_PARAMETER);
		set_root_path(NodePath());
		return OK;
	}
	MultiplayerSpawner *spawner = Object::cast_to<MultiplayerSpawner>(p_config.get_validated_object());
	MultiplayerSynchronizer *sync = Object::cast_to<MultiplayerSynchronizer>(p_config.get_validated_object());
	if (spawner) {
		return replicator->on_despawn(p_obj, p_config);
	}
	if (sync) {
		return replicator->on_replication_stop(p_obj, p_config);
	}
	return ERR_INVALID_PARAMETER;
}

void SceneMultiplayer::set_server_relay_enabled(bool p_enabled) {
	server_relay = p_enabled;
}

bool SceneMultiplayer::is_server_relay_enabled() const {
	return server_relay;
}

void SceneMultiplayer::set_max_sync_packet_size(int p_size) {
	replicator->set_max_sync_packet_size(p_size);
}

int SceneMultiplayer::get_max_sync_packet_size() const {
	return replicator->get_max_sync_packet_size();
}

void SceneMultiplayer::set_max_delta_packet_size(int p_size) {
	replicator->set_max_delta_packet_size(p_size);
}

int SceneMultiplayer::get_max_delta_packet_size() const {
	return replicator->get_max_delta_packet_size();
}

void SceneMultiplayer::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_root_path", "path"), &SceneMultiplayer::set_root_path);
	ClassDB::bind_method(D_METHOD("get_root_path"), &SceneMultiplayer::get_root_path);
	ClassDB::bind_method(D_METHOD("clear"), &SceneMultiplayer::clear);

	ClassDB::bind_method(D_METHOD("disconnect_peer", "id"), &SceneMultiplayer::disconnect_peer);

	ClassDB::bind_method(D_METHOD("get_authenticating_peers"), &SceneMultiplayer::get_authenticating_peer_ids);
	ClassDB::bind_method(D_METHOD("send_auth", "id", "data"), &SceneMultiplayer::send_auth);
	ClassDB::bind_method(D_METHOD("complete_auth", "id"), &SceneMultiplayer::complete_auth);

	ClassDB::bind_method(D_METHOD("set_auth_callback", "callback"), &SceneMultiplayer::set_auth_callback);
	ClassDB::bind_method(D_METHOD("get_auth_callback"), &SceneMultiplayer::get_auth_callback);
	ClassDB::bind_method(D_METHOD("set_auth_timeout", "timeout"), &SceneMultiplayer::set_auth_timeout);
	ClassDB::bind_method(D_METHOD("get_auth_timeout"), &SceneMultiplayer::get_auth_timeout);

	ClassDB::bind_method(D_METHOD("set_refuse_new_connections", "refuse"), &SceneMultiplayer::set_refuse_new_connections);
	ClassDB::bind_method(D_METHOD("is_refusing_new_connections"), &SceneMultiplayer::is_refusing_new_connections);
	ClassDB::bind_method(D_METHOD("set_allow_object_decoding", "enable"), &SceneMultiplayer::set_allow_object_decoding);
	ClassDB::bind_method(D_METHOD("is_object_decoding_allowed"), &SceneMultiplayer::is_object_decoding_allowed);
	ClassDB::bind_method(D_METHOD("set_server_relay_enabled", "enabled"), &SceneMultiplayer::set_server_relay_enabled);
	ClassDB::bind_method(D_METHOD("is_server_relay_enabled"), &SceneMultiplayer::is_server_relay_enabled);
	ClassDB::bind_method(D_METHOD("send_bytes", "bytes", "id", "mode", "channel"), &SceneMultiplayer::send_bytes, DEFVAL(MultiplayerPeer::TARGET_PEER_BROADCAST), DEFVAL(MultiplayerPeer::TRANSFER_MODE_RELIABLE), DEFVAL(0));

	ClassDB::bind_method(D_METHOD("get_max_sync_packet_size"), &SceneMultiplayer::get_max_sync_packet_size);
	ClassDB::bind_method(D_METHOD("set_max_sync_packet_size", "size"), &SceneMultiplayer::set_max_sync_packet_size);
	ClassDB::bind_method(D_METHOD("get_max_delta_packet_size"), &SceneMultiplayer::get_max_delta_packet_size);
	ClassDB::bind_method(D_METHOD("set_max_delta_packet_size", "size"), &SceneMultiplayer::set_max_delta_packet_size);

	ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "root_path"), "set_root_path", "get_root_path");
	ADD_PROPERTY(PropertyInfo(Variant::CALLABLE, "auth_callback"), "set_auth_callback", "get_auth_callback");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "auth_timeout", PROPERTY_HINT_RANGE, "0,30,0.1,or_greater,suffix:s"), "set_auth_timeout", "get_auth_timeout");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "allow_object_decoding"), "set_allow_object_decoding", "is_object_decoding_allowed");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "refuse_new_connections"), "set_refuse_new_connections", "is_refusing_new_connections");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "server_relay"), "set_server_relay_enabled", "is_server_relay_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "max_sync_packet_size"), "set_max_sync_packet_size", "get_max_sync_packet_size");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "max_delta_packet_size"), "set_max_delta_packet_size", "get_max_delta_packet_size");

	ADD_PROPERTY_DEFAULT("refuse_new_connections", false);

	ADD_SIGNAL(MethodInfo("peer_authenticating", PropertyInfo(Variant::INT, "id")));
	ADD_SIGNAL(MethodInfo("peer_authentication_failed", PropertyInfo(Variant::INT, "id")));
	ADD_SIGNAL(MethodInfo("peer_packet", PropertyInfo(Variant::INT, "id"), PropertyInfo(Variant::PACKED_BYTE_ARRAY, "packet")));
}

SceneMultiplayer::SceneMultiplayer() {
	relay_buffer.instantiate();
	cache.instantiate(this);
	replicator.instantiate(this, cache.ptr());
	rpc.instantiate(this, cache.ptr(), replicator.ptr());
	set_multiplayer_peer(Ref<OfflineMultiplayerPeer>(memnew(OfflineMultiplayerPeer)));
}

SceneMultiplayer::~SceneMultiplayer() {
	clear();
	// Ensure unref in reverse order for safety (we shouldn't use those pointers in the deconstructors anyway).
	rpc.unref();
	replicator.unref();
	cache.unref();
}

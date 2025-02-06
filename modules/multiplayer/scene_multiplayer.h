/**************************************************************************/
/*  scene_multiplayer.h                                                   */
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

#include "scene_cache_interface.h"
#include "scene_replication_interface.h"
#include "scene_rpc_interface.h"

#include "scene/main/multiplayer_api.h"

class OfflineMultiplayerPeer : public MultiplayerPeer {
	GDCLASS(OfflineMultiplayerPeer, MultiplayerPeer);

public:
	virtual int get_available_packet_count() const override { return 0; }
	virtual Error get_packet(const uint8_t **r_buffer, int &r_buffer_size) override {
		*r_buffer = nullptr;
		r_buffer_size = 0;
		return OK;
	}
	virtual Error put_packet(const uint8_t *p_buffer, int p_buffer_size) override { return OK; }
	virtual int get_max_packet_size() const override { return 0; }

	virtual void set_target_peer(int p_peer_id) override {}
	virtual int get_packet_peer() const override { return 0; }
	virtual TransferMode get_packet_mode() const override { return TRANSFER_MODE_RELIABLE; }
	virtual int get_packet_channel() const override { return 0; }
	virtual void disconnect_peer(int p_peer, bool p_force = false) override {}
	virtual bool is_server() const override { return true; }
	virtual void poll() override {}
	virtual void close() override {}
	virtual int get_unique_id() const override { return TARGET_PEER_SERVER; }
	virtual ConnectionStatus get_connection_status() const override { return CONNECTION_CONNECTED; }
};

class SceneMultiplayer : public MultiplayerAPI {
	GDCLASS(SceneMultiplayer, MultiplayerAPI);

public:
	enum NetworkCommands {
		NETWORK_COMMAND_REMOTE_CALL = 0,
		NETWORK_COMMAND_SIMPLIFY_PATH,
		NETWORK_COMMAND_CONFIRM_PATH,
		NETWORK_COMMAND_RAW,
		NETWORK_COMMAND_SPAWN,
		NETWORK_COMMAND_DESPAWN,
		NETWORK_COMMAND_SYNC,
		NETWORK_COMMAND_SYS,
	};

	enum SysCommands {
		SYS_COMMAND_AUTH,
		SYS_COMMAND_ADD_PEER,
		SYS_COMMAND_DEL_PEER,
		SYS_COMMAND_RELAY,
	};

	enum {
		SYS_CMD_SIZE = 6, // Command + sys command + peer_id (+ optional payload).
	};

	// For each command, the 4 MSB can contain custom flags, as defined by subsystems.
	enum {
		CMD_FLAG_0_SHIFT = 4,
		CMD_FLAG_1_SHIFT = 5,
		CMD_FLAG_2_SHIFT = 6,
		CMD_FLAG_3_SHIFT = 7,
	};

	// This is the mask that will be used to extract the command.
	enum {
		CMD_MASK = 7, // 0x7 -> 0b00000111
	};

private:
	struct PendingPeer {
		bool local = false;
		bool remote = false;
		uint64_t time = 0;
	};

	Ref<MultiplayerPeer> multiplayer_peer;
	MultiplayerPeer::ConnectionStatus last_connection_status = MultiplayerPeer::CONNECTION_DISCONNECTED;
	HashMap<int, PendingPeer> pending_peers; // true if locally finalized.
	Callable auth_callback;
	uint64_t auth_timeout = 3000;
	HashSet<int> connected_peers;
	int remote_sender_id = 0;
	int remote_sender_override = 0;

	Vector<uint8_t> packet_cache;

	NodePath root_path;
	bool allow_object_decoding = false;
	bool server_relay = true;
	Ref<StreamPeerBuffer> relay_buffer;

	Ref<SceneCacheInterface> cache;
	Ref<SceneReplicationInterface> replicator;
	Ref<SceneRPCInterface> rpc;

#ifdef DEBUG_ENABLED
	_FORCE_INLINE_ void _profile_bandwidth(const String &p_what, int p_value);
	_FORCE_INLINE_ Error _send(const uint8_t *p_packet, int p_packet_len); // Also profiles.
#else
	_FORCE_INLINE_ Error _send(const uint8_t *p_packet, int p_packet_len) {
		return multiplayer_peer->put_packet(p_packet, p_packet_len);
	}
#endif

protected:
	static void _bind_methods();

	void _process_packet(int p_from, const uint8_t *p_packet, int p_packet_len);
	void _process_raw(int p_from, const uint8_t *p_packet, int p_packet_len);
	void _process_sys(int p_from, const uint8_t *p_packet, int p_packet_len, MultiplayerPeer::TransferMode p_mode, int p_channel);

	void _add_peer(int p_id);
	void _admit_peer(int p_id);
	void _del_peer(int p_id);
	void _update_status();

public:
	virtual void set_multiplayer_peer(const Ref<MultiplayerPeer> &p_peer) override;
	virtual Ref<MultiplayerPeer> get_multiplayer_peer() override;

	virtual Error poll() override;
	virtual int get_unique_id() override;
	virtual Vector<int> get_peer_ids() override;
	virtual int get_remote_sender_id() override { return remote_sender_override ? remote_sender_override : remote_sender_id; }

	virtual Error rpcp(Object *p_obj, int p_peer_id, const StringName &p_method, const Variant **p_arg, int p_argcount) override;

	virtual Error object_configuration_add(Object *p_obj, Variant p_config) override;
	virtual Error object_configuration_remove(Object *p_obj, Variant p_config) override;

	void clear();

	// Usually from object_configuration_add/remove
	void set_root_path(const NodePath &p_path);
	NodePath get_root_path() const;

	void disconnect_peer(int p_id);

	Error send_auth(int p_to, Vector<uint8_t> p_bytes);
	Error complete_auth(int p_peer);
	void set_auth_callback(Callable p_callback);
	Callable get_auth_callback() const;
	void set_auth_timeout(double p_timeout);
	double get_auth_timeout() const;
	Vector<int> get_authenticating_peer_ids();

	Error send_command(int p_to, const uint8_t *p_packet, int p_packet_len); // Used internally to relay packets when needed.
	Error send_bytes(Vector<uint8_t> p_data, int p_to = MultiplayerPeer::TARGET_PEER_BROADCAST, MultiplayerPeer::TransferMode p_mode = MultiplayerPeer::TRANSFER_MODE_RELIABLE, int p_channel = 0);
	String get_rpc_md5(const Object *p_obj);

	const HashSet<int> get_connected_peers() const { return connected_peers; }

	void set_remote_sender_override(int p_id) { remote_sender_override = p_id; }
	void set_refuse_new_connections(bool p_refuse);
	bool is_refusing_new_connections() const;

	void set_allow_object_decoding(bool p_enable);
	bool is_object_decoding_allowed() const;

	void set_server_relay_enabled(bool p_enabled);
	bool is_server_relay_enabled() const;

	void set_max_sync_packet_size(int p_size);
	int get_max_sync_packet_size() const;

	void set_max_delta_packet_size(int p_size);
	int get_max_delta_packet_size() const;

	SceneMultiplayer();
	~SceneMultiplayer();
};

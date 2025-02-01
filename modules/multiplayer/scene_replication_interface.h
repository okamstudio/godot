/**************************************************************************/
/*  scene_replication_interface.h                                         */
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

#include "multiplayer_spawner.h"
#include "multiplayer_synchronizer.h"

#include "core/object/ref_counted.h"

class SceneMultiplayer;
class SceneCacheInterface;

class SceneReplicationInterface : public RefCounted {
	GDCLASS(SceneReplicationInterface, RefCounted);

private:
	struct TrackedNode {
		ObjectID id;
		uint32_t net_id = 0;
		uint32_t remote_peer = 0;
		ObjectID spawner;
		HashSet<ObjectID> synchronizers;

		bool operator==(const ObjectID &p_other) { return id == p_other; }

		TrackedNode() {}
		TrackedNode(const ObjectID &p_id) { id = p_id; }
		TrackedNode(const ObjectID &p_id, uint32_t p_net_id) {
			id = p_id;
			net_id = p_net_id;
		}
	};

	struct PeerInfo {
		HashSet<ObjectID> sync_nodes;
		HashSet<ObjectID> spawn_nodes;
		HashMap<ObjectID, uint64_t> last_watch_usecs;
		HashMap<uint32_t, ObjectID> recv_sync_ids;
		HashMap<uint32_t, ObjectID> recv_nodes;
		uint16_t last_sent_sync = 0;
	};

	// Replication state.
	HashMap<int, PeerInfo> peers_info;
	uint32_t last_net_id = 0;
	HashMap<ObjectID, TrackedNode> tracked_nodes;
	HashSet<ObjectID> spawned_nodes;
	HashSet<ObjectID> sync_nodes;

	// Pending local spawn information (handles spawning nested nodes during ready).
	HashSet<ObjectID> spawn_queue;

	// Pending remote spawn information.
	ObjectID pending_spawn;
	int pending_spawn_remote = 0;
	const uint8_t *pending_buffer = nullptr;
	int pending_buffer_size = 0;
	List<uint32_t> pending_sync_net_ids;

	// Replicator config.
	SceneMultiplayer *multiplayer = nullptr;
	SceneCacheInterface *multiplayer_cache = nullptr;
	PackedByteArray packet_cache;
	int sync_mtu = 1350; // Highly dependent on underlying protocol.
	int delta_mtu = 65535;

	TrackedNode &_track(const ObjectID &p_id);
	void _untrack(const ObjectID &p_id);
	void _node_ready(const ObjectID &p_oid);

	bool _has_authority(const Node *p_node);
	bool _verify_synchronizer(int p_peer, MultiplayerSynchronizer *p_sync, uint32_t &r_net_id);
	MultiplayerSynchronizer *_find_synchronizer(int p_peer, uint32_t p_net_ida);

	void _send_sync(int p_peer, const HashSet<ObjectID> &p_synchronizers, uint16_t p_sync_net_time, uint64_t p_usec);
	void _send_delta(int p_peer, const HashSet<ObjectID> &p_synchronizers, uint64_t p_usec, const HashMap<ObjectID, uint64_t> &p_last_watch_usecs);
	Error _make_spawn_packet(Node *p_node, MultiplayerSpawner *p_spawner, int &r_len);
	Error _make_despawn_packet(Node *p_node, int &r_len);
	Error _send_raw(const uint8_t *p_buffer, int p_size, int p_peer, bool p_reliable);

	void _visibility_changed(int p_peer, ObjectID p_oid);
	Error _update_sync_visibility(int p_peer, MultiplayerSynchronizer *p_sync);
	Error _update_spawn_visibility(int p_peer, const ObjectID &p_oid);
	void _free_remotes(const PeerInfo &p_info);

	template <typename T>
	static T *get_id_as(const ObjectID &p_id) {
		return p_id.is_valid() ? Object::cast_to<T>(ObjectDB::get_instance(p_id)) : nullptr;
	}

#ifdef DEBUG_ENABLED
	_FORCE_INLINE_ void _profile_node_data(const String &p_what, ObjectID p_id, int p_size);
#endif

public:
	static void make_default();

	void on_reset();
	void on_peer_change(int p_id, bool p_connected);

	Error on_spawn(Object *p_obj, Variant p_config);
	Error on_despawn(Object *p_obj, Variant p_config);
	Error on_replication_start(Object *p_obj, Variant p_config);
	Error on_replication_stop(Object *p_obj, Variant p_config);
	void on_network_process();

	Error on_spawn_receive(int p_from, const uint8_t *p_buffer, int p_buffer_len);
	Error on_despawn_receive(int p_from, const uint8_t *p_buffer, int p_buffer_len);
	Error on_sync_receive(int p_from, const uint8_t *p_buffer, int p_buffer_len);
	Error on_delta_receive(int p_from, const uint8_t *p_buffer, int p_buffer_len);

	bool is_rpc_visible(const ObjectID &p_oid, int p_peer) const;

	void set_max_sync_packet_size(int p_size);
	int get_max_sync_packet_size() const;

	void set_max_delta_packet_size(int p_size);
	int get_max_delta_packet_size() const;

	SceneReplicationInterface(SceneMultiplayer *p_multiplayer, SceneCacheInterface *p_cache) {
		multiplayer = p_multiplayer;
		multiplayer_cache = p_cache;
	}
};

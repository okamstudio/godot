/**************************************************************************/
/*  multiplayer_synchronizer.h                                            */
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

#include "scene_replication_config.h"

#include "scene/main/node.h"

class MultiplayerSynchronizer : public Node {
	GDCLASS(MultiplayerSynchronizer, Node);

public:
	enum VisibilityUpdateMode {
		VISIBILITY_PROCESS_IDLE,
		VISIBILITY_PROCESS_PHYSICS,
		VISIBILITY_PROCESS_NONE,
	};

private:
	struct Watcher {
		NodePath prop;
		uint64_t last_change_usec = 0;
		Variant value;
	};

	Ref<SceneReplicationConfig> replication_config;
	NodePath root_path = NodePath(".."); // Start with parent, like with AnimationPlayer.
	uint64_t sync_interval_usec = 0;
	uint64_t delta_interval_usec = 0;
	VisibilityUpdateMode visibility_update_mode = VISIBILITY_PROCESS_IDLE;
	HashSet<Callable> visibility_filters;
	HashSet<int> peer_visibility;
	Vector<Watcher> watchers;
	uint64_t last_watch_usec = 0;

	ObjectID root_node_cache;
	uint64_t last_sync_usec = 0;
	uint16_t last_inbound_sync = 0;
	uint32_t net_id = 0;
	bool sync_started = false;

	static Object *_get_prop_target(Object *p_obj, const NodePath &p_prop);
	void _start();
	void _stop();
	void _update_process();
	Error _watch_changes(uint64_t p_usec);

protected:
	static void _bind_methods();
	void _notification(int p_what);

public:
	static Error get_state(const List<NodePath> &p_properties, Object *p_obj, Vector<Variant> &r_variant, Vector<const Variant *> &r_variant_ptrs);
	static Error set_state(const List<NodePath> &p_properties, Object *p_obj, const Vector<Variant> &p_state);

	void reset();
	Node *get_root_node();

	uint32_t get_net_id() const;
	void set_net_id(uint32_t p_net_id);

	bool update_outbound_sync_time(uint64_t p_usec);
	bool update_inbound_sync_time(uint16_t p_network_time);

	PackedStringArray get_configuration_warnings() const override;

	void set_replication_interval(double p_interval);
	double get_replication_interval() const;

	void set_delta_interval(double p_interval);
	double get_delta_interval() const;

	void set_replication_config(Ref<SceneReplicationConfig> p_config);
	Ref<SceneReplicationConfig> get_replication_config();

	void set_root_path(const NodePath &p_path);
	NodePath get_root_path() const;
	virtual void set_multiplayer_authority(int p_peer_id, bool p_recursive = true) override;

	bool is_visibility_public() const;
	void set_visibility_public(bool p_public);
	bool is_visible_to(int p_peer);
	void set_visibility_for(int p_peer, bool p_visible);
	bool get_visibility_for(int p_peer) const;
	void update_visibility(int p_for_peer);
	void set_visibility_update_mode(VisibilityUpdateMode p_mode);
	void add_visibility_filter(Callable p_callback);
	void remove_visibility_filter(Callable p_callback);
	VisibilityUpdateMode get_visibility_update_mode() const;

	List<Variant> get_delta_state(uint64_t p_cur_usec, uint64_t p_last_usec, uint64_t &r_indexes);
	List<NodePath> get_delta_properties(uint64_t p_indexes);
	SceneReplicationConfig *get_replication_config_ptr() const;

	MultiplayerSynchronizer();
};

VARIANT_ENUM_CAST(MultiplayerSynchronizer::VisibilityUpdateMode);

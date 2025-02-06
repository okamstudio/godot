/**************************************************************************/
/*  multiplayer_synchronizer.cpp                                          */
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

#include "multiplayer_synchronizer.h"

#include "core/config/engine.h"
#include "scene/main/multiplayer_api.h"

Object *MultiplayerSynchronizer::_get_prop_target(Object *p_obj, const NodePath &p_path) {
	if (p_path.get_name_count() == 0) {
		return p_obj;
	}
	Node *node = Object::cast_to<Node>(p_obj);
	ERR_FAIL_COND_V_MSG(!node || !node->has_node(p_path), nullptr, vformat("Node '%s' not found.", p_path));
	return node->get_node(p_path);
}

void MultiplayerSynchronizer::_stop() {
#ifdef TOOLS_ENABLED
	if (Engine::get_singleton()->is_editor_hint()) {
		return;
	}
#endif
	root_node_cache = ObjectID();
	Node *node = is_inside_tree() ? get_node_or_null(root_path) : nullptr;
	if (node) {
		get_multiplayer()->object_configuration_remove(node, this);
	}
	reset();
}

void MultiplayerSynchronizer::_start() {
#ifdef TOOLS_ENABLED
	if (Engine::get_singleton()->is_editor_hint()) {
		return;
	}
#endif
	root_node_cache = ObjectID();
	reset();
	Node *node = is_inside_tree() ? get_node_or_null(root_path) : nullptr;
	if (node) {
		root_node_cache = node->get_instance_id();
		get_multiplayer()->object_configuration_add(node, this);
		_update_process();
	}
}

void MultiplayerSynchronizer::_update_process() {
#ifdef TOOLS_ENABLED
	if (Engine::get_singleton()->is_editor_hint()) {
		return;
	}
#endif
	Node *node = is_inside_tree() ? get_node_or_null(root_path) : nullptr;
	if (!node) {
		return;
	}
	set_process_internal(false);
	set_physics_process_internal(false);
	if (!visibility_filters.size()) {
		return;
	}
	switch (visibility_update_mode) {
		case VISIBILITY_PROCESS_IDLE:
			set_process_internal(true);
			break;
		case VISIBILITY_PROCESS_PHYSICS:
			set_physics_process_internal(true);
			break;
		case VISIBILITY_PROCESS_NONE:
			break;
	}
}

Node *MultiplayerSynchronizer::get_root_node() {
	return root_node_cache.is_valid() ? Object::cast_to<Node>(ObjectDB::get_instance(root_node_cache)) : nullptr;
}

void MultiplayerSynchronizer::reset() {
	net_id = 0;
	last_sync_usec = 0;
	last_inbound_sync = 0;
	last_watch_usec = 0;
	sync_started = false;
	watchers.clear();
}

uint32_t MultiplayerSynchronizer::get_net_id() const {
	return net_id;
}

void MultiplayerSynchronizer::set_net_id(uint32_t p_net_id) {
	net_id = p_net_id;
}

bool MultiplayerSynchronizer::update_outbound_sync_time(uint64_t p_usec) {
	if (last_sync_usec == p_usec) {
		// last_sync_usec has been updated in this frame.
		return true;
	}
	if (p_usec < last_sync_usec + sync_interval_usec) {
		// Too soon, should skip this synchronization frame.
		return false;
	}
	last_sync_usec = p_usec;
	return true;
}

bool MultiplayerSynchronizer::update_inbound_sync_time(uint16_t p_network_time) {
	if (!sync_started) {
		sync_started = true;
	} else if (p_network_time <= last_inbound_sync && last_inbound_sync - p_network_time < 32767) {
		return false;
	}
	last_inbound_sync = p_network_time;
	return true;
}

PackedStringArray MultiplayerSynchronizer::get_configuration_warnings() const {
	PackedStringArray warnings = Node::get_configuration_warnings();

	if (root_path.is_empty() || !has_node(root_path)) {
		warnings.push_back(RTR("A valid NodePath must be set in the \"Root Path\" property in order for MultiplayerSynchronizer to be able to synchronize properties."));
	}

	return warnings;
}

Error MultiplayerSynchronizer::get_state(const List<NodePath> &p_properties, Object *p_obj, Vector<Variant> &r_variant, Vector<const Variant *> &r_variant_ptrs) {
	ERR_FAIL_NULL_V(p_obj, ERR_INVALID_PARAMETER);
	r_variant.resize(p_properties.size());
	r_variant_ptrs.resize(r_variant.size());
	int i = 0;
	for (const NodePath &prop : p_properties) {
		bool valid = false;
		const Object *obj = _get_prop_target(p_obj, prop);
		ERR_FAIL_NULL_V(obj, FAILED);
		r_variant.write[i] = obj->get_indexed(prop.get_subnames(), &valid);
		r_variant_ptrs.write[i] = &r_variant[i];
		ERR_FAIL_COND_V_MSG(!valid, ERR_INVALID_DATA, vformat("Property '%s' not found.", prop));
		i++;
	}
	return OK;
}

Error MultiplayerSynchronizer::set_state(const List<NodePath> &p_properties, Object *p_obj, const Vector<Variant> &p_state) {
	ERR_FAIL_NULL_V(p_obj, ERR_INVALID_PARAMETER);
	int i = 0;
	for (const NodePath &prop : p_properties) {
		Object *obj = _get_prop_target(p_obj, prop);
		ERR_FAIL_NULL_V(obj, FAILED);
		obj->set_indexed(prop.get_subnames(), p_state[i]);
		i += 1;
	}
	return OK;
}

bool MultiplayerSynchronizer::is_visibility_public() const {
	return peer_visibility.has(0);
}

void MultiplayerSynchronizer::set_visibility_public(bool p_visible) {
	set_visibility_for(0, p_visible);
}

bool MultiplayerSynchronizer::is_visible_to(int p_peer) {
	if (visibility_filters.size()) {
		Variant arg = p_peer;
		const Variant *argv[1] = { &arg };
		for (Callable filter : visibility_filters) {
			Variant ret;
			Callable::CallError err;
			filter.callp(argv, 1, ret, err);
			ERR_FAIL_COND_V(err.error != Callable::CallError::CALL_OK || ret.get_type() != Variant::BOOL, false);
			if (!ret.operator bool()) {
				return false;
			}
		}
	}
	return peer_visibility.has(0) || peer_visibility.has(p_peer);
}

void MultiplayerSynchronizer::add_visibility_filter(Callable p_callback) {
	visibility_filters.insert(p_callback);
	_update_process();
}

void MultiplayerSynchronizer::remove_visibility_filter(Callable p_callback) {
	visibility_filters.erase(p_callback);
	_update_process();
}

void MultiplayerSynchronizer::set_visibility_for(int p_peer, bool p_visible) {
	if (peer_visibility.has(p_peer) == p_visible) {
		return;
	}
	if (p_visible) {
		peer_visibility.insert(p_peer);
	} else {
		peer_visibility.erase(p_peer);
	}
	update_visibility(p_peer);
}

bool MultiplayerSynchronizer::get_visibility_for(int p_peer) const {
	return peer_visibility.has(p_peer);
}

void MultiplayerSynchronizer::set_visibility_update_mode(VisibilityUpdateMode p_mode) {
	visibility_update_mode = p_mode;
	_update_process();
}

MultiplayerSynchronizer::VisibilityUpdateMode MultiplayerSynchronizer::get_visibility_update_mode() const {
	return visibility_update_mode;
}

void MultiplayerSynchronizer::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_root_path", "path"), &MultiplayerSynchronizer::set_root_path);
	ClassDB::bind_method(D_METHOD("get_root_path"), &MultiplayerSynchronizer::get_root_path);

	ClassDB::bind_method(D_METHOD("set_replication_interval", "milliseconds"), &MultiplayerSynchronizer::set_replication_interval);
	ClassDB::bind_method(D_METHOD("get_replication_interval"), &MultiplayerSynchronizer::get_replication_interval);

	ClassDB::bind_method(D_METHOD("set_delta_interval", "milliseconds"), &MultiplayerSynchronizer::set_delta_interval);
	ClassDB::bind_method(D_METHOD("get_delta_interval"), &MultiplayerSynchronizer::get_delta_interval);

	ClassDB::bind_method(D_METHOD("set_replication_config", "config"), &MultiplayerSynchronizer::set_replication_config);
	ClassDB::bind_method(D_METHOD("get_replication_config"), &MultiplayerSynchronizer::get_replication_config);

	ClassDB::bind_method(D_METHOD("set_visibility_update_mode", "mode"), &MultiplayerSynchronizer::set_visibility_update_mode);
	ClassDB::bind_method(D_METHOD("get_visibility_update_mode"), &MultiplayerSynchronizer::get_visibility_update_mode);
	ClassDB::bind_method(D_METHOD("update_visibility", "for_peer"), &MultiplayerSynchronizer::update_visibility, DEFVAL(0));

	ClassDB::bind_method(D_METHOD("set_visibility_public", "visible"), &MultiplayerSynchronizer::set_visibility_public);
	ClassDB::bind_method(D_METHOD("is_visibility_public"), &MultiplayerSynchronizer::is_visibility_public);

	ClassDB::bind_method(D_METHOD("add_visibility_filter", "filter"), &MultiplayerSynchronizer::add_visibility_filter);
	ClassDB::bind_method(D_METHOD("remove_visibility_filter", "filter"), &MultiplayerSynchronizer::remove_visibility_filter);
	ClassDB::bind_method(D_METHOD("set_visibility_for", "peer", "visible"), &MultiplayerSynchronizer::set_visibility_for);
	ClassDB::bind_method(D_METHOD("get_visibility_for", "peer"), &MultiplayerSynchronizer::get_visibility_for);

	ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "root_path"), "set_root_path", "get_root_path");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "replication_interval", PROPERTY_HINT_RANGE, "0,5,0.001,suffix:s"), "set_replication_interval", "get_replication_interval");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "delta_interval", PROPERTY_HINT_RANGE, "0,5,0.001,suffix:s"), "set_delta_interval", "get_delta_interval");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "replication_config", PROPERTY_HINT_RESOURCE_TYPE, "SceneReplicationConfig", PROPERTY_USAGE_NO_EDITOR | PROPERTY_USAGE_EDITOR_INSTANTIATE_OBJECT), "set_replication_config", "get_replication_config");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "visibility_update_mode", PROPERTY_HINT_ENUM, "Idle,Physics,None"), "set_visibility_update_mode", "get_visibility_update_mode");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "public_visibility"), "set_visibility_public", "is_visibility_public");

	BIND_ENUM_CONSTANT(VISIBILITY_PROCESS_IDLE);
	BIND_ENUM_CONSTANT(VISIBILITY_PROCESS_PHYSICS);
	BIND_ENUM_CONSTANT(VISIBILITY_PROCESS_NONE);

	ADD_SIGNAL(MethodInfo("synchronizing"));
	ADD_SIGNAL(MethodInfo("synchronized"));
	ADD_SIGNAL(MethodInfo("delta_synchronizing"));
	ADD_SIGNAL(MethodInfo("delta_synchronized"));
	ADD_SIGNAL(MethodInfo("visibility_changed", PropertyInfo(Variant::INT, "for_peer")));
}

void MultiplayerSynchronizer::_notification(int p_what) {
#ifdef TOOLS_ENABLED
	if (Engine::get_singleton()->is_editor_hint()) {
		return;
	}
#endif
	if (root_path.is_empty()) {
		return;
	}

	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			_start();
		} break;

		case NOTIFICATION_EXIT_TREE: {
			_stop();
		} break;

		case NOTIFICATION_INTERNAL_PROCESS:
		case NOTIFICATION_INTERNAL_PHYSICS_PROCESS: {
			update_visibility(0);
		} break;
	}
}

void MultiplayerSynchronizer::set_replication_interval(double p_interval) {
	ERR_FAIL_COND_MSG(p_interval < 0, "Interval must be greater or equal to 0 (where 0 means default)");
	sync_interval_usec = uint64_t(p_interval * 1000 * 1000);
}

double MultiplayerSynchronizer::get_replication_interval() const {
	return double(sync_interval_usec) / 1000.0 / 1000.0;
}

void MultiplayerSynchronizer::set_delta_interval(double p_interval) {
	ERR_FAIL_COND_MSG(p_interval < 0, "Interval must be greater or equal to 0 (where 0 means default)");
	delta_interval_usec = uint64_t(p_interval * 1000 * 1000);
}

double MultiplayerSynchronizer::get_delta_interval() const {
	return double(delta_interval_usec) / 1000.0 / 1000.0;
}

void MultiplayerSynchronizer::set_replication_config(Ref<SceneReplicationConfig> p_config) {
	replication_config = p_config;
}

Ref<SceneReplicationConfig> MultiplayerSynchronizer::get_replication_config() {
	return replication_config;
}

void MultiplayerSynchronizer::update_visibility(int p_for_peer) {
#ifdef TOOLS_ENABLED
	if (Engine::get_singleton()->is_editor_hint()) {
		return;
	}
#endif
	Node *node = is_inside_tree() ? get_node_or_null(root_path) : nullptr;
	if (node && get_multiplayer()->has_multiplayer_peer() && is_multiplayer_authority()) {
		emit_signal(SceneStringName(visibility_changed), p_for_peer);
	}
}

void MultiplayerSynchronizer::set_root_path(const NodePath &p_path) {
	if (p_path == root_path) {
		return;
	}
	_stop();
	root_path = p_path;
	_start();
	update_configuration_warnings();
}

NodePath MultiplayerSynchronizer::get_root_path() const {
	return root_path;
}

void MultiplayerSynchronizer::set_multiplayer_authority(int p_peer_id, bool p_recursive) {
	if (get_multiplayer_authority() == p_peer_id) {
		return;
	}
	_stop();
	Node::set_multiplayer_authority(p_peer_id, p_recursive);
	_start();
}

Error MultiplayerSynchronizer::_watch_changes(uint64_t p_usec) {
	ERR_FAIL_COND_V(replication_config.is_null(), FAILED);
	const List<NodePath> props = replication_config->get_watch_properties();
	if (props.size() != watchers.size()) {
		watchers.resize(props.size());
	}
	if (props.size() == 0) {
		return OK;
	}
	Node *node = get_root_node();
	ERR_FAIL_NULL_V(node, FAILED);
	int idx = -1;
	Watcher *ptr = watchers.ptrw();
	for (const NodePath &prop : props) {
		idx++;
		bool valid = false;
		const Object *obj = _get_prop_target(node, prop);
		ERR_CONTINUE_MSG(!obj, vformat("Node not found for property '%s'.", prop));
		Variant v = obj->get_indexed(prop.get_subnames(), &valid);
		ERR_CONTINUE_MSG(!valid, vformat("Property '%s' not found.", prop));
		Watcher &w = ptr[idx];
		if (w.prop != prop) {
			w.prop = prop;
			w.value = v.duplicate(true);
			w.last_change_usec = p_usec;
		} else if (!w.value.hash_compare(v)) {
			w.value = v.duplicate(true);
			w.last_change_usec = p_usec;
		}
	}
	return OK;
}

List<Variant> MultiplayerSynchronizer::get_delta_state(uint64_t p_cur_usec, uint64_t p_last_usec, uint64_t &r_indexes) {
	r_indexes = 0;
	List<Variant> out;

	if (last_watch_usec == p_cur_usec) {
		// We already watched for changes in this frame.

	} else if (p_cur_usec < p_last_usec + delta_interval_usec) {
		// Too soon skip delta synchronization.
		return out;

	} else {
		// Watch for changes.
		Error err = _watch_changes(p_cur_usec);
		ERR_FAIL_COND_V(err != OK, out);
		last_watch_usec = p_cur_usec;
	}

	const Watcher *ptr = watchers.size() ? watchers.ptr() : nullptr;
	for (int i = 0; i < watchers.size(); i++) {
		const Watcher &w = ptr[i];
		if (w.last_change_usec <= p_last_usec) {
			continue;
		}
		out.push_back(w.value);
		r_indexes |= 1ULL << i;
	}
	return out;
}

List<NodePath> MultiplayerSynchronizer::get_delta_properties(uint64_t p_indexes) {
	List<NodePath> out;
	ERR_FAIL_COND_V(replication_config.is_null(), out);
	const List<NodePath> watch_props = replication_config->get_watch_properties();
	int idx = 0;
	for (const NodePath &prop : watch_props) {
		if ((p_indexes & (1ULL << idx++)) == 0) {
			continue;
		}
		out.push_back(prop);
	}
	return out;
}

SceneReplicationConfig *MultiplayerSynchronizer::get_replication_config_ptr() const {
	return replication_config.ptr();
}

MultiplayerSynchronizer::MultiplayerSynchronizer() {
	// Publicly visible by default.
	peer_visibility.insert(0);
}

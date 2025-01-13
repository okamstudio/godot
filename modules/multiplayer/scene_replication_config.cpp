/**************************************************************************/
/*  scene_replication_config.cpp                                          */
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

#include "scene_replication_config.h"

bool SceneReplicationConfig::_set(const StringName &p_name, const Variant &p_value) {
	String prop_name = p_name;

	if (prop_name.begins_with("properties/")) {
		int idx = prop_name.get_slicec('/', 1).to_int();
		String what = prop_name.get_slicec('/', 2);

		if (properties.size() == idx && what == "path") {
			ERR_FAIL_COND_V(p_value.get_type() != Variant::NODE_PATH, false);
			NodePath path = p_value;
			ERR_FAIL_COND_V(path.is_empty() || path.get_subname_count() == 0, false);
			add_property(path);
			return true;
		}
		ERR_FAIL_INDEX_V(idx, properties.size(), false);
		const ReplicationProperty &prop = properties.get(idx);
		if (what == "replication_mode") {
			ERR_FAIL_COND_V(p_value.get_type() != Variant::INT, false);
			ReplicationMode mode = (ReplicationMode)p_value.operator int();
			ERR_FAIL_COND_V(mode < REPLICATION_MODE_NEVER || mode > REPLICATION_MODE_ON_CHANGE, false);
			property_set_replication_mode(prop.name, mode);
			return true;
		}
		ERR_FAIL_COND_V(p_value.get_type() != Variant::BOOL, false);
		if (what == "spawn") {
			property_set_spawn(prop.name, p_value);
			return true;
		} else if (what == "sync") {
			// Deprecated.
			property_set_sync(prop.name, p_value);
			return true;
		} else if (what == "watch") {
			// Deprecated.
			property_set_watch(prop.name, p_value);
			return true;
		}
	}
	return false;
}

bool SceneReplicationConfig::_get(const StringName &p_name, Variant &r_ret) const {
	String prop_name = p_name;

	if (prop_name.begins_with("properties/")) {
		int idx = prop_name.get_slicec('/', 1).to_int();
		String what = prop_name.get_slicec('/', 2);
		ERR_FAIL_INDEX_V(idx, properties.size(), false);
		const ReplicationProperty &prop = properties.get(idx);
		if (what == "path") {
			r_ret = prop.name;
			return true;
		} else if (what == "spawn") {
			r_ret = prop.spawn;
			return true;
		} else if (what == "replication_mode") {
			r_ret = prop.mode;
			return true;
		}
	}
	return false;
}

void SceneReplicationConfig::_get_property_list(List<PropertyInfo> *p_list) const {
	for (int i = 0; i < properties.size(); i++) {
		p_list->push_back(PropertyInfo(Variant::STRING, "properties/" + itos(i) + "/path", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NO_EDITOR | PROPERTY_USAGE_INTERNAL));
		p_list->push_back(PropertyInfo(Variant::STRING, "properties/" + itos(i) + "/spawn", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NO_EDITOR | PROPERTY_USAGE_INTERNAL));
		p_list->push_back(PropertyInfo(Variant::INT, "properties/" + itos(i) + "/replication_mode", PROPERTY_HINT_ENUM, "Never,Always,On Change", PROPERTY_USAGE_NO_EDITOR | PROPERTY_USAGE_INTERNAL));
	}
}

void SceneReplicationConfig::reset_state() {
	dirty = false;
	properties.clear();
	sync_props.clear();
	spawn_props.clear();
	watch_props.clear();
}

TypedArray<NodePath> SceneReplicationConfig::get_properties() const {
	TypedArray<NodePath> paths;
	for (const ReplicationProperty &prop : properties) {
		paths.push_back(prop.name);
	}
	return paths;
}

void SceneReplicationConfig::add_property(const NodePath &p_path, int p_index) {
	ERR_FAIL_COND(properties.find(p_path));
	ERR_FAIL_COND(p_path == NodePath());

	if (p_index < 0 || p_index == properties.size()) {
		properties.push_back(ReplicationProperty(p_path));
		dirty = true;
		return;
	}

	ERR_FAIL_INDEX(p_index, properties.size());

	List<ReplicationProperty>::Element *I = properties.front();
	int c = 0;
	while (c < p_index) {
		I = I->next();
		c++;
	}
	properties.insert_before(I, ReplicationProperty(p_path));
	dirty = true;
}

void SceneReplicationConfig::remove_property(const NodePath &p_path) {
	properties.erase(p_path);
	dirty = true;
}

bool SceneReplicationConfig::has_property(const NodePath &p_path) const {
	for (const ReplicationProperty &property : properties) {
		if (property.name == p_path) {
			return true;
		}
	}
	return false;
}

int SceneReplicationConfig::property_get_index(const NodePath &p_path) const {
	int i = 0;
	for (List<ReplicationProperty>::ConstIterator itr = properties.begin(); itr != properties.end(); ++itr, ++i) {
		if (itr->name == p_path) {
			return i;
		}
	}
	ERR_FAIL_V(-1);
}

bool SceneReplicationConfig::property_get_spawn(const NodePath &p_path) {
	List<ReplicationProperty>::Element *E = properties.find(p_path);
	ERR_FAIL_COND_V(!E, false);
	return E->get().spawn;
}

void SceneReplicationConfig::property_set_spawn(const NodePath &p_path, bool p_enabled) {
	List<ReplicationProperty>::Element *E = properties.find(p_path);
	ERR_FAIL_COND(!E);
	if (E->get().spawn == p_enabled) {
		return;
	}
	E->get().spawn = p_enabled;
	dirty = true;
}

bool SceneReplicationConfig::property_get_sync(const NodePath &p_path) {
	List<ReplicationProperty>::Element *E = properties.find(p_path);
	ERR_FAIL_COND_V(!E, false);
	return E->get().mode == REPLICATION_MODE_ALWAYS;
}

void SceneReplicationConfig::property_set_sync(const NodePath &p_path, bool p_enabled) {
	if (p_enabled) {
		property_set_replication_mode(p_path, REPLICATION_MODE_ALWAYS);
	} else if (property_get_replication_mode(p_path) == REPLICATION_MODE_ALWAYS) {
		property_set_replication_mode(p_path, REPLICATION_MODE_NEVER);
	}
}

bool SceneReplicationConfig::property_get_watch(const NodePath &p_path) {
	List<ReplicationProperty>::Element *E = properties.find(p_path);
	ERR_FAIL_COND_V(!E, false);
	return E->get().mode == REPLICATION_MODE_ON_CHANGE;
}

void SceneReplicationConfig::property_set_watch(const NodePath &p_path, bool p_enabled) {
	if (p_enabled) {
		property_set_replication_mode(p_path, REPLICATION_MODE_ON_CHANGE);
	} else if (property_get_replication_mode(p_path) == REPLICATION_MODE_ON_CHANGE) {
		property_set_replication_mode(p_path, REPLICATION_MODE_NEVER);
	}
}

SceneReplicationConfig::ReplicationMode SceneReplicationConfig::property_get_replication_mode(const NodePath &p_path) {
	List<ReplicationProperty>::Element *E = properties.find(p_path);
	ERR_FAIL_COND_V(!E, REPLICATION_MODE_NEVER);
	return E->get().mode;
}

void SceneReplicationConfig::property_set_replication_mode(const NodePath &p_path, ReplicationMode p_mode) {
	List<ReplicationProperty>::Element *E = properties.find(p_path);
	ERR_FAIL_COND(!E);
	if (E->get().mode == p_mode) {
		return;
	}
	E->get().mode = p_mode;
	dirty = true;
}

void SceneReplicationConfig::_update() {
	if (!dirty) {
		return;
	}
	dirty = false;
	sync_props.clear();
	spawn_props.clear();
	watch_props.clear();
	for (const ReplicationProperty &prop : properties) {
		if (prop.spawn) {
			spawn_props.push_back(prop.name);
		}
		switch (prop.mode) {
			case REPLICATION_MODE_ALWAYS:
				sync_props.push_back(prop.name);
				break;
			case REPLICATION_MODE_ON_CHANGE:
				watch_props.push_back(prop.name);
				break;
			default:
				break;
		}
	}
}

const List<NodePath> &SceneReplicationConfig::get_spawn_properties() {
	if (dirty) {
		_update();
	}
	return spawn_props;
}

const List<NodePath> &SceneReplicationConfig::get_sync_properties() {
	if (dirty) {
		_update();
	}
	return sync_props;
}

const List<NodePath> &SceneReplicationConfig::get_watch_properties() {
	if (dirty) {
		_update();
	}
	return watch_props;
}

void SceneReplicationConfig::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_properties"), &SceneReplicationConfig::get_properties);
	ClassDB::bind_method(D_METHOD("add_property", "path", "index"), &SceneReplicationConfig::add_property, DEFVAL(-1));
	ClassDB::bind_method(D_METHOD("has_property", "path"), &SceneReplicationConfig::has_property);
	ClassDB::bind_method(D_METHOD("remove_property", "path"), &SceneReplicationConfig::remove_property);
	ClassDB::bind_method(D_METHOD("property_get_index", "path"), &SceneReplicationConfig::property_get_index);
	ClassDB::bind_method(D_METHOD("property_get_spawn", "path"), &SceneReplicationConfig::property_get_spawn);
	ClassDB::bind_method(D_METHOD("property_set_spawn", "path", "enabled"), &SceneReplicationConfig::property_set_spawn);
	ClassDB::bind_method(D_METHOD("property_get_replication_mode", "path"), &SceneReplicationConfig::property_get_replication_mode);
	ClassDB::bind_method(D_METHOD("property_set_replication_mode", "path", "mode"), &SceneReplicationConfig::property_set_replication_mode);

	BIND_ENUM_CONSTANT(REPLICATION_MODE_NEVER);
	BIND_ENUM_CONSTANT(REPLICATION_MODE_ALWAYS);
	BIND_ENUM_CONSTANT(REPLICATION_MODE_ON_CHANGE);

	// Deprecated.
	ClassDB::bind_method(D_METHOD("property_get_sync", "path"), &SceneReplicationConfig::property_get_sync);
	ClassDB::bind_method(D_METHOD("property_set_sync", "path", "enabled"), &SceneReplicationConfig::property_set_sync);
	ClassDB::bind_method(D_METHOD("property_get_watch", "path"), &SceneReplicationConfig::property_get_watch);
	ClassDB::bind_method(D_METHOD("property_set_watch", "path", "enabled"), &SceneReplicationConfig::property_set_watch);
}

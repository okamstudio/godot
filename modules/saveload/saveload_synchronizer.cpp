/**************************************************************************/
/*  saveload_synchronizer.cpp                                             */
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

#include "saveload_synchronizer.h"
#include "saveload_api.h"

Dictionary SaveloadSynchronizer::SyncherState::to_dict() const {
	Dictionary dict;
	for (const KeyValue<const NodePath, Variant> &property : property_map) {
		dict[property.key] = property.value;
	}
	return dict;
};

SaveloadSynchronizer::SyncherState::SyncherState(const Dictionary &p_dict) {
	List<Variant> property_keys;
	p_dict.get_key_list(&property_keys);
	for (const NodePath property_key : property_keys) {
		property_map.insert(property_key, p_dict[property_key]);
	}
}

void SaveloadSynchronizer::_stop() {
#ifdef TOOLS_ENABLED
	if (Engine::get_singleton()->is_editor_hint()) {
		return;
	}
#endif
	root_node_cache = ObjectID();
	Node *node = is_inside_tree() ? get_node_or_null(root_path) : nullptr;
	if (node) {
		SaveloadAPI::get_singleton()->untrack(this);
	}
}

void SaveloadSynchronizer::_start() {
#ifdef TOOLS_ENABLED
	if (Engine::get_singleton()->is_editor_hint()) {
		return;
	}
#endif
	root_node_cache = ObjectID();
	Node *node = is_inside_tree() ? get_node_or_null(root_path) : nullptr;
	if (node) {
		root_node_cache = node->get_instance_id();
		SaveloadAPI::get_singleton()->track(this);
		_update_process();
	}
}

void SaveloadSynchronizer::_update_process() {
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
}

Node *SaveloadSynchronizer::get_root_node() const {
	return root_node_cache.is_valid() ? Object::cast_to<Node>(ObjectDB::get_instance(root_node_cache)) : nullptr;
}

PackedStringArray SaveloadSynchronizer::get_configuration_warnings() const {
	PackedStringArray warnings = Node::get_configuration_warnings();

	if (root_path.is_empty() || !has_node(root_path)) {
		warnings.push_back(RTR("A valid NodePath must be set in the \"Root Path\" property in order for SaveloadSynchronizer to be able to synchronize properties."));
	}

	return warnings;
}

SaveloadSynchronizer::SyncherState SaveloadSynchronizer::get_syncher_state() const {
	const List<NodePath> props = get_saveload_config()->get_sync_properties();
	SyncherState sync_state;
	sync_state.property_map.reserve(props.size());
	Node *root_node = get_root_node();
	ERR_FAIL_COND_V_MSG(!root_node, sync_state, vformat("Could not find root node at %s.", get_root_path()));
	for (const NodePath &prop : props) {
		const Object *obj;
		if (prop.get_name_count() == 0) {
			obj = root_node;
		} else {
			ERR_CONTINUE_MSG(!root_node->has_node(prop), vformat("Node '%s' not found.", prop));
			obj = root_node->get_node(prop);
		}
		bool valid = false;
		Variant value = obj->get_indexed(prop.get_subnames(), &valid);
		ERR_CONTINUE_MSG(!valid, vformat("Property '%s' not found.", prop));
		sync_state.property_map.insert(prop, value);
	}
	return sync_state;
}

Error SaveloadSynchronizer::set_syncher_state(const SaveloadSynchronizer::SyncherState &p_syncher_state) {
	for (const KeyValue<const NodePath, Variant> &property : p_syncher_state.property_map) {
		const NodePath path = property.key;
		const NodePath node_path = NodePath(path.get_concatenated_names());
		Node *node = get_root_node()->get_node_or_null(node_path);
		ERR_CONTINUE_MSG(!node, vformat("could not find node at %s", node_path));
		node->set_indexed(path.get_subnames(), property.value); //TODO: what if node doesn't have property?
	}
	return OK; //TODO: need to return a useful error
}

void SaveloadSynchronizer::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_root_path", "path"), &SaveloadSynchronizer::set_root_path);
	ClassDB::bind_method(D_METHOD("get_root_path"), &SaveloadSynchronizer::get_root_path);

	ClassDB::bind_method(D_METHOD("set_saveload_config", "config"), &SaveloadSynchronizer::set_saveload_config);
	ClassDB::bind_method(D_METHOD("get_saveload_config"), &SaveloadSynchronizer::get_saveload_config);

	ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "root_path"), "set_root_path", "get_root_path");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "saveload_config", PROPERTY_HINT_RESOURCE_TYPE, "SceneSaveloadConfig", PROPERTY_USAGE_NO_EDITOR), "set_saveload_config", "get_saveload_config");

	ADD_SIGNAL(MethodInfo("synchronized"));
}

void SaveloadSynchronizer::_notification(int p_what) {
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
	}
}

void SaveloadSynchronizer::set_saveload_config(Ref<SceneSaveloadConfig> p_config) {
	saveload_config = p_config;
}

Ref<SceneSaveloadConfig> SaveloadSynchronizer::get_saveload_config() const {
	return saveload_config;
}

void SaveloadSynchronizer::set_root_path(const NodePath &p_path) {
	_stop();
	root_path = p_path;
	_start();
}

NodePath SaveloadSynchronizer::get_root_path() const {
	return root_path;
}

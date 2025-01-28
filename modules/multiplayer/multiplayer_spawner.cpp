/**************************************************************************/
/*  multiplayer_spawner.cpp                                               */
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

#include "multiplayer_spawner.h"

#include "scene/main/multiplayer_api.h"

#ifdef TOOLS_ENABLED
/* This is editor only */
bool MultiplayerSpawner::_set(const StringName &p_name, const Variant &p_value) {
	if (p_name == "_spawnable_scene_count") {
		spawnable_scenes.resize(p_value);
		notify_property_list_changed();
		return true;
	} else {
		String ns = p_name;
		if (ns.begins_with("scenes/")) {
			uint32_t index = ns.get_slicec('/', 1).to_int();
			ERR_FAIL_UNSIGNED_INDEX_V(index, spawnable_scenes.size(), false);
			spawnable_scenes[index].path = ResourceUID::ensure_path(p_value);
			return true;
		}
	}
	return false;
}

bool MultiplayerSpawner::_get(const StringName &p_name, Variant &r_ret) const {
	if (p_name == "_spawnable_scene_count") {
		r_ret = spawnable_scenes.size();
		return true;
	} else {
		String ns = p_name;
		if (ns.begins_with("scenes/")) {
			uint32_t index = ns.get_slicec('/', 1).to_int();
			ERR_FAIL_UNSIGNED_INDEX_V(index, spawnable_scenes.size(), false);
			r_ret = ResourceUID::path_to_uid(spawnable_scenes[index].path);
			return true;
		}
	}
	return false;
}

void MultiplayerSpawner::_get_property_list(List<PropertyInfo> *p_list) const {
	p_list->push_back(PropertyInfo(Variant::INT, "_spawnable_scene_count", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_EDITOR | PROPERTY_USAGE_ARRAY, "Auto Spawn List,scenes/"));
	List<String> exts;
	ResourceLoader::get_recognized_extensions_for_type("PackedScene", &exts);
	String ext_hint;
	for (const String &E : exts) {
		if (!ext_hint.is_empty()) {
			ext_hint += ",";
		}
		ext_hint += "*." + E;
	}
	for (uint32_t i = 0; i < spawnable_scenes.size(); i++) {
		p_list->push_back(PropertyInfo(Variant::STRING, "scenes/" + itos(i), PROPERTY_HINT_FILE, ext_hint, PROPERTY_USAGE_EDITOR));
	}
}
#endif

PackedStringArray MultiplayerSpawner::get_configuration_warnings() const {
	PackedStringArray warnings = Node::get_configuration_warnings();

	if (spawn_path.is_empty() || !has_node(spawn_path)) {
		warnings.push_back(RTR("A valid NodePath must be set in the \"Spawn Path\" property in order for MultiplayerSpawner to be able to spawn Nodes."));
	}
	return warnings;
}

void MultiplayerSpawner::add_spawnable_scene(const String &p_path) {
	SpawnableScene sc;
	sc.path = ResourceUID::ensure_path(p_path);
	if (Engine::get_singleton()->is_editor_hint()) {
		ERR_FAIL_COND(!ResourceLoader::exists(sc.path));
	}
	spawnable_scenes.push_back(sc);
#ifdef TOOLS_ENABLED
	if (Engine::get_singleton()->is_editor_hint()) {
		return;
	}
#endif
	Node *node = get_spawn_node();
	if (spawnable_scenes.size() == 1 && node && !node->is_connected("child_entered_tree", callable_mp(this, &MultiplayerSpawner::_node_added))) {
		node->connect("child_entered_tree", callable_mp(this, &MultiplayerSpawner::_node_added));
	}
}

int MultiplayerSpawner::get_spawnable_scene_count() const {
	return spawnable_scenes.size();
}

String MultiplayerSpawner::get_spawnable_scene(int p_idx) const {
	ERR_FAIL_INDEX_V(p_idx, (int)spawnable_scenes.size(), "");
	return spawnable_scenes[p_idx].path;
}

void MultiplayerSpawner::clear_spawnable_scenes() {
	spawnable_scenes.clear();
#ifdef TOOLS_ENABLED
	if (Engine::get_singleton()->is_editor_hint()) {
		return;
	}
#endif
	Node *node = get_spawn_node();
	if (node && node->is_connected("child_entered_tree", callable_mp(this, &MultiplayerSpawner::_node_added))) {
		node->disconnect("child_entered_tree", callable_mp(this, &MultiplayerSpawner::_node_added));
	}
}

Vector<String> MultiplayerSpawner::_get_spawnable_scenes() const {
	Vector<String> ss;
	ss.resize(spawnable_scenes.size());
	for (int i = 0; i < ss.size(); i++) {
		ss.write[i] = ResourceUID::path_to_uid(spawnable_scenes[i].path);
	}
	return ss;
}

void MultiplayerSpawner::_set_spawnable_scenes(const Vector<String> &p_scenes) {
	clear_spawnable_scenes();
	for (int i = 0; i < p_scenes.size(); i++) {
		add_spawnable_scene(p_scenes[i]);
	}
}

void MultiplayerSpawner::_bind_methods() {
	ClassDB::bind_method(D_METHOD("add_spawnable_scene", "path"), &MultiplayerSpawner::add_spawnable_scene);
	ClassDB::bind_method(D_METHOD("get_spawnable_scene_count"), &MultiplayerSpawner::get_spawnable_scene_count);
	ClassDB::bind_method(D_METHOD("get_spawnable_scene", "index"), &MultiplayerSpawner::get_spawnable_scene);
	ClassDB::bind_method(D_METHOD("clear_spawnable_scenes"), &MultiplayerSpawner::clear_spawnable_scenes);

	ClassDB::bind_method(D_METHOD("_get_spawnable_scenes"), &MultiplayerSpawner::_get_spawnable_scenes);
	ClassDB::bind_method(D_METHOD("_set_spawnable_scenes", "scenes"), &MultiplayerSpawner::_set_spawnable_scenes);

	ADD_PROPERTY(PropertyInfo(Variant::PACKED_STRING_ARRAY, "_spawnable_scenes", PROPERTY_HINT_NONE, "", (PROPERTY_USAGE_STORAGE | PROPERTY_USAGE_INTERNAL)), "_set_spawnable_scenes", "_get_spawnable_scenes");

	ClassDB::bind_method(D_METHOD("spawn", "data"), &MultiplayerSpawner::spawn, DEFVAL(Variant()));

	ClassDB::bind_method(D_METHOD("get_spawn_path"), &MultiplayerSpawner::get_spawn_path);
	ClassDB::bind_method(D_METHOD("set_spawn_path", "path"), &MultiplayerSpawner::set_spawn_path);
	ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "spawn_path", PROPERTY_HINT_NONE, ""), "set_spawn_path", "get_spawn_path");

	ClassDB::bind_method(D_METHOD("get_spawn_limit"), &MultiplayerSpawner::get_spawn_limit);
	ClassDB::bind_method(D_METHOD("set_spawn_limit", "limit"), &MultiplayerSpawner::set_spawn_limit);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "spawn_limit", PROPERTY_HINT_RANGE, "0,1024,1,or_greater"), "set_spawn_limit", "get_spawn_limit");

	ClassDB::bind_method(D_METHOD("get_spawn_function"), &MultiplayerSpawner::get_spawn_function);
	ClassDB::bind_method(D_METHOD("set_spawn_function", "spawn_function"), &MultiplayerSpawner::set_spawn_function);
	ADD_PROPERTY(PropertyInfo(Variant::CALLABLE, "spawn_function", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "set_spawn_function", "get_spawn_function");

	ADD_SIGNAL(MethodInfo("despawned", PropertyInfo(Variant::OBJECT, "node", PROPERTY_HINT_RESOURCE_TYPE, "Node")));
	ADD_SIGNAL(MethodInfo("spawned", PropertyInfo(Variant::OBJECT, "node", PROPERTY_HINT_RESOURCE_TYPE, "Node")));
}

void MultiplayerSpawner::_update_spawn_node() {
#ifdef TOOLS_ENABLED
	if (Engine::get_singleton()->is_editor_hint()) {
		return;
	}
#endif
	if (spawn_node.is_valid()) {
		Node *node = Object::cast_to<Node>(ObjectDB::get_instance(spawn_node));
		if (node && node->is_connected("child_entered_tree", callable_mp(this, &MultiplayerSpawner::_node_added))) {
			node->disconnect("child_entered_tree", callable_mp(this, &MultiplayerSpawner::_node_added));
		}
	}
	Node *node = spawn_path.is_empty() && is_inside_tree() ? nullptr : get_node_or_null(spawn_path);
	if (node) {
		spawn_node = node->get_instance_id();
		if (get_spawnable_scene_count()) {
			node->connect("child_entered_tree", callable_mp(this, &MultiplayerSpawner::_node_added));
		}
	} else {
		spawn_node = ObjectID();
	}
}

void MultiplayerSpawner::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_POST_ENTER_TREE: {
			_update_spawn_node();
		} break;

		case NOTIFICATION_EXIT_TREE: {
			_update_spawn_node();

			for (const KeyValue<ObjectID, SpawnInfo> &E : tracked_nodes) {
				Node *node = Object::cast_to<Node>(ObjectDB::get_instance(E.key));
				ERR_CONTINUE(!node);
				node->disconnect(SceneStringName(tree_exiting), callable_mp(this, &MultiplayerSpawner::_node_exit));
				get_multiplayer()->object_configuration_remove(node, this);
			}
			tracked_nodes.clear();
		} break;
	}
}

void MultiplayerSpawner::_node_added(Node *p_node) {
	if (!get_multiplayer()->has_multiplayer_peer() || !is_multiplayer_authority()) {
		return;
	}
	if (tracked_nodes.has(p_node->get_instance_id())) {
		return;
	}
	const Node *parent = get_spawn_node();
	if (!parent || p_node->get_parent() != parent) {
		return;
	}
	int id = find_spawnable_scene_index_from_path(p_node->get_scene_file_path());
	if (id == INVALID_ID) {
		return;
	}
	const String name = p_node->get_name();
	ERR_FAIL_COND_MSG(name.validate_node_name() != name, vformat("Unable to auto-spawn node with reserved name: %s. Make sure to add your replicated scenes via 'add_child(node, true)' to produce valid names.", name));
	_track(p_node, Variant(), id);
}

NodePath MultiplayerSpawner::get_spawn_path() const {
	return spawn_path;
}

void MultiplayerSpawner::set_spawn_path(const NodePath &p_path) {
	spawn_path = p_path;
	_update_spawn_node();
	update_configuration_warnings();
}

void MultiplayerSpawner::_track(Node *p_node, const Variant &p_argument, int p_scene_id) {
	ObjectID oid = p_node->get_instance_id();
	if (!tracked_nodes.has(oid)) {
		tracked_nodes[oid] = SpawnInfo(p_argument.duplicate(true), p_scene_id);
		p_node->connect(SceneStringName(tree_exiting), callable_mp(this, &MultiplayerSpawner::_node_exit).bind(p_node->get_instance_id()), CONNECT_ONE_SHOT);
		_spawn_notify(p_node->get_instance_id());
	}
}

void MultiplayerSpawner::_spawn_notify(ObjectID p_id) {
	get_multiplayer()->object_configuration_add(ObjectDB::get_instance(p_id), this);
}

void MultiplayerSpawner::_node_exit(ObjectID p_id) {
	Node *node = Object::cast_to<Node>(ObjectDB::get_instance(p_id));
	ERR_FAIL_NULL(node);
	if (tracked_nodes.has(p_id)) {
		tracked_nodes.erase(p_id);
		get_multiplayer()->object_configuration_remove(node, this);
	}
}

int MultiplayerSpawner::find_spawnable_scene_index_from_path(const String &p_scene) const {
	for (uint32_t i = 0; i < spawnable_scenes.size(); i++) {
		if (spawnable_scenes[i].path == p_scene) {
			return i;
		}
	}
	return INVALID_ID;
}

int MultiplayerSpawner::find_spawnable_scene_index_from_object(const ObjectID &p_id) const {
	const SpawnInfo *info = tracked_nodes.getptr(p_id);
	return info ? info->id : INVALID_ID;
}

const Variant MultiplayerSpawner::get_spawn_argument(const ObjectID &p_id) const {
	const SpawnInfo *info = tracked_nodes.getptr(p_id);
	return info ? info->args : Variant();
}

Node *MultiplayerSpawner::instantiate_scene(int p_id) {
	ERR_FAIL_COND_V_MSG(spawn_limit && spawn_limit <= tracked_nodes.size(), nullptr, "Spawn limit reached!");
	ERR_FAIL_UNSIGNED_INDEX_V((uint32_t)p_id, spawnable_scenes.size(), nullptr);
	SpawnableScene &sc = spawnable_scenes[p_id];
	if (sc.cache.is_null()) {
		sc.cache = ResourceLoader::load(sc.path);
	}
	ERR_FAIL_COND_V_MSG(sc.cache.is_null(), nullptr, "Invalid spawnable scene: " + sc.path);
	return sc.cache->instantiate();
}

Node *MultiplayerSpawner::instantiate_custom(const Variant &p_data) {
	ERR_FAIL_COND_V_MSG(spawn_limit && spawn_limit <= tracked_nodes.size(), nullptr, "Spawn limit reached!");
	ERR_FAIL_COND_V_MSG(!spawn_function.is_valid(), nullptr, "Custom spawn requires a valid 'spawn_function'.");
	const Variant *argv[1] = { &p_data };
	Variant ret;
	Callable::CallError ce;
	spawn_function.callp(argv, 1, ret, ce);
	ERR_FAIL_COND_V_MSG(ce.error != Callable::CallError::CALL_OK, nullptr, "Failed to call spawn function.");
	ERR_FAIL_COND_V_MSG(ret.get_type() != Variant::OBJECT, nullptr, "The spawn function must return a Node.");
	return Object::cast_to<Node>(ret.operator Object *());
}

Node *MultiplayerSpawner::spawn(const Variant &p_data) {
	ERR_FAIL_COND_V(!is_inside_tree() || !get_multiplayer()->has_multiplayer_peer() || !is_multiplayer_authority(), nullptr);
	ERR_FAIL_COND_V_MSG(spawn_limit && spawn_limit <= tracked_nodes.size(), nullptr, "Spawn limit reached!");
	ERR_FAIL_COND_V_MSG(!spawn_function.is_valid(), nullptr, "Custom spawn requires the 'spawn_function' property to be a valid callable.");

	Node *parent = get_spawn_node();
	ERR_FAIL_NULL_V_MSG(parent, nullptr, "Cannot find spawn node.");

	Node *node = instantiate_custom(p_data);
	ERR_FAIL_NULL_V_MSG(node, nullptr, "The 'spawn_function' callable must return a valid node.");

	_track(node, p_data);
	parent->add_child(node, true);
	return node;
}

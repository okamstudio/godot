/**************************************************************************/
/*  saveload_spawner.cpp                                                  */
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

#include "saveload_spawner.h"
#include "saveload_api.h"
#include "scene_saveload.h"

#include "scene/main/window.h"
#include "scene/scene_string_names.h"

/*******************************
 * SpawnInfo Definitions Start *
 *******************************/

Dictionary SaveloadSpawner::SpawnInfo::to_dict() const {
	Dictionary dict;
	dict[StringName("path")] = path;
	dict[StringName("scene_index")] = scene_index;
	dict[StringName("spawn_args")] = spawn_args;
	return dict;
}

SaveloadSpawner::SpawnInfo::SpawnInfo(NodePath p_path, int p_scene_index, Variant p_spawn_args = Variant()) {
	path = p_path;
	scene_index = p_scene_index;
	spawn_args = p_spawn_args;
}

SaveloadSpawner::SpawnInfo::SpawnInfo(const Dictionary &p_dict) {
	path = p_dict[StringName("path")];
	scene_index = p_dict[StringName("scene_index")];
	spawn_args = p_dict[StringName("spawn_args")];
}

/*****************************
 * SpawnInfo Definitions End *
 *****************************/

/********************************
 * SpawnState Definitions Start *
 ********************************/

bool SaveloadSpawner::SpawnerState::has(const NodePath &p_path) const {
	return tracked_paths.has(p_path);
}

bool SaveloadSpawner::SpawnerState::erase(const NodePath &p_path) {
	if (!tracked_paths.has(p_path)) {
		return false;
	}
	int index = tracked_paths[p_path];
	spawn_infos.remove_at(index);
	tracked_paths.erase(p_path);
	return true;
}

void SaveloadSpawner::SpawnerState::clear() {
	spawn_infos.clear();
	tracked_paths.clear();
}

TypedArray<Dictionary> SaveloadSpawner::SpawnerState::to_array() const {
	TypedArray<Dictionary> array;
	array.resize(spawn_infos.size());
	for (uint32_t i = 0; i < spawn_infos.size(); ++i) {
		array[i] = spawn_infos[i].to_dict();
	}
	return array;
}

SaveloadSpawner::SpawnerState::SpawnerState(const TypedArray<Dictionary> &p_array) {
	tracked_paths.clear();
	spawn_infos.clear();
	spawn_infos.reserve(p_array.size());
	for (int i = 0; i < p_array.size(); ++i) {
		spawn_infos.push_back(SpawnInfo(p_array[i]));
		tracked_paths.insert(spawn_infos[i].path, i);
	}
}

/******************************
 * SpawnState Definitions End *
 ******************************/

/*************************************
 * SaveloadSpawner Definitions Start *
 *************************************/

#ifdef TOOLS_ENABLED
/* This is editor only */
bool SaveloadSpawner::_set(const StringName &p_name, const Variant &p_value) {
	if (p_name == "_spawnable_scene_count") {
		spawnable_scenes.resize(p_value);
		notify_property_list_changed();
		return true;
	} else {
		String ns = p_name;
		if (ns.begins_with("scenes/")) {
			uint32_t index = ns.get_slicec('/', 1).to_int();
			ERR_FAIL_UNSIGNED_INDEX_V(index, spawnable_scenes.size(), false);
			spawnable_scenes[index].path = p_value;
			return true;
		}
	}
	return false;
}

bool SaveloadSpawner::_get(const StringName &p_name, Variant &r_ret) const {
	if (p_name == "_spawnable_scene_count") {
		r_ret = spawnable_scenes.size();
		return true;
	} else {
		String ns = p_name;
		if (ns.begins_with("scenes/")) {
			uint32_t index = ns.get_slicec('/', 1).to_int();
			ERR_FAIL_UNSIGNED_INDEX_V(index, spawnable_scenes.size(), false);
			r_ret = spawnable_scenes[index].path;
			return true;
		}
	}
	return false;
}

void SaveloadSpawner::_get_property_list(List<PropertyInfo> *p_list) const {
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

PackedStringArray SaveloadSpawner::get_configuration_warnings() const {
	PackedStringArray warnings = Node::get_configuration_warnings();

	if (spawn_path.is_empty() || !has_node(spawn_path)) {
		warnings.push_back(RTR("A valid NodePath must be set in the \"Spawn Path\" property in order for SaveloadSpawner to be able to spawn Nodes."));
	}
	return warnings;
}

void SaveloadSpawner::add_spawnable_scene(const String &p_path) {
	SpawnableScene sc;
	sc.path = p_path;
	if (Engine::get_singleton()->is_editor_hint()) {
		ERR_FAIL_COND(!FileAccess::exists(p_path));
	}
	spawnable_scenes.push_back(sc);
#ifdef TOOLS_ENABLED
	if (Engine::get_singleton()->is_editor_hint()) {
		return;
	}
#endif
	Node *node = get_spawn_parent();
	if (spawnable_scenes.size() == 1 && node && !node->is_connected("child_entered_tree", callable_mp(this, &SaveloadSpawner::_node_added))) {
		node->connect("child_entered_tree", callable_mp(this, &SaveloadSpawner::_node_added));
	}
}

int SaveloadSpawner::get_spawnable_scene_count() const {
	return spawnable_scenes.size();
}

String SaveloadSpawner::get_spawnable_scene(int p_idx) const {
	ERR_FAIL_INDEX_V(p_idx, (int)spawnable_scenes.size(), "");
	return spawnable_scenes[p_idx].path;
}

void SaveloadSpawner::clear_spawnable_scenes() {
	spawnable_scenes.clear();
#ifdef TOOLS_ENABLED
	if (Engine::get_singleton()->is_editor_hint()) {
		return;
	}
#endif
	Node *node = get_spawn_parent();
	if (node && node->is_connected("child_entered_tree", callable_mp(this, &SaveloadSpawner::_node_added))) {
		node->disconnect("child_entered_tree", callable_mp(this, &SaveloadSpawner::_node_added));
	}
}

Vector<String> SaveloadSpawner::_get_spawnable_scenes() const {
	Vector<String> ss;
	ss.resize(spawnable_scenes.size());
	for (int i = 0; i < ss.size(); i++) {
		ss.write[i] = spawnable_scenes[i].path;
	}
	return ss;
}

void SaveloadSpawner::_set_spawnable_scenes(const Vector<String> &p_scenes) {
	clear_spawnable_scenes();
	for (int i = 0; i < p_scenes.size(); i++) {
		add_spawnable_scene(p_scenes[i]);
	}
}

void SaveloadSpawner::_bind_methods() {
	ClassDB::bind_method(D_METHOD("add_spawnable_scene", "path"), &SaveloadSpawner::add_spawnable_scene);
	ClassDB::bind_method(D_METHOD("get_spawnable_scene_count"), &SaveloadSpawner::get_spawnable_scene_count);
	ClassDB::bind_method(D_METHOD("get_spawnable_scene", "index"), &SaveloadSpawner::get_spawnable_scene);
	ClassDB::bind_method(D_METHOD("clear_spawnable_scenes"), &SaveloadSpawner::clear_spawnable_scenes);

	ClassDB::bind_method(D_METHOD("_get_spawnable_scenes"), &SaveloadSpawner::_get_spawnable_scenes);
	ClassDB::bind_method(D_METHOD("_set_spawnable_scenes", "scenes"), &SaveloadSpawner::_set_spawnable_scenes);

	ADD_PROPERTY(PropertyInfo(Variant::PACKED_STRING_ARRAY, "_spawnable_scenes", PROPERTY_HINT_NONE, "", (PROPERTY_USAGE_STORAGE | PROPERTY_USAGE_INTERNAL)), "_set_spawnable_scenes", "_get_spawnable_scenes");

	ClassDB::bind_method(D_METHOD("spawn", "data"), &SaveloadSpawner::spawn, DEFVAL(Variant()));

	ClassDB::bind_method(D_METHOD("get_spawn_path"), &SaveloadSpawner::get_spawn_path);
	ClassDB::bind_method(D_METHOD("set_spawn_path", "path"), &SaveloadSpawner::set_spawn_path);
	ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "spawn_path", PROPERTY_HINT_NONE, ""), "set_spawn_path", "get_spawn_path");

	ClassDB::bind_method(D_METHOD("get_spawn_limit"), &SaveloadSpawner::get_spawn_limit);
	ClassDB::bind_method(D_METHOD("set_spawn_limit", "limit"), &SaveloadSpawner::set_spawn_limit);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "spawn_limit", PROPERTY_HINT_RANGE, "0,1024,1,or_greater"), "set_spawn_limit", "get_spawn_limit");

	ClassDB::bind_method(D_METHOD("get_spawn_function"), &SaveloadSpawner::get_spawn_function);
	ClassDB::bind_method(D_METHOD("set_spawn_function", "spawn_function"), &SaveloadSpawner::set_spawn_function);
	ADD_PROPERTY(PropertyInfo(Variant::CALLABLE, "spawn_function", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NO_EDITOR), "set_spawn_function", "get_spawn_function");

	ADD_SIGNAL(MethodInfo("despawned", PropertyInfo(Variant::OBJECT, "node", PROPERTY_HINT_RESOURCE_TYPE, "Node")));
	ADD_SIGNAL(MethodInfo("spawned", PropertyInfo(Variant::OBJECT, "node", PROPERTY_HINT_RESOURCE_TYPE, "Node")));
}

void SaveloadSpawner::_update_spawn_parent() {
	//#ifdef TOOLS_ENABLED
	//	if (Engine::get_singleton()->is_editor_hint()) {
	//		return;
	//	}
	//#endif
	if (spawn_parent_id.is_valid()) {
		Node *spawn_parent = Object::cast_to<Node>(ObjectDB::get_instance(spawn_parent_id));
		if (spawn_parent && spawn_parent->is_connected("child_entered_tree", callable_mp(this, &SaveloadSpawner::_node_added))) {
			spawn_parent->disconnect("child_entered_tree", callable_mp(this, &SaveloadSpawner::_node_added));
		}
	}
	Node *spawn_parent = spawn_path.is_empty() && is_inside_tree() ? nullptr : get_node_or_null(spawn_path);
	if (spawn_parent) {
		spawn_parent_id = spawn_parent->get_instance_id();
		if (get_spawnable_scene_count()) {
			spawn_parent->connect("child_entered_tree", callable_mp(this, &SaveloadSpawner::_node_added));
		}
	} else {
		spawn_parent_id = ObjectID();
	}
}

void SaveloadSpawner::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_POST_ENTER_TREE: {
			_update_spawn_parent();
			SaveloadAPI::get_singleton()->track(this);
		} break;

		case NOTIFICATION_EXIT_TREE: {
			_update_spawn_parent();
			LocalVector<SpawnInfo> spawn_infos = spawner_state.spawn_infos;
			uint32_t size = spawner_state.size();
			for (uint32_t i = 0; i < size; ++i) {
				NodePath path = spawn_infos[size - 1 - i].path;
				Node *node = get_node_or_null(path);
				ERR_CONTINUE_MSG(!node, vformat("could not find node at path %s", path));
				node->disconnect(SceneStringNames::get_singleton()->tree_exiting, callable_mp(this, &SaveloadSpawner::_node_exit));
			}
			spawner_state.clear();
			SaveloadAPI::get_singleton()->untrack(this);
		} break;
	}
}

void SaveloadSpawner::_node_added(Node *p_node) {
	if (spawner_state.has(p_node->get_path())) {
		return;
	}
	const Node *parent = get_spawn_parent();
	if (!parent || p_node->get_parent() != parent) {
		return;
	}
	int scene_index = find_spawnable_scene_index_from_path(p_node->get_scene_file_path());
	if (scene_index == CUSTOM_SPAWN) {
		return;
	}
	const String name = p_node->get_name();
	ERR_FAIL_COND_MSG(name.validate_node_name() != name, vformat("Unable to auto-spawn node with reserved name: %s. Make sure to add your saveload scenes via 'add_child(node, true)' to produce valid names.", name));
	_track(p_node, scene_index);
}

NodePath SaveloadSpawner::get_spawn_path() const {
	return spawn_path;
}

void SaveloadSpawner::set_spawn_path(const NodePath &p_path) {
	spawn_path = p_path;
	_update_spawn_parent();
}

void SaveloadSpawner::_track(Node *p_node, int p_scene_index, const Variant &p_spawn_args) {
	NodePath node_path = p_node->get_path();
	if (!spawner_state.has(node_path)) { //TODO: Is this redundant with the checks in _noded_added?
		SpawnInfo spawn_info = SpawnInfo(node_path, p_scene_index, p_spawn_args);
		spawner_state.push_back(spawn_info);
		p_node->connect(SceneStringNames::get_singleton()->tree_exiting, callable_mp(this, &SaveloadSpawner::_node_exit).bind(p_node->get_instance_id()), CONNECT_ONE_SHOT);
		SaveloadAPI::get_singleton()->track(this);
	}
}

void SaveloadSpawner::_node_exit(ObjectID p_id) {
	Node *node = Object::cast_to<Node>(ObjectDB::get_instance(p_id));
	ERR_FAIL_COND_MSG(!node, vformat("could not find a Node at object id %s", p_id));
	spawner_state.erase(node->get_path());
}

int SaveloadSpawner::find_spawnable_scene_index_from_path(const String &p_scene) const {
	for (uint32_t i = 0; i < spawnable_scenes.size(); i++) {
		if (spawnable_scenes[i].path == p_scene) {
			return i;
		}
	}
	return CUSTOM_SPAWN;
}

void SaveloadSpawner::load_spawn_state(const SaveloadSpawner::SpawnerState &p_spawner_state) {
	free_tracked_nodes();
	for (const SpawnInfo &spawn_info : p_spawner_state.spawn_infos) {
		const Vector<StringName> spawn_path_as_vector = spawn_info.path.get_names();
		ERR_CONTINUE_MSG(spawn_path_as_vector.size() < 1, vformat("spawn path %s does not contain a node name", spawn_info.path));
		String spawn_name = spawn_path_as_vector[spawn_path_as_vector.size() - 1];
		_spawn(spawn_name, spawn_info.scene_index, spawn_info.spawn_args); //TODO: what do I do with spawn errors?
	}
}

void SaveloadSpawner::free_tracked_nodes() {
	uint32_t size = spawner_state.size();
	for (uint32_t i = 0; i < size; ++i) {
		SpawnInfo spawn_info = spawner_state.spawn_infos[size - 1 - i];
		Node *node = get_node_or_null(spawn_info.path);
		ERR_CONTINUE_MSG(!node, vformat("could not find a Node at path %s", spawn_info.path));
		Node *parent = node->get_parent();
		if (parent) {
			parent->remove_child(node);
		}
		node->queue_free();
	}
	spawner_state.clear();
}

Error SaveloadSpawner::_spawn(const String &p_name, int p_scene_index, const Variant &p_spawn_args) {
	ERR_FAIL_COND_V_MSG(p_name.validate_node_name() != p_name, ERR_INVALID_DATA, vformat("Invalid node name received: '%s'. Make sure to add nodes via 'add_child(node, true)' remotely.", p_name));

	// Check that we can spawn.
	Node *parent = get_spawn_parent();
	ERR_FAIL_COND_V_MSG(!parent, ERR_UNCONFIGURED, vformat("Failed to get spawn parent for %s", this));
	Node *node = parent->get_node_or_null(p_name);
	if (node) {
		WARN_PRINT_ED(vformat("node named %s is already a child of spawn parent %s", p_name, parent->get_path()));
		_track(node, p_scene_index, p_spawn_args);
		return OK;
	}
	if (p_scene_index == SaveloadSpawner::CUSTOM_SPAWN) {
		// Custom spawn.
		node = instantiate_custom(p_spawn_args);
	} else {
		// Scene based spawn.
		node = instantiate_scene(p_scene_index);
	}
	ERR_FAIL_COND_V_MSG(!node, ERR_UNAUTHORIZED, "spawned node was null");
	node->set_name(p_name);
	parent->add_child(node, true);
	_track(node, p_scene_index, p_spawn_args);

	emit_signal(SNAME("spawned"), node);

	return OK;
}

Node *SaveloadSpawner::spawn(const Variant &p_data) {
	ERR_FAIL_COND_V_MSG(!is_inside_tree(), nullptr, vformat("Spawner %s is not inside the scene tree", this));
	ERR_FAIL_COND_V_MSG(spawn_limit && spawn_limit <= spawner_state.size(), nullptr, "Spawn limit reached!");
	ERR_FAIL_COND_V_MSG(!spawn_function.is_valid(), nullptr, "Custom spawn requires the 'spawn_function' property to be a valid callable.");

	Node *parent = get_spawn_parent();
	ERR_FAIL_COND_V_MSG(!parent, nullptr, "Cannot find spawn node.");

	Node *node = instantiate_custom(p_data);
	ERR_FAIL_COND_V_MSG(!node, nullptr, "The 'spawn_function' callable must return a valid node.");

	_track(node, CUSTOM_SPAWN, p_data);
	parent->add_child(node, true);
	emit_signal(SNAME("spawned"), node);

	return node;
}

Node *SaveloadSpawner::instantiate_scene(int p_id) {
	ERR_FAIL_COND_V_MSG(spawn_limit && spawn_limit <= spawner_state.size(), nullptr, "Spawn limit reached!");
	ERR_FAIL_UNSIGNED_INDEX_V((uint32_t)p_id, spawnable_scenes.size(), nullptr);
	SpawnableScene &sc = spawnable_scenes[p_id];
	if (sc.cache.is_null()) {
		sc.cache = ResourceLoader::load(sc.path);
	}
	ERR_FAIL_COND_V_MSG(sc.cache.is_null(), nullptr, "Invalid spawnable scene: " + sc.path);
	return sc.cache->instantiate();
}

Node *SaveloadSpawner::instantiate_custom(const Variant &p_data) {
	ERR_FAIL_COND_V_MSG(spawn_limit && spawn_limit <= spawner_state.size(), nullptr, "Spawn limit reached!");
	ERR_FAIL_COND_V_MSG(!spawn_function.is_valid(), nullptr, "Custom spawn requires a valid 'spawn_function'.");
	const Variant *argv[1] = { &p_data };
	Variant ret;
	Callable::CallError ce;
	spawn_function.callp(argv, 1, ret, ce);
	ERR_FAIL_COND_V_MSG(ce.error != Callable::CallError::CALL_OK, nullptr, "Failed to call spawn function.");
	ERR_FAIL_COND_V_MSG(ret.get_type() != Variant::OBJECT, nullptr, "The spawn function must return a Node.");
	return Object::cast_to<Node>(ret.operator Object *());
}

/***********************************
 * SaveloadSpawner Definitions End *
 ***********************************/

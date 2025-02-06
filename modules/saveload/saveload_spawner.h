/**************************************************************************/
/*  saveload_spawner.h                                                    */
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

#ifndef SAVELOAD_SPAWNER_H
#define SAVELOAD_SPAWNER_H

#include "scene/main/node.h"
#include "scene/resources/packed_scene.h"

#include "scene_saveload_config.h"

class SceneSaveloadInterface;

class SaveloadSpawner : public Node {
	GDCLASS(SaveloadSpawner, Node);

public:
	enum {
		CUSTOM_SPAWN = 0xFF,
	};

	struct SpawnInfo {
		NodePath path;
		int scene_index;
		Variant spawn_args;

		Dictionary to_dict() const;

		SpawnInfo(NodePath p_path, int p_scene_index, Variant spawn_args);
		SpawnInfo(const Dictionary &p_dict);
		SpawnInfo() {}
	};

	struct SpawnerState {
		HashMap<const NodePath, int> tracked_paths;
		LocalVector<SpawnInfo> spawn_infos;

		_FORCE_INLINE_ uint32_t size() const { return spawn_infos.size(); }

		void push_back(SpawnInfo p_spawn_info) {
			spawn_infos.push_back(p_spawn_info);
			tracked_paths.insert(p_spawn_info.path, spawn_infos.size() - 1);
		}
		bool has(const NodePath &p_path) const;
		bool erase(const NodePath &p_path);
		void clear();

		TypedArray<Dictionary> to_array() const;

		SpawnerState(const TypedArray<Dictionary> &p_array);
		SpawnerState() {}
	};

private:
	struct SpawnableScene {
		String path;
		Ref<PackedScene> cache;
	};

	LocalVector<SpawnableScene> spawnable_scenes;

	HashSet<ResourceUID::ID> spawnable_ids;
	NodePath spawn_path;

	ObjectID spawn_parent_id;
	SpawnerState spawner_state;
	uint32_t spawn_limit = 0;
	Callable spawn_function;

	void _update_spawn_parent();
	Error _spawn(const String &p_name, int p_scene_index, const Variant &p_spawn_args = Variant());
	void _track(Node *p_node, int p_scene_index, const Variant &p_spawn_args = Variant());
	void _node_added(Node *p_node);
	void _node_exit(ObjectID p_id);

	Vector<String> _get_spawnable_scenes() const;
	void _set_spawnable_scenes(const Vector<String> &p_scenes);

protected:
	static void _bind_methods();
	void _notification(int p_what);

#ifdef TOOLS_ENABLED
	bool _set(const StringName &p_name, const Variant &p_value);
	bool _get(const StringName &p_name, Variant &r_ret) const;
	void _get_property_list(List<PropertyInfo> *p_list) const;
#endif
public:
	PackedStringArray get_configuration_warnings() const override;

	Node *get_spawn_parent() const {
		return spawn_parent_id.is_valid() ? Object::cast_to<Node>(ObjectDB::get_instance(spawn_parent_id)) : nullptr;
	}

	SpawnerState get_spawner_state() const { return spawner_state; }

	void add_spawnable_scene(const String &p_path);
	int get_spawnable_scene_count() const;
	String get_spawnable_scene(int p_idx) const;
	void clear_spawnable_scenes();

	NodePath get_spawn_path() const;
	void set_spawn_path(const NodePath &p_path);
	uint32_t get_spawn_limit() const { return spawn_limit; }
	void set_spawn_limit(uint32_t p_limit) { spawn_limit = p_limit; }
	void set_spawn_function(Callable p_spawn_function) { spawn_function = p_spawn_function; }
	Callable get_spawn_function() const { return spawn_function; }

	int find_spawnable_scene_index_from_path(const String &p_path) const;
	void load_spawn_state(const SpawnerState &p_spawner_state);
	void free_tracked_nodes();
	Node *spawn(const Variant &p_data = Variant());
	Node *instantiate_custom(const Variant &p_data);
	Node *instantiate_scene(int p_idx);

	SaveloadSpawner() {}
};

#endif // SAVELOAD_SPAWNER_H

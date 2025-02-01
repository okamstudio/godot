/**************************************************************************/
/*  multiplayer_spawner.h                                                 */
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

#include "core/templates/local_vector.h"
#include "scene/main/node.h"
#include "scene/resources/packed_scene.h"

class MultiplayerSpawner : public Node {
	GDCLASS(MultiplayerSpawner, Node);

public:
	enum {
		INVALID_ID = 0xFF,
	};

private:
	struct SpawnableScene {
		String path;
		Ref<PackedScene> cache;
	};

	LocalVector<SpawnableScene> spawnable_scenes;

	HashSet<ResourceUID::ID> spawnable_ids;
	NodePath spawn_path;

	struct SpawnInfo {
		Variant args;
		int id = INVALID_ID;
		SpawnInfo(Variant p_args, int p_id) {
			id = p_id;
			args = p_args;
		}
		SpawnInfo() {}
	};

	ObjectID spawn_node;
	HashMap<ObjectID, SpawnInfo> tracked_nodes;
	uint32_t spawn_limit = 0;
	Callable spawn_function;

	void _update_spawn_node();
	void _track(Node *p_node, const Variant &p_argument, int p_scene_id = INVALID_ID);
	void _node_added(Node *p_node);
	void _node_exit(ObjectID p_id);
	void _spawn_notify(ObjectID p_id);

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

	Node *get_spawn_node() const {
		return spawn_node.is_valid() ? Object::cast_to<Node>(ObjectDB::get_instance(spawn_node)) : nullptr;
	}

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

	const Variant get_spawn_argument(const ObjectID &p_id) const;
	int find_spawnable_scene_index_from_object(const ObjectID &p_id) const;
	int find_spawnable_scene_index_from_path(const String &p_path) const;
	Node *spawn(const Variant &p_data = Variant());
	Node *instantiate_custom(const Variant &p_data);
	Node *instantiate_scene(int p_idx);

	MultiplayerSpawner() {}
};

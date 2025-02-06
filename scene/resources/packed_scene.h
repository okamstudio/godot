/**************************************************************************/
/*  packed_scene.h                                                        */
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

#ifndef PACKED_SCENE_H
#define PACKED_SCENE_H

#include "core/io/resource.h"
#include "scene/main/node.h"

class SceneState : public RefCounted {
	GDCLASS(SceneState, RefCounted);

	Vector<StringName> names;
	Vector<Variant> variants;
	Vector<NodePath> node_paths;
	Vector<NodePath> exposed_nodes;
	Vector<NodePath> editable_instances;
	mutable HashMap<NodePath, int> node_path_cache;
	mutable HashMap<int, int> base_scene_node_remap;

	int base_scene_idx = -1;

	enum {
		NO_PARENT_SAVED = 0x7FFFFFFF,
		NAME_INDEX_BITS = 18,
		NAME_MASK = (1 << NAME_INDEX_BITS) - 1,
	};

	struct NodeData {
		int parent = 0;
		int owner = 0;
		int type = 0;
		int name = 0;
		int instance = 0;
		int index = 0;

		struct Property {
			int name = 0;
			int value = 0;
		};

		Vector<Property> properties;
		Vector<int> groups;
	};

	struct DeferredNodePathProperties {
		Node *base = nullptr;
		StringName property;
		Variant value;
	};

	Vector<NodeData> nodes;

	struct ConnectionData {
		int from = 0;
		int to = 0;
		int signal = 0;
		int method = 0;
		int flags = 0;
		int unbinds = 0;
		Vector<int> binds;
	};

	Vector<ConnectionData> connections;

	Error _parse_node(Node *p_owner, Node *p_node, int p_parent_idx, HashMap<StringName, int> &name_map, HashMap<Variant, int, VariantHasher, VariantComparator> &variant_map, HashMap<Node *, int> &node_map, HashMap<Node *, int> &nodepath_map);
	Error _parse_connections(Node *p_owner, Node *p_node, HashMap<StringName, int> &name_map, HashMap<Variant, int, VariantHasher, VariantComparator> &variant_map, HashMap<Node *, int> &node_map, HashMap<Node *, int> &nodepath_map);

	String path;

	uint64_t last_modified_time = 0;

	static bool disable_placeholders;

	Vector<String> _get_node_groups(int p_idx) const;

	int _find_base_scene_node_remap_key(int p_idx) const;

#ifdef TOOLS_ENABLED
public:
	typedef void (*InstantiationWarningNotify)(const String &p_warning);

private:
	static InstantiationWarningNotify instantiation_warn_notify;
#endif

protected:
	static void _bind_methods();

public:
	enum {
		FLAG_ID_IS_PATH = (1 << 30),
		TYPE_INSTANTIATED = 0x7FFFFFFF,
		FLAG_INSTANCE_IS_PLACEHOLDER = (1 << 30),
		FLAG_PATH_PROPERTY_IS_NODE = (1 << 30),
		FLAG_PROP_NAME_MASK = FLAG_PATH_PROPERTY_IS_NODE - 1,
		FLAG_MASK = (1 << 24) - 1,
	};

	enum GenEditState {
		GEN_EDIT_STATE_DISABLED,
		GEN_EDIT_STATE_INSTANCE,
		GEN_EDIT_STATE_MAIN,
		GEN_EDIT_STATE_MAIN_INHERITED,
	};

	struct PackState {
		Ref<SceneState> state;
		int node = -1;
	};

	static void set_disable_placeholders(bool p_disable);
	static Ref<Resource> get_remap_resource(const Ref<Resource> &p_resource, HashMap<Ref<Resource>, Ref<Resource>> &remap_cache, const Ref<Resource> &p_fallback, Node *p_for_scene);

	int find_node_by_path(const NodePath &p_node) const;
	Variant get_property_value(int p_node, const StringName &p_property, bool &r_found, bool &r_node_deferred) const;
	bool is_node_in_group(int p_node, const StringName &p_group) const;
	bool is_connection(int p_node, const StringName &p_signal, int p_to_node, const StringName &p_to_method) const;

	void set_bundled_scene(const Dictionary &p_dictionary);
	Dictionary get_bundled_scene() const;

	Error pack(Node *p_scene);

	void set_path(const String &p_path);
	String get_path() const;

	void clear();
	Error copy_from(const Ref<SceneState> &p_scene_state);

	bool can_instantiate() const;
	Node *instantiate(GenEditState p_edit_state) const;

	Array setup_resources_in_array(Array &array_to_scan, const SceneState::NodeData &n, HashMap<Ref<Resource>, Ref<Resource>> &resources_local_to_sub_scene, Node *node, const StringName sname, HashMap<Ref<Resource>, Ref<Resource>> &resources_local_to_scene, int i, Node **ret_nodes, SceneState::GenEditState p_edit_state) const;
	Dictionary setup_resources_in_dictionary(Dictionary &p_dictionary_to_scan, const SceneState::NodeData &p_n, HashMap<Ref<Resource>, Ref<Resource>> &p_resources_local_to_sub_scene, Node *p_node, const StringName p_sname, HashMap<Ref<Resource>, Ref<Resource>> &p_resources_local_to_scene, int p_i, Node **p_ret_nodes, SceneState::GenEditState p_edit_state) const;
	Variant make_local_resource(Variant &value, const SceneState::NodeData &p_node_data, HashMap<Ref<Resource>, Ref<Resource>> &p_resources_local_to_sub_scene, Node *p_node, const StringName p_sname, HashMap<Ref<Resource>, Ref<Resource>> &p_resources_local_to_scene, int p_i, Node **p_ret_nodes, SceneState::GenEditState p_edit_state) const;
	bool has_local_resource(const Array &p_array) const;

	Ref<SceneState> get_base_scene_state() const;

	void update_instance_resource(String p_path, Ref<PackedScene> p_packed_scene);

	//unbuild API

	int get_node_count() const;
	StringName get_node_type(int p_idx) const;
	StringName get_node_name(int p_idx) const;
	NodePath get_node_path(int p_idx, bool p_for_parent = false) const;
	NodePath get_node_owner_path(int p_idx) const;
	Ref<PackedScene> get_node_instance(int p_idx) const;
	String get_node_instance_placeholder(int p_idx) const;
	bool is_node_instance_placeholder(int p_idx) const;
	Vector<StringName> get_node_groups(int p_idx) const;
	int get_node_index(int p_idx) const;

	int get_node_property_count(int p_idx) const;
	StringName get_node_property_name(int p_idx, int p_prop) const;
	Variant get_node_property_value(int p_idx, int p_prop) const;
	Vector<String> get_node_deferred_nodepath_properties(int p_idx) const;

	int get_connection_count() const;
	NodePath get_connection_source(int p_idx) const;
	StringName get_connection_signal(int p_idx) const;
	NodePath get_connection_target(int p_idx) const;
	StringName get_connection_method(int p_idx) const;
	int get_connection_flags(int p_idx) const;
	int get_connection_unbinds(int p_idx) const;
	Array get_connection_binds(int p_idx) const;

	bool has_connection(const NodePath &p_node_from, const StringName &p_signal, const NodePath &p_node_to, const StringName &p_method, bool p_no_inheritance = false);

	Vector<NodePath> get_editable_instances() const;
	Vector<NodePath> get_exposed_nodes() const;
	Ref<Resource> get_sub_resource(const String &p_path);

	//build API

	int add_name(const StringName &p_name);
	int add_value(const Variant &p_value);
	int add_node_path(const NodePath &p_path);
	int add_node(int p_parent, int p_owner, int p_type, int p_name, int p_instance, int p_index);
	void add_node_property(int p_node, int p_name, int p_value, bool p_deferred_node_path = false);
	void add_node_group(int p_node, int p_group);
	void set_base_scene(int p_idx);
	void add_connection(int p_from, int p_to, int p_signal, int p_method, int p_flags, int p_unbinds, const Vector<int> &p_binds);
	void add_editable_instance(const NodePath &p_path);
	void add_exposed_node(const NodePath &p_path);

	bool remove_group_references(const StringName &p_name);
	bool rename_group_references(const StringName &p_old_name, const StringName &p_new_name);
	HashSet<StringName> get_all_groups();

	virtual void set_last_modified_time(uint64_t p_time) { last_modified_time = p_time; }
	uint64_t get_last_modified_time() const { return last_modified_time; }

	// Used when saving pointers (saves a path property instead).
	static String get_meta_pointer_property(const String &p_property);

#ifdef TOOLS_ENABLED
	static void set_instantiation_warning_notify_func(InstantiationWarningNotify p_warn_notify) { instantiation_warn_notify = p_warn_notify; }
#endif

	SceneState();
};

VARIANT_ENUM_CAST(SceneState::GenEditState)

class PackedScene : public Resource {
	GDCLASS(PackedScene, Resource);
	RES_BASE_EXTENSION("scn");

	Ref<SceneState> state;

	void _set_bundled_scene(const Dictionary &p_scene);
	Dictionary _get_bundled_scene() const;

protected:
	virtual bool editor_can_reload_from_file() override { return false; } // this is handled by editor better
	static void _bind_methods();
	virtual void reset_state() override;

public:
	enum GenEditState {
		GEN_EDIT_STATE_DISABLED,
		GEN_EDIT_STATE_INSTANCE,
		GEN_EDIT_STATE_MAIN,
		GEN_EDIT_STATE_MAIN_INHERITED,
	};

	Error pack(Node *p_scene);

	void clear();

	bool can_instantiate() const;
	Node *instantiate(GenEditState p_edit_state = GEN_EDIT_STATE_DISABLED) const;

	void recreate_state();
	void replace_state(Ref<SceneState> p_by);

	virtual void reload_from_file() override;

	virtual void set_path(const String &p_path, bool p_take_over = false) override;
	virtual void set_path_cache(const String &p_path) override;

#ifdef TOOLS_ENABLED
	virtual void set_last_modified_time(uint64_t p_time) override {
		Resource::set_last_modified_time(p_time);
		state->set_last_modified_time(p_time);
	}

	static HashSet<StringName> get_scene_groups(const String &p_path);
#endif
	Ref<SceneState> get_state() const;

	PackedScene();
};

VARIANT_ENUM_CAST(PackedScene::GenEditState)

#endif // PACKED_SCENE_H

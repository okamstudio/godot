/**************************************************************************/
/*  node.h                                                                */
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

#ifndef NODE_H
#define NODE_H

#include "core/string/node_path.h"
#include "core/variant/typed_array.h"
#include "scene/main/scene_tree.h"
#include "scene/scene_string_names.h"

class Viewport;
class Window;
class SceneState;
class Tween;
class PropertyTweener;

SAFE_FLAG_TYPE_PUN_GUARANTEES
SAFE_NUMERIC_TYPE_PUN_GUARANTEES(uint32_t)

class Node : public Object {
	GDCLASS(Node, Object);

protected:
	// During group processing, these are thread-safe.
	// Outside group processing, these avoid the cost of sync by working as plain primitive types.
	union MTFlag {
		SafeFlag mt;
		bool st;
		MTFlag() :
				mt{} {}
	};
	template <typename T>
	union MTNumeric {
		SafeNumeric<T> mt;
		T st;
		MTNumeric() :
				mt{} {}
	};

public:
	// N.B. Any enum stored as a bitfield should be specified as UNSIGNED to work around
	// some compilers trying to store it as signed, and requiring 1 more bit than necessary.
	enum ProcessMode : unsigned int {
		PROCESS_MODE_INHERIT, // same as parent node
		PROCESS_MODE_PAUSABLE, // process only if not paused
		PROCESS_MODE_WHEN_PAUSED, // process only if paused
		PROCESS_MODE_ALWAYS, // process always
		PROCESS_MODE_DISABLED, // never process
	};

	enum ProcessThreadGroup {
		PROCESS_THREAD_GROUP_INHERIT,
		PROCESS_THREAD_GROUP_MAIN_THREAD,
		PROCESS_THREAD_GROUP_SUB_THREAD,
	};

	enum ProcessThreadMessages {
		FLAG_PROCESS_THREAD_MESSAGES = 1,
		FLAG_PROCESS_THREAD_MESSAGES_PHYSICS = 2,
		FLAG_PROCESS_THREAD_MESSAGES_ALL = 3,
	};

	enum PhysicsInterpolationMode : unsigned int {
		PHYSICS_INTERPOLATION_MODE_INHERIT,
		PHYSICS_INTERPOLATION_MODE_ON,
		PHYSICS_INTERPOLATION_MODE_OFF,
	};

	enum DuplicateFlags {
		DUPLICATE_SIGNALS = 1,
		DUPLICATE_GROUPS = 2,
		DUPLICATE_SCRIPTS = 4,
		DUPLICATE_USE_INSTANTIATION = 8,
#ifdef TOOLS_ENABLED
		DUPLICATE_FROM_EDITOR = 16,
#endif
	};

	enum NameCasing {
		NAME_CASING_PASCAL_CASE,
		NAME_CASING_CAMEL_CASE,
		NAME_CASING_SNAKE_CASE
	};

	enum InternalMode {
		INTERNAL_MODE_DISABLED,
		INTERNAL_MODE_FRONT,
		INTERNAL_MODE_BACK,
	};

	enum AutoTranslateMode {
		AUTO_TRANSLATE_MODE_INHERIT,
		AUTO_TRANSLATE_MODE_ALWAYS,
		AUTO_TRANSLATE_MODE_DISABLED,
	};

	struct Comparator {
		bool operator()(const Node *p_a, const Node *p_b) const { return p_b->is_greater_than(p_a); }
	};

	static int orphan_node_count;

	void _update_process(bool p_enable, bool p_for_children);

private:
	struct GroupData {
		bool persistent = false;
		SceneTree::Group *group = nullptr;
	};

	struct ComparatorByIndex {
		bool operator()(const Node *p_left, const Node *p_right) const {
			static const uint32_t order[3] = { 1, 0, 2 };
			uint32_t order_left = order[p_left->data.internal_mode];
			uint32_t order_right = order[p_right->data.internal_mode];
			if (order_left == order_right) {
				return p_left->data.index < p_right->data.index;
			}
			return order_left < order_right;
		}
	};

	struct ComparatorWithPriority {
		bool operator()(const Node *p_a, const Node *p_b) const { return p_b->data.process_priority == p_a->data.process_priority ? p_b->is_greater_than(p_a) : p_b->data.process_priority > p_a->data.process_priority; }
	};

	struct ComparatorWithPhysicsPriority {
		bool operator()(const Node *p_a, const Node *p_b) const { return p_b->data.physics_process_priority == p_a->data.physics_process_priority ? p_b->is_greater_than(p_a) : p_b->data.physics_process_priority > p_a->data.physics_process_priority; }
	};

	// This Data struct is to avoid namespace pollution in derived classes.
	struct Data {
		String scene_file_path;
		Ref<SceneState> instance_state;
		Ref<SceneState> inherited_state;

		Node *parent = nullptr;
		Node *owner = nullptr;
		HashMap<StringName, Node *> children;
		mutable bool children_cache_dirty = true;
		mutable LocalVector<Node *> children_cache;
		HashMap<StringName, Node *> owned_unique_nodes;
		bool unique_name_in_owner = false;
		InternalMode internal_mode = INTERNAL_MODE_DISABLED;
		mutable int internal_children_front_count_cache = 0;
		mutable int internal_children_back_count_cache = 0;
		mutable int external_children_count_cache = 0;
		mutable int index = -1; // relative to front, normal or back.
		int depth = -1;
		int blocked = 0; // Safeguard that throws an error when attempting to modify the tree in a harmful way while being traversed.
		StringName name;
		SceneTree *tree = nullptr;

#ifdef TOOLS_ENABLED
		NodePath import_path; // Path used when imported, used by scene editors to keep tracking.
#endif
		String editor_description;

		Viewport *viewport = nullptr;

		HashMap<StringName, GroupData> grouped;
		List<Node *>::Element *OW = nullptr; // Owned element.
		List<Node *> owned;

		Node *process_owner = nullptr;
		ProcessThreadGroup process_thread_group = PROCESS_THREAD_GROUP_INHERIT;
		Node *process_thread_group_owner = nullptr;
		int process_thread_group_order = 0;
		BitField<ProcessThreadMessages> process_thread_messages;
		void *process_group = nullptr; // to avoid cyclic dependency

		int multiplayer_authority = 1; // Server by default.
		Variant rpc_config = Dictionary();

		// Variables used to properly sort the node when processing, ignored otherwise.
		int process_priority = 0;
		int physics_process_priority = 0;

		// Keep bitpacked values together to get better packing.
		ProcessMode process_mode : 3;
		PhysicsInterpolationMode physics_interpolation_mode : 2;

		bool physics_process : 1;
		bool process : 1;

		bool physics_process_internal : 1;
		bool process_internal : 1;

		bool input : 1;
		bool shortcut_input : 1;
		bool unhandled_input : 1;
		bool unhandled_key_input : 1;

		// Physics interpolation can be turned on and off on a per node basis.
		// This only takes effect when the SceneTree (or project setting) physics interpolation
		// is switched on.
		bool physics_interpolated : 1;

		// We can auto-reset physics interpolation when e.g. adding a node for the first time.
		bool physics_interpolation_reset_requested : 1;

		// Most nodes need not be interpolated in the scene tree, physics interpolation
		// is normally only needed in the RenderingServer. However if we need to read the
		// interpolated transform of a node in the SceneTree, it is necessary to duplicate
		// the interpolation logic client side, in order to prevent stalling the RenderingServer
		// by reading back.
		bool physics_interpolated_client_side : 1;

		// For certain nodes (e.g. CPU particles in global mode)
		// it can be useful to not send the instance transform to the
		// RenderingServer, and specify the mesh in world space.
		bool use_identity_transform : 1;

		bool parent_owned : 1;
		bool in_constructor : 1;
		bool use_placeholder : 1;

		bool display_folded : 1;
		bool editable_instance : 1;

		bool inside_tree : 1;
		bool ready_notified : 1;
		bool ready_first : 1;

		AutoTranslateMode auto_translate_mode = AUTO_TRANSLATE_MODE_INHERIT;
		mutable bool is_auto_translating = true;
		mutable bool is_auto_translate_dirty = true;

		mutable bool is_translation_domain_inherited = true;
		mutable bool is_translation_domain_dirty = true;

		mutable NodePath *path_cache = nullptr;

	} data;

	Ref<MultiplayerAPI> multiplayer;

	String _get_tree_string_pretty(const String &p_prefix, bool p_last);
	String _get_tree_string(const Node *p_node);

	Node *_get_child_by_name(const StringName &p_name) const;

	void _replace_connections_target(Node *p_new_target);

	void _validate_child_name(Node *p_child, bool p_force_human_readable = false);
	void _generate_serial_child_name(const Node *p_child, StringName &name) const;

	void _propagate_reverse_notification(int p_notification);
	void _propagate_deferred_notification(int p_notification, bool p_reverse);
	void _propagate_enter_tree();
	void _propagate_ready();
	void _propagate_exit_tree();
	void _propagate_after_exit_tree();
	void _propagate_physics_interpolated(bool p_interpolated);
	void _propagate_physics_interpolation_reset_requested(bool p_requested);
	void _propagate_process_owner(Node *p_owner, int p_pause_notification, int p_enabled_notification);
	void _propagate_groups_dirty();
	void _propagate_translation_domain_dirty();
	Array _get_node_and_resource(const NodePath &p_path);

	void _duplicate_properties(const Node *p_root, const Node *p_original, Node *p_copy, int p_flags) const;
	void _duplicate_signals(const Node *p_original, Node *p_copy) const;
	Node *_duplicate(int p_flags, HashMap<const Node *, Node *> *r_duplimap = nullptr) const;

	TypedArray<StringName> _get_groups() const;

	Error _rpc_bind(const Variant **p_args, int p_argcount, Callable::CallError &r_error);
	Error _rpc_id_bind(const Variant **p_args, int p_argcount, Callable::CallError &r_error);

	friend class SceneTree;

	void _set_tree(SceneTree *p_tree);
	void _propagate_pause_notification(bool p_enable);
	void _propagate_suspend_notification(bool p_enable);

	_FORCE_INLINE_ bool _can_process(bool p_paused) const;
	_FORCE_INLINE_ bool _is_enabled() const;

	void _release_unique_name_in_owner();
	void _acquire_unique_name_in_owner();

	void _clean_up_owner();

	_FORCE_INLINE_ void _update_children_cache() const {
		if (unlikely(data.children_cache_dirty)) {
			_update_children_cache_impl();
		}
	}

	void _update_children_cache_impl() const;

	// Process group management
	void _add_process_group();
	void _remove_process_group();
	void _add_to_process_thread_group();
	void _remove_from_process_thread_group();
	void _remove_tree_from_process_thread_group();
	void _add_tree_to_process_thread_group(Node *p_owner);

	static thread_local Node *current_process_thread_group;

	Variant _call_deferred_thread_group_bind(const Variant **p_args, int p_argcount, Callable::CallError &r_error);
	Variant _call_thread_safe_bind(const Variant **p_args, int p_argcount, Callable::CallError &r_error);

	// Editor only signal to keep the SceneTreeEditor in sync.
#ifdef TOOLS_ENABLED
	void _emit_editor_state_changed();
#else
	void _emit_editor_state_changed() {}
#endif

protected:
	void _block() { data.blocked++; }
	void _unblock() { data.blocked--; }

	void _notification(int p_notification);

	virtual void _physics_interpolated_changed();

	virtual void add_child_notify(Node *p_child);
	virtual void remove_child_notify(Node *p_child);
	virtual void move_child_notify(Node *p_child);
	virtual void owner_changed_notify();

	void _propagate_replace_owner(Node *p_owner, Node *p_by_owner);

	static void _bind_methods();
	static String _get_name_num_separator();

	friend class SceneState;

	void _add_child_nocheck(Node *p_child, const StringName &p_name, InternalMode p_internal_mode = INTERNAL_MODE_DISABLED);
	void _set_owner_nocheck(Node *p_owner);
	void _set_name_nocheck(const StringName &p_name);

	void _set_physics_interpolated_client_side(bool p_enable) { data.physics_interpolated_client_side = p_enable; }
	bool _is_physics_interpolated_client_side() const { return data.physics_interpolated_client_side; }

	void _set_physics_interpolation_reset_requested(bool p_enable) { data.physics_interpolation_reset_requested = p_enable; }
	bool _is_physics_interpolation_reset_requested() const { return data.physics_interpolation_reset_requested; }

	void _set_use_identity_transform(bool p_enable) { data.use_identity_transform = p_enable; }
	bool _is_using_identity_transform() const { return data.use_identity_transform; }

	//call from SceneTree
	void _call_input(const Ref<InputEvent> &p_event);
	void _call_shortcut_input(const Ref<InputEvent> &p_event);
	void _call_unhandled_input(const Ref<InputEvent> &p_event);
	void _call_unhandled_key_input(const Ref<InputEvent> &p_event);

	void _validate_property(PropertyInfo &p_property) const;

protected:
	virtual void input(const Ref<InputEvent> &p_event);
	virtual void shortcut_input(const Ref<InputEvent> &p_key_event);
	virtual void unhandled_input(const Ref<InputEvent> &p_event);
	virtual void unhandled_key_input(const Ref<InputEvent> &p_key_event);

	GDVIRTUAL1(_process, double)
	GDVIRTUAL1(_physics_process, double)
	GDVIRTUAL0(_enter_tree)
	GDVIRTUAL0(_exit_tree)
	GDVIRTUAL0(_ready)
	GDVIRTUAL0RC(Vector<String>, _get_configuration_warnings)

	GDVIRTUAL1(_input, Ref<InputEvent>)
	GDVIRTUAL1(_shortcut_input, Ref<InputEvent>)
	GDVIRTUAL1(_unhandled_input, Ref<InputEvent>)
	GDVIRTUAL1(_unhandled_key_input, Ref<InputEvent>)

public:
	enum {
		// You can make your own, but don't use the same numbers as other notifications in other nodes.
		NOTIFICATION_ENTER_TREE = 10,
		NOTIFICATION_EXIT_TREE = 11,
		NOTIFICATION_MOVED_IN_PARENT = 12,
		NOTIFICATION_READY = 13,
		NOTIFICATION_PAUSED = 14,
		NOTIFICATION_UNPAUSED = 15,
		NOTIFICATION_PHYSICS_PROCESS = 16,
		NOTIFICATION_PROCESS = 17,
		NOTIFICATION_PARENTED = 18,
		NOTIFICATION_UNPARENTED = 19,
		NOTIFICATION_SCENE_INSTANTIATED = 20,
		NOTIFICATION_DRAG_BEGIN = 21,
		NOTIFICATION_DRAG_END = 22,
		NOTIFICATION_PATH_RENAMED = 23,
		NOTIFICATION_CHILD_ORDER_CHANGED = 24,
		NOTIFICATION_INTERNAL_PROCESS = 25,
		NOTIFICATION_INTERNAL_PHYSICS_PROCESS = 26,
		NOTIFICATION_POST_ENTER_TREE = 27,
		NOTIFICATION_DISABLED = 28,
		NOTIFICATION_ENABLED = 29,
		NOTIFICATION_RESET_PHYSICS_INTERPOLATION = 2001, // A GodotSpace Odyssey.
		// Keep these linked to Node.
		NOTIFICATION_WM_MOUSE_ENTER = 1002,
		NOTIFICATION_WM_MOUSE_EXIT = 1003,
		NOTIFICATION_WM_WINDOW_FOCUS_IN = 1004,
		NOTIFICATION_WM_WINDOW_FOCUS_OUT = 1005,
		NOTIFICATION_WM_CLOSE_REQUEST = 1006,
		NOTIFICATION_WM_GO_BACK_REQUEST = 1007,
		NOTIFICATION_WM_SIZE_CHANGED = 1008,
		NOTIFICATION_WM_DPI_CHANGE = 1009,
		NOTIFICATION_VP_MOUSE_ENTER = 1010,
		NOTIFICATION_VP_MOUSE_EXIT = 1011,
		NOTIFICATION_WM_POSITION_CHANGED = 1012,

		NOTIFICATION_OS_MEMORY_WARNING = MainLoop::NOTIFICATION_OS_MEMORY_WARNING,
		NOTIFICATION_TRANSLATION_CHANGED = MainLoop::NOTIFICATION_TRANSLATION_CHANGED,
		NOTIFICATION_WM_ABOUT = MainLoop::NOTIFICATION_WM_ABOUT,
		NOTIFICATION_CRASH = MainLoop::NOTIFICATION_CRASH,
		NOTIFICATION_OS_IME_UPDATE = MainLoop::NOTIFICATION_OS_IME_UPDATE,
		NOTIFICATION_APPLICATION_RESUMED = MainLoop::NOTIFICATION_APPLICATION_RESUMED,
		NOTIFICATION_APPLICATION_PAUSED = MainLoop::NOTIFICATION_APPLICATION_PAUSED,
		NOTIFICATION_APPLICATION_FOCUS_IN = MainLoop::NOTIFICATION_APPLICATION_FOCUS_IN,
		NOTIFICATION_APPLICATION_FOCUS_OUT = MainLoop::NOTIFICATION_APPLICATION_FOCUS_OUT,
		NOTIFICATION_TEXT_SERVER_CHANGED = MainLoop::NOTIFICATION_TEXT_SERVER_CHANGED,

		// Editor specific node notifications
		NOTIFICATION_EDITOR_PRE_SAVE = 9001,
		NOTIFICATION_EDITOR_POST_SAVE = 9002,
		NOTIFICATION_SUSPENDED = 9003,
		NOTIFICATION_UNSUSPENDED = 9004
	};

	/* NODE/TREE */

	StringName get_name() const;
	String get_description() const;
	void set_name(const String &p_name);

	InternalMode get_internal_mode() const;

	void add_child(Node *p_child, bool p_force_readable_name = false, InternalMode p_internal = INTERNAL_MODE_DISABLED);
	void add_sibling(Node *p_sibling, bool p_force_readable_name = false);
	void remove_child(Node *p_child);

	int get_child_count(bool p_include_internal = true) const;
	Node *get_child(int p_index, bool p_include_internal = true) const;
	TypedArray<Node> get_children(bool p_include_internal = true) const;
	bool has_node(const NodePath &p_path) const;
	Node *get_node(const NodePath &p_path) const;
	Node *get_node_or_null(const NodePath &p_path) const;
	Node *find_child(const String &p_pattern, bool p_recursive = true, bool p_owned = true) const;
	TypedArray<Node> find_children(const String &p_pattern, const String &p_type = "", bool p_recursive = true, bool p_owned = true) const;
	bool has_node_and_resource(const NodePath &p_path) const;
	Node *get_node_and_resource(const NodePath &p_path, Ref<Resource> &r_res, Vector<StringName> &r_leftover_subpath, bool p_last_is_property = true) const;

	virtual void reparent(Node *p_parent, bool p_keep_global_transform = true);
	Node *get_parent() const;
	Node *find_parent(const String &p_pattern) const;

	Window *get_window() const;
	Window *get_last_exclusive_window() const;

	_FORCE_INLINE_ SceneTree *get_tree() const {
		ERR_FAIL_NULL_V(data.tree, nullptr);
		return data.tree;
	}

	_FORCE_INLINE_ bool is_inside_tree() const { return data.inside_tree; }

	bool is_ancestor_of(const Node *p_node) const;
	bool is_greater_than(const Node *p_node) const;

	NodePath get_path() const;
	NodePath get_path_to(const Node *p_node, bool p_use_unique_path = false) const;
	Node *find_common_parent_with(const Node *p_node) const;

	void add_to_group(const StringName &p_identifier, bool p_persistent = false);
	void remove_from_group(const StringName &p_identifier);
	bool is_in_group(const StringName &p_identifier) const;

	struct GroupInfo {
		StringName name;
		bool persistent = false;
	};

	void get_groups(List<GroupInfo> *p_groups) const;
	int get_persistent_group_count() const;

	void move_child(Node *p_child, int p_index);
	void _move_child(Node *p_child, int p_index, bool p_ignore_end = false);

	void set_owner(Node *p_owner);
	Node *get_owner() const;
	void get_owned_by(Node *p_by, List<Node *> *p_owned);

	void set_unique_name_in_owner(bool p_enabled);
	bool is_unique_name_in_owner() const;

	_FORCE_INLINE_ int get_index(bool p_include_internal = true) const {
		// p_include_internal = false doesn't make sense if the node is internal.
		ERR_FAIL_COND_V_MSG(!p_include_internal && data.internal_mode != INTERNAL_MODE_DISABLED, -1, "Node is internal. Can't get index with 'include_internal' being false.");
		if (!data.parent) {
			return data.index;
		}
		data.parent->_update_children_cache();

		if (!p_include_internal) {
			return data.index;
		} else {
			switch (data.internal_mode) {
				case INTERNAL_MODE_DISABLED: {
					return data.parent->data.internal_children_front_count_cache + data.index;
				} break;
				case INTERNAL_MODE_FRONT: {
					return data.index;
				} break;
				case INTERNAL_MODE_BACK: {
					return data.parent->data.internal_children_front_count_cache + data.parent->data.external_children_count_cache + data.index;
				} break;
			}
			return -1;
		}
	}

	Ref<Tween> create_tween();

	void print_tree();
	void print_tree_pretty();
	String get_tree_string();
	String get_tree_string_pretty();

	void set_scene_file_path(const String &p_scene_file_path);
	String get_scene_file_path() const;

	void set_editor_description(const String &p_editor_description);
	String get_editor_description() const;

	void set_editable_instance(Node *p_node, bool p_editable);
	bool is_editable_instance(const Node *p_node) const;
	Node *get_deepest_editable_node(Node *p_start_node) const;

#ifdef TOOLS_ENABLED
	void set_property_pinned(const String &p_property, bool p_pinned);
	bool is_property_pinned(const StringName &p_property) const;
	virtual StringName get_property_store_alias(const StringName &p_property) const;
	bool is_part_of_edited_scene() const;
#else
	bool is_part_of_edited_scene() const { return false; }
#endif
	void get_storable_properties(HashSet<StringName> &r_storable_properties) const;

	virtual String to_string() override;

	/* NOTIFICATIONS */

	void propagate_notification(int p_notification);

	void propagate_call(const StringName &p_method, const Array &p_args = Array(), const bool p_parent_first = false);

	/* PROCESSING */

	void set_physics_process(bool p_process);
	double get_physics_process_delta_time() const;
	bool is_physics_processing() const;

	void set_process(bool p_process);
	double get_process_delta_time() const;
	bool is_processing() const;

	void set_physics_process_internal(bool p_process_internal);
	bool is_physics_processing_internal() const;

	void set_process_internal(bool p_process_internal);
	bool is_processing_internal() const;

	void set_process_priority(int p_priority);
	int get_process_priority() const;

	void set_process_thread_group_order(int p_order);
	int get_process_thread_group_order() const;

	void set_physics_process_priority(int p_priority);
	int get_physics_process_priority() const;

	void set_process_input(bool p_enable);
	bool is_processing_input() const;

	void set_process_shortcut_input(bool p_enable);
	bool is_processing_shortcut_input() const;

	void set_process_unhandled_input(bool p_enable);
	bool is_processing_unhandled_input() const;

	void set_process_unhandled_key_input(bool p_enable);
	bool is_processing_unhandled_key_input() const;

	_FORCE_INLINE_ bool _is_any_processing() const {
		return data.process || data.process_internal || data.physics_process || data.physics_process_internal;
	}
	_FORCE_INLINE_ bool is_accessible_from_caller_thread() const {
		if (current_process_thread_group == nullptr) {
			// No thread processing.
			// Only accessible if node is outside the scene tree
			// or access will happen from a node-safe thread.
			return !data.inside_tree || is_current_thread_safe_for_nodes();
		} else {
			// Thread processing.
			return current_process_thread_group == data.process_thread_group_owner;
		}
	}

	_FORCE_INLINE_ bool is_readable_from_caller_thread() const {
		if (current_process_thread_group == nullptr) {
			// No thread processing.
			// Only accessible if node is outside the scene tree
			// or access will happen from a node-safe thread.
			return is_current_thread_safe_for_nodes() || unlikely(!data.inside_tree);
		} else {
			// Thread processing.
			return true;
		}
	}

	_FORCE_INLINE_ static bool is_group_processing() { return current_process_thread_group; }

	void set_process_thread_messages(BitField<ProcessThreadMessages> p_flags);
	BitField<ProcessThreadMessages> get_process_thread_messages() const;

	Node *duplicate(int p_flags = DUPLICATE_GROUPS | DUPLICATE_SIGNALS | DUPLICATE_SCRIPTS) const;
#ifdef TOOLS_ENABLED
	Node *duplicate_from_editor(HashMap<const Node *, Node *> &r_duplimap) const;
	Node *duplicate_from_editor(HashMap<const Node *, Node *> &r_duplimap, const HashMap<Ref<Resource>, Ref<Resource>> &p_resource_remap) const;
	void remap_node_resources(Node *p_node, const HashMap<Ref<Resource>, Ref<Resource>> &p_resource_remap) const;
	void remap_nested_resources(Ref<Resource> p_resource, const HashMap<Ref<Resource>, Ref<Resource>> &p_resource_remap) const;
#endif

	// used by editors, to save what has changed only
	void set_scene_instance_state(const Ref<SceneState> &p_state);
	Ref<SceneState> get_scene_instance_state() const;

	void set_scene_inherited_state(const Ref<SceneState> &p_state);
	Ref<SceneState> get_scene_inherited_state() const;

	void set_scene_instance_load_placeholder(bool p_enable);
	bool get_scene_instance_load_placeholder() const;

	template <typename... VarArgs>
	Vector<Variant> make_binds(VarArgs... p_args) {
		Vector<Variant> binds = { p_args... };
		return binds;
	}

	void replace_by(Node *p_node, bool p_keep_groups = false);

	void set_process_mode(ProcessMode p_mode);
	ProcessMode get_process_mode() const;
	bool can_process() const;
	bool can_process_notification(int p_what) const;

	void set_physics_interpolation_mode(PhysicsInterpolationMode p_mode);
	PhysicsInterpolationMode get_physics_interpolation_mode() const { return data.physics_interpolation_mode; }
	_FORCE_INLINE_ bool is_physics_interpolated() const { return data.physics_interpolated; }
	_FORCE_INLINE_ bool is_physics_interpolated_and_enabled() const { return is_inside_tree() && get_tree()->is_physics_interpolation_enabled() && is_physics_interpolated(); }
	void reset_physics_interpolation();

	bool is_enabled() const;
	bool is_ready() const;

	void request_ready();

	void set_process_thread_group(ProcessThreadGroup p_mode);
	ProcessThreadGroup get_process_thread_group() const;

	static void print_orphan_nodes();

#ifdef TOOLS_ENABLED
	String validate_child_name(Node *p_child);
	String prevalidate_child_name(Node *p_child, StringName p_name);
	void get_argument_options(const StringName &p_function, int p_idx, List<String> *r_options) const override;
#endif
	static String adjust_name_casing(const String &p_name);

	void queue_free();

	//hacks for speed
	static void init_node_hrcr();

	void force_parent_owned() { data.parent_owned = true; } //hack to avoid duplicate nodes

	void set_import_path(const NodePath &p_import_path); //path used when imported, used by scene editors to keep tracking
	NodePath get_import_path() const;

	bool is_owned_by_parent() const;

	void clear_internal_tree_resource_paths();

	_FORCE_INLINE_ Viewport *get_viewport() const { return data.viewport; }

	virtual PackedStringArray get_configuration_warnings() const;

	void update_configuration_warnings();

	void set_display_folded(bool p_folded);
	bool is_displayed_folded() const;

	/* NETWORK */

	virtual void set_multiplayer_authority(int p_peer_id, bool p_recursive = true);
	int get_multiplayer_authority() const;
	bool is_multiplayer_authority() const;

	void rpc_config(const StringName &p_method, const Variant &p_config); // config a local method for RPC
	Variant get_rpc_config() const;

	template <typename... VarArgs>
	Error rpc(const StringName &p_method, VarArgs... p_args);

	template <typename... VarArgs>
	Error rpc_id(int p_peer_id, const StringName &p_method, VarArgs... p_args);

	Error rpcp(int p_peer_id, const StringName &p_method, const Variant **p_arg, int p_argcount);

	Ref<MultiplayerAPI> get_multiplayer() const;

	/* INTERNATIONALIZATION */

	void set_auto_translate_mode(AutoTranslateMode p_mode);
	AutoTranslateMode get_auto_translate_mode() const;
	bool can_auto_translate() const;

	virtual StringName get_translation_domain() const override;
	virtual void set_translation_domain(const StringName &p_domain) override;
	void set_translation_domain_inherited();

	_FORCE_INLINE_ String atr(const String &p_message, const StringName &p_context = "") const { return can_auto_translate() ? tr(p_message, p_context) : p_message; }
	_FORCE_INLINE_ String atr_n(const String &p_message, const StringName &p_message_plural, int p_n, const StringName &p_context = "") const {
		if (can_auto_translate()) {
			return tr_n(p_message, p_message_plural, p_n, p_context);
		}
		return p_n == 1 ? p_message : String(p_message_plural);
	}

	/* THREADING */

	void call_deferred_thread_groupp(const StringName &p_method, const Variant **p_args, int p_argcount, bool p_show_error = false);
	template <typename... VarArgs>
	void call_deferred_thread_group(const StringName &p_method, VarArgs... p_args) {
		Variant args[sizeof...(p_args) + 1] = { p_args..., Variant() }; // +1 makes sure zero sized arrays are also supported.
		const Variant *argptrs[sizeof...(p_args) + 1];
		for (uint32_t i = 0; i < sizeof...(p_args); i++) {
			argptrs[i] = &args[i];
		}
		call_deferred_thread_groupp(p_method, sizeof...(p_args) == 0 ? nullptr : (const Variant **)argptrs, sizeof...(p_args));
	}
	void set_deferred_thread_group(const StringName &p_property, const Variant &p_value);
	void notify_deferred_thread_group(int p_notification);

	void call_thread_safep(const StringName &p_method, const Variant **p_args, int p_argcount, bool p_show_error = false);
	template <typename... VarArgs>
	void call_thread_safe(const StringName &p_method, VarArgs... p_args) {
		Variant args[sizeof...(p_args) + 1] = { p_args..., Variant() }; // +1 makes sure zero sized arrays are also supported.
		const Variant *argptrs[sizeof...(p_args) + 1];
		for (uint32_t i = 0; i < sizeof...(p_args); i++) {
			argptrs[i] = &args[i];
		}
		call_deferred_thread_groupp(p_method, sizeof...(p_args) == 0 ? nullptr : (const Variant **)argptrs, sizeof...(p_args));
	}
	void set_thread_safe(const StringName &p_property, const Variant &p_value);
	void notify_thread_safe(int p_notification);

	// These inherited functions need proper multithread locking when overridden in Node.
#ifdef DEBUG_ENABLED

	virtual void set_script(const Variant &p_script) override;
	virtual Variant get_script() const override;

	virtual bool has_meta(const StringName &p_name) const override;
	virtual void set_meta(const StringName &p_name, const Variant &p_value) override;
	virtual void remove_meta(const StringName &p_name) override;
	virtual Variant get_meta(const StringName &p_name, const Variant &p_default = Variant()) const override;
	virtual void get_meta_list(List<StringName> *p_list) const override;

	virtual Error emit_signalp(const StringName &p_name, const Variant **p_args, int p_argcount) override;
	virtual bool has_signal(const StringName &p_name) const override;
	virtual void get_signal_list(List<MethodInfo> *p_signals) const override;
	virtual void get_signal_connection_list(const StringName &p_signal, List<Connection> *p_connections) const override;
	virtual void get_all_signal_connections(List<Connection> *p_connections) const override;
	virtual int get_persistent_signal_connection_count() const override;
	virtual void get_signals_connected_to_this(List<Connection> *p_connections) const override;

	virtual Error connect(const StringName &p_signal, const Callable &p_callable, uint32_t p_flags = 0) override;
	virtual void disconnect(const StringName &p_signal, const Callable &p_callable) override;
	virtual bool is_connected(const StringName &p_signal, const Callable &p_callable) const override;
	virtual bool has_connections(const StringName &p_signal) const override;
#endif
	Node();
	~Node();
};

VARIANT_ENUM_CAST(Node::DuplicateFlags);
VARIANT_ENUM_CAST(Node::ProcessMode);
VARIANT_ENUM_CAST(Node::ProcessThreadGroup);
VARIANT_BITFIELD_CAST(Node::ProcessThreadMessages);
VARIANT_ENUM_CAST(Node::InternalMode);
VARIANT_ENUM_CAST(Node::PhysicsInterpolationMode);
VARIANT_ENUM_CAST(Node::AutoTranslateMode);

typedef HashSet<Node *, Node::Comparator> NodeSet;

// Template definitions must be in the header so they are always fully initialized before their usage.
// See this StackOverflow question for more information: https://stackoverflow.com/questions/495021/why-can-templates-only-be-implemented-in-the-header-file

template <typename... VarArgs>
Error Node::rpc(const StringName &p_method, VarArgs... p_args) {
	return rpc_id(0, p_method, p_args...);
}

template <typename... VarArgs>
Error Node::rpc_id(int p_peer_id, const StringName &p_method, VarArgs... p_args) {
	Variant args[sizeof...(p_args) + 1] = { p_args..., Variant() }; // +1 makes sure zero sized arrays are also supported.
	const Variant *argptrs[sizeof...(p_args) + 1];
	for (uint32_t i = 0; i < sizeof...(p_args); i++) {
		argptrs[i] = &args[i];
	}
	return rpcp(p_peer_id, p_method, sizeof...(p_args) == 0 ? nullptr : (const Variant **)argptrs, sizeof...(p_args));
}

#ifdef DEBUG_ENABLED
#define ERR_THREAD_GUARD ERR_FAIL_COND_MSG(!is_accessible_from_caller_thread(), vformat("Caller thread can't call this function in this node (%s). Use call_deferred() or call_thread_group() instead.", get_description()));
#define ERR_THREAD_GUARD_V(m_ret) ERR_FAIL_COND_V_MSG(!is_accessible_from_caller_thread(), (m_ret), vformat("Caller thread can't call this function in this node (%s). Use call_deferred() or call_thread_group() instead.", get_description()));
#define ERR_MAIN_THREAD_GUARD ERR_FAIL_COND_MSG(is_inside_tree() && !is_current_thread_safe_for_nodes(), vformat("This function in this node (%s) can only be accessed from the main thread. Use call_deferred() instead.", get_description()));
#define ERR_MAIN_THREAD_GUARD_V(m_ret) ERR_FAIL_COND_V_MSG(is_inside_tree() && !is_current_thread_safe_for_nodes(), (m_ret), vformat("This function in this node (%s) can only be accessed from the main thread. Use call_deferred() instead.", get_description()));
#define ERR_READ_THREAD_GUARD ERR_FAIL_COND_MSG(!is_readable_from_caller_thread(), vformat("This function in this node (%s) can only be accessed from either the main thread or a thread group. Use call_deferred() instead.", get_description()));
#define ERR_READ_THREAD_GUARD_V(m_ret) ERR_FAIL_COND_V_MSG(!is_readable_from_caller_thread(), (m_ret), vformat("This function in this node (%s) can only be accessed from either the main thread or a thread group. Use call_deferred() instead.", get_description()));
#else
#define ERR_THREAD_GUARD
#define ERR_THREAD_GUARD_V(m_ret)
#define ERR_MAIN_THREAD_GUARD
#define ERR_MAIN_THREAD_GUARD_V(m_ret)
#define ERR_READ_THREAD_GUARD
#define ERR_READ_THREAD_GUARD_V(m_ret)
#endif

// Add these macro to your class's 'get_configuration_warnings' function to have warnings show up in the scene tree inspector.
#define DEPRECATED_NODE_WARNING warnings.push_back(RTR("This node is marked as deprecated and will be removed in future versions.\nPlease check the Godot documentation for information about migration."));
#define EXPERIMENTAL_NODE_WARNING warnings.push_back(RTR("This node is marked as experimental and may be subject to removal or major changes in future versions."));

#endif // NODE_H

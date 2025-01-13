/**************************************************************************/
/*  scene_tree_editor.h                                                   */
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

#ifndef SCENE_TREE_EDITOR_H
#define SCENE_TREE_EDITOR_H

#include "scene/gui/check_box.h"
#include "scene/gui/check_button.h"
#include "scene/gui/dialogs.h"
#include "scene/gui/tree.h"

class EditorSelection;
class TextureRect;
class Timer;

class SceneTreeEditor : public Control {
	GDCLASS(SceneTreeEditor, Control);

	EditorSelection *editor_selection = nullptr;

	enum SceneTreeEditorButton {
		BUTTON_SUBSCENE = 0,
		BUTTON_VISIBILITY = 1,
		BUTTON_SCRIPT = 2,
		BUTTON_LOCK = 3,
		BUTTON_GROUP = 4,
		BUTTON_WARNING = 5,
		BUTTON_SIGNALS = 6,
		BUTTON_GROUPS = 7,
		BUTTON_PIN = 8,
		BUTTON_UNIQUE = 9,
	};

	struct CachedNode {
		Node *node = nullptr;
		TreeItem *item = nullptr;
		int index = -1;
		bool dirty = true;
		bool has_moved_children = false;
		bool removed = false;

		// Store the iterator for faster removal. This is safe as
		// HashMap never moves elements.
		HashMap<Node *, CachedNode>::Iterator cache_iterator;
		// This is safe because it gets compared to a uint8_t.
		uint16_t delete_serial = UINT16_MAX;

		// To know whether to update children or not.
		bool can_process = false;

		CachedNode() = delete; // Always an error.
		CachedNode(Node *p_node, TreeItem *p_item) :
				node(p_node), item(p_item) {}
	};

	struct NodeCache {
		~NodeCache() {
			clear();
		}

		NodeCache(SceneTreeEditor *p_editor) :
				editor(p_editor) {}

		HashMap<Node *, CachedNode>::Iterator add(Node *p_node, TreeItem *p_item);
		HashMap<Node *, CachedNode>::Iterator get(Node *p_node, bool p_deleted_ok = true);
		void remove(Node *p_node, bool p_recursive = false);
		void mark_dirty(Node *p_node, bool p_parents = true);
		void mark_children_dirty(Node *p_node, bool p_recursive = false);

		void delete_pending();
		void clear();

		SceneTreeEditor *editor;
		HashMap<Node *, CachedNode> cache;
		HashSet<CachedNode *> to_delete;
		Node *current_scene_node = nullptr;
		Node *current_pinned_node = nullptr;
		bool current_has_pin = false;
		bool force_update = false;
		uint8_t delete_serial = 0;
	};

	NodeCache node_cache;

	Tree *tree = nullptr;
	Node *selected = nullptr;
	ObjectID instance_node;

	String filter;
	String filter_term_warning;
	bool show_all_nodes = false;

	AcceptDialog *error = nullptr;
	AcceptDialog *warning = nullptr;

	ConfirmationDialog *revoke_dialog = nullptr;
	Label *revoke_dialog_label = nullptr;
	CheckBox *ask_before_revoke_checkbox = nullptr;
	Node *revoke_node = nullptr;

	bool auto_expand_selected = true;
	bool hide_filtered_out_parents = false;
	bool connect_to_script_mode = false;
	bool connecting_signal = false;
	bool update_when_invisible = true;

	int blocked;

	void _compute_hash(Node *p_node, uint64_t &hash);
	void _reset();
	PackedStringArray _get_node_configuration_warnings(Node *p_node);

	void _update_node_path(Node *p_node, bool p_recursive = true);
	void _update_node_subtree(Node *p_node, TreeItem *p_parent, bool p_force = false);
	void _update_node(Node *p_node, TreeItem *p_item, bool p_part_of_subscene);
	void _update_if_clean();

	void _test_update_tree();
	bool _update_filter(TreeItem *p_parent = nullptr, bool p_scroll_to_selected = false);
	bool _item_matches_all_terms(TreeItem *p_item, const PackedStringArray &p_terms);
	void _tree_changed();
	void _tree_process_mode_changed();

	void _move_node_children(HashMap<Node *, CachedNode>::Iterator &p_I);
	void _move_node_item(TreeItem *p_parent, HashMap<Node *, CachedNode>::Iterator &p_I);

	void _node_child_order_changed(Node *p_node);
	void _node_editor_state_changed(Node *p_node);
	void _node_added(Node *p_node);
	void _node_removed(Node *p_node);
	void _node_renamed(Node *p_node);

	TreeItem *_find(TreeItem *p_node, const NodePath &p_path);
	void _notification(int p_what);
	void _selected_changed();
	void _deselect_items();

	void _cell_collapsed(Object *p_obj);

	uint64_t last_hash;

	bool can_rename;
	bool can_open_instance;
	bool updating_tree = false;
	bool show_enabled_subscene = false;
	bool is_scene_tree_dock = false;

	void _edited();
	void _renamed(TreeItem *p_item, TreeItem *p_batch_item, Node *p_node = nullptr);

	HashSet<Node *> marked;
	bool marked_selectable = false;
	bool marked_children_selectable = false;
	bool display_foreign = false;
	bool tree_dirty = true;
	bool pending_test_update = false;
	Timer *update_node_tooltip_delay = nullptr;

	static void _bind_methods();

	void _cell_button_pressed(Object *p_item, int p_column, int p_id, MouseButton p_button);
	void _toggle_visible(Node *p_node);
	void _cell_multi_selected(Object *p_object, int p_cell, bool p_selected);
	void _update_selection(TreeItem *item);
	void _node_script_changed(Node *p_node);
	void _node_visibility_changed(Node *p_node);
	void _update_visibility_color(Node *p_node, TreeItem *p_item);
	void _set_item_custom_color(TreeItem *p_item, Color p_color);
	void _update_node_tooltip(Node *p_node, TreeItem *p_item);
	void _queue_update_node_tooltip(Node *p_node, TreeItem *p_item);
	void _tree_scroll_to_item(ObjectID p_item_id);

	void _selection_changed();
	Node *get_scene_node() const;

	Variant get_drag_data_fw(const Point2 &p_point, Control *p_from);
	bool can_drop_data_fw(const Point2 &p_point, const Variant &p_data, Control *p_from) const;
	void drop_data_fw(const Point2 &p_point, const Variant &p_data, Control *p_from);

	void _empty_clicked(const Vector2 &p_pos, MouseButton p_button);
	void _rmb_select(const Vector2 &p_pos, MouseButton p_button = MouseButton::RIGHT);

	void _warning_changed(Node *p_for_node);
	void _update_marking_list(const HashSet<Node *> &p_marked);

	Timer *update_timer = nullptr;

	List<StringName> *script_types;
	bool _is_script_type(const StringName &p_type) const;

	Vector<StringName> valid_types;

	void _update_ask_before_revoking_unique_name();
	void _revoke_unique_name();

public:
	// Public for use with callable_mp.
	void _update_tree(bool p_scroll_to_selected = false);

	void rename_node(Node *p_node, const String &p_name, TreeItem *p_item = nullptr);

	void set_filter(const String &p_filter);
	String get_filter() const;
	String get_filter_term_warning();
	void set_show_all_nodes(bool p_show_all_nodes);

	void set_as_scene_tree_dock();
	void set_display_foreign_nodes(bool p_display);

	void set_marked(const HashSet<Node *> &p_marked, bool p_selectable = true, bool p_children_selectable = true);
	void set_marked(Node *p_marked, bool p_selectable = true, bool p_children_selectable = true);
	void set_selected(Node *p_node, bool p_emit_selected = true);
	Node *get_selected();
	void set_can_rename(bool p_can_rename) { can_rename = p_can_rename; }
	void set_editor_selection(EditorSelection *p_selection);

	void set_show_enabled_subscene(bool p_show) { show_enabled_subscene = p_show; }
	void set_valid_types(const Vector<StringName> &p_valid);

	inline void update_tree() { _update_tree(); }

	void set_auto_expand_selected(bool p_auto, bool p_update_settings);
	void set_hide_filtered_out_parents(bool p_hide, bool p_update_settings);
	void set_connect_to_script_mode(bool p_enable);
	void set_connecting_signal(bool p_enable);
	void set_update_when_invisible(bool p_enable);

	Tree *get_scene_tree() { return tree; }

	void update_warning();

	SceneTreeEditor(bool p_label = true, bool p_can_rename = false, bool p_can_open_instance = false);
	~SceneTreeEditor();
};

class SceneTreeDialog : public ConfirmationDialog {
	GDCLASS(SceneTreeDialog, ConfirmationDialog);

	VBoxContainer *content = nullptr;
	SceneTreeEditor *tree = nullptr;
	LineEdit *filter = nullptr;
	CheckButton *show_all_nodes = nullptr;
	LocalVector<TextureRect *> valid_type_icons;
	HBoxContainer *allowed_types_hbox = nullptr;

	void _select();
	void _cancel();
	void _selected_changed();
	void _filter_changed(const String &p_filter);
	void _on_filter_gui_input(const Ref<InputEvent> &p_event);
	void _show_all_nodes_changed(bool p_button_pressed);

protected:
	void _update_valid_type_icons();
	void _notification(int p_what);
	static void _bind_methods();

public:
	void popup_scenetree_dialog(Node *p_selected_node = nullptr, Node *p_marked_node = nullptr, bool p_marked_node_selectable = true, bool p_marked_node_children_selectable = true);
	void set_valid_types(const Vector<StringName> &p_valid);

	SceneTreeEditor *get_scene_tree() { return tree; }
	LineEdit *get_filter_line_edit() { return filter; }

	SceneTreeDialog();
	~SceneTreeDialog();
};

#endif // SCENE_TREE_EDITOR_H

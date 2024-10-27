/**************************************************************************/
/*  scene_tree_dock.h                                                     */
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

#ifndef SCENE_TREE_DOCK_H
#define SCENE_TREE_DOCK_H

#include "editor/gui/scene_tree_editor.h"
#include "editor/script_create_dialog.h"
#include "scene/gui/box_container.h"
#include "scene/resources/animation.h"

class CheckBox;
class EditorData;
class EditorSelection;
class MenuButton;
class ReparentDialog;
class ShaderCreateDialog;
class TextureRect;

#include "modules/modules_enabled.gen.h" // For regex.
#ifdef MODULE_REGEX_ENABLED
class RenameDialog;
#endif // MODULE_REGEX_ENABLED

class SceneTreeDock : public VBoxContainer {
	GDCLASS(SceneTreeDock, VBoxContainer);

	enum Tool {
		TOOL_NEW,
		TOOL_INSTANTIATE,
		TOOL_EXPAND_COLLAPSE,
		TOOL_CUT,
		TOOL_COPY,
		TOOL_PASTE,
		TOOL_PASTE_AS_SIBLING,
		TOOL_RENAME,
#ifdef MODULE_REGEX_ENABLED
		TOOL_BATCH_RENAME,
#endif // MODULE_REGEX_ENABLED
		TOOL_REPLACE,
		TOOL_EXTEND_SCRIPT,
		TOOL_ATTACH_SCRIPT,
		TOOL_DETACH_SCRIPT,
		TOOL_MOVE_UP,
		TOOL_MOVE_DOWN,
		TOOL_DUPLICATE,
		TOOL_REPARENT,
		TOOL_REPARENT_TO_NEW_NODE,
		TOOL_MAKE_ROOT,
		TOOL_NEW_SCENE_FROM,
		TOOL_MULTI_EDIT,
		TOOL_ERASE,
		TOOL_COPY_NODE_PATH,
		TOOL_SHOW_IN_FILE_SYSTEM,
		TOOL_OPEN_DOCUMENTATION,
		TOOL_AUTO_EXPAND,
		TOOL_SCENE_EDITABLE_CHILDREN,
		TOOL_SCENE_USE_PLACEHOLDER,
		TOOL_SCENE_MAKE_LOCAL,
		TOOL_SCENE_OPEN,
		TOOL_SCENE_CLEAR_INHERITANCE,
		TOOL_SCENE_CLEAR_INHERITANCE_CONFIRM,
		TOOL_SCENE_OPEN_INHERITED,
		TOOL_TOGGLE_SCENE_UNIQUE_NAME,
		TOOL_CREATE_2D_SCENE,
		TOOL_CREATE_3D_SCENE,
		TOOL_CREATE_USER_INTERFACE,
		TOOL_CREATE_FAVORITE,
		TOOL_CENTER_PARENT,

	};

	enum {
		EDIT_SUBRESOURCE_BASE = 100
	};

	Vector<ObjectID> subresources;

	bool reset_create_dialog = false;

	int current_option = 0;
	CreateDialog *create_dialog = nullptr;
#ifdef MODULE_REGEX_ENABLED
	RenameDialog *rename_dialog = nullptr;
#endif // MODULE_REGEX_ENABLED

	Button *button_add = nullptr;
	Button *button_instance = nullptr;
	Button *button_create_script = nullptr;
	Button *button_detach_script = nullptr;
	Button *button_extend_script = nullptr;
	MenuButton *button_tree_menu = nullptr;

	Button *node_shortcuts_toggle = nullptr;
	VBoxContainer *beginner_node_shortcuts = nullptr;
	VBoxContainer *favorite_node_shortcuts = nullptr;

	Button *button_2d = nullptr;
	Button *button_3d = nullptr;
	Button *button_ui = nullptr;
	Button *button_custom = nullptr;
	Button *button_clipboard = nullptr;

	HBoxContainer *button_hb = nullptr;
	Button *edit_local, *edit_remote;
	SceneTreeEditor *scene_tree = nullptr;
	Control *remote_tree = nullptr;

	HBoxContainer *tool_hbc = nullptr;
	void _tool_selected(int p_tool, bool p_confirm_override = false);
	void _property_selected(int p_idx);

	Node *property_drop_node = nullptr;
	String resource_drop_path;
	void _perform_property_drop(Node *p_node, const String &p_property, Ref<Resource> p_res);

	EditorData *editor_data = nullptr;
	EditorSelection *editor_selection = nullptr;

	List<Node *> node_clipboard;
	HashSet<Node *> node_clipboard_edited_scene_owned;
	String clipboard_source_scene;
	HashMap<String, HashMap<Ref<Resource>, Ref<Resource>>> clipboard_resource_remap;

	ScriptCreateDialog *script_create_dialog = nullptr;
	ShaderCreateDialog *shader_create_dialog = nullptr;
	AcceptDialog *accept = nullptr;
	ConfirmationDialog *delete_dialog = nullptr;
	Label *delete_dialog_label = nullptr;
	CheckBox *delete_tracks_checkbox = nullptr;
	ConfirmationDialog *editable_instance_remove_dialog = nullptr;
	ConfirmationDialog *placeholder_editable_instance_remove_dialog = nullptr;

	ReparentDialog *reparent_dialog = nullptr;
	EditorFileDialog *new_scene_from_dialog = nullptr;

	enum FilterMenuItems {
		FILTER_BY_TYPE = 64, // Used in the same menus as the Tool enum.
		FILTER_BY_GROUP,
	};

	LineEdit *filter = nullptr;
	PopupMenu *filter_quick_menu = nullptr;
	TextureRect *filter_icon = nullptr;

	PopupMenu *menu = nullptr;
	PopupMenu *menu_subresources = nullptr;
	PopupMenu *menu_properties = nullptr;
	ConfirmationDialog *clear_inherit_confirm = nullptr;

	bool first_enter = true;

	void _create();
	Node *_do_create(Node *p_parent);
	void _post_do_create(Node *p_child);
	Node *scene_root = nullptr;
	Node *edited_scene = nullptr;
	Node *pending_click_select = nullptr;
	bool tree_clicked = false;

	VBoxContainer *create_root_dialog = nullptr;
	String selected_favorite_root;

	Ref<ShaderMaterial> selected_shader_material;

	void _add_children_to_popup(Object *p_obj, int p_depth);

	void _node_reparent(NodePath p_path, bool p_keep_global_xform);
	void _do_reparent(Node *p_new_parent, int p_position_in_parent, Vector<Node *> p_nodes, bool p_keep_global_xform);

	void _set_owners(Node *p_owner, const Array &p_nodes);

	enum ReplaceOwnerMode {
		MODE_BIDI,
		MODE_DO,
		MODE_UNDO
	};

	void _node_replace_owner(Node *p_base, Node *p_node, Node *p_root, ReplaceOwnerMode p_mode = MODE_BIDI);
	void _node_strip_signal_inheritance(Node *p_node);
	void _load_request(const String &p_path);
	void _script_open_request(const Ref<Script> &p_script);
	void _push_item(Object *p_object);
	void _handle_select(Node *p_node);

	bool _cyclical_dependency_exists(const String &p_target_scene_path, Node *p_desired_node);
	bool _track_inherit(const String &p_target_scene_path, Node *p_desired_node);

	void _node_selected();
	void _node_renamed();
	void _script_created(Ref<Script> p_script);
	void _shader_created(Ref<Shader> p_shader);
	void _script_creation_closed();
	void _shader_creation_closed();

	void _delete_confirm(bool p_cut = false);
	void _delete_dialog_closed();

	void _toggle_editable_children_from_selection();

	void _reparent_nodes_to_root(Node *p_root, const Array &p_nodes, Node *p_owner);
	void _reparent_nodes_to_paths_with_transform_and_name(Node *p_root, const Array &p_nodes, const Array &p_paths, const Array &p_transforms, const Array &p_names, Node *p_owner);
	void _toggle_editable_children(Node *p_node);

	void _toggle_placeholder_from_selection();

	void _node_prerenamed(Node *p_node, const String &p_new_name);

	void _nodes_drag_begin();

	void _handle_hover_to_inspect();
	void _inspect_hovered_node();
	void _reset_hovering_timer();
	Timer *inspect_hovered_node_delay = nullptr;
	TreeItem *tree_item_inspected = nullptr;
	Node *node_hovered_now = nullptr;
	Node *node_hovered_previously = nullptr;
	bool select_node_hovered_at_end_of_drag = false;
	bool hovered_but_reparenting = false;

	virtual void input(const Ref<InputEvent> &p_event) override;
	virtual void shortcut_input(const Ref<InputEvent> &p_event) override;
	void _scene_tree_gui_input(Ref<InputEvent> p_event);

	void _new_scene_from(const String &p_file);
	void _set_node_owner_recursive(Node *p_node, Node *p_owner, const HashMap<const Node *, Node *> &p_inverse_duplimap);

	bool _validate_no_foreign();
	bool _validate_no_instance();
	void _selection_changed();
	void _update_script_button();

	void _fill_path_renames(Vector<StringName> base_path, Vector<StringName> new_base_path, Node *p_node, HashMap<Node *, NodePath> *p_renames);
	bool _has_tracks_to_delete(Node *p_node, List<Node *> &p_to_delete) const;

	void _normalize_drop(Node *&to_node, int &to_pos, int p_type);
	Array _get_selection_array();

	void _nodes_dragged(const Array &p_nodes, NodePath p_to, int p_type);
	void _files_dropped(const Vector<String> &p_files, NodePath p_to, int p_type);
	void _script_dropped(const String &p_file, NodePath p_to);
	void _quick_open(const String &p_file_path);

	void _tree_rmb(const Vector2 &p_menu_pos);
	void _update_tree_menu();

	void _filter_changed(const String &p_filter);
	void _filter_gui_input(const Ref<InputEvent> &p_event);
	void _filter_option_selected(int option);
	void _append_filter_options_to(PopupMenu *p_menu, bool p_include_separator = true);

	void _perform_instantiate_scenes(const Vector<String> &p_files, Node *p_parent, int p_pos);
	void _perform_create_audio_stream_players(const Vector<String> &p_files, Node *p_parent, int p_pos);
	void _replace_with_branch_scene(const String &p_file, Node *base);

	void _remote_tree_selected();
	void _local_tree_selected();

	void _update_create_root_dialog(bool p_initializing = false);
	void _favorite_root_selected(const String &p_class);

	void _feature_profile_changed();

	void _clear_clipboard();
	void _create_remap_for_node(Node *p_node, HashMap<Ref<Resource>, Ref<Resource>> &r_remap);
	void _create_remap_for_resource(Ref<Resource> p_resource, HashMap<Ref<Resource>, Ref<Resource>> &r_remap);

	void _list_all_subresources(PopupMenu *p_menu);
	void _gather_resources(Node *p_node, List<Pair<Ref<Resource>, Node *>> &r_resources);
	void _edit_subresource(int p_idx, const PopupMenu *p_from_menu);

	bool profile_allow_editing = true;
	bool profile_allow_script_editing = true;

	static void _update_configuration_warning();

	bool _update_node_path(Node *p_root_node, NodePath &r_node_path, HashMap<Node *, NodePath> *p_renames) const;
	bool _check_node_path_recursive(Node *p_root_node, Variant &r_variant, HashMap<Node *, NodePath> *p_renames, bool p_inside_resource = false) const;
	bool _check_node_recursive(Variant &r_variant, Node *p_node, Node *p_by_node, const String type_hint, String &r_warn_message);
	void _replace_node(Node *p_node, Node *p_by_node, bool p_keep_properties = true, bool p_remove_old = true);

private:
	static SceneTreeDock *singleton;

public:
	static SceneTreeDock *get_singleton() { return singleton; }

protected:
	void _notification(int p_what);
	static void _bind_methods();

public:
	String get_filter();
	void set_filter(const String &p_filter);
	void save_branch_to_file(const String &p_directory);

	void _focus_node();

	void add_root_node(Node *p_node);
	void set_edited_scene(Node *p_scene);
	void instantiate(const String &p_file);
	void instantiate_scenes(const Vector<String> &p_files, Node *p_parent = nullptr);
	void set_selection(const Vector<Node *> &p_nodes);
	void set_selected(Node *p_node, bool p_emit_selected = false);
	void fill_path_renames(Node *p_node, Node *p_new_parent, HashMap<Node *, NodePath> *p_renames);
	void perform_node_renames(Node *p_base, HashMap<Node *, NodePath> *p_renames, HashMap<Ref<Animation>, HashSet<int>> *r_rem_anims = nullptr);
	void perform_node_replace(Node *p_base, Node *p_node, Node *p_by_node);
	SceneTreeEditor *get_tree_editor() { return scene_tree; }
	EditorData *get_editor_data() { return editor_data; }

	void add_remote_tree_editor(Control *p_remote);
	void show_remote_tree();
	void hide_remote_tree();
	void show_tab_buttons();
	void hide_tab_buttons();

	void replace_node(Node *p_node, Node *p_by_node);

	void attach_script_to_selected(bool p_extend);
	void open_script_dialog(Node *p_for_node, bool p_extend);

	void attach_shader_to_selected(int p_preferred_mode = -1);
	void open_shader_dialog(const Ref<ShaderMaterial> &p_for_material, int p_preferred_mode = -1);

	void open_add_child_dialog();
	void open_instance_child_dialog();

	List<Node *> paste_nodes(bool p_paste_as_sibling = false);
	List<Node *> get_node_clipboard() const;

	ScriptCreateDialog *get_script_create_dialog() {
		return script_create_dialog;
	}

	SceneTreeDock(Node *p_scene_root, EditorSelection *p_editor_selection, EditorData &p_editor_data);
	~SceneTreeDock();
};

#endif // SCENE_TREE_DOCK_H

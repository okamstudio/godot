/**************************************************************************/
/*  animation_blend_tree_editor_plugin.h                                  */
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

#ifndef ANIMATION_BLEND_TREE_EDITOR_PLUGIN_H
#define ANIMATION_BLEND_TREE_EDITOR_PLUGIN_H

#include "core/object/script_language.h"
#include "editor/editor_inspector.h"
#include "editor/plugins/animation_tree_editor_plugin.h"
#include "scene/animation/animation_blend_tree.h"
#include "scene/gui/button.h"
#include "scene/gui/dialogs.h"
#include "scene/gui/graph_edit.h"
#include "scene/gui/panel_container.h"
#include "scene/gui/popup.h"
#include "scene/gui/tree.h"

class AcceptDialog;
class CheckBox;
class ProgressBar;
class EditorFileDialog;
class EditorProperty;
class MenuButton;
class PanelContainer;
class EditorInspectorPluginAnimationNodeAnimation;

class AnimationNodeBlendTreeEditor : public AnimationTreeNodeEditorPlugin {
	GDCLASS(AnimationNodeBlendTreeEditor, AnimationTreeNodeEditorPlugin);

	Ref<AnimationNodeBlendTree> blend_tree;

	bool read_only = false;

	GraphEdit *graph = nullptr;
	MenuButton *add_node = nullptr;
	Vector2 position_from_popup_menu;
	bool use_position_from_popup_menu;

	PanelContainer *error_panel = nullptr;
	Label *error_label = nullptr;

	AcceptDialog *filter_dialog = nullptr;
	Tree *filters = nullptr;
	CheckBox *filter_enabled = nullptr;
	Button *filter_fill_selection = nullptr;
	Button *filter_invert_selection = nullptr;
	Button *filter_clear_selection = nullptr;

	HashMap<StringName, ProgressBar *> animations;
	Vector<EditorProperty *> visible_properties;

	String to_node = "";
	int to_slot = -1;
	String from_node = "";

	struct AddOption {
		String name;
		String type;
		Ref<Script> script;
		int input_port_count;
		AddOption(const String &p_name = String(), const String &p_type = String(), int p_input_port_count = 0) :
				name(p_name),
				type(p_type),
				input_port_count(p_input_port_count) {
		}
	};

	Vector<AddOption> add_options;

	void _add_node(int p_idx);
	void _update_options_menu(bool p_has_input_ports = false);

	static AnimationNodeBlendTreeEditor *singleton;

	void _node_dragged(const Vector2 &p_from, const Vector2 &p_to, const StringName &p_which);
	void _node_renamed(const String &p_text, Ref<AnimationNode> p_node);
	void _node_renamed_focus_out(Ref<AnimationNode> p_node);
	void _node_rename_lineedit_changed(const String &p_text);
	void _node_changed(const StringName &p_node_name);

	String current_node_rename_text;
	bool updating;

	void _connection_request(const String &p_from, int p_from_index, const String &p_to, int p_to_index);
	void _disconnection_request(const String &p_from, int p_from_index, const String &p_to, int p_to_index);

	void _scroll_changed(const Vector2 &p_scroll);
	void _node_selected(Object *p_node);
	void _open_in_editor(const String &p_which);
	void _anim_selected(int p_index, const Array &p_options, const String &p_node);
	void _delete_node_request(const String &p_which);
	void _delete_nodes_request(const TypedArray<StringName> &p_nodes);

	bool _update_filters(const Ref<AnimationNode> &anode);
	void _inspect_filters(const String &p_which);
	void _filter_edited();
	void _filter_toggled();
	void _filter_fill_selection();
	void _filter_invert_selection();
	void _filter_clear_selection();
	void _filter_fill_selection_recursive(EditorUndoRedoManager *p_undo_redo, TreeItem *p_item, bool p_parent_filtered);
	void _filter_invert_selection_recursive(EditorUndoRedoManager *p_undo_redo, TreeItem *p_item);
	void _filter_clear_selection_recursive(EditorUndoRedoManager *p_undo_redo, TreeItem *p_item);
	Ref<AnimationNode> _filter_edit;

	void _popup(bool p_has_input_ports, const Vector2 &p_node_position);
	void _popup_request(const Vector2 &p_position);
	void _connection_to_empty(const String &p_from, int p_from_slot, const Vector2 &p_release_position);
	void _connection_from_empty(const String &p_to, int p_to_slot, const Vector2 &p_release_position);
	void _popup_hide();

	void _property_changed(const StringName &p_property, const Variant &p_value, const String &p_field, bool p_changing);

	void _update_editor_settings();

	EditorFileDialog *open_file = nullptr;
	Ref<AnimationNode> file_loaded;
	void _file_opened(const String &p_file);

	enum {
		MENU_LOAD_FILE = 1000,
		MENU_PASTE = 1001,
		MENU_LOAD_FILE_CONFIRM = 1002
	};

	Ref<EditorInspectorPluginAnimationNodeAnimation> animation_node_inspector_plugin;

protected:
	void _notification(int p_what);
	static void _bind_methods();

public:
	static AnimationNodeBlendTreeEditor *get_singleton() { return singleton; }

	void add_custom_type(const String &p_name, const Ref<Script> &p_script);
	void remove_custom_type(const Ref<Script> &p_script);

	virtual Size2 get_minimum_size() const override;

	virtual bool can_edit(const Ref<AnimationNode> &p_node) override;
	virtual void edit(const Ref<AnimationNode> &p_node) override;

	void update_graph();

	AnimationNodeBlendTreeEditor();
	~AnimationNodeBlendTreeEditor();
};

// EditorPluginAnimationNodeAnimation

class EditorInspectorPluginAnimationNodeAnimation : public EditorInspectorPlugin {
	GDCLASS(EditorInspectorPluginAnimationNodeAnimation, EditorInspectorPlugin);

public:
	virtual bool can_handle(Object *p_object) override;
	virtual bool parse_property(Object *p_object, const Variant::Type p_type, const String &p_path, const PropertyHint p_hint, const String &p_hint_text, const BitField<PropertyUsageFlags> p_usage, const bool p_wide) override;
};

class AnimationNodeAnimationEditorDialog : public ConfirmationDialog {
	GDCLASS(AnimationNodeAnimationEditorDialog, ConfirmationDialog);

	friend class AnimationNodeAnimationEditor;

	OptionButton *select_start = nullptr;
	OptionButton *select_end = nullptr;

public:
	AnimationNodeAnimationEditorDialog();
	~AnimationNodeAnimationEditorDialog();
};

class AnimationNodeAnimationEditor : public VBoxContainer {
	GDCLASS(AnimationNodeAnimationEditor, VBoxContainer);

	Ref<AnimationNodeAnimation> animation_node_animation;
	Button *button = nullptr;
	AnimationNodeAnimationEditorDialog *dialog = nullptr;
	void _open_set_custom_timeline_from_marker_dialog();
	void _validate_markers(int p_id);
	void _confirm_set_custom_timeline_from_marker_dialog();

public:
	AnimationNodeAnimationEditor(Ref<AnimationNodeAnimation> p_animation_node_animation);
	~AnimationNodeAnimationEditor();

protected:
	void _notification(int p_what);
};

#endif // ANIMATION_BLEND_TREE_EDITOR_PLUGIN_H

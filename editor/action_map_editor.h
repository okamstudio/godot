/**************************************************************************/
/*  action_map_editor.h                                                   */
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

#include "scene/gui/control.h"

class Button;
class HBoxContainer;
class EventListenerLineEdit;
class LineEdit;
class CheckButton;
class AcceptDialog;
class InputEventConfigurationDialog;
class Tree;

class ActionMapEditor : public Control {
	GDCLASS(ActionMapEditor, Control);

public:
	struct ActionInfo {
		String name;
		Dictionary action;
		bool has_initial = false;
		Dictionary action_initial;

		Ref<Texture2D> icon;
		bool editable = true;
	};

private:
	enum ItemButton {
		BUTTON_ADD_EVENT,
		BUTTON_EDIT_EVENT,
		BUTTON_REMOVE_ACTION,
		BUTTON_REMOVE_EVENT,
		BUTTON_REVERT_ACTION,
	};

	Vector<ActionInfo> actions_cache;
	Tree *action_tree = nullptr;

	// Storing which action/event is currently being edited in the InputEventConfigurationDialog.

	Dictionary current_action;
	String current_action_name;
	int current_action_event_index = -1;

	// Popups

	InputEventConfigurationDialog *event_config_dialog = nullptr;
	AcceptDialog *message = nullptr;

	// Filtering and Adding actions

	bool show_builtin_actions = false;
	CheckButton *show_builtin_actions_checkbutton = nullptr;
	LineEdit *action_list_search = nullptr;
	EventListenerLineEdit *action_list_search_by_event = nullptr;
	Button *clear_all_search = nullptr;

	HBoxContainer *add_hbox = nullptr;
	LineEdit *add_edit = nullptr;
	Button *add_button = nullptr;

	void _event_config_confirmed();

	void _add_action_pressed();
	void _add_edit_text_changed(const String &p_name);
	String _check_new_action_name(const String &p_name);
	bool _has_action(const String &p_name) const;
	void _add_action(const String &p_name);
	void _action_edited();

	void _tree_button_pressed(Object *p_item, int p_column, int p_id, MouseButton p_button);
	void _tree_item_activated();
	void _search_term_updated(const String &p_search_term);
	void _search_by_event(const Ref<InputEvent> &p_event);
	bool _should_display_action(const String &p_name, const Array &p_events) const;

	Variant get_drag_data_fw(const Point2 &p_point, Control *p_from);
	bool can_drop_data_fw(const Point2 &p_point, const Variant &p_data, Control *p_from) const;
	void drop_data_fw(const Point2 &p_point, const Variant &p_data, Control *p_from);

	void _on_filter_focused();
	void _on_filter_unfocused();

protected:
	void _notification(int p_what);
	static void _bind_methods();

public:
	LineEdit *get_search_box() const;
	LineEdit *get_path_box() const;
	InputEventConfigurationDialog *get_configuration_dialog();

	// Dictionary represents an Action with "events" (Array) and "deadzone" (float) items. Pass with no param to update list from cached action map.
	void update_action_list(const Vector<ActionInfo> &p_action_infos = Vector<ActionInfo>());
	void show_message(const String &p_message);

	void set_show_builtin_actions(bool p_show);

	void use_external_search_box(LineEdit *p_searchbox);

	ActionMapEditor();
};

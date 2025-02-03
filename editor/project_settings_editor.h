/**************************************************************************/
/*  project_settings_editor.h                                             */
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

#ifndef PROJECT_SETTINGS_EDITOR_H
#define PROJECT_SETTINGS_EDITOR_H

#include "core/undo_redo.h"
#include "editor/editor_autoload_settings.h"
#include "editor/editor_data.h"
#include "editor/editor_locale_dialog.h"
#include "editor/editor_plugin_settings.h"
#include "editor/editor_sectioned_inspector.h"
#include "editor/group_settings_editor.h"
#include "editor/import_defaults_editor.h"
#include "scene/gui/dialogs.h"
#include "scene/gui/tab_container.h"

class ProjectSettingsEditor : public AcceptDialog {
	GDCLASS(ProjectSettingsEditor, AcceptDialog);

	enum InputType {
		INPUT_KEY_PHYSICAL,
		INPUT_KEY,
		INPUT_JOY_BUTTON,
		INPUT_JOY_MOTION,
		INPUT_MOUSE_BUTTON
	};

	TabContainer *tab_container;
	VBoxContainer *general_editor;

	Timer *timer;
	InputType add_type;
	String add_at;
	int edit_idx;

	EditorData *data;
	UndoRedo *undo_redo;
	SectionedInspector *globals_editor;

	HBoxContainer *search_bar;
	Button *search_button;
	LineEdit *search_box;

	HBoxContainer *add_prop_bar;
	AcceptDialog *message;
	LineEdit *property;
	OptionButton *type_box;
	PopupMenu *popup_add;
	ConfirmationDialog *press_a_key;
	bool press_a_key_physical;
	Label *press_a_key_label;
	ConfirmationDialog *device_input;
	OptionButton *device_id;
	OptionButton *device_index;
	Label *device_index_label;
	MenuButton *popup_copy_to_feature;

	LineEdit *action_name;
	Button *action_add;
	CheckButton *show_builtin_actions_checkbutton;
	Tree *input_editor;
	bool setting;
	bool updating_translations;
	bool show_builtin_actions;

	Ref<InputEventKey> last_wait_for_key;

	EditorLocaleDialog *locale_select;
	EditorFileDialog *translation_file_open;
	Tree *translation_list;

	Button *translation_res_option_add_button;
	EditorFileDialog *translation_res_file_open;
	EditorFileDialog *translation_res_option_file_open;
	Tree *translation_remap;
	Tree *translation_remap_options;

	EditorAutoloadSettings *autoload_settings;

	EditorPluginSettings *plugin_settings;

	GroupSettingsEditor *group_settings;

	void _item_selected(const String &);
	void _item_adds(String);
	void _item_add();
	void _item_del();
	void _update_actions();
	void _save();
	void _add_item(int p_item, Ref<InputEvent> p_exiting_event = nullptr);
	void _edit_item(Ref<InputEvent> p_exiting_event);

	void _action_check(String p_action);
	void _action_adds(String);
	void _action_add();
	void _device_input_add();
	void _set_show_builtin_actions(bool p_show);

	void _item_checked(const String &p_item, bool p_check);
	void _action_selected();
	void _action_edited();
	void _action_activated();
	void _action_button_pressed(Object *p_obj, int p_column, int p_id);
	void _wait_for_key(const Ref<InputEvent> &p_event);
	void _press_a_key_confirm();
	void _show_last_added(const Ref<InputEvent> &p_event, const String &p_name);

	void _settings_prop_edited(const String &p_name);
	void _settings_changed();

	void _copy_to_platform(int p_which);

	void _translation_file_open();
	void _translation_add(const PoolStringArray &p_paths);
	void _translation_delete(Object *p_item, int p_column, int p_button);
	void _update_translations();

	void _translation_res_file_open();
	void _translation_res_add(const PoolStringArray &p_paths);
	void _translation_res_delete(Object *p_item, int p_column, int p_button);
	void _translation_res_select();
	void _translation_res_option_file_open();
	void _translation_res_option_add(const PoolStringArray &p_paths);
	void _translation_res_option_changed();
	void _translation_res_option_delete(Object *p_item, int p_column, int p_button);
	void _translation_res_option_popup(bool p_arrow_clicked);
	void _translation_res_option_selected(const String &p_locale);

	void _toggle_search_bar(bool p_pressed);

	Variant get_drag_data_fw(const Point2 &p_point, Control *p_from);
	bool can_drop_data_fw(const Point2 &p_point, const Variant &p_data, Control *p_from) const;
	void drop_data_fw(const Point2 &p_point, const Variant &p_data, Control *p_from);

	void _copy_to_platform_about_to_show();

	static ProjectSettingsEditor *singleton;

	Label *restart_label;
	TextureRect *restart_icon;
	PanelContainer *restart_container;
	ToolButton *restart_close_button;

	ImportDefaultsEditor *import_defaults_editor;

	void _editor_restart_request();
	void _editor_restart();
	void _editor_restart_close();

	void _update_theme();

protected:
	void _unhandled_input(const Ref<InputEvent> &p_event);
	void _notification(int p_what);
	static void _bind_methods();

	int _get_current_device();
	void _set_current_device(int i_device);
	String _get_device_string(int i_device);

public:
	void add_translation(const String &p_translation);
	static ProjectSettingsEditor *get_singleton() { return singleton; }
	void popup_project_settings();
	void set_plugins_page();
	void set_general_page(const String &p_category);
	void update_plugins();

	EditorAutoloadSettings *get_autoload_settings() { return autoload_settings; }

	GroupSettingsEditor *get_group_settings() { return group_settings; }

	TabContainer *get_tabs();

	void queue_save();

	ProjectSettingsEditor(EditorData *p_data);
};

#endif // PROJECT_SETTINGS_EDITOR_H

/**************************************************************************/
/*  project_settings_editor.cpp                                           */
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

#include "project_settings_editor.h"

#include "core/config/project_settings.h"
#include "editor/editor_inspector.h"
#include "editor/editor_node.h"
#include "editor/editor_settings.h"
#include "editor/editor_string_names.h"
#include "editor/editor_undo_redo_manager.h"
#include "editor/export/editor_export.h"
#include "editor/themes/editor_scale.h"
#include "scene/gui/check_button.h"
#include "servers/movie_writer/movie_writer.h"

ProjectSettingsEditor *ProjectSettingsEditor::singleton = nullptr;

void ProjectSettingsEditor::connect_filesystem_dock_signals(FileSystemDock *p_fs_dock) {
	localization_editor->connect_filesystem_dock_signals(p_fs_dock);
	group_settings->connect_filesystem_dock_signals(p_fs_dock);
}

void ProjectSettingsEditor::popup_project_settings(bool p_clear_filter) {
	// Restore valid window bounds or pop up at default size.
	Rect2 saved_size = EditorSettings::get_singleton()->get_project_metadata("dialog_bounds", "project_settings", Rect2());
	if (saved_size != Rect2()) {
		popup(saved_size);
	} else {
		popup_centered_clamped(Size2(1200, 700) * EDSCALE, 0.8);
	}

	_add_feature_overrides();
	general_settings_inspector->update_category_list();
	set_process_shortcut_input(true);

	localization_editor->update_translations();
	autoload_settings->update_autoload();
	group_settings->update_groups();
	plugin_settings->update_plugins();
	import_defaults_editor->clear();

	if (p_clear_filter) {
		search_box->clear();
	}

	_focus_current_search_box();
}

void ProjectSettingsEditor::queue_save() {
	timer->start();
}

void ProjectSettingsEditor::set_plugins_page() {
	tab_container->set_current_tab(tab_container->get_tab_idx_from_control(plugin_settings));
}

void ProjectSettingsEditor::set_general_page(const String &p_category) {
	tab_container->set_current_tab(tab_container->get_tab_idx_from_control(general_editor));
	general_settings_inspector->set_current_section(p_category);
}

void ProjectSettingsEditor::update_plugins() {
	plugin_settings->update_plugins();
}

void ProjectSettingsEditor::init_autoloads() {
	autoload_settings->init_autoloads();
}

void ProjectSettingsEditor::_setting_edited(const String &p_name) {
	queue_save();
}

void ProjectSettingsEditor::_update_advanced(bool p_is_advanced) {
	custom_properties->set_visible(p_is_advanced);
}

void ProjectSettingsEditor::_advanced_toggled(bool p_button_pressed) {
	EditorSettings::get_singleton()->set("_project_settings_advanced_mode", p_button_pressed);
	EditorSettings::get_singleton()->save();
	_update_advanced(p_button_pressed);
}

void ProjectSettingsEditor::_setting_selected(const String &p_path) {
	if (p_path.is_empty()) {
		return;
	}

	property_box->set_text(general_settings_inspector->get_current_section() + "/" + p_path);

	_update_property_box(); // set_text doesn't trigger text_changed
}

void ProjectSettingsEditor::_add_setting() {
	String setting = _get_setting_name();

	// Initialize the property with the default value for the given type.
	Callable::CallError ce;
	Variant value;
	Variant::construct(Variant::Type(type_box->get_selected_id()), value, nullptr, 0, ce);

	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	undo_redo->create_action(TTR("Add Project Setting"));
	undo_redo->add_do_property(ps, setting, value);
	undo_redo->add_undo_property(ps, setting, ps->has_setting(setting) ? ps->get(setting) : Variant());

	undo_redo->add_do_method(general_settings_inspector, "update_category_list");
	undo_redo->add_undo_method(general_settings_inspector, "update_category_list");
	undo_redo->add_do_method(this, "queue_save");
	undo_redo->add_undo_method(this, "queue_save");
	undo_redo->commit_action();

	general_settings_inspector->set_current_section(setting.get_slice("/", 1));
	add_button->release_focus();
}

void ProjectSettingsEditor::_delete_setting() {
	String setting = _get_setting_name();
	Variant value = ps->get(setting);
	int order = ps->get_order(setting);

	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	undo_redo->create_action(TTR("Delete Item"));

	undo_redo->add_do_method(ps, "clear", setting);
	undo_redo->add_undo_method(ps, "set", setting, value);
	undo_redo->add_undo_method(ps, "set_order", setting, order);

	undo_redo->add_do_method(general_settings_inspector, "update_category_list");
	undo_redo->add_undo_method(general_settings_inspector, "update_category_list");
	undo_redo->add_do_method(this, "queue_save");
	undo_redo->add_undo_method(this, "queue_save");

	undo_redo->commit_action();

	property_box->clear();
	del_button->release_focus();
}

void ProjectSettingsEditor::_property_box_changed(const String &p_text) {
	_update_property_box();
}

void ProjectSettingsEditor::_feature_selected(int p_index) {
	Vector<String> t = property_box->get_text().strip_edges().split(".", true, 1);
	const String feature = p_index ? "." + feature_box->get_item_text(p_index) : "";
	property_box->set_text(t[0] + feature);
	_update_property_box();
}

void ProjectSettingsEditor::_update_property_box() {
	const String setting = _get_setting_name();
	const Vector<String> t = setting.split(".", true, 1);
	const String &name = t[0];
	const String feature = (t.size() == 2) ? t[1] : "";
	bool feature_invalid = (t.size() == 2) && (t[1].is_empty());

	add_button->set_disabled(true);
	del_button->set_disabled(true);

	if (!feature.is_empty()) {
		feature_invalid = true;
		for (int i = 1; i < feature_box->get_item_count(); i++) {
			if (feature == feature_box->get_item_text(i)) {
				feature_invalid = false;
				feature_box->select(i);
				break;
			}
		}
	}

	if (feature.is_empty() || feature_invalid) {
		feature_box->select(0);
	}

	if (property_box->get_text().is_empty()) {
		return;
	}

	if (ps->has_setting(setting)) {
		del_button->set_disabled(ps->is_builtin_setting(setting));
		_select_type(ps->get_setting(setting).get_type());
	} else {
		if (ps->has_setting(name)) {
			_select_type(ps->get_setting(name).get_type());
		} else {
			type_box->select(0);
		}

		if (feature_invalid) {
			return;
		}

		const Vector<String> names = name.split("/");
		for (int i = 0; i < names.size(); i++) {
			if (!names[i].is_valid_ascii_identifier()) {
				return;
			}
		}

		add_button->set_disabled(false);
	}
}

void ProjectSettingsEditor::_select_type(Variant::Type p_type) {
	type_box->select(type_box->get_item_index(p_type));
}

void ProjectSettingsEditor::shortcut_input(const Ref<InputEvent> &p_event) {
	const Ref<InputEventKey> k = p_event;
	if (k.is_valid() && k->is_pressed()) {
		bool handled = false;

		if (ED_IS_SHORTCUT("ui_undo", p_event)) {
			EditorNode::get_singleton()->undo();
			handled = true;
		}

		if (ED_IS_SHORTCUT("ui_redo", p_event)) {
			EditorNode::get_singleton()->redo();
			handled = true;
		}

		if (ED_IS_SHORTCUT("editor/open_search", p_event)) {
			_focus_current_search_box();
			handled = true;
		}

		if (ED_IS_SHORTCUT("file_dialog/focus_path", p_event)) {
			_focus_current_path_box();
			handled = true;
		}

		if (handled) {
			set_input_as_handled();
		}
	}
}

String ProjectSettingsEditor::_get_setting_name() const {
	String name = property_box->get_text().strip_edges();
	if (!name.begins_with("_") && !name.contains("/")) {
		name = "global/" + name;
	}
	return name;
}

void ProjectSettingsEditor::_add_feature_overrides() {
	HashSet<String> presets;

	presets.insert("bptc");
	presets.insert("s3tc");
	presets.insert("etc2");
	presets.insert("editor");
	presets.insert("editor_hint");
	presets.insert("editor_runtime");
	presets.insert("template_debug");
	presets.insert("template_release");
	presets.insert("debug");
	presets.insert("release");
	presets.insert("template");
	presets.insert("double");
	presets.insert("single");
	presets.insert("32");
	presets.insert("64");
	presets.insert("movie");

	EditorExport *ee = EditorExport::get_singleton();

	for (int i = 0; i < ee->get_export_platform_count(); i++) {
		List<String> p;
		ee->get_export_platform(i)->get_platform_features(&p);
		for (const String &E : p) {
			presets.insert(E);
		}
	}

	for (int i = 0; i < ee->get_export_preset_count(); i++) {
		List<String> p;
		ee->get_export_preset(i)->get_platform()->get_preset_features(ee->get_export_preset(i), &p);
		for (const String &E : p) {
			presets.insert(E);
		}

		String custom = ee->get_export_preset(i)->get_custom_features();
		Vector<String> custom_list = custom.split(",");
		for (int j = 0; j < custom_list.size(); j++) {
			String f = custom_list[j].strip_edges();
			if (!f.is_empty()) {
				presets.insert(f);
			}
		}
	}

	feature_box->clear();
	feature_box->add_item(TTR("(All)"), 0); // So it is always on top.
	int id = 1;
	for (const String &E : presets) {
		feature_box->add_item(E, id++);
	}
}

void ProjectSettingsEditor::_tabs_tab_changed(int p_tab) {
	_focus_current_search_box();
}

void ProjectSettingsEditor::_focus_current_search_box() {
	Control *tab = tab_container->get_current_tab_control();
	LineEdit *current_search_box = nullptr;
	if (tab == general_editor) {
		current_search_box = search_box;
	} else if (tab == action_map_editor) {
		current_search_box = action_map_editor->get_search_box();
	}

	if (current_search_box) {
		current_search_box->grab_focus();
		current_search_box->select_all();
	}
}

void ProjectSettingsEditor::_focus_current_path_box() {
	Control *tab = tab_container->get_current_tab_control();
	LineEdit *current_path_box = nullptr;
	if (tab == general_editor) {
		current_path_box = property_box;
	} else if (tab == action_map_editor) {
		current_path_box = action_map_editor->get_path_box();
	} else if (tab == autoload_settings) {
		current_path_box = autoload_settings->get_path_box();
	} else if (tab == shaders_global_shader_uniforms_editor) {
		current_path_box = shaders_global_shader_uniforms_editor->get_name_box();
	} else if (tab == group_settings) {
		current_path_box = group_settings->get_name_box();
	}

	if (current_path_box) {
		current_path_box->grab_focus();
		current_path_box->select_all();
	}
}

void ProjectSettingsEditor::_editor_restart() {
	ProjectSettings::get_singleton()->save();
	EditorNode::get_singleton()->save_all_scenes();
	EditorNode::get_singleton()->restart_editor();
}

void ProjectSettingsEditor::_editor_restart_request() {
	restart_container->show();
}

void ProjectSettingsEditor::_editor_restart_close() {
	restart_container->hide();
}

void ProjectSettingsEditor::_action_added(const String &p_name) {
	String name = "input/" + p_name;

	ERR_FAIL_COND_MSG(ProjectSettings::get_singleton()->has_setting(name),
			"An action with this name already exists.");

	Dictionary action;
	action["events"] = Array();
	action["deadzone"] = 0.2f;

	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	undo_redo->create_action(TTR("Add Input Action"));
	undo_redo->add_do_method(ProjectSettings::get_singleton(), "set", name, action);
	undo_redo->add_undo_method(ProjectSettings::get_singleton(), "clear", name);

	undo_redo->add_do_method(this, "_update_action_map_editor");
	undo_redo->add_undo_method(this, "_update_action_map_editor");
	undo_redo->add_do_method(this, "queue_save");
	undo_redo->add_undo_method(this, "queue_save");
	undo_redo->commit_action();
}

void ProjectSettingsEditor::_action_edited(const String &p_name, const Dictionary &p_action) {
	const String property_name = "input/" + p_name;
	Dictionary old_val = GLOBAL_GET(property_name);

	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	if (old_val["deadzone"] != p_action["deadzone"]) {
		// Deadzone Changed
		undo_redo->create_action(TTR("Change Action deadzone"));
		undo_redo->add_do_method(ProjectSettings::get_singleton(), "set", property_name, p_action);
		undo_redo->add_undo_method(ProjectSettings::get_singleton(), "set", property_name, old_val);

	} else {
		// Events changed
		undo_redo->create_action(TTR("Change Input Action Event(s)"));
		undo_redo->add_do_method(ProjectSettings::get_singleton(), "set", property_name, p_action);
		undo_redo->add_undo_method(ProjectSettings::get_singleton(), "set", property_name, old_val);
	}

	undo_redo->add_do_method(this, "_update_action_map_editor");
	undo_redo->add_undo_method(this, "_update_action_map_editor");
	undo_redo->add_do_method(this, "queue_save");
	undo_redo->add_undo_method(this, "queue_save");
	undo_redo->commit_action();
}

void ProjectSettingsEditor::_action_removed(const String &p_name) {
	const String property_name = "input/" + p_name;

	Dictionary old_val = GLOBAL_GET(property_name);
	int order = ProjectSettings::get_singleton()->get_order(property_name);

	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	undo_redo->create_action(TTR("Erase Input Action"));
	undo_redo->add_do_method(ProjectSettings::get_singleton(), "clear", property_name);
	undo_redo->add_undo_method(ProjectSettings::get_singleton(), "set", property_name, old_val);
	undo_redo->add_undo_method(ProjectSettings::get_singleton(), "set_order", property_name, order);

	undo_redo->add_do_method(this, "_update_action_map_editor");
	undo_redo->add_undo_method(this, "_update_action_map_editor");
	undo_redo->add_do_method(this, "queue_save");
	undo_redo->add_undo_method(this, "queue_save");
	undo_redo->commit_action();
}

void ProjectSettingsEditor::_action_renamed(const String &p_old_name, const String &p_new_name) {
	const String old_property_name = "input/" + p_old_name;
	const String new_property_name = "input/" + p_new_name;

	ERR_FAIL_COND_MSG(ProjectSettings::get_singleton()->has_setting(new_property_name),
			"An action with this name already exists.");

	int order = ProjectSettings::get_singleton()->get_order(old_property_name);
	Dictionary action = GLOBAL_GET(old_property_name);

	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	undo_redo->create_action(TTR("Rename Input Action"));
	// Do: clear old, set new
	undo_redo->add_do_method(ProjectSettings::get_singleton(), "clear", old_property_name);
	undo_redo->add_do_method(ProjectSettings::get_singleton(), "set", new_property_name, action);
	undo_redo->add_do_method(ProjectSettings::get_singleton(), "set_order", new_property_name, order);
	// Undo: clear new, set old
	undo_redo->add_undo_method(ProjectSettings::get_singleton(), "clear", new_property_name);
	undo_redo->add_undo_method(ProjectSettings::get_singleton(), "set", old_property_name, action);
	undo_redo->add_undo_method(ProjectSettings::get_singleton(), "set_order", old_property_name, order);

	undo_redo->add_do_method(this, "_update_action_map_editor");
	undo_redo->add_undo_method(this, "_update_action_map_editor");
	undo_redo->add_do_method(this, "queue_save");
	undo_redo->add_undo_method(this, "queue_save");
	undo_redo->commit_action();
}

void ProjectSettingsEditor::_action_reordered(const String &p_action_name, const String &p_relative_to, bool p_before) {
	const String action_name = "input/" + p_action_name;
	const String target_name = "input/" + p_relative_to;

	// It is much easier to rebuild the custom "input" properties rather than messing around with the "order" values of them.
	Variant action_value = ps->get(action_name);
	Variant target_value = ps->get(target_name);

	List<PropertyInfo> props;
	HashMap<String, Variant> action_values;
	ProjectSettings::get_singleton()->get_property_list(&props);

	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	undo_redo->create_action(TTR("Update Input Action Order"));

	for (const PropertyInfo &prop : props) {
		// Skip builtins and non-inputs
		// Order matters here, checking for "input/" filters out properties that aren't settings and produce errors in is_builtin_setting().
		if (!prop.name.begins_with("input/") || ProjectSettings::get_singleton()->is_builtin_setting(prop.name)) {
			continue;
		}

		action_values.insert(prop.name, ps->get(prop.name));

		undo_redo->add_do_method(ProjectSettings::get_singleton(), "clear", prop.name);
		undo_redo->add_undo_method(ProjectSettings::get_singleton(), "clear", prop.name);
	}

	for (const KeyValue<String, Variant> &E : action_values) {
		String name = E.key;
		const Variant &value = E.value;

		if (name == target_name) {
			if (p_before) {
				// Insert before target
				undo_redo->add_do_method(ProjectSettings::get_singleton(), "set", action_name, action_value);
				undo_redo->add_do_method(ProjectSettings::get_singleton(), "set", target_name, target_value);

				undo_redo->add_undo_method(ProjectSettings::get_singleton(), "set", target_name, target_value);
				undo_redo->add_undo_method(ProjectSettings::get_singleton(), "set", action_name, action_value);
			} else {
				// Insert after target
				undo_redo->add_do_method(ProjectSettings::get_singleton(), "set", target_name, target_value);
				undo_redo->add_do_method(ProjectSettings::get_singleton(), "set", action_name, action_value);

				undo_redo->add_undo_method(ProjectSettings::get_singleton(), "set", action_name, action_value);
				undo_redo->add_undo_method(ProjectSettings::get_singleton(), "set", target_name, target_value);
			}

		} else if (name != action_name) {
			undo_redo->add_do_method(ProjectSettings::get_singleton(), "set", name, value);
			undo_redo->add_undo_method(ProjectSettings::get_singleton(), "set", name, value);
		}
	}

	undo_redo->add_do_method(this, "_update_action_map_editor");
	undo_redo->add_undo_method(this, "_update_action_map_editor");
	undo_redo->add_do_method(this, "queue_save");
	undo_redo->add_undo_method(this, "queue_save");
	undo_redo->commit_action();
}

void ProjectSettingsEditor::_update_action_map_editor() {
	Vector<ActionMapEditor::ActionInfo> actions;

	List<PropertyInfo> props;
	ProjectSettings::get_singleton()->get_property_list(&props);

	const Ref<Texture2D> builtin_icon = get_editor_theme_icon(SNAME("PinPressed"));
	for (const PropertyInfo &E : props) {
		const String property_name = E.name;

		if (!property_name.begins_with("input/")) {
			continue;
		}

		// Strip the "input/" from the left.
		String display_name = property_name.substr(String("input/").size() - 1);
		Dictionary action = GLOBAL_GET(property_name);

		ActionMapEditor::ActionInfo action_info;
		action_info.action = action;
		action_info.editable = true;
		action_info.name = display_name;

		const bool is_builtin_input = ProjectSettings::get_singleton()->get_input_presets().find(property_name) != nullptr;
		if (is_builtin_input) {
			action_info.editable = false;
			action_info.icon = builtin_icon;
			action_info.has_initial = true;
			action_info.action_initial = ProjectSettings::get_singleton()->property_get_revert(property_name);
		}

		actions.push_back(action_info);
	}

	action_map_editor->update_action_list(actions);
}

void ProjectSettingsEditor::_update_theme() {
	add_button->set_icon(get_editor_theme_icon(SNAME("Add")));
	del_button->set_icon(get_editor_theme_icon(SNAME("Remove")));
	search_box->set_right_icon(get_editor_theme_icon(SNAME("Search")));
	restart_close_button->set_icon(get_editor_theme_icon(SNAME("Close")));
	restart_container->add_theme_style_override(SceneStringName(panel), get_theme_stylebox(SceneStringName(panel), SNAME("Tree")));
	restart_icon->set_texture(get_editor_theme_icon(SNAME("StatusWarning")));
	restart_label->add_theme_color_override(SceneStringName(font_color), get_theme_color(SNAME("warning_color"), EditorStringName(Editor)));

	type_box->clear();
	for (int i = 0; i < Variant::VARIANT_MAX; i++) {
		if (i == Variant::NIL || i == Variant::OBJECT || i == Variant::CALLABLE || i == Variant::SIGNAL || i == Variant::RID) {
			// These types can't be serialized properly, so skip them.
			continue;
		}
		String type = Variant::get_type_name(Variant::Type(i));
		type_box->add_icon_item(get_editor_theme_icon(type), type, i);
	}
}

void ProjectSettingsEditor::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_VISIBILITY_CHANGED: {
			if (!is_visible()) {
				EditorSettings::get_singleton()->set_project_metadata("dialog_bounds", "project_settings", Rect2(get_position(), get_size()));
			}
		} break;

		case NOTIFICATION_ENTER_TREE: {
			general_settings_inspector->edit(ps);
			_update_action_map_editor();
			_update_theme();
		} break;

		case NOTIFICATION_THEME_CHANGED: {
			_update_theme();
		} break;
	}
}

void ProjectSettingsEditor::_bind_methods() {
	ClassDB::bind_method(D_METHOD("queue_save"), &ProjectSettingsEditor::queue_save);

	ClassDB::bind_method(D_METHOD("_update_action_map_editor"), &ProjectSettingsEditor::_update_action_map_editor);
}

ProjectSettingsEditor::ProjectSettingsEditor(EditorData *p_data) {
	singleton = this;
	set_title(TTR("Project Settings (project.godot)"));
	set_clamp_to_embedder(true);

	ps = ProjectSettings::get_singleton();
	data = p_data;

	tab_container = memnew(TabContainer);
	tab_container->set_use_hidden_tabs_for_min_size(true);
	tab_container->set_theme_type_variation("TabContainerOdd");
	tab_container->connect("tab_changed", callable_mp(this, &ProjectSettingsEditor::_tabs_tab_changed));
	add_child(tab_container);

	general_editor = memnew(VBoxContainer);
	general_editor->set_name(TTR("General"));
	general_editor->set_alignment(BoxContainer::ALIGNMENT_BEGIN);
	general_editor->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	tab_container->add_child(general_editor);

	HBoxContainer *search_bar = memnew(HBoxContainer);
	general_editor->add_child(search_bar);

	search_box = memnew(LineEdit);
	search_box->set_placeholder(TTR("Filter Settings"));
	search_box->set_clear_button_enabled(true);
	search_box->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	search_bar->add_child(search_box);

	advanced = memnew(CheckButton);
	advanced->set_text(TTR("Advanced Settings"));
	advanced->connect(SceneStringName(toggled), callable_mp(this, &ProjectSettingsEditor::_advanced_toggled));
	search_bar->add_child(advanced);

	custom_properties = memnew(HBoxContainer);
	general_editor->add_child(custom_properties);

	property_box = memnew(LineEdit);
	property_box->set_placeholder(TTR("Select a Setting or Type its Name"));
	property_box->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	property_box->connect(SceneStringName(text_changed), callable_mp(this, &ProjectSettingsEditor::_property_box_changed));
	custom_properties->add_child(property_box);

	feature_box = memnew(OptionButton);
	feature_box->set_custom_minimum_size(Size2(120, 0) * EDSCALE);
	feature_box->connect(SceneStringName(item_selected), callable_mp(this, &ProjectSettingsEditor::_feature_selected));
	custom_properties->add_child(feature_box);

	type_box = memnew(OptionButton);
	type_box->set_custom_minimum_size(Size2(120, 0) * EDSCALE);
	custom_properties->add_child(type_box);

	add_button = memnew(Button);
	add_button->set_text(TTR("Add"));
	add_button->set_disabled(true);
	add_button->connect(SceneStringName(pressed), callable_mp(this, &ProjectSettingsEditor::_add_setting));
	custom_properties->add_child(add_button);

	del_button = memnew(Button);
	del_button->set_text(TTR("Delete"));
	del_button->set_disabled(true);
	del_button->connect(SceneStringName(pressed), callable_mp(this, &ProjectSettingsEditor::_delete_setting));
	custom_properties->add_child(del_button);

	general_settings_inspector = memnew(SectionedInspector);
	general_settings_inspector->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	general_settings_inspector->register_search_box(search_box);
	general_settings_inspector->register_advanced_toggle(advanced);
	general_settings_inspector->get_inspector()->set_use_filter(true);
	general_settings_inspector->get_inspector()->connect("property_selected", callable_mp(this, &ProjectSettingsEditor::_setting_selected));
	general_settings_inspector->get_inspector()->connect("property_edited", callable_mp(this, &ProjectSettingsEditor::_setting_edited));
	general_settings_inspector->get_inspector()->connect("restart_requested", callable_mp(this, &ProjectSettingsEditor::_editor_restart_request));
	general_editor->add_child(general_settings_inspector);

	restart_container = memnew(PanelContainer);
	general_editor->add_child(restart_container);

	HBoxContainer *restart_hb = memnew(HBoxContainer);
	restart_container->hide();
	restart_container->add_child(restart_hb);

	restart_icon = memnew(TextureRect);
	restart_icon->set_v_size_flags(Control::SIZE_SHRINK_CENTER);
	restart_hb->add_child(restart_icon);

	restart_label = memnew(Label);
	restart_label->set_text(TTR("Changed settings will be applied to the editor after restarting."));
	restart_hb->add_child(restart_label);
	restart_hb->add_spacer();

	Button *restart_button = memnew(Button);
	restart_button->connect(SceneStringName(pressed), callable_mp(this, &ProjectSettingsEditor::_editor_restart));
	restart_hb->add_child(restart_button);
	restart_button->set_text(TTR("Save & Restart"));

	restart_close_button = memnew(Button);
	restart_close_button->set_flat(true);
	restart_close_button->connect(SceneStringName(pressed), callable_mp(this, &ProjectSettingsEditor::_editor_restart_close));
	restart_hb->add_child(restart_close_button);

	action_map_editor = memnew(ActionMapEditor);
	action_map_editor->set_name(TTR("Input Map"));
	action_map_editor->connect("action_added", callable_mp(this, &ProjectSettingsEditor::_action_added));
	action_map_editor->connect("action_edited", callable_mp(this, &ProjectSettingsEditor::_action_edited));
	action_map_editor->connect("action_removed", callable_mp(this, &ProjectSettingsEditor::_action_removed));
	action_map_editor->connect("action_renamed", callable_mp(this, &ProjectSettingsEditor::_action_renamed));
	action_map_editor->connect("action_reordered", callable_mp(this, &ProjectSettingsEditor::_action_reordered));
	action_map_editor->connect(SNAME("filter_focused"), callable_mp((AcceptDialog *)this, &AcceptDialog::set_close_on_escape).bind(false));
	action_map_editor->connect(SNAME("filter_unfocused"), callable_mp((AcceptDialog *)this, &AcceptDialog::set_close_on_escape).bind(true));
	tab_container->add_child(action_map_editor);

	localization_editor = memnew(LocalizationEditor);
	localization_editor->set_name(TTR("Localization"));
	localization_editor->connect("localization_changed", callable_mp(this, &ProjectSettingsEditor::queue_save));
	tab_container->add_child(localization_editor);

	TabContainer *globals_container = memnew(TabContainer);
	globals_container->set_name(TTR("Globals"));
	tab_container->add_child(globals_container);

	autoload_settings = memnew(EditorAutoloadSettings);
	autoload_settings->set_name(TTR("Autoload"));
	autoload_settings->connect("autoload_changed", callable_mp(this, &ProjectSettingsEditor::queue_save));
	globals_container->add_child(autoload_settings);

	shaders_global_shader_uniforms_editor = memnew(ShaderGlobalsEditor);
	shaders_global_shader_uniforms_editor->set_name(TTR("Shader Globals"));
	shaders_global_shader_uniforms_editor->connect("globals_changed", callable_mp(this, &ProjectSettingsEditor::queue_save));
	globals_container->add_child(shaders_global_shader_uniforms_editor);

	group_settings = memnew(GroupSettingsEditor);
	group_settings->set_name(TTR("Groups"));
	group_settings->connect("group_changed", callable_mp(this, &ProjectSettingsEditor::queue_save));
	globals_container->add_child(group_settings);

	plugin_settings = memnew(EditorPluginSettings);
	plugin_settings->set_name(TTR("Plugins"));
	tab_container->add_child(plugin_settings);

	timer = memnew(Timer);
	timer->set_wait_time(1.5);
	timer->connect("timeout", callable_mp(ps, &ProjectSettings::save));
	timer->set_one_shot(true);
	add_child(timer);

	set_ok_button_text(TTR("Close"));
	set_hide_on_ok(true);

	bool use_advanced = EDITOR_DEF("_project_settings_advanced_mode", false);
	if (use_advanced) {
		advanced->set_pressed(true);
	}

	_update_advanced(use_advanced);

	import_defaults_editor = memnew(ImportDefaultsEditor);
	import_defaults_editor->set_name(TTR("Import Defaults"));
	tab_container->add_child(import_defaults_editor);

	MovieWriter::set_extensions_hint(); // ensure extensions are properly displayed.
}

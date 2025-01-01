/**************************************************************************/
/*  editor_settings_dialog.cpp                                            */
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

#include "editor_settings_dialog.h"

#include "core/input/input_map.h"
#include "core/os/keyboard.h"
#include "editor/debugger/editor_debugger_node.h"
#include "editor/editor_inspector.h"
#include "editor/editor_log.h"
#include "editor/editor_node.h"
#include "editor/editor_property_name_processor.h"
#include "editor/editor_sectioned_inspector.h"
#include "editor/editor_settings.h"
#include "editor/editor_string_names.h"
#include "editor/editor_undo_redo_manager.h"
#include "editor/event_listener_line_edit.h"
#include "editor/input_event_configuration_dialog.h"
#include "editor/plugins/node_3d_editor_plugin.h"
#include "editor/themes/editor_scale.h"
#include "editor/themes/editor_theme_manager.h"
#include "scene/gui/check_button.h"
#include "scene/gui/panel_container.h"
#include "scene/gui/tab_container.h"
#include "scene/gui/texture_rect.h"
#include "scene/gui/tree.h"

void EditorSettingsDialog::ok_pressed() {
	if (!EditorSettings::get_singleton()) {
		return;
	}

	_settings_save();
	timer->stop();
}

void EditorSettingsDialog::_settings_changed() {
	timer->start();
}

void EditorSettingsDialog::_settings_property_edited(const String &p_name) {
	String full_name = inspector->get_full_item_path(p_name);

	// Set theme presets to Custom when controlled settings change.

	if (full_name == "interface/theme/accent_color" || full_name == "interface/theme/base_color" || full_name == "interface/theme/contrast" || full_name == "interface/theme/draw_extra_borders") {
		EditorSettings::get_singleton()->set_manually("interface/theme/preset", "Custom");
	} else if (full_name == "interface/theme/base_spacing" || full_name == "interface/theme/additional_spacing") {
		EditorSettings::get_singleton()->set_manually("interface/theme/spacing_preset", "Custom");
	} else if (full_name.begins_with("text_editor/theme/highlighting")) {
		EditorSettings::get_singleton()->set_manually("text_editor/theme/color_theme", "Custom");
	} else if (full_name.begins_with("editors/visual_editors/connection_colors") || full_name.begins_with("editors/visual_editors/category_colors")) {
		EditorSettings::get_singleton()->set_manually("editors/visual_editors/color_theme", "Custom");
	} else if (full_name == "editors/3d/navigation/orbit_mouse_button" || full_name == "editors/3d/navigation/pan_mouse_button" || full_name == "editors/3d/navigation/zoom_mouse_button" || full_name == "editors/3d/navigation/emulate_3_button_mouse") {
		EditorSettings::get_singleton()->set_manually("editors/3d/navigation/navigation_scheme", (int)Node3DEditorViewport::NAVIGATION_CUSTOM);
	} else if (full_name == "editors/3d/navigation/navigation_scheme") {
		update_navigation_preset();
	}
}

void EditorSettingsDialog::update_navigation_preset() {
	Node3DEditorViewport::NavigationScheme nav_scheme = (Node3DEditorViewport::NavigationScheme)EDITOR_GET("editors/3d/navigation/navigation_scheme").operator int();
	Node3DEditorViewport::ViewportNavMouseButton set_orbit_mouse_button = Node3DEditorViewport::NAVIGATION_LEFT_MOUSE;
	Node3DEditorViewport::ViewportNavMouseButton set_pan_mouse_button = Node3DEditorViewport::NAVIGATION_LEFT_MOUSE;
	Node3DEditorViewport::ViewportNavMouseButton set_zoom_mouse_button = Node3DEditorViewport::NAVIGATION_LEFT_MOUSE;
	bool set_3_button_mouse = false;
	Ref<InputEventKey> orbit_mod_key_1;
	Ref<InputEventKey> orbit_mod_key_2;
	Ref<InputEventKey> pan_mod_key_1;
	Ref<InputEventKey> pan_mod_key_2;
	Ref<InputEventKey> zoom_mod_key_1;
	Ref<InputEventKey> zoom_mod_key_2;
	bool set_preset = false;

	if (nav_scheme == Node3DEditorViewport::NAVIGATION_GODOT) {
		set_preset = true;
		set_orbit_mouse_button = Node3DEditorViewport::NAVIGATION_MIDDLE_MOUSE;
		set_pan_mouse_button = Node3DEditorViewport::NAVIGATION_MIDDLE_MOUSE;
		set_zoom_mouse_button = Node3DEditorViewport::NAVIGATION_MIDDLE_MOUSE;
		set_3_button_mouse = false;
		orbit_mod_key_1 = InputEventKey::create_reference(Key::NONE);
		orbit_mod_key_2 = InputEventKey::create_reference(Key::NONE);
		pan_mod_key_1 = InputEventKey::create_reference(Key::SHIFT);
		pan_mod_key_2 = InputEventKey::create_reference(Key::NONE);
		zoom_mod_key_1 = InputEventKey::create_reference(Key::CTRL);
		zoom_mod_key_2 = InputEventKey::create_reference(Key::NONE);
	} else if (nav_scheme == Node3DEditorViewport::NAVIGATION_MAYA) {
		set_preset = true;
		set_orbit_mouse_button = Node3DEditorViewport::NAVIGATION_LEFT_MOUSE;
		set_pan_mouse_button = Node3DEditorViewport::NAVIGATION_MIDDLE_MOUSE;
		set_zoom_mouse_button = Node3DEditorViewport::NAVIGATION_RIGHT_MOUSE;
		set_3_button_mouse = false;
		orbit_mod_key_1 = InputEventKey::create_reference(Key::ALT);
		orbit_mod_key_2 = InputEventKey::create_reference(Key::NONE);
		pan_mod_key_1 = InputEventKey::create_reference(Key::NONE);
		pan_mod_key_2 = InputEventKey::create_reference(Key::NONE);
		zoom_mod_key_1 = InputEventKey::create_reference(Key::ALT);
		zoom_mod_key_2 = InputEventKey::create_reference(Key::NONE);
	} else if (nav_scheme == Node3DEditorViewport::NAVIGATION_MODO) {
		set_preset = true;
		set_orbit_mouse_button = Node3DEditorViewport::NAVIGATION_LEFT_MOUSE;
		set_pan_mouse_button = Node3DEditorViewport::NAVIGATION_LEFT_MOUSE;
		set_zoom_mouse_button = Node3DEditorViewport::NAVIGATION_LEFT_MOUSE;
		set_3_button_mouse = false;
		orbit_mod_key_1 = InputEventKey::create_reference(Key::ALT);
		orbit_mod_key_2 = InputEventKey::create_reference(Key::NONE);
		pan_mod_key_1 = InputEventKey::create_reference(Key::SHIFT);
		pan_mod_key_2 = InputEventKey::create_reference(Key::ALT);
		zoom_mod_key_1 = InputEventKey::create_reference(Key::ALT);
		zoom_mod_key_2 = InputEventKey::create_reference(Key::CTRL);
	} else if (nav_scheme == Node3DEditorViewport::NAVIGATION_TABLET) {
		set_preset = true;
		set_orbit_mouse_button = Node3DEditorViewport::NAVIGATION_MIDDLE_MOUSE;
		set_pan_mouse_button = Node3DEditorViewport::NAVIGATION_MIDDLE_MOUSE;
		set_zoom_mouse_button = Node3DEditorViewport::NAVIGATION_MIDDLE_MOUSE;
		set_3_button_mouse = true;
		orbit_mod_key_1 = InputEventKey::create_reference(Key::ALT);
		orbit_mod_key_2 = InputEventKey::create_reference(Key::NONE);
		pan_mod_key_1 = InputEventKey::create_reference(Key::SHIFT);
		pan_mod_key_2 = InputEventKey::create_reference(Key::NONE);
		zoom_mod_key_1 = InputEventKey::create_reference(Key::CTRL);
		zoom_mod_key_2 = InputEventKey::create_reference(Key::NONE);
	}
	// Set settings to the desired preset values.
	if (set_preset) {
		EditorSettings::get_singleton()->set_manually("editors/3d/navigation/orbit_mouse_button", (int)set_orbit_mouse_button);
		EditorSettings::get_singleton()->set_manually("editors/3d/navigation/pan_mouse_button", (int)set_pan_mouse_button);
		EditorSettings::get_singleton()->set_manually("editors/3d/navigation/zoom_mouse_button", (int)set_zoom_mouse_button);
		EditorSettings::get_singleton()->set_manually("editors/3d/navigation/emulate_3_button_mouse", set_3_button_mouse);
		_set_shortcut_input("spatial_editor/viewport_orbit_modifier_1", orbit_mod_key_1);
		_set_shortcut_input("spatial_editor/viewport_orbit_modifier_2", orbit_mod_key_2);
		_set_shortcut_input("spatial_editor/viewport_pan_modifier_1", pan_mod_key_1);
		_set_shortcut_input("spatial_editor/viewport_pan_modifier_2", pan_mod_key_2);
		_set_shortcut_input("spatial_editor/viewport_zoom_modifier_1", zoom_mod_key_1);
		_set_shortcut_input("spatial_editor/viewport_zoom_modifier_2", zoom_mod_key_2);
	}
}

void EditorSettingsDialog::_set_shortcut_input(const String &p_name, Ref<InputEventKey> &p_event) {
	Array sc_events;
	if (p_event->get_keycode() != Key::NONE) {
		sc_events.push_back((Variant)p_event);
	}

	Ref<Shortcut> sc = EditorSettings::get_singleton()->get_shortcut(p_name);
	sc->set_events(sc_events);
}

void EditorSettingsDialog::_settings_save() {
	EditorSettings::get_singleton()->notify_changes();
	EditorSettings::get_singleton()->save();
}

void EditorSettingsDialog::cancel_pressed() {
	if (!EditorSettings::get_singleton()) {
		return;
	}

	EditorSettings::get_singleton()->notify_changes();
}

void EditorSettingsDialog::popup_edit_settings() {
	if (!EditorSettings::get_singleton()) {
		return;
	}

	EditorSettings::get_singleton()->list_text_editor_themes(); // make sure we have an up to date list of themes

	_update_dynamic_property_hints();

	inspector->edit(EditorSettings::get_singleton());
	inspector->get_inspector()->update_tree();

	_update_shortcuts();
	set_process_shortcut_input(true);

	// Restore valid window bounds or pop up at default size.
	Rect2 saved_size = EditorSettings::get_singleton()->get_project_metadata("dialog_bounds", "editor_settings", Rect2());
	if (saved_size != Rect2()) {
		popup(saved_size);
	} else {
		popup_centered_clamped(Size2(900, 700) * EDSCALE, 0.8);
	}

	_focus_current_search_box();
}

void EditorSettingsDialog::_filter_shortcuts(const String &) {
	_update_shortcuts();
}

void EditorSettingsDialog::_filter_shortcuts_by_event(const Ref<InputEvent> &p_event) {
	if (p_event.is_null() || (p_event->is_pressed() && !p_event->is_echo())) {
		_update_shortcuts();
	}
}

void EditorSettingsDialog::_undo_redo_callback(void *p_self, const String &p_name) {
	EditorNode::get_log()->add_message(p_name, EditorLog::MSG_TYPE_EDITOR);
}

void EditorSettingsDialog::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_VISIBILITY_CHANGED: {
			if (!is_visible()) {
				EditorSettings::get_singleton()->set_project_metadata("dialog_bounds", "editor_settings", Rect2(get_position(), get_size()));
				set_process_shortcut_input(false);
			}
		} break;

		case NOTIFICATION_READY: {
			EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
			undo_redo->get_or_create_history(EditorUndoRedoManager::GLOBAL_HISTORY).undo_redo->set_method_notify_callback(EditorDebuggerNode::_methods_changed, nullptr);
			undo_redo->get_or_create_history(EditorUndoRedoManager::GLOBAL_HISTORY).undo_redo->set_property_notify_callback(EditorDebuggerNode::_properties_changed, nullptr);
			undo_redo->get_or_create_history(EditorUndoRedoManager::GLOBAL_HISTORY).undo_redo->set_commit_notify_callback(_undo_redo_callback, this);
		} break;

		case NOTIFICATION_ENTER_TREE: {
			_update_icons();
		} break;

		case EditorSettings::NOTIFICATION_EDITOR_SETTINGS_CHANGED: {
			if (EditorThemeManager::is_generated_theme_outdated()) {
				_update_icons();
			}

			bool update_shortcuts_tab =
					EditorSettings::get_singleton()->check_changed_settings_in_group("shortcuts") ||
					EditorSettings::get_singleton()->check_changed_settings_in_group("builtin_action_overrides");
			if (update_shortcuts_tab) {
				_update_shortcuts();
			}

			if (EditorSettings::get_singleton()->check_changed_settings_in_group("editors/3d/navigation")) {
				// Shortcuts may have changed, so dynamic hint values must update.
				_update_dynamic_property_hints();
				inspector->get_inspector()->update_tree();
			}

			if (EditorSettings::get_singleton()->check_changed_settings_in_group("interface/editor/localize_settings")) {
				inspector->update_category_list();
			}
		} break;
	}
}

void EditorSettingsDialog::shortcut_input(const Ref<InputEvent> &p_event) {
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

		if (k->is_match(InputEventKey::create_reference(KeyModifierMask::CMD_OR_CTRL | Key::F))) {
			_focus_current_search_box();
			handled = true;
		}

		if (handled) {
			set_input_as_handled();
		}
	}
}

void EditorSettingsDialog::_update_icons() {
	search_box->set_right_icon(shortcuts->get_editor_theme_icon(SNAME("Search")));
	search_box->set_clear_button_enabled(true);
	shortcut_search_box->set_right_icon(shortcuts->get_editor_theme_icon(SNAME("Search")));
	shortcut_search_box->set_clear_button_enabled(true);

	restart_close_button->set_button_icon(shortcuts->get_editor_theme_icon(SNAME("Close")));
	restart_container->add_theme_style_override(SceneStringName(panel), shortcuts->get_theme_stylebox(SceneStringName(panel), SNAME("Tree")));
	restart_icon->set_texture(shortcuts->get_editor_theme_icon(SNAME("StatusWarning")));
	restart_label->add_theme_color_override(SceneStringName(font_color), shortcuts->get_theme_color(SNAME("warning_color"), EditorStringName(Editor)));
}

void EditorSettingsDialog::_event_config_confirmed() {
	Ref<InputEventKey> k = shortcut_editor->get_event();
	if (k.is_null()) {
		return;
	}

	if (current_event_index == -1) {
		// Add new event
		current_events.push_back(k);
	} else {
		// Edit existing event
		current_events[current_event_index] = k;
	}

	if (is_editing_action) {
		_update_builtin_action(current_edited_identifier, current_events);
	} else {
		_update_shortcut_events(current_edited_identifier, current_events);
	}
}

void EditorSettingsDialog::_update_builtin_action(const String &p_name, const Array &p_events) {
	Array old_input_array = EditorSettings::get_singleton()->get_builtin_action_overrides(p_name);

	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	undo_redo->create_action(vformat(TTR("Edit Built-in Action: %s"), p_name));
	undo_redo->add_do_method(EditorSettings::get_singleton(), "mark_setting_changed", "builtin_action_overrides");
	undo_redo->add_undo_method(EditorSettings::get_singleton(), "mark_setting_changed", "builtin_action_overrides");
	undo_redo->add_do_method(EditorSettings::get_singleton(), "set_builtin_action_override", p_name, p_events);
	undo_redo->add_undo_method(EditorSettings::get_singleton(), "set_builtin_action_override", p_name, old_input_array);
	undo_redo->add_do_method(this, "_settings_changed");
	undo_redo->add_undo_method(this, "_settings_changed");
	undo_redo->commit_action();

	_update_shortcuts();
}

void EditorSettingsDialog::_update_shortcut_events(const String &p_path, const Array &p_events) {
	Ref<Shortcut> current_sc = EditorSettings::get_singleton()->get_shortcut(p_path);

	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	undo_redo->create_action(vformat(TTR("Edit Shortcut: %s"), p_path), UndoRedo::MERGE_DISABLE, EditorSettings::get_singleton());
	// History must be fixed based on the EditorSettings object because current_sc would
	// incorrectly make this action use the scene history.
	undo_redo->force_fixed_history();
	undo_redo->add_do_method(current_sc.ptr(), "set_events", p_events);
	undo_redo->add_undo_method(current_sc.ptr(), "set_events", current_sc->get_events());
	undo_redo->add_do_method(EditorSettings::get_singleton(), "mark_setting_changed", "shortcuts");
	undo_redo->add_undo_method(EditorSettings::get_singleton(), "mark_setting_changed", "shortcuts");
	undo_redo->add_do_method(this, "_update_shortcuts");
	undo_redo->add_undo_method(this, "_update_shortcuts");
	undo_redo->add_do_method(this, "_settings_changed");
	undo_redo->add_undo_method(this, "_settings_changed");
	undo_redo->commit_action();

	bool path_is_orbit_mod = p_path == "spatial_editor/viewport_orbit_modifier_1" || p_path == "spatial_editor/viewport_orbit_modifier_2";
	bool path_is_pan_mod = p_path == "spatial_editor/viewport_pan_modifier_1" || p_path == "spatial_editor/viewport_pan_modifier_2";
	bool path_is_zoom_mod = p_path == "spatial_editor/viewport_zoom_modifier_1" || p_path == "spatial_editor/viewport_zoom_modifier_2";
	if (path_is_orbit_mod || path_is_pan_mod || path_is_zoom_mod) {
		EditorSettings::get_singleton()->set_manually("editors/3d/navigation/navigation_scheme", (int)Node3DEditorViewport::NAVIGATION_CUSTOM);
	}
}

Array EditorSettingsDialog::_event_list_to_array_helper(const List<Ref<InputEvent>> &p_events) {
	Array events;

	// Convert the list to an array, and only keep key events as this is for the editor.
	for (const List<Ref<InputEvent>>::Element *E = p_events.front(); E; E = E->next()) {
		Ref<InputEventKey> k = E->get();
		if (k.is_valid()) {
			events.append(E->get());
		}
	}

	return events;
}

TreeItem *EditorSettingsDialog::_create_shortcut_treeitem(TreeItem *p_parent, const String &p_shortcut_identifier, const String &p_display, Array &p_events, bool p_allow_revert, bool p_is_action, bool p_is_collapsed) {
	TreeItem *shortcut_item = shortcuts->create_item(p_parent);
	shortcut_item->set_collapsed(p_is_collapsed);
	shortcut_item->set_text(0, p_display);

	Ref<InputEvent> primary = p_events.size() > 0 ? Ref<InputEvent>(p_events[0]) : Ref<InputEvent>();
	Ref<InputEvent> secondary = p_events.size() > 1 ? Ref<InputEvent>(p_events[1]) : Ref<InputEvent>();

	String sc_text = TTRC("None");
	if (primary.is_valid()) {
		sc_text = primary->as_text();

		if (secondary.is_valid()) {
			sc_text += ", " + secondary->as_text();

			if (p_events.size() > 2) {
				sc_text += " (+" + itos(p_events.size() - 2) + ")";
			}
		}
		shortcut_item->set_auto_translate_mode(1, AUTO_TRANSLATE_MODE_DISABLED);
	}

	shortcut_item->set_text(1, sc_text);
	if (sc_text == "None") {
		// Fade out unassigned shortcut labels for easier visual grepping.
		shortcut_item->set_custom_color(1, shortcuts->get_theme_color(SceneStringName(font_color), SNAME("Label")) * Color(1, 1, 1, 0.5));
	}

	if (p_allow_revert) {
		shortcut_item->add_button(1, shortcuts->get_editor_theme_icon(SNAME("Reload")), SHORTCUT_REVERT);
	}

	shortcut_item->add_button(1, shortcuts->get_editor_theme_icon(SNAME("Add")), SHORTCUT_ADD);
	shortcut_item->add_button(1, shortcuts->get_editor_theme_icon(SNAME("Close")), SHORTCUT_ERASE);

	shortcut_item->set_meta("is_action", p_is_action);
	shortcut_item->set_meta("type", "shortcut");
	shortcut_item->set_meta("shortcut_identifier", p_shortcut_identifier);
	shortcut_item->set_meta("events", p_events);

	// Shortcut Input Events
	for (int i = 0; i < p_events.size(); i++) {
		Ref<InputEvent> ie = p_events[i];
		if (ie.is_null()) {
			continue;
		}

		TreeItem *event_item = shortcuts->create_item(shortcut_item);

		event_item->set_text(0, shortcut_item->get_child_count() == 1 ? TTRC("Primary") : "");
		event_item->set_text(1, ie->as_text());
		event_item->set_auto_translate_mode(1, AUTO_TRANSLATE_MODE_DISABLED);

		event_item->add_button(1, shortcuts->get_editor_theme_icon(SNAME("Edit")), SHORTCUT_EDIT);
		event_item->add_button(1, shortcuts->get_editor_theme_icon(SNAME("Close")), SHORTCUT_ERASE);

		event_item->set_custom_bg_color(0, shortcuts->get_theme_color(SNAME("dark_color_3"), EditorStringName(Editor)));
		event_item->set_custom_bg_color(1, shortcuts->get_theme_color(SNAME("dark_color_3"), EditorStringName(Editor)));

		event_item->set_meta("is_action", p_is_action);
		event_item->set_meta("type", "event");
		event_item->set_meta("event_index", i);
	}

	return shortcut_item;
}

bool EditorSettingsDialog::_should_display_shortcut(const String &p_name, const Array &p_events, bool p_match_localized_name) const {
	const Ref<InputEvent> search_ev = shortcut_search_by_event->get_event();
	if (search_ev.is_valid()) {
		bool event_match = false;
		for (int i = 0; i < p_events.size(); ++i) {
			const Ref<InputEvent> ev = p_events[i];
			if (ev.is_valid() && ev->is_match(search_ev, true)) {
				event_match = true;
				break;
			}
		}
		if (!event_match) {
			return false;
		}
	}

	const String &search_text = shortcut_search_box->get_text();
	if (search_text.is_empty()) {
		return true;
	}
	if (search_text.is_subsequence_ofn(p_name)) {
		return true;
	}
	if (p_match_localized_name && search_text.is_subsequence_ofn(TTR(p_name))) {
		return true;
	}

	return false;
}

void EditorSettingsDialog::_update_shortcuts() {
	// Before clearing the tree, take note of which categories are collapsed so that this state can be maintained when the tree is repopulated.
	HashMap<String, bool> collapsed;

	if (shortcuts->get_root() && shortcuts->get_root()->get_first_child()) {
		TreeItem *ti = shortcuts->get_root()->get_first_child();
		while (ti) {
			// Not all items have valid or unique text in the first column - so if it has an identifier, use that, as it should be unique.
			if (ti->get_first_child() && ti->has_meta("shortcut_identifier")) {
				collapsed[ti->get_meta("shortcut_identifier")] = ti->is_collapsed();
			} else {
				collapsed[ti->get_text(0)] = ti->is_collapsed();
			}

			// Try go down tree
			TreeItem *ti_next = ti->get_first_child();
			// Try go to the next node via in-order traversal
			if (!ti_next) {
				ti_next = ti;
				while (ti_next && !ti_next->get_next()) {
					ti_next = ti_next->get_parent();
				}
				if (ti_next) {
					ti_next = ti_next->get_next();
				}
			}

			ti = ti_next;
		}
	}

	shortcuts->clear();

	TreeItem *root = shortcuts->create_item();
	HashMap<String, TreeItem *> sections;

	// Set up section for Common/Built-in actions
	TreeItem *common_section = shortcuts->create_item(root);
	sections["Common"] = common_section;
	common_section->set_text(0, TTRC("Common"));
	common_section->set_selectable(0, false);
	common_section->set_selectable(1, false);
	if (collapsed.has("Common")) {
		common_section->set_collapsed(collapsed["Common"]);
	}
	common_section->set_custom_bg_color(0, shortcuts->get_theme_color(SNAME("prop_subsection"), EditorStringName(Editor)));
	common_section->set_custom_bg_color(1, shortcuts->get_theme_color(SNAME("prop_subsection"), EditorStringName(Editor)));

	// Get the action map for the editor, and add each item to the "Common" section.
	for (const KeyValue<StringName, InputMap::Action> &E : InputMap::get_singleton()->get_action_map()) {
		const String &action_name = E.key;
		const InputMap::Action &action = E.value;

		// Skip non-builtin actions.
		if (!InputMap::get_singleton()->get_builtins_with_feature_overrides_applied().has(action_name)) {
			continue;
		}

		const List<Ref<InputEvent>> &all_default_events = InputMap::get_singleton()->get_builtins_with_feature_overrides_applied().find(action_name)->value;
		Array action_events = _event_list_to_array_helper(action.inputs);
		if (!_should_display_shortcut(action_name, action_events, false)) {
			continue;
		}

		Array default_events = _event_list_to_array_helper(all_default_events);
		bool same_as_defaults = Shortcut::is_event_array_equal(default_events, action_events);
		bool collapse = !collapsed.has(action_name) || (collapsed.has(action_name) && collapsed[action_name]);

		TreeItem *item = _create_shortcut_treeitem(common_section, action_name, action_name, action_events, !same_as_defaults, true, collapse);
		item->set_auto_translate_mode(0, AUTO_TRANSLATE_MODE_DISABLED); // `ui_*` input action names are untranslatable identifiers.
	}

	// Editor Shortcuts

	List<String> slist;
	EditorSettings::get_singleton()->get_shortcut_list(&slist);
	slist.sort(); // Sort alphabetically.

	const EditorPropertyNameProcessor::Style name_style = EditorPropertyNameProcessor::get_settings_style();
	const EditorPropertyNameProcessor::Style tooltip_style = EditorPropertyNameProcessor::get_tooltip_style(name_style);

	// Create all sections first.
	for (const String &E : slist) {
		Ref<Shortcut> sc = EditorSettings::get_singleton()->get_shortcut(E);
		String section_name = E.get_slice("/", 0);

		if (sections.has(section_name)) {
			continue;
		}

		TreeItem *section = shortcuts->create_item(root);

		const String item_name = EditorPropertyNameProcessor::get_singleton()->process_name(section_name, name_style, E);
		const String tooltip = EditorPropertyNameProcessor::get_singleton()->process_name(section_name, tooltip_style, E);

		section->set_auto_translate_mode(0, AUTO_TRANSLATE_MODE_DISABLED); // Already translated manually.
		section->set_text(0, item_name);
		section->set_tooltip_text(0, tooltip);
		section->set_selectable(0, false);
		section->set_selectable(1, false);
		section->set_custom_bg_color(0, shortcuts->get_theme_color(SNAME("prop_subsection"), EditorStringName(Editor)));
		section->set_custom_bg_color(1, shortcuts->get_theme_color(SNAME("prop_subsection"), EditorStringName(Editor)));

		if (collapsed.has(item_name)) {
			section->set_collapsed(collapsed[item_name]);
		}

		sections[section_name] = section;
	}

	// Add shortcuts to sections.
	for (const String &E : slist) {
		Ref<Shortcut> sc = EditorSettings::get_singleton()->get_shortcut(E);
		if (!sc->has_meta("original")) {
			continue;
		}

		String section_name = E.get_slice("/", 0);
		TreeItem *section = sections[section_name];

		if (!_should_display_shortcut(sc->get_name(), sc->get_events(), true)) {
			continue;
		}

		Array original = sc->get_meta("original");
		Array shortcuts_array = sc->get_events().duplicate(true);
		bool same_as_defaults = Shortcut::is_event_array_equal(original, shortcuts_array);
		bool collapse = !collapsed.has(E) || (collapsed.has(E) && collapsed[E]);

		_create_shortcut_treeitem(section, E, sc->get_name(), shortcuts_array, !same_as_defaults, false, collapse);
	}

	// remove sections with no shortcuts
	for (KeyValue<String, TreeItem *> &E : sections) {
		TreeItem *section = E.value;
		if (section->get_first_child() == nullptr) {
			root->remove_child(section);
			memdelete(section);
		}
	}

	// Update UI.
	clear_all_search->set_disabled(shortcut_search_box->get_text().is_empty() && shortcut_search_by_event->get_event().is_null());
}

void EditorSettingsDialog::_shortcut_button_pressed(Object *p_item, int p_column, int p_idx, MouseButton p_button) {
	if (p_button != MouseButton::LEFT) {
		return;
	}
	TreeItem *ti = Object::cast_to<TreeItem>(p_item);
	ERR_FAIL_NULL_MSG(ti, "Object passed is not a TreeItem.");

	ShortcutButton button_idx = (ShortcutButton)p_idx;

	is_editing_action = ti->get_meta("is_action");

	String type = ti->get_meta("type");

	if (type == "event") {
		current_edited_identifier = ti->get_parent()->get_meta("shortcut_identifier");
		current_events = ti->get_parent()->get_meta("events");
		current_event_index = ti->get_meta("event_index");
	} else { // Type is "shortcut"
		current_edited_identifier = ti->get_meta("shortcut_identifier");
		current_events = ti->get_meta("events");
		current_event_index = -1;
	}

	switch (button_idx) {
		case EditorSettingsDialog::SHORTCUT_ADD: {
			// Only for "shortcut" types
			shortcut_editor->popup_and_configure();
		} break;
		case EditorSettingsDialog::SHORTCUT_EDIT: {
			// Only for "event" types
			shortcut_editor->popup_and_configure(current_events[current_event_index]);
		} break;
		case EditorSettingsDialog::SHORTCUT_ERASE: {
			if (type == "shortcut") {
				if (is_editing_action) {
					_update_builtin_action(current_edited_identifier, Array());
				} else {
					_update_shortcut_events(current_edited_identifier, Array());
				}
			} else if (type == "event") {
				current_events.remove_at(current_event_index);

				if (is_editing_action) {
					_update_builtin_action(current_edited_identifier, current_events);
				} else {
					_update_shortcut_events(current_edited_identifier, current_events);
				}
			}
		} break;
		case EditorSettingsDialog::SHORTCUT_REVERT: {
			// Only for "shortcut" types
			if (is_editing_action) {
				List<Ref<InputEvent>> defaults = InputMap::get_singleton()->get_builtins_with_feature_overrides_applied()[current_edited_identifier];
				Array events = _event_list_to_array_helper(defaults);

				_update_builtin_action(current_edited_identifier, events);
			} else {
				Ref<Shortcut> sc = EditorSettings::get_singleton()->get_shortcut(current_edited_identifier);
				Array original = sc->get_meta("original");
				_update_shortcut_events(current_edited_identifier, original);
			}
		} break;
		default:
			break;
	}
}

void EditorSettingsDialog::_shortcut_cell_double_clicked() {
	// When a shortcut cell is double clicked:
	// If the cell has children and is in the bindings column, and if its first child is editable,
	// then uncollapse the cell, and if the first child is the only child, then edit that child.
	// If the cell is in the bindings column and can be edited, then edit it.
	// If the cell is in the name column, then toggle collapse.
	const ShortcutButton edit_btn_id = EditorSettingsDialog::SHORTCUT_EDIT;
	const int edit_btn_col = 1;
	TreeItem *ti = shortcuts->get_selected();
	if (ti == nullptr) {
		return;
	}

	String type = ti->get_meta("type");
	int col = shortcuts->get_selected_column();
	if (type == "shortcut" && col == 0) {
		if (ti->get_first_child()) {
			ti->set_collapsed(!ti->is_collapsed());
		}
	} else if (type == "shortcut" && col == 1) {
		if (ti->get_first_child()) {
			TreeItem *child_ti = ti->get_first_child();
			if (child_ti->get_button_by_id(edit_btn_col, edit_btn_id) != -1) {
				ti->set_collapsed(false);
				if (ti->get_child_count() == 1) {
					_shortcut_button_pressed(child_ti, edit_btn_col, edit_btn_id);
				}
			}
		}
	} else if (type == "event" && col == 1) {
		if (ti->get_button_by_id(edit_btn_col, edit_btn_id) != -1) {
			_shortcut_button_pressed(ti, edit_btn_col, edit_btn_id);
		}
	}
}

Variant EditorSettingsDialog::get_drag_data_fw(const Point2 &p_point, Control *p_from) {
	TreeItem *selected = shortcuts->get_selected();

	// Only allow drag for events
	if (!selected || (String)selected->get_meta("type", "") != "event") {
		return Variant();
	}

	String label_text = vformat(TTRC("Event %d"), selected->get_meta("event_index"));
	Label *label = memnew(Label(label_text));
	label->set_modulate(Color(1, 1, 1, 1.0f));
	shortcuts->set_drag_preview(label);

	shortcuts->set_drop_mode_flags(Tree::DROP_MODE_INBETWEEN);

	return Dictionary(); // No data required
}

bool EditorSettingsDialog::can_drop_data_fw(const Point2 &p_point, const Variant &p_data, Control *p_from) const {
	TreeItem *selected = shortcuts->get_selected();
	TreeItem *item = shortcuts->get_item_at_position(p_point);
	if (!selected || !item || item == selected || (String)item->get_meta("type", "") != "event") {
		return false;
	}

	// Don't allow moving an events in-between shortcuts.
	if (selected->get_parent()->get_meta("shortcut_identifier") != item->get_parent()->get_meta("shortcut_identifier")) {
		return false;
	}

	return true;
}

void EditorSettingsDialog::drop_data_fw(const Point2 &p_point, const Variant &p_data, Control *p_from) {
	if (!can_drop_data_fw(p_point, p_data, p_from)) {
		return;
	}

	TreeItem *selected = shortcuts->get_selected();
	TreeItem *target = shortcuts->get_item_at_position(p_point);

	if (!target) {
		return;
	}

	int target_event_index = target->get_meta("event_index");
	int index_moving_from = selected->get_meta("event_index");

	Array events = selected->get_parent()->get_meta("events");

	Variant event_moved = events[index_moving_from];
	events.remove_at(index_moving_from);
	events.insert(target_event_index, event_moved);

	String ident = selected->get_parent()->get_meta("shortcut_identifier");
	if (selected->get_meta("is_action")) {
		_update_builtin_action(ident, events);
	} else {
		_update_shortcut_events(ident, events);
	}
}

void EditorSettingsDialog::_tabs_tab_changed(int p_tab) {
	_focus_current_search_box();

	// When tab has switched, shortcuts may have changed.
	_update_dynamic_property_hints();
	inspector->get_inspector()->update_tree();
}

void EditorSettingsDialog::_update_dynamic_property_hints() {
	// Calling add_property_hint overrides the existing hint.
	EditorSettings *settings = EditorSettings::get_singleton();
	settings->add_property_hint(_create_mouse_shortcut_property_info("editors/3d/navigation/orbit_mouse_button", "spatial_editor/viewport_orbit_modifier_1", "spatial_editor/viewport_orbit_modifier_2"));
	settings->add_property_hint(_create_mouse_shortcut_property_info("editors/3d/navigation/pan_mouse_button", "spatial_editor/viewport_pan_modifier_1", "spatial_editor/viewport_pan_modifier_2"));
	settings->add_property_hint(_create_mouse_shortcut_property_info("editors/3d/navigation/zoom_mouse_button", "spatial_editor/viewport_zoom_modifier_1", "spatial_editor/viewport_zoom_modifier_2"));
}

PropertyInfo EditorSettingsDialog::_create_mouse_shortcut_property_info(const String &p_property_name, const String &p_shortcut_1_name, const String &p_shortcut_2_name) {
	String hint_string;
	hint_string += _get_shortcut_button_string(p_shortcut_1_name) + _get_shortcut_button_string(p_shortcut_2_name);
	hint_string += "Left Mouse,";
	hint_string += _get_shortcut_button_string(p_shortcut_1_name) + _get_shortcut_button_string(p_shortcut_2_name);
	hint_string += "Middle Mouse,";
	hint_string += _get_shortcut_button_string(p_shortcut_1_name) + _get_shortcut_button_string(p_shortcut_2_name);
	hint_string += "Right Mouse,";
	hint_string += _get_shortcut_button_string(p_shortcut_1_name) + _get_shortcut_button_string(p_shortcut_2_name);
	hint_string += "Mouse Button 4,";
	hint_string += _get_shortcut_button_string(p_shortcut_1_name) + _get_shortcut_button_string(p_shortcut_2_name);
	hint_string += "Mouse Button 5";

	return PropertyInfo(Variant::INT, p_property_name, PROPERTY_HINT_ENUM, hint_string);
}

String EditorSettingsDialog::_get_shortcut_button_string(const String &p_shortcut_name) {
	String button_string;
	Ref<Shortcut> shortcut_ref = EditorSettings::get_singleton()->get_shortcut(p_shortcut_name);
	Array events = shortcut_ref->get_events();
	for (Ref<InputEvent> input_event : events) {
		button_string += input_event->as_text() + " + ";
	}
	return button_string;
}

void EditorSettingsDialog::_focus_current_search_box() {
	Control *tab = tabs->get_current_tab_control();
	LineEdit *current_search_box = nullptr;
	if (tab == tab_general) {
		current_search_box = search_box;
	} else if (tab == tab_shortcuts) {
		current_search_box = shortcut_search_box;
	}

	if (current_search_box) {
		current_search_box->grab_focus();
		current_search_box->select_all();
	}
}

void EditorSettingsDialog::_advanced_toggled(bool p_button_pressed) {
	EditorSettings::get_singleton()->set("_editor_settings_advanced_mode", p_button_pressed);
}

void EditorSettingsDialog::_editor_restart() {
	EditorNode::get_singleton()->save_all_scenes();
	EditorNode::get_singleton()->restart_editor();
}

void EditorSettingsDialog::_editor_restart_request() {
	restart_container->show();
}

void EditorSettingsDialog::_editor_restart_close() {
	restart_container->hide();
}

void EditorSettingsDialog::_bind_methods() {
	ClassDB::bind_method(D_METHOD("_update_shortcuts"), &EditorSettingsDialog::_update_shortcuts);
	ClassDB::bind_method(D_METHOD("_settings_changed"), &EditorSettingsDialog::_settings_changed);
}

EditorSettingsDialog::EditorSettingsDialog() {
	set_title(TTR("Editor Settings"));
	set_clamp_to_embedder(true);

	tabs = memnew(TabContainer);
	tabs->set_theme_type_variation("TabContainerOdd");
	tabs->connect("tab_changed", callable_mp(this, &EditorSettingsDialog::_tabs_tab_changed));
	add_child(tabs);

	// General Tab

	tab_general = memnew(VBoxContainer);
	tabs->add_child(tab_general);
	tab_general->set_name(TTR("General"));

	HBoxContainer *hbc = memnew(HBoxContainer);
	hbc->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	tab_general->add_child(hbc);

	search_box = memnew(LineEdit);
	search_box->set_placeholder(TTR("Filter Settings"));
	search_box->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	hbc->add_child(search_box);

	advanced_switch = memnew(CheckButton(TTR("Advanced Settings")));
	hbc->add_child(advanced_switch);

	bool use_advanced = EDITOR_DEF("_editor_settings_advanced_mode", false);
	advanced_switch->set_pressed(use_advanced);
	advanced_switch->connect(SceneStringName(toggled), callable_mp(this, &EditorSettingsDialog::_advanced_toggled));

	inspector = memnew(SectionedInspector);
	inspector->get_inspector()->set_use_filter(true);
	inspector->register_search_box(search_box);
	inspector->register_advanced_toggle(advanced_switch);
	inspector->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	tab_general->add_child(inspector);
	inspector->get_inspector()->connect("property_edited", callable_mp(this, &EditorSettingsDialog::_settings_property_edited));
	inspector->get_inspector()->connect("restart_requested", callable_mp(this, &EditorSettingsDialog::_editor_restart_request));

	restart_container = memnew(PanelContainer);
	tab_general->add_child(restart_container);
	HBoxContainer *restart_hb = memnew(HBoxContainer);
	restart_container->add_child(restart_hb);
	restart_icon = memnew(TextureRect);
	restart_icon->set_v_size_flags(Control::SIZE_SHRINK_CENTER);
	restart_hb->add_child(restart_icon);
	restart_label = memnew(Label);
	restart_label->set_text(TTR("The editor must be restarted for changes to take effect."));
	restart_hb->add_child(restart_label);
	restart_hb->add_spacer();
	Button *restart_button = memnew(Button);
	restart_button->connect(SceneStringName(pressed), callable_mp(this, &EditorSettingsDialog::_editor_restart));
	restart_hb->add_child(restart_button);
	restart_button->set_text(TTR("Save & Restart"));
	restart_close_button = memnew(Button);
	restart_close_button->set_flat(true);
	restart_close_button->connect(SceneStringName(pressed), callable_mp(this, &EditorSettingsDialog::_editor_restart_close));
	restart_hb->add_child(restart_close_button);
	restart_container->hide();

	// Shortcuts Tab

	tab_shortcuts = memnew(VBoxContainer);

	tabs->add_child(tab_shortcuts);
	tab_shortcuts->set_name(TTR("Shortcuts"));

	HBoxContainer *top_hbox = memnew(HBoxContainer);
	top_hbox->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	tab_shortcuts->add_child(top_hbox);

	shortcut_search_box = memnew(LineEdit);
	shortcut_search_box->set_placeholder(TTR("Filter by Name"));
	shortcut_search_box->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	top_hbox->add_child(shortcut_search_box);
	shortcut_search_box->connect(SceneStringName(text_changed), callable_mp(this, &EditorSettingsDialog::_filter_shortcuts));

	shortcut_search_by_event = memnew(EventListenerLineEdit);
	shortcut_search_by_event->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	shortcut_search_by_event->set_stretch_ratio(0.75);
	shortcut_search_by_event->set_allowed_input_types(INPUT_KEY);
	shortcut_search_by_event->connect("event_changed", callable_mp(this, &EditorSettingsDialog::_filter_shortcuts_by_event));
	shortcut_search_by_event->connect(SceneStringName(focus_entered), callable_mp((AcceptDialog *)this, &AcceptDialog::set_close_on_escape).bind(false));
	shortcut_search_by_event->connect(SceneStringName(focus_exited), callable_mp((AcceptDialog *)this, &AcceptDialog::set_close_on_escape).bind(true));
	top_hbox->add_child(shortcut_search_by_event);

	clear_all_search = memnew(Button);
	clear_all_search->set_text(TTR("Clear All"));
	clear_all_search->set_tooltip_text(TTR("Clear all search filters."));
	clear_all_search->connect(SceneStringName(pressed), callable_mp(shortcut_search_box, &LineEdit::clear));
	clear_all_search->connect(SceneStringName(pressed), callable_mp(shortcut_search_by_event, &EventListenerLineEdit::clear_event));
	top_hbox->add_child(clear_all_search);

	shortcuts = memnew(Tree);
	shortcuts->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	shortcuts->set_columns(2);
	shortcuts->set_hide_root(true);
	shortcuts->set_column_titles_visible(true);
	shortcuts->set_column_title(0, TTRC("Name"));
	shortcuts->set_column_title(1, TTRC("Binding"));
	shortcuts->connect("button_clicked", callable_mp(this, &EditorSettingsDialog::_shortcut_button_pressed));
	shortcuts->connect("item_activated", callable_mp(this, &EditorSettingsDialog::_shortcut_cell_double_clicked));
	tab_shortcuts->add_child(shortcuts);

	SET_DRAG_FORWARDING_GCD(shortcuts, EditorSettingsDialog);

	// Adding event dialog
	shortcut_editor = memnew(InputEventConfigurationDialog);
	shortcut_editor->connect(SceneStringName(confirmed), callable_mp(this, &EditorSettingsDialog::_event_config_confirmed));
	shortcut_editor->set_allowed_input_types(INPUT_KEY);
	add_child(shortcut_editor);

	set_hide_on_ok(true);

	timer = memnew(Timer);
	timer->set_wait_time(1.5);
	timer->connect("timeout", callable_mp(this, &EditorSettingsDialog::_settings_save));
	timer->set_one_shot(true);
	add_child(timer);
	EditorSettings::get_singleton()->connect("settings_changed", callable_mp(this, &EditorSettingsDialog::_settings_changed));
	set_ok_button_text(TTR("Close"));
}

EditorSettingsDialog::~EditorSettingsDialog() {
}

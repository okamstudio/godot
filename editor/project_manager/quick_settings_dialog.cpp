/**************************************************************************/
/*  quick_settings_dialog.cpp                                             */
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

#include "quick_settings_dialog.h"

#include "core/config/project_settings.h"
#include "core/string/translation_server.h"
#include "editor/editor_settings.h"
#include "editor/editor_string_names.h"
#include "editor/themes/editor_scale.h"
#include "scene/gui/box_container.h"
#include "scene/gui/button.h"
#include "scene/gui/label.h"
#include "scene/gui/option_button.h"
#include "scene/gui/panel_container.h"

void QuickSettingsDialog::_fetch_setting_values() {
#ifndef ANDROID_ENABLED
	editor_languages.clear();
#endif
	editor_themes.clear();
	editor_scales.clear();
	editor_network_modes.clear();
	editor_directory_naming_conventions.clear();

	{
		List<PropertyInfo> editor_settings_properties;
		EditorSettings::get_singleton()->get_property_list(&editor_settings_properties);

		for (const PropertyInfo &pi : editor_settings_properties) {
			if (pi.name == "interface/editor/editor_language") {
#ifndef ANDROID_ENABLED
				editor_languages = pi.hint_string.split(",");
#endif
			} else if (pi.name == "interface/theme/preset") {
				editor_themes = pi.hint_string.split(",");
			} else if (pi.name == "interface/editor/display_scale") {
				editor_scales = pi.hint_string.split(",");
			} else if (pi.name == "network/connection/network_mode") {
				editor_network_modes = pi.hint_string.split(",");
			} else if (pi.name == "project_manager/directory_naming_convention") {
				editor_directory_naming_conventions = pi.hint_string.split(",");
			}
		}
	}
}

void QuickSettingsDialog::_update_current_values() {
#ifndef ANDROID_ENABLED
	// Language options.
	{
		const String current_lang = EDITOR_GET("interface/editor/editor_language");

		for (int i = 0; i < editor_languages.size(); i++) {
			const String &lang_value = editor_languages[i];
			if (current_lang == lang_value) {
				language_option_button->set_text(current_lang);
				language_option_button->select(i);
			}
		}
	}
#endif

	// Theme options.
	{
		const String current_theme = EDITOR_GET("interface/theme/preset");

		for (int i = 0; i < editor_themes.size(); i++) {
			const String &theme_value = editor_themes[i];
			if (current_theme == theme_value) {
				theme_option_button->set_text(current_theme);
				theme_option_button->select(i);

				custom_theme_label->set_visible(current_theme == "Custom");
			}
		}
	}

	// Scale options.
	{
		const int current_scale = EDITOR_GET("interface/editor/display_scale");

		for (int i = 0; i < editor_scales.size(); i++) {
			const String &scale_value = editor_scales[i];
			if (current_scale == i) {
				scale_option_button->set_text(scale_value);
				scale_option_button->select(i);
			}
		}
	}

	// Network mode options.
	{
		const int current_network_mode = EDITOR_GET("network/connection/network_mode");

		for (int i = 0; i < editor_network_modes.size(); i++) {
			const String &network_mode_value = editor_network_modes[i];
			if (current_network_mode == i) {
				network_mode_option_button->set_text(network_mode_value);
				network_mode_option_button->select(i);
			}
		}
	}

	// Project directory naming options.
	{
		const int current_directory_naming = EDITOR_GET("project_manager/directory_naming_convention");

		for (int i = 0; i < editor_directory_naming_conventions.size(); i++) {
			const String &directory_naming_value = editor_directory_naming_conventions[i];
			if (current_directory_naming == i) {
				directory_naming_convention_button->set_text(directory_naming_value);
				directory_naming_convention_button->select(i);
			}
		}
	}
}

void QuickSettingsDialog::_add_setting_control(const String &p_text, Control *p_control) {
	HBoxContainer *container = memnew(HBoxContainer);
	settings_list->add_child(container);

	Label *label = memnew(Label(p_text));
	label->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	container->add_child(label);

	p_control->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	p_control->set_stretch_ratio(2.0);
	container->add_child(p_control);
}

#ifndef ANDROID_ENABLED
void QuickSettingsDialog::_language_selected(int p_id) {
	const String selected_language = language_option_button->get_item_metadata(p_id);
	_set_setting_value("interface/editor/editor_language", selected_language, true);
}
#endif

void QuickSettingsDialog::_theme_selected(int p_id) {
	const String selected_theme = theme_option_button->get_item_text(p_id);
	_set_setting_value("interface/theme/preset", selected_theme);

	custom_theme_label->set_visible(selected_theme == "Custom");
}

void QuickSettingsDialog::_scale_selected(int p_id) {
	_set_setting_value("interface/editor/display_scale", p_id, true);
}

void QuickSettingsDialog::_network_mode_selected(int p_id) {
	_set_setting_value("network/connection/network_mode", p_id);
}

void QuickSettingsDialog::_directory_naming_convention_selected(int p_id) {
	_set_setting_value("project_manager/directory_naming_convention", p_id);
}

void QuickSettingsDialog::_set_setting_value(const String &p_setting, const Variant &p_value, bool p_restart_required) {
	EditorSettings::get_singleton()->set(p_setting, p_value);
	EditorSettings::get_singleton()->notify_changes();
	EditorSettings::get_singleton()->save();

	if (p_restart_required) {
		restart_required_label->show();

		if (!restart_required_button) {
			restart_required_button = add_button(TTR("Restart Now"), !GLOBAL_GET("gui/common/swap_cancel_ok"));
			restart_required_button->connect(SceneStringName(pressed), callable_mp(this, &QuickSettingsDialog::_request_restart));
		}
	}
}

void QuickSettingsDialog::_request_restart() {
	emit_signal("restart_required");
}

void QuickSettingsDialog::update_size_limits(const Size2 &p_max_popup_size) {
#ifndef ANDROID_ENABLED
	language_option_button->get_popup()->set_max_size(p_max_popup_size);
#endif
}

void QuickSettingsDialog::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_THEME_CHANGED: {
			settings_list_panel->add_theme_style_override(SceneStringName(panel), get_theme_stylebox(SNAME("Background"), EditorStringName(EditorStyles)));

			restart_required_label->add_theme_color_override(SceneStringName(font_color), get_theme_color(SNAME("warning_color"), EditorStringName(Editor)));
			custom_theme_label->add_theme_color_override(SceneStringName(font_color), get_theme_color(SNAME("font_placeholder_color"), EditorStringName(Editor)));
		} break;

		case NOTIFICATION_VISIBILITY_CHANGED: {
			if (is_visible()) {
				_update_current_values();
			}
		} break;
	}
}

void QuickSettingsDialog::_bind_methods() {
	ADD_SIGNAL(MethodInfo("restart_required"));
}

QuickSettingsDialog::QuickSettingsDialog() {
	set_title(TTR("Quick Settings"));
	set_ok_button_text(TTR("Close"));

	VBoxContainer *main_vbox = memnew(VBoxContainer);
	add_child(main_vbox);
	main_vbox->set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT);

	// Settings grid.
	{
		_fetch_setting_values();

		settings_list_panel = memnew(PanelContainer);
		main_vbox->add_child(settings_list_panel);

		settings_list = memnew(VBoxContainer);
		settings_list_panel->add_child(settings_list);

#ifndef ANDROID_ENABLED
		// Language options.
		{
			language_option_button = memnew(OptionButton);
			language_option_button->set_fit_to_longest_item(false);
			language_option_button->connect(SceneStringName(item_selected), callable_mp(this, &QuickSettingsDialog::_language_selected));

			for (int i = 0; i < editor_languages.size(); i++) {
				const String &lang_value = editor_languages[i];
				String lang_name = TranslationServer::get_singleton()->get_locale_name(lang_value);
				language_option_button->add_item(vformat("[%s] %s", lang_value, lang_name), i);
				language_option_button->set_item_metadata(i, lang_value);
			}

			_add_setting_control(TTR("Language"), language_option_button);
		}
#endif

		// Theme options.
		{
			theme_option_button = memnew(OptionButton);
			theme_option_button->set_fit_to_longest_item(false);
			theme_option_button->connect(SceneStringName(item_selected), callable_mp(this, &QuickSettingsDialog::_theme_selected));

			for (int i = 0; i < editor_themes.size(); i++) {
				const String &theme_value = editor_themes[i];
				theme_option_button->add_item(theme_value, i);
			}

			_add_setting_control(TTR("Interface Theme"), theme_option_button);

			custom_theme_label = memnew(Label(TTR("Custom preset can be further configured in the editor.")));
			custom_theme_label->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_RIGHT);
			custom_theme_label->set_custom_minimum_size(Size2(220, 0) * EDSCALE);
			custom_theme_label->set_autowrap_mode(TextServer::AUTOWRAP_WORD);
			custom_theme_label->set_h_size_flags(Control::SIZE_EXPAND_FILL);
			custom_theme_label->set_stretch_ratio(2.0);
			custom_theme_label->hide();
			settings_list->add_child(custom_theme_label);
		}

		// Scale options.
		{
			scale_option_button = memnew(OptionButton);
			scale_option_button->set_fit_to_longest_item(false);
			scale_option_button->connect(SceneStringName(item_selected), callable_mp(this, &QuickSettingsDialog::_scale_selected));

			for (int i = 0; i < editor_scales.size(); i++) {
				const String &scale_value = editor_scales[i];
				scale_option_button->add_item(scale_value, i);
			}

			_add_setting_control(TTR("Display Scale"), scale_option_button);
		}

		// Network mode options.
		{
			network_mode_option_button = memnew(OptionButton);
			network_mode_option_button->set_fit_to_longest_item(false);
			network_mode_option_button->connect(SceneStringName(item_selected), callable_mp(this, &QuickSettingsDialog::_network_mode_selected));

			for (int i = 0; i < editor_network_modes.size(); i++) {
				const String &network_mode_value = editor_network_modes[i];
				network_mode_option_button->add_item(network_mode_value, i);
			}

			_add_setting_control(TTR("Network Mode"), network_mode_option_button);
		}

		// Project directory naming options.
		{
			directory_naming_convention_button = memnew(OptionButton);
			directory_naming_convention_button->set_fit_to_longest_item(false);
			directory_naming_convention_button->connect(SceneStringName(item_selected), callable_mp(this, &QuickSettingsDialog::_directory_naming_convention_selected));

			for (int i = 0; i < editor_directory_naming_conventions.size(); i++) {
				const String &directory_naming_convention = editor_directory_naming_conventions[i];
				directory_naming_convention_button->add_item(directory_naming_convention, i);
			}

			_add_setting_control(TTR("Directory Naming Convention"), directory_naming_convention_button);
		}

		_update_current_values();
	}

	// Restart required panel.
	{
		restart_required_label = memnew(Label(TTR("Settings changed! The project manager must be restarted for changes to take effect.")));
		restart_required_label->set_custom_minimum_size(Size2(560, 0) * EDSCALE);
		restart_required_label->set_autowrap_mode(TextServer::AUTOWRAP_WORD);
		restart_required_label->set_h_size_flags(Control::SIZE_EXPAND_FILL);
		restart_required_label->hide();
		main_vbox->add_child(restart_required_label);
	}
}

/**************************************************************************/
/*  quick_settings_dialog.h                                               */
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

#include "scene/gui/dialogs.h"

class Button;
class Label;
class MarginContainer;
class OptionButton;
class PanelContainer;
class VBoxContainer;

class QuickSettingsDialog : public AcceptDialog {
	GDCLASS(QuickSettingsDialog, AcceptDialog);

#ifndef ANDROID_ENABLED
	Vector<String> editor_languages;
#endif
	Vector<String> editor_themes;
	Vector<String> editor_scales;
	Vector<String> editor_network_modes;
	Vector<String> editor_directory_naming_conventions;

	void _fetch_setting_values();
	void _update_current_values();

	PanelContainer *settings_list_panel = nullptr;
	VBoxContainer *settings_list = nullptr;

	void _add_setting_control(const String &p_text, Control *p_control);

#ifndef ANDROID_ENABLED
	// The language selection dropdown doesn't work on Android (as the setting isn't saved), see GH-60353.
	// Also, the dropdown it spawns is very tall and can't be scrolled without a hardware mouse.
	OptionButton *language_option_button = nullptr;
#endif
	OptionButton *theme_option_button = nullptr;
	OptionButton *scale_option_button = nullptr;
	OptionButton *network_mode_option_button = nullptr;
	OptionButton *directory_naming_convention_button = nullptr;

	Label *custom_theme_label = nullptr;

#ifndef ANDROID_ENABLED
	void _language_selected(int p_id);
#endif
	void _theme_selected(int p_id);
	void _scale_selected(int p_id);
	void _network_mode_selected(int p_id);
	void _directory_naming_convention_selected(int p_id);
	void _set_setting_value(const String &p_setting, const Variant &p_value, bool p_restart_required = false);

	Label *restart_required_label = nullptr;
	Button *restart_required_button = nullptr;

	void _request_restart();

protected:
	void _notification(int p_what);
	static void _bind_methods();

public:
	void update_size_limits(const Size2 &p_max_popup_size);

	QuickSettingsDialog();
};

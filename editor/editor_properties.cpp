/**************************************************************************/
/*  editor_properties.cpp                                                 */
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

#include "editor_properties.h"

#include "core/config/project_settings.h"
#include "editor/create_dialog.h"
#include "editor/editor_node.h"
#include "editor/editor_properties_array_dict.h"
#include "editor/editor_properties_vector.h"
#include "editor/editor_resource_picker.h"
#include "editor/editor_settings.h"
#include "editor/editor_string_names.h"
#include "editor/gui/editor_file_dialog.h"
#include "editor/gui/editor_spin_slider.h"
#include "editor/gui/scene_tree_editor.h"
#include "editor/inspector_dock.h"
#include "editor/plugins/script_editor_plugin.h"
#include "editor/project_settings_editor.h"
#include "editor/property_selector.h"
#include "editor/scene_tree_dock.h"
#include "editor/themes/editor_scale.h"
#include "editor/themes/editor_theme_manager.h"
#include "scene/2d/gpu_particles_2d.h"
#include "scene/3d/fog_volume.h"
#include "scene/3d/gpu_particles_3d.h"
#include "scene/gui/color_picker.h"
#include "scene/main/window.h"
#include "scene/resources/font.h"
#include "scene/resources/mesh.h"
#include "scene/resources/packed_scene.h"
#include "scene/resources/visual_shader_nodes.h"

///////////////////// Nil /////////////////////////

void EditorPropertyNil::update_property() {
}

EditorPropertyNil::EditorPropertyNil() {
	Label *prop_label = memnew(Label);
	prop_label->set_text("<null>");
	add_child(prop_label);
}

///////////////////// TEXT /////////////////////////

void EditorPropertyText::_set_read_only(bool p_read_only) {
	text->set_editable(!p_read_only);
}

void EditorPropertyText::_text_submitted(const String &p_string) {
	if (updating) {
		return;
	}

	if (text->has_focus()) {
		_text_changed(p_string);
	}
}

void EditorPropertyText::_text_changed(const String &p_string) {
	if (updating) {
		return;
	}

	if (string_name) {
		emit_changed(get_edited_property(), StringName(p_string));
	} else {
		emit_changed(get_edited_property(), p_string);
	}
}

void EditorPropertyText::update_property() {
	String s = get_edited_property_value();
	updating = true;
	if (text->get_text() != s) {
		int caret = text->get_caret_column();
		text->set_text(s);
		text->set_caret_column(caret);
	}
	text->set_editable(!is_read_only());
	updating = false;
}

void EditorPropertyText::set_string_name(bool p_enabled) {
	string_name = p_enabled;
	if (p_enabled) {
		Label *prefix = memnew(Label("&"));
		prefix->set_tooltip_text("StringName");
		prefix->set_mouse_filter(MOUSE_FILTER_STOP);
		text->get_parent()->add_child(prefix);
		text->get_parent()->move_child(prefix, 0);
	}
}

void EditorPropertyText::set_secret(bool p_enabled) {
	text->set_secret(p_enabled);
}

void EditorPropertyText::set_placeholder(const String &p_string) {
	text->set_placeholder(p_string);
}

EditorPropertyText::EditorPropertyText() {
	HBoxContainer *hb = memnew(HBoxContainer);
	add_child(hb);

	text = memnew(LineEdit);
	hb->add_child(text);
	add_focusable(text);
	text->set_h_size_flags(SIZE_EXPAND_FILL);
	text->connect(SceneStringName(text_changed), callable_mp(this, &EditorPropertyText::_text_changed));
	text->connect("text_submitted", callable_mp(this, &EditorPropertyText::_text_submitted));
}

///////////////////// MULTILINE TEXT /////////////////////////

void EditorPropertyMultilineText::_set_read_only(bool p_read_only) {
	text->set_editable(!p_read_only);
	open_big_text->set_disabled(p_read_only);
}

void EditorPropertyMultilineText::_big_text_changed() {
	text->set_text(big_text->get_text());
	emit_changed(get_edited_property(), big_text->get_text(), "", true);
}

void EditorPropertyMultilineText::_text_changed() {
	emit_changed(get_edited_property(), text->get_text(), "", true);
}

void EditorPropertyMultilineText::_open_big_text() {
	if (!big_text_dialog) {
		big_text = memnew(TextEdit);
		if (expression) {
			big_text->set_syntax_highlighter(text->get_syntax_highlighter());
			big_text->add_theme_font_override(SceneStringName(font), get_theme_font(SNAME("expression"), EditorStringName(EditorFonts)));
			big_text->add_theme_font_size_override(SceneStringName(font_size), get_theme_font_size(SNAME("expression_size"), EditorStringName(EditorFonts)));
		}
		big_text->connect(SceneStringName(text_changed), callable_mp(this, &EditorPropertyMultilineText::_big_text_changed));
		big_text->set_line_wrapping_mode(TextEdit::LineWrappingMode::LINE_WRAPPING_BOUNDARY);
		big_text_dialog = memnew(AcceptDialog);
		big_text_dialog->add_child(big_text);
		big_text_dialog->set_title(TTR("Edit Text:"));
		add_child(big_text_dialog);
	}

	big_text_dialog->popup_centered_clamped(Size2(1000, 900) * EDSCALE, 0.8);
	big_text->set_text(text->get_text());
	big_text->grab_focus();
}

void EditorPropertyMultilineText::update_property() {
	String t = get_edited_property_value();
	if (text->get_text() != t) {
		text->set_text(t);
		if (big_text && big_text->is_visible_in_tree()) {
			big_text->set_text(t);
		}
	}
}

void EditorPropertyMultilineText::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_THEME_CHANGED:
		case NOTIFICATION_ENTER_TREE: {
			Ref<Texture2D> df = get_editor_theme_icon(SNAME("DistractionFree"));
			open_big_text->set_icon(df);

			Ref<Font> font;
			int font_size;
			if (expression) {
				font = get_theme_font(SNAME("expression"), EditorStringName(EditorFonts));
				font_size = get_theme_font_size(SNAME("expression_size"), EditorStringName(EditorFonts));

				text->add_theme_font_override(SceneStringName(font), font);
				text->add_theme_font_size_override(SceneStringName(font_size), font_size);
				if (big_text) {
					big_text->add_theme_font_override(SceneStringName(font), font);
					big_text->add_theme_font_size_override(SceneStringName(font_size), font_size);
				}
			} else {
				font = get_theme_font(SceneStringName(font), SNAME("TextEdit"));
				font_size = get_theme_font_size(SceneStringName(font_size), SNAME("TextEdit"));
			}
			text->set_custom_minimum_size(Vector2(0, font->get_height(font_size) * 6));
		} break;
	}
}

EditorPropertyMultilineText::EditorPropertyMultilineText(bool p_expression) {
	HBoxContainer *hb = memnew(HBoxContainer);
	hb->add_theme_constant_override("separation", 0);
	add_child(hb);
	set_bottom_editor(hb);
	text = memnew(TextEdit);
	text->connect(SceneStringName(text_changed), callable_mp(this, &EditorPropertyMultilineText::_text_changed));
	text->set_line_wrapping_mode(TextEdit::LineWrappingMode::LINE_WRAPPING_BOUNDARY);
	add_focusable(text);
	hb->add_child(text);
	text->set_h_size_flags(SIZE_EXPAND_FILL);
	open_big_text = memnew(Button);
	open_big_text->set_flat(true);
	open_big_text->connect(SceneStringName(pressed), callable_mp(this, &EditorPropertyMultilineText::_open_big_text));
	hb->add_child(open_big_text);
	big_text_dialog = nullptr;
	big_text = nullptr;
	if (p_expression) {
		expression = true;
		Ref<EditorStandardSyntaxHighlighter> highlighter;
		highlighter.instantiate();
		text->set_syntax_highlighter(highlighter);
	}
}

///////////////////// TEXT ENUM /////////////////////////

void EditorPropertyTextEnum::_set_read_only(bool p_read_only) {
	option_button->set_disabled(p_read_only);
	edit_button->set_disabled(p_read_only);
}

void EditorPropertyTextEnum::_emit_changed_value(const String &p_string) {
	if (string_name) {
		emit_changed(get_edited_property(), StringName(p_string));
	} else {
		emit_changed(get_edited_property(), p_string);
	}
}

void EditorPropertyTextEnum::_option_selected(int p_which) {
	_emit_changed_value(option_button->get_item_text(p_which));
}

void EditorPropertyTextEnum::_edit_custom_value() {
	default_layout->hide();
	edit_custom_layout->show();
	custom_value_edit->grab_focus();
}

void EditorPropertyTextEnum::_custom_value_submitted(const String &p_value) {
	edit_custom_layout->hide();
	default_layout->show();

	_emit_changed_value(p_value.strip_edges());
}

void EditorPropertyTextEnum::_custom_value_accepted() {
	String new_value = custom_value_edit->get_text().strip_edges();
	_custom_value_submitted(new_value);
}

void EditorPropertyTextEnum::_custom_value_canceled() {
	custom_value_edit->set_text(get_edited_property_value());

	edit_custom_layout->hide();
	default_layout->show();
}

void EditorPropertyTextEnum::update_property() {
	String current_value = get_edited_property_value();
	int default_option = options.find(current_value);

	// The list can change in the loose mode.
	if (loose_mode) {
		custom_value_edit->set_text(current_value);
		option_button->clear();

		// Manually entered value.
		if (default_option < 0 && !current_value.is_empty()) {
			option_button->add_item(current_value, options.size() + 1001);
			option_button->select(0);

			option_button->add_separator();
		}

		// Add an explicit empty value for clearing the property.
		option_button->add_item("", options.size() + 1000);

		for (int i = 0; i < options.size(); i++) {
			option_button->add_item(options[i], i);
			if (options[i] == current_value) {
				option_button->select(option_button->get_item_count() - 1);
			}
		}
	} else {
		option_button->select(default_option);
	}
}

void EditorPropertyTextEnum::setup(const Vector<String> &p_options, bool p_string_name, bool p_loose_mode) {
	string_name = p_string_name;
	loose_mode = p_loose_mode;

	options.clear();

	if (loose_mode) {
		// Add an explicit empty value for clearing the property in the loose mode.
		option_button->add_item("", options.size() + 1000);
	}

	for (int i = 0; i < p_options.size(); i++) {
		options.append(p_options[i]);
		option_button->add_item(p_options[i], i);
	}

	if (loose_mode) {
		edit_button->show();
	}
}

void EditorPropertyTextEnum::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE:
		case NOTIFICATION_THEME_CHANGED: {
			edit_button->set_icon(get_editor_theme_icon(SNAME("Edit")));
			accept_button->set_icon(get_editor_theme_icon(SNAME("ImportCheck")));
			cancel_button->set_icon(get_editor_theme_icon(SNAME("ImportFail")));
		} break;
	}
}

EditorPropertyTextEnum::EditorPropertyTextEnum() {
	HBoxContainer *hb = memnew(HBoxContainer);
	add_child(hb);

	default_layout = memnew(HBoxContainer);
	default_layout->set_h_size_flags(SIZE_EXPAND_FILL);
	hb->add_child(default_layout);

	edit_custom_layout = memnew(HBoxContainer);
	edit_custom_layout->set_h_size_flags(SIZE_EXPAND_FILL);
	edit_custom_layout->hide();
	hb->add_child(edit_custom_layout);

	option_button = memnew(OptionButton);
	option_button->set_h_size_flags(SIZE_EXPAND_FILL);
	option_button->set_clip_text(true);
	option_button->set_flat(true);
	option_button->set_auto_translate_mode(AUTO_TRANSLATE_MODE_DISABLED);
	default_layout->add_child(option_button);
	option_button->connect(SceneStringName(item_selected), callable_mp(this, &EditorPropertyTextEnum::_option_selected));

	edit_button = memnew(Button);
	edit_button->set_flat(true);
	edit_button->hide();
	default_layout->add_child(edit_button);
	edit_button->connect(SceneStringName(pressed), callable_mp(this, &EditorPropertyTextEnum::_edit_custom_value));

	custom_value_edit = memnew(LineEdit);
	custom_value_edit->set_h_size_flags(SIZE_EXPAND_FILL);
	edit_custom_layout->add_child(custom_value_edit);
	custom_value_edit->connect("text_submitted", callable_mp(this, &EditorPropertyTextEnum::_custom_value_submitted));

	accept_button = memnew(Button);
	accept_button->set_flat(true);
	edit_custom_layout->add_child(accept_button);
	accept_button->connect(SceneStringName(pressed), callable_mp(this, &EditorPropertyTextEnum::_custom_value_accepted));

	cancel_button = memnew(Button);
	cancel_button->set_flat(true);
	edit_custom_layout->add_child(cancel_button);
	cancel_button->connect(SceneStringName(pressed), callable_mp(this, &EditorPropertyTextEnum::_custom_value_canceled));

	add_focusable(option_button);
	add_focusable(edit_button);
	add_focusable(custom_value_edit);
	add_focusable(accept_button);
	add_focusable(cancel_button);
}

//////////////////// LOCALE ////////////////////////

void EditorPropertyLocale::_locale_selected(const String &p_locale) {
	emit_changed(get_edited_property(), p_locale);
	update_property();
}

void EditorPropertyLocale::_locale_pressed() {
	if (!dialog) {
		dialog = memnew(EditorLocaleDialog);
		dialog->connect("locale_selected", callable_mp(this, &EditorPropertyLocale::_locale_selected));
		add_child(dialog);
	}

	String locale_code = get_edited_property_value();
	dialog->set_locale(locale_code);
	dialog->popup_locale_dialog();
}

void EditorPropertyLocale::update_property() {
	String locale_code = get_edited_property_value();
	locale->set_text(locale_code);
	locale->set_tooltip_text(locale_code);
}

void EditorPropertyLocale::setup(const String &p_hint_text) {
}

void EditorPropertyLocale::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE:
		case NOTIFICATION_THEME_CHANGED: {
			locale_edit->set_icon(get_editor_theme_icon(SNAME("Translation")));
		} break;
	}
}

void EditorPropertyLocale::_locale_focus_exited() {
	_locale_selected(locale->get_text());
}

EditorPropertyLocale::EditorPropertyLocale() {
	HBoxContainer *locale_hb = memnew(HBoxContainer);
	add_child(locale_hb);
	locale = memnew(LineEdit);
	locale_hb->add_child(locale);
	locale->connect("text_submitted", callable_mp(this, &EditorPropertyLocale::_locale_selected));
	locale->connect(SceneStringName(focus_exited), callable_mp(this, &EditorPropertyLocale::_locale_focus_exited));
	locale->set_h_size_flags(SIZE_EXPAND_FILL);

	locale_edit = memnew(Button);
	locale_edit->set_clip_text(true);
	locale_hb->add_child(locale_edit);
	add_focusable(locale);
	dialog = nullptr;
	locale_edit->connect(SceneStringName(pressed), callable_mp(this, &EditorPropertyLocale::_locale_pressed));
}

///////////////////// PATH /////////////////////////

void EditorPropertyPath::_set_read_only(bool p_read_only) {
	path->set_editable(!p_read_only);
	path_edit->set_disabled(p_read_only);
}

void EditorPropertyPath::_path_selected(const String &p_path) {
	emit_changed(get_edited_property(), p_path);
	update_property();
}

void EditorPropertyPath::_path_pressed() {
	if (!dialog) {
		dialog = memnew(EditorFileDialog);
		dialog->connect("file_selected", callable_mp(this, &EditorPropertyPath::_path_selected));
		dialog->connect("dir_selected", callable_mp(this, &EditorPropertyPath::_path_selected));
		add_child(dialog);
	}

	String full_path = get_edited_property_value();

	dialog->clear_filters();

	if (global) {
		dialog->set_access(EditorFileDialog::ACCESS_FILESYSTEM);
	} else {
		dialog->set_access(EditorFileDialog::ACCESS_RESOURCES);
	}

	if (folder) {
		dialog->set_file_mode(EditorFileDialog::FILE_MODE_OPEN_DIR);
		dialog->set_current_dir(full_path);
	} else {
		dialog->set_file_mode(save_mode ? EditorFileDialog::FILE_MODE_SAVE_FILE : EditorFileDialog::FILE_MODE_OPEN_FILE);
		for (int i = 0; i < extensions.size(); i++) {
			String e = extensions[i].strip_edges();
			if (!e.is_empty()) {
				dialog->add_filter(extensions[i].strip_edges());
			}
		}
		dialog->set_current_path(full_path);
	}

	dialog->popup_file_dialog();
}

void EditorPropertyPath::update_property() {
	String full_path = get_edited_property_value();
	path->set_text(full_path);
	path->set_tooltip_text(full_path);
}

void EditorPropertyPath::setup(const Vector<String> &p_extensions, bool p_folder, bool p_global) {
	extensions = p_extensions;
	folder = p_folder;
	global = p_global;
}

void EditorPropertyPath::set_save_mode() {
	save_mode = true;
}

void EditorPropertyPath::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE:
		case NOTIFICATION_THEME_CHANGED: {
			if (folder) {
				path_edit->set_icon(get_editor_theme_icon(SNAME("FolderBrowse")));
			} else {
				path_edit->set_icon(get_editor_theme_icon(SNAME("FileBrowse")));
			}
		} break;
	}
}

void EditorPropertyPath::_path_focus_exited() {
	_path_selected(path->get_text());
}

void EditorPropertyPath::_drop_data_fw(const Point2 &p_point, const Variant &p_data, Control *p_from) {
	const Dictionary drag_data = p_data;
	if (!drag_data.has("type")) {
		return;
	}
	if (String(drag_data["type"]) != "files") {
		return;
	}
	const Vector<String> filesPaths = drag_data["files"];
	if (filesPaths.size() == 0) {
		return;
	}

	emit_changed(get_edited_property(), filesPaths[0]);
	update_property();
}

bool EditorPropertyPath::_can_drop_data_fw(const Point2 &p_point, const Variant &p_data, Control *p_from) const {
	const Dictionary drag_data = p_data;
	if (!drag_data.has("type")) {
		return false;
	}
	if (String(drag_data["type"]) != "files") {
		return false;
	}
	const Vector<String> filesPaths = drag_data["files"];
	if (filesPaths.size() == 0) {
		return false;
	}

	for (const String &extension : extensions) {
		if (filesPaths[0].ends_with(extension.substr(1, extension.size() - 1))) {
			return true;
		}
	}

	return false;
}

EditorPropertyPath::EditorPropertyPath() {
	HBoxContainer *path_hb = memnew(HBoxContainer);
	add_child(path_hb);
	path = memnew(LineEdit);
	SET_DRAG_FORWARDING_CDU(path, EditorPropertyPath);
	path->set_structured_text_bidi_override(TextServer::STRUCTURED_TEXT_FILE);
	path_hb->add_child(path);
	path->connect("text_submitted", callable_mp(this, &EditorPropertyPath::_path_selected));
	path->connect(SceneStringName(focus_exited), callable_mp(this, &EditorPropertyPath::_path_focus_exited));
	path->set_h_size_flags(SIZE_EXPAND_FILL);

	path_edit = memnew(Button);
	path_edit->set_clip_text(true);
	path_hb->add_child(path_edit);
	add_focusable(path);
	dialog = nullptr;
	path_edit->connect(SceneStringName(pressed), callable_mp(this, &EditorPropertyPath::_path_pressed));
}

///////////////////// CLASS NAME /////////////////////////

void EditorPropertyClassName::_set_read_only(bool p_read_only) {
	property->set_disabled(p_read_only);
}

void EditorPropertyClassName::setup(const String &p_base_type, const String &p_selected_type) {
	base_type = p_base_type;
	dialog->set_base_type(base_type);
	selected_type = p_selected_type;
	property->set_text(selected_type);
}

void EditorPropertyClassName::update_property() {
	String s = get_edited_property_value();
	property->set_text(s);
	selected_type = s;
}

void EditorPropertyClassName::_property_selected() {
	dialog->popup_create(true, true, get_edited_property_value(), get_edited_property());
}

void EditorPropertyClassName::_dialog_created() {
	selected_type = dialog->get_selected_type();
	emit_changed(get_edited_property(), selected_type);
	update_property();
}

EditorPropertyClassName::EditorPropertyClassName() {
	property = memnew(Button);
	property->set_clip_text(true);
	add_child(property);
	add_focusable(property);
	property->set_text(selected_type);
	property->connect(SceneStringName(pressed), callable_mp(this, &EditorPropertyClassName::_property_selected));
	dialog = memnew(CreateDialog);
	dialog->set_base_type(base_type);
	dialog->connect("create", callable_mp(this, &EditorPropertyClassName::_dialog_created));
	add_child(dialog);
}

///////////////////// CHECK /////////////////////////

void EditorPropertyCheck::_set_read_only(bool p_read_only) {
	checkbox->set_disabled(p_read_only);
}

void EditorPropertyCheck::_checkbox_pressed() {
	emit_changed(get_edited_property(), checkbox->is_pressed());
}

void EditorPropertyCheck::update_property() {
	bool c = get_edited_property_value();
	checkbox->set_pressed(c);
	checkbox->set_disabled(is_read_only());
}

EditorPropertyCheck::EditorPropertyCheck() {
	checkbox = memnew(CheckBox);
	checkbox->set_text(TTR("On"));
	add_child(checkbox);
	add_focusable(checkbox);
	checkbox->connect(SceneStringName(pressed), callable_mp(this, &EditorPropertyCheck::_checkbox_pressed));
}

///////////////////// ENUM /////////////////////////

void EditorPropertyEnum::_set_read_only(bool p_read_only) {
	options->set_disabled(p_read_only);
}

void EditorPropertyEnum::_option_selected(int p_which) {
	int64_t val = options->get_item_metadata(p_which);
	emit_changed(get_edited_property(), val);
}

void EditorPropertyEnum::update_property() {
	Variant current = get_edited_property_value();
	if (current.get_type() == Variant::NIL) {
		options->select(-1);
		return;
	}

	int64_t which = current;
	for (int i = 0; i < options->get_item_count(); i++) {
		if (which == (int64_t)options->get_item_metadata(i)) {
			options->select(i);
			return;
		}
	}
}

void EditorPropertyEnum::setup(const Vector<String> &p_options) {
	options->clear();
	HashMap<int64_t, Vector<String>> items;
	int64_t current_val = 0;
	for (const String &option : p_options) {
		Vector<String> text_split = option.split(":");
		if (text_split.size() != 1) {
			current_val = text_split[1].to_int();
		}
		items[current_val].push_back(text_split[0]);
		current_val += 1;
	}

	for (const KeyValue<int64_t, Vector<String>> &K : items) {
		options->add_item(String(", ").join(K.value));
		options->set_item_metadata(-1, K.key);
	}
}

void EditorPropertyEnum::set_option_button_clip(bool p_enable) {
	options->set_clip_text(p_enable);
}

EditorPropertyEnum::EditorPropertyEnum() {
	options = memnew(OptionButton);
	options->set_clip_text(true);
	options->set_flat(true);
	options->set_auto_translate_mode(AUTO_TRANSLATE_MODE_DISABLED);
	add_child(options);
	add_focusable(options);
	options->connect(SceneStringName(item_selected), callable_mp(this, &EditorPropertyEnum::_option_selected));
}

///////////////////// FLAGS /////////////////////////

void EditorPropertyFlags::_set_read_only(bool p_read_only) {
	for (CheckBox *check : flags) {
		check->set_disabled(p_read_only);
	}
}

void EditorPropertyFlags::_flag_toggled(int p_index) {
	uint32_t value = get_edited_property_value();
	if (flags[p_index]->is_pressed()) {
		value |= flag_values[p_index];
	} else {
		value &= ~flag_values[p_index];
	}

	emit_changed(get_edited_property(), value);
}

void EditorPropertyFlags::update_property() {
	uint32_t value = get_edited_property_value();

	for (int i = 0; i < flags.size(); i++) {
		flags[i]->set_pressed((value & flag_values[i]) == flag_values[i]);
	}
}

void EditorPropertyFlags::setup(const Vector<String> &p_options) {
	ERR_FAIL_COND(flags.size());

	bool first = true;
	uint32_t current_val;
	for (int i = 0; i < p_options.size(); i++) {
		// An empty option is not considered a "flag".
		String option = p_options[i].strip_edges();
		if (option.is_empty()) {
			continue;
		}
		const int flag_index = flags.size(); // Index of the next element (added by the code below).

		// Value for a flag can be explicitly overridden.
		Vector<String> text_split = option.split(":");
		if (text_split.size() != 1) {
			current_val = text_split[1].to_int();
		} else {
			current_val = 1 << i;
		}
		flag_values.push_back(current_val);

		// Create a CheckBox for the current flag.
		CheckBox *cb = memnew(CheckBox);
		cb->set_text(text_split[0]);
		cb->set_clip_text(true);
		cb->connect(SceneStringName(pressed), callable_mp(this, &EditorPropertyFlags::_flag_toggled).bind(flag_index));
		add_focusable(cb);
		vbox->add_child(cb);
		flags.push_back(cb);

		// Can't use `i == 0` because we want to find the first none-empty option.
		if (first) {
			set_label_reference(cb);
			first = false;
		}
	}
}

EditorPropertyFlags::EditorPropertyFlags() {
	vbox = memnew(VBoxContainer);
	add_child(vbox);
}

///////////////////// LAYERS /////////////////////////

void EditorPropertyLayersGrid::_rename_pressed(int p_menu) {
	// Show rename popup for active layer.
	if (renamed_layer_index == INT32_MAX) {
		return;
	}
	String name = names[renamed_layer_index];
	rename_dialog->set_title(vformat(TTR("Renaming layer %d:"), renamed_layer_index + 1));
	rename_dialog_text->set_text(name);
	rename_dialog_text->select(0, name.length());
	rename_dialog->popup_centered(Size2(300, 80) * EDSCALE);
	rename_dialog_text->grab_focus();
}

void EditorPropertyLayersGrid::_rename_operation_confirm() {
	String new_name = rename_dialog_text->get_text().strip_edges();
	if (new_name.length() == 0) {
		EditorNode::get_singleton()->show_warning(TTR("No name provided."));
		return;
	} else if (new_name.contains("/") || new_name.contains("\\") || new_name.contains(":")) {
		EditorNode::get_singleton()->show_warning(TTR("Name contains invalid characters."));
		return;
	}
	names.set(renamed_layer_index, new_name);
	tooltips.set(renamed_layer_index, new_name + "\n" + vformat(TTR("Bit %d, value %d"), renamed_layer_index, 1 << renamed_layer_index));
	emit_signal(SNAME("rename_confirmed"), renamed_layer_index, new_name);
}

EditorPropertyLayersGrid::EditorPropertyLayersGrid() {
	rename_dialog = memnew(ConfirmationDialog);
	VBoxContainer *rename_dialog_vb = memnew(VBoxContainer);
	rename_dialog->add_child(rename_dialog_vb);
	rename_dialog_text = memnew(LineEdit);
	rename_dialog_vb->add_margin_child(TTR("Name:"), rename_dialog_text);
	rename_dialog->set_ok_button_text(TTR("Rename"));
	add_child(rename_dialog);
	rename_dialog->register_text_enter(rename_dialog_text);
	rename_dialog->connect(SceneStringName(confirmed), callable_mp(this, &EditorPropertyLayersGrid::_rename_operation_confirm));
	layer_rename = memnew(PopupMenu);
	layer_rename->add_item(TTR("Rename layer"), 0);
	add_child(layer_rename);
	layer_rename->connect(SceneStringName(id_pressed), callable_mp(this, &EditorPropertyLayersGrid::_rename_pressed));
}

Size2 EditorPropertyLayersGrid::get_grid_size() const {
	Ref<Font> font = get_theme_font(SceneStringName(font), SNAME("Label"));
	int font_size = get_theme_font_size(SceneStringName(font_size), SNAME("Label"));
	return Vector2(0, font->get_height(font_size) * 3);
}

void EditorPropertyLayersGrid::set_read_only(bool p_read_only) {
	read_only = p_read_only;
}

Size2 EditorPropertyLayersGrid::get_minimum_size() const {
	Size2 min_size = get_grid_size();

	// Add extra rows when expanded.
	if (expanded) {
		const int bsize = (min_size.height * 80 / 100) / 2;
		for (int i = 0; i < expansion_rows; ++i) {
			min_size.y += 2 * (bsize + 1) + 3;
		}
	}

	return min_size;
}

String EditorPropertyLayersGrid::get_tooltip(const Point2 &p_pos) const {
	for (int i = 0; i < flag_rects.size(); i++) {
		if (i < tooltips.size() && flag_rects[i].has_point(p_pos)) {
			return tooltips[i];
		}
	}
	return String();
}

void EditorPropertyLayersGrid::_update_hovered(const Vector2 &p_position) {
	bool expand_was_hovered = expand_hovered;
	expand_hovered = expand_rect.has_point(p_position);
	if (expand_hovered != expand_was_hovered) {
		queue_redraw();
	}

	if (!expand_hovered) {
		for (int i = 0; i < flag_rects.size(); i++) {
			if (flag_rects[i].has_point(p_position)) {
				// Used to highlight the hovered flag in the layers grid.
				hovered_index = i;
				queue_redraw();
				return;
			}
		}
	}

	// Remove highlight when no square is hovered.
	if (hovered_index != INT32_MAX) {
		hovered_index = INT32_MAX;
		queue_redraw();
	}
}

void EditorPropertyLayersGrid::_on_hover_exit() {
	if (expand_hovered) {
		expand_hovered = false;
		queue_redraw();
	}
	if (hovered_index != INT32_MAX) {
		hovered_index = INT32_MAX;
		queue_redraw();
	}
}

void EditorPropertyLayersGrid::_update_flag(bool p_replace) {
	if (hovered_index != INT32_MAX) {
		// Toggle the flag.
		// We base our choice on the hovered flag, so that it always matches the hovered flag.
		if (p_replace) {
			// Replace all flags with the hovered flag ("solo mode"),
			// instead of toggling the hovered flags while preserving other flags' state.
			if (value == uint32_t(1 << hovered_index)) {
				// If the flag is already enabled, enable all other items and disable the current flag.
				// This allows for quicker toggling.
				value = INT32_MAX - (1 << hovered_index);
			} else {
				value = 1 << hovered_index;
			}
		} else {
			if (value & (1 << hovered_index)) {
				value &= ~(1 << hovered_index);
			} else {
				value |= (1 << hovered_index);
			}
		}

		emit_signal(SNAME("flag_changed"), value);
		queue_redraw();
	} else if (expand_hovered) {
		expanded = !expanded;
		update_minimum_size();
		queue_redraw();
	}
}

void EditorPropertyLayersGrid::gui_input(const Ref<InputEvent> &p_ev) {
	if (read_only) {
		return;
	}
	const Ref<InputEventMouseMotion> mm = p_ev;
	if (mm.is_valid()) {
		_update_hovered(mm->get_position());
		return;
	}

	const Ref<InputEventMouseButton> mb = p_ev;
	if (mb.is_valid() && mb->get_button_index() == MouseButton::LEFT && mb->is_pressed()) {
		_update_hovered(mb->get_position());
		_update_flag(mb->is_command_or_control_pressed());
	}
	if (mb.is_valid() && mb->get_button_index() == MouseButton::RIGHT && mb->is_pressed()) {
		if (hovered_index != INT32_MAX) {
			renamed_layer_index = hovered_index;
			layer_rename->set_position(get_screen_position() + mb->get_position());
			layer_rename->reset_size();
			layer_rename->popup();
		}
	}
}

void EditorPropertyLayersGrid::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_DRAW: {
			Size2 grid_size = get_grid_size();
			grid_size.x = get_size().x;

			flag_rects.clear();

			int prev_expansion_rows = expansion_rows;
			expansion_rows = 0;

			const int bsize = (grid_size.height * 80 / 100) / 2;
			const int h = bsize * 2 + 1;

			Color color = get_theme_color(read_only ? SNAME("highlight_disabled_color") : SNAME("highlight_color"), EditorStringName(Editor));

			Color text_color = get_theme_color(read_only ? SNAME("font_disabled_color") : SceneStringName(font_color), EditorStringName(Editor));
			text_color.a *= 0.5;

			Color text_color_on = get_theme_color(read_only ? SNAME("font_disabled_color") : SNAME("font_hover_color"), EditorStringName(Editor));
			text_color_on.a *= 0.7;

			const int vofs = (grid_size.height - h) / 2;

			uint32_t layer_index = 0;

			Point2 arrow_pos;

			Point2 block_ofs(4, vofs);

			while (true) {
				Point2 ofs = block_ofs;

				for (int i = 0; i < 2; i++) {
					for (int j = 0; j < layer_group_size; j++) {
						const bool on = value & (1 << layer_index);
						Rect2 rect2 = Rect2(ofs, Size2(bsize, bsize));

						color.a = on ? 0.6 : 0.2;
						if (layer_index == hovered_index) {
							// Add visual feedback when hovering a flag.
							color.a += 0.15;
						}

						draw_rect(rect2, color);
						flag_rects.push_back(rect2);

						Ref<Font> font = get_theme_font(SceneStringName(font), SNAME("Label"));
						int font_size = get_theme_font_size(SceneStringName(font_size), SNAME("Label"));
						Vector2 offset;
						offset.y = rect2.size.y * 0.75;

						draw_string(font, rect2.position + offset, itos(layer_index + 1), HORIZONTAL_ALIGNMENT_CENTER, rect2.size.x, font_size, on ? text_color_on : text_color);

						ofs.x += bsize + 1;

						++layer_index;
					}

					ofs.x = block_ofs.x;
					ofs.y += bsize + 1;
				}

				if (layer_index >= layer_count) {
					if (!flag_rects.is_empty() && (expansion_rows == 0)) {
						const Rect2 &last_rect = flag_rects[flag_rects.size() - 1];
						arrow_pos = last_rect.get_end();
					}
					break;
				}

				int block_size_x = layer_group_size * (bsize + 1);
				block_ofs.x += block_size_x + 3;

				if (block_ofs.x + block_size_x + 12 > grid_size.width) {
					// Keep last valid cell position for the expansion icon.
					if (!flag_rects.is_empty() && (expansion_rows == 0)) {
						const Rect2 &last_rect = flag_rects[flag_rects.size() - 1];
						arrow_pos = last_rect.get_end();
					}
					++expansion_rows;

					if (expanded) {
						// Expand grid to next line.
						block_ofs.x = 4;
						block_ofs.y += 2 * (bsize + 1) + 3;
					} else {
						// Skip remaining blocks.
						break;
					}
				}
			}

			if ((expansion_rows != prev_expansion_rows) && expanded) {
				update_minimum_size();
			}

			if ((expansion_rows == 0) && (layer_index == layer_count)) {
				// Whole grid was drawn, no need for expansion icon.
				break;
			}

			Ref<Texture2D> arrow = get_theme_icon(SNAME("arrow"), SNAME("Tree"));
			ERR_FAIL_COND(arrow.is_null());

			Color arrow_color = get_theme_color(SNAME("highlight_color"), EditorStringName(Editor));
			arrow_color.a = expand_hovered ? 1.0 : 0.6;

			arrow_pos.x += 2.0;
			arrow_pos.y -= arrow->get_height();

			Rect2 arrow_draw_rect(arrow_pos, arrow->get_size());
			expand_rect = arrow_draw_rect;
			if (expanded) {
				arrow_draw_rect.size.y *= -1.0; // Flip arrow vertically when expanded.
			}

			RID ci = get_canvas_item();
			arrow->draw_rect(ci, arrow_draw_rect, false, arrow_color);

		} break;

		case NOTIFICATION_MOUSE_EXIT: {
			_on_hover_exit();
		} break;
	}
}

void EditorPropertyLayersGrid::set_flag(uint32_t p_flag) {
	value = p_flag;
	queue_redraw();
}

void EditorPropertyLayersGrid::_bind_methods() {
	ADD_SIGNAL(MethodInfo("flag_changed", PropertyInfo(Variant::INT, "flag")));
	ADD_SIGNAL(MethodInfo("rename_confirmed", PropertyInfo(Variant::INT, "layer_id"), PropertyInfo(Variant::STRING, "new_name")));
}

void EditorPropertyLayers::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE:
		case NOTIFICATION_THEME_CHANGED: {
			button->set_texture_normal(get_editor_theme_icon(SNAME("GuiTabMenuHl")));
			button->set_texture_pressed(get_editor_theme_icon(SNAME("GuiTabMenuHl")));
			button->set_texture_disabled(get_editor_theme_icon(SNAME("GuiTabMenu")));
		} break;
	}
}

void EditorPropertyLayers::_set_read_only(bool p_read_only) {
	button->set_disabled(p_read_only);
	grid->set_read_only(p_read_only);
}

void EditorPropertyLayers::_grid_changed(uint32_t p_grid) {
	emit_changed(get_edited_property(), p_grid);
}

void EditorPropertyLayers::update_property() {
	uint32_t value = get_edited_property_value();

	grid->set_flag(value);
}

void EditorPropertyLayers::setup(LayerType p_layer_type) {
	layer_type = p_layer_type;
	int layer_group_size = 0;
	int layer_count = 0;
	switch (p_layer_type) {
		case LAYER_RENDER_2D: {
			basename = "layer_names/2d_render";
			layer_group_size = 5;
			layer_count = 20;
		} break;

		case LAYER_PHYSICS_2D: {
			basename = "layer_names/2d_physics";
			layer_group_size = 4;
			layer_count = 32;
		} break;

		case LAYER_NAVIGATION_2D: {
			basename = "layer_names/2d_navigation";
			layer_group_size = 4;
			layer_count = 32;
		} break;

		case LAYER_RENDER_3D: {
			basename = "layer_names/3d_render";
			layer_group_size = 5;
			layer_count = 20;
		} break;

		case LAYER_PHYSICS_3D: {
			basename = "layer_names/3d_physics";
			layer_group_size = 4;
			layer_count = 32;
		} break;

		case LAYER_NAVIGATION_3D: {
			basename = "layer_names/3d_navigation";
			layer_group_size = 4;
			layer_count = 32;
		} break;

		case LAYER_AVOIDANCE: {
			basename = "layer_names/avoidance";
			layer_group_size = 4;
			layer_count = 32;
		} break;
	}

	Vector<String> names;
	Vector<String> tooltips;
	for (int i = 0; i < layer_count; i++) {
		String name;

		if (ProjectSettings::get_singleton()->has_setting(basename + vformat("/layer_%d", i + 1))) {
			name = GLOBAL_GET(basename + vformat("/layer_%d", i + 1));
		}

		if (name.is_empty()) {
			name = vformat(TTR("Layer %d"), i + 1);
		}

		names.push_back(name);
		tooltips.push_back(name + "\n" + vformat(TTR("Bit %d, value %d"), i, 1 << i));
	}

	grid->names = names;
	grid->tooltips = tooltips;
	grid->layer_group_size = layer_group_size;
	grid->layer_count = layer_count;
}

void EditorPropertyLayers::set_layer_name(int p_index, const String &p_name) {
	const String property_name = basename + vformat("/layer_%d", p_index + 1);
	if (ProjectSettings::get_singleton()->has_setting(property_name)) {
		ProjectSettings::get_singleton()->set(property_name, p_name);
		ProjectSettings::get_singleton()->save();
	}
}

String EditorPropertyLayers::get_layer_name(int p_index) const {
	const String property_name = basename + vformat("/layer_%d", p_index + 1);
	if (ProjectSettings::get_singleton()->has_setting(property_name)) {
		return GLOBAL_GET(property_name);
	}
	return String();
}

void EditorPropertyLayers::_button_pressed() {
	int layer_count = grid->layer_count;
	layers->clear();
	for (int i = 0; i < layer_count; i++) {
		const String name = get_layer_name(i);
		if (name.is_empty()) {
			continue;
		}
		layers->add_check_item(name, i);
		int idx = layers->get_item_index(i);
		layers->set_item_checked(idx, grid->value & (1 << i));
	}

	if (layers->get_item_count() == 0) {
		layers->add_item(TTR("No Named Layers"));
		layers->set_item_disabled(0, true);
	}
	layers->add_separator();
	layers->add_icon_item(get_editor_theme_icon("Edit"), TTR("Edit Layer Names"), grid->layer_count);

	Rect2 gp = button->get_screen_rect();
	layers->reset_size();
	Vector2 popup_pos = gp.position - Vector2(layers->get_contents_minimum_size().x, 0);
	layers->set_position(popup_pos);
	layers->popup();
}

void EditorPropertyLayers::_menu_pressed(int p_menu) {
	if (uint32_t(p_menu) == grid->layer_count) {
		ProjectSettingsEditor::get_singleton()->popup_project_settings(true);
		ProjectSettingsEditor::get_singleton()->set_general_page(basename);
	} else {
		if (grid->value & (1 << p_menu)) {
			grid->value &= ~(1 << p_menu);
		} else {
			grid->value |= (1 << p_menu);
		}
		grid->queue_redraw();
		layers->set_item_checked(layers->get_item_index(p_menu), grid->value & (1 << p_menu));
		_grid_changed(grid->value);
	}
}

void EditorPropertyLayers::_refresh_names() {
	setup(layer_type);
}

EditorPropertyLayers::EditorPropertyLayers() {
	HBoxContainer *hb = memnew(HBoxContainer);
	hb->set_clip_contents(true);
	add_child(hb);
	grid = memnew(EditorPropertyLayersGrid);
	grid->connect("flag_changed", callable_mp(this, &EditorPropertyLayers::_grid_changed));
	grid->connect("rename_confirmed", callable_mp(this, &EditorPropertyLayers::set_layer_name));
	grid->set_h_size_flags(SIZE_EXPAND_FILL);
	hb->add_child(grid);

	button = memnew(TextureButton);
	button->set_stretch_mode(TextureButton::STRETCH_KEEP_CENTERED);
	button->set_toggle_mode(true);
	button->connect(SceneStringName(pressed), callable_mp(this, &EditorPropertyLayers::_button_pressed));
	hb->add_child(button);

	set_bottom_editor(hb);

	layers = memnew(PopupMenu);
	add_child(layers);
	layers->set_hide_on_checkable_item_selection(false);
	layers->connect(SceneStringName(id_pressed), callable_mp(this, &EditorPropertyLayers::_menu_pressed));
	layers->connect("popup_hide", callable_mp((BaseButton *)button, &BaseButton::set_pressed).bind(false));
	ProjectSettings::get_singleton()->connect("settings_changed", callable_mp(this, &EditorPropertyLayers::_refresh_names));
}

///////////////////// INT /////////////////////////

void EditorPropertyInteger::_set_read_only(bool p_read_only) {
	spin->set_read_only(p_read_only);
}

void EditorPropertyInteger::_value_changed(int64_t val) {
	emit_changed(get_edited_property(), val);
}

void EditorPropertyInteger::update_property() {
	int64_t val = get_edited_property_value();
	spin->set_value_no_signal(val);
#ifdef DEBUG_ENABLED
	// If spin (currently EditorSplinSlider : Range) is changed so that it can use int64_t, then the below warning wouldn't be a problem.
	if (val != (int64_t)(double)(val)) {
		WARN_PRINT("Cannot reliably represent '" + itos(val) + "' in the inspector, value is too large.");
	}
#endif
}

void EditorPropertyInteger::setup(int64_t p_min, int64_t p_max, int64_t p_step, bool p_hide_slider, bool p_allow_greater, bool p_allow_lesser, const String &p_suffix) {
	spin->set_min(p_min);
	spin->set_max(p_max);
	spin->set_step(p_step);
	spin->set_hide_slider(p_hide_slider);
	spin->set_allow_greater(p_allow_greater);
	spin->set_allow_lesser(p_allow_lesser);
	spin->set_suffix(p_suffix);
}

EditorPropertyInteger::EditorPropertyInteger() {
	spin = memnew(EditorSpinSlider);
	spin->set_flat(true);
	add_child(spin);
	add_focusable(spin);
	spin->connect(SceneStringName(value_changed), callable_mp(this, &EditorPropertyInteger::_value_changed));
}

///////////////////// OBJECT ID /////////////////////////

void EditorPropertyObjectID::_set_read_only(bool p_read_only) {
	edit->set_disabled(p_read_only);
}

void EditorPropertyObjectID::_edit_pressed() {
	emit_signal(SNAME("object_id_selected"), get_edited_property(), get_edited_property_value());
}

void EditorPropertyObjectID::update_property() {
	String type = base_type;
	if (type.is_empty()) {
		type = "Object";
	}

	ObjectID id = get_edited_property_value();
	if (id.is_valid()) {
		edit->set_text(type + " ID: " + uitos(id));
		edit->set_tooltip_text(type + " ID: " + uitos(id));
		edit->set_disabled(false);
		edit->set_icon(EditorNode::get_singleton()->get_class_icon(type));
	} else {
		edit->set_text(TTR("<empty>"));
		edit->set_tooltip_text("");
		edit->set_disabled(true);
		edit->set_icon(Ref<Texture2D>());
	}
}

void EditorPropertyObjectID::setup(const String &p_base_type) {
	base_type = p_base_type;
}

EditorPropertyObjectID::EditorPropertyObjectID() {
	edit = memnew(Button);
	add_child(edit);
	add_focusable(edit);
	edit->set_text_overrun_behavior(TextServer::OVERRUN_TRIM_ELLIPSIS);
	edit->connect(SceneStringName(pressed), callable_mp(this, &EditorPropertyObjectID::_edit_pressed));
}

///////////////////// SIGNAL /////////////////////////

void EditorPropertySignal::_edit_pressed() {
	Signal signal = get_edited_property_value();
	emit_signal(SNAME("object_id_selected"), get_edited_property(), signal.get_object_id());
}

void EditorPropertySignal::update_property() {
	String type = base_type;

	Signal signal = get_edited_property_value();

	edit->set_text("Signal: " + signal.get_name());
	edit->set_disabled(false);
	edit->set_icon(get_editor_theme_icon(SNAME("Signals")));
}

EditorPropertySignal::EditorPropertySignal() {
	edit = memnew(Button);
	add_child(edit);
	add_focusable(edit);
	edit->connect(SceneStringName(pressed), callable_mp(this, &EditorPropertySignal::_edit_pressed));
}

///////////////////// CALLABLE /////////////////////////

void EditorPropertyCallable::update_property() {
	String type = base_type;

	Callable callable = get_edited_property_value();

	edit->set_text("Callable");
	edit->set_disabled(true);
	edit->set_icon(get_editor_theme_icon(SNAME("Callable")));
}

EditorPropertyCallable::EditorPropertyCallable() {
	edit = memnew(Button);
	add_child(edit);
	add_focusable(edit);
}

///////////////////// FLOAT /////////////////////////

void EditorPropertyFloat::_set_read_only(bool p_read_only) {
	spin->set_read_only(p_read_only);
}

void EditorPropertyFloat::_value_changed(double val) {
	if (radians_as_degrees) {
		val = Math::deg_to_rad(val);
	}
	emit_changed(get_edited_property(), val);
}

void EditorPropertyFloat::update_property() {
	double val = get_edited_property_value();
	if (radians_as_degrees) {
		val = Math::rad_to_deg(val);
	}
	spin->set_value_no_signal(val);
}

void EditorPropertyFloat::setup(double p_min, double p_max, double p_step, bool p_hide_slider, bool p_exp_range, bool p_greater, bool p_lesser, const String &p_suffix, bool p_radians_as_degrees) {
	radians_as_degrees = p_radians_as_degrees;
	spin->set_min(p_min);
	spin->set_max(p_max);
	spin->set_step(p_step);
	spin->set_hide_slider(p_hide_slider);
	spin->set_exp_ratio(p_exp_range);
	spin->set_allow_greater(p_greater);
	spin->set_allow_lesser(p_lesser);
	spin->set_suffix(p_suffix);
}

EditorPropertyFloat::EditorPropertyFloat() {
	spin = memnew(EditorSpinSlider);
	spin->set_flat(true);
	add_child(spin);
	add_focusable(spin);
	spin->connect(SceneStringName(value_changed), callable_mp(this, &EditorPropertyFloat::_value_changed));
}

///////////////////// EASING /////////////////////////

void EditorPropertyEasing::_set_read_only(bool p_read_only) {
	spin->set_read_only(p_read_only);
}

void EditorPropertyEasing::_drag_easing(const Ref<InputEvent> &p_ev) {
	if (is_read_only()) {
		return;
	}
	const Ref<InputEventMouseButton> mb = p_ev;
	if (mb.is_valid()) {
		if (mb->is_double_click() && mb->get_button_index() == MouseButton::LEFT) {
			_setup_spin();
		}

		if (mb->is_pressed() && mb->get_button_index() == MouseButton::RIGHT) {
			preset->set_position(easing_draw->get_screen_position() + mb->get_position());
			preset->reset_size();
			preset->popup();

			// Ensure the easing doesn't appear as being dragged
			dragging = false;
			easing_draw->queue_redraw();
		}

		if (mb->get_button_index() == MouseButton::LEFT) {
			dragging = mb->is_pressed();
			// Update to display the correct dragging color
			easing_draw->queue_redraw();
		}
	}

	const Ref<InputEventMouseMotion> mm = p_ev;

	if (dragging && mm.is_valid() && (mm->get_button_mask().has_flag(MouseButtonMask::LEFT))) {
		float rel = mm->get_relative().x;
		if (rel == 0) {
			return;
		}

		if (flip) {
			rel = -rel;
		}

		float val = get_edited_property_value();
		bool sg = val < 0;
		val = Math::absf(val);

		val = Math::log(val) / Math::log((float)2.0);
		// Logarithmic space.
		val += rel * 0.05;

		val = Math::pow(2.0f, val);
		if (sg) {
			val = -val;
		}

		// 0 is a singularity, but both positive and negative values
		// are otherwise allowed. Enforce 0+ as workaround.
		if (Math::is_zero_approx(val)) {
			val = 0.00001;
		}

		// Limit to a reasonable value to prevent the curve going into infinity,
		// which can cause crashes and other issues.
		val = CLAMP(val, -1'000'000, 1'000'000);

		emit_changed(get_edited_property(), val);
		easing_draw->queue_redraw();
	}
}

void EditorPropertyEasing::_draw_easing() {
	RID ci = easing_draw->get_canvas_item();

	Size2 s = easing_draw->get_size();

	const int point_count = 48;

	const float exp = get_edited_property_value();

	const Ref<Font> f = get_theme_font(SceneStringName(font), SNAME("Label"));
	int font_size = get_theme_font_size(SceneStringName(font_size), SNAME("Label"));
	const Color font_color = get_theme_color(is_read_only() ? SNAME("font_uneditable_color") : SceneStringName(font_color), SNAME("LineEdit"));
	Color line_color;
	if (dragging) {
		line_color = get_theme_color(SNAME("accent_color"), EditorStringName(Editor));
	} else {
		line_color = get_theme_color(is_read_only() ? SNAME("font_uneditable_color") : SceneStringName(font_color), SNAME("LineEdit")) * Color(1, 1, 1, 0.9);
	}

	Vector<Point2> points;
	for (int i = 0; i <= point_count; i++) {
		float ifl = i / float(point_count);

		const float h = 1.0 - Math::ease(ifl, exp);

		if (flip) {
			ifl = 1.0 - ifl;
		}

		points.push_back(Point2(ifl * s.width, h * s.height));
	}

	easing_draw->draw_polyline(points, line_color, 1.0, true);
	// Draw more decimals for small numbers since higher precision is usually required for fine adjustments.
	int decimals;
	if (Math::abs(exp) < 0.1 - CMP_EPSILON) {
		decimals = 4;
	} else if (Math::abs(exp) < 1 - CMP_EPSILON) {
		decimals = 3;
	} else if (Math::abs(exp) < 10 - CMP_EPSILON) {
		decimals = 2;
	} else {
		decimals = 1;
	}
	f->draw_string(ci, Point2(10, 10 + f->get_ascent(font_size)), TS->format_number(rtos(exp).pad_decimals(decimals)), HORIZONTAL_ALIGNMENT_LEFT, -1, font_size, font_color);
}

void EditorPropertyEasing::update_property() {
	easing_draw->queue_redraw();
}

void EditorPropertyEasing::_set_preset(int p_preset) {
	static const float preset_value[EASING_MAX] = { 0.0, 1.0, 2.0, 0.5, -2.0, -0.5 };

	emit_changed(get_edited_property(), preset_value[p_preset]);
	easing_draw->queue_redraw();
}

void EditorPropertyEasing::_setup_spin() {
	spin->setup_and_show();
	spin->get_line_edit()->set_text(TS->format_number(rtos(get_edited_property_value())));
	spin->show();
}

void EditorPropertyEasing::_spin_value_changed(double p_value) {
	// 0 is a singularity, but both positive and negative values
	// are otherwise allowed. Enforce 0+ as workaround.
	if (Math::is_zero_approx(p_value)) {
		p_value = 0.00001;
	}

	// Limit to a reasonable value to prevent the curve going into infinity,
	// which can cause crashes and other issues.
	p_value = CLAMP(p_value, -1'000'000, 1'000'000);

	if (positive_only) {
		// Force a positive or zero value if a negative value was manually entered by double-clicking.
		p_value = MAX(0.0, p_value);
	}

	emit_changed(get_edited_property(), p_value);
	_spin_focus_exited();
}

void EditorPropertyEasing::_spin_focus_exited() {
	spin->hide();
	// Ensure the easing doesn't appear as being dragged
	dragging = false;
	easing_draw->queue_redraw();
}

void EditorPropertyEasing::setup(bool p_positive_only, bool p_flip) {
	flip = p_flip;
	positive_only = p_positive_only;
}

void EditorPropertyEasing::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_THEME_CHANGED:
		case NOTIFICATION_ENTER_TREE: {
			preset->clear();
			preset->add_icon_item(get_editor_theme_icon(SNAME("CurveLinear")), "Linear", EASING_LINEAR);
			preset->add_icon_item(get_editor_theme_icon(SNAME("CurveIn")), "Ease In", EASING_IN);
			preset->add_icon_item(get_editor_theme_icon(SNAME("CurveOut")), "Ease Out", EASING_OUT);
			preset->add_icon_item(get_editor_theme_icon(SNAME("CurveConstant")), "Zero", EASING_ZERO);
			if (!positive_only) {
				preset->add_icon_item(get_editor_theme_icon(SNAME("CurveInOut")), "Ease In-Out", EASING_IN_OUT);
				preset->add_icon_item(get_editor_theme_icon(SNAME("CurveOutIn")), "Ease Out-In", EASING_OUT_IN);
			}
			easing_draw->set_custom_minimum_size(Size2(0, get_theme_font(SceneStringName(font), SNAME("Label"))->get_height(get_theme_font_size(SceneStringName(font_size), SNAME("Label"))) * 2));
		} break;
	}
}

EditorPropertyEasing::EditorPropertyEasing() {
	easing_draw = memnew(Control);
	easing_draw->connect(SceneStringName(draw), callable_mp(this, &EditorPropertyEasing::_draw_easing));
	easing_draw->connect(SceneStringName(gui_input), callable_mp(this, &EditorPropertyEasing::_drag_easing));
	easing_draw->set_default_cursor_shape(Control::CURSOR_MOVE);
	add_child(easing_draw);

	preset = memnew(PopupMenu);
	add_child(preset);
	preset->connect(SceneStringName(id_pressed), callable_mp(this, &EditorPropertyEasing::_set_preset));

	spin = memnew(EditorSpinSlider);
	spin->set_flat(true);
	spin->set_min(-100);
	spin->set_max(100);
	spin->set_step(0);
	spin->set_hide_slider(true);
	spin->set_allow_lesser(true);
	spin->set_allow_greater(true);
	spin->connect(SceneStringName(value_changed), callable_mp(this, &EditorPropertyEasing::_spin_value_changed));
	spin->get_line_edit()->connect(SceneStringName(focus_exited), callable_mp(this, &EditorPropertyEasing::_spin_focus_exited));
	spin->hide();
	add_child(spin);
}

///////////////////// RECT2 /////////////////////////

void EditorPropertyRect2::_set_read_only(bool p_read_only) {
	for (int i = 0; i < 4; i++) {
		spin[i]->set_read_only(p_read_only);
	}
}

void EditorPropertyRect2::_value_changed(double val, const String &p_name) {
	Rect2 r2;
	r2.position.x = spin[0]->get_value();
	r2.position.y = spin[1]->get_value();
	r2.size.x = spin[2]->get_value();
	r2.size.y = spin[3]->get_value();
	emit_changed(get_edited_property(), r2, p_name);
}

void EditorPropertyRect2::update_property() {
	Rect2 val = get_edited_property_value();
	spin[0]->set_value_no_signal(val.position.x);
	spin[1]->set_value_no_signal(val.position.y);
	spin[2]->set_value_no_signal(val.size.x);
	spin[3]->set_value_no_signal(val.size.y);
}

void EditorPropertyRect2::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE:
		case NOTIFICATION_THEME_CHANGED: {
			const Color *colors = _get_property_colors();
			for (int i = 0; i < 4; i++) {
				spin[i]->add_theme_color_override("label_color", colors[i % 2]);
			}
		} break;
	}
}

void EditorPropertyRect2::setup(double p_min, double p_max, double p_step, bool p_hide_slider, const String &p_suffix) {
	for (int i = 0; i < 4; i++) {
		spin[i]->set_min(p_min);
		spin[i]->set_max(p_max);
		spin[i]->set_step(p_step);
		spin[i]->set_hide_slider(p_hide_slider);
		spin[i]->set_allow_greater(true);
		spin[i]->set_allow_lesser(true);
		spin[i]->set_suffix(p_suffix);
	}
}

EditorPropertyRect2::EditorPropertyRect2(bool p_force_wide) {
	bool horizontal = p_force_wide || bool(EDITOR_GET("interface/inspector/horizontal_vector_types_editing"));
	bool grid = false;
	BoxContainer *bc;

	if (p_force_wide) {
		bc = memnew(HBoxContainer);
		add_child(bc);
	} else if (horizontal) {
		bc = memnew(VBoxContainer);
		add_child(bc);
		set_bottom_editor(bc);

		bc->add_child(memnew(HBoxContainer));
		bc->add_child(memnew(HBoxContainer));
		grid = true;
	} else {
		bc = memnew(VBoxContainer);
		add_child(bc);
	}

	static const char *desc[4] = { "x", "y", "w", "h" };
	for (int i = 0; i < 4; i++) {
		spin[i] = memnew(EditorSpinSlider);
		spin[i]->set_label(desc[i]);
		spin[i]->set_flat(true);

		if (grid) {
			bc->get_child(i / 2)->add_child(spin[i]);
		} else {
			bc->add_child(spin[i]);
		}

		add_focusable(spin[i]);
		spin[i]->connect(SceneStringName(value_changed), callable_mp(this, &EditorPropertyRect2::_value_changed).bind(desc[i]));
		if (horizontal) {
			spin[i]->set_h_size_flags(SIZE_EXPAND_FILL);
		}
	}

	if (!horizontal) {
		set_label_reference(spin[0]); //show text and buttons around this
	}
}

///////////////////// RECT2i /////////////////////////

void EditorPropertyRect2i::_set_read_only(bool p_read_only) {
	for (int i = 0; i < 4; i++) {
		spin[i]->set_read_only(p_read_only);
	}
}

void EditorPropertyRect2i::_value_changed(double val, const String &p_name) {
	Rect2i r2;
	r2.position.x = spin[0]->get_value();
	r2.position.y = spin[1]->get_value();
	r2.size.x = spin[2]->get_value();
	r2.size.y = spin[3]->get_value();
	emit_changed(get_edited_property(), r2, p_name);
}

void EditorPropertyRect2i::update_property() {
	Rect2i val = get_edited_property_value();
	spin[0]->set_value_no_signal(val.position.x);
	spin[1]->set_value_no_signal(val.position.y);
	spin[2]->set_value_no_signal(val.size.x);
	spin[3]->set_value_no_signal(val.size.y);
}

void EditorPropertyRect2i::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE:
		case NOTIFICATION_THEME_CHANGED: {
			const Color *colors = _get_property_colors();
			for (int i = 0; i < 4; i++) {
				spin[i]->add_theme_color_override("label_color", colors[i % 2]);
			}
		} break;
	}
}

void EditorPropertyRect2i::setup(int p_min, int p_max, const String &p_suffix) {
	for (int i = 0; i < 4; i++) {
		spin[i]->set_min(p_min);
		spin[i]->set_max(p_max);
		spin[i]->set_step(1);
		spin[i]->set_allow_greater(true);
		spin[i]->set_allow_lesser(true);
		spin[i]->set_suffix(p_suffix);
	}
}

EditorPropertyRect2i::EditorPropertyRect2i(bool p_force_wide) {
	bool horizontal = p_force_wide || bool(EDITOR_GET("interface/inspector/horizontal_vector_types_editing"));
	bool grid = false;
	BoxContainer *bc;

	if (p_force_wide) {
		bc = memnew(HBoxContainer);
		add_child(bc);
	} else if (horizontal) {
		bc = memnew(VBoxContainer);
		add_child(bc);
		set_bottom_editor(bc);

		bc->add_child(memnew(HBoxContainer));
		bc->add_child(memnew(HBoxContainer));
		grid = true;
	} else {
		bc = memnew(VBoxContainer);
		add_child(bc);
	}

	static const char *desc[4] = { "x", "y", "w", "h" };
	for (int i = 0; i < 4; i++) {
		spin[i] = memnew(EditorSpinSlider);
		spin[i]->set_label(desc[i]);
		spin[i]->set_flat(true);

		if (grid) {
			bc->get_child(i / 2)->add_child(spin[i]);
		} else {
			bc->add_child(spin[i]);
		}

		add_focusable(spin[i]);
		spin[i]->connect(SceneStringName(value_changed), callable_mp(this, &EditorPropertyRect2i::_value_changed).bind(desc[i]));
		if (horizontal) {
			spin[i]->set_h_size_flags(SIZE_EXPAND_FILL);
		}
	}

	if (!horizontal) {
		set_label_reference(spin[0]); //show text and buttons around this
	}
}

///////////////////// PLANE /////////////////////////

void EditorPropertyPlane::_set_read_only(bool p_read_only) {
	for (int i = 0; i < 4; i++) {
		spin[i]->set_read_only(p_read_only);
	}
}

void EditorPropertyPlane::_value_changed(double val, const String &p_name) {
	Plane p;
	p.normal.x = spin[0]->get_value();
	p.normal.y = spin[1]->get_value();
	p.normal.z = spin[2]->get_value();
	p.d = spin[3]->get_value();
	emit_changed(get_edited_property(), p, p_name);
}

void EditorPropertyPlane::update_property() {
	Plane val = get_edited_property_value();
	spin[0]->set_value_no_signal(val.normal.x);
	spin[1]->set_value_no_signal(val.normal.y);
	spin[2]->set_value_no_signal(val.normal.z);
	spin[3]->set_value_no_signal(val.d);
}

void EditorPropertyPlane::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE:
		case NOTIFICATION_THEME_CHANGED: {
			const Color *colors = _get_property_colors();
			for (int i = 0; i < 4; i++) {
				spin[i]->add_theme_color_override("label_color", colors[i]);
			}
		} break;
	}
}

void EditorPropertyPlane::setup(double p_min, double p_max, double p_step, bool p_hide_slider, const String &p_suffix) {
	for (int i = 0; i < 4; i++) {
		spin[i]->set_min(p_min);
		spin[i]->set_max(p_max);
		spin[i]->set_step(p_step);
		spin[i]->set_hide_slider(p_hide_slider);
		spin[i]->set_allow_greater(true);
		spin[i]->set_allow_lesser(true);
	}
	spin[3]->set_suffix(p_suffix);
}

EditorPropertyPlane::EditorPropertyPlane(bool p_force_wide) {
	bool horizontal = p_force_wide || bool(EDITOR_GET("interface/inspector/horizontal_vector_types_editing"));

	BoxContainer *bc;

	if (p_force_wide) {
		bc = memnew(HBoxContainer);
		add_child(bc);
	} else if (horizontal) {
		bc = memnew(HBoxContainer);
		add_child(bc);
		set_bottom_editor(bc);
	} else {
		bc = memnew(VBoxContainer);
		add_child(bc);
	}

	static const char *desc[4] = { "x", "y", "z", "d" };
	for (int i = 0; i < 4; i++) {
		spin[i] = memnew(EditorSpinSlider);
		spin[i]->set_flat(true);
		spin[i]->set_label(desc[i]);
		bc->add_child(spin[i]);
		add_focusable(spin[i]);
		spin[i]->connect(SceneStringName(value_changed), callable_mp(this, &EditorPropertyPlane::_value_changed).bind(desc[i]));
		if (horizontal) {
			spin[i]->set_h_size_flags(SIZE_EXPAND_FILL);
		}
	}

	if (!horizontal) {
		set_label_reference(spin[0]); //show text and buttons around this
	}
}

///////////////////// QUATERNION /////////////////////////

void EditorPropertyQuaternion::_set_read_only(bool p_read_only) {
	for (int i = 0; i < 4; i++) {
		spin[i]->set_read_only(p_read_only);
	}
	for (int i = 0; i < 3; i++) {
		euler[i]->set_read_only(p_read_only);
	}
}

void EditorPropertyQuaternion::_edit_custom_value() {
	if (edit_button->is_pressed()) {
		edit_custom_bc->show();
		for (int i = 0; i < 3; i++) {
			euler[i]->grab_focus();
		}
	} else {
		edit_custom_bc->hide();
		for (int i = 0; i < 4; i++) {
			spin[i]->grab_focus();
		}
	}
	update_property();
}

void EditorPropertyQuaternion::_custom_value_changed(double val) {
	edit_euler.x = euler[0]->get_value();
	edit_euler.y = euler[1]->get_value();
	edit_euler.z = euler[2]->get_value();

	Vector3 v;
	v.x = Math::deg_to_rad(edit_euler.x);
	v.y = Math::deg_to_rad(edit_euler.y);
	v.z = Math::deg_to_rad(edit_euler.z);

	Quaternion temp_q = Quaternion::from_euler(v);
	spin[0]->set_value_no_signal(temp_q.x);
	spin[1]->set_value_no_signal(temp_q.y);
	spin[2]->set_value_no_signal(temp_q.z);
	spin[3]->set_value_no_signal(temp_q.w);
	_value_changed(-1, "");
}

void EditorPropertyQuaternion::_value_changed(double val, const String &p_name) {
	Quaternion p;
	p.x = spin[0]->get_value();
	p.y = spin[1]->get_value();
	p.z = spin[2]->get_value();
	p.w = spin[3]->get_value();

	emit_changed(get_edited_property(), p, p_name);
}

bool EditorPropertyQuaternion::is_grabbing_euler() {
	bool is_grabbing = false;
	for (int i = 0; i < 3; i++) {
		is_grabbing |= euler[i]->is_grabbing();
	}
	return is_grabbing;
}

void EditorPropertyQuaternion::update_property() {
	Quaternion val = get_edited_property_value();
	spin[0]->set_value_no_signal(val.x);
	spin[1]->set_value_no_signal(val.y);
	spin[2]->set_value_no_signal(val.z);
	spin[3]->set_value_no_signal(val.w);
	if (!is_grabbing_euler()) {
		Vector3 v = val.normalized().get_euler();
		edit_euler.x = Math::rad_to_deg(v.x);
		edit_euler.y = Math::rad_to_deg(v.y);
		edit_euler.z = Math::rad_to_deg(v.z);
		euler[0]->set_value_no_signal(edit_euler.x);
		euler[1]->set_value_no_signal(edit_euler.y);
		euler[2]->set_value_no_signal(edit_euler.z);
	}
}

void EditorPropertyQuaternion::_warning_pressed() {
	warning_dialog->popup_centered();
}

void EditorPropertyQuaternion::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE:
		case NOTIFICATION_THEME_CHANGED: {
			const Color *colors = _get_property_colors();
			for (int i = 0; i < 4; i++) {
				spin[i]->add_theme_color_override("label_color", colors[i]);
			}
			for (int i = 0; i < 3; i++) {
				euler[i]->add_theme_color_override("label_color", colors[i]);
			}
			edit_button->set_icon(get_editor_theme_icon(SNAME("Edit")));
			euler_label->add_theme_color_override(SceneStringName(font_color), get_theme_color(SNAME("property_color"), SNAME("EditorProperty")));
			warning->set_icon(get_editor_theme_icon(SNAME("NodeWarning")));
			warning->add_theme_color_override(SceneStringName(font_color), get_theme_color(SNAME("warning_color"), EditorStringName(Editor)));
		} break;
	}
}

void EditorPropertyQuaternion::setup(double p_min, double p_max, double p_step, bool p_hide_slider, const String &p_suffix, bool p_hide_editor) {
	for (int i = 0; i < 4; i++) {
		spin[i]->set_min(p_min);
		spin[i]->set_max(p_max);
		spin[i]->set_step(p_step);
		spin[i]->set_hide_slider(p_hide_slider);
		spin[i]->set_allow_greater(true);
		spin[i]->set_allow_lesser(true);
		// Quaternion is inherently unitless, however someone may want to use it as
		// a generic way to store 4 values, so we'll still respect the suffix.
		spin[i]->set_suffix(p_suffix);
	}

	for (int i = 0; i < 3; i++) {
		euler[i]->set_min(-360);
		euler[i]->set_max(360);
		euler[i]->set_step(0.1);
		euler[i]->set_hide_slider(false);
		euler[i]->set_allow_greater(true);
		euler[i]->set_allow_lesser(true);
		euler[i]->set_suffix(U"\u00B0");
	}

	if (p_hide_editor) {
		edit_button->hide();
	}
}

EditorPropertyQuaternion::EditorPropertyQuaternion() {
	bool horizontal = EDITOR_GET("interface/inspector/horizontal_vector_types_editing");

	VBoxContainer *bc = memnew(VBoxContainer);
	edit_custom_bc = memnew(VBoxContainer);
	BoxContainer *edit_custom_layout;
	if (horizontal) {
		default_layout = memnew(HBoxContainer);
		edit_custom_layout = memnew(HBoxContainer);
		set_bottom_editor(bc);
	} else {
		default_layout = memnew(VBoxContainer);
		edit_custom_layout = memnew(VBoxContainer);
	}
	edit_custom_bc->hide();
	add_child(bc);
	edit_custom_bc->set_h_size_flags(SIZE_EXPAND_FILL);
	default_layout->set_h_size_flags(SIZE_EXPAND_FILL);
	edit_custom_layout->set_h_size_flags(SIZE_EXPAND_FILL);
	bc->add_child(default_layout);
	bc->add_child(edit_custom_bc);

	static const char *desc[4] = { "x", "y", "z", "w" };
	for (int i = 0; i < 4; i++) {
		spin[i] = memnew(EditorSpinSlider);
		spin[i]->set_flat(true);
		spin[i]->set_label(desc[i]);
		default_layout->add_child(spin[i]);
		add_focusable(spin[i]);
		spin[i]->connect(SceneStringName(value_changed), callable_mp(this, &EditorPropertyQuaternion::_value_changed).bind(desc[i]));
		if (horizontal) {
			spin[i]->set_h_size_flags(SIZE_EXPAND_FILL);
		}
	}

	warning = memnew(Button);
	warning->set_text(TTR("Temporary Euler may be changed implicitly!"));
	warning->set_clip_text(true);
	warning->connect(SceneStringName(pressed), callable_mp(this, &EditorPropertyQuaternion::_warning_pressed));
	warning_dialog = memnew(AcceptDialog);
	add_child(warning_dialog);
	warning_dialog->set_text(TTR("Temporary Euler will not be stored in the object with the original value. Instead, it will be stored as Quaternion with irreversible conversion.\nThis is due to the fact that the result of Euler->Quaternion can be determined uniquely, but the result of Quaternion->Euler can be multi-existent."));

	euler_label = memnew(Label);
	euler_label->set_text(TTR("Temporary Euler"));

	edit_custom_bc->add_child(warning);
	edit_custom_bc->add_child(edit_custom_layout);
	edit_custom_layout->add_child(euler_label);

	for (int i = 0; i < 3; i++) {
		euler[i] = memnew(EditorSpinSlider);
		euler[i]->set_flat(true);
		euler[i]->set_label(desc[i]);
		edit_custom_layout->add_child(euler[i]);
		add_focusable(euler[i]);
		euler[i]->connect(SceneStringName(value_changed), callable_mp(this, &EditorPropertyQuaternion::_custom_value_changed));
		if (horizontal) {
			euler[i]->set_h_size_flags(SIZE_EXPAND_FILL);
		}
	}

	edit_button = memnew(Button);
	edit_button->set_flat(true);
	edit_button->set_toggle_mode(true);
	default_layout->add_child(edit_button);
	edit_button->connect(SceneStringName(pressed), callable_mp(this, &EditorPropertyQuaternion::_edit_custom_value));

	add_focusable(edit_button);

	if (!horizontal) {
		set_label_reference(spin[0]); //show text and buttons around this
	}
}

///////////////////// AABB /////////////////////////

void EditorPropertyAABB::_set_read_only(bool p_read_only) {
	for (int i = 0; i < 6; i++) {
		spin[i]->set_read_only(p_read_only);
	}
}

void EditorPropertyAABB::_value_changed(double val, const String &p_name) {
	AABB p;
	p.position.x = spin[0]->get_value();
	p.position.y = spin[1]->get_value();
	p.position.z = spin[2]->get_value();
	p.size.x = spin[3]->get_value();
	p.size.y = spin[4]->get_value();
	p.size.z = spin[5]->get_value();
	emit_changed(get_edited_property(), p, p_name);
}

void EditorPropertyAABB::update_property() {
	AABB val = get_edited_property_value();
	spin[0]->set_value_no_signal(val.position.x);
	spin[1]->set_value_no_signal(val.position.y);
	spin[2]->set_value_no_signal(val.position.z);
	spin[3]->set_value_no_signal(val.size.x);
	spin[4]->set_value_no_signal(val.size.y);
	spin[5]->set_value_no_signal(val.size.z);
}

void EditorPropertyAABB::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE:
		case NOTIFICATION_THEME_CHANGED: {
			const Color *colors = _get_property_colors();
			for (int i = 0; i < 6; i++) {
				spin[i]->add_theme_color_override("label_color", colors[i % 3]);
			}
		} break;
	}
}

void EditorPropertyAABB::setup(double p_min, double p_max, double p_step, bool p_hide_slider, const String &p_suffix) {
	for (int i = 0; i < 6; i++) {
		spin[i]->set_min(p_min);
		spin[i]->set_max(p_max);
		spin[i]->set_step(p_step);
		spin[i]->set_hide_slider(p_hide_slider);
		spin[i]->set_allow_greater(true);
		spin[i]->set_allow_lesser(true);
		spin[i]->set_suffix(p_suffix);
	}
}

EditorPropertyAABB::EditorPropertyAABB() {
	GridContainer *g = memnew(GridContainer);
	g->set_columns(3);
	add_child(g);

	static const char *desc[6] = { "x", "y", "z", "w", "h", "d" };
	for (int i = 0; i < 6; i++) {
		spin[i] = memnew(EditorSpinSlider);
		spin[i]->set_label(desc[i]);
		spin[i]->set_flat(true);

		g->add_child(spin[i]);
		spin[i]->set_h_size_flags(SIZE_EXPAND_FILL);
		add_focusable(spin[i]);
		spin[i]->connect(SceneStringName(value_changed), callable_mp(this, &EditorPropertyAABB::_value_changed).bind(desc[i]));
	}
	set_bottom_editor(g);
}

///////////////////// TRANSFORM2D /////////////////////////

void EditorPropertyTransform2D::_set_read_only(bool p_read_only) {
	for (int i = 0; i < 6; i++) {
		spin[i]->set_read_only(p_read_only);
	}
}

void EditorPropertyTransform2D::_value_changed(double val, const String &p_name) {
	Transform2D p;
	p[0][0] = spin[0]->get_value();
	p[1][0] = spin[1]->get_value();
	p[2][0] = spin[2]->get_value();
	p[0][1] = spin[3]->get_value();
	p[1][1] = spin[4]->get_value();
	p[2][1] = spin[5]->get_value();

	emit_changed(get_edited_property(), p, p_name);
}

void EditorPropertyTransform2D::update_property() {
	Transform2D val = get_edited_property_value();
	spin[0]->set_value_no_signal(val[0][0]);
	spin[1]->set_value_no_signal(val[1][0]);
	spin[2]->set_value_no_signal(val[2][0]);
	spin[3]->set_value_no_signal(val[0][1]);
	spin[4]->set_value_no_signal(val[1][1]);
	spin[5]->set_value_no_signal(val[2][1]);
}

void EditorPropertyTransform2D::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE:
		case NOTIFICATION_THEME_CHANGED: {
			const Color *colors = _get_property_colors();
			for (int i = 0; i < 6; i++) {
				// For Transform2D, use the 4th color (cyan) for the origin vector.
				if (i % 3 == 2) {
					spin[i]->add_theme_color_override("label_color", colors[3]);
				} else {
					spin[i]->add_theme_color_override("label_color", colors[i % 3]);
				}
			}
		} break;
	}
}

void EditorPropertyTransform2D::setup(double p_min, double p_max, double p_step, bool p_hide_slider, const String &p_suffix) {
	for (int i = 0; i < 6; i++) {
		spin[i]->set_min(p_min);
		spin[i]->set_max(p_max);
		spin[i]->set_step(p_step);
		spin[i]->set_hide_slider(p_hide_slider);
		spin[i]->set_allow_greater(true);
		spin[i]->set_allow_lesser(true);
		if (i % 3 == 2) {
			spin[i]->set_suffix(p_suffix);
		}
	}
}

EditorPropertyTransform2D::EditorPropertyTransform2D(bool p_include_origin) {
	GridContainer *g = memnew(GridContainer);
	g->set_columns(p_include_origin ? 3 : 2);
	add_child(g);

	static const char *desc[6] = { "xx", "xy", "xo", "yx", "yy", "yo" };
	for (int i = 0; i < 6; i++) {
		spin[i] = memnew(EditorSpinSlider);
		spin[i]->set_label(desc[i]);
		spin[i]->set_flat(true);
		if (p_include_origin || i % 3 != 2) {
			g->add_child(spin[i]);
		}
		spin[i]->set_h_size_flags(SIZE_EXPAND_FILL);
		add_focusable(spin[i]);
		spin[i]->connect(SceneStringName(value_changed), callable_mp(this, &EditorPropertyTransform2D::_value_changed).bind(desc[i]));
	}
	set_bottom_editor(g);
}

///////////////////// BASIS /////////////////////////

void EditorPropertyBasis::_set_read_only(bool p_read_only) {
	for (int i = 0; i < 9; i++) {
		spin[i]->set_read_only(p_read_only);
	}
}

void EditorPropertyBasis::_value_changed(double val, const String &p_name) {
	Basis p;
	p[0][0] = spin[0]->get_value();
	p[0][1] = spin[1]->get_value();
	p[0][2] = spin[2]->get_value();
	p[1][0] = spin[3]->get_value();
	p[1][1] = spin[4]->get_value();
	p[1][2] = spin[5]->get_value();
	p[2][0] = spin[6]->get_value();
	p[2][1] = spin[7]->get_value();
	p[2][2] = spin[8]->get_value();

	emit_changed(get_edited_property(), p, p_name);
}

void EditorPropertyBasis::update_property() {
	Basis val = get_edited_property_value();
	spin[0]->set_value_no_signal(val[0][0]);
	spin[1]->set_value_no_signal(val[0][1]);
	spin[2]->set_value_no_signal(val[0][2]);
	spin[3]->set_value_no_signal(val[1][0]);
	spin[4]->set_value_no_signal(val[1][1]);
	spin[5]->set_value_no_signal(val[1][2]);
	spin[6]->set_value_no_signal(val[2][0]);
	spin[7]->set_value_no_signal(val[2][1]);
	spin[8]->set_value_no_signal(val[2][2]);
}

void EditorPropertyBasis::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE:
		case NOTIFICATION_THEME_CHANGED: {
			const Color *colors = _get_property_colors();
			for (int i = 0; i < 9; i++) {
				spin[i]->add_theme_color_override("label_color", colors[i % 3]);
			}
		} break;
	}
}

void EditorPropertyBasis::setup(double p_min, double p_max, double p_step, bool p_hide_slider, const String &p_suffix) {
	for (int i = 0; i < 9; i++) {
		spin[i]->set_min(p_min);
		spin[i]->set_max(p_max);
		spin[i]->set_step(p_step);
		spin[i]->set_hide_slider(p_hide_slider);
		spin[i]->set_allow_greater(true);
		spin[i]->set_allow_lesser(true);
		// Basis is inherently unitless, however someone may want to use it as
		// a generic way to store 9 values, so we'll still respect the suffix.
		spin[i]->set_suffix(p_suffix);
	}
}

EditorPropertyBasis::EditorPropertyBasis() {
	GridContainer *g = memnew(GridContainer);
	g->set_columns(3);
	add_child(g);

	static const char *desc[9] = { "xx", "xy", "xz", "yx", "yy", "yz", "zx", "zy", "zz" };
	for (int i = 0; i < 9; i++) {
		spin[i] = memnew(EditorSpinSlider);
		spin[i]->set_label(desc[i]);
		spin[i]->set_flat(true);
		g->add_child(spin[i]);
		spin[i]->set_h_size_flags(SIZE_EXPAND_FILL);
		add_focusable(spin[i]);
		spin[i]->connect(SceneStringName(value_changed), callable_mp(this, &EditorPropertyBasis::_value_changed).bind(desc[i]));
	}
	set_bottom_editor(g);
}

///////////////////// TRANSFORM3D /////////////////////////

void EditorPropertyTransform3D::_set_read_only(bool p_read_only) {
	for (int i = 0; i < 12; i++) {
		spin[i]->set_read_only(p_read_only);
	}
}

void EditorPropertyTransform3D::_value_changed(double val, const String &p_name) {
	Transform3D p;
	p.basis[0][0] = spin[0]->get_value();
	p.basis[0][1] = spin[1]->get_value();
	p.basis[0][2] = spin[2]->get_value();
	p.origin[0] = spin[3]->get_value();
	p.basis[1][0] = spin[4]->get_value();
	p.basis[1][1] = spin[5]->get_value();
	p.basis[1][2] = spin[6]->get_value();
	p.origin[1] = spin[7]->get_value();
	p.basis[2][0] = spin[8]->get_value();
	p.basis[2][1] = spin[9]->get_value();
	p.basis[2][2] = spin[10]->get_value();
	p.origin[2] = spin[11]->get_value();

	emit_changed(get_edited_property(), p, p_name);
}

void EditorPropertyTransform3D::update_property() {
	update_using_transform(get_edited_property_value());
}

void EditorPropertyTransform3D::update_using_transform(Transform3D p_transform) {
	spin[0]->set_value_no_signal(p_transform.basis[0][0]);
	spin[1]->set_value_no_signal(p_transform.basis[0][1]);
	spin[2]->set_value_no_signal(p_transform.basis[0][2]);
	spin[3]->set_value_no_signal(p_transform.origin[0]);
	spin[4]->set_value_no_signal(p_transform.basis[1][0]);
	spin[5]->set_value_no_signal(p_transform.basis[1][1]);
	spin[6]->set_value_no_signal(p_transform.basis[1][2]);
	spin[7]->set_value_no_signal(p_transform.origin[1]);
	spin[8]->set_value_no_signal(p_transform.basis[2][0]);
	spin[9]->set_value_no_signal(p_transform.basis[2][1]);
	spin[10]->set_value_no_signal(p_transform.basis[2][2]);
	spin[11]->set_value_no_signal(p_transform.origin[2]);
}

void EditorPropertyTransform3D::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE:
		case NOTIFICATION_THEME_CHANGED: {
			const Color *colors = _get_property_colors();
			for (int i = 0; i < 12; i++) {
				spin[i]->add_theme_color_override("label_color", colors[i % 4]);
			}
		} break;
	}
}

void EditorPropertyTransform3D::setup(double p_min, double p_max, double p_step, bool p_hide_slider, const String &p_suffix) {
	for (int i = 0; i < 12; i++) {
		spin[i]->set_min(p_min);
		spin[i]->set_max(p_max);
		spin[i]->set_step(p_step);
		spin[i]->set_hide_slider(p_hide_slider);
		spin[i]->set_allow_greater(true);
		spin[i]->set_allow_lesser(true);
		if (i % 4 == 3) {
			spin[i]->set_suffix(p_suffix);
		}
	}
}

EditorPropertyTransform3D::EditorPropertyTransform3D() {
	GridContainer *g = memnew(GridContainer);
	g->set_columns(4);
	add_child(g);

	static const char *desc[12] = { "xx", "xy", "xz", "xo", "yx", "yy", "yz", "yo", "zx", "zy", "zz", "zo" };
	for (int i = 0; i < 12; i++) {
		spin[i] = memnew(EditorSpinSlider);
		spin[i]->set_label(desc[i]);
		spin[i]->set_flat(true);
		g->add_child(spin[i]);
		spin[i]->set_h_size_flags(SIZE_EXPAND_FILL);
		add_focusable(spin[i]);
		spin[i]->connect(SceneStringName(value_changed), callable_mp(this, &EditorPropertyTransform3D::_value_changed).bind(desc[i]));
	}
	set_bottom_editor(g);
}

///////////////////// PROJECTION /////////////////////////

void EditorPropertyProjection::_set_read_only(bool p_read_only) {
	for (int i = 0; i < 12; i++) {
		spin[i]->set_read_only(p_read_only);
	}
}

void EditorPropertyProjection::_value_changed(double val, const String &p_name) {
	Projection p;
	p.columns[0][0] = spin[0]->get_value();
	p.columns[0][1] = spin[1]->get_value();
	p.columns[0][2] = spin[2]->get_value();
	p.columns[0][3] = spin[3]->get_value();
	p.columns[1][0] = spin[4]->get_value();
	p.columns[1][1] = spin[5]->get_value();
	p.columns[1][2] = spin[6]->get_value();
	p.columns[1][3] = spin[7]->get_value();
	p.columns[2][0] = spin[8]->get_value();
	p.columns[2][1] = spin[9]->get_value();
	p.columns[2][2] = spin[10]->get_value();
	p.columns[2][3] = spin[11]->get_value();
	p.columns[3][0] = spin[12]->get_value();
	p.columns[3][1] = spin[13]->get_value();
	p.columns[3][2] = spin[14]->get_value();
	p.columns[3][3] = spin[15]->get_value();

	emit_changed(get_edited_property(), p, p_name);
}

void EditorPropertyProjection::update_property() {
	update_using_transform(get_edited_property_value());
}

void EditorPropertyProjection::update_using_transform(Projection p_transform) {
	spin[0]->set_value_no_signal(p_transform.columns[0][0]);
	spin[1]->set_value_no_signal(p_transform.columns[0][1]);
	spin[2]->set_value_no_signal(p_transform.columns[0][2]);
	spin[3]->set_value_no_signal(p_transform.columns[0][3]);
	spin[4]->set_value_no_signal(p_transform.columns[1][0]);
	spin[5]->set_value_no_signal(p_transform.columns[1][1]);
	spin[6]->set_value_no_signal(p_transform.columns[1][2]);
	spin[7]->set_value_no_signal(p_transform.columns[1][3]);
	spin[8]->set_value_no_signal(p_transform.columns[2][0]);
	spin[9]->set_value_no_signal(p_transform.columns[2][1]);
	spin[10]->set_value_no_signal(p_transform.columns[2][2]);
	spin[11]->set_value_no_signal(p_transform.columns[2][3]);
	spin[12]->set_value_no_signal(p_transform.columns[3][0]);
	spin[13]->set_value_no_signal(p_transform.columns[3][1]);
	spin[14]->set_value_no_signal(p_transform.columns[3][2]);
	spin[15]->set_value_no_signal(p_transform.columns[3][3]);
}

void EditorPropertyProjection::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE:
		case NOTIFICATION_THEME_CHANGED: {
			const Color *colors = _get_property_colors();
			for (int i = 0; i < 16; i++) {
				spin[i]->add_theme_color_override("label_color", colors[i % 4]);
			}
		} break;
	}
}

void EditorPropertyProjection::setup(double p_min, double p_max, double p_step, bool p_hide_slider, const String &p_suffix) {
	for (int i = 0; i < 16; i++) {
		spin[i]->set_min(p_min);
		spin[i]->set_max(p_max);
		spin[i]->set_step(p_step);
		spin[i]->set_hide_slider(p_hide_slider);
		spin[i]->set_allow_greater(true);
		spin[i]->set_allow_lesser(true);
		if (i % 4 == 3) {
			spin[i]->set_suffix(p_suffix);
		}
	}
}

EditorPropertyProjection::EditorPropertyProjection() {
	GridContainer *g = memnew(GridContainer);
	g->set_columns(4);
	add_child(g);

	static const char *desc[16] = { "xx", "xy", "xz", "xw", "yx", "yy", "yz", "yw", "zx", "zy", "zz", "zw", "wx", "wy", "wz", "ww" };
	for (int i = 0; i < 16; i++) {
		spin[i] = memnew(EditorSpinSlider);
		spin[i]->set_label(desc[i]);
		spin[i]->set_flat(true);
		g->add_child(spin[i]);
		spin[i]->set_h_size_flags(SIZE_EXPAND_FILL);
		add_focusable(spin[i]);
		spin[i]->connect(SceneStringName(value_changed), callable_mp(this, &EditorPropertyProjection::_value_changed).bind(desc[i]));
	}
	set_bottom_editor(g);
}
////////////// COLOR PICKER //////////////////////

void EditorPropertyColor::_set_read_only(bool p_read_only) {
	picker->set_disabled(p_read_only);
}

void EditorPropertyColor::_color_changed(const Color &p_color) {
	if (!live_changes_enabled) {
		return;
	}

	// Cancel the color change if the current color is identical to the new one.
	if (((Color)get_edited_property_value()).is_equal_approx(p_color)) {
		return;
	}

	// Preview color change, bypassing undo/redo.
	get_edited_object()->set(get_edited_property(), p_color);
}

void EditorPropertyColor::_popup_closed() {
	get_edited_object()->set(get_edited_property(), was_checked ? Variant(last_color) : Variant());
	if (!picker->get_pick_color().is_equal_approx(last_color)) {
		emit_changed(get_edited_property(), picker->get_pick_color(), "", false);
	}
}

void EditorPropertyColor::_picker_opening() {
	last_color = picker->get_pick_color();
	was_checked = !is_checkable() || is_checked();
}

void EditorPropertyColor::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE:
		case NOTIFICATION_THEME_CHANGED: {
			picker->set_custom_minimum_size(Size2(0, get_theme_constant(SNAME("color_picker_button_height"), EditorStringName(Editor))));
		} break;
	}
}

void EditorPropertyColor::update_property() {
	picker->set_pick_color(get_edited_property_value());
	const Color color = picker->get_pick_color();

	// Add a tooltip to display each channel's values without having to click the ColorPickerButton
	if (picker->is_editing_alpha()) {
		picker->set_tooltip_text(vformat(
				"R: %s\nG: %s\nB: %s\nA: %s",
				rtos(color.r).pad_decimals(2),
				rtos(color.g).pad_decimals(2),
				rtos(color.b).pad_decimals(2),
				rtos(color.a).pad_decimals(2)));
	} else {
		picker->set_tooltip_text(vformat(
				"R: %s\nG: %s\nB: %s",
				rtos(color.r).pad_decimals(2),
				rtos(color.g).pad_decimals(2),
				rtos(color.b).pad_decimals(2)));
	}
}

void EditorPropertyColor::setup(bool p_show_alpha) {
	picker->set_edit_alpha(p_show_alpha);
}

void EditorPropertyColor::set_live_changes_enabled(bool p_enabled) {
	live_changes_enabled = p_enabled;
}

EditorPropertyColor::EditorPropertyColor() {
	picker = memnew(ColorPickerButton);
	add_child(picker);
	picker->set_flat(true);
	picker->connect("color_changed", callable_mp(this, &EditorPropertyColor::_color_changed));
	picker->connect("popup_closed", callable_mp(this, &EditorPropertyColor::_popup_closed));
	picker->get_popup()->connect("about_to_popup", callable_mp(EditorNode::get_singleton(), &EditorNode::setup_color_picker).bind(picker->get_picker()));
	picker->get_popup()->connect("about_to_popup", callable_mp(this, &EditorPropertyColor::_picker_opening));
}

////////////// NODE PATH //////////////////////

void EditorPropertyNodePath::_set_read_only(bool p_read_only) {
	assign->set_disabled(p_read_only);
	menu->set_disabled(p_read_only);
};

Variant EditorPropertyNodePath::_get_cache_value(const StringName &p_prop, bool &r_valid) const {
	if (p_prop == get_edited_property()) {
		r_valid = true;
		return const_cast<EditorPropertyNodePath *>(this)->get_edited_object()->get(get_edited_property(), &r_valid);
	}
	return Variant();
}

void EditorPropertyNodePath::_node_selected(const NodePath &p_path, bool p_absolute) {
	NodePath path = p_path;
	Node *base_node = get_base_node();

	if (!base_node && Object::cast_to<RefCounted>(get_edited_object())) {
		Node *to_node = get_node(p_path);
		ERR_FAIL_NULL(to_node);
		path = get_tree()->get_edited_scene_root()->get_path_to(to_node);
	}

	if (p_absolute && base_node) { // for AnimationTrackKeyEdit
		path = base_node->get_path().rel_path_to(p_path);
	}

	if (editing_node) {
		if (!base_node) {
			emit_changed(get_edited_property(), get_tree()->get_edited_scene_root()->get_node(path));
		} else {
			emit_changed(get_edited_property(), base_node->get_node(path));
		}
	} else {
		emit_changed(get_edited_property(), path);
	}
	update_property();
}

void EditorPropertyNodePath::_node_assign() {
	if (!scene_tree) {
		scene_tree = memnew(SceneTreeDialog);
		scene_tree->get_scene_tree()->set_show_enabled_subscene(true);
		scene_tree->set_valid_types(valid_types);
		add_child(scene_tree);
		scene_tree->connect("selected", callable_mp(this, &EditorPropertyNodePath::_node_selected).bind(true));
	}

	Variant val = get_edited_property_value();
	Node *n = nullptr;
	if (val.get_type() == Variant::Type::NODE_PATH) {
		Node *base_node = get_base_node();
		n = base_node == nullptr ? nullptr : base_node->get_node_or_null(val);
	} else {
		n = Object::cast_to<Node>(val);
	}
	scene_tree->popup_scenetree_dialog(n, get_base_node());
}

void EditorPropertyNodePath::_update_menu() {
	const NodePath &np = _get_node_path();

	menu->get_popup()->set_item_disabled(ACTION_CLEAR, np.is_empty());
	menu->get_popup()->set_item_disabled(ACTION_COPY, np.is_empty());

	Node *edited_node = Object::cast_to<Node>(get_edited_object());
	menu->get_popup()->set_item_disabled(ACTION_SELECT, !edited_node || !edited_node->has_node(np));
}

void EditorPropertyNodePath::_menu_option(int p_idx) {
	switch (p_idx) {
		case ACTION_CLEAR: {
			if (editing_node) {
				emit_changed(get_edited_property(), Variant());
			} else {
				emit_changed(get_edited_property(), NodePath());
			}
			update_property();
		} break;

		case ACTION_COPY: {
			DisplayServer::get_singleton()->clipboard_set(_get_node_path());
		} break;

		case ACTION_EDIT: {
			assign->hide();
			menu->hide();

			const NodePath &np = _get_node_path();
			edit->set_text(np);
			edit->show();
			callable_mp((Control *)edit, &Control::grab_focus).call_deferred();
		} break;

		case ACTION_SELECT: {
			const Node *edited_node = get_base_node();
			ERR_FAIL_NULL(edited_node);

			const NodePath &np = _get_node_path();
			Node *target_node = edited_node->get_node_or_null(np);
			ERR_FAIL_NULL(target_node);

			SceneTreeDock::get_singleton()->set_selected(target_node);
		} break;
	}
}

void EditorPropertyNodePath::_accept_text() {
	_text_submitted(edit->get_text());
}

void EditorPropertyNodePath::_text_submitted(const String &p_text) {
	NodePath np = p_text;
	_node_selected(np, false);
	edit->hide();
	assign->show();
	menu->show();
}

const NodePath EditorPropertyNodePath::_get_node_path() const {
	const Node *base_node = const_cast<EditorPropertyNodePath *>(this)->get_base_node();

	Variant val = get_edited_property_value();
	Node *n = Object::cast_to<Node>(val);
	if (n) {
		if (!n->is_inside_tree()) {
			return NodePath();
		}
		if (base_node) {
			return base_node->get_path_to(n);
		} else {
			return get_tree()->get_edited_scene_root()->get_path_to(n);
		}
	} else {
		return val;
	}
}

bool EditorPropertyNodePath::can_drop_data_fw(const Point2 &p_point, const Variant &p_data, Control *p_from) const {
	return !is_read_only() && is_drop_valid(p_data);
}

void EditorPropertyNodePath::drop_data_fw(const Point2 &p_point, const Variant &p_data, Control *p_from) {
	ERR_FAIL_COND(!is_drop_valid(p_data));
	Dictionary data_dict = p_data;
	Array nodes = data_dict["nodes"];
	Node *node = get_tree()->get_edited_scene_root()->get_node(nodes[0]);

	if (node) {
		_node_selected(node->get_path());
	}
}

bool EditorPropertyNodePath::is_drop_valid(const Dictionary &p_drag_data) const {
	if (p_drag_data["type"] != "nodes") {
		return false;
	}
	Array nodes = p_drag_data["nodes"];
	if (nodes.size() != 1) {
		return false;
	}

	Node *dropped_node = get_tree()->get_edited_scene_root()->get_node(nodes[0]);
	ERR_FAIL_NULL_V(dropped_node, false);

	if (valid_types.is_empty()) {
		// No type requirements specified so any type is valid.
		return true;
	}

	for (const StringName &E : valid_types) {
		if (dropped_node->is_class(E) ||
				EditorNode::get_singleton()->is_object_of_custom_type(dropped_node, E)) {
			return true;
		} else {
			Ref<Script> dropped_node_script = dropped_node->get_script();
			while (dropped_node_script.is_valid()) {
				if (dropped_node_script->get_path() == E) {
					return true;
				}
				dropped_node_script = dropped_node_script->get_base_script();
			}
		}
	}

	return false;
}

void EditorPropertyNodePath::update_property() {
	const Node *base_node = get_base_node();
	const NodePath &p = _get_node_path();
	assign->set_tooltip_text(p);

	if (p.is_empty()) {
		assign->set_icon(Ref<Texture2D>());
		assign->set_text(TTR("Assign..."));
		assign->set_flat(false);
		return;
	}
	assign->set_flat(true);

	if (!base_node || !base_node->has_node(p)) {
		assign->set_icon(Ref<Texture2D>());
		assign->set_text(p);
		return;
	}

	const Node *target_node = base_node->get_node(p);
	ERR_FAIL_NULL(target_node);

	if (String(target_node->get_name()).contains("@")) {
		assign->set_icon(Ref<Texture2D>());
		assign->set_text(p);
		return;
	}

	assign->set_text(target_node->get_name());
	assign->set_icon(EditorNode::get_singleton()->get_object_icon(target_node, "Node"));
}

void EditorPropertyNodePath::setup(const Vector<StringName> &p_valid_types, bool p_use_path_from_scene_root, bool p_editing_node) {
	valid_types = p_valid_types;
	editing_node = p_editing_node;
	use_path_from_scene_root = p_use_path_from_scene_root;
}

void EditorPropertyNodePath::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE:
		case NOTIFICATION_THEME_CHANGED: {
			menu->set_icon(get_editor_theme_icon(SNAME("GuiTabMenuHl")));
			menu->get_popup()->set_item_icon(ACTION_CLEAR, get_editor_theme_icon(SNAME("Clear")));
			menu->get_popup()->set_item_icon(ACTION_COPY, get_editor_theme_icon(SNAME("ActionCopy")));
			menu->get_popup()->set_item_icon(ACTION_EDIT, get_editor_theme_icon(SNAME("Edit")));
			menu->get_popup()->set_item_icon(ACTION_SELECT, get_editor_theme_icon(SNAME("ExternalLink")));
		} break;
	}
}

Node *EditorPropertyNodePath::get_base_node() {
	Node *base_node = Object::cast_to<Node>(get_edited_object());

	if (!base_node) {
		base_node = Object::cast_to<Node>(InspectorDock::get_inspector_singleton()->get_edited_object());
	}
	if (!base_node) {
		// Try a base node within history.
		if (EditorNode::get_singleton()->get_editor_selection_history()->get_path_size() > 0) {
			Object *base = ObjectDB::get_instance(EditorNode::get_singleton()->get_editor_selection_history()->get_path_object(0));
			if (base) {
				base_node = Object::cast_to<Node>(base);
			}
		}
	}
	if (use_path_from_scene_root) {
		if (get_edited_object()->has_method("get_root_path")) {
			base_node = Object::cast_to<Node>(get_edited_object()->call("get_root_path"));
		} else {
			base_node = get_tree()->get_edited_scene_root();
		}
	}

	return base_node;
}

EditorPropertyNodePath::EditorPropertyNodePath() {
	HBoxContainer *hbc = memnew(HBoxContainer);
	hbc->add_theme_constant_override("separation", 0);
	add_child(hbc);
	assign = memnew(Button);
	assign->set_flat(true);
	assign->set_h_size_flags(SIZE_EXPAND_FILL);
	assign->set_clip_text(true);
	assign->set_auto_translate_mode(AUTO_TRANSLATE_MODE_DISABLED);
	assign->set_expand_icon(true);
	assign->connect(SceneStringName(pressed), callable_mp(this, &EditorPropertyNodePath::_node_assign));
	SET_DRAG_FORWARDING_CD(assign, EditorPropertyNodePath);
	hbc->add_child(assign);

	menu = memnew(MenuButton);
	menu->set_flat(true);
	menu->connect(SNAME("about_to_popup"), callable_mp(this, &EditorPropertyNodePath::_update_menu));
	hbc->add_child(menu);

	menu->get_popup()->add_item(TTR("Clear"), ACTION_CLEAR);
	menu->get_popup()->add_item(TTR("Copy as Text"), ACTION_COPY);
	menu->get_popup()->add_item(TTR("Edit"), ACTION_EDIT);
	menu->get_popup()->add_item(TTR("Show Node in Tree"), ACTION_SELECT);
	menu->get_popup()->connect(SceneStringName(id_pressed), callable_mp(this, &EditorPropertyNodePath::_menu_option));

	edit = memnew(LineEdit);
	edit->set_h_size_flags(SIZE_EXPAND_FILL);
	edit->hide();
	edit->connect(SceneStringName(focus_exited), callable_mp(this, &EditorPropertyNodePath::_accept_text));
	edit->connect(SNAME("text_submitted"), callable_mp(this, &EditorPropertyNodePath::_text_submitted));
	hbc->add_child(edit);
}

///////////////////// RID /////////////////////////

void EditorPropertyRID::update_property() {
	RID rid = get_edited_property_value();
	if (rid.is_valid()) {
		uint64_t id = rid.get_id();
		label->set_text("RID: " + uitos(id));
	} else {
		label->set_text(TTR("Invalid RID"));
	}
}

EditorPropertyRID::EditorPropertyRID() {
	label = memnew(Label);
	add_child(label);
}

////////////// RESOURCE //////////////////////

void EditorPropertyResource::_set_read_only(bool p_read_only) {
	resource_picker->set_editable(!p_read_only);
}

void EditorPropertyResource::_resource_selected(const Ref<Resource> &p_resource, bool p_inspect) {
	if (p_resource->is_built_in() && !p_resource->get_path().is_empty()) {
		String parent = p_resource->get_path().get_slice("::", 0);
		List<String> extensions;
		ResourceLoader::get_recognized_extensions_for_type("PackedScene", &extensions);

		if (p_inspect) {
			if (extensions.find(parent.get_extension()) && (!EditorNode::get_singleton()->get_edited_scene() || EditorNode::get_singleton()->get_edited_scene()->get_scene_file_path() != parent)) {
				// If the resource belongs to another (non-imported) scene, edit it in that scene instead.
				if (!FileAccess::exists(parent + ".import")) {
					callable_mp(EditorNode::get_singleton(), &EditorNode::edit_foreign_resource).call_deferred(p_resource);
					return;
				}
			}
		}
	}

	if (!p_inspect && use_sub_inspector) {
		bool unfold = !get_edited_object()->editor_is_section_unfolded(get_edited_property());
		get_edited_object()->editor_set_section_unfold(get_edited_property(), unfold);
		update_property();
	} else {
		emit_signal(SNAME("resource_selected"), get_edited_property(), p_resource);
	}
}

static bool _find_recursive_resources(const Variant &v, HashSet<Resource *> &resources_found) {
	switch (v.get_type()) {
		case Variant::ARRAY: {
			Array a = v;
			for (int i = 0; i < a.size(); i++) {
				Variant v2 = a[i];
				if (v2.get_type() != Variant::ARRAY && v2.get_type() != Variant::DICTIONARY && v2.get_type() != Variant::OBJECT) {
					continue;
				}
				if (_find_recursive_resources(v2, resources_found)) {
					return true;
				}
			}
		} break;
		case Variant::DICTIONARY: {
			Dictionary d = v;
			List<Variant> keys;
			d.get_key_list(&keys);
			for (const Variant &k : keys) {
				if (k.get_type() == Variant::ARRAY || k.get_type() == Variant::DICTIONARY || k.get_type() == Variant::OBJECT) {
					if (_find_recursive_resources(k, resources_found)) {
						return true;
					}
				}
				Variant v2 = d[k];
				if (v2.get_type() == Variant::ARRAY || v2.get_type() == Variant::DICTIONARY || v2.get_type() == Variant::OBJECT) {
					if (_find_recursive_resources(v2, resources_found)) {
						return true;
					}
				}
			}
		} break;
		case Variant::OBJECT: {
			Ref<Resource> r = v;

			if (r.is_null()) {
				return false;
			}

			if (resources_found.has(r.ptr())) {
				return true;
			}

			resources_found.insert(r.ptr());

			List<PropertyInfo> plist;
			r->get_property_list(&plist);
			for (const PropertyInfo &pinfo : plist) {
				if (!(pinfo.usage & PROPERTY_USAGE_STORAGE)) {
					continue;
				}

				if (pinfo.type != Variant::ARRAY && pinfo.type != Variant::DICTIONARY && pinfo.type != Variant::OBJECT) {
					continue;
				}
				if (_find_recursive_resources(r->get(pinfo.name), resources_found)) {
					return true;
				}
			}

			resources_found.erase(r.ptr());
		} break;
		default: {
		}
	}
	return false;
}

void EditorPropertyResource::_resource_changed(const Ref<Resource> &p_resource) {
	Resource *r = Object::cast_to<Resource>(get_edited_object());
	if (r) {
		// Check for recursive setting of resource
		HashSet<Resource *> resources_found;
		resources_found.insert(r);
		bool found = _find_recursive_resources(p_resource, resources_found);
		if (found) {
			EditorNode::get_singleton()->show_warning(TTR("Recursion detected, unable to assign resource to property."));
			emit_changed(get_edited_property(), Ref<Resource>());
			update_property();
			return;
		}
	}

	// The bool is_script applies only to an object's main script.
	// Changing the value of Script-type exported variables of the main script should not trigger saving/reloading properties.
	bool is_script = false;
	Ref<Script> s = p_resource;
	if (get_edited_object() && s.is_valid() && get_edited_property() == CoreStringName(script)) {
		is_script = true;
		InspectorDock::get_singleton()->store_script_properties(get_edited_object());
		s->call("set_instance_base_type", get_edited_object()->get_class());
	}

	// Prevent the creation of invalid ViewportTextures when possible.
	Ref<ViewportTexture> vpt = p_resource;
	if (vpt.is_valid()) {
		r = Object::cast_to<Resource>(get_edited_object());
		if (Object::cast_to<VisualShaderNodeTexture>(r)) {
			EditorNode::get_singleton()->show_warning(TTR("Can't create a ViewportTexture in a Texture2D node because the texture will not be bound to a scene.\nUse a Texture2DParameter node instead and set the texture in the \"Shader Parameters\" tab."));
			emit_changed(get_edited_property(), Ref<Resource>());
			update_property();
			return;
		}

		if (r && r->get_path().is_resource_file()) {
			EditorNode::get_singleton()->show_warning(TTR("Can't create a ViewportTexture on resources saved as a file.\nResource needs to belong to a scene."));
			emit_changed(get_edited_property(), Ref<Resource>());
			update_property();
			return;
		}

		if (r && !r->is_local_to_scene()) {
			EditorNode::get_singleton()->show_warning(TTR("Can't create a ViewportTexture on this resource because it's not set as local to scene.\nPlease switch on the 'local to scene' property on it (and all resources containing it up to a node)."));
			emit_changed(get_edited_property(), Ref<Resource>());
			update_property();
			return;
		}
	}

	emit_changed(get_edited_property(), p_resource);
	update_property();

	if (is_script) {
		// Restore properties if script was changed.
		InspectorDock::get_singleton()->apply_script_properties(get_edited_object());
	}

	// Automatically suggest setting up the path for a ViewportTexture.
	if (vpt.is_valid() && vpt->get_viewport_path_in_scene().is_empty()) {
		if (!scene_tree) {
			scene_tree = memnew(SceneTreeDialog);
			scene_tree->set_title(TTR("Pick a Viewport"));

			Vector<StringName> valid_types;
			valid_types.push_back("Viewport");
			scene_tree->set_valid_types(valid_types);
			scene_tree->get_scene_tree()->set_show_enabled_subscene(true);

			add_child(scene_tree);
			scene_tree->connect("selected", callable_mp(this, &EditorPropertyResource::_viewport_selected));
		}
		scene_tree->popup_scenetree_dialog();
	}
}

void EditorPropertyResource::_sub_inspector_property_keyed(const String &p_property, const Variant &p_value, bool p_advance) {
	// The second parameter could be null, causing the event to fire with less arguments, so use the pointer call which preserves it.
	const Variant args[3] = { String(get_edited_property()) + ":" + p_property, p_value, p_advance };
	const Variant *argp[3] = { &args[0], &args[1], &args[2] };
	emit_signalp(SNAME("property_keyed_with_value"), argp, 3);
}

void EditorPropertyResource::_sub_inspector_resource_selected(const Ref<Resource> &p_resource, const String &p_property) {
	emit_signal(SNAME("resource_selected"), String(get_edited_property()) + ":" + p_property, p_resource);
}

void EditorPropertyResource::_sub_inspector_object_id_selected(int p_id) {
	emit_signal(SNAME("object_id_selected"), get_edited_property(), p_id);
}

void EditorPropertyResource::_open_editor_pressed() {
	Ref<Resource> res = get_edited_property_value();
	if (res.is_valid()) {
		// May clear the editor so do it deferred.
		callable_mp(EditorNode::get_singleton(), &EditorNode::edit_item).call_deferred(res.ptr(), this);
	}
}

void EditorPropertyResource::_update_preferred_shader() {
	Node *parent = get_parent();
	EditorProperty *parent_property = nullptr;

	while (parent && !parent_property) {
		parent_property = Object::cast_to<EditorProperty>(parent);
		parent = parent->get_parent();
	}

	if (parent_property) {
		EditorShaderPicker *shader_picker = Object::cast_to<EditorShaderPicker>(resource_picker);
		Object *ed_object = parent_property->get_edited_object();
		const StringName &ed_property = parent_property->get_edited_property();

		// Set preferred shader based on edited parent type.
		if ((Object::cast_to<GPUParticles2D>(ed_object) || Object::cast_to<GPUParticles3D>(ed_object)) && ed_property == SNAME("process_material")) {
			shader_picker->set_preferred_mode(Shader::MODE_PARTICLES);
		} else if (Object::cast_to<FogVolume>(ed_object)) {
			shader_picker->set_preferred_mode(Shader::MODE_FOG);
		} else if (Object::cast_to<CanvasItem>(ed_object)) {
			shader_picker->set_preferred_mode(Shader::MODE_CANVAS_ITEM);
		} else if (Object::cast_to<Node3D>(ed_object) || Object::cast_to<Mesh>(ed_object)) {
			shader_picker->set_preferred_mode(Shader::MODE_SPATIAL);
		} else if (Object::cast_to<Sky>(ed_object)) {
			shader_picker->set_preferred_mode(Shader::MODE_SKY);
		}
	}
}

bool EditorPropertyResource::_should_stop_editing() const {
	return !resource_picker->is_toggle_pressed();
}

void EditorPropertyResource::_viewport_selected(const NodePath &p_path) {
	Node *to_node = get_node(p_path);
	if (!Object::cast_to<Viewport>(to_node)) {
		EditorNode::get_singleton()->show_warning(TTR("Selected node is not a Viewport!"));
		return;
	}

	Ref<ViewportTexture> vt;
	vt.instantiate();
	vt->set_viewport_path_in_scene(get_tree()->get_edited_scene_root()->get_path_to(to_node));

	emit_changed(get_edited_property(), vt);
	update_property();
}

void EditorPropertyResource::setup(Object *p_object, const String &p_path, const String &p_base_type) {
	if (resource_picker) {
		memdelete(resource_picker);
		resource_picker = nullptr;
	}

	if (p_path == "script" && p_base_type == "Script" && Object::cast_to<Node>(p_object)) {
		EditorScriptPicker *script_picker = memnew(EditorScriptPicker);
		script_picker->set_script_owner(Object::cast_to<Node>(p_object));
		resource_picker = script_picker;
	} else if (p_path == "shader" && p_base_type == "Shader" && Object::cast_to<ShaderMaterial>(p_object)) {
		EditorShaderPicker *shader_picker = memnew(EditorShaderPicker);
		shader_picker->set_edited_material(Object::cast_to<ShaderMaterial>(p_object));
		resource_picker = shader_picker;
		connect(SceneStringName(ready), callable_mp(this, &EditorPropertyResource::_update_preferred_shader));
	} else if (p_base_type == "AudioStream") {
		EditorAudioStreamPicker *astream_picker = memnew(EditorAudioStreamPicker);
		resource_picker = astream_picker;
	} else {
		resource_picker = memnew(EditorResourcePicker);
	}

	resource_picker->set_base_type(p_base_type);
	resource_picker->set_resource_owner(p_object);
	resource_picker->set_editable(true);
	resource_picker->set_h_size_flags(SIZE_EXPAND_FILL);
	add_child(resource_picker);

	resource_picker->connect("resource_selected", callable_mp(this, &EditorPropertyResource::_resource_selected));
	resource_picker->connect("resource_changed", callable_mp(this, &EditorPropertyResource::_resource_changed));

	for (int i = 0; i < resource_picker->get_child_count(); i++) {
		Button *b = Object::cast_to<Button>(resource_picker->get_child(i));
		if (b) {
			add_focusable(b);
		}
	}
}

void EditorPropertyResource::update_property() {
	Ref<Resource> res = get_edited_property_value();

	if (use_sub_inspector) {
		if (res.is_valid() != resource_picker->is_toggle_mode()) {
			resource_picker->set_toggle_mode(res.is_valid());
		}

		if (res.is_valid() && get_edited_object()->editor_is_section_unfolded(get_edited_property())) {
			if (!sub_inspector) {
				sub_inspector = memnew(EditorInspector);
				sub_inspector->set_vertical_scroll_mode(ScrollContainer::SCROLL_MODE_DISABLED);
				sub_inspector->set_use_doc_hints(true);

				EditorInspector *parent_inspector = get_parent_inspector();
				ERR_FAIL_NULL(parent_inspector);
				sub_inspector->set_root_inspector(parent_inspector->get_root_inspector());

				sub_inspector->set_property_name_style(InspectorDock::get_singleton()->get_property_name_style());

				sub_inspector->connect("property_keyed", callable_mp(this, &EditorPropertyResource::_sub_inspector_property_keyed));
				sub_inspector->connect("resource_selected", callable_mp(this, &EditorPropertyResource::_sub_inspector_resource_selected));
				sub_inspector->connect("object_id_selected", callable_mp(this, &EditorPropertyResource::_sub_inspector_object_id_selected));
				sub_inspector->set_keying(is_keying());
				sub_inspector->set_read_only(is_read_only());
				sub_inspector->set_use_folding(is_using_folding());

				sub_inspector->set_mouse_filter(MOUSE_FILTER_STOP);
				add_child(sub_inspector);
				set_bottom_editor(sub_inspector);

				resource_picker->set_toggle_pressed(true);

				Array editor_list;
				for (int i = 0; i < EditorNode::get_editor_data().get_editor_plugin_count(); i++) {
					EditorPlugin *ep = EditorNode::get_editor_data().get_editor_plugin(i);
					if (ep->handles(res.ptr())) {
						editor_list.push_back(ep);
					}
				}

				if (!editor_list.is_empty()) {
					// Open editor directly.
					_open_editor_pressed();
					opened_editor = true;
				}
			}

			if (res.ptr() != sub_inspector->get_edited_object()) {
				sub_inspector->edit(res.ptr());
				_update_property_bg();
			}

		} else {
			if (sub_inspector) {
				set_bottom_editor(nullptr);
				memdelete(sub_inspector);
				sub_inspector = nullptr;

				if (opened_editor) {
					EditorNode::get_singleton()->hide_unused_editors();
					opened_editor = false;
				}
			}
		}
	}

	resource_picker->set_edited_resource_no_check(res);
}

void EditorPropertyResource::collapse_all_folding() {
	if (sub_inspector) {
		sub_inspector->collapse_all_folding();
	}
}

void EditorPropertyResource::expand_all_folding() {
	if (sub_inspector) {
		sub_inspector->expand_all_folding();
	}
}

void EditorPropertyResource::expand_revertable() {
	if (sub_inspector) {
		sub_inspector->expand_revertable();
	}
}

void EditorPropertyResource::set_use_sub_inspector(bool p_enable) {
	use_sub_inspector = p_enable;
}

void EditorPropertyResource::fold_resource() {
	bool unfolded = get_edited_object()->editor_is_section_unfolded(get_edited_property());
	if (unfolded) {
		resource_picker->set_toggle_pressed(false);
		get_edited_object()->editor_set_section_unfold(get_edited_property(), false);
		update_property();
	}
}

bool EditorPropertyResource::is_colored(ColorationMode p_mode) {
	switch (p_mode) {
		case COLORATION_CONTAINER_RESOURCE:
			return sub_inspector != nullptr;
		case COLORATION_RESOURCE:
			return true;
		case COLORATION_EXTERNAL:
			if (sub_inspector) {
				Resource *edited_resource = Object::cast_to<Resource>(sub_inspector->get_edited_object());
				return edited_resource && !edited_resource->is_built_in();
			}
			break;
	}
	return false;
}

void EditorPropertyResource::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_EXIT_TREE: {
			const EditorInspector *ei = get_parent_inspector();
			const EditorInspector *main_ei = InspectorDock::get_inspector_singleton();
			if (ei && main_ei && ei != main_ei && !main_ei->is_ancestor_of(ei)) {
				fold_resource();
			}
		} break;
	}
}

void EditorPropertyResource::_bind_methods() {
	ClassDB::bind_method(D_METHOD("_should_stop_editing"), &EditorPropertyResource::_should_stop_editing);
}

EditorPropertyResource::EditorPropertyResource() {
	use_sub_inspector = bool(EDITOR_GET("interface/inspector/open_resources_in_current_inspector"));
	has_borders = true;
}

////////////// DEFAULT PLUGIN //////////////////////

bool EditorInspectorDefaultPlugin::can_handle(Object *p_object) {
	return true; // Can handle everything.
}

bool EditorInspectorDefaultPlugin::parse_property(Object *p_object, const Variant::Type p_type, const String &p_path, const PropertyHint p_hint, const String &p_hint_text, const BitField<PropertyUsageFlags> p_usage, const bool p_wide) {
	Control *editor = EditorInspectorDefaultPlugin::get_editor_for_property(p_object, p_type, p_path, p_hint, p_hint_text, p_usage, p_wide);
	if (editor) {
		add_property_editor(p_path, editor);
	}
	return false;
}

struct EditorPropertyRangeHint {
	bool or_greater = true;
	bool or_less = true;
	double min = -99999.0;
	double max = 99999.0;
	double step = 1.0;
	String suffix;
	bool exp_range = false;
	bool hide_slider = true;
	bool radians_as_degrees = false;
};

static EditorPropertyRangeHint _parse_range_hint(PropertyHint p_hint, const String &p_hint_text, double p_default_step, bool is_int = false) {
	EditorPropertyRangeHint hint;
	hint.step = p_default_step;
	if (is_int) {
		hint.hide_slider = false; // Always show slider for ints, unless specified in hint range.
	}
	Vector<String> slices = p_hint_text.split(",");
	if (p_hint == PROPERTY_HINT_RANGE) {
		ERR_FAIL_COND_V_MSG(slices.size() < 2, hint,
				vformat("Invalid PROPERTY_HINT_RANGE with hint \"%s\": Missing required min and/or max values.", p_hint_text));

		hint.or_greater = false; // If using ranged, assume false by default.
		hint.or_less = false;

		hint.min = slices[0].to_float();
		hint.max = slices[1].to_float();

		if (slices.size() >= 3 && slices[2].is_valid_float()) {
			// Step is optional, could be something else if not a number.
			hint.step = slices[2].to_float();
		}
		hint.hide_slider = false;
		for (int i = 2; i < slices.size(); i++) {
			String slice = slices[i].strip_edges();
			if (slice == "or_greater") {
				hint.or_greater = true;
			} else if (slice == "or_less") {
				hint.or_less = true;
			} else if (slice == "hide_slider") {
				hint.hide_slider = true;
			} else if (slice == "exp") {
				hint.exp_range = true;
			}
		}
	}
	bool degrees = false;
	for (int i = 0; i < slices.size(); i++) {
		String slice = slices[i].strip_edges();
		if (slice == "radians_as_degrees"
#ifndef DISABLE_DEPRECATED
				|| slice == "radians"
#endif // DISABLE_DEPRECATED
		) {
			hint.radians_as_degrees = true;
		} else if (slice == "degrees") {
			degrees = true;
		} else if (slice.begins_with("suffix:")) {
			hint.suffix = " " + slice.replace_first("suffix:", "").strip_edges();
		}
	}

	if ((hint.radians_as_degrees || degrees) && hint.suffix.is_empty()) {
		hint.suffix = U"\u00B0";
	}

	ERR_FAIL_COND_V_MSG(hint.step == 0, hint,
			vformat("Invalid PROPERTY_HINT_RANGE with hint \"%s\": Step cannot be 0.", p_hint_text));

	return hint;
}

EditorProperty *EditorInspectorDefaultPlugin::get_editor_for_property(Object *p_object, const Variant::Type p_type, const String &p_path, const PropertyHint p_hint, const String &p_hint_text, const BitField<PropertyUsageFlags> p_usage, const bool p_wide) {
	double default_float_step = EDITOR_GET("interface/inspector/default_float_step");

	switch (p_type) {
		// atomic types
		case Variant::NIL: {
			EditorPropertyNil *editor = memnew(EditorPropertyNil);
			return editor;
		} break;
		case Variant::BOOL: {
			EditorPropertyCheck *editor = memnew(EditorPropertyCheck);
			return editor;
		} break;
		case Variant::INT: {
			if (p_hint == PROPERTY_HINT_ENUM) {
				EditorPropertyEnum *editor = memnew(EditorPropertyEnum);
				Vector<String> options = p_hint_text.split(",");
				editor->setup(options);
				return editor;

			} else if (p_hint == PROPERTY_HINT_FLAGS) {
				EditorPropertyFlags *editor = memnew(EditorPropertyFlags);
				Vector<String> options = p_hint_text.split(",");
				editor->setup(options);
				return editor;

			} else if (p_hint == PROPERTY_HINT_LAYERS_2D_PHYSICS ||
					p_hint == PROPERTY_HINT_LAYERS_2D_RENDER ||
					p_hint == PROPERTY_HINT_LAYERS_2D_NAVIGATION ||
					p_hint == PROPERTY_HINT_LAYERS_3D_PHYSICS ||
					p_hint == PROPERTY_HINT_LAYERS_3D_RENDER ||
					p_hint == PROPERTY_HINT_LAYERS_3D_NAVIGATION ||
					p_hint == PROPERTY_HINT_LAYERS_AVOIDANCE) {
				EditorPropertyLayers::LayerType lt = EditorPropertyLayers::LAYER_RENDER_2D;
				switch (p_hint) {
					case PROPERTY_HINT_LAYERS_2D_RENDER:
						lt = EditorPropertyLayers::LAYER_RENDER_2D;
						break;
					case PROPERTY_HINT_LAYERS_2D_PHYSICS:
						lt = EditorPropertyLayers::LAYER_PHYSICS_2D;
						break;
					case PROPERTY_HINT_LAYERS_2D_NAVIGATION:
						lt = EditorPropertyLayers::LAYER_NAVIGATION_2D;
						break;
					case PROPERTY_HINT_LAYERS_3D_RENDER:
						lt = EditorPropertyLayers::LAYER_RENDER_3D;
						break;
					case PROPERTY_HINT_LAYERS_3D_PHYSICS:
						lt = EditorPropertyLayers::LAYER_PHYSICS_3D;
						break;
					case PROPERTY_HINT_LAYERS_3D_NAVIGATION:
						lt = EditorPropertyLayers::LAYER_NAVIGATION_3D;
						break;
					case PROPERTY_HINT_LAYERS_AVOIDANCE:
						lt = EditorPropertyLayers::LAYER_AVOIDANCE;
						break;
					default: {
					} //compiler could be smarter here and realize this can't happen
				}
				EditorPropertyLayers *editor = memnew(EditorPropertyLayers);
				editor->setup(lt);
				return editor;
			} else if (p_hint == PROPERTY_HINT_OBJECT_ID) {
				EditorPropertyObjectID *editor = memnew(EditorPropertyObjectID);
				editor->setup(p_hint_text);
				return editor;

			} else {
				EditorPropertyInteger *editor = memnew(EditorPropertyInteger);

				EditorPropertyRangeHint hint = _parse_range_hint(p_hint, p_hint_text, 1, true);
				editor->setup(hint.min, hint.max, hint.step, hint.hide_slider, hint.or_greater, hint.or_less, hint.suffix);

				return editor;
			}
		} break;
		case Variant::FLOAT: {
			if (p_hint == PROPERTY_HINT_EXP_EASING) {
				EditorPropertyEasing *editor = memnew(EditorPropertyEasing);
				bool positive_only = false;
				bool flip = false;
				const Vector<String> hints = p_hint_text.split(",");
				for (int i = 0; i < hints.size(); i++) {
					const String hint = hints[i].strip_edges();
					if (hint == "attenuation") {
						flip = true;
					}
					if (hint == "positive_only") {
						positive_only = true;
					}
				}

				editor->setup(positive_only, flip);
				return editor;

			} else {
				EditorPropertyFloat *editor = memnew(EditorPropertyFloat);

				EditorPropertyRangeHint hint = _parse_range_hint(p_hint, p_hint_text, default_float_step);
				editor->setup(hint.min, hint.max, hint.step, hint.hide_slider, hint.exp_range, hint.or_greater, hint.or_less, hint.suffix, hint.radians_as_degrees);

				return editor;
			}
		} break;
		case Variant::STRING: {
			if (p_hint == PROPERTY_HINT_ENUM || p_hint == PROPERTY_HINT_ENUM_SUGGESTION) {
				EditorPropertyTextEnum *editor = memnew(EditorPropertyTextEnum);
				Vector<String> options = p_hint_text.split(",", false);
				editor->setup(options, false, (p_hint == PROPERTY_HINT_ENUM_SUGGESTION));
				return editor;
			} else if (p_hint == PROPERTY_HINT_MULTILINE_TEXT) {
				EditorPropertyMultilineText *editor = memnew(EditorPropertyMultilineText);
				return editor;
			} else if (p_hint == PROPERTY_HINT_EXPRESSION) {
				EditorPropertyMultilineText *editor = memnew(EditorPropertyMultilineText(true));
				return editor;
			} else if (p_hint == PROPERTY_HINT_TYPE_STRING) {
				EditorPropertyClassName *editor = memnew(EditorPropertyClassName);
				editor->setup(p_hint_text, p_hint_text);
				return editor;
			} else if (p_hint == PROPERTY_HINT_LOCALE_ID) {
				EditorPropertyLocale *editor = memnew(EditorPropertyLocale);
				editor->setup(p_hint_text);
				return editor;
			} else if (p_hint == PROPERTY_HINT_DIR || p_hint == PROPERTY_HINT_FILE || p_hint == PROPERTY_HINT_SAVE_FILE || p_hint == PROPERTY_HINT_GLOBAL_SAVE_FILE || p_hint == PROPERTY_HINT_GLOBAL_DIR || p_hint == PROPERTY_HINT_GLOBAL_FILE) {
				Vector<String> extensions = p_hint_text.split(",");
				bool global = p_hint == PROPERTY_HINT_GLOBAL_DIR || p_hint == PROPERTY_HINT_GLOBAL_FILE || p_hint == PROPERTY_HINT_GLOBAL_SAVE_FILE;
				bool folder = p_hint == PROPERTY_HINT_DIR || p_hint == PROPERTY_HINT_GLOBAL_DIR;
				bool save = p_hint == PROPERTY_HINT_SAVE_FILE || p_hint == PROPERTY_HINT_GLOBAL_SAVE_FILE;
				EditorPropertyPath *editor = memnew(EditorPropertyPath);
				editor->setup(extensions, folder, global);
				if (save) {
					editor->set_save_mode();
				}
				return editor;
			} else {
				EditorPropertyText *editor = memnew(EditorPropertyText);
				if (p_hint == PROPERTY_HINT_PLACEHOLDER_TEXT) {
					editor->set_placeholder(p_hint_text);
				} else if (p_hint == PROPERTY_HINT_PASSWORD) {
					editor->set_secret(true);
					editor->set_placeholder(p_hint_text);
				}
				return editor;
			}
		} break;

			// math types

		case Variant::VECTOR2: {
			EditorPropertyVector2 *editor = memnew(EditorPropertyVector2(p_wide));

			EditorPropertyRangeHint hint = _parse_range_hint(p_hint, p_hint_text, default_float_step);
			editor->setup(hint.min, hint.max, hint.step, hint.hide_slider, p_hint == PROPERTY_HINT_LINK, hint.suffix);
			return editor;

		} break;
		case Variant::VECTOR2I: {
			EditorPropertyVector2i *editor = memnew(EditorPropertyVector2i(p_wide));
			EditorPropertyRangeHint hint = _parse_range_hint(p_hint, p_hint_text, 1, true);
			editor->setup(hint.min, hint.max, 1, false, p_hint == PROPERTY_HINT_LINK, hint.suffix);
			return editor;

		} break;
		case Variant::RECT2: {
			EditorPropertyRect2 *editor = memnew(EditorPropertyRect2(p_wide));
			EditorPropertyRangeHint hint = _parse_range_hint(p_hint, p_hint_text, default_float_step);
			editor->setup(hint.min, hint.max, hint.step, hint.hide_slider, hint.suffix);
			return editor;
		} break;
		case Variant::RECT2I: {
			EditorPropertyRect2i *editor = memnew(EditorPropertyRect2i(p_wide));
			EditorPropertyRangeHint hint = _parse_range_hint(p_hint, p_hint_text, 1, true);
			editor->setup(hint.min, hint.max, hint.suffix);

			return editor;
		} break;
		case Variant::VECTOR3: {
			EditorPropertyVector3 *editor = memnew(EditorPropertyVector3(p_wide));
			EditorPropertyRangeHint hint = _parse_range_hint(p_hint, p_hint_text, default_float_step);
			editor->setup(hint.min, hint.max, hint.step, hint.hide_slider, p_hint == PROPERTY_HINT_LINK, hint.suffix, hint.radians_as_degrees);
			return editor;

		} break;
		case Variant::VECTOR3I: {
			EditorPropertyVector3i *editor = memnew(EditorPropertyVector3i(p_wide));
			EditorPropertyRangeHint hint = _parse_range_hint(p_hint, p_hint_text, 1, true);
			editor->setup(hint.min, hint.max, 1, false, p_hint == PROPERTY_HINT_LINK, hint.suffix);
			return editor;

		} break;
		case Variant::VECTOR4: {
			EditorPropertyVector4 *editor = memnew(EditorPropertyVector4);
			EditorPropertyRangeHint hint = _parse_range_hint(p_hint, p_hint_text, default_float_step);
			editor->setup(hint.min, hint.max, hint.step, hint.hide_slider, p_hint == PROPERTY_HINT_LINK, hint.suffix);
			return editor;

		} break;
		case Variant::VECTOR4I: {
			EditorPropertyVector4i *editor = memnew(EditorPropertyVector4i);
			EditorPropertyRangeHint hint = _parse_range_hint(p_hint, p_hint_text, 1, true);
			editor->setup(hint.min, hint.max, 1, false, p_hint == PROPERTY_HINT_LINK, hint.suffix);
			return editor;

		} break;
		case Variant::TRANSFORM2D: {
			EditorPropertyTransform2D *editor = memnew(EditorPropertyTransform2D);
			EditorPropertyRangeHint hint = _parse_range_hint(p_hint, p_hint_text, default_float_step);
			editor->setup(hint.min, hint.max, hint.step, hint.hide_slider, hint.suffix);
			return editor;
		} break;
		case Variant::PLANE: {
			EditorPropertyPlane *editor = memnew(EditorPropertyPlane(p_wide));
			EditorPropertyRangeHint hint = _parse_range_hint(p_hint, p_hint_text, default_float_step);
			editor->setup(hint.min, hint.max, hint.step, hint.hide_slider, hint.suffix);
			return editor;
		} break;
		case Variant::QUATERNION: {
			EditorPropertyQuaternion *editor = memnew(EditorPropertyQuaternion);
			EditorPropertyRangeHint hint = _parse_range_hint(p_hint, p_hint_text, default_float_step);
			editor->setup(hint.min, hint.max, hint.step, hint.hide_slider, hint.suffix, p_hint == PROPERTY_HINT_HIDE_QUATERNION_EDIT);
			return editor;
		} break;
		case Variant::AABB: {
			EditorPropertyAABB *editor = memnew(EditorPropertyAABB);
			EditorPropertyRangeHint hint = _parse_range_hint(p_hint, p_hint_text, default_float_step);
			editor->setup(hint.min, hint.max, hint.step, hint.hide_slider, hint.suffix);
			return editor;
		} break;
		case Variant::BASIS: {
			EditorPropertyBasis *editor = memnew(EditorPropertyBasis);
			EditorPropertyRangeHint hint = _parse_range_hint(p_hint, p_hint_text, default_float_step);
			editor->setup(hint.min, hint.max, hint.step, hint.hide_slider, hint.suffix);
			return editor;
		} break;
		case Variant::TRANSFORM3D: {
			EditorPropertyTransform3D *editor = memnew(EditorPropertyTransform3D);
			EditorPropertyRangeHint hint = _parse_range_hint(p_hint, p_hint_text, default_float_step);
			editor->setup(hint.min, hint.max, hint.step, hint.hide_slider, hint.suffix);
			return editor;

		} break;
		case Variant::PROJECTION: {
			EditorPropertyProjection *editor = memnew(EditorPropertyProjection);
			EditorPropertyRangeHint hint = _parse_range_hint(p_hint, p_hint_text, default_float_step);
			editor->setup(hint.min, hint.max, hint.step, hint.hide_slider, hint.suffix);
			return editor;

		} break;

		// misc types
		case Variant::COLOR: {
			EditorPropertyColor *editor = memnew(EditorPropertyColor);
			editor->setup(p_hint != PROPERTY_HINT_COLOR_NO_ALPHA);
			return editor;
		} break;
		case Variant::STRING_NAME: {
			if (p_hint == PROPERTY_HINT_ENUM || p_hint == PROPERTY_HINT_ENUM_SUGGESTION) {
				EditorPropertyTextEnum *editor = memnew(EditorPropertyTextEnum);
				Vector<String> options = p_hint_text.split(",", false);
				editor->setup(options, true, (p_hint == PROPERTY_HINT_ENUM_SUGGESTION));
				return editor;
			} else {
				EditorPropertyText *editor = memnew(EditorPropertyText);
				if (p_hint == PROPERTY_HINT_PLACEHOLDER_TEXT) {
					editor->set_placeholder(p_hint_text);
				} else if (p_hint == PROPERTY_HINT_PASSWORD) {
					editor->set_secret(true);
					editor->set_placeholder(p_hint_text);
				}
				editor->set_string_name(true);
				return editor;
			}
		} break;
		case Variant::NODE_PATH: {
			EditorPropertyNodePath *editor = memnew(EditorPropertyNodePath);
			if (p_hint == PROPERTY_HINT_NODE_PATH_VALID_TYPES && !p_hint_text.is_empty()) {
				Vector<String> types = p_hint_text.split(",", false);
				Vector<StringName> sn = Variant(types); //convert via variant
				editor->setup(sn, (p_usage & PROPERTY_USAGE_NODE_PATH_FROM_SCENE_ROOT));
			}
			return editor;

		} break;
		case Variant::RID: {
			EditorPropertyRID *editor = memnew(EditorPropertyRID);
			return editor;
		} break;
		case Variant::OBJECT: {
			if (p_hint == PROPERTY_HINT_NODE_TYPE) {
				EditorPropertyNodePath *editor = memnew(EditorPropertyNodePath);
				Vector<String> types = p_hint_text.split(",", false);
				Vector<StringName> sn = Variant(types); //convert via variant
				editor->setup(sn, false, true);
				return editor;
			} else {
				EditorPropertyResource *editor = memnew(EditorPropertyResource);
				editor->setup(p_object, p_path, p_hint == PROPERTY_HINT_RESOURCE_TYPE ? p_hint_text : "Resource");

				if (p_hint == PROPERTY_HINT_RESOURCE_TYPE) {
					const PackedStringArray open_in_new_inspector = EDITOR_GET("interface/inspector/resources_to_open_in_new_inspector");

					for (const String &type : open_in_new_inspector) {
						for (int j = 0; j < p_hint_text.get_slice_count(","); j++) {
							const String inherits = p_hint_text.get_slicec(',', j);
							if (ClassDB::is_parent_class(inherits, type)) {
								editor->set_use_sub_inspector(false);
							}
						}
					}
				}

				return editor;
			}

		} break;
		case Variant::CALLABLE: {
			EditorPropertyCallable *editor = memnew(EditorPropertyCallable);
			return editor;
		} break;
		case Variant::SIGNAL: {
			EditorPropertySignal *editor = memnew(EditorPropertySignal);
			return editor;
		} break;
		case Variant::DICTIONARY: {
			if (p_hint == PROPERTY_HINT_LOCALIZABLE_STRING) {
				EditorPropertyLocalizableString *editor = memnew(EditorPropertyLocalizableString);
				return editor;
			} else {
				EditorPropertyDictionary *editor = memnew(EditorPropertyDictionary);
				editor->setup(p_hint, p_hint_text);
				return editor;
			}
		} break;
		case Variant::ARRAY: {
			EditorPropertyArray *editor = memnew(EditorPropertyArray);
			editor->setup(Variant::ARRAY, p_hint_text);
			return editor;
		} break;
		case Variant::PACKED_BYTE_ARRAY: {
			EditorPropertyArray *editor = memnew(EditorPropertyArray);
			editor->setup(Variant::PACKED_BYTE_ARRAY, p_hint_text);
			return editor;
		} break;
		case Variant::PACKED_INT32_ARRAY: {
			EditorPropertyArray *editor = memnew(EditorPropertyArray);
			editor->setup(Variant::PACKED_INT32_ARRAY, p_hint_text);
			return editor;
		} break;
		case Variant::PACKED_INT64_ARRAY: {
			EditorPropertyArray *editor = memnew(EditorPropertyArray);
			editor->setup(Variant::PACKED_INT64_ARRAY, p_hint_text);
			return editor;
		} break;
		case Variant::PACKED_FLOAT32_ARRAY: {
			EditorPropertyArray *editor = memnew(EditorPropertyArray);
			editor->setup(Variant::PACKED_FLOAT32_ARRAY, p_hint_text);
			return editor;
		} break;
		case Variant::PACKED_FLOAT64_ARRAY: {
			EditorPropertyArray *editor = memnew(EditorPropertyArray);
			editor->setup(Variant::PACKED_FLOAT64_ARRAY, p_hint_text);
			return editor;
		} break;
		case Variant::PACKED_STRING_ARRAY: {
			EditorPropertyArray *editor = memnew(EditorPropertyArray);
			editor->setup(Variant::PACKED_STRING_ARRAY, p_hint_text);
			return editor;
		} break;
		case Variant::PACKED_VECTOR2_ARRAY: {
			EditorPropertyArray *editor = memnew(EditorPropertyArray);
			editor->setup(Variant::PACKED_VECTOR2_ARRAY, p_hint_text);
			return editor;
		} break;
		case Variant::PACKED_VECTOR3_ARRAY: {
			EditorPropertyArray *editor = memnew(EditorPropertyArray);
			editor->setup(Variant::PACKED_VECTOR3_ARRAY, p_hint_text);
			return editor;
		} break;
		case Variant::PACKED_COLOR_ARRAY: {
			EditorPropertyArray *editor = memnew(EditorPropertyArray);
			editor->setup(Variant::PACKED_COLOR_ARRAY, p_hint_text);
			return editor;
		} break;
		case Variant::PACKED_VECTOR4_ARRAY: {
			EditorPropertyArray *editor = memnew(EditorPropertyArray);
			editor->setup(Variant::PACKED_VECTOR4_ARRAY, p_hint_text);
			return editor;
		} break;
		default: {
		}
	}

	return nullptr;
}

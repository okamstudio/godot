/**************************************************************************/
/*  dynamic_font_import_settings.cpp                                      */
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

#include "dynamic_font_import_settings.h"

#include "unicode_ranges.inc"

#include "core/config/project_settings.h"
#include "core/string/translation.h"
#include "editor/editor_file_system.h"
#include "editor/editor_inspector.h"
#include "editor/editor_locale_dialog.h"
#include "editor/editor_node.h"
#include "editor/editor_string_names.h"
#include "editor/gui/editor_file_dialog.h"
#include "editor/themes/editor_scale.h"
#include "scene/gui/split_container.h"

/*************************************************************************/
/* Settings data                                                         */
/*************************************************************************/

bool DynamicFontImportSettingsData::_set(const StringName &p_name, const Variant &p_value) {
	if (defaults.has(p_name) && defaults[p_name] == p_value) {
		settings.erase(p_name);
	} else {
		settings[p_name] = p_value;
	}
	return true;
}

bool DynamicFontImportSettingsData::_get(const StringName &p_name, Variant &r_ret) const {
	if (settings.has(p_name)) {
		r_ret = settings[p_name];
		return true;
	}
	if (defaults.has(p_name)) {
		r_ret = defaults[p_name];
		return true;
	}
	return false;
}

void DynamicFontImportSettingsData::_get_property_list(List<PropertyInfo> *p_list) const {
	for (const List<ResourceImporter::ImportOption>::Element *E = options.front(); E; E = E->next()) {
		if (owner && owner->import_settings_data.is_valid()) {
			if (owner->import_settings_data->get("multichannel_signed_distance_field") && (E->get().option.name == "size" || E->get().option.name == "outline_size" || E->get().option.name == "oversampling")) {
				continue;
			}
			if (!owner->import_settings_data->get("multichannel_signed_distance_field") && (E->get().option.name == "msdf_pixel_range" || E->get().option.name == "msdf_size")) {
				continue;
			}
		}
		p_list->push_back(E->get().option);
	}
}

Ref<FontFile> DynamicFontImportSettingsData::get_font() const {
	return fd;
}

/*************************************************************************/
/* Glyph ranges                                                          */
/*************************************************************************/

void DynamicFontImportSettingsDialog::_add_glyph_range_item(int32_t p_start, int32_t p_end, const String &p_name) {
	const int page_size = 512;
	int pages = (p_end - p_start) / page_size;
	int remain = (p_end - p_start) % page_size;

	int32_t start = p_start;
	for (int i = 0; i < pages; i++) {
		TreeItem *item = glyph_tree->create_item(glyph_root);
		ERR_FAIL_NULL(item);
		item->set_cell_mode(0, TreeItem::CELL_MODE_CHECK);
		item->set_text(0, _pad_zeros(String::num_int64(start, 16)) + " - " + _pad_zeros(String::num_int64(start + page_size, 16)));
		item->set_text(1, p_name);
		item->set_metadata(0, Vector2i(start, start + page_size));
		start += page_size;
	}
	if (remain > 0) {
		TreeItem *item = glyph_tree->create_item(glyph_root);
		ERR_FAIL_NULL(item);
		item->set_cell_mode(0, TreeItem::CELL_MODE_CHECK);
		item->set_text(0, _pad_zeros(String::num_int64(start, 16)) + " - " + _pad_zeros(String::num_int64(p_end, 16)));
		item->set_text(1, p_name);
		item->set_metadata(0, Vector2i(start, p_end));
	}
}

/*************************************************************************/
/* Page 1 callbacks: Rendering Options                                   */
/*************************************************************************/

void DynamicFontImportSettingsDialog::_main_prop_changed(const String &p_edited_property) {
	// Update font preview.

	if (font_preview.is_valid()) {
		if (p_edited_property == "antialiasing") {
			font_preview->set_antialiasing((TextServer::FontAntialiasing)import_settings_data->get("antialiasing").operator int());
		} else if (p_edited_property == "generate_mipmaps") {
			font_preview->set_generate_mipmaps(import_settings_data->get("generate_mipmaps"));
		} else if (p_edited_property == "disable_embedded_bitmaps") {
			font_preview->set_disable_embedded_bitmaps(import_settings_data->get("disable_embedded_bitmaps"));
		} else if (p_edited_property == "multichannel_signed_distance_field") {
			font_preview->set_multichannel_signed_distance_field(import_settings_data->get("multichannel_signed_distance_field"));
			_variation_selected();
			_variations_validate();
		} else if (p_edited_property == "msdf_pixel_range") {
			font_preview->set_msdf_pixel_range(import_settings_data->get("msdf_pixel_range"));
		} else if (p_edited_property == "msdf_size") {
			font_preview->set_msdf_size(import_settings_data->get("msdf_size"));
		} else if (p_edited_property == "allow_system_fallback") {
			font_preview->set_allow_system_fallback(import_settings_data->get("allow_system_fallback"));
		} else if (p_edited_property == "force_autohinter") {
			font_preview->set_force_autohinter(import_settings_data->get("force_autohinter"));
		} else if (p_edited_property == "hinting") {
			font_preview->set_hinting((TextServer::Hinting)import_settings_data->get("hinting").operator int());
		} else if (p_edited_property == "subpixel_positioning") {
			font_preview->set_subpixel_positioning((TextServer::SubpixelPositioning)import_settings_data->get("subpixel_positioning").operator int());
		} else if (p_edited_property == "keep_rounding_remainders") {
			font_preview->set_keep_rounding_remainders(import_settings_data->get("keep_rounding_remainders"));
		} else if (p_edited_property == "oversampling") {
			font_preview->set_oversampling(import_settings_data->get("oversampling"));
		}
	}

	font_preview_label->add_theme_font_override(SceneStringName(font), font_preview);
	font_preview_label->add_theme_font_size_override(SceneStringName(font_size), 200 * EDSCALE);
	font_preview_label->queue_redraw();
}

/*************************************************************************/
/* Page 2 callbacks: Configurations                                      */
/*************************************************************************/

void DynamicFontImportSettingsDialog::_variation_add() {
	TreeItem *vars_item = vars_list->create_item(vars_list_root);
	ERR_FAIL_NULL(vars_item);

	vars_item->set_text(0, TTR("New Configuration"));
	vars_item->set_editable(0, true);
	vars_item->add_button(1, get_editor_theme_icon(SNAME("Remove")), BUTTON_REMOVE_VAR, false, TTR("Remove Variation"));
	vars_item->set_button_color(1, 0, Color(1, 1, 1, 0.75));

	Ref<DynamicFontImportSettingsData> import_variation_data;
	import_variation_data.instantiate();
	import_variation_data->owner = this;
	ERR_FAIL_COND(import_variation_data.is_null());

	for (List<ResourceImporter::ImportOption>::Element *E = options_variations.front(); E; E = E->next()) {
		import_variation_data->defaults[E->get().option.name] = E->get().default_value;
	}

	import_variation_data->options = options_variations;
	inspector_vars->edit(import_variation_data.ptr());
	import_variation_data->notify_property_list_changed();
	import_variation_data->fd = font_main;

	vars_item->set_metadata(0, import_variation_data);

	_variations_validate();
}

void DynamicFontImportSettingsDialog::_variation_selected() {
	TreeItem *vars_item = vars_list->get_selected();
	if (vars_item) {
		Ref<DynamicFontImportSettingsData> import_variation_data = vars_item->get_metadata(0);
		ERR_FAIL_COND(import_variation_data.is_null());

		inspector_vars->edit(import_variation_data.ptr());
		import_variation_data->notify_property_list_changed();

		label_glyphs->set_text(vformat(TTR("Preloaded glyphs: %d"), import_variation_data->selected_glyphs.size()));
		_range_selected();
		_change_text_opts();

		btn_fill->set_disabled(false);
		btn_fill_locales->set_disabled(false);
	} else {
		btn_fill->set_disabled(true);
		btn_fill_locales->set_disabled(true);
	}
}

void DynamicFontImportSettingsDialog::_variation_remove(Object *p_item, int p_column, int p_id, MouseButton p_button) {
	if (p_button != MouseButton::LEFT) {
		return;
	}

	TreeItem *vars_item = (TreeItem *)p_item;
	ERR_FAIL_NULL(vars_item);

	inspector_vars->edit(nullptr);

	vars_list_root->remove_child(vars_item);
	memdelete(vars_item);

	if (vars_list_root->get_first_child()) {
		Ref<DynamicFontImportSettingsData> import_variation_data = vars_list_root->get_first_child()->get_metadata(0);
		inspector_vars->edit(import_variation_data.ptr());
		import_variation_data->notify_property_list_changed();
	}

	_variations_validate();

	vars_item = vars_list->get_selected();
	if (vars_item) {
		btn_fill->set_disabled(false);
		btn_fill_locales->set_disabled(false);
	} else {
		btn_fill->set_disabled(true);
		btn_fill_locales->set_disabled(true);
	}
}

void DynamicFontImportSettingsDialog::_variation_changed(const String &p_edited_property) {
	_variations_validate();
}

void DynamicFontImportSettingsDialog::_variations_validate() {
	String warn;
	if (!vars_list_root->get_first_child()) {
		warn = TTR("Warning: There are no configurations specified, no glyphs will be pre-rendered.");
	}
	for (TreeItem *vars_item_a = vars_list_root->get_first_child(); vars_item_a; vars_item_a = vars_item_a->get_next()) {
		Ref<DynamicFontImportSettingsData> import_variation_data_a = vars_item_a->get_metadata(0);
		ERR_FAIL_COND(import_variation_data_a.is_null());

		for (TreeItem *vars_item_b = vars_list_root->get_first_child(); vars_item_b; vars_item_b = vars_item_b->get_next()) {
			if (vars_item_b != vars_item_a) {
				bool match = true;
				for (const KeyValue<StringName, Variant> &E : import_variation_data_a->settings) {
					Ref<DynamicFontImportSettingsData> import_variation_data_b = vars_item_b->get_metadata(0);
					ERR_FAIL_COND(import_variation_data_b.is_null());
					match = match && (import_variation_data_b->settings[E.key] == E.value);
				}
				if (match) {
					warn = TTR("Warning: Multiple configurations have identical settings. Duplicates will be ignored.");
					break;
				}
			}
		}
	}
	if ((TextServer::FontAntialiasing)(int)import_settings_data->get("antialiasing") == TextServer::FONT_ANTIALIASING_LCD) {
		warn += "\n" + TTR("Note: LCD Subpixel antialiasing is selected, each of the glyphs will be pre-rendered for all supported subpixel layouts (5x).");
	}
	if ((TextServer::SubpixelPositioning)(int)import_settings_data->get("subpixel_positioning") != TextServer::SUBPIXEL_POSITIONING_DISABLED) {
		warn += "\n" + TTR("Note: Subpixel positioning is selected, each of the glyphs might be pre-rendered for multiple subpixel offsets (up to 4x).");
	}
	if (warn.is_empty()) {
		label_warn->set_text("");
		label_warn->hide();
	} else {
		label_warn->set_text(warn);
		label_warn->show();
	}
}

/*************************************************************************/
/* Page 2.1 callbacks: Text to select glyphs                             */
/*************************************************************************/

void DynamicFontImportSettingsDialog::_change_text_opts() {
	Ref<DynamicFontImportSettingsData> import_variation_data;

	TreeItem *vars_item = vars_list->get_selected();
	if (vars_item) {
		import_variation_data = vars_item->get_metadata(0);
	}
	if (import_variation_data.is_null()) {
		return;
	}

	Ref<FontVariation> font_main_text;
	font_main_text.instantiate();
	font_main_text->set_base_font(font_main);
	font_main_text->set_opentype_features(text_settings_data->get("opentype_features"));
	font_main_text->set_variation_opentype(import_variation_data->get("variation_opentype"));
	font_main_text->set_variation_embolden(import_variation_data->get("variation_embolden"));
	font_main_text->set_variation_face_index(import_variation_data->get("variation_face_index"));
	font_main_text->set_variation_transform(import_variation_data->get("variation_transform"));

	text_edit->add_theme_font_override(SceneStringName(font), font_main_text);
}

void DynamicFontImportSettingsDialog::_glyph_update_lbl() {
	Ref<DynamicFontImportSettingsData> import_variation_data;

	TreeItem *vars_item = vars_list->get_selected();
	if (vars_item) {
		import_variation_data = vars_item->get_metadata(0);
	}
	if (import_variation_data.is_null()) {
		return;
	}

	int linked_glyphs = 0;
	for (const char32_t &c : import_variation_data->selected_chars) {
		if (import_variation_data->selected_glyphs.has(font_main->get_glyph_index(16, c))) {
			linked_glyphs++;
		}
	}
	int unlinked_glyphs = import_variation_data->selected_glyphs.size() - linked_glyphs;
	label_glyphs->set_text(vformat(TTR("Preloaded glyphs: %d"), unlinked_glyphs + import_variation_data->selected_chars.size()));
}

void DynamicFontImportSettingsDialog::_glyph_clear() {
	Ref<DynamicFontImportSettingsData> import_variation_data;

	TreeItem *vars_item = vars_list->get_selected();
	if (vars_item) {
		import_variation_data = vars_item->get_metadata(0);
	}
	if (import_variation_data.is_null()) {
		return;
	}

	import_variation_data->selected_glyphs.clear();
	_glyph_update_lbl();
	_range_selected();
}

void DynamicFontImportSettingsDialog::_glyph_text_selected() {
	Ref<DynamicFontImportSettingsData> import_variation_data;

	TreeItem *vars_item = vars_list->get_selected();
	if (vars_item) {
		import_variation_data = vars_item->get_metadata(0);
	}
	if (import_variation_data.is_null()) {
		return;
	}
	RID text_rid = TS->create_shaped_text();
	if (text_rid.is_valid()) {
		TS->shaped_text_add_string(text_rid, text_edit->get_text(), font_main->get_rids(), 16, text_settings_data->get("opentype_features"), text_settings_data->get("language"));
		TS->shaped_text_shape(text_rid);
		const Glyph *gl = TS->shaped_text_get_glyphs(text_rid);
		const int gl_size = TS->shaped_text_get_glyph_count(text_rid);

		for (int i = 0; i < gl_size; i++) {
			if (gl[i].font_rid.is_valid() && gl[i].index != 0) {
				import_variation_data->selected_glyphs.insert(gl[i].index);
			}
		}
		TS->free_rid(text_rid);
		_glyph_update_lbl();
	}
	_range_selected();
}

/*************************************************************************/
/* Page 2.2 callbacks: Character map                                     */
/*************************************************************************/

void DynamicFontImportSettingsDialog::_glyph_selected() {
	Ref<DynamicFontImportSettingsData> import_variation_data;

	TreeItem *vars_item = vars_list->get_selected();
	if (vars_item) {
		import_variation_data = vars_item->get_metadata(0);
	}
	if (import_variation_data.is_null()) {
		return;
	}

	TreeItem *item = glyph_table->get_selected();
	ERR_FAIL_NULL(item);

	Color scol = glyph_table->get_theme_color(SNAME("box_selection_fill_color"), EditorStringName(Editor));
	Color fcol = glyph_table->get_theme_color(SNAME("font_selected_color"), EditorStringName(Editor));
	scol.a = 1.f;

	int32_t c = item->get_metadata(glyph_table->get_selected_column());
	if (font_main->has_char(c)) {
		if (_char_update(c)) {
			item->set_custom_color(glyph_table->get_selected_column(), fcol);
			item->set_custom_bg_color(glyph_table->get_selected_column(), scol);
		} else {
			item->clear_custom_color(glyph_table->get_selected_column());
			item->clear_custom_bg_color(glyph_table->get_selected_column());
		}
	}
	_glyph_update_lbl();

	item = glyph_tree->get_selected();
	ERR_FAIL_NULL(item);
	Vector2i range = item->get_metadata(0);

	int total_chars = range.y - range.x;
	int selected_count = 0;
	for (int i = range.x; i < range.y; i++) {
		if (!font_main->has_char(i)) {
			total_chars--;
		}

		if (import_variation_data->selected_chars.has(i)) {
			selected_count++;
		}
	}

	if (selected_count == total_chars) {
		item->set_checked(0, true);
	} else if (selected_count > 0) {
		item->set_indeterminate(0, true);
	} else {
		item->set_checked(0, false);
	}
}

void DynamicFontImportSettingsDialog::_range_edited() {
	TreeItem *item = glyph_tree->get_selected();
	ERR_FAIL_NULL(item);
	Vector2i range = item->get_metadata(0);
	_range_update(range.x, range.y);
}

void DynamicFontImportSettingsDialog::_range_selected() {
	TreeItem *item = glyph_tree->get_selected();
	if (item) {
		Vector2i range = item->get_metadata(0);
		_edit_range(range.x, range.y);
	}
}

void DynamicFontImportSettingsDialog::_edit_range(int32_t p_start, int32_t p_end) {
	Ref<DynamicFontImportSettingsData> import_variation_data;

	TreeItem *vars_item = vars_list->get_selected();
	if (vars_item) {
		import_variation_data = vars_item->get_metadata(0);
	}
	if (import_variation_data.is_null()) {
		return;
	}

	glyph_table->clear();

	TreeItem *root = glyph_table->create_item();
	ERR_FAIL_NULL(root);

	Color scol = glyph_table->get_theme_color(SNAME("box_selection_fill_color"), EditorStringName(Editor));
	Color fcol = glyph_table->get_theme_color(SNAME("font_selected_color"), EditorStringName(Editor));
	scol.a = 1.f;

	TreeItem *item = nullptr;
	int col = 0;

	Ref<Font> font_main_big = font_main->duplicate();

	for (int32_t c = p_start; c <= p_end; c++) {
		if (col == 0) {
			item = glyph_table->create_item(root);
			ERR_FAIL_NULL(item);
			item->set_text(0, _pad_zeros(String::num_int64(c, 16)));
			item->set_text_alignment(0, HORIZONTAL_ALIGNMENT_LEFT);
			item->set_selectable(0, false);
			item->set_custom_bg_color(0, glyph_table->get_theme_color(SNAME("dark_color_3"), EditorStringName(Editor)));
		}
		if (font_main->has_char(c)) {
			item->set_text(col + 1, String::chr(c));
			item->set_custom_color(col + 1, Color(1, 1, 1));
			if (import_variation_data->selected_chars.has(c) || import_variation_data->selected_glyphs.has(font_main->get_glyph_index(16, c))) {
				item->set_custom_color(col + 1, fcol);
				item->set_custom_bg_color(col + 1, scol);
			} else {
				item->clear_custom_color(col + 1);
				item->clear_custom_bg_color(col + 1);
			}
		} else {
			item->set_custom_bg_color(col + 1, glyph_table->get_theme_color(SNAME("dark_color_2"), EditorStringName(Editor)));
		}
		item->set_metadata(col + 1, c);
		item->set_text_alignment(col + 1, HORIZONTAL_ALIGNMENT_CENTER);
		item->set_selectable(col + 1, true);

		item->set_custom_font(col + 1, font_main_big);
		item->set_custom_font_size(col + 1, get_theme_font_size(SceneStringName(font_size)) * 2);

		col++;
		if (col == 16) {
			col = 0;
		}
	}
	_glyph_update_lbl();
}

bool DynamicFontImportSettingsDialog::_char_update(int32_t p_char) {
	Ref<DynamicFontImportSettingsData> import_variation_data;

	TreeItem *vars_item = vars_list->get_selected();
	if (vars_item) {
		import_variation_data = vars_item->get_metadata(0);
	}
	if (import_variation_data.is_null()) {
		return false;
	}

	if (import_variation_data->selected_chars.has(p_char)) {
		import_variation_data->selected_chars.erase(p_char);
		return false;
	} else if (font_main.is_valid() && import_variation_data->selected_glyphs.has(font_main->get_glyph_index(16, p_char))) {
		import_variation_data->selected_glyphs.erase(font_main->get_glyph_index(16, p_char));
		return false;
	} else {
		import_variation_data->selected_chars.insert(p_char);
		return true;
	}
}

void DynamicFontImportSettingsDialog::_range_update(int32_t p_start, int32_t p_end) {
	Ref<DynamicFontImportSettingsData> import_variation_data;

	TreeItem *vars_item = vars_list->get_selected();
	if (vars_item) {
		import_variation_data = vars_item->get_metadata(0);
	}
	if (import_variation_data.is_null()) {
		return;
	}

	bool all_selected = true;
	for (int32_t i = p_start; i <= p_end; i++) {
		if (font_main->has_char(i)) {
			if (font_main.is_valid()) {
				all_selected = all_selected && (import_variation_data->selected_chars.has(i) || import_variation_data->selected_glyphs.has(font_main->get_glyph_index(16, i)));
			} else {
				all_selected = all_selected && import_variation_data->selected_chars.has(i);
			}
		}
	}
	for (int32_t i = p_start; i <= p_end; i++) {
		if (font_main->has_char(i)) {
			if (!all_selected) {
				import_variation_data->selected_chars.insert(i);
			} else {
				import_variation_data->selected_chars.erase(i);
				if (font_main.is_valid()) {
					import_variation_data->selected_glyphs.erase(font_main->get_glyph_index(16, i));
				}
			}
		}
	}
	_edit_range(p_start, p_end);

	TreeItem *item = glyph_tree->get_selected();
	ERR_FAIL_NULL(item);
	item->set_checked(0, !all_selected);
}

/*************************************************************************/
/* Common                                                                */
/*************************************************************************/

DynamicFontImportSettingsDialog *DynamicFontImportSettingsDialog::singleton = nullptr;

String DynamicFontImportSettingsDialog::_pad_zeros(const String &p_hex) const {
	int len = CLAMP(5 - p_hex.length(), 0, 5);
	return String("0").repeat(len) + p_hex;
}

void DynamicFontImportSettingsDialog::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY: {
			connect(SceneStringName(confirmed), callable_mp(this, &DynamicFontImportSettingsDialog::_re_import));
		} break;

		case NOTIFICATION_THEME_CHANGED: {
			add_var->set_button_icon(get_editor_theme_icon(SNAME("Add")));
			label_warn->add_theme_color_override(SceneStringName(font_color), get_theme_color(SNAME("warning_color"), EditorStringName(Editor)));
		} break;
	}
}

void DynamicFontImportSettingsDialog::_re_import() {
	HashMap<StringName, Variant> main_settings;

	main_settings["face_index"] = import_settings_data->get("face_index");
	main_settings["antialiasing"] = import_settings_data->get("antialiasing");
	main_settings["generate_mipmaps"] = import_settings_data->get("generate_mipmaps");
	main_settings["disable_embedded_bitmaps"] = import_settings_data->get("disable_embedded_bitmaps");
	main_settings["multichannel_signed_distance_field"] = import_settings_data->get("multichannel_signed_distance_field");
	main_settings["msdf_pixel_range"] = import_settings_data->get("msdf_pixel_range");
	main_settings["msdf_size"] = import_settings_data->get("msdf_size");
	main_settings["allow_system_fallback"] = import_settings_data->get("allow_system_fallback");
	main_settings["force_autohinter"] = import_settings_data->get("force_autohinter");
	main_settings["hinting"] = import_settings_data->get("hinting");
	main_settings["subpixel_positioning"] = import_settings_data->get("subpixel_positioning");
	main_settings["keep_rounding_remainders"] = import_settings_data->get("keep_rounding_remainders");
	main_settings["oversampling"] = import_settings_data->get("oversampling");
	main_settings["fallbacks"] = import_settings_data->get("fallbacks");
	main_settings["compress"] = import_settings_data->get("compress");

	Array configurations;
	for (TreeItem *vars_item = vars_list_root->get_first_child(); vars_item; vars_item = vars_item->get_next()) {
		Ref<DynamicFontImportSettingsData> import_variation_data = vars_item->get_metadata(0);
		ERR_FAIL_COND(import_variation_data.is_null());

		Dictionary preload_config;
		preload_config["name"] = vars_item->get_text(0);

		Size2i conf_size = Vector2i(16, 0);
		for (const KeyValue<StringName, Variant> &E : import_variation_data->settings) {
			if (E.key == "size") {
				conf_size.x = E.value;
			}
			if (E.key == "outline_size") {
				conf_size.y = E.value;
			} else {
				preload_config[E.key] = E.value;
			}
		}
		preload_config["size"] = conf_size;

		Array chars;
		for (const char32_t &E : import_variation_data->selected_chars) {
			chars.push_back(E);
		}
		preload_config["chars"] = chars;

		Array glyphs;
		for (const int32_t &E : import_variation_data->selected_glyphs) {
			glyphs.push_back(E);
		}
		preload_config["glyphs"] = glyphs;

		configurations.push_back(preload_config);
	}
	main_settings["preload"] = configurations;
	main_settings["language_support"] = import_settings_data->get("language_support");
	main_settings["script_support"] = import_settings_data->get("script_support");
	main_settings["opentype_features"] = import_settings_data->get("opentype_features");

	if (OS::get_singleton()->is_stdout_verbose()) {
		print_line("Import settings:");
		for (const KeyValue<StringName, Variant> &E : main_settings) {
			print_line(String("    ") + String(E.key).utf8().get_data() + " == " + String(E.value).utf8().get_data());
		}
	}

	EditorFileSystem::get_singleton()->reimport_file_with_custom_parameters(base_path, "font_data_dynamic", main_settings);
}

void DynamicFontImportSettingsDialog::_locale_edited() {
	TreeItem *item = locale_tree->get_selected();
	ERR_FAIL_NULL(item);
	item->set_checked(0, !item->is_checked(0));
}

void DynamicFontImportSettingsDialog::_process_locales() {
	Ref<DynamicFontImportSettingsData> import_variation_data;

	TreeItem *vars_item = vars_list->get_selected();
	if (vars_item) {
		import_variation_data = vars_item->get_metadata(0);
	}
	if (import_variation_data.is_null()) {
		return;
	}

	for (int i = 0; i < locale_root->get_child_count(); i++) {
		TreeItem *item = locale_root->get_child(i);
		if (item) {
			if (item->is_checked(0)) {
				String locale = item->get_text(0);
				Ref<Translation> tr = ResourceLoader::load(locale);
				if (tr.is_valid()) {
					Vector<String> messages = tr->get_translated_message_list();
					for (const String &E : messages) {
						RID text_rid = TS->create_shaped_text();
						if (text_rid.is_valid()) {
							TS->shaped_text_add_string(text_rid, E, font_main->get_rids(), 16, Dictionary(), tr->get_locale());
							TS->shaped_text_shape(text_rid);
							const Glyph *gl = TS->shaped_text_get_glyphs(text_rid);
							const int gl_size = TS->shaped_text_get_glyph_count(text_rid);

							for (int j = 0; j < gl_size; j++) {
								if (gl[j].font_rid.is_valid() && gl[j].index != 0) {
									import_variation_data->selected_glyphs.insert(gl[j].index);
								}
							}
							TS->free_rid(text_rid);
						}
					}
				}
			}
		}
	}

	_glyph_update_lbl();
	_range_selected();
}

void DynamicFontImportSettingsDialog::open_settings(const String &p_path) {
	// Load base font data.
	Vector<uint8_t> font_data = FileAccess::get_file_as_bytes(p_path);

	// Load project locale list.
	locale_tree->clear();
	locale_root = locale_tree->create_item();
	ERR_FAIL_NULL(locale_root);

	Vector<String> translations = GLOBAL_GET("internationalization/locale/translations");
	for (const String &E : translations) {
		TreeItem *item = locale_tree->create_item(locale_root);
		ERR_FAIL_NULL(item);
		item->set_cell_mode(0, TreeItem::CELL_MODE_CHECK);
		item->set_text(0, E);
	}

	// Load font for preview.
	font_preview.instantiate();
	font_preview->set_data(font_data);

	String font_name = vformat("%s (%s)", font_preview->get_font_name(), font_preview->get_font_style_name());
	String sample;
	static const String sample_base = U"12漢字ԱբΑαАбΑαאבابܐܒހށआআਆઆଆஆఆಆആආกิກິༀကႠა한글ሀᎣᐁᚁᚠᜀᜠᝀᝠកᠠᤁᥐAb😀";
	for (int i = 0; i < sample_base.length(); i++) {
		if (font_preview->has_char(sample_base[i])) {
			sample += sample_base[i];
		}
	}
	if (sample.is_empty()) {
		sample = font_preview->get_supported_chars().substr(0, 6);
	}
	font_preview_label->set_text(sample);

	Ref<Font> bold_font = get_theme_font(SNAME("bold"), EditorStringName(EditorFonts));
	if (bold_font.is_valid()) {
		font_name_label->add_theme_font_override("bold_font", bold_font);
	}
	font_name_label->set_text(font_name);

	// Load second copy of font with MSDF disabled for the glyph table and metadata extraction.
	font_main.instantiate();
	font_main->set_data(font_data);
	font_main->set_multichannel_signed_distance_field(false);

	text_edit->add_theme_font_override(SceneStringName(font), font_main);

	base_path = p_path;

	inspector_vars->edit(nullptr);
	inspector_text->edit(nullptr);
	inspector_general->edit(nullptr);

	text_settings_data.instantiate();
	ERR_FAIL_COND(text_settings_data.is_null());

	text_settings_data->owner = this;

	for (List<ResourceImporter::ImportOption>::Element *F = options_text.front(); F; F = F->next()) {
		text_settings_data->defaults[F->get().option.name] = F->get().default_value;
	}

	text_settings_data->fd = font_main;
	text_settings_data->options = options_text;

	inspector_text->edit(text_settings_data.ptr());

	int gww = get_theme_font(SceneStringName(font))->get_string_size("00000").x + 50;
	glyph_table->set_column_custom_minimum_width(0, gww);
	glyph_table->clear();
	vars_list->clear();

	glyph_tree->set_selected(glyph_root->get_child(0));

	vars_list_root = vars_list->create_item();

	import_settings_data->settings.clear();
	import_settings_data->defaults.clear();
	for (List<ResourceImporter::ImportOption>::Element *E = options_general.front(); E; E = E->next()) {
		import_settings_data->defaults[E->get().option.name] = E->get().default_value;
	}

	Ref<ConfigFile> config;
	config.instantiate();
	ERR_FAIL_COND(config.is_null());

	Error err = config->load(p_path + ".import");
	print_verbose("Loading import settings:");
	if (err == OK) {
		List<String> keys;
		config->get_section_keys("params", &keys);
		for (List<String>::Element *E = keys.front(); E; E = E->next()) {
			String key = E->get();
			print_verbose(String("    ") + key + " == " + String(config->get_value("params", key)));
			if (key == "preload") {
				Array preload_configurations = config->get_value("params", key);
				for (int i = 0; i < preload_configurations.size(); i++) {
					Dictionary preload_config = preload_configurations[i];

					Dictionary variation = preload_config.has("variation_opentype") ? preload_config["variation_opentype"].operator Dictionary() : Dictionary();
					double embolden = preload_config.has("variation_embolden") ? preload_config["variation_embolden"].operator double() : 0;
					int face_index = preload_config.has("variation_face_index") ? preload_config["variation_face_index"].operator int() : 0;
					Transform2D transform = preload_config.has("variation_transform") ? preload_config["variation_transform"].operator Transform2D() : Transform2D();
					Vector2i font_size = preload_config.has("size") ? preload_config["size"].operator Vector2i() : Vector2i(16, 0);
					String cfg_name = preload_config.has("name") ? preload_config["name"].operator String() : vformat("Configuration %d", i);

					TreeItem *vars_item = vars_list->create_item(vars_list_root);
					ERR_FAIL_NULL(vars_item);

					vars_item->set_text(0, cfg_name);
					vars_item->set_editable(0, true);
					vars_item->add_button(1, get_editor_theme_icon(SNAME("Remove")), BUTTON_REMOVE_VAR, false, TTR("Remove Variation"));
					vars_item->set_button_color(1, 0, Color(1, 1, 1, 0.75));

					Ref<DynamicFontImportSettingsData> import_variation_data_custom;
					import_variation_data_custom.instantiate();
					ERR_FAIL_COND(import_variation_data_custom.is_null());

					import_variation_data_custom->owner = this;
					for (List<ResourceImporter::ImportOption>::Element *F = options_variations.front(); F; F = F->next()) {
						import_variation_data_custom->defaults[F->get().option.name] = F->get().default_value;
					}

					import_variation_data_custom->fd = font_main;

					import_variation_data_custom->options = options_variations;
					vars_item->set_metadata(0, import_variation_data_custom);

					import_variation_data_custom->set("size", font_size.x);
					import_variation_data_custom->set("outline_size", font_size.y);
					import_variation_data_custom->set("variation_opentype", variation);
					import_variation_data_custom->set("variation_embolden", embolden);
					import_variation_data_custom->set("variation_face_index", face_index);
					import_variation_data_custom->set("variation_transform", transform);

					Array chars = preload_config["chars"];
					for (int j = 0; j < chars.size(); j++) {
						char32_t c = chars[j].operator int();
						import_variation_data_custom->selected_chars.insert(c);
					}

					Array glyphs = preload_config["glyphs"];
					for (int j = 0; j < glyphs.size(); j++) {
						int32_t c = glyphs[j];
						import_variation_data_custom->selected_glyphs.insert(c);
					}
				}
				if (preload_configurations.is_empty()) {
					_variation_add(); // Add default variation.
				}
				vars_list->set_selected(vars_list_root->get_child(0));
			} else {
				Variant value = config->get_value("params", key);
				import_settings_data->defaults[key] = value;
			}
		}
	}

	import_settings_data->fd = font_main;
	import_settings_data->options = options_general;
	inspector_general->edit(import_settings_data.ptr());
	import_settings_data->notify_property_list_changed();

	if (font_preview.is_valid()) {
		font_preview->set_antialiasing((TextServer::FontAntialiasing)import_settings_data->get("antialiasing").operator int());
		font_preview->set_multichannel_signed_distance_field(import_settings_data->get("multichannel_signed_distance_field"));
		font_preview->set_msdf_pixel_range(import_settings_data->get("msdf_pixel_range"));
		font_preview->set_msdf_size(import_settings_data->get("msdf_size"));
		font_preview->set_allow_system_fallback(import_settings_data->get("allow_system_fallback"));
		font_preview->set_force_autohinter(import_settings_data->get("force_autohinter"));
		font_preview->set_hinting((TextServer::Hinting)import_settings_data->get("hinting").operator int());
		font_preview->set_subpixel_positioning((TextServer::SubpixelPositioning)import_settings_data->get("subpixel_positioning").operator int());
		font_preview->set_keep_rounding_remainders(import_settings_data->get("keep_rounding_remainders"));
		font_preview->set_oversampling(import_settings_data->get("oversampling"));
	}
	font_preview_label->add_theme_font_override(SceneStringName(font), font_preview);
	font_preview_label->add_theme_font_size_override(SceneStringName(font_size), 200 * EDSCALE);
	font_preview_label->queue_redraw();

	_variations_validate();

	popup_centered_ratio();

	set_title(vformat(TTR("Advanced Import Settings for '%s'"), base_path.get_file()));
}

DynamicFontImportSettingsDialog *DynamicFontImportSettingsDialog::get_singleton() {
	return singleton;
}

DynamicFontImportSettingsDialog::DynamicFontImportSettingsDialog() {
	singleton = this;

	options_general.push_back(ResourceImporter::ImportOption(PropertyInfo(Variant::NIL, "Rendering", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_GROUP), Variant()));

	options_general.push_back(ResourceImporter::ImportOption(PropertyInfo(Variant::INT, "antialiasing", PROPERTY_HINT_ENUM, "None,Grayscale,LCD Subpixel"), 1));
	options_general.push_back(ResourceImporter::ImportOption(PropertyInfo(Variant::BOOL, "generate_mipmaps"), false));
	options_general.push_back(ResourceImporter::ImportOption(PropertyInfo(Variant::BOOL, "disable_embedded_bitmaps"), true));
	options_general.push_back(ResourceImporter::ImportOption(PropertyInfo(Variant::BOOL, "multichannel_signed_distance_field", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_UPDATE_ALL_IF_MODIFIED), true));
	options_general.push_back(ResourceImporter::ImportOption(PropertyInfo(Variant::INT, "msdf_pixel_range", PROPERTY_HINT_RANGE, "1,100,1"), 8));
	options_general.push_back(ResourceImporter::ImportOption(PropertyInfo(Variant::INT, "msdf_size", PROPERTY_HINT_RANGE, "1,250,1"), 48));
	options_general.push_back(ResourceImporter::ImportOption(PropertyInfo(Variant::BOOL, "allow_system_fallback"), true));
	options_general.push_back(ResourceImporter::ImportOption(PropertyInfo(Variant::BOOL, "force_autohinter"), false));
	options_general.push_back(ResourceImporter::ImportOption(PropertyInfo(Variant::INT, "hinting", PROPERTY_HINT_ENUM, "None,Light,Normal"), 1));
	options_general.push_back(ResourceImporter::ImportOption(PropertyInfo(Variant::INT, "subpixel_positioning", PROPERTY_HINT_ENUM, "Disabled,Auto,One Half of a Pixel,One Quarter of a Pixel"), 1));
	options_general.push_back(ResourceImporter::ImportOption(PropertyInfo(Variant::BOOL, "keep_rounding_remainders"), true));
	options_general.push_back(ResourceImporter::ImportOption(PropertyInfo(Variant::FLOAT, "oversampling", PROPERTY_HINT_RANGE, "0,10,0.1"), 0.0));

	options_general.push_back(ResourceImporter::ImportOption(PropertyInfo(Variant::NIL, "Metadata Overrides", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_GROUP), Variant()));
	options_general.push_back(ResourceImporter::ImportOption(PropertyInfo(Variant::DICTIONARY, "language_support"), Dictionary()));
	options_general.push_back(ResourceImporter::ImportOption(PropertyInfo(Variant::DICTIONARY, "script_support"), Dictionary()));
	options_general.push_back(ResourceImporter::ImportOption(PropertyInfo(Variant::DICTIONARY, "opentype_features"), Dictionary()));

	options_general.push_back(ResourceImporter::ImportOption(PropertyInfo(Variant::NIL, "Fallbacks", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_GROUP), Variant()));
	options_general.push_back(ResourceImporter::ImportOption(PropertyInfo(Variant::ARRAY, "fallbacks", PROPERTY_HINT_ARRAY_TYPE, MAKE_RESOURCE_TYPE_HINT("Font")), Array()));

	options_general.push_back(ResourceImporter::ImportOption(PropertyInfo(Variant::NIL, "Compress", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_GROUP), Variant()));
	options_general.push_back(ResourceImporter::ImportOption(PropertyInfo(Variant::BOOL, "compress", PROPERTY_HINT_NONE, ""), false));

	options_text.push_back(ResourceImporter::ImportOption(PropertyInfo(Variant::DICTIONARY, "opentype_features"), Dictionary()));
	options_text.push_back(ResourceImporter::ImportOption(PropertyInfo(Variant::STRING, "language", PROPERTY_HINT_LOCALE_ID, ""), ""));

	options_variations.push_back(ResourceImporter::ImportOption(PropertyInfo(Variant::INT, "size", PROPERTY_HINT_RANGE, "0,127,1"), 16));
	options_variations.push_back(ResourceImporter::ImportOption(PropertyInfo(Variant::INT, "outline_size", PROPERTY_HINT_RANGE, "0,127,1"), 0));
	options_variations.push_back(ResourceImporter::ImportOption(PropertyInfo(Variant::NIL, "Variation", PROPERTY_HINT_NONE, "variation", PROPERTY_USAGE_GROUP), Variant()));
	options_variations.push_back(ResourceImporter::ImportOption(PropertyInfo(Variant::DICTIONARY, "variation_opentype"), Dictionary()));
	options_variations.push_back(ResourceImporter::ImportOption(PropertyInfo(Variant::FLOAT, "variation_embolden", PROPERTY_HINT_RANGE, "-2,2,0.01"), 0));
	options_variations.push_back(ResourceImporter::ImportOption(PropertyInfo(Variant::INT, "variation_face_index"), 0));
	options_variations.push_back(ResourceImporter::ImportOption(PropertyInfo(Variant::TRANSFORM2D, "variation_transform"), Transform2D()));

	// Root layout

	VBoxContainer *root_vb = memnew(VBoxContainer);
	add_child(root_vb);

	main_pages = memnew(TabContainer);
	main_pages->set_tab_alignment(TabBar::ALIGNMENT_CENTER);
	main_pages->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	main_pages->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	main_pages->set_theme_type_variation("TabContainerOdd");
	root_vb->add_child(main_pages);

	label_warn = memnew(Label);
	label_warn->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
	label_warn->set_text("");
	root_vb->add_child(label_warn);
	label_warn->hide();

	// Page 1 layout: Rendering Options

	VBoxContainer *page1_vb = memnew(VBoxContainer);
	page1_vb->set_name(TTR("Rendering Options"));
	main_pages->add_child(page1_vb);

	page1_description = memnew(Label);
	page1_description->set_text(TTR("Select font rendering options, fallback font, and metadata override:"));
	page1_description->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	page1_vb->add_child(page1_description);

	HSplitContainer *page1_hb = memnew(HSplitContainer);
	page1_hb->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	page1_hb->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	page1_vb->add_child(page1_hb);

	VBoxContainer *page1_lbl_vb = memnew(VBoxContainer);
	page1_lbl_vb->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	page1_lbl_vb->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	page1_hb->add_child(page1_lbl_vb);

	font_name_label = memnew(Label);
	font_name_label->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
	font_name_label->set_clip_text(true);
	font_name_label->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	page1_lbl_vb->add_child(font_name_label);

	font_preview_label = memnew(Label);
	font_preview_label->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
	font_preview_label->set_vertical_alignment(VERTICAL_ALIGNMENT_CENTER);
	font_preview_label->set_autowrap_mode(TextServer::AUTOWRAP_ARBITRARY);
	font_preview_label->set_clip_text(true);
	font_preview_label->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	font_preview_label->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	page1_lbl_vb->add_child(font_preview_label);

	inspector_general = memnew(EditorInspector);
	inspector_general->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	inspector_general->set_custom_minimum_size(Size2(300 * EDSCALE, 250 * EDSCALE));
	page1_hb->add_child(inspector_general);
	inspector_general->connect("property_edited", callable_mp(this, &DynamicFontImportSettingsDialog::_main_prop_changed));

	// Page 2 layout: Configurations
	VBoxContainer *page2_vb = memnew(VBoxContainer);
	page2_vb->set_name(TTR("Pre-render Configurations"));
	main_pages->add_child(page2_vb);

	page2_description = memnew(Label);
	page2_description->set_text(TTR("Add font size, and variation coordinates, and select glyphs to pre-render:"));
	page2_description->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	page2_description->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
	page2_description->set_custom_minimum_size(Size2(300 * EDSCALE, 1));
	page2_vb->add_child(page2_description);

	HSplitContainer *page2_hb = memnew(HSplitContainer);
	page2_hb->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	page2_hb->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	page2_vb->add_child(page2_hb);

	VBoxContainer *page2_side_vb = memnew(VBoxContainer);
	page2_hb->add_child(page2_side_vb);

	HBoxContainer *page2_hb_vars = memnew(HBoxContainer);
	page2_side_vb->add_child(page2_hb_vars);

	label_vars = memnew(Label);
	label_vars->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
	label_vars->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	label_vars->set_text(TTR("Configuration:"));
	page2_hb_vars->add_child(label_vars);

	add_var = memnew(Button);
	add_var->set_tooltip_text(TTR("Add configuration"));
	page2_hb_vars->add_child(add_var);
	add_var->connect(SceneStringName(pressed), callable_mp(this, &DynamicFontImportSettingsDialog::_variation_add));

	vars_list = memnew(Tree);
	vars_list->set_auto_translate_mode(AUTO_TRANSLATE_MODE_DISABLED);
	vars_list->set_custom_minimum_size(Size2(300 * EDSCALE, 0));
	vars_list->set_hide_root(true);
	vars_list->set_columns(2);
	vars_list->set_column_expand(0, true);
	vars_list->set_column_custom_minimum_width(0, 80 * EDSCALE);
	vars_list->set_column_expand(1, false);
	vars_list->set_column_custom_minimum_width(1, 50 * EDSCALE);
	vars_list->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	page2_side_vb->add_child(vars_list);
	vars_list->connect(SceneStringName(item_selected), callable_mp(this, &DynamicFontImportSettingsDialog::_variation_selected));
	vars_list->connect("button_clicked", callable_mp(this, &DynamicFontImportSettingsDialog::_variation_remove));

	inspector_vars = memnew(EditorInspector);
	inspector_vars->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	page2_side_vb->add_child(inspector_vars);
	inspector_vars->connect("property_edited", callable_mp(this, &DynamicFontImportSettingsDialog::_variation_changed));

	VBoxContainer *preload_pages_vb = memnew(VBoxContainer);
	page2_hb->add_child(preload_pages_vb);

	preload_pages = memnew(TabContainer);
	preload_pages->set_tab_alignment(TabBar::ALIGNMENT_CENTER);
	preload_pages->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	preload_pages->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	preload_pages_vb->add_child(preload_pages);

	HBoxContainer *gl_hb = memnew(HBoxContainer);
	gl_hb->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	preload_pages_vb->add_child(gl_hb);

	label_glyphs = memnew(Label);
	label_glyphs->set_text(vformat(TTR("Preloaded glyphs: %d"), 0));
	label_glyphs->set_custom_minimum_size(Size2(50 * EDSCALE, 0));
	gl_hb->add_child(label_glyphs);

	Button *btn_clear = memnew(Button);
	btn_clear->set_text(TTR("Clear Glyph List"));
	gl_hb->add_child(btn_clear);
	btn_clear->connect(SceneStringName(pressed), callable_mp(this, &DynamicFontImportSettingsDialog::_glyph_clear));

	VBoxContainer *page2_0_vb = memnew(VBoxContainer);
	page2_0_vb->set_name(TTR("Glyphs from the Translations"));
	preload_pages->add_child(page2_0_vb);

	page2_0_description = memnew(Label);
	page2_0_description->set_text(TTR("Select translations to add all required glyphs to pre-render list:"));
	page2_0_description->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	page2_0_description->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
	page2_0_description->set_custom_minimum_size(Size2(300 * EDSCALE, 1));
	page2_0_vb->add_child(page2_0_description);

	locale_tree = memnew(Tree);
	locale_tree->set_auto_translate_mode(AUTO_TRANSLATE_MODE_DISABLED);
	locale_tree->set_columns(1);
	locale_tree->set_hide_root(true);
	locale_tree->set_column_expand(0, true);
	locale_tree->set_column_custom_minimum_width(0, 120 * EDSCALE);
	locale_tree->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	page2_0_vb->add_child(locale_tree);
	locale_tree->connect("item_activated", callable_mp(this, &DynamicFontImportSettingsDialog::_locale_edited));

	locale_root = locale_tree->create_item();

	HBoxContainer *locale_hb = memnew(HBoxContainer);
	locale_hb->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	page2_0_vb->add_child(locale_hb);

	btn_fill_locales = memnew(Button);
	btn_fill_locales->set_text(TTR("Shape all Strings in the Translations and Add Glyphs"));
	locale_hb->add_child(btn_fill_locales);
	btn_fill_locales->connect(SceneStringName(pressed), callable_mp(this, &DynamicFontImportSettingsDialog::_process_locales));

	// Page 2.1 layout: Text to select glyphs
	VBoxContainer *page2_1_vb = memnew(VBoxContainer);
	page2_1_vb->set_name(TTR("Glyphs from the Text"));
	preload_pages->add_child(page2_1_vb);

	page2_1_description = memnew(Label);
	page2_1_description->set_text(TTR("Enter a text and select OpenType features to shape and add all required glyphs to pre-render list:"));
	page2_1_description->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	page2_1_description->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
	page2_1_description->set_custom_minimum_size(Size2(300 * EDSCALE, 1));
	page2_1_vb->add_child(page2_1_description);

	HSplitContainer *page2_1_hb = memnew(HSplitContainer);
	page2_1_hb->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	page2_1_hb->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	page2_1_vb->add_child(page2_1_hb);

	inspector_text = memnew(EditorInspector);

	inspector_text->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	inspector_text->set_custom_minimum_size(Size2(300 * EDSCALE, 250 * EDSCALE));
	page2_1_hb->add_child(inspector_text);
	inspector_text->connect("property_edited", callable_mp(this, &DynamicFontImportSettingsDialog::_change_text_opts));

	text_edit = memnew(TextEdit);
	text_edit->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	text_edit->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	page2_1_hb->add_child(text_edit);

	HBoxContainer *text_hb = memnew(HBoxContainer);
	text_hb->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	page2_1_vb->add_child(text_hb);

	btn_fill = memnew(Button);
	btn_fill->set_text(TTR("Shape Text and Add Glyphs"));
	text_hb->add_child(btn_fill);
	btn_fill->connect(SceneStringName(pressed), callable_mp(this, &DynamicFontImportSettingsDialog::_glyph_text_selected));

	// Page 2.2 layout: Character map
	VBoxContainer *page2_2_vb = memnew(VBoxContainer);
	page2_2_vb->set_name(TTR("Glyphs from the Character Map"));
	preload_pages->add_child(page2_2_vb);

	page2_2_description = memnew(Label);
	page2_2_description->set_text(TTR("Add or remove glyphs from the character map to pre-render list:\nNote: Some stylistic alternatives and glyph variants do not have one-to-one correspondence to character, and not shown in this map, use \"Glyphs from the text\" tab to add these."));
	page2_2_description->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	page2_2_description->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
	page2_2_description->set_custom_minimum_size(Size2(300 * EDSCALE, 1));
	page2_2_vb->add_child(page2_2_description);

	HSplitContainer *glyphs_split = memnew(HSplitContainer);
	glyphs_split->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	glyphs_split->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	page2_2_vb->add_child(glyphs_split);

	glyph_table = memnew(Tree);
	glyph_table->set_auto_translate_mode(AUTO_TRANSLATE_MODE_DISABLED);
	glyph_table->set_custom_minimum_size(Size2((30 * 16 + 100) * EDSCALE, 0));
	glyph_table->set_columns(17);
	glyph_table->set_column_expand(0, false);
	glyph_table->set_hide_root(true);
	glyph_table->set_allow_reselect(true);
	glyph_table->set_select_mode(Tree::SELECT_SINGLE);
	glyph_table->set_column_titles_visible(true);
	for (int i = 0; i < 16; i++) {
		glyph_table->set_column_title(i + 1, String::num_int64(i, 16));
	}
	glyph_table->add_theme_style_override("selected", glyph_table->get_theme_stylebox(SceneStringName(panel)));
	glyph_table->add_theme_style_override("selected_focus", glyph_table->get_theme_stylebox(SceneStringName(panel)));
	glyph_table->add_theme_constant_override("h_separation", 0);
	glyph_table->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	glyph_table->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	glyphs_split->add_child(glyph_table);
	glyph_table->connect("item_activated", callable_mp(this, &DynamicFontImportSettingsDialog::_glyph_selected));

	glyph_tree = memnew(Tree);
	glyph_tree->set_auto_translate_mode(AUTO_TRANSLATE_MODE_DISABLED);
	glyph_tree->set_custom_minimum_size(Size2(300 * EDSCALE, 0));
	glyph_tree->set_columns(2);
	glyph_tree->set_hide_root(true);
	glyph_tree->set_column_expand(0, false);
	glyph_tree->set_column_expand(1, true);
	glyph_tree->set_column_custom_minimum_width(0, 120 * EDSCALE);
	glyph_tree->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	glyph_root = glyph_tree->create_item();
	for (int i = 0; !unicode_ranges[i].name.is_empty(); i++) {
		_add_glyph_range_item(unicode_ranges[i].start, unicode_ranges[i].end, unicode_ranges[i].name);
	}
	glyphs_split->add_child(glyph_tree);
	glyph_tree->connect("item_activated", callable_mp(this, &DynamicFontImportSettingsDialog::_range_edited));
	glyph_tree->connect(SceneStringName(item_selected), callable_mp(this, &DynamicFontImportSettingsDialog::_range_selected));

	// Common

	import_settings_data.instantiate();
	import_settings_data->owner = this;

	set_ok_button_text(TTR("Reimport"));
	set_cancel_button_text(TTR("Close"));
}

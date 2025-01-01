/**************************************************************************/
/*  theme.cpp                                                             */
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

#include "theme.h"

#include "scene/theme/theme_db.h"

// Dynamic properties.
bool Theme::_set(const StringName &p_name, const Variant &p_value) {
	String sname = p_name;

	if (sname.contains_char('/')) {
		String type = sname.get_slicec('/', 1);
		String theme_type = sname.get_slicec('/', 0);
		String prop_name = sname.get_slicec('/', 2);

		if (type == "icons") {
			set_icon(prop_name, theme_type, p_value);
		} else if (type == "styles") {
			set_stylebox(prop_name, theme_type, p_value);
		} else if (type == "fonts") {
			set_font(prop_name, theme_type, p_value);
		} else if (type == "font_sizes") {
			set_font_size(prop_name, theme_type, p_value);
		} else if (type == "colors") {
			set_color(prop_name, theme_type, p_value);
		} else if (type == "constants") {
			set_constant(prop_name, theme_type, p_value);
		} else if (type == "base_type") {
			set_type_variation(theme_type, p_value);
		} else {
			return false;
		}

		return true;
	}

	return false;
}

bool Theme::_get(const StringName &p_name, Variant &r_ret) const {
	String sname = p_name;

	if (sname.contains_char('/')) {
		String type = sname.get_slicec('/', 1);
		String theme_type = sname.get_slicec('/', 0);
		String prop_name = sname.get_slicec('/', 2);

		if (type == "icons") {
			if (!has_icon(prop_name, theme_type)) {
				r_ret = Ref<Texture2D>();
			} else {
				r_ret = get_icon(prop_name, theme_type);
			}
		} else if (type == "styles") {
			if (!has_stylebox(prop_name, theme_type)) {
				r_ret = Ref<StyleBox>();
			} else {
				r_ret = get_stylebox(prop_name, theme_type);
			}
		} else if (type == "fonts") {
			if (!has_font(prop_name, theme_type)) {
				r_ret = Ref<Font>();
			} else {
				r_ret = get_font(prop_name, theme_type);
			}
		} else if (type == "font_sizes") {
			r_ret = get_font_size(prop_name, theme_type);
		} else if (type == "colors") {
			r_ret = get_color(prop_name, theme_type);
		} else if (type == "constants") {
			r_ret = get_constant(prop_name, theme_type);
		} else if (type == "base_type") {
			r_ret = get_type_variation_base(theme_type);
		} else {
			return false;
		}

		return true;
	}

	return false;
}

void Theme::_get_property_list(List<PropertyInfo> *p_list) const {
	List<PropertyInfo> list;

	// Type variations.
	for (const KeyValue<StringName, StringName> &E : variation_map) {
		list.push_back(PropertyInfo(Variant::STRING_NAME, String() + E.key + "/base_type"));
	}

	// Icons.
	for (const KeyValue<StringName, ThemeIconMap> &E : icon_map) {
		for (const KeyValue<StringName, Ref<Texture2D>> &F : E.value) {
			list.push_back(PropertyInfo(Variant::OBJECT, String() + E.key + "/icons/" + F.key, PROPERTY_HINT_RESOURCE_TYPE, "Texture2D", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_STORE_IF_NULL));
		}
	}

	// Styles.
	for (const KeyValue<StringName, ThemeStyleMap> &E : style_map) {
		for (const KeyValue<StringName, Ref<StyleBox>> &F : E.value) {
			list.push_back(PropertyInfo(Variant::OBJECT, String() + E.key + "/styles/" + F.key, PROPERTY_HINT_RESOURCE_TYPE, "StyleBox", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_STORE_IF_NULL));
		}
	}

	// Fonts.
	for (const KeyValue<StringName, ThemeFontMap> &E : font_map) {
		for (const KeyValue<StringName, Ref<Font>> &F : E.value) {
			list.push_back(PropertyInfo(Variant::OBJECT, String() + E.key + "/fonts/" + F.key, PROPERTY_HINT_RESOURCE_TYPE, "Font", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_STORE_IF_NULL));
		}
	}

	// Font sizes.
	for (const KeyValue<StringName, ThemeFontSizeMap> &E : font_size_map) {
		for (const KeyValue<StringName, int> &F : E.value) {
			list.push_back(PropertyInfo(Variant::INT, String() + E.key + "/font_sizes/" + F.key, PROPERTY_HINT_RANGE, "0,256,1,or_greater,suffix:px"));
		}
	}

	// Colors.
	for (const KeyValue<StringName, ThemeColorMap> &E : color_map) {
		for (const KeyValue<StringName, Color> &F : E.value) {
			list.push_back(PropertyInfo(Variant::COLOR, String() + E.key + "/colors/" + F.key));
		}
	}

	// Constants.
	for (const KeyValue<StringName, ThemeConstantMap> &E : constant_map) {
		for (const KeyValue<StringName, int> &F : E.value) {
			list.push_back(PropertyInfo(Variant::INT, String() + E.key + "/constants/" + F.key));
		}
	}

	// Sort and store properties.
	list.sort();
	String prev_type;
	for (const PropertyInfo &E : list) {
		// Add groups for types so that their names are left unchanged in the inspector.
		String current_type = E.name.get_slice("/", 0);
		if (prev_type != current_type) {
			p_list->push_back(PropertyInfo(Variant::NIL, current_type, PROPERTY_HINT_NONE, current_type + "/", PROPERTY_USAGE_GROUP));
			prev_type = current_type;
		}

		p_list->push_back(E);
	}
}

// Static helpers.
bool Theme::is_valid_type_name(const String &p_name) {
	for (int i = 0; i < p_name.length(); i++) {
		if (!is_ascii_identifier_char(p_name[i])) {
			return false;
		}
	}
	return true;
}

bool Theme::is_valid_item_name(const String &p_name) {
	if (p_name.is_empty()) {
		return false;
	}
	for (int i = 0; i < p_name.length(); i++) {
		if (!is_ascii_identifier_char(p_name[i])) {
			return false;
		}
	}
	return true;
}

// Fallback values for theme item types, configurable per theme.
void Theme::set_default_base_scale(float p_base_scale) {
	if (default_base_scale == p_base_scale) {
		return;
	}

	default_base_scale = p_base_scale;

	_emit_theme_changed();
}

float Theme::get_default_base_scale() const {
	return default_base_scale;
}

bool Theme::has_default_base_scale() const {
	return default_base_scale > 0.0;
}

void Theme::set_default_font(const Ref<Font> &p_default_font) {
	if (default_font == p_default_font) {
		return;
	}

	if (default_font.is_valid()) {
		default_font->disconnect_changed(callable_mp(this, &Theme::_emit_theme_changed));
	}

	default_font = p_default_font;

	if (default_font.is_valid()) {
		default_font->connect_changed(callable_mp(this, &Theme::_emit_theme_changed).bind(false), CONNECT_REFERENCE_COUNTED);
	}

	_emit_theme_changed();
}

Ref<Font> Theme::get_default_font() const {
	return default_font;
}

bool Theme::has_default_font() const {
	return default_font.is_valid();
}

void Theme::set_default_font_size(int p_font_size) {
	if (default_font_size == p_font_size) {
		return;
	}

	default_font_size = p_font_size;

	_emit_theme_changed();
}

int Theme::get_default_font_size() const {
	return default_font_size;
}

bool Theme::has_default_font_size() const {
	return default_font_size > 0;
}

// Icons.
void Theme::set_icon(const StringName &p_name, const StringName &p_theme_type, const Ref<Texture2D> &p_icon) {
	ERR_FAIL_COND_MSG(!is_valid_item_name(p_name), vformat("Invalid item name: '%s'", p_name));
	ERR_FAIL_COND_MSG(!is_valid_type_name(p_theme_type), vformat("Invalid type name: '%s'", p_theme_type));

	bool existing = false;
	if (icon_map[p_theme_type].has(p_name) && icon_map[p_theme_type][p_name].is_valid()) {
		existing = true;
		icon_map[p_theme_type][p_name]->disconnect_changed(callable_mp(this, &Theme::_emit_theme_changed));
	}

	icon_map[p_theme_type][p_name] = p_icon;

	if (p_icon.is_valid()) {
		icon_map[p_theme_type][p_name]->connect_changed(callable_mp(this, &Theme::_emit_theme_changed).bind(false), CONNECT_REFERENCE_COUNTED);
	}

	_emit_theme_changed(!existing);
}

Ref<Texture2D> Theme::get_icon(const StringName &p_name, const StringName &p_theme_type) const {
	if (icon_map.has(p_theme_type) && icon_map[p_theme_type].has(p_name) && icon_map[p_theme_type][p_name].is_valid()) {
		return icon_map[p_theme_type][p_name];
	} else {
		return ThemeDB::get_singleton()->get_fallback_icon();
	}
}

bool Theme::has_icon(const StringName &p_name, const StringName &p_theme_type) const {
	return (icon_map.has(p_theme_type) && icon_map[p_theme_type].has(p_name) && icon_map[p_theme_type][p_name].is_valid());
}

bool Theme::has_icon_nocheck(const StringName &p_name, const StringName &p_theme_type) const {
	return (icon_map.has(p_theme_type) && icon_map[p_theme_type].has(p_name));
}

void Theme::rename_icon(const StringName &p_old_name, const StringName &p_name, const StringName &p_theme_type) {
	ERR_FAIL_COND_MSG(!is_valid_item_name(p_name), vformat("Invalid item name: '%s'", p_name));
	ERR_FAIL_COND_MSG(!is_valid_type_name(p_theme_type), vformat("Invalid type name: '%s'", p_theme_type));
	ERR_FAIL_COND_MSG(!icon_map.has(p_theme_type), "Cannot rename the icon '" + String(p_old_name) + "' because the node type '" + String(p_theme_type) + "' does not exist.");
	ERR_FAIL_COND_MSG(icon_map[p_theme_type].has(p_name), "Cannot rename the icon '" + String(p_old_name) + "' because the new name '" + String(p_name) + "' already exists.");
	ERR_FAIL_COND_MSG(!icon_map[p_theme_type].has(p_old_name), "Cannot rename the icon '" + String(p_old_name) + "' because it does not exist.");

	icon_map[p_theme_type][p_name] = icon_map[p_theme_type][p_old_name];
	icon_map[p_theme_type].erase(p_old_name);

	_emit_theme_changed(true);
}

void Theme::clear_icon(const StringName &p_name, const StringName &p_theme_type) {
	ERR_FAIL_COND_MSG(!icon_map.has(p_theme_type), "Cannot clear the icon '" + String(p_name) + "' because the node type '" + String(p_theme_type) + "' does not exist.");
	ERR_FAIL_COND_MSG(!icon_map[p_theme_type].has(p_name), "Cannot clear the icon '" + String(p_name) + "' because it does not exist.");

	if (icon_map[p_theme_type][p_name].is_valid()) {
		icon_map[p_theme_type][p_name]->disconnect_changed(callable_mp(this, &Theme::_emit_theme_changed));
	}

	icon_map[p_theme_type].erase(p_name);

	_emit_theme_changed(true);
}

void Theme::get_icon_list(const StringName &p_theme_type, List<StringName> *p_list) const {
	ERR_FAIL_NULL(p_list);

	if (!icon_map.has(p_theme_type)) {
		return;
	}

	for (const KeyValue<StringName, Ref<Texture2D>> &E : icon_map[p_theme_type]) {
		p_list->push_back(E.key);
	}
}

void Theme::add_icon_type(const StringName &p_theme_type) {
	ERR_FAIL_COND_MSG(!is_valid_type_name(p_theme_type), vformat("Invalid type name: '%s'", p_theme_type));

	if (icon_map.has(p_theme_type)) {
		return;
	}
	icon_map[p_theme_type] = ThemeIconMap();
}

void Theme::remove_icon_type(const StringName &p_theme_type) {
	if (!icon_map.has(p_theme_type)) {
		return;
	}

	_freeze_change_propagation();

	for (const KeyValue<StringName, Ref<Texture2D>> &E : icon_map[p_theme_type]) {
		Ref<Texture2D> icon = E.value;
		if (icon.is_valid()) {
			icon->disconnect_changed(callable_mp(this, &Theme::_emit_theme_changed));
		}
	}

	icon_map.erase(p_theme_type);

	_unfreeze_and_propagate_changes();
}

void Theme::get_icon_type_list(List<StringName> *p_list) const {
	ERR_FAIL_NULL(p_list);

	for (const KeyValue<StringName, ThemeIconMap> &E : icon_map) {
		p_list->push_back(E.key);
	}
}

// Styleboxes.
void Theme::set_stylebox(const StringName &p_name, const StringName &p_theme_type, const Ref<StyleBox> &p_style) {
	ERR_FAIL_COND_MSG(!is_valid_item_name(p_name), vformat("Invalid item name: '%s'", p_name));
	ERR_FAIL_COND_MSG(!is_valid_type_name(p_theme_type), vformat("Invalid type name: '%s'", p_theme_type));

	bool existing = false;
	if (style_map[p_theme_type].has(p_name) && style_map[p_theme_type][p_name].is_valid()) {
		existing = true;
		style_map[p_theme_type][p_name]->disconnect_changed(callable_mp(this, &Theme::_emit_theme_changed));
	}

	style_map[p_theme_type][p_name] = p_style;

	if (p_style.is_valid()) {
		style_map[p_theme_type][p_name]->connect_changed(callable_mp(this, &Theme::_emit_theme_changed).bind(false), CONNECT_REFERENCE_COUNTED);
	}

	_emit_theme_changed(!existing);
}

Ref<StyleBox> Theme::get_stylebox(const StringName &p_name, const StringName &p_theme_type) const {
	if (style_map.has(p_theme_type) && style_map[p_theme_type].has(p_name) && style_map[p_theme_type][p_name].is_valid()) {
		return style_map[p_theme_type][p_name];
	} else {
		return ThemeDB::get_singleton()->get_fallback_stylebox();
	}
}

bool Theme::has_stylebox(const StringName &p_name, const StringName &p_theme_type) const {
	return (style_map.has(p_theme_type) && style_map[p_theme_type].has(p_name) && style_map[p_theme_type][p_name].is_valid());
}

bool Theme::has_stylebox_nocheck(const StringName &p_name, const StringName &p_theme_type) const {
	return (style_map.has(p_theme_type) && style_map[p_theme_type].has(p_name));
}

void Theme::rename_stylebox(const StringName &p_old_name, const StringName &p_name, const StringName &p_theme_type) {
	ERR_FAIL_COND_MSG(!is_valid_item_name(p_name), vformat("Invalid item name: '%s'", p_name));
	ERR_FAIL_COND_MSG(!is_valid_type_name(p_theme_type), vformat("Invalid type name: '%s'", p_theme_type));
	ERR_FAIL_COND_MSG(!style_map.has(p_theme_type), "Cannot rename the stylebox '" + String(p_old_name) + "' because the node type '" + String(p_theme_type) + "' does not exist.");
	ERR_FAIL_COND_MSG(style_map[p_theme_type].has(p_name), "Cannot rename the stylebox '" + String(p_old_name) + "' because the new name '" + String(p_name) + "' already exists.");
	ERR_FAIL_COND_MSG(!style_map[p_theme_type].has(p_old_name), "Cannot rename the stylebox '" + String(p_old_name) + "' because it does not exist.");

	style_map[p_theme_type][p_name] = style_map[p_theme_type][p_old_name];
	style_map[p_theme_type].erase(p_old_name);

	_emit_theme_changed(true);
}

void Theme::clear_stylebox(const StringName &p_name, const StringName &p_theme_type) {
	ERR_FAIL_COND_MSG(!style_map.has(p_theme_type), "Cannot clear the stylebox '" + String(p_name) + "' because the node type '" + String(p_theme_type) + "' does not exist.");
	ERR_FAIL_COND_MSG(!style_map[p_theme_type].has(p_name), "Cannot clear the stylebox '" + String(p_name) + "' because it does not exist.");

	if (style_map[p_theme_type][p_name].is_valid()) {
		style_map[p_theme_type][p_name]->disconnect_changed(callable_mp(this, &Theme::_emit_theme_changed));
	}

	style_map[p_theme_type].erase(p_name);

	_emit_theme_changed(true);
}

void Theme::get_stylebox_list(const StringName &p_theme_type, List<StringName> *p_list) const {
	ERR_FAIL_NULL(p_list);

	if (!style_map.has(p_theme_type)) {
		return;
	}

	for (const KeyValue<StringName, Ref<StyleBox>> &E : style_map[p_theme_type]) {
		p_list->push_back(E.key);
	}
}

void Theme::add_stylebox_type(const StringName &p_theme_type) {
	ERR_FAIL_COND_MSG(!is_valid_type_name(p_theme_type), vformat("Invalid type name: '%s'", p_theme_type));

	if (style_map.has(p_theme_type)) {
		return;
	}
	style_map[p_theme_type] = ThemeStyleMap();
}

void Theme::remove_stylebox_type(const StringName &p_theme_type) {
	if (!style_map.has(p_theme_type)) {
		return;
	}

	_freeze_change_propagation();

	for (const KeyValue<StringName, Ref<StyleBox>> &E : style_map[p_theme_type]) {
		Ref<StyleBox> style = E.value;
		if (style.is_valid()) {
			style->disconnect_changed(callable_mp(this, &Theme::_emit_theme_changed));
		}
	}

	style_map.erase(p_theme_type);

	_unfreeze_and_propagate_changes();
}

void Theme::get_stylebox_type_list(List<StringName> *p_list) const {
	ERR_FAIL_NULL(p_list);

	for (const KeyValue<StringName, ThemeStyleMap> &E : style_map) {
		p_list->push_back(E.key);
	}
}

// Fonts.
void Theme::set_font(const StringName &p_name, const StringName &p_theme_type, const Ref<Font> &p_font) {
	ERR_FAIL_COND_MSG(!is_valid_item_name(p_name), vformat("Invalid item name: '%s'", p_name));
	ERR_FAIL_COND_MSG(!is_valid_type_name(p_theme_type), vformat("Invalid type name: '%s'", p_theme_type));

	bool existing = false;
	if (font_map[p_theme_type][p_name].is_valid()) {
		existing = true;
		font_map[p_theme_type][p_name]->disconnect_changed(callable_mp(this, &Theme::_emit_theme_changed));
	}

	font_map[p_theme_type][p_name] = p_font;

	if (p_font.is_valid()) {
		font_map[p_theme_type][p_name]->connect_changed(callable_mp(this, &Theme::_emit_theme_changed).bind(false), CONNECT_REFERENCE_COUNTED);
	}

	_emit_theme_changed(!existing);
}

Ref<Font> Theme::get_font(const StringName &p_name, const StringName &p_theme_type) const {
	if (font_map.has(p_theme_type) && font_map[p_theme_type].has(p_name) && font_map[p_theme_type][p_name].is_valid()) {
		return font_map[p_theme_type][p_name];
	} else if (has_default_font()) {
		return default_font;
	} else {
		return ThemeDB::get_singleton()->get_fallback_font();
	}
}

bool Theme::has_font(const StringName &p_name, const StringName &p_theme_type) const {
	return ((font_map.has(p_theme_type) && font_map[p_theme_type].has(p_name) && font_map[p_theme_type][p_name].is_valid()) || has_default_font());
}

bool Theme::has_font_nocheck(const StringName &p_name, const StringName &p_theme_type) const {
	return (font_map.has(p_theme_type) && font_map[p_theme_type].has(p_name));
}

void Theme::rename_font(const StringName &p_old_name, const StringName &p_name, const StringName &p_theme_type) {
	ERR_FAIL_COND_MSG(!is_valid_item_name(p_name), vformat("Invalid item name: '%s'", p_name));
	ERR_FAIL_COND_MSG(!is_valid_type_name(p_theme_type), vformat("Invalid type name: '%s'", p_theme_type));
	ERR_FAIL_COND_MSG(!font_map.has(p_theme_type), "Cannot rename the font '" + String(p_old_name) + "' because the node type '" + String(p_theme_type) + "' does not exist.");
	ERR_FAIL_COND_MSG(font_map[p_theme_type].has(p_name), "Cannot rename the font '" + String(p_old_name) + "' because the new name '" + String(p_name) + "' already exists.");
	ERR_FAIL_COND_MSG(!font_map[p_theme_type].has(p_old_name), "Cannot rename the font '" + String(p_old_name) + "' because it does not exist.");

	font_map[p_theme_type][p_name] = font_map[p_theme_type][p_old_name];
	font_map[p_theme_type].erase(p_old_name);

	_emit_theme_changed(true);
}

void Theme::clear_font(const StringName &p_name, const StringName &p_theme_type) {
	ERR_FAIL_COND_MSG(!font_map.has(p_theme_type), "Cannot clear the font '" + String(p_name) + "' because the node type '" + String(p_theme_type) + "' does not exist.");
	ERR_FAIL_COND_MSG(!font_map[p_theme_type].has(p_name), "Cannot clear the font '" + String(p_name) + "' because it does not exist.");

	if (font_map[p_theme_type][p_name].is_valid()) {
		font_map[p_theme_type][p_name]->disconnect_changed(callable_mp(this, &Theme::_emit_theme_changed));
	}

	font_map[p_theme_type].erase(p_name);

	_emit_theme_changed(true);
}

void Theme::get_font_list(const StringName &p_theme_type, List<StringName> *p_list) const {
	ERR_FAIL_NULL(p_list);

	if (!font_map.has(p_theme_type)) {
		return;
	}

	for (const KeyValue<StringName, Ref<Font>> &E : font_map[p_theme_type]) {
		p_list->push_back(E.key);
	}
}

void Theme::add_font_type(const StringName &p_theme_type) {
	ERR_FAIL_COND_MSG(!is_valid_type_name(p_theme_type), vformat("Invalid type name: '%s'", p_theme_type));

	if (font_map.has(p_theme_type)) {
		return;
	}
	font_map[p_theme_type] = ThemeFontMap();
}

void Theme::remove_font_type(const StringName &p_theme_type) {
	if (!font_map.has(p_theme_type)) {
		return;
	}

	_freeze_change_propagation();

	for (const KeyValue<StringName, Ref<Font>> &E : font_map[p_theme_type]) {
		Ref<Font> font = E.value;
		if (font.is_valid()) {
			font->disconnect_changed(callable_mp(this, &Theme::_emit_theme_changed));
		}
	}

	font_map.erase(p_theme_type);

	_unfreeze_and_propagate_changes();
}

void Theme::get_font_type_list(List<StringName> *p_list) const {
	ERR_FAIL_NULL(p_list);

	for (const KeyValue<StringName, ThemeFontMap> &E : font_map) {
		p_list->push_back(E.key);
	}
}

// Font sizes.
void Theme::set_font_size(const StringName &p_name, const StringName &p_theme_type, int p_font_size) {
	ERR_FAIL_COND_MSG(!is_valid_item_name(p_name), vformat("Invalid item name: '%s'", p_name));
	ERR_FAIL_COND_MSG(!is_valid_type_name(p_theme_type), vformat("Invalid type name: '%s'", p_theme_type));

	bool existing = has_font_size_nocheck(p_name, p_theme_type);
	font_size_map[p_theme_type][p_name] = p_font_size;

	_emit_theme_changed(!existing);
}

int Theme::get_font_size(const StringName &p_name, const StringName &p_theme_type) const {
	if (font_size_map.has(p_theme_type) && font_size_map[p_theme_type].has(p_name) && (font_size_map[p_theme_type][p_name] > 0)) {
		return font_size_map[p_theme_type][p_name];
	} else if (has_default_font_size()) {
		return default_font_size;
	} else {
		return ThemeDB::get_singleton()->get_fallback_font_size();
	}
}

bool Theme::has_font_size(const StringName &p_name, const StringName &p_theme_type) const {
	return ((font_size_map.has(p_theme_type) && font_size_map[p_theme_type].has(p_name) && (font_size_map[p_theme_type][p_name] > 0)) || has_default_font_size());
}

bool Theme::has_font_size_nocheck(const StringName &p_name, const StringName &p_theme_type) const {
	return (font_size_map.has(p_theme_type) && font_size_map[p_theme_type].has(p_name));
}

void Theme::rename_font_size(const StringName &p_old_name, const StringName &p_name, const StringName &p_theme_type) {
	ERR_FAIL_COND_MSG(!is_valid_item_name(p_name), vformat("Invalid item name: '%s'", p_name));
	ERR_FAIL_COND_MSG(!is_valid_type_name(p_theme_type), vformat("Invalid type name: '%s'", p_theme_type));
	ERR_FAIL_COND_MSG(!font_size_map.has(p_theme_type), "Cannot rename the font size '" + String(p_old_name) + "' because the node type '" + String(p_theme_type) + "' does not exist.");
	ERR_FAIL_COND_MSG(font_size_map[p_theme_type].has(p_name), "Cannot rename the font size '" + String(p_old_name) + "' because the new name '" + String(p_name) + "' already exists.");
	ERR_FAIL_COND_MSG(!font_size_map[p_theme_type].has(p_old_name), "Cannot rename the font size '" + String(p_old_name) + "' because it does not exist.");

	font_size_map[p_theme_type][p_name] = font_size_map[p_theme_type][p_old_name];
	font_size_map[p_theme_type].erase(p_old_name);

	_emit_theme_changed(true);
}

void Theme::clear_font_size(const StringName &p_name, const StringName &p_theme_type) {
	ERR_FAIL_COND_MSG(!font_size_map.has(p_theme_type), "Cannot clear the font size '" + String(p_name) + "' because the node type '" + String(p_theme_type) + "' does not exist.");
	ERR_FAIL_COND_MSG(!font_size_map[p_theme_type].has(p_name), "Cannot clear the font size '" + String(p_name) + "' because it does not exist.");

	font_size_map[p_theme_type].erase(p_name);

	_emit_theme_changed(true);
}

void Theme::get_font_size_list(const StringName &p_theme_type, List<StringName> *p_list) const {
	ERR_FAIL_NULL(p_list);

	if (!font_size_map.has(p_theme_type)) {
		return;
	}

	for (const KeyValue<StringName, int> &E : font_size_map[p_theme_type]) {
		p_list->push_back(E.key);
	}
}

void Theme::add_font_size_type(const StringName &p_theme_type) {
	ERR_FAIL_COND_MSG(!is_valid_type_name(p_theme_type), vformat("Invalid type name: '%s'", p_theme_type));

	if (font_size_map.has(p_theme_type)) {
		return;
	}
	font_size_map[p_theme_type] = ThemeFontSizeMap();
}

void Theme::remove_font_size_type(const StringName &p_theme_type) {
	if (!font_size_map.has(p_theme_type)) {
		return;
	}

	font_size_map.erase(p_theme_type);
}

void Theme::get_font_size_type_list(List<StringName> *p_list) const {
	ERR_FAIL_NULL(p_list);

	for (const KeyValue<StringName, ThemeFontSizeMap> &E : font_size_map) {
		p_list->push_back(E.key);
	}
}

// Colors.
void Theme::set_color(const StringName &p_name, const StringName &p_theme_type, const Color &p_color) {
	ERR_FAIL_COND_MSG(!is_valid_item_name(p_name), vformat("Invalid item name: '%s'", p_name));
	ERR_FAIL_COND_MSG(!is_valid_type_name(p_theme_type), vformat("Invalid type name: '%s'", p_theme_type));

	bool existing = has_color_nocheck(p_name, p_theme_type);
	color_map[p_theme_type][p_name] = p_color;

	_emit_theme_changed(!existing);
}

Color Theme::get_color(const StringName &p_name, const StringName &p_theme_type) const {
	if (color_map.has(p_theme_type) && color_map[p_theme_type].has(p_name)) {
		return color_map[p_theme_type][p_name];
	} else {
		return Color();
	}
}

bool Theme::has_color(const StringName &p_name, const StringName &p_theme_type) const {
	return (color_map.has(p_theme_type) && color_map[p_theme_type].has(p_name));
}

bool Theme::has_color_nocheck(const StringName &p_name, const StringName &p_theme_type) const {
	return (color_map.has(p_theme_type) && color_map[p_theme_type].has(p_name));
}

void Theme::rename_color(const StringName &p_old_name, const StringName &p_name, const StringName &p_theme_type) {
	ERR_FAIL_COND_MSG(!is_valid_item_name(p_name), vformat("Invalid item name: '%s'", p_name));
	ERR_FAIL_COND_MSG(!is_valid_type_name(p_theme_type), vformat("Invalid type name: '%s'", p_theme_type));
	ERR_FAIL_COND_MSG(!color_map.has(p_theme_type), "Cannot rename the color '" + String(p_old_name) + "' because the node type '" + String(p_theme_type) + "' does not exist.");
	ERR_FAIL_COND_MSG(color_map[p_theme_type].has(p_name), "Cannot rename the color '" + String(p_old_name) + "' because the new name '" + String(p_name) + "' already exists.");
	ERR_FAIL_COND_MSG(!color_map[p_theme_type].has(p_old_name), "Cannot rename the color '" + String(p_old_name) + "' because it does not exist.");

	color_map[p_theme_type][p_name] = color_map[p_theme_type][p_old_name];
	color_map[p_theme_type].erase(p_old_name);

	_emit_theme_changed(true);
}

void Theme::clear_color(const StringName &p_name, const StringName &p_theme_type) {
	ERR_FAIL_COND_MSG(!color_map.has(p_theme_type), "Cannot clear the color '" + String(p_name) + "' because the node type '" + String(p_theme_type) + "' does not exist.");
	ERR_FAIL_COND_MSG(!color_map[p_theme_type].has(p_name), "Cannot clear the color '" + String(p_name) + "' because it does not exist.");

	color_map[p_theme_type].erase(p_name);

	_emit_theme_changed(true);
}

void Theme::get_color_list(const StringName &p_theme_type, List<StringName> *p_list) const {
	ERR_FAIL_NULL(p_list);

	if (!color_map.has(p_theme_type)) {
		return;
	}

	for (const KeyValue<StringName, Color> &E : color_map[p_theme_type]) {
		p_list->push_back(E.key);
	}
}

void Theme::add_color_type(const StringName &p_theme_type) {
	ERR_FAIL_COND_MSG(!is_valid_type_name(p_theme_type), vformat("Invalid type name: '%s'", p_theme_type));

	if (color_map.has(p_theme_type)) {
		return;
	}
	color_map[p_theme_type] = ThemeColorMap();
}

void Theme::remove_color_type(const StringName &p_theme_type) {
	if (!color_map.has(p_theme_type)) {
		return;
	}

	color_map.erase(p_theme_type);
}

void Theme::get_color_type_list(List<StringName> *p_list) const {
	ERR_FAIL_NULL(p_list);

	for (const KeyValue<StringName, ThemeColorMap> &E : color_map) {
		p_list->push_back(E.key);
	}
}

// Theme constants.
void Theme::set_constant(const StringName &p_name, const StringName &p_theme_type, int p_constant) {
	ERR_FAIL_COND_MSG(!is_valid_item_name(p_name), vformat("Invalid item name: '%s'", p_name));
	ERR_FAIL_COND_MSG(!is_valid_type_name(p_theme_type), vformat("Invalid type name: '%s'", p_theme_type));

	bool existing = has_constant_nocheck(p_name, p_theme_type);
	constant_map[p_theme_type][p_name] = p_constant;

	_emit_theme_changed(!existing);
}

int Theme::get_constant(const StringName &p_name, const StringName &p_theme_type) const {
	if (constant_map.has(p_theme_type) && constant_map[p_theme_type].has(p_name)) {
		return constant_map[p_theme_type][p_name];
	} else {
		return 0;
	}
}

bool Theme::has_constant(const StringName &p_name, const StringName &p_theme_type) const {
	return (constant_map.has(p_theme_type) && constant_map[p_theme_type].has(p_name));
}

bool Theme::has_constant_nocheck(const StringName &p_name, const StringName &p_theme_type) const {
	return (constant_map.has(p_theme_type) && constant_map[p_theme_type].has(p_name));
}

void Theme::rename_constant(const StringName &p_old_name, const StringName &p_name, const StringName &p_theme_type) {
	ERR_FAIL_COND_MSG(!is_valid_item_name(p_name), vformat("Invalid item name: '%s'", p_name));
	ERR_FAIL_COND_MSG(!is_valid_type_name(p_theme_type), vformat("Invalid type name: '%s'", p_theme_type));
	ERR_FAIL_COND_MSG(!constant_map.has(p_theme_type), "Cannot rename the constant '" + String(p_old_name) + "' because the node type '" + String(p_theme_type) + "' does not exist.");
	ERR_FAIL_COND_MSG(constant_map[p_theme_type].has(p_name), "Cannot rename the constant '" + String(p_old_name) + "' because the new name '" + String(p_name) + "' already exists.");
	ERR_FAIL_COND_MSG(!constant_map[p_theme_type].has(p_old_name), "Cannot rename the constant '" + String(p_old_name) + "' because it does not exist.");

	constant_map[p_theme_type][p_name] = constant_map[p_theme_type][p_old_name];
	constant_map[p_theme_type].erase(p_old_name);

	_emit_theme_changed(true);
}

void Theme::clear_constant(const StringName &p_name, const StringName &p_theme_type) {
	ERR_FAIL_COND_MSG(!constant_map.has(p_theme_type), "Cannot clear the constant '" + String(p_name) + "' because the node type '" + String(p_theme_type) + "' does not exist.");
	ERR_FAIL_COND_MSG(!constant_map[p_theme_type].has(p_name), "Cannot clear the constant '" + String(p_name) + "' because it does not exist.");

	constant_map[p_theme_type].erase(p_name);

	_emit_theme_changed(true);
}

void Theme::get_constant_list(const StringName &p_theme_type, List<StringName> *p_list) const {
	ERR_FAIL_NULL(p_list);

	if (!constant_map.has(p_theme_type)) {
		return;
	}

	for (const KeyValue<StringName, int> &E : constant_map[p_theme_type]) {
		p_list->push_back(E.key);
	}
}

void Theme::add_constant_type(const StringName &p_theme_type) {
	ERR_FAIL_COND_MSG(!is_valid_type_name(p_theme_type), vformat("Invalid type name: '%s'", p_theme_type));

	if (constant_map.has(p_theme_type)) {
		return;
	}
	constant_map[p_theme_type] = ThemeConstantMap();
}

void Theme::remove_constant_type(const StringName &p_theme_type) {
	if (!constant_map.has(p_theme_type)) {
		return;
	}

	constant_map.erase(p_theme_type);
}

void Theme::get_constant_type_list(List<StringName> *p_list) const {
	ERR_FAIL_NULL(p_list);

	for (const KeyValue<StringName, ThemeConstantMap> &E : constant_map) {
		p_list->push_back(E.key);
	}
}

// Generic methods for managing theme items.
void Theme::set_theme_item(DataType p_data_type, const StringName &p_name, const StringName &p_theme_type, const Variant &p_value) {
	switch (p_data_type) {
		case DATA_TYPE_COLOR: {
			ERR_FAIL_COND_MSG(p_value.get_type() != Variant::COLOR, "Theme item's data type (Color) does not match Variant's type (" + Variant::get_type_name(p_value.get_type()) + ").");

			Color color_value = p_value;
			set_color(p_name, p_theme_type, color_value);
		} break;
		case DATA_TYPE_CONSTANT: {
			ERR_FAIL_COND_MSG(p_value.get_type() != Variant::INT, "Theme item's data type (int) does not match Variant's type (" + Variant::get_type_name(p_value.get_type()) + ").");

			int constant_value = p_value;
			set_constant(p_name, p_theme_type, constant_value);
		} break;
		case DATA_TYPE_FONT: {
			ERR_FAIL_COND_MSG(p_value.get_type() != Variant::OBJECT, "Theme item's data type (Object) does not match Variant's type (" + Variant::get_type_name(p_value.get_type()) + ").");

			Ref<Font> font_value = Object::cast_to<Font>(p_value.get_validated_object());
			set_font(p_name, p_theme_type, font_value);
		} break;
		case DATA_TYPE_FONT_SIZE: {
			ERR_FAIL_COND_MSG(p_value.get_type() != Variant::INT, "Theme item's data type (int) does not match Variant's type (" + Variant::get_type_name(p_value.get_type()) + ").");

			int font_size_value = p_value;
			set_font_size(p_name, p_theme_type, font_size_value);
		} break;
		case DATA_TYPE_ICON: {
			ERR_FAIL_COND_MSG(p_value.get_type() != Variant::OBJECT, "Theme item's data type (Object) does not match Variant's type (" + Variant::get_type_name(p_value.get_type()) + ").");

			Ref<Texture2D> icon_value = Object::cast_to<Texture2D>(p_value.get_validated_object());
			set_icon(p_name, p_theme_type, icon_value);
		} break;
		case DATA_TYPE_STYLEBOX: {
			ERR_FAIL_COND_MSG(p_value.get_type() != Variant::OBJECT, "Theme item's data type (Object) does not match Variant's type (" + Variant::get_type_name(p_value.get_type()) + ").");

			Ref<StyleBox> stylebox_value = Object::cast_to<StyleBox>(p_value.get_validated_object());
			set_stylebox(p_name, p_theme_type, stylebox_value);
		} break;
		case DATA_TYPE_MAX:
			break; // Can't happen, but silences warning.
	}
}

Variant Theme::get_theme_item(DataType p_data_type, const StringName &p_name, const StringName &p_theme_type) const {
	switch (p_data_type) {
		case DATA_TYPE_COLOR:
			return get_color(p_name, p_theme_type);
		case DATA_TYPE_CONSTANT:
			return get_constant(p_name, p_theme_type);
		case DATA_TYPE_FONT:
			return get_font(p_name, p_theme_type);
		case DATA_TYPE_FONT_SIZE:
			return get_font_size(p_name, p_theme_type);
		case DATA_TYPE_ICON:
			return get_icon(p_name, p_theme_type);
		case DATA_TYPE_STYLEBOX:
			return get_stylebox(p_name, p_theme_type);
		case DATA_TYPE_MAX:
			break; // Can't happen, but silences warning.
	}

	return Variant();
}

bool Theme::has_theme_item(DataType p_data_type, const StringName &p_name, const StringName &p_theme_type) const {
	switch (p_data_type) {
		case DATA_TYPE_COLOR:
			return has_color(p_name, p_theme_type);
		case DATA_TYPE_CONSTANT:
			return has_constant(p_name, p_theme_type);
		case DATA_TYPE_FONT:
			return has_font(p_name, p_theme_type);
		case DATA_TYPE_FONT_SIZE:
			return has_font_size(p_name, p_theme_type);
		case DATA_TYPE_ICON:
			return has_icon(p_name, p_theme_type);
		case DATA_TYPE_STYLEBOX:
			return has_stylebox(p_name, p_theme_type);
		case DATA_TYPE_MAX:
			break; // Can't happen, but silences warning.
	}

	return false;
}

bool Theme::has_theme_item_nocheck(DataType p_data_type, const StringName &p_name, const StringName &p_theme_type) const {
	switch (p_data_type) {
		case DATA_TYPE_COLOR:
			return has_color_nocheck(p_name, p_theme_type);
		case DATA_TYPE_CONSTANT:
			return has_constant_nocheck(p_name, p_theme_type);
		case DATA_TYPE_FONT:
			return has_font_nocheck(p_name, p_theme_type);
		case DATA_TYPE_FONT_SIZE:
			return has_font_size_nocheck(p_name, p_theme_type);
		case DATA_TYPE_ICON:
			return has_icon_nocheck(p_name, p_theme_type);
		case DATA_TYPE_STYLEBOX:
			return has_stylebox_nocheck(p_name, p_theme_type);
		case DATA_TYPE_MAX:
			break; // Can't happen, but silences warning.
	}

	return false;
}

void Theme::rename_theme_item(DataType p_data_type, const StringName &p_old_name, const StringName &p_name, const StringName &p_theme_type) {
	switch (p_data_type) {
		case DATA_TYPE_COLOR:
			rename_color(p_old_name, p_name, p_theme_type);
			break;
		case DATA_TYPE_CONSTANT:
			rename_constant(p_old_name, p_name, p_theme_type);
			break;
		case DATA_TYPE_FONT:
			rename_font(p_old_name, p_name, p_theme_type);
			break;
		case DATA_TYPE_FONT_SIZE:
			rename_font_size(p_old_name, p_name, p_theme_type);
			break;
		case DATA_TYPE_ICON:
			rename_icon(p_old_name, p_name, p_theme_type);
			break;
		case DATA_TYPE_STYLEBOX:
			rename_stylebox(p_old_name, p_name, p_theme_type);
			break;
		case DATA_TYPE_MAX:
			break; // Can't happen, but silences warning.
	}
}

void Theme::clear_theme_item(DataType p_data_type, const StringName &p_name, const StringName &p_theme_type) {
	switch (p_data_type) {
		case DATA_TYPE_COLOR:
			clear_color(p_name, p_theme_type);
			break;
		case DATA_TYPE_CONSTANT:
			clear_constant(p_name, p_theme_type);
			break;
		case DATA_TYPE_FONT:
			clear_font(p_name, p_theme_type);
			break;
		case DATA_TYPE_FONT_SIZE:
			clear_font_size(p_name, p_theme_type);
			break;
		case DATA_TYPE_ICON:
			clear_icon(p_name, p_theme_type);
			break;
		case DATA_TYPE_STYLEBOX:
			clear_stylebox(p_name, p_theme_type);
			break;
		case DATA_TYPE_MAX:
			break; // Can't happen, but silences warning.
	}
}

void Theme::get_theme_item_list(DataType p_data_type, const StringName &p_theme_type, List<StringName> *p_list) const {
	switch (p_data_type) {
		case DATA_TYPE_COLOR:
			get_color_list(p_theme_type, p_list);
			break;
		case DATA_TYPE_CONSTANT:
			get_constant_list(p_theme_type, p_list);
			break;
		case DATA_TYPE_FONT:
			get_font_list(p_theme_type, p_list);
			break;
		case DATA_TYPE_FONT_SIZE:
			get_font_size_list(p_theme_type, p_list);
			break;
		case DATA_TYPE_ICON:
			get_icon_list(p_theme_type, p_list);
			break;
		case DATA_TYPE_STYLEBOX:
			get_stylebox_list(p_theme_type, p_list);
			break;
		case DATA_TYPE_MAX:
			break; // Can't happen, but silences warning.
	}
}

void Theme::add_theme_item_type(DataType p_data_type, const StringName &p_theme_type) {
	switch (p_data_type) {
		case DATA_TYPE_COLOR:
			add_color_type(p_theme_type);
			break;
		case DATA_TYPE_CONSTANT:
			add_constant_type(p_theme_type);
			break;
		case DATA_TYPE_FONT:
			add_font_type(p_theme_type);
			break;
		case DATA_TYPE_FONT_SIZE:
			add_font_size_type(p_theme_type);
			break;
		case DATA_TYPE_ICON:
			add_icon_type(p_theme_type);
			break;
		case DATA_TYPE_STYLEBOX:
			add_stylebox_type(p_theme_type);
			break;
		case DATA_TYPE_MAX:
			break; // Can't happen, but silences warning.
	}
}

void Theme::remove_theme_item_type(DataType p_data_type, const StringName &p_theme_type) {
	switch (p_data_type) {
		case DATA_TYPE_COLOR:
			remove_color_type(p_theme_type);
			break;
		case DATA_TYPE_CONSTANT:
			remove_constant_type(p_theme_type);
			break;
		case DATA_TYPE_FONT:
			remove_font_type(p_theme_type);
			break;
		case DATA_TYPE_FONT_SIZE:
			remove_font_size_type(p_theme_type);
			break;
		case DATA_TYPE_ICON:
			remove_icon_type(p_theme_type);
			break;
		case DATA_TYPE_STYLEBOX:
			remove_stylebox_type(p_theme_type);
			break;
		case DATA_TYPE_MAX:
			break; // Can't happen, but silences warning.
	}
}

void Theme::get_theme_item_type_list(DataType p_data_type, List<StringName> *p_list) const {
	switch (p_data_type) {
		case DATA_TYPE_COLOR:
			get_color_type_list(p_list);
			break;
		case DATA_TYPE_CONSTANT:
			get_constant_type_list(p_list);
			break;
		case DATA_TYPE_FONT:
			get_font_type_list(p_list);
			break;
		case DATA_TYPE_FONT_SIZE:
			get_font_size_type_list(p_list);
			break;
		case DATA_TYPE_ICON:
			get_icon_type_list(p_list);
			break;
		case DATA_TYPE_STYLEBOX:
			get_stylebox_type_list(p_list);
			break;
		case DATA_TYPE_MAX:
			break; // Can't happen, but silences warning.
	}
}

// Theme type variations.
void Theme::set_type_variation(const StringName &p_theme_type, const StringName &p_base_type) {
	ERR_FAIL_COND_MSG(!is_valid_type_name(p_theme_type), vformat("Invalid type name: '%s'", p_theme_type));
	ERR_FAIL_COND_MSG(!is_valid_type_name(p_base_type), vformat("Invalid type name: '%s'", p_base_type));
	ERR_FAIL_COND_MSG(p_theme_type == StringName(), "An empty theme type cannot be marked as a variation of another type.");
	ERR_FAIL_COND_MSG(ClassDB::class_exists(p_theme_type), "A type associated with a built-in class cannot be marked as a variation of another type.");
	ERR_FAIL_COND_MSG(p_base_type == StringName(), "An empty theme type cannot be the base type of a variation. Use clear_type_variation() instead if you want to unmark '" + String(p_theme_type) + "' as a variation.");

	if (variation_map.has(p_theme_type)) {
		StringName old_base = variation_map[p_theme_type];
		variation_base_map[old_base].erase(p_theme_type);
	}

	variation_map[p_theme_type] = p_base_type;
	variation_base_map[p_base_type].push_back(p_theme_type);

	_emit_theme_changed(true);
}

bool Theme::is_type_variation(const StringName &p_theme_type, const StringName &p_base_type) const {
	return (variation_map.has(p_theme_type) && variation_map[p_theme_type] == p_base_type);
}

void Theme::clear_type_variation(const StringName &p_theme_type) {
	ERR_FAIL_COND_MSG(!variation_map.has(p_theme_type), "Cannot clear the type variation '" + String(p_theme_type) + "' because it does not exist.");

	StringName base_type = variation_map[p_theme_type];
	variation_base_map[base_type].erase(p_theme_type);
	variation_map.erase(p_theme_type);

	_emit_theme_changed(true);
}

StringName Theme::get_type_variation_base(const StringName &p_theme_type) const {
	if (!variation_map.has(p_theme_type)) {
		return StringName();
	}

	return variation_map[p_theme_type];
}

void Theme::get_type_variation_list(const StringName &p_base_type, List<StringName> *p_list) const {
	ERR_FAIL_NULL(p_list);

	if (!variation_base_map.has(p_base_type)) {
		return;
	}

	for (const StringName &E : variation_base_map[p_base_type]) {
		// Prevent infinite loops if variants were set to be cross-dependent (that's still invalid usage, but handling for stability sake).
		if (p_list->find(E)) {
			continue;
		}

		p_list->push_back(E);
		// Continue looking for sub-variations.
		get_type_variation_list(E, p_list);
	}
}

// Theme types.
void Theme::add_type(const StringName &p_theme_type) {
	// Add a record to every data type map.
	for (int i = 0; i < Theme::DATA_TYPE_MAX; i++) {
		Theme::DataType dt = (Theme::DataType)i;
		add_theme_item_type(dt, p_theme_type);
	}

	_emit_theme_changed(true);
}

void Theme::remove_type(const StringName &p_theme_type) {
	// Gracefully remove the record from every data type map.
	for (int i = 0; i < Theme::DATA_TYPE_MAX; i++) {
		Theme::DataType dt = (Theme::DataType)i;
		remove_theme_item_type(dt, p_theme_type);
	}

	// If type is a variation, remove that connection.
	if (get_type_variation_base(p_theme_type) != StringName()) {
		clear_type_variation(p_theme_type);
	}

	// If type is a variation base, remove all those connections.
	List<StringName> names;
	get_type_variation_list(p_theme_type, &names);
	for (const StringName &E : names) {
		clear_type_variation(E);
	}

	_emit_theme_changed(true);
}

void Theme::get_type_list(List<StringName> *p_list) const {
	ERR_FAIL_NULL(p_list);

	// This Set guarantees uniqueness.
	// Because each map can have the same type defined, but for this method
	// we only want one occurrence of each type.
	HashSet<StringName> types;

	// Icons.
	for (const KeyValue<StringName, ThemeIconMap> &E : icon_map) {
		types.insert(E.key);
	}

	// Styles.
	for (const KeyValue<StringName, ThemeStyleMap> &E : style_map) {
		types.insert(E.key);
	}

	// Fonts.
	for (const KeyValue<StringName, ThemeFontMap> &E : font_map) {
		types.insert(E.key);
	}

	// Font sizes.
	for (const KeyValue<StringName, ThemeFontSizeMap> &E : font_size_map) {
		types.insert(E.key);
	}

	// Colors.
	for (const KeyValue<StringName, ThemeColorMap> &E : color_map) {
		types.insert(E.key);
	}

	// Constants.
	for (const KeyValue<StringName, ThemeConstantMap> &E : constant_map) {
		types.insert(E.key);
	}

	// Variations.
	for (const KeyValue<StringName, StringName> &E : variation_map) {
		types.insert(E.key);
	}

	for (const StringName &E : types) {
		p_list->push_back(E);
	}
}

void Theme::get_type_dependencies(const StringName &p_base_type, const StringName &p_type_variation, Vector<StringName> &r_result) {
	// Build the dependency chain for type variations.
	if (p_type_variation != StringName()) {
		StringName variation_name = p_type_variation;
		while (variation_name != StringName()) {
			r_result.push_back(variation_name);
			variation_name = get_type_variation_base(variation_name);

			// If we have reached the base type dependency, it's safe to stop (assuming no funny business was done to the Theme).
			if (variation_name == p_base_type) {
				break;
			}
		}
	}

	// Continue building the chain using native class hierarchy.
	ThemeDB::get_singleton()->get_native_type_dependencies(p_base_type, r_result);
}

// Internal methods for getting lists as a Vector of String (compatible with public API).
Vector<String> Theme::_get_icon_list(const String &p_theme_type) const {
	Vector<String> ilret;
	List<StringName> il;

	get_icon_list(p_theme_type, &il);
	ilret.resize(il.size());

	int i = 0;
	String *w = ilret.ptrw();
	for (List<StringName>::Element *E = il.front(); E; E = E->next(), i++) {
		w[i] = E->get();
	}
	return ilret;
}

Vector<String> Theme::_get_icon_type_list() const {
	Vector<String> ilret;
	List<StringName> il;

	get_icon_type_list(&il);
	ilret.resize(il.size());

	int i = 0;
	String *w = ilret.ptrw();
	for (List<StringName>::Element *E = il.front(); E; E = E->next(), i++) {
		w[i] = E->get();
	}
	return ilret;
}

Vector<String> Theme::_get_stylebox_list(const String &p_theme_type) const {
	Vector<String> ilret;
	List<StringName> il;

	get_stylebox_list(p_theme_type, &il);
	ilret.resize(il.size());

	int i = 0;
	String *w = ilret.ptrw();
	for (List<StringName>::Element *E = il.front(); E; E = E->next(), i++) {
		w[i] = E->get();
	}
	return ilret;
}

Vector<String> Theme::_get_stylebox_type_list() const {
	Vector<String> ilret;
	List<StringName> il;

	get_stylebox_type_list(&il);
	ilret.resize(il.size());

	int i = 0;
	String *w = ilret.ptrw();
	for (List<StringName>::Element *E = il.front(); E; E = E->next(), i++) {
		w[i] = E->get();
	}
	return ilret;
}

Vector<String> Theme::_get_font_list(const String &p_theme_type) const {
	Vector<String> ilret;
	List<StringName> il;

	get_font_list(p_theme_type, &il);
	ilret.resize(il.size());

	int i = 0;
	String *w = ilret.ptrw();
	for (List<StringName>::Element *E = il.front(); E; E = E->next(), i++) {
		w[i] = E->get();
	}
	return ilret;
}

Vector<String> Theme::_get_font_type_list() const {
	Vector<String> ilret;
	List<StringName> il;

	get_font_type_list(&il);
	ilret.resize(il.size());

	int i = 0;
	String *w = ilret.ptrw();
	for (List<StringName>::Element *E = il.front(); E; E = E->next(), i++) {
		w[i] = E->get();
	}
	return ilret;
}

Vector<String> Theme::_get_font_size_list(const String &p_theme_type) const {
	Vector<String> ilret;
	List<StringName> il;

	get_font_size_list(p_theme_type, &il);
	ilret.resize(il.size());

	int i = 0;
	String *w = ilret.ptrw();
	for (List<StringName>::Element *E = il.front(); E; E = E->next(), i++) {
		w[i] = E->get();
	}
	return ilret;
}

Vector<String> Theme::_get_font_size_type_list() const {
	Vector<String> ilret;
	List<StringName> il;

	get_font_size_type_list(&il);
	ilret.resize(il.size());

	int i = 0;
	String *w = ilret.ptrw();
	for (List<StringName>::Element *E = il.front(); E; E = E->next(), i++) {
		w[i] = E->get();
	}
	return ilret;
}

Vector<String> Theme::_get_color_list(const String &p_theme_type) const {
	Vector<String> ilret;
	List<StringName> il;

	get_color_list(p_theme_type, &il);
	ilret.resize(il.size());

	int i = 0;
	String *w = ilret.ptrw();
	for (List<StringName>::Element *E = il.front(); E; E = E->next(), i++) {
		w[i] = E->get();
	}
	return ilret;
}

Vector<String> Theme::_get_color_type_list() const {
	Vector<String> ilret;
	List<StringName> il;

	get_color_type_list(&il);
	ilret.resize(il.size());

	int i = 0;
	String *w = ilret.ptrw();
	for (List<StringName>::Element *E = il.front(); E; E = E->next(), i++) {
		w[i] = E->get();
	}
	return ilret;
}

Vector<String> Theme::_get_constant_list(const String &p_theme_type) const {
	Vector<String> ilret;
	List<StringName> il;

	get_constant_list(p_theme_type, &il);
	ilret.resize(il.size());

	int i = 0;
	String *w = ilret.ptrw();
	for (List<StringName>::Element *E = il.front(); E; E = E->next(), i++) {
		w[i] = E->get();
	}
	return ilret;
}

Vector<String> Theme::_get_constant_type_list() const {
	Vector<String> ilret;
	List<StringName> il;

	get_constant_type_list(&il);
	ilret.resize(il.size());

	int i = 0;
	String *w = ilret.ptrw();
	for (List<StringName>::Element *E = il.front(); E; E = E->next(), i++) {
		w[i] = E->get();
	}
	return ilret;
}

Vector<String> Theme::_get_theme_item_list(DataType p_data_type, const String &p_theme_type) const {
	switch (p_data_type) {
		case DATA_TYPE_COLOR:
			return _get_color_list(p_theme_type);
		case DATA_TYPE_CONSTANT:
			return _get_constant_list(p_theme_type);
		case DATA_TYPE_FONT:
			return _get_font_list(p_theme_type);
		case DATA_TYPE_FONT_SIZE:
			return _get_font_size_list(p_theme_type);
		case DATA_TYPE_ICON:
			return _get_icon_list(p_theme_type);
		case DATA_TYPE_STYLEBOX:
			return _get_stylebox_list(p_theme_type);
		case DATA_TYPE_MAX:
			break; // Can't happen, but silences warning.
	}

	return Vector<String>();
}

Vector<String> Theme::_get_theme_item_type_list(DataType p_data_type) const {
	switch (p_data_type) {
		case DATA_TYPE_COLOR:
			return _get_color_type_list();
		case DATA_TYPE_CONSTANT:
			return _get_constant_type_list();
		case DATA_TYPE_FONT:
			return _get_font_type_list();
		case DATA_TYPE_FONT_SIZE:
			return _get_font_size_type_list();
		case DATA_TYPE_ICON:
			return _get_icon_type_list();
		case DATA_TYPE_STYLEBOX:
			return _get_stylebox_type_list();
		case DATA_TYPE_MAX:
			break; // Can't happen, but silences warning.
	}

	return Vector<String>();
}

Vector<String> Theme::_get_type_variation_list(const StringName &p_theme_type) const {
	Vector<String> ilret;
	List<StringName> il;

	get_type_variation_list(p_theme_type, &il);
	ilret.resize(il.size());

	int i = 0;
	String *w = ilret.ptrw();
	for (List<StringName>::Element *E = il.front(); E; E = E->next(), i++) {
		w[i] = E->get();
	}
	return ilret;
}

Vector<String> Theme::_get_type_list() const {
	Vector<String> ilret;
	List<StringName> il;

	get_type_list(&il);
	ilret.resize(il.size());

	int i = 0;
	String *w = ilret.ptrw();
	for (List<StringName>::Element *E = il.front(); E; E = E->next(), i++) {
		w[i] = E->get();
	}
	return ilret;
}

// Theme bulk manipulations.
void Theme::_emit_theme_changed(bool p_notify_list_changed) {
	if (no_change_propagation) {
		return;
	}

	if (p_notify_list_changed) {
		notify_property_list_changed();
	}
	emit_changed();
}

void Theme::_freeze_change_propagation() {
	no_change_propagation = true;
}

void Theme::_unfreeze_and_propagate_changes() {
	no_change_propagation = false;
	_emit_theme_changed(true);
}

void Theme::merge_with(const Ref<Theme> &p_other) {
	if (p_other.is_null()) {
		return;
	}

	_freeze_change_propagation();

	// Colors.
	{
		for (const KeyValue<StringName, ThemeColorMap> &E : p_other->color_map) {
			for (const KeyValue<StringName, Color> &F : E.value) {
				set_color(F.key, E.key, F.value);
			}
		}
	}

	// Constants.
	{
		for (const KeyValue<StringName, ThemeConstantMap> &E : p_other->constant_map) {
			for (const KeyValue<StringName, int> &F : E.value) {
				set_constant(F.key, E.key, F.value);
			}
		}
	}

	// Fonts.
	{
		for (const KeyValue<StringName, ThemeFontMap> &E : p_other->font_map) {
			for (const KeyValue<StringName, Ref<Font>> &F : E.value) {
				set_font(F.key, E.key, F.value);
			}
		}
	}

	// Font sizes.
	{
		for (const KeyValue<StringName, ThemeFontSizeMap> &E : p_other->font_size_map) {
			for (const KeyValue<StringName, int> &F : E.value) {
				set_font_size(F.key, E.key, F.value);
			}
		}
	}

	// Icons.
	{
		for (const KeyValue<StringName, ThemeIconMap> &E : p_other->icon_map) {
			for (const KeyValue<StringName, Ref<Texture2D>> &F : E.value) {
				set_icon(F.key, E.key, F.value);
			}
		}
	}

	// Styleboxes.
	{
		for (const KeyValue<StringName, ThemeStyleMap> &E : p_other->style_map) {
			for (const KeyValue<StringName, Ref<StyleBox>> &F : E.value) {
				set_stylebox(F.key, E.key, F.value);
			}
		}
	}

	// Type variations.
	{
		for (const KeyValue<StringName, StringName> &E : p_other->variation_map) {
			set_type_variation(E.key, E.value);
		}
	}

	// Defaults.
	if (p_other->has_default_font()) {
		set_default_font(p_other->default_font);
	}
	if (p_other->has_default_font_size()) {
		set_default_font_size(p_other->default_font_size);
	}
	if (p_other->has_default_base_scale()) {
		set_default_base_scale(p_other->default_base_scale);
	}

	_unfreeze_and_propagate_changes();
}

void Theme::clear() {
	// These items need disconnecting.
	{
		for (const KeyValue<StringName, ThemeIconMap> &E : icon_map) {
			for (const KeyValue<StringName, Ref<Texture2D>> &F : E.value) {
				if (F.value.is_valid()) {
					Ref<Texture2D> icon = F.value;
					icon->disconnect_changed(callable_mp(this, &Theme::_emit_theme_changed));
				}
			}
		}
	}

	{
		for (const KeyValue<StringName, ThemeStyleMap> &E : style_map) {
			for (const KeyValue<StringName, Ref<StyleBox>> &F : E.value) {
				if (F.value.is_valid()) {
					Ref<StyleBox> style = F.value;
					style->disconnect_changed(callable_mp(this, &Theme::_emit_theme_changed));
				}
			}
		}
	}

	{
		for (const KeyValue<StringName, ThemeFontMap> &E : font_map) {
			for (const KeyValue<StringName, Ref<Font>> &F : E.value) {
				if (F.value.is_valid()) {
					Ref<Font> font = F.value;
					font->disconnect_changed(callable_mp(this, &Theme::_emit_theme_changed));
				}
			}
		}
	}

	icon_map.clear();
	style_map.clear();
	font_map.clear();
	font_size_map.clear();
	color_map.clear();
	constant_map.clear();

	variation_map.clear();
	variation_base_map.clear();

	_emit_theme_changed(true);
}

void Theme::reset_state() {
	clear();
}

void Theme::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_icon", "name", "theme_type", "texture"), &Theme::set_icon);
	ClassDB::bind_method(D_METHOD("get_icon", "name", "theme_type"), &Theme::get_icon);
	ClassDB::bind_method(D_METHOD("has_icon", "name", "theme_type"), &Theme::has_icon);
	ClassDB::bind_method(D_METHOD("rename_icon", "old_name", "name", "theme_type"), &Theme::rename_icon);
	ClassDB::bind_method(D_METHOD("clear_icon", "name", "theme_type"), &Theme::clear_icon);
	ClassDB::bind_method(D_METHOD("get_icon_list", "theme_type"), &Theme::_get_icon_list);
	ClassDB::bind_method(D_METHOD("get_icon_type_list"), &Theme::_get_icon_type_list);

	ClassDB::bind_method(D_METHOD("set_stylebox", "name", "theme_type", "texture"), &Theme::set_stylebox);
	ClassDB::bind_method(D_METHOD("get_stylebox", "name", "theme_type"), &Theme::get_stylebox);
	ClassDB::bind_method(D_METHOD("has_stylebox", "name", "theme_type"), &Theme::has_stylebox);
	ClassDB::bind_method(D_METHOD("rename_stylebox", "old_name", "name", "theme_type"), &Theme::rename_stylebox);
	ClassDB::bind_method(D_METHOD("clear_stylebox", "name", "theme_type"), &Theme::clear_stylebox);
	ClassDB::bind_method(D_METHOD("get_stylebox_list", "theme_type"), &Theme::_get_stylebox_list);
	ClassDB::bind_method(D_METHOD("get_stylebox_type_list"), &Theme::_get_stylebox_type_list);

	ClassDB::bind_method(D_METHOD("set_font", "name", "theme_type", "font"), &Theme::set_font);
	ClassDB::bind_method(D_METHOD("get_font", "name", "theme_type"), &Theme::get_font);
	ClassDB::bind_method(D_METHOD("has_font", "name", "theme_type"), &Theme::has_font);
	ClassDB::bind_method(D_METHOD("rename_font", "old_name", "name", "theme_type"), &Theme::rename_font);
	ClassDB::bind_method(D_METHOD("clear_font", "name", "theme_type"), &Theme::clear_font);
	ClassDB::bind_method(D_METHOD("get_font_list", "theme_type"), &Theme::_get_font_list);
	ClassDB::bind_method(D_METHOD("get_font_type_list"), &Theme::_get_font_type_list);

	ClassDB::bind_method(D_METHOD("set_font_size", "name", "theme_type", "font_size"), &Theme::set_font_size);
	ClassDB::bind_method(D_METHOD("get_font_size", "name", "theme_type"), &Theme::get_font_size);
	ClassDB::bind_method(D_METHOD("has_font_size", "name", "theme_type"), &Theme::has_font_size);
	ClassDB::bind_method(D_METHOD("rename_font_size", "old_name", "name", "theme_type"), &Theme::rename_font_size);
	ClassDB::bind_method(D_METHOD("clear_font_size", "name", "theme_type"), &Theme::clear_font_size);
	ClassDB::bind_method(D_METHOD("get_font_size_list", "theme_type"), &Theme::_get_font_size_list);
	ClassDB::bind_method(D_METHOD("get_font_size_type_list"), &Theme::_get_font_size_type_list);

	ClassDB::bind_method(D_METHOD("set_color", "name", "theme_type", "color"), &Theme::set_color);
	ClassDB::bind_method(D_METHOD("get_color", "name", "theme_type"), &Theme::get_color);
	ClassDB::bind_method(D_METHOD("has_color", "name", "theme_type"), &Theme::has_color);
	ClassDB::bind_method(D_METHOD("rename_color", "old_name", "name", "theme_type"), &Theme::rename_color);
	ClassDB::bind_method(D_METHOD("clear_color", "name", "theme_type"), &Theme::clear_color);
	ClassDB::bind_method(D_METHOD("get_color_list", "theme_type"), &Theme::_get_color_list);
	ClassDB::bind_method(D_METHOD("get_color_type_list"), &Theme::_get_color_type_list);

	ClassDB::bind_method(D_METHOD("set_constant", "name", "theme_type", "constant"), &Theme::set_constant);
	ClassDB::bind_method(D_METHOD("get_constant", "name", "theme_type"), &Theme::get_constant);
	ClassDB::bind_method(D_METHOD("has_constant", "name", "theme_type"), &Theme::has_constant);
	ClassDB::bind_method(D_METHOD("rename_constant", "old_name", "name", "theme_type"), &Theme::rename_constant);
	ClassDB::bind_method(D_METHOD("clear_constant", "name", "theme_type"), &Theme::clear_constant);
	ClassDB::bind_method(D_METHOD("get_constant_list", "theme_type"), &Theme::_get_constant_list);
	ClassDB::bind_method(D_METHOD("get_constant_type_list"), &Theme::_get_constant_type_list);

	ClassDB::bind_method(D_METHOD("set_default_base_scale", "base_scale"), &Theme::set_default_base_scale);
	ClassDB::bind_method(D_METHOD("get_default_base_scale"), &Theme::get_default_base_scale);
	ClassDB::bind_method(D_METHOD("has_default_base_scale"), &Theme::has_default_base_scale);

	ClassDB::bind_method(D_METHOD("set_default_font", "font"), &Theme::set_default_font);
	ClassDB::bind_method(D_METHOD("get_default_font"), &Theme::get_default_font);
	ClassDB::bind_method(D_METHOD("has_default_font"), &Theme::has_default_font);

	ClassDB::bind_method(D_METHOD("set_default_font_size", "font_size"), &Theme::set_default_font_size);
	ClassDB::bind_method(D_METHOD("get_default_font_size"), &Theme::get_default_font_size);
	ClassDB::bind_method(D_METHOD("has_default_font_size"), &Theme::has_default_font_size);

	ClassDB::bind_method(D_METHOD("set_theme_item", "data_type", "name", "theme_type", "value"), &Theme::set_theme_item);
	ClassDB::bind_method(D_METHOD("get_theme_item", "data_type", "name", "theme_type"), &Theme::get_theme_item);
	ClassDB::bind_method(D_METHOD("has_theme_item", "data_type", "name", "theme_type"), &Theme::has_theme_item);
	ClassDB::bind_method(D_METHOD("rename_theme_item", "data_type", "old_name", "name", "theme_type"), &Theme::rename_theme_item);
	ClassDB::bind_method(D_METHOD("clear_theme_item", "data_type", "name", "theme_type"), &Theme::clear_theme_item);
	ClassDB::bind_method(D_METHOD("get_theme_item_list", "data_type", "theme_type"), &Theme::_get_theme_item_list);
	ClassDB::bind_method(D_METHOD("get_theme_item_type_list", "data_type"), &Theme::_get_theme_item_type_list);

	ClassDB::bind_method(D_METHOD("set_type_variation", "theme_type", "base_type"), &Theme::set_type_variation);
	ClassDB::bind_method(D_METHOD("is_type_variation", "theme_type", "base_type"), &Theme::is_type_variation);
	ClassDB::bind_method(D_METHOD("clear_type_variation", "theme_type"), &Theme::clear_type_variation);
	ClassDB::bind_method(D_METHOD("get_type_variation_base", "theme_type"), &Theme::get_type_variation_base);
	ClassDB::bind_method(D_METHOD("get_type_variation_list", "base_type"), &Theme::_get_type_variation_list);

	ClassDB::bind_method(D_METHOD("add_type", "theme_type"), &Theme::add_type);
	ClassDB::bind_method(D_METHOD("remove_type", "theme_type"), &Theme::remove_type);
	ClassDB::bind_method(D_METHOD("get_type_list"), &Theme::_get_type_list);

	ClassDB::bind_method(D_METHOD("merge_with", "other"), &Theme::merge_with);
	ClassDB::bind_method(D_METHOD("clear"), &Theme::clear);

	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "default_base_scale", PROPERTY_HINT_RANGE, "0.0,2.0,0.01,or_greater"), "set_default_base_scale", "get_default_base_scale");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "default_font", PROPERTY_HINT_RESOURCE_TYPE, "Font"), "set_default_font", "get_default_font");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "default_font_size", PROPERTY_HINT_RANGE, "0,256,1,or_greater,suffix:px"), "set_default_font_size", "get_default_font_size");

	BIND_ENUM_CONSTANT(DATA_TYPE_COLOR);
	BIND_ENUM_CONSTANT(DATA_TYPE_CONSTANT);
	BIND_ENUM_CONSTANT(DATA_TYPE_FONT);
	BIND_ENUM_CONSTANT(DATA_TYPE_FONT_SIZE);
	BIND_ENUM_CONSTANT(DATA_TYPE_ICON);
	BIND_ENUM_CONSTANT(DATA_TYPE_STYLEBOX);
	BIND_ENUM_CONSTANT(DATA_TYPE_MAX);
}

Theme::Theme() {
}

Theme::~Theme() {
}

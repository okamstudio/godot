/**************************************************************************/
/*  label_settings.cpp                                                    */
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

#include "label_settings.h"

void LabelSettings::_font_changed() {
	emit_changed();
}

void LabelSettings::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_line_spacing", "spacing"), &LabelSettings::set_line_spacing);
	ClassDB::bind_method(D_METHOD("get_line_spacing"), &LabelSettings::get_line_spacing);

	ClassDB::bind_method(D_METHOD("set_paragraph_spacing", "spacing"), &LabelSettings::set_paragraph_spacing);
	ClassDB::bind_method(D_METHOD("get_paragraph_spacing"), &LabelSettings::get_paragraph_spacing);

	ClassDB::bind_method(D_METHOD("set_font", "font"), &LabelSettings::set_font);
	ClassDB::bind_method(D_METHOD("get_font"), &LabelSettings::get_font);

	ClassDB::bind_method(D_METHOD("set_font_size", "size"), &LabelSettings::set_font_size);
	ClassDB::bind_method(D_METHOD("get_font_size"), &LabelSettings::get_font_size);

	ClassDB::bind_method(D_METHOD("set_font_color", "color"), &LabelSettings::set_font_color);
	ClassDB::bind_method(D_METHOD("get_font_color"), &LabelSettings::get_font_color);

	ClassDB::bind_method(D_METHOD("set_outline_size", "size"), &LabelSettings::set_outline_size);
	ClassDB::bind_method(D_METHOD("get_outline_size"), &LabelSettings::get_outline_size);

	ClassDB::bind_method(D_METHOD("set_outline_color", "color"), &LabelSettings::set_outline_color);
	ClassDB::bind_method(D_METHOD("get_outline_color"), &LabelSettings::get_outline_color);

	ClassDB::bind_method(D_METHOD("set_shadow_size", "size"), &LabelSettings::set_shadow_size);
	ClassDB::bind_method(D_METHOD("get_shadow_size"), &LabelSettings::get_shadow_size);

	ClassDB::bind_method(D_METHOD("set_shadow_color", "color"), &LabelSettings::set_shadow_color);
	ClassDB::bind_method(D_METHOD("get_shadow_color"), &LabelSettings::get_shadow_color);

	ClassDB::bind_method(D_METHOD("set_shadow_offset", "offset"), &LabelSettings::set_shadow_offset);
	ClassDB::bind_method(D_METHOD("get_shadow_offset"), &LabelSettings::get_shadow_offset);

	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "line_spacing", PROPERTY_HINT_NONE, "suffix:px"), "set_line_spacing", "get_line_spacing");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "paragraph_spacing", PROPERTY_HINT_NONE, "suffix:px"), "set_paragraph_spacing", "get_paragraph_spacing");

	ADD_GROUP("Font", "font_");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "font", PROPERTY_HINT_RESOURCE_TYPE, "Font"), "set_font", "get_font");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "font_size", PROPERTY_HINT_RANGE, "1,1024,1,or_greater,suffix:px"), "set_font_size", "get_font_size");
	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "font_color"), "set_font_color", "get_font_color");

	ADD_GROUP("Outline", "outline_");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "outline_size", PROPERTY_HINT_RANGE, "0,127,1,or_greater,suffix:px"), "set_outline_size", "get_outline_size");
	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "outline_color"), "set_outline_color", "get_outline_color");

	ADD_GROUP("Shadow", "shadow_");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "shadow_size", PROPERTY_HINT_RANGE, "0,127,1,or_greater,suffix:px"), "set_shadow_size", "get_shadow_size");
	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "shadow_color"), "set_shadow_color", "get_shadow_color");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "shadow_offset", PROPERTY_HINT_NONE, "suffix:px"), "set_shadow_offset", "get_shadow_offset");
}

void LabelSettings::set_line_spacing(real_t p_spacing) {
	if (line_spacing != p_spacing) {
		line_spacing = p_spacing;
		emit_changed();
	}
}

real_t LabelSettings::get_line_spacing() const {
	return line_spacing;
}

void LabelSettings::set_paragraph_spacing(real_t p_spacing) {
	if (paragraph_spacing != p_spacing) {
		paragraph_spacing = p_spacing;
		emit_changed();
	}
}

real_t LabelSettings::get_paragraph_spacing() const {
	return paragraph_spacing;
}

void LabelSettings::set_font(const Ref<Font> &p_font) {
	if (font != p_font) {
		if (font.is_valid()) {
			font->disconnect_changed(callable_mp(this, &LabelSettings::_font_changed));
		}
		font = p_font;
		if (font.is_valid()) {
			font->connect_changed(callable_mp(this, &LabelSettings::_font_changed), CONNECT_REFERENCE_COUNTED);
		}
		emit_changed();
	}
}

Ref<Font> LabelSettings::get_font() const {
	return font;
}

void LabelSettings::set_font_size(int p_size) {
	if (font_size != p_size) {
		font_size = p_size;
		emit_changed();
	}
}

int LabelSettings::get_font_size() const {
	return font_size;
}

void LabelSettings::set_font_color(const Color &p_color) {
	if (font_color != p_color) {
		font_color = p_color;
		emit_changed();
	}
}

Color LabelSettings::get_font_color() const {
	return font_color;
}

void LabelSettings::set_outline_size(int p_size) {
	if (outline_size != p_size) {
		outline_size = p_size;
		emit_changed();
	}
}

int LabelSettings::get_outline_size() const {
	return outline_size;
}

void LabelSettings::set_outline_color(const Color &p_color) {
	if (outline_color != p_color) {
		outline_color = p_color;
		emit_changed();
	}
}

Color LabelSettings::get_outline_color() const {
	return outline_color;
}

void LabelSettings::set_shadow_size(int p_size) {
	if (shadow_size != p_size) {
		shadow_size = p_size;
		emit_changed();
	}
}

int LabelSettings::get_shadow_size() const {
	return shadow_size;
}

void LabelSettings::set_shadow_color(const Color &p_color) {
	if (shadow_color != p_color) {
		shadow_color = p_color;
		emit_changed();
	}
}

Color LabelSettings::get_shadow_color() const {
	return shadow_color;
}

void LabelSettings::set_shadow_offset(const Vector2 &p_offset) {
	if (shadow_offset != p_offset) {
		shadow_offset = p_offset;
		emit_changed();
	}
}

Vector2 LabelSettings::get_shadow_offset() const {
	return shadow_offset;
}

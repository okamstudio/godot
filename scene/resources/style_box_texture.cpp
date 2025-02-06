/**************************************************************************/
/*  style_box_texture.cpp                                                 */
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

#include "style_box_texture.h"

float StyleBoxTexture::get_style_margin(Side p_side) const {
	ERR_FAIL_INDEX_V((int)p_side, 4, 0.0);

	return texture_margin[p_side];
}

void StyleBoxTexture::set_texture(Ref<Texture2D> p_texture) {
	if (texture == p_texture) {
		return;
	}
	texture = p_texture;
	emit_changed();
}

Ref<Texture2D> StyleBoxTexture::get_texture() const {
	return texture;
}

void StyleBoxTexture::set_texture_margin(Side p_side, float p_size) {
	ERR_FAIL_INDEX((int)p_side, 4);

	texture_margin[p_side] = p_size;
	emit_changed();
}

void StyleBoxTexture::set_texture_margin_all(float p_size) {
	for (int i = 0; i < 4; i++) {
		texture_margin[i] = p_size;
	}
	emit_changed();
}

void StyleBoxTexture::set_texture_margin_individual(float p_left, float p_top, float p_right, float p_bottom) {
	texture_margin[SIDE_LEFT] = p_left;
	texture_margin[SIDE_TOP] = p_top;
	texture_margin[SIDE_RIGHT] = p_right;
	texture_margin[SIDE_BOTTOM] = p_bottom;
	emit_changed();
}

float StyleBoxTexture::get_texture_margin(Side p_side) const {
	ERR_FAIL_INDEX_V((int)p_side, 4, 0.0);

	return texture_margin[p_side];
}

void StyleBoxTexture::set_expand_margin(Side p_side, float p_size) {
	ERR_FAIL_INDEX((int)p_side, 4);
	expand_margin[p_side] = p_size;
	emit_changed();
}

void StyleBoxTexture::set_expand_margin_all(float p_expand_margin_size) {
	for (int i = 0; i < 4; i++) {
		expand_margin[i] = p_expand_margin_size;
	}
	emit_changed();
}

void StyleBoxTexture::set_expand_margin_individual(float p_left, float p_top, float p_right, float p_bottom) {
	expand_margin[SIDE_LEFT] = p_left;
	expand_margin[SIDE_TOP] = p_top;
	expand_margin[SIDE_RIGHT] = p_right;
	expand_margin[SIDE_BOTTOM] = p_bottom;
	emit_changed();
}

float StyleBoxTexture::get_expand_margin(Side p_side) const {
	ERR_FAIL_INDEX_V((int)p_side, 4, 0);
	return expand_margin[p_side];
}

void StyleBoxTexture::set_region_rect(const Rect2 &p_region_rect) {
	if (region_rect == p_region_rect) {
		return;
	}

	region_rect = p_region_rect;
	emit_changed();
}

Rect2 StyleBoxTexture::get_region_rect() const {
	return region_rect;
}

void StyleBoxTexture::set_draw_center(bool p_enabled) {
	draw_center = p_enabled;
	emit_changed();
}

bool StyleBoxTexture::is_draw_center_enabled() const {
	return draw_center;
}

void StyleBoxTexture::set_h_axis_stretch_mode(AxisStretchMode p_mode) {
	ERR_FAIL_INDEX((int)p_mode, 3);
	axis_h = p_mode;
	emit_changed();
}

StyleBoxTexture::AxisStretchMode StyleBoxTexture::get_h_axis_stretch_mode() const {
	return axis_h;
}

void StyleBoxTexture::set_v_axis_stretch_mode(AxisStretchMode p_mode) {
	ERR_FAIL_INDEX((int)p_mode, 3);
	axis_v = p_mode;
	emit_changed();
}

StyleBoxTexture::AxisStretchMode StyleBoxTexture::get_v_axis_stretch_mode() const {
	return axis_v;
}

void StyleBoxTexture::set_modulate(const Color &p_modulate) {
	if (modulate == p_modulate) {
		return;
	}
	modulate = p_modulate;
	emit_changed();
}

Color StyleBoxTexture::get_modulate() const {
	return modulate;
}

Rect2 StyleBoxTexture::get_draw_rect(const Rect2 &p_rect) const {
	return p_rect.grow_individual(expand_margin[SIDE_LEFT], expand_margin[SIDE_TOP], expand_margin[SIDE_RIGHT], expand_margin[SIDE_BOTTOM]);
}

void StyleBoxTexture::draw(RID p_canvas_item, const Rect2 &p_rect) const {
	if (texture.is_null()) {
		return;
	}

	Rect2 rect = p_rect;
	Rect2 src_rect = region_rect;

	texture->get_rect_region(rect, src_rect, rect, src_rect);

	rect.position.x -= expand_margin[SIDE_LEFT];
	rect.position.y -= expand_margin[SIDE_TOP];
	rect.size.x += expand_margin[SIDE_LEFT] + expand_margin[SIDE_RIGHT];
	rect.size.y += expand_margin[SIDE_TOP] + expand_margin[SIDE_BOTTOM];

	Vector2 start_offset = Vector2(texture_margin[SIDE_LEFT], texture_margin[SIDE_TOP]);
	Vector2 end_offset = Vector2(texture_margin[SIDE_RIGHT], texture_margin[SIDE_BOTTOM]);

	RenderingServer::get_singleton()->canvas_item_add_nine_patch(p_canvas_item, rect, src_rect, texture->get_rid(), start_offset, end_offset, RS::NinePatchAxisMode(axis_h), RS::NinePatchAxisMode(axis_v), draw_center, modulate);
}

void StyleBoxTexture::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_texture", "texture"), &StyleBoxTexture::set_texture);
	ClassDB::bind_method(D_METHOD("get_texture"), &StyleBoxTexture::get_texture);

	ClassDB::bind_method(D_METHOD("set_texture_margin", "margin", "size"), &StyleBoxTexture::set_texture_margin);
	ClassDB::bind_method(D_METHOD("set_texture_margin_all", "size"), &StyleBoxTexture::set_texture_margin_all);
	ClassDB::bind_method(D_METHOD("get_texture_margin", "margin"), &StyleBoxTexture::get_texture_margin);

	ClassDB::bind_method(D_METHOD("set_expand_margin", "margin", "size"), &StyleBoxTexture::set_expand_margin);
	ClassDB::bind_method(D_METHOD("set_expand_margin_all", "size"), &StyleBoxTexture::set_expand_margin_all);
	ClassDB::bind_method(D_METHOD("get_expand_margin", "margin"), &StyleBoxTexture::get_expand_margin);

	ClassDB::bind_method(D_METHOD("set_region_rect", "region"), &StyleBoxTexture::set_region_rect);
	ClassDB::bind_method(D_METHOD("get_region_rect"), &StyleBoxTexture::get_region_rect);

	ClassDB::bind_method(D_METHOD("set_draw_center", "enable"), &StyleBoxTexture::set_draw_center);
	ClassDB::bind_method(D_METHOD("is_draw_center_enabled"), &StyleBoxTexture::is_draw_center_enabled);

	ClassDB::bind_method(D_METHOD("set_modulate", "color"), &StyleBoxTexture::set_modulate);
	ClassDB::bind_method(D_METHOD("get_modulate"), &StyleBoxTexture::get_modulate);

	ClassDB::bind_method(D_METHOD("set_h_axis_stretch_mode", "mode"), &StyleBoxTexture::set_h_axis_stretch_mode);
	ClassDB::bind_method(D_METHOD("get_h_axis_stretch_mode"), &StyleBoxTexture::get_h_axis_stretch_mode);

	ClassDB::bind_method(D_METHOD("set_v_axis_stretch_mode", "mode"), &StyleBoxTexture::set_v_axis_stretch_mode);
	ClassDB::bind_method(D_METHOD("get_v_axis_stretch_mode"), &StyleBoxTexture::get_v_axis_stretch_mode);

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "texture", PROPERTY_HINT_RESOURCE_TYPE, "Texture2D"), "set_texture", "get_texture");

	ADD_GROUP("Texture Margins", "texture_margin_");
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "texture_margin_left", PROPERTY_HINT_RANGE, "0,2048,1,suffix:px"), "set_texture_margin", "get_texture_margin", SIDE_LEFT);
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "texture_margin_top", PROPERTY_HINT_RANGE, "0,2048,1,suffix:px"), "set_texture_margin", "get_texture_margin", SIDE_TOP);
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "texture_margin_right", PROPERTY_HINT_RANGE, "0,2048,1,suffix:px"), "set_texture_margin", "get_texture_margin", SIDE_RIGHT);
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "texture_margin_bottom", PROPERTY_HINT_RANGE, "0,2048,1,suffix:px"), "set_texture_margin", "get_texture_margin", SIDE_BOTTOM);

	ADD_GROUP("Expand Margins", "expand_margin_");
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "expand_margin_left", PROPERTY_HINT_RANGE, "0,2048,1,suffix:px"), "set_expand_margin", "get_expand_margin", SIDE_LEFT);
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "expand_margin_top", PROPERTY_HINT_RANGE, "0,2048,1,suffix:px"), "set_expand_margin", "get_expand_margin", SIDE_TOP);
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "expand_margin_right", PROPERTY_HINT_RANGE, "0,2048,1,suffix:px"), "set_expand_margin", "get_expand_margin", SIDE_RIGHT);
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "expand_margin_bottom", PROPERTY_HINT_RANGE, "0,2048,1,suffix:px"), "set_expand_margin", "get_expand_margin", SIDE_BOTTOM);

	ADD_GROUP("Axis Stretch", "axis_stretch_");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "axis_stretch_horizontal", PROPERTY_HINT_ENUM, "Stretch,Tile,Tile Fit,Center"), "set_h_axis_stretch_mode", "get_h_axis_stretch_mode");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "axis_stretch_vertical", PROPERTY_HINT_ENUM, "Stretch,Tile,Tile Fit,Center"), "set_v_axis_stretch_mode", "get_v_axis_stretch_mode");

	ADD_GROUP("Sub-Region", "region_");
	ADD_PROPERTY(PropertyInfo(Variant::RECT2, "region_rect", PROPERTY_HINT_NONE, "suffix:px"), "set_region_rect", "get_region_rect");

	ADD_GROUP("Modulate", "modulate_");
	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "modulate_color"), "set_modulate", "get_modulate");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "draw_center"), "set_draw_center", "is_draw_center_enabled");

	BIND_ENUM_CONSTANT(AXIS_STRETCH_MODE_STRETCH);
	BIND_ENUM_CONSTANT(AXIS_STRETCH_MODE_TILE);
	BIND_ENUM_CONSTANT(AXIS_STRETCH_MODE_TILE_FIT);
	BIND_ENUM_CONSTANT(AXIS_STRETCH_MODE_CENTER);
}

StyleBoxTexture::StyleBoxTexture() {}

StyleBoxTexture::~StyleBoxTexture() {}

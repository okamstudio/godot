/**************************************************************************/
/*  nine_patch_rect.cpp                                                   */
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

#include "nine_patch_rect.h"

#include "servers/rendering_server.h"

void NinePatchRect::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_DRAW: {
			if (texture.is_null()) {
				return;
			}

			Rect2 rect = Rect2(Point2(), get_size());
			Rect2 src_rect = region_rect;

			texture->get_rect_region(rect, src_rect, rect, src_rect);

			RID ci = get_canvas_item();
			RS::get_singleton()->canvas_item_add_nine_patch(ci, rect, src_rect, texture->get_rid(), Vector2(margin[SIDE_LEFT], margin[SIDE_TOP]), Vector2(margin[SIDE_RIGHT], margin[SIDE_BOTTOM]), RS::NinePatchAxisMode(axis_h), RS::NinePatchAxisMode(axis_v), draw_center);
		} break;
	}
}

Size2 NinePatchRect::get_minimum_size() const {
	return Size2(margin[SIDE_LEFT] + margin[SIDE_RIGHT], margin[SIDE_TOP] + margin[SIDE_BOTTOM]);
}

void NinePatchRect::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_texture", "texture"), &NinePatchRect::set_texture);
	ClassDB::bind_method(D_METHOD("get_texture"), &NinePatchRect::get_texture);
	ClassDB::bind_method(D_METHOD("set_patch_margin", "margin", "value"), &NinePatchRect::set_patch_margin);
	ClassDB::bind_method(D_METHOD("get_patch_margin", "margin"), &NinePatchRect::get_patch_margin);
	ClassDB::bind_method(D_METHOD("set_region_rect", "rect"), &NinePatchRect::set_region_rect);
	ClassDB::bind_method(D_METHOD("get_region_rect"), &NinePatchRect::get_region_rect);
	ClassDB::bind_method(D_METHOD("set_draw_center", "draw_center"), &NinePatchRect::set_draw_center);
	ClassDB::bind_method(D_METHOD("is_draw_center_enabled"), &NinePatchRect::is_draw_center_enabled);
	ClassDB::bind_method(D_METHOD("set_h_axis_stretch_mode", "mode"), &NinePatchRect::set_h_axis_stretch_mode);
	ClassDB::bind_method(D_METHOD("get_h_axis_stretch_mode"), &NinePatchRect::get_h_axis_stretch_mode);
	ClassDB::bind_method(D_METHOD("set_v_axis_stretch_mode", "mode"), &NinePatchRect::set_v_axis_stretch_mode);
	ClassDB::bind_method(D_METHOD("get_v_axis_stretch_mode"), &NinePatchRect::get_v_axis_stretch_mode);

	ADD_SIGNAL(MethodInfo("texture_changed"));

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "texture", PROPERTY_HINT_RESOURCE_TYPE, "Texture2D"), "set_texture", "get_texture");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "draw_center"), "set_draw_center", "is_draw_center_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::RECT2, "region_rect", PROPERTY_HINT_NONE, "suffix:px"), "set_region_rect", "get_region_rect");

	ADD_GROUP("Patch Margin", "patch_margin_");
	ADD_PROPERTYI(PropertyInfo(Variant::INT, "patch_margin_left", PROPERTY_HINT_RANGE, "0,16384,1,suffix:px"), "set_patch_margin", "get_patch_margin", SIDE_LEFT);
	ADD_PROPERTYI(PropertyInfo(Variant::INT, "patch_margin_top", PROPERTY_HINT_RANGE, "0,16384,1,suffix:px"), "set_patch_margin", "get_patch_margin", SIDE_TOP);
	ADD_PROPERTYI(PropertyInfo(Variant::INT, "patch_margin_right", PROPERTY_HINT_RANGE, "0,16384,1,suffix:px"), "set_patch_margin", "get_patch_margin", SIDE_RIGHT);
	ADD_PROPERTYI(PropertyInfo(Variant::INT, "patch_margin_bottom", PROPERTY_HINT_RANGE, "0,16384,1,suffix:px"), "set_patch_margin", "get_patch_margin", SIDE_BOTTOM);
	ADD_GROUP("Axis Stretch", "axis_stretch_");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "axis_stretch_horizontal", PROPERTY_HINT_ENUM, "Stretch,Tile,Tile Fit,Center"), "set_h_axis_stretch_mode", "get_h_axis_stretch_mode");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "axis_stretch_vertical", PROPERTY_HINT_ENUM, "Stretch,Tile,Tile Fit,Center"), "set_v_axis_stretch_mode", "get_v_axis_stretch_mode");

	BIND_ENUM_CONSTANT(AXIS_STRETCH_MODE_STRETCH);
	BIND_ENUM_CONSTANT(AXIS_STRETCH_MODE_TILE);
	BIND_ENUM_CONSTANT(AXIS_STRETCH_MODE_TILE_FIT);
	BIND_ENUM_CONSTANT(AXIS_STRETCH_MODE_CENTER);
}

void NinePatchRect::_texture_changed() {
	queue_redraw();
	update_minimum_size();
}

void NinePatchRect::set_texture(const Ref<Texture2D> &p_tex) {
	if (texture == p_tex) {
		return;
	}

	if (texture.is_valid()) {
		texture->disconnect_changed(callable_mp(this, &NinePatchRect::_texture_changed));
	}

	texture = p_tex;

	if (texture.is_valid()) {
		texture->connect_changed(callable_mp(this, &NinePatchRect::_texture_changed));
	}

	queue_redraw();
	update_minimum_size();
	emit_signal(SceneStringName(texture_changed));
}

Ref<Texture2D> NinePatchRect::get_texture() const {
	return texture;
}

void NinePatchRect::set_patch_margin(Side p_side, int p_size) {
	ERR_FAIL_INDEX((int)p_side, 4);

	if (margin[p_side] == p_size) {
		return;
	}

	margin[p_side] = p_size;
	queue_redraw();
	update_minimum_size();
}

int NinePatchRect::get_patch_margin(Side p_side) const {
	ERR_FAIL_INDEX_V((int)p_side, 4, 0);
	return margin[p_side];
}

void NinePatchRect::set_region_rect(const Rect2 &p_region_rect) {
	if (region_rect == p_region_rect) {
		return;
	}

	region_rect = p_region_rect;

	item_rect_changed();
}

Rect2 NinePatchRect::get_region_rect() const {
	return region_rect;
}

void NinePatchRect::set_draw_center(bool p_enabled) {
	if (draw_center == p_enabled) {
		return;
	}

	draw_center = p_enabled;
	queue_redraw();
}

bool NinePatchRect::is_draw_center_enabled() const {
	return draw_center;
}

void NinePatchRect::set_h_axis_stretch_mode(AxisStretchMode p_mode) {
	if (axis_h == p_mode) {
		return;
	}

	axis_h = p_mode;
	queue_redraw();
}

NinePatchRect::AxisStretchMode NinePatchRect::get_h_axis_stretch_mode() const {
	return axis_h;
}

void NinePatchRect::set_v_axis_stretch_mode(AxisStretchMode p_mode) {
	if (axis_v == p_mode) {
		return;
	}

	axis_v = p_mode;
	queue_redraw();
}

NinePatchRect::AxisStretchMode NinePatchRect::get_v_axis_stretch_mode() const {
	return axis_v;
}

NinePatchRect::NinePatchRect() {
	set_mouse_filter(MOUSE_FILTER_IGNORE);
}

NinePatchRect::~NinePatchRect() {
}

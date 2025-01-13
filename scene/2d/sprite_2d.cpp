/**************************************************************************/
/*  sprite_2d.cpp                                                         */
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

#include "sprite_2d.h"

#include "scene/main/viewport.h"

#ifdef TOOLS_ENABLED
Dictionary Sprite2D::_edit_get_state() const {
	Dictionary state = Node2D::_edit_get_state();
	state["offset"] = offset;
	return state;
}

void Sprite2D::_edit_set_state(const Dictionary &p_state) {
	Node2D::_edit_set_state(p_state);
	set_offset(p_state["offset"]);
}

void Sprite2D::_edit_set_pivot(const Point2 &p_pivot) {
	set_offset(get_offset() - p_pivot);
	set_position(get_transform().xform(p_pivot));
}

Point2 Sprite2D::_edit_get_pivot() const {
	return Vector2();
}

bool Sprite2D::_edit_use_pivot() const {
	return true;
}
#endif // TOOLS_ENABLED

#ifdef DEBUG_ENABLED
bool Sprite2D::_edit_is_selected_on_click(const Point2 &p_point, double p_tolerance) const {
	return is_pixel_opaque(p_point);
}

Rect2 Sprite2D::_edit_get_rect() const {
	return get_rect();
}

bool Sprite2D::_edit_use_rect() const {
	return texture.is_valid();
}
#endif // DEBUG_ENABLED

Rect2 Sprite2D::get_anchorable_rect() const {
	return get_rect();
}

void Sprite2D::_get_rects(Rect2 &r_src_rect, Rect2 &r_dst_rect, bool &r_filter_clip_enabled) const {
	Rect2 base_rect;

	if (region_enabled) {
		r_filter_clip_enabled = region_filter_clip_enabled;
		base_rect = region_rect;
	} else {
		r_filter_clip_enabled = false;
		base_rect = Rect2(0, 0, texture->get_width(), texture->get_height());
	}

	Size2 frame_size = base_rect.size / Size2(hframes, vframes);
	Point2 frame_offset = Point2(frame % hframes, frame / hframes);
	frame_offset *= frame_size;

	r_src_rect.size = frame_size;
	r_src_rect.position = base_rect.position + frame_offset;

	Point2 dest_offset = offset;
	if (centered) {
		dest_offset -= frame_size / 2;
	}

	if (get_viewport() && get_viewport()->is_snap_2d_transforms_to_pixel_enabled()) {
		dest_offset = (dest_offset + Point2(0.5, 0.5)).floor();
	}

	r_dst_rect = Rect2(dest_offset, frame_size);

	if (hflip) {
		r_dst_rect.size.x = -r_dst_rect.size.x;
	}
	if (vflip) {
		r_dst_rect.size.y = -r_dst_rect.size.y;
	}
}

void Sprite2D::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_DRAW: {
			if (texture.is_null()) {
				return;
			}

			RID ci = get_canvas_item();

			Rect2 src_rect, dst_rect;
			bool filter_clip_enabled;
			_get_rects(src_rect, dst_rect, filter_clip_enabled);

			texture->draw_rect_region(ci, dst_rect, src_rect, Color(1, 1, 1), false, filter_clip_enabled);
		} break;
	}
}

void Sprite2D::set_texture(const Ref<Texture2D> &p_texture) {
	if (p_texture == texture) {
		return;
	}

	if (texture.is_valid()) {
		texture->disconnect_changed(callable_mp(this, &Sprite2D::_texture_changed));
	}

	texture = p_texture;

	if (texture.is_valid()) {
		texture->connect_changed(callable_mp(this, &Sprite2D::_texture_changed));
	}

	queue_redraw();
	emit_signal(SceneStringName(texture_changed));
	item_rect_changed();
}

Ref<Texture2D> Sprite2D::get_texture() const {
	return texture;
}

void Sprite2D::set_centered(bool p_center) {
	if (centered == p_center) {
		return;
	}

	centered = p_center;
	queue_redraw();
	item_rect_changed();
}

bool Sprite2D::is_centered() const {
	return centered;
}

void Sprite2D::set_offset(const Point2 &p_offset) {
	if (offset == p_offset) {
		return;
	}

	offset = p_offset;
	queue_redraw();
	item_rect_changed();
}

Point2 Sprite2D::get_offset() const {
	return offset;
}

void Sprite2D::set_flip_h(bool p_flip) {
	if (hflip == p_flip) {
		return;
	}

	hflip = p_flip;
	queue_redraw();
}

bool Sprite2D::is_flipped_h() const {
	return hflip;
}

void Sprite2D::set_flip_v(bool p_flip) {
	if (vflip == p_flip) {
		return;
	}

	vflip = p_flip;
	queue_redraw();
}

bool Sprite2D::is_flipped_v() const {
	return vflip;
}

void Sprite2D::set_region_enabled(bool p_region_enabled) {
	if (region_enabled == p_region_enabled) {
		return;
	}

	region_enabled = p_region_enabled;
	queue_redraw();
	notify_property_list_changed();
}

bool Sprite2D::is_region_enabled() const {
	return region_enabled;
}

void Sprite2D::set_region_rect(const Rect2 &p_region_rect) {
	if (region_rect == p_region_rect) {
		return;
	}

	region_rect = p_region_rect;

	if (region_enabled) {
		item_rect_changed();
	}
}

Rect2 Sprite2D::get_region_rect() const {
	return region_rect;
}

void Sprite2D::set_region_filter_clip_enabled(bool p_region_filter_clip_enabled) {
	if (region_filter_clip_enabled == p_region_filter_clip_enabled) {
		return;
	}

	region_filter_clip_enabled = p_region_filter_clip_enabled;
	queue_redraw();
}

bool Sprite2D::is_region_filter_clip_enabled() const {
	return region_filter_clip_enabled;
}

void Sprite2D::set_frame(int p_frame) {
	ERR_FAIL_INDEX(p_frame, vframes * hframes);

	if (frame == p_frame) {
		return;
	}

	frame = p_frame;
	item_rect_changed();
	emit_signal(SceneStringName(frame_changed));
}

int Sprite2D::get_frame() const {
	return frame;
}

void Sprite2D::set_frame_coords(const Vector2i &p_coord) {
	ERR_FAIL_INDEX(p_coord.x, hframes);
	ERR_FAIL_INDEX(p_coord.y, vframes);

	set_frame(p_coord.y * hframes + p_coord.x);
}

Vector2i Sprite2D::get_frame_coords() const {
	return Vector2i(frame % hframes, frame / hframes);
}

void Sprite2D::set_vframes(int p_amount) {
	ERR_FAIL_COND_MSG(p_amount < 1, "Amount of vframes cannot be smaller than 1.");

	if (vframes == p_amount) {
		return;
	}

	vframes = p_amount;
	if (frame >= vframes * hframes) {
		frame = 0;
	}
	queue_redraw();
	item_rect_changed();
	notify_property_list_changed();
}

int Sprite2D::get_vframes() const {
	return vframes;
}

void Sprite2D::set_hframes(int p_amount) {
	ERR_FAIL_COND_MSG(p_amount < 1, "Amount of hframes cannot be smaller than 1.");

	if (hframes == p_amount) {
		return;
	}

	if (vframes > 1) {
		// Adjust the frame to fit new sheet dimensions.
		int original_column = frame % hframes;
		if (original_column >= p_amount) {
			// Frame's column was dropped, reset.
			frame = 0;
		} else {
			int original_row = frame / hframes;
			frame = original_row * p_amount + original_column;
		}
	}
	hframes = p_amount;
	if (frame >= vframes * hframes) {
		frame = 0;
	}
	queue_redraw();
	item_rect_changed();
	notify_property_list_changed();
}

int Sprite2D::get_hframes() const {
	return hframes;
}

bool Sprite2D::is_pixel_opaque(const Point2 &p_point) const {
	if (texture.is_null()) {
		return false;
	}

	if (texture->get_size().width == 0 || texture->get_size().height == 0) {
		return false;
	}

	Rect2 src_rect, dst_rect;
	bool filter_clip_enabled;
	_get_rects(src_rect, dst_rect, filter_clip_enabled);
	dst_rect.size = dst_rect.size.abs();

	if (!dst_rect.has_point(p_point)) {
		return false;
	}

	Vector2 q = (p_point - dst_rect.position) / dst_rect.size;
	if (hflip) {
		q.x = 1.0f - q.x;
	}
	if (vflip) {
		q.y = 1.0f - q.y;
	}
	q = q * src_rect.size + src_rect.position;
	TextureRepeat repeat_mode = get_texture_repeat_in_tree();
	bool is_repeat = repeat_mode == TEXTURE_REPEAT_ENABLED || repeat_mode == TEXTURE_REPEAT_MIRROR;
	bool is_mirrored_repeat = repeat_mode == TEXTURE_REPEAT_MIRROR;
	if (is_repeat) {
		int mirror_x = 0;
		int mirror_y = 0;
		if (is_mirrored_repeat) {
			mirror_x = (int)(q.x / texture->get_size().width);
			mirror_y = (int)(q.y / texture->get_size().height);
		}
		q.x = Math::fmod(q.x, texture->get_size().width);
		q.y = Math::fmod(q.y, texture->get_size().height);
		if (mirror_x % 2 == 1) {
			q.x = texture->get_size().width - q.x - 1;
		}
		if (mirror_y % 2 == 1) {
			q.y = texture->get_size().height - q.y - 1;
		}
	} else {
		q = q.min(texture->get_size() - Vector2(1, 1));
	}

	return texture->is_pixel_opaque((int)q.x, (int)q.y);
}

Rect2 Sprite2D::get_rect() const {
	if (texture.is_null()) {
		return Rect2(0, 0, 1, 1);
	}

	Size2i s;

	if (region_enabled) {
		s = region_rect.size;
	} else {
		s = texture->get_size();
	}

	s = s / Point2(hframes, vframes);

	Point2 ofs = offset;
	if (centered) {
		ofs -= Size2(s) / 2;
	}

	if (get_viewport() && get_viewport()->is_snap_2d_transforms_to_pixel_enabled()) {
		ofs = (ofs + Point2(0.5, 0.5)).floor();
	}

	if (s == Size2(0, 0)) {
		s = Size2(1, 1);
	}

	return Rect2(ofs, s);
}

void Sprite2D::_validate_property(PropertyInfo &p_property) const {
	if (p_property.name == "frame") {
		p_property.hint = PROPERTY_HINT_RANGE;
		p_property.hint_string = "0," + itos(vframes * hframes - 1) + ",1";
		p_property.usage |= PROPERTY_USAGE_KEYING_INCREMENTS;
	}

	if (p_property.name == "frame_coords") {
		p_property.usage |= PROPERTY_USAGE_KEYING_INCREMENTS;
	}

	if (!region_enabled && (p_property.name == "region_rect" || p_property.name == "region_filter_clip")) {
		p_property.usage = PROPERTY_USAGE_NO_EDITOR;
	}
}

void Sprite2D::_texture_changed() {
	// Changes to the texture need to trigger an update to make
	// the editor redraw the sprite with the updated texture.
	if (texture.is_valid()) {
		queue_redraw();
	}
}

void Sprite2D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_texture", "texture"), &Sprite2D::set_texture);
	ClassDB::bind_method(D_METHOD("get_texture"), &Sprite2D::get_texture);

	ClassDB::bind_method(D_METHOD("set_centered", "centered"), &Sprite2D::set_centered);
	ClassDB::bind_method(D_METHOD("is_centered"), &Sprite2D::is_centered);

	ClassDB::bind_method(D_METHOD("set_offset", "offset"), &Sprite2D::set_offset);
	ClassDB::bind_method(D_METHOD("get_offset"), &Sprite2D::get_offset);

	ClassDB::bind_method(D_METHOD("set_flip_h", "flip_h"), &Sprite2D::set_flip_h);
	ClassDB::bind_method(D_METHOD("is_flipped_h"), &Sprite2D::is_flipped_h);

	ClassDB::bind_method(D_METHOD("set_flip_v", "flip_v"), &Sprite2D::set_flip_v);
	ClassDB::bind_method(D_METHOD("is_flipped_v"), &Sprite2D::is_flipped_v);

	ClassDB::bind_method(D_METHOD("set_region_enabled", "enabled"), &Sprite2D::set_region_enabled);
	ClassDB::bind_method(D_METHOD("is_region_enabled"), &Sprite2D::is_region_enabled);

	ClassDB::bind_method(D_METHOD("is_pixel_opaque", "pos"), &Sprite2D::is_pixel_opaque);

	ClassDB::bind_method(D_METHOD("set_region_rect", "rect"), &Sprite2D::set_region_rect);
	ClassDB::bind_method(D_METHOD("get_region_rect"), &Sprite2D::get_region_rect);

	ClassDB::bind_method(D_METHOD("set_region_filter_clip_enabled", "enabled"), &Sprite2D::set_region_filter_clip_enabled);
	ClassDB::bind_method(D_METHOD("is_region_filter_clip_enabled"), &Sprite2D::is_region_filter_clip_enabled);

	ClassDB::bind_method(D_METHOD("set_frame", "frame"), &Sprite2D::set_frame);
	ClassDB::bind_method(D_METHOD("get_frame"), &Sprite2D::get_frame);

	ClassDB::bind_method(D_METHOD("set_frame_coords", "coords"), &Sprite2D::set_frame_coords);
	ClassDB::bind_method(D_METHOD("get_frame_coords"), &Sprite2D::get_frame_coords);

	ClassDB::bind_method(D_METHOD("set_vframes", "vframes"), &Sprite2D::set_vframes);
	ClassDB::bind_method(D_METHOD("get_vframes"), &Sprite2D::get_vframes);

	ClassDB::bind_method(D_METHOD("set_hframes", "hframes"), &Sprite2D::set_hframes);
	ClassDB::bind_method(D_METHOD("get_hframes"), &Sprite2D::get_hframes);

	ClassDB::bind_method(D_METHOD("get_rect"), &Sprite2D::get_rect);

	ADD_SIGNAL(MethodInfo("frame_changed"));
	ADD_SIGNAL(MethodInfo("texture_changed"));

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "texture", PROPERTY_HINT_RESOURCE_TYPE, "Texture2D"), "set_texture", "get_texture");
	ADD_GROUP("Offset", "");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "centered"), "set_centered", "is_centered");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "offset", PROPERTY_HINT_NONE, "suffix:px"), "set_offset", "get_offset");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "flip_h"), "set_flip_h", "is_flipped_h");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "flip_v"), "set_flip_v", "is_flipped_v");
	ADD_GROUP("Animation", "");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "hframes", PROPERTY_HINT_RANGE, "1,16384,1"), "set_hframes", "get_hframes");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "vframes", PROPERTY_HINT_RANGE, "1,16384,1"), "set_vframes", "get_vframes");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "frame"), "set_frame", "get_frame");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2I, "frame_coords", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_EDITOR), "set_frame_coords", "get_frame_coords");

	ADD_GROUP("Region", "region_");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "region_enabled"), "set_region_enabled", "is_region_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::RECT2, "region_rect"), "set_region_rect", "get_region_rect");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "region_filter_clip_enabled"), "set_region_filter_clip_enabled", "is_region_filter_clip_enabled");
}

Sprite2D::Sprite2D() {
}

Sprite2D::~Sprite2D() {
}

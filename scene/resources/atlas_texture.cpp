/**************************************************************************/
/*  atlas_texture.cpp                                                     */
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

#include "atlas_texture.h"

int AtlasTexture::get_width() const {
	return transpose ? get_region_height() : get_region_width();
}

int AtlasTexture::get_height() const {
	return transpose ? get_region_width() : get_region_height();
}

int AtlasTexture::get_region_width() const {
	if (region.size.width == 0) {
		if (atlas.is_valid()) {
			return atlas->get_width();
		}
		return 1;
	} else {
		return region.size.width + margin.size.width;
	}
}

int AtlasTexture::get_region_height() const {
	if (region.size.height == 0) {
		if (atlas.is_valid()) {
			return atlas->get_height();
		}
		return 1;
	} else {
		return region.size.height + margin.size.height;
	}
}

RID AtlasTexture::get_rid() const {
	if (atlas.is_valid()) {
		return atlas->get_rid();
	}

	return RID();
}

bool AtlasTexture::has_alpha() const {
	if (atlas.is_valid()) {
		return atlas->has_alpha();
	}

	return false;
}

void AtlasTexture::set_atlas(const Ref<Texture2D> &p_atlas) {
	ERR_FAIL_COND(p_atlas == this);
	if (atlas == p_atlas) {
		return;
	}
	// Support recursive AtlasTextures.
	if (Ref<AtlasTexture>(atlas).is_valid()) {
		atlas->disconnect_changed(callable_mp((Resource *)this, &AtlasTexture::emit_changed));
	}
	atlas = p_atlas;
	if (Ref<AtlasTexture>(atlas).is_valid()) {
		atlas->connect_changed(callable_mp((Resource *)this, &AtlasTexture::emit_changed));
	}

	emit_changed();
}

Ref<Texture2D> AtlasTexture::get_atlas() const {
	return atlas;
}

void AtlasTexture::set_region(const Rect2 &p_region) {
	if (region == p_region) {
		return;
	}
	region = p_region;
	emit_changed();
}

Rect2 AtlasTexture::get_region() const {
	return region;
}

void AtlasTexture::set_margin(const Rect2 &p_margin) {
	if (margin == p_margin) {
		return;
	}
	margin = p_margin;
	emit_changed();
}

Rect2 AtlasTexture::get_margin() const {
	return margin;
}

void AtlasTexture::set_flip_h(const bool p_flip) {
	if (hflip == p_flip) {
		return;
	}
	hflip = p_flip;
	emit_changed();
}

bool AtlasTexture::is_flipped_h() const {
	return hflip;
}

void AtlasTexture::set_flip_v(const bool p_flip) {
	if (vflip == p_flip) {
		return;
	}
	vflip = p_flip;
	emit_changed();
}

bool AtlasTexture::is_flipped_v() const {
	return vflip;
}

void AtlasTexture::set_transpose(const bool p_transpose) {
	if (transpose == p_transpose) {
		return;
	}
	transpose = p_transpose;
	emit_changed();
}

bool AtlasTexture::is_transposed() const {
	return transpose;
}

void AtlasTexture::set_filter_clip(const bool p_enable) {
	filter_clip = p_enable;
	emit_changed();
}

bool AtlasTexture::has_filter_clip() const {
	return filter_clip;
}

Rect2 AtlasTexture::_get_region_rect() const {
	Rect2 rc = region;
	if (atlas.is_valid()) {
		if (rc.size.width == 0) {
			rc.size.width = atlas->get_width();
		}
		if (rc.size.height == 0) {
			rc.size.height = atlas->get_height();
		}
	}
	return rc;
}

void AtlasTexture::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_atlas", "atlas"), &AtlasTexture::set_atlas);
	ClassDB::bind_method(D_METHOD("get_atlas"), &AtlasTexture::get_atlas);

	ClassDB::bind_method(D_METHOD("set_region", "region"), &AtlasTexture::set_region);
	ClassDB::bind_method(D_METHOD("get_region"), &AtlasTexture::get_region);

	ClassDB::bind_method(D_METHOD("set_margin", "margin"), &AtlasTexture::set_margin);
	ClassDB::bind_method(D_METHOD("get_margin"), &AtlasTexture::get_margin);

	ClassDB::bind_method(D_METHOD("set_flip_h", "flip_h"), &AtlasTexture::set_flip_h);
	ClassDB::bind_method(D_METHOD("is_flipped_h"), &AtlasTexture::is_flipped_h);

	ClassDB::bind_method(D_METHOD("set_flip_v", "flip_v"), &AtlasTexture::set_flip_v);
	ClassDB::bind_method(D_METHOD("is_flipped_v"), &AtlasTexture::is_flipped_v);

	ClassDB::bind_method(D_METHOD("set_transpose", "transpose"), &AtlasTexture::set_transpose);
	ClassDB::bind_method(D_METHOD("is_transposed"), &AtlasTexture::is_transposed);

	ClassDB::bind_method(D_METHOD("set_filter_clip", "enable"), &AtlasTexture::set_filter_clip);
	ClassDB::bind_method(D_METHOD("has_filter_clip"), &AtlasTexture::has_filter_clip);

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "atlas", PROPERTY_HINT_RESOURCE_TYPE, "Texture2D"), "set_atlas", "get_atlas");
	ADD_PROPERTY(PropertyInfo(Variant::RECT2, "region", PROPERTY_HINT_NONE, "suffix:px"), "set_region", "get_region");
	ADD_PROPERTY(PropertyInfo(Variant::RECT2, "margin", PROPERTY_HINT_NONE, "suffix:px"), "set_margin", "get_margin");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "flip_h"), "set_flip_h", "is_flipped_h");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "flip_v"), "set_flip_v", "is_flipped_v");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "transpose"), "set_transpose", "is_transposed");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "filter_clip"), "set_filter_clip", "has_filter_clip");
}

void AtlasTexture::draw_oriented(RID p_canvas_item, const Rect2 p_draw, const Rect2 p_source, const Color &p_modulate, bool p_transpose) const {
	Rect2 source = p_source;

	if (hflip) {
		source.size.x = -source.size.x;
	}

	if (vflip) {
		source.size.y = -source.size.y;
	}

	atlas->draw_rect_region(p_canvas_item, p_draw, source, p_modulate, p_transpose ? !transpose : transpose, filter_clip);
}

void AtlasTexture::draw(RID p_canvas_item, const Point2 &p_pos, const Color &p_modulate, bool p_transpose) const {
	if (atlas.is_null()) {
		return;
	}
	const Rect2 rc = _get_region_rect();
	draw_oriented(p_canvas_item, Rect2(p_pos + margin.position, rc.size), rc, p_modulate, p_transpose);
}

void AtlasTexture::draw_rect(RID p_canvas_item, const Rect2 &p_rect, bool p_tile, const Color &p_modulate, bool p_transpose) const {
	if (atlas.is_null()) {
		return;
	}

	Rect2 src_rect = Rect2(0, 0, get_width(), get_height());

	Rect2 dr;
	Rect2 src_c;
	if (get_rect_region(p_rect, src_rect, dr, src_c)) {
		draw_oriented(p_canvas_item, dr, src_c, p_modulate, p_transpose);
	}
}

void AtlasTexture::draw_rect_region(RID p_canvas_item, const Rect2 &p_rect, const Rect2 &p_src_rect, const Color &p_modulate, bool p_transpose, bool p_clip_uv) const {
	// This might not necessarily work well if using a rect, needs to be fixed properly.
	if (atlas.is_null()) {
		return;
	}

	Rect2 dr;
	Rect2 src_c;
	if (get_rect_region(p_rect, p_src_rect, dr, src_c)) {
		draw_oriented(p_canvas_item, dr, src_c, p_modulate, p_transpose);
	}
}

bool AtlasTexture::get_rect_region(const Rect2 &p_rect, const Rect2 &p_src_rect, Rect2 &r_rect, Rect2 &r_src_rect) const {
	if (atlas.is_null()) {
		return false;
	}

	Rect2 src = p_src_rect;
	if (src.size == Size2()) {
		src.size = region.size;
	}
	if (src.size == Size2() && atlas.is_valid()) {
		src.size = atlas->get_size();
	}
	Vector2 scale = p_rect.size / src.size;

	src.position += (region.position - margin.position);

	Rect2 clip_target = src;
	if (transpose) {
		clip_target.size = Vector2(clip_target.size.y, clip_target.size.x);
	}

	Rect2 src_clipped = _get_region_rect().intersection(clip_target);
	if (src_clipped.size == Size2()) {
		return false;
	}

	Vector2 ofs = (src_clipped.position - src.position);
	if (scale.x < 0) {
		ofs.x += (src_clipped.size.x - src.size.x);
	}
	if (scale.y < 0) {
		ofs.y += (src_clipped.size.y - src.size.y);
	}

	r_rect = Rect2(p_rect.position + ofs * scale, src_clipped.size * scale);
	r_src_rect = src_clipped;
	return true;
}

bool AtlasTexture::is_pixel_opaque(int p_x, int p_y) const {
	if (atlas.is_null()) {
		return true;
	}

	int x = region.position.x - margin.position.x;
	int y = region.position.y - margin.position.y;

	if (transpose) {
		if (hflip) {
			y += region.size.y - p_x;
		} else {
			y += p_x;
		}

		if (vflip) {
			x += region.size.x - p_y;
		} else {
			x += p_y;
		}
	} else {
		if (hflip) {
			x += region.size.x - p_x;
		} else {
			x += p_x;
		}

		if (vflip) {
			y += region.size.y - p_y;
		} else {
			y += p_y;
		}
	}

	// Margin edge may outside of atlas.
	if (x < 0 || x >= atlas->get_width()) {
		return false;
	}
	if (y < 0 || y >= atlas->get_height()) {
		return false;
	}

	return atlas->is_pixel_opaque(x, y);
}

Ref<Image> AtlasTexture::get_image() const {
	if (atlas.is_null()) {
		return Ref<Image>();
	}

	const Ref<Image> &atlas_image = atlas->get_image();
	if (atlas_image.is_null()) {
		return Ref<Image>();
	}

	return atlas_image->get_region(_get_region_rect());
}

AtlasTexture::AtlasTexture() {}

/**************************************************************************/
/*  texture_rect.cpp                                                      */
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

#include "texture_rect.h"

void TextureRect::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_DRAW: {
			if (texture.is_null()) {
				return;
			}

			Size2 size;
			Point2 offset;
			Rect2 region;
			bool tile = false;

			switch (stretch_mode) {
				case STRETCH_SCALE: {
					size = get_size();
				} break;
				case STRETCH_TILE: {
					size = get_size();
					tile = true;
				} break;
				case STRETCH_KEEP: {
					size = texture->get_size();
				} break;
				case STRETCH_KEEP_CENTERED: {
					offset = (get_size() - texture->get_size()) / 2;
					size = texture->get_size();
				} break;
				case STRETCH_KEEP_ASPECT_CENTERED:
				case STRETCH_KEEP_ASPECT: {
					size = get_size();
					int tex_width = texture->get_width() * size.height / texture->get_height();
					int tex_height = size.height;

					if (tex_width > size.width) {
						tex_width = size.width;
						tex_height = texture->get_height() * tex_width / texture->get_width();
					}

					if (stretch_mode == STRETCH_KEEP_ASPECT_CENTERED) {
						offset.x += (size.width - tex_width) / 2;
						offset.y += (size.height - tex_height) / 2;
					}

					size.width = tex_width;
					size.height = tex_height;
				} break;
				case STRETCH_KEEP_ASPECT_COVERED: {
					size = get_size();

					Size2 tex_size = texture->get_size();
					Size2 scale_size(size.width / tex_size.width, size.height / tex_size.height);
					float scale = scale_size.width > scale_size.height ? scale_size.width : scale_size.height;
					Size2 scaled_tex_size = tex_size * scale;

					region.position = ((scaled_tex_size - size) / scale).abs() / 2.0f;
					region.size = size / scale;
				} break;
			}

			size.width *= hflip ? -1.0f : 1.0f;
			size.height *= vflip ? -1.0f : 1.0f;

			if (region.has_area()) {
				draw_texture_rect_region(texture, Rect2(offset, size), region);
			} else {
				draw_texture_rect(texture, Rect2(offset, size), tile);
			}
		} break;
		case NOTIFICATION_RESIZED: {
			update_minimum_size();
		} break;
	}
}

Size2 TextureRect::get_minimum_size() const {
	if (texture.is_valid()) {
		switch (expand_mode) {
			case EXPAND_KEEP_SIZE: {
				return texture->get_size();
			} break;
			case EXPAND_IGNORE_SIZE: {
				return Size2();
			} break;
			case EXPAND_FIT_WIDTH: {
				return Size2(get_size().y, 0);
			} break;
			case EXPAND_FIT_WIDTH_PROPORTIONAL: {
				real_t ratio = real_t(texture->get_width()) / texture->get_height();
				return Size2(get_size().y * ratio, 0);
			} break;
			case EXPAND_FIT_HEIGHT: {
				return Size2(0, get_size().x);
			} break;
			case EXPAND_FIT_HEIGHT_PROPORTIONAL: {
				real_t ratio = real_t(texture->get_height()) / texture->get_width();
				return Size2(0, get_size().x * ratio);
			} break;
		}
	}
	return Size2();
}

void TextureRect::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_texture", "texture"), &TextureRect::set_texture);
	ClassDB::bind_method(D_METHOD("get_texture"), &TextureRect::get_texture);
	ClassDB::bind_method(D_METHOD("set_expand_mode", "expand_mode"), &TextureRect::set_expand_mode);
	ClassDB::bind_method(D_METHOD("get_expand_mode"), &TextureRect::get_expand_mode);
	ClassDB::bind_method(D_METHOD("set_flip_h", "enable"), &TextureRect::set_flip_h);
	ClassDB::bind_method(D_METHOD("is_flipped_h"), &TextureRect::is_flipped_h);
	ClassDB::bind_method(D_METHOD("set_flip_v", "enable"), &TextureRect::set_flip_v);
	ClassDB::bind_method(D_METHOD("is_flipped_v"), &TextureRect::is_flipped_v);
	ClassDB::bind_method(D_METHOD("set_stretch_mode", "stretch_mode"), &TextureRect::set_stretch_mode);
	ClassDB::bind_method(D_METHOD("get_stretch_mode"), &TextureRect::get_stretch_mode);

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "texture", PROPERTY_HINT_RESOURCE_TYPE, "Texture2D"), "set_texture", "get_texture");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "expand_mode", PROPERTY_HINT_ENUM, "Keep Size,Ignore Size,Fit Width,Fit Width Proportional,Fit Height,Fit Height Proportional"), "set_expand_mode", "get_expand_mode");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "stretch_mode", PROPERTY_HINT_ENUM, "Scale,Tile,Keep,Keep Centered,Keep Aspect,Keep Aspect Centered,Keep Aspect Covered"), "set_stretch_mode", "get_stretch_mode");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "flip_h"), "set_flip_h", "is_flipped_h");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "flip_v"), "set_flip_v", "is_flipped_v");

	BIND_ENUM_CONSTANT(EXPAND_KEEP_SIZE);
	BIND_ENUM_CONSTANT(EXPAND_IGNORE_SIZE);
	BIND_ENUM_CONSTANT(EXPAND_FIT_WIDTH);
	BIND_ENUM_CONSTANT(EXPAND_FIT_WIDTH_PROPORTIONAL);
	BIND_ENUM_CONSTANT(EXPAND_FIT_HEIGHT);
	BIND_ENUM_CONSTANT(EXPAND_FIT_HEIGHT_PROPORTIONAL);

	BIND_ENUM_CONSTANT(STRETCH_SCALE);
	BIND_ENUM_CONSTANT(STRETCH_TILE);
	BIND_ENUM_CONSTANT(STRETCH_KEEP);
	BIND_ENUM_CONSTANT(STRETCH_KEEP_CENTERED);
	BIND_ENUM_CONSTANT(STRETCH_KEEP_ASPECT);
	BIND_ENUM_CONSTANT(STRETCH_KEEP_ASPECT_CENTERED);
	BIND_ENUM_CONSTANT(STRETCH_KEEP_ASPECT_COVERED);
}

#ifndef DISABLE_DEPRECATED
bool TextureRect::_set(const StringName &p_name, const Variant &p_value) {
	if ((p_name == SNAME("expand") || p_name == SNAME("ignore_texture_size")) && p_value.operator bool()) {
		expand_mode = EXPAND_IGNORE_SIZE;
		return true;
	}
	return false;
}
#endif

void TextureRect::_texture_changed() {
	queue_redraw();
	update_minimum_size();
}

void TextureRect::set_texture(const Ref<Texture2D> &p_tex) {
	if (p_tex == texture) {
		return;
	}

	if (texture.is_valid()) {
		texture->disconnect_changed(callable_mp(this, &TextureRect::_texture_changed));
	}

	texture = p_tex;

	if (texture.is_valid()) {
		texture->connect_changed(callable_mp(this, &TextureRect::_texture_changed));
	}

	queue_redraw();
	update_minimum_size();
}

Ref<Texture2D> TextureRect::get_texture() const {
	return texture;
}

void TextureRect::set_expand_mode(ExpandMode p_mode) {
	if (expand_mode == p_mode) {
		return;
	}

	expand_mode = p_mode;
	queue_redraw();
	update_minimum_size();
}

TextureRect::ExpandMode TextureRect::get_expand_mode() const {
	return expand_mode;
}

void TextureRect::set_stretch_mode(StretchMode p_mode) {
	if (stretch_mode == p_mode) {
		return;
	}

	stretch_mode = p_mode;
	queue_redraw();
}

TextureRect::StretchMode TextureRect::get_stretch_mode() const {
	return stretch_mode;
}

void TextureRect::set_flip_h(bool p_flip) {
	if (hflip == p_flip) {
		return;
	}

	hflip = p_flip;
	queue_redraw();
}

bool TextureRect::is_flipped_h() const {
	return hflip;
}

void TextureRect::set_flip_v(bool p_flip) {
	if (vflip == p_flip) {
		return;
	}

	vflip = p_flip;
	queue_redraw();
}

bool TextureRect::is_flipped_v() const {
	return vflip;
}

TextureRect::TextureRect() {
	set_mouse_filter(MOUSE_FILTER_PASS);
}

TextureRect::~TextureRect() {
}

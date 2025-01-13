/**************************************************************************/
/*  texture_button.cpp                                                    */
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

#include "texture_button.h"

#include "core/typedefs.h"

#include <stdlib.h>

Size2 TextureButton::get_minimum_size() const {
	Size2 rscale = Control::get_minimum_size();

	if (!ignore_texture_size) {
		if (normal.is_null()) {
			if (pressed.is_null()) {
				if (hover.is_null()) {
					if (click_mask.is_null()) {
						rscale = Size2();
					} else {
						rscale = click_mask->get_size();
					}
				} else {
					rscale = hover->get_size();
				}
			} else {
				rscale = pressed->get_size();
			}

		} else {
			rscale = normal->get_size();
		}
	}

	return rscale.abs();
}

bool TextureButton::has_point(const Point2 &p_point) const {
	if (click_mask.is_valid()) {
		Point2 point = p_point;
		Rect2 rect;
		Size2 mask_size = click_mask->get_size();

		if (!_position_rect.has_area()) {
			rect.size = mask_size;
		} else if (_tile) {
			// if the stretch mode is tile we offset the point to keep it inside the mask size
			rect.size = mask_size;
			if (_position_rect.has_point(point)) {
				int cols = (int)Math::ceil(_position_rect.size.x / mask_size.x);
				int rows = (int)Math::ceil(_position_rect.size.y / mask_size.y);
				int col = (int)(point.x / mask_size.x) % cols;
				int row = (int)(point.y / mask_size.y) % rows;
				point.x -= mask_size.x * col;
				point.y -= mask_size.y * row;
			}
		} else {
			// we need to transform the point from our scaled / translated image back to our mask image
			Point2 ofs = _position_rect.position;
			Size2 scale = mask_size / _position_rect.size;

			switch (stretch_mode) {
				case STRETCH_KEEP_ASPECT_COVERED: {
					// if the stretch mode is aspect covered the image uses a texture region so we need to take that into account
					float min = MIN(scale.x, scale.y);
					scale.x = min;
					scale.y = min;
					ofs -= _texture_region.position / min;
				} break;
				default: {
					// FIXME: Why a switch if we only handle one enum value?
				}
			}

			// offset and scale the new point position to adjust it to the bitmask size
			point -= ofs;
			point *= scale;

			// finally, we need to check if the point is inside a rectangle with a position >= 0,0 and a size <= mask_size
			rect.position = _texture_region.position.maxf(0);
			rect.size = mask_size.min(_texture_region.size);
		}

		if (!rect.has_point(point)) {
			return false;
		}

		Point2i p = point;
		return click_mask->get_bitv(p);
	}

	return Control::has_point(p_point);
}

void TextureButton::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_DRAW: {
			DrawMode draw_mode = get_draw_mode();

			Ref<Texture2D> texdraw;

			switch (draw_mode) {
				case DRAW_NORMAL: {
					if (normal.is_valid()) {
						texdraw = normal;
					}
				} break;
				case DRAW_HOVER_PRESSED:
				case DRAW_PRESSED: {
					if (pressed.is_null()) {
						if (hover.is_null()) {
							if (normal.is_valid()) {
								texdraw = normal;
							}
						} else {
							texdraw = hover;
						}

					} else {
						texdraw = pressed;
					}
				} break;
				case DRAW_HOVER: {
					if (hover.is_null()) {
						if (pressed.is_valid() && is_pressed()) {
							texdraw = pressed;
						} else if (normal.is_valid()) {
							texdraw = normal;
						}
					} else {
						texdraw = hover;
					}
				} break;
				case DRAW_DISABLED: {
					if (disabled.is_null()) {
						if (normal.is_valid()) {
							texdraw = normal;
						}
					} else {
						texdraw = disabled;
					}
				} break;
			}

			Point2 ofs;
			Size2 size;
			bool draw_focus = (has_focus() && focused.is_valid());

			// If no other texture is valid, try using focused texture.
			bool draw_focus_only = draw_focus && !texdraw.is_valid();
			if (draw_focus_only) {
				texdraw = focused;
			}

			if (texdraw.is_valid() || click_mask.is_valid()) {
				const Size2 texdraw_size = texdraw.is_valid() ? texdraw->get_size() : Size2(click_mask->get_size());

				size = texdraw_size;
				_texture_region = Rect2(Point2(), texdraw_size);
				_tile = false;
				switch (stretch_mode) {
					case STRETCH_KEEP:
						break;
					case STRETCH_SCALE:
						size = get_size();
						break;
					case STRETCH_TILE:
						size = get_size();
						_tile = true;
						break;
					case STRETCH_KEEP_CENTERED:
						ofs = (get_size() - texdraw_size) / 2;
						break;
					case STRETCH_KEEP_ASPECT_CENTERED:
					case STRETCH_KEEP_ASPECT: {
						Size2 _size = get_size();
						float tex_width = texdraw_size.width * _size.height / texdraw_size.height;
						float tex_height = _size.height;

						if (tex_width > _size.width) {
							tex_width = _size.width;
							tex_height = texdraw_size.height * tex_width / texdraw_size.width;
						}

						if (stretch_mode == STRETCH_KEEP_ASPECT_CENTERED) {
							ofs.x = (_size.width - tex_width) / 2;
							ofs.y = (_size.height - tex_height) / 2;
						}
						size.width = tex_width;
						size.height = tex_height;
					} break;
					case STRETCH_KEEP_ASPECT_COVERED: {
						size = get_size();
						Size2 scale_size = size / texdraw_size;
						float scale = scale_size.width > scale_size.height ? scale_size.width : scale_size.height;
						Size2 scaled_tex_size = texdraw_size * scale;
						Point2 ofs2 = ((scaled_tex_size - size) / scale).abs() / 2.0f;
						_texture_region = Rect2(ofs2, size / scale);
					} break;
				}

				_position_rect = Rect2(ofs, size);

				size.width *= hflip ? -1.0f : 1.0f;
				size.height *= vflip ? -1.0f : 1.0f;

				if (draw_focus_only) {
					// Do nothing, we only needed to calculate the rectangle.
				} else if (texdraw.is_valid()) {
					if (_tile) {
						draw_texture_rect(texdraw, Rect2(ofs, size), _tile);
					} else {
						draw_texture_rect_region(texdraw, Rect2(ofs, size), _texture_region);
					}
				}
			} else {
				_position_rect = Rect2();
			}

			if (draw_focus) {
				draw_texture_rect(focused, Rect2(ofs, size), false);
			};
		} break;
	}
}

void TextureButton::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_texture_normal", "texture"), &TextureButton::set_texture_normal);
	ClassDB::bind_method(D_METHOD("set_texture_pressed", "texture"), &TextureButton::set_texture_pressed);
	ClassDB::bind_method(D_METHOD("set_texture_hover", "texture"), &TextureButton::set_texture_hover);
	ClassDB::bind_method(D_METHOD("set_texture_disabled", "texture"), &TextureButton::set_texture_disabled);
	ClassDB::bind_method(D_METHOD("set_texture_focused", "texture"), &TextureButton::set_texture_focused);
	ClassDB::bind_method(D_METHOD("set_click_mask", "mask"), &TextureButton::set_click_mask);
	ClassDB::bind_method(D_METHOD("set_ignore_texture_size", "ignore"), &TextureButton::set_ignore_texture_size);
	ClassDB::bind_method(D_METHOD("set_stretch_mode", "mode"), &TextureButton::set_stretch_mode);
	ClassDB::bind_method(D_METHOD("set_flip_h", "enable"), &TextureButton::set_flip_h);
	ClassDB::bind_method(D_METHOD("is_flipped_h"), &TextureButton::is_flipped_h);
	ClassDB::bind_method(D_METHOD("set_flip_v", "enable"), &TextureButton::set_flip_v);
	ClassDB::bind_method(D_METHOD("is_flipped_v"), &TextureButton::is_flipped_v);

	ClassDB::bind_method(D_METHOD("get_texture_normal"), &TextureButton::get_texture_normal);
	ClassDB::bind_method(D_METHOD("get_texture_pressed"), &TextureButton::get_texture_pressed);
	ClassDB::bind_method(D_METHOD("get_texture_hover"), &TextureButton::get_texture_hover);
	ClassDB::bind_method(D_METHOD("get_texture_disabled"), &TextureButton::get_texture_disabled);
	ClassDB::bind_method(D_METHOD("get_texture_focused"), &TextureButton::get_texture_focused);
	ClassDB::bind_method(D_METHOD("get_click_mask"), &TextureButton::get_click_mask);
	ClassDB::bind_method(D_METHOD("get_ignore_texture_size"), &TextureButton::get_ignore_texture_size);
	ClassDB::bind_method(D_METHOD("get_stretch_mode"), &TextureButton::get_stretch_mode);

	ADD_GROUP("Textures", "texture_");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "texture_normal", PROPERTY_HINT_RESOURCE_TYPE, "Texture2D"), "set_texture_normal", "get_texture_normal");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "texture_pressed", PROPERTY_HINT_RESOURCE_TYPE, "Texture2D"), "set_texture_pressed", "get_texture_pressed");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "texture_hover", PROPERTY_HINT_RESOURCE_TYPE, "Texture2D"), "set_texture_hover", "get_texture_hover");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "texture_disabled", PROPERTY_HINT_RESOURCE_TYPE, "Texture2D"), "set_texture_disabled", "get_texture_disabled");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "texture_focused", PROPERTY_HINT_RESOURCE_TYPE, "Texture2D"), "set_texture_focused", "get_texture_focused");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "texture_click_mask", PROPERTY_HINT_RESOURCE_TYPE, "BitMap"), "set_click_mask", "get_click_mask");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "ignore_texture_size", PROPERTY_HINT_RESOURCE_TYPE, "bool"), "set_ignore_texture_size", "get_ignore_texture_size");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "stretch_mode", PROPERTY_HINT_ENUM, "Scale,Tile,Keep,Keep Centered,Keep Aspect,Keep Aspect Centered,Keep Aspect Covered"), "set_stretch_mode", "get_stretch_mode");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "flip_h", PROPERTY_HINT_RESOURCE_TYPE, "bool"), "set_flip_h", "is_flipped_h");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "flip_v", PROPERTY_HINT_RESOURCE_TYPE, "bool"), "set_flip_v", "is_flipped_v");

	BIND_ENUM_CONSTANT(STRETCH_SCALE);
	BIND_ENUM_CONSTANT(STRETCH_TILE);
	BIND_ENUM_CONSTANT(STRETCH_KEEP);
	BIND_ENUM_CONSTANT(STRETCH_KEEP_CENTERED);
	BIND_ENUM_CONSTANT(STRETCH_KEEP_ASPECT);
	BIND_ENUM_CONSTANT(STRETCH_KEEP_ASPECT_CENTERED);
	BIND_ENUM_CONSTANT(STRETCH_KEEP_ASPECT_COVERED);
}

void TextureButton::set_texture_normal(const Ref<Texture2D> &p_normal) {
	_set_texture(&normal, p_normal);
}

void TextureButton::set_texture_pressed(const Ref<Texture2D> &p_pressed) {
	_set_texture(&pressed, p_pressed);
}

void TextureButton::set_texture_hover(const Ref<Texture2D> &p_hover) {
	_set_texture(&hover, p_hover);
}

void TextureButton::set_texture_disabled(const Ref<Texture2D> &p_disabled) {
	_set_texture(&disabled, p_disabled);
}

void TextureButton::set_click_mask(const Ref<BitMap> &p_click_mask) {
	if (click_mask == p_click_mask) {
		return;
	}
	click_mask = p_click_mask;
	_texture_changed();
}

Ref<Texture2D> TextureButton::get_texture_normal() const {
	return normal;
}

Ref<Texture2D> TextureButton::get_texture_pressed() const {
	return pressed;
}

Ref<Texture2D> TextureButton::get_texture_hover() const {
	return hover;
}

Ref<Texture2D> TextureButton::get_texture_disabled() const {
	return disabled;
}

Ref<BitMap> TextureButton::get_click_mask() const {
	return click_mask;
}

Ref<Texture2D> TextureButton::get_texture_focused() const {
	return focused;
}

void TextureButton::set_texture_focused(const Ref<Texture2D> &p_focused) {
	_set_texture(&focused, p_focused);
}

void TextureButton::_set_texture(Ref<Texture2D> *p_destination, const Ref<Texture2D> &p_texture) {
	DEV_ASSERT(p_destination);
	Ref<Texture2D> &destination = *p_destination;
	if (destination == p_texture) {
		return;
	}
	if (destination.is_valid()) {
		destination->disconnect_changed(callable_mp(this, &TextureButton::_texture_changed));
	}
	destination = p_texture;
	if (destination.is_valid()) {
		// Pass `CONNECT_REFERENCE_COUNTED` to avoid early disconnect in case the same texture is assigned to different "slots".
		destination->connect_changed(callable_mp(this, &TextureButton::_texture_changed), CONNECT_REFERENCE_COUNTED);
	}
	_texture_changed();
}

void TextureButton::_texture_changed() {
	queue_redraw();
	update_minimum_size();
}

bool TextureButton::get_ignore_texture_size() const {
	return ignore_texture_size;
}

void TextureButton::set_ignore_texture_size(bool p_ignore) {
	if (ignore_texture_size == p_ignore) {
		return;
	}

	ignore_texture_size = p_ignore;
	update_minimum_size();
	queue_redraw();
}

void TextureButton::set_stretch_mode(StretchMode p_stretch_mode) {
	if (stretch_mode == p_stretch_mode) {
		return;
	}

	stretch_mode = p_stretch_mode;
	queue_redraw();
}

TextureButton::StretchMode TextureButton::get_stretch_mode() const {
	return stretch_mode;
}

void TextureButton::set_flip_h(bool p_flip) {
	if (hflip == p_flip) {
		return;
	}

	hflip = p_flip;
	queue_redraw();
}

bool TextureButton::is_flipped_h() const {
	return hflip;
}

void TextureButton::set_flip_v(bool p_flip) {
	if (vflip == p_flip) {
		return;
	}

	vflip = p_flip;
	queue_redraw();
}

bool TextureButton::is_flipped_v() const {
	return vflip;
}

TextureButton::TextureButton() {}

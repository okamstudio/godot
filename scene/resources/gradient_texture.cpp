/**************************************************************************/
/*  gradient_texture.cpp                                                  */
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

#include "gradient_texture.h"

#include "core/math/geometry_2d.h"

GradientTexture1D::GradientTexture1D() {
	_queue_update();
}

GradientTexture1D::~GradientTexture1D() {
	if (texture.is_valid()) {
		ERR_FAIL_NULL(RenderingServer::get_singleton());
		RS::get_singleton()->free(texture);
	}
}

void GradientTexture1D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_gradient", "gradient"), &GradientTexture1D::set_gradient);
	ClassDB::bind_method(D_METHOD("get_gradient"), &GradientTexture1D::get_gradient);

	ClassDB::bind_method(D_METHOD("set_width", "width"), &GradientTexture1D::set_width);
	// The `get_width()` method is already exposed by the parent class Texture2D.

	ClassDB::bind_method(D_METHOD("set_use_hdr", "enabled"), &GradientTexture1D::set_use_hdr);
	ClassDB::bind_method(D_METHOD("is_using_hdr"), &GradientTexture1D::is_using_hdr);

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "gradient", PROPERTY_HINT_RESOURCE_TYPE, "Gradient", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_EDITOR_INSTANTIATE_OBJECT), "set_gradient", "get_gradient");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "width", PROPERTY_HINT_RANGE, "1,16384,suffix:px"), "set_width", "get_width");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "use_hdr"), "set_use_hdr", "is_using_hdr");
}

void GradientTexture1D::set_gradient(Ref<Gradient> p_gradient) {
	if (p_gradient == gradient) {
		return;
	}
	if (gradient.is_valid()) {
		gradient->disconnect_changed(callable_mp(this, &GradientTexture1D::_queue_update));
	}
	gradient = p_gradient;
	if (gradient.is_valid()) {
		gradient->connect_changed(callable_mp(this, &GradientTexture1D::_queue_update));
	}
	_queue_update();
}

Ref<Gradient> GradientTexture1D::get_gradient() const {
	return gradient;
}

void GradientTexture1D::_queue_update() {
	if (update_pending) {
		return;
	}
	update_pending = true;
	callable_mp(this, &GradientTexture1D::_update).call_deferred();
}

void GradientTexture1D::_update_now() const {
	update_pending = false;

	if (gradient.is_null()) {
		return;
	}

	if (use_hdr) {
		// High dynamic range.
		Ref<Image> image = memnew(Image(width, 1, false, Image::FORMAT_RGBAF));
		Gradient &g = **gradient;
		// `create()` isn't available for non-uint8_t data, so fill in the data manually.
		for (int i = 0; i < width; i++) {
			float ofs = float(i) / (width - 1);
			image->set_pixel(i, 0, g.get_color_at_offset(ofs));
		}

		if (texture.is_valid()) {
			RID new_texture = RS::get_singleton()->texture_2d_create(image);
			RS::get_singleton()->texture_replace(texture, new_texture);
		} else {
			texture = RS::get_singleton()->texture_2d_create(image);
		}
	} else {
		// Low dynamic range. "Overbright" colors will be clamped.
		Vector<uint8_t> data;
		data.resize(width * 4);
		{
			uint8_t *wd8 = data.ptrw();
			Gradient &g = **gradient;

			for (int i = 0; i < width; i++) {
				float ofs = float(i) / (width - 1);
				Color color = g.get_color_at_offset(ofs);

				wd8[i * 4 + 0] = uint8_t(CLAMP(color.r * 255.0, 0, 255));
				wd8[i * 4 + 1] = uint8_t(CLAMP(color.g * 255.0, 0, 255));
				wd8[i * 4 + 2] = uint8_t(CLAMP(color.b * 255.0, 0, 255));
				wd8[i * 4 + 3] = uint8_t(CLAMP(color.a * 255.0, 0, 255));
			}
		}

		Ref<Image> image = memnew(Image(width, 1, false, Image::FORMAT_RGBA8, data));

		if (texture.is_valid()) {
			RID new_texture = RS::get_singleton()->texture_2d_create(image);
			RS::get_singleton()->texture_replace(texture, new_texture);
		} else {
			texture = RS::get_singleton()->texture_2d_create(image);
		}
	}
	RS::get_singleton()->texture_set_path(texture, get_path());
}

void GradientTexture1D::_update() {
	_update_now();
	emit_changed();
}

void GradientTexture1D::set_width(int p_width) {
	ERR_FAIL_COND_MSG(p_width <= 0 || p_width > 16384, "Texture dimensions have to be within 1 to 16384 range.");
	width = p_width;
	_queue_update();
}

int GradientTexture1D::get_width() const {
	return width;
}

void GradientTexture1D::set_use_hdr(bool p_enabled) {
	if (p_enabled == use_hdr) {
		return;
	}

	use_hdr = p_enabled;
	_queue_update();
}

bool GradientTexture1D::is_using_hdr() const {
	return use_hdr;
}

RID GradientTexture1D::get_rid() const {
	if (texture.is_null()) {
		texture = RS::get_singleton()->texture_2d_placeholder_create();
	}
	return texture;
}

Ref<Image> GradientTexture1D::get_image() const {
	if (update_pending) {
		_update_now();
	}
	if (texture.is_null()) {
		return Ref<Image>();
	}
	return RenderingServer::get_singleton()->texture_2d_get(texture);
}

//////////////////

GradientTexture2D::GradientTexture2D() {
	_queue_update();
}

GradientTexture2D::~GradientTexture2D() {
	if (texture.is_valid()) {
		ERR_FAIL_NULL(RenderingServer::get_singleton());
		RS::get_singleton()->free(texture);
	}
}

void GradientTexture2D::set_gradient(Ref<Gradient> p_gradient) {
	if (gradient == p_gradient) {
		return;
	}
	if (gradient.is_valid()) {
		gradient->disconnect_changed(callable_mp(this, &GradientTexture2D::_queue_update));
	}
	gradient = p_gradient;
	if (gradient.is_valid()) {
		gradient->connect_changed(callable_mp(this, &GradientTexture2D::_queue_update));
	}
	_queue_update();
}

Ref<Gradient> GradientTexture2D::get_gradient() const {
	return gradient;
}

void GradientTexture2D::_queue_update() {
	if (update_pending) {
		return;
	}
	update_pending = true;
	callable_mp(this, &GradientTexture2D::_update).call_deferred();
}

void GradientTexture2D::_update_now() const {
	update_pending = false;

	if (gradient.is_null()) {
		return;
	}
	Ref<Image> image;
	image.instantiate();

	if (gradient->get_point_count() <= 1) { // No need to interpolate.
		image->initialize_data(width, height, false, (use_hdr) ? Image::FORMAT_RGBAF : Image::FORMAT_RGBA8);
		image->fill((gradient->get_point_count() == 1) ? gradient->get_color(0) : Color(0, 0, 0, 1));
	} else {
		if (use_hdr) {
			image->initialize_data(width, height, false, Image::FORMAT_RGBAF);
			Gradient &g = **gradient;
			// `create()` isn't available for non-uint8_t data, so fill in the data manually.
			for (int y = 0; y < height; y++) {
				for (int x = 0; x < width; x++) {
					float ofs = _get_gradient_offset_at(x, y);
					image->set_pixel(x, y, g.get_color_at_offset(ofs));
				}
			}
		} else {
			Vector<uint8_t> data;
			data.resize(width * height * 4);
			{
				uint8_t *wd8 = data.ptrw();
				Gradient &g = **gradient;
				for (int y = 0; y < height; y++) {
					for (int x = 0; x < width; x++) {
						float ofs = _get_gradient_offset_at(x, y);
						const Color &c = g.get_color_at_offset(ofs);

						wd8[(x + (y * width)) * 4 + 0] = uint8_t(CLAMP(c.r * 255.0, 0, 255));
						wd8[(x + (y * width)) * 4 + 1] = uint8_t(CLAMP(c.g * 255.0, 0, 255));
						wd8[(x + (y * width)) * 4 + 2] = uint8_t(CLAMP(c.b * 255.0, 0, 255));
						wd8[(x + (y * width)) * 4 + 3] = uint8_t(CLAMP(c.a * 255.0, 0, 255));
					}
				}
			}
			image->set_data(width, height, false, Image::FORMAT_RGBA8, data);
		}
	}

	if (texture.is_valid()) {
		RID new_texture = RS::get_singleton()->texture_2d_create(image);
		RS::get_singleton()->texture_replace(texture, new_texture);
	} else {
		texture = RS::get_singleton()->texture_2d_create(image);
	}
	RS::get_singleton()->texture_set_path(texture, get_path());
}

void GradientTexture2D::_update() {
	_update_now();
	emit_changed();
}

float GradientTexture2D::_get_gradient_offset_at(int x, int y) const {
	if (fill_to == fill_from) {
		return 0;
	}
	float ofs = 0;
	Vector2 pos;
	if (width > 1) {
		pos.x = static_cast<float>(x) / (width - 1);
	}
	if (height > 1) {
		pos.y = static_cast<float>(y) / (height - 1);
	}
	if (fill == Fill::FILL_LINEAR) {
		Vector2 segment[2];
		segment[0] = fill_from;
		segment[1] = fill_to;
		Vector2 closest = Geometry2D::get_closest_point_to_segment_uncapped(pos, &segment[0]);
		ofs = (closest - fill_from).length() / (fill_to - fill_from).length();
		if ((closest - fill_from).dot(fill_to - fill_from) < 0) {
			ofs *= -1;
		}
	} else if (fill == Fill::FILL_RADIAL) {
		ofs = (pos - fill_from).length() / (fill_to - fill_from).length();
	} else if (fill == Fill::FILL_SQUARE) {
		ofs = MAX(Math::abs(pos.x - fill_from.x), Math::abs(pos.y - fill_from.y)) / MAX(Math::abs(fill_to.x - fill_from.x), Math::abs(fill_to.y - fill_from.y));
	}
	if (repeat == Repeat::REPEAT_NONE) {
		ofs = CLAMP(ofs, 0.0, 1.0);
	} else if (repeat == Repeat::REPEAT) {
		ofs = Math::fmod(ofs, 1.0f);
		if (ofs < 0) {
			ofs = 1 + ofs;
		}
	} else if (repeat == Repeat::REPEAT_MIRROR) {
		ofs = Math::abs(ofs);
		ofs = Math::fmod(ofs, 2.0f);
		if (ofs > 1.0) {
			ofs = 2.0 - ofs;
		}
	}
	return ofs;
}

void GradientTexture2D::set_width(int p_width) {
	ERR_FAIL_COND_MSG(p_width <= 0 || p_width > 16384, "Texture dimensions have to be within 1 to 16384 range.");
	width = p_width;
	_queue_update();
}

int GradientTexture2D::get_width() const {
	return width;
}

void GradientTexture2D::set_height(int p_height) {
	ERR_FAIL_COND_MSG(p_height <= 0 || p_height > 16384, "Texture dimensions have to be within 1 to 16384 range.");
	height = p_height;
	_queue_update();
}
int GradientTexture2D::get_height() const {
	return height;
}

void GradientTexture2D::set_use_hdr(bool p_enabled) {
	if (p_enabled == use_hdr) {
		return;
	}

	use_hdr = p_enabled;
	_queue_update();
}

bool GradientTexture2D::is_using_hdr() const {
	return use_hdr;
}

void GradientTexture2D::set_fill_from(Vector2 p_fill_from) {
	fill_from = p_fill_from;
	_queue_update();
}

Vector2 GradientTexture2D::get_fill_from() const {
	return fill_from;
}

void GradientTexture2D::set_fill_to(Vector2 p_fill_to) {
	fill_to = p_fill_to;
	_queue_update();
}

Vector2 GradientTexture2D::get_fill_to() const {
	return fill_to;
}

void GradientTexture2D::set_fill(Fill p_fill) {
	fill = p_fill;
	_queue_update();
}

GradientTexture2D::Fill GradientTexture2D::get_fill() const {
	return fill;
}

void GradientTexture2D::set_repeat(Repeat p_repeat) {
	repeat = p_repeat;
	_queue_update();
}

GradientTexture2D::Repeat GradientTexture2D::get_repeat() const {
	return repeat;
}

RID GradientTexture2D::get_rid() const {
	if (texture.is_null()) {
		texture = RS::get_singleton()->texture_2d_placeholder_create();
	}
	return texture;
}

Ref<Image> GradientTexture2D::get_image() const {
	if (update_pending) {
		_update_now();
	}
	if (texture.is_null()) {
		return Ref<Image>();
	}
	return RenderingServer::get_singleton()->texture_2d_get(texture);
}

void GradientTexture2D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_gradient", "gradient"), &GradientTexture2D::set_gradient);
	ClassDB::bind_method(D_METHOD("get_gradient"), &GradientTexture2D::get_gradient);

	ClassDB::bind_method(D_METHOD("set_width", "width"), &GradientTexture2D::set_width);
	ClassDB::bind_method(D_METHOD("set_height", "height"), &GradientTexture2D::set_height);

	ClassDB::bind_method(D_METHOD("set_use_hdr", "enabled"), &GradientTexture2D::set_use_hdr);
	ClassDB::bind_method(D_METHOD("is_using_hdr"), &GradientTexture2D::is_using_hdr);

	ClassDB::bind_method(D_METHOD("set_fill", "fill"), &GradientTexture2D::set_fill);
	ClassDB::bind_method(D_METHOD("get_fill"), &GradientTexture2D::get_fill);
	ClassDB::bind_method(D_METHOD("set_fill_from", "fill_from"), &GradientTexture2D::set_fill_from);
	ClassDB::bind_method(D_METHOD("get_fill_from"), &GradientTexture2D::get_fill_from);
	ClassDB::bind_method(D_METHOD("set_fill_to", "fill_to"), &GradientTexture2D::set_fill_to);
	ClassDB::bind_method(D_METHOD("get_fill_to"), &GradientTexture2D::get_fill_to);

	ClassDB::bind_method(D_METHOD("set_repeat", "repeat"), &GradientTexture2D::set_repeat);
	ClassDB::bind_method(D_METHOD("get_repeat"), &GradientTexture2D::get_repeat);

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "gradient", PROPERTY_HINT_RESOURCE_TYPE, "Gradient", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_EDITOR_INSTANTIATE_OBJECT), "set_gradient", "get_gradient");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "width", PROPERTY_HINT_RANGE, "1,2048,or_greater,suffix:px"), "set_width", "get_width");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "height", PROPERTY_HINT_RANGE, "1,2048,or_greater,suffix:px"), "set_height", "get_height");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "use_hdr"), "set_use_hdr", "is_using_hdr");

	ADD_GROUP("Fill", "fill_");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "fill", PROPERTY_HINT_ENUM, "Linear,Radial,Square"), "set_fill", "get_fill");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "fill_from"), "set_fill_from", "get_fill_from");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "fill_to"), "set_fill_to", "get_fill_to");

	ADD_GROUP("Repeat", "repeat_");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "repeat", PROPERTY_HINT_ENUM, "No Repeat,Repeat,Mirror Repeat"), "set_repeat", "get_repeat");

	BIND_ENUM_CONSTANT(FILL_LINEAR);
	BIND_ENUM_CONSTANT(FILL_RADIAL);
	BIND_ENUM_CONSTANT(FILL_SQUARE);

	BIND_ENUM_CONSTANT(REPEAT_NONE);
	BIND_ENUM_CONSTANT(REPEAT);
	BIND_ENUM_CONSTANT(REPEAT_MIRROR);
}

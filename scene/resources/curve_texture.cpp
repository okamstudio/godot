/**************************************************************************/
/*  curve_texture.cpp                                                     */
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

#include "curve_texture.h"

void CurveTexture::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_width", "width"), &CurveTexture::set_width);

	ClassDB::bind_method(D_METHOD("set_curve", "curve"), &CurveTexture::set_curve);
	ClassDB::bind_method(D_METHOD("get_curve"), &CurveTexture::get_curve);

	ClassDB::bind_method(D_METHOD("set_texture_mode", "texture_mode"), &CurveTexture::set_texture_mode);
	ClassDB::bind_method(D_METHOD("get_texture_mode"), &CurveTexture::get_texture_mode);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "width", PROPERTY_HINT_RANGE, "1,4096,suffix:px"), "set_width", "get_width");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "texture_mode", PROPERTY_HINT_ENUM, "RGB,Red"), "set_texture_mode", "get_texture_mode");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "curve", PROPERTY_HINT_RESOURCE_TYPE, "Curve"), "set_curve", "get_curve");

	BIND_ENUM_CONSTANT(TEXTURE_MODE_RGB);
	BIND_ENUM_CONSTANT(TEXTURE_MODE_RED);
}

void CurveTexture::set_width(int p_width) {
	ERR_FAIL_COND(p_width < 32 || p_width > 4096);

	if (_width == p_width) {
		return;
	}

	_width = p_width;
	_update();
}

int CurveTexture::get_width() const {
	return _width;
}

void CurveTexture::ensure_default_setup(float p_min, float p_max) {
	if (_curve.is_null()) {
		Ref<Curve> curve = Ref<Curve>(memnew(Curve));
		curve->add_point(Vector2(0, 1));
		curve->add_point(Vector2(1, 1));
		curve->set_min_value(p_min);
		curve->set_max_value(p_max);
		set_curve(curve);
		// Min and max is 0..1 by default
	}
}

void CurveTexture::set_curve(Ref<Curve> p_curve) {
	if (_curve != p_curve) {
		if (_curve.is_valid()) {
			_curve->disconnect_changed(callable_mp(this, &CurveTexture::_update));
		}
		_curve = p_curve;
		if (_curve.is_valid()) {
			_curve->connect_changed(callable_mp(this, &CurveTexture::_update));
		}
		_update();
	}
}

void CurveTexture::_update() {
	Vector<uint8_t> data;
	data.resize(_width * sizeof(float) * (texture_mode == TEXTURE_MODE_RGB ? 3 : 1));

	// The array is locked in that scope
	{
		uint8_t *wd8 = data.ptrw();
		float *wd = (float *)wd8;

		if (_curve.is_valid()) {
			Curve &curve = **_curve;
			for (int i = 0; i < _width; ++i) {
				float t = i / static_cast<float>(_width);
				if (texture_mode == TEXTURE_MODE_RGB) {
					wd[i * 3 + 0] = curve.sample_baked(t);
					wd[i * 3 + 1] = wd[i * 3 + 0];
					wd[i * 3 + 2] = wd[i * 3 + 0];
				} else {
					wd[i] = curve.sample_baked(t);
				}
			}

		} else {
			for (int i = 0; i < _width; ++i) {
				if (texture_mode == TEXTURE_MODE_RGB) {
					wd[i * 3 + 0] = 0;
					wd[i * 3 + 1] = 0;
					wd[i * 3 + 2] = 0;
				} else {
					wd[i] = 0;
				}
			}
		}
	}

	Ref<Image> image = memnew(Image(_width, 1, false, texture_mode == TEXTURE_MODE_RGB ? Image::FORMAT_RGBF : Image::FORMAT_RF, data));

	if (_texture.is_valid()) {
		if (_current_texture_mode != texture_mode || _current_width != _width) {
			RID new_texture = RS::get_singleton()->texture_2d_create(image);
			RS::get_singleton()->texture_replace(_texture, new_texture);
		} else {
			RS::get_singleton()->texture_2d_update(_texture, image);
		}
	} else {
		_texture = RS::get_singleton()->texture_2d_create(image);
	}
	_current_texture_mode = texture_mode;
	_current_width = _width;

	emit_changed();
}

Ref<Curve> CurveTexture::get_curve() const {
	return _curve;
}

Ref<Image> CurveTexture::get_image() const {
	if (!_texture.is_valid()) {
		return Ref<Image>();
	}
	return RenderingServer::get_singleton()->texture_2d_get(_texture);
}

void CurveTexture::set_texture_mode(TextureMode p_mode) {
	ERR_FAIL_COND(p_mode < TEXTURE_MODE_RGB || p_mode > TEXTURE_MODE_RED);
	if (texture_mode == p_mode) {
		return;
	}
	texture_mode = p_mode;
	_update();
}
CurveTexture::TextureMode CurveTexture::get_texture_mode() const {
	return texture_mode;
}

RID CurveTexture::get_rid() const {
	if (!_texture.is_valid()) {
		_texture = RS::get_singleton()->texture_2d_placeholder_create();
	}
	return _texture;
}

CurveTexture::CurveTexture() {}

CurveTexture::~CurveTexture() {
	if (_texture.is_valid()) {
		ERR_FAIL_NULL(RenderingServer::get_singleton());
		RS::get_singleton()->free(_texture);
	}
}

//////////////////

void CurveXYZTexture::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_width", "width"), &CurveXYZTexture::set_width);

	ClassDB::bind_method(D_METHOD("set_curve_x", "curve"), &CurveXYZTexture::set_curve_x);
	ClassDB::bind_method(D_METHOD("get_curve_x"), &CurveXYZTexture::get_curve_x);

	ClassDB::bind_method(D_METHOD("set_curve_y", "curve"), &CurveXYZTexture::set_curve_y);
	ClassDB::bind_method(D_METHOD("get_curve_y"), &CurveXYZTexture::get_curve_y);

	ClassDB::bind_method(D_METHOD("set_curve_z", "curve"), &CurveXYZTexture::set_curve_z);
	ClassDB::bind_method(D_METHOD("get_curve_z"), &CurveXYZTexture::get_curve_z);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "width", PROPERTY_HINT_RANGE, "1,4096,suffix:px"), "set_width", "get_width");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "curve_x", PROPERTY_HINT_RESOURCE_TYPE, "Curve"), "set_curve_x", "get_curve_x");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "curve_y", PROPERTY_HINT_RESOURCE_TYPE, "Curve"), "set_curve_y", "get_curve_y");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "curve_z", PROPERTY_HINT_RESOURCE_TYPE, "Curve"), "set_curve_z", "get_curve_z");
}

void CurveXYZTexture::set_width(int p_width) {
	ERR_FAIL_COND(p_width < 32 || p_width > 4096);

	if (_width == p_width) {
		return;
	}

	_width = p_width;
	_update();
}

int CurveXYZTexture::get_width() const {
	return _width;
}

void CurveXYZTexture::ensure_default_setup(float p_min, float p_max) {
	if (_curve_x.is_null()) {
		Ref<Curve> curve = Ref<Curve>(memnew(Curve));
		curve->add_point(Vector2(0, 1));
		curve->add_point(Vector2(1, 1));
		curve->set_min_value(p_min);
		curve->set_max_value(p_max);
		set_curve_x(curve);
	}

	if (_curve_y.is_null()) {
		Ref<Curve> curve = Ref<Curve>(memnew(Curve));
		curve->add_point(Vector2(0, 1));
		curve->add_point(Vector2(1, 1));
		curve->set_min_value(p_min);
		curve->set_max_value(p_max);
		set_curve_y(curve);
	}

	if (_curve_z.is_null()) {
		Ref<Curve> curve = Ref<Curve>(memnew(Curve));
		curve->add_point(Vector2(0, 1));
		curve->add_point(Vector2(1, 1));
		curve->set_min_value(p_min);
		curve->set_max_value(p_max);
		set_curve_z(curve);
	}
}

void CurveXYZTexture::set_curve_x(Ref<Curve> p_curve) {
	if (_curve_x != p_curve) {
		if (_curve_x.is_valid()) {
			_curve_x->disconnect_changed(callable_mp(this, &CurveXYZTexture::_update));
		}
		_curve_x = p_curve;
		if (_curve_x.is_valid()) {
			_curve_x->connect_changed(callable_mp(this, &CurveXYZTexture::_update), CONNECT_REFERENCE_COUNTED);
		}
		_update();
	}
}

void CurveXYZTexture::set_curve_y(Ref<Curve> p_curve) {
	if (_curve_y != p_curve) {
		if (_curve_y.is_valid()) {
			_curve_y->disconnect_changed(callable_mp(this, &CurveXYZTexture::_update));
		}
		_curve_y = p_curve;
		if (_curve_y.is_valid()) {
			_curve_y->connect_changed(callable_mp(this, &CurveXYZTexture::_update), CONNECT_REFERENCE_COUNTED);
		}
		_update();
	}
}

void CurveXYZTexture::set_curve_z(Ref<Curve> p_curve) {
	if (_curve_z != p_curve) {
		if (_curve_z.is_valid()) {
			_curve_z->disconnect_changed(callable_mp(this, &CurveXYZTexture::_update));
		}
		_curve_z = p_curve;
		if (_curve_z.is_valid()) {
			_curve_z->connect_changed(callable_mp(this, &CurveXYZTexture::_update), CONNECT_REFERENCE_COUNTED);
		}
		_update();
	}
}

void CurveXYZTexture::_update() {
	Vector<uint8_t> data;
	data.resize(_width * sizeof(float) * 3);

	// The array is locked in that scope
	{
		uint8_t *wd8 = data.ptrw();
		float *wd = (float *)wd8;

		if (_curve_x.is_valid()) {
			Curve &curve_x = **_curve_x;
			for (int i = 0; i < _width; ++i) {
				float t = i / static_cast<float>(_width);
				wd[i * 3 + 0] = curve_x.sample_baked(t);
			}

		} else {
			for (int i = 0; i < _width; ++i) {
				wd[i * 3 + 0] = 0;
			}
		}

		if (_curve_y.is_valid()) {
			Curve &curve_y = **_curve_y;
			for (int i = 0; i < _width; ++i) {
				float t = i / static_cast<float>(_width);
				wd[i * 3 + 1] = curve_y.sample_baked(t);
			}

		} else {
			for (int i = 0; i < _width; ++i) {
				wd[i * 3 + 1] = 0;
			}
		}

		if (_curve_z.is_valid()) {
			Curve &curve_z = **_curve_z;
			for (int i = 0; i < _width; ++i) {
				float t = i / static_cast<float>(_width);
				wd[i * 3 + 2] = curve_z.sample_baked(t);
			}

		} else {
			for (int i = 0; i < _width; ++i) {
				wd[i * 3 + 2] = 0;
			}
		}
	}

	Ref<Image> image = memnew(Image(_width, 1, false, Image::FORMAT_RGBF, data));

	if (_texture.is_valid()) {
		if (_current_width != _width) {
			RID new_texture = RS::get_singleton()->texture_2d_create(image);
			RS::get_singleton()->texture_replace(_texture, new_texture);
		} else {
			RS::get_singleton()->texture_2d_update(_texture, image);
		}
	} else {
		_texture = RS::get_singleton()->texture_2d_create(image);
	}
	_current_width = _width;

	emit_changed();
}

Ref<Curve> CurveXYZTexture::get_curve_x() const {
	return _curve_x;
}

Ref<Curve> CurveXYZTexture::get_curve_y() const {
	return _curve_y;
}

Ref<Curve> CurveXYZTexture::get_curve_z() const {
	return _curve_z;
}

RID CurveXYZTexture::get_rid() const {
	if (!_texture.is_valid()) {
		_texture = RS::get_singleton()->texture_2d_placeholder_create();
	}
	return _texture;
}

Ref<Image> CurveXYZTexture::get_image() const {
	if (!_texture.is_valid()) {
		return Ref<Image>();
	}
	return RenderingServer::get_singleton()->texture_2d_get(_texture);
}

CurveXYZTexture::CurveXYZTexture() {}

CurveXYZTexture::~CurveXYZTexture() {
	if (_texture.is_valid()) {
		ERR_FAIL_NULL(RenderingServer::get_singleton());
		RS::get_singleton()->free(_texture);
	}
}

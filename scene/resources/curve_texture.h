/**************************************************************************/
/*  curve_texture.h                                                       */
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

#pragma once

#include "scene/resources/texture.h"

class CurveTexture : public Texture2D {
	GDCLASS(CurveTexture, Texture2D);
	RES_BASE_EXTENSION("curvetex")
public:
	enum TextureMode {
		TEXTURE_MODE_RGB,
		TEXTURE_MODE_RED,
	};

private:
	mutable RID _texture;
	Ref<Curve> _curve;
	int _width = 256;
	int _current_width = 0;
	TextureMode texture_mode = TEXTURE_MODE_RGB;
	TextureMode _current_texture_mode = TEXTURE_MODE_RGB;

	void _update();

protected:
	static void _bind_methods();

public:
	void set_width(int p_width);
	int get_width() const override;

	void set_texture_mode(TextureMode p_mode);
	TextureMode get_texture_mode() const;

	void ensure_default_setup(float p_min = 0, float p_max = 1);

	void set_curve(Ref<Curve> p_curve);
	Ref<Curve> get_curve() const;

	virtual RID get_rid() const override;

	virtual int get_height() const override { return 1; }
	virtual bool has_alpha() const override { return false; }

	CurveTexture();
	~CurveTexture();
};

VARIANT_ENUM_CAST(CurveTexture::TextureMode)

class CurveXYZTexture : public Texture2D {
	GDCLASS(CurveXYZTexture, Texture2D);
	RES_BASE_EXTENSION("curvetex")

private:
	mutable RID _texture;
	Ref<Curve> _curve_x;
	Ref<Curve> _curve_y;
	Ref<Curve> _curve_z;
	int _width = 256;
	int _current_width = 0;

	void _update();

protected:
	static void _bind_methods();

public:
	void set_width(int p_width);
	int get_width() const override;

	void ensure_default_setup(float p_min = 0, float p_max = 1);

	void set_curve_x(Ref<Curve> p_curve);
	Ref<Curve> get_curve_x() const;

	void set_curve_y(Ref<Curve> p_curve);
	Ref<Curve> get_curve_y() const;

	void set_curve_z(Ref<Curve> p_curve);
	Ref<Curve> get_curve_z() const;

	virtual RID get_rid() const override;

	virtual int get_height() const override { return 1; }
	virtual bool has_alpha() const override { return false; }

	CurveXYZTexture();
	~CurveXYZTexture();
};

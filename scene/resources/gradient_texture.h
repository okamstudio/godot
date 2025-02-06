/**************************************************************************/
/*  gradient_texture.h                                                    */
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

class GradientTexture1D : public Texture2D {
	GDCLASS(GradientTexture1D, Texture2D);

private:
	Ref<Gradient> gradient;
	mutable bool update_pending = false;
	mutable RID texture;
	int width = 256;
	bool use_hdr = false;

	void _queue_update();
	void _update() const;

protected:
	static void _bind_methods();

public:
	void set_gradient(Ref<Gradient> p_gradient);
	Ref<Gradient> get_gradient() const;

	void set_width(int p_width);
	int get_width() const override;

	void set_use_hdr(bool p_enabled);
	bool is_using_hdr() const;

	virtual RID get_rid() const override;
	virtual int get_height() const override { return 1; }
	virtual bool has_alpha() const override { return true; }

	virtual Ref<Image> get_image() const override;
	void update_now() const;

	GradientTexture1D();
	virtual ~GradientTexture1D();
};

class GradientTexture2D : public Texture2D {
	GDCLASS(GradientTexture2D, Texture2D);

public:
	enum Fill {
		FILL_LINEAR,
		FILL_RADIAL,
		FILL_SQUARE,
	};
	enum Repeat {
		REPEAT_NONE,
		REPEAT,
		REPEAT_MIRROR,
	};

private:
	Ref<Gradient> gradient;
	mutable RID texture;

	int width = 64;
	int height = 64;

	bool use_hdr = false;

	Vector2 fill_from;
	Vector2 fill_to = Vector2(1, 0);

	Fill fill = FILL_LINEAR;
	Repeat repeat = REPEAT_NONE;

	float _get_gradient_offset_at(int x, int y) const;

	mutable bool update_pending = false;
	void _queue_update();
	void _update() const;

protected:
	static void _bind_methods();

public:
	void set_gradient(Ref<Gradient> p_gradient);
	Ref<Gradient> get_gradient() const;

	void set_width(int p_width);
	virtual int get_width() const override;
	void set_height(int p_height);
	virtual int get_height() const override;

	void set_use_hdr(bool p_enabled);
	bool is_using_hdr() const;

	void set_fill(Fill p_fill);
	Fill get_fill() const;
	void set_fill_from(Vector2 p_fill_from);
	Vector2 get_fill_from() const;
	void set_fill_to(Vector2 p_fill_to);
	Vector2 get_fill_to() const;

	void set_repeat(Repeat p_repeat);
	Repeat get_repeat() const;

	virtual RID get_rid() const override;
	virtual bool has_alpha() const override { return true; }
	virtual Ref<Image> get_image() const override;
	void update_now() const;

	GradientTexture2D();
	virtual ~GradientTexture2D();
};

VARIANT_ENUM_CAST(GradientTexture2D::Fill);
VARIANT_ENUM_CAST(GradientTexture2D::Repeat);

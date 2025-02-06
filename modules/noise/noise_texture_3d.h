/**************************************************************************/
/*  noise_texture_3d.h                                                    */
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

#include "noise.h"

#include "core/object/ref_counted.h"
#include "scene/resources/texture.h"

class NoiseTexture3D : public Texture3D {
	GDCLASS(NoiseTexture3D, Texture3D);

private:
	Thread noise_thread;

	bool first_time = true;
	bool update_queued = false;
	bool regen_queued = false;

	mutable RID texture;
	uint32_t flags = 0;

	int width = 64;
	int height = 64;
	int depth = 64;
	bool invert = false;
	bool seamless = false;
	real_t seamless_blend_skirt = 0.1;
	bool normalize = true;

	Ref<Gradient> color_ramp;
	Ref<Noise> noise;

	Image::Format format = Image::FORMAT_L8;

	void _thread_done(const TypedArray<Image> &p_data);
	static void _thread_function(void *p_ud);

	void _queue_update();
	TypedArray<Image> _generate_texture();
	void _update_texture();
	void _set_texture_data(const TypedArray<Image> &p_data);

	Ref<Image> _modulate_with_gradient(Ref<Image> p_image, Ref<Gradient> p_gradient);

protected:
	static void _bind_methods();
	void _validate_property(PropertyInfo &p_property) const;

public:
	void set_noise(Ref<Noise> p_noise);
	Ref<Noise> get_noise();

	void set_width(int p_width);
	void set_height(int p_height);
	void set_depth(int p_depth);

	void set_invert(bool p_invert);
	bool get_invert() const;

	void set_seamless(bool p_seamless);
	bool get_seamless();

	void set_seamless_blend_skirt(real_t p_blend_skirt);
	real_t get_seamless_blend_skirt();

	void set_normalize(bool p_normalize);
	bool is_normalized() const;

	void set_color_ramp(const Ref<Gradient> &p_gradient);
	Ref<Gradient> get_color_ramp() const;

	virtual int get_width() const override;
	virtual int get_height() const override;
	virtual int get_depth() const override;

	virtual bool has_mipmaps() const override;

	virtual RID get_rid() const override;

	virtual Vector<Ref<Image>> get_data() const override;
	virtual Image::Format get_format() const override;

	NoiseTexture3D();
	virtual ~NoiseTexture3D();
};

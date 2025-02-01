/**************************************************************************/
/*  glow.h                                                                */
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

#ifdef GLES3_ENABLED

#include "drivers/gles3/shaders/effects/glow.glsl.gen.h"

namespace GLES3 {

class Glow {
private:
	static Glow *singleton;

	struct GLOW {
		GlowShaderGLES3 shader;
		RID shader_version;
	} glow;

	float luminance_multiplier = 1.0;

	float glow_intensity = 1.0;
	float glow_bloom = 0.0;
	float glow_hdr_bleed_threshold = 1.0;
	float glow_hdr_bleed_scale = 2.0;
	float glow_hdr_luminance_cap = 12.0;

	// Use for full-screen effects. Slightly more efficient than screen_quad as this eliminates pixel overdraw along the diagonal.
	GLuint screen_triangle = 0;
	GLuint screen_triangle_array = 0;

	void _draw_screen_triangle();

public:
	struct GLOWLEVEL {
		Size2i size;
		GLuint color = 0;
		GLuint fbo = 0;
	};

	static Glow *get_singleton();

	Glow();
	~Glow();

	void set_intensity(float p_value) { glow_intensity = p_value; }
	void set_luminance_multiplier(float p_luminance_multiplier) { luminance_multiplier = p_luminance_multiplier; }
	void set_glow_bloom(float p_bloom) { glow_bloom = p_bloom; }
	void set_glow_hdr_bleed_threshold(float p_threshold) { glow_hdr_bleed_threshold = p_threshold; }
	void set_glow_hdr_bleed_scale(float p_scale) { glow_hdr_bleed_scale = p_scale; }
	void set_glow_hdr_luminance_cap(float p_cap) { glow_hdr_luminance_cap = p_cap; }

	void process_glow(GLuint p_source_color, Size2i p_size, const GLOWLEVEL *p_glow_buffers, uint32_t p_view = 0, bool p_use_multiview = false);
};

} //namespace GLES3

#endif // GLES3_ENABLED

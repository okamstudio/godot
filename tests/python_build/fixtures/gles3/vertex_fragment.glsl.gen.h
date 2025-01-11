/**************************************************************************/
/*  vertex_fragment.glsl.gen.h                                            */
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

/* THIS FILE IS GENERATED. EDITS WILL BE LOST. */

#ifndef VERTEX_FRAGMENT_GLES3_GLSL_GEN_H
#define VERTEX_FRAGMENT_GLES3_GLSL_GEN_H

#include "drivers/gles3/shader_gles3.h"

class VertexFragmentShaderGLES3 : public ShaderGLES3 {
public:
	enum ShaderVariant {
		MODE_NINEPATCH
	};

	enum Specializations {
		DISABLE_LIGHTING = 1
	};

	_FORCE_INLINE_ bool version_bind_shader(RID p_version, ShaderVariant p_variant, uint64_t p_specialization = 0) {
		return _version_bind_shader(p_version, p_variant, p_specialization);
	}

protected:
	virtual void _init() override {
		static const char** _uniform_strings = nullptr;
		static const char* _variant_defines[] = {
			"#define USE_NINEPATCH"
		};
		static TexUnitPair *_texunit_pairs = nullptr;
		static UBOPair *_ubo_pairs = nullptr;
		static Specialization _spec_pairs[] = {
			{ "DISABLE_LIGHTING", false }
		};
		static const Feedback *_feedbacks = nullptr;
		static const char _vertex_code[] = {
R"<!>(
precision highp float;
precision highp int;

layout(location = 0) in highp vec3 vertex;

out highp vec4 position_interp;

void main() {
	position_interp = vec4(vertex.x,1,0,1);
}

)<!>"
		};

		static const char _fragment_code[] = {
R"<!>(
precision highp float;
precision highp int;

in highp vec4 position_interp;

void main() {
	highp float depth = ((position_interp.z / position_interp.w) + 1.0);
	frag_color = vec4(depth);
}
)<!>"
		};

		_setup(_vertex_code, _fragment_code, "VertexFragmentShaderGLES3",
				0, _uniform_strings, 0, _ubo_pairs,
				0, _feedbacks, 0, _texunit_pairs,
				1, _spec_pairs, 1, _variant_defines);
	}
};

#endif // VERTEX_FRAGMENT_GLES3_GLSL_GEN_H

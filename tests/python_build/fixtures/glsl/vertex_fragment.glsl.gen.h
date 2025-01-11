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

#ifndef VERTEX_FRAGMENT_RAW_GLSL_GEN_H
#define VERTEX_FRAGMENT_RAW_GLSL_GEN_H

static const char vertex_fragment_shader_glsl[] = {
R"<!>(#[versions]

lines = "#define MODE_LINES";

#[vertex]

#version 450

#VERSION_DEFINES

layout(location = 0) out vec3 uv_interp;

void main() {

#ifdef MODE_LINES
	uv_interp = vec3(0,0,1);
#endif
}

#[fragment]

#version 450

#VERSION_DEFINES

#define M_PI 3.14159265359

layout(location = 0) out vec4 dst_color;

void main() {
	dst_color = vec4(1,1,0,0);
}
)<!>"
};

#endif // VERTEX_FRAGMENT_RAW_GLSL_GEN_H

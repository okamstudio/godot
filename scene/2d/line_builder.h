/**************************************************************************/
/*  line_builder.h                                                        */
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

#include "line_2d.h"

class LineBuilder {
public:
	// TODO Move in a struct and reference it
	// Input
	Vector<Vector2> points;
	Line2D::LineJointMode joint_mode = Line2D::LINE_JOINT_SHARP;
	Line2D::LineCapMode begin_cap_mode = Line2D::LINE_CAP_NONE;
	Line2D::LineCapMode end_cap_mode = Line2D::LINE_CAP_NONE;
	bool closed = false;
	float width = 10.0;
	Curve *curve = nullptr;
	Color default_color = Color(0.4, 0.5, 1);
	Gradient *gradient = nullptr;
	Line2D::LineTextureMode texture_mode = Line2D::LineTextureMode::LINE_TEXTURE_NONE;
	float sharp_limit = 2.f;
	int round_precision = 8;
	float tile_aspect = 1.f; // w/h
	// TODO offset_joints option (offers alternative implementation of round joints)

	// TODO Move in a struct and reference it
	// Output
	Vector<Vector2> vertices;
	Vector<Color> colors;
	Vector<Vector2> uvs;
	Vector<int> indices;

	LineBuilder();

	void build();

private:
	enum Orientation {
		UP = 0,
		DOWN = 1
	};

	// Triangle-strip methods
	void strip_begin(Vector2 up, Vector2 down, Color color, float uvx);
	void strip_new_quad(Vector2 up, Vector2 down, Color color, float uvx);
	void strip_add_quad(Vector2 up, Vector2 down, Color color, float uvx);
	void strip_add_tri(Vector2 up, Orientation orientation);
	void strip_add_arc(Vector2 center, float angle_delta, Orientation orientation);

	void new_arc(Vector2 center, Vector2 vbegin, float angle_delta, Color color, Rect2 uv_rect);

private:
	bool _interpolate_color = false;
	int _last_index[2] = {}; // Index of last up and down vertices of the strip
};

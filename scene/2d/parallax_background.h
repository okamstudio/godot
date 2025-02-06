/**************************************************************************/
/*  parallax_background.h                                                 */
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

#include "scene/main/canvas_layer.h"

class ParallaxBackground : public CanvasLayer {
	GDCLASS(ParallaxBackground, CanvasLayer);

	Point2 offset;
	real_t scale = 1.0;
	Point2 base_offset;
	Point2 base_scale = Vector2(1, 1);
	Point2 screen_offset;
	String group_name;
	Point2 limit_begin;
	Point2 limit_end;
	Point2 final_offset;
	bool ignore_camera_zoom = false;

	void _update_scroll();

protected:
	void _camera_moved(const Transform2D &p_transform, const Point2 &p_screen_offset, const Point2 &p_adj_screen_offset);

	void _notification(int p_what);
	static void _bind_methods();

public:
	void set_scroll_offset(const Point2 &p_ofs);
	Point2 get_scroll_offset() const;

	void set_scroll_scale(real_t p_scale);
	real_t get_scroll_scale() const;

	void set_scroll_base_offset(const Point2 &p_ofs);
	Point2 get_scroll_base_offset() const;

	void set_scroll_base_scale(const Point2 &p_ofs);
	Point2 get_scroll_base_scale() const;

	void set_limit_begin(const Point2 &p_ofs);
	Point2 get_limit_begin() const;

	void set_limit_end(const Point2 &p_ofs);
	Point2 get_limit_end() const;

	void set_ignore_camera_zoom(bool ignore);
	bool is_ignore_camera_zoom();

	Vector2 get_final_offset() const;

	ParallaxBackground();
};

/**************************************************************************/
/*  parallax_2d.h                                                         */
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

#include "scene/2d/node_2d.h"

class Parallax2D : public Node2D {
	GDCLASS(Parallax2D, Node2D);

	static constexpr real_t DEFAULT_LIMIT = 10000000;

	String group_name;
	Size2 scroll_scale = Size2(1, 1);
	Point2 scroll_offset;
	Point2 screen_offset;
	Vector2 repeat_size;
	int repeat_times = 1;
	Point2 limit_begin = Point2(-DEFAULT_LIMIT, -DEFAULT_LIMIT);
	Point2 limit_end = Point2(DEFAULT_LIMIT, DEFAULT_LIMIT);
	Point2 autoscroll;
	bool follow_viewport = true;
	bool ignore_camera_scroll = false;

	void _update_process();
	void _update_repeat();
	void _update_scroll();

protected:
#ifdef TOOLS_ENABLED
	void _edit_set_position(const Point2 &p_position) override;
#endif // TOOLS_ENABLED
	void _validate_property(PropertyInfo &p_property) const;
	void _camera_moved(const Transform2D &p_transform, const Point2 &p_screen_offset, const Point2 &p_adj_screen_offset);
	void _notification(int p_what);
	static void _bind_methods();

public:
	void set_scroll_scale(const Size2 &p_scale);
	Size2 get_scroll_scale() const;

	void set_repeat_size(const Size2 &p_repeat_size);
	Size2 get_repeat_size() const;

	void set_repeat_times(int p_repeat_times);
	int get_repeat_times() const;

	void set_autoscroll(const Point2 &p_autoscroll);
	Point2 get_autoscroll() const;

	void set_scroll_offset(const Point2 &p_offset);
	Point2 get_scroll_offset() const;

	void set_screen_offset(const Point2 &p_offset);
	Point2 get_screen_offset() const;

	void set_limit_begin(const Point2 &p_offset);
	Point2 get_limit_begin() const;

	void set_limit_end(const Point2 &p_offset);
	Point2 get_limit_end() const;

	void set_follow_viewport(bool p_follow);
	bool get_follow_viewport();

	void set_ignore_camera_scroll(bool p_ignore);
	bool is_ignore_camera_scroll();

	Parallax2D();
};

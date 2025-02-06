/**************************************************************************/
/*  style_box_flat.h                                                      */
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

#include "scene/resources/style_box.h"

class StyleBoxFlat : public StyleBox {
	GDCLASS(StyleBoxFlat, StyleBox);

	Color bg_color = Color(0.6, 0.6, 0.6);
	Color shadow_color = Color(0, 0, 0, 0.6);
	Color border_color = Color(0.8, 0.8, 0.8);

	real_t border_width[4] = {};
	real_t expand_margin[4] = {};
	real_t corner_radius[4] = {};

	bool draw_center = true;
	bool blend_border = false;
	Vector2 skew;
	bool anti_aliased = true;

	int corner_detail = 8;
	int shadow_size = 0;
	Point2 shadow_offset;
	real_t aa_size = 1;

protected:
	virtual float get_style_margin(Side p_side) const override;
	static void _bind_methods();
	void _validate_property(PropertyInfo &p_property) const;

public:
	void set_bg_color(const Color &p_color);
	Color get_bg_color() const;

	void set_border_color(const Color &p_color);
	Color get_border_color() const;

	void set_border_width_all(int p_size);
	int get_border_width_min() const;

	void set_border_width(Side p_side, int p_width);
	int get_border_width(Side p_side) const;

	void set_border_blend(bool p_blend);
	bool get_border_blend() const;

	void set_corner_radius_all(int radius);
	void set_corner_radius_individual(const int radius_top_left, const int radius_top_right, const int radius_bottom_right, const int radius_bottom_left);
	void set_corner_radius(Corner p_corner, const int radius);
	int get_corner_radius(Corner p_corner) const;

	void set_corner_detail(const int &p_corner_detail);
	int get_corner_detail() const;

	void set_expand_margin(Side p_expand_side, float p_size);
	void set_expand_margin_all(float p_expand_margin_size);
	void set_expand_margin_individual(float p_left, float p_top, float p_right, float p_bottom);
	float get_expand_margin(Side p_expand_side) const;

	void set_draw_center(bool p_enabled);
	bool is_draw_center_enabled() const;

	void set_skew(Vector2 p_skew);
	Vector2 get_skew() const;

	void set_shadow_color(const Color &p_color);
	Color get_shadow_color() const;

	void set_shadow_size(const int &p_size);
	int get_shadow_size() const;

	void set_shadow_offset(const Point2 &p_offset);
	Point2 get_shadow_offset() const;

	void set_anti_aliased(const bool &p_anti_aliased);
	bool is_anti_aliased() const;
	void set_aa_size(const real_t p_aa_size);
	real_t get_aa_size() const;

	virtual Rect2 get_draw_rect(const Rect2 &p_rect) const override;
	virtual void draw(RID p_canvas_item, const Rect2 &p_rect) const override;

	StyleBoxFlat();
	~StyleBoxFlat();
};

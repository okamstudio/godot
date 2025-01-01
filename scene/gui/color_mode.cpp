/**************************************************************************/
/*  color_mode.cpp                                                        */
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

#include "color_mode.h"

#include "core/math/color.h"
#include "scene/gui/slider.h"

ColorMode::ColorMode(ColorPicker *p_color_picker) {
	color_picker = p_color_picker;
}

String ColorModeRGB::get_slider_label(int idx) const {
	ERR_FAIL_INDEX_V_MSG(idx, 3, String(), "Couldn't get slider label.");
	return labels[idx];
}

float ColorModeRGB::get_slider_max(int idx) const {
	ERR_FAIL_INDEX_V_MSG(idx, 4, 0, "Couldn't get slider max value.");
	Color color = color_picker->get_pick_color();
	return next_power_of_2(MAX(255, color.components[idx] * 255.0)) - 1;
}

float ColorModeRGB::get_slider_value(int idx) const {
	ERR_FAIL_INDEX_V_MSG(idx, 4, 0, "Couldn't get slider value.");
	return color_picker->get_pick_color().components[idx] * 255;
}

Color ColorModeRGB::get_color() const {
	Vector<float> values = color_picker->get_active_slider_values();
	Color color;
	for (int i = 0; i < 4; i++) {
		color.components[i] = values[i] / 255.0;
	}
	return color;
}

void ColorModeRGB::slider_draw(int p_which) {
	Vector<Vector2> pos;
	pos.resize(4);
	Vector<Color> col;
	col.resize(4);
	HSlider *slider = color_picker->get_slider(p_which);
	Size2 size = slider->get_size();
	Color left_color;
	Color right_color;
	Color color = color_picker->get_pick_color();
	const real_t margin = 16 * color_picker->theme_cache.base_scale;

	if (p_which == ColorPicker::SLIDER_COUNT) {
		slider->draw_texture_rect(color_picker->theme_cache.sample_bg, Rect2(Point2(0, 0), Size2(size.x, margin)), true);

		left_color = color;
		left_color.a = 0;
		right_color = color;
		right_color.a = 1;
	} else {
		left_color = Color(
				p_which == 0 ? 0 : color.r,
				p_which == 1 ? 0 : color.g,
				p_which == 2 ? 0 : color.b);
		right_color = Color(
				p_which == 0 ? 1 : color.r,
				p_which == 1 ? 1 : color.g,
				p_which == 2 ? 1 : color.b);
	}

	col.set(0, left_color);
	col.set(1, right_color);
	col.set(2, right_color);
	col.set(3, left_color);
	pos.set(0, Vector2(0, 0));
	pos.set(1, Vector2(size.x, 0));
	pos.set(2, Vector2(size.x, margin));
	pos.set(3, Vector2(0, margin));

	slider->draw_polygon(pos, col);
}

void ColorModeHSV::_value_changed() {
	Vector<float> values = color_picker->get_active_slider_values();

	if (values[1] > 0 || values[0] != cached_hue) {
		cached_hue = values[0];
	}
	if (values[2] > 0 || values[1] != cached_saturation) {
		cached_saturation = values[1];
	}
}

String ColorModeHSV::get_slider_label(int idx) const {
	ERR_FAIL_INDEX_V_MSG(idx, 3, String(), "Couldn't get slider label.");
	return labels[idx];
}

float ColorModeHSV::get_slider_max(int idx) const {
	ERR_FAIL_INDEX_V_MSG(idx, 4, 0, "Couldn't get slider max value.");
	return slider_max[idx];
}

float ColorModeHSV::get_slider_value(int idx) const {
	switch (idx) {
		case 0: {
			if (color_picker->get_pick_color().get_s() > 0) {
				return color_picker->get_pick_color().get_h() * 360.0;
			} else {
				return cached_hue;
			}
		}
		case 1: {
			if (color_picker->get_pick_color().get_v() > 0) {
				return color_picker->get_pick_color().get_s() * 100.0;
			} else {
				return cached_saturation;
			}
		}
		case 2:
			return color_picker->get_pick_color().get_v() * 100.0;
		case 3:
			return Math::round(color_picker->get_pick_color().components[3] * 255.0);
		default:
			ERR_FAIL_V_MSG(0, "Couldn't get slider value.");
	}
}

Color ColorModeHSV::get_color() const {
	Vector<float> values = color_picker->get_active_slider_values();
	Color color;
	color.set_hsv(values[0] / 360.0, values[1] / 100.0, values[2] / 100.0, values[3] / 255.0);
	return color;
}

void ColorModeHSV::slider_draw(int p_which) {
	Vector<Vector2> pos;
	pos.resize(4);
	Vector<Color> col;
	col.resize(4);
	HSlider *slider = color_picker->get_slider(p_which);
	Size2 size = slider->get_size();
	Color left_color;
	Color right_color;
	Color color = color_picker->get_pick_color();
	const real_t margin = 16 * color_picker->theme_cache.base_scale;

	if (p_which == ColorPicker::SLIDER_COUNT) {
		slider->draw_texture_rect(color_picker->theme_cache.sample_bg, Rect2(Point2(0, 0), Size2(size.x, margin)), true);

		left_color = color;
		left_color.a = 0;
		right_color = color;
		right_color.a = 1;
	} else if (p_which == 0) {
		float v = color.get_v();
		left_color = Color(v, v, v);
		right_color = left_color;
	} else {
		Color s_col;
		Color v_col;
		s_col.set_hsv(color.get_h(), 0, color.get_v());
		left_color = (p_which == 1) ? s_col : Color(0, 0, 0);

		float s_col_hue = (Math::is_zero_approx(color.get_s())) ? cached_hue / 360.0 : color.get_h();
		s_col.set_hsv(s_col_hue, 1, color.get_v());
		v_col.set_hsv(color.get_h(), color.get_s(), 1);
		right_color = (p_which == 1) ? s_col : v_col;
	}
	col.set(0, left_color);
	col.set(1, right_color);
	col.set(2, right_color);
	col.set(3, left_color);
	pos.set(0, Vector2(0, 0));
	pos.set(1, Vector2(size.x, 0));
	pos.set(2, Vector2(size.x, margin));
	pos.set(3, Vector2(0, margin));

	slider->draw_polygon(pos, col);

	if (p_which == 0) { // H
		Ref<Texture2D> hue = color_picker->theme_cache.color_hue;
		slider->draw_texture_rect(hue, Rect2(Vector2(), Vector2(size.x, margin)), false, Color::from_hsv(0, 0, color.get_v(), color.get_s()));
	}
}

String ColorModeRAW::get_slider_label(int idx) const {
	ERR_FAIL_INDEX_V_MSG(idx, 3, String(), "Couldn't get slider label.");
	return labels[idx];
}

float ColorModeRAW::get_slider_max(int idx) const {
	ERR_FAIL_INDEX_V_MSG(idx, 4, 0, "Couldn't get slider max value.");
	return slider_max[idx];
}

float ColorModeRAW::get_slider_value(int idx) const {
	ERR_FAIL_INDEX_V_MSG(idx, 4, 0, "Couldn't get slider value.");
	return color_picker->get_pick_color().components[idx];
}

Color ColorModeRAW::get_color() const {
	Vector<float> values = color_picker->get_active_slider_values();
	Color color;
	for (int i = 0; i < 4; i++) {
		color.components[i] = values[i];
	}
	return color;
}

void ColorModeRAW::slider_draw(int p_which) {
	Vector<Vector2> pos;
	pos.resize(4);
	Vector<Color> col;
	col.resize(4);
	HSlider *slider = color_picker->get_slider(p_which);
	Size2 size = slider->get_size();
	Color left_color;
	Color right_color;
	Color color = color_picker->get_pick_color();
	const real_t margin = 16 * color_picker->theme_cache.base_scale;

	if (p_which == ColorPicker::SLIDER_COUNT) {
		slider->draw_texture_rect(color_picker->theme_cache.sample_bg, Rect2(Point2(0, 0), Size2(size.x, margin)), true);

		left_color = color;
		left_color.a = 0;
		right_color = color;
		right_color.a = 1;

		col.set(0, left_color);
		col.set(1, right_color);
		col.set(2, right_color);
		col.set(3, left_color);
		pos.set(0, Vector2(0, 0));
		pos.set(1, Vector2(size.x, 0));
		pos.set(2, Vector2(size.x, margin));
		pos.set(3, Vector2(0, margin));

		slider->draw_polygon(pos, col);
	}
}

bool ColorModeRAW::apply_theme() const {
	for (int i = 0; i < 4; i++) {
		HSlider *slider = color_picker->get_slider(i);
		slider->remove_theme_icon_override("grabber");
		slider->remove_theme_icon_override("grabber_highlight");
		slider->remove_theme_style_override("slider");
		slider->remove_theme_constant_override("grabber_offset");
	}

	return true;
}

void ColorModeOKHSL::_value_changed() {
	Vector<float> values = color_picker->get_active_slider_values();

	if (values[1] > 0 || values[0] != cached_hue) {
		cached_hue = values[0];
	}
	if (values[2] > 0 || values[1] != cached_saturation) {
		cached_saturation = values[1];
	}
}

String ColorModeOKHSL::get_slider_label(int idx) const {
	ERR_FAIL_INDEX_V_MSG(idx, 3, String(), "Couldn't get slider label.");
	return labels[idx];
}

float ColorModeOKHSL::get_slider_max(int idx) const {
	ERR_FAIL_INDEX_V_MSG(idx, 4, 0, "Couldn't get slider max value.");
	return slider_max[idx];
}

float ColorModeOKHSL::get_slider_value(int idx) const {
	switch (idx) {
		case 0: {
			if (color_picker->get_pick_color().get_ok_hsl_s() > 0) {
				return color_picker->get_pick_color().get_ok_hsl_h() * 360.0;
			} else {
				return cached_hue;
			}
		}
		case 1: {
			if (color_picker->get_pick_color().get_ok_hsl_l() > 0) {
				return color_picker->get_pick_color().get_ok_hsl_s() * 100.0;
			} else {
				return cached_saturation;
			}
		}
		case 2:
			return color_picker->get_pick_color().get_ok_hsl_l() * 100.0;
		case 3:
			return Math::round(color_picker->get_pick_color().components[3] * 255.0);
		default:
			ERR_FAIL_V_MSG(0, "Couldn't get slider value.");
	}
}

Color ColorModeOKHSL::get_color() const {
	Vector<float> values = color_picker->get_active_slider_values();
	Color color;
	color.set_ok_hsl(values[0] / 360.0, values[1] / 100.0, values[2] / 100.0, values[3] / 255.0);
	return color;
}

void ColorModeOKHSL::slider_draw(int p_which) {
	HSlider *slider = color_picker->get_slider(p_which);
	Size2 size = slider->get_size();
	const real_t margin = 16 * color_picker->theme_cache.base_scale;

	Vector<Vector2> pos;
	Vector<Color> col;
	Color left_color;
	Color right_color;
	Color color = color_picker->get_pick_color();

	if (p_which == 2) { // L
		pos.resize(6);
		col.resize(6);
		left_color = Color(0, 0, 0);
		Color middle_color;
		float slider_hue = (Math::is_zero_approx(color.get_ok_hsl_s())) ? cached_hue / 360.0 : color.get_ok_hsl_h();
		float slider_sat = (Math::is_zero_approx(color.get_ok_hsl_l())) ? cached_saturation / 100.0 : color.get_ok_hsl_s();

		middle_color.set_ok_hsl(slider_hue, slider_sat, 0.5);
		right_color.set_ok_hsl(slider_hue, slider_sat, 1);

		col.set(0, left_color);
		col.set(1, middle_color);
		col.set(2, right_color);
		col.set(3, right_color);
		col.set(4, middle_color);
		col.set(5, left_color);
		pos.set(0, Vector2(0, 0));
		pos.set(1, Vector2(size.x * 0.5, 0));
		pos.set(2, Vector2(size.x, 0));
		pos.set(3, Vector2(size.x, margin));
		pos.set(4, Vector2(size.x * 0.5, margin));
		pos.set(5, Vector2(0, margin));
	} else {
		pos.resize(4);
		col.resize(4);

		if (p_which == ColorPicker::SLIDER_COUNT) {
			slider->draw_texture_rect(color_picker->theme_cache.sample_bg, Rect2(Point2(0, 0), Size2(size.x, margin)), true);

			left_color = color;
			left_color.a = 0;
			right_color = color;
			right_color.a = 1;
		} else if (p_which == 0) {
			float l = color.get_ok_hsl_l();
			left_color = Color(l, l, l);
			right_color = left_color;
		} else {
			left_color.set_ok_hsl(color.get_ok_hsl_h(), 0, color.get_ok_hsl_l());
			float s_col_hue = (Math::is_zero_approx(color.get_ok_hsl_s())) ? cached_hue / 360.0 : color.get_ok_hsl_h();
			right_color.set_ok_hsl(s_col_hue, 1, color.get_ok_hsl_l());
		}

		col.set(0, left_color);
		col.set(1, right_color);
		col.set(2, right_color);
		col.set(3, left_color);
		pos.set(0, Vector2(0, 0));
		pos.set(1, Vector2(size.x, 0));
		pos.set(2, Vector2(size.x, margin));
		pos.set(3, Vector2(0, margin));
	}

	slider->draw_polygon(pos, col);

	if (p_which == 0) { // H
		Ref<Texture2D> hue = color_picker->theme_cache.color_okhsl_hue;
		slider->draw_texture_rect(hue, Rect2(Vector2(), Vector2(size.x, margin)), false, Color::from_hsv(0, 0, color.get_ok_hsl_l() * 2.0, color.get_ok_hsl_s()));
		return;
	}
}

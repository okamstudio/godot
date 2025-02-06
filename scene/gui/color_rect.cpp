/**************************************************************************/
/*  color_rect.cpp                                                        */
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

#include "color_rect.h"

void ColorRect::set_draw_background(bool p_draw_background) {
	if (draw_background == p_draw_background) {
		return;
	}
	draw_background = p_draw_background;
	queue_redraw();
	notify_property_list_changed();
}

bool ColorRect::is_drawing_background() const {
	return draw_background;
}

void ColorRect::set_color(const Color &p_color) {
	if (color == p_color) {
		return;
	}
	color = p_color;
	queue_redraw();
}

Color ColorRect::get_color() const {
	return color;
}

void ColorRect::set_antialiased(bool p_antialiased) {
	if (antialiased == p_antialiased) {
		return;
	}
	antialiased = p_antialiased;
	queue_redraw();
}

bool ColorRect::is_antialiased() const {
	return antialiased;
}

void ColorRect::set_draw_outline(bool p_draw_outline) {
	if (draw_outline == p_draw_outline) {
		return;
	}
	draw_outline = p_draw_outline;
	queue_redraw();
	notify_property_list_changed();
}

bool ColorRect::is_drawing_outline() const {
	return draw_outline;
}

void ColorRect::set_line_color(const Color &p_line_color) {
	if (line_color == p_line_color) {
		return;
	}
	line_color = p_line_color;
	queue_redraw();
}

Color ColorRect::get_line_color() const {
	return line_color;
}

void ColorRect::set_line_width(float p_line_width) {
	if (line_width == p_line_width) {
		return;
	}
	line_width = p_line_width;
	queue_redraw();
}

float ColorRect::get_line_width() const {
	return line_width;
}

void ColorRect::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_DRAW: {
			if (draw_background) {
				draw_rect(Rect2(Point2(), get_size()), color, true, -1.0f, antialiased);
			}

			if (draw_outline) {
				// Draw the rect's line on top of the fill shape.
				// Avoid warning message when antialiasing is enabled if line width does not support antialiasing.
				draw_rect(Rect2(Point2(), get_size()), line_color, false, line_width, line_width >= 0.0 ? antialiased : false);
			}
		} break;
	}
}

void ColorRect::_validate_property(PropertyInfo &p_property) const {
	if (p_property.name == "color") {
		if (!draw_background) {
			p_property.usage = PROPERTY_USAGE_NO_EDITOR;
		}
	}

	if (p_property.name == "line_color" || p_property.name == "line_width") {
		if (!draw_outline) {
			p_property.usage = PROPERTY_USAGE_NO_EDITOR;
		}
	}
}

void ColorRect::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_draw_background", "draw_background"), &ColorRect::set_draw_background);
	ClassDB::bind_method(D_METHOD("is_drawing_background"), &ColorRect::is_drawing_background);

	ClassDB::bind_method(D_METHOD("set_color", "color"), &ColorRect::set_color);
	ClassDB::bind_method(D_METHOD("get_color"), &ColorRect::get_color);

	ClassDB::bind_method(D_METHOD("set_antialiased", "antialiased"), &ColorRect::set_antialiased);
	ClassDB::bind_method(D_METHOD("is_antialiased"), &ColorRect::is_antialiased);

	ClassDB::bind_method(D_METHOD("set_draw_outline", "draw_outline"), &ColorRect::set_draw_outline);
	ClassDB::bind_method(D_METHOD("is_drawing_outline"), &ColorRect::is_drawing_outline);

	ClassDB::bind_method(D_METHOD("set_line_color", "color"), &ColorRect::set_line_color);
	ClassDB::bind_method(D_METHOD("get_line_color"), &ColorRect::get_line_color);

	ClassDB::bind_method(D_METHOD("set_line_width", "line_width"), &ColorRect::set_line_width);
	ClassDB::bind_method(D_METHOD("get_line_width"), &ColorRect::get_line_width);

	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "draw_background"), "set_draw_background", "is_drawing_background");
	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "color"), "set_color", "get_color");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "antialiased"), "set_antialiased", "is_antialiased");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "draw_outline"), "set_draw_outline", "is_drawing_outline");
	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "line_color"), "set_line_color", "get_line_color");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "line_width", PROPERTY_HINT_RANGE, "-1.0,100.0,0.01,or_greater"), "set_line_width", "get_line_width");
}

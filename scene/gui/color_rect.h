/**************************************************************************/
/*  color_rect.h                                                          */
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

#ifndef COLOR_RECT_H
#define COLOR_RECT_H

#include "scene/gui/control.h"

class ColorRect : public Control {
	GDCLASS(ColorRect, Control);

	bool draw_background = true;
	Color color = Color(1, 1, 1);
	bool antialiased = false;
	bool draw_outline = false;
	Color line_color = Color(1, 1, 1);
	float line_width = 1.0f;

protected:
	void _notification(int p_what);
	void _validate_property(PropertyInfo &p_property) const;
	static void _bind_methods();

public:
	void set_draw_background(bool p_draw_background);
	bool is_drawing_background() const;

	void set_color(const Color &p_color);
	Color get_color() const;

	void set_antialiased(bool p_antialiased);
	bool is_antialiased() const;

	void set_draw_outline(bool p_draw_outline);
	bool is_drawing_outline() const;

	void set_line_color(const Color &p_color);
	Color get_line_color() const;

	void set_line_width(float p_width);
	float get_line_width() const;
};

#endif // COLOR_RECT_H

/**************************************************************************/
/*  label_settings.h                                                      */
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

#include "core/io/resource.h"
#include "font.h"

/*************************************************************************/

class LabelSettings : public Resource {
	GDCLASS(LabelSettings, Resource);

	real_t line_spacing = 3;
	real_t paragraph_spacing = 0;

	Ref<Font> font;
	int font_size = Font::DEFAULT_FONT_SIZE;
	Color font_color = Color(1, 1, 1);

	int outline_size = 0;
	Color outline_color = Color(1, 1, 1);

	int shadow_size = 1;
	Color shadow_color = Color(0, 0, 0, 0);
	Vector2 shadow_offset = Vector2(1, 1);

	void _font_changed();

protected:
	static void _bind_methods();

public:
	void set_line_spacing(real_t p_spacing);
	real_t get_line_spacing() const;

	void set_paragraph_spacing(real_t p_spacing);
	real_t get_paragraph_spacing() const;

	void set_font(const Ref<Font> &p_font);
	Ref<Font> get_font() const;

	void set_font_size(int p_size);
	int get_font_size() const;

	void set_font_color(const Color &p_color);
	Color get_font_color() const;

	void set_outline_size(int p_size);
	int get_outline_size() const;

	void set_outline_color(const Color &p_color);
	Color get_outline_color() const;

	void set_shadow_size(int p_size);
	int get_shadow_size() const;

	void set_shadow_color(const Color &p_color);
	Color get_shadow_color() const;

	void set_shadow_offset(const Vector2 &p_offset);
	Vector2 get_shadow_offset() const;
};

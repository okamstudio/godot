/**************************************************************************/
/*  rich_text_effect.h                                                    */
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
#include "core/object/gdvirtual.gen.inc"

class CharFXTransform : public RefCounted {
	GDCLASS(CharFXTransform, RefCounted);

protected:
	static void _bind_methods();

public:
	Transform2D transform;
	Vector2i range;
	bool visibility = true;
	bool outline = false;
	Point2 offset;
	Color color;
	double elapsed_time = 0.0f;
	Dictionary environment;
	uint32_t glyph_index = 0;
	uint16_t glyph_flags = 0;
	uint8_t glyph_count = 0;
	int32_t relative_index = 0;
	RID font;

	CharFXTransform();
	~CharFXTransform();

	void set_transform(const Transform2D &p_transform) { transform = p_transform; }
	const Transform2D &get_transform() { return transform; }

	Vector2i get_range() { return range; }
	void set_range(const Vector2i &p_range) { range = p_range; }

	double get_elapsed_time() { return elapsed_time; }
	void set_elapsed_time(double p_elapsed_time) { elapsed_time = p_elapsed_time; }

	bool is_visible() { return visibility; }
	void set_visibility(bool p_visibility) { visibility = p_visibility; }

	bool is_outline() { return outline; }
	void set_outline(bool p_outline) { outline = p_outline; }

	Point2 get_offset() { return offset; }
	void set_offset(Point2 p_offset) { offset = p_offset; }

	Color get_color() { return color; }
	void set_color(Color p_color) { color = p_color; }

	uint32_t get_glyph_index() const { return glyph_index; }
	void set_glyph_index(uint32_t p_glyph_index) { glyph_index = p_glyph_index; }

	uint16_t get_glyph_flags() const { return glyph_flags; }
	void set_glyph_flags(uint16_t p_glyph_flags) { glyph_flags = p_glyph_flags; }

	uint8_t get_glyph_count() const { return glyph_count; }
	void set_glyph_count(uint8_t p_glyph_count) { glyph_count = p_glyph_count; }

	int32_t get_relative_index() const { return relative_index; }
	void set_relative_index(int32_t p_relative_index) { relative_index = p_relative_index; }

	RID get_font() const { return font; }
	void set_font(RID p_font) { font = p_font; }

	Dictionary get_environment() { return environment; }
	void set_environment(Dictionary p_environment) { environment = p_environment; }
};

class RichTextEffect : public Resource {
	GDCLASS(RichTextEffect, Resource);
	OBJ_SAVE_TYPE(RichTextEffect);

protected:
	static void _bind_methods();

	GDVIRTUAL1RC(bool, _process_custom_fx, Ref<CharFXTransform>)

public:
	Variant get_bbcode() const;
	bool _process_effect_impl(Ref<class CharFXTransform> p_cfx);

	RichTextEffect();
};

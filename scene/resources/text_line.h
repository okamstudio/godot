/**************************************************************************/
/*  text_line.h                                                           */
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

#ifndef TEXT_LINE_H
#define TEXT_LINE_H

#include "scene/resources/font.h"
#include "servers/text_server.h"

/*************************************************************************/

class TextLine : public RefCounted {
	GDCLASS(TextLine, RefCounted);

private:
	RID rid;

	mutable bool dirty = true;

	float width = -1.0;
	BitField<TextServer::JustificationFlag> flags = TextServer::JUSTIFICATION_WORD_BOUND | TextServer::JUSTIFICATION_KASHIDA;
	HorizontalAlignment alignment = HORIZONTAL_ALIGNMENT_LEFT;
	String el_char = U"…";
	TextServer::OverrunBehavior overrun_behavior = TextServer::OVERRUN_TRIM_ELLIPSIS;

	Vector<float> tab_stops;

protected:
	static void _bind_methods();

	void _shape() const;

public:
	RID get_rid() const;

	void clear();

	void set_direction(TextServer::Direction p_direction);
	TextServer::Direction get_direction() const;

	void set_bidi_override(const Array &p_override);

	void set_orientation(TextServer::Orientation p_orientation);
	TextServer::Orientation get_orientation() const;

	void set_preserve_invalid(bool p_enabled);
	bool get_preserve_invalid() const;

	void set_preserve_control(bool p_enabled);
	bool get_preserve_control() const;

	bool add_string(const String &p_text, const Ref<Font> &p_font, int p_font_size, const String &p_language = "", const Variant &p_meta = Variant());
	bool add_object(Variant p_key, const Size2 &p_size, InlineAlignment p_inline_align = INLINE_ALIGNMENT_CENTER, int p_length = 1, float p_baseline = 0.0);
	bool resize_object(Variant p_key, const Size2 &p_size, InlineAlignment p_inline_align = INLINE_ALIGNMENT_CENTER, float p_baseline = 0.0);

	void set_horizontal_alignment(HorizontalAlignment p_alignment);
	HorizontalAlignment get_horizontal_alignment() const;

	void tab_align(const Vector<float> &p_tab_stops);

	void set_flags(BitField<TextServer::JustificationFlag> p_flags);
	BitField<TextServer::JustificationFlag> get_flags() const;

	void set_text_overrun_behavior(TextServer::OverrunBehavior p_behavior);
	TextServer::OverrunBehavior get_text_overrun_behavior() const;

	void set_ellipsis_char(const String &p_char);
	String get_ellipsis_char() const;

	void set_width(float p_width);
	float get_width() const;

	Array get_objects() const;
	Rect2 get_object_rect(Variant p_key) const;

	Size2 get_size() const;

	float get_line_ascent() const;
	float get_line_descent() const;
	float get_line_width() const;
	float get_line_underline_position() const;
	float get_line_underline_thickness() const;

	void draw(RID p_canvas, const Vector2 &p_pos, const Color &p_color = Color(1, 1, 1)) const;
	void draw_outline(RID p_canvas, const Vector2 &p_pos, int p_outline_size = 1, const Color &p_color = Color(1, 1, 1)) const;

	int hit_test(float p_coords) const;

	TextLine(const String &p_text, const Ref<Font> &p_font, int p_font_size, const String &p_language = "", TextServer::Direction p_direction = TextServer::DIRECTION_AUTO, TextServer::Orientation p_orientation = TextServer::ORIENTATION_HORIZONTAL);
	TextLine();
	~TextLine();
};

#endif // TEXT_LINE_H

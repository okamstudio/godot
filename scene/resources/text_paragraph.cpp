/**************************************************************************/
/*  text_paragraph.cpp                                                    */
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

#include "scene/resources/text_paragraph.h"

void TextParagraph::_bind_methods() {
	ClassDB::bind_method(D_METHOD("clear"), &TextParagraph::clear);

	ClassDB::bind_method(D_METHOD("set_direction", "direction"), &TextParagraph::set_direction);
	ClassDB::bind_method(D_METHOD("get_direction"), &TextParagraph::get_direction);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "direction", PROPERTY_HINT_ENUM, "Auto,Light-to-right,Right-to-left"), "set_direction", "get_direction");

	ClassDB::bind_method(D_METHOD("set_custom_punctuation", "custom_punctuation"), &TextParagraph::set_custom_punctuation);
	ClassDB::bind_method(D_METHOD("get_custom_punctuation"), &TextParagraph::get_custom_punctuation);

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "custom_punctuation"), "set_custom_punctuation", "get_custom_punctuation");

	ClassDB::bind_method(D_METHOD("set_orientation", "orientation"), &TextParagraph::set_orientation);
	ClassDB::bind_method(D_METHOD("get_orientation"), &TextParagraph::get_orientation);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "orientation", PROPERTY_HINT_ENUM, "Horizontal,Orientation"), "set_orientation", "get_orientation");

	ClassDB::bind_method(D_METHOD("set_preserve_invalid", "enabled"), &TextParagraph::set_preserve_invalid);
	ClassDB::bind_method(D_METHOD("get_preserve_invalid"), &TextParagraph::get_preserve_invalid);

	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "preserve_invalid"), "set_preserve_invalid", "get_preserve_invalid");

	ClassDB::bind_method(D_METHOD("set_preserve_control", "enabled"), &TextParagraph::set_preserve_control);
	ClassDB::bind_method(D_METHOD("get_preserve_control"), &TextParagraph::get_preserve_control);

	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "preserve_control"), "set_preserve_control", "get_preserve_control");

	ClassDB::bind_method(D_METHOD("set_bidi_override", "override"), &TextParagraph::set_bidi_override);

	ClassDB::bind_method(D_METHOD("set_dropcap", "text", "font", "font_size", "dropcap_margins", "language"), &TextParagraph::set_dropcap, DEFVAL(Rect2()), DEFVAL(""));
	ClassDB::bind_method(D_METHOD("clear_dropcap"), &TextParagraph::clear_dropcap);

	ClassDB::bind_method(D_METHOD("add_string", "text", "font", "font_size", "language", "meta"), &TextParagraph::add_string, DEFVAL(""), DEFVAL(Variant()));
	ClassDB::bind_method(D_METHOD("add_object", "key", "size", "inline_align", "length", "baseline"), &TextParagraph::add_object, DEFVAL(INLINE_ALIGNMENT_CENTER), DEFVAL(1), DEFVAL(0.0));
	ClassDB::bind_method(D_METHOD("resize_object", "key", "size", "inline_align", "baseline"), &TextParagraph::resize_object, DEFVAL(INLINE_ALIGNMENT_CENTER), DEFVAL(0.0));

	ClassDB::bind_method(D_METHOD("set_alignment", "alignment"), &TextParagraph::set_alignment);
	ClassDB::bind_method(D_METHOD("get_alignment"), &TextParagraph::get_alignment);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "alignment", PROPERTY_HINT_ENUM, "Left,Center,Right,Fill"), "set_alignment", "get_alignment");

	ClassDB::bind_method(D_METHOD("tab_align", "tab_stops"), &TextParagraph::tab_align);

	ClassDB::bind_method(D_METHOD("set_break_flags", "flags"), &TextParagraph::set_break_flags);
	ClassDB::bind_method(D_METHOD("get_break_flags"), &TextParagraph::get_break_flags);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "break_flags", PROPERTY_HINT_FLAGS, "Mandatory,Word Bound,Grapheme Bound,Adaptive,Trim Spaces"), "set_break_flags", "get_break_flags");

	ClassDB::bind_method(D_METHOD("set_justification_flags", "flags"), &TextParagraph::set_justification_flags);
	ClassDB::bind_method(D_METHOD("get_justification_flags"), &TextParagraph::get_justification_flags);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "justification_flags", PROPERTY_HINT_FLAGS, "Kashida Justification:1,Word Justification:2,Trim Edge Spaces After Justification:4,Justify Only After Last Tab:8,Constrain Ellipsis:16,Skip Last Line:32,Skip Last Line With Visible Characters:64,Do Not Skip Single Line:128"), "set_justification_flags", "get_justification_flags");

	ClassDB::bind_method(D_METHOD("set_text_overrun_behavior", "overrun_behavior"), &TextParagraph::set_text_overrun_behavior);
	ClassDB::bind_method(D_METHOD("get_text_overrun_behavior"), &TextParagraph::get_text_overrun_behavior);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "text_overrun_behavior", PROPERTY_HINT_ENUM, "Trim Nothing,Trim Characters,Trim Words,Ellipsis,Word Ellipsis"), "set_text_overrun_behavior", "get_text_overrun_behavior");

	ClassDB::bind_method(D_METHOD("set_ellipsis_char", "char"), &TextParagraph::set_ellipsis_char);
	ClassDB::bind_method(D_METHOD("get_ellipsis_char"), &TextParagraph::get_ellipsis_char);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "ellipsis_char"), "set_ellipsis_char", "get_ellipsis_char");

	ClassDB::bind_method(D_METHOD("set_width", "width"), &TextParagraph::set_width);
	ClassDB::bind_method(D_METHOD("get_width"), &TextParagraph::get_width);

	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "width"), "set_width", "get_width");

	ClassDB::bind_method(D_METHOD("get_non_wrapped_size"), &TextParagraph::get_non_wrapped_size);
	ClassDB::bind_method(D_METHOD("get_size"), &TextParagraph::get_size);

	ClassDB::bind_method(D_METHOD("get_rid"), &TextParagraph::get_rid);
	ClassDB::bind_method(D_METHOD("get_line_rid", "line"), &TextParagraph::get_line_rid);
	ClassDB::bind_method(D_METHOD("get_dropcap_rid"), &TextParagraph::get_dropcap_rid);

	ClassDB::bind_method(D_METHOD("get_line_count"), &TextParagraph::get_line_count);

	ClassDB::bind_method(D_METHOD("set_max_lines_visible", "max_lines_visible"), &TextParagraph::set_max_lines_visible);
	ClassDB::bind_method(D_METHOD("get_max_lines_visible"), &TextParagraph::get_max_lines_visible);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "max_lines_visible"), "set_max_lines_visible", "get_max_lines_visible");

	ClassDB::bind_method(D_METHOD("set_line_spacing", "line_spacing"), &TextParagraph::set_line_spacing);
	ClassDB::bind_method(D_METHOD("get_line_spacing"), &TextParagraph::get_line_spacing);

	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "line_spacing"), "set_line_spacing", "get_line_spacing");

	ClassDB::bind_method(D_METHOD("get_line_objects", "line"), &TextParagraph::get_line_objects);
	ClassDB::bind_method(D_METHOD("get_line_object_rect", "line", "key"), &TextParagraph::get_line_object_rect);
	ClassDB::bind_method(D_METHOD("get_line_size", "line"), &TextParagraph::get_line_size);
	ClassDB::bind_method(D_METHOD("get_line_range", "line"), &TextParagraph::get_line_range);
	ClassDB::bind_method(D_METHOD("get_line_ascent", "line"), &TextParagraph::get_line_ascent);
	ClassDB::bind_method(D_METHOD("get_line_descent", "line"), &TextParagraph::get_line_descent);
	ClassDB::bind_method(D_METHOD("get_line_width", "line"), &TextParagraph::get_line_width);
	ClassDB::bind_method(D_METHOD("get_line_underline_position", "line"), &TextParagraph::get_line_underline_position);
	ClassDB::bind_method(D_METHOD("get_line_underline_thickness", "line"), &TextParagraph::get_line_underline_thickness);

	ClassDB::bind_method(D_METHOD("get_dropcap_size"), &TextParagraph::get_dropcap_size);
	ClassDB::bind_method(D_METHOD("get_dropcap_lines"), &TextParagraph::get_dropcap_lines);

	ClassDB::bind_method(D_METHOD("draw", "canvas", "pos", "color", "dc_color"), &TextParagraph::draw, DEFVAL(Color(1, 1, 1)), DEFVAL(Color(1, 1, 1)));
	ClassDB::bind_method(D_METHOD("draw_outline", "canvas", "pos", "outline_size", "color", "dc_color"), &TextParagraph::draw_outline, DEFVAL(1), DEFVAL(Color(1, 1, 1)), DEFVAL(Color(1, 1, 1)));

	ClassDB::bind_method(D_METHOD("draw_line", "canvas", "pos", "line", "color"), &TextParagraph::draw_line, DEFVAL(Color(1, 1, 1)));
	ClassDB::bind_method(D_METHOD("draw_line_outline", "canvas", "pos", "line", "outline_size", "color"), &TextParagraph::draw_line_outline, DEFVAL(1), DEFVAL(Color(1, 1, 1)));

	ClassDB::bind_method(D_METHOD("draw_dropcap", "canvas", "pos", "color"), &TextParagraph::draw_dropcap, DEFVAL(Color(1, 1, 1)));
	ClassDB::bind_method(D_METHOD("draw_dropcap_outline", "canvas", "pos", "outline_size", "color"), &TextParagraph::draw_dropcap_outline, DEFVAL(1), DEFVAL(Color(1, 1, 1)));

	ClassDB::bind_method(D_METHOD("hit_test", "coords"), &TextParagraph::hit_test);
}

void TextParagraph::_shape_lines() const {
	// When a shaped text is invalidated by an external source, we want to reshape it.
	if (!TS->shaped_text_is_ready(rid) || !TS->shaped_text_is_ready(dropcap_rid)) {
		lines_dirty = true;
	}

	for (const RID &line_rid : lines_rid) {
		if (!TS->shaped_text_is_ready(line_rid)) {
			lines_dirty = true;
			break;
		}
	}

	if (lines_dirty) {
		for (const RID &line_rid : lines_rid) {
			TS->free_rid(line_rid);
		}
		lines_rid.clear();

		if (!tab_stops.is_empty()) {
			TS->shaped_text_tab_align(rid, tab_stops);
		}

		float h_offset = 0.f;
		float v_offset = 0.f;
		int start = 0;
		dropcap_lines = 0;

		if (TS->shaped_text_get_orientation(dropcap_rid) == TextServer::ORIENTATION_HORIZONTAL) {
			h_offset = TS->shaped_text_get_size(dropcap_rid).x + dropcap_margins.size.x + dropcap_margins.position.x;
			v_offset = TS->shaped_text_get_size(dropcap_rid).y + dropcap_margins.size.y + dropcap_margins.position.y;
		} else {
			h_offset = TS->shaped_text_get_size(dropcap_rid).y + dropcap_margins.size.y + dropcap_margins.position.y;
			v_offset = TS->shaped_text_get_size(dropcap_rid).x + dropcap_margins.size.x + dropcap_margins.position.x;
		}

		Size2i range = TS->shaped_text_get_range(rid);
		if (h_offset > 0) {
			// Dropcap, flow around.
			PackedInt32Array line_breaks = TS->shaped_text_get_line_breaks(rid, width - h_offset, 0, brk_flags);
			for (int i = 0; i < line_breaks.size(); i = i + 2) {
				RID line = TS->shaped_text_substr(rid, line_breaks[i], line_breaks[i + 1] - line_breaks[i]);
				float h = (TS->shaped_text_get_orientation(line) == TextServer::ORIENTATION_HORIZONTAL) ? TS->shaped_text_get_size(line).y : TS->shaped_text_get_size(line).x;
				h += line_spacing;
				if (!tab_stops.is_empty()) {
					TS->shaped_text_tab_align(line, tab_stops);
				}
				start = (i < line_breaks.size() - 2) ? line_breaks[i + 2] : range.y;
				lines_rid.push_back(line);
				if (v_offset < h) {
					break;
				}
				dropcap_lines++;
				v_offset -= h;
			}
		}
		// Use fixed for the rest of lines.
		if (start == 0 || start < range.y) {
			PackedInt32Array line_breaks = TS->shaped_text_get_line_breaks(rid, width, start, brk_flags);
			for (int i = 0; i < line_breaks.size(); i = i + 2) {
				RID line = TS->shaped_text_substr(rid, line_breaks[i], line_breaks[i + 1] - line_breaks[i]);
				if (!tab_stops.is_empty()) {
					TS->shaped_text_tab_align(line, tab_stops);
				}
				lines_rid.push_back(line);
			}
		}

		BitField<TextServer::TextOverrunFlag> overrun_flags = TextServer::OVERRUN_NO_TRIM;
		if (overrun_behavior != TextServer::OVERRUN_NO_TRIMMING) {
			switch (overrun_behavior) {
				case TextServer::OVERRUN_TRIM_WORD_ELLIPSIS:
					overrun_flags.set_flag(TextServer::OVERRUN_TRIM);
					overrun_flags.set_flag(TextServer::OVERRUN_TRIM_WORD_ONLY);
					overrun_flags.set_flag(TextServer::OVERRUN_ADD_ELLIPSIS);
					break;
				case TextServer::OVERRUN_TRIM_ELLIPSIS:
					overrun_flags.set_flag(TextServer::OVERRUN_TRIM);
					overrun_flags.set_flag(TextServer::OVERRUN_ADD_ELLIPSIS);
					break;
				case TextServer::OVERRUN_TRIM_WORD:
					overrun_flags.set_flag(TextServer::OVERRUN_TRIM);
					overrun_flags.set_flag(TextServer::OVERRUN_TRIM_WORD_ONLY);
					break;
				case TextServer::OVERRUN_TRIM_CHAR:
					overrun_flags.set_flag(TextServer::OVERRUN_TRIM);
					break;
				case TextServer::OVERRUN_NO_TRIMMING:
					break;
			}
		}

		bool autowrap_enabled = brk_flags.has_flag(TextServer::BREAK_WORD_BOUND) || brk_flags.has_flag(TextServer::BREAK_GRAPHEME_BOUND);

		// Fill after min_size calculation.
		if (autowrap_enabled) {
			int visible_lines = (max_lines_visible >= 0) ? MIN(max_lines_visible, (int)lines_rid.size()) : (int)lines_rid.size();
			bool lines_hidden = visible_lines > 0 && visible_lines < (int)lines_rid.size();
			if (lines_hidden) {
				overrun_flags.set_flag(TextServer::OVERRUN_ENFORCE_ELLIPSIS);
			}
			if (alignment == HORIZONTAL_ALIGNMENT_FILL) {
				int jst_to_line = visible_lines;
				if (lines_rid.size() == 1 && jst_flags.has_flag(TextServer::JUSTIFICATION_DO_NOT_SKIP_SINGLE_LINE)) {
					jst_to_line = lines_rid.size();
				} else {
					if (jst_flags.has_flag(TextServer::JUSTIFICATION_SKIP_LAST_LINE)) {
						jst_to_line = visible_lines - 1;
					}
					if (jst_flags.has_flag(TextServer::JUSTIFICATION_SKIP_LAST_LINE_WITH_VISIBLE_CHARS)) {
						for (int i = visible_lines - 1; i >= 0; i--) {
							if (TS->shaped_text_has_visible_chars(lines_rid[i])) {
								jst_to_line = i;
								break;
							}
						}
					}
				}
				for (int i = 0; i < (int)lines_rid.size(); i++) {
					float line_w = (i <= dropcap_lines) ? (width - h_offset) : width;
					if (i < jst_to_line) {
						TS->shaped_text_fit_to_width(lines_rid[i], line_w, jst_flags);
					} else if (i == (visible_lines - 1)) {
						TS->shaped_text_set_custom_ellipsis(lines_rid[visible_lines - 1], (el_char.length() > 0) ? el_char[0] : 0x2026);
						TS->shaped_text_overrun_trim_to_width(lines_rid[visible_lines - 1], line_w, overrun_flags);
					}
				}
			} else if (lines_hidden) {
				TS->shaped_text_set_custom_ellipsis(lines_rid[visible_lines - 1], (el_char.length() > 0) ? el_char[0] : 0x2026);
				TS->shaped_text_overrun_trim_to_width(lines_rid[visible_lines - 1], (visible_lines - 1 <= dropcap_lines) ? (width - h_offset) : width, overrun_flags);
			}
		} else {
			// Autowrap disabled.
			int jst_to_line = lines_rid.size();
			if (lines_rid.size() == 1 && jst_flags.has_flag(TextServer::JUSTIFICATION_DO_NOT_SKIP_SINGLE_LINE)) {
				jst_to_line = lines_rid.size();
			} else {
				if (jst_flags.has_flag(TextServer::JUSTIFICATION_SKIP_LAST_LINE)) {
					jst_to_line = lines_rid.size() - 1;
				}
				if (jst_flags.has_flag(TextServer::JUSTIFICATION_SKIP_LAST_LINE_WITH_VISIBLE_CHARS)) {
					for (int i = lines_rid.size() - 1; i >= 0; i--) {
						if (TS->shaped_text_has_visible_chars(lines_rid[i])) {
							jst_to_line = i;
							break;
						}
					}
				}
			}
			for (int i = 0; i < (int)lines_rid.size(); i++) {
				float line_w = (i <= dropcap_lines) ? (width - h_offset) : width;
				if (i < jst_to_line && alignment == HORIZONTAL_ALIGNMENT_FILL) {
					TS->shaped_text_fit_to_width(lines_rid[i], line_w, jst_flags);
					overrun_flags.set_flag(TextServer::OVERRUN_JUSTIFICATION_AWARE);
					TS->shaped_text_set_custom_ellipsis(lines_rid[i], (el_char.length() > 0) ? el_char[0] : 0x2026);
					TS->shaped_text_overrun_trim_to_width(lines_rid[i], line_w, overrun_flags);
					TS->shaped_text_fit_to_width(lines_rid[i], line_w, jst_flags | TextServer::JUSTIFICATION_CONSTRAIN_ELLIPSIS);
				} else {
					TS->shaped_text_set_custom_ellipsis(lines_rid[i], (el_char.length() > 0) ? el_char[0] : 0x2026);
					TS->shaped_text_overrun_trim_to_width(lines_rid[i], line_w, overrun_flags);
				}
			}
		}
		lines_dirty = false;
	}
}

RID TextParagraph::get_rid() const {
	return rid;
}

RID TextParagraph::get_line_rid(int p_line) const {
	_THREAD_SAFE_METHOD_

	_shape_lines();
	ERR_FAIL_COND_V(p_line < 0 || p_line >= (int)lines_rid.size(), RID());
	return lines_rid[p_line];
}

RID TextParagraph::get_dropcap_rid() const {
	return dropcap_rid;
}

void TextParagraph::clear() {
	_THREAD_SAFE_METHOD_

	for (const RID &line_rid : lines_rid) {
		TS->free_rid(line_rid);
	}
	lines_rid.clear();
	TS->shaped_text_clear(rid);
	TS->shaped_text_clear(dropcap_rid);
}

void TextParagraph::set_preserve_invalid(bool p_enabled) {
	_THREAD_SAFE_METHOD_

	TS->shaped_text_set_preserve_invalid(rid, p_enabled);
	TS->shaped_text_set_preserve_invalid(dropcap_rid, p_enabled);
	lines_dirty = true;
}

bool TextParagraph::get_preserve_invalid() const {
	_THREAD_SAFE_METHOD_

	return TS->shaped_text_get_preserve_invalid(rid);
}

void TextParagraph::set_preserve_control(bool p_enabled) {
	_THREAD_SAFE_METHOD_

	TS->shaped_text_set_preserve_control(rid, p_enabled);
	TS->shaped_text_set_preserve_control(dropcap_rid, p_enabled);
	lines_dirty = true;
}

bool TextParagraph::get_preserve_control() const {
	_THREAD_SAFE_METHOD_

	return TS->shaped_text_get_preserve_control(rid);
}

void TextParagraph::set_direction(TextServer::Direction p_direction) {
	_THREAD_SAFE_METHOD_

	TS->shaped_text_set_direction(rid, p_direction);
	TS->shaped_text_set_direction(dropcap_rid, p_direction);
	lines_dirty = true;
}

TextServer::Direction TextParagraph::get_direction() const {
	_THREAD_SAFE_METHOD_

	_shape_lines();
	return TS->shaped_text_get_direction(rid);
}

void TextParagraph::set_custom_punctuation(const String &p_punct) {
	_THREAD_SAFE_METHOD_

	TS->shaped_text_set_custom_punctuation(rid, p_punct);
	lines_dirty = true;
}

String TextParagraph::get_custom_punctuation() const {
	_THREAD_SAFE_METHOD_

	return TS->shaped_text_get_custom_punctuation(rid);
}

void TextParagraph::set_orientation(TextServer::Orientation p_orientation) {
	_THREAD_SAFE_METHOD_

	TS->shaped_text_set_orientation(rid, p_orientation);
	TS->shaped_text_set_orientation(dropcap_rid, p_orientation);
	lines_dirty = true;
}

TextServer::Orientation TextParagraph::get_orientation() const {
	_THREAD_SAFE_METHOD_

	_shape_lines();
	return TS->shaped_text_get_orientation(rid);
}

bool TextParagraph::set_dropcap(const String &p_text, const Ref<Font> &p_font, int p_font_size, const Rect2 &p_dropcap_margins, const String &p_language) {
	_THREAD_SAFE_METHOD_
	ERR_FAIL_COND_V(p_font.is_null(), false);
	TS->shaped_text_clear(dropcap_rid);
	dropcap_margins = p_dropcap_margins;
	bool res = TS->shaped_text_add_string(dropcap_rid, p_text, p_font->get_rids(), p_font_size, p_font->get_opentype_features(), p_language);
	lines_dirty = true;
	return res;
}

void TextParagraph::clear_dropcap() {
	_THREAD_SAFE_METHOD_
	dropcap_margins = Rect2();
	TS->shaped_text_clear(dropcap_rid);
	lines_dirty = true;
}

bool TextParagraph::add_string(const String &p_text, const Ref<Font> &p_font, int p_font_size, const String &p_language, const Variant &p_meta) {
	_THREAD_SAFE_METHOD_
	ERR_FAIL_COND_V(p_font.is_null(), false);
	bool res = TS->shaped_text_add_string(rid, p_text, p_font->get_rids(), p_font_size, p_font->get_opentype_features(), p_language, p_meta);
	lines_dirty = true;
	return res;
}

void TextParagraph::set_bidi_override(const Array &p_override) {
	_THREAD_SAFE_METHOD_

	TS->shaped_text_set_bidi_override(rid, p_override);
	lines_dirty = true;
}

bool TextParagraph::add_object(Variant p_key, const Size2 &p_size, InlineAlignment p_inline_align, int p_length, float p_baseline) {
	_THREAD_SAFE_METHOD_

	bool res = TS->shaped_text_add_object(rid, p_key, p_size, p_inline_align, p_length, p_baseline);
	lines_dirty = true;
	return res;
}

bool TextParagraph::resize_object(Variant p_key, const Size2 &p_size, InlineAlignment p_inline_align, float p_baseline) {
	_THREAD_SAFE_METHOD_

	bool res = TS->shaped_text_resize_object(rid, p_key, p_size, p_inline_align, p_baseline);
	lines_dirty = true;
	return res;
}

void TextParagraph::set_alignment(HorizontalAlignment p_alignment) {
	_THREAD_SAFE_METHOD_

	if (alignment != p_alignment) {
		if (alignment == HORIZONTAL_ALIGNMENT_FILL || p_alignment == HORIZONTAL_ALIGNMENT_FILL) {
			alignment = p_alignment;
			lines_dirty = true;
		} else {
			alignment = p_alignment;
		}
	}
}

HorizontalAlignment TextParagraph::get_alignment() const {
	return alignment;
}

void TextParagraph::tab_align(const Vector<float> &p_tab_stops) {
	_THREAD_SAFE_METHOD_

	tab_stops = p_tab_stops;
	lines_dirty = true;
}

void TextParagraph::set_justification_flags(BitField<TextServer::JustificationFlag> p_flags) {
	_THREAD_SAFE_METHOD_

	if (jst_flags != p_flags) {
		jst_flags = p_flags;
		lines_dirty = true;
	}
}

BitField<TextServer::JustificationFlag> TextParagraph::get_justification_flags() const {
	return jst_flags;
}

void TextParagraph::set_break_flags(BitField<TextServer::LineBreakFlag> p_flags) {
	_THREAD_SAFE_METHOD_

	if (brk_flags != p_flags) {
		brk_flags = p_flags;
		lines_dirty = true;
	}
}

BitField<TextServer::LineBreakFlag> TextParagraph::get_break_flags() const {
	return brk_flags;
}

void TextParagraph::set_text_overrun_behavior(TextServer::OverrunBehavior p_behavior) {
	_THREAD_SAFE_METHOD_

	if (overrun_behavior != p_behavior) {
		overrun_behavior = p_behavior;
		lines_dirty = true;
	}
}

TextServer::OverrunBehavior TextParagraph::get_text_overrun_behavior() const {
	return overrun_behavior;
}

void TextParagraph::set_ellipsis_char(const String &p_char) {
	String c = p_char;
	if (c.length() > 1) {
		WARN_PRINT("Ellipsis must be exactly one character long (" + itos(c.length()) + " characters given).");
		c = c.left(1);
	}
	if (el_char == c) {
		return;
	}
	el_char = c;
	lines_dirty = true;
}

String TextParagraph::get_ellipsis_char() const {
	return el_char;
}

void TextParagraph::set_width(float p_width) {
	_THREAD_SAFE_METHOD_

	if (width != p_width) {
		width = p_width;
		lines_dirty = true;
	}
}

float TextParagraph::get_width() const {
	return width;
}

Size2 TextParagraph::get_non_wrapped_size() const {
	_THREAD_SAFE_METHOD_

	_shape_lines();
	return TS->shaped_text_get_size(rid);
}

Size2 TextParagraph::get_size() const {
	_THREAD_SAFE_METHOD_

	_shape_lines();

	float h_offset = 0.f;
	float v_offset = 0.f;
	if (TS->shaped_text_get_orientation(dropcap_rid) == TextServer::ORIENTATION_HORIZONTAL) {
		h_offset = TS->shaped_text_get_size(dropcap_rid).x + dropcap_margins.size.x + dropcap_margins.position.x;
		v_offset = TS->shaped_text_get_size(dropcap_rid).y + dropcap_margins.size.y + dropcap_margins.position.y;
	} else {
		h_offset = TS->shaped_text_get_size(dropcap_rid).y + dropcap_margins.size.y + dropcap_margins.position.y;
		v_offset = TS->shaped_text_get_size(dropcap_rid).x + dropcap_margins.size.x + dropcap_margins.position.x;
	}

	Size2 size;
	int visible_lines = (max_lines_visible >= 0) ? MIN(max_lines_visible, (int)lines_rid.size()) : (int)lines_rid.size();
	for (int i = 0; i < visible_lines; i++) {
		Size2 lsize = TS->shaped_text_get_size(lines_rid[i]);
		if (TS->shaped_text_get_orientation(lines_rid[i]) == TextServer::ORIENTATION_HORIZONTAL) {
			if (h_offset > 0 && i <= dropcap_lines) {
				lsize.x += h_offset;
			}
			size.x = MAX(size.x, lsize.x);
			size.y += lsize.y;
			if (i != visible_lines - 1) {
				size.y += line_spacing;
			}
		} else {
			if (h_offset > 0 && i <= dropcap_lines) {
				lsize.y += h_offset;
			}
			size.x += lsize.x;
			size.y = MAX(size.y, lsize.y);
			if (i != visible_lines - 1) {
				size.x += line_spacing;
			}
		}
	}
	if (h_offset > 0) {
		if (TS->shaped_text_get_orientation(dropcap_rid) == TextServer::ORIENTATION_HORIZONTAL) {
			size.y = MAX(size.y, v_offset);
		} else {
			size.x = MAX(size.x, v_offset);
		}
	}
	return size;
}

int TextParagraph::get_line_count() const {
	_THREAD_SAFE_METHOD_

	_shape_lines();
	return (int)lines_rid.size();
}

void TextParagraph::set_max_lines_visible(int p_lines) {
	_THREAD_SAFE_METHOD_

	if (p_lines != max_lines_visible) {
		max_lines_visible = p_lines;
		lines_dirty = true;
	}
}

int TextParagraph::get_max_lines_visible() const {
	return max_lines_visible;
}

void TextParagraph::set_line_spacing(float p_spacing) {
	_THREAD_SAFE_METHOD_

	if (line_spacing != p_spacing) {
		line_spacing = p_spacing;
		lines_dirty = true;
	}
}

float TextParagraph::get_line_spacing() const {
	return line_spacing;
}

Array TextParagraph::get_line_objects(int p_line) const {
	_THREAD_SAFE_METHOD_

	_shape_lines();
	ERR_FAIL_COND_V(p_line < 0 || p_line >= (int)lines_rid.size(), Array());
	return TS->shaped_text_get_objects(lines_rid[p_line]);
}

Rect2 TextParagraph::get_line_object_rect(int p_line, Variant p_key) const {
	_THREAD_SAFE_METHOD_

	_shape_lines();
	ERR_FAIL_COND_V(p_line < 0 || p_line >= (int)lines_rid.size(), Rect2());

	Vector2 ofs;

	float h_offset = 0.f;
	if (TS->shaped_text_get_orientation(dropcap_rid) == TextServer::ORIENTATION_HORIZONTAL) {
		h_offset = TS->shaped_text_get_size(dropcap_rid).x + dropcap_margins.size.x + dropcap_margins.position.x;
	} else {
		h_offset = TS->shaped_text_get_size(dropcap_rid).y + dropcap_margins.size.y + dropcap_margins.position.y;
	}

	for (int i = 0; i <= p_line; i++) {
		float l_width = width;
		if (TS->shaped_text_get_orientation(lines_rid[i]) == TextServer::ORIENTATION_HORIZONTAL) {
			ofs.x = 0.f;
			ofs.y += TS->shaped_text_get_ascent(lines_rid[i]);
			if (i <= dropcap_lines) {
				if (TS->shaped_text_get_inferred_direction(dropcap_rid) == TextServer::DIRECTION_LTR) {
					ofs.x -= h_offset;
				}
				l_width -= h_offset;
			}
		} else {
			ofs.y = 0.f;
			ofs.x += TS->shaped_text_get_ascent(lines_rid[i]);
			if (i <= dropcap_lines) {
				if (TS->shaped_text_get_inferred_direction(dropcap_rid) == TextServer::DIRECTION_LTR) {
					ofs.y -= h_offset;
				}
				l_width -= h_offset;
			}
		}
		float length = TS->shaped_text_get_width(lines_rid[i]);
		if (width > 0) {
			switch (alignment) {
				case HORIZONTAL_ALIGNMENT_FILL:
					if (TS->shaped_text_get_inferred_direction(lines_rid[i]) == TextServer::DIRECTION_RTL) {
						if (TS->shaped_text_get_orientation(lines_rid[i]) == TextServer::ORIENTATION_HORIZONTAL) {
							ofs.x += l_width - length;
						} else {
							ofs.y += l_width - length;
						}
					}
					break;
				case HORIZONTAL_ALIGNMENT_LEFT:
					break;
				case HORIZONTAL_ALIGNMENT_CENTER: {
					if (length <= l_width) {
						if (TS->shaped_text_get_orientation(lines_rid[i]) == TextServer::ORIENTATION_HORIZONTAL) {
							ofs.x += Math::floor((l_width - length) / 2.0);
						} else {
							ofs.y += Math::floor((l_width - length) / 2.0);
						}
					} else if (TS->shaped_text_get_inferred_direction(lines_rid[i]) == TextServer::DIRECTION_RTL) {
						if (TS->shaped_text_get_orientation(lines_rid[i]) == TextServer::ORIENTATION_HORIZONTAL) {
							ofs.x += l_width - length;
						} else {
							ofs.y += l_width - length;
						}
					}
				} break;
				case HORIZONTAL_ALIGNMENT_RIGHT: {
					if (TS->shaped_text_get_orientation(lines_rid[i]) == TextServer::ORIENTATION_HORIZONTAL) {
						ofs.x += l_width - length;
					} else {
						ofs.y += l_width - length;
					}
				} break;
			}
		}
		if (i != p_line) {
			if (TS->shaped_text_get_orientation(lines_rid[i]) == TextServer::ORIENTATION_HORIZONTAL) {
				ofs.x = 0.f;
				ofs.y += TS->shaped_text_get_descent(lines_rid[i]) + line_spacing;
			} else {
				ofs.y = 0.f;
				ofs.x += TS->shaped_text_get_descent(lines_rid[i]) + line_spacing;
			}
		}
	}

	Rect2 rect = TS->shaped_text_get_object_rect(lines_rid[p_line], p_key);
	rect.position += ofs;

	return rect;
}

Size2 TextParagraph::get_line_size(int p_line) const {
	_THREAD_SAFE_METHOD_

	_shape_lines();
	ERR_FAIL_COND_V(p_line < 0 || p_line >= (int)lines_rid.size(), Size2());
	return TS->shaped_text_get_size(lines_rid[p_line]);
}

Vector2i TextParagraph::get_line_range(int p_line) const {
	_THREAD_SAFE_METHOD_

	_shape_lines();
	ERR_FAIL_COND_V(p_line < 0 || p_line >= (int)lines_rid.size(), Vector2i());
	return TS->shaped_text_get_range(lines_rid[p_line]);
}

float TextParagraph::get_line_ascent(int p_line) const {
	_THREAD_SAFE_METHOD_

	_shape_lines();
	ERR_FAIL_COND_V(p_line < 0 || p_line >= (int)lines_rid.size(), 0.f);
	return TS->shaped_text_get_ascent(lines_rid[p_line]);
}

float TextParagraph::get_line_descent(int p_line) const {
	_THREAD_SAFE_METHOD_

	_shape_lines();
	ERR_FAIL_COND_V(p_line < 0 || p_line >= (int)lines_rid.size(), 0.f);
	return TS->shaped_text_get_descent(lines_rid[p_line]);
}

float TextParagraph::get_line_width(int p_line) const {
	_THREAD_SAFE_METHOD_

	_shape_lines();
	ERR_FAIL_COND_V(p_line < 0 || p_line >= (int)lines_rid.size(), 0.f);
	return TS->shaped_text_get_width(lines_rid[p_line]);
}

float TextParagraph::get_line_underline_position(int p_line) const {
	_THREAD_SAFE_METHOD_

	_shape_lines();
	ERR_FAIL_COND_V(p_line < 0 || p_line >= (int)lines_rid.size(), 0.f);
	return TS->shaped_text_get_underline_position(lines_rid[p_line]);
}

float TextParagraph::get_line_underline_thickness(int p_line) const {
	_THREAD_SAFE_METHOD_

	_shape_lines();
	ERR_FAIL_COND_V(p_line < 0 || p_line >= (int)lines_rid.size(), 0.f);
	return TS->shaped_text_get_underline_thickness(lines_rid[p_line]);
}

Size2 TextParagraph::get_dropcap_size() const {
	_THREAD_SAFE_METHOD_

	return TS->shaped_text_get_size(dropcap_rid) + dropcap_margins.size + dropcap_margins.position;
}

int TextParagraph::get_dropcap_lines() const {
	return dropcap_lines;
}

void TextParagraph::draw(RID p_canvas, const Vector2 &p_pos, const Color &p_color, const Color &p_dc_color) const {
	_THREAD_SAFE_METHOD_

	_shape_lines();
	Vector2 ofs = p_pos;
	float h_offset = 0.f;
	if (TS->shaped_text_get_orientation(dropcap_rid) == TextServer::ORIENTATION_HORIZONTAL) {
		h_offset = TS->shaped_text_get_size(dropcap_rid).x + dropcap_margins.size.x + dropcap_margins.position.x;
	} else {
		h_offset = TS->shaped_text_get_size(dropcap_rid).y + dropcap_margins.size.y + dropcap_margins.position.y;
	}

	if (h_offset > 0) {
		// Draw dropcap.
		Vector2 dc_off = ofs;
		if (TS->shaped_text_get_inferred_direction(dropcap_rid) == TextServer::DIRECTION_RTL) {
			if (TS->shaped_text_get_orientation(dropcap_rid) == TextServer::ORIENTATION_HORIZONTAL) {
				dc_off.x += width - h_offset;
			} else {
				dc_off.y += width - h_offset;
			}
		}
		TS->shaped_text_draw(dropcap_rid, p_canvas, dc_off + Vector2(0, TS->shaped_text_get_ascent(dropcap_rid) + dropcap_margins.size.y + dropcap_margins.position.y / 2), -1, -1, p_dc_color);
	}

	int lines_visible = (max_lines_visible >= 0) ? MIN(max_lines_visible, (int)lines_rid.size()) : (int)lines_rid.size();

	for (int i = 0; i < lines_visible; i++) {
		float l_width = width;
		if (TS->shaped_text_get_orientation(lines_rid[i]) == TextServer::ORIENTATION_HORIZONTAL) {
			ofs.x = p_pos.x;
			ofs.y += TS->shaped_text_get_ascent(lines_rid[i]);
			if (i <= dropcap_lines) {
				if (TS->shaped_text_get_inferred_direction(dropcap_rid) == TextServer::DIRECTION_LTR) {
					ofs.x -= h_offset;
				}
				l_width -= h_offset;
			}
		} else {
			ofs.y = p_pos.y;
			ofs.x += TS->shaped_text_get_ascent(lines_rid[i]);
			if (i <= dropcap_lines) {
				if (TS->shaped_text_get_inferred_direction(dropcap_rid) == TextServer::DIRECTION_LTR) {
					ofs.y -= h_offset;
				}
				l_width -= h_offset;
			}
		}
		float line_width = TS->shaped_text_get_width(lines_rid[i]);
		if (width > 0) {
			switch (alignment) {
				case HORIZONTAL_ALIGNMENT_FILL:
					if (TS->shaped_text_get_inferred_direction(lines_rid[i]) == TextServer::DIRECTION_RTL) {
						if (TS->shaped_text_get_orientation(lines_rid[i]) == TextServer::ORIENTATION_HORIZONTAL) {
							ofs.x += l_width - line_width;
						} else {
							ofs.y += l_width - line_width;
						}
					}
					break;
				case HORIZONTAL_ALIGNMENT_LEFT:
					break;
				case HORIZONTAL_ALIGNMENT_CENTER: {
					if (line_width <= l_width) {
						if (TS->shaped_text_get_orientation(lines_rid[i]) == TextServer::ORIENTATION_HORIZONTAL) {
							ofs.x += Math::floor((l_width - line_width) / 2.0);
						} else {
							ofs.y += Math::floor((l_width - line_width) / 2.0);
						}
					} else if (TS->shaped_text_get_inferred_direction(lines_rid[i]) == TextServer::DIRECTION_RTL) {
						if (TS->shaped_text_get_orientation(lines_rid[i]) == TextServer::ORIENTATION_HORIZONTAL) {
							ofs.x += l_width - line_width;
						} else {
							ofs.y += l_width - line_width;
						}
					}
				} break;
				case HORIZONTAL_ALIGNMENT_RIGHT: {
					if (TS->shaped_text_get_orientation(lines_rid[i]) == TextServer::ORIENTATION_HORIZONTAL) {
						ofs.x += l_width - line_width;
					} else {
						ofs.y += l_width - line_width;
					}
				} break;
			}
		}
		float clip_l;
		if (TS->shaped_text_get_orientation(lines_rid[i]) == TextServer::ORIENTATION_HORIZONTAL) {
			clip_l = MAX(0, p_pos.x - ofs.x);
		} else {
			clip_l = MAX(0, p_pos.y - ofs.y);
		}
		TS->shaped_text_draw(lines_rid[i], p_canvas, ofs, clip_l, clip_l + l_width, p_color);
		if (TS->shaped_text_get_orientation(lines_rid[i]) == TextServer::ORIENTATION_HORIZONTAL) {
			ofs.x = p_pos.x;
			ofs.y += TS->shaped_text_get_descent(lines_rid[i]) + line_spacing;
		} else {
			ofs.y = p_pos.y;
			ofs.x += TS->shaped_text_get_descent(lines_rid[i]) + line_spacing;
		}
	}
}

void TextParagraph::draw_outline(RID p_canvas, const Vector2 &p_pos, int p_outline_size, const Color &p_color, const Color &p_dc_color) const {
	_THREAD_SAFE_METHOD_

	_shape_lines();
	Vector2 ofs = p_pos;

	float h_offset = 0.f;
	if (TS->shaped_text_get_orientation(dropcap_rid) == TextServer::ORIENTATION_HORIZONTAL) {
		h_offset = TS->shaped_text_get_size(dropcap_rid).x + dropcap_margins.size.x + dropcap_margins.position.x;
	} else {
		h_offset = TS->shaped_text_get_size(dropcap_rid).y + dropcap_margins.size.y + dropcap_margins.position.y;
	}

	if (h_offset > 0) {
		// Draw dropcap.
		Vector2 dc_off = ofs;
		if (TS->shaped_text_get_inferred_direction(dropcap_rid) == TextServer::DIRECTION_RTL) {
			if (TS->shaped_text_get_orientation(dropcap_rid) == TextServer::ORIENTATION_HORIZONTAL) {
				dc_off.x += width - h_offset;
			} else {
				dc_off.y += width - h_offset;
			}
		}
		TS->shaped_text_draw_outline(dropcap_rid, p_canvas, dc_off + Vector2(dropcap_margins.position.x, TS->shaped_text_get_ascent(dropcap_rid) + dropcap_margins.position.y), -1, -1, p_outline_size, p_dc_color);
	}

	for (int i = 0; i < (int)lines_rid.size(); i++) {
		float l_width = width;
		if (TS->shaped_text_get_orientation(lines_rid[i]) == TextServer::ORIENTATION_HORIZONTAL) {
			ofs.x = p_pos.x;
			ofs.y += TS->shaped_text_get_ascent(lines_rid[i]);
			if (i <= dropcap_lines) {
				if (TS->shaped_text_get_inferred_direction(dropcap_rid) == TextServer::DIRECTION_LTR) {
					ofs.x -= h_offset;
				}
				l_width -= h_offset;
			}
		} else {
			ofs.y = p_pos.y;
			ofs.x += TS->shaped_text_get_ascent(lines_rid[i]);
			if (i <= dropcap_lines) {
				if (TS->shaped_text_get_inferred_direction(dropcap_rid) == TextServer::DIRECTION_LTR) {
					ofs.y -= h_offset;
				}
				l_width -= h_offset;
			}
		}
		float length = TS->shaped_text_get_width(lines_rid[i]);
		if (width > 0) {
			switch (alignment) {
				case HORIZONTAL_ALIGNMENT_FILL:
					if (TS->shaped_text_get_inferred_direction(lines_rid[i]) == TextServer::DIRECTION_RTL) {
						if (TS->shaped_text_get_orientation(lines_rid[i]) == TextServer::ORIENTATION_HORIZONTAL) {
							ofs.x += l_width - length;
						} else {
							ofs.y += l_width - length;
						}
					}
					break;
				case HORIZONTAL_ALIGNMENT_LEFT:
					break;
				case HORIZONTAL_ALIGNMENT_CENTER: {
					if (length <= l_width) {
						if (TS->shaped_text_get_orientation(lines_rid[i]) == TextServer::ORIENTATION_HORIZONTAL) {
							ofs.x += Math::floor((l_width - length) / 2.0);
						} else {
							ofs.y += Math::floor((l_width - length) / 2.0);
						}
					} else if (TS->shaped_text_get_inferred_direction(lines_rid[i]) == TextServer::DIRECTION_RTL) {
						if (TS->shaped_text_get_orientation(lines_rid[i]) == TextServer::ORIENTATION_HORIZONTAL) {
							ofs.x += l_width - length;
						} else {
							ofs.y += l_width - length;
						}
					}
				} break;
				case HORIZONTAL_ALIGNMENT_RIGHT: {
					if (TS->shaped_text_get_orientation(lines_rid[i]) == TextServer::ORIENTATION_HORIZONTAL) {
						ofs.x += l_width - length;
					} else {
						ofs.y += l_width - length;
					}
				} break;
			}
		}
		float clip_l;
		if (TS->shaped_text_get_orientation(lines_rid[i]) == TextServer::ORIENTATION_HORIZONTAL) {
			clip_l = MAX(0, p_pos.x - ofs.x);
		} else {
			clip_l = MAX(0, p_pos.y - ofs.y);
		}
		TS->shaped_text_draw_outline(lines_rid[i], p_canvas, ofs, clip_l, clip_l + l_width, p_outline_size, p_color);
		if (TS->shaped_text_get_orientation(lines_rid[i]) == TextServer::ORIENTATION_HORIZONTAL) {
			ofs.x = p_pos.x;
			ofs.y += TS->shaped_text_get_descent(lines_rid[i]) + line_spacing;
		} else {
			ofs.y = p_pos.y;
			ofs.x += TS->shaped_text_get_descent(lines_rid[i]) + line_spacing;
		}
	}
}

int TextParagraph::hit_test(const Point2 &p_coords) const {
	_THREAD_SAFE_METHOD_

	_shape_lines();
	Vector2 ofs;
	if (TS->shaped_text_get_orientation(rid) == TextServer::ORIENTATION_HORIZONTAL) {
		if (ofs.y < 0) {
			return 0;
		}
	} else {
		if (ofs.x < 0) {
			return 0;
		}
	}
	for (const RID &line_rid : lines_rid) {
		if (TS->shaped_text_get_orientation(line_rid) == TextServer::ORIENTATION_HORIZONTAL) {
			if ((p_coords.y >= ofs.y) && (p_coords.y <= ofs.y + TS->shaped_text_get_size(line_rid).y)) {
				return TS->shaped_text_hit_test_position(line_rid, p_coords.x);
			}
			ofs.y += TS->shaped_text_get_size(line_rid).y + line_spacing;
		} else {
			if ((p_coords.x >= ofs.x) && (p_coords.x <= ofs.x + TS->shaped_text_get_size(line_rid).x)) {
				return TS->shaped_text_hit_test_position(line_rid, p_coords.y);
			}
			ofs.y += TS->shaped_text_get_size(line_rid).x + line_spacing;
		}
	}
	return TS->shaped_text_get_range(rid).y;
}

bool TextParagraph::is_dirty() {
	return lines_dirty;
}

void TextParagraph::draw_dropcap(RID p_canvas, const Vector2 &p_pos, const Color &p_color) const {
	_THREAD_SAFE_METHOD_

	Vector2 ofs = p_pos;
	float h_offset = 0.f;
	if (TS->shaped_text_get_orientation(dropcap_rid) == TextServer::ORIENTATION_HORIZONTAL) {
		h_offset = TS->shaped_text_get_size(dropcap_rid).x + dropcap_margins.size.x + dropcap_margins.position.x;
	} else {
		h_offset = TS->shaped_text_get_size(dropcap_rid).y + dropcap_margins.size.y + dropcap_margins.position.y;
	}

	if (h_offset > 0) {
		// Draw dropcap.
		if (TS->shaped_text_get_inferred_direction(dropcap_rid) == TextServer::DIRECTION_RTL) {
			if (TS->shaped_text_get_orientation(dropcap_rid) == TextServer::ORIENTATION_HORIZONTAL) {
				ofs.x += width - h_offset;
			} else {
				ofs.y += width - h_offset;
			}
		}
		TS->shaped_text_draw(dropcap_rid, p_canvas, ofs + Vector2(dropcap_margins.position.x, TS->shaped_text_get_ascent(dropcap_rid) + dropcap_margins.position.y), -1, -1, p_color);
	}
}

void TextParagraph::draw_dropcap_outline(RID p_canvas, const Vector2 &p_pos, int p_outline_size, const Color &p_color) const {
	_THREAD_SAFE_METHOD_

	Vector2 ofs = p_pos;
	float h_offset = 0.f;
	if (TS->shaped_text_get_orientation(dropcap_rid) == TextServer::ORIENTATION_HORIZONTAL) {
		h_offset = TS->shaped_text_get_size(dropcap_rid).x + dropcap_margins.size.x + dropcap_margins.position.x;
	} else {
		h_offset = TS->shaped_text_get_size(dropcap_rid).y + dropcap_margins.size.y + dropcap_margins.position.y;
	}

	if (h_offset > 0) {
		// Draw dropcap.
		if (TS->shaped_text_get_inferred_direction(dropcap_rid) == TextServer::DIRECTION_RTL) {
			if (TS->shaped_text_get_orientation(dropcap_rid) == TextServer::ORIENTATION_HORIZONTAL) {
				ofs.x += width - h_offset;
			} else {
				ofs.y += width - h_offset;
			}
		}
		TS->shaped_text_draw_outline(dropcap_rid, p_canvas, ofs + Vector2(dropcap_margins.position.x, TS->shaped_text_get_ascent(dropcap_rid) + dropcap_margins.position.y), -1, -1, p_outline_size, p_color);
	}
}

void TextParagraph::draw_line(RID p_canvas, const Vector2 &p_pos, int p_line, const Color &p_color) const {
	_THREAD_SAFE_METHOD_

	_shape_lines();
	ERR_FAIL_COND(p_line < 0 || p_line >= (int)lines_rid.size());

	Vector2 ofs = p_pos;

	if (TS->shaped_text_get_orientation(lines_rid[p_line]) == TextServer::ORIENTATION_HORIZONTAL) {
		ofs.y += TS->shaped_text_get_ascent(lines_rid[p_line]);
	} else {
		ofs.x += TS->shaped_text_get_ascent(lines_rid[p_line]);
	}
	return TS->shaped_text_draw(lines_rid[p_line], p_canvas, ofs, -1, -1, p_color);
}

void TextParagraph::draw_line_outline(RID p_canvas, const Vector2 &p_pos, int p_line, int p_outline_size, const Color &p_color) const {
	_THREAD_SAFE_METHOD_

	_shape_lines();
	ERR_FAIL_COND(p_line < 0 || p_line >= (int)lines_rid.size());

	Vector2 ofs = p_pos;
	if (TS->shaped_text_get_orientation(lines_rid[p_line]) == TextServer::ORIENTATION_HORIZONTAL) {
		ofs.y += TS->shaped_text_get_ascent(lines_rid[p_line]);
	} else {
		ofs.x += TS->shaped_text_get_ascent(lines_rid[p_line]);
	}
	return TS->shaped_text_draw_outline(lines_rid[p_line], p_canvas, ofs, -1, -1, p_outline_size, p_color);
}

TextParagraph::TextParagraph(const String &p_text, const Ref<Font> &p_font, int p_font_size, const String &p_language, float p_width, TextServer::Direction p_direction, TextServer::Orientation p_orientation) {
	rid = TS->create_shaped_text(p_direction, p_orientation);
	if (p_font.is_valid()) {
		TS->shaped_text_add_string(rid, p_text, p_font->get_rids(), p_font_size, p_font->get_opentype_features(), p_language);
	}
	width = p_width;
}

TextParagraph::TextParagraph() {
	rid = TS->create_shaped_text();
	dropcap_rid = TS->create_shaped_text();
}

TextParagraph::~TextParagraph() {
	for (const RID &line_rid : lines_rid) {
		TS->free_rid(line_rid);
	}
	lines_rid.clear();
	TS->free_rid(rid);
	TS->free_rid(dropcap_rid);
}

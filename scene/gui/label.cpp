/**************************************************************************/
/*  label.cpp                                                             */
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

#include "label.h"

#include "scene/gui/container.h"
#include "scene/theme/theme_db.h"
#include "servers/text_server.h"

void Label::set_autowrap_mode(TextServer::AutowrapMode p_mode) {
	if (autowrap_mode == p_mode) {
		return;
	}

	autowrap_mode = p_mode;
	for (Paragraph &para : paragraphs) {
		para.lines_dirty = true;
	}
	queue_redraw();
	update_configuration_warnings();

	if (clip || overrun_behavior != TextServer::OVERRUN_NO_TRIMMING) {
		update_minimum_size();
	}
}

TextServer::AutowrapMode Label::get_autowrap_mode() const {
	return autowrap_mode;
}

void Label::set_justification_flags(BitField<TextServer::JustificationFlag> p_flags) {
	if (jst_flags == p_flags) {
		return;
	}

	jst_flags = p_flags;
	for (Paragraph &para : paragraphs) {
		para.lines_dirty = true;
	}
	queue_redraw();
}

BitField<TextServer::JustificationFlag> Label::get_justification_flags() const {
	return jst_flags;
}

void Label::set_uppercase(bool p_uppercase) {
	if (uppercase == p_uppercase) {
		return;
	}

	uppercase = p_uppercase;
	text_dirty = true;

	queue_redraw();
}

bool Label::is_uppercase() const {
	return uppercase;
}

int Label::get_line_height(int p_line) const {
	Ref<Font> font = (settings.is_valid() && settings->get_font().is_valid()) ? settings->get_font() : theme_cache.font;
	int font_size = settings.is_valid() ? settings->get_font_size() : theme_cache.font_size;
	int font_h = font->get_height(font_size);
	if (p_line >= 0 && p_line < total_line_count) {
		return MAX(font_h, TS->shaped_text_get_size(get_line_rid(p_line)).y);
	} else if (total_line_count > 0) {
		int h = font_h;
		for (const Paragraph &para : paragraphs) {
			for (const RID &line_rid : para.lines_rid) {
				h = MAX(h, TS->shaped_text_get_size(line_rid).y);
			}
		}
		return h;
	} else {
		return font->get_height(font_size);
	}
}

void Label::_shape() const {
	Ref<StyleBox> style = theme_cache.normal_style;
	int width = (get_size().width - style->get_minimum_size().width);

	if (text_dirty) {
		for (Paragraph &para : paragraphs) {
			for (const RID &line_rid : para.lines_rid) {
				TS->free_rid(line_rid);
			}
			para.lines_rid.clear();
			TS->free_rid(para.text_rid);
		}
		paragraphs.clear();

		String txt = (uppercase) ? TS->string_to_upper(xl_text, language) : xl_text;
		if (visible_chars >= 0 && visible_chars_behavior == TextServer::VC_CHARS_BEFORE_SHAPING) {
			txt = txt.substr(0, visible_chars);
		}
		String ps = paragraph_separator.c_unescape();
		Vector<String> para_text = txt.split(ps);
		int start = 0;
		for (const String &str : para_text) {
			Paragraph para;
			para.text_rid = TS->create_shaped_text();
			para.text = str;
			para.start = start;
			start += str.length() + ps.length();
			paragraphs.push_back(para);
		}
		text_dirty = false;
	}

	total_line_count = 0;
	for (Paragraph &para : paragraphs) {
		if (para.dirty || font_dirty) {
			if (para.dirty) {
				TS->shaped_text_clear(para.text_rid);
			}
			if (text_direction == Control::TEXT_DIRECTION_INHERITED) {
				TS->shaped_text_set_direction(para.text_rid, is_layout_rtl() ? TextServer::DIRECTION_RTL : TextServer::DIRECTION_LTR);
			} else {
				TS->shaped_text_set_direction(para.text_rid, (TextServer::Direction)text_direction);
			}
			const Ref<Font> &font = (settings.is_valid() && settings->get_font().is_valid()) ? settings->get_font() : theme_cache.font;
			int font_size = settings.is_valid() ? settings->get_font_size() : theme_cache.font_size;
			ERR_FAIL_COND(font.is_null());

			if (para.dirty) {
				TS->shaped_text_add_string(para.text_rid, para.text, font->get_rids(), font_size, font->get_opentype_features(), language);
			} else {
				int spans = TS->shaped_get_span_count(para.text_rid);
				for (int i = 0; i < spans; i++) {
					TS->shaped_set_span_update_font(para.text_rid, i, font->get_rids(), font_size, font->get_opentype_features());
				}
			}
			TS->shaped_text_set_bidi_override(para.text_rid, structured_text_parser(st_parser, st_args, para.text));
			if (!tab_stops.is_empty()) {
				TS->shaped_text_tab_align(para.text_rid, tab_stops);
			}
			para.dirty = false;
			para.lines_dirty = true;
		}

		if (para.lines_dirty) {
			for (const RID &line_rid : para.lines_rid) {
				TS->free_rid(line_rid);
			}
			para.lines_rid.clear();

			BitField<TextServer::LineBreakFlag> autowrap_flags = TextServer::BREAK_MANDATORY;
			switch (autowrap_mode) {
				case TextServer::AUTOWRAP_WORD_SMART:
					autowrap_flags = TextServer::BREAK_WORD_BOUND | TextServer::BREAK_ADAPTIVE | TextServer::BREAK_MANDATORY;
					break;
				case TextServer::AUTOWRAP_WORD:
					autowrap_flags = TextServer::BREAK_WORD_BOUND | TextServer::BREAK_MANDATORY;
					break;
				case TextServer::AUTOWRAP_ARBITRARY:
					autowrap_flags = TextServer::BREAK_GRAPHEME_BOUND | TextServer::BREAK_MANDATORY;
					break;
				case TextServer::AUTOWRAP_OFF:
					break;
			}
			autowrap_flags = autowrap_flags | TextServer::BREAK_TRIM_EDGE_SPACES;

			PackedInt32Array line_breaks = TS->shaped_text_get_line_breaks(para.text_rid, width, 0, autowrap_flags);
			for (int i = 0; i < line_breaks.size(); i = i + 2) {
				RID line = TS->shaped_text_substr(para.text_rid, line_breaks[i], line_breaks[i + 1] - line_breaks[i]);
				if (!tab_stops.is_empty()) {
					TS->shaped_text_tab_align(line, tab_stops);
				}
				para.lines_rid.push_back(line);
			}
		}
		total_line_count += para.lines_rid.size();
	}
	dirty = false;
	font_dirty = false;

	if (xl_text.length() == 0) {
		minsize = Size2(1, get_line_height());
		return;
	}

	int visible_lines = get_visible_line_count();
	bool lines_hidden = visible_lines > 0 && visible_lines < total_line_count;

	int line_index = 0;
	if (autowrap_mode == TextServer::AUTOWRAP_OFF) {
		minsize.width = 0.0f;
	}
	for (Paragraph &para : paragraphs) {
		if (autowrap_mode == TextServer::AUTOWRAP_OFF) {
			for (const RID &line_rid : para.lines_rid) {
				if (minsize.width < TS->shaped_text_get_size(line_rid).x) {
					minsize.width = TS->shaped_text_get_size(line_rid).x;
				}
			}
		}

		if (para.lines_dirty) {
			BitField<TextServer::TextOverrunFlag> overrun_flags = TextServer::OVERRUN_NO_TRIM;
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

			// Fill after min_size calculation.

			BitField<TextServer::JustificationFlag> line_jst_flags = jst_flags;
			if (!tab_stops.is_empty()) {
				line_jst_flags.set_flag(TextServer::JUSTIFICATION_AFTER_LAST_TAB);
			}
			if (autowrap_mode != TextServer::AUTOWRAP_OFF) {
				if (lines_hidden) {
					overrun_flags.set_flag(TextServer::OVERRUN_ENFORCE_ELLIPSIS);
				}
				if (horizontal_alignment == HORIZONTAL_ALIGNMENT_FILL) {
					int jst_to_line = para.lines_rid.size();
					if (para.lines_rid.size() == 1 && line_jst_flags.has_flag(TextServer::JUSTIFICATION_DO_NOT_SKIP_SINGLE_LINE)) {
						jst_to_line = para.lines_rid.size();
					} else {
						if (line_jst_flags.has_flag(TextServer::JUSTIFICATION_SKIP_LAST_LINE)) {
							jst_to_line = para.lines_rid.size() - 1;
						}
						if (line_jst_flags.has_flag(TextServer::JUSTIFICATION_SKIP_LAST_LINE_WITH_VISIBLE_CHARS)) {
							for (int i = para.lines_rid.size() - 1; i >= 0; i--) {
								if (TS->shaped_text_has_visible_chars(para.lines_rid[i])) {
									jst_to_line = i;
									break;
								}
							}
						}
					}
					for (int i = 0; i < para.lines_rid.size(); i++) {
						if (i < jst_to_line) {
							TS->shaped_text_fit_to_width(para.lines_rid[i], width, line_jst_flags);
						} else if (i == (visible_lines - line_index - 1)) {
							TS->shaped_text_set_custom_ellipsis(para.lines_rid[i], (el_char.length() > 0) ? el_char[0] : 0x2026);
							TS->shaped_text_overrun_trim_to_width(para.lines_rid[i], width, overrun_flags);
						}
					}
				} else if (lines_hidden && (visible_lines - line_index - 1 >= 0) && (visible_lines - line_index - 1) < para.lines_rid.size()) {
					TS->shaped_text_set_custom_ellipsis(para.lines_rid[visible_lines - line_index - 1], (el_char.length() > 0) ? el_char[0] : 0x2026);
					TS->shaped_text_overrun_trim_to_width(para.lines_rid[visible_lines - line_index - 1], width, overrun_flags);
				}
			} else {
				// Autowrap disabled.
				int jst_to_line = para.lines_rid.size();
				if (para.lines_rid.size() == 1 && line_jst_flags.has_flag(TextServer::JUSTIFICATION_DO_NOT_SKIP_SINGLE_LINE)) {
					jst_to_line = para.lines_rid.size();
				} else {
					if (line_jst_flags.has_flag(TextServer::JUSTIFICATION_SKIP_LAST_LINE)) {
						jst_to_line = para.lines_rid.size() - 1;
					}
					if (line_jst_flags.has_flag(TextServer::JUSTIFICATION_SKIP_LAST_LINE_WITH_VISIBLE_CHARS)) {
						for (int i = para.lines_rid.size() - 1; i >= 0; i--) {
							if (TS->shaped_text_has_visible_chars(para.lines_rid[i])) {
								jst_to_line = i;
								break;
							}
						}
					}
				}
				for (int i = 0; i < para.lines_rid.size(); i++) {
					if (i < jst_to_line && horizontal_alignment == HORIZONTAL_ALIGNMENT_FILL) {
						TS->shaped_text_fit_to_width(para.lines_rid[i], width, line_jst_flags);
						overrun_flags.set_flag(TextServer::OVERRUN_JUSTIFICATION_AWARE);
						TS->shaped_text_set_custom_ellipsis(para.lines_rid[i], (el_char.length() > 0) ? el_char[0] : 0x2026);
						TS->shaped_text_overrun_trim_to_width(para.lines_rid[i], width, overrun_flags);
						TS->shaped_text_fit_to_width(para.lines_rid[i], width, line_jst_flags | TextServer::JUSTIFICATION_CONSTRAIN_ELLIPSIS);
					} else {
						TS->shaped_text_set_custom_ellipsis(para.lines_rid[i], (el_char.length() > 0) ? el_char[0] : 0x2026);
						TS->shaped_text_overrun_trim_to_width(para.lines_rid[i], width, overrun_flags);
					}
				}
			}
			para.lines_dirty = false;
		}
		line_index += para.lines_rid.size();
	}

	_update_visible();

	if (autowrap_mode == TextServer::AUTOWRAP_OFF || !clip || overrun_behavior == TextServer::OVERRUN_NO_TRIMMING) {
		const_cast<Label *>(this)->update_minimum_size();
	}
}

void Label::_update_visible() const {
	int line_spacing = settings.is_valid() ? settings->get_line_spacing() : theme_cache.line_spacing;
	int paragraph_spacing = settings.is_valid() ? settings->get_paragraph_spacing() : theme_cache.paragraph_spacing;
	Ref<StyleBox> style = theme_cache.normal_style;
	Ref<Font> font = (settings.is_valid() && settings->get_font().is_valid()) ? settings->get_font() : theme_cache.font;
	int font_size = settings.is_valid() ? settings->get_font_size() : theme_cache.font_size;
	int font_h = font->get_height(font_size);
	int lines_visible = total_line_count;

	if (max_lines_visible >= 0 && lines_visible > max_lines_visible) {
		lines_visible = max_lines_visible;
	}

	minsize.height = 0;
	int last_line = MIN(total_line_count, lines_visible + lines_skipped);

	int line_index = 0;
	for (const Paragraph &para : paragraphs) {
		if (line_index + para.lines_rid.size() <= lines_skipped) {
			line_index += para.lines_rid.size();
		} else {
			int start = (line_index < lines_skipped) ? lines_skipped - line_index : 0;
			int end = (line_index + para.lines_rid.size() < last_line) ? para.lines_rid.size() : last_line - line_index;
			if (end <= 0) {
				break;
			}
			for (int i = start; i < end; i++) {
				minsize.height += MAX(font_h, TS->shaped_text_get_size(para.lines_rid[i]).y) + line_spacing;
			}
			minsize.height += paragraph_spacing;
			line_index += para.lines_rid.size();
		}
	}

	if (minsize.height > 0) {
		minsize.height -= (line_spacing + paragraph_spacing);
	}
}

inline void draw_glyph(const Glyph &p_gl, const RID &p_canvas, const Color &p_font_color, const Vector2 &p_ofs) {
	if (p_gl.font_rid != RID()) {
		TS->font_draw_glyph(p_gl.font_rid, p_canvas, p_gl.font_size, p_ofs + Vector2(p_gl.x_off, p_gl.y_off), p_gl.index, p_font_color);
	} else if (((p_gl.flags & TextServer::GRAPHEME_IS_VIRTUAL) != TextServer::GRAPHEME_IS_VIRTUAL) && ((p_gl.flags & TextServer::GRAPHEME_IS_EMBEDDED_OBJECT) != TextServer::GRAPHEME_IS_EMBEDDED_OBJECT)) {
		TS->draw_hex_code_box(p_canvas, p_gl.font_size, p_ofs + Vector2(p_gl.x_off, p_gl.y_off), p_gl.index, p_font_color);
	}
}

inline void draw_glyph_shadow(const Glyph &p_gl, const RID &p_canvas, const Color &p_font_shadow_color, int p_shadow_outline_size, const Vector2 &p_ofs, const Vector2 &shadow_ofs) {
	if (p_gl.font_rid != RID()) {
		if (p_font_shadow_color.a > 0) {
			TS->font_draw_glyph(p_gl.font_rid, p_canvas, p_gl.font_size, p_ofs + Vector2(p_gl.x_off, p_gl.y_off) + shadow_ofs, p_gl.index, p_font_shadow_color);
		}
		if (p_font_shadow_color.a > 0 && p_shadow_outline_size > 0) {
			TS->font_draw_glyph_outline(p_gl.font_rid, p_canvas, p_gl.font_size, p_shadow_outline_size, p_ofs + Vector2(p_gl.x_off, p_gl.y_off) + shadow_ofs, p_gl.index, p_font_shadow_color);
		}
	}
}

inline void draw_glyph_outline(const Glyph &p_gl, const RID &p_canvas, const Color &p_font_outline_color, int p_outline_size, const Vector2 &p_ofs) {
	if (p_gl.font_rid != RID()) {
		if (p_font_outline_color.a != 0.0 && p_outline_size > 0) {
			TS->font_draw_glyph_outline(p_gl.font_rid, p_canvas, p_gl.font_size, p_outline_size, p_ofs + Vector2(p_gl.x_off, p_gl.y_off), p_gl.index, p_font_outline_color);
		}
	}
}

void Label::_ensure_shaped() const {
	if (dirty || font_dirty || text_dirty) {
		_shape();
	} else {
		for (const Paragraph &para : paragraphs) {
			if (para.lines_dirty || para.dirty) {
				_shape();
				return;
			}
		}
	}
}

RID Label::get_line_rid(int p_line) const {
	if (p_line < 0 || p_line >= total_line_count) {
		return RID();
	}

	int line_index = 0;
	for (const Paragraph &para : paragraphs) {
		if (line_index + para.lines_rid.size() <= p_line) {
			line_index += para.lines_rid.size();
		} else {
			return para.lines_rid[p_line - line_index];
		}
	}

	return RID();
}

Rect2 Label::get_line_rect(int p_line) const {
	if (p_line < 0 || p_line >= total_line_count) {
		return Rect2();
	}

	int line_index = 0;
	for (int p = 0; p < paragraphs.size(); p++) {
		const Paragraph &para = paragraphs[p];
		if (line_index + para.lines_rid.size() <= p_line) {
			line_index += para.lines_rid.size();
		} else {
			return _get_line_rect(p, p_line - line_index);
		}
	}

	return Rect2();
}

Rect2 Label::_get_line_rect(int p_para, int p_line) const {
	// Returns a rect providing the line's horizontal offset and total size. To determine the vertical
	// offset, use r_offset and r_line_spacing from get_layout_data.
	bool rtl = TS->shaped_text_get_inferred_direction(paragraphs[p_para].text_rid) == TextServer::DIRECTION_RTL;
	bool rtl_layout = is_layout_rtl();
	Ref<StyleBox> style = theme_cache.normal_style;
	Ref<Font> font = (settings.is_valid() && settings->get_font().is_valid()) ? settings->get_font() : theme_cache.font;
	int font_size = settings.is_valid() ? settings->get_font_size() : theme_cache.font_size;
	int font_h = font->get_height(font_size);
	Size2 size = get_size();
	Size2 line_size = TS->shaped_text_get_size(paragraphs[p_para].lines_rid[p_line]);
	line_size.y = MAX(font_h, line_size.y);
	Vector2 offset;
	switch (horizontal_alignment) {
		case HORIZONTAL_ALIGNMENT_FILL:
			if (rtl && autowrap_mode != TextServer::AUTOWRAP_OFF) {
				offset.x = int(size.width - style->get_margin(SIDE_RIGHT) - line_size.width);
			} else {
				offset.x = style->get_offset().x;
			}
			break;
		case HORIZONTAL_ALIGNMENT_LEFT: {
			if (rtl_layout) {
				offset.x = int(size.width - style->get_margin(SIDE_RIGHT) - line_size.width);
			} else {
				offset.x = style->get_offset().x;
			}
		} break;
		case HORIZONTAL_ALIGNMENT_CENTER: {
			offset.x = int(size.width - line_size.width) / 2;
		} break;
		case HORIZONTAL_ALIGNMENT_RIGHT: {
			if (rtl_layout) {
				offset.x = style->get_offset().x;
			} else {
				offset.x = int(size.width - style->get_margin(SIDE_RIGHT) - line_size.width);
			}
		} break;
	}
	return Rect2(offset, line_size);
}

int Label::get_layout_data(Vector2 &r_offset, int &r_last_line, int &r_line_spacing) const {
	// Computes several common parameters involved in laying out and rendering text set to this label.
	// Only vertical margin is considered in r_offset: use get_line_rect to get the horizontal offset
	// for a given line of text.
	Size2 size = get_size();
	Ref<StyleBox> style = theme_cache.normal_style;
	Ref<Font> font = (settings.is_valid() && settings->get_font().is_valid()) ? settings->get_font() : theme_cache.font;
	int font_size = settings.is_valid() ? settings->get_font_size() : theme_cache.font_size;
	int font_h = font->get_height(font_size);
	int line_spacing = settings.is_valid() ? settings->get_line_spacing() : theme_cache.line_spacing;
	int paragraph_spacing = settings.is_valid() ? settings->get_paragraph_spacing() : theme_cache.paragraph_spacing;
	float total_h = 0.0;
	int lines_visible = 0;

	// Get number of lines to fit to the height.
	int line_index = 0;
	for (const Paragraph &para : paragraphs) {
		if (line_index + para.lines_rid.size() <= lines_skipped) {
			line_index += para.lines_rid.size();
		} else {
			int start = (line_index < lines_skipped) ? lines_skipped - line_index : 0;
			for (int i = start; i < para.lines_rid.size(); i++) {
				total_h += MAX(font_h, TS->shaped_text_get_size(para.lines_rid[i]).y) + line_spacing;
				if (total_h > (get_size().height - style->get_minimum_size().height + line_spacing)) {
					break;
				}
				lines_visible++;
			}
			total_h += paragraph_spacing;
			line_index += para.lines_rid.size();
		}
	}

	if (max_lines_visible >= 0 && lines_visible > max_lines_visible) {
		lines_visible = max_lines_visible;
	}

	r_last_line = MIN(total_line_count, lines_visible + lines_skipped);

	// Get real total height.
	int total_glyphs = 0;
	total_h = 0;
	line_index = 0;
	for (const Paragraph &para : paragraphs) {
		if (line_index + para.lines_rid.size() <= lines_skipped) {
			line_index += para.lines_rid.size();
		} else {
			int start = (line_index < lines_skipped) ? lines_skipped - line_index : 0;
			int end = (line_index + para.lines_rid.size() < r_last_line) ? para.lines_rid.size() : r_last_line - line_index;
			if (end <= 0) {
				break;
			}
			for (int i = start; i < end; i++) {
				total_h += MAX(font_h, TS->shaped_text_get_size(para.lines_rid[i]).y) + line_spacing;
				total_glyphs += TS->shaped_text_get_glyph_count(para.lines_rid[i]) + TS->shaped_text_get_ellipsis_glyph_count(para.lines_rid[i]);
			}
			total_h += paragraph_spacing;
			line_index += para.lines_rid.size();
		}
	}
	total_h += style->get_margin(SIDE_TOP) + style->get_margin(SIDE_BOTTOM);
	int vbegin = 0, vsep = 0;
	if (lines_visible > 0) {
		switch (vertical_alignment) {
			case VERTICAL_ALIGNMENT_TOP: {
				// Nothing.
			} break;
			case VERTICAL_ALIGNMENT_CENTER: {
				vbegin = (size.y - (total_h - line_spacing - paragraph_spacing)) / 2;
				vsep = 0;
			} break;
			case VERTICAL_ALIGNMENT_BOTTOM: {
				vbegin = size.y - (total_h - line_spacing - paragraph_spacing);
				vsep = 0;
			} break;
			case VERTICAL_ALIGNMENT_FILL: {
				vbegin = 0;
				if (lines_visible > 1) {
					vsep = (size.y - (total_h - line_spacing - paragraph_spacing)) / (lines_visible - 1);
				} else {
					vsep = 0;
				}
			} break;
		}
	}
	r_offset = { 0, style->get_offset().y + vbegin };
	r_line_spacing = line_spacing + vsep;

	return total_glyphs;
}

PackedStringArray Label::get_configuration_warnings() const {
	PackedStringArray warnings = Control::get_configuration_warnings();

	// FIXME: This is not ideal and the sizing model should be fixed,
	// but for now we have to warn about this impossible to resolve combination.
	// See GH-83546.
	if (is_inside_tree() && get_tree()->get_edited_scene_root() != this) {
		// If the Label happens to be the root node of the edited scene, we don't need
		// to check what its parent is. It's going to be some node from the editor tree
		// and it can be a container, but that makes no difference to the user.
		Container *parent_container = Object::cast_to<Container>(get_parent_control());
		if (parent_container && autowrap_mode != TextServer::AUTOWRAP_OFF && get_custom_minimum_size() == Size2()) {
			warnings.push_back(RTR("Labels with autowrapping enabled must have a custom minimum size configured to work correctly inside a container."));
		}
	}

	// Ensure that the font can render all of the required glyphs.
	Ref<Font> font;
	if (settings.is_valid()) {
		font = settings->get_font();
	}
	if (font.is_null()) {
		font = theme_cache.font;
	}

	if (font.is_valid()) {
		_ensure_shaped();

		for (const Paragraph &para : paragraphs) {
			const Glyph *glyph = TS->shaped_text_get_glyphs(para.text_rid);
			int64_t glyph_count = TS->shaped_text_get_glyph_count(para.text_rid);
			for (int64_t i = 0; i < glyph_count; i++) {
				if (glyph[i].font_rid == RID()) {
					warnings.push_back(RTR("The current font does not support rendering one or more characters used in this Label's text."));
					break;
				}
			}
		}
	}

	return warnings;
}

void Label::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_TRANSLATION_CHANGED: {
			String new_text = atr(text);
			if (new_text == xl_text) {
				return; // Nothing new.
			}
			xl_text = new_text;
			if (visible_ratio < 1) {
				visible_chars = get_total_character_count() * visible_ratio;
			}
			text_dirty = true;

			queue_redraw();
			update_configuration_warnings();
		} break;

		case NOTIFICATION_LAYOUT_DIRECTION_CHANGED: {
			queue_redraw();
		} break;

		case NOTIFICATION_DRAW: {
			if (clip) {
				RenderingServer::get_singleton()->canvas_item_set_clip(get_canvas_item(), true);
			}

			// When a shaped text is invalidated by an external source, we want to reshape it.
			for (Paragraph &para : paragraphs) {
				if (!TS->shaped_text_is_ready(para.text_rid)) {
					para.dirty = true;
				}

				for (const RID &line_rid : para.lines_rid) {
					if (!TS->shaped_text_is_ready(line_rid)) {
						para.lines_dirty = true;
						break;
					}
				}
			}

			_ensure_shaped();

			RID ci = get_canvas_item();

			bool has_settings = settings.is_valid();

			Size2 string_size;
			Ref<StyleBox> style = theme_cache.normal_style;
			Ref<Font> font = (settings.is_valid() && settings->get_font().is_valid()) ? settings->get_font() : theme_cache.font;
			int font_size = settings.is_valid() ? settings->get_font_size() : theme_cache.font_size;
			int font_h = font->get_height(font_size);
			Color font_color = has_settings ? settings->get_font_color() : theme_cache.font_color;
			Color font_shadow_color = has_settings ? settings->get_shadow_color() : theme_cache.font_shadow_color;
			Point2 shadow_ofs = has_settings ? settings->get_shadow_offset() : theme_cache.font_shadow_offset;
			int paragraph_spacing = has_settings ? settings->get_paragraph_spacing() : theme_cache.paragraph_spacing;
			Color font_outline_color = has_settings ? settings->get_outline_color() : theme_cache.font_outline_color;
			int outline_size = has_settings ? settings->get_outline_size() : theme_cache.font_outline_size;
			int shadow_outline_size = has_settings ? settings->get_shadow_size() : theme_cache.font_shadow_outline_size;
			bool rtl_layout = is_layout_rtl();

			style->draw(ci, Rect2(Point2(0, 0), get_size()));

			bool trim_chars = (visible_chars >= 0) && (visible_chars_behavior == TextServer::VC_CHARS_AFTER_SHAPING);
			bool trim_glyphs_ltr = (visible_chars >= 0) && ((visible_chars_behavior == TextServer::VC_GLYPHS_LTR) || ((visible_chars_behavior == TextServer::VC_GLYPHS_AUTO) && !rtl_layout));
			bool trim_glyphs_rtl = (visible_chars >= 0) && ((visible_chars_behavior == TextServer::VC_GLYPHS_RTL) || ((visible_chars_behavior == TextServer::VC_GLYPHS_AUTO) && rtl_layout));

			Vector2 ofs;
			int line_spacing;
			int last_line;
			int total_glyphs = get_layout_data(ofs, last_line, line_spacing);
			int processed_glyphs = 0;
			int visible_glyphs = total_glyphs * visible_ratio;

			int line_index = 0;
			for (int p = 0; p < paragraphs.size(); p++) {
				const Paragraph &para = paragraphs[p];
				if (line_index + para.lines_rid.size() <= lines_skipped) {
					line_index += para.lines_rid.size();
				} else {
					int start = (line_index < lines_skipped) ? lines_skipped - line_index : 0;
					int end = (line_index + para.lines_rid.size() < last_line) ? para.lines_rid.size() : last_line - line_index;
					if (end <= 0) {
						break;
					}
					bool rtl = (TS->shaped_text_get_inferred_direction(para.text_rid) == TextServer::DIRECTION_RTL);
					for (int i = start; i < end; i++) {
						RID line_rid = para.lines_rid[i];
						Vector2 line_offset = _get_line_rect(p, i).position;
						ofs.x = line_offset.x;

						int asc = TS->shaped_text_get_ascent(line_rid);
						int dsc = TS->shaped_text_get_descent(line_rid);
						if (asc + dsc < font_h) {
							dsc = font_h - asc;
						}

						const Glyph *glyphs = TS->shaped_text_get_glyphs(line_rid);
						int gl_size = TS->shaped_text_get_glyph_count(line_rid);

						int ellipsis_pos = TS->shaped_text_get_ellipsis_pos(line_rid);
						int trim_pos = TS->shaped_text_get_trim_pos(line_rid);

						const Glyph *ellipsis_glyphs = TS->shaped_text_get_ellipsis_glyphs(line_rid);
						int ellipsis_gl_size = TS->shaped_text_get_ellipsis_glyph_count(line_rid);

						ofs.y += asc;

						// Draw shadow, outline and text. Note: Do not merge this into the single loop iteration, to prevent overlaps.
						int processed_glyphs_step = 0;
						for (int step = DRAW_STEP_SHADOW; step < DRAW_STEP_MAX; step++) {
							if (step == DRAW_STEP_SHADOW && (font_shadow_color.a == 0)) {
								continue;
							}
							if (step == DRAW_STEP_OUTLINE && (outline_size <= 0 || font_outline_color.a == 0)) {
								continue;
							}

							processed_glyphs_step = processed_glyphs;
							Vector2 offset_step = ofs;
							// Draw RTL ellipsis string when necessary.
							if (rtl && ellipsis_pos >= 0) {
								for (int gl_idx = ellipsis_gl_size - 1; gl_idx >= 0; gl_idx--) {
									for (int j = 0; j < ellipsis_glyphs[gl_idx].repeat; j++) {
										bool skip = (trim_chars && ellipsis_glyphs[gl_idx].end + para.start > visible_chars) || (trim_glyphs_ltr && (processed_glyphs_step >= visible_glyphs)) || (trim_glyphs_rtl && (processed_glyphs_step < total_glyphs - visible_glyphs));
										if (!skip) {
											if (step == DRAW_STEP_SHADOW) {
												draw_glyph_shadow(ellipsis_glyphs[gl_idx], ci, font_shadow_color, shadow_outline_size, offset_step, shadow_ofs);
											} else if (step == DRAW_STEP_OUTLINE) {
												draw_glyph_outline(ellipsis_glyphs[gl_idx], ci, font_outline_color, outline_size, offset_step);
											} else if (step == DRAW_STEP_TEXT) {
												draw_glyph(ellipsis_glyphs[gl_idx], ci, font_color, offset_step);
											}
										}
										processed_glyphs_step++;
										offset_step.x += ellipsis_glyphs[gl_idx].advance;
									}
								}
							}
							// Draw main text.
							for (int j = 0; j < gl_size; j++) {
								// Trim when necessary.
								if (trim_pos >= 0) {
									if (rtl) {
										if (j < trim_pos) {
											continue;
										}
									} else {
										if (j >= trim_pos) {
											break;
										}
									}
								}
								for (int k = 0; k < glyphs[j].repeat; k++) {
									bool skip = (trim_chars && glyphs[j].end + para.start > visible_chars) || (trim_glyphs_ltr && (processed_glyphs_step >= visible_glyphs)) || (trim_glyphs_rtl && (processed_glyphs_step < total_glyphs - visible_glyphs));
									if (!skip) {
										if (step == DRAW_STEP_SHADOW) {
											draw_glyph_shadow(glyphs[j], ci, font_shadow_color, shadow_outline_size, offset_step, shadow_ofs);
										} else if (step == DRAW_STEP_OUTLINE) {
											draw_glyph_outline(glyphs[j], ci, font_outline_color, outline_size, offset_step);
										} else if (step == DRAW_STEP_TEXT) {
											draw_glyph(glyphs[j], ci, font_color, offset_step);
										}
									}
									processed_glyphs_step++;
									offset_step.x += glyphs[j].advance;
								}
							}
							// Draw LTR ellipsis string when necessary.
							if (!rtl && ellipsis_pos >= 0) {
								for (int gl_idx = 0; gl_idx < ellipsis_gl_size; gl_idx++) {
									for (int j = 0; j < ellipsis_glyphs[gl_idx].repeat; j++) {
										bool skip = (trim_chars && ellipsis_glyphs[gl_idx].end + para.start > visible_chars) || (trim_glyphs_ltr && (processed_glyphs_step >= visible_glyphs)) || (trim_glyphs_rtl && (processed_glyphs_step < total_glyphs - visible_glyphs));
										if (!skip) {
											if (step == DRAW_STEP_SHADOW) {
												draw_glyph_shadow(ellipsis_glyphs[gl_idx], ci, font_shadow_color, shadow_outline_size, offset_step, shadow_ofs);
											} else if (step == DRAW_STEP_OUTLINE) {
												draw_glyph_outline(ellipsis_glyphs[gl_idx], ci, font_outline_color, outline_size, offset_step);
											} else if (step == DRAW_STEP_TEXT) {
												draw_glyph(ellipsis_glyphs[gl_idx], ci, font_color, offset_step);
											}
										}
										processed_glyphs_step++;
										offset_step.x += ellipsis_glyphs[gl_idx].advance;
									}
								}
							}
						}
						processed_glyphs = processed_glyphs_step;
						ofs.y += dsc + line_spacing;
					}
					ofs.y += paragraph_spacing;
					line_index += para.lines_rid.size();
				}
			}
		} break;

		case NOTIFICATION_THEME_CHANGED: {
			font_dirty = true;
			queue_redraw();
		} break;

		case NOTIFICATION_RESIZED: {
			for (Paragraph &para : paragraphs) {
				para.lines_dirty = true;
			}
		} break;
	}
}

Rect2 Label::get_character_bounds(int p_pos) const {
	_ensure_shaped();

	int paragraph_spacing = settings.is_valid() ? settings->get_paragraph_spacing() : theme_cache.paragraph_spacing;
	Ref<Font> font = (settings.is_valid() && settings->get_font().is_valid()) ? settings->get_font() : theme_cache.font;
	int font_size = settings.is_valid() ? settings->get_font_size() : theme_cache.font_size;
	int font_h = font->get_height(font_size);

	Vector2 ofs;
	int line_spacing;
	int last_line;
	get_layout_data(ofs, last_line, line_spacing);

	int line_index = 0;
	for (int p = 0; p < paragraphs.size(); p++) {
		const Paragraph &para = paragraphs[p];
		if (line_index + para.lines_rid.size() <= lines_skipped) {
			line_index += para.lines_rid.size();
		} else {
			int start = (line_index < lines_skipped) ? lines_skipped - line_index : 0;
			int end = (line_index + para.lines_rid.size() < last_line) ? para.lines_rid.size() : last_line - line_index;
			if (end <= 0) {
				break;
			}
			for (int i = start; i < end; i++) {
				RID line_rid = para.lines_rid[i];
				Rect2 line_rect = _get_line_rect(p, i);
				ofs.x = line_rect.position.x;

				int v_size = TS->shaped_text_get_glyph_count(line_rid);
				const Glyph *glyphs = TS->shaped_text_get_glyphs(line_rid);

				float gl_off = 0.0f;
				for (int j = 0; j < v_size; j++) {
					if ((glyphs[j].count > 0) && ((glyphs[j].index != 0) || ((glyphs[j].flags & TextServer::GRAPHEME_IS_SPACE) == TextServer::GRAPHEME_IS_SPACE))) {
						if (p_pos >= glyphs[j].start + para.start && p_pos < glyphs[j].end + para.start) {
							float advance = 0.f;
							for (int k = 0; k < glyphs[j].count; k++) {
								advance += glyphs[j + k].advance;
							}
							Rect2 rect;
							rect.position = ofs + Vector2(gl_off, 0);
							rect.size = Vector2(advance, line_rect.size.y);
							return rect;
						}
					}
					gl_off += glyphs[j].advance * glyphs[j].repeat;
				}
				ofs.y += MAX(font_h, TS->shaped_text_get_ascent(line_rid) + TS->shaped_text_get_descent(line_rid)) + line_spacing;
			}
			ofs.y += paragraph_spacing;
			line_index += para.lines_rid.size();
		}
	}
	return Rect2();
}

Size2 Label::get_minimum_size() const {
	_ensure_shaped();

	Size2 min_size = minsize;

	const Ref<Font> &font = (settings.is_valid() && settings->get_font().is_valid()) ? settings->get_font() : theme_cache.font;
	int font_size = settings.is_valid() ? settings->get_font_size() : theme_cache.font_size;

	min_size.height = MAX(min_size.height, font->get_height(font_size) + font->get_spacing(TextServer::SPACING_TOP) + font->get_spacing(TextServer::SPACING_BOTTOM));

	Size2 min_style = theme_cache.normal_style->get_minimum_size();
	if (autowrap_mode != TextServer::AUTOWRAP_OFF) {
		return Size2(1, (clip || overrun_behavior != TextServer::OVERRUN_NO_TRIMMING) ? 1 : min_size.height) + min_style;
	} else {
		if (clip || overrun_behavior != TextServer::OVERRUN_NO_TRIMMING) {
			min_size.width = 1;
		}
		return min_size + min_style;
	}
}

#ifndef DISABLE_DEPRECATED
bool Label::_set(const StringName &p_name, const Variant &p_value) {
	if (p_name == SNAME("valign")) {
		set_vertical_alignment((VerticalAlignment)p_value.operator int());
		return true;
	} else if (p_name == SNAME("align")) {
		set_horizontal_alignment((HorizontalAlignment)p_value.operator int());
		return true;
	}
	return false;
}
#endif

int Label::get_line_count() const {
	if (!is_inside_tree()) {
		return 1;
	}
	_ensure_shaped();

	return total_line_count;
}

int Label::get_visible_line_count() const {
	Ref<StyleBox> style = theme_cache.normal_style;
	Ref<Font> font = (settings.is_valid() && settings->get_font().is_valid()) ? settings->get_font() : theme_cache.font;
	int font_size = settings.is_valid() ? settings->get_font_size() : theme_cache.font_size;
	int font_h = font->get_height(font_size);
	int line_spacing = settings.is_valid() ? settings->get_line_spacing() : theme_cache.line_spacing;
	int paragraph_spacing = settings.is_valid() ? settings->get_paragraph_spacing() : theme_cache.paragraph_spacing;
	int lines_visible = 0;
	float total_h = 0.0;

	int line_index = 0;
	for (const Paragraph &para : paragraphs) {
		if (line_index + para.lines_rid.size() <= lines_skipped) {
			line_index += para.lines_rid.size();
		} else {
			int start = (line_index < lines_skipped) ? lines_skipped - line_index : 0;
			for (int i = start; i < para.lines_rid.size(); i++) {
				total_h += MAX(font_h, TS->shaped_text_get_size(para.lines_rid[i]).y) + line_spacing;
				if (total_h > (get_size().height - style->get_minimum_size().height + line_spacing)) {
					break;
				}
				lines_visible++;
			}
			total_h += paragraph_spacing;
			line_index += para.lines_rid.size();
		}
	}

	if (max_lines_visible >= 0 && lines_visible > max_lines_visible) {
		lines_visible = max_lines_visible;
	}

	return lines_visible;
}

void Label::set_horizontal_alignment(HorizontalAlignment p_alignment) {
	ERR_FAIL_INDEX((int)p_alignment, 4);
	if (horizontal_alignment == p_alignment) {
		return;
	}

	if (horizontal_alignment == HORIZONTAL_ALIGNMENT_FILL || p_alignment == HORIZONTAL_ALIGNMENT_FILL) {
		for (Paragraph &para : paragraphs) {
			para.lines_dirty = true; // Reshape lines.
		}
	}
	horizontal_alignment = p_alignment;

	queue_redraw();
}

HorizontalAlignment Label::get_horizontal_alignment() const {
	return horizontal_alignment;
}

void Label::set_vertical_alignment(VerticalAlignment p_alignment) {
	ERR_FAIL_INDEX((int)p_alignment, 4);

	if (vertical_alignment == p_alignment) {
		return;
	}

	vertical_alignment = p_alignment;
	queue_redraw();
}

VerticalAlignment Label::get_vertical_alignment() const {
	return vertical_alignment;
}

void Label::set_text(const String &p_string) {
	if (text == p_string) {
		return;
	}
	text = p_string;
	xl_text = atr(p_string);
	text_dirty = true;
	if (visible_ratio < 1) {
		visible_chars = get_total_character_count() * visible_ratio;
	}
	queue_redraw();
	update_minimum_size();
	update_configuration_warnings();
}

void Label::_invalidate() {
	font_dirty = true;
	queue_redraw();
}

void Label::set_label_settings(const Ref<LabelSettings> &p_settings) {
	if (settings != p_settings) {
		if (settings.is_valid()) {
			settings->disconnect_changed(callable_mp(this, &Label::_invalidate));
		}
		settings = p_settings;
		if (settings.is_valid()) {
			settings->connect_changed(callable_mp(this, &Label::_invalidate), CONNECT_REFERENCE_COUNTED);
		}
		_invalidate();
	}
}

Ref<LabelSettings> Label::get_label_settings() const {
	return settings;
}

void Label::set_text_direction(Control::TextDirection p_text_direction) {
	ERR_FAIL_COND((int)p_text_direction < -1 || (int)p_text_direction > 3);
	if (text_direction != p_text_direction) {
		text_direction = p_text_direction;
		for (Paragraph &para : paragraphs) {
			para.dirty = true;
		}
		queue_redraw();
	}
}

void Label::set_structured_text_bidi_override(TextServer::StructuredTextParser p_parser) {
	if (st_parser != p_parser) {
		st_parser = p_parser;
		for (Paragraph &para : paragraphs) {
			para.dirty = true;
		}
		queue_redraw();
	}
}

TextServer::StructuredTextParser Label::get_structured_text_bidi_override() const {
	return st_parser;
}

void Label::set_structured_text_bidi_override_options(Array p_args) {
	if (st_args == p_args) {
		return;
	}

	st_args = p_args;
	for (Paragraph &para : paragraphs) {
		para.dirty = true;
	}
	queue_redraw();
}

Array Label::get_structured_text_bidi_override_options() const {
	return st_args;
}

Control::TextDirection Label::get_text_direction() const {
	return text_direction;
}

void Label::set_language(const String &p_language) {
	if (language != p_language) {
		language = p_language;
		for (Paragraph &para : paragraphs) {
			para.dirty = true;
		}
		queue_redraw();
	}
}

String Label::get_language() const {
	return language;
}

void Label::set_paragraph_separator(const String &p_paragraph_separator) {
	if (paragraph_separator != p_paragraph_separator) {
		paragraph_separator = p_paragraph_separator;
		text_dirty = true;
		queue_redraw();
	}
}

String Label::get_paragraph_separator() const {
	return paragraph_separator;
}

void Label::set_clip_text(bool p_clip) {
	if (clip == p_clip) {
		return;
	}

	clip = p_clip;
	queue_redraw();
	update_minimum_size();
}

bool Label::is_clipping_text() const {
	return clip;
}

void Label::set_tab_stops(const PackedFloat32Array &p_tab_stops) {
	if (tab_stops != p_tab_stops) {
		tab_stops = p_tab_stops;
		for (Paragraph &para : paragraphs) {
			para.dirty = true;
		}
		queue_redraw();
	}
}

PackedFloat32Array Label::get_tab_stops() const {
	return tab_stops;
}

void Label::set_text_overrun_behavior(TextServer::OverrunBehavior p_behavior) {
	if (overrun_behavior == p_behavior) {
		return;
	}

	overrun_behavior = p_behavior;
	for (Paragraph &para : paragraphs) {
		para.lines_dirty = true;
	}
	queue_redraw();
	if (clip || overrun_behavior != TextServer::OVERRUN_NO_TRIMMING) {
		update_minimum_size();
	}
}

TextServer::OverrunBehavior Label::get_text_overrun_behavior() const {
	return overrun_behavior;
}

void Label::set_ellipsis_char(const String &p_char) {
	String c = p_char;
	if (c.length() > 1) {
		WARN_PRINT("Ellipsis must be exactly one character long (" + itos(c.length()) + " characters given).");
		c = c.left(1);
	}
	if (el_char == c) {
		return;
	}
	el_char = c;
	for (Paragraph &para : paragraphs) {
		para.lines_dirty = true;
	}
	queue_redraw();
	if (clip || overrun_behavior != TextServer::OVERRUN_NO_TRIMMING) {
		update_minimum_size();
	}
}

String Label::get_ellipsis_char() const {
	return el_char;
}

String Label::get_text() const {
	return text;
}

void Label::set_visible_characters(int p_amount) {
	if (visible_chars != p_amount) {
		visible_chars = p_amount;
		if (get_total_character_count() > 0) {
			visible_ratio = (float)p_amount / (float)get_total_character_count();
		} else {
			visible_ratio = 1.0;
		}
		if (visible_chars_behavior == TextServer::VC_CHARS_BEFORE_SHAPING) {
			text_dirty = true;
		}
		queue_redraw();
	}
}

int Label::get_visible_characters() const {
	return visible_chars;
}

void Label::set_visible_ratio(float p_ratio) {
	if (visible_ratio != p_ratio) {
		if (p_ratio >= 1.0) {
			visible_chars = -1;
			visible_ratio = 1.0;
		} else if (p_ratio < 0.0) {
			visible_chars = 0;
			visible_ratio = 0.0;
		} else {
			visible_chars = get_total_character_count() * p_ratio;
			visible_ratio = p_ratio;
		}

		if (visible_chars_behavior == TextServer::VC_CHARS_BEFORE_SHAPING) {
			text_dirty = true;
		}
		queue_redraw();
	}
}

float Label::get_visible_ratio() const {
	return visible_ratio;
}

TextServer::VisibleCharactersBehavior Label::get_visible_characters_behavior() const {
	return visible_chars_behavior;
}

void Label::set_visible_characters_behavior(TextServer::VisibleCharactersBehavior p_behavior) {
	if (visible_chars_behavior != p_behavior) {
		if (visible_chars_behavior == TextServer::VC_CHARS_BEFORE_SHAPING || p_behavior == TextServer::VC_CHARS_BEFORE_SHAPING) {
			text_dirty = true;
		}
		visible_chars_behavior = p_behavior;
		queue_redraw();
	}
}

void Label::set_lines_skipped(int p_lines) {
	ERR_FAIL_COND(p_lines < 0);

	if (lines_skipped == p_lines) {
		return;
	}

	lines_skipped = p_lines;
	_update_visible();
	queue_redraw();
}

int Label::get_lines_skipped() const {
	return lines_skipped;
}

void Label::set_max_lines_visible(int p_lines) {
	if (max_lines_visible == p_lines) {
		return;
	}

	max_lines_visible = p_lines;
	_update_visible();
	queue_redraw();
}

int Label::get_max_lines_visible() const {
	return max_lines_visible;
}

int Label::get_total_character_count() const {
	return xl_text.length();
}

void Label::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_horizontal_alignment", "alignment"), &Label::set_horizontal_alignment);
	ClassDB::bind_method(D_METHOD("get_horizontal_alignment"), &Label::get_horizontal_alignment);
	ClassDB::bind_method(D_METHOD("set_vertical_alignment", "alignment"), &Label::set_vertical_alignment);
	ClassDB::bind_method(D_METHOD("get_vertical_alignment"), &Label::get_vertical_alignment);
	ClassDB::bind_method(D_METHOD("set_text", "text"), &Label::set_text);
	ClassDB::bind_method(D_METHOD("get_text"), &Label::get_text);
	ClassDB::bind_method(D_METHOD("set_label_settings", "settings"), &Label::set_label_settings);
	ClassDB::bind_method(D_METHOD("get_label_settings"), &Label::get_label_settings);
	ClassDB::bind_method(D_METHOD("set_text_direction", "direction"), &Label::set_text_direction);
	ClassDB::bind_method(D_METHOD("get_text_direction"), &Label::get_text_direction);
	ClassDB::bind_method(D_METHOD("set_language", "language"), &Label::set_language);
	ClassDB::bind_method(D_METHOD("get_language"), &Label::get_language);
	ClassDB::bind_method(D_METHOD("set_paragraph_separator", "paragraph_separator"), &Label::set_paragraph_separator);
	ClassDB::bind_method(D_METHOD("get_paragraph_separator"), &Label::get_paragraph_separator);
	ClassDB::bind_method(D_METHOD("set_autowrap_mode", "autowrap_mode"), &Label::set_autowrap_mode);
	ClassDB::bind_method(D_METHOD("get_autowrap_mode"), &Label::get_autowrap_mode);
	ClassDB::bind_method(D_METHOD("set_justification_flags", "justification_flags"), &Label::set_justification_flags);
	ClassDB::bind_method(D_METHOD("get_justification_flags"), &Label::get_justification_flags);
	ClassDB::bind_method(D_METHOD("set_clip_text", "enable"), &Label::set_clip_text);
	ClassDB::bind_method(D_METHOD("is_clipping_text"), &Label::is_clipping_text);
	ClassDB::bind_method(D_METHOD("set_tab_stops", "tab_stops"), &Label::set_tab_stops);
	ClassDB::bind_method(D_METHOD("get_tab_stops"), &Label::get_tab_stops);
	ClassDB::bind_method(D_METHOD("set_text_overrun_behavior", "overrun_behavior"), &Label::set_text_overrun_behavior);
	ClassDB::bind_method(D_METHOD("get_text_overrun_behavior"), &Label::get_text_overrun_behavior);
	ClassDB::bind_method(D_METHOD("set_ellipsis_char", "char"), &Label::set_ellipsis_char);
	ClassDB::bind_method(D_METHOD("get_ellipsis_char"), &Label::get_ellipsis_char);
	ClassDB::bind_method(D_METHOD("set_uppercase", "enable"), &Label::set_uppercase);
	ClassDB::bind_method(D_METHOD("is_uppercase"), &Label::is_uppercase);
	ClassDB::bind_method(D_METHOD("get_line_height", "line"), &Label::get_line_height, DEFVAL(-1));
	ClassDB::bind_method(D_METHOD("get_line_count"), &Label::get_line_count);
	ClassDB::bind_method(D_METHOD("get_visible_line_count"), &Label::get_visible_line_count);
	ClassDB::bind_method(D_METHOD("get_total_character_count"), &Label::get_total_character_count);
	ClassDB::bind_method(D_METHOD("set_visible_characters", "amount"), &Label::set_visible_characters);
	ClassDB::bind_method(D_METHOD("get_visible_characters"), &Label::get_visible_characters);
	ClassDB::bind_method(D_METHOD("get_visible_characters_behavior"), &Label::get_visible_characters_behavior);
	ClassDB::bind_method(D_METHOD("set_visible_characters_behavior", "behavior"), &Label::set_visible_characters_behavior);
	ClassDB::bind_method(D_METHOD("set_visible_ratio", "ratio"), &Label::set_visible_ratio);
	ClassDB::bind_method(D_METHOD("get_visible_ratio"), &Label::get_visible_ratio);
	ClassDB::bind_method(D_METHOD("set_lines_skipped", "lines_skipped"), &Label::set_lines_skipped);
	ClassDB::bind_method(D_METHOD("get_lines_skipped"), &Label::get_lines_skipped);
	ClassDB::bind_method(D_METHOD("set_max_lines_visible", "lines_visible"), &Label::set_max_lines_visible);
	ClassDB::bind_method(D_METHOD("get_max_lines_visible"), &Label::get_max_lines_visible);
	ClassDB::bind_method(D_METHOD("set_structured_text_bidi_override", "parser"), &Label::set_structured_text_bidi_override);
	ClassDB::bind_method(D_METHOD("get_structured_text_bidi_override"), &Label::get_structured_text_bidi_override);
	ClassDB::bind_method(D_METHOD("set_structured_text_bidi_override_options", "args"), &Label::set_structured_text_bidi_override_options);
	ClassDB::bind_method(D_METHOD("get_structured_text_bidi_override_options"), &Label::get_structured_text_bidi_override_options);

	ClassDB::bind_method(D_METHOD("get_character_bounds", "pos"), &Label::get_character_bounds);

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "text", PROPERTY_HINT_MULTILINE_TEXT), "set_text", "get_text");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "label_settings", PROPERTY_HINT_RESOURCE_TYPE, "LabelSettings"), "set_label_settings", "get_label_settings");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "horizontal_alignment", PROPERTY_HINT_ENUM, "Left,Center,Right,Fill"), "set_horizontal_alignment", "get_horizontal_alignment");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "vertical_alignment", PROPERTY_HINT_ENUM, "Top,Center,Bottom,Fill"), "set_vertical_alignment", "get_vertical_alignment");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "autowrap_mode", PROPERTY_HINT_ENUM, "Off,Arbitrary,Word,Word (Smart)"), "set_autowrap_mode", "get_autowrap_mode");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "justification_flags", PROPERTY_HINT_FLAGS, "Kashida Justification:1,Word Justification:2,Justify Only After Last Tab:8,Skip Last Line:32,Skip Last Line With Visible Characters:64,Do Not Skip Single Line:128"), "set_justification_flags", "get_justification_flags");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "paragraph_separator"), "set_paragraph_separator", "get_paragraph_separator");

	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "clip_text"), "set_clip_text", "is_clipping_text");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "text_overrun_behavior", PROPERTY_HINT_ENUM, "Trim Nothing,Trim Characters,Trim Words,Ellipsis,Word Ellipsis"), "set_text_overrun_behavior", "get_text_overrun_behavior");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "ellipsis_char"), "set_ellipsis_char", "get_ellipsis_char");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "uppercase"), "set_uppercase", "is_uppercase");
	ADD_PROPERTY(PropertyInfo(Variant::PACKED_FLOAT32_ARRAY, "tab_stops"), "set_tab_stops", "get_tab_stops");

	ADD_GROUP("Displayed Text", "");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "lines_skipped", PROPERTY_HINT_RANGE, "0,999,1"), "set_lines_skipped", "get_lines_skipped");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "max_lines_visible", PROPERTY_HINT_RANGE, "-1,999,1"), "set_max_lines_visible", "get_max_lines_visible");
	// Note: "visible_characters" and "visible_ratio" should be set after "text" to be correctly applied.
	ADD_PROPERTY(PropertyInfo(Variant::INT, "visible_characters", PROPERTY_HINT_RANGE, "-1,128000,1"), "set_visible_characters", "get_visible_characters");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "visible_characters_behavior", PROPERTY_HINT_ENUM, "Characters Before Shaping,Characters After Shaping,Glyphs (Layout Direction),Glyphs (Left-to-Right),Glyphs (Right-to-Left)"), "set_visible_characters_behavior", "get_visible_characters_behavior");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "visible_ratio", PROPERTY_HINT_RANGE, "0,1,0.001"), "set_visible_ratio", "get_visible_ratio");

	ADD_GROUP("BiDi", "");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "text_direction", PROPERTY_HINT_ENUM, "Auto,Left-to-Right,Right-to-Left,Inherited"), "set_text_direction", "get_text_direction");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "language", PROPERTY_HINT_LOCALE_ID, ""), "set_language", "get_language");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "structured_text_bidi_override", PROPERTY_HINT_ENUM, "Default,URI,File,Email,List,None,Custom"), "set_structured_text_bidi_override", "get_structured_text_bidi_override");
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "structured_text_bidi_override_options"), "set_structured_text_bidi_override_options", "get_structured_text_bidi_override_options");

	BIND_THEME_ITEM_CUSTOM(Theme::DATA_TYPE_STYLEBOX, Label, normal_style, "normal");
	BIND_THEME_ITEM(Theme::DATA_TYPE_CONSTANT, Label, line_spacing);
	BIND_THEME_ITEM(Theme::DATA_TYPE_CONSTANT, Label, paragraph_spacing);

	BIND_THEME_ITEM(Theme::DATA_TYPE_FONT, Label, font);
	BIND_THEME_ITEM(Theme::DATA_TYPE_FONT_SIZE, Label, font_size);
	BIND_THEME_ITEM(Theme::DATA_TYPE_COLOR, Label, font_color);
	BIND_THEME_ITEM(Theme::DATA_TYPE_COLOR, Label, font_shadow_color);
	BIND_THEME_ITEM_CUSTOM(Theme::DATA_TYPE_CONSTANT, Label, font_shadow_offset.x, "shadow_offset_x");
	BIND_THEME_ITEM_CUSTOM(Theme::DATA_TYPE_CONSTANT, Label, font_shadow_offset.y, "shadow_offset_y");
	BIND_THEME_ITEM(Theme::DATA_TYPE_COLOR, Label, font_outline_color);
	BIND_THEME_ITEM_CUSTOM(Theme::DATA_TYPE_CONSTANT, Label, font_outline_size, "outline_size");
	BIND_THEME_ITEM_CUSTOM(Theme::DATA_TYPE_CONSTANT, Label, font_shadow_outline_size, "shadow_outline_size");
}

Label::Label(const String &p_text) {
	set_mouse_filter(MOUSE_FILTER_IGNORE);
	set_text(p_text);
	set_v_size_flags(SIZE_SHRINK_CENTER);
}

Label::~Label() {
	for (Paragraph &para : paragraphs) {
		for (const RID &line_rid : para.lines_rid) {
			TS->free_rid(line_rid);
		}
		para.lines_rid.clear();
		TS->free_rid(para.text_rid);
	}
	paragraphs.clear();
}

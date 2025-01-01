/**************************************************************************/
/*  text_edit.cpp                                                         */
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

#include "text_edit.h"
#include "text_edit.compat.inc"

#include "core/config/project_settings.h"
#include "core/input/input.h"
#include "core/input/input_map.h"
#include "core/object/script_language.h"
#include "core/os/keyboard.h"
#include "core/os/os.h"
#include "core/string/string_builder.h"
#include "core/string/translation_server.h"
#include "scene/gui/label.h"
#include "scene/main/window.h"
#include "scene/theme/theme_db.h"

///////////////////////////////////////////////////////////////////////////////
///                            TEXT                                         ///
///////////////////////////////////////////////////////////////////////////////

void TextEdit::Text::set_font(const Ref<Font> &p_font) {
	if (font == p_font) {
		return;
	}
	font = p_font;
	is_dirty = true;
}

void TextEdit::Text::set_font_size(int p_font_size) {
	if (font_size == p_font_size) {
		return;
	}
	font_size = p_font_size;
	is_dirty = true;
}

void TextEdit::Text::set_tab_size(int p_tab_size) {
	if (tab_size == p_tab_size) {
		return;
	}
	tab_size = p_tab_size;
	tab_size_dirty = true;
}

int TextEdit::Text::get_tab_size() const {
	return tab_size;
}

void TextEdit::Text::set_indent_wrapped_lines(bool p_enabled) {
	if (indent_wrapped_lines == p_enabled) {
		return;
	}
	indent_wrapped_lines = p_enabled;
	tab_size_dirty = true;
}

bool TextEdit::Text::is_indent_wrapped_lines() const {
	return indent_wrapped_lines;
}

void TextEdit::Text::set_direction_and_language(TextServer::Direction p_direction, const String &p_language) {
	if (direction == p_direction && language == p_language) {
		return;
	}
	direction = p_direction;
	language = p_language;
	is_dirty = true;
}

void TextEdit::Text::set_draw_control_chars(bool p_enabled) {
	if (draw_control_chars == p_enabled) {
		return;
	}
	draw_control_chars = p_enabled;
	is_dirty = true;
}

int TextEdit::Text::get_line_width(int p_line, int p_wrap_index) const {
	ERR_FAIL_INDEX_V(p_line, text.size(), 0);
	if (p_wrap_index != -1) {
		return text[p_line].data_buf->get_line_width(p_wrap_index);
	}
	return text[p_line].data_buf->get_size().x;
}

int TextEdit::Text::get_max_width() const {
	if (max_line_width_dirty) {
		int new_max_line_width = 0;
		for (const Line &l : text) {
			if (l.hidden) {
				continue;
			}
			new_max_line_width = MAX(new_max_line_width, l.width);
		}
		max_line_width = new_max_line_width;
	}

	return max_line_width;
}

int TextEdit::Text::get_line_height() const {
	if (max_line_height_dirty) {
		int new_max_line_height = 0;
		for (const Line &l : text) {
			if (l.hidden) {
				continue;
			}
			new_max_line_height = MAX(new_max_line_height, l.height);
		}
		max_line_height = new_max_line_height;
	}

	return max_line_height;
}

void TextEdit::Text::set_width(float p_width) {
	width = p_width;
}

float TextEdit::Text::get_width() const {
	return width;
}

void TextEdit::Text::set_brk_flags(BitField<TextServer::LineBreakFlag> p_flags) {
	brk_flags = p_flags;
}

BitField<TextServer::LineBreakFlag> TextEdit::Text::get_brk_flags() const {
	return brk_flags;
}

int TextEdit::Text::get_line_wrap_amount(int p_line) const {
	ERR_FAIL_INDEX_V(p_line, text.size(), 0);

	return text[p_line].line_count - 1;
}

Vector<Vector2i> TextEdit::Text::get_line_wrap_ranges(int p_line) const {
	Vector<Vector2i> ret;
	ERR_FAIL_INDEX_V(p_line, text.size(), ret);

	Ref<TextParagraph> data_buf = text[p_line].data_buf;
	int line_count = data_buf->get_line_count();
	for (int i = 0; i < line_count; i++) {
		ret.push_back(data_buf->get_line_range(i));
	}
	return ret;
}

const Ref<TextParagraph> TextEdit::Text::get_line_data(int p_line) const {
	ERR_FAIL_INDEX_V(p_line, text.size(), Ref<TextParagraph>());
	return text[p_line].data_buf;
}

_FORCE_INLINE_ String TextEdit::Text::operator[](int p_line) const {
	ERR_FAIL_INDEX_V(p_line, text.size(), "");
	return text[p_line].data;
}

void TextEdit::Text::invalidate_cache(int p_line, int p_column, bool p_text_changed, const String &p_ime_text, const Array &p_bidi_override) {
	ERR_FAIL_INDEX(p_line, text.size());

	if (font.is_null()) {
		return; // Not in tree?
	}

	Line &text_line = text.write[p_line];
	if (p_text_changed) {
		text_line.data_buf->clear();
	}

	BitField<TextServer::LineBreakFlag> flags = brk_flags;
	if (indent_wrapped_lines) {
		flags.set_flag(TextServer::BREAK_TRIM_INDENT);
	}

	text_line.data_buf->set_width(width);
	text_line.data_buf->set_direction((TextServer::Direction)direction);
	text_line.data_buf->set_break_flags(flags);
	text_line.data_buf->set_preserve_control(draw_control_chars);
	text_line.data_buf->set_custom_punctuation(get_enabled_word_separators());

	if (p_ime_text.length() > 0) {
		if (p_text_changed) {
			text_line.data_buf->add_string(p_ime_text, font, font_size, language);
		}
		if (!p_bidi_override.is_empty()) {
			TS->shaped_text_set_bidi_override(text_line.data_buf->get_rid(), p_bidi_override);
		}
	} else {
		if (p_text_changed) {
			text_line.data_buf->add_string(text_line.data, font, font_size, language);
		}
		if (!text_line.bidi_override.is_empty()) {
			TS->shaped_text_set_bidi_override(text_line.data_buf->get_rid(), text_line.bidi_override);
		}
	}

	if (!p_text_changed) {
		RID r = text_line.data_buf->get_rid();
		int spans = TS->shaped_get_span_count(r);
		for (int i = 0; i < spans; i++) {
			TS->shaped_set_span_update_font(r, i, font->get_rids(), font_size, font->get_opentype_features());
		}
	}

	// Apply tab align.
	if (tab_size > 0) {
		Vector<float> tabs;
		tabs.push_back(font->get_char_size(' ', font_size).width * tab_size);
		text_line.data_buf->tab_align(tabs);
	}

	// Update wrap amount.
	const int old_line_count = text_line.line_count;
	text_line.line_count = text_line.data_buf->get_line_count();
	if (!text_line.hidden && text_line.line_count != old_line_count) {
		total_visible_line_count += text_line.line_count - old_line_count;
	}

	// Update height.
	const int old_height = text_line.height;
	text_line.height = font_height;
	for (int i = 0; i < text_line.line_count; i++) {
		text_line.height = MAX(text_line.height, text_line.data_buf->get_line_size(i).y);
	}

	// If this line has shrunk, this may no longer be the tallest line.
	if (!text_line.hidden) {
		if (old_height == max_line_height && text_line.height < old_height) {
			max_line_height_dirty = true;
		} else {
			max_line_height = MAX(text_line.height, max_line_height);
		}
	}

	// Update width.
	const int old_width = text_line.width;
	text_line.width = get_line_width(p_line);

	if (!text_line.hidden) {
		// If this line has shrunk, this may no longer be the longest line.
		if (old_width == max_line_width && text_line.width < old_width) {
			max_line_width_dirty = true;
		} else {
			max_line_width = MAX(text_line.width, max_line_width);
		}
	}
}

void TextEdit::Text::invalidate_all_lines() {
	for (int i = 0; i < text.size(); i++) {
		if (tab_size_dirty) {
			if (tab_size > 0) {
				Vector<float> tabs;
				tabs.push_back(font->get_char_size(' ', font_size).width * tab_size);
				text[i].data_buf->tab_align(tabs);
			}
		}
		invalidate_cache(i, -1, false);
	}
	tab_size_dirty = false;
}

void TextEdit::Text::invalidate_font() {
	if (!is_dirty) {
		return;
	}

	max_line_width_dirty = true;
	max_line_height_dirty = true;

	if (font.is_valid() && font_size > 0) {
		font_height = font->get_height(font_size);
	}

	for (int i = 0; i < text.size(); i++) {
		invalidate_cache(i, -1, false);
	}
	is_dirty = false;
}

void TextEdit::Text::invalidate_all() {
	if (!is_dirty) {
		return;
	}

	max_line_width_dirty = true;
	max_line_height_dirty = true;

	if (font.is_valid() && font_size > 0) {
		font_height = font->get_height(font_size);
	}

	for (int i = 0; i < text.size(); i++) {
		invalidate_cache(i, -1, true);
	}
	is_dirty = false;
}

void TextEdit::Text::clear() {
	text.clear();

	max_line_width_dirty = true;
	max_line_height_dirty = true;
	total_visible_line_count = 0;

	Line line;
	line.gutters.resize(gutter_count);
	line.data = "";
	text.insert(0, line);
	invalidate_cache(0, -1, true);
}

int TextEdit::Text::get_total_visible_line_count() const {
	return total_visible_line_count;
}

void TextEdit::Text::set(int p_line, const String &p_text, const Array &p_bidi_override) {
	ERR_FAIL_INDEX(p_line, text.size());

	text.write[p_line].data = p_text;
	text.write[p_line].bidi_override = p_bidi_override;
	invalidate_cache(p_line, -1, true);
}

void TextEdit::Text::set_hidden(int p_line, bool p_hidden) {
	ERR_FAIL_INDEX(p_line, text.size());

	Line &text_line = text.write[p_line];
	if (text_line.hidden == p_hidden) {
		return;
	}
	text_line.hidden = p_hidden;
	if (p_hidden) {
		total_visible_line_count -= text_line.line_count;
		if (text_line.width == max_line_width) {
			max_line_width_dirty = true;
		}
		if (text_line.height == max_line_height) {
			max_line_height_dirty = true;
		}
	} else {
		total_visible_line_count += text_line.line_count;
		max_line_width = MAX(text_line.width, max_line_width);
		max_line_height = MAX(text_line.height, max_line_height);
	}
}

bool TextEdit::Text::is_hidden(int p_line) const {
	ERR_FAIL_INDEX_V(p_line, text.size(), true);
	return text[p_line].hidden;
}

void TextEdit::Text::insert(int p_at, const Vector<String> &p_text, const Vector<Array> &p_bidi_override) {
	ERR_FAIL_INDEX(p_at, text.size() + 1);

	int new_line_count = p_text.size() - 1;
	if (new_line_count > 0) {
		text.resize(text.size() + new_line_count);
		for (int i = (text.size() - 1); i > p_at; i--) {
			if ((i - new_line_count) <= 0) {
				break;
			}
			text.write[i] = text[i - new_line_count];
		}
	}

	for (int i = 0; i < p_text.size(); i++) {
		if (i == 0) {
			set(p_at + i, p_text[i], p_bidi_override[i]);
			continue;
		}
		Line line;
		line.gutters.resize(gutter_count);
		line.data = p_text[i];
		line.bidi_override = p_bidi_override[i];
		text.write[p_at + i] = line;
		invalidate_cache(p_at + i, -1, true);
	}
}

void TextEdit::Text::remove_range(int p_from_line, int p_to_line) {
	p_from_line = MAX(p_from_line, 0);
	ERR_FAIL_INDEX(p_from_line, text.size());

	p_to_line = MIN(p_to_line, text.size());
	ERR_FAIL_COND(p_to_line < p_from_line);

	if (p_from_line == p_to_line) {
		return;
	}

	for (int i = p_from_line; i < p_to_line; i++) {
		const Line &text_line = text[i];
		if (text_line.hidden) {
			continue;
		}

		if (text_line.height == max_line_height) {
			max_line_height_dirty = true;
		}
		if (text_line.width == max_line_width) {
			max_line_width_dirty = true;
		}
		total_visible_line_count -= text_line.line_count;
	}

	int diff = (p_to_line - p_from_line);
	for (int i = p_to_line; i < text.size() - 1; i++) {
		text.write[(i - diff) + 1] = text[i + 1];
	}
	text.resize(text.size() - diff);

	ERR_FAIL_COND(total_visible_line_count < 0); // BUG
}

void TextEdit::Text::add_gutter(int p_at) {
	for (int i = 0; i < text.size(); i++) {
		if (p_at < 0 || p_at > gutter_count) {
			text.write[i].gutters.push_back(Gutter());
		} else {
			text.write[i].gutters.insert(p_at, Gutter());
		}
	}
	gutter_count++;
}

void TextEdit::Text::remove_gutter(int p_gutter) {
	ERR_FAIL_INDEX(p_gutter, text.size());

	for (int i = 0; i < text.size(); i++) {
		text.write[i].gutters.remove_at(p_gutter);
	}
	gutter_count--;
}

void TextEdit::Text::move_gutters(int p_from_line, int p_to_line) {
	ERR_FAIL_INDEX(p_from_line, text.size());
	ERR_FAIL_INDEX(p_to_line, text.size());

	text.write[p_to_line].gutters = text[p_from_line].gutters;
	text.write[p_from_line].gutters.clear();
	text.write[p_from_line].gutters.resize(gutter_count);
}

void TextEdit::Text::set_use_default_word_separators(bool p_enabled) {
	if (use_default_word_separators == p_enabled) {
		return;
	}
	use_default_word_separators = p_enabled;
	invalidate_all_lines();
}

void TextEdit::Text::set_use_custom_word_separators(bool p_enabled) {
	if (use_custom_word_separators == p_enabled) {
		return;
	}
	use_custom_word_separators = p_enabled;
	invalidate_all_lines();
}

bool TextEdit::Text::is_default_word_separators_enabled() const {
	return use_default_word_separators;
}

bool TextEdit::Text::is_custom_word_separators_enabled() const {
	return use_custom_word_separators;
}

String TextEdit::Text::get_custom_word_separators() const {
	return custom_word_separators;
}

String TextEdit::Text::get_default_word_separators() const {
	String concat_separators = "!\"#$%&'()*+,-./:;<=>?@[\\]^`{|}~";
	for (char32_t ch = 0x2000; ch <= 0x206F; ++ch) { // General punctuation block.
		concat_separators += ch;
	}
	for (char32_t ch = 0x3000; ch <= 0x303F; ++ch) { // CJK punctuation block.
		concat_separators += ch;
	}
	return concat_separators;
}

// Get default and/or custom word separators depending on the option enabled.
String TextEdit::Text::get_enabled_word_separators() const {
	String all_separators;
	if (use_default_word_separators) {
		all_separators += get_default_word_separators();
	}
	if (use_custom_word_separators) {
		all_separators += get_custom_word_separators();
	}
	return all_separators;
}

///////////////////////////////////////////////////////////////////////////////
///                            TEXT EDIT                                    ///
///////////////////////////////////////////////////////////////////////////////

void TextEdit::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_POSTINITIALIZE: {
			_update_caches();
		} break;

		case NOTIFICATION_ENTER_TREE: {
			_update_caches();
			if (caret_pos_dirty) {
				callable_mp(this, &TextEdit::_emit_caret_changed).call_deferred();
			}
			if (text_changed_dirty) {
				callable_mp(this, &TextEdit::_emit_text_changed).call_deferred();
			}
			_update_wrap_at_column(true);
		} break;

		case NOTIFICATION_RESIZED: {
			_update_scrollbars();
			_update_wrap_at_column();
		} break;

		case NOTIFICATION_VISIBILITY_CHANGED: {
			if (is_visible()) {
				callable_mp(this, &TextEdit::_update_scrollbars).call_deferred();
				callable_mp(this, &TextEdit::_update_wrap_at_column).call_deferred(false);
			}
		} break;

		case NOTIFICATION_LAYOUT_DIRECTION_CHANGED:
		case NOTIFICATION_TRANSLATION_CHANGED:
		case NOTIFICATION_THEME_CHANGED: {
			if (is_inside_tree()) {
				_update_caches();
				_update_wrap_at_column(true);
			}
		} break;

		case NOTIFICATION_WM_WINDOW_FOCUS_IN: {
			window_has_focus = true;
			draw_caret = true;
			queue_redraw();
		} break;

		case NOTIFICATION_WM_WINDOW_FOCUS_OUT: {
			window_has_focus = false;
			draw_caret = false;
			queue_redraw();
		} break;

		case NOTIFICATION_INTERNAL_PHYSICS_PROCESS: {
			if (scrolling && get_v_scroll() != target_v_scroll) {
				double target_y = target_v_scroll - get_v_scroll();
				double dist = abs(target_y);
				// To ensure minimap is responsive override the speed setting.
				double vel = ((target_y / dist) * ((minimap_clicked) ? 3000 : v_scroll_speed)) * get_physics_process_delta_time();

				// Prevent small velocities from blocking scrolling.
				if (Math::abs(vel) < v_scroll->get_step()) {
					vel = v_scroll->get_step() * SIGN(vel);
				}

				if (Math::abs(vel) >= dist) {
					set_v_scroll(target_v_scroll);
					scrolling = false;
					minimap_clicked = false;
					set_physics_process_internal(false);
				} else {
					set_v_scroll(get_v_scroll() + vel);
				}
			} else {
				scrolling = false;
				minimap_clicked = false;
				set_physics_process_internal(false);
			}
		} break;

		case NOTIFICATION_DRAW: {
			if (first_draw) {
				// Size may not be the final one, so attempts to ensure caret was visible may have failed.
				adjust_viewport_to_caret();
				first_draw = false;
			}

			/* Prevent the resource getting lost between the editor and game. */
			if (Engine::get_singleton()->is_editor_hint()) {
				if (syntax_highlighter.is_valid() && syntax_highlighter->get_text_edit() != this) {
					syntax_highlighter->set_text_edit(this);
				}
			}

			Size2 size = get_size();
			bool rtl = is_layout_rtl();
			if ((!has_focus() && !(menu && menu->has_focus())) || !window_has_focus) {
				draw_caret = false;
			}

			_update_scrollbars();

			RID ci = get_canvas_item();
			int xmargin_beg = theme_cache.style_normal->get_margin(SIDE_LEFT) + gutters_width + gutter_padding;

			int xmargin_end = size.width - theme_cache.style_normal->get_margin(SIDE_RIGHT);
			if (draw_minimap) {
				xmargin_end -= minimap_width;
			}
			// Let's do it easy for now.
			theme_cache.style_normal->draw(ci, Rect2(Point2(), size));
			if (!editable) {
				theme_cache.style_readonly->draw(ci, Rect2(Point2(), size));
				draw_caret = is_drawing_caret_when_editable_disabled();
			}
			if (has_focus()) {
				theme_cache.style_focus->draw(ci, Rect2(Point2(), size));
			}

			int visible_rows = get_visible_line_count() + 1;

			if (theme_cache.background_color.a > 0.01) {
				RenderingServer::get_singleton()->canvas_item_add_rect(ci, Rect2(Point2i(), get_size()), theme_cache.background_color);
			}

			Vector<BraceMatchingData> brace_matching;
			if (highlight_matching_braces_enabled) {
				brace_matching.resize(get_caret_count());

				for (int caret = 0; caret < get_caret_count(); caret++) {
					BraceMatchingData &brace_match = brace_matching.write[caret];

					if (get_caret_line(caret) < 0 || get_caret_line(caret) >= text.size() || get_caret_column(caret) < 0) {
						continue;
					}

					if (get_caret_column(caret) < text[get_caret_line(caret)].length()) {
						// Check for open.
						char32_t c = text[get_caret_line(caret)][get_caret_column(caret)];
						char32_t closec = 0;

						if (c == '[') {
							closec = ']';
						} else if (c == '{') {
							closec = '}';
						} else if (c == '(') {
							closec = ')';
						}

						if (closec != 0) {
							int stack = 1;

							for (int i = get_caret_line(caret); i < text.size(); i++) {
								int from = i == get_caret_line(caret) ? get_caret_column(caret) + 1 : 0;
								for (int j = from; j < text[i].length(); j++) {
									char32_t cc = text[i][j];
									// Ignore any brackets inside a string.
									if (cc == '"' || cc == '\'') {
										char32_t quotation = cc;
										do {
											j++;
											if (!(j < text[i].length())) {
												break;
											}
											cc = text[i][j];
											// Skip over escaped quotation marks inside strings.
											if (cc == '\\') {
												bool escaped = true;
												while (j + 1 < text[i].length() && text[i][j + 1] == '\\') {
													escaped = !escaped;
													j++;
												}
												if (escaped) {
													j++;
													continue;
												}
											}
										} while (cc != quotation);
									} else if (cc == c) {
										stack++;
									} else if (cc == closec) {
										stack--;
									}

									if (stack == 0) {
										brace_match.open_match_line = i;
										brace_match.open_match_column = j;
										brace_match.open_matching = true;

										break;
									}
								}
								if (brace_match.open_match_line != -1) {
									break;
								}
							}

							if (!brace_match.open_matching) {
								brace_match.open_mismatch = true;
							}
						}
					}

					if (get_caret_column(caret) > 0) {
						char32_t c = text[get_caret_line(caret)][get_caret_column(caret) - 1];
						char32_t closec = 0;

						if (c == ']') {
							closec = '[';
						} else if (c == '}') {
							closec = '{';
						} else if (c == ')') {
							closec = '(';
						}

						if (closec != 0) {
							int stack = 1;

							for (int i = get_caret_line(caret); i >= 0; i--) {
								int from = i == get_caret_line(caret) ? get_caret_column(caret) - 2 : text[i].length() - 1;
								for (int j = from; j >= 0; j--) {
									char32_t cc = text[i][j];
									// Ignore any brackets inside a string.
									if (cc == '"' || cc == '\'') {
										char32_t quotation = cc;
										do {
											j--;
											if (!(j >= 0)) {
												break;
											}
											cc = text[i][j];
											// Skip over escaped quotation marks inside strings.
											if (cc == quotation) {
												bool escaped = false;
												while (j - 1 >= 0 && text[i][j - 1] == '\\') {
													escaped = !escaped;
													j--;
												}
												if (escaped) {
													cc = '\\';
													continue;
												}
											}
										} while (cc != quotation);
									} else if (cc == c) {
										stack++;
									} else if (cc == closec) {
										stack--;
									}

									if (stack == 0) {
										brace_match.close_match_line = i;
										brace_match.close_match_column = j;
										brace_match.close_matching = true;

										break;
									}
								}
								if (brace_match.close_match_line != -1) {
									break;
								}
							}

							if (!brace_match.close_matching) {
								brace_match.close_mismatch = true;
							}
						}
					}
				}
			}

			bool draw_placeholder = _using_placeholder();

			// Get the highlighted words.
			String highlighted_text = get_selected_text(0);

			// Check if highlighted words contain only whitespaces (tabs or spaces).
			bool only_whitespaces_highlighted = highlighted_text.strip_edges().is_empty();

			Vector<Pair<int, int>> highlighted_lines;
			highlighted_lines.resize(carets.size());
			Vector<int> carets_wrap_index;
			carets_wrap_index.resize(carets.size());
			for (int i = 0; i < carets.size(); i++) {
				carets.write[i].visible = false;
				int wrap_index = get_caret_wrap_index(i);
				highlighted_lines.write[i] = Pair<int, int>(get_caret_line(i), wrap_index);
				carets_wrap_index.write[i] = wrap_index;
			}

			int first_vis_line = get_first_visible_line() - 1;
			int draw_amount = visible_rows + (smooth_scroll_enabled ? 1 : 0);
			draw_amount += draw_placeholder ? placeholder_wraped_rows.size() - 1 : get_line_wrap_count(first_vis_line + 1);

			// Draw minimap.
			if (draw_minimap) {
				int minimap_visible_lines = get_minimap_visible_lines();
				int minimap_line_height = (minimap_char_size.y + minimap_line_spacing);
				int tab_size = text.get_tab_size();

				// Calculate viewport size and y offset.
				int viewport_height = (draw_amount - 1) * minimap_line_height;
				int control_height = _get_control_height() - viewport_height;
				int viewport_offset_y = round(get_scroll_pos_for_line(first_vis_line + 1) * control_height) / ((v_scroll->get_max() <= minimap_visible_lines) ? (minimap_visible_lines - draw_amount) : (v_scroll->get_max() - draw_amount));

				// Calculate the first line.
				int num_lines_before = round((viewport_offset_y) / minimap_line_height);
				int minimap_line = (v_scroll->get_max() <= minimap_visible_lines) ? -1 : first_vis_line;
				if (minimap_line >= 0) {
					minimap_line -= get_next_visible_line_index_offset_from(first_vis_line, 0, -num_lines_before).x;
					minimap_line -= (minimap_line > 0 && smooth_scroll_enabled ? 1 : 0);
				}
				int minimap_draw_amount = minimap_visible_lines + get_line_wrap_count(minimap_line + 1);

				// Draw the minimap.

				// Add visual feedback when dragging or hovering the visible area rectangle.
				Color viewport_color = theme_cache.caret_color;
				if (dragging_minimap) {
					viewport_color.a = 0.25;
				} else if (hovering_minimap) {
					viewport_color.a = 0.175;
				} else {
					viewport_color.a = 0.1;
				}

				if (rtl) {
					RenderingServer::get_singleton()->canvas_item_add_rect(ci, Rect2(size.width - (xmargin_end + 2) - minimap_width, viewport_offset_y, minimap_width, viewport_height), viewport_color);
				} else {
					RenderingServer::get_singleton()->canvas_item_add_rect(ci, Rect2((xmargin_end + 2), viewport_offset_y, minimap_width, viewport_height), viewport_color);
				}

				for (int i = 0; i < minimap_draw_amount; i++) {
					minimap_line++;

					if (minimap_line < 0 || minimap_line >= (int)text.size()) {
						break;
					}

					while (_is_line_hidden(minimap_line)) {
						minimap_line++;
						if (minimap_line < 0 || minimap_line >= (int)text.size()) {
							break;
						}
					}

					if (minimap_line < 0 || minimap_line >= (int)text.size()) {
						break;
					}

					const Vector<Pair<int64_t, Color>> color_map = _get_line_syntax_highlighting(minimap_line);

					Color line_background_color = text.get_line_background_color(minimap_line);

					if (line_background_color != theme_cache.background_color) {
						// Make non-default background colors more visible, such as error markers.
						line_background_color.a = 1.0;
					} else {
						line_background_color.a *= 0.6;
					}

					Color current_color = editable ? theme_cache.font_color : theme_cache.font_readonly_color;

					const Vector<String> wrap_rows = get_line_wrapped_text(minimap_line);
					int line_wrap_amount = get_line_wrap_count(minimap_line);
					int last_wrap_column = 0;

					for (int line_wrap_index = 0; line_wrap_index < line_wrap_amount + 1; line_wrap_index++) {
						if (line_wrap_index != 0) {
							i++;
							if (i >= minimap_draw_amount) {
								break;
							}
						}

						const String &str = wrap_rows[line_wrap_index];
						int indent_px = line_wrap_index != 0 ? get_indent_level(minimap_line) : 0;
						if (indent_px >= wrap_at_column) {
							indent_px = 0;
						}
						indent_px = minimap_char_size.x * indent_px;

						if (line_wrap_index > 0) {
							last_wrap_column += wrap_rows[line_wrap_index - 1].length();
						}

						if (highlight_current_line && highlighted_lines.has(Pair<int, int>(minimap_line, line_wrap_index))) {
							if (rtl) {
								RenderingServer::get_singleton()->canvas_item_add_rect(ci, Rect2(size.width - (xmargin_end + 2) - minimap_width, i * 3, minimap_width, 2), theme_cache.current_line_color);
							} else {
								RenderingServer::get_singleton()->canvas_item_add_rect(ci, Rect2((xmargin_end + 2), i * 3, minimap_width, 2), theme_cache.current_line_color);
							}
						} else if (line_background_color.a > 0) {
							if (rtl) {
								RenderingServer::get_singleton()->canvas_item_add_rect(ci, Rect2(size.width - (xmargin_end + 2) - minimap_width, i * 3, minimap_width, 2), line_background_color);
							} else {
								RenderingServer::get_singleton()->canvas_item_add_rect(ci, Rect2((xmargin_end + 2), i * 3, minimap_width, 2), line_background_color);
							}
						}

						Color next_color = current_color;
						int characters = 0;
						int tab_alignment = 0;
						int xpos = xmargin_end + 2 + indent_px;
						for (int j = 0; j < str.length(); j++) {
							bool next_is_whitespace = false;
							bool next_is_tab = false;
							// Get the number of characters to draw together.
							for (characters = 0; j + characters < str.length(); characters++) {
								int next_char_index = j + characters;

								for (const Pair<int64_t, Color> &color_data : color_map) {
									if (last_wrap_column + next_char_index >= color_data.first) {
										next_color = color_data.second;
										if (!editable) {
											next_color.a = theme_cache.font_readonly_color.a;
										}
										next_color.a *= 0.6;
									} else {
										break;
									}
								}
								if (characters == 0) {
									current_color = next_color;
								}
								if (next_color != current_color) {
									break;
								}
								next_is_whitespace = is_whitespace(str[next_char_index]);
								if (next_is_whitespace) {
									if (str[next_char_index] == '\t') {
										next_is_tab = true;
									}
									break;
								}
								bool out_of_bounds = xpos + minimap_char_size.x * characters >= xmargin_end + minimap_width;
								if (out_of_bounds) {
									break;
								}
							}
							if (!next_is_whitespace && characters == 0) {
								break;
							}

							if (characters > 0) {
								if (rtl) {
									RenderingServer::get_singleton()->canvas_item_add_rect(ci, Rect2(Point2(size.width - xpos - minimap_char_size.x * characters, minimap_line_height * i), Point2(minimap_char_size.x * characters, minimap_char_size.y)), current_color);
								} else {
									RenderingServer::get_singleton()->canvas_item_add_rect(ci, Rect2(Point2(xpos, minimap_line_height * i), Point2(minimap_char_size.x * characters, minimap_char_size.y)), current_color);
								}
							}

							j += characters - 1;
							xpos += minimap_char_size.x * characters;
							tab_alignment += characters;

							if (next_is_whitespace) {
								if (next_is_tab) {
									tab_alignment %= tab_size;
									xpos += minimap_char_size.x * (tab_size - tab_alignment);
									tab_alignment = 0;
								} else {
									xpos += minimap_char_size.x;
									tab_alignment += 1;
								}
								j += 1;
							}

							if (xpos >= xmargin_end + minimap_width) {
								// Out of bounds.
								break;
							}
						}
					}
				}
			}

			int top_limit_y = 0;
			int bottom_limit_y = get_size().height;
			if (!editable) {
				top_limit_y += theme_cache.style_readonly->get_margin(SIDE_TOP);
				bottom_limit_y -= theme_cache.style_readonly->get_margin(SIDE_BOTTOM);
			} else {
				top_limit_y += theme_cache.style_normal->get_margin(SIDE_TOP);
				bottom_limit_y -= theme_cache.style_normal->get_margin(SIDE_BOTTOM);
			}

			// Draw main text.
			line_drawing_cache.clear();
			int row_height = draw_placeholder ? placeholder_line_height + theme_cache.line_spacing : get_line_height();
			int line = first_vis_line;
			for (int i = 0; i < draw_amount; i++) {
				line++;

				if (line < 0 || line >= (int)text.size()) {
					continue;
				}

				while (_is_line_hidden(line)) {
					line++;
					if (line < 0 || line >= (int)text.size()) {
						break;
					}
				}

				if (line < 0 || line >= (int)text.size()) {
					continue;
				}

				LineDrawingCache cache_entry;

				const Vector<Pair<int64_t, Color>> color_map = _get_line_syntax_highlighting(line);

				// Ensure we at least use the font color.
				Color current_color = !editable ? theme_cache.font_readonly_color : theme_cache.font_color;
				if (draw_placeholder) {
					current_color = theme_cache.font_placeholder_color;
				}

				const Ref<TextParagraph> ldata = draw_placeholder ? placeholder_data_buf : text.get_line_data(line);

				const Vector<String> wrap_rows = draw_placeholder ? placeholder_wraped_rows : get_line_wrapped_text(line);
				int line_wrap_amount = draw_placeholder ? placeholder_wraped_rows.size() - 1 : get_line_wrap_count(line);

				for (int line_wrap_index = 0; line_wrap_index <= line_wrap_amount; line_wrap_index++) {
					if (line_wrap_index != 0) {
						i++;
						if (i >= draw_amount) {
							break;
						}
					}

					const String &str = wrap_rows[line_wrap_index];
					int char_margin = xmargin_beg - first_visible_col;

					int ofs_x = 0;
					int ofs_y = 0;
					if (!editable) {
						ofs_x = theme_cache.style_readonly->get_offset().x / 2;
						ofs_x -= theme_cache.style_normal->get_offset().x / 2;
						ofs_y = theme_cache.style_readonly->get_offset().y / 2;
					} else {
						ofs_y = theme_cache.style_normal->get_offset().y / 2;
					}

					ofs_y += i * row_height + theme_cache.line_spacing / 2;
					ofs_y -= first_visible_line_wrap_ofs * row_height;
					ofs_y -= _get_v_scroll_offset() * row_height;

					bool clipped = false;
					if (ofs_y + row_height < top_limit_y) {
						// Line is outside the top margin, clip current line.
						// Still need to go through the process to prepare color changes for next lines.
						clipped = true;
					}

					if (ofs_y > bottom_limit_y) {
						// Line is outside the bottom margin, clip any remaining text.
						i = draw_amount;
						break;
					}

					if (text.get_line_background_color(line).a > 0.0) {
						if (rtl) {
							RS::get_singleton()->canvas_item_add_rect(ci, Rect2(size.width - ofs_x - xmargin_end, ofs_y, xmargin_end - xmargin_beg, row_height), text.get_line_background_color(line));
						} else {
							RS::get_singleton()->canvas_item_add_rect(ci, Rect2(xmargin_beg + ofs_x, ofs_y, xmargin_end - xmargin_beg, row_height), text.get_line_background_color(line));
						}
					}

					// Draw current line highlight.
					if (highlight_current_line && highlighted_lines.has(Pair<int, int>(line, line_wrap_index))) {
						if (rtl) {
							RS::get_singleton()->canvas_item_add_rect(ci, Rect2(size.width - ofs_x - xmargin_end, ofs_y, xmargin_end, row_height), theme_cache.current_line_color);
						} else {
							RS::get_singleton()->canvas_item_add_rect(ci, Rect2(ofs_x, ofs_y, xmargin_end, row_height), theme_cache.current_line_color);
						}
					}

					if (line_wrap_index == 0) {
						// Only do these if we are on the first wrapped part of a line.

						cache_entry.y_offset = ofs_y;

						int gutter_offset = theme_cache.style_normal->get_margin(SIDE_LEFT);
						for (int g = 0; g < gutters.size(); g++) {
							const GutterInfo &gutter = gutters[g];

							if (!gutter.draw || gutter.width <= 0) {
								continue;
							}

							switch (gutter.type) {
								case GUTTER_TYPE_STRING: {
									const String &txt = get_line_gutter_text(line, g);
									if (txt.is_empty()) {
										break;
									}

									Ref<TextLine> tl;
									tl.instantiate();
									tl->add_string(txt, theme_cache.font, theme_cache.font_size);

									int yofs = ofs_y + (row_height - tl->get_size().y) / 2;
									if (theme_cache.outline_size > 0 && theme_cache.outline_color.a > 0) {
										tl->draw_outline(ci, Point2(gutter_offset + ofs_x, yofs), theme_cache.outline_size, theme_cache.outline_color);
									}
									tl->draw(ci, Point2(gutter_offset + ofs_x, yofs), get_line_gutter_item_color(line, g));
								} break;
								case GUTTER_TYPE_ICON: {
									const Ref<Texture2D> icon = get_line_gutter_icon(line, g);
									if (icon.is_null()) {
										break;
									}

									Rect2 gutter_rect = Rect2(Point2i(gutter_offset, ofs_y), Size2i(gutter.width, row_height));

									int horizontal_padding = gutter_rect.size.x / 6;
									int vertical_padding = gutter_rect.size.y / 6;

									gutter_rect.position += Point2(horizontal_padding, vertical_padding);
									gutter_rect.size -= Point2(horizontal_padding, vertical_padding) * 2;

									// Correct icon aspect ratio.
									float icon_ratio = icon->get_width() / icon->get_height();
									float gutter_ratio = gutter_rect.size.x / gutter_rect.size.y;
									if (gutter_ratio > icon_ratio) {
										gutter_rect.size.x = floor(icon->get_width() * (gutter_rect.size.y / icon->get_height()));
									} else {
										gutter_rect.size.y = floor(icon->get_height() * (gutter_rect.size.x / icon->get_width()));
									}
									if (rtl) {
										gutter_rect.position.x = size.width - gutter_rect.position.x - gutter_rect.size.x;
									}

									icon->draw_rect(ci, gutter_rect, false, get_line_gutter_item_color(line, g));
								} break;
								case GUTTER_TYPE_CUSTOM: {
									if (gutter.custom_draw_callback.is_valid()) {
										Rect2i gutter_rect = Rect2i(Point2i(gutter_offset, ofs_y), Size2i(gutter.width, row_height));
										if (rtl) {
											gutter_rect.position.x = size.width - gutter_rect.position.x - gutter_rect.size.x;
										}
										gutter.custom_draw_callback.call(line, g, Rect2(gutter_rect));
									}
								} break;
							}

							gutter_offset += gutter.width;
						}
					}

					// Draw line.
					RID rid = ldata->get_line_rid(line_wrap_index);
					float text_height = TS->shaped_text_get_size(rid).y;
					float wrap_indent = (text.is_indent_wrapped_lines() && line_wrap_index > 0) ? get_indent_level(line) * theme_cache.font->get_char_size(' ', theme_cache.font_size).width : 0.0;

					if (rtl) {
						char_margin = size.width - char_margin - (TS->shaped_text_get_size(rid).x + wrap_indent);
					} else {
						char_margin += wrap_indent;
					}

					// Draw selections.
					float char_w = theme_cache.font->get_char_size(' ', theme_cache.font_size).width;
					for (int c = 0; c < get_caret_count(); c++) {
						if (!clipped && has_selection(c) && line >= get_selection_from_line(c) && line <= get_selection_to_line(c)) {
							int sel_from = (line > get_selection_from_line(c)) ? TS->shaped_text_get_range(rid).x : get_selection_from_column(c);
							int sel_to = (line < get_selection_to_line(c)) ? TS->shaped_text_get_range(rid).y : get_selection_to_column(c);
							Vector<Vector2> sel = TS->shaped_text_get_selection(rid, sel_from, sel_to);

							// Show selection at the end of line.
							if (line_wrap_index == line_wrap_amount && line < get_selection_to_line(c)) {
								if (rtl) {
									sel.push_back(Vector2(-char_w, 0));
								} else {
									float line_end = TS->shaped_text_get_size(rid).width;
									sel.push_back(Vector2(line_end, line_end + char_w));
								}
							}

							for (int j = 0; j < sel.size(); j++) {
								Rect2 rect = Rect2(sel[j].x + char_margin + ofs_x, ofs_y, Math::ceil(sel[j].y) - sel[j].x, row_height);
								if (rect.position.x + rect.size.x <= xmargin_beg || rect.position.x > xmargin_end) {
									continue;
								}
								if (rect.position.x < xmargin_beg) {
									rect.size.x -= (xmargin_beg - rect.position.x);
									rect.position.x = xmargin_beg;
								}
								if (rect.position.x + rect.size.x > xmargin_end) {
									rect.size.x = xmargin_end - rect.position.x;
								}
								RS::get_singleton()->canvas_item_add_rect(ci, rect, theme_cache.selection_color);
							}
						}
					}

					int start = TS->shaped_text_get_range(rid).x;
					if (!clipped && !search_text.is_empty()) { // Search highlight
						int search_text_col = _get_column_pos_of_word(search_text, str, search_flags, 0);
						int search_text_len = search_text.length();
						while (search_text_col != -1) {
							const Vector<Vector2> sel = TS->shaped_text_get_selection(rid, search_text_col + start, search_text_col + search_text_len + start);
							for (int j = 0; j < sel.size(); j++) {
								Rect2 rect = Rect2(sel[j].x + char_margin + ofs_x, ofs_y, sel[j].y - sel[j].x, row_height);
								if (rect.position.x + rect.size.x <= xmargin_beg || rect.position.x > xmargin_end) {
									continue;
								}
								if (rect.position.x < xmargin_beg) {
									rect.size.x -= (xmargin_beg - rect.position.x);
									rect.position.x = xmargin_beg;
								} else if (rect.position.x + rect.size.x > xmargin_end) {
									rect.size.x = xmargin_end - rect.position.x;
								}
								RS::get_singleton()->canvas_item_add_rect(ci, rect, theme_cache.search_result_color);
								draw_rect(rect, theme_cache.search_result_border_color, false);
							}

							search_text_col = _get_column_pos_of_word(search_text, str, search_flags, search_text_col + search_text_len);
						}
					}

					if (!clipped && highlight_all_occurrences && !only_whitespaces_highlighted && !highlighted_text.is_empty()) { // Highlight
						int highlighted_text_col = _get_column_pos_of_word(highlighted_text, str, SEARCH_MATCH_CASE | SEARCH_WHOLE_WORDS, 0);
						int highlighted_text_len = highlighted_text.length();
						while (highlighted_text_col != -1) {
							const Vector<Vector2> sel = TS->shaped_text_get_selection(rid, highlighted_text_col + start, highlighted_text_col + highlighted_text_len + start);
							for (int j = 0; j < sel.size(); j++) {
								Rect2 rect = Rect2(sel[j].x + char_margin + ofs_x, ofs_y, sel[j].y - sel[j].x, row_height);
								if (rect.position.x + rect.size.x <= xmargin_beg || rect.position.x > xmargin_end) {
									continue;
								}
								if (rect.position.x < xmargin_beg) {
									rect.size.x -= (xmargin_beg - rect.position.x);
									rect.position.x = xmargin_beg;
								} else if (rect.position.x + rect.size.x > xmargin_end) {
									rect.size.x = xmargin_end - rect.position.x;
								}
								RS::get_singleton()->canvas_item_add_rect(ci, rect, theme_cache.word_highlighted_color);
							}

							highlighted_text_col = _get_column_pos_of_word(highlighted_text, str, SEARCH_MATCH_CASE | SEARCH_WHOLE_WORDS, highlighted_text_col + highlighted_text_len);
						}
					}

					if (!clipped && lookup_symbol_word.length() != 0) { // Highlight word
						if (is_unicode_identifier_start(lookup_symbol_word[0]) || lookup_symbol_word[0] == '.') {
							Color highlight_underline_color = !editable ? theme_cache.font_readonly_color : theme_cache.font_color;
							int lookup_symbol_word_col = _get_column_pos_of_word(lookup_symbol_word, str, SEARCH_MATCH_CASE | SEARCH_WHOLE_WORDS, 0);
							int lookup_symbol_word_len = lookup_symbol_word.length();
							while (lookup_symbol_word_col != -1) {
								const Vector<Vector2> sel = TS->shaped_text_get_selection(rid, lookup_symbol_word_col + start, lookup_symbol_word_col + lookup_symbol_word_len + start);
								for (int j = 0; j < sel.size(); j++) {
									Rect2 rect = Rect2(sel[j].x + char_margin + ofs_x, ofs_y + (theme_cache.line_spacing / 2), sel[j].y - sel[j].x, row_height);
									if (rect.position.x + rect.size.x <= xmargin_beg || rect.position.x > xmargin_end) {
										continue;
									}
									if (rect.position.x < xmargin_beg) {
										rect.size.x -= (xmargin_beg - rect.position.x);
										rect.position.x = xmargin_beg;
									} else if (rect.position.x + rect.size.x > xmargin_end) {
										rect.size.x = xmargin_end - rect.position.x;
									}
									rect.position.y += ceil(TS->shaped_text_get_ascent(rid)) + ceil(theme_cache.font->get_underline_position(theme_cache.font_size));
									rect.size.y = MAX(1, theme_cache.font->get_underline_thickness(theme_cache.font_size));
									RS::get_singleton()->canvas_item_add_rect(ci, rect, highlight_underline_color);
								}

								lookup_symbol_word_col = _get_column_pos_of_word(lookup_symbol_word, str, SEARCH_MATCH_CASE | SEARCH_WHOLE_WORDS, lookup_symbol_word_col + lookup_symbol_word_len);
							}
						}
					}

					ofs_y += (row_height - text_height) / 2;

					const Glyph *glyphs = TS->shaped_text_get_glyphs(rid);
					int gl_size = TS->shaped_text_get_glyph_count(rid);

					ofs_y += ldata->get_line_ascent(line_wrap_index);

					int first_visible_char = TS->shaped_text_get_range(rid).y;
					int last_visible_char = TS->shaped_text_get_range(rid).x;

					float char_ofs = 0;
					if (theme_cache.outline_size > 0 && theme_cache.outline_color.a > 0) {
						for (int j = 0; j < gl_size; j++) {
							for (int k = 0; k < glyphs[j].repeat; k++) {
								if ((char_ofs + char_margin) >= xmargin_beg && (char_ofs + glyphs[j].advance + char_margin) <= xmargin_end) {
									if (glyphs[j].font_rid != RID()) {
										TS->font_draw_glyph_outline(glyphs[j].font_rid, ci, glyphs[j].font_size, theme_cache.outline_size, Vector2(char_margin + char_ofs + ofs_x + glyphs[j].x_off, ofs_y + glyphs[j].y_off), glyphs[j].index, theme_cache.outline_color);
									}
								}
								char_ofs += glyphs[j].advance;
							}
							if ((char_ofs + char_margin) >= xmargin_end) {
								break;
							}
						}
						char_ofs = 0;
					}
					for (int j = 0; j < gl_size; j++) {
						for (const Pair<int64_t, Color> &color_data : color_map) {
							if (color_data.first <= glyphs[j].start) {
								current_color = color_data.second;
								if (!editable && current_color.a > theme_cache.font_readonly_color.a) {
									current_color.a = theme_cache.font_readonly_color.a;
								}
							} else {
								break;
							}
						}
						Color gl_color = current_color;

						for (int c = 0; c < get_caret_count(); c++) {
							if (has_selection(c) && line >= get_selection_from_line(c) && line <= get_selection_to_line(c)) { // Selection
								int sel_from = (line > get_selection_from_line(c)) ? TS->shaped_text_get_range(rid).x : get_selection_from_column(c);
								int sel_to = (line < get_selection_to_line(c)) ? TS->shaped_text_get_range(rid).y : get_selection_to_column(c);

								if (glyphs[j].start >= sel_from && glyphs[j].end <= sel_to && use_selected_font_color) {
									gl_color = theme_cache.font_selected_color;
								}
							}
						}

						float char_pos = char_ofs + char_margin + ofs_x;
						if (char_pos >= xmargin_beg) {
							if (highlight_matching_braces_enabled) {
								for (int c = 0; c < get_caret_count(); c++) {
									const BraceMatchingData &brace_match = brace_matching[c];
									if ((brace_match.open_match_line == line && brace_match.open_match_column == glyphs[j].start) ||
											(get_caret_column(c) == glyphs[j].start && get_caret_line(c) == line && carets_wrap_index[c] == line_wrap_index && (brace_match.open_matching || brace_match.open_mismatch))) {
										if (brace_match.open_mismatch) {
											gl_color = _get_brace_mismatch_color();
										}
										Rect2 rect = Rect2(char_pos, ofs_y + theme_cache.font->get_underline_position(theme_cache.font_size), glyphs[j].advance * glyphs[j].repeat, MAX(theme_cache.font->get_underline_thickness(theme_cache.font_size) * theme_cache.base_scale, 1));
										RS::get_singleton()->canvas_item_add_rect(ci, rect, gl_color);
									}

									if ((brace_match.close_match_line == line && brace_match.close_match_column == glyphs[j].start) ||
											(get_caret_column(c) == glyphs[j].start + 1 && get_caret_line(c) == line && carets_wrap_index[c] == line_wrap_index && (brace_match.close_matching || brace_match.close_mismatch))) {
										if (brace_match.close_mismatch) {
											gl_color = _get_brace_mismatch_color();
										}
										Rect2 rect = Rect2(char_pos, ofs_y + theme_cache.font->get_underline_position(theme_cache.font_size), glyphs[j].advance * glyphs[j].repeat, MAX(theme_cache.font->get_underline_thickness(theme_cache.font_size) * theme_cache.base_scale, 1));
										RS::get_singleton()->canvas_item_add_rect(ci, rect, gl_color);
									}
								}
							}

							if (draw_tabs && ((glyphs[j].flags & TextServer::GRAPHEME_IS_TAB) == TextServer::GRAPHEME_IS_TAB)) {
								int yofs = (text_height - theme_cache.tab_icon->get_height()) / 2 - ldata->get_line_ascent(line_wrap_index);
								theme_cache.tab_icon->draw(ci, Point2(char_pos, ofs_y + yofs), gl_color);
							} else if (draw_spaces && ((glyphs[j].flags & TextServer::GRAPHEME_IS_SPACE) == TextServer::GRAPHEME_IS_SPACE) && ((glyphs[j].flags & TextServer::GRAPHEME_IS_VIRTUAL) != TextServer::GRAPHEME_IS_VIRTUAL)) {
								int yofs = (text_height - theme_cache.space_icon->get_height()) / 2 - ldata->get_line_ascent(line_wrap_index);
								int xofs = (glyphs[j].advance * glyphs[j].repeat - theme_cache.space_icon->get_width()) / 2;
								theme_cache.space_icon->draw(ci, Point2(char_pos + xofs, ofs_y + yofs), gl_color);
							}
						}

						bool had_glyphs_drawn = false;
						for (int k = 0; k < glyphs[j].repeat; k++) {
							if (!clipped && (char_ofs + char_margin) >= xmargin_beg && (char_ofs + glyphs[j].advance + char_margin) <= xmargin_end) {
								if (glyphs[j].font_rid != RID()) {
									TS->font_draw_glyph(glyphs[j].font_rid, ci, glyphs[j].font_size, Vector2(char_margin + char_ofs + ofs_x + glyphs[j].x_off, ofs_y + glyphs[j].y_off), glyphs[j].index, gl_color);
									had_glyphs_drawn = true;
								} else if (((glyphs[j].flags & TextServer::GRAPHEME_IS_VIRTUAL) != TextServer::GRAPHEME_IS_VIRTUAL) && ((glyphs[j].flags & TextServer::GRAPHEME_IS_EMBEDDED_OBJECT) != TextServer::GRAPHEME_IS_EMBEDDED_OBJECT)) {
									TS->draw_hex_code_box(ci, glyphs[j].font_size, Vector2(char_margin + char_ofs + ofs_x + glyphs[j].x_off, ofs_y + glyphs[j].y_off), glyphs[j].index, gl_color);
									had_glyphs_drawn = true;
								}
							}
							char_ofs += glyphs[j].advance;
						}

						if (had_glyphs_drawn) {
							if (first_visible_char > glyphs[j].start) {
								first_visible_char = glyphs[j].start;
							}
							if (last_visible_char < glyphs[j].end) {
								last_visible_char = glyphs[j].end;
							}
						}

						if ((char_ofs + char_margin) >= xmargin_end) {
							break;
						}
					}

					cache_entry.first_visible_chars.push_back(first_visible_char);
					cache_entry.last_visible_chars.push_back(last_visible_char);

					// is_line_folded
					if (line_wrap_index == line_wrap_amount && line < text.size() - 1 && _is_line_hidden(line + 1)) {
						int xofs = char_ofs + char_margin + ofs_x + (_get_folded_eol_icon()->get_width() / 2);
						if (xofs >= xmargin_beg && xofs < xmargin_end) {
							int yofs = (text_height - _get_folded_eol_icon()->get_height()) / 2 - ldata->get_line_ascent(line_wrap_index);
							Color eol_color = _get_code_folding_color();
							eol_color.a = 1;
							_get_folded_eol_icon()->draw(ci, Point2(xofs, ofs_y + yofs), eol_color);
						}
					}

					// Carets.
					// Prevent carets from disappearing at theme scales below 1.0 (if the caret width is 1).
					const int caret_width = theme_cache.caret_width * MAX(1, theme_cache.base_scale);

					for (int c = 0; c < carets.size(); c++) {
						if (!clipped && get_caret_line(c) == line && carets_wrap_index[c] == line_wrap_index) {
							carets.write[c].draw_pos.y = ofs_y + ldata->get_line_descent(line_wrap_index);

							if (ime_text.is_empty() || ime_selection.y == 0) {
								// Normal caret.
								CaretInfo ts_caret;
								if (!str.is_empty() || !ime_text.is_empty()) {
									// Get carets.
									ts_caret = TS->shaped_text_get_carets(rid, ime_text.is_empty() ? get_caret_column(c) : get_caret_column(c) + ime_selection.x);
								} else {
									// No carets, add one at the start.
									int h = theme_cache.font->get_height(theme_cache.font_size);
									if (rtl) {
										ts_caret.l_dir = TextServer::DIRECTION_RTL;
										ts_caret.l_caret = Rect2(Vector2(xmargin_end - char_margin + ofs_x, -h / 2), Size2(caret_width * 4, h));
									} else {
										ts_caret.l_dir = TextServer::DIRECTION_LTR;
										ts_caret.l_caret = Rect2(Vector2(char_ofs, -h / 2), Size2(caret_width * 4, h));
									}
								}

								if ((ts_caret.l_caret != Rect2() && (ts_caret.l_dir == TextServer::DIRECTION_AUTO || ts_caret.l_dir == (TextServer::Direction)input_direction)) || (ts_caret.t_caret == Rect2())) {
									carets.write[c].draw_pos.x = char_margin + ofs_x + ts_caret.l_caret.position.x;
								} else {
									carets.write[c].draw_pos.x = char_margin + ofs_x + ts_caret.t_caret.position.x;
								}

								if (get_caret_draw_pos(c).x >= xmargin_beg && get_caret_draw_pos(c).x < xmargin_end) {
									carets.write[c].visible = true;
									if (draw_caret || drag_caret_force_displayed) {
										if (caret_type == CaretType::CARET_TYPE_BLOCK || overtype_mode) {
											//Block or underline caret, draw trailing carets at full height.
											int h = theme_cache.font->get_height(theme_cache.font_size);

											if (ts_caret.t_caret != Rect2()) {
												if (overtype_mode) {
													ts_caret.t_caret.position.y = TS->shaped_text_get_descent(rid);
													ts_caret.t_caret.size.y = caret_width;
												} else {
													ts_caret.t_caret.position.y = -TS->shaped_text_get_ascent(rid);
													ts_caret.t_caret.size.y = h;
												}
												ts_caret.t_caret.position += Vector2(char_margin + ofs_x, ofs_y);
												draw_rect(ts_caret.t_caret, theme_cache.caret_color, overtype_mode);

												if (ts_caret.l_caret != Rect2() && ts_caret.l_dir != ts_caret.t_dir) {
													// Draw split caret (leading part).
													ts_caret.l_caret.position += Vector2(char_margin + ofs_x, ofs_y);
													ts_caret.l_caret.size.x = caret_width;
													RS::get_singleton()->canvas_item_add_rect(ci, ts_caret.l_caret, theme_cache.caret_color);
													// Draw extra direction marker on top of split caret.
													float d = (ts_caret.l_dir == TextServer::DIRECTION_LTR) ? 0.5 : -3;
													Rect2 trect = Rect2(ts_caret.l_caret.position.x + d * caret_width, ts_caret.l_caret.position.y + ts_caret.l_caret.size.y - caret_width, 3 * caret_width, caret_width);
													RS::get_singleton()->canvas_item_add_rect(ci, trect, theme_cache.caret_color);
												}
											} else { // End of the line.
												if (gl_size > 0) {
													// Adjust for actual line dimensions.
													if (overtype_mode) {
														ts_caret.l_caret.position.y = TS->shaped_text_get_descent(rid);
														ts_caret.l_caret.size.y = caret_width;
													} else {
														ts_caret.l_caret.position.y = -TS->shaped_text_get_ascent(rid);
														ts_caret.l_caret.size.y = h;
													}
												} else if (overtype_mode) {
													ts_caret.l_caret.position.y += ts_caret.l_caret.size.y;
													ts_caret.l_caret.size.y = caret_width;
												}
												if (Math::ceil(ts_caret.l_caret.position.x) >= TS->shaped_text_get_size(rid).x) {
													ts_caret.l_caret.size.x = theme_cache.font->get_char_size('m', theme_cache.font_size).x;
												} else {
													ts_caret.l_caret.size.x = 3 * caret_width;
												}
												ts_caret.l_caret.position += Vector2(char_margin + ofs_x, ofs_y);
												if (ts_caret.l_dir == TextServer::DIRECTION_RTL) {
													ts_caret.l_caret.position.x -= ts_caret.l_caret.size.x;
												}
												draw_rect(ts_caret.l_caret, theme_cache.caret_color, overtype_mode);
											}
										} else {
											// Normal caret.
											if (ts_caret.l_caret != Rect2() && ts_caret.l_dir == TextServer::DIRECTION_AUTO) {
												// Draw extra marker on top of mid caret.
												Rect2 trect = Rect2(ts_caret.l_caret.position.x - 2.5 * caret_width, ts_caret.l_caret.position.y, 6 * caret_width, caret_width);
												trect.position += Vector2(char_margin + ofs_x, ofs_y);
												RS::get_singleton()->canvas_item_add_rect(ci, trect, theme_cache.caret_color);
											} else if (ts_caret.l_caret != Rect2() && ts_caret.t_caret != Rect2() && ts_caret.l_dir != ts_caret.t_dir) {
												// Draw extra direction marker on top of split caret.
												float d = (ts_caret.l_dir == TextServer::DIRECTION_LTR) ? 0.5 : -3;
												Rect2 trect = Rect2(ts_caret.l_caret.position.x + d * caret_width, ts_caret.l_caret.position.y + ts_caret.l_caret.size.y - caret_width, 3 * caret_width, caret_width);
												trect.position += Vector2(char_margin + ofs_x, ofs_y);
												RS::get_singleton()->canvas_item_add_rect(ci, trect, theme_cache.caret_color);

												d = (ts_caret.t_dir == TextServer::DIRECTION_LTR) ? 0.5 : -3;
												trect = Rect2(ts_caret.t_caret.position.x + d * caret_width, ts_caret.t_caret.position.y, 3 * caret_width, caret_width);
												trect.position += Vector2(char_margin + ofs_x, ofs_y);
												RS::get_singleton()->canvas_item_add_rect(ci, trect, theme_cache.caret_color);
											}
											ts_caret.l_caret.position += Vector2(char_margin + ofs_x, ofs_y);
											ts_caret.l_caret.size.x = caret_width;

											RS::get_singleton()->canvas_item_add_rect(ci, ts_caret.l_caret, theme_cache.caret_color);

											ts_caret.t_caret.position += Vector2(char_margin + ofs_x, ofs_y);
											ts_caret.t_caret.size.x = caret_width;

											RS::get_singleton()->canvas_item_add_rect(ci, ts_caret.t_caret, theme_cache.caret_color);
										}
									}
								}
							}
							if (!ime_text.is_empty()) {
								{
									// IME Intermediate text range.
									const Vector<Vector2> sel = TS->shaped_text_get_selection(rid, get_caret_column(c), get_caret_column(c) + ime_text.length());
									for (int j = 0; j < sel.size(); j++) {
										Rect2 rect = Rect2(sel[j].x + char_margin + ofs_x, ofs_y, sel[j].y - sel[j].x, text_height);
										if (rect.position.x + rect.size.x <= xmargin_beg || rect.position.x > xmargin_end) {
											continue;
										}
										if (rect.position.x < xmargin_beg) {
											rect.size.x -= (xmargin_beg - rect.position.x);
											rect.position.x = xmargin_beg;
										} else if (rect.position.x + rect.size.x > xmargin_end) {
											rect.size.x = xmargin_end - rect.position.x;
										}
										rect.size.y = caret_width;
										draw_rect(rect, theme_cache.caret_color);
										carets.write[c].draw_pos.x = rect.position.x;
									}
								}
								if (ime_selection.y > 0) {
									// IME caret.
									const Vector<Vector2> sel = TS->shaped_text_get_selection(rid, get_caret_column(c) + ime_selection.x, get_caret_column(c) + ime_selection.x + ime_selection.y);
									for (int j = 0; j < sel.size(); j++) {
										Rect2 rect = Rect2(sel[j].x + char_margin + ofs_x, ofs_y, sel[j].y - sel[j].x, text_height);
										if (rect.position.x + rect.size.x <= xmargin_beg || rect.position.x > xmargin_end) {
											continue;
										}
										if (rect.position.x < xmargin_beg) {
											rect.size.x -= (xmargin_beg - rect.position.x);
											rect.position.x = xmargin_beg;
										} else if (rect.position.x + rect.size.x > xmargin_end) {
											rect.size.x = xmargin_end - rect.position.x;
										}
										rect.size.y = caret_width * 3;
										draw_rect(rect, theme_cache.caret_color);
										carets.write[c].draw_pos.x = rect.position.x;
									}
								}
							}
						}
					}
				}

				if (!draw_placeholder) {
					line_drawing_cache[line] = cache_entry;
				}
			}

			if (has_focus()) {
				_update_ime_window_position();
			}
		} break;

		case NOTIFICATION_FOCUS_ENTER: {
			if (caret_blink_enabled) {
				caret_blink_timer->start();
			} else {
				draw_caret = true;
			}

			if (editable) {
				_show_virtual_keyboard();
			}
		} break;

		case NOTIFICATION_FOCUS_EXIT: {
			if (caret_blink_enabled) {
				caret_blink_timer->stop();
			}

			apply_ime();

			if (DisplayServer::get_singleton()->has_feature(DisplayServer::FEATURE_VIRTUAL_KEYBOARD) && virtual_keyboard_enabled) {
				DisplayServer::get_singleton()->virtual_keyboard_hide();
			}

			if (deselect_on_focus_loss_enabled && !selection_drag_attempt) {
				deselect();
			}
		} break;

		case MainLoop::NOTIFICATION_OS_IME_UPDATE: {
			if (has_focus()) {
				bool had_ime_text = has_ime_text();
				ime_text = DisplayServer::get_singleton()->ime_get_text();
				ime_selection = DisplayServer::get_singleton()->ime_get_selection();

				if (!had_ime_text && has_ime_text()) {
					_cancel_drag_and_drop_text();
				}

				if (has_ime_text() && has_selection()) {
					delete_selection();
				}

				_update_ime_text();
				adjust_viewport_to_caret(0);
				queue_redraw();
			}
		} break;

		case NOTIFICATION_DRAG_BEGIN: {
			set_selection_mode(SelectionMode::SELECTION_MODE_NONE);
			drag_action = true;
			dragging_minimap = false;
			dragging_selection = false;
			can_drag_minimap = false;
			click_select_held->stop();
		} break;

		case NOTIFICATION_DRAG_END: {
			remove_drag_caret();
			if (selection_drag_attempt && is_drag_successful()) {
				// Dropped elsewhere.
				if (is_editable() && !Input::get_singleton()->is_key_pressed(Key::CMD_OR_CTRL)) {
					delete_selection();
				} else if (deselect_on_focus_loss_enabled) {
					deselect();
				}
			}
			selection_drag_attempt = false;
			drag_action = false;
			drag_caret_force_displayed = false;
		} break;

		case NOTIFICATION_MOUSE_EXIT_SELF: {
			if (drag_caret_force_displayed) {
				drag_caret_force_displayed = false;
				queue_redraw();
			}
		} break;
	}
}

void TextEdit::unhandled_key_input(const Ref<InputEvent> &p_event) {
	Ref<InputEventKey> k = p_event;

	if (k.is_valid()) {
		if (!k->is_pressed()) {
			return;
		}
		// Handle Unicode (with modifiers active, process after shortcuts).
		if (has_focus() && editable && (k->get_unicode() >= 32)) {
			handle_unicode_input(k->get_unicode());
			accept_event();
		}
	}
}

bool TextEdit::alt_input(const Ref<InputEvent> &p_gui_input) {
	Ref<InputEventKey> k = p_gui_input;
	if (k.is_valid()) {
		// Start Unicode input (hold).
		if (k->is_alt_pressed() && k->get_keycode() == Key::KP_ADD && !alt_start && !alt_start_no_hold) {
			if (has_selection()) {
				delete_selection();
			}
			alt_start = true;
			alt_code = 0;
			ime_text = "u";
			ime_selection = Vector2i(0, -1);
			_update_ime_text();
			return true;
		}

		// Start Unicode input (press).
		if (k->is_action("ui_unicode_start", true) && !alt_start && !alt_start_no_hold) {
			if (has_selection()) {
				delete_selection();
			}
			alt_start_no_hold = true;
			alt_code = 0;
			ime_text = "u";
			ime_selection = Vector2i(0, -1);
			_update_ime_text();
			return true;
		}

		// Update Unicode input.
		if (k->is_pressed() && ((k->is_alt_pressed() && alt_start) || alt_start_no_hold)) {
			if (k->get_keycode() >= Key::KEY_0 && k->get_keycode() <= Key::KEY_9) {
				alt_code = alt_code << 4;
				alt_code += (uint32_t)(k->get_keycode() - Key::KEY_0);
			} else if (k->get_keycode() >= Key::KP_0 && k->get_keycode() <= Key::KP_9) {
				alt_code = alt_code << 4;
				alt_code += (uint32_t)(k->get_keycode() - Key::KP_0);
			} else if (k->get_keycode() >= Key::A && k->get_keycode() <= Key::F) {
				alt_code = alt_code << 4;
				alt_code += (uint32_t)(k->get_keycode() - Key::A) + 10;
			} else if ((Key)k->get_unicode() >= Key::KEY_0 && (Key)k->get_unicode() <= Key::KEY_9) {
				alt_code = alt_code << 4;
				alt_code += (uint32_t)((Key)k->get_unicode() - Key::KEY_0);
			} else if ((Key)k->get_unicode() >= Key::A && (Key)k->get_unicode() <= Key::F) {
				alt_code = alt_code << 4;
				alt_code += (uint32_t)((Key)k->get_unicode() - Key::A) + 10;
			} else if (k->get_physical_keycode() >= Key::KEY_0 && k->get_physical_keycode() <= Key::KEY_9) {
				alt_code = alt_code << 4;
				alt_code += (uint32_t)(k->get_physical_keycode() - Key::KEY_0);
			}
			if (k->get_keycode() == Key::BACKSPACE) {
				alt_code = alt_code >> 4;
			}
			if (alt_code > 0x10ffff) {
				alt_code = 0x10ffff;
			}
			if (alt_code > 0) {
				ime_text = vformat("u%s", String::num_int64(alt_code, 16, true));
			} else {
				ime_text = "u";
			}
			ime_selection = Vector2i(0, -1);
			_update_ime_text();
			return true;
		}

		// Submit Unicode input.
		if ((!k->is_pressed() && alt_start && k->get_keycode() == Key::ALT) || (alt_start_no_hold && (k->is_action("ui_text_submit", true) || k->is_action("ui_accept", true)))) {
			alt_start = false;
			alt_start_no_hold = false;
			if ((alt_code > 0x31 && alt_code < 0xd800) || (alt_code > 0xdfff && alt_code <= 0x10ffff)) {
				ime_text = String();
				ime_selection = Vector2i();
				handle_unicode_input(alt_code);
			} else {
				ime_text = String();
				ime_selection = Vector2i();
			}
			_update_ime_text();
			return true;
		}

		// Cancel Unicode input.
		if (alt_start_no_hold && k->is_action("ui_cancel", true)) {
			alt_start = false;
			alt_start_no_hold = false;
			ime_text = String();
			ime_selection = Vector2i();
			_update_ime_text();
			return true;
		}
	}
	return false;
}

void TextEdit::gui_input(const Ref<InputEvent> &p_gui_input) {
	ERR_FAIL_COND(p_gui_input.is_null());

	double prev_v_scroll = v_scroll->get_value();
	double prev_h_scroll = h_scroll->get_value();

	Ref<InputEventMouseButton> mb = p_gui_input;

	if (mb.is_valid()) {
		Vector2i mpos = mb->get_position();
		if (is_layout_rtl()) {
			mpos.x = get_size().x - mpos.x;
		}

		if (mb->is_pressed()) {
			if (mb->get_button_index() == MouseButton::WHEEL_UP && !mb->is_command_or_control_pressed()) {
				if (mb->is_shift_pressed()) {
					h_scroll->set_value(h_scroll->get_value() - (100 * mb->get_factor()));
				} else if (mb->is_alt_pressed()) {
					// Scroll 5 times as fast as normal (like in Visual Studio Code).
					_scroll_up(15 * mb->get_factor(), true);
				} else if (v_scroll->is_visible()) {
					// Scroll 3 lines.
					_scroll_up(3 * mb->get_factor(), true);
				}
			}
			if (mb->get_button_index() == MouseButton::WHEEL_DOWN && !mb->is_command_or_control_pressed()) {
				if (mb->is_shift_pressed()) {
					h_scroll->set_value(h_scroll->get_value() + (100 * mb->get_factor()));
				} else if (mb->is_alt_pressed()) {
					// Scroll 5 times as fast as normal (like in Visual Studio Code).
					_scroll_down(15 * mb->get_factor(), true);
				} else if (v_scroll->is_visible()) {
					// Scroll 3 lines.
					_scroll_down(3 * mb->get_factor(), true);
				}
			}
			if (mb->get_button_index() == MouseButton::WHEEL_LEFT) {
				h_scroll->set_value(h_scroll->get_value() - (100 * mb->get_factor()));
			}
			if (mb->get_button_index() == MouseButton::WHEEL_RIGHT) {
				h_scroll->set_value(h_scroll->get_value() + (100 * mb->get_factor()));
			}

			if (mb->get_button_index() == MouseButton::LEFT) {
				_reset_caret_blink_timer();

				apply_ime();

				Point2i pos = get_line_column_at_pos(mpos);
				int line = pos.y;
				int col = pos.x;

				// Gutters.
				int left_margin = theme_cache.style_normal->get_margin(SIDE_LEFT);
				for (int i = 0; i < gutters.size(); i++) {
					if (!gutters[i].draw || gutters[i].width <= 0) {
						continue;
					}

					if (mpos.x >= left_margin && mpos.x <= left_margin + gutters[i].width) {
						emit_signal(SNAME("gutter_clicked"), line, i);
						return;
					}

					left_margin += gutters[i].width;
				}

				// Minimap.
				if (draw_minimap) {
					_update_minimap_click();
					if (dragging_minimap) {
						return;
					}
				}

				// Update caret.

				int caret = carets.size() - 1;
				int prev_col = get_caret_column(caret);
				int prev_line = get_caret_line(caret);

				int mouse_over_selection_caret = get_selection_at_line_column(line, col, true);

				const int triple_click_timeout = 600;
				const int triple_click_tolerance = 5;
				bool is_triple_click = (!mb->is_double_click() && (OS::get_singleton()->get_ticks_msec() - last_dblclk) < triple_click_timeout && mb->get_position().distance_to(last_dblclk_pos) < triple_click_tolerance);

				if (!mb->is_double_click() && !is_triple_click) {
					if (mb->is_alt_pressed()) {
						prev_line = line;
						prev_col = col;

						// Remove caret at clicked location.
						if (get_caret_count() > 1) {
							// Deselect if clicked on caret or its selection.
							int clicked_caret = get_selection_at_line_column(line, col, true, false);
							if (clicked_caret != -1) {
								remove_caret(clicked_caret);
								last_dblclk = 0;
								return;
							}
						}

						if (mouse_over_selection_caret >= 0) {
							// Did not remove selection under mouse, don't add a new caret.
							return;
						}

						// Create new caret at clicked location.
						caret = add_caret(line, col);
						if (caret == -1) {
							return;
						}

						last_dblclk = 0;
					} else if (!mb->is_shift_pressed()) {
						if (drag_and_drop_selection_enabled && mouse_over_selection_caret >= 0) {
							// Try to drag and drop.
							set_selection_mode(SelectionMode::SELECTION_MODE_NONE);
							selection_drag_attempt = true;
							drag_and_drop_origin_caret_index = mouse_over_selection_caret;
							last_dblclk = 0;
							// Don't update caret until we know if it is not drag and drop.
							return;
						} else {
							// A regular click clears all other carets.
							caret = 0;
							remove_secondary_carets();
							deselect();
						}
					}

					_push_current_op();
					set_caret_line(line, false, true, -1, caret);
					set_caret_column(col, false, caret);
					selection_drag_attempt = false;
					bool caret_moved = get_caret_column(caret) != prev_col || get_caret_line(caret) != prev_line;

					if (selecting_enabled && mb->is_shift_pressed() && !has_selection(caret) && caret_moved) {
						// Select from the previous caret position.
						select(prev_line, prev_col, line, col, caret);
					}

					// Start regular select mode.
					set_selection_mode(SelectionMode::SELECTION_MODE_POINTER);
					_update_selection_mode_pointer(true);
				} else if (is_triple_click) {
					// Start triple-click select line mode.
					set_selection_mode(SelectionMode::SELECTION_MODE_LINE);
					_update_selection_mode_line(true);
					last_dblclk = 0;
				} else if (mb->is_double_click()) {
					// Start double-click select word mode.
					set_selection_mode(SelectionMode::SELECTION_MODE_WORD);
					_update_selection_mode_word(true);
					last_dblclk = OS::get_singleton()->get_ticks_msec();
					last_dblclk_pos = mb->get_position();
				}
				queue_redraw();
			}

			if (is_middle_mouse_paste_enabled() && mb->get_button_index() == MouseButton::MIDDLE && DisplayServer::get_singleton()->has_feature(DisplayServer::FEATURE_CLIPBOARD_PRIMARY)) {
				apply_ime();
				paste_primary_clipboard();
			}

			if (mb->get_button_index() == MouseButton::RIGHT && (context_menu_enabled || is_move_caret_on_right_click_enabled())) {
				_push_current_op();
				_reset_caret_blink_timer();
				apply_ime();
				_cancel_drag_and_drop_text();

				Point2i pos = get_line_column_at_pos(mpos);
				int mouse_line = pos.y;
				int mouse_column = pos.x;

				if (is_move_caret_on_right_click_enabled()) {
					bool selection_clicked = get_selection_at_line_column(mouse_line, mouse_column, true) >= 0;
					if (!selection_clicked) {
						deselect();
						remove_secondary_carets();
						set_caret_line(mouse_line, false, false, -1);
						set_caret_column(mouse_column);
					}
				}

				if (context_menu_enabled) {
					_update_context_menu();
					menu->set_position(get_screen_position() + mpos);
					menu->reset_size();
					menu->popup();
					grab_focus();
				}
			}
		} else {
			if (has_ime_text()) {
				// Ignore mouse up in IME input mode.
				return;
			}

			if (mb->get_button_index() == MouseButton::LEFT) {
				if (!drag_action && selection_drag_attempt && is_mouse_over_selection()) {
					// This is not a drag and drop attempt, update the caret.
					selection_drag_attempt = false;
					remove_secondary_carets();
					deselect();

					Point2i pos = get_line_column_at_pos(get_local_mouse_pos());
					set_caret_line(pos.y, false, true, -1, 0);
					set_caret_column(pos.x, true, 0);
				}
				dragging_minimap = false;
				dragging_selection = false;
				can_drag_minimap = false;
				click_select_held->stop();
				if (DisplayServer::get_singleton()->has_feature(DisplayServer::FEATURE_CLIPBOARD_PRIMARY)) {
					DisplayServer::get_singleton()->clipboard_set_primary(get_selected_text());
				}
			}

			if (editable) {
				_show_virtual_keyboard();
			}
		}
	}

	const Ref<InputEventPanGesture> pan_gesture = p_gui_input;
	if (pan_gesture.is_valid()) {
		const real_t delta = pan_gesture->get_delta().y;
		if (delta < 0) {
			_scroll_up(-delta, false);
		} else {
			_scroll_down(delta, false);
		}
		h_scroll->set_value(h_scroll->get_value() + pan_gesture->get_delta().x * 100);
		if (v_scroll->get_value() != prev_v_scroll || h_scroll->get_value() != prev_h_scroll) {
			accept_event(); // Accept event if scroll changed.
		}

		return;
	}

	Ref<InputEventMouseMotion> mm = p_gui_input;

	if (mm.is_valid()) {
		Vector2i mpos = mm->get_position();
		if (is_layout_rtl()) {
			mpos.x = get_size().x - mpos.x;
		}

		if (mm->get_button_mask().has_flag(MouseButtonMask::LEFT) && get_viewport()->gui_get_drag_data() == Variant()) {
			// Update if not in drag and drop.
			_reset_caret_blink_timer();

			if (draw_minimap && !dragging_selection) {
				_update_minimap_drag();
			}

			if (!dragging_minimap && !has_ime_text()) {
				switch (selecting_mode) {
					case SelectionMode::SELECTION_MODE_POINTER: {
						_update_selection_mode_pointer();
					} break;
					case SelectionMode::SELECTION_MODE_WORD: {
						_update_selection_mode_word();
					} break;
					case SelectionMode::SELECTION_MODE_LINE: {
						_update_selection_mode_line();
					} break;
					default: {
						break;
					}
				}
			}
		}

		// Check if user is hovering a different gutter, and update if yes.
		Vector2i current_hovered_gutter = Vector2i(-1, -1);

		int left_margin = theme_cache.style_normal->get_margin(SIDE_LEFT);
		if (mpos.x <= left_margin + gutters_width + gutter_padding) {
			int hovered_row = get_line_column_at_pos(mpos).y;
			for (int i = 0; i < gutters.size(); i++) {
				if (!gutters[i].draw || gutters[i].width <= 0) {
					continue;
				}

				if (mpos.x >= left_margin && mpos.x < left_margin + gutters[i].width) {
					// We are in this gutter i's horizontal area.
					current_hovered_gutter = Vector2i(i, hovered_row);
					break;
				}

				left_margin += gutters[i].width;
			}
		}

		if (current_hovered_gutter != hovered_gutter) {
			hovered_gutter = current_hovered_gutter;
			queue_redraw();
		}

		if (drag_action && can_drop_data(mpos, get_viewport()->gui_get_drag_data())) {
			apply_ime();
			// Update drag and drop caret.
			drag_caret_force_displayed = true;
			Point2i pos = get_line_column_at_pos(get_local_mouse_pos());

			if (drag_caret_index == -1) {
				// Force create a new caret for drag and drop.
				carets.push_back(Caret());
				drag_caret_index = carets.size() - 1;
			}

			drag_caret_force_displayed = true;
			set_caret_line(pos.y, false, true, -1, drag_caret_index);
			set_caret_column(pos.x, true, drag_caret_index);
			dragging_selection = true;
		}
	}

	if (draw_minimap && !dragging_selection) {
		_update_minimap_hover();
	}

	if (v_scroll->get_value() != prev_v_scroll || h_scroll->get_value() != prev_h_scroll) {
		accept_event(); // Accept event if scroll changed.
	}

	Ref<InputEventKey> k = p_gui_input;

	if (k.is_valid()) {
		if (alt_input(p_gui_input)) {
			accept_event();
			return;
		}
		if (!k->is_pressed()) {
			return;
		}

		// If a modifier has been pressed, and nothing else, return.
		if (k->get_keycode() == Key::CTRL || k->get_keycode() == Key::ALT || k->get_keycode() == Key::SHIFT || k->get_keycode() == Key::META || k->get_keycode() == Key::CAPSLOCK) {
			return;
		}

		_cancel_drag_and_drop_text();

		_reset_caret_blink_timer();

		// Allow unicode handling if:
		// * No modifiers are pressed (except Shift and CapsLock)
		bool allow_unicode_handling = !(k->is_ctrl_pressed() || k->is_alt_pressed() || k->is_meta_pressed());

		// Check and handle all built-in shortcuts.

		// NEWLINES.
		if (k->is_action("ui_text_newline_above", true)) {
			_new_line(false, true);
			accept_event();
			return;
		}
		if (k->is_action("ui_text_newline_blank", true)) {
			_new_line(false);
			accept_event();
			return;
		}
		if (k->is_action("ui_text_newline", true)) {
			_new_line();
			accept_event();
			return;
		}

		// BACKSPACE AND DELETE.
		if (k->is_action("ui_text_backspace_all_to_left", true)) {
			_do_backspace(false, true);
			accept_event();
			return;
		}
		if (k->is_action("ui_text_backspace_word", true)) {
			_do_backspace(true);
			accept_event();
			return;
		}
		if (k->is_action("ui_text_backspace", true)) {
			_do_backspace();
			accept_event();
			return;
		}
		if (k->is_action("ui_text_delete_all_to_right", true)) {
			_delete(false, true);
			accept_event();
			return;
		}
		if (k->is_action("ui_text_delete_word", true)) {
			_delete(true);
			accept_event();
			return;
		}
		if (k->is_action("ui_text_delete", true)) {
			_delete();
			accept_event();
			return;
		}

		// SCROLLING.
		if (k->is_action("ui_text_scroll_up", true)) {
			_scroll_lines_up();
			accept_event();
			return;
		}
		if (k->is_action("ui_text_scroll_down", true)) {
			_scroll_lines_down();
			accept_event();
			return;
		}

		if (is_shortcut_keys_enabled()) {
			// SELECT ALL, SELECT WORD UNDER CARET, ADD SELECTION FOR NEXT OCCURRENCE, SKIP SELECTION FOR NEXT OCCURRENCE,
			// CLEAR CARETS AND SELECTIONS, CUT, COPY, PASTE.
			if (k->is_action("ui_text_select_all", true)) {
				select_all();
				accept_event();
				return;
			}
			if (k->is_action("ui_text_select_word_under_caret", true)) {
				select_word_under_caret();
				accept_event();
				return;
			}
			if (k->is_action("ui_text_add_selection_for_next_occurrence", true)) {
				add_selection_for_next_occurrence();
				accept_event();
				return;
			}
			if (k->is_action("ui_text_skip_selection_for_next_occurrence", true)) {
				skip_selection_for_next_occurrence();
				accept_event();
				return;
			}
			if (k->is_action("ui_text_clear_carets_and_selection", true)) {
				// Since the default shortcut is ESC, accepts the event only if it's actually performed.
				if (_clear_carets_and_selection()) {
					accept_event();
					return;
				}
			}
			if (k->is_action("ui_cut", true)) {
				cut();
				accept_event();
				return;
			}
			if (k->is_action("ui_copy", true)) {
				copy();
				accept_event();
				return;
			}
			if (k->is_action("ui_paste", true)) {
				paste();
				accept_event();
				return;
			}

			// UNDO/REDO.
			if (k->is_action("ui_undo", true)) {
				undo();
				accept_event();
				return;
			}
			if (k->is_action("ui_redo", true)) {
				redo();
				accept_event();
				return;
			}

			if (k->is_action("ui_text_caret_add_below", true)) {
				add_caret_at_carets(true);
				accept_event();
				return;
			}
			if (k->is_action("ui_text_caret_add_above", true)) {
				add_caret_at_carets(false);
				accept_event();
				return;
			}
		}

		// MISC.
		if (k->is_action("ui_menu", true)) {
			_push_current_op();
			if (context_menu_enabled) {
				_update_context_menu();
				adjust_viewport_to_caret();
				menu->set_position(get_screen_position() + get_caret_draw_pos());
				menu->reset_size();
				menu->popup();
				menu->grab_focus();
			}
			accept_event();
			return;
		}
		if (k->is_action("ui_text_toggle_insert_mode", true)) {
			set_overtype_mode_enabled(!overtype_mode);
			accept_event();
			return;
		}
		if (k->is_action("ui_swap_input_direction", true)) {
			_swap_current_input_direction();
			accept_event();
			return;
		}

		// CARET MOVEMENT

		k = k->duplicate();
		bool shift_pressed = k->is_shift_pressed();
		// Remove shift or else actions will not match. Use above variable for selection.
		k->set_shift_pressed(false);

		// CARET MOVEMENT - LEFT, RIGHT.
		if (k->is_action("ui_text_caret_word_left", true)) {
			_move_caret_left(shift_pressed, true);
			accept_event();
			return;
		}
		if (k->is_action("ui_text_caret_left", true)) {
			_move_caret_left(shift_pressed, false);
			accept_event();
			return;
		}
		if (k->is_action("ui_text_caret_word_right", true)) {
			_move_caret_right(shift_pressed, true);
			accept_event();
			return;
		}
		if (k->is_action("ui_text_caret_right", true)) {
			_move_caret_right(shift_pressed, false);
			accept_event();
			return;
		}

		// CARET MOVEMENT - UP, DOWN.
		if (k->is_action("ui_text_caret_up", true)) {
			_move_caret_up(shift_pressed);
			accept_event();
			return;
		}
		if (k->is_action("ui_text_caret_down", true)) {
			_move_caret_down(shift_pressed);
			accept_event();
			return;
		}

		// CARET MOVEMENT - DOCUMENT START/END.
		if (k->is_action("ui_text_caret_document_start", true)) { // && shift_pressed) {
			_move_caret_document_start(shift_pressed);
			accept_event();
			return;
		}
		if (k->is_action("ui_text_caret_document_end", true)) { // && shift_pressed) {
			_move_caret_document_end(shift_pressed);
			accept_event();
			return;
		}

		// CARET MOVEMENT - LINE START/END.
		if (k->is_action("ui_text_caret_line_start", true)) {
			_move_caret_to_line_start(shift_pressed);
			accept_event();
			return;
		}
		if (k->is_action("ui_text_caret_line_end", true)) {
			_move_caret_to_line_end(shift_pressed);
			accept_event();
			return;
		}

		// CARET MOVEMENT - PAGE UP/DOWN.
		if (k->is_action("ui_text_caret_page_up", true)) {
			_move_caret_page_up(shift_pressed);
			accept_event();
			return;
		}
		if (k->is_action("ui_text_caret_page_down", true)) {
			_move_caret_page_down(shift_pressed);
			accept_event();
			return;
		}

		// Handle tab as it has no set unicode value.
		if (k->is_action("ui_text_indent", true)) {
			if (editable) {
				insert_text_at_caret("\t");
			}
			accept_event();
			return;
		}

		// Handle Unicode (if no modifiers active).
		if (allow_unicode_handling && editable && k->get_unicode() >= 32) {
			handle_unicode_input(k->get_unicode());
			accept_event();
			return;
		}
	}
}

/* Input actions. */
void TextEdit::_swap_current_input_direction() {
	if (input_direction == TEXT_DIRECTION_LTR) {
		input_direction = TEXT_DIRECTION_RTL;
	} else {
		input_direction = TEXT_DIRECTION_LTR;
	}
	for (int i = 0; i < carets.size(); i++) {
		set_caret_column(get_caret_column(i), i == 0, i);
	}
	queue_redraw();
}

void TextEdit::_new_line(bool p_split_current_line, bool p_above) {
	if (!editable) {
		return;
	}

	begin_complex_operation();
	begin_multicaret_edit();

	for (int i = 0; i < get_caret_count(); i++) {
		if (multicaret_edit_ignore_caret(i)) {
			continue;
		}
		if (p_split_current_line) {
			insert_text_at_caret("\n", i);
		} else {
			int line = get_caret_line(i);
			insert_text("\n", line, p_above ? 0 : text[line].length(), p_above, p_above);
			deselect(i);
			set_caret_line(p_above ? line : line + 1, false, true, -1, i);
			set_caret_column(0, i == 0, i);
		}
	}

	end_multicaret_edit();
	end_complex_operation();
}

void TextEdit::_move_caret_left(bool p_select, bool p_move_by_word) {
	_push_current_op();
	for (int i = 0; i < get_caret_count(); i++) {
		// Handle selection.
		if (p_select) {
			_pre_shift_selection(i);
		} else if (has_selection(i) && !p_move_by_word) {
			// If a selection is active, move caret to start of selection.
			set_caret_line(get_selection_from_line(i), false, true, -1, i);
			set_caret_column(get_selection_from_column(i), i == 0, i);
			deselect(i);
			continue;
		} else {
			deselect(i);
		}

		if (p_move_by_word) {
			int cc = get_caret_column(i);
			// If the caret is at the start of the line, and not on the first line, move it up to the end of the previous line.
			if (cc == 0 && get_caret_line(i) > 0) {
				set_caret_line(get_caret_line(i) - 1, false, true, -1, i);
				set_caret_column(text[get_caret_line(i)].length(), i == 0, i);
			} else {
				PackedInt32Array words = TS->shaped_text_get_word_breaks(text.get_line_data(get_caret_line(i))->get_rid());
				if (words.is_empty() || cc <= words[0]) {
					// Move to the start when there are no more words.
					cc = 0;
				} else {
					for (int j = words.size() - 2; j >= 0; j = j - 2) {
						if (words[j] < cc) {
							cc = words[j];
							break;
						}
					}
				}
				set_caret_column(cc, i == 0, i);
			}
		} else {
			// If the caret is at the start of the line, and not on the first line, move it up to the end of the previous line.
			if (get_caret_column(i) == 0) {
				if (get_caret_line(i) > 0) {
					int new_caret_line = get_caret_line(i) - get_next_visible_line_offset_from(CLAMP(get_caret_line(i) - 1, 0, text.size() - 1), -1);
					set_caret_line(new_caret_line, false, true, -1, i);
					set_caret_column(text[get_caret_line(i)].length(), i == 0, i);
				}
			} else {
				if (caret_mid_grapheme_enabled) {
					set_caret_column(get_caret_column(i) - 1, i == 0, i);
				} else {
					set_caret_column(TS->shaped_text_prev_character_pos(text.get_line_data(get_caret_line(i))->get_rid(), get_caret_column(i)), i == 0, i);
				}
			}
		}
	}
	merge_overlapping_carets();
}

void TextEdit::_move_caret_right(bool p_select, bool p_move_by_word) {
	_push_current_op();
	for (int i = 0; i < get_caret_count(); i++) {
		// Handle selection.
		if (p_select) {
			_pre_shift_selection(i);
		} else if (has_selection(i) && !p_move_by_word) {
			// If a selection is active, move caret to end of selection.
			set_caret_line(get_selection_to_line(i), false, true, -1, i);
			set_caret_column(get_selection_to_column(i), i == 0, i);
			deselect(i);
			continue;
		} else {
			deselect(i);
		}

		if (p_move_by_word) {
			int cc = get_caret_column(i);
			// If the caret is at the end of the line, and not on the last line, move it down to the beginning of the next line.
			if (cc == text[get_caret_line(i)].length() && get_caret_line(i) < text.size() - 1) {
				set_caret_line(get_caret_line(i) + 1, false, true, -1, i);
				set_caret_column(0, i == 0, i);
			} else {
				PackedInt32Array words = TS->shaped_text_get_word_breaks(text.get_line_data(get_caret_line(i))->get_rid());
				if (words.is_empty() || cc >= words[words.size() - 1]) {
					// Move to the end when there are no more words.
					cc = text[get_caret_line(i)].length();
				} else {
					for (int j = 1; j < words.size(); j = j + 2) {
						if (words[j] > cc) {
							cc = words[j];
							break;
						}
					}
				}
				set_caret_column(cc, i == 0, i);
			}
		} else {
			// If we are at the end of the line, move the caret to the next line down.
			if (get_caret_column(i) == text[get_caret_line(i)].length()) {
				if (get_caret_line(i) < text.size() - 1) {
					int new_caret_line = get_caret_line(i) + get_next_visible_line_offset_from(CLAMP(get_caret_line(i) + 1, 0, text.size() - 1), 1);
					set_caret_line(new_caret_line, false, false, -1, i);
					set_caret_column(0, i == 0, i);
				}
			} else {
				if (caret_mid_grapheme_enabled) {
					set_caret_column(get_caret_column(i) + 1, i == 0, i);
				} else {
					set_caret_column(TS->shaped_text_next_character_pos(text.get_line_data(get_caret_line(i))->get_rid(), get_caret_column(i)), i == 0, i);
				}
			}
		}
	}
	merge_overlapping_carets();
}

void TextEdit::_move_caret_up(bool p_select) {
	_push_current_op();
	for (int i = 0; i < get_caret_count(); i++) {
		if (p_select) {
			_pre_shift_selection(i);
		} else {
			deselect(i);
		}

		int cur_wrap_index = get_caret_wrap_index(i);
		if (cur_wrap_index > 0) {
			set_caret_line(get_caret_line(i), true, false, cur_wrap_index - 1, i);
		} else if (get_caret_line(i) == 0) {
			set_caret_column(0, i == 0, i);
		} else {
			int new_line = get_caret_line(i) - get_next_visible_line_offset_from(get_caret_line(i) - 1, -1);
			if (is_line_wrapped(new_line)) {
				set_caret_line(new_line, i == 0, false, get_line_wrap_count(new_line), i);
			} else {
				set_caret_line(new_line, i == 0, false, 0, i);
			}
		}
	}
	merge_overlapping_carets();
}

void TextEdit::_move_caret_down(bool p_select) {
	_push_current_op();
	for (int i = 0; i < get_caret_count(); i++) {
		if (p_select) {
			_pre_shift_selection(i);
		} else {
			deselect(i);
		}

		int cur_wrap_index = get_caret_wrap_index(i);
		if (cur_wrap_index < get_line_wrap_count(get_caret_line(i))) {
			set_caret_line(get_caret_line(i), i == 0, false, cur_wrap_index + 1, i);
		} else if (get_caret_line(i) == get_last_unhidden_line()) {
			set_caret_column(text[get_caret_line(i)].length());
		} else {
			int new_line = get_caret_line(i) + get_next_visible_line_offset_from(CLAMP(get_caret_line(i) + 1, 0, text.size() - 1), 1);
			set_caret_line(new_line, i == 0, false, 0, i);
		}
	}
	merge_overlapping_carets();
}

void TextEdit::_move_caret_to_line_start(bool p_select) {
	_push_current_op();
	for (int i = 0; i < get_caret_count(); i++) {
		if (p_select) {
			_pre_shift_selection(i);
		} else {
			deselect(i);
		}

		// Move caret column to start of wrapped row and then to start of text.
		Vector<String> rows = get_line_wrapped_text(get_caret_line(i));
		int wi = get_caret_wrap_index(i);
		int row_start_col = 0;
		for (int j = 0; j < wi; j++) {
			row_start_col += rows[j].length();
		}
		if (get_caret_column(i) == row_start_col || wi == 0) {
			// Compute whitespace symbols sequence length.
			int current_line_whitespace_len = get_first_non_whitespace_column(get_caret_line(i));
			if (get_caret_column(i) == current_line_whitespace_len) {
				set_caret_column(0, i == 0, i);
			} else {
				set_caret_column(current_line_whitespace_len, i == 0, i);
			}
		} else {
			set_caret_column(row_start_col, i == 0, i);
		}
	}
	merge_overlapping_carets();
}

void TextEdit::_move_caret_to_line_end(bool p_select) {
	_push_current_op();
	for (int i = 0; i < get_caret_count(); i++) {
		if (p_select) {
			_pre_shift_selection(i);
		} else {
			deselect(i);
		}

		// Move caret column to end of wrapped row and then to end of text.
		Vector<String> rows = get_line_wrapped_text(get_caret_line(i));
		int wi = get_caret_wrap_index(i);
		int row_end_col = -1;
		for (int j = 0; j < wi + 1; j++) {
			row_end_col += rows[j].length();
		}
		if (wi == rows.size() - 1 || get_caret_column(i) == row_end_col) {
			set_caret_column(text[get_caret_line(i)].length(), i == 0, i);
		} else {
			set_caret_column(row_end_col, i == 0, i);
		}
	}
	merge_overlapping_carets();
}

void TextEdit::_move_caret_page_up(bool p_select) {
	_push_current_op();
	for (int i = 0; i < get_caret_count(); i++) {
		if (p_select) {
			_pre_shift_selection(i);
		} else {
			deselect(i);
		}

		Point2i next_line = get_next_visible_line_index_offset_from(get_caret_line(i), get_caret_wrap_index(i), -get_visible_line_count());
		int n_line = get_caret_line(i) - next_line.x + 1;
		set_caret_line(n_line, i == 0, false, next_line.y, i);
	}
	merge_overlapping_carets();
}

void TextEdit::_move_caret_page_down(bool p_select) {
	_push_current_op();
	for (int i = 0; i < get_caret_count(); i++) {
		if (p_select) {
			_pre_shift_selection(i);
		} else {
			deselect(i);
		}

		Point2i next_line = get_next_visible_line_index_offset_from(get_caret_line(i), get_caret_wrap_index(i), get_visible_line_count());
		int n_line = get_caret_line(i) + next_line.x - 1;
		set_caret_line(n_line, i == 0, false, next_line.y, i);
	}
	merge_overlapping_carets();
}

void TextEdit::_do_backspace(bool p_word, bool p_all_to_left) {
	if (!editable) {
		return;
	}

	start_action(EditAction::ACTION_BACKSPACE);
	begin_multicaret_edit();

	Vector<int> sorted_carets = get_sorted_carets();
	sorted_carets.reverse();
	for (int i = 0; i < sorted_carets.size(); i++) {
		int caret_index = sorted_carets[i];
		if (multicaret_edit_ignore_caret(caret_index)) {
			continue;
		}

		if (get_caret_column(caret_index) == 0 && get_caret_line(caret_index) == 0 && !has_selection(caret_index)) {
			continue;
		}

		if (has_selection(caret_index) || (!p_all_to_left && !p_word) || get_caret_column(caret_index) == 0) {
			backspace(caret_index);
			continue;
		}

		if (p_all_to_left) {
			// Remove everything to left of caret to the start of the line.
			int caret_current_column = get_caret_column(caret_index);
			_remove_text(get_caret_line(caret_index), 0, get_caret_line(caret_index), caret_current_column);
			collapse_carets(get_caret_line(caret_index), 0, get_caret_line(caret_index), caret_current_column);
			set_caret_column(0, caret_index == 0, caret_index);
			_offset_carets_after(get_caret_line(caret_index), caret_current_column, get_caret_line(caret_index), 0);
			continue;
		}

		if (p_word) {
			// Remove text to the start of the word left of the caret.
			int from_column = get_caret_column(caret_index);
			int column = get_caret_column(caret_index);
			// Check for the case "<word><space><caret>" and ignore the space.
			// No need to check for column being 0 since it is checked above.
			if (is_whitespace(text[get_caret_line(caret_index)][get_caret_column(caret_index) - 1])) {
				column -= 1;
			}

			// Get a list with the indices of the word bounds of the given text line.
			const PackedInt32Array words = TS->shaped_text_get_word_breaks(text.get_line_data(get_caret_line(caret_index))->get_rid());
			if (words.is_empty() || column <= words[0]) {
				// Delete to the start when there are no more words.
				column = 0;
			} else {
				// Otherwise search for the first word break that is smaller than the index from we're currently deleting.
				for (int c = words.size() - 2; c >= 0; c = c - 2) {
					if (words[c] < column) {
						column = words[c];
						break;
					}
				}
			}

			_remove_text(get_caret_line(caret_index), column, get_caret_line(caret_index), from_column);
			collapse_carets(get_caret_line(caret_index), column, get_caret_line(caret_index), from_column);
			set_caret_column(column, caret_index == 0, caret_index);
			_offset_carets_after(get_caret_line(caret_index), from_column, get_caret_line(caret_index), column);
		}
	}

	end_multicaret_edit();
	end_action();
}

void TextEdit::_delete(bool p_word, bool p_all_to_right) {
	if (!editable) {
		return;
	}

	start_action(EditAction::ACTION_DELETE);
	begin_multicaret_edit();

	Vector<int> sorted_carets = get_sorted_carets();
	for (int i = 0; i < sorted_carets.size(); i++) {
		int caret_index = sorted_carets[i];
		if (multicaret_edit_ignore_caret(caret_index)) {
			continue;
		}

		if (has_selection(caret_index)) {
			delete_selection(caret_index);
			continue;
		}

		int curline_len = text[get_caret_line(caret_index)].length();
		if (get_caret_line(caret_index) == text.size() - 1 && get_caret_column(caret_index) == curline_len) {
			continue; // Last line, last column: Nothing to do.
		}

		int next_line = get_caret_column(caret_index) < curline_len ? get_caret_line(caret_index) : get_caret_line(caret_index) + 1;
		int next_column;

		if (p_all_to_right) {
			if (get_caret_column(caret_index) == curline_len) {
				continue;
			}

			// Delete everything to right of caret.
			next_column = curline_len;
			next_line = get_caret_line(caret_index);
		} else if (p_word && get_caret_column(caret_index) < curline_len - 1) {
			// Delete next word to right of caret.
			int line = get_caret_line(caret_index);
			int column = get_caret_column(caret_index);

			PackedInt32Array words = TS->shaped_text_get_word_breaks(text.get_line_data(line)->get_rid());
			if (words.is_empty() || column >= words[words.size() - 1]) {
				// Delete to the end when there are no more words.
				column = text[get_caret_line(i)].length();
			} else {
				for (int j = 1; j < words.size(); j = j + 2) {
					if (words[j] > column) {
						column = words[j];
						break;
					}
				}
			}

			next_line = line;
			next_column = column;
		} else {
			// Delete one character.
			if (caret_mid_grapheme_enabled) {
				next_column = get_caret_column(caret_index) < curline_len ? (get_caret_column(caret_index) + 1) : 0;
			} else {
				next_column = get_caret_column(caret_index) < curline_len ? TS->shaped_text_next_character_pos(text.get_line_data(get_caret_line(caret_index))->get_rid(), (get_caret_column(caret_index))) : 0;
			}
		}

		_remove_text(get_caret_line(caret_index), get_caret_column(caret_index), next_line, next_column);
		collapse_carets(get_caret_line(caret_index), get_caret_column(caret_index), next_line, next_column);
		_offset_carets_after(next_line, next_column, get_caret_line(caret_index), get_caret_column(caret_index));
	}

	end_multicaret_edit();
	end_action();
}

void TextEdit::_move_caret_document_start(bool p_select) {
	remove_secondary_carets();
	if (p_select) {
		_pre_shift_selection(0);
	} else {
		deselect();
	}

	set_caret_line(0, false, true, -1);
	set_caret_column(0);
}

void TextEdit::_move_caret_document_end(bool p_select) {
	remove_secondary_carets();
	if (p_select) {
		_pre_shift_selection(0);
	} else {
		deselect();
	}

	set_caret_line(get_last_unhidden_line(), true, false, -1);
	set_caret_column(text[get_caret_line()].length());
}

bool TextEdit::_clear_carets_and_selection() {
	_push_current_op();
	if (get_caret_count() > 1) {
		remove_secondary_carets();
		return true;
	}

	if (has_selection()) {
		deselect();
		return true;
	}

	return false;
}

void TextEdit::_update_placeholder() {
	if (theme_cache.font.is_null() || theme_cache.font_size <= 0) {
		return; // Not in tree?
	}

	const String placeholder_translated = atr(placeholder_text);

	// Placeholder is generally smaller then text documents, and updates less so this should be fast enough for now.
	placeholder_data_buf->clear();
	placeholder_data_buf->set_width(text.get_width());
	BitField<TextServer::LineBreakFlag> flags = text.get_brk_flags();
	if (text.is_indent_wrapped_lines()) {
		flags.set_flag(TextServer::BREAK_TRIM_INDENT);
	}
	placeholder_data_buf->set_break_flags(flags);
	if (text_direction == Control::TEXT_DIRECTION_INHERITED) {
		placeholder_data_buf->set_direction(is_layout_rtl() ? TextServer::DIRECTION_RTL : TextServer::DIRECTION_LTR);
	} else {
		placeholder_data_buf->set_direction((TextServer::Direction)text_direction);
	}
	placeholder_data_buf->set_preserve_control(draw_control_chars);
	placeholder_data_buf->add_string(placeholder_translated, theme_cache.font, theme_cache.font_size, language);

	placeholder_bidi_override = structured_text_parser(st_parser, st_args, placeholder_translated);
	if (placeholder_bidi_override.is_empty()) {
		TS->shaped_text_set_bidi_override(placeholder_data_buf->get_rid(), placeholder_bidi_override);
	}

	if (get_tab_size() > 0) {
		Vector<float> tabs;
		tabs.push_back(theme_cache.font->get_char_size(' ', theme_cache.font_size).width * get_tab_size());
		placeholder_data_buf->tab_align(tabs);
	}

	// Update height.
	const int wrap_amount = placeholder_data_buf->get_line_count() - 1;
	placeholder_line_height = theme_cache.font->get_height(theme_cache.font_size);
	for (int i = 0; i <= wrap_amount; i++) {
		placeholder_line_height = MAX(placeholder_line_height, placeholder_data_buf->get_line_size(i).y);
	}

	// Update width.
	placeholder_max_width = placeholder_data_buf->get_size().x;

	// Update wrapped rows.
	placeholder_wraped_rows.clear();
	for (int i = 0; i <= wrap_amount; i++) {
		Vector2i line_range = placeholder_data_buf->get_line_range(i);
		placeholder_wraped_rows.push_back(placeholder_translated.substr(line_range.x, line_range.y - line_range.x));
	}
}

bool TextEdit::_using_placeholder() const {
	return text.size() == 1 && text[0].is_empty() && ime_text.is_empty();
}

void TextEdit::_update_theme_item_cache() {
	Control::_update_theme_item_cache();

	theme_cache.base_scale = get_theme_default_base_scale();
	use_selected_font_color = theme_cache.font_selected_color != Color(0, 0, 0, 0);

	if (text.get_line_height() + theme_cache.line_spacing < 1) {
		WARN_PRINT("Line height is too small, please increase font_size and/or line_spacing");
	}
}

void TextEdit::_update_caches() {
	/* Text properties. */
	TextServer::Direction dir;
	if (text_direction == Control::TEXT_DIRECTION_INHERITED) {
		dir = is_layout_rtl() ? TextServer::DIRECTION_RTL : TextServer::DIRECTION_LTR;
	} else {
		dir = (TextServer::Direction)text_direction;
	}
	text.set_direction_and_language(dir, (!language.is_empty()) ? language : TranslationServer::get_singleton()->get_tool_locale());
	text.set_draw_control_chars(draw_control_chars);
	text.set_font(theme_cache.font);
	text.set_font_size(theme_cache.font_size);
	text.invalidate_font();
	_update_placeholder();

	/* Syntax highlighting. */
	if (syntax_highlighter.is_valid()) {
		syntax_highlighter->set_text_edit(this);
	}
	_clear_syntax_highlighting_cache();
}

void TextEdit::_close_ime_window() {
	DisplayServer::WindowID wid = get_window() ? get_window()->get_window_id() : DisplayServer::INVALID_WINDOW_ID;
	if (wid == DisplayServer::INVALID_WINDOW_ID || !DisplayServer::get_singleton()->has_feature(DisplayServer::FEATURE_IME)) {
		return;
	}
	DisplayServer::get_singleton()->window_set_ime_position(Point2(), wid);
	DisplayServer::get_singleton()->window_set_ime_active(false, wid);
}

void TextEdit::_update_ime_window_position() {
	DisplayServer::WindowID wid = get_window() ? get_window()->get_window_id() : DisplayServer::INVALID_WINDOW_ID;
	if (wid == DisplayServer::INVALID_WINDOW_ID || !DisplayServer::get_singleton()->has_feature(DisplayServer::FEATURE_IME)) {
		return;
	}
	DisplayServer::get_singleton()->window_set_ime_active(true, wid);
	Point2 pos = get_global_position() + get_caret_draw_pos();
	if (get_window()->get_embedder()) {
		pos += get_viewport()->get_popup_base_transform().get_origin();
	}
	// The window will move to the updated position the next time the IME is updated, not immediately.
	DisplayServer::get_singleton()->window_set_ime_position(pos, wid);
}

void TextEdit::_update_ime_text() {
	if (has_ime_text()) {
		// Update text to visually include IME text.
		for (int i = 0; i < get_caret_count(); i++) {
			String text_with_ime = text[get_caret_line(i)].substr(0, get_caret_column(i)) + ime_text + text[get_caret_line(i)].substr(get_caret_column(i), text[get_caret_line(i)].length());
			text.invalidate_cache(get_caret_line(i), get_caret_column(i), true, text_with_ime, structured_text_parser(st_parser, st_args, text_with_ime));
		}
	} else {
		// Reset text.
		for (int i = 0; i < get_caret_count(); i++) {
			text.invalidate_cache(get_caret_line(i), get_caret_column(i), true);
		}
	}
	queue_redraw();
}

void TextEdit::_show_virtual_keyboard() {
	_update_ime_window_position();

	if (virtual_keyboard_enabled && DisplayServer::get_singleton()->has_feature(DisplayServer::FEATURE_VIRTUAL_KEYBOARD)) {
		int caret_start = -1;
		int caret_end = -1;

		if (!has_selection(0)) {
			String full_text = _base_get_text(0, 0, get_caret_line(), get_caret_column());

			caret_start = full_text.length();
		} else {
			String pre_text = _base_get_text(0, 0, get_selection_from_line(), get_selection_from_column());
			String post_text = get_selected_text(0);

			caret_start = pre_text.length();
			caret_end = caret_start + post_text.length();
		}

		DisplayServer::get_singleton()->virtual_keyboard_show(get_text(), get_global_rect(), DisplayServer::KEYBOARD_TYPE_MULTILINE, -1, caret_start, caret_end);
	}
}

/* General overrides. */
Size2 TextEdit::get_minimum_size() const {
	Size2 size = theme_cache.style_normal->get_minimum_size();
	if (fit_content_height) {
		size.y += content_size_cache.y;
	}
	if (fit_content_width) {
		size.x += content_size_cache.x;
	}
	return size;
}

bool TextEdit::is_text_field() const {
	return true;
}

Variant TextEdit::get_drag_data(const Point2 &p_point) {
	Variant ret = Control::get_drag_data(p_point);
	if (ret != Variant()) {
		return ret;
	}

	if (has_selection() && selection_drag_attempt) {
		String t = get_selected_text();
		Label *l = memnew(Label);
		l->set_auto_translate_mode(AUTO_TRANSLATE_MODE_DISABLED); // Don't translate user input.
		l->set_text(t);
		set_drag_preview(l);
		return t;
	}

	return Variant();
}

bool TextEdit::can_drop_data(const Point2 &p_point, const Variant &p_data) const {
	bool drop_override = Control::can_drop_data(p_point, p_data); // In case user wants to drop custom data.
	if (drop_override) {
		return drop_override;
	}

	return is_editable() && p_data.is_string();
}

void TextEdit::drop_data(const Point2 &p_point, const Variant &p_data) {
	Control::drop_data(p_point, p_data);

	if (p_data.is_string() && is_editable()) {
		Point2i pos = get_line_column_at_pos(get_local_mouse_pos());
		int drop_at_line = pos.y;
		int drop_at_column = pos.x;
		int selection_index = get_selection_at_line_column(drop_at_line, drop_at_column, !Input::get_singleton()->is_key_pressed(Key::CMD_OR_CTRL));

		// Remove drag caret before the complex operation starts so it won't appear in undo.
		remove_caret(drag_caret_index);

		if (selection_drag_attempt && selection_index >= 0 && selection_index == drag_and_drop_origin_caret_index) {
			// Dropped onto original selection, do nothing.
			selection_drag_attempt = false;
			return;
		}

		begin_complex_operation();
		begin_multicaret_edit();
		if (selection_drag_attempt) {
			// Drop from self.
			selection_drag_attempt = false;
			if (!Input::get_singleton()->is_key_pressed(Key::CMD_OR_CTRL)) {
				// Delete all selections.
				int temp_caret = add_caret(drop_at_line, drop_at_column);

				delete_selection();

				// Use a temporary caret to update the drop at position.
				drop_at_line = get_caret_line(temp_caret);
				drop_at_column = get_caret_column(temp_caret);
			}
		}
		remove_secondary_carets();
		deselect();

		// Insert the dragged text.
		set_caret_line(drop_at_line, true, false, -1);
		set_caret_column(drop_at_column);
		insert_text_at_caret(p_data);

		select(drop_at_line, drop_at_column, get_caret_line(), get_caret_column());
		grab_focus();
		adjust_viewport_to_caret();
		end_multicaret_edit();
		end_complex_operation();
	}
}

Control::CursorShape TextEdit::get_cursor_shape(const Point2 &p_pos) const {
	Point2i pos = get_line_column_at_pos(p_pos);
	int row = pos.y;

	int left_margin = theme_cache.style_normal->get_margin(SIDE_LEFT);
	int gutter = left_margin + gutters_width;
	if (p_pos.x < gutter) {
		for (int i = 0; i < gutters.size(); i++) {
			if (!gutters[i].draw) {
				continue;
			}

			if (p_pos.x >= left_margin && p_pos.x < left_margin + gutters[i].width) {
				if (gutters[i].clickable || is_line_gutter_clickable(row, i)) {
					return CURSOR_POINTING_HAND;
				}
			}
			left_margin += gutters[i].width;
		}
		return CURSOR_ARROW;
	}

	int xmargin_end = get_size().width - theme_cache.style_normal->get_margin(SIDE_RIGHT);
	if (draw_minimap && p_pos.x > xmargin_end - minimap_width && p_pos.x <= xmargin_end) {
		return CURSOR_ARROW;
	}
	return get_default_cursor_shape();
}

String TextEdit::get_tooltip(const Point2 &p_pos) const {
	if (!tooltip_callback.is_valid()) {
		return Control::get_tooltip(p_pos);
	}
	Point2i pos = get_line_column_at_pos(p_pos);
	int row = pos.y;
	int col = pos.x;

	String s = text[row];
	if (s.length() == 0) {
		return Control::get_tooltip(p_pos);
	}
	int beg, end;
	if (select_word(s, col, beg, end)) {
		Variant args[1] = { s.substr(beg, end - beg) };
		const Variant *argp[] = { &args[0] };
		Callable::CallError ce;
		Variant ret;
		tooltip_callback.callp(argp, 1, ret, ce);
		ERR_FAIL_COND_V_MSG(ce.error != Callable::CallError::CALL_OK, "", "Failed to call custom tooltip.");
		return ret;
	}

	return Control::get_tooltip(p_pos);
}

void TextEdit::set_tooltip_request_func(const Callable &p_tooltip_callback) {
	tooltip_callback = p_tooltip_callback;
}

/* Text */
// Text properties.
bool TextEdit::has_ime_text() const {
	return !ime_text.is_empty();
}

void TextEdit::cancel_ime() {
	if (!has_ime_text()) {
		_close_ime_window();
		return;
	}
	ime_text = String();
	ime_selection = Vector2i();
	alt_start = false;
	alt_start_no_hold = false;
	_close_ime_window();
	_update_ime_text();
}

void TextEdit::apply_ime() {
	if (!has_ime_text()) {
		_close_ime_window();
		return;
	}

	// Force apply the current IME text.
	if (alt_start || alt_start_no_hold) {
		cancel_ime();
		if ((alt_code > 0x31 && alt_code < 0xd800) || (alt_code > 0xdfff && alt_code <= 0x10ffff)) {
			handle_unicode_input(alt_code);
		}
	} else {
		String insert_ime_text = ime_text;
		cancel_ime();
		insert_text_at_caret(insert_ime_text);
	}
}

void TextEdit::set_editable(bool p_editable) {
	if (editable == p_editable) {
		return;
	}

	editable = p_editable;

	queue_redraw();
}

bool TextEdit::is_editable() const {
	return editable;
}

void TextEdit::set_text_direction(Control::TextDirection p_text_direction) {
	ERR_FAIL_COND((int)p_text_direction < -1 || (int)p_text_direction > 3);
	if (text_direction != p_text_direction) {
		text_direction = p_text_direction;
		if (text_direction != TEXT_DIRECTION_AUTO && text_direction != TEXT_DIRECTION_INHERITED) {
			input_direction = text_direction;
		}
		TextServer::Direction dir;
		if (text_direction == Control::TEXT_DIRECTION_INHERITED) {
			dir = is_layout_rtl() ? TextServer::DIRECTION_RTL : TextServer::DIRECTION_LTR;
		} else {
			dir = (TextServer::Direction)text_direction;
		}
		text.set_direction_and_language(dir, (!language.is_empty()) ? language : TranslationServer::get_singleton()->get_tool_locale());
		text.invalidate_font();
		_update_placeholder();

		if (menu_dir) {
			menu_dir->set_item_checked(menu_dir->get_item_index(MENU_DIR_INHERITED), text_direction == TEXT_DIRECTION_INHERITED);
			menu_dir->set_item_checked(menu_dir->get_item_index(MENU_DIR_AUTO), text_direction == TEXT_DIRECTION_AUTO);
			menu_dir->set_item_checked(menu_dir->get_item_index(MENU_DIR_LTR), text_direction == TEXT_DIRECTION_LTR);
			menu_dir->set_item_checked(menu_dir->get_item_index(MENU_DIR_RTL), text_direction == TEXT_DIRECTION_RTL);
		}
		queue_redraw();
	}
}

Control::TextDirection TextEdit::get_text_direction() const {
	return text_direction;
}

void TextEdit::set_language(const String &p_language) {
	if (language != p_language) {
		language = p_language;
		TextServer::Direction dir;
		if (text_direction == Control::TEXT_DIRECTION_INHERITED) {
			dir = is_layout_rtl() ? TextServer::DIRECTION_RTL : TextServer::DIRECTION_LTR;
		} else {
			dir = (TextServer::Direction)text_direction;
		}
		text.set_direction_and_language(dir, (!language.is_empty()) ? language : TranslationServer::get_singleton()->get_tool_locale());
		text.invalidate_all();
		_update_placeholder();
		queue_redraw();
	}
}

String TextEdit::get_language() const {
	return language;
}

void TextEdit::set_structured_text_bidi_override(TextServer::StructuredTextParser p_parser) {
	if (st_parser != p_parser) {
		st_parser = p_parser;
		for (int i = 0; i < text.size(); i++) {
			text.set(i, text[i], structured_text_parser(st_parser, st_args, text[i]));
		}
		queue_redraw();
	}
}

TextServer::StructuredTextParser TextEdit::get_structured_text_bidi_override() const {
	return st_parser;
}

void TextEdit::set_structured_text_bidi_override_options(Array p_args) {
	if (st_args == p_args) {
		return;
	}

	st_args = p_args;
	for (int i = 0; i < text.size(); i++) {
		text.set(i, text[i], structured_text_parser(st_parser, st_args, text[i]));
	}
	queue_redraw();
}

Array TextEdit::get_structured_text_bidi_override_options() const {
	return st_args;
}

void TextEdit::set_tab_size(const int p_size) {
	ERR_FAIL_COND_MSG(p_size <= 0, "Tab size must be greater than 0.");
	if (p_size == text.get_tab_size()) {
		return;
	}
	text.set_tab_size(p_size);
	text.invalidate_all_lines();
	_update_placeholder();
	queue_redraw();
}

int TextEdit::get_tab_size() const {
	return text.get_tab_size();
}

void TextEdit::set_indent_wrapped_lines(bool p_enabled) {
	if (text.is_indent_wrapped_lines() == p_enabled) {
		return;
	}
	text.set_indent_wrapped_lines(p_enabled);
	text.invalidate_all_lines();
	_update_placeholder();
	queue_redraw();
}

bool TextEdit::is_indent_wrapped_lines() const {
	return text.is_indent_wrapped_lines();
}

// User controls
void TextEdit::set_overtype_mode_enabled(bool p_enabled) {
	if (overtype_mode == p_enabled) {
		return;
	}

	overtype_mode = p_enabled;
	queue_redraw();
}

bool TextEdit::is_overtype_mode_enabled() const {
	return overtype_mode;
}

void TextEdit::set_context_menu_enabled(bool p_enabled) {
	context_menu_enabled = p_enabled;
}

bool TextEdit::is_context_menu_enabled() const {
	return context_menu_enabled;
}

void TextEdit::set_shortcut_keys_enabled(bool p_enabled) {
	shortcut_keys_enabled = p_enabled;
}

bool TextEdit::is_shortcut_keys_enabled() const {
	return shortcut_keys_enabled;
}

void TextEdit::set_virtual_keyboard_enabled(bool p_enabled) {
	virtual_keyboard_enabled = p_enabled;
}

bool TextEdit::is_virtual_keyboard_enabled() const {
	return virtual_keyboard_enabled;
}

void TextEdit::set_middle_mouse_paste_enabled(bool p_enabled) {
	middle_mouse_paste_enabled = p_enabled;
}

bool TextEdit::is_middle_mouse_paste_enabled() const {
	return middle_mouse_paste_enabled;
}

void TextEdit::set_empty_selection_clipboard_enabled(bool p_enabled) {
	empty_selection_clipboard_enabled = p_enabled;
}

bool TextEdit::is_empty_selection_clipboard_enabled() const {
	return empty_selection_clipboard_enabled;
}

// Text manipulation
void TextEdit::clear() {
	setting_text = true;
	_clear();
	setting_text = false;
	emit_signal(SNAME("text_set"));
}

void TextEdit::_clear() {
	if (editable && undo_enabled) {
		remove_secondary_carets();
		_move_caret_document_start(false);
		begin_complex_operation();

		_remove_text(0, 0, MAX(0, get_line_count() - 1), MAX(get_line(MAX(get_line_count() - 1, 0)).size() - 1, 0));
		insert_text_at_caret("");
		text.clear();

		end_complex_operation();
		return;
	}
	// Cannot merge with above, as we are not part of the tree on creation.
	int old_text_size = text.size();

	clear_undo_history();
	text.clear();
	remove_secondary_carets();
	set_caret_line(0, false, true, -1);
	set_caret_column(0);
	first_visible_col = 0;
	first_visible_line = 0;
	first_visible_line_wrap_ofs = 0;
	carets.write[0].last_fit_x = 0;
	deselect();

	emit_signal(SNAME("lines_edited_from"), old_text_size, 0);
}

void TextEdit::set_text(const String &p_text) {
	setting_text = true;
	if (!undo_enabled) {
		_clear();
		insert_text_at_caret(p_text);
	}

	if (undo_enabled) {
		remove_secondary_carets();
		set_caret_line(0);
		set_caret_column(0);

		begin_complex_operation();
		deselect();
		_remove_text(0, 0, MAX(0, get_line_count() - 1), MAX(get_line(MAX(get_line_count() - 1, 0)).size() - 1, 0));
		insert_text_at_caret(p_text);
		end_complex_operation();
	}

	set_caret_line(0);
	set_caret_column(0);

	queue_redraw();
	setting_text = false;
	emit_signal(SNAME("text_set"));
}

String TextEdit::get_text() const {
	StringBuilder ret_text;
	const int text_size = text.size();
	for (int i = 0; i < text_size; i++) {
		ret_text += text[i];
		if (i != text_size - 1) {
			ret_text += "\n";
		}
	}
	return ret_text.as_string();
}

int TextEdit::get_line_count() const {
	return text.size();
}

void TextEdit::set_placeholder(const String &p_text) {
	if (placeholder_text == p_text) {
		return;
	}

	placeholder_text = p_text;
	_update_placeholder();
	queue_redraw();
}

String TextEdit::get_placeholder() const {
	return placeholder_text;
}

void TextEdit::set_line(int p_line, const String &p_new_text) {
	if (p_line < 0 || p_line >= text.size()) {
		return;
	}
	begin_complex_operation();

	int old_column = text[p_line].length();

	// Set the affected carets column to update their last offset x.
	for (int i = 0; i < get_caret_count(); i++) {
		if (_is_line_col_in_range(get_caret_line(i), get_caret_column(i), p_line, 0, p_line, old_column)) {
			set_caret_column(get_caret_column(i), false, i);
		}
		if (has_selection(i) && _is_line_col_in_range(get_selection_origin_line(i), get_selection_origin_column(i), p_line, 0, p_line, old_column)) {
			set_selection_origin_column(get_selection_origin_column(i), i);
		}
	}

	_remove_text(p_line, 0, p_line, old_column);
	int new_line, new_column;
	_insert_text(p_line, 0, p_new_text, &new_line, &new_column);

	// Don't offset carets that were on the old line.
	_offset_carets_after(p_line, old_column, new_line, new_column, false, false);

	// Set the caret lines to update the column to match visually.
	for (int i = 0; i < get_caret_count(); i++) {
		if (_is_line_col_in_range(get_caret_line(i), get_caret_column(i), p_line, 0, p_line, old_column)) {
			set_caret_line(get_caret_line(i), false, true, 0, i);
		}
		if (has_selection(i) && _is_line_col_in_range(get_selection_origin_line(i), get_selection_origin_column(i), p_line, 0, p_line, old_column)) {
			set_selection_origin_line(get_selection_origin_line(i), true, 0, i);
		}
	}
	merge_overlapping_carets();
	end_complex_operation();
}

String TextEdit::get_line(int p_line) const {
	if (p_line < 0 || p_line >= text.size()) {
		return "";
	}
	return text[p_line];
}

int TextEdit::get_line_width(int p_line, int p_wrap_index) const {
	ERR_FAIL_INDEX_V(p_line, text.size(), 0);
	ERR_FAIL_COND_V(p_wrap_index > get_line_wrap_count(p_line), 0);

	return text.get_line_width(p_line, p_wrap_index);
}

int TextEdit::get_line_height() const {
	return MAX(text.get_line_height() + theme_cache.line_spacing, 1);
}

int TextEdit::get_indent_level(int p_line) const {
	ERR_FAIL_INDEX_V(p_line, text.size(), 0);

	int tab_count = 0;
	int whitespace_count = 0;
	int line_length = text[p_line].size();
	for (int i = 0; i < line_length - 1; i++) {
		if (text[p_line][i] == '\t') {
			tab_count++;
		} else if (text[p_line][i] == ' ') {
			whitespace_count++;
		} else {
			break;
		}
	}
	return tab_count * text.get_tab_size() + whitespace_count;
}

int TextEdit::get_first_non_whitespace_column(int p_line) const {
	ERR_FAIL_INDEX_V(p_line, text.size(), 0);

	int col = 0;
	while (col < text[p_line].length() && is_whitespace(text[p_line][col])) {
		col++;
	}
	return col;
}

void TextEdit::swap_lines(int p_from_line, int p_to_line) {
	ERR_FAIL_INDEX(p_from_line, text.size());
	ERR_FAIL_INDEX(p_to_line, text.size());

	if (p_from_line == p_to_line) {
		return;
	}

	String from_line_text = get_line(p_from_line);
	String to_line_text = get_line(p_to_line);
	begin_complex_operation();
	begin_multicaret_edit();
	// Don't use set_line to avoid clamping and updating carets.
	_remove_text(p_to_line, 0, p_to_line, text[p_to_line].length());
	_insert_text(p_to_line, 0, from_line_text);
	_remove_text(p_from_line, 0, p_from_line, text[p_from_line].length());
	_insert_text(p_from_line, 0, to_line_text);

	// Swap carets.
	for (int i = 0; i < get_caret_count(); i++) {
		bool selected = has_selection(i);
		if (get_caret_line(i) == p_from_line || get_caret_line(i) == p_to_line) {
			int caret_new_line = get_caret_line(i) == p_from_line ? p_to_line : p_from_line;
			int caret_column = get_caret_column(i);
			set_caret_line(caret_new_line, false, true, -1, i);
			set_caret_column(caret_column, false, i);
		}
		if (selected && (get_selection_origin_line(i) == p_from_line || get_selection_origin_line(i) == p_to_line)) {
			int origin_new_line = get_selection_origin_line(i) == p_from_line ? p_to_line : p_from_line;
			int origin_column = get_selection_origin_column(i);
			select(origin_new_line, origin_column, get_caret_line(i), get_caret_column(i), i);
		}
	}
	// If only part of a selection was changed, it may now overlap.
	merge_overlapping_carets();

	end_multicaret_edit();
	end_complex_operation();
}

void TextEdit::insert_line_at(int p_line, const String &p_text) {
	ERR_FAIL_INDEX(p_line, text.size());

	// Use a complex operation so subsequent calls aren't merged together.
	begin_complex_operation();

	int new_line, new_column;
	_insert_text(p_line, 0, p_text + "\n", &new_line, &new_column);
	_offset_carets_after(p_line, 0, new_line, new_column);

	end_complex_operation();
}

void TextEdit::remove_line_at(int p_line, bool p_move_carets_down) {
	ERR_FAIL_INDEX(p_line, text.size());

	if (get_line_count() == 1) {
		// Only one line, just remove contents.
		begin_complex_operation();
		int line_length = get_line(p_line).length();
		_remove_text(p_line, 0, p_line, line_length);
		collapse_carets(p_line, 0, p_line, line_length, true);
		end_complex_operation();
		return;
	}

	begin_complex_operation();

	bool is_last_line = p_line == get_line_count() - 1;
	int from_line = is_last_line ? p_line - 1 : p_line;
	int next_line = is_last_line ? p_line : p_line + 1;
	int from_column = is_last_line ? get_line(from_line).length() : 0;
	int next_column = is_last_line ? get_line(next_line).length() : 0;

	if ((!is_last_line && p_move_carets_down) || (p_line != 0 && !p_move_carets_down)) {
		// Set the carets column to update their last offset x.
		for (int i = 0; i < get_caret_count(); i++) {
			if (get_caret_line(i) == p_line) {
				set_caret_column(get_caret_column(i), false, i);
			}
			if (has_selection(i) && get_selection_origin_line(i) == p_line) {
				set_selection_origin_column(get_selection_origin_column(i), i);
			}
		}
	}

	// Remove line.
	_remove_text(from_line, from_column, next_line, next_column);

	begin_multicaret_edit();
	if ((is_last_line && p_move_carets_down) || (p_line == 0 && !p_move_carets_down)) {
		// Collapse carets.
		collapse_carets(from_line, from_column, next_line, next_column, true);
	} else {
		// Move carets to visually line up.
		int target_line = p_move_carets_down ? p_line : p_line - 1;
		for (int i = 0; i < get_caret_count(); i++) {
			bool selected = has_selection(i);
			if (get_caret_line(i) == p_line) {
				set_caret_line(target_line, i == 0, true, 0, i);
			}
			if (selected && get_selection_origin_line(i) == p_line) {
				set_selection_origin_line(target_line, true, 0, i);
				select(get_selection_origin_line(i), get_selection_origin_column(i), get_caret_line(i), get_caret_column(i), i);
			}
		}

		merge_overlapping_carets();
	}
	_offset_carets_after(next_line, next_column, from_line, from_column);
	end_multicaret_edit();
	end_complex_operation();
}

void TextEdit::insert_text_at_caret(const String &p_text, int p_caret) {
	ERR_FAIL_COND(p_caret >= get_caret_count() || p_caret < -1);

	begin_complex_operation();
	begin_multicaret_edit();
	for (int i = 0; i < get_caret_count(); i++) {
		if (p_caret != -1 && p_caret != i) {
			continue;
		}
		if (p_caret == -1 && multicaret_edit_ignore_caret(i)) {
			continue;
		}

		delete_selection(i);

		int from_line = get_caret_line(i);
		int from_col = get_caret_column(i);

		int new_line, new_column;
		_insert_text(from_line, from_col, p_text, &new_line, &new_column);
		_update_scrollbars();
		_offset_carets_after(from_line, from_col, new_line, new_column);

		set_caret_line(new_line, false, true, -1, i);
		set_caret_column(new_column, i == 0, i);
	}

	if (has_ime_text()) {
		_update_ime_text();
	}

	end_multicaret_edit();
	end_complex_operation();
}

void TextEdit::insert_text(const String &p_text, int p_line, int p_column, bool p_before_selection_begin, bool p_before_selection_end) {
	ERR_FAIL_INDEX(p_line, text.size());
	ERR_FAIL_INDEX(p_column, text[p_line].length() + 1);

	begin_complex_operation();

	int new_line, new_column;
	_insert_text(p_line, p_column, p_text, &new_line, &new_column);

	_offset_carets_after(p_line, p_column, new_line, new_column, p_before_selection_begin, p_before_selection_end);

	end_complex_operation();
}

void TextEdit::remove_text(int p_from_line, int p_from_column, int p_to_line, int p_to_column) {
	ERR_FAIL_INDEX(p_from_line, text.size());
	ERR_FAIL_INDEX(p_from_column, text[p_from_line].length() + 1);
	ERR_FAIL_INDEX(p_to_line, text.size());
	ERR_FAIL_INDEX(p_to_column, text[p_to_line].length() + 1);
	ERR_FAIL_COND(p_to_line < p_from_line);
	ERR_FAIL_COND(p_to_line == p_from_line && p_to_column < p_from_column);

	begin_complex_operation();

	_remove_text(p_from_line, p_from_column, p_to_line, p_to_column);
	collapse_carets(p_from_line, p_from_column, p_to_line, p_to_column);
	_offset_carets_after(p_to_line, p_to_column, p_from_line, p_from_column);

	end_complex_operation();
}

int TextEdit::get_last_unhidden_line() const {
	// Returns the last line in the text that is not hidden.
	if (!_is_hiding_enabled()) {
		return text.size() - 1;
	}

	int last_line;
	for (last_line = text.size() - 1; last_line > 0; last_line--) {
		if (!_is_line_hidden(last_line)) {
			break;
		}
	}
	return last_line;
}

int TextEdit::get_next_visible_line_offset_from(int p_line_from, int p_visible_amount) const {
	// Returns the number of lines (hidden and unhidden) from p_line_from to (p_line_from + visible_amount of unhidden lines).
	ERR_FAIL_INDEX_V(p_line_from, text.size(), ABS(p_visible_amount));

	if (!_is_hiding_enabled()) {
		return ABS(p_visible_amount);
	}

	int num_visible = 0;
	int num_total = 0;
	if (p_visible_amount >= 0) {
		for (int i = p_line_from; i < text.size(); i++) {
			num_total++;
			if (!_is_line_hidden(i)) {
				num_visible++;
			}
			if (num_visible >= p_visible_amount) {
				break;
			}
		}
	} else {
		p_visible_amount = ABS(p_visible_amount);
		for (int i = p_line_from; i >= 0; i--) {
			num_total++;
			if (!_is_line_hidden(i)) {
				num_visible++;
			}
			if (num_visible >= p_visible_amount) {
				break;
			}
		}
	}
	return num_total;
}

Point2i TextEdit::get_next_visible_line_index_offset_from(int p_line_from, int p_wrap_index_from, int p_visible_amount) const {
	// Returns the number of lines (hidden and unhidden) from (p_line_from + p_wrap_index_from) row to (p_line_from + visible_amount of unhidden and wrapped rows).
	// Wrap index is set to the wrap index of the last line.
	int wrap_index = 0;
	ERR_FAIL_INDEX_V(p_line_from, text.size(), Point2i(ABS(p_visible_amount), 0));

	if (!_is_hiding_enabled() && get_line_wrapping_mode() == LineWrappingMode::LINE_WRAPPING_NONE) {
		return Point2i(ABS(p_visible_amount), 0);
	}

	int num_visible = 0;
	int num_total = 0;
	if (p_visible_amount == 0) {
		num_total = 0;
		wrap_index = 0;
	} else if (p_visible_amount > 0) {
		int i;
		num_visible -= p_wrap_index_from;
		for (i = p_line_from; i < text.size(); i++) {
			num_total++;
			if (!_is_line_hidden(i)) {
				num_visible++;
				num_visible += get_line_wrap_count(i);
			}
			if (num_visible >= p_visible_amount) {
				break;
			}
		}
		wrap_index = get_line_wrap_count(MIN(i, text.size() - 1)) - MAX(0, num_visible - p_visible_amount);

		// If we are a hidden line, then we are the last line as we cannot reach "p_visible_amount".
		// This means we need to backtrack to get last visible line.
		// Currently, line 0 cannot be hidden so this should always be valid.
		int line = (p_line_from + num_total) - 1;
		if (_is_line_hidden(line)) {
			Point2i backtrack = get_next_visible_line_index_offset_from(line, 0, -1);
			num_total = num_total - (backtrack.x - 1);
			wrap_index = backtrack.y;
		}
	} else {
		p_visible_amount = ABS(p_visible_amount);
		int i;
		num_visible -= get_line_wrap_count(p_line_from) - p_wrap_index_from;
		for (i = p_line_from; i >= 0; i--) {
			num_total++;
			if (!_is_line_hidden(i)) {
				num_visible++;
				num_visible += get_line_wrap_count(i);
			}
			if (num_visible >= p_visible_amount) {
				break;
			}
		}
		wrap_index = MAX(0, num_visible - p_visible_amount);
	}
	wrap_index = MAX(wrap_index, 0);
	return Point2i(num_total, wrap_index);
}

// Overridable actions
void TextEdit::handle_unicode_input(const uint32_t p_unicode, int p_caret) {
	if (GDVIRTUAL_CALL(_handle_unicode_input, p_unicode, p_caret)) {
		return;
	}
	_handle_unicode_input_internal(p_unicode, p_caret);
}

void TextEdit::backspace(int p_caret) {
	if (GDVIRTUAL_CALL(_backspace, p_caret)) {
		return;
	}
	_backspace_internal(p_caret);
}

void TextEdit::cut(int p_caret) {
	if (GDVIRTUAL_CALL(_cut, p_caret)) {
		return;
	}
	_cut_internal(p_caret);
}

void TextEdit::copy(int p_caret) {
	if (GDVIRTUAL_CALL(_copy, p_caret)) {
		return;
	}
	_copy_internal(p_caret);
}

void TextEdit::paste(int p_caret) {
	if (GDVIRTUAL_CALL(_paste, p_caret)) {
		return;
	}
	_paste_internal(p_caret);
}

void TextEdit::paste_primary_clipboard(int p_caret) {
	if (GDVIRTUAL_CALL(_paste_primary_clipboard, p_caret)) {
		return;
	}
	_paste_primary_clipboard_internal(p_caret);
}

// Context menu.
PopupMenu *TextEdit::get_menu() const {
	if (!menu) {
		const_cast<TextEdit *>(this)->_generate_context_menu();
	}
	return menu;
}

bool TextEdit::is_menu_visible() const {
	return menu && menu->is_visible();
}

void TextEdit::menu_option(int p_option) {
	switch (p_option) {
		case MENU_CUT: {
			cut();
		} break;
		case MENU_COPY: {
			copy();
		} break;
		case MENU_PASTE: {
			paste();
		} break;
		case MENU_CLEAR: {
			if (editable) {
				clear();
			}
		} break;
		case MENU_SELECT_ALL: {
			select_all();
		} break;
		case MENU_UNDO: {
			undo();
		} break;
		case MENU_REDO: {
			redo();
		} break;
		case MENU_DIR_INHERITED: {
			set_text_direction(TEXT_DIRECTION_INHERITED);
		} break;
		case MENU_DIR_AUTO: {
			set_text_direction(TEXT_DIRECTION_AUTO);
		} break;
		case MENU_DIR_LTR: {
			set_text_direction(TEXT_DIRECTION_LTR);
		} break;
		case MENU_DIR_RTL: {
			set_text_direction(TEXT_DIRECTION_RTL);
		} break;
		case MENU_DISPLAY_UCC: {
			set_draw_control_chars(!get_draw_control_chars());
		} break;
		case MENU_INSERT_LRM: {
			if (editable) {
				insert_text_at_caret(String::chr(0x200E));
			}
		} break;
		case MENU_INSERT_RLM: {
			if (editable) {
				insert_text_at_caret(String::chr(0x200F));
			}
		} break;
		case MENU_INSERT_LRE: {
			if (editable) {
				insert_text_at_caret(String::chr(0x202A));
			}
		} break;
		case MENU_INSERT_RLE: {
			if (editable) {
				insert_text_at_caret(String::chr(0x202B));
			}
		} break;
		case MENU_INSERT_LRO: {
			if (editable) {
				insert_text_at_caret(String::chr(0x202D));
			}
		} break;
		case MENU_INSERT_RLO: {
			if (editable) {
				insert_text_at_caret(String::chr(0x202E));
			}
		} break;
		case MENU_INSERT_PDF: {
			if (editable) {
				insert_text_at_caret(String::chr(0x202C));
			}
		} break;
		case MENU_INSERT_ALM: {
			if (editable) {
				insert_text_at_caret(String::chr(0x061C));
			}
		} break;
		case MENU_INSERT_LRI: {
			if (editable) {
				insert_text_at_caret(String::chr(0x2066));
			}
		} break;
		case MENU_INSERT_RLI: {
			if (editable) {
				insert_text_at_caret(String::chr(0x2067));
			}
		} break;
		case MENU_INSERT_FSI: {
			if (editable) {
				insert_text_at_caret(String::chr(0x2068));
			}
		} break;
		case MENU_INSERT_PDI: {
			if (editable) {
				insert_text_at_caret(String::chr(0x2069));
			}
		} break;
		case MENU_INSERT_ZWJ: {
			if (editable) {
				insert_text_at_caret(String::chr(0x200D));
			}
		} break;
		case MENU_INSERT_ZWNJ: {
			if (editable) {
				insert_text_at_caret(String::chr(0x200C));
			}
		} break;
		case MENU_INSERT_WJ: {
			if (editable) {
				insert_text_at_caret(String::chr(0x2060));
			}
		} break;
		case MENU_INSERT_SHY: {
			if (editable) {
				insert_text_at_caret(String::chr(0x00AD));
			}
		}
	}
}

/* Versioning */
void TextEdit::start_action(EditAction p_action) {
	if (current_action != p_action) {
		if (current_action != EditAction::ACTION_NONE) {
			in_action = false;
			pending_action_end = false;
			end_complex_operation();
		}

		if (p_action != EditAction::ACTION_NONE) {
			in_action = true;
			begin_complex_operation();
		}
	} else if (current_action != EditAction::ACTION_NONE) {
		pending_action_end = false;
	}
	current_action = p_action;
}

void TextEdit::end_action() {
	if (current_action != EditAction::ACTION_NONE) {
		pending_action_end = true;
	}
}

TextEdit::EditAction TextEdit::get_current_action() const {
	return current_action;
}

void TextEdit::begin_complex_operation() {
	_push_current_op();
	if (complex_operation_count == 0) {
		next_operation_is_complex = true;
		current_op.start_carets = carets;
	}
	complex_operation_count++;
}

void TextEdit::end_complex_operation() {
	_push_current_op();

	complex_operation_count = MAX(complex_operation_count - 1, 0);
	if (complex_operation_count > 0) {
		return;
	}
	if (undo_stack.is_empty()) {
		return;
	}

	undo_stack.back()->get().end_carets = carets;
	if (undo_stack.back()->get().chain_forward) {
		undo_stack.back()->get().chain_forward = false;
		return;
	}

	undo_stack.back()->get().chain_backward = true;
}

bool TextEdit::has_undo() const {
	if (undo_stack_pos == nullptr) {
		int pending = current_op.type == TextOperation::TYPE_NONE ? 0 : 1;
		return undo_stack.size() + pending > 0;
	}
	return undo_stack_pos != undo_stack.front();
}

bool TextEdit::has_redo() const {
	return undo_stack_pos != nullptr;
}

void TextEdit::undo() {
	if (!editable) {
		return;
	}

	if (in_action) {
		pending_action_end = true;
	}
	_push_current_op();

	if (undo_stack_pos == nullptr) {
		if (undo_stack.is_empty()) {
			return; // Nothing to undo.
		}

		undo_stack_pos = undo_stack.back();

	} else if (undo_stack_pos == undo_stack.front()) {
		return; // At the bottom of the undo stack.
	} else {
		undo_stack_pos = undo_stack_pos->prev();
	}

	deselect();

	TextOperation op = undo_stack_pos->get();
	_do_text_op(op, true);

	current_op.version = op.prev_version;
	if (undo_stack_pos->get().chain_backward) {
		// This was part of a complex operation, undo until the chain forward at the start of the complex operation.
		while (true) {
			ERR_BREAK(!undo_stack_pos->prev());
			undo_stack_pos = undo_stack_pos->prev();
			op = undo_stack_pos->get();
			_do_text_op(op, true);
			current_op.version = op.prev_version;
			if (undo_stack_pos->get().chain_forward) {
				break;
			}
		}
	}

	_update_scrollbars();
	bool dirty_carets = get_caret_count() != undo_stack_pos->get().start_carets.size();
	if (!dirty_carets) {
		for (int i = 0; i < get_caret_count(); i++) {
			if (carets[i].line != undo_stack_pos->get().start_carets[i].line || carets[i].column != undo_stack_pos->get().start_carets[i].column) {
				dirty_carets = true;
				break;
			}
		}
	}

	carets = undo_stack_pos->get().start_carets;

	_unhide_carets();

	if (dirty_carets) {
		_caret_changed();
		_selection_changed();
	}
	adjust_viewport_to_caret();
}

void TextEdit::redo() {
	if (!editable) {
		return;
	}

	if (in_action) {
		pending_action_end = true;
	}
	_push_current_op();

	if (!has_redo()) {
		return; // Nothing to do.
	}

	deselect();

	TextOperation op = undo_stack_pos->get();
	_do_text_op(op, false);
	current_op.version = op.version;
	if (undo_stack_pos->get().chain_forward) {
		// This was part of a complex operation, redo until the chain backward at the end of the complex operation.
		while (true) {
			ERR_BREAK(!undo_stack_pos->next());
			undo_stack_pos = undo_stack_pos->next();
			op = undo_stack_pos->get();
			_do_text_op(op, false);
			current_op.version = op.version;
			if (undo_stack_pos->get().chain_backward) {
				break;
			}
		}
	}

	_update_scrollbars();
	bool dirty_carets = get_caret_count() != undo_stack_pos->get().end_carets.size();
	if (!dirty_carets) {
		for (int i = 0; i < get_caret_count(); i++) {
			if (carets[i].line != undo_stack_pos->get().end_carets[i].line || carets[i].column != undo_stack_pos->get().end_carets[i].column) {
				dirty_carets = true;
				break;
			}
		}
	}

	carets = undo_stack_pos->get().end_carets;
	undo_stack_pos = undo_stack_pos->next();

	_unhide_carets();

	if (dirty_carets) {
		_caret_changed();
		_selection_changed();
	}
	adjust_viewport_to_caret();
}

void TextEdit::clear_undo_history() {
	saved_version = 0;
	current_op.type = TextOperation::TYPE_NONE;
	undo_stack_pos = nullptr;
	undo_stack.clear();
}

bool TextEdit::is_insert_text_operation() const {
	return (current_op.type == TextOperation::TYPE_INSERT || current_action == EditAction::ACTION_TYPING);
}

void TextEdit::tag_saved_version() {
	saved_version = get_version();
}

uint32_t TextEdit::get_version() const {
	return current_op.version;
}

uint32_t TextEdit::get_saved_version() const {
	return saved_version;
}

/* Search */
void TextEdit::set_search_text(const String &p_search_text) {
	search_text = p_search_text;
}

void TextEdit::set_search_flags(uint32_t p_flags) {
	search_flags = p_flags;
}

Point2i TextEdit::search(const String &p_key, uint32_t p_search_flags, int p_from_line, int p_from_column) const {
	if (p_key.length() == 0) {
		return Point2(-1, -1);
	}
	ERR_FAIL_INDEX_V(p_from_line, text.size(), Point2i(-1, -1));
	ERR_FAIL_INDEX_V(p_from_column, text[p_from_line].length() + 1, Point2i(-1, -1));

	// Search through the whole document, but start by current line.

	int line = p_from_line;
	int pos = -1;

	bool key_start_is_symbol = is_symbol(p_key[0]);
	bool key_end_is_symbol = is_symbol(p_key[p_key.length() - 1]);

	for (int i = 0; i < text.size() + 1; i++) {
		if (line < 0) {
			line = text.size() - 1;
		}
		if (line == text.size()) {
			line = 0;
		}

		String text_line = text[line];
		int from_column = 0;
		if (line == p_from_line) {
			if (i == text.size()) {
				// Wrapped.

				if (p_search_flags & SEARCH_BACKWARDS) {
					from_column = text_line.length();
				} else {
					from_column = 0;
				}

			} else {
				from_column = p_from_column;
			}

		} else {
			if (p_search_flags & SEARCH_BACKWARDS) {
				from_column = text_line.length() - 1;
			} else {
				from_column = 0;
			}
		}

		pos = -1;

		int pos_from = (p_search_flags & SEARCH_BACKWARDS) ? text_line.length() : 0;
		int last_pos = -1;

		while (true) {
			if (p_search_flags & SEARCH_BACKWARDS) {
				while ((last_pos = (p_search_flags & SEARCH_MATCH_CASE) ? text_line.rfind(p_key, pos_from) : text_line.rfindn(p_key, pos_from)) != -1) {
					if (last_pos <= from_column) {
						pos = last_pos;
						break;
					}
					pos_from = last_pos - p_key.length();
					if (pos_from < 0) {
						break;
					}
				}
			} else {
				while ((last_pos = (p_search_flags & SEARCH_MATCH_CASE) ? text_line.find(p_key, pos_from) : text_line.findn(p_key, pos_from)) != -1) {
					if (last_pos >= from_column) {
						pos = last_pos;
						break;
					}
					pos_from = last_pos + p_key.length();
				}
			}

			bool is_match = true;

			if (pos != -1 && (p_search_flags & SEARCH_WHOLE_WORDS)) {
				// Validate for whole words.
				if (!key_start_is_symbol && pos > 0 && !is_symbol(text_line[pos - 1])) {
					is_match = false;
				} else if (!key_end_is_symbol && pos + p_key.length() < text_line.length() && !is_symbol(text_line[pos + p_key.length()])) {
					is_match = false;
				}
			}

			if (pos_from == -1) {
				pos = -1;
			}

			if (is_match || last_pos == -1 || pos == -1) {
				break;
			}

			pos_from = (p_search_flags & SEARCH_BACKWARDS) ? pos - 1 : pos + 1;
			pos = -1;
		}

		if (pos != -1) {
			break;
		}

		if (p_search_flags & SEARCH_BACKWARDS) {
			line--;
		} else {
			line++;
		}
	}
	return (pos == -1) ? Point2i(-1, -1) : Point2i(pos, line);
}

/* Mouse */
Point2 TextEdit::get_local_mouse_pos() const {
	Point2 mp = get_local_mouse_position();
	if (is_layout_rtl()) {
		mp.x = get_size().width - mp.x;
	}
	return mp;
}

String TextEdit::get_word_at_pos(const Vector2 &p_pos) const {
	Point2i pos = get_line_column_at_pos(p_pos);
	int row = pos.y;
	int col = pos.x;

	String s = text[row];
	if (s.length() == 0) {
		return "";
	}
	int beg, end;
	if (select_word(s, col, beg, end)) {
		bool inside_quotes = false;
		char32_t selected_quote = '\0';
		int qbegin = 0, qend = 0;
		for (int i = 0; i < s.length(); i++) {
			if (s[i] == '"' || s[i] == '\'') {
				if (i == 0 || s[i - 1] != '\\') {
					if (inside_quotes && selected_quote == s[i]) {
						qend = i;
						inside_quotes = false;
						selected_quote = '\0';
						if (col >= qbegin && col <= qend) {
							return s.substr(qbegin, qend - qbegin);
						}
					} else if (!inside_quotes) {
						qbegin = i + 1;
						inside_quotes = true;
						selected_quote = s[i];
					}
				}
			}
		}

		return s.substr(beg, end - beg);
	}

	return String();
}

Point2i TextEdit::get_line_column_at_pos(const Point2i &p_pos, bool p_allow_out_of_bounds) const {
	float rows = p_pos.y - theme_cache.style_normal->get_margin(SIDE_TOP);
	if (!editable) {
		rows -= theme_cache.style_readonly->get_offset().y / 2;
		rows += theme_cache.style_normal->get_offset().y / 2;
	}
	rows /= get_line_height();
	rows += _get_v_scroll_offset();
	int first_vis_line = get_first_visible_line();
	int row = first_vis_line + Math::floor(rows);
	int wrap_index = 0;

	if (get_line_wrapping_mode() != LineWrappingMode::LINE_WRAPPING_NONE || _is_hiding_enabled()) {
		Point2i f_ofs = get_next_visible_line_index_offset_from(first_vis_line, first_visible_line_wrap_ofs, rows + (1 * SIGN(rows)));
		wrap_index = f_ofs.y;

		if (rows < 0) {
			row = first_vis_line - (f_ofs.x - 1);
		} else {
			row = first_vis_line + (f_ofs.x - 1);
		}
	}

	row = CLAMP(row, 0, text.size() - 1);

	int visible_lines = get_visible_line_count_in_range(first_vis_line, row);
	if (rows > visible_lines) {
		if (!p_allow_out_of_bounds) {
			return Point2i(-1, -1);
		}
		return Point2i(text[row].length(), row);
	}

	int colx = p_pos.x - (theme_cache.style_normal->get_margin(SIDE_LEFT) + gutters_width + gutter_padding);
	colx += first_visible_col;
	if (!editable) {
		colx -= theme_cache.style_readonly->get_offset().x / 2;
		colx += theme_cache.style_normal->get_offset().x / 2;
	}

	RID text_rid = text.get_line_data(row)->get_line_rid(wrap_index);
	float wrap_indent = (text.is_indent_wrapped_lines() && wrap_index > 0) ? get_indent_level(row) * theme_cache.font->get_char_size(' ', theme_cache.font_size).width : 0.0;
	if (is_layout_rtl()) {
		colx = TS->shaped_text_get_size(text_rid).x - colx + wrap_indent;
	} else {
		colx -= wrap_indent;
	}
	int col = TS->shaped_text_hit_test_position(text_rid, colx);
	if (!caret_mid_grapheme_enabled) {
		col = TS->shaped_text_closest_character_pos(text_rid, col);
	}

	return Point2i(col, row);
}

Point2i TextEdit::get_pos_at_line_column(int p_line, int p_column) const {
	Rect2i rect = get_rect_at_line_column(p_line, p_column);
	return rect.position.x == -1 ? rect.position : rect.position + Vector2i(0, get_line_height());
}

Rect2i TextEdit::get_rect_at_line_column(int p_line, int p_column) const {
	ERR_FAIL_INDEX_V(p_line, text.size(), Rect2i(-1, -1, 0, 0));
	ERR_FAIL_COND_V(p_column < 0, Rect2i(-1, -1, 0, 0));
	ERR_FAIL_COND_V(p_column > text[p_line].length(), Rect2i(-1, -1, 0, 0));

	if (text.size() == 1 && text[0].length() == 0) {
		// The TextEdit is empty.
		return Rect2i();
	}

	if (line_drawing_cache.size() == 0 || !line_drawing_cache.has(p_line)) {
		// Line is not in the cache, which means it's outside of the viewing area.
		return Rect2i(-1, -1, 0, 0);
	}
	LineDrawingCache cache_entry = line_drawing_cache[p_line];

	int wrap_index = get_line_wrap_index_at_column(p_line, p_column);
	if (wrap_index >= cache_entry.first_visible_chars.size()) {
		// Line seems to be wrapped beyond the viewable area.
		return Rect2i(-1, -1, 0, 0);
	}

	int first_visible_char = cache_entry.first_visible_chars[wrap_index];
	int last_visible_char = cache_entry.last_visible_chars[wrap_index];
	if (p_column < first_visible_char || p_column > last_visible_char) {
		// Character is outside of the viewing area, no point calculating its position.
		return Rect2i(-1, -1, 0, 0);
	}

	Point2i pos, size;
	pos.y = cache_entry.y_offset + get_line_height() * wrap_index;
	pos.x = get_total_gutter_width() + theme_cache.style_normal->get_margin(SIDE_LEFT) - get_h_scroll();

	RID text_rid = text.get_line_data(p_line)->get_line_rid(wrap_index);
	Vector2 col_bounds = TS->shaped_text_get_grapheme_bounds(text_rid, p_column);
	pos.x += col_bounds.x;
	size.x = col_bounds.y - col_bounds.x;

	size.y = get_line_height();

	return Rect2i(pos, size);
}

int TextEdit::get_minimap_line_at_pos(const Point2i &p_pos) const {
	float rows = p_pos.y;
	rows -= theme_cache.style_normal->get_margin(SIDE_TOP);
	rows /= (minimap_char_size.y + minimap_line_spacing);
	rows += _get_v_scroll_offset();

	// Calculate visible lines.
	int minimap_visible_lines = get_minimap_visible_lines();
	int visible_rows = get_visible_line_count() + 1;
	int first_vis_line = get_first_visible_line() - 1;
	int draw_amount = visible_rows + (smooth_scroll_enabled ? 1 : 0);
	draw_amount += get_line_wrap_count(first_vis_line + 1);
	int minimap_line_height = (minimap_char_size.y + minimap_line_spacing);

	// Calculate viewport size and y offset.
	int viewport_height = (draw_amount - 1) * minimap_line_height;
	int control_height = _get_control_height() - viewport_height;
	int viewport_offset_y = round(get_scroll_pos_for_line(first_vis_line + 1) * control_height) / ((v_scroll->get_max() <= minimap_visible_lines) ? (minimap_visible_lines - draw_amount) : (v_scroll->get_max() - draw_amount));

	// Calculate the first line.
	int num_lines_before = round((viewport_offset_y) / minimap_line_height);
	int minimap_line = (v_scroll->get_max() <= minimap_visible_lines) ? -1 : first_vis_line;
	if (first_vis_line > 0 && minimap_line >= 0) {
		minimap_line -= get_next_visible_line_index_offset_from(first_vis_line, 0, -num_lines_before).x;
		minimap_line -= (minimap_line > 0 && smooth_scroll_enabled ? 1 : 0);
	}

	if (minimap_line < 0) {
		minimap_line = 0;
	}

	int row = minimap_line + Math::floor(rows);
	if (get_line_wrapping_mode() != LineWrappingMode::LINE_WRAPPING_NONE || _is_hiding_enabled()) {
		int f_ofs = get_next_visible_line_index_offset_from(minimap_line, first_visible_line_wrap_ofs, rows + (1 * SIGN(rows))).x - 1;
		if (rows < 0) {
			row = minimap_line - f_ofs;
		} else {
			row = minimap_line + f_ofs;
		}
	}

	row = CLAMP(row, 0, text.size() - 1);

	return row;
}

bool TextEdit::is_dragging_cursor() const {
	return dragging_selection || dragging_minimap;
}

bool TextEdit::is_mouse_over_selection(bool p_edges, int p_caret) const {
	Point2i pos = get_line_column_at_pos(get_local_mouse_pos());
	int line = pos.y;
	int column = pos.x;

	if ((p_caret == -1 && get_selection_at_line_column(line, column, p_edges) != -1) || (p_caret != -1 && _selection_contains(p_caret, line, column, p_edges))) {
		return true;
	}
	return false;
}

/* Caret */
void TextEdit::set_caret_type(CaretType p_type) {
	if (caret_type == p_type) {
		return;
	}

	caret_type = p_type;
	queue_redraw();
}

TextEdit::CaretType TextEdit::get_caret_type() const {
	return caret_type;
}

void TextEdit::set_caret_blink_enabled(bool p_enabled) {
	if (caret_blink_enabled == p_enabled) {
		return;
	}

	caret_blink_enabled = p_enabled;

	if (has_focus()) {
		if (p_enabled) {
			caret_blink_timer->start();
		} else {
			caret_blink_timer->stop();
		}
	}
	draw_caret = true;
}

bool TextEdit::is_caret_blink_enabled() const {
	return caret_blink_enabled;
}

float TextEdit::get_caret_blink_interval() const {
	return caret_blink_timer->get_wait_time();
}

void TextEdit::set_caret_blink_interval(const float p_interval) {
	ERR_FAIL_COND(p_interval <= 0);
	caret_blink_timer->set_wait_time(p_interval);
}

void TextEdit::set_draw_caret_when_editable_disabled(bool p_enable) {
	if (draw_caret_when_editable_disabled == p_enable) {
		return;
	}
	draw_caret_when_editable_disabled = p_enable;
	queue_redraw();
}

bool TextEdit::is_drawing_caret_when_editable_disabled() const {
	return draw_caret_when_editable_disabled;
}

void TextEdit::set_move_caret_on_right_click_enabled(bool p_enabled) {
	move_caret_on_right_click = p_enabled;
}

bool TextEdit::is_move_caret_on_right_click_enabled() const {
	return move_caret_on_right_click;
}

void TextEdit::set_caret_mid_grapheme_enabled(bool p_enabled) {
	caret_mid_grapheme_enabled = p_enabled;
}

bool TextEdit::is_caret_mid_grapheme_enabled() const {
	return caret_mid_grapheme_enabled;
}

void TextEdit::set_multiple_carets_enabled(bool p_enabled) {
	multi_carets_enabled = p_enabled;
	if (!multi_carets_enabled) {
		remove_secondary_carets();
		multicaret_edit_count = 0;
		multicaret_edit_ignore_carets.clear();
		multicaret_edit_merge_queued = false;
	}
}

bool TextEdit::is_multiple_carets_enabled() const {
	return multi_carets_enabled;
}

int TextEdit::add_caret(int p_line, int p_column) {
	if (!multi_carets_enabled) {
		return -1;
	}
	_cancel_drag_and_drop_text();

	p_line = CLAMP(p_line, 0, text.size() - 1);
	p_column = CLAMP(p_column, 0, get_line(p_line).length());

	if (!is_in_mulitcaret_edit()) {
		// Carets cannot overlap.
		if (get_selection_at_line_column(p_line, p_column, true, false) != -1) {
			return -1;
		}
	}

	carets.push_back(Caret());
	int new_index = carets.size() - 1;
	set_caret_line(p_line, false, false, -1, new_index);
	set_caret_column(p_column, false, new_index);
	_caret_changed(new_index);

	if (is_in_mulitcaret_edit()) {
		multicaret_edit_ignore_carets.insert(new_index);
		merge_overlapping_carets();
	}
	return new_index;
}

void TextEdit::remove_caret(int p_caret) {
	ERR_FAIL_COND_MSG(carets.size() <= 1, "The main caret should not be removed.");
	ERR_FAIL_INDEX(p_caret, carets.size());

	_caret_changed(p_caret);
	carets.remove_at(p_caret);

	if (drag_caret_index >= 0) {
		if (p_caret == drag_caret_index) {
			drag_caret_index = -1;
		} else if (p_caret < drag_caret_index) {
			drag_caret_index -= 1;
		}
	}
}

void TextEdit::remove_drag_caret() {
	if (drag_caret_index >= 0) {
		if (drag_caret_index < carets.size()) {
			remove_caret(drag_caret_index);
		}
		drag_caret_index = -1;
	}
}

void TextEdit::remove_secondary_carets() {
	if (carets.size() == 1) {
		return;
	}

	_caret_changed();
	carets.resize(1);

	if (drag_caret_index >= 0) {
		drag_caret_index = -1;
	}
}

int TextEdit::get_caret_count() const {
	// Don't include drag caret.
	if (drag_caret_index >= 0) {
		return carets.size() - 1;
	}
	return carets.size();
}

void TextEdit::add_caret_at_carets(bool p_below) {
	if (!multi_carets_enabled) {
		return;
	}
	const int last_line_max_wrap = get_line_wrap_count(text.size() - 1);

	begin_multicaret_edit();
	int view_target_caret = -1;
	int view_line = p_below ? -1 : INT_MAX;
	int num_carets = get_caret_count();
	for (int i = 0; i < num_carets; i++) {
		const int caret_line = get_caret_line(i);
		const int caret_column = get_caret_column(i);
		bool is_selected = has_selection(i) || carets[i].last_fit_x != carets[i].selection.origin_last_fit_x;
		const int selection_origin_line = get_selection_origin_line(i);
		const int selection_origin_column = get_selection_origin_column(i);
		const int caret_wrap_index = get_caret_wrap_index(i);
		const int selection_origin_wrap_index = !is_selected ? -1 : get_line_wrap_index_at_column(selection_origin_line, selection_origin_column);

		if (caret_line == 0 && !p_below && (caret_wrap_index == 0 || selection_origin_wrap_index == 0)) {
			// Can't add above the first line.
			continue;
		}
		if (caret_line == text.size() - 1 && p_below && (caret_wrap_index == last_line_max_wrap || selection_origin_wrap_index == last_line_max_wrap)) {
			// Can't add below the last line.
			continue;
		}

		// Add a new caret.
		int new_caret_index = add_caret(caret_line, caret_column);
		ERR_FAIL_COND_MSG(new_caret_index < 0, "Failed to add a caret.");

		// Copy the selection origin and last fit.
		set_selection_origin_line(selection_origin_line, true, -1, new_caret_index);
		set_selection_origin_column(selection_origin_column, new_caret_index);
		carets.write[new_caret_index].last_fit_x = carets[i].last_fit_x;
		carets.write[new_caret_index].selection.origin_last_fit_x = carets[i].selection.origin_last_fit_x;

		// Move the caret up or down one visible line.
		if (!p_below) {
			// Move caret up.
			if (caret_wrap_index > 0) {
				set_caret_line(caret_line, false, false, caret_wrap_index - 1, new_caret_index);
			} else {
				int new_line = caret_line - get_next_visible_line_offset_from(caret_line - 1, -1);
				if (is_line_wrapped(new_line)) {
					set_caret_line(new_line, false, false, get_line_wrap_count(new_line), new_caret_index);
				} else {
					set_caret_line(new_line, false, false, 0, new_caret_index);
				}
			}
			// Move selection origin up.
			if (is_selected) {
				if (selection_origin_wrap_index > 0) {
					set_selection_origin_line(caret_line, false, selection_origin_wrap_index - 1, new_caret_index);
				} else {
					int new_line = selection_origin_line - get_next_visible_line_offset_from(selection_origin_line - 1, -1);
					if (is_line_wrapped(new_line)) {
						set_selection_origin_line(new_line, false, get_line_wrap_count(new_line), new_caret_index);
					} else {
						set_selection_origin_line(new_line, false, 0, new_caret_index);
					}
				}
			}
			if (get_caret_line(new_caret_index) < view_line) {
				view_line = get_caret_line(new_caret_index);
				view_target_caret = new_caret_index;
			}
		} else {
			// Move caret down.
			if (caret_wrap_index < get_line_wrap_count(caret_line)) {
				set_caret_line(caret_line, false, false, caret_wrap_index + 1, new_caret_index);
			} else {
				int new_line = caret_line + get_next_visible_line_offset_from(CLAMP(caret_line + 1, 0, text.size() - 1), 1);
				set_caret_line(new_line, false, false, 0, new_caret_index);
			}
			// Move selection origin down.
			if (is_selected) {
				if (selection_origin_wrap_index < get_line_wrap_count(selection_origin_line)) {
					set_selection_origin_line(selection_origin_line, false, selection_origin_wrap_index + 1, new_caret_index);
				} else {
					int new_line = selection_origin_line + get_next_visible_line_offset_from(CLAMP(selection_origin_line + 1, 0, text.size() - 1), 1);
					set_selection_origin_line(new_line, false, 0, new_caret_index);
				}
			}
			if (get_caret_line(new_caret_index) > view_line) {
				view_line = get_caret_line(new_caret_index);
				view_target_caret = new_caret_index;
			}
		}
		if (is_selected) {
			// Make sure selection is active.
			select(get_selection_origin_line(new_caret_index), get_selection_origin_column(new_caret_index), get_caret_line(new_caret_index), get_caret_column(new_caret_index), new_caret_index);
			carets.write[new_caret_index].last_fit_x = carets[i].last_fit_x;
			carets.write[new_caret_index].selection.origin_last_fit_x = carets[i].selection.origin_last_fit_x;
		}

		bool check_edges = !has_selection(0) || !has_selection(new_caret_index);
		bool will_merge_with_main_caret = _selection_contains(0, get_caret_line(new_caret_index), get_caret_column(new_caret_index), check_edges, false) || _selection_contains(new_caret_index, get_caret_line(0), get_caret_column(0), check_edges, false);
		if (will_merge_with_main_caret) {
			// Move next to the main caret so it stays the main caret after merging.
			Caret new_caret = carets[new_caret_index];
			carets.remove_at(new_caret_index);
			carets.insert(0, new_caret);
			i++;
		}
	}

	// Show the topmost caret if added above or bottommost caret if added below.
	if (view_target_caret >= 0 && view_target_caret < get_caret_count()) {
		adjust_viewport_to_caret(view_target_caret);
	}

	merge_overlapping_carets();
	end_multicaret_edit();
}

struct _CaretSortComparator {
	_FORCE_INLINE_ bool operator()(const Vector3i &a, const Vector3i &b) const {
		// x is column, y is line, z is caret index.
		if (a.y == b.y) {
			return a.x < b.x;
		}
		return a.y < b.y;
	}
};

Vector<int> TextEdit::get_sorted_carets(bool p_include_ignored_carets) const {
	// Returns caret indexes sorted by selection start or caret position from top to bottom of text.
	Vector<Vector3i> caret_line_col_indexes;
	for (int i = 0; i < get_caret_count(); i++) {
		if (!p_include_ignored_carets && multicaret_edit_ignore_caret(i)) {
			continue;
		}
		caret_line_col_indexes.push_back(Vector3i(get_selection_from_column(i), get_selection_from_line(i), i));
	}
	caret_line_col_indexes.sort_custom<_CaretSortComparator>();
	Vector<int> sorted;
	sorted.resize(caret_line_col_indexes.size());
	for (int i = 0; i < caret_line_col_indexes.size(); i++) {
		sorted.set(i, caret_line_col_indexes[i].z);
	}
	return sorted;
}

void TextEdit::collapse_carets(int p_from_line, int p_from_column, int p_to_line, int p_to_column, bool p_inclusive) {
	// Collapse carets in the selected range to the from position.

	// Clamp the collapse target position.
	int collapse_line = CLAMP(p_from_line, 0, text.size() - 1);
	int collapse_column = CLAMP(p_from_column, 0, text[collapse_line].length());

	// Swap the lines if they are in the wrong order.
	if (p_from_line > p_to_line) {
		SWAP(p_from_line, p_to_line);
		SWAP(p_from_column, p_to_column);
	}
	if (p_from_line == p_to_line && p_from_column > p_to_column) {
		SWAP(p_from_column, p_to_column);
	}
	bool any_collapsed = false;

	// Intentionally includes carets in the multicaret_edit_ignore list so that they are moved together.
	for (int i = 0; i < get_caret_count(); i++) {
		bool is_caret_in = _is_line_col_in_range(get_caret_line(i), get_caret_column(i), p_from_line, p_from_column, p_to_line, p_to_column, p_inclusive);
		if (!has_selection(i)) {
			if (is_caret_in) {
				// Caret was in the collapsed area.
				set_caret_line(collapse_line, false, true, -1, i);
				set_caret_column(collapse_column, false, i);
				if (is_in_mulitcaret_edit() && get_caret_count() > 1) {
					multicaret_edit_ignore_carets.insert(i);
				}
				any_collapsed = true;
			}
		} else {
			bool is_origin_in = _is_line_col_in_range(get_selection_origin_line(i), get_selection_origin_column(i), p_from_line, p_from_column, p_to_line, p_to_column, p_inclusive);

			if (is_caret_in && is_origin_in) {
				// Selection was completely encapsulated.
				deselect(i);
				set_caret_line(collapse_line, false, true, -1, i);
				set_caret_column(collapse_column, false, i);
				if (is_in_mulitcaret_edit() && get_caret_count() > 1) {
					multicaret_edit_ignore_carets.insert(i);
				}
				any_collapsed = true;
			} else if (is_caret_in) {
				// Only caret was inside.
				set_caret_line(collapse_line, false, true, -1, i);
				set_caret_column(collapse_column, false, i);
				any_collapsed = true;
			} else if (is_origin_in) {
				// Only selection origin was inside.
				set_selection_origin_line(collapse_line, true, -1, i);
				set_selection_origin_column(collapse_column, i);
				any_collapsed = true;
			}
		}
		if (!p_inclusive && !any_collapsed) {
			if ((get_caret_line(i) == collapse_line && get_caret_column(i) == collapse_column) || (get_selection_origin_line(i) == collapse_line && get_selection_origin_column(i) == collapse_column)) {
				// Make sure to queue a merge, even if we didn't include it.
				any_collapsed = true;
			}
		}
	}
	if (any_collapsed) {
		merge_overlapping_carets();
	}
}

void TextEdit::merge_overlapping_carets() {
	if (is_in_mulitcaret_edit()) {
		// Queue merge to be performed the end of the multicaret edit.
		multicaret_edit_merge_queued = true;
		return;
	}

	multicaret_edit_merge_queued = false;
	multicaret_edit_ignore_carets.clear();

	if (get_caret_count() == 1) {
		return;
	}

	Vector<int> sorted_carets = get_sorted_carets(true);
	for (int i = 0; i < sorted_carets.size() - 1; i++) {
		int first_caret = sorted_carets[i];
		int second_caret = sorted_carets[i + 1];

		bool merge_carets;
		if (!has_selection(first_caret) || !has_selection(second_caret)) {
			// Merge if touching.
			merge_carets = get_selection_from_line(second_caret) < get_selection_to_line(first_caret) || (get_selection_from_line(second_caret) == get_selection_to_line(first_caret) && get_selection_from_column(second_caret) <= get_selection_to_column(first_caret));
		} else {
			// Merge two selections if overlapping.
			merge_carets = get_selection_from_line(second_caret) < get_selection_to_line(first_caret) || (get_selection_from_line(second_caret) == get_selection_to_line(first_caret) && get_selection_from_column(second_caret) < get_selection_to_column(first_caret));
		}

		if (!merge_carets) {
			continue;
		}

		// Save the newest one for Click + Drag.
		int caret_to_save = first_caret;
		int caret_to_remove = second_caret;
		if (first_caret < second_caret) {
			caret_to_save = second_caret;
			caret_to_remove = first_caret;
		}

		if (get_selection_from_line(caret_to_save) != get_selection_from_line(caret_to_remove) || get_selection_to_line(caret_to_save) != get_selection_to_line(caret_to_remove) || get_selection_from_column(caret_to_save) != get_selection_from_column(caret_to_remove) || get_selection_to_column(caret_to_save) != get_selection_to_column(caret_to_remove)) {
			// Selections are not the same, merge them into one bigger selection.
			int new_from_line = MIN(get_selection_from_line(caret_to_remove), get_selection_from_line(caret_to_save));
			int new_to_line = MAX(get_selection_to_line(caret_to_remove), get_selection_to_line(caret_to_save));
			int new_from_col;
			int new_to_col;
			if (get_selection_from_line(caret_to_remove) < get_selection_from_line(caret_to_save)) {
				new_from_col = get_selection_from_column(caret_to_remove);
			} else if (get_selection_from_line(caret_to_remove) > get_selection_from_line(caret_to_save)) {
				new_from_col = get_selection_from_column(caret_to_save);
			} else {
				new_from_col = MIN(get_selection_from_column(caret_to_remove), get_selection_from_column(caret_to_save));
			}
			if (get_selection_to_line(caret_to_remove) < get_selection_to_line(caret_to_save)) {
				new_to_col = get_selection_to_column(caret_to_save);
			} else if (get_selection_to_line(caret_to_remove) > get_selection_to_line(caret_to_save)) {
				new_to_col = get_selection_to_column(caret_to_remove);
			} else {
				new_to_col = MAX(get_selection_to_column(caret_to_remove), get_selection_to_column(caret_to_save));
			}

			// Use the direction from the last caret or the saved one.
			int caret_dir_to_copy;
			if (has_selection(caret_to_remove) && has_selection(caret_to_save)) {
				caret_dir_to_copy = caret_to_remove == get_caret_count() - 1 ? caret_to_remove : caret_to_save;
			} else {
				caret_dir_to_copy = !has_selection(caret_to_remove) ? caret_to_save : caret_to_remove;
			}

			if (is_caret_after_selection_origin(caret_dir_to_copy)) {
				select(new_from_line, new_from_col, new_to_line, new_to_col, caret_to_save);
			} else {
				select(new_to_line, new_to_col, new_from_line, new_from_col, caret_to_save);
			}
		}

		if (caret_to_save == 0) {
			adjust_viewport_to_caret(caret_to_save);
		}
		remove_caret(caret_to_remove);

		// Update the rest of the sorted list.
		for (int j = i; j < sorted_carets.size(); j++) {
			if (sorted_carets[j] > caret_to_remove) {
				// Shift the index since a caret before it was removed.
				sorted_carets.write[j] -= 1;
			}
		}
		// Remove the caret from the sorted array.
		sorted_carets.remove_at(caret_to_remove == first_caret ? i : i + 1);

		// Process the caret again, since it and the next caret might also overlap.
		i--;
	}
}

// Starts a multicaret edit operation. Call this before iterating over the carets and call [end_multicaret_edit] afterwards.
void TextEdit::begin_multicaret_edit() {
	if (!multi_carets_enabled) {
		return;
	}
	multicaret_edit_count++;
}

void TextEdit::end_multicaret_edit() {
	if (!multi_carets_enabled) {
		return;
	}
	if (multicaret_edit_count > 0) {
		multicaret_edit_count--;
	}
	if (multicaret_edit_count != 0) {
		return;
	}

	// This was the last multicaret edit operation.
	if (multicaret_edit_merge_queued) {
		merge_overlapping_carets();
	}
	multicaret_edit_ignore_carets.clear();
}

bool TextEdit::is_in_mulitcaret_edit() const {
	return multicaret_edit_count > 0;
}

bool TextEdit::multicaret_edit_ignore_caret(int p_caret) const {
	return multicaret_edit_ignore_carets.has(p_caret);
}

bool TextEdit::is_caret_visible(int p_caret) const {
	ERR_FAIL_INDEX_V(p_caret, carets.size(), false);
	return carets[p_caret].visible;
}

Point2 TextEdit::get_caret_draw_pos(int p_caret) const {
	ERR_FAIL_INDEX_V(p_caret, carets.size(), Point2(0, 0));
	return carets[p_caret].draw_pos;
}

void TextEdit::set_caret_line(int p_line, bool p_adjust_viewport, bool p_can_be_hidden, int p_wrap_index, int p_caret) {
	ERR_FAIL_INDEX(p_caret, carets.size());
	if (setting_caret_line) {
		return;
	}

	setting_caret_line = true;
	p_line = CLAMP(p_line, 0, text.size() - 1);

	if (!p_can_be_hidden) {
		if (_is_line_hidden(p_line)) {
			int move_down = get_next_visible_line_offset_from(p_line, 1) - 1;
			if (p_line + move_down <= text.size() - 1 && !_is_line_hidden(p_line + move_down)) {
				p_line += move_down;
			} else {
				int move_up = get_next_visible_line_offset_from(p_line, -1) - 1;
				if (p_line - move_up > 0 && !_is_line_hidden(p_line - move_up)) {
					p_line -= move_up;
				} else {
					WARN_PRINT("Caret set to hidden line " + itos(p_line) + " and there are no nonhidden lines.");
				}
			}
		}
	}
	bool caret_moved = get_caret_line(p_caret) != p_line;
	carets.write[p_caret].line = p_line;

	int n_col;
	if (p_wrap_index >= 0) {
		// Keep caret in same visual x position it was at previously.
		n_col = _get_char_pos_for_line(carets[p_caret].last_fit_x, p_line, p_wrap_index);
		if (n_col != 0 && get_line_wrapping_mode() != LineWrappingMode::LINE_WRAPPING_NONE && p_wrap_index < get_line_wrap_count(p_line)) {
			// Offset by one to not go past the end of the wrapped line.
			if (n_col >= text.get_line_wrap_ranges(p_line)[p_wrap_index].y) {
				n_col -= 1;
			}
		}
	} else {
		// Clamp the column.
		n_col = MIN(get_caret_column(p_caret), get_line(p_line).length());
	}
	caret_moved = (caret_moved || get_caret_column(p_caret) != n_col);
	carets.write[p_caret].column = n_col;

	// Unselect if the caret moved to the selection origin.
	if (p_wrap_index >= 0 && has_selection(p_caret) && get_caret_line(p_caret) == get_selection_origin_line(p_caret) && get_caret_column(p_caret) == get_selection_origin_column(p_caret)) {
		deselect(p_caret);
	}

	if (is_inside_tree() && p_adjust_viewport) {
		adjust_viewport_to_caret(p_caret);
	}

	setting_caret_line = false;

	if (caret_moved) {
		_caret_changed(p_caret);
	}
}

int TextEdit::get_caret_line(int p_caret) const {
	ERR_FAIL_INDEX_V(p_caret, carets.size(), 0);
	return carets[p_caret].line;
}

void TextEdit::set_caret_column(int p_column, bool p_adjust_viewport, int p_caret) {
	ERR_FAIL_INDEX(p_caret, carets.size());

	p_column = CLAMP(p_column, 0, get_line(get_caret_line(p_caret)).length());

	bool caret_moved = get_caret_column(p_caret) != p_column;
	carets.write[p_caret].column = p_column;

	carets.write[p_caret].last_fit_x = _get_column_x_offset_for_line(get_caret_column(p_caret), get_caret_line(p_caret), get_caret_column(p_caret));

	if (!has_selection(p_caret)) {
		// Set the selection origin last fit x to be the same, so we can tell if there was a selection.
		carets.write[p_caret].selection.origin_last_fit_x = carets[p_caret].last_fit_x;
	}

	// Unselect if the caret moved to the selection origin.
	if (has_selection(p_caret) && get_caret_line(p_caret) == get_selection_origin_line(p_caret) && get_caret_column(p_caret) == get_selection_origin_column(p_caret)) {
		deselect(p_caret);
	}

	if (is_inside_tree() && p_adjust_viewport) {
		adjust_viewport_to_caret(p_caret);
	}

	if (caret_moved) {
		_caret_changed(p_caret);
	}
}

int TextEdit::get_caret_column(int p_caret) const {
	ERR_FAIL_INDEX_V(p_caret, carets.size(), 0);
	return carets[p_caret].column;
}

int TextEdit::get_caret_wrap_index(int p_caret) const {
	ERR_FAIL_INDEX_V(p_caret, carets.size(), 0);
	return get_line_wrap_index_at_column(get_caret_line(p_caret), get_caret_column(p_caret));
}

String TextEdit::get_word_under_caret(int p_caret) const {
	ERR_FAIL_COND_V(p_caret >= carets.size() || p_caret < -1, "");

	StringBuilder selected_text;
	for (int c = 0; c < carets.size(); c++) {
		if (p_caret != -1 && p_caret != c) {
			continue;
		}

		PackedInt32Array words = TS->shaped_text_get_word_breaks(text.get_line_data(get_caret_line(c))->get_rid());
		for (int i = 0; i < words.size(); i = i + 2) {
			if (words[i] <= get_caret_column(c) && words[i + 1] > get_caret_column(c)) {
				selected_text += text[get_caret_line(c)].substr(words[i], words[i + 1] - words[i]);
				if (p_caret == -1 && c != carets.size() - 1) {
					selected_text += "\n";
				}
			}
		}
	}
	return selected_text.as_string();
}

/* Selection. */
void TextEdit::set_selecting_enabled(bool p_enabled) {
	if (selecting_enabled == p_enabled) {
		return;
	}

	selecting_enabled = p_enabled;

	if (!selecting_enabled) {
		deselect();
	}
}

bool TextEdit::is_selecting_enabled() const {
	return selecting_enabled;
}

void TextEdit::set_deselect_on_focus_loss_enabled(bool p_enabled) {
	if (deselect_on_focus_loss_enabled == p_enabled) {
		return;
	}

	deselect_on_focus_loss_enabled = p_enabled;
	if (p_enabled && has_selection() && !has_focus()) {
		deselect();
	}
}

bool TextEdit::is_deselect_on_focus_loss_enabled() const {
	return deselect_on_focus_loss_enabled;
}

void TextEdit::set_drag_and_drop_selection_enabled(bool p_enabled) {
	drag_and_drop_selection_enabled = p_enabled;
}

bool TextEdit::is_drag_and_drop_selection_enabled() const {
	return drag_and_drop_selection_enabled;
}

void TextEdit::set_selection_mode(SelectionMode p_mode) {
	selecting_mode = p_mode;
}

TextEdit::SelectionMode TextEdit::get_selection_mode() const {
	return selecting_mode;
}

void TextEdit::select_all() {
	_push_current_op();
	if (!selecting_enabled) {
		return;
	}

	if (text.size() == 1 && text[0].length() == 0) {
		return;
	}

	remove_secondary_carets();
	set_selection_mode(SelectionMode::SELECTION_MODE_SHIFT);
	select(0, 0, text.size() - 1, text[text.size() - 1].length());
}

void TextEdit::select_word_under_caret(int p_caret) {
	ERR_FAIL_COND(p_caret >= carets.size() || p_caret < -1);

	_push_current_op();
	if (!selecting_enabled) {
		return;
	}

	if (text.size() == 1 && text[0].length() == 0) {
		return;
	}

	for (int c = 0; c < carets.size(); c++) {
		if (p_caret != -1 && p_caret != c) {
			continue;
		}

		if (has_selection(c)) {
			// Allow toggling selection by pressing the shortcut a second time.
			// This is also usable as a general-purpose "deselect" shortcut after
			// selecting anything.
			deselect(c);
			continue;
		}

		int begin = 0;
		int end = 0;
		const PackedInt32Array words = TS->shaped_text_get_word_breaks(text.get_line_data(get_caret_line(c))->get_rid());
		for (int i = 0; i < words.size(); i = i + 2) {
			if ((words[i] <= get_caret_column(c) && words[i + 1] >= get_caret_column(c)) || (i == words.size() - 2 && get_caret_column(c) == words[i + 1])) {
				begin = words[i];
				end = words[i + 1];
				break;
			}
		}

		// No word found.
		if (begin == 0 && end == 0) {
			continue;
		}

		select(get_caret_line(c), begin, get_caret_line(c), end, c);
	}
	merge_overlapping_carets();
}

void TextEdit::add_selection_for_next_occurrence() {
	if (!selecting_enabled || !is_multiple_carets_enabled()) {
		return;
	}

	if (text.size() == 1 && text[0].length() == 0) {
		return;
	}

	_push_current_op();
	// Always use the last caret, to correctly search for
	// the next occurrence that comes after this caret.
	int caret = get_caret_count() - 1;

	if (!has_selection(caret)) {
		select_word_under_caret(caret);
		return;
	}

	const String &highlighted_text = get_selected_text(caret);
	int column = get_selection_from_column(caret) + 1;
	int line = get_selection_from_line(caret);

	const Point2i next_occurrence = search(highlighted_text, SEARCH_MATCH_CASE, line, column);

	if (next_occurrence.x == -1 || next_occurrence.y == -1) {
		return;
	}

	int to_column = get_selection_to_column(caret) + 1;
	int end = next_occurrence.x + (to_column - column);
	int new_caret = add_caret(next_occurrence.y, end);

	if (new_caret != -1) {
		select(next_occurrence.y, next_occurrence.x, next_occurrence.y, end, new_caret);
		_unhide_carets();
		adjust_viewport_to_caret(new_caret);
		merge_overlapping_carets();
	}
}

void TextEdit::skip_selection_for_next_occurrence() {
	if (!selecting_enabled) {
		return;
	}

	if (text.size() == 1 && text[0].length() == 0) {
		return;
	}

	// Always use the last caret, to correctly search for
	// the next occurrence that comes after this caret.
	int caret = get_caret_count() - 1;

	// Supports getting the text under caret without selecting it.
	// It allows to use this shortcut to simply jump to the next (under caret) word.
	// Due to const and &(reference) presence, ternary operator is a way to avoid errors and warnings.
	const String &searched_text = has_selection(caret) ? get_selected_text(caret) : get_word_under_caret(caret);

	int column = get_selection_from_column(caret) + 1;
	int line = get_selection_from_line(caret);

	const Point2i next_occurrence = search(searched_text, SEARCH_MATCH_CASE, line, column);

	if (next_occurrence.x == -1 || next_occurrence.y == -1) {
		return;
	}

	int to_column = get_selection_to_column(caret) + 1;
	int end = next_occurrence.x + (to_column - column);
	int new_caret = add_caret(next_occurrence.y, end);

	if (new_caret != -1) {
		select(next_occurrence.y, next_occurrence.x, next_occurrence.y, end, new_caret);
		_unhide_carets();
		adjust_viewport_to_caret(new_caret);
		merge_overlapping_carets();
	}

	// Deselect word under previous caret.
	if (has_selection(caret)) {
		select_word_under_caret(caret);
	}

	// Remove previous caret.
	if (get_caret_count() > 1) {
		remove_caret(caret);
	}
}

void TextEdit::select(int p_origin_line, int p_origin_column, int p_caret_line, int p_caret_column, int p_caret) {
	ERR_FAIL_INDEX(p_caret, get_caret_count());

	p_caret_line = CLAMP(p_caret_line, 0, text.size() - 1);
	p_caret_column = CLAMP(p_caret_column, 0, text[p_caret_line].length());
	set_caret_line(p_caret_line, false, true, -1, p_caret);
	set_caret_column(p_caret_column, false, p_caret);

	if (!selecting_enabled) {
		return;
	}

	p_origin_line = CLAMP(p_origin_line, 0, text.size() - 1);
	p_origin_column = CLAMP(p_origin_column, 0, text[p_origin_line].length());
	set_selection_origin_line(p_origin_line, true, -1, p_caret);
	set_selection_origin_column(p_origin_column, p_caret);

	bool had_selection = has_selection(p_caret);
	bool activate = p_origin_line != p_caret_line || p_origin_column != p_caret_column;
	carets.write[p_caret].selection.active = activate;
	if (had_selection != activate) {
		_selection_changed(p_caret);
	}
}

bool TextEdit::has_selection(int p_caret) const {
	ERR_FAIL_COND_V(p_caret >= carets.size() || p_caret < -1, false);
	if (p_caret >= 0) {
		return carets[p_caret].selection.active;
	}
	for (int i = 0; i < carets.size(); i++) {
		if (carets[i].selection.active) {
			return true;
		}
	}
	return false;
}

String TextEdit::get_selected_text(int p_caret) {
	ERR_FAIL_COND_V(p_caret >= carets.size() || p_caret < -1, "");

	if (p_caret >= 0) {
		if (!has_selection(p_caret)) {
			return "";
		}
		return _base_get_text(get_selection_from_line(p_caret), get_selection_from_column(p_caret), get_selection_to_line(p_caret), get_selection_to_column(p_caret));
	}

	StringBuilder selected_text;
	Vector<int> sorted_carets = get_sorted_carets();
	for (int i = 0; i < sorted_carets.size(); i++) {
		int caret_index = sorted_carets[i];

		if (!has_selection(caret_index)) {
			continue;
		}
		if (selected_text.get_string_length() != 0) {
			selected_text += "\n";
		}
		selected_text += _base_get_text(get_selection_from_line(caret_index), get_selection_from_column(caret_index), get_selection_to_line(caret_index), get_selection_to_column(caret_index));
	}

	return selected_text.as_string();
}

int TextEdit::get_selection_at_line_column(int p_line, int p_column, bool p_include_edges, bool p_only_selections) const {
	// Return the caret index of the found selection, or -1.
	for (int i = 0; i < get_caret_count(); i++) {
		if (_selection_contains(i, p_line, p_column, p_include_edges, p_only_selections)) {
			return i;
		}
	}
	return -1;
}

Vector<Point2i> TextEdit::get_line_ranges_from_carets(bool p_only_selections, bool p_merge_adjacent) const {
	// Get a series of line ranges that cover all lines that have a caret or selection.
	// For each Point2i range, x is the first line and y is the last line.
	Vector<Point2i> ret;
	int last_to_line = INT_MIN;

	Vector<int> sorted_carets = get_sorted_carets();
	for (int i = 0; i < sorted_carets.size(); i++) {
		int caret_index = sorted_carets[i];
		if (p_only_selections && !has_selection(caret_index)) {
			continue;
		}
		Point2i range = Point2i(get_selection_from_line(caret_index), get_selection_to_line(caret_index));
		if (has_selection(caret_index) && get_selection_to_column(caret_index) == 0) {
			// Dont include selection end line if it ends at column 0.
			range.y--;
		}
		if (range.x == last_to_line || (p_merge_adjacent && range.x - 1 == last_to_line)) {
			// Merge if starts on the same line or adjacent line.
			ret.write[ret.size() - 1].y = range.y;
		} else {
			ret.append(range);
		}
		last_to_line = range.y;
	}
	return ret;
}

TypedArray<Vector2i> TextEdit::get_line_ranges_from_carets_typed_array(bool p_only_selections, bool p_merge_adjacent) const {
	// Wrapper for `get_line_ranges_from_carets` to return a datatype that can be exposed.
	TypedArray<Vector2i> ret;
	Vector<Point2i> ranges = get_line_ranges_from_carets(p_only_selections, p_merge_adjacent);
	for (const Point2i &range : ranges) {
		ret.push_back(range);
	}
	return ret;
}

void TextEdit::set_selection_origin_line(int p_line, bool p_can_be_hidden, int p_wrap_index, int p_caret) {
	if (!selecting_enabled) {
		return;
	}
	ERR_FAIL_INDEX(p_caret, carets.size());
	p_line = CLAMP(p_line, 0, text.size() - 1);

	if (!p_can_be_hidden) {
		if (_is_line_hidden(p_line)) {
			int move_down = get_next_visible_line_offset_from(p_line, 1) - 1;
			if (p_line + move_down <= text.size() - 1 && !_is_line_hidden(p_line + move_down)) {
				p_line += move_down;
			} else {
				int move_up = get_next_visible_line_offset_from(p_line, -1) - 1;
				if (p_line - move_up > 0 && !_is_line_hidden(p_line - move_up)) {
					p_line -= move_up;
				} else {
					WARN_PRINT("Selection origin set to hidden line " + itos(p_line) + " and there are no nonhidden lines.");
				}
			}
		}
	}

	bool selection_moved = get_selection_origin_line(p_caret) != p_line;
	carets.write[p_caret].selection.origin_line = p_line;

	int n_col;
	if (p_wrap_index >= 0) {
		// Keep selection origin in same visual x position it was at previously.
		n_col = _get_char_pos_for_line(carets[p_caret].selection.origin_last_fit_x, p_line, p_wrap_index);
		if (n_col != 0 && get_line_wrapping_mode() != LineWrappingMode::LINE_WRAPPING_NONE && p_wrap_index < get_line_wrap_count(p_line)) {
			// Offset by one to not go past the end of the wrapped line.
			if (n_col >= text.get_line_wrap_ranges(p_line)[p_wrap_index].y) {
				n_col -= 1;
			}
		}
	} else {
		// Clamp the column.
		n_col = MIN(get_selection_origin_column(p_caret), get_line(p_line).length());
	}
	selection_moved = (selection_moved || get_selection_origin_column(p_caret) != n_col);
	carets.write[p_caret].selection.origin_column = n_col;

	// Unselect if the selection origin moved to the caret.
	if (p_wrap_index >= 0 && has_selection(p_caret) && get_caret_line(p_caret) == get_selection_origin_line(p_caret) && get_caret_column(p_caret) == get_selection_origin_column(p_caret)) {
		deselect(p_caret);
	}

	if (selection_moved && has_selection(p_caret)) {
		_selection_changed(p_caret);
	}
}

void TextEdit::set_selection_origin_column(int p_column, int p_caret) {
	if (!selecting_enabled) {
		return;
	}
	ERR_FAIL_INDEX(p_caret, carets.size());

	p_column = CLAMP(p_column, 0, get_line(get_selection_origin_line(p_caret)).length());

	bool selection_moved = get_selection_origin_column(p_caret) != p_column;

	carets.write[p_caret].selection.origin_column = p_column;

	carets.write[p_caret].selection.origin_last_fit_x = _get_column_x_offset_for_line(get_selection_origin_column(p_caret), get_selection_origin_line(p_caret), get_selection_origin_column(p_caret));

	// Unselect if the selection origin moved to the caret.
	if (has_selection(p_caret) && get_caret_line(p_caret) == get_selection_origin_line(p_caret) && get_caret_column(p_caret) == get_selection_origin_column(p_caret)) {
		deselect(p_caret);
	}

	if (selection_moved && has_selection(p_caret)) {
		_selection_changed(p_caret);
	}
}

int TextEdit::get_selection_origin_line(int p_caret) const {
	ERR_FAIL_INDEX_V(p_caret, carets.size(), -1);
	return carets[p_caret].selection.origin_line;
}

int TextEdit::get_selection_origin_column(int p_caret) const {
	ERR_FAIL_INDEX_V(p_caret, carets.size(), -1);
	return carets[p_caret].selection.origin_column;
}

int TextEdit::get_selection_from_line(int p_caret) const {
	ERR_FAIL_INDEX_V(p_caret, carets.size(), -1);
	if (!has_selection(p_caret)) {
		return carets[p_caret].line;
	}
	return MIN(carets[p_caret].selection.origin_line, carets[p_caret].line);
}

int TextEdit::get_selection_from_column(int p_caret) const {
	ERR_FAIL_INDEX_V(p_caret, carets.size(), -1);
	if (!has_selection(p_caret)) {
		return carets[p_caret].column;
	}
	if (carets[p_caret].selection.origin_line < carets[p_caret].line) {
		return carets[p_caret].selection.origin_column;
	} else if (carets[p_caret].selection.origin_line > carets[p_caret].line) {
		return carets[p_caret].column;
	} else {
		return MIN(carets[p_caret].selection.origin_column, carets[p_caret].column);
	}
}

int TextEdit::get_selection_to_line(int p_caret) const {
	ERR_FAIL_INDEX_V(p_caret, carets.size(), -1);
	if (!has_selection(p_caret)) {
		return carets[p_caret].line;
	}
	return MAX(carets[p_caret].selection.origin_line, carets[p_caret].line);
}

int TextEdit::get_selection_to_column(int p_caret) const {
	ERR_FAIL_INDEX_V(p_caret, carets.size(), -1);
	if (!has_selection(p_caret)) {
		return carets[p_caret].column;
	}
	if (carets[p_caret].selection.origin_line < carets[p_caret].line) {
		return carets[p_caret].column;
	} else if (carets[p_caret].selection.origin_line > carets[p_caret].line) {
		return carets[p_caret].selection.origin_column;
	} else {
		return MAX(carets[p_caret].selection.origin_column, carets[p_caret].column);
	}
}

bool TextEdit::is_caret_after_selection_origin(int p_caret) const {
	ERR_FAIL_INDEX_V(p_caret, carets.size(), false);
	if (!has_selection(p_caret)) {
		return true;
	}
	return carets[p_caret].line > carets[p_caret].selection.origin_line || (carets[p_caret].line == carets[p_caret].selection.origin_line && carets[p_caret].column >= carets[p_caret].selection.origin_column);
}

void TextEdit::deselect(int p_caret) {
	ERR_FAIL_COND(p_caret >= carets.size() || p_caret < -1);
	bool selection_changed = false;
	if (p_caret >= 0) {
		selection_changed = carets.write[p_caret].selection.active;
		carets.write[p_caret].selection.active = false;
	} else {
		for (int i = 0; i < carets.size(); i++) {
			selection_changed |= carets.write[i].selection.active;
			carets.write[i].selection.active = false;
		}
	}
	if (selection_changed) {
		_selection_changed(p_caret);
	}
}

void TextEdit::delete_selection(int p_caret) {
	ERR_FAIL_COND(p_caret >= get_caret_count() || p_caret < -1);

	begin_complex_operation();
	begin_multicaret_edit();
	for (int i = 0; i < get_caret_count(); i++) {
		if (p_caret != -1 && p_caret != i) {
			continue;
		}
		if (p_caret == -1 && multicaret_edit_ignore_caret(i)) {
			continue;
		}

		if (!has_selection(i)) {
			continue;
		}

		int selection_from_line = get_selection_from_line(i);
		int selection_from_column = get_selection_from_column(i);
		int selection_to_line = get_selection_to_line(i);
		int selection_to_column = get_selection_to_column(i);

		_remove_text(selection_from_line, selection_from_column, selection_to_line, selection_to_column);
		_offset_carets_after(selection_to_line, selection_to_column, selection_from_line, selection_from_column);
		merge_overlapping_carets();

		deselect(i);
		set_caret_line(selection_from_line, false, false, -1, i);
		set_caret_column(selection_from_column, i == 0, i);
	}
	end_multicaret_edit();
	end_complex_operation();
}

/* Line wrapping. */
void TextEdit::set_line_wrapping_mode(LineWrappingMode p_wrapping_mode) {
	if (line_wrapping_mode != p_wrapping_mode) {
		line_wrapping_mode = p_wrapping_mode;
		_update_wrap_at_column(true);
		queue_redraw();
	}
}

TextEdit::LineWrappingMode TextEdit::get_line_wrapping_mode() const {
	return line_wrapping_mode;
}

void TextEdit::set_autowrap_mode(TextServer::AutowrapMode p_mode) {
	if (autowrap_mode == p_mode) {
		return;
	}

	autowrap_mode = p_mode;
	if (get_line_wrapping_mode() != LineWrappingMode::LINE_WRAPPING_NONE) {
		_update_wrap_at_column(true);
		queue_redraw();
	}
}

TextServer::AutowrapMode TextEdit::get_autowrap_mode() const {
	return autowrap_mode;
}

bool TextEdit::is_line_wrapped(int p_line) const {
	ERR_FAIL_INDEX_V(p_line, text.size(), false);
	if (get_line_wrapping_mode() == LineWrappingMode::LINE_WRAPPING_NONE) {
		return false;
	}
	return text.get_line_wrap_amount(p_line) > 0;
}

int TextEdit::get_line_wrap_count(int p_line) const {
	ERR_FAIL_INDEX_V(p_line, text.size(), 0);

	if (!is_line_wrapped(p_line)) {
		return 0;
	}

	return text.get_line_wrap_amount(p_line);
}

int TextEdit::get_line_wrap_index_at_column(int p_line, int p_column) const {
	ERR_FAIL_INDEX_V(p_line, text.size(), 0);
	ERR_FAIL_COND_V(p_column < 0, 0);
	ERR_FAIL_COND_V(p_column > text[p_line].length(), 0);

	if (!is_line_wrapped(p_line)) {
		return 0;
	}

	/* Loop through wraps in the line text until we get to the column. */
	int wrap_index = 0;
	int col = 0;
	Vector<String> lines = get_line_wrapped_text(p_line);
	for (int i = 0; i < lines.size(); i++) {
		wrap_index = i;
		col += lines[wrap_index].length();
		if (col > p_column) {
			break;
		}
	}
	return wrap_index;
}

Vector<String> TextEdit::get_line_wrapped_text(int p_line) const {
	ERR_FAIL_INDEX_V(p_line, text.size(), Vector<String>());

	Vector<String> lines;
	if (!is_line_wrapped(p_line)) {
		lines.push_back(text[p_line]);
		return lines;
	}

	const String &line_text = text[p_line];
	Vector<Vector2i> line_ranges = text.get_line_wrap_ranges(p_line);
	for (int i = 0; i < line_ranges.size(); i++) {
		lines.push_back(line_text.substr(line_ranges[i].x, line_ranges[i].y - line_ranges[i].x));
	}

	return lines;
}

/* Viewport */
// Scrolling.
void TextEdit::set_smooth_scroll_enabled(bool p_enabled) {
	v_scroll->set_smooth_scroll_enabled(p_enabled);
	smooth_scroll_enabled = p_enabled;
}

bool TextEdit::is_smooth_scroll_enabled() const {
	return smooth_scroll_enabled;
}

void TextEdit::set_scroll_past_end_of_file_enabled(bool p_enabled) {
	if (scroll_past_end_of_file_enabled == p_enabled) {
		return;
	}

	scroll_past_end_of_file_enabled = p_enabled;
	queue_redraw();
}

bool TextEdit::is_scroll_past_end_of_file_enabled() const {
	return scroll_past_end_of_file_enabled;
}

VScrollBar *TextEdit::get_v_scroll_bar() const {
	return v_scroll;
}

HScrollBar *TextEdit::get_h_scroll_bar() const {
	return h_scroll;
}

void TextEdit::set_v_scroll(double p_scroll) {
	v_scroll->set_value(p_scroll);
	int max_v_scroll = v_scroll->get_max() - v_scroll->get_page();
	if (p_scroll >= max_v_scroll - 1.0) {
		_scroll_moved(v_scroll->get_value());
	}
}

double TextEdit::get_v_scroll() const {
	return v_scroll->get_value();
}

void TextEdit::set_h_scroll(int p_scroll) {
	if (p_scroll < 0) {
		p_scroll = 0;
	}
	h_scroll->set_value(p_scroll);
}

int TextEdit::get_h_scroll() const {
	return h_scroll->get_value();
}

void TextEdit::set_v_scroll_speed(float p_speed) {
	// Prevent setting a vertical scroll speed value under 1.
	ERR_FAIL_COND(p_speed < 1.0);
	v_scroll_speed = p_speed;
}

float TextEdit::get_v_scroll_speed() const {
	return v_scroll_speed;
}

void TextEdit::set_fit_content_height_enabled(bool p_enabled) {
	if (fit_content_height == p_enabled) {
		return;
	}
	fit_content_height = p_enabled;
	update_minimum_size();
}

bool TextEdit::is_fit_content_height_enabled() const {
	return fit_content_height;
}

void TextEdit::set_fit_content_width_enabled(bool p_enabled) {
	if (fit_content_width == p_enabled) {
		return;
	}
	fit_content_width = p_enabled;
	update_minimum_size();
}

bool TextEdit::is_fit_content_width_enabled() const {
	return fit_content_width;
}

double TextEdit::get_scroll_pos_for_line(int p_line, int p_wrap_index) const {
	ERR_FAIL_INDEX_V(p_line, text.size(), 0);
	ERR_FAIL_COND_V(p_wrap_index < 0, 0);
	ERR_FAIL_COND_V(p_wrap_index > get_line_wrap_count(p_line), 0);

	if (get_line_wrapping_mode() == LineWrappingMode::LINE_WRAPPING_NONE && !_is_hiding_enabled()) {
		return p_line;
	}

	double new_line_scroll_pos = 0.0;
	if (p_line > 0) {
		new_line_scroll_pos = get_visible_line_count_in_range(0, MIN(p_line - 1, text.size() - 1));
	}
	new_line_scroll_pos += p_wrap_index;
	return new_line_scroll_pos;
}

// Visible lines.
void TextEdit::set_line_as_first_visible(int p_line, int p_wrap_index) {
	ERR_FAIL_INDEX(p_line, text.size());
	ERR_FAIL_COND(p_wrap_index < 0);
	ERR_FAIL_COND(p_wrap_index > get_line_wrap_count(p_line));
	set_v_scroll(get_scroll_pos_for_line(p_line, p_wrap_index));
}

int TextEdit::get_first_visible_line() const {
	return CLAMP(first_visible_line, 0, text.size() - 1);
}

void TextEdit::set_line_as_center_visible(int p_line, int p_wrap_index) {
	ERR_FAIL_INDEX(p_line, text.size());
	ERR_FAIL_COND(p_wrap_index < 0);
	ERR_FAIL_COND(p_wrap_index > get_line_wrap_count(p_line));

	int visible_rows = get_visible_line_count();
	Point2i next_line = get_next_visible_line_index_offset_from(p_line, p_wrap_index, (-visible_rows / 2) - 1);
	int first_line = p_line - next_line.x + 1;

	if (first_line < 0) {
		set_v_scroll(0);
		return;
	}
	set_v_scroll(get_scroll_pos_for_line(first_line, next_line.y));
}

void TextEdit::set_line_as_last_visible(int p_line, int p_wrap_index) {
	ERR_FAIL_INDEX(p_line, text.size());
	ERR_FAIL_COND(p_wrap_index < 0);
	ERR_FAIL_COND(p_wrap_index > get_line_wrap_count(p_line));

	Point2i next_line = get_next_visible_line_index_offset_from(p_line, p_wrap_index, -get_visible_line_count() - 1);
	int first_line = p_line - next_line.x + 1;

	// Adding _get_visible_lines_offset is not 100% correct as we end up showing almost p_line + 1, however, it provides a
	// better user experience. Therefore we need to special case < visible line count, else showing line 0 is impossible.
	if (get_visible_line_count_in_range(0, p_line) < get_visible_line_count() + 1) {
		set_v_scroll(0);
		return;
	}
	set_v_scroll(Math::round(get_scroll_pos_for_line(first_line, next_line.y) + _get_visible_lines_offset()));
}

int TextEdit::get_last_full_visible_line() const {
	int first_vis_line = get_first_visible_line();
	int last_vis_line = 0;
	last_vis_line = first_vis_line + get_next_visible_line_index_offset_from(first_vis_line, first_visible_line_wrap_ofs, get_visible_line_count()).x - 1;
	last_vis_line = CLAMP(last_vis_line, 0, text.size() - 1);
	return last_vis_line;
}

int TextEdit::get_last_full_visible_line_wrap_index() const {
	int first_vis_line = get_first_visible_line();
	return get_next_visible_line_index_offset_from(first_vis_line, first_visible_line_wrap_ofs, get_visible_line_count()).y;
}

int TextEdit::get_visible_line_count() const {
	return _get_control_height() / get_line_height();
}

int TextEdit::get_visible_line_count_in_range(int p_from_line, int p_to_line) const {
	ERR_FAIL_INDEX_V(p_from_line, text.size(), 0);
	ERR_FAIL_INDEX_V(p_to_line, text.size(), 0);

	// So we can handle inputs in whatever order.
	if (p_from_line > p_to_line) {
		SWAP(p_from_line, p_to_line);
	}

	// Returns the total number of (lines + wrapped - hidden).
	if (!_is_hiding_enabled() && get_line_wrapping_mode() == LineWrappingMode::LINE_WRAPPING_NONE) {
		return (p_to_line - p_from_line) + 1;
	}

	int total_rows = 0;
	for (int i = p_from_line; i <= p_to_line; i++) {
		if (!text.is_hidden(i)) {
			total_rows++;
			total_rows += get_line_wrap_count(i);
		}
	}
	return total_rows;
}

int TextEdit::get_total_visible_line_count() const {
	return text.get_total_visible_line_count();
}

// Auto adjust.
void TextEdit::adjust_viewport_to_caret(int p_caret) {
	ERR_FAIL_INDEX(p_caret, carets.size());

	// Make sure Caret is visible on the screen.
	scrolling = false;
	minimap_clicked = false;

	int cur_line = get_caret_line(p_caret);
	int cur_wrap = get_caret_wrap_index(p_caret);

	int first_vis_line = get_first_visible_line();
	int first_vis_wrap = first_visible_line_wrap_ofs;
	int last_vis_line = get_last_full_visible_line();
	int last_vis_wrap = get_last_full_visible_line_wrap_index();

	if (cur_line < first_vis_line || (cur_line == first_vis_line && cur_wrap < first_vis_wrap)) {
		// Caret is above screen.
		set_line_as_first_visible(cur_line, cur_wrap);
	} else if (cur_line > last_vis_line || (cur_line == last_vis_line && cur_wrap > last_vis_wrap)) {
		// Caret is below screen.
		set_line_as_last_visible(cur_line, cur_wrap);
	}

	int visible_width = get_size().width - theme_cache.style_normal->get_minimum_size().width - gutters_width - gutter_padding;
	if (draw_minimap) {
		visible_width -= minimap_width;
	}
	if (v_scroll->is_visible_in_tree()) {
		visible_width -= v_scroll->get_combined_minimum_size().width;
	}
	visible_width -= 20; // Give it a little more space.

	if (visible_width <= 0) {
		// Not resized yet.
		return;
	}

	Vector2i caret_pos;

	// Get position of the start of caret.
	if (has_ime_text() && ime_selection.x != 0) {
		caret_pos.x = _get_column_x_offset_for_line(get_caret_column(p_caret) + ime_selection.x, get_caret_line(p_caret), get_caret_column(p_caret));
	} else {
		caret_pos.x = _get_column_x_offset_for_line(get_caret_column(p_caret), get_caret_line(p_caret), get_caret_column(p_caret));
	}

	// Get position of the end of caret.
	if (has_ime_text()) {
		if (ime_selection.y > 0) {
			caret_pos.y = _get_column_x_offset_for_line(get_caret_column(p_caret) + ime_selection.x + ime_selection.y, get_caret_line(p_caret), get_caret_column(p_caret));
		} else {
			caret_pos.y = _get_column_x_offset_for_line(get_caret_column(p_caret) + ime_text.size(), get_caret_line(p_caret), get_caret_column(p_caret));
		}
	} else {
		caret_pos.y = caret_pos.x;
	}

	if (MAX(caret_pos.x, caret_pos.y) > (first_visible_col + visible_width)) {
		first_visible_col = MAX(caret_pos.x, caret_pos.y) - visible_width + 1;
	}

	if (MIN(caret_pos.x, caret_pos.y) < first_visible_col) {
		first_visible_col = MIN(caret_pos.x, caret_pos.y);
	}
	h_scroll->set_value(first_visible_col);

	queue_redraw();
}

void TextEdit::center_viewport_to_caret(int p_caret) {
	ERR_FAIL_INDEX(p_caret, carets.size());

	// Move viewport so the caret is in the center of the screen.
	scrolling = false;
	minimap_clicked = false;

	set_line_as_center_visible(get_caret_line(p_caret), get_caret_wrap_index(p_caret));
	int visible_width = get_size().width - theme_cache.style_normal->get_minimum_size().width - gutters_width - gutter_padding;
	if (draw_minimap) {
		visible_width -= minimap_width;
	}
	if (v_scroll->is_visible_in_tree()) {
		visible_width -= v_scroll->get_combined_minimum_size().width;
	}
	visible_width -= 20; // Give it a little more space.

	if (get_line_wrapping_mode() != LineWrappingMode::LINE_WRAPPING_NONE) {
		// Center x offset.

		Vector2i caret_pos;

		// Get position of the start of caret.
		if (has_ime_text() && ime_selection.x != 0) {
			caret_pos.x = _get_column_x_offset_for_line(get_caret_column(p_caret) + ime_selection.x, get_caret_line(p_caret), get_caret_column(p_caret));
		} else {
			caret_pos.x = _get_column_x_offset_for_line(get_caret_column(p_caret), get_caret_line(p_caret), get_caret_column(p_caret));
		}

		// Get position of the end of caret.
		if (has_ime_text()) {
			if (ime_selection.y > 0) {
				caret_pos.y = _get_column_x_offset_for_line(get_caret_column(p_caret) + ime_selection.x + ime_selection.y, get_caret_line(p_caret), get_caret_column(p_caret));
			} else {
				caret_pos.y = _get_column_x_offset_for_line(get_caret_column(p_caret) + ime_text.size(), get_caret_line(p_caret), get_caret_column(p_caret));
			}
		} else {
			caret_pos.y = caret_pos.x;
		}

		if (MAX(caret_pos.x, caret_pos.y) > (first_visible_col + visible_width)) {
			first_visible_col = MAX(caret_pos.x, caret_pos.y) - visible_width + 1;
		}

		if (MIN(caret_pos.x, caret_pos.y) < first_visible_col) {
			first_visible_col = MIN(caret_pos.x, caret_pos.y);
		}
	} else {
		first_visible_col = 0;
	}
	h_scroll->set_value(first_visible_col);

	queue_redraw();
}

/* Minimap */
void TextEdit::set_draw_minimap(bool p_enabled) {
	if (draw_minimap == p_enabled) {
		return;
	}

	draw_minimap = p_enabled;
	_update_wrap_at_column();
	queue_redraw();
}

bool TextEdit::is_drawing_minimap() const {
	return draw_minimap;
}

void TextEdit::set_minimap_width(int p_minimap_width) {
	if (minimap_width == p_minimap_width) {
		return;
	}

	minimap_width = p_minimap_width;
	_update_wrap_at_column();
	queue_redraw();
}

int TextEdit::get_minimap_width() const {
	return minimap_width;
}

int TextEdit::get_minimap_visible_lines() const {
	return _get_control_height() / (minimap_char_size.y + minimap_line_spacing);
}

/* Gutters. */
void TextEdit::add_gutter(int p_at) {
	if (p_at < 0 || p_at > gutters.size()) {
		gutters.push_back(GutterInfo());
	} else {
		gutters.insert(p_at, GutterInfo());
	}

	text.add_gutter(p_at);

	_update_gutter_width();

	emit_signal(SNAME("gutter_added"));
	queue_redraw();
}

void TextEdit::remove_gutter(int p_gutter) {
	ERR_FAIL_INDEX(p_gutter, gutters.size());

	gutters.remove_at(p_gutter);

	text.remove_gutter(p_gutter);

	_update_gutter_width();

	emit_signal(SNAME("gutter_removed"));
	queue_redraw();
}

int TextEdit::get_gutter_count() const {
	return gutters.size();
}

void TextEdit::set_gutter_name(int p_gutter, const String &p_name) {
	ERR_FAIL_INDEX(p_gutter, gutters.size());
	gutters.write[p_gutter].name = p_name;
}

String TextEdit::get_gutter_name(int p_gutter) const {
	ERR_FAIL_INDEX_V(p_gutter, gutters.size(), "");
	return gutters[p_gutter].name;
}

void TextEdit::set_gutter_type(int p_gutter, GutterType p_type) {
	ERR_FAIL_INDEX(p_gutter, gutters.size());

	if (gutters[p_gutter].type == p_type) {
		return;
	}

	gutters.write[p_gutter].type = p_type;
	queue_redraw();
}

TextEdit::GutterType TextEdit::get_gutter_type(int p_gutter) const {
	ERR_FAIL_INDEX_V(p_gutter, gutters.size(), GUTTER_TYPE_STRING);
	return gutters[p_gutter].type;
}

void TextEdit::set_gutter_width(int p_gutter, int p_width) {
	ERR_FAIL_INDEX(p_gutter, gutters.size());
	if (gutters[p_gutter].width == p_width) {
		return;
	}
	gutters.write[p_gutter].width = p_width;
	_update_gutter_width();
}

int TextEdit::get_gutter_width(int p_gutter) const {
	ERR_FAIL_INDEX_V(p_gutter, gutters.size(), -1);
	return gutters[p_gutter].width;
}

int TextEdit::get_total_gutter_width() const {
	return gutters_width + gutter_padding;
}

void TextEdit::set_gutter_draw(int p_gutter, bool p_draw) {
	ERR_FAIL_INDEX(p_gutter, gutters.size());
	if (gutters[p_gutter].draw == p_draw) {
		return;
	}
	gutters.write[p_gutter].draw = p_draw;
	_update_gutter_width();
}

bool TextEdit::is_gutter_drawn(int p_gutter) const {
	ERR_FAIL_INDEX_V(p_gutter, gutters.size(), false);
	return gutters[p_gutter].draw;
}

void TextEdit::set_gutter_clickable(int p_gutter, bool p_clickable) {
	ERR_FAIL_INDEX(p_gutter, gutters.size());

	if (gutters[p_gutter].clickable == p_clickable) {
		return;
	}

	gutters.write[p_gutter].clickable = p_clickable;
	queue_redraw();
}

bool TextEdit::is_gutter_clickable(int p_gutter) const {
	ERR_FAIL_INDEX_V(p_gutter, gutters.size(), false);
	return gutters[p_gutter].clickable;
}

void TextEdit::set_gutter_overwritable(int p_gutter, bool p_overwritable) {
	ERR_FAIL_INDEX(p_gutter, gutters.size());
	gutters.write[p_gutter].overwritable = p_overwritable;
}

bool TextEdit::is_gutter_overwritable(int p_gutter) const {
	ERR_FAIL_INDEX_V(p_gutter, gutters.size(), false);
	return gutters[p_gutter].overwritable;
}

void TextEdit::merge_gutters(int p_from_line, int p_to_line) {
	ERR_FAIL_INDEX(p_from_line, text.size());
	ERR_FAIL_INDEX(p_to_line, text.size());
	if (p_from_line == p_to_line) {
		return;
	}

	for (int i = 0; i < gutters.size(); i++) {
		if (!gutters[i].overwritable) {
			continue;
		}

		if (text.get_line_gutter_text(p_from_line, i) != "") {
			text.set_line_gutter_text(p_to_line, i, text.get_line_gutter_text(p_from_line, i));
			text.set_line_gutter_item_color(p_to_line, i, text.get_line_gutter_item_color(p_from_line, i));
		}

		if (text.get_line_gutter_icon(p_from_line, i).is_valid()) {
			text.set_line_gutter_icon(p_to_line, i, text.get_line_gutter_icon(p_from_line, i));
			text.set_line_gutter_item_color(p_to_line, i, text.get_line_gutter_item_color(p_from_line, i));
		}

		if (text.get_line_gutter_metadata(p_from_line, i) != "") {
			text.set_line_gutter_metadata(p_to_line, i, text.get_line_gutter_metadata(p_from_line, i));
		}

		if (text.is_line_gutter_clickable(p_from_line, i)) {
			text.set_line_gutter_clickable(p_to_line, i, true);
		}
	}
	queue_redraw();
}

void TextEdit::set_gutter_custom_draw(int p_gutter, const Callable &p_draw_callback) {
	ERR_FAIL_INDEX(p_gutter, gutters.size());

	if (gutters[p_gutter].custom_draw_callback == p_draw_callback) {
		return;
	}

	gutters.write[p_gutter].custom_draw_callback = p_draw_callback;
	queue_redraw();
}

// Line gutters.
void TextEdit::set_line_gutter_metadata(int p_line, int p_gutter, const Variant &p_metadata) {
	ERR_FAIL_INDEX(p_line, text.size());
	ERR_FAIL_INDEX(p_gutter, gutters.size());
	text.set_line_gutter_metadata(p_line, p_gutter, p_metadata);
}

Variant TextEdit::get_line_gutter_metadata(int p_line, int p_gutter) const {
	ERR_FAIL_INDEX_V(p_line, text.size(), "");
	ERR_FAIL_INDEX_V(p_gutter, gutters.size(), "");
	return text.get_line_gutter_metadata(p_line, p_gutter);
}

void TextEdit::set_line_gutter_text(int p_line, int p_gutter, const String &p_text) {
	ERR_FAIL_INDEX(p_line, text.size());
	ERR_FAIL_INDEX(p_gutter, gutters.size());

	if (text.get_line_gutter_text(p_line, p_gutter) == p_text) {
		return;
	}

	text.set_line_gutter_text(p_line, p_gutter, p_text);
	queue_redraw();
}

String TextEdit::get_line_gutter_text(int p_line, int p_gutter) const {
	ERR_FAIL_INDEX_V(p_line, text.size(), "");
	ERR_FAIL_INDEX_V(p_gutter, gutters.size(), "");
	return text.get_line_gutter_text(p_line, p_gutter);
}

void TextEdit::set_line_gutter_icon(int p_line, int p_gutter, const Ref<Texture2D> &p_icon) {
	ERR_FAIL_INDEX(p_line, text.size());
	ERR_FAIL_INDEX(p_gutter, gutters.size());

	if (text.get_line_gutter_icon(p_line, p_gutter) == p_icon) {
		return;
	}

	text.set_line_gutter_icon(p_line, p_gutter, p_icon);
	queue_redraw();
}

Ref<Texture2D> TextEdit::get_line_gutter_icon(int p_line, int p_gutter) const {
	ERR_FAIL_INDEX_V(p_line, text.size(), Ref<Texture2D>());
	ERR_FAIL_INDEX_V(p_gutter, gutters.size(), Ref<Texture2D>());
	return text.get_line_gutter_icon(p_line, p_gutter);
}

void TextEdit::set_line_gutter_item_color(int p_line, int p_gutter, const Color &p_color) {
	ERR_FAIL_INDEX(p_line, text.size());
	ERR_FAIL_INDEX(p_gutter, gutters.size());

	if (text.get_line_gutter_item_color(p_line, p_gutter) == p_color) {
		return;
	}

	text.set_line_gutter_item_color(p_line, p_gutter, p_color);
	queue_redraw();
}

Color TextEdit::get_line_gutter_item_color(int p_line, int p_gutter) const {
	ERR_FAIL_INDEX_V(p_line, text.size(), Color());
	ERR_FAIL_INDEX_V(p_gutter, gutters.size(), Color());
	return text.get_line_gutter_item_color(p_line, p_gutter);
}

void TextEdit::set_line_gutter_clickable(int p_line, int p_gutter, bool p_clickable) {
	ERR_FAIL_INDEX(p_line, text.size());
	ERR_FAIL_INDEX(p_gutter, gutters.size());
	text.set_line_gutter_clickable(p_line, p_gutter, p_clickable);
}

bool TextEdit::is_line_gutter_clickable(int p_line, int p_gutter) const {
	ERR_FAIL_INDEX_V(p_line, text.size(), false);
	ERR_FAIL_INDEX_V(p_gutter, gutters.size(), false);
	return text.is_line_gutter_clickable(p_line, p_gutter);
}

// Line style
void TextEdit::set_line_background_color(int p_line, const Color &p_color) {
	ERR_FAIL_INDEX(p_line, text.size());

	if (text.get_line_background_color(p_line) == p_color) {
		return;
	}

	text.set_line_background_color(p_line, p_color);
	queue_redraw();
}

Color TextEdit::get_line_background_color(int p_line) const {
	ERR_FAIL_INDEX_V(p_line, text.size(), Color());
	return text.get_line_background_color(p_line);
}

/* Syntax Highlighting. */
void TextEdit::set_syntax_highlighter(Ref<SyntaxHighlighter> p_syntax_highlighter) {
	if (syntax_highlighter == p_syntax_highlighter && syntax_highlighter.is_valid() == p_syntax_highlighter.is_valid()) {
		return;
	}

	syntax_highlighter = p_syntax_highlighter;
	if (syntax_highlighter.is_valid()) {
		syntax_highlighter->set_text_edit(this);
	}
	_clear_syntax_highlighting_cache();
	queue_redraw();
}

Ref<SyntaxHighlighter> TextEdit::get_syntax_highlighter() const {
	return syntax_highlighter;
}

/* Visual. */
void TextEdit::set_highlight_current_line(bool p_enabled) {
	if (highlight_current_line == p_enabled) {
		return;
	}

	highlight_current_line = p_enabled;
	queue_redraw();
}

bool TextEdit::is_highlight_current_line_enabled() const {
	return highlight_current_line;
}

void TextEdit::set_highlight_all_occurrences(bool p_enabled) {
	if (highlight_all_occurrences == p_enabled) {
		return;
	}

	highlight_all_occurrences = p_enabled;
	queue_redraw();
}

bool TextEdit::is_highlight_all_occurrences_enabled() const {
	return highlight_all_occurrences;
}

void TextEdit::set_use_default_word_separators(bool p_enabled) {
	text.set_use_default_word_separators(p_enabled);
}

bool TextEdit::is_default_word_separators_enabled() const {
	return text.is_default_word_separators_enabled();
}

// Set word separators. Combine default separators with custom separators if those options are enabled.
void TextEdit::set_custom_word_separators(const String &p_separators) {
	text.set_custom_word_separators(p_separators);
}

void TextEdit::Text::set_custom_word_separators(const String &p_separators) {
	if (custom_word_separators == p_separators) {
		return;
	}
	custom_word_separators = p_separators;
	invalidate_all_lines();
}

bool TextEdit::is_custom_word_separators_enabled() const {
	return text.is_custom_word_separators_enabled();
}

String TextEdit::get_custom_word_separators() const {
	return text.get_custom_word_separators();
}

// Enable or disable custom word separators.
void TextEdit::set_use_custom_word_separators(bool p_enabled) {
	text.set_use_custom_word_separators(p_enabled);
}

String TextEdit::get_default_word_separators() const {
	return text.get_default_word_separators();
}

void TextEdit::set_draw_control_chars(bool p_enabled) {
	if (draw_control_chars != p_enabled) {
		draw_control_chars = p_enabled;
		if (menu) {
			menu->set_item_checked(menu->get_item_index(MENU_DISPLAY_UCC), draw_control_chars);
		}
		text.set_draw_control_chars(draw_control_chars);
		text.invalidate_font();
		_update_placeholder();
		queue_redraw();
	}
}

bool TextEdit::get_draw_control_chars() const {
	return draw_control_chars;
}

void TextEdit::set_draw_tabs(bool p_enabled) {
	if (draw_tabs == p_enabled) {
		return;
	}

	draw_tabs = p_enabled;
	queue_redraw();
}

bool TextEdit::is_drawing_tabs() const {
	return draw_tabs;
}

void TextEdit::set_draw_spaces(bool p_enabled) {
	if (draw_spaces == p_enabled) {
		return;
	}

	draw_spaces = p_enabled;
	queue_redraw();
}

bool TextEdit::is_drawing_spaces() const {
	return draw_spaces;
}

Color TextEdit::get_font_color() const {
	return theme_cache.font_color;
}

void TextEdit::_bind_methods() {
	/* Text */
	// Text properties
	ClassDB::bind_method(D_METHOD("has_ime_text"), &TextEdit::has_ime_text);
	ClassDB::bind_method(D_METHOD("cancel_ime"), &TextEdit::cancel_ime);
	ClassDB::bind_method(D_METHOD("apply_ime"), &TextEdit::apply_ime);

	ClassDB::bind_method(D_METHOD("set_editable", "enabled"), &TextEdit::set_editable);
	ClassDB::bind_method(D_METHOD("is_editable"), &TextEdit::is_editable);

	ClassDB::bind_method(D_METHOD("set_text_direction", "direction"), &TextEdit::set_text_direction);
	ClassDB::bind_method(D_METHOD("get_text_direction"), &TextEdit::get_text_direction);

	ClassDB::bind_method(D_METHOD("set_language", "language"), &TextEdit::set_language);
	ClassDB::bind_method(D_METHOD("get_language"), &TextEdit::get_language);

	ClassDB::bind_method(D_METHOD("set_structured_text_bidi_override", "parser"), &TextEdit::set_structured_text_bidi_override);
	ClassDB::bind_method(D_METHOD("get_structured_text_bidi_override"), &TextEdit::get_structured_text_bidi_override);
	ClassDB::bind_method(D_METHOD("set_structured_text_bidi_override_options", "args"), &TextEdit::set_structured_text_bidi_override_options);
	ClassDB::bind_method(D_METHOD("get_structured_text_bidi_override_options"), &TextEdit::get_structured_text_bidi_override_options);

	ClassDB::bind_method(D_METHOD("set_tab_size", "size"), &TextEdit::set_tab_size);
	ClassDB::bind_method(D_METHOD("get_tab_size"), &TextEdit::get_tab_size);

	ClassDB::bind_method(D_METHOD("set_indent_wrapped_lines", "enabled"), &TextEdit::set_indent_wrapped_lines);
	ClassDB::bind_method(D_METHOD("is_indent_wrapped_lines"), &TextEdit::is_indent_wrapped_lines);

	// User controls
	ClassDB::bind_method(D_METHOD("set_overtype_mode_enabled", "enabled"), &TextEdit::set_overtype_mode_enabled);
	ClassDB::bind_method(D_METHOD("is_overtype_mode_enabled"), &TextEdit::is_overtype_mode_enabled);

	ClassDB::bind_method(D_METHOD("set_context_menu_enabled", "enabled"), &TextEdit::set_context_menu_enabled);
	ClassDB::bind_method(D_METHOD("is_context_menu_enabled"), &TextEdit::is_context_menu_enabled);

	ClassDB::bind_method(D_METHOD("set_shortcut_keys_enabled", "enabled"), &TextEdit::set_shortcut_keys_enabled);
	ClassDB::bind_method(D_METHOD("is_shortcut_keys_enabled"), &TextEdit::is_shortcut_keys_enabled);

	ClassDB::bind_method(D_METHOD("set_virtual_keyboard_enabled", "enabled"), &TextEdit::set_virtual_keyboard_enabled);
	ClassDB::bind_method(D_METHOD("is_virtual_keyboard_enabled"), &TextEdit::is_virtual_keyboard_enabled);

	ClassDB::bind_method(D_METHOD("set_middle_mouse_paste_enabled", "enabled"), &TextEdit::set_middle_mouse_paste_enabled);
	ClassDB::bind_method(D_METHOD("is_middle_mouse_paste_enabled"), &TextEdit::is_middle_mouse_paste_enabled);

	ClassDB::bind_method(D_METHOD("set_empty_selection_clipboard_enabled", "enabled"), &TextEdit::set_empty_selection_clipboard_enabled);
	ClassDB::bind_method(D_METHOD("is_empty_selection_clipboard_enabled"), &TextEdit::is_empty_selection_clipboard_enabled);

	// Text manipulation
	ClassDB::bind_method(D_METHOD("clear"), &TextEdit::clear);

	ClassDB::bind_method(D_METHOD("set_text", "text"), &TextEdit::set_text);
	ClassDB::bind_method(D_METHOD("get_text"), &TextEdit::get_text);

	ClassDB::bind_method(D_METHOD("get_line_count"), &TextEdit::get_line_count);

	ClassDB::bind_method(D_METHOD("set_placeholder", "text"), &TextEdit::set_placeholder);
	ClassDB::bind_method(D_METHOD("get_placeholder"), &TextEdit::get_placeholder);

	ClassDB::bind_method(D_METHOD("set_line", "line", "new_text"), &TextEdit::set_line);
	ClassDB::bind_method(D_METHOD("get_line", "line"), &TextEdit::get_line);

	ClassDB::bind_method(D_METHOD("get_line_width", "line", "wrap_index"), &TextEdit::get_line_width, DEFVAL(-1));
	ClassDB::bind_method(D_METHOD("get_line_height"), &TextEdit::get_line_height);

	ClassDB::bind_method(D_METHOD("get_indent_level", "line"), &TextEdit::get_indent_level);
	ClassDB::bind_method(D_METHOD("get_first_non_whitespace_column", "line"), &TextEdit::get_first_non_whitespace_column);

	ClassDB::bind_method(D_METHOD("swap_lines", "from_line", "to_line"), &TextEdit::swap_lines);

	ClassDB::bind_method(D_METHOD("insert_line_at", "line", "text"), &TextEdit::insert_line_at);
	ClassDB::bind_method(D_METHOD("remove_line_at", "line", "move_carets_down"), &TextEdit::remove_line_at, DEFVAL(true));
	ClassDB::bind_method(D_METHOD("insert_text_at_caret", "text", "caret_index"), &TextEdit::insert_text_at_caret, DEFVAL(-1));

	ClassDB::bind_method(D_METHOD("insert_text", "text", "line", "column", "before_selection_begin", "before_selection_end"), &TextEdit::insert_text, DEFVAL(true), DEFVAL(false));
	ClassDB::bind_method(D_METHOD("remove_text", "from_line", "from_column", "to_line", "to_column"), &TextEdit::remove_text);

	ClassDB::bind_method(D_METHOD("get_last_unhidden_line"), &TextEdit::get_last_unhidden_line);
	ClassDB::bind_method(D_METHOD("get_next_visible_line_offset_from", "line", "visible_amount"), &TextEdit::get_next_visible_line_offset_from);
	ClassDB::bind_method(D_METHOD("get_next_visible_line_index_offset_from", "line", "wrap_index", "visible_amount"), &TextEdit::get_next_visible_line_index_offset_from);

	// Overridable actions
	ClassDB::bind_method(D_METHOD("backspace", "caret_index"), &TextEdit::backspace, DEFVAL(-1));

	ClassDB::bind_method(D_METHOD("cut", "caret_index"), &TextEdit::cut, DEFVAL(-1));
	ClassDB::bind_method(D_METHOD("copy", "caret_index"), &TextEdit::copy, DEFVAL(-1));
	ClassDB::bind_method(D_METHOD("paste", "caret_index"), &TextEdit::paste, DEFVAL(-1));
	ClassDB::bind_method(D_METHOD("paste_primary_clipboard", "caret_index"), &TextEdit::paste_primary_clipboard, DEFVAL(-1));

	GDVIRTUAL_BIND(_handle_unicode_input, "unicode_char", "caret_index")
	GDVIRTUAL_BIND(_backspace, "caret_index")
	GDVIRTUAL_BIND(_cut, "caret_index")
	GDVIRTUAL_BIND(_copy, "caret_index")
	GDVIRTUAL_BIND(_paste, "caret_index")
	GDVIRTUAL_BIND(_paste_primary_clipboard, "caret_index")

	// Context Menu
	BIND_ENUM_CONSTANT(MENU_CUT);
	BIND_ENUM_CONSTANT(MENU_COPY);
	BIND_ENUM_CONSTANT(MENU_PASTE);
	BIND_ENUM_CONSTANT(MENU_CLEAR);
	BIND_ENUM_CONSTANT(MENU_SELECT_ALL);
	BIND_ENUM_CONSTANT(MENU_UNDO);
	BIND_ENUM_CONSTANT(MENU_REDO);
	BIND_ENUM_CONSTANT(MENU_SUBMENU_TEXT_DIR);
	BIND_ENUM_CONSTANT(MENU_DIR_INHERITED);
	BIND_ENUM_CONSTANT(MENU_DIR_AUTO);
	BIND_ENUM_CONSTANT(MENU_DIR_LTR);
	BIND_ENUM_CONSTANT(MENU_DIR_RTL);
	BIND_ENUM_CONSTANT(MENU_DISPLAY_UCC);
	BIND_ENUM_CONSTANT(MENU_SUBMENU_INSERT_UCC);
	BIND_ENUM_CONSTANT(MENU_INSERT_LRM);
	BIND_ENUM_CONSTANT(MENU_INSERT_RLM);
	BIND_ENUM_CONSTANT(MENU_INSERT_LRE);
	BIND_ENUM_CONSTANT(MENU_INSERT_RLE);
	BIND_ENUM_CONSTANT(MENU_INSERT_LRO);
	BIND_ENUM_CONSTANT(MENU_INSERT_RLO);
	BIND_ENUM_CONSTANT(MENU_INSERT_PDF);
	BIND_ENUM_CONSTANT(MENU_INSERT_ALM);
	BIND_ENUM_CONSTANT(MENU_INSERT_LRI);
	BIND_ENUM_CONSTANT(MENU_INSERT_RLI);
	BIND_ENUM_CONSTANT(MENU_INSERT_FSI);
	BIND_ENUM_CONSTANT(MENU_INSERT_PDI);
	BIND_ENUM_CONSTANT(MENU_INSERT_ZWJ);
	BIND_ENUM_CONSTANT(MENU_INSERT_ZWNJ);
	BIND_ENUM_CONSTANT(MENU_INSERT_WJ);
	BIND_ENUM_CONSTANT(MENU_INSERT_SHY);
	BIND_ENUM_CONSTANT(MENU_MAX);

	/* Versioning */
	BIND_ENUM_CONSTANT(ACTION_NONE);
	BIND_ENUM_CONSTANT(ACTION_TYPING);
	BIND_ENUM_CONSTANT(ACTION_BACKSPACE);
	BIND_ENUM_CONSTANT(ACTION_DELETE);

	ClassDB::bind_method(D_METHOD("start_action", "action"), &TextEdit::start_action);
	ClassDB::bind_method(D_METHOD("end_action"), &TextEdit::end_complex_operation);

	ClassDB::bind_method(D_METHOD("begin_complex_operation"), &TextEdit::begin_complex_operation);
	ClassDB::bind_method(D_METHOD("end_complex_operation"), &TextEdit::end_complex_operation);

	ClassDB::bind_method(D_METHOD("has_undo"), &TextEdit::has_undo);
	ClassDB::bind_method(D_METHOD("has_redo"), &TextEdit::has_redo);
	ClassDB::bind_method(D_METHOD("undo"), &TextEdit::undo);
	ClassDB::bind_method(D_METHOD("redo"), &TextEdit::redo);
	ClassDB::bind_method(D_METHOD("clear_undo_history"), &TextEdit::clear_undo_history);

	ClassDB::bind_method(D_METHOD("tag_saved_version"), &TextEdit::tag_saved_version);

	ClassDB::bind_method(D_METHOD("get_version"), &TextEdit::get_version);
	ClassDB::bind_method(D_METHOD("get_saved_version"), &TextEdit::get_saved_version);

	/* Search */
	BIND_ENUM_CONSTANT(SEARCH_MATCH_CASE);
	BIND_ENUM_CONSTANT(SEARCH_WHOLE_WORDS);
	BIND_ENUM_CONSTANT(SEARCH_BACKWARDS);

	ClassDB::bind_method(D_METHOD("set_search_text", "search_text"), &TextEdit::set_search_text);
	ClassDB::bind_method(D_METHOD("set_search_flags", "flags"), &TextEdit::set_search_flags);

	ClassDB::bind_method(D_METHOD("search", "text", "flags", "from_line", "from_column"), &TextEdit::search);

	/* Tooltip */
	ClassDB::bind_method(D_METHOD("set_tooltip_request_func", "callback"), &TextEdit::set_tooltip_request_func);

	/* Mouse */
	ClassDB::bind_method(D_METHOD("get_local_mouse_pos"), &TextEdit::get_local_mouse_pos);

	ClassDB::bind_method(D_METHOD("get_word_at_pos", "position"), &TextEdit::get_word_at_pos);

	ClassDB::bind_method(D_METHOD("get_line_column_at_pos", "position", "allow_out_of_bounds"), &TextEdit::get_line_column_at_pos, DEFVAL(true));
	ClassDB::bind_method(D_METHOD("get_pos_at_line_column", "line", "column"), &TextEdit::get_pos_at_line_column);
	ClassDB::bind_method(D_METHOD("get_rect_at_line_column", "line", "column"), &TextEdit::get_rect_at_line_column);

	ClassDB::bind_method(D_METHOD("get_minimap_line_at_pos", "position"), &TextEdit::get_minimap_line_at_pos);

	ClassDB::bind_method(D_METHOD("is_dragging_cursor"), &TextEdit::is_dragging_cursor);
	ClassDB::bind_method(D_METHOD("is_mouse_over_selection", "edges", "caret_index"), &TextEdit::is_mouse_over_selection, DEFVAL(-1));

	/* Caret. */
	BIND_ENUM_CONSTANT(CARET_TYPE_LINE);
	BIND_ENUM_CONSTANT(CARET_TYPE_BLOCK);

	ClassDB::bind_method(D_METHOD("set_caret_type", "type"), &TextEdit::set_caret_type);
	ClassDB::bind_method(D_METHOD("get_caret_type"), &TextEdit::get_caret_type);

	ClassDB::bind_method(D_METHOD("set_caret_blink_enabled", "enable"), &TextEdit::set_caret_blink_enabled);
	ClassDB::bind_method(D_METHOD("is_caret_blink_enabled"), &TextEdit::is_caret_blink_enabled);

	ClassDB::bind_method(D_METHOD("set_caret_blink_interval", "interval"), &TextEdit::set_caret_blink_interval);
	ClassDB::bind_method(D_METHOD("get_caret_blink_interval"), &TextEdit::get_caret_blink_interval);

	ClassDB::bind_method(D_METHOD("set_draw_caret_when_editable_disabled", "enable"), &TextEdit::set_draw_caret_when_editable_disabled);
	ClassDB::bind_method(D_METHOD("is_drawing_caret_when_editable_disabled"), &TextEdit::is_drawing_caret_when_editable_disabled);

	ClassDB::bind_method(D_METHOD("set_move_caret_on_right_click_enabled", "enable"), &TextEdit::set_move_caret_on_right_click_enabled);
	ClassDB::bind_method(D_METHOD("is_move_caret_on_right_click_enabled"), &TextEdit::is_move_caret_on_right_click_enabled);

	ClassDB::bind_method(D_METHOD("set_caret_mid_grapheme_enabled", "enabled"), &TextEdit::set_caret_mid_grapheme_enabled);
	ClassDB::bind_method(D_METHOD("is_caret_mid_grapheme_enabled"), &TextEdit::is_caret_mid_grapheme_enabled);

	ClassDB::bind_method(D_METHOD("set_multiple_carets_enabled", "enabled"), &TextEdit::set_multiple_carets_enabled);
	ClassDB::bind_method(D_METHOD("is_multiple_carets_enabled"), &TextEdit::is_multiple_carets_enabled);

	ClassDB::bind_method(D_METHOD("add_caret", "line", "column"), &TextEdit::add_caret);
	ClassDB::bind_method(D_METHOD("remove_caret", "caret"), &TextEdit::remove_caret);
	ClassDB::bind_method(D_METHOD("remove_secondary_carets"), &TextEdit::remove_secondary_carets);
	ClassDB::bind_method(D_METHOD("get_caret_count"), &TextEdit::get_caret_count);
	ClassDB::bind_method(D_METHOD("add_caret_at_carets", "below"), &TextEdit::add_caret_at_carets);

	ClassDB::bind_method(D_METHOD("get_sorted_carets", "include_ignored_carets"), &TextEdit::get_sorted_carets, DEFVAL(false));
	ClassDB::bind_method(D_METHOD("collapse_carets", "from_line", "from_column", "to_line", "to_column", "inclusive"), &TextEdit::collapse_carets, DEFVAL(false));

	ClassDB::bind_method(D_METHOD("merge_overlapping_carets"), &TextEdit::merge_overlapping_carets);
	ClassDB::bind_method(D_METHOD("begin_multicaret_edit"), &TextEdit::begin_multicaret_edit);
	ClassDB::bind_method(D_METHOD("end_multicaret_edit"), &TextEdit::end_multicaret_edit);
	ClassDB::bind_method(D_METHOD("is_in_mulitcaret_edit"), &TextEdit::is_in_mulitcaret_edit);
	ClassDB::bind_method(D_METHOD("multicaret_edit_ignore_caret", "caret_index"), &TextEdit::multicaret_edit_ignore_caret);

	ClassDB::bind_method(D_METHOD("is_caret_visible", "caret_index"), &TextEdit::is_caret_visible, DEFVAL(0));
	ClassDB::bind_method(D_METHOD("get_caret_draw_pos", "caret_index"), &TextEdit::get_caret_draw_pos, DEFVAL(0));

	ClassDB::bind_method(D_METHOD("set_caret_line", "line", "adjust_viewport", "can_be_hidden", "wrap_index", "caret_index"), &TextEdit::set_caret_line, DEFVAL(true), DEFVAL(true), DEFVAL(0), DEFVAL(0));
	ClassDB::bind_method(D_METHOD("get_caret_line", "caret_index"), &TextEdit::get_caret_line, DEFVAL(0));

	ClassDB::bind_method(D_METHOD("set_caret_column", "column", "adjust_viewport", "caret_index"), &TextEdit::set_caret_column, DEFVAL(true), DEFVAL(0));
	ClassDB::bind_method(D_METHOD("get_caret_column", "caret_index"), &TextEdit::get_caret_column, DEFVAL(0));

	ClassDB::bind_method(D_METHOD("get_caret_wrap_index", "caret_index"), &TextEdit::get_caret_wrap_index, DEFVAL(0));

	ClassDB::bind_method(D_METHOD("get_word_under_caret", "caret_index"), &TextEdit::get_word_under_caret, DEFVAL(-1));

	ClassDB::bind_method(D_METHOD("set_use_default_word_separators", "enabled"), &TextEdit::set_use_default_word_separators);
	ClassDB::bind_method(D_METHOD("is_default_word_separators_enabled"), &TextEdit::is_default_word_separators_enabled);

	ClassDB::bind_method(D_METHOD("set_use_custom_word_separators", "enabled"), &TextEdit::set_use_custom_word_separators);
	ClassDB::bind_method(D_METHOD("is_custom_word_separators_enabled"), &TextEdit::is_custom_word_separators_enabled);
	ClassDB::bind_method(D_METHOD("set_custom_word_separators", "custom_word_separators"), &TextEdit::set_custom_word_separators);
	ClassDB::bind_method(D_METHOD("get_custom_word_separators"), &TextEdit::get_custom_word_separators);

	/* Selection. */
	BIND_ENUM_CONSTANT(SELECTION_MODE_NONE);
	BIND_ENUM_CONSTANT(SELECTION_MODE_SHIFT);
	BIND_ENUM_CONSTANT(SELECTION_MODE_POINTER);
	BIND_ENUM_CONSTANT(SELECTION_MODE_WORD);
	BIND_ENUM_CONSTANT(SELECTION_MODE_LINE);

	ClassDB::bind_method(D_METHOD("set_selecting_enabled", "enable"), &TextEdit::set_selecting_enabled);
	ClassDB::bind_method(D_METHOD("is_selecting_enabled"), &TextEdit::is_selecting_enabled);

	ClassDB::bind_method(D_METHOD("set_deselect_on_focus_loss_enabled", "enable"), &TextEdit::set_deselect_on_focus_loss_enabled);
	ClassDB::bind_method(D_METHOD("is_deselect_on_focus_loss_enabled"), &TextEdit::is_deselect_on_focus_loss_enabled);

	ClassDB::bind_method(D_METHOD("set_drag_and_drop_selection_enabled", "enable"), &TextEdit::set_drag_and_drop_selection_enabled);
	ClassDB::bind_method(D_METHOD("is_drag_and_drop_selection_enabled"), &TextEdit::is_drag_and_drop_selection_enabled);

	ClassDB::bind_method(D_METHOD("set_selection_mode", "mode"), &TextEdit::set_selection_mode);
	ClassDB::bind_method(D_METHOD("get_selection_mode"), &TextEdit::get_selection_mode);

	ClassDB::bind_method(D_METHOD("select_all"), &TextEdit::select_all);
	ClassDB::bind_method(D_METHOD("select_word_under_caret", "caret_index"), &TextEdit::select_word_under_caret, DEFVAL(-1));
	ClassDB::bind_method(D_METHOD("add_selection_for_next_occurrence"), &TextEdit::add_selection_for_next_occurrence);
	ClassDB::bind_method(D_METHOD("skip_selection_for_next_occurrence"), &TextEdit::skip_selection_for_next_occurrence);
	ClassDB::bind_method(D_METHOD("select", "origin_line", "origin_column", "caret_line", "caret_column", "caret_index"), &TextEdit::select, DEFVAL(0));

	ClassDB::bind_method(D_METHOD("has_selection", "caret_index"), &TextEdit::has_selection, DEFVAL(-1));

	ClassDB::bind_method(D_METHOD("get_selected_text", "caret_index"), &TextEdit::get_selected_text, DEFVAL(-1));
	ClassDB::bind_method(D_METHOD("get_selection_at_line_column", "line", "column", "include_edges", "only_selections"), &TextEdit::get_selection_at_line_column, DEFVAL(true), DEFVAL(true));
	ClassDB::bind_method(D_METHOD("get_line_ranges_from_carets", "only_selections", "merge_adjacent"), &TextEdit::get_line_ranges_from_carets_typed_array, DEFVAL(false), DEFVAL(true));

	ClassDB::bind_method(D_METHOD("get_selection_origin_line", "caret_index"), &TextEdit::get_selection_origin_line, DEFVAL(0));
	ClassDB::bind_method(D_METHOD("get_selection_origin_column", "caret_index"), &TextEdit::get_selection_origin_column, DEFVAL(0));
	ClassDB::bind_method(D_METHOD("set_selection_origin_line", "line", "can_be_hidden", "wrap_index", "caret_index"), &TextEdit::set_selection_origin_line, DEFVAL(true), DEFVAL(-1), DEFVAL(0));
	ClassDB::bind_method(D_METHOD("set_selection_origin_column", "column", "caret_index"), &TextEdit::set_selection_origin_column, DEFVAL(0));

	ClassDB::bind_method(D_METHOD("get_selection_from_line", "caret_index"), &TextEdit::get_selection_from_line, DEFVAL(0));
	ClassDB::bind_method(D_METHOD("get_selection_from_column", "caret_index"), &TextEdit::get_selection_from_column, DEFVAL(0));
	ClassDB::bind_method(D_METHOD("get_selection_to_line", "caret_index"), &TextEdit::get_selection_to_line, DEFVAL(0));
	ClassDB::bind_method(D_METHOD("get_selection_to_column", "caret_index"), &TextEdit::get_selection_to_column, DEFVAL(0));

	ClassDB::bind_method(D_METHOD("is_caret_after_selection_origin", "caret_index"), &TextEdit::is_caret_after_selection_origin, DEFVAL(0));

	ClassDB::bind_method(D_METHOD("deselect", "caret_index"), &TextEdit::deselect, DEFVAL(-1));
	ClassDB::bind_method(D_METHOD("delete_selection", "caret_index"), &TextEdit::delete_selection, DEFVAL(-1));

	/* Line wrapping. */
	BIND_ENUM_CONSTANT(LINE_WRAPPING_NONE);
	BIND_ENUM_CONSTANT(LINE_WRAPPING_BOUNDARY);

	ClassDB::bind_method(D_METHOD("set_line_wrapping_mode", "mode"), &TextEdit::set_line_wrapping_mode);
	ClassDB::bind_method(D_METHOD("get_line_wrapping_mode"), &TextEdit::get_line_wrapping_mode);

	ClassDB::bind_method(D_METHOD("set_autowrap_mode", "autowrap_mode"), &TextEdit::set_autowrap_mode);
	ClassDB::bind_method(D_METHOD("get_autowrap_mode"), &TextEdit::get_autowrap_mode);

	ClassDB::bind_method(D_METHOD("is_line_wrapped", "line"), &TextEdit::is_line_wrapped);
	ClassDB::bind_method(D_METHOD("get_line_wrap_count", "line"), &TextEdit::get_line_wrap_count);
	ClassDB::bind_method(D_METHOD("get_line_wrap_index_at_column", "line", "column"), &TextEdit::get_line_wrap_index_at_column);

	ClassDB::bind_method(D_METHOD("get_line_wrapped_text", "line"), &TextEdit::get_line_wrapped_text);

	/* Viewport. */
	// Scrolling.
	ClassDB::bind_method(D_METHOD("set_smooth_scroll_enabled", "enable"), &TextEdit::set_smooth_scroll_enabled);
	ClassDB::bind_method(D_METHOD("is_smooth_scroll_enabled"), &TextEdit::is_smooth_scroll_enabled);

	ClassDB::bind_method(D_METHOD("get_v_scroll_bar"), &TextEdit::get_v_scroll_bar);
	ClassDB::bind_method(D_METHOD("get_h_scroll_bar"), &TextEdit::get_h_scroll_bar);

	ClassDB::bind_method(D_METHOD("set_v_scroll", "value"), &TextEdit::set_v_scroll);
	ClassDB::bind_method(D_METHOD("get_v_scroll"), &TextEdit::get_v_scroll);

	ClassDB::bind_method(D_METHOD("set_h_scroll", "value"), &TextEdit::set_h_scroll);
	ClassDB::bind_method(D_METHOD("get_h_scroll"), &TextEdit::get_h_scroll);

	ClassDB::bind_method(D_METHOD("set_scroll_past_end_of_file_enabled", "enable"), &TextEdit::set_scroll_past_end_of_file_enabled);
	ClassDB::bind_method(D_METHOD("is_scroll_past_end_of_file_enabled"), &TextEdit::is_scroll_past_end_of_file_enabled);

	ClassDB::bind_method(D_METHOD("set_v_scroll_speed", "speed"), &TextEdit::set_v_scroll_speed);
	ClassDB::bind_method(D_METHOD("get_v_scroll_speed"), &TextEdit::get_v_scroll_speed);

	ClassDB::bind_method(D_METHOD("set_fit_content_height_enabled", "enabled"), &TextEdit::set_fit_content_height_enabled);
	ClassDB::bind_method(D_METHOD("is_fit_content_height_enabled"), &TextEdit::is_fit_content_height_enabled);

	ClassDB::bind_method(D_METHOD("set_fit_content_width_enabled", "enabled"), &TextEdit::set_fit_content_width_enabled);
	ClassDB::bind_method(D_METHOD("is_fit_content_width_enabled"), &TextEdit::is_fit_content_width_enabled);

	ClassDB::bind_method(D_METHOD("get_scroll_pos_for_line", "line", "wrap_index"), &TextEdit::get_scroll_pos_for_line, DEFVAL(0));

	// Visible lines.
	ClassDB::bind_method(D_METHOD("set_line_as_first_visible", "line", "wrap_index"), &TextEdit::set_line_as_first_visible, DEFVAL(0));
	ClassDB::bind_method(D_METHOD("get_first_visible_line"), &TextEdit::get_first_visible_line);

	ClassDB::bind_method(D_METHOD("set_line_as_center_visible", "line", "wrap_index"), &TextEdit::set_line_as_center_visible, DEFVAL(0));

	ClassDB::bind_method(D_METHOD("set_line_as_last_visible", "line", "wrap_index"), &TextEdit::set_line_as_last_visible, DEFVAL(0));
	ClassDB::bind_method(D_METHOD("get_last_full_visible_line"), &TextEdit::get_last_full_visible_line);
	ClassDB::bind_method(D_METHOD("get_last_full_visible_line_wrap_index"), &TextEdit::get_last_full_visible_line_wrap_index);

	ClassDB::bind_method(D_METHOD("get_visible_line_count"), &TextEdit::get_visible_line_count);
	ClassDB::bind_method(D_METHOD("get_visible_line_count_in_range", "from_line", "to_line"), &TextEdit::get_visible_line_count_in_range);
	ClassDB::bind_method(D_METHOD("get_total_visible_line_count"), &TextEdit::get_total_visible_line_count);

	// Auto adjust
	ClassDB::bind_method(D_METHOD("adjust_viewport_to_caret", "caret_index"), &TextEdit::adjust_viewport_to_caret, DEFVAL(0));
	ClassDB::bind_method(D_METHOD("center_viewport_to_caret", "caret_index"), &TextEdit::center_viewport_to_caret, DEFVAL(0));

	// Minimap
	ClassDB::bind_method(D_METHOD("set_draw_minimap", "enabled"), &TextEdit::set_draw_minimap);
	ClassDB::bind_method(D_METHOD("is_drawing_minimap"), &TextEdit::is_drawing_minimap);

	ClassDB::bind_method(D_METHOD("set_minimap_width", "width"), &TextEdit::set_minimap_width);
	ClassDB::bind_method(D_METHOD("get_minimap_width"), &TextEdit::get_minimap_width);

	ClassDB::bind_method(D_METHOD("get_minimap_visible_lines"), &TextEdit::get_minimap_visible_lines);

	/* Gutters. */
	BIND_ENUM_CONSTANT(GUTTER_TYPE_STRING);
	BIND_ENUM_CONSTANT(GUTTER_TYPE_ICON);
	BIND_ENUM_CONSTANT(GUTTER_TYPE_CUSTOM);

	ClassDB::bind_method(D_METHOD("add_gutter", "at"), &TextEdit::add_gutter, DEFVAL(-1));
	ClassDB::bind_method(D_METHOD("remove_gutter", "gutter"), &TextEdit::remove_gutter);
	ClassDB::bind_method(D_METHOD("get_gutter_count"), &TextEdit::get_gutter_count);
	ClassDB::bind_method(D_METHOD("set_gutter_name", "gutter", "name"), &TextEdit::set_gutter_name);
	ClassDB::bind_method(D_METHOD("get_gutter_name", "gutter"), &TextEdit::get_gutter_name);
	ClassDB::bind_method(D_METHOD("set_gutter_type", "gutter", "type"), &TextEdit::set_gutter_type);
	ClassDB::bind_method(D_METHOD("get_gutter_type", "gutter"), &TextEdit::get_gutter_type);
	ClassDB::bind_method(D_METHOD("set_gutter_width", "gutter", "width"), &TextEdit::set_gutter_width);
	ClassDB::bind_method(D_METHOD("get_gutter_width", "gutter"), &TextEdit::get_gutter_width);
	ClassDB::bind_method(D_METHOD("set_gutter_draw", "gutter", "draw"), &TextEdit::set_gutter_draw);
	ClassDB::bind_method(D_METHOD("is_gutter_drawn", "gutter"), &TextEdit::is_gutter_drawn);
	ClassDB::bind_method(D_METHOD("set_gutter_clickable", "gutter", "clickable"), &TextEdit::set_gutter_clickable);
	ClassDB::bind_method(D_METHOD("is_gutter_clickable", "gutter"), &TextEdit::is_gutter_clickable);
	ClassDB::bind_method(D_METHOD("set_gutter_overwritable", "gutter", "overwritable"), &TextEdit::set_gutter_overwritable);
	ClassDB::bind_method(D_METHOD("is_gutter_overwritable", "gutter"), &TextEdit::is_gutter_overwritable);
	ClassDB::bind_method(D_METHOD("merge_gutters", "from_line", "to_line"), &TextEdit::merge_gutters);
	ClassDB::bind_method(D_METHOD("set_gutter_custom_draw", "column", "draw_callback"), &TextEdit::set_gutter_custom_draw);
	ClassDB::bind_method(D_METHOD("get_total_gutter_width"), &TextEdit::get_total_gutter_width);

	// Line gutters.
	ClassDB::bind_method(D_METHOD("set_line_gutter_metadata", "line", "gutter", "metadata"), &TextEdit::set_line_gutter_metadata);
	ClassDB::bind_method(D_METHOD("get_line_gutter_metadata", "line", "gutter"), &TextEdit::get_line_gutter_metadata);
	ClassDB::bind_method(D_METHOD("set_line_gutter_text", "line", "gutter", "text"), &TextEdit::set_line_gutter_text);
	ClassDB::bind_method(D_METHOD("get_line_gutter_text", "line", "gutter"), &TextEdit::get_line_gutter_text);
	ClassDB::bind_method(D_METHOD("set_line_gutter_icon", "line", "gutter", "icon"), &TextEdit::set_line_gutter_icon);
	ClassDB::bind_method(D_METHOD("get_line_gutter_icon", "line", "gutter"), &TextEdit::get_line_gutter_icon);
	ClassDB::bind_method(D_METHOD("set_line_gutter_item_color", "line", "gutter", "color"), &TextEdit::set_line_gutter_item_color);
	ClassDB::bind_method(D_METHOD("get_line_gutter_item_color", "line", "gutter"), &TextEdit::get_line_gutter_item_color);
	ClassDB::bind_method(D_METHOD("set_line_gutter_clickable", "line", "gutter", "clickable"), &TextEdit::set_line_gutter_clickable);
	ClassDB::bind_method(D_METHOD("is_line_gutter_clickable", "line", "gutter"), &TextEdit::is_line_gutter_clickable);

	// Line style
	ClassDB::bind_method(D_METHOD("set_line_background_color", "line", "color"), &TextEdit::set_line_background_color);
	ClassDB::bind_method(D_METHOD("get_line_background_color", "line"), &TextEdit::get_line_background_color);

	/* Syntax Highlighting. */
	ClassDB::bind_method(D_METHOD("set_syntax_highlighter", "syntax_highlighter"), &TextEdit::set_syntax_highlighter);
	ClassDB::bind_method(D_METHOD("get_syntax_highlighter"), &TextEdit::get_syntax_highlighter);

	/* Visual. */
	ClassDB::bind_method(D_METHOD("set_highlight_current_line", "enabled"), &TextEdit::set_highlight_current_line);
	ClassDB::bind_method(D_METHOD("is_highlight_current_line_enabled"), &TextEdit::is_highlight_current_line_enabled);

	ClassDB::bind_method(D_METHOD("set_highlight_all_occurrences", "enabled"), &TextEdit::set_highlight_all_occurrences);
	ClassDB::bind_method(D_METHOD("is_highlight_all_occurrences_enabled"), &TextEdit::is_highlight_all_occurrences_enabled);

	ClassDB::bind_method(D_METHOD("get_draw_control_chars"), &TextEdit::get_draw_control_chars);
	ClassDB::bind_method(D_METHOD("set_draw_control_chars", "enabled"), &TextEdit::set_draw_control_chars);

	ClassDB::bind_method(D_METHOD("set_draw_tabs", "enabled"), &TextEdit::set_draw_tabs);
	ClassDB::bind_method(D_METHOD("is_drawing_tabs"), &TextEdit::is_drawing_tabs);

	ClassDB::bind_method(D_METHOD("set_draw_spaces", "enabled"), &TextEdit::set_draw_spaces);
	ClassDB::bind_method(D_METHOD("is_drawing_spaces"), &TextEdit::is_drawing_spaces);

	ClassDB::bind_method(D_METHOD("get_menu"), &TextEdit::get_menu);
	ClassDB::bind_method(D_METHOD("is_menu_visible"), &TextEdit::is_menu_visible);
	ClassDB::bind_method(D_METHOD("menu_option", "option"), &TextEdit::menu_option);

	/* Deprecated */
#ifndef DISABLE_DEPRECATED
	ClassDB::bind_method(D_METHOD("adjust_carets_after_edit", "caret", "from_line", "from_col", "to_line", "to_col"), &TextEdit::adjust_carets_after_edit);
	ClassDB::bind_method(D_METHOD("get_caret_index_edit_order"), &TextEdit::get_caret_index_edit_order);
	ClassDB::bind_method(D_METHOD("get_selection_line", "caret_index"), &TextEdit::get_selection_line, DEFVAL(0));
	ClassDB::bind_method(D_METHOD("get_selection_column", "caret_index"), &TextEdit::get_selection_column, DEFVAL(0));
#endif

	/* Inspector */
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "text", PROPERTY_HINT_MULTILINE_TEXT), "set_text", "get_text");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "placeholder_text", PROPERTY_HINT_MULTILINE_TEXT), "set_placeholder", "get_placeholder");

	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "editable"), "set_editable", "is_editable");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "context_menu_enabled"), "set_context_menu_enabled", "is_context_menu_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "shortcut_keys_enabled"), "set_shortcut_keys_enabled", "is_shortcut_keys_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "selecting_enabled"), "set_selecting_enabled", "is_selecting_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "deselect_on_focus_loss_enabled"), "set_deselect_on_focus_loss_enabled", "is_deselect_on_focus_loss_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "drag_and_drop_selection_enabled"), "set_drag_and_drop_selection_enabled", "is_drag_and_drop_selection_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "virtual_keyboard_enabled"), "set_virtual_keyboard_enabled", "is_virtual_keyboard_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "middle_mouse_paste_enabled"), "set_middle_mouse_paste_enabled", "is_middle_mouse_paste_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "empty_selection_clipboard_enabled"), "set_empty_selection_clipboard_enabled", "is_empty_selection_clipboard_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "wrap_mode", PROPERTY_HINT_ENUM, "None,Boundary"), "set_line_wrapping_mode", "get_line_wrapping_mode");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "autowrap_mode", PROPERTY_HINT_ENUM, "Arbitrary:1,Word:2,Word (Smart):3"), "set_autowrap_mode", "get_autowrap_mode");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "indent_wrapped_lines"), "set_indent_wrapped_lines", "is_indent_wrapped_lines");

	ADD_GROUP("Scroll", "scroll_");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "scroll_smooth"), "set_smooth_scroll_enabled", "is_smooth_scroll_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "scroll_v_scroll_speed", PROPERTY_HINT_NONE, "suffix:lines/s"), "set_v_scroll_speed", "get_v_scroll_speed");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "scroll_past_end_of_file"), "set_scroll_past_end_of_file_enabled", "is_scroll_past_end_of_file_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "scroll_vertical", PROPERTY_HINT_NONE, "suffix:lines"), "set_v_scroll", "get_v_scroll");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "scroll_horizontal", PROPERTY_HINT_NONE, "suffix:px"), "set_h_scroll", "get_h_scroll");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "scroll_fit_content_height"), "set_fit_content_height_enabled", "is_fit_content_height_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "scroll_fit_content_width"), "set_fit_content_width_enabled", "is_fit_content_width_enabled");

	ADD_GROUP("Minimap", "minimap_");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "minimap_draw"), "set_draw_minimap", "is_drawing_minimap");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "minimap_width", PROPERTY_HINT_NONE, "suffix:px"), "set_minimap_width", "get_minimap_width");

	ADD_GROUP("Caret", "caret_");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "caret_type", PROPERTY_HINT_ENUM, "Line,Block"), "set_caret_type", "get_caret_type");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "caret_blink"), "set_caret_blink_enabled", "is_caret_blink_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "caret_blink_interval", PROPERTY_HINT_RANGE, "0.1,10,0.01,suffix:s"), "set_caret_blink_interval", "get_caret_blink_interval");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "caret_draw_when_editable_disabled"), "set_draw_caret_when_editable_disabled", "is_drawing_caret_when_editable_disabled");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "caret_move_on_right_click"), "set_move_caret_on_right_click_enabled", "is_move_caret_on_right_click_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "caret_mid_grapheme"), "set_caret_mid_grapheme_enabled", "is_caret_mid_grapheme_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "caret_multiple"), "set_multiple_carets_enabled", "is_multiple_carets_enabled");

	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "use_default_word_separators"), "set_use_default_word_separators", "is_default_word_separators_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "use_custom_word_separators"), "set_use_custom_word_separators", "is_custom_word_separators_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "custom_word_separators"), "set_custom_word_separators", "get_custom_word_separators");

	ADD_GROUP("Highlighting", "");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "syntax_highlighter", PROPERTY_HINT_RESOURCE_TYPE, "SyntaxHighlighter", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_ALWAYS_DUPLICATE), "set_syntax_highlighter", "get_syntax_highlighter");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "highlight_all_occurrences"), "set_highlight_all_occurrences", "is_highlight_all_occurrences_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "highlight_current_line"), "set_highlight_current_line", "is_highlight_current_line_enabled");

	ADD_GROUP("Visual Whitespace", "draw_");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "draw_control_chars"), "set_draw_control_chars", "get_draw_control_chars");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "draw_tabs"), "set_draw_tabs", "is_drawing_tabs");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "draw_spaces"), "set_draw_spaces", "is_drawing_spaces");

	ADD_GROUP("BiDi", "");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "text_direction", PROPERTY_HINT_ENUM, "Auto,Left-to-Right,Right-to-Left,Inherited"), "set_text_direction", "get_text_direction");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "language", PROPERTY_HINT_LOCALE_ID, ""), "set_language", "get_language");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "structured_text_bidi_override", PROPERTY_HINT_ENUM, "Default,URI,File,Email,List,None,Custom"), "set_structured_text_bidi_override", "get_structured_text_bidi_override");
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "structured_text_bidi_override_options"), "set_structured_text_bidi_override_options", "get_structured_text_bidi_override_options");

	/* Signals */
	/* Core. */
	ADD_SIGNAL(MethodInfo("text_set"));
	ADD_SIGNAL(MethodInfo("text_changed"));
	ADD_SIGNAL(MethodInfo("lines_edited_from", PropertyInfo(Variant::INT, "from_line"), PropertyInfo(Variant::INT, "to_line")));

	/* Caret. */
	ADD_SIGNAL(MethodInfo("caret_changed"));

	/* Gutters. */
	ADD_SIGNAL(MethodInfo("gutter_clicked", PropertyInfo(Variant::INT, "line"), PropertyInfo(Variant::INT, "gutter")));
	ADD_SIGNAL(MethodInfo("gutter_added"));
	ADD_SIGNAL(MethodInfo("gutter_removed"));

	// Theme items
	/* Search */
	BIND_THEME_ITEM(Theme::DATA_TYPE_COLOR, TextEdit, search_result_color);
	BIND_THEME_ITEM(Theme::DATA_TYPE_COLOR, TextEdit, search_result_border_color);

	/* Caret */
	BIND_THEME_ITEM(Theme::DATA_TYPE_CONSTANT, TextEdit, caret_width);
	BIND_THEME_ITEM(Theme::DATA_TYPE_COLOR, TextEdit, caret_color);
	BIND_THEME_ITEM(Theme::DATA_TYPE_COLOR, TextEdit, caret_background_color);

	/* Selection */
	BIND_THEME_ITEM(Theme::DATA_TYPE_COLOR, TextEdit, font_selected_color);
	BIND_THEME_ITEM(Theme::DATA_TYPE_COLOR, TextEdit, selection_color);

	/* Other visuals */
	BIND_THEME_ITEM_CUSTOM(Theme::DATA_TYPE_STYLEBOX, TextEdit, style_normal, "normal");
	BIND_THEME_ITEM_CUSTOM(Theme::DATA_TYPE_STYLEBOX, TextEdit, style_focus, "focus");
	BIND_THEME_ITEM_CUSTOM(Theme::DATA_TYPE_STYLEBOX, TextEdit, style_readonly, "read_only");

	BIND_THEME_ITEM_CUSTOM(Theme::DATA_TYPE_ICON, TextEdit, tab_icon, "tab");
	BIND_THEME_ITEM_CUSTOM(Theme::DATA_TYPE_ICON, TextEdit, space_icon, "space");

	BIND_THEME_ITEM(Theme::DATA_TYPE_FONT, TextEdit, font);
	BIND_THEME_ITEM(Theme::DATA_TYPE_FONT_SIZE, TextEdit, font_size);
	BIND_THEME_ITEM(Theme::DATA_TYPE_COLOR, TextEdit, font_color);
	BIND_THEME_ITEM(Theme::DATA_TYPE_COLOR, TextEdit, font_readonly_color);
	BIND_THEME_ITEM(Theme::DATA_TYPE_COLOR, TextEdit, font_placeholder_color);

	BIND_THEME_ITEM(Theme::DATA_TYPE_CONSTANT, TextEdit, outline_size);
	BIND_THEME_ITEM_CUSTOM(Theme::DATA_TYPE_COLOR, TextEdit, outline_color, "font_outline_color");

	BIND_THEME_ITEM(Theme::DATA_TYPE_CONSTANT, TextEdit, line_spacing);

	BIND_THEME_ITEM(Theme::DATA_TYPE_COLOR, TextEdit, background_color);
	BIND_THEME_ITEM(Theme::DATA_TYPE_COLOR, TextEdit, current_line_color);
	BIND_THEME_ITEM(Theme::DATA_TYPE_COLOR, TextEdit, word_highlighted_color);

	/* Settings. */
	GLOBAL_DEF(PropertyInfo(Variant::FLOAT, "gui/timers/text_edit_idle_detect_sec", PROPERTY_HINT_RANGE, "0,10,0.01,or_greater"), 3);
	GLOBAL_DEF(PropertyInfo(Variant::INT, "gui/common/text_edit_undo_stack_max_size", PROPERTY_HINT_RANGE, "0,10000,1,or_greater"), 1024);
}

/* Internal API for CodeEdit. */
// Line hiding.
void TextEdit::_set_hiding_enabled(bool p_enabled) {
	if (hiding_enabled == p_enabled) {
		return;
	}

	if (!p_enabled) {
		_unhide_all_lines();
	}
	hiding_enabled = p_enabled;
	queue_redraw();
}

bool TextEdit::_is_hiding_enabled() const {
	return hiding_enabled;
}

bool TextEdit::_is_line_hidden(int p_line) const {
	ERR_FAIL_INDEX_V(p_line, text.size(), false);
	return text.is_hidden(p_line);
}

void TextEdit::_unhide_all_lines() {
	for (int i = 0; i < text.size(); i++) {
		text.set_hidden(i, false);
	}
	_update_scrollbars();
	queue_redraw();
}

void TextEdit::_unhide_carets() {
	// Override for functionality.
}

void TextEdit::_set_line_as_hidden(int p_line, bool p_hidden) {
	ERR_FAIL_INDEX(p_line, text.size());

	if (text.is_hidden(p_line) == p_hidden) {
		return;
	}

	if (_is_hiding_enabled() || !p_hidden) {
		text.set_hidden(p_line, p_hidden);
	}
	queue_redraw();
}

// Symbol lookup.
void TextEdit::_set_symbol_lookup_word(const String &p_symbol) {
	if (lookup_symbol_word == p_symbol) {
		return;
	}

	lookup_symbol_word = p_symbol;
	queue_redraw();
}

/* Text manipulation */

// Overridable actions
void TextEdit::_handle_unicode_input_internal(const uint32_t p_unicode, int p_caret) {
	ERR_FAIL_COND(p_caret >= get_caret_count() || p_caret < -1);
	if (!editable) {
		return;
	}

	start_action(EditAction::ACTION_TYPING);
	begin_multicaret_edit();
	for (int i = 0; i < get_caret_count(); i++) {
		if (p_caret == -1 && multicaret_edit_ignore_caret(i)) {
			continue;
		}
		if (p_caret != -1 && p_caret != i) {
			continue;
		}

		// Remove the old character if in insert mode and no selection.
		if (overtype_mode && !has_selection(i)) {
			// Make sure we don't try and remove empty space.
			int cl = get_caret_line(i);
			int cc = get_caret_column(i);
			if (cc < get_line(cl).length()) {
				_remove_text(cl, cc, cl, cc + 1);
			}
		}

		const char32_t chr[2] = { (char32_t)p_unicode, 0 };
		insert_text_at_caret(chr, i);
	}
	end_multicaret_edit();
	end_action();
}

void TextEdit::_backspace_internal(int p_caret) {
	ERR_FAIL_COND(p_caret >= get_caret_count() || p_caret < -1);
	if (!editable) {
		return;
	}

	if (has_selection(p_caret)) {
		delete_selection(p_caret);
		return;
	}

	begin_complex_operation();
	begin_multicaret_edit();
	for (int i = 0; i < get_caret_count(); i++) {
		if (p_caret == -1 && multicaret_edit_ignore_caret(i)) {
			continue;
		}
		if (p_caret != -1 && p_caret != i) {
			continue;
		}

		int to_line = get_caret_line(i);
		int to_column = get_caret_column(i);

		if (to_column == 0 && to_line == 0) {
			continue;
		}

		int from_line = to_column > 0 ? to_line : to_line - 1;
		int from_column = to_column > 0 ? (to_column - 1) : (text[to_line - 1].length());

		merge_gutters(from_line, to_line);

		_remove_text(from_line, from_column, to_line, to_column);
		collapse_carets(from_line, from_column, to_line, to_column);
		_offset_carets_after(to_line, to_column, from_line, from_column);

		set_caret_line(from_line, false, true, -1, i);
		set_caret_column(from_column, i == 0, i);
	}
	end_multicaret_edit();
	end_complex_operation();
}

void TextEdit::_cut_internal(int p_caret) {
	ERR_FAIL_COND(p_caret >= get_caret_count() || p_caret < -1);

	_copy_internal(p_caret);

	if (!editable) {
		return;
	}

	if (has_selection(p_caret)) {
		delete_selection(p_caret);
		return;
	}

	if (!empty_selection_clipboard_enabled) {
		return;
	}

	// Remove full lines.
	begin_complex_operation();
	begin_multicaret_edit();
	Vector<Point2i> line_ranges;
	if (p_caret == -1) {
		line_ranges = get_line_ranges_from_carets();
	} else {
		line_ranges.push_back(Point2i(get_caret_line(p_caret), get_caret_line(p_caret)));
	}
	int line_offset = 0;
	for (Point2i line_range : line_ranges) {
		// Preserve carets on the last line.
		remove_line_at(line_range.y + line_offset);
		if (line_range.x != line_range.y) {
			remove_text(line_range.x + line_offset, 0, line_range.y + line_offset, 0);
		}
		line_offset += line_range.x - line_range.y - 1;
	}
	end_multicaret_edit();
	end_complex_operation();
}

void TextEdit::_copy_internal(int p_caret) {
	ERR_FAIL_COND(p_caret >= get_caret_count() || p_caret < -1);
	if (has_selection(p_caret)) {
		DisplayServer::get_singleton()->clipboard_set(get_selected_text(p_caret));
		cut_copy_line = "";
		return;
	}

	if (!empty_selection_clipboard_enabled) {
		return;
	}

	// Copy full lines.
	StringBuilder clipboard;
	Vector<Point2i> line_ranges;
	if (p_caret == -1) {
		// When there are multiple carets on a line, only copy it once.
		line_ranges = get_line_ranges_from_carets(false, true);
	} else {
		line_ranges.push_back(Point2i(get_caret_line(p_caret), get_caret_line(p_caret)));
	}
	for (Point2i line_range : line_ranges) {
		for (int i = line_range.x; i <= line_range.y; i++) {
			if (text[i].length() != 0) {
				clipboard += _base_get_text(i, 0, i, text[i].length());
			}
			clipboard += "\n";
		}
	}

	String clipboard_string = clipboard.as_string();
	DisplayServer::get_singleton()->clipboard_set(clipboard_string);
	// Set the cut copy line so we know to paste as a line.
	if (get_caret_count() == 1) {
		cut_copy_line = clipboard_string;
	} else {
		cut_copy_line = "";
	}
}

void TextEdit::_paste_internal(int p_caret) {
	ERR_FAIL_COND(p_caret >= get_caret_count() || p_caret < -1);
	if (!editable) {
		return;
	}

	String clipboard = DisplayServer::get_singleton()->clipboard_get();
	if (clipboard.is_empty()) {
		// Nothing to paste.
		return;
	}

	// Paste a full line. Ignore '\r' characters that may have been added to the clipboard by the OS.
	if (get_caret_count() == 1 && !has_selection(0) && !cut_copy_line.is_empty() && cut_copy_line == clipboard.replace("\r", "")) {
		insert_text(clipboard, get_caret_line(), 0);
		return;
	}

	// Paste text at each caret or one line per caret.
	Vector<String> clipboad_lines = clipboard.split("\n");
	bool insert_line_per_caret = p_caret == -1 && get_caret_count() > 1 && clipboad_lines.size() == get_caret_count();

	begin_complex_operation();
	begin_multicaret_edit();
	Vector<int> sorted_carets = get_sorted_carets();
	for (int i = 0; i < sorted_carets.size(); i++) {
		int caret_index = sorted_carets[i];
		if (p_caret != -1 && p_caret != caret_index) {
			continue;
		}

		if (has_selection(caret_index)) {
			delete_selection(caret_index);
		}

		if (insert_line_per_caret) {
			clipboard = clipboad_lines[i];
		}

		insert_text_at_caret(clipboard, caret_index);
	}
	end_multicaret_edit();
	end_complex_operation();
}

void TextEdit::_paste_primary_clipboard_internal(int p_caret) {
	ERR_FAIL_COND(p_caret >= get_caret_count() || p_caret < -1);
	if (!is_editable() || !DisplayServer::get_singleton()->has_feature(DisplayServer::FEATURE_CLIPBOARD_PRIMARY)) {
		return;
	}

	String paste_buffer = DisplayServer::get_singleton()->clipboard_get_primary();

	if (get_caret_count() == 1) {
		Point2i pos = get_line_column_at_pos(get_local_mouse_pos());
		deselect();
		set_caret_line(pos.y, true, false, -1);
		set_caret_column(pos.x);
	}

	if (!paste_buffer.is_empty()) {
		insert_text_at_caret(paste_buffer);
	}

	grab_focus();
}

// Context menu.

Key TextEdit::_get_menu_action_accelerator(const String &p_action) {
	const List<Ref<InputEvent>> *events = InputMap::get_singleton()->action_get_events(p_action);
	if (!events) {
		return Key::NONE;
	}

	// Use first event in the list for the accelerator.
	const List<Ref<InputEvent>>::Element *first_event = events->front();
	if (!first_event) {
		return Key::NONE;
	}

	const Ref<InputEventKey> event = first_event->get();
	if (event.is_null()) {
		return Key::NONE;
	}

	// Use physical keycode if non-zero.
	if (event->get_physical_keycode() != Key::NONE) {
		return event->get_physical_keycode_with_modifiers();
	} else {
		return event->get_keycode_with_modifiers();
	}
}

void TextEdit::_generate_context_menu() {
	menu = memnew(PopupMenu);
	add_child(menu, false, INTERNAL_MODE_FRONT);

	menu_dir = memnew(PopupMenu);
	menu_dir->add_radio_check_item(ETR("Same as Layout Direction"), MENU_DIR_INHERITED);
	menu_dir->add_radio_check_item(ETR("Auto-Detect Direction"), MENU_DIR_AUTO);
	menu_dir->add_radio_check_item(ETR("Left-to-Right"), MENU_DIR_LTR);
	menu_dir->add_radio_check_item(ETR("Right-to-Left"), MENU_DIR_RTL);

	menu_ctl = memnew(PopupMenu);
	menu_ctl->add_item(ETR("Left-to-Right Mark (LRM)"), MENU_INSERT_LRM);
	menu_ctl->add_item(ETR("Right-to-Left Mark (RLM)"), MENU_INSERT_RLM);
	menu_ctl->add_item(ETR("Start of Left-to-Right Embedding (LRE)"), MENU_INSERT_LRE);
	menu_ctl->add_item(ETR("Start of Right-to-Left Embedding (RLE)"), MENU_INSERT_RLE);
	menu_ctl->add_item(ETR("Start of Left-to-Right Override (LRO)"), MENU_INSERT_LRO);
	menu_ctl->add_item(ETR("Start of Right-to-Left Override (RLO)"), MENU_INSERT_RLO);
	menu_ctl->add_item(ETR("Pop Direction Formatting (PDF)"), MENU_INSERT_PDF);
	menu_ctl->add_separator();
	menu_ctl->add_item(ETR("Arabic Letter Mark (ALM)"), MENU_INSERT_ALM);
	menu_ctl->add_item(ETR("Left-to-Right Isolate (LRI)"), MENU_INSERT_LRI);
	menu_ctl->add_item(ETR("Right-to-Left Isolate (RLI)"), MENU_INSERT_RLI);
	menu_ctl->add_item(ETR("First Strong Isolate (FSI)"), MENU_INSERT_FSI);
	menu_ctl->add_item(ETR("Pop Direction Isolate (PDI)"), MENU_INSERT_PDI);
	menu_ctl->add_separator();
	menu_ctl->add_item(ETR("Zero-Width Joiner (ZWJ)"), MENU_INSERT_ZWJ);
	menu_ctl->add_item(ETR("Zero-Width Non-Joiner (ZWNJ)"), MENU_INSERT_ZWNJ);
	menu_ctl->add_item(ETR("Word Joiner (WJ)"), MENU_INSERT_WJ);
	menu_ctl->add_item(ETR("Soft Hyphen (SHY)"), MENU_INSERT_SHY);

	menu->add_item(ETR("Cut"), MENU_CUT);
	menu->add_item(ETR("Copy"), MENU_COPY);
	menu->add_item(ETR("Paste"), MENU_PASTE);
	menu->add_separator();
	menu->add_item(ETR("Select All"), MENU_SELECT_ALL);
	menu->add_item(ETR("Clear"), MENU_CLEAR);
	menu->add_separator();
	menu->add_item(ETR("Undo"), MENU_UNDO);
	menu->add_item(ETR("Redo"), MENU_REDO);
	menu->add_separator();
	menu->add_submenu_node_item(ETR("Text Writing Direction"), menu_dir, MENU_SUBMENU_TEXT_DIR);
	menu->add_separator();
	menu->add_check_item(ETR("Display Control Characters"), MENU_DISPLAY_UCC);
	menu->add_submenu_node_item(ETR("Insert Control Character"), menu_ctl, MENU_SUBMENU_INSERT_UCC);

	menu->connect(SceneStringName(id_pressed), callable_mp(this, &TextEdit::menu_option));
	menu_dir->connect(SceneStringName(id_pressed), callable_mp(this, &TextEdit::menu_option));
	menu_ctl->connect(SceneStringName(id_pressed), callable_mp(this, &TextEdit::menu_option));
}

void TextEdit::_update_context_menu() {
	if (!menu) {
		_generate_context_menu();
	}

	int idx = -1;

#define MENU_ITEM_ACTION_DISABLED(m_menu, m_id, m_action, m_disabled)                                                  \
	idx = m_menu->get_item_index(m_id);                                                                                \
	if (idx >= 0) {                                                                                                    \
		m_menu->set_item_accelerator(idx, shortcut_keys_enabled ? _get_menu_action_accelerator(m_action) : Key::NONE); \
		m_menu->set_item_disabled(idx, m_disabled);                                                                    \
	}

#define MENU_ITEM_ACTION(m_menu, m_id, m_action)                                                                       \
	idx = m_menu->get_item_index(m_id);                                                                                \
	if (idx >= 0) {                                                                                                    \
		m_menu->set_item_accelerator(idx, shortcut_keys_enabled ? _get_menu_action_accelerator(m_action) : Key::NONE); \
	}

#define MENU_ITEM_DISABLED(m_menu, m_id, m_disabled) \
	idx = m_menu->get_item_index(m_id);              \
	if (idx >= 0) {                                  \
		m_menu->set_item_disabled(idx, m_disabled);  \
	}

#define MENU_ITEM_CHECKED(m_menu, m_id, m_checked) \
	idx = m_menu->get_item_index(m_id);            \
	if (idx >= 0) {                                \
		m_menu->set_item_checked(idx, m_checked);  \
	}

	MENU_ITEM_ACTION_DISABLED(menu, MENU_CUT, "ui_cut", !editable)
	MENU_ITEM_ACTION(menu, MENU_COPY, "ui_copy")
	MENU_ITEM_ACTION_DISABLED(menu, MENU_PASTE, "ui_paste", !editable)
	MENU_ITEM_ACTION_DISABLED(menu, MENU_SELECT_ALL, "ui_text_select_all", !selecting_enabled)
	MENU_ITEM_DISABLED(menu, MENU_CLEAR, !editable)
	MENU_ITEM_ACTION_DISABLED(menu, MENU_UNDO, "ui_undo", !editable || !has_undo())
	MENU_ITEM_ACTION_DISABLED(menu, MENU_REDO, "ui_redo", !editable || !has_redo())
	MENU_ITEM_CHECKED(menu_dir, MENU_DIR_INHERITED, text_direction == TEXT_DIRECTION_INHERITED)
	MENU_ITEM_CHECKED(menu_dir, MENU_DIR_AUTO, text_direction == TEXT_DIRECTION_AUTO)
	MENU_ITEM_CHECKED(menu_dir, MENU_DIR_LTR, text_direction == TEXT_DIRECTION_LTR)
	MENU_ITEM_CHECKED(menu_dir, MENU_DIR_RTL, text_direction == TEXT_DIRECTION_RTL)
	MENU_ITEM_CHECKED(menu, MENU_DISPLAY_UCC, draw_control_chars)
	MENU_ITEM_DISABLED(menu, MENU_SUBMENU_INSERT_UCC, !editable)

#undef MENU_ITEM_ACTION_DISABLED
#undef MENU_ITEM_ACTION
#undef MENU_ITEM_DISABLED
#undef MENU_ITEM_CHECKED
}

/* Versioning */
void TextEdit::_push_current_op() {
	if (pending_action_end) {
		start_action(EditAction::ACTION_NONE);
		return;
	}
	if (current_op.type == TextOperation::TYPE_NONE) {
		return; // Nothing to do.
	}

	if (next_operation_is_complex) {
		current_op.chain_forward = true;
		next_operation_is_complex = false;
	}

	undo_stack.push_back(current_op);
	current_op.type = TextOperation::TYPE_NONE;
	current_op.text = "";
	current_op.chain_forward = false;

	if (undo_stack.size() > undo_stack_max_size) {
		undo_stack.pop_front();
	}
}

void TextEdit::_do_text_op(const TextOperation &p_op, bool p_reverse) {
	ERR_FAIL_COND(p_op.type == TextOperation::TYPE_NONE);

	bool insert = p_op.type == TextOperation::TYPE_INSERT;
	if (p_reverse) {
		insert = !insert;
	}

	if (insert) {
		int check_line;
		int check_column;
		_base_insert_text(p_op.from_line, p_op.from_column, p_op.text, check_line, check_column);
		ERR_FAIL_COND(check_line != p_op.to_line); // BUG.
		ERR_FAIL_COND(check_column != p_op.to_column); // BUG.
	} else {
		_base_remove_text(p_op.from_line, p_op.from_column, p_op.to_line, p_op.to_column);
	}
}

void TextEdit::_clear_redo() {
	if (undo_stack_pos == nullptr) {
		return; // Nothing to clear.
	}

	_push_current_op();

	while (undo_stack_pos) {
		List<TextOperation>::Element *elem = undo_stack_pos;
		undo_stack_pos = undo_stack_pos->next();
		undo_stack.erase(elem);
	}
}

/* Search */
int TextEdit::_get_column_pos_of_word(const String &p_key, const String &p_search, uint32_t p_search_flags, int p_from_column) const {
	int col = -1;

	if (p_key.length() > 0 && p_search.length() > 0) {
		if (p_from_column < 0 || p_from_column > p_search.length()) {
			p_from_column = 0;
		}

		bool key_start_is_symbol = is_symbol(p_key[0]);
		bool key_end_is_symbol = is_symbol(p_key[p_key.length() - 1]);

		while (col == -1 && p_from_column <= p_search.length()) {
			if (p_search_flags & SEARCH_MATCH_CASE) {
				col = p_search.find(p_key, p_from_column);
			} else {
				col = p_search.findn(p_key, p_from_column);
			}

			// If not found, just break early to improve performance.
			if (col == -1) {
				break;
			}

			// Whole words only.
			if (col != -1 && p_search_flags & SEARCH_WHOLE_WORDS) {
				p_from_column = col;

				if (!key_start_is_symbol && col > 0 && !is_symbol(p_search[col - 1])) {
					col = -1;
				} else if (!key_end_is_symbol && (col + p_key.length()) < p_search.length() && !is_symbol(p_search[col + p_key.length()])) {
					col = -1;
				}
			}

			p_from_column += 1;
		}
	}
	return col;
}

/* Mouse */
int TextEdit::_get_char_pos_for_line(int p_px, int p_line, int p_wrap_index) const {
	ERR_FAIL_INDEX_V(p_line, text.size(), 0);
	p_wrap_index = MIN(p_wrap_index, text.get_line_data(p_line)->get_line_count() - 1);

	RID text_rid = text.get_line_data(p_line)->get_line_rid(p_wrap_index);
	float wrap_indent = (text.is_indent_wrapped_lines() && p_wrap_index > 0) ? get_indent_level(p_line) * theme_cache.font->get_char_size(' ', theme_cache.font_size).width : 0.0;
	if (is_layout_rtl()) {
		p_px = TS->shaped_text_get_size(text_rid).x - p_px + wrap_indent;
	} else {
		p_px -= wrap_indent;
	}
	int ofs = TS->shaped_text_hit_test_position(text_rid, p_px);
	if (!caret_mid_grapheme_enabled) {
		ofs = TS->shaped_text_closest_character_pos(text_rid, ofs);
	}
	return ofs;
}

/* Caret */
void TextEdit::_caret_changed(int p_caret) {
	queue_redraw();

	if (has_selection(p_caret)) {
		_selection_changed(p_caret);
	}

	if (caret_pos_dirty) {
		return;
	}

	if (is_inside_tree()) {
		callable_mp(this, &TextEdit::_emit_caret_changed).call_deferred();
	}
	caret_pos_dirty = true;
}

void TextEdit::_emit_caret_changed() {
	emit_signal(SNAME("caret_changed"));
	caret_pos_dirty = false;
}

void TextEdit::_reset_caret_blink_timer() {
	if (!caret_blink_enabled) {
		return;
	}

	draw_caret = true;
	if (has_focus()) {
		caret_blink_timer->stop();
		caret_blink_timer->start();
		queue_redraw();
	}
}

void TextEdit::_toggle_draw_caret() {
	draw_caret = !draw_caret;
	if (is_visible_in_tree() && has_focus() && window_has_focus) {
		queue_redraw();
	}
}

int TextEdit::_get_column_x_offset_for_line(int p_char, int p_line, int p_column) const {
	ERR_FAIL_INDEX_V(p_line, text.size(), 0);

	int row = 0;
	Vector<Vector2i> rows2 = text.get_line_wrap_ranges(p_line);
	for (int i = 0; i < rows2.size(); i++) {
		if ((p_char >= rows2[i].x) && (p_char < rows2[i].y || (i == rows2.size() - 1 && p_char == rows2[i].y))) {
			row = i;
			break;
		}
	}

	RID text_rid = text.get_line_data(p_line)->get_line_rid(row);
	float wrap_indent = (text.is_indent_wrapped_lines() && row > 0) ? get_indent_level(p_line) * theme_cache.font->get_char_size(' ', theme_cache.font_size).width : 0.0;
	CaretInfo ts_caret = TS->shaped_text_get_carets(text_rid, p_column);
	if ((ts_caret.l_caret != Rect2() && (ts_caret.l_dir == TextServer::DIRECTION_AUTO || ts_caret.l_dir == (TextServer::Direction)input_direction)) || (ts_caret.t_caret == Rect2())) {
		return ts_caret.l_caret.position.x + (is_layout_rtl() ? -wrap_indent : wrap_indent);
	} else {
		return ts_caret.t_caret.position.x + (is_layout_rtl() ? -wrap_indent : wrap_indent);
	}
}

bool TextEdit::_is_line_col_in_range(int p_line, int p_column, int p_from_line, int p_from_column, int p_to_line, int p_to_column, bool p_include_edges) const {
	if (p_line >= p_from_line && p_line <= p_to_line && (p_line > p_from_line || p_column > p_from_column) && (p_line < p_to_line || p_column < p_to_column)) {
		return true;
	}
	if (p_include_edges) {
		if ((p_line == p_from_line && p_column == p_from_column) || (p_line == p_to_line && p_column == p_to_column)) {
			return true;
		}
	}
	return false;
}

void TextEdit::_offset_carets_after(int p_old_line, int p_old_column, int p_new_line, int p_new_column, bool p_include_selection_begin, bool p_include_selection_end) {
	// Moves all carets at or after old_line and old_column.
	// Called after deleting or inserting text so that the carets stay with the text they are at.

	int edit_height = p_new_line - p_old_line;
	int edit_size = p_new_column - p_old_column;
	if (edit_height == 0 && edit_size == 0) {
		return;
	}

	// Intentionally includes carets in the multicaret_edit_ignore list so that they are moved together.
	for (int i = 0; i < get_caret_count(); i++) {
		bool selected = has_selection(i);
		bool caret_at_end = selected && is_caret_after_selection_origin(i);
		bool include_caret_at = caret_at_end ? p_include_selection_end : p_include_selection_begin;

		// Move caret.
		int caret_line = get_caret_line(i);
		int caret_column = get_caret_column(i);
		bool caret_after = caret_line > p_old_line || (caret_line == p_old_line && caret_column > p_old_column);
		bool caret_at = caret_line == p_old_line && caret_column == p_old_column;
		if (caret_after || (caret_at && include_caret_at)) {
			caret_line += edit_height;
			if (caret_line == p_new_line) {
				caret_column += edit_size;
			}

			if (edit_height != 0) {
				set_caret_line(caret_line, false, true, -1, i);
			}
			set_caret_column(caret_column, false, i);
		}

		// Move selection origin.
		if (!selected) {
			continue;
		}
		bool include_selection_origin_at = !caret_at_end ? p_include_selection_end : p_include_selection_begin;

		int selection_origin_line = get_selection_origin_line(i);
		int selection_origin_column = get_selection_origin_column(i);
		bool selection_origin_after = selection_origin_line > p_old_line || (selection_origin_line == p_old_line && selection_origin_column > p_old_column);
		bool selection_origin_at = selection_origin_line == p_old_line && selection_origin_column == p_old_column;
		if (selection_origin_after || (selection_origin_at && include_selection_origin_at)) {
			selection_origin_line += edit_height;
			if (selection_origin_line == p_new_line) {
				selection_origin_column += edit_size;
			}
			select(selection_origin_line, selection_origin_column, caret_line, caret_column, i);
		}
	}
	if (!p_include_selection_begin && p_include_selection_end && has_selection()) {
		// It is possible that two adjacent selections now overlap.
		merge_overlapping_carets();
	}
}

void TextEdit::_cancel_drag_and_drop_text() {
	// Cancel the drag operation if drag originated from here.
	if (selection_drag_attempt && get_viewport()) {
		get_viewport()->gui_cancel_drag();
	}
}

/* Selection */
void TextEdit::_selection_changed(int p_caret) {
	if (!selecting_enabled) {
		return;
	}

	_cancel_drag_and_drop_text();
	queue_redraw();
}

void TextEdit::_click_selection_held() {
	// Update the selection mode on a timer so it is updated when the view scrolls even if the mouse isn't moving.
	if (!Input::get_singleton()->is_mouse_button_pressed(MouseButton::LEFT) || get_selection_mode() == SelectionMode::SELECTION_MODE_NONE) {
		click_select_held->stop();
		return;
	}
	switch (get_selection_mode()) {
		case SelectionMode::SELECTION_MODE_POINTER: {
			_update_selection_mode_pointer();
		} break;
		case SelectionMode::SELECTION_MODE_WORD: {
			_update_selection_mode_word();
		} break;
		case SelectionMode::SELECTION_MODE_LINE: {
			_update_selection_mode_line();
		} break;
		default: {
			break;
		}
	}
}

void TextEdit::_update_selection_mode_pointer(bool p_initial) {
	Point2 mp = get_local_mouse_pos();

	Point2i pos = get_line_column_at_pos(mp);
	int line = pos.y;
	int column = pos.x;
	int caret_index = get_caret_count() - 1;

	if (p_initial && !has_selection(caret_index)) {
		set_selection_origin_line(line, true, -1, caret_index);
		set_selection_origin_column(column, caret_index);
		// Set the word begin and end to the column in case the mode changes later.
		carets.write[caret_index].selection.word_begin_column = column;
		carets.write[caret_index].selection.word_end_column = column;
	} else {
		select(get_selection_origin_line(caret_index), get_selection_origin_column(caret_index), line, column, caret_index);
	}
	adjust_viewport_to_caret(caret_index);

	if (has_selection(caret_index)) {
		// Only set to true if any selection has been made.
		dragging_selection = true;
	}

	click_select_held->start();
	merge_overlapping_carets();
}

void TextEdit::_update_selection_mode_word(bool p_initial) {
	dragging_selection = true;
	Point2 mp = get_local_mouse_pos();

	Point2i pos = get_line_column_at_pos(mp);
	int line = pos.y;
	int column = pos.x;
	int caret_index = get_caret_count() - 1;

	int caret_pos = CLAMP(column, 0, text[line].length());
	int beg = caret_pos;
	int end = beg;
	PackedInt32Array words = TS->shaped_text_get_word_breaks(text.get_line_data(line)->get_rid());
	for (int i = 0; i < words.size(); i = i + 2) {
		if ((words[i] < caret_pos && words[i + 1] > caret_pos) || (i == words.size() - 2 && caret_pos == words[i + 1])) {
			beg = words[i];
			end = words[i + 1];
			break;
		}
	}

	if (p_initial && !has_selection(caret_index)) {
		// Set the selection origin if there is no existing selection.
		select(line, beg, line, end, caret_index);
		carets.write[caret_index].selection.word_begin_column = beg;
		carets.write[caret_index].selection.word_end_column = end;
	} else {
		// Expand the word selection to the mouse.
		int origin_line = get_selection_origin_line(caret_index);
		bool is_new_selection_dir_right = line > origin_line || (line == origin_line && column >= carets[caret_index].selection.word_begin_column);
		int origin_col = is_new_selection_dir_right ? carets[caret_index].selection.word_begin_column : carets[caret_index].selection.word_end_column;
		int caret_col = is_new_selection_dir_right ? end : beg;

		select(origin_line, origin_col, line, caret_col, caret_index);
	}
	adjust_viewport_to_caret(caret_index);

	if (DisplayServer::get_singleton()->has_feature(DisplayServer::FEATURE_CLIPBOARD_PRIMARY)) {
		DisplayServer::get_singleton()->clipboard_set_primary(get_selected_text());
	}

	click_select_held->start();
	merge_overlapping_carets();
}

void TextEdit::_update_selection_mode_line(bool p_initial) {
	dragging_selection = true;
	Point2 mp = get_local_mouse_pos();

	Point2i pos = get_line_column_at_pos(mp);
	int line = pos.y;
	int caret_index = get_caret_count() - 1;

	int origin_line = p_initial && !has_selection(caret_index) ? line : get_selection_origin_line();
	bool line_below = line >= origin_line;
	int origin_col = line_below ? 0 : get_line(origin_line).length();
	int caret_line = line_below ? line + 1 : line;
	int caret_col = caret_line < text.size() ? 0 : get_line(text.size() - 1).length();

	select(origin_line, origin_col, caret_line, caret_col, caret_index);
	adjust_viewport_to_caret(caret_index);

	if (p_initial) {
		// Set the word begin and end to the start and end of the origin line in case the mode changes later.
		carets.write[caret_index].selection.word_begin_column = 0;
		carets.write[caret_index].selection.word_end_column = get_line(origin_line).length();
	}

	if (DisplayServer::get_singleton()->has_feature(DisplayServer::FEATURE_CLIPBOARD_PRIMARY)) {
		DisplayServer::get_singleton()->clipboard_set_primary(get_selected_text());
	}

	click_select_held->start();
	merge_overlapping_carets();
}

void TextEdit::_pre_shift_selection(int p_caret) {
	if (!selecting_enabled) {
		return;
	}

	set_selection_mode(SelectionMode::SELECTION_MODE_SHIFT);
	if (has_selection(p_caret)) {
		return;
	}
	// Prepare selection to start at current caret position.
	set_selection_origin_line(get_caret_line(p_caret), true, -1, p_caret);
	set_selection_origin_column(get_caret_column(p_caret), p_caret);
	carets.write[p_caret].selection.active = true;
	carets.write[p_caret].selection.word_begin_column = get_caret_column(p_caret);
	carets.write[p_caret].selection.word_end_column = get_caret_column(p_caret);
}

bool TextEdit::_selection_contains(int p_caret, int p_line, int p_column, bool p_include_edges, bool p_only_selections) const {
	if (!has_selection(p_caret)) {
		return !p_only_selections && p_line == get_caret_line(p_caret) && p_column == get_caret_column(p_caret);
	}
	return _is_line_col_in_range(p_line, p_column, get_selection_from_line(p_caret), get_selection_from_column(p_caret), get_selection_to_line(p_caret), get_selection_to_column(p_caret), p_include_edges);
}

/* Line Wrapping */
void TextEdit::_update_wrap_at_column(bool p_force) {
	int new_wrap_at = get_size().width - theme_cache.style_normal->get_minimum_size().width - gutters_width - gutter_padding;
	if (draw_minimap) {
		new_wrap_at -= minimap_width;
	}
	if (v_scroll->is_visible_in_tree()) {
		new_wrap_at -= v_scroll->get_combined_minimum_size().width;
	}
	/* Give it a little more space. */
	new_wrap_at -= wrap_right_offset;

	if ((wrap_at_column != new_wrap_at) || p_force) {
		wrap_at_column = new_wrap_at;
		if (line_wrapping_mode) {
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
			text.set_brk_flags(autowrap_flags);
			text.set_width(wrap_at_column);
		} else {
			text.set_width(-1);
		}

		text.invalidate_all_lines();
		_update_placeholder();
	}

	// Update viewport.
	int first_vis_line = get_first_visible_line();
	if (is_line_wrapped(first_vis_line)) {
		first_visible_line_wrap_ofs = MIN(first_visible_line_wrap_ofs, get_line_wrap_count(first_vis_line));
	} else {
		first_visible_line_wrap_ofs = 0;
	}
	set_line_as_first_visible(first_visible_line, first_visible_line_wrap_ofs);
}

/* Viewport. */
void TextEdit::_update_scrollbars() {
	Size2 size = get_size();
	Size2 hmin = h_scroll->get_combined_minimum_size();
	Size2 vmin = v_scroll->get_combined_minimum_size();

	v_scroll->set_begin(Point2(size.width - vmin.width, theme_cache.style_normal->get_margin(SIDE_TOP)));
	v_scroll->set_end(Point2(size.width, size.height - theme_cache.style_normal->get_margin(SIDE_TOP) - theme_cache.style_normal->get_margin(SIDE_BOTTOM)));

	h_scroll->set_begin(Point2(0, size.height - hmin.height));
	h_scroll->set_end(Point2(size.width - vmin.width, size.height));

	bool draw_placeholder = _using_placeholder();

	int visible_rows = get_visible_line_count();
	int total_rows = draw_placeholder ? placeholder_wraped_rows.size() : get_total_visible_line_count();
	if (scroll_past_end_of_file_enabled && !fit_content_height) {
		total_rows += visible_rows - 1;
	}

	int visible_width = size.width - theme_cache.style_normal->get_minimum_size().width;
	int total_width = (draw_placeholder ? placeholder_max_width : text.get_max_width()) + gutters_width + gutter_padding;

	if (draw_minimap) {
		total_width += minimap_width;
	}

	content_size_cache = Vector2i(total_width + 10, MAX(total_rows, 1) * get_line_height());
	if (fit_content_height || fit_content_width) {
		update_minimum_size();
	}

	updating_scrolls = true;

	if (!fit_content_height && total_rows > visible_rows) {
		v_scroll->show();
		v_scroll->set_max(total_rows + _get_visible_lines_offset());
		v_scroll->set_page(visible_rows + _get_visible_lines_offset());
		set_v_scroll(get_v_scroll());

	} else {
		first_visible_line = 0;
		first_visible_line_wrap_ofs = 0;
		v_scroll->set_value(0);
		v_scroll->set_max(0);
		v_scroll->hide();
	}

	if (total_width > visible_width) {
		h_scroll->show();
		h_scroll->set_max(total_width);
		h_scroll->set_page(visible_width);
		if (first_visible_col > (total_width - visible_width)) {
			first_visible_col = (total_width - visible_width);
		}
		if (fabs(h_scroll->get_value() - (double)first_visible_col) >= 1) {
			h_scroll->set_value(first_visible_col);
		}

	} else {
		first_visible_col = 0;
		h_scroll->set_value(0);
		h_scroll->set_max(0);
		h_scroll->hide();
	}

	updating_scrolls = false;
}

int TextEdit::_get_control_height() const {
	int control_height = get_size().height;
	control_height -= theme_cache.style_normal->get_minimum_size().height;
	if (h_scroll->is_visible_in_tree()) {
		control_height -= h_scroll->get_size().height;
	}
	return control_height;
}

void TextEdit::_v_scroll_input() {
	scrolling = false;
	minimap_clicked = false;
}

void TextEdit::_scroll_moved(double p_to_val) {
	if (updating_scrolls) {
		return;
	}

	if (h_scroll->is_visible_in_tree()) {
		first_visible_col = h_scroll->get_value();
	}
	if (v_scroll->is_visible_in_tree()) {
		// Set line ofs and wrap ofs.
		bool draw_placeholder = _using_placeholder();

		int v_scroll_i = floor(get_v_scroll());
		int sc = 0;
		int n_line;
		for (n_line = 0; n_line < text.size(); n_line++) {
			if (!_is_line_hidden(n_line)) {
				sc++;
				sc += draw_placeholder ? placeholder_wraped_rows.size() - 1 : get_line_wrap_count(n_line);
				if (sc > v_scroll_i) {
					break;
				}
			}
		}
		n_line = MIN(n_line, text.size() - 1);
		int line_wrap_amount = draw_placeholder ? placeholder_wraped_rows.size() - 1 : get_line_wrap_count(n_line);
		int wi = line_wrap_amount - (sc - v_scroll_i - 1);
		wi = CLAMP(wi, 0, line_wrap_amount);

		first_visible_line = n_line;
		first_visible_line_wrap_ofs = wi;
	}
	queue_redraw();
}

double TextEdit::_get_visible_lines_offset() const {
	double total = _get_control_height();
	total /= (double)get_line_height();
	total = total - floor(total);
	total = -CLAMP(total, 0.001, 1) + 1;
	return total;
}

double TextEdit::_get_v_scroll_offset() const {
	double val = get_v_scroll() - floor(get_v_scroll());
	return CLAMP(val, 0, 1);
}

void TextEdit::_scroll_up(real_t p_delta, bool p_animate) {
	if (scrolling && smooth_scroll_enabled && SIGN(target_v_scroll - v_scroll->get_value()) != SIGN(-p_delta)) {
		scrolling = false;
		minimap_clicked = false;
	}

	if (scrolling) {
		target_v_scroll = (target_v_scroll - p_delta);
	} else {
		target_v_scroll = (get_v_scroll() - p_delta);
	}

	if (smooth_scroll_enabled) {
		if (target_v_scroll <= 0) {
			target_v_scroll = 0;
		}
		if (!p_animate || Math::abs(target_v_scroll - v_scroll->get_value()) < 1.0) {
			v_scroll->set_value(target_v_scroll);
		} else {
			scrolling = true;
			set_physics_process_internal(true);
		}
	} else {
		set_v_scroll(target_v_scroll);
	}
}

void TextEdit::_scroll_down(real_t p_delta, bool p_animate) {
	if (scrolling && smooth_scroll_enabled && SIGN(target_v_scroll - v_scroll->get_value()) != SIGN(p_delta)) {
		scrolling = false;
		minimap_clicked = false;
	}

	if (scrolling) {
		target_v_scroll = (target_v_scroll + p_delta);
	} else {
		target_v_scroll = (get_v_scroll() + p_delta);
	}

	if (smooth_scroll_enabled) {
		int max_v_scroll = round(v_scroll->get_max() - v_scroll->get_page());
		if (target_v_scroll > max_v_scroll) {
			target_v_scroll = max_v_scroll;
		}
		if (!p_animate || Math::abs(target_v_scroll - v_scroll->get_value()) < 1.0) {
			v_scroll->set_value(target_v_scroll);
		} else {
			scrolling = true;
			set_physics_process_internal(true);
		}
	} else {
		set_v_scroll(target_v_scroll);
	}
}

void TextEdit::_scroll_lines_up() {
	scrolling = false;
	minimap_clicked = false;

	// Adjust the vertical scroll.
	set_v_scroll(get_v_scroll() - 1);

	// Adjust the caret to viewport.
	for (int i = 0; i < carets.size(); i++) {
		if (has_selection(i)) {
			continue;
		}

		int last_vis_line = get_last_full_visible_line();
		int last_vis_wrap = get_last_full_visible_line_wrap_index();
		if (get_caret_line(i) > last_vis_line || (get_caret_line(i) == last_vis_line && get_caret_wrap_index(i) > last_vis_wrap)) {
			set_caret_line(last_vis_line, false, false, last_vis_wrap, i);
		}
	}
	merge_overlapping_carets();
}

void TextEdit::_scroll_lines_down() {
	scrolling = false;
	minimap_clicked = false;

	// Adjust the vertical scroll.
	set_v_scroll(get_v_scroll() + 1);

	// Adjust the caret to viewport.
	for (int i = 0; i < carets.size(); i++) {
		if (has_selection(i)) {
			continue;
		}

		int first_vis_line = get_first_visible_line();
		if (get_caret_line(i) < first_vis_line || (get_caret_line(i) == first_vis_line && get_caret_wrap_index(i) < first_visible_line_wrap_ofs)) {
			set_caret_line(first_vis_line, false, false, first_visible_line_wrap_ofs, i);
		}
	}
	merge_overlapping_carets();
}

// Minimap

void TextEdit::_update_minimap_hover() {
	const Point2 mp = get_local_mouse_pos();
	const int xmargin_end = get_size().width - theme_cache.style_normal->get_margin(SIDE_RIGHT);

	bool hovering_sidebar = mp.x > xmargin_end - minimap_width && mp.x < xmargin_end;
	if (!hovering_sidebar) {
		if (hovering_minimap) {
			// Only redraw if the hovering status changed.
			hovering_minimap = false;
			queue_redraw();
		}

		// Return early to avoid running the operations below when not needed.
		return;
	}

	const int row = get_minimap_line_at_pos(mp);

	bool new_hovering_minimap = row >= get_first_visible_line() && row <= get_last_full_visible_line();
	if (new_hovering_minimap != hovering_minimap) {
		// Only redraw if the hovering status changed.
		hovering_minimap = new_hovering_minimap;
		queue_redraw();
	}
}

void TextEdit::_update_minimap_click() {
	Point2 mp = get_local_mouse_pos();

	int xmargin_end = get_size().width - theme_cache.style_normal->get_margin(SIDE_RIGHT);
	if (!dragging_minimap && (mp.x < xmargin_end - minimap_width || mp.x > xmargin_end)) {
		minimap_clicked = false;
		return;
	}
	minimap_clicked = true;
	dragging_minimap = true;

	int row = get_minimap_line_at_pos(mp);

	if (row >= get_first_visible_line() && (row < get_last_full_visible_line() || row >= (text.size() - 1))) {
		minimap_scroll_ratio = v_scroll->get_as_ratio();
		minimap_scroll_click_pos = mp.y;
		can_drag_minimap = true;
		return;
	}

	Point2i next_line = get_next_visible_line_index_offset_from(row, 0, -get_visible_line_count() / 2);
	int first_line = MAX(0, row - next_line.x + 1);
	double delta = get_scroll_pos_for_line(first_line, next_line.y) - get_v_scroll();
	if (delta < 0) {
		_scroll_up(-delta, true);
	} else {
		_scroll_down(delta, true);
	}
}

void TextEdit::_update_minimap_drag() {
	if (!can_drag_minimap) {
		return;
	}

	int control_height = _get_control_height();
	int scroll_height = v_scroll->get_max() * (minimap_char_size.y + minimap_line_spacing);
	if (control_height > scroll_height) {
		control_height = scroll_height;
	}

	Point2 mp = get_local_mouse_pos();

	double diff = (mp.y - minimap_scroll_click_pos) / control_height;
	v_scroll->set_as_ratio(minimap_scroll_ratio + diff);
}

/* Gutters. */
void TextEdit::_update_gutter_width() {
	gutters_width = 0;
	for (int i = 0; i < gutters.size(); i++) {
		if (gutters[i].draw) {
			gutters_width += gutters[i].width;
		}
	}
	if (gutters_width > 0) {
		gutter_padding = 2;
	}
	queue_redraw();
}

/* Syntax highlighting. */
Vector<Pair<int64_t, Color>> TextEdit::_get_line_syntax_highlighting(int p_line) {
	if (syntax_highlighter.is_null() || setting_text) {
		return Vector<Pair<int64_t, Color>>();
	}

	HashMap<int, Vector<Pair<int64_t, Color>>>::Iterator E = syntax_highlighting_cache.find(p_line);
	if (E) {
		return E->value;
	}

	Dictionary color_map = syntax_highlighter->get_line_syntax_highlighting(p_line);
	Vector<Pair<int64_t, Color>> result;
	result.resize(color_map.size());
	int i = 0;
	for (const Variant *key = color_map.next(nullptr); key; key = color_map.next(key), i++) {
		int64_t key_data = *key;
		const Variant *color_data = color_map.getptr(*key);
		Color color_value = editable ? theme_cache.font_color : theme_cache.font_readonly_color;
		if (color_data != nullptr) {
			color_value = (color_data->operator Dictionary()).get("color", color_value);
		}
		result.write[i] = Pair<int64_t, Color>(key_data, color_value);
	}
	syntax_highlighting_cache.insert(p_line, result);

	return result;
}

void TextEdit::_clear_syntax_highlighting_cache() {
	syntax_highlighting_cache.clear();
}

/* Deprecated. */
#ifndef DISABLE_DEPRECATED
Vector<int> TextEdit::get_caret_index_edit_order() {
	Vector<int> carets_order = get_sorted_carets();
	carets_order.reverse();
	return carets_order;
}

void TextEdit::adjust_carets_after_edit(int p_caret, int p_from_line, int p_from_col, int p_to_line, int p_to_col) {
}

int TextEdit::get_selection_line(int p_caret) const {
	return get_selection_origin_line(p_caret);
}

int TextEdit::get_selection_column(int p_caret) const {
	return get_selection_origin_column(p_caret);
}
#endif

/*** Super internal Core API. Everything builds on it. ***/

void TextEdit::_text_changed() {
	_clear_syntax_highlighting_cache();
	_cancel_drag_and_drop_text();
	queue_redraw();

	if (text_changed_dirty || setting_text) {
		return;
	}

	if (is_inside_tree()) {
		callable_mp(this, &TextEdit::_emit_text_changed).call_deferred();
	}
	text_changed_dirty = true;
}

void TextEdit::_emit_text_changed() {
	emit_signal(SceneStringName(text_changed));
	text_changed_dirty = false;
}

void TextEdit::_insert_text(int p_line, int p_char, const String &p_text, int *r_end_line, int *r_end_char) {
	if (!setting_text && idle_detect->is_inside_tree()) {
		idle_detect->start();
	}

	if (undo_enabled) {
		_clear_redo();
	}

	int retline, retchar;
	_base_insert_text(p_line, p_char, p_text, retline, retchar);
	if (r_end_line) {
		*r_end_line = retline;
	}
	if (r_end_char) {
		*r_end_char = retchar;
	}

	if (!undo_enabled) {
		return;
	}

	/* UNDO!! */
	TextOperation op;
	op.type = TextOperation::TYPE_INSERT;
	op.from_line = p_line;
	op.from_column = p_char;
	op.to_line = retline;
	op.to_column = retchar;
	op.text = p_text;
	op.version = ++version;
	op.chain_forward = false;
	op.chain_backward = false;
	if (next_operation_is_complex) {
		op.start_carets = current_op.start_carets;
	} else {
		op.start_carets = carets;
	}
	op.end_carets = carets;

	op.prev_version = get_version();
	_push_current_op();
	current_op = op;
}

void TextEdit::_remove_text(int p_from_line, int p_from_column, int p_to_line, int p_to_column) {
	if (!setting_text && idle_detect->is_inside_tree()) {
		idle_detect->start();
	}

	String txt;
	if (undo_enabled) {
		_clear_redo();
		txt = _base_get_text(p_from_line, p_from_column, p_to_line, p_to_column);
	}

	_base_remove_text(p_from_line, p_from_column, p_to_line, p_to_column);

	if (!undo_enabled) {
		return;
	}

	/* UNDO! */
	TextOperation op;
	op.type = TextOperation::TYPE_REMOVE;
	op.from_line = p_from_line;
	op.from_column = p_from_column;
	op.to_line = p_to_line;
	op.to_column = p_to_column;
	op.text = txt;
	op.version = ++version;
	op.chain_forward = false;
	op.chain_backward = false;
	if (next_operation_is_complex) {
		op.start_carets = current_op.start_carets;
	} else {
		op.start_carets = carets;
	}
	op.end_carets = carets;

	op.prev_version = get_version();
	_push_current_op();
	current_op = op;
}

void TextEdit::_base_insert_text(int p_line, int p_char, const String &p_text, int &r_end_line, int &r_end_column) {
	// Save for undo.
	ERR_FAIL_INDEX(p_line, text.size());
	ERR_FAIL_COND(p_char < 0);

	/* STEP 1: Remove \r from source text and separate in substrings. */
	const String text_to_insert = p_text.replace("\r", "");
	Vector<String> substrings = text_to_insert.split("\n");

	// Is this just a new empty line?
	bool shift_first_line = p_char == 0 && substrings.size() == 2 && text_to_insert == "\n";

	/* STEP 2: Add spaces if the char is greater than the end of the line. */
	while (p_char > text[p_line].length()) {
		text.set(p_line, text[p_line] + String::chr(' '), structured_text_parser(st_parser, st_args, text[p_line] + String::chr(' ')));
	}

	/* STEP 3: Separate dest string in pre and post text. */
	String postinsert_text = text[p_line].substr(p_char, text[p_line].size());

	substrings.write[0] = text[p_line].substr(0, p_char) + substrings[0];
	substrings.write[substrings.size() - 1] += postinsert_text;

	Vector<Array> bidi_override;
	bidi_override.resize(substrings.size());
	for (int i = 0; i < substrings.size(); i++) {
		bidi_override.write[i] = structured_text_parser(st_parser, st_args, substrings[i]);
	}

	text.insert(p_line, substrings, bidi_override);

	if (shift_first_line) {
		text.move_gutters(p_line, p_line + 1);
		text.set_hidden(p_line + 1, text.is_hidden(p_line));

		text.set_hidden(p_line, false);
	}

	r_end_line = p_line + substrings.size() - 1;
	r_end_column = text[r_end_line].length() - postinsert_text.length();

	TextServer::Direction dir = TS->shaped_text_get_dominant_direction_in_range(text.get_line_data(r_end_line)->get_rid(), (r_end_line == p_line) ? carets[0].column : 0, r_end_column);
	if (dir != TextServer::DIRECTION_AUTO) {
		input_direction = (TextDirection)dir;
	}

	_text_changed();
	emit_signal(SNAME("lines_edited_from"), p_line, r_end_line);
}

String TextEdit::_base_get_text(int p_from_line, int p_from_column, int p_to_line, int p_to_column) const {
	ERR_FAIL_INDEX_V(p_from_line, text.size(), String());
	ERR_FAIL_INDEX_V(p_from_column, text[p_from_line].length() + 1, String());
	ERR_FAIL_INDEX_V(p_to_line, text.size(), String());
	ERR_FAIL_INDEX_V(p_to_column, text[p_to_line].length() + 1, String());
	ERR_FAIL_COND_V(p_to_line < p_from_line, String()); // 'from > to'.
	ERR_FAIL_COND_V(p_to_line == p_from_line && p_to_column < p_from_column, String()); // 'from > to'.

	StringBuilder ret;

	for (int i = p_from_line; i <= p_to_line; i++) {
		int begin = (i == p_from_line) ? p_from_column : 0;
		int end = (i == p_to_line) ? p_to_column : text[i].length();

		if (i > p_from_line) {
			ret += "\n";
		}
		ret += text[i].substr(begin, end - begin);
	}

	return ret.as_string();
}

void TextEdit::_base_remove_text(int p_from_line, int p_from_column, int p_to_line, int p_to_column) {
	ERR_FAIL_INDEX(p_from_line, text.size());
	ERR_FAIL_INDEX(p_from_column, text[p_from_line].length() + 1);
	ERR_FAIL_INDEX(p_to_line, text.size());
	ERR_FAIL_INDEX(p_to_column, text[p_to_line].length() + 1);
	ERR_FAIL_COND(p_to_line < p_from_line); // 'from > to'.
	ERR_FAIL_COND(p_to_line == p_from_line && p_to_column < p_from_column); // 'from > to'.

	String pre_text = text[p_from_line].substr(0, p_from_column);
	String post_text = text[p_to_line].substr(p_to_column, text[p_to_line].length());

	text.remove_range(p_from_line, p_to_line);
	text.set(p_from_line, pre_text + post_text, structured_text_parser(st_parser, st_args, pre_text + post_text));

	_text_changed();
	emit_signal(SNAME("lines_edited_from"), p_to_line, p_from_line);
}

TextEdit::TextEdit(const String &p_placeholder) {
	placeholder_data_buf.instantiate();
	carets.push_back(Caret());

	clear();
	set_focus_mode(FOCUS_ALL);
	set_default_cursor_shape(CURSOR_IBEAM);
	set_process_unhandled_key_input(true);

	text.set_tab_size(text.get_tab_size());

	h_scroll = memnew(HScrollBar);
	v_scroll = memnew(VScrollBar);

	add_child(h_scroll, false, INTERNAL_MODE_FRONT);
	add_child(v_scroll, false, INTERNAL_MODE_FRONT);

	h_scroll->connect(SceneStringName(value_changed), callable_mp(this, &TextEdit::_scroll_moved));
	v_scroll->connect(SceneStringName(value_changed), callable_mp(this, &TextEdit::_scroll_moved));

	v_scroll->connect("scrolling", callable_mp(this, &TextEdit::_v_scroll_input));

	/* Caret. */
	caret_blink_timer = memnew(Timer);
	add_child(caret_blink_timer, false, INTERNAL_MODE_FRONT);
	caret_blink_timer->set_wait_time(0.65);
	caret_blink_timer->connect("timeout", callable_mp(this, &TextEdit::_toggle_draw_caret));
	set_caret_blink_enabled(false);

	/* Selection. */
	click_select_held = memnew(Timer);
	add_child(click_select_held, false, INTERNAL_MODE_FRONT);
	click_select_held->set_wait_time(0.05);
	click_select_held->connect("timeout", callable_mp(this, &TextEdit::_click_selection_held));

	idle_detect = memnew(Timer);
	add_child(idle_detect, false, INTERNAL_MODE_FRONT);
	idle_detect->set_one_shot(true);
	idle_detect->set_wait_time(GLOBAL_GET("gui/timers/text_edit_idle_detect_sec"));
	idle_detect->connect("timeout", callable_mp(this, &TextEdit::_push_current_op));

	undo_stack_max_size = GLOBAL_GET("gui/common/text_edit_undo_stack_max_size");

	set_placeholder(p_placeholder);

	set_clip_contents(true);
	set_editable(true);
}

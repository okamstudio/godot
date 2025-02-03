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

#include "core/message_queue.h"
#include "core/os/input.h"
#include "core/os/keyboard.h"
#include "core/os/os.h"
#include "core/project_settings.h"
#include "core/script_language.h"
#include "label.h"
#include "scene/main/viewport.h"

#ifdef TOOLS_ENABLED
#include "editor/editor_scale.h"
#endif

#define TAB_PIXELS

inline bool _is_symbol(CharType c) {
	return is_symbol(c);
}

static bool _is_text_char(CharType c) {
	return !is_symbol(c);
}

static bool _is_whitespace(CharType c) {
	return c == '\t' || c == ' ';
}

static bool _is_char(CharType c) {
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

static bool _is_number(CharType c) {
	return (c >= '0' && c <= '9');
}

static bool _is_hex_symbol(CharType c) {
	return ((c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'));
}

static bool _is_pair_right_symbol(CharType c) {
	return c == '"' ||
			c == '\'' ||
			c == ')' ||
			c == ']' ||
			c == '}';
}

static bool _is_pair_left_symbol(CharType c) {
	return c == '"' ||
			c == '\'' ||
			c == '(' ||
			c == '[' ||
			c == '{';
}

static bool _is_pair_symbol(CharType c) {
	return _is_pair_left_symbol(c) || _is_pair_right_symbol(c);
}

static CharType _get_right_pair_symbol(CharType c) {
	if (c == '"') {
		return '"';
	}
	if (c == '\'') {
		return '\'';
	}
	if (c == '(') {
		return ')';
	}
	if (c == '[') {
		return ']';
	}
	if (c == '{') {
		return '}';
	}
	return 0;
}

static int _find_first_non_whitespace_column_of_line(const String &line) {
	int left = 0;
	while (left < line.length() && _is_whitespace(line[left])) {
		left++;
	}
	return left;
}

void TextEdit::Text::set_font(const Ref<Font> &p_font) {
	font = p_font;
}

void TextEdit::Text::set_indent_size(int p_indent_size) {
	indent_size = p_indent_size;
}

void TextEdit::Text::_update_line_cache(int p_line) const {
	int w = 0;

	int len = text[p_line].data.length();
	const CharType *str = text[p_line].data.c_str();

	// Update width.

	for (int i = 0; i < len; i++) {
		w += get_char_width(str[i], str[i + 1], w);
	}

	text.write[p_line].width_cache = w;

	text.write[p_line].wrap_amount_cache = -1;

	// Update regions.

	text.write[p_line].region_info.clear();

	for (int i = 0; i < len; i++) {
		if (!_is_symbol(str[i])) {
			continue;
		}
		if (str[i] == '\\') {
			i++; // Skip quoted anything.
			continue;
		}

		int left = len - i;

		for (int j = 0; j < color_regions->size(); j++) {
			const ColorRegion &cr = color_regions->operator[](j);

			/* BEGIN */

			int lr = cr.begin_key.length();
			const CharType *kc;
			bool match;

			if (lr != 0 && lr <= left) {
				kc = cr.begin_key.c_str();

				match = true;

				for (int k = 0; k < lr; k++) {
					if (kc[k] != str[i + k]) {
						match = false;
						break;
					}
				}

				if (match) {
					ColorRegionInfo cri;
					cri.end = false;
					cri.region = j;
					text.write[p_line].region_info[i] = cri;
					i += lr - 1;

					break;
				}
			}

			/* END */

			lr = cr.end_key.length();
			if (lr != 0 && lr <= left) {
				kc = cr.end_key.c_str();

				match = true;

				for (int k = 0; k < lr; k++) {
					if (kc[k] != str[i + k]) {
						match = false;
						break;
					}
				}

				if (match) {
					ColorRegionInfo cri;
					cri.end = true;
					cri.region = j;
					text.write[p_line].region_info[i] = cri;
					i += lr - 1;

					break;
				}
			}
		}
	}
}

const Map<int, TextEdit::Text::ColorRegionInfo> &TextEdit::Text::get_color_region_info(int p_line) const {
	static Map<int, ColorRegionInfo> cri;
	ERR_FAIL_INDEX_V(p_line, text.size(), cri);

	if (text[p_line].width_cache == -1) {
		_update_line_cache(p_line);
	}

	return text[p_line].region_info;
}

int TextEdit::Text::get_line_width(int p_line) const {
	ERR_FAIL_INDEX_V(p_line, text.size(), -1);

	if (text[p_line].width_cache == -1) {
		_update_line_cache(p_line);
	}

	return text[p_line].width_cache;
}

void TextEdit::Text::set_line_wrap_amount(int p_line, int p_wrap_amount) const {
	ERR_FAIL_INDEX(p_line, text.size());

	text.write[p_line].wrap_amount_cache = p_wrap_amount;
}

int TextEdit::Text::get_line_wrap_amount(int p_line) const {
	ERR_FAIL_INDEX_V(p_line, text.size(), -1);

	return text[p_line].wrap_amount_cache;
}

void TextEdit::Text::clear_width_cache() {
	for (int i = 0; i < text.size(); i++) {
		text.write[i].width_cache = -1;
	}
}

void TextEdit::Text::clear_wrap_cache() {
	for (int i = 0; i < text.size(); i++) {
		text.write[i].wrap_amount_cache = -1;
	}
}

void TextEdit::Text::clear_info_icons() {
	for (int i = 0; i < text.size(); i++) {
		text.write[i].has_info = false;
	}
}

void TextEdit::Text::clear() {
	text.clear();
	insert(0, "");
}

int TextEdit::Text::get_max_width(bool p_exclude_hidden) const {
	// Quite some work, but should be fast enough.

	int max = 0;
	for (int i = 0; i < text.size(); i++) {
		if (!p_exclude_hidden || !is_hidden(i)) {
			max = MAX(max, get_line_width(i));
		}
	}
	return max;
}

void TextEdit::Text::set(int p_line, const String &p_text) {
	ERR_FAIL_INDEX(p_line, text.size());

	text.write[p_line].width_cache = -1;
	text.write[p_line].wrap_amount_cache = -1;
	text.write[p_line].data = p_text;
}

void TextEdit::Text::insert(int p_at, const String &p_text) {
	Line line;
	line.marked = false;
	line.safe = false;
	line.breakpoint = false;
	line.bookmark = false;
	line.hidden = false;
	line.has_info = false;
	line.width_cache = -1;
	line.wrap_amount_cache = -1;
	line.data = p_text;
	text.insert(p_at, line);
}
void TextEdit::Text::remove(int p_at) {
	text.remove(p_at);
}

int TextEdit::Text::get_char_width(CharType c, CharType next_c, int px) const {
	int tab_w = font->get_char_size(' ').width * indent_size;
	int w = 0;

	if (c == '\t') {
		int left = px % tab_w;
		if (left == 0) {
			w = tab_w;
		} else {
			w = tab_w - px % tab_w; // Is right.
		}
	} else {
		w = font->get_char_size(c, next_c).width;
	}
	return w;
}

void TextEdit::_update_scrollbars() {
	Size2 size = get_size();
	Size2 hmin = h_scroll->get_combined_minimum_size();
	Size2 vmin = v_scroll->get_combined_minimum_size();

	v_scroll->set_begin(Point2(size.width - vmin.width, cache.style_normal->get_margin(MARGIN_TOP)));
	v_scroll->set_end(Point2(size.width, size.height - cache.style_normal->get_margin(MARGIN_TOP) - cache.style_normal->get_margin(MARGIN_BOTTOM)));

	h_scroll->set_begin(Point2(0, size.height - hmin.height));
	h_scroll->set_end(Point2(size.width - vmin.width, size.height));

	int visible_rows = get_visible_rows();
	int total_rows = get_total_visible_rows();
	if (scroll_past_end_of_file_enabled) {
		total_rows += visible_rows - 1;
	}

	int visible_width = size.width - cache.style_normal->get_minimum_size().width;
	int total_width = text.get_max_width(true) + vmin.x;

	if (line_numbers) {
		total_width += cache.line_number_w;
	}

	if (draw_breakpoint_gutter || draw_bookmark_gutter) {
		total_width += cache.breakpoint_gutter_width;
	}

	if (draw_info_gutter) {
		total_width += cache.info_gutter_width;
	}

	if (draw_fold_gutter) {
		total_width += cache.fold_gutter_width;
	}

	if (draw_minimap) {
		total_width += cache.minimap_width;
	}

	updating_scrolls = true;

	if (total_rows > visible_rows) {
		v_scroll->show();
		v_scroll->set_max(total_rows + get_visible_rows_offset());
		v_scroll->set_page(visible_rows + get_visible_rows_offset());
		if (smooth_scroll_enabled) {
			v_scroll->set_step(0.25);
		} else {
			v_scroll->set_step(1);
		}
		set_v_scroll(get_v_scroll());

	} else {
		cursor.line_ofs = 0;
		cursor.wrap_ofs = 0;
		v_scroll->set_value(0);
		v_scroll->set_max(0);
		v_scroll->hide();
	}

	if (total_width > visible_width && !is_wrap_enabled()) {
		h_scroll->show();
		h_scroll->set_max(total_width);
		h_scroll->set_page(visible_width);
		if (cursor.x_ofs > (total_width - visible_width)) {
			cursor.x_ofs = (total_width - visible_width);
		}
		if (fabs(h_scroll->get_value() - (double)cursor.x_ofs) >= 1) {
			h_scroll->set_value(cursor.x_ofs);
		}

	} else {
		cursor.x_ofs = 0;
		h_scroll->set_value(0);
		h_scroll->set_max(0);
		h_scroll->hide();
	}

	updating_scrolls = false;
}

void TextEdit::_click_selection_held() {
	// Warning: is_mouse_button_pressed(BUTTON_LEFT) returns false for double+ clicks, so this doesn't work for MODE_WORD
	// and MODE_LINE. However, moving the mouse triggers _gui_input, which calls these functions too, so that's not a huge problem.
	// I'm unsure if there's an actual fix that doesn't have a ton of side effects.
	if (Input::get_singleton()->is_mouse_button_pressed(BUTTON_LEFT) && selection.selecting_mode != Selection::MODE_NONE) {
		switch (selection.selecting_mode) {
			case Selection::MODE_POINTER: {
				_update_selection_mode_pointer();
			} break;
			case Selection::MODE_WORD: {
				_update_selection_mode_word();
			} break;
			case Selection::MODE_LINE: {
				_update_selection_mode_line();
			} break;
			default: {
				break;
			}
		}
	} else {
		click_select_held->stop();
	}
}

void TextEdit::_update_selection_mode_pointer() {
	selection.drag_attempt = false;
	dragging_selection = true;
	Point2 mp = get_local_mouse_position();

	int row, col;
	_get_mouse_pos(Point2i(mp.x, mp.y), row, col);

	select(selection.selecting_line, selection.selecting_column, row, col);

	cursor_set_line(row, false);
	cursor_set_column(col);
	update();

	click_select_held->start();
}

void TextEdit::_update_selection_mode_word() {
	selection.drag_attempt = false;
	dragging_selection = true;
	Point2 mp = get_local_mouse_position();

	int row, col;
	_get_mouse_pos(Point2i(mp.x, mp.y), row, col);

	String line = text[row];
	int beg = CLAMP(col, 0, line.length());
	// If its the first selection and on whitespace make sure we grab the word instead.
	if (!selection.active) {
		while (beg > 0 && line[beg] <= 32) {
			beg--;
		}
	}
	int end = beg;
	bool symbol = beg < line.length() && _is_symbol(line[beg]);

	// Get the word end and begin points.
	while (beg > 0 && line[beg - 1] > 32 && (symbol == _is_symbol(line[beg - 1]))) {
		beg--;
	}
	while (end < line.length() && line[end + 1] > 32 && (symbol == _is_symbol(line[end + 1]))) {
		end++;
	}
	if (end < line.length()) {
		end += 1;
	}

	// Initial selection.
	if (!selection.active) {
		select(row, beg, row, end);
		selection.selecting_column = beg;
		selection.selected_word_beg = beg;
		selection.selected_word_end = end;
		selection.selected_word_origin = beg;
		cursor_set_line(selection.to_line, false);
		cursor_set_column(selection.to_column);
	} else {
		if ((col <= selection.selected_word_origin && row == selection.selecting_line) || row < selection.selecting_line) {
			selection.selecting_column = selection.selected_word_end;
			select(row, beg, selection.selecting_line, selection.selected_word_end);
			cursor_set_line(selection.from_line, false);
			cursor_set_column(selection.from_column);
		} else {
			selection.selecting_column = selection.selected_word_beg;
			select(selection.selecting_line, selection.selected_word_beg, row, end);
			cursor_set_line(selection.to_line, false);
			cursor_set_column(selection.to_column);
		}
	}

	if (OS::get_singleton()->has_feature("primary_clipboard")) {
		OS::get_singleton()->set_clipboard_primary(get_selection_text());
	}

	update();

	click_select_held->start();
}

void TextEdit::_update_selection_mode_line() {
	selection.drag_attempt = false;
	dragging_selection = true;
	Point2 mp = get_local_mouse_position();

	int row, col;
	_get_mouse_pos(Point2i(mp.x, mp.y), row, col);

	col = 0;
	if (row < selection.selecting_line) {
		// Cursor is above us.
		cursor_set_line(row - 1, false);
		selection.selecting_column = text[selection.selecting_line].length();
	} else {
		// Cursor is below us.
		cursor_set_line(row + 1, false);
		selection.selecting_column = 0;
		col = text[row].length();
	}
	cursor_set_column(0);

	select(selection.selecting_line, selection.selecting_column, row, col);
	if (OS::get_singleton()->has_feature("primary_clipboard")) {
		OS::get_singleton()->set_clipboard_primary(get_selection_text());
	}
	update();

	click_select_held->start();
}

void TextEdit::_update_minimap_hover() {
	const Point2 mp = get_local_mouse_position();
	const int xmargin_end = get_size().width - cache.style_normal->get_margin(MARGIN_RIGHT);

	const bool hovering_sidebar = mp.x > xmargin_end - minimap_width && mp.x < xmargin_end;
	if (!hovering_sidebar) {
		if (hovering_minimap) {
			// Only redraw if the hovering status changed.
			hovering_minimap = false;
			update();
		}

		// Return early to avoid running the operations below when not needed.
		return;
	}

	int row;
	_get_minimap_mouse_row(Point2i(mp.x, mp.y), row);

	const bool new_hovering_minimap = row >= get_first_visible_line() && row <= get_last_full_visible_line();
	if (new_hovering_minimap != hovering_minimap) {
		// Only redraw if the hovering status changed.
		hovering_minimap = new_hovering_minimap;
		update();
	}
}

void TextEdit::_update_minimap_click() {
	Point2 mp = get_local_mouse_position();

	int xmargin_end = get_size().width - cache.style_normal->get_margin(MARGIN_RIGHT);
	if (!dragging_minimap && (mp.x < xmargin_end - minimap_width || mp.y > xmargin_end)) {
		minimap_clicked = false;
		return;
	}
	minimap_clicked = true;
	dragging_minimap = true;

	int row;
	_get_minimap_mouse_row(Point2i(mp.x, mp.y), row);

	if (row >= get_first_visible_line() && (row < get_last_full_visible_line() || row >= (text.size() - 1))) {
		minimap_scroll_ratio = v_scroll->get_as_ratio();
		minimap_scroll_click_pos = mp.y;
		can_drag_minimap = true;
		return;
	}

	int wi;
	int first_line = row - num_lines_from_rows(row, 0, -get_visible_rows() / 2, wi) + 1;
	double delta = get_scroll_pos_for_line(first_line, wi) - get_v_scroll();
	if (delta < 0) {
		_scroll_up(-delta);
	} else {
		_scroll_down(delta);
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

	Point2 mp = get_local_mouse_position();
	double diff = (mp.y - minimap_scroll_click_pos) / control_height;
	v_scroll->set_as_ratio(minimap_scroll_ratio + diff);
}

void TextEdit::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			_update_caches();
			if (cursor_changed_dirty) {
				MessageQueue::get_singleton()->push_call(this, "_cursor_changed_emit");
			}
			if (text_changed_dirty) {
				MessageQueue::get_singleton()->push_call(this, "_text_changed_emit");
			}
			_update_wrap_at();
		} break;
		case NOTIFICATION_RESIZED: {
			_update_scrollbars();
			_update_wrap_at();
		} break;
		case NOTIFICATION_VISIBILITY_CHANGED: {
			if (is_visible()) {
				call_deferred("_update_scrollbars");
				call_deferred("_update_wrap_at");
			}
		} break;
		case NOTIFICATION_THEME_CHANGED: {
			_update_caches();
			_update_wrap_at();
			syntax_highlighting_cache.clear();
		} break;
		case MainLoop::NOTIFICATION_WM_FOCUS_IN: {
			window_has_focus = true;
			draw_caret = true;
			update();
		} break;
		case MainLoop::NOTIFICATION_WM_FOCUS_OUT: {
			window_has_focus = false;
			draw_caret = false;
			update();
		} break;
		case NOTIFICATION_INTERNAL_PHYSICS_PROCESS: {
			if (scrolling && get_v_scroll() != target_v_scroll) {
				double target_y = target_v_scroll - get_v_scroll();
				double dist = sqrt(target_y * target_y);
				// To ensure minimap is responsive override the speed setting.
				double vel = ((target_y / dist) * ((minimap_clicked) ? 3000 : v_scroll_speed)) * get_physics_process_delta_time();

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
				// Size may not be the final one, so attempts to ensure cursor was visible may have failed.
				adjust_viewport_to_cursor();
				first_draw = false;
			}
			Size2 size = get_size();
			if ((!has_focus() && !menu->has_focus()) || !window_has_focus) {
				draw_caret = false;
			}

			if (draw_breakpoint_gutter || draw_bookmark_gutter) {
				breakpoint_gutter_width = (get_row_height() * 55) / 100;
				cache.breakpoint_gutter_width = breakpoint_gutter_width;
			} else {
				cache.breakpoint_gutter_width = 0;
			}

			if (draw_info_gutter) {
				info_gutter_width = (get_row_height());
				cache.info_gutter_width = info_gutter_width;
			} else {
				cache.info_gutter_width = 0;
			}

			if (draw_fold_gutter) {
				fold_gutter_width = (get_row_height() * 55) / 100;
				cache.fold_gutter_width = fold_gutter_width;
			} else {
				cache.fold_gutter_width = 0;
			}

			cache.minimap_width = 0;
			if (draw_minimap) {
				cache.minimap_width = minimap_width;
			}

			int line_number_char_count = 0;

			{
				int lc = text.size();
				cache.line_number_w = 0;
				while (lc) {
					cache.line_number_w += 1;
					lc /= 10;
				};

				if (line_numbers) {
					line_number_char_count = cache.line_number_w;
					cache.line_number_w = (cache.line_number_w + 1) * cache.font->get_char_size('0').width;
				} else {
					cache.line_number_w = 0;
				}
			}
			_update_scrollbars();

			RID ci = get_canvas_item();
			VisualServer::get_singleton()->canvas_item_set_clip(get_canvas_item(), true);
			int xmargin_beg = cache.style_normal->get_margin(MARGIN_LEFT) + cache.line_number_w + cache.breakpoint_gutter_width + cache.fold_gutter_width + cache.info_gutter_width;

			int xmargin_end = size.width - cache.style_normal->get_margin(MARGIN_RIGHT) - cache.minimap_width;
			// Let's do it easy for now.
			cache.style_normal->draw(ci, Rect2(Point2(), size));
			if (readonly) {
				cache.style_readonly->draw(ci, Rect2(Point2(), size));
				draw_caret = is_caret_on_readonly();
			}
			if (has_focus()) {
				cache.style_focus->draw(ci, Rect2(Point2(), size));
			}

			int ascent = cache.font->get_ascent();

			int visible_rows = get_visible_rows() + 1;

			Color color = readonly ? cache.font_color_readonly : cache.font_color;

			if (syntax_coloring) {
				if (cache.background_color.a > 0.01) {
					VisualServer::get_singleton()->canvas_item_add_rect(ci, Rect2(Point2i(), get_size()), cache.background_color);
				}
			}

			if (line_length_guidelines) {
				const int hard_x = xmargin_beg + (int)cache.font->get_char_size('0').width * line_length_guideline_hard_col - cursor.x_ofs;
				if (hard_x > xmargin_beg && hard_x < xmargin_end) {
					VisualServer::get_singleton()->canvas_item_add_line(ci, Point2(hard_x, 0), Point2(hard_x, size.height), cache.line_length_guideline_color);
				}

				// Draw a "Soft" line length guideline, less visible than the hard line length guideline.
				// It's usually set to a lower column compared to the hard line length guideline.
				// Only drawn if its column differs from the hard line length guideline.
				const int soft_x = xmargin_beg + (int)cache.font->get_char_size('0').width * line_length_guideline_soft_col - cursor.x_ofs;
				if (hard_x != soft_x && soft_x > xmargin_beg && soft_x < xmargin_end) {
					VisualServer::get_singleton()->canvas_item_add_line(ci, Point2(soft_x, 0), Point2(soft_x, size.height), cache.line_length_guideline_color * Color(1, 1, 1, 0.5));
				}
			}

			int brace_open_match_line = -1;
			int brace_open_match_column = -1;
			bool brace_open_matching = false;
			bool brace_open_mismatch = false;
			int brace_close_match_line = -1;
			int brace_close_match_column = -1;
			bool brace_close_matching = false;
			bool brace_close_mismatch = false;

			if (brace_matching_enabled && cursor.line >= 0 && cursor.line < text.size() && cursor.column >= 0) {
				if (cursor.column < text[cursor.line].length()) {
					// Check for open.
					CharType c = text[cursor.line][cursor.column];
					CharType closec = 0;

					if (c == '[') {
						closec = ']';
					} else if (c == '{') {
						closec = '}';
					} else if (c == '(') {
						closec = ')';
					}

					if (closec != 0) {
						int stack = 1;

						for (int i = cursor.line; i < text.size(); i++) {
							int from = i == cursor.line ? cursor.column + 1 : 0;
							for (int j = from; j < text[i].length(); j++) {
								CharType cc = text[i][j];
								// Ignore any brackets inside a string.
								if (cc == '"' || cc == '\'') {
									CharType quotation = cc;
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
									brace_open_match_line = i;
									brace_open_match_column = j;
									brace_open_matching = true;

									break;
								}
							}
							if (brace_open_match_line != -1) {
								break;
							}
						}

						if (!brace_open_matching) {
							brace_open_mismatch = true;
						}
					}
				}

				if (cursor.column > 0) {
					CharType c = text[cursor.line][cursor.column - 1];
					CharType closec = 0;

					if (c == ']') {
						closec = '[';
					} else if (c == '}') {
						closec = '{';
					} else if (c == ')') {
						closec = '(';
					}

					if (closec != 0) {
						int stack = 1;

						for (int i = cursor.line; i >= 0; i--) {
							int from = i == cursor.line ? cursor.column - 2 : text[i].length() - 1;
							for (int j = from; j >= 0; j--) {
								CharType cc = text[i][j];
								// Ignore any brackets inside a string.
								if (cc == '"' || cc == '\'') {
									CharType quotation = cc;
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
									brace_close_match_line = i;
									brace_close_match_column = j;
									brace_close_matching = true;

									break;
								}
							}
							if (brace_close_match_line != -1) {
								break;
							}
						}

						if (!brace_close_matching) {
							brace_close_mismatch = true;
						}
					}
				}
			}

			Point2 cursor_pos;
			bool is_cursor_visible = false;
			int cursor_insert_offset_y = 0;

			// Get the highlighted words.
			String highlighted_text = get_selection_text();

			// Check if highlighted words contains only whitespaces (tabs or spaces).
			bool only_whitespaces_highlighted = highlighted_text.strip_edges() == String();

			String line_num_padding = line_numbers_zero_padded ? "0" : " ";

			int cursor_wrap_index = get_cursor_wrap_index();

			FontDrawer drawer(cache.font, Color(1, 1, 1));

			int first_visible_line = get_first_visible_line() - 1;
			int draw_amount = visible_rows + (smooth_scroll_enabled ? 1 : 0);
			draw_amount += times_line_wraps(first_visible_line + 1);

			// Draw minimap.
			if (draw_minimap) {
				int minimap_visible_lines = _get_minimap_visible_rows();
				int minimap_line_height = (minimap_char_size.y + minimap_line_spacing);
				int minimap_tab_size = minimap_char_size.x * indent_size;

				// calculate viewport size and y offset
				int viewport_height = (draw_amount - 1) * minimap_line_height;
				int control_height = _get_control_height() - viewport_height;
				int viewport_offset_y = round(get_scroll_pos_for_line(first_visible_line + 1) * control_height) / ((v_scroll->get_max() <= minimap_visible_lines) ? (minimap_visible_lines - draw_amount) : (v_scroll->get_max() - draw_amount));

				// calculate the first line.
				int num_lines_before = round((viewport_offset_y) / minimap_line_height);
				int wi;
				int minimap_line = (v_scroll->get_max() <= minimap_visible_lines) ? -1 : first_visible_line;
				if (minimap_line >= 0) {
					minimap_line -= num_lines_from_rows(first_visible_line, 0, -num_lines_before, wi);
					minimap_line -= (minimap_line > 0 && smooth_scroll_enabled ? 1 : 0);
				}
				int minimap_draw_amount = minimap_visible_lines + times_line_wraps(minimap_line + 1);

				// Draw the minimap.

				// Add visual feedback when dragging or hovering the the visible area rectangle.
				float viewport_alpha;
				if (dragging_minimap) {
					viewport_alpha = 0.25;
				} else if (hovering_minimap) {
					viewport_alpha = 0.175;
				} else {
					viewport_alpha = 0.1;
				}

				const Color viewport_color = (cache.background_color.get_v() < 0.5) ? Color(1, 1, 1, viewport_alpha) : Color(0, 0, 0, viewport_alpha);
				VisualServer::get_singleton()->canvas_item_add_rect(ci, Rect2((xmargin_end + 2), viewport_offset_y, cache.minimap_width, viewport_height), viewport_color);
				for (int i = 0; i < minimap_draw_amount; i++) {
					minimap_line++;

					if (minimap_line < 0 || minimap_line >= (int)text.size()) {
						break;
					}

					while (is_line_hidden(minimap_line)) {
						minimap_line++;
						if (minimap_line < 0 || minimap_line >= (int)text.size()) {
							break;
						}
					}

					if (minimap_line < 0 || minimap_line >= (int)text.size()) {
						break;
					}

					Map<int, HighlighterInfo> color_map;
					if (syntax_coloring) {
						color_map = _get_line_syntax_highlighting(minimap_line);
					}

					Color current_color = cache.font_color;
					if (readonly) {
						current_color = cache.font_color_readonly;
					}

					Vector<String> wrap_rows = get_wrap_rows_text(minimap_line);
					int line_wrap_amount = times_line_wraps(minimap_line);
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
						if (indent_px >= wrap_at) {
							indent_px = 0;
						}
						indent_px = minimap_char_size.x * indent_px;

						if (line_wrap_index > 0) {
							last_wrap_column += wrap_rows[line_wrap_index - 1].length();
						}

						if (minimap_line == cursor.line && cursor_wrap_index == line_wrap_index && highlight_current_line) {
							VisualServer::get_singleton()->canvas_item_add_rect(ci, Rect2((xmargin_end + 2), i * 3, cache.minimap_width, 2), cache.current_line_color);
						}

						Color previous_color;
						int characters = 0;
						int tabs = 0;
						for (int j = 0; j < str.length(); j++) {
							if (syntax_coloring) {
								if (color_map.has(last_wrap_column + j)) {
									current_color = color_map[last_wrap_column + j].color;
									if (readonly) {
										current_color.a = cache.font_color_readonly.a;
									}
								}
								color = current_color;
							}

							if (j == 0) {
								previous_color = color;
							}

							int xpos = indent_px + ((xmargin_end + minimap_char_size.x) + (minimap_char_size.x * j)) + tabs;
							bool out_of_bounds = (xpos >= xmargin_end + cache.minimap_width);

							bool is_whitespace = _is_whitespace(str[j]);
							if (!is_whitespace) {
								characters++;

								if (j < str.length() - 1 && color == previous_color && !out_of_bounds) {
									continue;
								}

								// If we've changed colour we are at the start of a new section, therefore we need to go back to the end
								// of the previous section to draw it, we'll also add the character back on.
								if (color != previous_color) {
									characters--;
									j--;

									if (str[j] == '\t') {
										tabs -= minimap_tab_size;
									}
								}
							}

							if (characters > 0) {
								previous_color.a *= 0.6;
								// take one for zero indexing, and if we hit whitespace / the end of a word.
								int chars = MAX(0, (j - (characters - 1)) - (is_whitespace ? 1 : 0)) + 1;
								int char_x_ofs = indent_px + ((xmargin_end + minimap_char_size.x) + (minimap_char_size.x * chars)) + tabs;
								VisualServer::get_singleton()->canvas_item_add_rect(ci, Rect2(Point2(char_x_ofs, minimap_line_height * i), Point2(minimap_char_size.x * characters, minimap_char_size.y)), previous_color);
							}

							if (out_of_bounds) {
								break;
							}

							// re-adjust if we went backwards.
							if (color != previous_color && !is_whitespace) {
								characters++;
							}

							if (str[j] == '\t') {
								tabs += minimap_tab_size;
							}

							previous_color = color;
							characters = 0;
						}
					}
				}
			}

			int top_limit_y = 0;
			int bottom_limit_y = get_size().height;
			if (readonly) {
				top_limit_y += cache.style_readonly->get_margin(MARGIN_BOTTOM);
				bottom_limit_y -= cache.style_readonly->get_margin(MARGIN_BOTTOM);
			} else {
				top_limit_y += cache.style_normal->get_margin(MARGIN_TOP);
				bottom_limit_y -= cache.style_normal->get_margin(MARGIN_TOP);
			}

			// Draw main text.
			line_drawing_cache.clear();
			int line = first_visible_line;
			for (int i = 0; i < draw_amount; i++) {
				line++;

				if (line < 0 || line >= (int)text.size()) {
					continue;
				}

				while (is_line_hidden(line)) {
					line++;
					if (line < 0 || line >= (int)text.size()) {
						break;
					}
				}

				if (line < 0 || line >= (int)text.size()) {
					continue;
				}

				const String &fullstr = text[line];
				LineDrawingCache cache_entry;

				Map<int, HighlighterInfo> color_map;
				if (syntax_coloring) {
					color_map = _get_line_syntax_highlighting(line);
				}
				// Ensure we at least use the font color.
				Color current_color = readonly ? cache.font_color_readonly : cache.font_color;

				bool underlined = false;

				Vector<String> wrap_rows = get_wrap_rows_text(line);
				int line_wrap_amount = times_line_wraps(line);
				int last_wrap_column = 0;
				int wrap_column_offset = 0;

				for (int line_wrap_index = 0; line_wrap_index < line_wrap_amount + 1; line_wrap_index++) {
					if (line_wrap_index != 0) {
						i++;
						if (i >= draw_amount) {
							break;
						}
					}

					const String &str = wrap_rows[line_wrap_index];
					int indent_px = line_wrap_index != 0 ? get_indent_level(line) * cache.font->get_char_size(' ').width : 0;
					if (indent_px >= wrap_at) {
						indent_px = 0;
					}

					if (line_wrap_index > 0) {
						last_wrap_column += wrap_rows[line_wrap_index - 1].length();
					}

					int char_margin = xmargin_beg - cursor.x_ofs;
					char_margin += indent_px;
					int char_ofs = 0;

					int ofs_x = 0;
					int ofs_y = 0;
					if (readonly) {
						ofs_x = cache.style_readonly->get_offset().x / 2;
						ofs_x -= cache.style_normal->get_offset().x / 2;
						ofs_y = cache.style_readonly->get_offset().y / 2;
					} else {
						ofs_y = cache.style_normal->get_offset().y / 2;
					}

					ofs_y += (i * get_row_height() + cache.line_spacing / 2);
					ofs_y -= cursor.wrap_ofs * get_row_height();
					ofs_y -= get_v_scroll_offset() * get_row_height();

					bool clipped = false;
					if (ofs_y + get_row_height() < top_limit_y) {
						// Line is outside the top margin, clip current line.
						// Still need to go through the process to prepare color changes for next lines.
						clipped = true;
					}

					if (ofs_y > bottom_limit_y) {
						// Line is outside the bottom margin, clip any remaining text.
						i = draw_amount;
						break;
					}

					// Check if line contains highlighted word.
					int highlighted_text_col = -1;
					int search_text_col = -1;
					int highlighted_word_col = -1;

					if (!search_text.empty()) {
						search_text_col = _get_column_pos_of_word(search_text, str, search_flags, 0);
					}

					if (highlighted_text.length() != 0 && highlighted_text != search_text) {
						highlighted_text_col = _get_column_pos_of_word(highlighted_text, str, SEARCH_MATCH_CASE | SEARCH_WHOLE_WORDS, 0);
					}

					if (select_identifiers_enabled && highlighted_word.length() != 0) {
						if (_is_char(highlighted_word[0]) || highlighted_word[0] == '.') {
							highlighted_word_col = _get_column_pos_of_word(highlighted_word, fullstr, SEARCH_MATCH_CASE | SEARCH_WHOLE_WORDS, 0);
						}
					}

					if (text.is_marked(line)) {
						VisualServer::get_singleton()->canvas_item_add_rect(ci, Rect2(xmargin_beg + ofs_x, ofs_y, xmargin_end - xmargin_beg, get_row_height()), cache.mark_color);
					}

					if (str.length() == 0) {
						// Draw line background if empty as we won't loop at at all.
						if (line == cursor.line && cursor_wrap_index == line_wrap_index && highlight_current_line) {
							VisualServer::get_singleton()->canvas_item_add_rect(ci, Rect2(ofs_x, ofs_y, xmargin_end, get_row_height()), cache.current_line_color);
						}

						// Give visual indication of empty selected line.
						if (selection.active && line >= selection.from_line && line <= selection.to_line && char_margin >= xmargin_beg) {
							int char_w = cache.font->get_char_size(' ').width;
							VisualServer::get_singleton()->canvas_item_add_rect(ci, Rect2(xmargin_beg + ofs_x, ofs_y, char_w, get_row_height()), cache.selection_color);
						}
					} else {
						// If it has text, then draw current line marker in the margin, as line number etc will draw over it, draw the rest of line marker later.
						if (line == cursor.line && cursor_wrap_index == line_wrap_index && highlight_current_line) {
							VisualServer::get_singleton()->canvas_item_add_rect(ci, Rect2(0, ofs_y, xmargin_beg + ofs_x, get_row_height()), cache.current_line_color);
						}
					}

					if (line_wrap_index == 0) {
						// Only do these if we are on the first wrapped part of a line.

						cache_entry.y_offset = ofs_y;

						if (text.is_breakpoint(line) && !draw_breakpoint_gutter) {
#ifdef TOOLS_ENABLED
							VisualServer::get_singleton()->canvas_item_add_rect(ci, Rect2(xmargin_beg + ofs_x, ofs_y + get_row_height() - EDSCALE, xmargin_end - xmargin_beg, EDSCALE), cache.breakpoint_color);
#else
							VisualServer::get_singleton()->canvas_item_add_rect(ci, Rect2(xmargin_beg + ofs_x, ofs_y, xmargin_end - xmargin_beg, get_row_height()), cache.breakpoint_color);
#endif
						}

						// Draw bookmark marker.
						if (text.is_bookmark(line)) {
							if (draw_bookmark_gutter) {
								int vertical_gap = (get_row_height() * 40) / 100;
								int horizontal_gap = (cache.breakpoint_gutter_width * 30) / 100;
								int marker_radius = get_row_height() - (vertical_gap * 2);
								VisualServer::get_singleton()->canvas_item_add_circle(ci, Point2(cache.style_normal->get_margin(MARGIN_LEFT) + horizontal_gap - 2 + marker_radius / 2, ofs_y + vertical_gap + marker_radius / 2), marker_radius, Color(cache.bookmark_color.r, cache.bookmark_color.g, cache.bookmark_color.b));
							}
						}

						// Draw breakpoint marker.
						if (text.is_breakpoint(line)) {
							if (draw_breakpoint_gutter) {
								int vertical_gap = (get_row_height() * 40) / 100;
								int horizontal_gap = (cache.breakpoint_gutter_width * 30) / 100;
								int marker_height = get_row_height() - (vertical_gap * 2);
								int marker_width = cache.breakpoint_gutter_width - (horizontal_gap * 2);
								// No transparency on marker.
								VisualServer::get_singleton()->canvas_item_add_rect(ci, Rect2(cache.style_normal->get_margin(MARGIN_LEFT) + horizontal_gap - 2, ofs_y + vertical_gap, marker_width, marker_height), Color(cache.breakpoint_color.r, cache.breakpoint_color.g, cache.breakpoint_color.b));
							}
						}

						// Draw info icons.
						if (draw_info_gutter && text.has_info_icon(line)) {
							int vertical_gap = (get_row_height() * 40) / 100;
							int horizontal_gap = (cache.info_gutter_width * 30) / 100;
							int gutter_left = cache.style_normal->get_margin(MARGIN_LEFT) + cache.breakpoint_gutter_width;

							Ref<Texture> info_icon = text.get_info_icon(line);
							// Ensure the icon fits the gutter size.
							Size2i icon_size = info_icon->get_size();
							if (icon_size.width > cache.info_gutter_width - horizontal_gap) {
								icon_size.width = cache.info_gutter_width - horizontal_gap;
							}
							if (icon_size.height > get_row_height() - horizontal_gap) {
								icon_size.height = get_row_height() - horizontal_gap;
							}

							Size2i icon_pos;
							int xofs = horizontal_gap - (info_icon->get_width() / 4);
							int yofs = vertical_gap - (info_icon->get_height() / 4);
							icon_pos.x = gutter_left + xofs + ofs_x;
							icon_pos.y = ofs_y + yofs;

							draw_texture_rect(info_icon, Rect2(icon_pos, icon_size));
						}

						// Draw execution marker.
						if (executing_line == line) {
							if (draw_breakpoint_gutter) {
								int icon_extra_size = 4;
								int vertical_gap = (get_row_height() * 40) / 100;
								int horizontal_gap = (cache.breakpoint_gutter_width * 30) / 100;
								int marker_height = get_row_height() - (vertical_gap * 2) + icon_extra_size;
								int marker_width = cache.breakpoint_gutter_width - (horizontal_gap * 2) + icon_extra_size;
								cache.executing_icon->draw_rect(ci, Rect2(cache.style_normal->get_margin(MARGIN_LEFT) + horizontal_gap - 2 - icon_extra_size / 2, ofs_y + vertical_gap - icon_extra_size / 2, marker_width, marker_height), false, Color(cache.executing_line_color.r, cache.executing_line_color.g, cache.executing_line_color.b));
							} else {
#ifdef TOOLS_ENABLED
								VisualServer::get_singleton()->canvas_item_add_rect(ci, Rect2(xmargin_beg + ofs_x, ofs_y + get_row_height() - EDSCALE, xmargin_end - xmargin_beg, EDSCALE), cache.executing_line_color);
#else
								VisualServer::get_singleton()->canvas_item_add_rect(ci, Rect2(xmargin_beg + ofs_x, ofs_y, xmargin_end - xmargin_beg, get_row_height()), cache.executing_line_color);
#endif
							}
						}

						// Draw fold markers.
						if (draw_fold_gutter) {
							int horizontal_gap = (cache.fold_gutter_width * 30) / 100;
							int gutter_left = cache.style_normal->get_margin(MARGIN_LEFT) + cache.breakpoint_gutter_width + cache.line_number_w + cache.info_gutter_width;
							if (is_folded(line)) {
								int xofs = horizontal_gap - (cache.can_fold_icon->get_width()) / 2;
								int yofs = (get_row_height() - cache.folded_icon->get_height()) / 2;
								cache.folded_icon->draw(ci, Point2(gutter_left + xofs + ofs_x, ofs_y + yofs), cache.code_folding_color);
							} else if (can_fold(line)) {
								int xofs = -cache.can_fold_icon->get_width() / 2 - horizontal_gap + 3;
								int yofs = (get_row_height() - cache.can_fold_icon->get_height()) / 2;
								cache.can_fold_icon->draw(ci, Point2(gutter_left + xofs + ofs_x, ofs_y + yofs), cache.code_folding_color);
							}
						}

						// Draw line numbers.
						if (cache.line_number_w) {
							int yofs = ofs_y + (get_row_height() - cache.font->get_height()) / 2;
							String fc = String::num(line + 1);
							while (fc.length() < line_number_char_count) {
								fc = line_num_padding + fc;
							}

							cache.font->draw(ci, Point2(cache.style_normal->get_margin(MARGIN_LEFT) + cache.breakpoint_gutter_width + cache.info_gutter_width + ofs_x, yofs + cache.font->get_ascent()), fc, text.is_safe(line) ? cache.safe_line_number_color : cache.line_number_color);
						}
					}

					int first_visible_char = str.length();
					int last_visible_char = 0;

					// Loop through characters in one line.
					int j = 0;
					for (; j < str.length(); j++) {
						if (syntax_coloring) {
							if (color_map.has(last_wrap_column + j)) {
								current_color = color_map[last_wrap_column + j].color;
								if (readonly && current_color.a > cache.font_color_readonly.a) {
									current_color.a = cache.font_color_readonly.a;
								}
							}
							color = current_color;
						}

						int char_w;

						// Handle tabulator.
						char_w = text.get_char_width(str[j], str[j + 1], char_ofs);

						if ((char_ofs + char_margin) < xmargin_beg) {
							char_ofs += char_w;

							// Line highlighting handle horizontal clipping.
							if (line == cursor.line && cursor_wrap_index == line_wrap_index && highlight_current_line) {
								if (j == str.length() - 1) {
									// End of line when last char is skipped.
									VisualServer::get_singleton()->canvas_item_add_rect(ci, Rect2(xmargin_beg + ofs_x, ofs_y, xmargin_end - (char_ofs + char_margin + char_w), get_row_height()), cache.current_line_color);
								} else if ((char_ofs + char_margin) > xmargin_beg) {
									// Char next to margin is skipped.
									VisualServer::get_singleton()->canvas_item_add_rect(ci, Rect2(xmargin_beg + ofs_x, ofs_y, (char_ofs + char_margin) - (xmargin_beg + ofs_x), get_row_height()), cache.current_line_color);
								}
							}
							continue;
						}

						if ((char_ofs + char_margin + char_w) >= xmargin_end) {
							break;
						}

						bool in_search_result = false;

						if (search_text_col != -1) {
							// If we are at the end check for new search result on same line.
							if (j >= search_text_col + search_text.length()) {
								search_text_col = _get_column_pos_of_word(search_text, str, search_flags, j);
							}

							in_search_result = j >= search_text_col && j < search_text_col + search_text.length();

							if (in_search_result) {
								VisualServer::get_singleton()->canvas_item_add_rect(ci, Rect2(Point2i(char_ofs + char_margin, ofs_y), Size2i(char_w, get_row_height())), cache.search_result_color);
							}
						}

						// Current line highlighting.
						bool in_selection = (selection.active && line >= selection.from_line && line <= selection.to_line && (line > selection.from_line || last_wrap_column + j >= selection.from_column) && (line < selection.to_line || last_wrap_column + j < selection.to_column));

						if (!clipped && line == cursor.line && cursor_wrap_index == line_wrap_index && highlight_current_line) {
							// Draw the wrap indent offset highlight.
							if (line_wrap_index != 0 && j == 0) {
								VisualServer::get_singleton()->canvas_item_add_rect(ci, Rect2(char_ofs + char_margin + ofs_x - indent_px, ofs_y, indent_px, get_row_height()), cache.current_line_color);
							}
							// If its the last char draw to end of the line.
							if (j == str.length() - 1) {
								VisualServer::get_singleton()->canvas_item_add_rect(ci, Rect2(char_ofs + char_margin + char_w + ofs_x, ofs_y, xmargin_end - (char_ofs + char_margin + char_w), get_row_height()), cache.current_line_color);
							}
							// Actual text.
							if (!in_selection) {
								VisualServer::get_singleton()->canvas_item_add_rect(ci, Rect2(Point2i(char_ofs + char_margin + ofs_x, ofs_y), Size2i(char_w, get_row_height())), cache.current_line_color);
							}
						}

						if (!clipped && in_selection) {
							VisualServer::get_singleton()->canvas_item_add_rect(ci, Rect2(Point2i(char_ofs + char_margin + ofs_x, ofs_y), Size2i(char_w, get_row_height())), cache.selection_color);
						}

						if (!clipped && in_search_result) {
							Color border_color = (line == search_result_line && j >= search_result_col && j < search_result_col + search_text.length()) ? cache.font_color : cache.search_result_border_color;

							VisualServer::get_singleton()->canvas_item_add_rect(ci, Rect2(Point2i(char_ofs + char_margin + ofs_x, ofs_y), Size2i(char_w, 1)), border_color);
							VisualServer::get_singleton()->canvas_item_add_rect(ci, Rect2(Point2i(char_ofs + char_margin + ofs_x, ofs_y + get_row_height() - 1), Size2i(char_w, 1)), border_color);

							if (j == search_text_col) {
								VisualServer::get_singleton()->canvas_item_add_rect(ci, Rect2(Point2i(char_ofs + char_margin + ofs_x, ofs_y), Size2i(1, get_row_height())), border_color);
							}
							if (j == search_text_col + search_text.length() - 1) {
								VisualServer::get_singleton()->canvas_item_add_rect(ci, Rect2(Point2i(char_ofs + char_margin + char_w + ofs_x - 1, ofs_y), Size2i(1, get_row_height())), border_color);
							}
						}

						if (!clipped && highlight_all_occurrences && !only_whitespaces_highlighted) {
							if (highlighted_text_col != -1) {
								// If we are at the end check for new word on same line.
								if (j > highlighted_text_col + highlighted_text.length()) {
									highlighted_text_col = _get_column_pos_of_word(highlighted_text, str, SEARCH_MATCH_CASE | SEARCH_WHOLE_WORDS, j);
								}

								bool in_highlighted_word = (j >= highlighted_text_col && j < highlighted_text_col + highlighted_text.length());

								// If this is the original highlighted text we don't want to highlight it again.
								if (cursor.line == line && cursor_wrap_index == line_wrap_index && (cursor.column >= highlighted_text_col && cursor.column <= highlighted_text_col + highlighted_text.length())) {
									in_highlighted_word = false;
								}

								if (in_highlighted_word) {
									VisualServer::get_singleton()->canvas_item_add_rect(ci, Rect2(Point2i(char_ofs + char_margin + ofs_x, ofs_y), Size2i(char_w, get_row_height())), cache.word_highlighted_color);
								}
							}
						}

						if (highlighted_word_col != -1) {
							if (j + last_wrap_column > highlighted_word_col + highlighted_word.length()) {
								highlighted_word_col = _get_column_pos_of_word(highlighted_word, fullstr, SEARCH_MATCH_CASE | SEARCH_WHOLE_WORDS, j + last_wrap_column);
							}
							underlined = (j + last_wrap_column >= highlighted_word_col && j + last_wrap_column < highlighted_word_col + highlighted_word.length());
						}

						if (brace_matching_enabled) {
							int yofs = ofs_y + (get_row_height() - cache.font->get_height()) / 2;
							if ((brace_open_match_line == line && brace_open_match_column == last_wrap_column + j) ||
									(cursor.column == last_wrap_column + j && cursor.line == line && cursor_wrap_index == line_wrap_index && (brace_open_matching || brace_open_mismatch))) {
								if (brace_open_mismatch) {
									color = cache.brace_mismatch_color;
								}
								drawer.draw_char(ci, Point2i(char_ofs + char_margin + ofs_x, yofs + ascent), '_', str[j + 1], in_selection && override_selected_font_color ? cache.font_color_selected : color);
							}

							if ((brace_close_match_line == line && brace_close_match_column == last_wrap_column + j) ||
									(cursor.column == last_wrap_column + j + 1 && cursor.line == line && cursor_wrap_index == line_wrap_index && (brace_close_matching || brace_close_mismatch))) {
								if (brace_close_mismatch) {
									color = cache.brace_mismatch_color;
								}
								drawer.draw_char(ci, Point2i(char_ofs + char_margin + ofs_x, yofs + ascent), '_', str[j + 1], in_selection && override_selected_font_color ? cache.font_color_selected : color);
							}
						}

						if (!clipped && cursor.column == last_wrap_column + j && cursor.line == line && cursor_wrap_index == line_wrap_index) {
							is_cursor_visible = true;
							cursor_pos = Point2i(char_ofs + char_margin + ofs_x, ofs_y);
							cursor_pos.y += (get_row_height() - cache.font->get_height()) / 2;

							if (insert_mode) {
								cursor_insert_offset_y = (cache.font->get_height() - 3);
								cursor_pos.y += cursor_insert_offset_y;
							}

							int caret_w = (str[j] == '\t') ? cache.font->get_char_size(' ').width : char_w;
							if (ime_text.length() > 0) {
								int ofs = 0;
								while (true) {
									if (ofs >= ime_text.length()) {
										break;
									}

									CharType cchar = ime_text[ofs];
									CharType next = ime_text[ofs + 1];
									int im_char_width = cache.font->get_char_size(cchar, next).width;

									if ((char_ofs + char_margin + im_char_width) >= xmargin_end) {
										break;
									}

									bool selected = ofs >= ime_selection.x && ofs < ime_selection.x + ime_selection.y;
									if (selected) {
										VisualServer::get_singleton()->canvas_item_add_rect(ci, Rect2(Point2(char_ofs + char_margin, ofs_y + get_row_height()), Size2(im_char_width, 3)), color);
									} else {
										VisualServer::get_singleton()->canvas_item_add_rect(ci, Rect2(Point2(char_ofs + char_margin, ofs_y + get_row_height()), Size2(im_char_width, 1)), color);
									}

									drawer.draw_char(ci, Point2(char_ofs + char_margin + ofs_x, ofs_y + ascent), cchar, next, color);

									char_ofs += im_char_width;
									ofs++;
								}
							}
							if (ime_text.length() == 0) {
								if (draw_caret || drag_caret_force_displayed) {
									if (insert_mode) {
#ifdef TOOLS_ENABLED
										int caret_h = (block_caret) ? 4 : 2 * EDSCALE;
#else
										int caret_h = (block_caret) ? 4 : 2;
#endif
										VisualServer::get_singleton()->canvas_item_add_rect(ci, Rect2(cursor_pos, Size2i(caret_w, caret_h)), cache.caret_color);
									} else {
#ifdef TOOLS_ENABLED
										caret_w = (block_caret) ? caret_w : 2 * EDSCALE;
#else
										caret_w = (block_caret) ? caret_w : 2;
#endif

										VisualServer::get_singleton()->canvas_item_add_rect(ci, Rect2(cursor_pos, Size2i(caret_w, cache.font->get_height())), cache.caret_color);
									}
								}
							}
						}

						if (!clipped) {
							if (cursor.column == last_wrap_column + j && cursor.line == line && cursor_wrap_index == line_wrap_index && block_caret && draw_caret && !insert_mode) {
								color = cache.caret_background_color;
							} else if (!syntax_coloring && block_caret) {
								color = readonly ? cache.font_color_readonly : cache.font_color;
							}

							if (str[j] >= 32) {
								int yofs = ofs_y + (get_row_height() - cache.font->get_height()) / 2;
								int w = drawer.draw_char(ci, Point2i(char_ofs + char_margin + ofs_x, yofs + ascent), str[j], str[j + 1], in_selection && override_selected_font_color ? cache.font_color_selected : color);
								if (underlined) {
									float line_width = 1.0;
#ifdef TOOLS_ENABLED
									line_width *= EDSCALE;
#endif

									draw_rect(Rect2(char_ofs + char_margin + ofs_x, yofs + ascent + 2, w, line_width), in_selection && override_selected_font_color ? cache.font_color_selected : color);
								}
							} else if (draw_tabs && str[j] == '\t') {
								int yofs = (get_row_height() - cache.tab_icon->get_height()) / 2;
								cache.tab_icon->draw(ci, Point2(char_ofs + char_margin + ofs_x, ofs_y + yofs), in_selection && override_selected_font_color ? cache.font_color_selected : color);
							}

							if (draw_spaces && str[j] == ' ') {
								int yofs = (get_row_height() - cache.space_icon->get_height()) / 2;
								cache.space_icon->draw(ci, Point2(char_ofs + char_margin + ofs_x, ofs_y + yofs), in_selection && override_selected_font_color ? cache.font_color_selected : color);
							}

							if (first_visible_char > j) {
								first_visible_char = j;
							}
							if (last_visible_char < j) {
								last_visible_char = j;
							}
						}

						char_ofs += char_w;

						if (line_wrap_index == line_wrap_amount && j == str.length() - 1 && is_folded(line)) {
							int yofs = (get_row_height() - cache.folded_eol_icon->get_height()) / 2;
							int xofs = cache.folded_eol_icon->get_width() / 2;
							Color eol_color = cache.code_folding_color;
							eol_color.a = 1;
							cache.folded_eol_icon->draw(ci, Point2(char_ofs + char_margin + xofs + ofs_x, ofs_y + yofs), eol_color);
						}
					}

					if (!clipped && cursor.column == (last_wrap_column + j) && cursor.line == line && cursor_wrap_index == line_wrap_index && (char_ofs + char_margin) >= xmargin_beg) {
						is_cursor_visible = true;
						cursor_pos = Point2i(char_ofs + char_margin + ofs_x, ofs_y);
						cursor_pos.y += (get_row_height() - cache.font->get_height()) / 2;

						if (insert_mode) {
							cursor_insert_offset_y = cache.font->get_height() - 3;
							cursor_pos.y += cursor_insert_offset_y;
						}
						if (ime_text.length() > 0) {
							int ofs = 0;
							while (true) {
								if (ofs >= ime_text.length()) {
									break;
								}

								CharType cchar = ime_text[ofs];
								CharType next = ime_text[ofs + 1];
								int im_char_width = cache.font->get_char_size(cchar, next).width;

								if ((char_ofs + char_margin + im_char_width) >= xmargin_end) {
									break;
								}

								bool selected = ofs >= ime_selection.x && ofs < ime_selection.x + ime_selection.y;
								if (selected) {
									VisualServer::get_singleton()->canvas_item_add_rect(ci, Rect2(Point2(char_ofs + char_margin, ofs_y + get_row_height()), Size2(im_char_width, 3)), color);
								} else {
									VisualServer::get_singleton()->canvas_item_add_rect(ci, Rect2(Point2(char_ofs + char_margin, ofs_y + get_row_height()), Size2(im_char_width, 1)), color);
								}

								drawer.draw_char(ci, Point2(char_ofs + char_margin + ofs_x, ofs_y + ascent), cchar, next, color);

								char_ofs += im_char_width;
								ofs++;
							}
						}
						if (ime_text.length() == 0) {
							if (draw_caret || drag_caret_force_displayed) {
								if (insert_mode) {
									int char_w = cache.font->get_char_size(' ').width;
#ifdef TOOLS_ENABLED
									int caret_h = (block_caret) ? 4 : 2 * EDSCALE;
#else
									int caret_h = (block_caret) ? 4 : 2;
#endif
									VisualServer::get_singleton()->canvas_item_add_rect(ci, Rect2(cursor_pos, Size2i(char_w, caret_h)), cache.caret_color);
								} else {
									int char_w = cache.font->get_char_size(' ').width;
#ifdef TOOLS_ENABLED
									int caret_w = (block_caret) ? char_w : 2 * EDSCALE;
#else
									int caret_w = (block_caret) ? char_w : 2;
#endif

									VisualServer::get_singleton()->canvas_item_add_rect(ci, Rect2(cursor_pos, Size2i(caret_w, cache.font->get_height())), cache.caret_color);
								}
							}
						}
					}

					cache_entry.first_visible_char.push_back(wrap_column_offset + first_visible_char);
					cache_entry.last_visible_char.push_back(wrap_column_offset + last_visible_char);

					wrap_column_offset += str.length();
				}

				line_drawing_cache[line] = cache_entry;
			}

			// Flush out any text in the drawer BEFORE
			// drawing the completion box, as we want the completion
			// box to overwrite the underlying text.
			drawer.flush();

			bool completion_below = false;
			if (completion_active && is_cursor_visible && completion_options.size() > 0) {
				// Completion panel

				const Ref<StyleBox> csb = get_stylebox("completion");
				const int maxlines = get_constant("completion_lines");
				const int cmax_width = get_constant("completion_max_width") * cache.font->get_char_size('x').x;
				const Color scrollc = get_color("completion_scroll_color");

				const int row_height = get_row_height();
				const int completion_options_size = completion_options.size();
				const int row_count = MIN(completion_options_size, maxlines);
				const int completion_rows_height = row_count * row_height;
				const int completion_base_width = cache.font->get_string_size(completion_base).width;

				int scroll_rectangle_width = get_constant("completion_scroll_width");
				int width = 0;

				// Compute max width of the panel based on the longest completion option.
				// Limit the number of results for automatic width calculation to avoid freezing while showing results.
				if (completion_options_size < 1000) {
					for (int i = 0; i < completion_options_size; i++) {
						int line_width = MIN(cache.font->get_string_size(completion_options[i].display).x, cmax_width);
						if (line_width > width) {
							width = line_width;
						}
					}
				} else {
					// Fall back to predetermined width.
					width = cmax_width;
				}

				// Add space for completion icons.
				const int icon_hsep = get_constant("hseparation", "ItemList");
				const Size2 icon_area_size(row_height, row_height);
				const int icon_area_width = icon_area_size.width + icon_hsep;
				width += icon_area_size.width + icon_hsep;

				const int line_from = CLAMP((completion_force_item_center < 0 ? completion_index : completion_force_item_center) - row_count / 2, 0, completion_options_size - row_count);

				for (int i = 0; i < row_count; i++) {
					int l = line_from + i;
					ERR_CONTINUE(l < 0 || l >= completion_options_size);
					if (completion_options[l].default_value.get_type() == Variant::COLOR) {
						width += icon_area_size.width;
						break;
					}
				}

				// Position completion panel
				completion_rect.size.width = width + 2;
				completion_rect.size.height = completion_rows_height;

				if (completion_options_size <= maxlines) {
					scroll_rectangle_width = 0;
				}

				const Point2 csb_offset = csb->get_offset();
				const int total_height = completion_rect.size.height + csb->get_minimum_size().y;
				const int ajdusted_cursor_y = cursor_pos.y - cursor_insert_offset_y - (get_row_height() - cache.font->get_height()) / 2;

				completion_rect.position.x = cursor_pos.x - completion_base_width - icon_area_width - csb_offset.x;

				if (ajdusted_cursor_y + row_height + total_height > get_size().height && ajdusted_cursor_y > total_height) {
					// Completion panel above the cursor line
					completion_rect.position.y = ajdusted_cursor_y - total_height;
				} else {
					// Completion panel below the cursor line
					completion_rect.position.y = ajdusted_cursor_y + row_height;
					completion_below = true;
				}

				draw_style_box(csb, Rect2(completion_rect.position - csb_offset, completion_rect.size + csb->get_minimum_size() + Size2(scroll_rectangle_width, 0)));

				if (cache.completion_background_color.a > 0.01) {
					VisualServer::get_singleton()->canvas_item_add_rect(ci, Rect2(completion_rect.position, completion_rect.size + Size2(scroll_rectangle_width, 0)), cache.completion_background_color);
				}
				VisualServer::get_singleton()->canvas_item_add_rect(ci, Rect2(Point2(completion_rect.position.x, completion_rect.position.y + (completion_index - line_from) * get_row_height()), Size2(completion_rect.size.width, get_row_height())), cache.completion_selected_color);
				draw_rect(Rect2(completion_rect.position + Vector2(icon_area_size.x + icon_hsep, 0), Size2(MIN(completion_base_width, completion_rect.size.width - (icon_area_size.x + icon_hsep)), completion_rect.size.height)), cache.completion_existing_color);

				for (int i = 0; i < row_count; i++) {
					int l = line_from + i;
					ERR_CONTINUE(l < 0 || l >= completion_options_size);
					Color text_color = cache.completion_font_color;
					for (int j = 0; j < color_regions.size(); j++) {
						if (completion_options[l].insert_text.begins_with(color_regions[j].begin_key)) {
							text_color = color_regions[j].color;
						}
					}
					int yofs = (get_row_height() - cache.font->get_height()) / 2;
					Point2 title_pos(completion_rect.position.x, completion_rect.position.y + i * get_row_height() + cache.font->get_ascent() + yofs);

					// Draw completion icon if it is valid.
					Ref<Texture> icon = completion_options[l].icon;
					Rect2 icon_area(completion_rect.position.x, completion_rect.position.y + i * get_row_height(), icon_area_size.width, icon_area_size.height);
					if (icon.is_valid()) {
						const real_t max_scale = 0.7f;
						const real_t side = max_scale * icon_area.size.width;
						real_t scale = MIN(side / icon->get_width(), side / icon->get_height());
						Size2 icon_size = icon->get_size() * scale;
						draw_texture_rect(icon, Rect2(icon_area.position + (icon_area.size - icon_size) / 2, icon_size));
					}

					title_pos.x = icon_area.position.x + icon_area.size.width + icon_hsep;

					if (completion_options[l].default_value.get_type() == Variant::COLOR) {
						draw_rect(Rect2(Point2(completion_rect.position.x + completion_rect.size.width - icon_area_size.x, icon_area.position.y), icon_area_size), (Color)completion_options[l].default_value);
					}

					draw_string(cache.font, title_pos, completion_options[l].display, text_color, completion_rect.size.width - (icon_area_size.x + icon_hsep));
				}

				if (scroll_rectangle_width) {
					// Draw a small scroll rectangle to show a position in the options.
					float r = (float)maxlines / completion_options_size;
					float o = (float)line_from / completion_options_size;
					draw_rect(Rect2(completion_rect.position.x + completion_rect.size.width, completion_rect.position.y + o * completion_rect.size.y, scroll_rectangle_width, completion_rect.size.y * r), scrollc);
				}

				completion_line_ofs = line_from;
			}

			// Check to see if the hint should be drawn.
			bool show_hint = false;
			if (is_cursor_visible && completion_hint != "") {
				if (completion_active) {
					if (completion_below && !callhint_below) {
						show_hint = true;
					} else if (!completion_below && callhint_below) {
						show_hint = true;
					}
				} else {
					show_hint = true;
				}
			}

			if (show_hint) {
				Ref<StyleBox> sb = get_stylebox("panel", "TooltipPanel");
				Ref<Font> font = cache.font;
				Color font_color = get_color("font_color", "TooltipLabel");

				int max_w = 0;
				int sc = completion_hint.get_slice_count("\n");
				int offset = 0;
				int spacing = 0;
				for (int i = 0; i < sc; i++) {
					String l = completion_hint.get_slice("\n", i);
					int len = font->get_string_size(l).x;
					max_w = MAX(len, max_w);
					if (i == 0) {
						offset = font->get_string_size(l.substr(0, l.find(String::chr(0xFFFF)))).x;
					} else {
						spacing += cache.line_spacing;
					}
				}

				Size2 size2 = Size2(max_w, sc * font->get_height() + spacing);
				Size2 minsize = size2 + sb->get_minimum_size();

				if (completion_hint_offset == -0xFFFF) {
					completion_hint_offset = cursor_pos.x - offset;
				}

				Point2 hint_ofs = Vector2(completion_hint_offset, cursor_pos.y - cursor_insert_offset_y - (get_row_height() - cache.font->get_height()) / 2) + callhint_offset;

				if (callhint_below) {
					hint_ofs.y += get_row_height() + sb->get_offset().y;
				} else {
					hint_ofs.y -= minsize.y + sb->get_offset().y;
				}

				draw_style_box(sb, Rect2(hint_ofs, minsize));

				spacing = 0;
				for (int i = 0; i < sc; i++) {
					int begin = 0;
					int end = 0;
					String l = completion_hint.get_slice("\n", i);

					if (l.find(String::chr(0xFFFF)) != -1) {
						begin = font->get_string_size(l.substr(0, l.find(String::chr(0xFFFF)))).x;
						end = font->get_string_size(l.substr(0, l.rfind(String::chr(0xFFFF)))).x;
					}

					Point2 round_ofs = hint_ofs + sb->get_offset() + Vector2(0, font->get_ascent() + font->get_height() * i + spacing);
					round_ofs = round_ofs.round();
					draw_string(font, round_ofs, l.replace(String::chr(0xFFFF), ""), font_color);
					if (end > 0) {
						Vector2 b = hint_ofs + sb->get_offset() + Vector2(begin, font->get_height() + font->get_height() * i + spacing - 1);
						draw_line(b, b + Vector2(end - begin, 0), font_color);
					}
					spacing += cache.line_spacing;
				}
			}

			if (has_focus()) {
				OS::get_singleton()->set_ime_active(true);
				OS::get_singleton()->set_ime_position(get_global_position() + cursor_pos + Point2(0, get_row_height()));
			}
		} break;
		case NOTIFICATION_FOCUS_ENTER: {
			if (caret_blink_enabled) {
				caret_blink_timer->start();
			} else {
				draw_caret = true;
			}

			OS::get_singleton()->set_ime_active(true);
			Point2 cursor_pos = Point2(cursor_get_column(), cursor_get_line()) * get_row_height();
			OS::get_singleton()->set_ime_position(get_global_position() + cursor_pos);

			if (OS::get_singleton()->has_virtual_keyboard() && virtual_keyboard_enabled) {
				int cursor_start = -1;
				int cursor_end = -1;

				if (!selection.active) {
					String full_text = _base_get_text(0, 0, cursor.line, cursor.column);

					cursor_start = full_text.length();
				} else {
					String pre_text = _base_get_text(0, 0, selection.from_line, selection.from_column);
					String post_text = _base_get_text(selection.from_line, selection.from_column, selection.to_line, selection.to_column);

					cursor_start = pre_text.length();
					cursor_end = cursor_start + post_text.length();
				}

				OS::get_singleton()->show_virtual_keyboard(get_text(), get_global_rect(), OS::KEYBOARD_TYPE_MULTILINE, -1, cursor_start, cursor_end);
			}
		} break;
		case NOTIFICATION_FOCUS_EXIT: {
			if (caret_blink_enabled) {
				caret_blink_timer->stop();
			}

			OS::get_singleton()->set_ime_position(Point2());
			OS::get_singleton()->set_ime_active(false);
			ime_text = "";
			ime_selection = Point2();

			if (OS::get_singleton()->has_virtual_keyboard() && virtual_keyboard_enabled) {
				OS::get_singleton()->hide_virtual_keyboard();
			}

			if (deselect_on_focus_loss_enabled && !popup_show) {
				deselect();
			}
			popup_show = false;
		} break;
		case MainLoop::NOTIFICATION_OS_IME_UPDATE: {
			if (has_focus()) {
				ime_text = OS::get_singleton()->get_ime_text();
				ime_selection = OS::get_singleton()->get_ime_selection();
				update();
			}
		} break;
		case Control::NOTIFICATION_DRAG_BEGIN: {
			selection.selecting_mode = Selection::MODE_NONE;
			drag_action = true;
			dragging_minimap = false;
			dragging_selection = false;
			can_drag_minimap = false;
			click_select_held->stop();
		} break;
		case Control::NOTIFICATION_DRAG_END: {
			if (is_drag_successful()) {
				if (selection.drag_attempt) {
					selection.drag_attempt = false;
					if (!readonly && !Input::get_singleton()->is_key_pressed(KEY_CONTROL)) {
						_remove_text(selection.from_line, selection.from_column, selection.to_line, selection.to_column);
						cursor_set_line(selection.from_line, false);
						cursor_set_column(selection.from_column);
						selection.active = false;
						selection.selecting_mode = Selection::MODE_NONE;
						update();
					} else if (deselect_on_focus_loss_enabled) {
						deselect();
					}
				}
			} else {
				selection.drag_attempt = false;
			}
			drag_action = false;
			drag_caret_force_displayed = false;
			dragging_minimap = false;
			dragging_selection = false;
			can_drag_minimap = false;
			click_select_held->stop();
		} break;
	}
}

void TextEdit::_consume_pair_symbol(CharType ch) {
	int cursor_position_to_move = cursor_get_column() + 1;

	CharType ch_single[2] = { ch, 0 };
	CharType ch_single_pair[2] = { _get_right_pair_symbol(ch), 0 };
	CharType ch_pair[3] = { ch, _get_right_pair_symbol(ch), 0 };

	if (is_selection_active()) {
		int new_column, new_line;

		begin_complex_operation();
		_insert_text(get_selection_from_line(), get_selection_from_column(),
				ch_single,
				&new_line, &new_column);

		int to_col_offset = 0;
		if (get_selection_from_line() == get_selection_to_line()) {
			to_col_offset = 1;
		}

		_insert_text(get_selection_to_line(),
				get_selection_to_column() + to_col_offset,
				ch_single_pair,
				&new_line, &new_column);
		end_complex_operation();

		cursor_set_line(get_selection_to_line());
		cursor_set_column(get_selection_to_column() + to_col_offset);

		deselect();
		update();
		return;
	}

	if ((ch == '\'' || ch == '"') &&
			cursor_get_column() > 0 && _is_text_char(text[cursor.line][cursor_get_column() - 1]) && !_is_pair_right_symbol(text[cursor.line][cursor_get_column()])) {
		insert_text_at_cursor(ch_single);
		cursor_set_column(cursor_position_to_move);
		return;
	}

	if (cursor_get_column() < text[cursor.line].length()) {
		if (_is_text_char(text[cursor.line][cursor_get_column()])) {
			insert_text_at_cursor(ch_single);
			cursor_set_column(cursor_position_to_move);
			return;
		}
		if (_is_pair_right_symbol(ch) &&
				text[cursor.line][cursor_get_column()] == ch) {
			cursor_set_column(cursor_position_to_move);
			return;
		}
	}

	String line = text[cursor.line];

	bool in_single_quote = false;
	bool in_double_quote = false;
	bool found_comment = false;

	int c = 0;
	while (c < line.length()) {
		if (line[c] == '\\') {
			c++; // Skip quoted anything.

			if (cursor.column == c) {
				break;
			}
		} else if (!in_single_quote && !in_double_quote && line[c] == '#') {
			found_comment = true;
			break;
		} else {
			if (line[c] == '\'' && !in_double_quote) {
				in_single_quote = !in_single_quote;
			} else if (line[c] == '"' && !in_single_quote) {
				in_double_quote = !in_double_quote;
			}
		}

		c++;

		if (cursor.column == c) {
			break;
		}
	}

	// Do not need to duplicate quotes while in comments
	if (found_comment) {
		insert_text_at_cursor(ch_single);
		cursor_set_column(cursor_position_to_move);

		return;
	}

	// Disallow inserting duplicated quotes while already in string
	if ((in_single_quote || in_double_quote) && (ch == '"' || ch == '\'')) {
		insert_text_at_cursor(ch_single);
		cursor_set_column(cursor_position_to_move);

		return;
	}

	insert_text_at_cursor(ch_pair);
	cursor_set_column(cursor_position_to_move);
}

void TextEdit::_consume_backspace_for_pair_symbol(int prev_line, int prev_column) {
	bool remove_right_symbol = false;

	if (cursor.column < text[cursor.line].length() && cursor.column > 0) {
		CharType left_char = text[cursor.line][cursor.column - 1];
		CharType right_char = text[cursor.line][cursor.column];

		if (right_char == _get_right_pair_symbol(left_char)) {
			remove_right_symbol = true;
		}
	}
	if (remove_right_symbol) {
		_remove_text(prev_line, prev_column, cursor.line, cursor.column + 1);
	} else {
		_remove_text(prev_line, prev_column, cursor.line, cursor.column);
	}
}

void TextEdit::backspace_at_cursor() {
	if (readonly) {
		return;
	}

	if (cursor.column == 0 && cursor.line == 0) {
		return;
	}

	int prev_line = cursor.column ? cursor.line : cursor.line - 1;
	int prev_column = cursor.column ? (cursor.column - 1) : (text[cursor.line - 1].length());

	if (is_line_hidden(cursor.line)) {
		set_line_as_hidden(prev_line, true);
	}
	if (is_line_set_as_breakpoint(cursor.line)) {
		if (!text.is_breakpoint(prev_line)) {
			emit_signal("breakpoint_toggled", prev_line);
		}
		set_line_as_breakpoint(prev_line, true);
	}

	if (text.has_info_icon(cursor.line)) {
		set_line_info_icon(prev_line, text.get_info_icon(cursor.line), text.get_info(cursor.line));
	}

	if (auto_brace_completion_enabled &&
			cursor.column > 0 &&
			_is_pair_left_symbol(text[cursor.line][cursor.column - 1])) {
		_consume_backspace_for_pair_symbol(prev_line, prev_column);
	} else {
		// Handle space indentation.
		if (cursor.column != 0 && indent_using_spaces) {
			// Check if there are no other chars before cursor, just indentation.
			bool unindent = true;
			int i = 0;
			while (i < cursor.column && i < text[cursor.line].length()) {
				if (!_is_whitespace(text[cursor.line][i])) {
					unindent = false;
					break;
				}
				i++;
			}

			// Then we can remove all spaces as a single character.
			if (unindent) {
				// We want to remove spaces up to closest indent, or whole indent if cursor is pointing at it.
				int spaces_to_delete = _calculate_spaces_till_next_left_indent(cursor.column);
				prev_column = cursor.column - spaces_to_delete;
				_remove_text(cursor.line, prev_column, cursor.line, cursor.column);
			} else {
				_remove_text(prev_line, prev_column, cursor.line, cursor.column);
			}
		} else {
			_remove_text(prev_line, prev_column, cursor.line, cursor.column);
		}
	}

	cursor_set_line(prev_line, false, true);
	cursor_set_column(prev_column);
}

void TextEdit::indent_right() {
	int start_line;
	int end_line;

	// This value informs us by how much we changed selection position by indenting right.
	// Default is 1 for tab indentation.
	int selection_offset = 1;
	begin_complex_operation();

	if (is_selection_active()) {
		start_line = get_selection_from_line();
		end_line = get_selection_to_line();
	} else {
		start_line = cursor.line;
		end_line = start_line;
	}

	// Ignore if the cursor is not past the first column.
	if (is_selection_active() && get_selection_to_column() == 0) {
		selection_offset = 0;
		end_line--;
	}

	for (int i = start_line; i <= end_line; i++) {
		String line_text = get_line(i);
		if (line_text.size() == 0 && is_selection_active()) {
			continue;
		}
		if (indent_using_spaces) {
			// We don't really care where selection is - we just need to know indentation level at the beginning of the line.
			int left = _find_first_non_whitespace_column_of_line(line_text);
			int spaces_to_add = _calculate_spaces_till_next_right_indent(left);
			// Since we will add this much spaces we want move whole selection and cursor by this much.
			selection_offset = spaces_to_add;
			for (int j = 0; j < spaces_to_add; j++) {
				line_text = ' ' + line_text;
			}
		} else {
			line_text = '\t' + line_text;
		}
		set_line(i, line_text);
	}

	// Fix selection and cursor being off after shifting selection right.
	if (is_selection_active()) {
		select(selection.from_line, selection.from_column + selection_offset, selection.to_line, selection.to_column + selection_offset);
	}
	cursor_set_column(cursor.column + selection_offset, false);
	end_complex_operation();
	update();
}

void TextEdit::indent_left() {
	int start_line;
	int end_line;

	// Moving cursor and selection after unindenting can get tricky because
	// changing content of line can move cursor and selection on it's own (if new line ends before previous position of either),
	// therefore we just remember initial values and at the end of the operation offset them by number of removed characters.
	int removed_characters = 0;
	int initial_selection_end_column = selection.to_column;
	int initial_cursor_column = cursor.column;

	begin_complex_operation();

	if (is_selection_active()) {
		start_line = get_selection_from_line();
		end_line = get_selection_to_line();
	} else {
		start_line = cursor.line;
		end_line = start_line;
	}

	// Ignore if the cursor is not past the first column.
	if (is_selection_active() && get_selection_to_column() == 0) {
		end_line--;
	}
	String first_line_text = get_line(start_line);
	String last_line_text = get_line(end_line);

	for (int i = start_line; i <= end_line; i++) {
		String line_text = get_line(i);

		if (line_text.begins_with("\t")) {
			line_text = line_text.substr(1, line_text.length());
			set_line(i, line_text);
			removed_characters = 1;
		} else if (line_text.begins_with(" ")) {
			// When unindenting we aim to remove spaces before line that has selection no matter what is selected,
			// so we start of by finding first non whitespace character of line
			int left = _find_first_non_whitespace_column_of_line(line_text);

			// Here we remove only enough spaces to align text to nearest full multiple of indentation_size.
			// In case where selection begins at the start of indentation_size multiple we remove whole indentation level.
			int spaces_to_remove = _calculate_spaces_till_next_left_indent(left);

			line_text = line_text.substr(spaces_to_remove, line_text.length());
			set_line(i, line_text);
			removed_characters = spaces_to_remove;
		}
	}

	if (is_selection_active()) {
		// Fix selection being off by one on the first line.
		if (first_line_text != get_line(start_line)) {
			select(selection.from_line, selection.from_column - removed_characters,
					selection.to_line, initial_selection_end_column);
		}
		// Fix selection being off by one on the last line.
		if (last_line_text != get_line(end_line)) {
			select(selection.from_line, selection.from_column,
					selection.to_line, initial_selection_end_column - removed_characters);
		}
	}
	cursor_set_column(initial_cursor_column - removed_characters, false);
	end_complex_operation();
	update();
}

int TextEdit::_calculate_spaces_till_next_left_indent(int column) {
	int spaces_till_indent = column % indent_size;
	if (spaces_till_indent == 0) {
		spaces_till_indent = indent_size;
	}
	return spaces_till_indent;
}

int TextEdit::_calculate_spaces_till_next_right_indent(int column) {
	return indent_size - column % indent_size;
}

void TextEdit::_get_mouse_pos(const Point2i &p_mouse, int &r_row, int &r_col) const {
	float rows = p_mouse.y;
	rows -= cache.style_normal->get_margin(MARGIN_TOP);
	rows /= get_row_height();
	rows += get_v_scroll_offset();
	int first_vis_line = get_first_visible_line();
	int row = first_vis_line + Math::floor(rows);
	int wrap_index = 0;

	if (is_wrap_enabled() || is_hiding_enabled()) {
		int f_ofs = num_lines_from_rows(first_vis_line, cursor.wrap_ofs, rows + (1 * SGN(rows)), wrap_index) - 1;
		if (rows < 0) {
			row = first_vis_line - f_ofs;
		} else {
			row = first_vis_line + f_ofs;
		}
	}

	if (row < 0) {
		row = 0; // TODO.
	}

	int col = 0;

	if (row >= text.size()) {
		row = text.size() - 1;
		col = text[row].size();
	} else {
		int colx = p_mouse.x - (cache.style_normal->get_margin(MARGIN_LEFT) + cache.line_number_w + cache.breakpoint_gutter_width + cache.fold_gutter_width + cache.info_gutter_width);
		colx += cursor.x_ofs;
		col = get_char_pos_for_line(colx, row, wrap_index);
		if (is_wrap_enabled() && wrap_index < times_line_wraps(row)) {
			// Move back one if we are at the end of the row.
			Vector<String> rows2 = get_wrap_rows_text(row);
			int row_end_col = 0;
			for (int i = 0; i < wrap_index + 1; i++) {
				row_end_col += rows2[i].length();
			}
			if (col >= row_end_col) {
				col -= 1;
			}
		}
	}

	r_row = row;
	r_col = col;
}

Vector2i TextEdit::_get_cursor_pixel_pos() {
	adjust_viewport_to_cursor();
	int row = (cursor.line - get_first_visible_line() - cursor.wrap_ofs);
	// Correct for hidden and wrapped lines
	for (int i = get_first_visible_line(); i < cursor.line; i++) {
		if (is_line_hidden(i)) {
			row -= 1;
			continue;
		}
		row += times_line_wraps(i);
	}
	// Row might be wrapped. Adjust row and r_column
	Vector<String> rows2 = get_wrap_rows_text(cursor.line);
	while (rows2.size() > 1) {
		if (cursor.column >= rows2[0].length()) {
			cursor.column -= rows2[0].length();
			rows2.remove(0);
			row++;
		} else {
			break;
		}
	}

	// Calculate final pixel position
	int y = (row - get_v_scroll_offset() + 1 /*Bottom of line*/) * get_row_height();
	int x = cache.style_normal->get_margin(MARGIN_LEFT) + cache.line_number_w + cache.breakpoint_gutter_width + cache.fold_gutter_width + cache.info_gutter_width - cursor.x_ofs;
	int ix = 0;
	while (ix < rows2[0].size() && ix < cursor.column) {
		if (cache.font != nullptr) {
			x += cache.font->get_char_size(rows2[0].get(ix)).width;
		}
		ix++;
	}
	x += get_indent_level(cursor.line) * cache.font->get_char_size(' ').width;

	return Vector2i(x, y);
}

void TextEdit::_get_minimap_mouse_row(const Point2i &p_mouse, int &r_row) const {
	float rows = p_mouse.y;
	rows -= cache.style_normal->get_margin(MARGIN_TOP);
	rows /= (minimap_char_size.y + minimap_line_spacing);
	rows += get_v_scroll_offset();

	// calculate visible lines
	int minimap_visible_lines = _get_minimap_visible_rows();
	int visible_rows = get_visible_rows() + 1;
	int first_visible_line = get_first_visible_line() - 1;
	int draw_amount = visible_rows + (smooth_scroll_enabled ? 1 : 0);
	draw_amount += times_line_wraps(first_visible_line + 1);
	int minimap_line_height = (minimap_char_size.y + minimap_line_spacing);

	// calculate viewport size and y offset
	int viewport_height = (draw_amount - 1) * minimap_line_height;
	int control_height = _get_control_height() - viewport_height;
	int viewport_offset_y = round(get_scroll_pos_for_line(first_visible_line) * control_height) / ((v_scroll->get_max() <= minimap_visible_lines) ? (minimap_visible_lines - draw_amount) : (v_scroll->get_max() - draw_amount));

	// calculate the first line.
	int num_lines_before = round((viewport_offset_y) / minimap_line_height);
	int wi;
	int minimap_line = (v_scroll->get_max() <= minimap_visible_lines) ? -1 : first_visible_line;
	if (first_visible_line > 0 && minimap_line >= 0) {
		minimap_line -= num_lines_from_rows(first_visible_line, 0, -num_lines_before, wi);
		minimap_line -= (minimap_line > 0 && smooth_scroll_enabled ? 1 : 0);
	} else {
		minimap_line = 0;
	}

	int row = minimap_line + Math::floor(rows);
	int wrap_index = 0;

	if (is_wrap_enabled() || is_hiding_enabled()) {
		int f_ofs = num_lines_from_rows(minimap_line, cursor.wrap_ofs, rows + (1 * SGN(rows)), wrap_index) - 1;
		if (rows < 0) {
			row = minimap_line - f_ofs;
		} else {
			row = minimap_line + f_ofs;
		}
	}

	if (row < 0) {
		row = 0;
	}

	if (row >= text.size()) {
		row = text.size() - 1;
	}

	r_row = row;
}

void TextEdit::_gui_input(const Ref<InputEvent> &p_gui_input) {
	ERR_FAIL_COND(p_gui_input.is_null());

	double prev_v_scroll = v_scroll->get_value();
	double prev_h_scroll = h_scroll->get_value();

	Ref<InputEventMouseButton> mb = p_gui_input;

	if (mb.is_valid()) {
		if (completion_active && completion_rect.has_point(mb->get_position())) {
			if (!mb->is_pressed()) {
				return;
			}

			if (mb->get_button_index() == BUTTON_WHEEL_UP) {
				if (completion_index > 0) {
					completion_index--;
					completion_current = completion_options[completion_index];
					completion_force_item_center = -1;
					update();
				}
			}
			if (mb->get_button_index() == BUTTON_WHEEL_DOWN) {
				if (completion_index < completion_options.size() - 1) {
					completion_index++;
					completion_current = completion_options[completion_index];
					completion_force_item_center = -1;
					update();
				}
			}

			if (mb->get_button_index() == BUTTON_LEFT) {
				if (completion_force_item_center == -1) {
					completion_force_item_center = completion_index;
				}

				completion_index = CLAMP(completion_line_ofs + (mb->get_position().y - completion_rect.position.y) / get_row_height(), 0, completion_options.size() - 1);
				completion_current = completion_options[completion_index];
				update();
				if (mb->is_doubleclick()) {
					_confirm_completion();
				}
			}
			return;
		} else {
			_cancel_completion();
			_cancel_code_hint();
		}

		if (mb->is_pressed()) {
			if (mb->get_button_index() == BUTTON_WHEEL_UP && !mb->get_command()) {
				if (mb->get_shift()) {
					h_scroll->set_value(h_scroll->get_value() - (100 * mb->get_factor()));
				} else if (mb->get_alt()) {
					// Scroll 5 times as fast as normal (like in Visual Studio Code).
					_scroll_up(15 * mb->get_factor());
				} else if (v_scroll->is_visible()) {
					// Scroll 3 lines.
					_scroll_up(3 * mb->get_factor());
				}
			}
			if (mb->get_button_index() == BUTTON_WHEEL_DOWN && !mb->get_command()) {
				if (mb->get_shift()) {
					h_scroll->set_value(h_scroll->get_value() + (100 * mb->get_factor()));
				} else if (mb->get_alt()) {
					// Scroll 5 times as fast as normal (like in Visual Studio Code).
					_scroll_down(15 * mb->get_factor());
				} else if (v_scroll->is_visible()) {
					// Scroll 3 lines.
					_scroll_down(3 * mb->get_factor());
				}
			}
			if (mb->get_button_index() == BUTTON_WHEEL_LEFT) {
				h_scroll->set_value(h_scroll->get_value() - (100 * mb->get_factor()));
			}
			if (mb->get_button_index() == BUTTON_WHEEL_RIGHT) {
				h_scroll->set_value(h_scroll->get_value() + (100 * mb->get_factor()));
			}
			if (mb->get_button_index() == BUTTON_LEFT) {
				_reset_caret_blink_timer();

				int row, col;
				_get_mouse_pos(Point2i(mb->get_position().x, mb->get_position().y), row, col);

				// Toggle breakpoint on gutter click.
				if (draw_breakpoint_gutter) {
					int gutter = cache.style_normal->get_margin(MARGIN_LEFT);
					if (mb->get_position().x > gutter - 6 && mb->get_position().x <= gutter + cache.breakpoint_gutter_width - 3) {
						set_line_as_breakpoint(row, !is_line_set_as_breakpoint(row));
						emit_signal("breakpoint_toggled", row);
						return;
					}
				}

				// Emit info clicked.
				if (draw_info_gutter && text.has_info_icon(row)) {
					int left_margin = cache.style_normal->get_margin(MARGIN_LEFT);
					int gutter_left = left_margin + cache.breakpoint_gutter_width;
					if (mb->get_position().x > gutter_left - 6 && mb->get_position().x <= gutter_left + cache.info_gutter_width - 3) {
						emit_signal("info_clicked", row, text.get_info(row));
						return;
					}
				}

				// Toggle fold on gutter click if can.
				if (draw_fold_gutter) {
					int left_margin = cache.style_normal->get_margin(MARGIN_LEFT);
					int gutter_left = left_margin + cache.breakpoint_gutter_width + cache.line_number_w + cache.info_gutter_width;
					if (mb->get_position().x > gutter_left - 6 && mb->get_position().x <= gutter_left + cache.fold_gutter_width - 3) {
						if (is_folded(row)) {
							unfold_line(row);
						} else if (can_fold(row)) {
							fold_line(row);
						}
						return;
					}
				}

				// Unfold on folded icon click.
				if (is_folded(row)) {
					int line_width = text.get_line_width(row);
					line_width += cache.style_normal->get_margin(MARGIN_LEFT) + cache.line_number_w + cache.breakpoint_gutter_width + cache.info_gutter_width + cache.fold_gutter_width - cursor.x_ofs;
					if (mb->get_position().x > line_width - 3 && mb->get_position().x <= line_width + cache.folded_eol_icon->get_width() + 3) {
						unfold_line(row);
						return;
					}
				}

				// minimap
				if (draw_minimap) {
					_update_minimap_click();
					if (dragging_minimap) {
						return;
					}
				}

				int prev_col = cursor.column;
				int prev_line = cursor.line;

				cursor_set_line(row, false, false);
				cursor_set_column(col);
				selection.drag_attempt = false;

				if (mb->get_shift() && (cursor.column != prev_col || cursor.line != prev_line)) {
					if (!selection.active) {
						selection.active = true;
						selection.selecting_mode = Selection::MODE_POINTER;
						selection.from_column = prev_col;
						selection.from_line = prev_line;
						selection.to_column = cursor.column;
						selection.to_line = cursor.line;

						if (selection.from_line > selection.to_line || (selection.from_line == selection.to_line && selection.from_column > selection.to_column)) {
							SWAP(selection.from_column, selection.to_column);
							SWAP(selection.from_line, selection.to_line);
							selection.shiftclick_left = false;
						} else {
							selection.shiftclick_left = true;
						}
						selection.selecting_line = prev_line;
						selection.selecting_column = prev_col;
						update();
					} else {
						if (cursor.line < selection.selecting_line || (cursor.line == selection.selecting_line && cursor.column < selection.selecting_column)) {
							if (selection.shiftclick_left) {
								SWAP(selection.from_column, selection.to_column);
								SWAP(selection.from_line, selection.to_line);
								selection.shiftclick_left = !selection.shiftclick_left;
							}
							selection.from_column = cursor.column;
							selection.from_line = cursor.line;

						} else if (cursor.line > selection.selecting_line || (cursor.line == selection.selecting_line && cursor.column > selection.selecting_column)) {
							if (!selection.shiftclick_left) {
								SWAP(selection.from_column, selection.to_column);
								SWAP(selection.from_line, selection.to_line);
								selection.shiftclick_left = !selection.shiftclick_left;
							}
							selection.to_column = cursor.column;
							selection.to_line = cursor.line;

						} else {
							selection.active = false;
						}

						update();
					}
				} else if (drag_and_drop_selection_enabled && is_mouse_over_selection()) {
					selection.selecting_mode = Selection::MODE_NONE;
					selection.drag_attempt = true;
				} else {
					selection.active = false;
					selection.selecting_mode = Selection::MODE_POINTER;
					selection.selecting_line = row;
					selection.selecting_column = col;
				}

				const int triple_click_timeout = 600;
				const int triple_click_tolerance = 5;

				if (!mb->is_doubleclick() && (OS::get_singleton()->get_ticks_msec() - last_dblclk) < triple_click_timeout && mb->get_position().distance_to(last_dblclk_pos) < triple_click_tolerance) {
					// Triple-click select line.
					selection.selecting_mode = Selection::MODE_LINE;
					_update_selection_mode_line();
					last_dblclk = 0;
				} else if (mb->is_doubleclick() && text[cursor.line].length()) {
					// Double-click select word.
					selection.selecting_mode = Selection::MODE_WORD;
					_update_selection_mode_word();
					last_dblclk = OS::get_singleton()->get_ticks_msec();
					last_dblclk_pos = mb->get_position();
				}

				update();
			}

			if (is_middle_mouse_paste_enabled() && mb->get_button_index() == BUTTON_MIDDLE && !readonly && OS::get_singleton()->has_feature("primary_clipboard")) {
				String paste_buffer = OS::get_singleton()->get_clipboard_primary();

				int row, col;
				_get_mouse_pos(Point2i(mb->get_position().x, mb->get_position().y), row, col);
				begin_complex_operation();

				deselect();
				cursor_set_line(row, true, false);
				cursor_set_column(col);
				if (!paste_buffer.empty()) {
					_insert_text_at_cursor(paste_buffer);
				}
				end_complex_operation();

				grab_focus();
				update();
			}

			if (mb->get_button_index() == BUTTON_RIGHT && context_menu_enabled) {
				_reset_caret_blink_timer();

				int row, col;
				_get_mouse_pos(Point2i(mb->get_position().x, mb->get_position().y), row, col);

				if (is_right_click_moving_caret()) {
					if (is_selection_active()) {
						int from_line = get_selection_from_line();
						int to_line = get_selection_to_line();
						int from_column = get_selection_from_column();
						int to_column = get_selection_to_column();

						if (row < from_line || row > to_line || (row == from_line && col < from_column) || (row == to_line && col > to_column)) {
							// Right click is outside the selected text.
							deselect();
						}
					}
					if (!is_selection_active()) {
						cursor_set_line(row, true, false);
						cursor_set_column(col);
					}
				}

				popup_show = true;
				if (!readonly) {
					menu->set_item_disabled(menu->get_item_index(MENU_UNDO), !has_undo());
					menu->set_item_disabled(menu->get_item_index(MENU_REDO), !has_redo());
				}

				menu->set_position(get_global_transform().xform(get_local_mouse_position()));
				menu->set_size(Vector2(1, 1));
				menu->set_scale(get_global_transform().get_scale());
				menu->popup();
			}
		} else {
			if (mb->get_button_index() == BUTTON_LEFT) {
				if (selection.drag_attempt && selection.selecting_mode == Selection::MODE_NONE && is_mouse_over_selection()) {
					selection.active = false;
				}
				if (mb->get_command() && highlighted_word != String()) {
					int row, col;
					_get_mouse_pos(Point2i(mb->get_position().x, mb->get_position().y), row, col);

					emit_signal("symbol_lookup", highlighted_word, row, col);
					return;
				}

				dragging_minimap = false;
				dragging_selection = false;
				can_drag_minimap = false;
				click_select_held->stop();
				if (!drag_action) {
					selection.drag_attempt = false;
				}
				if (OS::get_singleton()->has_feature("primary_clipboard")) {
					OS::get_singleton()->set_clipboard_primary(get_selection_text());
				}
			}

			// Notify to show soft keyboard.
			notification(NOTIFICATION_FOCUS_ENTER);
		}
	}

	const Ref<InputEventPanGesture> pan_gesture = p_gui_input;
	if (pan_gesture.is_valid()) {
		const real_t delta = pan_gesture->get_delta().y;
		if (delta < 0) {
			_scroll_up(-delta);
		} else {
			_scroll_down(delta);
		}
		h_scroll->set_value(h_scroll->get_value() + pan_gesture->get_delta().x * 100);
		if (v_scroll->get_value() != prev_v_scroll || h_scroll->get_value() != prev_h_scroll) {
			accept_event(); // Accept event if scroll changed.
		}

		return;
	}

	Ref<InputEventMouseMotion> mm = p_gui_input;

	if (mm.is_valid()) {
		if (select_identifiers_enabled) {
			if (!dragging_minimap && !dragging_selection && mm->get_command() && mm->get_button_mask() == 0) {
				String new_word = get_word_at_pos(mm->get_position());
				if (new_word != highlighted_word) {
					highlighted_word = new_word;
					update();
				}
			} else {
				if (highlighted_word != String()) {
					highlighted_word = String();
					update();
				}
			}
		}

		if (draw_minimap && !dragging_selection) {
			_update_minimap_hover();
		}

		if (mm->get_button_mask() & BUTTON_MASK_LEFT && get_viewport()->gui_get_drag_data() == Variant()) { // Ignore if dragging.
			_reset_caret_blink_timer();

			if (draw_minimap && !dragging_selection) {
				_update_minimap_drag();
			}

			if (!dragging_minimap) {
				switch (selection.selecting_mode) {
					case Selection::MODE_POINTER: {
						_update_selection_mode_pointer();
					} break;
					case Selection::MODE_WORD: {
						_update_selection_mode_word();
					} break;
					case Selection::MODE_LINE: {
						_update_selection_mode_line();
					} break;
					default: {
						break;
					}
				}
			}
		}

		if (drag_action && can_drop_data(mm->get_position(), get_viewport()->gui_get_drag_data())) {
			drag_caret_force_displayed = true;
			Point2 mp = get_local_mouse_position();
			int row, col;
			_get_mouse_pos(Point2i(mp.x, mp.y), row, col);
			cursor_set_line(row, true);
			cursor_set_column(col);
			if (!scroll_past_end_of_file_enabled) {
				if (row <= get_first_visible_line()) {
					_scroll_lines_up();
				} else if (row >= get_last_full_visible_line()) {
					_scroll_lines_down();
				}
			}
			dragging_selection = true;
			update();
		}
	}

	if (v_scroll->get_value() != prev_v_scroll || h_scroll->get_value() != prev_h_scroll) {
		accept_event(); // Accept event if scroll changed.
	}

	Ref<InputEventKey> k = p_gui_input;

	if (k.is_valid()) {
		k = k->duplicate(); // It will be modified later on.

#ifdef OSX_ENABLED
		if (k->get_scancode() == KEY_META) {
#else
		if (k->get_scancode() == KEY_CONTROL) {

#endif
			if (select_identifiers_enabled) {
				if (k->is_pressed() && !dragging_minimap && !dragging_selection) {
					highlighted_word = get_word_at_pos(get_local_mouse_position());
					update();

				} else {
					highlighted_word = String();
					update();
				}
			}
		}

		if (!k->is_pressed()) {
			return;
		}

		if (completion_active) {
			if (readonly) {
				return;
			}

			bool valid = true;
			if (k->get_command() || k->get_metakey()) {
				valid = false;
			}

			if (valid) {
				if (!k->get_alt()) {
					if (k->get_scancode() == KEY_UP) {
						if (completion_index > 0) {
							completion_index--;
						} else {
							completion_index = completion_options.size() - 1;
						}
						completion_current = completion_options[completion_index];
						completion_force_item_center = -1;
						update();

						accept_event();
						return;
					}

					if (k->get_scancode() == KEY_DOWN) {
						if (completion_index < completion_options.size() - 1) {
							completion_index++;
						} else {
							completion_index = 0;
						}
						completion_current = completion_options[completion_index];
						completion_force_item_center = -1;
						update();

						accept_event();
						return;
					}

					if (k->get_scancode() == KEY_PAGEUP) {
						completion_index -= get_constant("completion_lines");
						if (completion_index < 0) {
							completion_index = 0;
						}
						completion_current = completion_options[completion_index];
						completion_force_item_center = -1;
						update();
						accept_event();
						return;
					}

					if (k->get_scancode() == KEY_PAGEDOWN) {
						completion_index += get_constant("completion_lines");
						if (completion_index >= completion_options.size()) {
							completion_index = completion_options.size() - 1;
						}
						completion_current = completion_options[completion_index];
						completion_force_item_center = -1;
						update();
						accept_event();
						return;
					}

					if (k->get_scancode() == KEY_HOME && completion_index > 0) {
						completion_index = 0;
						completion_current = completion_options[completion_index];
						completion_force_item_center = -1;
						update();
						accept_event();
						return;
					}

					if (k->get_scancode() == KEY_END && completion_index < completion_options.size() - 1) {
						completion_index = completion_options.size() - 1;
						completion_current = completion_options[completion_index];
						completion_force_item_center = -1;
						update();
						accept_event();
						return;
					}

					if (k->get_scancode() == KEY_KP_ENTER || k->get_scancode() == KEY_ENTER || k->get_scancode() == KEY_TAB) {
						_confirm_completion();
						accept_event();
						return;
					}

					if (k->get_scancode() == KEY_BACKSPACE) {
						_reset_caret_blink_timer();

						backspace_at_cursor();
						_update_completion_candidates();
						accept_event();
						return;
					}

					if (k->get_scancode() == KEY_SHIFT) {
						accept_event();
						return;
					}
				}

				if (k->get_unicode() > 32) {
					_reset_caret_blink_timer();

					const CharType chr[2] = { (CharType)k->get_unicode(), 0 };
					if (auto_brace_completion_enabled && _is_pair_symbol(chr[0])) {
						_consume_pair_symbol(chr[0]);
					} else {
						// Remove the old character if in insert mode.
						if (insert_mode) {
							begin_complex_operation();

							// Make sure we don't try and remove empty space.
							if (cursor.column < get_line(cursor.line).length()) {
								_remove_text(cursor.line, cursor.column, cursor.line, cursor.column + 1);
							}
						}

						_insert_text_at_cursor(chr);

						if (insert_mode) {
							end_complex_operation();
						}
					}
					_update_completion_candidates();
					accept_event();

					return;
				}
			}

			_cancel_completion();
		}

		/* TEST CONTROL FIRST! */

		// Some remaps for duplicate functions.
		if (k->get_command() && !k->get_shift() && !k->get_alt() && !k->get_metakey() && k->get_scancode() == KEY_INSERT) {
			k->set_scancode(KEY_C);
		}
		if (!k->get_command() && k->get_shift() && !k->get_alt() && !k->get_metakey() && k->get_scancode() == KEY_INSERT) {
			k->set_scancode(KEY_V);
			k->set_command(true);
			k->set_shift(false);
		}
#ifdef APPLE_STYLE_KEYS
		if (k->get_control() && !k->get_shift() && !k->get_alt() && !k->get_command()) {
			uint32_t remap_key = KEY_UNKNOWN;
			switch (k->get_scancode()) {
				case KEY_F: {
					remap_key = KEY_RIGHT;
				} break;
				case KEY_B: {
					remap_key = KEY_LEFT;
				} break;
				case KEY_P: {
					remap_key = KEY_UP;
				} break;
				case KEY_N: {
					remap_key = KEY_DOWN;
				} break;
				case KEY_D: {
					remap_key = KEY_DELETE;
				} break;
				case KEY_H: {
					remap_key = KEY_BACKSPACE;
				} break;
			}

			if (remap_key != KEY_UNKNOWN) {
				k->set_scancode(remap_key);
				k->set_control(false);
			}
		}
#endif

		_reset_caret_blink_timer();

		// Save here for insert mode as well as arrow navigation, just in case it is cleared in the following section.
		bool had_selection = selection.active;

		// Stuff to do when selection is active.
		if (!readonly && selection.active) {
			bool clear = false;
			bool unselect = false;
			bool dobreak = false;

			switch (k->get_scancode()) {
				case KEY_TAB: {
					if (k->get_shift()) {
						indent_left();
					} else {
						indent_right();
					}
					dobreak = true;
					accept_event();
				} break;
				case KEY_X:
				case KEY_C:
					// Special keys often used with control, wait.
					clear = (!k->get_command() || k->get_shift() || k->get_alt());
					break;
				case KEY_DELETE:
					if (!k->get_shift()) {
						accept_event();
						clear = true;
						dobreak = true;
					} else if (k->get_command() || k->get_alt()) {
						dobreak = true;
					}
					break;
				case KEY_BACKSPACE:
					accept_event();
					clear = true;
					dobreak = true;
					break;
				case KEY_LEFT:
				case KEY_RIGHT:
				case KEY_UP:
				case KEY_DOWN:
				case KEY_PAGEUP:
				case KEY_PAGEDOWN:
				case KEY_HOME:
				case KEY_END:
					// Ignore arrows if any modifiers are held (shift = selecting, others may be used for editor hotkeys).
					if (k->get_command() || k->get_shift() || k->get_alt()) {
						break;
					}
					unselect = true;
					break;

				default:
					if (k->get_unicode() >= 32 && !k->get_command() && !k->get_alt() && !k->get_metakey()) {
						clear = true;
					}
					if (auto_brace_completion_enabled && _is_pair_left_symbol(k->get_unicode())) {
						clear = false;
					}
			}

			if (unselect) {
				selection.active = false;
				selection.selecting_mode = Selection::MODE_NONE;
				update();
			}
			if (clear) {
				if (!dobreak) {
					begin_complex_operation();
				}
				selection.active = false;
				update();
				_remove_text(selection.from_line, selection.from_column, selection.to_line, selection.to_column);
				cursor_set_line(selection.from_line, false, false);
				cursor_set_column(selection.from_column);
				update();
			}
			if (dobreak) {
				return;
			}
		}

		selection.selecting_text = false;

		bool scancode_handled = true;

		// Special scancode test.

		switch (k->get_scancode()) {
			case KEY_KP_ENTER:
			case KEY_ENTER: {
				if (readonly) {
					break;
				}

				String ins = "\n";

				// Keep indentation.
				int space_count = 0;
				for (int i = 0; i < cursor.column; i++) {
					if (text[cursor.line][i] == '\t') {
						if (indent_using_spaces) {
							ins += space_indent;
						} else {
							ins += "\t";
						}
						space_count = 0;
					} else if (text[cursor.line][i] == ' ') {
						space_count++;

						if (space_count == indent_size) {
							if (indent_using_spaces) {
								ins += space_indent;
							} else {
								ins += "\t";
							}
							space_count = 0;
						}
					} else {
						break;
					}
				}

				if (is_folded(cursor.line)) {
					unfold_line(cursor.line);
				}

				bool brace_indent = false;

				// No need to indent if we are going upwards.
				if (auto_indent && !(k->get_command() && k->get_shift())) {
					// Indent once again if previous line will end with ':','{','[','(' and the line is not a comment
					// (i.e. colon/brace precedes current cursor position).
					if (cursor.column > 0) {
						const Map<int, Text::ColorRegionInfo> &cri_map = text.get_color_region_info(cursor.line);
						bool indent_char_found = false;
						bool should_indent = false;
						char indent_char = ':';
						char c = text[cursor.line][cursor.column];

						for (int i = 0; i < cursor.column; i++) {
							c = text[cursor.line][i];
							switch (c) {
								case ':':
								case '{':
								case '[':
								case '(':
									indent_char_found = true;
									should_indent = true;
									indent_char = c;
									continue;
							}

							if (indent_char_found && cri_map.has(i) && (color_regions[cri_map[i].region].begin_key == "#" || color_regions[cri_map[i].region].begin_key == "//")) {
								should_indent = true;
								break;
							} else if (indent_char_found && !_is_whitespace(c)) {
								should_indent = false;
								indent_char_found = false;
							}
						}

						if (!is_line_comment(cursor.line) && should_indent) {
							if (indent_using_spaces) {
								ins += space_indent;
							} else {
								ins += "\t";
							}

							// No need to move the brace below if we are not taking the text with us.
							char closing_char = _get_right_pair_symbol(indent_char);
							if ((closing_char != 0) && (closing_char == text[cursor.line][cursor.column]) && !k->get_command()) {
								brace_indent = true;
								ins += "\n" + ins.substr(1, ins.length() - 2);
							}
						}
					}
				}
				begin_complex_operation();
				bool first_line = false;
				if (k->get_command()) {
					if (k->get_shift()) {
						if (cursor.line > 0) {
							cursor_set_line(cursor.line - 1);
							cursor_set_column(text[cursor.line].length());
						} else {
							cursor_set_column(0);
							first_line = true;
						}
					} else {
						cursor_set_column(text[cursor.line].length());
					}
				}

				insert_text_at_cursor(ins);

				if (first_line) {
					cursor_set_line(0);
				} else if (brace_indent) {
					cursor_set_line(cursor.line - 1);
					cursor_set_column(text[cursor.line].length());
				}
				end_complex_operation();
			} break;
			case KEY_ESCAPE: {
				if (completion_hint != "") {
					completion_hint = "";
					update();
				} else {
					scancode_handled = false;
				}
			} break;
			case KEY_TAB: {
				if (k->get_command()) {
					break; // Avoid tab when command.
				}

				if (readonly) {
					break;
				}

				if (is_selection_active()) {
					if (k->get_shift()) {
						indent_left();
					} else {
						indent_right();
					}
				} else {
					if (k->get_shift()) {
						// Simple unindent.
						int cc = cursor.column;
						const String &line = text[cursor.line];

						int left = _find_first_non_whitespace_column_of_line(line);
						cc = MIN(cc, left);

						while (cc < indent_size && cc < left && line[cc] == ' ') {
							cc++;
						}

						if (cc > 0 && cc <= text[cursor.line].length()) {
							if (text[cursor.line][cc - 1] == '\t') {
								// Tabs unindentation.
								_remove_text(cursor.line, cc - 1, cursor.line, cc);
								if (cursor.column >= left) {
									cursor_set_column(MAX(0, cursor.column - 1));
								}
								update();
							} else {
								// Spaces unindentation.
								int spaces_to_remove = _calculate_spaces_till_next_left_indent(cc);
								if (spaces_to_remove > 0) {
									_remove_text(cursor.line, cc - spaces_to_remove, cursor.line, cc);
									if (cursor.column > left - spaces_to_remove) { // Inside text?
										cursor_set_column(MAX(0, cursor.column - spaces_to_remove));
									}
									update();
								}
							}
						} else if (cc == 0 && line.length() > 0 && line[0] == '\t') {
							_remove_text(cursor.line, 0, cursor.line, 1);
							update();
						}
					} else {
						// Simple indent.
						if (indent_using_spaces) {
							// Insert only as much spaces as needed till next indentation level.
							int spaces_to_add = _calculate_spaces_till_next_right_indent(cursor.column);
							String indent_to_insert = String();
							for (int i = 0; i < spaces_to_add; i++) {
								indent_to_insert = ' ' + indent_to_insert;
							}
							_insert_text_at_cursor(indent_to_insert);
						} else {
							_insert_text_at_cursor("\t");
						}
					}
				}

			} break;
			case KEY_BACKSPACE: {
				if (readonly) {
					break;
				}

#ifdef APPLE_STYLE_KEYS
				if (k->get_alt() && cursor.column > 1) {
#else
				if (k->get_alt()) {
					scancode_handled = false;
					break;
				} else if (k->get_command() && cursor.column > 1) {
#endif
					int line = cursor.line;
					int column = cursor.column;

					// Check if we are removing a single whitespace, if so remove it and the next char type,
					// else we just remove the whitespace.
					bool only_whitespace = false;
					if (_is_whitespace(text[line][column - 1]) && _is_whitespace(text[line][column - 2])) {
						only_whitespace = true;
					} else if (_is_whitespace(text[line][column - 1])) {
						// Remove the single whitespace.
						column--;
					}

					// Check if its a text char.
					bool only_char = (_is_text_char(text[line][column - 1]) && !only_whitespace);

					// If its not whitespace or char then symbol.
					bool only_symbols = !(only_whitespace || only_char);

					while (column > 0) {
						bool is_whitespace = _is_whitespace(text[line][column - 1]);
						bool is_text_char = _is_text_char(text[line][column - 1]);

						if (only_whitespace && !is_whitespace) {
							break;
						} else if (only_char && !is_text_char) {
							break;
						} else if (only_symbols && (is_whitespace || is_text_char)) {
							break;
						}
						column--;
					}

					_remove_text(line, column, cursor.line, cursor.column);

					cursor_set_line(line);
					cursor_set_column(column);

#ifdef APPLE_STYLE_KEYS
				} else if (k->get_command()) {
					int cursor_current_column = cursor.column;
					cursor.column = 0;
					_remove_text(cursor.line, 0, cursor.line, cursor_current_column);
#endif
				} else {
					if (cursor.line > 0 && is_line_hidden(cursor.line - 1)) {
						unfold_line(cursor.line - 1);
					}
					backspace_at_cursor();
				}

			} break;
			case KEY_KP_4: {
				if (k->get_unicode() != 0) {
					scancode_handled = false;
					break;
				}
				FALLTHROUGH;
			}
			case KEY_LEFT: {
				if (k->get_shift()) {
					_pre_shift_selection();
				} else if (had_selection && !k->get_command() && !k->get_alt()) {
					cursor_set_line(selection.from_line);
					cursor_set_column(selection.from_column);
					deselect();
					break;
#ifdef APPLE_STYLE_KEYS
				} else {
#else
				} else if (!k->get_alt()) {
#endif
					deselect();
				}

#ifdef APPLE_STYLE_KEYS
				if (k->get_command()) {
					// Start at first column (it's slightly faster that way) and look for the first non-whitespace character.
					int new_cursor_pos = 0;
					for (int i = 0; i < text[cursor.line].length(); ++i) {
						if (!_is_whitespace(text[cursor.line][i])) {
							new_cursor_pos = i;
							break;
						}
					}
					if (new_cursor_pos == cursor.column) {
						// We're already at the first text character, so move to the very beginning of the line.
						cursor_set_column(0);
					} else {
						// We're somewhere to the right of the first text character; move to the first one.
						cursor_set_column(new_cursor_pos);
					}
				} else if (k->get_alt()) {
#else
				if (k->get_alt()) {
					scancode_handled = false;
					break;
				} else if (k->get_command()) {
#endif
					int cc = cursor.column;

					if (cc == 0 && cursor.line > 0) {
						cursor_set_line(cursor.line - 1);
						cursor_set_column(text[cursor.line].length());
					} else {
						bool prev_char = false;

						while (cc > 0) {
							bool ischar = _is_text_char(text[cursor.line][cc - 1]);

							if (prev_char && !ischar) {
								break;
							}

							prev_char = ischar;
							cc--;
						}
						cursor_set_column(cc);
					}

				} else if (cursor.column == 0) {
					if (cursor.line > 0) {
						cursor_set_line(cursor.line - num_lines_from(CLAMP(cursor.line - 1, 0, text.size() - 1), -1));
						cursor_set_column(text[cursor.line].length());
					}
				} else {
					cursor_set_column(cursor_get_column() - 1);
				}

				if (k->get_shift()) {
					_post_shift_selection();
				}

			} break;
			case KEY_KP_6: {
				if (k->get_unicode() != 0) {
					scancode_handled = false;
					break;
				}
				FALLTHROUGH;
			}
			case KEY_RIGHT: {
				if (k->get_shift()) {
					_pre_shift_selection();
				} else if (had_selection && !k->get_command() && !k->get_alt()) {
					cursor_set_line(selection.to_line);
					cursor_set_column(selection.to_column);
					deselect();
					break;
#ifdef APPLE_STYLE_KEYS
				} else {
#else
				} else if (!k->get_alt()) {
#endif
					deselect();
				}

#ifdef APPLE_STYLE_KEYS
				if (k->get_command()) {
					cursor_set_column(text[cursor.line].length());
				} else if (k->get_alt()) {
#else
				if (k->get_alt()) {
					scancode_handled = false;
					break;
				} else if (k->get_command()) {
#endif
					int cc = cursor.column;

					if (cc == text[cursor.line].length() && cursor.line < text.size() - 1) {
						cursor_set_line(cursor.line + 1);
						cursor_set_column(0);
					} else {
						bool prev_char = false;

						while (cc < text[cursor.line].length()) {
							bool ischar = _is_text_char(text[cursor.line][cc]);

							if (prev_char && !ischar) {
								break;
							}
							prev_char = ischar;
							cc++;
						}
						cursor_set_column(cc);
					}

				} else if (cursor.column == text[cursor.line].length()) {
					if (cursor.line < text.size() - 1) {
						cursor_set_line(cursor_get_line() + num_lines_from(CLAMP(cursor.line + 1, 0, text.size() - 1), 1), true, false);
						cursor_set_column(0);
					}
				} else {
					cursor_set_column(cursor_get_column() + 1);
				}

				if (k->get_shift()) {
					_post_shift_selection();
				}

			} break;
			case KEY_KP_8: {
				if (k->get_unicode() != 0) {
					scancode_handled = false;
					break;
				}
				FALLTHROUGH;
			}
			case KEY_UP: {
				if (k->get_alt()) {
					scancode_handled = false;
					break;
				}
#ifndef APPLE_STYLE_KEYS
				if (k->get_command()) {
#else
				if (k->get_command() && k->get_alt()) {
#endif
					_scroll_lines_up();
					break;
				}

				if (k->get_shift()) {
					_pre_shift_selection();
				}

#ifdef APPLE_STYLE_KEYS
				if (k->get_command()) {
					cursor_set_line(0);
				} else
#endif
				{
					int cur_wrap_index = get_cursor_wrap_index();
					if (cur_wrap_index > 0) {
						cursor_set_line(cursor.line, true, false, cur_wrap_index - 1);
					} else if (cursor.line == 0) {
						cursor_set_column(0);
					} else {
						int new_line = cursor.line - num_lines_from(cursor.line - 1, -1);
						if (line_wraps(new_line)) {
							cursor_set_line(new_line, true, false, times_line_wraps(new_line));
						} else {
							cursor_set_line(new_line, true, false);
						}
					}
				}

				if (k->get_shift()) {
					_post_shift_selection();
				}
				_cancel_code_hint();

			} break;
			case KEY_KP_2: {
				if (k->get_unicode() != 0) {
					scancode_handled = false;
					break;
				}
				FALLTHROUGH;
			}
			case KEY_DOWN: {
				if (k->get_alt()) {
					scancode_handled = false;
					break;
				}
#ifndef APPLE_STYLE_KEYS
				if (k->get_command()) {
#else
				if (k->get_command() && k->get_alt()) {
#endif
					_scroll_lines_down();
					break;
				}

				if (k->get_shift()) {
					_pre_shift_selection();
				}

#ifdef APPLE_STYLE_KEYS
				if (k->get_command()) {
					cursor_set_line(get_last_unhidden_line(), true, false, 9999);
				} else
#endif
				{
					int cur_wrap_index = get_cursor_wrap_index();
					if (cur_wrap_index < times_line_wraps(cursor.line)) {
						cursor_set_line(cursor.line, true, false, cur_wrap_index + 1);
					} else if (cursor.line == get_last_unhidden_line()) {
						cursor_set_column(text[cursor.line].length());
					} else {
						int new_line = cursor.line + num_lines_from(CLAMP(cursor.line + 1, 0, text.size() - 1), 1);
						cursor_set_line(new_line, true, false, 0);
					}
				}

				if (k->get_shift()) {
					_post_shift_selection();
				}
				_cancel_code_hint();

			} break;
			case KEY_DELETE: {
				if (readonly) {
					break;
				}

				if (k->get_shift() && !k->get_command() && !k->get_alt() && is_shortcut_keys_enabled()) {
					cut();
					break;
				}

				int curline_len = text[cursor.line].length();

				if (cursor.line == text.size() - 1 && cursor.column == curline_len) {
					break; // Nothing to do.
				}

				int next_line = cursor.column < curline_len ? cursor.line : cursor.line + 1;
				int next_column;

#ifdef APPLE_STYLE_KEYS
				if (k->get_alt() && cursor.column < curline_len - 1) {
#else
				if (k->get_alt()) {
					scancode_handled = false;
					break;
				} else if (k->get_command() && cursor.column < curline_len - 1) {
#endif

					int line = cursor.line;
					int column = cursor.column;

					// Check if we are removing a single whitespace, if so remove it and the next char type,
					// else we just remove the whitespace.
					bool only_whitespace = false;
					if (_is_whitespace(text[line][column]) && _is_whitespace(text[line][column + 1])) {
						only_whitespace = true;
					} else if (_is_whitespace(text[line][column])) {
						// Remove the single whitespace.
						column++;
					}

					// Check if its a text char.
					bool only_char = (_is_text_char(text[line][column]) && !only_whitespace);

					// If its not whitespace or char then symbol.
					bool only_symbols = !(only_whitespace || only_char);

					while (column < curline_len) {
						bool is_whitespace = _is_whitespace(text[line][column]);
						bool is_text_char = _is_text_char(text[line][column]);

						if (only_whitespace && !is_whitespace) {
							break;
						} else if (only_char && !is_text_char) {
							break;
						} else if (only_symbols && (is_whitespace || is_text_char)) {
							break;
						}
						column++;
					}

					next_line = line;
					next_column = column;
#ifdef APPLE_STYLE_KEYS
				} else if (k->get_command()) {
					next_column = curline_len;
					next_line = cursor.line;
#endif
				} else {
					next_column = cursor.column < curline_len ? (cursor.column + 1) : 0;
				}

				_remove_text(cursor.line, cursor.column, next_line, next_column);
				update();

			} break;
			case KEY_KP_7: {
				if (k->get_unicode() != 0) {
					scancode_handled = false;
					break;
				}
				FALLTHROUGH;
			}
			case KEY_HOME: {
#ifdef APPLE_STYLE_KEYS
				if (k->get_shift()) {
					_pre_shift_selection();
				}

				cursor_set_line(0);

				if (k->get_shift()) {
					_post_shift_selection();
				} else if (k->get_command() || k->get_control()) {
					deselect();
				}
#else
				if (k->get_shift()) {
					_pre_shift_selection();
				}

				if (k->get_command()) {
					cursor_set_line(0);
					cursor_set_column(0);
				} else {
					// Move cursor column to start of wrapped row and then to start of text.
					Vector<String> rows = get_wrap_rows_text(cursor.line);
					int wi = get_cursor_wrap_index();
					int row_start_col = 0;
					for (int i = 0; i < wi; i++) {
						row_start_col += rows[i].length();
					}
					if (cursor.column == row_start_col || wi == 0) {
						// Compute whitespace symbols seq length.
						int current_line_whitespace_len = 0;
						while (current_line_whitespace_len < text[cursor.line].length()) {
							CharType c = text[cursor.line][current_line_whitespace_len];
							if (c != '\t' && c != ' ') {
								break;
							}
							current_line_whitespace_len++;
						}

						if (cursor_get_column() == current_line_whitespace_len) {
							cursor_set_column(0);
						} else {
							cursor_set_column(current_line_whitespace_len);
						}
					} else {
						cursor_set_column(row_start_col);
					}
				}

				if (k->get_shift()) {
					_post_shift_selection();
				} else if (k->get_command() || k->get_control()) {
					deselect();
				}
				_cancel_completion();
				completion_hint = "";
#endif
			} break;
			case KEY_KP_1: {
				if (k->get_unicode() != 0) {
					scancode_handled = false;
					break;
				}
				FALLTHROUGH;
			}
			case KEY_END: {
#ifdef APPLE_STYLE_KEYS
				if (k->get_shift()) {
					_pre_shift_selection();
				}

				cursor_set_line(get_last_unhidden_line(), true, false, 9999);

				if (k->get_shift()) {
					_post_shift_selection();
				} else if (k->get_command() || k->get_control()) {
					deselect();
				}
#else
				if (k->get_shift()) {
					_pre_shift_selection();
				}

				if (k->get_command()) {
					cursor_set_line(get_last_unhidden_line(), true, false, 9999);
				}

				// Move cursor column to end of wrapped row and then to end of text.
				Vector<String> rows = get_wrap_rows_text(cursor.line);
				int wi = get_cursor_wrap_index();
				int row_end_col = -1;
				for (int i = 0; i < wi + 1; i++) {
					row_end_col += rows[i].length();
				}
				if (wi == rows.size() - 1 || cursor.column == row_end_col) {
					cursor_set_column(text[cursor.line].length());
				} else {
					cursor_set_column(row_end_col);
				}

				if (k->get_shift()) {
					_post_shift_selection();
				} else if (k->get_command() || k->get_control()) {
					deselect();
				}

				_cancel_completion();
				completion_hint = "";
#endif
			} break;
			case KEY_KP_9: {
				if (k->get_unicode() != 0) {
					scancode_handled = false;
					break;
				}
				FALLTHROUGH;
			}
			case KEY_PAGEUP: {
				if (k->get_shift()) {
					_pre_shift_selection();
				}

				int wi;
				int n_line = cursor.line - num_lines_from_rows(cursor.line, get_cursor_wrap_index(), -get_visible_rows(), wi) + 1;
				cursor_set_line(n_line, true, false, wi);

				if (k->get_shift()) {
					_post_shift_selection();
				}

				_cancel_completion();
				completion_hint = "";

			} break;
			case KEY_KP_3: {
				if (k->get_unicode() != 0) {
					scancode_handled = false;
					break;
				}
				FALLTHROUGH;
			}
			case KEY_PAGEDOWN: {
				if (k->get_shift()) {
					_pre_shift_selection();
				}

				int wi;
				int n_line = cursor.line + num_lines_from_rows(cursor.line, get_cursor_wrap_index(), get_visible_rows(), wi) - 1;
				cursor_set_line(n_line, true, false, wi);

				if (k->get_shift()) {
					_post_shift_selection();
				}

				_cancel_completion();
				completion_hint = "";

			} break;
			case KEY_A: {
#ifndef APPLE_STYLE_KEYS
				if (!k->get_control() || k->get_shift() || k->get_alt()) {
					scancode_handled = false;
					break;
				}
				if (is_shortcut_keys_enabled()) {
					select_all();
				}
#else
				if ((!k->get_command() && !k->get_control())) {
					scancode_handled = false;
					break;
				}
				if (!k->get_shift() && k->get_command() && is_shortcut_keys_enabled())
					select_all();
				else if (k->get_control()) {
					if (k->get_shift())
						_pre_shift_selection();

					int current_line_whitespace_len = 0;
					while (current_line_whitespace_len < text[cursor.line].length()) {
						CharType c = text[cursor.line][current_line_whitespace_len];
						if (c != '\t' && c != ' ')
							break;
						current_line_whitespace_len++;
					}

					if (cursor_get_column() == current_line_whitespace_len)
						cursor_set_column(0);
					else
						cursor_set_column(current_line_whitespace_len);

					if (k->get_shift())
						_post_shift_selection();
					else if (k->get_command() || k->get_control())
						deselect();
				}
			} break;
			case KEY_E: {
				if (!k->get_control() || k->get_command() || k->get_alt()) {
					scancode_handled = false;
					break;
				}

				if (k->get_shift())
					_pre_shift_selection();

				if (k->get_command())
					cursor_set_line(text.size() - 1, true, false);
				cursor_set_column(text[cursor.line].length());

				if (k->get_shift())
					_post_shift_selection();
				else if (k->get_command() || k->get_control())
					deselect();

				_cancel_completion();
				completion_hint = "";
#endif
			} break;
			case KEY_X: {
				if (readonly) {
					break;
				}
				if (!k->get_command() || k->get_shift() || k->get_alt()) {
					scancode_handled = false;
					break;
				}
				if (is_shortcut_keys_enabled()) {
					cut();
				}

			} break;
			case KEY_C: {
				if (!k->get_command() || k->get_shift() || k->get_alt()) {
					scancode_handled = false;
					break;
				}

				if (is_shortcut_keys_enabled()) {
					copy();
				}

			} break;
			case KEY_Z: {
				if (readonly) {
					break;
				}

				if (!k->get_command()) {
					scancode_handled = false;
					break;
				}

				if (is_shortcut_keys_enabled()) {
					if (k->get_shift()) {
						redo();
					} else {
						undo();
					}
				}
			} break;
			case KEY_Y: {
				if (readonly) {
					break;
				}

				if (!k->get_command()) {
					scancode_handled = false;
					break;
				}

				if (is_shortcut_keys_enabled()) {
					redo();
				}
			} break;
			case KEY_V: {
				if (readonly) {
					break;
				}
				if (!k->get_command() || k->get_shift() || k->get_alt()) {
					scancode_handled = false;
					break;
				}

				if (is_shortcut_keys_enabled()) {
					paste();
				}

			} break;
			case KEY_SPACE: {
#ifdef OSX_ENABLED
				if (completion_enabled && k->get_metakey()) { // cmd-space is spotlight shortcut in OSX
#else
				if (completion_enabled && k->get_command()) {
#endif

					query_code_comple();
					scancode_handled = true;
				} else {
					scancode_handled = false;
				}

			} break;

			case KEY_MENU: {
				if (context_menu_enabled) {
					popup_show = true;
					if (!readonly) {
						menu->set_item_disabled(menu->get_item_index(MENU_UNDO), !has_undo());
						menu->set_item_disabled(menu->get_item_index(MENU_REDO), !has_redo());
					}

					menu->set_position(get_global_transform().xform(_get_cursor_pixel_pos()));
					menu->set_size(Vector2(1, 1));
					menu->set_scale(get_global_transform().get_scale());
					menu->popup();
					menu->grab_focus();
				}
			} break;

			default: {
				scancode_handled = false;
			} break;
		}

		if (scancode_handled) {
			accept_event();
		}

		if (k->get_scancode() == KEY_INSERT) {
			set_insert_mode(!insert_mode);
			accept_event();
			return;
		}

		if (!scancode_handled && !k->get_command()) { // For German keyboards.

			if (k->get_unicode() >= 32) {
				if (readonly) {
					return;
				}

				// Remove the old character if in insert mode and no selection.
				if (insert_mode && !had_selection) {
					begin_complex_operation();

					// Make sure we don't try and remove empty space.
					if (cursor.column < get_line(cursor.line).length()) {
						_remove_text(cursor.line, cursor.column, cursor.line, cursor.column + 1);
					}
				}

				const CharType chr[2] = { (CharType)k->get_unicode(), 0 };

				if (completion_hint != "" && k->get_unicode() == ')') {
					completion_hint = "";
				}
				if (auto_brace_completion_enabled && _is_pair_symbol(chr[0])) {
					_consume_pair_symbol(chr[0]);
				} else {
					_insert_text_at_cursor(chr);
				}

				if (insert_mode && !had_selection) {
					end_complex_operation();
				}

				if (selection.active != had_selection) {
					end_complex_operation();
				}
				accept_event();
			}
		}

		return;
	}
}

void TextEdit::_scroll_up(real_t p_delta) {
	if (scrolling && smooth_scroll_enabled && SGN(target_v_scroll - v_scroll->get_value()) != SGN(-p_delta)) {
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
		if (Math::abs(target_v_scroll - v_scroll->get_value()) < 1.0) {
			v_scroll->set_value(target_v_scroll);
		} else {
			scrolling = true;
			set_physics_process_internal(true);
		}
	} else {
		set_v_scroll(target_v_scroll);
	}
}

void TextEdit::_scroll_down(real_t p_delta) {
	if (scrolling && smooth_scroll_enabled && SGN(target_v_scroll - v_scroll->get_value()) != SGN(p_delta)) {
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
		if (Math::abs(target_v_scroll - v_scroll->get_value()) < 1.0) {
			v_scroll->set_value(target_v_scroll);
		} else {
			scrolling = true;
			set_physics_process_internal(true);
		}
	} else {
		set_v_scroll(target_v_scroll);
	}
}

void TextEdit::_pre_shift_selection() {
	if (!selection.active || selection.selecting_mode == Selection::MODE_NONE) {
		selection.selecting_line = cursor.line;
		selection.selecting_column = cursor.column;
		selection.active = true;
	}

	selection.selecting_mode = Selection::MODE_SHIFT;
}

void TextEdit::_post_shift_selection() {
	if (selection.active && selection.selecting_mode == Selection::MODE_SHIFT) {
		select(selection.selecting_line, selection.selecting_column, cursor.line, cursor.column);
		update();
	}

	selection.selecting_text = true;
}

void TextEdit::_scroll_lines_up() {
	scrolling = false;
	minimap_clicked = false;

	// Adjust the vertical scroll.
	set_v_scroll(get_v_scroll() - 1);

	// Adjust the cursor to viewport.
	if (!selection.active) {
		int cur_line = cursor.line;
		int cur_wrap = get_cursor_wrap_index();
		int last_vis_line = get_last_full_visible_line();
		int last_vis_wrap = get_last_full_visible_line_wrap_index();

		if (cur_line > last_vis_line || (cur_line == last_vis_line && cur_wrap > last_vis_wrap)) {
			cursor_set_line(last_vis_line, false, false, last_vis_wrap);
		}
	}
}

void TextEdit::_scroll_lines_down() {
	scrolling = false;
	minimap_clicked = false;

	// Adjust the vertical scroll.
	set_v_scroll(get_v_scroll() + 1);

	// Adjust the cursor to viewport.
	if (!selection.active) {
		int cur_line = cursor.line;
		int cur_wrap = get_cursor_wrap_index();
		int first_vis_line = get_first_visible_line();
		int first_vis_wrap = cursor.wrap_ofs;

		if (cur_line < first_vis_line || (cur_line == first_vis_line && cur_wrap < first_vis_wrap)) {
			cursor_set_line(first_vis_line, false, false, first_vis_wrap);
		}
	}
}

/**** TEXT EDIT CORE API ****/

void TextEdit::_base_insert_text(int p_line, int p_char, const String &p_text, int &r_end_line, int &r_end_column) {
	// Save for undo.
	ERR_FAIL_INDEX(p_line, text.size());
	ERR_FAIL_COND(p_char < 0);

	/* STEP 1: Remove \r from source text and separate in substrings. */

	Vector<String> substrings = p_text.replace("\r", "").split("\n");

	/* STEP 2: Fire breakpoint_toggled signals. */

	// Is this just a new empty line?
	bool shift_first_line = p_char == 0 && p_text.replace("\r", "") == "\n";

	int i = p_line + !shift_first_line;
	int lines = substrings.size() - 1;
	for (; i < text.size(); i++) {
		if (text.is_breakpoint(i)) {
			if ((i - lines < p_line || !text.is_breakpoint(i - lines)) || (i - lines == p_line && !shift_first_line)) {
				emit_signal("breakpoint_toggled", i);
			}
			if (i + lines >= text.size() || !text.is_breakpoint(i + lines)) {
				emit_signal("breakpoint_toggled", i + lines);
			}
		}
	}

	/* STEP 3: Add spaces if the char is greater than the end of the line. */
	while (p_char > text[p_line].length()) {
		text.set(p_line, text[p_line] + String::chr(' '));
	}

	/* STEP 4: Separate dest string in pre and post text. */

	String preinsert_text = text[p_line].substr(0, p_char);
	String postinsert_text = text[p_line].substr(p_char, text[p_line].size());

	for (int j = 0; j < substrings.size(); j++) {
		// Insert the substrings.

		if (j == 0) {
			text.set(p_line, preinsert_text + substrings[j]);
		} else {
			text.insert(p_line + j, substrings[j]);
		}

		if (j == substrings.size() - 1) {
			text.set(p_line + j, text[p_line + j] + postinsert_text);
		}
	}

	if (shift_first_line) {
		text.set_breakpoint(p_line + 1, text.is_breakpoint(p_line));
		text.set_hidden(p_line + 1, text.is_hidden(p_line));
		if (text.has_info_icon(p_line)) {
			text.set_info_icon(p_line + 1, text.get_info_icon(p_line), text.get_info(p_line));
		}

		text.set_breakpoint(p_line, false);
		text.set_hidden(p_line, false);
		text.set_info_icon(p_line, nullptr, "");
	}

	text.set_line_wrap_amount(p_line, -1);

	r_end_line = p_line + substrings.size() - 1;
	r_end_column = text[r_end_line].length() - postinsert_text.length();

	if (!text_changed_dirty && !setting_text) {
		if (is_inside_tree()) {
			MessageQueue::get_singleton()->push_call(this, "_text_changed_emit");
		}
		text_changed_dirty = true;
	}
	_line_edited_from(p_line);
}

String TextEdit::_base_get_text(int p_from_line, int p_from_column, int p_to_line, int p_to_column) const {
	ERR_FAIL_INDEX_V(p_from_line, text.size(), String());
	ERR_FAIL_INDEX_V(p_from_column, text[p_from_line].length() + 1, String());
	ERR_FAIL_INDEX_V(p_to_line, text.size(), String());
	ERR_FAIL_INDEX_V(p_to_column, text[p_to_line].length() + 1, String());
	ERR_FAIL_COND_V(p_to_line < p_from_line, String()); // 'from > to'.
	ERR_FAIL_COND_V(p_to_line == p_from_line && p_to_column < p_from_column, String()); // 'from > to'.

	String ret;

	for (int i = p_from_line; i <= p_to_line; i++) {
		int begin = (i == p_from_line) ? p_from_column : 0;
		int end = (i == p_to_line) ? p_to_column : text[i].length();

		if (i > p_from_line) {
			ret += "\n";
		}
		ret += text[i].substr(begin, end - begin);
	}

	return ret;
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

	int lines = p_to_line - p_from_line;

	for (int i = p_from_line + 1; i < text.size(); i++) {
		if (text.is_breakpoint(i)) {
			if (i + lines >= text.size() || !text.is_breakpoint(i + lines)) {
				emit_signal("breakpoint_toggled", i);
			}
			if (i > p_to_line && (i - lines < 0 || !text.is_breakpoint(i - lines))) {
				emit_signal("breakpoint_toggled", i - lines);
			}
		}
	}

	for (int i = p_from_line; i < p_to_line; i++) {
		text.remove(p_from_line + 1);
	}
	text.set(p_from_line, pre_text + post_text);

	text.set_line_wrap_amount(p_from_line, -1);

	if (!text_changed_dirty && !setting_text) {
		if (is_inside_tree()) {
			MessageQueue::get_singleton()->push_call(this, "_text_changed_emit");
		}
		text_changed_dirty = true;
	}
	_line_edited_from(p_from_line);
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

	// See if it should just be set as current op.
	if (current_op.type != op.type) {
		op.prev_version = get_version();
		_push_current_op();
		current_op = op;

		return; // Set as current op, return.
	}
	// See if it can be merged.
	if (current_op.to_line != p_line || current_op.to_column != p_char) {
		op.prev_version = get_version();
		_push_current_op();
		current_op = op;
		return; // Set as current op, return.
	}
	// Merge current op.

	current_op.text += p_text;
	current_op.to_column = retchar;
	current_op.to_line = retline;
	current_op.version = op.version;
}

void TextEdit::_remove_text(int p_from_line, int p_from_column, int p_to_line, int p_to_column) {
	if (!setting_text && idle_detect->is_inside_tree()) {
		idle_detect->start();
	}

	String text;
	if (undo_enabled) {
		_clear_redo();
		text = _base_get_text(p_from_line, p_from_column, p_to_line, p_to_column);
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
	op.text = text;
	op.version = ++version;
	op.chain_forward = false;
	op.chain_backward = false;

	// See if it should just be set as current op.
	if (current_op.type != op.type) {
		op.prev_version = get_version();
		_push_current_op();
		current_op = op;
		return; // Set as current op, return.
	}
	// See if it can be merged.
	if (current_op.from_line == p_to_line && current_op.from_column == p_to_column) {
		// Backspace or similar.
		current_op.text = text + current_op.text;
		current_op.from_line = p_from_line;
		current_op.from_column = p_from_column;
		return; // Update current op.
	}

	op.prev_version = get_version();
	_push_current_op();
	current_op = op;
}

void TextEdit::_insert_text_at_cursor(const String &p_text) {
	int new_column, new_line;
	_insert_text(cursor.line, cursor.column, p_text, &new_line, &new_column);
	_update_scrollbars();
	cursor_set_line(new_line, false);
	cursor_set_column(new_column);

	update();
}

void TextEdit::_line_edited_from(int p_line) {
	int cache_size = color_region_cache.size();
	for (int i = p_line; i < cache_size; i++) {
		color_region_cache.erase(i);
	}

	if (syntax_highlighting_cache.size() > 0) {
		cache_size = syntax_highlighting_cache.back()->key();
		for (int i = p_line - 1; i <= cache_size; i++) {
			if (syntax_highlighting_cache.has(i)) {
				syntax_highlighting_cache.erase(i);
			}
		}
	}
}

int TextEdit::get_char_count() {
	int totalsize = 0;

	for (int i = 0; i < text.size(); i++) {
		if (i > 0) {
			totalsize++; // Include \n.
		}
		totalsize += text[i].length();
	}

	return totalsize; // Omit last \n.
}

Size2 TextEdit::get_minimum_size() const {
	return cache.style_normal->get_minimum_size();
}

int TextEdit::_get_control_height() const {
	int control_height = get_size().height;
	control_height -= cache.style_normal->get_minimum_size().height;
	if (h_scroll->is_visible_in_tree()) {
		control_height -= h_scroll->get_size().height;
	}
	return control_height;
}

void TextEdit::_generate_context_menu() {
	// Reorganize context menu.
	menu->clear();
	if (!readonly) {
		menu->add_item(RTR("Cut"), MENU_CUT, is_shortcut_keys_enabled() ? KEY_MASK_CMD | KEY_X : 0);
	}
	menu->add_item(RTR("Copy"), MENU_COPY, is_shortcut_keys_enabled() ? KEY_MASK_CMD | KEY_C : 0);
	if (!readonly) {
		menu->add_item(RTR("Paste"), MENU_PASTE, is_shortcut_keys_enabled() ? KEY_MASK_CMD | KEY_V : 0);
	}
	if (selecting_enabled || !readonly) {
		menu->add_separator();
	}
	if (selecting_enabled) {
		menu->add_item(RTR("Select All"), MENU_SELECT_ALL, is_shortcut_keys_enabled() ? KEY_MASK_CMD | KEY_A : 0);
	}
	if (!readonly) {
		menu->add_item(RTR("Clear"), MENU_CLEAR);
		menu->add_separator();
		menu->add_item(RTR("Undo"), MENU_UNDO, is_shortcut_keys_enabled() ? KEY_MASK_CMD | KEY_Z : 0);
		menu->add_item(RTR("Redo"), MENU_REDO, is_shortcut_keys_enabled() ? KEY_MASK_CMD | KEY_MASK_SHIFT | KEY_Z : 0);
	}
}

int TextEdit::get_visible_rows() const {
	return _get_control_height() / get_row_height();
}

int TextEdit::_get_minimap_visible_rows() const {
	return _get_control_height() / (minimap_char_size.y + minimap_line_spacing);
}

int TextEdit::get_total_visible_rows() const {
	// Returns the total amount of rows we need in the editor.
	// This skips hidden lines and counts each wrapping of a line.
	if (!is_hiding_enabled() && !is_wrap_enabled()) {
		return text.size();
	}

	int total_rows = 0;
	for (int i = 0; i < text.size(); i++) {
		if (!text.is_hidden(i)) {
			total_rows++;
			total_rows += times_line_wraps(i);
		}
	}
	return total_rows;
}

void TextEdit::_update_wrap_at() {
	wrap_at = get_size().width - cache.style_normal->get_minimum_size().width - cache.line_number_w - cache.breakpoint_gutter_width - cache.fold_gutter_width - cache.info_gutter_width - cache.minimap_width - wrap_right_offset;
	update_cursor_wrap_offset();
	text.clear_wrap_cache();

	for (int i = 0; i < text.size(); i++) {
		// Update all values that wrap.
		if (!line_wraps(i)) {
			continue;
		}
		Vector<String> rows = get_wrap_rows_text(i);
		text.set_line_wrap_amount(i, rows.size() - 1);
	}
}

void TextEdit::adjust_viewport_to_cursor() {
	// Make sure cursor is visible on the screen.
	scrolling = false;
	minimap_clicked = false;

	int cur_line = cursor.line;
	int cur_wrap = get_cursor_wrap_index();

	int first_vis_line = get_first_visible_line();
	int first_vis_wrap = cursor.wrap_ofs;
	int last_vis_line = get_last_full_visible_line();
	int last_vis_wrap = get_last_full_visible_line_wrap_index();

	if (cur_line < first_vis_line || (cur_line == first_vis_line && cur_wrap < first_vis_wrap)) {
		// Cursor is above screen.
		set_line_as_first_visible(cur_line, cur_wrap);
	} else if (cur_line > last_vis_line || (cur_line == last_vis_line && cur_wrap > last_vis_wrap)) {
		// Cursor is below screen.
		set_line_as_last_visible(cur_line, cur_wrap);
	}

	int visible_width = get_size().width - cache.style_normal->get_minimum_size().width - cache.line_number_w - cache.breakpoint_gutter_width - cache.fold_gutter_width - cache.info_gutter_width - cache.minimap_width;
	if (v_scroll->is_visible_in_tree()) {
		visible_width -= v_scroll->get_combined_minimum_size().width;
	}
	visible_width -= 20; // Give it a little more space.

	if (!is_wrap_enabled()) {
		// Adjust x offset.
		int cursor_x = get_column_x_offset(cursor.column, text[cursor.line]);

		if (cursor_x > (cursor.x_ofs + visible_width)) {
			cursor.x_ofs = cursor_x - visible_width + 1;
		}

		if (cursor_x < cursor.x_ofs) {
			cursor.x_ofs = cursor_x;
		}
	} else {
		cursor.x_ofs = 0;
	}
	h_scroll->set_value(cursor.x_ofs);

	update();
}

void TextEdit::center_viewport_to_cursor() {
	// Move viewport so the cursor is in the center of the screen.
	scrolling = false;
	minimap_clicked = false;

	if (is_line_hidden(cursor.line)) {
		unfold_line(cursor.line);
	}

	set_line_as_center_visible(cursor.line, get_cursor_wrap_index());
	int visible_width = get_size().width - cache.style_normal->get_minimum_size().width - cache.line_number_w - cache.breakpoint_gutter_width - cache.fold_gutter_width - cache.info_gutter_width - cache.minimap_width;
	if (v_scroll->is_visible_in_tree()) {
		visible_width -= v_scroll->get_combined_minimum_size().width;
	}
	visible_width -= 20; // Give it a little more space.

	if (is_wrap_enabled()) {
		// Center x offset.
		int cursor_x = get_column_x_offset_for_line(cursor.column, cursor.line);

		if (cursor_x > (cursor.x_ofs + visible_width)) {
			cursor.x_ofs = cursor_x - visible_width + 1;
		}

		if (cursor_x < cursor.x_ofs) {
			cursor.x_ofs = cursor_x;
		}
	} else {
		cursor.x_ofs = 0;
	}
	h_scroll->set_value(cursor.x_ofs);

	update();
}

void TextEdit::update_cursor_wrap_offset() {
	int first_vis_line = get_first_visible_line();
	if (line_wraps(first_vis_line)) {
		cursor.wrap_ofs = MIN(cursor.wrap_ofs, times_line_wraps(first_vis_line));
	} else {
		cursor.wrap_ofs = 0;
	}
	set_line_as_first_visible(cursor.line_ofs, cursor.wrap_ofs);
}

bool TextEdit::line_wraps(int line) const {
	ERR_FAIL_INDEX_V(line, text.size(), 0);
	if (!is_wrap_enabled()) {
		return false;
	}
	return text.get_line_width(line) > wrap_at;
}

int TextEdit::times_line_wraps(int line) const {
	ERR_FAIL_INDEX_V(line, text.size(), 0);
	if (!line_wraps(line)) {
		return 0;
	}

	int wrap_amount = text.get_line_wrap_amount(line);
	if (wrap_amount == -1) {
		// Update the value.
		Vector<String> rows = get_wrap_rows_text(line);
		wrap_amount = rows.size() - 1;
		text.set_line_wrap_amount(line, wrap_amount);
	}

	return wrap_amount;
}

Vector<String> TextEdit::get_wrap_rows_text(int p_line) const {
	ERR_FAIL_INDEX_V(p_line, text.size(), Vector<String>());

	Vector<String> lines;
	if (!line_wraps(p_line)) {
		lines.push_back(text[p_line]);
		return lines;
	}

	int px = 0;
	int col = 0;
	String line_text = text[p_line];
	String wrap_substring = "";

	int word_px = 0;
	String word_str = "";
	int cur_wrap_index = 0;

	int tab_offset_px = get_indent_level(p_line) * cache.font->get_char_size(' ').width;
	if (tab_offset_px >= wrap_at) {
		tab_offset_px = 0;
	}

	while (col < line_text.length()) {
		CharType c = line_text[col];
		int w = text.get_char_width(c, line_text[col + 1], px + word_px);

		int indent_ofs = (cur_wrap_index != 0 ? tab_offset_px : 0);

		if (indent_ofs + word_px + w > wrap_at) {
			// Not enough space to add this char; start next line.
			wrap_substring += word_str;
			lines.push_back(wrap_substring);
			cur_wrap_index++;
			wrap_substring = "";
			px = 0;

			word_str = "";
			word_str += c;
			word_px = w;
		} else {
			word_str += c;
			word_px += w;
			if (c == ' ') {
				// End of a word; add this word to the substring.
				wrap_substring += word_str;
				px += word_px;
				word_str = "";
				word_px = 0;
			}

			if (indent_ofs + px + word_px > wrap_at) {
				// This word will be moved to the next line.
				lines.push_back(wrap_substring);
				// Reset for next wrap.
				cur_wrap_index++;
				wrap_substring = "";
				px = 0;
			}
		}
		col++;
	}
	// Line ends before hit wrap_at; add this word to the substring.
	wrap_substring += word_str;
	lines.push_back(wrap_substring);

	// Update cache.
	text.set_line_wrap_amount(p_line, lines.size() - 1);

	return lines;
}

int TextEdit::get_cursor_wrap_index() const {
	return get_line_wrap_index_at_col(cursor.line, cursor.column);
}

int TextEdit::get_line_wrap_index_at_col(int p_line, int p_column) const {
	ERR_FAIL_INDEX_V(p_line, text.size(), 0);

	if (!line_wraps(p_line)) {
		return 0;
	}

	// Loop through wraps in the line text until we get to the column.
	int wrap_index = 0;
	int col = 0;
	Vector<String> rows = get_wrap_rows_text(p_line);
	for (int i = 0; i < rows.size(); i++) {
		wrap_index = i;
		String s = rows[wrap_index];
		col += s.length();
		if (col > p_column) {
			break;
		}
	}
	return wrap_index;
}

void TextEdit::cursor_set_column(int p_col, bool p_adjust_viewport) {
	if (p_col < 0) {
		p_col = 0;
	}

	cursor.column = p_col;
	if (cursor.column > get_line(cursor.line).length()) {
		cursor.column = get_line(cursor.line).length();
	}

	cursor.last_fit_x = get_column_x_offset_for_line(cursor.column, cursor.line);

	if (p_adjust_viewport) {
		adjust_viewport_to_cursor();
	}

	if (!cursor_changed_dirty) {
		if (is_inside_tree()) {
			MessageQueue::get_singleton()->push_call(this, "_cursor_changed_emit");
		}
		cursor_changed_dirty = true;
	}
}

void TextEdit::cursor_set_line(int p_row, bool p_adjust_viewport, bool p_can_be_hidden, int p_wrap_index) {
	if (setting_row) {
		return;
	}

	setting_row = true;
	if (p_row < 0) {
		p_row = 0;
	}

	if (p_row >= text.size()) {
		p_row = text.size() - 1;
	}

	if (!p_can_be_hidden) {
		if (is_line_hidden(CLAMP(p_row, 0, text.size() - 1))) {
			int move_down = num_lines_from(p_row, 1) - 1;
			if (p_row + move_down <= text.size() - 1 && !is_line_hidden(p_row + move_down)) {
				p_row += move_down;
			} else {
				int move_up = num_lines_from(p_row, -1) - 1;
				if (p_row - move_up > 0 && !is_line_hidden(p_row - move_up)) {
					p_row -= move_up;
				} else {
					WARN_PRINT(("Cursor set to hidden line " + itos(p_row) + " and there are no nonhidden lines."));
				}
			}
		}
	}
	cursor.line = p_row;

	int n_col = get_char_pos_for_line(cursor.last_fit_x, p_row, p_wrap_index);
	if (n_col != 0 && is_wrap_enabled() && p_wrap_index < times_line_wraps(p_row)) {
		Vector<String> rows = get_wrap_rows_text(p_row);
		int row_end_col = 0;
		for (int i = 0; i < p_wrap_index + 1; i++) {
			row_end_col += rows[i].length();
		}
		if (n_col >= row_end_col) {
			n_col -= 1;
		}
	}
	cursor.column = n_col;

	if (p_adjust_viewport) {
		adjust_viewport_to_cursor();
	}

	setting_row = false;

	if (!cursor_changed_dirty) {
		if (is_inside_tree()) {
			MessageQueue::get_singleton()->push_call(this, "_cursor_changed_emit");
		}
		cursor_changed_dirty = true;
	}
}

int TextEdit::cursor_get_column() const {
	return cursor.column;
}

int TextEdit::cursor_get_line() const {
	return cursor.line;
}

bool TextEdit::cursor_get_blink_enabled() const {
	return caret_blink_enabled;
}

void TextEdit::cursor_set_blink_enabled(const bool p_enabled) {
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

float TextEdit::cursor_get_blink_speed() const {
	return caret_blink_timer->get_wait_time();
}

void TextEdit::cursor_set_blink_speed(const float p_speed) {
	ERR_FAIL_COND(p_speed <= 0);
	caret_blink_timer->set_wait_time(p_speed);
}

void TextEdit::cursor_set_block_mode(const bool p_enable) {
	block_caret = p_enable;
	update();
}

bool TextEdit::cursor_is_block_mode() const {
	return block_caret;
}

void TextEdit::set_right_click_moves_caret(bool p_enable) {
	right_click_moves_caret = p_enable;
}

bool TextEdit::is_right_click_moving_caret() const {
	return right_click_moves_caret;
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
		cursor.x_ofs = h_scroll->get_value();
	}
	if (v_scroll->is_visible_in_tree()) {
		// Set line ofs and wrap ofs.
		int v_scroll_i = floor(get_v_scroll());
		int sc = 0;
		int n_line;
		for (n_line = 0; n_line < text.size(); n_line++) {
			if (!is_line_hidden(n_line)) {
				sc++;
				sc += times_line_wraps(n_line);
				if (sc > v_scroll_i) {
					break;
				}
			}
		}
		n_line = MIN(n_line, text.size() - 1);
		int line_wrap_amount = times_line_wraps(n_line);
		int wi = line_wrap_amount - (sc - v_scroll_i - 1);
		wi = CLAMP(wi, 0, line_wrap_amount);

		cursor.line_ofs = n_line;
		cursor.wrap_ofs = wi;
	}
	update();
}

int TextEdit::get_row_height() const {
	return cache.font->get_height() + cache.line_spacing;
}

/* Line and character position. */
Point2 TextEdit::get_pos_at_line_column(int p_line, int p_column) const {
	Rect2i rect = get_rect_at_line_column(p_line, p_column);
	return rect.position + Vector2i(0, get_line_height());
}

Rect2 TextEdit::get_rect_at_line_column(int p_line, int p_column) const {
	ERR_FAIL_INDEX_V(p_line, text.size(), Rect2i(-1, -1, 0, 0));
	ERR_FAIL_COND_V(p_column < 0, Rect2i(-1, -1, 0, 0));
	ERR_FAIL_COND_V(p_column > text[p_line].length(), Rect2i(-1, -1, 0, 0));

	if (line_drawing_cache.size() == 0 || !line_drawing_cache.has(p_line)) {
		// Line is not in the cache, which means it's outside of the viewing area.
		return Rect2i(-1, -1, 0, 0);
	}
	LineDrawingCache cache_entry = line_drawing_cache[p_line];

	int wrap_index = get_line_wrap_index_at_col(p_line, p_column);
	if (wrap_index >= cache_entry.first_visible_char.size()) {
		// Line seems to be wrapped beyond the viewable area.
		return Rect2i(-1, -1, 0, 0);
	}

	int first_visible_char = cache_entry.first_visible_char[wrap_index];
	int last_visible_char = cache_entry.last_visible_char[wrap_index];
	if (p_column < first_visible_char || p_column > (last_visible_char + 1)) {
		// Character is outside of the viewing area, no point calculating its position.
		return Rect2i(-1, -1, 0, 0);
	}

	Point2i pos, size;
	pos.y = cache_entry.y_offset + get_line_height() * wrap_index;
	pos.x = get_total_gutter_width() + cache.style_normal->get_margin(MARGIN_LEFT) - get_h_scroll();

	int start_x = get_column_x_offset_for_line(p_column, p_line);
	pos.x += start_x;

	String line = text[p_line];
	size.x = cache.font->get_char_size(line[p_column]).width;
	size.y = get_line_height();

	return Rect2i(pos, size);
}

Point2 TextEdit::get_line_column_at_pos(const Point2 &p_pos) const {
	int row, col;
	_get_mouse_pos(p_pos, row, col);

	return Point2i(col, row);
}

int TextEdit::get_char_pos_for_line(int p_px, int p_line, int p_wrap_index) const {
	ERR_FAIL_INDEX_V(p_line, text.size(), 0);

	if (line_wraps(p_line)) {
		int line_wrap_amount = times_line_wraps(p_line);
		int wrap_offset_px = get_indent_level(p_line) * cache.font->get_char_size(' ').width;
		if (wrap_offset_px >= wrap_at) {
			wrap_offset_px = 0;
		}
		if (p_wrap_index > line_wrap_amount) {
			p_wrap_index = line_wrap_amount;
		}
		if (p_wrap_index > 0) {
			p_px -= wrap_offset_px;
		} else {
			p_wrap_index = 0;
		}
		Vector<String> rows = get_wrap_rows_text(p_line);
		int c_pos = get_char_pos_for(p_px, rows[p_wrap_index]);
		for (int i = 0; i < p_wrap_index; i++) {
			String s = rows[i];
			c_pos += s.length();
		}

		return c_pos;
	} else {
		return get_char_pos_for(p_px, text[p_line]);
	}
}

int TextEdit::get_column_x_offset_for_line(int p_char, int p_line) const {
	ERR_FAIL_INDEX_V(p_line, text.size(), 0);

	if (line_wraps(p_line)) {
		int n_char = p_char;
		int col = 0;
		Vector<String> rows = get_wrap_rows_text(p_line);
		int wrap_index = 0;
		for (int i = 0; i < rows.size(); i++) {
			wrap_index = i;
			String s = rows[wrap_index];
			col += s.length();
			if (col > p_char) {
				break;
			}
			n_char -= s.length();
		}
		int px = get_column_x_offset(n_char, rows[wrap_index]);

		int wrap_offset_px = get_indent_level(p_line) * cache.font->get_char_size(' ').width;
		if (wrap_offset_px >= wrap_at) {
			wrap_offset_px = 0;
		}
		if (wrap_index != 0) {
			px += wrap_offset_px;
		}

		return px;
	} else {
		return get_column_x_offset(p_char, text[p_line]);
	}
}

int TextEdit::get_char_pos_for(int p_px, String p_str) const {
	int px = 0;
	int c = 0;

	while (c < p_str.length()) {
		int w = text.get_char_width(p_str[c], p_str[c + 1], px);

		if (p_px < (px + w / 2)) {
			break;
		}
		px += w;
		c++;
	}

	return c;
}

int TextEdit::get_column_x_offset(int p_char, String p_str) const {
	int px = 0;

	for (int i = 0; i < p_char; i++) {
		if (i >= p_str.length()) {
			break;
		}

		px += text.get_char_width(p_str[i], p_str[i + 1], px);
	}

	return px;
}

void TextEdit::insert_text_at_cursor(const String &p_text) {
	if (selection.active) {
		cursor_set_line(selection.from_line, false);
		cursor_set_column(selection.from_column);

		_remove_text(selection.from_line, selection.from_column, selection.to_line, selection.to_column);
		selection.active = false;
		selection.selecting_mode = Selection::MODE_NONE;
	}

	_insert_text_at_cursor(p_text);
	update();
}

Variant TextEdit::get_drag_data(const Point2 &p_point) {
	if (selection.active && selection.drag_attempt) {
		String t = get_selection_text();
		Label *l = memnew(Label);
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

	return !readonly && p_data.get_type() == Variant::STRING;
}

void TextEdit::drop_data(const Point2 &p_point, const Variant &p_data) {
	Control::drop_data(p_point, p_data);

	if (p_data.get_type() == Variant::STRING && !readonly) {
		Point2 mp = get_local_mouse_position();
		int caret_row_tmp, caret_column_tmp;
		_get_mouse_pos(Point2i(mp.x, mp.y), caret_row_tmp, caret_column_tmp);
		if (selection.drag_attempt) {
			selection.drag_attempt = false;
			if (!is_mouse_over_selection(!Input::get_singleton()->is_key_pressed(KEY_CONTROL))) {
				begin_complex_operation();
				if (!Input::get_singleton()->is_key_pressed(KEY_CONTROL)) {
					if (caret_row_tmp > selection.to_line) {
						caret_row_tmp = caret_row_tmp - (selection.to_line - selection.from_line);
					} else if (caret_row_tmp == selection.to_line && caret_column_tmp >= selection.to_column) {
						caret_column_tmp = caret_column_tmp - (selection.to_column - selection.from_column);
					}

					_remove_text(selection.from_line, selection.from_column, selection.to_line, selection.to_column);
					cursor_set_line(selection.from_line, false);
					cursor_set_column(selection.from_column);
					selection.active = false;
					selection.selecting_mode = Selection::MODE_NONE;
				} else {
					deselect();
				}

				cursor_set_line(caret_row_tmp, true, false);
				cursor_set_column(caret_column_tmp);
				insert_text_at_cursor(p_data);
				end_complex_operation();
			}
		} else if (is_mouse_over_selection()) {
			begin_complex_operation();
			caret_row_tmp = selection.from_line;
			caret_column_tmp = selection.from_column;

			_remove_text(selection.from_line, selection.from_column, selection.to_line, selection.to_column);
			cursor_set_line(selection.from_line, false);
			cursor_set_column(selection.from_column);
			selection.active = false;
			selection.selecting_mode = Selection::MODE_NONE;

			cursor_set_line(caret_row_tmp, true, false);
			cursor_set_column(caret_column_tmp);
			insert_text_at_cursor(p_data);
			end_complex_operation();
			grab_focus();
		} else {
			deselect();
			cursor_set_line(caret_row_tmp, true, false);
			cursor_set_column(caret_column_tmp);
			insert_text_at_cursor(p_data);
			grab_focus();
		}

		if (caret_row_tmp != cursor.line || caret_column_tmp != cursor.column) {
			select(caret_row_tmp, caret_column_tmp, cursor.line, cursor.column);
		}
	}
}

bool TextEdit::is_mouse_over_selection(bool p_edges) const {
	if (!selection.active) {
		return false;
	}
	Point2 mp = get_local_mouse_position();
	int row, col;
	_get_mouse_pos(Point2i(mp.x, mp.y), row, col);
	if (p_edges) {
		if ((row == selection.from_line && col == selection.from_column) || (row == selection.to_line && col == selection.to_column)) {
			return true;
		}
	}
	return (row >= selection.from_line && row <= selection.to_line && (row > selection.from_line || col > selection.from_column) && (row < selection.to_line || col < selection.to_column));
}

Control::CursorShape TextEdit::get_cursor_shape(const Point2 &p_pos) const {
	if (highlighted_word != String()) {
		return CURSOR_POINTING_HAND;
	}

	if ((completion_active && completion_rect.has_point(p_pos)) || (is_readonly() && (!is_selecting_enabled() || text.size() == 0))) {
		return CURSOR_ARROW;
	}

	int gutter = cache.style_normal->get_margin(MARGIN_LEFT) + cache.line_number_w + cache.breakpoint_gutter_width + cache.fold_gutter_width + cache.info_gutter_width;
	if (p_pos.x < gutter) {
		int row, col;
		_get_mouse_pos(p_pos, row, col);
		int left_margin = cache.style_normal->get_margin(MARGIN_LEFT);

		// Breakpoint icon.
		if (draw_breakpoint_gutter && p_pos.x > left_margin - 6 && p_pos.x <= left_margin + cache.breakpoint_gutter_width - 3) {
			return CURSOR_POINTING_HAND;
		}

		// Info icons.
		int gutter_left = left_margin + cache.breakpoint_gutter_width + cache.info_gutter_width;
		if (draw_info_gutter && p_pos.x > left_margin + cache.breakpoint_gutter_width - 6 && p_pos.x <= gutter_left - 3) {
			if (text.has_info_icon(row)) {
				return CURSOR_POINTING_HAND;
			}
			return CURSOR_ARROW;
		}

		// Fold icon.
		if (draw_fold_gutter && p_pos.x > gutter_left + cache.line_number_w - 6 && p_pos.x <= gutter_left + cache.line_number_w + cache.fold_gutter_width - 3) {
			if (is_folded(row) || can_fold(row)) {
				return CURSOR_POINTING_HAND;
			} else {
				return CURSOR_ARROW;
			}
		}

		return CURSOR_ARROW;
	} else {
		int xmargin_end = get_size().width - cache.style_normal->get_margin(MARGIN_RIGHT);
		if (draw_minimap && p_pos.x > xmargin_end - minimap_width && p_pos.x <= xmargin_end) {
			return CURSOR_ARROW;
		}

		int row, col;
		_get_mouse_pos(p_pos, row, col);
		// EOL fold icon.
		if (is_folded(row)) {
			int line_width = text.get_line_width(row);
			line_width += cache.style_normal->get_margin(MARGIN_LEFT) + cache.line_number_w + cache.breakpoint_gutter_width + cache.fold_gutter_width + cache.info_gutter_width - cursor.x_ofs;
			if (p_pos.x > line_width - 3 && p_pos.x <= line_width + cache.folded_eol_icon->get_width() + 3) {
				return CURSOR_POINTING_HAND;
			}
		}
	}

	return get_default_cursor_shape();
}

void TextEdit::set_text(String p_text) {
	setting_text = true;
	if (!undo_enabled) {
		_clear();
		_insert_text_at_cursor(p_text);
	}

	if (undo_enabled) {
		cursor_set_line(0);
		cursor_set_column(0);

		begin_complex_operation();
		_remove_text(0, 0, MAX(0, get_line_count() - 1), MAX(get_line(MAX(get_line_count() - 1, 0)).size() - 1, 0));
		_insert_text_at_cursor(p_text);
		end_complex_operation();
		selection.active = false;
	}

	cursor_set_line(0);
	cursor_set_column(0);

	update();
	setting_text = false;
};

String TextEdit::get_text() {
	String longthing;
	int len = text.size();
	for (int i = 0; i < len; i++) {
		longthing += text[i];
		if (i != len - 1) {
			longthing += "\n";
		}
	}

	return longthing;
};

String TextEdit::get_text_for_lookup_completion() {
	int row, col;
	_get_mouse_pos(get_local_mouse_position(), row, col);

	String longthing;
	int len = text.size();
	for (int i = 0; i < len; i++) {
		if (i == row) {
			longthing += text[i].substr(0, col);
			longthing += String::chr(0xFFFF); // Not unicode, represents the cursor.
			longthing += text[i].substr(col, text[i].size());
		} else {
			longthing += text[i];
		}

		if (i != len - 1) {
			longthing += "\n";
		}
	}

	return longthing;
}

String TextEdit::get_text_for_completion() {
	String longthing;
	int len = text.size();
	for (int i = 0; i < len; i++) {
		if (i == cursor.line) {
			longthing += text[i].substr(0, cursor.column);
			longthing += String::chr(0xFFFF); // Not unicode, represents the cursor.
			longthing += text[i].substr(cursor.column, text[i].size());
		} else {
			longthing += text[i];
		}

		if (i != len - 1) {
			longthing += "\n";
		}
	}

	return longthing;
};

String TextEdit::get_line(int line) const {
	if (line < 0 || line >= text.size()) {
		return "";
	}

	return text[line];
};

void TextEdit::_clear() {
	clear_undo_history();
	text.clear();
	cursor.column = 0;
	cursor.line = 0;
	cursor.x_ofs = 0;
	cursor.line_ofs = 0;
	cursor.wrap_ofs = 0;
	cursor.last_fit_x = 0;
	selection.active = false;
}

void TextEdit::clear() {
	setting_text = true;
	_clear();
	setting_text = false;
};

void TextEdit::set_readonly(bool p_readonly) {
	if (readonly == p_readonly) {
		return;
	}

	readonly = p_readonly;
	_generate_context_menu();

	// Reorganize context menu.
	menu->clear();

	if (!readonly) {
		menu->add_item(RTR("Undo"), MENU_UNDO, KEY_MASK_CMD | KEY_Z);
		menu->add_item(RTR("Redo"), MENU_REDO, KEY_MASK_CMD | KEY_MASK_SHIFT | KEY_Z);
	}

	if (!readonly) {
		menu->add_separator();
		menu->add_item(RTR("Cut"), MENU_CUT, KEY_MASK_CMD | KEY_X);
	}

	menu->add_item(RTR("Copy"), MENU_COPY, KEY_MASK_CMD | KEY_C);

	if (!readonly) {
		menu->add_item(RTR("Paste"), MENU_PASTE, KEY_MASK_CMD | KEY_V);
	}

	menu->add_separator();
	menu->add_item(RTR("Select All"), MENU_SELECT_ALL, KEY_MASK_CMD | KEY_A);

	if (!readonly) {
		menu->add_item(RTR("Clear"), MENU_CLEAR);
	}

	update();
}

bool TextEdit::is_readonly() const {
	return readonly;
}

void TextEdit::set_wrap_enabled(bool p_wrap_enabled) {
	wrap_enabled = p_wrap_enabled;
}

bool TextEdit::is_wrap_enabled() const {
	return wrap_enabled;
}

void TextEdit::_reset_caret_blink_timer() {
	if (caret_blink_enabled) {
		draw_caret = true;
		if (has_focus()) {
			caret_blink_timer->stop();
			caret_blink_timer->start();
			update();
		}
	}
}

void TextEdit::_toggle_draw_caret() {
	draw_caret = !draw_caret;
	if (is_visible_in_tree() && has_focus() && window_has_focus) {
		update();
	}
}

void TextEdit::_update_caches() {
	cache.style_normal = get_stylebox("normal");
	cache.style_focus = get_stylebox("focus");
	cache.style_readonly = get_stylebox("read_only");
	cache.completion_background_color = get_color("completion_background_color");
	cache.completion_selected_color = get_color("completion_selected_color");
	cache.completion_existing_color = get_color("completion_existing_color");
	cache.completion_font_color = get_color("completion_font_color");
	cache.font = get_font("font");
	cache.caret_color = get_color("caret_color");
	cache.caret_background_color = get_color("caret_background_color");
	cache.line_number_color = get_color("line_number_color");
	cache.safe_line_number_color = get_color("safe_line_number_color");
	cache.font_color = get_color("font_color");
	cache.font_color_selected = get_color("font_color_selected");
	cache.font_color_readonly = get_color("font_color_readonly");
	cache.keyword_color = get_color("keyword_color");
	cache.control_flow_keyword_color = get_color("control_flow_keyword_color");
	cache.function_color = get_color("function_color");
	cache.member_variable_color = get_color("member_variable_color");
	cache.number_color = get_color("number_color");
	cache.selection_color = get_color("selection_color");
	cache.mark_color = get_color("mark_color");
	cache.current_line_color = get_color("current_line_color");
	cache.line_length_guideline_color = get_color("line_length_guideline_color");
	cache.bookmark_color = get_color("bookmark_color");
	cache.breakpoint_color = get_color("breakpoint_color");
	cache.executing_line_color = get_color("executing_line_color");
	cache.code_folding_color = get_color("code_folding_color");
	cache.brace_mismatch_color = get_color("brace_mismatch_color");
	cache.word_highlighted_color = get_color("word_highlighted_color");
	cache.search_result_color = get_color("search_result_color");
	cache.search_result_border_color = get_color("search_result_border_color");
	cache.symbol_color = get_color("symbol_color");
	cache.background_color = get_color("background_color");
#ifdef TOOLS_ENABLED
	cache.line_spacing = get_constant("line_spacing") * EDSCALE;
#else
	cache.line_spacing = get_constant("line_spacing");
#endif
	cache.row_height = cache.font->get_height() + cache.line_spacing;
	cache.tab_icon = get_icon("tab");
	cache.space_icon = get_icon("space");
	cache.folded_icon = get_icon("folded");
	cache.can_fold_icon = get_icon("fold");
	cache.folded_eol_icon = get_icon("GuiEllipsis", "EditorIcons");
	cache.executing_icon = get_icon("TextEditorPlay", "EditorIcons");
	text.set_font(cache.font);

	if (syntax_highlighter) {
		syntax_highlighter->_update_cache();
	}
}

SyntaxHighlighter *TextEdit::_get_syntax_highlighting() {
	return syntax_highlighter;
}

void TextEdit::_set_syntax_highlighting(SyntaxHighlighter *p_syntax_highlighter) {
	syntax_highlighter = p_syntax_highlighter;
	if (syntax_highlighter) {
		syntax_highlighter->set_text_editor(this);
		syntax_highlighter->_update_cache();
	}
	syntax_highlighting_cache.clear();
	update();
}

int TextEdit::_is_line_in_region(int p_line) {
	// Do we have in cache?
	if (color_region_cache.has(p_line)) {
		return color_region_cache[p_line];
	}

	// If not find the closest line we have.
	int previous_line = p_line - 1;
	for (; previous_line > -1; previous_line--) {
		if (color_region_cache.has(previous_line)) {
			break;
		}
	}

	// Calculate up to line we need and update the cache along the way.
	int in_region = color_region_cache[previous_line];
	if (previous_line == -1) {
		in_region = -1;
	}
	for (int i = previous_line; i < p_line; i++) {
		const Map<int, Text::ColorRegionInfo> &cri_map = _get_line_color_region_info(i);
		for (const Map<int, Text::ColorRegionInfo>::Element *E = cri_map.front(); E; E = E->next()) {
			const Text::ColorRegionInfo &cri = E->get();
			if (in_region == -1) {
				if (!cri.end) {
					in_region = cri.region;
				}
			} else if (in_region == cri.region && !_get_color_region(cri.region).line_only) {
				if (cri.end || _get_color_region(cri.region).eq) {
					in_region = -1;
				}
			}
		}

		if (in_region >= 0 && _get_color_region(in_region).line_only) {
			in_region = -1;
		}

		color_region_cache[i + 1] = in_region;
	}
	return in_region;
}

TextEdit::ColorRegion TextEdit::_get_color_region(int p_region) const {
	if (p_region < 0 || p_region >= color_regions.size()) {
		return ColorRegion();
	}
	return color_regions[p_region];
}

Map<int, TextEdit::Text::ColorRegionInfo> TextEdit::_get_line_color_region_info(int p_line) const {
	if (p_line < 0 || p_line > text.size() - 1) {
		return Map<int, Text::ColorRegionInfo>();
	}
	return text.get_color_region_info(p_line);
}

void TextEdit::clear_colors() {
	keywords.clear();
	member_keywords.clear();
	color_regions.clear();
	color_region_cache.clear();
	syntax_highlighting_cache.clear();
	text.clear_width_cache();
	update();
}

void TextEdit::add_keyword_color(const String &p_keyword, const Color &p_color) {
	keywords[p_keyword] = p_color;
	syntax_highlighting_cache.clear();
	update();
}

bool TextEdit::has_keyword_color(String p_keyword) const {
	return keywords.has(p_keyword);
}

Color TextEdit::get_keyword_color(String p_keyword) const {
	ERR_FAIL_COND_V(!keywords.has(p_keyword), Color());
	return keywords[p_keyword];
}

void TextEdit::add_color_region(const String &p_begin_key, const String &p_end_key, const Color &p_color, bool p_line_only) {
	color_regions.push_back(ColorRegion(p_begin_key, p_end_key, p_color, p_line_only));
	syntax_highlighting_cache.clear();
	text.clear_width_cache();
	update();
}

void TextEdit::add_member_keyword(const String &p_keyword, const Color &p_color) {
	member_keywords[p_keyword] = p_color;
	syntax_highlighting_cache.clear();
	update();
}

bool TextEdit::has_member_color(String p_member) const {
	return member_keywords.has(p_member);
}

Color TextEdit::get_member_color(String p_member) const {
	return member_keywords[p_member];
}

void TextEdit::clear_member_keywords() {
	member_keywords.clear();
	syntax_highlighting_cache.clear();
	update();
}

void TextEdit::set_syntax_coloring(bool p_enabled) {
	syntax_coloring = p_enabled;
	update();
}

bool TextEdit::is_syntax_coloring_enabled() const {
	return syntax_coloring;
}

void TextEdit::set_auto_indent(bool p_auto_indent) {
	auto_indent = p_auto_indent;
}

void TextEdit::cut() {
	if (!selection.active) {
		String clipboard = text[cursor.line];
		OS::get_singleton()->set_clipboard(clipboard);
		cursor_set_line(cursor.line);
		cursor_set_column(0);

		if (cursor.line == 0 && get_line_count() > 1) {
			_remove_text(cursor.line, 0, cursor.line + 1, 0);
		} else {
			_remove_text(cursor.line, 0, cursor.line, text[cursor.line].length());
			backspace_at_cursor();
			cursor_set_line(cursor.line + 1);
		}

		update();
		cut_copy_line = clipboard;

	} else {
		String clipboard = _base_get_text(selection.from_line, selection.from_column, selection.to_line, selection.to_column);
		OS::get_singleton()->set_clipboard(clipboard);

		_remove_text(selection.from_line, selection.from_column, selection.to_line, selection.to_column);
		cursor_set_line(selection.from_line, false); // Set afterwards else it causes the view to be offset.
		cursor_set_column(selection.from_column);

		selection.active = false;
		selection.selecting_mode = Selection::MODE_NONE;
		update();
		cut_copy_line = "";
	}
}

void TextEdit::copy() {
	if (!selection.active) {
		if (text[cursor.line].length() != 0) {
			String clipboard = _base_get_text(cursor.line, 0, cursor.line, text[cursor.line].length());
			OS::get_singleton()->set_clipboard(clipboard);
			cut_copy_line = clipboard;
		}
	} else {
		String clipboard = _base_get_text(selection.from_line, selection.from_column, selection.to_line, selection.to_column);
		OS::get_singleton()->set_clipboard(clipboard);
		cut_copy_line = "";
	}
}

void TextEdit::paste() {
	String clipboard = OS::get_singleton()->get_clipboard();

	begin_complex_operation();
	if (selection.active) {
		selection.active = false;
		selection.selecting_mode = Selection::MODE_NONE;
		_remove_text(selection.from_line, selection.from_column, selection.to_line, selection.to_column);
		cursor_set_line(selection.from_line, false);
		cursor_set_column(selection.from_column);

	} else if (!cut_copy_line.empty() && cut_copy_line == clipboard) {
		cursor_set_column(0);
		String ins = "\n";
		clipboard += ins;
	}

	_insert_text_at_cursor(clipboard);
	end_complex_operation();

	update();
}

void TextEdit::select_all() {
	if (!selecting_enabled) {
		return;
	}

	if (text.size() == 1 && text[0].length() == 0) {
		return;
	}
	selection.active = true;
	selection.from_line = 0;
	selection.from_column = 0;
	selection.selecting_line = 0;
	selection.selecting_column = 0;
	selection.to_line = text.size() - 1;
	selection.to_column = text[selection.to_line].length();
	selection.selecting_mode = Selection::MODE_SHIFT;
	selection.shiftclick_left = true;
	cursor_set_line(selection.to_line, false);
	cursor_set_column(selection.to_column, false);
	update();
}

void TextEdit::deselect() {
	selection.active = false;
	update();
}

void TextEdit::select(int p_from_line, int p_from_column, int p_to_line, int p_to_column) {
	if (!selecting_enabled) {
		return;
	}

	if (p_from_line < 0) {
		p_from_line = 0;
	} else if (p_from_line >= text.size()) {
		p_from_line = text.size() - 1;
	}
	if (p_from_column >= text[p_from_line].length()) {
		p_from_column = text[p_from_line].length();
	}
	if (p_from_column < 0) {
		p_from_column = 0;
	}

	if (p_to_line < 0) {
		p_to_line = 0;
	} else if (p_to_line >= text.size()) {
		p_to_line = text.size() - 1;
	}
	if (p_to_column >= text[p_to_line].length()) {
		p_to_column = text[p_to_line].length();
	}
	if (p_to_column < 0) {
		p_to_column = 0;
	}

	selection.from_line = p_from_line;
	selection.from_column = p_from_column;
	selection.to_line = p_to_line;
	selection.to_column = p_to_column;

	selection.active = true;

	if (selection.from_line == selection.to_line) {
		if (selection.from_column == selection.to_column) {
			selection.active = false;

		} else if (selection.from_column > selection.to_column) {
			selection.shiftclick_left = false;
			SWAP(selection.from_column, selection.to_column);
		} else {
			selection.shiftclick_left = true;
		}
	} else if (selection.from_line > selection.to_line) {
		selection.shiftclick_left = false;
		SWAP(selection.from_line, selection.to_line);
		SWAP(selection.from_column, selection.to_column);
	} else {
		selection.shiftclick_left = true;
	}

	update();
}
void TextEdit::swap_lines(int line1, int line2) {
	String tmp = get_line(line1);
	String tmp2 = get_line(line2);
	set_line(line2, tmp);
	set_line(line1, tmp2);
}
bool TextEdit::is_selection_active() const {
	return selection.active;
}
int TextEdit::get_selection_from_line() const {
	ERR_FAIL_COND_V(!selection.active, -1);
	return selection.from_line;
}
int TextEdit::get_selection_from_column() const {
	ERR_FAIL_COND_V(!selection.active, -1);
	return selection.from_column;
}
int TextEdit::get_selection_to_line() const {
	ERR_FAIL_COND_V(!selection.active, -1);
	return selection.to_line;
}
int TextEdit::get_selection_to_column() const {
	ERR_FAIL_COND_V(!selection.active, -1);
	return selection.to_column;
}

String TextEdit::get_selection_text() const {
	if (!selection.active) {
		return "";
	}

	return _base_get_text(selection.from_line, selection.from_column, selection.to_line, selection.to_column);
}

String TextEdit::get_word_under_cursor() const {
	int prev_cc = cursor.column;
	while (prev_cc > 0) {
		bool is_char = _is_text_char(text[cursor.line][prev_cc - 1]);
		if (!is_char) {
			break;
		}
		--prev_cc;
	}

	int next_cc = cursor.column;
	while (next_cc < text[cursor.line].length()) {
		bool is_char = _is_text_char(text[cursor.line][next_cc]);
		if (!is_char) {
			break;
		}
		++next_cc;
	}
	if (prev_cc == cursor.column || next_cc == cursor.column) {
		return "";
	}
	return text[cursor.line].substr(prev_cc, next_cc - prev_cc);
}

void TextEdit::set_search_text(const String &p_search_text) {
	search_text = p_search_text;
}

void TextEdit::set_search_flags(uint32_t p_flags) {
	search_flags = p_flags;
}

void TextEdit::set_current_search_result(int line, int col) {
	search_result_line = line;
	search_result_col = col;
	update();
}

void TextEdit::set_highlight_all_occurrences(const bool p_enabled) {
	highlight_all_occurrences = p_enabled;
	update();
}

bool TextEdit::is_highlight_all_occurrences_enabled() const {
	return highlight_all_occurrences;
}

int TextEdit::_get_column_pos_of_word(const String &p_key, const String &p_search, uint32_t p_search_flags, int p_from_column) {
	int col = -1;

	if (p_key.length() > 0 && p_search.length() > 0) {
		if (p_from_column < 0 || p_from_column > p_search.length()) {
			p_from_column = 0;
		}

		while (col == -1 && p_from_column <= p_search.length()) {
			if (p_search_flags & SEARCH_MATCH_CASE) {
				col = p_search.find(p_key, p_from_column);
			} else {
				col = p_search.findn(p_key, p_from_column);
			}

			// Whole words only.
			if (col != -1 && p_search_flags & SEARCH_WHOLE_WORDS) {
				p_from_column = col;

				if (col > 0 && _is_text_char(p_search[col - 1])) {
					col = -1;
				} else if ((col + p_key.length()) < p_search.length() && _is_text_char(p_search[col + p_key.length()])) {
					col = -1;
				}
			}

			p_from_column += 1;
		}
	}
	return col;
}

PoolVector<int> TextEdit::_search_bind(const String &p_key, uint32_t p_search_flags, int p_from_line, int p_from_column) const {
	int col, line;
	if (search(p_key, p_search_flags, p_from_line, p_from_column, line, col)) {
		PoolVector<int> result;
		result.resize(2);
		result.set(SEARCH_RESULT_COLUMN, col);
		result.set(SEARCH_RESULT_LINE, line);
		return result;

	} else {
		return PoolVector<int>();
	}
}

bool TextEdit::search(const String &p_key, uint32_t p_search_flags, int p_from_line, int p_from_column, int &r_line, int &r_column) const {
	if (p_key.length() == 0) {
		return false;
	}
	ERR_FAIL_INDEX_V(p_from_line, text.size(), false);
	ERR_FAIL_INDEX_V(p_from_column, text[p_from_line].length() + 1, false);

	// Search through the whole document, but start by current line.

	int line = p_from_line;
	int pos = -1;

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
				if (pos > 0 && _is_text_char(text_line[pos - 1])) {
					is_match = false;
				} else if (pos + p_key.length() < text_line.length() && _is_text_char(text_line[pos + p_key.length()])) {
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

	if (pos == -1) {
		r_line = -1;
		r_column = -1;
		return false;
	}

	r_line = line;
	r_column = pos;

	return true;
}

void TextEdit::_cursor_changed_emit() {
	emit_signal("cursor_changed");
	cursor_changed_dirty = false;
}

void TextEdit::_text_changed_emit() {
	emit_signal("text_changed");
	text_changed_dirty = false;
}

void TextEdit::set_line_as_marked(int p_line, bool p_marked) {
	ERR_FAIL_INDEX(p_line, text.size());
	text.set_marked(p_line, p_marked);
	update();
}

void TextEdit::set_line_as_safe(int p_line, bool p_safe) {
	ERR_FAIL_INDEX(p_line, text.size());
	text.set_safe(p_line, p_safe);
	update();
}

bool TextEdit::is_line_set_as_safe(int p_line) const {
	ERR_FAIL_INDEX_V(p_line, text.size(), false);
	return text.is_safe(p_line);
}

void TextEdit::set_executing_line(int p_line) {
	ERR_FAIL_INDEX(p_line, text.size());
	executing_line = p_line;
	update();
}

void TextEdit::clear_executing_line() {
	executing_line = -1;
	update();
}

void TextEdit::set_caret_on_readonly(bool p_value) {
	caret_on_readonly = p_value;
}

bool TextEdit::is_line_set_as_bookmark(int p_line) const {
	ERR_FAIL_INDEX_V(p_line, text.size(), false);
	return text.is_bookmark(p_line);
}

void TextEdit::set_line_as_bookmark(int p_line, bool p_bookmark) {
	ERR_FAIL_INDEX(p_line, text.size());
	text.set_bookmark(p_line, p_bookmark);
	update();
}

void TextEdit::get_bookmarks(List<int> *p_bookmarks) const {
	for (int i = 0; i < text.size(); i++) {
		if (text.is_bookmark(i)) {
			p_bookmarks->push_back(i);
		}
	}
}

Array TextEdit::get_bookmarks_array() const {
	Array arr;
	for (int i = 0; i < text.size(); i++) {
		if (text.is_bookmark(i)) {
			arr.append(i);
		}
	}
	return arr;
}

bool TextEdit::is_line_set_as_breakpoint(int p_line) const {
	ERR_FAIL_INDEX_V(p_line, text.size(), false);
	return text.is_breakpoint(p_line);
}

void TextEdit::set_line_as_breakpoint(int p_line, bool p_breakpoint) {
	ERR_FAIL_INDEX(p_line, text.size());
	text.set_breakpoint(p_line, p_breakpoint);
	update();
}

void TextEdit::get_breakpoints(List<int> *p_breakpoints) const {
	for (int i = 0; i < text.size(); i++) {
		if (text.is_breakpoint(i)) {
			p_breakpoints->push_back(i);
		}
	}
}

Array TextEdit::get_breakpoints_array() const {
	Array arr;
	for (int i = 0; i < text.size(); i++) {
		if (text.is_breakpoint(i)) {
			arr.append(i);
		}
	}
	return arr;
}

void TextEdit::remove_breakpoints() {
	for (int i = 0; i < text.size(); i++) {
		if (text.is_breakpoint(i)) {
			/* Should "breakpoint_toggled" be fired when breakpoints are removed this way? */
			text.set_breakpoint(i, false);
		}
	}
}

void TextEdit::set_line_info_icon(int p_line, Ref<Texture> p_icon, String p_info) {
	ERR_FAIL_INDEX(p_line, text.size());
	text.set_info_icon(p_line, p_icon, p_info);
	update();
}

void TextEdit::clear_info_icons() {
	text.clear_info_icons();
	update();
}

void TextEdit::set_line_as_hidden(int p_line, bool p_hidden) {
	ERR_FAIL_INDEX(p_line, text.size());
	if (is_hiding_enabled() || !p_hidden) {
		text.set_hidden(p_line, p_hidden);
	}
	update();
}

bool TextEdit::is_line_hidden(int p_line) const {
	ERR_FAIL_INDEX_V(p_line, text.size(), false);
	return text.is_hidden(p_line);
}

void TextEdit::fold_all_lines() {
	for (int i = 0; i < text.size(); i++) {
		fold_line(i);
	}
	_update_scrollbars();
	update();
}

void TextEdit::unhide_all_lines() {
	for (int i = 0; i < text.size(); i++) {
		text.set_hidden(i, false);
	}
	_update_scrollbars();
	update();
}

int TextEdit::num_lines_from(int p_line_from, int visible_amount) const {
	// Returns the number of lines (hidden and unhidden) from p_line_from to (p_line_from + visible_amount of unhidden lines).
	ERR_FAIL_INDEX_V(p_line_from, text.size(), ABS(visible_amount));

	if (!is_hiding_enabled()) {
		return ABS(visible_amount);
	}

	int num_visible = 0;
	int num_total = 0;
	if (visible_amount >= 0) {
		for (int i = p_line_from; i < text.size(); i++) {
			num_total++;
			if (!is_line_hidden(i)) {
				num_visible++;
			}
			if (num_visible >= visible_amount) {
				break;
			}
		}
	} else {
		visible_amount = ABS(visible_amount);
		for (int i = p_line_from; i >= 0; i--) {
			num_total++;
			if (!is_line_hidden(i)) {
				num_visible++;
			}
			if (num_visible >= visible_amount) {
				break;
			}
		}
	}
	return num_total;
}

int TextEdit::num_lines_from_rows(int p_line_from, int p_wrap_index_from, int visible_amount, int &wrap_index) const {
	// Returns the number of lines (hidden and unhidden) from (p_line_from + p_wrap_index_from) row to (p_line_from + visible_amount of unhidden and wrapped rows).
	// Wrap index is set to the wrap index of the last line.
	wrap_index = 0;
	ERR_FAIL_INDEX_V(p_line_from, text.size(), ABS(visible_amount));

	if (!is_hiding_enabled() && !is_wrap_enabled()) {
		return ABS(visible_amount);
	}

	int num_visible = 0;
	int num_total = 0;
	if (visible_amount == 0) {
		num_total = 0;
		wrap_index = 0;
	} else if (visible_amount > 0) {
		int i;
		num_visible -= p_wrap_index_from;
		for (i = p_line_from; i < text.size(); i++) {
			num_total++;
			if (!is_line_hidden(i)) {
				num_visible++;
				num_visible += times_line_wraps(i);
			}
			if (num_visible >= visible_amount) {
				break;
			}
		}
		wrap_index = times_line_wraps(MIN(i, text.size() - 1)) - (num_visible - visible_amount);
	} else {
		visible_amount = ABS(visible_amount);
		int i;
		num_visible -= times_line_wraps(p_line_from) - p_wrap_index_from;
		for (i = p_line_from; i >= 0; i--) {
			num_total++;
			if (!is_line_hidden(i)) {
				num_visible++;
				num_visible += times_line_wraps(i);
			}
			if (num_visible >= visible_amount) {
				break;
			}
		}
		wrap_index = (num_visible - visible_amount);
	}
	wrap_index = MAX(wrap_index, 0);
	return num_total;
}

int TextEdit::get_last_unhidden_line() const {
	// Returns the last line in the text that is not hidden.
	if (!is_hiding_enabled()) {
		return text.size() - 1;
	}

	int last_line;
	for (last_line = text.size() - 1; last_line > 0; last_line--) {
		if (!is_line_hidden(last_line)) {
			break;
		}
	}
	return last_line;
}

int TextEdit::get_indent_level(int p_line) const {
	ERR_FAIL_INDEX_V(p_line, text.size(), 0);

	// Counts number of tabs and spaces before line starts.
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
	return tab_count * indent_size + whitespace_count;
}

bool TextEdit::is_line_comment(int p_line) const {
	// Checks to see if this line is the start of a comment.
	ERR_FAIL_INDEX_V(p_line, text.size(), false);

	const Map<int, Text::ColorRegionInfo> &cri_map = text.get_color_region_info(p_line);

	int line_length = text[p_line].size();
	for (int i = 0; i < line_length - 1; i++) {
		if (_is_symbol(text[p_line][i]) && cri_map.has(i)) {
			const Text::ColorRegionInfo &cri = cri_map[i];
			return color_regions[cri.region].begin_key == "#" || color_regions[cri.region].begin_key == "//";
		} else if (_is_whitespace(text[p_line][i])) {
			continue;
		} else {
			break;
		}
	}
	return false;
}

bool TextEdit::is_caret_on_readonly() {
	return caret_on_readonly;
}

bool TextEdit::can_fold(int p_line) const {
	ERR_FAIL_INDEX_V(p_line, text.size(), false);
	if (!is_hiding_enabled()) {
		return false;
	}
	if (p_line + 1 >= text.size()) {
		return false;
	}
	if (text[p_line].strip_edges().size() == 0) {
		return false;
	}
	if (is_folded(p_line)) {
		return false;
	}
	if (is_line_hidden(p_line)) {
		return false;
	}
	if (is_line_comment(p_line)) {
		return false;
	}

	int start_indent = get_indent_level(p_line);

	for (int i = p_line + 1; i < text.size(); i++) {
		if (text[i].strip_edges().size() == 0) {
			continue;
		}
		int next_indent = get_indent_level(i);
		if (is_line_comment(i)) {
			continue;
		} else if (next_indent > start_indent) {
			return true;
		} else {
			return false;
		}
	}

	return false;
}

bool TextEdit::is_folded(int p_line) const {
	ERR_FAIL_INDEX_V(p_line, text.size(), false);
	if (p_line + 1 >= text.size()) {
		return false;
	}
	return !is_line_hidden(p_line) && is_line_hidden(p_line + 1);
}

Vector<int> TextEdit::get_folded_lines() const {
	Vector<int> folded_lines;

	for (int i = 0; i < text.size(); i++) {
		if (is_folded(i)) {
			folded_lines.push_back(i);
		}
	}
	return folded_lines;
}

void TextEdit::fold_line(int p_line) {
	ERR_FAIL_INDEX(p_line, text.size());
	if (!is_hiding_enabled()) {
		return;
	}
	if (!can_fold(p_line)) {
		return;
	}

	// Hide lines below this one.
	int start_indent = get_indent_level(p_line);
	int last_line = start_indent;
	for (int i = p_line + 1; i < text.size(); i++) {
		if (text[i].strip_edges().size() != 0) {
			if (is_line_comment(i) && get_indent_level(i) <= start_indent) {
				// Checked indent to make sure indented comments that finish a code block are folded.
				continue;
			} else if (get_indent_level(i) > start_indent) {
				last_line = i;
			} else {
				break;
			}
		}
	}
	for (int i = p_line + 1; i <= last_line; i++) {
		set_line_as_hidden(i, true);
	}

	// Fix selection.
	if (is_selection_active()) {
		if (is_line_hidden(selection.from_line) && is_line_hidden(selection.to_line)) {
			deselect();
		} else if (is_line_hidden(selection.from_line)) {
			select(p_line, 9999, selection.to_line, selection.to_column);
		} else if (is_line_hidden(selection.to_line)) {
			select(selection.from_line, selection.from_column, p_line, 9999);
		}
	}

	// Reset cursor.
	if (is_line_hidden(cursor.line)) {
		cursor_set_line(p_line, false, false);
		cursor_set_column(get_line(p_line).length(), false);
	}
	_update_scrollbars();
	update();
}

void TextEdit::unfold_line(int p_line) {
	ERR_FAIL_INDEX(p_line, text.size());

	if (!is_folded(p_line) && !is_line_hidden(p_line)) {
		return;
	}
	int fold_start;
	for (fold_start = p_line; fold_start > 0; fold_start--) {
		if (is_folded(fold_start)) {
			break;
		}
	}
	fold_start = is_folded(fold_start) ? fold_start : p_line;

	for (int i = fold_start + 1; i < text.size(); i++) {
		if (is_line_hidden(i)) {
			set_line_as_hidden(i, false);
		} else {
			break;
		}
	}
	_update_scrollbars();
	update();
}

void TextEdit::toggle_fold_line(int p_line) {
	ERR_FAIL_INDEX(p_line, text.size());

	if (!is_folded(p_line)) {
		fold_line(p_line);
	} else {
		unfold_line(p_line);
	}
}

int TextEdit::get_line_count() const {
	return text.size();
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
	_push_current_op();

	if (undo_stack_pos == nullptr) {
		if (!undo_stack.size()) {
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

	if (op.type != TextOperation::TYPE_INSERT && (op.from_line != op.to_line || op.to_column != op.from_column + 1)) {
		select(op.from_line, op.from_column, op.to_line, op.to_column);
	}

	_update_scrollbars();
	if (undo_stack_pos->get().type == TextOperation::TYPE_REMOVE) {
		cursor_set_line(undo_stack_pos->get().to_line, false);
		cursor_set_column(undo_stack_pos->get().to_column);
		_cancel_code_hint();
	} else {
		cursor_set_line(undo_stack_pos->get().from_line, false);
		cursor_set_column(undo_stack_pos->get().from_column);
	}
	update();
}

void TextEdit::redo() {
	_push_current_op();

	if (undo_stack_pos == nullptr) {
		return; // Nothing to do.
	}

	deselect();

	TextOperation op = undo_stack_pos->get();
	_do_text_op(op, false);
	current_op.version = op.version;
	if (undo_stack_pos->get().chain_forward) {
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
	cursor_set_line(undo_stack_pos->get().to_line, false);
	cursor_set_column(undo_stack_pos->get().to_column);
	undo_stack_pos = undo_stack_pos->next();
	update();
}

void TextEdit::clear_undo_history() {
	saved_version = 0;
	current_op.type = TextOperation::TYPE_NONE;
	undo_stack_pos = nullptr;
	undo_stack.clear();
}

void TextEdit::begin_complex_operation() {
	_push_current_op();
	next_operation_is_complex = true;
}

void TextEdit::end_complex_operation() {
	_push_current_op();
	ERR_FAIL_COND(undo_stack.size() == 0);

	if (undo_stack.back()->get().chain_forward) {
		undo_stack.back()->get().chain_forward = false;
		return;
	}

	undo_stack.back()->get().chain_backward = true;
}

void TextEdit::_push_current_op() {
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

void TextEdit::set_indent_using_spaces(const bool p_use_spaces) {
	indent_using_spaces = p_use_spaces;
}

bool TextEdit::is_indent_using_spaces() const {
	return indent_using_spaces;
}

void TextEdit::set_indent_size(const int p_size) {
	ERR_FAIL_COND_MSG(p_size <= 0, "Indend size must be greater than 0.");
	indent_size = p_size;
	text.set_indent_size(p_size);

	space_indent = "";
	for (int i = 0; i < p_size; i++) {
		space_indent += " ";
	}

	update();
}

int TextEdit::get_indent_size() {
	return indent_size;
}

void TextEdit::set_draw_tabs(bool p_draw) {
	draw_tabs = p_draw;
	update();
}

bool TextEdit::is_drawing_tabs() const {
	return draw_tabs;
}

void TextEdit::set_draw_spaces(bool p_draw) {
	draw_spaces = p_draw;
}

bool TextEdit::is_drawing_spaces() const {
	return draw_spaces;
}

void TextEdit::set_override_selected_font_color(bool p_override_selected_font_color) {
	override_selected_font_color = p_override_selected_font_color;
}

bool TextEdit::is_overriding_selected_font_color() const {
	return override_selected_font_color;
}

void TextEdit::set_insert_mode(bool p_enabled) {
	insert_mode = p_enabled;
	update();
}

bool TextEdit::is_insert_mode() const {
	return insert_mode;
}

bool TextEdit::is_insert_text_operation() {
	return (current_op.type == TextOperation::TYPE_INSERT);
}

uint32_t TextEdit::get_version() const {
	return current_op.version;
}

uint32_t TextEdit::get_saved_version() const {
	return saved_version;
}

void TextEdit::tag_saved_version() {
	saved_version = get_version();
}

double TextEdit::get_scroll_pos_for_line(int p_line, int p_wrap_index) const {
	if (!is_wrap_enabled() && !is_hiding_enabled()) {
		return p_line;
	}

	// Count the number of visible lines up to this line.
	double new_line_scroll_pos = 0;
	int to = CLAMP(p_line, 0, text.size() - 1);
	for (int i = 0; i < to; i++) {
		if (!text.is_hidden(i)) {
			new_line_scroll_pos++;
			new_line_scroll_pos += times_line_wraps(i);
		}
	}
	new_line_scroll_pos += p_wrap_index;
	return new_line_scroll_pos;
}

void TextEdit::set_line_as_first_visible(int p_line, int p_wrap_index) {
	set_v_scroll(get_scroll_pos_for_line(p_line, p_wrap_index));
}

void TextEdit::set_line_as_center_visible(int p_line, int p_wrap_index) {
	int visible_rows = get_visible_rows();
	int wi;
	int first_line = p_line - num_lines_from_rows(p_line, p_wrap_index, -visible_rows / 2, wi) + 1;

	set_v_scroll(get_scroll_pos_for_line(first_line, wi));
}

void TextEdit::set_line_as_last_visible(int p_line, int p_wrap_index) {
	int wi;
	int first_line = p_line - num_lines_from_rows(p_line, p_wrap_index, -get_visible_rows() - 1, wi) + 1;

	set_v_scroll(get_scroll_pos_for_line(first_line, wi) + get_visible_rows_offset());
}

int TextEdit::get_first_visible_line() const {
	return CLAMP(cursor.line_ofs, 0, text.size() - 1);
}

int TextEdit::get_last_full_visible_line() const {
	int first_vis_line = get_first_visible_line();
	int last_vis_line = 0;
	int wi;
	last_vis_line = first_vis_line + num_lines_from_rows(first_vis_line, cursor.wrap_ofs, get_visible_rows(), wi) - 1;
	last_vis_line = CLAMP(last_vis_line, 0, text.size() - 1);
	return last_vis_line;
}

int TextEdit::get_last_full_visible_line_wrap_index() const {
	int first_vis_line = get_first_visible_line();
	int wi;
	num_lines_from_rows(first_vis_line, cursor.wrap_ofs, get_visible_rows(), wi);
	return wi;
}

double TextEdit::get_visible_rows_offset() const {
	double total = _get_control_height();
	total /= (double)get_row_height();
	total = total - floor(total);
	total = -CLAMP(total, 0.001, 1) + 1;
	return total;
}

double TextEdit::get_v_scroll_offset() const {
	double val = get_v_scroll() - floor(get_v_scroll());
	return CLAMP(val, 0, 1);
}

double TextEdit::get_v_scroll() const {
	return v_scroll->get_value();
}

void TextEdit::set_v_scroll(double p_scroll) {
	v_scroll->set_value(p_scroll);
	int max_v_scroll = v_scroll->get_max() - v_scroll->get_page();
	if (p_scroll >= max_v_scroll - 1.0) {
		_scroll_moved(v_scroll->get_value());
	}
}

int TextEdit::get_h_scroll() const {
	return h_scroll->get_value();
}

void TextEdit::set_h_scroll(int p_scroll) {
	if (p_scroll < 0) {
		p_scroll = 0;
	}
	h_scroll->set_value(p_scroll);
}

void TextEdit::set_smooth_scroll_enabled(bool p_enable) {
	v_scroll->set_smooth_scroll_enabled(p_enable);
	smooth_scroll_enabled = p_enable;
}

bool TextEdit::is_smooth_scroll_enabled() const {
	return smooth_scroll_enabled;
}

void TextEdit::set_v_scroll_speed(float p_speed) {
	v_scroll_speed = p_speed;
}

float TextEdit::get_v_scroll_speed() const {
	return v_scroll_speed;
}

void TextEdit::set_completion(bool p_enabled, const Vector<String> &p_prefixes) {
	completion_prefixes.clear();
	completion_enabled = p_enabled;
	for (int i = 0; i < p_prefixes.size(); i++) {
		completion_prefixes.insert(p_prefixes[i]);
	}
}

void TextEdit::_confirm_completion() {
	begin_complex_operation();

	_remove_text(cursor.line, cursor.column - completion_base.length(), cursor.line, cursor.column);
	cursor_set_column(cursor.column - completion_base.length(), false);
	insert_text_at_cursor(completion_current.insert_text);

	// When inserted into the middle of an existing string/method, don't add an unnecessary quote/bracket.
	String line = text[cursor.line];
	CharType next_char = line[cursor.column];
	CharType last_completion_char = completion_current.insert_text[completion_current.insert_text.length() - 1];
	CharType last_completion_char_display = completion_current.display[completion_current.display.length() - 1];

	if ((last_completion_char == '"' || last_completion_char == '\'') && (last_completion_char == next_char || last_completion_char_display == next_char)) {
		_remove_text(cursor.line, cursor.column, cursor.line, cursor.column + 1);
	}

	if (last_completion_char == '(') {
		if (next_char == last_completion_char) {
			_remove_text(cursor.line, cursor.column - 1, cursor.line, cursor.column);
		} else if (auto_brace_completion_enabled) {
			insert_text_at_cursor(")");
			cursor.column--;
		}
	} else if (last_completion_char == ')' && next_char == '(') {
		_remove_text(cursor.line, cursor.column - 2, cursor.line, cursor.column);
		if (line[cursor.column + 1] != ')') {
			cursor.column--;
		}
	}

	end_complex_operation();

	_cancel_completion();

	if (last_completion_char == '(') {
		query_code_comple();
	}
}

void TextEdit::_cancel_code_hint() {
	completion_hint = "";
	update();
}

void TextEdit::_cancel_completion() {
	if (!completion_active) {
		return;
	}

	completion_active = false;
	completion_forced = false;
	update();
}

static bool _is_completable(CharType c) {
	return !_is_symbol(c) || c == '"' || c == '\'';
}

void TextEdit::_update_completion_candidates() {
	String l = text[cursor.line];
	int cofs = CLAMP(cursor.column, 0, l.length());

	String s;

	// Look for keywords first.

	bool inquote = false;
	int first_quote = -1;
	int restore_quotes = -1;

	int c = cofs - 1;
	while (c >= 0) {
		if (l[c] == '"' || l[c] == '\'') {
			inquote = !inquote;
			if (first_quote == -1) {
				first_quote = c;
			}
			restore_quotes = 0;
		} else if (restore_quotes == 0 && l[c] == '$') {
			restore_quotes = 1;
		} else if (restore_quotes == 0 && !_is_whitespace(l[c])) {
			restore_quotes = -1;
		}
		c--;
	}

	bool pre_keyword = false;
	bool cancel = false;

	if (!inquote && first_quote == cofs - 1) {
		// No completion here.
		cancel = true;
	} else if (inquote && first_quote != -1) {
		s = l.substr(first_quote, cofs - first_quote);
	} else if (cofs > 0 && l[cofs - 1] == ' ') {
		int kofs = cofs - 1;
		String kw;
		while (kofs >= 0 && l[kofs] == ' ') {
			kofs--;
		}

		while (kofs >= 0 && l[kofs] > 32 && _is_completable(l[kofs])) {
			kw = String::chr(l[kofs]) + kw;
			kofs--;
		}

		pre_keyword = keywords.has(kw);

	} else {
		while (cofs > 0 && l[cofs - 1] > 32 && (l[cofs - 1] == '/' || _is_completable(l[cofs - 1]))) {
			s = String::chr(l[cofs - 1]) + s;
			if (l[cofs - 1] == '\'' || l[cofs - 1] == '"' || l[cofs - 1] == '$') {
				break;
			}

			cofs--;
		}
	}

	if (cursor.column > 0 && l[cursor.column - 1] == '(' && !pre_keyword && !completion_forced) {
		cancel = true;
	}

	update();

	bool prev_is_prefix = false;
	if (cofs > 0 && completion_prefixes.has(String::chr(l[cofs - 1]))) {
		prev_is_prefix = true;
	}
	// Check with one space before prefix, to allow indent.
	if (cofs > 1 && l[cofs - 1] == ' ' && completion_prefixes.has(String::chr(l[cofs - 2]))) {
		prev_is_prefix = true;
	}

	if (cancel || (!pre_keyword && s == "" && (cofs == 0 || !prev_is_prefix))) {
		// None to complete, cancel.
		_cancel_completion();
		return;
	}

	completion_options.clear();
	completion_index = 0;
	completion_force_item_center = -1;
	completion_base = s;
	Vector<float> sim_cache;
	bool single_quote = s.begins_with("'");
	Vector<ScriptCodeCompletionOption> completion_options_casei;

	for (List<ScriptCodeCompletionOption>::Element *E = completion_sources.front(); E; E = E->next()) {
		ScriptCodeCompletionOption &option = E->get();

		if (single_quote && option.display.is_quoted()) {
			option.display = option.display.unquote().quote("'");
		}

		if (inquote && restore_quotes == 1 && !option.display.is_quoted()) {
			String quote = single_quote ? "'" : "\"";
			option.display = option.display.quote(quote);
			option.insert_text = option.insert_text.quote(quote);
		}

		if (option.display.begins_with(s)) {
			completion_options.push_back(option);
		} else if (option.display.to_lower().begins_with(s.to_lower())) {
			completion_options_casei.push_back(option);
		}
	}

	completion_options.append_array(completion_options_casei);

	if (completion_options.size() == 0) {
		for (int i = 0; i < completion_sources.size(); i++) {
			if (s.is_subsequence_of(completion_sources[i].display)) {
				completion_options.push_back(completion_sources[i]);
			}
		}
	}

	if (completion_options.size() == 0) {
		for (int i = 0; i < completion_sources.size(); i++) {
			if (s.is_subsequence_ofi(completion_sources[i].display)) {
				completion_options.push_back(completion_sources[i]);
			}
		}
	}

	if (completion_options.size() == 0) {
		// No options to complete, cancel.
		_cancel_completion();
		return;
	}

	if (completion_options.size() == 1 && s == completion_options[0].display) {
		// A perfect match, stop completion.
		_cancel_completion();
		return;
	}

	// The top of the list is the best match.
	completion_current = completion_options[0];
	completion_enabled = true;
}

void TextEdit::query_code_comple() {
	String l = text[cursor.line];
	int ofs = CLAMP(cursor.column, 0, l.length());

	bool inquote = false;

	int c = ofs - 1;
	while (c >= 0) {
		if (l[c] == '"' || l[c] == '\'') {
			inquote = !inquote;
		}
		c--;
	}

	bool ignored = completion_active && !completion_options.empty();
	if (ignored) {
		ScriptCodeCompletionOption::Kind kind = ScriptCodeCompletionOption::KIND_PLAIN_TEXT;
		const ScriptCodeCompletionOption *previous_option = nullptr;
		for (int i = 0; i < completion_options.size(); i++) {
			const ScriptCodeCompletionOption &current_option = completion_options[i];
			if (!previous_option) {
				previous_option = &current_option;
				kind = current_option.kind;
			}
			if (previous_option->kind != current_option.kind) {
				ignored = false;
				break;
			}
		}
		ignored = ignored && (kind == ScriptCodeCompletionOption::KIND_FILE_PATH || kind == ScriptCodeCompletionOption::KIND_NODE_PATH || kind == ScriptCodeCompletionOption::KIND_SIGNAL);
	}

	if (!ignored) {
		if (ofs > 0 && (inquote || _is_completable(l[ofs - 1]) || completion_prefixes.has(String::chr(l[ofs - 1])))) {
			emit_signal("request_completion");
		} else if (ofs > 1 && l[ofs - 1] == ' ' && completion_prefixes.has(String::chr(l[ofs - 2]))) { // Make it work with a space too, it's good enough.
			emit_signal("request_completion");
		}
	}
}

void TextEdit::set_code_hint(const String &p_hint) {
	completion_hint = p_hint;
	completion_hint_offset = -0xFFFF;
	update();
}

void TextEdit::code_complete(const List<ScriptCodeCompletionOption> &p_strings, bool p_forced) {
	completion_sources = p_strings;
	completion_active = true;
	completion_forced = p_forced;
	completion_current = ScriptCodeCompletionOption();
	completion_index = 0;
	completion_force_item_center = -1;
	_update_completion_candidates();
}

String TextEdit::get_word_at_pos(const Vector2 &p_pos) const {
	int row, col;
	_get_mouse_pos(p_pos, row, col);

	String s = text[row];
	if (s.length() == 0) {
		return "";
	}
	int beg, end;
	if (select_word(s, col, beg, end)) {
		bool inside_quotes = false;
		CharType selected_quote = '\0';
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

String TextEdit::get_tooltip(const Point2 &p_pos) const {
	Object *tooltip_obj = ObjectDB::get_instance(tooltip_obj_id);
	if (!tooltip_obj) {
		return Control::get_tooltip(p_pos);
	}
	int row, col;
	_get_mouse_pos(p_pos, row, col);

	String s = text[row];
	if (s.length() == 0) {
		return Control::get_tooltip(p_pos);
	}
	int beg, end;
	if (select_word(s, col, beg, end)) {
		String tt = tooltip_obj->call(tooltip_func, s.substr(beg, end - beg), tooltip_ud);

		return tt;
	}

	return Control::get_tooltip(p_pos);
}

void TextEdit::set_tooltip_request_func(Object *p_obj, const StringName &p_function, const Variant &p_udata) {
	ERR_FAIL_NULL(p_obj);
	tooltip_obj_id = p_obj->get_instance_id();
	tooltip_func = p_function;
	tooltip_ud = p_udata;
}

void TextEdit::set_line(int line, String new_text) {
	if (line < 0 || line >= text.size()) {
		return;
	}
	_remove_text(line, 0, line, text[line].length());
	_insert_text(line, 0, new_text);
	if (cursor.line == line) {
		cursor.column = MIN(cursor.column, new_text.length());
	}
	if (is_selection_active() && line == selection.to_line && selection.to_column > text[line].length()) {
		selection.to_column = text[line].length();
	}
}

void TextEdit::insert_at(const String &p_text, int at) {
	_insert_text(at, 0, p_text + "\n");
	if (cursor.line >= at) {
		// offset cursor when located after inserted line
		++cursor.line;
	}
	if (is_selection_active()) {
		if (selection.from_line >= at) {
			// offset selection when located after inserted line
			++selection.from_line;
			++selection.to_line;
		} else if (selection.to_line >= at) {
			// extend selection that includes inserted line
			++selection.to_line;
		}
	}
}

void TextEdit::set_show_line_numbers(bool p_show) {
	line_numbers = p_show;
	update();
}

void TextEdit::set_line_numbers_zero_padded(bool p_zero_padded) {
	line_numbers_zero_padded = p_zero_padded;
	update();
}

bool TextEdit::is_show_line_numbers_enabled() const {
	return line_numbers;
}

void TextEdit::set_show_line_length_guidelines(bool p_show) {
	line_length_guidelines = p_show;
	update();
}

void TextEdit::set_line_length_guideline_soft_column(int p_column) {
	line_length_guideline_soft_col = p_column;
	update();
}

void TextEdit::set_line_length_guideline_hard_column(int p_column) {
	line_length_guideline_hard_col = p_column;
	update();
}

void TextEdit::set_bookmark_gutter_enabled(bool p_draw) {
	draw_bookmark_gutter = p_draw;
	update();
}

bool TextEdit::is_bookmark_gutter_enabled() const {
	return draw_bookmark_gutter;
}

void TextEdit::set_breakpoint_gutter_enabled(bool p_draw) {
	draw_breakpoint_gutter = p_draw;
	update();
}

bool TextEdit::is_breakpoint_gutter_enabled() const {
	return draw_breakpoint_gutter;
}

void TextEdit::set_breakpoint_gutter_width(int p_gutter_width) {
	breakpoint_gutter_width = p_gutter_width;
	update();
}

int TextEdit::get_breakpoint_gutter_width() const {
	return cache.breakpoint_gutter_width;
}

void TextEdit::set_draw_fold_gutter(bool p_draw) {
	draw_fold_gutter = p_draw;
	update();
}

bool TextEdit::is_drawing_fold_gutter() const {
	return draw_fold_gutter;
}

void TextEdit::set_fold_gutter_width(int p_gutter_width) {
	fold_gutter_width = p_gutter_width;
	update();
}

int TextEdit::get_fold_gutter_width() const {
	return cache.fold_gutter_width;
}

void TextEdit::set_draw_info_gutter(bool p_draw) {
	draw_info_gutter = p_draw;
	update();
}

bool TextEdit::is_drawing_info_gutter() const {
	return draw_info_gutter;
}

void TextEdit::set_info_gutter_width(int p_gutter_width) {
	info_gutter_width = p_gutter_width;
	update();
}

int TextEdit::get_info_gutter_width() const {
	return info_gutter_width;
}

int TextEdit::get_total_gutter_width() const {
	return cache.line_number_w + cache.breakpoint_gutter_width + cache.fold_gutter_width + cache.info_gutter_width;
}

void TextEdit::set_draw_minimap(bool p_draw) {
	draw_minimap = p_draw;
	update();
}

bool TextEdit::is_drawing_minimap() const {
	return draw_minimap;
}

void TextEdit::set_minimap_width(int p_minimap_width) {
	minimap_width = p_minimap_width;
	update();
}

int TextEdit::get_minimap_width() const {
	return minimap_width;
}

void TextEdit::set_hiding_enabled(bool p_enabled) {
	if (!p_enabled) {
		unhide_all_lines();
	}
	hiding_enabled = p_enabled;
	update();
}

bool TextEdit::is_hiding_enabled() const {
	return hiding_enabled;
}

void TextEdit::set_highlight_current_line(bool p_enabled) {
	highlight_current_line = p_enabled;
	update();
}

bool TextEdit::is_highlight_current_line_enabled() const {
	return highlight_current_line;
}

bool TextEdit::is_text_field() const {
	return true;
}

void TextEdit::menu_option(int p_option) {
	switch (p_option) {
		case MENU_CUT: {
			if (!readonly) {
				cut();
			}
		} break;
		case MENU_COPY: {
			copy();
		} break;
		case MENU_PASTE: {
			if (!readonly) {
				paste();
			}
		} break;
		case MENU_CLEAR: {
			if (!readonly) {
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
		}
	}
}

void TextEdit::set_select_identifiers_on_hover(bool p_enable) {
	select_identifiers_enabled = p_enable;
}

bool TextEdit::is_selecting_identifiers_on_hover_enabled() const {
	return select_identifiers_enabled;
}

void TextEdit::set_context_menu_enabled(bool p_enable) {
	context_menu_enabled = p_enable;
}

bool TextEdit::is_context_menu_enabled() {
	return context_menu_enabled;
}

void TextEdit::set_shortcut_keys_enabled(bool p_enabled) {
	shortcut_keys_enabled = p_enabled;

	_generate_context_menu();
}

void TextEdit::set_virtual_keyboard_enabled(bool p_enable) {
	virtual_keyboard_enabled = p_enable;
}

void TextEdit::set_middle_mouse_paste_enabled(bool p_enabled) {
	middle_mouse_paste_enabled = p_enabled;
}

void TextEdit::set_selecting_enabled(bool p_enabled) {
	selecting_enabled = p_enabled;

	if (!selecting_enabled) {
		deselect();
	}

	_generate_context_menu();
}

bool TextEdit::is_selecting_enabled() const {
	return selecting_enabled;
}

void TextEdit::set_deselect_on_focus_loss_enabled(const bool p_enabled) {
	deselect_on_focus_loss_enabled = p_enabled;
	if (p_enabled && selection.active && !has_focus()) {
		deselect();
	}
}

bool TextEdit::is_deselect_on_focus_loss_enabled() const {
	return deselect_on_focus_loss_enabled;
}

void TextEdit::set_drag_and_drop_selection_enabled(const bool p_enabled) {
	drag_and_drop_selection_enabled = p_enabled;
}

bool TextEdit::is_drag_and_drop_selection_enabled() const {
	return drag_and_drop_selection_enabled;
}

bool TextEdit::is_shortcut_keys_enabled() const {
	return shortcut_keys_enabled;
}

bool TextEdit::is_virtual_keyboard_enabled() const {
	return virtual_keyboard_enabled;
}

bool TextEdit::is_middle_mouse_paste_enabled() const {
	return middle_mouse_paste_enabled;
}

PopupMenu *TextEdit::get_menu() const {
	return menu;
}

int TextEdit::get_line_width(int p_line, int p_wrap_index) const {
	ERR_FAIL_INDEX_V(p_line, text.size(), 0);

	if (p_wrap_index >= 0 && line_wraps(p_line)) {
		Vector<String> rows = get_wrap_rows_text(p_line);
		ERR_FAIL_INDEX_V(p_wrap_index, rows.size(), 0);

		int w = 0;
		int len = rows[p_wrap_index].length();
		const CharType *str = rows[p_wrap_index].c_str();
		for (int i = 0; i < len; i++) {
			w += text.get_char_width(str[i], str[i + 1], w);
		}

		return w;
	}

	return text.get_line_width(p_line);
}

int TextEdit::get_line_height() const {
	return get_row_height();
}

void TextEdit::_bind_methods() {
	ClassDB::bind_method(D_METHOD("_gui_input"), &TextEdit::_gui_input);
	ClassDB::bind_method(D_METHOD("_scroll_moved"), &TextEdit::_scroll_moved);
	ClassDB::bind_method(D_METHOD("_cursor_changed_emit"), &TextEdit::_cursor_changed_emit);
	ClassDB::bind_method(D_METHOD("_text_changed_emit"), &TextEdit::_text_changed_emit);
	ClassDB::bind_method(D_METHOD("_push_current_op"), &TextEdit::_push_current_op);
	ClassDB::bind_method(D_METHOD("_click_selection_held"), &TextEdit::_click_selection_held);
	ClassDB::bind_method(D_METHOD("_toggle_draw_caret"), &TextEdit::_toggle_draw_caret);
	ClassDB::bind_method(D_METHOD("_v_scroll_input"), &TextEdit::_v_scroll_input);
	ClassDB::bind_method(D_METHOD("_update_wrap_at"), &TextEdit::_update_wrap_at);

	BIND_ENUM_CONSTANT(SEARCH_MATCH_CASE);
	BIND_ENUM_CONSTANT(SEARCH_WHOLE_WORDS);
	BIND_ENUM_CONSTANT(SEARCH_BACKWARDS);

	BIND_ENUM_CONSTANT(SEARCH_RESULT_COLUMN);
	BIND_ENUM_CONSTANT(SEARCH_RESULT_LINE);

	/*
	ClassDB::bind_method(D_METHOD("delete_char"),&TextEdit::delete_char);
	ClassDB::bind_method(D_METHOD("delete_line"),&TextEdit::delete_line);
*/

	ClassDB::bind_method(D_METHOD("set_text", "text"), &TextEdit::set_text);
	ClassDB::bind_method(D_METHOD("insert_text_at_cursor", "text"), &TextEdit::insert_text_at_cursor);

	ClassDB::bind_method(D_METHOD("get_line_count"), &TextEdit::get_line_count);
	ClassDB::bind_method(D_METHOD("get_text"), &TextEdit::get_text);
	ClassDB::bind_method(D_METHOD("get_line", "line"), &TextEdit::get_line);
	ClassDB::bind_method(D_METHOD("set_line", "line", "new_text"), &TextEdit::set_line);
	ClassDB::bind_method(D_METHOD("get_line_wrapped_text", "line"), &TextEdit::get_wrap_rows_text);

	ClassDB::bind_method(D_METHOD("get_line_width", "line", "wrap_index"), &TextEdit::get_line_width, DEFVAL(-1));
	ClassDB::bind_method(D_METHOD("get_line_height"), &TextEdit::get_line_height);

	ClassDB::bind_method(D_METHOD("is_line_wrapped", "line"), &TextEdit::line_wraps);
	ClassDB::bind_method(D_METHOD("get_line_wrap_count", "line"), &TextEdit::times_line_wraps);

	ClassDB::bind_method(D_METHOD("center_viewport_to_cursor"), &TextEdit::center_viewport_to_cursor);
	ClassDB::bind_method(D_METHOD("cursor_set_column", "column", "adjust_viewport"), &TextEdit::cursor_set_column, DEFVAL(true));
	ClassDB::bind_method(D_METHOD("cursor_set_line", "line", "adjust_viewport", "can_be_hidden", "wrap_index"), &TextEdit::cursor_set_line, DEFVAL(true), DEFVAL(true), DEFVAL(0));

	ClassDB::bind_method(D_METHOD("cursor_get_column"), &TextEdit::cursor_get_column);
	ClassDB::bind_method(D_METHOD("cursor_get_line"), &TextEdit::cursor_get_line);
	ClassDB::bind_method(D_METHOD("cursor_set_blink_enabled", "enable"), &TextEdit::cursor_set_blink_enabled);
	ClassDB::bind_method(D_METHOD("cursor_get_blink_enabled"), &TextEdit::cursor_get_blink_enabled);
	ClassDB::bind_method(D_METHOD("cursor_set_blink_speed", "blink_speed"), &TextEdit::cursor_set_blink_speed);
	ClassDB::bind_method(D_METHOD("cursor_get_blink_speed"), &TextEdit::cursor_get_blink_speed);
	ClassDB::bind_method(D_METHOD("cursor_set_block_mode", "enable"), &TextEdit::cursor_set_block_mode);
	ClassDB::bind_method(D_METHOD("cursor_is_block_mode"), &TextEdit::cursor_is_block_mode);

	ClassDB::bind_method(D_METHOD("set_right_click_moves_caret", "enable"), &TextEdit::set_right_click_moves_caret);
	ClassDB::bind_method(D_METHOD("is_right_click_moving_caret"), &TextEdit::is_right_click_moving_caret);

	/* Line and character position. */
	ClassDB::bind_method(D_METHOD("get_pos_at_line_column", "line", "column"), &TextEdit::get_pos_at_line_column);
	ClassDB::bind_method(D_METHOD("get_rect_at_line_column", "line", "column"), &TextEdit::get_rect_at_line_column);
	ClassDB::bind_method(D_METHOD("get_line_column_at_pos", "position"), &TextEdit::get_line_column_at_pos);

	ClassDB::bind_method(D_METHOD("set_readonly", "enable"), &TextEdit::set_readonly);
	ClassDB::bind_method(D_METHOD("is_readonly"), &TextEdit::is_readonly);

	ClassDB::bind_method(D_METHOD("set_wrap_enabled", "enable"), &TextEdit::set_wrap_enabled);
	ClassDB::bind_method(D_METHOD("is_wrap_enabled"), &TextEdit::is_wrap_enabled);
	ClassDB::bind_method(D_METHOD("set_context_menu_enabled", "enable"), &TextEdit::set_context_menu_enabled);
	ClassDB::bind_method(D_METHOD("is_context_menu_enabled"), &TextEdit::is_context_menu_enabled);
	ClassDB::bind_method(D_METHOD("set_shortcut_keys_enabled", "enable"), &TextEdit::set_shortcut_keys_enabled);
	ClassDB::bind_method(D_METHOD("is_shortcut_keys_enabled"), &TextEdit::is_shortcut_keys_enabled);
	ClassDB::bind_method(D_METHOD("set_virtual_keyboard_enabled", "enable"), &TextEdit::set_virtual_keyboard_enabled);
	ClassDB::bind_method(D_METHOD("is_virtual_keyboard_enabled"), &TextEdit::is_virtual_keyboard_enabled);
	ClassDB::bind_method(D_METHOD("set_middle_mouse_paste_enabled", "enable"), &TextEdit::set_middle_mouse_paste_enabled);
	ClassDB::bind_method(D_METHOD("is_middle_mouse_paste_enabled"), &TextEdit::is_middle_mouse_paste_enabled);
	ClassDB::bind_method(D_METHOD("set_selecting_enabled", "enable"), &TextEdit::set_selecting_enabled);
	ClassDB::bind_method(D_METHOD("is_selecting_enabled"), &TextEdit::is_selecting_enabled);
	ClassDB::bind_method(D_METHOD("set_deselect_on_focus_loss_enabled", "enable"), &TextEdit::set_deselect_on_focus_loss_enabled);
	ClassDB::bind_method(D_METHOD("is_deselect_on_focus_loss_enabled"), &TextEdit::is_deselect_on_focus_loss_enabled);
	ClassDB::bind_method(D_METHOD("set_drag_and_drop_selection_enabled", "enable"), &TextEdit::set_drag_and_drop_selection_enabled);
	ClassDB::bind_method(D_METHOD("is_drag_and_drop_selection_enabled"), &TextEdit::is_drag_and_drop_selection_enabled);
	ClassDB::bind_method(D_METHOD("is_line_set_as_safe", "line"), &TextEdit::is_line_set_as_safe);
	ClassDB::bind_method(D_METHOD("set_line_as_safe", "line", "safe"), &TextEdit::set_line_as_safe);
	ClassDB::bind_method(D_METHOD("is_line_set_as_bookmark", "line"), &TextEdit::is_line_set_as_bookmark);
	ClassDB::bind_method(D_METHOD("set_line_as_bookmark", "line", "bookmark"), &TextEdit::set_line_as_bookmark);
	ClassDB::bind_method(D_METHOD("set_line_as_breakpoint", "line", "breakpoint"), &TextEdit::set_line_as_breakpoint);
	ClassDB::bind_method(D_METHOD("is_line_set_as_breakpoint", "line"), &TextEdit::is_line_set_as_breakpoint);

	ClassDB::bind_method(D_METHOD("cut"), &TextEdit::cut);
	ClassDB::bind_method(D_METHOD("copy"), &TextEdit::copy);
	ClassDB::bind_method(D_METHOD("paste"), &TextEdit::paste);

	ClassDB::bind_method(D_METHOD("select", "from_line", "from_column", "to_line", "to_column"), &TextEdit::select);
	ClassDB::bind_method(D_METHOD("select_all"), &TextEdit::select_all);
	ClassDB::bind_method(D_METHOD("deselect"), &TextEdit::deselect);

	ClassDB::bind_method(D_METHOD("is_selection_active"), &TextEdit::is_selection_active);
	ClassDB::bind_method(D_METHOD("get_selection_from_line"), &TextEdit::get_selection_from_line);
	ClassDB::bind_method(D_METHOD("get_selection_from_column"), &TextEdit::get_selection_from_column);
	ClassDB::bind_method(D_METHOD("get_selection_to_line"), &TextEdit::get_selection_to_line);
	ClassDB::bind_method(D_METHOD("get_selection_to_column"), &TextEdit::get_selection_to_column);
	ClassDB::bind_method(D_METHOD("get_selection_text"), &TextEdit::get_selection_text);
	ClassDB::bind_method(D_METHOD("is_mouse_over_selection", "edges"), &TextEdit::is_mouse_over_selection);
	ClassDB::bind_method(D_METHOD("get_word_under_cursor"), &TextEdit::get_word_under_cursor);
	ClassDB::bind_method(D_METHOD("search", "key", "flags", "from_line", "from_column"), &TextEdit::_search_bind);

	ClassDB::bind_method(D_METHOD("has_undo"), &TextEdit::has_undo);
	ClassDB::bind_method(D_METHOD("has_redo"), &TextEdit::has_redo);
	ClassDB::bind_method(D_METHOD("undo"), &TextEdit::undo);
	ClassDB::bind_method(D_METHOD("redo"), &TextEdit::redo);
	ClassDB::bind_method(D_METHOD("clear_undo_history"), &TextEdit::clear_undo_history);

	ClassDB::bind_method(D_METHOD("set_caret_on_readonly", "enable"), &TextEdit::set_caret_on_readonly);
	ClassDB::bind_method(D_METHOD("is_caret_on_readonly"), &TextEdit::is_caret_on_readonly);

	ClassDB::bind_method(D_METHOD("set_show_line_numbers", "enable"), &TextEdit::set_show_line_numbers);
	ClassDB::bind_method(D_METHOD("is_show_line_numbers_enabled"), &TextEdit::is_show_line_numbers_enabled);
	ClassDB::bind_method(D_METHOD("set_draw_tabs", "enable"), &TextEdit::set_draw_tabs);
	ClassDB::bind_method(D_METHOD("is_drawing_tabs"), &TextEdit::is_drawing_tabs);
	ClassDB::bind_method(D_METHOD("set_draw_spaces", "enable"), &TextEdit::set_draw_spaces);
	ClassDB::bind_method(D_METHOD("is_drawing_spaces"), &TextEdit::is_drawing_spaces);
	ClassDB::bind_method(D_METHOD("set_bookmark_gutter_enabled", "enable"), &TextEdit::set_bookmark_gutter_enabled);
	ClassDB::bind_method(D_METHOD("is_bookmark_gutter_enabled"), &TextEdit::is_bookmark_gutter_enabled);
	ClassDB::bind_method(D_METHOD("set_breakpoint_gutter_enabled", "enable"), &TextEdit::set_breakpoint_gutter_enabled);
	ClassDB::bind_method(D_METHOD("is_breakpoint_gutter_enabled"), &TextEdit::is_breakpoint_gutter_enabled);
	ClassDB::bind_method(D_METHOD("set_draw_fold_gutter", "enable"), &TextEdit::set_draw_fold_gutter);
	ClassDB::bind_method(D_METHOD("is_drawing_fold_gutter"), &TextEdit::is_drawing_fold_gutter);
	ClassDB::bind_method(D_METHOD("get_total_gutter_width"), &TextEdit::get_total_gutter_width);
	ClassDB::bind_method(D_METHOD("get_visible_rows"), &TextEdit::get_visible_rows);
	ClassDB::bind_method(D_METHOD("get_total_visible_rows"), &TextEdit::get_total_visible_rows);

	ClassDB::bind_method(D_METHOD("set_hiding_enabled", "enable"), &TextEdit::set_hiding_enabled);
	ClassDB::bind_method(D_METHOD("is_hiding_enabled"), &TextEdit::is_hiding_enabled);
	ClassDB::bind_method(D_METHOD("set_line_as_hidden", "line", "enable"), &TextEdit::set_line_as_hidden);
	ClassDB::bind_method(D_METHOD("is_line_hidden", "line"), &TextEdit::is_line_hidden);
	ClassDB::bind_method(D_METHOD("fold_all_lines"), &TextEdit::fold_all_lines);
	ClassDB::bind_method(D_METHOD("unhide_all_lines"), &TextEdit::unhide_all_lines);
	ClassDB::bind_method(D_METHOD("fold_line", "line"), &TextEdit::fold_line);
	ClassDB::bind_method(D_METHOD("unfold_line", "line"), &TextEdit::unfold_line);
	ClassDB::bind_method(D_METHOD("toggle_fold_line", "line"), &TextEdit::toggle_fold_line);
	ClassDB::bind_method(D_METHOD("can_fold", "line"), &TextEdit::can_fold);
	ClassDB::bind_method(D_METHOD("is_folded", "line"), &TextEdit::is_folded);

	ClassDB::bind_method(D_METHOD("set_highlight_all_occurrences", "enable"), &TextEdit::set_highlight_all_occurrences);
	ClassDB::bind_method(D_METHOD("is_highlight_all_occurrences_enabled"), &TextEdit::is_highlight_all_occurrences_enabled);

	ClassDB::bind_method(D_METHOD("set_override_selected_font_color", "override"), &TextEdit::set_override_selected_font_color);
	ClassDB::bind_method(D_METHOD("is_overriding_selected_font_color"), &TextEdit::is_overriding_selected_font_color);

	ClassDB::bind_method(D_METHOD("set_syntax_coloring", "enable"), &TextEdit::set_syntax_coloring);
	ClassDB::bind_method(D_METHOD("is_syntax_coloring_enabled"), &TextEdit::is_syntax_coloring_enabled);

	ClassDB::bind_method(D_METHOD("set_highlight_current_line", "enabled"), &TextEdit::set_highlight_current_line);
	ClassDB::bind_method(D_METHOD("is_highlight_current_line_enabled"), &TextEdit::is_highlight_current_line_enabled);

	ClassDB::bind_method(D_METHOD("set_smooth_scroll_enable", "enable"), &TextEdit::set_smooth_scroll_enabled);
	ClassDB::bind_method(D_METHOD("is_smooth_scroll_enabled"), &TextEdit::is_smooth_scroll_enabled);
	ClassDB::bind_method(D_METHOD("set_v_scroll_speed", "speed"), &TextEdit::set_v_scroll_speed);
	ClassDB::bind_method(D_METHOD("get_v_scroll_speed"), &TextEdit::get_v_scroll_speed);
	ClassDB::bind_method(D_METHOD("set_v_scroll", "value"), &TextEdit::set_v_scroll);
	ClassDB::bind_method(D_METHOD("get_v_scroll"), &TextEdit::get_v_scroll);
	ClassDB::bind_method(D_METHOD("set_h_scroll", "value"), &TextEdit::set_h_scroll);
	ClassDB::bind_method(D_METHOD("get_h_scroll"), &TextEdit::get_h_scroll);

	ClassDB::bind_method(D_METHOD("add_keyword_color", "keyword", "color"), &TextEdit::add_keyword_color);
	ClassDB::bind_method(D_METHOD("has_keyword_color", "keyword"), &TextEdit::has_keyword_color);
	ClassDB::bind_method(D_METHOD("get_keyword_color", "keyword"), &TextEdit::get_keyword_color);
	ClassDB::bind_method(D_METHOD("add_color_region", "begin_key", "end_key", "color", "line_only"), &TextEdit::add_color_region, DEFVAL(false));
	ClassDB::bind_method(D_METHOD("clear_colors"), &TextEdit::clear_colors);
	ClassDB::bind_method(D_METHOD("menu_option", "option"), &TextEdit::menu_option);
	ClassDB::bind_method(D_METHOD("get_menu"), &TextEdit::get_menu);

	ClassDB::bind_method(D_METHOD("get_breakpoints"), &TextEdit::get_breakpoints_array);
	ClassDB::bind_method(D_METHOD("remove_breakpoints"), &TextEdit::remove_breakpoints);

	ClassDB::bind_method(D_METHOD("draw_minimap", "draw"), &TextEdit::set_draw_minimap);
	ClassDB::bind_method(D_METHOD("is_drawing_minimap"), &TextEdit::is_drawing_minimap);
	ClassDB::bind_method(D_METHOD("set_minimap_width", "width"), &TextEdit::set_minimap_width);
	ClassDB::bind_method(D_METHOD("get_minimap_width"), &TextEdit::get_minimap_width);

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "text", PROPERTY_HINT_MULTILINE_TEXT), "set_text", "get_text");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "readonly"), "set_readonly", "is_readonly");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "highlight_current_line"), "set_highlight_current_line", "is_highlight_current_line_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "syntax_highlighting"), "set_syntax_coloring", "is_syntax_coloring_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "show_line_numbers"), "set_show_line_numbers", "is_show_line_numbers_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "draw_tabs"), "set_draw_tabs", "is_drawing_tabs");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "draw_spaces"), "set_draw_spaces", "is_drawing_spaces");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "bookmark_gutter"), "set_bookmark_gutter_enabled", "is_bookmark_gutter_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "breakpoint_gutter"), "set_breakpoint_gutter_enabled", "is_breakpoint_gutter_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "fold_gutter"), "set_draw_fold_gutter", "is_drawing_fold_gutter");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "highlight_all_occurrences"), "set_highlight_all_occurrences", "is_highlight_all_occurrences_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "override_selected_font_color"), "set_override_selected_font_color", "is_overriding_selected_font_color");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "context_menu_enabled"), "set_context_menu_enabled", "is_context_menu_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "shortcut_keys_enabled"), "set_shortcut_keys_enabled", "is_shortcut_keys_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "virtual_keyboard_enabled"), "set_virtual_keyboard_enabled", "is_virtual_keyboard_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "middle_mouse_paste_enabled"), "set_middle_mouse_paste_enabled", "is_middle_mouse_paste_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "selecting_enabled"), "set_selecting_enabled", "is_selecting_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "deselect_on_focus_loss_enabled"), "set_deselect_on_focus_loss_enabled", "is_deselect_on_focus_loss_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "drag_and_drop_selection_enabled"), "set_drag_and_drop_selection_enabled", "is_drag_and_drop_selection_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "smooth_scrolling"), "set_smooth_scroll_enable", "is_smooth_scroll_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::REAL, "v_scroll_speed"), "set_v_scroll_speed", "get_v_scroll_speed");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "hiding_enabled"), "set_hiding_enabled", "is_hiding_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "wrap_enabled"), "set_wrap_enabled", "is_wrap_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::REAL, "scroll_vertical"), "set_v_scroll", "get_v_scroll");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "scroll_horizontal"), "set_h_scroll", "get_h_scroll");

	ADD_GROUP("Minimap", "minimap_");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "minimap_draw"), "draw_minimap", "is_drawing_minimap");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "minimap_width"), "set_minimap_width", "get_minimap_width");

	ADD_GROUP("Caret", "caret_");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "caret_block_mode"), "cursor_set_block_mode", "cursor_is_block_mode");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "caret_blink"), "cursor_set_blink_enabled", "cursor_get_blink_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::REAL, "caret_blink_speed", PROPERTY_HINT_RANGE, "0.1,10,0.01"), "cursor_set_blink_speed", "cursor_get_blink_speed");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "caret_moving_by_right_click"), "set_right_click_moves_caret", "is_right_click_moving_caret");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "caret_on_readonly"), "set_caret_on_readonly", "is_caret_on_readonly");

	ADD_SIGNAL(MethodInfo("cursor_changed"));
	ADD_SIGNAL(MethodInfo("text_changed"));
	ADD_SIGNAL(MethodInfo("request_completion"));
	ADD_SIGNAL(MethodInfo("breakpoint_toggled", PropertyInfo(Variant::INT, "row")));
	ADD_SIGNAL(MethodInfo("symbol_lookup", PropertyInfo(Variant::STRING, "symbol"), PropertyInfo(Variant::INT, "row"), PropertyInfo(Variant::INT, "column")));
	ADD_SIGNAL(MethodInfo("info_clicked", PropertyInfo(Variant::INT, "row"), PropertyInfo(Variant::STRING, "info")));

	BIND_ENUM_CONSTANT(MENU_CUT);
	BIND_ENUM_CONSTANT(MENU_COPY);
	BIND_ENUM_CONSTANT(MENU_PASTE);
	BIND_ENUM_CONSTANT(MENU_CLEAR);
	BIND_ENUM_CONSTANT(MENU_SELECT_ALL);
	BIND_ENUM_CONSTANT(MENU_UNDO);
	BIND_ENUM_CONSTANT(MENU_REDO);
	BIND_ENUM_CONSTANT(MENU_MAX);

	GLOBAL_DEF("gui/timers/text_edit_idle_detect_sec", 3);
	ProjectSettings::get_singleton()->set_custom_property_info("gui/timers/text_edit_idle_detect_sec", PropertyInfo(Variant::REAL, "gui/timers/text_edit_idle_detect_sec", PROPERTY_HINT_RANGE, "0,10,0.01,or_greater")); // No negative numbers.
	GLOBAL_DEF("gui/common/text_edit_undo_stack_max_size", 1024);
	ProjectSettings::get_singleton()->set_custom_property_info("gui/common/text_edit_undo_stack_max_size", PropertyInfo(Variant::INT, "gui/common/text_edit_undo_stack_max_size", PROPERTY_HINT_RANGE, "0,10000,1,or_greater")); // No negative numbers.
}

TextEdit::TextEdit() {
	setting_row = false;
	draw_tabs = false;
	caret_on_readonly = false;
	draw_spaces = false;
	override_selected_font_color = false;
	draw_caret = true;
	clear();
	wrap_enabled = false;
	wrap_at = 0;
	wrap_right_offset = 10;
	set_focus_mode(FOCUS_ALL);
	syntax_highlighter = nullptr;
	_update_caches();
	cache.row_height = 1;
	cache.line_spacing = 1;
	cache.line_number_w = 1;
	cache.breakpoint_gutter_width = 0;
	breakpoint_gutter_width = 0;
	cache.fold_gutter_width = 0;
	fold_gutter_width = 0;
	info_gutter_width = 0;
	cache.info_gutter_width = 0;
	set_default_cursor_shape(CURSOR_IBEAM);

	indent_size = 4;
	text.set_indent_size(indent_size);
	text.clear();
	text.set_color_regions(&color_regions);

	h_scroll = memnew(HScrollBar);
	v_scroll = memnew(VScrollBar);

	add_child(h_scroll);
	add_child(v_scroll);

	updating_scrolls = false;
	selection.active = false;

	h_scroll->connect("value_changed", this, "_scroll_moved");
	v_scroll->connect("value_changed", this, "_scroll_moved");

	v_scroll->connect("scrolling", this, "_v_scroll_input");

	cursor_changed_dirty = false;
	text_changed_dirty = false;

	selection.selecting_mode = Selection::MODE_NONE;
	selection.selecting_line = 0;
	selection.selecting_column = 0;
	selection.selecting_text = false;
	selection.active = false;
	syntax_coloring = false;

	block_caret = false;
	caret_blink_enabled = false;
	caret_blink_timer = memnew(Timer);
	add_child(caret_blink_timer);
	caret_blink_timer->set_wait_time(0.65);
	caret_blink_timer->connect("timeout", this, "_toggle_draw_caret");
	cursor_set_blink_enabled(false);
	right_click_moves_caret = true;

	idle_detect = memnew(Timer);
	add_child(idle_detect);
	idle_detect->set_one_shot(true);
	idle_detect->set_wait_time(GLOBAL_GET("gui/timers/text_edit_idle_detect_sec"));
	idle_detect->connect("timeout", this, "_push_current_op");

	click_select_held = memnew(Timer);
	add_child(click_select_held);
	click_select_held->set_wait_time(0.05);
	click_select_held->connect("timeout", this, "_click_selection_held");

	current_op.type = TextOperation::TYPE_NONE;
	undo_enabled = true;
	undo_stack_max_size = GLOBAL_GET("gui/common/text_edit_undo_stack_max_size");
	undo_stack_pos = nullptr;
	setting_text = false;
	last_dblclk = 0;
	current_op.version = 0;
	version = 0;
	saved_version = 0;

	completion_enabled = false;
	completion_active = false;
	completion_line_ofs = 0;
	tooltip_obj_id = ObjectID();
	line_numbers = false;
	line_numbers_zero_padded = false;
	line_length_guidelines = false;
	line_length_guideline_soft_col = 80;
	line_length_guideline_hard_col = 100;
	draw_bookmark_gutter = false;
	draw_breakpoint_gutter = false;
	draw_fold_gutter = false;
	draw_info_gutter = false;
	hiding_enabled = false;
	next_operation_is_complex = false;
	scroll_past_end_of_file_enabled = false;
	auto_brace_completion_enabled = false;
	brace_matching_enabled = false;
	highlight_all_occurrences = false;
	highlight_current_line = false;
	indent_using_spaces = false;
	space_indent = "    ";
	auto_indent = false;
	insert_mode = false;
	window_has_focus = true;
	select_identifiers_enabled = false;
	smooth_scroll_enabled = false;
	scrolling = false;
	minimap_clicked = false;
	hovering_minimap = false;
	dragging_minimap = false;
	can_drag_minimap = false;
	minimap_scroll_ratio = 0;
	minimap_scroll_click_pos = 0;
	dragging_selection = false;
	target_v_scroll = 0;
	v_scroll_speed = 80;
	draw_minimap = false;
	minimap_width = 80;
	minimap_char_size = Point2(1, 2);
	minimap_line_spacing = 1;

	selecting_enabled = true;
	deselect_on_focus_loss_enabled = true;
	context_menu_enabled = true;
	shortcut_keys_enabled = true;
	middle_mouse_paste_enabled = true;
	menu = memnew(PopupMenu);
	add_child(menu);
	readonly = true; // Initialise to opposite first, so we get past the early-out in set_readonly.
	set_readonly(false);
	menu->connect("id_pressed", this, "menu_option");
	first_draw = true;

	executing_line = -1;
}

TextEdit::~TextEdit() {
}

///////////////////////////////////////////////////////////////////////////////

Map<int, TextEdit::HighlighterInfo> TextEdit::_get_line_syntax_highlighting(int p_line) {
	if (syntax_highlighting_cache.has(p_line)) {
		return syntax_highlighting_cache[p_line];
	}

	if (syntax_highlighter != nullptr) {
		Map<int, HighlighterInfo> color_map = syntax_highlighter->_get_line_syntax_highlighting(p_line);
		syntax_highlighting_cache[p_line] = color_map;
		return color_map;
	}

	Map<int, HighlighterInfo> color_map;

	bool prev_is_char = false;
	bool prev_is_number = false;
	bool in_keyword = false;
	bool in_word = false;
	bool in_function_name = false;
	bool in_member_variable = false;
	bool is_hex_notation = false;
	Color keyword_color;
	Color color;

	int in_region = _is_line_in_region(p_line);
	int deregion = 0;

	const Map<int, TextEdit::Text::ColorRegionInfo> cri_map = text.get_color_region_info(p_line);
	const String &str = text[p_line];
	Color prev_color;
	for (int j = 0; j < str.length(); j++) {
		HighlighterInfo highlighter_info;

		if (deregion > 0) {
			deregion--;
			if (deregion == 0) {
				in_region = -1;
			}
		}

		if (deregion != 0) {
			if (color != prev_color) {
				prev_color = color;
				highlighter_info.color = color;
				color_map[j] = highlighter_info;
			}
			continue;
		}

		color = cache.font_color;

		bool is_char = _is_text_char(str[j]);
		bool is_symbol = _is_symbol(str[j]);
		bool is_number = _is_number(str[j]);

		// Allow ABCDEF in hex notation.
		if (is_hex_notation && (_is_hex_symbol(str[j]) || is_number)) {
			is_number = true;
		} else {
			is_hex_notation = false;
		}

		// Check for dot or underscore or 'x' for hex notation in floating point number or 'e' for scientific notation.
		if ((str[j] == '.' || str[j] == 'x' || str[j] == '_' || str[j] == 'f' || str[j] == 'e') && !in_word && prev_is_number && !is_number) {
			is_number = true;
			is_symbol = false;
			is_char = false;

			if (str[j] == 'x' && str[j - 1] == '0') {
				is_hex_notation = true;
			}
		}

		if (!in_word && _is_char(str[j]) && !is_number) {
			in_word = true;
		}

		if ((in_keyword || in_word) && !is_hex_notation) {
			is_number = false;
		}

		if (is_symbol && str[j] != '.' && in_word) {
			in_word = false;
		}

		if (is_symbol && cri_map.has(j)) {
			const TextEdit::Text::ColorRegionInfo &cri = cri_map[j];

			if (in_region == -1) {
				if (!cri.end) {
					in_region = cri.region;
				}
			} else if (in_region == cri.region && !color_regions[cri.region].line_only) { // Ignore otherwise.
				if (cri.end || color_regions[cri.region].eq) {
					deregion = color_regions[cri.region].eq ? color_regions[cri.region].begin_key.length() : color_regions[cri.region].end_key.length();
				}
			}
		}

		if (!is_char) {
			in_keyword = false;
		}

		if (in_region == -1 && !in_keyword && is_char && !prev_is_char) {
			int to = j;
			while (to < str.length() && _is_text_char(str[to])) {
				to++;
			}

			uint32_t hash = String::hash(&str[j], to - j);
			StrRange range(&str[j], to - j);

			const Color *col = keywords.custom_getptr(range, hash);

			if (!col) {
				col = member_keywords.custom_getptr(range, hash);

				if (col) {
					for (int k = j - 1; k >= 0; k--) {
						if (str[k] == '.') {
							col = nullptr; // Member indexing not allowed.
							break;
						} else if (str[k] > 32) {
							break;
						}
					}
				}
			}

			if (col) {
				in_keyword = true;
				keyword_color = *col;
			}
		}

		if (!in_function_name && in_word && !in_keyword) {
			int k = j;
			while (k < str.length() && !_is_symbol(str[k]) && str[k] != '\t' && str[k] != ' ') {
				k++;
			}

			// Check for space between name and bracket.
			while (k < str.length() && (str[k] == '\t' || str[k] == ' ')) {
				k++;
			}

			if (str[k] == '(') {
				in_function_name = true;
			}
		}

		if (!in_function_name && !in_member_variable && !in_keyword && !is_number && in_word) {
			int k = j;
			while (k > 0 && !_is_symbol(str[k]) && str[k] != '\t' && str[k] != ' ') {
				k--;
			}

			if (str[k] == '.') {
				in_member_variable = true;
			}
		}

		if (is_symbol) {
			in_function_name = false;
			in_member_variable = false;
		}

		if (in_region >= 0) {
			color = color_regions[in_region].color;
		} else if (in_keyword) {
			color = keyword_color;
		} else if (in_member_variable) {
			color = cache.member_variable_color;
		} else if (in_function_name) {
			color = cache.function_color;
		} else if (is_symbol) {
			color = cache.symbol_color;
		} else if (is_number) {
			color = cache.number_color;
		}

		prev_is_char = is_char;
		prev_is_number = is_number;

		if (color != prev_color) {
			prev_color = color;
			highlighter_info.color = color;
			color_map[j] = highlighter_info;
		}
	}

	syntax_highlighting_cache[p_line] = color_map;
	return color_map;
}

void SyntaxHighlighter::set_text_editor(TextEdit *p_text_editor) {
	text_editor = p_text_editor;
}

TextEdit *SyntaxHighlighter::get_text_editor() {
	return text_editor;
}

/**************************************************************************/
/*  code_edit.cpp                                                         */
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

#include "code_edit.h"
#include "code_edit.compat.inc"

#include "core/config/project_settings.h"
#include "core/os/keyboard.h"
#include "core/string/string_builder.h"
#include "core/string/ustring.h"
#include "scene/theme/theme_db.h"

void CodeEdit::_apply_project_settings() {
	symbol_tooltip_timer->set_wait_time(GLOBAL_GET("gui/timers/tooltip_delay_sec"));
}

void CodeEdit::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY: {
			_apply_project_settings();
#ifdef TOOLS_ENABLED
			if (Engine::get_singleton()->is_editor_hint()) {
				ProjectSettings::get_singleton()->connect("settings_changed", callable_mp(this, &CodeEdit::_apply_project_settings));
			}
#endif // TOOLS_ENABLED
		} break;

		case NOTIFICATION_THEME_CHANGED: {
			set_gutter_width(main_gutter, get_line_height());
			_update_line_number_gutter_width();
			set_gutter_width(fold_gutter, get_line_height() / 1.2);
			_clear_line_number_text_cache();
		} break;

		case NOTIFICATION_TRANSLATION_CHANGED:
			[[fallthrough]];
		case NOTIFICATION_LAYOUT_DIRECTION_CHANGED:
			[[fallthrough]];
		case NOTIFICATION_VISIBILITY_CHANGED: {
			// Avoid having many hidden text editors with unused cache filling up memory.
			_clear_line_number_text_cache();
		} break;

		case NOTIFICATION_DRAW: {
			RID ci = get_canvas_item();
			const Size2 size = get_size();
			const bool caret_visible = is_caret_visible();
			const bool rtl = is_layout_rtl();
			const int row_height = get_line_height();

			if (line_length_guideline_columns.size() > 0) {
				const int xmargin_beg = theme_cache.style_normal->get_margin(SIDE_LEFT) + get_total_gutter_width();
				const int xmargin_end = size.width - theme_cache.style_normal->get_margin(SIDE_RIGHT) - (is_drawing_minimap() ? get_minimap_width() : 0);
				const float char_size = theme_cache.font->get_char_size('0', theme_cache.font_size).width;

				for (int i = 0; i < line_length_guideline_columns.size(); i++) {
					const int xoffset = xmargin_beg + char_size * (int)line_length_guideline_columns[i] - get_h_scroll();
					if (xoffset > xmargin_beg && xoffset < xmargin_end) {
						Color guideline_color = (i == 0) ? theme_cache.line_length_guideline_color : theme_cache.line_length_guideline_color * Color(1, 1, 1, 0.5);
						if (rtl) {
							RenderingServer::get_singleton()->canvas_item_add_line(ci, Point2(size.width - xoffset, 0), Point2(size.width - xoffset, size.height), guideline_color);
							continue;
						}
						RenderingServer::get_singleton()->canvas_item_add_line(ci, Point2(xoffset, 0), Point2(xoffset, size.height), guideline_color);
					}
				}
			}

			if (caret_visible) {
				const bool draw_code_completion = code_completion_active && !code_completion_options.is_empty();
				const bool draw_code_hint = !code_hint.is_empty();

				/* Code hint */
				Size2 code_hint_minsize;
				if (draw_code_hint) {
					const int font_height = theme_cache.font->get_height(theme_cache.font_size);

					Vector<String> code_hint_lines = code_hint.split("\n");
					int line_count = code_hint_lines.size();

					int max_width = 0;
					for (int i = 0; i < line_count; i++) {
						max_width = MAX(max_width, theme_cache.font->get_string_size(code_hint_lines[i], HORIZONTAL_ALIGNMENT_LEFT, -1, theme_cache.font_size).x);
					}
					code_hint_minsize = theme_cache.code_hint_style->get_minimum_size() + Size2(max_width, line_count * font_height + (theme_cache.line_spacing * line_count - 1));

					int offset = theme_cache.font->get_string_size(code_hint_lines[0].substr(0, code_hint_lines[0].find(String::chr(0xFFFF))), HORIZONTAL_ALIGNMENT_LEFT, -1, theme_cache.font_size).x;
					if (code_hint_xpos == -0xFFFF) {
						code_hint_xpos = get_caret_draw_pos().x - offset;
					}
					Point2 hint_ofs = Vector2(code_hint_xpos, get_caret_draw_pos().y);
					if (code_hint_draw_below) {
						hint_ofs.y += theme_cache.line_spacing / 2.0f;
					} else {
						hint_ofs.y -= (code_hint_minsize.y + row_height) - theme_cache.line_spacing;
					}

					draw_style_box(theme_cache.code_hint_style, Rect2(hint_ofs, code_hint_minsize));

					int yofs = 0;
					for (int i = 0; i < line_count; i++) {
						const String &line = code_hint_lines[i];

						int begin = 0;
						int end = 0;
						if (line.contains(String::chr(0xFFFF))) {
							begin = theme_cache.font->get_string_size(line.substr(0, line.find(String::chr(0xFFFF))), HORIZONTAL_ALIGNMENT_LEFT, -1, theme_cache.font_size).x;
							end = theme_cache.font->get_string_size(line.substr(0, line.rfind(String::chr(0xFFFF))), HORIZONTAL_ALIGNMENT_LEFT, -1, theme_cache.font_size).x;
						}

						Point2 round_ofs = hint_ofs + theme_cache.code_hint_style->get_offset() + Vector2(0, theme_cache.font->get_ascent(theme_cache.font_size) + font_height * i + yofs);
						round_ofs = round_ofs.round();
						draw_string(theme_cache.font, round_ofs, line.replace(String::chr(0xFFFF), ""), HORIZONTAL_ALIGNMENT_LEFT, -1, theme_cache.font_size, theme_cache.code_hint_color);
						if (end > 0) {
							// Draw an underline for the currently edited function parameter.
							const Vector2 b = hint_ofs + theme_cache.code_hint_style->get_offset() + Vector2(begin, font_height + font_height * i + yofs);
							draw_line(b, b + Vector2(end - begin, 0), theme_cache.code_hint_color, 2);

							// Draw a translucent text highlight as well.
							const Rect2 highlight_rect = Rect2(
									b - Vector2(0, font_height),
									Vector2(end - begin, font_height));
							draw_rect(highlight_rect, theme_cache.code_hint_color * Color(1, 1, 1, 0.2));
						}
						yofs += theme_cache.line_spacing;
					}
				}

				/* Code completion */
				if (draw_code_completion) {
					const int code_completion_options_count = code_completion_options.size();
					const int lines = MIN(code_completion_options_count, theme_cache.code_completion_max_lines);
					const Size2 icon_area_size(row_height, row_height);

					code_completion_rect.size.width = code_completion_longest_line + theme_cache.code_completion_icon_separation + icon_area_size.width + 2;
					code_completion_rect.size.height = lines * row_height;

					const Point2 caret_pos = get_caret_draw_pos();
					const int total_height = theme_cache.code_completion_style->get_minimum_size().y + code_completion_rect.size.height;
					int min_y = caret_pos.y - row_height;
					int max_y = caret_pos.y + row_height + total_height;
					if (draw_code_hint) {
						if (code_hint_draw_below) {
							max_y += code_hint_minsize.y;
						} else {
							min_y -= code_hint_minsize.y;
						}
					}

					const bool can_fit_completion_above = (min_y > total_height);
					const bool can_fit_completion_below = (max_y <= get_size().height);
					if (!can_fit_completion_below && can_fit_completion_above) {
						code_completion_rect.position.y = (caret_pos.y - total_height - row_height) + theme_cache.line_spacing;
						if (draw_code_hint && !code_hint_draw_below) {
							code_completion_rect.position.y -= code_hint_minsize.y;
						}
					} else {
						code_completion_rect.position.y = caret_pos.y + (theme_cache.line_spacing / 2.0f);
						if (draw_code_hint && code_hint_draw_below) {
							code_completion_rect.position.y += code_hint_minsize.y;
						}
					}

					const int scroll_width = code_completion_options_count > theme_cache.code_completion_max_lines ? theme_cache.code_completion_scroll_width : 0;
					const int code_completion_base_width = theme_cache.font->get_string_size(code_completion_base, HORIZONTAL_ALIGNMENT_LEFT, -1, theme_cache.font_size).width;
					if (caret_pos.x - code_completion_base_width + code_completion_rect.size.width + scroll_width > get_size().width) {
						code_completion_rect.position.x = get_size().width - code_completion_rect.size.width - scroll_width;
					} else {
						code_completion_rect.position.x = caret_pos.x - code_completion_base_width;
					}

					draw_style_box(theme_cache.code_completion_style, Rect2(code_completion_rect.position - theme_cache.code_completion_style->get_offset(), code_completion_rect.size + theme_cache.code_completion_style->get_minimum_size() + Size2(scroll_width, 0)));
					if (theme_cache.code_completion_background_color.a > 0.01) {
						RenderingServer::get_singleton()->canvas_item_add_rect(ci, Rect2(code_completion_rect.position, code_completion_rect.size + Size2(scroll_width, 0)), theme_cache.code_completion_background_color);
					}

					code_completion_scroll_rect.position = code_completion_rect.position + Vector2(code_completion_rect.size.width, 0);
					code_completion_scroll_rect.size = Vector2(scroll_width, code_completion_rect.size.height);

					code_completion_line_ofs = CLAMP((code_completion_force_item_center < 0 ? code_completion_current_selected : code_completion_force_item_center) - lines / 2, 0, code_completion_options_count - lines);
					RenderingServer::get_singleton()->canvas_item_add_rect(ci, Rect2(Point2(code_completion_rect.position.x, code_completion_rect.position.y + (code_completion_current_selected - code_completion_line_ofs) * row_height), Size2(code_completion_rect.size.width, row_height)), theme_cache.code_completion_selected_color);

					for (int i = 0; i < lines; i++) {
						int l = code_completion_line_ofs + i;
						ERR_CONTINUE(l < 0 || l >= code_completion_options_count);

						Ref<TextLine> tl;
						tl.instantiate();
						tl->add_string(code_completion_options[l].display, theme_cache.font, theme_cache.font_size);

						int yofs = (row_height - tl->get_size().y) / 2;
						Point2 title_pos(code_completion_rect.position.x, code_completion_rect.position.y + i * row_height + yofs);

						/* Draw completion icon if it is valid. */
						const Ref<Texture2D> &icon = code_completion_options[l].icon;
						Rect2 icon_area(code_completion_rect.position.x, code_completion_rect.position.y + i * row_height, icon_area_size.width, icon_area_size.height);
						if (icon.is_valid()) {
							Size2 icon_size = icon_area.size * 0.7;
							icon->draw_rect(ci, Rect2(icon_area.position + (icon_area.size - icon_size) / 2, icon_size));
						}
						title_pos.x = icon_area.position.x + icon_area.size.width + theme_cache.code_completion_icon_separation;

						tl->set_width(code_completion_rect.size.width - (icon_area_size.x + theme_cache.code_completion_icon_separation));
						if (rtl) {
							if (code_completion_options[l].default_value.get_type() == Variant::COLOR) {
								draw_rect(Rect2(Point2(code_completion_rect.position.x, icon_area.position.y), icon_area_size), (Color)code_completion_options[l].default_value);
							}
							tl->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_RIGHT);
						} else {
							if (code_completion_options[l].default_value.get_type() == Variant::COLOR) {
								const Color color = code_completion_options[l].default_value;
								const Rect2 rect = Rect2(Point2(code_completion_rect.position.x + code_completion_rect.size.width - icon_area_size.x, icon_area.position.y), icon_area_size);
								if (color.a < 1.0) {
									draw_texture_rect(theme_cache.completion_color_bg, rect, true);
								}

								draw_rect(rect, color);
							}
							tl->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_LEFT);
						}

						Point2 match_pos = Point2(code_completion_rect.position.x + icon_area_size.x + theme_cache.code_completion_icon_separation, code_completion_rect.position.y + i * row_height);

						for (int j = 0; j < code_completion_options[l].matches.size(); j++) {
							Pair<int, int> match_segment = code_completion_options[l].matches[j];
							int match_offset = theme_cache.font->get_string_size(code_completion_options[l].display.substr(0, match_segment.first), HORIZONTAL_ALIGNMENT_LEFT, -1, theme_cache.font_size).width;
							int match_len = theme_cache.font->get_string_size(code_completion_options[l].display.substr(match_segment.first, match_segment.second), HORIZONTAL_ALIGNMENT_LEFT, -1, theme_cache.font_size).width;

							draw_rect(Rect2(match_pos + Point2(match_offset, 0), Size2(match_len, row_height)), theme_cache.code_completion_existing_color);
						}
						tl->draw(ci, title_pos, code_completion_options[l].font_color);
					}

					/* Draw a small scroll rectangle to show a position in the options. */
					if (scroll_width) {
						Color scroll_color = is_code_completion_scroll_hovered || is_code_completion_scroll_pressed ? theme_cache.code_completion_scroll_hovered_color : theme_cache.code_completion_scroll_color;

						float r = (float)theme_cache.code_completion_max_lines / code_completion_options_count;
						float o = (float)code_completion_line_ofs / code_completion_options_count;
						draw_rect(Rect2(code_completion_rect.position.x + code_completion_rect.size.width, code_completion_rect.position.y + o * code_completion_rect.size.y, scroll_width, code_completion_rect.size.y * r), scroll_color);
					}
				}
			}
		} break;

		case NOTIFICATION_DRAG_BEGIN: {
			cancel_code_completion();
		} break;

		case NOTIFICATION_MOUSE_EXIT: {
			symbol_tooltip_timer->stop();
		} break;
	}
}

void CodeEdit::gui_input(const Ref<InputEvent> &p_gui_input) {
	Ref<InputEventPanGesture> pan_gesture = p_gui_input;
	if (pan_gesture.is_valid() && code_completion_active && code_completion_rect.has_point(pan_gesture->get_position())) {
		const real_t delta = pan_gesture->get_delta().y;
		code_completion_pan_offset += delta;
		if (code_completion_pan_offset <= -1.0) {
			if (code_completion_current_selected > 0) {
				code_completion_current_selected--;
				code_completion_force_item_center = -1;
				queue_redraw();
			}
			code_completion_pan_offset = 0;
		} else if (code_completion_pan_offset >= +1.0) {
			if (code_completion_current_selected < code_completion_options.size() - 1) {
				code_completion_current_selected++;
				code_completion_force_item_center = -1;
				queue_redraw();
			}
			code_completion_pan_offset = 0;
		}
		accept_event();
		return;
	}

	Ref<InputEventMouseButton> mb = p_gui_input;
	if (mb.is_valid()) {
		// Ignore mouse clicks in IME input mode, let TextEdit handle it.
		if (has_ime_text()) {
			TextEdit::gui_input(p_gui_input);
			return;
		}

		if (is_code_completion_scroll_pressed && mb->get_button_index() == MouseButton::LEFT) {
			is_code_completion_scroll_pressed = false;
			accept_event();
			queue_redraw();
			return;
		}

		if (is_code_completion_drag_started && !mb->is_pressed()) {
			is_code_completion_drag_started = false;
			accept_event();
			queue_redraw();
			return;
		}

		if (code_completion_active && code_completion_rect.has_point(mb->get_position())) {
			if (!mb->is_pressed()) {
				accept_event();
				return;
			}
			is_code_completion_drag_started = true;

			switch (mb->get_button_index()) {
				case MouseButton::WHEEL_UP: {
					if (code_completion_current_selected > 0) {
						code_completion_current_selected--;
						code_completion_force_item_center = -1;
						code_completion_pan_offset = 0.0f;
						queue_redraw();
					}
				} break;
				case MouseButton::WHEEL_DOWN: {
					if (code_completion_current_selected < code_completion_options.size() - 1) {
						code_completion_current_selected++;
						code_completion_force_item_center = -1;
						code_completion_pan_offset = 0.0f;
						queue_redraw();
					}
				} break;
				case MouseButton::LEFT: {
					if (code_completion_force_item_center == -1) {
						code_completion_force_item_center = code_completion_current_selected;
					}

					code_completion_current_selected = CLAMP(code_completion_line_ofs + (mb->get_position().y - code_completion_rect.position.y) / get_line_height(), 0, code_completion_options.size() - 1);
					code_completion_pan_offset = 0.0f;
					if (mb->is_double_click()) {
						confirm_code_completion();
					}
					queue_redraw();
				} break;
				default:
					break;
			}

			accept_event();
			return;
		} else if (code_completion_active && code_completion_scroll_rect.has_point(mb->get_position())) {
			if (mb->get_button_index() != MouseButton::LEFT) {
				accept_event();
				return;
			}

			if (mb->is_pressed()) {
				is_code_completion_drag_started = true;
				is_code_completion_scroll_pressed = true;

				_update_scroll_selected_line(mb->get_position().y);
				queue_redraw();
			}

			accept_event();
			return;
		}

		cancel_code_completion();
		set_code_hint("");

		if (mb->is_pressed()) {
			Vector2i mpos = mb->get_position();
			if (is_layout_rtl()) {
				mpos.x = get_size().x - mpos.x;
			}

			Point2i pos = get_line_column_at_pos(mpos, false);
			int line = pos.y;
			int col = pos.x;

			if (line != -1 && mb->get_button_index() == MouseButton::LEFT) {
				if (is_line_folded(line)) {
					int wrap_index = get_line_wrap_index_at_column(line, col);
					if (wrap_index == get_line_wrap_count(line)) {
						int eol_icon_width = theme_cache.folded_eol_icon->get_width();
						int left_margin = get_total_gutter_width() + eol_icon_width + get_line_width(line, wrap_index) - get_h_scroll();
						if (mpos.x > left_margin && mpos.x <= left_margin + eol_icon_width + 3) {
							unfold_line(line);
							return;
						}
					}
				}
			}
		} else {
			if (mb->get_button_index() == MouseButton::LEFT) {
				if (mb->is_command_or_control_pressed() && !symbol_lookup_word.is_empty()) {
					Vector2i mpos = mb->get_position();
					if (is_layout_rtl()) {
						mpos.x = get_size().x - mpos.x;
					}

					Point2i pos = get_line_column_at_pos(mpos, false);
					int line = pos.y;
					int col = pos.x;

					if (line != -1) {
						emit_signal(SNAME("symbol_lookup"), symbol_lookup_word, line, col);
					}
					return;
				}
			}
		}
	}

	Ref<InputEventMouseMotion> mm = p_gui_input;
	if (mm.is_valid()) {
		Vector2i mpos = mm->get_position();
		if (is_layout_rtl()) {
			mpos.x = get_size().x - mpos.x;
		}

		if (symbol_lookup_on_click_enabled) {
			if (mm->is_command_or_control_pressed() && mm->get_button_mask().is_empty()) {
				symbol_lookup_pos = get_line_column_at_pos(mpos);
				symbol_lookup_new_word = get_word_at_pos(mpos);
				if (symbol_lookup_new_word != symbol_lookup_word) {
					emit_signal(SNAME("symbol_validate"), symbol_lookup_new_word);
				}
			} else if (!mm->is_command_or_control_pressed() || (!mm->get_button_mask().is_empty() && symbol_lookup_pos != get_line_column_at_pos(mpos))) {
				set_symbol_lookup_word_as_valid(false);
			}
		}

		if (symbol_tooltip_on_hover_enabled) {
			symbol_tooltip_pos = get_line_column_at_pos(mpos, false);
			symbol_tooltip_word = get_word_at_pos(mpos);
			symbol_tooltip_timer->start();
		}

		bool scroll_hovered = code_completion_scroll_rect.has_point(mpos);
		if (is_code_completion_scroll_hovered != scroll_hovered) {
			is_code_completion_scroll_hovered = scroll_hovered;
			accept_event();
			queue_redraw();
		}

		if (is_code_completion_scroll_pressed) {
			_update_scroll_selected_line(mpos.y);
			accept_event();
			queue_redraw();
			return;
		}

		if (code_completion_active && code_completion_rect.has_point(mm->get_position())) {
			accept_event();
			return;
		}
	}

	Ref<InputEventKey> k = p_gui_input;
	if (TextEdit::alt_input(p_gui_input)) {
		accept_event();
		return;
	}

	bool update_code_completion = false;
	if (k.is_null()) {
		// MouseMotion events should not be handled by TextEdit logic if we're
		// currently clicking and dragging from the code completion panel.
		if (mm.is_null() || !is_code_completion_drag_started) {
			TextEdit::gui_input(p_gui_input);
		}
		return;
	}

	/* Ctrl + Hover symbols */
	bool mac_keys = OS::get_singleton()->has_feature("macos") || OS::get_singleton()->has_feature("web_macos") || OS::get_singleton()->has_feature("web_ios");
	if ((mac_keys && k->get_keycode() == Key::META) || (!mac_keys && k->get_keycode() == Key::CTRL)) {
		if (symbol_lookup_on_click_enabled) {
			if (k->is_pressed() && !is_dragging_cursor()) {
				symbol_lookup_new_word = get_word_at_pos(get_local_mouse_pos());
				if (symbol_lookup_new_word != symbol_lookup_word) {
					emit_signal(SNAME("symbol_validate"), symbol_lookup_new_word);
				}
			} else {
				set_symbol_lookup_word_as_valid(false);
			}
		}
		return;
	}

	/* If a modifier has been pressed, and nothing else, return. */
	if (!k->is_pressed() || k->get_keycode() == Key::CTRL || k->get_keycode() == Key::ALT || k->get_keycode() == Key::SHIFT || k->get_keycode() == Key::META || k->get_keycode() == Key::CAPSLOCK) {
		return;
	}

	// Allow unicode handling if:
	// No modifiers are pressed (except Shift and CapsLock)
	bool allow_unicode_handling = !(k->is_ctrl_pressed() || k->is_alt_pressed() || k->is_meta_pressed());

	/* AUTO-COMPLETE */
	if (code_completion_enabled && k->is_action("ui_text_completion_query", true)) {
		request_code_completion(true);
		accept_event();
		return;
	}

	if (code_completion_active) {
		if (k->is_action("ui_up", true)) {
			if (code_completion_current_selected > 0) {
				code_completion_current_selected--;
			} else {
				code_completion_current_selected = code_completion_options.size() - 1;
			}
			code_completion_force_item_center = -1;
			code_completion_pan_offset = 0.0f;
			queue_redraw();
			accept_event();
			return;
		}
		if (k->is_action("ui_down", true)) {
			if (code_completion_current_selected < code_completion_options.size() - 1) {
				code_completion_current_selected++;
			} else {
				code_completion_current_selected = 0;
			}
			code_completion_force_item_center = -1;
			code_completion_pan_offset = 0.0f;
			queue_redraw();
			accept_event();
			return;
		}
		if (k->is_action("ui_page_up", true)) {
			code_completion_current_selected = MAX(0, code_completion_current_selected - theme_cache.code_completion_max_lines);
			code_completion_force_item_center = -1;
			code_completion_pan_offset = 0.0f;
			queue_redraw();
			accept_event();
			return;
		}
		if (k->is_action("ui_page_down", true)) {
			code_completion_current_selected = MIN(code_completion_options.size() - 1, code_completion_current_selected + theme_cache.code_completion_max_lines);
			code_completion_force_item_center = -1;
			code_completion_pan_offset = 0.0f;
			queue_redraw();
			accept_event();
			return;
		}
		if (k->is_action("ui_text_caret_line_start", true) || k->is_action("ui_text_caret_line_end", true)) {
			cancel_code_completion();
		}
		if (k->is_action("ui_text_completion_replace", true) || k->is_action("ui_text_completion_accept", true)) {
			confirm_code_completion(k->is_action("ui_text_completion_replace", true));
			accept_event();
			return;
		}
		if (k->is_action("ui_cancel", true)) {
			cancel_code_completion();
			accept_event();
			return;
		}
		if (k->is_action("ui_text_backspace", true)) {
			backspace();
			_filter_code_completion_candidates_impl();
			accept_event();
			return;
		}

		if (k->is_action("ui_left", true) || k->is_action("ui_right", true)) {
			update_code_completion = true;
		} else {
			update_code_completion = (allow_unicode_handling && k->get_unicode() >= 32);
		}
	}

	/* MISC */
	if (!code_hint.is_empty() && k->is_action("ui_cancel", true)) {
		set_code_hint("");
		accept_event();
		return;
	}
	if (allow_unicode_handling && k->get_unicode() == ')') {
		set_code_hint("");
	}

	/* Indentation */
	if (k->is_action("ui_text_indent", true)) {
		do_indent();
		accept_event();
		return;
	}

	if (k->is_action("ui_text_dedent", true)) {
		unindent_lines();
		accept_event();
		return;
	}

	// Override new line actions, for auto indent.
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

	// Remove shift, otherwise actions will not match.
	k = k->duplicate();
	k->set_shift_pressed(false);

	if (k->is_action("ui_text_caret_up", true) ||
			k->is_action("ui_text_caret_down", true) ||
			k->is_action("ui_text_caret_line_start", true) ||
			k->is_action("ui_text_caret_line_end", true) ||
			k->is_action("ui_text_caret_page_up", true) ||
			k->is_action("ui_text_caret_page_down", true)) {
		set_code_hint("");
	}

	TextEdit::gui_input(p_gui_input);

	if (update_code_completion) {
		_filter_code_completion_candidates_impl();
	}
}

/* General overrides */
Control::CursorShape CodeEdit::get_cursor_shape(const Point2 &p_pos) const {
	if (!symbol_lookup_word.is_empty()) {
		return CURSOR_POINTING_HAND;
	}

	if ((code_completion_active && code_completion_rect.has_point(p_pos)) || (!is_editable() && (!is_selecting_enabled() || get_line_count() == 0))) {
		return CURSOR_ARROW;
	}

	if (code_completion_active && code_completion_scroll_rect.has_point(p_pos)) {
		return CURSOR_ARROW;
	}

	Point2i pos = get_line_column_at_pos(p_pos, false);
	int line = pos.y;
	int col = pos.x;

	if (line != -1 && is_line_folded(line)) {
		int wrap_index = get_line_wrap_index_at_column(line, col);
		if (wrap_index == get_line_wrap_count(line)) {
			int eol_icon_width = theme_cache.folded_eol_icon->get_width();
			int left_margin = get_total_gutter_width() + eol_icon_width + get_line_width(line, wrap_index) - get_h_scroll();
			if (p_pos.x > left_margin && p_pos.x <= left_margin + eol_icon_width + 3) {
				return CURSOR_POINTING_HAND;
			}
		}
	}

	return TextEdit::get_cursor_shape(p_pos);
}

void CodeEdit::_unhide_carets() {
	// Unfold caret and selection origin.
	for (int i = 0; i < get_caret_count(); i++) {
		if (_is_line_hidden(get_caret_line(i))) {
			unfold_line(get_caret_line(i));
		}
		if (has_selection(i) && _is_line_hidden(get_selection_origin_line(i))) {
			unfold_line(get_selection_origin_line(i));
		}
	}
}

/* Text manipulation */

// Overridable actions
void CodeEdit::_handle_unicode_input_internal(const uint32_t p_unicode, int p_caret) {
	start_action(EditAction::ACTION_TYPING);
	begin_multicaret_edit();
	for (int i = 0; i < get_caret_count(); i++) {
		if (p_caret != -1 && p_caret != i) {
			continue;
		}
		if (p_caret == -1 && multicaret_edit_ignore_caret(i)) {
			continue;
		}

		bool had_selection = has_selection(i);
		String selection_text = (had_selection ? get_selected_text(i) : "");

		if (had_selection) {
			delete_selection(i);
		}

		// Remove the old character if in overtype mode and no selection.
		if (is_overtype_mode_enabled() && !had_selection) {
			// Make sure we don't try and remove empty space.
			if (get_caret_column(i) < get_line(get_caret_line(i)).length()) {
				remove_text(get_caret_line(i), get_caret_column(i), get_caret_line(i), get_caret_column(i) + 1);
			}
		}

		const char32_t chr[2] = { (char32_t)p_unicode, 0 };

		if (auto_brace_completion_enabled) {
			int cl = get_caret_line(i);
			int cc = get_caret_column(i);

			if (had_selection) {
				insert_text_at_caret(chr, i);

				String close_key = get_auto_brace_completion_close_key(chr);
				if (!close_key.is_empty()) {
					insert_text_at_caret(selection_text + close_key, i);
					set_caret_column(get_caret_column(i) - 1, i == 0, i);
				}
			} else {
				int caret_move_offset = 1;

				int post_brace_pair = cc < get_line(cl).length() ? _get_auto_brace_pair_close_at_pos(cl, cc) : -1;

				if (has_string_delimiter(chr) && cc > 0 && !is_symbol(get_line(cl)[cc - 1]) && post_brace_pair == -1) {
					insert_text_at_caret(chr, i);
				} else if (cc < get_line(cl).length() && !is_symbol(get_line(cl)[cc])) {
					insert_text_at_caret(chr, i);
				} else if (post_brace_pair != -1 && auto_brace_completion_pairs[post_brace_pair].close_key[0] == chr[0]) {
					caret_move_offset = auto_brace_completion_pairs[post_brace_pair].close_key.length();
				} else if (is_in_comment(cl, cc) != -1 || (is_in_string(cl, cc) != -1 && has_string_delimiter(chr))) {
					insert_text_at_caret(chr, i);
				} else {
					insert_text_at_caret(chr, i);

					int pre_brace_pair = _get_auto_brace_pair_open_at_pos(cl, cc + 1);
					if (pre_brace_pair != -1) {
						insert_text_at_caret(auto_brace_completion_pairs[pre_brace_pair].close_key, i);
					}
				}
				set_caret_column(cc + caret_move_offset, i == 0, i);
			}
		} else {
			insert_text_at_caret(chr, i);
		}
	}
	end_multicaret_edit();
	end_action();
}

void CodeEdit::_backspace_internal(int p_caret) {
	if (!is_editable()) {
		return;
	}

	if (has_selection(p_caret)) {
		delete_selection(p_caret);
		return;
	}

	begin_complex_operation();
	begin_multicaret_edit();
	for (int i = 0; i < get_caret_count(); i++) {
		if (p_caret != -1 && p_caret != i) {
			continue;
		}
		if (p_caret == -1 && multicaret_edit_ignore_caret(i)) {
			continue;
		}

		int to_line = get_caret_line(i);
		int to_column = get_caret_column(i);

		if (to_column == 0 && to_line == 0) {
			continue;
		}

		if (to_line > 0 && to_column == 0 && _is_line_hidden(to_line - 1)) {
			unfold_line(to_line - 1);
		}

		int from_line = to_column > 0 ? to_line : to_line - 1;
		int from_column = to_column > 0 ? (to_column - 1) : (get_line(to_line - 1).length());

		merge_gutters(from_line, to_line);

		if (auto_brace_completion_enabled && to_column > 0) {
			int idx = _get_auto_brace_pair_open_at_pos(to_line, to_column);
			if (idx != -1) {
				from_column = to_column - auto_brace_completion_pairs[idx].open_key.length();

				if (_get_auto_brace_pair_close_at_pos(to_line, to_column) == idx) {
					to_column += auto_brace_completion_pairs[idx].close_key.length();
				}
			}
		}

		// For space indentation we need to do a basic unindent if there are no chars to the left, acting the same way as tabs.
		if (indent_using_spaces && to_column != 0) {
			if (get_first_non_whitespace_column(to_line) >= to_column) {
				from_column = to_column - _calculate_spaces_till_next_left_indent(to_column);
				from_line = to_line;
			}
		}

		remove_text(from_line, from_column, to_line, to_column);

		set_caret_line(from_line, false, true, -1, i);
		set_caret_column(from_column, i == 0, i);
	}

	end_multicaret_edit();
	end_complex_operation();
}

void CodeEdit::_cut_internal(int p_caret) {
	// Overridden to unfold lines.
	_copy_internal(p_caret);

	if (!is_editable()) {
		return;
	}

	if (has_selection(p_caret)) {
		delete_selection(p_caret);
		return;
	}
	if (!is_empty_selection_clipboard_enabled()) {
		return;
	}
	if (p_caret == -1) {
		delete_lines();
	} else {
		unfold_line(get_caret_line(p_caret));
		remove_line_at(get_caret_line(p_caret));
	}
}

/* Indent management */
void CodeEdit::set_indent_size(const int p_size) {
	ERR_FAIL_COND_MSG(p_size <= 0, "Indend size must be greater than 0.");
	if (indent_size == p_size) {
		return;
	}

	indent_size = p_size;
	if (indent_using_spaces) {
		indent_text = String(" ").repeat(p_size);
	} else {
		indent_text = "\t";
	}
	set_tab_size(p_size);
}

int CodeEdit::get_indent_size() const {
	return indent_size;
}

void CodeEdit::set_indent_using_spaces(const bool p_use_spaces) {
	indent_using_spaces = p_use_spaces;
	if (indent_using_spaces) {
		indent_text = String(" ").repeat(indent_size);
	} else {
		indent_text = "\t";
	}
}

bool CodeEdit::is_indent_using_spaces() const {
	return indent_using_spaces;
}

void CodeEdit::set_auto_indent_enabled(bool p_enabled) {
	auto_indent = p_enabled;
}

bool CodeEdit::is_auto_indent_enabled() const {
	return auto_indent;
}

void CodeEdit::set_auto_indent_prefixes(const TypedArray<String> &p_prefixes) {
	auto_indent_prefixes.clear();
	for (int i = 0; i < p_prefixes.size(); i++) {
		const String prefix = p_prefixes[i];
		auto_indent_prefixes.insert(prefix[0]);
	}
}

TypedArray<String> CodeEdit::get_auto_indent_prefixes() const {
	TypedArray<String> prefixes;
	for (const char32_t &E : auto_indent_prefixes) {
		prefixes.push_back(String::chr(E));
	}
	return prefixes;
}

void CodeEdit::do_indent() {
	if (!is_editable()) {
		return;
	}

	if (has_selection()) {
		indent_lines();
		return;
	}

	if (!indent_using_spaces) {
		insert_text_at_caret("\t");
		return;
	}

	begin_complex_operation();
	begin_multicaret_edit();
	for (int i = 0; i < get_caret_count(); i++) {
		if (multicaret_edit_ignore_caret(i)) {
			continue;
		}
		int spaces_to_add = _calculate_spaces_till_next_right_indent(get_caret_column(i));
		if (spaces_to_add > 0) {
			insert_text_at_caret(String(" ").repeat(spaces_to_add), i);
		}
	}
	end_multicaret_edit();
	end_complex_operation();
}

void CodeEdit::indent_lines() {
	if (!is_editable()) {
		return;
	}

	begin_complex_operation();
	begin_multicaret_edit();

	Vector<Point2i> line_ranges = get_line_ranges_from_carets();
	for (Point2i line_range : line_ranges) {
		for (int i = line_range.x; i <= line_range.y; i++) {
			const String line_text = get_line(i);
			if (line_text.size() == 0) {
				// Ignore empty lines.
				continue;
			}

			if (indent_using_spaces) {
				int spaces_to_add = _calculate_spaces_till_next_right_indent(get_first_non_whitespace_column(i));
				insert_text(String(" ").repeat(spaces_to_add), i, 0, false);
			} else {
				insert_text("\t", i, 0, false);
			}
		}
	}

	end_multicaret_edit();
	end_complex_operation();
}

void CodeEdit::unindent_lines() {
	if (!is_editable()) {
		return;
	}

	begin_complex_operation();
	begin_multicaret_edit();

	Vector<Point2i> line_ranges = get_line_ranges_from_carets();
	for (Point2i line_range : line_ranges) {
		for (int i = line_range.x; i <= line_range.y; i++) {
			const String line_text = get_line(i);

			if (line_text.begins_with("\t")) {
				remove_text(i, 0, i, 1);
			} else if (line_text.begins_with(" ")) {
				// Remove only enough spaces to align text to nearest full multiple of indentation_size.
				int spaces_to_remove = _calculate_spaces_till_next_left_indent(get_first_non_whitespace_column(i));
				remove_text(i, 0, i, spaces_to_remove);
			}
		}
	}

	end_multicaret_edit();
	end_complex_operation();
}

void CodeEdit::convert_indent(int p_from_line, int p_to_line) {
	if (!is_editable()) {
		return;
	}

	// Check line range.
	p_from_line = (p_from_line < 0) ? 0 : p_from_line;
	p_to_line = (p_to_line < 0) ? get_line_count() - 1 : p_to_line;

	ERR_FAIL_COND(p_from_line >= get_line_count());
	ERR_FAIL_COND(p_to_line >= get_line_count());
	ERR_FAIL_COND(p_to_line < p_from_line);

	// Check lines within range.
	const char32_t from_indent_char = indent_using_spaces ? '\t' : ' ';
	int size_diff = indent_using_spaces ? indent_size - 1 : -(indent_size - 1);
	bool changed_indentation = false;
	for (int i = p_from_line; i <= p_to_line; i++) {
		String line = get_line(i);

		if (line.length() <= 0) {
			continue;
		}

		// Check chars in the line.
		int j = 0;
		int space_count = 0;
		bool line_changed = false;
		while (j < line.length() && (line[j] == ' ' || line[j] == '\t')) {
			if (line[j] != from_indent_char) {
				space_count = 0;
				j++;
				continue;
			}
			space_count++;

			if (!indent_using_spaces && space_count != indent_size) {
				j++;
				continue;
			}

			line_changed = true;
			if (!changed_indentation) {
				begin_complex_operation();
				begin_multicaret_edit();
				changed_indentation = true;
			}

			// Calculate new line.
			line = line.left(j + ((size_diff < 0) ? size_diff : 0)) + indent_text + line.substr(j + 1);

			space_count = 0;
			j += size_diff;
		}

		if (line_changed) {
			// Use set line to preserve carets visual position.
			set_line(i, line);
		}
	}

	if (!changed_indentation) {
		return;
	}

	merge_overlapping_carets();
	end_multicaret_edit();
	end_complex_operation();
}

int CodeEdit::_calculate_spaces_till_next_left_indent(int p_column) const {
	int spaces_till_indent = p_column % indent_size;
	if (spaces_till_indent == 0) {
		spaces_till_indent = indent_size;
	}
	return spaces_till_indent;
}

int CodeEdit::_calculate_spaces_till_next_right_indent(int p_column) const {
	return indent_size - p_column % indent_size;
}

void CodeEdit::_new_line(bool p_split_current_line, bool p_above) {
	if (!is_editable()) {
		return;
	}

	begin_complex_operation();
	begin_multicaret_edit();

	for (int i = 0; i < get_caret_count(); i++) {
		if (multicaret_edit_ignore_caret(i)) {
			continue;
		}
		// When not splitting the line, we need to factor in indentation from the end of the current line.
		const int cc = p_split_current_line ? get_caret_column(i) : get_line(get_caret_line(i)).length();
		const int cl = get_caret_line(i);

		const String line = get_line(cl);

		String ins = "";
		if (!p_above) {
			ins = "\n";
		}

		// Append current indentation.
		int space_count = 0;
		int line_col = 0;
		for (; line_col < cc; line_col++) {
			if (line[line_col] == '\t') {
				ins += indent_text;
				space_count = 0;
				continue;
			}

			if (line[line_col] == ' ') {
				space_count++;

				if (space_count == indent_size) {
					ins += indent_text;
					space_count = 0;
				}
				continue;
			}
			break;
		}
		if (p_above) {
			ins += "\n";
		}

		if (is_line_folded(cl)) {
			unfold_line(cl);
		}

		// Indent once again if the previous line needs it, ie ':'.
		// Then add an addition new line for any closing pairs aka '()'.
		// Skip this in comments or if we are going above.
		bool brace_indent = false;
		if (auto_indent && !p_above && cc > 0 && is_in_comment(cl) == -1) {
			bool should_indent = false;
			char32_t indent_char = ' ';

			for (; line_col < cc; line_col++) {
				char32_t c = line[line_col];
				if (auto_indent_prefixes.has(c) && is_in_comment(cl, line_col) == -1) {
					should_indent = true;
					indent_char = c;
					continue;
				}

				// Make sure this is the last char, trailing whitespace or comments are okay.
				// Increment column for comments because the delimiter (#) should be ignored.
				if (should_indent && (!is_whitespace(c) && is_in_comment(cl, line_col + 1) == -1)) {
					should_indent = false;
				}
			}

			if (should_indent) {
				ins += indent_text;

				String closing_pair = get_auto_brace_completion_close_key(String::chr(indent_char));
				if (!closing_pair.is_empty() && line.find(closing_pair, cc) == cc) {
					// No need to move the brace below if we are not taking the text with us.
					if (p_split_current_line) {
						brace_indent = true;
						ins += "\n" + ins.substr(indent_text.size(), ins.length() - 2);
					} else {
						brace_indent = false;
						ins = "\n" + ins.substr(indent_text.size(), ins.length() - 2);
					}
				}
			}
		}

		if (p_split_current_line) {
			insert_text_at_caret(ins, i);
		} else {
			insert_text(ins, cl, p_above ? 0 : get_line(cl).length(), p_above, p_above);
			deselect(i);
			set_caret_line(p_above ? cl : cl + 1, false, true, -1, i);
			set_caret_column(get_line(get_caret_line(i)).length(), i == 0, i);
		}
		if (brace_indent) {
			// Move to inner indented line.
			set_caret_line(get_caret_line(i) - 1, false, true, 0, i);
			set_caret_column(get_line(get_caret_line(i)).length(), i == 0, i);
		}
	}

	end_multicaret_edit();
	end_complex_operation();
}

/* Auto brace completion */
void CodeEdit::set_auto_brace_completion_enabled(bool p_enabled) {
	auto_brace_completion_enabled = p_enabled;
}

bool CodeEdit::is_auto_brace_completion_enabled() const {
	return auto_brace_completion_enabled;
}

void CodeEdit::set_highlight_matching_braces_enabled(bool p_enabled) {
	highlight_matching_braces_enabled = p_enabled;
	queue_redraw();
}

bool CodeEdit::is_highlight_matching_braces_enabled() const {
	return highlight_matching_braces_enabled;
}

void CodeEdit::add_auto_brace_completion_pair(const String &p_open_key, const String &p_close_key) {
	ERR_FAIL_COND_MSG(p_open_key.is_empty(), "auto brace completion open key cannot be empty");
	ERR_FAIL_COND_MSG(p_close_key.is_empty(), "auto brace completion close key cannot be empty");

	for (int i = 0; i < p_open_key.length(); i++) {
		ERR_FAIL_COND_MSG(!is_symbol(p_open_key[i]), "auto brace completion open key must be a symbol");
	}
	for (int i = 0; i < p_close_key.length(); i++) {
		ERR_FAIL_COND_MSG(!is_symbol(p_close_key[i]), "auto brace completion close key must be a symbol");
	}

	int at = 0;
	for (int i = 0; i < auto_brace_completion_pairs.size(); i++) {
		ERR_FAIL_COND_MSG(auto_brace_completion_pairs[i].open_key == p_open_key, "auto brace completion open key '" + p_open_key + "' already exists.");
		if (p_open_key.length() < auto_brace_completion_pairs[i].open_key.length()) {
			at++;
		}
	}

	BracePair brace_pair;
	brace_pair.open_key = p_open_key;
	brace_pair.close_key = p_close_key;
	auto_brace_completion_pairs.insert(at, brace_pair);
}

void CodeEdit::set_auto_brace_completion_pairs(const Dictionary &p_auto_brace_completion_pairs) {
	auto_brace_completion_pairs.clear();

	Array keys = p_auto_brace_completion_pairs.keys();
	for (int i = 0; i < keys.size(); i++) {
		add_auto_brace_completion_pair(keys[i], p_auto_brace_completion_pairs[keys[i]]);
	}
}

Dictionary CodeEdit::get_auto_brace_completion_pairs() const {
	Dictionary brace_pairs;
	for (int i = 0; i < auto_brace_completion_pairs.size(); i++) {
		brace_pairs[auto_brace_completion_pairs[i].open_key] = auto_brace_completion_pairs[i].close_key;
	}
	return brace_pairs;
}

bool CodeEdit::has_auto_brace_completion_open_key(const String &p_open_key) const {
	for (int i = 0; i < auto_brace_completion_pairs.size(); i++) {
		if (auto_brace_completion_pairs[i].open_key == p_open_key) {
			return true;
		}
	}
	return false;
}

bool CodeEdit::has_auto_brace_completion_close_key(const String &p_close_key) const {
	for (int i = 0; i < auto_brace_completion_pairs.size(); i++) {
		if (auto_brace_completion_pairs[i].close_key == p_close_key) {
			return true;
		}
	}
	return false;
}

String CodeEdit::get_auto_brace_completion_close_key(const String &p_open_key) const {
	for (int i = 0; i < auto_brace_completion_pairs.size(); i++) {
		if (auto_brace_completion_pairs[i].open_key == p_open_key) {
			return auto_brace_completion_pairs[i].close_key;
		}
	}
	return String();
}

/* Main Gutter */
void CodeEdit::_update_draw_main_gutter() {
	set_gutter_draw(main_gutter, draw_breakpoints || draw_bookmarks || draw_executing_lines);
}

void CodeEdit::set_draw_breakpoints_gutter(bool p_draw) {
	draw_breakpoints = p_draw;
	set_gutter_clickable(main_gutter, p_draw);
	_update_draw_main_gutter();
}

bool CodeEdit::is_drawing_breakpoints_gutter() const {
	return draw_breakpoints;
}

void CodeEdit::set_draw_bookmarks_gutter(bool p_draw) {
	draw_bookmarks = p_draw;
	_update_draw_main_gutter();
}

bool CodeEdit::is_drawing_bookmarks_gutter() const {
	return draw_bookmarks;
}

void CodeEdit::set_draw_executing_lines_gutter(bool p_draw) {
	draw_executing_lines = p_draw;
	_update_draw_main_gutter();
}

bool CodeEdit::is_drawing_executing_lines_gutter() const {
	return draw_executing_lines;
}

void CodeEdit::_main_gutter_draw_callback(int p_line, int p_gutter, const Rect2 &p_region) {
	bool hovering = get_hovered_gutter() == Vector2i(main_gutter, p_line);
	if (draw_breakpoints && theme_cache.breakpoint_icon.is_valid()) {
		bool breakpointed = is_line_breakpointed(p_line);
		bool shift_pressed = Input::get_singleton()->is_key_pressed(Key::SHIFT);

		if (breakpointed || (hovering && !is_dragging_cursor() && !shift_pressed)) {
			int padding = p_region.size.x / 6;

			Color use_color = theme_cache.breakpoint_color;
			if (hovering && !shift_pressed) {
				use_color = breakpointed ? use_color.lightened(0.3) : use_color.darkened(0.5);
			}
			Rect2 icon_region = p_region;
			icon_region.position += Point2(padding, padding);
			icon_region.size -= Point2(padding, padding) * 2;
			theme_cache.breakpoint_icon->draw_rect(get_canvas_item(), icon_region, false, use_color);
		}
	}

	if (draw_bookmarks && theme_cache.bookmark_icon.is_valid()) {
		bool bookmarked = is_line_bookmarked(p_line);
		bool shift_pressed = Input::get_singleton()->is_key_pressed(Key::SHIFT);

		if (bookmarked || (hovering && !is_dragging_cursor() && shift_pressed)) {
			int horizontal_padding = p_region.size.x / 2;
			int vertical_padding = p_region.size.y / 4;

			Color use_color = theme_cache.bookmark_color;
			if (hovering && shift_pressed) {
				use_color = bookmarked ? use_color.lightened(0.3) : use_color.darkened(0.5);
			}
			Rect2 icon_region = p_region;
			icon_region.position += Point2(horizontal_padding, 0);
			icon_region.size -= Point2(horizontal_padding * 1.1, vertical_padding);
			theme_cache.bookmark_icon->draw_rect(get_canvas_item(), icon_region, false, use_color);
		}
	}

	if (draw_executing_lines && is_line_executing(p_line) && theme_cache.executing_line_icon.is_valid()) {
		int horizontal_padding = p_region.size.x / 10;
		int vertical_padding = p_region.size.y / 4;

		Rect2 icon_region = p_region;
		icon_region.position += Point2(horizontal_padding, vertical_padding);
		icon_region.size -= Point2(horizontal_padding, vertical_padding) * 2;
		theme_cache.executing_line_icon->draw_rect(get_canvas_item(), icon_region, false, theme_cache.executing_line_color);
	}
}

// Breakpoints
void CodeEdit::set_line_as_breakpoint(int p_line, bool p_breakpointed) {
	ERR_FAIL_INDEX(p_line, get_line_count());

	int mask = get_line_gutter_metadata(p_line, main_gutter);
	set_line_gutter_metadata(p_line, main_gutter, p_breakpointed ? mask | MAIN_GUTTER_BREAKPOINT : mask & ~MAIN_GUTTER_BREAKPOINT);
	if (p_breakpointed) {
		breakpointed_lines[p_line] = true;
	} else if (breakpointed_lines.has(p_line)) {
		breakpointed_lines.erase(p_line);
	}
	emit_signal(SNAME("breakpoint_toggled"), p_line);
	queue_redraw();
}

bool CodeEdit::is_line_breakpointed(int p_line) const {
	return (int)get_line_gutter_metadata(p_line, main_gutter) & MAIN_GUTTER_BREAKPOINT;
}

void CodeEdit::clear_breakpointed_lines() {
	for (int i = 0; i < get_line_count(); i++) {
		if (is_line_breakpointed(i)) {
			set_line_as_breakpoint(i, false);
		}
	}
}

PackedInt32Array CodeEdit::get_breakpointed_lines() const {
	PackedInt32Array ret;
	for (int i = 0; i < get_line_count(); i++) {
		if (is_line_breakpointed(i)) {
			ret.append(i);
		}
	}
	return ret;
}

// Bookmarks
void CodeEdit::set_line_as_bookmarked(int p_line, bool p_bookmarked) {
	int mask = get_line_gutter_metadata(p_line, main_gutter);
	set_line_gutter_metadata(p_line, main_gutter, p_bookmarked ? mask | MAIN_GUTTER_BOOKMARK : mask & ~MAIN_GUTTER_BOOKMARK);
	queue_redraw();
}

bool CodeEdit::is_line_bookmarked(int p_line) const {
	return (int)get_line_gutter_metadata(p_line, main_gutter) & MAIN_GUTTER_BOOKMARK;
}

void CodeEdit::clear_bookmarked_lines() {
	for (int i = 0; i < get_line_count(); i++) {
		if (is_line_bookmarked(i)) {
			set_line_as_bookmarked(i, false);
		}
	}
}

PackedInt32Array CodeEdit::get_bookmarked_lines() const {
	PackedInt32Array ret;
	for (int i = 0; i < get_line_count(); i++) {
		if (is_line_bookmarked(i)) {
			ret.append(i);
		}
	}
	return ret;
}

// Executing lines
void CodeEdit::set_line_as_executing(int p_line, bool p_executing) {
	int mask = get_line_gutter_metadata(p_line, main_gutter);
	set_line_gutter_metadata(p_line, main_gutter, p_executing ? mask | MAIN_GUTTER_EXECUTING : mask & ~MAIN_GUTTER_EXECUTING);
	queue_redraw();
}

bool CodeEdit::is_line_executing(int p_line) const {
	return (int)get_line_gutter_metadata(p_line, main_gutter) & MAIN_GUTTER_EXECUTING;
}

void CodeEdit::clear_executing_lines() {
	for (int i = 0; i < get_line_count(); i++) {
		if (is_line_executing(i)) {
			set_line_as_executing(i, false);
		}
	}
}

PackedInt32Array CodeEdit::get_executing_lines() const {
	PackedInt32Array ret;
	for (int i = 0; i < get_line_count(); i++) {
		if (is_line_executing(i)) {
			ret.append(i);
		}
	}
	return ret;
}

/* Line numbers */
void CodeEdit::set_draw_line_numbers(bool p_draw) {
	set_gutter_draw(line_number_gutter, p_draw);
}

bool CodeEdit::is_draw_line_numbers_enabled() const {
	return is_gutter_drawn(line_number_gutter);
}

void CodeEdit::set_line_numbers_zero_padded(bool p_zero_padded) {
	String new_line_number_padding = p_zero_padded ? "0" : " ";
	if (line_number_padding == new_line_number_padding) {
		return;
	}

	line_number_padding = new_line_number_padding;
	_clear_line_number_text_cache();
	queue_redraw();
}

bool CodeEdit::is_line_numbers_zero_padded() const {
	return line_number_padding == "0";
}

void CodeEdit::_line_number_draw_callback(int p_line, int p_gutter, const Rect2 &p_region) {
	if (!Rect2(Vector2(0, 0), get_size()).intersects(p_region)) {
		return;
	}

	bool rtl = is_layout_rtl();
	HashMap<int, RID>::Iterator E = line_number_text_cache.find(p_line);
	RID text_rid;
	if (E) {
		text_rid = E->value;
	} else {
		String fc = String::num_int64(p_line + 1).lpad(line_number_digits, line_number_padding);
		if (is_localizing_numeral_system()) {
			fc = TS->format_number(fc);
		}

		text_rid = TS->create_shaped_text();
		if (theme_cache.font.is_valid()) {
			TS->shaped_text_add_string(text_rid, fc, theme_cache.font->get_rids(), theme_cache.font_size, theme_cache.font->get_opentype_features());
		}
		line_number_text_cache.insert(p_line, text_rid);
	}

	Size2 text_size = TS->shaped_text_get_size(text_rid);
	Point2 ofs = p_region.get_center() - text_size / 2;
	ofs.y += TS->shaped_text_get_ascent(text_rid);

	if (rtl) {
		ofs.x = p_region.get_end().x - text_size.width;
	} else {
		ofs.x = p_region.position.x;
	}

	Color number_color = get_line_gutter_item_color(p_line, line_number_gutter);
	if (number_color == Color(1, 1, 1)) {
		number_color = theme_cache.line_number_color;
	}

	TS->shaped_text_draw(text_rid, get_canvas_item(), ofs, -1, -1, number_color);
}

void CodeEdit::_clear_line_number_text_cache() {
	for (const KeyValue<int, RID> &KV : line_number_text_cache) {
		TS->free_rid(KV.value);
	}
	line_number_text_cache.clear();
}

void CodeEdit::_update_line_number_gutter_width() {
	set_gutter_width(line_number_gutter, (line_number_digits + 1) * theme_cache.font->get_char_size('0', theme_cache.font_size).width);
}

/* Fold Gutter */
void CodeEdit::set_draw_fold_gutter(bool p_draw) {
	set_gutter_draw(fold_gutter, p_draw);
}

bool CodeEdit::is_drawing_fold_gutter() const {
	return is_gutter_drawn(fold_gutter);
}

void CodeEdit::_fold_gutter_draw_callback(int p_line, int p_gutter, Rect2 p_region) {
	if (!can_fold_line(p_line) && !is_line_folded(p_line)) {
		set_line_gutter_clickable(p_line, fold_gutter, false);
		return;
	}
	set_line_gutter_clickable(p_line, fold_gutter, true);

	int horizontal_padding = p_region.size.x / 10;
	int vertical_padding = p_region.size.y / 6;

	p_region.position += Point2(horizontal_padding, vertical_padding);
	p_region.size -= Point2(horizontal_padding, vertical_padding) * 2;

	bool can_fold = can_fold_line(p_line);

	if (is_line_code_region_start(p_line)) {
		Color region_icon_color = theme_cache.folded_code_region_color;
		region_icon_color.a = MAX(region_icon_color.a, 0.4f);
		if (can_fold) {
			theme_cache.can_fold_code_region_icon->draw_rect(get_canvas_item(), p_region, false, region_icon_color);
		} else {
			theme_cache.folded_code_region_icon->draw_rect(get_canvas_item(), p_region, false, region_icon_color);
		}
		return;
	}
	if (can_fold) {
		theme_cache.can_fold_icon->draw_rect(get_canvas_item(), p_region, false, theme_cache.code_folding_color);
		return;
	}
	theme_cache.folded_icon->draw_rect(get_canvas_item(), p_region, false, theme_cache.code_folding_color);
}

/* Line Folding */
void CodeEdit::set_line_folding_enabled(bool p_enabled) {
	line_folding_enabled = p_enabled;
	_set_hiding_enabled(p_enabled);
}

bool CodeEdit::is_line_folding_enabled() const {
	return line_folding_enabled;
}

bool CodeEdit::can_fold_line(int p_line) const {
	ERR_FAIL_INDEX_V(p_line, get_line_count(), false);
	if (!line_folding_enabled) {
		return false;
	}

	if (p_line + 1 >= get_line_count() || get_line(p_line).strip_edges().size() == 0) {
		return false;
	}

	if (_is_line_hidden(p_line) || is_line_folded(p_line)) {
		return false;
	}

	// Check for code region.
	if (is_line_code_region_end(p_line)) {
		return false;
	}
	if (is_line_code_region_start(p_line)) {
		int region_level = 0;
		// Check if there is a valid end region tag.
		for (int next_line = p_line + 1; next_line < get_line_count(); next_line++) {
			if (is_line_code_region_end(next_line)) {
				region_level -= 1;
				if (region_level == -1) {
					return true;
				}
			}
			if (is_line_code_region_start(next_line)) {
				region_level += 1;
			}
		}
		return false;
	}

	/* Check for full multiline line or block strings / comments. */
	int in_comment = is_in_comment(p_line);
	int in_string = (in_comment == -1) ? is_in_string(p_line) : -1;
	if (in_string != -1 || in_comment != -1) {
		if (get_delimiter_start_position(p_line, get_line(p_line).size() - 1).y != p_line) {
			return false;
		}

		int delimiter_end_line = get_delimiter_end_position(p_line, get_line(p_line).size() - 1).y;
		/* No end line, therefore we have a multiline region over the rest of the file. */
		if (delimiter_end_line == -1) {
			return true;
		}
		/* End line is the same therefore we have a block. */
		if (delimiter_end_line == p_line) {
			/* Check we are the start of the block. */
			if (p_line - 1 >= 0) {
				if ((in_string != -1 && is_in_string(p_line - 1) != -1) || (in_comment != -1 && is_in_comment(p_line - 1) != -1)) {
					return false;
				}
			}
			/* Check it continues for at least one line. */
			return ((in_string != -1 && is_in_string(p_line + 1) != -1) || (in_comment != -1 && is_in_comment(p_line + 1) != -1));
		}
		return ((in_string != -1 && is_in_string(delimiter_end_line) != -1) || (in_comment != -1 && is_in_comment(delimiter_end_line) != -1));
	}

	/* Otherwise check indent levels. */
	int start_indent = get_indent_level(p_line);
	for (int i = p_line + 1; i < get_line_count(); i++) {
		if (is_in_string(i) != -1 || is_in_comment(i) != -1 || get_line(i).strip_edges().size() == 0) {
			continue;
		}
		return (get_indent_level(i) > start_indent);
	}
	return false;
}

void CodeEdit::fold_line(int p_line) {
	ERR_FAIL_INDEX(p_line, get_line_count());
	if (!is_line_folding_enabled() || !can_fold_line(p_line)) {
		return;
	}

	/* Find the last line to be hidden. */
	const int line_count = get_line_count() - 1;
	int end_line = line_count;

	// Fold code region.
	if (is_line_code_region_start(p_line)) {
		int region_level = 0;
		for (int endregion_line = p_line + 1; endregion_line < get_line_count(); endregion_line++) {
			if (is_line_code_region_start(endregion_line)) {
				region_level += 1;
			}
			if (is_line_code_region_end(endregion_line)) {
				region_level -= 1;
				if (region_level == -1) {
					end_line = endregion_line;
					break;
				}
			}
		}
		set_line_background_color(p_line, theme_cache.folded_code_region_color);
	}

	int in_comment = is_in_comment(p_line);
	int in_string = (in_comment == -1) ? is_in_string(p_line) : -1;
	if (!is_line_code_region_start(p_line)) {
		if (in_string != -1 || in_comment != -1) {
			end_line = get_delimiter_end_position(p_line, get_line(p_line).size() - 1).y;
			// End line is the same therefore we have a block of single line delimiters.
			if (end_line == p_line) {
				for (int i = p_line + 1; i <= line_count; i++) {
					if ((in_string != -1 && is_in_string(i) == -1) || (in_comment != -1 && is_in_comment(i) == -1)) {
						break;
					}
					end_line = i;
				}
			}
		} else {
			int start_indent = get_indent_level(p_line);
			for (int i = p_line + 1; i <= line_count; i++) {
				if (get_line(i).strip_edges().size() == 0) {
					continue;
				}
				if (get_indent_level(i) > start_indent) {
					end_line = i;
					continue;
				}
				if (is_in_string(i) == -1 && is_in_comment(i) == -1) {
					break;
				}
			}
		}
	}

	for (int i = p_line + 1; i <= end_line; i++) {
		_set_line_as_hidden(i, true);
	}

	// Collapse any carets in the hidden area.
	collapse_carets(p_line, get_line(p_line).length(), end_line, get_line(end_line).length(), true);
}

void CodeEdit::unfold_line(int p_line) {
	ERR_FAIL_INDEX(p_line, get_line_count());
	if (!is_line_folded(p_line) && !_is_line_hidden(p_line)) {
		return;
	}

	int fold_start = p_line;
	for (; fold_start > 0; fold_start--) {
		if (is_line_folded(fold_start)) {
			break;
		}
	}
	fold_start = is_line_folded(fold_start) ? fold_start : p_line;

	for (int i = fold_start + 1; i < get_line_count(); i++) {
		if (!_is_line_hidden(i)) {
			break;
		}
		_set_line_as_hidden(i, false);
		if (is_line_code_region_start(i - 1)) {
			set_line_background_color(i - 1, Color(0.0, 0.0, 0.0, 0.0));
		}
	}
	queue_redraw();
}

void CodeEdit::fold_all_lines() {
	for (int i = 0; i < get_line_count(); i++) {
		fold_line(i);
	}
	queue_redraw();
}

void CodeEdit::unfold_all_lines() {
	_unhide_all_lines();
}

void CodeEdit::toggle_foldable_line(int p_line) {
	ERR_FAIL_INDEX(p_line, get_line_count());
	if (is_line_folded(p_line)) {
		unfold_line(p_line);
		return;
	}
	fold_line(p_line);
}

void CodeEdit::toggle_foldable_lines_at_carets() {
	begin_multicaret_edit();
	int previous_line = -1;
	Vector<int> sorted = get_sorted_carets();
	for (int caret_idx : sorted) {
		if (multicaret_edit_ignore_caret(caret_idx)) {
			continue;
		}
		int line_idx = get_caret_line(caret_idx);
		if (line_idx != previous_line) {
			toggle_foldable_line(line_idx);
			previous_line = line_idx;
		}
	}
	end_multicaret_edit();
}

bool CodeEdit::is_line_folded(int p_line) const {
	ERR_FAIL_INDEX_V(p_line, get_line_count(), false);
	return p_line + 1 < get_line_count() && !_is_line_hidden(p_line) && _is_line_hidden(p_line + 1);
}

TypedArray<int> CodeEdit::get_folded_lines() const {
	TypedArray<int> folded_lines;
	for (int i = 0; i < get_line_count(); i++) {
		if (is_line_folded(i)) {
			folded_lines.push_back(i);
		}
	}
	return folded_lines;
}

/* Code region */
void CodeEdit::create_code_region() {
	// Abort if there is no selected text.
	if (!has_selection()) {
		return;
	}
	// Check that region tag find a comment delimiter and is valid.
	if (code_region_start_string.is_empty()) {
		WARN_PRINT_ONCE("Cannot create code region without any one line comment delimiters");
		return;
	}
	String region_name = atr(ETR("New Code Region"));

	begin_complex_operation();
	begin_multicaret_edit();
	Vector<Point2i> line_ranges = get_line_ranges_from_carets(true, false);

	// Add start and end region tags.
	int line_offset = 0;
	for (Point2i line_range : line_ranges) {
		insert_text("\n" + code_region_end_string, line_range.y + line_offset, get_line(line_range.y + line_offset).length());
		insert_line_at(line_range.x + line_offset, code_region_start_string + " " + region_name);
		fold_line(line_range.x + line_offset);
		line_offset += 2;
	}
	int first_region_start = line_ranges[0].x;

	// Select name of the first region to allow quick edit.
	remove_secondary_carets();
	int tag_length = code_region_start_string.length() + region_name.length() + 1;
	select(first_region_start, code_region_start_string.length() + 1, first_region_start, tag_length);

	end_multicaret_edit();
	end_complex_operation();
}

String CodeEdit::get_code_region_start_tag() const {
	return code_region_start_tag;
}

String CodeEdit::get_code_region_end_tag() const {
	return code_region_end_tag;
}

void CodeEdit::set_code_region_tags(const String &p_start, const String &p_end) {
	ERR_FAIL_COND_MSG(p_start == p_end, "Starting and ending region tags cannot be identical.");
	ERR_FAIL_COND_MSG(p_start.is_empty(), "Starting region tag cannot be empty.");
	ERR_FAIL_COND_MSG(p_end.is_empty(), "Ending region tag cannot be empty.");
	code_region_start_tag = p_start;
	code_region_end_tag = p_end;
	_update_code_region_tags();
}

bool CodeEdit::is_line_code_region_start(int p_line) const {
	ERR_FAIL_INDEX_V(p_line, get_line_count(), false);
	if (code_region_start_string.is_empty()) {
		return false;
	}
	if (is_in_string(p_line) != -1) {
		return false;
	}
	return get_line(p_line).strip_edges().begins_with(code_region_start_string);
}

bool CodeEdit::is_line_code_region_end(int p_line) const {
	ERR_FAIL_INDEX_V(p_line, get_line_count(), false);
	if (code_region_start_string.is_empty()) {
		return false;
	}
	if (is_in_string(p_line) != -1) {
		return false;
	}
	return get_line(p_line).strip_edges().begins_with(code_region_end_string);
}

/* Delimiters */
// Strings
void CodeEdit::add_string_delimiter(const String &p_start_key, const String &p_end_key, bool p_line_only) {
	_add_delimiter(p_start_key, p_end_key, p_line_only, TYPE_STRING);
}

void CodeEdit::remove_string_delimiter(const String &p_start_key) {
	_remove_delimiter(p_start_key, TYPE_STRING);
}

bool CodeEdit::has_string_delimiter(const String &p_start_key) const {
	return _has_delimiter(p_start_key, TYPE_STRING);
}

void CodeEdit::set_string_delimiters(const TypedArray<String> &p_string_delimiters) {
	_set_delimiters(p_string_delimiters, TYPE_STRING);
}

void CodeEdit::clear_string_delimiters() {
	_clear_delimiters(TYPE_STRING);
}

TypedArray<String> CodeEdit::get_string_delimiters() const {
	return _get_delimiters(TYPE_STRING);
}

int CodeEdit::is_in_string(int p_line, int p_column) const {
	return _is_in_delimiter(p_line, p_column, TYPE_STRING);
}

// Comments
void CodeEdit::add_comment_delimiter(const String &p_start_key, const String &p_end_key, bool p_line_only) {
	_add_delimiter(p_start_key, p_end_key, p_line_only, TYPE_COMMENT);
}

void CodeEdit::remove_comment_delimiter(const String &p_start_key) {
	_remove_delimiter(p_start_key, TYPE_COMMENT);
}

bool CodeEdit::has_comment_delimiter(const String &p_start_key) const {
	return _has_delimiter(p_start_key, TYPE_COMMENT);
}

void CodeEdit::set_comment_delimiters(const TypedArray<String> &p_comment_delimiters) {
	_set_delimiters(p_comment_delimiters, TYPE_COMMENT);
}

void CodeEdit::clear_comment_delimiters() {
	_clear_delimiters(TYPE_COMMENT);
}

TypedArray<String> CodeEdit::get_comment_delimiters() const {
	return _get_delimiters(TYPE_COMMENT);
}

int CodeEdit::is_in_comment(int p_line, int p_column) const {
	return _is_in_delimiter(p_line, p_column, TYPE_COMMENT);
}

String CodeEdit::get_delimiter_start_key(int p_delimiter_idx) const {
	ERR_FAIL_INDEX_V(p_delimiter_idx, delimiters.size(), "");
	return delimiters[p_delimiter_idx].start_key;
}

String CodeEdit::get_delimiter_end_key(int p_delimiter_idx) const {
	ERR_FAIL_INDEX_V(p_delimiter_idx, delimiters.size(), "");
	return delimiters[p_delimiter_idx].end_key;
}

Point2 CodeEdit::get_delimiter_start_position(int p_line, int p_column) const {
	if (delimiters.size() == 0) {
		return Point2(-1, -1);
	}
	ERR_FAIL_INDEX_V(p_line, get_line_count(), Point2(-1, -1));
	ERR_FAIL_COND_V(p_column - 1 > get_line(p_line).size(), Point2(-1, -1));

	Point2 start_position;
	start_position.y = -1;
	start_position.x = -1;

	bool in_region = ((p_line <= 0 || delimiter_cache[p_line - 1].size() < 1) ? -1 : delimiter_cache[p_line - 1].back()->get()) != -1;

	/* Check the keys for this line. */
	for (const KeyValue<int, int> &E : delimiter_cache[p_line]) {
		if (E.key > p_column) {
			break;
		}
		in_region = E.value != -1;
		start_position.x = in_region ? E.key : -1;
	}

	/* Region was found on this line and is not a multiline continuation. */
	int line_length = get_line(p_line).length();
	if (start_position.x != -1 && line_length > 0 && start_position.x != line_length + 1) {
		start_position.y = p_line;
		return start_position;
	}

	/* Not in a region */
	if (!in_region) {
		return start_position;
	}

	/* Region starts on a previous line */
	for (int i = p_line - 1; i >= 0; i--) {
		if (delimiter_cache[i].size() < 1) {
			continue;
		}
		start_position.y = i;
		start_position.x = delimiter_cache[i].back()->key();

		/* Make sure it's not a multiline continuation. */
		line_length = get_line(i).length();
		if (line_length > 0 && start_position.x != line_length + 1) {
			break;
		}
	}
	return start_position;
}

Point2 CodeEdit::get_delimiter_end_position(int p_line, int p_column) const {
	if (delimiters.size() == 0) {
		return Point2(-1, -1);
	}
	ERR_FAIL_INDEX_V(p_line, get_line_count(), Point2(-1, -1));
	ERR_FAIL_COND_V(p_column - 1 > get_line(p_line).size(), Point2(-1, -1));

	Point2 end_position;
	end_position.y = -1;
	end_position.x = -1;

	int region = (p_line <= 0 || delimiter_cache[p_line - 1].size() < 1) ? -1 : delimiter_cache[p_line - 1].back()->value();

	/* Check the keys for this line. */
	for (const KeyValue<int, int> &E : delimiter_cache[p_line]) {
		end_position.x = (E.value == -1) ? E.key : -1;
		if (E.key > p_column) {
			break;
		}
		region = E.value;
	}

	/* Region was found on this line and is not a multiline continuation. */
	if (region != -1 && end_position.x != -1 && (delimiters[region].line_only || end_position.x != get_line(p_line).length() + 1)) {
		end_position.y = p_line;
		return end_position;
	}

	/* Not in a region */
	if (region == -1) {
		end_position.x = -1;
		return end_position;
	}

	/* Region ends on a later line */
	for (int i = p_line + 1; i < get_line_count(); i++) {
		if (delimiter_cache[i].size() < 1 || delimiter_cache[i].front()->value() != -1) {
			continue;
		}
		end_position.x = delimiter_cache[i].front()->key();

		/* Make sure it's not a multiline continuation. */
		if (get_line(i).length() > 0 && end_position.x != get_line(i).length() + 1) {
			end_position.y = i;
			break;
		}
		end_position.x = -1;
	}
	return end_position;
}

/* Code hint */
void CodeEdit::set_code_hint(const String &p_hint) {
	if (code_hint == p_hint) {
		return;
	}
	code_hint = p_hint;
	code_hint_xpos = -0xFFFF;
	queue_redraw();
}

void CodeEdit::set_code_hint_draw_below(bool p_below) {
	if (code_hint_draw_below == p_below) {
		return;
	}
	code_hint_draw_below = p_below;
	queue_redraw();
}

/* Code Completion */
void CodeEdit::set_code_completion_enabled(bool p_enable) {
	code_completion_enabled = p_enable;
}

bool CodeEdit::is_code_completion_enabled() const {
	return code_completion_enabled;
}

void CodeEdit::set_code_completion_prefixes(const TypedArray<String> &p_prefixes) {
	code_completion_prefixes.clear();
	for (int i = 0; i < p_prefixes.size(); i++) {
		const String prefix = p_prefixes[i];

		ERR_CONTINUE_MSG(prefix.is_empty(), "Code completion prefix cannot be empty.");
		code_completion_prefixes.insert(prefix[0]);
	}
}

TypedArray<String> CodeEdit::get_code_completion_prefixes() const {
	TypedArray<String> prefixes;
	for (const char32_t &E : code_completion_prefixes) {
		prefixes.push_back(String::chr(E));
	}
	return prefixes;
}

String CodeEdit::get_text_for_code_completion() const {
	StringBuilder completion_text;
	const int text_size = get_line_count();
	for (int i = 0; i < text_size; i++) {
		String line = get_line(i);

		if (i == get_caret_line()) {
			completion_text += line.substr(0, get_caret_column());
			/* Not unicode, represents the caret. */
			completion_text += String::chr(0xFFFF);
			completion_text += line.substr(get_caret_column(), line.size());
		} else {
			completion_text += line;
		}

		if (i != text_size - 1) {
			completion_text += "\n";
		}
	}
	return completion_text.as_string();
}

void CodeEdit::request_code_completion(bool p_force) {
	if (GDVIRTUAL_CALL(_request_code_completion, p_force)) {
		return;
	}

	/* Don't re-query if all existing options are quoted types, eg path, signal. */
	bool ignored = code_completion_active && !code_completion_options.is_empty();
	if (ignored) {
		ScriptLanguage::CodeCompletionKind kind = ScriptLanguage::CODE_COMPLETION_KIND_PLAIN_TEXT;
		const ScriptLanguage::CodeCompletionOption *previous_option = nullptr;
		for (int i = 0; i < code_completion_options.size(); i++) {
			const ScriptLanguage::CodeCompletionOption &current_option = code_completion_options[i];
			if (!previous_option) {
				previous_option = &current_option;
				kind = current_option.kind;
			}
			if (previous_option->kind != current_option.kind) {
				ignored = false;
				break;
			}
		}
		ignored = ignored && (kind == ScriptLanguage::CODE_COMPLETION_KIND_FILE_PATH || kind == ScriptLanguage::CODE_COMPLETION_KIND_NODE_PATH || kind == ScriptLanguage::CODE_COMPLETION_KIND_SIGNAL);
	}

	if (ignored) {
		return;
	}

	if (p_force) {
		emit_signal(SNAME("code_completion_requested"));
		return;
	}

	String line = get_line(get_caret_line());
	int ofs = CLAMP(get_caret_column(), 0, line.length());

	if (ofs > 0 && (is_in_string(get_caret_line(), ofs) != -1 || !is_symbol(line[ofs - 1]) || code_completion_prefixes.has(line[ofs - 1]))) {
		emit_signal(SNAME("code_completion_requested"));
	} else if (ofs > 1 && line[ofs - 1] == ' ' && code_completion_prefixes.has(line[ofs - 2])) {
		emit_signal(SNAME("code_completion_requested"));
	}
}

void CodeEdit::add_code_completion_option(CodeCompletionKind p_type, const String &p_display_text, const String &p_insert_text, const Color &p_text_color, const Ref<Resource> &p_icon, const Variant &p_value, int p_location) {
	ScriptLanguage::CodeCompletionOption completion_option;
	completion_option.kind = (ScriptLanguage::CodeCompletionKind)p_type;
	completion_option.display = p_display_text;
	completion_option.insert_text = p_insert_text;
	completion_option.font_color = p_text_color;
	completion_option.icon = p_icon;
	completion_option.default_value = p_value;
	completion_option.location = p_location;
	code_completion_option_submitted.push_back(completion_option);
}

void CodeEdit::update_code_completion_options(bool p_forced) {
	code_completion_forced = p_forced;
	code_completion_option_sources = code_completion_option_submitted;
	code_completion_option_submitted.clear();
	_filter_code_completion_candidates_impl();
}

TypedArray<Dictionary> CodeEdit::get_code_completion_options() const {
	if (!code_completion_active) {
		return TypedArray<Dictionary>();
	}

	TypedArray<Dictionary> completion_options;
	completion_options.resize(code_completion_options.size());
	for (int i = 0; i < code_completion_options.size(); i++) {
		Dictionary option;
		option["kind"] = code_completion_options[i].kind;
		option["display_text"] = code_completion_options[i].display;
		option["insert_text"] = code_completion_options[i].insert_text;
		option["font_color"] = code_completion_options[i].font_color;
		option["icon"] = code_completion_options[i].icon;
		option["location"] = code_completion_options[i].location;
		option["default_value"] = code_completion_options[i].default_value;
		completion_options[i] = option;
	}
	return completion_options;
}

Dictionary CodeEdit::get_code_completion_option(int p_index) const {
	if (!code_completion_active) {
		return Dictionary();
	}
	ERR_FAIL_INDEX_V(p_index, code_completion_options.size(), Dictionary());

	Dictionary option;
	option["kind"] = code_completion_options[p_index].kind;
	option["display_text"] = code_completion_options[p_index].display;
	option["insert_text"] = code_completion_options[p_index].insert_text;
	option["font_color"] = code_completion_options[p_index].font_color;
	option["icon"] = code_completion_options[p_index].icon;
	option["location"] = code_completion_options[p_index].location;
	option["default_value"] = code_completion_options[p_index].default_value;
	return option;
}

int CodeEdit::get_code_completion_selected_index() const {
	return (code_completion_active) ? code_completion_current_selected : -1;
}

void CodeEdit::set_code_completion_selected_index(int p_index) {
	if (!code_completion_active) {
		return;
	}
	ERR_FAIL_INDEX(p_index, code_completion_options.size());
	code_completion_current_selected = p_index;
	code_completion_force_item_center = -1;
	code_completion_pan_offset = 0.0f;
	queue_redraw();
}

void CodeEdit::confirm_code_completion(bool p_replace) {
	if (!is_editable() || !code_completion_active) {
		return;
	}

	if (GDVIRTUAL_CALL(_confirm_code_completion, p_replace)) {
		return;
	}

	char32_t caret_last_completion_char = 0;
	begin_complex_operation();
	begin_multicaret_edit();

	for (int i = 0; i < get_caret_count(); i++) {
		if (multicaret_edit_ignore_caret(i)) {
			continue;
		}
		int caret_line = get_caret_line(i);

		const String &insert_text = code_completion_options[code_completion_current_selected].insert_text;
		const String &display_text = code_completion_options[code_completion_current_selected].display;

		if (p_replace) {
			// Find end of current section.
			const String line = get_line(caret_line);
			int caret_col = get_caret_column(i);
			int caret_remove_line = caret_line;

			bool merge_text = true;
			int in_string = is_in_string(caret_line, caret_col);
			if (in_string != -1) {
				Point2 string_end = get_delimiter_end_position(caret_line, caret_col);
				if (string_end.x != -1) {
					merge_text = false;
					caret_remove_line = string_end.y;
					caret_col = string_end.x - 1;
				}
			}

			if (merge_text) {
				for (; caret_col < line.length(); caret_col++) {
					if (is_symbol(line[caret_col])) {
						break;
					}
				}
			}

			// Replace.
			remove_text(caret_line, get_caret_column(i) - code_completion_base.length(), caret_remove_line, caret_col);
			insert_text_at_caret(insert_text, i);
		} else {
			// Get first non-matching char.
			const String line = get_line(caret_line);
			int caret_col = get_caret_column(i);
			int matching_chars = code_completion_base.length();
			for (; matching_chars <= insert_text.length(); matching_chars++) {
				if (caret_col >= line.length() || line[caret_col] != insert_text[matching_chars]) {
					break;
				}
				caret_col++;
			}

			// Remove base completion text.
			remove_text(caret_line, get_caret_column(i) - code_completion_base.length(), caret_line, get_caret_column(i));

			// Merge with text.
			insert_text_at_caret(insert_text.substr(0, code_completion_base.length()), i);
			set_caret_column(caret_col, false, i);
			insert_text_at_caret(insert_text.substr(matching_chars), i);
		}

		// Handle merging of symbols eg strings, brackets.
		const String line = get_line(caret_line);
		char32_t next_char = line[get_caret_column(i)];
		char32_t last_completion_char = insert_text[insert_text.length() - 1];
		if (i == 0) {
			caret_last_completion_char = last_completion_char;
		}
		char32_t last_completion_char_display = display_text[display_text.length() - 1];

		bool last_char_matches = (last_completion_char == next_char || last_completion_char_display == next_char);
		int pre_brace_pair = get_caret_column(i) > 0 ? _get_auto_brace_pair_open_at_pos(caret_line, get_caret_column(i)) : -1;
		int post_brace_pair = get_caret_column(i) < get_line(caret_line).length() ? _get_auto_brace_pair_close_at_pos(caret_line, get_caret_column(i)) : -1;

		// Strings do not nest like brackets, so ensure we don't add an additional closing pair.
		if (has_string_delimiter(String::chr(last_completion_char))) {
			if (post_brace_pair != -1 && last_char_matches) {
				remove_text(caret_line, get_caret_column(i), caret_line, get_caret_column(i) + 1);
			}
		} else {
			if (pre_brace_pair != -1 && pre_brace_pair != post_brace_pair && last_char_matches) {
				remove_text(caret_line, get_caret_column(i), caret_line, get_caret_column(i) + 1);
			} else if (auto_brace_completion_enabled && pre_brace_pair != -1) {
				insert_text_at_caret(auto_brace_completion_pairs[pre_brace_pair].close_key, i);
				set_caret_column(get_caret_column(i) - auto_brace_completion_pairs[pre_brace_pair].close_key.length(), i == 0, i);
			}
		}

		if (pre_brace_pair == -1 && post_brace_pair == -1 && get_caret_column(i) > 0 && get_caret_column(i) < get_line(caret_line).length()) {
			pre_brace_pair = _get_auto_brace_pair_open_at_pos(caret_line, get_caret_column(i) + 1);
			if (pre_brace_pair != -1 && pre_brace_pair == _get_auto_brace_pair_close_at_pos(caret_line, get_caret_column(i) - 1)) {
				remove_text(caret_line, get_caret_column(i) - 2, caret_line, get_caret_column(i));
				if (_get_auto_brace_pair_close_at_pos(caret_line, get_caret_column(i) + 1) != pre_brace_pair) {
					set_caret_column(get_caret_column(i) + 1, i == 0, i);
				} else {
					set_caret_column(get_caret_column(i) + 2, i == 0, i);
				}
			}
		}
	}

	end_multicaret_edit();
	end_complex_operation();

	cancel_code_completion();
	if (code_completion_prefixes.has(caret_last_completion_char)) {
		request_code_completion();
	}
}

void CodeEdit::cancel_code_completion() {
	if (!code_completion_active) {
		return;
	}
	code_completion_forced = false;
	code_completion_active = false;
	is_code_completion_drag_started = false;
	queue_redraw();
}

/* Line length guidelines */
void CodeEdit::set_line_length_guidelines(TypedArray<int> p_guideline_columns) {
	line_length_guideline_columns = p_guideline_columns;
	queue_redraw();
}

TypedArray<int> CodeEdit::get_line_length_guidelines() const {
	return line_length_guideline_columns;
}

/* Symbol lookup */
void CodeEdit::set_symbol_lookup_on_click_enabled(bool p_enabled) {
	symbol_lookup_on_click_enabled = p_enabled;
	set_symbol_lookup_word_as_valid(false);
}

bool CodeEdit::is_symbol_lookup_on_click_enabled() const {
	return symbol_lookup_on_click_enabled;
}

String CodeEdit::get_text_for_symbol_lookup() const {
	Point2i mp = get_local_mouse_pos();
	Point2i pos = get_line_column_at_pos(mp, false);
	int line = pos.y;
	int col = pos.x;

	if (line == -1) {
		return String();
	}

	return get_text_with_cursor_char(line, col);
}

String CodeEdit::get_text_with_cursor_char(int p_line, int p_column) const {
	const int text_size = get_line_count();
	StringBuilder result;
	for (int i = 0; i < text_size; i++) {
		String line_text = get_line(i);
		if (i == p_line && p_column >= 0 && p_column <= line_text.size()) {
			result += line_text.substr(0, p_column);
			/* Not unicode, represents the cursor. */
			result += String::chr(0xFFFF);
			result += line_text.substr(p_column, line_text.size());
		} else {
			result += line_text;
		}

		if (i != text_size - 1) {
			result += "\n";
		}
	}

	return result.as_string();
}

void CodeEdit::set_symbol_lookup_word_as_valid(bool p_valid) {
	symbol_lookup_word = p_valid ? symbol_lookup_new_word : "";
	symbol_lookup_new_word = "";
	if (lookup_symbol_word != symbol_lookup_word) {
		_set_symbol_lookup_word(symbol_lookup_word);
	}
}

/* Symbol tooltip */
void CodeEdit::set_symbol_tooltip_on_hover_enabled(bool p_enabled) {
	symbol_tooltip_on_hover_enabled = p_enabled;
	if (!p_enabled) {
		symbol_tooltip_timer->stop();
	}
}

bool CodeEdit::is_symbol_tooltip_on_hover_enabled() const {
	return symbol_tooltip_on_hover_enabled;
}

void CodeEdit::_on_symbol_tooltip_timer_timeout() {
	const int line = symbol_tooltip_pos.y;
	const int column = symbol_tooltip_pos.x;
	if (line >= 0 && column >= 0 && !symbol_tooltip_word.is_empty() && !has_selection() && !Input::get_singleton()->is_anything_pressed()) {
		emit_signal(SNAME("symbol_hovered"), symbol_tooltip_word, line, column);
	}
}

/* Text manipulation */
void CodeEdit::move_lines_up() {
	begin_complex_operation();
	begin_multicaret_edit();

	// Move lines up by swapping each line with the one above it.
	Vector<Point2i> line_ranges = get_line_ranges_from_carets();
	for (Point2i line_range : line_ranges) {
		if (line_range.x == 0) {
			continue;
		}
		unfold_line(line_range.x - 1);
		for (int line = line_range.x; line <= line_range.y; line++) {
			unfold_line(line);
			swap_lines(line - 1, line);
		}
		// Fix selection if the last one ends at column 0, since it wasn't moved.
		for (int i = 0; i < get_caret_count(); i++) {
			if (has_selection(i) && get_selection_to_column(i) == 0 && get_selection_to_line(i) == line_range.y + 1) {
				if (is_caret_after_selection_origin(i)) {
					set_caret_line(get_caret_line(i) - 1, false, true, -1, i);
				} else {
					set_selection_origin_line(get_selection_origin_line(i) - 1, true, -1, i);
				}
				break;
			}
		}
	}
	adjust_viewport_to_caret();

	end_multicaret_edit();
	end_complex_operation();
}

void CodeEdit::move_lines_down() {
	begin_complex_operation();
	begin_multicaret_edit();

	// Move lines down by swapping each line with the one below it.
	Vector<Point2i> line_ranges = get_line_ranges_from_carets();
	// Reverse in case line ranges are adjacent, if the first ends at column 0.
	line_ranges.reverse();
	for (Point2i line_range : line_ranges) {
		if (line_range.y == get_line_count() - 1) {
			continue;
		}
		// Fix selection if the last one ends at column 0, since it won't be moved.
		bool selection_to_line_at_end = false;
		for (int i = 0; i < get_caret_count(); i++) {
			if (has_selection(i) && get_selection_to_column(i) == 0 && get_selection_to_line(i) == line_range.y + 1) {
				selection_to_line_at_end = get_selection_to_line(i) == get_line_count() - 1;
				if (selection_to_line_at_end) {
					break;
				}
				if (is_caret_after_selection_origin(i)) {
					set_caret_line(get_caret_line(i) + 1, false, true, -1, i);
				} else {
					set_selection_origin_line(get_selection_origin_line(i) + 1, true, -1, i);
				}
				break;
			}
		}
		if (selection_to_line_at_end) {
			continue;
		}

		unfold_line(line_range.y + 1);
		for (int line = line_range.y; line >= line_range.x; line--) {
			unfold_line(line);
			swap_lines(line + 1, line);
		}
	}
	adjust_viewport_to_caret();

	end_multicaret_edit();
	end_complex_operation();
}

void CodeEdit::delete_lines() {
	begin_complex_operation();
	begin_multicaret_edit();

	Vector<Point2i> line_ranges = get_line_ranges_from_carets();
	int line_offset = 0;
	for (Point2i line_range : line_ranges) {
		// Remove last line of range separately to preserve carets.
		unfold_line(line_range.y + line_offset);
		remove_line_at(line_range.y + line_offset);
		if (line_range.x != line_range.y) {
			remove_text(line_range.x + line_offset, 0, line_range.y + line_offset, 0);
		}
		line_offset += line_range.x - line_range.y - 1;
	}

	// Deselect all.
	deselect();

	end_multicaret_edit();
	end_complex_operation();
}

void CodeEdit::duplicate_selection() {
	begin_complex_operation();
	begin_multicaret_edit();

	// Duplicate lines from carets without selections first.
	for (int i = 0; i < get_caret_count(); i++) {
		if (multicaret_edit_ignore_caret(i)) {
			continue;
		}
		for (int l = get_selection_from_line(i); l <= get_selection_to_line(i); l++) {
			unfold_line(l);
		}
		if (has_selection(i)) {
			continue;
		}

		String text_to_insert = get_line(get_caret_line(i)) + "\n";
		// Insert new text before the line, so the caret is on the second one.
		insert_text(text_to_insert, get_caret_line(i), 0);
	}

	// Duplicate selections.
	for (int i = 0; i < get_caret_count(); i++) {
		if (multicaret_edit_ignore_caret(i)) {
			continue;
		}
		if (!has_selection(i)) {
			continue;
		}

		// Insert new text before the selection, so the caret is on the second one.
		insert_text(get_selected_text(i), get_selection_from_line(i), get_selection_from_column(i));
	}

	end_multicaret_edit();
	end_complex_operation();
}

void CodeEdit::duplicate_lines() {
	begin_complex_operation();
	begin_multicaret_edit();

	Vector<Point2i> line_ranges = get_line_ranges_from_carets(false, false);
	int line_offset = 0;
	for (Point2i line_range : line_ranges) {
		// The text that will be inserted. All lines in one string.
		String text_to_insert;

		for (int i = line_range.x + line_offset; i <= line_range.y + line_offset; i++) {
			text_to_insert += get_line(i) + "\n";
			unfold_line(i);
		}

		// Insert new text before the line.
		insert_text(text_to_insert, line_range.x + line_offset, 0);
		line_offset += line_range.y - line_range.x + 1;
	}

	end_multicaret_edit();
	end_complex_operation();
}

/* Visual */
Color CodeEdit::_get_brace_mismatch_color() const {
	return theme_cache.brace_mismatch_color;
}

Color CodeEdit::_get_code_folding_color() const {
	return theme_cache.code_folding_color;
}

Ref<Texture2D> CodeEdit::_get_folded_eol_icon() const {
	return theme_cache.folded_eol_icon;
}

void CodeEdit::_bind_methods() {
	/* Indent management */
	ClassDB::bind_method(D_METHOD("set_indent_size", "size"), &CodeEdit::set_indent_size);
	ClassDB::bind_method(D_METHOD("get_indent_size"), &CodeEdit::get_indent_size);

	ClassDB::bind_method(D_METHOD("set_indent_using_spaces", "use_spaces"), &CodeEdit::set_indent_using_spaces);
	ClassDB::bind_method(D_METHOD("is_indent_using_spaces"), &CodeEdit::is_indent_using_spaces);

	ClassDB::bind_method(D_METHOD("set_auto_indent_enabled", "enable"), &CodeEdit::set_auto_indent_enabled);
	ClassDB::bind_method(D_METHOD("is_auto_indent_enabled"), &CodeEdit::is_auto_indent_enabled);

	ClassDB::bind_method(D_METHOD("set_auto_indent_prefixes", "prefixes"), &CodeEdit::set_auto_indent_prefixes);
	ClassDB::bind_method(D_METHOD("get_auto_indent_prefixes"), &CodeEdit::get_auto_indent_prefixes);

	ClassDB::bind_method(D_METHOD("do_indent"), &CodeEdit::do_indent);

	ClassDB::bind_method(D_METHOD("indent_lines"), &CodeEdit::indent_lines);
	ClassDB::bind_method(D_METHOD("unindent_lines"), &CodeEdit::unindent_lines);

	ClassDB::bind_method(D_METHOD("convert_indent", "from_line", "to_line"), &CodeEdit::convert_indent, DEFVAL(-1), DEFVAL(-1));

	/* Auto brace completion */
	ClassDB::bind_method(D_METHOD("set_auto_brace_completion_enabled", "enable"), &CodeEdit::set_auto_brace_completion_enabled);
	ClassDB::bind_method(D_METHOD("is_auto_brace_completion_enabled"), &CodeEdit::is_auto_brace_completion_enabled);

	ClassDB::bind_method(D_METHOD("set_highlight_matching_braces_enabled", "enable"), &CodeEdit::set_highlight_matching_braces_enabled);
	ClassDB::bind_method(D_METHOD("is_highlight_matching_braces_enabled"), &CodeEdit::is_highlight_matching_braces_enabled);

	ClassDB::bind_method(D_METHOD("add_auto_brace_completion_pair", "start_key", "end_key"), &CodeEdit::add_auto_brace_completion_pair);
	ClassDB::bind_method(D_METHOD("set_auto_brace_completion_pairs", "pairs"), &CodeEdit::set_auto_brace_completion_pairs);
	ClassDB::bind_method(D_METHOD("get_auto_brace_completion_pairs"), &CodeEdit::get_auto_brace_completion_pairs);

	ClassDB::bind_method(D_METHOD("has_auto_brace_completion_open_key", "open_key"), &CodeEdit::has_auto_brace_completion_open_key);
	ClassDB::bind_method(D_METHOD("has_auto_brace_completion_close_key", "close_key"), &CodeEdit::has_auto_brace_completion_close_key);

	ClassDB::bind_method(D_METHOD("get_auto_brace_completion_close_key", "open_key"), &CodeEdit::get_auto_brace_completion_close_key);

	/* Main Gutter */
	ClassDB::bind_method(D_METHOD("set_draw_breakpoints_gutter", "enable"), &CodeEdit::set_draw_breakpoints_gutter);
	ClassDB::bind_method(D_METHOD("is_drawing_breakpoints_gutter"), &CodeEdit::is_drawing_breakpoints_gutter);

	ClassDB::bind_method(D_METHOD("set_draw_bookmarks_gutter", "enable"), &CodeEdit::set_draw_bookmarks_gutter);
	ClassDB::bind_method(D_METHOD("is_drawing_bookmarks_gutter"), &CodeEdit::is_drawing_bookmarks_gutter);

	ClassDB::bind_method(D_METHOD("set_draw_executing_lines_gutter", "enable"), &CodeEdit::set_draw_executing_lines_gutter);
	ClassDB::bind_method(D_METHOD("is_drawing_executing_lines_gutter"), &CodeEdit::is_drawing_executing_lines_gutter);

	// Breakpoints
	ClassDB::bind_method(D_METHOD("set_line_as_breakpoint", "line", "breakpointed"), &CodeEdit::set_line_as_breakpoint);
	ClassDB::bind_method(D_METHOD("is_line_breakpointed", "line"), &CodeEdit::is_line_breakpointed);
	ClassDB::bind_method(D_METHOD("clear_breakpointed_lines"), &CodeEdit::clear_breakpointed_lines);
	ClassDB::bind_method(D_METHOD("get_breakpointed_lines"), &CodeEdit::get_breakpointed_lines);

	// Bookmarks
	ClassDB::bind_method(D_METHOD("set_line_as_bookmarked", "line", "bookmarked"), &CodeEdit::set_line_as_bookmarked);
	ClassDB::bind_method(D_METHOD("is_line_bookmarked", "line"), &CodeEdit::is_line_bookmarked);
	ClassDB::bind_method(D_METHOD("clear_bookmarked_lines"), &CodeEdit::clear_bookmarked_lines);
	ClassDB::bind_method(D_METHOD("get_bookmarked_lines"), &CodeEdit::get_bookmarked_lines);

	// Executing lines
	ClassDB::bind_method(D_METHOD("set_line_as_executing", "line", "executing"), &CodeEdit::set_line_as_executing);
	ClassDB::bind_method(D_METHOD("is_line_executing", "line"), &CodeEdit::is_line_executing);
	ClassDB::bind_method(D_METHOD("clear_executing_lines"), &CodeEdit::clear_executing_lines);
	ClassDB::bind_method(D_METHOD("get_executing_lines"), &CodeEdit::get_executing_lines);

	/* Line numbers */
	ClassDB::bind_method(D_METHOD("set_draw_line_numbers", "enable"), &CodeEdit::set_draw_line_numbers);
	ClassDB::bind_method(D_METHOD("is_draw_line_numbers_enabled"), &CodeEdit::is_draw_line_numbers_enabled);
	ClassDB::bind_method(D_METHOD("set_line_numbers_zero_padded", "enable"), &CodeEdit::set_line_numbers_zero_padded);
	ClassDB::bind_method(D_METHOD("is_line_numbers_zero_padded"), &CodeEdit::is_line_numbers_zero_padded);

	/* Fold Gutter */
	ClassDB::bind_method(D_METHOD("set_draw_fold_gutter", "enable"), &CodeEdit::set_draw_fold_gutter);
	ClassDB::bind_method(D_METHOD("is_drawing_fold_gutter"), &CodeEdit::is_drawing_fold_gutter);

	/* Line folding */
	ClassDB::bind_method(D_METHOD("set_line_folding_enabled", "enabled"), &CodeEdit::set_line_folding_enabled);
	ClassDB::bind_method(D_METHOD("is_line_folding_enabled"), &CodeEdit::is_line_folding_enabled);

	ClassDB::bind_method(D_METHOD("can_fold_line", "line"), &CodeEdit::can_fold_line);

	ClassDB::bind_method(D_METHOD("fold_line", "line"), &CodeEdit::fold_line);
	ClassDB::bind_method(D_METHOD("unfold_line", "line"), &CodeEdit::unfold_line);
	ClassDB::bind_method(D_METHOD("fold_all_lines"), &CodeEdit::fold_all_lines);
	ClassDB::bind_method(D_METHOD("unfold_all_lines"), &CodeEdit::unfold_all_lines);
	ClassDB::bind_method(D_METHOD("toggle_foldable_line", "line"), &CodeEdit::toggle_foldable_line);
	ClassDB::bind_method(D_METHOD("toggle_foldable_lines_at_carets"), &CodeEdit::toggle_foldable_lines_at_carets);

	ClassDB::bind_method(D_METHOD("is_line_folded", "line"), &CodeEdit::is_line_folded);
	ClassDB::bind_method(D_METHOD("get_folded_lines"), &CodeEdit::get_folded_lines);

	/* Code region */
	ClassDB::bind_method(D_METHOD("create_code_region"), &CodeEdit::create_code_region);
	ClassDB::bind_method(D_METHOD("get_code_region_start_tag"), &CodeEdit::get_code_region_start_tag);
	ClassDB::bind_method(D_METHOD("get_code_region_end_tag"), &CodeEdit::get_code_region_end_tag);
	ClassDB::bind_method(D_METHOD("set_code_region_tags", "start", "end"), &CodeEdit::set_code_region_tags, DEFVAL("region"), DEFVAL("endregion"));
	ClassDB::bind_method(D_METHOD("is_line_code_region_start", "line"), &CodeEdit::is_line_code_region_start);
	ClassDB::bind_method(D_METHOD("is_line_code_region_end", "line"), &CodeEdit::is_line_code_region_end);

	/* Delimiters */
	// Strings
	ClassDB::bind_method(D_METHOD("add_string_delimiter", "start_key", "end_key", "line_only"), &CodeEdit::add_string_delimiter, DEFVAL(false));
	ClassDB::bind_method(D_METHOD("remove_string_delimiter", "start_key"), &CodeEdit::remove_string_delimiter);
	ClassDB::bind_method(D_METHOD("has_string_delimiter", "start_key"), &CodeEdit::has_string_delimiter);

	ClassDB::bind_method(D_METHOD("set_string_delimiters", "string_delimiters"), &CodeEdit::set_string_delimiters);
	ClassDB::bind_method(D_METHOD("clear_string_delimiters"), &CodeEdit::clear_string_delimiters);
	ClassDB::bind_method(D_METHOD("get_string_delimiters"), &CodeEdit::get_string_delimiters);

	ClassDB::bind_method(D_METHOD("is_in_string", "line", "column"), &CodeEdit::is_in_string, DEFVAL(-1));

	// Comments
	ClassDB::bind_method(D_METHOD("add_comment_delimiter", "start_key", "end_key", "line_only"), &CodeEdit::add_comment_delimiter, DEFVAL(false));
	ClassDB::bind_method(D_METHOD("remove_comment_delimiter", "start_key"), &CodeEdit::remove_comment_delimiter);
	ClassDB::bind_method(D_METHOD("has_comment_delimiter", "start_key"), &CodeEdit::has_comment_delimiter);

	ClassDB::bind_method(D_METHOD("set_comment_delimiters", "comment_delimiters"), &CodeEdit::set_comment_delimiters);
	ClassDB::bind_method(D_METHOD("clear_comment_delimiters"), &CodeEdit::clear_comment_delimiters);
	ClassDB::bind_method(D_METHOD("get_comment_delimiters"), &CodeEdit::get_comment_delimiters);

	ClassDB::bind_method(D_METHOD("is_in_comment", "line", "column"), &CodeEdit::is_in_comment, DEFVAL(-1));

	// Util
	ClassDB::bind_method(D_METHOD("get_delimiter_start_key", "delimiter_index"), &CodeEdit::get_delimiter_start_key);
	ClassDB::bind_method(D_METHOD("get_delimiter_end_key", "delimiter_index"), &CodeEdit::get_delimiter_end_key);

	ClassDB::bind_method(D_METHOD("get_delimiter_start_position", "line", "column"), &CodeEdit::get_delimiter_start_position);
	ClassDB::bind_method(D_METHOD("get_delimiter_end_position", "line", "column"), &CodeEdit::get_delimiter_end_position);

	/* Code hint */
	ClassDB::bind_method(D_METHOD("set_code_hint", "code_hint"), &CodeEdit::set_code_hint);
	ClassDB::bind_method(D_METHOD("set_code_hint_draw_below", "draw_below"), &CodeEdit::set_code_hint_draw_below);

	/* Code Completion */
	BIND_ENUM_CONSTANT(KIND_CLASS);
	BIND_ENUM_CONSTANT(KIND_FUNCTION);
	BIND_ENUM_CONSTANT(KIND_SIGNAL);
	BIND_ENUM_CONSTANT(KIND_VARIABLE);
	BIND_ENUM_CONSTANT(KIND_MEMBER);
	BIND_ENUM_CONSTANT(KIND_ENUM);
	BIND_ENUM_CONSTANT(KIND_CONSTANT);
	BIND_ENUM_CONSTANT(KIND_NODE_PATH);
	BIND_ENUM_CONSTANT(KIND_FILE_PATH);
	BIND_ENUM_CONSTANT(KIND_PLAIN_TEXT);

	BIND_ENUM_CONSTANT(LOCATION_LOCAL);
	BIND_ENUM_CONSTANT(LOCATION_PARENT_MASK);
	BIND_ENUM_CONSTANT(LOCATION_OTHER_USER_CODE)
	BIND_ENUM_CONSTANT(LOCATION_OTHER);

	ClassDB::bind_method(D_METHOD("get_text_for_code_completion"), &CodeEdit::get_text_for_code_completion);
	ClassDB::bind_method(D_METHOD("request_code_completion", "force"), &CodeEdit::request_code_completion, DEFVAL(false));
	ClassDB::bind_method(D_METHOD("add_code_completion_option", "type", "display_text", "insert_text", "text_color", "icon", "value", "location"), &CodeEdit::add_code_completion_option, DEFVAL(Color(1, 1, 1)), DEFVAL(Ref<Resource>()), DEFVAL(Variant()), DEFVAL(LOCATION_OTHER));
	ClassDB::bind_method(D_METHOD("update_code_completion_options", "force"), &CodeEdit::update_code_completion_options);
	ClassDB::bind_method(D_METHOD("get_code_completion_options"), &CodeEdit::get_code_completion_options);
	ClassDB::bind_method(D_METHOD("get_code_completion_option", "index"), &CodeEdit::get_code_completion_option);
	ClassDB::bind_method(D_METHOD("get_code_completion_selected_index"), &CodeEdit::get_code_completion_selected_index);
	ClassDB::bind_method(D_METHOD("set_code_completion_selected_index", "index"), &CodeEdit::set_code_completion_selected_index);

	ClassDB::bind_method(D_METHOD("confirm_code_completion", "replace"), &CodeEdit::confirm_code_completion, DEFVAL(false));
	ClassDB::bind_method(D_METHOD("cancel_code_completion"), &CodeEdit::cancel_code_completion);

	ClassDB::bind_method(D_METHOD("set_code_completion_enabled", "enable"), &CodeEdit::set_code_completion_enabled);
	ClassDB::bind_method(D_METHOD("is_code_completion_enabled"), &CodeEdit::is_code_completion_enabled);

	ClassDB::bind_method(D_METHOD("set_code_completion_prefixes", "prefixes"), &CodeEdit::set_code_completion_prefixes);
	ClassDB::bind_method(D_METHOD("get_code_completion_prefixes"), &CodeEdit::get_code_completion_prefixes);

	// Overridable

	GDVIRTUAL_BIND(_confirm_code_completion, "replace")
	GDVIRTUAL_BIND(_request_code_completion, "force")
	GDVIRTUAL_BIND(_filter_code_completion_candidates, "candidates")

	/* Line length guidelines */
	ClassDB::bind_method(D_METHOD("set_line_length_guidelines", "guideline_columns"), &CodeEdit::set_line_length_guidelines);
	ClassDB::bind_method(D_METHOD("get_line_length_guidelines"), &CodeEdit::get_line_length_guidelines);

	/* Symbol lookup */
	ClassDB::bind_method(D_METHOD("set_symbol_lookup_on_click_enabled", "enable"), &CodeEdit::set_symbol_lookup_on_click_enabled);
	ClassDB::bind_method(D_METHOD("is_symbol_lookup_on_click_enabled"), &CodeEdit::is_symbol_lookup_on_click_enabled);

	ClassDB::bind_method(D_METHOD("get_text_for_symbol_lookup"), &CodeEdit::get_text_for_symbol_lookup);
	ClassDB::bind_method(D_METHOD("get_text_with_cursor_char", "line", "column"), &CodeEdit::get_text_with_cursor_char);

	ClassDB::bind_method(D_METHOD("set_symbol_lookup_word_as_valid", "valid"), &CodeEdit::set_symbol_lookup_word_as_valid);

	/* Symbol tooltip */
	ClassDB::bind_method(D_METHOD("set_symbol_tooltip_on_hover_enabled", "enable"), &CodeEdit::set_symbol_tooltip_on_hover_enabled);
	ClassDB::bind_method(D_METHOD("is_symbol_tooltip_on_hover_enabled"), &CodeEdit::is_symbol_tooltip_on_hover_enabled);

	/* Text manipulation */
	ClassDB::bind_method(D_METHOD("move_lines_up"), &CodeEdit::move_lines_up);
	ClassDB::bind_method(D_METHOD("move_lines_down"), &CodeEdit::move_lines_down);
	ClassDB::bind_method(D_METHOD("delete_lines"), &CodeEdit::delete_lines);
	ClassDB::bind_method(D_METHOD("duplicate_selection"), &CodeEdit::duplicate_selection);
	ClassDB::bind_method(D_METHOD("duplicate_lines"), &CodeEdit::duplicate_lines);

	/* Inspector */
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "symbol_lookup_on_click"), "set_symbol_lookup_on_click_enabled", "is_symbol_lookup_on_click_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "symbol_tooltip_on_hover"), "set_symbol_tooltip_on_hover_enabled", "is_symbol_tooltip_on_hover_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "line_folding"), "set_line_folding_enabled", "is_line_folding_enabled");

	ADD_PROPERTY(PropertyInfo(Variant::PACKED_INT32_ARRAY, "line_length_guidelines"), "set_line_length_guidelines", "get_line_length_guidelines");

	ADD_GROUP("Gutters", "gutters_");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "gutters_draw_breakpoints_gutter"), "set_draw_breakpoints_gutter", "is_drawing_breakpoints_gutter");

	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "gutters_draw_bookmarks"), "set_draw_bookmarks_gutter", "is_drawing_bookmarks_gutter");

	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "gutters_draw_executing_lines"), "set_draw_executing_lines_gutter", "is_drawing_executing_lines_gutter");

	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "gutters_draw_line_numbers"), "set_draw_line_numbers", "is_draw_line_numbers_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "gutters_zero_pad_line_numbers"), "set_line_numbers_zero_padded", "is_line_numbers_zero_padded");

	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "gutters_draw_fold_gutter"), "set_draw_fold_gutter", "is_drawing_fold_gutter");

	ADD_GROUP("Delimiters", "delimiter_");
	ADD_PROPERTY(PropertyInfo(Variant::PACKED_STRING_ARRAY, "delimiter_strings"), "set_string_delimiters", "get_string_delimiters");
	ADD_PROPERTY(PropertyInfo(Variant::PACKED_STRING_ARRAY, "delimiter_comments"), "set_comment_delimiters", "get_comment_delimiters");

	ADD_GROUP("Code Completion", "code_completion_");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "code_completion_enabled"), "set_code_completion_enabled", "is_code_completion_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::PACKED_STRING_ARRAY, "code_completion_prefixes"), "set_code_completion_prefixes", "get_code_completion_prefixes");

	ADD_GROUP("Indentation", "indent_");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "indent_size"), "set_indent_size", "get_indent_size");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "indent_use_spaces"), "set_indent_using_spaces", "is_indent_using_spaces");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "indent_automatic"), "set_auto_indent_enabled", "is_auto_indent_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::PACKED_STRING_ARRAY, "indent_automatic_prefixes"), "set_auto_indent_prefixes", "get_auto_indent_prefixes");

	ADD_GROUP("Auto Brace Completion", "auto_brace_completion_");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "auto_brace_completion_enabled"), "set_auto_brace_completion_enabled", "is_auto_brace_completion_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "auto_brace_completion_highlight_matching"), "set_highlight_matching_braces_enabled", "is_highlight_matching_braces_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::DICTIONARY, "auto_brace_completion_pairs"), "set_auto_brace_completion_pairs", "get_auto_brace_completion_pairs");

	/* Signals */
	/* Gutters */
	ADD_SIGNAL(MethodInfo("breakpoint_toggled", PropertyInfo(Variant::INT, "line")));

	/* Code Completion */
	ADD_SIGNAL(MethodInfo("code_completion_requested"));

	/* Symbol lookup */
	ADD_SIGNAL(MethodInfo("symbol_lookup", PropertyInfo(Variant::STRING, "symbol"), PropertyInfo(Variant::INT, "line"), PropertyInfo(Variant::INT, "column")));
	ADD_SIGNAL(MethodInfo("symbol_validate", PropertyInfo(Variant::STRING, "symbol")));

	/* Symbol tooltip */
	ADD_SIGNAL(MethodInfo("symbol_hovered", PropertyInfo(Variant::STRING, "symbol"), PropertyInfo(Variant::INT, "line"), PropertyInfo(Variant::INT, "column")));

	/* Theme items */
	/* Gutters */
	BIND_THEME_ITEM(Theme::DATA_TYPE_COLOR, CodeEdit, code_folding_color);
	BIND_THEME_ITEM(Theme::DATA_TYPE_COLOR, CodeEdit, folded_code_region_color);
	BIND_THEME_ITEM_CUSTOM(Theme::DATA_TYPE_ICON, CodeEdit, can_fold_icon, "can_fold");
	BIND_THEME_ITEM_CUSTOM(Theme::DATA_TYPE_ICON, CodeEdit, folded_icon, "folded");
	BIND_THEME_ITEM_CUSTOM(Theme::DATA_TYPE_ICON, CodeEdit, can_fold_code_region_icon, "can_fold_code_region");
	BIND_THEME_ITEM_CUSTOM(Theme::DATA_TYPE_ICON, CodeEdit, folded_code_region_icon, "folded_code_region");
	BIND_THEME_ITEM(Theme::DATA_TYPE_ICON, CodeEdit, folded_eol_icon);
	BIND_THEME_ITEM(Theme::DATA_TYPE_ICON, CodeEdit, completion_color_bg);

	BIND_THEME_ITEM(Theme::DATA_TYPE_COLOR, CodeEdit, breakpoint_color);
	BIND_THEME_ITEM_CUSTOM(Theme::DATA_TYPE_ICON, CodeEdit, breakpoint_icon, "breakpoint");

	BIND_THEME_ITEM(Theme::DATA_TYPE_COLOR, CodeEdit, bookmark_color);
	BIND_THEME_ITEM_CUSTOM(Theme::DATA_TYPE_ICON, CodeEdit, bookmark_icon, "bookmark");

	BIND_THEME_ITEM(Theme::DATA_TYPE_COLOR, CodeEdit, executing_line_color);
	BIND_THEME_ITEM_CUSTOM(Theme::DATA_TYPE_ICON, CodeEdit, executing_line_icon, "executing_line");

	BIND_THEME_ITEM(Theme::DATA_TYPE_COLOR, CodeEdit, line_number_color);

	/* Code Completion */
	BIND_THEME_ITEM_CUSTOM(Theme::DATA_TYPE_STYLEBOX, CodeEdit, code_completion_style, "completion");
	BIND_THEME_ITEM_EXT(Theme::DATA_TYPE_CONSTANT, CodeEdit, code_completion_icon_separation, "h_separation", "ItemList");

	BIND_THEME_ITEM_CUSTOM(Theme::DATA_TYPE_CONSTANT, CodeEdit, code_completion_max_width, "completion_max_width");
	BIND_THEME_ITEM_CUSTOM(Theme::DATA_TYPE_CONSTANT, CodeEdit, code_completion_max_lines, "completion_lines");
	BIND_THEME_ITEM_CUSTOM(Theme::DATA_TYPE_CONSTANT, CodeEdit, code_completion_scroll_width, "completion_scroll_width");
	BIND_THEME_ITEM_CUSTOM(Theme::DATA_TYPE_COLOR, CodeEdit, code_completion_scroll_color, "completion_scroll_color");
	BIND_THEME_ITEM_CUSTOM(Theme::DATA_TYPE_COLOR, CodeEdit, code_completion_scroll_hovered_color, "completion_scroll_hovered_color");
	BIND_THEME_ITEM_CUSTOM(Theme::DATA_TYPE_COLOR, CodeEdit, code_completion_background_color, "completion_background_color");
	BIND_THEME_ITEM_CUSTOM(Theme::DATA_TYPE_COLOR, CodeEdit, code_completion_selected_color, "completion_selected_color");
	BIND_THEME_ITEM_CUSTOM(Theme::DATA_TYPE_COLOR, CodeEdit, code_completion_existing_color, "completion_existing_color");

	/* Code hint */
	BIND_THEME_ITEM_EXT(Theme::DATA_TYPE_STYLEBOX, CodeEdit, code_hint_style, "panel", "TooltipPanel");
	BIND_THEME_ITEM_EXT(Theme::DATA_TYPE_COLOR, CodeEdit, code_hint_color, "font_color", "TooltipLabel");

	/* Line length guideline */
	BIND_THEME_ITEM(Theme::DATA_TYPE_COLOR, CodeEdit, line_length_guideline_color);

	/* Other visuals */
	BIND_THEME_ITEM_CUSTOM(Theme::DATA_TYPE_STYLEBOX, CodeEdit, style_normal, "normal");

	BIND_THEME_ITEM(Theme::DATA_TYPE_COLOR, CodeEdit, brace_mismatch_color);

	BIND_THEME_ITEM(Theme::DATA_TYPE_FONT, CodeEdit, font);
	BIND_THEME_ITEM(Theme::DATA_TYPE_FONT_SIZE, CodeEdit, font_size);

	BIND_THEME_ITEM(Theme::DATA_TYPE_CONSTANT, CodeEdit, line_spacing);
}

/* Auto brace completion */
int CodeEdit::_get_auto_brace_pair_open_at_pos(int p_line, int p_col) {
	const String &line = get_line(p_line);

	/* Should be fast enough, expecting low amount of pairs... */
	for (int i = 0; i < auto_brace_completion_pairs.size(); i++) {
		const String &open_key = auto_brace_completion_pairs[i].open_key;
		if (p_col - open_key.length() < 0) {
			continue;
		}

		bool is_match = true;
		for (int j = 0; j < open_key.length(); j++) {
			if (line[(p_col - 1) - j] != open_key[(open_key.length() - 1) - j]) {
				is_match = false;
				break;
			}
		}

		if (is_match) {
			return i;
		}
	}
	return -1;
}

int CodeEdit::_get_auto_brace_pair_close_at_pos(int p_line, int p_col) {
	const String &line = get_line(p_line);

	/* Should be fast enough, expecting low amount of pairs... */
	for (int i = 0; i < auto_brace_completion_pairs.size(); i++) {
		if (p_col + auto_brace_completion_pairs[i].close_key.length() > line.length()) {
			continue;
		}

		bool is_match = true;
		for (int j = 0; j < auto_brace_completion_pairs[i].close_key.length(); j++) {
			if (line[p_col + j] != auto_brace_completion_pairs[i].close_key[j]) {
				is_match = false;
				break;
			}
		}

		if (is_match) {
			return i;
		}
	}
	return -1;
}

/* Gutters */
void CodeEdit::_gutter_clicked(int p_line, int p_gutter) {
	bool shift_pressed = Input::get_singleton()->is_key_pressed(Key::SHIFT);

	if (p_gutter == main_gutter) {
		if (draw_breakpoints && !shift_pressed) {
			set_line_as_breakpoint(p_line, !is_line_breakpointed(p_line));
		} else if (draw_bookmarks && shift_pressed) {
			set_line_as_bookmarked(p_line, !is_line_bookmarked(p_line));
		}
		return;
	}

	if (p_gutter == line_number_gutter) {
		remove_secondary_carets();
		set_selection_mode(TextEdit::SelectionMode::SELECTION_MODE_LINE);
		if (p_line == get_line_count() - 1) {
			select(p_line, 0, p_line, INT_MAX);
		} else {
			select(p_line, 0, p_line + 1, 0);
		}
		return;
	}

	if (p_gutter == fold_gutter) {
		if (is_line_folded(p_line)) {
			unfold_line(p_line);
		} else if (can_fold_line(p_line)) {
			fold_line(p_line);
		}
		return;
	}
}

void CodeEdit::_update_gutter_indexes() {
	for (int i = 0; i < get_gutter_count(); i++) {
		if (get_gutter_name(i) == "main_gutter") {
			main_gutter = i;
			continue;
		}

		if (get_gutter_name(i) == "line_numbers") {
			line_number_gutter = i;
			continue;
		}

		if (get_gutter_name(i) == "fold_gutter") {
			fold_gutter = i;
			continue;
		}
	}
}

/* Code Region */
void CodeEdit::_update_code_region_tags() {
	code_region_start_string = "";
	code_region_end_string = "";

	if (code_region_start_tag.is_empty() || code_region_end_tag.is_empty()) {
		return;
	}

	// A shorter delimiter has higher priority.
	for (int i = delimiters.size() - 1; i >= 0; i--) {
		if (delimiters[i].type != DelimiterType::TYPE_COMMENT) {
			continue;
		}
		if (delimiters[i].end_key.is_empty() && delimiters[i].line_only == true) {
			code_region_start_string = delimiters[i].start_key + code_region_start_tag;
			code_region_end_string = delimiters[i].start_key + code_region_end_tag;
			return;
		}
	}
}

/* Delimiters */
void CodeEdit::_update_delimiter_cache(int p_from_line, int p_to_line) {
	if (delimiters.size() == 0) {
		return;
	}

	int line_count = get_line_count();
	if (p_to_line == -1) {
		p_to_line = line_count;
	}

	int start_line = MIN(p_from_line, p_to_line);
	int end_line = MAX(p_from_line, p_to_line);

	/* Make sure delimiter_cache has all the lines. */
	if (start_line != end_line) {
		if (p_to_line < p_from_line) {
			for (int i = end_line; i > start_line; i--) {
				delimiter_cache.remove_at(i);
			}
		} else {
			for (int i = start_line; i < end_line; i++) {
				delimiter_cache.insert(i, RBMap<int, int>());
			}
		}
	}

	int in_region = -1;
	for (int i = start_line; i < MIN(end_line + 1, line_count); i++) {
		int current_end_region = (i < 0 || delimiter_cache[i].size() < 1) ? -1 : delimiter_cache[i].back()->value();
		in_region = (i <= 0 || delimiter_cache[i - 1].size() < 1) ? -1 : delimiter_cache[i - 1].back()->value();

		const String &str = get_line(i);
		const int line_length = str.length();
		delimiter_cache.write[i].clear();

		if (str.length() == 0) {
			if (in_region != -1) {
				delimiter_cache.write[i][0] = in_region;
			}
			if (i == end_line && current_end_region != in_region) {
				end_line++;
				end_line = MIN(end_line, line_count);
			}
			continue;
		}

		int end_region = -1;
		for (int j = 0; j < line_length; j++) {
			int from = j;
			for (; from < line_length; from++) {
				if (str[from] == '\\') {
					from++;
					continue;
				}
				break;
			}

			/* check if we are in entering a region */
			bool same_line = false;
			if (in_region == -1) {
				for (int d = 0; d < delimiters.size(); d++) {
					/* check there is enough room */
					int chars_left = line_length - from;
					int start_key_length = delimiters[d].start_key.length();
					int end_key_length = delimiters[d].end_key.length();
					if (chars_left < start_key_length) {
						continue;
					}

					/* search the line */
					bool match = true;
					const char32_t *start_key = delimiters[d].start_key.get_data();
					for (int k = 0; k < start_key_length; k++) {
						if (start_key[k] != str[from + k]) {
							match = false;
							break;
						}
					}
					if (!match) {
						continue;
					}
					same_line = true;
					in_region = d;
					delimiter_cache.write[i][from + 1] = d;
					from += start_key_length;

					/* check if it's the whole line */
					if (end_key_length == 0 || delimiters[d].line_only || from + end_key_length > line_length) {
						j = line_length;
						if (delimiters[d].line_only) {
							delimiter_cache.write[i][line_length + 1] = -1;
						} else {
							end_region = in_region;
						}
					}
					break;
				}

				if (j == line_length || in_region == -1) {
					continue;
				}
			}

			/* if we are in one find the end key */
			/* search the line */
			int region_end_index = -1;
			int end_key_length = delimiters[in_region].end_key.length();
			const char32_t *end_key = delimiters[in_region].end_key.get_data();
			for (; from < line_length; from++) {
				if (line_length - from < end_key_length) {
					break;
				}

				if (!is_symbol(str[from])) {
					continue;
				}

				if (str[from] == '\\') {
					from++;
					continue;
				}

				region_end_index = from;
				for (int k = 0; k < end_key_length; k++) {
					if (end_key[k] != str[from + k]) {
						region_end_index = -1;
						break;
					}
				}

				if (region_end_index != -1) {
					break;
				}
			}

			j = from + (end_key_length - 1);
			end_region = (region_end_index == -1) ? in_region : -1;
			if (!same_line || region_end_index != -1) {
				delimiter_cache.write[i][j + 1] = end_region;
			}
			in_region = -1;
		}

		if (i == end_line && current_end_region != end_region) {
			end_line++;
			end_line = MIN(end_line, line_count);
		}
	}
}

int CodeEdit::_is_in_delimiter(int p_line, int p_column, DelimiterType p_type) const {
	if (delimiters.size() == 0 || p_line >= delimiter_cache.size()) {
		return -1;
	}
	ERR_FAIL_INDEX_V(p_line, get_line_count(), 0);

	int region = (p_line <= 0 || delimiter_cache[p_line - 1].size() < 1) ? -1 : delimiter_cache[p_line - 1].back()->value();
	bool in_region = region != -1 && delimiters[region].type == p_type;
	for (RBMap<int, int>::Element *E = delimiter_cache[p_line].front(); E; E = E->next()) {
		/* If column is specified, loop until the key is larger then the column. */
		if (p_column != -1) {
			if (E->key() > p_column) {
				break;
			}
			in_region = E->value() != -1 && delimiters[E->value()].type == p_type;
			region = in_region ? E->value() : -1;
			continue;
		}

		/* If no column, calculate if the entire line is a region       */
		/* excluding whitespace.                                       */
		const String line = get_line(p_line);
		if (!in_region) {
			if (E->value() == -1 || delimiters[E->value()].type != p_type) {
				break;
			}

			region = E->value();
			in_region = true;
			for (int i = E->key() - 2; i >= 0; i--) {
				if (!is_whitespace(line[i])) {
					return -1;
				}
			}
		}

		if (delimiters[region].line_only) {
			return region;
		}

		int end_col = E->key();
		if (E->value() != -1) {
			if (!E->next()) {
				return region;
			}
			end_col = E->next()->key();
		}

		for (int i = end_col; i < line.length(); i++) {
			if (!is_whitespace(line[i])) {
				return -1;
			}
		}
		return region;
	}
	return in_region ? region : -1;
}

void CodeEdit::_add_delimiter(const String &p_start_key, const String &p_end_key, bool p_line_only, DelimiterType p_type) {
	// If we are the editor allow "null" as a valid start key, otherwise users cannot add delimiters via the inspector.
	if (!(Engine::get_singleton()->is_editor_hint() && p_start_key == "null")) {
		ERR_FAIL_COND_MSG(p_start_key.is_empty(), "delimiter start key cannot be empty");

		for (int i = 0; i < p_start_key.length(); i++) {
			ERR_FAIL_COND_MSG(!is_symbol(p_start_key[i]), "delimiter must start with a symbol");
		}
	}

	if (p_end_key.length() > 0) {
		for (int i = 0; i < p_end_key.length(); i++) {
			ERR_FAIL_COND_MSG(!is_symbol(p_end_key[i]), "delimiter must end with a symbol");
		}
	}

	int at = 0;
	for (int i = 0; i < delimiters.size(); i++) {
		ERR_FAIL_COND_MSG(delimiters[i].start_key == p_start_key, "delimiter with start key '" + p_start_key + "' already exists.");
		if (p_start_key.length() < delimiters[i].start_key.length()) {
			at++;
		} else {
			break;
		}
	}

	Delimiter delimiter;
	delimiter.type = p_type;
	delimiter.start_key = p_start_key;
	delimiter.end_key = p_end_key;
	delimiter.line_only = p_line_only || p_end_key.is_empty();
	delimiters.insert(at, delimiter);
	if (!setting_delimiters) {
		delimiter_cache.clear();
		_update_delimiter_cache();
	}
	if (p_type == DelimiterType::TYPE_COMMENT) {
		_update_code_region_tags();
	}
}

void CodeEdit::_remove_delimiter(const String &p_start_key, DelimiterType p_type) {
	for (int i = 0; i < delimiters.size(); i++) {
		if (delimiters[i].start_key != p_start_key) {
			continue;
		}

		if (delimiters[i].type != p_type) {
			break;
		}

		delimiters.remove_at(i);
		if (!setting_delimiters) {
			delimiter_cache.clear();
			_update_delimiter_cache();
		}
		if (p_type == DelimiterType::TYPE_COMMENT) {
			_update_code_region_tags();
		}
		break;
	}
}

bool CodeEdit::_has_delimiter(const String &p_start_key, DelimiterType p_type) const {
	for (int i = 0; i < delimiters.size(); i++) {
		if (delimiters[i].start_key == p_start_key) {
			return delimiters[i].type == p_type;
		}
	}
	return false;
}

void CodeEdit::_set_delimiters(const TypedArray<String> &p_delimiters, DelimiterType p_type) {
	setting_delimiters = true;
	_clear_delimiters(p_type);

	for (int i = 0; i < p_delimiters.size(); i++) {
		String key = p_delimiters[i];

		if (key.is_empty()) {
			continue;
		}

		const String start_key = key.get_slice(" ", 0);
		const String end_key = key.get_slice_count(" ") > 1 ? key.get_slice(" ", 1) : String();

		_add_delimiter(start_key, end_key, end_key.is_empty(), p_type);
	}
	setting_delimiters = false;
	_update_delimiter_cache();
}

void CodeEdit::_clear_delimiters(DelimiterType p_type) {
	for (int i = delimiters.size() - 1; i >= 0; i--) {
		if (delimiters[i].type == p_type) {
			delimiters.remove_at(i);
		}
	}
	delimiter_cache.clear();
	if (!setting_delimiters) {
		_update_delimiter_cache();
	}
	if (p_type == DelimiterType::TYPE_COMMENT) {
		_update_code_region_tags();
	}
}

TypedArray<String> CodeEdit::_get_delimiters(DelimiterType p_type) const {
	TypedArray<String> r_delimiters;
	for (int i = 0; i < delimiters.size(); i++) {
		if (delimiters[i].type != p_type) {
			continue;
		}
		r_delimiters.push_back(delimiters[i].start_key + (delimiters[i].end_key.is_empty() ? "" : " " + delimiters[i].end_key));
	}
	return r_delimiters;
}

/* Code Completion */
void CodeEdit::_update_scroll_selected_line(float p_mouse_y) {
	float percent = (float)(p_mouse_y - code_completion_scroll_rect.position.y) / code_completion_scroll_rect.size.height;
	percent = CLAMP(percent, 0.0f, 1.0f);

	code_completion_current_selected = (int)(percent * (code_completion_options.size() - 1));
	code_completion_force_item_center = -1;
	code_completion_pan_offset = 0.0f;
}

void CodeEdit::_filter_code_completion_candidates_impl() {
	int line_height = get_line_height();

	if (GDVIRTUAL_IS_OVERRIDDEN(_filter_code_completion_candidates)) {
		Vector<ScriptLanguage::CodeCompletionOption> code_completion_options_new;
		code_completion_base = "";

		/* Build options argument. */
		TypedArray<Dictionary> completion_options_sources;
		completion_options_sources.resize(code_completion_option_sources.size());
		int i = 0;
		for (const ScriptLanguage::CodeCompletionOption &E : code_completion_option_sources) {
			Dictionary option;
			option["kind"] = E.kind;
			option["display_text"] = E.display;
			option["insert_text"] = E.insert_text;
			option["font_color"] = E.font_color;
			option["icon"] = E.icon;
			option["default_value"] = E.default_value;
			option["location"] = E.location;
			completion_options_sources[i] = option;
			i++;
		}

		TypedArray<Dictionary> completion_options;

		GDVIRTUAL_CALL(_filter_code_completion_candidates, completion_options_sources, completion_options);

		/* No options to complete, cancel. */
		if (completion_options.size() == 0) {
			cancel_code_completion();
			return;
		}

		/* Convert back into options. */
		int max_width = 0;
		for (i = 0; i < completion_options.size(); i++) {
			ScriptLanguage::CodeCompletionOption option;
			option.kind = (ScriptLanguage::CodeCompletionKind)(int)completion_options[i].get("kind");
			option.display = completion_options[i].get("display_text");
			option.insert_text = completion_options[i].get("insert_text");
			option.font_color = completion_options[i].get("font_color");
			option.icon = completion_options[i].get("icon");
			option.location = completion_options[i].get("location");
			option.default_value = completion_options[i].get("default_value");

			int offset = 0;
			if (option.default_value.get_type() == Variant::COLOR) {
				offset = line_height;
			}

			if (theme_cache.font.is_valid()) {
				max_width = MAX(max_width, theme_cache.font->get_string_size(option.display, HORIZONTAL_ALIGNMENT_LEFT, -1, theme_cache.font_size).width + offset);
			}
			code_completion_options_new.push_back(option);
		}

		if (_should_reset_selected_option_for_new_options(code_completion_options_new)) {
			code_completion_current_selected = 0;
			code_completion_pan_offset = 0.0f;
		}
		code_completion_options = code_completion_options_new;

		code_completion_longest_line = MIN(max_width, theme_cache.code_completion_max_width * theme_cache.font_size);
		code_completion_force_item_center = -1;
		code_completion_active = true;
		queue_redraw();
		return;
	}

	const int caret_line = get_caret_line();
	const int caret_column = get_caret_column();
	const String line = get_line(caret_line);
	ERR_FAIL_INDEX_MSG(caret_column, line.length() + 1, "Caret column exceeds line length.");

	if (caret_column > 0 && line[caret_column - 1] == '(' && !code_completion_forced) {
		cancel_code_completion();
		return;
	}

	/* Get string status, are we in one or at the close. */
	int in_string = is_in_string(caret_line, caret_column);
	int first_quote_col = -1;
	if (in_string != -1) {
		Point2 string_start_pos = get_delimiter_start_position(caret_line, caret_column);
		first_quote_col = (string_start_pos.y == caret_line) ? string_start_pos.x : -1;
	} else if (caret_column > 0) {
		if (is_in_string(caret_line, caret_column - 1) != -1) {
			first_quote_col = caret_column - 1;
		}
	}

	int cofs = caret_column;
	String string_to_complete;
	bool prev_is_word = false;

	/* Cancel if we are at the close of a string. */
	if (caret_column > 0 && in_string == -1 && first_quote_col == cofs - 1) {
		cancel_code_completion();
		return;
		/* In a string, therefore we are trying to complete the string text. */
	} else if (in_string != -1 && first_quote_col != -1) {
		int key_length = delimiters[in_string].start_key.length();
		string_to_complete = line.substr(first_quote_col - key_length, (cofs - first_quote_col) + key_length);
		/* If we have a space, previous word might be a keyword. eg "func |". */
	} else if (cofs > 0 && line[cofs - 1] == ' ') {
		int ofs = cofs - 1;
		while (ofs > 0 && line[ofs] == ' ') {
			ofs--;
		}
		prev_is_word = !is_symbol(line[ofs]);
		/* Otherwise get current word and set cofs to the start. */
	} else {
		int start_cofs = cofs;
		while (cofs > 0 && line[cofs - 1] > 32 && (line[cofs - 1] == '/' || !is_symbol(line[cofs - 1]))) {
			cofs--;
		}
		string_to_complete = line.substr(cofs, start_cofs - cofs);
	}

	/* If all else fails, check for a prefix.         */
	/* Single space between caret and prefix is okay. */
	bool prev_is_prefix = false;
	if (cofs > 0 && code_completion_prefixes.has(line[cofs - 1])) {
		prev_is_prefix = true;
	} else if (cofs > 1 && line[cofs - 1] == ' ' && code_completion_prefixes.has(line[cofs - 2])) {
		prev_is_prefix = true;
	}

	if (!prev_is_word && string_to_complete.is_empty() && (cofs == 0 || !prev_is_prefix)) {
		cancel_code_completion();
		return;
	}

	/* Filter Options. */
	/* For now handle only traditional quoted strings. */
	bool single_quote = in_string != -1 && first_quote_col > 0 && delimiters[in_string].start_key == "'";

	Vector<ScriptLanguage::CodeCompletionOption> code_completion_options_new;
	code_completion_base = string_to_complete;

	/* Don't autocomplete setting numerical values. */
	if (code_completion_base.is_numeric()) {
		cancel_code_completion();
		return;
	}

	int max_width = 0;
	String string_to_complete_lower = string_to_complete.to_lower();

	for (ScriptLanguage::CodeCompletionOption &option : code_completion_option_sources) {
		option.matches.clear();
		if (single_quote && option.display.is_quoted()) {
			option.display = option.display.unquote().quote("'");
		}

		int offset = option.default_value.get_type() == Variant::COLOR ? line_height : 0;

		if (in_string != -1) {
			// The completion string may have a literal behind it, which should be removed before re-quoting.
			String literal;
			if (option.insert_text.substr(1).is_quoted()) {
				literal = option.display.left(1);
				option.display = option.display.substr(1);
				option.insert_text = option.insert_text.substr(1);
			}
			String quote = single_quote ? "'" : "\"";
			option.display = literal + (option.display.unquote().quote(quote));
			option.insert_text = literal + (option.insert_text.unquote().quote(quote));
		}

		if (option.display.length() == 0) {
			continue;
		}

		if (string_to_complete.length() == 0) {
			option.get_option_characteristics(string_to_complete);
			code_completion_options_new.push_back(option);

			if (theme_cache.font.is_valid()) {
				max_width = MAX(max_width, theme_cache.font->get_string_size(option.display, HORIZONTAL_ALIGNMENT_LEFT, -1, theme_cache.font_size).width + offset);
			}
			continue;
		}

		String target_lower = option.display.to_lower();
		int long_option = target_lower.size() > 50;
		const char32_t *string_to_complete_char_lower = &string_to_complete_lower[0];
		const char32_t *target_char_lower = &target_lower[0];

		Vector<Vector<Pair<int, int>>> all_possible_subsequence_matches;
		for (int i = 0; *target_char_lower; i++, target_char_lower++) {
			if (*target_char_lower == *string_to_complete_char_lower) {
				all_possible_subsequence_matches.push_back({ { i, 1 } });
			}
		}
		string_to_complete_char_lower++;

		for (int i = 1; *string_to_complete_char_lower && (all_possible_subsequence_matches.size() > 0); i++, string_to_complete_char_lower++) {
			// find all occurrences of ssq_lower to avoid looking everywhere each time
			Vector<int> all_occurrences;
			if (long_option) {
				all_occurrences.push_back(target_lower.find_char(*string_to_complete_char_lower));
			} else {
				for (int j = i; j < target_lower.length(); j++) {
					if (target_lower[j] == *string_to_complete_char_lower) {
						all_occurrences.push_back(j);
					}
				}
			}
			Vector<Vector<Pair<int, int>>> next_subsequence_matches;
			for (Vector<Pair<int, int>> &subsequence_match : all_possible_subsequence_matches) {
				Pair<int, int> match_last_segment = subsequence_match[subsequence_match.size() - 1];
				int next_index = match_last_segment.first + match_last_segment.second;
				// get the last index from current sequence
				// and look for next char starting from that index
				if (target_lower[next_index] == *string_to_complete_char_lower) {
					Vector<Pair<int, int>> new_match = subsequence_match;
					new_match.write[new_match.size() - 1].second++;
					next_subsequence_matches.push_back(new_match);
					if (long_option) {
						continue;
					}
				}
				for (int index : all_occurrences) {
					if (index > next_index) {
						Vector<Pair<int, int>> new_match = subsequence_match;
						new_match.push_back({ index, 1 });
						next_subsequence_matches.push_back(new_match);
					}
				}
			}
			all_possible_subsequence_matches = next_subsequence_matches;
		}
		// go through all possible matches to get the best one as defined by CodeCompletionOptionCompare
		if (all_possible_subsequence_matches.size() > 0) {
			option.matches = all_possible_subsequence_matches[0];
			option.get_option_characteristics(string_to_complete);
			all_possible_subsequence_matches = all_possible_subsequence_matches.slice(1);
			if (all_possible_subsequence_matches.size() > 0) {
				CodeCompletionOptionCompare compare;
				ScriptLanguage::CodeCompletionOption compared_option = option;
				compared_option.clear_characteristics();
				for (Vector<Pair<int, int>> &matches : all_possible_subsequence_matches) {
					compared_option.matches = matches;
					compared_option.get_option_characteristics(string_to_complete);
					if (compare(compared_option, option)) {
						option = compared_option;
						compared_option.clear_characteristics();
					}
				}
			}

			code_completion_options_new.push_back(option);
			if (theme_cache.font.is_valid()) {
				max_width = MAX(max_width, theme_cache.font->get_string_size(option.display, HORIZONTAL_ALIGNMENT_LEFT, -1, theme_cache.font_size).width + offset);
			}
		}
	}

	/* No options to complete, cancel. */
	if (code_completion_options_new.size() == 0) {
		cancel_code_completion();
		return;
	}

	/* A perfect match, stop completion. */
	if (code_completion_options_new.size() == 1 && string_to_complete == code_completion_options_new[0].display) {
		cancel_code_completion();
		return;
	}

	code_completion_options_new.sort_custom<CodeCompletionOptionCompare>();
	if (_should_reset_selected_option_for_new_options(code_completion_options_new)) {
		code_completion_current_selected = 0;
		code_completion_pan_offset = 0.0f;
	}
	code_completion_options = code_completion_options_new;

	code_completion_longest_line = MIN(max_width, theme_cache.code_completion_max_width * theme_cache.font_size);
	code_completion_force_item_center = -1;
	code_completion_active = true;
	queue_redraw();
}

// Assumes both the new_options and the code_completion_options are sorted.
bool CodeEdit::_should_reset_selected_option_for_new_options(const Vector<ScriptLanguage::CodeCompletionOption> &p_new_options) {
	if (code_completion_current_selected >= p_new_options.size()) {
		return true;
	}

	for (int i = 0; i < code_completion_options.size() && i < p_new_options.size(); i++) {
		if (i > code_completion_current_selected) {
			return false;
		}
		if (code_completion_options[i].display != p_new_options[i].display) {
			return true;
		}
	}
	return false;
}

void CodeEdit::_lines_edited_from(int p_from_line, int p_to_line) {
	_update_delimiter_cache(p_from_line, p_to_line);

	if (p_from_line == p_to_line) {
		return;
	}

	lines_edited_changed += p_to_line - p_from_line;
	lines_edited_from = (lines_edited_from == -1) ? MIN(p_from_line, p_to_line) : MIN(lines_edited_from, MIN(p_from_line, p_to_line));
	lines_edited_to = (lines_edited_to == -1) ? MAX(p_from_line, p_to_line) : MAX(lines_edited_from, MAX(p_from_line, p_to_line));
}

void CodeEdit::_text_set() {
	lines_edited_from = 0;
	lines_edited_to = 9999;
	_text_changed();
}

void CodeEdit::_text_changed() {
	if (lines_edited_from == -1) {
		return;
	}

	int lc = get_line_count();
	int new_line_number_digits = log10l(lc) + 1;
	if (line_number_digits != new_line_number_digits) {
		_clear_line_number_text_cache();
	}
	line_number_digits = new_line_number_digits;
	_update_line_number_gutter_width();

	List<int> breakpoints;
	for (const KeyValue<int, bool> &E : breakpointed_lines) {
		breakpoints.push_back(E.key);
	}
	for (const int &line : breakpoints) {
		if (line < lines_edited_from || (line < lc && is_line_breakpointed(line))) {
			continue;
		}

		breakpointed_lines.erase(line);
		emit_signal(SNAME("breakpoint_toggled"), line);

		int next_line = line + lines_edited_changed;
		if (next_line > -1 && next_line < lc && is_line_breakpointed(next_line)) {
			emit_signal(SNAME("breakpoint_toggled"), next_line);
			breakpointed_lines[next_line] = true;
			continue;
		}
	}

	lines_edited_from = -1;
	lines_edited_to = -1;
	lines_edited_changed = 0;
}

CodeEdit::CodeEdit() {
	/* Indent management */
	auto_indent_prefixes.insert(':');
	auto_indent_prefixes.insert('{');
	auto_indent_prefixes.insert('[');
	auto_indent_prefixes.insert('(');

	/* Auto brace completion */
	add_auto_brace_completion_pair("(", ")");
	add_auto_brace_completion_pair("{", "}");
	add_auto_brace_completion_pair("[", "]");
	add_auto_brace_completion_pair("\"", "\"");
	add_auto_brace_completion_pair("\'", "\'");

	/* Delimiter tracking */
	add_string_delimiter("\"", "\"", false);
	add_string_delimiter("\'", "\'", false);

	/* Text Direction */
	set_layout_direction(LAYOUT_DIRECTION_LTR);
	set_text_direction(TEXT_DIRECTION_LTR);

	/* Gutters */
	int gutter_idx = 0;

	/* Main Gutter */
	add_gutter();
	set_gutter_name(gutter_idx, "main_gutter");
	set_gutter_draw(gutter_idx, false);
	set_gutter_overwritable(gutter_idx, true);
	set_gutter_type(gutter_idx, GUTTER_TYPE_CUSTOM);
	set_gutter_custom_draw(gutter_idx, callable_mp(this, &CodeEdit::_main_gutter_draw_callback));
	gutter_idx++;

	/* Line numbers */
	add_gutter();
	set_gutter_name(gutter_idx, "line_numbers");
	set_gutter_draw(gutter_idx, false);
	set_gutter_type(gutter_idx, GUTTER_TYPE_CUSTOM);
	set_gutter_custom_draw(gutter_idx, callable_mp(this, &CodeEdit::_line_number_draw_callback));
	gutter_idx++;

	/* Fold Gutter */
	add_gutter();
	set_gutter_name(gutter_idx, "fold_gutter");
	set_gutter_draw(gutter_idx, false);
	set_gutter_type(gutter_idx, GUTTER_TYPE_CUSTOM);
	set_gutter_custom_draw(gutter_idx, callable_mp(this, &CodeEdit::_fold_gutter_draw_callback));
	gutter_idx++;

	/* Symbol tooltip */
	symbol_tooltip_timer = memnew(Timer);
	symbol_tooltip_timer->set_wait_time(0.5); // See `_apply_project_settings()`.
	symbol_tooltip_timer->set_one_shot(true);
	symbol_tooltip_timer->connect("timeout", callable_mp(this, &CodeEdit::_on_symbol_tooltip_timer_timeout));
	add_child(symbol_tooltip_timer, false, INTERNAL_MODE_FRONT);

	connect("lines_edited_from", callable_mp(this, &CodeEdit::_lines_edited_from));
	connect("text_set", callable_mp(this, &CodeEdit::_text_set));
	connect(SceneStringName(text_changed), callable_mp(this, &CodeEdit::_text_changed));

	connect("gutter_clicked", callable_mp(this, &CodeEdit::_gutter_clicked));
	connect("gutter_added", callable_mp(this, &CodeEdit::_update_gutter_indexes));
	connect("gutter_removed", callable_mp(this, &CodeEdit::_update_gutter_indexes));
	_update_gutter_indexes();
}

CodeEdit::~CodeEdit() {
	_clear_line_number_text_cache();
}

// Return true if l should come before r
bool CodeCompletionOptionCompare::operator()(const ScriptLanguage::CodeCompletionOption &l, const ScriptLanguage::CodeCompletionOption &r) const {
	TypedArray<int> lcharac = l.get_option_cached_characteristics();
	TypedArray<int> rcharac = r.get_option_cached_characteristics();

	if (lcharac != rcharac) {
		return lcharac < rcharac;
	}

	// to get here they need to have the same size so we can take the size of whichever we want
	for (int i = 0; i < l.matches.size(); ++i) {
		if (l.matches[i].first != r.matches[i].first) {
			return l.matches[i].first < r.matches[i].first;
		}
		if (l.matches[i].second != r.matches[i].second) {
			return l.matches[i].second > r.matches[i].second;
		}
	}
	return l.display.naturalnocasecmp_to(r.display) < 0;
}

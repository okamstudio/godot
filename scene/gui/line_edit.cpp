/**************************************************************************/
/*  line_edit.cpp                                                         */
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

#include "line_edit.h"

#include "core/message_queue.h"
#include "core/os/input.h"
#include "core/os/keyboard.h"
#include "core/os/os.h"
#include "core/print_string.h"
#include "core/translation.h"
#include "label.h"
#include "scene/main/viewport.h"

#ifdef TOOLS_ENABLED
#include "editor/editor_scale.h"
#include "editor/editor_settings.h"
#endif

bool LineEdit::_is_text_char(CharType c) {
	return !is_symbol(c);
}

void LineEdit::_gui_input(Ref<InputEvent> p_event) {
	ERR_FAIL_COND(p_event.is_null());

	Ref<InputEventMouseButton> b = p_event;

	if (b.is_valid()) {
		if (b->is_pressed() && b->get_button_index() == BUTTON_RIGHT && context_menu_enabled) {
			popup_show = true;
			if (editable) {
				menu->set_item_disabled(menu->get_item_index(MENU_UNDO), !has_undo());
				menu->set_item_disabled(menu->get_item_index(MENU_REDO), !has_redo());
			}

			menu->set_position(get_global_transform().xform(get_local_mouse_position()));
			menu->set_size(Vector2(1, 1));
			menu->set_scale(get_global_transform().get_scale());
			menu->popup();
			accept_event();
			return;
		}

		if (is_middle_mouse_paste_enabled() && b->is_pressed() && b->get_button_index() == BUTTON_MIDDLE && is_editable() && OS::get_singleton()->has_feature("primary_clipboard")) {
			String paste_buffer = OS::get_singleton()->get_clipboard_primary().strip_escapes();

			selection.enabled = false;
			set_cursor_at_pixel_pos(b->get_position().x);
			if (!paste_buffer.empty()) {
				append_at_cursor(paste_buffer);

				if (!text_changed_dirty) {
					if (is_inside_tree()) {
						MessageQueue::get_singleton()->push_call(this, "_text_changed");
					}
					text_changed_dirty = true;
				}
			}

			grab_focus();
			return;
		}

		if (b->get_button_index() != BUTTON_LEFT) {
			return;
		}

		_reset_caret_blink_timer();
		if (b->is_pressed()) {
			accept_event(); //don't pass event further when clicked on text field
			if (!text.empty() && is_editable() && _is_over_clear_button(b->get_position())) {
				clear_button_status.press_attempt = true;
				clear_button_status.pressing_inside = true;
				update();
				return;
			}

			if (b->get_shift()) {
				shift_selection_check_pre(true);
			}

			set_cursor_at_pixel_pos(b->get_position().x);

			if (b->get_shift()) {
				selection_fill_at_cursor();
				selection.creating = true;

			} else {
				if (selecting_enabled) {
					if (!b->is_doubleclick() && (OS::get_singleton()->get_ticks_msec() - selection.last_dblclk) < 600) {
						// Triple-click select all.
						selection.enabled = true;
						selection.begin = 0;
						selection.end = text.length();
						selection.doubleclick = true;
						selection.last_dblclk = 0;
						if (!pass && OS::get_singleton()->has_feature("primary_clipboard")) {
							OS::get_singleton()->set_clipboard_primary(text);
						}
					} else if (b->is_doubleclick()) {
						// Double-click select word.
						selection.enabled = true;
						int beg = cursor_pos;
						int end = beg;
						bool symbol = beg < text.length() && is_symbol(text[beg]);
						while (beg > 0 && text[beg - 1] > 32 && (symbol == is_symbol(text[beg - 1]))) {
							beg--;
						}
						while (end < text.length() && text[end + 1] > 32 && (symbol == is_symbol(text[end + 1]))) {
							end++;
						}
						if (end < text.length()) {
							end += 1;
						}
						selection.begin = beg;
						selection.end = end;
						selection.doubleclick = true;
						selection.last_dblclk = OS::get_singleton()->get_ticks_msec();
						if (!pass && OS::get_singleton()->has_feature("primary_clipboard")) {
							OS::get_singleton()->set_clipboard_primary(text.substr(selection.begin, selection.end - selection.begin));
						}
					}
				}

				selection.drag_attempt = false;

				if ((cursor_pos < selection.begin) || (cursor_pos > selection.end) || !selection.enabled) {
					deselect();
					selection.cursor_start = cursor_pos;
					selection.creating = true;
				} else if (selection.enabled && !selection.doubleclick) {
					selection.drag_attempt = true;
				}
			}

			update();

		} else {
			if (selection.enabled && !pass && b->get_button_index() == BUTTON_LEFT && OS::get_singleton()->has_feature("primary_clipboard")) {
				OS::get_singleton()->set_clipboard_primary(text.substr(selection.begin, selection.end - selection.begin));
			}
			if (!text.empty() && is_editable() && clear_button_enabled) {
				bool press_attempt = clear_button_status.press_attempt;
				clear_button_status.press_attempt = false;
				if (press_attempt && clear_button_status.pressing_inside && _is_over_clear_button(b->get_position())) {
					clear();
					return;
				}
			}

			if ((!selection.creating) && (!selection.doubleclick)) {
				deselect();
			}
			selection.creating = false;
			selection.doubleclick = false;
			if (!drag_action) {
				selection.drag_attempt = false;
			}

			show_virtual_keyboard();
		}

		update();
	}

	Ref<InputEventMouseMotion> m = p_event;

	if (m.is_valid()) {
		if (!text.empty() && is_editable() && clear_button_enabled) {
			bool last_press_inside = clear_button_status.pressing_inside;
			clear_button_status.pressing_inside = clear_button_status.press_attempt && _is_over_clear_button(m->get_position());
			if (last_press_inside != clear_button_status.pressing_inside) {
				update();
			}
		}

		if (m->get_button_mask() & BUTTON_LEFT) {
			if (selection.creating) {
				set_cursor_at_pixel_pos(m->get_position().x);
				selection_fill_at_cursor();
			}
		}

		if (drag_action && can_drop_data(m->get_position(), get_viewport()->gui_get_drag_data())) {
			drag_caret_force_displayed = true;
			set_cursor_at_pixel_pos(m->get_position().x);
		}
	}

	Ref<InputEventKey> k = p_event;

	if (k.is_valid()) {
		if (!k->is_pressed()) {
			return;
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

		unsigned int code = k->get_scancode();

		if (k->get_command() && is_shortcut_keys_enabled()) {
			bool handled = true;

			switch (code) {
				case (KEY_X): { // CUT.

					if (editable) {
						cut_text();
					}

				} break;

				case (KEY_C): { // COPY.

					copy_text();

				} break;

				case (KEY_V): { // PASTE.

					if (editable) {
						paste_text();
					}

				} break;

				case (KEY_Z): { // Undo/redo.
					if (editable) {
						if (k->get_shift()) {
							redo();
						} else {
							undo();
						}
					}
				} break;

				case (KEY_U): { // Delete from start to cursor.

					if (editable) {
						deselect();
						text = text.substr(cursor_pos, text.length() - cursor_pos);
						update_cached_width();
						set_cursor_position(0);
						_text_changed();
					}

				} break;

				case (KEY_Y): { // PASTE (Yank for unix users).

					if (editable) {
						paste_text();
					}

				} break;
				case (KEY_K): { // Delete from cursor_pos to end.

					if (editable) {
						deselect();
						text = text.substr(0, cursor_pos);
						_text_changed();
					}

				} break;
				case (KEY_A): { // Select all.
					select();

				} break;
#ifdef APPLE_STYLE_KEYS
				case (KEY_LEFT): { // Go to start of text - like HOME key.
					shift_selection_check_pre(k->get_shift());
					set_cursor_position(0);
					shift_selection_check_post(k->get_shift());
				} break;
				case (KEY_RIGHT): { // Go to end of text - like END key.
					shift_selection_check_pre(k->get_shift());
					set_cursor_position(text.length());
					shift_selection_check_post(k->get_shift());
				} break;
				case (KEY_BACKSPACE): {
					if (!editable)
						break;

					// If selected, delete the selection
					if (selection.enabled) {
						selection_delete();
						break;
					}

					// Otherwise delete from cursor to beginning of text edit
					int current_pos = get_cursor_position();
					if (current_pos != 0) {
						delete_text(0, current_pos);
					}
				} break;
#endif
				default: {
					handled = false;
				}
			}

			if (handled) {
				accept_event();
				return;
			}
		}

		_reset_caret_blink_timer();
		if (!k->get_metakey()) {
			bool handled = true;
			switch (code) {
				case KEY_KP_ENTER:
				case KEY_ENTER: {
					emit_signal("text_entered", text);
					if (OS::get_singleton()->has_virtual_keyboard() && virtual_keyboard_enabled) {
						OS::get_singleton()->hide_virtual_keyboard();
					}

				} break;

				case KEY_BACKSPACE: {
					if (!editable) {
						break;
					}

					if (selection.enabled) {
						selection_delete();
						break;
					}

#ifdef APPLE_STYLE_KEYS
					if (k->get_alt()) {
#else
					if (k->get_alt()) {
						handled = false;
						break;
					} else if (k->get_command()) {
#endif
						int cc = cursor_pos;
						bool prev_char = false;

						while (cc > 0) {
							bool ischar = _is_text_char(text[cc - 1]);

							if (prev_char && !ischar) {
								break;
							}

							prev_char = ischar;
							cc--;
						}

						delete_text(cc, cursor_pos);

						set_cursor_position(cc);

					} else {
						delete_char();
					}

				} break;
				case KEY_KP_4: {
					if (k->get_unicode() != 0) {
						handled = false;
						break;
					}
					FALLTHROUGH;
				}
				case KEY_LEFT: {
#ifndef APPLE_STYLE_KEYS
					if (!k->get_alt())
#endif
					{
						if (selection.enabled && !k->get_shift()) {
							set_cursor_position(selection.begin);
							deselect();
							break;
						}
						shift_selection_check_pre(k->get_shift());
					}

#ifdef APPLE_STYLE_KEYS
					if (k->get_command()) {
						set_cursor_position(0);
					} else if (k->get_alt()) {
#else
					if (k->get_alt()) {
						handled = false;
						break;
					} else if (k->get_command()) {
#endif
						bool prev_char = false;
						int cc = cursor_pos;

						while (cc > 0) {
							bool ischar = _is_text_char(text[cc - 1]);

							if (prev_char && !ischar) {
								break;
							}

							prev_char = ischar;
							cc--;
						}

						set_cursor_position(cc);

					} else {
						set_cursor_position(get_cursor_position() - 1);
					}

					shift_selection_check_post(k->get_shift());

				} break;
				case KEY_KP_6: {
					if (k->get_unicode() != 0) {
						handled = false;
						break;
					}
					FALLTHROUGH;
				}
				case KEY_RIGHT: {
#ifndef APPLE_STYLE_KEYS
					if (!k->get_alt())
#endif
					{
						if (selection.enabled && !k->get_shift()) {
							set_cursor_position(selection.end);
							deselect();
							break;
						}
						shift_selection_check_pre(k->get_shift());
					}
#ifdef APPLE_STYLE_KEYS
					if (k->get_command()) {
						set_cursor_position(text.length());
					} else if (k->get_alt()) {
#else
					if (k->get_alt()) {
						handled = false;
						break;
					} else if (k->get_command()) {
#endif
						bool prev_char = false;
						int cc = cursor_pos;

						while (cc < text.length()) {
							bool ischar = _is_text_char(text[cc]);

							if (prev_char && !ischar) {
								break;
							}

							prev_char = ischar;
							cc++;
						}

						set_cursor_position(cc);

					} else {
						set_cursor_position(get_cursor_position() + 1);
					}

					shift_selection_check_post(k->get_shift());

				} break;
				case KEY_UP: {
					shift_selection_check_pre(k->get_shift());
					if (get_cursor_position() == 0) {
						handled = false;
					}
					set_cursor_position(0);
					shift_selection_check_post(k->get_shift());
				} break;
				case KEY_DOWN: {
					shift_selection_check_pre(k->get_shift());
					if (get_cursor_position() == text.length()) {
						handled = false;
					}
					set_cursor_position(text.length());
					shift_selection_check_post(k->get_shift());
				} break;
				case KEY_KP_PERIOD: {
					if (k->get_unicode() != 0) {
						handled = false;
						break;
					}
					FALLTHROUGH;
				}
				case KEY_DELETE: {
					if (!editable) {
						break;
					}

					if (k->get_shift() && !k->get_command() && !k->get_alt()) {
						cut_text();
						break;
					}

					if (selection.enabled) {
						selection_delete();
						break;
					}

					int text_len = text.length();

					if (cursor_pos == text_len) {
						break; // Nothing to do.
					}

#ifdef APPLE_STYLE_KEYS
					if (k->get_alt()) {
#else
					if (k->get_alt()) {
						handled = false;
						break;
					} else if (k->get_command()) {
#endif
						int cc = cursor_pos;

						bool prev_char = false;

						while (cc < text.length()) {
							bool ischar = _is_text_char(text[cc]);

							if (prev_char && !ischar) {
								break;
							}
							prev_char = ischar;
							cc++;
						}

						delete_text(cursor_pos, cc);

					} else {
						set_cursor_position(cursor_pos + 1);
						delete_char();
					}

				} break;
				case KEY_KP_7: {
					if (k->get_unicode() != 0) {
						handled = false;
						break;
					}
					FALLTHROUGH;
				}
				case KEY_HOME: {
					shift_selection_check_pre(k->get_shift());
					set_cursor_position(0);
					shift_selection_check_post(k->get_shift());
				} break;
				case KEY_KP_1: {
					if (k->get_unicode() != 0) {
						handled = false;
						break;
					}
					FALLTHROUGH;
				}
				case KEY_END: {
					shift_selection_check_pre(k->get_shift());
					set_cursor_position(text.length());
					shift_selection_check_post(k->get_shift());
				} break;
				case KEY_MENU: {
					if (context_menu_enabled) {
						popup_show = true;
						if (editable) {
							menu->set_item_disabled(menu->get_item_index(MENU_UNDO), !has_undo());
							menu->set_item_disabled(menu->get_item_index(MENU_REDO), !has_redo());
						}

						Point2 pos = Point2(get_cursor_pixel_pos(), (get_size().y + get_font("font")->get_height()) / 2);
						menu->set_position(get_global_transform().xform(pos));
						menu->set_size(Vector2(1, 1));
						menu->set_scale(get_global_transform().get_scale());
						menu->popup();
						menu->grab_focus();
					}
				} break;

				default: {
					handled = false;
				} break;
			}

			if (handled) {
				accept_event();
			} else if (!k->get_command()) {
				if (k->get_unicode() >= 32 && k->get_scancode() != KEY_DELETE) {
					if (editable) {
						selection_delete();
						CharType ucodestr[2] = { (CharType)k->get_unicode(), 0 };
						int prev_len = text.length();
						append_at_cursor(ucodestr);
						if (text.length() != prev_len) {
							_text_changed();
						}
						accept_event();
					}

				} else {
					return;
				}
			}

			update();
		}

		return;
	}
}

void LineEdit::set_align(Align p_align) {
	ERR_FAIL_INDEX((int)p_align, 4);
	align = p_align;
	update();
}

LineEdit::Align LineEdit::get_align() const {
	return align;
}

Variant LineEdit::get_drag_data(const Point2 &p_point) {
	if (selection.drag_attempt && selection.enabled) {
		String t = text.substr(selection.begin, selection.end - selection.begin);
		Label *l = memnew(Label);
		l->set_text(t);
		set_drag_preview(l);
		return t;
	}

	return Variant();
}

bool LineEdit::can_drop_data(const Point2 &p_point, const Variant &p_data) const {
	bool drop_override = Control::can_drop_data(p_point, p_data); // In case user wants to drop custom data.
	if (drop_override) {
		return drop_override;
	}

	return is_editable() && p_data.get_type() == Variant::STRING;
}

void LineEdit::drop_data(const Point2 &p_point, const Variant &p_data) {
	Control::drop_data(p_point, p_data);

	if (p_data.get_type() == Variant::STRING && is_editable()) {
		set_cursor_at_pixel_pos(p_point.x);
		int caret_column_tmp = cursor_pos;
		bool is_inside_sel = selection.enabled && cursor_pos >= selection.begin && cursor_pos <= selection.end;
		if (Input::get_singleton()->is_key_pressed(KEY_CONTROL)) {
			is_inside_sel = selection.enabled && cursor_pos > selection.begin && cursor_pos < selection.end;
		}
		if (selection.drag_attempt) {
			selection.drag_attempt = false;
			if (!is_inside_sel) {
				if (!Input::get_singleton()->is_key_pressed(KEY_CONTROL)) {
					if (caret_column_tmp > selection.end) {
						caret_column_tmp = caret_column_tmp - (selection.end - selection.begin);
					}
					selection_delete();
				}

				set_cursor_position(caret_column_tmp);
				append_at_cursor(p_data);
			}
		} else if (selection.enabled && cursor_pos >= selection.begin && cursor_pos <= selection.end) {
			caret_column_tmp = selection.begin;
			selection_delete();
			set_cursor_position(caret_column_tmp);
			append_at_cursor(p_data);
			grab_focus();
		} else {
			append_at_cursor(p_data);
			grab_focus();
		}
		select(caret_column_tmp, cursor_pos);
		if (!text_changed_dirty) {
			if (is_inside_tree()) {
				MessageQueue::get_singleton()->push_call(this, "_text_changed");
			}
			text_changed_dirty = true;
		}
		update();
	}
}

Control::CursorShape LineEdit::get_cursor_shape(const Point2 &p_pos) const {
	if ((!text.empty() && is_editable() && _is_over_clear_button(p_pos)) || (!is_editable() && (!is_selecting_enabled() || text.empty()))) {
		return CURSOR_ARROW;
	}
	return Control::get_cursor_shape(p_pos);
}

bool LineEdit::_is_over_clear_button(const Point2 &p_pos) const {
	if (!clear_button_enabled || !has_point(p_pos)) {
		return false;
	}
	Ref<Texture> icon = Control::get_icon("clear");
	int x_ofs = get_stylebox("normal")->get_margin(MARGIN_RIGHT);
	return p_pos.x > get_size().width - icon->get_width() - x_ofs;
}

void LineEdit::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
#ifdef TOOLS_ENABLED
			if (Engine::get_singleton()->is_editor_hint() && !get_tree()->is_node_being_edited(this)) {
				cursor_set_blink_enabled(EditorSettings::get_singleton()->is_caret_blink_active());
				cursor_set_blink_speed(EDITOR_GET("text_editor/cursor/caret_blink_speed"));

				if (!EditorSettings::get_singleton()->is_connected("settings_changed", this, "_editor_settings_changed")) {
					EditorSettings::get_singleton()->connect("settings_changed", this, "_editor_settings_changed");
				}
			}
#endif
			update_cached_width();
			update_placeholder_width();
		} break;
		case NOTIFICATION_THEME_CHANGED: {
			update_cached_width();
			update_placeholder_width();
			update();
		} break;
		case NOTIFICATION_RESIZED: {
			scroll_offset = 0;
			set_cursor_position(get_cursor_position());

		} break;
		case NOTIFICATION_TRANSLATION_CHANGED: {
			placeholder_translated = tr(placeholder);
			update_placeholder_width();
			update();
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
		case NOTIFICATION_DRAW: {
			if ((!has_focus() && !menu->has_focus()) || !window_has_focus) {
				draw_caret = false;
			}

			int width, height;

			Size2 size = get_size();
			width = size.width;
			height = size.height;

			RID ci = get_canvas_item();

			Ref<StyleBox> style = get_stylebox("normal");
			if (!is_editable()) {
				style = get_stylebox("read_only");
				draw_caret = false;
			}

			Ref<Font> font = get_font("font");
			select_font(font);

			style->draw(ci, Rect2(Point2(), size));

			if (has_focus()) {
				get_stylebox("focus")->draw(ci, Rect2(Point2(), size));
			}

			int x_ofs = 0;
			bool using_placeholder = text.empty() && ime_text.empty();
			int cached_text_width = using_placeholder ? cached_placeholder_width : cached_width;

			switch (align) {
				case ALIGN_FILL:
				case ALIGN_LEFT: {
					x_ofs = style->get_offset().x;
				} break;
				case ALIGN_CENTER: {
					if (scroll_offset != 0) {
						x_ofs = style->get_offset().x;
					} else {
						x_ofs = MAX(style->get_margin(MARGIN_LEFT), int(size.width - (cached_text_width)) / 2);
					}
				} break;
				case ALIGN_RIGHT: {
					x_ofs = MAX(style->get_margin(MARGIN_LEFT), int(size.width - style->get_margin(MARGIN_RIGHT) - (cached_text_width)));
				} break;
			}

			int ofs_max = width - style->get_margin(MARGIN_RIGHT);
			int char_ofs = scroll_offset;

			int y_area = height - style->get_minimum_size().height;
			int y_ofs = style->get_offset().y + (y_area - font->get_height()) / 2;

			int font_ascent = font->get_ascent();

			Color selection_color = get_color("selection_color");
			Color font_color = is_editable() ? get_color("font_color") : get_color("font_color_uneditable");
			Color font_color_selected = get_color("font_color_selected");
			Color cursor_color = get_color("cursor_color");

			const String &t = using_placeholder ? placeholder_translated : text;
			// Draw placeholder color.
			if (using_placeholder) {
				font_color.a *= placeholder_alpha;
			}

			bool display_clear_icon = !using_placeholder && is_editable() && clear_button_enabled;
			if (right_icon.is_valid() || display_clear_icon) {
				Ref<Texture> r_icon = display_clear_icon ? Control::get_icon("clear") : right_icon;
				Color color_icon(1, 1, 1, !is_editable() ? .5 * .9 : .9);
				if (display_clear_icon) {
					if (clear_button_status.press_attempt && clear_button_status.pressing_inside) {
						color_icon = get_color("clear_button_color_pressed");
					} else {
						color_icon = get_color("clear_button_color");
					}
				}

				r_icon->draw(ci, Point2(width - r_icon->get_width() - style->get_margin(MARGIN_RIGHT), height / 2 - r_icon->get_height() / 2), color_icon);

				if (align == ALIGN_CENTER) {
					if (scroll_offset == 0) {
						x_ofs = MAX(style->get_margin(MARGIN_LEFT), int(size.width - cached_text_width - r_icon->get_width() - style->get_margin(MARGIN_RIGHT) * 2) / 2);
					}
				} else {
					x_ofs = MAX(style->get_margin(MARGIN_LEFT), x_ofs - r_icon->get_width() - style->get_margin(MARGIN_RIGHT));
				}

				ofs_max -= r_icon->get_width();
			}

			int caret_height = font->get_height() > y_area ? y_area : font->get_height();
			FontDrawer drawer(font, Color(1, 1, 1));
			while (true) {
				// End of string, break.
				if (char_ofs >= t.length()) {
					break;
				}

				if (char_ofs == cursor_pos) {
					if (ime_text.length() > 0) {
						int ofs = 0;
						while (true) {
							if (ofs >= ime_text.length()) {
								break;
							}

							CharType cchar = (pass && !text.empty()) ? secret_character[0] : ime_text[ofs];
							CharType next = (pass && !text.empty()) ? secret_character[0] : ime_text[ofs + 1];
							int im_char_width = font->get_char_size(cchar, next).width;

							if ((x_ofs + im_char_width) > ofs_max) {
								break;
							}

							bool selected = ofs >= ime_selection.x && ofs < ime_selection.x + ime_selection.y;
							if (selected) {
								VisualServer::get_singleton()->canvas_item_add_rect(ci, Rect2(Point2(x_ofs, y_ofs + caret_height), Size2(im_char_width, 3)), font_color);
							} else {
								VisualServer::get_singleton()->canvas_item_add_rect(ci, Rect2(Point2(x_ofs, y_ofs + caret_height), Size2(im_char_width, 1)), font_color);
							}

							drawer.draw_char(ci, Point2(x_ofs, y_ofs + font_ascent), cchar, next, font_color);

							x_ofs += im_char_width;
							ofs++;
						}
					}
				}

				CharType cchar = (pass && !text.empty()) ? secret_character[0] : t[char_ofs];
				CharType next = (pass && !text.empty()) ? secret_character[0] : t[char_ofs + 1];
				int char_width = font->get_char_size(cchar, next).width;

				// End of widget, break.
				if ((x_ofs + char_width) > ofs_max) {
					break;
				}

				bool selected = selection.enabled && char_ofs >= selection.begin && char_ofs < selection.end;

				if (selected) {
					VisualServer::get_singleton()->canvas_item_add_rect(ci, Rect2(Point2(x_ofs, y_ofs), Size2(char_width, caret_height)), selection_color);
				}

				int yofs = y_ofs + (caret_height - font->get_height()) / 2;
				drawer.draw_char(ci, Point2(x_ofs, yofs + font_ascent), cchar, next, selected ? font_color_selected : font_color);

				if (char_ofs == cursor_pos && draw_caret && !using_placeholder) {
					if (ime_text.length() == 0) {
#ifdef TOOLS_ENABLED
						VisualServer::get_singleton()->canvas_item_add_rect(ci, Rect2(Point2(x_ofs, y_ofs), Size2(Math::round(EDSCALE), caret_height)), cursor_color);
#else
						VisualServer::get_singleton()->canvas_item_add_rect(ci, Rect2(Point2(x_ofs, y_ofs), Size2(1, caret_height)), cursor_color);
#endif
					}
				}

				x_ofs += char_width;
				char_ofs++;
			}

			if (char_ofs == cursor_pos) {
				if (ime_text.length() > 0) {
					int ofs = 0;
					while (true) {
						if (ofs >= ime_text.length()) {
							break;
						}

						CharType cchar = (pass && !text.empty()) ? secret_character[0] : ime_text[ofs];
						CharType next = (pass && !text.empty()) ? secret_character[0] : ime_text[ofs + 1];
						int im_char_width = font->get_char_size(cchar, next).width;

						if ((x_ofs + im_char_width) > ofs_max) {
							break;
						}

						bool selected = ofs >= ime_selection.x && ofs < ime_selection.x + ime_selection.y;
						if (selected) {
							VisualServer::get_singleton()->canvas_item_add_rect(ci, Rect2(Point2(x_ofs, y_ofs + caret_height), Size2(im_char_width, 3)), font_color);
						} else {
							VisualServer::get_singleton()->canvas_item_add_rect(ci, Rect2(Point2(x_ofs, y_ofs + caret_height), Size2(im_char_width, 1)), font_color);
						}

						drawer.draw_char(ci, Point2(x_ofs, y_ofs + font_ascent), cchar, next, font_color);

						x_ofs += im_char_width;
						ofs++;
					}
				}
			}

			if ((char_ofs == cursor_pos || using_placeholder || drag_caret_force_displayed) && draw_caret) { // May be at the end, or placeholder.
				if (ime_text.length() == 0) {
					int caret_x_ofs = x_ofs;
					if (using_placeholder) {
						switch (align) {
							case ALIGN_LEFT:
							case ALIGN_FILL: {
								caret_x_ofs = style->get_offset().x;
							} break;
							case ALIGN_CENTER: {
								caret_x_ofs = ofs_max / 2;
							} break;
							case ALIGN_RIGHT: {
								caret_x_ofs = ofs_max;
							} break;
						}
					}
#ifdef TOOLS_ENABLED
					VisualServer::get_singleton()->canvas_item_add_rect(ci, Rect2(Point2(caret_x_ofs, y_ofs), Size2(Math::round(EDSCALE), caret_height)), cursor_color);
#else
					VisualServer::get_singleton()->canvas_item_add_rect(ci, Rect2(Point2(caret_x_ofs, y_ofs), Size2(1, caret_height)), cursor_color);
#endif
				}
			}

			if (has_focus()) {
				OS::get_singleton()->set_ime_active(true);
				OS::get_singleton()->set_ime_position(get_global_position() + Point2(using_placeholder ? 0 : x_ofs, y_ofs + caret_height));
			}
		} break;
		case NOTIFICATION_FOCUS_ENTER: {
			if (caret_blink_enabled) {
				caret_blink_timer->start();
			} else {
				draw_caret = true;
			}

			{
				OS::get_singleton()->set_ime_active(true);
				Point2 cursor_pos2 = Point2(get_cursor_position(), 1) * get_minimum_size().height;
				OS::get_singleton()->set_ime_position(get_global_position() + cursor_pos2);
			}

			show_virtual_keyboard();
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
			drag_action = true;
		} break;
		case Control::NOTIFICATION_DRAG_END: {
			if (is_drag_successful()) {
				if (selection.drag_attempt) {
					selection.drag_attempt = false;
					if (is_editable() && !Input::get_singleton()->is_key_pressed(KEY_CONTROL)) {
						selection_delete();
						// } else if (deselect_on_focus_loss_enabled) {
						// 	deselect();
					}
				}
			} else {
				selection.drag_attempt = false;
			}
			drag_action = false;
			drag_caret_force_displayed = false;
		} break;
	}
}

void LineEdit::copy_text() {
	if (selection.enabled && !pass) {
		OS::get_singleton()->set_clipboard(text.substr(selection.begin, selection.end - selection.begin));
	}
}

void LineEdit::cut_text() {
	if (selection.enabled && !pass) {
		OS::get_singleton()->set_clipboard(text.substr(selection.begin, selection.end - selection.begin));
		selection_delete();
	}
}

void LineEdit::paste_text() {
	// Strip escape characters like \n and \t as they can't be displayed on LineEdit.
	String paste_buffer = OS::get_singleton()->get_clipboard().strip_escapes();

	if (paste_buffer != "") {
		int prev_len = text.length();
		if (selection.enabled) {
			selection_delete();
		}
		append_at_cursor(paste_buffer);

		if (!text_changed_dirty) {
			if (is_inside_tree() && text.length() != prev_len) {
				MessageQueue::get_singleton()->push_call(this, "_text_changed");
			}
			text_changed_dirty = true;
		}
	}
}

bool LineEdit::has_undo() const {
	if (undo_stack_pos == nullptr) {
		return undo_stack.size() > 1;
	}
	return undo_stack_pos != undo_stack.front();
}

bool LineEdit::has_redo() const {
	return undo_stack_pos != nullptr && undo_stack_pos != undo_stack.back();
}

void LineEdit::undo() {
	if (undo_stack_pos == nullptr) {
		if (undo_stack.size() <= 1) {
			return;
		}
		undo_stack_pos = undo_stack.back();
	} else if (undo_stack_pos == undo_stack.front()) {
		return;
	}

	deselect();

	undo_stack_pos = undo_stack_pos->prev();
	TextOperation op = undo_stack_pos->get();
	text = op.text;
	cached_width = op.cached_width;
	scroll_offset = op.scroll_offset;
	set_cursor_position(op.cursor_pos);

	if (expand_to_text_length) {
		minimum_size_changed();
	}

	_emit_text_change();
}

void LineEdit::redo() {
	if (undo_stack_pos == nullptr) {
		return;
	}
	if (undo_stack_pos == undo_stack.back()) {
		return;
	}

	deselect();

	undo_stack_pos = undo_stack_pos->next();
	TextOperation op = undo_stack_pos->get();
	text = op.text;
	cached_width = op.cached_width;
	scroll_offset = op.scroll_offset;
	set_cursor_position(op.cursor_pos);

	if (expand_to_text_length) {
		minimum_size_changed();
	}

	_emit_text_change();
}

void LineEdit::shift_selection_check_pre(bool p_shift) {
	if (!selection.enabled && p_shift) {
		selection.cursor_start = cursor_pos;
	}
	if (!p_shift) {
		deselect();
	}
}

void LineEdit::shift_selection_check_post(bool p_shift) {
	if (p_shift) {
		selection_fill_at_cursor();
	}
}

void LineEdit::set_cursor_at_pixel_pos(int p_x) {
	Ref<Font> font = get_font("font");
	int ofs = scroll_offset;
	Ref<StyleBox> style = get_stylebox("normal");
	int pixel_ofs = 0;
	Size2 size = get_size();
	bool display_clear_icon = !text.empty() && is_editable() && clear_button_enabled;
	int r_icon_width = Control::get_icon("clear")->get_width();

	switch (align) {
		case ALIGN_FILL:
		case ALIGN_LEFT: {
			pixel_ofs = int(style->get_offset().x);
		} break;
		case ALIGN_CENTER: {
			if (scroll_offset != 0) {
				pixel_ofs = int(style->get_offset().x);
			} else {
				pixel_ofs = int(size.width - (cached_width)) / 2;
			}

			if (display_clear_icon) {
				pixel_ofs -= int(r_icon_width / 2 + style->get_margin(MARGIN_RIGHT));
			}
		} break;
		case ALIGN_RIGHT: {
			pixel_ofs = int(size.width - style->get_margin(MARGIN_RIGHT) - (cached_width));

			if (display_clear_icon) {
				pixel_ofs -= int(r_icon_width + style->get_margin(MARGIN_RIGHT));
			}
		} break;
	}

	while (ofs < text.length()) {
		int char_w = 0;
		if (font != nullptr) {
			char_w = font->get_char_size(pass ? secret_character[0] : text[ofs]).width;
		}
		pixel_ofs += char_w;

		if (pixel_ofs > p_x) { // Found what we look for.
			break;
		}

		ofs++;
	}

	set_cursor_position(ofs);
}

int LineEdit::get_cursor_pixel_pos() {
	Ref<Font> font = get_font("font");
	int ofs = scroll_offset;
	Ref<StyleBox> style = get_stylebox("normal");
	int pixel_ofs = 0;
	Size2 size = get_size();
	bool display_clear_icon = !text.empty() && is_editable() && clear_button_enabled;
	int r_icon_width = Control::get_icon("clear")->get_width();

	switch (align) {
		case ALIGN_FILL:
		case ALIGN_LEFT: {
			pixel_ofs = int(style->get_offset().x);
		} break;
		case ALIGN_CENTER: {
			if (scroll_offset != 0) {
				pixel_ofs = int(style->get_offset().x);
			} else {
				pixel_ofs = int(size.width - (cached_width)) / 2;
			}

			if (display_clear_icon) {
				pixel_ofs -= int(r_icon_width / 2 + style->get_margin(MARGIN_RIGHT));
			}
		} break;
		case ALIGN_RIGHT: {
			pixel_ofs = int(size.width - style->get_margin(MARGIN_RIGHT) - (cached_width));

			if (display_clear_icon) {
				pixel_ofs -= int(r_icon_width + style->get_margin(MARGIN_RIGHT));
			}
		} break;
	}

	while (ofs < cursor_pos) {
		if (font != nullptr) {
			pixel_ofs += font->get_char_size(pass ? secret_character[0] : text[ofs]).width;
		}
		ofs++;
	}

	return pixel_ofs;
}

bool LineEdit::cursor_get_blink_enabled() const {
	return caret_blink_enabled;
}

void LineEdit::cursor_set_blink_enabled(const bool p_enabled) {
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

float LineEdit::cursor_get_blink_speed() const {
	return caret_blink_timer->get_wait_time();
}

void LineEdit::cursor_set_blink_speed(const float p_speed) {
	ERR_FAIL_COND(p_speed <= 0);
	caret_blink_timer->set_wait_time(p_speed);
}

void LineEdit::_reset_caret_blink_timer() {
	if (caret_blink_enabled) {
		draw_caret = true;
		if (has_focus()) {
			caret_blink_timer->stop();
			caret_blink_timer->start();
			update();
		}
	}
}

void LineEdit::_toggle_draw_caret() {
	draw_caret = !draw_caret;
	if (is_visible_in_tree() && has_focus() && window_has_focus) {
		update();
	}
}

void LineEdit::delete_char() {
	if ((text.length() <= 0) || (cursor_pos == 0)) {
		return;
	}

	Ref<Font> font = get_font("font");
	if (font != nullptr) {
		cached_width -= font->get_char_size(pass ? secret_character[0] : text[cursor_pos - 1]).width;
	}

	text.erase(cursor_pos - 1, 1);

	set_cursor_position(get_cursor_position() - 1);

	if (align == ALIGN_CENTER || align == ALIGN_RIGHT) {
		scroll_offset = CLAMP(scroll_offset - 1, 0, MAX(text.length() - 1, 0));
	}

	_text_changed();
}

void LineEdit::delete_text(int p_from_column, int p_to_column) {
	ERR_FAIL_COND_MSG(p_from_column < 0 || p_from_column > p_to_column || p_to_column > text.length(),
			vformat("Positional parameters (from: %d, to: %d) are inverted or outside the text length (%d).", p_from_column, p_to_column, text.length()));
	if (text.size() > 0) {
		Ref<Font> font = get_font("font");
		if (font != nullptr) {
			for (int i = p_from_column; i < p_to_column; i++) {
				cached_width -= font->get_char_size(pass ? secret_character[0] : text[i]).width;
			}
		}
	} else {
		cached_width = 0;
	}

	text.erase(p_from_column, p_to_column - p_from_column);
	cursor_pos -= CLAMP(cursor_pos - p_from_column, 0, p_to_column - p_from_column);

	if (cursor_pos >= text.length()) {
		cursor_pos = text.length();
	}
	if (scroll_offset > cursor_pos) {
		scroll_offset = cursor_pos;
	}

	if (align == ALIGN_CENTER || align == ALIGN_RIGHT) {
		scroll_offset = CLAMP(scroll_offset - (p_to_column - p_from_column), 0, MAX(text.length() - 1, 0));
	}

	if (!text_changed_dirty) {
		if (is_inside_tree()) {
			MessageQueue::get_singleton()->push_call(this, "_text_changed");
		}
		text_changed_dirty = true;
	}
}

void LineEdit::set_text(String p_text) {
	clear_internal();
	append_at_cursor(p_text);
	_create_undo_state();

	if (expand_to_text_length) {
		minimum_size_changed();
	}

	update();
	cursor_pos = 0;
	scroll_offset = 0;
}

void LineEdit::set_text_with_selection(const String &p_text) {
	Selection selection_copy = selection;

	clear_internal();
	append_at_cursor(p_text);
	_create_undo_state();

	if (expand_to_text_length) {
		minimum_size_changed();
	}

	int tlen = text.length();
	selection = selection_copy;
	selection.begin = MIN(selection.begin, tlen);
	selection.end = MIN(selection.end, tlen);
	selection.cursor_start = MIN(selection.cursor_start, tlen);

	update();
}

void LineEdit::clear() {
	clear_internal();
	_text_changed();

	// This should reset virtual keyboard state if needed.
	if (has_focus()) {
		show_virtual_keyboard();
	}
}

void LineEdit::show_virtual_keyboard() {
	if (OS::get_singleton()->has_virtual_keyboard() && virtual_keyboard_enabled) {
		if (selection.enabled) {
			OS::get_singleton()->show_virtual_keyboard(text, get_global_rect(), (OS::VirtualKeyboardType)virtual_keyboard_type, max_length, selection.begin, selection.end);
		} else {
			OS::get_singleton()->show_virtual_keyboard(text, get_global_rect(), (OS::VirtualKeyboardType)virtual_keyboard_type, max_length, cursor_pos);
		}
	}
}

String LineEdit::get_text() const {
	return text;
}

void LineEdit::set_placeholder(String p_text) {
	placeholder = p_text;
	placeholder_translated = tr(placeholder);
	update_placeholder_width();
	update();
}

String LineEdit::get_placeholder() const {
	return placeholder;
}

void LineEdit::set_placeholder_alpha(float p_alpha) {
	placeholder_alpha = p_alpha;
	update();
}

float LineEdit::get_placeholder_alpha() const {
	return placeholder_alpha;
}

void LineEdit::set_cursor_position(int p_pos) {
	if (p_pos > (int)text.length()) {
		p_pos = text.length();
	}

	if (p_pos < 0) {
		p_pos = 0;
	}

	cursor_pos = p_pos;

	if (!is_inside_tree()) {
		scroll_offset = cursor_pos;
		return;
	}

	Ref<StyleBox> style = get_stylebox("normal");
	Ref<Font> font = get_font("font");

	if (cursor_pos <= scroll_offset) {
		// Adjust window if cursor goes too much to the left.
		set_scroll_offset(MAX(0, cursor_pos - 1));
	} else {
		// Adjust window if cursor goes too much to the right.
		int window_width = get_size().width - style->get_minimum_size().width;
		bool display_clear_icon = !text.empty() && is_editable() && clear_button_enabled;
		if (right_icon.is_valid() || display_clear_icon) {
			Ref<Texture> r_icon = display_clear_icon ? Control::get_icon("clear") : right_icon;
			window_width -= r_icon->get_width();
		}

		if (window_width < 0) {
			return;
		}
		int wp = scroll_offset;

		if (font.is_valid()) {
			int accum_width = 0;

			for (int i = cursor_pos; i >= scroll_offset; i--) {
				if (i >= text.length()) {
					// Do not do this, because if the cursor is at the end, its just fine that it takes no space.
					// accum_width = font->get_char_size(' ').width;
				} else {
					if (pass) {
						accum_width += font->get_char_size(secret_character[0], i + 1 < text.length() ? secret_character[0] : 0).width;
					} else {
						accum_width += font->get_char_size(text[i], i + 1 < text.length() ? text[i + 1] : 0).width; // Anything should do.
					}
				}
				if (accum_width > window_width) {
					break;
				}

				wp = i;
			}
		}

		if (wp != scroll_offset) {
			set_scroll_offset(wp);
		}
	}
	update();
}

int LineEdit::get_cursor_position() const {
	return cursor_pos;
}

void LineEdit::set_scroll_offset(int p_pos) {
	scroll_offset = p_pos;
	if (scroll_offset < 0) {
		scroll_offset = 0;
	}
}

int LineEdit::get_scroll_offset() const {
	return scroll_offset;
}

void LineEdit::append_at_cursor(String p_text) {
	if (max_length > 0) {
		// Truncate text to append to fit in max_length, if needed.
		int available_chars = max_length - text.length();
		if (p_text.length() > available_chars) {
			emit_signal("text_change_rejected", p_text.substr(available_chars));
			p_text = p_text.substr(0, available_chars);
		}
	}
	String pre = text.substr(0, cursor_pos);
	String post = text.substr(cursor_pos, text.length() - cursor_pos);
	text = pre + p_text + post;
	update_cached_width();
	set_cursor_position(cursor_pos + p_text.length());
}

void LineEdit::clear_internal() {
	deselect();
	_clear_undo_stack();
	cached_width = 0;
	cursor_pos = 0;
	scroll_offset = 0;
	undo_text = "";
	text = "";
	update();
}

Size2 LineEdit::get_minimum_size() const {
	Ref<StyleBox> style = get_stylebox("normal");
	Ref<Font> font = get_font("font");

	Size2 min_size;

	// Minimum size of text.
	int space_size = font->get_char_size(' ').x;
	min_size.width = get_constant("minimum_spaces") * space_size;

	if (expand_to_text_length) {
		// Add a space because some fonts are too exact, and because cursor needs a bit more when at the end.
		min_size.width = MAX(min_size.width, font->get_string_size(text).x + space_size);
	}

	min_size.height = font->get_height();

	// Take icons into account.
	if (clear_button_enabled) {
		min_size.width = MAX(min_size.width, Control::get_icon("clear")->get_width());
		min_size.height = MAX(min_size.height, Control::get_icon("clear")->get_height());
	}
	if (right_icon.is_valid()) {
		min_size.width = MAX(min_size.width, right_icon->get_width());
		min_size.height = MAX(min_size.height, right_icon->get_height());
	}

	return style->get_minimum_size() + min_size;
}

void LineEdit::deselect() {
	selection.begin = 0;
	selection.end = 0;
	selection.cursor_start = 0;
	selection.enabled = false;
	selection.creating = false;
	selection.doubleclick = false;
	update();
}

bool LineEdit::has_selection() const {
	return selection.enabled;
}

int LineEdit::get_selection_from_column() const {
	ERR_FAIL_COND_V(!selection.enabled, -1);
	return selection.begin;
}

int LineEdit::get_selection_to_column() const {
	ERR_FAIL_COND_V(!selection.enabled, -1);
	return selection.end;
}

void LineEdit::selection_delete() {
	if (selection.enabled) {
		delete_text(selection.begin, selection.end);
	}

	deselect();
}

void LineEdit::set_max_length(int p_max_length) {
	ERR_FAIL_COND(p_max_length < 0);
	max_length = p_max_length;
	set_text(text);
}

int LineEdit::get_max_length() const {
	return max_length;
}

void LineEdit::selection_fill_at_cursor() {
	if (!selecting_enabled) {
		return;
	}

	selection.begin = cursor_pos;
	selection.end = selection.cursor_start;

	if (selection.end < selection.begin) {
		int aux = selection.end;
		selection.end = selection.begin;
		selection.begin = aux;
	}

	selection.enabled = (selection.begin != selection.end);
}

void LineEdit::select_all() {
	if (!selecting_enabled) {
		return;
	}

	if (!text.length()) {
		return;
	}

	selection.begin = 0;
	selection.end = text.length();
	selection.enabled = true;
	update();
}

void LineEdit::set_editable(bool p_editable) {
	if (editable == p_editable) {
		return;
	}

	editable = p_editable;
	_generate_context_menu();

	minimum_size_changed();
	update();
}

bool LineEdit::is_editable() const {
	return editable;
}

void LineEdit::set_secret(bool p_secret) {
	pass = p_secret;
	update_cached_width();
	update();
}

bool LineEdit::is_secret() const {
	return pass;
}

void LineEdit::set_secret_character(const String &p_string) {
	// An empty string as the secret character would crash the engine.
	// It also wouldn't make sense to use multiple characters as the secret character.
	ERR_FAIL_COND_MSG(p_string.length() != 1, "Secret character must be exactly one character long (" + itos(p_string.length()) + " characters given).");

	secret_character = p_string;
	update_cached_width();
	update();
}

String LineEdit::get_secret_character() const {
	return secret_character;
}

void LineEdit::select(int p_from, int p_to) {
	if (!selecting_enabled) {
		return;
	}

	if (p_from == 0 && p_to == 0) {
		deselect();
		return;
	}

	int len = text.length();
	if (p_from < 0) {
		p_from = 0;
	}
	if (p_from > len) {
		p_from = len;
	}
	if (p_to < 0 || p_to > len) {
		p_to = len;
	}

	if (p_from >= p_to) {
		return;
	}

	selection.enabled = true;
	selection.begin = p_from;
	selection.end = p_to;
	selection.creating = false;
	selection.doubleclick = false;
	update();
}

bool LineEdit::is_text_field() const {
	return true;
}

void LineEdit::menu_option(int p_option) {
	switch (p_option) {
		case MENU_CUT: {
			if (editable) {
				cut_text();
			}
		} break;
		case MENU_COPY: {
			copy_text();
		} break;
		case MENU_PASTE: {
			if (editable) {
				paste_text();
			}
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
			if (editable) {
				undo();
			}
		} break;
		case MENU_REDO: {
			if (editable) {
				redo();
			}
		}
	}
}

void LineEdit::set_context_menu_enabled(bool p_enable) {
	context_menu_enabled = p_enable;
}

bool LineEdit::is_context_menu_enabled() {
	return context_menu_enabled;
}

PopupMenu *LineEdit::get_menu() const {
	return menu;
}

void LineEdit::_editor_settings_changed() {
#ifdef TOOLS_ENABLED
	cursor_set_blink_enabled(EditorSettings::get_singleton()->is_caret_blink_active());
	cursor_set_blink_speed(EDITOR_GET("text_editor/cursor/caret_blink_speed"));
#endif
}

void LineEdit::set_expand_to_text_length(bool p_enabled) {
	expand_to_text_length = p_enabled;
	minimum_size_changed();
	set_scroll_offset(0);
}

bool LineEdit::get_expand_to_text_length() const {
	return expand_to_text_length;
}

void LineEdit::set_clear_button_enabled(bool p_enabled) {
	if (clear_button_enabled == p_enabled) {
		return;
	}
	clear_button_enabled = p_enabled;
	minimum_size_changed();
	update();
}

bool LineEdit::is_clear_button_enabled() const {
	return clear_button_enabled;
}

void LineEdit::set_shortcut_keys_enabled(bool p_enabled) {
	shortcut_keys_enabled = p_enabled;

	_generate_context_menu();
}

bool LineEdit::is_shortcut_keys_enabled() const {
	return shortcut_keys_enabled;
}

void LineEdit::set_virtual_keyboard_enabled(bool p_enable) {
	virtual_keyboard_enabled = p_enable;
}

bool LineEdit::is_virtual_keyboard_enabled() const {
	return virtual_keyboard_enabled;
}

void LineEdit::set_virtual_keyboard_type(VirtualKeyboardType p_type) {
	virtual_keyboard_type = p_type;
}

LineEdit::VirtualKeyboardType LineEdit::get_virtual_keyboard_type() const {
	return virtual_keyboard_type;
}

void LineEdit::set_middle_mouse_paste_enabled(bool p_enabled) {
	middle_mouse_paste_enabled = p_enabled;
}

bool LineEdit::is_middle_mouse_paste_enabled() const {
	return middle_mouse_paste_enabled;
}

void LineEdit::set_selecting_enabled(bool p_enabled) {
	selecting_enabled = p_enabled;

	if (!selecting_enabled) {
		deselect();
	}

	_generate_context_menu();
}

bool LineEdit::is_selecting_enabled() const {
	return selecting_enabled;
}

void LineEdit::set_deselect_on_focus_loss_enabled(const bool p_enabled) {
	deselect_on_focus_loss_enabled = p_enabled;
	if (p_enabled && selection.enabled && !has_focus()) {
		deselect();
	}
}

bool LineEdit::is_deselect_on_focus_loss_enabled() const {
	return deselect_on_focus_loss_enabled;
}

void LineEdit::set_right_icon(const Ref<Texture> &p_icon) {
	if (right_icon == p_icon) {
		return;
	}
	right_icon = p_icon;
	minimum_size_changed();
	update();
}

Ref<Texture> LineEdit::get_right_icon() {
	return right_icon;
}

void LineEdit::_text_changed() {
	if (expand_to_text_length) {
		minimum_size_changed();
	}

	_emit_text_change();
	_clear_redo();
}

void LineEdit::_emit_text_change() {
	emit_signal("text_changed", text);
	_change_notify("text");
	text_changed_dirty = false;
}

void LineEdit::update_cached_width() {
	Ref<Font> font = get_font("font");
	cached_width = 0;
	if (font != nullptr) {
		String text = get_text();
		for (int i = 0; i < text.length(); i++) {
			cached_width += font->get_char_size(pass ? secret_character[0] : text[i]).width;
		}
	}
}

void LineEdit::update_placeholder_width() {
	Ref<Font> font = get_font("font");
	cached_placeholder_width = 0;
	if (font != nullptr) {
		for (int i = 0; i < placeholder_translated.length(); i++) {
			cached_placeholder_width += font->get_char_size(placeholder_translated[i]).width;
		}
	}
}

void LineEdit::_clear_redo() {
	_create_undo_state();
	if (undo_stack_pos == nullptr) {
		return;
	}

	undo_stack_pos = undo_stack_pos->next();
	while (undo_stack_pos) {
		List<TextOperation>::Element *elem = undo_stack_pos;
		undo_stack_pos = undo_stack_pos->next();
		undo_stack.erase(elem);
	}
	_create_undo_state();
}

void LineEdit::_clear_undo_stack() {
	undo_stack.clear();
	undo_stack_pos = nullptr;
	_create_undo_state();
}

void LineEdit::_create_undo_state() {
	TextOperation op;
	op.text = text;
	op.cached_width = cached_width;
	op.cursor_pos = cursor_pos;
	op.scroll_offset = scroll_offset;
	undo_stack.push_back(op);
}

void LineEdit::_generate_context_menu() {
	// Reorganize context menu.
	menu->clear();
	if (editable) {
		menu->add_item(RTR("Cut"), MENU_CUT, is_shortcut_keys_enabled() ? KEY_MASK_CMD | KEY_X : 0);
	}
	menu->add_item(RTR("Copy"), MENU_COPY, is_shortcut_keys_enabled() ? KEY_MASK_CMD | KEY_C : 0);
	if (editable) {
		menu->add_item(RTR("Paste"), MENU_PASTE, is_shortcut_keys_enabled() ? KEY_MASK_CMD | KEY_V : 0);
	}
	menu->add_separator();
	if (is_selecting_enabled()) {
		menu->add_item(RTR("Select All"), MENU_SELECT_ALL, is_shortcut_keys_enabled() ? KEY_MASK_CMD | KEY_A : 0);
	}
	if (editable) {
		menu->add_item(RTR("Clear"), MENU_CLEAR);
		menu->add_separator();
		menu->add_item(RTR("Undo"), MENU_UNDO, is_shortcut_keys_enabled() ? KEY_MASK_CMD | KEY_Z : 0);
		menu->add_item(RTR("Redo"), MENU_REDO, is_shortcut_keys_enabled() ? KEY_MASK_CMD | KEY_MASK_SHIFT | KEY_Z : 0);
	}
}

void LineEdit::_bind_methods() {
	ClassDB::bind_method(D_METHOD("_text_changed"), &LineEdit::_text_changed);
	ClassDB::bind_method(D_METHOD("_toggle_draw_caret"), &LineEdit::_toggle_draw_caret);

	ClassDB::bind_method("_editor_settings_changed", &LineEdit::_editor_settings_changed);

	ClassDB::bind_method(D_METHOD("set_align", "align"), &LineEdit::set_align);
	ClassDB::bind_method(D_METHOD("get_align"), &LineEdit::get_align);

	ClassDB::bind_method(D_METHOD("_gui_input"), &LineEdit::_gui_input);
	ClassDB::bind_method(D_METHOD("clear"), &LineEdit::clear);
	ClassDB::bind_method(D_METHOD("select", "from", "to"), &LineEdit::select, DEFVAL(0), DEFVAL(-1));
	ClassDB::bind_method(D_METHOD("select_all"), &LineEdit::select_all);
	ClassDB::bind_method(D_METHOD("deselect"), &LineEdit::deselect);
	ClassDB::bind_method(D_METHOD("has_selection"), &LineEdit::has_selection);
	ClassDB::bind_method(D_METHOD("get_selection_from_column"), &LineEdit::get_selection_from_column);
	ClassDB::bind_method(D_METHOD("get_selection_to_column"), &LineEdit::get_selection_to_column);
	ClassDB::bind_method(D_METHOD("set_text", "text"), &LineEdit::set_text);
	ClassDB::bind_method(D_METHOD("get_text"), &LineEdit::get_text);
	ClassDB::bind_method(D_METHOD("set_placeholder", "text"), &LineEdit::set_placeholder);
	ClassDB::bind_method(D_METHOD("get_placeholder"), &LineEdit::get_placeholder);
	ClassDB::bind_method(D_METHOD("set_placeholder_alpha", "alpha"), &LineEdit::set_placeholder_alpha);
	ClassDB::bind_method(D_METHOD("get_placeholder_alpha"), &LineEdit::get_placeholder_alpha);
	ClassDB::bind_method(D_METHOD("set_cursor_position", "position"), &LineEdit::set_cursor_position);
	ClassDB::bind_method(D_METHOD("get_cursor_position"), &LineEdit::get_cursor_position);
	ClassDB::bind_method(D_METHOD("get_scroll_offset"), &LineEdit::get_scroll_offset);
	ClassDB::bind_method(D_METHOD("set_expand_to_text_length", "enabled"), &LineEdit::set_expand_to_text_length);
	ClassDB::bind_method(D_METHOD("get_expand_to_text_length"), &LineEdit::get_expand_to_text_length);
	ClassDB::bind_method(D_METHOD("cursor_set_blink_enabled", "enabled"), &LineEdit::cursor_set_blink_enabled);
	ClassDB::bind_method(D_METHOD("cursor_get_blink_enabled"), &LineEdit::cursor_get_blink_enabled);
	ClassDB::bind_method(D_METHOD("cursor_set_blink_speed", "blink_speed"), &LineEdit::cursor_set_blink_speed);
	ClassDB::bind_method(D_METHOD("cursor_get_blink_speed"), &LineEdit::cursor_get_blink_speed);
	ClassDB::bind_method(D_METHOD("set_max_length", "chars"), &LineEdit::set_max_length);
	ClassDB::bind_method(D_METHOD("get_max_length"), &LineEdit::get_max_length);
	ClassDB::bind_method(D_METHOD("append_at_cursor", "text"), &LineEdit::append_at_cursor);
	ClassDB::bind_method(D_METHOD("delete_char_at_cursor"), &LineEdit::delete_char);
	ClassDB::bind_method(D_METHOD("delete_text", "from_column", "to_column"), &LineEdit::delete_text);
	ClassDB::bind_method(D_METHOD("set_editable", "enabled"), &LineEdit::set_editable);
	ClassDB::bind_method(D_METHOD("is_editable"), &LineEdit::is_editable);
	ClassDB::bind_method(D_METHOD("set_secret", "enabled"), &LineEdit::set_secret);
	ClassDB::bind_method(D_METHOD("is_secret"), &LineEdit::is_secret);
	ClassDB::bind_method(D_METHOD("set_secret_character", "character"), &LineEdit::set_secret_character);
	ClassDB::bind_method(D_METHOD("get_secret_character"), &LineEdit::get_secret_character);
	ClassDB::bind_method(D_METHOD("menu_option", "option"), &LineEdit::menu_option);
	ClassDB::bind_method(D_METHOD("get_menu"), &LineEdit::get_menu);
	ClassDB::bind_method(D_METHOD("set_context_menu_enabled", "enable"), &LineEdit::set_context_menu_enabled);
	ClassDB::bind_method(D_METHOD("is_context_menu_enabled"), &LineEdit::is_context_menu_enabled);
	ClassDB::bind_method(D_METHOD("set_virtual_keyboard_enabled", "enable"), &LineEdit::set_virtual_keyboard_enabled);
	ClassDB::bind_method(D_METHOD("is_virtual_keyboard_enabled"), &LineEdit::is_virtual_keyboard_enabled);
	ClassDB::bind_method(D_METHOD("set_virtual_keyboard_type", "type"), &LineEdit::set_virtual_keyboard_type);
	ClassDB::bind_method(D_METHOD("get_virtual_keyboard_type"), &LineEdit::get_virtual_keyboard_type);
	ClassDB::bind_method(D_METHOD("set_clear_button_enabled", "enable"), &LineEdit::set_clear_button_enabled);
	ClassDB::bind_method(D_METHOD("is_clear_button_enabled"), &LineEdit::is_clear_button_enabled);
	ClassDB::bind_method(D_METHOD("set_shortcut_keys_enabled", "enable"), &LineEdit::set_shortcut_keys_enabled);
	ClassDB::bind_method(D_METHOD("is_shortcut_keys_enabled"), &LineEdit::is_shortcut_keys_enabled);
	ClassDB::bind_method(D_METHOD("set_middle_mouse_paste_enabled", "enable"), &LineEdit::set_middle_mouse_paste_enabled);
	ClassDB::bind_method(D_METHOD("is_middle_mouse_paste_enabled"), &LineEdit::is_middle_mouse_paste_enabled);
	ClassDB::bind_method(D_METHOD("set_selecting_enabled", "enable"), &LineEdit::set_selecting_enabled);
	ClassDB::bind_method(D_METHOD("is_selecting_enabled"), &LineEdit::is_selecting_enabled);
	ClassDB::bind_method(D_METHOD("set_deselect_on_focus_loss_enabled", "enable"), &LineEdit::set_deselect_on_focus_loss_enabled);
	ClassDB::bind_method(D_METHOD("is_deselect_on_focus_loss_enabled"), &LineEdit::is_deselect_on_focus_loss_enabled);
	ClassDB::bind_method(D_METHOD("set_right_icon", "icon"), &LineEdit::set_right_icon);
	ClassDB::bind_method(D_METHOD("get_right_icon"), &LineEdit::get_right_icon);

	ADD_SIGNAL(MethodInfo("text_changed", PropertyInfo(Variant::STRING, "new_text")));
	ADD_SIGNAL(MethodInfo("text_change_rejected", PropertyInfo(Variant::STRING, "rejected_substring")));
	ADD_SIGNAL(MethodInfo("text_entered", PropertyInfo(Variant::STRING, "new_text")));

	BIND_ENUM_CONSTANT(ALIGN_LEFT);
	BIND_ENUM_CONSTANT(ALIGN_CENTER);
	BIND_ENUM_CONSTANT(ALIGN_RIGHT);
	BIND_ENUM_CONSTANT(ALIGN_FILL);

	BIND_ENUM_CONSTANT(MENU_CUT);
	BIND_ENUM_CONSTANT(MENU_COPY);
	BIND_ENUM_CONSTANT(MENU_PASTE);
	BIND_ENUM_CONSTANT(MENU_CLEAR);
	BIND_ENUM_CONSTANT(MENU_SELECT_ALL);
	BIND_ENUM_CONSTANT(MENU_UNDO);
	BIND_ENUM_CONSTANT(MENU_REDO);
	BIND_ENUM_CONSTANT(MENU_MAX);

	BIND_ENUM_CONSTANT(KEYBOARD_TYPE_DEFAULT);
	BIND_ENUM_CONSTANT(KEYBOARD_TYPE_MULTILINE);
	BIND_ENUM_CONSTANT(KEYBOARD_TYPE_NUMBER);
	BIND_ENUM_CONSTANT(KEYBOARD_TYPE_NUMBER_DECIMAL);
	BIND_ENUM_CONSTANT(KEYBOARD_TYPE_PHONE);
	BIND_ENUM_CONSTANT(KEYBOARD_TYPE_EMAIL_ADDRESS);
	BIND_ENUM_CONSTANT(KEYBOARD_TYPE_PASSWORD);
	BIND_ENUM_CONSTANT(KEYBOARD_TYPE_URL);

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "text"), "set_text", "get_text");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "align", PROPERTY_HINT_ENUM, "Left,Center,Right,Fill"), "set_align", "get_align");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "max_length", PROPERTY_HINT_RANGE, "0,1000,1,or_greater"), "set_max_length", "get_max_length");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "editable"), "set_editable", "is_editable");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "secret"), "set_secret", "is_secret");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "secret_character"), "set_secret_character", "get_secret_character");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "expand_to_text_length"), "set_expand_to_text_length", "get_expand_to_text_length");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "context_menu_enabled"), "set_context_menu_enabled", "is_context_menu_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "virtual_keyboard_enabled"), "set_virtual_keyboard_enabled", "is_virtual_keyboard_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "virtual_keyboard_type", PROPERTY_HINT_ENUM, "Default,Multiline,Number,Decimal,Phone,Email,Password,URL"), "set_virtual_keyboard_type", "get_virtual_keyboard_type");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "clear_button_enabled"), "set_clear_button_enabled", "is_clear_button_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "shortcut_keys_enabled"), "set_shortcut_keys_enabled", "is_shortcut_keys_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "middle_mouse_paste_enabled"), "set_middle_mouse_paste_enabled", "is_middle_mouse_paste_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "selecting_enabled"), "set_selecting_enabled", "is_selecting_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "deselect_on_focus_loss_enabled"), "set_deselect_on_focus_loss_enabled", "is_deselect_on_focus_loss_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "right_icon", PROPERTY_HINT_RESOURCE_TYPE, "Texture"), "set_right_icon", "get_right_icon");
	ADD_GROUP("Placeholder", "placeholder_");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "placeholder_text"), "set_placeholder", "get_placeholder");
	ADD_PROPERTY(PropertyInfo(Variant::REAL, "placeholder_alpha", PROPERTY_HINT_RANGE, "0,1,0.001"), "set_placeholder_alpha", "get_placeholder_alpha");
	ADD_GROUP("Caret", "caret_");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "caret_blink"), "cursor_set_blink_enabled", "cursor_get_blink_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::REAL, "caret_blink_speed", PROPERTY_HINT_RANGE, "0.1,10,0.01"), "cursor_set_blink_speed", "cursor_get_blink_speed");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "caret_position", PROPERTY_HINT_RANGE, "0,1000,1,or_greater"), "set_cursor_position", "get_cursor_position");
}

LineEdit::LineEdit() {
	align = ALIGN_LEFT;
	cached_width = 0;
	cached_placeholder_width = 0;
	cursor_pos = 0;
	scroll_offset = 0;
	window_has_focus = true;
	max_length = 0;
	pass = false;
	secret_character = "*";
	text_changed_dirty = false;
	placeholder_alpha = 0.6;
	clear_button_enabled = false;
	clear_button_status.press_attempt = false;
	clear_button_status.pressing_inside = false;
	shortcut_keys_enabled = true;
	middle_mouse_paste_enabled = true;
	selecting_enabled = true;
	deselect_on_focus_loss_enabled = true;

	undo_stack_pos = nullptr;
	_create_undo_state();

	deselect();
	set_focus_mode(FOCUS_ALL);
	set_default_cursor_shape(CURSOR_IBEAM);
	set_mouse_filter(MOUSE_FILTER_STOP);

	draw_caret = true;
	caret_blink_enabled = false;
	caret_blink_timer = memnew(Timer);
	add_child(caret_blink_timer);
	caret_blink_timer->set_wait_time(0.65);
	caret_blink_timer->connect("timeout", this, "_toggle_draw_caret");
	cursor_set_blink_enabled(false);

	context_menu_enabled = true;
	menu = memnew(PopupMenu);
	add_child(menu);
	editable = false; // Initialise to opposite first, so we get past the early-out in set_editable.
	set_editable(true);
	menu->connect("id_pressed", this, "menu_option");
	expand_to_text_length = false;
}

LineEdit::~LineEdit() {
}

/**************************************************************************/
/*  tab_bar.cpp                                                           */
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

#include "tab_bar.h"

#include "scene/gui/box_container.h"
#include "scene/gui/label.h"
#include "scene/gui/texture_rect.h"
#include "scene/main/viewport.h"
#include "scene/theme/theme_db.h"

Size2 TabBar::get_minimum_size() const {
	Size2 ms;

	if (tabs.is_empty()) {
		return ms;
	}

	int y_margin = MAX(MAX(MAX(theme_cache.tab_unselected_style->get_minimum_size().height, theme_cache.tab_hovered_style->get_minimum_size().height), theme_cache.tab_selected_style->get_minimum_size().height), theme_cache.tab_disabled_style->get_minimum_size().height);

	for (int i = 0; i < tabs.size(); i++) {
		if (tabs[i].hidden) {
			continue;
		}

		int ofs = ms.width;

		Ref<StyleBox> style;
		if (tabs[i].disabled) {
			style = theme_cache.tab_disabled_style;
		} else if (current == i) {
			style = theme_cache.tab_selected_style;
		} else if (hover == i) {
			style = theme_cache.tab_hovered_style;
		} else {
			style = theme_cache.tab_unselected_style;
		}
		ms.width += style->get_minimum_size().width;

		if (tabs[i].icon.is_valid()) {
			const Size2 icon_size = _get_tab_icon_size(i);
			ms.height = MAX(ms.height, icon_size.height + y_margin);
			ms.width += icon_size.width + theme_cache.h_separation;
		}

		if (!tabs[i].text.is_empty()) {
			ms.width += tabs[i].size_text + theme_cache.h_separation;
		}
		ms.height = MAX(ms.height, tabs[i].text_buf->get_size().y + y_margin);

		bool close_visible = cb_displaypolicy == CLOSE_BUTTON_SHOW_ALWAYS || (cb_displaypolicy == CLOSE_BUTTON_SHOW_ACTIVE_ONLY && i == current);

		if (tabs[i].right_button.is_valid()) {
			Ref<Texture2D> rb = tabs[i].right_button;

			if (close_visible) {
				ms.width += theme_cache.button_hl_style->get_minimum_size().width + rb->get_width();
			} else {
				ms.width += theme_cache.button_hl_style->get_margin(SIDE_LEFT) + rb->get_width() + theme_cache.h_separation;
			}

			ms.height = MAX(ms.height, rb->get_height() + y_margin);
		}

		if (close_visible) {
			ms.width += theme_cache.button_hl_style->get_margin(SIDE_LEFT) + theme_cache.close_icon->get_width() + theme_cache.h_separation;

			ms.height = MAX(ms.height, theme_cache.close_icon->get_height() + y_margin);
		}

		if (ms.width - ofs > style->get_minimum_size().width) {
			ms.width -= theme_cache.h_separation;
		}
	}

	if (clip_tabs) {
		ms.width = 0;
	}

	return ms;
}

void TabBar::gui_input(const Ref<InputEvent> &p_event) {
	ERR_FAIL_COND(p_event.is_null());

	Ref<InputEventMouseMotion> mm = p_event;

	if (mm.is_valid()) {
		Point2 pos = mm->get_position();

		if (buttons_visible) {
			if (is_layout_rtl()) {
				if (pos.x < theme_cache.decrement_icon->get_width()) {
					if (highlight_arrow != 1) {
						highlight_arrow = 1;
						queue_redraw();
					}
				} else if (pos.x < theme_cache.increment_icon->get_width() + theme_cache.decrement_icon->get_width()) {
					if (highlight_arrow != 0) {
						highlight_arrow = 0;
						queue_redraw();
					}
				} else if (highlight_arrow != -1) {
					highlight_arrow = -1;
					queue_redraw();
				}
			} else {
				int limit_minus_buttons = get_size().width - theme_cache.increment_icon->get_width() - theme_cache.decrement_icon->get_width();
				if (pos.x > limit_minus_buttons + theme_cache.decrement_icon->get_width()) {
					if (highlight_arrow != 1) {
						highlight_arrow = 1;
						queue_redraw();
					}
				} else if (pos.x > limit_minus_buttons) {
					if (highlight_arrow != 0) {
						highlight_arrow = 0;
						queue_redraw();
					}
				} else if (highlight_arrow != -1) {
					highlight_arrow = -1;
					queue_redraw();
				}
			}
		}

		if (get_viewport()->gui_is_dragging() && can_drop_data(pos, get_viewport()->gui_get_drag_data())) {
			dragging_valid_tab = true;
			queue_redraw();
		}

		if (!tabs.is_empty()) {
			_update_hover();
		}

		return;
	}

	Ref<InputEventMouseButton> mb = p_event;

	if (mb.is_valid()) {
		if (mb->is_pressed() && mb->get_button_index() == MouseButton::WHEEL_UP && !mb->is_command_or_control_pressed()) {
			if (scrolling_enabled && buttons_visible) {
				if (offset > 0) {
					offset--;
					_update_cache();
					queue_redraw();
				}
			}
		}

		if (mb->is_pressed() && mb->get_button_index() == MouseButton::WHEEL_DOWN && !mb->is_command_or_control_pressed()) {
			if (scrolling_enabled && buttons_visible) {
				if (missing_right && offset < tabs.size()) {
					offset++;
					_update_cache();
					queue_redraw();
				}
			}
		}

		if (rb_pressing && !mb->is_pressed() && mb->get_button_index() == MouseButton::LEFT) {
			if (rb_hover != -1) {
				emit_signal(SNAME("tab_button_pressed"), rb_hover);
			}

			rb_pressing = false;
			queue_redraw();
		}

		if (cb_pressing && !mb->is_pressed() && mb->get_button_index() == MouseButton::LEFT) {
			if (cb_hover != -1) {
				emit_signal(SNAME("tab_close_pressed"), cb_hover);
			}

			cb_pressing = false;
			queue_redraw();
		}

		if (mb->is_pressed() && (mb->get_button_index() == MouseButton::LEFT || (select_with_rmb && mb->get_button_index() == MouseButton::RIGHT))) {
			Point2 pos = mb->get_position();

			if (buttons_visible) {
				if (is_layout_rtl()) {
					if (pos.x < theme_cache.decrement_icon->get_width()) {
						if (missing_right) {
							offset++;
							_update_cache();
							queue_redraw();
						}
						return;
					} else if (pos.x < theme_cache.increment_icon->get_width() + theme_cache.decrement_icon->get_width()) {
						if (offset > 0) {
							offset--;
							_update_cache();
							queue_redraw();
						}
						return;
					}
				} else {
					int limit = get_size().width - theme_cache.increment_icon->get_width() - theme_cache.decrement_icon->get_width();
					if (pos.x > limit + theme_cache.decrement_icon->get_width()) {
						if (missing_right) {
							offset++;
							_update_cache();
							queue_redraw();
						}
						return;
					} else if (pos.x > limit) {
						if (offset > 0) {
							offset--;
							_update_cache();
							queue_redraw();
						}
						return;
					}
				}
			}

			if (tabs.is_empty()) {
				// Return early if there are no actual tabs to handle input for.
				return;
			}

			int found = -1;
			for (int i = offset; i <= max_drawn_tab; i++) {
				if (tabs[i].hidden) {
					continue;
				}

				if (tabs[i].rb_rect.has_point(pos)) {
					rb_pressing = true;
					_update_hover();
					queue_redraw();
					return;
				}

				if (tabs[i].cb_rect.has_point(pos) && (cb_displaypolicy == CLOSE_BUTTON_SHOW_ALWAYS || (cb_displaypolicy == CLOSE_BUTTON_SHOW_ACTIVE_ONLY && i == current))) {
					cb_pressing = true;
					_update_hover();
					queue_redraw();
					return;
				}

				if (pos.x >= get_tab_rect(i).position.x && pos.x < get_tab_rect(i).position.x + tabs[i].size_cache) {
					if (!tabs[i].disabled) {
						found = i;
					}
					break;
				}
			}

			if (found != -1) {
				if (deselect_enabled && get_current_tab() == found) {
					set_current_tab(-1);
				} else {
					set_current_tab(found);
				}

				if (mb->get_button_index() == MouseButton::RIGHT) {
					// Right mouse button clicked.
					emit_signal(SNAME("tab_rmb_clicked"), found);
				}

				emit_signal(SNAME("tab_clicked"), found);
			}
		}
	}

	if (p_event->is_pressed()) {
		Input *input = Input::get_singleton();
		Ref<InputEventJoypadMotion> joypadmotion_event = p_event;
		Ref<InputEventJoypadButton> joypadbutton_event = p_event;
		bool is_joypad_event = (joypadmotion_event.is_valid() || joypadbutton_event.is_valid());
		if (p_event->is_action("ui_right", true)) {
			if (is_joypad_event) {
				if (!input->is_action_just_pressed("ui_right", true)) {
					return;
				}
				set_process_internal(true);
			}
			if (is_layout_rtl() ? select_previous_available() : select_next_available()) {
				accept_event();
			}
		} else if (p_event->is_action("ui_left", true)) {
			if (is_joypad_event) {
				if (!input->is_action_just_pressed("ui_left", true)) {
					return;
				}
				set_process_internal(true);
			}
			if (is_layout_rtl() ? select_next_available() : select_previous_available()) {
				accept_event();
			}
		}
	}
}

String TabBar::get_tooltip(const Point2 &p_pos) const {
	int tab_idx = get_tab_idx_at_point(p_pos);
	if (tab_idx < 0) {
		return Control::get_tooltip(p_pos);
	}

	if (tabs[tab_idx].tooltip.is_empty() && tabs[tab_idx].truncated) {
		return tabs[tab_idx].text;
	}

	return tabs[tab_idx].tooltip;
}

void TabBar::_shape(int p_tab) {
	tabs.write[p_tab].text_buf->clear();
	tabs.write[p_tab].text_buf->set_width(-1);
	if (tabs[p_tab].text_direction == Control::TEXT_DIRECTION_INHERITED) {
		tabs.write[p_tab].text_buf->set_direction(is_layout_rtl() ? TextServer::DIRECTION_RTL : TextServer::DIRECTION_LTR);
	} else {
		tabs.write[p_tab].text_buf->set_direction((TextServer::Direction)tabs[p_tab].text_direction);
	}

	tabs.write[p_tab].text_buf->add_string(atr(tabs[p_tab].text), theme_cache.font, theme_cache.font_size, tabs[p_tab].language);
}

void TabBar::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			if (scroll_to_selected) {
				ensure_tab_visible(current);
			}
			// Set initialized even if no tabs were set.
			initialized = true;
		} break;

		case NOTIFICATION_INTERNAL_PROCESS: {
			Input *input = Input::get_singleton();

			if (input->is_action_just_released("ui_left") || input->is_action_just_released("ui_right")) {
				gamepad_event_delay_ms = DEFAULT_GAMEPAD_EVENT_DELAY_MS;
				set_process_internal(false);
				return;
			}

			gamepad_event_delay_ms -= get_process_delta_time();
			if (gamepad_event_delay_ms <= 0) {
				gamepad_event_delay_ms = GAMEPAD_EVENT_REPEAT_RATE_MS + gamepad_event_delay_ms;
				if (input->is_action_pressed("ui_right")) {
					is_layout_rtl() ? select_previous_available() : select_next_available();
				}

				if (input->is_action_pressed("ui_left")) {
					is_layout_rtl() ? select_next_available() : select_previous_available();
				}
			}
		} break;

		case NOTIFICATION_LAYOUT_DIRECTION_CHANGED: {
			queue_redraw();
		} break;

		case NOTIFICATION_THEME_CHANGED:
		case NOTIFICATION_TRANSLATION_CHANGED: {
			for (int i = 0; i < tabs.size(); ++i) {
				_shape(i);
			}

			queue_redraw();
			update_minimum_size();

			[[fallthrough]];
		}
		case NOTIFICATION_RESIZED: {
			int ofs_old = offset;
			int max_old = max_drawn_tab;

			_update_cache();
			_ensure_no_over_offset();

			if (scroll_to_selected && (offset != ofs_old || max_drawn_tab != max_old)) {
				ensure_tab_visible(current);
			}
		} break;

		case NOTIFICATION_DRAG_END: {
			if (dragging_valid_tab) {
				dragging_valid_tab = false;
				queue_redraw();
			}
		} break;

		case NOTIFICATION_DRAW: {
			bool rtl = is_layout_rtl();
			Vector2 size = get_size();

			if (tabs.is_empty()) {
				// Draw the drop indicator where the first tab would be if there are no tabs.
				if (dragging_valid_tab) {
					int x = rtl ? size.x : 0;
					theme_cache.drop_mark_icon->draw(get_canvas_item(), Point2(x - (theme_cache.drop_mark_icon->get_width() / 2), (size.height - theme_cache.drop_mark_icon->get_height()) / 2), theme_cache.drop_mark_color);
				}

				return;
			}

			int limit_minus_buttons = size.width - theme_cache.increment_icon->get_width() - theme_cache.decrement_icon->get_width();

			int ofs = tabs[offset].ofs_cache;

			// Draw unselected tabs in the back.
			for (int i = offset; i <= max_drawn_tab; i++) {
				if (tabs[i].hidden) {
					continue;
				}

				if (i != current) {
					Ref<StyleBox> sb;
					Color col;

					if (tabs[i].disabled) {
						sb = theme_cache.tab_disabled_style;
						col = theme_cache.font_disabled_color;
					} else if (i == hover) {
						sb = theme_cache.tab_hovered_style;
						col = theme_cache.font_hovered_color;
					} else {
						sb = theme_cache.tab_unselected_style;
						col = theme_cache.font_unselected_color;
					}

					_draw_tab(sb, col, i, rtl ? size.width - ofs - tabs[i].size_cache : ofs, false);
				}

				ofs += tabs[i].size_cache;
			}

			// Draw selected tab in the front, but only if it's visible.
			if (current >= offset && current <= max_drawn_tab && !tabs[current].hidden) {
				Ref<StyleBox> sb = tabs[current].disabled ? theme_cache.tab_disabled_style : theme_cache.tab_selected_style;
				float x = rtl ? size.width - tabs[current].ofs_cache - tabs[current].size_cache : tabs[current].ofs_cache;

				_draw_tab(sb, theme_cache.font_selected_color, current, x, has_focus());
			}

			if (buttons_visible) {
				int vofs = (size.height - theme_cache.increment_icon->get_size().height) / 2;

				if (rtl) {
					if (missing_right) {
						draw_texture(highlight_arrow == 1 ? theme_cache.decrement_hl_icon : theme_cache.decrement_icon, Point2(0, vofs));
					} else {
						draw_texture(theme_cache.decrement_icon, Point2(0, vofs), Color(1, 1, 1, 0.5));
					}

					if (offset > 0) {
						draw_texture(highlight_arrow == 0 ? theme_cache.increment_hl_icon : theme_cache.increment_icon, Point2(theme_cache.increment_icon->get_size().width, vofs));
					} else {
						draw_texture(theme_cache.increment_icon, Point2(theme_cache.increment_icon->get_size().width, vofs), Color(1, 1, 1, 0.5));
					}
				} else {
					if (offset > 0) {
						draw_texture(highlight_arrow == 0 ? theme_cache.decrement_hl_icon : theme_cache.decrement_icon, Point2(limit_minus_buttons, vofs));
					} else {
						draw_texture(theme_cache.decrement_icon, Point2(limit_minus_buttons, vofs), Color(1, 1, 1, 0.5));
					}

					if (missing_right) {
						draw_texture(highlight_arrow == 1 ? theme_cache.increment_hl_icon : theme_cache.increment_icon, Point2(limit_minus_buttons + theme_cache.decrement_icon->get_size().width, vofs));
					} else {
						draw_texture(theme_cache.increment_icon, Point2(limit_minus_buttons + theme_cache.decrement_icon->get_size().width, vofs), Color(1, 1, 1, 0.5));
					}
				}
			}

			if (dragging_valid_tab) {
				int x;

				int tab_hover = get_hovered_tab();
				if (tab_hover != -1) {
					Rect2 tab_rect = get_tab_rect(tab_hover);

					x = tab_rect.position.x;
					if (get_local_mouse_position().x > x + tab_rect.size.width / 2) {
						x += tab_rect.size.width;
					}
				} else {
					if (rtl ^ (get_local_mouse_position().x < get_tab_rect(0).position.x)) {
						x = get_tab_rect(0).position.x;
						if (rtl) {
							x += get_tab_rect(0).size.width;
						}
					} else {
						Rect2 tab_rect = get_tab_rect(get_tab_count() - 1);

						x = tab_rect.position.x;
						if (!rtl) {
							x += tab_rect.size.width;
						}
					}
				}

				theme_cache.drop_mark_icon->draw(get_canvas_item(), Point2(x - theme_cache.drop_mark_icon->get_width() / 2, (size.height - theme_cache.drop_mark_icon->get_height()) / 2), theme_cache.drop_mark_color);
			}
		} break;
	}
}

void TabBar::_draw_tab(Ref<StyleBox> &p_tab_style, Color &p_font_color, int p_index, float p_x, bool p_focus) {
	RID ci = get_canvas_item();
	bool rtl = is_layout_rtl();

	Rect2 sb_rect = Rect2(p_x, 0, tabs[p_index].size_cache, get_size().height);
	if (tab_style_v_flip) {
		draw_set_transform(Point2(0.0, p_tab_style->get_draw_rect(sb_rect).size.y), 0.0, Size2(1.0, -1.0));
	}
	p_tab_style->draw(ci, sb_rect);
	if (tab_style_v_flip) {
		draw_set_transform(Point2(), 0.0, Size2(1.0, 1.0));
	}
	if (p_focus) {
		Ref<StyleBox> focus_style = theme_cache.tab_focus_style;
		focus_style->draw(ci, sb_rect);
	}

	p_x += rtl ? tabs[p_index].size_cache - p_tab_style->get_margin(SIDE_LEFT) : p_tab_style->get_margin(SIDE_LEFT);

	Size2i sb_ms = p_tab_style->get_minimum_size();

	// Draw the icon.
	Ref<Texture2D> icon = tabs[p_index].icon;
	if (icon.is_valid()) {
		const Size2 icon_size = _get_tab_icon_size(p_index);
		const Point2 icon_pos = Point2i(rtl ? p_x - icon_size.width : p_x, p_tab_style->get_margin(SIDE_TOP) + ((sb_rect.size.y - sb_ms.y) - icon_size.height) / 2);
		icon->draw_rect(ci, Rect2(icon_pos, icon_size));

		p_x = rtl ? p_x - icon_size.width - theme_cache.h_separation : p_x + icon_size.width + theme_cache.h_separation;
	}

	// Draw the text.
	if (!tabs[p_index].text.is_empty()) {
		Point2i text_pos = Point2i(rtl ? p_x - tabs[p_index].size_text : p_x,
				p_tab_style->get_margin(SIDE_TOP) + ((sb_rect.size.y - sb_ms.y) - tabs[p_index].text_buf->get_size().y) / 2);

		if (theme_cache.outline_size > 0 && theme_cache.font_outline_color.a > 0) {
			tabs[p_index].text_buf->draw_outline(ci, text_pos, theme_cache.outline_size, theme_cache.font_outline_color);
		}
		tabs[p_index].text_buf->draw(ci, text_pos, p_font_color);

		p_x = rtl ? p_x - tabs[p_index].size_text - theme_cache.h_separation : p_x + tabs[p_index].size_text + theme_cache.h_separation;
	}

	// Draw and calculate rect of the right button.
	if (tabs[p_index].right_button.is_valid()) {
		Ref<StyleBox> style = theme_cache.button_hl_style;
		Ref<Texture2D> rb = tabs[p_index].right_button;

		Rect2 rb_rect;
		rb_rect.size = style->get_minimum_size() + rb->get_size();
		rb_rect.position.x = rtl ? p_x - rb_rect.size.width : p_x;
		rb_rect.position.y = p_tab_style->get_margin(SIDE_TOP) + ((sb_rect.size.y - sb_ms.y) - (rb_rect.size.y)) / 2;

		tabs.write[p_index].rb_rect = rb_rect;

		if (rb_hover == p_index) {
			if (rb_pressing) {
				theme_cache.button_pressed_style->draw(ci, rb_rect);
			} else {
				style->draw(ci, rb_rect);
			}
		}

		rb->draw(ci, Point2i(rb_rect.position.x + style->get_margin(SIDE_LEFT), rb_rect.position.y + style->get_margin(SIDE_TOP)));

		p_x = rtl ? rb_rect.position.x : rb_rect.position.x + rb_rect.size.width;
	} else {
		tabs.write[p_index].rb_rect = Rect2();
	}

	// Draw and calculate rect of the close button.
	if (cb_displaypolicy == CLOSE_BUTTON_SHOW_ALWAYS || (cb_displaypolicy == CLOSE_BUTTON_SHOW_ACTIVE_ONLY && p_index == current)) {
		Ref<StyleBox> style = theme_cache.button_hl_style;
		Ref<Texture2D> cb = theme_cache.close_icon;

		Rect2 cb_rect;
		cb_rect.size = style->get_minimum_size() + cb->get_size();
		cb_rect.position.x = rtl ? p_x - cb_rect.size.width : p_x;
		cb_rect.position.y = p_tab_style->get_margin(SIDE_TOP) + ((sb_rect.size.y - sb_ms.y) - (cb_rect.size.y)) / 2;

		tabs.write[p_index].cb_rect = cb_rect;

		if (!tabs[p_index].disabled && cb_hover == p_index) {
			if (cb_pressing) {
				theme_cache.button_pressed_style->draw(ci, cb_rect);
			} else {
				style->draw(ci, cb_rect);
			}
		}

		cb->draw(ci, Point2i(cb_rect.position.x + style->get_margin(SIDE_LEFT), cb_rect.position.y + style->get_margin(SIDE_TOP)));
	} else {
		tabs.write[p_index].cb_rect = Rect2();
	}
}

void TabBar::set_tab_count(int p_count) {
	if (p_count == tabs.size()) {
		return;
	}

	ERR_FAIL_COND(p_count < 0);
	tabs.resize(p_count);

	if (p_count == 0) {
		offset = 0;
		max_drawn_tab = 0;
		current = -1;
		previous = -1;
	} else {
		offset = MIN(offset, p_count - 1);
		max_drawn_tab = MIN(max_drawn_tab, p_count - 1);
		current = MIN(current, p_count - 1);
		// Fix range if unable to deselect.
		if (current == -1 && !_can_deselect()) {
			current = 0;
		}

		_update_cache();
		_ensure_no_over_offset();
		if (scroll_to_selected) {
			ensure_tab_visible(current);
		}
	}

	if (!initialized) {
		initialized = true;
		if (queued_current != CURRENT_TAB_UNINITIALIZED && queued_current != current) {
			set_current_tab(queued_current);
		}
	}

	queue_redraw();
	update_minimum_size();
	notify_property_list_changed();
}

int TabBar::get_tab_count() const {
	return tabs.size();
}

void TabBar::set_current_tab(int p_current) {
	if (p_current == -1) {
		// An index of -1 is only valid if deselecting is enabled or there are no valid tabs.
		ERR_FAIL_COND_MSG(!_can_deselect(), "Cannot deselect tabs, deselection is not enabled.");
	} else {
		if (!initialized && p_current >= get_tab_count()) {
			queued_current = p_current;
			return;
		}
		ERR_FAIL_INDEX(p_current, get_tab_count());
	}

	previous = current;
	current = p_current;

	if (current == previous) {
		emit_signal(SNAME("tab_selected"), current);
		return;
	}

	emit_signal(SNAME("tab_selected"), current);

	_update_cache();
	if (scroll_to_selected) {
		ensure_tab_visible(current);
	}
	queue_redraw();

	emit_signal(SNAME("tab_changed"), p_current);
}

int TabBar::get_current_tab() const {
	return current;
}

int TabBar::get_previous_tab() const {
	return previous;
}

int TabBar::get_hovered_tab() const {
	return hover;
}

bool TabBar::select_previous_available() {
	const int offset_end = (get_current_tab() + 1);
	for (int i = 1; i < offset_end; i++) {
		int target_tab = get_current_tab() - i;
		if (target_tab < 0) {
			target_tab += get_tab_count();
		}
		if (!is_tab_disabled(target_tab) && !is_tab_hidden(target_tab)) {
			set_current_tab(target_tab);
			return true;
		}
	}
	return false;
}

bool TabBar::select_next_available() {
	const int offset_end = (get_tab_count() - get_current_tab());
	for (int i = 1; i < offset_end; i++) {
		int target_tab = (get_current_tab() + i) % get_tab_count();
		if (!is_tab_disabled(target_tab) && !is_tab_hidden(target_tab)) {
			set_current_tab(target_tab);
			return true;
		}
	}
	return false;
}

void TabBar::set_tab_offset(int p_offset) {
	ERR_FAIL_INDEX(p_offset, tabs.size());
	offset = p_offset;
	_update_cache();
	queue_redraw();
}

int TabBar::get_tab_offset() const {
	return offset;
}

bool TabBar::get_offset_buttons_visible() const {
	return buttons_visible;
}

void TabBar::set_tab_title(int p_tab, const String &p_title) {
	ERR_FAIL_INDEX(p_tab, tabs.size());

	if (tabs[p_tab].text == p_title) {
		return;
	}

	tabs.write[p_tab].text = p_title;

	_shape(p_tab);
	_update_cache();
	_ensure_no_over_offset();
	if (scroll_to_selected) {
		ensure_tab_visible(current);
	}
	queue_redraw();
	update_minimum_size();
}

String TabBar::get_tab_title(int p_tab) const {
	ERR_FAIL_INDEX_V(p_tab, tabs.size(), "");
	return tabs[p_tab].text;
}

void TabBar::set_tab_tooltip(int p_tab, const String &p_tooltip) {
	ERR_FAIL_INDEX(p_tab, tabs.size());
	tabs.write[p_tab].tooltip = p_tooltip;
}

String TabBar::get_tab_tooltip(int p_tab) const {
	ERR_FAIL_INDEX_V(p_tab, tabs.size(), "");
	return tabs[p_tab].tooltip;
}

void TabBar::set_tab_text_direction(int p_tab, Control::TextDirection p_text_direction) {
	ERR_FAIL_INDEX(p_tab, tabs.size());
	ERR_FAIL_COND((int)p_text_direction < -1 || (int)p_text_direction > 3);

	if (tabs[p_tab].text_direction != p_text_direction) {
		tabs.write[p_tab].text_direction = p_text_direction;
		_shape(p_tab);
		queue_redraw();
	}
}

Control::TextDirection TabBar::get_tab_text_direction(int p_tab) const {
	ERR_FAIL_INDEX_V(p_tab, tabs.size(), Control::TEXT_DIRECTION_INHERITED);
	return tabs[p_tab].text_direction;
}

void TabBar::set_tab_language(int p_tab, const String &p_language) {
	ERR_FAIL_INDEX(p_tab, tabs.size());

	if (tabs[p_tab].language != p_language) {
		tabs.write[p_tab].language = p_language;
		_shape(p_tab);
		_update_cache();
		_ensure_no_over_offset();
		if (scroll_to_selected) {
			ensure_tab_visible(current);
		}
		queue_redraw();
		update_minimum_size();
	}
}

String TabBar::get_tab_language(int p_tab) const {
	ERR_FAIL_INDEX_V(p_tab, tabs.size(), "");
	return tabs[p_tab].language;
}

void TabBar::set_tab_icon(int p_tab, const Ref<Texture2D> &p_icon) {
	ERR_FAIL_INDEX(p_tab, tabs.size());

	if (tabs[p_tab].icon == p_icon) {
		return;
	}

	tabs.write[p_tab].icon = p_icon;

	_update_cache();
	_ensure_no_over_offset();
	if (scroll_to_selected) {
		ensure_tab_visible(current);
	}
	queue_redraw();
	update_minimum_size();
}

Ref<Texture2D> TabBar::get_tab_icon(int p_tab) const {
	ERR_FAIL_INDEX_V(p_tab, tabs.size(), Ref<Texture2D>());
	return tabs[p_tab].icon;
}

void TabBar::set_tab_icon_max_width(int p_tab, int p_width) {
	ERR_FAIL_INDEX(p_tab, tabs.size());

	if (tabs[p_tab].icon_max_width == p_width) {
		return;
	}

	tabs.write[p_tab].icon_max_width = p_width;

	_update_cache();
	_ensure_no_over_offset();
	if (scroll_to_selected) {
		ensure_tab_visible(current);
	}
	queue_redraw();
	update_minimum_size();
}

int TabBar::get_tab_icon_max_width(int p_tab) const {
	ERR_FAIL_INDEX_V(p_tab, tabs.size(), 0);
	return tabs[p_tab].icon_max_width;
}

void TabBar::set_tab_disabled(int p_tab, bool p_disabled) {
	ERR_FAIL_INDEX(p_tab, tabs.size());

	if (tabs[p_tab].disabled == p_disabled) {
		return;
	}

	tabs.write[p_tab].disabled = p_disabled;

	_update_cache();
	_ensure_no_over_offset();
	if (scroll_to_selected) {
		ensure_tab_visible(current);
	}
	queue_redraw();
	update_minimum_size();
}

bool TabBar::is_tab_disabled(int p_tab) const {
	ERR_FAIL_INDEX_V(p_tab, tabs.size(), false);
	return tabs[p_tab].disabled;
}

void TabBar::set_tab_hidden(int p_tab, bool p_hidden) {
	ERR_FAIL_INDEX(p_tab, tabs.size());

	if (tabs[p_tab].hidden == p_hidden) {
		return;
	}

	tabs.write[p_tab].hidden = p_hidden;

	_update_cache();
	_ensure_no_over_offset();
	if (scroll_to_selected) {
		ensure_tab_visible(current);
	}
	queue_redraw();
	update_minimum_size();
}

bool TabBar::is_tab_hidden(int p_tab) const {
	ERR_FAIL_INDEX_V(p_tab, tabs.size(), false);
	return tabs[p_tab].hidden;
}

void TabBar::set_tab_metadata(int p_tab, const Variant &p_metadata) {
	ERR_FAIL_INDEX(p_tab, tabs.size());
	tabs.write[p_tab].metadata = p_metadata;
}

Variant TabBar::get_tab_metadata(int p_tab) const {
	ERR_FAIL_INDEX_V(p_tab, tabs.size(), Variant());
	return tabs[p_tab].metadata;
}

void TabBar::set_tab_button_icon(int p_tab, const Ref<Texture2D> &p_icon) {
	ERR_FAIL_INDEX(p_tab, tabs.size());

	if (tabs[p_tab].right_button == p_icon) {
		return;
	}

	tabs.write[p_tab].right_button = p_icon;

	_update_cache();
	_ensure_no_over_offset();
	if (scroll_to_selected) {
		ensure_tab_visible(current);
	}
	queue_redraw();
	update_minimum_size();
}

Ref<Texture2D> TabBar::get_tab_button_icon(int p_tab) const {
	ERR_FAIL_INDEX_V(p_tab, tabs.size(), Ref<Texture2D>());
	return tabs[p_tab].right_button;
}

void TabBar::_update_hover() {
	if (!is_inside_tree()) {
		return;
	}

	ERR_FAIL_COND(tabs.is_empty());

	const Point2 &pos = get_local_mouse_position();
	// Test hovering to display right or close button.
	int hover_now = -1;
	int hover_buttons = -1;
	for (int i = offset; i <= max_drawn_tab; i++) {
		if (tabs[i].hidden) {
			continue;
		}

		Rect2 rect = get_tab_rect(i);
		if (rect.has_point(pos)) {
			hover_now = i;
		}

		if (tabs[i].rb_rect.has_point(pos)) {
			rb_hover = i;
			cb_hover = -1;
			hover_buttons = i;
		} else if (!tabs[i].disabled && tabs[i].cb_rect.has_point(pos)) {
			cb_hover = i;
			rb_hover = -1;
			hover_buttons = i;
		}

		if (hover_buttons != -1) {
			queue_redraw();
			break;
		}
	}

	if (hover != hover_now) {
		hover = hover_now;

		if (hover != -1) {
			emit_signal(SNAME("tab_hovered"), hover);
		}

		_update_cache();
		queue_redraw();
	}

	if (hover_buttons == -1) { // No hover.
		int rb_hover_old = rb_hover;
		int cb_hover_old = cb_hover;

		rb_hover = hover_buttons;
		cb_hover = hover_buttons;

		if (rb_hover != rb_hover_old || cb_hover != cb_hover_old) {
			queue_redraw();
		}
	}
}

void TabBar::_update_cache(bool p_update_hover) {
	if (tabs.is_empty()) {
		buttons_visible = false;
		return;
	}

	int limit = get_size().width;
	int limit_minus_buttons = limit - theme_cache.increment_icon->get_width() - theme_cache.decrement_icon->get_width();

	int w = 0;

	max_drawn_tab = tabs.size() - 1;

	for (int i = 0; i < tabs.size(); i++) {
		tabs.write[i].text_buf->set_width(-1);
		tabs.write[i].size_text = Math::ceil(tabs[i].text_buf->get_size().x);
		tabs.write[i].size_cache = get_tab_width(i);

		tabs.write[i].truncated = max_width > 0 && tabs[i].size_cache > max_width;
		if (tabs[i].truncated) {
			int size_textless = tabs[i].size_cache - tabs[i].size_text;
			int mw = MAX(size_textless, max_width);

			tabs.write[i].size_text = MAX(mw - size_textless, 1);
			tabs.write[i].text_buf->set_width(tabs[i].size_text);
			tabs.write[i].size_cache = size_textless + tabs[i].size_text;
		}

		if (i < offset || i > max_drawn_tab) {
			tabs.write[i].ofs_cache = 0;
			continue;
		}

		tabs.write[i].ofs_cache = w;

		if (tabs[i].hidden) {
			continue;
		}

		w += tabs[i].size_cache;

		// Check if all tabs would fit inside the area.
		if (clip_tabs && i > offset && (w > limit || (offset > 0 && w > limit_minus_buttons))) {
			tabs.write[i].ofs_cache = 0;

			w -= tabs[i].size_cache;
			max_drawn_tab = i - 1;

			while (w > limit_minus_buttons && max_drawn_tab > offset) {
				tabs.write[max_drawn_tab].ofs_cache = 0;

				if (!tabs[max_drawn_tab].hidden) {
					w -= tabs[max_drawn_tab].size_cache;
				}

				max_drawn_tab--;
			}
		}
	}

	missing_right = max_drawn_tab < tabs.size() - 1;
	buttons_visible = offset > 0 || missing_right;

	if (tab_alignment == ALIGNMENT_LEFT) {
		if (p_update_hover) {
			_update_hover();
		}
		return;
	}

	if (tab_alignment == ALIGNMENT_CENTER) {
		w = ((buttons_visible ? limit_minus_buttons : limit) - w) / 2;
	} else if (tab_alignment == ALIGNMENT_RIGHT) {
		w = (buttons_visible ? limit_minus_buttons : limit) - w;
	}

	for (int i = offset; i <= max_drawn_tab; i++) {
		tabs.write[i].ofs_cache = w;

		if (!tabs[i].hidden) {
			w += tabs[i].size_cache;
		}
	}

	if (p_update_hover) {
		_update_hover();
	}
}

void TabBar::_on_mouse_exited() {
	rb_hover = -1;
	cb_hover = -1;
	hover = -1;
	highlight_arrow = -1;
	dragging_valid_tab = false;

	_update_cache(false);
	queue_redraw();
}

void TabBar::add_tab(const String &p_str, const Ref<Texture2D> &p_icon) {
	Tab t;
	t.text = p_str;
	t.text_buf->set_direction(is_layout_rtl() ? TextServer::DIRECTION_RTL : TextServer::DIRECTION_LTR);
	t.icon = p_icon;
	tabs.push_back(t);

	_shape(tabs.size() - 1);
	_update_cache();
	if (scroll_to_selected) {
		ensure_tab_visible(current);
	}
	queue_redraw();
	update_minimum_size();

	if (tabs.size() == 1) {
		if (is_inside_tree()) {
			set_current_tab(0);
		} else {
			current = 0;
			previous = -1;
		}
	}
}

void TabBar::clear_tabs() {
	if (tabs.is_empty()) {
		return;
	}

	tabs.clear();
	offset = 0;
	max_drawn_tab = 0;
	current = -1;
	previous = -1;

	queue_redraw();
	update_minimum_size();
	notify_property_list_changed();
}

void TabBar::remove_tab(int p_idx) {
	ERR_FAIL_INDEX(p_idx, tabs.size());
	tabs.remove_at(p_idx);

	bool is_tab_changing = current == p_idx;

	if (current >= p_idx && current > 0) {
		current--;
	}
	if (previous >= p_idx && previous > 0) {
		previous--;
	}

	if (tabs.is_empty()) {
		offset = 0;
		max_drawn_tab = 0;
		current = -1;
		previous = -1;
	} else {
		if (current != -1) {
			// Try to change to a valid tab if possible (without firing the `tab_selected` signal).
			for (int i = current; i < tabs.size(); i++) {
				if (!is_tab_disabled(i) && !is_tab_hidden(i)) {
					current = i;
					break;
				}
			}
			// If nothing, try backwards.
			if (is_tab_disabled(current) || is_tab_hidden(current)) {
				for (int i = current - 1; i >= 0; i--) {
					if (!is_tab_disabled(i) && !is_tab_hidden(i)) {
						current = i;
						break;
					}
				}
			}
			// If still no valid tab, deselect.
			if (is_tab_disabled(current) || is_tab_hidden(current)) {
				current = -1;
			}
		}
		offset = MIN(offset, tabs.size() - 1);
		max_drawn_tab = MIN(max_drawn_tab, tabs.size() - 1);

		_update_cache();
		_ensure_no_over_offset();
		if (scroll_to_selected) {
			ensure_tab_visible(current);
		}
	}

	queue_redraw();
	update_minimum_size();
	notify_property_list_changed();

	if (is_tab_changing && is_inside_tree()) {
		emit_signal(SNAME("tab_changed"), current);
	}
}

Variant TabBar::get_drag_data(const Point2 &p_point) {
	if (!drag_to_rearrange_enabled) {
		return Control::get_drag_data(p_point); // Allow stuff like TabContainer to override it.
	}
	return _handle_get_drag_data("tab_bar_tab", p_point);
}

bool TabBar::can_drop_data(const Point2 &p_point, const Variant &p_data) const {
	if (!drag_to_rearrange_enabled) {
		return Control::can_drop_data(p_point, p_data); // Allow stuff like TabContainer to override it.
	}
	return _handle_can_drop_data("tab_bar_tab", p_point, p_data);
}

void TabBar::drop_data(const Point2 &p_point, const Variant &p_data) {
	if (!drag_to_rearrange_enabled) {
		Control::drop_data(p_point, p_data); // Allow stuff like TabContainer to override it.
		return;
	}

	_handle_drop_data("tab_bar_tab", p_point, p_data, callable_mp(this, &TabBar::move_tab), callable_mp(this, &TabBar::_move_tab_from));
}

Variant TabBar::_handle_get_drag_data(const String &p_type, const Point2 &p_point) {
	int tab_over = get_tab_idx_at_point(p_point);
	if (tab_over < 0) {
		return Variant();
	}

	HBoxContainer *drag_preview = memnew(HBoxContainer);

	if (tabs[tab_over].icon.is_valid()) {
		const Size2 icon_size = _get_tab_icon_size(tab_over);

		TextureRect *tf = memnew(TextureRect);
		tf->set_texture(tabs[tab_over].icon);
		tf->set_stretch_mode(TextureRect::STRETCH_KEEP_ASPECT_CENTERED);
		tf->set_expand_mode(TextureRect::EXPAND_IGNORE_SIZE);
		tf->set_custom_minimum_size(icon_size);

		drag_preview->add_child(tf);
	}

	Label *label = memnew(Label(get_tab_title(tab_over)));
	label->set_auto_translate_mode(get_auto_translate_mode()); // Reflect how the title is displayed.
	drag_preview->add_child(label);

	set_drag_preview(drag_preview);

	Dictionary drag_data;
	drag_data["type"] = p_type;
	drag_data["tab_index"] = tab_over;
	drag_data["from_path"] = get_path();

	return drag_data;
}

bool TabBar::_handle_can_drop_data(const String &p_type, const Point2 &p_point, const Variant &p_data) const {
	Dictionary d = p_data;
	if (!d.has("type")) {
		return false;
	}

	if (String(d["type"]) == p_type) {
		NodePath from_path = d["from_path"];
		NodePath to_path = get_path();
		if (from_path == to_path) {
			return true;
		} else if (get_tabs_rearrange_group() != -1) {
			// Drag and drop between other TabBars.
			Node *from_node = get_node(from_path);
			TabBar *from_tabs = Object::cast_to<TabBar>(from_node);
			if (from_tabs && from_tabs->get_tabs_rearrange_group() == get_tabs_rearrange_group()) {
				return true;
			}
		}
	}

	return false;
}

void TabBar::_handle_drop_data(const String &p_type, const Point2 &p_point, const Variant &p_data, const Callable &p_move_tab_callback, const Callable &p_move_tab_from_other_callback) {
	Dictionary d = p_data;
	if (!d.has("type")) {
		return;
	}

	if (String(d["type"]) == p_type) {
		int tab_from_id = d["tab_index"];
		int hover_now = get_tab_idx_at_point(p_point);
		NodePath from_path = d["from_path"];
		NodePath to_path = get_path();

		if (from_path == to_path) {
			if (tab_from_id == hover_now) {
				return;
			}

			// Drop the new tab to the left or right depending on where the target tab is being hovered.
			if (hover_now != -1) {
				Rect2 tab_rect = get_tab_rect(hover_now);
				if (is_layout_rtl() ^ (p_point.x <= tab_rect.position.x + tab_rect.size.width / 2)) {
					if (hover_now > tab_from_id) {
						hover_now -= 1;
					}
				} else if (tab_from_id > hover_now) {
					hover_now += 1;
				}
			} else {
				int x = tabs.is_empty() ? 0 : get_tab_rect(0).position.x;
				hover_now = is_layout_rtl() ^ (p_point.x < x) ? 0 : get_tab_count() - 1;
			}

			p_move_tab_callback.call(tab_from_id, hover_now);
			if (!is_tab_disabled(hover_now)) {
				emit_signal(SNAME("active_tab_rearranged"), hover_now);
				set_current_tab(hover_now);
			}
		} else if (get_tabs_rearrange_group() != -1) {
			// Drag and drop between Tabs.

			Node *from_node = get_node(from_path);
			TabBar *from_tabs = Object::cast_to<TabBar>(from_node);

			if (from_tabs && from_tabs->get_tabs_rearrange_group() == get_tabs_rearrange_group()) {
				if (tab_from_id >= from_tabs->get_tab_count()) {
					return;
				}

				// Drop the new tab to the left or right depending on where the target tab is being hovered.
				if (hover_now != -1) {
					Rect2 tab_rect = get_tab_rect(hover_now);
					if (is_layout_rtl() ^ (p_point.x > tab_rect.position.x + tab_rect.size.width / 2)) {
						hover_now += 1;
					}
				} else {
					hover_now = tabs.is_empty() || (is_layout_rtl() ^ (p_point.x < get_tab_rect(0).position.x)) ? 0 : get_tab_count();
				}

				p_move_tab_from_other_callback.call(from_tabs, tab_from_id, hover_now);
			}
		}
	}
}

void TabBar::_move_tab_from(TabBar *p_from_tabbar, int p_from_index, int p_to_index) {
	Tab moving_tab = p_from_tabbar->tabs[p_from_index];
	p_from_tabbar->remove_tab(p_from_index);
	tabs.insert(p_to_index, moving_tab);

	if (tabs.size() > 1) {
		if (current >= p_to_index) {
			current++;
		}
		if (previous >= p_to_index) {
			previous++;
		}
	}

	if (!is_tab_disabled(p_to_index)) {
		set_current_tab(p_to_index);
	} else {
		_update_cache();
		queue_redraw();
	}

	update_minimum_size();
}

int TabBar::get_tab_idx_at_point(const Point2 &p_point) const {
	int hover_now = -1;

	if (!tabs.is_empty()) {
		for (int i = offset; i <= max_drawn_tab; i++) {
			Rect2 rect = get_tab_rect(i);
			if (rect.has_point(p_point)) {
				hover_now = i;
			}
		}
	}

	return hover_now;
}

void TabBar::set_tab_alignment(AlignmentMode p_alignment) {
	ERR_FAIL_INDEX(p_alignment, ALIGNMENT_MAX);

	if (tab_alignment == p_alignment) {
		return;
	}

	tab_alignment = p_alignment;

	_update_cache();
	queue_redraw();
}

TabBar::AlignmentMode TabBar::get_tab_alignment() const {
	return tab_alignment;
}

void TabBar::set_clip_tabs(bool p_clip_tabs) {
	if (clip_tabs == p_clip_tabs) {
		return;
	}
	clip_tabs = p_clip_tabs;

	if (!clip_tabs) {
		offset = 0;
		max_drawn_tab = 0;
	}

	_update_cache();
	if (scroll_to_selected) {
		ensure_tab_visible(current);
	}
	queue_redraw();
	update_minimum_size();
}

bool TabBar::get_clip_tabs() const {
	return clip_tabs;
}

void TabBar::set_tab_style_v_flip(bool p_tab_style_v_flip) {
	tab_style_v_flip = p_tab_style_v_flip;
}

void TabBar::move_tab(int p_from, int p_to) {
	if (p_from == p_to) {
		return;
	}

	ERR_FAIL_INDEX(p_from, tabs.size());
	ERR_FAIL_INDEX(p_to, tabs.size());

	Tab tab_from = tabs[p_from];
	tabs.remove_at(p_from);
	tabs.insert(p_to, tab_from);

	if (current == p_from) {
		current = p_to;
	} else if (current > p_from && current <= p_to) {
		current--;
	} else if (current < p_from && current >= p_to) {
		current++;
	}

	if (previous == p_from) {
		previous = p_to;
	} else if (previous > p_from && previous <= p_to) {
		previous--;
	} else if (previous < p_from && previous >= p_to) {
		previous++;
	}

	_update_cache();
	_ensure_no_over_offset();
	if (scroll_to_selected) {
		ensure_tab_visible(current);
	}
	queue_redraw();
	notify_property_list_changed();
}

int TabBar::get_tab_width(int p_idx) const {
	ERR_FAIL_INDEX_V(p_idx, tabs.size(), 0);

	Ref<StyleBox> style;

	if (tabs[p_idx].disabled) {
		style = theme_cache.tab_disabled_style;
	} else if (current == p_idx) {
		style = theme_cache.tab_selected_style;
		// Always pick the widest style between hovered and unselected, to avoid an infinite loop when switching tabs with the mouse.
	} else if (theme_cache.tab_hovered_style->get_minimum_size().width > theme_cache.tab_unselected_style->get_minimum_size().width) {
		style = theme_cache.tab_hovered_style;
	} else {
		style = theme_cache.tab_unselected_style;
	}
	int x = style->get_minimum_size().width;

	if (tabs[p_idx].icon.is_valid()) {
		const Size2 icon_size = _get_tab_icon_size(p_idx);
		x += icon_size.width + theme_cache.h_separation;
	}

	if (!tabs[p_idx].text.is_empty()) {
		x += tabs[p_idx].size_text + theme_cache.h_separation;
	}

	bool close_visible = cb_displaypolicy == CLOSE_BUTTON_SHOW_ALWAYS || (cb_displaypolicy == CLOSE_BUTTON_SHOW_ACTIVE_ONLY && p_idx == current);

	if (tabs[p_idx].right_button.is_valid()) {
		Ref<StyleBox> btn_style = theme_cache.button_hl_style;
		Ref<Texture2D> rb = tabs[p_idx].right_button;

		if (close_visible) {
			x += btn_style->get_minimum_size().width + rb->get_width();
		} else {
			x += btn_style->get_margin(SIDE_LEFT) + rb->get_width() + theme_cache.h_separation;
		}
	}

	if (close_visible) {
		Ref<StyleBox> btn_style = theme_cache.button_hl_style;
		Ref<Texture2D> cb = theme_cache.close_icon;
		x += btn_style->get_margin(SIDE_LEFT) + cb->get_width() + theme_cache.h_separation;
	}

	if (x > style->get_minimum_size().width) {
		x -= theme_cache.h_separation;
	}

	return x;
}

Size2 TabBar::_get_tab_icon_size(int p_index) const {
	ERR_FAIL_INDEX_V(p_index, tabs.size(), Size2());
	const TabBar::Tab &tab = tabs[p_index];
	Size2 icon_size = tab.icon->get_size();

	int icon_max_width = 0;
	if (theme_cache.icon_max_width > 0) {
		icon_max_width = theme_cache.icon_max_width;
	}
	if (tab.icon_max_width > 0 && (icon_max_width == 0 || tab.icon_max_width < icon_max_width)) {
		icon_max_width = tab.icon_max_width;
	}

	if (icon_max_width > 0 && icon_size.width > icon_max_width) {
		icon_size.height = icon_size.height * icon_max_width / icon_size.width;
		icon_size.width = icon_max_width;
	}

	return icon_size;
}

void TabBar::_ensure_no_over_offset() {
	if (!is_inside_tree() || !buttons_visible) {
		return;
	}

	int limit_minus_buttons = get_size().width - theme_cache.increment_icon->get_width() - theme_cache.decrement_icon->get_width();

	int prev_offset = offset;

	int total_w = tabs[max_drawn_tab].ofs_cache + tabs[max_drawn_tab].size_cache - tabs[offset].ofs_cache;
	for (int i = offset; i > 0; i--) {
		if (tabs[i - 1].hidden) {
			continue;
		}

		total_w += tabs[i - 1].size_cache;

		if (total_w < limit_minus_buttons) {
			offset--;
		} else {
			break;
		}
	}

	if (prev_offset != offset) {
		_update_cache();
		queue_redraw();
	}
}

bool TabBar::_can_deselect() const {
	if (deselect_enabled) {
		return true;
	}
	// All tabs must be disabled or hidden.
	for (const Tab &tab : tabs) {
		if (!tab.disabled && !tab.hidden) {
			return false;
		}
	}
	return true;
}

void TabBar::ensure_tab_visible(int p_idx) {
	if (p_idx == -1 || !is_inside_tree() || !buttons_visible) {
		return;
	}
	ERR_FAIL_INDEX(p_idx, tabs.size());

	if (tabs[p_idx].hidden || (p_idx >= offset && p_idx <= max_drawn_tab)) {
		return;
	}

	if (p_idx < offset) {
		offset = p_idx;
		_update_cache();
		queue_redraw();

		return;
	}

	int limit_minus_buttons = get_size().width - theme_cache.increment_icon->get_width() - theme_cache.decrement_icon->get_width();

	int total_w = tabs[max_drawn_tab].ofs_cache - tabs[offset].ofs_cache;
	for (int i = max_drawn_tab; i <= p_idx; i++) {
		if (tabs[i].hidden) {
			continue;
		}

		total_w += tabs[i].size_cache;
	}

	int prev_offset = offset;

	for (int i = offset; i < p_idx; i++) {
		if (tabs[i].hidden) {
			continue;
		}

		if (total_w > limit_minus_buttons) {
			total_w -= tabs[i].size_cache;
			offset++;
		} else {
			break;
		}
	}

	if (prev_offset != offset) {
		_update_cache();
		queue_redraw();
	}
}

Rect2 TabBar::get_tab_rect(int p_tab) const {
	ERR_FAIL_INDEX_V(p_tab, tabs.size(), Rect2());
	if (is_layout_rtl()) {
		return Rect2(get_size().width - tabs[p_tab].ofs_cache - tabs[p_tab].size_cache, 0, tabs[p_tab].size_cache, get_size().height);
	} else {
		return Rect2(tabs[p_tab].ofs_cache, 0, tabs[p_tab].size_cache, get_size().height);
	}
}

void TabBar::set_tab_close_display_policy(CloseButtonDisplayPolicy p_policy) {
	ERR_FAIL_INDEX(p_policy, CLOSE_BUTTON_MAX);

	if (cb_displaypolicy == p_policy) {
		return;
	}

	cb_displaypolicy = p_policy;

	_update_cache();
	_ensure_no_over_offset();
	if (scroll_to_selected) {
		ensure_tab_visible(current);
	}
	queue_redraw();
	update_minimum_size();
}

TabBar::CloseButtonDisplayPolicy TabBar::get_tab_close_display_policy() const {
	return cb_displaypolicy;
}

void TabBar::set_max_tab_width(int p_width) {
	ERR_FAIL_COND(p_width < 0);

	if (max_width == p_width) {
		return;
	}

	max_width = p_width;

	_update_cache();
	_ensure_no_over_offset();
	if (scroll_to_selected) {
		ensure_tab_visible(current);
	}
	queue_redraw();
	update_minimum_size();
}

int TabBar::get_max_tab_width() const {
	return max_width;
}

void TabBar::set_scrolling_enabled(bool p_enabled) {
	scrolling_enabled = p_enabled;
}

bool TabBar::get_scrolling_enabled() const {
	return scrolling_enabled;
}

void TabBar::set_drag_to_rearrange_enabled(bool p_enabled) {
	drag_to_rearrange_enabled = p_enabled;
}

bool TabBar::get_drag_to_rearrange_enabled() const {
	return drag_to_rearrange_enabled;
}

void TabBar::set_tabs_rearrange_group(int p_group_id) {
	tabs_rearrange_group = p_group_id;
}

int TabBar::get_tabs_rearrange_group() const {
	return tabs_rearrange_group;
}

void TabBar::set_scroll_to_selected(bool p_enabled) {
	scroll_to_selected = p_enabled;
	if (p_enabled) {
		ensure_tab_visible(current);
	}
}

bool TabBar::get_scroll_to_selected() const {
	return scroll_to_selected;
}

void TabBar::set_select_with_rmb(bool p_enabled) {
	select_with_rmb = p_enabled;
}

bool TabBar::get_select_with_rmb() const {
	return select_with_rmb;
}

void TabBar::set_deselect_enabled(bool p_enabled) {
	if (deselect_enabled == p_enabled) {
		return;
	}
	deselect_enabled = p_enabled;
	if (!deselect_enabled && current == -1 && !tabs.is_empty()) {
		select_next_available();
	}
}

bool TabBar::get_deselect_enabled() const {
	return deselect_enabled;
}

void TabBar::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_tab_count", "count"), &TabBar::set_tab_count);
	ClassDB::bind_method(D_METHOD("get_tab_count"), &TabBar::get_tab_count);
	ClassDB::bind_method(D_METHOD("set_current_tab", "tab_idx"), &TabBar::set_current_tab);
	ClassDB::bind_method(D_METHOD("get_current_tab"), &TabBar::get_current_tab);
	ClassDB::bind_method(D_METHOD("get_previous_tab"), &TabBar::get_previous_tab);
	ClassDB::bind_method(D_METHOD("select_previous_available"), &TabBar::select_previous_available);
	ClassDB::bind_method(D_METHOD("select_next_available"), &TabBar::select_next_available);
	ClassDB::bind_method(D_METHOD("set_tab_title", "tab_idx", "title"), &TabBar::set_tab_title);
	ClassDB::bind_method(D_METHOD("get_tab_title", "tab_idx"), &TabBar::get_tab_title);
	ClassDB::bind_method(D_METHOD("set_tab_tooltip", "tab_idx", "tooltip"), &TabBar::set_tab_tooltip);
	ClassDB::bind_method(D_METHOD("get_tab_tooltip", "tab_idx"), &TabBar::get_tab_tooltip);
	ClassDB::bind_method(D_METHOD("set_tab_text_direction", "tab_idx", "direction"), &TabBar::set_tab_text_direction);
	ClassDB::bind_method(D_METHOD("get_tab_text_direction", "tab_idx"), &TabBar::get_tab_text_direction);
	ClassDB::bind_method(D_METHOD("set_tab_language", "tab_idx", "language"), &TabBar::set_tab_language);
	ClassDB::bind_method(D_METHOD("get_tab_language", "tab_idx"), &TabBar::get_tab_language);
	ClassDB::bind_method(D_METHOD("set_tab_icon", "tab_idx", "icon"), &TabBar::set_tab_icon);
	ClassDB::bind_method(D_METHOD("get_tab_icon", "tab_idx"), &TabBar::get_tab_icon);
	ClassDB::bind_method(D_METHOD("set_tab_icon_max_width", "tab_idx", "width"), &TabBar::set_tab_icon_max_width);
	ClassDB::bind_method(D_METHOD("get_tab_icon_max_width", "tab_idx"), &TabBar::get_tab_icon_max_width);
	ClassDB::bind_method(D_METHOD("set_tab_button_icon", "tab_idx", "icon"), &TabBar::set_tab_button_icon);
	ClassDB::bind_method(D_METHOD("get_tab_button_icon", "tab_idx"), &TabBar::get_tab_button_icon);
	ClassDB::bind_method(D_METHOD("set_tab_disabled", "tab_idx", "disabled"), &TabBar::set_tab_disabled);
	ClassDB::bind_method(D_METHOD("is_tab_disabled", "tab_idx"), &TabBar::is_tab_disabled);
	ClassDB::bind_method(D_METHOD("set_tab_hidden", "tab_idx", "hidden"), &TabBar::set_tab_hidden);
	ClassDB::bind_method(D_METHOD("is_tab_hidden", "tab_idx"), &TabBar::is_tab_hidden);
	ClassDB::bind_method(D_METHOD("set_tab_metadata", "tab_idx", "metadata"), &TabBar::set_tab_metadata);
	ClassDB::bind_method(D_METHOD("get_tab_metadata", "tab_idx"), &TabBar::get_tab_metadata);
	ClassDB::bind_method(D_METHOD("remove_tab", "tab_idx"), &TabBar::remove_tab);
	ClassDB::bind_method(D_METHOD("add_tab", "title", "icon"), &TabBar::add_tab, DEFVAL(""), DEFVAL(Ref<Texture2D>()));
	ClassDB::bind_method(D_METHOD("get_tab_idx_at_point", "point"), &TabBar::get_tab_idx_at_point);
	ClassDB::bind_method(D_METHOD("set_tab_alignment", "alignment"), &TabBar::set_tab_alignment);
	ClassDB::bind_method(D_METHOD("get_tab_alignment"), &TabBar::get_tab_alignment);
	ClassDB::bind_method(D_METHOD("set_clip_tabs", "clip_tabs"), &TabBar::set_clip_tabs);
	ClassDB::bind_method(D_METHOD("get_clip_tabs"), &TabBar::get_clip_tabs);
	ClassDB::bind_method(D_METHOD("get_tab_offset"), &TabBar::get_tab_offset);
	ClassDB::bind_method(D_METHOD("get_offset_buttons_visible"), &TabBar::get_offset_buttons_visible);
	ClassDB::bind_method(D_METHOD("ensure_tab_visible", "idx"), &TabBar::ensure_tab_visible);
	ClassDB::bind_method(D_METHOD("get_tab_rect", "tab_idx"), &TabBar::get_tab_rect);
	ClassDB::bind_method(D_METHOD("move_tab", "from", "to"), &TabBar::move_tab);
	ClassDB::bind_method(D_METHOD("set_tab_close_display_policy", "policy"), &TabBar::set_tab_close_display_policy);
	ClassDB::bind_method(D_METHOD("get_tab_close_display_policy"), &TabBar::get_tab_close_display_policy);
	ClassDB::bind_method(D_METHOD("set_max_tab_width", "width"), &TabBar::set_max_tab_width);
	ClassDB::bind_method(D_METHOD("get_max_tab_width"), &TabBar::get_max_tab_width);
	ClassDB::bind_method(D_METHOD("set_scrolling_enabled", "enabled"), &TabBar::set_scrolling_enabled);
	ClassDB::bind_method(D_METHOD("get_scrolling_enabled"), &TabBar::get_scrolling_enabled);
	ClassDB::bind_method(D_METHOD("set_drag_to_rearrange_enabled", "enabled"), &TabBar::set_drag_to_rearrange_enabled);
	ClassDB::bind_method(D_METHOD("get_drag_to_rearrange_enabled"), &TabBar::get_drag_to_rearrange_enabled);
	ClassDB::bind_method(D_METHOD("set_tabs_rearrange_group", "group_id"), &TabBar::set_tabs_rearrange_group);
	ClassDB::bind_method(D_METHOD("get_tabs_rearrange_group"), &TabBar::get_tabs_rearrange_group);
	ClassDB::bind_method(D_METHOD("set_scroll_to_selected", "enabled"), &TabBar::set_scroll_to_selected);
	ClassDB::bind_method(D_METHOD("get_scroll_to_selected"), &TabBar::get_scroll_to_selected);
	ClassDB::bind_method(D_METHOD("set_select_with_rmb", "enabled"), &TabBar::set_select_with_rmb);
	ClassDB::bind_method(D_METHOD("get_select_with_rmb"), &TabBar::get_select_with_rmb);
	ClassDB::bind_method(D_METHOD("set_deselect_enabled", "enabled"), &TabBar::set_deselect_enabled);
	ClassDB::bind_method(D_METHOD("get_deselect_enabled"), &TabBar::get_deselect_enabled);
	ClassDB::bind_method(D_METHOD("clear_tabs"), &TabBar::clear_tabs);

	ADD_SIGNAL(MethodInfo("tab_selected", PropertyInfo(Variant::INT, "tab")));
	ADD_SIGNAL(MethodInfo("tab_changed", PropertyInfo(Variant::INT, "tab")));
	ADD_SIGNAL(MethodInfo("tab_clicked", PropertyInfo(Variant::INT, "tab")));
	ADD_SIGNAL(MethodInfo("tab_rmb_clicked", PropertyInfo(Variant::INT, "tab")));
	ADD_SIGNAL(MethodInfo("tab_close_pressed", PropertyInfo(Variant::INT, "tab")));
	ADD_SIGNAL(MethodInfo("tab_button_pressed", PropertyInfo(Variant::INT, "tab")));
	ADD_SIGNAL(MethodInfo("tab_hovered", PropertyInfo(Variant::INT, "tab")));
	ADD_SIGNAL(MethodInfo("active_tab_rearranged", PropertyInfo(Variant::INT, "idx_to")));

	ADD_PROPERTY(PropertyInfo(Variant::INT, "current_tab", PROPERTY_HINT_RANGE, "-1,4096,1"), "set_current_tab", "get_current_tab");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "tab_alignment", PROPERTY_HINT_ENUM, "Left,Center,Right"), "set_tab_alignment", "get_tab_alignment");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "clip_tabs"), "set_clip_tabs", "get_clip_tabs");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "tab_close_display_policy", PROPERTY_HINT_ENUM, "Show Never,Show Active Only,Show Always"), "set_tab_close_display_policy", "get_tab_close_display_policy");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "max_tab_width", PROPERTY_HINT_RANGE, "0,99999,1,suffix:px"), "set_max_tab_width", "get_max_tab_width");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "scrolling_enabled"), "set_scrolling_enabled", "get_scrolling_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "drag_to_rearrange_enabled"), "set_drag_to_rearrange_enabled", "get_drag_to_rearrange_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "tabs_rearrange_group"), "set_tabs_rearrange_group", "get_tabs_rearrange_group");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "scroll_to_selected"), "set_scroll_to_selected", "get_scroll_to_selected");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "select_with_rmb"), "set_select_with_rmb", "get_select_with_rmb");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "deselect_enabled"), "set_deselect_enabled", "get_deselect_enabled");

	ADD_ARRAY_COUNT("Tabs", "tab_count", "set_tab_count", "get_tab_count", "tab_");

	BIND_ENUM_CONSTANT(ALIGNMENT_LEFT);
	BIND_ENUM_CONSTANT(ALIGNMENT_CENTER);
	BIND_ENUM_CONSTANT(ALIGNMENT_RIGHT);
	BIND_ENUM_CONSTANT(ALIGNMENT_MAX);

	BIND_ENUM_CONSTANT(CLOSE_BUTTON_SHOW_NEVER);
	BIND_ENUM_CONSTANT(CLOSE_BUTTON_SHOW_ACTIVE_ONLY);
	BIND_ENUM_CONSTANT(CLOSE_BUTTON_SHOW_ALWAYS);
	BIND_ENUM_CONSTANT(CLOSE_BUTTON_MAX);

	BIND_THEME_ITEM(Theme::DATA_TYPE_CONSTANT, TabBar, h_separation);
	BIND_THEME_ITEM(Theme::DATA_TYPE_CONSTANT, TabBar, icon_max_width);

	BIND_THEME_ITEM_CUSTOM(Theme::DATA_TYPE_STYLEBOX, TabBar, tab_unselected_style, "tab_unselected");
	BIND_THEME_ITEM_CUSTOM(Theme::DATA_TYPE_STYLEBOX, TabBar, tab_hovered_style, "tab_hovered");
	BIND_THEME_ITEM_CUSTOM(Theme::DATA_TYPE_STYLEBOX, TabBar, tab_selected_style, "tab_selected");
	BIND_THEME_ITEM_CUSTOM(Theme::DATA_TYPE_STYLEBOX, TabBar, tab_disabled_style, "tab_disabled");
	BIND_THEME_ITEM_CUSTOM(Theme::DATA_TYPE_STYLEBOX, TabBar, tab_focus_style, "tab_focus");

	BIND_THEME_ITEM_CUSTOM(Theme::DATA_TYPE_ICON, TabBar, increment_icon, "increment");
	BIND_THEME_ITEM_CUSTOM(Theme::DATA_TYPE_ICON, TabBar, increment_hl_icon, "increment_highlight");
	BIND_THEME_ITEM_CUSTOM(Theme::DATA_TYPE_ICON, TabBar, decrement_icon, "decrement");
	BIND_THEME_ITEM_CUSTOM(Theme::DATA_TYPE_ICON, TabBar, decrement_hl_icon, "decrement_highlight");
	BIND_THEME_ITEM_CUSTOM(Theme::DATA_TYPE_ICON, TabBar, drop_mark_icon, "drop_mark");
	BIND_THEME_ITEM(Theme::DATA_TYPE_COLOR, TabBar, drop_mark_color);

	BIND_THEME_ITEM(Theme::DATA_TYPE_COLOR, TabBar, font_selected_color);
	BIND_THEME_ITEM(Theme::DATA_TYPE_COLOR, TabBar, font_hovered_color);
	BIND_THEME_ITEM(Theme::DATA_TYPE_COLOR, TabBar, font_unselected_color);
	BIND_THEME_ITEM(Theme::DATA_TYPE_COLOR, TabBar, font_disabled_color);
	BIND_THEME_ITEM(Theme::DATA_TYPE_COLOR, TabBar, font_outline_color);

	BIND_THEME_ITEM(Theme::DATA_TYPE_FONT, TabBar, font);
	BIND_THEME_ITEM(Theme::DATA_TYPE_FONT_SIZE, TabBar, font_size);
	BIND_THEME_ITEM(Theme::DATA_TYPE_CONSTANT, TabBar, outline_size);

	BIND_THEME_ITEM_CUSTOM(Theme::DATA_TYPE_ICON, TabBar, close_icon, "close");
	BIND_THEME_ITEM_CUSTOM(Theme::DATA_TYPE_STYLEBOX, TabBar, button_pressed_style, "button_pressed");
	BIND_THEME_ITEM_CUSTOM(Theme::DATA_TYPE_STYLEBOX, TabBar, button_hl_style, "button_highlight");

	Tab defaults(true);

	base_property_helper.set_prefix("tab_");
	base_property_helper.set_array_length_getter(&TabBar::get_tab_count);
	base_property_helper.register_property(PropertyInfo(Variant::STRING, "title"), defaults.text, &TabBar::set_tab_title, &TabBar::get_tab_title);
	base_property_helper.register_property(PropertyInfo(Variant::STRING, "tooltip"), defaults.tooltip, &TabBar::set_tab_tooltip, &TabBar::get_tab_tooltip);
	base_property_helper.register_property(PropertyInfo(Variant::OBJECT, "icon", PROPERTY_HINT_RESOURCE_TYPE, "Texture2D"), defaults.icon, &TabBar::set_tab_icon, &TabBar::get_tab_icon);
	base_property_helper.register_property(PropertyInfo(Variant::BOOL, "disabled"), defaults.disabled, &TabBar::set_tab_disabled, &TabBar::is_tab_disabled);
	PropertyListHelper::register_base_helper(&base_property_helper);
}

TabBar::TabBar() {
	set_size(Size2(get_size().width, get_minimum_size().height));
	set_focus_mode(FOCUS_ALL);
	connect(SceneStringName(mouse_exited), callable_mp(this, &TabBar::_on_mouse_exited));

	property_helper.setup_for_instance(base_property_helper, this);
}

/**************************************************************************/
/*  slider.cpp                                                            */
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

#include "slider.h"

#include "scene/theme/theme_db.h"

Size2 Slider::get_minimum_size() const {
	Size2i ss = theme_cache.slider_style->get_minimum_size();
	Size2i rs = theme_cache.grabber_icon->get_size();

	if (orientation == HORIZONTAL) {
		return Size2i(ss.width, MAX(ss.height, rs.height));
	} else {
		return Size2i(MAX(ss.width, rs.width), ss.height);
	}
}

void Slider::gui_input(const Ref<InputEvent> &p_event) {
	ERR_FAIL_COND(p_event.is_null());

	if (!editable) {
		return;
	}

	Ref<InputEventMouseButton> mb = p_event;

	if (mb.is_valid()) {
		if (mb->get_button_index() == MouseButton::LEFT) {
			if (mb->is_pressed()) {
				Ref<Texture2D> grabber;
				if (mouse_inside || has_focus()) {
					grabber = theme_cache.grabber_hl_icon;
				} else {
					grabber = theme_cache.grabber_icon;
				}

				grab.pos = orientation == VERTICAL ? mb->get_position().y : mb->get_position().x;
				grab.value_before_dragging = get_as_ratio();
				emit_signal(SNAME("drag_started"));

				double grab_width = theme_cache.center_grabber ? 0.0 : (double)grabber->get_width();
				double grab_height = theme_cache.center_grabber ? 0.0 : (double)grabber->get_height();
				double max = orientation == VERTICAL ? get_size().height - grab_height : get_size().width - grab_width;
				set_block_signals(true);
				if (orientation == VERTICAL) {
					set_as_ratio(1 - (((double)grab.pos - (grab_height / 2.0)) / max));
				} else {
					double v = ((double)grab.pos - (grab_width / 2.0)) / max;
					set_as_ratio(is_layout_rtl() ? 1 - v : v);
				}
				set_block_signals(false);
				grab.active = true;
				grab.uvalue = get_as_ratio();

				_notify_shared_value_changed();
			} else {
				grab.active = false;

				const bool value_changed = !Math::is_equal_approx((double)grab.value_before_dragging, get_as_ratio());
				emit_signal(SNAME("drag_ended"), value_changed);
			}
		} else if (scrollable) {
			if (mb->is_pressed() && mb->get_button_index() == MouseButton::WHEEL_UP) {
				if (get_focus_mode() != FOCUS_NONE) {
					grab_focus();
				}
				set_value(get_value() + get_step());
			} else if (mb->is_pressed() && mb->get_button_index() == MouseButton::WHEEL_DOWN) {
				if (get_focus_mode() != FOCUS_NONE) {
					grab_focus();
				}
				set_value(get_value() - get_step());
			}
		}
	}

	Ref<InputEventMouseMotion> mm = p_event;

	if (mm.is_valid()) {
		if (grab.active) {
			Size2i size = get_size();
			Ref<Texture2D> grabber = theme_cache.grabber_hl_icon;
			double grab_width = theme_cache.center_grabber ? 0.0 : (double)grabber->get_width();
			double grab_height = theme_cache.center_grabber ? 0.0 : (double)grabber->get_height();
			double motion = (orientation == VERTICAL ? mm->get_position().y : mm->get_position().x) - grab.pos;
			if (orientation == VERTICAL) {
				motion = -motion;
			} else if (is_layout_rtl()) {
				motion = -motion;
			}
			double areasize = orientation == VERTICAL ? size.height - grab_height : size.width - grab_width;
			if (areasize <= 0) {
				return;
			}
			double umotion = motion / double(areasize);
			set_as_ratio(grab.uvalue + umotion);
		}
	}

	Input *input = Input::get_singleton();
	Ref<InputEventJoypadMotion> joypadmotion_event = p_event;
	Ref<InputEventJoypadButton> joypadbutton_event = p_event;
	bool is_joypad_event = (joypadmotion_event.is_valid() || joypadbutton_event.is_valid());

	if (mm.is_null() && mb.is_null()) {
		if (p_event->is_action_pressed("ui_left", true)) {
			if (orientation != HORIZONTAL) {
				return;
			}
			if (is_joypad_event) {
				if (!input->is_action_just_pressed("ui_left", true)) {
					return;
				}
				set_process_internal(true);
			}
			if (is_layout_rtl()) {
				set_value(get_value() + (custom_step >= 0 ? custom_step : get_step()));
			} else {
				set_value(get_value() - (custom_step >= 0 ? custom_step : get_step()));
			}
			accept_event();
		} else if (p_event->is_action_pressed("ui_right", true)) {
			if (orientation != HORIZONTAL) {
				return;
			}
			if (is_joypad_event) {
				if (!input->is_action_just_pressed("ui_right", true)) {
					return;
				}
				set_process_internal(true);
			}
			if (is_layout_rtl()) {
				set_value(get_value() - (custom_step >= 0 ? custom_step : get_step()));
			} else {
				set_value(get_value() + (custom_step >= 0 ? custom_step : get_step()));
			}
			accept_event();
		} else if (p_event->is_action_pressed("ui_up", true)) {
			if (orientation != VERTICAL) {
				return;
			}
			if (is_joypad_event) {
				if (!input->is_action_just_pressed("ui_up", true)) {
					return;
				}
				set_process_internal(true);
			}
			set_value(get_value() + (custom_step >= 0 ? custom_step : get_step()));
			accept_event();
		} else if (p_event->is_action_pressed("ui_down", true)) {
			if (orientation != VERTICAL) {
				return;
			}
			if (is_joypad_event) {
				if (!input->is_action_just_pressed("ui_down", true)) {
					return;
				}
				set_process_internal(true);
			}
			set_value(get_value() - (custom_step >= 0 ? custom_step : get_step()));
			accept_event();
		} else if (p_event->is_action("ui_home", true) && p_event->is_pressed()) {
			set_value(get_min());
			accept_event();
		} else if (p_event->is_action("ui_end", true) && p_event->is_pressed()) {
			set_value(get_max());
			accept_event();
		}
	}
}

void Slider::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_INTERNAL_PROCESS: {
			Input *input = Input::get_singleton();

			if (input->is_action_just_released("ui_left") || input->is_action_just_released("ui_right") || input->is_action_just_released("ui_up") || input->is_action_just_released("ui_down")) {
				gamepad_event_delay_ms = DEFAULT_GAMEPAD_EVENT_DELAY_MS;
				set_process_internal(false);
				return;
			}

			gamepad_event_delay_ms -= get_process_delta_time();
			if (gamepad_event_delay_ms <= 0) {
				gamepad_event_delay_ms = GAMEPAD_EVENT_REPEAT_RATE_MS + gamepad_event_delay_ms;
				if (orientation == HORIZONTAL) {
					if (input->is_action_pressed("ui_left")) {
						if (is_layout_rtl()) {
							set_value(get_value() + (custom_step >= 0 ? custom_step : get_step()));
						} else {
							set_value(get_value() - (custom_step >= 0 ? custom_step : get_step()));
						}
					}

					if (input->is_action_pressed("ui_right")) {
						if (is_layout_rtl()) {
							set_value(get_value() - (custom_step >= 0 ? custom_step : get_step()));
						} else {
							set_value(get_value() + (custom_step >= 0 ? custom_step : get_step()));
						}
					}
				} else if (orientation == VERTICAL) {
					if (input->is_action_pressed("ui_down")) {
						set_value(get_value() - (custom_step >= 0 ? custom_step : get_step()));
					}

					if (input->is_action_pressed("ui_up")) {
						set_value(get_value() + (custom_step >= 0 ? custom_step : get_step()));
					}
				}
			}

		} break;

		case NOTIFICATION_THEME_CHANGED: {
			update_minimum_size();
			queue_redraw();
		} break;

		case NOTIFICATION_MOUSE_ENTER: {
			mouse_inside = true;
			queue_redraw();
		} break;

		case NOTIFICATION_MOUSE_EXIT: {
			mouse_inside = false;
			queue_redraw();
		} break;

		case NOTIFICATION_VISIBILITY_CHANGED:
		case NOTIFICATION_EXIT_TREE: {
			mouse_inside = false;
			grab.active = false;
		} break;

		case NOTIFICATION_DRAW: {
			RID ci = get_canvas_item();
			Size2i size = get_size();
			double ratio = Math::is_nan(get_as_ratio()) ? 0 : get_as_ratio();

			Ref<StyleBox> style = theme_cache.slider_style;
			Ref<Texture2D> tick = theme_cache.tick_icon;

			bool highlighted = editable && (mouse_inside || has_focus());
			Ref<Texture2D> grabber;
			if (editable) {
				if (highlighted) {
					grabber = theme_cache.grabber_hl_icon;
				} else {
					grabber = theme_cache.grabber_icon;
				}
			} else {
				grabber = theme_cache.grabber_disabled_icon;
			}

			Ref<StyleBox> grabber_area;
			if (highlighted) {
				grabber_area = theme_cache.grabber_area_hl_style;
			} else {
				grabber_area = theme_cache.grabber_area_style;
			}

			if (orientation == VERTICAL) {
				int widget_width = style->get_minimum_size().width;
				double areasize = size.height - (theme_cache.center_grabber ? 0 : grabber->get_height());
				int grabber_shift = theme_cache.center_grabber ? grabber->get_height() / 2 : 0;
				style->draw(ci, Rect2i(Point2i(size.width / 2 - widget_width / 2, 0), Size2i(widget_width, size.height)));
				grabber_area->draw(ci, Rect2i(Point2i((size.width - widget_width) / 2, Math::round(size.height - areasize * ratio - grabber->get_height() / 2 + grabber_shift)), Size2i(widget_width, Math::round(areasize * ratio + grabber->get_height() / 2 - grabber_shift))));

				if (ticks > 1) {
					int grabber_offset = (grabber->get_height() / 2 - tick->get_height() / 2);
					for (int i = 0; i < ticks; i++) {
						if (!ticks_on_borders && (i == 0 || i + 1 == ticks)) {
							continue;
						}
						int ofs = (i * areasize / (ticks - 1)) + grabber_offset - grabber_shift;
						tick->draw(ci, Point2i((size.width - widget_width) / 2, ofs));
					}
				}
				grabber->draw(ci, Point2i(size.width / 2 - grabber->get_width() / 2 + theme_cache.grabber_offset, size.height - ratio * areasize - grabber->get_height() + grabber_shift));
			} else {
				int widget_height = style->get_minimum_size().height;
				double areasize = size.width - (theme_cache.center_grabber ? 0 : grabber->get_size().width);
				int grabber_shift = theme_cache.center_grabber ? -grabber->get_width() / 2 : 0;
				bool rtl = is_layout_rtl();

				style->draw(ci, Rect2i(Point2i(0, (size.height - widget_height) / 2), Size2i(size.width, widget_height)));
				int p = areasize * (rtl ? 1 - ratio : ratio) + grabber->get_width() / 2 + grabber_shift;
				if (rtl) {
					grabber_area->draw(ci, Rect2i(Point2i(p, (size.height - widget_height) / 2), Size2i(size.width - p, widget_height)));
				} else {
					grabber_area->draw(ci, Rect2i(Point2i(0, (size.height - widget_height) / 2), Size2i(p, widget_height)));
				}

				if (ticks > 1) {
					int grabber_offset = (grabber->get_width() / 2 - tick->get_width() / 2);
					for (int i = 0; i < ticks; i++) {
						if ((!ticks_on_borders) && ((i == 0) || ((i + 1) == ticks))) {
							continue;
						}
						int ofs = (i * areasize / (ticks - 1)) + grabber_offset + grabber_shift;
						tick->draw(ci, Point2i(ofs, (size.height - widget_height) / 2));
					}
				}
				grabber->draw(ci, Point2i((rtl ? 1 - ratio : ratio) * areasize + grabber_shift, size.height / 2 - grabber->get_height() / 2 + theme_cache.grabber_offset));
			}
		} break;
	}
}

void Slider::set_custom_step(double p_custom_step) {
	custom_step = p_custom_step;
}

double Slider::get_custom_step() const {
	return custom_step;
}

void Slider::set_ticks(int p_count) {
	if (ticks == p_count) {
		return;
	}

	ticks = p_count;
	queue_redraw();
}

int Slider::get_ticks() const {
	return ticks;
}

bool Slider::get_ticks_on_borders() const {
	return ticks_on_borders;
}

void Slider::set_ticks_on_borders(bool _tob) {
	if (ticks_on_borders == _tob) {
		return;
	}

	ticks_on_borders = _tob;
	queue_redraw();
}

void Slider::set_editable(bool p_editable) {
	if (editable == p_editable) {
		return;
	}
	grab.active = false;

	editable = p_editable;
	queue_redraw();
}

bool Slider::is_editable() const {
	return editable;
}

void Slider::set_scrollable(bool p_scrollable) {
	scrollable = p_scrollable;
}

bool Slider::is_scrollable() const {
	return scrollable;
}

void Slider::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_ticks", "count"), &Slider::set_ticks);
	ClassDB::bind_method(D_METHOD("get_ticks"), &Slider::get_ticks);

	ClassDB::bind_method(D_METHOD("get_ticks_on_borders"), &Slider::get_ticks_on_borders);
	ClassDB::bind_method(D_METHOD("set_ticks_on_borders", "ticks_on_border"), &Slider::set_ticks_on_borders);

	ClassDB::bind_method(D_METHOD("set_editable", "editable"), &Slider::set_editable);
	ClassDB::bind_method(D_METHOD("is_editable"), &Slider::is_editable);
	ClassDB::bind_method(D_METHOD("set_scrollable", "scrollable"), &Slider::set_scrollable);
	ClassDB::bind_method(D_METHOD("is_scrollable"), &Slider::is_scrollable);

	ADD_SIGNAL(MethodInfo("drag_started"));
	ADD_SIGNAL(MethodInfo("drag_ended", PropertyInfo(Variant::BOOL, "value_changed")));

	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "editable"), "set_editable", "is_editable");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "scrollable"), "set_scrollable", "is_scrollable");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "tick_count", PROPERTY_HINT_RANGE, "0,4096,1"), "set_ticks", "get_ticks");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "ticks_on_borders"), "set_ticks_on_borders", "get_ticks_on_borders");

	BIND_THEME_ITEM_CUSTOM(Theme::DATA_TYPE_STYLEBOX, Slider, slider_style, "slider");
	BIND_THEME_ITEM_CUSTOM(Theme::DATA_TYPE_STYLEBOX, Slider, grabber_area_style, "grabber_area");
	BIND_THEME_ITEM_CUSTOM(Theme::DATA_TYPE_STYLEBOX, Slider, grabber_area_hl_style, "grabber_area_highlight");

	BIND_THEME_ITEM_CUSTOM(Theme::DATA_TYPE_ICON, Slider, grabber_icon, "grabber");
	BIND_THEME_ITEM_CUSTOM(Theme::DATA_TYPE_ICON, Slider, grabber_hl_icon, "grabber_highlight");
	BIND_THEME_ITEM_CUSTOM(Theme::DATA_TYPE_ICON, Slider, grabber_disabled_icon, "grabber_disabled");
	BIND_THEME_ITEM_CUSTOM(Theme::DATA_TYPE_ICON, Slider, tick_icon, "tick");

	BIND_THEME_ITEM(Theme::DATA_TYPE_CONSTANT, Slider, center_grabber);
	BIND_THEME_ITEM(Theme::DATA_TYPE_CONSTANT, Slider, grabber_offset);
}

Slider::Slider(Orientation p_orientation) {
	orientation = p_orientation;
	set_focus_mode(FOCUS_ALL);
}

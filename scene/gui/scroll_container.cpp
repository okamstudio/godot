/**************************************************************************/
/*  scroll_container.cpp                                                  */
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

#include "scroll_container.h"

#include "core/config/project_settings.h"
#include "scene/main/window.h"
#include "scene/theme/theme_db.h"

Size2 ScrollContainer::get_minimum_size() const {
	// Calculated in this function, as it needs to traverse all child controls once to calculate;
	// and needs to be calculated before being used by update_scrollbars().
	largest_child_min_size = Size2();

	for (int i = 0; i < get_child_count(); i++) {
		Control *c = as_sortable_control(get_child(i), SortableVisibilityMode::VISIBLE);
		if (!c) {
			continue;
		}
		if (c == h_scroll || c == v_scroll) {
			continue;
		}

		Size2 child_min_size = c->get_combined_minimum_size();

		largest_child_min_size = largest_child_min_size.max(child_min_size);
	}

	Size2 min_size;
	const Size2 size = get_size();

	if (horizontal_scroll_mode == SCROLL_MODE_DISABLED) {
		min_size.x = largest_child_min_size.x;
		bool v_scroll_show = vertical_scroll_mode == SCROLL_MODE_SHOW_ALWAYS || vertical_scroll_mode == SCROLL_MODE_RESERVE || (vertical_scroll_mode == SCROLL_MODE_AUTO && largest_child_min_size.y > size.y);
		if (v_scroll_show && v_scroll->get_parent() == this) {
			min_size.x += v_scroll->get_minimum_size().x;
		}
	}

	if (vertical_scroll_mode == SCROLL_MODE_DISABLED) {
		min_size.y = largest_child_min_size.y;
		bool h_scroll_show = horizontal_scroll_mode == SCROLL_MODE_SHOW_ALWAYS || horizontal_scroll_mode == SCROLL_MODE_RESERVE || (horizontal_scroll_mode == SCROLL_MODE_AUTO && largest_child_min_size.x > size.x);
		if (h_scroll_show && h_scroll->get_parent() == this) {
			min_size.y += h_scroll->get_minimum_size().y;
		}
	}

	min_size += theme_cache.panel_style->get_minimum_size();
	return min_size;
}

void ScrollContainer::_cancel_drag() {
	set_physics_process_internal(false);
	drag_touching_deaccel = false;
	drag_touching = false;
	drag_speed = Vector2();
	drag_accum = Vector2();
	last_drag_accum = Vector2();
	drag_from = Vector2();

	if (beyond_deadzone) {
		emit_signal(SNAME("scroll_ended"));
		propagate_notification(NOTIFICATION_SCROLL_END);
		beyond_deadzone = false;
	}
}

bool ScrollContainer::_is_h_scroll_visible() const {
	// Scrolls may have been moved out for reasons.
	return h_scroll->is_visible() && h_scroll->get_parent() == this;
}

bool ScrollContainer::_is_v_scroll_visible() const {
	return v_scroll->is_visible() && v_scroll->get_parent() == this;
}

void ScrollContainer::gui_input(const Ref<InputEvent> &p_gui_input) {
	ERR_FAIL_COND(p_gui_input.is_null());

	double prev_v_scroll = v_scroll->get_value();
	double prev_h_scroll = h_scroll->get_value();
	bool h_scroll_enabled = horizontal_scroll_mode != SCROLL_MODE_DISABLED;
	bool v_scroll_enabled = vertical_scroll_mode != SCROLL_MODE_DISABLED;

	Ref<InputEventMouseButton> mb = p_gui_input;

	if (mb.is_valid()) {
		if (mb->is_pressed()) {
			bool scroll_value_modified = false;

			bool v_scroll_hidden = !v_scroll->is_visible() && vertical_scroll_mode != SCROLL_MODE_SHOW_NEVER;
			if (mb->get_button_index() == MouseButton::WHEEL_UP) {
				// By default, the vertical orientation takes precedence. This is an exception.
				if ((h_scroll_enabled && mb->is_shift_pressed()) || v_scroll_hidden) {
					h_scroll->scroll(-h_scroll->get_page() / 8 * mb->get_factor());
					scroll_value_modified = true;
				} else if (v_scroll_enabled) {
					v_scroll->scroll(-v_scroll->get_page() / 8 * mb->get_factor());
					scroll_value_modified = true;
				}
			}
			if (mb->get_button_index() == MouseButton::WHEEL_DOWN) {
				if ((h_scroll_enabled && mb->is_shift_pressed()) || v_scroll_hidden) {
					h_scroll->scroll(h_scroll->get_page() / 8 * mb->get_factor());
					scroll_value_modified = true;
				} else if (v_scroll_enabled) {
					v_scroll->scroll(v_scroll->get_page() / 8 * mb->get_factor());
					scroll_value_modified = true;
				}
			}

			bool h_scroll_hidden = !h_scroll->is_visible() && horizontal_scroll_mode != SCROLL_MODE_SHOW_NEVER;
			if (mb->get_button_index() == MouseButton::WHEEL_LEFT) {
				// By default, the horizontal orientation takes precedence. This is an exception.
				if ((v_scroll_enabled && mb->is_shift_pressed()) || h_scroll_hidden) {
					v_scroll->scroll(-v_scroll->get_page() / 8 * mb->get_factor());
					scroll_value_modified = true;
				} else if (h_scroll_enabled) {
					h_scroll->scroll(-h_scroll->get_page() / 8 * mb->get_factor());
					scroll_value_modified = true;
				}
			}
			if (mb->get_button_index() == MouseButton::WHEEL_RIGHT) {
				if ((v_scroll_enabled && mb->is_shift_pressed()) || h_scroll_hidden) {
					v_scroll->scroll(v_scroll->get_page() / 8 * mb->get_factor());
					scroll_value_modified = true;
				} else if (h_scroll_enabled) {
					h_scroll->scroll(h_scroll->get_page() / 8 * mb->get_factor());
					scroll_value_modified = true;
				}
			}

			if (scroll_value_modified && (v_scroll->get_value() != prev_v_scroll || h_scroll->get_value() != prev_h_scroll)) {
				accept_event(); // Accept event if scroll changed.
				return;
			}
		}

		bool is_touchscreen_available = DisplayServer::get_singleton()->is_touchscreen_available();
		if (!is_touchscreen_available) {
			return;
		}

		if (mb->get_button_index() != MouseButton::LEFT) {
			return;
		}

		if (mb->is_pressed()) {
			if (drag_touching) {
				_cancel_drag();
			}

			drag_speed = Vector2();
			drag_accum = Vector2();
			last_drag_accum = Vector2();
			drag_from = Vector2(prev_h_scroll, prev_v_scroll);
			drag_touching = true;
			drag_touching_deaccel = false;
			beyond_deadzone = false;
			time_since_motion = 0;
			set_physics_process_internal(true);
			time_since_motion = 0;

		} else {
			if (drag_touching) {
				if (drag_speed == Vector2()) {
					_cancel_drag();
				} else {
					drag_touching_deaccel = true;
				}
			}
		}
		return;
	}

	Ref<InputEventMouseMotion> mm = p_gui_input;

	if (mm.is_valid()) {
		if (drag_touching && !drag_touching_deaccel) {
			Vector2 motion = mm->get_relative();
			drag_accum -= motion;

			if (beyond_deadzone || (h_scroll_enabled && Math::abs(drag_accum.x) > deadzone) || (v_scroll_enabled && Math::abs(drag_accum.y) > deadzone)) {
				if (!beyond_deadzone) {
					propagate_notification(NOTIFICATION_SCROLL_BEGIN);
					emit_signal(SNAME("scroll_started"));

					beyond_deadzone = true;
					// Resetting drag_accum here ensures smooth scrolling after reaching deadzone.
					drag_accum = -motion;
				}
				Vector2 diff = drag_from + drag_accum;
				if (h_scroll_enabled) {
					h_scroll->scroll_to(diff.x);
				} else {
					drag_accum.x = 0;
				}
				if (v_scroll_enabled) {
					v_scroll->scroll_to(diff.y);
				} else {
					drag_accum.y = 0;
				}
				time_since_motion = 0;
			}
		}

		if (v_scroll->get_value() != prev_v_scroll || h_scroll->get_value() != prev_h_scroll) {
			accept_event(); // Accept event if scroll changed.
		}
		return;
	}

	Ref<InputEventPanGesture> pan_gesture = p_gui_input;
	if (pan_gesture.is_valid()) {
		if (h_scroll_enabled) {
			h_scroll->scroll(h_scroll->get_page() * pan_gesture->get_delta().x / 8);
		}
		if (v_scroll_enabled) {
			v_scroll->scroll(v_scroll->get_page() * pan_gesture->get_delta().y / 8);
		}

		if (v_scroll->get_value() != prev_v_scroll || h_scroll->get_value() != prev_h_scroll) {
			accept_event(); // Accept event if scroll changed.
		}
		return;
	}
}

void ScrollContainer::_update_scrollbar_position() {
	if (!_updating_scrollbars) {
		return;
	}

	Size2 hmin = h_scroll->is_visible() ? h_scroll->get_combined_minimum_size() : Size2();
	Size2 vmin = v_scroll->is_visible() ? v_scroll->get_combined_minimum_size() : Size2();

	int lmar = is_layout_rtl() ? theme_cache.panel_style->get_margin(SIDE_RIGHT) : theme_cache.panel_style->get_margin(SIDE_LEFT);
	int rmar = is_layout_rtl() ? theme_cache.panel_style->get_margin(SIDE_LEFT) : theme_cache.panel_style->get_margin(SIDE_RIGHT);

	h_scroll->set_anchor_and_offset(SIDE_LEFT, ANCHOR_BEGIN, lmar);
	h_scroll->set_anchor_and_offset(SIDE_RIGHT, ANCHOR_END, -rmar - vmin.width);
	h_scroll->set_anchor_and_offset(SIDE_TOP, ANCHOR_END, -hmin.height - theme_cache.panel_style->get_margin(SIDE_BOTTOM));
	h_scroll->set_anchor_and_offset(SIDE_BOTTOM, ANCHOR_END, -theme_cache.panel_style->get_margin(SIDE_BOTTOM));

	v_scroll->set_anchor_and_offset(SIDE_LEFT, ANCHOR_END, -vmin.width - rmar);
	v_scroll->set_anchor_and_offset(SIDE_RIGHT, ANCHOR_END, -rmar);
	v_scroll->set_anchor_and_offset(SIDE_TOP, ANCHOR_BEGIN, theme_cache.panel_style->get_margin(SIDE_TOP));
	v_scroll->set_anchor_and_offset(SIDE_BOTTOM, ANCHOR_END, -hmin.height - theme_cache.panel_style->get_margin(SIDE_BOTTOM));

	_updating_scrollbars = false;
}

void ScrollContainer::_gui_focus_changed(Control *p_control) {
	if (follow_focus && is_ancestor_of(p_control)) {
		ensure_control_visible(p_control);
	}
	if (draw_focus_border) {
		const bool _should_draw_focus_border = has_focus() || child_has_focus();
		if (focus_border_is_drawn != _should_draw_focus_border) {
			queue_redraw();
		}
	}
}

void ScrollContainer::ensure_control_visible(Control *p_control) {
	ERR_FAIL_COND_MSG(!is_ancestor_of(p_control), "Must be an ancestor of the control.");

	// Just eliminate the rotation of this ScrollContainer.
	Transform2D other_in_this = get_global_transform().affine_inverse() * p_control->get_global_transform();

	Size2 size = get_size();
	Rect2 other_rect = other_in_this.xform(Rect2(Point2(), p_control->get_size()));

	float side_margin = v_scroll->is_visible() ? v_scroll->get_size().x : 0.0f;
	float bottom_margin = h_scroll->is_visible() ? h_scroll->get_size().y : 0.0f;

	Vector2 diff = Vector2(MAX(MIN(other_rect.position.x - (is_layout_rtl() ? side_margin : 0.0f), 0.0f), other_rect.position.x + other_rect.size.x - size.x + (!is_layout_rtl() ? side_margin : 0.0f)),
			MAX(MIN(other_rect.position.y, 0.0f), other_rect.position.y + other_rect.size.y - size.y + bottom_margin));

	set_h_scroll(get_h_scroll() + diff.x);
	set_v_scroll(get_v_scroll() + diff.y);
}

void ScrollContainer::_reposition_children() {
	update_scrollbars();
	Size2 size = get_size();
	Point2 ofs;

	size -= theme_cache.panel_style->get_minimum_size();
	ofs += theme_cache.panel_style->get_offset();
	bool rtl = is_layout_rtl();

	if (_is_h_scroll_visible() || horizontal_scroll_mode == SCROLL_MODE_RESERVE) {
		size.y -= h_scroll->get_minimum_size().y;
	}

	if (_is_v_scroll_visible() || vertical_scroll_mode == SCROLL_MODE_RESERVE) {
		size.x -= v_scroll->get_minimum_size().x;
	}

	for (int i = 0; i < get_child_count(); i++) {
		Control *c = as_sortable_control(get_child(i));
		if (!c) {
			continue;
		}
		if (c == h_scroll || c == v_scroll) {
			continue;
		}
		Size2 minsize = c->get_combined_minimum_size();

		Rect2 r = Rect2(-Size2(get_h_scroll(), get_v_scroll()), minsize);
		if (c->get_h_size_flags().has_flag(SIZE_EXPAND)) {
			r.size.width = MAX(size.width, minsize.width);
		}
		if (c->get_v_size_flags().has_flag(SIZE_EXPAND)) {
			r.size.height = MAX(size.height, minsize.height);
		}
		r.position += ofs;
		if (rtl && _is_v_scroll_visible()) {
			r.position.x += v_scroll->get_minimum_size().x;
		}
		r.position = r.position.floor();
		fit_child_in_rect(c, r);
	}

	queue_redraw();
}

void ScrollContainer::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE:
		case NOTIFICATION_THEME_CHANGED:
		case NOTIFICATION_LAYOUT_DIRECTION_CHANGED:
		case NOTIFICATION_TRANSLATION_CHANGED: {
			_updating_scrollbars = true;
			callable_mp(this, is_ready() ? &ScrollContainer::_reposition_children : &ScrollContainer::_update_scrollbar_position).call_deferred();
		} break;

		case NOTIFICATION_READY: {
			Viewport *viewport = get_viewport();
			ERR_FAIL_NULL(viewport);
			viewport->connect("gui_focus_changed", callable_mp(this, &ScrollContainer::_gui_focus_changed));
			_reposition_children();
		} break;

		case NOTIFICATION_SORT_CHILDREN: {
			_reposition_children();
		} break;

		case NOTIFICATION_DRAW: {
			draw_style_box(theme_cache.panel_style, Rect2(Vector2(), get_size()));
			if (draw_focus_border && (has_focus() || child_has_focus())) {
				RID ci = get_canvas_item();
				RenderingServer::get_singleton()->canvas_item_add_clip_ignore(ci, true);
				draw_style_box(theme_cache.focus_style, Rect2(Point2(), get_size()));
				RenderingServer::get_singleton()->canvas_item_add_clip_ignore(ci, false);
				focus_border_is_drawn = true;
			} else {
				focus_border_is_drawn = false;
			}
		} break;

		case NOTIFICATION_INTERNAL_PHYSICS_PROCESS: {
			if (drag_touching) {
				if (drag_touching_deaccel) {
					Vector2 pos = Vector2(h_scroll->get_value(), v_scroll->get_value());
					pos += drag_speed * get_physics_process_delta_time();

					bool turnoff_h = false;
					bool turnoff_v = false;

					if (pos.x < 0) {
						pos.x = 0;
						turnoff_h = true;
					}
					if (pos.x > (h_scroll->get_max() - h_scroll->get_page())) {
						pos.x = h_scroll->get_max() - h_scroll->get_page();
						turnoff_h = true;
					}

					if (pos.y < 0) {
						pos.y = 0;
						turnoff_v = true;
					}
					if (pos.y > (v_scroll->get_max() - v_scroll->get_page())) {
						pos.y = v_scroll->get_max() - v_scroll->get_page();
						turnoff_v = true;
					}

					if (horizontal_scroll_mode != SCROLL_MODE_DISABLED) {
						h_scroll->scroll_to(pos.x);
					}
					if (vertical_scroll_mode != SCROLL_MODE_DISABLED) {
						v_scroll->scroll_to(pos.y);
					}

					float sgn_x = drag_speed.x < 0 ? -1 : 1;
					float val_x = Math::abs(drag_speed.x);
					val_x -= 1000 * get_physics_process_delta_time();

					if (val_x < 0) {
						turnoff_h = true;
					}

					float sgn_y = drag_speed.y < 0 ? -1 : 1;
					float val_y = Math::abs(drag_speed.y);
					val_y -= 1000 * get_physics_process_delta_time();

					if (val_y < 0) {
						turnoff_v = true;
					}

					drag_speed = Vector2(sgn_x * val_x, sgn_y * val_y);

					if (turnoff_h && turnoff_v) {
						_cancel_drag();
					}

				} else {
					if (time_since_motion == 0 || time_since_motion > 0.1) {
						Vector2 diff = drag_accum - last_drag_accum;
						last_drag_accum = drag_accum;
						drag_speed = diff / get_physics_process_delta_time();
					}

					time_since_motion += get_physics_process_delta_time();
				}
			}
		} break;
	}
}

void ScrollContainer::update_scrollbars() {
	Size2 size = get_size();
	size -= theme_cache.panel_style->get_minimum_size();

	Size2 hmin = h_scroll->get_combined_minimum_size();
	Size2 vmin = v_scroll->get_combined_minimum_size();

	h_scroll->set_visible(horizontal_scroll_mode == SCROLL_MODE_SHOW_ALWAYS || ((horizontal_scroll_mode == SCROLL_MODE_AUTO || horizontal_scroll_mode == SCROLL_MODE_RESERVE) && largest_child_min_size.width > size.width));
	v_scroll->set_visible(vertical_scroll_mode == SCROLL_MODE_SHOW_ALWAYS || ((vertical_scroll_mode == SCROLL_MODE_AUTO || vertical_scroll_mode == SCROLL_MODE_RESERVE) && largest_child_min_size.height > size.height));

	h_scroll->set_max(largest_child_min_size.width);
	h_scroll->set_page(_is_v_scroll_visible() ? size.width - vmin.width : size.width);

	v_scroll->set_max(largest_child_min_size.height);
	v_scroll->set_page(_is_h_scroll_visible() ? size.height - hmin.height : size.height);

	// Avoid scrollbar overlapping.
	_updating_scrollbars = true;
	callable_mp(this, &ScrollContainer::_update_scrollbar_position).call_deferred();
}

void ScrollContainer::_scroll_moved(float) {
	queue_sort();
}

void ScrollContainer::set_h_scroll(int p_pos) {
	h_scroll->set_value(p_pos);
	_cancel_drag();
}

int ScrollContainer::get_h_scroll() const {
	return h_scroll->get_value();
}

void ScrollContainer::set_v_scroll(int p_pos) {
	v_scroll->set_value(p_pos);
	_cancel_drag();
}

int ScrollContainer::get_v_scroll() const {
	return v_scroll->get_value();
}

void ScrollContainer::set_horizontal_custom_step(float p_custom_step) {
	h_scroll->set_custom_step(p_custom_step);
}

float ScrollContainer::get_horizontal_custom_step() const {
	return h_scroll->get_custom_step();
}

void ScrollContainer::set_vertical_custom_step(float p_custom_step) {
	v_scroll->set_custom_step(p_custom_step);
}

float ScrollContainer::get_vertical_custom_step() const {
	return v_scroll->get_custom_step();
}

void ScrollContainer::set_horizontal_scroll_mode(ScrollMode p_mode) {
	if (horizontal_scroll_mode == p_mode) {
		return;
	}

	horizontal_scroll_mode = p_mode;
	update_minimum_size();
	queue_sort();
}

ScrollContainer::ScrollMode ScrollContainer::get_horizontal_scroll_mode() const {
	return horizontal_scroll_mode;
}

void ScrollContainer::set_vertical_scroll_mode(ScrollMode p_mode) {
	if (vertical_scroll_mode == p_mode) {
		return;
	}

	vertical_scroll_mode = p_mode;
	update_minimum_size();
	queue_sort();
}

ScrollContainer::ScrollMode ScrollContainer::get_vertical_scroll_mode() const {
	return vertical_scroll_mode;
}

int ScrollContainer::get_deadzone() const {
	return deadzone;
}

void ScrollContainer::set_deadzone(int p_deadzone) {
	deadzone = p_deadzone;
}

bool ScrollContainer::is_following_focus() const {
	return follow_focus;
}

void ScrollContainer::set_follow_focus(bool p_follow) {
	follow_focus = p_follow;
}

PackedStringArray ScrollContainer::get_configuration_warnings() const {
	PackedStringArray warnings = Container::get_configuration_warnings();

	int found = 0;

	for (int i = 0; i < get_child_count(); i++) {
		Control *c = as_sortable_control(get_child(i), SortableVisibilityMode::VISIBLE);
		if (!c) {
			continue;
		}
		if (c == h_scroll || c == v_scroll) {
			continue;
		}

		found++;
	}

	if (found != 1) {
		warnings.push_back(RTR("ScrollContainer is intended to work with a single child control.\nUse a container as child (VBox, HBox, etc.), or a Control and set the custom minimum size manually."));
	}

	return warnings;
}

HScrollBar *ScrollContainer::get_h_scroll_bar() {
	return h_scroll;
}

VScrollBar *ScrollContainer::get_v_scroll_bar() {
	return v_scroll;
}

void ScrollContainer::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_h_scroll", "value"), &ScrollContainer::set_h_scroll);
	ClassDB::bind_method(D_METHOD("get_h_scroll"), &ScrollContainer::get_h_scroll);

	ClassDB::bind_method(D_METHOD("set_v_scroll", "value"), &ScrollContainer::set_v_scroll);
	ClassDB::bind_method(D_METHOD("get_v_scroll"), &ScrollContainer::get_v_scroll);

	ClassDB::bind_method(D_METHOD("set_horizontal_custom_step", "value"), &ScrollContainer::set_horizontal_custom_step);
	ClassDB::bind_method(D_METHOD("get_horizontal_custom_step"), &ScrollContainer::get_horizontal_custom_step);

	ClassDB::bind_method(D_METHOD("set_vertical_custom_step", "value"), &ScrollContainer::set_vertical_custom_step);
	ClassDB::bind_method(D_METHOD("get_vertical_custom_step"), &ScrollContainer::get_vertical_custom_step);

	ClassDB::bind_method(D_METHOD("set_horizontal_scroll_mode", "enable"), &ScrollContainer::set_horizontal_scroll_mode);
	ClassDB::bind_method(D_METHOD("get_horizontal_scroll_mode"), &ScrollContainer::get_horizontal_scroll_mode);

	ClassDB::bind_method(D_METHOD("set_vertical_scroll_mode", "enable"), &ScrollContainer::set_vertical_scroll_mode);
	ClassDB::bind_method(D_METHOD("get_vertical_scroll_mode"), &ScrollContainer::get_vertical_scroll_mode);

	ClassDB::bind_method(D_METHOD("set_deadzone", "deadzone"), &ScrollContainer::set_deadzone);
	ClassDB::bind_method(D_METHOD("get_deadzone"), &ScrollContainer::get_deadzone);

	ClassDB::bind_method(D_METHOD("set_follow_focus", "enabled"), &ScrollContainer::set_follow_focus);
	ClassDB::bind_method(D_METHOD("is_following_focus"), &ScrollContainer::is_following_focus);

	ClassDB::bind_method(D_METHOD("get_h_scroll_bar"), &ScrollContainer::get_h_scroll_bar);
	ClassDB::bind_method(D_METHOD("get_v_scroll_bar"), &ScrollContainer::get_v_scroll_bar);
	ClassDB::bind_method(D_METHOD("ensure_control_visible", "control"), &ScrollContainer::ensure_control_visible);

	ClassDB::bind_method(D_METHOD("set_draw_focus_border", "draw"), &ScrollContainer::set_draw_focus_border);
	ClassDB::bind_method(D_METHOD("get_draw_focus_border"), &ScrollContainer::get_draw_focus_border);

	ADD_SIGNAL(MethodInfo("scroll_started"));
	ADD_SIGNAL(MethodInfo("scroll_ended"));

	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "follow_focus"), "set_follow_focus", "is_following_focus");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "draw_focus_border"), "set_draw_focus_border", "get_draw_focus_border");

	ADD_GROUP("Scroll", "scroll_");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "scroll_horizontal", PROPERTY_HINT_NONE, "suffix:px"), "set_h_scroll", "get_h_scroll");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "scroll_vertical", PROPERTY_HINT_NONE, "suffix:px"), "set_v_scroll", "get_v_scroll");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "scroll_horizontal_custom_step", PROPERTY_HINT_RANGE, "-1,4096,suffix:px"), "set_horizontal_custom_step", "get_horizontal_custom_step");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "scroll_vertical_custom_step", PROPERTY_HINT_RANGE, "-1,4096,suffix:px"), "set_vertical_custom_step", "get_vertical_custom_step");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "horizontal_scroll_mode", PROPERTY_HINT_ENUM, "Disabled,Auto,Always Show,Never Show,Reserve"), "set_horizontal_scroll_mode", "get_horizontal_scroll_mode");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "vertical_scroll_mode", PROPERTY_HINT_ENUM, "Disabled,Auto,Always Show,Never Show,Reserve"), "set_vertical_scroll_mode", "get_vertical_scroll_mode");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "scroll_deadzone"), "set_deadzone", "get_deadzone");

	BIND_ENUM_CONSTANT(SCROLL_MODE_DISABLED);
	BIND_ENUM_CONSTANT(SCROLL_MODE_AUTO);
	BIND_ENUM_CONSTANT(SCROLL_MODE_SHOW_ALWAYS);
	BIND_ENUM_CONSTANT(SCROLL_MODE_SHOW_NEVER);
	BIND_ENUM_CONSTANT(SCROLL_MODE_RESERVE);

	BIND_THEME_ITEM_CUSTOM(Theme::DATA_TYPE_STYLEBOX, ScrollContainer, panel_style, "panel");
	BIND_THEME_ITEM_CUSTOM(Theme::DATA_TYPE_STYLEBOX, ScrollContainer, focus_style, "focus");

	GLOBAL_DEF("gui/common/default_scroll_deadzone", 0);
}

void ScrollContainer::set_draw_focus_border(bool p_draw) {
	if (draw_focus_border == p_draw) {
		return;
	}
	draw_focus_border = p_draw;
	queue_redraw();
}

bool ScrollContainer::get_draw_focus_border() {
	return draw_focus_border;
}

bool ScrollContainer::child_has_focus() {
	const Control *focus_owner = get_viewport() ? get_viewport()->gui_get_focus_owner() : nullptr;
	return focus_owner && is_ancestor_of(focus_owner);
}

ScrollContainer::ScrollContainer() {
	h_scroll = memnew(HScrollBar);
	h_scroll->set_name("_h_scroll");
	add_child(h_scroll, false, INTERNAL_MODE_BACK);
	h_scroll->connect(SceneStringName(value_changed), callable_mp(this, &ScrollContainer::_scroll_moved));

	v_scroll = memnew(VScrollBar);
	v_scroll->set_name("_v_scroll");
	add_child(v_scroll, false, INTERNAL_MODE_BACK);
	v_scroll->connect(SceneStringName(value_changed), callable_mp(this, &ScrollContainer::_scroll_moved));

	deadzone = GLOBAL_GET("gui/common/default_scroll_deadzone");

	set_clip_contents(true);
}

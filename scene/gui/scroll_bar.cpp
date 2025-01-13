/**************************************************************************/
/*  scroll_bar.cpp                                                        */
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

#include "scroll_bar.h"

#include "core/os/keyboard.h"
#include "core/os/os.h"
#include "core/string/print_string.h"
#include "scene/main/window.h"
#include "scene/theme/theme_db.h"

bool ScrollBar::focus_by_default = false;

void ScrollBar::set_can_focus_by_default(bool p_can_focus) {
	focus_by_default = p_can_focus;
}

void ScrollBar::gui_input(const Ref<InputEvent> &p_event) {
	ERR_FAIL_COND(p_event.is_null());

	Ref<InputEventMouseMotion> m = p_event;

	Ref<InputEventMouseButton> b = p_event;

	if (b.is_valid()) {
		accept_event();

		if (b->get_button_index() == MouseButton::WHEEL_DOWN && b->is_pressed()) {
			double change = get_page() != 0.0 ? get_page() / 4.0 : (get_max() - get_min()) / 16.0;
			scroll(MAX(change, get_step()));
			accept_event();
		}

		if (b->get_button_index() == MouseButton::WHEEL_UP && b->is_pressed()) {
			double change = get_page() != 0.0 ? get_page() / 4.0 : (get_max() - get_min()) / 16.0;
			scroll(-MAX(change, get_step()));
			accept_event();
		}

		if (b->get_button_index() != MouseButton::LEFT) {
			return;
		}

		if (b->is_pressed()) {
			double ofs = orientation == VERTICAL ? b->get_position().y : b->get_position().x;
			Ref<Texture2D> decr = theme_cache.decrement_icon;
			Ref<Texture2D> incr = theme_cache.increment_icon;

			double decr_size = orientation == VERTICAL ? decr->get_height() : decr->get_width();
			double incr_size = orientation == VERTICAL ? incr->get_height() : incr->get_width();
			double grabber_ofs = get_grabber_offset();
			double grabber_size = get_grabber_size();
			double total = orientation == VERTICAL ? get_size().height : get_size().width;

			if (ofs < decr_size) {
				decr_active = true;
				scroll(-(custom_step >= 0 ? custom_step : get_step()));
				queue_redraw();
				return;
			}

			if (ofs > total - incr_size) {
				incr_active = true;
				scroll(custom_step >= 0 ? custom_step : get_step());
				queue_redraw();
				return;
			}

			ofs -= decr_size + theme_cache.scroll_style->get_margin(orientation == VERTICAL ? SIDE_TOP : SIDE_LEFT);

			if (ofs < grabber_ofs) {
				if (scrolling) {
					target_scroll = CLAMP(target_scroll - get_page(), get_min(), get_max() - get_page());
				} else {
					double change = get_page() != 0.0 ? get_page() : (get_max() - get_min()) / 16.0;
					target_scroll = CLAMP(get_value() - change, get_min(), get_max() - get_page());
				}

				if (smooth_scroll_enabled) {
					scrolling = true;
					set_physics_process_internal(true);
				} else {
					scroll_to(target_scroll);
				}
				return;
			}

			ofs -= grabber_ofs;

			if (ofs < grabber_size) {
				drag.active = true;
				drag.pos_at_click = grabber_ofs + ofs;
				drag.value_at_click = get_as_ratio();
				queue_redraw();
			} else {
				if (scrolling) {
					target_scroll = CLAMP(target_scroll + get_page(), get_min(), get_max() - get_page());
				} else {
					double change = get_page() != 0.0 ? get_page() : (get_max() - get_min()) / 16.0;
					target_scroll = CLAMP(get_value() + change, get_min(), get_max() - get_page());
				}

				if (smooth_scroll_enabled) {
					scrolling = true;
					set_physics_process_internal(true);
				} else {
					scroll_to(target_scroll);
				}
			}

		} else {
			incr_active = false;
			decr_active = false;
			drag.active = false;
			queue_redraw();
		}
	}

	if (m.is_valid()) {
		accept_event();

		if (drag.active) {
			double ofs = orientation == VERTICAL ? m->get_position().y : m->get_position().x;
			Ref<Texture2D> decr = theme_cache.decrement_icon;

			double decr_size = orientation == VERTICAL ? decr->get_height() : decr->get_width();
			ofs -= decr_size + theme_cache.scroll_style->get_margin(orientation == VERTICAL ? SIDE_TOP : SIDE_LEFT);

			double diff = (ofs - drag.pos_at_click) / get_area_size();

			double prev_scroll = get_value();

			set_as_ratio(drag.value_at_click + diff);

			if (!Math::is_equal_approx(prev_scroll, get_value())) {
				emit_signal(SNAME("scrolling"));
			}
		} else {
			double ofs = orientation == VERTICAL ? m->get_position().y : m->get_position().x;
			Ref<Texture2D> decr = theme_cache.decrement_icon;
			Ref<Texture2D> incr = theme_cache.increment_icon;

			double decr_size = orientation == VERTICAL ? decr->get_height() : decr->get_width();
			double incr_size = orientation == VERTICAL ? incr->get_height() : incr->get_width();
			double total = orientation == VERTICAL ? get_size().height : get_size().width;

			HighlightStatus new_hilite;

			if (ofs < decr_size) {
				new_hilite = HIGHLIGHT_DECR;

			} else if (ofs > total - incr_size) {
				new_hilite = HIGHLIGHT_INCR;

			} else {
				new_hilite = HIGHLIGHT_RANGE;
			}

			if (new_hilite != highlight) {
				highlight = new_hilite;
				queue_redraw();
			}
		}
	}

	if (p_event->is_pressed()) {
		if (p_event->is_action("ui_left", true)) {
			if (orientation != HORIZONTAL) {
				return;
			}
			scroll(-(custom_step >= 0 ? custom_step : get_step()));

		} else if (p_event->is_action("ui_right", true)) {
			if (orientation != HORIZONTAL) {
				return;
			}
			scroll(custom_step >= 0 ? custom_step : get_step());

		} else if (p_event->is_action("ui_up", true)) {
			if (orientation != VERTICAL) {
				return;
			}

			scroll(-(custom_step >= 0 ? custom_step : get_step()));

		} else if (p_event->is_action("ui_down", true)) {
			if (orientation != VERTICAL) {
				return;
			}
			scroll(custom_step >= 0 ? custom_step : get_step());

		} else if (p_event->is_action("ui_home", true)) {
			scroll_to(get_min());

		} else if (p_event->is_action("ui_end", true)) {
			scroll_to(get_max());
		}
	}
}

void ScrollBar::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_DRAW: {
			RID ci = get_canvas_item();

			Ref<Texture2D> decr, incr;

			if (decr_active) {
				decr = theme_cache.decrement_pressed_icon;
			} else if (highlight == HIGHLIGHT_DECR) {
				decr = theme_cache.decrement_hl_icon;
			} else {
				decr = theme_cache.decrement_icon;
			}

			if (incr_active) {
				incr = theme_cache.increment_pressed_icon;
			} else if (highlight == HIGHLIGHT_INCR) {
				incr = theme_cache.increment_hl_icon;
			} else {
				incr = theme_cache.increment_icon;
			}

			Ref<StyleBox> grabber;
			if (drag.active) {
				grabber = theme_cache.grabber_pressed_style;
			} else if (highlight == HIGHLIGHT_RANGE) {
				grabber = theme_cache.grabber_hl_style;
			} else {
				grabber = theme_cache.grabber_style;
			}

			Point2 ofs;

			decr->draw(ci, Point2());

			if (orientation == HORIZONTAL) {
				ofs.x += decr->get_width();
			} else {
				ofs.y += decr->get_height();
			}

			Size2 area = get_size();

			if (orientation == HORIZONTAL) {
				area.width -= incr->get_width() + decr->get_width();
			} else {
				area.height -= incr->get_height() + decr->get_height();
			}

			if (has_focus()) {
				theme_cache.scroll_focus_style->draw(ci, Rect2(ofs, area));
			} else {
				theme_cache.scroll_style->draw(ci, Rect2(ofs, area));
			}

			if (orientation == HORIZONTAL) {
				ofs.width += area.width;
			} else {
				ofs.height += area.height;
			}

			incr->draw(ci, ofs);
			Rect2 grabber_rect;

			if (orientation == HORIZONTAL) {
				grabber_rect.size.width = get_grabber_size();
				grabber_rect.size.height = get_size().height;
				grabber_rect.position.y = 0;
				grabber_rect.position.x = get_grabber_offset() + decr->get_width() + theme_cache.scroll_style->get_margin(SIDE_LEFT);
			} else {
				grabber_rect.size.width = get_size().width;
				grabber_rect.size.height = get_grabber_size();
				grabber_rect.position.y = get_grabber_offset() + decr->get_height() + theme_cache.scroll_style->get_margin(SIDE_TOP);
				grabber_rect.position.x = 0;
			}

			grabber->draw(ci, grabber_rect);
		} break;

		case NOTIFICATION_ENTER_TREE: {
			if (has_node(drag_node_path)) {
				Node *n = get_node(drag_node_path);
				drag_node = Object::cast_to<Control>(n);
			}

			if (drag_node) {
				drag_node->connect(SceneStringName(gui_input), callable_mp(this, &ScrollBar::_drag_node_input));
				drag_node->connect(SceneStringName(tree_exiting), callable_mp(this, &ScrollBar::_drag_node_exit), CONNECT_ONE_SHOT);
			}
		} break;

		case NOTIFICATION_EXIT_TREE: {
			if (drag_node) {
				drag_node->disconnect(SceneStringName(gui_input), callable_mp(this, &ScrollBar::_drag_node_input));
				drag_node->disconnect(SceneStringName(tree_exiting), callable_mp(this, &ScrollBar::_drag_node_exit));
			}

			drag_node = nullptr;
		} break;

		case NOTIFICATION_INTERNAL_PHYSICS_PROCESS: {
			if (scrolling) {
				if (get_value() != target_scroll) {
					double target = target_scroll - get_value();
					double dist = abs(target);
					double vel = ((target / dist) * 500) * get_physics_process_delta_time();

					if (Math::abs(vel) >= dist) {
						scroll_to(target_scroll);
						scrolling = false;
						set_physics_process_internal(false);
					} else {
						scroll(vel);
					}
				} else {
					scrolling = false;
					set_physics_process_internal(false);
				}

			} else if (drag_node_touching) {
				if (drag_node_touching_deaccel) {
					Vector2 pos = Vector2(orientation == HORIZONTAL ? get_value() : 0, orientation == VERTICAL ? get_value() : 0);
					pos += drag_node_speed * get_physics_process_delta_time();

					bool turnoff = false;

					if (orientation == HORIZONTAL) {
						if (pos.x < 0) {
							pos.x = 0;
							turnoff = true;
						}

						if (pos.x > (get_max() - get_page())) {
							pos.x = get_max() - get_page();
							turnoff = true;
						}

						scroll_to(pos.x);

						float sgn_x = drag_node_speed.x < 0 ? -1 : 1;
						float val_x = Math::abs(drag_node_speed.x);
						val_x -= 1000 * get_physics_process_delta_time();

						if (val_x < 0) {
							turnoff = true;
						}

						drag_node_speed.x = sgn_x * val_x;

					} else {
						if (pos.y < 0) {
							pos.y = 0;
							turnoff = true;
						}

						if (pos.y > (get_max() - get_page())) {
							pos.y = get_max() - get_page();
							turnoff = true;
						}

						scroll_to(pos.y);

						float sgn_y = drag_node_speed.y < 0 ? -1 : 1;
						float val_y = Math::abs(drag_node_speed.y);
						val_y -= 1000 * get_physics_process_delta_time();

						if (val_y < 0) {
							turnoff = true;
						}
						drag_node_speed.y = sgn_y * val_y;
					}

					if (turnoff) {
						set_physics_process_internal(false);
						drag_node_touching = false;
						drag_node_touching_deaccel = false;
					}

				} else {
					if (time_since_motion == 0 || time_since_motion > 0.1) {
						Vector2 diff = drag_node_accum - last_drag_node_accum;
						last_drag_node_accum = drag_node_accum;
						drag_node_speed = diff / get_physics_process_delta_time();
					}

					time_since_motion += get_physics_process_delta_time();
				}
			}
		} break;

		case NOTIFICATION_VISIBILITY_CHANGED: {
			if (!is_visible()) {
				incr_active = false;
				decr_active = false;
				drag.active = false;
			}
		} break;

		case NOTIFICATION_MOUSE_EXIT: {
			highlight = HIGHLIGHT_NONE;
			queue_redraw();
		} break;
	}
}

double ScrollBar::get_grabber_min_size() const {
	Ref<StyleBox> grabber = theme_cache.grabber_style;
	Size2 gminsize = grabber->get_minimum_size();
	return (orientation == VERTICAL) ? gminsize.height : gminsize.width;
}

double ScrollBar::get_grabber_size() const {
	float range = get_max() - get_min();
	if (range <= 0) {
		return 0;
	}

	float page = (get_page() > 0) ? get_page() : 0;
	double area_size = get_area_size();
	double grabber_size = page / range * area_size;
	return grabber_size + get_grabber_min_size();
}

double ScrollBar::get_area_size() const {
	switch (orientation) {
		case VERTICAL: {
			double area = get_size().height;
			area -= theme_cache.scroll_style->get_minimum_size().height;
			area -= theme_cache.increment_icon->get_height();
			area -= theme_cache.decrement_icon->get_height();
			area -= get_grabber_min_size();
			return area;
		} break;
		case HORIZONTAL: {
			double area = get_size().width;
			area -= theme_cache.scroll_style->get_minimum_size().width;
			area -= theme_cache.increment_icon->get_width();
			area -= theme_cache.decrement_icon->get_width();
			area -= get_grabber_min_size();
			return area;
		} break;
		default: {
			return 0.0;
		}
	}
}

double ScrollBar::get_grabber_offset() const {
	return get_area_size() * get_as_ratio();
}

Size2 ScrollBar::get_minimum_size() const {
	Ref<Texture2D> incr = theme_cache.increment_icon;
	Ref<Texture2D> decr = theme_cache.decrement_icon;
	Ref<StyleBox> bg = theme_cache.scroll_style;
	Size2 minsize;

	if (orientation == VERTICAL) {
		minsize.width = MAX(incr->get_size().width, bg->get_minimum_size().width);
		minsize.height += incr->get_size().height;
		minsize.height += decr->get_size().height;
		minsize.height += bg->get_minimum_size().height;
		minsize.height += get_grabber_min_size();
	}

	if (orientation == HORIZONTAL) {
		minsize.height = MAX(incr->get_size().height, bg->get_minimum_size().height);
		minsize.width += incr->get_size().width;
		minsize.width += decr->get_size().width;
		minsize.width += bg->get_minimum_size().width;
		minsize.width += get_grabber_min_size();
	}

	return minsize;
}

void ScrollBar::scroll(double p_amount) {
	scroll_to(get_value() + p_amount);
}

void ScrollBar::scroll_to(double p_position) {
	double prev_scroll = get_value();
	set_value(p_position);
	if (!Math::is_equal_approx(prev_scroll, get_value())) {
		emit_signal(SNAME("scrolling"));
	}
}

void ScrollBar::set_custom_step(float p_custom_step) {
	custom_step = p_custom_step;
}

float ScrollBar::get_custom_step() const {
	return custom_step;
}

void ScrollBar::_drag_node_exit() {
	if (drag_node) {
		drag_node->disconnect(SceneStringName(gui_input), callable_mp(this, &ScrollBar::_drag_node_input));
	}
	drag_node = nullptr;
}

void ScrollBar::_drag_node_input(const Ref<InputEvent> &p_input) {
	if (!drag_node_enabled) {
		return;
	}

	Ref<InputEventMouseButton> mb = p_input;

	if (mb.is_valid()) {
		if (mb->get_button_index() != MouseButton::LEFT) {
			return;
		}

		if (mb->is_pressed()) {
			drag_node_speed = Vector2();
			drag_node_accum = Vector2();
			last_drag_node_accum = Vector2();
			drag_node_from = Vector2(orientation == HORIZONTAL ? get_value() : 0, orientation == VERTICAL ? get_value() : 0);
			drag_node_touching = DisplayServer::get_singleton()->is_touchscreen_available();
			drag_node_touching_deaccel = false;
			time_since_motion = 0;

			if (drag_node_touching) {
				set_physics_process_internal(true);
				time_since_motion = 0;
			}

		} else {
			if (drag_node_touching) {
				if (drag_node_speed == Vector2()) {
					drag_node_touching_deaccel = false;
					drag_node_touching = false;
					set_physics_process_internal(false);
				} else {
					drag_node_touching_deaccel = true;
				}
			}
		}
	}

	Ref<InputEventMouseMotion> mm = p_input;

	if (mm.is_valid()) {
		if (drag_node_touching && !drag_node_touching_deaccel) {
			Vector2 motion = mm->get_relative();

			drag_node_accum -= motion;
			Vector2 diff = drag_node_from + drag_node_accum;

			if (orientation == HORIZONTAL) {
				scroll_to(diff.x);
			}

			if (orientation == VERTICAL) {
				scroll_to(diff.y);
			}

			time_since_motion = 0;
		}
	}
}

void ScrollBar::set_drag_node(const NodePath &p_path) {
	if (is_inside_tree()) {
		if (drag_node) {
			drag_node->disconnect(SceneStringName(gui_input), callable_mp(this, &ScrollBar::_drag_node_input));
			drag_node->disconnect(SceneStringName(tree_exiting), callable_mp(this, &ScrollBar::_drag_node_exit));
		}
	}

	drag_node = nullptr;
	drag_node_path = p_path;

	if (is_inside_tree()) {
		if (has_node(p_path)) {
			Node *n = get_node(p_path);
			drag_node = Object::cast_to<Control>(n);
		}

		if (drag_node) {
			drag_node->connect(SceneStringName(gui_input), callable_mp(this, &ScrollBar::_drag_node_input));
			drag_node->connect(SceneStringName(tree_exiting), callable_mp(this, &ScrollBar::_drag_node_exit), CONNECT_ONE_SHOT);
		}
	}
}

NodePath ScrollBar::get_drag_node() const {
	return drag_node_path;
}

void ScrollBar::set_drag_node_enabled(bool p_enable) {
	drag_node_enabled = p_enable;
}

void ScrollBar::set_smooth_scroll_enabled(bool p_enable) {
	smooth_scroll_enabled = p_enable;
}

bool ScrollBar::is_smooth_scroll_enabled() const {
	return smooth_scroll_enabled;
}

void ScrollBar::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_custom_step", "step"), &ScrollBar::set_custom_step);
	ClassDB::bind_method(D_METHOD("get_custom_step"), &ScrollBar::get_custom_step);

	ADD_SIGNAL(MethodInfo("scrolling"));

	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "custom_step", PROPERTY_HINT_RANGE, "-1,4096,suffix:px"), "set_custom_step", "get_custom_step");

	BIND_THEME_ITEM_CUSTOM(Theme::DATA_TYPE_STYLEBOX, ScrollBar, scroll_style, "scroll");
	BIND_THEME_ITEM_CUSTOM(Theme::DATA_TYPE_STYLEBOX, ScrollBar, scroll_focus_style, "scroll_focus");
	BIND_THEME_ITEM_CUSTOM(Theme::DATA_TYPE_STYLEBOX, ScrollBar, grabber_style, "grabber");
	BIND_THEME_ITEM_CUSTOM(Theme::DATA_TYPE_STYLEBOX, ScrollBar, grabber_hl_style, "grabber_highlight");
	BIND_THEME_ITEM_CUSTOM(Theme::DATA_TYPE_STYLEBOX, ScrollBar, grabber_pressed_style, "grabber_pressed");

	BIND_THEME_ITEM_CUSTOM(Theme::DATA_TYPE_ICON, ScrollBar, increment_icon, "increment");
	BIND_THEME_ITEM_CUSTOM(Theme::DATA_TYPE_ICON, ScrollBar, increment_hl_icon, "increment_highlight");
	BIND_THEME_ITEM_CUSTOM(Theme::DATA_TYPE_ICON, ScrollBar, increment_pressed_icon, "increment_pressed");
	BIND_THEME_ITEM_CUSTOM(Theme::DATA_TYPE_ICON, ScrollBar, decrement_icon, "decrement");
	BIND_THEME_ITEM_CUSTOM(Theme::DATA_TYPE_ICON, ScrollBar, decrement_hl_icon, "decrement_highlight");
	BIND_THEME_ITEM_CUSTOM(Theme::DATA_TYPE_ICON, ScrollBar, decrement_pressed_icon, "decrement_pressed");
}

ScrollBar::ScrollBar(Orientation p_orientation) {
	orientation = p_orientation;

	if (focus_by_default) {
		set_focus_mode(FOCUS_ALL);
	}
	set_step(0);
}

ScrollBar::~ScrollBar() {
}

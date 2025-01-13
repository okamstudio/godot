/**************************************************************************/
/*  spin_box.cpp                                                          */
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

#include "spin_box.h"

#include "core/input/input.h"
#include "core/math/expression.h"
#include "scene/theme/theme_db.h"

Size2 SpinBox::get_minimum_size() const {
	Size2 ms = line_edit->get_combined_minimum_size();
	ms.width += sizing_cache.buttons_block_width;
	return ms;
}

void SpinBox::_update_text(bool p_keep_line_edit) {
	double step = get_step();
	if (use_custom_arrow_step && custom_arrow_step != 0.0) {
		step = custom_arrow_step;
	}
	String value = String::num(get_value(), Math::range_step_decimals(step));
	if (is_localizing_numeral_system()) {
		value = TS->format_number(value);
	}

	if (!line_edit->is_editing()) {
		if (!prefix.is_empty()) {
			value = prefix + " " + value;
		}
		if (!suffix.is_empty()) {
			value += " " + suffix;
		}
	}

	if (p_keep_line_edit && value == last_updated_text && value != line_edit->get_text()) {
		return;
	}

	line_edit->set_text_with_selection(value);
	last_updated_text = value;
}

void SpinBox::_text_submitted(const String &p_string) {
	if (p_string.is_empty()) {
		return;
	}

	Ref<Expression> expr;
	expr.instantiate();

	// Convert commas ',' to dots '.' for French/German etc. keyboard layouts.
	String text = p_string.replace(",", ".");
	text = text.replace(";", ",");
	text = TS->parse_number(text);
	// Ignore the prefix and suffix in the expression.
	text = text.trim_prefix(prefix + " ").trim_suffix(" " + suffix);

	Error err = expr->parse(text);

	use_custom_arrow_step = false;

	if (err != OK) {
		// If the expression failed try without converting commas to dots - they might have been for parameter separation.
		text = p_string;
		text = TS->parse_number(text);
		text = text.trim_prefix(prefix + " ").trim_suffix(" " + suffix);

		err = expr->parse(text);
		if (err != OK) {
			_update_text();
			return;
		}
	}

	Variant value = expr->execute(Array(), nullptr, false, true);
	if (value.get_type() != Variant::NIL) {
		set_value(value);
	}
	_update_text();
}

void SpinBox::_text_changed(const String &p_string) {
	int cursor_pos = line_edit->get_caret_column();

	_text_submitted(p_string);

	// Line edit 'set_text' method resets the cursor position so we need to undo that.
	line_edit->set_caret_column(cursor_pos);
}

LineEdit *SpinBox::get_line_edit() {
	return line_edit;
}

void SpinBox::_line_edit_input(const Ref<InputEvent> &p_event) {
}

void SpinBox::_range_click_timeout() {
	if (!drag.enabled && Input::get_singleton()->is_mouse_button_pressed(MouseButton::LEFT)) {
		bool up = get_local_mouse_position().y < (get_size().height / 2);
		double step = get_step();
		// Arrow button is being pressed, so we also need to set the step to the same value as custom_arrow_step if its not 0.
		double temp_step = get_custom_arrow_step() != 0.0 ? get_custom_arrow_step() : get_step();
		_set_step_no_signal(temp_step);
		set_value(get_value() + (up ? temp_step : -temp_step));
		_set_step_no_signal(step);
		use_custom_arrow_step = true;

		if (range_click_timer->is_one_shot()) {
			range_click_timer->set_wait_time(0.075);
			range_click_timer->set_one_shot(false);
			range_click_timer->start();
		}

	} else {
		range_click_timer->stop();
	}
}

void SpinBox::_release_mouse_from_drag_mode() {
	if (drag.enabled) {
		drag.enabled = false;
		Input::get_singleton()->set_mouse_mode(Input::MOUSE_MODE_HIDDEN);
		warp_mouse(drag.capture_pos);
		Input::get_singleton()->set_mouse_mode(Input::MOUSE_MODE_VISIBLE);
	}
}

void SpinBox::_mouse_exited() {
	if (state_cache.up_button_hovered || state_cache.down_button_hovered) {
		state_cache.up_button_hovered = false;
		state_cache.down_button_hovered = false;
		queue_redraw();
	}
}

void SpinBox::gui_input(const Ref<InputEvent> &p_event) {
	ERR_FAIL_COND(p_event.is_null());

	if (!is_editable()) {
		return;
	}

	Ref<InputEventMouse> me = p_event;
	Ref<InputEventMouseButton> mb = p_event;
	Ref<InputEventMouseMotion> mm = p_event;

	double step = get_step();
	Vector2 mpos;
	bool mouse_on_up_button = false;
	bool mouse_on_down_button = false;
	if (mb.is_valid() || mm.is_valid()) {
		Rect2 up_button_rc = Rect2(sizing_cache.buttons_left, 0, sizing_cache.buttons_width, sizing_cache.button_up_height);
		Rect2 down_button_rc = Rect2(sizing_cache.buttons_left, sizing_cache.second_button_top, sizing_cache.buttons_width, sizing_cache.button_down_height);

		mpos = me->get_position();

		mouse_on_up_button = up_button_rc.has_point(mpos);
		mouse_on_down_button = down_button_rc.has_point(mpos);
	}

	if (mb.is_valid() && mb->is_pressed()) {
		switch (mb->get_button_index()) {
			case MouseButton::LEFT: {
				line_edit->grab_focus();

				if (mouse_on_up_button || mouse_on_down_button) {
					// Arrow button is being pressed, so step is being changed temporarily.
					double temp_step = get_custom_arrow_step() != 0.0 ? get_custom_arrow_step() : get_step();
					_set_step_no_signal(temp_step);
					set_value(get_value() + (mouse_on_up_button ? temp_step : -temp_step));
					_set_step_no_signal(step);
					use_custom_arrow_step = true;
				}
				state_cache.up_button_pressed = mouse_on_up_button;
				state_cache.down_button_pressed = mouse_on_down_button;
				queue_redraw();

				range_click_timer->set_wait_time(0.6);
				range_click_timer->set_one_shot(true);
				range_click_timer->start();

				drag.allowed = true;
				drag.capture_pos = mb->get_position();
			} break;
			case MouseButton::RIGHT: {
				line_edit->grab_focus();
				if (mouse_on_up_button || mouse_on_down_button) {
					use_custom_arrow_step = false;
					set_value(mouse_on_up_button ? get_max() : get_min());
				}
			} break;
			case MouseButton::WHEEL_UP: {
				if (line_edit->is_editing()) {
					use_custom_arrow_step = false;
					set_value(get_value() + step * mb->get_factor());
					accept_event();
				}
			} break;
			case MouseButton::WHEEL_DOWN: {
				if (line_edit->is_editing()) {
					use_custom_arrow_step = false;
					set_value(get_value() - step * mb->get_factor());
					accept_event();
				}
			} break;
			default:
				break;
		}
	}

	if (mb.is_valid() && !mb->is_pressed() && mb->get_button_index() == MouseButton::LEFT) {
		if (state_cache.up_button_pressed || state_cache.down_button_pressed) {
			state_cache.up_button_pressed = false;
			state_cache.down_button_pressed = false;
			queue_redraw();
		}

		//set_default_cursor_shape(CURSOR_ARROW);
		range_click_timer->stop();
		_release_mouse_from_drag_mode();
		drag.allowed = false;
		line_edit->clear_pending_select_all_on_focus();
	}

	if (mm.is_valid()) {
		bool old_up_hovered = state_cache.up_button_hovered;
		bool old_down_hovered = state_cache.down_button_hovered;

		state_cache.up_button_hovered = mouse_on_up_button;
		state_cache.down_button_hovered = mouse_on_down_button;

		if (old_up_hovered != state_cache.up_button_hovered || old_down_hovered != state_cache.down_button_hovered) {
			queue_redraw();
		}
	}

	if (mm.is_valid() && (mm->get_button_mask().has_flag(MouseButtonMask::LEFT))) {
		if (drag.enabled) {
			drag.diff_y += mm->get_relative().y;
			double diff_y = -0.01 * Math::pow(ABS(drag.diff_y), 1.8) * SIGN(drag.diff_y);
			use_custom_arrow_step = false;
			set_value(CLAMP(drag.base_val + step * diff_y, get_min(), get_max()));
		} else if (drag.allowed && drag.capture_pos.distance_to(mm->get_position()) > 2) {
			Input::get_singleton()->set_mouse_mode(Input::MOUSE_MODE_CAPTURED);
			drag.enabled = true;
			drag.base_val = get_value();
			drag.diff_y = 0;
		}
	}
}

void SpinBox::_line_edit_editing_toggled(bool p_toggled_on) {
	if (p_toggled_on) {
		int col = line_edit->get_caret_column();
		_update_text();
		line_edit->set_caret_column(col);

		// LineEdit text might change and it clears any selection. Have to re-select here.
		if (line_edit->is_select_all_on_focus() && !Input::get_singleton()->is_mouse_button_pressed(MouseButton::LEFT)) {
			line_edit->select_all();
		}
	} else {
		// Discontinue because the focus_exit was caused by canceling or the text is empty.
		if (Input::get_singleton()->is_action_pressed("ui_cancel") || line_edit->get_text().is_empty()) {
			_update_text();
			return;
		}

		line_edit->deselect();
		_text_submitted(line_edit->get_text());
	}
}

inline void SpinBox::_compute_sizes() {
	int buttons_block_wanted_width = theme_cache.buttons_width + theme_cache.field_and_buttons_separation;
	int buttons_block_icon_enforced_width = _get_widest_button_icon_width() + theme_cache.field_and_buttons_separation;

#ifndef DISABLE_DEPRECATED
	const bool min_width_from_icons = theme_cache.set_min_buttons_width_from_icons || (theme_cache.buttons_width < 0);
#else
	const bool min_width_from_icons = theme_cache.buttons_width < 0;
#endif
	int w = min_width_from_icons != 0 ? MAX(buttons_block_icon_enforced_width, buttons_block_wanted_width) : buttons_block_wanted_width;

	if (w != sizing_cache.buttons_block_width) {
		line_edit->set_offset(SIDE_LEFT, 0);
		line_edit->set_offset(SIDE_RIGHT, -w);
		sizing_cache.buttons_block_width = w;
	}

	Size2i size = get_size();

	sizing_cache.buttons_width = w - theme_cache.field_and_buttons_separation;
	sizing_cache.buttons_vertical_separation = CLAMP(theme_cache.buttons_vertical_separation, 0, size.height);
	sizing_cache.buttons_left = is_layout_rtl() ? 0 : size.width - sizing_cache.buttons_width;
	sizing_cache.button_up_height = (size.height - sizing_cache.buttons_vertical_separation) / 2;
	sizing_cache.button_down_height = size.height - sizing_cache.button_up_height - sizing_cache.buttons_vertical_separation;
	sizing_cache.second_button_top = size.height - sizing_cache.button_down_height;

	sizing_cache.buttons_separator_top = sizing_cache.button_up_height;
	sizing_cache.field_and_buttons_separator_left = is_layout_rtl() ? sizing_cache.buttons_width : size.width - sizing_cache.buttons_block_width;
	sizing_cache.field_and_buttons_separator_width = theme_cache.field_and_buttons_separation;
}

inline int SpinBox::_get_widest_button_icon_width() {
	int max = 0;
	max = MAX(max, theme_cache.updown_icon->get_width());
	max = MAX(max, theme_cache.up_icon->get_width());
	max = MAX(max, theme_cache.up_hover_icon->get_width());
	max = MAX(max, theme_cache.up_pressed_icon->get_width());
	max = MAX(max, theme_cache.up_disabled_icon->get_width());
	max = MAX(max, theme_cache.down_icon->get_width());
	max = MAX(max, theme_cache.down_hover_icon->get_width());
	max = MAX(max, theme_cache.down_pressed_icon->get_width());
	max = MAX(max, theme_cache.down_disabled_icon->get_width());
	return max;
}

void SpinBox::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_DRAW: {
			_update_text(true);
			_compute_sizes();

			RID ci = get_canvas_item();
			Size2i size = get_size();

			Ref<StyleBox> up_stylebox = theme_cache.up_base_stylebox;
			Ref<StyleBox> down_stylebox = theme_cache.down_base_stylebox;
			Ref<Texture2D> up_icon = theme_cache.up_icon;
			Ref<Texture2D> down_icon = theme_cache.down_icon;
			Color up_icon_modulate = theme_cache.up_icon_modulate;
			Color down_icon_modulate = theme_cache.down_icon_modulate;

			bool is_fully_disabled = !is_editable();

			if (state_cache.up_button_disabled || is_fully_disabled) {
				up_stylebox = theme_cache.up_disabled_stylebox;
				up_icon = theme_cache.up_disabled_icon;
				up_icon_modulate = theme_cache.up_disabled_icon_modulate;
			} else if (state_cache.up_button_pressed && !drag.enabled) {
				up_stylebox = theme_cache.up_pressed_stylebox;
				up_icon = theme_cache.up_pressed_icon;
				up_icon_modulate = theme_cache.up_pressed_icon_modulate;
			} else if (state_cache.up_button_hovered && !drag.enabled) {
				up_stylebox = theme_cache.up_hover_stylebox;
				up_icon = theme_cache.up_hover_icon;
				up_icon_modulate = theme_cache.up_hover_icon_modulate;
			}

			if (state_cache.down_button_disabled || is_fully_disabled) {
				down_stylebox = theme_cache.down_disabled_stylebox;
				down_icon = theme_cache.down_disabled_icon;
				down_icon_modulate = theme_cache.down_disabled_icon_modulate;
			} else if (state_cache.down_button_pressed && !drag.enabled) {
				down_stylebox = theme_cache.down_pressed_stylebox;
				down_icon = theme_cache.down_pressed_icon;
				down_icon_modulate = theme_cache.down_pressed_icon_modulate;
			} else if (state_cache.down_button_hovered && !drag.enabled) {
				down_stylebox = theme_cache.down_hover_stylebox;
				down_icon = theme_cache.down_hover_icon;
				down_icon_modulate = theme_cache.down_hover_icon_modulate;
			}

			int updown_icon_left = sizing_cache.buttons_left + (sizing_cache.buttons_width - theme_cache.updown_icon->get_width()) / 2;
			int updown_icon_top = (size.height - theme_cache.updown_icon->get_height()) / 2;

			// Compute center icon positions once we know which one is used.
			int up_icon_left = sizing_cache.buttons_left + (sizing_cache.buttons_width - up_icon->get_width()) / 2;
			int up_icon_top = (sizing_cache.button_up_height - up_icon->get_height()) / 2;
			int down_icon_left = sizing_cache.buttons_left + (sizing_cache.buttons_width - down_icon->get_width()) / 2;
			int down_icon_top = sizing_cache.second_button_top + (sizing_cache.button_down_height - down_icon->get_height()) / 2;

			// Draw separators.
			draw_style_box(theme_cache.up_down_buttons_separator, Rect2(sizing_cache.buttons_left, sizing_cache.buttons_separator_top, sizing_cache.buttons_width, sizing_cache.buttons_vertical_separation));
			draw_style_box(theme_cache.field_and_buttons_separator, Rect2(sizing_cache.field_and_buttons_separator_left, 0, sizing_cache.field_and_buttons_separator_width, size.height));

			// Draw buttons.
			draw_style_box(up_stylebox, Rect2(sizing_cache.buttons_left, 0, sizing_cache.buttons_width, sizing_cache.button_up_height));
			draw_style_box(down_stylebox, Rect2(sizing_cache.buttons_left, sizing_cache.second_button_top, sizing_cache.buttons_width, sizing_cache.button_down_height));

			// Draw arrows.
			theme_cache.updown_icon->draw(ci, Point2i(updown_icon_left, updown_icon_top));
			draw_texture(up_icon, Point2i(up_icon_left, up_icon_top), up_icon_modulate);
			draw_texture(down_icon, Point2i(down_icon_left, down_icon_top), down_icon_modulate);

		} break;

		case NOTIFICATION_MOUSE_EXIT: {
			_mouse_exited();
		} break;

		case NOTIFICATION_ENTER_TREE: {
			_compute_sizes();
			_update_text();
			_update_buttons_state_for_current_value();
		} break;

		case NOTIFICATION_VISIBILITY_CHANGED:
			drag.allowed = false;
			[[fallthrough]];
		case NOTIFICATION_EXIT_TREE: {
			_release_mouse_from_drag_mode();
		} break;

		case NOTIFICATION_TRANSLATION_CHANGED: {
			queue_redraw();
		} break;

		case NOTIFICATION_THEME_CHANGED: {
			callable_mp((Control *)this, &Control::update_minimum_size).call_deferred();
			callable_mp((Control *)get_line_edit(), &Control::update_minimum_size).call_deferred();
		} break;

		case NOTIFICATION_LAYOUT_DIRECTION_CHANGED: {
			queue_redraw();
		} break;
	}
}

void SpinBox::set_horizontal_alignment(HorizontalAlignment p_alignment) {
	line_edit->set_horizontal_alignment(p_alignment);
}

HorizontalAlignment SpinBox::get_horizontal_alignment() const {
	return line_edit->get_horizontal_alignment();
}

void SpinBox::set_suffix(const String &p_suffix) {
	if (suffix == p_suffix) {
		return;
	}

	suffix = p_suffix;
	_update_text();
}

String SpinBox::get_suffix() const {
	return suffix;
}

void SpinBox::set_prefix(const String &p_prefix) {
	if (prefix == p_prefix) {
		return;
	}

	prefix = p_prefix;
	_update_text();
}

String SpinBox::get_prefix() const {
	return prefix;
}

void SpinBox::set_update_on_text_changed(bool p_enabled) {
	if (update_on_text_changed == p_enabled) {
		return;
	}

	update_on_text_changed = p_enabled;

	if (p_enabled) {
		line_edit->connect(SceneStringName(text_changed), callable_mp(this, &SpinBox::_text_changed), CONNECT_DEFERRED);
	} else {
		line_edit->disconnect(SceneStringName(text_changed), callable_mp(this, &SpinBox::_text_changed));
	}
}

bool SpinBox::get_update_on_text_changed() const {
	return update_on_text_changed;
}

void SpinBox::set_select_all_on_focus(bool p_enabled) {
	line_edit->set_select_all_on_focus(p_enabled);
}

bool SpinBox::is_select_all_on_focus() const {
	return line_edit->is_select_all_on_focus();
}

void SpinBox::set_editable(bool p_enabled) {
	line_edit->set_editable(p_enabled);
	queue_redraw();
}

bool SpinBox::is_editable() const {
	return line_edit->is_editable();
}

void SpinBox::apply() {
	_text_submitted(line_edit->get_text());
}

void SpinBox::set_custom_arrow_step(double p_custom_arrow_step) {
	custom_arrow_step = p_custom_arrow_step;
}

double SpinBox::get_custom_arrow_step() const {
	return custom_arrow_step;
}

void SpinBox::_value_changed(double p_value) {
	_update_buttons_state_for_current_value();
}

void SpinBox::_update_buttons_state_for_current_value() {
	double value = get_value();
	bool should_disable_up = value == get_max() && !is_greater_allowed();
	bool should_disable_down = value == get_min() && !is_lesser_allowed();

	if (state_cache.up_button_disabled != should_disable_up || state_cache.down_button_disabled != should_disable_down) {
		state_cache.up_button_disabled = should_disable_up;
		state_cache.down_button_disabled = should_disable_down;
		queue_redraw();
	}
}

void SpinBox::_set_step_no_signal(double p_step) {
	set_block_signals(true);
	set_step(p_step);
	set_block_signals(false);
}

void SpinBox::_validate_property(PropertyInfo &p_property) const {
	if (p_property.name == "exp_edit") {
		p_property.usage = PROPERTY_USAGE_NONE;
	}
}

void SpinBox::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_horizontal_alignment", "alignment"), &SpinBox::set_horizontal_alignment);
	ClassDB::bind_method(D_METHOD("get_horizontal_alignment"), &SpinBox::get_horizontal_alignment);
	ClassDB::bind_method(D_METHOD("set_suffix", "suffix"), &SpinBox::set_suffix);
	ClassDB::bind_method(D_METHOD("get_suffix"), &SpinBox::get_suffix);
	ClassDB::bind_method(D_METHOD("set_prefix", "prefix"), &SpinBox::set_prefix);
	ClassDB::bind_method(D_METHOD("get_prefix"), &SpinBox::get_prefix);
	ClassDB::bind_method(D_METHOD("set_editable", "enabled"), &SpinBox::set_editable);
	ClassDB::bind_method(D_METHOD("set_custom_arrow_step", "arrow_step"), &SpinBox::set_custom_arrow_step);
	ClassDB::bind_method(D_METHOD("get_custom_arrow_step"), &SpinBox::get_custom_arrow_step);
	ClassDB::bind_method(D_METHOD("is_editable"), &SpinBox::is_editable);
	ClassDB::bind_method(D_METHOD("set_update_on_text_changed", "enabled"), &SpinBox::set_update_on_text_changed);
	ClassDB::bind_method(D_METHOD("get_update_on_text_changed"), &SpinBox::get_update_on_text_changed);
	ClassDB::bind_method(D_METHOD("set_select_all_on_focus", "enabled"), &SpinBox::set_select_all_on_focus);
	ClassDB::bind_method(D_METHOD("is_select_all_on_focus"), &SpinBox::is_select_all_on_focus);
	ClassDB::bind_method(D_METHOD("apply"), &SpinBox::apply);
	ClassDB::bind_method(D_METHOD("get_line_edit"), &SpinBox::get_line_edit);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "alignment", PROPERTY_HINT_ENUM, "Left,Center,Right,Fill"), "set_horizontal_alignment", "get_horizontal_alignment");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "editable"), "set_editable", "is_editable");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "update_on_text_changed"), "set_update_on_text_changed", "get_update_on_text_changed");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "prefix"), "set_prefix", "get_prefix");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "suffix"), "set_suffix", "get_suffix");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "custom_arrow_step", PROPERTY_HINT_RANGE, "0,10000,0.0001,or_greater"), "set_custom_arrow_step", "get_custom_arrow_step");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "select_all_on_focus"), "set_select_all_on_focus", "is_select_all_on_focus");

	BIND_THEME_ITEM(Theme::DATA_TYPE_CONSTANT, SpinBox, buttons_vertical_separation);
	BIND_THEME_ITEM(Theme::DATA_TYPE_CONSTANT, SpinBox, field_and_buttons_separation);
	BIND_THEME_ITEM(Theme::DATA_TYPE_CONSTANT, SpinBox, buttons_width);
#ifndef DISABLE_DEPRECATED
	BIND_THEME_ITEM(Theme::DATA_TYPE_CONSTANT, SpinBox, set_min_buttons_width_from_icons);
#endif

	BIND_THEME_ITEM_CUSTOM(Theme::DATA_TYPE_ICON, SpinBox, updown_icon, "updown");
	BIND_THEME_ITEM_CUSTOM(Theme::DATA_TYPE_ICON, SpinBox, up_icon, "up");
	BIND_THEME_ITEM_CUSTOM(Theme::DATA_TYPE_ICON, SpinBox, up_hover_icon, "up_hover");
	BIND_THEME_ITEM_CUSTOM(Theme::DATA_TYPE_ICON, SpinBox, up_pressed_icon, "up_pressed");
	BIND_THEME_ITEM_CUSTOM(Theme::DATA_TYPE_ICON, SpinBox, up_disabled_icon, "up_disabled");
	BIND_THEME_ITEM_CUSTOM(Theme::DATA_TYPE_ICON, SpinBox, down_icon, "down");
	BIND_THEME_ITEM_CUSTOM(Theme::DATA_TYPE_ICON, SpinBox, down_hover_icon, "down_hover");
	BIND_THEME_ITEM_CUSTOM(Theme::DATA_TYPE_ICON, SpinBox, down_pressed_icon, "down_pressed");
	BIND_THEME_ITEM_CUSTOM(Theme::DATA_TYPE_ICON, SpinBox, down_disabled_icon, "down_disabled");

	BIND_THEME_ITEM_CUSTOM(Theme::DATA_TYPE_STYLEBOX, SpinBox, up_base_stylebox, "up_background");
	BIND_THEME_ITEM_CUSTOM(Theme::DATA_TYPE_STYLEBOX, SpinBox, up_hover_stylebox, "up_background_hovered");
	BIND_THEME_ITEM_CUSTOM(Theme::DATA_TYPE_STYLEBOX, SpinBox, up_pressed_stylebox, "up_background_pressed");
	BIND_THEME_ITEM_CUSTOM(Theme::DATA_TYPE_STYLEBOX, SpinBox, up_disabled_stylebox, "up_background_disabled");
	BIND_THEME_ITEM_CUSTOM(Theme::DATA_TYPE_STYLEBOX, SpinBox, down_base_stylebox, "down_background");
	BIND_THEME_ITEM_CUSTOM(Theme::DATA_TYPE_STYLEBOX, SpinBox, down_hover_stylebox, "down_background_hovered");
	BIND_THEME_ITEM_CUSTOM(Theme::DATA_TYPE_STYLEBOX, SpinBox, down_pressed_stylebox, "down_background_pressed");
	BIND_THEME_ITEM_CUSTOM(Theme::DATA_TYPE_STYLEBOX, SpinBox, down_disabled_stylebox, "down_background_disabled");

	BIND_THEME_ITEM_CUSTOM(Theme::DATA_TYPE_COLOR, SpinBox, up_icon_modulate, "up_icon_modulate");
	BIND_THEME_ITEM_CUSTOM(Theme::DATA_TYPE_COLOR, SpinBox, up_hover_icon_modulate, "up_hover_icon_modulate");
	BIND_THEME_ITEM_CUSTOM(Theme::DATA_TYPE_COLOR, SpinBox, up_pressed_icon_modulate, "up_pressed_icon_modulate");
	BIND_THEME_ITEM_CUSTOM(Theme::DATA_TYPE_COLOR, SpinBox, up_disabled_icon_modulate, "up_disabled_icon_modulate");
	BIND_THEME_ITEM_CUSTOM(Theme::DATA_TYPE_COLOR, SpinBox, down_icon_modulate, "down_icon_modulate");
	BIND_THEME_ITEM_CUSTOM(Theme::DATA_TYPE_COLOR, SpinBox, down_hover_icon_modulate, "down_hover_icon_modulate");
	BIND_THEME_ITEM_CUSTOM(Theme::DATA_TYPE_COLOR, SpinBox, down_pressed_icon_modulate, "down_pressed_icon_modulate");
	BIND_THEME_ITEM_CUSTOM(Theme::DATA_TYPE_COLOR, SpinBox, down_disabled_icon_modulate, "down_disabled_icon_modulate");

	BIND_THEME_ITEM_CUSTOM(Theme::DATA_TYPE_STYLEBOX, SpinBox, field_and_buttons_separator, "field_and_buttons_separator");
	BIND_THEME_ITEM_CUSTOM(Theme::DATA_TYPE_STYLEBOX, SpinBox, up_down_buttons_separator, "up_down_buttons_separator");
}

SpinBox::SpinBox() {
	line_edit = memnew(LineEdit);
	line_edit->set_emoji_menu_enabled(false);
	add_child(line_edit, false, INTERNAL_MODE_FRONT);

	line_edit->set_theme_type_variation("SpinBoxInnerLineEdit");
	line_edit->set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT);
	line_edit->set_mouse_filter(MOUSE_FILTER_PASS);
	line_edit->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_LEFT);

	line_edit->connect(SceneStringName(text_submitted), callable_mp(this, &SpinBox::_text_submitted), CONNECT_DEFERRED);
	line_edit->connect("editing_toggled", callable_mp(this, &SpinBox::_line_edit_editing_toggled), CONNECT_DEFERRED);
	line_edit->connect(SceneStringName(gui_input), callable_mp(this, &SpinBox::_line_edit_input));

	range_click_timer = memnew(Timer);
	range_click_timer->connect("timeout", callable_mp(this, &SpinBox::_range_click_timeout));
	add_child(range_click_timer, false, INTERNAL_MODE_FRONT);
}

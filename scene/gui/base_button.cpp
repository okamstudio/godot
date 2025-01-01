/**************************************************************************/
/*  base_button.cpp                                                       */
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

#include "base_button.h"

#include "core/config/project_settings.h"
#include "scene/gui/label.h"
#include "scene/main/window.h"

void BaseButton::_unpress_group() {
	if (button_group.is_null()) {
		return;
	}

	if (toggle_mode && !button_group->is_allow_unpress()) {
		status.pressed = true;
	}

	for (BaseButton *E : button_group->buttons) {
		if (E == this) {
			continue;
		}

		E->set_pressed(false);
	}
}

void BaseButton::gui_input(const Ref<InputEvent> &p_event) {
	ERR_FAIL_COND(p_event.is_null());

	if (status.disabled) { // no interaction with disabled button
		return;
	}

	Ref<InputEventMouseButton> mouse_button = p_event;
	bool ui_accept = p_event->is_action("ui_accept", true) && !p_event->is_echo();

	bool button_masked = mouse_button.is_valid() && button_mask.has_flag(mouse_button_to_mask(mouse_button->get_button_index()));
	if (button_masked || ui_accept) {
		was_mouse_pressed = button_masked;
		on_action_event(p_event);
		was_mouse_pressed = false;

		return;
	}

	Ref<InputEventMouseMotion> mouse_motion = p_event;
	if (mouse_motion.is_valid()) {
		if (status.press_attempt) {
			bool last_press_inside = status.pressing_inside;
			status.pressing_inside = has_point(mouse_motion->get_position());
			if (last_press_inside != status.pressing_inside) {
				queue_redraw();
			}
		}
	}
}

void BaseButton::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_MOUSE_ENTER: {
			status.hovering = true;
			queue_redraw();
		} break;

		case NOTIFICATION_MOUSE_EXIT: {
			status.hovering = false;
			queue_redraw();
		} break;

		case NOTIFICATION_DRAG_BEGIN:
		case NOTIFICATION_SCROLL_BEGIN: {
			if (status.press_attempt) {
				status.press_attempt = false;
				queue_redraw();
			}
		} break;

		case NOTIFICATION_FOCUS_ENTER: {
			queue_redraw();
		} break;

		case NOTIFICATION_FOCUS_EXIT: {
			if (status.press_attempt) {
				status.press_attempt = false;
				queue_redraw();
			} else if (status.hovering) {
				queue_redraw();
			}
		} break;

		case NOTIFICATION_VISIBILITY_CHANGED:
		case NOTIFICATION_EXIT_TREE: {
			if (p_what == NOTIFICATION_VISIBILITY_CHANGED && is_visible_in_tree()) {
				break;
			}
			if (!toggle_mode) {
				status.pressed = false;
			}
			status.hovering = false;
			status.press_attempt = false;
			status.pressing_inside = false;
		} break;
	}
}

void BaseButton::_pressed() {
	GDVIRTUAL_CALL(_pressed);
	pressed();
	emit_signal(SceneStringName(pressed));
}

void BaseButton::_toggled(bool p_pressed) {
	GDVIRTUAL_CALL(_toggled, p_pressed);
	toggled(p_pressed);
	emit_signal(SceneStringName(toggled), p_pressed);
}

void BaseButton::on_action_event(Ref<InputEvent> p_event) {
	Ref<InputEventMouseButton> mouse_button = p_event;

	if (p_event->is_pressed() && (mouse_button.is_null() || status.hovering)) {
		status.press_attempt = true;
		status.pressing_inside = true;
		emit_signal(SNAME("button_down"));
	}

	if (status.press_attempt && status.pressing_inside) {
		if (toggle_mode) {
			bool is_pressed = p_event->is_pressed();
			if ((is_pressed && action_mode == ACTION_MODE_BUTTON_PRESS) || (!is_pressed && action_mode == ACTION_MODE_BUTTON_RELEASE)) {
				if (action_mode == ACTION_MODE_BUTTON_PRESS) {
					status.press_attempt = false;
					status.pressing_inside = false;
				}
				status.pressed = !status.pressed;
				_unpress_group();
				if (button_group.is_valid()) {
					button_group->emit_signal(SceneStringName(pressed), this);
				}
				_toggled(status.pressed);
				_pressed();
			}
		} else {
			if ((p_event->is_pressed() && action_mode == ACTION_MODE_BUTTON_PRESS) || (!p_event->is_pressed() && action_mode == ACTION_MODE_BUTTON_RELEASE)) {
				_pressed();
			}
		}
	}

	if (!p_event->is_pressed()) {
		status.press_attempt = false;
		status.pressing_inside = false;
		emit_signal(SNAME("button_up"));
	}

	queue_redraw();
}

void BaseButton::pressed() {
}

void BaseButton::toggled(bool p_pressed) {
}

void BaseButton::set_disabled(bool p_disabled) {
	if (status.disabled == p_disabled) {
		return;
	}

	status.disabled = p_disabled;
	if (p_disabled) {
		if (!toggle_mode) {
			status.pressed = false;
		}
		status.press_attempt = false;
		status.pressing_inside = false;
	}
	queue_redraw();
	update_minimum_size();
}

bool BaseButton::is_disabled() const {
	return status.disabled;
}

void BaseButton::set_pressed(bool p_pressed) {
	bool prev_pressed = status.pressed;
	set_pressed_no_signal(p_pressed);

	if (status.pressed == prev_pressed) {
		return;
	}

	if (p_pressed) {
		_unpress_group();
		if (button_group.is_valid()) {
			button_group->emit_signal(SceneStringName(pressed), this);
		}
	}
	_toggled(status.pressed);
}

void BaseButton::set_pressed_no_signal(bool p_pressed) {
	if (!toggle_mode) {
		return;
	}
	if (status.pressed == p_pressed) {
		return;
	}
	status.pressed = p_pressed;

	queue_redraw();
}

bool BaseButton::is_pressing() const {
	return status.press_attempt;
}

bool BaseButton::is_pressed() const {
	return toggle_mode ? status.pressed : status.press_attempt;
}

bool BaseButton::is_hovered() const {
	return status.hovering;
}

BaseButton::DrawMode BaseButton::get_draw_mode() const {
	if (status.disabled) {
		return DRAW_DISABLED;
	}

	if (in_shortcut_feedback) {
		return DRAW_HOVER_PRESSED;
	}

	if (!status.press_attempt && status.hovering) {
		if (status.pressed) {
			return DRAW_HOVER_PRESSED;
		}

		return DRAW_HOVER;
	} else {
		// Determine if pressed or not.
		bool pressing;
		if (status.press_attempt) {
			pressing = (status.pressing_inside || keep_pressed_outside);
			if (status.pressed) {
				pressing = !pressing;
			}
		} else {
			pressing = status.pressed;
		}

		if (pressing) {
			return DRAW_PRESSED;
		} else {
			return DRAW_NORMAL;
		}
	}
}

void BaseButton::set_toggle_mode(bool p_on) {
	// Make sure to set 'pressed' to false if we are not in toggle mode
	if (!p_on) {
		set_pressed(false);
	}

	toggle_mode = p_on;
	update_configuration_warnings();
}

bool BaseButton::is_toggle_mode() const {
	return toggle_mode;
}

void BaseButton::set_shortcut_in_tooltip(bool p_on) {
	shortcut_in_tooltip = p_on;
}

bool BaseButton::is_shortcut_in_tooltip_enabled() const {
	return shortcut_in_tooltip;
}

void BaseButton::set_action_mode(ActionMode p_mode) {
	action_mode = p_mode;
}

BaseButton::ActionMode BaseButton::get_action_mode() const {
	return action_mode;
}

void BaseButton::set_button_mask(BitField<MouseButtonMask> p_mask) {
	button_mask = p_mask;
}

BitField<MouseButtonMask> BaseButton::get_button_mask() const {
	return button_mask;
}

void BaseButton::set_keep_pressed_outside(bool p_on) {
	keep_pressed_outside = p_on;
}

bool BaseButton::is_keep_pressed_outside() const {
	return keep_pressed_outside;
}

void BaseButton::set_shortcut_feedback(bool p_enable) {
	shortcut_feedback = p_enable;
}

bool BaseButton::is_shortcut_feedback() const {
	return shortcut_feedback;
}

void BaseButton::set_shortcut(const Ref<Shortcut> &p_shortcut) {
	shortcut = p_shortcut;
	set_process_shortcut_input(shortcut.is_valid());
}

Ref<Shortcut> BaseButton::get_shortcut() const {
	return shortcut;
}

void BaseButton::_shortcut_feedback_timeout() {
	in_shortcut_feedback = false;
	queue_redraw();
}

void BaseButton::shortcut_input(const Ref<InputEvent> &p_event) {
	ERR_FAIL_COND(p_event.is_null());

	if (!is_disabled() && p_event->is_pressed() && is_visible_in_tree() && !p_event->is_echo() && shortcut.is_valid() && shortcut->matches_event(p_event)) {
		if (toggle_mode) {
			status.pressed = !status.pressed;

			_unpress_group();
			if (button_group.is_valid()) {
				button_group->emit_signal(SceneStringName(pressed), this);
			}

			_toggled(status.pressed);
			_pressed();

		} else {
			_pressed();
		}
		queue_redraw();
		accept_event();

		if (shortcut_feedback && is_inside_tree()) {
			if (shortcut_feedback_timer == nullptr) {
				shortcut_feedback_timer = memnew(Timer);
				shortcut_feedback_timer->set_one_shot(true);
				add_child(shortcut_feedback_timer);
				shortcut_feedback_timer->set_wait_time(GLOBAL_GET("gui/timers/button_shortcut_feedback_highlight_time"));
				shortcut_feedback_timer->connect("timeout", callable_mp(this, &BaseButton::_shortcut_feedback_timeout));
			}

			in_shortcut_feedback = true;
			shortcut_feedback_timer->start();
		}
	}
}

Control *BaseButton::make_custom_tooltip(const String &p_text) const {
	Control *control = Control::make_custom_tooltip(p_text);
	if (control) {
		return control;
	}
	if (!shortcut_in_tooltip || shortcut.is_null() || !shortcut->has_valid_event()) {
		return nullptr; // Use the default tooltip label.
	}

	String text = atr(shortcut->get_name()) + " (" + shortcut->get_as_text() + ")";
	if (!p_text.is_empty() && shortcut->get_name().nocasecmp_to(p_text) != 0) {
		text += "\n" + atr(p_text);
	}

	// Make a label similar to the default tooltip label.
	// Auto translation is disabled because we already did that manually above.
	//
	// We can't customize the tooltip text by overriding `get_tooltip()`
	// because otherwise user-defined `_make_custom_tooltip()` would receive
	// the translated and annotated text.
	Label *label = memnew(Label(text));
	label->set_auto_translate_mode(AUTO_TRANSLATE_MODE_DISABLED);
	label->set_theme_type_variation(SNAME("TooltipLabel"));

	return label;
}

void BaseButton::set_button_group(const Ref<ButtonGroup> &p_group) {
	if (button_group.is_valid()) {
		button_group->buttons.erase(this);
	}

	button_group = p_group;

	if (button_group.is_valid()) {
		button_group->buttons.insert(this);
	}

	queue_redraw(); //checkbox changes to radio if set a buttongroup
	update_configuration_warnings();
}

Ref<ButtonGroup> BaseButton::get_button_group() const {
	return button_group;
}

bool BaseButton::_was_pressed_by_mouse() const {
	return was_mouse_pressed;
}

PackedStringArray BaseButton::get_configuration_warnings() const {
	PackedStringArray warnings = Control::get_configuration_warnings();

	if (get_button_group().is_valid() && !is_toggle_mode()) {
		warnings.push_back(RTR("ButtonGroup is intended to be used only with buttons that have toggle_mode set to true."));
	}

	return warnings;
}

void BaseButton::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_pressed", "pressed"), &BaseButton::set_pressed);
	ClassDB::bind_method(D_METHOD("is_pressed"), &BaseButton::is_pressed);
	ClassDB::bind_method(D_METHOD("set_pressed_no_signal", "pressed"), &BaseButton::set_pressed_no_signal);
	ClassDB::bind_method(D_METHOD("is_hovered"), &BaseButton::is_hovered);
	ClassDB::bind_method(D_METHOD("set_toggle_mode", "enabled"), &BaseButton::set_toggle_mode);
	ClassDB::bind_method(D_METHOD("is_toggle_mode"), &BaseButton::is_toggle_mode);
	ClassDB::bind_method(D_METHOD("set_shortcut_in_tooltip", "enabled"), &BaseButton::set_shortcut_in_tooltip);
	ClassDB::bind_method(D_METHOD("is_shortcut_in_tooltip_enabled"), &BaseButton::is_shortcut_in_tooltip_enabled);
	ClassDB::bind_method(D_METHOD("set_disabled", "disabled"), &BaseButton::set_disabled);
	ClassDB::bind_method(D_METHOD("is_disabled"), &BaseButton::is_disabled);
	ClassDB::bind_method(D_METHOD("set_action_mode", "mode"), &BaseButton::set_action_mode);
	ClassDB::bind_method(D_METHOD("get_action_mode"), &BaseButton::get_action_mode);
	ClassDB::bind_method(D_METHOD("set_button_mask", "mask"), &BaseButton::set_button_mask);
	ClassDB::bind_method(D_METHOD("get_button_mask"), &BaseButton::get_button_mask);
	ClassDB::bind_method(D_METHOD("get_draw_mode"), &BaseButton::get_draw_mode);
	ClassDB::bind_method(D_METHOD("set_keep_pressed_outside", "enabled"), &BaseButton::set_keep_pressed_outside);
	ClassDB::bind_method(D_METHOD("is_keep_pressed_outside"), &BaseButton::is_keep_pressed_outside);
	ClassDB::bind_method(D_METHOD("set_shortcut_feedback", "enabled"), &BaseButton::set_shortcut_feedback);
	ClassDB::bind_method(D_METHOD("is_shortcut_feedback"), &BaseButton::is_shortcut_feedback);

	ClassDB::bind_method(D_METHOD("set_shortcut", "shortcut"), &BaseButton::set_shortcut);
	ClassDB::bind_method(D_METHOD("get_shortcut"), &BaseButton::get_shortcut);

	ClassDB::bind_method(D_METHOD("set_button_group", "button_group"), &BaseButton::set_button_group);
	ClassDB::bind_method(D_METHOD("get_button_group"), &BaseButton::get_button_group);

	GDVIRTUAL_BIND(_pressed);
	GDVIRTUAL_BIND(_toggled, "toggled_on");

	ADD_SIGNAL(MethodInfo("pressed"));
	ADD_SIGNAL(MethodInfo("button_up"));
	ADD_SIGNAL(MethodInfo("button_down"));
	ADD_SIGNAL(MethodInfo("toggled", PropertyInfo(Variant::BOOL, "toggled_on")));

	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "disabled"), "set_disabled", "is_disabled");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "toggle_mode"), "set_toggle_mode", "is_toggle_mode");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "button_pressed"), "set_pressed", "is_pressed");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "action_mode", PROPERTY_HINT_ENUM, "Button Press,Button Release"), "set_action_mode", "get_action_mode");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "button_mask", PROPERTY_HINT_FLAGS, "Mouse Left, Mouse Right, Mouse Middle"), "set_button_mask", "get_button_mask");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "keep_pressed_outside"), "set_keep_pressed_outside", "is_keep_pressed_outside");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "button_group", PROPERTY_HINT_RESOURCE_TYPE, "ButtonGroup"), "set_button_group", "get_button_group");

	ADD_GROUP("Shortcut", "");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "shortcut", PROPERTY_HINT_RESOURCE_TYPE, "Shortcut"), "set_shortcut", "get_shortcut");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "shortcut_feedback"), "set_shortcut_feedback", "is_shortcut_feedback");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "shortcut_in_tooltip"), "set_shortcut_in_tooltip", "is_shortcut_in_tooltip_enabled");

	BIND_ENUM_CONSTANT(DRAW_NORMAL);
	BIND_ENUM_CONSTANT(DRAW_PRESSED);
	BIND_ENUM_CONSTANT(DRAW_HOVER);
	BIND_ENUM_CONSTANT(DRAW_DISABLED);
	BIND_ENUM_CONSTANT(DRAW_HOVER_PRESSED);

	BIND_ENUM_CONSTANT(ACTION_MODE_BUTTON_PRESS);
	BIND_ENUM_CONSTANT(ACTION_MODE_BUTTON_RELEASE);

	GLOBAL_DEF(PropertyInfo(Variant::FLOAT, "gui/timers/button_shortcut_feedback_highlight_time", PROPERTY_HINT_RANGE, "0.01,10,0.01,suffix:s"), 0.2);
}

BaseButton::BaseButton() {
	set_focus_mode(FOCUS_ALL);
}

BaseButton::~BaseButton() {
	if (button_group.is_valid()) {
		button_group->buttons.erase(this);
	}
}

void ButtonGroup::get_buttons(List<BaseButton *> *r_buttons) {
	for (BaseButton *E : buttons) {
		r_buttons->push_back(E);
	}
}

TypedArray<BaseButton> ButtonGroup::_get_buttons() {
	TypedArray<BaseButton> btns;
	for (const BaseButton *E : buttons) {
		btns.push_back(E);
	}

	return btns;
}

BaseButton *ButtonGroup::get_pressed_button() {
	for (BaseButton *E : buttons) {
		if (E->is_pressed()) {
			return E;
		}
	}

	return nullptr;
}

void ButtonGroup::set_allow_unpress(bool p_enabled) {
	allow_unpress = p_enabled;
}
bool ButtonGroup::is_allow_unpress() {
	return allow_unpress;
}

void ButtonGroup::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_pressed_button"), &ButtonGroup::get_pressed_button);
	ClassDB::bind_method(D_METHOD("get_buttons"), &ButtonGroup::_get_buttons);
	ClassDB::bind_method(D_METHOD("set_allow_unpress", "enabled"), &ButtonGroup::set_allow_unpress);
	ClassDB::bind_method(D_METHOD("is_allow_unpress"), &ButtonGroup::is_allow_unpress);

	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "allow_unpress"), "set_allow_unpress", "is_allow_unpress");

	ADD_SIGNAL(MethodInfo("pressed", PropertyInfo(Variant::OBJECT, "button", PROPERTY_HINT_RESOURCE_TYPE, "BaseButton")));
}

ButtonGroup::ButtonGroup() {
	set_local_to_scene(true);
}

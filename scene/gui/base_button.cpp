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

#include "core/os/keyboard.h"
#include "scene/main/viewport.h"
#include "scene/scene_string_names.h"

void BaseButton::_unpress_group() {
	if (!button_group.is_valid()) {
		return;
	}

	if (toggle_mode) {
		status.pressed = true;
	}

	for (Set<BaseButton *>::Element *E = button_group->buttons.front(); E; E = E->next()) {
		if (E->get() == this) {
			continue;
		}

		E->get()->set_pressed(false);
	}
}

void BaseButton::_gui_input(Ref<InputEvent> p_event) {
	ERR_FAIL_COND(p_event.is_null());

	if (status.disabled) { // no interaction with disabled button
		return;
	}

	Ref<InputEventMouseButton> mouse_button = p_event;
	bool ui_accept = p_event->is_action("ui_accept") && !p_event->is_echo();

	bool button_masked = mouse_button.is_valid() && ((1 << (mouse_button->get_button_index() - 1)) & button_mask) > 0;
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
				update();
			}
		}
	}
}

void BaseButton::_notification(int p_what) {
	if (p_what == NOTIFICATION_MOUSE_ENTER) {
		status.hovering = true;
		update();
	}

	if (p_what == NOTIFICATION_MOUSE_EXIT) {
		status.hovering = false;
		update();
	}
	if (p_what == NOTIFICATION_DRAG_BEGIN || p_what == NOTIFICATION_SCROLL_BEGIN) {
		if (status.press_attempt) {
			status.press_attempt = false;
			update();
		}
	}

	if (p_what == NOTIFICATION_FOCUS_ENTER) {
		update();
	}

	if (p_what == NOTIFICATION_FOCUS_EXIT) {
		if (status.press_attempt) {
			status.press_attempt = false;
			update();
		} else if (status.hovering) {
			update();
		}
	}

	if (p_what == NOTIFICATION_EXIT_TREE || (p_what == NOTIFICATION_VISIBILITY_CHANGED && !is_visible_in_tree())) {
		if (!toggle_mode) {
			status.pressed = false;
		}
		status.hovering = false;
		status.press_attempt = false;
		status.pressing_inside = false;
	}
}

void BaseButton::_pressed() {
	if (get_script_instance()) {
		get_script_instance()->call(SceneStringNames::get_singleton()->_pressed);
	}
	pressed();
	emit_signal("pressed");
}

void BaseButton::_toggled(bool p_pressed) {
	if (get_script_instance()) {
		get_script_instance()->call(SceneStringNames::get_singleton()->_toggled, p_pressed);
	}
	toggled(p_pressed);
	emit_signal("toggled", p_pressed);
}

void BaseButton::on_action_event(Ref<InputEvent> p_event) {
	if (p_event->is_pressed()) {
		status.press_attempt = true;
		status.pressing_inside = true;
		emit_signal("button_down");
	}

	if (status.press_attempt && status.pressing_inside) {
		bool activate = p_event->is_pressed() == (action_mode == ACTION_MODE_BUTTON_PRESS);
		if (action_mode == ACTION_MODE_HYBRID) {
			Ref<InputEventMouseButton> mouse_button = p_event;
			if (mouse_button.is_valid()) {
				activate = !p_event->is_pressed();
			} else {
				activate = p_event->is_pressed();
			}
		}
		if (toggle_mode) {
			if (activate) {
				if (action_mode == ACTION_MODE_BUTTON_PRESS) {
					status.press_attempt = false;
					status.pressing_inside = false;
				}
				status.pressed = !status.pressed;
				_unpress_group();
				if (button_group.is_valid()) {
					button_group->emit_signal("pressed", this);
				}
				_toggled(status.pressed);
				_pressed();
			}
		} else {
			if (activate) {
				_pressed();
			}
		}
	}

	if (!p_event->is_pressed()) {
		Ref<InputEventMouseButton> mouse_button = p_event;
		if (mouse_button.is_valid()) {
			if (!has_point(mouse_button->get_position())) {
				status.hovering = false;
			}
		}
		status.press_attempt = false;
		status.pressing_inside = false;
		emit_signal("button_up");
	}

	update();
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
	update();
	_change_notify("disabled");
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
	_change_notify("pressed");

	if (p_pressed) {
		_unpress_group();
		if (button_group.is_valid()) {
			button_group->emit_signal("pressed", this);
		}
	}
	_toggled(status.pressed);

	update();
}

void BaseButton::set_pressed_no_signal(bool p_pressed) {
	if (!toggle_mode) {
		return;
	}
	if (status.pressed == p_pressed) {
		return;
	}
	status.pressed = p_pressed;

	update();
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
	};

	if (!status.press_attempt && status.hovering) {
		if (status.pressed) {
			return DRAW_HOVER_PRESSED;
		}

		return DRAW_HOVER;
	} else {
		/* determine if pressed or not */

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

	return DRAW_NORMAL;
}

void BaseButton::set_toggle_mode(bool p_on) {
	toggle_mode = p_on;
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

void BaseButton::set_button_mask(int p_mask) {
	button_mask = p_mask;
}

int BaseButton::get_button_mask() const {
	return button_mask;
}

void BaseButton::set_enabled_focus_mode(FocusMode p_mode) {
	enabled_focus_mode = p_mode;
	if (!status.disabled) {
		set_focus_mode(p_mode);
	}
}

Control::FocusMode BaseButton::get_enabled_focus_mode() const {
	return enabled_focus_mode;
}

void BaseButton::set_keep_pressed_outside(bool p_on) {
	keep_pressed_outside = p_on;
}

bool BaseButton::is_keep_pressed_outside() const {
	return keep_pressed_outside;
}

void BaseButton::set_shortcut(const Ref<ShortCut> &p_shortcut) {
	shortcut = p_shortcut;
	set_process_unhandled_input(shortcut.is_valid());
}

Ref<ShortCut> BaseButton::get_shortcut() const {
	return shortcut;
}

void BaseButton::_unhandled_input(Ref<InputEvent> p_event) {
	ERR_FAIL_COND(p_event.is_null());

	if (!is_disabled() && is_visible_in_tree() && !p_event->is_echo() && shortcut.is_valid() && shortcut->is_shortcut(p_event)) {
		if (get_viewport()->get_modal_stack_top() && !get_viewport()->get_modal_stack_top()->is_a_parent_of(this)) {
			return; //ignore because of modal window
		}

		on_action_event(p_event);
	}
}

String BaseButton::get_tooltip(const Point2 &p_pos) const {
	String tooltip = Control::get_tooltip(p_pos);
	if (shortcut_in_tooltip && shortcut.is_valid() && shortcut->is_valid()) {
		String text = shortcut->get_name() + " (" + shortcut->get_as_text() + ")";
		if (shortcut->get_name().nocasecmp_to(tooltip) != 0) {
			text += "\n" + tr(tooltip);
		}
		tooltip = text;
	}
	return tooltip;
}

void BaseButton::set_button_group(const Ref<ButtonGroup> &p_group) {
	if (button_group.is_valid()) {
		button_group->buttons.erase(this);
	}

	button_group = p_group;

	if (button_group.is_valid()) {
		button_group->buttons.insert(this);
	}

	update(); //checkbox changes to radio if set a buttongroup
}

Ref<ButtonGroup> BaseButton::get_button_group() const {
	return button_group;
}

bool BaseButton::_was_pressed_by_mouse() const {
	return was_mouse_pressed;
}

void BaseButton::_bind_methods() {
	ClassDB::bind_method(D_METHOD("_gui_input"), &BaseButton::_gui_input);
	ClassDB::bind_method(D_METHOD("_unhandled_input"), &BaseButton::_unhandled_input);
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
	ClassDB::bind_method(D_METHOD("set_enabled_focus_mode", "mode"), &BaseButton::set_enabled_focus_mode);
	ClassDB::bind_method(D_METHOD("get_enabled_focus_mode"), &BaseButton::get_enabled_focus_mode);
	ClassDB::bind_method(D_METHOD("set_keep_pressed_outside", "enabled"), &BaseButton::set_keep_pressed_outside);
	ClassDB::bind_method(D_METHOD("is_keep_pressed_outside"), &BaseButton::is_keep_pressed_outside);

	ClassDB::bind_method(D_METHOD("set_shortcut", "shortcut"), &BaseButton::set_shortcut);
	ClassDB::bind_method(D_METHOD("get_shortcut"), &BaseButton::get_shortcut);

	ClassDB::bind_method(D_METHOD("set_button_group", "button_group"), &BaseButton::set_button_group);
	ClassDB::bind_method(D_METHOD("get_button_group"), &BaseButton::get_button_group);

	BIND_VMETHOD(MethodInfo("_pressed"));
	BIND_VMETHOD(MethodInfo("_toggled", PropertyInfo(Variant::BOOL, "button_pressed")));

	ADD_SIGNAL(MethodInfo("pressed"));
	ADD_SIGNAL(MethodInfo("button_up"));
	ADD_SIGNAL(MethodInfo("button_down"));
	ADD_SIGNAL(MethodInfo("toggled", PropertyInfo(Variant::BOOL, "button_pressed")));
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "disabled"), "set_disabled", "is_disabled");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "toggle_mode"), "set_toggle_mode", "is_toggle_mode");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "shortcut_in_tooltip"), "set_shortcut_in_tooltip", "is_shortcut_in_tooltip_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "pressed"), "set_pressed", "is_pressed");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "action_mode", PROPERTY_HINT_ENUM, "Button Press, Button Release, Hybrid"), "set_action_mode", "get_action_mode");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "button_mask", PROPERTY_HINT_FLAGS, "Mouse Left, Mouse Right, Mouse Middle"), "set_button_mask", "get_button_mask");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "enabled_focus_mode", PROPERTY_HINT_ENUM, "None,Click,All"), "set_enabled_focus_mode", "get_enabled_focus_mode");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "keep_pressed_outside"), "set_keep_pressed_outside", "is_keep_pressed_outside");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "shortcut", PROPERTY_HINT_RESOURCE_TYPE, "ShortCut"), "set_shortcut", "get_shortcut");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "group", PROPERTY_HINT_RESOURCE_TYPE, "ButtonGroup"), "set_button_group", "get_button_group");

	BIND_ENUM_CONSTANT(DRAW_NORMAL);
	BIND_ENUM_CONSTANT(DRAW_PRESSED);
	BIND_ENUM_CONSTANT(DRAW_HOVER);
	BIND_ENUM_CONSTANT(DRAW_DISABLED);
	BIND_ENUM_CONSTANT(DRAW_HOVER_PRESSED);

	BIND_ENUM_CONSTANT(ACTION_MODE_BUTTON_PRESS);
	BIND_ENUM_CONSTANT(ACTION_MODE_BUTTON_RELEASE);
	BIND_ENUM_CONSTANT(ACTION_MODE_HYBRID);
}

BaseButton::BaseButton() {
	toggle_mode = false;
	shortcut_in_tooltip = true;
	keep_pressed_outside = false;
	was_mouse_pressed = false;
	status.pressed = false;
	status.press_attempt = false;
	status.hovering = false;
	status.pressing_inside = false;
	status.disabled = false;
	set_focus_mode(FOCUS_ALL);
	enabled_focus_mode = FOCUS_ALL;
	action_mode = ACTION_MODE_BUTTON_RELEASE;
	button_mask = BUTTON_MASK_LEFT;
}

BaseButton::~BaseButton() {
	if (button_group.is_valid()) {
		button_group->buttons.erase(this);
	}
}

void ButtonGroup::get_buttons(List<BaseButton *> *r_buttons) {
	for (Set<BaseButton *>::Element *E = buttons.front(); E; E = E->next()) {
		r_buttons->push_back(E->get());
	}
}

Array ButtonGroup::_get_buttons() {
	Array btns;
	for (Set<BaseButton *>::Element *E = buttons.front(); E; E = E->next()) {
		btns.push_back(E->get());
	}

	return btns;
}

BaseButton *ButtonGroup::get_pressed_button() {
	for (Set<BaseButton *>::Element *E = buttons.front(); E; E = E->next()) {
		if (E->get()->is_pressed()) {
			return E->get();
		}
	}

	return nullptr;
}

void ButtonGroup::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_pressed_button"), &ButtonGroup::get_pressed_button);
	ClassDB::bind_method(D_METHOD("get_buttons"), &ButtonGroup::_get_buttons);
	ADD_SIGNAL(MethodInfo("pressed", PropertyInfo(Variant::OBJECT, "button")));
}

ButtonGroup::ButtonGroup() {
	set_local_to_scene(true);
}

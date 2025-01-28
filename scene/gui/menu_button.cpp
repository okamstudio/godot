/**************************************************************************/
/*  menu_button.cpp                                                       */
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

#include "menu_button.h"

#include "scene/main/window.h"

void MenuButton::shortcut_input(const Ref<InputEvent> &p_event) {
	ERR_FAIL_COND(p_event.is_null());

	if (disable_shortcuts) {
		return;
	}

	if (p_event->is_pressed() && !is_disabled() && is_visible_in_tree() && popup->activate_item_by_event(p_event, false)) {
		accept_event();
		return;
	}

	Button::shortcut_input(p_event);
}

void MenuButton::_popup_visibility_changed(bool p_visible) {
	set_pressed(p_visible);

	if (!p_visible) {
		set_process_internal(false);
		return;
	}

	if (switch_on_hover) {
		set_process_internal(true);
	}
}

void MenuButton::pressed() {
	if (popup->is_visible()) {
		popup->hide();
		return;
	}

	show_popup();
}

PopupMenu *MenuButton::get_popup() const {
	return popup;
}

void MenuButton::show_popup() {
	if (!get_viewport()) {
		return;
	}

	emit_signal(SNAME("about_to_popup"));
	Rect2 rect = get_screen_rect();
	rect.position.y += rect.size.height;
	rect.size.height = 0;
	popup->set_size(rect.size);
	if (is_layout_rtl()) {
		rect.position.x += rect.size.width - popup->get_size().width;
	}
	popup->set_position(rect.position);

	// If not triggered by the mouse, start the popup with its first enabled item focused.
	if (!_was_pressed_by_mouse()) {
		for (int i = 0; i < popup->get_item_count(); i++) {
			if (!popup->is_item_disabled(i)) {
				popup->set_focused_item(i);
				break;
			}
		}
	}

	popup->popup();
}

void MenuButton::set_switch_on_hover(bool p_enabled) {
	switch_on_hover = p_enabled;
}

bool MenuButton::is_switch_on_hover() {
	return switch_on_hover;
}

void MenuButton::set_item_count(int p_count) {
	ERR_FAIL_COND(p_count < 0);

	if (popup->get_item_count() == p_count) {
		return;
	}

	popup->set_item_count(p_count);
	notify_property_list_changed();
}

int MenuButton::get_item_count() const {
	return popup->get_item_count();
}

void MenuButton::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_LAYOUT_DIRECTION_CHANGED: {
			popup->set_layout_direction((Window::LayoutDirection)get_layout_direction());
		} break;

		case NOTIFICATION_VISIBILITY_CHANGED: {
			if (!is_visible_in_tree()) {
				popup->hide();
			}
		} break;

		case NOTIFICATION_INTERNAL_PROCESS: {
			MenuButton *menu_btn_other = Object::cast_to<MenuButton>(get_viewport()->gui_get_hovered_control());

			if (menu_btn_other && menu_btn_other != this && menu_btn_other->is_switch_on_hover() && !menu_btn_other->is_disabled() &&
					(get_parent()->is_ancestor_of(menu_btn_other) || menu_btn_other->get_parent()->is_ancestor_of(popup))) {
				popup->hide();

				menu_btn_other->pressed();
				// As the popup wasn't triggered by a mouse click, the item focus needs to be removed manually.
				menu_btn_other->get_popup()->set_focused_item(-1);
			}
		} break;
	}
}

bool MenuButton::_set(const StringName &p_name, const Variant &p_value) {
	const String sname = p_name;
	if (property_helper.is_property_valid(sname)) {
		bool valid;
		popup->set(sname.trim_prefix("popup/"), p_value, &valid);
		return valid;
	}
	return false;
}

bool MenuButton::_get(const StringName &p_name, Variant &r_ret) const {
	const String sname = p_name;
	if (property_helper.is_property_valid(sname)) {
		bool valid;
		r_ret = popup->get(sname.trim_prefix("popup/"), &valid);
		return valid;
	}
	return false;
}

void MenuButton::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_popup"), &MenuButton::get_popup);
	ClassDB::bind_method(D_METHOD("show_popup"), &MenuButton::show_popup);
	ClassDB::bind_method(D_METHOD("set_switch_on_hover", "enable"), &MenuButton::set_switch_on_hover);
	ClassDB::bind_method(D_METHOD("is_switch_on_hover"), &MenuButton::is_switch_on_hover);
	ClassDB::bind_method(D_METHOD("set_disable_shortcuts", "disabled"), &MenuButton::set_disable_shortcuts);

	ClassDB::bind_method(D_METHOD("set_item_count", "count"), &MenuButton::set_item_count);
	ClassDB::bind_method(D_METHOD("get_item_count"), &MenuButton::get_item_count);

	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "switch_on_hover"), "set_switch_on_hover", "is_switch_on_hover");
	ADD_ARRAY_COUNT("Items", "item_count", "set_item_count", "get_item_count", "popup/item_");

	ADD_SIGNAL(MethodInfo("about_to_popup"));

	PopupMenu::Item defaults(true);

	base_property_helper.set_prefix("popup/item_");
	base_property_helper.set_array_length_getter(&MenuButton::get_item_count);
	base_property_helper.register_property(PropertyInfo(Variant::STRING, "text"), defaults.text);
	base_property_helper.register_property(PropertyInfo(Variant::OBJECT, "icon", PROPERTY_HINT_RESOURCE_TYPE, "Texture2D"), defaults.icon);
	base_property_helper.register_property(PropertyInfo(Variant::INT, "checkable", PROPERTY_HINT_ENUM, "No,As Checkbox,As Radio Button"), defaults.checkable_type);
	base_property_helper.register_property(PropertyInfo(Variant::BOOL, "checked"), defaults.checked);
	base_property_helper.register_property(PropertyInfo(Variant::INT, "id", PROPERTY_HINT_RANGE, "0,10,1,or_greater", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_STORE_IF_NULL), defaults.id);
	base_property_helper.register_property(PropertyInfo(Variant::BOOL, "disabled"), defaults.disabled);
	base_property_helper.register_property(PropertyInfo(Variant::BOOL, "separator"), defaults.separator);
	PropertyListHelper::register_base_helper(&base_property_helper);
}

void MenuButton::set_disable_shortcuts(bool p_disabled) {
	disable_shortcuts = p_disabled;
}

#ifdef TOOLS_ENABLED
PackedStringArray MenuButton::get_configuration_warnings() const {
	PackedStringArray warnings = Button::get_configuration_warnings();
	warnings.append_array(popup->get_configuration_warnings());
	return warnings;
}
#endif

MenuButton::MenuButton(const String &p_text) :
		Button(p_text) {
	set_flat(true);
	set_toggle_mode(true);
	set_disable_shortcuts(false);
	set_process_shortcut_input(true);
	set_focus_mode(FOCUS_NONE);
	set_action_mode(ACTION_MODE_BUTTON_PRESS);

	popup = memnew(PopupMenu);
	popup->hide();
	add_child(popup, false, INTERNAL_MODE_FRONT);
	popup->connect("about_to_popup", callable_mp(this, &MenuButton::_popup_visibility_changed).bind(true));
	popup->connect("popup_hide", callable_mp(this, &MenuButton::_popup_visibility_changed).bind(false));

	property_helper.setup_for_instance(base_property_helper, this);
}

MenuButton::~MenuButton() {
}

/**************************************************************************/
/*  input_event.cpp                                                       */
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

#include "input_event.h"

#include "core/input/input_map.h"
#include "core/input/shortcut.h"
#include "core/os/keyboard.h"
#include "core/os/os.h"

const int InputEvent::DEVICE_ID_EMULATION = -1;
const int InputEvent::DEVICE_ID_INTERNAL = -2;

void InputEvent::set_device(int p_device) {
	device = p_device;
	emit_changed();
}

int InputEvent::get_device() const {
	return device;
}

bool InputEvent::is_action(const StringName &p_action, bool p_exact_match) const {
	return InputMap::get_singleton()->event_is_action(Ref<InputEvent>(const_cast<InputEvent *>(this)), p_action, p_exact_match);
}

bool InputEvent::is_action_pressed(const StringName &p_action, bool p_allow_echo, bool p_exact_match) const {
	bool pressed_state;
	bool valid = InputMap::get_singleton()->event_get_action_status(Ref<InputEvent>(const_cast<InputEvent *>(this)), p_action, p_exact_match, &pressed_state, nullptr, nullptr);
	return valid && pressed_state && (p_allow_echo || !is_echo());
}

bool InputEvent::is_action_released(const StringName &p_action, bool p_exact_match) const {
	bool pressed_state;
	bool valid = InputMap::get_singleton()->event_get_action_status(Ref<InputEvent>(const_cast<InputEvent *>(this)), p_action, p_exact_match, &pressed_state, nullptr, nullptr);
	return valid && !pressed_state;
}

float InputEvent::get_action_strength(const StringName &p_action, bool p_exact_match) const {
	float strength;
	bool valid = InputMap::get_singleton()->event_get_action_status(Ref<InputEvent>(const_cast<InputEvent *>(this)), p_action, p_exact_match, nullptr, &strength, nullptr);
	return valid ? strength : 0.0f;
}

float InputEvent::get_action_raw_strength(const StringName &p_action, bool p_exact_match) const {
	float raw_strength;
	bool valid = InputMap::get_singleton()->event_get_action_status(Ref<InputEvent>(const_cast<InputEvent *>(this)), p_action, p_exact_match, nullptr, nullptr, &raw_strength);
	return valid ? raw_strength : 0.0f;
}

bool InputEvent::is_canceled() const {
	return canceled;
}

bool InputEvent::is_pressed() const {
	return pressed && !canceled;
}

bool InputEvent::is_released() const {
	return !pressed && !canceled;
}

bool InputEvent::is_echo() const {
	return false;
}

Ref<InputEvent> InputEvent::xformed_by(const Transform2D &p_xform, const Vector2 &p_local_ofs) const {
	return Ref<InputEvent>(const_cast<InputEvent *>(this));
}

bool InputEvent::action_match(const Ref<InputEvent> &p_event, bool p_exact_match, float p_deadzone, bool *r_pressed, float *r_strength, float *r_raw_strength) const {
	return false;
}

bool InputEvent::is_match(const Ref<InputEvent> &p_event, bool p_exact_match) const {
	return false;
}

bool InputEvent::is_action_type() const {
	return false;
}

void InputEvent::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_device", "device"), &InputEvent::set_device);
	ClassDB::bind_method(D_METHOD("get_device"), &InputEvent::get_device);

	ClassDB::bind_method(D_METHOD("is_action", "action", "exact_match"), &InputEvent::is_action, DEFVAL(false));
	ClassDB::bind_method(D_METHOD("is_action_pressed", "action", "allow_echo", "exact_match"), &InputEvent::is_action_pressed, DEFVAL(false), DEFVAL(false));
	ClassDB::bind_method(D_METHOD("is_action_released", "action", "exact_match"), &InputEvent::is_action_released, DEFVAL(false));
	ClassDB::bind_method(D_METHOD("get_action_strength", "action", "exact_match"), &InputEvent::get_action_strength, DEFVAL(false));

	ClassDB::bind_method(D_METHOD("is_canceled"), &InputEvent::is_canceled);
	ClassDB::bind_method(D_METHOD("is_pressed"), &InputEvent::is_pressed);
	ClassDB::bind_method(D_METHOD("is_released"), &InputEvent::is_released);
	ClassDB::bind_method(D_METHOD("is_echo"), &InputEvent::is_echo);

	ClassDB::bind_method(D_METHOD("as_text"), &InputEvent::as_text);

	ClassDB::bind_method(D_METHOD("is_match", "event", "exact_match"), &InputEvent::is_match, DEFVAL(true));

	ClassDB::bind_method(D_METHOD("is_action_type"), &InputEvent::is_action_type);

	ClassDB::bind_method(D_METHOD("accumulate", "with_event"), &InputEvent::accumulate);

	ClassDB::bind_method(D_METHOD("xformed_by", "xform", "local_ofs"), &InputEvent::xformed_by, DEFVAL(Vector2()));

	ADD_PROPERTY(PropertyInfo(Variant::INT, "device"), "set_device", "get_device");

	BIND_CONSTANT(DEVICE_ID_EMULATION);
}

///////////////////////////////////

void InputEventFromWindow::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_window_id", "id"), &InputEventFromWindow::set_window_id);
	ClassDB::bind_method(D_METHOD("get_window_id"), &InputEventFromWindow::get_window_id);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "window_id"), "set_window_id", "get_window_id");
}

void InputEventFromWindow::set_window_id(int64_t p_id) {
	window_id = p_id;
	emit_changed();
}

int64_t InputEventFromWindow::get_window_id() const {
	return window_id;
}

///////////////////////////////////

void InputEventWithModifiers::set_command_or_control_autoremap(bool p_enabled) {
	command_or_control_autoremap = p_enabled;
	if (command_or_control_autoremap) {
		if (OS::get_singleton()->has_feature("macos") || OS::get_singleton()->has_feature("web_macos") || OS::get_singleton()->has_feature("web_ios")) {
			ctrl_pressed = false;
			meta_pressed = true;
		} else {
			ctrl_pressed = true;
			meta_pressed = false;
		}
	} else {
		ctrl_pressed = false;
		meta_pressed = false;
	}
	emit_changed();
}

bool InputEventWithModifiers::is_command_or_control_autoremap() const {
	return command_or_control_autoremap;
}

bool InputEventWithModifiers::is_command_or_control_pressed() const {
	if (OS::get_singleton()->has_feature("macos") || OS::get_singleton()->has_feature("web_macos") || OS::get_singleton()->has_feature("web_ios")) {
		return meta_pressed;
	} else {
		return ctrl_pressed;
	}
}

void InputEventWithModifiers::set_shift_pressed(bool p_enabled) {
	shift_pressed = p_enabled;
	emit_changed();
}

bool InputEventWithModifiers::is_shift_pressed() const {
	return shift_pressed;
}

void InputEventWithModifiers::set_alt_pressed(bool p_enabled) {
	alt_pressed = p_enabled;
	emit_changed();
}

bool InputEventWithModifiers::is_alt_pressed() const {
	return alt_pressed;
}

void InputEventWithModifiers::set_ctrl_pressed(bool p_enabled) {
	ERR_FAIL_COND_MSG(command_or_control_autoremap, "Command or Control autoremapping is enabled, cannot set Control directly!");
	ctrl_pressed = p_enabled;
	emit_changed();
}

bool InputEventWithModifiers::is_ctrl_pressed() const {
	return ctrl_pressed;
}

void InputEventWithModifiers::set_meta_pressed(bool p_enabled) {
	ERR_FAIL_COND_MSG(command_or_control_autoremap, "Command or Control autoremapping is enabled, cannot set Meta directly!");
	meta_pressed = p_enabled;
	emit_changed();
}

bool InputEventWithModifiers::is_meta_pressed() const {
	return meta_pressed;
}

void InputEventWithModifiers::set_modifiers_from_event(const InputEventWithModifiers *event) {
	set_alt_pressed(event->is_alt_pressed());
	set_shift_pressed(event->is_shift_pressed());
	set_ctrl_pressed(event->is_ctrl_pressed());
	set_meta_pressed(event->is_meta_pressed());
}

BitField<KeyModifierMask> InputEventWithModifiers::get_modifiers_mask() const {
	BitField<KeyModifierMask> mask;
	if (is_ctrl_pressed()) {
		mask.set_flag(KeyModifierMask::CTRL);
	}
	if (is_shift_pressed()) {
		mask.set_flag(KeyModifierMask::SHIFT);
	}
	if (is_alt_pressed()) {
		mask.set_flag(KeyModifierMask::ALT);
	}
	if (is_meta_pressed()) {
		mask.set_flag(KeyModifierMask::META);
	}
	if (is_command_or_control_autoremap()) {
		if (OS::get_singleton()->has_feature("macos") || OS::get_singleton()->has_feature("web_macos") || OS::get_singleton()->has_feature("web_ios")) {
			mask.set_flag(KeyModifierMask::META);
		} else {
			mask.set_flag(KeyModifierMask::CTRL);
		}
	}
	return mask;
}

String InputEventWithModifiers::as_text() const {
	Vector<String> mod_names;

	if (is_ctrl_pressed()) {
		mod_names.push_back(find_keycode_name(Key::CTRL));
	}
	if (is_shift_pressed()) {
		mod_names.push_back(find_keycode_name(Key::SHIFT));
	}
	if (is_alt_pressed()) {
		mod_names.push_back(find_keycode_name(Key::ALT));
	}
	if (is_meta_pressed()) {
		mod_names.push_back(find_keycode_name(Key::META));
	}

	if (!mod_names.is_empty()) {
		return String("+").join(mod_names);
	} else {
		return "";
	}
}

String InputEventWithModifiers::to_string() {
	return as_text();
}

void InputEventWithModifiers::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_command_or_control_autoremap", "enable"), &InputEventWithModifiers::set_command_or_control_autoremap);
	ClassDB::bind_method(D_METHOD("is_command_or_control_autoremap"), &InputEventWithModifiers::is_command_or_control_autoremap);

	ClassDB::bind_method(D_METHOD("is_command_or_control_pressed"), &InputEventWithModifiers::is_command_or_control_pressed);

	ClassDB::bind_method(D_METHOD("set_alt_pressed", "pressed"), &InputEventWithModifiers::set_alt_pressed);
	ClassDB::bind_method(D_METHOD("is_alt_pressed"), &InputEventWithModifiers::is_alt_pressed);

	ClassDB::bind_method(D_METHOD("set_shift_pressed", "pressed"), &InputEventWithModifiers::set_shift_pressed);
	ClassDB::bind_method(D_METHOD("is_shift_pressed"), &InputEventWithModifiers::is_shift_pressed);

	ClassDB::bind_method(D_METHOD("set_ctrl_pressed", "pressed"), &InputEventWithModifiers::set_ctrl_pressed);
	ClassDB::bind_method(D_METHOD("is_ctrl_pressed"), &InputEventWithModifiers::is_ctrl_pressed);

	ClassDB::bind_method(D_METHOD("set_meta_pressed", "pressed"), &InputEventWithModifiers::set_meta_pressed);
	ClassDB::bind_method(D_METHOD("is_meta_pressed"), &InputEventWithModifiers::is_meta_pressed);

	ClassDB::bind_method(D_METHOD("get_modifiers_mask"), &InputEventWithModifiers::get_modifiers_mask);

	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "command_or_control_autoremap"), "set_command_or_control_autoremap", "is_command_or_control_autoremap");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "alt_pressed"), "set_alt_pressed", "is_alt_pressed");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "shift_pressed"), "set_shift_pressed", "is_shift_pressed");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "ctrl_pressed"), "set_ctrl_pressed", "is_ctrl_pressed");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "meta_pressed"), "set_meta_pressed", "is_meta_pressed");
}

void InputEventWithModifiers::_validate_property(PropertyInfo &p_property) const {
	if (command_or_control_autoremap) {
		// Cannot be used with Meta/Command or Control!
		if (p_property.name == "meta_pressed") {
			p_property.usage ^= PROPERTY_USAGE_STORAGE;
		}
		if (p_property.name == "ctrl_pressed") {
			p_property.usage ^= PROPERTY_USAGE_STORAGE;
		}
	} else {
		if (p_property.name == "command_or_control_autoremap") {
			p_property.usage ^= PROPERTY_USAGE_STORAGE;
		}
	}
}

///////////////////////////////////

void InputEventKey::set_pressed(bool p_pressed) {
	pressed = p_pressed;
	emit_changed();
}

void InputEventKey::set_keycode(Key p_keycode) {
	keycode = p_keycode;
	emit_changed();
}

Key InputEventKey::get_keycode() const {
	return keycode;
}

void InputEventKey::set_key_label(Key p_key_label) {
	key_label = p_key_label;
	emit_changed();
}

Key InputEventKey::get_key_label() const {
	return key_label;
}

void InputEventKey::set_physical_keycode(Key p_keycode) {
	physical_keycode = p_keycode;
	emit_changed();
}

Key InputEventKey::get_physical_keycode() const {
	return physical_keycode;
}

void InputEventKey::set_unicode(char32_t p_unicode) {
	unicode = p_unicode;
	emit_changed();
}

char32_t InputEventKey::get_unicode() const {
	return unicode;
}

void InputEventKey::set_location(KeyLocation p_key_location) {
	location = p_key_location;
	emit_changed();
}

KeyLocation InputEventKey::get_location() const {
	return location;
}

void InputEventKey::set_echo(bool p_enable) {
	echo = p_enable;
	emit_changed();
}

bool InputEventKey::is_echo() const {
	return echo;
}

Key InputEventKey::get_keycode_with_modifiers() const {
	return keycode | (int64_t)get_modifiers_mask();
}

Key InputEventKey::get_physical_keycode_with_modifiers() const {
	return physical_keycode | (int64_t)get_modifiers_mask();
}

Key InputEventKey::get_key_label_with_modifiers() const {
	return key_label | get_modifiers_mask();
}

String InputEventKey::as_text_physical_keycode() const {
	String kc;

	if (physical_keycode != Key::NONE) {
		kc = keycode_get_string(physical_keycode);
	} else {
		kc = "(" + RTR("Unset") + ")";
	}

	if (kc.is_empty()) {
		return kc;
	}

	String mods_text = InputEventWithModifiers::as_text();
	return mods_text.is_empty() ? kc : mods_text + "+" + kc;
}

String InputEventKey::as_text_keycode() const {
	String kc;

	if (keycode != Key::NONE) {
		kc = keycode_get_string(keycode);
	} else {
		kc = "(" + RTR("Unset") + ")";
	}

	if (kc.is_empty()) {
		return kc;
	}

	String mods_text = InputEventWithModifiers::as_text();
	return mods_text.is_empty() ? kc : mods_text + "+" + kc;
}

String InputEventKey::as_text_key_label() const {
	String kc;

	if (key_label != Key::NONE) {
		kc = keycode_get_string(key_label);
	} else {
		kc = "(" + RTR("Unset") + ")";
	}

	if (kc.is_empty()) {
		return kc;
	}

	String mods_text = InputEventWithModifiers::as_text();
	return mods_text.is_empty() ? kc : mods_text + "+" + kc;
}

String InputEventKey::as_text_location() const {
	String loc;

	switch (location) {
		case KeyLocation::LEFT:
			loc = "left";
			break;
		case KeyLocation::RIGHT:
			loc = "right";
			break;
		default:
			break;
	}

	return loc;
}

String InputEventKey::as_text() const {
	String kc;

	if (keycode == Key::NONE && physical_keycode == Key::NONE && key_label != Key::NONE) {
		kc = keycode_get_string(key_label) + " (Unicode)";
	} else if (keycode != Key::NONE) {
		kc = keycode_get_string(keycode);
	} else if (physical_keycode != Key::NONE) {
		kc = keycode_get_string(physical_keycode) + " (" + RTR("Physical") + ")";
	} else {
		kc = "(" + RTR("Unset") + ")";
	}

	if (kc.is_empty()) {
		return kc;
	}

	String mods_text = InputEventWithModifiers::as_text();
	return mods_text.is_empty() ? kc : mods_text + "+" + kc;
}

String InputEventKey::to_string() {
	String p = is_pressed() ? "true" : "false";
	String e = is_echo() ? "true" : "false";

	String kc = "";
	String physical = "false";

	String loc = as_text_location();
	if (loc.is_empty()) {
		loc = "unspecified";
	}

	if (keycode == Key::NONE && physical_keycode == Key::NONE && unicode != 0) {
		kc = "U+" + String::num_uint64(unicode, 16) + " (" + String::chr(unicode) + ")";
	} else if (keycode != Key::NONE) {
		kc = itos((int64_t)keycode) + " (" + keycode_get_string(keycode) + ")";
	} else if (physical_keycode != Key::NONE) {
		kc = itos((int64_t)physical_keycode) + " (" + keycode_get_string(physical_keycode) + ")";
		physical = "true";
	} else {
		kc = "(" + RTR("Unset") + ")";
	}

	String mods = InputEventWithModifiers::as_text();
	mods = mods.is_empty() ? "none" : mods;

	return vformat("InputEventKey: keycode=%s, mods=%s, physical=%s, location=%s, pressed=%s, echo=%s", kc, mods, physical, loc, p, e);
}

Ref<InputEventKey> InputEventKey::create_reference(Key p_keycode, bool p_physical) {
	Ref<InputEventKey> ie;
	ie.instantiate();
	if (p_physical) {
		ie->set_physical_keycode(p_keycode & KeyModifierMask::CODE_MASK);
	} else {
		ie->set_keycode(p_keycode & KeyModifierMask::CODE_MASK);
	}

	char32_t ch = char32_t(p_keycode & KeyModifierMask::CODE_MASK);
	if (ch < 0xd800 || (ch > 0xdfff && ch <= 0x10ffff)) {
		ie->set_unicode(ch);
	}

	if ((p_keycode & KeyModifierMask::SHIFT) != Key::NONE) {
		ie->set_shift_pressed(true);
	}
	if ((p_keycode & KeyModifierMask::ALT) != Key::NONE) {
		ie->set_alt_pressed(true);
	}
	if ((p_keycode & KeyModifierMask::CMD_OR_CTRL) != Key::NONE) {
		ie->set_command_or_control_autoremap(true);
		if ((p_keycode & KeyModifierMask::CTRL) != Key::NONE || (p_keycode & KeyModifierMask::META) != Key::NONE) {
			WARN_PRINT("Invalid Key Modifiers: Command or Control autoremapping is enabled, Meta and Control values are ignored!");
		}
	} else {
		if ((p_keycode & KeyModifierMask::CTRL) != Key::NONE) {
			ie->set_ctrl_pressed(true);
		}
		if ((p_keycode & KeyModifierMask::META) != Key::NONE) {
			ie->set_meta_pressed(true);
		}
	}

	return ie;
}

bool InputEventKey::action_match(const Ref<InputEvent> &p_event, bool p_exact_match, float p_deadzone, bool *r_pressed, float *r_strength, float *r_raw_strength) const {
	Ref<InputEventKey> key = p_event;
	if (key.is_null()) {
		return false;
	}

	bool match;
	if (keycode == Key::NONE && physical_keycode == Key::NONE && key_label != Key::NONE) {
		match = key_label == key->key_label;
	} else if (keycode != Key::NONE) {
		match = keycode == key->keycode;
	} else if (physical_keycode != Key::NONE) {
		match = physical_keycode == key->physical_keycode;
		if (location != KeyLocation::UNSPECIFIED) {
			match &= location == key->location;
		}
	} else {
		match = false;
	}

	Key action_mask = (Key)(int64_t)get_modifiers_mask();
	Key key_mask = (Key)(int64_t)key->get_modifiers_mask();
	if (key->is_pressed()) {
		match &= (action_mask & key_mask) == action_mask;
	}
	if (p_exact_match) {
		match &= action_mask == key_mask;
	}
	if (match) {
		bool key_pressed = key->is_pressed();
		if (r_pressed != nullptr) {
			*r_pressed = key_pressed;
		}
		float strength = key_pressed ? 1.0f : 0.0f;
		if (r_strength != nullptr) {
			*r_strength = strength;
		}
		if (r_raw_strength != nullptr) {
			*r_raw_strength = strength;
		}
	}
	return match;
}

bool InputEventKey::is_match(const Ref<InputEvent> &p_event, bool p_exact_match) const {
	Ref<InputEventKey> key = p_event;
	if (key.is_null()) {
		return false;
	}

	if (keycode == Key::NONE && physical_keycode == Key::NONE && key_label != Key::NONE) {
		return (key_label == key->key_label) &&
				(!p_exact_match || get_modifiers_mask() == key->get_modifiers_mask());
	} else if (keycode != Key::NONE) {
		return (keycode == key->keycode) &&
				(!p_exact_match || get_modifiers_mask() == key->get_modifiers_mask());
	} else if (physical_keycode != Key::NONE) {
		if (location != KeyLocation::UNSPECIFIED && location != key->location) {
			return false;
		}
		return (physical_keycode == key->physical_keycode) &&
				(!p_exact_match || get_modifiers_mask() == key->get_modifiers_mask());
	} else {
		return false;
	}
}

void InputEventKey::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_pressed", "pressed"), &InputEventKey::set_pressed);

	ClassDB::bind_method(D_METHOD("set_keycode", "keycode"), &InputEventKey::set_keycode);
	ClassDB::bind_method(D_METHOD("get_keycode"), &InputEventKey::get_keycode);

	ClassDB::bind_method(D_METHOD("set_physical_keycode", "physical_keycode"), &InputEventKey::set_physical_keycode);
	ClassDB::bind_method(D_METHOD("get_physical_keycode"), &InputEventKey::get_physical_keycode);

	ClassDB::bind_method(D_METHOD("set_key_label", "key_label"), &InputEventKey::set_key_label);
	ClassDB::bind_method(D_METHOD("get_key_label"), &InputEventKey::get_key_label);

	ClassDB::bind_method(D_METHOD("set_unicode", "unicode"), &InputEventKey::set_unicode);
	ClassDB::bind_method(D_METHOD("get_unicode"), &InputEventKey::get_unicode);

	ClassDB::bind_method(D_METHOD("set_location", "location"), &InputEventKey::set_location);
	ClassDB::bind_method(D_METHOD("get_location"), &InputEventKey::get_location);

	ClassDB::bind_method(D_METHOD("set_echo", "echo"), &InputEventKey::set_echo);

	ClassDB::bind_method(D_METHOD("get_keycode_with_modifiers"), &InputEventKey::get_keycode_with_modifiers);
	ClassDB::bind_method(D_METHOD("get_physical_keycode_with_modifiers"), &InputEventKey::get_physical_keycode_with_modifiers);
	ClassDB::bind_method(D_METHOD("get_key_label_with_modifiers"), &InputEventKey::get_key_label_with_modifiers);

	ClassDB::bind_method(D_METHOD("as_text_keycode"), &InputEventKey::as_text_keycode);
	ClassDB::bind_method(D_METHOD("as_text_physical_keycode"), &InputEventKey::as_text_physical_keycode);
	ClassDB::bind_method(D_METHOD("as_text_key_label"), &InputEventKey::as_text_key_label);
	ClassDB::bind_method(D_METHOD("as_text_location"), &InputEventKey::as_text_location);

	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "pressed"), "set_pressed", "is_pressed");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "keycode"), "set_keycode", "get_keycode");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "physical_keycode"), "set_physical_keycode", "get_physical_keycode");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "key_label"), "set_key_label", "get_key_label");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "unicode"), "set_unicode", "get_unicode");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "location", PROPERTY_HINT_ENUM, "Unspecified,Left,Right"), "set_location", "get_location");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "echo"), "set_echo", "is_echo");
}

///////////////////////////////////

void InputEventMouse::set_button_mask(BitField<MouseButtonMask> p_mask) {
	button_mask = p_mask;
	emit_changed();
}

BitField<MouseButtonMask> InputEventMouse::get_button_mask() const {
	return button_mask;
}

void InputEventMouse::set_position(const Vector2 &p_pos) {
	pos = p_pos;
}

Vector2 InputEventMouse::get_position() const {
	return pos;
}

void InputEventMouse::set_global_position(const Vector2 &p_global_pos) {
	global_pos = p_global_pos;
}

Vector2 InputEventMouse::get_global_position() const {
	return global_pos;
}

void InputEventMouse::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_button_mask", "button_mask"), &InputEventMouse::set_button_mask);
	ClassDB::bind_method(D_METHOD("get_button_mask"), &InputEventMouse::get_button_mask);

	ClassDB::bind_method(D_METHOD("set_position", "position"), &InputEventMouse::set_position);
	ClassDB::bind_method(D_METHOD("get_position"), &InputEventMouse::get_position);

	ClassDB::bind_method(D_METHOD("set_global_position", "global_position"), &InputEventMouse::set_global_position);
	ClassDB::bind_method(D_METHOD("get_global_position"), &InputEventMouse::get_global_position);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "button_mask"), "set_button_mask", "get_button_mask");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "position", PROPERTY_HINT_NONE, "suffix:px"), "set_position", "get_position");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "global_position", PROPERTY_HINT_NONE, "suffix:px"), "set_global_position", "get_global_position");
}

///////////////////////////////////

void InputEventMouseButton::set_factor(float p_factor) {
	factor = p_factor;
}

float InputEventMouseButton::get_factor() const {
	return factor;
}

void InputEventMouseButton::set_button_index(MouseButton p_index) {
	button_index = p_index;
	emit_changed();
}

MouseButton InputEventMouseButton::get_button_index() const {
	return button_index;
}

void InputEventMouseButton::set_pressed(bool p_pressed) {
	pressed = p_pressed;
}

void InputEventMouseButton::set_canceled(bool p_canceled) {
	canceled = p_canceled;
}

void InputEventMouseButton::set_double_click(bool p_double_click) {
	double_click = p_double_click;
}

bool InputEventMouseButton::is_double_click() const {
	return double_click;
}

Ref<InputEvent> InputEventMouseButton::xformed_by(const Transform2D &p_xform, const Vector2 &p_local_ofs) const {
	Vector2 g = get_global_position();
	Vector2 l = p_xform.xform(get_position() + p_local_ofs);

	Ref<InputEventMouseButton> mb;
	mb.instantiate();

	mb->set_device(get_device());
	mb->set_window_id(get_window_id());
	mb->set_modifiers_from_event(this);

	mb->set_position(l);
	mb->set_global_position(g);

	mb->set_button_mask(get_button_mask());
	mb->set_pressed(pressed);
	mb->set_canceled(canceled);
	mb->set_double_click(double_click);
	mb->set_factor(factor);
	mb->set_button_index(button_index);

	mb->merge_meta_from(this);

	return mb;
}

bool InputEventMouseButton::action_match(const Ref<InputEvent> &p_event, bool p_exact_match, float p_deadzone, bool *r_pressed, float *r_strength, float *r_raw_strength) const {
	Ref<InputEventMouseButton> mb = p_event;
	if (mb.is_null()) {
		return false;
	}

	bool match = button_index == mb->button_index;
	Key action_modifiers_mask = (Key)(int64_t)get_modifiers_mask();
	Key button_modifiers_mask = (Key)(int64_t)mb->get_modifiers_mask();
	if (mb->is_pressed()) {
		match &= (action_modifiers_mask & button_modifiers_mask) == action_modifiers_mask;
	}
	if (p_exact_match) {
		match &= action_modifiers_mask == button_modifiers_mask;
	}
	if (match) {
		bool mb_pressed = mb->is_pressed();
		if (r_pressed != nullptr) {
			*r_pressed = mb_pressed;
		}
		float strength = mb_pressed ? 1.0f : 0.0f;
		if (r_strength != nullptr) {
			*r_strength = strength;
		}
		if (r_raw_strength != nullptr) {
			*r_raw_strength = strength;
		}
	}

	return match;
}

bool InputEventMouseButton::is_match(const Ref<InputEvent> &p_event, bool p_exact_match) const {
	Ref<InputEventMouseButton> mb = p_event;
	if (mb.is_null()) {
		return false;
	}

	return button_index == mb->button_index &&
			(!p_exact_match || get_modifiers_mask() == mb->get_modifiers_mask());
}

static const char *_mouse_button_descriptions[9] = {
	TTRC("Left Mouse Button"),
	TTRC("Right Mouse Button"),
	TTRC("Middle Mouse Button"),
	TTRC("Mouse Wheel Up"),
	TTRC("Mouse Wheel Down"),
	TTRC("Mouse Wheel Left"),
	TTRC("Mouse Wheel Right"),
	TTRC("Mouse Thumb Button 1"),
	TTRC("Mouse Thumb Button 2")
};

String InputEventMouseButton::as_text() const {
	// Modifiers
	String mods_text = InputEventWithModifiers::as_text();
	String full_string = mods_text.is_empty() ? "" : mods_text + "+";

	// Button
	MouseButton idx = get_button_index();
	switch (idx) {
		case MouseButton::LEFT:
		case MouseButton::RIGHT:
		case MouseButton::MIDDLE:
		case MouseButton::WHEEL_UP:
		case MouseButton::WHEEL_DOWN:
		case MouseButton::WHEEL_LEFT:
		case MouseButton::WHEEL_RIGHT:
		case MouseButton::MB_XBUTTON1:
		case MouseButton::MB_XBUTTON2:
			full_string += RTR(_mouse_button_descriptions[(size_t)idx - 1]); // button index starts from 1, array index starts from 0, so subtract 1
			break;
		default:
			full_string += RTR("Button") + " #" + itos((int64_t)idx);
			break;
	}

	// Double Click
	if (double_click) {
		full_string += " (" + RTR("Double Click") + ")";
	}

	return full_string;
}

String InputEventMouseButton::to_string() {
	String p = is_pressed() ? "true" : "false";
	String canceled_state = is_canceled() ? "true" : "false";
	String d = double_click ? "true" : "false";

	MouseButton idx = get_button_index();
	String button_string = itos((int64_t)idx);

	switch (idx) {
		case MouseButton::LEFT:
		case MouseButton::RIGHT:
		case MouseButton::MIDDLE:
		case MouseButton::WHEEL_UP:
		case MouseButton::WHEEL_DOWN:
		case MouseButton::WHEEL_LEFT:
		case MouseButton::WHEEL_RIGHT:
		case MouseButton::MB_XBUTTON1:
		case MouseButton::MB_XBUTTON2:
			button_string += vformat(" (%s)", TTRGET(_mouse_button_descriptions[(size_t)idx - 1])); // button index starts from 1, array index starts from 0, so subtract 1
			break;
		default:
			break;
	}

	String mods = InputEventWithModifiers::as_text();
	mods = mods.is_empty() ? "none" : mods;

	// Work around the fact vformat can only take 5 substitutions but 6 need to be passed.
	String index_and_mods = vformat("button_index=%s, mods=%s", button_index, mods);
	return vformat("InputEventMouseButton: %s, pressed=%s, canceled=%s, position=(%s), button_mask=%d, double_click=%s", index_and_mods, p, canceled_state, String(get_position()), get_button_mask(), d);
}

void InputEventMouseButton::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_factor", "factor"), &InputEventMouseButton::set_factor);
	ClassDB::bind_method(D_METHOD("get_factor"), &InputEventMouseButton::get_factor);

	ClassDB::bind_method(D_METHOD("set_button_index", "button_index"), &InputEventMouseButton::set_button_index);
	ClassDB::bind_method(D_METHOD("get_button_index"), &InputEventMouseButton::get_button_index);

	ClassDB::bind_method(D_METHOD("set_pressed", "pressed"), &InputEventMouseButton::set_pressed);
	ClassDB::bind_method(D_METHOD("set_canceled", "canceled"), &InputEventMouseButton::set_canceled);

	ClassDB::bind_method(D_METHOD("set_double_click", "double_click"), &InputEventMouseButton::set_double_click);
	ClassDB::bind_method(D_METHOD("is_double_click"), &InputEventMouseButton::is_double_click);

	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "factor"), "set_factor", "get_factor");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "button_index"), "set_button_index", "get_button_index");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "canceled"), "set_canceled", "is_canceled");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "pressed"), "set_pressed", "is_pressed");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "double_click"), "set_double_click", "is_double_click");
}

///////////////////////////////////

void InputEventMouseMotion::set_tilt(const Vector2 &p_tilt) {
	tilt = p_tilt;
}

Vector2 InputEventMouseMotion::get_tilt() const {
	return tilt;
}

void InputEventMouseMotion::set_pressure(float p_pressure) {
	pressure = p_pressure;
}

float InputEventMouseMotion::get_pressure() const {
	return pressure;
}

void InputEventMouseMotion::set_pen_inverted(bool p_inverted) {
	pen_inverted = p_inverted;
}

bool InputEventMouseMotion::get_pen_inverted() const {
	return pen_inverted;
}

void InputEventMouseMotion::set_relative(const Vector2 &p_relative) {
	relative = p_relative;
}

Vector2 InputEventMouseMotion::get_relative() const {
	return relative;
}

void InputEventMouseMotion::set_relative_screen_position(const Vector2 &p_relative) {
	screen_relative = p_relative;
}

Vector2 InputEventMouseMotion::get_relative_screen_position() const {
	return screen_relative;
}

void InputEventMouseMotion::set_velocity(const Vector2 &p_velocity) {
	velocity = p_velocity;
}

Vector2 InputEventMouseMotion::get_velocity() const {
	return velocity;
}

void InputEventMouseMotion::set_screen_velocity(const Vector2 &p_velocity) {
	screen_velocity = p_velocity;
}

Vector2 InputEventMouseMotion::get_screen_velocity() const {
	return screen_velocity;
}

Ref<InputEvent> InputEventMouseMotion::xformed_by(const Transform2D &p_xform, const Vector2 &p_local_ofs) const {
	Ref<InputEventMouseMotion> mm;
	mm.instantiate();

	mm->set_device(get_device());
	mm->set_window_id(get_window_id());

	mm->set_modifiers_from_event(this);

	mm->set_position(p_xform.xform(get_position() + p_local_ofs));
	mm->set_pressure(get_pressure());
	mm->set_pen_inverted(get_pen_inverted());
	mm->set_tilt(get_tilt());
	mm->set_global_position(get_global_position());

	mm->set_button_mask(get_button_mask());
	mm->set_relative(p_xform.basis_xform(get_relative()));
	mm->set_relative_screen_position(get_relative_screen_position());
	mm->set_velocity(p_xform.basis_xform(get_velocity()));
	mm->set_screen_velocity(get_screen_velocity());

	mm->merge_meta_from(this);

	return mm;
}

String InputEventMouseMotion::as_text() const {
	return vformat(RTR("Mouse motion at position (%s) with velocity (%s)"), String(get_position()), String(get_velocity()));
}

String InputEventMouseMotion::to_string() {
	BitField<MouseButtonMask> mouse_button_mask = get_button_mask();
	String button_mask_string = itos((int64_t)mouse_button_mask);

	if (mouse_button_mask.has_flag(MouseButtonMask::LEFT)) {
		button_mask_string += vformat(" (%s)", TTRGET(_mouse_button_descriptions[(size_t)MouseButton::LEFT - 1]));
	}
	if (mouse_button_mask.has_flag(MouseButtonMask::MIDDLE)) {
		button_mask_string += vformat(" (%s)", TTRGET(_mouse_button_descriptions[(size_t)MouseButton::MIDDLE - 1]));
	}
	if (mouse_button_mask.has_flag(MouseButtonMask::RIGHT)) {
		button_mask_string += vformat(" (%s)", TTRGET(_mouse_button_descriptions[(size_t)MouseButton::RIGHT - 1]));
	}
	if (mouse_button_mask.has_flag(MouseButtonMask::MB_XBUTTON1)) {
		button_mask_string += vformat(" (%s)", TTRGET(_mouse_button_descriptions[(size_t)MouseButton::MB_XBUTTON1 - 1]));
	}
	if (mouse_button_mask.has_flag(MouseButtonMask::MB_XBUTTON2)) {
		button_mask_string += vformat(" (%s)", TTRGET(_mouse_button_descriptions[(size_t)MouseButton::MB_XBUTTON2 - 1]));
	}

	// Work around the fact vformat can only take 5 substitutions but 7 need to be passed.
	String mask_and_position_and_relative = vformat("button_mask=%s, position=(%s), relative=(%s)", button_mask_string, String(get_position()), String(get_relative()));
	return vformat("InputEventMouseMotion: %s, velocity=(%s), pressure=%.2f, tilt=(%s), pen_inverted=(%s)", mask_and_position_and_relative, String(get_velocity()), get_pressure(), String(get_tilt()), get_pen_inverted());
}

bool InputEventMouseMotion::accumulate(const Ref<InputEvent> &p_event) {
	Ref<InputEventMouseMotion> motion = p_event;
	if (motion.is_null()) {
		return false;
	}

	if (get_window_id() != motion->get_window_id()) {
		return false;
	}

	if (is_canceled() != motion->is_canceled()) {
		return false;
	}

	if (is_pressed() != motion->is_pressed()) {
		return false;
	}

	if (get_button_mask() != motion->get_button_mask()) {
		return false;
	}

	if (is_shift_pressed() != motion->is_shift_pressed()) {
		return false;
	}

	if (is_ctrl_pressed() != motion->is_ctrl_pressed()) {
		return false;
	}

	if (is_alt_pressed() != motion->is_alt_pressed()) {
		return false;
	}

	if (is_meta_pressed() != motion->is_meta_pressed()) {
		return false;
	}

	set_position(motion->get_position());
	set_global_position(motion->get_global_position());
	set_velocity(motion->get_velocity());
	set_screen_velocity(motion->get_screen_velocity());
	relative += motion->get_relative();
	screen_relative += motion->get_relative_screen_position();

	return true;
}

void InputEventMouseMotion::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_tilt", "tilt"), &InputEventMouseMotion::set_tilt);
	ClassDB::bind_method(D_METHOD("get_tilt"), &InputEventMouseMotion::get_tilt);

	ClassDB::bind_method(D_METHOD("set_pressure", "pressure"), &InputEventMouseMotion::set_pressure);
	ClassDB::bind_method(D_METHOD("get_pressure"), &InputEventMouseMotion::get_pressure);

	ClassDB::bind_method(D_METHOD("set_pen_inverted", "pen_inverted"), &InputEventMouseMotion::set_pen_inverted);
	ClassDB::bind_method(D_METHOD("get_pen_inverted"), &InputEventMouseMotion::get_pen_inverted);

	ClassDB::bind_method(D_METHOD("set_relative", "relative"), &InputEventMouseMotion::set_relative);
	ClassDB::bind_method(D_METHOD("get_relative"), &InputEventMouseMotion::get_relative);

	ClassDB::bind_method(D_METHOD("set_screen_relative", "relative"), &InputEventMouseMotion::set_relative_screen_position);
	ClassDB::bind_method(D_METHOD("get_screen_relative"), &InputEventMouseMotion::get_relative_screen_position);

	ClassDB::bind_method(D_METHOD("set_velocity", "velocity"), &InputEventMouseMotion::set_velocity);
	ClassDB::bind_method(D_METHOD("get_velocity"), &InputEventMouseMotion::get_velocity);

	ClassDB::bind_method(D_METHOD("set_screen_velocity", "velocity"), &InputEventMouseMotion::set_screen_velocity);
	ClassDB::bind_method(D_METHOD("get_screen_velocity"), &InputEventMouseMotion::get_screen_velocity);

	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "tilt"), "set_tilt", "get_tilt");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "pressure"), "set_pressure", "get_pressure");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "pen_inverted"), "set_pen_inverted", "get_pen_inverted");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "relative", PROPERTY_HINT_NONE, "suffix:px"), "set_relative", "get_relative");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "screen_relative", PROPERTY_HINT_NONE, "suffix:px"), "set_screen_relative", "get_screen_relative");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "velocity", PROPERTY_HINT_NONE, "suffix:px/s"), "set_velocity", "get_velocity");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "screen_velocity", PROPERTY_HINT_NONE, "suffix:px/s"), "set_screen_velocity", "get_screen_velocity");
}

///////////////////////////////////

void InputEventJoypadMotion::set_axis(JoyAxis p_axis) {
	ERR_FAIL_COND(p_axis < JoyAxis::INVALID || p_axis > JoyAxis::MAX);

	axis = p_axis;
	emit_changed();
}

JoyAxis InputEventJoypadMotion::get_axis() const {
	return axis;
}

void InputEventJoypadMotion::set_axis_value(float p_value) {
	axis_value = p_value;
	pressed = Math::abs(axis_value) >= InputMap::DEFAULT_DEADZONE;
	emit_changed();
}

float InputEventJoypadMotion::get_axis_value() const {
	return axis_value;
}

bool InputEventJoypadMotion::action_match(const Ref<InputEvent> &p_event, bool p_exact_match, float p_deadzone, bool *r_pressed, float *r_strength, float *r_raw_strength) const {
	Ref<InputEventJoypadMotion> jm = p_event;
	if (jm.is_null()) {
		return false;
	}

	// Matches even if not in the same direction, but returns a "not pressed" event.
	bool match = axis == jm->axis;
	if (p_exact_match) {
		match &= (axis_value < 0) == (jm->axis_value < 0);
	}
	if (match) {
		float jm_abs_axis_value = Math::abs(jm->get_axis_value());
		bool same_direction = (((axis_value < 0) == (jm->axis_value < 0)) || jm->axis_value == 0);
		bool pressed_state = same_direction && jm_abs_axis_value >= p_deadzone;
		if (r_pressed != nullptr) {
			*r_pressed = pressed_state;
		}
		if (r_strength != nullptr) {
			if (pressed_state) {
				if (p_deadzone == 1.0f) {
					*r_strength = 1.0f;
				} else {
					*r_strength = CLAMP(Math::inverse_lerp(p_deadzone, 1.0f, jm_abs_axis_value), 0.0f, 1.0f);
				}
			} else {
				*r_strength = 0.0f;
			}
		}
		if (r_raw_strength != nullptr) {
			if (same_direction) { // NOT pressed, because we want to ignore the deadzone.
				*r_raw_strength = jm_abs_axis_value;
			} else {
				*r_raw_strength = 0.0f;
			}
		}
	}
	return match;
}

bool InputEventJoypadMotion::is_match(const Ref<InputEvent> &p_event, bool p_exact_match) const {
	Ref<InputEventJoypadMotion> jm = p_event;
	if (jm.is_null()) {
		return false;
	}

	return axis == jm->axis &&
			(!p_exact_match || ((axis_value < 0) == (jm->axis_value < 0)));
}

static const char *_joy_axis_descriptions[(size_t)JoyAxis::MAX] = {
	TTRC("Left Stick X-Axis, Joystick 0 X-Axis"),
	TTRC("Left Stick Y-Axis, Joystick 0 Y-Axis"),
	TTRC("Right Stick X-Axis, Joystick 1 X-Axis"),
	TTRC("Right Stick Y-Axis, Joystick 1 Y-Axis"),
	TTRC("Joystick 2 X-Axis, Left Trigger, Sony L2, Xbox LT"),
	TTRC("Joystick 2 Y-Axis, Right Trigger, Sony R2, Xbox RT"),
	TTRC("Joystick 3 X-Axis"),
	TTRC("Joystick 3 Y-Axis"),
	TTRC("Joystick 4 X-Axis"),
	TTRC("Joystick 4 Y-Axis"),
};

String InputEventJoypadMotion::as_text() const {
	String desc = axis < JoyAxis::MAX ? TTRGET(_joy_axis_descriptions[(size_t)axis]) : RTR("Unknown Joypad Axis");

	return vformat(RTR("Joypad Motion on Axis %d (%s) with Value %.2f"), axis, desc, axis_value);
}

String InputEventJoypadMotion::to_string() {
	return vformat("InputEventJoypadMotion: axis=%d, axis_value=%.2f", axis, axis_value);
}

Ref<InputEventJoypadMotion> InputEventJoypadMotion::create_reference(JoyAxis p_axis, float p_value) {
	Ref<InputEventJoypadMotion> ie;
	ie.instantiate();
	ie->set_axis(p_axis);
	ie->set_axis_value(p_value);

	return ie;
}

void InputEventJoypadMotion::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_axis", "axis"), &InputEventJoypadMotion::set_axis);
	ClassDB::bind_method(D_METHOD("get_axis"), &InputEventJoypadMotion::get_axis);

	ClassDB::bind_method(D_METHOD("set_axis_value", "axis_value"), &InputEventJoypadMotion::set_axis_value);
	ClassDB::bind_method(D_METHOD("get_axis_value"), &InputEventJoypadMotion::get_axis_value);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "axis"), "set_axis", "get_axis");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "axis_value"), "set_axis_value", "get_axis_value");
}

///////////////////////////////////

void InputEventJoypadButton::set_button_index(JoyButton p_index) {
	button_index = p_index;
	emit_changed();
}

JoyButton InputEventJoypadButton::get_button_index() const {
	return button_index;
}

void InputEventJoypadButton::set_pressed(bool p_pressed) {
	pressed = p_pressed;
}

void InputEventJoypadButton::set_pressure(float p_pressure) {
	pressure = p_pressure;
}

float InputEventJoypadButton::get_pressure() const {
	return pressure;
}

bool InputEventJoypadButton::action_match(const Ref<InputEvent> &p_event, bool p_exact_match, float p_deadzone, bool *r_pressed, float *r_strength, float *r_raw_strength) const {
	Ref<InputEventJoypadButton> jb = p_event;
	if (jb.is_null()) {
		return false;
	}

	bool match = button_index == jb->button_index;
	if (match) {
		bool jb_pressed = jb->is_pressed();
		if (r_pressed != nullptr) {
			*r_pressed = jb_pressed;
		}
		float strength = jb_pressed ? 1.0f : 0.0f;
		if (r_strength != nullptr) {
			*r_strength = strength;
		}
		if (r_raw_strength != nullptr) {
			*r_raw_strength = strength;
		}
	}

	return match;
}

bool InputEventJoypadButton::is_match(const Ref<InputEvent> &p_event, bool p_exact_match) const {
	Ref<InputEventJoypadButton> button = p_event;
	if (button.is_null()) {
		return false;
	}

	return button_index == button->button_index;
}

static const char *_joy_button_descriptions[(size_t)JoyButton::SDL_MAX] = {
	TTRC("Bottom Action, Sony Cross, Xbox A, Nintendo B"),
	TTRC("Right Action, Sony Circle, Xbox B, Nintendo A"),
	TTRC("Left Action, Sony Square, Xbox X, Nintendo Y"),
	TTRC("Top Action, Sony Triangle, Xbox Y, Nintendo X"),
	TTRC("Back, Sony Select, Xbox Back, Nintendo -"),
	TTRC("Guide, Sony PS, Xbox Home"),
	TTRC("Start, Xbox Menu, Nintendo +"),
	TTRC("Left Stick, Sony L3, Xbox L/LS"),
	TTRC("Right Stick, Sony R3, Xbox R/RS"),
	TTRC("Left Shoulder, Sony L1, Xbox LB"),
	TTRC("Right Shoulder, Sony R1, Xbox RB"),
	TTRC("D-pad Up"),
	TTRC("D-pad Down"),
	TTRC("D-pad Left"),
	TTRC("D-pad Right"),
	TTRC("Xbox Share, PS5 Microphone, Nintendo Capture"),
	TTRC("Xbox Paddle 1"),
	TTRC("Xbox Paddle 2"),
	TTRC("Xbox Paddle 3"),
	TTRC("Xbox Paddle 4"),
	TTRC("PS4/5 Touchpad"),
};

String InputEventJoypadButton::as_text() const {
	String text = vformat(RTR("Joypad Button %d"), (int64_t)button_index);

	if (button_index > JoyButton::INVALID && button_index < JoyButton::SDL_MAX) {
		text += vformat(" (%s)", TTRGET(_joy_button_descriptions[(size_t)button_index]));
	}

	if (pressure != 0) {
		text += ", " + RTR("Pressure:") + " " + String(Variant(pressure));
	}

	return text;
}

String InputEventJoypadButton::to_string() {
	String p = is_pressed() ? "true" : "false";
	return vformat("InputEventJoypadButton: button_index=%d, pressed=%s, pressure=%.2f", button_index, p, pressure);
}

Ref<InputEventJoypadButton> InputEventJoypadButton::create_reference(JoyButton p_btn_index) {
	Ref<InputEventJoypadButton> ie;
	ie.instantiate();
	ie->set_button_index(p_btn_index);

	return ie;
}

void InputEventJoypadButton::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_button_index", "button_index"), &InputEventJoypadButton::set_button_index);
	ClassDB::bind_method(D_METHOD("get_button_index"), &InputEventJoypadButton::get_button_index);

	ClassDB::bind_method(D_METHOD("set_pressure", "pressure"), &InputEventJoypadButton::set_pressure);
	ClassDB::bind_method(D_METHOD("get_pressure"), &InputEventJoypadButton::get_pressure);

	ClassDB::bind_method(D_METHOD("set_pressed", "pressed"), &InputEventJoypadButton::set_pressed);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "button_index"), "set_button_index", "get_button_index");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "pressure"), "set_pressure", "get_pressure");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "pressed"), "set_pressed", "is_pressed");
}

///////////////////////////////////

void InputEventScreenTouch::set_index(int p_index) {
	index = p_index;
}

int InputEventScreenTouch::get_index() const {
	return index;
}

void InputEventScreenTouch::set_position(const Vector2 &p_pos) {
	pos = p_pos;
}

Vector2 InputEventScreenTouch::get_position() const {
	return pos;
}

void InputEventScreenTouch::set_pressed(bool p_pressed) {
	pressed = p_pressed;
}

void InputEventScreenTouch::set_canceled(bool p_canceled) {
	canceled = p_canceled;
}

void InputEventScreenTouch::set_double_tap(bool p_double_tap) {
	double_tap = p_double_tap;
}
bool InputEventScreenTouch::is_double_tap() const {
	return double_tap;
}

Ref<InputEvent> InputEventScreenTouch::xformed_by(const Transform2D &p_xform, const Vector2 &p_local_ofs) const {
	Ref<InputEventScreenTouch> st;
	st.instantiate();
	st->set_device(get_device());
	st->set_window_id(get_window_id());
	st->set_index(index);
	st->set_position(p_xform.xform(pos + p_local_ofs));
	st->set_pressed(pressed);
	st->set_canceled(canceled);
	st->set_double_tap(double_tap);

	st->merge_meta_from(this);

	return st;
}

String InputEventScreenTouch::as_text() const {
	String status = canceled ? RTR("canceled") : (pressed ? RTR("touched") : RTR("released"));

	return vformat(RTR("Screen %s at (%s) with %s touch points"), status, String(get_position()), itos(index));
}

String InputEventScreenTouch::to_string() {
	String p = pressed ? "true" : "false";
	String canceled_state = canceled ? "true" : "false";
	String double_tap_string = double_tap ? "true" : "false";
	return vformat("InputEventScreenTouch: index=%d, pressed=%s, canceled=%s, position=(%s), double_tap=%s", index, p, canceled_state, String(get_position()), double_tap_string);
}

void InputEventScreenTouch::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_index", "index"), &InputEventScreenTouch::set_index);
	ClassDB::bind_method(D_METHOD("get_index"), &InputEventScreenTouch::get_index);

	ClassDB::bind_method(D_METHOD("set_position", "position"), &InputEventScreenTouch::set_position);
	ClassDB::bind_method(D_METHOD("get_position"), &InputEventScreenTouch::get_position);

	ClassDB::bind_method(D_METHOD("set_pressed", "pressed"), &InputEventScreenTouch::set_pressed);
	ClassDB::bind_method(D_METHOD("set_canceled", "canceled"), &InputEventScreenTouch::set_canceled);

	ClassDB::bind_method(D_METHOD("set_double_tap", "double_tap"), &InputEventScreenTouch::set_double_tap);
	ClassDB::bind_method(D_METHOD("is_double_tap"), &InputEventScreenTouch::is_double_tap);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "index"), "set_index", "get_index");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "position", PROPERTY_HINT_NONE, "suffix:px"), "set_position", "get_position");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "canceled"), "set_canceled", "is_canceled");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "pressed"), "set_pressed", "is_pressed");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "double_tap"), "set_double_tap", "is_double_tap");
}

///////////////////////////////////

void InputEventScreenDrag::set_index(int p_index) {
	index = p_index;
}

int InputEventScreenDrag::get_index() const {
	return index;
}

void InputEventScreenDrag::set_tilt(const Vector2 &p_tilt) {
	tilt = p_tilt;
}

Vector2 InputEventScreenDrag::get_tilt() const {
	return tilt;
}

void InputEventScreenDrag::set_pressure(float p_pressure) {
	pressure = p_pressure;
}

float InputEventScreenDrag::get_pressure() const {
	return pressure;
}

void InputEventScreenDrag::set_pen_inverted(bool p_inverted) {
	pen_inverted = p_inverted;
}

bool InputEventScreenDrag::get_pen_inverted() const {
	return pen_inverted;
}

void InputEventScreenDrag::set_position(const Vector2 &p_pos) {
	pos = p_pos;
}

Vector2 InputEventScreenDrag::get_position() const {
	return pos;
}

void InputEventScreenDrag::set_relative(const Vector2 &p_relative) {
	relative = p_relative;
}

Vector2 InputEventScreenDrag::get_relative() const {
	return relative;
}

void InputEventScreenDrag::set_relative_screen_position(const Vector2 &p_relative) {
	screen_relative = p_relative;
}

Vector2 InputEventScreenDrag::get_relative_screen_position() const {
	return screen_relative;
}

void InputEventScreenDrag::set_velocity(const Vector2 &p_velocity) {
	velocity = p_velocity;
}

Vector2 InputEventScreenDrag::get_velocity() const {
	return velocity;
}

void InputEventScreenDrag::set_screen_velocity(const Vector2 &p_velocity) {
	screen_velocity = p_velocity;
}

Vector2 InputEventScreenDrag::get_screen_velocity() const {
	return screen_velocity;
}

Ref<InputEvent> InputEventScreenDrag::xformed_by(const Transform2D &p_xform, const Vector2 &p_local_ofs) const {
	Ref<InputEventScreenDrag> sd;

	sd.instantiate();

	sd->set_device(get_device());
	sd->set_window_id(get_window_id());

	sd->set_index(index);
	sd->set_pressure(get_pressure());
	sd->set_pen_inverted(get_pen_inverted());
	sd->set_tilt(get_tilt());
	sd->set_position(p_xform.xform(pos + p_local_ofs));
	sd->set_relative(p_xform.basis_xform(relative));
	sd->set_relative_screen_position(get_relative_screen_position());
	sd->set_velocity(p_xform.basis_xform(velocity));
	sd->set_screen_velocity(get_screen_velocity());

	sd->merge_meta_from(this);

	return sd;
}

String InputEventScreenDrag::as_text() const {
	return vformat(RTR("Screen dragged with %s touch points at position (%s) with velocity of (%s)"), itos(index), String(get_position()), String(get_velocity()));
}

String InputEventScreenDrag::to_string() {
	return vformat("InputEventScreenDrag: index=%d, position=(%s), relative=(%s), velocity=(%s), pressure=%.2f, tilt=(%s), pen_inverted=(%s)", index, String(get_position()), String(get_relative()), String(get_velocity()), get_pressure(), String(get_tilt()), get_pen_inverted());
}

bool InputEventScreenDrag::accumulate(const Ref<InputEvent> &p_event) {
	Ref<InputEventScreenDrag> drag = p_event;
	if (drag.is_null()) {
		return false;
	}

	if (get_index() != drag->get_index()) {
		return false;
	}

	set_position(drag->get_position());
	set_velocity(drag->get_velocity());
	set_screen_velocity(drag->get_screen_velocity());
	relative += drag->get_relative();
	screen_relative += drag->get_relative_screen_position();

	return true;
}

void InputEventScreenDrag::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_index", "index"), &InputEventScreenDrag::set_index);
	ClassDB::bind_method(D_METHOD("get_index"), &InputEventScreenDrag::get_index);

	ClassDB::bind_method(D_METHOD("set_tilt", "tilt"), &InputEventScreenDrag::set_tilt);
	ClassDB::bind_method(D_METHOD("get_tilt"), &InputEventScreenDrag::get_tilt);

	ClassDB::bind_method(D_METHOD("set_pressure", "pressure"), &InputEventScreenDrag::set_pressure);
	ClassDB::bind_method(D_METHOD("get_pressure"), &InputEventScreenDrag::get_pressure);

	ClassDB::bind_method(D_METHOD("set_pen_inverted", "pen_inverted"), &InputEventScreenDrag::set_pen_inverted);
	ClassDB::bind_method(D_METHOD("get_pen_inverted"), &InputEventScreenDrag::get_pen_inverted);

	ClassDB::bind_method(D_METHOD("set_position", "position"), &InputEventScreenDrag::set_position);
	ClassDB::bind_method(D_METHOD("get_position"), &InputEventScreenDrag::get_position);

	ClassDB::bind_method(D_METHOD("set_relative", "relative"), &InputEventScreenDrag::set_relative);
	ClassDB::bind_method(D_METHOD("get_relative"), &InputEventScreenDrag::get_relative);

	ClassDB::bind_method(D_METHOD("set_screen_relative", "relative"), &InputEventScreenDrag::set_relative_screen_position);
	ClassDB::bind_method(D_METHOD("get_screen_relative"), &InputEventScreenDrag::get_relative_screen_position);

	ClassDB::bind_method(D_METHOD("set_velocity", "velocity"), &InputEventScreenDrag::set_velocity);
	ClassDB::bind_method(D_METHOD("get_velocity"), &InputEventScreenDrag::get_velocity);

	ClassDB::bind_method(D_METHOD("set_screen_velocity", "velocity"), &InputEventScreenDrag::set_screen_velocity);
	ClassDB::bind_method(D_METHOD("get_screen_velocity"), &InputEventScreenDrag::get_screen_velocity);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "index"), "set_index", "get_index");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "tilt"), "set_tilt", "get_tilt");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "pressure"), "set_pressure", "get_pressure");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "pen_inverted"), "set_pen_inverted", "get_pen_inverted");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "position", PROPERTY_HINT_NONE, "suffix:px"), "set_position", "get_position");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "relative", PROPERTY_HINT_NONE, "suffix:px"), "set_relative", "get_relative");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "screen_relative", PROPERTY_HINT_NONE, "suffix:px"), "set_screen_relative", "get_screen_relative");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "velocity", PROPERTY_HINT_NONE, "suffix:px/s"), "set_velocity", "get_velocity");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "screen_velocity", PROPERTY_HINT_NONE, "suffix:px/s"), "set_screen_velocity", "get_screen_velocity");
}

///////////////////////////////////

void InputEventAction::set_action(const StringName &p_action) {
	action = p_action;
}

StringName InputEventAction::get_action() const {
	return action;
}

void InputEventAction::set_pressed(bool p_pressed) {
	pressed = p_pressed;
}

void InputEventAction::set_strength(float p_strength) {
	strength = CLAMP(p_strength, 0.0f, 1.0f);
}

float InputEventAction::get_strength() const {
	return strength;
}

void InputEventAction::set_event_index(int p_index) {
	event_index = p_index;
}

int InputEventAction::get_event_index() const {
	return event_index;
}

bool InputEventAction::is_match(const Ref<InputEvent> &p_event, bool p_exact_match) const {
	if (p_event.is_null()) {
		return false;
	}

	return p_event->is_action(action, p_exact_match);
}

bool InputEventAction::is_action(const StringName &p_action) const {
	return action == p_action;
}

bool InputEventAction::action_match(const Ref<InputEvent> &p_event, bool p_exact_match, float p_deadzone, bool *r_pressed, float *r_strength, float *r_raw_strength) const {
	Ref<InputEventAction> act = p_event;
	if (act.is_null()) {
		return false;
	}

	bool match = action == act->action;
	if (match) {
		bool act_pressed = act->is_pressed();
		if (r_pressed != nullptr) {
			*r_pressed = act_pressed;
		}
		float act_strength = act_pressed ? 1.0f : 0.0f;
		if (r_strength != nullptr) {
			*r_strength = act_strength;
		}
		if (r_raw_strength != nullptr) {
			*r_raw_strength = act_strength;
		}
	}
	return match;
}

String InputEventAction::as_text() const {
	const List<Ref<InputEvent>> *events = InputMap::get_singleton()->action_get_events(action);
	if (!events) {
		return String();
	}

	for (const Ref<InputEvent> &E : *events) {
		if (E.is_valid()) {
			return E->as_text();
		}
	}

	return String();
}

String InputEventAction::to_string() {
	String p = is_pressed() ? "true" : "false";
	return vformat("InputEventAction: action=\"%s\", pressed=%s", action, p);
}

void InputEventAction::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_action", "action"), &InputEventAction::set_action);
	ClassDB::bind_method(D_METHOD("get_action"), &InputEventAction::get_action);

	ClassDB::bind_method(D_METHOD("set_pressed", "pressed"), &InputEventAction::set_pressed);

	ClassDB::bind_method(D_METHOD("set_strength", "strength"), &InputEventAction::set_strength);
	ClassDB::bind_method(D_METHOD("get_strength"), &InputEventAction::get_strength);

	ClassDB::bind_method(D_METHOD("set_event_index", "index"), &InputEventAction::set_event_index);
	ClassDB::bind_method(D_METHOD("get_event_index"), &InputEventAction::get_event_index);

	ADD_PROPERTY(PropertyInfo(Variant::STRING_NAME, "action"), "set_action", "get_action");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "pressed"), "set_pressed", "is_pressed");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "strength", PROPERTY_HINT_RANGE, "0,1,0.01"), "set_strength", "get_strength");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "event_index", PROPERTY_HINT_RANGE, "-1,31,1"), "set_event_index", "get_event_index"); // The max value equals to Input::MAX_EVENT - 1.
}

///////////////////////////////////

void InputEventGesture::set_position(const Vector2 &p_pos) {
	pos = p_pos;
}

void InputEventGesture::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_position", "position"), &InputEventGesture::set_position);
	ClassDB::bind_method(D_METHOD("get_position"), &InputEventGesture::get_position);

	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "position", PROPERTY_HINT_NONE, "suffix:px"), "set_position", "get_position");
}

Vector2 InputEventGesture::get_position() const {
	return pos;
}

///////////////////////////////////

void InputEventMagnifyGesture::set_factor(real_t p_factor) {
	factor = p_factor;
}

real_t InputEventMagnifyGesture::get_factor() const {
	return factor;
}

Ref<InputEvent> InputEventMagnifyGesture::xformed_by(const Transform2D &p_xform, const Vector2 &p_local_ofs) const {
	Ref<InputEventMagnifyGesture> ev;
	ev.instantiate();

	ev->set_device(get_device());
	ev->set_window_id(get_window_id());

	ev->set_modifiers_from_event(this);

	ev->set_position(p_xform.xform(get_position() + p_local_ofs));
	ev->set_factor(get_factor());

	ev->merge_meta_from(this);

	return ev;
}

String InputEventMagnifyGesture::as_text() const {
	return vformat(RTR("Magnify Gesture at (%s) with factor %s"), String(get_position()), rtos(get_factor()));
}

String InputEventMagnifyGesture::to_string() {
	return vformat("InputEventMagnifyGesture: factor=%.2f, position=(%s)", factor, String(get_position()));
}

void InputEventMagnifyGesture::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_factor", "factor"), &InputEventMagnifyGesture::set_factor);
	ClassDB::bind_method(D_METHOD("get_factor"), &InputEventMagnifyGesture::get_factor);

	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "factor"), "set_factor", "get_factor");
}

///////////////////////////////////

void InputEventPanGesture::set_delta(const Vector2 &p_delta) {
	delta = p_delta;
}

Vector2 InputEventPanGesture::get_delta() const {
	return delta;
}

Ref<InputEvent> InputEventPanGesture::xformed_by(const Transform2D &p_xform, const Vector2 &p_local_ofs) const {
	Ref<InputEventPanGesture> ev;
	ev.instantiate();

	ev->set_device(get_device());
	ev->set_window_id(get_window_id());

	ev->set_modifiers_from_event(this);

	ev->set_position(p_xform.xform(get_position() + p_local_ofs));
	ev->set_delta(get_delta());

	ev->merge_meta_from(this);

	return ev;
}

String InputEventPanGesture::as_text() const {
	return vformat(RTR("Pan Gesture at (%s) with delta (%s)"), String(get_position()), String(get_delta()));
}

String InputEventPanGesture::to_string() {
	return vformat("InputEventPanGesture: delta=(%s), position=(%s)", String(get_delta()), String(get_position()));
}

void InputEventPanGesture::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_delta", "delta"), &InputEventPanGesture::set_delta);
	ClassDB::bind_method(D_METHOD("get_delta"), &InputEventPanGesture::get_delta);

	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "delta"), "set_delta", "get_delta");
}

///////////////////////////////////

void InputEventMIDI::set_channel(const int p_channel) {
	channel = p_channel;
}

int InputEventMIDI::get_channel() const {
	return channel;
}

void InputEventMIDI::set_message(const MIDIMessage p_message) {
	message = p_message;
}

MIDIMessage InputEventMIDI::get_message() const {
	return message;
}

void InputEventMIDI::set_pitch(const int p_pitch) {
	pitch = p_pitch;
}

int InputEventMIDI::get_pitch() const {
	return pitch;
}

void InputEventMIDI::set_velocity(const int p_velocity) {
	velocity = p_velocity;
}

int InputEventMIDI::get_velocity() const {
	return velocity;
}

void InputEventMIDI::set_instrument(const int p_instrument) {
	instrument = p_instrument;
}

int InputEventMIDI::get_instrument() const {
	return instrument;
}

void InputEventMIDI::set_pressure(const int p_pressure) {
	pressure = p_pressure;
}

int InputEventMIDI::get_pressure() const {
	return pressure;
}

void InputEventMIDI::set_controller_number(const int p_controller_number) {
	controller_number = p_controller_number;
}

int InputEventMIDI::get_controller_number() const {
	return controller_number;
}

void InputEventMIDI::set_controller_value(const int p_controller_value) {
	controller_value = p_controller_value;
}

int InputEventMIDI::get_controller_value() const {
	return controller_value;
}

String InputEventMIDI::as_text() const {
	return vformat(RTR("MIDI Input on Channel=%s Message=%s"), itos(channel), itos((int64_t)message));
}

String InputEventMIDI::to_string() {
	String ret;
	switch (message) {
		case MIDIMessage::NOTE_ON:
			ret = vformat("Note On: channel=%d, pitch=%d, velocity=%d", channel, pitch, velocity);
			break;
		case MIDIMessage::NOTE_OFF:
			ret = vformat("Note Off: channel=%d, pitch=%d, velocity=%d", channel, pitch, velocity);
			break;
		case MIDIMessage::PITCH_BEND:
			ret = vformat("Pitch Bend: channel=%d, pitch=%d", channel, pitch);
			break;
		case MIDIMessage::CHANNEL_PRESSURE:
			ret = vformat("Channel Pressure: channel=%d, pressure=%d", channel, pressure);
			break;
		case MIDIMessage::CONTROL_CHANGE:
			ret = vformat("Control Change: channel=%d, controller_number=%d, controller_value=%d", channel, controller_number, controller_value);
			break;
		default:
			ret = vformat("channel=%d, message=%d, pitch=%d, velocity=%d, pressure=%d, controller_number=%d, controller_value=%d, instrument=%d", channel, message, pitch, velocity, pressure, controller_number, controller_value, instrument);
	}
	return "InputEventMIDI: " + ret;
}

void InputEventMIDI::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_channel", "channel"), &InputEventMIDI::set_channel);
	ClassDB::bind_method(D_METHOD("get_channel"), &InputEventMIDI::get_channel);
	ClassDB::bind_method(D_METHOD("set_message", "message"), &InputEventMIDI::set_message);
	ClassDB::bind_method(D_METHOD("get_message"), &InputEventMIDI::get_message);
	ClassDB::bind_method(D_METHOD("set_pitch", "pitch"), &InputEventMIDI::set_pitch);
	ClassDB::bind_method(D_METHOD("get_pitch"), &InputEventMIDI::get_pitch);
	ClassDB::bind_method(D_METHOD("set_velocity", "velocity"), &InputEventMIDI::set_velocity);
	ClassDB::bind_method(D_METHOD("get_velocity"), &InputEventMIDI::get_velocity);
	ClassDB::bind_method(D_METHOD("set_instrument", "instrument"), &InputEventMIDI::set_instrument);
	ClassDB::bind_method(D_METHOD("get_instrument"), &InputEventMIDI::get_instrument);
	ClassDB::bind_method(D_METHOD("set_pressure", "pressure"), &InputEventMIDI::set_pressure);
	ClassDB::bind_method(D_METHOD("get_pressure"), &InputEventMIDI::get_pressure);
	ClassDB::bind_method(D_METHOD("set_controller_number", "controller_number"), &InputEventMIDI::set_controller_number);
	ClassDB::bind_method(D_METHOD("get_controller_number"), &InputEventMIDI::get_controller_number);
	ClassDB::bind_method(D_METHOD("set_controller_value", "controller_value"), &InputEventMIDI::set_controller_value);
	ClassDB::bind_method(D_METHOD("get_controller_value"), &InputEventMIDI::get_controller_value);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "channel"), "set_channel", "get_channel");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "message"), "set_message", "get_message");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "pitch"), "set_pitch", "get_pitch");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "velocity"), "set_velocity", "get_velocity");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "instrument"), "set_instrument", "get_instrument");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "pressure"), "set_pressure", "get_pressure");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "controller_number"), "set_controller_number", "get_controller_number");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "controller_value"), "set_controller_value", "get_controller_value");
}

///////////////////////////////////

void InputEventShortcut::set_shortcut(Ref<Shortcut> p_shortcut) {
	shortcut = p_shortcut;
	emit_changed();
}

Ref<Shortcut> InputEventShortcut::get_shortcut() {
	return shortcut;
}

void InputEventShortcut::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_shortcut", "shortcut"), &InputEventShortcut::set_shortcut);
	ClassDB::bind_method(D_METHOD("get_shortcut"), &InputEventShortcut::get_shortcut);

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "shortcut", PROPERTY_HINT_RESOURCE_TYPE, "Shortcut"), "set_shortcut", "get_shortcut");
}

String InputEventShortcut::as_text() const {
	ERR_FAIL_COND_V(shortcut.is_null(), "None");

	return vformat(RTR("Input Event with Shortcut=%s"), shortcut->get_as_text());
}

String InputEventShortcut::to_string() {
	ERR_FAIL_COND_V(shortcut.is_null(), "None");

	return vformat("InputEventShortcut: shortcut=%s", shortcut->get_as_text());
}

InputEventShortcut::InputEventShortcut() {
	pressed = true;
}

/**************************************************************************/
/*  test_input_event_key.h                                                */
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

#pragma once

#include "core/input/input_event.h"
#include "core/os/keyboard.h"

#include "tests/test_macros.h"

namespace TestInputEventKey {

TEST_CASE("[InputEventKey] Key correctly registers being pressed") {
	InputEventKey key;
	key.set_pressed(true);
	CHECK(key.is_pressed() == true);

	key.set_pressed(false);
	CHECK(key.is_pressed() == false);
}

TEST_CASE("[InputEventKey] Key correctly stores and retrieves keycode") {
	InputEventKey key;

	key.set_keycode(Key::ENTER);
	CHECK(key.get_keycode() == Key::ENTER);
	CHECK(key.get_keycode() != Key::PAUSE);

	key.set_physical_keycode(Key::BACKSPACE);
	CHECK(key.get_physical_keycode() == Key::BACKSPACE);
	CHECK(key.get_physical_keycode() != Key::PAUSE);
}

TEST_CASE("[InputEventKey] Key correctly stores and retrieves keycode with modifiers") {
	InputEventKey key;

	key.set_keycode(Key::ENTER);
	key.set_ctrl_pressed(true);

	CHECK(key.get_keycode_with_modifiers() == (Key::ENTER | KeyModifierMask::CTRL));
	CHECK(key.get_keycode_with_modifiers() != (Key::ENTER | KeyModifierMask::SHIFT));
	CHECK(key.get_keycode_with_modifiers() != Key::ENTER);

	key.set_physical_keycode(Key::SPACE);
	key.set_ctrl_pressed(true);

	CHECK(key.get_physical_keycode_with_modifiers() == (Key::SPACE | KeyModifierMask::CTRL));
	CHECK(key.get_physical_keycode_with_modifiers() != (Key::SPACE | KeyModifierMask::SHIFT));
	CHECK(key.get_physical_keycode_with_modifiers() != Key::SPACE);
}

TEST_CASE("[InputEventKey] Key correctly stores and retrieves unicode") {
	InputEventKey key;

	key.set_unicode('x');
	CHECK(key.get_unicode() == 'x');
	CHECK(key.get_unicode() != 'y');
}

TEST_CASE("[InputEventKey] Key correctly stores and retrieves location") {
	InputEventKey key;

	CHECK(key.get_location() == KeyLocation::UNSPECIFIED);

	key.set_location(KeyLocation::LEFT);
	CHECK(key.get_location() == KeyLocation::LEFT);
	CHECK(key.get_location() != KeyLocation::RIGHT);
}

TEST_CASE("[InputEventKey] Key correctly stores and checks echo") {
	InputEventKey key;

	key.set_echo(true);
	CHECK(key.is_echo() == true);

	key.set_echo(false);
	CHECK(key.is_echo() == false);
}

TEST_CASE("[InputEventKey] Key correctly converts itself to text") {
	InputEventKey none_key;

	// These next three tests test the functionality of getting a key that is set to None
	// as text. These cases are a bit weird, since None has no textual representation
	// (find_keycode_name(Key::NONE) results in a nullptr). Thus, these tests look weird
	// with only (Physical) or a lonely modifier with (Physical) but (as far as I
	// understand the code, that is intended behavior.

	// Key is None without a physical key.
	none_key.set_keycode(Key::NONE);
	CHECK(none_key.as_text() == "(Unset)");

	// Key is none and has modifiers.
	none_key.set_ctrl_pressed(true);
	CHECK(none_key.as_text() == "Ctrl+(Unset)");

	// Key is None WITH a physical key AND modifiers.
	none_key.set_physical_keycode(Key::ENTER);
	CHECK(none_key.as_text() == "Ctrl+Enter (Physical)");

	InputEventKey none_key2;

	// Key is None without modifiers with a physical key.
	none_key2.set_keycode(Key::NONE);
	none_key2.set_physical_keycode(Key::ENTER);

	CHECK(none_key2.as_text() == "Enter (Physical)");

	InputEventKey key;

	// Key has keycode.
	key.set_keycode(Key::SPACE);
	CHECK(key.as_text() != "");
	CHECK(key.as_text() == "Space");

	// Key has keycode and modifiers.
	key.set_ctrl_pressed(true);
	CHECK(key.as_text() != "Space");
	CHECK(key.as_text() == "Ctrl+Space");

	// Since the keycode is set to Key::NONE upon initialization of the
	// InputEventKey and you can only update it with another Key, the keycode
	// cannot be empty, so the kc.is_empty() case cannot be tested.
}

TEST_CASE("[InputEventKey] Key correctly converts its state to a string representation") {
	InputEventKey none_key;

	CHECK(none_key.to_string() == "InputEventKey: keycode=(Unset), mods=none, physical=false, location=unspecified, pressed=false, echo=false");
	// Set physical key to Escape.
	none_key.set_physical_keycode(Key::ESCAPE);
	CHECK(none_key.to_string() == "InputEventKey: keycode=4194305 (Escape), mods=none, physical=true, location=unspecified, pressed=false, echo=false");

	InputEventKey key;

	// Set physical to None, set keycode to Space.
	key.set_keycode(Key::SPACE);
	CHECK(key.to_string() == "InputEventKey: keycode=32 (Space), mods=none, physical=false, location=unspecified, pressed=false, echo=false");

	// Set location
	key.set_location(KeyLocation::RIGHT);
	CHECK(key.to_string() == "InputEventKey: keycode=32 (Space), mods=none, physical=false, location=right, pressed=false, echo=false");

	// Set pressed to true.
	key.set_pressed(true);
	CHECK(key.to_string() == "InputEventKey: keycode=32 (Space), mods=none, physical=false, location=right, pressed=true, echo=false");

	// set echo to true.
	key.set_echo(true);
	CHECK(key.to_string() == "InputEventKey: keycode=32 (Space), mods=none, physical=false, location=right, pressed=true, echo=true");

	// Press Ctrl and Alt.
	key.set_ctrl_pressed(true);
	key.set_alt_pressed(true);
#ifdef MACOS_ENABLED
	CHECK(key.to_string() == "InputEventKey: keycode=32 (Space), mods=Ctrl+Option, physical=false, location=right, pressed=true, echo=true");
#else
	CHECK(key.to_string() == "InputEventKey: keycode=32 (Space), mods=Ctrl+Alt, physical=false, location=right, pressed=true, echo=true");
#endif
}

TEST_CASE("[InputEventKey] Key is correctly converted to reference") {
	InputEventKey base_key;
	Ref<InputEventKey> key_ref = base_key.create_reference(Key::ENTER);

	CHECK(key_ref->get_keycode() == Key::ENTER);
}

TEST_CASE("[InputEventKey] Keys are correctly matched based on action") {
	bool pressed = false;
	float strength, raw_strength = 0.0;

	InputEventKey key;

	// Nullptr.
	CHECK_MESSAGE(key.action_match(nullptr, false, 0.0f, &pressed, &strength, &raw_strength) == false, "nullptr as key reference should result in false");

	// Match on keycode.
	key.set_keycode(Key::SPACE);
	Ref<InputEventKey> match = key.create_reference(Key::SPACE);
	Ref<InputEventKey> no_match = key.create_reference(Key::ENTER);

	CHECK(key.action_match(match, false, 0.0f, &pressed, &strength, &raw_strength) == true);
	CHECK(key.action_match(no_match, false, 0.0f, &pressed, &strength, &raw_strength) == false);

	// Check that values are correctly transferred to the pointers.
	CHECK(pressed == false);
	CHECK(strength < 0.5);
	CHECK(raw_strength < 0.5);

	match->set_pressed(true);
	key.action_match(match, false, 0.0f, &pressed, &strength, &raw_strength);

	CHECK(pressed == true);
	CHECK(strength > 0.5);
	CHECK(raw_strength > 0.5);

	// Tests when keycode is None: Then you rely on physical keycode.
	InputEventKey none_key;
	none_key.set_physical_keycode(Key::SPACE);

	Ref<InputEventKey> match_none = none_key.create_reference(Key::NONE);
	match_none->set_physical_keycode(Key::SPACE);

	Ref<InputEventKey> no_match_none = none_key.create_reference(Key::NONE);
	no_match_none->set_physical_keycode(Key::ENTER);

	CHECK(none_key.action_match(match_none, false, 0.0f, &pressed, &strength, &raw_strength) == true);
	CHECK(none_key.action_match(no_match_none, false, 0.0f, &pressed, &strength, &raw_strength) == false);

	// Test exact match.
	InputEventKey key2, ref_key;
	key2.set_keycode(Key::SPACE);

	Ref<InputEventKey> match2 = ref_key.create_reference(Key::SPACE);

	// Now both press Ctrl and Shift.
	key2.set_ctrl_pressed(true);
	key2.set_shift_pressed(true);

	match2->set_ctrl_pressed(true);
	match2->set_shift_pressed(true);

	// Now they should match.
	bool exact_match = true;
	CHECK(key2.action_match(match2, exact_match, 0.0f, &pressed, &strength, &raw_strength) == true);

	// Modify matching key such that it does no longer match in terms of modifiers: Shift
	// is no longer pressed.
	match2->set_shift_pressed(false);
	CHECK(match2->is_shift_pressed() == false);
	CHECK(key2.action_match(match2, exact_match, 0.0f, &pressed, &strength, &raw_strength) == false);
}

TEST_CASE("[IsMatch] Keys are correctly matched") {
	// Key with NONE as keycode.
	InputEventKey key;
	key.set_keycode(Key::NONE);
	key.set_physical_keycode(Key::SPACE);

	// Nullptr.
	CHECK(key.is_match(nullptr, false) == false);

	Ref<InputEventKey> none_ref = key.create_reference(Key::NONE);

	none_ref->set_physical_keycode(Key::SPACE);
	CHECK(key.is_match(none_ref, false) == true);

	none_ref->set_physical_keycode(Key::ENTER);
	CHECK(key.is_match(none_ref, false) == false);

	none_ref->set_physical_keycode(Key::SPACE);

	key.set_ctrl_pressed(true);
	none_ref->set_ctrl_pressed(false);
	CHECK(key.is_match(none_ref, true) == false);

	none_ref->set_ctrl_pressed(true);
	CHECK(key.is_match(none_ref, true) == true);

	// Ref with actual keycode.
	InputEventKey key2;
	key2.set_keycode(Key::SPACE);

	Ref<InputEventKey> match = key2.create_reference(Key::SPACE);
	Ref<InputEventKey> no_match = key2.create_reference(Key::ENTER);

	CHECK(key2.is_match(match, false) == true);
	CHECK(key2.is_match(no_match, false) == false);

	// Now the keycode is the same, but the modifiers differ.
	no_match->set_keycode(Key::SPACE);

	key2.set_ctrl_pressed(true);
	match->set_ctrl_pressed(true);
	no_match->set_shift_pressed(true);

	CHECK(key2.is_match(match, true) == true);
	CHECK(key2.is_match(no_match, true) == false);

	// Physical key with location.
	InputEventKey key3;
	key3.set_keycode(Key::NONE);
	key3.set_physical_keycode(Key::SHIFT);

	Ref<InputEventKey> loc_ref = key.create_reference(Key::NONE);

	loc_ref->set_keycode(Key::SHIFT);
	loc_ref->set_physical_keycode(Key::SHIFT);

	CHECK(key3.is_match(loc_ref, false) == true);
	key3.set_location(KeyLocation::UNSPECIFIED);
	CHECK(key3.is_match(loc_ref, false) == true);

	loc_ref->set_location(KeyLocation::LEFT);
	CHECK(key3.is_match(loc_ref, false) == true);

	key3.set_location(KeyLocation::LEFT);
	CHECK(key3.is_match(loc_ref, false) == true);

	key3.set_location(KeyLocation::RIGHT);
	CHECK(key3.is_match(loc_ref, false) == false);

	// Keycode key with location.
	key3.set_physical_keycode(Key::NONE);
	key3.set_keycode(Key::SHIFT);
	CHECK(key3.is_match(loc_ref, false) == true);
}
} // namespace TestInputEventKey

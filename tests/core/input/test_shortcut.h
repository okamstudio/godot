/**************************************************************************/
/*  test_shortcut.h                                                       */
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
#include "core/input/shortcut.h"
#include "core/io/config_file.h"
#include "core/object/ref_counted.h"
#include "core/os/keyboard.h"
#include "core/os/os.h"

#include "tests/test_macros.h"

namespace TestShortcut {

TEST_CASE("[Shortcut] Empty shortcut should have no valid events and text equal to None") {
	Shortcut s;

	CHECK(s.get_as_text() == "None");
	CHECK(s.has_valid_event() == false);
}

TEST_CASE("[Shortcut] Setting and getting an event should result in the same event as the input") {
	Ref<InputEventKey> k1;
	Ref<InputEventKey> k2;
	k1.instantiate();
	k2.instantiate();

	k1->set_keycode(Key::ENTER);
	k2->set_keycode(Key::BACKSPACE);

	// Cast to InputEvent so the internal code recognizes the objects.
	Ref<InputEvent> e1 = k1;
	Ref<InputEvent> e2 = k2;

	Array input_array;
	input_array.append(e1);
	input_array.append(e2);

	Shortcut s;
	s.set_events(input_array);

	// Get result, read it out, check whether it equals the input.
	Array result_array = s.get_events();
	Ref<InputEventKey> result1 = result_array.front();
	Ref<InputEventKey> result2 = result_array.back();

	CHECK(result1->get_keycode() == k1->get_keycode());
	CHECK(result2->get_keycode() == k2->get_keycode());
}

TEST_CASE("[Shortcut] 'set_events_list' should result in the same events as the input") {
	Ref<InputEventKey> k1;
	Ref<InputEventKey> k2;
	k1.instantiate();
	k2.instantiate();

	k1->set_keycode(Key::ENTER);
	k2->set_keycode(Key::BACKSPACE);

	// Cast to InputEvent so the set_events_list() method recognizes the objects.
	Ref<InputEvent> e1 = k1;
	Ref<InputEvent> e2 = k2;

	List<Ref<InputEvent>> list;
	list.push_back(e1);
	list.push_back(e2);

	Shortcut s;
	s.set_events_list(&list);

	// Get result, read it out, check whether it equals the input.
	Array result_array = s.get_events();
	Ref<InputEventKey> result1 = result_array.front();
	Ref<InputEventKey> result2 = result_array.back();

	CHECK(result1->get_keycode() == k1->get_keycode());
	CHECK(result2->get_keycode() == k2->get_keycode());
}

TEST_CASE("[Shortcut] 'matches_event' should correctly match the same event") {
	Ref<InputEventKey> original; // The one we compare with.
	Ref<InputEventKey> similar_but_not_equal; // Same keycode, different event.
	Ref<InputEventKey> different; // Different event, different keycode.
	Ref<InputEventKey> copy; // Copy of original event.

	original.instantiate();
	similar_but_not_equal.instantiate();
	different.instantiate();
	copy.instantiate();

	original->set_keycode(Key::ENTER);
	similar_but_not_equal->set_keycode(Key::ENTER);
	similar_but_not_equal->set_keycode(Key::ESCAPE);
	copy = original;

	// Only the copy is really the same, so only that one should match.
	// The rest should not match.

	Ref<InputEvent> e_original = original;

	Ref<InputEvent> e_similar_but_not_equal = similar_but_not_equal;
	Ref<InputEvent> e_different = different;
	Ref<InputEvent> e_copy = copy;

	Array a;
	a.append(e_original);
	Shortcut s;
	s.set_events(a);

	CHECK(s.matches_event(e_similar_but_not_equal) == false);
	CHECK(s.matches_event(e_different) == false);

	CHECK(s.matches_event(e_copy) == true);
}

TEST_CASE("[Shortcut] 'get_as_text' text representation should be correct") {
	Ref<InputEventKey> same;
	// k2 will not go into the shortcut but only be used to compare.
	Ref<InputEventKey> different;

	same.instantiate();
	different.instantiate();

	same->set_keycode(Key::ENTER);
	different->set_keycode(Key::ESCAPE);

	Ref<InputEvent> key_event1 = same;

	Array a;
	a.append(key_event1);
	Shortcut s;
	s.set_events(a);

	CHECK(s.get_as_text() == same->as_text());
	CHECK(s.get_as_text() != different->as_text());
}

TEST_CASE("[Shortcut] Event validity should be correctly checked.") {
	Ref<InputEventKey> valid;
	// k2 will not go into the shortcut but only be used to compare.
	Ref<InputEventKey> invalid = nullptr;

	valid.instantiate();
	valid->set_keycode(Key::ENTER);

	Ref<InputEvent> valid_event = valid;
	Ref<InputEvent> invalid_event = invalid;

	Array a;
	a.append(invalid_event);
	a.append(valid_event);

	Shortcut s;
	s.set_events(a);

	CHECK(s.has_valid_event() == true);

	Array b;
	b.append(invalid_event);

	Shortcut shortcut_with_invalid_event;
	shortcut_with_invalid_event.set_events(b);

	CHECK(shortcut_with_invalid_event.has_valid_event() == false);
}

TEST_CASE("[Shortcut] Equal arrays should be recognized as such.") {
	Ref<InputEventKey> k1;
	// k2 will not go into the shortcut but only be used to compare.
	Ref<InputEventKey> k2;

	k1.instantiate();
	k2.instantiate();

	k1->set_keycode(Key::ENTER);
	k2->set_keycode(Key::ESCAPE);

	Ref<InputEvent> key_event1 = k1;
	Ref<InputEvent> key_event2 = k2;

	Array same;
	same.append(key_event1);

	Array same_as_same;
	same_as_same.append(key_event1);

	Array different1;
	different1.append(key_event2);

	Array different2;
	different2.append(key_event1);
	different2.append(key_event2);

	Array different3;

	Shortcut s;

	CHECK(s.is_event_array_equal(same, same_as_same) == true);
	CHECK(s.is_event_array_equal(same, different1) == false);
	CHECK(s.is_event_array_equal(same, different2) == false);
	CHECK(s.is_event_array_equal(same, different3) == false);
}
} // namespace TestShortcut

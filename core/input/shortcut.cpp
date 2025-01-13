/**************************************************************************/
/*  shortcut.cpp                                                          */
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

#include "shortcut.h"

void Shortcut::set_events(const Array &p_events) {
	for (int i = 0; i < p_events.size(); i++) {
		Ref<InputEventShortcut> ies = p_events[i];
		ERR_FAIL_COND_MSG(ies.is_valid(), "Cannot set a shortcut event to an instance of InputEventShortcut.");
	}

	events = p_events;
	emit_changed();
}

void Shortcut::set_events_list(const List<Ref<InputEvent>> *p_events) {
	events.clear();

	for (const Ref<InputEvent> &ie : *p_events) {
		events.push_back(ie);
	}
}

Array Shortcut::get_events() const {
	return events;
}

bool Shortcut::matches_event(const Ref<InputEvent> &p_event) const {
	Ref<InputEventShortcut> ies = p_event;
	if (ies.is_valid()) {
		if (ies->get_shortcut().ptr() == this) {
			return true;
		}
	}

	for (int i = 0; i < events.size(); i++) {
		Ref<InputEvent> ie = events[i];
		bool valid = ie.is_valid() && ie->is_match(p_event);

		// Stop on first valid event - don't need to check further.
		if (valid) {
			return true;
		}
	}

	return false;
}

String Shortcut::get_as_text() const {
	for (int i = 0; i < events.size(); i++) {
		Ref<InputEvent> ie = events[i];
		// Return first shortcut which is valid
		if (ie.is_valid()) {
			return ie->as_text();
		}
	}

	return "None";
}

bool Shortcut::has_valid_event() const {
	// Tests if there is ANY input event which is valid.
	for (int i = 0; i < events.size(); i++) {
		Ref<InputEvent> ie = events[i];
		if (ie.is_valid()) {
			return true;
		}
	}

	return false;
}

void Shortcut::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_events", "events"), &Shortcut::set_events);
	ClassDB::bind_method(D_METHOD("get_events"), &Shortcut::get_events);

	ClassDB::bind_method(D_METHOD("has_valid_event"), &Shortcut::has_valid_event);

	ClassDB::bind_method(D_METHOD("matches_event", "event"), &Shortcut::matches_event);
	ClassDB::bind_method(D_METHOD("get_as_text"), &Shortcut::get_as_text);

	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "events", PROPERTY_HINT_ARRAY_TYPE, MAKE_RESOURCE_TYPE_HINT("InputEvent")), "set_events", "get_events");
}

bool Shortcut::is_event_array_equal(const Array &p_event_array1, const Array &p_event_array2) {
	if (p_event_array1.size() != p_event_array2.size()) {
		return false;
	}

	bool is_same = true;
	for (int i = 0; i < p_event_array1.size(); i++) {
		Ref<InputEvent> ie_1 = p_event_array1[i];
		Ref<InputEvent> ie_2 = p_event_array2[i];

		is_same = ie_1->is_match(ie_2);

		// Break on the first that doesn't match - don't need to check further.
		if (!is_same) {
			break;
		}
	}

	return is_same;
}

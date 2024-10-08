/**************************************************************************/
/*  input_map.cpp                                                         */
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

#include "input_map.h"
#include "input_map.compat.inc"

#include "core/config/project_settings.h"
#include "core/input/input.h"
#include "core/os/keyboard.h"
#include "core/os/os.h"
#include "core/variant/typed_array.h"

InputMap *InputMap::singleton = nullptr;

int InputMap::ALL_DEVICES = -1;

void InputMap::_bind_methods() {
	ClassDB::bind_method(D_METHOD("has_action", "action"), &InputMap::has_action);
	ClassDB::bind_method(D_METHOD("get_actions"), &InputMap::_get_actions);
	ClassDB::bind_method(D_METHOD("add_action", "action", "deadzone"), &InputMap::add_action, DEFVAL(0.2f));
	ClassDB::bind_method(D_METHOD("erase_action", "action"), &InputMap::erase_action);

	ClassDB::bind_method(D_METHOD("action_set_deadzone", "action", "deadzone"), &InputMap::action_set_deadzone);
	ClassDB::bind_method(D_METHOD("action_get_deadzone", "action"), &InputMap::action_get_deadzone);
	ClassDB::bind_method(D_METHOD("action_add_event", "action", "event"), &InputMap::action_add_event);
	ClassDB::bind_method(D_METHOD("action_has_event", "action", "event"), &InputMap::action_has_event);
	ClassDB::bind_method(D_METHOD("action_erase_event", "action", "event"), &InputMap::action_erase_event);
	ClassDB::bind_method(D_METHOD("action_erase_events", "action"), &InputMap::action_erase_events);
	ClassDB::bind_method(D_METHOD("action_get_events", "action"), &InputMap::_action_get_events);
	ClassDB::bind_method(D_METHOD("event_is_action", "event", "action", "exact_match"), &InputMap::event_is_action, DEFVAL(false));
	ClassDB::bind_method(D_METHOD("load_from_project_settings"), &InputMap::load_from_project_settings);
}

/**
 * Returns an nonexistent action error message with a suggestion of the closest
 * matching action name (if possible).
 */
String InputMap::suggest_actions(const StringName &p_action) const {
	List<StringName> actions = get_actions();
	StringName closest_action;
	float closest_similarity = 0.0;

	// Find the most action with the most similar name.
	for (const StringName &action : actions) {
		const float similarity = String(action).similarity(p_action);

		if (similarity > closest_similarity) {
			closest_action = action;
			closest_similarity = similarity;
		}
	}

	String error_message = vformat("The InputMap action \"%s\" doesn't exist.", p_action);

	if (closest_similarity >= 0.4) {
		// Only include a suggestion in the error message if it's similar enough.
		error_message += vformat(" Did you mean \"%s\"?", closest_action);
	}
	return error_message;
}

#ifdef TOOLS_ENABLED
void InputMap::get_argument_options(const StringName &p_function, int p_idx, List<String> *r_options) const {
	const String pf = p_function;
	bool first_argument_is_action = false;
	if (p_idx == 0) {
		first_argument_is_action = (pf == "has_action" || pf == "erase_action" ||
				pf == "action_set_deadzone" || pf == "action_get_deadzone" ||
				pf == "action_has_event" || pf == "action_add_event" || pf == "action_get_events" ||
				pf == "action_erase_event" || pf == "action_erase_events");
	}
	if (first_argument_is_action || (p_idx == 1 && pf == "event_is_action")) {
		// Cannot rely on `get_actions()`, otherwise the actions would be in the context of the Editor (no user-defined actions).
		List<PropertyInfo> pinfo;
		ProjectSettings::get_singleton()->get_property_list(&pinfo);

		for (const PropertyInfo &pi : pinfo) {
			if (!pi.name.begins_with("input/")) {
				continue;
			}

			String name = pi.name.substr(pi.name.find("/") + 1, pi.name.length());
			r_options->push_back(name.quote());
		}
	}

	Object::get_argument_options(p_function, p_idx, r_options);
}
#endif

void InputMap::add_action(const StringName &p_action, float p_deadzone) {
	ERR_FAIL_COND_MSG(input_map.has(p_action), "InputMap already has action \"" + String(p_action) + "\".");
	input_map[p_action] = Action();
	static int last_id = 1;
	input_map[p_action].id = last_id;
	input_map[p_action].deadzone = p_deadzone;
	last_id++;
}

void InputMap::erase_action(const StringName &p_action) {
	ERR_FAIL_COND_MSG(!input_map.has(p_action), suggest_actions(p_action));

	input_map.erase(p_action);
}

TypedArray<StringName> InputMap::_get_actions() {
	TypedArray<StringName> ret;
	List<StringName> actions = get_actions();
	if (actions.is_empty()) {
		return ret;
	}

	for (const StringName &E : actions) {
		ret.push_back(E);
	}

	return ret;
}

List<StringName> InputMap::get_actions() const {
	List<StringName> actions = List<StringName>();
	if (input_map.is_empty()) {
		return actions;
	}

	for (const KeyValue<StringName, Action> &E : input_map) {
		actions.push_back(E.key);
	}

	return actions;
}

List<Ref<InputEvent>>::Element *InputMap::_find_event(Action &p_action, const Ref<InputEvent> &p_event, bool p_exact_match, bool *r_pressed, float *r_strength, float *r_raw_strength, int *r_event_index) const {
	ERR_FAIL_COND_V(!p_event.is_valid(), nullptr);

	int i = 0;
	for (List<Ref<InputEvent>>::Element *E = p_action.inputs.front(); E; E = E->next()) {
		int device = E->get()->get_device();
		if (device == ALL_DEVICES || device == p_event->get_device()) {
			if (E->get()->action_match(p_event, p_exact_match, p_action.deadzone, r_pressed, r_strength, r_raw_strength)) {
				if (r_event_index) {
					*r_event_index = i;
				}
				return E;
			}
		}
		i++;
	}

	return nullptr;
}

bool InputMap::has_action(const StringName &p_action) const {
	return input_map.has(p_action);
}

float InputMap::action_get_deadzone(const StringName &p_action) {
	ERR_FAIL_COND_V_MSG(!input_map.has(p_action), 0.0f, suggest_actions(p_action));

	return input_map[p_action].deadzone;
}

void InputMap::action_set_deadzone(const StringName &p_action, float p_deadzone) {
	ERR_FAIL_COND_MSG(!input_map.has(p_action), suggest_actions(p_action));

	input_map[p_action].deadzone = p_deadzone;
}

void InputMap::action_add_event(const StringName &p_action, const Ref<InputEvent> &p_event) {
	ERR_FAIL_COND_MSG(p_event.is_null(), "It's not a reference to a valid InputEvent object.");
	ERR_FAIL_COND_MSG(!input_map.has(p_action), suggest_actions(p_action));
	if (_find_event(input_map[p_action], p_event, true)) {
		return; // Already added.
	}

	input_map[p_action].inputs.push_back(p_event);
}

bool InputMap::action_has_event(const StringName &p_action, const Ref<InputEvent> &p_event) {
	ERR_FAIL_COND_V_MSG(!input_map.has(p_action), false, suggest_actions(p_action));
	return (_find_event(input_map[p_action], p_event, true) != nullptr);
}

void InputMap::action_erase_event(const StringName &p_action, const Ref<InputEvent> &p_event) {
	ERR_FAIL_COND_MSG(!input_map.has(p_action), suggest_actions(p_action));

	List<Ref<InputEvent>>::Element *E = _find_event(input_map[p_action], p_event, true);
	if (E) {
		input_map[p_action].inputs.erase(E);

		if (Input::get_singleton()->is_action_pressed(p_action)) {
			Input::get_singleton()->action_release(p_action);
		}
	}
}

void InputMap::action_erase_events(const StringName &p_action) {
	ERR_FAIL_COND_MSG(!input_map.has(p_action), suggest_actions(p_action));

	input_map[p_action].inputs.clear();
}

TypedArray<InputEvent> InputMap::_action_get_events(const StringName &p_action) {
	TypedArray<InputEvent> ret;
	const List<Ref<InputEvent>> *al = action_get_events(p_action);
	if (al) {
		for (const List<Ref<InputEvent>>::Element *E = al->front(); E; E = E->next()) {
			ret.push_back(E->get());
		}
	}

	return ret;
}

const List<Ref<InputEvent>> *InputMap::action_get_events(const StringName &p_action) {
	HashMap<StringName, Action>::Iterator E = input_map.find(p_action);
	if (!E) {
		return nullptr;
	}

	return &E->value.inputs;
}

bool InputMap::event_is_action(const Ref<InputEvent> &p_event, const StringName &p_action, bool p_exact_match) const {
	return event_get_action_status(p_event, p_action, p_exact_match);
}

int InputMap::event_get_index(const Ref<InputEvent> &p_event, const StringName &p_action, bool p_exact_match) const {
	int index = -1;
	event_get_action_status(p_event, p_action, p_exact_match, nullptr, nullptr, nullptr, &index);
	return index;
}

bool InputMap::event_get_action_status(const Ref<InputEvent> &p_event, const StringName &p_action, bool p_exact_match, bool *r_pressed, float *r_strength, float *r_raw_strength, int *r_event_index) const {
	HashMap<StringName, Action>::Iterator E = input_map.find(p_action);
	ERR_FAIL_COND_V_MSG(!E, false, suggest_actions(p_action));

	Ref<InputEventAction> input_event_action = p_event;
	if (input_event_action.is_valid()) {
		const bool pressed = input_event_action->is_pressed();
		if (r_pressed != nullptr) {
			*r_pressed = pressed;
		}
		const float strength = pressed ? input_event_action->get_strength() : 0.0f;
		if (r_strength != nullptr) {
			*r_strength = strength;
		}
		if (r_raw_strength != nullptr) {
			*r_raw_strength = strength;
		}
		if (r_event_index) {
			if (input_event_action->get_event_index() >= 0) {
				*r_event_index = input_event_action->get_event_index();
			} else {
				*r_event_index = E->value.inputs.size();
			}
		}
		return input_event_action->get_action() == p_action;
	}

	List<Ref<InputEvent>>::Element *event = _find_event(E->value, p_event, p_exact_match, r_pressed, r_strength, r_raw_strength, r_event_index);
	return event != nullptr;
}

const HashMap<StringName, InputMap::Action> &InputMap::get_action_map() const {
	return input_map;
}

void InputMap::load_from_project_settings() {
	input_map.clear();

	List<PropertyInfo> pinfo;
	ProjectSettings::get_singleton()->get_property_list(&pinfo);

	for (const PropertyInfo &pi : pinfo) {
		if (!pi.name.begins_with("input/")) {
			continue;
		}

		String name = pi.name.substr(pi.name.find("/") + 1, pi.name.length());

		Dictionary action = GLOBAL_GET(pi.name);
		float deadzone = action.has("deadzone") ? (float)action["deadzone"] : 0.2f;
		Array events = action["events"];

		add_action(name, deadzone);
		for (int i = 0; i < events.size(); i++) {
			Ref<InputEvent> event = events[i];
			if (event.is_null()) {
				continue;
			}
			action_add_event(name, event);
		}
	}
}

struct _BuiltinActionDisplayName {
	const char *name;
	const char *display_name;
};

static const _BuiltinActionDisplayName _builtin_action_display_names[] = {
	/* clang-format off */
    { "ui_accept",                                     TTRC("Accept") },
    { "ui_select",                                     TTRC("Select") },
    { "ui_cancel",                                     TTRC("Cancel") },
    { "ui_focus_next",                                 TTRC("Focus Next") },
    { "ui_focus_prev",                                 TTRC("Focus Prev") },
    { "ui_left",                                       TTRC("Left") },
    { "ui_right",                                      TTRC("Right") },
    { "ui_up",                                         TTRC("Up") },
    { "ui_down",                                       TTRC("Down") },
    { "ui_page_up",                                    TTRC("Page Up") },
    { "ui_page_down",                                  TTRC("Page Down") },
    { "ui_home",                                       TTRC("Home") },
    { "ui_end",                                        TTRC("End") },
    { "ui_cut",                                        TTRC("Cut") },
    { "ui_copy",                                       TTRC("Copy") },
    { "ui_paste",                                      TTRC("Paste") },
    { "ui_undo",                                       TTRC("Undo") },
    { "ui_redo",                                       TTRC("Redo") },
    { "ui_text_completion_query",                      TTRC("Completion Query") },
    { "ui_text_newline",                               TTRC("New Line") },
    { "ui_text_newline_blank",                         TTRC("New Blank Line") },
    { "ui_text_newline_above",                         TTRC("New Line Above") },
    { "ui_text_indent",                                TTRC("Indent") },
    { "ui_text_dedent",                                TTRC("Dedent") },
    { "ui_text_backspace",                             TTRC("Backspace") },
    { "ui_text_backspace_word",                        TTRC("Backspace Word") },
    { "ui_text_backspace_word.macos",                  TTRC("Backspace Word") },
    { "ui_text_backspace_all_to_left",                 TTRC("Backspace all to Left") },
    { "ui_text_backspace_all_to_left.macos",           TTRC("Backspace all to Left") },
    { "ui_text_delete",                                TTRC("Delete") },
    { "ui_text_delete_word",                           TTRC("Delete Word") },
    { "ui_text_delete_word.macos",                     TTRC("Delete Word") },
    { "ui_text_delete_all_to_right",                   TTRC("Delete all to Right") },
    { "ui_text_delete_all_to_right.macos",             TTRC("Delete all to Right") },
    { "ui_text_caret_left",                            TTRC("Caret Left") },
    { "ui_text_caret_word_left",                       TTRC("Caret Word Left") },
    { "ui_text_caret_word_left.macos",                 TTRC("Caret Word Left") },
    { "ui_text_caret_right",                           TTRC("Caret Right") },
    { "ui_text_caret_word_right",                      TTRC("Caret Word Right") },
    { "ui_text_caret_word_right.macos",                TTRC("Caret Word Right") },
    { "ui_text_caret_up",                              TTRC("Caret Up") },
    { "ui_text_caret_down",                            TTRC("Caret Down") },
    { "ui_text_caret_line_start",                      TTRC("Caret Line Start") },
    { "ui_text_caret_line_start.macos",                TTRC("Caret Line Start") },
    { "ui_text_caret_line_end",                        TTRC("Caret Line End") },
    { "ui_text_caret_line_end.macos",                  TTRC("Caret Line End") },
    { "ui_text_caret_page_up",                         TTRC("Caret Page Up") },
    { "ui_text_caret_page_down",                       TTRC("Caret Page Down") },
    { "ui_text_caret_document_start",                  TTRC("Caret Document Start") },
    { "ui_text_caret_document_start.macos",            TTRC("Caret Document Start") },
    { "ui_text_caret_document_end",                    TTRC("Caret Document End") },
    { "ui_text_caret_document_end.macos",              TTRC("Caret Document End") },
    { "ui_text_caret_add_below",                       TTRC("Caret Add Below") },
    { "ui_text_caret_add_below.macos",                 TTRC("Caret Add Below") },
    { "ui_text_caret_add_above",                       TTRC("Caret Add Above") },
    { "ui_text_caret_add_above.macos",                 TTRC("Caret Add Above") },
    { "ui_text_scroll_up",                             TTRC("Scroll Up") },
    { "ui_text_scroll_up.macos",                       TTRC("Scroll Up") },
    { "ui_text_scroll_down",                           TTRC("Scroll Down") },
    { "ui_text_scroll_down.macos",                     TTRC("Scroll Down") },
    { "ui_text_select_all",                            TTRC("Select All") },
    { "ui_text_select_word_under_caret",               TTRC("Select Word Under Caret") },
    { "ui_text_add_selection_for_next_occurrence",     TTRC("Add Selection for Next Occurrence") },
    { "ui_text_skip_selection_for_next_occurrence",    TTRC("Skip Selection for Next Occurrence") },
    { "ui_text_clear_carets_and_selection",            TTRC("Clear Carets and Selection") },
    { "ui_text_toggle_insert_mode",                    TTRC("Toggle Insert Mode") },
    { "ui_text_submit",                                TTRC("Submit Text") },
    { "ui_graph_duplicate",                            TTRC("Duplicate Nodes") },
    { "ui_graph_delete",                               TTRC("Delete Nodes") },
    { "ui_filedialog_up_one_level",                    TTRC("Go Up One Level") },
    { "ui_filedialog_refresh",                         TTRC("Refresh") },
    { "ui_filedialog_show_hidden",                     TTRC("Show Hidden") },
    { "ui_swap_input_direction ",                      TTRC("Swap Input Direction") },
    { "ui_unicode_start",                              TTRC("Start Unicode Character Input") },
    { "",                                              ""}
	/* clang-format on */
};

String InputMap::get_builtin_display_name(const String &p_name) const {
	int len = sizeof(_builtin_action_display_names) / sizeof(_BuiltinActionDisplayName);

	for (int i = 0; i < len; i++) {
		if (_builtin_action_display_names[i].name == p_name) {
			return RTR(_builtin_action_display_names[i].display_name);
		}
	}

	return p_name;
}

const HashMap<String, List<Ref<InputEvent>>> &InputMap::get_builtins() {
	// Return cache if it has already been built.
	if (default_builtin_cache.size()) {
		return default_builtin_cache;
	}

	List<Ref<InputEvent>> inputs;
	inputs.push_back(InputEventKey::create_reference(Key::ENTER));
	inputs.push_back(InputEventKey::create_reference(Key::KP_ENTER));
	inputs.push_back(InputEventKey::create_reference(Key::SPACE));
	default_builtin_cache.insert("ui_accept", inputs);

	inputs = List<Ref<InputEvent>>();
	inputs.push_back(InputEventJoypadButton::create_reference(JoyButton::Y));
	inputs.push_back(InputEventKey::create_reference(Key::SPACE));
	default_builtin_cache.insert("ui_select", inputs);

	inputs = List<Ref<InputEvent>>();
	inputs.push_back(InputEventKey::create_reference(Key::ESCAPE));
	default_builtin_cache.insert("ui_cancel", inputs);

	inputs = List<Ref<InputEvent>>();
	inputs.push_back(InputEventKey::create_reference(Key::TAB));
	default_builtin_cache.insert("ui_focus_next", inputs);

	inputs = List<Ref<InputEvent>>();
	inputs.push_back(InputEventKey::create_reference(Key::TAB | KeyModifierMask::SHIFT));
	default_builtin_cache.insert("ui_focus_prev", inputs);

	inputs = List<Ref<InputEvent>>();
	inputs.push_back(InputEventKey::create_reference(Key::LEFT));
	inputs.push_back(InputEventJoypadButton::create_reference(JoyButton::DPAD_LEFT));
	inputs.push_back(InputEventJoypadMotion::create_reference(JoyAxis::LEFT_X, -1.0));
	default_builtin_cache.insert("ui_left", inputs);

	inputs = List<Ref<InputEvent>>();
	inputs.push_back(InputEventKey::create_reference(Key::RIGHT));
	inputs.push_back(InputEventJoypadButton::create_reference(JoyButton::DPAD_RIGHT));
	inputs.push_back(InputEventJoypadMotion::create_reference(JoyAxis::LEFT_X, 1.0));
	default_builtin_cache.insert("ui_right", inputs);

	inputs = List<Ref<InputEvent>>();
	inputs.push_back(InputEventKey::create_reference(Key::UP));
	inputs.push_back(InputEventJoypadButton::create_reference(JoyButton::DPAD_UP));
	inputs.push_back(InputEventJoypadMotion::create_reference(JoyAxis::LEFT_Y, -1.0));
	default_builtin_cache.insert("ui_up", inputs);

	inputs = List<Ref<InputEvent>>();
	inputs.push_back(InputEventKey::create_reference(Key::DOWN));
	inputs.push_back(InputEventJoypadButton::create_reference(JoyButton::DPAD_DOWN));
	inputs.push_back(InputEventJoypadMotion::create_reference(JoyAxis::LEFT_Y, 1.0));
	default_builtin_cache.insert("ui_down", inputs);

	inputs = List<Ref<InputEvent>>();
	inputs.push_back(InputEventKey::create_reference(Key::PAGEUP));
	default_builtin_cache.insert("ui_page_up", inputs);

	inputs = List<Ref<InputEvent>>();
	inputs.push_back(InputEventKey::create_reference(Key::PAGEDOWN));
	default_builtin_cache.insert("ui_page_down", inputs);

	inputs = List<Ref<InputEvent>>();
	inputs.push_back(InputEventKey::create_reference(Key::HOME));
	default_builtin_cache.insert("ui_home", inputs);

	inputs = List<Ref<InputEvent>>();
	inputs.push_back(InputEventKey::create_reference(Key::END));
	default_builtin_cache.insert("ui_end", inputs);

	// ///// UI basic Shortcuts /////

	inputs = List<Ref<InputEvent>>();
	inputs.push_back(InputEventKey::create_reference(Key::X | KeyModifierMask::CMD_OR_CTRL));
	inputs.push_back(InputEventKey::create_reference(Key::KEY_DELETE | KeyModifierMask::SHIFT));
	default_builtin_cache.insert("ui_cut", inputs);

	inputs = List<Ref<InputEvent>>();
	inputs.push_back(InputEventKey::create_reference(Key::C | KeyModifierMask::CMD_OR_CTRL));
	inputs.push_back(InputEventKey::create_reference(Key::INSERT | KeyModifierMask::CMD_OR_CTRL));
	default_builtin_cache.insert("ui_copy", inputs);

	inputs = List<Ref<InputEvent>>();
	inputs.push_back(InputEventKey::create_reference(Key::V | KeyModifierMask::CMD_OR_CTRL));
	inputs.push_back(InputEventKey::create_reference(Key::INSERT | KeyModifierMask::SHIFT));
	default_builtin_cache.insert("ui_paste", inputs);

	inputs = List<Ref<InputEvent>>();
	inputs.push_back(InputEventKey::create_reference(Key::Z | KeyModifierMask::CMD_OR_CTRL));
	default_builtin_cache.insert("ui_undo", inputs);

	inputs = List<Ref<InputEvent>>();
	inputs.push_back(InputEventKey::create_reference(Key::Z | KeyModifierMask::CMD_OR_CTRL | KeyModifierMask::SHIFT));
	inputs.push_back(InputEventKey::create_reference(Key::Y | KeyModifierMask::CMD_OR_CTRL));
	default_builtin_cache.insert("ui_redo", inputs);

	// ///// UI Text Input Shortcuts /////
	inputs = List<Ref<InputEvent>>();
	inputs.push_back(InputEventKey::create_reference(Key::SPACE | KeyModifierMask::CTRL));
	default_builtin_cache.insert("ui_text_completion_query", inputs);

	inputs = List<Ref<InputEvent>>();
	inputs.push_back(InputEventKey::create_reference(Key::ENTER));
	inputs.push_back(InputEventKey::create_reference(Key::KP_ENTER));
	default_builtin_cache.insert("ui_text_completion_accept", inputs);

	inputs = List<Ref<InputEvent>>();
	inputs.push_back(InputEventKey::create_reference(Key::TAB));
	default_builtin_cache.insert("ui_text_completion_replace", inputs);

	// Newlines
	inputs = List<Ref<InputEvent>>();
	inputs.push_back(InputEventKey::create_reference(Key::ENTER));
	inputs.push_back(InputEventKey::create_reference(Key::KP_ENTER));
	default_builtin_cache.insert("ui_text_newline", inputs);

	inputs = List<Ref<InputEvent>>();

	inputs.push_back(InputEventKey::create_reference(Key::ENTER | KeyModifierMask::CMD_OR_CTRL));
	inputs.push_back(InputEventKey::create_reference(Key::KP_ENTER | KeyModifierMask::CMD_OR_CTRL));
	default_builtin_cache.insert("ui_text_newline_blank", inputs);

	inputs = List<Ref<InputEvent>>();
	inputs.push_back(InputEventKey::create_reference(Key::ENTER | KeyModifierMask::SHIFT | KeyModifierMask::CMD_OR_CTRL));
	inputs.push_back(InputEventKey::create_reference(Key::KP_ENTER | KeyModifierMask::SHIFT | KeyModifierMask::CMD_OR_CTRL));
	default_builtin_cache.insert("ui_text_newline_above", inputs);

	// Indentation
	inputs = List<Ref<InputEvent>>();
	inputs.push_back(InputEventKey::create_reference(Key::TAB));
	default_builtin_cache.insert("ui_text_indent", inputs);

	inputs = List<Ref<InputEvent>>();
	inputs.push_back(InputEventKey::create_reference(Key::TAB | KeyModifierMask::SHIFT));
	default_builtin_cache.insert("ui_text_dedent", inputs);

	// Text Backspace and Delete
	inputs = List<Ref<InputEvent>>();
	inputs.push_back(InputEventKey::create_reference(Key::BACKSPACE));
	inputs.push_back(InputEventKey::create_reference(Key::BACKSPACE | KeyModifierMask::SHIFT));
	default_builtin_cache.insert("ui_text_backspace", inputs);

	inputs = List<Ref<InputEvent>>();
	inputs.push_back(InputEventKey::create_reference(Key::BACKSPACE | KeyModifierMask::CMD_OR_CTRL));
	default_builtin_cache.insert("ui_text_backspace_word", inputs);

	inputs = List<Ref<InputEvent>>();
	inputs.push_back(InputEventKey::create_reference(Key::BACKSPACE | KeyModifierMask::ALT));
	default_builtin_cache.insert("ui_text_backspace_word.macos", inputs);

	inputs = List<Ref<InputEvent>>();
	default_builtin_cache.insert("ui_text_backspace_all_to_left", inputs);

	inputs = List<Ref<InputEvent>>();
	inputs.push_back(InputEventKey::create_reference(Key::BACKSPACE | KeyModifierMask::CMD_OR_CTRL));
	default_builtin_cache.insert("ui_text_backspace_all_to_left.macos", inputs);

	inputs = List<Ref<InputEvent>>();
	inputs.push_back(InputEventKey::create_reference(Key::KEY_DELETE));
	default_builtin_cache.insert("ui_text_delete", inputs);

	inputs = List<Ref<InputEvent>>();
	inputs.push_back(InputEventKey::create_reference(Key::KEY_DELETE | KeyModifierMask::CMD_OR_CTRL));
	default_builtin_cache.insert("ui_text_delete_word", inputs);

	inputs = List<Ref<InputEvent>>();
	inputs.push_back(InputEventKey::create_reference(Key::KEY_DELETE | KeyModifierMask::ALT));
	default_builtin_cache.insert("ui_text_delete_word.macos", inputs);

	inputs = List<Ref<InputEvent>>();
	default_builtin_cache.insert("ui_text_delete_all_to_right", inputs);

	inputs = List<Ref<InputEvent>>();
	inputs.push_back(InputEventKey::create_reference(Key::KEY_DELETE | KeyModifierMask::CMD_OR_CTRL));
	default_builtin_cache.insert("ui_text_delete_all_to_right.macos", inputs);

	// Text Caret Movement Left/Right

	inputs = List<Ref<InputEvent>>();
	inputs.push_back(InputEventKey::create_reference(Key::LEFT));
	default_builtin_cache.insert("ui_text_caret_left", inputs);

	inputs = List<Ref<InputEvent>>();
	inputs.push_back(InputEventKey::create_reference(Key::LEFT | KeyModifierMask::CMD_OR_CTRL));
	default_builtin_cache.insert("ui_text_caret_word_left", inputs);

	inputs = List<Ref<InputEvent>>();
	inputs.push_back(InputEventKey::create_reference(Key::LEFT | KeyModifierMask::ALT));
	default_builtin_cache.insert("ui_text_caret_word_left.macos", inputs);

	inputs = List<Ref<InputEvent>>();
	inputs.push_back(InputEventKey::create_reference(Key::RIGHT));
	default_builtin_cache.insert("ui_text_caret_right", inputs);

	inputs = List<Ref<InputEvent>>();
	inputs.push_back(InputEventKey::create_reference(Key::RIGHT | KeyModifierMask::CMD_OR_CTRL));
	default_builtin_cache.insert("ui_text_caret_word_right", inputs);

	inputs = List<Ref<InputEvent>>();
	inputs.push_back(InputEventKey::create_reference(Key::RIGHT | KeyModifierMask::ALT));
	default_builtin_cache.insert("ui_text_caret_word_right.macos", inputs);

	// Text Caret Movement Up/Down

	inputs = List<Ref<InputEvent>>();
	inputs.push_back(InputEventKey::create_reference(Key::UP));
	default_builtin_cache.insert("ui_text_caret_up", inputs);

	inputs = List<Ref<InputEvent>>();
	inputs.push_back(InputEventKey::create_reference(Key::DOWN));
	default_builtin_cache.insert("ui_text_caret_down", inputs);

	// Text Caret Movement Line Start/End

	inputs = List<Ref<InputEvent>>();
	inputs.push_back(InputEventKey::create_reference(Key::HOME));
	default_builtin_cache.insert("ui_text_caret_line_start", inputs);

	inputs = List<Ref<InputEvent>>();
	inputs.push_back(InputEventKey::create_reference(Key::A | KeyModifierMask::CTRL));
	inputs.push_back(InputEventKey::create_reference(Key::LEFT | KeyModifierMask::CMD_OR_CTRL));
	inputs.push_back(InputEventKey::create_reference(Key::HOME));
	default_builtin_cache.insert("ui_text_caret_line_start.macos", inputs);

	inputs = List<Ref<InputEvent>>();
	inputs.push_back(InputEventKey::create_reference(Key::END));
	default_builtin_cache.insert("ui_text_caret_line_end", inputs);

	inputs = List<Ref<InputEvent>>();
	inputs.push_back(InputEventKey::create_reference(Key::E | KeyModifierMask::CTRL));
	inputs.push_back(InputEventKey::create_reference(Key::RIGHT | KeyModifierMask::CMD_OR_CTRL));
	inputs.push_back(InputEventKey::create_reference(Key::END));
	default_builtin_cache.insert("ui_text_caret_line_end.macos", inputs);

	// Text Caret Movement Page Up/Down

	inputs = List<Ref<InputEvent>>();
	inputs.push_back(InputEventKey::create_reference(Key::PAGEUP));
	default_builtin_cache.insert("ui_text_caret_page_up", inputs);

	inputs = List<Ref<InputEvent>>();
	inputs.push_back(InputEventKey::create_reference(Key::PAGEDOWN));
	default_builtin_cache.insert("ui_text_caret_page_down", inputs);

	// Text Caret Movement Document Start/End

	inputs = List<Ref<InputEvent>>();
	inputs.push_back(InputEventKey::create_reference(Key::HOME | KeyModifierMask::CMD_OR_CTRL));
	default_builtin_cache.insert("ui_text_caret_document_start", inputs);

	inputs = List<Ref<InputEvent>>();
	inputs.push_back(InputEventKey::create_reference(Key::UP | KeyModifierMask::CMD_OR_CTRL));
	inputs.push_back(InputEventKey::create_reference(Key::HOME | KeyModifierMask::CMD_OR_CTRL));
	default_builtin_cache.insert("ui_text_caret_document_start.macos", inputs);

	inputs = List<Ref<InputEvent>>();
	inputs.push_back(InputEventKey::create_reference(Key::END | KeyModifierMask::CMD_OR_CTRL));
	default_builtin_cache.insert("ui_text_caret_document_end", inputs);

	inputs = List<Ref<InputEvent>>();
	inputs.push_back(InputEventKey::create_reference(Key::DOWN | KeyModifierMask::CMD_OR_CTRL));
	inputs.push_back(InputEventKey::create_reference(Key::END | KeyModifierMask::CMD_OR_CTRL));
	default_builtin_cache.insert("ui_text_caret_document_end.macos", inputs);

	// Text Caret Addition Below/Above

	inputs = List<Ref<InputEvent>>();
	inputs.push_back(InputEventKey::create_reference(Key::DOWN | KeyModifierMask::SHIFT | KeyModifierMask::CMD_OR_CTRL));
	default_builtin_cache.insert("ui_text_caret_add_below", inputs);

	inputs = List<Ref<InputEvent>>();
	inputs.push_back(InputEventKey::create_reference(Key::L | KeyModifierMask::SHIFT | KeyModifierMask::CMD_OR_CTRL));
	default_builtin_cache.insert("ui_text_caret_add_below.macos", inputs);

	inputs = List<Ref<InputEvent>>();
	inputs.push_back(InputEventKey::create_reference(Key::UP | KeyModifierMask::SHIFT | KeyModifierMask::CMD_OR_CTRL));
	default_builtin_cache.insert("ui_text_caret_add_above", inputs);

	inputs = List<Ref<InputEvent>>();
	inputs.push_back(InputEventKey::create_reference(Key::O | KeyModifierMask::SHIFT | KeyModifierMask::CMD_OR_CTRL));
	default_builtin_cache.insert("ui_text_caret_add_above.macos", inputs);

	// Text Scrolling

	inputs = List<Ref<InputEvent>>();
	inputs.push_back(InputEventKey::create_reference(Key::UP | KeyModifierMask::CMD_OR_CTRL));
	default_builtin_cache.insert("ui_text_scroll_up", inputs);

	inputs = List<Ref<InputEvent>>();
	inputs.push_back(InputEventKey::create_reference(Key::UP | KeyModifierMask::CMD_OR_CTRL | KeyModifierMask::ALT));
	default_builtin_cache.insert("ui_text_scroll_up.macos", inputs);

	inputs = List<Ref<InputEvent>>();
	inputs.push_back(InputEventKey::create_reference(Key::DOWN | KeyModifierMask::CMD_OR_CTRL));
	default_builtin_cache.insert("ui_text_scroll_down", inputs);

	inputs = List<Ref<InputEvent>>();
	inputs.push_back(InputEventKey::create_reference(Key::DOWN | KeyModifierMask::CMD_OR_CTRL | KeyModifierMask::ALT));
	default_builtin_cache.insert("ui_text_scroll_down.macos", inputs);

	// Text Misc

	inputs = List<Ref<InputEvent>>();
	inputs.push_back(InputEventKey::create_reference(Key::A | KeyModifierMask::CMD_OR_CTRL));
	default_builtin_cache.insert("ui_text_select_all", inputs);

	inputs = List<Ref<InputEvent>>();
	inputs.push_back(InputEventKey::create_reference(Key::G | KeyModifierMask::ALT));
	default_builtin_cache.insert("ui_text_select_word_under_caret", inputs);

	inputs = List<Ref<InputEvent>>();
	inputs.push_back(InputEventKey::create_reference(Key::G | KeyModifierMask::CTRL | KeyModifierMask::META));
	default_builtin_cache.insert("ui_text_select_word_under_caret.macos", inputs);

	inputs = List<Ref<InputEvent>>();
	inputs.push_back(InputEventKey::create_reference(Key::D | KeyModifierMask::CMD_OR_CTRL));
	default_builtin_cache.insert("ui_text_add_selection_for_next_occurrence", inputs);

	inputs = List<Ref<InputEvent>>();
	inputs.push_back(InputEventKey::create_reference(Key::D | KeyModifierMask::CMD_OR_CTRL | KeyModifierMask::ALT));
	default_builtin_cache.insert("ui_text_skip_selection_for_next_occurrence", inputs);

	inputs = List<Ref<InputEvent>>();
	inputs.push_back(InputEventKey::create_reference(Key::ESCAPE));
	default_builtin_cache.insert("ui_text_clear_carets_and_selection", inputs);

	inputs = List<Ref<InputEvent>>();
	inputs.push_back(InputEventKey::create_reference(Key::INSERT));
	default_builtin_cache.insert("ui_text_toggle_insert_mode", inputs);

	inputs = List<Ref<InputEvent>>();
	inputs.push_back(InputEventKey::create_reference(Key::MENU));
	default_builtin_cache.insert("ui_menu", inputs);

	inputs = List<Ref<InputEvent>>();
	inputs.push_back(InputEventKey::create_reference(Key::ENTER));
	inputs.push_back(InputEventKey::create_reference(Key::KP_ENTER));
	default_builtin_cache.insert("ui_text_submit", inputs);

	inputs = List<Ref<InputEvent>>();
	inputs.push_back(InputEventKey::create_reference(Key::U | KeyModifierMask::CTRL | KeyModifierMask::SHIFT));
	default_builtin_cache.insert("ui_unicode_start", inputs);

	// ///// UI Graph Shortcuts /////

	inputs = List<Ref<InputEvent>>();
	inputs.push_back(InputEventKey::create_reference(Key::D | KeyModifierMask::CMD_OR_CTRL));
	default_builtin_cache.insert("ui_graph_duplicate", inputs);

	inputs = List<Ref<InputEvent>>();
	inputs.push_back(InputEventKey::create_reference(Key::KEY_DELETE));
	default_builtin_cache.insert("ui_graph_delete", inputs);

	// ///// UI File Dialog Shortcuts /////
	inputs = List<Ref<InputEvent>>();
	inputs.push_back(InputEventKey::create_reference(Key::BACKSPACE));
	default_builtin_cache.insert("ui_filedialog_up_one_level", inputs);

	inputs = List<Ref<InputEvent>>();
	inputs.push_back(InputEventKey::create_reference(Key::F5));
	default_builtin_cache.insert("ui_filedialog_refresh", inputs);

	inputs = List<Ref<InputEvent>>();
	inputs.push_back(InputEventKey::create_reference(Key::H));
	default_builtin_cache.insert("ui_filedialog_show_hidden", inputs);

	inputs = List<Ref<InputEvent>>();
	inputs.push_back(InputEventKey::create_reference(Key::QUOTELEFT | KeyModifierMask::CMD_OR_CTRL));
	default_builtin_cache.insert("ui_swap_input_direction", inputs);

	return default_builtin_cache;
}

const HashMap<String, List<Ref<InputEvent>>> &InputMap::get_builtins_with_feature_overrides_applied() {
	if (default_builtin_with_overrides_cache.size() > 0) {
		return default_builtin_with_overrides_cache;
	}

	const HashMap<String, List<Ref<InputEvent>>> &builtins = get_builtins();

	// Get a list of all built in inputs which are valid overrides for the OS
	// Key = builtin name (e.g. ui_accept)
	// Value = override/feature names (e.g. macos, if it was defined as "ui_accept.macos" and the platform supports that feature)
	HashMap<String, Vector<String>> builtins_with_overrides;
	for (const KeyValue<String, List<Ref<InputEvent>>> &E : builtins) {
		String fullname = E.key;

		Vector<String> split = fullname.split(".");
		const String &name = split[0];
		String override_for = split.size() > 1 ? split[1] : String();

		if (!override_for.is_empty() && OS::get_singleton()->has_feature(override_for)) {
			builtins_with_overrides[name].push_back(override_for);
		}
	}

	for (const KeyValue<String, List<Ref<InputEvent>>> &E : builtins) {
		String fullname = E.key;

		Vector<String> split = fullname.split(".");
		const String &name = split[0];
		String override_for = split.size() > 1 ? split[1] : String();

		if (builtins_with_overrides.has(name) && override_for.is_empty()) {
			// Builtin has an override but this particular one is not an override, so skip.
			continue;
		}

		if (!override_for.is_empty() && !OS::get_singleton()->has_feature(override_for)) {
			// OS does not support this override - skip.
			continue;
		}

		default_builtin_with_overrides_cache.insert(name, E.value);
	}

	return default_builtin_with_overrides_cache;
}

void InputMap::load_default() {
	HashMap<String, List<Ref<InputEvent>>> builtins = get_builtins_with_feature_overrides_applied();

	for (const KeyValue<String, List<Ref<InputEvent>>> &E : builtins) {
		String name = E.key;

		add_action(name);

		const List<Ref<InputEvent>> &inputs = E.value;
		for (const List<Ref<InputEvent>>::Element *I = inputs.front(); I; I = I->next()) {
			Ref<InputEventKey> iek = I->get();

			// For the editor, only add keyboard actions.
			if (iek.is_valid()) {
				action_add_event(name, I->get());
			}
		}
	}
}

InputMap::InputMap() {
	ERR_FAIL_COND_MSG(singleton, "Singleton in InputMap already exist.");
	singleton = this;
}

InputMap::~InputMap() {
	singleton = nullptr;
}

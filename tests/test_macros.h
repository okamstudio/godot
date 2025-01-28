/**************************************************************************/
/*  test_macros.h                                                         */
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

#ifndef TEST_MACROS_H
#define TEST_MACROS_H

#include "display_server_mock.h"

#include "core/core_globals.h"
#include "core/input/input_map.h"
#include "core/object/message_queue.h"
#include "core/variant/variant.h"

// See documentation for doctest at:
// https://github.com/onqtam/doctest/blob/master/doc/markdown/readme.md#reference
#include "thirdparty/doctest/doctest.h"

// The test is skipped with this, run pending tests with `--test --no-skip`.
#define TEST_CASE_PENDING(name) TEST_CASE(name *doctest::skip())

// The test case is marked as failed, but does not fail the entire test run.
#define TEST_CASE_MAY_FAIL(name) TEST_CASE(name *doctest::may_fail())

// Provide aliases to conform with Godot naming conventions (see error macros).
#define TEST_COND(cond, ...) DOCTEST_CHECK_FALSE_MESSAGE(cond, __VA_ARGS__)
#define TEST_FAIL(cond, ...) DOCTEST_FAIL(cond, __VA_ARGS__)
#define TEST_FAIL_COND(cond, ...) DOCTEST_REQUIRE_FALSE_MESSAGE(cond, __VA_ARGS__)
#define TEST_FAIL_COND_WARN(cond, ...) DOCTEST_WARN_FALSE_MESSAGE(cond, __VA_ARGS__)

// Temporarily disable error prints to test failure paths.
// This allows to avoid polluting the test summary with error messages.
// The `print_error_enabled` boolean is defined in `core/core_globals.cpp` and
// works at global scope. It's used by various loggers in `should_log()` method,
// which are used by error macros which call into `OS::print_error`, effectively
// disabling any error messages to be printed from the engine side (not tests).
#define ERR_PRINT_OFF CoreGlobals::print_error_enabled = false;
#define ERR_PRINT_ON CoreGlobals::print_error_enabled = true;

// Stringify all `Variant` compatible types for doctest output by default.
// https://github.com/onqtam/doctest/blob/master/doc/markdown/stringification.md

#define DOCTEST_STRINGIFY_VARIANT(m_type)                     \
	template <>                                               \
	struct doctest::StringMaker<m_type> {                     \
		static doctest::String convert(const m_type &p_val) { \
			const Variant val = p_val;                        \
			return val.operator ::String().utf8().get_data(); \
		}                                                     \
	};

#define DOCTEST_STRINGIFY_VARIANT_POINTER(m_type)             \
	template <>                                               \
	struct doctest::StringMaker<m_type> {                     \
		static doctest::String convert(const m_type *p_val) { \
			const Variant val = p_val;                        \
			return val.operator ::String().utf8().get_data(); \
		}                                                     \
	};

DOCTEST_STRINGIFY_VARIANT(Variant);
DOCTEST_STRINGIFY_VARIANT(::String); // Disambiguate from `doctest::String`.

DOCTEST_STRINGIFY_VARIANT(Vector2);
DOCTEST_STRINGIFY_VARIANT(Vector2i);
DOCTEST_STRINGIFY_VARIANT(Rect2);
DOCTEST_STRINGIFY_VARIANT(Rect2i);
DOCTEST_STRINGIFY_VARIANT(Vector3);
DOCTEST_STRINGIFY_VARIANT(Vector3i);
DOCTEST_STRINGIFY_VARIANT(Vector4);
DOCTEST_STRINGIFY_VARIANT(Vector4i);
DOCTEST_STRINGIFY_VARIANT(Transform2D);
DOCTEST_STRINGIFY_VARIANT(Plane);
DOCTEST_STRINGIFY_VARIANT(Projection);
DOCTEST_STRINGIFY_VARIANT(Quaternion);
DOCTEST_STRINGIFY_VARIANT(AABB);
DOCTEST_STRINGIFY_VARIANT(Basis);
DOCTEST_STRINGIFY_VARIANT(Transform3D);

DOCTEST_STRINGIFY_VARIANT(::Color); // Disambiguate from `doctest::Color`.
DOCTEST_STRINGIFY_VARIANT(StringName);
DOCTEST_STRINGIFY_VARIANT(NodePath);
DOCTEST_STRINGIFY_VARIANT(RID);
DOCTEST_STRINGIFY_VARIANT_POINTER(Object);
DOCTEST_STRINGIFY_VARIANT(Callable);
DOCTEST_STRINGIFY_VARIANT(Signal);
DOCTEST_STRINGIFY_VARIANT(Dictionary);
DOCTEST_STRINGIFY_VARIANT(Array);

DOCTEST_STRINGIFY_VARIANT(PackedByteArray);
DOCTEST_STRINGIFY_VARIANT(PackedInt32Array);
DOCTEST_STRINGIFY_VARIANT(PackedInt64Array);
DOCTEST_STRINGIFY_VARIANT(PackedFloat32Array);
DOCTEST_STRINGIFY_VARIANT(PackedFloat64Array);
DOCTEST_STRINGIFY_VARIANT(PackedStringArray);
DOCTEST_STRINGIFY_VARIANT(PackedVector2Array);
DOCTEST_STRINGIFY_VARIANT(PackedVector3Array);
DOCTEST_STRINGIFY_VARIANT(PackedColorArray);
DOCTEST_STRINGIFY_VARIANT(PackedVector4Array);

// Register test commands to be launched from the command-line.
// For instance: REGISTER_TEST_COMMAND("gdscript-parser" &test_parser_func).
// Example usage: `godot --test gdscript-parser`.

typedef void (*TestFunc)();
extern HashMap<String, TestFunc> *test_commands;
int register_test_command(String p_command, TestFunc p_function);

#define REGISTER_TEST_COMMAND(m_command, m_function)                 \
	DOCTEST_GLOBAL_NO_WARNINGS(DOCTEST_ANONYMOUS(DOCTEST_ANON_VAR_), \
			register_test_command(m_command, m_function))

// Utility macros to send an event actions to a given object
// Requires Message Queue and InputMap to be setup.
// SEND_GUI_ACTION    - takes an input map key. e.g SEND_GUI_ACTION("ui_text_newline").
// SEND_GUI_KEY_EVENT - takes a keycode set.   e.g SEND_GUI_KEY_EVENT(Key::A | KeyModifierMask::META).
// SEND_GUI_KEY_UP_EVENT - takes a keycode set.   e.g SEND_GUI_KEY_UP_EVENT(Key::A | KeyModifierMask::META).
// SEND_GUI_MOUSE_BUTTON_EVENT - takes a position, mouse button, mouse mask and modifiers e.g SEND_GUI_MOUSE_BUTTON_EVENT(Vector2(50, 50), MOUSE_BUTTON_NONE, MOUSE_BUTTON_NONE, Key::None);
// SEND_GUI_MOUSE_BUTTON_RELEASED_EVENT - takes a position, mouse button, mouse mask and modifiers e.g SEND_GUI_MOUSE_BUTTON_RELEASED_EVENT(Vector2(50, 50), MOUSE_BUTTON_NONE, MOUSE_BUTTON_NONE, Key::None);
// SEND_GUI_MOUSE_MOTION_EVENT - takes a position, mouse mask and modifiers e.g SEND_GUI_MOUSE_MOTION_EVENT(Vector2(50, 50), MouseButtonMask::LEFT, KeyModifierMask::META);
// SEND_GUI_DOUBLE_CLICK - takes a position and modifiers. e.g SEND_GUI_DOUBLE_CLICK(Vector2(50, 50), KeyModifierMask::META);

#define _SEND_DISPLAYSERVER_EVENT(m_event) ((DisplayServerMock *)(DisplayServer::get_singleton()))->simulate_event(m_event);

#define SEND_GUI_ACTION(m_action)                                                                     \
	{                                                                                                 \
		const List<Ref<InputEvent>> *events = InputMap::get_singleton()->action_get_events(m_action); \
		const List<Ref<InputEvent>>::Element *first_event = events->front();                          \
		Ref<InputEventKey> event = first_event->get()->duplicate();                                   \
		event->set_pressed(true);                                                                     \
		_SEND_DISPLAYSERVER_EVENT(event);                                                             \
		MessageQueue::get_singleton()->flush();                                                       \
	}

#define SEND_GUI_KEY_EVENT(m_input)                                          \
	{                                                                        \
		Ref<InputEventKey> event = InputEventKey::create_reference(m_input); \
		event->set_pressed(true);                                            \
		_SEND_DISPLAYSERVER_EVENT(event);                                    \
		MessageQueue::get_singleton()->flush();                              \
	}

#define SEND_GUI_KEY_UP_EVENT(m_input)                                       \
	{                                                                        \
		Ref<InputEventKey> event = InputEventKey::create_reference(m_input); \
		event->set_pressed(false);                                           \
		_SEND_DISPLAYSERVER_EVENT(event);                                    \
		MessageQueue::get_singleton()->flush();                              \
	}

#define _UPDATE_EVENT_MODIFIERS(m_event, m_modifiers)                                  \
	m_event->set_shift_pressed(((m_modifiers) & KeyModifierMask::SHIFT) != Key::NONE); \
	m_event->set_alt_pressed(((m_modifiers) & KeyModifierMask::ALT) != Key::NONE);     \
	m_event->set_ctrl_pressed(((m_modifiers) & KeyModifierMask::CTRL) != Key::NONE);   \
	m_event->set_meta_pressed(((m_modifiers) & KeyModifierMask::META) != Key::NONE);

#define _CREATE_GUI_MOUSE_EVENT(m_screen_pos, m_input, m_mask, m_modifiers) \
	Ref<InputEventMouseButton> event;                                       \
	event.instantiate();                                                    \
	event->set_position(m_screen_pos);                                      \
	event->set_button_index(m_input);                                       \
	event->set_button_mask(m_mask);                                         \
	event->set_factor(1);                                                   \
	_UPDATE_EVENT_MODIFIERS(event, m_modifiers);                            \
	event->set_pressed(true);

#define _CREATE_GUI_TOUCH_EVENT(m_screen_pos, m_pressed, m_double) \
	Ref<InputEventScreenTouch> event;                              \
	event.instantiate();                                           \
	event->set_position(m_screen_pos);                             \
	event->set_pressed(m_pressed);                                 \
	event->set_double_tap(m_double);

#define SEND_GUI_MOUSE_BUTTON_EVENT(m_screen_pos, m_input, m_mask, m_modifiers) \
	{                                                                           \
		_CREATE_GUI_MOUSE_EVENT(m_screen_pos, m_input, m_mask, m_modifiers);    \
		_SEND_DISPLAYSERVER_EVENT(event);                                       \
		MessageQueue::get_singleton()->flush();                                 \
	}

#define SEND_GUI_MOUSE_BUTTON_RELEASED_EVENT(m_screen_pos, m_input, m_mask, m_modifiers) \
	{                                                                                    \
		_CREATE_GUI_MOUSE_EVENT(m_screen_pos, m_input, m_mask, m_modifiers);             \
		event->set_pressed(false);                                                       \
		_SEND_DISPLAYSERVER_EVENT(event);                                                \
		MessageQueue::get_singleton()->flush();                                          \
	}

#define SEND_GUI_DOUBLE_CLICK(m_screen_pos, m_modifiers)                          \
	{                                                                             \
		_CREATE_GUI_MOUSE_EVENT(m_screen_pos, MouseButton::LEFT, 0, m_modifiers); \
		event->set_double_click(true);                                            \
		_SEND_DISPLAYSERVER_EVENT(event);                                         \
		MessageQueue::get_singleton()->flush();                                   \
	}

// We toggle _print_error_enabled to prevent display server not supported warnings.
#define SEND_GUI_MOUSE_MOTION_EVENT(m_screen_pos, m_mask, m_modifiers) \
	{                                                                  \
		bool errors_enabled = CoreGlobals::print_error_enabled;        \
		CoreGlobals::print_error_enabled = false;                      \
		Ref<InputEventMouseMotion> event;                              \
		event.instantiate();                                           \
		event->set_position(m_screen_pos);                             \
		event->set_button_mask(m_mask);                                \
		_UPDATE_EVENT_MODIFIERS(event, m_modifiers);                   \
		_SEND_DISPLAYSERVER_EVENT(event);                              \
		MessageQueue::get_singleton()->flush();                        \
		CoreGlobals::print_error_enabled = errors_enabled;             \
	}

#define SEND_GUI_TOUCH_EVENT(m_screen_pos, m_pressed, m_double)    \
	{                                                              \
		_CREATE_GUI_TOUCH_EVENT(m_screen_pos, m_pressed, m_double) \
		_SEND_DISPLAYSERVER_EVENT(event);                          \
		MessageQueue::get_singleton()->flush();                    \
	}

// Utility class / macros for testing signals
//
// Use SIGNAL_WATCH(*object, "signal_name") to start watching
// Makes sure to call SIGNAL_UNWATCH(*object, "signal_name") to stop watching in cleanup, this is not done automatically.
//
// The SignalWatcher will capture all signals and their args sent between checks.
//
// Use SIGNAL_CHECK("signal_name"), Vector<Vector<Variant>>), to check the arguments of all fired signals.
// The outer vector is each fired signal, the inner vector the list of arguments for that signal. Order does matter.
//
// Use SIGNAL_CHECK_FALSE("signal_name") to check if a signal was not fired.
//
// Use SIGNAL_DISCARD("signal_name") to discard records all of the given signal, use only in placed you don't need to check.
//
// All signals are automatically discarded between test/sub test cases.

class SignalWatcher : public Object {
private:
	inline static SignalWatcher *singleton;

	/* Equal to: RBMap<String, Vector<Vector<Variant>>> */
	HashMap<String, Array> _signals;
	void _add_signal_entry(const Array &p_args, const String &p_name) {
		if (!_signals.has(p_name)) {
			_signals[p_name] = Array();
		}
		_signals[p_name].push_back(p_args);
	}

	void _signal_callback_zero(const String &p_name) {
		Array args;
		_add_signal_entry(args, p_name);
	}

	void _signal_callback_one(Variant p_arg1, const String &p_name) {
		Array args;
		args.push_back(p_arg1);
		_add_signal_entry(args, p_name);
	}

	void _signal_callback_two(Variant p_arg1, Variant p_arg2, const String &p_name) {
		Array args;
		args.push_back(p_arg1);
		args.push_back(p_arg2);
		_add_signal_entry(args, p_name);
	}

	void _signal_callback_three(Variant p_arg1, Variant p_arg2, Variant p_arg3, const String &p_name) {
		Array args;
		args.push_back(p_arg1);
		args.push_back(p_arg2);
		args.push_back(p_arg3);
		_add_signal_entry(args, p_name);
	}

public:
	static SignalWatcher *get_singleton() { return singleton; }

	void watch_signal(Object *p_object, const String &p_signal) {
		MethodInfo method_info;
		ClassDB::get_signal(p_object->get_class(), p_signal, &method_info);
		switch (method_info.arguments.size()) {
			case 0: {
				p_object->connect(p_signal, callable_mp(this, &SignalWatcher::_signal_callback_zero).bind(p_signal));
			} break;
			case 1: {
				p_object->connect(p_signal, callable_mp(this, &SignalWatcher::_signal_callback_one).bind(p_signal));
			} break;
			case 2: {
				p_object->connect(p_signal, callable_mp(this, &SignalWatcher::_signal_callback_two).bind(p_signal));
			} break;
			case 3: {
				p_object->connect(p_signal, callable_mp(this, &SignalWatcher::_signal_callback_three).bind(p_signal));
			} break;
			default: {
				MESSAGE("Signal ", p_signal, " arg count not supported.");
			} break;
		}
	}

	void unwatch_signal(Object *p_object, const String &p_signal) {
		MethodInfo method_info;
		ClassDB::get_signal(p_object->get_class(), p_signal, &method_info);
		switch (method_info.arguments.size()) {
			case 0: {
				p_object->disconnect(p_signal, callable_mp(this, &SignalWatcher::_signal_callback_zero));
			} break;
			case 1: {
				p_object->disconnect(p_signal, callable_mp(this, &SignalWatcher::_signal_callback_one));
			} break;
			case 2: {
				p_object->disconnect(p_signal, callable_mp(this, &SignalWatcher::_signal_callback_two));
			} break;
			case 3: {
				p_object->disconnect(p_signal, callable_mp(this, &SignalWatcher::_signal_callback_three));
			} break;
			default: {
				MESSAGE("Signal ", p_signal, " arg count not supported.");
			} break;
		}
	}

	bool check(const String &p_name, const Array &p_args) {
		if (!_signals.has(p_name)) {
			MESSAGE("Signal ", p_name, " not emitted");
			return false;
		}

		if (p_args.size() != _signals[p_name].size()) {
			MESSAGE("Signal has " << _signals[p_name] << " expected " << p_args);
			discard_signal(p_name);
			return false;
		}

		bool match = true;
		for (int i = 0; i < p_args.size(); i++) {
			if (((Array)p_args[i]).size() != ((Array)_signals[p_name][i]).size()) {
				MESSAGE("Signal has " << _signals[p_name][i] << " expected " << p_args[i]);
				match = false;
				continue;
			}

			for (int j = 0; j < ((Array)p_args[i]).size(); j++) {
				if (((Array)p_args[i])[j] != ((Array)_signals[p_name][i])[j]) {
					MESSAGE("Signal has " << _signals[p_name][i] << " expected " << p_args[i]);
					match = false;
					break;
				}
			}
		}

		discard_signal(p_name);
		return match;
	}

	bool check_false(const String &p_name) {
		bool has = _signals.has(p_name);
		if (has) {
			MESSAGE("Signal has " << _signals[p_name] << " expected none.");
		}
		discard_signal(p_name);
		return !has;
	}

	void discard_signal(const String &p_name) {
		if (_signals.has(p_name)) {
			_signals.erase(p_name);
		}
	}

	void _clear_signals() {
		_signals.clear();
	}

	SignalWatcher() {
		singleton = this;
	}

	~SignalWatcher() {
		singleton = nullptr;
	}
};

#define SIGNAL_WATCH(m_object, m_signal) SignalWatcher::get_singleton()->watch_signal(m_object, m_signal);
#define SIGNAL_UNWATCH(m_object, m_signal) SignalWatcher::get_singleton()->unwatch_signal(m_object, m_signal);

#define SIGNAL_CHECK(m_signal, m_args) CHECK(SignalWatcher::get_singleton()->check(m_signal, m_args));
#define SIGNAL_CHECK_FALSE(m_signal) CHECK(SignalWatcher::get_singleton()->check_false(m_signal));
#define SIGNAL_DISCARD(m_signal) SignalWatcher::get_singleton()->discard_signal(m_signal);

#define MULTICHECK_STRING_EQ(m_obj, m_func, m_param1, m_eq) \
	CHECK(m_obj.m_func(m_param1) == m_eq);                  \
	CHECK(m_obj.m_func(U##m_param1) == m_eq);               \
	CHECK(m_obj.m_func(L##m_param1) == m_eq);               \
	CHECK(m_obj.m_func(String(m_param1)) == m_eq);

#define MULTICHECK_STRING_INT_EQ(m_obj, m_func, m_param1, m_param2, m_eq) \
	CHECK(m_obj.m_func(m_param1, m_param2) == m_eq);                      \
	CHECK(m_obj.m_func(U##m_param1, m_param2) == m_eq);                   \
	CHECK(m_obj.m_func(L##m_param1, m_param2) == m_eq);                   \
	CHECK(m_obj.m_func(String(m_param1), m_param2) == m_eq);

#define MULTICHECK_STRING_INT_INT_EQ(m_obj, m_func, m_param1, m_param2, m_param3, m_eq) \
	CHECK(m_obj.m_func(m_param1, m_param2, m_param3) == m_eq);                          \
	CHECK(m_obj.m_func(U##m_param1, m_param2, m_param3) == m_eq);                       \
	CHECK(m_obj.m_func(L##m_param1, m_param2, m_param3) == m_eq);                       \
	CHECK(m_obj.m_func(String(m_param1), m_param2, m_param3) == m_eq);

#define MULTICHECK_STRING_STRING_EQ(m_obj, m_func, m_param1, m_param2, m_eq) \
	CHECK(m_obj.m_func(m_param1, m_param2) == m_eq);                         \
	CHECK(m_obj.m_func(U##m_param1, U##m_param2) == m_eq);                   \
	CHECK(m_obj.m_func(L##m_param1, L##m_param2) == m_eq);                   \
	CHECK(m_obj.m_func(String(m_param1), String(m_param2)) == m_eq);

#define MULTICHECK_GET_SLICE(m_obj, m_param1, m_slices)                 \
	for (int i = 0; i < m_obj.get_slice_count(m_param1); ++i) {         \
		CHECK(m_obj.get_slice(m_param1, i) == m_slices[i]);             \
	}                                                                   \
	for (int i = 0; i < m_obj.get_slice_count(U##m_param1); ++i) {      \
		CHECK(m_obj.get_slice(U##m_param1, i) == m_slices[i]);          \
	}                                                                   \
	for (int i = 0; i < m_obj.get_slice_count(L##m_param1); ++i) {      \
		CHECK(m_obj.get_slice(L##m_param1, i) == m_slices[i]);          \
	}                                                                   \
	for (int i = 0; i < m_obj.get_slice_count(String(m_param1)); ++i) { \
		CHECK(m_obj.get_slice(String(m_param1), i) == m_slices[i]);     \
	}

#define MULTICHECK_SPLIT(m_obj, m_func, m_param1, m_param2, m_param3, m_slices, m_expected_size) \
	do {                                                                                         \
		Vector<String> string_list;                                                              \
                                                                                                 \
		string_list = m_obj.m_func(m_param1, m_param2, m_param3);                                \
		CHECK(m_expected_size == string_list.size());                                            \
		for (int i = 0; i < string_list.size(); ++i) {                                           \
			CHECK(string_list[i] == m_slices[i]);                                                \
		}                                                                                        \
                                                                                                 \
		string_list = m_obj.m_func(U##m_param1, m_param2, m_param3);                             \
		CHECK(m_expected_size == string_list.size());                                            \
		for (int i = 0; i < string_list.size(); ++i) {                                           \
			CHECK(string_list[i] == m_slices[i]);                                                \
		}                                                                                        \
                                                                                                 \
		string_list = m_obj.m_func(L##m_param1, m_param2, m_param3);                             \
		CHECK(m_expected_size == string_list.size());                                            \
		for (int i = 0; i < string_list.size(); ++i) {                                           \
			CHECK(string_list[i] == m_slices[i]);                                                \
		}                                                                                        \
                                                                                                 \
		string_list = m_obj.m_func(String(m_param1), m_param2, m_param3);                        \
		CHECK(m_expected_size == string_list.size());                                            \
		for (int i = 0; i < string_list.size(); ++i) {                                           \
			CHECK(string_list[i] == m_slices[i]);                                                \
		}                                                                                        \
	} while (false)

#endif // TEST_MACROS_H

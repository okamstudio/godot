/**************************************************************************/
/*  input_map.h                                                           */
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

#ifndef INPUT_MAP_H
#define INPUT_MAP_H

#include "core/input/input_event.h"
#include "core/object/class_db.h"
#include "core/object/object.h"
#include "core/templates/hash_map.h"

template <typename T>
class TypedArray;

class InputMap : public Object {
	GDCLASS(InputMap, Object);

public:
	/**
	 * A special value used to signify that a given Action can be triggered by any device
	 */
	static int ALL_DEVICES;

	struct Action {
		int id;
		float deadzone;
		List<Ref<InputEvent>> inputs;
	};

private:
	static InputMap *singleton;

	mutable HashMap<StringName, Action> input_map;
	HashMap<String, List<Ref<InputEvent>>> default_builtin_cache;
	HashMap<String, List<Ref<InputEvent>>> default_builtin_with_overrides_cache;

	List<Ref<InputEvent>>::Element *_find_event(Action &p_action, const Ref<InputEvent> &p_event, bool p_exact_match = false, bool *r_pressed = nullptr, float *r_strength = nullptr, float *r_raw_strength = nullptr, int *r_event_index = nullptr) const;

	TypedArray<InputEvent> _action_get_events(const StringName &p_action);
	TypedArray<StringName> _get_actions();

protected:
	static void _bind_methods();

#ifndef DISABLE_DEPRECATED
	void _add_action_bind_compat_97281(const StringName &p_action, float p_deadzone = 0.5);
	static void _bind_compatibility_methods();
#endif // DISABLE_DEPRECATED

public:
	static _FORCE_INLINE_ InputMap *get_singleton() { return singleton; }

	bool has_action(const StringName &p_action) const;
	List<StringName> get_actions() const;
	void add_action(const StringName &p_action, float p_deadzone = 0.2);
	void erase_action(const StringName &p_action);

	float action_get_deadzone(const StringName &p_action);
	void action_set_deadzone(const StringName &p_action, float p_deadzone);
	void action_add_event(const StringName &p_action, const Ref<InputEvent> &p_event);
	bool action_has_event(const StringName &p_action, const Ref<InputEvent> &p_event);
	void action_erase_event(const StringName &p_action, const Ref<InputEvent> &p_event);
	void action_erase_events(const StringName &p_action);

	const List<Ref<InputEvent>> *action_get_events(const StringName &p_action);
	bool event_is_action(const Ref<InputEvent> &p_event, const StringName &p_action, bool p_exact_match = false) const;
	int event_get_index(const Ref<InputEvent> &p_event, const StringName &p_action, bool p_exact_match = false) const;
	bool event_get_action_status(const Ref<InputEvent> &p_event, const StringName &p_action, bool p_exact_match = false, bool *r_pressed = nullptr, float *r_strength = nullptr, float *r_raw_strength = nullptr, int *r_event_index = nullptr) const;

	const HashMap<StringName, Action> &get_action_map() const;
	void load_from_project_settings();
	void load_default();

	String suggest_actions(const StringName &p_action) const;

#ifdef TOOLS_ENABLED
	virtual void get_argument_options(const StringName &p_function, int p_idx, List<String> *r_options) const override;
#endif

	String get_builtin_display_name(const String &p_name) const;
	// Use an Ordered Map so insertion order is preserved. We want the elements to be 'grouped' somewhat.
	const HashMap<String, List<Ref<InputEvent>>> &get_builtins();
	const HashMap<String, List<Ref<InputEvent>>> &get_builtins_with_feature_overrides_applied();

	InputMap();
	~InputMap();
};

#endif // INPUT_MAP_H

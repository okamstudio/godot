/**************************************************************************/
/*  script_instance.h                                                     */
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

#include "core/object/ref_counted.h"

class Script;
class ScriptLanguage;

class ScriptInstance {
public:
	virtual bool set(const StringName &p_name, const Variant &p_value) = 0;
	virtual bool get(const StringName &p_name, Variant &r_ret) const = 0;
	virtual void get_property_list(List<PropertyInfo> *p_properties) const = 0;
	virtual Variant::Type get_property_type(const StringName &p_name, bool *r_is_valid = nullptr) const = 0;
	virtual void validate_property(PropertyInfo &p_property) const = 0;

	virtual bool property_can_revert(const StringName &p_name) const = 0;
	virtual bool property_get_revert(const StringName &p_name, Variant &r_ret) const = 0;

	virtual Object *get_owner() { return nullptr; }
	virtual void get_property_state(List<Pair<StringName, Variant>> &state);

	virtual void get_method_list(List<MethodInfo> *p_list) const = 0;
	virtual bool has_method(const StringName &p_method) const = 0;

	virtual int get_method_argument_count(const StringName &p_method, bool *r_is_valid = nullptr) const;

	virtual Variant callp(const StringName &p_method, const Variant **p_args, int p_argcount, Callable::CallError &r_error) = 0;

	template <typename... VarArgs>
	Variant call(const StringName &p_method, VarArgs... p_args) {
		Variant args[sizeof...(p_args) + 1] = { p_args..., Variant() }; // +1 makes sure zero sized arrays are also supported.
		const Variant *argptrs[sizeof...(p_args) + 1];
		for (uint32_t i = 0; i < sizeof...(p_args); i++) {
			argptrs[i] = &args[i];
		}
		Callable::CallError cerr;
		return callp(p_method, sizeof...(p_args) == 0 ? nullptr : (const Variant **)argptrs, sizeof...(p_args), cerr);
	}

	virtual Variant call_const(const StringName &p_method, const Variant **p_args, int p_argcount, Callable::CallError &r_error); // implement if language supports const functions
	virtual void notification(int p_notification, bool p_reversed = false) = 0;
	virtual String to_string(bool *r_valid) {
		if (r_valid) {
			*r_valid = false;
		}
		return String();
	}

	//this is used by script languages that keep a reference counter of their own
	//you can make Ref<> not die when it reaches zero, so deleting the reference
	//depends entirely from the script

	virtual void refcount_incremented() {}
	virtual bool refcount_decremented() { return true; } //return true if it can die

	virtual Ref<Script> get_script() const = 0;

	virtual bool is_placeholder() const { return false; }

	virtual void property_set_fallback(const StringName &p_name, const Variant &p_value, bool *r_valid);
	virtual Variant property_get_fallback(const StringName &p_name, bool *r_valid);

	virtual const Variant get_rpc_config() const;

	virtual ScriptLanguage *get_language() = 0;
	virtual ~ScriptInstance();
};

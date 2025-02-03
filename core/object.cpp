/**************************************************************************/
/*  object.cpp                                                            */
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

#include "object.h"

#include "core/class_db.h"
#include "core/core_string_names.h"
#include "core/message_queue.h"
#include "core/object_rc.h"
#include "core/os/os.h"
#include "core/print_string.h"
#include "core/resource.h"
#include "core/script_language.h"
#include "core/translation.h"

#ifdef DEBUG_ENABLED

struct _ObjectDebugLock {
	ObjectID obj_id;

	_ObjectDebugLock(Object *p_obj) {
		obj_id = p_obj->get_instance_id();
		p_obj->_lock_index.ref();
	}
	~_ObjectDebugLock() {
		Object *obj_ptr = ObjectDB::get_instance(obj_id);
		if (likely(obj_ptr)) {
			obj_ptr->_lock_index.unref();
		}
	}
};

#define OBJ_DEBUG_LOCK _ObjectDebugLock _debug_lock(this);

#else

#define OBJ_DEBUG_LOCK

#endif

PropertyInfo::operator Dictionary() const {
	Dictionary d;
	d["name"] = name;
	d["class_name"] = class_name;
	d["type"] = type;
	d["hint"] = hint;
	d["hint_string"] = hint_string;
	d["usage"] = usage;
	return d;
}

PropertyInfo PropertyInfo::from_dict(const Dictionary &p_dict) {
	PropertyInfo pi;

	if (p_dict.has("type")) {
		pi.type = Variant::Type(int(p_dict["type"]));
	}

	if (p_dict.has("name")) {
		pi.name = p_dict["name"];
	}

	if (p_dict.has("class_name")) {
		pi.class_name = p_dict["class_name"];
	}

	if (p_dict.has("hint")) {
		pi.hint = PropertyHint(int(p_dict["hint"]));
	}

	if (p_dict.has("hint_string")) {
		pi.hint_string = p_dict["hint_string"];
	}

	if (p_dict.has("usage")) {
		pi.usage = p_dict["usage"];
	}

	return pi;
}

Array convert_property_list(const List<PropertyInfo> *p_list) {
	Array va;
	for (const List<PropertyInfo>::Element *E = p_list->front(); E; E = E->next()) {
		va.push_back(Dictionary(E->get()));
	}

	return va;
}

MethodInfo::operator Dictionary() const {
	Dictionary d;
	d["name"] = name;
	d["args"] = convert_property_list(&arguments);
	Array da;
	for (int i = 0; i < default_arguments.size(); i++) {
		da.push_back(default_arguments[i]);
	}
	d["default_args"] = da;
	d["flags"] = flags;
	d["id"] = id;
	Dictionary r = return_val;
	d["return"] = r;
	return d;
}

MethodInfo::MethodInfo() :
		flags(METHOD_FLAG_NORMAL),
		id(0) {
}

MethodInfo MethodInfo::from_dict(const Dictionary &p_dict) {
	MethodInfo mi;

	if (p_dict.has("name")) {
		mi.name = p_dict["name"];
	}
	Array args;
	if (p_dict.has("args")) {
		args = p_dict["args"];
	}

	for (int i = 0; i < args.size(); i++) {
		Dictionary d = args[i];
		mi.arguments.push_back(PropertyInfo::from_dict(d));
	}
	Array defargs;
	if (p_dict.has("default_args")) {
		defargs = p_dict["default_args"];
	}
	for (int i = 0; i < defargs.size(); i++) {
		mi.default_arguments.push_back(defargs[i]);
	}

	if (p_dict.has("return")) {
		mi.return_val = PropertyInfo::from_dict(p_dict["return"]);
	}

	if (p_dict.has("flags")) {
		mi.flags = p_dict["flags"];
	}

	return mi;
}

MethodInfo::MethodInfo(const String &p_name) :
		name(p_name),
		flags(METHOD_FLAG_NORMAL),
		id(0) {
}
MethodInfo::MethodInfo(const String &p_name, const PropertyInfo &p_param1) :
		name(p_name),
		flags(METHOD_FLAG_NORMAL),
		id(0) {
	arguments.push_back(p_param1);
}
MethodInfo::MethodInfo(const String &p_name, const PropertyInfo &p_param1, const PropertyInfo &p_param2) :
		name(p_name),
		flags(METHOD_FLAG_NORMAL),
		id(0) {
	arguments.push_back(p_param1);
	arguments.push_back(p_param2);
}

MethodInfo::MethodInfo(const String &p_name, const PropertyInfo &p_param1, const PropertyInfo &p_param2, const PropertyInfo &p_param3) :
		name(p_name),
		flags(METHOD_FLAG_NORMAL),
		id(0) {
	arguments.push_back(p_param1);
	arguments.push_back(p_param2);
	arguments.push_back(p_param3);
}

MethodInfo::MethodInfo(const String &p_name, const PropertyInfo &p_param1, const PropertyInfo &p_param2, const PropertyInfo &p_param3, const PropertyInfo &p_param4) :
		name(p_name),
		flags(METHOD_FLAG_NORMAL),
		id(0) {
	arguments.push_back(p_param1);
	arguments.push_back(p_param2);
	arguments.push_back(p_param3);
	arguments.push_back(p_param4);
}

MethodInfo::MethodInfo(const String &p_name, const PropertyInfo &p_param1, const PropertyInfo &p_param2, const PropertyInfo &p_param3, const PropertyInfo &p_param4, const PropertyInfo &p_param5) :
		name(p_name),
		flags(METHOD_FLAG_NORMAL),
		id(0) {
	arguments.push_back(p_param1);
	arguments.push_back(p_param2);
	arguments.push_back(p_param3);
	arguments.push_back(p_param4);
	arguments.push_back(p_param5);
}

MethodInfo::MethodInfo(Variant::Type ret) :
		flags(METHOD_FLAG_NORMAL),
		id(0) {
	return_val.type = ret;
}

MethodInfo::MethodInfo(Variant::Type ret, const String &p_name) :
		name(p_name),
		flags(METHOD_FLAG_NORMAL),
		id(0) {
	return_val.type = ret;
}
MethodInfo::MethodInfo(Variant::Type ret, const String &p_name, const PropertyInfo &p_param1) :
		name(p_name),
		flags(METHOD_FLAG_NORMAL),
		id(0) {
	return_val.type = ret;
	arguments.push_back(p_param1);
}
MethodInfo::MethodInfo(Variant::Type ret, const String &p_name, const PropertyInfo &p_param1, const PropertyInfo &p_param2) :
		name(p_name),
		flags(METHOD_FLAG_NORMAL),
		id(0) {
	return_val.type = ret;
	arguments.push_back(p_param1);
	arguments.push_back(p_param2);
}

MethodInfo::MethodInfo(Variant::Type ret, const String &p_name, const PropertyInfo &p_param1, const PropertyInfo &p_param2, const PropertyInfo &p_param3) :
		name(p_name),
		flags(METHOD_FLAG_NORMAL),
		id(0) {
	return_val.type = ret;
	arguments.push_back(p_param1);
	arguments.push_back(p_param2);
	arguments.push_back(p_param3);
}

MethodInfo::MethodInfo(Variant::Type ret, const String &p_name, const PropertyInfo &p_param1, const PropertyInfo &p_param2, const PropertyInfo &p_param3, const PropertyInfo &p_param4) :
		name(p_name),
		flags(METHOD_FLAG_NORMAL),
		id(0) {
	return_val.type = ret;
	arguments.push_back(p_param1);
	arguments.push_back(p_param2);
	arguments.push_back(p_param3);
	arguments.push_back(p_param4);
}

MethodInfo::MethodInfo(Variant::Type ret, const String &p_name, const PropertyInfo &p_param1, const PropertyInfo &p_param2, const PropertyInfo &p_param3, const PropertyInfo &p_param4, const PropertyInfo &p_param5) :
		name(p_name),
		flags(METHOD_FLAG_NORMAL),
		id(0) {
	return_val.type = ret;
	arguments.push_back(p_param1);
	arguments.push_back(p_param2);
	arguments.push_back(p_param3);
	arguments.push_back(p_param4);
	arguments.push_back(p_param5);
}

MethodInfo::MethodInfo(const PropertyInfo &p_ret, const String &p_name) :
		name(p_name),
		return_val(p_ret),
		flags(METHOD_FLAG_NORMAL),
		id(0) {
}

MethodInfo::MethodInfo(const PropertyInfo &p_ret, const String &p_name, const PropertyInfo &p_param1) :
		name(p_name),
		return_val(p_ret),
		flags(METHOD_FLAG_NORMAL),
		id(0) {
	arguments.push_back(p_param1);
}

MethodInfo::MethodInfo(const PropertyInfo &p_ret, const String &p_name, const PropertyInfo &p_param1, const PropertyInfo &p_param2) :
		name(p_name),
		return_val(p_ret),
		flags(METHOD_FLAG_NORMAL),
		id(0) {
	arguments.push_back(p_param1);
	arguments.push_back(p_param2);
}

MethodInfo::MethodInfo(const PropertyInfo &p_ret, const String &p_name, const PropertyInfo &p_param1, const PropertyInfo &p_param2, const PropertyInfo &p_param3) :
		name(p_name),
		return_val(p_ret),
		flags(METHOD_FLAG_NORMAL),
		id(0) {
	arguments.push_back(p_param1);
	arguments.push_back(p_param2);
	arguments.push_back(p_param3);
}

MethodInfo::MethodInfo(const PropertyInfo &p_ret, const String &p_name, const PropertyInfo &p_param1, const PropertyInfo &p_param2, const PropertyInfo &p_param3, const PropertyInfo &p_param4) :
		name(p_name),
		return_val(p_ret),
		flags(METHOD_FLAG_NORMAL),
		id(0) {
	arguments.push_back(p_param1);
	arguments.push_back(p_param2);
	arguments.push_back(p_param3);
	arguments.push_back(p_param4);
}

MethodInfo::MethodInfo(const PropertyInfo &p_ret, const String &p_name, const PropertyInfo &p_param1, const PropertyInfo &p_param2, const PropertyInfo &p_param3, const PropertyInfo &p_param4, const PropertyInfo &p_param5) :
		name(p_name),
		return_val(p_ret),
		flags(METHOD_FLAG_NORMAL),
		id(0) {
	arguments.push_back(p_param1);
	arguments.push_back(p_param2);
	arguments.push_back(p_param3);
	arguments.push_back(p_param4);
	arguments.push_back(p_param5);
}

Object::Connection::operator Variant() const {
	Dictionary d;
	d["source"] = source;
	d["signal"] = signal;
	d["target"] = target;
	d["method"] = method;
	d["flags"] = flags;
	d["binds"] = binds;
	return d;
}

bool Object::Connection::operator<(const Connection &p_conn) const {
	if (source == p_conn.source) {
		if (signal == p_conn.signal) {
			if (target == p_conn.target) {
				return method < p_conn.method;
			} else {
				return target->get_instance_id() < p_conn.target->get_instance_id();
			}
		} else {
			return signal < p_conn.signal;
		}
	} else {
		return source->get_instance_id() < p_conn.source->get_instance_id();
	}
}
Object::Connection::Connection(const Variant &p_variant) {
	Dictionary d = p_variant;
	if (d.has("source")) {
		source = d["source"];
	}
	if (d.has("signal")) {
		signal = d["signal"];
	}
	if (d.has("target")) {
		target = d["target"];
	}
	if (d.has("method")) {
		method = d["method"];
	}
	if (d.has("flags")) {
		flags = d["flags"];
	}
	if (d.has("binds")) {
		binds = d["binds"];
	}
}

bool Object::_predelete() {
	_predelete_ok = 1;
	notification(NOTIFICATION_PREDELETE, true);
	if (_predelete_ok) {
		_class_ptr = nullptr; //must restore so destructors can access class ptr correctly
	}
	return _predelete_ok;
}

void Object::_postinitialize() {
	_class_ptr = _get_class_namev();
	_initialize_classv();
	notification(NOTIFICATION_POSTINITIALIZE);
}

void Object::get_valid_parents_static(List<String> *p_parents) {
}
void Object::_get_valid_parents_static(List<String> *p_parents) {
}

void Object::set(const StringName &p_name, const Variant &p_value, bool *r_valid) {
#ifdef TOOLS_ENABLED

	_edited = true;
#endif

	if (script_instance) {
		if (script_instance->set(p_name, p_value)) {
			if (r_valid) {
				*r_valid = true;
			}
			return;
		}
	}

	//try built-in setgetter
	{
		if (ClassDB::set_property(this, p_name, p_value, r_valid)) {
			/*
			if (r_valid)
				*r_valid=true;
			*/
			return;
		}
	}

	if (p_name == CoreStringNames::get_singleton()->_script) {
		set_script(p_value);
		if (r_valid) {
			*r_valid = true;
		}
		return;

	} else if (p_name == CoreStringNames::get_singleton()->_meta) {
		//set_meta(p_name,p_value);
		metadata = p_value.duplicate();
		if (r_valid) {
			*r_valid = true;
		}
		return;
	}

	//something inside the object... :|
	bool success = _setv(p_name, p_value);
	if (success) {
		if (r_valid) {
			*r_valid = true;
		}
		return;
	}

	{
		bool valid;
		setvar(p_name, p_value, &valid);
		if (valid) {
			if (r_valid) {
				*r_valid = true;
			}
			return;
		}
	}

#ifdef TOOLS_ENABLED
	if (script_instance) {
		bool valid;
		script_instance->property_set_fallback(p_name, p_value, &valid);
		if (valid) {
			if (r_valid) {
				*r_valid = true;
			}
			return;
		}
	}
#endif

	if (r_valid) {
		*r_valid = false;
	}
}

Variant Object::get(const StringName &p_name, bool *r_valid) const {
	Variant ret;

	if (script_instance) {
		if (script_instance->get(p_name, ret)) {
			if (r_valid) {
				*r_valid = true;
			}
			return ret;
		}
	}

	//try built-in setgetter
	{
		if (ClassDB::get_property(const_cast<Object *>(this), p_name, ret)) {
			if (r_valid) {
				*r_valid = true;
			}
			return ret;
		}
	}

	if (p_name == CoreStringNames::get_singleton()->_script) {
		ret = get_script();
		if (r_valid) {
			*r_valid = true;
		}
		return ret;

	} else if (p_name == CoreStringNames::get_singleton()->_meta) {
		ret = metadata;
		if (r_valid) {
			*r_valid = true;
		}
		return ret;

	} else {
		//something inside the object... :|
		bool success = _getv(p_name, ret);
		if (success) {
			if (r_valid) {
				*r_valid = true;
			}
			return ret;
		}

		//if nothing else, use getvar
		{
			bool valid;
			ret = getvar(p_name, &valid);
			if (valid) {
				if (r_valid) {
					*r_valid = true;
				}
				return ret;
			}
		}

#ifdef TOOLS_ENABLED
		if (script_instance) {
			bool valid;
			ret = script_instance->property_get_fallback(p_name, &valid);
			if (valid) {
				if (r_valid) {
					*r_valid = true;
				}
				return ret;
			}
		}
#endif

		if (r_valid) {
			*r_valid = false;
		}
		return Variant();
	}
}

void Object::set_indexed(const Vector<StringName> &p_names, const Variant &p_value, bool *r_valid) {
	if (p_names.empty()) {
		if (r_valid) {
			*r_valid = false;
		}
		return;
	}
	if (p_names.size() == 1) {
		set(p_names[0], p_value, r_valid);
		return;
	}

	bool valid = false;
	if (!r_valid) {
		r_valid = &valid;
	}

	List<Variant> value_stack;

	value_stack.push_back(get(p_names[0], r_valid));

	if (!*r_valid) {
		value_stack.clear();
		return;
	}

	for (int i = 1; i < p_names.size() - 1; i++) {
		value_stack.push_back(value_stack.back()->get().get_named(p_names[i], r_valid));

		if (!*r_valid) {
			value_stack.clear();
			return;
		}
	}

	value_stack.push_back(p_value); // p_names[p_names.size() - 1]

	for (int i = p_names.size() - 1; i > 0; i--) {
		value_stack.back()->prev()->get().set_named(p_names[i], value_stack.back()->get(), r_valid);
		value_stack.pop_back();

		if (!*r_valid) {
			value_stack.clear();
			return;
		}
	}

	set(p_names[0], value_stack.back()->get(), r_valid);
	value_stack.pop_back();

	ERR_FAIL_COND(!value_stack.empty());
}

Variant Object::get_indexed(const Vector<StringName> &p_names, bool *r_valid) const {
	if (p_names.empty()) {
		if (r_valid) {
			*r_valid = false;
		}
		return Variant();
	}
	bool valid = false;

	Variant current_value = get(p_names[0], &valid);
	for (int i = 1; i < p_names.size(); i++) {
		current_value = current_value.get_named(p_names[i], &valid);

		if (!valid) {
			break;
		}
	}
	if (r_valid) {
		*r_valid = valid;
	}

	return current_value;
}

void Object::get_property_list(List<PropertyInfo> *p_list, bool p_reversed) const {
	if (script_instance && p_reversed) {
		p_list->push_back(PropertyInfo(Variant::NIL, "Script Variables", PROPERTY_HINT_NONE, String(), PROPERTY_USAGE_CATEGORY));
		script_instance->get_property_list(p_list);
	}

	_get_property_listv(p_list, p_reversed);

	if (!is_class("Script")) { // can still be set, but this is for userfriendlyness
		p_list->push_back(PropertyInfo(Variant::OBJECT, "script", PROPERTY_HINT_RESOURCE_TYPE, "Script", PROPERTY_USAGE_DEFAULT));
	}
	if (!metadata.empty()) {
		p_list->push_back(PropertyInfo(Variant::DICTIONARY, "__meta__", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NOEDITOR | PROPERTY_USAGE_INTERNAL));
	}
	if (script_instance && !p_reversed) {
		p_list->push_back(PropertyInfo(Variant::NIL, "Script Variables", PROPERTY_HINT_NONE, String(), PROPERTY_USAGE_CATEGORY));
		script_instance->get_property_list(p_list);
	}
}

void Object::_validate_property(PropertyInfo &property) const {
}

void Object::get_method_list(List<MethodInfo> *p_list) const {
	ClassDB::get_method_list(get_class_name(), p_list);
	if (script_instance) {
		script_instance->get_method_list(p_list);
	}
}

Variant Object::_call_bind(const Variant **p_args, int p_argcount, Variant::CallError &r_error) {
	if (p_argcount < 1) {
		r_error.error = Variant::CallError::CALL_ERROR_TOO_FEW_ARGUMENTS;
		r_error.argument = 0;
		return Variant();
	}

	if (p_args[0]->get_type() != Variant::STRING) {
		r_error.error = Variant::CallError::CALL_ERROR_INVALID_ARGUMENT;
		r_error.argument = 0;
		r_error.expected = Variant::STRING;
		return Variant();
	}

	StringName method = *p_args[0];

	return call(method, &p_args[1], p_argcount - 1, r_error);
}

Variant Object::_call_deferred_bind(const Variant **p_args, int p_argcount, Variant::CallError &r_error) {
	if (p_argcount < 1) {
		r_error.error = Variant::CallError::CALL_ERROR_TOO_FEW_ARGUMENTS;
		r_error.argument = 0;
		return Variant();
	}

	if (p_args[0]->get_type() != Variant::STRING) {
		r_error.error = Variant::CallError::CALL_ERROR_INVALID_ARGUMENT;
		r_error.argument = 0;
		r_error.expected = Variant::STRING;
		return Variant();
	}

	r_error.error = Variant::CallError::CALL_OK;

	StringName method = *p_args[0];

	MessageQueue::get_singleton()->push_call(get_instance_id(), method, &p_args[1], p_argcount - 1, true);

	return Variant();
}

#ifdef DEBUG_ENABLED
static void _test_call_error(const StringName &p_func, const Variant::CallError &error) {
	switch (error.error) {
		case Variant::CallError::CALL_OK:
		case Variant::CallError::CALL_ERROR_INVALID_METHOD:
			break;
		case Variant::CallError::CALL_ERROR_INVALID_ARGUMENT: {
			ERR_FAIL_MSG("Error calling function: " + String(p_func) + " - Invalid type for argument " + itos(error.argument) + ", expected " + Variant::get_type_name(error.expected) + ".");
			break;
		}
		case Variant::CallError::CALL_ERROR_TOO_MANY_ARGUMENTS: {
			ERR_FAIL_MSG("Error calling function: " + String(p_func) + " - Too many arguments, expected " + itos(error.argument) + ".");
			break;
		}
		case Variant::CallError::CALL_ERROR_TOO_FEW_ARGUMENTS: {
			ERR_FAIL_MSG("Error calling function: " + String(p_func) + " - Too few arguments, expected " + itos(error.argument) + ".");
			break;
		}
		case Variant::CallError::CALL_ERROR_INSTANCE_IS_NULL:
			break;
	}
}
#else

#define _test_call_error(m_str, m_err)

#endif

void Object::call_multilevel(const StringName &p_method, const Variant **p_args, int p_argcount) {
	if (p_method == CoreStringNames::get_singleton()->_free) {
#ifdef DEBUG_ENABLED
		ERR_FAIL_COND_MSG(Object::cast_to<Reference>(this), "Can't 'free' a reference.");

		ERR_FAIL_COND_MSG(_lock_index.get() > 1, "Object is locked and can't be freed.");
#endif

		//must be here, must be before everything,
		memdelete(this);
		return;
	}

	//Variant ret;
	OBJ_DEBUG_LOCK

	Variant::CallError error;

	if (script_instance) {
		script_instance->call_multilevel(p_method, p_args, p_argcount);
		//_test_call_error(p_method,error);
	}

	MethodBind *method = ClassDB::get_method(get_class_name(), p_method);

	if (method) {
		method->call(this, p_args, p_argcount, error);
		_test_call_error(p_method, error);
	}
}

void Object::call_multilevel_reversed(const StringName &p_method, const Variant **p_args, int p_argcount) {
	MethodBind *method = ClassDB::get_method(get_class_name(), p_method);

	Variant::CallError error;
	OBJ_DEBUG_LOCK

	if (method) {
		method->call(this, p_args, p_argcount, error);
		_test_call_error(p_method, error);
	}

	//Variant ret;

	if (script_instance) {
		script_instance->call_multilevel_reversed(p_method, p_args, p_argcount);
		//_test_call_error(p_method,error);
	}
}

bool Object::has_method(const StringName &p_method) const {
	if (p_method == CoreStringNames::get_singleton()->_free) {
		return true;
	}

	if (script_instance && script_instance->has_method(p_method)) {
		return true;
	}

	MethodBind *method = ClassDB::get_method(get_class_name(), p_method);

	return method != nullptr;
}

Variant Object::getvar(const Variant &p_key, bool *r_valid) const {
	if (r_valid) {
		*r_valid = false;
	}
	return Variant();
}
void Object::setvar(const Variant &p_key, const Variant &p_value, bool *r_valid) {
	if (r_valid) {
		*r_valid = false;
	}
}

Variant Object::callv(const StringName &p_method, const Array &p_args) {
	const Variant **argptrs = nullptr;

	if (p_args.size() > 0) {
		argptrs = (const Variant **)alloca(sizeof(Variant *) * p_args.size());
		for (int i = 0; i < p_args.size(); i++) {
			argptrs[i] = &p_args[i];
		}
	}

	Variant::CallError ce;
	Variant ret = call(p_method, argptrs, p_args.size(), ce);
	if (ce.error != Variant::CallError::CALL_OK) {
		ERR_FAIL_V_MSG(Variant(), "Error calling method from 'callv': " + Variant::get_call_error_text(this, p_method, argptrs, p_args.size(), ce) + ".");
	}
	return ret;
}

Variant Object::call(const StringName &p_name, VARIANT_ARG_DECLARE) {
	VARIANT_ARGPTRS;

	int argc = 0;
	for (int i = 0; i < VARIANT_ARG_MAX; i++) {
		if (argptr[i]->get_type() == Variant::NIL) {
			break;
		}
		argc++;
	}

	Variant::CallError error;

	Variant ret = call(p_name, argptr, argc, error);
	return ret;
}

void Object::call_multilevel(const StringName &p_name, VARIANT_ARG_DECLARE) {
	VARIANT_ARGPTRS;

	int argc = 0;
	for (int i = 0; i < VARIANT_ARG_MAX; i++) {
		if (argptr[i]->get_type() == Variant::NIL) {
			break;
		}
		argc++;
	}

	//Variant::CallError error;
	call_multilevel(p_name, argptr, argc);
}

Variant Object::call(const StringName &p_method, const Variant **p_args, int p_argcount, Variant::CallError &r_error) {
	r_error.error = Variant::CallError::CALL_OK;

	if (p_method == CoreStringNames::get_singleton()->_free) {
//free must be here, before anything, always ready
#ifdef DEBUG_ENABLED
		if (p_argcount != 0) {
			r_error.argument = 0;
			r_error.error = Variant::CallError::CALL_ERROR_TOO_MANY_ARGUMENTS;
			return Variant();
		}
		if (Object::cast_to<Reference>(this)) {
			r_error.argument = 0;
			r_error.error = Variant::CallError::CALL_ERROR_INVALID_METHOD;
			ERR_FAIL_V_MSG(Variant(), "Can't 'free' a reference.");
		}

		if (_lock_index.get() > 1) {
			r_error.argument = 0;
			r_error.error = Variant::CallError::CALL_ERROR_INVALID_METHOD;
			ERR_FAIL_V_MSG(Variant(), "Object is locked and can't be freed.");
		}

#endif
		//must be here, must be before everything,
		memdelete(this);
		r_error.error = Variant::CallError::CALL_OK;
		return Variant();
	}

	Variant ret;
	OBJ_DEBUG_LOCK
	if (script_instance) {
		ret = script_instance->call(p_method, p_args, p_argcount, r_error);
		//force jumptable
		switch (r_error.error) {
			case Variant::CallError::CALL_OK:
				return ret;
			case Variant::CallError::CALL_ERROR_INVALID_METHOD:
				break;
			case Variant::CallError::CALL_ERROR_INVALID_ARGUMENT:
			case Variant::CallError::CALL_ERROR_TOO_MANY_ARGUMENTS:
			case Variant::CallError::CALL_ERROR_TOO_FEW_ARGUMENTS:
				return ret;
			case Variant::CallError::CALL_ERROR_INSTANCE_IS_NULL: {
			}
		}
	}

	MethodBind *method = ClassDB::get_method(get_class_name(), p_method);

	if (method) {
		ret = method->call(this, p_args, p_argcount, r_error);
	} else {
		r_error.error = Variant::CallError::CALL_ERROR_INVALID_METHOD;
	}

	return ret;
}

void Object::notification(int p_notification, bool p_reversed) {
	_notificationv(p_notification, p_reversed);

	if (script_instance) {
		script_instance->notification(p_notification);
	}
}

String Object::to_string() {
	if (script_instance) {
		bool valid;
		String ret = script_instance->to_string(&valid);
		if (valid) {
			return ret;
		}
	}
	return "[" + get_class() + ":" + itos(get_instance_id()) + "]";
}

void Object::_changed_callback(Object *p_changed, const char *p_prop) {
}

void Object::add_change_receptor(Object *p_receptor) {
	change_receptors.insert(p_receptor);
}

void Object::remove_change_receptor(Object *p_receptor) {
	change_receptors.erase(p_receptor);
}

void Object::property_list_changed_notify() {
	_change_notify();
}

void Object::cancel_delete() {
	_predelete_ok = true;
}

ObjectRC *Object::_use_rc() {
	// The RC object is lazily created the first time it's requested;
	// that way, there's no need to allocate and release it at all if this Object
	// is not being referred by any Variant at all.

	// Although when dealing with Objects from multiple threads some locking
	// mechanism should be used, this at least makes safe the case of first
	// assignment.

	ObjectRC *rc = nullptr;
	ObjectRC *const creating = reinterpret_cast<ObjectRC *>(1);
	if (unlikely(_rc.compare_exchange_strong(rc, creating, std::memory_order_acq_rel))) {
		// Not created yet
		rc = memnew(ObjectRC(this));
		_rc.store(rc, std::memory_order_release);
		return rc;
	}

	// Spin-wait until we know it's created (or just return if it's already created)
	for (;;) {
		if (likely(rc != creating)) {
			rc->increment();
			return rc;
		}
		rc = _rc.load(std::memory_order_acquire);
	}
}

void Object::set_script_and_instance(const RefPtr &p_script, ScriptInstance *p_instance) {
	//this function is not meant to be used in any of these ways
	ERR_FAIL_COND(p_script.is_null());
	ERR_FAIL_COND(!p_instance);
	ERR_FAIL_COND(script_instance != nullptr || !script.is_null());

	script = p_script;
	script_instance = p_instance;
}

void Object::set_script(const RefPtr &p_script) {
	if (script == p_script) {
		return;
	}

	Ref<Script> s = p_script;
	ERR_FAIL_COND_MSG(s.is_null() && !p_script.is_null(), "Invalid parameter, it should be a reference to a valid script (or null).");

	script = p_script;

	if (script_instance) {
		memdelete(script_instance);
		script_instance = nullptr;
	}

	if (!s.is_null()) {
		if (s->can_instance()) {
			OBJ_DEBUG_LOCK
			script_instance = s->instance_create(this);
		} else if (Engine::get_singleton()->is_editor_hint()) {
			OBJ_DEBUG_LOCK
			script_instance = s->placeholder_instance_create(this);
		}
	}

	_change_notify(); //scripts may add variables, so refresh is desired
	emit_signal(CoreStringNames::get_singleton()->script_changed);
}

void Object::set_script_instance(ScriptInstance *p_instance) {
	if (script_instance == p_instance) {
		return;
	}

	if (script_instance) {
		memdelete(script_instance);
	}

	script_instance = p_instance;

	if (p_instance) {
		script = p_instance->get_script().get_ref_ptr();
	} else {
		script = RefPtr();
	}
}

RefPtr Object::get_script() const {
	return script;
}

bool Object::has_meta(const String &p_name) const {
	return metadata.has(p_name);
}

void Object::set_meta(const String &p_name, const Variant &p_value) {
	if (p_value.get_type() == Variant::NIL) {
		metadata.erase(p_name);
		return;
	};

	metadata[p_name] = p_value;
}

Variant Object::get_meta(const String &p_name, const Variant &p_default) const {
	if (!metadata.has(p_name)) {
		if (p_default != Variant()) {
			return p_default;
		} else {
			ERR_FAIL_V_MSG(Variant(), "The object does not have any 'meta' values with the key '" + p_name + "'.");
		}
	}
	return metadata[p_name];
}

void Object::remove_meta(const String &p_name) {
	metadata.erase(p_name);
}

Array Object::_get_property_list_bind() const {
	List<PropertyInfo> lpi;
	get_property_list(&lpi);
	return convert_property_list(&lpi);
}

Array Object::_get_method_list_bind() const {
	List<MethodInfo> ml;
	get_method_list(&ml);
	Array ret;

	for (List<MethodInfo>::Element *E = ml.front(); E; E = E->next()) {
		Dictionary d = E->get();
		//va.push_back(d);
		ret.push_back(d);
	}

	return ret;
}

PoolVector<String> Object::_get_meta_list_bind() const {
	PoolVector<String> _metaret;

	List<Variant> keys;
	metadata.get_key_list(&keys);
	for (List<Variant>::Element *E = keys.front(); E; E = E->next()) {
		_metaret.push_back(E->get());
	}

	return _metaret;
}
void Object::get_meta_list(List<String> *p_list) const {
	List<Variant> keys;
	metadata.get_key_list(&keys);
	for (List<Variant>::Element *E = keys.front(); E; E = E->next()) {
		p_list->push_back(E->get());
	}
}

void Object::add_user_signal(const MethodInfo &p_signal) {
	ERR_FAIL_COND_MSG(p_signal.name == "", "Signal name cannot be empty.");
	ERR_FAIL_COND_MSG(ClassDB::has_signal(get_class_name(), p_signal.name), "User signal's name conflicts with a built-in signal of '" + get_class_name() + "'.");
	ERR_FAIL_COND_MSG(signal_map.has(p_signal.name), "Trying to add already existing signal '" + p_signal.name + "'.");
	Signal s;
	s.user = p_signal;
	signal_map[p_signal.name] = s;
}

bool Object::_has_user_signal(const StringName &p_name) const {
	if (!signal_map.has(p_name)) {
		return false;
	}
	return signal_map[p_name].user.name.length() > 0;
}

struct _ObjectSignalDisconnectData {
	StringName signal;
	Object *target;
	StringName method;
};

Variant Object::_emit_signal(const Variant **p_args, int p_argcount, Variant::CallError &r_error) {
	r_error.error = Variant::CallError::CALL_ERROR_TOO_FEW_ARGUMENTS;

	ERR_FAIL_COND_V(p_argcount < 1, Variant());
	if (p_args[0]->get_type() != Variant::STRING) {
		r_error.error = Variant::CallError::CALL_ERROR_INVALID_ARGUMENT;
		r_error.argument = 0;
		r_error.expected = Variant::STRING;
		ERR_FAIL_COND_V(p_args[0]->get_type() != Variant::STRING, Variant());
	}

	r_error.error = Variant::CallError::CALL_OK;

	StringName signal = *p_args[0];

	const Variant **args = nullptr;

	int argc = p_argcount - 1;
	if (argc) {
		args = &p_args[1];
	}

	emit_signal(signal, args, argc);

	return Variant();
}

Error Object::emit_signal(const StringName &p_name, const Variant **p_args, int p_argcount) {
	if (_block_signals) {
		return ERR_CANT_ACQUIRE_RESOURCE; //no emit, signals blocked
	}

	Signal *s = signal_map.getptr(p_name);
	if (!s) {
#ifdef DEBUG_ENABLED
		bool signal_is_valid = ClassDB::has_signal(get_class_name(), p_name);
		//check in script
		ERR_FAIL_COND_V_MSG(!signal_is_valid && !script.is_null() && !Ref<Script>(script)->has_script_signal(p_name), ERR_UNAVAILABLE, "Can't emit non-existing signal " + String("\"") + p_name + "\".");
#endif
		//not connected? just return
		return ERR_UNAVAILABLE;
	}

	List<_ObjectSignalDisconnectData> disconnect_data;

	//copy on write will ensure that disconnecting the signal or even deleting the object will not affect the signal calling.
	//this happens automatically and will not change the performance of calling.
	//awesome, isn't it?
	VMap<Signal::Target, Signal::Slot> slot_map = s->slot_map;

	int ssize = slot_map.size();

	OBJ_DEBUG_LOCK

	Vector<const Variant *> bind_mem;

	Error err = OK;

	for (int i = 0; i < ssize; i++) {
		const Connection &c = slot_map.getv(i).conn;

		Object *target = ObjectDB::get_instance(slot_map.getk(i)._id);
		if (!target) {
			// Target might have been deleted during signal callback, this is expected and OK.
			continue;
		}

		const Variant **args = p_args;
		int argc = p_argcount;

		if (c.binds.size()) {
			//handle binds
			bind_mem.resize(p_argcount + c.binds.size());

			for (int j = 0; j < p_argcount; j++) {
				bind_mem.write[j] = p_args[j];
			}
			for (int j = 0; j < c.binds.size(); j++) {
				bind_mem.write[p_argcount + j] = &c.binds[j];
			}

			args = (const Variant **)bind_mem.ptr();
			argc = bind_mem.size();
		}

		if (c.flags & CONNECT_DEFERRED) {
			MessageQueue::get_singleton()->push_call(target->get_instance_id(), c.method, args, argc, true);
		} else {
			Variant::CallError ce;
			_emitting = true;
			target->call(c.method, args, argc, ce);
			_emitting = false;

			if (ce.error != Variant::CallError::CALL_OK) {
#ifdef DEBUG_ENABLED
				if (c.flags & CONNECT_PERSIST && Engine::get_singleton()->is_editor_hint() && (script.is_null() || !Ref<Script>(script)->is_tool())) {
					continue;
				}
#endif
				if (ce.error == Variant::CallError::CALL_ERROR_INVALID_METHOD && !ClassDB::class_exists(target->get_class_name())) {
					//most likely object is not initialized yet, do not throw error.
				} else {
					ERR_PRINT("Error calling method from signal '" + String(p_name) + "': " + Variant::get_call_error_text(target, c.method, args, argc, ce) + ".");
					err = ERR_METHOD_NOT_FOUND;
				}
			}
		}

		bool disconnect = c.flags & CONNECT_ONESHOT;
#ifdef TOOLS_ENABLED
		if (disconnect && (c.flags & CONNECT_PERSIST) && Engine::get_singleton()->is_editor_hint()) {
			//this signal was connected from the editor, and is being edited. just don't disconnect for now
			disconnect = false;
		}
#endif
		if (disconnect) {
			_ObjectSignalDisconnectData dd;
			dd.signal = p_name;
			dd.target = target;
			dd.method = c.method;
			disconnect_data.push_back(dd);
		}
	}

	while (!disconnect_data.empty()) {
		const _ObjectSignalDisconnectData &dd = disconnect_data.front()->get();
		disconnect(dd.signal, dd.target, dd.method);
		disconnect_data.pop_front();
	}

	return err;
}

Error Object::emit_signal(const StringName &p_name, VARIANT_ARG_DECLARE) {
	VARIANT_ARGPTRS;

	int argc = 0;

	for (int i = 0; i < VARIANT_ARG_MAX; i++) {
		if (argptr[i]->get_type() == Variant::NIL) {
			break;
		}
		argc++;
	}

	return emit_signal(p_name, argptr, argc);
}

void Object::_add_user_signal(const String &p_name, const Array &p_args) {
	// this version of add_user_signal is meant to be used from scripts or external apis
	// without access to ADD_SIGNAL in bind_methods
	// added events are per instance, as opposed to the other ones, which are global

	MethodInfo mi;
	mi.name = p_name;

	for (int i = 0; i < p_args.size(); i++) {
		Dictionary d = p_args[i];
		PropertyInfo param;

		if (d.has("name")) {
			param.name = d["name"];
		}
		if (d.has("type")) {
			param.type = (Variant::Type)(int)d["type"];
		}

		mi.arguments.push_back(param);
	}

	add_user_signal(mi);
}

Array Object::_get_signal_list() const {
	List<MethodInfo> signal_list;
	get_signal_list(&signal_list);

	Array ret;
	for (List<MethodInfo>::Element *E = signal_list.front(); E; E = E->next()) {
		ret.push_back(Dictionary(E->get()));
	}

	return ret;
}

Array Object::_get_signal_connection_list(const String &p_signal) const {
	List<Connection> conns;
	get_all_signal_connections(&conns);

	Array ret;

	for (List<Connection>::Element *E = conns.front(); E; E = E->next()) {
		Connection &c = E->get();
		if (c.signal == p_signal) {
			Dictionary rc;
			rc["signal"] = c.signal;
			rc["method"] = c.method;
			rc["source"] = c.source;
			rc["target"] = c.target;
			rc["binds"] = c.binds;
			rc["flags"] = c.flags;
			ret.push_back(rc);
		}
	}

	return ret;
}

Array Object::_get_incoming_connections() const {
	Array ret;
	int connections_amount = connections.size();
	for (int idx_conn = 0; idx_conn < connections_amount; idx_conn++) {
		Dictionary conn_data;
		conn_data["source"] = connections[idx_conn].source;
		conn_data["signal_name"] = connections[idx_conn].signal;
		conn_data["method_name"] = connections[idx_conn].method;
		ret.push_back(conn_data);
	}

	return ret;
}

bool Object::has_signal(const StringName &p_name) const {
	if (!script.is_null()) {
		Ref<Script> scr = script;
		if (scr.is_valid() && scr->has_script_signal(p_name)) {
			return true;
		}
	}

	if (ClassDB::has_signal(get_class_name(), p_name)) {
		return true;
	}

	if (_has_user_signal(p_name)) {
		return true;
	}

	return false;
}

void Object::get_signal_list(List<MethodInfo> *p_signals) const {
	if (!script.is_null()) {
		Ref<Script> scr = script;
		if (scr.is_valid()) {
			scr->get_script_signal_list(p_signals);
		}
	}

	ClassDB::get_signal_list(get_class_name(), p_signals);
	//find maybe usersignals?
	const StringName *S = nullptr;

	while ((S = signal_map.next(S))) {
		if (signal_map[*S].user.name != "") {
			//user signal
			p_signals->push_back(signal_map[*S].user);
		}
	}
}

void Object::get_all_signal_connections(List<Connection> *p_connections) const {
	const StringName *S = nullptr;

	while ((S = signal_map.next(S))) {
		const Signal *s = &signal_map[*S];

		for (int i = 0; i < s->slot_map.size(); i++) {
			p_connections->push_back(s->slot_map.getv(i).conn);
		}
	}
}

void Object::get_signal_connection_list(const StringName &p_signal, List<Connection> *p_connections) const {
	const Signal *s = signal_map.getptr(p_signal);
	if (!s) {
		return; //nothing
	}

	for (int i = 0; i < s->slot_map.size(); i++) {
		p_connections->push_back(s->slot_map.getv(i).conn);
	}
}

int Object::get_persistent_signal_connection_count() const {
	int count = 0;
	const StringName *S = nullptr;

	while ((S = signal_map.next(S))) {
		const Signal *s = &signal_map[*S];

		for (int i = 0; i < s->slot_map.size(); i++) {
			if (s->slot_map.getv(i).conn.flags & CONNECT_PERSIST) {
				count += 1;
			}
		}
	}

	return count;
}

void Object::get_signals_connected_to_this(List<Connection> *p_connections) const {
	for (const List<Connection>::Element *E = connections.front(); E; E = E->next()) {
		p_connections->push_back(E->get());
	}
}

Error Object::connect(const StringName &p_signal, Object *p_to_object, const StringName &p_to_method, const Vector<Variant> &p_binds, uint32_t p_flags) {
	ERR_FAIL_NULL_V(p_to_object, ERR_INVALID_PARAMETER);

	Signal *s = signal_map.getptr(p_signal);
	if (!s) {
		bool signal_is_valid = ClassDB::has_signal(get_class_name(), p_signal);
		//check in script
		if (!signal_is_valid && !script.is_null()) {
			if (Ref<Script>(script)->has_script_signal(p_signal)) {
				signal_is_valid = true;
			}
#ifdef TOOLS_ENABLED
			else {
				//allow connecting signals anyway if script is invalid, see issue #17070
				if (!Ref<Script>(script)->is_valid()) {
					signal_is_valid = true;
				}
			}
#endif
		}

		ERR_FAIL_COND_V_MSG(!signal_is_valid, ERR_INVALID_PARAMETER, "In Object of type '" + String(get_class()) + "': Attempt to connect nonexistent signal '" + p_signal + "' to method '" + p_to_object->get_class() + "." + p_to_method + "'.");

		signal_map[p_signal] = Signal();
		s = &signal_map[p_signal];
	}

	Signal::Target target(p_to_object->get_instance_id(), p_to_method);
	if (s->slot_map.has(target)) {
		if (p_flags & CONNECT_REFERENCE_COUNTED) {
			s->slot_map[target].reference_count++;
			return OK;
		} else {
			ERR_FAIL_V_MSG(ERR_INVALID_PARAMETER,
					vformat("Signal \"%s\" from \"%s\" is already connected to given method \"%s\" in \"%s\".",
							p_signal, to_string(), p_to_method, p_to_object->to_string()));
		}
	}

	Signal::Slot slot;

	Connection conn;
	conn.source = this;
	conn.target = p_to_object;
	conn.method = p_to_method;
	conn.signal = p_signal;
	conn.flags = p_flags;
	conn.binds = p_binds;
	slot.conn = conn;
	slot.cE = p_to_object->connections.push_back(conn);
	if (p_flags & CONNECT_REFERENCE_COUNTED) {
		slot.reference_count = 1;
	}

	s->slot_map[target] = slot;

	return OK;
}

bool Object::is_connected(const StringName &p_signal, Object *p_to_object, const StringName &p_to_method) const {
	ERR_FAIL_NULL_V(p_to_object, false);
	const Signal *s = signal_map.getptr(p_signal);
	if (!s) {
		bool signal_is_valid = ClassDB::has_signal(get_class_name(), p_signal);
		if (signal_is_valid) {
			return false;
		}

		if (!script.is_null() && Ref<Script>(script)->has_script_signal(p_signal)) {
			return false;
		}

		ERR_FAIL_V_MSG(false, "Nonexistent signal: " + p_signal + ".");
	}

	Signal::Target target(p_to_object->get_instance_id(), p_to_method);

	return s->slot_map.has(target);
	//const Map<Signal::Target,Signal::Slot>::Element *E = s->slot_map.find(target);
	//return (E!=NULL);
}

void Object::disconnect(const StringName &p_signal, Object *p_to_object, const StringName &p_to_method) {
	_disconnect(p_signal, p_to_object, p_to_method);
}
void Object::_disconnect(const StringName &p_signal, Object *p_to_object, const StringName &p_to_method, bool p_force) {
	ERR_FAIL_NULL(p_to_object);
	Signal *s = signal_map.getptr(p_signal);
	if (!s) {
		bool signal_is_valid = ClassDB::has_signal(get_class_name(), p_signal) ||
				(!script.is_null() && Ref<Script>(script)->has_script_signal(p_signal));
		ERR_FAIL_COND_MSG(signal_is_valid,
				vformat("Attempt to disconnect a nonexistent connection to signal '%s' in %s, with target '%s' in %s.",
						p_signal, to_string(), p_to_method, p_to_object->to_string()));
	}
	ERR_FAIL_COND_MSG(!s, vformat("Disconnecting nonexistent signal '%s' in %s.", p_signal, to_string()));

	Signal::Target target(p_to_object->get_instance_id(), p_to_method);

	ERR_FAIL_COND_MSG(!s->slot_map.has(target), "Disconnecting nonexistent signal '" + p_signal + "', slot: " + itos(target._id) + ":" + target.method + ".");

	Signal::Slot *slot = &s->slot_map[target];

	if (!p_force) {
		slot->reference_count--; // by default is zero, if it was not referenced it will go below it
		if (slot->reference_count > 0) {
			return;
		}
	}

	p_to_object->connections.erase(slot->cE);
	s->slot_map.erase(target);

	if (s->slot_map.empty() && ClassDB::has_signal(get_class_name(), p_signal)) {
		//not user signal, delete
		signal_map.erase(p_signal);
	}
}

void Object::_set_bind(const String &p_set, const Variant &p_value) {
	set(p_set, p_value);
}

Variant Object::_get_bind(const String &p_name) const {
	return get(p_name);
}

void Object::_set_indexed_bind(const NodePath &p_name, const Variant &p_value) {
	set_indexed(p_name.get_as_property_path().get_subnames(), p_value);
}

Variant Object::_get_indexed_bind(const NodePath &p_name) const {
	return get_indexed(p_name.get_as_property_path().get_subnames());
}

void Object::initialize_class() {
	static bool initialized = false;
	if (initialized) {
		return;
	}
	ClassDB::_add_class<Object>();
	_bind_methods();
	initialized = true;
}

StringName Object::tr(const StringName &p_message) const {
	if (!_can_translate || !TranslationServer::get_singleton()) {
		return p_message;
	}

	return TranslationServer::get_singleton()->translate(p_message);
}

void Object::_clear_internal_resource_paths(const Variant &p_var) {
	switch (p_var.get_type()) {
		case Variant::OBJECT: {
			RES r = p_var;
			if (!r.is_valid()) {
				return;
			}

			if (!r->get_path().begins_with("res://") || r->get_path().find("::") == -1) {
				return; //not an internal resource
			}

			Object *object = p_var;
			if (!object) {
				return;
			}

			r->set_path("");
			r->clear_internal_resource_paths();
		} break;
		case Variant::ARRAY: {
			Array a = p_var;
			for (int i = 0; i < a.size(); i++) {
				_clear_internal_resource_paths(a[i]);
			}

		} break;
		case Variant::DICTIONARY: {
			Dictionary d = p_var;
			List<Variant> keys;
			d.get_key_list(&keys);

			for (List<Variant>::Element *E = keys.front(); E; E = E->next()) {
				_clear_internal_resource_paths(E->get());
				_clear_internal_resource_paths(d[E->get()]);
			}
		} break;
		default: {
		}
	}
}

#ifdef TOOLS_ENABLED
void Object::editor_set_section_unfold(const String &p_section, bool p_unfolded) {
	set_edited(true);
	if (p_unfolded) {
		editor_section_folding.insert(p_section);
	} else {
		editor_section_folding.erase(p_section);
	}
}

bool Object::editor_is_section_unfolded(const String &p_section) {
	return editor_section_folding.has(p_section);
}

#endif

void Object::clear_internal_resource_paths() {
	List<PropertyInfo> pinfo;

	get_property_list(&pinfo);

	for (List<PropertyInfo>::Element *E = pinfo.front(); E; E = E->next()) {
		_clear_internal_resource_paths(get(E->get().name));
	}
}

void Object::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_class"), &Object::get_class);
	ClassDB::bind_method(D_METHOD("is_class", "class"), &Object::is_class);
	ClassDB::bind_method(D_METHOD("set", "property", "value"), &Object::_set_bind);
	ClassDB::bind_method(D_METHOD("get", "property"), &Object::_get_bind);
	ClassDB::bind_method(D_METHOD("set_indexed", "property_path", "value"), &Object::_set_indexed_bind);
	ClassDB::bind_method(D_METHOD("get_indexed", "property_path"), &Object::_get_indexed_bind);
	ClassDB::bind_method(D_METHOD("get_property_list"), &Object::_get_property_list_bind);
	ClassDB::bind_method(D_METHOD("get_method_list"), &Object::_get_method_list_bind);
	ClassDB::bind_method(D_METHOD("notification", "what", "reversed"), &Object::notification, DEFVAL(false));
	ClassDB::bind_method(D_METHOD("to_string"), &Object::to_string);
	ClassDB::bind_method(D_METHOD("get_instance_id"), &Object::get_instance_id);

	ClassDB::bind_method(D_METHOD("set_script", "script"), &Object::set_script);
	ClassDB::bind_method(D_METHOD("get_script"), &Object::get_script);

	ClassDB::bind_method(D_METHOD("set_meta", "name", "value"), &Object::set_meta);
	ClassDB::bind_method(D_METHOD("remove_meta", "name"), &Object::remove_meta);
	ClassDB::bind_method(D_METHOD("get_meta", "name", "default"), &Object::get_meta, DEFVAL(Variant()));
	ClassDB::bind_method(D_METHOD("has_meta", "name"), &Object::has_meta);
	ClassDB::bind_method(D_METHOD("get_meta_list"), &Object::_get_meta_list_bind);

	ClassDB::bind_method(D_METHOD("add_user_signal", "signal", "arguments"), &Object::_add_user_signal, DEFVAL(Array()));
	ClassDB::bind_method(D_METHOD("has_user_signal", "signal"), &Object::_has_user_signal);

	{
		MethodInfo mi;
		mi.name = "emit_signal";
		mi.arguments.push_back(PropertyInfo(Variant::STRING, "signal"));

		ClassDB::bind_vararg_method(METHOD_FLAGS_DEFAULT, "emit_signal", &Object::_emit_signal, mi, varray(), false);
	}

	{
		MethodInfo mi;
		mi.name = "call";
		mi.arguments.push_back(PropertyInfo(Variant::STRING, "method"));

		ClassDB::bind_vararg_method(METHOD_FLAGS_DEFAULT, "call", &Object::_call_bind, mi);
	}

	{
		MethodInfo mi;
		mi.name = "call_deferred";
		mi.arguments.push_back(PropertyInfo(Variant::STRING, "method"));

		ClassDB::bind_vararg_method(METHOD_FLAGS_DEFAULT, "call_deferred", &Object::_call_deferred_bind, mi, varray(), false);
	}

	ClassDB::bind_method(D_METHOD("set_deferred", "property", "value"), &Object::set_deferred);

	ClassDB::bind_method(D_METHOD("callv", "method", "arg_array"), &Object::callv);

	ClassDB::bind_method(D_METHOD("has_method", "method"), &Object::has_method);

	ClassDB::bind_method(D_METHOD("has_signal", "signal"), &Object::has_signal);
	ClassDB::bind_method(D_METHOD("get_signal_list"), &Object::_get_signal_list);
	ClassDB::bind_method(D_METHOD("get_signal_connection_list", "signal"), &Object::_get_signal_connection_list);
	ClassDB::bind_method(D_METHOD("get_incoming_connections"), &Object::_get_incoming_connections);

	ClassDB::bind_method(D_METHOD("connect", "signal", "target", "method", "binds", "flags"), &Object::connect, DEFVAL(Array()), DEFVAL(0));
	ClassDB::bind_method(D_METHOD("disconnect", "signal", "target", "method"), &Object::disconnect);
	ClassDB::bind_method(D_METHOD("is_connected", "signal", "target", "method"), &Object::is_connected);

	ClassDB::bind_method(D_METHOD("set_block_signals", "enable"), &Object::set_block_signals);
	ClassDB::bind_method(D_METHOD("is_blocking_signals"), &Object::is_blocking_signals);
	ClassDB::bind_method(D_METHOD("property_list_changed_notify"), &Object::property_list_changed_notify);

	ClassDB::bind_method(D_METHOD("set_message_translation", "enable"), &Object::set_message_translation);
	ClassDB::bind_method(D_METHOD("can_translate_messages"), &Object::can_translate_messages);
	ClassDB::bind_method(D_METHOD("tr", "message"), &Object::tr);

	ClassDB::bind_method(D_METHOD("is_queued_for_deletion"), &Object::is_queued_for_deletion);

	ClassDB::add_virtual_method("Object", MethodInfo("free"), false);

	ADD_SIGNAL(MethodInfo("script_changed"));

	BIND_VMETHOD(MethodInfo("_notification", PropertyInfo(Variant::INT, "what")));
	BIND_VMETHOD(MethodInfo(Variant::BOOL, "_set", PropertyInfo(Variant::STRING, "property"), PropertyInfo(Variant::NIL, "value")));
#ifdef TOOLS_ENABLED
	MethodInfo miget("_get", PropertyInfo(Variant::STRING, "property"));
	miget.return_val.name = "Variant";
	miget.return_val.usage |= PROPERTY_USAGE_NIL_IS_VARIANT;
	BIND_VMETHOD(miget);

	MethodInfo plget("_get_property_list");

	plget.return_val.type = Variant::ARRAY;
	BIND_VMETHOD(plget);

#endif
	BIND_VMETHOD(MethodInfo("_init"));
	BIND_VMETHOD(MethodInfo(Variant::STRING, "_to_string"));

	BIND_CONSTANT(NOTIFICATION_POSTINITIALIZE);
	BIND_CONSTANT(NOTIFICATION_PREDELETE);

	BIND_ENUM_CONSTANT(CONNECT_DEFERRED);
	BIND_ENUM_CONSTANT(CONNECT_PERSIST);
	BIND_ENUM_CONSTANT(CONNECT_ONESHOT);
	BIND_ENUM_CONSTANT(CONNECT_REFERENCE_COUNTED);
}

void Object::call_deferred(const StringName &p_method, VARIANT_ARG_DECLARE) {
	MessageQueue::get_singleton()->push_call(this, p_method, VARIANT_ARG_PASS);
}

void Object::set_deferred(const StringName &p_property, const Variant &p_value) {
	MessageQueue::get_singleton()->push_set(this, p_property, p_value);
}

void Object::set_block_signals(bool p_block) {
	_block_signals = p_block;
}

bool Object::is_blocking_signals() const {
	return _block_signals;
}

void Object::get_translatable_strings(List<String> *p_strings) const {
	List<PropertyInfo> plist;
	get_property_list(&plist);

	for (List<PropertyInfo>::Element *E = plist.front(); E; E = E->next()) {
		if (!(E->get().usage & PROPERTY_USAGE_INTERNATIONALIZED)) {
			continue;
		}

		String text = get(E->get().name);

		if (text == "") {
			continue;
		}

		p_strings->push_back(text);
	}
}

Variant::Type Object::get_static_property_type(const StringName &p_property, bool *r_valid) const {
	bool valid;
	Variant::Type t = ClassDB::get_property_type(get_class_name(), p_property, &valid);
	if (valid) {
		if (r_valid) {
			*r_valid = true;
		}
		return t;
	}

	if (get_script_instance()) {
		return get_script_instance()->get_property_type(p_property, r_valid);
	}
	if (r_valid) {
		*r_valid = false;
	}

	return Variant::NIL;
}

Variant::Type Object::get_static_property_type_indexed(const Vector<StringName> &p_path, bool *r_valid) const {
	if (p_path.size() == 0) {
		if (r_valid) {
			*r_valid = false;
		}

		return Variant::NIL;
	}

	bool valid = false;
	Variant::Type t = get_static_property_type(p_path[0], &valid);
	if (!valid) {
		if (r_valid) {
			*r_valid = false;
		}

		return Variant::NIL;
	}

	Variant::CallError ce;
	Variant check = Variant::construct(t, nullptr, 0, ce);

	for (int i = 1; i < p_path.size(); i++) {
		if (check.get_type() == Variant::OBJECT || check.get_type() == Variant::DICTIONARY || check.get_type() == Variant::ARRAY) {
			// We cannot be sure about the type of properties this types can have
			if (r_valid) {
				*r_valid = false;
			}
			return Variant::NIL;
		}

		check = check.get_named(p_path[i], &valid);

		if (!valid) {
			if (r_valid) {
				*r_valid = false;
			}
			return Variant::NIL;
		}
	}

	if (r_valid) {
		*r_valid = true;
	}

	return check.get_type();
}

bool Object::is_queued_for_deletion() const {
	return _is_queued_for_deletion;
}

#ifdef TOOLS_ENABLED
void Object::set_edited(bool p_edited) {
	_edited = p_edited;
	_edited_version++;
}

bool Object::is_edited() const {
	return _edited;
}

uint32_t Object::get_edited_version() const {
	return _edited_version;
}
#endif

void *Object::get_script_instance_binding(int p_script_language_index) {
#ifdef DEBUG_ENABLED
	ERR_FAIL_INDEX_V(p_script_language_index, MAX_SCRIPT_INSTANCE_BINDINGS, nullptr);
#endif

	//it's up to the script language to make this thread safe, if the function is called twice due to threads being out of syncro
	//just return the same pointer.
	//if you want to put a big lock in the entire function and keep allocated pointers in a map or something, feel free to do it
	//as it should not really affect performance much (won't be called too often), as in far most caes the condition below will be false afterwards

	if (!_script_instance_bindings[p_script_language_index]) {
		void *script_data = ScriptServer::get_language(p_script_language_index)->alloc_instance_binding_data(this);
		if (script_data) {
			instance_binding_count.increment();
			_script_instance_bindings[p_script_language_index] = script_data;
		}
	}

	return _script_instance_bindings[p_script_language_index];
}

bool Object::has_script_instance_binding(int p_script_language_index) {
	return _script_instance_bindings[p_script_language_index] != nullptr;
}

void Object::set_script_instance_binding(int p_script_language_index, void *p_data) {
#ifdef DEBUG_ENABLED
	CRASH_COND(_script_instance_bindings[p_script_language_index] != nullptr);
#endif
	_script_instance_bindings[p_script_language_index] = p_data;
}

Object::Object() {
	_class_ptr = nullptr;
	_block_signals = false;
	_predelete_ok = 0;
	_instance_id = 0;
	_instance_id = ObjectDB::add_instance(this);
	_can_translate = true;
	_is_queued_for_deletion = false;
	_emitting = false;
	memset(_script_instance_bindings, 0, sizeof(void *) * MAX_SCRIPT_INSTANCE_BINDINGS);
	script_instance = nullptr;
	_rc.store(nullptr, std::memory_order_release);
#ifdef TOOLS_ENABLED

	_edited = false;
	_edited_version = 0;
#endif

#ifdef DEBUG_ENABLED
	_lock_index.init(1);
#endif
}

Object::~Object() {
	ObjectRC *rc = _rc.load(std::memory_order_acquire);
	if (rc) {
		if (rc->invalidate()) {
			memdelete(rc);
		}
	}

	if (script_instance) {
		memdelete(script_instance);
	}
	script_instance = nullptr;

	const StringName *S = nullptr;

	if (_emitting) {
		//@todo this may need to actually reach the debugger prioritarily somehow because it may crash before
		ERR_PRINT("Object " + to_string() + " was freed or unreferenced while a signal is being emitted from it. Try connecting to the signal using 'CONNECT_DEFERRED' flag, or use queue_free() to free the object (if this object is a Node) to avoid this error and potential crashes.");
	}

	while ((S = signal_map.next(nullptr))) {
		Signal *s = &signal_map[*S];

		//brute force disconnect for performance
		int slot_count = s->slot_map.size();
		const VMap<Signal::Target, Signal::Slot>::Pair *slot_list = s->slot_map.get_array();

		for (int i = 0; i < slot_count; i++) {
			slot_list[i].value.conn.target->connections.erase(slot_list[i].value.cE);
		}

		signal_map.erase(*S);
	}

	//signals from nodes that connect to this node
	while (connections.size()) {
		Connection c = connections.front()->get();
		c.source->_disconnect(c.signal, c.target, c.method, true);
	}

	ObjectDB::remove_instance(this);
	_instance_id = 0;
	_predelete_ok = 2;

	if (!ScriptServer::are_languages_finished()) {
		for (int i = 0; i < MAX_SCRIPT_INSTANCE_BINDINGS; i++) {
			if (_script_instance_bindings[i]) {
				ScriptServer::get_language(i)->free_instance_binding_data(this, _script_instance_bindings[i]);
			}
		}
	}
}

bool predelete_handler(Object *p_object) {
	return p_object->_predelete();
}

void postinitialize_handler(Object *p_object) {
	p_object->_postinitialize();
}

HashMap<ObjectID, Object *> ObjectDB::instances;
ObjectID ObjectDB::instance_counter = 1;
HashMap<Object *, ObjectID, ObjectDB::ObjectPtrHash> ObjectDB::instance_checks;
ObjectID ObjectDB::add_instance(Object *p_object) {
	ERR_FAIL_COND_V(p_object->get_instance_id() != 0, 0);

	rw_lock.write_lock();
	ObjectID instance_id = ++instance_counter;
	instances[instance_id] = p_object;
	instance_checks[p_object] = instance_id;

	rw_lock.write_unlock();

	return instance_id;
}

void ObjectDB::remove_instance(Object *p_object) {
	rw_lock.write_lock();

	instances.erase(p_object->get_instance_id());
	instance_checks.erase(p_object);

	rw_lock.write_unlock();
}
Object *ObjectDB::get_instance(ObjectID p_instance_id) {
	rw_lock.read_lock();
	Object **obj = instances.getptr(p_instance_id);
	rw_lock.read_unlock();

	if (!obj) {
		return nullptr;
	}
	return *obj;
}

void ObjectDB::debug_objects(DebugFunc p_func) {
	rw_lock.read_lock();

	const ObjectID *K = nullptr;
	while ((K = instances.next(K))) {
		p_func(instances[*K]);
	}

	rw_lock.read_unlock();
}

void Object::get_argument_options(const StringName &p_function, int p_idx, List<String> *r_options) const {
}

int ObjectDB::get_object_count() {
	rw_lock.read_lock();
	int count = instances.size();
	rw_lock.read_unlock();

	return count;
}

RWLock ObjectDB::rw_lock;

void ObjectDB::cleanup() {
	rw_lock.write_lock();
	if (instances.size()) {
		WARN_PRINT("ObjectDB instances leaked at exit (run with --verbose for details).");
		if (OS::get_singleton()->is_stdout_verbose()) {
			// Ensure calling the native classes because if a leaked instance has a script
			// that overrides any of those methods, it'd not be OK to call them at this point,
			// now the scripting languages have already been terminated.
			MethodBind *node_get_name = ClassDB::get_method("Node", "get_name");
			MethodBind *resource_get_path = ClassDB::get_method("Resource", "get_path");
			Variant::CallError call_error;

			const ObjectID *K = nullptr;
			while ((K = instances.next(K))) {
				String extra_info;
				if (instances[*K]->is_class("Node")) {
					extra_info = " - Node name: " + String(node_get_name->call(instances[*K], nullptr, 0, call_error));
				}
				if (instances[*K]->is_class("Resource")) {
					extra_info = " - Resource path: " + String(resource_get_path->call(instances[*K], nullptr, 0, call_error));
				}
				print_line("Leaked instance: " + String(instances[*K]->get_class()) + ":" + itos(*K) + extra_info);
			}
			print_line("Hint: Leaked instances typically happen when nodes are removed from the scene tree (with `remove_child()`) but not freed (with `free()` or `queue_free()`).");
		}
	}
	instances.clear();
	instance_checks.clear();
	rw_lock.write_unlock();
}

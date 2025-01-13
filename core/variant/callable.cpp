/**************************************************************************/
/*  callable.cpp                                                          */
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

#include "callable.h"

#include "core/object/object.h"
#include "core/object/ref_counted.h"
#include "core/object/script_language.h"
#include "core/variant/callable_bind.h"
#include "core/variant/variant_callable.h"

void Callable::call_deferredp(const Variant **p_arguments, int p_argcount) const {
	MessageQueue::get_singleton()->push_callablep(*this, p_arguments, p_argcount, true);
}

void Callable::callp(const Variant **p_arguments, int p_argcount, Variant &r_return_value, CallError &r_call_error) const {
	if (is_null()) {
		r_call_error.error = CallError::CALL_ERROR_INSTANCE_IS_NULL;
		r_call_error.argument = 0;
		r_call_error.expected = 0;
		r_return_value = Variant();
	} else if (is_custom()) {
		if (!is_valid()) {
			r_call_error.error = CallError::CALL_ERROR_INSTANCE_IS_NULL;
			r_call_error.argument = 0;
			r_call_error.expected = 0;
			r_return_value = Variant();
			return;
		}
		custom->call(p_arguments, p_argcount, r_return_value, r_call_error);
	} else {
		Object *obj = ObjectDB::get_instance(ObjectID(object));
#ifdef DEBUG_ENABLED
		if (!obj) {
			r_call_error.error = CallError::CALL_ERROR_INSTANCE_IS_NULL;
			r_call_error.argument = 0;
			r_call_error.expected = 0;
			r_return_value = Variant();
			return;
		}
#endif
		r_return_value = obj->callp(method, p_arguments, p_argcount, r_call_error);
	}
}

Variant Callable::callv(const Array &p_arguments) const {
	int argcount = p_arguments.size();
	const Variant **argptrs = nullptr;
	if (argcount) {
		argptrs = (const Variant **)alloca(sizeof(Variant *) * argcount);
		for (int i = 0; i < argcount; i++) {
			argptrs[i] = &p_arguments[i];
		}
	}
	CallError ce;
	Variant ret;
	callp(argptrs, argcount, ret, ce);
	return ret;
}

Error Callable::rpcp(int p_id, const Variant **p_arguments, int p_argcount, CallError &r_call_error) const {
	if (is_null()) {
		r_call_error.error = CallError::CALL_ERROR_INSTANCE_IS_NULL;
		r_call_error.argument = 0;
		r_call_error.expected = 0;
		return ERR_UNCONFIGURED;
	} else if (!is_custom()) {
		Object *obj = ObjectDB::get_instance(ObjectID(object));
#ifdef DEBUG_ENABLED
		if (!obj || !obj->is_class("Node")) {
			r_call_error.error = CallError::CALL_ERROR_INSTANCE_IS_NULL;
			r_call_error.argument = 0;
			r_call_error.expected = 0;
			return ERR_UNCONFIGURED;
		}
#endif

		int argcount = p_argcount + 2;
		const Variant **argptrs = (const Variant **)alloca(sizeof(Variant *) * argcount);
		const Variant args[2] = { p_id, method };

		argptrs[0] = &args[0];
		argptrs[1] = &args[1];
		for (int i = 0; i < p_argcount; ++i) {
			argptrs[i + 2] = p_arguments[i];
		}

		CallError tmp; // TODO: Check `tmp`?
		Error err = (Error)obj->callp(SNAME("rpc_id"), argptrs, argcount, tmp).operator int64_t();

		r_call_error.error = Callable::CallError::CALL_OK;
		return err;
	} else {
		return custom->rpc(p_id, p_arguments, p_argcount, r_call_error);
	}
}

Callable Callable::bindp(const Variant **p_arguments, int p_argcount) const {
	Vector<Variant> args;
	args.resize(p_argcount);
	for (int i = 0; i < p_argcount; i++) {
		args.write[i] = *p_arguments[i];
	}
	return Callable(memnew(CallableCustomBind(*this, args)));
}

Callable Callable::bindv(const Array &p_arguments) {
	if (p_arguments.is_empty()) {
		return *this; // No point in creating a new callable if nothing is bound.
	}

	Vector<Variant> args;
	args.resize(p_arguments.size());
	for (int i = 0; i < p_arguments.size(); i++) {
		args.write[i] = p_arguments[i];
	}
	return Callable(memnew(CallableCustomBind(*this, args)));
}

Callable Callable::unbind(int p_argcount) const {
	ERR_FAIL_COND_V_MSG(p_argcount <= 0, Callable(*this), "Amount of unbind() arguments must be 1 or greater.");
	return Callable(memnew(CallableCustomUnbind(*this, p_argcount)));
}

bool Callable::is_valid() const {
	if (is_custom()) {
		return get_custom()->is_valid();
	} else {
		return get_object() && get_object()->has_method(get_method());
	}
}

Object *Callable::get_object() const {
	if (is_null()) {
		return nullptr;
	} else if (is_custom()) {
		return ObjectDB::get_instance(custom->get_object());
	} else {
		return ObjectDB::get_instance(ObjectID(object));
	}
}

ObjectID Callable::get_object_id() const {
	if (is_null()) {
		return ObjectID();
	} else if (is_custom()) {
		return custom->get_object();
	} else {
		return ObjectID(object);
	}
}

StringName Callable::get_method() const {
	if (is_custom()) {
		return get_custom()->get_method();
	}
	return method;
}

int Callable::get_argument_count(bool *r_is_valid) const {
	if (is_custom()) {
		bool valid = false;
		return custom->get_argument_count(r_is_valid ? *r_is_valid : valid);
	} else if (!is_null()) {
		return get_object()->get_method_argument_count(method, r_is_valid);
	} else {
		if (r_is_valid) {
			*r_is_valid = false;
		}
		return 0;
	}
}

int Callable::get_bound_arguments_count() const {
	if (!is_null() && is_custom()) {
		return custom->get_bound_arguments_count();
	} else {
		return 0;
	}
}

void Callable::get_bound_arguments_ref(Vector<Variant> &r_arguments) const {
	if (!is_null() && is_custom()) {
		custom->get_bound_arguments(r_arguments);
	} else {
		r_arguments.clear();
	}
}

Array Callable::get_bound_arguments() const {
	Vector<Variant> arr;
	get_bound_arguments_ref(arr);
	Array ret;
	ret.resize(arr.size());
	for (int i = 0; i < arr.size(); i++) {
		ret[i] = arr[i];
	}
	return ret;
}

int Callable::get_unbound_arguments_count() const {
	if (!is_null() && is_custom()) {
		return custom->get_unbound_arguments_count();
	} else {
		return 0;
	}
}

CallableCustom *Callable::get_custom() const {
	ERR_FAIL_COND_V_MSG(!is_custom(), nullptr,
			vformat("Can't get custom on non-CallableCustom \"%s\".", operator String()));
	return custom;
}

const Callable *Callable::get_base_comparator() const {
	const Callable *comparator = nullptr;
	if (is_custom()) {
		comparator = custom->get_base_comparator();
	}
	if (comparator) {
		return comparator;
	} else {
		return this;
	}
}

uint32_t Callable::hash() const {
	if (is_custom()) {
		return custom->hash();
	} else {
		uint32_t hash = method.hash();
		hash = hash_murmur3_one_64(object, hash);
		return hash_fmix32(hash);
	}
}

bool Callable::operator==(const Callable &p_callable) const {
	bool custom_a = is_custom();
	bool custom_b = p_callable.is_custom();

	if (custom_a == custom_b) {
		if (custom_a) {
			if (custom == p_callable.custom) {
				return true; //same pointer, don't even compare
			}

			CallableCustom::CompareEqualFunc eq_a = custom->get_compare_equal_func();
			CallableCustom::CompareEqualFunc eq_b = p_callable.custom->get_compare_equal_func();
			if (eq_a == eq_b) {
				return eq_a(custom, p_callable.custom);
			} else {
				return false;
			}
		} else {
			return object == p_callable.object && method == p_callable.method;
		}
	} else {
		return false;
	}
}

bool Callable::operator!=(const Callable &p_callable) const {
	return !(*this == p_callable);
}

bool Callable::operator<(const Callable &p_callable) const {
	bool custom_a = is_custom();
	bool custom_b = p_callable.is_custom();

	if (custom_a == custom_b) {
		if (custom_a) {
			if (custom == p_callable.custom) {
				return false; //same pointer, don't even compare
			}

			CallableCustom::CompareLessFunc less_a = custom->get_compare_less_func();
			CallableCustom::CompareLessFunc less_b = p_callable.custom->get_compare_less_func();
			if (less_a == less_b) {
				return less_a(custom, p_callable.custom);
			} else {
				return less_a < less_b; //it's something..
			}

		} else {
			if (object == p_callable.object) {
				return method < p_callable.method;
			} else {
				return object < p_callable.object;
			}
		}
	} else {
		return int(custom_a ? 1 : 0) < int(custom_b ? 1 : 0);
	}
}

void Callable::operator=(const Callable &p_callable) {
	CallableCustom *cleanup_ref = nullptr;
	if (is_custom()) {
		if (p_callable.is_custom()) {
			if (custom == p_callable.custom) {
				return;
			}
		}
		cleanup_ref = custom;
		custom = nullptr;
	}

	if (p_callable.is_custom()) {
		method = StringName();
		object = 0;
		if (p_callable.custom->ref_count.ref()) {
			custom = p_callable.custom;
		}
	} else {
		method = p_callable.method;
		object = p_callable.object;
	}

	if (cleanup_ref != nullptr && cleanup_ref->ref_count.unref()) {
		memdelete(cleanup_ref);
	}
	cleanup_ref = nullptr;
}

Callable::operator String() const {
	if (is_custom()) {
		return custom->get_as_text();
	} else {
		if (is_null()) {
			return "null::null";
		}

		Object *base = get_object();
		if (base) {
			String class_name = base->get_class();
			Ref<Script> script = base->get_script();
			if (script.is_valid()) {
				if (!script->get_global_name().is_empty()) {
					class_name += "(" + script->get_global_name() + ")";
				} else if (script->get_path().is_resource_file()) {
					class_name += "(" + script->get_path().get_file() + ")";
				}
			}
			return class_name + "::" + String(method);
		} else {
			return "null::" + String(method);
		}
	}
}

Callable Callable::create(const Variant &p_variant, const StringName &p_method) {
	ERR_FAIL_COND_V_MSG(p_method == StringName(), Callable(), "Method argument to Callable::create method must be a non-empty string.");

	switch (p_variant.get_type()) {
		case Variant::NIL:
			return Callable(ObjectID(), p_method);
		case Variant::OBJECT:
			return Callable(p_variant.operator ObjectID(), p_method);
		default:
			return Callable(memnew(VariantCallable(p_variant, p_method)));
	}
}

Callable::Callable(const Object *p_object, const StringName &p_method) {
	if (unlikely(p_method == StringName())) {
		object = 0;
		ERR_FAIL_MSG("Method argument to Callable constructor must be a non-empty string.");
	}
	if (unlikely(p_object == nullptr)) {
		object = 0;
		ERR_FAIL_MSG("Object argument to Callable constructor must be non-null.");
	}

	object = p_object->get_instance_id();
	method = p_method;
}

Callable::Callable(ObjectID p_object, const StringName &p_method) {
	if (unlikely(p_method == StringName())) {
		object = 0;
		ERR_FAIL_MSG("Method argument to Callable constructor must be a non-empty string.");
	}

	object = p_object;
	method = p_method;
}

Callable::Callable(CallableCustom *p_custom) {
	if (unlikely(p_custom->referenced)) {
		object = 0;
		ERR_FAIL_MSG("Callable custom is already referenced.");
	}
	p_custom->referenced = true;
	object = 0; //ensure object is all zero, since pointer may be 32 bits
	custom = p_custom;
}

Callable::Callable(const Callable &p_callable) {
	if (p_callable.is_custom()) {
		if (!p_callable.custom->ref_count.ref()) {
			object = 0;
		} else {
			object = 0;
			custom = p_callable.custom;
		}
	} else {
		method = p_callable.method;
		object = p_callable.object;
	}
}

Callable::~Callable() {
	if (is_custom()) {
		if (custom->ref_count.unref()) {
			memdelete(custom);
			custom = nullptr;
		}
	}
}

bool CallableCustom::is_valid() const {
	// Sensible default implementation so most custom callables don't need their own.
	return ObjectDB::get_instance(get_object());
}

StringName CallableCustom::get_method() const {
	ERR_FAIL_V_MSG(StringName(), vformat("Can't get method on CallableCustom \"%s\".", get_as_text()));
}

Error CallableCustom::rpc(int p_peer_id, const Variant **p_arguments, int p_argcount, Callable::CallError &r_call_error) const {
	r_call_error.error = Callable::CallError::CALL_ERROR_INVALID_METHOD;
	r_call_error.argument = 0;
	r_call_error.expected = 0;
	return ERR_UNCONFIGURED;
}

const Callable *CallableCustom::get_base_comparator() const {
	return nullptr;
}

int CallableCustom::get_argument_count(bool &r_is_valid) const {
	r_is_valid = false;
	return 0;
}

int CallableCustom::get_bound_arguments_count() const {
	return 0;
}

void CallableCustom::get_bound_arguments(Vector<Variant> &r_arguments) const {
	r_arguments.clear();
}

int CallableCustom::get_unbound_arguments_count() const {
	return 0;
}

CallableCustom::CallableCustom() {
	ref_count.init();
}

//////////////////////////////////

Object *Signal::get_object() const {
	return ObjectDB::get_instance(object);
}

ObjectID Signal::get_object_id() const {
	return object;
}

StringName Signal::get_name() const {
	return name;
}

bool Signal::operator==(const Signal &p_signal) const {
	return object == p_signal.object && name == p_signal.name;
}

bool Signal::operator!=(const Signal &p_signal) const {
	return object != p_signal.object || name != p_signal.name;
}

bool Signal::operator<(const Signal &p_signal) const {
	if (object == p_signal.object) {
		return name < p_signal.name;
	} else {
		return object < p_signal.object;
	}
}

Signal::operator String() const {
	Object *base = get_object();
	if (base) {
		String class_name = base->get_class();
		Ref<Script> script = base->get_script();
		if (script.is_valid() && script->get_path().is_resource_file()) {
			class_name += "(" + script->get_path().get_file() + ")";
		}
		return class_name + "::[signal]" + String(name);
	} else {
		return "null::[signal]" + String(name);
	}
}

Error Signal::emit(const Variant **p_arguments, int p_argcount) const {
	Object *obj = ObjectDB::get_instance(object);
	if (!obj) {
		return ERR_INVALID_DATA;
	}

	return obj->emit_signalp(name, p_arguments, p_argcount);
}

Error Signal::connect(const Callable &p_callable, uint32_t p_flags) {
	Object *obj = get_object();
	ERR_FAIL_NULL_V(obj, ERR_UNCONFIGURED);

	return obj->connect(name, p_callable, p_flags);
}

void Signal::disconnect(const Callable &p_callable) {
	Object *obj = get_object();
	ERR_FAIL_NULL(obj);
	obj->disconnect(name, p_callable);
}

bool Signal::is_connected(const Callable &p_callable) const {
	Object *obj = get_object();
	ERR_FAIL_NULL_V(obj, false);

	return obj->is_connected(name, p_callable);
}

bool Signal::has_connections() const {
	Object *obj = get_object();
	ERR_FAIL_NULL_V(obj, false);

	return obj->has_connections(name);
}

Array Signal::get_connections() const {
	Object *obj = get_object();
	if (!obj) {
		return Array();
	}

	List<Object::Connection> connections;
	obj->get_signal_connection_list(name, &connections);

	Array arr;
	for (const Object::Connection &E : connections) {
		arr.push_back(E);
	}
	return arr;
}

Signal::Signal(const Object *p_object, const StringName &p_name) {
	ERR_FAIL_NULL_MSG(p_object, "Object argument to Signal constructor must be non-null.");

	object = p_object->get_instance_id();
	name = p_name;
}

Signal::Signal(ObjectID p_object, const StringName &p_name) {
	object = p_object;
	name = p_name;
}

bool CallableComparator::operator()(const Variant &p_l, const Variant &p_r) const {
	const Variant *args[2] = { &p_l, &p_r };
	Callable::CallError err;
	Variant res;
	func.callp(args, 2, res, err);
	ERR_FAIL_COND_V_MSG(err.error != Callable::CallError::CALL_OK, false,
			"Error calling compare method: " + Variant::get_callable_error_text(func, args, 2, err));
	return res;
}

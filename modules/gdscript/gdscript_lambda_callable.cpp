/**************************************************************************/
/*  gdscript_lambda_callable.cpp                                          */
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

#include "gdscript_lambda_callable.h"

#include "gdscript.h"

#include "core/templates/hashfuncs.h"

bool GDScriptLambdaCallable::compare_equal(const CallableCustom *p_a, const CallableCustom *p_b) {
	// Lambda callables are only compared by reference.
	return p_a == p_b;
}

bool GDScriptLambdaCallable::compare_less(const CallableCustom *p_a, const CallableCustom *p_b) {
	// Lambda callables are only compared by reference.
	return p_a < p_b;
}

bool GDScriptLambdaCallable::is_valid() const {
	return CallableCustom::is_valid() && function != nullptr;
}

uint32_t GDScriptLambdaCallable::hash() const {
	return h;
}

String GDScriptLambdaCallable::get_as_text() const {
	if (function == nullptr) {
		return "<invalid lambda>";
	}
	if (function->get_name() != StringName()) {
		return function->get_name().operator String() + "(lambda)";
	}
	return "(anonymous lambda)";
}

CallableCustom::CompareEqualFunc GDScriptLambdaCallable::get_compare_equal_func() const {
	return compare_equal;
}

CallableCustom::CompareLessFunc GDScriptLambdaCallable::get_compare_less_func() const {
	return compare_less;
}

ObjectID GDScriptLambdaCallable::get_object() const {
	return script->get_instance_id();
}

StringName GDScriptLambdaCallable::get_method() const {
	return function->get_name();
}

int GDScriptLambdaCallable::get_argument_count(bool &r_is_valid) const {
	if (function == nullptr) {
		r_is_valid = false;
		return 0;
	}
	r_is_valid = true;
	return function->get_argument_count() - captures.size();
}

void GDScriptLambdaCallable::call(const Variant **p_arguments, int p_argcount, Variant &r_return_value, Callable::CallError &r_call_error) const {
	int captures_amount = captures.size();

	if (function == nullptr) {
		r_return_value = Variant();
		r_call_error.error = Callable::CallError::CALL_ERROR_INSTANCE_IS_NULL;
		return;
	}

	if (captures_amount > 0) {
		const int total_argcount = p_argcount + captures_amount;
		const Variant **args = (const Variant **)alloca(sizeof(Variant *) * total_argcount);
		for (int i = 0; i < captures_amount; i++) {
			args[i] = &captures[i];
			if (captures[i].get_type() == Variant::OBJECT) {
				bool was_freed = false;
				captures[i].get_validated_object_with_check(was_freed);
				if (was_freed) {
					ERR_PRINT(vformat(R"(Lambda capture at index %d was freed. Passed "null" instead.)", i));
					static Variant nil;
					args[i] = &nil;
				}
			}
		}
		for (int i = 0; i < p_argcount; i++) {
			args[i + captures_amount] = p_arguments[i];
		}

		r_return_value = function->call(nullptr, args, total_argcount, r_call_error);
		switch (r_call_error.error) {
			case Callable::CallError::CALL_ERROR_INVALID_ARGUMENT:
				r_call_error.argument -= captures_amount;
#ifdef DEBUG_ENABLED
				if (r_call_error.argument < 0) {
					ERR_PRINT(vformat("GDScript bug (please report): Invalid value of lambda capture at index %d.", captures_amount + r_call_error.argument));
					r_call_error.error = Callable::CallError::CALL_ERROR_INVALID_METHOD; // TODO: Add a more suitable error code.
					r_call_error.argument = 0;
					r_call_error.expected = 0;
				}
#endif
				break;
			case Callable::CallError::CALL_ERROR_TOO_MANY_ARGUMENTS:
			case Callable::CallError::CALL_ERROR_TOO_FEW_ARGUMENTS:
				r_call_error.expected -= captures_amount;
#ifdef DEBUG_ENABLED
				if (r_call_error.expected < 0) {
					ERR_PRINT("GDScript bug (please report): Invalid lambda captures count.");
					r_call_error.error = Callable::CallError::CALL_ERROR_INVALID_METHOD; // TODO: Add a more suitable error code.
					r_call_error.argument = 0;
					r_call_error.expected = 0;
				}
#endif
				break;
			default:
				break;
		}
	} else {
		r_return_value = function->call(nullptr, p_arguments, p_argcount, r_call_error);
	}
}

GDScriptLambdaCallable::GDScriptLambdaCallable(Ref<GDScript> p_script, GDScriptFunction *p_function, const Vector<Variant> &p_captures) :
		function(p_function) {
	ERR_FAIL_COND(p_script.is_null());
	ERR_FAIL_NULL(p_function);
	script = p_script;
	captures = p_captures;

	h = (uint32_t)hash_murmur3_one_64((uint64_t)this);
}

bool GDScriptLambdaSelfCallable::compare_equal(const CallableCustom *p_a, const CallableCustom *p_b) {
	// Lambda callables are only compared by reference.
	return p_a == p_b;
}

bool GDScriptLambdaSelfCallable::compare_less(const CallableCustom *p_a, const CallableCustom *p_b) {
	// Lambda callables are only compared by reference.
	return p_a < p_b;
}

bool GDScriptLambdaSelfCallable::is_valid() const {
	return CallableCustom::is_valid() && function != nullptr;
}

uint32_t GDScriptLambdaSelfCallable::hash() const {
	return h;
}

String GDScriptLambdaSelfCallable::get_as_text() const {
	if (function == nullptr) {
		return "<invalid self lambda>";
	}
	if (function->get_name() != StringName()) {
		return function->get_name().operator String() + "(self lambda)";
	}
	return "(anonymous self lambda)";
}

CallableCustom::CompareEqualFunc GDScriptLambdaSelfCallable::get_compare_equal_func() const {
	return compare_equal;
}

CallableCustom::CompareLessFunc GDScriptLambdaSelfCallable::get_compare_less_func() const {
	return compare_less;
}

ObjectID GDScriptLambdaSelfCallable::get_object() const {
	return object->get_instance_id();
}

StringName GDScriptLambdaSelfCallable::get_method() const {
	return function->get_name();
}

int GDScriptLambdaSelfCallable::get_argument_count(bool &r_is_valid) const {
	if (function == nullptr) {
		r_is_valid = false;
		return 0;
	}
	r_is_valid = true;
	return function->get_argument_count() - captures.size();
}

void GDScriptLambdaSelfCallable::call(const Variant **p_arguments, int p_argcount, Variant &r_return_value, Callable::CallError &r_call_error) const {
#ifdef DEBUG_ENABLED
	if (object->get_script_instance() == nullptr || object->get_script_instance()->get_language() != GDScriptLanguage::get_singleton()) {
		ERR_PRINT("Trying to call a lambda with an invalid instance.");
		r_call_error.error = Callable::CallError::CALL_ERROR_INSTANCE_IS_NULL;
		return;
	}
#endif

	int captures_amount = captures.size();

	if (function == nullptr) {
		r_return_value = Variant();
		r_call_error.error = Callable::CallError::CALL_ERROR_INSTANCE_IS_NULL;
		return;
	}

	if (captures_amount > 0) {
		const int total_argcount = p_argcount + captures_amount;
		const Variant **args = (const Variant **)alloca(sizeof(Variant *) * total_argcount);
		for (int i = 0; i < captures_amount; i++) {
			args[i] = &captures[i];
			if (captures[i].get_type() == Variant::OBJECT) {
				bool was_freed = false;
				captures[i].get_validated_object_with_check(was_freed);
				if (was_freed) {
					ERR_PRINT(vformat(R"(Lambda capture at index %d was freed. Passed "null" instead.)", i));
					static Variant nil;
					args[i] = &nil;
				}
			}
		}
		for (int i = 0; i < p_argcount; i++) {
			args[i + captures_amount] = p_arguments[i];
		}

		r_return_value = function->call(static_cast<GDScriptInstance *>(object->get_script_instance()), args, total_argcount, r_call_error);
		switch (r_call_error.error) {
			case Callable::CallError::CALL_ERROR_INVALID_ARGUMENT:
				r_call_error.argument -= captures_amount;
#ifdef DEBUG_ENABLED
				if (r_call_error.argument < 0) {
					ERR_PRINT(vformat("GDScript bug (please report): Invalid value of lambda capture at index %d.", captures_amount + r_call_error.argument));
					r_call_error.error = Callable::CallError::CALL_ERROR_INVALID_METHOD; // TODO: Add a more suitable error code.
					r_call_error.argument = 0;
					r_call_error.expected = 0;
				}
#endif
				break;
			case Callable::CallError::CALL_ERROR_TOO_MANY_ARGUMENTS:
			case Callable::CallError::CALL_ERROR_TOO_FEW_ARGUMENTS:
				r_call_error.expected -= captures_amount;
#ifdef DEBUG_ENABLED
				if (r_call_error.expected < 0) {
					ERR_PRINT("GDScript bug (please report): Invalid lambda captures count.");
					r_call_error.error = Callable::CallError::CALL_ERROR_INVALID_METHOD; // TODO: Add a more suitable error code.
					r_call_error.argument = 0;
					r_call_error.expected = 0;
				}
#endif
				break;
			default:
				break;
		}
	} else {
		r_return_value = function->call(static_cast<GDScriptInstance *>(object->get_script_instance()), p_arguments, p_argcount, r_call_error);
	}
}

GDScriptLambdaSelfCallable::GDScriptLambdaSelfCallable(Ref<RefCounted> p_self, GDScriptFunction *p_function, const Vector<Variant> &p_captures) :
		function(p_function) {
	ERR_FAIL_COND(p_self.is_null());
	ERR_FAIL_NULL(p_function);
	reference = p_self;
	object = p_self.ptr();
	captures = p_captures;

	h = (uint32_t)hash_murmur3_one_64((uint64_t)this);
}

GDScriptLambdaSelfCallable::GDScriptLambdaSelfCallable(Object *p_self, GDScriptFunction *p_function, const Vector<Variant> &p_captures) :
		function(p_function) {
	ERR_FAIL_NULL(p_self);
	ERR_FAIL_NULL(p_function);
	object = p_self;
	captures = p_captures;

	h = (uint32_t)hash_murmur3_one_64((uint64_t)this);
}

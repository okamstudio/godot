/**************************************************************************/
/*  callable_method_pointer.h                                             */
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

#ifndef CALLABLE_METHOD_POINTER_H
#define CALLABLE_METHOD_POINTER_H

#include "core/object/object.h"
#include "core/variant/binder_common.h"
#include "core/variant/callable.h"

#include <type_traits>

class CallableCustomMethodPointerBase : public CallableCustom {
	uint32_t *comp_ptr = nullptr;
	uint32_t comp_size;
	uint32_t h;
#ifdef DEBUG_METHODS_ENABLED
	const char *text = "";
#endif
	static bool compare_equal(const CallableCustom *p_a, const CallableCustom *p_b);
	static bool compare_less(const CallableCustom *p_a, const CallableCustom *p_b);

protected:
	void _setup(uint32_t *p_base_ptr, uint32_t p_ptr_size);

public:
	virtual StringName get_method() const {
#ifdef DEBUG_METHODS_ENABLED
		return StringName(text);
#else
		return StringName();
#endif
	}

#ifdef DEBUG_METHODS_ENABLED
	void set_text(const char *p_text) {
		text = p_text;
	}
	virtual String get_as_text() const {
		return text;
	}
#else
	virtual String get_as_text() const {
		return String();
	}
#endif
	virtual CompareEqualFunc get_compare_equal_func() const;
	virtual CompareLessFunc get_compare_less_func() const;

	virtual uint32_t hash() const;
};

template <typename T, typename R, typename... P>
class CallableCustomMethodPointer : public CallableCustomMethodPointerBase {
	struct Data {
		T *instance;
		uint64_t object_id;
		R(T::*method)
		(P...);
	} data;

public:
	virtual ObjectID get_object() const {
		if (ObjectDB::get_instance(ObjectID(data.object_id)) == nullptr) {
			return ObjectID();
		}
		return data.instance->get_instance_id();
	}

	virtual int get_argument_count(bool &r_is_valid) const {
		r_is_valid = true;
		return sizeof...(P);
	}

	virtual void call(const Variant **p_arguments, int p_argcount, Variant &r_return_value, Callable::CallError &r_call_error) const {
		ERR_FAIL_NULL_MSG(ObjectDB::get_instance(ObjectID(data.object_id)), "Invalid Object id '" + uitos(data.object_id) + "', can't call method.");
		if constexpr (std::is_same<R, void>::value) {
			call_with_variant_args(data.instance, data.method, p_arguments, p_argcount, r_call_error);
		} else {
			call_with_variant_args_ret(data.instance, data.method, p_arguments, p_argcount, r_return_value, r_call_error);
		}
	}

	CallableCustomMethodPointer(T *p_instance, R (T::*p_method)(P...)) {
		memset(&data, 0, sizeof(Data)); // Clear beforehand, may have padding bytes.
		data.instance = p_instance;
		data.object_id = p_instance->get_instance_id();
		data.method = p_method;
		_setup((uint32_t *)&data, sizeof(Data));
	}
};

template <typename T, typename... P>
Callable create_custom_callable_function_pointer(T *p_instance,
#ifdef DEBUG_METHODS_ENABLED
		const char *p_func_text,
#endif
		void (T::*p_method)(P...)) {
	typedef CallableCustomMethodPointer<T, void, P...> CCMP; // Messes with memnew otherwise.
	CCMP *ccmp = memnew(CCMP(p_instance, p_method));
#ifdef DEBUG_METHODS_ENABLED
	ccmp->set_text(p_func_text + 1); // Try to get rid of the ampersand.
#endif
	return Callable(ccmp);
}

template <typename T, typename R, typename... P>
Callable create_custom_callable_function_pointer(T *p_instance,
#ifdef DEBUG_METHODS_ENABLED
		const char *p_func_text,
#endif
		R (T::*p_method)(P...)) {
	typedef CallableCustomMethodPointer<T, R, P...> CCMP; // Messes with memnew otherwise.
	CCMP *ccmp = memnew(CCMP(p_instance, p_method));
#ifdef DEBUG_METHODS_ENABLED
	ccmp->set_text(p_func_text + 1); // Try to get rid of the ampersand.
#endif
	return Callable(ccmp);
}

// CONST VERSION

template <typename T, typename R, typename... P>
class CallableCustomMethodPointerC : public CallableCustomMethodPointerBase {
	struct Data {
		T *instance;
		uint64_t object_id;
		R(T::*method)
		(P...) const;
	} data;

public:
	virtual ObjectID get_object() const override {
		if (ObjectDB::get_instance(ObjectID(data.object_id)) == nullptr) {
			return ObjectID();
		}
		return data.instance->get_instance_id();
	}

	virtual int get_argument_count(bool &r_is_valid) const override {
		r_is_valid = true;
		return sizeof...(P);
	}

	virtual void call(const Variant **p_arguments, int p_argcount, Variant &r_return_value, Callable::CallError &r_call_error) const override {
		ERR_FAIL_NULL_MSG(ObjectDB::get_instance(ObjectID(data.object_id)), "Invalid Object id '" + uitos(data.object_id) + "', can't call method.");
		if constexpr (std::is_same<R, void>::value) {
			call_with_variant_argsc(data.instance, data.method, p_arguments, p_argcount, r_call_error);
		} else {
			call_with_variant_args_retc(data.instance, data.method, p_arguments, p_argcount, r_return_value, r_call_error);
		}
	}

	CallableCustomMethodPointerC(T *p_instance, R (T::*p_method)(P...) const) {
		memset(&data, 0, sizeof(Data)); // Clear beforehand, may have padding bytes.
		data.instance = p_instance;
		data.object_id = p_instance->get_instance_id();
		data.method = p_method;
		_setup((uint32_t *)&data, sizeof(Data));
	}
};

template <typename T, typename... P>
Callable create_custom_callable_function_pointer(T *p_instance,
#ifdef DEBUG_METHODS_ENABLED
		const char *p_func_text,
#endif
		void (T::*p_method)(P...) const) {
	typedef CallableCustomMethodPointerC<T, void, P...> CCMP; // Messes with memnew otherwise.
	CCMP *ccmp = memnew(CCMP(p_instance, p_method));
#ifdef DEBUG_METHODS_ENABLED
	ccmp->set_text(p_func_text + 1); // Try to get rid of the ampersand.
#endif
	return Callable(ccmp);
}

template <typename T, typename R, typename... P>
Callable create_custom_callable_function_pointer(T *p_instance,
#ifdef DEBUG_METHODS_ENABLED
		const char *p_func_text,
#endif
		R (T::*p_method)(P...) const) {
	typedef CallableCustomMethodPointerC<T, R, P...> CCMP; // Messes with memnew otherwise.
	CCMP *ccmp = memnew(CCMP(p_instance, p_method));
#ifdef DEBUG_METHODS_ENABLED
	ccmp->set_text(p_func_text + 1); // Try to get rid of the ampersand.
#endif
	return Callable(ccmp);
}

#ifdef DEBUG_METHODS_ENABLED
#define callable_mp(I, M) create_custom_callable_function_pointer(I, #M, M)
#else
#define callable_mp(I, M) create_custom_callable_function_pointer(I, M)
#endif

// STATIC VERSIONS

template <typename R, typename... P>
class CallableCustomStaticMethodPointer : public CallableCustomMethodPointerBase {
	struct Data {
		R(*method)
		(P...);
	} data;

public:
	virtual bool is_valid() const override {
		return true;
	}

	virtual ObjectID get_object() const override {
		return ObjectID();
	}

	virtual int get_argument_count(bool &r_is_valid) const override {
		r_is_valid = true;
		return sizeof...(P);
	}

	virtual void call(const Variant **p_arguments, int p_argcount, Variant &r_return_value, Callable::CallError &r_call_error) const override {
		if constexpr (std::is_same<R, void>::value) {
			call_with_variant_args_static(data.method, p_arguments, p_argcount, r_call_error);
		} else {
			call_with_variant_args_static_ret(data.method, p_arguments, p_argcount, r_return_value, r_call_error);
		}
	}

	CallableCustomStaticMethodPointer(R (*p_method)(P...)) {
		memset(&data, 0, sizeof(Data)); // Clear beforehand, may have padding bytes.
		data.method = p_method;
		_setup((uint32_t *)&data, sizeof(Data));
	}
};

template <typename... P>
Callable create_custom_callable_static_function_pointer(
#ifdef DEBUG_METHODS_ENABLED
		const char *p_func_text,
#endif
		void (*p_method)(P...)) {
	typedef CallableCustomStaticMethodPointer<void, P...> CCMP; // Messes with memnew otherwise.
	CCMP *ccmp = memnew(CCMP(p_method));
#ifdef DEBUG_METHODS_ENABLED
	ccmp->set_text(p_func_text + 1); // Try to get rid of the ampersand.
#endif
	return Callable(ccmp);
}

template <typename R, typename... P>
Callable create_custom_callable_static_function_pointer(
#ifdef DEBUG_METHODS_ENABLED
		const char *p_func_text,
#endif
		R (*p_method)(P...)) {
	typedef CallableCustomStaticMethodPointer<R, P...> CCMP; // Messes with memnew otherwise.
	CCMP *ccmp = memnew(CCMP(p_method));
#ifdef DEBUG_METHODS_ENABLED
	ccmp->set_text(p_func_text + 1); // Try to get rid of the ampersand.
#endif
	return Callable(ccmp);
}

#ifdef DEBUG_METHODS_ENABLED
#define callable_mp_static(M) create_custom_callable_static_function_pointer(#M, M)
#else
#define callable_mp_static(M) create_custom_callable_static_function_pointer(M)
#endif

#endif // CALLABLE_METHOD_POINTER_H

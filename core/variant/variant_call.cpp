/**************************************************************************/
/*  variant_call.cpp                                                      */
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

#include "variant.h"

#include "core/crypto/crypto_core.h"
#include "core/debugger/engine_debugger.h"
#include "core/io/compression.h"
#include "core/io/marshalls.h"
#include "core/object/class_db.h"
#include "core/os/os.h"
#include "core/templates/local_vector.h"
#include "core/templates/oa_hash_map.h"

typedef void (*VariantFunc)(Variant &r_ret, Variant &p_self, const Variant **p_args);
typedef void (*VariantConstructFunc)(Variant &r_ret, const Variant **p_args);

template <typename R, typename... P>
static _FORCE_INLINE_ void vc_static_method_call(R (*method)(P...), const Variant **p_args, int p_argcount, Variant &r_ret, const Vector<Variant> &p_defvals, Callable::CallError &r_error) {
	call_with_variant_args_static_ret_dv(method, p_args, p_argcount, r_ret, r_error, p_defvals);
}

template <typename... P>
static _FORCE_INLINE_ void vc_static_method_call(void (*method)(P...), const Variant **p_args, int p_argcount, Variant &r_ret, const Vector<Variant> &p_defvals, Callable::CallError &r_error) {
	call_with_variant_args_static_dv(method, p_args, p_argcount, r_error, p_defvals);
}

template <typename R, typename T, typename... P>
static _FORCE_INLINE_ void vc_method_call(R (T::*method)(P...), Variant *base, const Variant **p_args, int p_argcount, Variant &r_ret, const Vector<Variant> &p_defvals, Callable::CallError &r_error) {
	call_with_variant_args_ret_dv(VariantGetInternalPtr<T>::get_ptr(base), method, p_args, p_argcount, r_ret, r_error, p_defvals);
}

template <typename R, typename T, typename... P>
static _FORCE_INLINE_ void vc_method_call(R (T::*method)(P...) const, Variant *base, const Variant **p_args, int p_argcount, Variant &r_ret, const Vector<Variant> &p_defvals, Callable::CallError &r_error) {
	call_with_variant_args_retc_dv(VariantGetInternalPtr<T>::get_ptr(base), method, p_args, p_argcount, r_ret, r_error, p_defvals);
}

template <typename T, typename... P>
static _FORCE_INLINE_ void vc_method_call(void (T::*method)(P...), Variant *base, const Variant **p_args, int p_argcount, Variant &r_ret, const Vector<Variant> &p_defvals, Callable::CallError &r_error) {
	VariantInternal::clear(&r_ret);
	call_with_variant_args_dv(VariantGetInternalPtr<T>::get_ptr(base), method, p_args, p_argcount, r_error, p_defvals);
}

template <typename T, typename... P>
static _FORCE_INLINE_ void vc_method_call(void (T::*method)(P...) const, Variant *base, const Variant **p_args, int p_argcount, Variant &r_ret, const Vector<Variant> &p_defvals, Callable::CallError &r_error) {
	VariantInternal::clear(&r_ret);
	call_with_variant_argsc_dv(VariantGetInternalPtr<T>::get_ptr(base), method, p_args, p_argcount, r_error, p_defvals);
}

template <typename From, typename R, typename T, typename... P>
static _FORCE_INLINE_ void vc_convert_method_call(R (T::*method)(P...), Variant *base, const Variant **p_args, int p_argcount, Variant &r_ret, const Vector<Variant> &p_defvals, Callable::CallError &r_error) {
	T converted(static_cast<T>(*VariantGetInternalPtr<From>::get_ptr(base)));
	call_with_variant_args_ret_dv(&converted, method, p_args, p_argcount, r_ret, r_error, p_defvals);
}

template <typename From, typename R, typename T, typename... P>
static _FORCE_INLINE_ void vc_convert_method_call(R (T::*method)(P...) const, Variant *base, const Variant **p_args, int p_argcount, Variant &r_ret, const Vector<Variant> &p_defvals, Callable::CallError &r_error) {
	T converted(static_cast<T>(*VariantGetInternalPtr<From>::get_ptr(base)));
	call_with_variant_args_retc_dv(&converted, method, p_args, p_argcount, r_ret, r_error, p_defvals);
}

template <typename From, typename T, typename... P>
static _FORCE_INLINE_ void vc_convert_method_call(void (T::*method)(P...), Variant *base, const Variant **p_args, int p_argcount, Variant &r_ret, const Vector<Variant> &p_defvals, Callable::CallError &r_error) {
	T converted(static_cast<T>(*VariantGetInternalPtr<From>::get_ptr(base)));
	call_with_variant_args_dv(&converted, method, p_args, p_argcount, r_error, p_defvals);
}

template <typename From, typename T, typename... P>
static _FORCE_INLINE_ void vc_convert_method_call(void (T::*method)(P...) const, Variant *base, const Variant **p_args, int p_argcount, Variant &r_ret, const Vector<Variant> &p_defvals, Callable::CallError &r_error) {
	T converted(static_cast<T>(*VariantGetInternalPtr<From>::get_ptr(base)));
	call_with_variant_argsc_dv(&converted, method, p_args, p_argcount, r_error, p_defvals);
}

template <typename R, typename T, typename... P>
static _FORCE_INLINE_ void vc_method_call_static(R (*method)(T *, P...), Variant *base, const Variant **p_args, int p_argcount, Variant &r_ret, const Vector<Variant> &p_defvals, Callable::CallError &r_error) {
	call_with_variant_args_retc_static_helper_dv(VariantGetInternalPtr<T>::get_ptr(base), method, p_args, p_argcount, r_ret, p_defvals, r_error);
}

template <typename T, typename... P>
static _FORCE_INLINE_ void vc_method_call_static(void (*method)(T *, P...), Variant *base, const Variant **p_args, int p_argcount, Variant &r_ret, const Vector<Variant> &p_defvals, Callable::CallError &r_error) {
	call_with_variant_args_static_helper_dv(VariantGetInternalPtr<T>::get_ptr(base), method, p_args, p_argcount, p_defvals, r_error);
}

template <typename R, typename T, typename... P>
static _FORCE_INLINE_ void vc_validated_call(R (T::*method)(P...), Variant *base, const Variant **p_args, Variant *r_ret) {
	call_with_validated_variant_args_ret(base, method, p_args, r_ret);
}

template <typename R, typename T, typename... P>
static _FORCE_INLINE_ void vc_validated_call(R (T::*method)(P...) const, Variant *base, const Variant **p_args, Variant *r_ret) {
	call_with_validated_variant_args_retc(base, method, p_args, r_ret);
}
template <typename T, typename... P>
static _FORCE_INLINE_ void vc_validated_call(void (T::*method)(P...), Variant *base, const Variant **p_args, Variant *r_ret) {
	call_with_validated_variant_args(base, method, p_args);
}

template <typename T, typename... P>
static _FORCE_INLINE_ void vc_validated_call(void (T::*method)(P...) const, Variant *base, const Variant **p_args, Variant *r_ret) {
	call_with_validated_variant_argsc(base, method, p_args);
}

template <typename From, typename R, typename T, typename... P>
static _FORCE_INLINE_ void vc_convert_validated_call(R (T::*method)(P...), Variant *base, const Variant **p_args, Variant *r_ret) {
	T converted(static_cast<T>(*VariantGetInternalPtr<From>::get_ptr(base)));
	call_with_validated_variant_args_ret_helper<T, R, P...>(&converted, method, p_args, r_ret, BuildIndexSequence<sizeof...(P)>{});
}

template <typename From, typename R, typename T, typename... P>
static _FORCE_INLINE_ void vc_convert_validated_call(R (T::*method)(P...) const, Variant *base, const Variant **p_args, Variant *r_ret) {
	T converted(static_cast<T>(*VariantGetInternalPtr<From>::get_ptr(base)));
	call_with_validated_variant_args_retc_helper<T, R, P...>(&converted, method, p_args, r_ret, BuildIndexSequence<sizeof...(P)>{});
}
template <typename From, typename T, typename... P>
static _FORCE_INLINE_ void vc_convert_validated_call(void (T::*method)(P...), Variant *base, const Variant **p_args, Variant *r_ret) {
	T converted(static_cast<T>(*VariantGetInternalPtr<From>::get_ptr(base)));
	call_with_validated_variant_args_helper<T, P...>(&converted, method, p_args, r_ret, BuildIndexSequence<sizeof...(P)>{});
}

template <typename From, typename T, typename... P>
static _FORCE_INLINE_ void vc_convert_validated_call(void (T::*method)(P...) const, Variant *base, const Variant **p_args, Variant *r_ret) {
	T converted(static_cast<T>(*VariantGetInternalPtr<From>::get_ptr(base)));
	call_with_validated_variant_argsc_helper<T, P...>(&converted, method, p_args, r_ret, BuildIndexSequence<sizeof...(P)>{});
}

template <typename R, typename T, typename... P>
static _FORCE_INLINE_ void vc_validated_call_static(R (*method)(T *, P...), Variant *base, const Variant **p_args, Variant *r_ret) {
	call_with_validated_variant_args_static_retc(base, method, p_args, r_ret);
}

template <typename T, typename... P>
static _FORCE_INLINE_ void vc_validated_call_static(void (*method)(T *, P...), Variant *base, const Variant **p_args, Variant *r_ret) {
	call_with_validated_variant_args_static(base, method, p_args);
}

template <typename R, typename... P>
static _FORCE_INLINE_ void vc_validated_static_call(R (*method)(P...), const Variant **p_args, Variant *r_ret) {
	call_with_validated_variant_args_static_method_ret(method, p_args, r_ret);
}

template <typename... P>
static _FORCE_INLINE_ void vc_validated_static_call(void (*method)(P...), const Variant **p_args, Variant *r_ret) {
	call_with_validated_variant_args_static_method(method, p_args);
}

template <typename R, typename T, typename... P>
static _FORCE_INLINE_ void vc_ptrcall(R (T::*method)(P...), void *p_base, const void **p_args, void *r_ret) {
	call_with_ptr_args_ret(reinterpret_cast<T *>(p_base), method, p_args, r_ret);
}

template <typename R, typename T, typename... P>
static _FORCE_INLINE_ void vc_ptrcall(R (T::*method)(P...) const, void *p_base, const void **p_args, void *r_ret) {
	call_with_ptr_args_retc(reinterpret_cast<T *>(p_base), method, p_args, r_ret);
}

template <typename T, typename... P>
static _FORCE_INLINE_ void vc_ptrcall(void (T::*method)(P...), void *p_base, const void **p_args, void *r_ret) {
	call_with_ptr_args(reinterpret_cast<T *>(p_base), method, p_args);
}

template <typename T, typename... P>
static _FORCE_INLINE_ void vc_ptrcall(void (T::*method)(P...) const, void *p_base, const void **p_args, void *r_ret) {
	call_with_ptr_argsc(reinterpret_cast<T *>(p_base), method, p_args);
}

template <typename From, typename R, typename T, typename... P>
static _FORCE_INLINE_ void vc_convert_ptrcall(R (T::*method)(P...), void *p_base, const void **p_args, void *r_ret) {
	T converted(*reinterpret_cast<From *>(p_base));
	call_with_ptr_args_ret(&converted, method, p_args, r_ret);
}

template <typename From, typename R, typename T, typename... P>
static _FORCE_INLINE_ void vc_convert_ptrcall(R (T::*method)(P...) const, void *p_base, const void **p_args, void *r_ret) {
	T converted(*reinterpret_cast<From *>(p_base));
	call_with_ptr_args_retc(&converted, method, p_args, r_ret);
}

template <typename From, typename T, typename... P>
static _FORCE_INLINE_ void vc_convert_ptrcall(void (T::*method)(P...), void *p_base, const void **p_args, void *r_ret) {
	T converted(*reinterpret_cast<From *>(p_base));
	call_with_ptr_args(&converted, method, p_args);
}

template <typename From, typename T, typename... P>
static _FORCE_INLINE_ void vc_convert_ptrcall(void (T::*method)(P...) const, void *p_base, const void **p_args, void *r_ret) {
	T converted(*reinterpret_cast<From *>(p_base));
	call_with_ptr_argsc(&converted, method, p_args);
}

template <typename R, typename T, typename... P>
static _FORCE_INLINE_ int vc_get_argument_count(R (T::*method)(P...)) {
	return sizeof...(P);
}
template <typename R, typename T, typename... P>
static _FORCE_INLINE_ int vc_get_argument_count(R (T::*method)(P...) const) {
	return sizeof...(P);
}

template <typename T, typename... P>
static _FORCE_INLINE_ int vc_get_argument_count(void (T::*method)(P...)) {
	return sizeof...(P);
}

template <typename T, typename... P>
static _FORCE_INLINE_ int vc_get_argument_count(void (T::*method)(P...) const) {
	return sizeof...(P);
}

template <typename R, typename T, typename... P>
static _FORCE_INLINE_ int vc_get_argument_count(R (*method)(T *, P...)) {
	return sizeof...(P);
}

template <typename R, typename... P>
static _FORCE_INLINE_ int vc_get_argument_count_static(R (*method)(P...)) {
	return sizeof...(P);
}

template <typename R, typename T, typename... P>
static _FORCE_INLINE_ Variant::Type vc_get_argument_type(R (T::*method)(P...), int p_arg) {
	return call_get_argument_type<P...>(p_arg);
}
template <typename R, typename T, typename... P>
static _FORCE_INLINE_ Variant::Type vc_get_argument_type(R (T::*method)(P...) const, int p_arg) {
	return call_get_argument_type<P...>(p_arg);
}

template <typename T, typename... P>
static _FORCE_INLINE_ Variant::Type vc_get_argument_type(void (T::*method)(P...), int p_arg) {
	return call_get_argument_type<P...>(p_arg);
}

template <typename T, typename... P>
static _FORCE_INLINE_ Variant::Type vc_get_argument_type(void (T::*method)(P...) const, int p_arg) {
	return call_get_argument_type<P...>(p_arg);
}

template <typename R, typename T, typename... P>
static _FORCE_INLINE_ Variant::Type vc_get_argument_type(R (*method)(T *, P...), int p_arg) {
	return call_get_argument_type<P...>(p_arg);
}

template <typename R, typename... P>
static _FORCE_INLINE_ Variant::Type vc_get_argument_type_static(R (*method)(P...), int p_arg) {
	return call_get_argument_type<P...>(p_arg);
}

template <typename R, typename T, typename... P>
static _FORCE_INLINE_ Variant::Type vc_get_return_type(R (T::*method)(P...)) {
	return GetTypeInfo<R>::VARIANT_TYPE;
}

template <typename R, typename T, typename... P>
static _FORCE_INLINE_ Variant::Type vc_get_return_type(R (T::*method)(P...) const) {
	return GetTypeInfo<R>::VARIANT_TYPE;
}

template <typename T, typename... P>
static _FORCE_INLINE_ Variant::Type vc_get_return_type(void (T::*method)(P...)) {
	return Variant::NIL;
}

template <typename T, typename... P>
static _FORCE_INLINE_ Variant::Type vc_get_return_type(void (T::*method)(P...) const) {
	return Variant::NIL;
}

template <typename R, typename... P>
static _FORCE_INLINE_ Variant::Type vc_get_return_type(R (*method)(P...)) {
	return GetTypeInfo<R>::VARIANT_TYPE;
}

template <typename... P>
static _FORCE_INLINE_ Variant::Type vc_get_return_type(void (*method)(P...)) {
	return Variant::NIL;
}

template <typename R, typename T, typename... P>
static _FORCE_INLINE_ bool vc_has_return_type(R (T::*method)(P...)) {
	return true;
}
template <typename R, typename T, typename... P>
static _FORCE_INLINE_ bool vc_has_return_type(R (T::*method)(P...) const) {
	return true;
}

template <typename T, typename... P>
static _FORCE_INLINE_ bool vc_has_return_type(void (T::*method)(P...)) {
	return false;
}

template <typename T, typename... P>
static _FORCE_INLINE_ bool vc_has_return_type(void (T::*method)(P...) const) {
	return false;
}

template <typename... P>
static _FORCE_INLINE_ bool vc_has_return_type_static(void (*method)(P...)) {
	return false;
}

template <typename R, typename... P>
static _FORCE_INLINE_ bool vc_has_return_type_static(R (*method)(P...)) {
	return true;
}

template <typename R, typename T, typename... P>
static _FORCE_INLINE_ bool vc_is_const(R (T::*method)(P...)) {
	return false;
}
template <typename R, typename T, typename... P>
static _FORCE_INLINE_ bool vc_is_const(R (T::*method)(P...) const) {
	return true;
}

template <typename T, typename... P>
static _FORCE_INLINE_ bool vc_is_const(void (T::*method)(P...)) {
	return false;
}

template <typename T, typename... P>
static _FORCE_INLINE_ bool vc_is_const(void (T::*method)(P...) const) {
	return true;
}

template <typename R, typename T, typename... P>
static _FORCE_INLINE_ Variant::Type vc_get_base_type(R (T::*method)(P...)) {
	return GetTypeInfo<T>::VARIANT_TYPE;
}
template <typename R, typename T, typename... P>
static _FORCE_INLINE_ Variant::Type vc_get_base_type(R (T::*method)(P...) const) {
	return GetTypeInfo<T>::VARIANT_TYPE;
}

template <typename T, typename... P>
static _FORCE_INLINE_ Variant::Type vc_get_base_type(void (T::*method)(P...)) {
	return GetTypeInfo<T>::VARIANT_TYPE;
}

template <typename T, typename... P>
static _FORCE_INLINE_ Variant::Type vc_get_base_type(void (T::*method)(P...) const) {
	return GetTypeInfo<T>::VARIANT_TYPE;
}

#define METHOD_CLASS(m_class, m_method_name, m_method_ptr)                                                                                                        \
	struct Method_##m_class##_##m_method_name {                                                                                                                   \
		static void call(Variant *base, const Variant **p_args, int p_argcount, Variant &r_ret, const Vector<Variant> &p_defvals, Callable::CallError &r_error) { \
			vc_method_call(m_method_ptr, base, p_args, p_argcount, r_ret, p_defvals, r_error);                                                                    \
		}                                                                                                                                                         \
		static void validated_call(Variant *base, const Variant **p_args, int p_argcount, Variant *r_ret) {                                                       \
			vc_validated_call(m_method_ptr, base, p_args, r_ret);                                                                                                 \
		}                                                                                                                                                         \
		static void ptrcall(void *p_base, const void **p_args, void *r_ret, int p_argcount) {                                                                     \
			vc_ptrcall(m_method_ptr, p_base, p_args, r_ret);                                                                                                      \
		}                                                                                                                                                         \
		static int get_argument_count() {                                                                                                                         \
			return vc_get_argument_count(m_method_ptr);                                                                                                           \
		}                                                                                                                                                         \
		static Variant::Type get_argument_type(int p_arg) {                                                                                                       \
			return vc_get_argument_type(m_method_ptr, p_arg);                                                                                                     \
		}                                                                                                                                                         \
		static Variant::Type get_return_type() {                                                                                                                  \
			return vc_get_return_type(m_method_ptr);                                                                                                              \
		}                                                                                                                                                         \
		static bool has_return_type() {                                                                                                                           \
			return vc_has_return_type(m_method_ptr);                                                                                                              \
		}                                                                                                                                                         \
		static bool is_const() {                                                                                                                                  \
			return vc_is_const(m_method_ptr);                                                                                                                     \
		}                                                                                                                                                         \
		static bool is_static() {                                                                                                                                 \
			return false;                                                                                                                                         \
		}                                                                                                                                                         \
		static bool is_vararg() {                                                                                                                                 \
			return false;                                                                                                                                         \
		}                                                                                                                                                         \
		static Variant::Type get_base_type() {                                                                                                                    \
			return vc_get_base_type(m_method_ptr);                                                                                                                \
		}                                                                                                                                                         \
		static StringName get_name() {                                                                                                                            \
			return #m_method_name;                                                                                                                                \
		}                                                                                                                                                         \
	};

#define CONVERT_METHOD_CLASS(m_class, m_method_name, m_method_ptr)                                                                                                \
	struct Method_##m_class##_##m_method_name {                                                                                                                   \
		static void call(Variant *base, const Variant **p_args, int p_argcount, Variant &r_ret, const Vector<Variant> &p_defvals, Callable::CallError &r_error) { \
			vc_convert_method_call<m_class>(m_method_ptr, base, p_args, p_argcount, r_ret, p_defvals, r_error);                                                   \
		}                                                                                                                                                         \
		static void validated_call(Variant *base, const Variant **p_args, int p_argcount, Variant *r_ret) {                                                       \
			vc_convert_validated_call<m_class>(m_method_ptr, base, p_args, r_ret);                                                                                \
		}                                                                                                                                                         \
		static void ptrcall(void *p_base, const void **p_args, void *r_ret, int p_argcount) {                                                                     \
			vc_convert_ptrcall<m_class>(m_method_ptr, p_base, p_args, r_ret);                                                                                     \
		}                                                                                                                                                         \
		static int get_argument_count() {                                                                                                                         \
			return vc_get_argument_count(m_method_ptr);                                                                                                           \
		}                                                                                                                                                         \
		static Variant::Type get_argument_type(int p_arg) {                                                                                                       \
			return vc_get_argument_type(m_method_ptr, p_arg);                                                                                                     \
		}                                                                                                                                                         \
		static Variant::Type get_return_type() {                                                                                                                  \
			return vc_get_return_type(m_method_ptr);                                                                                                              \
		}                                                                                                                                                         \
		static bool has_return_type() {                                                                                                                           \
			return vc_has_return_type(m_method_ptr);                                                                                                              \
		}                                                                                                                                                         \
		static bool is_const() {                                                                                                                                  \
			return vc_is_const(m_method_ptr);                                                                                                                     \
		}                                                                                                                                                         \
		static bool is_static() {                                                                                                                                 \
			return false;                                                                                                                                         \
		}                                                                                                                                                         \
		static bool is_vararg() {                                                                                                                                 \
			return false;                                                                                                                                         \
		}                                                                                                                                                         \
		static Variant::Type get_base_type() {                                                                                                                    \
			return GetTypeInfo<m_class>::VARIANT_TYPE;                                                                                                            \
		}                                                                                                                                                         \
		static StringName get_name() {                                                                                                                            \
			return #m_method_name;                                                                                                                                \
		}                                                                                                                                                         \
	};

template <typename R, typename... P>
static _FORCE_INLINE_ void vc_static_ptrcall(R (*method)(P...), const void **p_args, void *r_ret) {
	call_with_ptr_args_static_method_ret<R, P...>(method, p_args, r_ret);
}

template <typename... P>
static _FORCE_INLINE_ void vc_static_ptrcall(void (*method)(P...), const void **p_args, void *r_ret) {
	call_with_ptr_args_static_method<P...>(method, p_args);
}

#define STATIC_METHOD_CLASS(m_class, m_method_name, m_method_ptr)                                                                                                 \
	struct Method_##m_class##_##m_method_name {                                                                                                                   \
		static void call(Variant *base, const Variant **p_args, int p_argcount, Variant &r_ret, const Vector<Variant> &p_defvals, Callable::CallError &r_error) { \
			vc_static_method_call(m_method_ptr, p_args, p_argcount, r_ret, p_defvals, r_error);                                                                   \
		}                                                                                                                                                         \
		static void validated_call(Variant *base, const Variant **p_args, int p_argcount, Variant *r_ret) {                                                       \
			vc_validated_static_call(m_method_ptr, p_args, r_ret);                                                                                                \
		}                                                                                                                                                         \
		static void ptrcall(void *p_base, const void **p_args, void *r_ret, int p_argcount) {                                                                     \
			vc_static_ptrcall(m_method_ptr, p_args, r_ret);                                                                                                       \
		}                                                                                                                                                         \
		static int get_argument_count() {                                                                                                                         \
			return vc_get_argument_count_static(m_method_ptr);                                                                                                    \
		}                                                                                                                                                         \
		static Variant::Type get_argument_type(int p_arg) {                                                                                                       \
			return vc_get_argument_type_static(m_method_ptr, p_arg);                                                                                              \
		}                                                                                                                                                         \
		static Variant::Type get_return_type() {                                                                                                                  \
			return vc_get_return_type(m_method_ptr);                                                                                                              \
		}                                                                                                                                                         \
		static bool has_return_type() {                                                                                                                           \
			return vc_has_return_type_static(m_method_ptr);                                                                                                       \
		}                                                                                                                                                         \
		static bool is_const() {                                                                                                                                  \
			return false;                                                                                                                                         \
		}                                                                                                                                                         \
		static bool is_static() {                                                                                                                                 \
			return true;                                                                                                                                          \
		}                                                                                                                                                         \
		static bool is_vararg() {                                                                                                                                 \
			return false;                                                                                                                                         \
		}                                                                                                                                                         \
		static Variant::Type get_base_type() {                                                                                                                    \
			return GetTypeInfo<m_class>::VARIANT_TYPE;                                                                                                            \
		}                                                                                                                                                         \
		static StringName get_name() {                                                                                                                            \
			return #m_method_name;                                                                                                                                \
		}                                                                                                                                                         \
	};

template <typename R, typename T, typename... P>
static _FORCE_INLINE_ void vc_ptrcall(R (*method)(T *, P...), void *p_base, const void **p_args, void *r_ret) {
	call_with_ptr_args_static_retc<T, R, P...>(reinterpret_cast<T *>(p_base), method, p_args, r_ret);
}

template <typename T, typename... P>
static _FORCE_INLINE_ void vc_ptrcall(void (*method)(T *, P...), void *p_base, const void **p_args, void *r_ret) {
	call_with_ptr_args_static<T, P...>(reinterpret_cast<T *>(p_base), method, p_args);
}

#define FUNCTION_CLASS(m_class, m_method_name, m_method_ptr, m_const)                                                                                             \
	struct Method_##m_class##_##m_method_name {                                                                                                                   \
		static void call(Variant *base, const Variant **p_args, int p_argcount, Variant &r_ret, const Vector<Variant> &p_defvals, Callable::CallError &r_error) { \
			vc_method_call_static(m_method_ptr, base, p_args, p_argcount, r_ret, p_defvals, r_error);                                                             \
		}                                                                                                                                                         \
		static void validated_call(Variant *base, const Variant **p_args, int p_argcount, Variant *r_ret) {                                                       \
			vc_validated_call_static(m_method_ptr, base, p_args, r_ret);                                                                                          \
		}                                                                                                                                                         \
		static void ptrcall(void *p_base, const void **p_args, void *r_ret, int p_argcount) {                                                                     \
			vc_ptrcall(m_method_ptr, p_base, p_args, r_ret);                                                                                                      \
		}                                                                                                                                                         \
		static int get_argument_count() {                                                                                                                         \
			return vc_get_argument_count(m_method_ptr);                                                                                                           \
		}                                                                                                                                                         \
		static Variant::Type get_argument_type(int p_arg) {                                                                                                       \
			return vc_get_argument_type(m_method_ptr, p_arg);                                                                                                     \
		}                                                                                                                                                         \
		static Variant::Type get_return_type() {                                                                                                                  \
			return vc_get_return_type(m_method_ptr);                                                                                                              \
		}                                                                                                                                                         \
		static bool has_return_type() {                                                                                                                           \
			return vc_has_return_type_static(m_method_ptr);                                                                                                       \
		}                                                                                                                                                         \
		static bool is_const() {                                                                                                                                  \
			return m_const;                                                                                                                                       \
		}                                                                                                                                                         \
		static bool is_static() {                                                                                                                                 \
			return false;                                                                                                                                         \
		}                                                                                                                                                         \
		static bool is_vararg() {                                                                                                                                 \
			return false;                                                                                                                                         \
		}                                                                                                                                                         \
		static Variant::Type get_base_type() {                                                                                                                    \
			return GetTypeInfo<m_class>::VARIANT_TYPE;                                                                                                            \
		}                                                                                                                                                         \
		static StringName get_name() {                                                                                                                            \
			return #m_method_name;                                                                                                                                \
		}                                                                                                                                                         \
	};

#define VARARG_CLASS(m_class, m_method_name, m_method_ptr, m_has_return, m_return_type)                                                                           \
	struct Method_##m_class##_##m_method_name {                                                                                                                   \
		static void call(Variant *base, const Variant **p_args, int p_argcount, Variant &r_ret, const Vector<Variant> &p_defvals, Callable::CallError &r_error) { \
			m_method_ptr(base, p_args, p_argcount, r_ret, r_error);                                                                                               \
		}                                                                                                                                                         \
		static void validated_call(Variant *base, const Variant **p_args, int p_argcount, Variant *r_ret) {                                                       \
			Callable::CallError ce;                                                                                                                               \
			m_method_ptr(base, p_args, p_argcount, *r_ret, ce);                                                                                                   \
		}                                                                                                                                                         \
		static void ptrcall(void *p_base, const void **p_args, void *r_ret, int p_argcount) {                                                                     \
			LocalVector<Variant> vars;                                                                                                                            \
			LocalVector<const Variant *> vars_ptrs;                                                                                                               \
			vars.resize(p_argcount);                                                                                                                              \
			vars_ptrs.resize(p_argcount);                                                                                                                         \
			for (int i = 0; i < p_argcount; i++) {                                                                                                                \
				vars[i] = PtrToArg<Variant>::convert(p_args[i]);                                                                                                  \
				vars_ptrs[i] = &vars[i];                                                                                                                          \
			}                                                                                                                                                     \
			Variant base = PtrToArg<m_class>::convert(p_base);                                                                                                    \
			Variant ret;                                                                                                                                          \
			Callable::CallError ce;                                                                                                                               \
			m_method_ptr(&base, vars_ptrs.ptr(), p_argcount, ret, ce);                                                                                            \
			if (m_has_return) {                                                                                                                                   \
				m_return_type r = ret;                                                                                                                            \
				PtrToArg<m_return_type>::encode(ret, r_ret);                                                                                                      \
			}                                                                                                                                                     \
		}                                                                                                                                                         \
		static int get_argument_count() {                                                                                                                         \
			return 0;                                                                                                                                             \
		}                                                                                                                                                         \
		static Variant::Type get_argument_type(int p_arg) {                                                                                                       \
			return Variant::NIL;                                                                                                                                  \
		}                                                                                                                                                         \
		static Variant::Type get_return_type() {                                                                                                                  \
			return GetTypeInfo<m_return_type>::VARIANT_TYPE;                                                                                                      \
		}                                                                                                                                                         \
		static bool has_return_type() {                                                                                                                           \
			return m_has_return;                                                                                                                                  \
		}                                                                                                                                                         \
		static bool is_const() {                                                                                                                                  \
			return true;                                                                                                                                          \
		}                                                                                                                                                         \
		static bool is_static() {                                                                                                                                 \
			return false;                                                                                                                                         \
		}                                                                                                                                                         \
		static bool is_vararg() {                                                                                                                                 \
			return true;                                                                                                                                          \
		}                                                                                                                                                         \
		static Variant::Type get_base_type() {                                                                                                                    \
			return GetTypeInfo<m_class>::VARIANT_TYPE;                                                                                                            \
		}                                                                                                                                                         \
		static StringName get_name() {                                                                                                                            \
			return #m_method_name;                                                                                                                                \
		}                                                                                                                                                         \
	};

#define VARARG_CLASS1(m_class, m_method_name, m_method_ptr, m_arg_type)                                                                                           \
	struct Method_##m_class##_##m_method_name {                                                                                                                   \
		static void call(Variant *base, const Variant **p_args, int p_argcount, Variant &r_ret, const Vector<Variant> &p_defvals, Callable::CallError &r_error) { \
			m_method_ptr(base, p_args, p_argcount, r_ret, r_error);                                                                                               \
		}                                                                                                                                                         \
		static void validated_call(Variant *base, const Variant **p_args, int p_argcount, Variant *r_ret) {                                                       \
			Callable::CallError ce;                                                                                                                               \
			m_method_ptr(base, p_args, p_argcount, *r_ret, ce);                                                                                                   \
		}                                                                                                                                                         \
		static void ptrcall(void *p_base, const void **p_args, void *r_ret, int p_argcount) {                                                                     \
			LocalVector<Variant> vars;                                                                                                                            \
			LocalVector<const Variant *> vars_ptrs;                                                                                                               \
			vars.resize(p_argcount);                                                                                                                              \
			vars_ptrs.resize(p_argcount);                                                                                                                         \
			for (int i = 0; i < p_argcount; i++) {                                                                                                                \
				vars[i] = PtrToArg<Variant>::convert(p_args[i]);                                                                                                  \
				vars_ptrs[i] = &vars[i];                                                                                                                          \
			}                                                                                                                                                     \
			Variant base = PtrToArg<m_class>::convert(p_base);                                                                                                    \
			Variant ret;                                                                                                                                          \
			Callable::CallError ce;                                                                                                                               \
			m_method_ptr(&base, vars_ptrs.ptr(), p_argcount, ret, ce);                                                                                            \
		}                                                                                                                                                         \
		static int get_argument_count() {                                                                                                                         \
			return 1;                                                                                                                                             \
		}                                                                                                                                                         \
		static Variant::Type get_argument_type(int p_arg) {                                                                                                       \
			return m_arg_type;                                                                                                                                    \
		}                                                                                                                                                         \
		static Variant::Type get_return_type() {                                                                                                                  \
			return Variant::NIL;                                                                                                                                  \
		}                                                                                                                                                         \
		static bool has_return_type() {                                                                                                                           \
			return false;                                                                                                                                         \
		}                                                                                                                                                         \
		static bool is_const() {                                                                                                                                  \
			return true;                                                                                                                                          \
		}                                                                                                                                                         \
		static bool is_static() {                                                                                                                                 \
			return false;                                                                                                                                         \
		}                                                                                                                                                         \
		static bool is_vararg() {                                                                                                                                 \
			return true;                                                                                                                                          \
		}                                                                                                                                                         \
		static Variant::Type get_base_type() {                                                                                                                    \
			return GetTypeInfo<m_class>::VARIANT_TYPE;                                                                                                            \
		}                                                                                                                                                         \
		static StringName get_name() {                                                                                                                            \
			return #m_method_name;                                                                                                                                \
		}                                                                                                                                                         \
	};

#define VARCALL_PACKED_GETTER(m_packed_type, m_return_type)                                       \
	static m_return_type func_##m_packed_type##_get(m_packed_type *p_instance, int64_t p_index) { \
		return p_instance->get(p_index);                                                          \
	}

struct _VariantCall {
	VARCALL_PACKED_GETTER(PackedByteArray, uint8_t)
	VARCALL_PACKED_GETTER(PackedColorArray, Color)
	VARCALL_PACKED_GETTER(PackedFloat32Array, float)
	VARCALL_PACKED_GETTER(PackedFloat64Array, double)
	VARCALL_PACKED_GETTER(PackedInt32Array, int32_t)
	VARCALL_PACKED_GETTER(PackedInt64Array, int64_t)
	VARCALL_PACKED_GETTER(PackedStringArray, String)
	VARCALL_PACKED_GETTER(PackedVector2Array, Vector2)
	VARCALL_PACKED_GETTER(PackedVector3Array, Vector3)
	VARCALL_PACKED_GETTER(PackedVector4Array, Vector4)

	static String func_PackedByteArray_get_string_from_ascii(PackedByteArray *p_instance) {
		String s;
		if (p_instance->size() > 0) {
			const uint8_t *r = p_instance->ptr();
			CharString cs;
			cs.resize(p_instance->size() + 1);
			memcpy(cs.ptrw(), r, p_instance->size());
			cs[(int)p_instance->size()] = 0;

			s = cs.get_data();
		}
		return s;
	}

	static String func_PackedByteArray_get_string_from_utf8(PackedByteArray *p_instance) {
		String s;
		if (p_instance->size() > 0) {
			const uint8_t *r = p_instance->ptr();
			s.parse_utf8((const char *)r, p_instance->size());
		}
		return s;
	}

	static String func_PackedByteArray_get_string_from_utf16(PackedByteArray *p_instance) {
		String s;
		if (p_instance->size() > 0) {
			const uint8_t *r = p_instance->ptr();
			s.parse_utf16((const char16_t *)r, floor((double)p_instance->size() / (double)sizeof(char16_t)));
		}
		return s;
	}

	static String func_PackedByteArray_get_string_from_utf32(PackedByteArray *p_instance) {
		String s;
		if (p_instance->size() > 0) {
			const uint8_t *r = p_instance->ptr();
			s = String((const char32_t *)r, floor((double)p_instance->size() / (double)sizeof(char32_t)));
		}
		return s;
	}

	static String func_PackedByteArray_get_string_from_wchar(PackedByteArray *p_instance) {
		String s;
		if (p_instance->size() > 0) {
			const uint8_t *r = p_instance->ptr();
#ifdef WINDOWS_ENABLED
			s.parse_utf16((const char16_t *)r, floor((double)p_instance->size() / (double)sizeof(char16_t)));
#else
			s = String((const char32_t *)r, floor((double)p_instance->size() / (double)sizeof(char32_t)));
#endif
		}
		return s;
	}

	static PackedByteArray func_PackedByteArray_compress(PackedByteArray *p_instance, int p_mode) {
		PackedByteArray compressed;

		if (p_instance->size() > 0) {
			Compression::Mode mode = (Compression::Mode)(p_mode);
			compressed.resize(Compression::get_max_compressed_buffer_size(p_instance->size(), mode));
			int result = Compression::compress(compressed.ptrw(), p_instance->ptr(), p_instance->size(), mode);

			result = result >= 0 ? result : 0;
			compressed.resize(result);
		}

		return compressed;
	}

	static PackedByteArray func_PackedByteArray_decompress(PackedByteArray *p_instance, int64_t p_buffer_size, int p_mode) {
		PackedByteArray decompressed;
		Compression::Mode mode = (Compression::Mode)(p_mode);

		int64_t buffer_size = p_buffer_size;

		if (buffer_size <= 0) {
			ERR_FAIL_V_MSG(decompressed, "Decompression buffer size must be greater than zero.");
		}
		if (p_instance->size() == 0) {
			ERR_FAIL_V_MSG(decompressed, "Compressed buffer size must be greater than zero.");
		}

		decompressed.resize(buffer_size);
		int result = Compression::decompress(decompressed.ptrw(), buffer_size, p_instance->ptr(), p_instance->size(), mode);

		result = result >= 0 ? result : 0;
		decompressed.resize(result);

		return decompressed;
	}

	static PackedByteArray func_PackedByteArray_decompress_dynamic(PackedByteArray *p_instance, int64_t p_buffer_size, int p_mode) {
		PackedByteArray decompressed;
		int64_t max_output_size = p_buffer_size;
		Compression::Mode mode = (Compression::Mode)(p_mode);

		int result = Compression::decompress_dynamic(&decompressed, max_output_size, p_instance->ptr(), p_instance->size(), mode);

		if (result == OK) {
			return decompressed;
		} else {
			decompressed.clear();
			ERR_FAIL_V_MSG(decompressed, "Decompression failed.");
		}
	}

	static String func_PackedByteArray_hex_encode(PackedByteArray *p_instance) {
		if (p_instance->size() == 0) {
			return String();
		}
		const uint8_t *r = p_instance->ptr();
		String s = String::hex_encode_buffer(&r[0], p_instance->size());
		return s;
	}

	static int64_t func_PackedByteArray_decode_u8(PackedByteArray *p_instance, int64_t p_offset) {
		uint64_t size = p_instance->size();
		ERR_FAIL_COND_V(p_offset < 0 || p_offset > int64_t(size) - 1, 0);
		const uint8_t *r = p_instance->ptr();
		return r[p_offset];
	}
	static int64_t func_PackedByteArray_decode_s8(PackedByteArray *p_instance, int64_t p_offset) {
		uint64_t size = p_instance->size();
		ERR_FAIL_COND_V(p_offset < 0 || p_offset > int64_t(size) - 1, 0);
		const uint8_t *r = p_instance->ptr();
		return *((const int8_t *)&r[p_offset]);
	}
	static int64_t func_PackedByteArray_decode_u16(PackedByteArray *p_instance, int64_t p_offset) {
		uint64_t size = p_instance->size();
		ERR_FAIL_COND_V(p_offset < 0 || p_offset > (int64_t(size) - 2), 0);
		const uint8_t *r = p_instance->ptr();
		return decode_uint16(&r[p_offset]);
	}
	static int64_t func_PackedByteArray_decode_s16(PackedByteArray *p_instance, int64_t p_offset) {
		uint64_t size = p_instance->size();
		ERR_FAIL_COND_V(p_offset < 0 || p_offset > (int64_t(size) - 2), 0);
		const uint8_t *r = p_instance->ptr();
		return (int16_t)decode_uint16(&r[p_offset]);
	}
	static int64_t func_PackedByteArray_decode_u32(PackedByteArray *p_instance, int64_t p_offset) {
		uint64_t size = p_instance->size();
		ERR_FAIL_COND_V(p_offset < 0 || p_offset > (int64_t(size) - 4), 0);
		const uint8_t *r = p_instance->ptr();
		return decode_uint32(&r[p_offset]);
	}
	static int64_t func_PackedByteArray_decode_s32(PackedByteArray *p_instance, int64_t p_offset) {
		uint64_t size = p_instance->size();
		ERR_FAIL_COND_V(p_offset < 0 || p_offset > (int64_t(size) - 4), 0);
		const uint8_t *r = p_instance->ptr();
		return (int32_t)decode_uint32(&r[p_offset]);
	}
	static int64_t func_PackedByteArray_decode_u64(PackedByteArray *p_instance, int64_t p_offset) {
		uint64_t size = p_instance->size();
		ERR_FAIL_COND_V(p_offset < 0 || p_offset > (int64_t(size) - 8), 0);
		const uint8_t *r = p_instance->ptr();
		return (int64_t)decode_uint64(&r[p_offset]);
	}
	static int64_t func_PackedByteArray_decode_s64(PackedByteArray *p_instance, int64_t p_offset) {
		uint64_t size = p_instance->size();
		ERR_FAIL_COND_V(p_offset < 0 || p_offset > (int64_t(size) - 8), 0);
		const uint8_t *r = p_instance->ptr();
		return (int64_t)decode_uint64(&r[p_offset]);
	}
	static double func_PackedByteArray_decode_half(PackedByteArray *p_instance, int64_t p_offset) {
		uint64_t size = p_instance->size();
		ERR_FAIL_COND_V(p_offset < 0 || p_offset > (int64_t(size) - 2), 0);
		const uint8_t *r = p_instance->ptr();
		return Math::half_to_float(decode_uint16(&r[p_offset]));
	}
	static double func_PackedByteArray_decode_float(PackedByteArray *p_instance, int64_t p_offset) {
		uint64_t size = p_instance->size();
		ERR_FAIL_COND_V(p_offset < 0 || p_offset > (int64_t(size) - 4), 0);
		const uint8_t *r = p_instance->ptr();
		return decode_float(&r[p_offset]);
	}

	static double func_PackedByteArray_decode_double(PackedByteArray *p_instance, int64_t p_offset) {
		uint64_t size = p_instance->size();
		ERR_FAIL_COND_V(p_offset < 0 || p_offset > (int64_t(size) - 8), 0);
		const uint8_t *r = p_instance->ptr();
		return decode_double(&r[p_offset]);
	}

	static bool func_PackedByteArray_has_encoded_var(PackedByteArray *p_instance, int64_t p_offset, bool p_allow_objects) {
		uint64_t size = p_instance->size();
		ERR_FAIL_COND_V(p_offset < 0, false);
		const uint8_t *r = p_instance->ptr();
		Variant ret;
		Error err = decode_variant(ret, r + p_offset, size - p_offset, nullptr, p_allow_objects);
		return err == OK;
	}

	static Variant func_PackedByteArray_decode_var(PackedByteArray *p_instance, int64_t p_offset, bool p_allow_objects) {
		uint64_t size = p_instance->size();
		ERR_FAIL_COND_V(p_offset < 0, Variant());
		const uint8_t *r = p_instance->ptr();
		Variant ret;
		Error err = decode_variant(ret, r + p_offset, size - p_offset, nullptr, p_allow_objects);
		if (err != OK) {
			ret = Variant();
		}
		return ret;
	}

	static int64_t func_PackedByteArray_decode_var_size(PackedByteArray *p_instance, int64_t p_offset, bool p_allow_objects) {
		uint64_t size = p_instance->size();
		ERR_FAIL_COND_V(p_offset < 0, 0);
		const uint8_t *r = p_instance->ptr();
		Variant ret;
		int r_size;
		Error err = decode_variant(ret, r + p_offset, size - p_offset, &r_size, p_allow_objects);
		if (err == OK) {
			return r_size;
		}
		return 0;
	}

	static PackedInt32Array func_PackedByteArray_decode_s32_array(PackedByteArray *p_instance) {
		uint64_t size = p_instance->size();
		PackedInt32Array dest;
		if (size == 0) {
			return dest;
		}
		ERR_FAIL_COND_V_MSG(size % sizeof(int32_t), dest, "PackedByteArray size must be a multiple of 4 (size of 32-bit integer) to convert to PackedInt32Array.");
		const uint8_t *r = p_instance->ptr();
		dest.resize(size / sizeof(int32_t));
		ERR_FAIL_COND_V(dest.is_empty(), dest); // Avoid UB in case resize failed.
		memcpy(dest.ptrw(), r, dest.size() * sizeof(int32_t));
		return dest;
	}

	static PackedInt64Array func_PackedByteArray_decode_s64_array(PackedByteArray *p_instance) {
		uint64_t size = p_instance->size();
		PackedInt64Array dest;
		if (size == 0) {
			return dest;
		}
		ERR_FAIL_COND_V_MSG(size % sizeof(int64_t), dest, "PackedByteArray size must be a multiple of 8 (size of 64-bit integer) to convert to PackedInt64Array.");
		const uint8_t *r = p_instance->ptr();
		dest.resize(size / sizeof(int64_t));
		ERR_FAIL_COND_V(dest.is_empty(), dest); // Avoid UB in case resize failed.
		memcpy(dest.ptrw(), r, dest.size() * sizeof(int64_t));
		return dest;
	}

	static PackedFloat32Array func_PackedByteArray_decode_float_array(PackedByteArray *p_instance) {
		uint64_t size = p_instance->size();
		PackedFloat32Array dest;
		if (size == 0) {
			return dest;
		}
		ERR_FAIL_COND_V_MSG(size % sizeof(float), dest, "PackedByteArray size must be a multiple of 4 (size of 32-bit float) to convert to PackedFloat32Array.");
		const uint8_t *r = p_instance->ptr();
		dest.resize(size / sizeof(float));
		ERR_FAIL_COND_V(dest.is_empty(), dest); // Avoid UB in case resize failed.
		memcpy(dest.ptrw(), r, dest.size() * sizeof(float));
		return dest;
	}

	static PackedFloat64Array func_PackedByteArray_decode_double_array(PackedByteArray *p_instance) {
		uint64_t size = p_instance->size();
		PackedFloat64Array dest;
		if (size == 0) {
			return dest;
		}
		ERR_FAIL_COND_V_MSG(size % sizeof(double), dest, "PackedByteArray size must be a multiple of 8 (size of 64-bit double) to convert to PackedFloat64Array.");
		const uint8_t *r = p_instance->ptr();
		dest.resize(size / sizeof(double));
		ERR_FAIL_COND_V(dest.is_empty(), dest); // Avoid UB in case resize failed.
		memcpy(dest.ptrw(), r, dest.size() * sizeof(double));
		return dest;
	}

	static PackedVector2Array func_PackedByteArray_decode_vector2_array(PackedByteArray *p_instance) {
		uint64_t size = p_instance->size();
		PackedVector2Array dest;
		if (size == 0) {
			return dest;
		}
		ERR_FAIL_COND_V_MSG(size % sizeof(Vector2), dest, "PackedByteArray size must be a multiple of " + itos(sizeof(Vector2)) + " (size of Vector2) to convert to PackedVector2Array.");
		const uint8_t *r = p_instance->ptr();
		dest.resize(size / sizeof(Vector2));
		ERR_FAIL_COND_V(dest.is_empty(), dest); // Avoid UB in case resize failed.
		memcpy(dest.ptrw(), r, dest.size() * sizeof(Vector2));
		return dest;
	}

	static PackedVector3Array func_PackedByteArray_decode_vector3_array(PackedByteArray *p_instance) {
		uint64_t size = p_instance->size();
		PackedVector3Array dest;
		if (size == 0) {
			return dest;
		}
		ERR_FAIL_COND_V_MSG(size % sizeof(Vector3), dest, "PackedByteArray size must be a multiple of " + itos(sizeof(Vector3)) + " (size of Vector3) to convert to PackedVector3Array.");
		const uint8_t *r = p_instance->ptr();
		dest.resize(size / sizeof(Vector3));
		ERR_FAIL_COND_V(dest.is_empty(), dest); // Avoid UB in case resize failed.
		memcpy(dest.ptrw(), r, dest.size() * sizeof(Vector3));
		return dest;
	}

	static PackedVector4Array func_PackedByteArray_decode_vector4_array(PackedByteArray *p_instance) {
		uint64_t size = p_instance->size();
		PackedVector4Array dest;
		if (size == 0) {
			return dest;
		}
		ERR_FAIL_COND_V_MSG(size % sizeof(Vector4), dest, "PackedByteArray size must be a multiple of " + itos(sizeof(Vector4)) + " (size of Vector4) to convert to PackedVector4Array.");
		const uint8_t *r = p_instance->ptr();
		dest.resize(size / sizeof(Vector4));
		ERR_FAIL_COND_V(dest.is_empty(), dest); // Avoid UB in case resize failed.
		memcpy(dest.ptrw(), r, dest.size() * sizeof(Vector4));
		return dest;
	}

	static PackedColorArray func_PackedByteArray_decode_color_array(PackedByteArray *p_instance) {
		uint64_t size = p_instance->size();
		PackedColorArray dest;
		if (size == 0) {
			return dest;
		}
		ERR_FAIL_COND_V_MSG(size % sizeof(Color), dest, "PackedByteArray size must be a multiple of " + itos(sizeof(Color)) + " (size of Color variant) to convert to PackedColorArray.");
		const uint8_t *r = p_instance->ptr();
		dest.resize(size / sizeof(Color));
		ERR_FAIL_COND_V(dest.is_empty(), dest); // Avoid UB in case resize failed.
		memcpy(dest.ptrw(), r, dest.size() * sizeof(Color));
		return dest;
	}

	static void func_PackedByteArray_encode_u8(PackedByteArray *p_instance, int64_t p_offset, int64_t p_value) {
		uint64_t size = p_instance->size();
		ERR_FAIL_COND(p_offset < 0 || p_offset > int64_t(size) - 1);
		uint8_t *w = p_instance->ptrw();
		*((uint8_t *)&w[p_offset]) = p_value;
	}
	static void func_PackedByteArray_encode_s8(PackedByteArray *p_instance, int64_t p_offset, int64_t p_value) {
		uint64_t size = p_instance->size();
		ERR_FAIL_COND(p_offset < 0 || p_offset > int64_t(size) - 1);
		uint8_t *w = p_instance->ptrw();
		*((int8_t *)&w[p_offset]) = p_value;
	}

	static void func_PackedByteArray_encode_u16(PackedByteArray *p_instance, int64_t p_offset, int64_t p_value) {
		uint64_t size = p_instance->size();
		ERR_FAIL_COND(p_offset < 0 || p_offset > int64_t(size) - 2);
		uint8_t *w = p_instance->ptrw();
		encode_uint16((uint16_t)p_value, &w[p_offset]);
	}
	static void func_PackedByteArray_encode_s16(PackedByteArray *p_instance, int64_t p_offset, int64_t p_value) {
		uint64_t size = p_instance->size();
		ERR_FAIL_COND(p_offset < 0 || p_offset > int64_t(size) - 2);
		uint8_t *w = p_instance->ptrw();
		encode_uint16((int16_t)p_value, &w[p_offset]);
	}

	static void func_PackedByteArray_encode_u32(PackedByteArray *p_instance, int64_t p_offset, int64_t p_value) {
		uint64_t size = p_instance->size();
		ERR_FAIL_COND(p_offset < 0 || p_offset > int64_t(size) - 4);
		uint8_t *w = p_instance->ptrw();
		encode_uint32((uint32_t)p_value, &w[p_offset]);
	}
	static void func_PackedByteArray_encode_s32(PackedByteArray *p_instance, int64_t p_offset, int64_t p_value) {
		uint64_t size = p_instance->size();
		ERR_FAIL_COND(p_offset < 0 || p_offset > int64_t(size) - 4);
		uint8_t *w = p_instance->ptrw();
		encode_uint32((int32_t)p_value, &w[p_offset]);
	}

	static void func_PackedByteArray_encode_u64(PackedByteArray *p_instance, int64_t p_offset, int64_t p_value) {
		uint64_t size = p_instance->size();
		ERR_FAIL_COND(p_offset < 0 || p_offset > int64_t(size) - 8);
		uint8_t *w = p_instance->ptrw();
		encode_uint64((uint64_t)p_value, &w[p_offset]);
	}
	static void func_PackedByteArray_encode_s64(PackedByteArray *p_instance, int64_t p_offset, int64_t p_value) {
		uint64_t size = p_instance->size();
		ERR_FAIL_COND(p_offset < 0 || p_offset > int64_t(size) - 8);
		uint8_t *w = p_instance->ptrw();
		encode_uint64((int64_t)p_value, &w[p_offset]);
	}

	static void func_PackedByteArray_encode_half(PackedByteArray *p_instance, int64_t p_offset, double p_value) {
		uint64_t size = p_instance->size();
		ERR_FAIL_COND(p_offset < 0 || p_offset > int64_t(size) - 2);
		uint8_t *w = p_instance->ptrw();
		encode_uint16(Math::make_half_float(p_value), &w[p_offset]);
	}
	static void func_PackedByteArray_encode_float(PackedByteArray *p_instance, int64_t p_offset, double p_value) {
		uint64_t size = p_instance->size();
		ERR_FAIL_COND(p_offset < 0 || p_offset > int64_t(size) - 4);
		uint8_t *w = p_instance->ptrw();
		encode_float(p_value, &w[p_offset]);
	}
	static void func_PackedByteArray_encode_double(PackedByteArray *p_instance, int64_t p_offset, double p_value) {
		uint64_t size = p_instance->size();
		ERR_FAIL_COND(p_offset < 0 || p_offset > int64_t(size) - 8);
		uint8_t *w = p_instance->ptrw();
		encode_double(p_value, &w[p_offset]);
	}
	static int64_t func_PackedByteArray_encode_var(PackedByteArray *p_instance, int64_t p_offset, const Variant &p_value, bool p_allow_objects) {
		uint64_t size = p_instance->size();
		ERR_FAIL_COND_V(p_offset < 0, -1);
		uint8_t *w = p_instance->ptrw();
		int len;
		Error err = encode_variant(p_value, nullptr, len, p_allow_objects);
		if (err != OK) {
			return -1;
		}
		if (uint64_t(p_offset + len) > size) {
			return -1; // did not fit
		}
		encode_variant(p_value, w + p_offset, len, p_allow_objects);

		return len;
	}

	static void func_Callable_call(Variant *v, const Variant **p_args, int p_argcount, Variant &r_ret, Callable::CallError &r_error) {
		Callable *callable = VariantGetInternalPtr<Callable>::get_ptr(v);
		callable->callp(p_args, p_argcount, r_ret, r_error);
	}

	static void func_Callable_call_deferred(Variant *v, const Variant **p_args, int p_argcount, Variant &r_ret, Callable::CallError &r_error) {
		Callable *callable = VariantGetInternalPtr<Callable>::get_ptr(v);
		callable->call_deferredp(p_args, p_argcount);
	}

	static void func_Callable_rpc(Variant *v, const Variant **p_args, int p_argcount, Variant &r_ret, Callable::CallError &r_error) {
		Callable *callable = VariantGetInternalPtr<Callable>::get_ptr(v);
		callable->rpcp(0, p_args, p_argcount, r_error);
	}

	static void func_Callable_rpc_id(Variant *v, const Variant **p_args, int p_argcount, Variant &r_ret, Callable::CallError &r_error) {
		if (p_argcount == 0) {
			r_error.error = Callable::CallError::CALL_ERROR_TOO_FEW_ARGUMENTS;
			r_error.expected = 1;
		} else if (p_args[0]->get_type() != Variant::INT) {
			r_error.error = Callable::CallError::CALL_ERROR_INVALID_ARGUMENT;
			r_error.argument = 0;
			r_error.expected = Variant::INT;
		} else {
			Callable *callable = VariantGetInternalPtr<Callable>::get_ptr(v);
			callable->rpcp(*p_args[0], &p_args[1], p_argcount - 1, r_error);
		}
	}

	static void func_Callable_bind(Variant *v, const Variant **p_args, int p_argcount, Variant &r_ret, Callable::CallError &r_error) {
		Callable *callable = VariantGetInternalPtr<Callable>::get_ptr(v);
		r_ret = callable->bindp(p_args, p_argcount);
	}

	static int func_Callable_get_argument_count(Callable *p_callable) {
		return p_callable->get_argument_count();
	}

	static void func_Signal_emit(Variant *v, const Variant **p_args, int p_argcount, Variant &r_ret, Callable::CallError &r_error) {
		Signal *signal = VariantGetInternalPtr<Signal>::get_ptr(v);
		signal->emit(p_args, p_argcount);
	}

	struct ConstantData {
		HashMap<StringName, int64_t> value;
#ifdef DEBUG_ENABLED
		List<StringName> value_ordered;
#endif
		HashMap<StringName, Variant> variant_value;
#ifdef DEBUG_ENABLED
		List<StringName> variant_value_ordered;
#endif
	};

	static ConstantData *constant_data;

	static void add_constant(int p_type, const StringName &p_constant_name, int64_t p_constant_value) {
#ifdef DEBUG_ENABLED
		ERR_FAIL_COND(constant_data[p_type].value.has(p_constant_name));
		ERR_FAIL_COND(enum_data[p_type].value.has(p_constant_name));
		ERR_FAIL_COND(enum_data[p_type].value_to_enum.has(p_constant_name));
#endif
		constant_data[p_type].value[p_constant_name] = p_constant_value;
#ifdef DEBUG_ENABLED
		constant_data[p_type].value_ordered.push_back(p_constant_name);
#endif
	}

	static void add_variant_constant(int p_type, const StringName &p_constant_name, const Variant &p_constant_value) {
		constant_data[p_type].variant_value[p_constant_name] = p_constant_value;
#ifdef DEBUG_ENABLED
		constant_data[p_type].variant_value_ordered.push_back(p_constant_name);
#endif
	}

	struct EnumData {
		HashMap<StringName, HashMap<StringName, int>> value;
		HashMap<StringName, StringName> value_to_enum;
	};

	static EnumData *enum_data;

	static void add_enum_constant(int p_type, const StringName &p_enum_type_name, const StringName &p_enumeration_name, int p_enum_value) {
#ifdef DEBUG_ENABLED
		ERR_FAIL_COND(constant_data[p_type].value.has(p_enumeration_name));
		ERR_FAIL_COND(enum_data[p_type].value.has(p_enumeration_name));
		ERR_FAIL_COND(enum_data[p_type].value_to_enum.has(p_enumeration_name));
#endif
		enum_data[p_type].value[p_enum_type_name][p_enumeration_name] = p_enum_value;
		enum_data[p_type].value_to_enum[p_enumeration_name] = p_enum_type_name;
	}
};

_VariantCall::ConstantData *_VariantCall::constant_data = nullptr;
_VariantCall::EnumData *_VariantCall::enum_data = nullptr;

struct VariantBuiltInMethodInfo {
	void (*call)(Variant *base, const Variant **p_args, int p_argcount, Variant &r_ret, const Vector<Variant> &p_defvals, Callable::CallError &r_error) = nullptr;
	Variant::ValidatedBuiltInMethod validated_call = nullptr;
	Variant::PTRBuiltInMethod ptrcall = nullptr;

	Vector<Variant> default_arguments;
	Vector<String> argument_names;

	bool is_const = false;
	bool is_static = false;
	bool has_return_type = false;
	bool is_vararg = false;
	Variant::Type return_type;
	int argument_count = 0;
	Variant::Type (*get_argument_type)(int p_arg) = nullptr;

	MethodInfo get_method_info(const StringName &p_name) const {
		MethodInfo mi;
		mi.name = p_name;

		if (has_return_type) {
			mi.return_val.type = return_type;
			if (mi.return_val.type == Variant::NIL) {
				mi.return_val.usage |= PROPERTY_USAGE_NIL_IS_VARIANT;
			}
		}

		if (is_const) {
			mi.flags |= METHOD_FLAG_CONST;
		}
		if (is_vararg) {
			mi.flags |= METHOD_FLAG_VARARG;
		}
		if (is_static) {
			mi.flags |= METHOD_FLAG_STATIC;
		}

		for (int i = 0; i < argument_count; i++) {
			PropertyInfo pi;
#ifdef DEBUG_METHODS_ENABLED
			pi.name = argument_names[i];
#else
			pi.name = "arg" + itos(i + 1);
#endif
			pi.type = (*get_argument_type)(i);
			if (pi.type == Variant::NIL) {
				pi.usage |= PROPERTY_USAGE_NIL_IS_VARIANT;
			}
			mi.arguments.push_back(pi);
		}

		mi.default_arguments = default_arguments;

		return mi;
	}
};

typedef OAHashMap<StringName, VariantBuiltInMethodInfo> BuiltinMethodMap;
static BuiltinMethodMap *builtin_method_info;
static List<StringName> *builtin_method_names;

template <typename T>
static void register_builtin_method(const Vector<String> &p_argnames, const Vector<Variant> &p_def_args) {
	StringName name = T::get_name();

	ERR_FAIL_COND(builtin_method_info[T::get_base_type()].has(name));

	VariantBuiltInMethodInfo imi;

	imi.call = T::call;
	imi.validated_call = T::validated_call;
	imi.ptrcall = T::ptrcall;

	imi.default_arguments = p_def_args;
	imi.argument_names = p_argnames;

	imi.is_const = T::is_const();
	imi.is_static = T::is_static();
	imi.is_vararg = T::is_vararg();
	imi.has_return_type = T::has_return_type();
	imi.return_type = T::get_return_type();
	imi.argument_count = T::get_argument_count();
	imi.get_argument_type = T::get_argument_type;
#ifdef DEBUG_METHODS_ENABLED
	ERR_FAIL_COND(!imi.is_vararg && imi.argument_count != imi.argument_names.size());
#endif

	builtin_method_info[T::get_base_type()].insert(name, imi);
	builtin_method_names[T::get_base_type()].push_back(name);
}

void Variant::callp(const StringName &p_method, const Variant **p_args, int p_argcount, Variant &r_ret, Callable::CallError &r_error) {
	if (type == Variant::OBJECT) {
		//call object
		Object *obj = _get_obj().obj;
		if (!obj) {
			r_error.error = Callable::CallError::CALL_ERROR_INSTANCE_IS_NULL;
			return;
		}
#ifdef DEBUG_ENABLED
		if (EngineDebugger::is_active() && !_get_obj().id.is_ref_counted() && ObjectDB::get_instance(_get_obj().id) == nullptr) {
			r_error.error = Callable::CallError::CALL_ERROR_INSTANCE_IS_NULL;
			return;
		}

#endif
		r_ret = _get_obj().obj->callp(p_method, p_args, p_argcount, r_error);

	} else {
		r_error.error = Callable::CallError::CALL_OK;

		const VariantBuiltInMethodInfo *imf = builtin_method_info[type].lookup_ptr(p_method);

		if (!imf) {
			r_error.error = Callable::CallError::CALL_ERROR_INVALID_METHOD;
			return;
		}

		imf->call(this, p_args, p_argcount, r_ret, imf->default_arguments, r_error);
	}
}

void Variant::call_const(const StringName &p_method, const Variant **p_args, int p_argcount, Variant &r_ret, Callable::CallError &r_error) {
	if (type == Variant::OBJECT) {
		//call object
		Object *obj = _get_obj().obj;
		if (!obj) {
			r_error.error = Callable::CallError::CALL_ERROR_INSTANCE_IS_NULL;
			return;
		}
#ifdef DEBUG_ENABLED
		if (EngineDebugger::is_active() && !_get_obj().id.is_ref_counted() && ObjectDB::get_instance(_get_obj().id) == nullptr) {
			r_error.error = Callable::CallError::CALL_ERROR_INSTANCE_IS_NULL;
			return;
		}

#endif
		r_ret = _get_obj().obj->call_const(p_method, p_args, p_argcount, r_error);

		//else if (type==Variant::METHOD) {
	} else {
		r_error.error = Callable::CallError::CALL_OK;

		const VariantBuiltInMethodInfo *imf = builtin_method_info[type].lookup_ptr(p_method);

		if (!imf) {
			r_error.error = Callable::CallError::CALL_ERROR_INVALID_METHOD;
			return;
		}

		if (!imf->is_const) {
			r_error.error = Callable::CallError::CALL_ERROR_METHOD_NOT_CONST;
			return;
		}

		imf->call(this, p_args, p_argcount, r_ret, imf->default_arguments, r_error);
	}
}

void Variant::call_static(Variant::Type p_type, const StringName &p_method, const Variant **p_args, int p_argcount, Variant &r_ret, Callable::CallError &r_error) {
	r_error.error = Callable::CallError::CALL_OK;

	const VariantBuiltInMethodInfo *imf = builtin_method_info[p_type].lookup_ptr(p_method);

	if (!imf) {
		r_error.error = Callable::CallError::CALL_ERROR_INVALID_METHOD;
		return;
	}

	if (!imf->is_static) {
		r_error.error = Callable::CallError::CALL_ERROR_INSTANCE_IS_NULL;
		return;
	}

	imf->call(nullptr, p_args, p_argcount, r_ret, imf->default_arguments, r_error);
}

bool Variant::has_method(const StringName &p_method) const {
	if (type == OBJECT) {
		Object *obj = get_validated_object();
		if (!obj) {
			return false;
		}

		return obj->has_method(p_method);
	}

	return builtin_method_info[type].has(p_method);
}

bool Variant::has_builtin_method(Variant::Type p_type, const StringName &p_method) {
	ERR_FAIL_INDEX_V(p_type, Variant::VARIANT_MAX, false);
	return builtin_method_info[p_type].has(p_method);
}

Variant::ValidatedBuiltInMethod Variant::get_validated_builtin_method(Variant::Type p_type, const StringName &p_method) {
	ERR_FAIL_INDEX_V(p_type, Variant::VARIANT_MAX, nullptr);
	const VariantBuiltInMethodInfo *method = builtin_method_info[p_type].lookup_ptr(p_method);
	ERR_FAIL_NULL_V(method, nullptr);
	return method->validated_call;
}

Variant::PTRBuiltInMethod Variant::get_ptr_builtin_method(Variant::Type p_type, const StringName &p_method) {
	ERR_FAIL_INDEX_V(p_type, Variant::VARIANT_MAX, nullptr);
	const VariantBuiltInMethodInfo *method = builtin_method_info[p_type].lookup_ptr(p_method);
	ERR_FAIL_NULL_V(method, nullptr);
	return method->ptrcall;
}

MethodInfo Variant::get_builtin_method_info(Variant::Type p_type, const StringName &p_method) {
	ERR_FAIL_INDEX_V(p_type, Variant::VARIANT_MAX, MethodInfo());
	const VariantBuiltInMethodInfo *method = builtin_method_info[p_type].lookup_ptr(p_method);
	ERR_FAIL_NULL_V(method, MethodInfo());
	return method->get_method_info(p_method);
}

int Variant::get_builtin_method_argument_count(Variant::Type p_type, const StringName &p_method) {
	ERR_FAIL_INDEX_V(p_type, Variant::VARIANT_MAX, 0);
	const VariantBuiltInMethodInfo *method = builtin_method_info[p_type].lookup_ptr(p_method);
	ERR_FAIL_NULL_V(method, 0);
	return method->argument_count;
}

Variant::Type Variant::get_builtin_method_argument_type(Variant::Type p_type, const StringName &p_method, int p_argument) {
	ERR_FAIL_INDEX_V(p_type, Variant::VARIANT_MAX, Variant::NIL);
	const VariantBuiltInMethodInfo *method = builtin_method_info[p_type].lookup_ptr(p_method);
	ERR_FAIL_NULL_V(method, Variant::NIL);
	ERR_FAIL_INDEX_V(p_argument, method->argument_count, Variant::NIL);
	return method->get_argument_type(p_argument);
}

String Variant::get_builtin_method_argument_name(Variant::Type p_type, const StringName &p_method, int p_argument) {
	ERR_FAIL_INDEX_V(p_type, Variant::VARIANT_MAX, String());
	const VariantBuiltInMethodInfo *method = builtin_method_info[p_type].lookup_ptr(p_method);
	ERR_FAIL_NULL_V(method, String());
#ifdef DEBUG_METHODS_ENABLED
	ERR_FAIL_INDEX_V(p_argument, method->argument_count, String());
	return method->argument_names[p_argument];
#else
	return "arg" + itos(p_argument + 1);
#endif
}

Vector<Variant> Variant::get_builtin_method_default_arguments(Variant::Type p_type, const StringName &p_method) {
	ERR_FAIL_INDEX_V(p_type, Variant::VARIANT_MAX, Vector<Variant>());
	const VariantBuiltInMethodInfo *method = builtin_method_info[p_type].lookup_ptr(p_method);
	ERR_FAIL_NULL_V(method, Vector<Variant>());
	return method->default_arguments;
}

bool Variant::has_builtin_method_return_value(Variant::Type p_type, const StringName &p_method) {
	ERR_FAIL_INDEX_V(p_type, Variant::VARIANT_MAX, false);
	const VariantBuiltInMethodInfo *method = builtin_method_info[p_type].lookup_ptr(p_method);
	ERR_FAIL_NULL_V(method, false);
	return method->has_return_type;
}

void Variant::get_builtin_method_list(Variant::Type p_type, List<StringName> *p_list) {
	ERR_FAIL_INDEX(p_type, Variant::VARIANT_MAX);
	for (const StringName &E : builtin_method_names[p_type]) {
		p_list->push_back(E);
	}
}

int Variant::get_builtin_method_count(Variant::Type p_type) {
	ERR_FAIL_INDEX_V(p_type, Variant::VARIANT_MAX, -1);
	return builtin_method_names[p_type].size();
}

Variant::Type Variant::get_builtin_method_return_type(Variant::Type p_type, const StringName &p_method) {
	ERR_FAIL_INDEX_V(p_type, Variant::VARIANT_MAX, Variant::NIL);
	const VariantBuiltInMethodInfo *method = builtin_method_info[p_type].lookup_ptr(p_method);
	ERR_FAIL_NULL_V(method, Variant::NIL);
	return method->return_type;
}

bool Variant::is_builtin_method_const(Variant::Type p_type, const StringName &p_method) {
	ERR_FAIL_INDEX_V(p_type, Variant::VARIANT_MAX, false);
	const VariantBuiltInMethodInfo *method = builtin_method_info[p_type].lookup_ptr(p_method);
	ERR_FAIL_NULL_V(method, false);
	return method->is_const;
}

bool Variant::is_builtin_method_static(Variant::Type p_type, const StringName &p_method) {
	ERR_FAIL_INDEX_V(p_type, Variant::VARIANT_MAX, false);
	const VariantBuiltInMethodInfo *method = builtin_method_info[p_type].lookup_ptr(p_method);
	ERR_FAIL_NULL_V(method, false);
	return method->is_static;
}

bool Variant::is_builtin_method_vararg(Variant::Type p_type, const StringName &p_method) {
	ERR_FAIL_INDEX_V(p_type, Variant::VARIANT_MAX, false);
	const VariantBuiltInMethodInfo *method = builtin_method_info[p_type].lookup_ptr(p_method);
	ERR_FAIL_NULL_V(method, false);
	return method->is_vararg;
}

uint32_t Variant::get_builtin_method_hash(Variant::Type p_type, const StringName &p_method) {
	ERR_FAIL_INDEX_V(p_type, Variant::VARIANT_MAX, 0);
	const VariantBuiltInMethodInfo *method = builtin_method_info[p_type].lookup_ptr(p_method);
	ERR_FAIL_NULL_V(method, 0);
	uint32_t hash = hash_murmur3_one_32(method->is_const);
	hash = hash_murmur3_one_32(method->is_static, hash);
	hash = hash_murmur3_one_32(method->is_vararg, hash);
	hash = hash_murmur3_one_32(method->has_return_type, hash);
	if (method->has_return_type) {
		hash = hash_murmur3_one_32(method->return_type, hash);
	}
	hash = hash_murmur3_one_32(method->argument_count, hash);
	for (int i = 0; i < method->argument_count; i++) {
		hash = hash_murmur3_one_32(method->get_argument_type(i), hash);
	}

	return hash_fmix32(hash);
}

void Variant::get_method_list(List<MethodInfo> *p_list) const {
	if (type == OBJECT) {
		Object *obj = get_validated_object();
		if (obj) {
			obj->get_method_list(p_list);
		}
	} else {
		for (const StringName &E : builtin_method_names[type]) {
			const VariantBuiltInMethodInfo *method = builtin_method_info[type].lookup_ptr(E);
			ERR_CONTINUE(!method);
			p_list->push_back(method->get_method_info(E));
		}
	}
}

void Variant::get_constants_for_type(Variant::Type p_type, List<StringName> *p_constants) {
	ERR_FAIL_INDEX(p_type, Variant::VARIANT_MAX);

	const _VariantCall::ConstantData &cd = _VariantCall::constant_data[p_type];

#ifdef DEBUG_ENABLED
	for (const List<StringName>::Element *E = cd.value_ordered.front(); E; E = E->next()) {
		p_constants->push_back(E->get());
#else
	for (const KeyValue<StringName, int64_t> &E : cd.value) {
		p_constants->push_back(E.key);
#endif
	}

#ifdef DEBUG_ENABLED
	for (const List<StringName>::Element *E = cd.variant_value_ordered.front(); E; E = E->next()) {
		p_constants->push_back(E->get());
#else
	for (const KeyValue<StringName, Variant> &E : cd.variant_value) {
		p_constants->push_back(E.key);
#endif
	}
}

int Variant::get_constants_count_for_type(Variant::Type p_type) {
	ERR_FAIL_INDEX_V(p_type, Variant::VARIANT_MAX, -1);
	_VariantCall::ConstantData &cd = _VariantCall::constant_data[p_type];

	return cd.value.size() + cd.variant_value.size();
}

bool Variant::has_constant(Variant::Type p_type, const StringName &p_value) {
	ERR_FAIL_INDEX_V(p_type, Variant::VARIANT_MAX, false);
	_VariantCall::ConstantData &cd = _VariantCall::constant_data[p_type];
	return cd.value.has(p_value) || cd.variant_value.has(p_value);
}

Variant Variant::get_constant_value(Variant::Type p_type, const StringName &p_value, bool *r_valid) {
	if (r_valid) {
		*r_valid = false;
	}

	ERR_FAIL_INDEX_V(p_type, Variant::VARIANT_MAX, 0);
	_VariantCall::ConstantData &cd = _VariantCall::constant_data[p_type];

	HashMap<StringName, int64_t>::Iterator E = cd.value.find(p_value);
	if (!E) {
		HashMap<StringName, Variant>::Iterator F = cd.variant_value.find(p_value);
		if (F) {
			if (r_valid) {
				*r_valid = true;
			}
			return F->value;
		} else {
			return -1;
		}
	}
	if (r_valid) {
		*r_valid = true;
	}

	return E->value;
}

void Variant::get_enums_for_type(Variant::Type p_type, List<StringName> *p_enums) {
	ERR_FAIL_INDEX(p_type, Variant::VARIANT_MAX);

	_VariantCall::EnumData &enum_data = _VariantCall::enum_data[p_type];

	for (const KeyValue<StringName, HashMap<StringName, int>> &E : enum_data.value) {
		p_enums->push_back(E.key);
	}
}

void Variant::get_enumerations_for_enum(Variant::Type p_type, const StringName &p_enum_name, List<StringName> *p_enumerations) {
	ERR_FAIL_INDEX(p_type, Variant::VARIANT_MAX);

	_VariantCall::EnumData &enum_data = _VariantCall::enum_data[p_type];

	for (const KeyValue<StringName, HashMap<StringName, int>> &E : enum_data.value) {
		for (const KeyValue<StringName, int> &V : E.value) {
			p_enumerations->push_back(V.key);
		}
	}
}

int Variant::get_enum_value(Variant::Type p_type, const StringName &p_enum_name, const StringName &p_enumeration, bool *r_valid) {
	if (r_valid) {
		*r_valid = false;
	}

	ERR_FAIL_INDEX_V(p_type, Variant::VARIANT_MAX, -1);

	_VariantCall::EnumData &enum_data = _VariantCall::enum_data[p_type];

	HashMap<StringName, HashMap<StringName, int>>::Iterator E = enum_data.value.find(p_enum_name);
	if (!E) {
		return -1;
	}

	HashMap<StringName, int>::Iterator V = E->value.find(p_enumeration);
	if (!V) {
		return -1;
	}

	if (r_valid) {
		*r_valid = true;
	}

	return V->value;
}

bool Variant::has_enum(Variant::Type p_type, const StringName &p_enum_name) {
	ERR_FAIL_INDEX_V(p_type, Variant::VARIANT_MAX, false);

	_VariantCall::EnumData &enum_data = _VariantCall::enum_data[p_type];

	return enum_data.value.has(p_enum_name);
}

StringName Variant::get_enum_for_enumeration(Variant::Type p_type, const StringName &p_enumeration) {
	ERR_FAIL_INDEX_V(p_type, Variant::VARIANT_MAX, StringName());

	_VariantCall::EnumData &enum_data = _VariantCall::enum_data[p_type];

	const StringName *enum_name = enum_data.value_to_enum.getptr(p_enumeration);
	return (enum_name == nullptr) ? StringName() : *enum_name;
}

#ifdef DEBUG_METHODS_ENABLED
#define bind_method(m_type, m_method, m_arg_names, m_default_args) \
	METHOD_CLASS(m_type, m_method, &m_type::m_method);             \
	register_builtin_method<Method_##m_type##_##m_method>(m_arg_names, m_default_args);
#else
#define bind_method(m_type, m_method, m_arg_names, m_default_args) \
	METHOD_CLASS(m_type, m_method, &m_type ::m_method);            \
	register_builtin_method<Method_##m_type##_##m_method>(sarray(), m_default_args);
#endif

#ifdef DEBUG_METHODS_ENABLED
#define bind_convert_method(m_type_from, m_type_to, m_method, m_arg_names, m_default_args) \
	CONVERT_METHOD_CLASS(m_type_from, m_method, &m_type_to::m_method);                     \
	register_builtin_method<Method_##m_type_from##_##m_method>(m_arg_names, m_default_args);
#else
#define bind_convert_method(m_type_from, m_type_to, m_method, m_arg_names, m_default_args) \
	CONVERT_METHOD_CLASS(m_type_from, m_method, &m_type_to ::m_method);                    \
	register_builtin_method<Method_##m_type_from##_##m_method>(sarray(), m_default_args);
#endif

#ifdef DEBUG_METHODS_ENABLED
#define bind_static_method(m_type, m_method, m_arg_names, m_default_args) \
	STATIC_METHOD_CLASS(m_type, m_method, m_type::m_method);              \
	register_builtin_method<Method_##m_type##_##m_method>(m_arg_names, m_default_args);
#else
#define bind_static_method(m_type, m_method, m_arg_names, m_default_args) \
	STATIC_METHOD_CLASS(m_type, m_method, m_type ::m_method);             \
	register_builtin_method<Method_##m_type##_##m_method>(sarray(), m_default_args);
#endif

#ifdef DEBUG_METHODS_ENABLED
#define bind_methodv(m_type, m_name, m_method, m_arg_names, m_default_args) \
	METHOD_CLASS(m_type, m_name, m_method);                                 \
	register_builtin_method<Method_##m_type##_##m_name>(m_arg_names, m_default_args);
#else
#define bind_methodv(m_type, m_name, m_method, m_arg_names, m_default_args) \
	METHOD_CLASS(m_type, m_name, m_method);                                 \
	register_builtin_method<Method_##m_type##_##m_name>(sarray(), m_default_args);
#endif

#ifdef DEBUG_METHODS_ENABLED
#define bind_convert_methodv(m_type_from, m_type_to, m_name, m_method, m_arg_names, m_default_args) \
	CONVERT_METHOD_CLASS(m_type_from, m_name, m_method);                                            \
	register_builtin_method<Method_##m_type_from##_##m_name>(m_arg_names, m_default_args);
#else
#define bind_convert_methodv(m_type_from, m_type_to, m_name, m_method, m_arg_names, m_default_args) \
	CONVERT_METHOD_CLASS(m_type_from, m_name, m_method);                                            \
	register_builtin_method<Method_##m_type_from##_##m_name>(sarray(), m_default_args);
#endif

#ifdef DEBUG_METHODS_ENABLED
#define bind_function(m_type, m_name, m_method, m_arg_names, m_default_args) \
	FUNCTION_CLASS(m_type, m_name, m_method, true);                          \
	register_builtin_method<Method_##m_type##_##m_name>(m_arg_names, m_default_args);
#else
#define bind_function(m_type, m_name, m_method, m_arg_names, m_default_args) \
	FUNCTION_CLASS(m_type, m_name, m_method, true);                          \
	register_builtin_method<Method_##m_type##_##m_name>(sarray(), m_default_args);
#endif

#ifdef DEBUG_METHODS_ENABLED
#define bind_functionnc(m_type, m_name, m_method, m_arg_names, m_default_args) \
	FUNCTION_CLASS(m_type, m_name, m_method, false);                           \
	register_builtin_method<Method_##m_type##_##m_name>(m_arg_names, m_default_args);
#else
#define bind_functionnc(m_type, m_name, m_method, m_arg_names, m_default_args) \
	FUNCTION_CLASS(m_type, m_name, m_method, false);                           \
	register_builtin_method<Method_##m_type##_##m_name>(sarray(), m_default_args);
#endif

#define bind_string_method(m_method, m_arg_names, m_default_args) \
	bind_method(String, m_method, m_arg_names, m_default_args);   \
	bind_convert_method(StringName, String, m_method, m_arg_names, m_default_args);

#define bind_string_methodv(m_name, m_method, m_arg_names, m_default_args) \
	bind_methodv(String, m_name, m_method, m_arg_names, m_default_args);   \
	bind_convert_methodv(StringName, String, m_name, m_method, m_arg_names, m_default_args);

#define bind_custom(m_type, m_name, m_method, m_has_return, m_ret_type) \
	VARARG_CLASS(m_type, m_name, m_method, m_has_return, m_ret_type)    \
	register_builtin_method<Method_##m_type##_##m_name>(sarray(), Vector<Variant>());

#define bind_custom1(m_type, m_name, m_method, m_arg_type, m_arg_name) \
	VARARG_CLASS1(m_type, m_name, m_method, m_arg_type)                \
	register_builtin_method<Method_##m_type##_##m_name>(sarray(m_arg_name), Vector<Variant>());

static void _register_variant_builtin_methods_string() {
	_VariantCall::constant_data = memnew_arr(_VariantCall::ConstantData, Variant::VARIANT_MAX);
	_VariantCall::enum_data = memnew_arr(_VariantCall::EnumData, Variant::VARIANT_MAX);
	builtin_method_info = memnew_arr(BuiltinMethodMap, Variant::VARIANT_MAX);
	builtin_method_names = memnew_arr(List<StringName>, Variant::VARIANT_MAX);

	/* String */

	bind_string_method(casecmp_to, sarray("to"), varray());
	bind_string_method(nocasecmp_to, sarray("to"), varray());
	bind_string_method(naturalcasecmp_to, sarray("to"), varray());
	bind_string_method(naturalnocasecmp_to, sarray("to"), varray());
	bind_string_method(filecasecmp_to, sarray("to"), varray());
	bind_string_method(filenocasecmp_to, sarray("to"), varray());
	bind_string_method(length, sarray(), varray());
	bind_string_method(substr, sarray("from", "len"), varray(-1));
	bind_string_methodv(get_slice, static_cast<String (String::*)(const String &, int) const>(&String::get_slice), sarray("delimiter", "slice"), varray());
	bind_string_method(get_slicec, sarray("delimiter", "slice"), varray());
	bind_string_methodv(get_slice_count, static_cast<int (String::*)(const String &) const>(&String::get_slice_count), sarray("delimiter"), varray());
	bind_string_methodv(find, static_cast<int (String::*)(const String &, int) const>(&String::find), sarray("what", "from"), varray(0));
	bind_string_methodv(findn, static_cast<int (String::*)(const String &, int) const>(&String::findn), sarray("what", "from"), varray(0));
	bind_string_methodv(count, static_cast<int (String::*)(const String &, int, int) const>(&String::count), sarray("what", "from", "to"), varray(0, 0));
	bind_string_methodv(countn, static_cast<int (String::*)(const String &, int, int) const>(&String::countn), sarray("what", "from", "to"), varray(0, 0));
	bind_string_methodv(rfind, static_cast<int (String::*)(const String &, int) const>(&String::rfind), sarray("what", "from"), varray(-1));
	bind_string_methodv(rfindn, static_cast<int (String::*)(const String &, int) const>(&String::rfindn), sarray("what", "from"), varray(-1));
	bind_string_method(match, sarray("expr"), varray());
	bind_string_method(matchn, sarray("expr"), varray());
	bind_string_methodv(begins_with, static_cast<bool (String::*)(const String &) const>(&String::begins_with), sarray("text"), varray());
	bind_string_methodv(ends_with, static_cast<bool (String::*)(const String &) const>(&String::ends_with), sarray("text"), varray());
	bind_string_method(is_subsequence_of, sarray("text"), varray());
	bind_string_method(is_subsequence_ofn, sarray("text"), varray());
	bind_string_method(bigrams, sarray(), varray());
	bind_string_method(similarity, sarray("text"), varray());

	bind_string_method(format, sarray("values", "placeholder"), varray("{_}"));
	bind_string_methodv(replace, static_cast<String (String::*)(const String &, const String &) const>(&String::replace), sarray("what", "forwhat"), varray());
	bind_string_methodv(replacen, static_cast<String (String::*)(const String &, const String &) const>(&String::replacen), sarray("what", "forwhat"), varray());
	bind_string_method(repeat, sarray("count"), varray());
	bind_string_method(reverse, sarray(), varray());
	bind_string_method(insert, sarray("position", "what"), varray());
	bind_string_method(erase, sarray("position", "chars"), varray(1));
	bind_string_method(capitalize, sarray(), varray());
	bind_string_method(to_camel_case, sarray(), varray());
	bind_string_method(to_pascal_case, sarray(), varray());
	bind_string_method(to_snake_case, sarray(), varray());
	bind_string_methodv(split, static_cast<Vector<String> (String::*)(const String &, bool, int) const>(&String::split), sarray("delimiter", "allow_empty", "maxsplit"), varray("", true, 0));
	bind_string_methodv(rsplit, static_cast<Vector<String> (String::*)(const String &, bool, int) const>(&String::rsplit), sarray("delimiter", "allow_empty", "maxsplit"), varray("", true, 0));
	bind_string_method(split_floats, sarray("delimiter", "allow_empty"), varray(true));
	bind_string_method(join, sarray("parts"), varray());

	bind_string_method(to_upper, sarray(), varray());
	bind_string_method(to_lower, sarray(), varray());

	bind_string_method(left, sarray("length"), varray());
	bind_string_method(right, sarray("length"), varray());

	bind_string_method(strip_edges, sarray("left", "right"), varray(true, true));
	bind_string_method(strip_escapes, sarray(), varray());
	bind_string_method(lstrip, sarray("chars"), varray());
	bind_string_method(rstrip, sarray("chars"), varray());
	bind_string_method(get_extension, sarray(), varray());
	bind_string_method(get_basename, sarray(), varray());
	bind_string_method(path_join, sarray("file"), varray());
	bind_string_method(unicode_at, sarray("at"), varray());
	bind_string_method(indent, sarray("prefix"), varray());
	bind_string_method(dedent, sarray(), varray());
	bind_method(String, hash, sarray(), varray());
	bind_string_method(md5_text, sarray(), varray());
	bind_string_method(sha1_text, sarray(), varray());
	bind_string_method(sha256_text, sarray(), varray());
	bind_string_method(md5_buffer, sarray(), varray());
	bind_string_method(sha1_buffer, sarray(), varray());
	bind_string_method(sha256_buffer, sarray(), varray());
	bind_string_method(is_empty, sarray(), varray());
	bind_string_methodv(contains, static_cast<bool (String::*)(const String &) const>(&String::contains), sarray("what"), varray());
	bind_string_methodv(containsn, static_cast<bool (String::*)(const String &) const>(&String::containsn), sarray("what"), varray());

	bind_string_method(is_absolute_path, sarray(), varray());
	bind_string_method(is_relative_path, sarray(), varray());
	bind_string_method(simplify_path, sarray(), varray());
	bind_string_method(get_base_dir, sarray(), varray());
	bind_string_method(get_file, sarray(), varray());
	bind_string_method(xml_escape, sarray("escape_quotes"), varray(false));
	bind_string_method(xml_unescape, sarray(), varray());
	bind_string_method(uri_encode, sarray(), varray());
	bind_string_method(uri_decode, sarray(), varray());
	bind_string_method(c_escape, sarray(), varray());
	bind_string_method(c_unescape, sarray(), varray());
	bind_string_method(json_escape, sarray(), varray());

	bind_string_method(validate_node_name, sarray(), varray());
	bind_string_method(validate_filename, sarray(), varray());

	bind_string_method(is_valid_ascii_identifier, sarray(), varray());
	bind_string_method(is_valid_unicode_identifier, sarray(), varray());
	bind_string_method(is_valid_identifier, sarray(), varray());
	bind_string_method(is_valid_int, sarray(), varray());
	bind_string_method(is_valid_float, sarray(), varray());
	bind_string_method(is_valid_hex_number, sarray("with_prefix"), varray(false));
	bind_string_method(is_valid_html_color, sarray(), varray());
	bind_string_method(is_valid_ip_address, sarray(), varray());
	bind_string_method(is_valid_filename, sarray(), varray());

	bind_string_method(to_int, sarray(), varray());
	bind_string_method(to_float, sarray(), varray());
	bind_string_method(hex_to_int, sarray(), varray());
	bind_string_method(bin_to_int, sarray(), varray());

	bind_string_method(lpad, sarray("min_length", "character"), varray(" "));
	bind_string_method(rpad, sarray("min_length", "character"), varray(" "));
	bind_string_method(pad_decimals, sarray("digits"), varray());
	bind_string_method(pad_zeros, sarray("digits"), varray());
	bind_string_methodv(trim_prefix, static_cast<String (String::*)(const String &) const>(&String::trim_prefix), sarray("prefix"), varray());
	bind_string_methodv(trim_suffix, static_cast<String (String::*)(const String &) const>(&String::trim_suffix), sarray("suffix"), varray());

	bind_string_method(to_ascii_buffer, sarray(), varray());
	bind_string_method(to_utf8_buffer, sarray(), varray());
	bind_string_method(to_utf16_buffer, sarray(), varray());
	bind_string_method(to_utf32_buffer, sarray(), varray());
	bind_string_method(hex_decode, sarray(), varray());
	bind_string_method(to_wchar_buffer, sarray(), varray());

	bind_static_method(String, num_scientific, sarray("number"), varray());
	bind_static_method(String, num, sarray("number", "decimals"), varray(-1));
	bind_static_method(String, num_int64, sarray("number", "base", "capitalize_hex"), varray(10, false));
	bind_static_method(String, num_uint64, sarray("number", "base", "capitalize_hex"), varray(10, false));
	bind_static_method(String, chr, sarray("char"), varray());
	bind_static_method(String, humanize_size, sarray("size"), varray());

	/* StringName */

	bind_method(StringName, hash, sarray(), varray());
}

static void _register_variant_builtin_methods_math() {
	/* Vector2 */

	bind_method(Vector2, angle, sarray(), varray());
	bind_method(Vector2, angle_to, sarray("to"), varray());
	bind_method(Vector2, angle_to_point, sarray("to"), varray());
	bind_method(Vector2, direction_to, sarray("to"), varray());
	bind_method(Vector2, distance_to, sarray("to"), varray());
	bind_method(Vector2, distance_squared_to, sarray("to"), varray());
	bind_method(Vector2, length, sarray(), varray());
	bind_method(Vector2, length_squared, sarray(), varray());
	bind_method(Vector2, limit_length, sarray("length"), varray(1.0));
	bind_method(Vector2, normalized, sarray(), varray());
	bind_method(Vector2, is_normalized, sarray(), varray());
	bind_method(Vector2, is_equal_approx, sarray("to"), varray());
	bind_method(Vector2, is_zero_approx, sarray(), varray());
	bind_method(Vector2, is_finite, sarray(), varray());
	bind_method(Vector2, posmod, sarray("mod"), varray());
	bind_method(Vector2, posmodv, sarray("modv"), varray());
	bind_method(Vector2, project, sarray("b"), varray());
	bind_method(Vector2, lerp, sarray("to", "weight"), varray());
	bind_method(Vector2, slerp, sarray("to", "weight"), varray());
	bind_method(Vector2, cubic_interpolate, sarray("b", "pre_a", "post_b", "weight"), varray());
	bind_method(Vector2, cubic_interpolate_in_time, sarray("b", "pre_a", "post_b", "weight", "b_t", "pre_a_t", "post_b_t"), varray());
	bind_method(Vector2, bezier_interpolate, sarray("control_1", "control_2", "end", "t"), varray());
	bind_method(Vector2, bezier_derivative, sarray("control_1", "control_2", "end", "t"), varray());
	bind_method(Vector2, max_axis_index, sarray(), varray());
	bind_method(Vector2, min_axis_index, sarray(), varray());
	bind_method(Vector2, move_toward, sarray("to", "delta"), varray());
	bind_method(Vector2, rotated, sarray("angle"), varray());
	bind_method(Vector2, orthogonal, sarray(), varray());
	bind_method(Vector2, floor, sarray(), varray());
	bind_method(Vector2, ceil, sarray(), varray());
	bind_method(Vector2, round, sarray(), varray());
	bind_method(Vector2, aspect, sarray(), varray());
	bind_method(Vector2, dot, sarray("with"), varray());
	bind_method(Vector2, slide, sarray("n"), varray());
	bind_method(Vector2, bounce, sarray("n"), varray());
	bind_method(Vector2, reflect, sarray("line"), varray());
	bind_method(Vector2, cross, sarray("with"), varray());
	bind_method(Vector2, abs, sarray(), varray());
	bind_method(Vector2, sign, sarray(), varray());
	bind_method(Vector2, clamp, sarray("min", "max"), varray());
	bind_method(Vector2, clampf, sarray("min", "max"), varray());
	bind_method(Vector2, snapped, sarray("step"), varray());
	bind_method(Vector2, snappedf, sarray("step"), varray());
	bind_method(Vector2, min, sarray("with"), varray());
	bind_method(Vector2, minf, sarray("with"), varray());
	bind_method(Vector2, max, sarray("with"), varray());
	bind_method(Vector2, maxf, sarray("with"), varray());

	bind_static_method(Vector2, from_angle, sarray("angle"), varray());

	/* Vector2i */

	bind_method(Vector2i, aspect, sarray(), varray());
	bind_method(Vector2i, max_axis_index, sarray(), varray());
	bind_method(Vector2i, min_axis_index, sarray(), varray());
	bind_method(Vector2i, distance_to, sarray("to"), varray());
	bind_method(Vector2i, distance_squared_to, sarray("to"), varray());
	bind_method(Vector2i, length, sarray(), varray());
	bind_method(Vector2i, length_squared, sarray(), varray());
	bind_method(Vector2i, sign, sarray(), varray());
	bind_method(Vector2i, abs, sarray(), varray());
	bind_method(Vector2i, clamp, sarray("min", "max"), varray());
	bind_method(Vector2i, clampi, sarray("min", "max"), varray());
	bind_method(Vector2i, snapped, sarray("step"), varray());
	bind_method(Vector2i, snappedi, sarray("step"), varray());
	bind_method(Vector2i, min, sarray("with"), varray());
	bind_method(Vector2i, mini, sarray("with"), varray());
	bind_method(Vector2i, max, sarray("with"), varray());
	bind_method(Vector2i, maxi, sarray("with"), varray());

	/* Rect2 */

	bind_method(Rect2, get_center, sarray(), varray());
	bind_method(Rect2, get_area, sarray(), varray());
	bind_method(Rect2, has_area, sarray(), varray());
	bind_method(Rect2, has_point, sarray("point"), varray());
	bind_method(Rect2, is_equal_approx, sarray("rect"), varray());
	bind_method(Rect2, is_finite, sarray(), varray());
	bind_method(Rect2, intersects, sarray("b", "include_borders"), varray(false));
	bind_method(Rect2, encloses, sarray("b"), varray());
	bind_method(Rect2, intersection, sarray("b"), varray());
	bind_method(Rect2, merge, sarray("b"), varray());
	bind_method(Rect2, expand, sarray("to"), varray());
	bind_method(Rect2, get_support, sarray("direction"), varray());
	bind_method(Rect2, grow, sarray("amount"), varray());
	bind_methodv(Rect2, grow_side, &Rect2::grow_side_bind, sarray("side", "amount"), varray());
	bind_method(Rect2, grow_individual, sarray("left", "top", "right", "bottom"), varray());
	bind_method(Rect2, abs, sarray(), varray());

	/* Rect2i */

	bind_method(Rect2i, get_center, sarray(), varray());
	bind_method(Rect2i, get_area, sarray(), varray());
	bind_method(Rect2i, has_area, sarray(), varray());
	bind_method(Rect2i, has_point, sarray("point"), varray());
	bind_method(Rect2i, intersects, sarray("b"), varray());
	bind_method(Rect2i, encloses, sarray("b"), varray());
	bind_method(Rect2i, intersection, sarray("b"), varray());
	bind_method(Rect2i, merge, sarray("b"), varray());
	bind_method(Rect2i, expand, sarray("to"), varray());
	bind_method(Rect2i, grow, sarray("amount"), varray());
	bind_methodv(Rect2i, grow_side, &Rect2i::grow_side_bind, sarray("side", "amount"), varray());
	bind_method(Rect2i, grow_individual, sarray("left", "top", "right", "bottom"), varray());
	bind_method(Rect2i, abs, sarray(), varray());

	/* Vector3 */

	bind_method(Vector3, min_axis_index, sarray(), varray());
	bind_method(Vector3, max_axis_index, sarray(), varray());
	bind_method(Vector3, angle_to, sarray("to"), varray());
	bind_method(Vector3, signed_angle_to, sarray("to", "axis"), varray());
	bind_method(Vector3, direction_to, sarray("to"), varray());
	bind_method(Vector3, distance_to, sarray("to"), varray());
	bind_method(Vector3, distance_squared_to, sarray("to"), varray());
	bind_method(Vector3, length, sarray(), varray());
	bind_method(Vector3, length_squared, sarray(), varray());
	bind_method(Vector3, limit_length, sarray("length"), varray(1.0));
	bind_method(Vector3, normalized, sarray(), varray());
	bind_method(Vector3, is_normalized, sarray(), varray());
	bind_method(Vector3, is_equal_approx, sarray("to"), varray());
	bind_method(Vector3, is_zero_approx, sarray(), varray());
	bind_method(Vector3, is_finite, sarray(), varray());
	bind_method(Vector3, inverse, sarray(), varray());
	bind_method(Vector3, clamp, sarray("min", "max"), varray());
	bind_method(Vector3, clampf, sarray("min", "max"), varray());
	bind_method(Vector3, snapped, sarray("step"), varray());
	bind_method(Vector3, snappedf, sarray("step"), varray());
	bind_method(Vector3, rotated, sarray("axis", "angle"), varray());
	bind_method(Vector3, lerp, sarray("to", "weight"), varray());
	bind_method(Vector3, slerp, sarray("to", "weight"), varray());
	bind_method(Vector3, cubic_interpolate, sarray("b", "pre_a", "post_b", "weight"), varray());
	bind_method(Vector3, cubic_interpolate_in_time, sarray("b", "pre_a", "post_b", "weight", "b_t", "pre_a_t", "post_b_t"), varray());
	bind_method(Vector3, bezier_interpolate, sarray("control_1", "control_2", "end", "t"), varray());
	bind_method(Vector3, bezier_derivative, sarray("control_1", "control_2", "end", "t"), varray());
	bind_method(Vector3, move_toward, sarray("to", "delta"), varray());
	bind_method(Vector3, dot, sarray("with"), varray());
	bind_method(Vector3, cross, sarray("with"), varray());
	bind_method(Vector3, outer, sarray("with"), varray());
	bind_method(Vector3, abs, sarray(), varray());
	bind_method(Vector3, floor, sarray(), varray());
	bind_method(Vector3, ceil, sarray(), varray());
	bind_method(Vector3, round, sarray(), varray());
	bind_method(Vector3, posmod, sarray("mod"), varray());
	bind_method(Vector3, posmodv, sarray("modv"), varray());
	bind_method(Vector3, project, sarray("b"), varray());
	bind_method(Vector3, slide, sarray("n"), varray());
	bind_method(Vector3, bounce, sarray("n"), varray());
	bind_method(Vector3, reflect, sarray("n"), varray());
	bind_method(Vector3, sign, sarray(), varray());
	bind_method(Vector3, octahedron_encode, sarray(), varray());
	bind_method(Vector3, min, sarray("with"), varray());
	bind_method(Vector3, minf, sarray("with"), varray());
	bind_method(Vector3, max, sarray("with"), varray());
	bind_method(Vector3, maxf, sarray("with"), varray());
	bind_static_method(Vector3, octahedron_decode, sarray("uv"), varray());

	/* Vector3i */

	bind_method(Vector3i, min_axis_index, sarray(), varray());
	bind_method(Vector3i, max_axis_index, sarray(), varray());
	bind_method(Vector3i, distance_to, sarray("to"), varray());
	bind_method(Vector3i, distance_squared_to, sarray("to"), varray());
	bind_method(Vector3i, length, sarray(), varray());
	bind_method(Vector3i, length_squared, sarray(), varray());
	bind_method(Vector3i, sign, sarray(), varray());
	bind_method(Vector3i, abs, sarray(), varray());
	bind_method(Vector3i, clamp, sarray("min", "max"), varray());
	bind_method(Vector3i, clampi, sarray("min", "max"), varray());
	bind_method(Vector3i, snapped, sarray("step"), varray());
	bind_method(Vector3i, snappedi, sarray("step"), varray());
	bind_method(Vector3i, min, sarray("with"), varray());
	bind_method(Vector3i, mini, sarray("with"), varray());
	bind_method(Vector3i, max, sarray("with"), varray());
	bind_method(Vector3i, maxi, sarray("with"), varray());

	/* Vector4 */

	bind_method(Vector4, min_axis_index, sarray(), varray());
	bind_method(Vector4, max_axis_index, sarray(), varray());
	bind_method(Vector4, length, sarray(), varray());
	bind_method(Vector4, length_squared, sarray(), varray());
	bind_method(Vector4, abs, sarray(), varray());
	bind_method(Vector4, sign, sarray(), varray());
	bind_method(Vector4, floor, sarray(), varray());
	bind_method(Vector4, ceil, sarray(), varray());
	bind_method(Vector4, round, sarray(), varray());
	bind_method(Vector4, lerp, sarray("to", "weight"), varray());
	bind_method(Vector4, cubic_interpolate, sarray("b", "pre_a", "post_b", "weight"), varray());
	bind_method(Vector4, cubic_interpolate_in_time, sarray("b", "pre_a", "post_b", "weight", "b_t", "pre_a_t", "post_b_t"), varray());
	bind_method(Vector4, posmod, sarray("mod"), varray());
	bind_method(Vector4, posmodv, sarray("modv"), varray());
	bind_method(Vector4, snapped, sarray("step"), varray());
	bind_method(Vector4, snappedf, sarray("step"), varray());
	bind_method(Vector4, clamp, sarray("min", "max"), varray());
	bind_method(Vector4, clampf, sarray("min", "max"), varray());
	bind_method(Vector4, normalized, sarray(), varray());
	bind_method(Vector4, is_normalized, sarray(), varray());
	bind_method(Vector4, direction_to, sarray("to"), varray());
	bind_method(Vector4, distance_to, sarray("to"), varray());
	bind_method(Vector4, distance_squared_to, sarray("to"), varray());
	bind_method(Vector4, dot, sarray("with"), varray());
	bind_method(Vector4, inverse, sarray(), varray());
	bind_method(Vector4, is_equal_approx, sarray("to"), varray());
	bind_method(Vector4, is_zero_approx, sarray(), varray());
	bind_method(Vector4, is_finite, sarray(), varray());
	bind_method(Vector4, min, sarray("with"), varray());
	bind_method(Vector4, minf, sarray("with"), varray());
	bind_method(Vector4, max, sarray("with"), varray());
	bind_method(Vector4, maxf, sarray("with"), varray());

	/* Vector4i */

	bind_method(Vector4i, min_axis_index, sarray(), varray());
	bind_method(Vector4i, max_axis_index, sarray(), varray());
	bind_method(Vector4i, length, sarray(), varray());
	bind_method(Vector4i, length_squared, sarray(), varray());
	bind_method(Vector4i, sign, sarray(), varray());
	bind_method(Vector4i, abs, sarray(), varray());
	bind_method(Vector4i, clamp, sarray("min", "max"), varray());
	bind_method(Vector4i, clampi, sarray("min", "max"), varray());
	bind_method(Vector4i, snapped, sarray("step"), varray());
	bind_method(Vector4i, snappedi, sarray("step"), varray());
	bind_method(Vector4i, min, sarray("with"), varray());
	bind_method(Vector4i, mini, sarray("with"), varray());
	bind_method(Vector4i, max, sarray("with"), varray());
	bind_method(Vector4i, maxi, sarray("with"), varray());
	bind_method(Vector4i, distance_to, sarray("to"), varray());
	bind_method(Vector4i, distance_squared_to, sarray("to"), varray());

	/* Plane */

	bind_method(Plane, normalized, sarray(), varray());
	bind_method(Plane, get_center, sarray(), varray());
	bind_method(Plane, is_equal_approx, sarray("to_plane"), varray());
	bind_method(Plane, is_finite, sarray(), varray());
	bind_method(Plane, is_point_over, sarray("point"), varray());
	bind_method(Plane, distance_to, sarray("point"), varray());
	bind_method(Plane, has_point, sarray("point", "tolerance"), varray(CMP_EPSILON));
	bind_method(Plane, project, sarray("point"), varray());
	bind_methodv(Plane, intersect_3, &Plane::intersect_3_bind, sarray("b", "c"), varray());
	bind_methodv(Plane, intersects_ray, &Plane::intersects_ray_bind, sarray("from", "dir"), varray());
	bind_methodv(Plane, intersects_segment, &Plane::intersects_segment_bind, sarray("from", "to"), varray());

	/* Quaternion */

	bind_method(Quaternion, length, sarray(), varray());
	bind_method(Quaternion, length_squared, sarray(), varray());
	bind_method(Quaternion, normalized, sarray(), varray());
	bind_method(Quaternion, is_normalized, sarray(), varray());
	bind_method(Quaternion, is_equal_approx, sarray("to"), varray());
	bind_method(Quaternion, is_finite, sarray(), varray());
	bind_method(Quaternion, inverse, sarray(), varray());
	bind_method(Quaternion, log, sarray(), varray());
	bind_method(Quaternion, exp, sarray(), varray());
	bind_method(Quaternion, angle_to, sarray("to"), varray());
	bind_method(Quaternion, dot, sarray("with"), varray());
	bind_method(Quaternion, slerp, sarray("to", "weight"), varray());
	bind_method(Quaternion, slerpni, sarray("to", "weight"), varray());
	bind_method(Quaternion, spherical_cubic_interpolate, sarray("b", "pre_a", "post_b", "weight"), varray());
	bind_method(Quaternion, spherical_cubic_interpolate_in_time, sarray("b", "pre_a", "post_b", "weight", "b_t", "pre_a_t", "post_b_t"), varray());
	bind_method(Quaternion, get_euler, sarray("order"), varray((int64_t)EulerOrder::YXZ));
	bind_static_method(Quaternion, from_euler, sarray("euler"), varray());
	bind_method(Quaternion, get_axis, sarray(), varray());
	bind_method(Quaternion, get_angle, sarray(), varray());

	/* Color */

	bind_method(Color, to_argb32, sarray(), varray());
	bind_method(Color, to_abgr32, sarray(), varray());
	bind_method(Color, to_rgba32, sarray(), varray());
	bind_method(Color, to_argb64, sarray(), varray());
	bind_method(Color, to_abgr64, sarray(), varray());
	bind_method(Color, to_rgba64, sarray(), varray());
	bind_method(Color, to_html, sarray("with_alpha"), varray(true));

	bind_method(Color, clamp, sarray("min", "max"), varray(Color(0, 0, 0, 0), Color(1, 1, 1, 1)));
	bind_method(Color, inverted, sarray(), varray());
	bind_method(Color, lerp, sarray("to", "weight"), varray());
	bind_method(Color, lightened, sarray("amount"), varray());
	bind_method(Color, darkened, sarray("amount"), varray());
	bind_method(Color, blend, sarray("over"), varray());
	bind_method(Color, get_luminance, sarray(), varray());
	bind_method(Color, srgb_to_linear, sarray(), varray());
	bind_method(Color, linear_to_srgb, sarray(), varray());

	bind_method(Color, is_equal_approx, sarray("to"), varray());

	bind_static_method(Color, hex, sarray("hex"), varray());
	bind_static_method(Color, hex64, sarray("hex"), varray());
	bind_static_method(Color, html, sarray("rgba"), varray());
	bind_static_method(Color, html_is_valid, sarray("color"), varray());

	bind_static_method(Color, from_string, sarray("str", "default"), varray());
	bind_static_method(Color, from_hsv, sarray("h", "s", "v", "alpha"), varray(1.0));
	bind_static_method(Color, from_ok_hsl, sarray("h", "s", "l", "alpha"), varray(1.0));
	bind_static_method(Color, from_rgbe9995, sarray("rgbe"), varray());
	bind_static_method(Color, from_rgba8, sarray("r8", "g8", "b8", "a8"), varray(255));
}

static void _register_variant_builtin_methods_misc() {
	/* RID */

	bind_method(RID, is_valid, sarray(), varray());
	bind_method(RID, get_id, sarray(), varray());

	/* NodePath */

	bind_method(NodePath, is_absolute, sarray(), varray());
	bind_method(NodePath, get_name_count, sarray(), varray());
	bind_method(NodePath, get_name, sarray("idx"), varray());
	bind_method(NodePath, get_subname_count, sarray(), varray());
	bind_method(NodePath, hash, sarray(), varray());
	bind_method(NodePath, get_subname, sarray("idx"), varray());
	bind_method(NodePath, get_concatenated_names, sarray(), varray());
	bind_method(NodePath, get_concatenated_subnames, sarray(), varray());
	bind_method(NodePath, slice, sarray("begin", "end"), varray(INT_MAX));
	bind_method(NodePath, get_as_property_path, sarray(), varray());
	bind_method(NodePath, is_empty, sarray(), varray());

	/* Callable */

	bind_static_method(Callable, create, sarray("variant", "method"), varray());
	bind_method(Callable, callv, sarray("arguments"), varray());
	bind_method(Callable, is_null, sarray(), varray());
	bind_method(Callable, is_custom, sarray(), varray());
	bind_method(Callable, is_standard, sarray(), varray());
	bind_method(Callable, is_valid, sarray(), varray());
	bind_method(Callable, get_object, sarray(), varray());
	bind_method(Callable, get_object_id, sarray(), varray());
	bind_method(Callable, get_method, sarray(), varray());
	bind_function(Callable, get_argument_count, _VariantCall::func_Callable_get_argument_count, sarray(), varray());
	bind_method(Callable, get_bound_arguments_count, sarray(), varray());
	bind_method(Callable, get_bound_arguments, sarray(), varray());
	bind_method(Callable, get_unbound_arguments_count, sarray(), varray());
	bind_method(Callable, hash, sarray(), varray());
	bind_method(Callable, bindv, sarray("arguments"), varray());
	bind_method(Callable, unbind, sarray("argcount"), varray());

	bind_custom(Callable, call, _VariantCall::func_Callable_call, true, Variant);
	bind_custom(Callable, call_deferred, _VariantCall::func_Callable_call_deferred, false, Variant);
	bind_custom(Callable, rpc, _VariantCall::func_Callable_rpc, false, Variant);
	bind_custom1(Callable, rpc_id, _VariantCall::func_Callable_rpc_id, Variant::INT, "peer_id");
	bind_custom(Callable, bind, _VariantCall::func_Callable_bind, true, Callable);

	/* Signal */

	bind_method(Signal, is_null, sarray(), varray());
	bind_method(Signal, get_object, sarray(), varray());
	bind_method(Signal, get_object_id, sarray(), varray());
	bind_method(Signal, get_name, sarray(), varray());

	bind_method(Signal, connect, sarray("callable", "flags"), varray(0));
	bind_method(Signal, disconnect, sarray("callable"), varray());
	bind_method(Signal, is_connected, sarray("callable"), varray());
	bind_method(Signal, get_connections, sarray(), varray());
	bind_method(Signal, has_connections, sarray(), varray());

	bind_custom(Signal, emit, _VariantCall::func_Signal_emit, false, Variant);

	/* Transform2D */

	bind_method(Transform2D, inverse, sarray(), varray());
	bind_method(Transform2D, affine_inverse, sarray(), varray());
	bind_method(Transform2D, get_rotation, sarray(), varray());
	bind_method(Transform2D, get_origin, sarray(), varray());
	bind_method(Transform2D, get_scale, sarray(), varray());
	bind_method(Transform2D, get_skew, sarray(), varray());
	bind_method(Transform2D, orthonormalized, sarray(), varray());
	bind_method(Transform2D, rotated, sarray("angle"), varray());
	bind_method(Transform2D, rotated_local, sarray("angle"), varray());
	bind_method(Transform2D, scaled, sarray("scale"), varray());
	bind_method(Transform2D, scaled_local, sarray("scale"), varray());
	bind_method(Transform2D, translated, sarray("offset"), varray());
	bind_method(Transform2D, translated_local, sarray("offset"), varray());
	bind_method(Transform2D, determinant, sarray(), varray());
	bind_method(Transform2D, basis_xform, sarray("v"), varray());
	bind_method(Transform2D, basis_xform_inv, sarray("v"), varray());
	bind_method(Transform2D, interpolate_with, sarray("xform", "weight"), varray());
	bind_method(Transform2D, is_conformal, sarray(), varray());
	bind_method(Transform2D, is_equal_approx, sarray("xform"), varray());
	bind_method(Transform2D, is_finite, sarray(), varray());
	// Do not bind functions like set_rotation, set_scale, set_skew, etc because this type is immutable and can't be modified.
	bind_method(Transform2D, looking_at, sarray("target"), varray(Vector2()));

	/* Basis */

	bind_method(Basis, inverse, sarray(), varray());
	bind_method(Basis, transposed, sarray(), varray());
	bind_method(Basis, orthonormalized, sarray(), varray());
	bind_method(Basis, determinant, sarray(), varray());
	bind_methodv(Basis, rotated, static_cast<Basis (Basis::*)(const Vector3 &, real_t) const>(&Basis::rotated), sarray("axis", "angle"), varray());
	bind_method(Basis, scaled, sarray("scale"), varray());
	bind_method(Basis, get_scale, sarray(), varray());
	bind_method(Basis, get_euler, sarray("order"), varray((int64_t)EulerOrder::YXZ));
	bind_method(Basis, tdotx, sarray("with"), varray());
	bind_method(Basis, tdoty, sarray("with"), varray());
	bind_method(Basis, tdotz, sarray("with"), varray());
	bind_method(Basis, slerp, sarray("to", "weight"), varray());
	bind_method(Basis, is_conformal, sarray(), varray());
	bind_method(Basis, is_equal_approx, sarray("b"), varray());
	bind_method(Basis, is_finite, sarray(), varray());
	bind_method(Basis, get_rotation_quaternion, sarray(), varray());
	bind_static_method(Basis, looking_at, sarray("target", "up", "use_model_front"), varray(Vector3(0, 1, 0), false));
	bind_static_method(Basis, from_scale, sarray("scale"), varray());
	bind_static_method(Basis, from_euler, sarray("euler", "order"), varray((int64_t)EulerOrder::YXZ));

	/* AABB */

	bind_method(AABB, abs, sarray(), varray());
	bind_method(AABB, get_center, sarray(), varray());
	bind_method(AABB, get_volume, sarray(), varray());
	bind_method(AABB, has_volume, sarray(), varray());
	bind_method(AABB, has_surface, sarray(), varray());
	bind_method(AABB, has_point, sarray("point"), varray());
	bind_method(AABB, is_equal_approx, sarray("aabb"), varray());
	bind_method(AABB, is_finite, sarray(), varray());
	bind_method(AABB, intersects, sarray("with"), varray());
	bind_method(AABB, encloses, sarray("with"), varray());
	bind_method(AABB, intersects_plane, sarray("plane"), varray());
	bind_method(AABB, intersection, sarray("with"), varray());
	bind_method(AABB, merge, sarray("with"), varray());
	bind_method(AABB, expand, sarray("to_point"), varray());
	bind_method(AABB, grow, sarray("by"), varray());
	bind_method(AABB, get_support, sarray("direction"), varray());
	bind_method(AABB, get_longest_axis, sarray(), varray());
	bind_method(AABB, get_longest_axis_index, sarray(), varray());
	bind_method(AABB, get_longest_axis_size, sarray(), varray());
	bind_method(AABB, get_shortest_axis, sarray(), varray());
	bind_method(AABB, get_shortest_axis_index, sarray(), varray());
	bind_method(AABB, get_shortest_axis_size, sarray(), varray());
	bind_method(AABB, get_endpoint, sarray("idx"), varray());
	bind_methodv(AABB, intersects_segment, &AABB::intersects_segment_bind, sarray("from", "to"), varray());
	bind_methodv(AABB, intersects_ray, &AABB::intersects_ray_bind, sarray("from", "dir"), varray());

	/* Transform3D */

	bind_method(Transform3D, inverse, sarray(), varray());
	bind_method(Transform3D, affine_inverse, sarray(), varray());
	bind_method(Transform3D, orthonormalized, sarray(), varray());
	bind_method(Transform3D, rotated, sarray("axis", "angle"), varray());
	bind_method(Transform3D, rotated_local, sarray("axis", "angle"), varray());
	bind_method(Transform3D, scaled, sarray("scale"), varray());
	bind_method(Transform3D, scaled_local, sarray("scale"), varray());
	bind_method(Transform3D, translated, sarray("offset"), varray());
	bind_method(Transform3D, translated_local, sarray("offset"), varray());
	bind_method(Transform3D, looking_at, sarray("target", "up", "use_model_front"), varray(Vector3(0, 1, 0), false));
	bind_method(Transform3D, interpolate_with, sarray("xform", "weight"), varray());
	bind_method(Transform3D, is_equal_approx, sarray("xform"), varray());
	bind_method(Transform3D, is_finite, sarray(), varray());

	/* Projection */

	bind_static_method(Projection, create_depth_correction, sarray("flip_y"), varray());
	bind_static_method(Projection, create_light_atlas_rect, sarray("rect"), varray());
	bind_static_method(Projection, create_perspective, sarray("fovy", "aspect", "z_near", "z_far", "flip_fov"), varray(false));
	bind_static_method(Projection, create_perspective_hmd, sarray("fovy", "aspect", "z_near", "z_far", "flip_fov", "eye", "intraocular_dist", "convergence_dist"), varray());
	bind_static_method(Projection, create_for_hmd, sarray("eye", "aspect", "intraocular_dist", "display_width", "display_to_lens", "oversample", "z_near", "z_far"), varray());
	bind_static_method(Projection, create_orthogonal, sarray("left", "right", "bottom", "top", "z_near", "z_far"), varray());
	bind_static_method(Projection, create_orthogonal_aspect, sarray("size", "aspect", "z_near", "z_far", "flip_fov"), varray(false));
	bind_static_method(Projection, create_frustum, sarray("left", "right", "bottom", "top", "z_near", "z_far"), varray());
	bind_static_method(Projection, create_frustum_aspect, sarray("size", "aspect", "offset", "z_near", "z_far", "flip_fov"), varray(false));
	bind_static_method(Projection, create_fit_aabb, sarray("aabb"), varray());

	bind_method(Projection, determinant, sarray(), varray());
	bind_method(Projection, perspective_znear_adjusted, sarray("new_znear"), varray());
	bind_method(Projection, get_projection_plane, sarray("plane"), varray());
	bind_method(Projection, flipped_y, sarray(), varray());
	bind_method(Projection, jitter_offseted, sarray("offset"), varray());

	bind_static_method(Projection, get_fovy, sarray("fovx", "aspect"), varray());

	bind_method(Projection, get_z_far, sarray(), varray());
	bind_method(Projection, get_z_near, sarray(), varray());
	bind_method(Projection, get_aspect, sarray(), varray());
	bind_method(Projection, get_fov, sarray(), varray());
	bind_method(Projection, is_orthogonal, sarray(), varray());

	bind_method(Projection, get_viewport_half_extents, sarray(), varray());
	bind_method(Projection, get_far_plane_half_extents, sarray(), varray());

	bind_method(Projection, inverse, sarray(), varray());
	bind_method(Projection, get_pixels_per_meter, sarray("for_pixel_width"), varray());
	bind_method(Projection, get_lod_multiplier, sarray(), varray());

	/* Dictionary */

	bind_method(Dictionary, size, sarray(), varray());
	bind_method(Dictionary, is_empty, sarray(), varray());
	bind_method(Dictionary, clear, sarray(), varray());
	bind_method(Dictionary, assign, sarray("dictionary"), varray());
	bind_method(Dictionary, sort, sarray(), varray());
	bind_method(Dictionary, merge, sarray("dictionary", "overwrite"), varray(false));
	bind_method(Dictionary, merged, sarray("dictionary", "overwrite"), varray(false));
	bind_method(Dictionary, has, sarray("key"), varray());
	bind_method(Dictionary, has_all, sarray("keys"), varray());
	bind_method(Dictionary, find_key, sarray("value"), varray());
	bind_method(Dictionary, erase, sarray("key"), varray());
	bind_method(Dictionary, hash, sarray(), varray());
	bind_method(Dictionary, keys, sarray(), varray());
	bind_method(Dictionary, values, sarray(), varray());
	bind_method(Dictionary, duplicate, sarray("deep"), varray(false));
	bind_method(Dictionary, get, sarray("key", "default"), varray(Variant()));
	bind_method(Dictionary, get_or_add, sarray("key", "default"), varray(Variant()));
	bind_method(Dictionary, set, sarray("key", "value"), varray());
	bind_method(Dictionary, is_typed, sarray(), varray());
	bind_method(Dictionary, is_typed_key, sarray(), varray());
	bind_method(Dictionary, is_typed_value, sarray(), varray());
	bind_method(Dictionary, is_same_typed, sarray("dictionary"), varray());
	bind_method(Dictionary, is_same_typed_key, sarray("dictionary"), varray());
	bind_method(Dictionary, is_same_typed_value, sarray("dictionary"), varray());
	bind_method(Dictionary, get_typed_key_builtin, sarray(), varray());
	bind_method(Dictionary, get_typed_value_builtin, sarray(), varray());
	bind_method(Dictionary, get_typed_key_class_name, sarray(), varray());
	bind_method(Dictionary, get_typed_value_class_name, sarray(), varray());
	bind_method(Dictionary, get_typed_key_script, sarray(), varray());
	bind_method(Dictionary, get_typed_value_script, sarray(), varray());
	bind_method(Dictionary, make_read_only, sarray(), varray());
	bind_method(Dictionary, is_read_only, sarray(), varray());
	bind_method(Dictionary, recursive_equal, sarray("dictionary", "recursion_count"), varray());
}

static void _register_variant_builtin_methods_array() {
	/* Array */

	bind_method(Array, size, sarray(), varray());
	bind_method(Array, is_empty, sarray(), varray());
	bind_method(Array, clear, sarray(), varray());
	bind_method(Array, hash, sarray(), varray());
	bind_method(Array, assign, sarray("array"), varray());
	bind_method(Array, get, sarray("index"), varray());
	bind_method(Array, set, sarray("index", "value"), varray());
	bind_method(Array, push_back, sarray("value"), varray());
	bind_method(Array, push_front, sarray("value"), varray());
	bind_method(Array, append, sarray("value"), varray());
	bind_method(Array, append_array, sarray("array"), varray());
	bind_method(Array, resize, sarray("size"), varray());
	bind_method(Array, insert, sarray("position", "value"), varray());
	bind_method(Array, remove_at, sarray("position"), varray());
	bind_method(Array, fill, sarray("value"), varray());
	bind_method(Array, erase, sarray("value"), varray());
	bind_method(Array, front, sarray(), varray());
	bind_method(Array, back, sarray(), varray());
	bind_method(Array, pick_random, sarray(), varray());
	bind_method(Array, find, sarray("what", "from"), varray(0));
	bind_method(Array, find_custom, sarray("method", "from"), varray(0));
	bind_method(Array, rfind, sarray("what", "from"), varray(-1));
	bind_method(Array, rfind_custom, sarray("method", "from"), varray(-1));
	bind_method(Array, count, sarray("value"), varray());
	bind_method(Array, has, sarray("value"), varray());
	bind_method(Array, pop_back, sarray(), varray());
	bind_method(Array, pop_front, sarray(), varray());
	bind_method(Array, pop_at, sarray("position"), varray());
	bind_method(Array, sort, sarray(), varray());
	bind_method(Array, sort_custom, sarray("func"), varray());
	bind_method(Array, shuffle, sarray(), varray());
	bind_method(Array, bsearch, sarray("value", "before"), varray(true));
	bind_method(Array, bsearch_custom, sarray("value", "func", "before"), varray(true));
	bind_method(Array, reverse, sarray(), varray());
	bind_method(Array, duplicate, sarray("deep"), varray(false));
	bind_method(Array, slice, sarray("begin", "end", "step", "deep"), varray(INT_MAX, 1, false));
	bind_method(Array, filter, sarray("method"), varray());
	bind_method(Array, map, sarray("method"), varray());
	bind_method(Array, reduce, sarray("method", "accum"), varray(Variant()));
	bind_method(Array, any, sarray("method"), varray());
	bind_method(Array, all, sarray("method"), varray());
	bind_method(Array, max, sarray(), varray());
	bind_method(Array, min, sarray(), varray());
	bind_method(Array, is_typed, sarray(), varray());
	bind_method(Array, is_same_typed, sarray("array"), varray());
	bind_method(Array, get_typed_builtin, sarray(), varray());
	bind_method(Array, get_typed_class_name, sarray(), varray());
	bind_method(Array, get_typed_script, sarray(), varray());
	bind_method(Array, make_read_only, sarray(), varray());
	bind_method(Array, is_read_only, sarray(), varray());

	/* Packed*Array get (see VARCALL_PACKED_GETTER macro) */
	bind_function(PackedByteArray, get, _VariantCall::func_PackedByteArray_get, sarray("index"), varray());
	bind_function(PackedColorArray, get, _VariantCall::func_PackedColorArray_get, sarray("index"), varray());
	bind_function(PackedFloat32Array, get, _VariantCall::func_PackedFloat32Array_get, sarray("index"), varray());
	bind_function(PackedFloat64Array, get, _VariantCall::func_PackedFloat64Array_get, sarray("index"), varray());
	bind_function(PackedInt32Array, get, _VariantCall::func_PackedInt32Array_get, sarray("index"), varray());
	bind_function(PackedInt64Array, get, _VariantCall::func_PackedInt64Array_get, sarray("index"), varray());
	bind_function(PackedStringArray, get, _VariantCall::func_PackedStringArray_get, sarray("index"), varray());
	bind_function(PackedVector2Array, get, _VariantCall::func_PackedVector2Array_get, sarray("index"), varray());
	bind_function(PackedVector3Array, get, _VariantCall::func_PackedVector3Array_get, sarray("index"), varray());
	bind_function(PackedVector4Array, get, _VariantCall::func_PackedVector4Array_get, sarray("index"), varray());

	/* Byte Array */
	bind_method(PackedByteArray, size, sarray(), varray());
	bind_method(PackedByteArray, is_empty, sarray(), varray());
	bind_method(PackedByteArray, set, sarray("index", "value"), varray());
	bind_method(PackedByteArray, push_back, sarray("value"), varray());
	bind_method(PackedByteArray, append, sarray("value"), varray());
	bind_method(PackedByteArray, append_array, sarray("array"), varray());
	bind_method(PackedByteArray, remove_at, sarray("index"), varray());
	bind_method(PackedByteArray, insert, sarray("at_index", "value"), varray());
	bind_method(PackedByteArray, fill, sarray("value"), varray());
	bind_methodv(PackedByteArray, resize, &PackedByteArray::resize_zeroed, sarray("new_size"), varray());
	bind_method(PackedByteArray, clear, sarray(), varray());
	bind_method(PackedByteArray, has, sarray("value"), varray());
	bind_method(PackedByteArray, reverse, sarray(), varray());
	bind_method(PackedByteArray, slice, sarray("begin", "end"), varray(INT_MAX));
	bind_method(PackedByteArray, sort, sarray(), varray());
	bind_method(PackedByteArray, bsearch, sarray("value", "before"), varray(true));
	bind_method(PackedByteArray, duplicate, sarray(), varray());
	bind_method(PackedByteArray, find, sarray("value", "from"), varray(0));
	bind_method(PackedByteArray, rfind, sarray("value", "from"), varray(-1));
	bind_method(PackedByteArray, count, sarray("value"), varray());

	bind_function(PackedByteArray, get_string_from_ascii, _VariantCall::func_PackedByteArray_get_string_from_ascii, sarray(), varray());
	bind_function(PackedByteArray, get_string_from_utf8, _VariantCall::func_PackedByteArray_get_string_from_utf8, sarray(), varray());
	bind_function(PackedByteArray, get_string_from_utf16, _VariantCall::func_PackedByteArray_get_string_from_utf16, sarray(), varray());
	bind_function(PackedByteArray, get_string_from_utf32, _VariantCall::func_PackedByteArray_get_string_from_utf32, sarray(), varray());
	bind_function(PackedByteArray, get_string_from_wchar, _VariantCall::func_PackedByteArray_get_string_from_wchar, sarray(), varray());
	bind_function(PackedByteArray, hex_encode, _VariantCall::func_PackedByteArray_hex_encode, sarray(), varray());
	bind_function(PackedByteArray, compress, _VariantCall::func_PackedByteArray_compress, sarray("compression_mode"), varray(0));
	bind_function(PackedByteArray, decompress, _VariantCall::func_PackedByteArray_decompress, sarray("buffer_size", "compression_mode"), varray(0));
	bind_function(PackedByteArray, decompress_dynamic, _VariantCall::func_PackedByteArray_decompress_dynamic, sarray("max_output_size", "compression_mode"), varray(0));

	bind_function(PackedByteArray, decode_u8, _VariantCall::func_PackedByteArray_decode_u8, sarray("byte_offset"), varray());
	bind_function(PackedByteArray, decode_s8, _VariantCall::func_PackedByteArray_decode_s8, sarray("byte_offset"), varray());
	bind_function(PackedByteArray, decode_u16, _VariantCall::func_PackedByteArray_decode_u16, sarray("byte_offset"), varray());
	bind_function(PackedByteArray, decode_s16, _VariantCall::func_PackedByteArray_decode_s16, sarray("byte_offset"), varray());
	bind_function(PackedByteArray, decode_u32, _VariantCall::func_PackedByteArray_decode_u32, sarray("byte_offset"), varray());
	bind_function(PackedByteArray, decode_s32, _VariantCall::func_PackedByteArray_decode_s32, sarray("byte_offset"), varray());
	bind_function(PackedByteArray, decode_u64, _VariantCall::func_PackedByteArray_decode_u64, sarray("byte_offset"), varray());
	bind_function(PackedByteArray, decode_s64, _VariantCall::func_PackedByteArray_decode_s64, sarray("byte_offset"), varray());
	bind_function(PackedByteArray, decode_half, _VariantCall::func_PackedByteArray_decode_half, sarray("byte_offset"), varray());
	bind_function(PackedByteArray, decode_float, _VariantCall::func_PackedByteArray_decode_float, sarray("byte_offset"), varray());
	bind_function(PackedByteArray, decode_double, _VariantCall::func_PackedByteArray_decode_double, sarray("byte_offset"), varray());
	bind_function(PackedByteArray, has_encoded_var, _VariantCall::func_PackedByteArray_has_encoded_var, sarray("byte_offset", "allow_objects"), varray(false));
	bind_function(PackedByteArray, decode_var, _VariantCall::func_PackedByteArray_decode_var, sarray("byte_offset", "allow_objects"), varray(false));
	bind_function(PackedByteArray, decode_var_size, _VariantCall::func_PackedByteArray_decode_var_size, sarray("byte_offset", "allow_objects"), varray(false));

	bind_function(PackedByteArray, to_int32_array, _VariantCall::func_PackedByteArray_decode_s32_array, sarray(), varray());
	bind_function(PackedByteArray, to_int64_array, _VariantCall::func_PackedByteArray_decode_s64_array, sarray(), varray());
	bind_function(PackedByteArray, to_float32_array, _VariantCall::func_PackedByteArray_decode_float_array, sarray(), varray());
	bind_function(PackedByteArray, to_float64_array, _VariantCall::func_PackedByteArray_decode_double_array, sarray(), varray());
	bind_function(PackedByteArray, to_vector2_array, _VariantCall::func_PackedByteArray_decode_vector2_array, sarray(), varray());
	bind_function(PackedByteArray, to_vector3_array, _VariantCall::func_PackedByteArray_decode_vector3_array, sarray(), varray());
	bind_function(PackedByteArray, to_vector4_array, _VariantCall::func_PackedByteArray_decode_vector4_array, sarray(), varray());
	bind_function(PackedByteArray, to_color_array, _VariantCall::func_PackedByteArray_decode_color_array, sarray(), varray());

	bind_functionnc(PackedByteArray, encode_u8, _VariantCall::func_PackedByteArray_encode_u8, sarray("byte_offset", "value"), varray());
	bind_functionnc(PackedByteArray, encode_s8, _VariantCall::func_PackedByteArray_encode_s8, sarray("byte_offset", "value"), varray());
	bind_functionnc(PackedByteArray, encode_u16, _VariantCall::func_PackedByteArray_encode_u16, sarray("byte_offset", "value"), varray());
	bind_functionnc(PackedByteArray, encode_s16, _VariantCall::func_PackedByteArray_encode_s16, sarray("byte_offset", "value"), varray());
	bind_functionnc(PackedByteArray, encode_u32, _VariantCall::func_PackedByteArray_encode_u32, sarray("byte_offset", "value"), varray());
	bind_functionnc(PackedByteArray, encode_s32, _VariantCall::func_PackedByteArray_encode_s32, sarray("byte_offset", "value"), varray());
	bind_functionnc(PackedByteArray, encode_u64, _VariantCall::func_PackedByteArray_encode_u64, sarray("byte_offset", "value"), varray());
	bind_functionnc(PackedByteArray, encode_s64, _VariantCall::func_PackedByteArray_encode_s64, sarray("byte_offset", "value"), varray());
	bind_functionnc(PackedByteArray, encode_half, _VariantCall::func_PackedByteArray_encode_half, sarray("byte_offset", "value"), varray());
	bind_functionnc(PackedByteArray, encode_float, _VariantCall::func_PackedByteArray_encode_float, sarray("byte_offset", "value"), varray());
	bind_functionnc(PackedByteArray, encode_double, _VariantCall::func_PackedByteArray_encode_double, sarray("byte_offset", "value"), varray());
	bind_functionnc(PackedByteArray, encode_var, _VariantCall::func_PackedByteArray_encode_var, sarray("byte_offset", "value", "allow_objects"), varray(false));

	/* Int32 Array */

	bind_method(PackedInt32Array, size, sarray(), varray());
	bind_method(PackedInt32Array, is_empty, sarray(), varray());
	bind_method(PackedInt32Array, set, sarray("index", "value"), varray());
	bind_method(PackedInt32Array, push_back, sarray("value"), varray());
	bind_method(PackedInt32Array, append, sarray("value"), varray());
	bind_method(PackedInt32Array, append_array, sarray("array"), varray());
	bind_method(PackedInt32Array, remove_at, sarray("index"), varray());
	bind_method(PackedInt32Array, insert, sarray("at_index", "value"), varray());
	bind_method(PackedInt32Array, fill, sarray("value"), varray());
	bind_methodv(PackedInt32Array, resize, &PackedInt32Array::resize_zeroed, sarray("new_size"), varray());
	bind_method(PackedInt32Array, clear, sarray(), varray());
	bind_method(PackedInt32Array, has, sarray("value"), varray());
	bind_method(PackedInt32Array, reverse, sarray(), varray());
	bind_method(PackedInt32Array, slice, sarray("begin", "end"), varray(INT_MAX));
	bind_method(PackedInt32Array, to_byte_array, sarray(), varray());
	bind_method(PackedInt32Array, sort, sarray(), varray());
	bind_method(PackedInt32Array, bsearch, sarray("value", "before"), varray(true));
	bind_method(PackedInt32Array, duplicate, sarray(), varray());
	bind_method(PackedInt32Array, find, sarray("value", "from"), varray(0));
	bind_method(PackedInt32Array, rfind, sarray("value", "from"), varray(-1));
	bind_method(PackedInt32Array, count, sarray("value"), varray());

	/* Int64 Array */

	bind_method(PackedInt64Array, size, sarray(), varray());
	bind_method(PackedInt64Array, is_empty, sarray(), varray());
	bind_method(PackedInt64Array, set, sarray("index", "value"), varray());
	bind_method(PackedInt64Array, push_back, sarray("value"), varray());
	bind_method(PackedInt64Array, append, sarray("value"), varray());
	bind_method(PackedInt64Array, append_array, sarray("array"), varray());
	bind_method(PackedInt64Array, remove_at, sarray("index"), varray());
	bind_method(PackedInt64Array, insert, sarray("at_index", "value"), varray());
	bind_method(PackedInt64Array, fill, sarray("value"), varray());
	bind_methodv(PackedInt64Array, resize, &PackedInt64Array::resize_zeroed, sarray("new_size"), varray());
	bind_method(PackedInt64Array, clear, sarray(), varray());
	bind_method(PackedInt64Array, has, sarray("value"), varray());
	bind_method(PackedInt64Array, reverse, sarray(), varray());
	bind_method(PackedInt64Array, slice, sarray("begin", "end"), varray(INT_MAX));
	bind_method(PackedInt64Array, to_byte_array, sarray(), varray());
	bind_method(PackedInt64Array, sort, sarray(), varray());
	bind_method(PackedInt64Array, bsearch, sarray("value", "before"), varray(true));
	bind_method(PackedInt64Array, duplicate, sarray(), varray());
	bind_method(PackedInt64Array, find, sarray("value", "from"), varray(0));
	bind_method(PackedInt64Array, rfind, sarray("value", "from"), varray(-1));
	bind_method(PackedInt64Array, count, sarray("value"), varray());

	/* Float32 Array */

	bind_method(PackedFloat32Array, size, sarray(), varray());
	bind_method(PackedFloat32Array, is_empty, sarray(), varray());
	bind_method(PackedFloat32Array, set, sarray("index", "value"), varray());
	bind_method(PackedFloat32Array, push_back, sarray("value"), varray());
	bind_method(PackedFloat32Array, append, sarray("value"), varray());
	bind_method(PackedFloat32Array, append_array, sarray("array"), varray());
	bind_method(PackedFloat32Array, remove_at, sarray("index"), varray());
	bind_method(PackedFloat32Array, insert, sarray("at_index", "value"), varray());
	bind_method(PackedFloat32Array, fill, sarray("value"), varray());
	bind_methodv(PackedFloat32Array, resize, &PackedFloat32Array::resize_zeroed, sarray("new_size"), varray());
	bind_method(PackedFloat32Array, clear, sarray(), varray());
	bind_method(PackedFloat32Array, has, sarray("value"), varray());
	bind_method(PackedFloat32Array, reverse, sarray(), varray());
	bind_method(PackedFloat32Array, slice, sarray("begin", "end"), varray(INT_MAX));
	bind_method(PackedFloat32Array, to_byte_array, sarray(), varray());
	bind_method(PackedFloat32Array, sort, sarray(), varray());
	bind_method(PackedFloat32Array, bsearch, sarray("value", "before"), varray(true));
	bind_method(PackedFloat32Array, duplicate, sarray(), varray());
	bind_method(PackedFloat32Array, find, sarray("value", "from"), varray(0));
	bind_method(PackedFloat32Array, rfind, sarray("value", "from"), varray(-1));
	bind_method(PackedFloat32Array, count, sarray("value"), varray());

	/* Float64 Array */

	bind_method(PackedFloat64Array, size, sarray(), varray());
	bind_method(PackedFloat64Array, is_empty, sarray(), varray());
	bind_method(PackedFloat64Array, set, sarray("index", "value"), varray());
	bind_method(PackedFloat64Array, push_back, sarray("value"), varray());
	bind_method(PackedFloat64Array, append, sarray("value"), varray());
	bind_method(PackedFloat64Array, append_array, sarray("array"), varray());
	bind_method(PackedFloat64Array, remove_at, sarray("index"), varray());
	bind_method(PackedFloat64Array, insert, sarray("at_index", "value"), varray());
	bind_method(PackedFloat64Array, fill, sarray("value"), varray());
	bind_methodv(PackedFloat64Array, resize, &PackedFloat64Array::resize_zeroed, sarray("new_size"), varray());
	bind_method(PackedFloat64Array, clear, sarray(), varray());
	bind_method(PackedFloat64Array, has, sarray("value"), varray());
	bind_method(PackedFloat64Array, reverse, sarray(), varray());
	bind_method(PackedFloat64Array, slice, sarray("begin", "end"), varray(INT_MAX));
	bind_method(PackedFloat64Array, to_byte_array, sarray(), varray());
	bind_method(PackedFloat64Array, sort, sarray(), varray());
	bind_method(PackedFloat64Array, bsearch, sarray("value", "before"), varray(true));
	bind_method(PackedFloat64Array, duplicate, sarray(), varray());
	bind_method(PackedFloat64Array, find, sarray("value", "from"), varray(0));
	bind_method(PackedFloat64Array, rfind, sarray("value", "from"), varray(-1));
	bind_method(PackedFloat64Array, count, sarray("value"), varray());

	/* String Array */

	bind_method(PackedStringArray, size, sarray(), varray());
	bind_method(PackedStringArray, is_empty, sarray(), varray());
	bind_method(PackedStringArray, set, sarray("index", "value"), varray());
	bind_method(PackedStringArray, push_back, sarray("value"), varray());
	bind_method(PackedStringArray, append, sarray("value"), varray());
	bind_method(PackedStringArray, append_array, sarray("array"), varray());
	bind_method(PackedStringArray, remove_at, sarray("index"), varray());
	bind_method(PackedStringArray, insert, sarray("at_index", "value"), varray());
	bind_method(PackedStringArray, fill, sarray("value"), varray());
	bind_methodv(PackedStringArray, resize, &PackedStringArray::resize_zeroed, sarray("new_size"), varray());
	bind_method(PackedStringArray, clear, sarray(), varray());
	bind_method(PackedStringArray, has, sarray("value"), varray());
	bind_method(PackedStringArray, reverse, sarray(), varray());
	bind_method(PackedStringArray, slice, sarray("begin", "end"), varray(INT_MAX));
	bind_method(PackedStringArray, to_byte_array, sarray(), varray());
	bind_method(PackedStringArray, sort, sarray(), varray());
	bind_method(PackedStringArray, bsearch, sarray("value", "before"), varray(true));
	bind_method(PackedStringArray, duplicate, sarray(), varray());
	bind_method(PackedStringArray, find, sarray("value", "from"), varray(0));
	bind_method(PackedStringArray, rfind, sarray("value", "from"), varray(-1));
	bind_method(PackedStringArray, count, sarray("value"), varray());

	/* Vector2 Array */

	bind_method(PackedVector2Array, size, sarray(), varray());
	bind_method(PackedVector2Array, is_empty, sarray(), varray());
	bind_method(PackedVector2Array, set, sarray("index", "value"), varray());
	bind_method(PackedVector2Array, push_back, sarray("value"), varray());
	bind_method(PackedVector2Array, append, sarray("value"), varray());
	bind_method(PackedVector2Array, append_array, sarray("array"), varray());
	bind_method(PackedVector2Array, remove_at, sarray("index"), varray());
	bind_method(PackedVector2Array, insert, sarray("at_index", "value"), varray());
	bind_method(PackedVector2Array, fill, sarray("value"), varray());
	bind_methodv(PackedVector2Array, resize, &PackedVector2Array::resize_zeroed, sarray("new_size"), varray());
	bind_method(PackedVector2Array, clear, sarray(), varray());
	bind_method(PackedVector2Array, has, sarray("value"), varray());
	bind_method(PackedVector2Array, reverse, sarray(), varray());
	bind_method(PackedVector2Array, slice, sarray("begin", "end"), varray(INT_MAX));
	bind_method(PackedVector2Array, to_byte_array, sarray(), varray());
	bind_method(PackedVector2Array, sort, sarray(), varray());
	bind_method(PackedVector2Array, bsearch, sarray("value", "before"), varray(true));
	bind_method(PackedVector2Array, duplicate, sarray(), varray());
	bind_method(PackedVector2Array, find, sarray("value", "from"), varray(0));
	bind_method(PackedVector2Array, rfind, sarray("value", "from"), varray(-1));
	bind_method(PackedVector2Array, count, sarray("value"), varray());

	/* Vector3 Array */

	bind_method(PackedVector3Array, size, sarray(), varray());
	bind_method(PackedVector3Array, is_empty, sarray(), varray());
	bind_method(PackedVector3Array, set, sarray("index", "value"), varray());
	bind_method(PackedVector3Array, push_back, sarray("value"), varray());
	bind_method(PackedVector3Array, append, sarray("value"), varray());
	bind_method(PackedVector3Array, append_array, sarray("array"), varray());
	bind_method(PackedVector3Array, remove_at, sarray("index"), varray());
	bind_method(PackedVector3Array, insert, sarray("at_index", "value"), varray());
	bind_method(PackedVector3Array, fill, sarray("value"), varray());
	bind_methodv(PackedVector3Array, resize, &PackedVector3Array::resize_zeroed, sarray("new_size"), varray());
	bind_method(PackedVector3Array, clear, sarray(), varray());
	bind_method(PackedVector3Array, has, sarray("value"), varray());
	bind_method(PackedVector3Array, reverse, sarray(), varray());
	bind_method(PackedVector3Array, slice, sarray("begin", "end"), varray(INT_MAX));
	bind_method(PackedVector3Array, to_byte_array, sarray(), varray());
	bind_method(PackedVector3Array, sort, sarray(), varray());
	bind_method(PackedVector3Array, bsearch, sarray("value", "before"), varray(true));
	bind_method(PackedVector3Array, duplicate, sarray(), varray());
	bind_method(PackedVector3Array, find, sarray("value", "from"), varray(0));
	bind_method(PackedVector3Array, rfind, sarray("value", "from"), varray(-1));
	bind_method(PackedVector3Array, count, sarray("value"), varray());

	/* Color Array */

	bind_method(PackedColorArray, size, sarray(), varray());
	bind_method(PackedColorArray, is_empty, sarray(), varray());
	bind_method(PackedColorArray, set, sarray("index", "value"), varray());
	bind_method(PackedColorArray, push_back, sarray("value"), varray());
	bind_method(PackedColorArray, append, sarray("value"), varray());
	bind_method(PackedColorArray, append_array, sarray("array"), varray());
	bind_method(PackedColorArray, remove_at, sarray("index"), varray());
	bind_method(PackedColorArray, insert, sarray("at_index", "value"), varray());
	bind_method(PackedColorArray, fill, sarray("value"), varray());
	bind_methodv(PackedColorArray, resize, &PackedColorArray::resize_zeroed, sarray("new_size"), varray());
	bind_method(PackedColorArray, clear, sarray(), varray());
	bind_method(PackedColorArray, has, sarray("value"), varray());
	bind_method(PackedColorArray, reverse, sarray(), varray());
	bind_method(PackedColorArray, slice, sarray("begin", "end"), varray(INT_MAX));
	bind_method(PackedColorArray, to_byte_array, sarray(), varray());
	bind_method(PackedColorArray, sort, sarray(), varray());
	bind_method(PackedColorArray, bsearch, sarray("value", "before"), varray(true));
	bind_method(PackedColorArray, duplicate, sarray(), varray());
	bind_method(PackedColorArray, find, sarray("value", "from"), varray(0));
	bind_method(PackedColorArray, rfind, sarray("value", "from"), varray(-1));
	bind_method(PackedColorArray, count, sarray("value"), varray());

	/* Vector4 Array */

	bind_method(PackedVector4Array, size, sarray(), varray());
	bind_method(PackedVector4Array, is_empty, sarray(), varray());
	bind_method(PackedVector4Array, set, sarray("index", "value"), varray());
	bind_method(PackedVector4Array, push_back, sarray("value"), varray());
	bind_method(PackedVector4Array, append, sarray("value"), varray());
	bind_method(PackedVector4Array, append_array, sarray("array"), varray());
	bind_method(PackedVector4Array, remove_at, sarray("index"), varray());
	bind_method(PackedVector4Array, insert, sarray("at_index", "value"), varray());
	bind_method(PackedVector4Array, fill, sarray("value"), varray());
	bind_methodv(PackedVector4Array, resize, &PackedVector4Array::resize_zeroed, sarray("new_size"), varray());
	bind_method(PackedVector4Array, clear, sarray(), varray());
	bind_method(PackedVector4Array, has, sarray("value"), varray());
	bind_method(PackedVector4Array, reverse, sarray(), varray());
	bind_method(PackedVector4Array, slice, sarray("begin", "end"), varray(INT_MAX));
	bind_method(PackedVector4Array, to_byte_array, sarray(), varray());
	bind_method(PackedVector4Array, sort, sarray(), varray());
	bind_method(PackedVector4Array, bsearch, sarray("value", "before"), varray(true));
	bind_method(PackedVector4Array, duplicate, sarray(), varray());
	bind_method(PackedVector4Array, find, sarray("value", "from"), varray(0));
	bind_method(PackedVector4Array, rfind, sarray("value", "from"), varray(-1));
	bind_method(PackedVector4Array, count, sarray("value"), varray());
}

static void _register_variant_builtin_constants() {
	/* Register constants */

	int ncc = Color::get_named_color_count();
	for (int i = 0; i < ncc; i++) {
		_VariantCall::add_variant_constant(Variant::COLOR, Color::get_named_color_name(i), Color::get_named_color(i));
	}

	_VariantCall::add_enum_constant(Variant::VECTOR3, "Axis", "AXIS_X", Vector3::AXIS_X);
	_VariantCall::add_enum_constant(Variant::VECTOR3, "Axis", "AXIS_Y", Vector3::AXIS_Y);
	_VariantCall::add_enum_constant(Variant::VECTOR3, "Axis", "AXIS_Z", Vector3::AXIS_Z);

	_VariantCall::add_variant_constant(Variant::VECTOR3, "ZERO", Vector3(0, 0, 0));
	_VariantCall::add_variant_constant(Variant::VECTOR3, "ONE", Vector3(1, 1, 1));
	_VariantCall::add_variant_constant(Variant::VECTOR3, "INF", Vector3(INFINITY, INFINITY, INFINITY));
	_VariantCall::add_variant_constant(Variant::VECTOR3, "LEFT", Vector3(-1, 0, 0));
	_VariantCall::add_variant_constant(Variant::VECTOR3, "RIGHT", Vector3(1, 0, 0));
	_VariantCall::add_variant_constant(Variant::VECTOR3, "UP", Vector3(0, 1, 0));
	_VariantCall::add_variant_constant(Variant::VECTOR3, "DOWN", Vector3(0, -1, 0));
	_VariantCall::add_variant_constant(Variant::VECTOR3, "FORWARD", Vector3(0, 0, -1));
	_VariantCall::add_variant_constant(Variant::VECTOR3, "BACK", Vector3(0, 0, 1));

	_VariantCall::add_variant_constant(Variant::VECTOR3, "MODEL_LEFT", Vector3(1, 0, 0));
	_VariantCall::add_variant_constant(Variant::VECTOR3, "MODEL_RIGHT", Vector3(-1, 0, 0));
	_VariantCall::add_variant_constant(Variant::VECTOR3, "MODEL_TOP", Vector3(0, 1, 0));
	_VariantCall::add_variant_constant(Variant::VECTOR3, "MODEL_BOTTOM", Vector3(0, -1, 0));
	_VariantCall::add_variant_constant(Variant::VECTOR3, "MODEL_FRONT", Vector3(0, 0, 1));
	_VariantCall::add_variant_constant(Variant::VECTOR3, "MODEL_REAR", Vector3(0, 0, -1));

	_VariantCall::add_enum_constant(Variant::VECTOR4, "Axis", "AXIS_X", Vector4::AXIS_X);
	_VariantCall::add_enum_constant(Variant::VECTOR4, "Axis", "AXIS_Y", Vector4::AXIS_Y);
	_VariantCall::add_enum_constant(Variant::VECTOR4, "Axis", "AXIS_Z", Vector4::AXIS_Z);
	_VariantCall::add_enum_constant(Variant::VECTOR4, "Axis", "AXIS_W", Vector4::AXIS_W);

	_VariantCall::add_variant_constant(Variant::VECTOR4, "ZERO", Vector4(0, 0, 0, 0));
	_VariantCall::add_variant_constant(Variant::VECTOR4, "ONE", Vector4(1, 1, 1, 1));
	_VariantCall::add_variant_constant(Variant::VECTOR4, "INF", Vector4(INFINITY, INFINITY, INFINITY, INFINITY));

	_VariantCall::add_enum_constant(Variant::VECTOR3I, "Axis", "AXIS_X", Vector3i::AXIS_X);
	_VariantCall::add_enum_constant(Variant::VECTOR3I, "Axis", "AXIS_Y", Vector3i::AXIS_Y);
	_VariantCall::add_enum_constant(Variant::VECTOR3I, "Axis", "AXIS_Z", Vector3i::AXIS_Z);

	_VariantCall::add_enum_constant(Variant::VECTOR4I, "Axis", "AXIS_X", Vector4i::AXIS_X);
	_VariantCall::add_enum_constant(Variant::VECTOR4I, "Axis", "AXIS_Y", Vector4i::AXIS_Y);
	_VariantCall::add_enum_constant(Variant::VECTOR4I, "Axis", "AXIS_Z", Vector4i::AXIS_Z);
	_VariantCall::add_enum_constant(Variant::VECTOR4I, "Axis", "AXIS_W", Vector4i::AXIS_W);

	_VariantCall::add_variant_constant(Variant::VECTOR4I, "ZERO", Vector4i(0, 0, 0, 0));
	_VariantCall::add_variant_constant(Variant::VECTOR4I, "ONE", Vector4i(1, 1, 1, 1));
	_VariantCall::add_variant_constant(Variant::VECTOR4I, "MIN", Vector4i(INT32_MIN, INT32_MIN, INT32_MIN, INT32_MIN));
	_VariantCall::add_variant_constant(Variant::VECTOR4I, "MAX", Vector4i(INT32_MAX, INT32_MAX, INT32_MAX, INT32_MAX));

	_VariantCall::add_variant_constant(Variant::VECTOR3I, "ZERO", Vector3i(0, 0, 0));
	_VariantCall::add_variant_constant(Variant::VECTOR3I, "ONE", Vector3i(1, 1, 1));
	_VariantCall::add_variant_constant(Variant::VECTOR3I, "MIN", Vector3i(INT32_MIN, INT32_MIN, INT32_MIN));
	_VariantCall::add_variant_constant(Variant::VECTOR3I, "MAX", Vector3i(INT32_MAX, INT32_MAX, INT32_MAX));
	_VariantCall::add_variant_constant(Variant::VECTOR3I, "LEFT", Vector3i(-1, 0, 0));
	_VariantCall::add_variant_constant(Variant::VECTOR3I, "RIGHT", Vector3i(1, 0, 0));
	_VariantCall::add_variant_constant(Variant::VECTOR3I, "UP", Vector3i(0, 1, 0));
	_VariantCall::add_variant_constant(Variant::VECTOR3I, "DOWN", Vector3i(0, -1, 0));
	_VariantCall::add_variant_constant(Variant::VECTOR3I, "FORWARD", Vector3i(0, 0, -1));
	_VariantCall::add_variant_constant(Variant::VECTOR3I, "BACK", Vector3i(0, 0, 1));

	_VariantCall::add_enum_constant(Variant::VECTOR2, "Axis", "AXIS_X", Vector2::AXIS_X);
	_VariantCall::add_enum_constant(Variant::VECTOR2, "Axis", "AXIS_Y", Vector2::AXIS_Y);

	_VariantCall::add_enum_constant(Variant::VECTOR2I, "Axis", "AXIS_X", Vector2i::AXIS_X);
	_VariantCall::add_enum_constant(Variant::VECTOR2I, "Axis", "AXIS_Y", Vector2i::AXIS_Y);

	_VariantCall::add_variant_constant(Variant::VECTOR2, "ZERO", Vector2(0, 0));
	_VariantCall::add_variant_constant(Variant::VECTOR2, "ONE", Vector2(1, 1));
	_VariantCall::add_variant_constant(Variant::VECTOR2, "INF", Vector2(INFINITY, INFINITY));
	_VariantCall::add_variant_constant(Variant::VECTOR2, "LEFT", Vector2(-1, 0));
	_VariantCall::add_variant_constant(Variant::VECTOR2, "RIGHT", Vector2(1, 0));
	_VariantCall::add_variant_constant(Variant::VECTOR2, "UP", Vector2(0, -1));
	_VariantCall::add_variant_constant(Variant::VECTOR2, "DOWN", Vector2(0, 1));

	_VariantCall::add_variant_constant(Variant::VECTOR2I, "ZERO", Vector2i(0, 0));
	_VariantCall::add_variant_constant(Variant::VECTOR2I, "ONE", Vector2i(1, 1));
	_VariantCall::add_variant_constant(Variant::VECTOR2I, "MIN", Vector2i(INT32_MIN, INT32_MIN));
	_VariantCall::add_variant_constant(Variant::VECTOR2I, "MAX", Vector2i(INT32_MAX, INT32_MAX));
	_VariantCall::add_variant_constant(Variant::VECTOR2I, "LEFT", Vector2i(-1, 0));
	_VariantCall::add_variant_constant(Variant::VECTOR2I, "RIGHT", Vector2i(1, 0));
	_VariantCall::add_variant_constant(Variant::VECTOR2I, "UP", Vector2i(0, -1));
	_VariantCall::add_variant_constant(Variant::VECTOR2I, "DOWN", Vector2i(0, 1));

	_VariantCall::add_variant_constant(Variant::TRANSFORM2D, "IDENTITY", Transform2D());
	_VariantCall::add_variant_constant(Variant::TRANSFORM2D, "FLIP_X", Transform2D(-1, 0, 0, 1, 0, 0));
	_VariantCall::add_variant_constant(Variant::TRANSFORM2D, "FLIP_Y", Transform2D(1, 0, 0, -1, 0, 0));

	Transform3D identity_transform;
	Transform3D flip_x_transform = Transform3D(-1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0);
	Transform3D flip_y_transform = Transform3D(1, 0, 0, 0, -1, 0, 0, 0, 1, 0, 0, 0);
	Transform3D flip_z_transform = Transform3D(1, 0, 0, 0, 1, 0, 0, 0, -1, 0, 0, 0);
	_VariantCall::add_variant_constant(Variant::TRANSFORM3D, "IDENTITY", identity_transform);
	_VariantCall::add_variant_constant(Variant::TRANSFORM3D, "FLIP_X", flip_x_transform);
	_VariantCall::add_variant_constant(Variant::TRANSFORM3D, "FLIP_Y", flip_y_transform);
	_VariantCall::add_variant_constant(Variant::TRANSFORM3D, "FLIP_Z", flip_z_transform);

	Basis identity_basis;
	Basis flip_x_basis = Basis(-1, 0, 0, 0, 1, 0, 0, 0, 1);
	Basis flip_y_basis = Basis(1, 0, 0, 0, -1, 0, 0, 0, 1);
	Basis flip_z_basis = Basis(1, 0, 0, 0, 1, 0, 0, 0, -1);
	_VariantCall::add_variant_constant(Variant::BASIS, "IDENTITY", identity_basis);
	_VariantCall::add_variant_constant(Variant::BASIS, "FLIP_X", flip_x_basis);
	_VariantCall::add_variant_constant(Variant::BASIS, "FLIP_Y", flip_y_basis);
	_VariantCall::add_variant_constant(Variant::BASIS, "FLIP_Z", flip_z_basis);

	_VariantCall::add_variant_constant(Variant::PLANE, "PLANE_YZ", Plane(Vector3(1, 0, 0), 0));
	_VariantCall::add_variant_constant(Variant::PLANE, "PLANE_XZ", Plane(Vector3(0, 1, 0), 0));
	_VariantCall::add_variant_constant(Variant::PLANE, "PLANE_XY", Plane(Vector3(0, 0, 1), 0));

	_VariantCall::add_variant_constant(Variant::QUATERNION, "IDENTITY", Quaternion(0, 0, 0, 1));

	_VariantCall::add_enum_constant(Variant::PROJECTION, "Planes", "PLANE_NEAR", Projection::PLANE_NEAR);
	_VariantCall::add_enum_constant(Variant::PROJECTION, "Planes", "PLANE_FAR", Projection::PLANE_FAR);
	_VariantCall::add_enum_constant(Variant::PROJECTION, "Planes", "PLANE_LEFT", Projection::PLANE_LEFT);
	_VariantCall::add_enum_constant(Variant::PROJECTION, "Planes", "PLANE_TOP", Projection::PLANE_TOP);
	_VariantCall::add_enum_constant(Variant::PROJECTION, "Planes", "PLANE_RIGHT", Projection::PLANE_RIGHT);
	_VariantCall::add_enum_constant(Variant::PROJECTION, "Planes", "PLANE_BOTTOM", Projection::PLANE_BOTTOM);

	Projection p;
	_VariantCall::add_variant_constant(Variant::PROJECTION, "IDENTITY", p);
	p.set_zero();
	_VariantCall::add_variant_constant(Variant::PROJECTION, "ZERO", p);
}

void Variant::_register_variant_methods() {
	_register_variant_builtin_methods_string();
	_register_variant_builtin_methods_math();
	_register_variant_builtin_methods_misc();
	_register_variant_builtin_methods_array();
	_register_variant_builtin_constants();
}

void Variant::_unregister_variant_methods() {
	//clear methods
	memdelete_arr(builtin_method_names);
	memdelete_arr(builtin_method_info);
	memdelete_arr(_VariantCall::constant_data);
	memdelete_arr(_VariantCall::enum_data);
}

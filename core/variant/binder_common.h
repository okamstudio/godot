/**************************************************************************/
/*  binder_common.h                                                       */
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

#ifndef BINDER_COMMON_H
#define BINDER_COMMON_H

#include "core/input/input_enums.h"
#include "core/object/object.h"
#include "core/os/keyboard.h"
#include "core/templates/simple_type.h"
#include "core/typedefs.h"
#include "core/variant/method_ptrcall.h"
#include "core/variant/type_info.h"
#include "core/variant/variant.h"
#include "core/variant/variant_internal.h"

#include <stdio.h>

// Variant cannot define an implicit cast operator for every Object subclass, so the
// casting is done here, to allow binding methods with parameters more specific than Object *

template <typename T>
struct VariantCaster {
	static _FORCE_INLINE_ T cast(const Variant &p_variant) {
		using TStripped = std::remove_pointer_t<T>;
		if constexpr (std::is_base_of_v<Object, TStripped>) {
			return Object::cast_to<TStripped>(p_variant);
		} else {
			return p_variant;
		}
	}
};

template <typename T>
struct VariantCaster<T &> {
	static _FORCE_INLINE_ T cast(const Variant &p_variant) {
		using TStripped = std::remove_pointer_t<T>;
		if constexpr (std::is_base_of_v<Object, TStripped>) {
			return Object::cast_to<TStripped>(p_variant);
		} else {
			return p_variant;
		}
	}
};

template <typename T>
struct VariantCaster<const T &> {
	static _FORCE_INLINE_ T cast(const Variant &p_variant) {
		using TStripped = std::remove_pointer_t<T>;
		if constexpr (std::is_base_of_v<Object, TStripped>) {
			return Object::cast_to<TStripped>(p_variant);
		} else {
			return p_variant;
		}
	}
};

#define VARIANT_ENUM_CAST(m_enum)                                                                                       \
	MAKE_ENUM_TYPE_INFO(m_enum)                                                                                         \
	template <>                                                                                                         \
	struct VariantCaster<m_enum> {                                                                                      \
		static _FORCE_INLINE_ m_enum cast(const Variant &p_variant) {                                                   \
			return (m_enum)p_variant.operator int64_t();                                                                \
		}                                                                                                               \
	};                                                                                                                  \
	template <>                                                                                                         \
	struct PtrToArg<m_enum> {                                                                                           \
		_FORCE_INLINE_ static m_enum convert(const void *p_ptr) {                                                       \
			return m_enum(*reinterpret_cast<const int64_t *>(p_ptr));                                                   \
		}                                                                                                               \
		typedef int64_t EncodeT;                                                                                        \
		_FORCE_INLINE_ static void encode(m_enum p_val, const void *p_ptr) {                                            \
			*(int64_t *)p_ptr = (int64_t)p_val;                                                                         \
		}                                                                                                               \
	};                                                                                                                  \
	template <>                                                                                                         \
	struct ZeroInitializer<m_enum> {                                                                                    \
		static void initialize(m_enum &value) { value = (m_enum)0; }                                                    \
	};                                                                                                                  \
	template <>                                                                                                         \
	struct VariantInternalAccessor<m_enum> {                                                                            \
		static _FORCE_INLINE_ m_enum get(const Variant *v) { return m_enum(*VariantInternal::get_int(v)); }             \
		static _FORCE_INLINE_ void set(Variant *v, m_enum p_value) { *VariantInternal::get_int(v) = (int64_t)p_value; } \
	};

#define VARIANT_BITFIELD_CAST(m_enum)                                                                                                       \
	MAKE_BITFIELD_TYPE_INFO(m_enum)                                                                                                         \
	template <>                                                                                                                             \
	struct VariantCaster<BitField<m_enum>> {                                                                                                \
		static _FORCE_INLINE_ BitField<m_enum> cast(const Variant &p_variant) {                                                             \
			return BitField<m_enum>(p_variant.operator int64_t());                                                                          \
		}                                                                                                                                   \
	};                                                                                                                                      \
	template <>                                                                                                                             \
	struct PtrToArg<BitField<m_enum>> {                                                                                                     \
		_FORCE_INLINE_ static BitField<m_enum> convert(const void *p_ptr) {                                                                 \
			return BitField<m_enum>(*reinterpret_cast<const int64_t *>(p_ptr));                                                             \
		}                                                                                                                                   \
		typedef int64_t EncodeT;                                                                                                            \
		_FORCE_INLINE_ static void encode(BitField<m_enum> p_val, const void *p_ptr) {                                                      \
			*(int64_t *)p_ptr = p_val;                                                                                                      \
		}                                                                                                                                   \
	};                                                                                                                                      \
	template <>                                                                                                                             \
	struct ZeroInitializer<BitField<m_enum>> {                                                                                              \
		static void initialize(BitField<m_enum> &value) { value = 0; }                                                                      \
	};                                                                                                                                      \
	template <>                                                                                                                             \
	struct VariantInternalAccessor<BitField<m_enum>> {                                                                                      \
		static _FORCE_INLINE_ BitField<m_enum> get(const Variant *v) { return BitField<m_enum>(*VariantInternal::get_int(v)); }             \
		static _FORCE_INLINE_ void set(Variant *v, BitField<m_enum> p_value) { *VariantInternal::get_int(v) = p_value.operator int64_t(); } \
	};

// Object enum casts must go here
VARIANT_ENUM_CAST(Object::ConnectFlags);

VARIANT_ENUM_CAST(Vector2::Axis);
VARIANT_ENUM_CAST(Vector2i::Axis);
VARIANT_ENUM_CAST(Vector3::Axis);
VARIANT_ENUM_CAST(Vector3i::Axis);
VARIANT_ENUM_CAST(Vector4::Axis);
VARIANT_ENUM_CAST(Vector4i::Axis);
VARIANT_ENUM_CAST(EulerOrder);
VARIANT_ENUM_CAST(Projection::Planes);

VARIANT_ENUM_CAST(Error);
VARIANT_ENUM_CAST(Side);
VARIANT_ENUM_CAST(ClockDirection);
VARIANT_ENUM_CAST(Corner);
VARIANT_ENUM_CAST(HatDir);
VARIANT_BITFIELD_CAST(HatMask);
VARIANT_ENUM_CAST(JoyAxis);
VARIANT_ENUM_CAST(JoyButton);

VARIANT_ENUM_CAST(MIDIMessage);
VARIANT_ENUM_CAST(MouseButton);
VARIANT_BITFIELD_CAST(MouseButtonMask);
VARIANT_ENUM_CAST(Orientation);
VARIANT_ENUM_CAST(HorizontalAlignment);
VARIANT_ENUM_CAST(VerticalAlignment);
VARIANT_ENUM_CAST(InlineAlignment);
VARIANT_ENUM_CAST(PropertyHint);
VARIANT_BITFIELD_CAST(PropertyUsageFlags);
VARIANT_ENUM_CAST(Variant::Type);
VARIANT_ENUM_CAST(Variant::Operator);

// Key

VARIANT_ENUM_CAST(Key);
VARIANT_BITFIELD_CAST(KeyModifierMask);
VARIANT_ENUM_CAST(KeyLocation);

static inline Key &operator|=(Key &a, BitField<KeyModifierMask> b) {
	a = static_cast<Key>(static_cast<int>(a) | static_cast<int>(b.operator int64_t()));
	return a;
}

static inline Key &operator&=(Key &a, BitField<KeyModifierMask> b) {
	a = static_cast<Key>(static_cast<int>(a) & static_cast<int>(b.operator int64_t()));
	return a;
}

static inline Key operator|(Key a, BitField<KeyModifierMask> b) {
	return (Key)((int)a | (int)b.operator int64_t());
}

static inline Key operator&(Key a, BitField<KeyModifierMask> b) {
	return (Key)((int)a & (int)b.operator int64_t());
}

static inline Key operator+(BitField<KeyModifierMask> a, Key b) {
	return (Key)((int)a.operator int64_t() + (int)b);
}

static inline Key operator|(BitField<KeyModifierMask> a, Key b) {
	return (Key)((int)a.operator int64_t() | (int)b);
}

template <>
struct VariantCaster<char32_t> {
	static _FORCE_INLINE_ char32_t cast(const Variant &p_variant) {
		return (char32_t)p_variant.operator int();
	}
};

template <>
struct PtrToArg<char32_t> {
	_FORCE_INLINE_ static char32_t convert(const void *p_ptr) {
		return char32_t(*reinterpret_cast<const int64_t *>(p_ptr));
	}
	typedef int64_t EncodeT;
	_FORCE_INLINE_ static void encode(char32_t p_val, const void *p_ptr) {
		*(int64_t *)p_ptr = p_val;
	}
};

template <typename T>
struct VariantObjectClassChecker {
	static _FORCE_INLINE_ bool check(const Variant &p_variant) {
		using TStripped = std::remove_pointer_t<T>;
		if constexpr (std::is_base_of_v<Object, TStripped>) {
			Object *obj = p_variant;
			return Object::cast_to<TStripped>(p_variant) || !obj;
		} else {
			return true;
		}
	}
};

template <typename T>
class Ref;

template <typename T>
struct VariantObjectClassChecker<const Ref<T> &> {
	static _FORCE_INLINE_ bool check(const Variant &p_variant) {
		Object *obj = p_variant;
		const Ref<T> node = p_variant;
		return node.ptr() || !obj;
	}
};

#ifdef DEBUG_METHODS_ENABLED

template <typename T>
struct VariantCasterAndValidate {
	static _FORCE_INLINE_ T cast(const Variant **p_args, uint32_t p_arg_idx, Callable::CallError &r_error) {
		Variant::Type argtype = GetTypeInfo<T>::VARIANT_TYPE;
		if (!Variant::can_convert_strict(p_args[p_arg_idx]->get_type(), argtype) ||
				!VariantObjectClassChecker<T>::check(*p_args[p_arg_idx])) {
			r_error.error = Callable::CallError::CALL_ERROR_INVALID_ARGUMENT;
			r_error.argument = p_arg_idx;
			r_error.expected = argtype;
		}

		return VariantCaster<T>::cast(*p_args[p_arg_idx]);
	}
};

template <typename T>
struct VariantCasterAndValidate<T &> {
	static _FORCE_INLINE_ T cast(const Variant **p_args, uint32_t p_arg_idx, Callable::CallError &r_error) {
		Variant::Type argtype = GetTypeInfo<T>::VARIANT_TYPE;
		if (!Variant::can_convert_strict(p_args[p_arg_idx]->get_type(), argtype) ||
				!VariantObjectClassChecker<T>::check(*p_args[p_arg_idx])) {
			r_error.error = Callable::CallError::CALL_ERROR_INVALID_ARGUMENT;
			r_error.argument = p_arg_idx;
			r_error.expected = argtype;
		}

		return VariantCaster<T>::cast(*p_args[p_arg_idx]);
	}
};

template <typename T>
struct VariantCasterAndValidate<const T &> {
	static _FORCE_INLINE_ T cast(const Variant **p_args, uint32_t p_arg_idx, Callable::CallError &r_error) {
		Variant::Type argtype = GetTypeInfo<T>::VARIANT_TYPE;
		if (!Variant::can_convert_strict(p_args[p_arg_idx]->get_type(), argtype) ||
				!VariantObjectClassChecker<T>::check(*p_args[p_arg_idx])) {
			r_error.error = Callable::CallError::CALL_ERROR_INVALID_ARGUMENT;
			r_error.argument = p_arg_idx;
			r_error.expected = argtype;
		}

		return VariantCaster<T>::cast(*p_args[p_arg_idx]);
	}
};

#endif // DEBUG_METHODS_ENABLED

template <typename T, typename... P, size_t... Is>
void call_with_variant_args_helper(T *p_instance, void (T::*p_method)(P...), const Variant **p_args, Callable::CallError &r_error, IndexSequence<Is...>) {
	r_error.error = Callable::CallError::CALL_OK;

#ifdef DEBUG_METHODS_ENABLED
	(p_instance->*p_method)(VariantCasterAndValidate<P>::cast(p_args, Is, r_error)...);
#else
	(p_instance->*p_method)(VariantCaster<P>::cast(*p_args[Is])...);
#endif
	(void)(p_args); //avoid warning
}

template <typename T, typename... P, size_t... Is>
void call_with_variant_argsc_helper(T *p_instance, void (T::*p_method)(P...) const, const Variant **p_args, Callable::CallError &r_error, IndexSequence<Is...>) {
	r_error.error = Callable::CallError::CALL_OK;

#ifdef DEBUG_METHODS_ENABLED
	(p_instance->*p_method)(VariantCasterAndValidate<P>::cast(p_args, Is, r_error)...);
#else
	(p_instance->*p_method)(VariantCaster<P>::cast(*p_args[Is])...);
#endif
	(void)(p_args); //avoid warning
}

template <typename T, typename... P, size_t... Is>
void call_with_ptr_args_helper(T *p_instance, void (T::*p_method)(P...), const void **p_args, IndexSequence<Is...>) {
	(p_instance->*p_method)(PtrToArg<P>::convert(p_args[Is])...);
}

template <typename T, typename... P, size_t... Is>
void call_with_ptr_argsc_helper(T *p_instance, void (T::*p_method)(P...) const, const void **p_args, IndexSequence<Is...>) {
	(p_instance->*p_method)(PtrToArg<P>::convert(p_args[Is])...);
}

template <typename T, typename R, typename... P, size_t... Is>
void call_with_ptr_args_ret_helper(T *p_instance, R (T::*p_method)(P...), const void **p_args, void *r_ret, IndexSequence<Is...>) {
	PtrToArg<R>::encode((p_instance->*p_method)(PtrToArg<P>::convert(p_args[Is])...), r_ret);
}

template <typename T, typename R, typename... P, size_t... Is>
void call_with_ptr_args_retc_helper(T *p_instance, R (T::*p_method)(P...) const, const void **p_args, void *r_ret, IndexSequence<Is...>) {
	PtrToArg<R>::encode((p_instance->*p_method)(PtrToArg<P>::convert(p_args[Is])...), r_ret);
}

template <typename T, typename... P, size_t... Is>
void call_with_ptr_args_static_helper(T *p_instance, void (*p_method)(T *, P...), const void **p_args, IndexSequence<Is...>) {
	p_method(p_instance, PtrToArg<P>::convert(p_args[Is])...);
}

template <typename T, typename R, typename... P, size_t... Is>
void call_with_ptr_args_static_retc_helper(T *p_instance, R (*p_method)(T *, P...), const void **p_args, void *r_ret, IndexSequence<Is...>) {
	PtrToArg<R>::encode(p_method(p_instance, PtrToArg<P>::convert(p_args[Is])...), r_ret);
}

template <typename R, typename... P, size_t... Is>
void call_with_ptr_args_static_method_ret_helper(R (*p_method)(P...), const void **p_args, void *r_ret, IndexSequence<Is...>) {
	PtrToArg<R>::encode(p_method(PtrToArg<P>::convert(p_args[Is])...), r_ret);
}

template <typename... P, size_t... Is>
void call_with_ptr_args_static_method_helper(void (*p_method)(P...), const void **p_args, IndexSequence<Is...>) {
	p_method(PtrToArg<P>::convert(p_args[Is])...);
}

template <typename T, typename... P, size_t... Is>
void call_with_validated_variant_args_helper(T *p_instance, void (T::*p_method)(P...), const Variant **p_args, IndexSequence<Is...>) {
	(p_instance->*p_method)((VariantInternalAccessor<GetSimpleTypeT<P>>::get(p_args[Is]))...);
}

template <typename T, typename... P, size_t... Is>
void call_with_validated_variant_argsc_helper(T *p_instance, void (T::*p_method)(P...) const, const Variant **p_args, IndexSequence<Is...>) {
	(p_instance->*p_method)((VariantInternalAccessor<GetSimpleTypeT<P>>::get(p_args[Is]))...);
}

template <typename T, typename R, typename... P, size_t... Is>
void call_with_validated_variant_args_ret_helper(T *p_instance, R (T::*p_method)(P...), const Variant **p_args, Variant *r_ret, IndexSequence<Is...>) {
	VariantInternalAccessor<GetSimpleTypeT<R>>::set(r_ret, (p_instance->*p_method)((VariantInternalAccessor<GetSimpleTypeT<P>>::get(p_args[Is]))...));
}

template <typename T, typename R, typename... P, size_t... Is>
void call_with_validated_variant_args_retc_helper(T *p_instance, R (T::*p_method)(P...) const, const Variant **p_args, Variant *r_ret, IndexSequence<Is...>) {
	VariantInternalAccessor<GetSimpleTypeT<R>>::set(r_ret, (p_instance->*p_method)((VariantInternalAccessor<GetSimpleTypeT<P>>::get(p_args[Is]))...));
}

template <typename T, typename R, typename... P, size_t... Is>
void call_with_validated_variant_args_static_retc_helper(T *p_instance, R (*p_method)(T *, P...), const Variant **p_args, Variant *r_ret, IndexSequence<Is...>) {
	VariantInternalAccessor<GetSimpleTypeT<R>>::set(r_ret, p_method(p_instance, (VariantInternalAccessor<GetSimpleTypeT<P>>::get(p_args[Is]))...));
}

template <typename T, typename... P, size_t... Is>
void call_with_validated_variant_args_static_helper(T *p_instance, void (*p_method)(T *, P...), const Variant **p_args, IndexSequence<Is...>) {
	p_method(p_instance, (VariantInternalAccessor<GetSimpleTypeT<P>>::get(p_args[Is]))...);
}

template <typename R, typename... P, size_t... Is>
void call_with_validated_variant_args_static_method_ret_helper(R (*p_method)(P...), const Variant **p_args, Variant *r_ret, IndexSequence<Is...>) {
	VariantInternalAccessor<GetSimpleTypeT<R>>::set(r_ret, p_method((VariantInternalAccessor<GetSimpleTypeT<P>>::get(p_args[Is]))...));
}

template <typename... P, size_t... Is>
void call_with_validated_variant_args_static_method_helper(void (*p_method)(P...), const Variant **p_args, IndexSequence<Is...>) {
	p_method((VariantInternalAccessor<GetSimpleTypeT<P>>::get(p_args[Is]))...);
}

template <typename T, typename... P>
void call_with_variant_args(T *p_instance, void (T::*p_method)(P...), const Variant **p_args, int p_argcount, Callable::CallError &r_error) {
#ifdef DEBUG_METHODS_ENABLED
	if ((size_t)p_argcount > sizeof...(P)) {
		r_error.error = Callable::CallError::CALL_ERROR_TOO_MANY_ARGUMENTS;
		r_error.expected = sizeof...(P);
		return;
	}

	if ((size_t)p_argcount < sizeof...(P)) {
		r_error.error = Callable::CallError::CALL_ERROR_TOO_FEW_ARGUMENTS;
		r_error.expected = sizeof...(P);
		return;
	}
#endif
	call_with_variant_args_helper<T, P...>(p_instance, p_method, p_args, r_error, BuildIndexSequence<sizeof...(P)>{});
}

template <typename T, typename... P>
void call_with_variant_args_dv(T *p_instance, void (T::*p_method)(P...), const Variant **p_args, int p_argcount, Callable::CallError &r_error, const Vector<Variant> &default_values) {
#ifdef DEBUG_ENABLED
	if ((size_t)p_argcount > sizeof...(P)) {
		r_error.error = Callable::CallError::CALL_ERROR_TOO_MANY_ARGUMENTS;
		r_error.expected = sizeof...(P);
		return;
	}
#endif

	int32_t missing = (int32_t)sizeof...(P) - (int32_t)p_argcount;

	int32_t dvs = default_values.size();
#ifdef DEBUG_ENABLED
	if (missing > dvs) {
		r_error.error = Callable::CallError::CALL_ERROR_TOO_FEW_ARGUMENTS;
		r_error.expected = sizeof...(P);
		return;
	}
#endif

	const Variant *args[sizeof...(P) == 0 ? 1 : sizeof...(P)]; //avoid zero sized array
	for (int32_t i = 0; i < (int32_t)sizeof...(P); i++) {
		if (i < p_argcount) {
			args[i] = p_args[i];
		} else {
			args[i] = &default_values[i - p_argcount + (dvs - missing)];
		}
	}

	call_with_variant_args_helper(p_instance, p_method, args, r_error, BuildIndexSequence<sizeof...(P)>{});
}

template <typename T, typename... P>
void call_with_variant_argsc(T *p_instance, void (T::*p_method)(P...) const, const Variant **p_args, int p_argcount, Callable::CallError &r_error) {
#ifdef DEBUG_METHODS_ENABLED
	if ((size_t)p_argcount > sizeof...(P)) {
		r_error.error = Callable::CallError::CALL_ERROR_TOO_MANY_ARGUMENTS;
		r_error.expected = sizeof...(P);
		return;
	}

	if ((size_t)p_argcount < sizeof...(P)) {
		r_error.error = Callable::CallError::CALL_ERROR_TOO_FEW_ARGUMENTS;
		r_error.expected = sizeof...(P);
		return;
	}
#endif
	call_with_variant_argsc_helper<T, P...>(p_instance, p_method, p_args, r_error, BuildIndexSequence<sizeof...(P)>{});
}

template <typename T, typename... P>
void call_with_variant_argsc_dv(T *p_instance, void (T::*p_method)(P...) const, const Variant **p_args, int p_argcount, Callable::CallError &r_error, const Vector<Variant> &default_values) {
#ifdef DEBUG_ENABLED
	if ((size_t)p_argcount > sizeof...(P)) {
		r_error.error = Callable::CallError::CALL_ERROR_TOO_MANY_ARGUMENTS;
		r_error.expected = sizeof...(P);
		return;
	}
#endif

	int32_t missing = (int32_t)sizeof...(P) - (int32_t)p_argcount;

	int32_t dvs = default_values.size();
#ifdef DEBUG_ENABLED
	if (missing > dvs) {
		r_error.error = Callable::CallError::CALL_ERROR_TOO_FEW_ARGUMENTS;
		r_error.expected = sizeof...(P);
		return;
	}
#endif

	const Variant *args[sizeof...(P) == 0 ? 1 : sizeof...(P)]; //avoid zero sized array
	for (int32_t i = 0; i < (int32_t)sizeof...(P); i++) {
		if (i < p_argcount) {
			args[i] = p_args[i];
		} else {
			args[i] = &default_values[i - p_argcount + (dvs - missing)];
		}
	}

	call_with_variant_argsc_helper(p_instance, p_method, args, r_error, BuildIndexSequence<sizeof...(P)>{});
}

template <typename T, typename R, typename... P>
void call_with_variant_args_ret_dv(T *p_instance, R (T::*p_method)(P...), const Variant **p_args, int p_argcount, Variant &r_ret, Callable::CallError &r_error, const Vector<Variant> &default_values) {
#ifdef DEBUG_ENABLED
	if ((size_t)p_argcount > sizeof...(P)) {
		r_error.error = Callable::CallError::CALL_ERROR_TOO_MANY_ARGUMENTS;
		r_error.expected = sizeof...(P);
		return;
	}
#endif

	int32_t missing = (int32_t)sizeof...(P) - (int32_t)p_argcount;

	int32_t dvs = default_values.size();
#ifdef DEBUG_ENABLED
	if (missing > dvs) {
		r_error.error = Callable::CallError::CALL_ERROR_TOO_FEW_ARGUMENTS;
		r_error.expected = sizeof...(P);
		return;
	}
#endif

	const Variant *args[sizeof...(P) == 0 ? 1 : sizeof...(P)]; //avoid zero sized array
	for (int32_t i = 0; i < (int32_t)sizeof...(P); i++) {
		if (i < p_argcount) {
			args[i] = p_args[i];
		} else {
			args[i] = &default_values[i - p_argcount + (dvs - missing)];
		}
	}

	call_with_variant_args_ret_helper(p_instance, p_method, args, r_ret, r_error, BuildIndexSequence<sizeof...(P)>{});
}

template <typename T, typename R, typename... P>
void call_with_variant_args_retc_dv(T *p_instance, R (T::*p_method)(P...) const, const Variant **p_args, int p_argcount, Variant &r_ret, Callable::CallError &r_error, const Vector<Variant> &default_values) {
#ifdef DEBUG_ENABLED
	if ((size_t)p_argcount > sizeof...(P)) {
		r_error.error = Callable::CallError::CALL_ERROR_TOO_MANY_ARGUMENTS;
		r_error.expected = sizeof...(P);
		return;
	}
#endif

	int32_t missing = (int32_t)sizeof...(P) - (int32_t)p_argcount;

	int32_t dvs = default_values.size();
#ifdef DEBUG_ENABLED
	if (missing > dvs) {
		r_error.error = Callable::CallError::CALL_ERROR_TOO_FEW_ARGUMENTS;
		r_error.expected = sizeof...(P);
		return;
	}
#endif

	const Variant *args[sizeof...(P) == 0 ? 1 : sizeof...(P)]; //avoid zero sized array
	for (int32_t i = 0; i < (int32_t)sizeof...(P); i++) {
		if (i < p_argcount) {
			args[i] = p_args[i];
		} else {
			args[i] = &default_values[i - p_argcount + (dvs - missing)];
		}
	}

	call_with_variant_args_retc_helper(p_instance, p_method, args, r_ret, r_error, BuildIndexSequence<sizeof...(P)>{});
}

template <typename T, typename... P>
void call_with_ptr_args(T *p_instance, void (T::*p_method)(P...), const void **p_args) {
	call_with_ptr_args_helper<T, P...>(p_instance, p_method, p_args, BuildIndexSequence<sizeof...(P)>{});
}

template <typename T, typename... P>
void call_with_ptr_argsc(T *p_instance, void (T::*p_method)(P...) const, const void **p_args) {
	call_with_ptr_argsc_helper<T, P...>(p_instance, p_method, p_args, BuildIndexSequence<sizeof...(P)>{});
}

template <typename T, typename R, typename... P>
void call_with_ptr_args_ret(T *p_instance, R (T::*p_method)(P...), const void **p_args, void *r_ret) {
	call_with_ptr_args_ret_helper<T, R, P...>(p_instance, p_method, p_args, r_ret, BuildIndexSequence<sizeof...(P)>{});
}

template <typename T, typename R, typename... P>
void call_with_ptr_args_retc(T *p_instance, R (T::*p_method)(P...) const, const void **p_args, void *r_ret) {
	call_with_ptr_args_retc_helper<T, R, P...>(p_instance, p_method, p_args, r_ret, BuildIndexSequence<sizeof...(P)>{});
}

template <typename T, typename... P>
void call_with_ptr_args_static(T *p_instance, void (*p_method)(T *, P...), const void **p_args) {
	call_with_ptr_args_static_helper<T, P...>(p_instance, p_method, p_args, BuildIndexSequence<sizeof...(P)>{});
}

template <typename T, typename R, typename... P>
void call_with_ptr_args_static_retc(T *p_instance, R (*p_method)(T *, P...), const void **p_args, void *r_ret) {
	call_with_ptr_args_static_retc_helper<T, R, P...>(p_instance, p_method, p_args, r_ret, BuildIndexSequence<sizeof...(P)>{});
}

template <typename R, typename... P>
void call_with_ptr_args_static_method_ret(R (*p_method)(P...), const void **p_args, void *r_ret) {
	call_with_ptr_args_static_method_ret_helper<R, P...>(p_method, p_args, r_ret, BuildIndexSequence<sizeof...(P)>{});
}

template <typename... P>
void call_with_ptr_args_static_method(void (*p_method)(P...), const void **p_args) {
	call_with_ptr_args_static_method_helper<P...>(p_method, p_args, BuildIndexSequence<sizeof...(P)>{});
}

// Validated

template <typename T, typename... P>
void call_with_validated_variant_args(Variant *base, void (T::*p_method)(P...), const Variant **p_args) {
	call_with_validated_variant_args_helper<T, P...>(VariantGetInternalPtr<T>::get_ptr(base), p_method, p_args, BuildIndexSequence<sizeof...(P)>{});
}

template <typename T, typename R, typename... P>
void call_with_validated_variant_args_ret(Variant *base, R (T::*p_method)(P...), const Variant **p_args, Variant *r_ret) {
	call_with_validated_variant_args_ret_helper<T, R, P...>(VariantGetInternalPtr<T>::get_ptr(base), p_method, p_args, r_ret, BuildIndexSequence<sizeof...(P)>{});
}

template <typename T, typename R, typename... P>
void call_with_validated_variant_args_retc(Variant *base, R (T::*p_method)(P...) const, const Variant **p_args, Variant *r_ret) {
	call_with_validated_variant_args_retc_helper<T, R, P...>(VariantGetInternalPtr<T>::get_ptr(base), p_method, p_args, r_ret, BuildIndexSequence<sizeof...(P)>{});
}

template <typename T, typename... P>
void call_with_validated_variant_args_static(Variant *base, void (*p_method)(T *, P...), const Variant **p_args) {
	call_with_validated_variant_args_static_helper<T, P...>(VariantGetInternalPtr<T>::get_ptr(base), p_method, p_args, BuildIndexSequence<sizeof...(P)>{});
}

template <typename T, typename R, typename... P>
void call_with_validated_variant_args_static_retc(Variant *base, R (*p_method)(T *, P...), const Variant **p_args, Variant *r_ret) {
	call_with_validated_variant_args_static_retc_helper<T, R, P...>(VariantGetInternalPtr<T>::get_ptr(base), p_method, p_args, r_ret, BuildIndexSequence<sizeof...(P)>{});
}

template <typename... P>
void call_with_validated_variant_args_static_method(void (*p_method)(P...), const Variant **p_args) {
	call_with_validated_variant_args_static_method_helper<P...>(p_method, p_args, BuildIndexSequence<sizeof...(P)>{});
}

template <typename R, typename... P>
void call_with_validated_variant_args_static_method_ret(R (*p_method)(P...), const Variant **p_args, Variant *r_ret) {
	call_with_validated_variant_args_static_method_ret_helper<R, P...>(p_method, p_args, r_ret, BuildIndexSequence<sizeof...(P)>{});
}

// Validated Object

template <typename T, typename... P>
void call_with_validated_object_instance_args(T *base, void (T::*p_method)(P...), const Variant **p_args) {
	call_with_validated_variant_args_helper<T, P...>(base, p_method, p_args, BuildIndexSequence<sizeof...(P)>{});
}

template <typename T, typename... P>
void call_with_validated_object_instance_argsc(T *base, void (T::*p_method)(P...) const, const Variant **p_args) {
	call_with_validated_variant_argsc_helper<T, P...>(base, p_method, p_args, BuildIndexSequence<sizeof...(P)>{});
}

template <typename T, typename R, typename... P>
void call_with_validated_object_instance_args_ret(T *base, R (T::*p_method)(P...), const Variant **p_args, Variant *r_ret) {
	call_with_validated_variant_args_ret_helper<T, R, P...>(base, p_method, p_args, r_ret, BuildIndexSequence<sizeof...(P)>{});
}

template <typename T, typename R, typename... P>
void call_with_validated_object_instance_args_retc(T *base, R (T::*p_method)(P...) const, const Variant **p_args, Variant *r_ret) {
	call_with_validated_variant_args_retc_helper<T, R, P...>(base, p_method, p_args, r_ret, BuildIndexSequence<sizeof...(P)>{});
}

template <typename T, typename... P>
void call_with_validated_object_instance_args_static(T *base, void (*p_method)(T *, P...), const Variant **p_args) {
	call_with_validated_variant_args_static_helper<T, P...>(base, p_method, p_args, BuildIndexSequence<sizeof...(P)>{});
}

template <typename T, typename R, typename... P>
void call_with_validated_object_instance_args_static_retc(T *base, R (*p_method)(T *, P...), const Variant **p_args, Variant *r_ret) {
	call_with_validated_variant_args_static_retc_helper<T, R, P...>(base, p_method, p_args, r_ret, BuildIndexSequence<sizeof...(P)>{});
}

// GCC raises "parameter 'p_args' set but not used" when P = {},
// it's not clever enough to treat other P values as making this branch valid.
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-but-set-parameter"
#endif

template <typename Q>
void call_get_argument_type_helper(int p_arg, int &index, Variant::Type &type) {
	if (p_arg == index) {
		type = GetTypeInfo<Q>::VARIANT_TYPE;
	}
	index++;
}

template <typename... P>
Variant::Type call_get_argument_type(int p_arg) {
	Variant::Type type = Variant::NIL;
	int index = 0;
	// I think rocket science is simpler than modern C++.
	using expand_type = int[];
	expand_type a{ 0, (call_get_argument_type_helper<P>(p_arg, index, type), 0)... };
	(void)a; // Suppress (valid, but unavoidable) -Wunused-variable warning.
	(void)index; // Suppress GCC warning.
	return type;
}

template <typename Q>
void call_get_argument_type_info_helper(int p_arg, int &index, PropertyInfo &info) {
	if (p_arg == index) {
		info = GetTypeInfo<Q>::get_class_info();
	}
	index++;
}

template <typename... P>
void call_get_argument_type_info(int p_arg, PropertyInfo &info) {
	int index = 0;
	// I think rocket science is simpler than modern C++.
	using expand_type = int[];
	expand_type a{ 0, (call_get_argument_type_info_helper<P>(p_arg, index, info), 0)... };
	(void)a; // Suppress (valid, but unavoidable) -Wunused-variable warning.
	(void)index; // Suppress GCC warning.
}

#ifdef DEBUG_METHODS_ENABLED
template <typename Q>
void call_get_argument_metadata_helper(int p_arg, int &index, GodotTypeInfo::Metadata &md) {
	if (p_arg == index) {
		md = GetTypeInfo<Q>::METADATA;
	}
	index++;
}

template <typename... P>
GodotTypeInfo::Metadata call_get_argument_metadata(int p_arg) {
	GodotTypeInfo::Metadata md = GodotTypeInfo::METADATA_NONE;

	int index = 0;
	// I think rocket science is simpler than modern C++.
	using expand_type = int[];
	expand_type a{ 0, (call_get_argument_metadata_helper<P>(p_arg, index, md), 0)... };
	(void)a; // Suppress (valid, but unavoidable) -Wunused-variable warning.
	(void)index;
	return md;
}

#endif // DEBUG_METHODS_ENABLED

//////////////////////

template <typename T, typename R, typename... P, size_t... Is>
void call_with_variant_args_ret_helper(T *p_instance, R (T::*p_method)(P...), const Variant **p_args, Variant &r_ret, Callable::CallError &r_error, IndexSequence<Is...>) {
	r_error.error = Callable::CallError::CALL_OK;

#ifdef DEBUG_METHODS_ENABLED
	r_ret = (p_instance->*p_method)(VariantCasterAndValidate<P>::cast(p_args, Is, r_error)...);
#else
	r_ret = (p_instance->*p_method)(VariantCaster<P>::cast(*p_args[Is])...);
#endif
}

template <typename R, typename... P, size_t... Is>
void call_with_variant_args_static_ret(R (*p_method)(P...), const Variant **p_args, Variant &r_ret, Callable::CallError &r_error, IndexSequence<Is...>) {
	r_error.error = Callable::CallError::CALL_OK;

#ifdef DEBUG_METHODS_ENABLED
	r_ret = (p_method)(VariantCasterAndValidate<P>::cast(p_args, Is, r_error)...);
#else
	r_ret = (p_method)(VariantCaster<P>::cast(*p_args[Is])...);
#endif
}

template <typename... P, size_t... Is>
void call_with_variant_args_static(void (*p_method)(P...), const Variant **p_args, Callable::CallError &r_error, IndexSequence<Is...>) {
	r_error.error = Callable::CallError::CALL_OK;

#ifdef DEBUG_METHODS_ENABLED
	(p_method)(VariantCasterAndValidate<P>::cast(p_args, Is, r_error)...);
#else
	(p_method)(VariantCaster<P>::cast(*p_args[Is])...);
#endif
}

template <typename T, typename R, typename... P>
void call_with_variant_args_ret(T *p_instance, R (T::*p_method)(P...), const Variant **p_args, int p_argcount, Variant &r_ret, Callable::CallError &r_error) {
#ifdef DEBUG_METHODS_ENABLED
	if ((size_t)p_argcount > sizeof...(P)) {
		r_error.error = Callable::CallError::CALL_ERROR_TOO_MANY_ARGUMENTS;
		r_error.expected = sizeof...(P);
		return;
	}

	if ((size_t)p_argcount < sizeof...(P)) {
		r_error.error = Callable::CallError::CALL_ERROR_TOO_FEW_ARGUMENTS;
		r_error.expected = sizeof...(P);
		return;
	}
#endif
	call_with_variant_args_ret_helper<T, R, P...>(p_instance, p_method, p_args, r_ret, r_error, BuildIndexSequence<sizeof...(P)>{});
}

template <typename T, typename R, typename... P, size_t... Is>
void call_with_variant_args_retc_helper(T *p_instance, R (T::*p_method)(P...) const, const Variant **p_args, Variant &r_ret, Callable::CallError &r_error, IndexSequence<Is...>) {
	r_error.error = Callable::CallError::CALL_OK;

#ifdef DEBUG_METHODS_ENABLED
	r_ret = (p_instance->*p_method)(VariantCasterAndValidate<P>::cast(p_args, Is, r_error)...);
#else
	r_ret = (p_instance->*p_method)(VariantCaster<P>::cast(*p_args[Is])...);
#endif
	(void)p_args;
}

template <typename R, typename... P>
void call_with_variant_args_static_ret(R (*p_method)(P...), const Variant **p_args, int p_argcount, Variant &r_ret, Callable::CallError &r_error) {
#ifdef DEBUG_METHODS_ENABLED
	if ((size_t)p_argcount > sizeof...(P)) {
		r_error.error = Callable::CallError::CALL_ERROR_TOO_MANY_ARGUMENTS;
		r_error.expected = sizeof...(P);
		return;
	}

	if ((size_t)p_argcount < sizeof...(P)) {
		r_error.error = Callable::CallError::CALL_ERROR_TOO_FEW_ARGUMENTS;
		r_error.expected = sizeof...(P);
		return;
	}
#endif
	call_with_variant_args_static_ret<R, P...>(p_method, p_args, r_ret, r_error, BuildIndexSequence<sizeof...(P)>{});
}

template <typename... P>
void call_with_variant_args_static(void (*p_method)(P...), const Variant **p_args, int p_argcount, Callable::CallError &r_error) {
#ifdef DEBUG_METHODS_ENABLED
	if ((size_t)p_argcount > sizeof...(P)) {
		r_error.error = Callable::CallError::CALL_ERROR_TOO_MANY_ARGUMENTS;
		r_error.expected = sizeof...(P);
		return;
	}

	if ((size_t)p_argcount < sizeof...(P)) {
		r_error.error = Callable::CallError::CALL_ERROR_TOO_FEW_ARGUMENTS;
		r_error.expected = sizeof...(P);
		return;
	}
#endif
	call_with_variant_args_static<P...>(p_method, p_args, r_error, BuildIndexSequence<sizeof...(P)>{});
}

template <typename T, typename R, typename... P>
void call_with_variant_args_retc(T *p_instance, R (T::*p_method)(P...) const, const Variant **p_args, int p_argcount, Variant &r_ret, Callable::CallError &r_error) {
#ifdef DEBUG_METHODS_ENABLED
	if ((size_t)p_argcount > sizeof...(P)) {
		r_error.error = Callable::CallError::CALL_ERROR_TOO_MANY_ARGUMENTS;
		r_error.expected = sizeof...(P);
		return;
	}

	if ((size_t)p_argcount < sizeof...(P)) {
		r_error.error = Callable::CallError::CALL_ERROR_TOO_FEW_ARGUMENTS;
		r_error.expected = sizeof...(P);
		return;
	}
#endif
	call_with_variant_args_retc_helper<T, R, P...>(p_instance, p_method, p_args, r_ret, r_error, BuildIndexSequence<sizeof...(P)>{});
}

template <typename T, typename R, typename... P, size_t... Is>
void call_with_variant_args_retc_static_helper(T *p_instance, R (*p_method)(T *, P...), const Variant **p_args, Variant &r_ret, Callable::CallError &r_error, IndexSequence<Is...>) {
	r_error.error = Callable::CallError::CALL_OK;

#ifdef DEBUG_METHODS_ENABLED
	r_ret = (p_method)(p_instance, VariantCasterAndValidate<P>::cast(p_args, Is, r_error)...);
#else
	r_ret = (p_method)(p_instance, VariantCaster<P>::cast(*p_args[Is])...);
#endif

	(void)p_args;
}

template <typename T, typename R, typename... P>
void call_with_variant_args_retc_static_helper_dv(T *p_instance, R (*p_method)(T *, P...), const Variant **p_args, int p_argcount, Variant &r_ret, const Vector<Variant> &default_values, Callable::CallError &r_error) {
#ifdef DEBUG_ENABLED
	if ((size_t)p_argcount > sizeof...(P)) {
		r_error.error = Callable::CallError::CALL_ERROR_TOO_MANY_ARGUMENTS;
		r_error.expected = sizeof...(P);
		return;
	}
#endif

	int32_t missing = (int32_t)sizeof...(P) - (int32_t)p_argcount;

	int32_t dvs = default_values.size();
#ifdef DEBUG_ENABLED
	if (missing > dvs) {
		r_error.error = Callable::CallError::CALL_ERROR_TOO_FEW_ARGUMENTS;
		r_error.expected = sizeof...(P);
		return;
	}
#endif

	const Variant *args[sizeof...(P) == 0 ? 1 : sizeof...(P)]; //avoid zero sized array
	for (int32_t i = 0; i < (int32_t)sizeof...(P); i++) {
		if (i < p_argcount) {
			args[i] = p_args[i];
		} else {
			args[i] = &default_values[i - p_argcount + (dvs - missing)];
		}
	}

	call_with_variant_args_retc_static_helper(p_instance, p_method, args, r_ret, r_error, BuildIndexSequence<sizeof...(P)>{});
}

template <typename T, typename... P, size_t... Is>
void call_with_variant_args_static_helper(T *p_instance, void (*p_method)(T *, P...), const Variant **p_args, Callable::CallError &r_error, IndexSequence<Is...>) {
	r_error.error = Callable::CallError::CALL_OK;

#ifdef DEBUG_METHODS_ENABLED
	(p_method)(p_instance, VariantCasterAndValidate<P>::cast(p_args, Is, r_error)...);
#else
	(p_method)(p_instance, VariantCaster<P>::cast(*p_args[Is])...);
#endif

	(void)p_args;
}

template <typename T, typename... P>
void call_with_variant_args_static_helper_dv(T *p_instance, void (*p_method)(T *, P...), const Variant **p_args, int p_argcount, const Vector<Variant> &default_values, Callable::CallError &r_error) {
#ifdef DEBUG_ENABLED
	if ((size_t)p_argcount > sizeof...(P)) {
		r_error.error = Callable::CallError::CALL_ERROR_TOO_MANY_ARGUMENTS;
		r_error.expected = sizeof...(P);
		return;
	}
#endif

	int32_t missing = (int32_t)sizeof...(P) - (int32_t)p_argcount;

	int32_t dvs = default_values.size();
#ifdef DEBUG_ENABLED
	if (missing > dvs) {
		r_error.error = Callable::CallError::CALL_ERROR_TOO_FEW_ARGUMENTS;
		r_error.expected = sizeof...(P);
		return;
	}
#endif

	const Variant *args[sizeof...(P) == 0 ? 1 : sizeof...(P)]; //avoid zero sized array
	for (int32_t i = 0; i < (int32_t)sizeof...(P); i++) {
		if (i < p_argcount) {
			args[i] = p_args[i];
		} else {
			args[i] = &default_values[i - p_argcount + (dvs - missing)];
		}
	}

	call_with_variant_args_static_helper(p_instance, p_method, args, r_error, BuildIndexSequence<sizeof...(P)>{});
}

template <typename R, typename... P>
void call_with_variant_args_static_ret_dv(R (*p_method)(P...), const Variant **p_args, int p_argcount, Variant &r_ret, Callable::CallError &r_error, const Vector<Variant> &default_values) {
#ifdef DEBUG_ENABLED
	if ((size_t)p_argcount > sizeof...(P)) {
		r_error.error = Callable::CallError::CALL_ERROR_TOO_MANY_ARGUMENTS;
		r_error.expected = sizeof...(P);
		return;
	}
#endif

	int32_t missing = (int32_t)sizeof...(P) - (int32_t)p_argcount;

	int32_t dvs = default_values.size();
#ifdef DEBUG_ENABLED
	if (missing > dvs) {
		r_error.error = Callable::CallError::CALL_ERROR_TOO_FEW_ARGUMENTS;
		r_error.expected = sizeof...(P);
		return;
	}
#endif

	const Variant *args[sizeof...(P) == 0 ? 1 : sizeof...(P)]; //avoid zero sized array
	for (int32_t i = 0; i < (int32_t)sizeof...(P); i++) {
		if (i < p_argcount) {
			args[i] = p_args[i];
		} else {
			args[i] = &default_values[i - p_argcount + (dvs - missing)];
		}
	}

	call_with_variant_args_static_ret(p_method, args, r_ret, r_error, BuildIndexSequence<sizeof...(P)>{});
}

template <typename... P>
void call_with_variant_args_static_dv(void (*p_method)(P...), const Variant **p_args, int p_argcount, Callable::CallError &r_error, const Vector<Variant> &default_values) {
#ifdef DEBUG_ENABLED
	if ((size_t)p_argcount > sizeof...(P)) {
		r_error.error = Callable::CallError::CALL_ERROR_TOO_MANY_ARGUMENTS;
		r_error.expected = sizeof...(P);
		return;
	}
#endif

	int32_t missing = (int32_t)sizeof...(P) - (int32_t)p_argcount;

	int32_t dvs = default_values.size();
#ifdef DEBUG_ENABLED
	if (missing > dvs) {
		r_error.error = Callable::CallError::CALL_ERROR_TOO_FEW_ARGUMENTS;
		r_error.expected = sizeof...(P);
		return;
	}
#endif

	const Variant *args[sizeof...(P) == 0 ? 1 : sizeof...(P)]; //avoid zero sized array
	for (int32_t i = 0; i < (int32_t)sizeof...(P); i++) {
		if (i < p_argcount) {
			args[i] = p_args[i];
		} else {
			args[i] = &default_values[i - p_argcount + (dvs - missing)];
		}
	}

	call_with_variant_args_static(p_method, args, r_error, BuildIndexSequence<sizeof...(P)>{});
}

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif

#endif // BINDER_COMMON_H

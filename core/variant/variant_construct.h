/**************************************************************************/
/*  variant_construct.h                                                   */
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

#include "variant.h"

#include "core/crypto/crypto_core.h"
#include "core/debugger/engine_debugger.h"
#include "core/io/compression.h"
#include "core/object/class_db.h"
#include "core/os/os.h"
#include "core/templates/local_vector.h"
#include "core/templates/oa_hash_map.h"

template <typename T>
struct PtrConstruct {};

#define MAKE_PTRCONSTRUCT(m_type)                                                  \
	template <>                                                                    \
	struct PtrConstruct<m_type> {                                                  \
		_FORCE_INLINE_ static void construct(const m_type &p_value, void *p_ptr) { \
			memnew_placement(p_ptr, m_type(p_value));                              \
		}                                                                          \
	};

MAKE_PTRCONSTRUCT(bool);
MAKE_PTRCONSTRUCT(int64_t);
MAKE_PTRCONSTRUCT(double);
MAKE_PTRCONSTRUCT(String);
MAKE_PTRCONSTRUCT(Vector2);
MAKE_PTRCONSTRUCT(Vector2i);
MAKE_PTRCONSTRUCT(Rect2);
MAKE_PTRCONSTRUCT(Rect2i);
MAKE_PTRCONSTRUCT(Vector3);
MAKE_PTRCONSTRUCT(Vector3i);
MAKE_PTRCONSTRUCT(Vector4);
MAKE_PTRCONSTRUCT(Vector4i);
MAKE_PTRCONSTRUCT(Transform2D);
MAKE_PTRCONSTRUCT(Plane);
MAKE_PTRCONSTRUCT(Quaternion);
MAKE_PTRCONSTRUCT(AABB);
MAKE_PTRCONSTRUCT(Basis);
MAKE_PTRCONSTRUCT(Transform3D);
MAKE_PTRCONSTRUCT(Projection);
MAKE_PTRCONSTRUCT(Color);
MAKE_PTRCONSTRUCT(StringName);
MAKE_PTRCONSTRUCT(NodePath);
MAKE_PTRCONSTRUCT(RID);

template <>
struct PtrConstruct<Object *> {
	_FORCE_INLINE_ static void construct(Object *p_value, void *p_ptr) {
		*((Object **)p_ptr) = p_value;
	}
};

MAKE_PTRCONSTRUCT(Callable);
MAKE_PTRCONSTRUCT(Signal);
MAKE_PTRCONSTRUCT(Dictionary);
MAKE_PTRCONSTRUCT(Array);
MAKE_PTRCONSTRUCT(PackedByteArray);
MAKE_PTRCONSTRUCT(PackedInt32Array);
MAKE_PTRCONSTRUCT(PackedInt64Array);
MAKE_PTRCONSTRUCT(PackedFloat32Array);
MAKE_PTRCONSTRUCT(PackedFloat64Array);
MAKE_PTRCONSTRUCT(PackedStringArray);
MAKE_PTRCONSTRUCT(PackedVector2Array);
MAKE_PTRCONSTRUCT(PackedVector3Array);
MAKE_PTRCONSTRUCT(PackedColorArray);
MAKE_PTRCONSTRUCT(PackedVector4Array);
MAKE_PTRCONSTRUCT(Variant);

template <typename T, typename... P>
class VariantConstructor {
	template <size_t... Is>
	static _FORCE_INLINE_ void construct_helper(T &base, const Variant **p_args, Callable::CallError &r_error, IndexSequence<Is...>) {
		r_error.error = Callable::CallError::CALL_OK;

#ifdef DEBUG_METHODS_ENABLED
		base = T(VariantCasterAndValidate<P>::cast(p_args, Is, r_error)...);
#else
		base = T(VariantCaster<P>::cast(*p_args[Is])...);
#endif
	}

	template <size_t... Is>
	static _FORCE_INLINE_ void validated_construct_helper(T &base, const Variant **p_args, IndexSequence<Is...>) {
		base = T((*VariantGetInternalPtr<P>::get_ptr(p_args[Is]))...);
	}

	template <size_t... Is>
	static _FORCE_INLINE_ void ptr_construct_helper(void *base, const void **p_args, IndexSequence<Is...>) {
		PtrConstruct<T>::construct(T(PtrToArg<P>::convert(p_args[Is])...), base);
	}

public:
	static void construct(Variant &r_ret, const Variant **p_args, Callable::CallError &r_error) {
		r_error.error = Callable::CallError::CALL_OK;
		VariantTypeChanger<T>::change(&r_ret);
		construct_helper(*VariantGetInternalPtr<T>::get_ptr(&r_ret), p_args, r_error, BuildIndexSequence<sizeof...(P)>{});
	}

	static inline void validated_construct(Variant *r_ret, const Variant **p_args) {
		VariantTypeChanger<T>::change(r_ret);
		validated_construct_helper(*VariantGetInternalPtr<T>::get_ptr(r_ret), p_args, BuildIndexSequence<sizeof...(P)>{});
	}
	static void ptr_construct(void *base, const void **p_args) {
		ptr_construct_helper(base, p_args, BuildIndexSequence<sizeof...(P)>{});
	}

	static int get_argument_count() {
		return sizeof...(P);
	}

	static Variant::Type get_argument_type(int p_arg) {
		return call_get_argument_type<P...>(p_arg);
	}

	static Variant::Type get_base_type() {
		return GetTypeInfo<T>::VARIANT_TYPE;
	}
};

class VariantConstructorObject {
public:
	static void construct(Variant &r_ret, const Variant **p_args, Callable::CallError &r_error) {
		if (p_args[0]->get_type() == Variant::NIL) {
			VariantInternal::clear(&r_ret);
			VariantTypeChanger<Object *>::change(&r_ret);
			VariantInternal::object_reset_data(&r_ret);
			r_error.error = Callable::CallError::CALL_OK;
		} else if (p_args[0]->get_type() == Variant::OBJECT) {
			VariantTypeChanger<Object *>::change(&r_ret);
			VariantInternal::object_assign(&r_ret, p_args[0]);
			r_error.error = Callable::CallError::CALL_OK;
		} else {
			VariantInternal::clear(&r_ret);
			r_error.error = Callable::CallError::CALL_ERROR_INVALID_ARGUMENT;
			r_error.argument = 0;
			r_error.expected = Variant::OBJECT;
		}
	}

	static inline void validated_construct(Variant *r_ret, const Variant **p_args) {
		VariantTypeChanger<Object *>::change(r_ret);
		VariantInternal::object_assign(r_ret, p_args[0]);
	}
	static void ptr_construct(void *base, const void **p_args) {
		PtrConstruct<Object *>::construct(PtrToArg<Object *>::convert(p_args[0]), base);
	}

	static int get_argument_count() {
		return 1;
	}

	static Variant::Type get_argument_type(int p_arg) {
		return Variant::OBJECT;
	}

	static Variant::Type get_base_type() {
		return Variant::OBJECT;
	}
};

class VariantConstructorNilObject {
public:
	static void construct(Variant &r_ret, const Variant **p_args, Callable::CallError &r_error) {
		if (p_args[0]->get_type() != Variant::NIL) {
			r_error.error = Callable::CallError::CALL_ERROR_INVALID_ARGUMENT;
			r_error.argument = 0;
			r_error.expected = Variant::NIL;
		}

		VariantInternal::clear(&r_ret);
		VariantTypeChanger<Object *>::change(&r_ret);
		VariantInternal::object_reset_data(&r_ret);
	}

	static inline void validated_construct(Variant *r_ret, const Variant **p_args) {
		VariantInternal::clear(r_ret);
		VariantTypeChanger<Object *>::change(r_ret);
		VariantInternal::object_reset_data(r_ret);
	}
	static void ptr_construct(void *base, const void **p_args) {
		PtrConstruct<Object *>::construct(nullptr, base);
	}

	static int get_argument_count() {
		return 1;
	}

	static Variant::Type get_argument_type(int p_arg) {
		return Variant::NIL;
	}

	static Variant::Type get_base_type() {
		return Variant::OBJECT;
	}
};

template <typename T>
class VariantConstructorFromString {
public:
	static void construct(Variant &r_ret, const Variant **p_args, Callable::CallError &r_error) {
		if (!p_args[0]->is_string()) {
			r_error.error = Callable::CallError::CALL_ERROR_INVALID_ARGUMENT;
			r_error.argument = 0;
			r_error.expected = Variant::STRING;
			return;
		}

		VariantTypeChanger<T>::change(&r_ret);
		const String src_str = *p_args[0];

		if (r_ret.get_type() == Variant::Type::INT) {
			r_ret = src_str.to_int();
		} else if (r_ret.get_type() == Variant::Type::FLOAT) {
			r_ret = src_str.to_float();
		}
	}

	static inline void validated_construct(Variant *r_ret, const Variant **p_args) {
		VariantTypeChanger<T>::change(r_ret);
		const String &src_str = *VariantGetInternalPtr<String>::get_ptr(p_args[0]);
		T ret = Variant();
		if (r_ret->get_type() == Variant::Type::INT) {
			ret = src_str.to_int();
		} else if (r_ret->get_type() == Variant::Type::FLOAT) {
			ret = src_str.to_float();
		}
		*r_ret = ret;
	}

	static void ptr_construct(void *base, const void **p_args) {
		String src_str = PtrToArg<String>::convert(p_args[0]);
		T dst_var = Variant();
		Variant type_test = Variant(dst_var);
		if (type_test.get_type() == Variant::Type::INT) {
			dst_var = src_str.to_int();
		} else if (type_test.get_type() == Variant::Type::FLOAT) {
			dst_var = src_str.to_float();
		}
		PtrConstruct<T>::construct(dst_var, base);
	}

	static int get_argument_count() {
		return 1;
	}

	static Variant::Type get_argument_type(int p_arg) {
		return Variant::STRING;
	}

	static Variant::Type get_base_type() {
		return GetTypeInfo<T>::VARIANT_TYPE;
	}
};

class VariantConstructorCallableArgs {
public:
	static void construct(Variant &r_ret, const Variant **p_args, Callable::CallError &r_error) {
		ObjectID object_id;
		StringName method;

		if (p_args[0]->get_type() == Variant::NIL) {
			// leave as is
		} else if (p_args[0]->get_type() == Variant::OBJECT) {
			object_id = VariantInternal::get_object_id(p_args[0]);
		} else {
			r_error.error = Callable::CallError::CALL_ERROR_INVALID_ARGUMENT;
			r_error.argument = 0;
			r_error.expected = Variant::OBJECT;
			return;
		}

		if (p_args[1]->get_type() == Variant::STRING_NAME) {
			method = *VariantGetInternalPtr<StringName>::get_ptr(p_args[1]);
		} else if (p_args[1]->get_type() == Variant::STRING) {
			method = *VariantGetInternalPtr<String>::get_ptr(p_args[1]);
		} else {
			r_error.error = Callable::CallError::CALL_ERROR_INVALID_ARGUMENT;
			r_error.argument = 1;
			r_error.expected = Variant::STRING_NAME;
			return;
		}

		VariantTypeChanger<Callable>::change(&r_ret);
		*VariantGetInternalPtr<Callable>::get_ptr(&r_ret) = Callable(object_id, method);
	}

	static inline void validated_construct(Variant *r_ret, const Variant **p_args) {
		VariantTypeChanger<Callable>::change(r_ret);
		*VariantGetInternalPtr<Callable>::get_ptr(r_ret) = Callable(VariantInternal::get_object_id(p_args[0]), *VariantGetInternalPtr<StringName>::get_ptr(p_args[1]));
	}
	static void ptr_construct(void *base, const void **p_args) {
		PtrConstruct<Callable>::construct(Callable(PtrToArg<Object *>::convert(p_args[0]), PtrToArg<StringName>::convert(p_args[1])), base);
	}

	static int get_argument_count() {
		return 2;
	}

	static Variant::Type get_argument_type(int p_arg) {
		if (p_arg == 0) {
			return Variant::OBJECT;
		} else {
			return Variant::STRING_NAME;
		}
	}

	static Variant::Type get_base_type() {
		return Variant::CALLABLE;
	}
};

class VariantConstructorSignalArgs {
public:
	static void construct(Variant &r_ret, const Variant **p_args, Callable::CallError &r_error) {
		ObjectID object_id;
		StringName method;

		if (p_args[0]->get_type() == Variant::NIL) {
			// leave as is
		} else if (p_args[0]->get_type() == Variant::OBJECT) {
			object_id = VariantInternal::get_object_id(p_args[0]);
		} else {
			r_error.error = Callable::CallError::CALL_ERROR_INVALID_ARGUMENT;
			r_error.argument = 0;
			r_error.expected = Variant::OBJECT;
			return;
		}

		if (p_args[1]->get_type() == Variant::STRING_NAME) {
			method = *VariantGetInternalPtr<StringName>::get_ptr(p_args[1]);
		} else if (p_args[1]->get_type() == Variant::STRING) {
			method = *VariantGetInternalPtr<String>::get_ptr(p_args[1]);
		} else {
			r_error.error = Callable::CallError::CALL_ERROR_INVALID_ARGUMENT;
			r_error.argument = 1;
			r_error.expected = Variant::STRING_NAME;
			return;
		}

		VariantTypeChanger<Signal>::change(&r_ret);
		*VariantGetInternalPtr<Signal>::get_ptr(&r_ret) = Signal(object_id, method);
	}

	static inline void validated_construct(Variant *r_ret, const Variant **p_args) {
		VariantTypeChanger<Signal>::change(r_ret);
		*VariantGetInternalPtr<Signal>::get_ptr(r_ret) = Signal(VariantInternal::get_object_id(p_args[0]), *VariantGetInternalPtr<StringName>::get_ptr(p_args[1]));
	}
	static void ptr_construct(void *base, const void **p_args) {
		PtrConstruct<Signal>::construct(Signal(PtrToArg<Object *>::convert(p_args[0]), PtrToArg<StringName>::convert(p_args[1])), base);
	}

	static int get_argument_count() {
		return 2;
	}

	static Variant::Type get_argument_type(int p_arg) {
		if (p_arg == 0) {
			return Variant::OBJECT;
		} else {
			return Variant::STRING_NAME;
		}
	}

	static Variant::Type get_base_type() {
		return Variant::SIGNAL;
	}
};

class VariantConstructorTypedDictionary {
public:
	static void construct(Variant &r_ret, const Variant **p_args, Callable::CallError &r_error) {
		if (p_args[0]->get_type() != Variant::DICTIONARY) {
			r_error.error = Callable::CallError::CALL_ERROR_INVALID_ARGUMENT;
			r_error.argument = 0;
			r_error.expected = Variant::DICTIONARY;
			return;
		}

		if (p_args[1]->get_type() != Variant::INT) {
			r_error.error = Callable::CallError::CALL_ERROR_INVALID_ARGUMENT;
			r_error.argument = 1;
			r_error.expected = Variant::INT;
			return;
		}

		if (p_args[2]->get_type() != Variant::STRING_NAME) {
			r_error.error = Callable::CallError::CALL_ERROR_INVALID_ARGUMENT;
			r_error.argument = 2;
			r_error.expected = Variant::STRING_NAME;
			return;
		}

		if (p_args[4]->get_type() != Variant::INT) {
			r_error.error = Callable::CallError::CALL_ERROR_INVALID_ARGUMENT;
			r_error.argument = 4;
			r_error.expected = Variant::INT;
			return;
		}

		if (p_args[5]->get_type() != Variant::STRING_NAME) {
			r_error.error = Callable::CallError::CALL_ERROR_INVALID_ARGUMENT;
			r_error.argument = 5;
			r_error.expected = Variant::STRING_NAME;
			return;
		}

		const Dictionary &base_dict = *VariantGetInternalPtr<Dictionary>::get_ptr(p_args[0]);
		const uint32_t key_type = p_args[1]->operator uint32_t();
		const StringName &key_class_name = *VariantGetInternalPtr<StringName>::get_ptr(p_args[2]);
		const uint32_t value_type = p_args[4]->operator uint32_t();
		const StringName &value_class_name = *VariantGetInternalPtr<StringName>::get_ptr(p_args[5]);
		r_ret = Dictionary(base_dict, key_type, key_class_name, *p_args[3], value_type, value_class_name, *p_args[6]);
	}

	static inline void validated_construct(Variant *r_ret, const Variant **p_args) {
		const Dictionary &base_dict = *VariantGetInternalPtr<Dictionary>::get_ptr(p_args[0]);
		const uint32_t key_type = p_args[1]->operator uint32_t();
		const StringName &key_class_name = *VariantGetInternalPtr<StringName>::get_ptr(p_args[2]);
		const uint32_t value_type = p_args[4]->operator uint32_t();
		const StringName &value_class_name = *VariantGetInternalPtr<StringName>::get_ptr(p_args[5]);
		*r_ret = Dictionary(base_dict, key_type, key_class_name, *p_args[3], value_type, value_class_name, *p_args[6]);
	}

	static void ptr_construct(void *base, const void **p_args) {
		const Dictionary &base_dict = PtrToArg<Dictionary>::convert(p_args[0]);
		const uint32_t key_type = PtrToArg<uint32_t>::convert(p_args[1]);
		const StringName &key_class_name = PtrToArg<StringName>::convert(p_args[2]);
		const Variant &key_script = PtrToArg<Variant>::convert(p_args[3]);
		const uint32_t value_type = PtrToArg<uint32_t>::convert(p_args[4]);
		const StringName &value_class_name = PtrToArg<StringName>::convert(p_args[5]);
		const Variant &value_script = PtrToArg<Variant>::convert(p_args[6]);
		Dictionary dst_arr = Dictionary(base_dict, key_type, key_class_name, key_script, value_type, value_class_name, value_script);

		PtrConstruct<Dictionary>::construct(dst_arr, base);
	}

	static int get_argument_count() {
		return 7;
	}

	static Variant::Type get_argument_type(int p_arg) {
		switch (p_arg) {
			case 0: {
				return Variant::DICTIONARY;
			} break;
			case 1: {
				return Variant::INT;
			} break;
			case 2: {
				return Variant::STRING_NAME;
			} break;
			case 3: {
				return Variant::NIL;
			} break;
			case 4: {
				return Variant::INT;
			} break;
			case 5: {
				return Variant::STRING_NAME;
			} break;
			case 6: {
				return Variant::NIL;
			} break;
			default: {
				return Variant::NIL;
			} break;
		}
	}

	static Variant::Type get_base_type() {
		return Variant::DICTIONARY;
	}
};

class VariantConstructorTypedArray {
public:
	static void construct(Variant &r_ret, const Variant **p_args, Callable::CallError &r_error) {
		if (p_args[0]->get_type() != Variant::ARRAY) {
			r_error.error = Callable::CallError::CALL_ERROR_INVALID_ARGUMENT;
			r_error.argument = 0;
			r_error.expected = Variant::ARRAY;
			return;
		}

		if (p_args[1]->get_type() != Variant::INT) {
			r_error.error = Callable::CallError::CALL_ERROR_INVALID_ARGUMENT;
			r_error.argument = 1;
			r_error.expected = Variant::INT;
			return;
		}

		if (!p_args[2]->is_string()) {
			r_error.error = Callable::CallError::CALL_ERROR_INVALID_ARGUMENT;
			r_error.argument = 2;
			r_error.expected = Variant::STRING_NAME;
			return;
		}

		const Array &base_arr = *VariantGetInternalPtr<Array>::get_ptr(p_args[0]);
		const uint32_t type = p_args[1]->operator uint32_t();
		r_ret = Array(base_arr, type, *p_args[2], *p_args[3]);
	}

	static inline void validated_construct(Variant *r_ret, const Variant **p_args) {
		const Array &base_arr = *VariantGetInternalPtr<Array>::get_ptr(p_args[0]);
		const uint32_t type = p_args[1]->operator uint32_t();
		const StringName &class_name = *VariantGetInternalPtr<StringName>::get_ptr(p_args[2]);
		*r_ret = Array(base_arr, type, class_name, *p_args[3]);
	}

	static void ptr_construct(void *base, const void **p_args) {
		const Array &base_arr = PtrToArg<Array>::convert(p_args[0]);
		const uint32_t type = PtrToArg<uint32_t>::convert(p_args[1]);
		const StringName &class_name = PtrToArg<StringName>::convert(p_args[2]);
		const Variant &script = PtrToArg<Variant>::convert(p_args[3]);
		Array dst_arr = Array(base_arr, type, class_name, script);

		PtrConstruct<Array>::construct(dst_arr, base);
	}

	static int get_argument_count() {
		return 4;
	}

	static Variant::Type get_argument_type(int p_arg) {
		switch (p_arg) {
			case 0: {
				return Variant::ARRAY;
			} break;
			case 1: {
				return Variant::INT;
			} break;
			case 2: {
				return Variant::STRING_NAME;
			} break;
			case 3: {
				return Variant::NIL;
			} break;
			default: {
				return Variant::NIL;
			} break;
		}
	}

	static Variant::Type get_base_type() {
		return Variant::ARRAY;
	}
};

template <typename T>
class VariantConstructorToArray {
public:
	static void construct(Variant &r_ret, const Variant **p_args, Callable::CallError &r_error) {
		if (p_args[0]->get_type() != GetTypeInfo<T>::VARIANT_TYPE) {
			r_error.error = Callable::CallError::CALL_ERROR_INVALID_ARGUMENT;
			r_error.argument = 0;
			r_error.expected = GetTypeInfo<T>::VARIANT_TYPE;
			return;
		}

		r_ret = Array();
		Array &dst_arr = *VariantGetInternalPtr<Array>::get_ptr(&r_ret);
		const T &src_arr = *VariantGetInternalPtr<T>::get_ptr(p_args[0]);

		int size = src_arr.size();
		dst_arr.resize(size);
		for (int i = 0; i < size; i++) {
			dst_arr[i] = src_arr[i];
		}
	}

	static inline void validated_construct(Variant *r_ret, const Variant **p_args) {
		*r_ret = Array();
		Array &dst_arr = *VariantGetInternalPtr<Array>::get_ptr(r_ret);
		const T &src_arr = *VariantGetInternalPtr<T>::get_ptr(p_args[0]);

		int size = src_arr.size();
		dst_arr.resize(size);
		for (int i = 0; i < size; i++) {
			dst_arr[i] = src_arr[i];
		}
	}
	static void ptr_construct(void *base, const void **p_args) {
		Array dst_arr;
		T src_arr = PtrToArg<T>::convert(p_args[0]);

		int size = src_arr.size();
		dst_arr.resize(size);
		for (int i = 0; i < size; i++) {
			dst_arr[i] = src_arr[i];
		}

		PtrConstruct<Array>::construct(dst_arr, base);
	}

	static int get_argument_count() {
		return 1;
	}

	static Variant::Type get_argument_type(int p_arg) {
		return GetTypeInfo<T>::VARIANT_TYPE;
	}

	static Variant::Type get_base_type() {
		return Variant::ARRAY;
	}
};

template <typename T>
class VariantConstructorFromArray {
public:
	static void construct(Variant &r_ret, const Variant **p_args, Callable::CallError &r_error) {
		if (p_args[0]->get_type() != Variant::ARRAY) {
			r_error.error = Callable::CallError::CALL_ERROR_INVALID_ARGUMENT;
			r_error.argument = 0;
			r_error.expected = Variant::ARRAY;
			return;
		}

		VariantTypeChanger<T>::change(&r_ret);
		const Array &src_arr = *VariantGetInternalPtr<Array>::get_ptr(p_args[0]);
		T &dst_arr = *VariantGetInternalPtr<T>::get_ptr(&r_ret);

		int size = src_arr.size();
		dst_arr.resize(size);
		for (int i = 0; i < size; i++) {
			dst_arr.write[i] = src_arr[i];
		}
	}

	static inline void validated_construct(Variant *r_ret, const Variant **p_args) {
		VariantTypeChanger<T>::change(r_ret);
		const Array &src_arr = *VariantGetInternalPtr<Array>::get_ptr(p_args[0]);
		T &dst_arr = *VariantGetInternalPtr<T>::get_ptr(r_ret);

		int size = src_arr.size();
		dst_arr.resize(size);
		for (int i = 0; i < size; i++) {
			dst_arr.write[i] = src_arr[i];
		}
	}
	static void ptr_construct(void *base, const void **p_args) {
		Array src_arr = PtrToArg<Array>::convert(p_args[0]);
		T dst_arr;

		int size = src_arr.size();
		dst_arr.resize(size);
		for (int i = 0; i < size; i++) {
			dst_arr.write[i] = src_arr[i];
		}

		PtrConstruct<T>::construct(dst_arr, base);
	}

	static int get_argument_count() {
		return 1;
	}

	static Variant::Type get_argument_type(int p_arg) {
		return Variant::ARRAY;
	}

	static Variant::Type get_base_type() {
		return GetTypeInfo<T>::VARIANT_TYPE;
	}
};

class VariantConstructorNil {
public:
	static void construct(Variant &r_ret, const Variant **p_args, Callable::CallError &r_error) {
		if (p_args[0]->get_type() != Variant::NIL) {
			r_error.error = Callable::CallError::CALL_ERROR_INVALID_ARGUMENT;
			r_error.argument = 0;
			r_error.expected = Variant::NIL;
			return;
		}

		r_error.error = Callable::CallError::CALL_OK;
		VariantInternal::clear(&r_ret);
	}

	static inline void validated_construct(Variant *r_ret, const Variant **p_args) {
		VariantInternal::clear(r_ret);
	}
	static void ptr_construct(void *base, const void **p_args) {
		PtrConstruct<Variant>::construct(Variant(), base);
	}

	static int get_argument_count() {
		return 1;
	}

	static Variant::Type get_argument_type(int p_arg) {
		return Variant::NIL;
	}

	static Variant::Type get_base_type() {
		return Variant::NIL;
	}
};

template <typename T>
class VariantConstructNoArgs {
public:
	static void construct(Variant &r_ret, const Variant **p_args, Callable::CallError &r_error) {
		VariantTypeChanger<T>::change_and_reset(&r_ret);
		r_error.error = Callable::CallError::CALL_OK;
	}

	static inline void validated_construct(Variant *r_ret, const Variant **p_args) {
		VariantTypeChanger<T>::change_and_reset(r_ret);
	}
	static void ptr_construct(void *base, const void **p_args) {
		PtrConstruct<T>::construct(T(), base);
	}

	static int get_argument_count() {
		return 0;
	}

	static Variant::Type get_argument_type(int p_arg) {
		return Variant::NIL;
	}

	static Variant::Type get_base_type() {
		return GetTypeInfo<T>::VARIANT_TYPE;
	}
};

class VariantConstructNoArgsNil {
public:
	static void construct(Variant &r_ret, const Variant **p_args, Callable::CallError &r_error) {
		VariantInternal::clear(&r_ret);
		r_error.error = Callable::CallError::CALL_OK;
	}

	static inline void validated_construct(Variant *r_ret, const Variant **p_args) {
		VariantInternal::clear(r_ret);
	}
	static void ptr_construct(void *base, const void **p_args) {
		ERR_FAIL_MSG("Cannot ptrcall nil constructor");
	}

	static int get_argument_count() {
		return 0;
	}

	static Variant::Type get_argument_type(int p_arg) {
		return Variant::NIL;
	}

	static Variant::Type get_base_type() {
		return Variant::NIL;
	}
};

class VariantConstructNoArgsObject {
public:
	static void construct(Variant &r_ret, const Variant **p_args, Callable::CallError &r_error) {
		r_ret = (Object *)nullptr; // Must construct a TYPE_OBJECT containing nullptr.
		r_error.error = Callable::CallError::CALL_OK;
	}

	static inline void validated_construct(Variant *r_ret, const Variant **p_args) {
		*r_ret = (Object *)nullptr; // Must construct a TYPE_OBJECT containing nullptr.
	}
	static void ptr_construct(void *base, const void **p_args) {
		PtrConstruct<Object *>::construct(nullptr, base);
	}

	static int get_argument_count() {
		return 0;
	}

	static Variant::Type get_argument_type(int p_arg) {
		return Variant::NIL;
	}

	static Variant::Type get_base_type() {
		return Variant::OBJECT;
	}
};

/**************************************************************************/
/*  java_class_wrapper.h                                                  */
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

#ifndef JAVA_CLASS_WRAPPER_H
#define JAVA_CLASS_WRAPPER_H

#include "core/object/ref_counted.h"
#include "core/variant/typed_array.h"

#ifdef ANDROID_ENABLED
#include <android/log.h>
#include <jni.h>
#endif

#ifdef ANDROID_ENABLED
class JavaObject;
#endif

class JavaClass : public RefCounted {
	GDCLASS(JavaClass, RefCounted);

#ifdef ANDROID_ENABLED
	enum ArgumentType {
		ARG_TYPE_VOID,
		ARG_TYPE_BOOLEAN,
		ARG_TYPE_BYTE,
		ARG_TYPE_CHAR,
		ARG_TYPE_SHORT,
		ARG_TYPE_INT,
		ARG_TYPE_LONG,
		ARG_TYPE_FLOAT,
		ARG_TYPE_DOUBLE,
		ARG_TYPE_STRING, //special case
		ARG_TYPE_CHARSEQUENCE,
		ARG_TYPE_CALLABLE,
		ARG_TYPE_CLASS,
		ARG_ARRAY_BIT = 1 << 16,
		ARG_NUMBER_CLASS_BIT = 1 << 17,
		ARG_TYPE_MASK = (1 << 16) - 1
	};

	RBMap<StringName, Variant> constant_map;

	struct MethodInfo {
		bool _static = false;
		bool _constructor = false;
		Vector<uint32_t> param_types;
		Vector<StringName> param_sigs;
		uint32_t return_type = 0;
		jmethodID method;
	};

	_FORCE_INLINE_ static void _convert_to_variant_type(int p_sig, Variant::Type &r_type, float &likelihood) {
		likelihood = 1.0;
		r_type = Variant::NIL;

		switch (p_sig) {
			case ARG_TYPE_VOID:
				r_type = Variant::NIL;
				break;
			case ARG_TYPE_BOOLEAN | ARG_NUMBER_CLASS_BIT:
			case ARG_TYPE_BOOLEAN:
				r_type = Variant::BOOL;
				break;
			case ARG_TYPE_BYTE | ARG_NUMBER_CLASS_BIT:
			case ARG_TYPE_BYTE:
				r_type = Variant::INT;
				likelihood = 0.1;
				break;
			case ARG_TYPE_CHAR | ARG_NUMBER_CLASS_BIT:
			case ARG_TYPE_CHAR:
				r_type = Variant::INT;
				likelihood = 0.2;
				break;
			case ARG_TYPE_SHORT | ARG_NUMBER_CLASS_BIT:
			case ARG_TYPE_SHORT:
				r_type = Variant::INT;
				likelihood = 0.3;
				break;
			case ARG_TYPE_INT | ARG_NUMBER_CLASS_BIT:
			case ARG_TYPE_INT:
				r_type = Variant::INT;
				likelihood = 1.0;
				break;
			case ARG_TYPE_LONG | ARG_NUMBER_CLASS_BIT:
			case ARG_TYPE_LONG:
				r_type = Variant::INT;
				likelihood = 0.5;
				break;
			case ARG_TYPE_FLOAT | ARG_NUMBER_CLASS_BIT:
			case ARG_TYPE_FLOAT:
				r_type = Variant::FLOAT;
				likelihood = 1.0;
				break;
			case ARG_TYPE_DOUBLE | ARG_NUMBER_CLASS_BIT:
			case ARG_TYPE_DOUBLE:
				r_type = Variant::FLOAT;
				likelihood = 0.5;
				break;
			case ARG_TYPE_STRING:
			case ARG_TYPE_CHARSEQUENCE:
				r_type = Variant::STRING;
				break;
			case ARG_TYPE_CALLABLE:
				r_type = Variant::CALLABLE;
				break;
			case ARG_TYPE_CLASS:
				r_type = Variant::OBJECT;
				break;
			case ARG_ARRAY_BIT | ARG_TYPE_VOID:
				r_type = Variant::NIL;
				break;
			case ARG_ARRAY_BIT | ARG_TYPE_BOOLEAN:
				r_type = Variant::ARRAY;
				break;
			case ARG_ARRAY_BIT | ARG_TYPE_BYTE:
				r_type = Variant::PACKED_BYTE_ARRAY;
				likelihood = 1.0;
				break;
			case ARG_ARRAY_BIT | ARG_TYPE_CHAR:
				r_type = Variant::PACKED_BYTE_ARRAY;
				likelihood = 0.5;
				break;
			case ARG_ARRAY_BIT | ARG_TYPE_SHORT:
				r_type = Variant::PACKED_INT32_ARRAY;
				likelihood = 0.3;
				break;
			case ARG_ARRAY_BIT | ARG_TYPE_INT:
				r_type = Variant::PACKED_INT32_ARRAY;
				likelihood = 1.0;
				break;
			case ARG_ARRAY_BIT | ARG_TYPE_LONG:
				r_type = Variant::PACKED_INT32_ARRAY;
				likelihood = 0.5;
				break;
			case ARG_ARRAY_BIT | ARG_TYPE_FLOAT:
				r_type = Variant::PACKED_FLOAT32_ARRAY;
				likelihood = 1.0;
				break;
			case ARG_ARRAY_BIT | ARG_TYPE_DOUBLE:
				r_type = Variant::PACKED_FLOAT32_ARRAY;
				likelihood = 0.5;
				break;
			case ARG_ARRAY_BIT | ARG_TYPE_STRING:
			case ARG_ARRAY_BIT | ARG_TYPE_CHARSEQUENCE:
				r_type = Variant::PACKED_STRING_ARRAY;
				break;
			case ARG_ARRAY_BIT | ARG_TYPE_CLASS:
			case ARG_ARRAY_BIT | ARG_TYPE_CALLABLE:
				r_type = Variant::ARRAY;
				break;
		}
	}

	_FORCE_INLINE_ static bool _convert_object_to_variant(JNIEnv *env, jobject obj, Variant &var, uint32_t p_sig);

	bool _call_method(JavaObject *p_instance, const StringName &p_method, const Variant **p_args, int p_argcount, Callable::CallError &r_error, Variant &ret);

	friend class JavaClassWrapper;
	friend class JavaObject;
	String java_class_name;
	String java_constructor_name;
	HashMap<StringName, List<MethodInfo>> methods;
	jclass _class;
#endif

protected:
	static void _bind_methods();
	bool _get(const StringName &p_name, Variant &r_ret) const;

public:
	virtual Variant callp(const StringName &p_method, const Variant **p_args, int p_argcount, Callable::CallError &r_error) override;

	String get_java_class_name() const;
	TypedArray<Dictionary> get_java_method_list() const;
	Ref<JavaClass> get_java_parent_class() const;

#ifdef ANDROID_ENABLED
	virtual String to_string() override;
#endif

	JavaClass();
	~JavaClass();
};

class JavaObject : public RefCounted {
	GDCLASS(JavaObject, RefCounted);

#ifdef ANDROID_ENABLED
	Ref<JavaClass> base_class;
	friend class JavaClass;

	jobject instance = nullptr;
#endif

protected:
	static void _bind_methods();

public:
	virtual Variant callp(const StringName &p_method, const Variant **p_args, int p_argcount, Callable::CallError &r_error) override;

	Ref<JavaClass> get_java_class() const;

#ifdef ANDROID_ENABLED
	virtual String to_string() override;

	jobject get_instance() { return instance; }

	JavaObject();
	JavaObject(const Ref<JavaClass> &p_base, jobject p_instance);
	~JavaObject();
#endif
};

class JavaClassWrapper : public Object {
	GDCLASS(JavaClassWrapper, Object);

#ifdef ANDROID_ENABLED
	RBMap<String, Ref<JavaClass>> class_cache;
	friend class JavaClass;
	jmethodID Class_getDeclaredConstructors;
	jmethodID Class_getDeclaredMethods;
	jmethodID Class_getFields;
	jmethodID Class_getName;
	jmethodID Class_getSuperclass;
	jmethodID Constructor_getParameterTypes;
	jmethodID Constructor_getModifiers;
	jmethodID Method_getParameterTypes;
	jmethodID Method_getReturnType;
	jmethodID Method_getModifiers;
	jmethodID Method_getName;
	jmethodID Field_getName;
	jmethodID Field_getModifiers;
	jmethodID Field_get;
	jmethodID Boolean_booleanValue;
	jmethodID Byte_byteValue;
	jmethodID Character_characterValue;
	jmethodID Short_shortValue;
	jmethodID Integer_integerValue;
	jmethodID Long_longValue;
	jmethodID Float_floatValue;
	jmethodID Double_doubleValue;

	bool _get_type_sig(JNIEnv *env, jobject obj, uint32_t &sig, String &strsig);
#endif

	Ref<JavaClass> _wrap(const String &p_class, bool p_allow_private_methods_access);

	static JavaClassWrapper *singleton;

protected:
	static void _bind_methods();

public:
	static JavaClassWrapper *get_singleton() { return singleton; }

	Ref<JavaClass> wrap(const String &p_class) {
		return _wrap(p_class, false);
	}

#ifdef ANDROID_ENABLED
	Ref<JavaClass> wrap_jclass(jclass p_class, bool p_allow_private_methods_access = false);
#endif
	JavaClassWrapper();
};

#endif // JAVA_CLASS_WRAPPER_H

/**************************************************************************/
/*  callable_jni.cpp                                                      */
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

#include "callable_jni.h"

#include "jni_utils.h"

#include "core/error/error_macros.h"
#include "core/object/object.h"

static Callable _generate_callable(JNIEnv *p_env, jlong p_object_id, jstring p_method_name, jobjectArray p_parameters) {
	Object *obj = ObjectDB::get_instance(ObjectID(p_object_id));
	ERR_FAIL_NULL_V(obj, Callable());

	String str_method = jstring_to_string(p_method_name, p_env);

	int count = p_env->GetArrayLength(p_parameters);

	Variant *args = (Variant *)alloca(sizeof(Variant) * count);
	const Variant **argptrs = (const Variant **)alloca(sizeof(Variant *) * count);

	for (int i = 0; i < count; i++) {
		jobject jobj = p_env->GetObjectArrayElement(p_parameters, i);
		ERR_FAIL_NULL_V(jobj, Callable());
		memnew_placement(&args[i], Variant(_jobject_to_variant(p_env, jobj)));
		argptrs[i] = &args[i];
		p_env->DeleteLocalRef(jobj);
	}

	Callable ret = Callable(obj, str_method).bindp(argptrs, count);

	// Manually invoke the destructor to decrease the reference counts for the variant arguments.
	for (int i = 0; i < count; i++) {
		args[i].~Variant();
	}

	return ret;
}

extern "C" {
JNIEXPORT jobject JNICALL Java_org_godotengine_godot_variant_Callable_nativeCall(JNIEnv *p_env, jclass p_clazz, jlong p_native_callable, jobjectArray p_parameters) {
	const Variant *callable_variant = reinterpret_cast<const Variant *>(p_native_callable);
	ERR_FAIL_NULL_V(callable_variant, nullptr);
	if (callable_variant->get_type() != Variant::CALLABLE) {
		return nullptr;
	}

	int count = p_env->GetArrayLength(p_parameters);

	Variant *args = (Variant *)alloca(sizeof(Variant) * count);
	const Variant **argptrs = (const Variant **)alloca(sizeof(Variant *) * count);

	for (int i = 0; i < count; i++) {
		jobject jobj = p_env->GetObjectArrayElement(p_parameters, i);
		ERR_FAIL_NULL_V(jobj, nullptr);
		memnew_placement(&args[i], Variant(_jobject_to_variant(p_env, jobj)));
		argptrs[i] = &args[i];
		p_env->DeleteLocalRef(jobj);
	}

	Callable callable = *callable_variant;
	jobject ret = nullptr;
	if (callable.is_valid()) {
		Callable::CallError err;
		Variant result;
		callable.callp(argptrs, count, result, err);
		jvalret jresult = _variant_to_jvalue(p_env, result.get_type(), &result, true);
		ret = jresult.obj;
	}

	// Manually invoke the destructor to decrease the reference counts for the variant arguments.
	for (int i = 0; i < count; i++) {
		args[i].~Variant();
	}

	return ret;
}

JNIEXPORT jobject JNICALL Java_org_godotengine_godot_variant_Callable_nativeCallObject(JNIEnv *p_env, jclass p_clazz, jlong p_object_id, jstring p_method_name, jobjectArray p_parameters) {
	Callable callable = _generate_callable(p_env, p_object_id, p_method_name, p_parameters);
	if (callable.is_valid()) {
		Variant result = callable.call();
		jvalret jresult = _variant_to_jvalue(p_env, result.get_type(), &result, true);
		return jresult.obj;
	} else {
		return nullptr;
	}
}

JNIEXPORT void JNICALL Java_org_godotengine_godot_variant_Callable_nativeCallObjectDeferred(JNIEnv *p_env, jclass p_clazz, jlong p_object_id, jstring p_method_name, jobjectArray p_parameters) {
	Callable callable = _generate_callable(p_env, p_object_id, p_method_name, p_parameters);
	if (callable.is_valid()) {
		callable.call_deferred();
	}
}

JNIEXPORT void JNICALL
Java_org_godotengine_godot_variant_Callable_releaseNativePointer(JNIEnv *p_env, jclass clazz, jlong p_native_pointer) {
	Variant *variant = reinterpret_cast<Variant *>(p_native_pointer);
	ERR_FAIL_NULL(variant);
	memdelete(variant);
}
}

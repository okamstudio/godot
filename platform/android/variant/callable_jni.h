/**************************************************************************/
/*  callable_jni.h                                                        */
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

#include <jni.h>

extern "C" {
JNIEXPORT jobject JNICALL Java_org_godotengine_godot_variant_Callable_nativeCall(JNIEnv *p_env, jclass p_clazz, jlong p_native_callable, jobjectArray p_parameters);
JNIEXPORT jobject JNICALL Java_org_godotengine_godot_variant_Callable_nativeCallObject(JNIEnv *p_env, jclass p_clazz, jlong p_object_id, jstring p_method_name, jobjectArray p_parameters);
JNIEXPORT void JNICALL Java_org_godotengine_godot_variant_Callable_nativeCallObjectDeferred(JNIEnv *p_env, jclass p_clazz, jlong p_object_id, jstring p_method_name, jobjectArray p_parameters);
JNIEXPORT void JNICALL Java_org_godotengine_godot_variant_Callable_releaseNativePointer(JNIEnv *p_env, jclass clazz, jlong p_native_pointer);
}

/**************************************************************************/
/*  godot_plugin_jni.h                                                    */
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

#include <android/log.h>
#include <jni.h>

void unregister_plugins_singletons();

extern "C" {
JNIEXPORT jboolean JNICALL Java_org_godotengine_godot_plugin_GodotPlugin_nativeRegisterSingleton(JNIEnv *env, jclass clazz, jstring name, jobject obj);
JNIEXPORT void JNICALL Java_org_godotengine_godot_plugin_GodotPlugin_nativeRegisterMethod(JNIEnv *env, jclass clazz, jstring sname, jstring name, jstring ret, jobjectArray args);
JNIEXPORT void JNICALL Java_org_godotengine_godot_plugin_GodotPlugin_nativeRegisterSignal(JNIEnv *env, jclass clazz, jstring j_plugin_name, jstring j_signal_name, jobjectArray j_signal_param_types);
JNIEXPORT void JNICALL Java_org_godotengine_godot_plugin_GodotPlugin_nativeEmitSignal(JNIEnv *env, jclass clazz, jstring j_plugin_name, jstring j_signal_name, jobjectArray j_signal_params);
}

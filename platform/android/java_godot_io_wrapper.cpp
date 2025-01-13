/**************************************************************************/
/*  java_godot_io_wrapper.cpp                                             */
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

#include "java_godot_io_wrapper.h"

#include "core/error/error_list.h"
#include "core/math/rect2.h"
#include "core/variant/variant.h"

// JNIEnv is only valid within the thread it belongs to, in a multi threading environment
// we can't cache it.
// For GodotIO we call all access methods from our thread and we thus get a valid JNIEnv
// from get_jni_env().

GodotIOJavaWrapper::GodotIOJavaWrapper(JNIEnv *p_env, jobject p_godot_io_instance) {
	godot_io_instance = p_env->NewGlobalRef(p_godot_io_instance);
	if (godot_io_instance) {
		cls = p_env->GetObjectClass(godot_io_instance);
		if (cls) {
			cls = (jclass)p_env->NewGlobalRef(cls);
		} else {
			// this is a pretty serious fail.. bail... pointers will stay 0
			return;
		}

		_open_URI = p_env->GetMethodID(cls, "openURI", "(Ljava/lang/String;)I");
		_get_cache_dir = p_env->GetMethodID(cls, "getCacheDir", "()Ljava/lang/String;");
		_get_temp_dir = p_env->GetMethodID(cls, "getTempDir", "()Ljava/lang/String;");
		_get_data_dir = p_env->GetMethodID(cls, "getDataDir", "()Ljava/lang/String;");
		_get_display_cutouts = p_env->GetMethodID(cls, "getDisplayCutouts", "()[I"),
		_get_display_safe_area = p_env->GetMethodID(cls, "getDisplaySafeArea", "()[I"),
		_get_locale = p_env->GetMethodID(cls, "getLocale", "()Ljava/lang/String;");
		_get_model = p_env->GetMethodID(cls, "getModel", "()Ljava/lang/String;");
		_get_screen_DPI = p_env->GetMethodID(cls, "getScreenDPI", "()I");
		_get_scaled_density = p_env->GetMethodID(cls, "getScaledDensity", "()F");
		_get_screen_refresh_rate = p_env->GetMethodID(cls, "getScreenRefreshRate", "(D)D");
		_get_unique_id = p_env->GetMethodID(cls, "getUniqueID", "()Ljava/lang/String;");
		_show_keyboard = p_env->GetMethodID(cls, "showKeyboard", "(Ljava/lang/String;IIII)V");
		_hide_keyboard = p_env->GetMethodID(cls, "hideKeyboard", "()V");
		_has_hardware_keyboard = p_env->GetMethodID(cls, "hasHardwareKeyboard", "()Z");
		_set_screen_orientation = p_env->GetMethodID(cls, "setScreenOrientation", "(I)V");
		_get_screen_orientation = p_env->GetMethodID(cls, "getScreenOrientation", "()I");
		_get_system_dir = p_env->GetMethodID(cls, "getSystemDir", "(IZ)Ljava/lang/String;");
	}
}

GodotIOJavaWrapper::~GodotIOJavaWrapper() {
	JNIEnv *env = get_jni_env();
	ERR_FAIL_NULL(env);

	env->DeleteGlobalRef(cls);
	env->DeleteGlobalRef(godot_io_instance);
}

jobject GodotIOJavaWrapper::get_instance() {
	return godot_io_instance;
}

Error GodotIOJavaWrapper::open_uri(const String &p_uri) {
	if (_open_URI) {
		JNIEnv *env = get_jni_env();
		ERR_FAIL_NULL_V(env, ERR_UNAVAILABLE);
		jstring jStr = env->NewStringUTF(p_uri.utf8().get_data());
		Error result = env->CallIntMethod(godot_io_instance, _open_URI, jStr) ? ERR_CANT_OPEN : OK;
		env->DeleteLocalRef(jStr);
		return result;
	} else {
		return ERR_UNAVAILABLE;
	}
}

String GodotIOJavaWrapper::get_cache_dir() {
	if (_get_cache_dir) {
		JNIEnv *env = get_jni_env();
		ERR_FAIL_NULL_V(env, String());
		jstring s = (jstring)env->CallObjectMethod(godot_io_instance, _get_cache_dir);
		return jstring_to_string(s, env);
	} else {
		return String();
	}
}

String GodotIOJavaWrapper::get_temp_dir() {
	if (_get_temp_dir) {
		JNIEnv *env = get_jni_env();
		ERR_FAIL_NULL_V(env, String());
		jstring s = (jstring)env->CallObjectMethod(godot_io_instance, _get_temp_dir);
		return jstring_to_string(s, env);
	} else {
		return String();
	}
}

String GodotIOJavaWrapper::get_user_data_dir() {
	if (_get_data_dir) {
		JNIEnv *env = get_jni_env();
		ERR_FAIL_NULL_V(env, String());
		jstring s = (jstring)env->CallObjectMethod(godot_io_instance, _get_data_dir);
		return jstring_to_string(s, env);
	} else {
		return String();
	}
}

String GodotIOJavaWrapper::get_locale() {
	if (_get_locale) {
		JNIEnv *env = get_jni_env();
		ERR_FAIL_NULL_V(env, String());
		jstring s = (jstring)env->CallObjectMethod(godot_io_instance, _get_locale);
		return jstring_to_string(s, env);
	} else {
		return String();
	}
}

String GodotIOJavaWrapper::get_model() {
	if (_get_model) {
		JNIEnv *env = get_jni_env();
		ERR_FAIL_NULL_V(env, String());
		jstring s = (jstring)env->CallObjectMethod(godot_io_instance, _get_model);
		return jstring_to_string(s, env);
	} else {
		return String();
	}
}

int GodotIOJavaWrapper::get_screen_dpi() {
	if (_get_screen_DPI) {
		JNIEnv *env = get_jni_env();
		ERR_FAIL_NULL_V(env, 160);
		return env->CallIntMethod(godot_io_instance, _get_screen_DPI);
	} else {
		return 160;
	}
}

float GodotIOJavaWrapper::get_scaled_density() {
	if (_get_scaled_density) {
		JNIEnv *env = get_jni_env();
		ERR_FAIL_NULL_V(env, 1.0f);
		return env->CallFloatMethod(godot_io_instance, _get_scaled_density);
	} else {
		return 1.0f;
	}
}

float GodotIOJavaWrapper::get_screen_refresh_rate(float fallback) {
	if (_get_screen_refresh_rate) {
		JNIEnv *env = get_jni_env();
		if (env == nullptr) {
			ERR_PRINT("An error occurred while trying to get screen refresh rate.");
			return fallback;
		}
		return (float)env->CallDoubleMethod(godot_io_instance, _get_screen_refresh_rate, (double)fallback);
	}
	ERR_PRINT("An error occurred while trying to get the screen refresh rate.");
	return fallback;
}

TypedArray<Rect2> GodotIOJavaWrapper::get_display_cutouts() {
	TypedArray<Rect2> result;
	ERR_FAIL_NULL_V(_get_display_cutouts, result);
	JNIEnv *env = get_jni_env();
	ERR_FAIL_NULL_V(env, result);
	jintArray returnArray = (jintArray)env->CallObjectMethod(godot_io_instance, _get_display_cutouts);
	jint arrayLength = env->GetArrayLength(returnArray);
	jint *arrayBody = env->GetIntArrayElements(returnArray, JNI_FALSE);
	int cutouts = arrayLength / 4;
	for (int i = 0; i < cutouts; i++) {
		int x = arrayBody[i * 4];
		int y = arrayBody[i * 4 + 1];
		int width = arrayBody[i * 4 + 2];
		int height = arrayBody[i * 4 + 3];
		Rect2 cutout(x, y, width, height);
		result.append(cutout);
	}
	env->ReleaseIntArrayElements(returnArray, arrayBody, 0);
	return result;
}

Rect2i GodotIOJavaWrapper::get_display_safe_area() {
	Rect2i result;
	ERR_FAIL_NULL_V(_get_display_safe_area, result);
	JNIEnv *env = get_jni_env();
	ERR_FAIL_NULL_V(env, result);
	jintArray returnArray = (jintArray)env->CallObjectMethod(godot_io_instance, _get_display_safe_area);
	ERR_FAIL_COND_V(env->GetArrayLength(returnArray) != 4, result);
	jint *arrayBody = env->GetIntArrayElements(returnArray, JNI_FALSE);
	result = Rect2i(arrayBody[0], arrayBody[1], arrayBody[2], arrayBody[3]);
	env->ReleaseIntArrayElements(returnArray, arrayBody, 0);
	return result;
}

String GodotIOJavaWrapper::get_unique_id() {
	if (_get_unique_id) {
		JNIEnv *env = get_jni_env();
		ERR_FAIL_NULL_V(env, String());
		jstring s = (jstring)env->CallObjectMethod(godot_io_instance, _get_unique_id);
		return jstring_to_string(s, env);
	} else {
		return String();
	}
}

bool GodotIOJavaWrapper::has_vk() {
	return (_show_keyboard != nullptr) && (_hide_keyboard != nullptr);
}

bool GodotIOJavaWrapper::has_hardware_keyboard() {
	if (_has_hardware_keyboard) {
		JNIEnv *env = get_jni_env();
		ERR_FAIL_NULL_V(env, false);
		return env->CallBooleanMethod(godot_io_instance, _has_hardware_keyboard);
	} else {
		return false;
	}
}

void GodotIOJavaWrapper::show_vk(const String &p_existing, int p_type, int p_max_input_length, int p_cursor_start, int p_cursor_end) {
	if (_show_keyboard) {
		JNIEnv *env = get_jni_env();
		ERR_FAIL_NULL(env);
		jstring jStr = env->NewStringUTF(p_existing.utf8().get_data());
		env->CallVoidMethod(godot_io_instance, _show_keyboard, jStr, p_type, p_max_input_length, p_cursor_start, p_cursor_end);
		env->DeleteLocalRef(jStr);
	}
}

void GodotIOJavaWrapper::hide_vk() {
	if (_hide_keyboard) {
		JNIEnv *env = get_jni_env();
		ERR_FAIL_NULL(env);
		env->CallVoidMethod(godot_io_instance, _hide_keyboard);
	}
}

void GodotIOJavaWrapper::set_screen_orientation(int p_orient) {
	if (_set_screen_orientation) {
		JNIEnv *env = get_jni_env();
		ERR_FAIL_NULL(env);
		env->CallVoidMethod(godot_io_instance, _set_screen_orientation, p_orient);
	}
}

int GodotIOJavaWrapper::get_screen_orientation() {
	if (_get_screen_orientation) {
		JNIEnv *env = get_jni_env();
		ERR_FAIL_NULL_V(env, 0);
		return env->CallIntMethod(godot_io_instance, _get_screen_orientation);
	} else {
		return 0;
	}
}

String GodotIOJavaWrapper::get_system_dir(int p_dir, bool p_shared_storage) {
	if (_get_system_dir) {
		JNIEnv *env = get_jni_env();
		ERR_FAIL_NULL_V(env, String("."));
		jstring s = (jstring)env->CallObjectMethod(godot_io_instance, _get_system_dir, p_dir, p_shared_storage);
		return jstring_to_string(s, env);
	} else {
		return String(".");
	}
}

// SafeNumeric because it can be changed from non-main thread and we need to
// ensure the change is immediately visible to other threads.
static SafeNumeric<int> virtual_keyboard_height;

int GodotIOJavaWrapper::get_vk_height() {
	return virtual_keyboard_height.get();
}

void GodotIOJavaWrapper::set_vk_height(int p_height) {
	virtual_keyboard_height.set(p_height);
}

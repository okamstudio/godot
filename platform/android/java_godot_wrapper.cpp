/**************************************************************************/
/*  java_godot_wrapper.cpp                                                */
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

#include "java_godot_wrapper.h"

// JNIEnv is only valid within the thread it belongs to, in a multi threading environment
// we can't cache it.
// For Godot we call most access methods from our thread and we thus get a valid JNIEnv
// from get_jni_env(). For one or two we expect to pass the environment

// TODO we could probably create a base class for this...

GodotJavaWrapper::GodotJavaWrapper(JNIEnv *p_env, jobject p_activity, jobject p_godot_instance) {
	godot_instance = p_env->NewGlobalRef(p_godot_instance);
	activity = p_env->NewGlobalRef(p_activity);

	// get info about our Godot class so we can get pointers and stuff...
	godot_class = p_env->FindClass("org/godotengine/godot/Godot");
	if (godot_class) {
		godot_class = (jclass)p_env->NewGlobalRef(godot_class);
	} else {
		// this is a pretty serious fail.. bail... pointers will stay 0
		return;
	}
	activity_class = p_env->FindClass("android/app/Activity");
	if (activity_class) {
		activity_class = (jclass)p_env->NewGlobalRef(activity_class);
	} else {
		// this is a pretty serious fail.. bail... pointers will stay 0
		return;
	}

	// get some Godot method pointers...
	_restart = p_env->GetMethodID(godot_class, "restart", "()V");
	_finish = p_env->GetMethodID(godot_class, "forceQuit", "(I)Z");
	_set_keep_screen_on = p_env->GetMethodID(godot_class, "setKeepScreenOn", "(Z)V");
	_alert = p_env->GetMethodID(godot_class, "alert", "(Ljava/lang/String;Ljava/lang/String;)V");
	_is_dark_mode_supported = p_env->GetMethodID(godot_class, "isDarkModeSupported", "()Z");
	_is_dark_mode = p_env->GetMethodID(godot_class, "isDarkMode", "()Z");
	_get_accent_color = p_env->GetMethodID(godot_class, "getAccentColor", "()I");
	_get_base_color = p_env->GetMethodID(godot_class, "getBaseColor", "()I");
	_get_clipboard = p_env->GetMethodID(godot_class, "getClipboard", "()Ljava/lang/String;");
	_set_clipboard = p_env->GetMethodID(godot_class, "setClipboard", "(Ljava/lang/String;)V");
	_has_clipboard = p_env->GetMethodID(godot_class, "hasClipboard", "()Z");
	_show_dialog = p_env->GetMethodID(godot_class, "showDialog", "(Ljava/lang/String;Ljava/lang/String;[Ljava/lang/String;)V");
	_show_input_dialog = p_env->GetMethodID(godot_class, "showInputDialog", "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V");
	_show_file_picker = p_env->GetMethodID(godot_class, "showFilePicker", "(Ljava/lang/String;Ljava/lang/String;I[Ljava/lang/String;)V");
	_request_permission = p_env->GetMethodID(godot_class, "requestPermission", "(Ljava/lang/String;)Z");
	_request_permissions = p_env->GetMethodID(godot_class, "requestPermissions", "()Z");
	_get_granted_permissions = p_env->GetMethodID(godot_class, "getGrantedPermissions", "()[Ljava/lang/String;");
	_get_ca_certificates = p_env->GetMethodID(godot_class, "getCACertificates", "()Ljava/lang/String;");
	_init_input_devices = p_env->GetMethodID(godot_class, "initInputDevices", "()V");
	_vibrate = p_env->GetMethodID(godot_class, "vibrate", "(II)V");
	_get_input_fallback_mapping = p_env->GetMethodID(godot_class, "getInputFallbackMapping", "()Ljava/lang/String;");
	_on_godot_setup_completed = p_env->GetMethodID(godot_class, "onGodotSetupCompleted", "()V");
	_on_godot_main_loop_started = p_env->GetMethodID(godot_class, "onGodotMainLoopStarted", "()V");
	_on_godot_terminating = p_env->GetMethodID(godot_class, "onGodotTerminating", "()V");
	_create_new_godot_instance = p_env->GetMethodID(godot_class, "createNewGodotInstance", "([Ljava/lang/String;)I");
	_get_render_view = p_env->GetMethodID(godot_class, "getRenderView", "()Lorg/godotengine/godot/GodotRenderView;");
	_begin_benchmark_measure = p_env->GetMethodID(godot_class, "nativeBeginBenchmarkMeasure", "(Ljava/lang/String;Ljava/lang/String;)V");
	_end_benchmark_measure = p_env->GetMethodID(godot_class, "nativeEndBenchmarkMeasure", "(Ljava/lang/String;Ljava/lang/String;)V");
	_dump_benchmark = p_env->GetMethodID(godot_class, "nativeDumpBenchmark", "(Ljava/lang/String;)V");
	_get_gdextension_list_config_file = p_env->GetMethodID(godot_class, "getGDExtensionConfigFiles", "()[Ljava/lang/String;");
	_has_feature = p_env->GetMethodID(godot_class, "hasFeature", "(Ljava/lang/String;)Z");
	_sign_apk = p_env->GetMethodID(godot_class, "nativeSignApk", "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)I");
	_verify_apk = p_env->GetMethodID(godot_class, "nativeVerifyApk", "(Ljava/lang/String;)I");
	_enable_immersive_mode = p_env->GetMethodID(godot_class, "nativeEnableImmersiveMode", "(Z)V");
	_is_in_immersive_mode = p_env->GetMethodID(godot_class, "isInImmersiveMode", "()Z");
	_on_editor_workspace_selected = p_env->GetMethodID(godot_class, "nativeOnEditorWorkspaceSelected", "(Ljava/lang/String;)V");
}

GodotJavaWrapper::~GodotJavaWrapper() {
	if (godot_view) {
		delete godot_view;
	}

	JNIEnv *env = get_jni_env();
	ERR_FAIL_NULL(env);
	env->DeleteGlobalRef(godot_instance);
	env->DeleteGlobalRef(godot_class);
	env->DeleteGlobalRef(activity);
	env->DeleteGlobalRef(activity_class);
}

jobject GodotJavaWrapper::get_activity() {
	return activity;
}

GodotJavaViewWrapper *GodotJavaWrapper::get_godot_view() {
	if (godot_view != nullptr) {
		return godot_view;
	}
	if (_get_render_view) {
		JNIEnv *env = get_jni_env();
		ERR_FAIL_NULL_V(env, nullptr);
		jobject godot_render_view = env->CallObjectMethod(godot_instance, _get_render_view);
		if (!env->IsSameObject(godot_render_view, nullptr)) {
			godot_view = new GodotJavaViewWrapper(godot_render_view);
		}
	}
	return godot_view;
}

void GodotJavaWrapper::on_godot_setup_completed(JNIEnv *p_env) {
	if (_on_godot_setup_completed) {
		if (p_env == nullptr) {
			p_env = get_jni_env();
		}
		p_env->CallVoidMethod(godot_instance, _on_godot_setup_completed);
	}
}

void GodotJavaWrapper::on_godot_main_loop_started(JNIEnv *p_env) {
	if (_on_godot_main_loop_started) {
		if (p_env == nullptr) {
			p_env = get_jni_env();
		}
		ERR_FAIL_NULL(p_env);
		p_env->CallVoidMethod(godot_instance, _on_godot_main_loop_started);
	}
}

void GodotJavaWrapper::on_godot_terminating(JNIEnv *p_env) {
	if (_on_godot_terminating) {
		if (p_env == nullptr) {
			p_env = get_jni_env();
		}
		ERR_FAIL_NULL(p_env);
		p_env->CallVoidMethod(godot_instance, _on_godot_terminating);
	}
}

void GodotJavaWrapper::restart(JNIEnv *p_env) {
	if (_restart) {
		if (p_env == nullptr) {
			p_env = get_jni_env();
		}
		ERR_FAIL_NULL(p_env);
		p_env->CallVoidMethod(godot_instance, _restart);
	}
}

bool GodotJavaWrapper::force_quit(JNIEnv *p_env, int p_instance_id) {
	if (_finish) {
		if (p_env == nullptr) {
			p_env = get_jni_env();
		}
		ERR_FAIL_NULL_V(p_env, false);
		return p_env->CallBooleanMethod(godot_instance, _finish, p_instance_id);
	}
	return false;
}

void GodotJavaWrapper::set_keep_screen_on(bool p_enabled) {
	if (_set_keep_screen_on) {
		JNIEnv *env = get_jni_env();
		ERR_FAIL_NULL(env);
		env->CallVoidMethod(godot_instance, _set_keep_screen_on, p_enabled);
	}
}

void GodotJavaWrapper::alert(const String &p_message, const String &p_title) {
	if (_alert) {
		JNIEnv *env = get_jni_env();
		ERR_FAIL_NULL(env);
		jstring jStrMessage = env->NewStringUTF(p_message.utf8().get_data());
		jstring jStrTitle = env->NewStringUTF(p_title.utf8().get_data());
		env->CallVoidMethod(godot_instance, _alert, jStrMessage, jStrTitle);
		env->DeleteLocalRef(jStrMessage);
		env->DeleteLocalRef(jStrTitle);
	}
}

bool GodotJavaWrapper::is_dark_mode_supported() {
	if (_is_dark_mode_supported) {
		JNIEnv *env = get_jni_env();
		ERR_FAIL_NULL_V(env, false);
		return env->CallBooleanMethod(godot_instance, _is_dark_mode_supported);
	} else {
		return false;
	}
}

bool GodotJavaWrapper::is_dark_mode() {
	if (_is_dark_mode) {
		JNIEnv *env = get_jni_env();
		ERR_FAIL_NULL_V(env, false);
		return env->CallBooleanMethod(godot_instance, _is_dark_mode);
	} else {
		return false;
	}
}

// Convert ARGB to RGBA.
static Color _argb_to_rgba(int p_color) {
	int alpha = (p_color >> 24) & 0xFF;
	int red = (p_color >> 16) & 0xFF;
	int green = (p_color >> 8) & 0xFF;
	int blue = p_color & 0xFF;
	return Color(red / 255.0f, green / 255.0f, blue / 255.0f, alpha / 255.0f);
}

Color GodotJavaWrapper::get_accent_color() {
	if (_get_accent_color) {
		JNIEnv *env = get_jni_env();
		ERR_FAIL_NULL_V(env, Color(0, 0, 0, 0));
		int accent_color = env->CallIntMethod(godot_instance, _get_accent_color);
		return _argb_to_rgba(accent_color);
	} else {
		return Color(0, 0, 0, 0);
	}
}

Color GodotJavaWrapper::get_base_color() {
	if (_get_base_color) {
		JNIEnv *env = get_jni_env();
		ERR_FAIL_NULL_V(env, Color(0, 0, 0, 0));
		int base_color = env->CallIntMethod(godot_instance, _get_base_color);
		return _argb_to_rgba(base_color);
	} else {
		return Color(0, 0, 0, 0);
	}
}

bool GodotJavaWrapper::has_get_clipboard() {
	return _get_clipboard != nullptr;
}

String GodotJavaWrapper::get_clipboard() {
	String clipboard;
	if (_get_clipboard) {
		JNIEnv *env = get_jni_env();
		ERR_FAIL_NULL_V(env, String());
		jstring s = (jstring)env->CallObjectMethod(godot_instance, _get_clipboard);
		clipboard = jstring_to_string(s, env);
		env->DeleteLocalRef(s);
	}
	return clipboard;
}

String GodotJavaWrapper::get_input_fallback_mapping() {
	String input_fallback_mapping;
	if (_get_input_fallback_mapping) {
		JNIEnv *env = get_jni_env();
		ERR_FAIL_NULL_V(env, String());
		jstring fallback_mapping = (jstring)env->CallObjectMethod(godot_instance, _get_input_fallback_mapping);
		input_fallback_mapping = jstring_to_string(fallback_mapping, env);
		env->DeleteLocalRef(fallback_mapping);
	}
	return input_fallback_mapping;
}

bool GodotJavaWrapper::has_set_clipboard() {
	return _set_clipboard != nullptr;
}

void GodotJavaWrapper::set_clipboard(const String &p_text) {
	if (_set_clipboard) {
		JNIEnv *env = get_jni_env();
		ERR_FAIL_NULL(env);
		jstring jStr = env->NewStringUTF(p_text.utf8().get_data());
		env->CallVoidMethod(godot_instance, _set_clipboard, jStr);
		env->DeleteLocalRef(jStr);
	}
}

bool GodotJavaWrapper::has_has_clipboard() {
	return _has_clipboard != nullptr;
}

bool GodotJavaWrapper::has_clipboard() {
	if (_has_clipboard) {
		JNIEnv *env = get_jni_env();
		ERR_FAIL_NULL_V(env, false);
		return env->CallBooleanMethod(godot_instance, _has_clipboard);
	} else {
		return false;
	}
}

Error GodotJavaWrapper::show_dialog(const String &p_title, const String &p_description, const Vector<String> &p_buttons) {
	if (_show_input_dialog) {
		JNIEnv *env = get_jni_env();
		ERR_FAIL_NULL_V(env, ERR_UNCONFIGURED);
		jstring j_title = env->NewStringUTF(p_title.utf8().get_data());
		jstring j_description = env->NewStringUTF(p_description.utf8().get_data());
		jobjectArray j_buttons = env->NewObjectArray(p_buttons.size(), env->FindClass("java/lang/String"), nullptr);
		for (int i = 0; i < p_buttons.size(); ++i) {
			jstring j_button = env->NewStringUTF(p_buttons[i].utf8().get_data());
			env->SetObjectArrayElement(j_buttons, i, j_button);
			env->DeleteLocalRef(j_button);
		}
		env->CallVoidMethod(godot_instance, _show_dialog, j_title, j_description, j_buttons);
		env->DeleteLocalRef(j_title);
		env->DeleteLocalRef(j_description);
		env->DeleteLocalRef(j_buttons);
		return OK;
	} else {
		return ERR_UNCONFIGURED;
	}
}

Error GodotJavaWrapper::show_input_dialog(const String &p_title, const String &p_message, const String &p_existing_text) {
	if (_show_input_dialog) {
		JNIEnv *env = get_jni_env();
		ERR_FAIL_NULL_V(env, ERR_UNCONFIGURED);
		jstring jStrTitle = env->NewStringUTF(p_title.utf8().get_data());
		jstring jStrMessage = env->NewStringUTF(p_message.utf8().get_data());
		jstring jStrExistingText = env->NewStringUTF(p_existing_text.utf8().get_data());
		env->CallVoidMethod(godot_instance, _show_input_dialog, jStrTitle, jStrMessage, jStrExistingText);
		env->DeleteLocalRef(jStrTitle);
		env->DeleteLocalRef(jStrMessage);
		env->DeleteLocalRef(jStrExistingText);
		return OK;
	} else {
		return ERR_UNCONFIGURED;
	}
}

Error GodotJavaWrapper::show_file_picker(const String &p_current_directory, const String &p_filename, int p_mode, const Vector<String> &p_filters) {
	if (_show_file_picker) {
		JNIEnv *env = get_jni_env();
		ERR_FAIL_NULL_V(env, ERR_UNCONFIGURED);
		jstring j_current_directory = env->NewStringUTF(p_current_directory.utf8().get_data());
		jstring j_filename = env->NewStringUTF(p_filename.utf8().get_data());
		jint j_mode = p_mode;
		Vector<String> filters;
		for (const String &E : p_filters) {
			filters.append_array(E.get_slicec(';', 0).split(",")); // Add extensions.
			filters.append_array(E.get_slicec(';', 2).split(",")); // Add MIME types.
		}
		jobjectArray j_filters = env->NewObjectArray(filters.size(), env->FindClass("java/lang/String"), nullptr);
		for (int i = 0; i < filters.size(); ++i) {
			jstring j_filter = env->NewStringUTF(filters[i].utf8().get_data());
			env->SetObjectArrayElement(j_filters, i, j_filter);
			env->DeleteLocalRef(j_filter);
		}
		env->CallVoidMethod(godot_instance, _show_file_picker, j_current_directory, j_filename, j_mode, j_filters);
		env->DeleteLocalRef(j_current_directory);
		env->DeleteLocalRef(j_filename);
		env->DeleteLocalRef(j_filters);
		return OK;
	} else {
		return ERR_UNCONFIGURED;
	}
}

bool GodotJavaWrapper::request_permission(const String &p_name) {
	if (_request_permission) {
		JNIEnv *env = get_jni_env();
		ERR_FAIL_NULL_V(env, false);
		jstring jStrName = env->NewStringUTF(p_name.utf8().get_data());
		bool result = env->CallBooleanMethod(godot_instance, _request_permission, jStrName);
		env->DeleteLocalRef(jStrName);
		return result;
	} else {
		return false;
	}
}

bool GodotJavaWrapper::request_permissions() {
	if (_request_permissions) {
		JNIEnv *env = get_jni_env();
		ERR_FAIL_NULL_V(env, false);
		return env->CallBooleanMethod(godot_instance, _request_permissions);
	} else {
		return false;
	}
}

Vector<String> GodotJavaWrapper::get_granted_permissions() const {
	Vector<String> permissions_list;
	if (_get_granted_permissions) {
		JNIEnv *env = get_jni_env();
		ERR_FAIL_NULL_V(env, permissions_list);
		jobject permissions_object = env->CallObjectMethod(godot_instance, _get_granted_permissions);
		jobjectArray *arr = reinterpret_cast<jobjectArray *>(&permissions_object);

		jsize len = env->GetArrayLength(*arr);
		for (int i = 0; i < len; i++) {
			jstring jstr = (jstring)env->GetObjectArrayElement(*arr, i);
			String str = jstring_to_string(jstr, env);
			permissions_list.push_back(str);
			env->DeleteLocalRef(jstr);
		}
	}
	return permissions_list;
}

Vector<String> GodotJavaWrapper::get_gdextension_list_config_file() const {
	Vector<String> config_file_list;
	if (_get_gdextension_list_config_file) {
		JNIEnv *env = get_jni_env();
		ERR_FAIL_NULL_V(env, config_file_list);
		jobject config_file_list_object = env->CallObjectMethod(godot_instance, _get_gdextension_list_config_file);
		jobjectArray *arr = reinterpret_cast<jobjectArray *>(&config_file_list_object);

		jsize len = env->GetArrayLength(*arr);
		for (int i = 0; i < len; i++) {
			jstring j_config_file = (jstring)env->GetObjectArrayElement(*arr, i);
			String config_file = jstring_to_string(j_config_file, env);
			config_file_list.push_back(config_file);
			env->DeleteLocalRef(j_config_file);
		}
	}
	return config_file_list;
}

String GodotJavaWrapper::get_ca_certificates() const {
	String ca_certificates;
	if (_get_ca_certificates) {
		JNIEnv *env = get_jni_env();
		ERR_FAIL_NULL_V(env, String());
		jstring s = (jstring)env->CallObjectMethod(godot_instance, _get_ca_certificates);
		ca_certificates = jstring_to_string(s, env);
		env->DeleteLocalRef(s);
	}
	return ca_certificates;
}

void GodotJavaWrapper::init_input_devices() {
	if (_init_input_devices) {
		JNIEnv *env = get_jni_env();
		ERR_FAIL_NULL(env);
		env->CallVoidMethod(godot_instance, _init_input_devices);
	}
}

void GodotJavaWrapper::vibrate(int p_duration_ms, float p_amplitude) {
	if (_vibrate) {
		JNIEnv *env = get_jni_env();
		ERR_FAIL_NULL(env);

		int j_amplitude = -1.0;

		if (p_amplitude != -1.0) {
			j_amplitude = CLAMP(int(p_amplitude * 255), 1, 255);
		}

		env->CallVoidMethod(godot_instance, _vibrate, p_duration_ms, j_amplitude);
	}
}

int GodotJavaWrapper::create_new_godot_instance(const List<String> &args) {
	if (_create_new_godot_instance) {
		JNIEnv *env = get_jni_env();
		ERR_FAIL_NULL_V(env, 0);
		jobjectArray jargs = env->NewObjectArray(args.size(), env->FindClass("java/lang/String"), env->NewStringUTF(""));
		int i = 0;
		for (List<String>::ConstIterator itr = args.begin(); itr != args.end(); ++itr, ++i) {
			jstring j_arg = env->NewStringUTF(itr->utf8().get_data());
			env->SetObjectArrayElement(jargs, i, j_arg);
			env->DeleteLocalRef(j_arg);
		}
		return env->CallIntMethod(godot_instance, _create_new_godot_instance, jargs);
	} else {
		return 0;
	}
}

void GodotJavaWrapper::begin_benchmark_measure(const String &p_context, const String &p_label) {
	if (_begin_benchmark_measure) {
		JNIEnv *env = get_jni_env();
		ERR_FAIL_NULL(env);
		jstring j_context = env->NewStringUTF(p_context.utf8().get_data());
		jstring j_label = env->NewStringUTF(p_label.utf8().get_data());
		env->CallVoidMethod(godot_instance, _begin_benchmark_measure, j_context, j_label);
		env->DeleteLocalRef(j_context);
		env->DeleteLocalRef(j_label);
	}
}

void GodotJavaWrapper::end_benchmark_measure(const String &p_context, const String &p_label) {
	if (_end_benchmark_measure) {
		JNIEnv *env = get_jni_env();
		ERR_FAIL_NULL(env);
		jstring j_context = env->NewStringUTF(p_context.utf8().get_data());
		jstring j_label = env->NewStringUTF(p_label.utf8().get_data());
		env->CallVoidMethod(godot_instance, _end_benchmark_measure, j_context, j_label);
		env->DeleteLocalRef(j_context);
		env->DeleteLocalRef(j_label);
	}
}

void GodotJavaWrapper::dump_benchmark(const String &benchmark_file) {
	if (_dump_benchmark) {
		JNIEnv *env = get_jni_env();
		ERR_FAIL_NULL(env);
		jstring j_benchmark_file = env->NewStringUTF(benchmark_file.utf8().get_data());
		env->CallVoidMethod(godot_instance, _dump_benchmark, j_benchmark_file);
		env->DeleteLocalRef(j_benchmark_file);
	}
}

bool GodotJavaWrapper::has_feature(const String &p_feature) const {
	if (_has_feature) {
		JNIEnv *env = get_jni_env();
		ERR_FAIL_NULL_V(env, false);

		jstring j_feature = env->NewStringUTF(p_feature.utf8().get_data());
		bool result = env->CallBooleanMethod(godot_instance, _has_feature, j_feature);
		env->DeleteLocalRef(j_feature);
		return result;
	} else {
		return false;
	}
}

Error GodotJavaWrapper::sign_apk(const String &p_input_path, const String &p_output_path, const String &p_keystore_path, const String &p_keystore_user, const String &p_keystore_password) {
	if (_sign_apk) {
		JNIEnv *env = get_jni_env();
		ERR_FAIL_NULL_V(env, ERR_UNCONFIGURED);

		jstring j_input_path = env->NewStringUTF(p_input_path.utf8().get_data());
		jstring j_output_path = env->NewStringUTF(p_output_path.utf8().get_data());
		jstring j_keystore_path = env->NewStringUTF(p_keystore_path.utf8().get_data());
		jstring j_keystore_user = env->NewStringUTF(p_keystore_user.utf8().get_data());
		jstring j_keystore_password = env->NewStringUTF(p_keystore_password.utf8().get_data());

		int result = env->CallIntMethod(godot_instance, _sign_apk, j_input_path, j_output_path, j_keystore_path, j_keystore_user, j_keystore_password);

		env->DeleteLocalRef(j_input_path);
		env->DeleteLocalRef(j_output_path);
		env->DeleteLocalRef(j_keystore_path);
		env->DeleteLocalRef(j_keystore_user);
		env->DeleteLocalRef(j_keystore_password);

		return static_cast<Error>(result);
	} else {
		return ERR_UNCONFIGURED;
	}
}

Error GodotJavaWrapper::verify_apk(const String &p_apk_path) {
	if (_verify_apk) {
		JNIEnv *env = get_jni_env();
		ERR_FAIL_NULL_V(env, ERR_UNCONFIGURED);

		jstring j_apk_path = env->NewStringUTF(p_apk_path.utf8().get_data());
		int result = env->CallIntMethod(godot_instance, _verify_apk, j_apk_path);
		env->DeleteLocalRef(j_apk_path);
		return static_cast<Error>(result);
	} else {
		return ERR_UNCONFIGURED;
	}
}

void GodotJavaWrapper::enable_immersive_mode(bool p_enabled) {
	if (_enable_immersive_mode) {
		JNIEnv *env = get_jni_env();
		ERR_FAIL_NULL(env);
		env->CallVoidMethod(godot_instance, _enable_immersive_mode, p_enabled);
	}
}

bool GodotJavaWrapper::is_in_immersive_mode() {
	if (_is_in_immersive_mode) {
		JNIEnv *env = get_jni_env();
		ERR_FAIL_NULL_V(env, false);
		return env->CallBooleanMethod(godot_instance, _is_in_immersive_mode);
	} else {
		return false;
	}
}

void GodotJavaWrapper::on_editor_workspace_selected(const String &p_workspace) {
	if (_on_editor_workspace_selected) {
		JNIEnv *env = get_jni_env();
		ERR_FAIL_NULL(env);

		jstring j_workspace = env->NewStringUTF(p_workspace.utf8().get_data());
		env->CallVoidMethod(godot_instance, _on_editor_workspace_selected, j_workspace);
	}
}

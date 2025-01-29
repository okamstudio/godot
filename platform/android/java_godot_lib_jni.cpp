/**************************************************************************/
/*  java_godot_lib_jni.cpp                                                */
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

#include "java_godot_lib_jni.h"

#include "android_input_handler.h"
#include "api/java_class_wrapper.h"
#include "dir_access_jandroid.h"
#include "display_server_android.h"
#include "file_access_android.h"
#include "file_access_filesystem_jandroid.h"
#include "java_godot_io_wrapper.h"
#include "java_godot_wrapper.h"
#include "jni_utils.h"
#include "net_socket_android.h"
#include "os_android.h"
#include "plugin/godot_plugin_jni.h"
#include "thread_jandroid.h"
#include "tts_android.h"

#include "core/config/engine.h"
#include "core/config/project_settings.h"
#include "core/input/input.h"
#include "main/main.h"

#ifndef _3D_DISABLED
#include "servers/xr_server.h"
#endif // _3D_DISABLED

#ifdef TOOLS_ENABLED
#include "editor/editor_settings.h"
#endif

#include <android/asset_manager_jni.h>
#include <android/input.h>
#include <android/native_window_jni.h>
#include <unistd.h>

static JavaClassWrapper *java_class_wrapper = nullptr;
static OS_Android *os_android = nullptr;
static AndroidInputHandler *input_handler = nullptr;
static GodotJavaWrapper *godot_java = nullptr;
static GodotIOJavaWrapper *godot_io_java = nullptr;

enum StartupStep {
	STEP_TERMINATED = -1,
	STEP_SETUP,
	STEP_SHOW_LOGO,
	STEP_STARTED
};

static SafeNumeric<int> step; // Shared between UI and render threads

static Size2 new_size;
static Vector3 accelerometer;
static Vector3 gravity;
static Vector3 magnetometer;
static Vector3 gyroscope;

static void _terminate(JNIEnv *env, bool p_restart = false) {
	if (step.get() == STEP_TERMINATED) {
		return;
	}

	step.set(STEP_TERMINATED); // Ensure no further steps are attempted and no further events are sent

	// lets cleanup
	// Unregister android plugins
	unregister_plugins_singletons();

	if (java_class_wrapper) {
		memdelete(java_class_wrapper);
	}
	if (input_handler) {
		delete input_handler;
	}
	// Whether restarting is handled by 'Main::cleanup()'
	bool restart_on_cleanup = false;
	if (os_android) {
		restart_on_cleanup = os_android->is_restart_on_exit_set();
		os_android->main_loop_end();
		Main::cleanup();
		delete os_android;
	}
	if (godot_io_java) {
		delete godot_io_java;
	}

	TTS_Android::terminate();
	FileAccessAndroid::terminate();
	DirAccessJAndroid::terminate();
	FileAccessFilesystemJAndroid::terminate();
	NetSocketAndroid::terminate();

	if (godot_java) {
		godot_java->on_godot_terminating(env);
		if (!restart_on_cleanup) {
			if (p_restart) {
				godot_java->restart(env);
			} else {
				godot_java->force_quit(env);
			}
		}
		delete godot_java;
	}
}

extern "C" {

JNIEXPORT void JNICALL Java_org_godotengine_godot_GodotLib_setVirtualKeyboardHeight(JNIEnv *env, jclass clazz, jint p_height) {
	if (godot_io_java) {
		godot_io_java->set_vk_height(p_height);
	}
}

JNIEXPORT jboolean JNICALL Java_org_godotengine_godot_GodotLib_initialize(JNIEnv *env, jclass clazz, jobject p_activity, jobject p_godot_instance, jobject p_asset_manager, jobject p_godot_io, jobject p_net_utils, jobject p_directory_access_handler, jobject p_file_access_handler, jboolean p_use_apk_expansion) {
	JavaVM *jvm;
	env->GetJavaVM(&jvm);

	// create our wrapper classes
	godot_java = new GodotJavaWrapper(env, p_activity, p_godot_instance);
	godot_io_java = new GodotIOJavaWrapper(env, p_godot_io);

	init_thread_jandroid(jvm, env);

	FileAccessAndroid::setup(p_asset_manager);
	DirAccessJAndroid::setup(p_directory_access_handler);
	FileAccessFilesystemJAndroid::setup(p_file_access_handler);
	NetSocketAndroid::setup(p_net_utils);

	os_android = new OS_Android(godot_java, godot_io_java, p_use_apk_expansion);

	return true;
}

JNIEXPORT void JNICALL Java_org_godotengine_godot_GodotLib_ondestroy(JNIEnv *env, jclass clazz) {
	_terminate(env, false);
}

JNIEXPORT jboolean JNICALL Java_org_godotengine_godot_GodotLib_setup(JNIEnv *env, jclass clazz, jobjectArray p_cmdline, jobject p_godot_tts) {
	setup_android_thread();

	const char **cmdline = nullptr;
	jstring *j_cmdline = nullptr;
	int cmdlen = 0;
	if (p_cmdline) {
		cmdlen = env->GetArrayLength(p_cmdline);
		if (cmdlen) {
			cmdline = (const char **)memalloc((cmdlen + 1) * sizeof(const char *));
			ERR_FAIL_NULL_V_MSG(cmdline, false, "Out of memory.");
			cmdline[cmdlen] = nullptr;
			j_cmdline = (jstring *)memalloc(cmdlen * sizeof(jstring));
			ERR_FAIL_NULL_V_MSG(j_cmdline, false, "Out of memory.");

			for (int i = 0; i < cmdlen; i++) {
				jstring string = (jstring)env->GetObjectArrayElement(p_cmdline, i);
				const char *rawString = env->GetStringUTFChars(string, nullptr);

				cmdline[i] = rawString;
				j_cmdline[i] = string;
			}
		}
	}

	Error err = Main::setup(OS_Android::ANDROID_EXEC_PATH, cmdlen, (char **)cmdline, false);
	if (cmdline) {
		if (j_cmdline) {
			for (int i = 0; i < cmdlen; ++i) {
				env->ReleaseStringUTFChars(j_cmdline[i], cmdline[i]);
			}
			memfree(j_cmdline);
		}
		memfree(cmdline);
	}

	// Note: --help and --version return ERR_HELP, but this should be translated to 0 if exit codes are propagated.
	if (err != OK) {
		return false;
	}

	TTS_Android::setup(p_godot_tts);

	java_class_wrapper = memnew(JavaClassWrapper);
	return true;
}

JNIEXPORT void JNICALL Java_org_godotengine_godot_GodotLib_resize(JNIEnv *env, jclass clazz, jobject p_surface, jint p_width, jint p_height) {
	if (os_android) {
		os_android->set_display_size(Size2i(p_width, p_height));

		// No need to reset the surface during startup
		if (step.get() > STEP_SETUP) {
			if (p_surface) {
				ANativeWindow *native_window = ANativeWindow_fromSurface(env, p_surface);
				os_android->set_native_window(native_window);
			}
			DisplayServerAndroid::get_singleton()->reset_window();
			DisplayServerAndroid::get_singleton()->notify_surface_changed(p_width, p_height);
		}
	}
}

JNIEXPORT void JNICALL Java_org_godotengine_godot_GodotLib_newcontext(JNIEnv *env, jclass clazz, jobject p_surface) {
	if (os_android) {
		if (step.get() == STEP_SETUP) {
			// During startup
			if (p_surface) {
				ANativeWindow *native_window = ANativeWindow_fromSurface(env, p_surface);
				os_android->set_native_window(native_window);
			}
		} else {
			// Rendering context recreated because it was lost; restart app to let it reload everything
			_terminate(env, true);
		}
	}
}

JNIEXPORT void JNICALL Java_org_godotengine_godot_GodotLib_back(JNIEnv *env, jclass clazz) {
	if (step.get() <= STEP_SETUP) {
		return;
	}

	if (DisplayServerAndroid *dsa = Object::cast_to<DisplayServerAndroid>(DisplayServer::get_singleton())) {
		dsa->send_window_event(DisplayServer::WINDOW_EVENT_GO_BACK_REQUEST);
	}
}

JNIEXPORT void JNICALL Java_org_godotengine_godot_GodotLib_ttsCallback(JNIEnv *env, jclass clazz, jint event, jint id, jint pos) {
	TTS_Android::_java_utterance_callback(event, id, pos);
}

JNIEXPORT jboolean JNICALL Java_org_godotengine_godot_GodotLib_step(JNIEnv *env, jclass clazz) {
	if (step.get() == STEP_TERMINATED) {
		return true;
	}

	if (step.get() == STEP_SETUP) {
		// Since Godot is initialized on the UI thread, main_thread_id was set to that thread's id,
		// but for Godot purposes, the main thread is the one running the game loop
		Main::setup2(false); // The logo is shown in the next frame otherwise we run into rendering issues
		input_handler = new AndroidInputHandler();
		step.increment();
		return true;
	}

	if (step.get() == STEP_SHOW_LOGO) {
		bool xr_enabled = false;
#ifndef _3D_DISABLED
		// Unlike PCVR, there's no additional 2D screen onto which to render the boot logo,
		// so we skip this step if xr is enabled.
		if (XRServer::get_xr_mode() == XRServer::XRMODE_DEFAULT) {
			xr_enabled = GLOBAL_GET("xr/shaders/enabled");
		} else {
			xr_enabled = XRServer::get_xr_mode() == XRServer::XRMODE_ON;
		}
#endif // _3D_DISABLED
		if (!xr_enabled) {
			Main::setup_boot_logo();
		}

		step.increment();
		return true;
	}

	if (step.get() == STEP_STARTED) {
		if (Main::start() != EXIT_SUCCESS) {
			return true; // should exit instead and print the error
		}

		godot_java->on_godot_setup_completed(env);
		os_android->main_loop_begin();
		godot_java->on_godot_main_loop_started(env);
		step.increment();
	}

	DisplayServerAndroid::get_singleton()->process_accelerometer(accelerometer);
	DisplayServerAndroid::get_singleton()->process_gravity(gravity);
	DisplayServerAndroid::get_singleton()->process_magnetometer(magnetometer);
	DisplayServerAndroid::get_singleton()->process_gyroscope(gyroscope);

	bool should_swap_buffers = false;
	if (os_android->main_loop_iterate(&should_swap_buffers)) {
		_terminate(env, false);
	}

	return should_swap_buffers;
}

// Called on the UI thread
JNIEXPORT void JNICALL Java_org_godotengine_godot_GodotLib_dispatchMouseEvent(JNIEnv *env, jclass clazz, jint p_event_type, jint p_button_mask, jfloat p_x, jfloat p_y, jfloat p_delta_x, jfloat p_delta_y, jboolean p_double_click, jboolean p_source_mouse_relative, jfloat p_pressure, jfloat p_tilt_x, jfloat p_tilt_y) {
	if (step.get() <= STEP_SETUP) {
		return;
	}

	input_handler->process_mouse_event(p_event_type, p_button_mask, Point2(p_x, p_y), Vector2(p_delta_x, p_delta_y), p_double_click, p_source_mouse_relative, p_pressure, Vector2(p_tilt_x, p_tilt_y));
}

// Called on the UI thread
JNIEXPORT void JNICALL Java_org_godotengine_godot_GodotLib_dispatchTouchEvent(JNIEnv *env, jclass clazz, jint ev, jint pointer, jint pointer_count, jfloatArray position, jboolean p_double_tap) {
	if (step.get() <= STEP_SETUP) {
		return;
	}

	Vector<AndroidInputHandler::TouchPos> points;
	for (int i = 0; i < pointer_count; i++) {
		jfloat p[6];
		env->GetFloatArrayRegion(position, i * 6, 6, p);
		AndroidInputHandler::TouchPos tp;
		tp.id = (int)p[0];
		tp.pos = Point2(p[1], p[2]);
		tp.pressure = p[3];
		tp.tilt = Vector2(p[4], p[5]);
		points.push_back(tp);
	}

	input_handler->process_touch_event(ev, pointer, points, p_double_tap);
}

// Called on the UI thread
JNIEXPORT void JNICALL Java_org_godotengine_godot_GodotLib_magnify(JNIEnv *env, jclass clazz, jfloat p_x, jfloat p_y, jfloat p_factor) {
	if (step.get() <= STEP_SETUP) {
		return;
	}
	input_handler->process_magnify(Point2(p_x, p_y), p_factor);
}

// Called on the UI thread
JNIEXPORT void JNICALL Java_org_godotengine_godot_GodotLib_pan(JNIEnv *env, jclass clazz, jfloat p_x, jfloat p_y, jfloat p_delta_x, jfloat p_delta_y) {
	if (step.get() <= STEP_SETUP) {
		return;
	}
	input_handler->process_pan(Point2(p_x, p_y), Vector2(p_delta_x, p_delta_y));
}

// Called on the UI thread
JNIEXPORT void JNICALL Java_org_godotengine_godot_GodotLib_joybutton(JNIEnv *env, jclass clazz, jint p_device, jint p_button, jboolean p_pressed) {
	if (step.get() <= STEP_SETUP) {
		return;
	}

	AndroidInputHandler::JoypadEvent jevent;
	jevent.device = p_device;
	jevent.type = AndroidInputHandler::JOY_EVENT_BUTTON;
	jevent.index = p_button;
	jevent.pressed = p_pressed;

	input_handler->process_joy_event(jevent);
}

// Called on the UI thread
JNIEXPORT void JNICALL Java_org_godotengine_godot_GodotLib_joyaxis(JNIEnv *env, jclass clazz, jint p_device, jint p_axis, jfloat p_value) {
	if (step.get() <= STEP_SETUP) {
		return;
	}

	AndroidInputHandler::JoypadEvent jevent;
	jevent.device = p_device;
	jevent.type = AndroidInputHandler::JOY_EVENT_AXIS;
	jevent.index = p_axis;
	jevent.value = p_value;

	input_handler->process_joy_event(jevent);
}

// Called on the UI thread
JNIEXPORT void JNICALL Java_org_godotengine_godot_GodotLib_joyhat(JNIEnv *env, jclass clazz, jint p_device, jint p_hat_x, jint p_hat_y) {
	if (step.get() <= STEP_SETUP) {
		return;
	}

	AndroidInputHandler::JoypadEvent jevent;
	jevent.device = p_device;
	jevent.type = AndroidInputHandler::JOY_EVENT_HAT;
	BitField<HatMask> hat;
	if (p_hat_x != 0) {
		if (p_hat_x < 0) {
			hat.set_flag(HatMask::LEFT);
		} else {
			hat.set_flag(HatMask::RIGHT);
		}
	}
	if (p_hat_y != 0) {
		if (p_hat_y < 0) {
			hat.set_flag(HatMask::UP);
		} else {
			hat.set_flag(HatMask::DOWN);
		}
	}
	jevent.hat = hat;

	input_handler->process_joy_event(jevent);
}

// Called on the UI thread
JNIEXPORT void JNICALL Java_org_godotengine_godot_GodotLib_joyconnectionchanged(JNIEnv *env, jclass clazz, jint p_device, jboolean p_connected, jstring p_name) {
	if (os_android) {
		String name = jstring_to_string(p_name, env);
		Input::get_singleton()->joy_connection_changed(p_device, p_connected, name);
	}
}

// Called on the UI thread
JNIEXPORT void JNICALL Java_org_godotengine_godot_GodotLib_key(JNIEnv *env, jclass clazz, jint p_physical_keycode, jint p_unicode, jint p_key_label, jboolean p_pressed, jboolean p_echo) {
	if (step.get() <= STEP_SETUP) {
		return;
	}
	input_handler->process_key_event(p_physical_keycode, p_unicode, p_key_label, p_pressed, p_echo);
}

JNIEXPORT void JNICALL Java_org_godotengine_godot_GodotLib_accelerometer(JNIEnv *env, jclass clazz, jfloat x, jfloat y, jfloat z) {
	accelerometer = Vector3(x, y, z);
}

JNIEXPORT void JNICALL Java_org_godotengine_godot_GodotLib_gravity(JNIEnv *env, jclass clazz, jfloat x, jfloat y, jfloat z) {
	gravity = Vector3(x, y, z);
}

JNIEXPORT void JNICALL Java_org_godotengine_godot_GodotLib_magnetometer(JNIEnv *env, jclass clazz, jfloat x, jfloat y, jfloat z) {
	magnetometer = Vector3(x, y, z);
}

JNIEXPORT void JNICALL Java_org_godotengine_godot_GodotLib_gyroscope(JNIEnv *env, jclass clazz, jfloat x, jfloat y, jfloat z) {
	gyroscope = Vector3(x, y, z);
}

JNIEXPORT void JNICALL Java_org_godotengine_godot_GodotLib_focusin(JNIEnv *env, jclass clazz) {
	if (step.get() <= STEP_SETUP) {
		return;
	}

	os_android->main_loop_focusin();
}

JNIEXPORT void JNICALL Java_org_godotengine_godot_GodotLib_focusout(JNIEnv *env, jclass clazz) {
	if (step.get() <= STEP_SETUP) {
		return;
	}

	os_android->main_loop_focusout();
}

JNIEXPORT jstring JNICALL Java_org_godotengine_godot_GodotLib_getGlobal(JNIEnv *env, jclass clazz, jstring path) {
	String js = jstring_to_string(path, env);

	Variant setting_with_override = GLOBAL_GET(js);
	String setting_value = (setting_with_override.get_type() == Variant::NIL) ? "" : setting_with_override;
	return env->NewStringUTF(setting_value.utf8().get_data());
}

JNIEXPORT jstring JNICALL Java_org_godotengine_godot_GodotLib_getEditorSetting(JNIEnv *env, jclass clazz, jstring p_setting_key) {
	String editor_setting_value = "";
#ifdef TOOLS_ENABLED
	String godot_setting_key = jstring_to_string(p_setting_key, env);
	Variant editor_setting = EDITOR_GET(godot_setting_key);
	editor_setting_value = (editor_setting.get_type() == Variant::NIL) ? "" : editor_setting;
#else
	WARN_PRINT("Access to the Editor Settings in only available on Editor builds");
#endif

	return env->NewStringUTF(editor_setting_value.utf8().get_data());
}

JNIEXPORT void JNICALL Java_org_godotengine_godot_GodotLib_onNightModeChanged(JNIEnv *env, jclass clazz) {
	DisplayServerAndroid *ds = (DisplayServerAndroid *)DisplayServer::get_singleton();
	if (ds) {
		ds->emit_system_theme_changed();
	}
}

JNIEXPORT void JNICALL Java_org_godotengine_godot_GodotLib_filePickerCallback(JNIEnv *env, jclass clazz, jboolean p_ok, jobjectArray p_selected_paths) {
	DisplayServerAndroid *ds = (DisplayServerAndroid *)DisplayServer::get_singleton();
	if (ds) {
		Vector<String> selected_paths;

		jint length = env->GetArrayLength(p_selected_paths);
		for (jint i = 0; i < length; ++i) {
			jstring java_string = (jstring)env->GetObjectArrayElement(p_selected_paths, i);
			String path = jstring_to_string(java_string, env);
			selected_paths.push_back(path);
			env->DeleteLocalRef(java_string);
		}
		ds->emit_file_picker_callback(p_ok, selected_paths);
	}
}

JNIEXPORT void JNICALL Java_org_godotengine_godot_GodotLib_requestPermissionResult(JNIEnv *env, jclass clazz, jstring p_permission, jboolean p_result) {
	String permission = jstring_to_string(p_permission, env);
	if (permission == "android.permission.RECORD_AUDIO" && p_result) {
		AudioDriver::get_singleton()->input_start();
	}

	if (os_android->get_main_loop()) {
		os_android->get_main_loop()->emit_signal(SNAME("on_request_permissions_result"), permission, p_result == JNI_TRUE);
	}
}

JNIEXPORT void JNICALL Java_org_godotengine_godot_GodotLib_onRendererResumed(JNIEnv *env, jclass clazz) {
	if (step.get() <= STEP_SETUP) {
		return;
	}

	// We force redraw to ensure we render at least once when resuming the app.
	Main::force_redraw();
	if (os_android->get_main_loop()) {
		os_android->get_main_loop()->notification(MainLoop::NOTIFICATION_APPLICATION_RESUMED);
	}
}

JNIEXPORT void JNICALL Java_org_godotengine_godot_GodotLib_onRendererPaused(JNIEnv *env, jclass clazz) {
	if (step.get() <= STEP_SETUP) {
		return;
	}

	if (os_android->get_main_loop()) {
		os_android->get_main_loop()->notification(MainLoop::NOTIFICATION_APPLICATION_PAUSED);
	}
}

JNIEXPORT jboolean JNICALL Java_org_godotengine_godot_GodotLib_shouldDispatchInputToRenderThread(JNIEnv *env, jclass clazz) {
	Input *input = Input::get_singleton();
	if (input) {
		return !input->is_agile_input_event_flushing();
	}
	return false;
}

JNIEXPORT jstring JNICALL Java_org_godotengine_godot_GodotLib_getProjectResourceDir(JNIEnv *env, jclass clazz) {
	const String resource_dir = OS::get_singleton()->get_resource_dir();
	return env->NewStringUTF(resource_dir.utf8().get_data());
}
}

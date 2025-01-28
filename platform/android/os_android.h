/**************************************************************************/
/*  os_android.h                                                          */
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

#ifndef OS_ANDROID_H
#define OS_ANDROID_H

#include "audio_driver_opensl.h"

#include "core/os/main_loop.h"
#include "drivers/unix/os_unix.h"
#include "servers/audio_server.h"

class GodotJavaWrapper;
class GodotIOJavaWrapper;

struct ANativeWindow;

class OS_Android : public OS_Unix {
private:
	Size2i display_size;

	bool use_apk_expansion;

#if defined(GLES3_ENABLED)
	const char *gl_extensions;
#endif

#if defined(VULKAN_ENABLED)
	ANativeWindow *native_window = nullptr;
#endif

	mutable String data_dir_cache;
	mutable String cache_dir_cache;
	mutable String temp_dir_cache;
	mutable String remote_fs_dir;

	AudioDriverOpenSL audio_driver_android;

	MainLoop *main_loop = nullptr;

	struct FontInfo {
		String font_name;
		HashSet<String> lang;
		HashSet<String> script;
		int weight = 400;
		int stretch = 100;
		bool italic = false;
		int priority = 0;
		String filename;
	};

	mutable HashMap<String, String> font_aliases;
	mutable List<FontInfo> fonts;
	mutable HashSet<String> font_names;
	mutable bool font_config_loaded = false;

	GodotJavaWrapper *godot_java = nullptr;
	GodotIOJavaWrapper *godot_io_java = nullptr;

	void _load_system_font_config() const;
	String get_system_property(const char *key) const;

public:
	static const char *ANDROID_EXEC_PATH;
	static const int DEFAULT_WINDOW_WIDTH = 800;
	static const int DEFAULT_WINDOW_HEIGHT = 600;

#ifdef TOOLS_ENABLED
	Error sign_apk(const String &p_input_path, const String &p_output_path, const String &p_keystore_path, const String &p_keystore_user, const String &p_keystore_password);
	Error verify_apk(const String &p_apk_path);
#endif

	virtual void initialize_core() override;
	virtual void initialize() override;

	virtual void initialize_joypads() override;

	virtual void set_main_loop(MainLoop *p_main_loop) override;
	virtual void delete_main_loop() override;

	virtual void finalize() override;

	typedef int64_t ProcessID;

	static OS_Android *get_singleton();
	GodotJavaWrapper *get_godot_java();
	GodotIOJavaWrapper *get_godot_io_java();

	virtual bool request_permission(const String &p_name) override;
	virtual bool request_permissions() override;
	virtual Vector<String> get_granted_permissions() const override;

	virtual void alert(const String &p_alert, const String &p_title) override;

	virtual Error open_dynamic_library(const String &p_path, void *&p_library_handle, GDExtensionData *p_data = nullptr) override;

	virtual String get_name() const override;
	virtual String get_distribution_name() const override;
	virtual String get_version() const override;
	virtual MainLoop *get_main_loop() const override;

	void main_loop_begin();
	bool main_loop_iterate(bool *r_should_swap_buffers = nullptr);
	void main_loop_end();
	void main_loop_focusout();
	void main_loop_focusin();

	void set_display_size(const Size2i &p_size);
	Size2i get_display_size() const;

	void set_opengl_extensions(const char *p_gl_extensions);

	void set_native_window(ANativeWindow *p_native_window);
	ANativeWindow *get_native_window() const;

	virtual Error shell_open(const String &p_uri) override;

	virtual Vector<String> get_system_fonts() const override;
	virtual String get_system_font_path(const String &p_font_name, int p_weight = 400, int p_stretch = 100, bool p_italic = false) const override;
	virtual Vector<String> get_system_font_path_for_text(const String &p_font_name, const String &p_text, const String &p_locale = String(), const String &p_script = String(), int p_weight = 400, int p_stretch = 100, bool p_italic = false) const override;
	virtual String get_executable_path() const override;
	virtual String get_user_data_dir(const String &p_user_dir) const override;
	virtual String get_data_path() const override;
	virtual String get_cache_path() const override;
	virtual String get_temp_path() const override;
	virtual String get_resource_dir() const override;
	virtual String get_locale() const override;
	virtual String get_model_name() const override;

	virtual String get_unique_id() const override;

	virtual String get_system_dir(SystemDir p_dir, bool p_shared_storage = true) const override;

	virtual Error move_to_trash(const String &p_path) override;

	void vibrate_handheld(int p_duration_ms, float p_amplitude = -1.0) override;

	virtual String get_config_path() const override;

	virtual Error execute(const String &p_path, const List<String> &p_arguments, String *r_pipe = nullptr, int *r_exitcode = nullptr, bool read_stderr = false, Mutex *p_pipe_mutex = nullptr, bool p_open_console = false) override;
	virtual Error create_process(const String &p_path, const List<String> &p_arguments, ProcessID *r_child_id = nullptr, bool p_open_console = false) override;
	virtual Error create_instance(const List<String> &p_arguments, ProcessID *r_child_id = nullptr) override;
	virtual Error kill(const ProcessID &p_pid) override;
	virtual String get_system_ca_certificates() override;

	virtual Error setup_remote_filesystem(const String &p_server_host, int p_port, const String &p_password, String &r_project_path) override;

	virtual void benchmark_begin_measure(const String &p_context, const String &p_what) override;
	virtual void benchmark_end_measure(const String &p_context, const String &p_what) override;
	virtual void benchmark_dump() override;

	virtual void load_platform_gdextensions() const override;

	virtual bool _check_internal_feature_support(const String &p_feature) override;
	OS_Android(GodotJavaWrapper *p_godot_java, GodotIOJavaWrapper *p_godot_io_java, bool p_use_apk_expansion);
	~OS_Android();

private:
	// Location where we relocate external dynamic libraries to make them accessible.
	String get_dynamic_libraries_path() const;
	// Copy a dynamic library to the given location to make it accessible for loading.
	bool copy_dynamic_library(const String &p_library_path, const String &p_target_dir, String *r_copy_path = nullptr);
};

#endif // OS_ANDROID_H

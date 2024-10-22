/**************************************************************************/
/*  os.h                                                                  */
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

#ifndef OS_H
#define OS_H

#include "core/config/engine.h"
#include "core/io/logger.h"
#include "core/io/remote_filesystem_client.h"
#include "core/os/time_enums.h"
#include "core/string/ustring.h"
#include "core/templates/list.h"
#include "core/templates/vector.h"

#include <stdarg.h>
#include <stdlib.h>

class OS {
	static OS *singleton;
	static uint64_t target_ticks;
	String _execpath;
	List<String> _cmdline;
	List<String> _user_args;
	bool _keep_screen_on = true; // set default value to true, because this had been true before godot 2.0.
	bool low_processor_usage_mode = false;
	int low_processor_usage_mode_sleep_usec = 10000;
	bool _delta_smoothing_enabled = false;
	bool _verbose_stdout = false;
	bool _debug_stdout = false;
	String _local_clipboard;
	// Assume success by default, all failure cases need to set EXIT_FAILURE explicitly.
	int _exit_code = EXIT_SUCCESS;
	bool _allow_hidpi = false;
	bool _allow_layered = false;
	bool _stdout_enabled = true;
	bool _stderr_enabled = true;
	bool _writing_movie = false;
	bool _in_editor = false;

	CompositeLogger *_logger = nullptr;

	bool restart_on_exit = false;
	List<String> restart_commandline;

	// for the user interface we keep a record of the current display driver
	// so we can retrieve the rendering drivers available
	int _display_driver_id = -1;
	String _current_rendering_driver_name;
	String _current_rendering_method;
	bool _is_gles_over_gl = false;

	RemoteFilesystemClient default_rfs;

	// For tracking benchmark data
	bool use_benchmark = false;
	String benchmark_file;
	HashMap<Pair<String, String>, uint64_t, PairHash<String, String>> benchmark_marks_from;
	HashMap<Pair<String, String>, double, PairHash<String, String>> benchmark_marks_final;

protected:
	void _set_logger(CompositeLogger *p_logger);

public:
	typedef void (*ImeCallback)(void *p_inp, const String &p_text, Point2 p_selection);
	typedef bool (*HasServerFeatureCallback)(const String &p_feature);

	enum RenderThreadMode {
		RENDER_THREAD_UNSAFE,
		RENDER_THREAD_SAFE,
		RENDER_SEPARATE_THREAD
	};

protected:
	friend class Main;
	// Needed by tests to setup command-line args.
	friend int test_main(int argc, char *argv[]);

	HasServerFeatureCallback has_server_feature_callback = nullptr;
	RenderThreadMode _render_thread_mode = RENDER_THREAD_SAFE;

	// Functions used by Main to initialize/deinitialize the OS.
	void add_logger(Logger *p_logger);

	virtual void initialize() = 0;
	virtual void initialize_joypads() = 0;

	void set_display_driver_id(int p_display_driver_id) { _display_driver_id = p_display_driver_id; }

	virtual void set_main_loop(MainLoop *p_main_loop) = 0;
	virtual void delete_main_loop() = 0;

	virtual void finalize() = 0;
	virtual void finalize_core() = 0;

	virtual void set_cmdline(const char *p_execpath, const List<String> &p_args, const List<String> &p_user_args);

	virtual bool _check_internal_feature_support(const String &p_feature) = 0;

public:
	typedef int64_t ProcessID;

	static OS *get_singleton();

	void set_current_rendering_driver_name(const String &p_driver_name) { _current_rendering_driver_name = p_driver_name; }
	void set_current_rendering_method(const String &p_name) { _current_rendering_method = p_name; }
	void set_gles_over_gl(bool p_enabled) { _is_gles_over_gl = p_enabled; }

	String get_current_rendering_driver_name() const { return _current_rendering_driver_name; }
	String get_current_rendering_method() const { return _current_rendering_method; }
	bool get_gles_over_gl() const { return _is_gles_over_gl; }

	int get_display_driver_id() const { return _display_driver_id; }

	virtual Vector<String> get_video_adapter_driver_info() const = 0;
	virtual bool get_user_prefers_integrated_gpu() const { return false; }

	void print_error(const char *p_function, const char *p_file, int p_line, const char *p_code, const char *p_rationale, bool p_editor_notify = false, Logger::ErrorType p_type = Logger::ERR_ERROR);
	void print(const char *p_format, ...) _PRINTF_FORMAT_ATTRIBUTE_2_3;
	void print_rich(const char *p_format, ...) _PRINTF_FORMAT_ATTRIBUTE_2_3;
	void printerr(const char *p_format, ...) _PRINTF_FORMAT_ATTRIBUTE_2_3;

	virtual String get_stdin_string() = 0;

	virtual Error get_entropy(uint8_t *r_buffer, int p_bytes) = 0; // Should return cryptographically-safe random bytes.
	virtual String get_system_ca_certificates() { return ""; } // Concatenated certificates in PEM format.

	virtual PackedStringArray get_connected_midi_inputs();
	virtual void open_midi_inputs();
	virtual void close_midi_inputs();

	virtual void alert(const String &p_alert, const String &p_title = "ALERT!");

	struct GDExtensionData {
		bool also_set_library_path = false;
		String *r_resolved_path = nullptr;
		bool generate_temp_files = false;
		PackedStringArray *library_dependencies = nullptr;
	};

	virtual Error open_dynamic_library(const String &p_path, void *&p_library_handle, GDExtensionData *p_data = nullptr) { return ERR_UNAVAILABLE; }
	virtual Error close_dynamic_library(void *p_library_handle) { return ERR_UNAVAILABLE; }
	virtual Error get_dynamic_library_symbol_handle(void *p_library_handle, const String &p_name, void *&p_symbol_handle, bool p_optional = false) { return ERR_UNAVAILABLE; }

	virtual void set_low_processor_usage_mode(bool p_enabled);
	virtual bool is_in_low_processor_usage_mode() const;
	virtual void set_low_processor_usage_mode_sleep_usec(int p_usec);
	virtual int get_low_processor_usage_mode_sleep_usec() const;

	void set_delta_smoothing(bool p_enabled);
	bool is_delta_smoothing_enabled() const;

	virtual Vector<String> get_system_fonts() const { return Vector<String>(); };
	virtual String get_system_font_path(const String &p_font_name, int p_weight = 400, int p_stretch = 100, bool p_italic = false) const { return String(); };
	virtual Vector<String> get_system_font_path_for_text(const String &p_font_name, const String &p_text, const String &p_locale = String(), const String &p_script = String(), int p_weight = 400, int p_stretch = 100, bool p_italic = false) const { return Vector<String>(); };
	virtual String get_executable_path() const;
	virtual Error execute(const String &p_path, const List<String> &p_arguments, String *r_pipe = nullptr, int *r_exitcode = nullptr, bool read_stderr = false, Mutex *p_pipe_mutex = nullptr, bool p_open_console = false) = 0;
	virtual Dictionary execute_with_pipe(const String &p_path, const List<String> &p_arguments, bool p_blocking = true) { return Dictionary(); }
	virtual Error create_process(const String &p_path, const List<String> &p_arguments, ProcessID *r_child_id = nullptr, bool p_open_console = false) = 0;
	virtual Error create_instance(const List<String> &p_arguments, ProcessID *r_child_id = nullptr) { return create_process(get_executable_path(), p_arguments, r_child_id); };
	virtual Error kill(const ProcessID &p_pid) = 0;
	virtual int get_process_id() const;
	virtual bool is_process_running(const ProcessID &p_pid) const = 0;
	virtual int get_process_exit_code(const ProcessID &p_pid) const = 0;
	virtual void vibrate_handheld(int p_duration_ms = 500, float p_amplitude = -1.0) {}

	virtual Error shell_open(const String &p_uri);
	virtual Error shell_show_in_file_manager(String p_path, bool p_open_folder = true);
	virtual Error set_cwd(const String &p_cwd);

	virtual bool has_environment(const String &p_var) const = 0;
	virtual String get_environment(const String &p_var) const = 0;
	virtual void set_environment(const String &p_var, const String &p_value) const = 0;
	virtual void unset_environment(const String &p_var) const = 0;

	virtual String get_name() const = 0;
	virtual String get_identifier() const;
	virtual String get_distribution_name() const = 0;
	virtual String get_version() const = 0;
	virtual List<String> get_cmdline_args() const { return _cmdline; }
	virtual List<String> get_cmdline_user_args() const { return _user_args; }
	virtual List<String> get_cmdline_platform_args() const { return List<String>(); }
	virtual String get_model_name() const;

	bool is_layered_allowed() const { return _allow_layered; }
	bool is_hidpi_allowed() const { return _allow_hidpi; }

	void ensure_user_data_dir();

	virtual MainLoop *get_main_loop() const = 0;

	virtual void yield();

	struct DateTime {
		int64_t year;
		Month month;
		uint8_t day;
		Weekday weekday;
		uint8_t hour;
		uint8_t minute;
		uint8_t second;
		bool dst;
	};

	struct TimeZoneInfo {
		int bias;
		String name;
	};

	virtual DateTime get_datetime(bool utc = false) const = 0;
	virtual TimeZoneInfo get_time_zone_info() const = 0;
	virtual double get_unix_time() const;

	virtual void delay_usec(uint32_t p_usec) const = 0;
	virtual void add_frame_delay(bool p_can_draw);

	virtual uint64_t get_ticks_usec() const = 0;
	uint64_t get_ticks_msec() const;

	virtual bool is_userfs_persistent() const { return true; }

	bool is_stdout_verbose() const;
	bool is_stdout_debug_enabled() const;

	bool is_stdout_enabled() const;
	bool is_stderr_enabled() const;
	void set_stdout_enabled(bool p_enabled);
	void set_stderr_enabled(bool p_enabled);

	virtual void disable_crash_handler() {}
	virtual bool is_disable_crash_handler() const { return false; }
	virtual void initialize_debugging() {}

	virtual uint64_t get_static_memory_usage() const;
	virtual uint64_t get_static_memory_peak_usage() const;
	virtual Dictionary get_memory_info() const;

	RenderThreadMode get_render_thread_mode() const { return _render_thread_mode; }

	virtual String get_locale() const;
	String get_locale_language() const;

	virtual uint64_t get_embedded_pck_offset() const;

	String get_safe_dir_name(const String &p_dir_name, bool p_allow_paths = false) const;
	virtual String get_godot_dir_name() const;

	virtual String get_data_path() const;
	virtual String get_config_path() const;
	virtual String get_cache_path() const;
	virtual String get_bundle_resource_dir() const;
	virtual String get_bundle_icon_path() const;

	virtual String get_user_data_dir() const;
	virtual String get_resource_dir() const;

	enum SystemDir {
		SYSTEM_DIR_DESKTOP,
		SYSTEM_DIR_DCIM,
		SYSTEM_DIR_DOCUMENTS,
		SYSTEM_DIR_DOWNLOADS,
		SYSTEM_DIR_MOVIES,
		SYSTEM_DIR_MUSIC,
		SYSTEM_DIR_PICTURES,
		SYSTEM_DIR_RINGTONES,
	};

	virtual String get_system_dir(SystemDir p_dir, bool p_shared_storage = true) const;

	virtual Error move_to_trash(const String &p_path) { return FAILED; }

	virtual int get_exit_code() const;
	// `set_exit_code` should only be used from `SceneTree` (or from a similar
	// level, e.g. from the `Main::start` if leaving without creating a `SceneTree`).
	// For other components, `SceneTree.quit()` should be used instead.
	virtual void set_exit_code(int p_code);

	virtual int get_processor_count() const;
	virtual String get_processor_name() const;
	virtual int get_default_thread_pool_size() const { return get_processor_count(); }

	virtual String get_unique_id() const;

	bool has_feature(const String &p_feature);

	virtual bool is_sandboxed() const;

	void set_has_server_feature_callback(HasServerFeatureCallback p_callback);

	void set_restart_on_exit(bool p_restart, const List<String> &p_restart_arguments);
	bool is_restart_on_exit_set() const;
	List<String> get_restart_on_exit_arguments() const;

	virtual bool request_permission(const String &p_name) { return true; }
	virtual bool request_permissions() { return true; }
	virtual Vector<String> get_granted_permissions() const { return Vector<String>(); }
	virtual void revoke_granted_permissions() {}

	// For recording / measuring benchmark data. Only enabled with tools
	void set_use_benchmark(bool p_use_benchmark);
	bool is_use_benchmark_set();
	void set_benchmark_file(const String &p_benchmark_file);
	String get_benchmark_file();
	virtual void benchmark_begin_measure(const String &p_context, const String &p_what);
	virtual void benchmark_end_measure(const String &p_context, const String &p_what);
	virtual void benchmark_dump();

	virtual Error setup_remote_filesystem(const String &p_server_host, int p_port, const String &p_password, String &r_project_path);

	enum PreferredTextureFormat {
		PREFERRED_TEXTURE_FORMAT_S3TC_BPTC,
		PREFERRED_TEXTURE_FORMAT_ETC2_ASTC
	};

	virtual PreferredTextureFormat get_preferred_texture_format() const;

	// Load GDExtensions specific to this platform.
	// This is invoked by the GDExtensionManager after loading GDExtensions specified by the project.
	virtual void load_platform_gdextensions() const {}

	OS();
	virtual ~OS();
};

#endif // OS_H

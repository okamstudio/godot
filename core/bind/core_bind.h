/**************************************************************************/
/*  core_bind.h                                                           */
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

#ifndef CORE_BIND_H
#define CORE_BIND_H

#include "core/image.h"
#include "core/io/compression.h"
#include "core/io/resource_loader.h"
#include "core/io/resource_saver.h"
#include "core/os/dir_access.h"
#include "core/os/file_access.h"
#include "core/os/os.h"
#include "core/os/semaphore.h"
#include "core/os/thread.h"
#include "core/safe_refcount.h"

class _ResourceLoader : public Object {
	GDCLASS(_ResourceLoader, Object);

protected:
	static void _bind_methods();
	static _ResourceLoader *singleton;

public:
	static _ResourceLoader *get_singleton() { return singleton; }
	Ref<ResourceInteractiveLoader> load_interactive(const String &p_path, const String &p_type_hint = "", bool p_no_cache = false);
	RES load(const String &p_path, const String &p_type_hint = "", bool p_no_cache = false);
	PoolVector<String> get_recognized_extensions_for_type(const String &p_type);
	void set_abort_on_missing_resources(bool p_abort);
	PoolStringArray get_dependencies(const String &p_path);
#ifndef DISABLE_DEPRECATED
	bool has(const String &p_path);
#endif // DISABLE_DEPRECATED
	bool has_cached(const String &p_path);
	bool exists(const String &p_path, const String &p_type_hint = "");

	_ResourceLoader();
};

class _ResourceSaver : public Object {
	GDCLASS(_ResourceSaver, Object);

protected:
	static void _bind_methods();
	static _ResourceSaver *singleton;

public:
	enum SaverFlags {

		FLAG_RELATIVE_PATHS = 1,
		FLAG_BUNDLE_RESOURCES = 2,
		FLAG_CHANGE_PATH = 4,
		FLAG_OMIT_EDITOR_PROPERTIES = 8,
		FLAG_SAVE_BIG_ENDIAN = 16,
		FLAG_COMPRESS = 32,
		FLAG_REPLACE_SUBRESOURCE_PATHS = 64,
	};

	static _ResourceSaver *get_singleton() { return singleton; }

	Error save(const String &p_path, const RES &p_resource, SaverFlags p_flags);
	PoolVector<String> get_recognized_extensions(const RES &p_resource);

	_ResourceSaver();
};

VARIANT_ENUM_CAST(_ResourceSaver::SaverFlags);

class MainLoop;

class _OS : public Object {
	GDCLASS(_OS, Object);

protected:
	static void _bind_methods();
	static _OS *singleton;

public:
	enum VideoDriver {
		VIDEO_DRIVER_GLES3,
		VIDEO_DRIVER_GLES2,
	};

	enum PowerState {
		POWERSTATE_UNKNOWN, // Cannot determine power status.
		POWERSTATE_ON_BATTERY, // Not plugged in, running on the battery.
		POWERSTATE_NO_BATTERY, // Plugged in, no battery available.
		POWERSTATE_CHARGING, // Plugged in, charging battery.
		POWERSTATE_CHARGED // Plugged in, battery charged.
	};

	enum Weekday {
		DAY_SUNDAY,
		DAY_MONDAY,
		DAY_TUESDAY,
		DAY_WEDNESDAY,
		DAY_THURSDAY,
		DAY_FRIDAY,
		DAY_SATURDAY
	};

	enum Month {
		// Start at 1 to follow Windows SYSTEMTIME structure
		// https://msdn.microsoft.com/en-us/library/windows/desktop/ms724950(v=vs.85).aspx
		MONTH_JANUARY = 1,
		MONTH_FEBRUARY,
		MONTH_MARCH,
		MONTH_APRIL,
		MONTH_MAY,
		MONTH_JUNE,
		MONTH_JULY,
		MONTH_AUGUST,
		MONTH_SEPTEMBER,
		MONTH_OCTOBER,
		MONTH_NOVEMBER,
		MONTH_DECEMBER
	};

	enum HandleType {
		APPLICATION_HANDLE, // HINSTANCE, NSApplication*, UIApplication*, JNIEnv* ...
		DISPLAY_HANDLE, // X11::Display* ...
		WINDOW_HANDLE, // HWND, X11::Window*, NSWindow*, UIWindow*, Android activity ...
		WINDOW_VIEW, // HDC, NSView*, UIView*, Android surface ...
		OPENGL_CONTEXT, // HGLRC, X11::GLXContext, NSOpenGLContext*, EGLContext* ...
	};

	enum TTSUtteranceEvent {
		TTS_UTTERANCE_STARTED,
		TTS_UTTERANCE_ENDED,
		TTS_UTTERANCE_CANCELED,
		TTS_UTTERANCE_BOUNDARY,
		TTS_UTTERANCE_MAX,
	};

	void global_menu_add_item(const String &p_menu, const String &p_label, const Variant &p_signal, const Variant &p_meta);
	void global_menu_add_separator(const String &p_menu);
	void global_menu_remove_item(const String &p_menu, int p_idx);
	void global_menu_clear(const String &p_menu);

	bool tts_is_speaking() const;
	bool tts_is_paused() const;
	Array tts_get_voices() const;
	PoolStringArray tts_get_voices_for_language(const String &p_language) const;

	void tts_speak(const String &p_text, const String &p_voice, int p_volume = 50, float p_pitch = 1.f, float p_rate = 1.f, int p_utterance_id = 0, bool p_interrupt = false);
	void tts_pause();
	void tts_resume();
	void tts_stop();

	void tts_set_utterance_callback(TTSUtteranceEvent p_event, Object *p_object, String p_callback);

	Point2 get_mouse_position() const;
	void set_window_title(const String &p_title);
	void set_window_mouse_passthrough(const PoolVector2Array &p_region);
	int get_mouse_button_state() const;

	void set_clipboard(const String &p_text);
	String get_clipboard() const;
	bool has_clipboard() const;

	void set_video_mode(const Size2 &p_size, bool p_fullscreen, bool p_resizeable, int p_screen = 0);
	Size2 get_video_mode(int p_screen = 0) const;
	bool is_video_mode_fullscreen(int p_screen = 0) const;
	bool is_video_mode_resizable(int p_screen = 0) const;
	Array get_fullscreen_mode_list(int p_screen = 0) const;

	virtual int get_video_driver_count() const;
	virtual String get_video_driver_name(VideoDriver p_driver) const;
	virtual VideoDriver get_current_video_driver() const;

	virtual int get_audio_driver_count() const;
	virtual String get_audio_driver_name(int p_driver) const;

	virtual PoolStringArray get_connected_midi_inputs();
	virtual void open_midi_inputs();
	virtual void close_midi_inputs();

	virtual int get_screen_count() const;
	virtual int get_current_screen() const;
	virtual void set_current_screen(int p_screen);
	virtual Point2 get_screen_position(int p_screen = -1) const;
	virtual Size2 get_screen_size(int p_screen = -1) const;
	virtual int get_screen_dpi(int p_screen = -1) const;
	virtual float get_screen_scale(int p_screen = -1) const;
	virtual float get_screen_max_scale() const;
	virtual float get_screen_refresh_rate(int p_screen = -1) const;
	virtual Point2 get_window_position() const;
	virtual void set_window_position(const Point2 &p_position);
	virtual Size2 get_max_window_size() const;
	virtual Size2 get_min_window_size() const;
	virtual Size2 get_window_size() const;
	virtual Size2 get_real_window_size() const;
	virtual Rect2 get_window_safe_area() const;
	virtual Array get_display_cutouts() const;
	virtual void set_max_window_size(const Size2 &p_size);
	virtual void set_min_window_size(const Size2 &p_size);
	virtual void set_window_size(const Size2 &p_size);
	virtual void set_window_fullscreen(bool p_enabled);
	virtual bool is_window_fullscreen() const;
	virtual void set_window_resizable(bool p_enabled);
	virtual bool is_window_resizable() const;
	virtual void set_window_minimized(bool p_enabled);
	virtual bool is_window_minimized() const;
	virtual void set_window_maximized(bool p_enabled);
	virtual bool is_window_maximized() const;
	virtual void set_window_always_on_top(bool p_enabled);
	virtual bool is_window_always_on_top() const;
	virtual bool is_window_focused() const;
	virtual void request_attention();
	virtual void center_window();
	virtual void move_window_to_foreground();

	virtual int64_t get_native_handle(HandleType p_handle_type);

	virtual void set_borderless_window(bool p_borderless);
	virtual bool get_borderless_window() const;

	virtual bool get_window_per_pixel_transparency_enabled() const;
	virtual void set_window_per_pixel_transparency_enabled(bool p_enabled);

	virtual void set_ime_active(const bool p_active);
	virtual void set_ime_position(const Point2 &p_pos);
	virtual Point2 get_ime_selection() const;
	virtual String get_ime_text() const;

	Error native_video_play(String p_path, float p_volume, String p_audio_track, String p_subtitle_track);
	bool native_video_is_playing();
	void native_video_pause();
	void native_video_unpause();
	void native_video_stop();

	void set_low_processor_usage_mode(bool p_enabled);
	bool is_in_low_processor_usage_mode() const;

	void set_low_processor_usage_mode_sleep_usec(int p_usec);
	int get_low_processor_usage_mode_sleep_usec() const;

	String get_executable_path() const;
	String read_string_from_stdin();
	int execute(const String &p_path, const Vector<String> &p_arguments, bool p_blocking = true, Array p_output = Array(), bool p_read_stderr = false, bool p_open_console = false);

	Error kill(int p_pid);
	Error shell_open(String p_uri);

	bool is_process_running(int p_pid) const;
	int get_process_id() const;

	void set_restart_on_exit(bool p_restart, const Vector<String> &p_restart_arguments = Vector<String>());
	bool is_restart_on_exit_set() const;
	Vector<String> get_restart_on_exit_arguments() const;

	bool has_environment(const String &p_var) const;
	String get_environment(const String &p_var) const;
	bool set_environment(const String &p_var, const String &p_value) const;

	String get_name() const;
	Vector<String> get_cmdline_args();

	String get_locale() const;
	String get_locale_language() const;
	String get_latin_keyboard_variant() const;
	int keyboard_get_layout_count() const;
	int keyboard_get_current_layout() const;
	void keyboard_set_current_layout(int p_index);
	String keyboard_get_layout_language(int p_index) const;
	String keyboard_get_layout_name(int p_index) const;
	uint32_t keyboard_get_scancode_from_physical(uint32_t p_scancode) const;

	String get_model_name() const;

	void dump_memory_to_file(const String &p_file);
	void dump_resources_to_file(const String &p_file);

	enum VirtualKeyboardType {
		KEYBOARD_TYPE_DEFAULT,
		KEYBOARD_TYPE_MULTILINE,
		KEYBOARD_TYPE_NUMBER,
		KEYBOARD_TYPE_NUMBER_DECIMAL,
		KEYBOARD_TYPE_PHONE,
		KEYBOARD_TYPE_EMAIL_ADDRESS,
		KEYBOARD_TYPE_PASSWORD,
		KEYBOARD_TYPE_URL
	};

	void _show_virtual_keyboard(const String &p_existing_text = "", bool p_multiline = false) {
		show_virtual_keyboard(p_existing_text, p_multiline ? KEYBOARD_TYPE_MULTILINE : KEYBOARD_TYPE_DEFAULT);
	}

	bool has_virtual_keyboard() const;
	void show_virtual_keyboard(const String &p_existing_text = "", VirtualKeyboardType p_type = KEYBOARD_TYPE_DEFAULT);
	void hide_virtual_keyboard();
	int get_virtual_keyboard_height();

	void print_resources_in_use(bool p_short = false);
	void print_all_resources(const String &p_to_file);
	void print_all_textures_by_size();
	void print_resources_by_type(const Vector<String> &p_types);

	bool has_touchscreen_ui_hint() const;

	bool is_debug_build() const;

	String get_unique_id() const;

	String get_scancode_string(uint32_t p_code) const;
	bool is_scancode_unicode(uint32_t p_unicode) const;
	int find_scancode_from_string(const String &p_code) const;

	void set_use_file_access_save_and_swap(bool p_enable);

	void set_native_icon(const String &p_filename);
	void set_icon(const Ref<Image> &p_icon);

	int get_exit_code() const;
	void set_exit_code(int p_code);
	Dictionary get_date(bool utc) const;
	Dictionary get_time(bool utc) const;
	Dictionary get_datetime(bool utc) const;
	Dictionary get_datetime_from_unix_time(int64_t unix_time_val) const;
	int64_t get_unix_time_from_datetime(Dictionary datetime) const;
	Dictionary get_time_zone_info() const;
	uint64_t get_unix_time() const;
	uint64_t get_system_time_secs() const;
	uint64_t get_system_time_msecs() const;

	uint64_t get_static_memory_usage() const;
	uint64_t get_static_memory_peak_usage() const;
	uint64_t get_dynamic_memory_usage() const;

	void delay_usec(int p_usec) const;
	void delay_msec(int p_msec) const;
	uint64_t get_ticks_msec() const;
	uint64_t get_ticks_usec() const;
	uint32_t get_splash_tick_msec() const;

	bool can_use_threads() const;

	bool can_draw() const;

	bool is_userfs_persistent() const;

	bool is_stdout_verbose() const;

	int get_processor_count() const;
	String get_processor_name() const;

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

	enum ScreenOrientation {

		SCREEN_ORIENTATION_LANDSCAPE,
		SCREEN_ORIENTATION_PORTRAIT,
		SCREEN_ORIENTATION_REVERSE_LANDSCAPE,
		SCREEN_ORIENTATION_REVERSE_PORTRAIT,
		SCREEN_ORIENTATION_SENSOR_LANDSCAPE,
		SCREEN_ORIENTATION_SENSOR_PORTRAIT,
		SCREEN_ORIENTATION_SENSOR,
	};

	String get_system_dir(SystemDir p_dir, bool p_shared_storage = true) const;

	Error move_to_trash(const String &p_path) const;
	String get_user_data_dir() const;
	String get_config_dir() const;
	String get_data_dir() const;
	String get_cache_dir() const;

	void alert(const String &p_alert, const String &p_title = "ALERT!");
	void crash(const String &p_message);

	void set_screen_orientation(ScreenOrientation p_orientation);
	ScreenOrientation get_screen_orientation() const;

	void set_keep_screen_on(bool p_enabled);
	bool is_keep_screen_on() const;

	bool is_ok_left_and_cancel_right() const;

	Error set_thread_name(const String &p_name);
	Thread::ID get_thread_caller_id() const;
	Thread::ID get_main_thread_id() const;

	void set_use_vsync(bool p_enable);
	bool is_vsync_enabled() const;

	void set_vsync_via_compositor(bool p_enable);
	bool is_vsync_via_compositor_enabled() const;

	void set_delta_smoothing(bool p_enabled);
	bool is_delta_smoothing_enabled() const;

	PowerState get_power_state();
	int get_power_seconds_left();
	int get_power_percent_left();

	bool has_feature(const String &p_feature) const;

	bool request_permission(const String &p_name);
	bool request_permissions();
	Vector<String> get_granted_permissions() const;

	int get_tablet_driver_count() const;
	String get_tablet_driver_name(int p_driver) const;
	String get_current_tablet_driver() const;
	void set_current_tablet_driver(const String &p_driver);

	static _OS *get_singleton() { return singleton; }

	_OS();
};

VARIANT_ENUM_CAST(_OS::VideoDriver);
VARIANT_ENUM_CAST(_OS::PowerState);
VARIANT_ENUM_CAST(_OS::Weekday);
VARIANT_ENUM_CAST(_OS::Month);
VARIANT_ENUM_CAST(_OS::VirtualKeyboardType);
VARIANT_ENUM_CAST(_OS::SystemDir);
VARIANT_ENUM_CAST(_OS::ScreenOrientation);
VARIANT_ENUM_CAST(_OS::HandleType);
VARIANT_ENUM_CAST(_OS::TTSUtteranceEvent);

class _Geometry : public Object {
	GDCLASS(_Geometry, Object);

	static _Geometry *singleton;

protected:
	static void _bind_methods();

public:
	static _Geometry *get_singleton();
	PoolVector<Plane> build_box_planes(const Vector3 &p_extents);
	PoolVector<Plane> build_cylinder_planes(float p_radius, float p_height, int p_sides, Vector3::Axis p_axis = Vector3::AXIS_Z);
	PoolVector<Plane> build_capsule_planes(float p_radius, float p_height, int p_sides, int p_lats, Vector3::Axis p_axis = Vector3::AXIS_Z);
	Variant segment_intersects_segment_2d(const Vector2 &p_from_a, const Vector2 &p_to_a, const Vector2 &p_from_b, const Vector2 &p_to_b);
	Variant line_intersects_line_2d(const Vector2 &p_from_a, const Vector2 &p_dir_a, const Vector2 &p_from_b, const Vector2 &p_dir_b);
	PoolVector<Vector2> get_closest_points_between_segments_2d(const Vector2 &p1, const Vector2 &q1, const Vector2 &p2, const Vector2 &q2);
	PoolVector<Vector3> get_closest_points_between_segments(const Vector3 &p1, const Vector3 &p2, const Vector3 &q1, const Vector3 &q2);
	Vector2 get_closest_point_to_segment_2d(const Vector2 &p_point, const Vector2 &p_a, const Vector2 &p_b);
	Vector3 get_closest_point_to_segment(const Vector3 &p_point, const Vector3 &p_a, const Vector3 &p_b);
	Vector2 get_closest_point_to_segment_uncapped_2d(const Vector2 &p_point, const Vector2 &p_a, const Vector2 &p_b);
	Vector3 get_closest_point_to_segment_uncapped(const Vector3 &p_point, const Vector3 &p_a, const Vector3 &p_b);
	Variant ray_intersects_triangle(const Vector3 &p_from, const Vector3 &p_dir, const Vector3 &p_v0, const Vector3 &p_v1, const Vector3 &p_v2);
	Variant segment_intersects_triangle(const Vector3 &p_from, const Vector3 &p_to, const Vector3 &p_v0, const Vector3 &p_v1, const Vector3 &p_v2);
	bool point_is_inside_triangle(const Vector2 &s, const Vector2 &a, const Vector2 &b, const Vector2 &c) const;

	PoolVector<Vector3> segment_intersects_sphere(const Vector3 &p_from, const Vector3 &p_to, const Vector3 &p_sphere_pos, real_t p_sphere_radius);
	PoolVector<Vector3> segment_intersects_cylinder(const Vector3 &p_from, const Vector3 &p_to, float p_height, float p_radius);
	PoolVector<Vector3> segment_intersects_convex(const Vector3 &p_from, const Vector3 &p_to, const Vector<Plane> &p_planes);
	bool is_point_in_circle(const Vector2 &p_point, const Vector2 &p_circle_pos, real_t p_circle_radius);
	real_t segment_intersects_circle(const Vector2 &p_from, const Vector2 &p_to, const Vector2 &p_circle_pos, real_t p_circle_radius);
	int get_uv84_normal_bit(const Vector3 &p_vector);

	bool is_polygon_clockwise(const Vector<Vector2> &p_polygon);
	bool is_point_in_polygon(const Point2 &p_point, const Vector<Vector2> &p_polygon);
	Vector<int> triangulate_polygon(const Vector<Vector2> &p_polygon);
	Vector<int> triangulate_delaunay_2d(const Vector<Vector2> &p_points);
	Vector<Point2> convex_hull_2d(const Vector<Point2> &p_points);
	Vector<Vector3> clip_polygon(const Vector<Vector3> &p_points, const Plane &p_plane);

	enum PolyBooleanOperation {
		OPERATION_UNION,
		OPERATION_DIFFERENCE,
		OPERATION_INTERSECTION,
		OPERATION_XOR
	};
	// 2D polygon boolean operations.
	Array merge_polygons_2d(const Vector<Vector2> &p_polygon_a, const Vector<Vector2> &p_polygon_b); // Union (add).
	Array clip_polygons_2d(const Vector<Vector2> &p_polygon_a, const Vector<Vector2> &p_polygon_b); // Difference (subtract).
	Array intersect_polygons_2d(const Vector<Vector2> &p_polygon_a, const Vector<Vector2> &p_polygon_b); // Common area (multiply).
	Array exclude_polygons_2d(const Vector<Vector2> &p_polygon_a, const Vector<Vector2> &p_polygon_b); // All but common area (xor).

	// 2D polyline vs polygon operations.
	Array clip_polyline_with_polygon_2d(const Vector<Vector2> &p_polyline, const Vector<Vector2> &p_polygon); // Cut.
	Array intersect_polyline_with_polygon_2d(const Vector<Vector2> &p_polyline, const Vector<Vector2> &p_polygon); // Chop.

	// 2D offset polygons/polylines.
	enum PolyJoinType {
		JOIN_SQUARE,
		JOIN_ROUND,
		JOIN_MITER
	};
	enum PolyEndType {
		END_POLYGON,
		END_JOINED,
		END_BUTT,
		END_SQUARE,
		END_ROUND
	};
	Array offset_polygon_2d(const Vector<Vector2> &p_polygon, real_t p_delta, PolyJoinType p_join_type = JOIN_SQUARE);
	Array offset_polyline_2d(const Vector<Vector2> &p_polygon, real_t p_delta, PolyJoinType p_join_type = JOIN_SQUARE, PolyEndType p_end_type = END_SQUARE);

	Array minkowski_sum_polygon_2d(const Vector<Vector2> &p_polygon_pattern, const Vector<Vector2> &p_polygon_path);
	Array minkowski_diff_polygon_2d(const Vector<Vector2> &p_polygon_pattern, const Vector<Vector2> &p_polygon_path);
	Array minkowski_sum_polyline_2d(const Vector<Vector2> &p_polygon_pattern, const Vector<Vector2> &p_polyline_path);

	Dictionary make_atlas(const Vector<Size2> &p_rects);

	_Geometry();
};

VARIANT_ENUM_CAST(_Geometry::PolyBooleanOperation);
VARIANT_ENUM_CAST(_Geometry::PolyJoinType);
VARIANT_ENUM_CAST(_Geometry::PolyEndType);

class _File : public Reference {
	GDCLASS(_File, Reference);
	FileAccess *f;
	bool eswap;

protected:
	static void _bind_methods();

public:
	enum ModeFlags {

		READ = 1,
		WRITE = 2,
		READ_WRITE = 3,
		WRITE_READ = 7,
	};

	enum CompressionMode {
		COMPRESSION_FASTLZ = Compression::MODE_FASTLZ,
		COMPRESSION_DEFLATE = Compression::MODE_DEFLATE,
		COMPRESSION_ZSTD = Compression::MODE_ZSTD,
		COMPRESSION_GZIP = Compression::MODE_GZIP
	};

	Error open_encrypted(const String &p_path, ModeFlags p_mode_flags, const Vector<uint8_t> &p_key);
	Error open_encrypted_pass(const String &p_path, ModeFlags p_mode_flags, const String &p_pass);
	Error open_compressed(const String &p_path, ModeFlags p_mode_flags, CompressionMode p_compress_mode = COMPRESSION_FASTLZ);

	Error open(const String &p_path, ModeFlags p_mode_flags); // open a file.
	void flush(); // Flush a file (write its buffer to disk).
	void close(); // Close a file.
	bool is_open() const; // True when file is open.

	String get_path() const; // Returns the path for the current open file.
	String get_path_absolute() const; // Returns the absolute path for the current open file.

	void seek(int64_t p_position); // Seek to a given position.
	void seek_end(int64_t p_position = 0); // Seek from the end of file.
	uint64_t get_position() const; // Get position in the file.
	uint64_t get_len() const; // Get size of the file.

	bool eof_reached() const; // Reading passed EOF.

	uint8_t get_8() const; // Get a byte.
	uint16_t get_16() const; // Get 16 bits uint.
	uint32_t get_32() const; // Get 32 bits uint.
	uint64_t get_64() const; // Get 64 bits uint.

	float get_float() const;
	double get_double() const;
	real_t get_real() const;

	Variant get_var(bool p_allow_objects = false) const;

	PoolVector<uint8_t> get_buffer(int64_t p_length) const; // Get an array of bytes.
	String get_line() const;
	Vector<String> get_csv_line(const String &p_delim = ",") const;
	String get_as_text(bool p_skip_cr = true) const; // Skip CR by default for compat.
	String get_md5(const String &p_path) const;
	String get_sha256(const String &p_path) const;

	/* Use this for files WRITTEN in _big_ endian machines (ie, amiga/mac).
	 * It's not about the current CPU type but file formats.
	 * This flags get reset to false (little endian) on each open.
	 */

	void set_endian_swap(bool p_swap);
	bool get_endian_swap();

	Error get_error() const; // Get last error.

	void store_8(uint8_t p_dest); // Store a byte.
	void store_16(uint16_t p_dest); // Store 16 bits uint.
	void store_32(uint32_t p_dest); // Store 32 bits uint.
	void store_64(uint64_t p_dest); // Store 64 bits uint.

	void store_float(float p_dest);
	void store_double(double p_dest);
	void store_real(real_t p_real);

	void store_string(const String &p_string);
	void store_line(const String &p_string);
	void store_csv_line(const Vector<String> &p_values, const String &p_delim = ",");

	virtual void store_pascal_string(const String &p_string);
	virtual String get_pascal_string();

	void store_buffer(const PoolVector<uint8_t> &p_buffer); // Store an array of bytes.

	void store_var(const Variant &p_var, bool p_full_objects = false);

	bool file_exists(const String &p_name) const; // Return true if a file exists.

	uint64_t get_modified_time(const String &p_file) const;

	_File();
	virtual ~_File();
};

VARIANT_ENUM_CAST(_File::ModeFlags);
VARIANT_ENUM_CAST(_File::CompressionMode);

class _Directory : public Reference {
	GDCLASS(_Directory, Reference);
	DirAccess *d;

protected:
	static void _bind_methods();

public:
	Error open(const String &p_path);

	Error list_dir_begin(bool p_skip_navigational = false, bool p_skip_hidden = false); // This starts dir listing.
	String get_next();
	bool current_is_dir() const;

	void list_dir_end();

	int get_drive_count();
	String get_drive(int p_drive);
	int get_current_drive();

	Error change_dir(String p_dir); // Can be relative or absolute, return false on success.
	String get_current_dir(); // Return current dir location.

	Error make_dir(String p_dir);
	Error make_dir_recursive(String p_dir);

	bool file_exists(String p_file);
	bool dir_exists(String p_dir);

	uint64_t get_space_left();

	Error copy(String p_from, String p_to);
	Error rename(String p_from, String p_to);
	Error remove(String p_name);

	_Directory();
	virtual ~_Directory();

private:
	bool _list_skip_navigational;
	bool _list_skip_hidden;
};

class _Marshalls : public Object {
	GDCLASS(_Marshalls, Object);

	static _Marshalls *singleton;

protected:
	static void _bind_methods();

public:
	static _Marshalls *get_singleton();

	String variant_to_base64(const Variant &p_var, bool p_full_objects = false);
	Variant base64_to_variant(const String &p_str, bool p_allow_objects = false);

	String raw_to_base64(const PoolVector<uint8_t> &p_arr);
	PoolVector<uint8_t> base64_to_raw(const String &p_str);

	String utf8_to_base64(const String &p_str);
	String base64_to_utf8(const String &p_str);

	_Marshalls() { singleton = this; }
	~_Marshalls() { singleton = nullptr; }
};

class _Mutex : public Reference {
	GDCLASS(_Mutex, Reference);
	Mutex mutex;

	static void _bind_methods();

public:
	void lock();
	Error try_lock();
	void unlock();
};

class _Semaphore : public Reference {
	GDCLASS(_Semaphore, Reference);
	Semaphore semaphore;

	static void _bind_methods();

public:
	Error wait();
	Error try_wait();
	Error post();
};

class _Thread : public Reference {
	GDCLASS(_Thread, Reference);

protected:
	Variant ret;
	Variant userdata;
	SafeFlag running;
	ObjectID target_instance_id;
	StringName target_method;
	Thread thread;
	static void _bind_methods();
	static void _start_func(void *ud);

public:
	enum Priority {

		PRIORITY_LOW,
		PRIORITY_NORMAL,
		PRIORITY_HIGH,
		PRIORITY_MAX
	};

	Error start(Object *p_instance, const StringName &p_method, const Variant &p_userdata = Variant(), Priority p_priority = PRIORITY_NORMAL);
	String get_id() const;
	bool is_active() const;
	bool is_alive() const;
	Variant wait_to_finish();

	_Thread();
	~_Thread();
};

VARIANT_ENUM_CAST(_Thread::Priority);

class _ClassDB : public Object {
	GDCLASS(_ClassDB, Object);

protected:
	static void _bind_methods();

public:
	PoolStringArray get_class_list() const;
	PoolStringArray get_inheriters_from_class(const StringName &p_class) const;
	StringName get_parent_class(const StringName &p_class) const;
	bool class_exists(const StringName &p_class) const;
	bool is_parent_class(const StringName &p_class, const StringName &p_inherits) const;
	bool can_instance(const StringName &p_class) const;
	Variant instance(const StringName &p_class) const;

	bool has_signal(StringName p_class, StringName p_signal) const;
	Dictionary get_signal(StringName p_class, StringName p_signal) const;
	Array get_signal_list(StringName p_class, bool p_no_inheritance = false) const;

	Array get_property_list(StringName p_class, bool p_no_inheritance = false) const;
	Variant get_property(Object *p_object, const StringName &p_property) const;
	Error set_property(Object *p_object, const StringName &p_property, const Variant &p_value) const;

	bool has_method(StringName p_class, StringName p_method, bool p_no_inheritance = false) const;

	Array get_method_list(StringName p_class, bool p_no_inheritance = false) const;

	PoolStringArray get_integer_constant_list(const StringName &p_class, bool p_no_inheritance = false) const;
	bool has_integer_constant(const StringName &p_class, const StringName &p_name) const;
	int get_integer_constant(const StringName &p_class, const StringName &p_name) const;
	StringName get_category(const StringName &p_node) const;

	bool has_enum(const StringName &p_class, const StringName &p_name, bool p_no_inheritance = false) const;
	PoolStringArray get_enum_list(const StringName &p_class, bool p_no_inheritance = false) const;
	PoolStringArray get_enum_constants(const StringName &p_class, const StringName &p_enum, bool p_no_inheritance = false) const;
	StringName get_integer_constant_enum(const StringName &p_class, const StringName &p_name, bool p_no_inheritance = false) const;

	bool is_class_enabled(StringName p_class) const;

	_ClassDB();
	~_ClassDB();
};

class _Engine : public Object {
	GDCLASS(_Engine, Object);

protected:
	static void _bind_methods();
	static _Engine *singleton;

public:
	static _Engine *get_singleton() { return singleton; }
	void set_iterations_per_second(int p_ips);
	int get_iterations_per_second() const;

	void set_physics_jitter_fix(float p_threshold);
	float get_physics_jitter_fix() const;
	float get_physics_interpolation_fraction() const;

	void set_target_fps(int p_fps);
	int get_target_fps() const;

	float get_frames_per_second() const;
	uint64_t get_physics_frames() const;
	uint64_t get_idle_frames() const;

	int get_frames_drawn();

	void set_time_scale(float p_scale);
	float get_time_scale();

	MainLoop *get_main_loop() const;

	Dictionary get_version_info() const;
	Dictionary get_author_info() const;
	Array get_copyright_info() const;
	Dictionary get_donor_info() const;
	Dictionary get_license_info() const;
	String get_license_text() const;

	bool is_in_physics_frame() const;

	bool has_singleton(const String &p_name) const;
	Object *get_singleton_object(const String &p_name) const;

	void set_editor_hint(bool p_enabled);
	bool is_editor_hint() const;

	void set_print_error_messages(bool p_enabled);
	bool is_printing_error_messages() const;

	_Engine();
};

class _JSON;

class JSONParseResult : public Reference {
	GDCLASS(JSONParseResult, Reference);

	friend class _JSON;

	Error error;
	String error_string;
	int error_line;

	Variant result;

protected:
	static void _bind_methods();

public:
	void set_error(Error p_error);
	Error get_error() const;

	void set_error_string(const String &p_error_string);
	String get_error_string() const;

	void set_error_line(int p_error_line);
	int get_error_line() const;

	void set_result(const Variant &p_result);
	Variant get_result() const;

	JSONParseResult() :
			error_line(-1) {}
};

class _JSON : public Object {
	GDCLASS(_JSON, Object);

protected:
	static void _bind_methods();
	static _JSON *singleton;

public:
	static _JSON *get_singleton() { return singleton; }

	String print(const Variant &p_value, const String &p_indent = "", bool p_sort_keys = false);
	Ref<JSONParseResult> parse(const String &p_json);

	_JSON();
};

#endif // CORE_BIND_H

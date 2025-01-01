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

#include "core/debugger/engine_profiler.h"
#include "core/io/resource_loader.h"
#include "core/io/resource_saver.h"
#include "core/os/semaphore.h"
#include "core/os/thread.h"
#include "core/templates/safe_refcount.h"

class MainLoop;
template <typename T>
class TypedArray;

namespace core_bind {

class ResourceLoader : public Object {
	GDCLASS(ResourceLoader, Object);

protected:
	static void _bind_methods();
	static ResourceLoader *singleton;

public:
	enum ThreadLoadStatus {
		THREAD_LOAD_INVALID_RESOURCE,
		THREAD_LOAD_IN_PROGRESS,
		THREAD_LOAD_FAILED,
		THREAD_LOAD_LOADED
	};

	enum CacheMode {
		CACHE_MODE_IGNORE,
		CACHE_MODE_REUSE,
		CACHE_MODE_REPLACE,
		CACHE_MODE_IGNORE_DEEP,
		CACHE_MODE_REPLACE_DEEP,
	};

	static ResourceLoader *get_singleton() { return singleton; }

	Error load_threaded_request(const String &p_path, const String &p_type_hint = "", bool p_use_sub_threads = false, CacheMode p_cache_mode = CACHE_MODE_REUSE);
	ThreadLoadStatus load_threaded_get_status(const String &p_path, Array r_progress = ClassDB::default_array_arg);
	Ref<Resource> load_threaded_get(const String &p_path);

	Ref<Resource> load(const String &p_path, const String &p_type_hint = "", CacheMode p_cache_mode = CACHE_MODE_REUSE);
	Vector<String> get_recognized_extensions_for_type(const String &p_type);
	void add_resource_format_loader(Ref<ResourceFormatLoader> p_format_loader, bool p_at_front);
	void remove_resource_format_loader(Ref<ResourceFormatLoader> p_format_loader);
	void set_abort_on_missing_resources(bool p_abort);
	PackedStringArray get_dependencies(const String &p_path);
	bool has_cached(const String &p_path);
	Ref<Resource> get_cached_ref(const String &p_path);
	bool exists(const String &p_path, const String &p_type_hint = "");
	ResourceUID::ID get_resource_uid(const String &p_path);

	Vector<String> list_directory(const String &p_directory);

	ResourceLoader() { singleton = this; }
};

class ResourceSaver : public Object {
	GDCLASS(ResourceSaver, Object);

protected:
	static void _bind_methods();
	static ResourceSaver *singleton;

public:
	enum SaverFlags {
		FLAG_NONE = 0,
		FLAG_RELATIVE_PATHS = 1,
		FLAG_BUNDLE_RESOURCES = 2,
		FLAG_CHANGE_PATH = 4,
		FLAG_OMIT_EDITOR_PROPERTIES = 8,
		FLAG_SAVE_BIG_ENDIAN = 16,
		FLAG_COMPRESS = 32,
		FLAG_REPLACE_SUBRESOURCE_PATHS = 64,
	};

	static ResourceSaver *get_singleton() { return singleton; }

	Error save(const Ref<Resource> &p_resource, const String &p_path, BitField<SaverFlags> p_flags);
	Vector<String> get_recognized_extensions(const Ref<Resource> &p_resource);
	void add_resource_format_saver(Ref<ResourceFormatSaver> p_format_saver, bool p_at_front);
	void remove_resource_format_saver(Ref<ResourceFormatSaver> p_format_saver);

	ResourceUID::ID get_resource_id_for_path(const String &p_path, bool p_generate = false);

	ResourceSaver() { singleton = this; }
};

class OS : public Object {
	GDCLASS(OS, Object);

	mutable HashMap<String, bool> feature_cache;

protected:
	static void _bind_methods();
	static OS *singleton;

#ifndef DISABLE_DEPRECATED
	Dictionary _execute_with_pipe_bind_compat_94434(const String &p_path, const Vector<String> &p_arguments);

	String _read_string_from_stdin_bind_compat_91201();
	static void _bind_compatibility_methods();
#endif

public:
	enum RenderingDriver {
		RENDERING_DRIVER_VULKAN,
		RENDERING_DRIVER_OPENGL3,
		RENDERING_DRIVER_D3D12,
		RENDERING_DRIVER_METAL,
	};

	PackedByteArray get_entropy(int p_bytes);
	String get_system_ca_certificates();

	enum StdHandleType {
		STD_HANDLE_INVALID,
		STD_HANDLE_CONSOLE,
		STD_HANDLE_FILE,
		STD_HANDLE_PIPE,
		STD_HANDLE_UNKNOWN,
	};

	virtual PackedStringArray get_connected_midi_inputs();
	virtual void open_midi_inputs();
	virtual void close_midi_inputs();

	void set_low_processor_usage_mode(bool p_enabled);
	bool is_in_low_processor_usage_mode() const;

	void set_low_processor_usage_mode_sleep_usec(int p_usec);
	int get_low_processor_usage_mode_sleep_usec() const;

	void set_delta_smoothing(bool p_enabled);
	bool is_delta_smoothing_enabled() const;

	void alert(const String &p_alert, const String &p_title = "ALERT!");
	void crash(const String &p_message);

	Vector<String> get_system_fonts() const;
	String get_system_font_path(const String &p_font_name, int p_weight = 400, int p_stretch = 100, bool p_italic = false) const;
	Vector<String> get_system_font_path_for_text(const String &p_font_name, const String &p_text, const String &p_locale = String(), const String &p_script = String(), int p_weight = 400, int p_stretch = 100, bool p_italic = false) const;
	String get_executable_path() const;

	String read_string_from_stdin(int64_t p_buffer_size = 1024);
	PackedByteArray read_buffer_from_stdin(int64_t p_buffer_size = 1024);
	StdHandleType get_stdin_type() const;
	StdHandleType get_stdout_type() const;
	StdHandleType get_stderr_type() const;

	int execute(const String &p_path, const Vector<String> &p_arguments, Array r_output = ClassDB::default_array_arg, bool p_read_stderr = false, bool p_open_console = false);
	Dictionary execute_with_pipe(const String &p_path, const Vector<String> &p_arguments, bool p_blocking = true);
	int create_process(const String &p_path, const Vector<String> &p_arguments, bool p_open_console = false);
	int create_instance(const Vector<String> &p_arguments);
	Error kill(int p_pid);
	Error shell_open(const String &p_uri);
	Error shell_show_in_file_manager(const String &p_path, bool p_open_folder = true);

	bool is_process_running(int p_pid) const;
	int get_process_exit_code(int p_pid) const;
	int get_process_id() const;

	void set_restart_on_exit(bool p_restart, const Vector<String> &p_restart_arguments = Vector<String>());
	bool is_restart_on_exit_set() const;
	Vector<String> get_restart_on_exit_arguments() const;

	bool has_environment(const String &p_var) const;
	String get_environment(const String &p_var) const;
	void set_environment(const String &p_var, const String &p_value) const;
	void unset_environment(const String &p_var) const;

	String get_name() const;
	String get_distribution_name() const;
	String get_version() const;
	String get_version_alias() const;
	Vector<String> get_cmdline_args();
	Vector<String> get_cmdline_user_args();

	Vector<String> get_video_adapter_driver_info() const;

	String get_locale() const;
	String get_locale_language() const;

	String get_model_name() const;

	bool is_debug_build() const;

	String get_unique_id() const;

	String get_keycode_string(Key p_code) const;
	bool is_keycode_unicode(char32_t p_unicode) const;
	Key find_keycode_from_string(const String &p_code) const;

	void set_use_file_access_save_and_swap(bool p_enable);

	uint64_t get_static_memory_usage() const;
	uint64_t get_static_memory_peak_usage() const;
	Dictionary get_memory_info() const;

	void delay_usec(int p_usec) const;
	void delay_msec(int p_msec) const;
	uint64_t get_ticks_msec() const;
	uint64_t get_ticks_usec() const;

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

	String get_system_dir(SystemDir p_dir, bool p_shared_storage = true) const;

	Error move_to_trash(const String &p_path) const;
	String get_user_data_dir() const;
	String get_config_dir() const;
	String get_data_dir() const;
	String get_cache_dir() const;
	String get_temp_dir() const;

	Error set_thread_name(const String &p_name);
	::Thread::ID get_thread_caller_id() const;
	::Thread::ID get_main_thread_id() const;

	bool has_feature(const String &p_feature) const;
	bool is_sandboxed() const;

	bool request_permission(const String &p_name);
	bool request_permissions();
	Vector<String> get_granted_permissions() const;
	void revoke_granted_permissions();

	static OS *get_singleton() { return singleton; }

	OS() { singleton = this; }
};

class Geometry2D : public Object {
	GDCLASS(Geometry2D, Object);

	static Geometry2D *singleton;

protected:
	static void _bind_methods();

public:
	static Geometry2D *get_singleton();
	Variant segment_intersects_segment(const Vector2 &p_from_a, const Vector2 &p_to_a, const Vector2 &p_from_b, const Vector2 &p_to_b);
	Variant line_intersects_line(const Vector2 &p_from_a, const Vector2 &p_dir_a, const Vector2 &p_from_b, const Vector2 &p_dir_b);
	Vector<Vector2> get_closest_points_between_segments(const Vector2 &p1, const Vector2 &q1, const Vector2 &p2, const Vector2 &q2);
	Vector2 get_closest_point_to_segment(const Vector2 &p_point, const Vector2 &p_a, const Vector2 &p_b);
	Vector2 get_closest_point_to_segment_uncapped(const Vector2 &p_point, const Vector2 &p_a, const Vector2 &p_b);
	bool point_is_inside_triangle(const Vector2 &s, const Vector2 &a, const Vector2 &b, const Vector2 &c) const;

	bool is_point_in_circle(const Vector2 &p_point, const Vector2 &p_circle_pos, real_t p_circle_radius);
	real_t segment_intersects_circle(const Vector2 &p_from, const Vector2 &p_to, const Vector2 &p_circle_pos, real_t p_circle_radius);

	bool is_polygon_clockwise(const Vector<Vector2> &p_polygon);
	bool is_point_in_polygon(const Point2 &p_point, const Vector<Vector2> &p_polygon);
	Vector<int> triangulate_polygon(const Vector<Vector2> &p_polygon);
	Vector<int> triangulate_delaunay(const Vector<Vector2> &p_points);
	Vector<Point2> convex_hull(const Vector<Point2> &p_points);
	TypedArray<PackedVector2Array> decompose_polygon_in_convex(const Vector<Vector2> &p_polygon);

	enum PolyBooleanOperation {
		OPERATION_UNION,
		OPERATION_DIFFERENCE,
		OPERATION_INTERSECTION,
		OPERATION_XOR
	};
	// 2D polygon boolean operations.
	TypedArray<PackedVector2Array> merge_polygons(const Vector<Vector2> &p_polygon_a, const Vector<Vector2> &p_polygon_b); // Union (add).
	TypedArray<PackedVector2Array> clip_polygons(const Vector<Vector2> &p_polygon_a, const Vector<Vector2> &p_polygon_b); // Difference (subtract).
	TypedArray<PackedVector2Array> intersect_polygons(const Vector<Vector2> &p_polygon_a, const Vector<Vector2> &p_polygon_b); // Common area (multiply).
	TypedArray<PackedVector2Array> exclude_polygons(const Vector<Vector2> &p_polygon_a, const Vector<Vector2> &p_polygon_b); // All but common area (xor).

	// 2D polyline vs polygon operations.
	TypedArray<PackedVector2Array> clip_polyline_with_polygon(const Vector<Vector2> &p_polyline, const Vector<Vector2> &p_polygon); // Cut.
	TypedArray<PackedVector2Array> intersect_polyline_with_polygon(const Vector<Vector2> &p_polyline, const Vector<Vector2> &p_polygon); // Chop.

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
	TypedArray<PackedVector2Array> offset_polygon(const Vector<Vector2> &p_polygon, real_t p_delta, PolyJoinType p_join_type = JOIN_SQUARE);
	TypedArray<PackedVector2Array> offset_polyline(const Vector<Vector2> &p_polygon, real_t p_delta, PolyJoinType p_join_type = JOIN_SQUARE, PolyEndType p_end_type = END_SQUARE);

	Dictionary make_atlas(const Vector<Size2> &p_rects);

	TypedArray<Point2i> bresenham_line(const Point2i &p_from, const Point2i &p_to);

	Geometry2D() { singleton = this; }
};

class Geometry3D : public Object {
	GDCLASS(Geometry3D, Object);

	static Geometry3D *singleton;

protected:
	static void _bind_methods();

public:
	static Geometry3D *get_singleton();
	Vector<Vector3> compute_convex_mesh_points(const TypedArray<Plane> &p_planes);
	TypedArray<Plane> build_box_planes(const Vector3 &p_extents);
	TypedArray<Plane> build_cylinder_planes(float p_radius, float p_height, int p_sides, Vector3::Axis p_axis = Vector3::AXIS_Z);
	TypedArray<Plane> build_capsule_planes(float p_radius, float p_height, int p_sides, int p_lats, Vector3::Axis p_axis = Vector3::AXIS_Z);
	Vector<Vector3> get_closest_points_between_segments(const Vector3 &p1, const Vector3 &p2, const Vector3 &q1, const Vector3 &q2);
	Vector3 get_closest_point_to_segment(const Vector3 &p_point, const Vector3 &p_a, const Vector3 &p_b);
	Vector3 get_closest_point_to_segment_uncapped(const Vector3 &p_point, const Vector3 &p_a, const Vector3 &p_b);
	Vector3 get_triangle_barycentric_coords(const Vector3 &p_point, const Vector3 &p_v0, const Vector3 &p_v1, const Vector3 &p_v2);
	Variant ray_intersects_triangle(const Vector3 &p_from, const Vector3 &p_dir, const Vector3 &p_v0, const Vector3 &p_v1, const Vector3 &p_v2);
	Variant segment_intersects_triangle(const Vector3 &p_from, const Vector3 &p_to, const Vector3 &p_v0, const Vector3 &p_v1, const Vector3 &p_v2);

	Vector<Vector3> segment_intersects_sphere(const Vector3 &p_from, const Vector3 &p_to, const Vector3 &p_sphere_pos, real_t p_sphere_radius);
	Vector<Vector3> segment_intersects_cylinder(const Vector3 &p_from, const Vector3 &p_to, float p_height, float p_radius);
	Vector<Vector3> segment_intersects_convex(const Vector3 &p_from, const Vector3 &p_to, const TypedArray<Plane> &p_planes);

	Vector<Vector3> clip_polygon(const Vector<Vector3> &p_points, const Plane &p_plane);
	Vector<int32_t> tetrahedralize_delaunay(const Vector<Vector3> &p_points);

	Geometry3D() { singleton = this; }
};

class Marshalls : public Object {
	GDCLASS(Marshalls, Object);

	static Marshalls *singleton;

protected:
	static void _bind_methods();

public:
	static Marshalls *get_singleton();

	String variant_to_base64(const Variant &p_var, bool p_full_objects = false);
	Variant base64_to_variant(const String &p_str, bool p_allow_objects = false);

	String raw_to_base64(const Vector<uint8_t> &p_arr);
	Vector<uint8_t> base64_to_raw(const String &p_str);

	String utf8_to_base64(const String &p_str);
	String base64_to_utf8(const String &p_str);

	Marshalls() { singleton = this; }
	~Marshalls() { singleton = nullptr; }
};

class Mutex : public RefCounted {
	GDCLASS(Mutex, RefCounted);
	::Mutex mutex;

	static void _bind_methods();

public:
	void lock();
	bool try_lock();
	void unlock();
};

class Semaphore : public RefCounted {
	GDCLASS(Semaphore, RefCounted);
	::Semaphore semaphore;

protected:
	static void _bind_methods();
#ifndef DISABLE_DEPRECATED
	void _post_bind_compat_93605();
	static void _bind_compatibility_methods();
#endif // DISABLE_DEPRECATED

public:
	void wait();
	bool try_wait();
	void post(int p_count = 1);
};

class Thread : public RefCounted {
	GDCLASS(Thread, RefCounted);

protected:
	Variant ret;
	SafeFlag running;
	Callable target_callable;
	::Thread thread;
	static void _bind_methods();
	static void _start_func(void *ud);

public:
	enum Priority {
		PRIORITY_LOW,
		PRIORITY_NORMAL,
		PRIORITY_HIGH,
		PRIORITY_MAX
	};

	Error start(const Callable &p_callable, Priority p_priority = PRIORITY_NORMAL);
	String get_id() const;
	bool is_started() const;
	bool is_alive() const;
	Variant wait_to_finish();

	static void set_thread_safety_checks_enabled(bool p_enabled);
};

namespace special {

class ClassDB : public Object {
	GDCLASS(ClassDB, Object);

protected:
	static void _bind_methods();

public:
	enum APIType {
		API_CORE,
		API_EDITOR,
		API_EXTENSION,
		API_EDITOR_EXTENSION,
		API_NONE,
	};

	PackedStringArray get_class_list() const;
	PackedStringArray get_inheriters_from_class(const StringName &p_class) const;
	StringName get_parent_class(const StringName &p_class) const;
	bool class_exists(const StringName &p_class) const;
	bool is_parent_class(const StringName &p_class, const StringName &p_inherits) const;
	bool can_instantiate(const StringName &p_class) const;
	Variant instantiate(const StringName &p_class) const;

	APIType class_get_api_type(const StringName &p_class) const;
	bool class_has_signal(const StringName &p_class, const StringName &p_signal) const;
	Dictionary class_get_signal(const StringName &p_class, const StringName &p_signal) const;
	TypedArray<Dictionary> class_get_signal_list(const StringName &p_class, bool p_no_inheritance = false) const;

	TypedArray<Dictionary> class_get_property_list(const StringName &p_class, bool p_no_inheritance = false) const;
	StringName class_get_property_getter(const StringName &p_class, const StringName &p_property);
	StringName class_get_property_setter(const StringName &p_class, const StringName &p_property);
	Variant class_get_property(Object *p_object, const StringName &p_property) const;
	Error class_set_property(Object *p_object, const StringName &p_property, const Variant &p_value) const;

	Variant class_get_property_default_value(const StringName &p_class, const StringName &p_property) const;

	bool class_has_method(const StringName &p_class, const StringName &p_method, bool p_no_inheritance = false) const;

	int class_get_method_argument_count(const StringName &p_class, const StringName &p_method, bool p_no_inheritance = false) const;

	TypedArray<Dictionary> class_get_method_list(const StringName &p_class, bool p_no_inheritance = false) const;
	Variant class_call_static(const Variant **p_arguments, int p_argcount, Callable::CallError &r_call_error);

	PackedStringArray class_get_integer_constant_list(const StringName &p_class, bool p_no_inheritance = false) const;
	bool class_has_integer_constant(const StringName &p_class, const StringName &p_name) const;
	int64_t class_get_integer_constant(const StringName &p_class, const StringName &p_name) const;

	bool class_has_enum(const StringName &p_class, const StringName &p_name, bool p_no_inheritance = false) const;
	PackedStringArray class_get_enum_list(const StringName &p_class, bool p_no_inheritance = false) const;
	PackedStringArray class_get_enum_constants(const StringName &p_class, const StringName &p_enum, bool p_no_inheritance = false) const;
	StringName class_get_integer_constant_enum(const StringName &p_class, const StringName &p_name, bool p_no_inheritance = false) const;

	bool is_class_enum_bitfield(const StringName &p_class, const StringName &p_enum, bool p_no_inheritance = false) const;

	bool is_class_enabled(const StringName &p_class) const;

#ifdef TOOLS_ENABLED
	virtual void get_argument_options(const StringName &p_function, int p_idx, List<String> *r_options) const override;
#endif

	ClassDB() {}
	~ClassDB() {}
};

} // namespace special

class Engine : public Object {
	GDCLASS(Engine, Object);

protected:
	static void _bind_methods();
	static Engine *singleton;

public:
	static Engine *get_singleton() { return singleton; }
	void set_physics_ticks_per_second(int p_ips);
	int get_physics_ticks_per_second() const;

	void set_max_physics_steps_per_frame(int p_max_physics_steps);
	int get_max_physics_steps_per_frame() const;

	void set_physics_jitter_fix(double p_threshold);
	double get_physics_jitter_fix() const;
	double get_physics_interpolation_fraction() const;

	void set_max_fps(int p_fps);
	int get_max_fps() const;

	double get_frames_per_second() const;
	uint64_t get_physics_frames() const;
	uint64_t get_process_frames() const;

	int get_frames_drawn();

	void set_time_scale(double p_scale);
	double get_time_scale();

	MainLoop *get_main_loop() const;

	Dictionary get_version_info() const;
	Dictionary get_author_info() const;
	TypedArray<Dictionary> get_copyright_info() const;
	Dictionary get_donor_info() const;
	Dictionary get_license_info() const;
	String get_license_text() const;

	String get_architecture_name() const;

	bool is_in_physics_frame() const;

	bool has_singleton(const StringName &p_name) const;
	Object *get_singleton_object(const StringName &p_name) const;
	void register_singleton(const StringName &p_name, Object *p_object);
	void unregister_singleton(const StringName &p_name);
	Vector<String> get_singleton_list() const;

	Error register_script_language(ScriptLanguage *p_language);
	Error unregister_script_language(const ScriptLanguage *p_language);
	int get_script_language_count();
	ScriptLanguage *get_script_language(int p_index) const;

	void set_editor_hint(bool p_enabled);
	bool is_editor_hint() const;

	bool is_embedded_in_editor() const;

	// `set_write_movie_path()` is not exposed to the scripting API as changing it at run-time has no effect.
	String get_write_movie_path() const;

	void set_print_to_stdout(bool p_enabled);
	bool is_printing_to_stdout() const;

	void set_print_error_messages(bool p_enabled);
	bool is_printing_error_messages() const;

#ifdef TOOLS_ENABLED
	virtual void get_argument_options(const StringName &p_function, int p_idx, List<String> *r_options) const override;
#endif

	Engine() { singleton = this; }
};

class EngineDebugger : public Object {
	GDCLASS(EngineDebugger, Object);

	HashMap<StringName, Callable> captures;
	HashMap<StringName, Ref<EngineProfiler>> profilers;

protected:
	static void _bind_methods();
	static EngineDebugger *singleton;

public:
	static EngineDebugger *get_singleton() { return singleton; }

	bool is_active();

	void register_profiler(const StringName &p_name, Ref<EngineProfiler> p_profiler);
	void unregister_profiler(const StringName &p_name);
	bool is_profiling(const StringName &p_name);
	bool has_profiler(const StringName &p_name);
	void profiler_add_frame_data(const StringName &p_name, const Array &p_data);
	void profiler_enable(const StringName &p_name, bool p_enabled, const Array &p_opts = Array());

	void register_message_capture(const StringName &p_name, const Callable &p_callable);
	void unregister_message_capture(const StringName &p_name);
	bool has_capture(const StringName &p_name);

	void send_message(const String &p_msg, const Array &p_data);
	void debug(bool p_can_continue = true, bool p_is_error_breakpoint = false);
	void script_debug(ScriptLanguage *p_lang, bool p_can_continue = true, bool p_is_error_breakpoint = false);

	static Error call_capture(void *p_user, const String &p_cmd, const Array &p_data, bool &r_captured);

	void line_poll();

	void set_lines_left(int p_lines);
	int get_lines_left() const;

	void set_depth(int p_depth);
	int get_depth() const;

	bool is_breakpoint(int p_line, const StringName &p_source) const;
	bool is_skipping_breakpoints() const;
	void insert_breakpoint(int p_line, const StringName &p_source);
	void remove_breakpoint(int p_line, const StringName &p_source);
	void clear_breakpoints();

	EngineDebugger() { singleton = this; }
	~EngineDebugger();
};

} // namespace core_bind

VARIANT_ENUM_CAST(core_bind::ResourceLoader::ThreadLoadStatus);
VARIANT_ENUM_CAST(core_bind::ResourceLoader::CacheMode);

VARIANT_BITFIELD_CAST(core_bind::ResourceSaver::SaverFlags);

VARIANT_ENUM_CAST(core_bind::OS::RenderingDriver);
VARIANT_ENUM_CAST(core_bind::OS::SystemDir);
VARIANT_ENUM_CAST(core_bind::OS::StdHandleType);

VARIANT_ENUM_CAST(core_bind::Geometry2D::PolyBooleanOperation);
VARIANT_ENUM_CAST(core_bind::Geometry2D::PolyJoinType);
VARIANT_ENUM_CAST(core_bind::Geometry2D::PolyEndType);

VARIANT_ENUM_CAST(core_bind::Thread::Priority);

VARIANT_ENUM_CAST(core_bind::special::ClassDB::APIType);

#endif // CORE_BIND_H

/**************************************************************************/
/*  engine.h                                                              */
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

#ifndef ENGINE_H
#define ENGINE_H

#include "core/os/main_loop.h"
#include "core/string/ustring.h"
#include "core/templates/list.h"

template <typename T>
class TypedArray;

class Engine {
public:
	struct Singleton {
		StringName name;
		Object *ptr = nullptr;
		StringName class_name; // Used for binding generation hinting.
		// Singleton scope flags.
		bool user_created = false;
		bool editor_only = false;

		Singleton(const StringName &p_name = StringName(), Object *p_ptr = nullptr, const StringName &p_class_name = StringName());
	};

private:
	friend class Main;

	uint64_t frames_drawn = 0;
	uint32_t _frame_delay = 0;
	uint64_t _frame_ticks = 0;
	double _process_step = 0;

	int ips = 60;
	double physics_jitter_fix = 0.5;
	double _fps = 1;
	int _max_fps = 0;
	int _audio_output_latency = 0;
	double _time_scale = 1.0;
	uint64_t _physics_frames = 0;
	int max_physics_steps_per_frame = 8;
	double _physics_interpolation_fraction = 0.0f;
	bool abort_on_gpu_errors = false;
	bool use_validation_layers = false;
	bool generate_spirv_debug_info = false;
	bool extra_gpu_memory_tracking = false;
#if defined(DEBUG_ENABLED) || defined(DEV_ENABLED)
	bool accurate_breadcrumbs = false;
#endif
	int32_t gpu_idx = -1;

	uint64_t _process_frames = 0;
	bool _in_physics = false;

	List<Singleton> singletons;
	HashMap<StringName, Object *> singleton_ptrs;

	bool editor_hint = false;
	bool project_manager_hint = false;
	bool extension_reloading = false;
	bool embedded_in_editor = false;
	bool recovery_mode_hint = false;

	bool _print_header = true;

	static Engine *singleton;

	String write_movie_path;
	String shader_cache_path;

	static constexpr int SERVER_SYNC_FRAME_COUNT_WARNING = 5;
	int server_syncs = 0;
	bool frame_server_synced = false;

	bool freeze_time_scale = false;

public:
	static Engine *get_singleton();

	virtual void set_physics_ticks_per_second(int p_ips);
	virtual int get_physics_ticks_per_second() const;

	virtual void set_max_physics_steps_per_frame(int p_max_physics_steps);
	virtual int get_max_physics_steps_per_frame() const;

	void set_physics_jitter_fix(double p_threshold);
	double get_physics_jitter_fix() const;

	virtual void set_max_fps(int p_fps);
	virtual int get_max_fps() const;

	virtual void set_audio_output_latency(int p_msec);
	virtual int get_audio_output_latency() const;

	virtual double get_frames_per_second() const { return _fps; }

	uint64_t get_frames_drawn();

	uint64_t get_physics_frames() const { return _physics_frames; }
	uint64_t get_process_frames() const { return _process_frames; }
	bool is_in_physics_frame() const { return _in_physics; }
	uint64_t get_frame_ticks() const { return _frame_ticks; }
	double get_process_step() const { return _process_step; }
	double get_physics_interpolation_fraction() const { return _physics_interpolation_fraction; }

	void set_time_scale(double p_scale);
	double get_time_scale() const;
	double get_unfrozen_time_scale() const;

	void set_print_to_stdout(bool p_enabled);
	bool is_printing_to_stdout() const;

	void set_print_error_messages(bool p_enabled);
	bool is_printing_error_messages() const;
	void print_header(const String &p_string) const;
	void print_header_rich(const String &p_string) const;

	void set_frame_delay(uint32_t p_msec);
	uint32_t get_frame_delay() const;

	void add_singleton(const Singleton &p_singleton);
	void get_singletons(List<Singleton> *p_singletons);
	bool has_singleton(const StringName &p_name) const;
	Object *get_singleton_object(const StringName &p_name) const;
	void remove_singleton(const StringName &p_name);
	bool is_singleton_user_created(const StringName &p_name) const;
	bool is_singleton_editor_only(const StringName &p_name) const;

#ifdef TOOLS_ENABLED
	_FORCE_INLINE_ void set_editor_hint(bool p_enabled) { editor_hint = p_enabled; }
	_FORCE_INLINE_ bool is_editor_hint() const { return editor_hint; }

	_FORCE_INLINE_ void set_project_manager_hint(bool p_enabled) { project_manager_hint = p_enabled; }
	_FORCE_INLINE_ bool is_project_manager_hint() const { return project_manager_hint; }

	_FORCE_INLINE_ void set_extension_reloading_enabled(bool p_enabled) { extension_reloading = p_enabled; }
	_FORCE_INLINE_ bool is_extension_reloading_enabled() const { return extension_reloading; }

	_FORCE_INLINE_ void set_recovery_mode_hint(bool p_enabled) { recovery_mode_hint = p_enabled; }
	_FORCE_INLINE_ bool is_recovery_mode_hint() const { return recovery_mode_hint; }
#else
	_FORCE_INLINE_ void set_editor_hint(bool p_enabled) {}
	_FORCE_INLINE_ bool is_editor_hint() const { return false; }

	_FORCE_INLINE_ void set_project_manager_hint(bool p_enabled) {}
	_FORCE_INLINE_ bool is_project_manager_hint() const { return false; }

	_FORCE_INLINE_ void set_extension_reloading_enabled(bool p_enabled) {}
	_FORCE_INLINE_ bool is_extension_reloading_enabled() const { return false; }

	_FORCE_INLINE_ void set_recovery_mode_hint(bool p_enabled) {}
	_FORCE_INLINE_ bool is_recovery_mode_hint() const { return false; }
#endif

	Dictionary get_version_info() const;
	Dictionary get_author_info() const;
	TypedArray<Dictionary> get_copyright_info() const;
	Dictionary get_donor_info() const;
	Dictionary get_license_info() const;
	String get_license_text() const;

	void set_write_movie_path(const String &p_path);
	String get_write_movie_path() const;

	String get_architecture_name() const;

	void set_shader_cache_path(const String &p_path);
	String get_shader_cache_path() const;

	bool is_abort_on_gpu_errors_enabled() const;
	bool is_validation_layers_enabled() const;
	bool is_generate_spirv_debug_info_enabled() const;
	bool is_extra_gpu_memory_tracking_enabled() const;
#if defined(DEBUG_ENABLED) || defined(DEV_ENABLED)
	bool is_accurate_breadcrumbs_enabled() const;
#endif
	int32_t get_gpu_index() const;

	void increment_frames_drawn();
	bool notify_frame_server_synced();

	void set_freeze_time_scale(bool p_frozen);
	void set_embedded_in_editor(bool p_enabled);
	bool is_embedded_in_editor() const;

	Engine();
	virtual ~Engine();
};

#endif // ENGINE_H

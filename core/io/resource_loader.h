/**************************************************************************/
/*  resource_loader.h                                                     */
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

#ifndef RESOURCE_LOADER_H
#define RESOURCE_LOADER_H

#include "core/io/resource.h"
#include "core/object/gdvirtual.gen.inc"
#include "core/object/worker_thread_pool.h"
#include "core/os/thread.h"

class ConditionVariable;

template <int Tag>
class SafeBinaryMutex;

class ResourceFormatLoader : public RefCounted {
	GDCLASS(ResourceFormatLoader, RefCounted);

public:
	enum CacheMode {
		CACHE_MODE_IGNORE,
		CACHE_MODE_REUSE,
		CACHE_MODE_REPLACE,
		CACHE_MODE_IGNORE_DEEP,
		CACHE_MODE_REPLACE_DEEP,
	};

protected:
	static void _bind_methods();

	GDVIRTUAL0RC(Vector<String>, _get_recognized_extensions)
	GDVIRTUAL2RC(bool, _recognize_path, String, StringName)
	GDVIRTUAL1RC(bool, _handles_type, StringName)
	GDVIRTUAL1RC(String, _get_resource_type, String)
	GDVIRTUAL1RC(String, _get_resource_script_class, String)
	GDVIRTUAL1RC(ResourceUID::ID, _get_resource_uid, String)
	GDVIRTUAL2RC(Vector<String>, _get_dependencies, String, bool)
	GDVIRTUAL1RC(Vector<String>, _get_classes_used, String)
	GDVIRTUAL2RC(Error, _rename_dependencies, String, Dictionary)
	GDVIRTUAL1RC(bool, _exists, String)

	GDVIRTUAL4RC(Variant, _load, String, String, bool, int)

public:
	virtual Ref<Resource> load(const String &p_path, const String &p_original_path = "", Error *r_error = nullptr, bool p_use_sub_threads = false, float *r_progress = nullptr, CacheMode p_cache_mode = CACHE_MODE_REUSE);
	virtual bool exists(const String &p_path) const;
	virtual void get_recognized_extensions(List<String> *p_extensions) const;
	virtual void get_recognized_extensions_for_type(const String &p_type, List<String> *p_extensions) const;
	virtual bool recognize_path(const String &p_path, const String &p_for_type = String()) const;
	virtual bool handles_type(const String &p_type) const;
	virtual void get_classes_used(const String &p_path, HashSet<StringName> *r_classes);
	virtual String get_resource_type(const String &p_path) const;
	virtual String get_resource_script_class(const String &p_path) const;
	virtual ResourceUID::ID get_resource_uid(const String &p_path) const;
	virtual bool has_custom_uid_support() const;
	virtual void get_dependencies(const String &p_path, List<String> *p_dependencies, bool p_add_types = false);
	virtual Error rename_dependencies(const String &p_path, const HashMap<String, String> &p_map);
	virtual bool is_import_valid(const String &p_path) const { return true; }
	virtual bool is_imported(const String &p_path) const { return false; }
	virtual int get_import_order(const String &p_path) const { return 0; }
	virtual String get_import_group_file(const String &p_path) const { return ""; } //no group

	virtual ~ResourceFormatLoader() {}
};

VARIANT_ENUM_CAST(ResourceFormatLoader::CacheMode)

typedef void (*ResourceLoadErrorNotify)(const String &p_text);
typedef void (*DependencyErrorNotify)(const String &p_loading, const String &p_which, const String &p_type);

typedef Error (*ResourceLoaderImport)(const String &p_path);
typedef void (*ResourceLoadedCallback)(Ref<Resource> p_resource, const String &p_path);

class ResourceLoader {
	friend class LoadToken;

	enum {
		MAX_LOADERS = 64
	};

	struct ThreadLoadTask;

public:
	enum ThreadLoadStatus {
		THREAD_LOAD_INVALID_RESOURCE,
		THREAD_LOAD_IN_PROGRESS,
		THREAD_LOAD_FAILED,
		THREAD_LOAD_LOADED
	};

	enum LoadThreadMode {
		LOAD_THREAD_FROM_CURRENT,
		LOAD_THREAD_SPAWN_SINGLE,
		LOAD_THREAD_DISTRIBUTE,
	};

	struct LoadToken : public RefCounted {
		String local_path;
		String user_path;
		uint32_t user_rc = 0; // Having user RC implies regular RC incremented in one, until the user RC reaches zero.
		ThreadLoadTask *task_if_unregistered = nullptr;

		void clear();

		virtual ~LoadToken();
	};

	static const int BINARY_MUTEX_TAG = 1;

	static Ref<LoadToken> _load_start(const String &p_path, const String &p_type_hint, LoadThreadMode p_thread_mode, ResourceFormatLoader::CacheMode p_cache_mode, bool p_for_user = false);
	static Ref<Resource> _load_complete(LoadToken &p_load_token, Error *r_error);

private:
	static LoadToken *_load_threaded_request_reuse_user_token(const String &p_path);
	static void _load_threaded_request_setup_user_token(LoadToken *p_token, const String &p_path);

	static Ref<Resource> _load_complete_inner(LoadToken &p_load_token, Error *r_error, MutexLock<SafeBinaryMutex<BINARY_MUTEX_TAG>> &p_thread_load_lock);

	static Ref<ResourceFormatLoader> loader[MAX_LOADERS];
	static int loader_count;
	static bool timestamp_on_load;

	static void *err_notify_ud;
	static ResourceLoadErrorNotify err_notify;
	static void *dep_err_notify_ud;
	static DependencyErrorNotify dep_err_notify;
	static bool abort_on_missing_resource;
	static bool create_missing_resources_if_class_unavailable;
	static HashMap<String, Vector<String>> translation_remaps;
	static HashMap<String, String> path_remaps;

	static String _path_remap(const String &p_path, bool *r_translation_remapped = nullptr);
	friend class Resource;

	static SelfList<Resource>::List remapped_list;

	friend class ResourceFormatImporter;

	static Ref<Resource> _load(const String &p_path, const String &p_original_path, const String &p_type_hint, ResourceFormatLoader::CacheMode p_cache_mode, Error *r_error, bool p_use_sub_threads, float *r_progress);

	static ResourceLoadedCallback _loaded_callback;

	static Ref<ResourceFormatLoader> _find_custom_resource_format_loader(const String &path);

	struct ThreadLoadTask {
		WorkerThreadPool::TaskID task_id = 0; // Used if run on a worker thread from the pool.
		Thread::ID thread_id = 0; // Used if running on an user thread (e.g., simple non-threaded load).
		bool awaited = false; // If it's in the pool, this helps not awaiting from more than one dependent thread.
		ConditionVariable *cond_var = nullptr; // In not in the worker pool or already awaiting, this is used as a secondary awaiting mechanism.
		uint32_t awaiters_count = 0;
		bool need_wait = true;
		LoadToken *load_token = nullptr;
		String local_path;
		String type_hint;
		float progress = 0.0f;
		float max_reported_progress = 0.0f;
		uint64_t last_progress_check_main_thread_frame = UINT64_MAX;
		ThreadLoadStatus status = THREAD_LOAD_IN_PROGRESS;
		ResourceFormatLoader::CacheMode cache_mode = ResourceFormatLoader::CACHE_MODE_REUSE;
		Error error = OK;
		Ref<Resource> resource;
		bool use_sub_threads = false;
		HashSet<String> sub_tasks;

		struct ResourceChangedConnection {
			Resource *source = nullptr;
			Callable callable;
			uint32_t flags = 0;
		};
		LocalVector<ResourceChangedConnection> resource_changed_connections;
	};

	static void _run_load_task(void *p_userdata);

	static thread_local int load_nesting;
	static thread_local HashMap<int, HashMap<String, Ref<Resource>>> res_ref_overrides; // Outermost key is nesting level.
	static thread_local Vector<String> load_paths_stack;
	static thread_local ThreadLoadTask *curr_load_task;

	static SafeBinaryMutex<BINARY_MUTEX_TAG> thread_load_mutex;
	friend SafeBinaryMutex<BINARY_MUTEX_TAG> &_get_res_loader_mutex();

	static HashMap<String, ThreadLoadTask> thread_load_tasks;
	static bool cleaning_tasks;

	static HashMap<String, LoadToken *> user_load_tokens;

	static float _dependency_get_progress(const String &p_path);

	static bool _ensure_load_progress();

public:
	static Error load_threaded_request(const String &p_path, const String &p_type_hint = "", bool p_use_sub_threads = false, ResourceFormatLoader::CacheMode p_cache_mode = ResourceFormatLoader::CACHE_MODE_REUSE);
	static ThreadLoadStatus load_threaded_get_status(const String &p_path, float *r_progress = nullptr);
	static Ref<Resource> load_threaded_get(const String &p_path, Error *r_error = nullptr);

	static bool is_within_load() { return load_nesting > 0; }

	static void resource_changed_connect(Resource *p_source, const Callable &p_callable, uint32_t p_flags);
	static void resource_changed_disconnect(Resource *p_source, const Callable &p_callable);
	static void resource_changed_emit(Resource *p_source);

	static Ref<Resource> load(const String &p_path, const String &p_type_hint = "", ResourceFormatLoader::CacheMode p_cache_mode = ResourceFormatLoader::CACHE_MODE_REUSE, Error *r_error = nullptr);
	static bool exists(const String &p_path, const String &p_type_hint = "");

	static void get_recognized_extensions_for_type(const String &p_type, List<String> *p_extensions);
	static void add_resource_format_loader(Ref<ResourceFormatLoader> p_format_loader, bool p_at_front = false);
	static void remove_resource_format_loader(Ref<ResourceFormatLoader> p_format_loader);
	static void get_classes_used(const String &p_path, HashSet<StringName> *r_classes);
	static String get_resource_type(const String &p_path);
	static String get_resource_script_class(const String &p_path);
	static ResourceUID::ID get_resource_uid(const String &p_path);
	static bool has_custom_uid_support(const String &p_path);
	static void get_dependencies(const String &p_path, List<String> *p_dependencies, bool p_add_types = false);
	static Error rename_dependencies(const String &p_path, const HashMap<String, String> &p_map);
	static bool is_import_valid(const String &p_path);
	static String get_import_group_file(const String &p_path);
	static bool is_imported(const String &p_path);
	static int get_import_order(const String &p_path);

	static void set_timestamp_on_load(bool p_timestamp) { timestamp_on_load = p_timestamp; }
	static bool get_timestamp_on_load() { return timestamp_on_load; }

	// Loaders can safely use this regardless which thread they are running on.
	static void notify_load_error(const String &p_err) {
		if (err_notify) {
			MessageQueue::get_main_singleton()->push_callable(callable_mp_static(err_notify).bind(p_err));
		}
	}
	static void set_error_notify_func(ResourceLoadErrorNotify p_err_notify) {
		err_notify = p_err_notify;
	}

	// Loaders can safely use this regardless which thread they are running on.
	static void notify_dependency_error(const String &p_path, const String &p_dependency, const String &p_type) {
		if (dep_err_notify) {
			if (Thread::get_caller_id() == Thread::get_main_id()) {
				dep_err_notify(p_path, p_dependency, p_type);
			} else {
				MessageQueue::get_main_singleton()->push_callable(callable_mp_static(dep_err_notify).bind(p_path, p_dependency, p_type));
			}
		}
	}
	static void set_dependency_error_notify_func(DependencyErrorNotify p_err_notify) {
		dep_err_notify = p_err_notify;
	}

	static void set_abort_on_missing_resources(bool p_abort) { abort_on_missing_resource = p_abort; }
	static bool get_abort_on_missing_resources() { return abort_on_missing_resource; }

	static String path_remap(const String &p_path);
	static String import_remap(const String &p_path);

	static void load_path_remaps();
	static void clear_path_remaps();

	static void reload_translation_remaps();
	static void load_translation_remaps();
	static void clear_translation_remaps();

	static void clear_thread_load_tasks();

	static void set_load_callback(ResourceLoadedCallback p_callback);
	static ResourceLoaderImport import;

	static bool add_custom_resource_format_loader(const String &script_path);
	static void add_custom_loaders();
	static void remove_custom_loaders();

	static void set_create_missing_resources_if_class_unavailable(bool p_enable);
	_FORCE_INLINE_ static bool is_creating_missing_resources_if_class_unavailable_enabled() { return create_missing_resources_if_class_unavailable; }

	static Ref<Resource> ensure_resource_ref_override_for_outer_load(const String &p_path, const String &p_res_type);
	static Ref<Resource> get_resource_ref_override(const String &p_path);

	static bool is_cleaning_tasks();

	static Vector<String> list_directory(const String &p_directory);

	static void initialize();
	static void finalize();
};

#endif // RESOURCE_LOADER_H

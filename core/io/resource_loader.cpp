/**************************************************************************/
/*  resource_loader.cpp                                                   */
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

#include "resource_loader.h"

#include "core/config/project_settings.h"
#include "core/core_bind.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/resource_importer.h"
#include "core/object/script_language.h"
#include "core/os/condition_variable.h"
#include "core/os/os.h"
#include "core/os/safe_binary_mutex.h"
#include "core/string/print_string.h"
#include "core/string/translation_server.h"
#include "core/templates/rb_set.h"
#include "core/variant/variant_parser.h"
#include "servers/rendering_server.h"

#ifdef DEBUG_LOAD_THREADED
#define print_lt(m_text) print_line(m_text)
#else
#define print_lt(m_text)
#endif

Ref<ResourceFormatLoader> ResourceLoader::loader[ResourceLoader::MAX_LOADERS];

int ResourceLoader::loader_count = 0;

bool ResourceFormatLoader::recognize_path(const String &p_path, const String &p_for_type) const {
	bool ret = false;
	if (GDVIRTUAL_CALL(_recognize_path, p_path, p_for_type, ret)) {
		return ret;
	}

	String extension = p_path.get_extension();

	List<String> extensions;
	if (p_for_type.is_empty()) {
		get_recognized_extensions(&extensions);
	} else {
		get_recognized_extensions_for_type(p_for_type, &extensions);
	}

	for (const String &E : extensions) {
		if (E.nocasecmp_to(extension) == 0) {
			return true;
		}
	}

	return false;
}

bool ResourceFormatLoader::handles_type(const String &p_type) const {
	bool success = false;
	GDVIRTUAL_CALL(_handles_type, p_type, success);
	return success;
}

void ResourceFormatLoader::get_classes_used(const String &p_path, HashSet<StringName> *r_classes) {
	Vector<String> ret;
	if (GDVIRTUAL_CALL(_get_classes_used, p_path, ret)) {
		for (int i = 0; i < ret.size(); i++) {
			r_classes->insert(ret[i]);
		}
		return;
	}

	String res = get_resource_type(p_path);
	if (!res.is_empty()) {
		r_classes->insert(res);
	}
}

String ResourceFormatLoader::get_resource_type(const String &p_path) const {
	String ret;
	GDVIRTUAL_CALL(_get_resource_type, p_path, ret);
	return ret;
}

String ResourceFormatLoader::get_resource_script_class(const String &p_path) const {
	String ret;
	GDVIRTUAL_CALL(_get_resource_script_class, p_path, ret);
	return ret;
}

ResourceUID::ID ResourceFormatLoader::get_resource_uid(const String &p_path) const {
	int64_t uid = ResourceUID::INVALID_ID;
	if (has_custom_uid_support()) {
		GDVIRTUAL_CALL(_get_resource_uid, p_path, uid);
	} else {
		Ref<FileAccess> file = FileAccess::open(p_path + ".uid", FileAccess::READ);
		if (file.is_valid()) {
			uid = ResourceUID::get_singleton()->text_to_id(file->get_line());
		}
	}
	return uid;
}

bool ResourceFormatLoader::has_custom_uid_support() const {
	return GDVIRTUAL_IS_OVERRIDDEN(_get_resource_uid);
}

void ResourceFormatLoader::get_recognized_extensions_for_type(const String &p_type, List<String> *p_extensions) const {
	if (p_type.is_empty() || handles_type(p_type)) {
		get_recognized_extensions(p_extensions);
	}
}

void ResourceLoader::get_recognized_extensions_for_type(const String &p_type, List<String> *p_extensions) {
	for (int i = 0; i < loader_count; i++) {
		loader[i]->get_recognized_extensions_for_type(p_type, p_extensions);
	}
}

bool ResourceFormatLoader::exists(const String &p_path) const {
	bool success = false;
	if (GDVIRTUAL_CALL(_exists, p_path, success)) {
		return success;
	}
	return FileAccess::exists(p_path); // By default just check file.
}

void ResourceFormatLoader::get_recognized_extensions(List<String> *p_extensions) const {
	PackedStringArray exts;
	if (GDVIRTUAL_CALL(_get_recognized_extensions, exts)) {
		const String *r = exts.ptr();
		for (int i = 0; i < exts.size(); ++i) {
			p_extensions->push_back(r[i]);
		}
	}
}

Ref<Resource> ResourceFormatLoader::load(const String &p_path, const String &p_original_path, Error *r_error, bool p_use_sub_threads, float *r_progress, CacheMode p_cache_mode) {
	Variant res;
	if (GDVIRTUAL_CALL(_load, p_path, p_original_path, p_use_sub_threads, p_cache_mode, res)) {
		if (res.get_type() == Variant::INT) { // Error code, abort.
			if (r_error) {
				*r_error = (Error)res.operator int64_t();
			}
			return Ref<Resource>();
		} else { // Success, pass on result.
			if (r_error) {
				*r_error = OK;
			}
			return res;
		}
	}

	ERR_FAIL_V_MSG(Ref<Resource>(), vformat("Failed to load resource '%s'. ResourceFormatLoader::load was not implemented for this resource type.", p_path));
}

void ResourceFormatLoader::get_dependencies(const String &p_path, List<String> *p_dependencies, bool p_add_types) {
	PackedStringArray deps;
	if (GDVIRTUAL_CALL(_get_dependencies, p_path, p_add_types, deps)) {
		const String *r = deps.ptr();
		for (int i = 0; i < deps.size(); ++i) {
			p_dependencies->push_back(r[i]);
		}
	}
}

Error ResourceFormatLoader::rename_dependencies(const String &p_path, const HashMap<String, String> &p_map) {
	Dictionary deps_dict;
	for (KeyValue<String, String> E : p_map) {
		deps_dict[E.key] = E.value;
	}

	Error err = OK;
	GDVIRTUAL_CALL(_rename_dependencies, p_path, deps_dict, err);
	return err;
}

void ResourceFormatLoader::_bind_methods() {
	BIND_ENUM_CONSTANT(CACHE_MODE_IGNORE);
	BIND_ENUM_CONSTANT(CACHE_MODE_REUSE);
	BIND_ENUM_CONSTANT(CACHE_MODE_REPLACE);
	BIND_ENUM_CONSTANT(CACHE_MODE_IGNORE_DEEP);
	BIND_ENUM_CONSTANT(CACHE_MODE_REPLACE_DEEP);

	GDVIRTUAL_BIND(_get_recognized_extensions);
	GDVIRTUAL_BIND(_recognize_path, "path", "type");
	GDVIRTUAL_BIND(_handles_type, "type");
	GDVIRTUAL_BIND(_get_resource_type, "path");
	GDVIRTUAL_BIND(_get_resource_script_class, "path");
	GDVIRTUAL_BIND(_get_resource_uid, "path");
	GDVIRTUAL_BIND(_get_dependencies, "path", "add_types");
	GDVIRTUAL_BIND(_rename_dependencies, "path", "renames");
	GDVIRTUAL_BIND(_exists, "path");
	GDVIRTUAL_BIND(_get_classes_used, "path");
	GDVIRTUAL_BIND(_load, "path", "original_path", "use_sub_threads", "cache_mode");
}

///////////////////////////////////

// These are used before and after a wait for a WorkerThreadPool task
// because that can lead to another load started in the same thread,
// something we must treat as a different stack for the purposes
// of tracking nesting.

#define PREPARE_FOR_WTP_WAIT                                                   \
	int load_nesting_backup = ResourceLoader::load_nesting;                    \
	Vector<String> load_paths_stack_backup = ResourceLoader::load_paths_stack; \
	ResourceLoader::load_nesting = 0;                                          \
	ResourceLoader::load_paths_stack.clear();

#define RESTORE_AFTER_WTP_WAIT                                  \
	DEV_ASSERT(ResourceLoader::load_nesting == 0);              \
	DEV_ASSERT(ResourceLoader::load_paths_stack.is_empty());    \
	ResourceLoader::load_nesting = load_nesting_backup;         \
	ResourceLoader::load_paths_stack = load_paths_stack_backup; \
	load_paths_stack_backup.clear();

// This should be robust enough to be called redundantly without issues.
void ResourceLoader::LoadToken::clear() {
	WorkerThreadPool::TaskID task_to_await = 0;

	{
		MutexLock thread_load_lock(thread_load_mutex);
		// User-facing tokens shouldn't be deleted until completely claimed.
		DEV_ASSERT(user_rc == 0 && user_path.is_empty());

		if (!local_path.is_empty()) {
			if (task_if_unregistered) {
				memdelete(task_if_unregistered);
				task_if_unregistered = nullptr;
			} else {
				DEV_ASSERT(thread_load_tasks.has(local_path));
				ThreadLoadTask &load_task = thread_load_tasks[local_path];
				if (load_task.task_id && !load_task.awaited) {
					task_to_await = load_task.task_id;
				}
				// Removing a task which is still in progress would be catastrophic.
				// Tokens must be alive until the task thread function is done.
				DEV_ASSERT(load_task.status == THREAD_LOAD_FAILED || load_task.status == THREAD_LOAD_LOADED);
				thread_load_tasks.erase(local_path);
			}
			local_path.clear(); // Mark as already cleared.
		}
	}

	// If task is unused, await it here, locally, now the token data is consistent.
	if (task_to_await) {
		PREPARE_FOR_WTP_WAIT
		WorkerThreadPool::get_singleton()->wait_for_task_completion(task_to_await);
		RESTORE_AFTER_WTP_WAIT
	}
}

ResourceLoader::LoadToken::~LoadToken() {
	clear();
}

Ref<Resource> ResourceLoader::_load(const String &p_path, const String &p_original_path, const String &p_type_hint, ResourceFormatLoader::CacheMode p_cache_mode, Error *r_error, bool p_use_sub_threads, float *r_progress) {
	const String &original_path = p_original_path.is_empty() ? p_path : p_original_path;
	load_nesting++;
	if (load_paths_stack.size()) {
		MutexLock thread_load_lock(thread_load_mutex);
		const String &parent_task_path = load_paths_stack.get(load_paths_stack.size() - 1);
		HashMap<String, ThreadLoadTask>::Iterator E = thread_load_tasks.find(parent_task_path);
		// Avoid double-tracking, for progress reporting, resources that boil down to a remapped path containing the real payload (e.g., imported resources).
		bool is_remapped_load = original_path == parent_task_path;
		if (E && !is_remapped_load) {
			E->value.sub_tasks.insert(p_original_path);
		}
	}
	load_paths_stack.push_back(original_path);

	print_verbose(vformat("Loading resource: %s", p_path));

	// Try all loaders and pick the first match for the type hint
	bool found = false;
	Ref<Resource> res;
	for (int i = 0; i < loader_count; i++) {
		if (!loader[i]->recognize_path(p_path, p_type_hint)) {
			continue;
		}
		found = true;
		res = loader[i]->load(p_path, original_path, r_error, p_use_sub_threads, r_progress, p_cache_mode);
		if (res.is_valid()) {
			break;
		}
	}

	load_paths_stack.resize(load_paths_stack.size() - 1);
	res_ref_overrides.erase(load_nesting);
	load_nesting--;

	if (res.is_valid()) {
		return res;
	} else {
		print_verbose(vformat("Failed loading resource: %s", p_path));
	}

#ifdef TOOLS_ENABLED
	if (Engine::get_singleton()->is_editor_hint()) {
		if (ResourceFormatImporter::get_singleton()->get_importer_by_extension(p_path.get_extension()).is_valid()) {
			// The format is known to the editor, but the file hasn't been imported
			// (otherwise, ResourceFormatImporter would have been found as a suitable loader).
			found = true;
			if (r_error) {
				*r_error = ERR_FILE_NOT_FOUND;
			}
		}
	}
#endif
	ERR_FAIL_COND_V_MSG(found, Ref<Resource>(),
			vformat("Failed loading resource: %s. Make sure resources have been imported by opening the project in the editor at least once.", p_path));

#ifdef TOOLS_ENABLED
	Ref<FileAccess> file_check = FileAccess::create(FileAccess::ACCESS_RESOURCES);
	if (!file_check->file_exists(p_path)) {
		if (r_error) {
			*r_error = ERR_FILE_NOT_FOUND;
		}
		ERR_FAIL_V_MSG(Ref<Resource>(), vformat("Resource file not found: %s (expected type: %s)", p_path, p_type_hint));
	}
#endif

	if (r_error) {
		*r_error = ERR_FILE_UNRECOGNIZED;
	}
	ERR_FAIL_V_MSG(Ref<Resource>(), vformat("No loader found for resource: %s (expected type: %s)", p_path, p_type_hint));
}

// This implementation must allow re-entrancy for a task that started awaiting in a deeper stack frame.
// The load task token must be manually re-referenced before this is called, which includes threaded runs.
void ResourceLoader::_run_load_task(void *p_userdata) {
	ThreadLoadTask &load_task = *(ThreadLoadTask *)p_userdata;

	{
		MutexLock thread_load_lock(thread_load_mutex);
		if (cleaning_tasks) {
			load_task.status = THREAD_LOAD_FAILED;
			return;
		}
	}

	ThreadLoadTask *curr_load_task_backup = curr_load_task;
	curr_load_task = &load_task;

	// Thread-safe either if it's the current thread or a brand new one.
	CallQueue *own_mq_override = nullptr;
	if (load_nesting == 0) {
		DEV_ASSERT(load_paths_stack.is_empty());
		if (!Thread::is_main_thread()) {
			// Let the caller thread use its own, for added flexibility. Provide one otherwise.
			if (MessageQueue::get_singleton() == MessageQueue::get_main_singleton()) {
				own_mq_override = memnew(CallQueue);
				MessageQueue::set_thread_singleton_override(own_mq_override);
			}
			set_current_thread_safe_for_nodes(true);
		}
	}
	// --

	bool xl_remapped = false;
	const String &remapped_path = _path_remap(load_task.local_path, &xl_remapped);

	Error load_err = OK;
	Ref<Resource> res = _load(remapped_path, remapped_path != load_task.local_path ? load_task.local_path : String(), load_task.type_hint, load_task.cache_mode, &load_err, load_task.use_sub_threads, &load_task.progress);
	if (MessageQueue::get_singleton() != MessageQueue::get_main_singleton()) {
		MessageQueue::get_singleton()->flush();
	}

	thread_load_mutex.lock();

	load_task.resource = res;

	load_task.progress = 1.0; // It was fully loaded at this point, so force progress to 1.0.
	load_task.error = load_err;
	if (load_task.error != OK) {
		load_task.status = THREAD_LOAD_FAILED;
	} else {
		load_task.status = THREAD_LOAD_LOADED;
	}

	if (load_task.cond_var && load_task.need_wait) {
		load_task.cond_var->notify_all();
	}
	load_task.need_wait = false;

	bool ignoring = load_task.cache_mode == ResourceFormatLoader::CACHE_MODE_IGNORE || load_task.cache_mode == ResourceFormatLoader::CACHE_MODE_IGNORE_DEEP;
	bool replacing = load_task.cache_mode == ResourceFormatLoader::CACHE_MODE_REPLACE || load_task.cache_mode == ResourceFormatLoader::CACHE_MODE_REPLACE_DEEP;
	bool unlock_pending = true;
	if (load_task.resource.is_valid()) {
		// From now on, no critical section needed as no one will write to the task anymore.
		// Moreover, the mutex being unlocked is a requirement if some of the calls below
		// that set the resource up invoke code that in turn requests resource loading.
		thread_load_mutex.unlock();
		unlock_pending = false;

		if (!ignoring) {
			ResourceCache::lock.lock(); // Check and operations must happen atomically.
			bool pending_unlock = true;
			Ref<Resource> old_res = ResourceCache::get_ref(load_task.local_path);
			if (old_res.is_valid()) {
				if (old_res != load_task.resource) {
					// Resource can already exists at this point for two reasons:
					// a) The load uses replace mode.
					// b) There were more than one load in flight for the same path because of deadlock prevention.
					// Either case, we want to keep the resource that was already there.
					ResourceCache::lock.unlock();
					pending_unlock = false;
					if (replacing) {
						old_res->copy_from(load_task.resource);
					}
					load_task.resource = old_res;
				}
			} else {
				load_task.resource->set_path(load_task.local_path);
			}
			if (pending_unlock) {
				ResourceCache::lock.unlock();
			}
		} else {
			load_task.resource->set_path_cache(load_task.local_path);
		}

		if (xl_remapped) {
			load_task.resource->set_as_translation_remapped(true);
		}

#ifdef TOOLS_ENABLED
		load_task.resource->set_edited(false);
		if (timestamp_on_load) {
			uint64_t mt = FileAccess::get_modified_time(remapped_path);
			//printf("mt %s: %lli\n",remapped_path.utf8().get_data(),mt);
			load_task.resource->set_last_modified_time(mt);
		}
#endif

		if (_loaded_callback) {
			_loaded_callback(load_task.resource, load_task.local_path);
		}
	} else if (!ignoring) {
		Ref<Resource> existing = ResourceCache::get_ref(load_task.local_path);
		if (existing.is_valid()) {
			load_task.resource = existing;
			load_task.status = THREAD_LOAD_LOADED;
			load_task.progress = 1.0;

			thread_load_mutex.unlock();
			unlock_pending = false;

			if (_loaded_callback) {
				_loaded_callback(load_task.resource, load_task.local_path);
			}
		}
	}

	// It's safe now to let the task go in case no one else was grabbing the token.
	load_task.load_token->unreference();

	if (unlock_pending) {
		thread_load_mutex.unlock();
	}

	if (load_nesting == 0) {
		if (own_mq_override) {
			MessageQueue::set_thread_singleton_override(nullptr);
			memdelete(own_mq_override);
		}
		DEV_ASSERT(load_paths_stack.is_empty());
	}

	curr_load_task = curr_load_task_backup;
}

String ResourceLoader::_validate_local_path(const String &p_path) {
	ResourceUID::ID uid = ResourceUID::get_singleton()->text_to_id(p_path);
	if (uid != ResourceUID::INVALID_ID) {
		return ResourceUID::get_singleton()->get_id_path(uid);
	} else if (p_path.is_relative_path()) {
		return ("res://" + p_path).simplify_path();
	} else {
		return ProjectSettings::get_singleton()->localize_path(p_path);
	}
}

Error ResourceLoader::load_threaded_request(const String &p_path, const String &p_type_hint, bool p_use_sub_threads, ResourceFormatLoader::CacheMode p_cache_mode) {
	Ref<ResourceLoader::LoadToken> token = _load_start(p_path, p_type_hint, p_use_sub_threads ? LOAD_THREAD_DISTRIBUTE : LOAD_THREAD_SPAWN_SINGLE, p_cache_mode, true);
	return token.is_valid() ? OK : FAILED;
}

ResourceLoader::LoadToken *ResourceLoader::_load_threaded_request_reuse_user_token(const String &p_path) {
	HashMap<String, LoadToken *>::Iterator E = user_load_tokens.find(p_path);
	if (E) {
		print_verbose("load_threaded_request(): Another threaded load for resource path '" + p_path + "' has been initiated. Not an error.");
		LoadToken *token = E->value;
		token->user_rc++;
		return token;
	} else {
		return nullptr;
	}
}

void ResourceLoader::_load_threaded_request_setup_user_token(LoadToken *p_token, const String &p_path) {
	p_token->user_path = p_path;
	p_token->reference(); // Extra RC until all user requests have been gotten.
	p_token->user_rc = 1;
	user_load_tokens[p_path] = p_token;
	print_lt("REQUEST: user load tokens: " + itos(user_load_tokens.size()));
}

Ref<Resource> ResourceLoader::load(const String &p_path, const String &p_type_hint, ResourceFormatLoader::CacheMode p_cache_mode, Error *r_error) {
	if (r_error) {
		*r_error = OK;
	}

	LoadThreadMode thread_mode = LOAD_THREAD_FROM_CURRENT;
	if (WorkerThreadPool::get_singleton()->get_caller_task_id() != WorkerThreadPool::INVALID_TASK_ID) {
		// If user is initiating a single-threaded load from a WorkerThreadPool task,
		// we instead spawn a new task so there's a precondition that a load in a pool task
		// is always initiated by the engine. That makes certain aspects simpler, such as
		// cyclic load detection and awaiting.
		thread_mode = LOAD_THREAD_SPAWN_SINGLE;
	}
	Ref<LoadToken> load_token = _load_start(p_path, p_type_hint, thread_mode, p_cache_mode);
	if (load_token.is_null()) {
		if (r_error) {
			*r_error = FAILED;
		}
		return Ref<Resource>();
	}

	Ref<Resource> res = _load_complete(*load_token.ptr(), r_error);
	return res;
}

Ref<ResourceLoader::LoadToken> ResourceLoader::_load_start(const String &p_path, const String &p_type_hint, LoadThreadMode p_thread_mode, ResourceFormatLoader::CacheMode p_cache_mode, bool p_for_user) {
	String local_path = _validate_local_path(p_path);

	bool ignoring_cache = p_cache_mode == ResourceFormatLoader::CACHE_MODE_IGNORE || p_cache_mode == ResourceFormatLoader::CACHE_MODE_IGNORE_DEEP;

	Ref<LoadToken> load_token;
	bool must_not_register = false;
	ThreadLoadTask *load_task_ptr = nullptr;
	{
		MutexLock thread_load_lock(thread_load_mutex);

		if (p_for_user) {
			LoadToken *existing_token = _load_threaded_request_reuse_user_token(p_path);
			if (existing_token) {
				return Ref<LoadToken>(existing_token);
			}
		}

		if (!ignoring_cache && thread_load_tasks.has(local_path)) {
			load_token = Ref<LoadToken>(thread_load_tasks[local_path].load_token);
			if (load_token.is_valid()) {
				if (p_for_user) {
					// Load task exists, with no user tokens at the moment.
					// Let's "attach" to it.
					_load_threaded_request_setup_user_token(load_token.ptr(), p_path);
				}
				return load_token;
			} else {
				// The token is dying (reached 0 on another thread).
				// Ensure it's killed now so the path can be safely reused right away.
				thread_load_tasks[local_path].load_token->clear();
			}
		}

		load_token.instantiate();
		load_token->local_path = local_path;
		if (p_for_user) {
			_load_threaded_request_setup_user_token(load_token.ptr(), p_path);
		}

		//create load task
		{
			ThreadLoadTask load_task;

			load_task.load_token = load_token.ptr();
			load_task.local_path = local_path;
			load_task.type_hint = p_type_hint;
			load_task.cache_mode = p_cache_mode;
			load_task.use_sub_threads = p_thread_mode == LOAD_THREAD_DISTRIBUTE;
			if (p_cache_mode == ResourceFormatLoader::CACHE_MODE_REUSE) {
				Ref<Resource> existing = ResourceCache::get_ref(local_path);
				if (existing.is_valid()) {
					//referencing is fine
					load_task.resource = existing;
					load_task.status = THREAD_LOAD_LOADED;
					load_task.progress = 1.0;
					DEV_ASSERT(!thread_load_tasks.has(local_path));
					thread_load_tasks[local_path] = load_task;
					return load_token;
				}
			}

			// If we want to ignore cache, but there's another task loading it, we can't add this one to the map.
			must_not_register = ignoring_cache && thread_load_tasks.has(local_path);
			if (must_not_register) {
				load_token->task_if_unregistered = memnew(ThreadLoadTask(load_task));
				load_task_ptr = load_token->task_if_unregistered;
			} else {
				DEV_ASSERT(!thread_load_tasks.has(local_path));
				HashMap<String, ResourceLoader::ThreadLoadTask>::Iterator E = thread_load_tasks.insert(local_path, load_task);
				load_task_ptr = &E->value;
			}
		}

		// It's important to keep the token alive because until the load completes,
		// which includes before the thread start, it may happen that no one is grabbing
		// the token anymore so it's released.
		load_task_ptr->load_token->reference();

		if (p_thread_mode == LOAD_THREAD_FROM_CURRENT) {
			// The current thread may happen to be a thread from the pool.
			WorkerThreadPool::TaskID tid = WorkerThreadPool::get_singleton()->get_caller_task_id();
			if (tid != WorkerThreadPool::INVALID_TASK_ID) {
				load_task_ptr->task_id = tid;
			} else {
				load_task_ptr->thread_id = Thread::get_caller_id();
			}
		} else {
			load_task_ptr->task_id = WorkerThreadPool::get_singleton()->add_native_task(&ResourceLoader::_run_load_task, load_task_ptr);
		}
	} // MutexLock(thread_load_mutex).

	if (p_thread_mode == LOAD_THREAD_FROM_CURRENT) {
		_run_load_task(load_task_ptr);
	}

	return load_token;
}

float ResourceLoader::_dependency_get_progress(const String &p_path) {
	if (thread_load_tasks.has(p_path)) {
		ThreadLoadTask &load_task = thread_load_tasks[p_path];
		float current_progress = 0.0;
		int dep_count = load_task.sub_tasks.size();
		if (dep_count > 0) {
			for (const String &E : load_task.sub_tasks) {
				current_progress += _dependency_get_progress(E);
			}
			current_progress /= float(dep_count);
			current_progress *= 0.5;
			current_progress += load_task.progress * 0.5;
		} else {
			current_progress = load_task.progress;
		}
		load_task.max_reported_progress = MAX(load_task.max_reported_progress, current_progress);
		return load_task.max_reported_progress;
	} else {
		return 1.0; //assume finished loading it so it no longer exists
	}
}

ResourceLoader::ThreadLoadStatus ResourceLoader::load_threaded_get_status(const String &p_path, float *r_progress) {
	bool ensure_progress = false;
	ThreadLoadStatus status = THREAD_LOAD_IN_PROGRESS;
	{
		MutexLock thread_load_lock(thread_load_mutex);

		if (!user_load_tokens.has(p_path)) {
			print_verbose("load_threaded_get_status(): No threaded load for resource path '" + p_path + "' has been initiated or its result has already been collected.");
			return THREAD_LOAD_INVALID_RESOURCE;
		}

		String local_path = _validate_local_path(p_path);
		ERR_FAIL_COND_V_MSG(!thread_load_tasks.has(local_path), THREAD_LOAD_INVALID_RESOURCE, "Bug in ResourceLoader logic, please report.");

		ThreadLoadTask &load_task = thread_load_tasks[local_path];
		status = load_task.status;
		if (r_progress) {
			*r_progress = _dependency_get_progress(local_path);
		}

		// Support userland polling in a loop on the main thread.
		if (Thread::is_main_thread() && status == THREAD_LOAD_IN_PROGRESS) {
			uint64_t frame = Engine::get_singleton()->get_process_frames();
			if (frame == load_task.last_progress_check_main_thread_frame) {
				ensure_progress = true;
			} else {
				load_task.last_progress_check_main_thread_frame = frame;
			}
		}
	}

	if (ensure_progress) {
		_ensure_load_progress();
	}

	return status;
}

Ref<Resource> ResourceLoader::load_threaded_get(const String &p_path, Error *r_error) {
	if (r_error) {
		*r_error = OK;
	}

	Ref<Resource> res;
	{
		MutexLock thread_load_lock(thread_load_mutex);

		if (!user_load_tokens.has(p_path)) {
			print_verbose("load_threaded_get(): No threaded load for resource path '" + p_path + "' has been initiated or its result has already been collected.");
			if (r_error) {
				*r_error = ERR_INVALID_PARAMETER;
			}
			return Ref<Resource>();
		}

		LoadToken *load_token = user_load_tokens[p_path];
		DEV_ASSERT(load_token->user_rc >= 1);

		// Support userland requesting on the main thread before the load is reported to be complete.
		if (Thread::is_main_thread() && !load_token->local_path.is_empty()) {
			const ThreadLoadTask &load_task = thread_load_tasks[load_token->local_path];
			while (load_task.status == THREAD_LOAD_IN_PROGRESS) {
				thread_load_lock.temp_unlock();
				bool exit = !_ensure_load_progress();
				OS::get_singleton()->delay_usec(1000);
				thread_load_lock.temp_relock();
				if (exit) {
					break;
				}
			}
		}

		res = _load_complete_inner(*load_token, r_error, thread_load_lock);

		load_token->user_rc--;
		if (load_token->user_rc == 0) {
			load_token->user_path.clear();
			user_load_tokens.erase(p_path);
			if (load_token->unreference()) {
				memdelete(load_token);
				load_token = nullptr;
			}
		}
	}

	print_lt("GET: user load tokens: " + itos(user_load_tokens.size()));

	return res;
}

Ref<Resource> ResourceLoader::_load_complete(LoadToken &p_load_token, Error *r_error) {
	MutexLock thread_load_lock(thread_load_mutex);
	return _load_complete_inner(p_load_token, r_error, thread_load_lock);
}

Ref<Resource> ResourceLoader::_load_complete_inner(LoadToken &p_load_token, Error *r_error, MutexLock<SafeBinaryMutex<BINARY_MUTEX_TAG>> &p_thread_load_lock) {
	if (r_error) {
		*r_error = OK;
	}

	ThreadLoadTask *load_task_ptr = nullptr;
	if (p_load_token.task_if_unregistered) {
		load_task_ptr = p_load_token.task_if_unregistered;
	} else {
		if (!thread_load_tasks.has(p_load_token.local_path)) {
			if (r_error) {
				*r_error = ERR_BUG;
			}
			ERR_FAIL_V_MSG(Ref<Resource>(), "Bug in ResourceLoader logic, please report.");
		}

		ThreadLoadTask &load_task = thread_load_tasks[p_load_token.local_path];

		if (load_task.status == THREAD_LOAD_IN_PROGRESS) {
			DEV_ASSERT((load_task.task_id == 0) != (load_task.thread_id == 0));

			if ((load_task.task_id != 0 && load_task.task_id == WorkerThreadPool::get_singleton()->get_caller_task_id()) ||
					(load_task.thread_id != 0 && load_task.thread_id == Thread::get_caller_id())) {
				// Load is in progress, but it's precisely this thread the one in charge.
				// That means this is a cyclic load.
				if (r_error) {
					*r_error = ERR_BUSY;
				}
				return Ref<Resource>();
			}

			bool loader_is_wtp = load_task.task_id != 0;
			if (loader_is_wtp) {
				// Loading thread is in the worker pool.
				p_thread_load_lock.temp_unlock();

				PREPARE_FOR_WTP_WAIT
				Error wait_err = WorkerThreadPool::get_singleton()->wait_for_task_completion(load_task.task_id);
				RESTORE_AFTER_WTP_WAIT

				DEV_ASSERT(!wait_err || wait_err == ERR_BUSY);
				if (wait_err == ERR_BUSY) {
					// The WorkerThreadPool has reported that the current task wants to await on an older one.
					// That't not allowed for safety, to avoid deadlocks. Fortunately, though, in the context of
					// resource loading that means that the task to wait for can be restarted here to break the
					// cycle, with as much recursion into this process as needed.
					// When the stack is eventually unrolled, the original load will have been notified to go on.
					load_task.load_token->reference();
					_run_load_task(&load_task);
				}

				p_thread_load_lock.temp_relock();
				load_task.awaited = true;

				DEV_ASSERT(load_task.status == THREAD_LOAD_FAILED || load_task.status == THREAD_LOAD_LOADED);
			} else if (load_task.need_wait) {
				// Loading thread is main or user thread.
				if (!load_task.cond_var) {
					load_task.cond_var = memnew(ConditionVariable);
				}
				load_task.awaiters_count++;
				do {
					load_task.cond_var->wait(p_thread_load_lock);
					DEV_ASSERT(thread_load_tasks.has(p_load_token.local_path) && p_load_token.get_reference_count());
				} while (load_task.need_wait);
				load_task.awaiters_count--;
				if (load_task.awaiters_count == 0) {
					memdelete(load_task.cond_var);
					load_task.cond_var = nullptr;
				}

				DEV_ASSERT(load_task.status == THREAD_LOAD_FAILED || load_task.status == THREAD_LOAD_LOADED);
			}
		}

		if (cleaning_tasks) {
			load_task.resource = Ref<Resource>();
			load_task.error = FAILED;
		}

		load_task_ptr = &load_task;
	}

	p_thread_load_lock.temp_unlock();

	Ref<Resource> resource = load_task_ptr->resource;
	if (r_error) {
		*r_error = load_task_ptr->error;
	}

	if (resource.is_valid()) {
		if (curr_load_task) {
			// A task awaiting another => Let the awaiter accumulate the resource changed connections.
			DEV_ASSERT(curr_load_task != load_task_ptr);
			for (const ThreadLoadTask::ResourceChangedConnection &rcc : load_task_ptr->resource_changed_connections) {
				curr_load_task->resource_changed_connections.push_back(rcc);
			}
		} else {
			// A leaf task being awaited => Propagate the resource changed connections.
			if (Thread::is_main_thread()) {
				// On the main thread it's safe to migrate the connections to the standard signal mechanism.
				for (const ThreadLoadTask::ResourceChangedConnection &rcc : load_task_ptr->resource_changed_connections) {
					if (rcc.callable.is_valid()) {
						rcc.source->connect_changed(rcc.callable, rcc.flags);
					}
				}
			} else {
				// On non-main threads, we have to queue and call it done when processed.
				if (!load_task_ptr->resource_changed_connections.is_empty()) {
					for (const ThreadLoadTask::ResourceChangedConnection &rcc : load_task_ptr->resource_changed_connections) {
						if (rcc.callable.is_valid()) {
							MessageQueue::get_main_singleton()->push_callable(callable_mp(rcc.source, &Resource::connect_changed).bind(rcc.callable, rcc.flags));
						}
					}
					core_bind::Semaphore done;
					MessageQueue::get_main_singleton()->push_callable(callable_mp(&done, &core_bind::Semaphore::post).bind(1));
					done.wait();
				}
			}
		}
	}

	p_thread_load_lock.temp_relock();

	return resource;
}

bool ResourceLoader::_ensure_load_progress() {
	// Some servers may need a new engine iteration to allow the load to progress.
	// Since the only known one is the rendering server (in single thread mode), let's keep it simple and just sync it.
	// This may be refactored in the future to support other servers and have less coupling.
	if (OS::get_singleton()->is_separate_thread_rendering_enabled()) {
		return false; // Not needed.
	}
	RenderingServer::get_singleton()->sync();
	return true;
}

void ResourceLoader::resource_changed_connect(Resource *p_source, const Callable &p_callable, uint32_t p_flags) {
	print_lt(vformat("%d\t%ud:%s\t" FUNCTION_STR "\t%d", Thread::get_caller_id(), p_source->get_instance_id(), p_source->get_class(), p_callable.get_object_id()));

	MutexLock lock(thread_load_mutex);

	for (const ThreadLoadTask::ResourceChangedConnection &rcc : curr_load_task->resource_changed_connections) {
		if (unlikely(rcc.source == p_source && rcc.callable == p_callable)) {
			return;
		}
	}

	ThreadLoadTask::ResourceChangedConnection rcc;
	rcc.source = p_source;
	rcc.callable = p_callable;
	rcc.flags = p_flags;
	curr_load_task->resource_changed_connections.push_back(rcc);
}

void ResourceLoader::resource_changed_disconnect(Resource *p_source, const Callable &p_callable) {
	print_lt(vformat("%d\t%ud:%s\t" FUNCTION_STR "t%d", Thread::get_caller_id(), p_source->get_instance_id(), p_source->get_class(), p_callable.get_object_id()));

	MutexLock lock(thread_load_mutex);

	for (uint32_t i = 0; i < curr_load_task->resource_changed_connections.size(); ++i) {
		const ThreadLoadTask::ResourceChangedConnection &rcc = curr_load_task->resource_changed_connections[i];
		if (unlikely(rcc.source == p_source && rcc.callable == p_callable)) {
			curr_load_task->resource_changed_connections.remove_at_unordered(i);
			return;
		}
	}
}

void ResourceLoader::resource_changed_emit(Resource *p_source) {
	print_lt(vformat("%d\t%ud:%s\t" FUNCTION_STR, Thread::get_caller_id(), p_source->get_instance_id(), p_source->get_class()));

	MutexLock lock(thread_load_mutex);

	for (const ThreadLoadTask::ResourceChangedConnection &rcc : curr_load_task->resource_changed_connections) {
		if (unlikely(rcc.source == p_source)) {
			rcc.callable.call();
		}
	}
}

Ref<Resource> ResourceLoader::ensure_resource_ref_override_for_outer_load(const String &p_path, const String &p_res_type) {
	ERR_FAIL_COND_V(load_nesting == 0, Ref<Resource>()); // It makes no sense to use this from nesting level 0.
	const String &local_path = _validate_local_path(p_path);
	HashMap<String, Ref<Resource>> &overrides = res_ref_overrides[load_nesting - 1];
	HashMap<String, Ref<Resource>>::Iterator E = overrides.find(local_path);
	if (E) {
		return E->value;
	} else {
		Object *obj = ClassDB::instantiate(p_res_type);
		ERR_FAIL_NULL_V(obj, Ref<Resource>());
		Ref<Resource> res(obj);
		if (res.is_null()) {
			memdelete(obj);
			ERR_FAIL_V(Ref<Resource>());
		}
		overrides[local_path] = res;
		return res;
	}
}

Ref<Resource> ResourceLoader::get_resource_ref_override(const String &p_path) {
	DEV_ASSERT(p_path == _validate_local_path(p_path));
	HashMap<int, HashMap<String, Ref<Resource>>>::Iterator E = res_ref_overrides.find(load_nesting);
	if (!E) {
		return nullptr;
	}
	HashMap<String, Ref<Resource>>::Iterator F = E->value.find(p_path);
	if (!F) {
		return nullptr;
	}

	return F->value;
}

bool ResourceLoader::exists(const String &p_path, const String &p_type_hint) {
	String local_path = _validate_local_path(p_path);

	if (ResourceCache::has(local_path)) {
		return true; // If cached, it probably exists
	}

	bool xl_remapped = false;
	String path = _path_remap(local_path, &xl_remapped);

	// Try all loaders and pick the first match for the type hint
	for (int i = 0; i < loader_count; i++) {
		if (!loader[i]->recognize_path(path, p_type_hint)) {
			continue;
		}

		if (loader[i]->exists(path)) {
			return true;
		}
	}

	return false;
}

void ResourceLoader::add_resource_format_loader(Ref<ResourceFormatLoader> p_format_loader, bool p_at_front) {
	ERR_FAIL_COND(p_format_loader.is_null());
	ERR_FAIL_COND(loader_count >= MAX_LOADERS);

	if (p_at_front) {
		for (int i = loader_count; i > 0; i--) {
			loader[i] = loader[i - 1];
		}
		loader[0] = p_format_loader;
		loader_count++;
	} else {
		loader[loader_count++] = p_format_loader;
	}
}

void ResourceLoader::remove_resource_format_loader(Ref<ResourceFormatLoader> p_format_loader) {
	ERR_FAIL_COND(p_format_loader.is_null());

	// Find loader
	int i = 0;
	for (; i < loader_count; ++i) {
		if (loader[i] == p_format_loader) {
			break;
		}
	}

	ERR_FAIL_COND(i >= loader_count); // Not found

	// Shift next loaders up
	for (; i < loader_count - 1; ++i) {
		loader[i] = loader[i + 1];
	}
	loader[loader_count - 1].unref();
	--loader_count;
}

int ResourceLoader::get_import_order(const String &p_path) {
	String local_path = _path_remap(_validate_local_path(p_path));

	for (int i = 0; i < loader_count; i++) {
		if (!loader[i]->recognize_path(local_path)) {
			continue;
		}

		return loader[i]->get_import_order(p_path);
	}

	return 0;
}

String ResourceLoader::get_import_group_file(const String &p_path) {
	String local_path = _path_remap(_validate_local_path(p_path));

	for (int i = 0; i < loader_count; i++) {
		if (!loader[i]->recognize_path(local_path)) {
			continue;
		}

		return loader[i]->get_import_group_file(p_path);
	}

	return String(); //not found
}

bool ResourceLoader::is_import_valid(const String &p_path) {
	String local_path = _path_remap(_validate_local_path(p_path));

	for (int i = 0; i < loader_count; i++) {
		if (!loader[i]->recognize_path(local_path)) {
			continue;
		}

		return loader[i]->is_import_valid(p_path);
	}

	return false; //not found
}

bool ResourceLoader::is_imported(const String &p_path) {
	String local_path = _path_remap(_validate_local_path(p_path));

	for (int i = 0; i < loader_count; i++) {
		if (!loader[i]->recognize_path(local_path)) {
			continue;
		}

		return loader[i]->is_imported(p_path);
	}

	return false; //not found
}

void ResourceLoader::get_dependencies(const String &p_path, List<String> *p_dependencies, bool p_add_types) {
	String local_path = _path_remap(_validate_local_path(p_path));

	for (int i = 0; i < loader_count; i++) {
		if (!loader[i]->recognize_path(local_path)) {
			continue;
		}

		loader[i]->get_dependencies(local_path, p_dependencies, p_add_types);
	}
}

Error ResourceLoader::rename_dependencies(const String &p_path, const HashMap<String, String> &p_map) {
	String local_path = _path_remap(_validate_local_path(p_path));

	for (int i = 0; i < loader_count; i++) {
		if (!loader[i]->recognize_path(local_path)) {
			continue;
		}

		return loader[i]->rename_dependencies(local_path, p_map);
	}

	return OK; // ??
}

void ResourceLoader::get_classes_used(const String &p_path, HashSet<StringName> *r_classes) {
	String local_path = _validate_local_path(p_path);

	for (int i = 0; i < loader_count; i++) {
		if (!loader[i]->recognize_path(local_path)) {
			continue;
		}

		return loader[i]->get_classes_used(p_path, r_classes);
	}
}

String ResourceLoader::get_resource_type(const String &p_path) {
	String local_path = _validate_local_path(p_path);

	for (int i = 0; i < loader_count; i++) {
		String result = loader[i]->get_resource_type(local_path);
		if (!result.is_empty()) {
			return result;
		}
	}

	return "";
}

String ResourceLoader::get_resource_script_class(const String &p_path) {
	String local_path = _validate_local_path(p_path);

	for (int i = 0; i < loader_count; i++) {
		String result = loader[i]->get_resource_script_class(local_path);
		if (!result.is_empty()) {
			return result;
		}
	}

	return "";
}

ResourceUID::ID ResourceLoader::get_resource_uid(const String &p_path) {
	String local_path = _validate_local_path(p_path);

	for (int i = 0; i < loader_count; i++) {
		ResourceUID::ID id = loader[i]->get_resource_uid(local_path);
		if (id != ResourceUID::INVALID_ID) {
			return id;
		}
	}

	return ResourceUID::INVALID_ID;
}

bool ResourceLoader::has_custom_uid_support(const String &p_path) {
	String local_path = _validate_local_path(p_path);

	for (int i = 0; i < loader_count; i++) {
		if (!loader[i]->recognize_path(local_path)) {
			continue;
		}
		if (loader[i]->has_custom_uid_support()) {
			return true;
		}
	}

	return false;
}

bool ResourceLoader::should_create_uid_file(const String &p_path) {
	const String local_path = _validate_local_path(p_path);
	if (FileAccess::exists(local_path + ".uid")) {
		return false;
	}

	for (int i = 0; i < loader_count; i++) {
		if (loader[i]->recognize_path(local_path)) {
			return !loader[i]->has_custom_uid_support();
		}
	}
	return false;
}

String ResourceLoader::_path_remap(const String &p_path, bool *r_translation_remapped) {
	String new_path = p_path;

	if (translation_remaps.has(p_path)) {
		// translation_remaps has the following format:
		//   { "res://path.png": PackedStringArray( "res://path-ru.png:ru", "res://path-de.png:de" ) }

		// To find the path of the remapped resource, we extract the locale name after
		// the last ':' to match the project locale.

		// An extra remap may still be necessary afterwards due to the text -> binary converter on export.

		String locale = TranslationServer::get_singleton()->get_locale();
		ERR_FAIL_COND_V_MSG(locale.length() < 2, p_path, vformat("Could not remap path '%s' for translation as configured locale '%s' is invalid.", p_path, locale));

		Vector<String> &res_remaps = *translation_remaps.getptr(new_path);

		int best_score = 0;
		for (int i = 0; i < res_remaps.size(); i++) {
			int split = res_remaps[i].rfind_char(':');
			if (split == -1) {
				continue;
			}
			String l = res_remaps[i].substr(split + 1).strip_edges();
			int score = TranslationServer::get_singleton()->compare_locales(locale, l);
			if (score > 0 && score >= best_score) {
				new_path = res_remaps[i].left(split);
				best_score = score;
				if (score == 10) {
					break; // Exact match, skip the rest.
				}
			}
		}

		if (r_translation_remapped) {
			*r_translation_remapped = true;
		}

		// Fallback to p_path if new_path does not exist.
		if (!FileAccess::exists(new_path + ".import") && !FileAccess::exists(new_path)) {
			WARN_PRINT(vformat("Translation remap '%s' does not exist. Falling back to '%s'.", new_path, p_path));
			new_path = p_path;
		}
	}

	if (path_remaps.has(new_path)) {
		new_path = path_remaps[new_path];
	} else {
		// Try file remap.
		// Usually, there's no remap file and FileAccess::exists() is faster than FileAccess::open().
		new_path = ResourceUID::ensure_path(new_path);
		if (FileAccess::exists(new_path + ".remap")) {
			Error err;
			Ref<FileAccess> f = FileAccess::open(new_path + ".remap", FileAccess::READ, &err);
			if (f.is_valid()) {
				VariantParser::StreamFile stream;
				stream.f = f;

				String assign;
				Variant value;
				VariantParser::Tag next_tag;

				int lines = 0;
				String error_text;
				while (true) {
					assign = Variant();
					next_tag.fields.clear();
					next_tag.name = String();

					err = VariantParser::parse_tag_assign_eof(&stream, lines, error_text, next_tag, assign, value, nullptr, true);
					if (err == ERR_FILE_EOF) {
						break;
					} else if (err != OK) {
						ERR_PRINT(vformat("Parse error: %s.remap:%d error: %s.", p_path, lines, error_text));
						break;
					}

					if (assign == "path") {
						new_path = value;
						break;
					} else if (next_tag.name != "remap") {
						break;
					}
				}
			}
		}
	}

	return new_path;
}

String ResourceLoader::import_remap(const String &p_path) {
	if (ResourceFormatImporter::get_singleton()->recognize_path(p_path)) {
		return ResourceFormatImporter::get_singleton()->get_internal_resource_path(p_path);
	}

	return p_path;
}

String ResourceLoader::path_remap(const String &p_path) {
	return _path_remap(p_path);
}

void ResourceLoader::reload_translation_remaps() {
	List<Resource *> to_reload;

	{
		MutexLock lock(ResourceCache::lock);
		SelfList<Resource> *E = remapped_list.first();

		while (E) {
			to_reload.push_back(E->self());
			E = E->next();
		}
	}

	//now just make sure to not delete any of these resources while changing locale..
	while (to_reload.front()) {
		to_reload.front()->get()->reload_from_file();
		to_reload.pop_front();
	}
}

void ResourceLoader::load_translation_remaps() {
	if (!ProjectSettings::get_singleton()->has_setting("internationalization/locale/translation_remaps")) {
		return;
	}

	Dictionary remaps = GLOBAL_GET("internationalization/locale/translation_remaps");
	List<Variant> keys;
	remaps.get_key_list(&keys);
	for (const Variant &E : keys) {
		Array langs = remaps[E];
		Vector<String> lang_remaps;
		lang_remaps.resize(langs.size());
		String *lang_remaps_ptrw = lang_remaps.ptrw();
		for (const Variant &lang : langs) {
			*lang_remaps_ptrw++ = lang;
		}

		translation_remaps[String(E)] = lang_remaps;
	}
}

void ResourceLoader::clear_translation_remaps() {
	translation_remaps.clear();
	while (remapped_list.first() != nullptr) {
		remapped_list.remove(remapped_list.first());
	}
}

void ResourceLoader::clear_thread_load_tasks() {
	// Bring the thing down as quickly as possible without causing deadlocks or leaks.

	MutexLock thread_load_lock(thread_load_mutex);
	cleaning_tasks = true;

	while (true) {
		bool none_running = true;
		if (thread_load_tasks.size()) {
			for (KeyValue<String, ResourceLoader::ThreadLoadTask> &E : thread_load_tasks) {
				if (E.value.status == THREAD_LOAD_IN_PROGRESS) {
					if (E.value.cond_var && E.value.need_wait) {
						E.value.cond_var->notify_all();
					}
					E.value.need_wait = false;
					none_running = false;
				}
			}
		}
		if (none_running) {
			break;
		}
		thread_load_lock.temp_unlock();
		OS::get_singleton()->delay_usec(1000);
		thread_load_lock.temp_relock();
	}

	while (user_load_tokens.begin()) {
		LoadToken *user_token = user_load_tokens.begin()->value;
		user_load_tokens.remove(user_load_tokens.begin());
		DEV_ASSERT(user_token->user_rc > 0 && !user_token->user_path.is_empty());
		user_token->user_path.clear();
		user_token->user_rc = 0;
		user_token->unreference();
	}

	thread_load_tasks.clear();

	cleaning_tasks = false;
}

void ResourceLoader::load_path_remaps() {
	if (!ProjectSettings::get_singleton()->has_setting("path_remap/remapped_paths")) {
		return;
	}

	Vector<String> remaps = GLOBAL_GET("path_remap/remapped_paths");
	int rc = remaps.size();
	ERR_FAIL_COND(rc & 1); //must be even
	const String *r = remaps.ptr();

	for (int i = 0; i < rc; i += 2) {
		path_remaps[r[i]] = r[i + 1];
	}
}

void ResourceLoader::clear_path_remaps() {
	path_remaps.clear();
}

void ResourceLoader::set_load_callback(ResourceLoadedCallback p_callback) {
	_loaded_callback = p_callback;
}

ResourceLoadedCallback ResourceLoader::_loaded_callback = nullptr;

Ref<ResourceFormatLoader> ResourceLoader::_find_custom_resource_format_loader(const String &path) {
	for (int i = 0; i < loader_count; ++i) {
		if (loader[i]->get_script_instance() && loader[i]->get_script_instance()->get_script()->get_path() == path) {
			return loader[i];
		}
	}
	return Ref<ResourceFormatLoader>();
}

bool ResourceLoader::add_custom_resource_format_loader(const String &script_path) {
	if (_find_custom_resource_format_loader(script_path).is_valid()) {
		return false;
	}

	Ref<Resource> res = ResourceLoader::load(script_path);
	ERR_FAIL_COND_V(res.is_null(), false);
	ERR_FAIL_COND_V(!res->is_class("Script"), false);

	Ref<Script> s = res;
	StringName ibt = s->get_instance_base_type();
	bool valid_type = ClassDB::is_parent_class(ibt, "ResourceFormatLoader");
	ERR_FAIL_COND_V_MSG(!valid_type, false, vformat("Failed to add a custom resource loader, script '%s' does not inherit 'ResourceFormatLoader'.", script_path));

	Object *obj = ClassDB::instantiate(ibt);
	ERR_FAIL_NULL_V_MSG(obj, false, vformat("Failed to add a custom resource loader, cannot instantiate '%s'.", ibt));

	Ref<ResourceFormatLoader> crl = Object::cast_to<ResourceFormatLoader>(obj);
	crl->set_script(s);
	ResourceLoader::add_resource_format_loader(crl);

	return true;
}

void ResourceLoader::set_create_missing_resources_if_class_unavailable(bool p_enable) {
	create_missing_resources_if_class_unavailable = p_enable;
}

void ResourceLoader::add_custom_loaders() {
	// Custom loaders registration exploits global class names

	String custom_loader_base_class = ResourceFormatLoader::get_class_static();

	List<StringName> global_classes;
	ScriptServer::get_global_class_list(&global_classes);

	for (const StringName &class_name : global_classes) {
		StringName base_class = ScriptServer::get_global_class_native_base(class_name);

		if (base_class == custom_loader_base_class) {
			String path = ScriptServer::get_global_class_path(class_name);
			add_custom_resource_format_loader(path);
		}
	}
}

void ResourceLoader::remove_custom_loaders() {
	Vector<Ref<ResourceFormatLoader>> custom_loaders;
	for (int i = 0; i < loader_count; ++i) {
		if (loader[i]->get_script_instance()) {
			custom_loaders.push_back(loader[i]);
		}
	}

	for (int i = 0; i < custom_loaders.size(); ++i) {
		remove_resource_format_loader(custom_loaders[i]);
	}
}

bool ResourceLoader::is_cleaning_tasks() {
	MutexLock lock(thread_load_mutex);
	return cleaning_tasks;
}

Vector<String> ResourceLoader::list_directory(const String &p_directory) {
	RBSet<String> files_found;
	Ref<DirAccess> dir = DirAccess::open(p_directory);
	if (dir.is_null()) {
		return Vector<String>();
	}

	Error err = dir->list_dir_begin();
	if (err != OK) {
		return Vector<String>();
	}

	String d = dir->get_next();
	while (!d.is_empty()) {
		bool recognized = false;
		if (dir->current_is_dir()) {
			if (d != "." && d != "..") {
				d += "/";
				recognized = true;
			}
		} else {
			if (d.ends_with(".import") || d.ends_with(".remap") || d.ends_with(".uid")) {
				d = d.substr(0, d.rfind_char('.'));
			}

			if (d.ends_with(".gdc")) {
				d = d.substr(0, d.rfind_char('.'));
				d += ".gd";
			}

			const String full_path = p_directory.path_join(d);
			// Try all loaders and pick the first match for the type hint.
			for (int i = 0; i < loader_count; i++) {
				if (loader[i]->recognize_path(full_path)) {
					recognized = true;
					break;
				}
			}
		}

		if (recognized) {
			files_found.insert(d);
		}
		d = dir->get_next();
	}

	Vector<String> ret;
	for (const String &f : files_found) {
		ret.push_back(f);
	}

	return ret;
}

void ResourceLoader::initialize() {}

void ResourceLoader::finalize() {}

ResourceLoadErrorNotify ResourceLoader::err_notify = nullptr;
DependencyErrorNotify ResourceLoader::dep_err_notify = nullptr;

bool ResourceLoader::create_missing_resources_if_class_unavailable = false;
bool ResourceLoader::abort_on_missing_resource = true;
bool ResourceLoader::timestamp_on_load = false;

thread_local int ResourceLoader::load_nesting = 0;
thread_local Vector<String> ResourceLoader::load_paths_stack;
thread_local HashMap<int, HashMap<String, Ref<Resource>>> ResourceLoader::res_ref_overrides;
thread_local ResourceLoader::ThreadLoadTask *ResourceLoader::curr_load_task = nullptr;

SafeBinaryMutex<ResourceLoader::BINARY_MUTEX_TAG> &_get_res_loader_mutex() {
	return ResourceLoader::thread_load_mutex;
}

template <>
thread_local SafeBinaryMutex<ResourceLoader::BINARY_MUTEX_TAG>::TLSData SafeBinaryMutex<ResourceLoader::BINARY_MUTEX_TAG>::tls_data(_get_res_loader_mutex());
SafeBinaryMutex<ResourceLoader::BINARY_MUTEX_TAG> ResourceLoader::thread_load_mutex;
HashMap<String, ResourceLoader::ThreadLoadTask> ResourceLoader::thread_load_tasks;
bool ResourceLoader::cleaning_tasks = false;

HashMap<String, ResourceLoader::LoadToken *> ResourceLoader::user_load_tokens;

SelfList<Resource>::List ResourceLoader::remapped_list;
HashMap<String, Vector<String>> ResourceLoader::translation_remaps;
HashMap<String, String> ResourceLoader::path_remaps;

ResourceLoaderImport ResourceLoader::import = nullptr;

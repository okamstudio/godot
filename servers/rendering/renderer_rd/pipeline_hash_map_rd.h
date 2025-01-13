/**************************************************************************/
/*  pipeline_hash_map_rd.h                                                */
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

#ifndef PIPELINE_HASH_MAP_RD_H
#define PIPELINE_HASH_MAP_RD_H

#include "servers/rendering/rendering_device.h"
#include "servers/rendering_server.h"

#define PRINT_PIPELINE_COMPILATION_KEYS 0

template <typename Key, typename CreationClass, typename CreationFunction>
class PipelineHashMapRD {
private:
	CreationClass *creation_object = nullptr;
	CreationFunction creation_function = nullptr;
	Mutex *compilations_mutex = nullptr;
	uint32_t *compilations = nullptr;
	RBMap<uint32_t, RID> hash_map;
	LocalVector<Pair<uint32_t, RID>> compiled_queue;
	Mutex compiled_queue_mutex;
	RBSet<uint32_t> compilation_set;
	HashMap<uint32_t, WorkerThreadPool::TaskID> compilation_tasks;
	Mutex local_mutex;

	bool _add_new_pipelines_to_map() {
		thread_local Vector<uint32_t> hashes_added;
		hashes_added.clear();

		{
			MutexLock lock(compiled_queue_mutex);
			for (const Pair<uint32_t, RID> &pair : compiled_queue) {
				hash_map[pair.first] = pair.second;
				hashes_added.push_back(pair.first);
			}

			compiled_queue.clear();
		}

		{
			MutexLock local_lock(local_mutex);
			for (uint32_t hash : hashes_added) {
				HashMap<uint32_t, WorkerThreadPool::TaskID>::Iterator task_it = compilation_tasks.find(hash);
				if (task_it != compilation_tasks.end()) {
					compilation_tasks.remove(task_it);
				}
			}
		}

		return !hashes_added.is_empty();
	}

	void _wait_for_all_pipelines() {
		MutexLock local_lock(local_mutex);
		for (KeyValue<uint32_t, WorkerThreadPool::TaskID> key_value : compilation_tasks) {
			WorkerThreadPool::get_singleton()->wait_for_task_completion(key_value.value);
		}
	}

public:
	void add_compiled_pipeline(uint32_t p_hash, RID p_pipeline) {
		compiled_queue_mutex.lock();
		compiled_queue.push_back({ p_hash, p_pipeline });
		compiled_queue_mutex.unlock();
	}

	// Start compilation of a pipeline ahead of time in the background. Returns true if the compilation was started, false if it wasn't required. Source is only used for collecting statistics.
	void compile_pipeline(const Key &p_key, uint32_t p_key_hash, RS::PipelineSource p_source) {
		DEV_ASSERT((creation_object != nullptr) && (creation_function != nullptr) && "Creation object and function was not set before attempting to compile a pipeline.");

		MutexLock local_lock(local_mutex);
		if (compilation_set.has(p_key_hash)) {
			// Check if the pipeline was already submitted.
			return;
		}

		// Record the pipeline as submitted, a task can't be started for it again.
		compilation_set.insert(p_key_hash);

		if (compilations_mutex != nullptr) {
			MutexLock compilations_lock(*compilations_mutex);
			compilations[p_source]++;
		}

#if PRINT_PIPELINE_COMPILATION_KEYS
		String source_name = "UNKNOWN";
		switch (p_source) {
			case RS::PIPELINE_SOURCE_CANVAS:
				source_name = "CANVAS";
				break;
			case RS::PIPELINE_SOURCE_MESH:
				source_name = "MESH";
				break;
			case RS::PIPELINE_SOURCE_SURFACE:
				source_name = "SURFACE";
				break;
			case RS::PIPELINE_SOURCE_DRAW:
				source_name = "DRAW";
				break;
			case RS::PIPELINE_SOURCE_SPECIALIZATION:
				source_name = "SPECIALIZATION";
				break;
		}

		print_line("HASH:", p_key_hash, "SOURCE:", source_name);
#endif

		// Queue a background compilation task.
		WorkerThreadPool::TaskID task_id = WorkerThreadPool::get_singleton()->add_template_task(creation_object, creation_function, p_key, false, "PipelineCompilation");
		compilation_tasks.insert(p_key_hash, task_id);
	}

	void wait_for_pipeline(uint32_t p_key_hash) {
		MutexLock local_lock(local_mutex);
		if (!compilation_set.has(p_key_hash)) {
			// The pipeline was never submitted, we can't wait for it.
			return;
		}

		HashMap<uint32_t, WorkerThreadPool::TaskID>::Iterator task_it = compilation_tasks.find(p_key_hash);
		if (task_it != compilation_tasks.end()) {
			// Wait for and remove the compilation task if it exists.
			WorkerThreadPool::get_singleton()->wait_for_task_completion(task_it->value);
			compilation_tasks.remove(task_it);
		}
	}

	// Retrieve a pipeline. It'll return an empty pipeline if it's not available yet, but it'll be guaranteed to succeed if 'wait for compilation' is true and stall as necessary. Source is just an optional number to aid debugging.
	RID get_pipeline(const Key &p_key, uint32_t p_key_hash, bool p_wait_for_compilation, RS::PipelineSource p_source) {
		RBMap<uint32_t, RID>::Element *e = hash_map.find(p_key_hash);

		if (e == nullptr) {
			// Check if there's any new pipelines that need to be added and try again. This method triggers a mutex lock.
			if (_add_new_pipelines_to_map()) {
				e = hash_map.find(p_key_hash);
			}
		}

		if (e == nullptr) {
			// Request compilation. The method will ignore the request if it's already being compiled.
			compile_pipeline(p_key, p_key_hash, p_source);

			if (p_wait_for_compilation) {
				wait_for_pipeline(p_key_hash);
				_add_new_pipelines_to_map();

				e = hash_map.find(p_key_hash);
				if (e != nullptr) {
					return e->value();
				} else {
					// Pipeline could not be compiled due to an internal error. Store an empty RID so compilation is not attempted again.
					hash_map[p_key_hash] = RID();
					return RID();
				}
			} else {
				return RID();
			}
		} else {
			return e->value();
		}
	}

	// Delete all cached pipelines. Can stall if background compilation is in progress.
	void clear_pipelines() {
		_wait_for_all_pipelines();
		_add_new_pipelines_to_map();

		for (KeyValue<uint32_t, RID> entry : hash_map) {
			RD::get_singleton()->free(entry.value);
		}

		hash_map.clear();
		compilation_set.clear();
	}

	// Set the external pipeline compilations array to increase the counters on every time a pipeline is compiled.
	void set_compilations(uint32_t *p_compilations, Mutex *p_compilations_mutex) {
		compilations = p_compilations;
		compilations_mutex = p_compilations_mutex;
	}

	void set_creation_object_and_function(CreationClass *p_creation_object, CreationFunction p_creation_function) {
		creation_object = p_creation_object;
		creation_function = p_creation_function;
	}

	PipelineHashMapRD() {}

	~PipelineHashMapRD() {
		clear_pipelines();
	}
};

#endif // PIPELINE_HASH_MAP_RD_H

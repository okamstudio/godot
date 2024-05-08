/**************************************************************************/
/*  utilities.h                                                           */
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

#ifndef RENDERER_UTILITIES_H
#define RENDERER_UTILITIES_H

#include "servers/rendering_server.h"

class DependencyTracker;

class Dependency {
public:
	enum DependencyChangedNotification {
		DEPENDENCY_CHANGED_AABB,
		DEPENDENCY_CHANGED_MATERIAL,
		DEPENDENCY_CHANGED_MESH,
		DEPENDENCY_CHANGED_MULTIMESH,
		DEPENDENCY_CHANGED_MULTIMESH_VISIBLE_INSTANCES,
		DEPENDENCY_CHANGED_PARTICLES,
		DEPENDENCY_CHANGED_PARTICLES_INSTANCES,
		DEPENDENCY_CHANGED_DECAL,
		DEPENDENCY_CHANGED_SKELETON_DATA,
		DEPENDENCY_CHANGED_SKELETON_BONES,
		DEPENDENCY_CHANGED_LIGHT,
		DEPENDENCY_CHANGED_LIGHT_SOFT_SHADOW_AND_PROJECTOR,
		DEPENDENCY_CHANGED_REFLECTION_PROBE,
	};

	void changed_notify(DependencyChangedNotification p_notification);
	void deleted_notify(const RID &p_rid);

	~Dependency();

private:
	friend class DependencyTracker;
	HashMap<DependencyTracker *, uint32_t> instances;
};

class DependencyTracker {
public:
	void *userdata = nullptr;
	typedef void (*ChangedCallback)(Dependency::DependencyChangedNotification, DependencyTracker *);
	typedef void (*DeletedCallback)(const RID &, DependencyTracker *);

	ChangedCallback changed_callback = nullptr;
	DeletedCallback deleted_callback = nullptr;

	void update_begin() { // call before updating dependencies
		instance_version++;
	}

	void update_dependency(Dependency *p_dependency) { //called internally, can't be used directly, use update functions in Storage
		dependencies.insert(p_dependency);
		p_dependency->instances[this] = instance_version;
	}

	void update_end() { //call after updating dependencies
		List<Pair<Dependency *, DependencyTracker *>> to_clean_up;

		for (Dependency *E : dependencies) {
			Dependency *dep = E;
			HashMap<DependencyTracker *, uint32_t>::Iterator F = dep->instances.find(this);
			ERR_CONTINUE(!F);
			if (F->value != instance_version) {
				Pair<Dependency *, DependencyTracker *> p;
				p.first = dep;
				p.second = F->key;
				to_clean_up.push_back(p);
			}
		}

		while (to_clean_up.size()) {
			to_clean_up.get_front().first->instances.erase(to_clean_up.get_front().second);
			dependencies.erase(to_clean_up.get_front().first);
			to_clean_up.pop_front();
		}
	}

	void clear() { // clear all dependencies
		for (Dependency *E : dependencies) {
			Dependency *dep = E;
			dep->instances.erase(this);
		}
		dependencies.clear();
	}

	~DependencyTracker() { clear(); }

private:
	friend class Dependency;
	uint32_t instance_version = 0;
	HashSet<Dependency *> dependencies;
};

class RendererUtilities {
public:
	virtual ~RendererUtilities() {}

	/* INSTANCES */

	virtual RS::InstanceType get_base_type(RID p_rid) const = 0;
	virtual bool free(RID p_rid) = 0;

	/* DEPENDENCIES */

	virtual void base_update_dependency(RID p_base, DependencyTracker *p_instance) = 0;

	/* VISIBILITY NOTIFIER */

	virtual RID visibility_notifier_allocate() = 0;
	virtual void visibility_notifier_initialize(RID p_notifier) = 0;
	virtual void visibility_notifier_free(RID p_notifier) = 0;

	virtual void visibility_notifier_set_aabb(RID p_notifier, const AABB &p_aabb) = 0;
	virtual void visibility_notifier_set_callbacks(RID p_notifier, const Callable &p_enter_callbable, const Callable &p_exit_callable) = 0;

	virtual AABB visibility_notifier_get_aabb(RID p_notifier) const = 0;
	virtual void visibility_notifier_call(RID p_notifier, bool p_enter, bool p_deferred) = 0;

	/* TIMING */

	bool capturing_timestamps = false;

#define TIMESTAMP_BEGIN()                               \
	{                                                   \
		if (RSG::utilities->capturing_timestamps)       \
			RSG::utilities->capture_timestamps_begin(); \
	}

#define RENDER_TIMESTAMP(m_text)                       \
	{                                                  \
		if (RSG::utilities->capturing_timestamps)      \
			RSG::utilities->capture_timestamp(m_text); \
	}

	virtual void capture_timestamps_begin() = 0;
	virtual void capture_timestamp(const String &p_name) = 0;
	virtual uint32_t get_captured_timestamps_count() const = 0;
	virtual uint64_t get_captured_timestamps_frame() const = 0;
	virtual uint64_t get_captured_timestamp_gpu_time(uint32_t p_index) const = 0;
	virtual uint64_t get_captured_timestamp_cpu_time(uint32_t p_index) const = 0;
	virtual String get_captured_timestamp_name(uint32_t p_index) const = 0;

	/* MISC */

	virtual void update_dirty_resources() = 0;
	virtual void set_debug_generate_wireframes(bool p_generate) = 0;

	virtual bool has_os_feature(const String &p_feature) const = 0;

	virtual void update_memory_info() = 0;

	virtual uint64_t get_rendering_info(RS::RenderingInfo p_info) = 0;
	virtual String get_video_adapter_name() const = 0;
	virtual String get_video_adapter_vendor() const = 0;
	virtual RenderingDevice::DeviceType get_video_adapter_type() const = 0;
	virtual String get_video_adapter_api_version() const = 0;

	virtual Size2i get_maximum_viewport_size() const = 0;
};

#endif // RENDERER_UTILITIES_H

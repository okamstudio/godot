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

#ifndef UTILITIES_DUMMY_H
#define UTILITIES_DUMMY_H

#include "light_storage.h"
#include "material_storage.h"
#include "mesh_storage.h"
#include "servers/rendering/storage/utilities.h"
#include "texture_storage.h"

namespace RendererDummy {

class Utilities : public RendererUtilities {
private:
	static Utilities *singleton;

public:
	static Utilities *get_singleton() { return singleton; }

	Utilities();
	~Utilities();

	/* INSTANCES */

	virtual RS::InstanceType get_base_type(RID p_rid) const override {
		if (RendererDummy::MeshStorage::get_singleton()->owns_mesh(p_rid)) {
			return RS::INSTANCE_MESH;
		} else if (RendererDummy::MeshStorage::get_singleton()->owns_multimesh(p_rid)) {
			return RS::INSTANCE_MULTIMESH;
		} else if (RendererDummy::LightStorage::get_singleton()->owns_lightmap(p_rid)) {
			return RS::INSTANCE_LIGHTMAP;
		}
		return RS::INSTANCE_NONE;
	}

	virtual bool free(RID p_rid) override {
		if (RendererDummy::LightStorage::get_singleton()->free(p_rid)) {
			return true;
		} else if (RendererDummy::TextureStorage::get_singleton()->owns_texture(p_rid)) {
			RendererDummy::TextureStorage::get_singleton()->texture_free(p_rid);
			return true;
		} else if (RendererDummy::MeshStorage::get_singleton()->owns_mesh(p_rid)) {
			RendererDummy::MeshStorage::get_singleton()->mesh_free(p_rid);
			return true;
		} else if (RendererDummy::MeshStorage::get_singleton()->owns_multimesh(p_rid)) {
			RendererDummy::MeshStorage::get_singleton()->multimesh_free(p_rid);
			return true;
		} else if (RendererDummy::MaterialStorage::get_singleton()->owns_shader(p_rid)) {
			RendererDummy::MaterialStorage::get_singleton()->shader_free(p_rid);
			return true;
		} else if (RendererDummy::MaterialStorage::get_singleton()->owns_material(p_rid)) {
			RendererDummy::MaterialStorage::get_singleton()->material_free(p_rid);
			return true;
		}
		return false;
	}

	/* DEPENDENCIES */

	virtual void base_update_dependency(RID p_base, DependencyTracker *p_instance) override {}

	/* VISIBILITY NOTIFIER */

	virtual RID visibility_notifier_allocate() override { return RID(); }
	virtual void visibility_notifier_initialize(RID p_notifier) override {}
	virtual void visibility_notifier_free(RID p_notifier) override {}

	virtual void visibility_notifier_set_aabb(RID p_notifier, const AABB &p_aabb) override {}
	virtual void visibility_notifier_set_callbacks(RID p_notifier, const Callable &p_enter_callbable, const Callable &p_exit_callable) override {}

	virtual AABB visibility_notifier_get_aabb(RID p_notifier) const override { return AABB(); }
	virtual void visibility_notifier_call(RID p_notifier, bool p_enter, bool p_deferred) override {}

	/* TIMING */

	virtual void capture_timestamps_begin() override {}
	virtual void capture_timestamp(const String &p_name) override {}
	virtual uint32_t get_captured_timestamps_count() const override { return 0; }
	virtual uint64_t get_captured_timestamps_frame() const override { return 0; }
	virtual uint64_t get_captured_timestamp_gpu_time(uint32_t p_index) const override { return 0; }
	virtual uint64_t get_captured_timestamp_cpu_time(uint32_t p_index) const override { return 0; }
	virtual String get_captured_timestamp_name(uint32_t p_index) const override { return String(); }

	/* MISC */

	virtual void update_dirty_resources() override {}
	virtual void set_debug_generate_wireframes(bool p_generate) override {}

	virtual bool has_os_feature(const String &p_feature) const override {
		return p_feature == "rgtc" || p_feature == "bptc" || p_feature == "s3tc" || p_feature == "etc2";
	}

	virtual void update_memory_info() override {}

	virtual uint64_t get_rendering_info(RS::RenderingInfo p_info) override { return 0; }
	virtual String get_video_adapter_name() const override { return String(); }
	virtual String get_video_adapter_vendor() const override { return String(); }
	virtual RenderingDevice::DeviceType get_video_adapter_type() const override { return RenderingDevice::DeviceType::DEVICE_TYPE_OTHER; }
	virtual String get_video_adapter_api_version() const override { return String(); }

	virtual Size2i get_maximum_viewport_size() const override { return Size2i(); }
};

} // namespace RendererDummy

#endif // UTILITIES_DUMMY_H

/**************************************************************************/
/*  raycast_occlusion_cull.h                                              */
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

#pragma once

#include "core/math/projection.h"
#include "core/templates/local_vector.h"
#include "core/templates/rid_owner.h"
#include "servers/rendering/renderer_scene_occlusion_cull.h"

#include <embree4/rtcore.h>

class RaycastOcclusionCull : public RendererSceneOcclusionCull {
	typedef RTCRayHit16 CameraRayTile;

public:
	class RaycastHZBuffer : public HZBuffer {
	private:
		Size2i tile_grid_size;

		struct CameraRayThreadData {
			int thread_count;
			float z_near;
			float z_far;
			Vector3 camera_dir;
			Vector3 camera_pos;
			Vector3 pixel_corner;
			Vector3 pixel_u_interp;
			Vector3 pixel_v_interp;
			bool camera_orthogonal;
			Size2i buffer_size;
		};

		void _camera_rays_threaded(uint32_t p_thread, const CameraRayThreadData *p_data);
		void _generate_camera_rays(const CameraRayThreadData *p_data, int p_from, int p_to);

	public:
		unsigned int camera_rays_tile_count = 0;
		uint8_t *camera_rays_unaligned_buffer = nullptr;
		CameraRayTile *camera_rays = nullptr;
		LocalVector<uint32_t> camera_ray_masks;
		RID scenario_rid;

		virtual void clear() override;
		virtual void resize(const Size2i &p_size) override;
		void sort_rays(const Vector3 &p_camera_dir, bool p_orthogonal);
		void update_camera_rays(const Transform3D &p_cam_transform, const Vector3 &p_near_bottom_left, const Vector2 &p_near_extents, real_t p_z_far, bool p_cam_orthogonal);

		~RaycastHZBuffer();
	};

private:
	struct InstanceID {
		RID scenario;
		RID instance;

		static uint32_t hash(const InstanceID &p_ins) {
			uint32_t h = hash_murmur3_one_64(p_ins.scenario.get_id());
			return hash_fmix32(hash_murmur3_one_64(p_ins.instance.get_id(), h));
		}
		bool operator==(const InstanceID &rhs) const {
			return instance == rhs.instance && rhs.scenario == scenario;
			;
		}

		InstanceID() {}
		InstanceID(RID s, RID i) :
				scenario(s), instance(i) {}
	};

	struct Occluder {
		PackedVector3Array vertices;
		PackedInt32Array indices;
		HashSet<InstanceID, InstanceID> users;
	};

	struct OccluderInstance {
		RID occluder;
		LocalVector<uint32_t> indices;
		LocalVector<float> xformed_vertices;
		Transform3D xform;
		bool enabled = true;
		bool removed = false;
	};

	struct Scenario {
		struct RaycastThreadData {
			CameraRayTile *rays = nullptr;
			const uint32_t *masks;
		};

		struct TransformThreadData {
			uint32_t thread_count;
			uint32_t vertex_count;
			Transform3D xform;
			const Vector3 *read;
			float *write = nullptr;
		};

		Thread *commit_thread = nullptr;
		bool commit_done = true;
		bool dirty = false;

		RTCScene ebr_scene[2] = { nullptr, nullptr };
		int current_scene_idx = 0;

		HashMap<RID, OccluderInstance> instances;
		HashSet<RID> dirty_instances; // To avoid duplicates
		LocalVector<RID> dirty_instances_array; // To iterate and split into threads
		LocalVector<RID> removed_instances;

		void _update_dirty_instance_thread(int p_idx, RID *p_instances);
		void _update_dirty_instance(int p_idx, RID *p_instances);
		void _transform_vertices_thread(uint32_t p_thread, TransformThreadData *p_data);
		void _transform_vertices_range(const Vector3 *p_read, float *p_write, const Transform3D &p_xform, int p_from, int p_to);
		static void _commit_scene(void *p_ud);
		void free();
		void update();

		void _raycast(uint32_t p_thread, const RaycastThreadData *p_raycast_data) const;
		void raycast(CameraRayTile *r_rays, const uint32_t *p_valid_masks, uint32_t p_tile_count) const;
	};

	static RaycastOcclusionCull *raycast_singleton;

	static const int TILE_SIZE = 4;
	static const int TILE_RAYS = TILE_SIZE * TILE_SIZE;

	RTCDevice ebr_device = nullptr;
	RID_PtrOwner<Occluder> occluder_owner;
	HashMap<RID, Scenario> scenarios;
	HashMap<RID, RaycastHZBuffer> buffers;
	RS::ViewportOcclusionCullingBuildQuality build_quality;
	bool _jitter_enabled = false;

	void _init_embree();
	Vector2 _jitter_half_extents(const Vector2 &p_half_extents, const Size2i &p_viewport_size);

public:
	virtual bool is_occluder(RID p_rid) override;
	virtual RID occluder_allocate() override;
	virtual void occluder_initialize(RID p_occluder) override;
	virtual void occluder_set_mesh(RID p_occluder, const PackedVector3Array &p_vertices, const PackedInt32Array &p_indices) override;
	virtual void free_occluder(RID p_occluder) override;

	virtual void add_scenario(RID p_scenario) override;
	virtual void remove_scenario(RID p_scenario) override;
	virtual void scenario_set_instance(RID p_scenario, RID p_instance, RID p_occluder, const Transform3D &p_xform, bool p_enabled) override;
	virtual void scenario_remove_instance(RID p_scenario, RID p_instance) override;

	virtual void add_buffer(RID p_buffer) override;
	virtual void remove_buffer(RID p_buffer) override;
	virtual HZBuffer *buffer_get_ptr(RID p_buffer) override;
	virtual void buffer_set_scenario(RID p_buffer, RID p_scenario) override;
	virtual void buffer_set_size(RID p_buffer, const Vector2i &p_size) override;
	virtual void buffer_update(RID p_buffer, const Transform3D &p_cam_transform, const Projection &p_cam_projection, bool p_cam_orthogonal) override;

	virtual RID buffer_get_debug_texture(RID p_buffer) override;

	virtual void set_build_quality(RS::ViewportOcclusionCullingBuildQuality p_quality) override;

	RaycastOcclusionCull();
	~RaycastOcclusionCull();
};

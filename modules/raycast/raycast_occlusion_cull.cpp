/**************************************************************************/
/*  raycast_occlusion_cull.cpp                                            */
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

#include "raycast_occlusion_cull.h"

#include "core/config/project_settings.h"
#include "core/object/worker_thread_pool.h"
#include "core/templates/local_vector.h"

#ifdef __SSE2__
#include <pmmintrin.h>
#endif

RaycastOcclusionCull *RaycastOcclusionCull::raycast_singleton = nullptr;

void RaycastOcclusionCull::RaycastHZBuffer::clear() {
	HZBuffer::clear();

	if (camera_rays_unaligned_buffer) {
		memfree(camera_rays_unaligned_buffer);
		camera_rays_unaligned_buffer = nullptr;
		camera_rays = nullptr;
	}
	camera_ray_masks.clear();
	camera_rays_tile_count = 0;
	tile_grid_size = Size2i();
}

void RaycastOcclusionCull::RaycastHZBuffer::resize(const Size2i &p_size) {
	if (p_size == Size2i()) {
		clear();
		return;
	}

	if (!sizes.is_empty() && p_size == sizes[0]) {
		return; // Size didn't change
	}

	HZBuffer::resize(p_size);

	tile_grid_size = Size2i(Math::ceil(p_size.x / (float)TILE_SIZE), Math::ceil(p_size.y / (float)TILE_SIZE));
	camera_rays_tile_count = tile_grid_size.x * tile_grid_size.y;

	if (camera_rays_unaligned_buffer) {
		memfree(camera_rays_unaligned_buffer);
	}

	const int alignment = 64; // Embree requires ray packets to be 64-aligned
	camera_rays_unaligned_buffer = (uint8_t *)memalloc(camera_rays_tile_count * sizeof(CameraRayTile) + alignment);
	camera_rays = (CameraRayTile *)(camera_rays_unaligned_buffer + alignment - (((uint64_t)camera_rays_unaligned_buffer) % alignment));

	camera_ray_masks.resize(camera_rays_tile_count * TILE_RAYS);
	memset(camera_ray_masks.ptr(), ~0, camera_rays_tile_count * TILE_RAYS * sizeof(uint32_t));
}

void RaycastOcclusionCull::RaycastHZBuffer::update_camera_rays(const Transform3D &p_cam_transform, const Projection &p_cam_projection, bool p_cam_orthogonal) {
	CameraRayThreadData td;
	td.thread_count = WorkerThreadPool::get_singleton()->get_thread_count();

	td.z_near = p_cam_projection.get_z_near();
	td.z_far = p_cam_projection.get_z_far() * 1.05f;
	td.camera_pos = p_cam_transform.origin;
	td.camera_dir = -p_cam_transform.basis.get_column(2);
	td.camera_orthogonal = p_cam_orthogonal;

	Projection inv_camera_matrix = p_cam_projection.inverse();
	Vector3 camera_corner_proj = Vector3(-1.0f, -1.0f, -1.0f);
	Vector3 camera_corner_view = inv_camera_matrix.xform(camera_corner_proj);
	td.pixel_corner = p_cam_transform.xform(camera_corner_view);

	Vector3 top_corner_proj = Vector3(-1.0f, 1.0f, -1.0f);
	Vector3 top_corner_view = inv_camera_matrix.xform(top_corner_proj);
	Vector3 top_corner_world = p_cam_transform.xform(top_corner_view);

	Vector3 left_corner_proj = Vector3(1.0f, -1.0f, -1.0f);
	Vector3 left_corner_view = inv_camera_matrix.xform(left_corner_proj);
	Vector3 left_corner_world = p_cam_transform.xform(left_corner_view);

	td.pixel_u_interp = left_corner_world - td.pixel_corner;
	td.pixel_v_interp = top_corner_world - td.pixel_corner;

	debug_tex_range = td.z_far;

	WorkerThreadPool::GroupID group_task = WorkerThreadPool::get_singleton()->add_template_group_task(this, &RaycastHZBuffer::_camera_rays_threaded, &td, td.thread_count, -1, true, SNAME("RaycastOcclusionCullUpdateCamera"));
	WorkerThreadPool::get_singleton()->wait_for_group_task_completion(group_task);
}

void RaycastOcclusionCull::RaycastHZBuffer::_camera_rays_threaded(uint32_t p_thread, const CameraRayThreadData *p_data) {
	uint32_t total_tiles = camera_rays_tile_count;
	uint32_t total_threads = p_data->thread_count;
	uint32_t from = p_thread * total_tiles / total_threads;
	uint32_t to = (p_thread + 1 == total_threads) ? total_tiles : ((p_thread + 1) * total_tiles / total_threads);
	_generate_camera_rays(p_data, from, to);
}

void RaycastOcclusionCull::RaycastHZBuffer::_generate_camera_rays(const CameraRayThreadData *p_data, int p_from, int p_to) {
	const Size2i &buffer_size = sizes[0];

	for (int i = p_from; i < p_to; i++) {
		CameraRayTile &tile = camera_rays[i];
		int tile_x = (i % tile_grid_size.x) * TILE_SIZE;
		int tile_y = (i / tile_grid_size.x) * TILE_SIZE;

		for (int j = 0; j < TILE_RAYS; j++) {
			int x = tile_x + j % TILE_SIZE;
			int y = tile_y + j / TILE_SIZE;

			float u = (float(x) + 0.5f) / buffer_size.x;
			float v = (float(y) + 0.5f) / buffer_size.y;
			Vector3 pixel_pos = p_data->pixel_corner + u * p_data->pixel_u_interp + v * p_data->pixel_v_interp;

			tile.ray.tnear[j] = p_data->z_near;

			Vector3 dir;
			if (p_data->camera_orthogonal) {
				dir = -p_data->camera_dir;
				tile.ray.org_x[j] = pixel_pos.x - dir.x * p_data->z_near;
				tile.ray.org_y[j] = pixel_pos.y - dir.y * p_data->z_near;
				tile.ray.org_z[j] = pixel_pos.z - dir.z * p_data->z_near;
			} else {
				dir = (pixel_pos - p_data->camera_pos).normalized();
				tile.ray.org_x[j] = p_data->camera_pos.x;
				tile.ray.org_y[j] = p_data->camera_pos.y;
				tile.ray.org_z[j] = p_data->camera_pos.z;
				tile.ray.tnear[j] /= dir.dot(p_data->camera_dir);
			}

			tile.ray.dir_x[j] = dir.x;
			tile.ray.dir_y[j] = dir.y;
			tile.ray.dir_z[j] = dir.z;

			tile.ray.tfar[j] = p_data->z_far;
			tile.ray.time[j] = 0.0f;

			tile.ray.flags[j] = 0;
			tile.ray.mask[j] = ~0U;
			tile.hit.geomID[j] = RTC_INVALID_GEOMETRY_ID;
		}
	}
}

void RaycastOcclusionCull::RaycastHZBuffer::sort_rays(const Vector3 &p_camera_dir, bool p_orthogonal) {
	ERR_FAIL_COND(is_empty());

	Size2i buffer_size = sizes[0];
	for (int i = 0; i < tile_grid_size.y; i++) {
		for (int j = 0; j < tile_grid_size.x; j++) {
			for (int tile_i = 0; tile_i < TILE_SIZE; tile_i++) {
				for (int tile_j = 0; tile_j < TILE_SIZE; tile_j++) {
					int x = j * TILE_SIZE + tile_j;
					int y = i * TILE_SIZE + tile_i;
					if (x >= buffer_size.x || y >= buffer_size.y) {
						continue;
					}
					int k = tile_i * TILE_SIZE + tile_j;
					int tile_index = i * tile_grid_size.x + j;
					mips[0][y * buffer_size.x + x] = camera_rays[tile_index].ray.tfar[k];
				}
			}
		}
	}
}

RaycastOcclusionCull::RaycastHZBuffer::~RaycastHZBuffer() {
	if (camera_rays_unaligned_buffer) {
		memfree(camera_rays_unaligned_buffer);
	}
}

////////////////////////////////////////////////////////

bool RaycastOcclusionCull::is_occluder(RID p_rid) {
	return occluder_owner.owns(p_rid);
}

RID RaycastOcclusionCull::occluder_allocate() {
	return occluder_owner.allocate_rid();
}

void RaycastOcclusionCull::occluder_initialize(RID p_occluder) {
	Occluder *occluder = memnew(Occluder);
	occluder_owner.initialize_rid(p_occluder, occluder);
}

void RaycastOcclusionCull::occluder_set_mesh(RID p_occluder, const PackedVector3Array &p_vertices, const PackedInt32Array &p_indices) {
	Occluder *occluder = occluder_owner.get_or_null(p_occluder);
	ERR_FAIL_NULL(occluder);

	occluder->vertices = p_vertices;
	occluder->indices = p_indices;

	for (const InstanceID &E : occluder->users) {
		RID scenario_rid = E.scenario;
		RID instance_rid = E.instance;
		ERR_CONTINUE(!scenarios.has(scenario_rid));
		Scenario &scenario = scenarios[scenario_rid];
		ERR_CONTINUE(!scenario.instances.has(instance_rid));

		if (!scenario.dirty_instances.has(instance_rid)) {
			scenario.dirty_instances.insert(instance_rid);
			scenario.dirty_instances_array.push_back(instance_rid);
		}
	}
}

void RaycastOcclusionCull::free_occluder(RID p_occluder) {
	Occluder *occluder = occluder_owner.get_or_null(p_occluder);
	ERR_FAIL_NULL(occluder);
	memdelete(occluder);
	occluder_owner.free(p_occluder);
}

////////////////////////////////////////////////////////

void RaycastOcclusionCull::add_scenario(RID p_scenario) {
	ERR_FAIL_COND(scenarios.has(p_scenario));
	scenarios[p_scenario] = Scenario();
}

void RaycastOcclusionCull::remove_scenario(RID p_scenario) {
	Scenario *scenario = scenarios.getptr(p_scenario);
	ERR_FAIL_NULL(scenario);
	scenario->free();
	scenarios.erase(p_scenario);
}

void RaycastOcclusionCull::scenario_set_instance(RID p_scenario, RID p_instance, RID p_occluder, const Transform3D &p_xform, bool p_enabled) {
	ERR_FAIL_COND(!scenarios.has(p_scenario));
	Scenario &scenario = scenarios[p_scenario];

	if (!scenario.instances.has(p_instance)) {
		scenario.instances[p_instance] = OccluderInstance();
	}

	OccluderInstance &instance = scenario.instances[p_instance];

	bool changed = false;

	if (instance.removed) {
		instance.removed = false;
		scenario.removed_instances.erase(p_instance);
		changed = true; // It was removed and re-added, we might have missed some changes
	}

	if (instance.occluder != p_occluder) {
		Occluder *old_occluder = occluder_owner.get_or_null(instance.occluder);
		if (old_occluder) {
			old_occluder->users.erase(InstanceID(p_scenario, p_instance));
		}

		instance.occluder = p_occluder;

		if (p_occluder.is_valid()) {
			Occluder *occluder = occluder_owner.get_or_null(p_occluder);
			ERR_FAIL_NULL(occluder);
			occluder->users.insert(InstanceID(p_scenario, p_instance));
		}
		changed = true;
	}

	if (instance.xform != p_xform) {
		scenario.instances[p_instance].xform = p_xform;
		changed = true;
	}

	if (instance.enabled != p_enabled) {
		instance.enabled = p_enabled;
		scenario.dirty = true; // The scenario needs a scene re-build, but the instance doesn't need update
	}

	if (changed && !scenario.dirty_instances.has(p_instance)) {
		scenario.dirty_instances.insert(p_instance);
		scenario.dirty_instances_array.push_back(p_instance);
		scenario.dirty = true;
	}
}

void RaycastOcclusionCull::scenario_remove_instance(RID p_scenario, RID p_instance) {
	ERR_FAIL_COND(!scenarios.has(p_scenario));
	Scenario &scenario = scenarios[p_scenario];

	if (scenario.instances.has(p_instance)) {
		OccluderInstance &instance = scenario.instances[p_instance];

		if (!instance.removed) {
			Occluder *occluder = occluder_owner.get_or_null(instance.occluder);
			if (occluder) {
				occluder->users.erase(InstanceID(p_scenario, p_instance));
			}

			scenario.removed_instances.push_back(p_instance);
			instance.removed = true;
		}
	}
}

void RaycastOcclusionCull::Scenario::_update_dirty_instance_thread(int p_idx, RID *p_instances) {
	_update_dirty_instance(p_idx, p_instances);
}

void RaycastOcclusionCull::Scenario::_update_dirty_instance(int p_idx, RID *p_instances) {
	OccluderInstance *occ_inst = instances.getptr(p_instances[p_idx]);

	if (!occ_inst) {
		return;
	}

	Occluder *occ = raycast_singleton->occluder_owner.get_or_null(occ_inst->occluder);

	if (!occ) {
		return;
	}

	int vertices_size = occ->vertices.size();

	// Embree requires the last element to be readable by a 16-byte SSE load instruction, so we add padding to be safe.
	occ_inst->xformed_vertices.resize(vertices_size + 1);

	const Vector3 *read_ptr = occ->vertices.ptr();
	Vector3 *write_ptr = occ_inst->xformed_vertices.ptr();

	if (vertices_size > 1024) {
		TransformThreadData td;
		td.xform = occ_inst->xform;
		td.read = read_ptr;
		td.write = write_ptr;
		td.vertex_count = vertices_size;
		td.thread_count = WorkerThreadPool::get_singleton()->get_thread_count();
		WorkerThreadPool::GroupID group_task = WorkerThreadPool::get_singleton()->add_template_group_task(this, &Scenario::_transform_vertices_thread, &td, td.thread_count, -1, true, SNAME("RaycastOcclusionCull"));
		WorkerThreadPool::get_singleton()->wait_for_group_task_completion(group_task);

	} else {
		_transform_vertices_range(read_ptr, write_ptr, occ_inst->xform, 0, vertices_size);
	}

	occ_inst->indices.resize(occ->indices.size());
	memcpy(occ_inst->indices.ptr(), occ->indices.ptr(), occ->indices.size() * sizeof(int32_t));
}

void RaycastOcclusionCull::Scenario::_transform_vertices_thread(uint32_t p_thread, TransformThreadData *p_data) {
	uint32_t vertex_total = p_data->vertex_count;
	uint32_t total_threads = p_data->thread_count;
	uint32_t from = p_thread * vertex_total / total_threads;
	uint32_t to = (p_thread + 1 == total_threads) ? vertex_total : ((p_thread + 1) * vertex_total / total_threads);
	_transform_vertices_range(p_data->read, p_data->write, p_data->xform, from, to);
}

void RaycastOcclusionCull::Scenario::_transform_vertices_range(const Vector3 *p_read, Vector3 *p_write, const Transform3D &p_xform, int p_from, int p_to) {
	for (int i = p_from; i < p_to; i++) {
		p_write[i] = p_xform.xform(p_read[i]);
	}
}

void RaycastOcclusionCull::Scenario::free() {
	if (commit_thread) {
		if (commit_thread->is_started()) {
			commit_thread->wait_to_finish();
		}
		memdelete(commit_thread);
		commit_thread = nullptr;
	}

	for (int i = 0; i < 2; i++) {
		if (ebr_scene[i]) {
			rtcReleaseScene(ebr_scene[i]);
			ebr_scene[i] = nullptr;
		}
	}
}

void RaycastOcclusionCull::Scenario::_commit_scene(void *p_ud) {
	Scenario *scenario = (Scenario *)p_ud;
	int commit_idx = 1 - (scenario->current_scene_idx);
	rtcCommitScene(scenario->ebr_scene[commit_idx]);
	scenario->commit_done = true;
}

void RaycastOcclusionCull::Scenario::update() {
	ERR_FAIL_NULL(singleton);

	if (commit_thread == nullptr) {
		commit_thread = memnew(Thread);
	}

	if (commit_thread->is_started()) {
		if (commit_done) {
			commit_thread->wait_to_finish();
			current_scene_idx = 1 - current_scene_idx;
		} else {
			return;
		}
	}

	if (!dirty && removed_instances.is_empty() && dirty_instances_array.is_empty()) {
		return;
	}

	for (const RID &scenario : removed_instances) {
		instances.erase(scenario);
	}

	if (dirty_instances_array.size() / WorkerThreadPool::get_singleton()->get_thread_count() > 128) {
		// Lots of instances, use per-instance threading
		WorkerThreadPool::GroupID group_task = WorkerThreadPool::get_singleton()->add_template_group_task(this, &Scenario::_update_dirty_instance_thread, dirty_instances_array.ptr(), dirty_instances_array.size(), -1, true, SNAME("RaycastOcclusionCullUpdate"));
		WorkerThreadPool::get_singleton()->wait_for_group_task_completion(group_task);

	} else {
		// Few instances, use threading on the vertex transforms
		for (unsigned int i = 0; i < dirty_instances_array.size(); i++) {
			_update_dirty_instance(i, dirty_instances_array.ptr());
		}
	}

	dirty_instances.clear();
	dirty_instances_array.clear();
	removed_instances.clear();

	if (raycast_singleton->ebr_device == nullptr) {
		raycast_singleton->_init_embree();
	}

	int next_scene_idx = 1 - current_scene_idx;
	RTCScene &next_scene = ebr_scene[next_scene_idx];

	if (next_scene) {
		rtcReleaseScene(next_scene);
	}

	next_scene = rtcNewScene(raycast_singleton->ebr_device);
	rtcSetSceneBuildQuality(next_scene, RTCBuildQuality(raycast_singleton->build_quality));

	for (const KeyValue<RID, OccluderInstance> &E : instances) {
		const OccluderInstance *occ_inst = &E.value;
		const Occluder *occ = raycast_singleton->occluder_owner.get_or_null(occ_inst->occluder);

		if (!occ || !occ_inst->enabled) {
			continue;
		}

		RTCGeometry geom = rtcNewGeometry(raycast_singleton->ebr_device, RTC_GEOMETRY_TYPE_TRIANGLE);
		rtcSetSharedGeometryBuffer(geom, RTC_BUFFER_TYPE_VERTEX, 0, RTC_FORMAT_FLOAT3, occ_inst->xformed_vertices.ptr(), 0, sizeof(Vector3), occ_inst->xformed_vertices.size());
		rtcSetSharedGeometryBuffer(geom, RTC_BUFFER_TYPE_INDEX, 0, RTC_FORMAT_UINT3, occ_inst->indices.ptr(), 0, sizeof(uint32_t) * 3, occ_inst->indices.size() / 3);
		rtcCommitGeometry(geom);
		rtcAttachGeometry(next_scene, geom);
		rtcReleaseGeometry(geom);
	}

	dirty = false;
	commit_done = false;
	commit_thread->start(&Scenario::_commit_scene, this);
}

void RaycastOcclusionCull::Scenario::_raycast(uint32_t p_idx, const RaycastThreadData *p_raycast_data) const {
	RTCRayQueryContext context;
	rtcInitRayQueryContext(&context);
	RTCIntersectArguments args;
	rtcInitIntersectArguments(&args);
	args.flags = RTC_RAY_QUERY_FLAG_COHERENT;
	args.context = &context;
	rtcIntersect16((const int *)&p_raycast_data->masks[p_idx * TILE_RAYS], ebr_scene[current_scene_idx], &p_raycast_data->rays[p_idx], &args);
}

void RaycastOcclusionCull::Scenario::raycast(CameraRayTile *r_rays, const uint32_t *p_valid_masks, uint32_t p_tile_count) const {
	ERR_FAIL_NULL(singleton);
	if (raycast_singleton->ebr_device == nullptr) {
		return; // Embree is initialized on demand when there is some scenario with occluders in it.
	}

	if (ebr_scene[current_scene_idx] == nullptr) {
		return;
	}

	RaycastThreadData td;
	td.rays = r_rays;
	td.masks = p_valid_masks;

	WorkerThreadPool::GroupID group_task = WorkerThreadPool::get_singleton()->add_template_group_task(this, &Scenario::_raycast, &td, p_tile_count, -1, true, SNAME("RaycastOcclusionCullRaycast"));
	WorkerThreadPool::get_singleton()->wait_for_group_task_completion(group_task);
}

////////////////////////////////////////////////////////

void RaycastOcclusionCull::add_buffer(RID p_buffer) {
	ERR_FAIL_COND(buffers.has(p_buffer));
	buffers[p_buffer] = RaycastHZBuffer();
}

void RaycastOcclusionCull::remove_buffer(RID p_buffer) {
	ERR_FAIL_COND(!buffers.has(p_buffer));
	buffers.erase(p_buffer);
}

void RaycastOcclusionCull::buffer_set_scenario(RID p_buffer, RID p_scenario) {
	ERR_FAIL_COND(!buffers.has(p_buffer));
	ERR_FAIL_COND(p_scenario.is_valid() && !scenarios.has(p_scenario));
	buffers[p_buffer].scenario_rid = p_scenario;
}

void RaycastOcclusionCull::buffer_set_size(RID p_buffer, const Vector2i &p_size) {
	ERR_FAIL_COND(!buffers.has(p_buffer));
	buffers[p_buffer].resize(p_size);
}

Projection RaycastOcclusionCull::_jitter_projection(const Projection &p_cam_projection, const Size2i &p_viewport_size) {
	if (!_jitter_enabled) {
		return p_cam_projection;
	}

	// Prevent divide by zero when using NULL viewport.
	if ((p_viewport_size.x <= 0) || (p_viewport_size.y <= 0)) {
		return p_cam_projection;
	}

	Projection p = p_cam_projection;

	int32_t frame = Engine::get_singleton()->get_frames_drawn();
	frame %= 9;

	Vector2 jitter;

	switch (frame) {
		default:
			break;
		case 1: {
			jitter = Vector2(-1, -1);
		} break;
		case 2: {
			jitter = Vector2(1, -1);
		} break;
		case 3: {
			jitter = Vector2(-1, 1);
		} break;
		case 4: {
			jitter = Vector2(1, 1);
		} break;
		case 5: {
			jitter = Vector2(-0.5f, -0.5f);
		} break;
		case 6: {
			jitter = Vector2(0.5f, -0.5f);
		} break;
		case 7: {
			jitter = Vector2(-0.5f, 0.5f);
		} break;
		case 8: {
			jitter = Vector2(0.5f, 0.5f);
		} break;
	}

	// The multiplier here determines the divergence from center,
	// and is to some extent a balancing act.
	// Higher divergence gives fewer false hidden, but more false shown.
	// False hidden is obvious to viewer, false shown is not.
	// False shown can lower percentage that are occluded, and therefore performance.
	jitter *= Vector2(1 / (float)p_viewport_size.x, 1 / (float)p_viewport_size.y) * 0.05f;

	p.add_jitter_offset(jitter);

	return p;
}

void RaycastOcclusionCull::buffer_update(RID p_buffer, const Transform3D &p_cam_transform, const Projection &p_cam_projection, bool p_cam_orthogonal) {
	if (!buffers.has(p_buffer)) {
		return;
	}

	RaycastHZBuffer &buffer = buffers[p_buffer];

	if (buffer.is_empty() || !scenarios.has(buffer.scenario_rid)) {
		return;
	}

	Scenario &scenario = scenarios[buffer.scenario_rid];
	scenario.update();

	Projection jittered_proj = _jitter_projection(p_cam_projection, buffer.get_occlusion_buffer_size());

	buffer.update_camera_rays(p_cam_transform, jittered_proj, p_cam_orthogonal);

	scenario.raycast(buffer.camera_rays, buffer.camera_ray_masks.ptr(), buffer.camera_rays_tile_count);
	buffer.sort_rays(-p_cam_transform.basis.get_column(2), p_cam_orthogonal);
	buffer.update_mips();
}

RaycastOcclusionCull::HZBuffer *RaycastOcclusionCull::buffer_get_ptr(RID p_buffer) {
	if (!buffers.has(p_buffer)) {
		return nullptr;
	}
	return &buffers[p_buffer];
}

RID RaycastOcclusionCull::buffer_get_debug_texture(RID p_buffer) {
	ERR_FAIL_COND_V(!buffers.has(p_buffer), RID());
	return buffers[p_buffer].get_debug_texture();
}

////////////////////////////////////////////////////////

void RaycastOcclusionCull::set_build_quality(RS::ViewportOcclusionCullingBuildQuality p_quality) {
	if (build_quality == p_quality) {
		return;
	}

	build_quality = p_quality;

	for (KeyValue<RID, Scenario> &K : scenarios) {
		K.value.dirty = true;
	}
}

void RaycastOcclusionCull::_init_embree() {
#ifdef __SSE2__
	_MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_ON);
	_MM_SET_DENORMALS_ZERO_MODE(_MM_DENORMALS_ZERO_ON);
#endif

	String settings = vformat("threads=%d", MAX(1, OS::get_singleton()->get_processor_count() - 2));
	ebr_device = rtcNewDevice(settings.utf8().ptr());
}

RaycastOcclusionCull::RaycastOcclusionCull() {
	raycast_singleton = this;
	int default_quality = GLOBAL_GET("rendering/occlusion_culling/bvh_build_quality");
	_jitter_enabled = GLOBAL_GET("rendering/occlusion_culling/jitter_projection");
	build_quality = RS::ViewportOcclusionCullingBuildQuality(default_quality);
}

RaycastOcclusionCull::~RaycastOcclusionCull() {
	for (KeyValue<RID, Scenario> &K : scenarios) {
		K.value.free();
	}

	if (ebr_device != nullptr) {
		rtcReleaseDevice(ebr_device);
	}

	raycast_singleton = nullptr;
}

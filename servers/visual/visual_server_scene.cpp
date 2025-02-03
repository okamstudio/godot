/**************************************************************************/
/*  visual_server_scene.cpp                                               */
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

#include "visual_server_scene.h"

#include "core/math/transform_interpolator.h"
#include "core/os/os.h"
#include "visual_server_globals.h"
#include "visual_server_light_culler.h"
#include "visual_server_raster.h"

#include <new>

/* CAMERA API */

RID VisualServerScene::camera_create() {
	Camera *camera = memnew(Camera);
	return camera_owner.make_rid(camera);
}

void VisualServerScene::camera_set_perspective(RID p_camera, float p_fovy_degrees, float p_z_near, float p_z_far) {
	Camera *camera = camera_owner.get(p_camera);
	ERR_FAIL_COND(!camera);
	camera->type = Camera::PERSPECTIVE;
	camera->fov = p_fovy_degrees;
	camera->znear = p_z_near;
	camera->zfar = p_z_far;
}

void VisualServerScene::camera_set_orthogonal(RID p_camera, float p_size, float p_z_near, float p_z_far) {
	Camera *camera = camera_owner.get(p_camera);
	ERR_FAIL_COND(!camera);
	camera->type = Camera::ORTHOGONAL;
	camera->size = p_size;
	camera->znear = p_z_near;
	camera->zfar = p_z_far;
}

void VisualServerScene::camera_set_frustum(RID p_camera, float p_size, Vector2 p_offset, float p_z_near, float p_z_far) {
	Camera *camera = camera_owner.get(p_camera);
	ERR_FAIL_COND(!camera);
	camera->type = Camera::FRUSTUM;
	camera->size = p_size;
	camera->offset = p_offset;
	camera->znear = p_z_near;
	camera->zfar = p_z_far;
}

void VisualServerScene::camera_set_transform(RID p_camera, const Transform &p_transform) {
	Camera *camera = camera_owner.get(p_camera);
	ERR_FAIL_COND(!camera);

	camera->transform = p_transform.orthonormalized();
}

void VisualServerScene::camera_set_cull_mask(RID p_camera, uint32_t p_layers) {
	Camera *camera = camera_owner.get(p_camera);
	ERR_FAIL_COND(!camera);

	camera->visible_layers = p_layers;
}

void VisualServerScene::camera_set_environment(RID p_camera, RID p_env) {
	Camera *camera = camera_owner.get(p_camera);
	ERR_FAIL_COND(!camera);
	camera->env = p_env;
}

void VisualServerScene::camera_set_use_vertical_aspect(RID p_camera, bool p_enable) {
	Camera *camera = camera_owner.get(p_camera);
	ERR_FAIL_COND(!camera);
	camera->vaspect = p_enable;
}

/* SPATIAL PARTITIONING */

VisualServerScene::SpatialPartitioningScene_BVH::SpatialPartitioningScene_BVH() {
	_bvh.params_set_thread_safe(GLOBAL_GET("rendering/threads/thread_safe_bvh"));
	_bvh.params_set_pairing_expansion(GLOBAL_GET("rendering/quality/spatial_partitioning/bvh_collision_margin"));

	_dummy_cull_object = memnew(Instance);
}

VisualServerScene::SpatialPartitioningScene_BVH::~SpatialPartitioningScene_BVH() {
	if (_dummy_cull_object) {
		memdelete(_dummy_cull_object);
		_dummy_cull_object = nullptr;
	}
}

VisualServerScene::SpatialPartitionID VisualServerScene::SpatialPartitioningScene_BVH::create(Instance *p_userdata, const AABB &p_aabb, int p_subindex, bool p_pairable, uint32_t p_pairable_type, uint32_t p_pairable_mask) {
#if defined(DEBUG_ENABLED) && defined(TOOLS_ENABLED)
	// we are relying on this instance to be valid in order to pass
	// the visible flag to the bvh.
	DEV_ASSERT(p_userdata);
#endif

	// cache the pairable mask and pairable type on the instance as it is needed for user callbacks from the BVH, and this is
	// too complex to calculate each callback...
	p_userdata->bvh_pairable_mask = p_pairable_mask;
	p_userdata->bvh_pairable_type = p_pairable_type;

	uint32_t tree_collision_mask = 0;
	uint32_t tree_id = find_tree_id_and_collision_mask(p_pairable, tree_collision_mask);

	return _bvh.create(p_userdata, p_userdata->visible, tree_id, tree_collision_mask, p_aabb, p_subindex) + 1;
}

void VisualServerScene::SpatialPartitioningScene_BVH::erase(SpatialPartitionID p_handle) {
	_bvh.erase(p_handle - 1);
}

void VisualServerScene::SpatialPartitioningScene_BVH::move(SpatialPartitionID p_handle, const AABB &p_aabb) {
	_bvh.move(p_handle - 1, p_aabb);
}

void VisualServerScene::SpatialPartitioningScene_BVH::activate(SpatialPartitionID p_handle, const AABB &p_aabb) {
	// be very careful here, we are deferring the collision check, expecting a set_pairable to be called
	// immediately after.
	// see the notes in the BVH function.
	_bvh.activate(p_handle - 1, p_aabb, true);
}

void VisualServerScene::SpatialPartitioningScene_BVH::deactivate(SpatialPartitionID p_handle) {
	_bvh.deactivate(p_handle - 1);
}

void VisualServerScene::SpatialPartitioningScene_BVH::force_collision_check(SpatialPartitionID p_handle) {
	_bvh.force_collision_check(p_handle - 1);
}

void VisualServerScene::SpatialPartitioningScene_BVH::update() {
	_bvh.update();
}

void VisualServerScene::SpatialPartitioningScene_BVH::update_collisions() {
	_bvh.update_collisions();
}

void VisualServerScene::SpatialPartitioningScene_BVH::set_pairable(Instance *p_instance, bool p_pairable, uint32_t p_pairable_type, uint32_t p_pairable_mask) {
	SpatialPartitionID handle = p_instance->spatial_partition_id;

	p_instance->bvh_pairable_mask = p_pairable_mask;
	p_instance->bvh_pairable_type = p_pairable_type;

	uint32_t tree_collision_mask = 0;
	uint32_t tree_id = find_tree_id_and_collision_mask(p_pairable, tree_collision_mask);

	_bvh.set_tree(handle - 1, tree_id, tree_collision_mask);
}

int VisualServerScene::SpatialPartitioningScene_BVH::cull_convex(const Vector<Plane> &p_convex, Instance **p_result_array, int p_result_max, uint32_t p_mask) {
	_dummy_cull_object->bvh_pairable_mask = p_mask;
	_dummy_cull_object->bvh_pairable_type = 0;
	return _bvh.cull_convex(p_convex, p_result_array, p_result_max, _dummy_cull_object);
}

int VisualServerScene::SpatialPartitioningScene_BVH::cull_aabb(const AABB &p_aabb, Instance **p_result_array, int p_result_max, int *p_subindex_array, uint32_t p_mask) {
	_dummy_cull_object->bvh_pairable_mask = p_mask;
	_dummy_cull_object->bvh_pairable_type = 0;
	return _bvh.cull_aabb(p_aabb, p_result_array, p_result_max, _dummy_cull_object, 0xFFFFFFFF, p_subindex_array);
}

int VisualServerScene::SpatialPartitioningScene_BVH::cull_segment(const Vector3 &p_from, const Vector3 &p_to, Instance **p_result_array, int p_result_max, int *p_subindex_array, uint32_t p_mask) {
	_dummy_cull_object->bvh_pairable_mask = p_mask;
	_dummy_cull_object->bvh_pairable_type = 0;
	return _bvh.cull_segment(p_from, p_to, p_result_array, p_result_max, _dummy_cull_object, 0xFFFFFFFF, p_subindex_array);
}

void VisualServerScene::SpatialPartitioningScene_BVH::set_pair_callback(PairCallback p_callback, void *p_userdata) {
	_bvh.set_pair_callback(p_callback, p_userdata);
}

void VisualServerScene::SpatialPartitioningScene_BVH::set_unpair_callback(UnpairCallback p_callback, void *p_userdata) {
	_bvh.set_unpair_callback(p_callback, p_userdata);
}

///////////////////////

VisualServerScene::SpatialPartitionID VisualServerScene::SpatialPartitioningScene_Octree::create(Instance *p_userdata, const AABB &p_aabb, int p_subindex, bool p_pairable, uint32_t p_pairable_type, uint32_t p_pairable_mask) {
	return _octree.create(p_userdata, p_aabb, p_subindex, p_pairable, p_pairable_type, p_pairable_mask);
}

void VisualServerScene::SpatialPartitioningScene_Octree::erase(SpatialPartitionID p_handle) {
	_octree.erase(p_handle);
}

void VisualServerScene::SpatialPartitioningScene_Octree::move(SpatialPartitionID p_handle, const AABB &p_aabb) {
	_octree.move(p_handle, p_aabb);
}

void VisualServerScene::SpatialPartitioningScene_Octree::set_pairable(Instance *p_instance, bool p_pairable, uint32_t p_pairable_type, uint32_t p_pairable_mask) {
	SpatialPartitionID handle = p_instance->spatial_partition_id;
	_octree.set_pairable(handle, p_pairable, p_pairable_type, p_pairable_mask);
}

int VisualServerScene::SpatialPartitioningScene_Octree::cull_convex(const Vector<Plane> &p_convex, Instance **p_result_array, int p_result_max, uint32_t p_mask) {
	return _octree.cull_convex(p_convex, p_result_array, p_result_max, p_mask);
}

int VisualServerScene::SpatialPartitioningScene_Octree::cull_aabb(const AABB &p_aabb, Instance **p_result_array, int p_result_max, int *p_subindex_array, uint32_t p_mask) {
	return _octree.cull_aabb(p_aabb, p_result_array, p_result_max, p_subindex_array, p_mask);
}

int VisualServerScene::SpatialPartitioningScene_Octree::cull_segment(const Vector3 &p_from, const Vector3 &p_to, Instance **p_result_array, int p_result_max, int *p_subindex_array, uint32_t p_mask) {
	return _octree.cull_segment(p_from, p_to, p_result_array, p_result_max, p_subindex_array, p_mask);
}

void VisualServerScene::SpatialPartitioningScene_Octree::set_pair_callback(PairCallback p_callback, void *p_userdata) {
	_octree.set_pair_callback(p_callback, p_userdata);
}

void VisualServerScene::SpatialPartitioningScene_Octree::set_unpair_callback(UnpairCallback p_callback, void *p_userdata) {
	_octree.set_unpair_callback(p_callback, p_userdata);
}

void VisualServerScene::SpatialPartitioningScene_Octree::set_balance(float p_balance) {
	_octree.set_balance(p_balance);
}

/* SCENARIO API */

VisualServerScene::Scenario::Scenario() {
	debug = VS::SCENARIO_DEBUG_DISABLED;

	bool use_bvh_or_octree = GLOBAL_GET("rendering/quality/spatial_partitioning/use_bvh");

	if (use_bvh_or_octree) {
		sps = memnew(SpatialPartitioningScene_BVH);
	} else {
		sps = memnew(SpatialPartitioningScene_Octree);
	}
}

void *VisualServerScene::_instance_pair(void *p_self, SpatialPartitionID, Instance *p_A, int, SpatialPartitionID, Instance *p_B, int) {
	//VisualServerScene *self = (VisualServerScene*)p_self;
	Instance *A = p_A;
	Instance *B = p_B;

	//instance indices are designed so greater always contains lesser
	if (A->base_type > B->base_type) {
		SWAP(A, B); //lesser always first
	}

	if (B->base_type == VS::INSTANCE_LIGHT && ((1 << A->base_type) & VS::INSTANCE_GEOMETRY_MASK)) {
		InstanceLightData *light = static_cast<InstanceLightData *>(B->base_data);
		InstanceGeometryData *geom = static_cast<InstanceGeometryData *>(A->base_data);

		InstanceLightData::PairInfo pinfo;
		pinfo.geometry = A;
		pinfo.L = geom->lighting.push_back(B);

		List<InstanceLightData::PairInfo>::Element *E = light->geometries.push_back(pinfo);

		if (geom->can_cast_shadows) {
			light->make_shadow_dirty();
		}
		geom->lighting_dirty = true;

		return E; //this element should make freeing faster
	} else if (B->base_type == VS::INSTANCE_REFLECTION_PROBE && ((1 << A->base_type) & VS::INSTANCE_GEOMETRY_MASK)) {
		InstanceReflectionProbeData *reflection_probe = static_cast<InstanceReflectionProbeData *>(B->base_data);
		InstanceGeometryData *geom = static_cast<InstanceGeometryData *>(A->base_data);

		InstanceReflectionProbeData::PairInfo pinfo;
		pinfo.geometry = A;
		pinfo.L = geom->reflection_probes.push_back(B);

		List<InstanceReflectionProbeData::PairInfo>::Element *E = reflection_probe->geometries.push_back(pinfo);

		geom->reflection_dirty = true;

		return E; //this element should make freeing faster
	} else if (B->base_type == VS::INSTANCE_LIGHTMAP_CAPTURE && ((1 << A->base_type) & VS::INSTANCE_GEOMETRY_MASK)) {
		InstanceLightmapCaptureData *lightmap_capture = static_cast<InstanceLightmapCaptureData *>(B->base_data);
		InstanceGeometryData *geom = static_cast<InstanceGeometryData *>(A->base_data);

		InstanceLightmapCaptureData::PairInfo pinfo;
		pinfo.geometry = A;
		pinfo.L = geom->lightmap_captures.push_back(B);

		List<InstanceLightmapCaptureData::PairInfo>::Element *E = lightmap_capture->geometries.push_back(pinfo);
		((VisualServerScene *)p_self)->_instance_queue_update(A, false, false); //need to update capture

		return E; //this element should make freeing faster
	} else if (B->base_type == VS::INSTANCE_GI_PROBE && ((1 << A->base_type) & VS::INSTANCE_GEOMETRY_MASK)) {
		InstanceGIProbeData *gi_probe = static_cast<InstanceGIProbeData *>(B->base_data);
		InstanceGeometryData *geom = static_cast<InstanceGeometryData *>(A->base_data);

		InstanceGIProbeData::PairInfo pinfo;
		pinfo.geometry = A;
		pinfo.L = geom->gi_probes.push_back(B);

		List<InstanceGIProbeData::PairInfo>::Element *E = gi_probe->geometries.push_back(pinfo);

		geom->gi_probes_dirty = true;

		return E; //this element should make freeing faster

	} else if (B->base_type == VS::INSTANCE_GI_PROBE && A->base_type == VS::INSTANCE_LIGHT) {
		InstanceGIProbeData *gi_probe = static_cast<InstanceGIProbeData *>(B->base_data);
		return gi_probe->lights.insert(A);
	}

	return nullptr;
}

void VisualServerScene::_instance_unpair(void *p_self, SpatialPartitionID, Instance *p_A, int, SpatialPartitionID, Instance *p_B, int, void *udata) {
	//VisualServerScene *self = (VisualServerScene*)p_self;
	Instance *A = p_A;
	Instance *B = p_B;

	//instance indices are designed so greater always contains lesser
	if (A->base_type > B->base_type) {
		SWAP(A, B); //lesser always first
	}

	if (B->base_type == VS::INSTANCE_LIGHT && ((1 << A->base_type) & VS::INSTANCE_GEOMETRY_MASK)) {
		InstanceLightData *light = static_cast<InstanceLightData *>(B->base_data);
		InstanceGeometryData *geom = static_cast<InstanceGeometryData *>(A->base_data);

		List<InstanceLightData::PairInfo>::Element *E = reinterpret_cast<List<InstanceLightData::PairInfo>::Element *>(udata);

		geom->lighting.erase(E->get().L);
		light->geometries.erase(E);

		if (geom->can_cast_shadows) {
			light->make_shadow_dirty();
		}
		geom->lighting_dirty = true;

	} else if (B->base_type == VS::INSTANCE_REFLECTION_PROBE && ((1 << A->base_type) & VS::INSTANCE_GEOMETRY_MASK)) {
		InstanceReflectionProbeData *reflection_probe = static_cast<InstanceReflectionProbeData *>(B->base_data);
		InstanceGeometryData *geom = static_cast<InstanceGeometryData *>(A->base_data);

		List<InstanceReflectionProbeData::PairInfo>::Element *E = reinterpret_cast<List<InstanceReflectionProbeData::PairInfo>::Element *>(udata);

		geom->reflection_probes.erase(E->get().L);
		reflection_probe->geometries.erase(E);

		geom->reflection_dirty = true;
	} else if (B->base_type == VS::INSTANCE_LIGHTMAP_CAPTURE && ((1 << A->base_type) & VS::INSTANCE_GEOMETRY_MASK)) {
		InstanceLightmapCaptureData *lightmap_capture = static_cast<InstanceLightmapCaptureData *>(B->base_data);
		InstanceGeometryData *geom = static_cast<InstanceGeometryData *>(A->base_data);

		List<InstanceLightmapCaptureData::PairInfo>::Element *E = reinterpret_cast<List<InstanceLightmapCaptureData::PairInfo>::Element *>(udata);

		geom->lightmap_captures.erase(E->get().L);
		lightmap_capture->geometries.erase(E);
		((VisualServerScene *)p_self)->_instance_queue_update(A, false, false); //need to update capture

	} else if (B->base_type == VS::INSTANCE_GI_PROBE && ((1 << A->base_type) & VS::INSTANCE_GEOMETRY_MASK)) {
		InstanceGIProbeData *gi_probe = static_cast<InstanceGIProbeData *>(B->base_data);
		InstanceGeometryData *geom = static_cast<InstanceGeometryData *>(A->base_data);

		List<InstanceGIProbeData::PairInfo>::Element *E = reinterpret_cast<List<InstanceGIProbeData::PairInfo>::Element *>(udata);

		geom->gi_probes.erase(E->get().L);
		gi_probe->geometries.erase(E);

		geom->gi_probes_dirty = true;

	} else if (B->base_type == VS::INSTANCE_GI_PROBE && A->base_type == VS::INSTANCE_LIGHT) {
		InstanceGIProbeData *gi_probe = static_cast<InstanceGIProbeData *>(B->base_data);
		Set<Instance *>::Element *E = reinterpret_cast<Set<Instance *>::Element *>(udata);

		gi_probe->lights.erase(E);
	}
}

RID VisualServerScene::scenario_create() {
	Scenario *scenario = memnew(Scenario);
	ERR_FAIL_COND_V(!scenario, RID());
	RID scenario_rid = scenario_owner.make_rid(scenario);
	scenario->self = scenario_rid;

	scenario->sps->set_balance(GLOBAL_GET("rendering/quality/spatial_partitioning/render_tree_balance"));
	scenario->sps->set_pair_callback(_instance_pair, this);
	scenario->sps->set_unpair_callback(_instance_unpair, this);

	scenario->reflection_probe_shadow_atlas = VSG::scene_render->shadow_atlas_create();
	VSG::scene_render->shadow_atlas_set_size(scenario->reflection_probe_shadow_atlas, 1024); //make enough shadows for close distance, don't bother with rest
	VSG::scene_render->shadow_atlas_set_quadrant_subdivision(scenario->reflection_probe_shadow_atlas, 0, 4);
	VSG::scene_render->shadow_atlas_set_quadrant_subdivision(scenario->reflection_probe_shadow_atlas, 1, 4);
	VSG::scene_render->shadow_atlas_set_quadrant_subdivision(scenario->reflection_probe_shadow_atlas, 2, 4);
	VSG::scene_render->shadow_atlas_set_quadrant_subdivision(scenario->reflection_probe_shadow_atlas, 3, 8);
	scenario->reflection_atlas = VSG::scene_render->reflection_atlas_create();

	return scenario_rid;
}

void VisualServerScene::set_physics_interpolation_enabled(bool p_enabled) {
	_interpolation_data.interpolation_enabled = p_enabled;
}

void VisualServerScene::tick() {
	if (_interpolation_data.interpolation_enabled) {
		update_interpolation_tick(true);
	}
}

void VisualServerScene::pre_draw(bool p_will_draw) {
	// even when running and not drawing scenes, we still need to clear intermediate per frame
	// interpolation data .. hence the p_will_draw flag (so we can reduce the processing if the frame
	// will not be drawn)
	if (_interpolation_data.interpolation_enabled) {
		update_interpolation_frame(p_will_draw);
	}

	// Opportunity to cheaply get any project settings that have changed.
	if (ProjectSettings::get_singleton()->has_changes()) {
		light_culler->set_caster_culling_active(GLOBAL_GET("rendering/quality/shadows/caster_culling"));
		light_culler->set_light_culling_active(GLOBAL_GET("rendering/quality/shadows/light_culling"));
	}
}

void VisualServerScene::scenario_set_debug(RID p_scenario, VS::ScenarioDebugMode p_debug_mode) {
	Scenario *scenario = scenario_owner.get(p_scenario);
	ERR_FAIL_COND(!scenario);
	scenario->debug = p_debug_mode;
}

void VisualServerScene::scenario_set_environment(RID p_scenario, RID p_environment) {
	Scenario *scenario = scenario_owner.get(p_scenario);
	ERR_FAIL_COND(!scenario);
	scenario->environment = p_environment;
}

void VisualServerScene::scenario_set_fallback_environment(RID p_scenario, RID p_environment) {
	Scenario *scenario = scenario_owner.get(p_scenario);
	ERR_FAIL_COND(!scenario);
	scenario->fallback_environment = p_environment;
}

void VisualServerScene::scenario_set_reflection_atlas_size(RID p_scenario, int p_size, int p_subdiv) {
	Scenario *scenario = scenario_owner.get(p_scenario);
	ERR_FAIL_COND(!scenario);
	VSG::scene_render->reflection_atlas_set_size(scenario->reflection_atlas, p_size);
	VSG::scene_render->reflection_atlas_set_subdivision(scenario->reflection_atlas, p_subdiv);
}

/* INSTANCING API */

void VisualServerScene::_instance_queue_update(Instance *p_instance, bool p_update_aabb, bool p_update_materials) {
	if (p_update_aabb) {
		p_instance->update_aabb = true;
	}
	if (p_update_materials) {
		p_instance->update_materials = true;
	}

	if (p_instance->update_item.in_list()) {
		return;
	}

	_instance_update_list.add(&p_instance->update_item);
}

RID VisualServerScene::instance_create() {
	Instance *instance = memnew(Instance);
	ERR_FAIL_COND_V(!instance, RID());

	RID instance_rid = instance_owner.make_rid(instance);
	instance->self = instance_rid;

	return instance_rid;
}

void VisualServerScene::instance_set_base(RID p_instance, RID p_base) {
	Instance *instance = instance_owner.get(p_instance);
	ERR_FAIL_COND(!instance);

	Scenario *scenario = instance->scenario;

	if (instance->base_type != VS::INSTANCE_NONE) {
		//free anything related to that base

		VSG::storage->instance_remove_dependency(instance->base, instance);

		if (instance->base_type == VS::INSTANCE_GI_PROBE) {
			//if gi probe is baking, wait until done baking, else race condition may happen when removing it
			//from octree
			InstanceGIProbeData *gi_probe = static_cast<InstanceGIProbeData *>(instance->base_data);

			//make sure probes are done baking
			while (!probe_bake_list.empty()) {
				OS::get_singleton()->delay_usec(1);
			}
			//make sure this one is done baking

			while (gi_probe->dynamic.updating_stage == GI_UPDATE_STAGE_LIGHTING) {
				//wait until bake is done if it's baking
				OS::get_singleton()->delay_usec(1);
			}
		}

		if (scenario && instance->spatial_partition_id) {
			scenario->sps->erase(instance->spatial_partition_id);
			instance->spatial_partition_id = 0;
		}

		switch (instance->base_type) {
			case VS::INSTANCE_LIGHT: {
				InstanceLightData *light = static_cast<InstanceLightData *>(instance->base_data);

				if (instance->scenario && light->D) {
					instance->scenario->directional_lights.erase(light->D);
					light->D = nullptr;
				}
				VSG::scene_render->free(light->instance);
			} break;
			case VS::INSTANCE_REFLECTION_PROBE: {
				InstanceReflectionProbeData *reflection_probe = static_cast<InstanceReflectionProbeData *>(instance->base_data);
				VSG::scene_render->free(reflection_probe->instance);
				if (reflection_probe->update_list.in_list()) {
					reflection_probe_render_list.remove(&reflection_probe->update_list);
				}
			} break;
			case VS::INSTANCE_LIGHTMAP_CAPTURE: {
				InstanceLightmapCaptureData *lightmap_capture = static_cast<InstanceLightmapCaptureData *>(instance->base_data);
				//erase dependencies, since no longer a lightmap
				while (lightmap_capture->users.front()) {
					instance_set_use_lightmap(lightmap_capture->users.front()->get()->self, RID(), RID(), -1, Rect2(0, 0, 1, 1));
				}
			} break;
			case VS::INSTANCE_GI_PROBE: {
				InstanceGIProbeData *gi_probe = static_cast<InstanceGIProbeData *>(instance->base_data);

				if (gi_probe->update_element.in_list()) {
					gi_probe_update_list.remove(&gi_probe->update_element);
				}
				if (gi_probe->dynamic.probe_data.is_valid()) {
					VSG::storage->free(gi_probe->dynamic.probe_data);
				}

				if (instance->lightmap_capture) {
					Instance *capture = (Instance *)instance->lightmap_capture;
					InstanceLightmapCaptureData *lightmap_capture = static_cast<InstanceLightmapCaptureData *>(capture->base_data);
					lightmap_capture->users.erase(instance);
					instance->lightmap_capture = nullptr;
					instance->lightmap = RID();
				}

				VSG::scene_render->free(gi_probe->probe_instance);

			} break;
			default: {
			}
		}

		if (instance->base_data) {
			memdelete(instance->base_data);
			instance->base_data = nullptr;
		}

		instance->blend_values = PoolRealArray();

		for (int i = 0; i < instance->materials.size(); i++) {
			if (instance->materials[i].is_valid()) {
				VSG::storage->material_remove_instance_owner(instance->materials[i], instance);
			}
		}
		instance->materials.clear();
	}

	instance->base_type = VS::INSTANCE_NONE;
	instance->base = RID();

	if (p_base.is_valid()) {
		instance->base_type = VSG::storage->get_base_type(p_base);
		ERR_FAIL_COND(instance->base_type == VS::INSTANCE_NONE);

		switch (instance->base_type) {
			case VS::INSTANCE_LIGHT: {
				InstanceLightData *light = memnew(InstanceLightData);

				if (scenario && VSG::storage->light_get_type(p_base) == VS::LIGHT_DIRECTIONAL) {
					light->D = scenario->directional_lights.push_back(instance);
				}

				light->instance = VSG::scene_render->light_instance_create(p_base);

				instance->base_data = light;
			} break;
			case VS::INSTANCE_MESH:
			case VS::INSTANCE_MULTIMESH:
			case VS::INSTANCE_IMMEDIATE:
			case VS::INSTANCE_PARTICLES: {
				InstanceGeometryData *geom = memnew(InstanceGeometryData);
				instance->base_data = geom;
				if (instance->base_type == VS::INSTANCE_MESH) {
					instance->blend_values.resize(VSG::storage->mesh_get_blend_shape_count(p_base));
				}
			} break;
			case VS::INSTANCE_REFLECTION_PROBE: {
				InstanceReflectionProbeData *reflection_probe = memnew(InstanceReflectionProbeData);
				reflection_probe->owner = instance;
				instance->base_data = reflection_probe;

				reflection_probe->instance = VSG::scene_render->reflection_probe_instance_create(p_base);
			} break;
			case VS::INSTANCE_LIGHTMAP_CAPTURE: {
				InstanceLightmapCaptureData *lightmap_capture = memnew(InstanceLightmapCaptureData);
				instance->base_data = lightmap_capture;
				//lightmap_capture->instance = VSG::scene_render->lightmap_capture_instance_create(p_base);
			} break;
			case VS::INSTANCE_GI_PROBE: {
				InstanceGIProbeData *gi_probe = memnew(InstanceGIProbeData);
				instance->base_data = gi_probe;
				gi_probe->owner = instance;

				if (scenario && !gi_probe->update_element.in_list()) {
					gi_probe_update_list.add(&gi_probe->update_element);
				}

				gi_probe->probe_instance = VSG::scene_render->gi_probe_instance_create();

			} break;
			default: {
			}
		}

		VSG::storage->instance_add_dependency(p_base, instance);

		instance->base = p_base;

		if (scenario) {
			_instance_queue_update(instance, true, true);
		}
	}
}
void VisualServerScene::instance_set_scenario(RID p_instance, RID p_scenario) {
	Instance *instance = instance_owner.get(p_instance);
	ERR_FAIL_COND(!instance);

	if (instance->scenario) {
		instance->scenario->instances.remove(&instance->scenario_item);

		if (instance->spatial_partition_id) {
			instance->scenario->sps->erase(instance->spatial_partition_id);
			instance->spatial_partition_id = 0;
		}

		// handle occlusion changes
		if (instance->occlusion_handle) {
			_instance_destroy_occlusion_rep(instance);
		}

		// remove any interpolation data associated with the instance in this scenario
		_interpolation_data.notify_free_instance(p_instance, *instance);

		switch (instance->base_type) {
			case VS::INSTANCE_LIGHT: {
				InstanceLightData *light = static_cast<InstanceLightData *>(instance->base_data);

				if (light->D) {
					instance->scenario->directional_lights.erase(light->D);
					light->D = nullptr;
				}
			} break;
			case VS::INSTANCE_REFLECTION_PROBE: {
				InstanceReflectionProbeData *reflection_probe = static_cast<InstanceReflectionProbeData *>(instance->base_data);
				VSG::scene_render->reflection_probe_release_atlas_index(reflection_probe->instance);
			} break;
			case VS::INSTANCE_GI_PROBE: {
				InstanceGIProbeData *gi_probe = static_cast<InstanceGIProbeData *>(instance->base_data);
				if (gi_probe->update_element.in_list()) {
					gi_probe_update_list.remove(&gi_probe->update_element);
				}
			} break;
			default: {
			}
		}

		instance->scenario = nullptr;
	}

	if (p_scenario.is_valid()) {
		Scenario *scenario = scenario_owner.get(p_scenario);
		ERR_FAIL_COND(!scenario);

		instance->scenario = scenario;

		scenario->instances.add(&instance->scenario_item);

		switch (instance->base_type) {
			case VS::INSTANCE_LIGHT: {
				InstanceLightData *light = static_cast<InstanceLightData *>(instance->base_data);

				if (VSG::storage->light_get_type(instance->base) == VS::LIGHT_DIRECTIONAL) {
					light->D = scenario->directional_lights.push_back(instance);
				}
			} break;
			case VS::INSTANCE_GI_PROBE: {
				InstanceGIProbeData *gi_probe = static_cast<InstanceGIProbeData *>(instance->base_data);
				if (!gi_probe->update_element.in_list()) {
					gi_probe_update_list.add(&gi_probe->update_element);
				}
			} break;
			default: {
			}
		}

		// handle occlusion changes if necessary
		_instance_create_occlusion_rep(instance);

		_instance_queue_update(instance, true, true);
	}
}
void VisualServerScene::instance_set_layer_mask(RID p_instance, uint32_t p_mask) {
	Instance *instance = instance_owner.get(p_instance);
	ERR_FAIL_COND(!instance);

	if (instance->layer_mask == p_mask) {
		return;
	}

	instance->layer_mask = p_mask;

	// update lights to show / hide shadows according to the new mask
	if ((1 << instance->base_type) & VS::INSTANCE_GEOMETRY_MASK) {
		InstanceGeometryData *geom = static_cast<InstanceGeometryData *>(instance->base_data);

		if (geom->can_cast_shadows) {
			for (List<Instance *>::Element *E = geom->lighting.front(); E; E = E->next()) {
				InstanceLightData *light = static_cast<InstanceLightData *>(E->get()->base_data);
				light->make_shadow_dirty();
			}
		}
	}
}

void VisualServerScene::instance_set_pivot_data(RID p_instance, float p_sorting_offset, bool p_use_aabb_center) {
	Instance *instance = instance_owner.get(p_instance);
	ERR_FAIL_COND(!instance);

	instance->sorting_offset = p_sorting_offset;
	instance->use_aabb_center = p_use_aabb_center;
}

void VisualServerScene::instance_reset_physics_interpolation(RID p_instance) {
	Instance *instance = instance_owner.get(p_instance);
	ERR_FAIL_COND(!instance);

	if (_interpolation_data.interpolation_enabled && instance->interpolated) {
		instance->transform_prev = instance->transform_curr;
		instance->transform_checksum_prev = instance->transform_checksum_curr;

#ifdef VISUAL_SERVER_DEBUG_PHYSICS_INTERPOLATION
		print_line("instance_reset_physics_interpolation .. tick " + itos(Engine::get_singleton()->get_physics_frames()));
		print_line("\tprev " + rtos(instance->transform_prev.origin.x) + ", curr " + rtos(instance->transform_curr.origin.x));
#endif
	}
}

void VisualServerScene::instance_set_interpolated(RID p_instance, bool p_interpolated) {
	Instance *instance = instance_owner.get(p_instance);
	ERR_FAIL_COND(!instance);
	instance->interpolated = p_interpolated;
}

void VisualServerScene::instance_set_transform(RID p_instance, const Transform &p_transform) {
	Instance *instance = instance_owner.get(p_instance);
	ERR_FAIL_COND(!instance);

#ifdef VISUAL_SERVER_DEBUG_PHYSICS_INTERPOLATION
	print_line("instance_set_transform " + rtos(p_transform.origin.x) + " .. tick " + itos(Engine::get_singleton()->get_physics_frames()));
#endif

	if (!(_interpolation_data.interpolation_enabled && instance->interpolated) || !instance->scenario) {
		if (instance->transform == p_transform) {
			return; //must be checked to avoid worst evil
		}

#ifdef DEBUG_ENABLED

		for (int i = 0; i < 4; i++) {
			const Vector3 &v = i < 3 ? p_transform.basis.elements[i] : p_transform.origin;
			ERR_FAIL_COND(Math::is_inf(v.x));
			ERR_FAIL_COND(Math::is_nan(v.x));
			ERR_FAIL_COND(Math::is_inf(v.y));
			ERR_FAIL_COND(Math::is_nan(v.y));
			ERR_FAIL_COND(Math::is_inf(v.z));
			ERR_FAIL_COND(Math::is_nan(v.z));
		}

#endif
		instance->transform = p_transform;
		_instance_queue_update(instance, true);

#if defined(DEBUG_ENABLED) && defined(TOOLS_ENABLED)
		if ((_interpolation_data.interpolation_enabled && !instance->interpolated) && (Engine::get_singleton()->is_in_physics_frame())) {
			PHYSICS_INTERPOLATION_NODE_WARNING(instance->object_id, "Non-interpolated triggered from physics process");
		}
#endif

		return;
	}

	float new_checksum = TransformInterpolator::checksum_transform(p_transform);
	bool checksums_match = (instance->transform_checksum_curr == new_checksum) && (instance->transform_checksum_prev == new_checksum);

	// we can't entirely reject no changes because we need the interpolation
	// system to keep on stewing

	// Optimized check. First checks the checksums. If they pass it does the slow check at the end.
	// Alternatively we can do this non-optimized and ignore the checksum...
	// if no change
	if (checksums_match && (instance->transform_curr == p_transform) && (instance->transform_prev == p_transform)) {
		return;
	}

#ifdef DEBUG_ENABLED

	for (int i = 0; i < 4; i++) {
		const Vector3 &v = i < 3 ? p_transform.basis.elements[i] : p_transform.origin;
		ERR_FAIL_COND(Math::is_inf(v.x));
		ERR_FAIL_COND(Math::is_nan(v.x));
		ERR_FAIL_COND(Math::is_inf(v.y));
		ERR_FAIL_COND(Math::is_nan(v.y));
		ERR_FAIL_COND(Math::is_inf(v.z));
		ERR_FAIL_COND(Math::is_nan(v.z));
	}

#endif

	instance->transform_curr = p_transform;

#ifdef VISUAL_SERVER_DEBUG_PHYSICS_INTERPOLATION
	print_line("\tprev " + rtos(instance->transform_prev.origin.x) + ", curr " + rtos(instance->transform_curr.origin.x));
#endif

	// keep checksums up to date
	instance->transform_checksum_curr = new_checksum;

	if (!instance->on_interpolate_transform_list) {
		_interpolation_data.instance_transform_update_list_curr->push_back(p_instance);
		instance->on_interpolate_transform_list = true;
	} else {
		DEV_ASSERT(_interpolation_data.instance_transform_update_list_curr->size());
	}

	// If the instance is invisible, then we are simply updating the data flow, there is no need to calculate the interpolated
	// transform or anything else.
	// Ideally we would not even call the VisualServer::set_transform() when invisible but that would entail having logic
	// to keep track of the previous transform on the SceneTree side. The "early out" below is less efficient but a lot cleaner codewise.
	if (!instance->visible) {
		return;
	}

	// decide on the interpolation method .. slerp if possible
	instance->interpolation_method = TransformInterpolator::find_method(instance->transform_prev.basis, instance->transform_curr.basis);

	if (!instance->on_interpolate_list) {
		_interpolation_data.instance_interpolate_update_list.push_back(p_instance);
		instance->on_interpolate_list = true;
	} else {
		DEV_ASSERT(_interpolation_data.instance_interpolate_update_list.size());
	}

	_instance_queue_update(instance, true);

#if defined(DEBUG_ENABLED) && defined(TOOLS_ENABLED)
	if (!Engine::get_singleton()->is_in_physics_frame()) {
		PHYSICS_INTERPOLATION_NODE_WARNING(instance->object_id, "Interpolated triggered from outside physics process");
	}
#endif
}

void VisualServerScene::InterpolationData::notify_free_instance(RID p_rid, Instance &r_instance) {
	r_instance.on_interpolate_list = false;
	r_instance.on_interpolate_transform_list = false;

	if (!interpolation_enabled) {
		return;
	}

	// if the instance was on any of the lists, remove
	instance_interpolate_update_list.erase_multiple_unordered(p_rid);
	instance_transform_update_list_curr->erase_multiple_unordered(p_rid);
	instance_transform_update_list_prev->erase_multiple_unordered(p_rid);
}

void VisualServerScene::update_interpolation_tick(bool p_process) {
#ifdef VISUAL_SERVER_DEBUG_PHYSICS_INTERPOLATION
	print_line("update_interpolation_tick " + itos(Engine::get_singleton()->get_physics_frames()));
#endif

	// update interpolation in storage
	VSG::storage->update_interpolation_tick(p_process);

	// detect any that were on the previous transform list that are no longer active,
	// we should remove them from the interpolate list

	for (unsigned int n = 0; n < _interpolation_data.instance_transform_update_list_prev->size(); n++) {
		const RID &rid = (*_interpolation_data.instance_transform_update_list_prev)[n];
		Instance *instance = instance_owner.getornull(rid);

		bool active = true;

		// no longer active? (either the instance deleted or no longer being transformed)
		if (instance && !instance->on_interpolate_transform_list) {
			active = false;
			instance->on_interpolate_list = false;

			// make sure the most recent transform is set
			instance->transform = instance->transform_curr;

			// and that both prev and current are the same, just in case of any interpolations
			instance->transform_prev = instance->transform_curr;

			// make sure are updated one more time to ensure the AABBs are correct
			_instance_queue_update(instance, true);
		}

		if (!instance) {
			active = false;
		}

		if (!active) {
			_interpolation_data.instance_interpolate_update_list.erase(rid);
		}
	}

	// and now for any in the transform list (being actively interpolated), keep the previous transform
	// value up to date ready for the next tick
	if (p_process) {
		for (unsigned int n = 0; n < _interpolation_data.instance_transform_update_list_curr->size(); n++) {
			const RID &rid = (*_interpolation_data.instance_transform_update_list_curr)[n];
			Instance *instance = instance_owner.getornull(rid);
			if (instance) {
				instance->transform_prev = instance->transform_curr;
				instance->transform_checksum_prev = instance->transform_checksum_curr;
				instance->on_interpolate_transform_list = false;
			}
		}
	}

	// we maintain a mirror list for the transform updates, so we can detect when an instance
	// is no longer being transformed, and remove it from the interpolate list
	SWAP(_interpolation_data.instance_transform_update_list_curr, _interpolation_data.instance_transform_update_list_prev);

	// prepare for the next iteration
	_interpolation_data.instance_transform_update_list_curr->clear();
}

void VisualServerScene::update_interpolation_frame(bool p_process) {
	// update interpolation in storage
	VSG::storage->update_interpolation_frame(p_process);

	if (p_process) {
		real_t f = Engine::get_singleton()->get_physics_interpolation_fraction();

		for (unsigned int i = 0; i < _interpolation_data.instance_interpolate_update_list.size(); i++) {
			const RID &rid = _interpolation_data.instance_interpolate_update_list[i];
			Instance *instance = instance_owner.getornull(rid);
			if (instance) {
				TransformInterpolator::interpolate_transform_via_method(instance->transform_prev, instance->transform_curr, instance->transform, f, instance->interpolation_method);

#ifdef VISUAL_SERVER_DEBUG_PHYSICS_INTERPOLATION
				print_line("\t\tinterpolated: " + rtos(instance->transform.origin.x) + "\t( prev " + rtos(instance->transform_prev.origin.x) + ", curr " + rtos(instance->transform_curr.origin.x) + " ) on tick " + itos(Engine::get_singleton()->get_physics_frames()));
#endif

				// make sure AABBs are constantly up to date through the interpolation
				_instance_queue_update(instance, true);
			}
		} // for n
	}
}

void VisualServerScene::instance_attach_object_instance_id(RID p_instance, ObjectID p_id) {
	Instance *instance = instance_owner.get(p_instance);
	ERR_FAIL_COND(!instance);

	instance->object_id = p_id;
}
void VisualServerScene::instance_set_blend_shape_weight(RID p_instance, int p_shape, float p_weight) {
	Instance *instance = instance_owner.get(p_instance);
	ERR_FAIL_COND(!instance);

	if (instance->update_item.in_list()) {
		_update_dirty_instance(instance);
	}

	ERR_FAIL_INDEX(p_shape, instance->blend_values.size());
	instance->blend_values.write().ptr()[p_shape] = p_weight;
	VSG::storage->mesh_set_blend_shape_values(instance->base, instance->blend_values);
}

void VisualServerScene::instance_set_surface_material(RID p_instance, int p_surface, RID p_material) {
	Instance *instance = instance_owner.get(p_instance);
	ERR_FAIL_COND(!instance);

	if (instance->base_type == VS::INSTANCE_MESH) {
		//may not have been updated yet
		instance->materials.resize(VSG::storage->mesh_get_surface_count(instance->base));
	}

	ERR_FAIL_INDEX(p_surface, instance->materials.size());

	if (instance->materials[p_surface].is_valid()) {
		VSG::storage->material_remove_instance_owner(instance->materials[p_surface], instance);
	}
	instance->materials.write[p_surface] = p_material;
	instance->base_changed(false, true);

	if (instance->materials[p_surface].is_valid()) {
		VSG::storage->material_add_instance_owner(instance->materials[p_surface], instance);
	}
}

void VisualServerScene::instance_set_visible(RID p_instance, bool p_visible) {
	Instance *instance = instance_owner.get(p_instance);
	ERR_FAIL_COND(!instance);

	if (instance->visible == p_visible) {
		return;
	}

	instance->visible = p_visible;

	// Special case for physics interpolation, we want to ensure the interpolated data is up to date
	if (_interpolation_data.interpolation_enabled && p_visible && instance->interpolated && instance->scenario && !instance->on_interpolate_list) {
		// Do all the extra work we normally do on instance_set_transform(), because this is optimized out for hidden instances.
		// This prevents a glitch of stale interpolation transform data when unhiding before the next physics tick.
		instance->interpolation_method = TransformInterpolator::find_method(instance->transform_prev.basis, instance->transform_curr.basis);
		_interpolation_data.instance_interpolate_update_list.push_back(p_instance);
		instance->on_interpolate_list = true;
		_instance_queue_update(instance, true);

		// We must also place on the transform update list for a tick, so the system
		// can auto-detect if the instance is no longer moving, and remove from the interpolate lists again.
		// If this step is ignored, an unmoving instance could remain on the interpolate lists indefinitely
		// (or rather until the object is deleted) and cause unnecessary updates and drawcalls.
		if (!instance->on_interpolate_transform_list) {
			_interpolation_data.instance_transform_update_list_curr->push_back(p_instance);
			instance->on_interpolate_transform_list = true;
		}
	}

	// give the opportunity for the spatial partitioning scene to use a special implementation of visibility
	// for efficiency (supported in BVH but not octree)

	// slightly bug prone optimization here - we want to avoid doing a collision check twice
	// once when activating, and once when calling set_pairable. We do this by deferring the collision check.
	// However, in some cases (notably meshes), set_pairable never gets called. So we want to catch this case
	// and force a collision check (see later in this function).
	// This is only done in two stages to maintain compatibility with the octree.
	if (instance->spatial_partition_id && instance->scenario) {
		if (p_visible) {
			instance->scenario->sps->activate(instance->spatial_partition_id, instance->transformed_aabb);
		} else {
			instance->scenario->sps->deactivate(instance->spatial_partition_id);
		}
	}

	// when showing or hiding geometry, lights must be kept up to date to show / hide shadows
	if ((1 << instance->base_type) & VS::INSTANCE_GEOMETRY_MASK) {
		InstanceGeometryData *geom = static_cast<InstanceGeometryData *>(instance->base_data);

		if (geom->can_cast_shadows) {
			for (List<Instance *>::Element *E = geom->lighting.front(); E; E = E->next()) {
				InstanceLightData *light = static_cast<InstanceLightData *>(E->get()->base_data);
				light->make_shadow_dirty();
			}
		}
	}

	switch (instance->base_type) {
		case VS::INSTANCE_LIGHT: {
			if (VSG::storage->light_get_type(instance->base) != VS::LIGHT_DIRECTIONAL && instance->spatial_partition_id && instance->scenario) {
				instance->scenario->sps->set_pairable(instance, p_visible, 1 << VS::INSTANCE_LIGHT, p_visible ? VS::INSTANCE_GEOMETRY_MASK : 0);
			}

		} break;
		case VS::INSTANCE_REFLECTION_PROBE: {
			if (instance->spatial_partition_id && instance->scenario) {
				instance->scenario->sps->set_pairable(instance, p_visible, 1 << VS::INSTANCE_REFLECTION_PROBE, p_visible ? VS::INSTANCE_GEOMETRY_MASK : 0);
			}

		} break;
		case VS::INSTANCE_LIGHTMAP_CAPTURE: {
			if (instance->spatial_partition_id && instance->scenario) {
				instance->scenario->sps->set_pairable(instance, p_visible, 1 << VS::INSTANCE_LIGHTMAP_CAPTURE, p_visible ? VS::INSTANCE_GEOMETRY_MASK : 0);
			}

		} break;
		case VS::INSTANCE_GI_PROBE: {
			if (instance->spatial_partition_id && instance->scenario) {
				instance->scenario->sps->set_pairable(instance, p_visible, 1 << VS::INSTANCE_GI_PROBE, p_visible ? (VS::INSTANCE_GEOMETRY_MASK | (1 << VS::INSTANCE_LIGHT)) : 0);
			}

		} break;
		default: {
			// if we haven't called set_pairable, we STILL need to do a collision check
			// for activated items because we deferred it earlier in the call to activate.
			if (instance->spatial_partition_id && instance->scenario && p_visible) {
				instance->scenario->sps->force_collision_check(instance->spatial_partition_id);
			}
		}
	}
}
inline bool is_geometry_instance(VisualServer::InstanceType p_type) {
	return p_type == VS::INSTANCE_MESH || p_type == VS::INSTANCE_MULTIMESH || p_type == VS::INSTANCE_PARTICLES || p_type == VS::INSTANCE_IMMEDIATE;
}

void VisualServerScene::instance_set_use_lightmap(RID p_instance, RID p_lightmap_instance, RID p_lightmap, int p_lightmap_slice, const Rect2 &p_lightmap_uv_rect) {
	Instance *instance = instance_owner.get(p_instance);
	ERR_FAIL_COND(!instance);

	instance->lightmap = RID();
	instance->lightmap_slice = -1;
	instance->lightmap_uv_rect = Rect2(0, 0, 1, 1);
	instance->baked_light = false;

	if (instance->lightmap_capture) {
		InstanceLightmapCaptureData *lightmap_capture = static_cast<InstanceLightmapCaptureData *>(((Instance *)instance->lightmap_capture)->base_data);
		lightmap_capture->users.erase(instance);
		instance->lightmap_capture = nullptr;
	}

	if (p_lightmap_instance.is_valid()) {
		Instance *lightmap_instance = instance_owner.get(p_lightmap_instance);
		ERR_FAIL_COND(!lightmap_instance);
		ERR_FAIL_COND(lightmap_instance->base_type != VS::INSTANCE_LIGHTMAP_CAPTURE);
		instance->lightmap_capture = lightmap_instance;

		InstanceLightmapCaptureData *lightmap_capture = static_cast<InstanceLightmapCaptureData *>(((Instance *)instance->lightmap_capture)->base_data);
		lightmap_capture->users.insert(instance);
		instance->lightmap = p_lightmap;
		instance->lightmap_slice = p_lightmap_slice;
		instance->lightmap_uv_rect = p_lightmap_uv_rect;
		instance->baked_light = true;
	}
}

void VisualServerScene::instance_set_custom_aabb(RID p_instance, AABB p_aabb) {
	Instance *instance = instance_owner.get(p_instance);
	ERR_FAIL_COND(!instance);
	ERR_FAIL_COND(!is_geometry_instance(instance->base_type));

	if (p_aabb != AABB()) {
		// Set custom AABB
		if (instance->custom_aabb == nullptr) {
			instance->custom_aabb = memnew(AABB);
		}
		*instance->custom_aabb = p_aabb;

	} else {
		// Clear custom AABB
		if (instance->custom_aabb != nullptr) {
			memdelete(instance->custom_aabb);
			instance->custom_aabb = nullptr;
		}
	}

	if (instance->scenario) {
		_instance_queue_update(instance, true, false);
	}
}

void VisualServerScene::instance_attach_skeleton(RID p_instance, RID p_skeleton) {
	Instance *instance = instance_owner.get(p_instance);
	ERR_FAIL_COND(!instance);

	if (instance->skeleton == p_skeleton) {
		return;
	}

	if (instance->skeleton.is_valid()) {
		VSG::storage->instance_remove_skeleton(instance->skeleton, instance);
	}

	instance->skeleton = p_skeleton;

	if (instance->skeleton.is_valid()) {
		VSG::storage->instance_add_skeleton(instance->skeleton, instance);
	}

	_instance_queue_update(instance, true);
}

void VisualServerScene::instance_set_exterior(RID p_instance, bool p_enabled) {
}

void VisualServerScene::instance_set_extra_visibility_margin(RID p_instance, real_t p_margin) {
	Instance *instance = instance_owner.get(p_instance);
	ERR_FAIL_COND(!instance);

	instance->extra_margin = p_margin;
	_instance_queue_update(instance, true, false);
}

// Portals
void VisualServerScene::instance_set_portal_mode(RID p_instance, VisualServer::InstancePortalMode p_mode) {
	Instance *instance = instance_owner.get(p_instance);
	ERR_FAIL_COND(!instance);

	// no change?
	if (instance->portal_mode == p_mode) {
		return;
	}

	// should this happen?
	if (!instance->scenario) {
		instance->portal_mode = p_mode;
		return;
	}

	// destroy previous occlusion instance?
	_instance_destroy_occlusion_rep(instance);
	instance->portal_mode = p_mode;
	_instance_create_occlusion_rep(instance);
}

void VisualServerScene::_instance_create_occlusion_rep(Instance *p_instance) {
	ERR_FAIL_COND(!p_instance);
	ERR_FAIL_COND(!p_instance->scenario);

	switch (p_instance->portal_mode) {
		default: {
			p_instance->occlusion_handle = 0;
		} break;
		case VisualServer::InstancePortalMode::INSTANCE_PORTAL_MODE_ROAMING: {
			p_instance->occlusion_handle = p_instance->scenario->_portal_renderer.instance_moving_create(p_instance, p_instance->self, false, p_instance->transformed_aabb);
		} break;
		case VisualServer::InstancePortalMode::INSTANCE_PORTAL_MODE_GLOBAL: {
			p_instance->occlusion_handle = p_instance->scenario->_portal_renderer.instance_moving_create(p_instance, p_instance->self, true, p_instance->transformed_aabb);
		} break;
	}
}

void VisualServerScene::_instance_destroy_occlusion_rep(Instance *p_instance) {
	ERR_FAIL_COND(!p_instance);
	ERR_FAIL_COND(!p_instance->scenario);

	// not an error, can occur
	if (!p_instance->occlusion_handle) {
		return;
	}

	p_instance->scenario->_portal_renderer.instance_moving_destroy(p_instance->occlusion_handle);

	// unset
	p_instance->occlusion_handle = 0;
}

void *VisualServerScene::_instance_get_from_rid(RID p_instance) {
	Instance *instance = instance_owner.get(p_instance);
	return instance;
}

bool VisualServerScene::_instance_get_transformed_aabb(RID p_instance, AABB &r_aabb) {
	Instance *instance = instance_owner.get(p_instance);
	ERR_FAIL_NULL_V(instance, false);

	r_aabb = instance->transformed_aabb;

	return true;
}

// the portal has to be associated with a scenario, this is assumed to be
// the same scenario as the portal node
RID VisualServerScene::portal_create() {
	Portal *portal = memnew(Portal);
	ERR_FAIL_COND_V(!portal, RID());
	RID portal_rid = portal_owner.make_rid(portal);
	return portal_rid;
}

// should not be called multiple times, different scenarios etc, but just in case, we will support this
void VisualServerScene::portal_set_scenario(RID p_portal, RID p_scenario) {
	Portal *portal = portal_owner.getornull(p_portal);
	ERR_FAIL_COND(!portal);
	Scenario *scenario = scenario_owner.getornull(p_scenario);

	// noop?
	if (portal->scenario == scenario) {
		return;
	}

	// if the portal is in a scenario already, remove it
	if (portal->scenario) {
		portal->scenario->_portal_renderer.portal_destroy(portal->scenario_portal_id);
		portal->scenario = nullptr;
		portal->scenario_portal_id = 0;
	}

	// create when entering the world
	if (scenario) {
		portal->scenario = scenario;

		// defer the actual creation to here
		portal->scenario_portal_id = scenario->_portal_renderer.portal_create();
	}
}

void VisualServerScene::portal_set_geometry(RID p_portal, const Vector<Vector3> &p_points, real_t p_margin) {
	Portal *portal = portal_owner.getornull(p_portal);
	ERR_FAIL_COND(!portal);
	ERR_FAIL_COND(!portal->scenario);
	portal->scenario->_portal_renderer.portal_set_geometry(portal->scenario_portal_id, p_points, p_margin);
}

void VisualServerScene::portal_link(RID p_portal, RID p_room_from, RID p_room_to, bool p_two_way) {
	Portal *portal = portal_owner.getornull(p_portal);
	ERR_FAIL_COND(!portal);
	ERR_FAIL_COND(!portal->scenario);

	Room *room_from = room_owner.getornull(p_room_from);
	ERR_FAIL_COND(!room_from);
	Room *room_to = room_owner.getornull(p_room_to);
	ERR_FAIL_COND(!room_to);

	portal->scenario->_portal_renderer.portal_link(portal->scenario_portal_id, room_from->scenario_room_id, room_to->scenario_room_id, p_two_way);
}

void VisualServerScene::portal_set_active(RID p_portal, bool p_active) {
	Portal *portal = portal_owner.getornull(p_portal);
	ERR_FAIL_COND(!portal);
	ERR_FAIL_COND(!portal->scenario);
	portal->scenario->_portal_renderer.portal_set_active(portal->scenario_portal_id, p_active);
}

RID VisualServerScene::ghost_create() {
	Ghost *ci = memnew(Ghost);
	ERR_FAIL_COND_V(!ci, RID());
	RID ci_rid = ghost_owner.make_rid(ci);
	return ci_rid;
}

void VisualServerScene::ghost_set_scenario(RID p_ghost, RID p_scenario, ObjectID p_id, const AABB &p_aabb) {
	Ghost *ci = ghost_owner.getornull(p_ghost);
	ERR_FAIL_COND(!ci);

	ci->aabb = p_aabb;
	ci->object_id = p_id;

	Scenario *scenario = scenario_owner.getornull(p_scenario);

	// noop?
	if (ci->scenario == scenario) {
		return;
	}

	// if the portal is in a scenario already, remove it
	if (ci->scenario) {
		_ghost_destroy_occlusion_rep(ci);
		ci->scenario = nullptr;
	}

	// create when entering the world
	if (scenario) {
		ci->scenario = scenario;

		// defer the actual creation to here
		_ghost_create_occlusion_rep(ci);
	}
}

void VisualServerScene::ghost_update(RID p_ghost, const AABB &p_aabb) {
	Ghost *ci = ghost_owner.getornull(p_ghost);
	ERR_FAIL_COND(!ci);
	ERR_FAIL_COND(!ci->scenario);

	ci->aabb = p_aabb;

	if (ci->rghost_handle) {
		ci->scenario->_portal_renderer.rghost_update(ci->rghost_handle, p_aabb);
	}
}

void VisualServerScene::_ghost_create_occlusion_rep(Ghost *p_ghost) {
	ERR_FAIL_COND(!p_ghost);
	ERR_FAIL_COND(!p_ghost->scenario);

	if (!p_ghost->rghost_handle) {
		p_ghost->rghost_handle = p_ghost->scenario->_portal_renderer.rghost_create(p_ghost->object_id, p_ghost->aabb);
	}
}

void VisualServerScene::_ghost_destroy_occlusion_rep(Ghost *p_ghost) {
	ERR_FAIL_COND(!p_ghost);
	ERR_FAIL_COND(!p_ghost->scenario);

	// not an error, can occur
	if (!p_ghost->rghost_handle) {
		return;
	}

	p_ghost->scenario->_portal_renderer.rghost_destroy(p_ghost->rghost_handle);
	p_ghost->rghost_handle = 0;
}

RID VisualServerScene::roomgroup_create() {
	RoomGroup *rg = memnew(RoomGroup);
	ERR_FAIL_COND_V(!rg, RID());
	RID roomgroup_rid = roomgroup_owner.make_rid(rg);
	return roomgroup_rid;
}

void VisualServerScene::roomgroup_prepare(RID p_roomgroup, ObjectID p_roomgroup_object_id) {
	RoomGroup *roomgroup = roomgroup_owner.getornull(p_roomgroup);
	ERR_FAIL_COND(!roomgroup);
	ERR_FAIL_COND(!roomgroup->scenario);
	roomgroup->scenario->_portal_renderer.roomgroup_prepare(roomgroup->scenario_roomgroup_id, p_roomgroup_object_id);
}

void VisualServerScene::roomgroup_set_scenario(RID p_roomgroup, RID p_scenario) {
	RoomGroup *rg = roomgroup_owner.getornull(p_roomgroup);
	ERR_FAIL_COND(!rg);
	Scenario *scenario = scenario_owner.getornull(p_scenario);

	// noop?
	if (rg->scenario == scenario) {
		return;
	}

	// if the portal is in a scenario already, remove it
	if (rg->scenario) {
		rg->scenario->_portal_renderer.roomgroup_destroy(rg->scenario_roomgroup_id);
		rg->scenario = nullptr;
		rg->scenario_roomgroup_id = 0;
	}

	// create when entering the world
	if (scenario) {
		rg->scenario = scenario;

		// defer the actual creation to here
		rg->scenario_roomgroup_id = scenario->_portal_renderer.roomgroup_create();
	}
}

void VisualServerScene::roomgroup_add_room(RID p_roomgroup, RID p_room) {
	RoomGroup *roomgroup = roomgroup_owner.getornull(p_roomgroup);
	ERR_FAIL_COND(!roomgroup);
	ERR_FAIL_COND(!roomgroup->scenario);

	Room *room = room_owner.getornull(p_room);
	ERR_FAIL_COND(!room);
	ERR_FAIL_COND(!room->scenario);

	ERR_FAIL_COND(roomgroup->scenario != room->scenario);
	roomgroup->scenario->_portal_renderer.roomgroup_add_room(roomgroup->scenario_roomgroup_id, room->scenario_room_id);
}

// Occluders
RID VisualServerScene::occluder_instance_create() {
	OccluderInstance *ro = memnew(OccluderInstance);
	ERR_FAIL_COND_V(!ro, RID());
	RID occluder_rid = occluder_instance_owner.make_rid(ro);
	return occluder_rid;
}

void VisualServerScene::occluder_instance_link_resource(RID p_occluder_instance, RID p_occluder_resource) {
	OccluderInstance *oi = occluder_instance_owner.getornull(p_occluder_instance);
	ERR_FAIL_COND(!oi);
	ERR_FAIL_COND(!oi->scenario);

	OccluderResource *res = occluder_resource_owner.getornull(p_occluder_resource);
	ERR_FAIL_COND(!res);

	oi->scenario->_portal_renderer.occluder_instance_link(oi->scenario_occluder_id, res->occluder_resource_id);
}

void VisualServerScene::occluder_instance_set_scenario(RID p_occluder_instance, RID p_scenario) {
	OccluderInstance *oi = occluder_instance_owner.getornull(p_occluder_instance);
	ERR_FAIL_COND(!oi);
	Scenario *scenario = scenario_owner.getornull(p_scenario);

	// noop?
	if (oi->scenario == scenario) {
		return;
	}

	// if the portal is in a scenario already, remove it
	if (oi->scenario) {
		oi->scenario->_portal_renderer.occluder_instance_destroy(oi->scenario_occluder_id);
		oi->scenario = nullptr;
		oi->scenario_occluder_id = 0;
	}

	// create when entering the world
	if (scenario) {
		oi->scenario = scenario;
		oi->scenario_occluder_id = scenario->_portal_renderer.occluder_instance_create();
	}
}

void VisualServerScene::occluder_instance_set_active(RID p_occluder_instance, bool p_active) {
	OccluderInstance *oi = occluder_instance_owner.getornull(p_occluder_instance);
	ERR_FAIL_COND(!oi);
	ERR_FAIL_COND(!oi->scenario);
	oi->scenario->_portal_renderer.occluder_instance_set_active(oi->scenario_occluder_id, p_active);
}

void VisualServerScene::occluder_instance_set_transform(RID p_occluder_instance, const Transform &p_xform) {
	OccluderInstance *oi = occluder_instance_owner.getornull(p_occluder_instance);
	ERR_FAIL_COND(!oi);
	ERR_FAIL_COND(!oi->scenario);
	oi->scenario->_portal_renderer.occluder_instance_set_transform(oi->scenario_occluder_id, p_xform);
}

RID VisualServerScene::occluder_resource_create() {
	OccluderResource *res = memnew(OccluderResource);
	ERR_FAIL_COND_V(!res, RID());

	res->occluder_resource_id = _portal_resources.occluder_resource_create();

	RID occluder_resource_rid = occluder_resource_owner.make_rid(res);
	return occluder_resource_rid;
}

void VisualServerScene::occluder_resource_prepare(RID p_occluder_resource, VisualServer::OccluderType p_type) {
	OccluderResource *res = occluder_resource_owner.getornull(p_occluder_resource);
	ERR_FAIL_COND(!res);
	_portal_resources.occluder_resource_prepare(res->occluder_resource_id, (VSOccluder_Instance::Type)p_type);
}

void VisualServerScene::occluder_resource_spheres_update(RID p_occluder_resource, const Vector<Plane> &p_spheres) {
	OccluderResource *res = occluder_resource_owner.getornull(p_occluder_resource);
	ERR_FAIL_COND(!res);
	_portal_resources.occluder_resource_update_spheres(res->occluder_resource_id, p_spheres);
}

void VisualServerScene::occluder_resource_mesh_update(RID p_occluder_resource, const Geometry::OccluderMeshData &p_mesh_data) {
	OccluderResource *res = occluder_resource_owner.getornull(p_occluder_resource);
	ERR_FAIL_COND(!res);
	_portal_resources.occluder_resource_update_mesh(res->occluder_resource_id, p_mesh_data);
}

void VisualServerScene::set_use_occlusion_culling(bool p_enable) {
	// this is not scenario specific, and is global
	// (mainly for debugging)
	PortalRenderer::use_occlusion_culling = p_enable;
}

Geometry::MeshData VisualServerScene::occlusion_debug_get_current_polys(RID p_scenario) const {
	Scenario *scenario = scenario_owner.getornull(p_scenario);
	if (!scenario) {
		return Geometry::MeshData();
	}

	return scenario->_portal_renderer.occlusion_debug_get_current_polys();
}

// Rooms
void VisualServerScene::callbacks_register(VisualServerCallbacks *p_callbacks) {
	_visual_server_callbacks = p_callbacks;
}

// the room has to be associated with a scenario, this is assumed to be
// the same scenario as the room node
RID VisualServerScene::room_create() {
	Room *room = memnew(Room);
	ERR_FAIL_COND_V(!room, RID());
	RID room_rid = room_owner.make_rid(room);
	return room_rid;
}

// should not be called multiple times, different scenarios etc, but just in case, we will support this
void VisualServerScene::room_set_scenario(RID p_room, RID p_scenario) {
	Room *room = room_owner.getornull(p_room);
	ERR_FAIL_COND(!room);
	Scenario *scenario = scenario_owner.getornull(p_scenario);

	// no change?
	if (room->scenario == scenario) {
		return;
	}

	// if the room has an existing scenario, remove from it
	if (room->scenario) {
		room->scenario->_portal_renderer.room_destroy(room->scenario_room_id);
		room->scenario = nullptr;
		room->scenario_room_id = 0;
	}

	// create when entering the world
	if (scenario) {
		room->scenario = scenario;

		// defer the actual creation to here
		room->scenario_room_id = scenario->_portal_renderer.room_create();
	}
}

void VisualServerScene::room_add_ghost(RID p_room, ObjectID p_object_id, const AABB &p_aabb) {
	Room *room = room_owner.getornull(p_room);
	ERR_FAIL_COND(!room);
	ERR_FAIL_COND(!room->scenario);

	room->scenario->_portal_renderer.room_add_ghost(room->scenario_room_id, p_object_id, p_aabb);
}

void VisualServerScene::room_add_instance(RID p_room, RID p_instance, const AABB &p_aabb, const Vector<Vector3> &p_object_pts) {
	Room *room = room_owner.getornull(p_room);
	ERR_FAIL_COND(!room);
	ERR_FAIL_COND(!room->scenario);

	Instance *instance = instance_owner.getornull(p_instance);
	ERR_FAIL_COND(!instance);

	AABB bb = p_aabb;

	// the aabb passed from the client takes no account of the extra cull margin,
	// so we need to add this manually.
	// It is assumed it is in world space.
	if (instance->extra_margin != 0.0) {
		bb.grow_by(instance->extra_margin);
	}

	bool dynamic = false;

	// don't add if portal mode is not static or dynamic
	switch (instance->portal_mode) {
		default: {
			return; // this should be taken care of by the calling function, but just in case
		} break;
		case VisualServer::InstancePortalMode::INSTANCE_PORTAL_MODE_DYNAMIC: {
			dynamic = true;
		} break;
		case VisualServer::InstancePortalMode::INSTANCE_PORTAL_MODE_STATIC: {
			dynamic = false;
		} break;
	}

	instance->occlusion_handle = room->scenario->_portal_renderer.room_add_instance(room->scenario_room_id, p_instance, bb, dynamic, p_object_pts);
}

void VisualServerScene::room_prepare(RID p_room, int32_t p_priority) {
	Room *room = room_owner.getornull(p_room);
	ERR_FAIL_COND(!room);
	ERR_FAIL_COND(!room->scenario);
	room->scenario->_portal_renderer.room_prepare(room->scenario_room_id, p_priority);
}

void VisualServerScene::room_set_bound(RID p_room, ObjectID p_room_object_id, const Vector<Plane> &p_convex, const AABB &p_aabb, const Vector<Vector3> &p_verts) {
	Room *room = room_owner.getornull(p_room);
	ERR_FAIL_COND(!room);
	ERR_FAIL_COND(!room->scenario);
	room->scenario->_portal_renderer.room_set_bound(room->scenario_room_id, p_room_object_id, p_convex, p_aabb, p_verts);
}

void VisualServerScene::rooms_unload(RID p_scenario, String p_reason) {
	Scenario *scenario = scenario_owner.getornull(p_scenario);
	ERR_FAIL_COND(!scenario);
	scenario->_portal_renderer.rooms_unload(p_reason);
}

void VisualServerScene::rooms_and_portals_clear(RID p_scenario) {
	Scenario *scenario = scenario_owner.getornull(p_scenario);
	ERR_FAIL_COND(!scenario);
	scenario->_portal_renderer.rooms_and_portals_clear();
}

void VisualServerScene::rooms_finalize(RID p_scenario, bool p_generate_pvs, bool p_cull_using_pvs, bool p_use_secondary_pvs, bool p_use_signals, String p_pvs_filename, bool p_use_simple_pvs, bool p_log_pvs_generation) {
	Scenario *scenario = scenario_owner.getornull(p_scenario);
	ERR_FAIL_COND(!scenario);
	scenario->_portal_renderer.rooms_finalize(p_generate_pvs, p_cull_using_pvs, p_use_secondary_pvs, p_use_signals, p_pvs_filename, p_use_simple_pvs, p_log_pvs_generation);
}

void VisualServerScene::rooms_override_camera(RID p_scenario, bool p_override, const Vector3 &p_point, const Vector<Plane> *p_convex) {
	Scenario *scenario = scenario_owner.getornull(p_scenario);
	ERR_FAIL_COND(!scenario);
	scenario->_portal_renderer.rooms_override_camera(p_override, p_point, p_convex);
}

void VisualServerScene::rooms_set_active(RID p_scenario, bool p_active) {
	Scenario *scenario = scenario_owner.getornull(p_scenario);
	ERR_FAIL_COND(!scenario);
	scenario->_portal_renderer.rooms_set_active(p_active);
}

void VisualServerScene::rooms_set_params(RID p_scenario, int p_portal_depth_limit, real_t p_roaming_expansion_margin) {
	Scenario *scenario = scenario_owner.getornull(p_scenario);
	ERR_FAIL_COND(!scenario);
	scenario->_portal_renderer.rooms_set_params(p_portal_depth_limit, p_roaming_expansion_margin);
}

void VisualServerScene::rooms_set_debug_feature(RID p_scenario, VisualServer::RoomsDebugFeature p_feature, bool p_active) {
	Scenario *scenario = scenario_owner.getornull(p_scenario);
	ERR_FAIL_COND(!scenario);
	switch (p_feature) {
		default: {
		} break;
		case VisualServer::ROOMS_DEBUG_SPRAWL: {
			scenario->_portal_renderer.set_debug_sprawl(p_active);
		} break;
	}
}

void VisualServerScene::rooms_update_gameplay_monitor(RID p_scenario, const Vector<Vector3> &p_camera_positions) {
	Scenario *scenario = scenario_owner.getornull(p_scenario);
	ERR_FAIL_COND(!scenario);
	scenario->_portal_renderer.rooms_update_gameplay_monitor(p_camera_positions);
}

bool VisualServerScene::rooms_is_loaded(RID p_scenario) const {
	Scenario *scenario = scenario_owner.getornull(p_scenario);
	ERR_FAIL_COND_V(!scenario, false);
	return scenario->_portal_renderer.rooms_is_loaded();
}

Vector<ObjectID> VisualServerScene::instances_cull_aabb(const AABB &p_aabb, RID p_scenario) const {
	Vector<ObjectID> instances;
	Scenario *scenario = scenario_owner.get(p_scenario);
	ERR_FAIL_COND_V(!scenario, instances);

	const_cast<VisualServerScene *>(this)->update_dirty_instances(); // check dirty instances before culling

	int culled = 0;
	Instance *cull[1024];
	culled = scenario->sps->cull_aabb(p_aabb, cull, 1024);

	for (int i = 0; i < culled; i++) {
		Instance *instance = cull[i];
		ERR_CONTINUE(!instance);
		if (instance->object_id == 0) {
			continue;
		}

		instances.push_back(instance->object_id);
	}

	return instances;
}
Vector<ObjectID> VisualServerScene::instances_cull_ray(const Vector3 &p_from, const Vector3 &p_to, RID p_scenario) const {
	Vector<ObjectID> instances;
	Scenario *scenario = scenario_owner.get(p_scenario);
	ERR_FAIL_COND_V(!scenario, instances);
	const_cast<VisualServerScene *>(this)->update_dirty_instances(); // check dirty instances before culling

	int culled = 0;
	Instance *cull[1024];
	culled = scenario->sps->cull_segment(p_from, p_from + p_to * 10000, cull, 1024);

	for (int i = 0; i < culled; i++) {
		Instance *instance = cull[i];
		ERR_CONTINUE(!instance);
		if (instance->object_id == 0) {
			continue;
		}

		instances.push_back(instance->object_id);
	}

	return instances;
}
Vector<ObjectID> VisualServerScene::instances_cull_convex(const Vector<Plane> &p_convex, RID p_scenario) const {
	Vector<ObjectID> instances;
	Scenario *scenario = scenario_owner.get(p_scenario);
	ERR_FAIL_COND_V(!scenario, instances);
	const_cast<VisualServerScene *>(this)->update_dirty_instances(); // check dirty instances before culling

	int culled = 0;
	Instance *cull[1024];

	culled = scenario->sps->cull_convex(p_convex, cull, 1024);

	for (int i = 0; i < culled; i++) {
		Instance *instance = cull[i];
		ERR_CONTINUE(!instance);
		if (instance->object_id == 0) {
			continue;
		}

		instances.push_back(instance->object_id);
	}

	return instances;
}

// thin wrapper to allow rooms / portals to take over culling if active
int VisualServerScene::_cull_convex_from_point(Scenario *p_scenario, const Transform &p_cam_transform, const CameraMatrix &p_cam_projection, const Vector<Plane> &p_convex, Instance **p_result_array, int p_result_max, int32_t &r_previous_room_id_hint, uint32_t p_mask) {
	int res = -1;
	if (p_scenario->_portal_renderer.is_active()) {
		// Note that the portal renderer ASSUMES that the planes exactly match the convention in
		// CameraMatrix of enum Planes (6 planes, in order, near, far etc)
		// If this is not the case, it should not be used.
		res = p_scenario->_portal_renderer.cull_convex(p_cam_transform, p_cam_projection, p_convex, (VSInstance **)p_result_array, p_result_max, p_mask, r_previous_room_id_hint);
	}

	// fallback to BVH  / octree if portals not active
	if (res == -1) {
		res = p_scenario->sps->cull_convex(p_convex, p_result_array, p_result_max, p_mask);

		// Opportunity for occlusion culling on the main scene. This will be a noop if no occluders.
		if (p_scenario->_portal_renderer.occlusion_is_active()) {
			res = p_scenario->_portal_renderer.occlusion_cull(p_cam_transform, p_cam_projection, p_convex, (VSInstance **)p_result_array, res);
		}
	}
	return res;
}

void VisualServerScene::_rooms_instance_update(Instance *p_instance, const AABB &p_aabb) {
	// magic number for instances in the room / portal system, but not requiring an update
	// (due to being a STATIC or DYNAMIC object within a room)
	// Must match the value in PortalRenderer in VisualServer
	const uint32_t OCCLUSION_HANDLE_ROOM_BIT = 1 << 31;

	// if the instance is a moving object in the room / portal system, update it
	// Note that if rooms and portals is not in use, occlusion_handle should be zero in all cases unless the portal_mode
	// has been set to global or roaming. (which is unlikely as the default is static).
	// The exception is editor user interface elements.
	// These are always set to global and will always keep their aabb up to date in the portal renderer unnecessarily.
	// There is no easy way around this, but it should be very cheap, and have no impact outside the editor.
	if (p_instance->occlusion_handle && (p_instance->occlusion_handle != OCCLUSION_HANDLE_ROOM_BIT)) {
		p_instance->scenario->_portal_renderer.instance_moving_update(p_instance->occlusion_handle, p_aabb);
	}
}

void VisualServerScene::instance_geometry_set_flag(RID p_instance, VS::InstanceFlags p_flags, bool p_enabled) {
	Instance *instance = instance_owner.get(p_instance);
	ERR_FAIL_COND(!instance);

	switch (p_flags) {
		case VS::INSTANCE_FLAG_USE_BAKED_LIGHT: {
			instance->baked_light = p_enabled;

		} break;
		case VS::INSTANCE_FLAG_DRAW_NEXT_FRAME_IF_VISIBLE: {
			instance->redraw_if_visible = p_enabled;

		} break;
		default: {
		}
	}
}
void VisualServerScene::instance_geometry_set_cast_shadows_setting(RID p_instance, VS::ShadowCastingSetting p_shadow_casting_setting) {
	Instance *instance = instance_owner.get(p_instance);
	ERR_FAIL_COND(!instance);

	instance->cast_shadows = p_shadow_casting_setting;
	instance->base_changed(false, true); // to actually compute if shadows are visible or not
}
void VisualServerScene::instance_geometry_set_material_override(RID p_instance, RID p_material) {
	Instance *instance = instance_owner.get(p_instance);
	ERR_FAIL_COND(!instance);

	if (instance->material_override.is_valid()) {
		VSG::storage->material_remove_instance_owner(instance->material_override, instance);
	}
	instance->material_override = p_material;
	instance->base_changed(false, true);

	if (instance->material_override.is_valid()) {
		VSG::storage->material_add_instance_owner(instance->material_override, instance);
	}
}
void VisualServerScene::instance_geometry_set_material_overlay(RID p_instance, RID p_material) {
	Instance *instance = instance_owner.get(p_instance);
	ERR_FAIL_COND(!instance);

	if (instance->material_overlay.is_valid()) {
		VSG::storage->material_remove_instance_owner(instance->material_overlay, instance);
	}
	instance->material_overlay = p_material;
	instance->base_changed(false, true);

	if (instance->material_overlay.is_valid()) {
		VSG::storage->material_add_instance_owner(instance->material_overlay, instance);
	}
}

void VisualServerScene::_update_instance(Instance *p_instance) {
	p_instance->version++;

	// when not using interpolation the transform is used straight
	const Transform *instance_xform = &p_instance->transform;

	// Can possibly use the most up to date current transform here when using physics interpolation ..
	// uncomment the next line for this..
	// if (p_instance->is_currently_interpolated()) {
	// instance_xform = &p_instance->transform_curr;
	// }
	// However it does seem that using the interpolated transform (transform) works for keeping AABBs
	// up to date to avoid culling errors.

	if (p_instance->base_type == VS::INSTANCE_LIGHT) {
		InstanceLightData *light = static_cast<InstanceLightData *>(p_instance->base_data);

		VSG::scene_render->light_instance_set_transform(light->instance, *instance_xform);
		light->make_shadow_dirty();
	}

	if (p_instance->base_type == VS::INSTANCE_REFLECTION_PROBE) {
		InstanceReflectionProbeData *reflection_probe = static_cast<InstanceReflectionProbeData *>(p_instance->base_data);

		VSG::scene_render->reflection_probe_instance_set_transform(reflection_probe->instance, *instance_xform);
		reflection_probe->reflection_dirty = true;
	}

	if (p_instance->base_type == VS::INSTANCE_PARTICLES) {
		VSG::storage->particles_set_emission_transform(p_instance->base, *instance_xform);
	}

	if (p_instance->base_type == VS::INSTANCE_LIGHTMAP_CAPTURE) {
		InstanceLightmapCaptureData *capture = static_cast<InstanceLightmapCaptureData *>(p_instance->base_data);
		for (List<InstanceLightmapCaptureData::PairInfo>::Element *E = capture->geometries.front(); E; E = E->next()) {
			_instance_queue_update(E->get().geometry, false, true);
		}
	}

	if (p_instance->aabb.has_no_surface()) {
		return;
	}

	if ((1 << p_instance->base_type) & VS::INSTANCE_GEOMETRY_MASK) {
		InstanceGeometryData *geom = static_cast<InstanceGeometryData *>(p_instance->base_data);
		//make sure lights are updated if it casts shadow

		if (geom->can_cast_shadows) {
			for (List<Instance *>::Element *E = geom->lighting.front(); E; E = E->next()) {
				InstanceLightData *light = static_cast<InstanceLightData *>(E->get()->base_data);
				light->make_shadow_dirty();
			}
		}

		if (!p_instance->lightmap_capture && geom->lightmap_captures.size()) {
			//affected by lightmap captures, must update capture info!
			_update_instance_lightmap_captures(p_instance);
		} else {
			if (!p_instance->lightmap_capture_data.empty()) {
				p_instance->lightmap_capture_data.resize(0); //not in use, clear capture data
			}
		}
	}

	p_instance->mirror = instance_xform->basis.determinant() < 0.0;

	AABB new_aabb;

	new_aabb = instance_xform->xform(p_instance->aabb);

	p_instance->transformed_aabb = new_aabb;

	if (!p_instance->scenario) {
		return;
	}

	if (p_instance->spatial_partition_id == 0) {
		uint32_t base_type = 1 << p_instance->base_type;
		uint32_t pairable_mask = 0;
		bool pairable = false;

		if (p_instance->base_type == VS::INSTANCE_LIGHT || p_instance->base_type == VS::INSTANCE_REFLECTION_PROBE || p_instance->base_type == VS::INSTANCE_LIGHTMAP_CAPTURE) {
			pairable_mask = p_instance->visible ? VS::INSTANCE_GEOMETRY_MASK : 0;
			pairable = true;
		}

		if (p_instance->base_type == VS::INSTANCE_GI_PROBE) {
			//lights and geometries
			pairable_mask = p_instance->visible ? VS::INSTANCE_GEOMETRY_MASK | (1 << VS::INSTANCE_LIGHT) : 0;
			pairable = true;
		}

		// not inside octree
		p_instance->spatial_partition_id = p_instance->scenario->sps->create(p_instance, new_aabb, 0, pairable, base_type, pairable_mask);

	} else {
		/*
		if (new_aabb==p_instance->data.transformed_aabb)
			return;
		*/

		p_instance->scenario->sps->move(p_instance->spatial_partition_id, new_aabb);
	}

	// keep rooms and portals instance up to date if present
	_rooms_instance_update(p_instance, new_aabb);
}

void VisualServerScene::_update_instance_aabb(Instance *p_instance) {
	AABB new_aabb;

	ERR_FAIL_COND(p_instance->base_type != VS::INSTANCE_NONE && !p_instance->base.is_valid());

	switch (p_instance->base_type) {
		case VisualServer::INSTANCE_NONE: {
			// do nothing
		} break;
		case VisualServer::INSTANCE_MESH: {
			if (p_instance->custom_aabb) {
				new_aabb = *p_instance->custom_aabb;
			} else {
				new_aabb = VSG::storage->mesh_get_aabb(p_instance->base, p_instance->skeleton);
			}

		} break;

		case VisualServer::INSTANCE_MULTIMESH: {
			if (p_instance->custom_aabb) {
				new_aabb = *p_instance->custom_aabb;
			} else {
				new_aabb = VSG::storage->multimesh_get_aabb(p_instance->base);
			}

		} break;
		case VisualServer::INSTANCE_IMMEDIATE: {
			if (p_instance->custom_aabb) {
				new_aabb = *p_instance->custom_aabb;
			} else {
				new_aabb = VSG::storage->immediate_get_aabb(p_instance->base);
			}

		} break;
		case VisualServer::INSTANCE_PARTICLES: {
			if (p_instance->custom_aabb) {
				new_aabb = *p_instance->custom_aabb;
			} else {
				new_aabb = VSG::storage->particles_get_aabb(p_instance->base);
			}

		} break;
		case VisualServer::INSTANCE_LIGHT: {
			new_aabb = VSG::storage->light_get_aabb(p_instance->base);

		} break;
		case VisualServer::INSTANCE_REFLECTION_PROBE: {
			new_aabb = VSG::storage->reflection_probe_get_aabb(p_instance->base);

		} break;
		case VisualServer::INSTANCE_GI_PROBE: {
			new_aabb = VSG::storage->gi_probe_get_bounds(p_instance->base);

		} break;
		case VisualServer::INSTANCE_LIGHTMAP_CAPTURE: {
			new_aabb = VSG::storage->lightmap_capture_get_bounds(p_instance->base);

		} break;
		default: {
		}
	}

	// <Zylann> This is why I didn't re-use Instance::aabb to implement custom AABBs
	if (p_instance->extra_margin) {
		new_aabb.grow_by(p_instance->extra_margin);
	}

	p_instance->aabb = new_aabb;
}

_FORCE_INLINE_ static void _light_capture_sample_octree(const RasterizerStorage::LightmapCaptureOctree *p_octree, int p_cell_subdiv, const Vector3 &p_pos, const Vector3 &p_dir, float p_level, Vector3 &r_color, float &r_alpha) {
	static const Vector3 aniso_normal[6] = {
		Vector3(-1, 0, 0),
		Vector3(1, 0, 0),
		Vector3(0, -1, 0),
		Vector3(0, 1, 0),
		Vector3(0, 0, -1),
		Vector3(0, 0, 1)
	};

	int size = 1 << (p_cell_subdiv - 1);

	int clamp_v = size - 1;
	//first of all, clamp
	Vector3 pos;
	pos.x = CLAMP(p_pos.x, 0, clamp_v);
	pos.y = CLAMP(p_pos.y, 0, clamp_v);
	pos.z = CLAMP(p_pos.z, 0, clamp_v);

	float level = (p_cell_subdiv - 1) - p_level;

	int target_level;
	float level_filter;
	if (level <= 0.0) {
		level_filter = 0;
		target_level = 0;
	} else {
		target_level = Math::ceil(level);
		level_filter = target_level - level;
	}

	Vector3 color[2][8];
	float alpha[2][8];
	memset(alpha, 0, sizeof(float) * 2 * 8);

	//find cell at given level first

	for (int c = 0; c < 2; c++) {
		int current_level = MAX(0, target_level - c);
		int level_cell_size = (1 << (p_cell_subdiv - 1)) >> current_level;

		for (int n = 0; n < 8; n++) {
			int x = int(pos.x);
			int y = int(pos.y);
			int z = int(pos.z);

			if (n & 1) {
				x += level_cell_size;
			}
			if (n & 2) {
				y += level_cell_size;
			}
			if (n & 4) {
				z += level_cell_size;
			}

			int ofs_x = 0;
			int ofs_y = 0;
			int ofs_z = 0;

			x = CLAMP(x, 0, clamp_v);
			y = CLAMP(y, 0, clamp_v);
			z = CLAMP(z, 0, clamp_v);

			int half = size / 2;
			uint32_t cell = 0;
			for (int i = 0; i < current_level; i++) {
				const RasterizerStorage::LightmapCaptureOctree *bc = &p_octree[cell];

				int child = 0;
				if (x >= ofs_x + half) {
					child |= 1;
					ofs_x += half;
				}
				if (y >= ofs_y + half) {
					child |= 2;
					ofs_y += half;
				}
				if (z >= ofs_z + half) {
					child |= 4;
					ofs_z += half;
				}

				cell = bc->children[child];
				if (cell == RasterizerStorage::LightmapCaptureOctree::CHILD_EMPTY) {
					break;
				}

				half >>= 1;
			}

			if (cell != RasterizerStorage::LightmapCaptureOctree::CHILD_EMPTY) {
				alpha[c][n] = p_octree[cell].alpha;

				for (int i = 0; i < 6; i++) {
					//anisotropic read light
					float amount = p_dir.dot(aniso_normal[i]);

					if (amount > 0) {
						constexpr float ONE_1024TH = 1.0 / 1024.0;
						color[c][n].x += p_octree[cell].light[i][0] * ONE_1024TH * amount;
						color[c][n].y += p_octree[cell].light[i][1] * ONE_1024TH * amount;
						color[c][n].z += p_octree[cell].light[i][2] * ONE_1024TH * amount;
					}
				}
			}

			//print_line("\tlev " + itos(c) + " - " + itos(n) + " alpha: " + rtos(cells[test_cell].alpha) + " col: " + color[c][n]);
		}
	}

	float target_level_size = size >> target_level;
	Vector3 pos_fract[2];

	float target_level_size_inv = 1.0f / target_level_size;
	real_t res;
	res = pos.x * target_level_size_inv;
	pos_fract[0].x = res - (int)res;
	res = pos.y * target_level_size_inv;
	pos_fract[0].y = res - (int)res;
	res = pos.z * target_level_size_inv;
	pos_fract[0].z = res - (int)res;

	target_level_size = size >> MAX(0, target_level - 1);

	target_level_size_inv = 1.0f / target_level_size;
	res = pos.x * target_level_size_inv;
	pos_fract[1].x = res - (int)res;
	res = pos.y * target_level_size_inv;
	pos_fract[1].y = res - (int)res;
	res = pos.z * target_level_size_inv;
	pos_fract[1].z = res - (int)res;

	float alpha_interp[2];
	Vector3 color_interp[2];

	for (int i = 0; i < 2; i++) {
		Vector3 color_x00 = color[i][0].linear_interpolate(color[i][1], pos_fract[i].x);
		Vector3 color_xy0 = color[i][2].linear_interpolate(color[i][3], pos_fract[i].x);
		Vector3 blend_z0 = color_x00.linear_interpolate(color_xy0, pos_fract[i].y);

		Vector3 color_x0z = color[i][4].linear_interpolate(color[i][5], pos_fract[i].x);
		Vector3 color_xyz = color[i][6].linear_interpolate(color[i][7], pos_fract[i].x);
		Vector3 blend_z1 = color_x0z.linear_interpolate(color_xyz, pos_fract[i].y);

		color_interp[i] = blend_z0.linear_interpolate(blend_z1, pos_fract[i].z);

		float alpha_x00 = Math::lerp(alpha[i][0], alpha[i][1], pos_fract[i].x);
		float alpha_xy0 = Math::lerp(alpha[i][2], alpha[i][3], pos_fract[i].x);
		float alpha_z0 = Math::lerp(alpha_x00, alpha_xy0, pos_fract[i].y);

		float alpha_x0z = Math::lerp(alpha[i][4], alpha[i][5], pos_fract[i].x);
		float alpha_xyz = Math::lerp(alpha[i][6], alpha[i][7], pos_fract[i].x);
		float alpha_z1 = Math::lerp(alpha_x0z, alpha_xyz, pos_fract[i].y);

		alpha_interp[i] = Math::lerp(alpha_z0, alpha_z1, pos_fract[i].z);
	}

	r_color = color_interp[0].linear_interpolate(color_interp[1], level_filter);
	r_alpha = Math::lerp(alpha_interp[0], alpha_interp[1], level_filter);

	//print_line("pos: " + p_posf + " level " + rtos(p_level) + " down to " + itos(target_level) + "." + rtos(level_filter) + " color " + r_color + " alpha " + rtos(r_alpha));
}

_FORCE_INLINE_ static Color _light_capture_voxel_cone_trace(const RasterizerStorage::LightmapCaptureOctree *p_octree, const Vector3 &p_pos, const Vector3 &p_dir, float p_aperture, int p_cell_subdiv) {
	float bias = 0.0; //no need for bias here
	float max_distance = (Vector3(1, 1, 1) * (1 << (p_cell_subdiv - 1))).length();

	float dist = bias;
	float alpha = 0.0;
	Vector3 color;

	Vector3 scolor;
	float salpha;

	while (dist < max_distance && alpha < 0.95) {
		float diameter = MAX(1.0, 2.0 * p_aperture * dist);
		_light_capture_sample_octree(p_octree, p_cell_subdiv, p_pos + dist * p_dir, p_dir, log2(diameter), scolor, salpha);
		float a = (1.0 - alpha);
		color += scolor * a;
		alpha += a * salpha;
		dist += diameter * 0.5;
	}

	return Color(color.x, color.y, color.z, alpha);
}

void VisualServerScene::_update_instance_lightmap_captures(Instance *p_instance) {
	InstanceGeometryData *geom = static_cast<InstanceGeometryData *>(p_instance->base_data);

	static const Vector3 cone_traces[12] = {
		Vector3(0, 0, 1),
		Vector3(0.866025, 0, 0.5),
		Vector3(0.267617, 0.823639, 0.5),
		Vector3(-0.700629, 0.509037, 0.5),
		Vector3(-0.700629, -0.509037, 0.5),
		Vector3(0.267617, -0.823639, 0.5),
		Vector3(0, 0, -1),
		Vector3(0.866025, 0, -0.5),
		Vector3(0.267617, 0.823639, -0.5),
		Vector3(-0.700629, 0.509037, -0.5),
		Vector3(-0.700629, -0.509037, -0.5),
		Vector3(0.267617, -0.823639, -0.5)
	};

	float cone_aperture = 0.577; // tan(angle) 60 degrees

	if (p_instance->lightmap_capture_data.empty()) {
		p_instance->lightmap_capture_data.resize(12);
	}

	//print_line("update captures for pos: " + p_instance->transform.origin);

	for (int i = 0; i < 12; i++) {
		new (&p_instance->lightmap_capture_data.ptrw()[i]) Color;
	}

	bool interior = true;
	//this could use some sort of blending..
	for (List<Instance *>::Element *E = geom->lightmap_captures.front(); E; E = E->next()) {
		const PoolVector<RasterizerStorage::LightmapCaptureOctree> *octree = VSG::storage->lightmap_capture_get_octree_ptr(E->get()->base);
		//print_line("octree size: " + itos(octree->size()));
		if (octree->size() == 0) {
			continue;
		}
		Transform to_cell_xform = VSG::storage->lightmap_capture_get_octree_cell_transform(E->get()->base);
		int cell_subdiv = VSG::storage->lightmap_capture_get_octree_cell_subdiv(E->get()->base);
		to_cell_xform = to_cell_xform * E->get()->transform.affine_inverse();

		PoolVector<RasterizerStorage::LightmapCaptureOctree>::Read octree_r = octree->read();

		Vector3 pos = to_cell_xform.xform(p_instance->transform.origin);

		const float capture_energy = VSG::storage->lightmap_capture_get_energy(E->get()->base);
		interior = interior && VSG::storage->lightmap_capture_is_interior(E->get()->base);

		for (int i = 0; i < 12; i++) {
			Vector3 dir = to_cell_xform.basis.xform(cone_traces[i]).normalized();
			Color capture = _light_capture_voxel_cone_trace(octree_r.ptr(), pos, dir, cone_aperture, cell_subdiv);
			capture.r *= capture_energy;
			capture.g *= capture_energy;
			capture.b *= capture_energy;
			p_instance->lightmap_capture_data.write[i] += capture;
		}
	}
	p_instance->lightmap_capture_data.write[0].a = interior ? 0.0f : 1.0f;
}

bool VisualServerScene::_light_instance_update_shadow(Instance *p_instance, const Transform p_cam_transform, const CameraMatrix &p_cam_projection, bool p_cam_orthogonal, RID p_shadow_atlas, Scenario *p_scenario, uint32_t p_visible_layers) {
	InstanceLightData *light = static_cast<InstanceLightData *>(p_instance->base_data);

	Transform light_transform = p_instance->transform;
	light_transform.orthonormalize(); //scale does not count on lights

	bool animated_material_found = false;

	switch (VSG::storage->light_get_type(p_instance->base)) {
		case VS::LIGHT_DIRECTIONAL: {
			// Directional light always needs preparing as it takes a different path to other lights.
			light_culler->prepare_light(*p_instance);

			// Directional lights can always do a tighter cull.
			// This should occur because shadow_dirty_count is never decremented for directional lights.
#ifdef DEV_ENABLED
			DEV_CHECK_ONCE(!light->is_shadow_update_full());
#endif

			float max_distance = p_cam_projection.get_z_far();
			float shadow_max = VSG::storage->light_get_param(p_instance->base, VS::LIGHT_PARAM_SHADOW_MAX_DISTANCE);
			if (shadow_max > 0 && !p_cam_orthogonal) { //its impractical (and leads to unwanted behaviors) to set max distance in orthogonal camera
				max_distance = MIN(shadow_max, max_distance);
			}
			max_distance = MAX(max_distance, p_cam_projection.get_z_near() + 0.001);
			float min_distance = MIN(p_cam_projection.get_z_near(), max_distance);

			VS::LightDirectionalShadowDepthRangeMode depth_range_mode = VSG::storage->light_directional_get_shadow_depth_range_mode(p_instance->base);

			if (depth_range_mode == VS::LIGHT_DIRECTIONAL_SHADOW_DEPTH_RANGE_OPTIMIZED) {
				//optimize min/max
				Vector<Plane> planes = p_cam_projection.get_projection_planes(p_cam_transform);
				int cull_count = p_scenario->sps->cull_convex(planes, instance_shadow_cull_result, MAX_INSTANCE_CULL, VS::INSTANCE_GEOMETRY_MASK);
				Plane base(p_cam_transform.origin, -p_cam_transform.basis.get_axis(2));
				//check distance max and min

				bool found_items = false;
				float z_max = -1e20;
				float z_min = 1e20;

				for (int i = 0; i < cull_count; i++) {
					Instance *instance = instance_shadow_cull_result[i];
					if (!instance->visible || !((1 << instance->base_type) & VS::INSTANCE_GEOMETRY_MASK) || !static_cast<InstanceGeometryData *>(instance->base_data)->can_cast_shadows || !(p_visible_layers & instance->layer_mask)) {
						continue;
					}

					if (static_cast<InstanceGeometryData *>(instance->base_data)->material_is_animated) {
						animated_material_found = true;
					}

					float max, min;
					instance->transformed_aabb.project_range_in_plane(base, min, max);

					if (max > z_max) {
						z_max = max;
					}

					if (min < z_min) {
						z_min = min;
					}

					found_items = true;
				}

				if (found_items) {
					min_distance = MAX(min_distance, z_min);
					max_distance = MIN(max_distance, z_max);
				}
			}

			float range = max_distance - min_distance;

			int splits = 0;
			switch (VSG::storage->light_directional_get_shadow_mode(p_instance->base)) {
				case VS::LIGHT_DIRECTIONAL_SHADOW_ORTHOGONAL:
					splits = 1;
					break;
				case VS::LIGHT_DIRECTIONAL_SHADOW_PARALLEL_2_SPLITS:
					splits = 2;
					break;
				case VS::LIGHT_DIRECTIONAL_SHADOW_PARALLEL_3_SPLITS:
					splits = 3;
					break;
				case VS::LIGHT_DIRECTIONAL_SHADOW_PARALLEL_4_SPLITS:
					splits = 4;
					break;
			}

			float distances[5];

			distances[0] = min_distance;
			for (int i = 0; i < splits; i++) {
				distances[i + 1] = min_distance + VSG::storage->light_get_param(p_instance->base, VS::LightParam(VS::LIGHT_PARAM_SHADOW_SPLIT_1_OFFSET + i)) * range;
			};

			distances[splits] = max_distance;

			float texture_size = VSG::scene_render->get_directional_light_shadow_size(light->instance);

			bool overlap = VSG::storage->light_directional_get_blend_splits(p_instance->base);

			float first_radius = 0.0;

			for (int i = 0; i < splits; i++) {
				// setup a camera matrix for that range!
				CameraMatrix camera_matrix;

				float aspect = p_cam_projection.get_aspect();

				if (p_cam_orthogonal) {
					Vector2 vp_he = p_cam_projection.get_viewport_half_extents();

					camera_matrix.set_orthogonal(vp_he.y * 2.0, aspect, distances[(i == 0 || !overlap) ? i : i - 1], distances[i + 1], false);
				} else {
					float fov = p_cam_projection.get_fov();
					camera_matrix.set_perspective(fov, aspect, distances[(i == 0 || !overlap) ? i : i - 1], distances[i + 1], false);
				}

				//obtain the frustum endpoints

				Vector3 endpoints[8]; // frustum plane endpoints
				bool res = camera_matrix.get_endpoints(p_cam_transform, endpoints);
				ERR_CONTINUE(!res);

				// obtain the light frustm ranges (given endpoints)

				Transform transform = light_transform; //discard scale and stabilize light

				Vector3 x_vec = transform.basis.get_axis(Vector3::AXIS_X).normalized();
				Vector3 y_vec = transform.basis.get_axis(Vector3::AXIS_Y).normalized();
				Vector3 z_vec = transform.basis.get_axis(Vector3::AXIS_Z).normalized();
				//z_vec points agsint the camera, like in default opengl

				float x_min = 0.f, x_max = 0.f;
				float y_min = 0.f, y_max = 0.f;
				float z_min = 0.f, z_max = 0.f;

				// FIXME: z_max_cam is defined, computed, but not used below when setting up
				// ortho_camera. Commented out for now to fix warnings but should be investigated.
				float x_min_cam = 0.f, x_max_cam = 0.f;
				float y_min_cam = 0.f, y_max_cam = 0.f;
				float z_min_cam = 0.f;
				//float z_max_cam = 0.f;

				float bias_scale = 1.0;

				//used for culling

				for (int j = 0; j < 8; j++) {
					float d_x = x_vec.dot(endpoints[j]);
					float d_y = y_vec.dot(endpoints[j]);
					float d_z = z_vec.dot(endpoints[j]);

					if (j == 0 || d_x < x_min) {
						x_min = d_x;
					}
					if (j == 0 || d_x > x_max) {
						x_max = d_x;
					}

					if (j == 0 || d_y < y_min) {
						y_min = d_y;
					}
					if (j == 0 || d_y > y_max) {
						y_max = d_y;
					}

					if (j == 0 || d_z < z_min) {
						z_min = d_z;
					}
					if (j == 0 || d_z > z_max) {
						z_max = d_z;
					}
				}

				{
					//camera viewport stuff

					Vector3 center;

					for (int j = 0; j < 8; j++) {
						center += endpoints[j];
					}
					center /= 8.0;

					//center=x_vec*(x_max-x_min)*0.5 + y_vec*(y_max-y_min)*0.5 + z_vec*(z_max-z_min)*0.5;

					float radius = 0;

					for (int j = 0; j < 8; j++) {
						float d = center.distance_to(endpoints[j]);
						if (d > radius) {
							radius = d;
						}
					}

					radius *= texture_size / (texture_size - 2.0); //add a texel by each side

					if (i == 0) {
						first_radius = radius;
					} else {
						bias_scale = radius / first_radius;
					}

					x_max_cam = x_vec.dot(center) + radius;
					x_min_cam = x_vec.dot(center) - radius;
					y_max_cam = y_vec.dot(center) + radius;
					y_min_cam = y_vec.dot(center) - radius;
					//z_max_cam = z_vec.dot(center) + radius;
					z_min_cam = z_vec.dot(center) - radius;

					if (depth_range_mode == VS::LIGHT_DIRECTIONAL_SHADOW_DEPTH_RANGE_STABLE) {
						//this trick here is what stabilizes the shadow (make potential jaggies to not move)
						//at the cost of some wasted resolution. Still the quality increase is very well worth it

						float unit = radius * 2.0 / texture_size;

						x_max_cam = Math::stepify(x_max_cam, unit);
						x_min_cam = Math::stepify(x_min_cam, unit);
						y_max_cam = Math::stepify(y_max_cam, unit);
						y_min_cam = Math::stepify(y_min_cam, unit);
					}
				}

				//now that we now all ranges, we can proceed to make the light frustum planes, for culling octree

				Vector<Plane> light_frustum_planes;
				light_frustum_planes.resize(6);

				//right/left
				light_frustum_planes.write[0] = Plane(x_vec, x_max);
				light_frustum_planes.write[1] = Plane(-x_vec, -x_min);
				//top/bottom
				light_frustum_planes.write[2] = Plane(y_vec, y_max);
				light_frustum_planes.write[3] = Plane(-y_vec, -y_min);
				//near/far
				light_frustum_planes.write[4] = Plane(z_vec, z_max + 1e6);
				light_frustum_planes.write[5] = Plane(-z_vec, -z_min); // z_min is ok, since casters further than far-light plane are not needed

				int cull_count = p_scenario->sps->cull_convex(light_frustum_planes, instance_shadow_cull_result, MAX_INSTANCE_CULL, VS::INSTANCE_GEOMETRY_MASK);

				// a pre pass will need to be needed to determine the actual z-near to be used

				Plane near_plane(light_transform.origin, -light_transform.basis.get_axis(2));

				for (int j = 0; j < cull_count; j++) {
					float min, max;
					Instance *instance = instance_shadow_cull_result[j];
					if (!instance->visible || !((1 << instance->base_type) & VS::INSTANCE_GEOMETRY_MASK) || !static_cast<InstanceGeometryData *>(instance->base_data)->can_cast_shadows || !(p_visible_layers & instance->layer_mask)) {
						cull_count--;
						SWAP(instance_shadow_cull_result[j], instance_shadow_cull_result[cull_count]);
						j--;
						continue;
					}

					instance->transformed_aabb.project_range_in_plane(Plane(z_vec, 0), min, max);
					instance->depth = near_plane.distance_to(instance->transform.origin);
					instance->depth_layer = 0;
					if (max > z_max) {
						z_max = max;
					}
				}

				{
					CameraMatrix ortho_camera;
					real_t half_x = (x_max_cam - x_min_cam) * 0.5;
					real_t half_y = (y_max_cam - y_min_cam) * 0.5;

					ortho_camera.set_orthogonal(-half_x, half_x, -half_y, half_y, 0, (z_max - z_min_cam));

					Transform ortho_transform;
					ortho_transform.basis = transform.basis;
					ortho_transform.origin = x_vec * (x_min_cam + half_x) + y_vec * (y_min_cam + half_y) + z_vec * z_max;

					VSG::scene_render->light_instance_set_shadow_transform(light->instance, ortho_camera, ortho_transform, 0, distances[i + 1], i, bias_scale);
				}

				// Do a secondary cull to remove casters that don't intersect with the camera frustum.
				// Note this could possibly be done in a more efficient place if we can share the cull results for each split.
				cull_count = light_culler->cull(cull_count, instance_shadow_cull_result);

				VSG::scene_render->render_shadow(light->instance, p_shadow_atlas, i, (RasterizerScene::InstanceBase **)instance_shadow_cull_result, cull_count);
			}

		} break;
		case VS::LIGHT_OMNI: {
			VS::LightOmniShadowMode shadow_mode = VSG::storage->light_omni_get_shadow_mode(p_instance->base);

			if (shadow_mode == VS::LIGHT_OMNI_SHADOW_DUAL_PARABOLOID || !VSG::scene_render->light_instances_can_render_shadow_cube()) {
				for (int i = 0; i < 2; i++) {
					//using this one ensures that raster deferred will have it

					float radius = VSG::storage->light_get_param(p_instance->base, VS::LIGHT_PARAM_RANGE);

					float z = i == 0 ? -1 : 1;
					Vector<Plane> planes;
					planes.resize(6);
					planes.write[0] = light_transform.xform(Plane(Vector3(0, 0, z), radius));
					planes.write[1] = light_transform.xform(Plane(Vector3(1, 0, z).normalized(), radius));
					planes.write[2] = light_transform.xform(Plane(Vector3(-1, 0, z).normalized(), radius));
					planes.write[3] = light_transform.xform(Plane(Vector3(0, 1, z).normalized(), radius));
					planes.write[4] = light_transform.xform(Plane(Vector3(0, -1, z).normalized(), radius));
					planes.write[5] = light_transform.xform(Plane(Vector3(0, 0, -z), 0));

					int cull_count = p_scenario->sps->cull_convex(planes, instance_shadow_cull_result, MAX_INSTANCE_CULL, VS::INSTANCE_GEOMETRY_MASK);

					// Do a secondary cull to remove casters that don't intersect with the camera frustum.
					if (!light->is_shadow_update_full()) {
						cull_count = light_culler->cull(cull_count, instance_shadow_cull_result);
					}

					Plane near_plane(light_transform.origin, light_transform.basis.get_axis(2) * z);

					for (int j = 0; j < cull_count; j++) {
						Instance *instance = instance_shadow_cull_result[j];
						if (!instance->visible || !((1 << instance->base_type) & VS::INSTANCE_GEOMETRY_MASK) || !static_cast<InstanceGeometryData *>(instance->base_data)->can_cast_shadows || !(p_visible_layers & instance->layer_mask)) {
							cull_count--;
							SWAP(instance_shadow_cull_result[j], instance_shadow_cull_result[cull_count]);
							j--;
						} else {
							if (static_cast<InstanceGeometryData *>(instance->base_data)->material_is_animated) {
								animated_material_found = true;
							}

							instance->depth = near_plane.distance_to(instance->transform.origin);
							instance->depth_layer = 0;
						}
					}

					VSG::scene_render->light_instance_set_shadow_transform(light->instance, CameraMatrix(), light_transform, radius, 0, i);
					VSG::scene_render->render_shadow(light->instance, p_shadow_atlas, i, (RasterizerScene::InstanceBase **)instance_shadow_cull_result, cull_count);
				}
			} else { //shadow cube

				float radius = VSG::storage->light_get_param(p_instance->base, VS::LIGHT_PARAM_RANGE);
				CameraMatrix cm;
				cm.set_perspective(90, 1, 0.01, radius);

				for (int i = 0; i < 6; i++) {
					//using this one ensures that raster deferred will have it

					static const Vector3 view_normals[6] = {
						Vector3(-1, 0, 0),
						Vector3(+1, 0, 0),
						Vector3(0, -1, 0),
						Vector3(0, +1, 0),
						Vector3(0, 0, -1),
						Vector3(0, 0, +1)
					};
					static const Vector3 view_up[6] = {
						Vector3(0, -1, 0),
						Vector3(0, -1, 0),
						Vector3(0, 0, -1),
						Vector3(0, 0, +1),
						Vector3(0, -1, 0),
						Vector3(0, -1, 0)
					};

					Transform xform = light_transform * Transform().looking_at(view_normals[i], view_up[i]);

					Vector<Plane> planes = cm.get_projection_planes(xform);

					int cull_count = _cull_convex_from_point(p_scenario, light_transform, cm, planes, instance_shadow_cull_result, MAX_INSTANCE_CULL, light->previous_room_id_hint, VS::INSTANCE_GEOMETRY_MASK);

					// Do a secondary cull to remove casters that don't intersect with the camera frustum.
					if (!light->is_shadow_update_full()) {
						cull_count = light_culler->cull(cull_count, instance_shadow_cull_result);
					}

					Plane near_plane(xform.origin, -xform.basis.get_axis(2));
					for (int j = 0; j < cull_count; j++) {
						Instance *instance = instance_shadow_cull_result[j];
						if (!instance->visible || !((1 << instance->base_type) & VS::INSTANCE_GEOMETRY_MASK) || !static_cast<InstanceGeometryData *>(instance->base_data)->can_cast_shadows || !(p_visible_layers & instance->layer_mask)) {
							cull_count--;
							SWAP(instance_shadow_cull_result[j], instance_shadow_cull_result[cull_count]);
							j--;
						} else {
							if (static_cast<InstanceGeometryData *>(instance->base_data)->material_is_animated) {
								animated_material_found = true;
							}
							instance->depth = near_plane.distance_to(instance->transform.origin);
							instance->depth_layer = 0;
						}
					}

					VSG::scene_render->light_instance_set_shadow_transform(light->instance, cm, xform, radius, 0, i);
					VSG::scene_render->render_shadow(light->instance, p_shadow_atlas, i, (RasterizerScene::InstanceBase **)instance_shadow_cull_result, cull_count);
				}

				//restore the regular DP matrix
				VSG::scene_render->light_instance_set_shadow_transform(light->instance, CameraMatrix(), light_transform, radius, 0, 0);
			}

		} break;
		case VS::LIGHT_SPOT: {
			float radius = VSG::storage->light_get_param(p_instance->base, VS::LIGHT_PARAM_RANGE);
			float angle = VSG::storage->light_get_param(p_instance->base, VS::LIGHT_PARAM_SPOT_ANGLE);

			CameraMatrix cm;
			cm.set_perspective(angle * 2.0, 1.0, 0.01, radius);

			Vector<Plane> planes = cm.get_projection_planes(light_transform);
			int cull_count = _cull_convex_from_point(p_scenario, light_transform, cm, planes, instance_shadow_cull_result, MAX_INSTANCE_CULL, light->previous_room_id_hint, VS::INSTANCE_GEOMETRY_MASK);

			// Do a secondary cull to remove casters that don't intersect with the camera frustum.
			if (!light->is_shadow_update_full()) {
				cull_count = light_culler->cull(cull_count, instance_shadow_cull_result);
			}

			Plane near_plane(light_transform.origin, -light_transform.basis.get_axis(2));
			for (int j = 0; j < cull_count; j++) {
				Instance *instance = instance_shadow_cull_result[j];
				if (!instance->visible || !((1 << instance->base_type) & VS::INSTANCE_GEOMETRY_MASK) || !static_cast<InstanceGeometryData *>(instance->base_data)->can_cast_shadows || !(p_visible_layers & instance->layer_mask)) {
					cull_count--;
					SWAP(instance_shadow_cull_result[j], instance_shadow_cull_result[cull_count]);
					j--;
				} else {
					if (static_cast<InstanceGeometryData *>(instance->base_data)->material_is_animated) {
						animated_material_found = true;
					}
					instance->depth = near_plane.distance_to(instance->transform.origin);
					instance->depth_layer = 0;
				}
			}

			VSG::scene_render->light_instance_set_shadow_transform(light->instance, cm, light_transform, radius, 0, 0);
			VSG::scene_render->render_shadow(light->instance, p_shadow_atlas, 0, (RasterizerScene::InstanceBase **)instance_shadow_cull_result, cull_count);

		} break;
	}

	return animated_material_found;
}

void VisualServerScene::render_camera(RID p_camera, RID p_scenario, Size2 p_viewport_size, RID p_shadow_atlas) {
// render to mono camera
#ifndef _3D_DISABLED

	Camera *camera = camera_owner.getornull(p_camera);
	ERR_FAIL_COND(!camera);

	/* STEP 1 - SETUP CAMERA */
	CameraMatrix camera_matrix;
	bool ortho = false;

	switch (camera->type) {
		case Camera::ORTHOGONAL: {
			camera_matrix.set_orthogonal(
					camera->size,
					p_viewport_size.width / (float)p_viewport_size.height,
					camera->znear,
					camera->zfar,
					camera->vaspect);
			ortho = true;
		} break;
		case Camera::PERSPECTIVE: {
			camera_matrix.set_perspective(
					camera->fov,
					p_viewport_size.width / (float)p_viewport_size.height,
					camera->znear,
					camera->zfar,
					camera->vaspect);
			ortho = false;

		} break;
		case Camera::FRUSTUM: {
			camera_matrix.set_frustum(
					camera->size,
					p_viewport_size.width / (float)p_viewport_size.height,
					camera->offset,
					camera->znear,
					camera->zfar,
					camera->vaspect);
			ortho = false;
		} break;
	}

	_prepare_scene(camera->transform, camera_matrix, ortho, camera->env, camera->visible_layers, p_scenario, p_shadow_atlas, RID(), camera->previous_room_id_hint);
	_render_scene(camera->transform, camera_matrix, 0, ortho, camera->env, p_scenario, p_shadow_atlas, RID(), -1);
#endif
}

void VisualServerScene::render_camera(Ref<ARVRInterface> &p_interface, ARVRInterface::Eyes p_eye, RID p_camera, RID p_scenario, Size2 p_viewport_size, RID p_shadow_atlas) {
	// render for AR/VR interface

	Camera *camera = camera_owner.getornull(p_camera);
	ERR_FAIL_COND(!camera);

	/* SETUP CAMERA, we are ignoring type and FOV here */
	float aspect = p_viewport_size.width / (float)p_viewport_size.height;
	CameraMatrix camera_matrix = p_interface->get_projection_for_eye(p_eye, aspect, camera->znear, camera->zfar);

	// We also ignore our camera position, it will have been positioned with a slightly old tracking position.
	// Instead we take our origin point and have our ar/vr interface add fresh tracking data! Whoohoo!
	Transform world_origin = ARVRServer::get_singleton()->get_world_origin();
	Transform cam_transform = p_interface->get_transform_for_eye(p_eye, world_origin);

	// For stereo render we only prepare for our left eye and then reuse the outcome for our right eye
	if (p_eye == ARVRInterface::EYE_LEFT) {
		///@TODO possibly move responsibility for this into our ARVRServer or ARVRInterface?

		// Center our transform, we assume basis is equal.
		Transform mono_transform = cam_transform;
		Transform right_transform = p_interface->get_transform_for_eye(ARVRInterface::EYE_RIGHT, world_origin);
		mono_transform.origin += right_transform.origin;
		mono_transform.origin *= 0.5;

		// We need to combine our projection frustums for culling.
		// Ideally we should use our clipping planes for this and combine them,
		// however our shadow map logic uses our projection matrix.
		// Note: as our left and right frustums should be mirrored, we don't need our right projection matrix.

		// - get some base values we need
		float eye_dist = (mono_transform.origin - cam_transform.origin).length();
		float z_near = camera_matrix.get_z_near(); // get our near plane
		float z_far = camera_matrix.get_z_far(); // get our far plane
		float width = (2.0 * z_near) / camera_matrix.matrix[0][0];
		float x_shift = width * camera_matrix.matrix[2][0];
		float height = (2.0 * z_near) / camera_matrix.matrix[1][1];
		float y_shift = height * camera_matrix.matrix[2][1];

		// printf("Eye_dist = %f, Near = %f, Far = %f, Width = %f, Shift = %f\n", eye_dist, z_near, z_far, width, x_shift);

		// - calculate our near plane size (horizontal only, right_near is mirrored)
		float left_near = -eye_dist - ((width - x_shift) * 0.5);

		// - calculate our far plane size (horizontal only, right_far is mirrored)
		float left_far = -eye_dist - (z_far * (width - x_shift) * 0.5 / z_near);
		float left_far_right_eye = eye_dist - (z_far * (width + x_shift) * 0.5 / z_near);
		if (left_far > left_far_right_eye) {
			// on displays smaller then double our iod, the right eye far frustrum can overtake the left eyes.
			left_far = left_far_right_eye;
		}

		// - figure out required z-shift
		float slope = (left_far - left_near) / (z_far - z_near);
		float z_shift = (left_near / slope) - z_near;

		// - figure out new vertical near plane size (this will be slightly oversized thanks to our z-shift)
		float top_near = (height - y_shift) * 0.5;
		top_near += (top_near / z_near) * z_shift;
		float bottom_near = -(height + y_shift) * 0.5;
		bottom_near += (bottom_near / z_near) * z_shift;

		// printf("Left_near = %f, Left_far = %f, Top_near = %f, Bottom_near = %f, Z_shift = %f\n", left_near, left_far, top_near, bottom_near, z_shift);

		// - generate our frustum
		CameraMatrix combined_matrix;
		combined_matrix.set_frustum(left_near, -left_near, bottom_near, top_near, z_near + z_shift, z_far + z_shift);

		// and finally move our camera back
		Transform apply_z_shift;
		apply_z_shift.origin = Vector3(0.0, 0.0, z_shift); // z negative is forward so this moves it backwards
		mono_transform *= apply_z_shift;

		// now prepare our scene with our adjusted transform projection matrix
		_prepare_scene(mono_transform, combined_matrix, false, camera->env, camera->visible_layers, p_scenario, p_shadow_atlas, RID(), camera->previous_room_id_hint);
	} else if (p_eye == ARVRInterface::EYE_MONO) {
		// For mono render, prepare as per usual
		_prepare_scene(cam_transform, camera_matrix, false, camera->env, camera->visible_layers, p_scenario, p_shadow_atlas, RID(), camera->previous_room_id_hint);
	}

	// And render our scene...
	_render_scene(cam_transform, camera_matrix, p_eye, false, camera->env, p_scenario, p_shadow_atlas, RID(), -1);
};

void VisualServerScene::_prepare_scene(const Transform p_cam_transform, const CameraMatrix &p_cam_projection, bool p_cam_orthogonal, RID p_force_environment, uint32_t p_visible_layers, RID p_scenario, RID p_shadow_atlas, RID p_reflection_probe, int32_t &r_previous_room_id_hint) {
	// Prepare the light - camera volume culling system.
	light_culler->prepare_camera(p_cam_transform, p_cam_projection);

	// Note, in stereo rendering:
	// - p_cam_transform will be a transform in the middle of our two eyes
	// - p_cam_projection is a wider frustrum that encompasses both eyes

	Scenario *scenario = scenario_owner.getornull(p_scenario);

	render_pass++;
	uint32_t camera_layer_mask = p_visible_layers;

	VSG::scene_render->set_scene_pass(render_pass);

	//rasterizer->set_camera(camera->transform, camera_matrix,ortho);

	Vector<Plane> planes = p_cam_projection.get_projection_planes(p_cam_transform);

	Plane near_plane(p_cam_transform.origin, -p_cam_transform.basis.get_axis(2).normalized());
	float z_far = p_cam_projection.get_z_far();

	/* STEP 2 - CULL */
	instance_cull_count = _cull_convex_from_point(scenario, p_cam_transform, p_cam_projection, planes, instance_cull_result, MAX_INSTANCE_CULL, r_previous_room_id_hint);
	light_cull_count = 0;

	reflection_probe_cull_count = 0;

	//light_samplers_culled=0;

	/*
	print_line("OT: "+rtos( (OS::get_singleton()->get_ticks_usec()-t)/1000.0));
	print_line("OTO: "+itos(p_scenario->octree.get_octant_count()));
	print_line("OTE: "+itos(p_scenario->octree.get_elem_count()));
	print_line("OTP: "+itos(p_scenario->octree.get_pair_count()));
	*/

	/* STEP 3 - PROCESS PORTALS, VALIDATE ROOMS */
	//removed, will replace with culling

	/* STEP 4 - REMOVE FURTHER CULLED OBJECTS, ADD LIGHTS */

	for (int i = 0; i < instance_cull_count; i++) {
		Instance *ins = instance_cull_result[i];

		bool keep = false;

		if ((camera_layer_mask & ins->layer_mask) == 0) {
			//failure
		} else if (ins->base_type == VS::INSTANCE_LIGHT && ins->visible) {
			if (light_cull_count < MAX_LIGHTS_CULLED) {
				InstanceLightData *light = static_cast<InstanceLightData *>(ins->base_data);

				if (!light->geometries.empty()) {
					//do not add this light if no geometry is affected by it..
					light_cull_result[light_cull_count] = ins;
					light_instance_cull_result[light_cull_count] = light->instance;
					if (p_shadow_atlas.is_valid() && VSG::storage->light_has_shadow(ins->base)) {
						VSG::scene_render->light_instance_mark_visible(light->instance); //mark it visible for shadow allocation later
					}

					light_cull_count++;
				}
			}
		} else if (ins->base_type == VS::INSTANCE_REFLECTION_PROBE && ins->visible) {
			if (reflection_probe_cull_count < MAX_REFLECTION_PROBES_CULLED) {
				InstanceReflectionProbeData *reflection_probe = static_cast<InstanceReflectionProbeData *>(ins->base_data);

				if (p_reflection_probe != reflection_probe->instance) {
					//avoid entering The Matrix

					if (!reflection_probe->geometries.empty()) {
						//do not add this light if no geometry is affected by it..

						if (reflection_probe->reflection_dirty || VSG::scene_render->reflection_probe_instance_needs_redraw(reflection_probe->instance)) {
							if (!reflection_probe->update_list.in_list()) {
								reflection_probe->render_step = 0;
								reflection_probe_render_list.add_last(&reflection_probe->update_list);
							}

							reflection_probe->reflection_dirty = false;
						}

						if (VSG::scene_render->reflection_probe_instance_has_reflection(reflection_probe->instance)) {
							reflection_probe_instance_cull_result[reflection_probe_cull_count] = reflection_probe->instance;
							reflection_probe_cull_count++;
						}
					}
				}
			}

		} else if (ins->base_type == VS::INSTANCE_GI_PROBE && ins->visible) {
			InstanceGIProbeData *gi_probe = static_cast<InstanceGIProbeData *>(ins->base_data);
			if (!gi_probe->update_element.in_list()) {
				gi_probe_update_list.add(&gi_probe->update_element);
			}

		} else if (((1 << ins->base_type) & VS::INSTANCE_GEOMETRY_MASK) && ins->visible && ins->cast_shadows != VS::SHADOW_CASTING_SETTING_SHADOWS_ONLY) {
			keep = true;

			InstanceGeometryData *geom = static_cast<InstanceGeometryData *>(ins->base_data);

			if (ins->redraw_if_visible) {
				VisualServerRaster::redraw_request(false);
			}

			if (ins->base_type == VS::INSTANCE_PARTICLES) {
				//particles visible? process them
				if (VSG::storage->particles_is_inactive(ins->base)) {
					//but if nothing is going on, don't do it.
					keep = false;
				} else {
					if (OS::get_singleton()->is_update_pending(true)) {
						VSG::storage->particles_request_process(ins->base);
						//particles visible? request redraw
						VisualServerRaster::redraw_request(false);
					}
				}
			}

			if (geom->lighting_dirty) {
				int l = 0;
				//only called when lights AABB enter/exit this geometry
				ins->light_instances.resize(geom->lighting.size());

				for (List<Instance *>::Element *E = geom->lighting.front(); E; E = E->next()) {
					InstanceLightData *light = static_cast<InstanceLightData *>(E->get()->base_data);

					ins->light_instances.write[l++] = light->instance;
				}

				geom->lighting_dirty = false;
			}

			if (geom->reflection_dirty) {
				int l = 0;
				//only called when reflection probe AABB enter/exit this geometry
				ins->reflection_probe_instances.resize(geom->reflection_probes.size());

				for (List<Instance *>::Element *E = geom->reflection_probes.front(); E; E = E->next()) {
					InstanceReflectionProbeData *reflection_probe = static_cast<InstanceReflectionProbeData *>(E->get()->base_data);

					ins->reflection_probe_instances.write[l++] = reflection_probe->instance;
				}

				geom->reflection_dirty = false;
			}

			if (geom->gi_probes_dirty) {
				int l = 0;
				//only called when reflection probe AABB enter/exit this geometry
				ins->gi_probe_instances.resize(geom->gi_probes.size());

				for (List<Instance *>::Element *E = geom->gi_probes.front(); E; E = E->next()) {
					InstanceGIProbeData *gi_probe = static_cast<InstanceGIProbeData *>(E->get()->base_data);

					ins->gi_probe_instances.write[l++] = gi_probe->probe_instance;
				}

				geom->gi_probes_dirty = false;
			}
		}

		if (!keep) {
			// remove, no reason to keep
			instance_cull_count--;
			SWAP(instance_cull_result[i], instance_cull_result[instance_cull_count]);
			i--;
			ins->last_render_pass = 0; // make invalid
		} else {
			ins->last_render_pass = render_pass;
		}
	}

	/* STEP 5 - PROCESS LIGHTS */

	RID *directional_light_ptr = &light_instance_cull_result[light_cull_count];
	directional_light_count = 0;

	// directional lights
	{
		Instance **lights_with_shadow = (Instance **)alloca(sizeof(Instance *) * scenario->directional_lights.size());
		int directional_shadow_count = 0;

		for (List<Instance *>::Element *E = scenario->directional_lights.front(); E; E = E->next()) {
			if (light_cull_count + directional_light_count >= MAX_LIGHTS_CULLED) {
				break;
			}

			if (!E->get()->visible) {
				continue;
			}

			InstanceLightData *light = static_cast<InstanceLightData *>(E->get()->base_data);

			//check shadow..

			if (light) {
				if (p_shadow_atlas.is_valid() && VSG::storage->light_has_shadow(E->get()->base)) {
					lights_with_shadow[directional_shadow_count++] = E->get();
				}
				//add to list
				directional_light_ptr[directional_light_count++] = light->instance;
			}
		}

		VSG::scene_render->set_directional_shadow_count(directional_shadow_count);

		for (int i = 0; i < directional_shadow_count; i++) {
			_light_instance_update_shadow(lights_with_shadow[i], p_cam_transform, p_cam_projection, p_cam_orthogonal, p_shadow_atlas, scenario, p_visible_layers);
		}
	}

	{ //setup shadow maps

		//SortArray<Instance*,_InstanceLightsort> sorter;
		//sorter.sort(light_cull_result,light_cull_count);
		for (int i = 0; i < light_cull_count; i++) {
			Instance *ins = light_cull_result[i];

			if (!p_shadow_atlas.is_valid() || !VSG::storage->light_has_shadow(ins->base)) {
				continue;
			}

			InstanceLightData *light = static_cast<InstanceLightData *>(ins->base_data);

			float coverage = 0.f;

			{ //compute coverage

				Transform cam_xf = p_cam_transform;
				float zn = p_cam_projection.get_z_near();
				Plane p(cam_xf.origin + cam_xf.basis.get_axis(2) * -zn, -cam_xf.basis.get_axis(2)); //camera near plane

				// near plane half width and height
				Vector2 vp_half_extents = p_cam_projection.get_viewport_half_extents();

				switch (VSG::storage->light_get_type(ins->base)) {
					case VS::LIGHT_OMNI: {
						float radius = VSG::storage->light_get_param(ins->base, VS::LIGHT_PARAM_RANGE);

						//get two points parallel to near plane
						Vector3 points[2] = {
							ins->transform.origin,
							ins->transform.origin + cam_xf.basis.get_axis(0) * radius
						};

						if (!p_cam_orthogonal) {
							//if using perspetive, map them to near plane
							for (int j = 0; j < 2; j++) {
								if (p.distance_to(points[j]) < 0) {
									points[j].z = -zn; //small hack to keep size constant when hitting the screen
								}

								p.intersects_segment(cam_xf.origin, points[j], &points[j]); //map to plane
							}
						}

						float screen_diameter = points[0].distance_to(points[1]) * 2;
						coverage = screen_diameter / (vp_half_extents.x + vp_half_extents.y);
					} break;
					case VS::LIGHT_SPOT: {
						float radius = VSG::storage->light_get_param(ins->base, VS::LIGHT_PARAM_RANGE);
						float angle = VSG::storage->light_get_param(ins->base, VS::LIGHT_PARAM_SPOT_ANGLE);

						float w = radius * Math::sin(Math::deg2rad(angle));
						float d = radius * Math::cos(Math::deg2rad(angle));

						Vector3 base = ins->transform.origin - ins->transform.basis.get_axis(2).normalized() * d;

						Vector3 points[2] = {
							base,
							base + cam_xf.basis.get_axis(0) * w
						};

						if (!p_cam_orthogonal) {
							//if using perspetive, map them to near plane
							for (int j = 0; j < 2; j++) {
								if (p.distance_to(points[j]) < 0) {
									points[j].z = -zn; //small hack to keep size constant when hitting the screen
								}

								p.intersects_segment(cam_xf.origin, points[j], &points[j]); //map to plane
							}
						}

						float screen_diameter = points[0].distance_to(points[1]) * 2;
						coverage = screen_diameter / (vp_half_extents.x + vp_half_extents.y);

					} break;
					default: {
						ERR_PRINT("Invalid Light Type");
					}
				}
			}

			// We can detect whether multiple cameras are hitting this light, whether or not the shadow is dirty,
			// so that we can turn off tighter caster culling.
			light->detect_light_intersects_multiple_cameras(Engine::get_singleton()->get_frames_drawn());

			if (light->is_shadow_dirty()) {
				// Dirty shadows have no need to be drawn if
				// the light volume doesn't intersect the camera frustum.

				// Returns false if the entire light can be culled.
				bool allow_redraw = light_culler->prepare_light(*ins);

				// Directional lights aren't handled here, _light_instance_update_shadow is called from elsewhere.
				// Checking for this in case this changes, as this is assumed.
				DEV_CHECK_ONCE(VSG::storage->light_get_type(ins->base) != VS::LIGHT_DIRECTIONAL);

				// Tighter caster culling to the camera frustum should work correctly with multiple viewports + cameras.
				// The first camera will cull tightly, but if the light is present on more than 1 camera, the second will
				// do a full render, and mark the light as non-dirty.
				// There is however a cost to tighter shadow culling in this situation (2 shadow updates in 1 frame),
				// so we should detect this and switch off tighter caster culling automatically.
				// This is done in the logic for `decrement_shadow_dirty()`.
				if (allow_redraw) {
					light->last_version++;
					light->decrement_shadow_dirty();
				}
			}

			bool redraw = VSG::scene_render->shadow_atlas_update_light(p_shadow_atlas, light->instance, coverage, light->last_version);

			if (redraw) {
				//must redraw!
				if (_light_instance_update_shadow(ins, p_cam_transform, p_cam_projection, p_cam_orthogonal, p_shadow_atlas, scenario, p_visible_layers)) {
					// If the light requests another update (animated material?)...
					light->make_shadow_dirty();
				}
			}
		}
	}

	// Calculate instance->depth from the camera, after shadow calculation has stopped overwriting instance->depth
	for (int i = 0; i < instance_cull_count; i++) {
		Instance *ins = instance_cull_result[i];

		if (((1 << ins->base_type) & VS::INSTANCE_GEOMETRY_MASK) && ins->visible && ins->cast_shadows != VS::SHADOW_CASTING_SETTING_SHADOWS_ONLY) {
			Vector3 center = ins->transform.origin;
			if (ins->use_aabb_center) {
				center = ins->transformed_aabb.position + (ins->transformed_aabb.size * 0.5);
			}
			if (p_cam_orthogonal) {
				ins->depth = near_plane.distance_to(center) - ins->sorting_offset;
			} else {
				ins->depth = p_cam_transform.origin.distance_to(center) - ins->sorting_offset;
			}
			ins->depth_layer = CLAMP(int(ins->depth * 16 / z_far), 0, 15);
		}
	}
}

void VisualServerScene::_render_scene(const Transform p_cam_transform, const CameraMatrix &p_cam_projection, const int p_eye, bool p_cam_orthogonal, RID p_force_environment, RID p_scenario, RID p_shadow_atlas, RID p_reflection_probe, int p_reflection_probe_pass) {
	Scenario *scenario = scenario_owner.getornull(p_scenario);

	/* ENVIRONMENT */

	RID environment;
	if (p_force_environment.is_valid()) { //camera has more environment priority
		environment = p_force_environment;
	} else if (scenario->environment.is_valid()) {
		environment = scenario->environment;
	} else {
		environment = scenario->fallback_environment;
	}

	/* PROCESS GEOMETRY AND DRAW SCENE */

	VSG::scene_render->render_scene(p_cam_transform, p_cam_projection, p_eye, p_cam_orthogonal, (RasterizerScene::InstanceBase **)instance_cull_result, instance_cull_count, light_instance_cull_result, light_cull_count + directional_light_count, reflection_probe_instance_cull_result, reflection_probe_cull_count, environment, p_shadow_atlas, scenario->reflection_atlas, p_reflection_probe, p_reflection_probe_pass);
}

void VisualServerScene::render_empty_scene(RID p_scenario, RID p_shadow_atlas) {
#ifndef _3D_DISABLED

	Scenario *scenario = scenario_owner.getornull(p_scenario);

	RID environment;
	if (scenario->environment.is_valid()) {
		environment = scenario->environment;
	} else {
		environment = scenario->fallback_environment;
	}
	VSG::scene_render->render_scene(Transform(), CameraMatrix(), 0, true, nullptr, 0, nullptr, 0, nullptr, 0, environment, p_shadow_atlas, scenario->reflection_atlas, RID(), 0);
#endif
}

bool VisualServerScene::_render_reflection_probe_step(Instance *p_instance, int p_step) {
	InstanceReflectionProbeData *reflection_probe = static_cast<InstanceReflectionProbeData *>(p_instance->base_data);
	Scenario *scenario = p_instance->scenario;
	ERR_FAIL_COND_V(!scenario, true);

	VisualServerRaster::redraw_request(false); //update, so it updates in editor

	if (p_step == 0) {
		if (!VSG::scene_render->reflection_probe_instance_begin_render(reflection_probe->instance, scenario->reflection_atlas)) {
			return true; //sorry, all full :(
		}
	}

	if (p_step >= 0 && p_step < 6) {
		static const Vector3 view_normals[6] = {
			Vector3(-1, 0, 0),
			Vector3(+1, 0, 0),
			Vector3(0, -1, 0),
			Vector3(0, +1, 0),
			Vector3(0, 0, -1),
			Vector3(0, 0, +1)
		};

		Vector3 extents = VSG::storage->reflection_probe_get_extents(p_instance->base);
		Vector3 origin_offset = VSG::storage->reflection_probe_get_origin_offset(p_instance->base);
		float max_distance = VSG::storage->reflection_probe_get_origin_max_distance(p_instance->base);

		Vector3 edge = view_normals[p_step] * extents;
		float distance = ABS(view_normals[p_step].dot(edge) - view_normals[p_step].dot(origin_offset)); //distance from origin offset to actual view distance limit

		max_distance = MAX(max_distance, distance);

		//render cubemap side
		CameraMatrix cm;
		cm.set_perspective(90, 1, 0.01, max_distance);

		static const Vector3 view_up[6] = {
			Vector3(0, -1, 0),
			Vector3(0, -1, 0),
			Vector3(0, 0, -1),
			Vector3(0, 0, +1),
			Vector3(0, -1, 0),
			Vector3(0, -1, 0)
		};

		Transform local_view;
		local_view.set_look_at(origin_offset, origin_offset + view_normals[p_step], view_up[p_step]);

		Transform xform = p_instance->transform * local_view;

		RID shadow_atlas;

		if (VSG::storage->reflection_probe_renders_shadows(p_instance->base)) {
			shadow_atlas = scenario->reflection_probe_shadow_atlas;
		}

		_prepare_scene(xform, cm, false, RID(), VSG::storage->reflection_probe_get_cull_mask(p_instance->base), p_instance->scenario->self, shadow_atlas, reflection_probe->instance, reflection_probe->previous_room_id_hint);

		bool async_forbidden_backup = VSG::storage->is_shader_async_hidden_forbidden();
		VSG::storage->set_shader_async_hidden_forbidden(true);
		_render_scene(xform, cm, 0, false, RID(), p_instance->scenario->self, shadow_atlas, reflection_probe->instance, p_step);
		VSG::storage->set_shader_async_hidden_forbidden(async_forbidden_backup);

	} else {
		//do roughness postprocess step until it believes it's done
		return VSG::scene_render->reflection_probe_instance_postprocess_step(reflection_probe->instance);
	}

	return false;
}

void VisualServerScene::_gi_probe_fill_local_data(int p_idx, int p_level, int p_x, int p_y, int p_z, const GIProbeDataCell *p_cell, const GIProbeDataHeader *p_header, InstanceGIProbeData::LocalData *p_local_data, Vector<uint32_t> *prev_cell) {
	if ((uint32_t)p_level == p_header->cell_subdiv - 1) {
		Vector3 emission;
		emission.x = (p_cell[p_idx].emission >> 24) / 255.0;
		emission.y = ((p_cell[p_idx].emission >> 16) & 0xFF) / 255.0;
		emission.z = ((p_cell[p_idx].emission >> 8) & 0xFF) / 255.0;
		float l = (p_cell[p_idx].emission & 0xFF) / 255.0;
		l *= 8.0;

		emission *= l;

		p_local_data[p_idx].energy[0] = uint16_t(emission.x * 1024); //go from 0 to 1024 for light
		p_local_data[p_idx].energy[1] = uint16_t(emission.y * 1024); //go from 0 to 1024 for light
		p_local_data[p_idx].energy[2] = uint16_t(emission.z * 1024); //go from 0 to 1024 for light
	} else {
		p_local_data[p_idx].energy[0] = 0;
		p_local_data[p_idx].energy[1] = 0;
		p_local_data[p_idx].energy[2] = 0;

		int half = (1 << (p_header->cell_subdiv - 1)) >> (p_level + 1);

		for (int i = 0; i < 8; i++) {
			uint32_t child = p_cell[p_idx].children[i];

			if (child == 0xFFFFFFFF) {
				continue;
			}

			int x = p_x;
			int y = p_y;
			int z = p_z;

			if (i & 1) {
				x += half;
			}
			if (i & 2) {
				y += half;
			}
			if (i & 4) {
				z += half;
			}

			_gi_probe_fill_local_data(child, p_level + 1, x, y, z, p_cell, p_header, p_local_data, prev_cell);
		}
	}

	//position for each part of the mipmaped texture
	p_local_data[p_idx].pos[0] = p_x >> (p_header->cell_subdiv - p_level - 1);
	p_local_data[p_idx].pos[1] = p_y >> (p_header->cell_subdiv - p_level - 1);
	p_local_data[p_idx].pos[2] = p_z >> (p_header->cell_subdiv - p_level - 1);

	prev_cell[p_level].push_back(p_idx);
}

void VisualServerScene::_gi_probe_bake_threads(void *self) {
	VisualServerScene *vss = (VisualServerScene *)self;
	vss->_gi_probe_bake_thread();
}

void VisualServerScene::_setup_gi_probe(Instance *p_instance) {
	InstanceGIProbeData *probe = static_cast<InstanceGIProbeData *>(p_instance->base_data);

	if (probe->dynamic.probe_data.is_valid()) {
		VSG::storage->free(probe->dynamic.probe_data);
		probe->dynamic.probe_data = RID();
	}

	probe->dynamic.light_data = VSG::storage->gi_probe_get_dynamic_data(p_instance->base);

	if (probe->dynamic.light_data.size() == 0) {
		return;
	}
	//using dynamic data
	PoolVector<int>::Read r = probe->dynamic.light_data.read();

	const GIProbeDataHeader *header = (GIProbeDataHeader *)r.ptr();

	probe->dynamic.local_data.resize(header->cell_count);

	int cell_count = probe->dynamic.local_data.size();
	PoolVector<InstanceGIProbeData::LocalData>::Write ldw = probe->dynamic.local_data.write();
	const GIProbeDataCell *cells = (GIProbeDataCell *)&r[16];

	probe->dynamic.level_cell_lists.resize(header->cell_subdiv);

	_gi_probe_fill_local_data(0, 0, 0, 0, 0, cells, header, ldw.ptr(), probe->dynamic.level_cell_lists.ptrw());

	probe->dynamic.compression = RasterizerStorage::GI_PROBE_UNCOMPRESSED;

	probe->dynamic.probe_data = VSG::storage->gi_probe_dynamic_data_create(header->width, header->height, header->depth, probe->dynamic.compression);

	probe->dynamic.bake_dynamic_range = VSG::storage->gi_probe_get_dynamic_range(p_instance->base);

	probe->dynamic.mipmaps_3d.clear();
	probe->dynamic.propagate = VSG::storage->gi_probe_get_propagation(p_instance->base);

	probe->dynamic.grid_size[0] = header->width;
	probe->dynamic.grid_size[1] = header->height;
	probe->dynamic.grid_size[2] = header->depth;

	int size_limit = 1;
	int size_divisor = 1;

	if (probe->dynamic.compression == RasterizerStorage::GI_PROBE_S3TC) {
		size_limit = 4;
		size_divisor = 4;
	}
	for (int i = 0; i < (int)header->cell_subdiv; i++) {
		int x = header->width >> i;
		int y = header->height >> i;
		int z = header->depth >> i;

		//create and clear mipmap
		PoolVector<uint8_t> mipmap;
		int size = x * y * z * 4;
		size /= size_divisor;
		mipmap.resize(size);
		PoolVector<uint8_t>::Write w = mipmap.write();
		memset(w.ptr(), 0, size);
		w.release();

		probe->dynamic.mipmaps_3d.push_back(mipmap);

		if (x <= size_limit || y <= size_limit || z <= size_limit) {
			break;
		}
	}

	probe->dynamic.updating_stage = GI_UPDATE_STAGE_CHECK;
	probe->invalid = false;
	probe->dynamic.enabled = true;

	Transform cell_to_xform = VSG::storage->gi_probe_get_to_cell_xform(p_instance->base);
	AABB bounds = VSG::storage->gi_probe_get_bounds(p_instance->base);
	float cell_size = VSG::storage->gi_probe_get_cell_size(p_instance->base);

	probe->dynamic.light_to_cell_xform = cell_to_xform * p_instance->transform.affine_inverse();

	VSG::scene_render->gi_probe_instance_set_light_data(probe->probe_instance, p_instance->base, probe->dynamic.probe_data);
	VSG::scene_render->gi_probe_instance_set_transform_to_data(probe->probe_instance, probe->dynamic.light_to_cell_xform);

	VSG::scene_render->gi_probe_instance_set_bounds(probe->probe_instance, bounds.size / cell_size);

	probe->base_version = VSG::storage->gi_probe_get_version(p_instance->base);

	//if compression is S3TC, fill it up
	if (probe->dynamic.compression == RasterizerStorage::GI_PROBE_S3TC) {
		//create all blocks
		Vector<Map<uint32_t, InstanceGIProbeData::CompBlockS3TC>> comp_blocks;
		int mipmap_count = probe->dynamic.mipmaps_3d.size();
		comp_blocks.resize(mipmap_count);

		for (int i = 0; i < cell_count; i++) {
			const GIProbeDataCell &c = cells[i];
			const InstanceGIProbeData::LocalData &ld = ldw[i];
			int level = c.level_alpha >> 16;
			int mipmap = header->cell_subdiv - level - 1;
			if (mipmap >= mipmap_count) {
				continue; //uninteresting
			}

			int blockx = (ld.pos[0] >> 2);
			int blocky = (ld.pos[1] >> 2);
			int blockz = (ld.pos[2]); //compression is x/y only

			int blockw = (header->width >> mipmap) >> 2;
			int blockh = (header->height >> mipmap) >> 2;

			//print_line("cell "+itos(i)+" level "+itos(level)+"mipmap: "+itos(mipmap)+" pos: "+Vector3(blockx,blocky,blockz)+" size "+Vector2(blockw,blockh));

			uint32_t key = blockz * blockw * blockh + blocky * blockw + blockx;

			Map<uint32_t, InstanceGIProbeData::CompBlockS3TC> &cmap = comp_blocks.write[mipmap];

			if (!cmap.has(key)) {
				InstanceGIProbeData::CompBlockS3TC k;
				k.offset = key; //use offset as counter first
				k.source_count = 0;
				cmap[key] = k;
			}

			InstanceGIProbeData::CompBlockS3TC &k = cmap[key];
			ERR_CONTINUE(k.source_count == 16);
			k.sources[k.source_count++] = i;
		}

		//fix the blocks, precomputing what is needed
		probe->dynamic.mipmaps_s3tc.resize(mipmap_count);

		for (int i = 0; i < mipmap_count; i++) {
			//print_line("S3TC level: " + itos(i) + " blocks: " + itos(comp_blocks[i].size()));
			probe->dynamic.mipmaps_s3tc.write[i].resize(comp_blocks[i].size());
			PoolVector<InstanceGIProbeData::CompBlockS3TC>::Write w = probe->dynamic.mipmaps_s3tc.write[i].write();
			int block_idx = 0;

			for (Map<uint32_t, InstanceGIProbeData::CompBlockS3TC>::Element *E = comp_blocks[i].front(); E; E = E->next()) {
				InstanceGIProbeData::CompBlockS3TC k = E->get();

				//PRECOMPUTE ALPHA
				int max_alpha = -100000;
				int min_alpha = k.source_count == 16 ? 100000 : 0; //if the block is not completely full, minimum is always 0, (and those blocks will map to 1, which will be zero)

				uint8_t alpha_block[4][4] = { { 0, 0, 0, 0 }, { 0, 0, 0, 0 }, { 0, 0, 0, 0 }, { 0, 0, 0, 0 } };

				for (uint32_t j = 0; j < k.source_count; j++) {
					int alpha = (cells[k.sources[j]].level_alpha >> 8) & 0xFF;
					if (alpha < min_alpha) {
						min_alpha = alpha;
					}
					if (alpha > max_alpha) {
						max_alpha = alpha;
					}
					//fill up alpha block
					alpha_block[ldw[k.sources[j]].pos[0] % 4][ldw[k.sources[j]].pos[1] % 4] = alpha;
				}

				//use the first mode (8 adjustable levels)
				k.alpha[0] = max_alpha;
				k.alpha[1] = min_alpha;

				uint64_t alpha_bits = 0;

				if (max_alpha != min_alpha) {
					int idx = 0;

					for (int y = 0; y < 4; y++) {
						for (int x = 0; x < 4; x++) {
							//subtract minimum
							uint32_t a = uint32_t(alpha_block[x][y]) - min_alpha;
							//convert range to 3 bits
							a = int((a * 7.0 / (max_alpha - min_alpha)) + 0.5);
							a = MIN(a, 7); //just to be sure
							a = 7 - a; //because range is inverted in this mode
							if (a == 0) {
								//do none, remain
							} else if (a == 7) {
								a = 1;
							} else {
								a = a + 1;
							}

							alpha_bits |= uint64_t(a) << (idx * 3);
							idx++;
						}
					}
				}

				k.alpha[2] = (alpha_bits >> 0) & 0xFF;
				k.alpha[3] = (alpha_bits >> 8) & 0xFF;
				k.alpha[4] = (alpha_bits >> 16) & 0xFF;
				k.alpha[5] = (alpha_bits >> 24) & 0xFF;
				k.alpha[6] = (alpha_bits >> 32) & 0xFF;
				k.alpha[7] = (alpha_bits >> 40) & 0xFF;

				w[block_idx++] = k;
			}
		}
	}
}

void VisualServerScene::_gi_probe_bake_thread() {
	while (true) {
		probe_bake_sem.wait();
		if (probe_bake_thread_exit) {
			break;
		}

		Instance *to_bake = nullptr;

		probe_bake_mutex.lock();

		if (!probe_bake_list.empty()) {
			to_bake = probe_bake_list.front()->get();
			probe_bake_list.pop_front();
		}
		probe_bake_mutex.unlock();

		if (!to_bake) {
			continue;
		}

		_bake_gi_probe(to_bake);
	}
}

uint32_t VisualServerScene::_gi_bake_find_cell(const GIProbeDataCell *cells, int x, int y, int z, int p_cell_subdiv) {
	uint32_t cell = 0;

	int ofs_x = 0;
	int ofs_y = 0;
	int ofs_z = 0;
	int size = 1 << (p_cell_subdiv - 1);
	int half = size / 2;

	if (x < 0 || x >= size) {
		return -1;
	}
	if (y < 0 || y >= size) {
		return -1;
	}
	if (z < 0 || z >= size) {
		return -1;
	}

	for (int i = 0; i < p_cell_subdiv - 1; i++) {
		const GIProbeDataCell *bc = &cells[cell];

		int child = 0;
		if (x >= ofs_x + half) {
			child |= 1;
			ofs_x += half;
		}
		if (y >= ofs_y + half) {
			child |= 2;
			ofs_y += half;
		}
		if (z >= ofs_z + half) {
			child |= 4;
			ofs_z += half;
		}

		cell = bc->children[child];
		if (cell == 0xFFFFFFFF) {
			return 0xFFFFFFFF;
		}

		half >>= 1;
	}

	return cell;
}

static float _get_normal_advance(const Vector3 &p_normal) {
	Vector3 normal = p_normal;
	Vector3 unorm = normal.abs();

	if ((unorm.x >= unorm.y) && (unorm.x >= unorm.z)) {
		// x code
		unorm = normal.x > 0.0 ? Vector3(1.0, 0.0, 0.0) : Vector3(-1.0, 0.0, 0.0);
	} else if ((unorm.y > unorm.x) && (unorm.y >= unorm.z)) {
		// y code
		unorm = normal.y > 0.0 ? Vector3(0.0, 1.0, 0.0) : Vector3(0.0, -1.0, 0.0);
	} else if ((unorm.z > unorm.x) && (unorm.z > unorm.y)) {
		// z code
		unorm = normal.z > 0.0 ? Vector3(0.0, 0.0, 1.0) : Vector3(0.0, 0.0, -1.0);
	} else {
		// oh-no we messed up code
		// has to be
		unorm = Vector3(1.0, 0.0, 0.0);
	}

	return 1.0 / normal.dot(unorm);
}

void VisualServerScene::_bake_gi_probe_light(const GIProbeDataHeader *header, const GIProbeDataCell *cells, InstanceGIProbeData::LocalData *local_data, const uint32_t *leaves, int p_leaf_count, const InstanceGIProbeData::LightCache &light_cache, int p_sign) {
	int light_r = int(light_cache.color.r * light_cache.energy * 1024.0) * p_sign;
	int light_g = int(light_cache.color.g * light_cache.energy * 1024.0) * p_sign;
	int light_b = int(light_cache.color.b * light_cache.energy * 1024.0) * p_sign;

	float limits[3] = { float(header->width), float(header->height), float(header->depth) };
	Plane clip[3];
	int clip_planes = 0;

	switch (light_cache.type) {
		case VS::LIGHT_DIRECTIONAL: {
			float max_len = Vector3(limits[0], limits[1], limits[2]).length() * 1.1;

			Vector3 light_axis = -light_cache.transform.basis.get_axis(2).normalized();

			for (int i = 0; i < 3; i++) {
				if (Math::is_zero_approx(light_axis[i])) {
					continue;
				}
				clip[clip_planes].normal[i] = 1.0;

				if (light_axis[i] < 0) {
					clip[clip_planes].d = limits[i] + 1;
				} else {
					clip[clip_planes].d -= 1.0;
				}

				clip_planes++;
			}

			float distance_adv = _get_normal_advance(light_axis);

			for (int i = 0; i < p_leaf_count; i++) {
				uint32_t idx = leaves[i];

				const GIProbeDataCell *cell = &cells[idx];
				InstanceGIProbeData::LocalData *light = &local_data[idx];

				Vector3 to(light->pos[0] + 0.5, light->pos[1] + 0.5, light->pos[2] + 0.5);
				to += -light_axis.sign() * 0.47; //make it more likely to receive a ray

				Vector3 norm(
						(((cells[idx].normal >> 16) & 0xFF) / 255.0) * 2.0 - 1.0,
						(((cells[idx].normal >> 8) & 0xFF) / 255.0) * 2.0 - 1.0,
						(((cells[idx].normal >> 0) & 0xFF) / 255.0) * 2.0 - 1.0);

				float att = norm.dot(-light_axis);
				if (att < 0.001) {
					//not lighting towards this
					continue;
				}

				Vector3 from = to - max_len * light_axis;

				for (int j = 0; j < clip_planes; j++) {
					clip[j].intersects_segment(from, to, &from);
				}

				float distance = (to - from).length();
				distance += distance_adv - Math::fmod(distance, distance_adv); //make it reach the center of the box always
				from = to - light_axis * distance;

				uint32_t result = 0xFFFFFFFF;

				while (distance > -distance_adv) { //use this to avoid precision errors

					result = _gi_bake_find_cell(cells, int(floor(from.x)), int(floor(from.y)), int(floor(from.z)), header->cell_subdiv);
					if (result != 0xFFFFFFFF) {
						break;
					}

					from += light_axis * distance_adv;
					distance -= distance_adv;
				}

				if (result == idx) {
					//cell hit itself! hooray!
					light->energy[0] += int32_t(light_r * att * ((cell->albedo >> 16) & 0xFF) / 255.0);
					light->energy[1] += int32_t(light_g * att * ((cell->albedo >> 8) & 0xFF) / 255.0);
					light->energy[2] += int32_t(light_b * att * ((cell->albedo) & 0xFF) / 255.0);
				}
			}
		} break;
		case VS::LIGHT_OMNI:
		case VS::LIGHT_SPOT: {
			Vector3 light_pos = light_cache.transform.origin;
			Vector3 spot_axis = -light_cache.transform.basis.get_axis(2).normalized();

			float local_radius = light_cache.radius * light_cache.transform.basis.get_axis(2).length();

			for (int i = 0; i < p_leaf_count; i++) {
				uint32_t idx = leaves[i];

				const GIProbeDataCell *cell = &cells[idx];
				InstanceGIProbeData::LocalData *light = &local_data[idx];

				Vector3 to(light->pos[0] + 0.5, light->pos[1] + 0.5, light->pos[2] + 0.5);
				to += (light_pos - to).sign() * 0.47; //make it more likely to receive a ray

				Vector3 norm(
						(((cells[idx].normal >> 16) & 0xFF) / 255.0) * 2.0 - 1.0,
						(((cells[idx].normal >> 8) & 0xFF) / 255.0) * 2.0 - 1.0,
						(((cells[idx].normal >> 0) & 0xFF) / 255.0) * 2.0 - 1.0);

				Vector3 light_axis = (to - light_pos).normalized();
				float distance_adv = _get_normal_advance(light_axis);

				float att = norm.dot(-light_axis);
				if (att < 0.001) {
					//not lighting towards this
					continue;
				}

				{
					float d = light_pos.distance_to(to);
					if (d + distance_adv > local_radius) {
						continue; // too far away
					}

					float dt = CLAMP((d + distance_adv) / local_radius, 0, 1);
					att *= powf(1.0 - dt, light_cache.attenuation);
				}

				if (light_cache.type == VS::LIGHT_SPOT) {
					float angle = Math::rad2deg(acos(light_axis.dot(spot_axis)));
					if (angle > light_cache.spot_angle) {
						continue;
					}

					float d = CLAMP(angle / light_cache.spot_angle, 0, 1);
					att *= powf(1.0 - d, light_cache.spot_attenuation);
				}

				clip_planes = 0;

				for (int c = 0; c < 3; c++) {
					if (Math::is_zero_approx(light_axis[c])) {
						continue;
					}
					clip[clip_planes].normal[c] = 1.0;

					if (light_axis[c] < 0) {
						clip[clip_planes].d = limits[c] + 1;
					} else {
						clip[clip_planes].d -= 1.0;
					}

					clip_planes++;
				}

				Vector3 from = light_pos;

				for (int j = 0; j < clip_planes; j++) {
					clip[j].intersects_segment(from, to, &from);
				}

				float distance = (to - from).length();

				distance -= Math::fmod(distance, distance_adv); //make it reach the center of the box always, but this tame make it closer
				from = to - light_axis * distance;

				uint32_t result = 0xFFFFFFFF;

				while (distance > -distance_adv) { //use this to avoid precision errors

					result = _gi_bake_find_cell(cells, int(floor(from.x)), int(floor(from.y)), int(floor(from.z)), header->cell_subdiv);
					if (result != 0xFFFFFFFF) {
						break;
					}

					from += light_axis * distance_adv;
					distance -= distance_adv;
				}

				if (result == idx) {
					//cell hit itself! hooray!

					light->energy[0] += int32_t(light_r * att * ((cell->albedo >> 16) & 0xFF) / 255.0);
					light->energy[1] += int32_t(light_g * att * ((cell->albedo >> 8) & 0xFF) / 255.0);
					light->energy[2] += int32_t(light_b * att * ((cell->albedo) & 0xFF) / 255.0);
				}
			}
		} break;
	}
}

void VisualServerScene::_bake_gi_downscale_light(int p_idx, int p_level, const GIProbeDataCell *p_cells, const GIProbeDataHeader *p_header, InstanceGIProbeData::LocalData *p_local_data, float p_propagate) {
	//average light to upper level

	float divisor = 0;
	float sum[3] = { 0.0, 0.0, 0.0 };

	for (int i = 0; i < 8; i++) {
		uint32_t child = p_cells[p_idx].children[i];

		if (child == 0xFFFFFFFF) {
			continue;
		}

		if (p_level + 1 < (int)p_header->cell_subdiv - 1) {
			_bake_gi_downscale_light(child, p_level + 1, p_cells, p_header, p_local_data, p_propagate);
		}

		sum[0] += p_local_data[child].energy[0];
		sum[1] += p_local_data[child].energy[1];
		sum[2] += p_local_data[child].energy[2];
		divisor += 1.0;
	}

	divisor = Math::lerp((float)8.0, divisor, p_propagate);
	sum[0] /= divisor;
	sum[1] /= divisor;
	sum[2] /= divisor;

	//divide by eight for average
	p_local_data[p_idx].energy[0] = Math::fast_ftoi(sum[0]);
	p_local_data[p_idx].energy[1] = Math::fast_ftoi(sum[1]);
	p_local_data[p_idx].energy[2] = Math::fast_ftoi(sum[2]);
}

void VisualServerScene::_bake_gi_probe(Instance *p_gi_probe) {
	InstanceGIProbeData *probe_data = static_cast<InstanceGIProbeData *>(p_gi_probe->base_data);

	PoolVector<int>::Read r = probe_data->dynamic.light_data.read();

	const GIProbeDataHeader *header = (const GIProbeDataHeader *)r.ptr();
	const GIProbeDataCell *cells = (const GIProbeDataCell *)&r[16];

	int leaf_count = probe_data->dynamic.level_cell_lists[header->cell_subdiv - 1].size();
	const uint32_t *leaves = probe_data->dynamic.level_cell_lists[header->cell_subdiv - 1].ptr();

	PoolVector<InstanceGIProbeData::LocalData>::Write ldw = probe_data->dynamic.local_data.write();

	InstanceGIProbeData::LocalData *local_data = ldw.ptr();

	//remove what must be removed
	for (Map<RID, InstanceGIProbeData::LightCache>::Element *E = probe_data->dynamic.light_cache.front(); E; E = E->next()) {
		RID rid = E->key();
		const InstanceGIProbeData::LightCache &lc = E->get();

		if ((!probe_data->dynamic.light_cache_changes.has(rid) || probe_data->dynamic.light_cache_changes[rid] != lc) && lc.visible) {
			//erase light data

			_bake_gi_probe_light(header, cells, local_data, leaves, leaf_count, lc, -1);
		}
	}

	//add what must be added
	for (Map<RID, InstanceGIProbeData::LightCache>::Element *E = probe_data->dynamic.light_cache_changes.front(); E; E = E->next()) {
		RID rid = E->key();
		const InstanceGIProbeData::LightCache &lc = E->get();

		if ((!probe_data->dynamic.light_cache.has(rid) || probe_data->dynamic.light_cache[rid] != lc) && lc.visible) {
			//add light data

			_bake_gi_probe_light(header, cells, local_data, leaves, leaf_count, lc, 1);
		}
	}

	SWAP(probe_data->dynamic.light_cache_changes, probe_data->dynamic.light_cache);

	//downscale to lower res levels
	_bake_gi_downscale_light(0, 0, cells, header, local_data, probe_data->dynamic.propagate);

	//plot result to 3D texture!

	if (probe_data->dynamic.compression == RasterizerStorage::GI_PROBE_UNCOMPRESSED) {
		for (int i = 0; i < (int)header->cell_subdiv; i++) {
			int stage = header->cell_subdiv - i - 1;

			if (stage >= probe_data->dynamic.mipmaps_3d.size()) {
				continue; //no mipmap for this one
			}

			//print_line("generating mipmap stage: " + itos(stage));
			int level_cell_count = probe_data->dynamic.level_cell_lists[i].size();
			const uint32_t *level_cells = probe_data->dynamic.level_cell_lists[i].ptr();

			PoolVector<uint8_t>::Write lw = probe_data->dynamic.mipmaps_3d.write[stage].write();
			uint8_t *mipmapw = lw.ptr();

			uint32_t sizes[3] = { header->width >> stage, header->height >> stage, header->depth >> stage };

			for (int j = 0; j < level_cell_count; j++) {
				uint32_t idx = level_cells[j];

				uint32_t r2 = (uint32_t(local_data[idx].energy[0]) / probe_data->dynamic.bake_dynamic_range) >> 2;
				uint32_t g = (uint32_t(local_data[idx].energy[1]) / probe_data->dynamic.bake_dynamic_range) >> 2;
				uint32_t b = (uint32_t(local_data[idx].energy[2]) / probe_data->dynamic.bake_dynamic_range) >> 2;
				uint32_t a = (cells[idx].level_alpha >> 8) & 0xFF;

				uint32_t mm_ofs = sizes[0] * sizes[1] * (local_data[idx].pos[2]) + sizes[0] * (local_data[idx].pos[1]) + (local_data[idx].pos[0]);
				mm_ofs *= 4; //for RGBA (4 bytes)

				mipmapw[mm_ofs + 0] = uint8_t(MIN(r2, 255));
				mipmapw[mm_ofs + 1] = uint8_t(MIN(g, 255));
				mipmapw[mm_ofs + 2] = uint8_t(MIN(b, 255));
				mipmapw[mm_ofs + 3] = uint8_t(MIN(a, 255));
			}
		}
	} else if (probe_data->dynamic.compression == RasterizerStorage::GI_PROBE_S3TC) {
		int mipmap_count = probe_data->dynamic.mipmaps_3d.size();

		for (int mmi = 0; mmi < mipmap_count; mmi++) {
			PoolVector<uint8_t>::Write mmw = probe_data->dynamic.mipmaps_3d.write[mmi].write();
			int block_count = probe_data->dynamic.mipmaps_s3tc[mmi].size();
			PoolVector<InstanceGIProbeData::CompBlockS3TC>::Read mmr = probe_data->dynamic.mipmaps_s3tc[mmi].read();

			for (int i = 0; i < block_count; i++) {
				const InstanceGIProbeData::CompBlockS3TC &b = mmr[i];

				uint8_t *blockptr = &mmw[b.offset * 16];
				memcpy(blockptr, b.alpha, 8); //copy alpha part, which is precomputed

				Vector3 colors[16];

				for (uint32_t j = 0; j < b.source_count; j++) {
					colors[j].x = (local_data[b.sources[j]].energy[0] / float(probe_data->dynamic.bake_dynamic_range)) / 1024.0;
					colors[j].y = (local_data[b.sources[j]].energy[1] / float(probe_data->dynamic.bake_dynamic_range)) / 1024.0;
					colors[j].z = (local_data[b.sources[j]].energy[2] / float(probe_data->dynamic.bake_dynamic_range)) / 1024.0;
				}
				//super quick and dirty compression
				//find 2 most further apart
				float distance = 0;
				Vector3 from, to;

				if (b.source_count == 16) {
					//all cells are used so, find minmax between them
					int further_apart[2] = { 0, 0 };
					for (uint32_t j = 0; j < b.source_count; j++) {
						for (uint32_t k = j + 1; k < b.source_count; k++) {
							float d = colors[j].distance_squared_to(colors[k]);
							if (d > distance) {
								distance = d;
								further_apart[0] = j;
								further_apart[1] = k;
							}
						}
					}

					from = colors[further_apart[0]];
					to = colors[further_apart[1]];

				} else {
					//if a block is missing, the priority is that this block remains black,
					//otherwise the geometry will appear deformed
					//correct shape wins over correct color in this case
					//average all colors first
					Vector3 average;

					for (uint32_t j = 0; j < b.source_count; j++) {
						average += colors[j];
					}
					average.normalize();
					//find max distance in normal from average
					for (uint32_t j = 0; j < b.source_count; j++) {
						float d = average.dot(colors[j]);
						distance = MAX(d, distance);
					}

					from = Vector3(); //from black
					to = average * distance;
					//find max distance
				}

				int indices[16];
				uint16_t color_0 = 0;
				color_0 = CLAMP(int(from.x * 31), 0, 31) << 11;
				color_0 |= CLAMP(int(from.y * 63), 0, 63) << 5;
				color_0 |= CLAMP(int(from.z * 31), 0, 31);

				uint16_t color_1 = 0;
				color_1 = CLAMP(int(to.x * 31), 0, 31) << 11;
				color_1 |= CLAMP(int(to.y * 63), 0, 63) << 5;
				color_1 |= CLAMP(int(to.z * 31), 0, 31);

				if (color_1 > color_0) {
					SWAP(color_1, color_0);
					SWAP(from, to);
				}

				if (distance > 0) {
					Vector3 dir = (to - from).normalized();

					for (uint32_t j = 0; j < b.source_count; j++) {
						float d = (colors[j] - from).dot(dir) / distance;
						indices[j] = int(d * 3 + 0.5);

						static const int index_swap[4] = { 0, 3, 1, 2 };

						indices[j] = index_swap[CLAMP(indices[j], 0, 3)];
					}
				} else {
					for (uint32_t j = 0; j < b.source_count; j++) {
						indices[j] = 0;
					}
				}

				//by default, 1 is black, otherwise it will be overridden by source

				uint32_t index_block[16] = { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 };

				for (uint32_t j = 0; j < b.source_count; j++) {
					int x = local_data[b.sources[j]].pos[0] % 4;
					int y = local_data[b.sources[j]].pos[1] % 4;

					index_block[y * 4 + x] = indices[j];
				}

				uint32_t encode = 0;

				for (int j = 0; j < 16; j++) {
					encode |= index_block[j] << (j * 2);
				}

				blockptr[8] = color_0 & 0xFF;
				blockptr[9] = (color_0 >> 8) & 0xFF;
				blockptr[10] = color_1 & 0xFF;
				blockptr[11] = (color_1 >> 8) & 0xFF;
				blockptr[12] = encode & 0xFF;
				blockptr[13] = (encode >> 8) & 0xFF;
				blockptr[14] = (encode >> 16) & 0xFF;
				blockptr[15] = (encode >> 24) & 0xFF;
			}
		}
	}

	//send back to main thread to update un little chunks
	probe_bake_mutex.lock();
	probe_data->dynamic.updating_stage = GI_UPDATE_STAGE_UPLOADING;
	probe_bake_mutex.unlock();
}

bool VisualServerScene::_check_gi_probe(Instance *p_gi_probe) {
	InstanceGIProbeData *probe_data = static_cast<InstanceGIProbeData *>(p_gi_probe->base_data);

	probe_data->dynamic.light_cache_changes.clear();

	bool all_equal = true;

	for (List<Instance *>::Element *E = p_gi_probe->scenario->directional_lights.front(); E; E = E->next()) {
		if (VSG::storage->light_get_bake_mode(E->get()->base) == VS::LightBakeMode::LIGHT_BAKE_DISABLED) {
			continue;
		}

		InstanceGIProbeData::LightCache lc;
		lc.type = VSG::storage->light_get_type(E->get()->base);
		lc.color = VSG::storage->light_get_color(E->get()->base);
		lc.energy = VSG::storage->light_get_param(E->get()->base, VS::LIGHT_PARAM_ENERGY) * VSG::storage->light_get_param(E->get()->base, VS::LIGHT_PARAM_INDIRECT_ENERGY);
		lc.radius = VSG::storage->light_get_param(E->get()->base, VS::LIGHT_PARAM_RANGE);
		lc.attenuation = VSG::storage->light_get_param(E->get()->base, VS::LIGHT_PARAM_ATTENUATION);
		lc.spot_angle = VSG::storage->light_get_param(E->get()->base, VS::LIGHT_PARAM_SPOT_ANGLE);
		lc.spot_attenuation = VSG::storage->light_get_param(E->get()->base, VS::LIGHT_PARAM_SPOT_ATTENUATION);
		lc.transform = probe_data->dynamic.light_to_cell_xform * E->get()->transform;
		lc.visible = E->get()->visible;

		if (!probe_data->dynamic.light_cache.has(E->get()->self) || probe_data->dynamic.light_cache[E->get()->self] != lc) {
			all_equal = false;
		}

		probe_data->dynamic.light_cache_changes[E->get()->self] = lc;
	}

	for (Set<Instance *>::Element *E = probe_data->lights.front(); E; E = E->next()) {
		if (VSG::storage->light_get_bake_mode(E->get()->base) == VS::LightBakeMode::LIGHT_BAKE_DISABLED) {
			continue;
		}

		InstanceGIProbeData::LightCache lc;
		lc.type = VSG::storage->light_get_type(E->get()->base);
		lc.color = VSG::storage->light_get_color(E->get()->base);
		lc.energy = VSG::storage->light_get_param(E->get()->base, VS::LIGHT_PARAM_ENERGY) * VSG::storage->light_get_param(E->get()->base, VS::LIGHT_PARAM_INDIRECT_ENERGY);
		lc.radius = VSG::storage->light_get_param(E->get()->base, VS::LIGHT_PARAM_RANGE);
		lc.attenuation = VSG::storage->light_get_param(E->get()->base, VS::LIGHT_PARAM_ATTENUATION);
		lc.spot_angle = VSG::storage->light_get_param(E->get()->base, VS::LIGHT_PARAM_SPOT_ANGLE);
		lc.spot_attenuation = VSG::storage->light_get_param(E->get()->base, VS::LIGHT_PARAM_SPOT_ATTENUATION);
		lc.transform = probe_data->dynamic.light_to_cell_xform * E->get()->transform;
		lc.visible = E->get()->visible;

		if (!probe_data->dynamic.light_cache.has(E->get()->self) || probe_data->dynamic.light_cache[E->get()->self] != lc) {
			all_equal = false;
		}

		probe_data->dynamic.light_cache_changes[E->get()->self] = lc;
	}

	//lighting changed from after to before, must do some updating
	return !all_equal || probe_data->dynamic.light_cache_changes.size() != probe_data->dynamic.light_cache.size();
}

void VisualServerScene::render_probes() {
	/* REFLECTION PROBES */

	SelfList<InstanceReflectionProbeData> *ref_probe = reflection_probe_render_list.first();

	bool busy = false;

	while (ref_probe) {
		SelfList<InstanceReflectionProbeData> *next = ref_probe->next();
		RID base = ref_probe->self()->owner->base;

		switch (VSG::storage->reflection_probe_get_update_mode(base)) {
			case VS::REFLECTION_PROBE_UPDATE_ONCE: {
				if (busy) { //already rendering something
					break;
				}

				bool done = _render_reflection_probe_step(ref_probe->self()->owner, ref_probe->self()->render_step);
				if (done) {
					reflection_probe_render_list.remove(ref_probe);
				} else {
					ref_probe->self()->render_step++;
				}

				busy = true; //do not render another one of this kind
			} break;
			case VS::REFLECTION_PROBE_UPDATE_ALWAYS: {
				int step = 0;
				bool done = false;
				while (!done) {
					done = _render_reflection_probe_step(ref_probe->self()->owner, step);
					step++;
				}

				reflection_probe_render_list.remove(ref_probe);
			} break;
		}

		ref_probe = next;
	}

	/* GI PROBES */

	SelfList<InstanceGIProbeData> *gi_probe = gi_probe_update_list.first();

	while (gi_probe) {
		SelfList<InstanceGIProbeData> *next = gi_probe->next();

		InstanceGIProbeData *probe = gi_probe->self();
		Instance *instance_probe = probe->owner;

		//check if probe must be setup, but don't do if on the lighting thread

		bool force_lighting = false;

		if (probe->invalid || (probe->dynamic.updating_stage == GI_UPDATE_STAGE_CHECK && probe->base_version != VSG::storage->gi_probe_get_version(instance_probe->base))) {
			_setup_gi_probe(instance_probe);
			force_lighting = true;
		}

		float propagate = VSG::storage->gi_probe_get_propagation(instance_probe->base);

		if (probe->dynamic.propagate != propagate) {
			probe->dynamic.propagate = propagate;
			force_lighting = true;
		}

		if (!probe->invalid && probe->dynamic.enabled) {
			switch (probe->dynamic.updating_stage) {
				case GI_UPDATE_STAGE_CHECK: {
					if (_check_gi_probe(instance_probe) || force_lighting) { //send to lighting thread

#ifndef NO_THREADS
						probe_bake_mutex.lock();
						probe->dynamic.updating_stage = GI_UPDATE_STAGE_LIGHTING;
						probe_bake_list.push_back(instance_probe);
						probe_bake_mutex.unlock();
						probe_bake_sem.post();

#else

						_bake_gi_probe(instance_probe);
#endif
					}
				} break;
				case GI_UPDATE_STAGE_LIGHTING: {
					//do none, wait til done!

				} break;
				case GI_UPDATE_STAGE_UPLOADING: {
					//uint64_t us = OS::get_singleton()->get_ticks_usec();

					for (int i = 0; i < (int)probe->dynamic.mipmaps_3d.size(); i++) {
						PoolVector<uint8_t>::Read r = probe->dynamic.mipmaps_3d[i].read();
						VSG::storage->gi_probe_dynamic_data_update(probe->dynamic.probe_data, 0, probe->dynamic.grid_size[2] >> i, i, r.ptr());
					}

					probe->dynamic.updating_stage = GI_UPDATE_STAGE_CHECK;

					//print_line("UPLOAD TIME: " + rtos((OS::get_singleton()->get_ticks_usec() - us) / 1000000.0));
				} break;
			}
		}
		//_update_gi_probe(gi_probe->self()->owner);

		gi_probe = next;
	}
}

void VisualServerScene::_update_dirty_instance(Instance *p_instance) {
	if (p_instance->update_aabb) {
		_update_instance_aabb(p_instance);
	}

	if (p_instance->update_materials) {
		if (p_instance->base_type == VS::INSTANCE_MESH) {
			//remove materials no longer used and un-own them

			int new_mat_count = VSG::storage->mesh_get_surface_count(p_instance->base);
			for (int i = p_instance->materials.size() - 1; i >= new_mat_count; i--) {
				if (p_instance->materials[i].is_valid()) {
					VSG::storage->material_remove_instance_owner(p_instance->materials[i], p_instance);
				}
			}
			p_instance->materials.resize(new_mat_count);

			int new_blend_shape_count = VSG::storage->mesh_get_blend_shape_count(p_instance->base);
			if (new_blend_shape_count != p_instance->blend_values.size()) {
				p_instance->blend_values.resize(new_blend_shape_count);
				for (int i = 0; i < new_blend_shape_count; i++) {
					p_instance->blend_values.write().ptr()[i] = 0;
				}
			}
		}

		if ((1 << p_instance->base_type) & VS::INSTANCE_GEOMETRY_MASK) {
			InstanceGeometryData *geom = static_cast<InstanceGeometryData *>(p_instance->base_data);

			bool can_cast_shadows = true;
			bool is_animated = false;

			if (p_instance->cast_shadows == VS::SHADOW_CASTING_SETTING_OFF) {
				can_cast_shadows = false;
			} else if (p_instance->material_override.is_valid()) {
				can_cast_shadows = VSG::storage->material_casts_shadows(p_instance->material_override);
				is_animated = VSG::storage->material_is_animated(p_instance->material_override);
			} else {
				if (p_instance->base_type == VS::INSTANCE_MESH) {
					RID mesh = p_instance->base;

					if (mesh.is_valid()) {
						bool cast_shadows = false;

						for (int i = 0; i < p_instance->materials.size(); i++) {
							RID mat = p_instance->materials[i].is_valid() ? p_instance->materials[i] : VSG::storage->mesh_surface_get_material(mesh, i);

							if (!mat.is_valid()) {
								cast_shadows = true;
							} else {
								if (VSG::storage->material_casts_shadows(mat)) {
									cast_shadows = true;
								}

								if (VSG::storage->material_is_animated(mat)) {
									is_animated = true;
								}
							}
						}

						if (!cast_shadows) {
							can_cast_shadows = false;
						}
					}

				} else if (p_instance->base_type == VS::INSTANCE_MULTIMESH) {
					RID mesh = VSG::storage->multimesh_get_mesh(p_instance->base);
					if (mesh.is_valid()) {
						bool cast_shadows = false;

						int sc = VSG::storage->mesh_get_surface_count(mesh);
						for (int i = 0; i < sc; i++) {
							RID mat = VSG::storage->mesh_surface_get_material(mesh, i);

							if (!mat.is_valid()) {
								cast_shadows = true;

							} else {
								if (VSG::storage->material_casts_shadows(mat)) {
									cast_shadows = true;
								}
								if (VSG::storage->material_is_animated(mat)) {
									is_animated = true;
								}
							}
						}

						if (!cast_shadows) {
							can_cast_shadows = false;
						}
					}
				} else if (p_instance->base_type == VS::INSTANCE_IMMEDIATE) {
					RID mat = VSG::storage->immediate_get_material(p_instance->base);

					can_cast_shadows = !mat.is_valid() || VSG::storage->material_casts_shadows(mat);

					if (mat.is_valid() && VSG::storage->material_is_animated(mat)) {
						is_animated = true;
					}
				} else if (p_instance->base_type == VS::INSTANCE_PARTICLES) {
					bool cast_shadows = false;

					int dp = VSG::storage->particles_get_draw_passes(p_instance->base);

					for (int i = 0; i < dp; i++) {
						RID mesh = VSG::storage->particles_get_draw_pass_mesh(p_instance->base, i);
						if (!mesh.is_valid()) {
							continue;
						}

						int sc = VSG::storage->mesh_get_surface_count(mesh);
						for (int j = 0; j < sc; j++) {
							RID mat = VSG::storage->mesh_surface_get_material(mesh, j);

							if (!mat.is_valid()) {
								cast_shadows = true;
							} else {
								if (VSG::storage->material_casts_shadows(mat)) {
									cast_shadows = true;
								}

								if (VSG::storage->material_is_animated(mat)) {
									is_animated = true;
								}
							}
						}
					}

					if (!cast_shadows) {
						can_cast_shadows = false;
					}
				}
			}

			if (p_instance->material_overlay.is_valid()) {
				can_cast_shadows = can_cast_shadows || VSG::storage->material_casts_shadows(p_instance->material_overlay);
				is_animated = is_animated || VSG::storage->material_is_animated(p_instance->material_overlay);
			}

			if (can_cast_shadows != geom->can_cast_shadows) {
				//ability to cast shadows change, let lights now
				for (List<Instance *>::Element *E = geom->lighting.front(); E; E = E->next()) {
					InstanceLightData *light = static_cast<InstanceLightData *>(E->get()->base_data);
					light->make_shadow_dirty();
				}

				geom->can_cast_shadows = can_cast_shadows;
			}

			geom->material_is_animated = is_animated;
		}
	}

	_instance_update_list.remove(&p_instance->update_item);

	_update_instance(p_instance);

	p_instance->update_aabb = false;
	p_instance->update_materials = false;
}

void VisualServerScene::update_dirty_instances() {
	VSG::storage->update_dirty_resources();

	while (_instance_update_list.first()) {
		_update_dirty_instance(_instance_update_list.first()->self());
	}
}

bool VisualServerScene::free(RID p_rid) {
	if (camera_owner.owns(p_rid)) {
		Camera *camera = camera_owner.get(p_rid);
		camera_owner.free(p_rid);
		memdelete(camera);
	} else if (scenario_owner.owns(p_rid)) {
		Scenario *scenario = scenario_owner.get(p_rid);

		while (scenario->instances.first()) {
			instance_set_scenario(scenario->instances.first()->self()->self, RID());
		}
		VSG::scene_render->free(scenario->reflection_probe_shadow_atlas);
		VSG::scene_render->free(scenario->reflection_atlas);
		scenario_owner.free(p_rid);
		memdelete(scenario);

	} else if (instance_owner.owns(p_rid)) {
		// delete the instance

		update_dirty_instances();

		Instance *instance = instance_owner.get(p_rid);
		_interpolation_data.notify_free_instance(p_rid, *instance);

		instance_set_use_lightmap(p_rid, RID(), RID(), -1, Rect2(0, 0, 1, 1));
		instance_set_scenario(p_rid, RID());
		instance_set_base(p_rid, RID());
		instance_geometry_set_material_override(p_rid, RID());
		instance_geometry_set_material_overlay(p_rid, RID());
		instance_attach_skeleton(p_rid, RID());

		update_dirty_instances(); //in case something changed this

		instance_owner.free(p_rid);
		memdelete(instance);

	} else if (room_owner.owns(p_rid)) {
		Room *room = room_owner.get(p_rid);
		room_owner.free(p_rid);
		memdelete(room);
	} else if (portal_owner.owns(p_rid)) {
		Portal *portal = portal_owner.get(p_rid);
		portal_owner.free(p_rid);
		memdelete(portal);
	} else if (ghost_owner.owns(p_rid)) {
		Ghost *ghost = ghost_owner.get(p_rid);
		ghost_owner.free(p_rid);
		memdelete(ghost);
	} else if (roomgroup_owner.owns(p_rid)) {
		RoomGroup *roomgroup = roomgroup_owner.get(p_rid);
		roomgroup_owner.free(p_rid);
		memdelete(roomgroup);
	} else if (occluder_instance_owner.owns(p_rid)) {
		OccluderInstance *occ_inst = occluder_instance_owner.get(p_rid);
		occluder_instance_owner.free(p_rid);
		memdelete(occ_inst);
	} else if (occluder_resource_owner.owns(p_rid)) {
		OccluderResource *occ_res = occluder_resource_owner.get(p_rid);
		occ_res->destroy(_portal_resources);
		occluder_resource_owner.free(p_rid);
		memdelete(occ_res);
	} else {
		return false;
	}

	return true;
}

VisualServerScene *VisualServerScene::singleton = nullptr;

VisualServerScene::VisualServerScene() {
	probe_bake_thread.start(_gi_probe_bake_threads, this);
	probe_bake_thread_exit = false;

	light_culler = memnew(VisualServerLightCuller);

	render_pass = 1;
	singleton = this;
	_use_bvh = GLOBAL_DEF("rendering/quality/spatial_partitioning/use_bvh", true);
	GLOBAL_DEF("rendering/quality/spatial_partitioning/bvh_collision_margin", 0.1);
	ProjectSettings::get_singleton()->set_custom_property_info("rendering/quality/spatial_partitioning/bvh_collision_margin", PropertyInfo(Variant::REAL, "rendering/quality/spatial_partitioning/bvh_collision_margin", PROPERTY_HINT_RANGE, "0.0,2.0,0.01"));

	light_culler->set_caster_culling_active(GLOBAL_DEF("rendering/quality/shadows/caster_culling", true));
	light_culler->set_light_culling_active(GLOBAL_DEF("rendering/quality/shadows/light_culling", true));

	_visual_server_callbacks = nullptr;
}

VisualServerScene::~VisualServerScene() {
	probe_bake_thread_exit = true;
	probe_bake_sem.post();
	probe_bake_thread.wait_to_finish();

	if (light_culler) {
		memdelete(light_culler);
		light_culler = nullptr;
	}
}

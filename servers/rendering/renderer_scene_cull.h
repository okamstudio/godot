/**************************************************************************/
/*  renderer_scene_cull.h                                                 */
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

#ifndef RENDERER_SCENE_CULL_H
#define RENDERER_SCENE_CULL_H

#include "core/math/dynamic_bvh.h"
#include "core/math/transform_interpolator.h"
#include "core/templates/bin_sorted_array.h"
#include "core/templates/local_vector.h"
#include "core/templates/paged_allocator.h"
#include "core/templates/paged_array.h"
#include "core/templates/pass_func.h"
#include "core/templates/rid_owner.h"
#include "core/templates/self_list.h"
#include "servers/rendering/renderer_scene_occlusion_cull.h"
#include "servers/rendering/renderer_scene_render.h"
#include "servers/rendering/rendering_method.h"
#include "servers/rendering/rendering_server_globals.h"
#include "servers/rendering/storage/utilities.h"

#ifndef _3D_DISABLED
#include "servers/xr/xr_interface.h"
#endif // _3D_DISABLED

class RenderingLightCuller;

class RendererSceneCull : public RenderingMethod {
public:
	RendererSceneRender *scene_render = nullptr;

	enum {
		SDFGI_MAX_CASCADES = 8,
		SDFGI_MAX_REGIONS_PER_CASCADE = 3,
		MAX_INSTANCE_PAIRS = 32,
		MAX_UPDATE_SHADOWS = 512
	};

	uint64_t render_pass;

	static RendererSceneCull *singleton;

	/* EVENT QUEUING */

	void tick();
	void pre_draw(bool p_will_draw);

	/* CAMERA API */

	struct Camera {
		enum Type {
			PERSPECTIVE,
			ORTHOGONAL,
			FRUSTUM
		};
		Type type;
		float fov;
		float znear, zfar;
		float size;
		Vector2 offset;
		uint32_t visible_layers;
		bool vaspect;
		RID env;
		RID attributes;
		RID compositor;

		Transform3D transform;

		Camera() {
			visible_layers = 0xFFFFFFFF;
			fov = 75;
			type = PERSPECTIVE;
			znear = 0.05;
			zfar = 4000;
			size = 1.0;
			offset = Vector2();
			vaspect = false;
		}
	};

	mutable RID_Owner<Camera, true> camera_owner;

	virtual RID camera_allocate();
	virtual void camera_initialize(RID p_rid);

	virtual void camera_set_perspective(RID p_camera, float p_fovy_degrees, float p_z_near, float p_z_far);
	virtual void camera_set_orthogonal(RID p_camera, float p_size, float p_z_near, float p_z_far);
	virtual void camera_set_frustum(RID p_camera, float p_size, Vector2 p_offset, float p_z_near, float p_z_far);
	virtual void camera_set_transform(RID p_camera, const Transform3D &p_transform);
	virtual void camera_set_cull_mask(RID p_camera, uint32_t p_layers);
	virtual void camera_set_environment(RID p_camera, RID p_env);
	virtual void camera_set_camera_attributes(RID p_camera, RID p_attributes);
	virtual void camera_set_compositor(RID p_camera, RID p_compositor);
	virtual void camera_set_use_vertical_aspect(RID p_camera, bool p_enable);
	virtual bool is_camera(RID p_camera) const;

	/* OCCLUDER API */

	virtual RID occluder_allocate();
	virtual void occluder_initialize(RID p_occluder);
	virtual void occluder_set_mesh(RID p_occluder, const PackedVector3Array &p_vertices, const PackedInt32Array &p_indices);

	/* VISIBILITY NOTIFIER API */

	RendererSceneOcclusionCull *dummy_occlusion_culling = nullptr;

	/* SCENARIO API */

	struct Instance;

	struct PlaneSign {
		_ALWAYS_INLINE_ PlaneSign() {}
		_ALWAYS_INLINE_ PlaneSign(const Plane &p_plane) {
			if (p_plane.normal.x > 0) {
				signs[0] = 0;
			} else {
				signs[0] = 3;
			}
			if (p_plane.normal.y > 0) {
				signs[1] = 1;
			} else {
				signs[1] = 4;
			}
			if (p_plane.normal.z > 0) {
				signs[2] = 2;
			} else {
				signs[2] = 5;
			}
		}

		uint32_t signs[3];
	};

	struct Frustum {
		Vector<Plane> planes;
		Vector<PlaneSign> plane_signs;
		const Plane *planes_ptr;
		const PlaneSign *plane_signs_ptr;
		uint32_t plane_count;

		_ALWAYS_INLINE_ Frustum() {}
		_ALWAYS_INLINE_ Frustum(const Frustum &p_frustum) {
			planes = p_frustum.planes;
			plane_signs = p_frustum.plane_signs;

			planes_ptr = planes.ptr();
			plane_signs_ptr = plane_signs.ptr();
			plane_count = p_frustum.plane_count;
		}
		_ALWAYS_INLINE_ void operator=(const Frustum &p_frustum) {
			planes = p_frustum.planes;
			plane_signs = p_frustum.plane_signs;

			planes_ptr = planes.ptr();
			plane_signs_ptr = plane_signs.ptr();
			plane_count = p_frustum.plane_count;
		}
		_ALWAYS_INLINE_ Frustum(const Vector<Plane> &p_planes) {
			planes = p_planes;
			planes_ptr = planes.ptrw();
			plane_count = planes.size();
			for (int i = 0; i < planes.size(); i++) {
				PlaneSign ps(p_planes[i]);
				plane_signs.push_back(ps);
			}

			plane_signs_ptr = plane_signs.ptr();
		}
	};

	struct InstanceBounds {
		// Efficiently store instance bounds.
		// Because bounds checking is performed first,
		// keep it separated from data.

		real_t bounds[6];
		_ALWAYS_INLINE_ InstanceBounds() {}

		_ALWAYS_INLINE_ InstanceBounds(const AABB &p_aabb) {
			bounds[0] = p_aabb.position.x;
			bounds[1] = p_aabb.position.y;
			bounds[2] = p_aabb.position.z;
			bounds[3] = p_aabb.position.x + p_aabb.size.x;
			bounds[4] = p_aabb.position.y + p_aabb.size.y;
			bounds[5] = p_aabb.position.z + p_aabb.size.z;
		}
		_ALWAYS_INLINE_ bool in_frustum(const Frustum &p_frustum) const {
			// This is not a full SAT check and the possibility of false positives exist,
			// but the tradeoff vs performance is still very good.

			for (uint32_t i = 0; i < p_frustum.plane_count; i++) {
				Vector3 min(
						bounds[p_frustum.plane_signs_ptr[i].signs[0]],
						bounds[p_frustum.plane_signs_ptr[i].signs[1]],
						bounds[p_frustum.plane_signs_ptr[i].signs[2]]);

				if (p_frustum.planes_ptr[i].distance_to(min) >= 0.0) {
					return false;
				}
			}

			return true;
		}
		_ALWAYS_INLINE_ bool in_aabb(const AABB &p_aabb) const {
			Vector3 end = p_aabb.position + p_aabb.size;

			if (bounds[0] >= end.x) {
				return false;
			}
			if (bounds[3] <= p_aabb.position.x) {
				return false;
			}
			if (bounds[1] >= end.y) {
				return false;
			}
			if (bounds[4] <= p_aabb.position.y) {
				return false;
			}
			if (bounds[2] >= end.z) {
				return false;
			}
			if (bounds[5] <= p_aabb.position.z) {
				return false;
			}

			return true;
		}
	};

	struct InstanceVisibilityNotifierData;

	struct InstanceData {
		// Store instance pointer as well as common instance processing information,
		// to make processing more cache friendly.
		enum Flags {
			FLAG_BASE_TYPE_MASK = 0xFF,
			FLAG_CAST_SHADOWS = (1 << 8),
			FLAG_CAST_SHADOWS_ONLY = (1 << 9),
			FLAG_REDRAW_IF_VISIBLE = (1 << 10),
			FLAG_GEOM_LIGHTING_DIRTY = (1 << 11),
			FLAG_GEOM_REFLECTION_DIRTY = (1 << 12),
			FLAG_GEOM_DECAL_DIRTY = (1 << 13),
			FLAG_GEOM_VOXEL_GI_DIRTY = (1 << 14),
			FLAG_LIGHTMAP_CAPTURE = (1 << 15),
			FLAG_USES_BAKED_LIGHT = (1 << 16),
			FLAG_USES_MESH_INSTANCE = (1 << 17),
			FLAG_REFLECTION_PROBE_DIRTY = (1 << 18),
			FLAG_IGNORE_OCCLUSION_CULLING = (1 << 19),
			FLAG_VISIBILITY_DEPENDENCY_NEEDS_CHECK = (3 << 20), // 2 bits, overlaps with the other vis. dependency flags
			FLAG_VISIBILITY_DEPENDENCY_HIDDEN_CLOSE_RANGE = (1 << 20),
			FLAG_VISIBILITY_DEPENDENCY_HIDDEN = (1 << 21),
			FLAG_VISIBILITY_DEPENDENCY_FADE_CHILDREN = (1 << 22),
			FLAG_GEOM_PROJECTOR_SOFTSHADOW_DIRTY = (1 << 23),
			FLAG_IGNORE_ALL_CULLING = (1 << 24),
		};

		uint32_t flags = 0;
		uint32_t layer_mask = 0; //for fast layer-mask discard
		RID base_rid;
		union {
			uint64_t instance_data_rid;
			RenderGeometryInstance *instance_geometry;
			InstanceVisibilityNotifierData *visibility_notifier = nullptr;
		};
		Instance *instance = nullptr;
		int32_t parent_array_index = -1;
		int32_t visibility_index = -1;

		// Each time occlusion culling determines an instance is visible,
		// set this to occlusion_frame plus some delay.
		// Once the timeout is reached, allow the instance to be occlusion culled.
		// This creates a delay for occlusion culling, which prevents flickering
		// when jittering the raster occlusion projection.
		uint64_t occlusion_timeout = 0;
	};

	struct InstanceVisibilityData {
		uint64_t viewport_state = 0;
		int32_t array_index = -1;
		RS::VisibilityRangeFadeMode fade_mode = RS::VISIBILITY_RANGE_FADE_DISABLED;
		Vector3 position;
		Instance *instance = nullptr;
		float range_begin = 0.0f;
		float range_end = 0.0f;
		float range_begin_margin = 0.0f;
		float range_end_margin = 0.0f;
		float children_fade_alpha = 1.0f;
	};

	class VisibilityArray : public BinSortedArray<InstanceVisibilityData> {
		_FORCE_INLINE_ virtual void _update_idx(InstanceVisibilityData &r_element, uint64_t p_idx) {
			r_element.instance->visibility_index = p_idx;
			if (r_element.instance->scenario && r_element.instance->array_index != -1) {
				r_element.instance->scenario->instance_data[r_element.instance->array_index].visibility_index = p_idx;
			}
		}
	};

	PagedArrayPool<InstanceBounds> instance_aabb_page_pool;
	PagedArrayPool<InstanceData> instance_data_page_pool;
	PagedArrayPool<InstanceVisibilityData> instance_visibility_data_page_pool;

	struct Scenario {
		enum IndexerType {
			INDEXER_GEOMETRY, //for geometry
			INDEXER_VOLUMES, //for everything else
			INDEXER_MAX
		};

		DynamicBVH indexers[INDEXER_MAX];

		RID self;

		List<Instance *> directional_lights;
		RID environment;
		RID fallback_environment;
		RID camera_attributes;
		RID compositor;
		RID reflection_probe_shadow_atlas;
		RID reflection_atlas;
		uint64_t used_viewport_visibility_bits;
		HashMap<RID, uint64_t> viewport_visibility_masks;

		SelfList<Instance>::List instances;

		LocalVector<RID> dynamic_lights;

		PagedArray<InstanceBounds> instance_aabbs;
		PagedArray<InstanceData> instance_data;
		VisibilityArray instance_visibility;

		Scenario() {
			indexers[INDEXER_GEOMETRY].set_index(INDEXER_GEOMETRY);
			indexers[INDEXER_VOLUMES].set_index(INDEXER_VOLUMES);
			used_viewport_visibility_bits = 0;
		}
	};

	int indexer_update_iterations = 0;

	mutable RID_Owner<Scenario, true> scenario_owner;

	static void _instance_pair(Instance *p_A, Instance *p_B);
	static void _instance_unpair(Instance *p_A, Instance *p_B);

	void _instance_update_mesh_instance(Instance *p_instance);

	virtual RID scenario_allocate();
	virtual void scenario_initialize(RID p_rid);

	virtual void scenario_set_environment(RID p_scenario, RID p_environment);
	virtual void scenario_set_camera_attributes(RID p_scenario, RID p_attributes);
	virtual void scenario_set_fallback_environment(RID p_scenario, RID p_environment);
	virtual void scenario_set_compositor(RID p_scenario, RID p_compositor);
	virtual void scenario_set_reflection_atlas_size(RID p_scenario, int p_reflection_size, int p_reflection_count);
	virtual bool is_scenario(RID p_scenario) const;
	virtual RID scenario_get_environment(RID p_scenario);
	virtual void scenario_add_viewport_visibility_mask(RID p_scenario, RID p_viewport);
	virtual void scenario_remove_viewport_visibility_mask(RID p_scenario, RID p_viewport);

	/* INSTANCING API */

	struct InstancePair {
		Instance *a = nullptr;
		Instance *b = nullptr;
		SelfList<InstancePair> list_a;
		SelfList<InstancePair> list_b;
		InstancePair() :
				list_a(this), list_b(this) {}
	};

	PagedAllocator<InstancePair> pair_allocator;

	struct InstanceBaseData {
		virtual ~InstanceBaseData() {}
	};

	struct Instance {
		RS::InstanceType base_type;
		RID base;

		RID skeleton;
		RID material_override;
		RID material_overlay;

		RID mesh_instance; //only used for meshes and when skeleton/blendshapes exist

		// This is the main transform to be drawn with ...
		// This will either be the interpolated transform (when using fixed timestep interpolation)
		// or the ONLY transform (when not using FTI).
		Transform3D transform;

		// For interpolation we store the current transform (this physics tick)
		// and the transform in the previous tick.
		Transform3D transform_curr;
		Transform3D transform_prev;

		float lod_bias;

		bool ignore_occlusion_culling;
		bool ignore_all_culling;

		Vector<RID> materials;

		RS::ShadowCastingSetting cast_shadows;

		uint32_t layer_mask;
		// Fit in 32 bits.
		bool mirror : 1;
		bool receive_shadows : 1;
		bool visible : 1;
		bool baked_light : 1; // This flag is only to know if it actually did use baked light.
		bool dynamic_gi : 1; // Same as above for dynamic objects.
		bool redraw_if_visible : 1;

		bool on_interpolate_list : 1;
		bool on_interpolate_transform_list : 1;
		bool interpolated : 1;
		TransformInterpolator::Method interpolation_method : 3;

		// For fixed timestep interpolation.
		// Note 32 bits is plenty for checksum, no need for real_t
		float transform_checksum_curr;
		float transform_checksum_prev;

		Instance *lightmap = nullptr;
		Rect2 lightmap_uv_scale;
		int lightmap_slice_index;
		uint32_t lightmap_cull_index;
		Vector<Color> lightmap_sh; //spherical harmonic

		AABB aabb;
		AABB transformed_aabb;
		AABB prev_transformed_aabb;

		struct InstanceShaderParameter {
			int32_t index = -1;
			Variant value;
			Variant default_value;
			PropertyInfo info;
		};

		HashMap<StringName, InstanceShaderParameter> instance_shader_uniforms;
		bool instance_allocated_shader_uniforms = false;
		int32_t instance_allocated_shader_uniforms_offset = -1;

		//

		RID self;
		//scenario stuff
		DynamicBVH::ID indexer_id;
		int32_t array_index = -1;
		int32_t visibility_index = -1;
		float visibility_range_begin = 0.0f;
		float visibility_range_end = 0.0f;
		float visibility_range_begin_margin = 0.0f;
		float visibility_range_end_margin = 0.0f;
		RS::VisibilityRangeFadeMode visibility_range_fade_mode = RS::VISIBILITY_RANGE_FADE_DISABLED;
		Instance *visibility_parent = nullptr;
		HashSet<Instance *> visibility_dependencies;
		uint32_t visibility_dependencies_depth = 0;
		float transparency = 0.0f;
		Scenario *scenario = nullptr;
		SelfList<Instance> scenario_item;

		//aabb stuff
		bool update_aabb;
		bool update_dependencies;

		SelfList<Instance> update_item;

		AABB *custom_aabb = nullptr; // <Zylann> would using aabb directly with a bool be better?
		float extra_margin;
		ObjectID object_id;

		// sorting
		float sorting_offset = 0.0;
		bool use_aabb_center = true;

		Vector<Color> lightmap_target_sh; //target is used for incrementally changing the SH over time, this avoids pops in some corner cases and when going interior <-> exterior

		uint64_t last_frame_pass;

		uint64_t version; // changes to this, and changes to base increase version

		InstanceBaseData *base_data = nullptr;

		SelfList<InstancePair>::List pairs;
		uint64_t pair_check;

		DependencyTracker dependency_tracker;

		static void dependency_changed(Dependency::DependencyChangedNotification p_notification, DependencyTracker *tracker) {
			Instance *instance = (Instance *)tracker->userdata;
			switch (p_notification) {
				case Dependency::DEPENDENCY_CHANGED_SKELETON_DATA:
				case Dependency::DEPENDENCY_CHANGED_SKELETON_BONES:
				case Dependency::DEPENDENCY_CHANGED_AABB: {
					singleton->_instance_queue_update(instance, true, false);

				} break;
				case Dependency::DEPENDENCY_CHANGED_MULTIMESH_VISIBLE_INSTANCES:
				case Dependency::DEPENDENCY_CHANGED_MATERIAL: {
					singleton->_instance_queue_update(instance, false, true);
				} break;
				case Dependency::DEPENDENCY_CHANGED_MESH:
				case Dependency::DEPENDENCY_CHANGED_PARTICLES:
				case Dependency::DEPENDENCY_CHANGED_MULTIMESH:
				case Dependency::DEPENDENCY_CHANGED_DECAL:
				case Dependency::DEPENDENCY_CHANGED_LIGHT:
				case Dependency::DEPENDENCY_CHANGED_REFLECTION_PROBE: {
					singleton->_instance_queue_update(instance, true, true);
				} break;
				case Dependency::DEPENDENCY_CHANGED_LIGHT_SOFT_SHADOW_AND_PROJECTOR: {
					//requires repairing
					if (instance->indexer_id.is_valid()) {
						singleton->_unpair_instance(instance);
						singleton->_instance_queue_update(instance, true, true);
					}

				} break;
				default: {
					// Ignored notifications.
				} break;
			}
		}

		static void dependency_deleted(const RID &p_dependency, DependencyTracker *tracker) {
			Instance *instance = (Instance *)tracker->userdata;

			if (p_dependency == instance->base) {
				singleton->instance_set_base(instance->self, RID());
			} else if (p_dependency == instance->skeleton) {
				singleton->instance_attach_skeleton(instance->self, RID());
			} else {
				// It's possible the same material is used in multiple slots,
				// so we check whether we need to clear them all.
				if (p_dependency == instance->material_override) {
					singleton->instance_geometry_set_material_override(instance->self, RID());
				}
				if (p_dependency == instance->material_overlay) {
					singleton->instance_geometry_set_material_overlay(instance->self, RID());
				}
				for (int i = 0; i < instance->materials.size(); i++) {
					if (p_dependency == instance->materials[i]) {
						singleton->instance_set_surface_override_material(instance->self, i, RID());
					}
				}
				if (instance->base_type == RS::INSTANCE_PARTICLES) {
					RID particle_material = RSG::particles_storage->particles_get_process_material(instance->base);
					if (p_dependency == particle_material) {
						RSG::particles_storage->particles_set_process_material(instance->base, RID());
					}
				}

				// Even if no change is made we still need to call `_instance_queue_update`.
				// This dependency could also be a result of the freed material being used
				// by the mesh this mesh instance uses.
				singleton->_instance_queue_update(instance, false, true);
			}
		}

		Instance() :
				scenario_item(this),
				update_item(this) {
			base_type = RS::INSTANCE_NONE;
			cast_shadows = RS::SHADOW_CASTING_SETTING_ON;
			receive_shadows = true;
			visible = true;
			layer_mask = 1;
			baked_light = true;
			dynamic_gi = false;
			redraw_if_visible = false;

			on_interpolate_list = false;
			on_interpolate_transform_list = false;
			interpolated = true;
			interpolation_method = TransformInterpolator::INTERP_LERP;
			transform_checksum_curr = 0.0;
			transform_checksum_prev = 0.0;

			lightmap_slice_index = 0;
			lightmap = nullptr;
			lightmap_cull_index = 0;
			lod_bias = 1.0;
			ignore_occlusion_culling = false;
			ignore_all_culling = false;

			scenario = nullptr;

			update_aabb = false;
			update_dependencies = false;

			extra_margin = 0;

			visible = true;

			visibility_range_begin = 0;
			visibility_range_end = 0;
			visibility_range_begin_margin = 0;
			visibility_range_end_margin = 0;

			last_frame_pass = 0;
			version = 1;
			base_data = nullptr;

			custom_aabb = nullptr;

			pair_check = 0;
			array_index = -1;

			dependency_tracker.userdata = this;
			dependency_tracker.changed_callback = dependency_changed;
			dependency_tracker.deleted_callback = dependency_deleted;
		}

		~Instance() {
			if (base_data) {
				memdelete(base_data);
			}
			if (custom_aabb) {
				memdelete(custom_aabb);
			}
		}
	};

	SelfList<Instance>::List _instance_update_list;
	void _instance_queue_update(Instance *p_instance, bool p_update_aabb, bool p_update_dependencies = false);

	struct InstanceGeometryData : public InstanceBaseData {
		RenderGeometryInstance *geometry_instance = nullptr;
		HashSet<Instance *> lights;
		bool can_cast_shadows;
		bool material_is_animated;
		uint32_t projector_count = 0;
		uint32_t softshadow_count = 0;

		HashSet<Instance *> decals;
		HashSet<Instance *> reflection_probes;
		HashSet<Instance *> voxel_gi_instances;
		HashSet<Instance *> lightmap_captures;

		InstanceGeometryData() {
			can_cast_shadows = true;
			material_is_animated = true;
		}
	};

	struct InstanceReflectionProbeData : public InstanceBaseData {
		Instance *owner = nullptr;

		HashSet<Instance *> geometries;

		RID instance;
		SelfList<InstanceReflectionProbeData> update_list;

		int render_step;

		InstanceReflectionProbeData() :
				update_list(this) {
			render_step = -1;
		}
	};

	struct InstanceDecalData : public InstanceBaseData {
		Instance *owner = nullptr;
		RID instance;

		HashSet<Instance *> geometries;

		InstanceDecalData() {
		}
	};

	SelfList<InstanceReflectionProbeData>::List reflection_probe_render_list;

	struct InstanceParticlesCollisionData : public InstanceBaseData {
		RID instance;
	};

	struct InstanceFogVolumeData : public InstanceBaseData {
		RID instance;
		bool is_global;
	};

	struct InstanceVisibilityNotifierData : public InstanceBaseData {
		bool just_visible = false;
		uint64_t visible_in_frame = 0;
		RID base;
		SelfList<InstanceVisibilityNotifierData> list_element;
		InstanceVisibilityNotifierData() :
				list_element(this) {}
	};

	SpinLock visible_notifier_list_lock;
	SelfList<InstanceVisibilityNotifierData>::List visible_notifier_list;

	struct InstanceLightData : public InstanceBaseData {
		RID instance;
		uint64_t last_version;
		List<Instance *>::Element *D; // directional light in scenario

		bool uses_projector = false;
		bool uses_softshadow = false;

		HashSet<Instance *> geometries;

		Instance *baked_light = nullptr;

		RS::LightBakeMode bake_mode;
		uint32_t max_sdfgi_cascade = 2;

	private:
		// Instead of a single dirty flag, we maintain a count
		// so that we can detect lights that are being made dirty
		// each frame, and switch on tighter caster culling.
		int32_t shadow_dirty_count;

		uint32_t light_update_frame_id;
		bool light_intersects_multiple_cameras;
		uint32_t light_intersects_multiple_cameras_timeout_frame_id;

	public:
		bool is_shadow_dirty() const { return shadow_dirty_count != 0; }
		void make_shadow_dirty() { shadow_dirty_count = light_intersects_multiple_cameras ? 1 : 2; }
		void detect_light_intersects_multiple_cameras(uint32_t p_frame_id) {
			// We need to detect the case where shadow updates are occurring
			// more than once per frame. In this case, we need to turn off
			// tighter caster culling, so situation reverts to one full shadow update
			// per frame (light_intersects_multiple_cameras is set).
			if (p_frame_id == light_update_frame_id) {
				light_intersects_multiple_cameras = true;
				light_intersects_multiple_cameras_timeout_frame_id = p_frame_id + 60;
			} else {
				// When shadow_volume_intersects_multiple_cameras is set, we
				// want to detect the situation this is no longer the case, via a timeout.
				// The system can go back to tighter caster culling in this situation.
				// Having a long-ish timeout prevents rapid cycling.
				if (light_intersects_multiple_cameras && (p_frame_id >= light_intersects_multiple_cameras_timeout_frame_id)) {
					light_intersects_multiple_cameras = false;
					light_intersects_multiple_cameras_timeout_frame_id = UINT32_MAX;
				}
			}
			light_update_frame_id = p_frame_id;
		}

		void decrement_shadow_dirty() {
			shadow_dirty_count--;
			DEV_ASSERT(shadow_dirty_count >= 0);
		}

		// Shadow updates can either full (everything in the shadow volume)
		// or closely culled to the camera frustum.
		bool is_shadow_update_full() const { return shadow_dirty_count == 0; }

		InstanceLightData() {
			bake_mode = RS::LIGHT_BAKE_DISABLED;
			D = nullptr;
			last_version = 0;
			baked_light = nullptr;

			shadow_dirty_count = 1;
			light_update_frame_id = UINT32_MAX;
			light_intersects_multiple_cameras_timeout_frame_id = UINT32_MAX;
			light_intersects_multiple_cameras = false;
		}
	};

	struct InstanceVoxelGIData : public InstanceBaseData {
		Instance *owner = nullptr;

		HashSet<Instance *> geometries;
		HashSet<Instance *> dynamic_geometries;

		HashSet<Instance *> lights;

		struct LightCache {
			RS::LightType type;
			Transform3D transform;
			Color color;
			float energy;
			float intensity;
			float bake_energy;
			float radius;
			float attenuation;
			float spot_angle;
			float spot_attenuation;
			bool has_shadow;
			RS::LightDirectionalSkyMode sky_mode;
		};

		Vector<LightCache> light_cache;
		Vector<RID> light_instances;

		RID probe_instance;

		bool invalid;
		uint32_t base_version;

		SelfList<InstanceVoxelGIData> update_element;

		InstanceVoxelGIData() :
				update_element(this) {
			invalid = true;
			base_version = 0;
		}
	};

	SelfList<InstanceVoxelGIData>::List voxel_gi_update_list;

	struct InstanceLightmapData : public InstanceBaseData {
		RID instance;
		HashSet<Instance *> geometries;
		HashSet<Instance *> users;

		InstanceLightmapData() {
		}
	};

	uint64_t pair_pass = 1;

	struct PairInstances {
		Instance *instance = nullptr;
		PagedAllocator<InstancePair> *pair_allocator = nullptr;
		SelfList<InstancePair>::List pairs_found;
		DynamicBVH *bvh = nullptr;
		DynamicBVH *bvh2 = nullptr; //some may need to cull in two
		uint32_t pair_mask;
		uint64_t pair_pass;
		uint32_t cull_mask = 0xFFFFFFFF; // Needed for decals and lights in the mobile and compatibility renderers.

		_FORCE_INLINE_ bool operator()(void *p_data) {
			Instance *p_instance = (Instance *)p_data;

			if (instance != p_instance && instance->transformed_aabb.intersects(p_instance->transformed_aabb) && (pair_mask & (1 << p_instance->base_type)) && (cull_mask & p_instance->layer_mask)) {
				//test is more coarse in indexer
				p_instance->pair_check = pair_pass;
				InstancePair *pair = pair_allocator->alloc();
				pair->a = instance;
				pair->b = p_instance;
				pairs_found.add(&pair->list_a);
			}
			return false;
		}

		void pair() {
			if (bvh) {
				bvh->aabb_query(instance->transformed_aabb, *this);
			}
			if (bvh2) {
				bvh2->aabb_query(instance->transformed_aabb, *this);
			}
			while (instance->pairs.first()) {
				InstancePair *pair = instance->pairs.first()->self();
				Instance *other_instance = instance == pair->a ? pair->b : pair->a;
				if (other_instance->pair_check != pair_pass) {
					//unpaired
					_instance_unpair(instance, other_instance);
				} else {
					//kept
					other_instance->pair_check = 0; // if kept, then put pair check to zero, so we can distinguish with the newly added ones
				}

				pair_allocator->free(pair);
			}
			while (pairs_found.first()) {
				InstancePair *pair = pairs_found.first()->self();
				pairs_found.remove(pairs_found.first());

				if (pair->b->pair_check == pair_pass) {
					//paired
					_instance_pair(instance, pair->b);
				}
				pair->a->pairs.add(&pair->list_a);
				pair->b->pairs.add(&pair->list_b);
			}
		}
	};

	HashSet<Instance *> heightfield_particle_colliders_update_list;

	PagedArrayPool<Instance *> instance_cull_page_pool;
	PagedArrayPool<RenderGeometryInstance *> geometry_instance_cull_page_pool;
	PagedArrayPool<RID> rid_cull_page_pool;

	PagedArray<Instance *> instance_cull_result;
	PagedArray<Instance *> instance_shadow_cull_result;

	struct InstanceCullResult {
		PagedArray<RenderGeometryInstance *> geometry_instances;
		PagedArray<Instance *> lights;
		PagedArray<RID> light_instances;
		PagedArray<RID> lightmaps;
		PagedArray<RID> reflections;
		PagedArray<RID> decals;
		PagedArray<RID> voxel_gi_instances;
		PagedArray<RID> mesh_instances;
		PagedArray<RID> fog_volumes;

		struct DirectionalShadow {
			PagedArray<RenderGeometryInstance *> cascade_geometry_instances[RendererSceneRender::MAX_DIRECTIONAL_LIGHT_CASCADES];
		} directional_shadows[RendererSceneRender::MAX_DIRECTIONAL_LIGHTS];

		PagedArray<RenderGeometryInstance *> sdfgi_region_geometry_instances[SDFGI_MAX_CASCADES * SDFGI_MAX_REGIONS_PER_CASCADE];
		PagedArray<RID> sdfgi_cascade_lights[SDFGI_MAX_CASCADES];

		void clear() {
			geometry_instances.clear();
			lights.clear();
			light_instances.clear();
			lightmaps.clear();
			reflections.clear();
			decals.clear();
			voxel_gi_instances.clear();
			mesh_instances.clear();
			fog_volumes.clear();
			for (int i = 0; i < RendererSceneRender::MAX_DIRECTIONAL_LIGHTS; i++) {
				for (int j = 0; j < RendererSceneRender::MAX_DIRECTIONAL_LIGHT_CASCADES; j++) {
					directional_shadows[i].cascade_geometry_instances[j].clear();
				}
			}

			for (int i = 0; i < SDFGI_MAX_CASCADES * SDFGI_MAX_REGIONS_PER_CASCADE; i++) {
				sdfgi_region_geometry_instances[i].clear();
			}

			for (int i = 0; i < SDFGI_MAX_CASCADES; i++) {
				sdfgi_cascade_lights[i].clear();
			}
		}

		void reset() {
			geometry_instances.reset();
			lights.reset();
			light_instances.reset();
			lightmaps.reset();
			reflections.reset();
			decals.reset();
			voxel_gi_instances.reset();
			mesh_instances.reset();
			fog_volumes.reset();
			for (int i = 0; i < RendererSceneRender::MAX_DIRECTIONAL_LIGHTS; i++) {
				for (int j = 0; j < RendererSceneRender::MAX_DIRECTIONAL_LIGHT_CASCADES; j++) {
					directional_shadows[i].cascade_geometry_instances[j].reset();
				}
			}

			for (int i = 0; i < SDFGI_MAX_CASCADES * SDFGI_MAX_REGIONS_PER_CASCADE; i++) {
				sdfgi_region_geometry_instances[i].reset();
			}

			for (int i = 0; i < SDFGI_MAX_CASCADES; i++) {
				sdfgi_cascade_lights[i].reset();
			}
		}

		void append_from(InstanceCullResult &p_cull_result) {
			geometry_instances.merge_unordered(p_cull_result.geometry_instances);
			lights.merge_unordered(p_cull_result.lights);
			light_instances.merge_unordered(p_cull_result.light_instances);
			lightmaps.merge_unordered(p_cull_result.lightmaps);
			reflections.merge_unordered(p_cull_result.reflections);
			decals.merge_unordered(p_cull_result.decals);
			voxel_gi_instances.merge_unordered(p_cull_result.voxel_gi_instances);
			mesh_instances.merge_unordered(p_cull_result.mesh_instances);
			fog_volumes.merge_unordered(p_cull_result.fog_volumes);

			for (int i = 0; i < RendererSceneRender::MAX_DIRECTIONAL_LIGHTS; i++) {
				for (int j = 0; j < RendererSceneRender::MAX_DIRECTIONAL_LIGHT_CASCADES; j++) {
					directional_shadows[i].cascade_geometry_instances[j].merge_unordered(p_cull_result.directional_shadows[i].cascade_geometry_instances[j]);
				}
			}

			for (int i = 0; i < SDFGI_MAX_CASCADES * SDFGI_MAX_REGIONS_PER_CASCADE; i++) {
				sdfgi_region_geometry_instances[i].merge_unordered(p_cull_result.sdfgi_region_geometry_instances[i]);
			}

			for (int i = 0; i < SDFGI_MAX_CASCADES; i++) {
				sdfgi_cascade_lights[i].merge_unordered(p_cull_result.sdfgi_cascade_lights[i]);
			}
		}

		void init(PagedArrayPool<RID> *p_rid_pool, PagedArrayPool<RenderGeometryInstance *> *p_geometry_instance_pool, PagedArrayPool<Instance *> *p_instance_pool) {
			geometry_instances.set_page_pool(p_geometry_instance_pool);
			light_instances.set_page_pool(p_rid_pool);
			lights.set_page_pool(p_instance_pool);
			lightmaps.set_page_pool(p_rid_pool);
			reflections.set_page_pool(p_rid_pool);
			decals.set_page_pool(p_rid_pool);
			voxel_gi_instances.set_page_pool(p_rid_pool);
			mesh_instances.set_page_pool(p_rid_pool);
			fog_volumes.set_page_pool(p_rid_pool);
			for (int i = 0; i < RendererSceneRender::MAX_DIRECTIONAL_LIGHTS; i++) {
				for (int j = 0; j < RendererSceneRender::MAX_DIRECTIONAL_LIGHT_CASCADES; j++) {
					directional_shadows[i].cascade_geometry_instances[j].set_page_pool(p_geometry_instance_pool);
				}
			}

			for (int i = 0; i < SDFGI_MAX_CASCADES * SDFGI_MAX_REGIONS_PER_CASCADE; i++) {
				sdfgi_region_geometry_instances[i].set_page_pool(p_geometry_instance_pool);
			}

			for (int i = 0; i < SDFGI_MAX_CASCADES; i++) {
				sdfgi_cascade_lights[i].set_page_pool(p_rid_pool);
			}
		}
	};

	InstanceCullResult scene_cull_result;
	LocalVector<InstanceCullResult> scene_cull_result_threads;

	RendererSceneRender::RenderShadowData render_shadow_data[MAX_UPDATE_SHADOWS];
	uint32_t max_shadows_used = 0;

	RendererSceneRender::RenderSDFGIData render_sdfgi_data[SDFGI_MAX_CASCADES * SDFGI_MAX_REGIONS_PER_CASCADE];
	RendererSceneRender::RenderSDFGIUpdateData sdfgi_update_data;

	uint32_t thread_cull_threshold = 200;

	RID_Owner<Instance, true> instance_owner;

	uint32_t geometry_instance_pair_mask = 0; // used in traditional forward, unnecessary on clustered

	LocalVector<Vector2> camera_jitter_array;
	RenderingLightCuller *light_culler = nullptr;

	virtual RID instance_allocate();
	virtual void instance_initialize(RID p_rid);

	virtual void instance_set_base(RID p_instance, RID p_base);
	virtual void instance_set_scenario(RID p_instance, RID p_scenario);
	virtual void instance_set_layer_mask(RID p_instance, uint32_t p_mask);
	virtual void instance_set_pivot_data(RID p_instance, float p_sorting_offset, bool p_use_aabb_center);
	virtual void instance_set_transform(RID p_instance, const Transform3D &p_transform);
	virtual void instance_set_interpolated(RID p_instance, bool p_interpolated);
	virtual void instance_reset_physics_interpolation(RID p_instance);
	virtual void instance_attach_object_instance_id(RID p_instance, ObjectID p_id);
	virtual void instance_set_blend_shape_weight(RID p_instance, int p_shape, float p_weight);
	virtual void instance_set_surface_override_material(RID p_instance, int p_surface, RID p_material);
	virtual void instance_set_visible(RID p_instance, bool p_visible);
	virtual void instance_geometry_set_transparency(RID p_instance, float p_transparency);

	virtual void instance_set_custom_aabb(RID p_instance, AABB p_aabb);

	virtual void instance_attach_skeleton(RID p_instance, RID p_skeleton);

	virtual void instance_set_extra_visibility_margin(RID p_instance, real_t p_margin);

	virtual void instance_set_visibility_parent(RID p_instance, RID p_parent_instance);

	virtual void instance_set_ignore_culling(RID p_instance, bool p_enabled);

	bool _update_instance_visibility_depth(Instance *p_instance);
	void _update_instance_visibility_dependencies(Instance *p_instance);

	// don't use these in a game!
	virtual Vector<ObjectID> instances_cull_aabb(const AABB &p_aabb, RID p_scenario = RID()) const;
	virtual Vector<ObjectID> instances_cull_ray(const Vector3 &p_from, const Vector3 &p_to, RID p_scenario = RID()) const;
	virtual Vector<ObjectID> instances_cull_convex(const Vector<Plane> &p_convex, RID p_scenario = RID()) const;

	virtual void instance_geometry_set_flag(RID p_instance, RS::InstanceFlags p_flags, bool p_enabled);
	virtual void instance_geometry_set_cast_shadows_setting(RID p_instance, RS::ShadowCastingSetting p_shadow_casting_setting);
	virtual void instance_geometry_set_material_override(RID p_instance, RID p_material);
	virtual void instance_geometry_set_material_overlay(RID p_instance, RID p_material);

	virtual void instance_geometry_set_visibility_range(RID p_instance, float p_min, float p_max, float p_min_margin, float p_max_margin, RS::VisibilityRangeFadeMode p_fade_mode);

	virtual void instance_geometry_set_lightmap(RID p_instance, RID p_lightmap, const Rect2 &p_lightmap_uv_scale, int p_slice_index);
	virtual void instance_geometry_set_lod_bias(RID p_instance, float p_lod_bias);

	void _update_instance_shader_uniforms_from_material(HashMap<StringName, Instance::InstanceShaderParameter> &isparams, const HashMap<StringName, Instance::InstanceShaderParameter> &existing_isparams, RID p_material);

	virtual void instance_geometry_set_shader_parameter(RID p_instance, const StringName &p_parameter, const Variant &p_value);
	virtual void instance_geometry_get_shader_parameter_list(RID p_instance, List<PropertyInfo> *p_parameters) const;
	virtual Variant instance_geometry_get_shader_parameter(RID p_instance, const StringName &p_parameter) const;
	virtual Variant instance_geometry_get_shader_parameter_default_value(RID p_instance, const StringName &p_parameter) const;

	virtual void mesh_generate_pipelines(RID p_mesh, bool p_background_compilation);
	virtual uint32_t get_pipeline_compilations(RS::PipelineSource p_source);

	_FORCE_INLINE_ void _update_instance(Instance *p_instance);
	_FORCE_INLINE_ void _update_instance_aabb(Instance *p_instance);
	_FORCE_INLINE_ void _update_dirty_instance(Instance *p_instance);
	_FORCE_INLINE_ void _update_instance_lightmap_captures(Instance *p_instance);
	void _unpair_instance(Instance *p_instance);

	void _light_instance_setup_directional_shadow(int p_shadow_index, Instance *p_instance, const Transform3D p_cam_transform, const Projection &p_cam_projection, bool p_cam_orthogonal, bool p_cam_vaspect);

	_FORCE_INLINE_ bool _light_instance_update_shadow(Instance *p_instance, const Transform3D p_cam_transform, const Projection &p_cam_projection, bool p_cam_orthogonal, bool p_cam_vaspect, RID p_shadow_atlas, Scenario *p_scenario, float p_scren_mesh_lod_threshold, uint32_t p_visible_layers = 0xFFFFFF);

	RID _render_get_environment(RID p_camera, RID p_scenario);
	RID _render_get_compositor(RID p_camera, RID p_scenario);

	struct Cull {
		struct Shadow {
			RID light_instance;
			struct Cascade {
				Frustum frustum;

				Projection projection;
				Transform3D transform;
				real_t zfar;
				real_t split;
				real_t shadow_texel_size;
				real_t bias_scale;
				real_t range_begin;
				Vector2 uv_scale;

			} cascades[RendererSceneRender::MAX_DIRECTIONAL_LIGHT_CASCADES]; //max 4 cascades
			uint32_t cascade_count;

		} shadows[RendererSceneRender::MAX_DIRECTIONAL_LIGHTS];

		uint32_t shadow_count;

		struct SDFGI {
			//have arrays here because SDFGI functions expects this, plus regions can have areas
			AABB region_aabb[SDFGI_MAX_CASCADES * SDFGI_MAX_REGIONS_PER_CASCADE]; //max 3 regions per cascade
			uint32_t region_cascade[SDFGI_MAX_CASCADES * SDFGI_MAX_REGIONS_PER_CASCADE]; //max 3 regions per cascade
			uint32_t region_count = 0;

			uint32_t cascade_light_index[SDFGI_MAX_CASCADES];
			uint32_t cascade_light_count = 0;

		} sdfgi;

		SpinLock lock;

		Frustum frustum;
	} cull;

	struct VisibilityCullData {
		uint64_t viewport_mask;
		Scenario *scenario = nullptr;
		Vector3 camera_position;
		uint32_t cull_offset;
		uint32_t cull_count;
	};

	void _visibility_cull_threaded(uint32_t p_thread, VisibilityCullData *cull_data);
	void _visibility_cull(const VisibilityCullData &cull_data, uint64_t p_from, uint64_t p_to);
	template <bool p_fade_check>
	_FORCE_INLINE_ int _visibility_range_check(InstanceVisibilityData &r_vis_data, const Vector3 &p_camera_pos, uint64_t p_viewport_mask);

	struct CullData {
		Cull *cull = nullptr;
		Scenario *scenario = nullptr;
		RID shadow_atlas;
		Transform3D cam_transform;
		uint32_t visible_layers;
		Instance *render_reflection_probe = nullptr;
		const RendererSceneOcclusionCull::HZBuffer *occlusion_buffer;
		const Projection *camera_matrix;
		uint64_t visibility_viewport_mask;
	};

	void _scene_cull_threaded(uint32_t p_thread, CullData *cull_data);
	void _scene_cull(CullData &cull_data, InstanceCullResult &cull_result, uint64_t p_from, uint64_t p_to);
	_FORCE_INLINE_ bool _visibility_parent_check(const CullData &p_cull_data, const InstanceData &p_instance_data);

	bool _render_reflection_probe_step(Instance *p_instance, int p_step);
	void _render_scene(const RendererSceneRender::CameraData *p_camera_data, const Ref<RenderSceneBuffers> &p_render_buffers, RID p_environment, RID p_force_camera_attributes, RID p_compositor, uint32_t p_visible_layers, RID p_scenario, RID p_viewport, RID p_shadow_atlas, RID p_reflection_probe, int p_reflection_probe_pass, float p_screen_mesh_lod_threshold, bool p_using_shadows = true, RenderInfo *r_render_info = nullptr);
	void render_empty_scene(const Ref<RenderSceneBuffers> &p_render_buffers, RID p_scenario, RID p_shadow_atlas);

	void render_camera(const Ref<RenderSceneBuffers> &p_render_buffers, RID p_camera, RID p_scenario, RID p_viewport, Size2 p_viewport_size, uint32_t p_jitter_phase_count, float p_screen_mesh_lod_threshold, RID p_shadow_atlas, Ref<XRInterface> &p_xr_interface, RenderingMethod::RenderInfo *r_render_info = nullptr);
	void update_dirty_instances();

	void render_particle_colliders();
	virtual void render_probes();

	TypedArray<Image> bake_render_uv2(RID p_base, const TypedArray<RID> &p_material_overrides, const Size2i &p_image_size);

	//pass to scene render

	/* ENVIRONMENT API */

#ifdef PASSBASE
#undef PASSBASE
#endif

#define PASSBASE scene_render

	PASS1(voxel_gi_set_quality, RS::VoxelGIQuality)

	/* SKY API */

	PASS0R(RID, sky_allocate)
	PASS1(sky_initialize, RID)

	PASS2(sky_set_radiance_size, RID, int)
	PASS2(sky_set_mode, RID, RS::SkyMode)
	PASS2(sky_set_material, RID, RID)
	PASS4R(Ref<Image>, sky_bake_panorama, RID, float, bool, const Size2i &)

	// Compositor effect

	PASS0R(RID, compositor_effect_allocate)
	PASS1(compositor_effect_initialize, RID)

	PASS1RC(bool, is_compositor_effect, RID)

	PASS2(compositor_effect_set_enabled, RID, bool)
	PASS3(compositor_effect_set_callback, RID, RS::CompositorEffectCallbackType, const Callable &)
	PASS3(compositor_effect_set_flag, RID, RS::CompositorEffectFlags, bool)

	// Compositor

	PASS0R(RID, compositor_allocate)
	PASS1(compositor_initialize, RID)

	PASS1RC(bool, is_compositor, RID)

	PASS2(compositor_set_compositor_effects, RID, const TypedArray<RID> &)

	// Environment

	PASS0R(RID, environment_allocate)
	PASS1(environment_initialize, RID)

	PASS1RC(bool, is_environment, RID)

	// Background
	PASS2(environment_set_background, RID, RS::EnvironmentBG)
	PASS2(environment_set_sky, RID, RID)
	PASS2(environment_set_sky_custom_fov, RID, float)
	PASS2(environment_set_sky_orientation, RID, const Basis &)
	PASS2(environment_set_bg_color, RID, const Color &)
	PASS3(environment_set_bg_energy, RID, float, float)
	PASS2(environment_set_canvas_max_layer, RID, int)
	PASS6(environment_set_ambient_light, RID, const Color &, RS::EnvironmentAmbientSource, float, float, RS::EnvironmentReflectionSource)

	PASS1RC(RS::EnvironmentBG, environment_get_background, RID)
	PASS1RC(RID, environment_get_sky, RID)
	PASS1RC(float, environment_get_sky_custom_fov, RID)
	PASS1RC(Basis, environment_get_sky_orientation, RID)
	PASS1RC(Color, environment_get_bg_color, RID)
	PASS1RC(float, environment_get_bg_energy_multiplier, RID)
	PASS1RC(float, environment_get_bg_intensity, RID)
	PASS1RC(int, environment_get_canvas_max_layer, RID)
	PASS1RC(RS::EnvironmentAmbientSource, environment_get_ambient_source, RID)
	PASS1RC(Color, environment_get_ambient_light, RID)
	PASS1RC(float, environment_get_ambient_light_energy, RID)
	PASS1RC(float, environment_get_ambient_sky_contribution, RID)
	PASS1RC(RS::EnvironmentReflectionSource, environment_get_reflection_source, RID)

	// Tonemap
	PASS4(environment_set_tonemap, RID, RS::EnvironmentToneMapper, float, float)
	PASS1RC(RS::EnvironmentToneMapper, environment_get_tone_mapper, RID)
	PASS1RC(float, environment_get_exposure, RID)
	PASS1RC(float, environment_get_white, RID)

	// Fog
	PASS11(environment_set_fog, RID, bool, const Color &, float, float, float, float, float, float, float, RS::EnvironmentFogMode)

	PASS1RC(bool, environment_get_fog_enabled, RID)
	PASS1RC(Color, environment_get_fog_light_color, RID)
	PASS1RC(float, environment_get_fog_light_energy, RID)
	PASS1RC(float, environment_get_fog_sun_scatter, RID)
	PASS1RC(float, environment_get_fog_density, RID)
	PASS1RC(float, environment_get_fog_sky_affect, RID)
	PASS1RC(float, environment_get_fog_height, RID)
	PASS1RC(float, environment_get_fog_height_density, RID)
	PASS1RC(float, environment_get_fog_aerial_perspective, RID)
	PASS1RC(RS::EnvironmentFogMode, environment_get_fog_mode, RID)

	PASS2(environment_set_volumetric_fog_volume_size, int, int)
	PASS1(environment_set_volumetric_fog_filter_active, bool)

	// Depth Fog
	PASS4(environment_set_fog_depth, RID, float, float, float)
	PASS1RC(float, environment_get_fog_depth_curve, RID)
	PASS1RC(float, environment_get_fog_depth_begin, RID)
	PASS1RC(float, environment_get_fog_depth_end, RID)

	// Volumentric Fog
	PASS14(environment_set_volumetric_fog, RID, bool, float, const Color &, const Color &, float, float, float, float, float, bool, float, float, float)

	PASS1RC(bool, environment_get_volumetric_fog_enabled, RID)
	PASS1RC(float, environment_get_volumetric_fog_density, RID)
	PASS1RC(Color, environment_get_volumetric_fog_scattering, RID)
	PASS1RC(Color, environment_get_volumetric_fog_emission, RID)
	PASS1RC(float, environment_get_volumetric_fog_emission_energy, RID)
	PASS1RC(float, environment_get_volumetric_fog_anisotropy, RID)
	PASS1RC(float, environment_get_volumetric_fog_length, RID)
	PASS1RC(float, environment_get_volumetric_fog_detail_spread, RID)
	PASS1RC(float, environment_get_volumetric_fog_gi_inject, RID)
	PASS1RC(float, environment_get_volumetric_fog_sky_affect, RID)
	PASS1RC(bool, environment_get_volumetric_fog_temporal_reprojection, RID)
	PASS1RC(float, environment_get_volumetric_fog_temporal_reprojection_amount, RID)
	PASS1RC(float, environment_get_volumetric_fog_ambient_inject, RID)

	// Glow
	PASS13(environment_set_glow, RID, bool, Vector<float>, float, float, float, float, RS::EnvironmentGlowBlendMode, float, float, float, float, RID)

	PASS1RC(bool, environment_get_glow_enabled, RID)
	PASS1RC(Vector<float>, environment_get_glow_levels, RID)
	PASS1RC(float, environment_get_glow_intensity, RID)
	PASS1RC(float, environment_get_glow_strength, RID)
	PASS1RC(float, environment_get_glow_bloom, RID)
	PASS1RC(float, environment_get_glow_mix, RID)
	PASS1RC(RS::EnvironmentGlowBlendMode, environment_get_glow_blend_mode, RID)
	PASS1RC(float, environment_get_glow_hdr_bleed_threshold, RID)
	PASS1RC(float, environment_get_glow_hdr_luminance_cap, RID)
	PASS1RC(float, environment_get_glow_hdr_bleed_scale, RID)
	PASS1RC(float, environment_get_glow_map_strength, RID)
	PASS1RC(RID, environment_get_glow_map, RID)

	PASS1(environment_glow_set_use_bicubic_upscale, bool)

	// SSR
	PASS6(environment_set_ssr, RID, bool, int, float, float, float)

	PASS1RC(bool, environment_get_ssr_enabled, RID)
	PASS1RC(int, environment_get_ssr_max_steps, RID)
	PASS1RC(float, environment_get_ssr_fade_in, RID)
	PASS1RC(float, environment_get_ssr_fade_out, RID)
	PASS1RC(float, environment_get_ssr_depth_tolerance, RID)

	PASS1(environment_set_ssr_roughness_quality, RS::EnvironmentSSRRoughnessQuality)

	// SSAO
	PASS10(environment_set_ssao, RID, bool, float, float, float, float, float, float, float, float)

	PASS1RC(bool, environment_get_ssao_enabled, RID)
	PASS1RC(float, environment_get_ssao_radius, RID)
	PASS1RC(float, environment_get_ssao_intensity, RID)
	PASS1RC(float, environment_get_ssao_power, RID)
	PASS1RC(float, environment_get_ssao_detail, RID)
	PASS1RC(float, environment_get_ssao_horizon, RID)
	PASS1RC(float, environment_get_ssao_sharpness, RID)
	PASS1RC(float, environment_get_ssao_direct_light_affect, RID)
	PASS1RC(float, environment_get_ssao_ao_channel_affect, RID)

	PASS6(environment_set_ssao_quality, RS::EnvironmentSSAOQuality, bool, float, int, float, float)

	// SSIL
	PASS6(environment_set_ssil, RID, bool, float, float, float, float)

	PASS1RC(bool, environment_get_ssil_enabled, RID)
	PASS1RC(float, environment_get_ssil_radius, RID)
	PASS1RC(float, environment_get_ssil_intensity, RID)
	PASS1RC(float, environment_get_ssil_sharpness, RID)
	PASS1RC(float, environment_get_ssil_normal_rejection, RID)

	PASS6(environment_set_ssil_quality, RS::EnvironmentSSILQuality, bool, float, int, float, float)

	// SDFGI

	PASS11(environment_set_sdfgi, RID, bool, int, float, RS::EnvironmentSDFGIYScale, bool, float, bool, float, float, float)

	PASS1RC(bool, environment_get_sdfgi_enabled, RID)
	PASS1RC(int, environment_get_sdfgi_cascades, RID)
	PASS1RC(float, environment_get_sdfgi_min_cell_size, RID)
	PASS1RC(bool, environment_get_sdfgi_use_occlusion, RID)
	PASS1RC(float, environment_get_sdfgi_bounce_feedback, RID)
	PASS1RC(bool, environment_get_sdfgi_read_sky_light, RID)
	PASS1RC(float, environment_get_sdfgi_energy, RID)
	PASS1RC(float, environment_get_sdfgi_normal_bias, RID)
	PASS1RC(float, environment_get_sdfgi_probe_bias, RID)
	PASS1RC(RS::EnvironmentSDFGIYScale, environment_get_sdfgi_y_scale, RID)

	PASS1(environment_set_sdfgi_ray_count, RS::EnvironmentSDFGIRayCount)
	PASS1(environment_set_sdfgi_frames_to_converge, RS::EnvironmentSDFGIFramesToConverge)
	PASS1(environment_set_sdfgi_frames_to_update_light, RS::EnvironmentSDFGIFramesToUpdateLight)

	// Adjustment
	PASS7(environment_set_adjustment, RID, bool, float, float, float, bool, RID)

	PASS1RC(bool, environment_get_adjustments_enabled, RID)
	PASS1RC(float, environment_get_adjustments_brightness, RID)
	PASS1RC(float, environment_get_adjustments_contrast, RID)
	PASS1RC(float, environment_get_adjustments_saturation, RID)
	PASS1RC(bool, environment_get_use_1d_color_correction, RID)
	PASS1RC(RID, environment_get_color_correction, RID)

	PASS3R(Ref<Image>, environment_bake_panorama, RID, bool, const Size2i &)

	PASS3(screen_space_roughness_limiter_set_active, bool, float, float)
	PASS1(sub_surface_scattering_set_quality, RS::SubSurfaceScatteringQuality)
	PASS2(sub_surface_scattering_set_scale, float, float)

	PASS1(positional_soft_shadow_filter_set_quality, RS::ShadowQuality)
	PASS1(directional_soft_shadow_filter_set_quality, RS::ShadowQuality)

	PASS2(sdfgi_set_debug_probe_select, const Vector3 &, const Vector3 &)

	/* Render Buffers */

	PASS0R(Ref<RenderSceneBuffers>, render_buffers_create)
	PASS1(gi_set_use_half_resolution, bool)

	/* Misc */
	PASS1(set_debug_draw_mode, RS::ViewportDebugDraw)

	PASS1(decals_set_filter, RS::DecalFilter)
	PASS1(light_projectors_set_filter, RS::LightProjectorFilter)
	PASS1(lightmaps_set_bicubic_filter, bool)

	virtual void update();

	bool free(RID p_rid);

	void set_scene_render(RendererSceneRender *p_scene_render);

	virtual void update_visibility_notifiers();

	/* INTERPOLATION */

	void update_interpolation_tick(bool p_process = true);
	void update_interpolation_frame(bool p_process = true);
	virtual void set_physics_interpolation_enabled(bool p_enabled);

	struct InterpolationData {
		void notify_free_instance(RID p_rid, Instance &r_instance);
		LocalVector<RID> instance_interpolate_update_list;
		LocalVector<RID> instance_transform_update_lists[2];
		LocalVector<RID> *instance_transform_update_list_curr = &instance_transform_update_lists[0];
		LocalVector<RID> *instance_transform_update_list_prev = &instance_transform_update_lists[1];

		bool interpolation_enabled = false;
	} _interpolation_data;

	RendererSceneCull();
	virtual ~RendererSceneCull();
};

#endif // RENDERER_SCENE_CULL_H

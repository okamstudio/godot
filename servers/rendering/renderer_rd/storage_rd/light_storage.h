/**************************************************************************/
/*  light_storage.h                                                       */
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

#ifndef LIGHT_STORAGE_RD_H
#define LIGHT_STORAGE_RD_H

#include "core/templates/local_vector.h"
#include "core/templates/paged_array.h"
#include "core/templates/rid_owner.h"
#include "core/templates/self_list.h"
#include "servers/rendering/renderer_rd/cluster_builder_rd.h"
#include "servers/rendering/renderer_rd/environment/sky.h"
#include "servers/rendering/renderer_rd/storage_rd/forward_id_storage.h"
#include "servers/rendering/renderer_rd/storage_rd/render_scene_buffers_rd.h"
#include "servers/rendering/renderer_rd/storage_rd/texture_storage.h"
#include "servers/rendering/storage/light_storage.h"
#include "servers/rendering/storage/utilities.h"

class RenderDataRD;

namespace RendererRD {

class LightStorage : public RendererLightStorage {
public:
	enum ShadowAtlastQuadrant : uint32_t {
		QUADRANT_SHIFT = 27,
		OMNI_LIGHT_FLAG = 1 << 26,
		SHADOW_INDEX_MASK = OMNI_LIGHT_FLAG - 1,
		SHADOW_INVALID = 0xFFFFFFFF
	};

private:
	static LightStorage *singleton;
	uint32_t max_cluster_elements = 512;

	/* LIGHT */
	struct Light {
		RS::LightType type;
		float param[RS::LIGHT_PARAM_MAX];
		Color color = Color(1, 1, 1, 1);
		RID projector;
		bool shadow = false;
		bool negative = false;
		bool reverse_cull = false;
		RS::LightBakeMode bake_mode = RS::LIGHT_BAKE_DYNAMIC;
		uint32_t max_sdfgi_cascade = 2;
		uint32_t cull_mask = 0xFFFFFFFF;
		uint32_t shadow_caster_mask = 0xFFFFFFFF;
		bool distance_fade = false;
		real_t distance_fade_begin = 40.0;
		real_t distance_fade_shadow = 50.0;
		real_t distance_fade_length = 10.0;
		RS::LightOmniShadowMode omni_shadow_mode = RS::LIGHT_OMNI_SHADOW_DUAL_PARABOLOID;
		RS::LightDirectionalShadowMode directional_shadow_mode = RS::LIGHT_DIRECTIONAL_SHADOW_ORTHOGONAL;
		bool directional_blend_splits = false;
		RS::LightDirectionalSkyMode directional_sky_mode = RS::LIGHT_DIRECTIONAL_SKY_MODE_LIGHT_AND_SKY;
		uint64_t version = 0;

		Dependency dependency;
	};

	mutable RID_Owner<Light, true> light_owner;

	/* LIGHT INSTANCE */

	struct LightInstance {
		struct ShadowTransform {
			Projection camera;
			Transform3D transform;
			float farplane = 0.0;
			float split = 0.0;
			float bias_scale = 0.0;
			float shadow_texel_size = 0.0;
			float range_begin = 0.0;
			Rect2 atlas_rect;
			Vector2 uv_scale;
		};

		RS::LightType light_type = RS::LIGHT_DIRECTIONAL;

		ShadowTransform shadow_transform[6];

		AABB aabb;
		RID self;
		RID light;
		Transform3D transform;

		Vector3 light_vector;
		Vector3 spot_vector;
		float linear_att = 0.0;

		uint64_t shadow_pass = 0;
		uint64_t last_scene_pass = 0;
		uint64_t last_scene_shadow_pass = 0;
		uint64_t last_pass = 0;
		uint32_t cull_mask = 0;
		uint32_t light_directional_index = 0;

		Rect2 directional_rect;

		HashSet<RID> shadow_atlases; //shadow atlases where this light is registered

		ForwardID forward_id = -1;

		LightInstance() {}
	};

	mutable RID_Owner<LightInstance> light_instance_owner;

	/* OMNI/SPOT LIGHT DATA */

	struct LightData {
		float position[3];
		float inv_radius;
		float direction[3]; // in omni, x and y are used for dual paraboloid offset
		float size;

		float color[3];
		float attenuation;

		float inv_spot_attenuation;
		float cos_spot_angle;
		float specular_amount;
		float shadow_opacity;

		float atlas_rect[4]; // in omni, used for atlas uv, in spot, used for projector uv
		float shadow_matrix[16];
		float shadow_bias;
		float shadow_normal_bias;
		float transmittance_bias;
		float soft_shadow_size;
		float soft_shadow_scale;
		uint32_t mask;
		float volumetric_fog_energy;
		uint32_t bake_mode;
		float projector_rect[4];
	};

	struct LightInstanceDepthSort {
		float depth;
		LightInstance *light_instance;
		Light *light;
		bool operator<(const LightInstanceDepthSort &p_sort) const {
			return depth < p_sort.depth;
		}
	};

	uint32_t max_lights;
	uint32_t omni_light_count = 0;
	uint32_t spot_light_count = 0;
	LightData *omni_lights = nullptr;
	LightData *spot_lights = nullptr;
	LightInstanceDepthSort *omni_light_sort = nullptr;
	LightInstanceDepthSort *spot_light_sort = nullptr;
	RID omni_light_buffer;
	RID spot_light_buffer;

	/* DIRECTIONAL LIGHT DATA */

	struct DirectionalLightData {
		float direction[3];
		float energy;
		float color[3];
		float size;
		float specular;
		uint32_t mask;
		float softshadow_angle;
		float soft_shadow_scale;
		uint32_t blend_splits;
		float shadow_opacity;
		float fade_from;
		float fade_to;
		uint32_t pad[2];
		uint32_t bake_mode;
		float volumetric_fog_energy;
		float shadow_bias[4];
		float shadow_normal_bias[4];
		float shadow_transmittance_bias[4];
		float shadow_z_range[4];
		float shadow_range_begin[4];
		float shadow_split_offsets[4];
		float shadow_matrices[4][16];
		float uv_scale1[2];
		float uv_scale2[2];
		float uv_scale3[2];
		float uv_scale4[2];
	};

	uint32_t max_directional_lights;
	DirectionalLightData *directional_lights = nullptr;
	RID directional_light_buffer;

	/* REFLECTION PROBE */

	struct ReflectionProbe {
		RS::ReflectionProbeUpdateMode update_mode = RS::REFLECTION_PROBE_UPDATE_ONCE;
		int resolution = 256;
		float intensity = 1.0;
		float blend_distance = 1.0;
		RS::ReflectionProbeAmbientMode ambient_mode = RS::REFLECTION_PROBE_AMBIENT_ENVIRONMENT;
		Color ambient_color;
		float ambient_color_energy = 1.0;
		float max_distance = 0;
		Vector3 size = Vector3(20, 20, 20);
		Vector3 origin_offset;
		bool interior = false;
		bool box_projection = false;
		bool enable_shadows = false;
		uint32_t cull_mask = (1 << 20) - 1;
		uint32_t reflection_mask = (1 << 20) - 1;
		float mesh_lod_threshold = 0.01;
		float baked_exposure = 1.0;

		Dependency dependency;
	};
	mutable RID_Owner<ReflectionProbe, true> reflection_probe_owner;

	/* REFLECTION ATLAS */

	struct ReflectionAtlas {
		int count = 0;
		int size = 0;

		RID reflection;
		RID depth_buffer;
		RID depth_fb;

		struct Reflection {
			RID owner;
			RendererRD::SkyRD::ReflectionData data;
			RID fbs[6];
		};

		Vector<Reflection> reflections;

		Ref<RenderSceneBuffersRD> render_buffers; // Further render buffers used.

		ClusterBuilderRD *cluster_builder = nullptr; // only used if cluster builder is supported by the renderer.
	};

	mutable RID_Owner<ReflectionAtlas> reflection_atlas_owner;

	/* REFLECTION PROBE INSTANCE */

	struct ReflectionProbeInstance {
		RID probe;
		int atlas_index = -1;
		RID atlas;

		bool dirty = true;
		bool rendering = false;
		int processing_layer = 1;
		int processing_side = 0;

		uint64_t last_pass = 0;
		uint32_t cull_mask = 0;

		RendererRD::ForwardID forward_id = -1;

		Transform3D transform;
	};

	mutable RID_Owner<ReflectionProbeInstance> reflection_probe_instance_owner;

	/* REFLECTION DATA */

	enum {
		REFLECTION_AMBIENT_DISABLED = 0,
		REFLECTION_AMBIENT_ENVIRONMENT = 1,
		REFLECTION_AMBIENT_COLOR = 2,
	};

	struct ReflectionData {
		float box_extents[3];
		float index;
		float box_offset[3];
		uint32_t mask;
		float ambient[3]; // ambient color,
		float intensity;
		float blend_distance;
		uint32_t exterior;
		uint32_t box_project;
		uint32_t ambient_mode;
		float exposure_normalization;
		uint32_t pad0;
		uint32_t pad1;
		uint32_t pad2;
		float local_matrix[16]; // up to here for spot and omni, rest is for directional
	};

	struct ReflectionProbeInstanceSort {
		float size;
		ReflectionProbeInstance *probe_instance;
		bool operator<(const ReflectionProbeInstanceSort &p_sort) const {
			return size < p_sort.size;
		}
	};

	uint32_t max_reflections;
	uint32_t reflection_count = 0;
	// uint32_t max_reflection_probes_per_instance = 0; // seems unused
	ReflectionData *reflections = nullptr;
	ReflectionProbeInstanceSort *reflection_sort = nullptr;
	RID reflection_buffer;

	/* LIGHTMAP */

	struct Lightmap {
		RID light_texture;
		RID shadow_texture;
		RS::ShadowmaskMode shadowmask_mode = RS::SHADOWMASK_MODE_NONE;
		bool uses_spherical_harmonics = false;
		bool interior = false;
		AABB bounds = AABB(Vector3(), Vector3(1, 1, 1));
		float baked_exposure = 1.0;
		Vector2i light_texture_size;
		int32_t array_index = -1; //unassigned
		PackedVector3Array points;
		PackedColorArray point_sh;
		PackedInt32Array tetrahedra;
		PackedInt32Array bsp_tree;

		struct BSP {
			static const int32_t EMPTY_LEAF = INT32_MIN;
			float plane[4];
			int32_t over = EMPTY_LEAF, under = EMPTY_LEAF;
		};

		Dependency dependency;
	};

	bool using_lightmap_array;
	Vector<RID> lightmap_textures;
	uint64_t lightmap_array_version = 0;
	float lightmap_probe_capture_update_speed = 4;

	mutable RID_Owner<Lightmap, true> lightmap_owner;

	Vector<RID> shadowmask_textures;

	/* LIGHTMAP INSTANCE */

	struct LightmapInstance {
		RID lightmap;
		Transform3D transform;
	};

	mutable RID_Owner<LightmapInstance> lightmap_instance_owner;

	/* SHADOW ATLAS */

	uint64_t shadow_atlas_realloc_tolerance_msec = 500;

	struct ShadowShrinkStage {
		RID texture;
		RID filter_texture;
		uint32_t size = 0;
	};

	struct ShadowAtlas {
		struct Quadrant {
			uint32_t subdivision = 0;

			struct Shadow {
				RID owner;
				uint64_t version = 0;
				uint64_t fog_version = 0; // used for fog
				uint64_t alloc_tick = 0;

				Shadow() {}
			};

			Vector<Shadow> shadows;

			Quadrant() {}
		} quadrants[4];

		int size_order[4] = { 0, 1, 2, 3 };
		uint32_t smallest_subdiv = 0;

		int size = 0;
		bool use_16_bits = true;

		RID depth;
		RID fb; //for copying

		HashMap<RID, uint32_t> shadow_owners;
	};

	RID_Owner<ShadowAtlas> shadow_atlas_owner;

	void _update_shadow_atlas(ShadowAtlas *shadow_atlas);

	void _shadow_atlas_invalidate_shadow(ShadowAtlas::Quadrant::Shadow *p_shadow, RID p_atlas, ShadowAtlas *p_shadow_atlas, uint32_t p_quadrant, uint32_t p_shadow_idx);
	bool _shadow_atlas_find_shadow(ShadowAtlas *shadow_atlas, int *p_in_quadrants, int p_quadrant_count, int p_current_subdiv, uint64_t p_tick, int &r_quadrant, int &r_shadow);
	bool _shadow_atlas_find_omni_shadows(ShadowAtlas *shadow_atlas, int *p_in_quadrants, int p_quadrant_count, int p_current_subdiv, uint64_t p_tick, int &r_quadrant, int &r_shadow);

	/* DIRECTIONAL SHADOW */

	struct DirectionalShadow {
		RID depth;
		RID fb; //when renderign direct

		int light_count = 0;
		int size = 0;
		bool use_16_bits = true;
		int current_light = 0;
	} directional_shadow;

	/* SHADOW CUBEMAPS */

	struct ShadowCubemap {
		RID cubemap;
		RID side_fb[6];
	};

	HashMap<int, ShadowCubemap> shadow_cubemaps;
	ShadowCubemap *_get_shadow_cubemap(int p_size);

	/* PIPELINE HINTS */

	bool shadow_cubemaps_used = false;
	bool shadow_dual_paraboloid_used = false;

public:
	static LightStorage *get_singleton();

	LightStorage();
	virtual ~LightStorage();

	bool free(RID p_rid);

	/* Settings */
	void set_max_cluster_elements(const uint32_t p_max_cluster_elements) {
		max_cluster_elements = p_max_cluster_elements;
		set_max_reflection_probes(p_max_cluster_elements);
		set_max_lights(p_max_cluster_elements);
	}
	uint32_t get_max_cluster_elements() const { return max_cluster_elements; }

	/* LIGHT */

	bool owns_light(RID p_rid) { return light_owner.owns(p_rid); }

	void _light_initialize(RID p_rid, RS::LightType p_type);

	virtual RID directional_light_allocate() override;
	virtual void directional_light_initialize(RID p_light) override;

	virtual RID omni_light_allocate() override;
	virtual void omni_light_initialize(RID p_light) override;

	virtual RID spot_light_allocate() override;
	virtual void spot_light_initialize(RID p_light) override;

	virtual void light_free(RID p_rid) override;

	virtual void light_set_color(RID p_light, const Color &p_color) override;
	virtual void light_set_param(RID p_light, RS::LightParam p_param, float p_value) override;
	virtual void light_set_shadow(RID p_light, bool p_enabled) override;
	virtual void light_set_projector(RID p_light, RID p_texture) override;
	virtual void light_set_negative(RID p_light, bool p_enable) override;
	virtual void light_set_cull_mask(RID p_light, uint32_t p_mask) override;
	virtual void light_set_distance_fade(RID p_light, bool p_enabled, float p_begin, float p_shadow, float p_length) override;
	virtual void light_set_reverse_cull_face_mode(RID p_light, bool p_enabled) override;
	virtual void light_set_shadow_caster_mask(RID p_light, uint32_t p_caster_mask) override;
	virtual uint32_t light_get_shadow_caster_mask(RID p_light) const override;
	virtual void light_set_bake_mode(RID p_light, RS::LightBakeMode p_bake_mode) override;
	virtual void light_set_max_sdfgi_cascade(RID p_light, uint32_t p_cascade) override;

	virtual void light_omni_set_shadow_mode(RID p_light, RS::LightOmniShadowMode p_mode) override;

	virtual void light_directional_set_shadow_mode(RID p_light, RS::LightDirectionalShadowMode p_mode) override;
	virtual void light_directional_set_blend_splits(RID p_light, bool p_enable) override;
	virtual bool light_directional_get_blend_splits(RID p_light) const override;
	virtual void light_directional_set_sky_mode(RID p_light, RS::LightDirectionalSkyMode p_mode) override;
	virtual RS::LightDirectionalSkyMode light_directional_get_sky_mode(RID p_light) const override;

	virtual RS::LightDirectionalShadowMode light_directional_get_shadow_mode(RID p_light) override;
	virtual RS::LightOmniShadowMode light_omni_get_shadow_mode(RID p_light) override;

	virtual RS::LightType light_get_type(RID p_light) const override {
		const Light *light = light_owner.get_or_null(p_light);
		ERR_FAIL_NULL_V(light, RS::LIGHT_DIRECTIONAL);

		return light->type;
	}
	virtual AABB light_get_aabb(RID p_light) const override;

	virtual float light_get_param(RID p_light, RS::LightParam p_param) override {
		const Light *light = light_owner.get_or_null(p_light);
		ERR_FAIL_NULL_V(light, 0);

		return light->param[p_param];
	}

	_FORCE_INLINE_ RID light_get_projector(RID p_light) {
		const Light *light = light_owner.get_or_null(p_light);
		ERR_FAIL_NULL_V(light, RID());

		return light->projector;
	}

	virtual Color light_get_color(RID p_light) override {
		const Light *light = light_owner.get_or_null(p_light);
		ERR_FAIL_NULL_V(light, Color());

		return light->color;
	}

	_FORCE_INLINE_ bool light_is_distance_fade_enabled(RID p_light) {
		const Light *light = light_owner.get_or_null(p_light);
		return light->distance_fade;
	}

	_FORCE_INLINE_ float light_get_distance_fade_begin(RID p_light) {
		const Light *light = light_owner.get_or_null(p_light);
		return light->distance_fade_begin;
	}

	_FORCE_INLINE_ float light_get_distance_fade_shadow(RID p_light) {
		const Light *light = light_owner.get_or_null(p_light);
		return light->distance_fade_shadow;
	}

	_FORCE_INLINE_ float light_get_distance_fade_length(RID p_light) {
		const Light *light = light_owner.get_or_null(p_light);
		return light->distance_fade_length;
	}

	virtual bool light_has_shadow(RID p_light) const override {
		const Light *light = light_owner.get_or_null(p_light);
		ERR_FAIL_NULL_V(light, RS::LIGHT_DIRECTIONAL);

		return light->shadow;
	}

	virtual bool light_has_projector(RID p_light) const override {
		const Light *light = light_owner.get_or_null(p_light);
		ERR_FAIL_NULL_V(light, RS::LIGHT_DIRECTIONAL);

		return TextureStorage::get_singleton()->owns_texture(light->projector);
	}

	_FORCE_INLINE_ bool light_is_negative(RID p_light) const {
		const Light *light = light_owner.get_or_null(p_light);
		ERR_FAIL_NULL_V(light, RS::LIGHT_DIRECTIONAL);

		return light->negative;
	}

	_FORCE_INLINE_ float light_get_transmittance_bias(RID p_light) const {
		const Light *light = light_owner.get_or_null(p_light);
		ERR_FAIL_NULL_V(light, 0.0);

		return light->param[RS::LIGHT_PARAM_TRANSMITTANCE_BIAS];
	}

	virtual bool light_get_reverse_cull_face_mode(RID p_light) const override {
		const Light *light = light_owner.get_or_null(p_light);
		ERR_FAIL_NULL_V(light, false);

		return light->reverse_cull;
	}

	virtual RS::LightBakeMode light_get_bake_mode(RID p_light) override;
	virtual uint32_t light_get_max_sdfgi_cascade(RID p_light) override;
	virtual uint64_t light_get_version(RID p_light) const override;
	virtual uint32_t light_get_cull_mask(RID p_light) const override;

	Dependency *light_get_dependency(RID p_light) const;

	/* LIGHT INSTANCE API */

	bool owns_light_instance(RID p_rid) { return light_instance_owner.owns(p_rid); }

	virtual RID light_instance_create(RID p_light) override;
	virtual void light_instance_free(RID p_light) override;
	virtual void light_instance_set_transform(RID p_light_instance, const Transform3D &p_transform) override;
	virtual void light_instance_set_aabb(RID p_light_instance, const AABB &p_aabb) override;
	virtual void light_instance_set_shadow_transform(RID p_light_instance, const Projection &p_projection, const Transform3D &p_transform, float p_far, float p_split, int p_pass, float p_shadow_texel_size, float p_bias_scale = 1.0, float p_range_begin = 0, const Vector2 &p_uv_scale = Vector2()) override;
	virtual void light_instance_mark_visible(RID p_light_instance) override;

	virtual bool light_instance_is_shadow_visible_at_position(RID p_light_instance, const Vector3 &p_position) const override {
		const LightInstance *light_instance = light_instance_owner.get_or_null(p_light_instance);
		ERR_FAIL_NULL_V(light_instance, false);
		const Light *light = light_owner.get_or_null(light_instance->light);
		ERR_FAIL_NULL_V(light, false);

		if (!light->shadow) {
			return false;
		}

		if (!light->distance_fade) {
			return true;
		}

		real_t distance = p_position.distance_to(light_instance->transform.origin);

		if (distance > light->distance_fade_shadow + light->distance_fade_length) {
			return false;
		}

		return true;
	}

	_FORCE_INLINE_ RID light_instance_get_base_light(RID p_light_instance) {
		LightInstance *li = light_instance_owner.get_or_null(p_light_instance);
		return li->light;
	}

	_FORCE_INLINE_ Transform3D light_instance_get_base_transform(RID p_light_instance) {
		LightInstance *li = light_instance_owner.get_or_null(p_light_instance);
		return li->transform;
	}

	_FORCE_INLINE_ AABB light_instance_get_base_aabb(RID p_light_instance) {
		LightInstance *li = light_instance_owner.get_or_null(p_light_instance);
		return li->aabb;
	}

	_FORCE_INLINE_ void light_instance_set_cull_mask(RID p_light_instance, uint32_t p_cull_mask) {
		LightInstance *li = light_instance_owner.get_or_null(p_light_instance);
		li->cull_mask = p_cull_mask;
	}

	_FORCE_INLINE_ uint32_t light_instance_get_cull_mask(RID p_light_instance) {
		LightInstance *li = light_instance_owner.get_or_null(p_light_instance);
		return li->cull_mask;
	}

	_FORCE_INLINE_ Rect2 light_instance_get_shadow_atlas_rect(RID p_light_instance, RID p_shadow_atlas, Vector2i &r_omni_offset) {
		ShadowAtlas *shadow_atlas = shadow_atlas_owner.get_or_null(p_shadow_atlas);
		LightInstance *li = light_instance_owner.get_or_null(p_light_instance);
		uint32_t key = shadow_atlas->shadow_owners[li->self];

		uint32_t quadrant = (key >> QUADRANT_SHIFT) & 0x3;
		uint32_t shadow = key & SHADOW_INDEX_MASK;

		ERR_FAIL_COND_V(shadow >= (uint32_t)shadow_atlas->quadrants[quadrant].shadows.size(), Rect2());

		uint32_t atlas_size = shadow_atlas->size;
		uint32_t quadrant_size = atlas_size >> 1;

		uint32_t x = (quadrant & 1) * quadrant_size;
		uint32_t y = (quadrant >> 1) * quadrant_size;

		uint32_t shadow_size = (quadrant_size / shadow_atlas->quadrants[quadrant].subdivision);
		x += (shadow % shadow_atlas->quadrants[quadrant].subdivision) * shadow_size;
		y += (shadow / shadow_atlas->quadrants[quadrant].subdivision) * shadow_size;

		if (key & OMNI_LIGHT_FLAG) {
			if (((shadow + 1) % shadow_atlas->quadrants[quadrant].subdivision) == 0) {
				r_omni_offset.x = 1 - int(shadow_atlas->quadrants[quadrant].subdivision);
				r_omni_offset.y = 1;
			} else {
				r_omni_offset.x = 1;
				r_omni_offset.y = 0;
			}
		}

		uint32_t width = shadow_size;
		uint32_t height = shadow_size;

		return Rect2(x / float(shadow_atlas->size), y / float(shadow_atlas->size), width / float(shadow_atlas->size), height / float(shadow_atlas->size));
	}

	_FORCE_INLINE_ float light_instance_get_shadow_texel_size(RID p_light_instance, RID p_shadow_atlas) {
#ifdef DEBUG_ENABLED
		LightInstance *li = light_instance_owner.get_or_null(p_light_instance);
		ERR_FAIL_COND_V(!li->shadow_atlases.has(p_shadow_atlas), 0);
#endif
		ShadowAtlas *shadow_atlas = shadow_atlas_owner.get_or_null(p_shadow_atlas);
		ERR_FAIL_NULL_V(shadow_atlas, 0);
#ifdef DEBUG_ENABLED
		ERR_FAIL_COND_V(!shadow_atlas->shadow_owners.has(p_light_instance), 0);
#endif
		uint32_t key = shadow_atlas->shadow_owners[p_light_instance];

		uint32_t quadrant = (key >> QUADRANT_SHIFT) & 0x3;

		uint32_t quadrant_size = shadow_atlas->size >> 1;

		uint32_t shadow_size = (quadrant_size / shadow_atlas->quadrants[quadrant].subdivision);

		return float(1.0) / shadow_size;
	}

	_FORCE_INLINE_ Projection light_instance_get_shadow_camera(RID p_light_instance, int p_index) {
		LightInstance *li = light_instance_owner.get_or_null(p_light_instance);
		return li->shadow_transform[p_index].camera;
	}

	_FORCE_INLINE_ Transform3D light_instance_get_shadow_transform(RID p_light_instance, int p_index) {
		LightInstance *li = light_instance_owner.get_or_null(p_light_instance);
		return li->shadow_transform[p_index].transform;
	}
	_FORCE_INLINE_ float light_instance_get_shadow_bias_scale(RID p_light_instance, int p_index) {
		LightInstance *li = light_instance_owner.get_or_null(p_light_instance);
		return li->shadow_transform[p_index].bias_scale;
	}
	_FORCE_INLINE_ float light_instance_get_shadow_range(RID p_light_instance, int p_index) {
		LightInstance *li = light_instance_owner.get_or_null(p_light_instance);
		return li->shadow_transform[p_index].farplane;
	}
	_FORCE_INLINE_ float light_instance_get_shadow_range_begin(RID p_light_instance, int p_index) {
		LightInstance *li = light_instance_owner.get_or_null(p_light_instance);
		return li->shadow_transform[p_index].range_begin;
	}

	_FORCE_INLINE_ Vector2 light_instance_get_shadow_uv_scale(RID p_light_instance, int p_index) {
		LightInstance *li = light_instance_owner.get_or_null(p_light_instance);
		return li->shadow_transform[p_index].uv_scale;
	}

	_FORCE_INLINE_ void light_instance_set_directional_shadow_atlas_rect(RID p_light_instance, int p_index, const Rect2 p_atlas_rect) {
		LightInstance *li = light_instance_owner.get_or_null(p_light_instance);
		li->shadow_transform[p_index].atlas_rect = p_atlas_rect;
	}

	_FORCE_INLINE_ Rect2 light_instance_get_directional_shadow_atlas_rect(RID p_light_instance, int p_index) {
		LightInstance *li = light_instance_owner.get_or_null(p_light_instance);
		return li->shadow_transform[p_index].atlas_rect;
	}

	_FORCE_INLINE_ float light_instance_get_directional_shadow_split(RID p_light_instance, int p_index) {
		LightInstance *li = light_instance_owner.get_or_null(p_light_instance);
		return li->shadow_transform[p_index].split;
	}

	_FORCE_INLINE_ float light_instance_get_directional_shadow_texel_size(RID p_light_instance, int p_index) {
		LightInstance *li = light_instance_owner.get_or_null(p_light_instance);
		return li->shadow_transform[p_index].shadow_texel_size;
	}

	_FORCE_INLINE_ void light_instance_set_render_pass(RID p_light_instance, uint64_t p_pass) {
		LightInstance *li = light_instance_owner.get_or_null(p_light_instance);
		li->last_pass = p_pass;
	}

	_FORCE_INLINE_ uint64_t light_instance_get_render_pass(RID p_light_instance) {
		LightInstance *li = light_instance_owner.get_or_null(p_light_instance);
		return li->last_pass;
	}

	_FORCE_INLINE_ void light_instance_set_shadow_pass(RID p_light_instance, uint64_t p_pass) {
		LightInstance *li = light_instance_owner.get_or_null(p_light_instance);
		li->last_scene_shadow_pass = p_pass;
	}

	_FORCE_INLINE_ uint64_t light_instance_get_shadow_pass(RID p_light_instance) {
		LightInstance *li = light_instance_owner.get_or_null(p_light_instance);
		return li->last_scene_shadow_pass;
	}

	_FORCE_INLINE_ ForwardID light_instance_get_forward_id(RID p_light_instance) {
		LightInstance *li = light_instance_owner.get_or_null(p_light_instance);
		return li->forward_id;
	}

	_FORCE_INLINE_ RS::LightType light_instance_get_type(RID p_light_instance) {
		LightInstance *li = light_instance_owner.get_or_null(p_light_instance);
		return li->light_type;
	}

	_FORCE_INLINE_ void light_instance_set_directional_rect(RID p_light_instance, const Rect2 &p_directional_rect) {
		LightInstance *li = light_instance_owner.get_or_null(p_light_instance);
		li->directional_rect = p_directional_rect;
	}

	_FORCE_INLINE_ Rect2 light_instance_get_directional_rect(RID p_light_instance) {
		LightInstance *li = light_instance_owner.get_or_null(p_light_instance);
		return li->directional_rect;
	}

	/* LIGHT DATA */

	void free_light_data();
	void set_max_lights(const uint32_t p_max_lights);
	RID get_omni_light_buffer() { return omni_light_buffer; }
	RID get_spot_light_buffer() { return spot_light_buffer; }
	RID get_directional_light_buffer() { return directional_light_buffer; }
	uint32_t get_max_directional_lights() { return max_directional_lights; }
	uint32_t get_directional_light_blend_splits(uint32_t p_directional_light_count) const {
		uint32_t blend_splits = 0;
		for (uint32_t i = 0; i < p_directional_light_count; i++) {
			if (directional_lights[i].blend_splits) {
				blend_splits |= 1U << i;
			}
		}

		return blend_splits;
	}
	bool has_directional_shadows(const uint32_t p_directional_light_count) {
		for (uint32_t i = 0; i < p_directional_light_count; i++) {
			if (directional_lights[i].shadow_opacity > 0.001) {
				return true;
			}
		}
		return false;
	}
	void update_light_buffers(RenderDataRD *p_render_data, const PagedArray<RID> &p_lights, const Transform3D &p_camera_transform, RID p_shadow_atlas, bool p_using_shadows, uint32_t &r_directional_light_count, uint32_t &r_positional_light_count, bool &r_directional_light_soft_shadows);

	/* REFLECTION PROBE */

	bool owns_reflection_probe(RID p_rid) { return reflection_probe_owner.owns(p_rid); }

	virtual RID reflection_probe_allocate() override;
	virtual void reflection_probe_initialize(RID p_reflection_probe) override;
	virtual void reflection_probe_free(RID p_rid) override;

	virtual void reflection_probe_set_update_mode(RID p_probe, RS::ReflectionProbeUpdateMode p_mode) override;
	virtual void reflection_probe_set_intensity(RID p_probe, float p_intensity) override;
	virtual void reflection_probe_set_blend_distance(RID p_probe, float p_blend_distance) override;
	virtual void reflection_probe_set_ambient_mode(RID p_probe, RS::ReflectionProbeAmbientMode p_mode) override;
	virtual void reflection_probe_set_ambient_color(RID p_probe, const Color &p_color) override;
	virtual void reflection_probe_set_ambient_energy(RID p_probe, float p_energy) override;
	virtual void reflection_probe_set_max_distance(RID p_probe, float p_distance) override;
	virtual void reflection_probe_set_size(RID p_probe, const Vector3 &p_size) override;
	virtual void reflection_probe_set_origin_offset(RID p_probe, const Vector3 &p_offset) override;
	virtual void reflection_probe_set_as_interior(RID p_probe, bool p_enable) override;
	virtual void reflection_probe_set_enable_box_projection(RID p_probe, bool p_enable) override;
	virtual void reflection_probe_set_enable_shadows(RID p_probe, bool p_enable) override;
	virtual void reflection_probe_set_cull_mask(RID p_probe, uint32_t p_layers) override;
	virtual void reflection_probe_set_reflection_mask(RID p_probe, uint32_t p_layers) override;
	virtual void reflection_probe_set_resolution(RID p_probe, int p_resolution) override;
	virtual void reflection_probe_set_mesh_lod_threshold(RID p_probe, float p_ratio) override;

	void reflection_probe_set_baked_exposure(RID p_probe, float p_exposure);

	virtual AABB reflection_probe_get_aabb(RID p_probe) const override;
	virtual RS::ReflectionProbeUpdateMode reflection_probe_get_update_mode(RID p_probe) const override;
	virtual uint32_t reflection_probe_get_cull_mask(RID p_probe) const override;
	virtual uint32_t reflection_probe_get_reflection_mask(RID p_probe) const override;
	virtual Vector3 reflection_probe_get_size(RID p_probe) const override;
	virtual Vector3 reflection_probe_get_origin_offset(RID p_probe) const override;
	virtual float reflection_probe_get_origin_max_distance(RID p_probe) const override;
	virtual float reflection_probe_get_mesh_lod_threshold(RID p_probe) const override;

	int reflection_probe_get_resolution(RID p_probe) const;
	float reflection_probe_get_baked_exposure(RID p_probe) const;
	virtual bool reflection_probe_renders_shadows(RID p_probe) const override;

	float reflection_probe_get_intensity(RID p_probe) const;
	float reflection_probe_get_blend_distance(RID p_probe) const;
	bool reflection_probe_is_interior(RID p_probe) const;
	bool reflection_probe_is_box_projection(RID p_probe) const;
	RS::ReflectionProbeAmbientMode reflection_probe_get_ambient_mode(RID p_probe) const;
	Color reflection_probe_get_ambient_color(RID p_probe) const;
	float reflection_probe_get_ambient_color_energy(RID p_probe) const;

	Dependency *reflection_probe_get_dependency(RID p_probe) const;

	/* REFLECTION ATLAS */

	bool owns_reflection_atlas(RID p_rid) { return reflection_atlas_owner.owns(p_rid); }

	virtual RID reflection_atlas_create() override;
	virtual void reflection_atlas_free(RID p_ref_atlas) override;
	virtual void reflection_atlas_set_size(RID p_ref_atlas, int p_reflection_size, int p_reflection_count) override;
	virtual int reflection_atlas_get_size(RID p_ref_atlas) const override;

	_FORCE_INLINE_ RID reflection_atlas_get_texture(RID p_ref_atlas) {
		ReflectionAtlas *atlas = reflection_atlas_owner.get_or_null(p_ref_atlas);
		ERR_FAIL_NULL_V(atlas, RID());
		return atlas->reflection;
	}

	/* REFLECTION PROBE INSTANCE */

	bool owns_reflection_probe_instance(RID p_rid) { return reflection_probe_instance_owner.owns(p_rid); }

	virtual RID reflection_probe_instance_create(RID p_probe) override;
	virtual void reflection_probe_instance_free(RID p_instance) override;
	virtual void reflection_probe_instance_set_transform(RID p_instance, const Transform3D &p_transform) override;
	virtual bool reflection_probe_has_atlas_index(RID p_instance) override;
	virtual void reflection_probe_release_atlas_index(RID p_instance) override;
	virtual bool reflection_probe_instance_needs_redraw(RID p_instance) override;
	virtual bool reflection_probe_instance_has_reflection(RID p_instance) override;
	virtual bool reflection_probe_instance_begin_render(RID p_instance, RID p_reflection_atlas) override;
	virtual Ref<RenderSceneBuffers> reflection_probe_atlas_get_render_buffers(RID p_reflection_atlas) override;
	virtual bool reflection_probe_instance_postprocess_step(RID p_instance) override;

	uint32_t reflection_probe_instance_get_resolution(RID p_instance);
	RID reflection_probe_instance_get_framebuffer(RID p_instance, int p_index);
	RID reflection_probe_instance_get_depth_framebuffer(RID p_instance, int p_index);

	_FORCE_INLINE_ RID reflection_probe_instance_get_probe(RID p_instance) {
		ReflectionProbeInstance *rpi = reflection_probe_instance_owner.get_or_null(p_instance);
		ERR_FAIL_NULL_V(rpi, RID());

		return rpi->probe;
	}

	_FORCE_INLINE_ RendererRD::ForwardID reflection_probe_instance_get_forward_id(RID p_instance) {
		ReflectionProbeInstance *rpi = reflection_probe_instance_owner.get_or_null(p_instance);
		ERR_FAIL_NULL_V(rpi, 0);

		return rpi->forward_id;
	}

	_FORCE_INLINE_ void reflection_probe_instance_set_cull_mask(RID p_instance, uint32_t p_render_pass) {
		ReflectionProbeInstance *rpi = reflection_probe_instance_owner.get_or_null(p_instance);
		ERR_FAIL_NULL(rpi);
		rpi->cull_mask = p_render_pass;
	}

	_FORCE_INLINE_ void reflection_probe_instance_set_render_pass(RID p_instance, uint32_t p_render_pass) {
		ReflectionProbeInstance *rpi = reflection_probe_instance_owner.get_or_null(p_instance);
		ERR_FAIL_NULL(rpi);
		rpi->last_pass = p_render_pass;
	}

	_FORCE_INLINE_ uint32_t reflection_probe_instance_get_render_pass(RID p_instance) {
		ReflectionProbeInstance *rpi = reflection_probe_instance_owner.get_or_null(p_instance);
		ERR_FAIL_NULL_V(rpi, 0);

		return rpi->last_pass;
	}

	_FORCE_INLINE_ Transform3D reflection_probe_instance_get_transform(RID p_instance) {
		ReflectionProbeInstance *rpi = reflection_probe_instance_owner.get_or_null(p_instance);
		ERR_FAIL_NULL_V(rpi, Transform3D());

		return rpi->transform;
	}

	_FORCE_INLINE_ int reflection_probe_instance_get_atlas_index(RID p_instance) {
		ReflectionProbeInstance *rpi = reflection_probe_instance_owner.get_or_null(p_instance);
		ERR_FAIL_NULL_V(rpi, -1);

		return rpi->atlas_index;
	}

	ClusterBuilderRD *reflection_probe_instance_get_cluster_builder(RID p_instance, ClusterBuilderSharedDataRD *p_cluster_builder_shared);

	/* REFLECTION DATA */

	void free_reflection_data();
	void set_max_reflection_probes(const uint32_t p_max_reflection_probes);
	RID get_reflection_probe_buffer() { return reflection_buffer; }
	void update_reflection_probe_buffer(RenderDataRD *p_render_data, const PagedArray<RID> &p_reflections, const Transform3D &p_camera_inverse_transform, RID p_environment);
	static RD::DataFormat get_reflection_probe_color_format();
	static uint32_t get_reflection_probe_color_usage_bits();
	static RD::DataFormat get_reflection_probe_depth_format();
	static uint32_t get_reflection_probe_depth_usage_bits();

	/* LIGHTMAP */

	bool owns_lightmap(RID p_rid) { return lightmap_owner.owns(p_rid); }

	virtual RID lightmap_allocate() override;
	virtual void lightmap_initialize(RID p_lightmap) override;
	virtual void lightmap_free(RID p_rid) override;

	virtual void lightmap_set_textures(RID p_lightmap, RID p_light, bool p_uses_spherical_haromics) override;
	virtual void lightmap_set_probe_bounds(RID p_lightmap, const AABB &p_bounds) override;
	virtual void lightmap_set_probe_interior(RID p_lightmap, bool p_interior) override;
	virtual void lightmap_set_probe_capture_data(RID p_lightmap, const PackedVector3Array &p_points, const PackedColorArray &p_point_sh, const PackedInt32Array &p_tetrahedra, const PackedInt32Array &p_bsp_tree) override;
	virtual void lightmap_set_baked_exposure_normalization(RID p_lightmap, float p_exposure) override;
	virtual PackedVector3Array lightmap_get_probe_capture_points(RID p_lightmap) const override;
	virtual PackedColorArray lightmap_get_probe_capture_sh(RID p_lightmap) const override;
	virtual PackedInt32Array lightmap_get_probe_capture_tetrahedra(RID p_lightmap) const override;
	virtual PackedInt32Array lightmap_get_probe_capture_bsp_tree(RID p_lightmap) const override;
	virtual AABB lightmap_get_aabb(RID p_lightmap) const override;
	virtual bool lightmap_is_interior(RID p_lightmap) const override;
	virtual void lightmap_tap_sh_light(RID p_lightmap, const Vector3 &p_point, Color *r_sh) override;
	virtual void lightmap_set_probe_capture_update_speed(float p_speed) override;

	Dependency *lightmap_get_dependency(RID p_lightmap) const;

	virtual void lightmap_set_shadowmask_textures(RID p_lightmap, RID p_shadow) override;
	virtual RS::ShadowmaskMode lightmap_get_shadowmask_mode(RID p_lightmap) override;
	virtual void lightmap_set_shadowmask_mode(RID p_lightmap, RS::ShadowmaskMode p_mode) override;

	virtual float lightmap_get_probe_capture_update_speed() const override {
		return lightmap_probe_capture_update_speed;
	}
	_FORCE_INLINE_ RID lightmap_get_texture(RID p_lightmap) const {
		const Lightmap *lm = lightmap_owner.get_or_null(p_lightmap);
		ERR_FAIL_NULL_V(lm, RID());
		return lm->light_texture;
	}
	_FORCE_INLINE_ float lightmap_get_baked_exposure_normalization(RID p_lightmap) const {
		const Lightmap *lm = lightmap_owner.get_or_null(p_lightmap);
		ERR_FAIL_NULL_V(lm, 1.0);
		return lm->baked_exposure;
	}
	_FORCE_INLINE_ int32_t lightmap_get_array_index(RID p_lightmap) const {
		ERR_FAIL_COND_V(!using_lightmap_array, -1); //only for arrays
		const Lightmap *lm = lightmap_owner.get_or_null(p_lightmap);
		return lm->array_index;
	}
	_FORCE_INLINE_ bool lightmap_uses_spherical_harmonics(RID p_lightmap) const {
		ERR_FAIL_COND_V(!using_lightmap_array, false); //only for arrays
		const Lightmap *lm = lightmap_owner.get_or_null(p_lightmap);
		return lm->uses_spherical_harmonics;
	}
	_FORCE_INLINE_ Vector2i lightmap_get_light_texture_size(RID p_lightmap) const {
		const Lightmap *lm = lightmap_owner.get_or_null(p_lightmap);
		return lm->light_texture_size;
	}
	_FORCE_INLINE_ uint64_t lightmap_array_get_version() const {
		ERR_FAIL_COND_V(!using_lightmap_array, 0); //only for arrays
		return lightmap_array_version;
	}

	_FORCE_INLINE_ int lightmap_array_get_size() const {
		ERR_FAIL_COND_V(!using_lightmap_array, 0); //only for arrays
		return lightmap_textures.size();
	}

	_FORCE_INLINE_ const Vector<RID> &lightmap_array_get_textures() const {
		ERR_FAIL_COND_V(!using_lightmap_array, lightmap_textures); //only for arrays
		return lightmap_textures;
	}

	_FORCE_INLINE_ RID shadowmask_get_texture(RID p_lightmap) const {
		const Lightmap *lm = lightmap_owner.get_or_null(p_lightmap);
		ERR_FAIL_NULL_V(lm, RID());
		return lm->shadow_texture;
	}

	/* LIGHTMAP INSTANCE */

	bool owns_lightmap_instance(RID p_rid) { return lightmap_instance_owner.owns(p_rid); }

	virtual RID lightmap_instance_create(RID p_lightmap) override;
	virtual void lightmap_instance_free(RID p_lightmap) override;
	virtual void lightmap_instance_set_transform(RID p_lightmap, const Transform3D &p_transform) override;
	_FORCE_INLINE_ bool lightmap_instance_is_valid(RID p_lightmap_instance) {
		return lightmap_instance_owner.get_or_null(p_lightmap_instance) != nullptr;
	}

	_FORCE_INLINE_ RID lightmap_instance_get_lightmap(RID p_lightmap_instance) {
		LightmapInstance *li = lightmap_instance_owner.get_or_null(p_lightmap_instance);
		return li->lightmap;
	}
	_FORCE_INLINE_ Transform3D lightmap_instance_get_transform(RID p_lightmap_instance) {
		LightmapInstance *li = lightmap_instance_owner.get_or_null(p_lightmap_instance);
		return li->transform;
	}

	/* SHADOW ATLAS API */

	bool owns_shadow_atlas(RID p_rid) { return shadow_atlas_owner.owns(p_rid); }

	virtual RID shadow_atlas_create() override;
	virtual void shadow_atlas_free(RID p_atlas) override;

	virtual void shadow_atlas_set_size(RID p_atlas, int p_size, bool p_16_bits = true) override;
	virtual void shadow_atlas_set_quadrant_subdivision(RID p_atlas, int p_quadrant, int p_subdivision) override;
	virtual bool shadow_atlas_update_light(RID p_atlas, RID p_light_instance, float p_coverage, uint64_t p_light_version) override;
	_FORCE_INLINE_ bool shadow_atlas_owns_light_instance(RID p_atlas, RID p_light_instance) {
		ShadowAtlas *atlas = shadow_atlas_owner.get_or_null(p_atlas);
		ERR_FAIL_NULL_V(atlas, false);
		return atlas->shadow_owners.has(p_light_instance);
	}
	_FORCE_INLINE_ uint32_t shadow_atlas_get_light_instance_key(RID p_atlas, RID p_light_instance) {
		ShadowAtlas *atlas = shadow_atlas_owner.get_or_null(p_atlas);
		ERR_FAIL_NULL_V(atlas, -1);
		return atlas->shadow_owners[p_light_instance];
	}

	_FORCE_INLINE_ RID shadow_atlas_get_texture(RID p_atlas) {
		ShadowAtlas *atlas = shadow_atlas_owner.get_or_null(p_atlas);
		ERR_FAIL_NULL_V(atlas, RID());
		return atlas->depth;
	}

	_FORCE_INLINE_ int shadow_atlas_get_size(RID p_atlas) {
		ShadowAtlas *atlas = shadow_atlas_owner.get_or_null(p_atlas);
		ERR_FAIL_NULL_V(atlas, 0);
		return atlas->size;
	}

	_FORCE_INLINE_ int shadow_atlas_get_quadrant_shadow_size(RID p_atlas, uint32_t p_quadrant) {
		ShadowAtlas *atlas = shadow_atlas_owner.get_or_null(p_atlas);
		ERR_FAIL_NULL_V(atlas, 0);
		ERR_FAIL_UNSIGNED_INDEX_V(p_quadrant, 4, 0);
		return atlas->quadrants[p_quadrant].shadows.size();
	}

	_FORCE_INLINE_ uint32_t shadow_atlas_get_quadrant_subdivision(RID p_atlas, uint32_t p_quadrant) {
		ShadowAtlas *atlas = shadow_atlas_owner.get_or_null(p_atlas);
		ERR_FAIL_NULL_V(atlas, 0);
		ERR_FAIL_UNSIGNED_INDEX_V(p_quadrant, 4, 0);
		return atlas->quadrants[p_quadrant].subdivision;
	}

	_FORCE_INLINE_ RID shadow_atlas_get_fb(RID p_atlas) {
		ShadowAtlas *atlas = shadow_atlas_owner.get_or_null(p_atlas);
		ERR_FAIL_NULL_V(atlas, RID());
		return atlas->fb;
	}

	virtual void shadow_atlas_update(RID p_atlas) override;
	static RD::DataFormat get_shadow_atlas_depth_format(bool p_16_bits);
	static uint32_t get_shadow_atlas_depth_usage_bits();

	/* DIRECTIONAL SHADOW */

	virtual void directional_shadow_atlas_set_size(int p_size, bool p_16_bits = true) override;
	virtual int get_directional_light_shadow_size(RID p_light_instance) override;
	virtual void set_directional_shadow_count(int p_count) override;

	Rect2i get_directional_shadow_rect();
	void update_directional_shadow_atlas();

	_FORCE_INLINE_ RID directional_shadow_get_texture() {
		return directional_shadow.depth;
	}

	_FORCE_INLINE_ int directional_shadow_get_size() {
		return directional_shadow.size;
	}

	_FORCE_INLINE_ RID direction_shadow_get_fb() {
		return directional_shadow.fb;
	}

	_FORCE_INLINE_ void directional_shadow_increase_current_light() {
		directional_shadow.current_light++;
	}

	/* SHADOW CUBEMAPS */

	RID get_cubemap(int p_size);
	RID get_cubemap_fb(int p_size, int p_pass);
	static RD::DataFormat get_cubemap_depth_format();
	static uint32_t get_cubemap_depth_usage_bits();

	/* PIPELINE HINTS */

	bool get_shadow_cubemaps_used() const;
	bool get_shadow_dual_paraboloid_used() const;
};

} // namespace RendererRD

#endif // LIGHT_STORAGE_RD_H

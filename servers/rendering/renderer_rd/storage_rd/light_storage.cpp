/**************************************************************************/
/*  light_storage.cpp                                                     */
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

#include "light_storage.h"
#include "core/config/project_settings.h"
#include "servers/rendering/renderer_rd/renderer_scene_render_rd.h"
#include "texture_storage.h"

using namespace RendererRD;

LightStorage *LightStorage::singleton = nullptr;

LightStorage *LightStorage::get_singleton() {
	return singleton;
}

LightStorage::LightStorage() {
	singleton = this;

	TextureStorage *texture_storage = TextureStorage::get_singleton();

	directional_shadow.size = GLOBAL_GET("rendering/lights_and_shadows/directional_shadow/size");
	directional_shadow.use_16_bits = GLOBAL_GET("rendering/lights_and_shadows/directional_shadow/16_bits");

	using_lightmap_array = true; // high end
	if (using_lightmap_array) {
		uint64_t textures_per_stage = RD::get_singleton()->limit_get(RD::LIMIT_MAX_TEXTURES_PER_SHADER_STAGE);

		if (textures_per_stage <= 256) {
			lightmap_textures.resize(32);
			shadowmask_textures.resize(32);
		} else {
			lightmap_textures.resize(1024);
			shadowmask_textures.resize(1024);
		}

		for (RID &lightmap_texture : lightmap_textures) {
			lightmap_texture = texture_storage->texture_rd_get_default(TextureStorage::DEFAULT_RD_TEXTURE_2D_ARRAY_WHITE);
		}

		for (RID &shadowmask_texture : shadowmask_textures) {
			shadowmask_texture = texture_storage->texture_rd_get_default(TextureStorage::DEFAULT_RD_TEXTURE_2D_ARRAY_WHITE);
		}
	}

	lightmap_probe_capture_update_speed = GLOBAL_GET("rendering/lightmapping/probe_capture/update_speed");
}

LightStorage::~LightStorage() {
	free_reflection_data();
	free_light_data();

	for (const KeyValue<int, ShadowCubemap> &E : shadow_cubemaps) {
		RD::get_singleton()->free(E.value.cubemap);
	}

	singleton = nullptr;
}

bool LightStorage::free(RID p_rid) {
	if (owns_reflection_probe(p_rid)) {
		reflection_probe_free(p_rid);
		return true;
	} else if (owns_reflection_atlas(p_rid)) {
		reflection_atlas_free(p_rid);
		return true;
	} else if (owns_reflection_probe_instance(p_rid)) {
		reflection_probe_instance_free(p_rid);
		return true;
	} else if (owns_light(p_rid)) {
		light_free(p_rid);
		return true;
	} else if (owns_light_instance(p_rid)) {
		light_instance_free(p_rid);
		return true;
	} else if (owns_lightmap(p_rid)) {
		lightmap_free(p_rid);
		return true;
	} else if (owns_lightmap_instance(p_rid)) {
		lightmap_instance_free(p_rid);
		return true;
	} else if (owns_shadow_atlas(p_rid)) {
		shadow_atlas_free(p_rid);
		return true;
	}

	return false;
}

/* LIGHT */

void LightStorage::_light_initialize(RID p_light, RS::LightType p_type) {
	Light light;
	light.type = p_type;

	light.param[RS::LIGHT_PARAM_ENERGY] = 1.0;
	light.param[RS::LIGHT_PARAM_INDIRECT_ENERGY] = 1.0;
	light.param[RS::LIGHT_PARAM_VOLUMETRIC_FOG_ENERGY] = 1.0;
	light.param[RS::LIGHT_PARAM_SPECULAR] = 0.5;
	light.param[RS::LIGHT_PARAM_RANGE] = 1.0;
	light.param[RS::LIGHT_PARAM_SIZE] = 0.0;
	light.param[RS::LIGHT_PARAM_ATTENUATION] = 1.0;
	light.param[RS::LIGHT_PARAM_SPOT_ANGLE] = 45;
	light.param[RS::LIGHT_PARAM_SPOT_ATTENUATION] = 1.0;
	light.param[RS::LIGHT_PARAM_SHADOW_MAX_DISTANCE] = 0;
	light.param[RS::LIGHT_PARAM_SHADOW_SPLIT_1_OFFSET] = 0.1;
	light.param[RS::LIGHT_PARAM_SHADOW_SPLIT_2_OFFSET] = 0.3;
	light.param[RS::LIGHT_PARAM_SHADOW_SPLIT_3_OFFSET] = 0.6;
	light.param[RS::LIGHT_PARAM_SHADOW_FADE_START] = 0.8;
	light.param[RS::LIGHT_PARAM_SHADOW_NORMAL_BIAS] = 1.0;
	light.param[RS::LIGHT_PARAM_SHADOW_BIAS] = 0.02;
	light.param[RS::LIGHT_PARAM_SHADOW_OPACITY] = 1.0;
	light.param[RS::LIGHT_PARAM_SHADOW_BLUR] = 0;
	light.param[RS::LIGHT_PARAM_SHADOW_PANCAKE_SIZE] = 20.0;
	light.param[RS::LIGHT_PARAM_TRANSMITTANCE_BIAS] = 0.05;
	light.param[RS::LIGHT_PARAM_INTENSITY] = p_type == RS::LIGHT_DIRECTIONAL ? 100000.0 : 1000.0;

	light_owner.initialize_rid(p_light, light);
}

RID LightStorage::directional_light_allocate() {
	return light_owner.allocate_rid();
}

void LightStorage::directional_light_initialize(RID p_light) {
	_light_initialize(p_light, RS::LIGHT_DIRECTIONAL);
}

RID LightStorage::omni_light_allocate() {
	return light_owner.allocate_rid();
}

void LightStorage::omni_light_initialize(RID p_light) {
	_light_initialize(p_light, RS::LIGHT_OMNI);
}

RID LightStorage::spot_light_allocate() {
	return light_owner.allocate_rid();
}

void LightStorage::spot_light_initialize(RID p_light) {
	_light_initialize(p_light, RS::LIGHT_SPOT);
}

void LightStorage::light_free(RID p_rid) {
	light_set_projector(p_rid, RID()); //clear projector

	// delete the texture
	Light *light = light_owner.get_or_null(p_rid);
	light->dependency.deleted_notify(p_rid);
	light_owner.free(p_rid);
}

void LightStorage::light_set_color(RID p_light, const Color &p_color) {
	Light *light = light_owner.get_or_null(p_light);
	ERR_FAIL_NULL(light);

	light->color = p_color;
}

void LightStorage::light_set_param(RID p_light, RS::LightParam p_param, float p_value) {
	Light *light = light_owner.get_or_null(p_light);
	ERR_FAIL_NULL(light);
	ERR_FAIL_INDEX(p_param, RS::LIGHT_PARAM_MAX);

	if (light->param[p_param] == p_value) {
		return;
	}

	switch (p_param) {
		case RS::LIGHT_PARAM_RANGE:
		case RS::LIGHT_PARAM_SPOT_ANGLE:
		case RS::LIGHT_PARAM_SHADOW_MAX_DISTANCE:
		case RS::LIGHT_PARAM_SHADOW_SPLIT_1_OFFSET:
		case RS::LIGHT_PARAM_SHADOW_SPLIT_2_OFFSET:
		case RS::LIGHT_PARAM_SHADOW_SPLIT_3_OFFSET:
		case RS::LIGHT_PARAM_SHADOW_NORMAL_BIAS:
		case RS::LIGHT_PARAM_SHADOW_PANCAKE_SIZE:
		case RS::LIGHT_PARAM_SHADOW_BIAS: {
			light->version++;
			light->dependency.changed_notify(Dependency::DEPENDENCY_CHANGED_LIGHT);
		} break;
		case RS::LIGHT_PARAM_SIZE: {
			if ((light->param[p_param] > CMP_EPSILON) != (p_value > CMP_EPSILON)) {
				//changing from no size to size and the opposite
				light->dependency.changed_notify(Dependency::DEPENDENCY_CHANGED_LIGHT_SOFT_SHADOW_AND_PROJECTOR);
			}
		} break;
		default: {
		}
	}

	light->param[p_param] = p_value;
}

void LightStorage::light_set_shadow(RID p_light, bool p_enabled) {
	Light *light = light_owner.get_or_null(p_light);
	ERR_FAIL_NULL(light);
	light->shadow = p_enabled;

	light->version++;
	light->dependency.changed_notify(Dependency::DEPENDENCY_CHANGED_LIGHT);
}

void LightStorage::light_set_projector(RID p_light, RID p_texture) {
	TextureStorage *texture_storage = TextureStorage::get_singleton();
	Light *light = light_owner.get_or_null(p_light);
	ERR_FAIL_NULL(light);

	if (light->projector == p_texture) {
		return;
	}

	ERR_FAIL_COND(p_texture.is_valid() && !texture_storage->owns_texture(p_texture));

	if (light->type != RS::LIGHT_DIRECTIONAL && light->projector.is_valid()) {
		texture_storage->texture_remove_from_decal_atlas(light->projector, light->type == RS::LIGHT_OMNI);
	}

	light->projector = p_texture;

	if (light->type != RS::LIGHT_DIRECTIONAL) {
		if (light->projector.is_valid()) {
			texture_storage->texture_add_to_decal_atlas(light->projector, light->type == RS::LIGHT_OMNI);
		}
		light->dependency.changed_notify(Dependency::DEPENDENCY_CHANGED_LIGHT_SOFT_SHADOW_AND_PROJECTOR);
	}
}

void LightStorage::light_set_negative(RID p_light, bool p_enable) {
	Light *light = light_owner.get_or_null(p_light);
	ERR_FAIL_NULL(light);

	light->negative = p_enable;
}

void LightStorage::light_set_cull_mask(RID p_light, uint32_t p_mask) {
	Light *light = light_owner.get_or_null(p_light);
	ERR_FAIL_NULL(light);

	light->cull_mask = p_mask;

	light->version++;
	light->dependency.changed_notify(Dependency::DEPENDENCY_CHANGED_LIGHT);
}

void LightStorage::light_set_distance_fade(RID p_light, bool p_enabled, float p_begin, float p_shadow, float p_length) {
	Light *light = light_owner.get_or_null(p_light);
	ERR_FAIL_NULL(light);

	light->distance_fade = p_enabled;
	light->distance_fade_begin = p_begin;
	light->distance_fade_shadow = p_shadow;
	light->distance_fade_length = p_length;
}

void LightStorage::light_set_reverse_cull_face_mode(RID p_light, bool p_enabled) {
	Light *light = light_owner.get_or_null(p_light);
	ERR_FAIL_NULL(light);

	light->reverse_cull = p_enabled;

	light->version++;
	light->dependency.changed_notify(Dependency::DEPENDENCY_CHANGED_LIGHT);
}

void LightStorage::light_set_shadow_caster_mask(RID p_light, uint32_t p_caster_mask) {
	Light *light = light_owner.get_or_null(p_light);
	ERR_FAIL_NULL(light);

	light->shadow_caster_mask = p_caster_mask;

	light->version++;
	light->dependency.changed_notify(Dependency::DEPENDENCY_CHANGED_LIGHT);
}

uint32_t LightStorage::light_get_shadow_caster_mask(RID p_light) const {
	Light *light = light_owner.get_or_null(p_light);
	ERR_FAIL_NULL_V(light, 0);

	return light->shadow_caster_mask;
}

void LightStorage::light_set_bake_mode(RID p_light, RS::LightBakeMode p_bake_mode) {
	Light *light = light_owner.get_or_null(p_light);
	ERR_FAIL_NULL(light);

	light->bake_mode = p_bake_mode;

	light->version++;
	light->dependency.changed_notify(Dependency::DEPENDENCY_CHANGED_LIGHT);
}

void LightStorage::light_set_max_sdfgi_cascade(RID p_light, uint32_t p_cascade) {
	Light *light = light_owner.get_or_null(p_light);
	ERR_FAIL_NULL(light);

	light->max_sdfgi_cascade = p_cascade;

	light->version++;
	light->dependency.changed_notify(Dependency::DEPENDENCY_CHANGED_LIGHT);
}

void LightStorage::light_omni_set_shadow_mode(RID p_light, RS::LightOmniShadowMode p_mode) {
	Light *light = light_owner.get_or_null(p_light);
	ERR_FAIL_NULL(light);

	light->omni_shadow_mode = p_mode;

	light->version++;
	light->dependency.changed_notify(Dependency::DEPENDENCY_CHANGED_LIGHT);

	if (p_mode == RS::LIGHT_OMNI_SHADOW_DUAL_PARABOLOID) {
		shadow_dual_paraboloid_used = true;
	} else if (p_mode == RS::LIGHT_OMNI_SHADOW_CUBE) {
		shadow_cubemaps_used = true;
	}
}

RS::LightOmniShadowMode LightStorage::light_omni_get_shadow_mode(RID p_light) {
	const Light *light = light_owner.get_or_null(p_light);
	ERR_FAIL_NULL_V(light, RS::LIGHT_OMNI_SHADOW_CUBE);

	return light->omni_shadow_mode;
}

void LightStorage::light_directional_set_shadow_mode(RID p_light, RS::LightDirectionalShadowMode p_mode) {
	Light *light = light_owner.get_or_null(p_light);
	ERR_FAIL_NULL(light);

	light->directional_shadow_mode = p_mode;
	light->version++;
	light->dependency.changed_notify(Dependency::DEPENDENCY_CHANGED_LIGHT);
}

void LightStorage::light_directional_set_blend_splits(RID p_light, bool p_enable) {
	Light *light = light_owner.get_or_null(p_light);
	ERR_FAIL_NULL(light);

	light->directional_blend_splits = p_enable;
	light->version++;
	light->dependency.changed_notify(Dependency::DEPENDENCY_CHANGED_LIGHT);
}

bool LightStorage::light_directional_get_blend_splits(RID p_light) const {
	const Light *light = light_owner.get_or_null(p_light);
	ERR_FAIL_NULL_V(light, false);

	return light->directional_blend_splits;
}

void LightStorage::light_directional_set_sky_mode(RID p_light, RS::LightDirectionalSkyMode p_mode) {
	Light *light = light_owner.get_or_null(p_light);
	ERR_FAIL_NULL(light);

	light->directional_sky_mode = p_mode;
}

RS::LightDirectionalSkyMode LightStorage::light_directional_get_sky_mode(RID p_light) const {
	const Light *light = light_owner.get_or_null(p_light);
	ERR_FAIL_NULL_V(light, RS::LIGHT_DIRECTIONAL_SKY_MODE_LIGHT_AND_SKY);

	return light->directional_sky_mode;
}

RS::LightDirectionalShadowMode LightStorage::light_directional_get_shadow_mode(RID p_light) {
	const Light *light = light_owner.get_or_null(p_light);
	ERR_FAIL_NULL_V(light, RS::LIGHT_DIRECTIONAL_SHADOW_ORTHOGONAL);

	return light->directional_shadow_mode;
}

uint32_t LightStorage::light_get_max_sdfgi_cascade(RID p_light) {
	const Light *light = light_owner.get_or_null(p_light);
	ERR_FAIL_NULL_V(light, 0);

	return light->max_sdfgi_cascade;
}

RS::LightBakeMode LightStorage::light_get_bake_mode(RID p_light) {
	const Light *light = light_owner.get_or_null(p_light);
	ERR_FAIL_NULL_V(light, RS::LIGHT_BAKE_DISABLED);

	return light->bake_mode;
}

uint64_t LightStorage::light_get_version(RID p_light) const {
	const Light *light = light_owner.get_or_null(p_light);
	ERR_FAIL_NULL_V(light, 0);

	return light->version;
}

uint32_t LightStorage::light_get_cull_mask(RID p_light) const {
	const Light *light = light_owner.get_or_null(p_light);
	ERR_FAIL_NULL_V(light, 0);

	return light->cull_mask;
}

AABB LightStorage::light_get_aabb(RID p_light) const {
	const Light *light = light_owner.get_or_null(p_light);
	ERR_FAIL_NULL_V(light, AABB());

	switch (light->type) {
		case RS::LIGHT_SPOT: {
			float len = light->param[RS::LIGHT_PARAM_RANGE];
			float size = Math::tan(Math::deg_to_rad(light->param[RS::LIGHT_PARAM_SPOT_ANGLE])) * len;
			return AABB(Vector3(-size, -size, -len), Vector3(size * 2, size * 2, len));
		};
		case RS::LIGHT_OMNI: {
			float r = light->param[RS::LIGHT_PARAM_RANGE];
			return AABB(-Vector3(r, r, r), Vector3(r, r, r) * 2);
		};
		case RS::LIGHT_DIRECTIONAL: {
			return AABB();
		};
	}

	ERR_FAIL_V(AABB());
}

Dependency *LightStorage::light_get_dependency(RID p_light) const {
	Light *light = light_owner.get_or_null(p_light);
	ERR_FAIL_NULL_V(light, nullptr);

	return &light->dependency;
}

/* LIGHT INSTANCE API */

RID LightStorage::light_instance_create(RID p_light) {
	RID li = light_instance_owner.make_rid(LightInstance());

	LightInstance *light_instance = light_instance_owner.get_or_null(li);

	light_instance->self = li;
	light_instance->light = p_light;
	light_instance->light_type = light_get_type(p_light);
	if (light_instance->light_type != RS::LIGHT_DIRECTIONAL) {
		light_instance->forward_id = ForwardIDStorage::get_singleton()->allocate_forward_id(light_instance->light_type == RS::LIGHT_OMNI ? FORWARD_ID_TYPE_OMNI_LIGHT : FORWARD_ID_TYPE_SPOT_LIGHT);
	}

	return li;
}

void LightStorage::light_instance_free(RID p_light) {
	LightInstance *light_instance = light_instance_owner.get_or_null(p_light);

	//remove from shadow atlases..
	for (const RID &E : light_instance->shadow_atlases) {
		ShadowAtlas *shadow_atlas = shadow_atlas_owner.get_or_null(E);
		ERR_CONTINUE(!shadow_atlas->shadow_owners.has(p_light));
		uint32_t key = shadow_atlas->shadow_owners[p_light];
		uint32_t q = (key >> QUADRANT_SHIFT) & 0x3;
		uint32_t s = key & SHADOW_INDEX_MASK;

		shadow_atlas->quadrants[q].shadows.write[s].owner = RID();

		if (key & OMNI_LIGHT_FLAG) {
			// Omni lights use two atlas spots, make sure to clear the other as well
			shadow_atlas->quadrants[q].shadows.write[s + 1].owner = RID();
		}

		shadow_atlas->shadow_owners.erase(p_light);
	}

	if (light_instance->light_type != RS::LIGHT_DIRECTIONAL) {
		ForwardIDStorage::get_singleton()->free_forward_id(light_instance->light_type == RS::LIGHT_OMNI ? FORWARD_ID_TYPE_OMNI_LIGHT : FORWARD_ID_TYPE_SPOT_LIGHT, light_instance->forward_id);
	}
	light_instance_owner.free(p_light);
}

void LightStorage::light_instance_set_transform(RID p_light_instance, const Transform3D &p_transform) {
	LightInstance *light_instance = light_instance_owner.get_or_null(p_light_instance);
	ERR_FAIL_NULL(light_instance);

	light_instance->transform = p_transform;
}

void LightStorage::light_instance_set_aabb(RID p_light_instance, const AABB &p_aabb) {
	LightInstance *light_instance = light_instance_owner.get_or_null(p_light_instance);
	ERR_FAIL_NULL(light_instance);

	light_instance->aabb = p_aabb;
}

void LightStorage::light_instance_set_shadow_transform(RID p_light_instance, const Projection &p_projection, const Transform3D &p_transform, float p_far, float p_split, int p_pass, float p_shadow_texel_size, float p_bias_scale, float p_range_begin, const Vector2 &p_uv_scale) {
	LightInstance *light_instance = light_instance_owner.get_or_null(p_light_instance);
	ERR_FAIL_NULL(light_instance);

	ERR_FAIL_INDEX(p_pass, 6);

	light_instance->shadow_transform[p_pass].camera = p_projection;
	light_instance->shadow_transform[p_pass].transform = p_transform;
	light_instance->shadow_transform[p_pass].farplane = p_far;
	light_instance->shadow_transform[p_pass].split = p_split;
	light_instance->shadow_transform[p_pass].bias_scale = p_bias_scale;
	light_instance->shadow_transform[p_pass].range_begin = p_range_begin;
	light_instance->shadow_transform[p_pass].shadow_texel_size = p_shadow_texel_size;
	light_instance->shadow_transform[p_pass].uv_scale = p_uv_scale;
}

void LightStorage::light_instance_mark_visible(RID p_light_instance) {
	LightInstance *light_instance = light_instance_owner.get_or_null(p_light_instance);
	ERR_FAIL_NULL(light_instance);

	light_instance->last_scene_pass = RendererSceneRenderRD::get_singleton()->get_scene_pass();
}

/* LIGHT DATA */

void LightStorage::free_light_data() {
	if (directional_light_buffer.is_valid()) {
		RD::get_singleton()->free(directional_light_buffer);
		directional_light_buffer = RID();
	}

	if (omni_light_buffer.is_valid()) {
		RD::get_singleton()->free(omni_light_buffer);
		omni_light_buffer = RID();
	}

	if (spot_light_buffer.is_valid()) {
		RD::get_singleton()->free(spot_light_buffer);
		spot_light_buffer = RID();
	}

	if (directional_lights != nullptr) {
		memdelete_arr(directional_lights);
		directional_lights = nullptr;
	}

	if (omni_lights != nullptr) {
		memdelete_arr(omni_lights);
		omni_lights = nullptr;
	}

	if (spot_lights != nullptr) {
		memdelete_arr(spot_lights);
		spot_lights = nullptr;
	}

	if (omni_light_sort != nullptr) {
		memdelete_arr(omni_light_sort);
		omni_light_sort = nullptr;
	}

	if (spot_light_sort != nullptr) {
		memdelete_arr(spot_light_sort);
		spot_light_sort = nullptr;
	}
}

void LightStorage::set_max_lights(const uint32_t p_max_lights) {
	max_lights = p_max_lights;

	uint32_t light_buffer_size = max_lights * sizeof(LightData);
	omni_lights = memnew_arr(LightData, max_lights);
	omni_light_buffer = RD::get_singleton()->storage_buffer_create(light_buffer_size);
	omni_light_sort = memnew_arr(LightInstanceDepthSort, max_lights);
	spot_lights = memnew_arr(LightData, max_lights);
	spot_light_buffer = RD::get_singleton()->storage_buffer_create(light_buffer_size);
	spot_light_sort = memnew_arr(LightInstanceDepthSort, max_lights);
	//defines += "\n#define MAX_LIGHT_DATA_STRUCTS " + itos(max_lights) + "\n";

	max_directional_lights = RendererSceneRender::MAX_DIRECTIONAL_LIGHTS;
	uint32_t directional_light_buffer_size = max_directional_lights * sizeof(DirectionalLightData);
	directional_lights = memnew_arr(DirectionalLightData, max_directional_lights);
	directional_light_buffer = RD::get_singleton()->uniform_buffer_create(directional_light_buffer_size);
}

void LightStorage::update_light_buffers(RenderDataRD *p_render_data, const PagedArray<RID> &p_lights, const Transform3D &p_camera_transform, RID p_shadow_atlas, bool p_using_shadows, uint32_t &r_directional_light_count, uint32_t &r_positional_light_count, bool &r_directional_light_soft_shadows) {
	ForwardIDStorage *forward_id_storage = ForwardIDStorage::get_singleton();
	RendererRD::TextureStorage *texture_storage = RendererRD::TextureStorage::get_singleton();

	Transform3D inverse_transform = p_camera_transform.affine_inverse();

	r_directional_light_count = 0;
	r_positional_light_count = 0;

	omni_light_count = 0;
	spot_light_count = 0;

	r_directional_light_soft_shadows = false;

	for (int i = 0; i < (int)p_lights.size(); i++) {
		LightInstance *light_instance = light_instance_owner.get_or_null(p_lights[i]);
		if (!light_instance) {
			continue;
		}
		Light *light = light_owner.get_or_null(light_instance->light);

		ERR_CONTINUE(light == nullptr);

		switch (light->type) {
			case RS::LIGHT_DIRECTIONAL: {
				if (r_directional_light_count >= max_directional_lights || light->directional_sky_mode == RS::LIGHT_DIRECTIONAL_SKY_MODE_SKY_ONLY) {
					continue;
				}

				DirectionalLightData &light_data = directional_lights[r_directional_light_count];

				Transform3D light_transform = light_instance->transform;

				Vector3 direction = inverse_transform.basis.xform(light_transform.basis.xform(Vector3(0, 0, 1))).normalized();

				light_data.direction[0] = direction.x;
				light_data.direction[1] = direction.y;
				light_data.direction[2] = direction.z;

				float sign = light->negative ? -1 : 1;

				light_data.energy = sign * light->param[RS::LIGHT_PARAM_ENERGY];

				if (RendererSceneRenderRD::get_singleton()->is_using_physical_light_units()) {
					light_data.energy *= light->param[RS::LIGHT_PARAM_INTENSITY];
				} else {
					light_data.energy *= Math_PI;
				}

				if (p_render_data->camera_attributes.is_valid()) {
					light_data.energy *= RSG::camera_attributes->camera_attributes_get_exposure_normalization_factor(p_render_data->camera_attributes);
				}

				Color linear_col = light->color.srgb_to_linear();
				light_data.color[0] = linear_col.r;
				light_data.color[1] = linear_col.g;
				light_data.color[2] = linear_col.b;

				light_data.specular = light->param[RS::LIGHT_PARAM_SPECULAR];
				light_data.volumetric_fog_energy = light->param[RS::LIGHT_PARAM_VOLUMETRIC_FOG_ENERGY];
				light_data.mask = light->cull_mask;

				float size = light->param[RS::LIGHT_PARAM_SIZE];

				light_data.size = 1.0 - Math::cos(Math::deg_to_rad(size)); //angle to cosine offset

				light_data.shadow_opacity = (p_using_shadows && light->shadow)
						? light->param[RS::LIGHT_PARAM_SHADOW_OPACITY]
						: 0.0;

				float angular_diameter = light->param[RS::LIGHT_PARAM_SIZE];
				if (angular_diameter > 0.0) {
					// I know tan(0) is 0, but let's not risk it with numerical precision.
					// technically this will keep expanding until reaching the sun, but all we care
					// is expand until we reach the radius of the near plane (there can't be more occluders than that)
					angular_diameter = Math::tan(Math::deg_to_rad(angular_diameter));
					if (light->shadow && light->param[RS::LIGHT_PARAM_SHADOW_BLUR] > 0.0) {
						// Only enable PCSS-like soft shadows if blurring is enabled.
						// Otherwise, performance would decrease with no visual difference.
						r_directional_light_soft_shadows = true;
					}
				} else {
					angular_diameter = 0.0;
				}

				light_data.bake_mode = light->bake_mode;

				if (light_data.shadow_opacity > 0.001) {
					RS::LightDirectionalShadowMode smode = light->directional_shadow_mode;

					light_data.soft_shadow_scale = light->param[RS::LIGHT_PARAM_SHADOW_BLUR];
					light_data.softshadow_angle = angular_diameter;

					if (angular_diameter <= 0.0) {
						light_data.soft_shadow_scale *= RendererSceneRenderRD::get_singleton()->directional_shadow_quality_radius_get(); // Only use quality radius for PCF
					}

					int limit = smode == RS::LIGHT_DIRECTIONAL_SHADOW_ORTHOGONAL ? 0 : (smode == RS::LIGHT_DIRECTIONAL_SHADOW_PARALLEL_2_SPLITS ? 1 : 3);
					light_data.blend_splits = (smode != RS::LIGHT_DIRECTIONAL_SHADOW_ORTHOGONAL) && light->directional_blend_splits;
					for (int j = 0; j < 4; j++) {
						Rect2 atlas_rect = light_instance->shadow_transform[j].atlas_rect;
						Projection correction;
						correction.set_depth_correction(false, true, false);
						Projection matrix = correction * light_instance->shadow_transform[j].camera;
						float split = light_instance->shadow_transform[MIN(limit, j)].split;

						Projection bias;
						bias.set_light_bias();
						Projection rectm;
						rectm.set_light_atlas_rect(atlas_rect);

						Transform3D modelview = (inverse_transform * light_instance->shadow_transform[j].transform).inverse();

						Projection shadow_mtx = rectm * bias * matrix * modelview;
						light_data.shadow_split_offsets[j] = split;
						float bias_scale = light_instance->shadow_transform[j].bias_scale * light_data.soft_shadow_scale;
						light_data.shadow_bias[j] = light->param[RS::LIGHT_PARAM_SHADOW_BIAS] / 100.0 * bias_scale;
						light_data.shadow_normal_bias[j] = light->param[RS::LIGHT_PARAM_SHADOW_NORMAL_BIAS] * light_instance->shadow_transform[j].shadow_texel_size;
						light_data.shadow_transmittance_bias[j] = light->param[RS::LIGHT_PARAM_TRANSMITTANCE_BIAS] / 100.0 * bias_scale;
						light_data.shadow_z_range[j] = light_instance->shadow_transform[j].farplane;
						light_data.shadow_range_begin[j] = light_instance->shadow_transform[j].range_begin;
						RendererRD::MaterialStorage::store_camera(shadow_mtx, light_data.shadow_matrices[j]);

						Vector2 uv_scale = light_instance->shadow_transform[j].uv_scale;
						uv_scale *= atlas_rect.size; //adapt to atlas size
						switch (j) {
							case 0: {
								light_data.uv_scale1[0] = uv_scale.x;
								light_data.uv_scale1[1] = uv_scale.y;
							} break;
							case 1: {
								light_data.uv_scale2[0] = uv_scale.x;
								light_data.uv_scale2[1] = uv_scale.y;
							} break;
							case 2: {
								light_data.uv_scale3[0] = uv_scale.x;
								light_data.uv_scale3[1] = uv_scale.y;
							} break;
							case 3: {
								light_data.uv_scale4[0] = uv_scale.x;
								light_data.uv_scale4[1] = uv_scale.y;
							} break;
						}
					}

					float fade_start = light->param[RS::LIGHT_PARAM_SHADOW_FADE_START];
					light_data.fade_from = -light_data.shadow_split_offsets[3] * MIN(fade_start, 0.999); //using 1.0 would break smoothstep
					light_data.fade_to = -light_data.shadow_split_offsets[3];
				}

				r_directional_light_count++;
			} break;
			case RS::LIGHT_OMNI: {
				if (omni_light_count >= max_lights) {
					continue;
				}

				Transform3D light_transform = light_instance->transform;
				const real_t distance = p_camera_transform.origin.distance_to(light_transform.origin);

				if (light->distance_fade) {
					const float fade_begin = light->distance_fade_begin;
					const float fade_length = light->distance_fade_length;

					if (distance > fade_begin) {
						if (distance > fade_begin + fade_length) {
							// Out of range, don't draw this light to improve performance.
							continue;
						}
					}
				}

				omni_light_sort[omni_light_count].light_instance = light_instance;
				omni_light_sort[omni_light_count].light = light;
				omni_light_sort[omni_light_count].depth = distance;
				omni_light_count++;
			} break;
			case RS::LIGHT_SPOT: {
				if (spot_light_count >= max_lights) {
					continue;
				}

				Transform3D light_transform = light_instance->transform;
				const real_t distance = p_camera_transform.origin.distance_to(light_transform.origin);

				if (light->distance_fade) {
					const float fade_begin = light->distance_fade_begin;
					const float fade_length = light->distance_fade_length;

					if (distance > fade_begin) {
						if (distance > fade_begin + fade_length) {
							// Out of range, don't draw this light to improve performance.
							continue;
						}
					}
				}

				spot_light_sort[spot_light_count].light_instance = light_instance;
				spot_light_sort[spot_light_count].light = light;
				spot_light_sort[spot_light_count].depth = distance;
				spot_light_count++;
			} break;
		}

		light_instance->last_pass = RSG::rasterizer->get_frame_number();
	}

	if (omni_light_count) {
		SortArray<LightInstanceDepthSort> sorter;
		sorter.sort(omni_light_sort, omni_light_count);
	}

	if (spot_light_count) {
		SortArray<LightInstanceDepthSort> sorter;
		sorter.sort(spot_light_sort, spot_light_count);
	}

	bool using_forward_ids = forward_id_storage->uses_forward_ids();

	for (uint32_t i = 0; i < (omni_light_count + spot_light_count); i++) {
		uint32_t index = (i < omni_light_count) ? i : i - (omni_light_count);
		LightData &light_data = (i < omni_light_count) ? omni_lights[index] : spot_lights[index];
		RS::LightType type = (i < omni_light_count) ? RS::LIGHT_OMNI : RS::LIGHT_SPOT;
		LightInstance *light_instance = (i < omni_light_count) ? omni_light_sort[index].light_instance : spot_light_sort[index].light_instance;
		Light *light = (i < omni_light_count) ? omni_light_sort[index].light : spot_light_sort[index].light;
		real_t distance = (i < omni_light_count) ? omni_light_sort[index].depth : spot_light_sort[index].depth;

		if (using_forward_ids) {
			forward_id_storage->map_forward_id(type == RS::LIGHT_OMNI ? RendererRD::FORWARD_ID_TYPE_OMNI_LIGHT : RendererRD::FORWARD_ID_TYPE_SPOT_LIGHT, light_instance->forward_id, index, light_instance->last_pass);
		}

		Transform3D light_transform = light_instance->transform;

		float sign = light->negative ? -1 : 1;
		Color linear_col = light->color.srgb_to_linear();

		light_data.attenuation = light->param[RS::LIGHT_PARAM_ATTENUATION];

		// Reuse fade begin, fade length and distance for shadow LOD determination later.
		float fade_begin = 0.0;
		float fade_shadow = 0.0;
		float fade_length = 0.0;

		float fade = 1.0;
		float shadow_opacity_fade = 1.0;
		if (light->distance_fade) {
			fade_begin = light->distance_fade_begin;
			fade_shadow = light->distance_fade_shadow;
			fade_length = light->distance_fade_length;

			// Use `smoothstep()` to make opacity changes more gradual and less noticeable to the player.
			if (distance > fade_begin) {
				fade = Math::smoothstep(0.0f, 1.0f, 1.0f - float(distance - fade_begin) / fade_length);
			}

			if (distance > fade_shadow) {
				shadow_opacity_fade = Math::smoothstep(0.0f, 1.0f, 1.0f - float(distance - fade_shadow) / fade_length);
			}
		}

		float energy = sign * light->param[RS::LIGHT_PARAM_ENERGY] * fade;

		if (RendererSceneRenderRD::get_singleton()->is_using_physical_light_units()) {
			energy *= light->param[RS::LIGHT_PARAM_INTENSITY];

			// Convert from Luminous Power to Luminous Intensity
			if (type == RS::LIGHT_OMNI) {
				energy *= 1.0 / (Math_PI * 4.0);
			} else {
				// Spot Lights are not physically accurate, Luminous Intensity should change in relation to the cone angle.
				// We make this assumption to keep them easy to control.
				energy *= 1.0 / Math_PI;
			}
		} else {
			energy *= Math_PI;
		}

		if (p_render_data->camera_attributes.is_valid()) {
			energy *= RSG::camera_attributes->camera_attributes_get_exposure_normalization_factor(p_render_data->camera_attributes);
		}

		light_data.color[0] = linear_col.r * energy;
		light_data.color[1] = linear_col.g * energy;
		light_data.color[2] = linear_col.b * energy;
		light_data.specular_amount = light->param[RS::LIGHT_PARAM_SPECULAR] * 2.0;
		light_data.volumetric_fog_energy = light->param[RS::LIGHT_PARAM_VOLUMETRIC_FOG_ENERGY];
		light_data.bake_mode = light->bake_mode;

		float radius = MAX(0.001, light->param[RS::LIGHT_PARAM_RANGE]);
		light_data.inv_radius = 1.0 / radius;

		Vector3 pos = inverse_transform.xform(light_transform.origin);

		light_data.position[0] = pos.x;
		light_data.position[1] = pos.y;
		light_data.position[2] = pos.z;

		Vector3 direction = inverse_transform.basis.xform(light_transform.basis.xform(Vector3(0, 0, -1))).normalized();

		light_data.direction[0] = direction.x;
		light_data.direction[1] = direction.y;
		light_data.direction[2] = direction.z;

		float size = light->param[RS::LIGHT_PARAM_SIZE];

		light_data.size = size;

		light_data.inv_spot_attenuation = 1.0f / light->param[RS::LIGHT_PARAM_SPOT_ATTENUATION];
		float spot_angle = light->param[RS::LIGHT_PARAM_SPOT_ANGLE];
		light_data.cos_spot_angle = Math::cos(Math::deg_to_rad(spot_angle));

		light_data.mask = light->cull_mask;

		light_data.atlas_rect[0] = 0;
		light_data.atlas_rect[1] = 0;
		light_data.atlas_rect[2] = 0;
		light_data.atlas_rect[3] = 0;

		RID projector = light->projector;

		if (projector.is_valid()) {
			Rect2 rect = texture_storage->decal_atlas_get_texture_rect(projector);

			if (type == RS::LIGHT_SPOT) {
				light_data.projector_rect[0] = rect.position.x;
				light_data.projector_rect[1] = rect.position.y + rect.size.height; //flip because shadow is flipped
				light_data.projector_rect[2] = rect.size.width;
				light_data.projector_rect[3] = -rect.size.height;
			} else {
				light_data.projector_rect[0] = rect.position.x;
				light_data.projector_rect[1] = rect.position.y;
				light_data.projector_rect[2] = rect.size.width;
				light_data.projector_rect[3] = rect.size.height * 0.5; //used by dp, so needs to be half
			}
		} else {
			light_data.projector_rect[0] = 0;
			light_data.projector_rect[1] = 0;
			light_data.projector_rect[2] = 0;
			light_data.projector_rect[3] = 0;
		}

		const bool needs_shadow =
				p_using_shadows &&
				owns_shadow_atlas(p_shadow_atlas) &&
				shadow_atlas_owns_light_instance(p_shadow_atlas, light_instance->self) &&
				light->shadow;

		bool in_shadow_range = true;
		if (needs_shadow && light->distance_fade) {
			if (distance > light->distance_fade_shadow + light->distance_fade_length) {
				// Out of range, don't draw shadows to improve performance.
				in_shadow_range = false;
			}
		}

		if (needs_shadow && in_shadow_range) {
			// fill in the shadow information

			light_data.shadow_opacity = light->param[RS::LIGHT_PARAM_SHADOW_OPACITY] * shadow_opacity_fade;

			float shadow_texel_size = light_instance_get_shadow_texel_size(light_instance->self, p_shadow_atlas);
			light_data.shadow_normal_bias = light->param[RS::LIGHT_PARAM_SHADOW_NORMAL_BIAS] * shadow_texel_size * 10.0;

			if (type == RS::LIGHT_SPOT) {
				light_data.shadow_bias = light->param[RS::LIGHT_PARAM_SHADOW_BIAS] / 100.0;
			} else { //omni
				light_data.shadow_bias = light->param[RS::LIGHT_PARAM_SHADOW_BIAS];
			}

			light_data.transmittance_bias = light->param[RS::LIGHT_PARAM_TRANSMITTANCE_BIAS];

			Vector2i omni_offset;
			Rect2 rect = light_instance_get_shadow_atlas_rect(light_instance->self, p_shadow_atlas, omni_offset);

			light_data.atlas_rect[0] = rect.position.x;
			light_data.atlas_rect[1] = rect.position.y;
			light_data.atlas_rect[2] = rect.size.width;
			light_data.atlas_rect[3] = rect.size.height;

			light_data.soft_shadow_scale = light->param[RS::LIGHT_PARAM_SHADOW_BLUR];

			if (type == RS::LIGHT_OMNI) {
				Transform3D proj = (inverse_transform * light_transform).inverse();

				RendererRD::MaterialStorage::store_transform(proj, light_data.shadow_matrix);

				if (size > 0.0 && light_data.soft_shadow_scale > 0.0) {
					// Only enable PCSS-like soft shadows if blurring is enabled.
					// Otherwise, performance would decrease with no visual difference.
					light_data.soft_shadow_size = size;
				} else {
					light_data.soft_shadow_size = 0.0;
					light_data.soft_shadow_scale *= RendererSceneRenderRD::get_singleton()->shadows_quality_radius_get(); // Only use quality radius for PCF
				}

				light_data.direction[0] = omni_offset.x * float(rect.size.width);
				light_data.direction[1] = omni_offset.y * float(rect.size.height);
			} else if (type == RS::LIGHT_SPOT) {
				Transform3D modelview = (inverse_transform * light_transform).inverse();
				Projection bias;
				bias.set_light_bias();

				Projection correction;
				correction.set_depth_correction(false, true, false);
				Projection cm = correction * light_instance->shadow_transform[0].camera;
				Projection shadow_mtx = bias * cm * modelview;
				RendererRD::MaterialStorage::store_camera(shadow_mtx, light_data.shadow_matrix);

				if (size > 0.0 && light_data.soft_shadow_scale > 0.0) {
					// Only enable PCSS-like soft shadows if blurring is enabled.
					// Otherwise, performance would decrease with no visual difference.
					float half_np = cm.get_z_near() * Math::tan(Math::deg_to_rad(spot_angle));
					light_data.soft_shadow_size = (size * 0.5 / radius) / (half_np / cm.get_z_near()) * rect.size.width;
				} else {
					light_data.soft_shadow_size = 0.0;
					light_data.soft_shadow_scale *= RendererSceneRenderRD::get_singleton()->shadows_quality_radius_get(); // Only use quality radius for PCF
				}
				light_data.shadow_bias *= light_data.soft_shadow_scale;
			}
		} else {
			light_data.shadow_opacity = 0.0;
		}

		light_instance->cull_mask = light->cull_mask;

		// hook for subclass to do further processing.
		RendererSceneRenderRD::get_singleton()->setup_added_light(type, light_transform, radius, spot_angle);

		r_positional_light_count++;
	}

	//update without barriers
	if (omni_light_count) {
		RD::get_singleton()->buffer_update(omni_light_buffer, 0, sizeof(LightData) * omni_light_count, omni_lights);
	}

	if (spot_light_count) {
		RD::get_singleton()->buffer_update(spot_light_buffer, 0, sizeof(LightData) * spot_light_count, spot_lights);
	}

	if (r_directional_light_count) {
		RD::get_singleton()->buffer_update(directional_light_buffer, 0, sizeof(DirectionalLightData) * r_directional_light_count, directional_lights);
	}
}

/* REFLECTION PROBE */

RID LightStorage::reflection_probe_allocate() {
	return reflection_probe_owner.allocate_rid();
}

void LightStorage::reflection_probe_initialize(RID p_reflection_probe) {
	reflection_probe_owner.initialize_rid(p_reflection_probe, ReflectionProbe());
}

void LightStorage::reflection_probe_free(RID p_rid) {
	ReflectionProbe *reflection_probe = reflection_probe_owner.get_or_null(p_rid);
	reflection_probe->dependency.deleted_notify(p_rid);
	reflection_probe_owner.free(p_rid);
}

void LightStorage::reflection_probe_set_update_mode(RID p_probe, RS::ReflectionProbeUpdateMode p_mode) {
	ReflectionProbe *reflection_probe = reflection_probe_owner.get_or_null(p_probe);
	ERR_FAIL_NULL(reflection_probe);

	reflection_probe->update_mode = p_mode;
	reflection_probe->dependency.changed_notify(Dependency::DEPENDENCY_CHANGED_REFLECTION_PROBE);
}

void LightStorage::reflection_probe_set_intensity(RID p_probe, float p_intensity) {
	ReflectionProbe *reflection_probe = reflection_probe_owner.get_or_null(p_probe);
	ERR_FAIL_NULL(reflection_probe);

	reflection_probe->intensity = p_intensity;
}

void LightStorage::reflection_probe_set_blend_distance(RID p_probe, float p_blend_distance) {
	ReflectionProbe *reflection_probe = reflection_probe_owner.get_or_null(p_probe);
	ERR_FAIL_NULL(reflection_probe);

	reflection_probe->blend_distance = p_blend_distance;
}

void LightStorage::reflection_probe_set_ambient_mode(RID p_probe, RS::ReflectionProbeAmbientMode p_mode) {
	ReflectionProbe *reflection_probe = reflection_probe_owner.get_or_null(p_probe);
	ERR_FAIL_NULL(reflection_probe);

	reflection_probe->ambient_mode = p_mode;
}

void LightStorage::reflection_probe_set_ambient_color(RID p_probe, const Color &p_color) {
	ReflectionProbe *reflection_probe = reflection_probe_owner.get_or_null(p_probe);
	ERR_FAIL_NULL(reflection_probe);

	reflection_probe->ambient_color = p_color;
}

void LightStorage::reflection_probe_set_ambient_energy(RID p_probe, float p_energy) {
	ReflectionProbe *reflection_probe = reflection_probe_owner.get_or_null(p_probe);
	ERR_FAIL_NULL(reflection_probe);

	reflection_probe->ambient_color_energy = p_energy;
}

void LightStorage::reflection_probe_set_max_distance(RID p_probe, float p_distance) {
	ReflectionProbe *reflection_probe = reflection_probe_owner.get_or_null(p_probe);
	ERR_FAIL_NULL(reflection_probe);

	reflection_probe->max_distance = p_distance;

	reflection_probe->dependency.changed_notify(Dependency::DEPENDENCY_CHANGED_REFLECTION_PROBE);
}

void LightStorage::reflection_probe_set_size(RID p_probe, const Vector3 &p_size) {
	ReflectionProbe *reflection_probe = reflection_probe_owner.get_or_null(p_probe);
	ERR_FAIL_NULL(reflection_probe);

	if (reflection_probe->size == p_size) {
		return;
	}
	reflection_probe->size = p_size;
	reflection_probe->dependency.changed_notify(Dependency::DEPENDENCY_CHANGED_REFLECTION_PROBE);
}

void LightStorage::reflection_probe_set_origin_offset(RID p_probe, const Vector3 &p_offset) {
	ReflectionProbe *reflection_probe = reflection_probe_owner.get_or_null(p_probe);
	ERR_FAIL_NULL(reflection_probe);

	reflection_probe->origin_offset = p_offset;
	reflection_probe->dependency.changed_notify(Dependency::DEPENDENCY_CHANGED_REFLECTION_PROBE);
}

void LightStorage::reflection_probe_set_as_interior(RID p_probe, bool p_enable) {
	ReflectionProbe *reflection_probe = reflection_probe_owner.get_or_null(p_probe);
	ERR_FAIL_NULL(reflection_probe);

	reflection_probe->interior = p_enable;
	reflection_probe->dependency.changed_notify(Dependency::DEPENDENCY_CHANGED_REFLECTION_PROBE);
}

void LightStorage::reflection_probe_set_enable_box_projection(RID p_probe, bool p_enable) {
	ReflectionProbe *reflection_probe = reflection_probe_owner.get_or_null(p_probe);
	ERR_FAIL_NULL(reflection_probe);

	reflection_probe->box_projection = p_enable;
}

void LightStorage::reflection_probe_set_enable_shadows(RID p_probe, bool p_enable) {
	ReflectionProbe *reflection_probe = reflection_probe_owner.get_or_null(p_probe);
	ERR_FAIL_NULL(reflection_probe);

	reflection_probe->enable_shadows = p_enable;
	reflection_probe->dependency.changed_notify(Dependency::DEPENDENCY_CHANGED_REFLECTION_PROBE);
}

void LightStorage::reflection_probe_set_cull_mask(RID p_probe, uint32_t p_layers) {
	ReflectionProbe *reflection_probe = reflection_probe_owner.get_or_null(p_probe);
	ERR_FAIL_NULL(reflection_probe);

	reflection_probe->cull_mask = p_layers;
	reflection_probe->dependency.changed_notify(Dependency::DEPENDENCY_CHANGED_REFLECTION_PROBE);
}

void LightStorage::reflection_probe_set_reflection_mask(RID p_probe, uint32_t p_layers) {
	ReflectionProbe *reflection_probe = reflection_probe_owner.get_or_null(p_probe);
	ERR_FAIL_NULL(reflection_probe);

	reflection_probe->reflection_mask = p_layers;
	reflection_probe->dependency.changed_notify(Dependency::DEPENDENCY_CHANGED_REFLECTION_PROBE);
}

void LightStorage::reflection_probe_set_resolution(RID p_probe, int p_resolution) {
	ReflectionProbe *reflection_probe = reflection_probe_owner.get_or_null(p_probe);
	ERR_FAIL_NULL(reflection_probe);
	ERR_FAIL_COND(p_resolution < 32);

	reflection_probe->resolution = p_resolution;
}

void LightStorage::reflection_probe_set_mesh_lod_threshold(RID p_probe, float p_ratio) {
	ReflectionProbe *reflection_probe = reflection_probe_owner.get_or_null(p_probe);
	ERR_FAIL_NULL(reflection_probe);

	reflection_probe->mesh_lod_threshold = p_ratio;

	reflection_probe->dependency.changed_notify(Dependency::DEPENDENCY_CHANGED_REFLECTION_PROBE);
}

void LightStorage::reflection_probe_set_baked_exposure(RID p_probe, float p_exposure) {
	ReflectionProbe *reflection_probe = reflection_probe_owner.get_or_null(p_probe);
	ERR_FAIL_NULL(reflection_probe);

	reflection_probe->baked_exposure = p_exposure;
}

AABB LightStorage::reflection_probe_get_aabb(RID p_probe) const {
	const ReflectionProbe *reflection_probe = reflection_probe_owner.get_or_null(p_probe);
	ERR_FAIL_NULL_V(reflection_probe, AABB());

	AABB aabb;
	aabb.position = -reflection_probe->size / 2;
	aabb.size = reflection_probe->size;

	return aabb;
}

RS::ReflectionProbeUpdateMode LightStorage::reflection_probe_get_update_mode(RID p_probe) const {
	const ReflectionProbe *reflection_probe = reflection_probe_owner.get_or_null(p_probe);
	ERR_FAIL_NULL_V(reflection_probe, RS::REFLECTION_PROBE_UPDATE_ALWAYS);

	return reflection_probe->update_mode;
}

uint32_t LightStorage::reflection_probe_get_cull_mask(RID p_probe) const {
	const ReflectionProbe *reflection_probe = reflection_probe_owner.get_or_null(p_probe);
	ERR_FAIL_NULL_V(reflection_probe, 0);

	return reflection_probe->cull_mask;
}

uint32_t LightStorage::reflection_probe_get_reflection_mask(RID p_probe) const {
	const ReflectionProbe *reflection_probe = reflection_probe_owner.get_or_null(p_probe);
	ERR_FAIL_NULL_V(reflection_probe, 0);

	return reflection_probe->reflection_mask;
}

Vector3 LightStorage::reflection_probe_get_size(RID p_probe) const {
	const ReflectionProbe *reflection_probe = reflection_probe_owner.get_or_null(p_probe);
	ERR_FAIL_NULL_V(reflection_probe, Vector3());

	return reflection_probe->size;
}

Vector3 LightStorage::reflection_probe_get_origin_offset(RID p_probe) const {
	const ReflectionProbe *reflection_probe = reflection_probe_owner.get_or_null(p_probe);
	ERR_FAIL_NULL_V(reflection_probe, Vector3());

	return reflection_probe->origin_offset;
}

bool LightStorage::reflection_probe_renders_shadows(RID p_probe) const {
	const ReflectionProbe *reflection_probe = reflection_probe_owner.get_or_null(p_probe);
	ERR_FAIL_NULL_V(reflection_probe, false);

	return reflection_probe->enable_shadows;
}

float LightStorage::reflection_probe_get_origin_max_distance(RID p_probe) const {
	const ReflectionProbe *reflection_probe = reflection_probe_owner.get_or_null(p_probe);
	ERR_FAIL_NULL_V(reflection_probe, 0);

	return reflection_probe->max_distance;
}

float LightStorage::reflection_probe_get_mesh_lod_threshold(RID p_probe) const {
	const ReflectionProbe *reflection_probe = reflection_probe_owner.get_or_null(p_probe);
	ERR_FAIL_NULL_V(reflection_probe, 0);

	return reflection_probe->mesh_lod_threshold;
}

int LightStorage::reflection_probe_get_resolution(RID p_probe) const {
	const ReflectionProbe *reflection_probe = reflection_probe_owner.get_or_null(p_probe);
	ERR_FAIL_NULL_V(reflection_probe, 0);

	return reflection_probe->resolution;
}

float LightStorage::reflection_probe_get_baked_exposure(RID p_probe) const {
	const ReflectionProbe *reflection_probe = reflection_probe_owner.get_or_null(p_probe);
	ERR_FAIL_NULL_V(reflection_probe, 1.0);

	return reflection_probe->baked_exposure;
}

float LightStorage::reflection_probe_get_intensity(RID p_probe) const {
	const ReflectionProbe *reflection_probe = reflection_probe_owner.get_or_null(p_probe);
	ERR_FAIL_NULL_V(reflection_probe, 0);

	return reflection_probe->intensity;
}

float LightStorage::reflection_probe_get_blend_distance(RID p_probe) const {
	const ReflectionProbe *reflection_probe = reflection_probe_owner.get_or_null(p_probe);
	ERR_FAIL_NULL_V(reflection_probe, 0);

	return reflection_probe->blend_distance;
}

bool LightStorage::reflection_probe_is_interior(RID p_probe) const {
	const ReflectionProbe *reflection_probe = reflection_probe_owner.get_or_null(p_probe);
	ERR_FAIL_NULL_V(reflection_probe, false);

	return reflection_probe->interior;
}

bool LightStorage::reflection_probe_is_box_projection(RID p_probe) const {
	const ReflectionProbe *reflection_probe = reflection_probe_owner.get_or_null(p_probe);
	ERR_FAIL_NULL_V(reflection_probe, false);

	return reflection_probe->box_projection;
}

RS::ReflectionProbeAmbientMode LightStorage::reflection_probe_get_ambient_mode(RID p_probe) const {
	const ReflectionProbe *reflection_probe = reflection_probe_owner.get_or_null(p_probe);
	ERR_FAIL_NULL_V(reflection_probe, RS::REFLECTION_PROBE_AMBIENT_DISABLED);
	return reflection_probe->ambient_mode;
}

Color LightStorage::reflection_probe_get_ambient_color(RID p_probe) const {
	const ReflectionProbe *reflection_probe = reflection_probe_owner.get_or_null(p_probe);
	ERR_FAIL_NULL_V(reflection_probe, Color());

	return reflection_probe->ambient_color;
}
float LightStorage::reflection_probe_get_ambient_color_energy(RID p_probe) const {
	const ReflectionProbe *reflection_probe = reflection_probe_owner.get_or_null(p_probe);
	ERR_FAIL_NULL_V(reflection_probe, 0);

	return reflection_probe->ambient_color_energy;
}

Dependency *LightStorage::reflection_probe_get_dependency(RID p_probe) const {
	ReflectionProbe *reflection_probe = reflection_probe_owner.get_or_null(p_probe);
	ERR_FAIL_NULL_V(reflection_probe, nullptr);

	return &reflection_probe->dependency;
}

/* REFLECTION ATLAS */

RID LightStorage::reflection_atlas_create() {
	ReflectionAtlas ra;
	ra.count = GLOBAL_GET("rendering/reflections/reflection_atlas/reflection_count");
	ra.size = GLOBAL_GET("rendering/reflections/reflection_atlas/reflection_size");
	ra.cluster_builder = nullptr;

	return reflection_atlas_owner.make_rid(ra);
}

void LightStorage::reflection_atlas_free(RID p_ref_atlas) {
	reflection_atlas_set_size(p_ref_atlas, 0, 0);
	ReflectionAtlas *ra = reflection_atlas_owner.get_or_null(p_ref_atlas);
	if (ra->cluster_builder) {
		memdelete(ra->cluster_builder);
	}
	reflection_atlas_owner.free(p_ref_atlas);
}

void LightStorage::reflection_atlas_set_size(RID p_ref_atlas, int p_reflection_size, int p_reflection_count) {
	ReflectionAtlas *ra = reflection_atlas_owner.get_or_null(p_ref_atlas);
	ERR_FAIL_NULL(ra);

	if (ra->size == p_reflection_size && ra->count == p_reflection_count) {
		return; //no changes
	}

	if (ra->cluster_builder) {
		// only if we're using our cluster
		ra->cluster_builder->setup(Size2i(ra->size, ra->size), max_cluster_elements, RID(), RID(), RID());
	}

	ra->size = p_reflection_size;
	ra->count = p_reflection_count;

	if (ra->reflection.is_valid()) {
		//clear and invalidate everything
		RD::get_singleton()->free(ra->reflection);
		ra->reflection = RID();
		RD::get_singleton()->free(ra->depth_buffer);
		ra->depth_buffer = RID();
		for (int i = 0; i < ra->reflections.size(); i++) {
			ra->reflections.write[i].data.clear_reflection_data();
			if (ra->reflections[i].owner.is_null()) {
				continue;
			}
			reflection_probe_release_atlas_index(ra->reflections[i].owner);
			//rp->atlasindex clear
		}

		ra->reflections.clear();
	}

	if (ra->render_buffers.is_valid()) {
		ra->render_buffers->cleanup();
	}
}

int LightStorage::reflection_atlas_get_size(RID p_ref_atlas) const {
	ReflectionAtlas *ra = reflection_atlas_owner.get_or_null(p_ref_atlas);
	ERR_FAIL_NULL_V(ra, 0);

	return ra->size;
}

/* REFLECTION PROBE INSTANCE */

RID LightStorage::reflection_probe_instance_create(RID p_probe) {
	ReflectionProbeInstance rpi;
	rpi.probe = p_probe;
	rpi.forward_id = ForwardIDStorage::get_singleton()->allocate_forward_id(FORWARD_ID_TYPE_REFLECTION_PROBE);

	return reflection_probe_instance_owner.make_rid(rpi);
}

void LightStorage::reflection_probe_instance_free(RID p_instance) {
	ReflectionProbeInstance *rpi = reflection_probe_instance_owner.get_or_null(p_instance);
	ForwardIDStorage::get_singleton()->free_forward_id(FORWARD_ID_TYPE_REFLECTION_PROBE, rpi->forward_id);
	reflection_probe_release_atlas_index(p_instance);
	reflection_probe_instance_owner.free(p_instance);
}

void LightStorage::reflection_probe_instance_set_transform(RID p_instance, const Transform3D &p_transform) {
	ReflectionProbeInstance *rpi = reflection_probe_instance_owner.get_or_null(p_instance);
	ERR_FAIL_NULL(rpi);

	rpi->transform = p_transform;
	rpi->dirty = true;
}

bool LightStorage::reflection_probe_has_atlas_index(RID p_instance) {
	ReflectionProbeInstance *rpi = reflection_probe_instance_owner.get_or_null(p_instance);
	ERR_FAIL_NULL_V(rpi, false);

	if (rpi->atlas.is_null()) {
		return false;
	}

	return rpi->atlas_index >= 0;
}

void LightStorage::reflection_probe_release_atlas_index(RID p_instance) {
	ReflectionProbeInstance *rpi = reflection_probe_instance_owner.get_or_null(p_instance);
	ERR_FAIL_NULL(rpi);

	if (rpi->atlas.is_null()) {
		return; //nothing to release
	}
	ReflectionAtlas *atlas = reflection_atlas_owner.get_or_null(rpi->atlas);
	ERR_FAIL_NULL(atlas);
	ERR_FAIL_INDEX(rpi->atlas_index, atlas->reflections.size());
	atlas->reflections.write[rpi->atlas_index].owner = RID();

	// TODO investigate if this is enough? shouldn't we be freeing our textures and framebuffers?

	if (rpi->rendering) {
		// We were cancelled mid rendering, trigger refresh.
		rpi->rendering = false;
		rpi->dirty = true;
		rpi->processing_layer = 1;
		rpi->processing_side = 0;
	}

	rpi->atlas_index = -1;
	rpi->atlas = RID();
}

bool LightStorage::reflection_probe_instance_needs_redraw(RID p_instance) {
	ReflectionProbeInstance *rpi = reflection_probe_instance_owner.get_or_null(p_instance);
	ERR_FAIL_NULL_V(rpi, false);

	if (rpi->rendering) {
		return false;
	}

	if (rpi->dirty) {
		return true;
	}

	if (LightStorage::get_singleton()->reflection_probe_get_update_mode(rpi->probe) == RS::REFLECTION_PROBE_UPDATE_ALWAYS) {
		return true;
	}

	return rpi->atlas_index == -1;
}

bool LightStorage::reflection_probe_instance_has_reflection(RID p_instance) {
	ReflectionProbeInstance *rpi = reflection_probe_instance_owner.get_or_null(p_instance);
	ERR_FAIL_NULL_V(rpi, false);

	return rpi->atlas.is_valid();
}

bool LightStorage::reflection_probe_instance_begin_render(RID p_instance, RID p_reflection_atlas) {
	ReflectionAtlas *atlas = reflection_atlas_owner.get_or_null(p_reflection_atlas);

	ERR_FAIL_NULL_V(atlas, false);

	ReflectionProbeInstance *rpi = reflection_probe_instance_owner.get_or_null(p_instance);
	ERR_FAIL_NULL_V(rpi, false);

	if (atlas->render_buffers.is_null()) {
		atlas->render_buffers.instantiate();
	}

	RD::get_singleton()->draw_command_begin_label("Reflection probe render");

	if (LightStorage::get_singleton()->reflection_probe_get_update_mode(rpi->probe) == RS::REFLECTION_PROBE_UPDATE_ALWAYS && atlas->reflection.is_valid() && atlas->size != 256) {
		WARN_PRINT("ReflectionProbes set to UPDATE_ALWAYS must have an atlas size of 256. Please update the atlas size in the ProjectSettings.");
		reflection_atlas_set_size(p_reflection_atlas, 256, atlas->count);
	}

	if (LightStorage::get_singleton()->reflection_probe_get_update_mode(rpi->probe) == RS::REFLECTION_PROBE_UPDATE_ALWAYS && atlas->reflection.is_valid() && atlas->reflections[0].data.layers[0].mipmaps.size() != 8) {
		// Invalidate reflection atlas, need to regenerate
		RD::get_singleton()->free(atlas->reflection);
		atlas->reflection = RID();

		for (int i = 0; i < atlas->reflections.size(); i++) {
			if (atlas->reflections[i].owner.is_null()) {
				continue;
			}
			reflection_probe_release_atlas_index(atlas->reflections[i].owner);
		}

		atlas->reflections.clear();
	}

	if (atlas->reflection.is_null()) {
		int mipmaps = MIN(RendererSceneRenderRD::get_singleton()->get_sky()->roughness_layers, Image::get_image_required_mipmaps(atlas->size, atlas->size, Image::FORMAT_RGBAH) + 1);
		mipmaps = LightStorage::get_singleton()->reflection_probe_get_update_mode(rpi->probe) == RS::REFLECTION_PROBE_UPDATE_ALWAYS ? 8 : mipmaps; // always use 8 mipmaps with real time filtering
		{
			//reflection atlas was unused, create:
			RD::TextureFormat tf;
			tf.array_layers = 6 * atlas->count;
			tf.format = get_reflection_probe_color_format();
			tf.texture_type = RD::TEXTURE_TYPE_CUBE_ARRAY;
			tf.mipmaps = mipmaps;
			tf.width = atlas->size;
			tf.height = atlas->size;
			tf.usage_bits = get_reflection_probe_color_usage_bits();
			atlas->reflection = RD::get_singleton()->texture_create(tf, RD::TextureView());
		}
		{
			RD::TextureFormat tf;
			tf.format = get_reflection_probe_depth_format();
			tf.width = atlas->size;
			tf.height = atlas->size;
			tf.usage_bits = get_reflection_probe_depth_usage_bits();
			atlas->depth_buffer = RD::get_singleton()->texture_create(tf, RD::TextureView());
		}
		atlas->reflections.resize(atlas->count);
		for (int i = 0; i < atlas->count; i++) {
			atlas->reflections.write[i].data.update_reflection_data(atlas->size, mipmaps, false, atlas->reflection, i * 6, LightStorage::get_singleton()->reflection_probe_get_update_mode(rpi->probe) == RS::REFLECTION_PROBE_UPDATE_ALWAYS, RendererSceneRenderRD::get_singleton()->get_sky()->roughness_layers, RendererSceneRenderRD::get_singleton()->_render_buffers_get_color_format());
			for (int j = 0; j < 6; j++) {
				atlas->reflections.write[i].fbs[j] = RendererSceneRenderRD::get_singleton()->reflection_probe_create_framebuffer(atlas->reflections.write[i].data.layers[0].mipmaps[0].views[j], atlas->depth_buffer);
			}
		}

		Vector<RID> fb;
		fb.push_back(atlas->depth_buffer);
		atlas->depth_fb = RD::get_singleton()->framebuffer_create(fb);

		atlas->render_buffers->configure_for_reflections(Size2i(atlas->size, atlas->size));
	}

	if (rpi->atlas_index == -1) {
		for (int i = 0; i < atlas->reflections.size(); i++) {
			if (atlas->reflections[i].owner.is_null()) {
				rpi->atlas_index = i;
				break;
			}
		}
		//find the one used last
		if (rpi->atlas_index == -1) {
			//everything is in use, find the one least used via LRU
			uint64_t pass_min = 0;

			for (int i = 0; i < atlas->reflections.size(); i++) {
				ReflectionProbeInstance *rpi2 = reflection_probe_instance_owner.get_or_null(atlas->reflections[i].owner);
				if (rpi2->last_pass < pass_min) {
					pass_min = rpi2->last_pass;
					rpi->atlas_index = i;
				}
			}
		}
	}

	if (rpi->atlas_index != -1) { // should we fail if this is still -1 ?
		atlas->reflections.write[rpi->atlas_index].owner = p_instance;
	}

	rpi->atlas = p_reflection_atlas;
	rpi->rendering = true;
	rpi->dirty = false;
	rpi->processing_layer = 1;
	rpi->processing_side = 0;

	RD::get_singleton()->draw_command_end_label();

	return true;
}

Ref<RenderSceneBuffers> LightStorage::reflection_probe_atlas_get_render_buffers(RID p_reflection_atlas) {
	ReflectionAtlas *atlas = reflection_atlas_owner.get_or_null(p_reflection_atlas);
	ERR_FAIL_NULL_V(atlas, Ref<RenderSceneBuffersRD>());

	return atlas->render_buffers;
}

bool LightStorage::reflection_probe_instance_postprocess_step(RID p_instance) {
	ReflectionProbeInstance *rpi = reflection_probe_instance_owner.get_or_null(p_instance);
	ERR_FAIL_NULL_V(rpi, false);
	ERR_FAIL_COND_V(!rpi->rendering, false);

	ReflectionAtlas *atlas = reflection_atlas_owner.get_or_null(rpi->atlas);
	if (!atlas || rpi->atlas_index == -1) {
		// Does not belong to an atlas anymore, cancel (was removed from atlas or atlas changed while rendering).
		rpi->rendering = false;
		return false;
	}

	if (LightStorage::get_singleton()->reflection_probe_get_update_mode(rpi->probe) == RS::REFLECTION_PROBE_UPDATE_ALWAYS) {
		// Using real time reflections, all roughness is done in one step
		atlas->reflections.write[rpi->atlas_index].data.create_reflection_fast_filter(false);
		rpi->rendering = false;
		rpi->processing_side = 0;
		rpi->processing_layer = 1;
		return true;
	}

	if (rpi->processing_layer > 1) {
		atlas->reflections.write[rpi->atlas_index].data.create_reflection_importance_sample(false, 10, rpi->processing_layer, RendererSceneRenderRD::get_singleton()->get_sky()->sky_ggx_samples_quality);
		rpi->processing_layer++;
		if (rpi->processing_layer == atlas->reflections[rpi->atlas_index].data.layers[0].mipmaps.size()) {
			rpi->rendering = false;
			rpi->processing_side = 0;
			rpi->processing_layer = 1;
			return true;
		}
		return false;

	} else {
		atlas->reflections.write[rpi->atlas_index].data.create_reflection_importance_sample(false, rpi->processing_side, rpi->processing_layer, RendererSceneRenderRD::get_singleton()->get_sky()->sky_ggx_samples_quality);
	}

	rpi->processing_side++;
	if (rpi->processing_side == 6) {
		rpi->processing_side = 0;
		rpi->processing_layer++;
		if (rpi->processing_layer == atlas->reflections[rpi->atlas_index].data.layers[0].mipmaps.size()) {
			rpi->rendering = false;
			rpi->processing_layer = 1;
			return true;
		}
	}

	return false;
}

uint32_t LightStorage::reflection_probe_instance_get_resolution(RID p_instance) {
	ReflectionProbeInstance *rpi = reflection_probe_instance_owner.get_or_null(p_instance);
	ERR_FAIL_NULL_V(rpi, 0);

	ReflectionAtlas *atlas = reflection_atlas_owner.get_or_null(rpi->atlas);
	ERR_FAIL_NULL_V(atlas, 0);
	return atlas->size;
}

RID LightStorage::reflection_probe_instance_get_framebuffer(RID p_instance, int p_index) {
	ReflectionProbeInstance *rpi = reflection_probe_instance_owner.get_or_null(p_instance);
	ERR_FAIL_NULL_V(rpi, RID());
	ERR_FAIL_INDEX_V(p_index, 6, RID());

	ReflectionAtlas *atlas = reflection_atlas_owner.get_or_null(rpi->atlas);
	ERR_FAIL_NULL_V(atlas, RID());
	return atlas->reflections[rpi->atlas_index].fbs[p_index];
}

RID LightStorage::reflection_probe_instance_get_depth_framebuffer(RID p_instance, int p_index) {
	ReflectionProbeInstance *rpi = reflection_probe_instance_owner.get_or_null(p_instance);
	ERR_FAIL_NULL_V(rpi, RID());
	ERR_FAIL_INDEX_V(p_index, 6, RID());

	ReflectionAtlas *atlas = reflection_atlas_owner.get_or_null(rpi->atlas);
	ERR_FAIL_NULL_V(atlas, RID());
	return atlas->depth_fb;
}

ClusterBuilderRD *LightStorage::reflection_probe_instance_get_cluster_builder(RID p_instance, ClusterBuilderSharedDataRD *p_cluster_builder_shared) {
	ReflectionProbeInstance *rpi = reflection_probe_instance_owner.get_or_null(p_instance);
	ReflectionAtlas *ra = reflection_atlas_owner.get_or_null(rpi->atlas);
	if (!ra) {
		ERR_PRINT("reflection probe has no reflection atlas! Bug?");
		return nullptr;
	} else {
		if (ra->cluster_builder == nullptr) {
			ra->cluster_builder = memnew(ClusterBuilderRD);
			ra->cluster_builder->set_shared(p_cluster_builder_shared);
			ra->cluster_builder->setup(Size2i(ra->size, ra->size), get_max_cluster_elements(), RID(), RID(), RID());
		}
		return ra->cluster_builder;
	}
}

/* REFLECTION DATA */

void LightStorage::free_reflection_data() {
	if (reflection_buffer.is_valid()) {
		RD::get_singleton()->free(reflection_buffer);
		reflection_buffer = RID();
	}

	if (reflections != nullptr) {
		memdelete_arr(reflections);
		reflections = nullptr;
	}

	if (reflection_sort != nullptr) {
		memdelete_arr(reflection_sort);
		reflection_sort = nullptr;
	}
}

void LightStorage::set_max_reflection_probes(const uint32_t p_max_reflection_probes) {
	max_reflections = p_max_reflection_probes;
	reflections = memnew_arr(ReflectionData, max_reflections);
	reflection_sort = memnew_arr(ReflectionProbeInstanceSort, max_reflections);
	reflection_buffer = RD::get_singleton()->storage_buffer_create(sizeof(ReflectionData) * max_reflections);
}

void LightStorage::update_reflection_probe_buffer(RenderDataRD *p_render_data, const PagedArray<RID> &p_reflections, const Transform3D &p_camera_inverse_transform, RID p_environment) {
	ForwardIDStorage *forward_id_storage = ForwardIDStorage::get_singleton();

	reflection_count = 0;

	for (uint32_t i = 0; i < (uint32_t)p_reflections.size(); i++) {
		if (reflection_count == max_reflections) {
			break;
		}

		ReflectionProbeInstance *rpi = reflection_probe_instance_owner.get_or_null(p_reflections[i]);
		if (!rpi) {
			continue;
		}

		Transform3D transform = rpi->transform;

		reflection_sort[reflection_count].probe_instance = rpi;
		reflection_sort[reflection_count].depth = -p_camera_inverse_transform.xform(transform.origin).z;
		reflection_count++;
	}

	if (reflection_count > 0) {
		SortArray<ReflectionProbeInstanceSort> sort_array;
		sort_array.sort(reflection_sort, reflection_count);
	}

	bool using_forward_ids = forward_id_storage->uses_forward_ids();
	for (uint32_t i = 0; i < reflection_count; i++) {
		ReflectionProbeInstance *rpi = reflection_sort[i].probe_instance;

		rpi->last_pass = RSG::rasterizer->get_frame_number();

		if (using_forward_ids) {
			forward_id_storage->map_forward_id(FORWARD_ID_TYPE_REFLECTION_PROBE, rpi->forward_id, i, rpi->last_pass);
		}

		ReflectionProbe *probe = reflection_probe_owner.get_or_null(rpi->probe);

		ReflectionData &reflection_ubo = reflections[i];

		Vector3 extents = probe->size / 2;

		rpi->cull_mask = probe->reflection_mask;

		reflection_ubo.box_extents[0] = extents.x;
		reflection_ubo.box_extents[1] = extents.y;
		reflection_ubo.box_extents[2] = extents.z;
		reflection_ubo.index = rpi->atlas_index;

		Vector3 origin_offset = probe->origin_offset;

		reflection_ubo.box_offset[0] = origin_offset.x;
		reflection_ubo.box_offset[1] = origin_offset.y;
		reflection_ubo.box_offset[2] = origin_offset.z;
		reflection_ubo.mask = probe->reflection_mask;

		reflection_ubo.intensity = probe->intensity;
		reflection_ubo.blend_distance = probe->blend_distance;
		reflection_ubo.ambient_mode = probe->ambient_mode;

		reflection_ubo.exterior = !probe->interior;
		reflection_ubo.box_project = probe->box_projection;
		reflection_ubo.exposure_normalization = 1.0;

		if (p_render_data->camera_attributes.is_valid()) {
			float exposure = RSG::camera_attributes->camera_attributes_get_exposure_normalization_factor(p_render_data->camera_attributes);
			reflection_ubo.exposure_normalization = exposure / probe->baked_exposure;
		}

		Color ambient_linear = probe->ambient_color.srgb_to_linear();
		float interior_ambient_energy = probe->ambient_color_energy;
		reflection_ubo.ambient[0] = ambient_linear.r * interior_ambient_energy;
		reflection_ubo.ambient[1] = ambient_linear.g * interior_ambient_energy;
		reflection_ubo.ambient[2] = ambient_linear.b * interior_ambient_energy;

		Transform3D transform = rpi->transform;
		Transform3D proj = (p_camera_inverse_transform * transform).inverse();
		MaterialStorage::store_transform(proj, reflection_ubo.local_matrix);

		// hook for subclass to do further processing.
		RendererSceneRenderRD::get_singleton()->setup_added_reflection_probe(transform, extents);
	}

	if (reflection_count) {
		RD::get_singleton()->buffer_update(reflection_buffer, 0, reflection_count * sizeof(ReflectionData), reflections);
	}
}

RD::DataFormat LightStorage::get_reflection_probe_color_format() {
	return RendererSceneRenderRD::get_singleton()->_render_buffers_get_color_format();
}

uint32_t LightStorage::get_reflection_probe_color_usage_bits() {
	return RD::TEXTURE_USAGE_COLOR_ATTACHMENT_BIT | RD::TEXTURE_USAGE_SAMPLING_BIT | (RendererSceneRenderRD::get_singleton()->_render_buffers_can_be_storage() ? RD::TEXTURE_USAGE_STORAGE_BIT : 0);
}

RD::DataFormat LightStorage::get_reflection_probe_depth_format() {
	return RD::get_singleton()->texture_is_format_supported_for_usage(RD::DATA_FORMAT_D32_SFLOAT, RD::TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) ? RD::DATA_FORMAT_D32_SFLOAT : RD::DATA_FORMAT_X8_D24_UNORM_PACK32;
}

uint32_t LightStorage::get_reflection_probe_depth_usage_bits() {
	return RD::TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | RD::TEXTURE_USAGE_SAMPLING_BIT;
}

/* LIGHTMAP API */

RID LightStorage::lightmap_allocate() {
	return lightmap_owner.allocate_rid();
}

void LightStorage::lightmap_initialize(RID p_lightmap) {
	lightmap_owner.initialize_rid(p_lightmap, Lightmap());
}

void LightStorage::lightmap_free(RID p_rid) {
	lightmap_set_textures(p_rid, RID(), false);
	Lightmap *lightmap = lightmap_owner.get_or_null(p_rid);
	lightmap->dependency.deleted_notify(p_rid);
	lightmap_owner.free(p_rid);
}

void LightStorage::lightmap_set_textures(RID p_lightmap, RID p_light, bool p_uses_spherical_haromics) {
	TextureStorage *texture_storage = TextureStorage::get_singleton();

	Lightmap *lm = lightmap_owner.get_or_null(p_lightmap);
	ERR_FAIL_NULL(lm);

	lightmap_array_version++;

	//erase lightmap users
	if (lm->light_texture.is_valid()) {
		TextureStorage::Texture *t = texture_storage->get_texture(lm->light_texture);
		if (t) {
			t->lightmap_users.erase(p_lightmap);
		}
	}

	TextureStorage::Texture *t = texture_storage->get_texture(p_light);
	lm->light_texture = p_light;
	lm->uses_spherical_harmonics = p_uses_spherical_haromics;

	RID default_2d_array = texture_storage->texture_rd_get_default(TextureStorage::DEFAULT_RD_TEXTURE_2D_ARRAY_WHITE);
	if (!t) {
		if (using_lightmap_array) {
			if (lm->array_index >= 0) {
				lightmap_textures.write[lm->array_index] = default_2d_array;
				lm->array_index = -1;
			}
		}

		return;
	}

	t->lightmap_users.insert(p_lightmap);
	lm->light_texture_size = Vector2i(t->width, t->height);

	if (using_lightmap_array) {
		if (lm->array_index < 0) {
			//not in array, try to put in array
			for (int i = 0; i < lightmap_textures.size(); i++) {
				if (lightmap_textures[i] == default_2d_array) {
					lm->array_index = i;
					break;
				}
			}
		}
		ERR_FAIL_COND_MSG(lm->array_index < 0, "Maximum amount of lightmaps in use (" + itos(lightmap_textures.size()) + ") has been exceeded, lightmap will nod display properly.");

		lightmap_textures.write[lm->array_index] = t->rd_texture;
	}
}

void LightStorage::lightmap_set_probe_bounds(RID p_lightmap, const AABB &p_bounds) {
	Lightmap *lm = lightmap_owner.get_or_null(p_lightmap);
	ERR_FAIL_NULL(lm);
	lm->bounds = p_bounds;
}

void LightStorage::lightmap_set_probe_interior(RID p_lightmap, bool p_interior) {
	Lightmap *lm = lightmap_owner.get_or_null(p_lightmap);
	ERR_FAIL_NULL(lm);
	lm->interior = p_interior;
}

void LightStorage::lightmap_set_probe_capture_data(RID p_lightmap, const PackedVector3Array &p_points, const PackedColorArray &p_point_sh, const PackedInt32Array &p_tetrahedra, const PackedInt32Array &p_bsp_tree) {
	Lightmap *lm = lightmap_owner.get_or_null(p_lightmap);
	ERR_FAIL_NULL(lm);

	if (p_points.size()) {
		ERR_FAIL_COND(p_points.size() * 9 != p_point_sh.size());
		ERR_FAIL_COND((p_tetrahedra.size() % 4) != 0);
		ERR_FAIL_COND((p_bsp_tree.size() % 6) != 0);
	}

	lm->points = p_points;
	lm->bsp_tree = p_bsp_tree;
	lm->point_sh = p_point_sh;
	lm->tetrahedra = p_tetrahedra;
}

void LightStorage::lightmap_set_baked_exposure_normalization(RID p_lightmap, float p_exposure) {
	Lightmap *lm = lightmap_owner.get_or_null(p_lightmap);
	ERR_FAIL_NULL(lm);

	lm->baked_exposure = p_exposure;
}

PackedVector3Array LightStorage::lightmap_get_probe_capture_points(RID p_lightmap) const {
	Lightmap *lm = lightmap_owner.get_or_null(p_lightmap);
	ERR_FAIL_NULL_V(lm, PackedVector3Array());

	return lm->points;
}

PackedColorArray LightStorage::lightmap_get_probe_capture_sh(RID p_lightmap) const {
	Lightmap *lm = lightmap_owner.get_or_null(p_lightmap);
	ERR_FAIL_NULL_V(lm, PackedColorArray());
	return lm->point_sh;
}

PackedInt32Array LightStorage::lightmap_get_probe_capture_tetrahedra(RID p_lightmap) const {
	Lightmap *lm = lightmap_owner.get_or_null(p_lightmap);
	ERR_FAIL_NULL_V(lm, PackedInt32Array());
	return lm->tetrahedra;
}

PackedInt32Array LightStorage::lightmap_get_probe_capture_bsp_tree(RID p_lightmap) const {
	Lightmap *lm = lightmap_owner.get_or_null(p_lightmap);
	ERR_FAIL_NULL_V(lm, PackedInt32Array());
	return lm->bsp_tree;
}

void LightStorage::lightmap_set_probe_capture_update_speed(float p_speed) {
	lightmap_probe_capture_update_speed = p_speed;
}

Dependency *LightStorage::lightmap_get_dependency(RID p_lightmap) const {
	Lightmap *lm = lightmap_owner.get_or_null(p_lightmap);
	ERR_FAIL_NULL_V(lm, nullptr);

	return &lm->dependency;
}

void LightStorage::lightmap_tap_sh_light(RID p_lightmap, const Vector3 &p_point, Color *r_sh) {
	Lightmap *lm = lightmap_owner.get_or_null(p_lightmap);
	ERR_FAIL_NULL(lm);

	for (int i = 0; i < 9; i++) {
		r_sh[i] = Color(0, 0, 0, 0);
	}

	if (!lm->points.size() || !lm->bsp_tree.size() || !lm->tetrahedra.size()) {
		return;
	}

	static_assert(sizeof(Lightmap::BSP) == 24);

	const Lightmap::BSP *bsp = (const Lightmap::BSP *)lm->bsp_tree.ptr();
	int32_t node = 0;
	while (node >= 0) {
		if (Plane(bsp[node].plane[0], bsp[node].plane[1], bsp[node].plane[2], bsp[node].plane[3]).is_point_over(p_point)) {
#ifdef DEBUG_ENABLED
			ERR_FAIL_COND(bsp[node].over >= 0 && bsp[node].over < node);
#endif

			node = bsp[node].over;
		} else {
#ifdef DEBUG_ENABLED
			ERR_FAIL_COND(bsp[node].under >= 0 && bsp[node].under < node);
#endif
			node = bsp[node].under;
		}
	}

	if (node == Lightmap::BSP::EMPTY_LEAF) {
		return; //nothing could be done
	}

	node = ABS(node) - 1;

	uint32_t *tetrahedron = (uint32_t *)&lm->tetrahedra[node * 4];
	Vector3 points[4] = { lm->points[tetrahedron[0]], lm->points[tetrahedron[1]], lm->points[tetrahedron[2]], lm->points[tetrahedron[3]] };
	const Color *sh_colors[4]{ &lm->point_sh[tetrahedron[0] * 9], &lm->point_sh[tetrahedron[1] * 9], &lm->point_sh[tetrahedron[2] * 9], &lm->point_sh[tetrahedron[3] * 9] };
	Color barycentric = Geometry3D::tetrahedron_get_barycentric_coords(points[0], points[1], points[2], points[3], p_point);

	for (int i = 0; i < 4; i++) {
		float c = CLAMP(barycentric[i], 0.0, 1.0);
		for (int j = 0; j < 9; j++) {
			r_sh[j] += sh_colors[i][j] * c;
		}
	}
}

bool LightStorage::lightmap_is_interior(RID p_lightmap) const {
	const Lightmap *lm = lightmap_owner.get_or_null(p_lightmap);
	ERR_FAIL_NULL_V(lm, false);
	return lm->interior;
}

AABB LightStorage::lightmap_get_aabb(RID p_lightmap) const {
	const Lightmap *lm = lightmap_owner.get_or_null(p_lightmap);
	ERR_FAIL_NULL_V(lm, AABB());
	return lm->bounds;
}

void LightStorage::lightmap_set_shadowmask_textures(RID p_lightmap, RID p_shadow) {
	TextureStorage *texture_storage = TextureStorage::get_singleton();

	Lightmap *lm = lightmap_owner.get_or_null(p_lightmap);
	ERR_FAIL_NULL(lm);

	// Erase lightmap users from shadow texture.
	if (lm->shadow_texture.is_valid()) {
		TextureStorage::Texture *t = texture_storage->get_texture(lm->shadow_texture);
		if (t) {
			t->lightmap_users.erase(p_lightmap);
		}
	}

	TextureStorage::Texture *t = texture_storage->get_texture(p_shadow);
	lm->shadow_texture = p_shadow;

	RID default_2d_array = texture_storage->texture_rd_get_default(TextureStorage::DEFAULT_RD_TEXTURE_2D_ARRAY_WHITE);
	if (!t) {
		if (lm->array_index >= 0) {
			shadowmask_textures.write[lm->array_index] = default_2d_array;
			lm->array_index = -1;
		}

		return;
	}

	t->lightmap_users.insert(p_lightmap);

	if (lm->array_index < 0) {
		// Not in array, try to put in array.
		for (int i = 0; i < shadowmask_textures.size(); i++) {
			if (shadowmask_textures[i] == default_2d_array) {
				lm->array_index = i;
				break;
			}
		}
	}

	ERR_FAIL_COND_MSG(lm->array_index < 0, vformat("Maximum amount of shadowmasks in use (%d) has been exceeded, shadowmask will not display properly.", shadowmask_textures.size()));

	shadowmask_textures.write[lm->array_index] = t->rd_texture;
}

RS::ShadowmaskMode LightStorage::lightmap_get_shadowmask_mode(RID p_lightmap) {
	Lightmap *lm = lightmap_owner.get_or_null(p_lightmap);
	ERR_FAIL_NULL_V(lm, RS::SHADOWMASK_MODE_NONE);

	return lm->shadowmask_mode;
}

void LightStorage::lightmap_set_shadowmask_mode(RID p_lightmap, RS::ShadowmaskMode p_mode) {
	Lightmap *lm = lightmap_owner.get_or_null(p_lightmap);
	ERR_FAIL_NULL(lm);

	lm->shadowmask_mode = p_mode;
}

/* LIGHTMAP INSTANCE */

RID LightStorage::lightmap_instance_create(RID p_lightmap) {
	LightmapInstance li;
	li.lightmap = p_lightmap;
	return lightmap_instance_owner.make_rid(li);
}

void LightStorage::lightmap_instance_free(RID p_lightmap) {
	lightmap_instance_owner.free(p_lightmap);
}

void LightStorage::lightmap_instance_set_transform(RID p_lightmap, const Transform3D &p_transform) {
	LightmapInstance *li = lightmap_instance_owner.get_or_null(p_lightmap);
	ERR_FAIL_NULL(li);
	li->transform = p_transform;
}

/* SHADOW ATLAS API */

RID LightStorage::shadow_atlas_create() {
	return shadow_atlas_owner.make_rid(ShadowAtlas());
}

void LightStorage::shadow_atlas_free(RID p_atlas) {
	shadow_atlas_set_size(p_atlas, 0);
	shadow_atlas_owner.free(p_atlas);
}

void LightStorage::_update_shadow_atlas(ShadowAtlas *shadow_atlas) {
	if (shadow_atlas->size > 0 && shadow_atlas->depth.is_null()) {
		RD::TextureFormat tf;
		tf.format = get_shadow_atlas_depth_format(shadow_atlas->use_16_bits);
		tf.width = shadow_atlas->size;
		tf.height = shadow_atlas->size;
		tf.usage_bits = get_shadow_atlas_depth_usage_bits();

		shadow_atlas->depth = RD::get_singleton()->texture_create(tf, RD::TextureView());
		Vector<RID> fb_tex;
		fb_tex.push_back(shadow_atlas->depth);
		shadow_atlas->fb = RD::get_singleton()->framebuffer_create(fb_tex);
	}
}

void LightStorage::shadow_atlas_set_size(RID p_atlas, int p_size, bool p_16_bits) {
	ShadowAtlas *shadow_atlas = shadow_atlas_owner.get_or_null(p_atlas);
	ERR_FAIL_NULL(shadow_atlas);
	ERR_FAIL_COND(p_size < 0);
	p_size = next_power_of_2(p_size);

	if (p_size == shadow_atlas->size && p_16_bits == shadow_atlas->use_16_bits) {
		return;
	}

	// erasing atlas
	if (shadow_atlas->depth.is_valid()) {
		RD::get_singleton()->free(shadow_atlas->depth);
		shadow_atlas->depth = RID();
	}
	for (int i = 0; i < 4; i++) {
		//clear subdivisions
		shadow_atlas->quadrants[i].shadows.clear();
		shadow_atlas->quadrants[i].shadows.resize(int64_t(shadow_atlas->quadrants[i].subdivision * shadow_atlas->quadrants[i].subdivision));
	}

	//erase shadow atlas reference from lights
	for (const KeyValue<RID, uint32_t> &E : shadow_atlas->shadow_owners) {
		LightInstance *li = light_instance_owner.get_or_null(E.key);
		ERR_CONTINUE(!li);
		li->shadow_atlases.erase(p_atlas);
	}

	//clear owners
	shadow_atlas->shadow_owners.clear();

	shadow_atlas->size = p_size;
	shadow_atlas->use_16_bits = p_16_bits;
}

void LightStorage::shadow_atlas_set_quadrant_subdivision(RID p_atlas, int p_quadrant, int p_subdivision) {
	ShadowAtlas *shadow_atlas = shadow_atlas_owner.get_or_null(p_atlas);
	ERR_FAIL_NULL(shadow_atlas);
	ERR_FAIL_INDEX(p_quadrant, 4);
	ERR_FAIL_INDEX(p_subdivision, 16384);

	uint32_t subdiv = next_power_of_2(p_subdivision);
	if (subdiv & 0xaaaaaaaa) { //sqrt(subdiv) must be integer
		subdiv <<= 1;
	}

	subdiv = int(Math::sqrt((float)subdiv));

	//obtain the number that will be x*x

	if (shadow_atlas->quadrants[p_quadrant].subdivision == subdiv) {
		return;
	}

	//erase all data from quadrant
	for (int i = 0; i < shadow_atlas->quadrants[p_quadrant].shadows.size(); i++) {
		if (shadow_atlas->quadrants[p_quadrant].shadows[i].owner.is_valid()) {
			shadow_atlas->shadow_owners.erase(shadow_atlas->quadrants[p_quadrant].shadows[i].owner);
			LightInstance *li = light_instance_owner.get_or_null(shadow_atlas->quadrants[p_quadrant].shadows[i].owner);
			ERR_CONTINUE(!li);
			li->shadow_atlases.erase(p_atlas);
		}
	}

	shadow_atlas->quadrants[p_quadrant].shadows.clear();
	shadow_atlas->quadrants[p_quadrant].shadows.resize(subdiv * subdiv);
	shadow_atlas->quadrants[p_quadrant].subdivision = subdiv;

	//cache the smallest subdiv (for faster allocation in light update)

	shadow_atlas->smallest_subdiv = 1 << 30;

	for (int i = 0; i < 4; i++) {
		if (shadow_atlas->quadrants[i].subdivision) {
			shadow_atlas->smallest_subdiv = MIN(shadow_atlas->smallest_subdiv, shadow_atlas->quadrants[i].subdivision);
		}
	}

	if (shadow_atlas->smallest_subdiv == 1 << 30) {
		shadow_atlas->smallest_subdiv = 0;
	}

	//resort the size orders, simple bublesort for 4 elements..

	int swaps = 0;
	do {
		swaps = 0;

		for (int i = 0; i < 3; i++) {
			if (shadow_atlas->quadrants[shadow_atlas->size_order[i]].subdivision < shadow_atlas->quadrants[shadow_atlas->size_order[i + 1]].subdivision) {
				SWAP(shadow_atlas->size_order[i], shadow_atlas->size_order[i + 1]);
				swaps++;
			}
		}
	} while (swaps > 0);
}

bool LightStorage::_shadow_atlas_find_shadow(ShadowAtlas *shadow_atlas, int *p_in_quadrants, int p_quadrant_count, int p_current_subdiv, uint64_t p_tick, int &r_quadrant, int &r_shadow) {
	for (int i = p_quadrant_count - 1; i >= 0; i--) {
		int qidx = p_in_quadrants[i];

		if (shadow_atlas->quadrants[qidx].subdivision == (uint32_t)p_current_subdiv) {
			return false;
		}

		//look for an empty space
		int sc = shadow_atlas->quadrants[qidx].shadows.size();
		const ShadowAtlas::Quadrant::Shadow *sarr = shadow_atlas->quadrants[qidx].shadows.ptr();

		int found_free_idx = -1; //found a free one
		int found_used_idx = -1; //found existing one, must steal it
		uint64_t min_pass = 0; // pass of the existing one, try to use the least recently used one (LRU fashion)

		for (int j = 0; j < sc; j++) {
			if (!sarr[j].owner.is_valid()) {
				found_free_idx = j;
				break;
			}

			LightInstance *sli = light_instance_owner.get_or_null(sarr[j].owner);
			ERR_CONTINUE(!sli);

			if (sli->last_scene_pass != RendererSceneRenderRD::get_singleton()->get_scene_pass()) {
				//was just allocated, don't kill it so soon, wait a bit..
				if (p_tick - sarr[j].alloc_tick < shadow_atlas_realloc_tolerance_msec) {
					continue;
				}

				if (found_used_idx == -1 || sli->last_scene_pass < min_pass) {
					found_used_idx = j;
					min_pass = sli->last_scene_pass;
				}
			}
		}

		if (found_free_idx == -1 && found_used_idx == -1) {
			continue; //nothing found
		}

		if (found_free_idx == -1 && found_used_idx != -1) {
			found_free_idx = found_used_idx;
		}

		r_quadrant = qidx;
		r_shadow = found_free_idx;

		return true;
	}

	return false;
}

bool LightStorage::_shadow_atlas_find_omni_shadows(ShadowAtlas *shadow_atlas, int *p_in_quadrants, int p_quadrant_count, int p_current_subdiv, uint64_t p_tick, int &r_quadrant, int &r_shadow) {
	for (int i = p_quadrant_count - 1; i >= 0; i--) {
		int qidx = p_in_quadrants[i];

		if (shadow_atlas->quadrants[qidx].subdivision == (uint32_t)p_current_subdiv) {
			return false;
		}

		//look for an empty space
		int sc = shadow_atlas->quadrants[qidx].shadows.size();
		const ShadowAtlas::Quadrant::Shadow *sarr = shadow_atlas->quadrants[qidx].shadows.ptr();

		int found_idx = -1;
		uint64_t min_pass = 0; // sum of currently selected spots, try to get the least recently used pair

		for (int j = 0; j < sc - 1; j++) {
			uint64_t pass = 0;

			if (sarr[j].owner.is_valid()) {
				LightInstance *sli = light_instance_owner.get_or_null(sarr[j].owner);
				ERR_CONTINUE(!sli);

				if (sli->last_scene_pass == RendererSceneRenderRD::get_singleton()->get_scene_pass()) {
					continue;
				}

				//was just allocated, don't kill it so soon, wait a bit..
				if (p_tick - sarr[j].alloc_tick < shadow_atlas_realloc_tolerance_msec) {
					continue;
				}
				pass += sli->last_scene_pass;
			}

			if (sarr[j + 1].owner.is_valid()) {
				LightInstance *sli = light_instance_owner.get_or_null(sarr[j + 1].owner);
				ERR_CONTINUE(!sli);

				if (sli->last_scene_pass == RendererSceneRenderRD::get_singleton()->get_scene_pass()) {
					continue;
				}

				//was just allocated, don't kill it so soon, wait a bit..
				if (p_tick - sarr[j + 1].alloc_tick < shadow_atlas_realloc_tolerance_msec) {
					continue;
				}
				pass += sli->last_scene_pass;
			}

			if (found_idx == -1 || pass < min_pass) {
				found_idx = j;
				min_pass = pass;

				// we found two empty spots, no need to check the rest
				if (pass == 0) {
					break;
				}
			}
		}

		if (found_idx == -1) {
			continue; //nothing found
		}

		r_quadrant = qidx;
		r_shadow = found_idx;

		return true;
	}

	return false;
}

bool LightStorage::shadow_atlas_update_light(RID p_atlas, RID p_light_instance, float p_coverage, uint64_t p_light_version) {
	ShadowAtlas *shadow_atlas = shadow_atlas_owner.get_or_null(p_atlas);
	ERR_FAIL_NULL_V(shadow_atlas, false);

	LightInstance *li = light_instance_owner.get_or_null(p_light_instance);
	ERR_FAIL_NULL_V(li, false);

	if (shadow_atlas->size == 0 || shadow_atlas->smallest_subdiv == 0) {
		return false;
	}

	uint32_t quad_size = shadow_atlas->size >> 1;
	int desired_fit = MIN(quad_size / shadow_atlas->smallest_subdiv, next_power_of_2(quad_size * p_coverage));

	int valid_quadrants[4];
	int valid_quadrant_count = 0;
	int best_size = -1; //best size found
	int best_subdiv = -1; //subdiv for the best size

	//find the quadrants this fits into, and the best possible size it can fit into
	for (int i = 0; i < 4; i++) {
		int q = shadow_atlas->size_order[i];
		int sd = shadow_atlas->quadrants[q].subdivision;
		if (sd == 0) {
			continue; //unused
		}

		int max_fit = quad_size / sd;

		if (best_size != -1 && max_fit > best_size) {
			break; //too large
		}

		valid_quadrants[valid_quadrant_count++] = q;
		best_subdiv = sd;

		if (max_fit >= desired_fit) {
			best_size = max_fit;
		}
	}

	ERR_FAIL_COND_V(valid_quadrant_count == 0, false);

	uint64_t tick = OS::get_singleton()->get_ticks_msec();

	uint32_t old_key = SHADOW_INVALID;
	uint32_t old_quadrant = SHADOW_INVALID;
	uint32_t old_shadow = SHADOW_INVALID;
	int old_subdivision = -1;

	bool should_realloc = false;
	bool should_redraw = false;

	if (shadow_atlas->shadow_owners.has(p_light_instance)) {
		old_key = shadow_atlas->shadow_owners[p_light_instance];
		old_quadrant = (old_key >> QUADRANT_SHIFT) & 0x3;
		old_shadow = old_key & SHADOW_INDEX_MASK;

		should_realloc = shadow_atlas->quadrants[old_quadrant].subdivision != (uint32_t)best_subdiv && (tick - shadow_atlas->quadrants[old_quadrant].shadows[old_shadow].alloc_tick > shadow_atlas_realloc_tolerance_msec);
		should_redraw = shadow_atlas->quadrants[old_quadrant].shadows[old_shadow].version != p_light_version;

		if (!should_realloc) {
			shadow_atlas->quadrants[old_quadrant].shadows.write[old_shadow].version = p_light_version;
			//already existing, see if it should redraw or it's just OK
			return should_redraw;
		}

		old_subdivision = shadow_atlas->quadrants[old_quadrant].subdivision;
	}

	bool is_omni = li->light_type == RS::LIGHT_OMNI;
	bool found_shadow = false;
	int new_quadrant = -1;
	int new_shadow = -1;

	if (is_omni) {
		found_shadow = _shadow_atlas_find_omni_shadows(shadow_atlas, valid_quadrants, valid_quadrant_count, old_subdivision, tick, new_quadrant, new_shadow);
	} else {
		found_shadow = _shadow_atlas_find_shadow(shadow_atlas, valid_quadrants, valid_quadrant_count, old_subdivision, tick, new_quadrant, new_shadow);
	}

	if (found_shadow) {
		if (old_quadrant != SHADOW_INVALID) {
			shadow_atlas->quadrants[old_quadrant].shadows.write[old_shadow].version = 0;
			shadow_atlas->quadrants[old_quadrant].shadows.write[old_shadow].owner = RID();

			if (old_key & OMNI_LIGHT_FLAG) {
				shadow_atlas->quadrants[old_quadrant].shadows.write[old_shadow + 1].version = 0;
				shadow_atlas->quadrants[old_quadrant].shadows.write[old_shadow + 1].owner = RID();
			}
		}

		uint32_t new_key = new_quadrant << QUADRANT_SHIFT;
		new_key |= new_shadow;

		ShadowAtlas::Quadrant::Shadow *sh = &shadow_atlas->quadrants[new_quadrant].shadows.write[new_shadow];
		_shadow_atlas_invalidate_shadow(sh, p_atlas, shadow_atlas, new_quadrant, new_shadow);

		sh->owner = p_light_instance;
		sh->alloc_tick = tick;
		sh->version = p_light_version;

		if (is_omni) {
			new_key |= OMNI_LIGHT_FLAG;

			int new_omni_shadow = new_shadow + 1;
			ShadowAtlas::Quadrant::Shadow *extra_sh = &shadow_atlas->quadrants[new_quadrant].shadows.write[new_omni_shadow];
			_shadow_atlas_invalidate_shadow(extra_sh, p_atlas, shadow_atlas, new_quadrant, new_omni_shadow);

			extra_sh->owner = p_light_instance;
			extra_sh->alloc_tick = tick;
			extra_sh->version = p_light_version;
		}

		li->shadow_atlases.insert(p_atlas);

		//update it in map
		shadow_atlas->shadow_owners[p_light_instance] = new_key;
		//make it dirty, as it should redraw anyway
		return true;
	}

	return should_redraw;
}

void LightStorage::_shadow_atlas_invalidate_shadow(ShadowAtlas::Quadrant::Shadow *p_shadow, RID p_atlas, ShadowAtlas *p_shadow_atlas, uint32_t p_quadrant, uint32_t p_shadow_idx) {
	if (p_shadow->owner.is_valid()) {
		LightInstance *sli = light_instance_owner.get_or_null(p_shadow->owner);
		uint32_t old_key = p_shadow_atlas->shadow_owners[p_shadow->owner];

		if (old_key & OMNI_LIGHT_FLAG) {
			uint32_t s = old_key & SHADOW_INDEX_MASK;
			uint32_t omni_shadow_idx = p_shadow_idx + (s == (uint32_t)p_shadow_idx ? 1 : -1);
			ShadowAtlas::Quadrant::Shadow *omni_shadow = &p_shadow_atlas->quadrants[p_quadrant].shadows.write[omni_shadow_idx];
			omni_shadow->version = 0;
			omni_shadow->owner = RID();
		}

		p_shadow_atlas->shadow_owners.erase(p_shadow->owner);
		p_shadow->version = 0;
		p_shadow->owner = RID();
		sli->shadow_atlases.erase(p_atlas);
	}
}

void LightStorage::shadow_atlas_update(RID p_atlas) {
	ShadowAtlas *shadow_atlas = shadow_atlas_owner.get_or_null(p_atlas);
	ERR_FAIL_NULL(shadow_atlas);

	_update_shadow_atlas(shadow_atlas);
}

RD::DataFormat LightStorage::get_shadow_atlas_depth_format(bool p_16_bits) {
	return p_16_bits ? RD::DATA_FORMAT_D16_UNORM : RD::DATA_FORMAT_D32_SFLOAT;
}

uint32_t LightStorage::get_shadow_atlas_depth_usage_bits() {
	return RD::TEXTURE_USAGE_SAMPLING_BIT | RD::TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
}

/* DIRECTIONAL SHADOW */

void LightStorage::update_directional_shadow_atlas() {
	if (directional_shadow.depth.is_null() && directional_shadow.size > 0) {
		RD::TextureFormat tf;
		tf.format = get_shadow_atlas_depth_format(directional_shadow.use_16_bits);
		tf.width = directional_shadow.size;
		tf.height = directional_shadow.size;
		tf.usage_bits = get_shadow_atlas_depth_usage_bits();

		directional_shadow.depth = RD::get_singleton()->texture_create(tf, RD::TextureView());
		Vector<RID> fb_tex;
		fb_tex.push_back(directional_shadow.depth);
		directional_shadow.fb = RD::get_singleton()->framebuffer_create(fb_tex);
	}
}
void LightStorage::directional_shadow_atlas_set_size(int p_size, bool p_16_bits) {
	p_size = nearest_power_of_2_templated(p_size);

	if (directional_shadow.size == p_size && directional_shadow.use_16_bits == p_16_bits) {
		return;
	}

	directional_shadow.size = p_size;
	directional_shadow.use_16_bits = p_16_bits;

	if (directional_shadow.depth.is_valid()) {
		RD::get_singleton()->free(directional_shadow.depth);
		directional_shadow.depth = RID();
		RendererSceneRenderRD::get_singleton()->base_uniforms_changed();
	}
}

void LightStorage::set_directional_shadow_count(int p_count) {
	directional_shadow.light_count = p_count;
	directional_shadow.current_light = 0;
}

static Rect2i _get_directional_shadow_rect(int p_size, int p_shadow_count, int p_shadow_index) {
	int split_h = 1;
	int split_v = 1;

	while (split_h * split_v < p_shadow_count) {
		if (split_h == split_v) {
			split_h <<= 1;
		} else {
			split_v <<= 1;
		}
	}

	Rect2i rect(0, 0, p_size, p_size);
	rect.size.width /= split_h;
	rect.size.height /= split_v;

	rect.position.x = rect.size.width * (p_shadow_index % split_h);
	rect.position.y = rect.size.height * (p_shadow_index / split_h);

	return rect;
}

Rect2i LightStorage::get_directional_shadow_rect() {
	return _get_directional_shadow_rect(directional_shadow.size, directional_shadow.light_count, directional_shadow.current_light);
}

int LightStorage::get_directional_light_shadow_size(RID p_light_intance) {
	ERR_FAIL_COND_V(directional_shadow.light_count == 0, 0);

	Rect2i r = _get_directional_shadow_rect(directional_shadow.size, directional_shadow.light_count, 0);

	LightInstance *light_instance = light_instance_owner.get_or_null(p_light_intance);
	ERR_FAIL_NULL_V(light_instance, 0);

	switch (light_directional_get_shadow_mode(light_instance->light)) {
		case RS::LIGHT_DIRECTIONAL_SHADOW_ORTHOGONAL:
			break; //none
		case RS::LIGHT_DIRECTIONAL_SHADOW_PARALLEL_2_SPLITS:
			r.size.height /= 2;
			break;
		case RS::LIGHT_DIRECTIONAL_SHADOW_PARALLEL_4_SPLITS:
			r.size /= 2;
			break;
	}

	return MAX(r.size.width, r.size.height);
}

/* SHADOW CUBEMAPS */

LightStorage::ShadowCubemap *LightStorage::_get_shadow_cubemap(int p_size) {
	if (!shadow_cubemaps.has(p_size)) {
		ShadowCubemap sc;
		{
			RD::TextureFormat tf;
			tf.format = get_cubemap_depth_format();
			tf.width = p_size;
			tf.height = p_size;
			tf.texture_type = RD::TEXTURE_TYPE_CUBE;
			tf.array_layers = 6;
			tf.usage_bits = get_cubemap_depth_usage_bits();
			sc.cubemap = RD::get_singleton()->texture_create(tf, RD::TextureView());
		}

		for (int i = 0; i < 6; i++) {
			RID side_texture = RD::get_singleton()->texture_create_shared_from_slice(RD::TextureView(), sc.cubemap, i, 0);
			Vector<RID> fbtex;
			fbtex.push_back(side_texture);
			sc.side_fb[i] = RD::get_singleton()->framebuffer_create(fbtex);
		}

		shadow_cubemaps[p_size] = sc;
	}

	return &shadow_cubemaps[p_size];
}

RID LightStorage::get_cubemap(int p_size) {
	ShadowCubemap *cubemap = _get_shadow_cubemap(p_size);

	return cubemap->cubemap;
}

RID LightStorage::get_cubemap_fb(int p_size, int p_pass) {
	ShadowCubemap *cubemap = _get_shadow_cubemap(p_size);

	return cubemap->side_fb[p_pass];
}

RD::DataFormat LightStorage::get_cubemap_depth_format() {
	return RD::get_singleton()->texture_is_format_supported_for_usage(RD::DATA_FORMAT_D32_SFLOAT, RD::TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) ? RD::DATA_FORMAT_D32_SFLOAT : RD::DATA_FORMAT_X8_D24_UNORM_PACK32;
}

uint32_t LightStorage::get_cubemap_depth_usage_bits() {
	return RD::TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | RD::TEXTURE_USAGE_SAMPLING_BIT;
}

bool LightStorage::get_shadow_cubemaps_used() const {
	return shadow_cubemaps_used;
}

bool LightStorage::get_shadow_dual_paraboloid_used() const {
	return shadow_dual_paraboloid_used;
}

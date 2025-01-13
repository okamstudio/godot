/**************************************************************************/
/*  fog.cpp                                                               */
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

#include "fog.h"

#include "servers/rendering/renderer_rd/renderer_compositor_rd.h"
#include "servers/rendering/renderer_rd/storage_rd/material_storage.h"
#include "servers/rendering/renderer_rd/storage_rd/texture_storage.h"
#include "servers/rendering/rendering_server_default.h"

using namespace RendererRD;

Fog *Fog::singleton = nullptr;

Fog::Fog() {
	singleton = this;
}

Fog::~Fog() {
	singleton = nullptr;
}

/* FOG VOLUMES */

RID Fog::fog_volume_allocate() {
	return fog_volume_owner.allocate_rid();
}

void Fog::fog_volume_initialize(RID p_rid) {
	fog_volume_owner.initialize_rid(p_rid, FogVolume());
}

void Fog::fog_volume_free(RID p_rid) {
	FogVolume *fog_volume = fog_volume_owner.get_or_null(p_rid);
	fog_volume->dependency.deleted_notify(p_rid);
	fog_volume_owner.free(p_rid);
}

Dependency *Fog::fog_volume_get_dependency(RID p_fog_volume) const {
	FogVolume *fog_volume = fog_volume_owner.get_or_null(p_fog_volume);
	ERR_FAIL_NULL_V(fog_volume, nullptr);

	return &fog_volume->dependency;
}

void Fog::fog_volume_set_shape(RID p_fog_volume, RS::FogVolumeShape p_shape) {
	FogVolume *fog_volume = fog_volume_owner.get_or_null(p_fog_volume);
	ERR_FAIL_NULL(fog_volume);

	if (p_shape == fog_volume->shape) {
		return;
	}

	fog_volume->shape = p_shape;
	fog_volume->dependency.changed_notify(Dependency::DEPENDENCY_CHANGED_AABB);
}

void Fog::fog_volume_set_size(RID p_fog_volume, const Vector3 &p_size) {
	FogVolume *fog_volume = fog_volume_owner.get_or_null(p_fog_volume);
	ERR_FAIL_NULL(fog_volume);

	fog_volume->size = p_size;
	fog_volume->dependency.changed_notify(Dependency::DEPENDENCY_CHANGED_AABB);
}

void Fog::fog_volume_set_material(RID p_fog_volume, RID p_material) {
	FogVolume *fog_volume = fog_volume_owner.get_or_null(p_fog_volume);
	ERR_FAIL_NULL(fog_volume);
	fog_volume->material = p_material;
}

RID Fog::fog_volume_get_material(RID p_fog_volume) const {
	FogVolume *fog_volume = fog_volume_owner.get_or_null(p_fog_volume);
	ERR_FAIL_NULL_V(fog_volume, RID());

	return fog_volume->material;
}

RS::FogVolumeShape Fog::fog_volume_get_shape(RID p_fog_volume) const {
	FogVolume *fog_volume = fog_volume_owner.get_or_null(p_fog_volume);
	ERR_FAIL_NULL_V(fog_volume, RS::FOG_VOLUME_SHAPE_BOX);

	return fog_volume->shape;
}

AABB Fog::fog_volume_get_aabb(RID p_fog_volume) const {
	FogVolume *fog_volume = fog_volume_owner.get_or_null(p_fog_volume);
	ERR_FAIL_NULL_V(fog_volume, AABB());

	switch (fog_volume->shape) {
		case RS::FOG_VOLUME_SHAPE_ELLIPSOID:
		case RS::FOG_VOLUME_SHAPE_CONE:
		case RS::FOG_VOLUME_SHAPE_CYLINDER:
		case RS::FOG_VOLUME_SHAPE_BOX: {
			AABB aabb;
			aabb.position = -fog_volume->size / 2;
			aabb.size = fog_volume->size;
			return aabb;
		}
		default: {
			// Need some size otherwise will get culled
			return AABB(Vector3(-1, -1, -1), Vector3(2, 2, 2));
		}
	}
}

Vector3 Fog::fog_volume_get_size(RID p_fog_volume) const {
	const FogVolume *fog_volume = fog_volume_owner.get_or_null(p_fog_volume);
	ERR_FAIL_NULL_V(fog_volume, Vector3());
	return fog_volume->size;
}

////////////////////////////////////////////////////////////////////////////////
// Fog material

bool Fog::FogMaterialData::update_parameters(const HashMap<StringName, Variant> &p_parameters, bool p_uniform_dirty, bool p_textures_dirty) {
	uniform_set_updated = true;

	return update_parameters_uniform_set(p_parameters, p_uniform_dirty, p_textures_dirty, shader_data->uniforms, shader_data->ubo_offsets.ptr(), shader_data->texture_uniforms, shader_data->default_texture_params, shader_data->ubo_size, uniform_set, Fog::get_singleton()->volumetric_fog.shader.version_get_shader(shader_data->version, 0), VolumetricFogShader::FogSet::FOG_SET_MATERIAL, true, true);
}

Fog::FogMaterialData::~FogMaterialData() {
	free_parameters_uniform_set(uniform_set);
}

RendererRD::MaterialStorage::ShaderData *Fog::_create_fog_shader_func() {
	FogShaderData *shader_data = memnew(FogShaderData);
	return shader_data;
}

RendererRD::MaterialStorage::ShaderData *Fog::_create_fog_shader_funcs() {
	return Fog::get_singleton()->_create_fog_shader_func();
}

RendererRD::MaterialStorage::MaterialData *Fog::_create_fog_material_func(FogShaderData *p_shader) {
	FogMaterialData *material_data = memnew(FogMaterialData);
	material_data->shader_data = p_shader;
	//update will happen later anyway so do nothing.
	return material_data;
}

RendererRD::MaterialStorage::MaterialData *Fog::_create_fog_material_funcs(RendererRD::MaterialStorage::ShaderData *p_shader) {
	return Fog::get_singleton()->_create_fog_material_func(static_cast<FogShaderData *>(p_shader));
}

////////////////////////////////////////////////////////////////////////////////
// FOG VOLUMES INSTANCE

RID Fog::fog_volume_instance_create(RID p_fog_volume) {
	FogVolumeInstance fvi;
	fvi.volume = p_fog_volume;
	return fog_volume_instance_owner.make_rid(fvi);
}

void Fog::fog_instance_free(RID p_rid) {
	fog_volume_instance_owner.free(p_rid);
}

////////////////////////////////////////////////////////////////////////////////
// Volumetric Fog Shader

void Fog::init_fog_shader(uint32_t p_max_directional_lights, int p_roughness_layers, bool p_is_using_radiance_cubemap_array) {
	MaterialStorage *material_storage = MaterialStorage::get_singleton();

	{
		String defines = "#define SAMPLERS_BINDING_FIRST_INDEX " + itos(SAMPLERS_BINDING_FIRST_INDEX) + "\n";
		// Initialize local fog shader
		Vector<String> volumetric_fog_modes;
		volumetric_fog_modes.push_back("");
		volumetric_fog.shader.initialize(volumetric_fog_modes, defines);

		material_storage->shader_set_data_request_function(RendererRD::MaterialStorage::SHADER_TYPE_FOG, _create_fog_shader_funcs);
		material_storage->material_set_data_request_function(RendererRD::MaterialStorage::SHADER_TYPE_FOG, _create_fog_material_funcs);
		volumetric_fog.volume_ubo = RD::get_singleton()->uniform_buffer_create(sizeof(VolumetricFogShader::VolumeUBO));
	}

	{
		ShaderCompiler::DefaultIdentifierActions actions;

		actions.renames["TIME"] = "scene_params.time";
		actions.renames["PI"] = _MKSTR(Math_PI);
		actions.renames["TAU"] = _MKSTR(Math_TAU);
		actions.renames["E"] = _MKSTR(Math_E);
		actions.renames["WORLD_POSITION"] = "world.xyz";
		actions.renames["OBJECT_POSITION"] = "params.position";
		actions.renames["UVW"] = "uvw";
		actions.renames["SIZE"] = "params.size";
		actions.renames["ALBEDO"] = "albedo";
		actions.renames["DENSITY"] = "density";
		actions.renames["EMISSION"] = "emission";
		actions.renames["SDF"] = "sdf";

		actions.usage_defines["SDF"] = "#define SDF_USED\n";
		actions.usage_defines["DENSITY"] = "#define DENSITY_USED\n";
		actions.usage_defines["ALBEDO"] = "#define ALBEDO_USED\n";
		actions.usage_defines["EMISSION"] = "#define EMISSION_USED\n";

		actions.base_texture_binding_index = 1;
		actions.texture_layout_set = VolumetricFogShader::FogSet::FOG_SET_MATERIAL;
		actions.base_uniform_string = "material.";

		actions.default_filter = ShaderLanguage::FILTER_LINEAR_MIPMAP;
		actions.default_repeat = ShaderLanguage::REPEAT_DISABLE;
		actions.global_buffer_array_variable = "global_shader_uniforms.data";

		volumetric_fog.compiler.initialize(actions);
	}

	{
		// default material and shader for fog shader
		volumetric_fog.default_shader = material_storage->shader_allocate();
		material_storage->shader_initialize(volumetric_fog.default_shader);
		material_storage->shader_set_code(volumetric_fog.default_shader, R"(
// Default fog shader.

shader_type fog;

void fog() {
DENSITY = 1.0;
ALBEDO = vec3(1.0);
}
)");
		volumetric_fog.default_material = material_storage->material_allocate();
		material_storage->material_initialize(volumetric_fog.default_material);
		material_storage->material_set_shader(volumetric_fog.default_material, volumetric_fog.default_shader);

		FogMaterialData *md = static_cast<FogMaterialData *>(material_storage->material_get_data(volumetric_fog.default_material, RendererRD::MaterialStorage::SHADER_TYPE_FOG));
		volumetric_fog.default_shader_rd = volumetric_fog.shader.version_get_shader(md->shader_data->version, 0);

		Vector<RD::Uniform> uniforms;

		{
			RD::Uniform u;
			u.uniform_type = RD::UNIFORM_TYPE_STORAGE_BUFFER;
			u.binding = 2;
			u.append_id(RendererRD::MaterialStorage::get_singleton()->global_shader_uniforms_get_storage_buffer());
			uniforms.push_back(u);
		}

		material_storage->samplers_rd_get_default().append_uniforms(uniforms, SAMPLERS_BINDING_FIRST_INDEX);

		volumetric_fog.base_uniform_set = RD::get_singleton()->uniform_set_create(uniforms, volumetric_fog.default_shader_rd, VolumetricFogShader::FogSet::FOG_SET_BASE);
	}

	{
		String defines = "\n#define MAX_DIRECTIONAL_LIGHT_DATA_STRUCTS " + itos(p_max_directional_lights) + "\n";
		defines += "\n#define MAX_SKY_LOD " + itos(p_roughness_layers - 1) + ".0\n";
		if (p_is_using_radiance_cubemap_array) {
			defines += "\n#define USE_RADIANCE_CUBEMAP_ARRAY \n";
		}
		Vector<String> volumetric_fog_modes;
		volumetric_fog_modes.push_back("\n#define MODE_DENSITY\n");
		volumetric_fog_modes.push_back("\n#define MODE_DENSITY\n#define ENABLE_SDFGI\n");
		volumetric_fog_modes.push_back("\n#define MODE_FILTER\n");
		volumetric_fog_modes.push_back("\n#define MODE_FOG\n");
		volumetric_fog_modes.push_back("\n#define MODE_COPY\n");

		volumetric_fog.process_shader.initialize(volumetric_fog_modes, defines);
		volumetric_fog.process_shader_version = volumetric_fog.process_shader.version_create();
		for (int i = 0; i < VolumetricFogShader::VOLUMETRIC_FOG_PROCESS_SHADER_MAX; i++) {
			volumetric_fog.process_pipelines[i] = RD::get_singleton()->compute_pipeline_create(volumetric_fog.process_shader.version_get_shader(volumetric_fog.process_shader_version, i));
		}
		volumetric_fog.params_ubo = RD::get_singleton()->uniform_buffer_create(sizeof(VolumetricFogShader::ParamsUBO));
	}
}

void Fog::free_fog_shader() {
	MaterialStorage *material_storage = MaterialStorage::get_singleton();

	if (volumetric_fog.process_shader_version.is_valid()) {
		volumetric_fog.process_shader.version_free(volumetric_fog.process_shader_version);
	}
	if (volumetric_fog.volume_ubo.is_valid()) {
		RD::get_singleton()->free(volumetric_fog.volume_ubo);
	}
	if (volumetric_fog.params_ubo.is_valid()) {
		RD::get_singleton()->free(volumetric_fog.params_ubo);
	}
	if (volumetric_fog.default_shader.is_valid()) {
		material_storage->shader_free(volumetric_fog.default_shader);
	}
	if (volumetric_fog.default_material.is_valid()) {
		material_storage->material_free(volumetric_fog.default_material);
	}
}

void Fog::FogShaderData::set_code(const String &p_code) {
	//compile

	code = p_code;
	valid = false;
	ubo_size = 0;
	uniforms.clear();

	if (code.is_empty()) {
		return; //just invalid, but no error
	}

	ShaderCompiler::GeneratedCode gen_code;
	ShaderCompiler::IdentifierActions actions;
	actions.entry_point_stages["fog"] = ShaderCompiler::STAGE_COMPUTE;

	uses_time = false;

	actions.usage_flag_pointers["TIME"] = &uses_time;

	actions.uniforms = &uniforms;

	Fog *fog_singleton = Fog::get_singleton();

	Error err = fog_singleton->volumetric_fog.compiler.compile(RS::SHADER_FOG, code, &actions, path, gen_code);
	ERR_FAIL_COND_MSG(err != OK, "Fog shader compilation failed.");

	if (version.is_null()) {
		version = fog_singleton->volumetric_fog.shader.version_create();
	}

	fog_singleton->volumetric_fog.shader.version_set_compute_code(version, gen_code.code, gen_code.uniforms, gen_code.stage_globals[ShaderCompiler::STAGE_COMPUTE], gen_code.defines);
	ERR_FAIL_COND(!fog_singleton->volumetric_fog.shader.version_is_valid(version));

	ubo_size = gen_code.uniform_total_size;
	ubo_offsets = gen_code.uniform_offsets;
	texture_uniforms = gen_code.texture_uniforms;

	pipeline = RD::get_singleton()->compute_pipeline_create(fog_singleton->volumetric_fog.shader.version_get_shader(version, 0));

	valid = true;
}

bool Fog::FogShaderData::is_animated() const {
	return false;
}

bool Fog::FogShaderData::casts_shadows() const {
	return false;
}

RS::ShaderNativeSourceCode Fog::FogShaderData::get_native_source_code() const {
	Fog *fog_singleton = Fog::get_singleton();

	return fog_singleton->volumetric_fog.shader.version_get_native_source_code(version);
}

Fog::FogShaderData::~FogShaderData() {
	Fog *fog_singleton = Fog::get_singleton();
	ERR_FAIL_NULL(fog_singleton);
	//pipeline variants will clear themselves if shader is gone
	if (version.is_valid()) {
		fog_singleton->volumetric_fog.shader.version_free(version);
	}
}

////////////////////////////////////////////////////////////////////////////////
// Volumetric Fog

bool Fog::VolumetricFog::sync_gi_dependent_sets_validity(bool p_ensure_freed) {
	bool null = gi_dependent_sets.process_uniform_set_density.is_null();
	bool valid = !null && RD::get_singleton()->uniform_set_is_valid(gi_dependent_sets.process_uniform_set_density);

#ifdef DEV_ENABLED
	// It's all-or-nothing, or something else has changed that requires dev attention.
	DEV_ASSERT(null == gi_dependent_sets.process_uniform_set.is_null());
	DEV_ASSERT(null == gi_dependent_sets.process_uniform_set2.is_null());
	DEV_ASSERT(valid == RD::get_singleton()->uniform_set_is_valid(gi_dependent_sets.process_uniform_set));
	DEV_ASSERT(valid == RD::get_singleton()->uniform_set_is_valid(gi_dependent_sets.process_uniform_set2));
#endif

	if (valid) {
		if (p_ensure_freed) {
			RD::get_singleton()->free(gi_dependent_sets.process_uniform_set_density);
			RD::get_singleton()->free(gi_dependent_sets.process_uniform_set);
			RD::get_singleton()->free(gi_dependent_sets.process_uniform_set2);
			valid = false;
		}
	}

	if (!valid && !null) {
		gi_dependent_sets = {};
	}

	return valid;
}

void Fog::VolumetricFog::init(const Vector3i &fog_size, RID p_sky_shader) {
	width = fog_size.x;
	height = fog_size.y;
	depth = fog_size.z;

	RD::TextureFormat tf;
	tf.format = RD::DATA_FORMAT_R16G16B16A16_SFLOAT;
	tf.width = fog_size.x;
	tf.height = fog_size.y;
	tf.depth = fog_size.z;
	tf.texture_type = RD::TEXTURE_TYPE_3D;
	tf.usage_bits = RD::TEXTURE_USAGE_STORAGE_BIT | RD::TEXTURE_USAGE_CAN_COPY_FROM_BIT;

	light_density_map = RD::get_singleton()->texture_create(tf, RD::TextureView());
	RD::get_singleton()->set_resource_name(light_density_map, "Fog light-density map");

	tf.usage_bits = RD::TEXTURE_USAGE_STORAGE_BIT | RD::TEXTURE_USAGE_SAMPLING_BIT | RD::TEXTURE_USAGE_CAN_COPY_TO_BIT;

	prev_light_density_map = RD::get_singleton()->texture_create(tf, RD::TextureView());
	RD::get_singleton()->set_resource_name(prev_light_density_map, "Fog previous light-density map");
	RD::get_singleton()->texture_clear(prev_light_density_map, Color(0, 0, 0, 0), 0, 1, 0, 1);

	tf.usage_bits = RD::TEXTURE_USAGE_STORAGE_BIT | RD::TEXTURE_USAGE_SAMPLING_BIT;

	fog_map = RD::get_singleton()->texture_create(tf, RD::TextureView());
	RD::get_singleton()->set_resource_name(fog_map, "Fog map");

#if defined(MACOS_ENABLED) || defined(IOS_ENABLED)
	Vector<uint8_t> dm;
	dm.resize(fog_size.x * fog_size.y * fog_size.z * 4);
	dm.fill(0);

	density_map = RD::get_singleton()->storage_buffer_create(dm.size(), dm);
	RD::get_singleton()->set_resource_name(density_map, "Fog density map");
	light_map = RD::get_singleton()->storage_buffer_create(dm.size(), dm);
	RD::get_singleton()->set_resource_name(light_map, "Fog light map");
	emissive_map = RD::get_singleton()->storage_buffer_create(dm.size(), dm);
	RD::get_singleton()->set_resource_name(emissive_map, "Fog emissive map");
#else
	tf.format = RD::DATA_FORMAT_R32_UINT;
	tf.usage_bits = RD::TEXTURE_USAGE_STORAGE_BIT | RD::TEXTURE_USAGE_CAN_COPY_TO_BIT;
	density_map = RD::get_singleton()->texture_create(tf, RD::TextureView());
	RD::get_singleton()->set_resource_name(density_map, "Fog density map");
	RD::get_singleton()->texture_clear(density_map, Color(0, 0, 0, 0), 0, 1, 0, 1);
	light_map = RD::get_singleton()->texture_create(tf, RD::TextureView());
	RD::get_singleton()->set_resource_name(light_map, "Fog light map");
	RD::get_singleton()->texture_clear(light_map, Color(0, 0, 0, 0), 0, 1, 0, 1);
	emissive_map = RD::get_singleton()->texture_create(tf, RD::TextureView());
	RD::get_singleton()->set_resource_name(emissive_map, "Fog emissive map");
	RD::get_singleton()->texture_clear(emissive_map, Color(0, 0, 0, 0), 0, 1, 0, 1);
#endif

	Vector<RD::Uniform> uniforms;
	{
		RD::Uniform u;
		u.binding = 0;
		u.uniform_type = RD::UNIFORM_TYPE_TEXTURE;
		u.append_id(fog_map);
		uniforms.push_back(u);
	}

	sky_uniform_set = RD::get_singleton()->uniform_set_create(uniforms, p_sky_shader, RendererRD::SkyRD::SKY_SET_FOG);
}

Fog::VolumetricFog::~VolumetricFog() {
	RD::get_singleton()->free(prev_light_density_map);
	RD::get_singleton()->free(light_density_map);
	RD::get_singleton()->free(fog_map);
	RD::get_singleton()->free(density_map);
	RD::get_singleton()->free(light_map);
	RD::get_singleton()->free(emissive_map);

	if (fog_uniform_set.is_valid() && RD::get_singleton()->uniform_set_is_valid(fog_uniform_set)) {
		RD::get_singleton()->free(fog_uniform_set);
	}
	if (copy_uniform_set.is_valid() && RD::get_singleton()->uniform_set_is_valid(copy_uniform_set)) {
		RD::get_singleton()->free(copy_uniform_set);
	}

	sync_gi_dependent_sets_validity(true);

	if (sdfgi_uniform_set.is_valid() && RD::get_singleton()->uniform_set_is_valid(sdfgi_uniform_set)) {
		RD::get_singleton()->free(sdfgi_uniform_set);
	}
	if (sky_uniform_set.is_valid() && RD::get_singleton()->uniform_set_is_valid(sky_uniform_set)) {
		RD::get_singleton()->free(sky_uniform_set);
	}
}

Vector3i Fog::_point_get_position_in_froxel_volume(const Vector3 &p_point, float fog_end, const Vector2 &fog_near_size, const Vector2 &fog_far_size, float volumetric_fog_detail_spread, const Vector3 &fog_size, const Transform3D &p_cam_transform) {
	Vector3 view_position = p_cam_transform.affine_inverse().xform(p_point);
	view_position.z = MIN(view_position.z, -0.01); // Clamp to the front of camera
	Vector3 fog_position = Vector3(0, 0, 0);

	view_position.y = -view_position.y;
	fog_position.z = -view_position.z / fog_end;
	fog_position.x = (view_position.x / (2 * (fog_near_size.x * (1.0 - fog_position.z) + fog_far_size.x * fog_position.z))) + 0.5;
	fog_position.y = (view_position.y / (2 * (fog_near_size.y * (1.0 - fog_position.z) + fog_far_size.y * fog_position.z))) + 0.5;
	fog_position.z = Math::pow(float(fog_position.z), float(1.0 / volumetric_fog_detail_spread));
	fog_position = fog_position * fog_size - Vector3(0.5, 0.5, 0.5);

	fog_position = fog_position.clamp(Vector3(), fog_size);

	return Vector3i(fog_position);
}

void Fog::volumetric_fog_update(const VolumetricFogSettings &p_settings, const Projection &p_cam_projection, const Transform3D &p_cam_transform, const Transform3D &p_prev_cam_inv_transform, RID p_shadow_atlas, int p_directional_light_count, bool p_use_directional_shadows, int p_positional_light_count, int p_voxel_gi_count, const PagedArray<RID> &p_fog_volumes) {
	RendererRD::TextureStorage *texture_storage = RendererRD::TextureStorage::get_singleton();
	RendererRD::MaterialStorage *material_storage = RendererRD::MaterialStorage::get_singleton();

	RENDER_TIMESTAMP("> Volumetric Fog");
	RD::get_singleton()->draw_command_begin_label("Volumetric Fog");

	Ref<VolumetricFog> fog = p_settings.vfog;

	if (p_fog_volumes.size() > 0) {
		RD::get_singleton()->draw_command_begin_label("Render Volumetric Fog Volumes");

		RENDER_TIMESTAMP("Render FogVolumes");

		VolumetricFogShader::VolumeUBO params;

		Vector2 frustum_near_size = p_cam_projection.get_viewport_half_extents();
		Vector2 frustum_far_size = p_cam_projection.get_far_plane_half_extents();
		float z_near = p_cam_projection.get_z_near();
		float z_far = p_cam_projection.get_z_far();
		float fog_end = RendererSceneRenderRD::get_singleton()->environment_get_volumetric_fog_length(p_settings.env);

		Vector2 fog_far_size = frustum_near_size.lerp(frustum_far_size, (fog_end - z_near) / (z_far - z_near));
		Vector2 fog_near_size;
		if (p_cam_projection.is_orthogonal()) {
			fog_near_size = fog_far_size;
		} else {
			fog_near_size = frustum_near_size.maxf(0.001);
		}

		params.fog_frustum_size_begin[0] = fog_near_size.x;
		params.fog_frustum_size_begin[1] = fog_near_size.y;

		params.fog_frustum_size_end[0] = fog_far_size.x;
		params.fog_frustum_size_end[1] = fog_far_size.y;

		params.fog_frustum_end = fog_end;
		params.z_near = z_near;
		params.z_far = z_far;
		params.time = p_settings.time;

		params.fog_volume_size[0] = fog->width;
		params.fog_volume_size[1] = fog->height;
		params.fog_volume_size[2] = fog->depth;

		params.use_temporal_reprojection = RendererSceneRenderRD::get_singleton()->environment_get_volumetric_fog_temporal_reprojection(p_settings.env);
		params.temporal_frame = RSG::rasterizer->get_frame_number() % VolumetricFog::MAX_TEMPORAL_FRAMES;
		params.detail_spread = RendererSceneRenderRD::get_singleton()->environment_get_volumetric_fog_detail_spread(p_settings.env);
		params.temporal_blend = RendererSceneRenderRD::get_singleton()->environment_get_volumetric_fog_temporal_reprojection_amount(p_settings.env);

		Transform3D to_prev_cam_view = p_prev_cam_inv_transform * p_cam_transform;
		RendererRD::MaterialStorage::store_transform(to_prev_cam_view, params.to_prev_view);
		RendererRD::MaterialStorage::store_transform(p_cam_transform, params.transform);

		RD::get_singleton()->buffer_update(volumetric_fog.volume_ubo, 0, sizeof(VolumetricFogShader::VolumeUBO), &params);

		if (fog->fog_uniform_set.is_null() || !RD::get_singleton()->uniform_set_is_valid(fog->fog_uniform_set)) {
			Vector<RD::Uniform> uniforms;

			{
				RD::Uniform u;
#if defined(MACOS_ENABLED) || defined(IOS_ENABLED)
				u.uniform_type = RD::UNIFORM_TYPE_STORAGE_BUFFER;
#else
				u.uniform_type = RD::UNIFORM_TYPE_IMAGE;
#endif
				u.binding = 1;
				u.append_id(fog->emissive_map);
				uniforms.push_back(u);
			}

			{
				RD::Uniform u;
				u.uniform_type = RD::UNIFORM_TYPE_UNIFORM_BUFFER;
				u.binding = 2;
				u.append_id(volumetric_fog.volume_ubo);
				uniforms.push_back(u);
			}

			{
				RD::Uniform u;
#if defined(MACOS_ENABLED) || defined(IOS_ENABLED)
				u.uniform_type = RD::UNIFORM_TYPE_STORAGE_BUFFER;
#else
				u.uniform_type = RD::UNIFORM_TYPE_IMAGE;
#endif
				u.binding = 3;
				u.append_id(fog->density_map);
				uniforms.push_back(u);
			}

			{
				RD::Uniform u;
#if defined(MACOS_ENABLED) || defined(IOS_ENABLED)
				u.uniform_type = RD::UNIFORM_TYPE_STORAGE_BUFFER;
#else
				u.uniform_type = RD::UNIFORM_TYPE_IMAGE;
#endif
				u.binding = 4;
				u.append_id(fog->light_map);
				uniforms.push_back(u);
			}

			fog->fog_uniform_set = RD::get_singleton()->uniform_set_create(uniforms, volumetric_fog.default_shader_rd, VolumetricFogShader::FogSet::FOG_SET_UNIFORMS);
		}

		RD::ComputeListID compute_list = RD::get_singleton()->compute_list_begin();
		bool any_uses_time = false;
		Vector3 cam_position = p_cam_transform.get_origin();

		for (int i = 0; i < (int)p_fog_volumes.size(); i++) {
			FogVolumeInstance *fog_volume_instance = fog_volume_instance_owner.get_or_null(p_fog_volumes[i]);
			ERR_FAIL_NULL(fog_volume_instance);
			RID fog_volume = fog_volume_instance->volume;

			RID fog_material = RendererRD::Fog::get_singleton()->fog_volume_get_material(fog_volume);

			FogMaterialData *material = nullptr;

			if (fog_material.is_valid()) {
				material = static_cast<FogMaterialData *>(material_storage->material_get_data(fog_material, RendererRD::MaterialStorage::SHADER_TYPE_FOG));
				if (!material || !material->shader_data->valid) {
					material = nullptr;
				}
			}

			if (!material) {
				fog_material = volumetric_fog.default_material;
				material = static_cast<FogMaterialData *>(material_storage->material_get_data(fog_material, RendererRD::MaterialStorage::SHADER_TYPE_FOG));
			}

			ERR_FAIL_NULL(material);

			FogShaderData *shader_data = material->shader_data;

			ERR_FAIL_NULL(shader_data);

			any_uses_time |= shader_data->uses_time;

			Vector3i froxel_min;
			Vector3i froxel_max;
			Vector3i kernel_size;

			Vector3 fog_position = fog_volume_instance->transform.get_origin();
			RS::FogVolumeShape volume_type = RendererRD::Fog::get_singleton()->fog_volume_get_shape(fog_volume);
			Vector3 extents = RendererRD::Fog::get_singleton()->fog_volume_get_size(fog_volume) / 2;

			if (volume_type != RS::FOG_VOLUME_SHAPE_WORLD) {
				// Local fog volume.
				Vector3 fog_size = Vector3(fog->width, fog->height, fog->depth);
				float volumetric_fog_detail_spread = RendererSceneRenderRD::get_singleton()->environment_get_volumetric_fog_detail_spread(p_settings.env);
				Vector3 corners[8]{
					fog_volume_instance->transform.xform(Vector3(extents.x, extents.y, extents.z)),
					fog_volume_instance->transform.xform(Vector3(-extents.x, extents.y, extents.z)),
					fog_volume_instance->transform.xform(Vector3(extents.x, -extents.y, extents.z)),
					fog_volume_instance->transform.xform(Vector3(-extents.x, -extents.y, extents.z)),
					fog_volume_instance->transform.xform(Vector3(extents.x, extents.y, -extents.z)),
					fog_volume_instance->transform.xform(Vector3(-extents.x, extents.y, -extents.z)),
					fog_volume_instance->transform.xform(Vector3(extents.x, -extents.y, -extents.z)),
					fog_volume_instance->transform.xform(Vector3(-extents.x, -extents.y, -extents.z))
				};
				Vector3i froxels[8];
				Vector3 corner_min = corners[0];
				Vector3 corner_max = corners[0];
				for (int j = 0; j < 8; j++) {
					froxels[j] = _point_get_position_in_froxel_volume(corners[j], fog_end, fog_near_size, fog_far_size, volumetric_fog_detail_spread, fog_size, p_cam_transform);
					corner_min = corner_min.min(corners[j]);
					corner_max = corner_max.max(corners[j]);
				}

				froxel_min = Vector3i(int32_t(fog->width) - 1, int32_t(fog->height) - 1, int32_t(fog->depth) - 1);
				froxel_max = Vector3i(1, 1, 1);

				// Tracking just the corners of the fog volume can result in missing some fog:
				// when the camera's near plane is inside the fog, we must always consider the entire screen
				Vector3 near_plane_corner(frustum_near_size.x, frustum_near_size.y, z_near);
				float expand = near_plane_corner.length();
				if (cam_position.x > (corner_min.x - expand) && cam_position.x < (corner_max.x + expand) &&
						cam_position.y > (corner_min.y - expand) && cam_position.y < (corner_max.y + expand) &&
						cam_position.z > (corner_min.z - expand) && cam_position.z < (corner_max.z + expand)) {
					froxel_min.x = 0;
					froxel_min.y = 0;
					froxel_min.z = 0;
					froxel_max.x = int32_t(fog->width);
					froxel_max.y = int32_t(fog->height);
					for (int j = 0; j < 8; j++) {
						froxel_max.z = MAX(froxel_max.z, froxels[j].z);
					}
				} else {
					// Camera is guaranteed to be outside the fog volume
					for (int j = 0; j < 8; j++) {
						froxel_min = froxel_min.min(froxels[j]);
						froxel_max = froxel_max.max(froxels[j]);
					}
				}

				kernel_size = froxel_max - froxel_min;
			} else {
				// Volume type global runs on all cells
				extents = Vector3(fog->width, fog->height, fog->depth);
				froxel_min = Vector3i(0, 0, 0);
				kernel_size = Vector3i(int32_t(fog->width), int32_t(fog->height), int32_t(fog->depth));
			}

			if (kernel_size.x == 0 || kernel_size.y == 0 || kernel_size.z == 0) {
				continue;
			}

			VolumetricFogShader::FogPushConstant push_constant;
			push_constant.position[0] = fog_position.x;
			push_constant.position[1] = fog_position.y;
			push_constant.position[2] = fog_position.z;
			push_constant.size[0] = extents.x * 2;
			push_constant.size[1] = extents.y * 2;
			push_constant.size[2] = extents.z * 2;
			push_constant.corner[0] = froxel_min.x;
			push_constant.corner[1] = froxel_min.y;
			push_constant.corner[2] = froxel_min.z;
			push_constant.shape = uint32_t(RendererRD::Fog::get_singleton()->fog_volume_get_shape(fog_volume));
			RendererRD::MaterialStorage::store_transform(fog_volume_instance->transform.affine_inverse(), push_constant.transform);

			RD::get_singleton()->compute_list_bind_compute_pipeline(compute_list, shader_data->pipeline);

			RD::get_singleton()->compute_list_bind_uniform_set(compute_list, fog->fog_uniform_set, VolumetricFogShader::FogSet::FOG_SET_UNIFORMS);
			RD::get_singleton()->compute_list_set_push_constant(compute_list, &push_constant, sizeof(VolumetricFogShader::FogPushConstant));
			RD::get_singleton()->compute_list_bind_uniform_set(compute_list, volumetric_fog.base_uniform_set, VolumetricFogShader::FogSet::FOG_SET_BASE);
			if (material->uniform_set.is_valid() && RD::get_singleton()->uniform_set_is_valid(material->uniform_set)) { // Material may not have a uniform set.
				RD::get_singleton()->compute_list_bind_uniform_set(compute_list, material->uniform_set, VolumetricFogShader::FogSet::FOG_SET_MATERIAL);
				material->set_as_used();
			}

			RD::get_singleton()->compute_list_dispatch_threads(compute_list, kernel_size.x, kernel_size.y, kernel_size.z);
		}
		if (any_uses_time || RendererSceneRenderRD::get_singleton()->environment_get_volumetric_fog_temporal_reprojection(p_settings.env)) {
			RenderingServerDefault::redraw_request();
		}

		RD::get_singleton()->draw_command_end_label();

		RD::get_singleton()->compute_list_end();
	}

	bool gi_dependent_sets_valid = fog->sync_gi_dependent_sets_validity();
	if (!fog->copy_uniform_set.is_null() && !RD::get_singleton()->uniform_set_is_valid(fog->copy_uniform_set)) {
		fog->copy_uniform_set = RID();
	}
	if (!gi_dependent_sets_valid || fog->copy_uniform_set.is_null()) {
		//re create uniform set if needed
		Vector<RD::Uniform> uniforms;
		Vector<RD::Uniform> copy_uniforms;

		{
			RD::Uniform u;
			u.uniform_type = RD::UNIFORM_TYPE_TEXTURE;
			u.binding = 1;
			if (p_settings.shadow_atlas_depth.is_null()) {
				u.append_id(texture_storage->texture_rd_get_default(RendererRD::TextureStorage::DEFAULT_RD_TEXTURE_BLACK));
			} else {
				u.append_id(p_settings.shadow_atlas_depth);
			}

			uniforms.push_back(u);
			copy_uniforms.push_back(u);
		}

		{
			RD::Uniform u;
			u.uniform_type = RD::UNIFORM_TYPE_TEXTURE;
			u.binding = 2;
			if (p_settings.directional_shadow_depth.is_valid()) {
				u.append_id(p_settings.directional_shadow_depth);
			} else {
				u.append_id(texture_storage->texture_rd_get_default(RendererRD::TextureStorage::DEFAULT_RD_TEXTURE_BLACK));
			}
			uniforms.push_back(u);
			copy_uniforms.push_back(u);
		}

		{
			RD::Uniform u;
			u.uniform_type = RD::UNIFORM_TYPE_STORAGE_BUFFER;
			u.binding = 3;
			u.append_id(p_settings.omni_light_buffer);
			uniforms.push_back(u);
			copy_uniforms.push_back(u);
		}
		{
			RD::Uniform u;
			u.uniform_type = RD::UNIFORM_TYPE_STORAGE_BUFFER;
			u.binding = 4;
			u.append_id(p_settings.spot_light_buffer);
			uniforms.push_back(u);
			copy_uniforms.push_back(u);
		}

		{
			RD::Uniform u;
			u.uniform_type = RD::UNIFORM_TYPE_UNIFORM_BUFFER;
			u.binding = 5;
			u.append_id(p_settings.directional_light_buffer);
			uniforms.push_back(u);
			copy_uniforms.push_back(u);
		}

		{
			RD::Uniform u;
			u.uniform_type = RD::UNIFORM_TYPE_STORAGE_BUFFER;
			u.binding = 6;
			u.append_id(p_settings.cluster_builder->get_cluster_buffer());
			uniforms.push_back(u);
			copy_uniforms.push_back(u);
		}

		{
			RD::Uniform u;
			u.uniform_type = RD::UNIFORM_TYPE_SAMPLER;
			u.binding = 7;
			u.append_id(material_storage->sampler_rd_get_default(RS::CANVAS_ITEM_TEXTURE_FILTER_LINEAR, RS::CANVAS_ITEM_TEXTURE_REPEAT_DISABLED));
			uniforms.push_back(u);
			copy_uniforms.push_back(u);
		}

		{
			RD::Uniform u;
			u.uniform_type = RD::UNIFORM_TYPE_IMAGE;
			u.binding = 8;
			u.append_id(fog->light_density_map);
			uniforms.push_back(u);
			copy_uniforms.push_back(u);
		}

		{
			RD::Uniform u;
			u.uniform_type = RD::UNIFORM_TYPE_IMAGE;
			u.binding = 9;
			u.append_id(fog->fog_map);
			uniforms.push_back(u);
		}

		{
			RD::Uniform u;
			u.uniform_type = RD::UNIFORM_TYPE_IMAGE;
			u.binding = 9;
			u.append_id(fog->prev_light_density_map);
			copy_uniforms.push_back(u);
		}

		{
			RD::Uniform u;
			u.uniform_type = RD::UNIFORM_TYPE_SAMPLER;
			u.binding = 10;
			u.append_id(p_settings.shadow_sampler);
			uniforms.push_back(u);
			copy_uniforms.push_back(u);
		}

		{
			RD::Uniform u;
			u.uniform_type = RD::UNIFORM_TYPE_UNIFORM_BUFFER;
			u.binding = 11;
			u.append_id(p_settings.voxel_gi_buffer);
			uniforms.push_back(u);
			copy_uniforms.push_back(u);
		}

		{
			RD::Uniform u;
			u.uniform_type = RD::UNIFORM_TYPE_TEXTURE;
			u.binding = 12;
			for (int i = 0; i < RendererRD::GI::MAX_VOXEL_GI_INSTANCES; i++) {
				u.append_id(p_settings.rbgi->voxel_gi_textures[i]);
			}
			uniforms.push_back(u);
			copy_uniforms.push_back(u);
		}
		{
			RD::Uniform u;
			u.uniform_type = RD::UNIFORM_TYPE_SAMPLER;
			u.binding = 13;
			u.append_id(material_storage->sampler_rd_get_default(RS::CANVAS_ITEM_TEXTURE_FILTER_LINEAR_WITH_MIPMAPS, RS::CANVAS_ITEM_TEXTURE_REPEAT_DISABLED));
			uniforms.push_back(u);
			copy_uniforms.push_back(u);
		}
		{
			RD::Uniform u;
			u.uniform_type = RD::UNIFORM_TYPE_UNIFORM_BUFFER;
			u.binding = 14;
			u.append_id(volumetric_fog.params_ubo);
			uniforms.push_back(u);
			copy_uniforms.push_back(u);
		}
		{
			RD::Uniform u;
			u.uniform_type = RD::UNIFORM_TYPE_TEXTURE;
			u.binding = 15;
			u.append_id(fog->prev_light_density_map);
			uniforms.push_back(u);
		}
		{
			RD::Uniform u;
#if defined(MACOS_ENABLED) || defined(IOS_ENABLED)
			u.uniform_type = RD::UNIFORM_TYPE_STORAGE_BUFFER;
#else
			u.uniform_type = RD::UNIFORM_TYPE_IMAGE;
#endif
			u.binding = 16;
			u.append_id(fog->density_map);
			uniforms.push_back(u);
		}
		{
			RD::Uniform u;
#if defined(MACOS_ENABLED) || defined(IOS_ENABLED)
			u.uniform_type = RD::UNIFORM_TYPE_STORAGE_BUFFER;
#else
			u.uniform_type = RD::UNIFORM_TYPE_IMAGE;
#endif
			u.binding = 17;
			u.append_id(fog->light_map);
			uniforms.push_back(u);
		}

		{
			RD::Uniform u;
#if defined(MACOS_ENABLED) || defined(IOS_ENABLED)
			u.uniform_type = RD::UNIFORM_TYPE_STORAGE_BUFFER;
#else
			u.uniform_type = RD::UNIFORM_TYPE_IMAGE;
#endif
			u.binding = 18;
			u.append_id(fog->emissive_map);
			uniforms.push_back(u);
		}

		{
			RD::Uniform u;
			u.uniform_type = RD::UNIFORM_TYPE_TEXTURE;
			u.binding = 19;
			RID radiance_texture = texture_storage->texture_rd_get_default(p_settings.is_using_radiance_cubemap_array ? RendererRD::TextureStorage::DEFAULT_RD_TEXTURE_CUBEMAP_ARRAY_BLACK : RendererRD::TextureStorage::DEFAULT_RD_TEXTURE_CUBEMAP_BLACK);
			RID sky_texture = RendererSceneRenderRD::get_singleton()->environment_get_sky(p_settings.env).is_valid() ? p_settings.sky->sky_get_radiance_texture_rd(RendererSceneRenderRD::get_singleton()->environment_get_sky(p_settings.env)) : RID();
			u.append_id(sky_texture.is_valid() ? sky_texture : radiance_texture);
			uniforms.push_back(u);
		}

		if (fog->copy_uniform_set.is_valid() && RD::get_singleton()->uniform_set_is_valid(fog->copy_uniform_set)) {
			RD::get_singleton()->free(fog->copy_uniform_set);
		}
		fog->copy_uniform_set = RD::get_singleton()->uniform_set_create(copy_uniforms, volumetric_fog.process_shader.version_get_shader(volumetric_fog.process_shader_version, VolumetricFogShader::VOLUMETRIC_FOG_PROCESS_SHADER_COPY), 0);

		if (!gi_dependent_sets_valid) {
			fog->gi_dependent_sets.process_uniform_set = RD::get_singleton()->uniform_set_create(uniforms, volumetric_fog.process_shader.version_get_shader(volumetric_fog.process_shader_version, VolumetricFogShader::VOLUMETRIC_FOG_PROCESS_SHADER_FOG), 0);

			RID aux7 = uniforms.write[7].get_id(0);
			RID aux8 = uniforms.write[8].get_id(0);

			uniforms.write[7].set_id(0, aux8);
			uniforms.write[8].set_id(0, aux7);

			fog->gi_dependent_sets.process_uniform_set2 = RD::get_singleton()->uniform_set_create(uniforms, volumetric_fog.process_shader.version_get_shader(volumetric_fog.process_shader_version, VolumetricFogShader::VOLUMETRIC_FOG_PROCESS_SHADER_FOG), 0);

			uniforms.remove_at(8);
			uniforms.write[7].set_id(0, aux7);
			fog->gi_dependent_sets.process_uniform_set_density = RD::get_singleton()->uniform_set_create(uniforms, volumetric_fog.process_shader.version_get_shader(volumetric_fog.process_shader_version, VolumetricFogShader::VOLUMETRIC_FOG_PROCESS_SHADER_DENSITY), 0);
		}
	}

	bool using_sdfgi = RendererSceneRenderRD::get_singleton()->environment_get_volumetric_fog_gi_inject(p_settings.env) > 0.0001 && RendererSceneRenderRD::get_singleton()->environment_get_sdfgi_enabled(p_settings.env) && (p_settings.sdfgi.is_valid());

	if (using_sdfgi) {
		if (fog->sdfgi_uniform_set.is_null() || !RD::get_singleton()->uniform_set_is_valid(fog->sdfgi_uniform_set)) {
			Vector<RD::Uniform> uniforms;

			{
				RD::Uniform u;
				u.uniform_type = RD::UNIFORM_TYPE_UNIFORM_BUFFER;
				u.binding = 0;
				u.append_id(p_settings.gi->sdfgi_ubo);
				uniforms.push_back(u);
			}

			{
				RD::Uniform u;
				u.uniform_type = RD::UNIFORM_TYPE_TEXTURE;
				u.binding = 1;
				u.append_id(p_settings.sdfgi->ambient_texture);
				uniforms.push_back(u);
			}

			{
				RD::Uniform u;
				u.uniform_type = RD::UNIFORM_TYPE_TEXTURE;
				u.binding = 2;
				u.append_id(p_settings.sdfgi->occlusion_texture);
				uniforms.push_back(u);
			}

			fog->sdfgi_uniform_set = RD::get_singleton()->uniform_set_create(uniforms, volumetric_fog.process_shader.version_get_shader(volumetric_fog.process_shader_version, VolumetricFogShader::VOLUMETRIC_FOG_PROCESS_SHADER_DENSITY_WITH_SDFGI), 1);
		}
	}

	fog->length = RendererSceneRenderRD::get_singleton()->environment_get_volumetric_fog_length(p_settings.env);
	fog->spread = RendererSceneRenderRD::get_singleton()->environment_get_volumetric_fog_detail_spread(p_settings.env);

	VolumetricFogShader::ParamsUBO params;

	Vector2 frustum_near_size = p_cam_projection.get_viewport_half_extents();
	Vector2 frustum_far_size = p_cam_projection.get_far_plane_half_extents();
	float z_near = p_cam_projection.get_z_near();
	float z_far = p_cam_projection.get_z_far();
	float fog_end = RendererSceneRenderRD::get_singleton()->environment_get_volumetric_fog_length(p_settings.env);

	Vector2 fog_far_size = frustum_near_size.lerp(frustum_far_size, (fog_end - z_near) / (z_far - z_near));
	Vector2 fog_near_size;
	if (p_cam_projection.is_orthogonal()) {
		fog_near_size = fog_far_size;
	} else {
		fog_near_size = frustum_near_size.maxf(0.001);
	}

	params.fog_frustum_size_begin[0] = fog_near_size.x;
	params.fog_frustum_size_begin[1] = fog_near_size.y;

	params.fog_frustum_size_end[0] = fog_far_size.x;
	params.fog_frustum_size_end[1] = fog_far_size.y;

	params.ambient_inject = RendererSceneRenderRD::get_singleton()->environment_get_volumetric_fog_ambient_inject(p_settings.env) * RendererSceneRenderRD::get_singleton()->environment_get_ambient_light_energy(p_settings.env);
	params.z_far = z_far;

	params.fog_frustum_end = fog_end;

	Color ambient_color = RendererSceneRenderRD::get_singleton()->environment_get_ambient_light(p_settings.env).srgb_to_linear();
	params.ambient_color[0] = ambient_color.r;
	params.ambient_color[1] = ambient_color.g;
	params.ambient_color[2] = ambient_color.b;
	params.sky_contribution = RendererSceneRenderRD::get_singleton()->environment_get_ambient_sky_contribution(p_settings.env);

	params.fog_volume_size[0] = fog->width;
	params.fog_volume_size[1] = fog->height;
	params.fog_volume_size[2] = fog->depth;

	params.directional_light_count = p_directional_light_count;

	Color emission = RendererSceneRenderRD::get_singleton()->environment_get_volumetric_fog_emission(p_settings.env).srgb_to_linear();
	params.base_emission[0] = emission.r * RendererSceneRenderRD::get_singleton()->environment_get_volumetric_fog_emission_energy(p_settings.env);
	params.base_emission[1] = emission.g * RendererSceneRenderRD::get_singleton()->environment_get_volumetric_fog_emission_energy(p_settings.env);
	params.base_emission[2] = emission.b * RendererSceneRenderRD::get_singleton()->environment_get_volumetric_fog_emission_energy(p_settings.env);
	params.base_density = RendererSceneRenderRD::get_singleton()->environment_get_volumetric_fog_density(p_settings.env);

	Color base_scattering = RendererSceneRenderRD::get_singleton()->environment_get_volumetric_fog_scattering(p_settings.env).srgb_to_linear();
	params.base_scattering[0] = base_scattering.r;
	params.base_scattering[1] = base_scattering.g;
	params.base_scattering[2] = base_scattering.b;
	params.phase_g = RendererSceneRenderRD::get_singleton()->environment_get_volumetric_fog_anisotropy(p_settings.env);

	params.detail_spread = RendererSceneRenderRD::get_singleton()->environment_get_volumetric_fog_detail_spread(p_settings.env);
	params.gi_inject = RendererSceneRenderRD::get_singleton()->environment_get_volumetric_fog_gi_inject(p_settings.env);

	params.cam_rotation[0] = p_cam_transform.basis[0][0];
	params.cam_rotation[1] = p_cam_transform.basis[1][0];
	params.cam_rotation[2] = p_cam_transform.basis[2][0];
	params.cam_rotation[3] = 0;
	params.cam_rotation[4] = p_cam_transform.basis[0][1];
	params.cam_rotation[5] = p_cam_transform.basis[1][1];
	params.cam_rotation[6] = p_cam_transform.basis[2][1];
	params.cam_rotation[7] = 0;
	params.cam_rotation[8] = p_cam_transform.basis[0][2];
	params.cam_rotation[9] = p_cam_transform.basis[1][2];
	params.cam_rotation[10] = p_cam_transform.basis[2][2];
	params.cam_rotation[11] = 0;
	params.filter_axis = 0;
	params.max_voxel_gi_instances = RendererSceneRenderRD::get_singleton()->environment_get_volumetric_fog_gi_inject(p_settings.env) > 0.001 ? p_voxel_gi_count : 0;
	params.temporal_frame = RSG::rasterizer->get_frame_number() % VolumetricFog::MAX_TEMPORAL_FRAMES;

	Transform3D to_prev_cam_view = p_prev_cam_inv_transform * p_cam_transform;
	RendererRD::MaterialStorage::store_transform(to_prev_cam_view, params.to_prev_view);

	params.use_temporal_reprojection = RendererSceneRenderRD::get_singleton()->environment_get_volumetric_fog_temporal_reprojection(p_settings.env);
	params.temporal_blend = RendererSceneRenderRD::get_singleton()->environment_get_volumetric_fog_temporal_reprojection_amount(p_settings.env);

	{
		uint32_t cluster_size = p_settings.cluster_builder->get_cluster_size();
		params.cluster_shift = get_shift_from_power_of_2(cluster_size);

		uint32_t cluster_screen_width = Math::division_round_up((uint32_t)p_settings.rb_size.x, cluster_size);
		uint32_t cluster_screen_height = Math::division_round_up((uint32_t)p_settings.rb_size.y, cluster_size);
		params.max_cluster_element_count_div_32 = p_settings.max_cluster_elements / 32;
		params.cluster_type_size = cluster_screen_width * cluster_screen_height * (params.max_cluster_element_count_div_32 + 32);
		params.cluster_width = cluster_screen_width;

		params.screen_size[0] = p_settings.rb_size.x;
		params.screen_size[1] = p_settings.rb_size.y;
	}

	Basis sky_transform = RendererSceneRenderRD::get_singleton()->environment_get_sky_orientation(p_settings.env);
	sky_transform = sky_transform.inverse() * p_cam_transform.basis;
	RendererRD::MaterialStorage::store_transform_3x3(sky_transform, params.radiance_inverse_xform);

	RD::get_singleton()->draw_command_begin_label("Render Volumetric Fog");

	RENDER_TIMESTAMP("Render Fog");
	RD::get_singleton()->buffer_update(volumetric_fog.params_ubo, 0, sizeof(VolumetricFogShader::ParamsUBO), &params);

	RD::ComputeListID compute_list = RD::get_singleton()->compute_list_begin();

	RD::get_singleton()->compute_list_bind_compute_pipeline(compute_list, volumetric_fog.process_pipelines[using_sdfgi ? VolumetricFogShader::VOLUMETRIC_FOG_PROCESS_SHADER_DENSITY_WITH_SDFGI : VolumetricFogShader::VOLUMETRIC_FOG_PROCESS_SHADER_DENSITY]);

	RD::get_singleton()->compute_list_bind_uniform_set(compute_list, fog->gi_dependent_sets.process_uniform_set_density, 0);

	if (using_sdfgi) {
		RD::get_singleton()->compute_list_bind_uniform_set(compute_list, fog->sdfgi_uniform_set, 1);
	}
	RD::get_singleton()->compute_list_dispatch_threads(compute_list, fog->width, fog->height, fog->depth);
	RD::get_singleton()->compute_list_add_barrier(compute_list);

	// Copy fog to history buffer
	if (RendererSceneRenderRD::get_singleton()->environment_get_volumetric_fog_temporal_reprojection(p_settings.env)) {
		RD::get_singleton()->compute_list_bind_compute_pipeline(compute_list, volumetric_fog.process_pipelines[VolumetricFogShader::VOLUMETRIC_FOG_PROCESS_SHADER_COPY]);
		RD::get_singleton()->compute_list_bind_uniform_set(compute_list, fog->copy_uniform_set, 0);
		RD::get_singleton()->compute_list_dispatch_threads(compute_list, fog->width, fog->height, fog->depth);
		RD::get_singleton()->compute_list_add_barrier(compute_list);
	}
	RD::get_singleton()->draw_command_end_label();

	if (p_settings.volumetric_fog_filter_active) {
		RD::get_singleton()->draw_command_begin_label("Filter Fog");

		RENDER_TIMESTAMP("Filter Fog");

		RD::get_singleton()->compute_list_bind_compute_pipeline(compute_list, volumetric_fog.process_pipelines[VolumetricFogShader::VOLUMETRIC_FOG_PROCESS_SHADER_FILTER]);
		RD::get_singleton()->compute_list_bind_uniform_set(compute_list, fog->gi_dependent_sets.process_uniform_set, 0);
		RD::get_singleton()->compute_list_dispatch_threads(compute_list, fog->width, fog->height, fog->depth);

		RD::get_singleton()->compute_list_end();
		//need restart for buffer update

		params.filter_axis = 1;
		RD::get_singleton()->buffer_update(volumetric_fog.params_ubo, 0, sizeof(VolumetricFogShader::ParamsUBO), &params);

		compute_list = RD::get_singleton()->compute_list_begin();
		RD::get_singleton()->compute_list_bind_compute_pipeline(compute_list, volumetric_fog.process_pipelines[VolumetricFogShader::VOLUMETRIC_FOG_PROCESS_SHADER_FILTER]);
		RD::get_singleton()->compute_list_bind_uniform_set(compute_list, fog->gi_dependent_sets.process_uniform_set2, 0);
		RD::get_singleton()->compute_list_dispatch_threads(compute_list, fog->width, fog->height, fog->depth);

		RD::get_singleton()->compute_list_add_barrier(compute_list);
		RD::get_singleton()->draw_command_end_label();
	}

	RENDER_TIMESTAMP("Integrate Fog");
	RD::get_singleton()->draw_command_begin_label("Integrate Fog");

	RD::get_singleton()->compute_list_bind_compute_pipeline(compute_list, volumetric_fog.process_pipelines[VolumetricFogShader::VOLUMETRIC_FOG_PROCESS_SHADER_FOG]);
	RD::get_singleton()->compute_list_bind_uniform_set(compute_list, fog->gi_dependent_sets.process_uniform_set, 0);
	RD::get_singleton()->compute_list_dispatch_threads(compute_list, fog->width, fog->height, 1);

	RD::get_singleton()->compute_list_end();

	RENDER_TIMESTAMP("< Volumetric Fog");
	RD::get_singleton()->draw_command_end_label();
	RD::get_singleton()->draw_command_end_label();
}

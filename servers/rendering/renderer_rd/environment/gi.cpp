/**************************************************************************/
/*  gi.cpp                                                                */
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

#include "gi.h"

#include "core/config/project_settings.h"
#include "servers/rendering/renderer_rd/renderer_compositor_rd.h"
#include "servers/rendering/renderer_rd/renderer_scene_render_rd.h"
#include "servers/rendering/renderer_rd/storage_rd/material_storage.h"
#include "servers/rendering/renderer_rd/storage_rd/render_scene_buffers_rd.h"
#include "servers/rendering/renderer_rd/storage_rd/texture_storage.h"
#include "servers/rendering/rendering_server_default.h"

using namespace RendererRD;

const Vector3i GI::SDFGI::Cascade::DIRTY_ALL = Vector3i(0x7FFFFFFF, 0x7FFFFFFF, 0x7FFFFFFF);

GI *GI::singleton = nullptr;

////////////////////////////////////////////////////////////////////////////////
// VOXEL GI STORAGE

RID GI::voxel_gi_allocate() {
	return voxel_gi_owner.allocate_rid();
}

void GI::voxel_gi_free(RID p_voxel_gi) {
	voxel_gi_allocate_data(p_voxel_gi, Transform3D(), AABB(), Vector3i(), Vector<uint8_t>(), Vector<uint8_t>(), Vector<uint8_t>(), Vector<int>()); //deallocate
	VoxelGI *voxel_gi = voxel_gi_owner.get_or_null(p_voxel_gi);
	voxel_gi->dependency.deleted_notify(p_voxel_gi);
	voxel_gi_owner.free(p_voxel_gi);
}

void GI::voxel_gi_initialize(RID p_voxel_gi) {
	voxel_gi_owner.initialize_rid(p_voxel_gi, VoxelGI());
}

void GI::voxel_gi_allocate_data(RID p_voxel_gi, const Transform3D &p_to_cell_xform, const AABB &p_aabb, const Vector3i &p_octree_size, const Vector<uint8_t> &p_octree_cells, const Vector<uint8_t> &p_data_cells, const Vector<uint8_t> &p_distance_field, const Vector<int> &p_level_counts) {
	VoxelGI *voxel_gi = voxel_gi_owner.get_or_null(p_voxel_gi);
	ERR_FAIL_NULL(voxel_gi);

	if (voxel_gi->octree_buffer.is_valid()) {
		RD::get_singleton()->free(voxel_gi->octree_buffer);
		RD::get_singleton()->free(voxel_gi->data_buffer);
		if (voxel_gi->sdf_texture.is_valid()) {
			RD::get_singleton()->free(voxel_gi->sdf_texture);
		}

		voxel_gi->sdf_texture = RID();
		voxel_gi->octree_buffer = RID();
		voxel_gi->data_buffer = RID();
		voxel_gi->octree_buffer_size = 0;
		voxel_gi->data_buffer_size = 0;
		voxel_gi->cell_count = 0;
	}

	voxel_gi->to_cell_xform = p_to_cell_xform;
	voxel_gi->bounds = p_aabb;
	voxel_gi->octree_size = p_octree_size;
	voxel_gi->level_counts = p_level_counts;

	if (p_octree_cells.size()) {
		ERR_FAIL_COND(p_octree_cells.size() % 32 != 0); //cells size must be a multiple of 32

		uint32_t cell_count = p_octree_cells.size() / 32;

		ERR_FAIL_COND(p_data_cells.size() != (int)cell_count * 16); //see that data size matches

		voxel_gi->cell_count = cell_count;
		voxel_gi->octree_buffer = RD::get_singleton()->storage_buffer_create(p_octree_cells.size(), p_octree_cells);
		voxel_gi->octree_buffer_size = p_octree_cells.size();
		voxel_gi->data_buffer = RD::get_singleton()->storage_buffer_create(p_data_cells.size(), p_data_cells);
		voxel_gi->data_buffer_size = p_data_cells.size();

		if (p_distance_field.size()) {
			RD::TextureFormat tf;
			tf.format = RD::DATA_FORMAT_R8_UNORM;
			tf.width = voxel_gi->octree_size.x;
			tf.height = voxel_gi->octree_size.y;
			tf.depth = voxel_gi->octree_size.z;
			tf.texture_type = RD::TEXTURE_TYPE_3D;
			tf.usage_bits = RD::TEXTURE_USAGE_SAMPLING_BIT | RD::TEXTURE_USAGE_CAN_UPDATE_BIT | RD::TEXTURE_USAGE_CAN_COPY_FROM_BIT;
			Vector<Vector<uint8_t>> s;
			s.push_back(p_distance_field);
			voxel_gi->sdf_texture = RD::get_singleton()->texture_create(tf, RD::TextureView(), s);
			RD::get_singleton()->set_resource_name(voxel_gi->sdf_texture, "VoxelGI SDF Texture");
		}
#if 0
			{
				RD::TextureFormat tf;
				tf.format = RD::DATA_FORMAT_R8_UNORM;
				tf.width = voxel_gi->octree_size.x;
				tf.height = voxel_gi->octree_size.y;
				tf.depth = voxel_gi->octree_size.z;
				tf.type = RD::TEXTURE_TYPE_3D;
				tf.usage_bits = RD::TEXTURE_USAGE_SAMPLING_BIT | RD::TEXTURE_USAGE_STORAGE_BIT | RD::TEXTURE_USAGE_CAN_COPY_TO_BIT;
				tf.shareable_formats.push_back(RD::DATA_FORMAT_R8_UNORM);
				tf.shareable_formats.push_back(RD::DATA_FORMAT_R8_UINT);
				voxel_gi->sdf_texture = RD::get_singleton()->texture_create(tf, RD::TextureView());
				RD::get_singleton()->set_resource_name(voxel_gi->sdf_texture, "VoxelGI SDF Texture");
			}
			RID shared_tex;
			{
				RD::TextureView tv;
				tv.format_override = RD::DATA_FORMAT_R8_UINT;
				shared_tex = RD::get_singleton()->texture_create_shared(tv, voxel_gi->sdf_texture);
			}
			//update SDF texture
			Vector<RD::Uniform> uniforms;
			{
				RD::Uniform u;
				u.uniform_type = RD::UNIFORM_TYPE_STORAGE_BUFFER;
				u.binding = 1;
				u.append_id(voxel_gi->octree_buffer);
				uniforms.push_back(u);
			}
			{
				RD::Uniform u;
				u.uniform_type = RD::UNIFORM_TYPE_STORAGE_BUFFER;
				u.binding = 2;
				u.append_id(voxel_gi->data_buffer);
				uniforms.push_back(u);
			}
			{
				RD::Uniform u;
				u.uniform_type = RD::UNIFORM_TYPE_IMAGE;
				u.binding = 3;
				u.append_id(shared_tex);
				uniforms.push_back(u);
			}

			RID uniform_set = RD::get_singleton()->uniform_set_create(uniforms, voxel_gi_sdf_shader_version_shader, 0);

			{
				uint32_t push_constant[4] = { 0, 0, 0, 0 };

				for (int i = 0; i < voxel_gi->level_counts.size() - 1; i++) {
					push_constant[0] += voxel_gi->level_counts[i];
				}
				push_constant[1] = push_constant[0] + voxel_gi->level_counts[voxel_gi->level_counts.size() - 1];

				print_line("offset: " + itos(push_constant[0]));
				print_line("size: " + itos(push_constant[1]));
				//create SDF
				RD::ComputeListID compute_list = RD::get_singleton()->compute_list_begin();
				RD::get_singleton()->compute_list_bind_compute_pipeline(compute_list, voxel_gi_sdf_shader_pipeline);
				RD::get_singleton()->compute_list_bind_uniform_set(compute_list, uniform_set, 0);
				RD::get_singleton()->compute_list_set_push_constant(compute_list, push_constant, sizeof(uint32_t) * 4);
				RD::get_singleton()->compute_list_dispatch(compute_list, voxel_gi->octree_size.x / 4, voxel_gi->octree_size.y / 4, voxel_gi->octree_size.z / 4);
				RD::get_singleton()->compute_list_end();
			}

			RD::get_singleton()->free(uniform_set);
			RD::get_singleton()->free(shared_tex);
		}
#endif
	}

	voxel_gi->version++;
	voxel_gi->data_version++;

	voxel_gi->dependency.changed_notify(Dependency::DEPENDENCY_CHANGED_AABB);
}

AABB GI::voxel_gi_get_bounds(RID p_voxel_gi) const {
	VoxelGI *voxel_gi = voxel_gi_owner.get_or_null(p_voxel_gi);
	ERR_FAIL_NULL_V(voxel_gi, AABB());

	return voxel_gi->bounds;
}

Vector3i GI::voxel_gi_get_octree_size(RID p_voxel_gi) const {
	VoxelGI *voxel_gi = voxel_gi_owner.get_or_null(p_voxel_gi);
	ERR_FAIL_NULL_V(voxel_gi, Vector3i());
	return voxel_gi->octree_size;
}

Vector<uint8_t> GI::voxel_gi_get_octree_cells(RID p_voxel_gi) const {
	VoxelGI *voxel_gi = voxel_gi_owner.get_or_null(p_voxel_gi);
	ERR_FAIL_NULL_V(voxel_gi, Vector<uint8_t>());

	if (voxel_gi->octree_buffer.is_valid()) {
		return RD::get_singleton()->buffer_get_data(voxel_gi->octree_buffer);
	}
	return Vector<uint8_t>();
}

Vector<uint8_t> GI::voxel_gi_get_data_cells(RID p_voxel_gi) const {
	VoxelGI *voxel_gi = voxel_gi_owner.get_or_null(p_voxel_gi);
	ERR_FAIL_NULL_V(voxel_gi, Vector<uint8_t>());

	if (voxel_gi->data_buffer.is_valid()) {
		return RD::get_singleton()->buffer_get_data(voxel_gi->data_buffer);
	}
	return Vector<uint8_t>();
}

Vector<uint8_t> GI::voxel_gi_get_distance_field(RID p_voxel_gi) const {
	VoxelGI *voxel_gi = voxel_gi_owner.get_or_null(p_voxel_gi);
	ERR_FAIL_NULL_V(voxel_gi, Vector<uint8_t>());

	if (voxel_gi->data_buffer.is_valid()) {
		return RD::get_singleton()->texture_get_data(voxel_gi->sdf_texture, 0);
	}
	return Vector<uint8_t>();
}

Vector<int> GI::voxel_gi_get_level_counts(RID p_voxel_gi) const {
	VoxelGI *voxel_gi = voxel_gi_owner.get_or_null(p_voxel_gi);
	ERR_FAIL_NULL_V(voxel_gi, Vector<int>());

	return voxel_gi->level_counts;
}

Transform3D GI::voxel_gi_get_to_cell_xform(RID p_voxel_gi) const {
	VoxelGI *voxel_gi = voxel_gi_owner.get_or_null(p_voxel_gi);
	ERR_FAIL_NULL_V(voxel_gi, Transform3D());

	return voxel_gi->to_cell_xform;
}

void GI::voxel_gi_set_dynamic_range(RID p_voxel_gi, float p_range) {
	VoxelGI *voxel_gi = voxel_gi_owner.get_or_null(p_voxel_gi);
	ERR_FAIL_NULL(voxel_gi);

	voxel_gi->dynamic_range = p_range;
	voxel_gi->version++;
}

float GI::voxel_gi_get_dynamic_range(RID p_voxel_gi) const {
	VoxelGI *voxel_gi = voxel_gi_owner.get_or_null(p_voxel_gi);
	ERR_FAIL_NULL_V(voxel_gi, 0);

	return voxel_gi->dynamic_range;
}

void GI::voxel_gi_set_propagation(RID p_voxel_gi, float p_range) {
	VoxelGI *voxel_gi = voxel_gi_owner.get_or_null(p_voxel_gi);
	ERR_FAIL_NULL(voxel_gi);

	voxel_gi->propagation = p_range;
	voxel_gi->version++;
}

float GI::voxel_gi_get_propagation(RID p_voxel_gi) const {
	VoxelGI *voxel_gi = voxel_gi_owner.get_or_null(p_voxel_gi);
	ERR_FAIL_NULL_V(voxel_gi, 0);
	return voxel_gi->propagation;
}

void GI::voxel_gi_set_energy(RID p_voxel_gi, float p_energy) {
	VoxelGI *voxel_gi = voxel_gi_owner.get_or_null(p_voxel_gi);
	ERR_FAIL_NULL(voxel_gi);

	voxel_gi->energy = p_energy;
}

float GI::voxel_gi_get_energy(RID p_voxel_gi) const {
	VoxelGI *voxel_gi = voxel_gi_owner.get_or_null(p_voxel_gi);
	ERR_FAIL_NULL_V(voxel_gi, 0);
	return voxel_gi->energy;
}

void GI::voxel_gi_set_baked_exposure_normalization(RID p_voxel_gi, float p_baked_exposure) {
	VoxelGI *voxel_gi = voxel_gi_owner.get_or_null(p_voxel_gi);
	ERR_FAIL_NULL(voxel_gi);

	voxel_gi->baked_exposure = p_baked_exposure;
}

float GI::voxel_gi_get_baked_exposure_normalization(RID p_voxel_gi) const {
	VoxelGI *voxel_gi = voxel_gi_owner.get_or_null(p_voxel_gi);
	ERR_FAIL_NULL_V(voxel_gi, 0);
	return voxel_gi->baked_exposure;
}

void GI::voxel_gi_set_bias(RID p_voxel_gi, float p_bias) {
	VoxelGI *voxel_gi = voxel_gi_owner.get_or_null(p_voxel_gi);
	ERR_FAIL_NULL(voxel_gi);

	voxel_gi->bias = p_bias;
}

float GI::voxel_gi_get_bias(RID p_voxel_gi) const {
	VoxelGI *voxel_gi = voxel_gi_owner.get_or_null(p_voxel_gi);
	ERR_FAIL_NULL_V(voxel_gi, 0);
	return voxel_gi->bias;
}

void GI::voxel_gi_set_normal_bias(RID p_voxel_gi, float p_normal_bias) {
	VoxelGI *voxel_gi = voxel_gi_owner.get_or_null(p_voxel_gi);
	ERR_FAIL_NULL(voxel_gi);

	voxel_gi->normal_bias = p_normal_bias;
}

float GI::voxel_gi_get_normal_bias(RID p_voxel_gi) const {
	VoxelGI *voxel_gi = voxel_gi_owner.get_or_null(p_voxel_gi);
	ERR_FAIL_NULL_V(voxel_gi, 0);
	return voxel_gi->normal_bias;
}

void GI::voxel_gi_set_interior(RID p_voxel_gi, bool p_enable) {
	VoxelGI *voxel_gi = voxel_gi_owner.get_or_null(p_voxel_gi);
	ERR_FAIL_NULL(voxel_gi);

	voxel_gi->interior = p_enable;
}

void GI::voxel_gi_set_use_two_bounces(RID p_voxel_gi, bool p_enable) {
	VoxelGI *voxel_gi = voxel_gi_owner.get_or_null(p_voxel_gi);
	ERR_FAIL_NULL(voxel_gi);

	voxel_gi->use_two_bounces = p_enable;
	voxel_gi->version++;
}

bool GI::voxel_gi_is_using_two_bounces(RID p_voxel_gi) const {
	VoxelGI *voxel_gi = voxel_gi_owner.get_or_null(p_voxel_gi);
	ERR_FAIL_NULL_V(voxel_gi, false);
	return voxel_gi->use_two_bounces;
}

bool GI::voxel_gi_is_interior(RID p_voxel_gi) const {
	VoxelGI *voxel_gi = voxel_gi_owner.get_or_null(p_voxel_gi);
	ERR_FAIL_NULL_V(voxel_gi, false);
	return voxel_gi->interior;
}

uint32_t GI::voxel_gi_get_version(RID p_voxel_gi) const {
	VoxelGI *voxel_gi = voxel_gi_owner.get_or_null(p_voxel_gi);
	ERR_FAIL_NULL_V(voxel_gi, 0);
	return voxel_gi->version;
}

uint32_t GI::voxel_gi_get_data_version(RID p_voxel_gi) {
	VoxelGI *voxel_gi = voxel_gi_owner.get_or_null(p_voxel_gi);
	ERR_FAIL_NULL_V(voxel_gi, 0);
	return voxel_gi->data_version;
}

RID GI::voxel_gi_get_octree_buffer(RID p_voxel_gi) const {
	VoxelGI *voxel_gi = voxel_gi_owner.get_or_null(p_voxel_gi);
	ERR_FAIL_NULL_V(voxel_gi, RID());
	return voxel_gi->octree_buffer;
}

RID GI::voxel_gi_get_data_buffer(RID p_voxel_gi) const {
	VoxelGI *voxel_gi = voxel_gi_owner.get_or_null(p_voxel_gi);
	ERR_FAIL_NULL_V(voxel_gi, RID());
	return voxel_gi->data_buffer;
}

RID GI::voxel_gi_get_sdf_texture(RID p_voxel_gi) {
	VoxelGI *voxel_gi = voxel_gi_owner.get_or_null(p_voxel_gi);
	ERR_FAIL_NULL_V(voxel_gi, RID());

	return voxel_gi->sdf_texture;
}

Dependency *GI::voxel_gi_get_dependency(RID p_voxel_gi) const {
	VoxelGI *voxel_gi = voxel_gi_owner.get_or_null(p_voxel_gi);
	ERR_FAIL_NULL_V(voxel_gi, nullptr);

	return &voxel_gi->dependency;
}

void GI::sdfgi_reset() {
	sdfgi_current_version++;
}

////////////////////////////////////////////////////////////////////////////////
// SDFGI

static RID create_clear_texture(const RD::TextureFormat &p_format, const String &p_name) {
	RID texture = RD::get_singleton()->texture_create(p_format, RD::TextureView());
	ERR_FAIL_COND_V_MSG(texture.is_null(), RID(), String("Cannot create texture: ") + p_name);

	RD::get_singleton()->set_resource_name(texture, p_name);
	RD::get_singleton()->texture_clear(texture, Color(0, 0, 0, 0), 0, p_format.mipmaps, 0, p_format.array_layers);

	return texture;
}

void GI::SDFGI::create(RID p_env, const Vector3 &p_world_position, uint32_t p_requested_history_size, GI *p_gi) {
	RendererRD::TextureStorage *texture_storage = RendererRD::TextureStorage::get_singleton();
	RendererRD::MaterialStorage *material_storage = RendererRD::MaterialStorage::get_singleton();

	gi = p_gi;
	num_cascades = RendererSceneRenderRD::get_singleton()->environment_get_sdfgi_cascades(p_env);
	min_cell_size = RendererSceneRenderRD::get_singleton()->environment_get_sdfgi_min_cell_size(p_env);
	uses_occlusion = RendererSceneRenderRD::get_singleton()->environment_get_sdfgi_use_occlusion(p_env);
	y_scale_mode = RendererSceneRenderRD::get_singleton()->environment_get_sdfgi_y_scale(p_env);
	static const float y_scale[3] = { 2.0, 1.5, 1.0 };
	y_mult = y_scale[y_scale_mode];
	version = gi->sdfgi_current_version;
	cascades.resize(num_cascades);
	probe_axis_count = SDFGI::PROBE_DIVISOR + 1;
	solid_cell_ratio = gi->sdfgi_solid_cell_ratio;
	solid_cell_count = uint32_t(float(cascade_size * cascade_size * cascade_size) * solid_cell_ratio);

	float base_cell_size = min_cell_size;

	RD::TextureFormat tf_sdf;
	tf_sdf.format = RD::DATA_FORMAT_R8_UNORM;
	tf_sdf.width = cascade_size; // Always 64x64
	tf_sdf.height = cascade_size;
	tf_sdf.depth = cascade_size;
	tf_sdf.texture_type = RD::TEXTURE_TYPE_3D;
	tf_sdf.usage_bits = RD::TEXTURE_USAGE_SAMPLING_BIT | RD::TEXTURE_USAGE_STORAGE_BIT | RD::TEXTURE_USAGE_CAN_COPY_TO_BIT | RD::TEXTURE_USAGE_CAN_COPY_FROM_BIT;

	{
		RD::TextureFormat tf_render = tf_sdf;
		tf_render.format = RD::DATA_FORMAT_R16_UINT;
		render_albedo = create_clear_texture(tf_render, "SDFGI Render Albedo");

		tf_render.format = RD::DATA_FORMAT_R32_UINT;
		render_emission = create_clear_texture(tf_render, "SDFGI Render Emission");
		render_emission_aniso = create_clear_texture(tf_render, "SDFGI Render Emission Aniso");

		tf_render.format = RD::DATA_FORMAT_R8_UNORM; //at least its easy to visualize

		for (int i = 0; i < 8; i++) {
			render_occlusion[i] = create_clear_texture(tf_render, String("SDFGI Render Occlusion ") + itos(i));
		}

		tf_render.format = RD::DATA_FORMAT_R32_UINT;
		render_geom_facing = create_clear_texture(tf_render, "SDFGI Render Geometry Facing");

		tf_render.format = RD::DATA_FORMAT_R8G8B8A8_UINT;
		render_sdf[0] = create_clear_texture(tf_render, "SDFGI Render SDF 0");
		render_sdf[1] = create_clear_texture(tf_render, "SDFGI Render SDF 1");

		tf_render.width /= 2;
		tf_render.height /= 2;
		tf_render.depth /= 2;

		render_sdf_half[0] = create_clear_texture(tf_render, "SDFGI Render SDF Half 0");
		render_sdf_half[1] = create_clear_texture(tf_render, "SDFGI Render SDF Half 1");
	}

	RD::TextureFormat tf_occlusion = tf_sdf;
	tf_occlusion.format = RD::DATA_FORMAT_R16_UINT;
	tf_occlusion.shareable_formats.push_back(RD::DATA_FORMAT_R16_UINT);
	tf_occlusion.shareable_formats.push_back(RD::DATA_FORMAT_R4G4B4A4_UNORM_PACK16);
	tf_occlusion.depth *= cascades.size(); //use depth for occlusion slices
	tf_occlusion.width *= 2; //use width for the other half

	RD::TextureFormat tf_light = tf_sdf;
	tf_light.format = RD::DATA_FORMAT_R32_UINT;
	tf_light.shareable_formats.push_back(RD::DATA_FORMAT_R32_UINT);
	tf_light.shareable_formats.push_back(RD::DATA_FORMAT_E5B9G9R9_UFLOAT_PACK32);

	RD::TextureFormat tf_aniso0 = tf_sdf;
	tf_aniso0.format = RD::DATA_FORMAT_R8G8B8A8_UNORM;
	RD::TextureFormat tf_aniso1 = tf_sdf;
	tf_aniso1.format = RD::DATA_FORMAT_R8G8_UNORM;

	int passes = nearest_shift(cascade_size) - 1;

	//store lightprobe SH
	RD::TextureFormat tf_probes;
	tf_probes.format = RD::DATA_FORMAT_R16G16B16A16_SFLOAT;
	tf_probes.width = probe_axis_count * probe_axis_count;
	tf_probes.height = probe_axis_count * SDFGI::SH_SIZE;
	tf_probes.usage_bits = RD::TEXTURE_USAGE_SAMPLING_BIT | RD::TEXTURE_USAGE_STORAGE_BIT | RD::TEXTURE_USAGE_CAN_COPY_TO_BIT | RD::TEXTURE_USAGE_CAN_COPY_FROM_BIT;
	tf_probes.texture_type = RD::TEXTURE_TYPE_2D_ARRAY;

	history_size = p_requested_history_size;

	RD::TextureFormat tf_probe_history = tf_probes;
	tf_probe_history.format = RD::DATA_FORMAT_R16G16B16A16_SINT; //signed integer because SH are signed
	tf_probe_history.array_layers = history_size;

	RD::TextureFormat tf_probe_average = tf_probes;
	tf_probe_average.format = RD::DATA_FORMAT_R32G32B32A32_SINT; //signed integer because SH are signed
	tf_probe_average.texture_type = RD::TEXTURE_TYPE_2D;

	lightprobe_history_scroll = create_clear_texture(tf_probe_history, "SDFGI LightProbe History Scroll");
	lightprobe_average_scroll = create_clear_texture(tf_probe_average, "SDFGI LightProbe Average Scroll");

	{
		//octahedral lightprobes
		RD::TextureFormat tf_octprobes = tf_probes;
		tf_octprobes.array_layers = cascades.size() * 2;
		tf_octprobes.format = RD::DATA_FORMAT_R32_UINT; //pack well with RGBE
		tf_octprobes.width = probe_axis_count * probe_axis_count * (SDFGI::LIGHTPROBE_OCT_SIZE + 2);
		tf_octprobes.height = probe_axis_count * (SDFGI::LIGHTPROBE_OCT_SIZE + 2);
		tf_octprobes.shareable_formats.push_back(RD::DATA_FORMAT_R32_UINT);
		tf_octprobes.shareable_formats.push_back(RD::DATA_FORMAT_E5B9G9R9_UFLOAT_PACK32);
		//lightprobe texture is an octahedral texture

		lightprobe_data = create_clear_texture(tf_octprobes, "SDFGI LightProbe Data");
		RD::TextureView tv;
		tv.format_override = RD::DATA_FORMAT_E5B9G9R9_UFLOAT_PACK32;
		lightprobe_texture = RD::get_singleton()->texture_create_shared(tv, lightprobe_data);

		//texture handling ambient data, to integrate with volumetric foc
		RD::TextureFormat tf_ambient = tf_probes;
		tf_ambient.array_layers = cascades.size();
		tf_ambient.format = RD::DATA_FORMAT_R16G16B16A16_SFLOAT; //pack well with RGBE
		tf_ambient.width = probe_axis_count * probe_axis_count;
		tf_ambient.height = probe_axis_count;
		tf_ambient.texture_type = RD::TEXTURE_TYPE_2D_ARRAY;
		//lightprobe texture is an octahedral texture
		ambient_texture = create_clear_texture(tf_ambient, "SDFGI Ambient Texture");
	}

	cascades_ubo = RD::get_singleton()->uniform_buffer_create(sizeof(SDFGI::Cascade::UBO) * SDFGI::MAX_CASCADES);

	occlusion_data = create_clear_texture(tf_occlusion, "SDFGI Occlusion Data");
	{
		RD::TextureView tv;
		tv.format_override = RD::DATA_FORMAT_R4G4B4A4_UNORM_PACK16;
		occlusion_texture = RD::get_singleton()->texture_create_shared(tv, occlusion_data);
	}

	for (SDFGI::Cascade &cascade : cascades) {
		/* 3D Textures */

		cascade.sdf_tex = create_clear_texture(tf_sdf, "SDFGI Cascade SDF Texture");

		cascade.light_data = create_clear_texture(tf_light, "SDFGI Cascade Light Data");

		cascade.light_aniso_0_tex = create_clear_texture(tf_aniso0, "SDFGI Cascade Light Aniso 0 Texture");
		cascade.light_aniso_1_tex = create_clear_texture(tf_aniso1, "SDFGI Cascade Light Aniso 1 Texture");

		{
			RD::TextureView tv;
			tv.format_override = RD::DATA_FORMAT_E5B9G9R9_UFLOAT_PACK32;
			cascade.light_tex = RD::get_singleton()->texture_create_shared(tv, cascade.light_data);
		}

		cascade.cell_size = base_cell_size;
		Vector3 world_position = p_world_position;
		world_position.y *= y_mult;
		int32_t probe_cells = cascade_size / SDFGI::PROBE_DIVISOR;
		Vector3 probe_size = Vector3(1, 1, 1) * cascade.cell_size * probe_cells;
		Vector3i probe_pos = Vector3i((world_position / probe_size + Vector3(0.5, 0.5, 0.5)).floor());
		cascade.position = probe_pos * probe_cells;

		cascade.dirty_regions = SDFGI::Cascade::DIRTY_ALL;

		base_cell_size *= 2.0;

		/* Probe History */

		cascade.lightprobe_history_tex = RD::get_singleton()->texture_create(tf_probe_history, RD::TextureView());
		RD::get_singleton()->set_resource_name(cascade.lightprobe_history_tex, "SDFGI Cascade LightProbe History Texture");
		RD::get_singleton()->texture_clear(cascade.lightprobe_history_tex, Color(0, 0, 0, 0), 0, 1, 0, tf_probe_history.array_layers); //needs to be cleared for average to work

		cascade.lightprobe_average_tex = RD::get_singleton()->texture_create(tf_probe_average, RD::TextureView());
		RD::get_singleton()->set_resource_name(cascade.lightprobe_average_tex, "SDFGI Cascade LightProbe Average Texture");
		RD::get_singleton()->texture_clear(cascade.lightprobe_average_tex, Color(0, 0, 0, 0), 0, 1, 0, 1); //needs to be cleared for average to work

		/* Buffers */

		cascade.solid_cell_buffer = RD::get_singleton()->storage_buffer_create(sizeof(SDFGI::Cascade::SolidCell) * solid_cell_count);
		cascade.solid_cell_dispatch_buffer_storage = RD::get_singleton()->storage_buffer_create(sizeof(uint32_t) * 4, Vector<uint8_t>());
		cascade.solid_cell_dispatch_buffer_call = RD::get_singleton()->storage_buffer_create(sizeof(uint32_t) * 4, Vector<uint8_t>(), RD::STORAGE_BUFFER_USAGE_DISPATCH_INDIRECT);
		cascade.lights_buffer = RD::get_singleton()->storage_buffer_create(sizeof(SDFGIShader::Light) * MAX(SDFGI::MAX_STATIC_LIGHTS, SDFGI::MAX_DYNAMIC_LIGHTS));
		{
			Vector<RD::Uniform> uniforms;
			{
				RD::Uniform u;
				u.uniform_type = RD::UNIFORM_TYPE_IMAGE;
				u.binding = 1;
				u.append_id(render_sdf[(passes & 1) ? 1 : 0]); //if passes are even, we read from buffer 0, else we read from buffer 1
				uniforms.push_back(u);
			}
			{
				RD::Uniform u;
				u.uniform_type = RD::UNIFORM_TYPE_IMAGE;
				u.binding = 2;
				u.append_id(render_albedo);
				uniforms.push_back(u);
			}
			{
				RD::Uniform u;
				u.uniform_type = RD::UNIFORM_TYPE_IMAGE;
				u.binding = 3;
				for (int j = 0; j < 8; j++) {
					u.append_id(render_occlusion[j]);
				}
				uniforms.push_back(u);
			}
			{
				RD::Uniform u;
				u.uniform_type = RD::UNIFORM_TYPE_IMAGE;
				u.binding = 4;
				u.append_id(render_emission);
				uniforms.push_back(u);
			}
			{
				RD::Uniform u;
				u.uniform_type = RD::UNIFORM_TYPE_IMAGE;
				u.binding = 5;
				u.append_id(render_emission_aniso);
				uniforms.push_back(u);
			}
			{
				RD::Uniform u;
				u.uniform_type = RD::UNIFORM_TYPE_IMAGE;
				u.binding = 6;
				u.append_id(render_geom_facing);
				uniforms.push_back(u);
			}

			{
				RD::Uniform u;
				u.uniform_type = RD::UNIFORM_TYPE_IMAGE;
				u.binding = 7;
				u.append_id(cascade.sdf_tex);
				uniforms.push_back(u);
			}
			{
				RD::Uniform u;
				u.uniform_type = RD::UNIFORM_TYPE_IMAGE;
				u.binding = 8;
				u.append_id(occlusion_data);
				uniforms.push_back(u);
			}
			{
				RD::Uniform u;
				u.uniform_type = RD::UNIFORM_TYPE_STORAGE_BUFFER;
				u.binding = 10;
				u.append_id(cascade.solid_cell_dispatch_buffer_storage);
				uniforms.push_back(u);
			}
			{
				RD::Uniform u;
				u.uniform_type = RD::UNIFORM_TYPE_STORAGE_BUFFER;
				u.binding = 11;
				u.append_id(cascade.solid_cell_buffer);
				uniforms.push_back(u);
			}

			cascade.sdf_store_uniform_set = RD::get_singleton()->uniform_set_create(uniforms, gi->sdfgi_shader.preprocess.version_get_shader(gi->sdfgi_shader.preprocess_shader, SDFGIShader::PRE_PROCESS_STORE), 0);
		}

		{
			Vector<RD::Uniform> uniforms;
			{
				RD::Uniform u;
				u.uniform_type = RD::UNIFORM_TYPE_IMAGE;
				u.binding = 1;
				u.append_id(render_albedo);
				uniforms.push_back(u);
			}
			{
				RD::Uniform u;
				u.uniform_type = RD::UNIFORM_TYPE_IMAGE;
				u.binding = 2;
				u.append_id(render_geom_facing);
				uniforms.push_back(u);
			}
			{
				RD::Uniform u;
				u.uniform_type = RD::UNIFORM_TYPE_IMAGE;
				u.binding = 3;
				u.append_id(render_emission);
				uniforms.push_back(u);
			}
			{
				RD::Uniform u;
				u.uniform_type = RD::UNIFORM_TYPE_IMAGE;
				u.binding = 4;
				u.append_id(render_emission_aniso);
				uniforms.push_back(u);
			}
			{
				RD::Uniform u;
				u.uniform_type = RD::UNIFORM_TYPE_STORAGE_BUFFER;
				u.binding = 5;
				u.append_id(cascade.solid_cell_dispatch_buffer_storage);
				uniforms.push_back(u);
			}
			{
				RD::Uniform u;
				u.uniform_type = RD::UNIFORM_TYPE_STORAGE_BUFFER;
				u.binding = 6;
				u.append_id(cascade.solid_cell_buffer);
				uniforms.push_back(u);
			}

			cascade.scroll_uniform_set = RD::get_singleton()->uniform_set_create(uniforms, gi->sdfgi_shader.preprocess.version_get_shader(gi->sdfgi_shader.preprocess_shader, SDFGIShader::PRE_PROCESS_SCROLL), 0);
		}
		{
			Vector<RD::Uniform> uniforms;
			{
				RD::Uniform u;
				u.uniform_type = RD::UNIFORM_TYPE_IMAGE;
				u.binding = 1;
				for (int j = 0; j < 8; j++) {
					u.append_id(render_occlusion[j]);
				}
				uniforms.push_back(u);
			}
			{
				RD::Uniform u;
				u.uniform_type = RD::UNIFORM_TYPE_IMAGE;
				u.binding = 2;
				u.append_id(occlusion_data);
				uniforms.push_back(u);
			}

			cascade.scroll_occlusion_uniform_set = RD::get_singleton()->uniform_set_create(uniforms, gi->sdfgi_shader.preprocess.version_get_shader(gi->sdfgi_shader.preprocess_shader, SDFGIShader::PRE_PROCESS_SCROLL_OCCLUSION), 0);
		}
	}

	//direct light
	for (SDFGI::Cascade &cascade : cascades) {
		Vector<RD::Uniform> uniforms;
		{
			RD::Uniform u;
			u.binding = 1;
			u.uniform_type = RD::UNIFORM_TYPE_TEXTURE;
			for (uint32_t j = 0; j < SDFGI::MAX_CASCADES; j++) {
				if (j < cascades.size()) {
					u.append_id(cascades[j].sdf_tex);
				} else {
					u.append_id(texture_storage->texture_rd_get_default(RendererRD::TextureStorage::DEFAULT_RD_TEXTURE_3D_WHITE));
				}
			}
			uniforms.push_back(u);
		}
		{
			RD::Uniform u;
			u.binding = 2;
			u.uniform_type = RD::UNIFORM_TYPE_SAMPLER;
			u.append_id(material_storage->sampler_rd_get_default(RS::CANVAS_ITEM_TEXTURE_FILTER_LINEAR, RS::CANVAS_ITEM_TEXTURE_REPEAT_DISABLED));
			uniforms.push_back(u);
		}
		{
			RD::Uniform u;
			u.binding = 3;
			u.uniform_type = RD::UNIFORM_TYPE_STORAGE_BUFFER;
			u.append_id(cascade.solid_cell_dispatch_buffer_storage);
			uniforms.push_back(u);
		}
		{
			RD::Uniform u;
			u.binding = 4;
			u.uniform_type = RD::UNIFORM_TYPE_STORAGE_BUFFER;
			u.append_id(cascade.solid_cell_buffer);
			uniforms.push_back(u);
		}
		{
			RD::Uniform u;
			u.binding = 5;
			u.uniform_type = RD::UNIFORM_TYPE_IMAGE;
			u.append_id(cascade.light_data);
			uniforms.push_back(u);
		}
		{
			RD::Uniform u;
			u.binding = 6;
			u.uniform_type = RD::UNIFORM_TYPE_IMAGE;
			u.append_id(cascade.light_aniso_0_tex);
			uniforms.push_back(u);
		}
		{
			RD::Uniform u;
			u.binding = 7;
			u.uniform_type = RD::UNIFORM_TYPE_IMAGE;
			u.append_id(cascade.light_aniso_1_tex);
			uniforms.push_back(u);
		}
		{
			RD::Uniform u;
			u.binding = 8;
			u.uniform_type = RD::UNIFORM_TYPE_UNIFORM_BUFFER;
			u.append_id(cascades_ubo);
			uniforms.push_back(u);
		}
		{
			RD::Uniform u;
			u.binding = 9;
			u.uniform_type = RD::UNIFORM_TYPE_STORAGE_BUFFER;
			u.append_id(cascade.lights_buffer);
			uniforms.push_back(u);
		}
		{
			RD::Uniform u;
			u.binding = 10;
			u.uniform_type = RD::UNIFORM_TYPE_TEXTURE;
			u.append_id(lightprobe_texture);
			uniforms.push_back(u);
		}
		{
			RD::Uniform u;
			u.binding = 11;
			u.uniform_type = RD::UNIFORM_TYPE_TEXTURE;
			u.append_id(occlusion_texture);
			uniforms.push_back(u);
		}

		cascade.sdf_direct_light_static_uniform_set = RD::get_singleton()->uniform_set_create(uniforms, gi->sdfgi_shader.direct_light.version_get_shader(gi->sdfgi_shader.direct_light_shader, SDFGIShader::DIRECT_LIGHT_MODE_STATIC), 0);
		cascade.sdf_direct_light_dynamic_uniform_set = RD::get_singleton()->uniform_set_create(uniforms, gi->sdfgi_shader.direct_light.version_get_shader(gi->sdfgi_shader.direct_light_shader, SDFGIShader::DIRECT_LIGHT_MODE_DYNAMIC), 0);
	}

	//preprocess initialize uniform set
	{
		Vector<RD::Uniform> uniforms;
		{
			RD::Uniform u;
			u.uniform_type = RD::UNIFORM_TYPE_IMAGE;
			u.binding = 1;
			u.append_id(render_albedo);
			uniforms.push_back(u);
		}
		{
			RD::Uniform u;
			u.uniform_type = RD::UNIFORM_TYPE_IMAGE;
			u.binding = 2;
			u.append_id(render_sdf[0]);
			uniforms.push_back(u);
		}

		sdf_initialize_uniform_set = RD::get_singleton()->uniform_set_create(uniforms, gi->sdfgi_shader.preprocess.version_get_shader(gi->sdfgi_shader.preprocess_shader, SDFGIShader::PRE_PROCESS_JUMP_FLOOD_INITIALIZE), 0);
	}

	{
		Vector<RD::Uniform> uniforms;
		{
			RD::Uniform u;
			u.uniform_type = RD::UNIFORM_TYPE_IMAGE;
			u.binding = 1;
			u.append_id(render_albedo);
			uniforms.push_back(u);
		}
		{
			RD::Uniform u;
			u.uniform_type = RD::UNIFORM_TYPE_IMAGE;
			u.binding = 2;
			u.append_id(render_sdf_half[0]);
			uniforms.push_back(u);
		}

		sdf_initialize_half_uniform_set = RD::get_singleton()->uniform_set_create(uniforms, gi->sdfgi_shader.preprocess.version_get_shader(gi->sdfgi_shader.preprocess_shader, SDFGIShader::PRE_PROCESS_JUMP_FLOOD_INITIALIZE_HALF), 0);
	}

	//jump flood uniform set
	{
		Vector<RD::Uniform> uniforms;
		{
			RD::Uniform u;
			u.uniform_type = RD::UNIFORM_TYPE_IMAGE;
			u.binding = 1;
			u.append_id(render_sdf[0]);
			uniforms.push_back(u);
		}
		{
			RD::Uniform u;
			u.uniform_type = RD::UNIFORM_TYPE_IMAGE;
			u.binding = 2;
			u.append_id(render_sdf[1]);
			uniforms.push_back(u);
		}

		jump_flood_uniform_set[0] = RD::get_singleton()->uniform_set_create(uniforms, gi->sdfgi_shader.preprocess.version_get_shader(gi->sdfgi_shader.preprocess_shader, SDFGIShader::PRE_PROCESS_JUMP_FLOOD), 0);
		RID aux0 = uniforms.write[0].get_id(0);
		RID aux1 = uniforms.write[1].get_id(0);
		uniforms.write[0].set_id(0, aux1);
		uniforms.write[1].set_id(0, aux0);
		jump_flood_uniform_set[1] = RD::get_singleton()->uniform_set_create(uniforms, gi->sdfgi_shader.preprocess.version_get_shader(gi->sdfgi_shader.preprocess_shader, SDFGIShader::PRE_PROCESS_JUMP_FLOOD), 0);
	}
	//jump flood half uniform set
	{
		Vector<RD::Uniform> uniforms;
		{
			RD::Uniform u;
			u.uniform_type = RD::UNIFORM_TYPE_IMAGE;
			u.binding = 1;
			u.append_id(render_sdf_half[0]);
			uniforms.push_back(u);
		}
		{
			RD::Uniform u;
			u.uniform_type = RD::UNIFORM_TYPE_IMAGE;
			u.binding = 2;
			u.append_id(render_sdf_half[1]);
			uniforms.push_back(u);
		}

		jump_flood_half_uniform_set[0] = RD::get_singleton()->uniform_set_create(uniforms, gi->sdfgi_shader.preprocess.version_get_shader(gi->sdfgi_shader.preprocess_shader, SDFGIShader::PRE_PROCESS_JUMP_FLOOD), 0);
		RID aux0 = uniforms.write[0].get_id(0);
		RID aux1 = uniforms.write[1].get_id(0);
		uniforms.write[0].set_id(0, aux1);
		uniforms.write[1].set_id(0, aux0);
		jump_flood_half_uniform_set[1] = RD::get_singleton()->uniform_set_create(uniforms, gi->sdfgi_shader.preprocess.version_get_shader(gi->sdfgi_shader.preprocess_shader, SDFGIShader::PRE_PROCESS_JUMP_FLOOD), 0);
	}

	//upscale half size sdf
	{
		Vector<RD::Uniform> uniforms;
		{
			RD::Uniform u;
			u.uniform_type = RD::UNIFORM_TYPE_IMAGE;
			u.binding = 1;
			u.append_id(render_albedo);
			uniforms.push_back(u);
		}
		{
			RD::Uniform u;
			u.uniform_type = RD::UNIFORM_TYPE_IMAGE;
			u.binding = 2;
			u.append_id(render_sdf_half[(passes & 1) ? 0 : 1]); //reverse pass order because half size
			uniforms.push_back(u);
		}
		{
			RD::Uniform u;
			u.uniform_type = RD::UNIFORM_TYPE_IMAGE;
			u.binding = 3;
			u.append_id(render_sdf[(passes & 1) ? 0 : 1]); //reverse pass order because it needs an extra JFA pass
			uniforms.push_back(u);
		}

		upscale_jfa_uniform_set_index = (passes & 1) ? 0 : 1;
		sdf_upscale_uniform_set = RD::get_singleton()->uniform_set_create(uniforms, gi->sdfgi_shader.preprocess.version_get_shader(gi->sdfgi_shader.preprocess_shader, SDFGIShader::PRE_PROCESS_JUMP_FLOOD_UPSCALE), 0);
	}

	//occlusion uniform set
	{
		Vector<RD::Uniform> uniforms;
		{
			RD::Uniform u;
			u.uniform_type = RD::UNIFORM_TYPE_IMAGE;
			u.binding = 1;
			u.append_id(render_albedo);
			uniforms.push_back(u);
		}
		{
			RD::Uniform u;
			u.uniform_type = RD::UNIFORM_TYPE_IMAGE;
			u.binding = 2;
			for (int i = 0; i < 8; i++) {
				u.append_id(render_occlusion[i]);
			}
			uniforms.push_back(u);
		}
		{
			RD::Uniform u;
			u.uniform_type = RD::UNIFORM_TYPE_IMAGE;
			u.binding = 3;
			u.append_id(render_geom_facing);
			uniforms.push_back(u);
		}

		occlusion_uniform_set = RD::get_singleton()->uniform_set_create(uniforms, gi->sdfgi_shader.preprocess.version_get_shader(gi->sdfgi_shader.preprocess_shader, SDFGIShader::PRE_PROCESS_OCCLUSION), 0);
	}

	for (uint32_t i = 0; i < cascades.size(); i++) {
		//integrate uniform

		Vector<RD::Uniform> uniforms;

		{
			RD::Uniform u;
			u.binding = 1;
			u.uniform_type = RD::UNIFORM_TYPE_TEXTURE;
			for (uint32_t j = 0; j < SDFGI::MAX_CASCADES; j++) {
				if (j < cascades.size()) {
					u.append_id(cascades[j].sdf_tex);
				} else {
					u.append_id(texture_storage->texture_rd_get_default(RendererRD::TextureStorage::DEFAULT_RD_TEXTURE_3D_WHITE));
				}
			}
			uniforms.push_back(u);
		}
		{
			RD::Uniform u;
			u.binding = 2;
			u.uniform_type = RD::UNIFORM_TYPE_TEXTURE;
			for (uint32_t j = 0; j < SDFGI::MAX_CASCADES; j++) {
				if (j < cascades.size()) {
					u.append_id(cascades[j].light_tex);
				} else {
					u.append_id(texture_storage->texture_rd_get_default(RendererRD::TextureStorage::DEFAULT_RD_TEXTURE_3D_WHITE));
				}
			}
			uniforms.push_back(u);
		}
		{
			RD::Uniform u;
			u.binding = 3;
			u.uniform_type = RD::UNIFORM_TYPE_TEXTURE;
			for (uint32_t j = 0; j < SDFGI::MAX_CASCADES; j++) {
				if (j < cascades.size()) {
					u.append_id(cascades[j].light_aniso_0_tex);
				} else {
					u.append_id(texture_storage->texture_rd_get_default(RendererRD::TextureStorage::DEFAULT_RD_TEXTURE_3D_WHITE));
				}
			}
			uniforms.push_back(u);
		}
		{
			RD::Uniform u;
			u.binding = 4;
			u.uniform_type = RD::UNIFORM_TYPE_TEXTURE;
			for (uint32_t j = 0; j < SDFGI::MAX_CASCADES; j++) {
				if (j < cascades.size()) {
					u.append_id(cascades[j].light_aniso_1_tex);
				} else {
					u.append_id(texture_storage->texture_rd_get_default(RendererRD::TextureStorage::DEFAULT_RD_TEXTURE_3D_WHITE));
				}
			}
			uniforms.push_back(u);
		}
		{
			RD::Uniform u;
			u.uniform_type = RD::UNIFORM_TYPE_SAMPLER;
			u.binding = 6;
			u.append_id(material_storage->sampler_rd_get_default(RS::CANVAS_ITEM_TEXTURE_FILTER_LINEAR, RS::CANVAS_ITEM_TEXTURE_REPEAT_DISABLED));
			uniforms.push_back(u);
		}

		{
			RD::Uniform u;
			u.uniform_type = RD::UNIFORM_TYPE_UNIFORM_BUFFER;
			u.binding = 7;
			u.append_id(cascades_ubo);
			uniforms.push_back(u);
		}
		{
			RD::Uniform u;
			u.uniform_type = RD::UNIFORM_TYPE_IMAGE;
			u.binding = 8;
			u.append_id(lightprobe_data);
			uniforms.push_back(u);
		}

		{
			RD::Uniform u;
			u.uniform_type = RD::UNIFORM_TYPE_IMAGE;
			u.binding = 9;
			u.append_id(cascades[i].lightprobe_history_tex);
			uniforms.push_back(u);
		}
		{
			RD::Uniform u;
			u.uniform_type = RD::UNIFORM_TYPE_IMAGE;
			u.binding = 10;
			u.append_id(cascades[i].lightprobe_average_tex);
			uniforms.push_back(u);
		}

		{
			RD::Uniform u;
			u.uniform_type = RD::UNIFORM_TYPE_IMAGE;
			u.binding = 11;
			u.append_id(lightprobe_history_scroll);
			uniforms.push_back(u);
		}
		{
			RD::Uniform u;
			u.uniform_type = RD::UNIFORM_TYPE_IMAGE;
			u.binding = 12;
			u.append_id(lightprobe_average_scroll);
			uniforms.push_back(u);
		}
		{
			RD::Uniform u;
			u.uniform_type = RD::UNIFORM_TYPE_IMAGE;
			u.binding = 13;
			RID parent_average;
			if (cascades.size() == 1) {
				// If there is only one SDFGI cascade, we can't use the previous cascade for blending.
				parent_average = cascades[i].lightprobe_average_tex;
			} else if (i < cascades.size() - 1) {
				parent_average = cascades[i + 1].lightprobe_average_tex;
			} else {
				parent_average = cascades[i - 1].lightprobe_average_tex; //to use something, but it won't be used
			}
			u.append_id(parent_average);
			uniforms.push_back(u);
		}
		{
			RD::Uniform u;
			u.uniform_type = RD::UNIFORM_TYPE_IMAGE;
			u.binding = 14;
			u.append_id(ambient_texture);
			uniforms.push_back(u);
		}

		cascades[i].integrate_uniform_set = RD::get_singleton()->uniform_set_create(uniforms, gi->sdfgi_shader.integrate.version_get_shader(gi->sdfgi_shader.integrate_shader, 0), 0);
	}

	bounce_feedback = RendererSceneRenderRD::get_singleton()->environment_get_sdfgi_bounce_feedback(p_env);
	energy = RendererSceneRenderRD::get_singleton()->environment_get_sdfgi_energy(p_env);
	normal_bias = RendererSceneRenderRD::get_singleton()->environment_get_sdfgi_normal_bias(p_env);
	probe_bias = RendererSceneRenderRD::get_singleton()->environment_get_sdfgi_probe_bias(p_env);
	reads_sky = RendererSceneRenderRD::get_singleton()->environment_get_sdfgi_read_sky_light(p_env);
}

void GI::SDFGI::free_data() {
	// we don't free things here, we handle SDFGI differently at the moment destructing the object when it needs to change.
}

GI::SDFGI::~SDFGI() {
	for (const SDFGI::Cascade &c : cascades) {
		RD::get_singleton()->free(c.light_data);
		RD::get_singleton()->free(c.light_aniso_0_tex);
		RD::get_singleton()->free(c.light_aniso_1_tex);
		RD::get_singleton()->free(c.sdf_tex);
		RD::get_singleton()->free(c.solid_cell_dispatch_buffer_storage);
		RD::get_singleton()->free(c.solid_cell_dispatch_buffer_call);
		RD::get_singleton()->free(c.solid_cell_buffer);
		RD::get_singleton()->free(c.lightprobe_history_tex);
		RD::get_singleton()->free(c.lightprobe_average_tex);
		RD::get_singleton()->free(c.lights_buffer);
	}

	RD::get_singleton()->free(render_albedo);
	RD::get_singleton()->free(render_emission);
	RD::get_singleton()->free(render_emission_aniso);

	RD::get_singleton()->free(render_sdf[0]);
	RD::get_singleton()->free(render_sdf[1]);

	RD::get_singleton()->free(render_sdf_half[0]);
	RD::get_singleton()->free(render_sdf_half[1]);

	for (int i = 0; i < 8; i++) {
		RD::get_singleton()->free(render_occlusion[i]);
	}

	RD::get_singleton()->free(render_geom_facing);

	RD::get_singleton()->free(lightprobe_data);
	RD::get_singleton()->free(lightprobe_history_scroll);
	RD::get_singleton()->free(lightprobe_average_scroll);
	RD::get_singleton()->free(occlusion_data);
	RD::get_singleton()->free(ambient_texture);

	RD::get_singleton()->free(cascades_ubo);

	for (uint32_t v = 0; v < RendererSceneRender::MAX_RENDER_VIEWS; v++) {
		if (RD::get_singleton()->uniform_set_is_valid(debug_uniform_set[v])) {
			RD::get_singleton()->free(debug_uniform_set[v]);
		}
		debug_uniform_set[v] = RID();
	}

	if (RD::get_singleton()->uniform_set_is_valid(debug_probes_uniform_set)) {
		RD::get_singleton()->free(debug_probes_uniform_set);
	}
	debug_probes_uniform_set = RID();

	if (debug_probes_scene_data_ubo.is_valid()) {
		RD::get_singleton()->free(debug_probes_scene_data_ubo);
		debug_probes_scene_data_ubo = RID();
	}
}

void GI::SDFGI::update(RID p_env, const Vector3 &p_world_position) {
	bounce_feedback = RendererSceneRenderRD::get_singleton()->environment_get_sdfgi_bounce_feedback(p_env);
	energy = RendererSceneRenderRD::get_singleton()->environment_get_sdfgi_energy(p_env);
	normal_bias = RendererSceneRenderRD::get_singleton()->environment_get_sdfgi_normal_bias(p_env);
	probe_bias = RendererSceneRenderRD::get_singleton()->environment_get_sdfgi_probe_bias(p_env);
	reads_sky = RendererSceneRenderRD::get_singleton()->environment_get_sdfgi_read_sky_light(p_env);

	int32_t drag_margin = (cascade_size / SDFGI::PROBE_DIVISOR) / 2;

	for (SDFGI::Cascade &cascade : cascades) {
		cascade.dirty_regions = Vector3i();

		Vector3 probe_half_size = Vector3(1, 1, 1) * cascade.cell_size * float(cascade_size / SDFGI::PROBE_DIVISOR) * 0.5;
		probe_half_size = Vector3(0, 0, 0);

		Vector3 world_position = p_world_position;
		world_position.y *= y_mult;
		Vector3i pos_in_cascade = Vector3i((world_position + probe_half_size) / cascade.cell_size);

		for (int j = 0; j < 3; j++) {
			if (pos_in_cascade[j] < cascade.position[j]) {
				while (pos_in_cascade[j] < (cascade.position[j] - drag_margin)) {
					cascade.position[j] -= drag_margin * 2;
					cascade.dirty_regions[j] += drag_margin * 2;
				}
			} else if (pos_in_cascade[j] > cascade.position[j]) {
				while (pos_in_cascade[j] > (cascade.position[j] + drag_margin)) {
					cascade.position[j] += drag_margin * 2;
					cascade.dirty_regions[j] -= drag_margin * 2;
				}
			}

			if (cascade.dirty_regions[j] == 0) {
				continue; // not dirty
			} else if (uint32_t(ABS(cascade.dirty_regions[j])) >= cascade_size) {
				//moved too much, just redraw everything (make all dirty)
				cascade.dirty_regions = SDFGI::Cascade::DIRTY_ALL;
				break;
			}
		}

		if (cascade.dirty_regions != Vector3i() && cascade.dirty_regions != SDFGI::Cascade::DIRTY_ALL) {
			//see how much the total dirty volume represents from the total volume
			uint32_t total_volume = cascade_size * cascade_size * cascade_size;
			uint32_t safe_volume = 1;
			for (int j = 0; j < 3; j++) {
				safe_volume *= cascade_size - ABS(cascade.dirty_regions[j]);
			}
			uint32_t dirty_volume = total_volume - safe_volume;
			if (dirty_volume > (safe_volume / 2)) {
				//more than half the volume is dirty, make all dirty so its only rendered once
				cascade.dirty_regions = SDFGI::Cascade::DIRTY_ALL;
			}
		}
	}
}

void GI::SDFGI::update_light() {
	RD::get_singleton()->draw_command_begin_label("SDFGI Update dynamic Light");

	for (uint32_t i = 0; i < cascades.size(); i++) {
		RD::get_singleton()->buffer_copy(cascades[i].solid_cell_dispatch_buffer_storage, cascades[i].solid_cell_dispatch_buffer_call, 0, 0, sizeof(uint32_t) * 4);
	}

	/* Update dynamic light */

	RD::ComputeListID compute_list = RD::get_singleton()->compute_list_begin();
	RD::get_singleton()->compute_list_bind_compute_pipeline(compute_list, gi->sdfgi_shader.direct_light_pipeline[SDFGIShader::DIRECT_LIGHT_MODE_DYNAMIC]);

	SDFGIShader::DirectLightPushConstant push_constant;

	push_constant.grid_size[0] = cascade_size;
	push_constant.grid_size[1] = cascade_size;
	push_constant.grid_size[2] = cascade_size;
	push_constant.max_cascades = cascades.size();
	push_constant.probe_axis_size = probe_axis_count;
	push_constant.bounce_feedback = bounce_feedback;
	push_constant.y_mult = y_mult;
	push_constant.use_occlusion = uses_occlusion;

	for (uint32_t i = 0; i < cascades.size(); i++) {
		SDFGI::Cascade &cascade = cascades[i];
		push_constant.light_count = cascade_dynamic_light_count[i];
		push_constant.cascade = i;

		if (cascades[i].all_dynamic_lights_dirty || gi->sdfgi_frames_to_update_light == RS::ENV_SDFGI_UPDATE_LIGHT_IN_1_FRAME) {
			push_constant.process_offset = 0;
			push_constant.process_increment = 1;
		} else {
			static const uint32_t frames_to_update_table[RS::ENV_SDFGI_UPDATE_LIGHT_MAX] = {
				1, 2, 4, 8, 16
			};

			uint32_t frames_to_update = frames_to_update_table[gi->sdfgi_frames_to_update_light];

			push_constant.process_offset = RSG::rasterizer->get_frame_number() % frames_to_update;
			push_constant.process_increment = frames_to_update;
		}
		cascades[i].all_dynamic_lights_dirty = false;

		RD::get_singleton()->compute_list_bind_uniform_set(compute_list, cascade.sdf_direct_light_dynamic_uniform_set, 0);
		RD::get_singleton()->compute_list_set_push_constant(compute_list, &push_constant, sizeof(SDFGIShader::DirectLightPushConstant));
		RD::get_singleton()->compute_list_dispatch_indirect(compute_list, cascade.solid_cell_dispatch_buffer_call, 0);
	}
	RD::get_singleton()->compute_list_end();
	RD::get_singleton()->draw_command_end_label();
}

void GI::SDFGI::update_probes(RID p_env, SkyRD::Sky *p_sky) {
	RD::get_singleton()->draw_command_begin_label("SDFGI Update Probes");

	SDFGIShader::IntegratePushConstant push_constant;
	push_constant.grid_size[1] = cascade_size;
	push_constant.grid_size[2] = cascade_size;
	push_constant.grid_size[0] = cascade_size;
	push_constant.max_cascades = cascades.size();
	push_constant.probe_axis_size = probe_axis_count;
	push_constant.history_index = render_pass % history_size;
	push_constant.history_size = history_size;
	static const uint32_t ray_count[RS::ENV_SDFGI_RAY_COUNT_MAX] = { 4, 8, 16, 32, 64, 96, 128 };
	push_constant.ray_count = ray_count[gi->sdfgi_ray_count];
	push_constant.ray_bias = probe_bias;
	push_constant.image_size[0] = probe_axis_count * probe_axis_count;
	push_constant.image_size[1] = probe_axis_count;
	push_constant.store_ambient_texture = RendererSceneRenderRD::get_singleton()->environment_get_volumetric_fog_enabled(p_env);

	RID sky_uniform_set = gi->sdfgi_shader.integrate_default_sky_uniform_set;
	push_constant.sky_flags = 0;
	push_constant.y_mult = y_mult;

	if (reads_sky && p_env.is_valid()) {
		push_constant.sky_energy = RendererSceneRenderRD::get_singleton()->environment_get_bg_energy_multiplier(p_env);

		if (RendererSceneRenderRD::get_singleton()->environment_get_background(p_env) == RS::ENV_BG_CLEAR_COLOR) {
			push_constant.sky_flags |= SDFGIShader::IntegratePushConstant::SKY_FLAGS_MODE_COLOR;
			Color c = RSG::texture_storage->get_default_clear_color().srgb_to_linear();
			push_constant.sky_color_or_orientation[0] = c.r;
			push_constant.sky_color_or_orientation[1] = c.g;
			push_constant.sky_color_or_orientation[2] = c.b;
		} else if (RendererSceneRenderRD::get_singleton()->environment_get_background(p_env) == RS::ENV_BG_COLOR) {
			push_constant.sky_flags |= SDFGIShader::IntegratePushConstant::SKY_FLAGS_MODE_COLOR;
			Color c = RendererSceneRenderRD::get_singleton()->environment_get_bg_color(p_env);
			push_constant.sky_color_or_orientation[0] = c.r;
			push_constant.sky_color_or_orientation[1] = c.g;
			push_constant.sky_color_or_orientation[2] = c.b;

		} else if (RendererSceneRenderRD::get_singleton()->environment_get_background(p_env) == RS::ENV_BG_SKY) {
			if (p_sky && p_sky->radiance.is_valid()) {
				if (integrate_sky_uniform_set.is_null() || !RD::get_singleton()->uniform_set_is_valid(integrate_sky_uniform_set)) {
					Vector<RD::Uniform> uniforms;

					{
						RD::Uniform u;
						u.uniform_type = RD::UNIFORM_TYPE_TEXTURE;
						u.binding = 0;
						u.append_id(p_sky->radiance);
						uniforms.push_back(u);
					}

					{
						RD::Uniform u;
						u.uniform_type = RD::UNIFORM_TYPE_SAMPLER;
						u.binding = 1;
						u.append_id(RendererRD::MaterialStorage::get_singleton()->sampler_rd_get_default(RS::CANVAS_ITEM_TEXTURE_FILTER_LINEAR_WITH_MIPMAPS, RS::CANVAS_ITEM_TEXTURE_REPEAT_DISABLED));
						uniforms.push_back(u);
					}

					integrate_sky_uniform_set = RD::get_singleton()->uniform_set_create(uniforms, gi->sdfgi_shader.integrate.version_get_shader(gi->sdfgi_shader.integrate_shader, 0), 1);
				}
				sky_uniform_set = integrate_sky_uniform_set;
				push_constant.sky_flags |= SDFGIShader::IntegratePushConstant::SKY_FLAGS_MODE_SKY;

				// Encode sky orientation as quaternion in existing push constants.
				const Basis sky_basis = RendererSceneRenderRD::get_singleton()->environment_get_sky_orientation(p_env);
				const Quaternion sky_quaternion = sky_basis.get_quaternion().inverse();
				push_constant.sky_color_or_orientation[0] = sky_quaternion.x;
				push_constant.sky_color_or_orientation[1] = sky_quaternion.y;
				push_constant.sky_color_or_orientation[2] = sky_quaternion.z;
				// Ideally we would reconstruct the largest component for least error, but sky contribution to GI is low frequency so just needs to get the idea across.
				push_constant.sky_flags |= SDFGIShader::IntegratePushConstant::SKY_FLAGS_ORIENTATION_SIGN * (sky_quaternion.w < 0.0 ? 0 : 1);
			}
		}
	}

	render_pass++;

	RD::ComputeListID compute_list = RD::get_singleton()->compute_list_begin();
	RD::get_singleton()->compute_list_bind_compute_pipeline(compute_list, gi->sdfgi_shader.integrate_pipeline[SDFGIShader::INTEGRATE_MODE_PROCESS]);

	int32_t probe_divisor = cascade_size / SDFGI::PROBE_DIVISOR;
	for (uint32_t i = 0; i < cascades.size(); i++) {
		push_constant.cascade = i;
		push_constant.world_offset[0] = cascades[i].position.x / probe_divisor;
		push_constant.world_offset[1] = cascades[i].position.y / probe_divisor;
		push_constant.world_offset[2] = cascades[i].position.z / probe_divisor;

		RD::get_singleton()->compute_list_bind_uniform_set(compute_list, cascades[i].integrate_uniform_set, 0);
		RD::get_singleton()->compute_list_bind_uniform_set(compute_list, sky_uniform_set, 1);

		RD::get_singleton()->compute_list_set_push_constant(compute_list, &push_constant, sizeof(SDFGIShader::IntegratePushConstant));
		RD::get_singleton()->compute_list_dispatch_threads(compute_list, probe_axis_count * probe_axis_count, probe_axis_count, 1);
	}

	RD::get_singleton()->compute_list_end();
	RD::get_singleton()->draw_command_end_label();
}

void GI::SDFGI::store_probes() {
	RD::get_singleton()->draw_command_begin_label("SDFGI Store Probes");

	SDFGIShader::IntegratePushConstant push_constant;
	push_constant.grid_size[1] = cascade_size;
	push_constant.grid_size[2] = cascade_size;
	push_constant.grid_size[0] = cascade_size;
	push_constant.max_cascades = cascades.size();
	push_constant.probe_axis_size = probe_axis_count;
	push_constant.history_index = render_pass % history_size;
	push_constant.history_size = history_size;
	static const uint32_t ray_count[RS::ENV_SDFGI_RAY_COUNT_MAX] = { 4, 8, 16, 32, 64, 96, 128 };
	push_constant.ray_count = ray_count[gi->sdfgi_ray_count];
	push_constant.ray_bias = probe_bias;
	push_constant.image_size[0] = probe_axis_count * probe_axis_count;
	push_constant.image_size[1] = probe_axis_count;
	push_constant.store_ambient_texture = false;

	push_constant.sky_flags = 0;
	push_constant.y_mult = y_mult;

	// Then store values into the lightprobe texture. Separating these steps has a small performance hit, but it allows for multiple bounces
	RENDER_TIMESTAMP("Average SDFGI Probes");

	RD::ComputeListID compute_list = RD::get_singleton()->compute_list_begin();
	RD::get_singleton()->compute_list_bind_compute_pipeline(compute_list, gi->sdfgi_shader.integrate_pipeline[SDFGIShader::INTEGRATE_MODE_STORE]);

	//convert to octahedral to store
	push_constant.image_size[0] *= SDFGI::LIGHTPROBE_OCT_SIZE;
	push_constant.image_size[1] *= SDFGI::LIGHTPROBE_OCT_SIZE;

	for (uint32_t i = 0; i < cascades.size(); i++) {
		push_constant.cascade = i;
		RD::get_singleton()->compute_list_bind_uniform_set(compute_list, cascades[i].integrate_uniform_set, 0);
		RD::get_singleton()->compute_list_bind_uniform_set(compute_list, gi->sdfgi_shader.integrate_default_sky_uniform_set, 1);
		RD::get_singleton()->compute_list_set_push_constant(compute_list, &push_constant, sizeof(SDFGIShader::IntegratePushConstant));
		RD::get_singleton()->compute_list_dispatch_threads(compute_list, probe_axis_count * probe_axis_count * SDFGI::LIGHTPROBE_OCT_SIZE, probe_axis_count * SDFGI::LIGHTPROBE_OCT_SIZE, 1);
	}

	RD::get_singleton()->compute_list_end();

	RD::get_singleton()->draw_command_end_label();
}

int GI::SDFGI::get_pending_region_data(int p_region, Vector3i &r_local_offset, Vector3i &r_local_size, AABB &r_bounds) const {
	int dirty_count = 0;
	for (uint32_t i = 0; i < cascades.size(); i++) {
		const SDFGI::Cascade &c = cascades[i];

		if (c.dirty_regions == SDFGI::Cascade::DIRTY_ALL) {
			if (dirty_count == p_region) {
				r_local_offset = Vector3i();
				r_local_size = Vector3i(1, 1, 1) * cascade_size;

				r_bounds.position = Vector3((Vector3i(1, 1, 1) * -int32_t(cascade_size >> 1) + c.position)) * c.cell_size * Vector3(1, 1.0 / y_mult, 1);
				r_bounds.size = Vector3(r_local_size) * c.cell_size * Vector3(1, 1.0 / y_mult, 1);
				return i;
			}
			dirty_count++;
		} else {
			for (int j = 0; j < 3; j++) {
				if (c.dirty_regions[j] != 0) {
					if (dirty_count == p_region) {
						Vector3i from = Vector3i(0, 0, 0);
						Vector3i to = Vector3i(1, 1, 1) * cascade_size;

						if (c.dirty_regions[j] > 0) {
							//fill from the beginning
							to[j] = c.dirty_regions[j];
						} else {
							//fill from the end
							from[j] = to[j] + c.dirty_regions[j];
						}

						for (int k = 0; k < j; k++) {
							// "chip" away previous regions to avoid re-voxelizing the same thing
							if (c.dirty_regions[k] > 0) {
								from[k] += c.dirty_regions[k];
							} else if (c.dirty_regions[k] < 0) {
								to[k] += c.dirty_regions[k];
							}
						}

						r_local_offset = from;
						r_local_size = to - from;

						r_bounds.position = Vector3(from + Vector3i(1, 1, 1) * -int32_t(cascade_size >> 1) + c.position) * c.cell_size * Vector3(1, 1.0 / y_mult, 1);
						r_bounds.size = Vector3(r_local_size) * c.cell_size * Vector3(1, 1.0 / y_mult, 1);

						return i;
					}

					dirty_count++;
				}
			}
		}
	}
	return -1;
}

void GI::SDFGI::update_cascades() {
	//update cascades
	SDFGI::Cascade::UBO cascade_data[SDFGI::MAX_CASCADES];
	int32_t probe_divisor = cascade_size / SDFGI::PROBE_DIVISOR;

	for (uint32_t i = 0; i < cascades.size(); i++) {
		Vector3 pos = Vector3((Vector3i(1, 1, 1) * -int32_t(cascade_size >> 1) + cascades[i].position)) * cascades[i].cell_size;

		cascade_data[i].offset[0] = pos.x;
		cascade_data[i].offset[1] = pos.y;
		cascade_data[i].offset[2] = pos.z;
		cascade_data[i].to_cell = 1.0 / cascades[i].cell_size;
		cascade_data[i].probe_offset[0] = cascades[i].position.x / probe_divisor;
		cascade_data[i].probe_offset[1] = cascades[i].position.y / probe_divisor;
		cascade_data[i].probe_offset[2] = cascades[i].position.z / probe_divisor;
		cascade_data[i].pad = 0;
	}

	RD::get_singleton()->buffer_update(cascades_ubo, 0, sizeof(SDFGI::Cascade::UBO) * SDFGI::MAX_CASCADES, cascade_data);
}

void GI::SDFGI::debug_draw(uint32_t p_view_count, const Projection *p_projections, const Transform3D &p_transform, int p_width, int p_height, RID p_render_target, RID p_texture, const Vector<RID> &p_texture_views) {
	RendererRD::TextureStorage *texture_storage = RendererRD::TextureStorage::get_singleton();
	RendererRD::MaterialStorage *material_storage = RendererRD::MaterialStorage::get_singleton();
	RendererRD::CopyEffects *copy_effects = RendererRD::CopyEffects::get_singleton();

	for (uint32_t v = 0; v < p_view_count; v++) {
		if (!debug_uniform_set[v].is_valid() || !RD::get_singleton()->uniform_set_is_valid(debug_uniform_set[v])) {
			Vector<RD::Uniform> uniforms;
			{
				RD::Uniform u;
				u.binding = 1;
				u.uniform_type = RD::UNIFORM_TYPE_TEXTURE;
				for (uint32_t i = 0; i < SDFGI::MAX_CASCADES; i++) {
					if (i < cascades.size()) {
						u.append_id(cascades[i].sdf_tex);
					} else {
						u.append_id(texture_storage->texture_rd_get_default(RendererRD::TextureStorage::DEFAULT_RD_TEXTURE_3D_WHITE));
					}
				}
				uniforms.push_back(u);
			}
			{
				RD::Uniform u;
				u.binding = 2;
				u.uniform_type = RD::UNIFORM_TYPE_TEXTURE;
				for (uint32_t i = 0; i < SDFGI::MAX_CASCADES; i++) {
					if (i < cascades.size()) {
						u.append_id(cascades[i].light_tex);
					} else {
						u.append_id(texture_storage->texture_rd_get_default(RendererRD::TextureStorage::DEFAULT_RD_TEXTURE_3D_WHITE));
					}
				}
				uniforms.push_back(u);
			}
			{
				RD::Uniform u;
				u.binding = 3;
				u.uniform_type = RD::UNIFORM_TYPE_TEXTURE;
				for (uint32_t i = 0; i < SDFGI::MAX_CASCADES; i++) {
					if (i < cascades.size()) {
						u.append_id(cascades[i].light_aniso_0_tex);
					} else {
						u.append_id(texture_storage->texture_rd_get_default(RendererRD::TextureStorage::DEFAULT_RD_TEXTURE_3D_WHITE));
					}
				}
				uniforms.push_back(u);
			}
			{
				RD::Uniform u;
				u.binding = 4;
				u.uniform_type = RD::UNIFORM_TYPE_TEXTURE;
				for (uint32_t i = 0; i < SDFGI::MAX_CASCADES; i++) {
					if (i < cascades.size()) {
						u.append_id(cascades[i].light_aniso_1_tex);
					} else {
						u.append_id(texture_storage->texture_rd_get_default(RendererRD::TextureStorage::DEFAULT_RD_TEXTURE_3D_WHITE));
					}
				}
				uniforms.push_back(u);
			}
			{
				RD::Uniform u;
				u.binding = 5;
				u.uniform_type = RD::UNIFORM_TYPE_TEXTURE;
				u.append_id(occlusion_texture);
				uniforms.push_back(u);
			}
			{
				RD::Uniform u;
				u.binding = 8;
				u.uniform_type = RD::UNIFORM_TYPE_SAMPLER;
				u.append_id(material_storage->sampler_rd_get_default(RS::CANVAS_ITEM_TEXTURE_FILTER_LINEAR, RS::CANVAS_ITEM_TEXTURE_REPEAT_DISABLED));
				uniforms.push_back(u);
			}
			{
				RD::Uniform u;
				u.binding = 9;
				u.uniform_type = RD::UNIFORM_TYPE_UNIFORM_BUFFER;
				u.append_id(cascades_ubo);
				uniforms.push_back(u);
			}
			{
				RD::Uniform u;
				u.binding = 10;
				u.uniform_type = RD::UNIFORM_TYPE_IMAGE;
				u.append_id(p_texture_views[v]);
				uniforms.push_back(u);
			}
			{
				RD::Uniform u;
				u.binding = 11;
				u.uniform_type = RD::UNIFORM_TYPE_TEXTURE;
				u.append_id(lightprobe_texture);
				uniforms.push_back(u);
			}
			debug_uniform_set[v] = RD::get_singleton()->uniform_set_create(uniforms, gi->sdfgi_shader.debug_shader_version, 0);
		}

		RD::ComputeListID compute_list = RD::get_singleton()->compute_list_begin();
		RD::get_singleton()->compute_list_bind_compute_pipeline(compute_list, gi->sdfgi_shader.debug_pipeline);
		RD::get_singleton()->compute_list_bind_uniform_set(compute_list, debug_uniform_set[v], 0);

		SDFGIShader::DebugPushConstant push_constant;
		push_constant.grid_size[0] = cascade_size;
		push_constant.grid_size[1] = cascade_size;
		push_constant.grid_size[2] = cascade_size;
		push_constant.max_cascades = cascades.size();
		push_constant.screen_size[0] = p_width;
		push_constant.screen_size[1] = p_height;
		push_constant.y_mult = y_mult;

		push_constant.z_near = -p_projections[v].get_z_near();

		for (int i = 0; i < 3; i++) {
			for (int j = 0; j < 3; j++) {
				push_constant.cam_basis[i][j] = p_transform.basis.rows[j][i];
			}
		}
		push_constant.cam_origin[0] = p_transform.origin[0];
		push_constant.cam_origin[1] = p_transform.origin[1];
		push_constant.cam_origin[2] = p_transform.origin[2];

		// need to properly unproject for asymmetric projection matrices in stereo..
		Projection inv_projection = p_projections[v].inverse();
		for (int i = 0; i < 4; i++) {
			for (int j = 0; j < 3; j++) {
				push_constant.inv_projection[j][i] = inv_projection.columns[i][j];
			}
		}

		RD::get_singleton()->compute_list_set_push_constant(compute_list, &push_constant, sizeof(SDFGIShader::DebugPushConstant));

		RD::get_singleton()->compute_list_dispatch_threads(compute_list, p_width, p_height, 1);
		RD::get_singleton()->compute_list_end();
	}

	Size2i rtsize = texture_storage->render_target_get_size(p_render_target);
	copy_effects->copy_to_fb_rect(p_texture, texture_storage->render_target_get_rd_framebuffer(p_render_target), Rect2i(Point2i(), rtsize), true, false, false, false, RID(), p_view_count > 1);
}

void GI::SDFGI::debug_probes(RID p_framebuffer, const uint32_t p_view_count, const Projection *p_camera_with_transforms) {
	RendererRD::MaterialStorage *material_storage = RendererRD::MaterialStorage::get_singleton();

	// setup scene data
	{
		SDFGIShader::DebugProbesSceneData scene_data;

		if (debug_probes_scene_data_ubo.is_null()) {
			debug_probes_scene_data_ubo = RD::get_singleton()->uniform_buffer_create(sizeof(SDFGIShader::DebugProbesSceneData));
		}

		for (uint32_t v = 0; v < p_view_count; v++) {
			RendererRD::MaterialStorage::store_camera(p_camera_with_transforms[v], scene_data.projection[v]);
		}

		RD::get_singleton()->buffer_update(debug_probes_scene_data_ubo, 0, sizeof(SDFGIShader::DebugProbesSceneData), &scene_data);
	}

	// setup push constant
	SDFGIShader::DebugProbesPushConstant push_constant;

	//gen spheres from strips
	uint32_t band_points = 16;
	push_constant.band_power = 4;
	push_constant.sections_in_band = ((band_points / 2) - 1);
	push_constant.band_mask = band_points - 2;
	push_constant.section_arc = Math_TAU / float(push_constant.sections_in_band);
	push_constant.y_mult = y_mult;

	uint32_t total_points = push_constant.sections_in_band * band_points;
	uint32_t total_probes = probe_axis_count * probe_axis_count * probe_axis_count;

	push_constant.grid_size[0] = cascade_size;
	push_constant.grid_size[1] = cascade_size;
	push_constant.grid_size[2] = cascade_size;
	push_constant.cascade = 0;

	push_constant.probe_axis_size = probe_axis_count;

	if (!debug_probes_uniform_set.is_valid() || !RD::get_singleton()->uniform_set_is_valid(debug_probes_uniform_set)) {
		Vector<RD::Uniform> uniforms;
		{
			RD::Uniform u;
			u.binding = 1;
			u.uniform_type = RD::UNIFORM_TYPE_UNIFORM_BUFFER;
			u.append_id(cascades_ubo);
			uniforms.push_back(u);
		}
		{
			RD::Uniform u;
			u.binding = 2;
			u.uniform_type = RD::UNIFORM_TYPE_TEXTURE;
			u.append_id(lightprobe_texture);
			uniforms.push_back(u);
		}
		{
			RD::Uniform u;
			u.binding = 3;
			u.uniform_type = RD::UNIFORM_TYPE_SAMPLER;
			u.append_id(material_storage->sampler_rd_get_default(RS::CANVAS_ITEM_TEXTURE_FILTER_LINEAR, RS::CANVAS_ITEM_TEXTURE_REPEAT_DISABLED));
			uniforms.push_back(u);
		}
		{
			RD::Uniform u;
			u.binding = 4;
			u.uniform_type = RD::UNIFORM_TYPE_TEXTURE;
			u.append_id(occlusion_texture);
			uniforms.push_back(u);
		}
		{
			RD::Uniform u;
			u.binding = 5;
			u.uniform_type = RD::UNIFORM_TYPE_UNIFORM_BUFFER;
			u.append_id(debug_probes_scene_data_ubo);
			uniforms.push_back(u);
		}

		debug_probes_uniform_set = RD::get_singleton()->uniform_set_create(uniforms, gi->sdfgi_shader.debug_probes.version_get_shader(gi->sdfgi_shader.debug_probes_shader, 0), 0);
	}

	SDFGIShader::ProbeDebugMode mode = p_view_count > 1 ? SDFGIShader::PROBE_DEBUG_PROBES_MULTIVIEW : SDFGIShader::PROBE_DEBUG_PROBES;

	RD::DrawListID draw_list = RD::get_singleton()->draw_list_begin(p_framebuffer);
	RD::get_singleton()->draw_command_begin_label("Debug SDFGI");

	RD::get_singleton()->draw_list_bind_render_pipeline(draw_list, gi->sdfgi_shader.debug_probes_pipeline[mode].get_render_pipeline(RD::INVALID_FORMAT_ID, RD::get_singleton()->framebuffer_get_format(p_framebuffer)));
	RD::get_singleton()->draw_list_bind_uniform_set(draw_list, debug_probes_uniform_set, 0);
	RD::get_singleton()->draw_list_set_push_constant(draw_list, &push_constant, sizeof(SDFGIShader::DebugProbesPushConstant));
	RD::get_singleton()->draw_list_draw(draw_list, false, total_probes, total_points);

	if (gi->sdfgi_debug_probe_dir != Vector3()) {
		uint32_t cascade = 0;
		Vector3 offset = Vector3((Vector3i(1, 1, 1) * -int32_t(cascade_size >> 1) + cascades[cascade].position)) * cascades[cascade].cell_size * Vector3(1.0, 1.0 / y_mult, 1.0);
		Vector3 probe_size = cascades[cascade].cell_size * (cascade_size / SDFGI::PROBE_DIVISOR) * Vector3(1.0, 1.0 / y_mult, 1.0);
		Vector3 ray_from = gi->sdfgi_debug_probe_pos;
		Vector3 ray_to = gi->sdfgi_debug_probe_pos + gi->sdfgi_debug_probe_dir * cascades[cascade].cell_size * Math::sqrt(3.0) * cascade_size;
		float sphere_radius = 0.2;
		float closest_dist = 1e20;
		gi->sdfgi_debug_probe_enabled = false;

		Vector3i probe_from = cascades[cascade].position / (cascade_size / SDFGI::PROBE_DIVISOR);
		for (int i = 0; i < (SDFGI::PROBE_DIVISOR + 1); i++) {
			for (int j = 0; j < (SDFGI::PROBE_DIVISOR + 1); j++) {
				for (int k = 0; k < (SDFGI::PROBE_DIVISOR + 1); k++) {
					Vector3 pos = offset + probe_size * Vector3(i, j, k);
					Vector3 res;
					if (Geometry3D::segment_intersects_sphere(ray_from, ray_to, pos, sphere_radius, &res)) {
						float d = ray_from.distance_to(res);
						if (d < closest_dist) {
							closest_dist = d;
							gi->sdfgi_debug_probe_enabled = true;
							gi->sdfgi_debug_probe_index = probe_from + Vector3i(i, j, k);
						}
					}
				}
			}
		}

		gi->sdfgi_debug_probe_dir = Vector3();
	}

	if (gi->sdfgi_debug_probe_enabled) {
		uint32_t cascade = 0;
		uint32_t probe_cells = (cascade_size / SDFGI::PROBE_DIVISOR);
		Vector3i probe_from = cascades[cascade].position / probe_cells;
		Vector3i ofs = gi->sdfgi_debug_probe_index - probe_from;
		if (ofs.x < 0 || ofs.y < 0 || ofs.z < 0) {
			return;
		}
		if (ofs.x > SDFGI::PROBE_DIVISOR || ofs.y > SDFGI::PROBE_DIVISOR || ofs.z > SDFGI::PROBE_DIVISOR) {
			return;
		}

		uint32_t mult = (SDFGI::PROBE_DIVISOR + 1);
		uint32_t index = ofs.z * mult * mult + ofs.y * mult + ofs.x;

		push_constant.probe_debug_index = index;

		uint32_t cell_count = probe_cells * 2 * probe_cells * 2 * probe_cells * 2;

		RD::get_singleton()->draw_list_bind_render_pipeline(draw_list, gi->sdfgi_shader.debug_probes_pipeline[p_view_count > 1 ? SDFGIShader::PROBE_DEBUG_VISIBILITY_MULTIVIEW : SDFGIShader::PROBE_DEBUG_VISIBILITY].get_render_pipeline(RD::INVALID_FORMAT_ID, RD::get_singleton()->framebuffer_get_format(p_framebuffer)));
		RD::get_singleton()->draw_list_bind_uniform_set(draw_list, debug_probes_uniform_set, 0);
		RD::get_singleton()->draw_list_set_push_constant(draw_list, &push_constant, sizeof(SDFGIShader::DebugProbesPushConstant));
		RD::get_singleton()->draw_list_draw(draw_list, false, cell_count, total_points);
	}

	RD::get_singleton()->draw_command_end_label();
	RD::get_singleton()->draw_list_end();
}

void GI::SDFGI::pre_process_gi(const Transform3D &p_transform, RenderDataRD *p_render_data) {
	if (p_render_data->sdfgi_update_data == nullptr) {
		return;
	}

	RendererRD::LightStorage *light_storage = RendererRD::LightStorage::get_singleton();
	/* Update general SDFGI Buffer */

	SDFGIData sdfgi_data;

	sdfgi_data.grid_size[0] = cascade_size;
	sdfgi_data.grid_size[1] = cascade_size;
	sdfgi_data.grid_size[2] = cascade_size;

	sdfgi_data.max_cascades = cascades.size();
	sdfgi_data.probe_axis_size = probe_axis_count;
	sdfgi_data.cascade_probe_size[0] = sdfgi_data.probe_axis_size - 1; //float version for performance
	sdfgi_data.cascade_probe_size[1] = sdfgi_data.probe_axis_size - 1;
	sdfgi_data.cascade_probe_size[2] = sdfgi_data.probe_axis_size - 1;

	float csize = cascade_size;
	sdfgi_data.probe_to_uvw = 1.0 / float(sdfgi_data.cascade_probe_size[0]);
	sdfgi_data.use_occlusion = uses_occlusion;
	//sdfgi_data.energy = energy;

	sdfgi_data.y_mult = y_mult;

	float cascade_voxel_size = (csize / sdfgi_data.cascade_probe_size[0]);
	float occlusion_clamp = (cascade_voxel_size - 0.5) / cascade_voxel_size;
	sdfgi_data.occlusion_clamp[0] = occlusion_clamp;
	sdfgi_data.occlusion_clamp[1] = occlusion_clamp;
	sdfgi_data.occlusion_clamp[2] = occlusion_clamp;
	sdfgi_data.normal_bias = (normal_bias / csize) * sdfgi_data.cascade_probe_size[0];

	//vec2 tex_pixel_size = 1.0 / vec2(ivec2( (OCT_SIZE+2) * params.probe_axis_size * params.probe_axis_size, (OCT_SIZE+2) * params.probe_axis_size ) );
	//vec3 probe_uv_offset = (ivec3(OCT_SIZE+2,OCT_SIZE+2,(OCT_SIZE+2) * params.probe_axis_size)) * tex_pixel_size.xyx;

	uint32_t oct_size = SDFGI::LIGHTPROBE_OCT_SIZE;

	sdfgi_data.lightprobe_tex_pixel_size[0] = 1.0 / ((oct_size + 2) * sdfgi_data.probe_axis_size * sdfgi_data.probe_axis_size);
	sdfgi_data.lightprobe_tex_pixel_size[1] = 1.0 / ((oct_size + 2) * sdfgi_data.probe_axis_size);
	sdfgi_data.lightprobe_tex_pixel_size[2] = 1.0;

	sdfgi_data.energy = energy;

	sdfgi_data.lightprobe_uv_offset[0] = float(oct_size + 2) * sdfgi_data.lightprobe_tex_pixel_size[0];
	sdfgi_data.lightprobe_uv_offset[1] = float(oct_size + 2) * sdfgi_data.lightprobe_tex_pixel_size[1];
	sdfgi_data.lightprobe_uv_offset[2] = float((oct_size + 2) * sdfgi_data.probe_axis_size) * sdfgi_data.lightprobe_tex_pixel_size[0];

	sdfgi_data.occlusion_renormalize[0] = 0.5;
	sdfgi_data.occlusion_renormalize[1] = 1.0;
	sdfgi_data.occlusion_renormalize[2] = 1.0 / float(sdfgi_data.max_cascades);

	int32_t probe_divisor = cascade_size / SDFGI::PROBE_DIVISOR;

	for (uint32_t i = 0; i < sdfgi_data.max_cascades; i++) {
		SDFGIData::ProbeCascadeData &c = sdfgi_data.cascades[i];
		Vector3 pos = Vector3((Vector3i(1, 1, 1) * -int32_t(cascade_size >> 1) + cascades[i].position)) * cascades[i].cell_size;
		Vector3 cam_origin = p_transform.origin;
		cam_origin.y *= y_mult;
		pos -= cam_origin; //make pos local to camera, to reduce numerical error
		c.position[0] = pos.x;
		c.position[1] = pos.y;
		c.position[2] = pos.z;
		c.to_probe = 1.0 / (float(cascade_size) * cascades[i].cell_size / float(probe_axis_count - 1));

		Vector3i probe_ofs = cascades[i].position / probe_divisor;
		c.probe_world_offset[0] = probe_ofs.x;
		c.probe_world_offset[1] = probe_ofs.y;
		c.probe_world_offset[2] = probe_ofs.z;

		c.to_cell = 1.0 / cascades[i].cell_size;
		c.exposure_normalization = 1.0;
		if (p_render_data->camera_attributes.is_valid()) {
			float exposure_normalization = RSG::camera_attributes->camera_attributes_get_exposure_normalization_factor(p_render_data->camera_attributes);
			c.exposure_normalization = exposure_normalization / cascades[i].baked_exposure_normalization;
		}
	}

	RD::get_singleton()->buffer_update(gi->sdfgi_ubo, 0, sizeof(SDFGIData), &sdfgi_data);

	/* Update dynamic lights in SDFGI cascades */

	for (uint32_t i = 0; i < cascades.size(); i++) {
		SDFGI::Cascade &cascade = cascades[i];

		SDFGIShader::Light lights[SDFGI::MAX_DYNAMIC_LIGHTS];
		uint32_t idx = 0;
		for (uint32_t j = 0; j < (uint32_t)p_render_data->sdfgi_update_data->directional_lights->size(); j++) {
			if (idx == SDFGI::MAX_DYNAMIC_LIGHTS) {
				break;
			}

			RID light_instance = p_render_data->sdfgi_update_data->directional_lights->get(j);
			ERR_CONTINUE(!light_storage->owns_light_instance(light_instance));

			RID light = light_storage->light_instance_get_base_light(light_instance);
			Transform3D light_transform = light_storage->light_instance_get_base_transform(light_instance);

			if (RSG::light_storage->light_directional_get_sky_mode(light) == RS::LIGHT_DIRECTIONAL_SKY_MODE_SKY_ONLY) {
				continue;
			}

			Vector3 dir = -light_transform.basis.get_column(Vector3::AXIS_Z);
			dir.y *= y_mult;
			dir.normalize();
			lights[idx].direction[0] = dir.x;
			lights[idx].direction[1] = dir.y;
			lights[idx].direction[2] = dir.z;
			Color color = RSG::light_storage->light_get_color(light);
			color = color.srgb_to_linear();
			lights[idx].color[0] = color.r;
			lights[idx].color[1] = color.g;
			lights[idx].color[2] = color.b;
			lights[idx].type = RS::LIGHT_DIRECTIONAL;
			lights[idx].energy = RSG::light_storage->light_get_param(light, RS::LIGHT_PARAM_ENERGY) * RSG::light_storage->light_get_param(light, RS::LIGHT_PARAM_INDIRECT_ENERGY);
			if (RendererSceneRenderRD::get_singleton()->is_using_physical_light_units()) {
				lights[idx].energy *= RSG::light_storage->light_get_param(light, RS::LIGHT_PARAM_INTENSITY);
			}

			if (p_render_data->camera_attributes.is_valid()) {
				lights[idx].energy *= RSG::camera_attributes->camera_attributes_get_exposure_normalization_factor(p_render_data->camera_attributes);
			}

			lights[idx].has_shadow = RSG::light_storage->light_has_shadow(light);

			idx++;
		}

		AABB cascade_aabb;
		cascade_aabb.position = Vector3((Vector3i(1, 1, 1) * -int32_t(cascade_size >> 1) + cascade.position)) * cascade.cell_size;
		cascade_aabb.size = Vector3(1, 1, 1) * cascade_size * cascade.cell_size;

		for (uint32_t j = 0; j < p_render_data->sdfgi_update_data->positional_light_count; j++) {
			if (idx == SDFGI::MAX_DYNAMIC_LIGHTS) {
				break;
			}

			RID light_instance = p_render_data->sdfgi_update_data->positional_light_instances[j];
			ERR_CONTINUE(!light_storage->owns_light_instance(light_instance));

			RID light = light_storage->light_instance_get_base_light(light_instance);
			AABB light_aabb = light_storage->light_instance_get_base_aabb(light_instance);
			Transform3D light_transform = light_storage->light_instance_get_base_transform(light_instance);

			uint32_t max_sdfgi_cascade = RSG::light_storage->light_get_max_sdfgi_cascade(light);
			if (i > max_sdfgi_cascade) {
				continue;
			}

			if (!cascade_aabb.intersects(light_aabb)) {
				continue;
			}

			Vector3 dir = -light_transform.basis.get_column(Vector3::AXIS_Z);
			//faster to not do this here
			//dir.y *= y_mult;
			//dir.normalize();
			lights[idx].direction[0] = dir.x;
			lights[idx].direction[1] = dir.y;
			lights[idx].direction[2] = dir.z;
			Vector3 pos = light_transform.origin;
			pos.y *= y_mult;
			lights[idx].position[0] = pos.x;
			lights[idx].position[1] = pos.y;
			lights[idx].position[2] = pos.z;
			Color color = RSG::light_storage->light_get_color(light);
			color = color.srgb_to_linear();
			lights[idx].color[0] = color.r;
			lights[idx].color[1] = color.g;
			lights[idx].color[2] = color.b;
			lights[idx].type = RSG::light_storage->light_get_type(light);

			lights[idx].energy = RSG::light_storage->light_get_param(light, RS::LIGHT_PARAM_ENERGY) * RSG::light_storage->light_get_param(light, RS::LIGHT_PARAM_INDIRECT_ENERGY);
			if (RendererSceneRenderRD::get_singleton()->is_using_physical_light_units()) {
				lights[idx].energy *= RSG::light_storage->light_get_param(light, RS::LIGHT_PARAM_INTENSITY);

				// Convert from Luminous Power to Luminous Intensity
				if (lights[idx].type == RS::LIGHT_OMNI) {
					lights[idx].energy *= 1.0 / (Math_PI * 4.0);
				} else if (lights[idx].type == RS::LIGHT_SPOT) {
					// Spot Lights are not physically accurate, Luminous Intensity should change in relation to the cone angle.
					// We make this assumption to keep them easy to control.
					lights[idx].energy *= 1.0 / Math_PI;
				}
			}

			if (p_render_data->camera_attributes.is_valid()) {
				lights[idx].energy *= RSG::camera_attributes->camera_attributes_get_exposure_normalization_factor(p_render_data->camera_attributes);
			}

			lights[idx].has_shadow = RSG::light_storage->light_has_shadow(light);
			lights[idx].attenuation = RSG::light_storage->light_get_param(light, RS::LIGHT_PARAM_ATTENUATION);
			lights[idx].radius = RSG::light_storage->light_get_param(light, RS::LIGHT_PARAM_RANGE);
			lights[idx].cos_spot_angle = Math::cos(Math::deg_to_rad(RSG::light_storage->light_get_param(light, RS::LIGHT_PARAM_SPOT_ANGLE)));
			lights[idx].inv_spot_attenuation = 1.0f / RSG::light_storage->light_get_param(light, RS::LIGHT_PARAM_SPOT_ATTENUATION);

			idx++;
		}

		if (idx > 0) {
			RD::get_singleton()->buffer_update(cascade.lights_buffer, 0, idx * sizeof(SDFGIShader::Light), lights);
		}

		cascade_dynamic_light_count[i] = idx;
	}
}

void GI::SDFGI::render_region(Ref<RenderSceneBuffersRD> p_render_buffers, int p_region, const PagedArray<RenderGeometryInstance *> &p_instances, float p_exposure_normalization) {
	//print_line("rendering region " + itos(p_region));
	ERR_FAIL_COND(p_render_buffers.is_null()); // we wouldn't be here if this failed but...
	AABB bounds;
	Vector3i from;
	Vector3i size;

	int cascade_prev = get_pending_region_data(p_region - 1, from, size, bounds);
	int cascade_next = get_pending_region_data(p_region + 1, from, size, bounds);
	int cascade = get_pending_region_data(p_region, from, size, bounds);
	ERR_FAIL_COND(cascade < 0);

	if (cascade_prev != cascade) {
		//initialize render
		RD::get_singleton()->texture_clear(render_albedo, Color(0, 0, 0, 0), 0, 1, 0, 1);
		RD::get_singleton()->texture_clear(render_emission, Color(0, 0, 0, 0), 0, 1, 0, 1);
		RD::get_singleton()->texture_clear(render_emission_aniso, Color(0, 0, 0, 0), 0, 1, 0, 1);
		RD::get_singleton()->texture_clear(render_geom_facing, Color(0, 0, 0, 0), 0, 1, 0, 1);
	}

	//print_line("rendering cascade " + itos(p_region) + " objects: " + itos(p_cull_count) + " bounds: " + bounds + " from: " + from + " size: " + size + " cell size: " + rtos(cascades[cascade].cell_size));
	RendererSceneRenderRD::get_singleton()->_render_sdfgi(p_render_buffers, from, size, bounds, p_instances, render_albedo, render_emission, render_emission_aniso, render_geom_facing, p_exposure_normalization);

	if (cascade_next != cascade) {
		RD::get_singleton()->draw_command_begin_label("SDFGI Pre-Process Cascade");

		RENDER_TIMESTAMP("> SDFGI Update SDF");
		//done rendering! must update SDF
		//clear dispatch indirect data

		SDFGIShader::PreprocessPushConstant push_constant;
		memset(&push_constant, 0, sizeof(SDFGIShader::PreprocessPushConstant));

		RENDER_TIMESTAMP("SDFGI Scroll SDF");

		//scroll
		if (cascades[cascade].dirty_regions != SDFGI::Cascade::DIRTY_ALL) {
			//for scroll
			Vector3i dirty = cascades[cascade].dirty_regions;
			push_constant.scroll[0] = dirty.x;
			push_constant.scroll[1] = dirty.y;
			push_constant.scroll[2] = dirty.z;
		} else {
			//for no scroll
			push_constant.scroll[0] = 0;
			push_constant.scroll[1] = 0;
			push_constant.scroll[2] = 0;
		}

		cascades[cascade].all_dynamic_lights_dirty = true;
		cascades[cascade].baked_exposure_normalization = p_exposure_normalization;

		push_constant.grid_size = cascade_size;
		push_constant.cascade = cascade;

		if (cascades[cascade].dirty_regions != SDFGI::Cascade::DIRTY_ALL) {
			RD::get_singleton()->buffer_copy(cascades[cascade].solid_cell_dispatch_buffer_storage, cascades[cascade].solid_cell_dispatch_buffer_call, 0, 0, sizeof(uint32_t) * 4);

			RD::ComputeListID compute_list = RD::get_singleton()->compute_list_begin();

			//must pre scroll existing data because not all is dirty
			RD::get_singleton()->compute_list_bind_compute_pipeline(compute_list, gi->sdfgi_shader.preprocess_pipeline[SDFGIShader::PRE_PROCESS_SCROLL]);
			RD::get_singleton()->compute_list_bind_uniform_set(compute_list, cascades[cascade].scroll_uniform_set, 0);

			RD::get_singleton()->compute_list_set_push_constant(compute_list, &push_constant, sizeof(SDFGIShader::PreprocessPushConstant));
			RD::get_singleton()->compute_list_dispatch_indirect(compute_list, cascades[cascade].solid_cell_dispatch_buffer_call, 0);
			// no barrier do all together

			RD::get_singleton()->compute_list_bind_compute_pipeline(compute_list, gi->sdfgi_shader.preprocess_pipeline[SDFGIShader::PRE_PROCESS_SCROLL_OCCLUSION]);
			RD::get_singleton()->compute_list_bind_uniform_set(compute_list, cascades[cascade].scroll_occlusion_uniform_set, 0);

			Vector3i dirty = cascades[cascade].dirty_regions;
			Vector3i groups;
			groups.x = cascade_size - ABS(dirty.x);
			groups.y = cascade_size - ABS(dirty.y);
			groups.z = cascade_size - ABS(dirty.z);

			RD::get_singleton()->compute_list_set_push_constant(compute_list, &push_constant, sizeof(SDFGIShader::PreprocessPushConstant));
			RD::get_singleton()->compute_list_dispatch_threads(compute_list, groups.x, groups.y, groups.z);

			//no barrier, continue together

			{
				//scroll probes and their history also

				SDFGIShader::IntegratePushConstant ipush_constant;
				ipush_constant.grid_size[1] = cascade_size;
				ipush_constant.grid_size[2] = cascade_size;
				ipush_constant.grid_size[0] = cascade_size;
				ipush_constant.max_cascades = cascades.size();
				ipush_constant.probe_axis_size = probe_axis_count;
				ipush_constant.history_index = 0;
				ipush_constant.history_size = history_size;
				ipush_constant.ray_count = 0;
				ipush_constant.ray_bias = 0;
				ipush_constant.sky_flags = 0;
				ipush_constant.sky_energy = 0;
				ipush_constant.sky_color_or_orientation[0] = 0;
				ipush_constant.sky_color_or_orientation[1] = 0;
				ipush_constant.sky_color_or_orientation[2] = 0;
				ipush_constant.y_mult = y_mult;
				ipush_constant.store_ambient_texture = false;

				ipush_constant.image_size[0] = probe_axis_count * probe_axis_count;
				ipush_constant.image_size[1] = probe_axis_count;

				int32_t probe_divisor = cascade_size / SDFGI::PROBE_DIVISOR;
				ipush_constant.cascade = cascade;
				ipush_constant.world_offset[0] = cascades[cascade].position.x / probe_divisor;
				ipush_constant.world_offset[1] = cascades[cascade].position.y / probe_divisor;
				ipush_constant.world_offset[2] = cascades[cascade].position.z / probe_divisor;

				ipush_constant.scroll[0] = dirty.x / probe_divisor;
				ipush_constant.scroll[1] = dirty.y / probe_divisor;
				ipush_constant.scroll[2] = dirty.z / probe_divisor;

				RD::get_singleton()->compute_list_bind_compute_pipeline(compute_list, gi->sdfgi_shader.integrate_pipeline[SDFGIShader::INTEGRATE_MODE_SCROLL]);
				RD::get_singleton()->compute_list_bind_uniform_set(compute_list, cascades[cascade].integrate_uniform_set, 0);
				RD::get_singleton()->compute_list_bind_uniform_set(compute_list, gi->sdfgi_shader.integrate_default_sky_uniform_set, 1);
				RD::get_singleton()->compute_list_set_push_constant(compute_list, &ipush_constant, sizeof(SDFGIShader::IntegratePushConstant));
				RD::get_singleton()->compute_list_dispatch_threads(compute_list, probe_axis_count * probe_axis_count, probe_axis_count, 1);

				RD::get_singleton()->compute_list_add_barrier(compute_list);

				RD::get_singleton()->compute_list_bind_compute_pipeline(compute_list, gi->sdfgi_shader.integrate_pipeline[SDFGIShader::INTEGRATE_MODE_SCROLL_STORE]);
				RD::get_singleton()->compute_list_bind_uniform_set(compute_list, cascades[cascade].integrate_uniform_set, 0);
				RD::get_singleton()->compute_list_bind_uniform_set(compute_list, gi->sdfgi_shader.integrate_default_sky_uniform_set, 1);
				RD::get_singleton()->compute_list_set_push_constant(compute_list, &ipush_constant, sizeof(SDFGIShader::IntegratePushConstant));
				RD::get_singleton()->compute_list_dispatch_threads(compute_list, probe_axis_count * probe_axis_count, probe_axis_count, 1);

				RD::get_singleton()->compute_list_add_barrier(compute_list);

				if (bounce_feedback > 0.0) {
					//multibounce requires this to be stored so direct light can read from it

					RD::get_singleton()->compute_list_bind_compute_pipeline(compute_list, gi->sdfgi_shader.integrate_pipeline[SDFGIShader::INTEGRATE_MODE_STORE]);

					//convert to octahedral to store
					ipush_constant.image_size[0] *= SDFGI::LIGHTPROBE_OCT_SIZE;
					ipush_constant.image_size[1] *= SDFGI::LIGHTPROBE_OCT_SIZE;

					RD::get_singleton()->compute_list_bind_uniform_set(compute_list, cascades[cascade].integrate_uniform_set, 0);
					RD::get_singleton()->compute_list_bind_uniform_set(compute_list, gi->sdfgi_shader.integrate_default_sky_uniform_set, 1);
					RD::get_singleton()->compute_list_set_push_constant(compute_list, &ipush_constant, sizeof(SDFGIShader::IntegratePushConstant));
					RD::get_singleton()->compute_list_dispatch_threads(compute_list, probe_axis_count * probe_axis_count * SDFGI::LIGHTPROBE_OCT_SIZE, probe_axis_count * SDFGI::LIGHTPROBE_OCT_SIZE, 1);
				}
			}

			//ok finally barrier
			RD::get_singleton()->compute_list_end();
		}

		//clear dispatch indirect data
		uint32_t dispatch_indirct_data[4] = { 0, 0, 0, 0 };
		RD::get_singleton()->buffer_update(cascades[cascade].solid_cell_dispatch_buffer_storage, 0, sizeof(uint32_t) * 4, dispatch_indirct_data);

		RD::ComputeListID compute_list = RD::get_singleton()->compute_list_begin();

		bool half_size = true; //much faster, very little difference
		static const int optimized_jf_group_size = 8;

		if (half_size) {
			push_constant.grid_size >>= 1;

			uint32_t cascade_half_size = cascade_size >> 1;
			RD::get_singleton()->compute_list_bind_compute_pipeline(compute_list, gi->sdfgi_shader.preprocess_pipeline[SDFGIShader::PRE_PROCESS_JUMP_FLOOD_INITIALIZE_HALF]);
			RD::get_singleton()->compute_list_bind_uniform_set(compute_list, sdf_initialize_half_uniform_set, 0);
			RD::get_singleton()->compute_list_set_push_constant(compute_list, &push_constant, sizeof(SDFGIShader::PreprocessPushConstant));
			RD::get_singleton()->compute_list_dispatch_threads(compute_list, cascade_half_size, cascade_half_size, cascade_half_size);
			RD::get_singleton()->compute_list_add_barrier(compute_list);

			//must start with regular jumpflood

			push_constant.half_size = true;
			{
				RENDER_TIMESTAMP("SDFGI Jump Flood (Half-Size)");

				uint32_t s = cascade_half_size;

				RD::get_singleton()->compute_list_bind_compute_pipeline(compute_list, gi->sdfgi_shader.preprocess_pipeline[SDFGIShader::PRE_PROCESS_JUMP_FLOOD]);

				int jf_us = 0;
				//start with regular jump flood for very coarse reads, as this is impossible to optimize
				while (s > 1) {
					s /= 2;
					push_constant.step_size = s;
					RD::get_singleton()->compute_list_bind_uniform_set(compute_list, jump_flood_half_uniform_set[jf_us], 0);
					RD::get_singleton()->compute_list_set_push_constant(compute_list, &push_constant, sizeof(SDFGIShader::PreprocessPushConstant));
					RD::get_singleton()->compute_list_dispatch_threads(compute_list, cascade_half_size, cascade_half_size, cascade_half_size);
					RD::get_singleton()->compute_list_add_barrier(compute_list);
					jf_us = jf_us == 0 ? 1 : 0;

					if (cascade_half_size / (s / 2) >= optimized_jf_group_size) {
						break;
					}
				}

				RENDER_TIMESTAMP("SDFGI Jump Flood Optimized (Half-Size)");

				//continue with optimized jump flood for smaller reads
				RD::get_singleton()->compute_list_bind_compute_pipeline(compute_list, gi->sdfgi_shader.preprocess_pipeline[SDFGIShader::PRE_PROCESS_JUMP_FLOOD_OPTIMIZED]);
				while (s > 1) {
					s /= 2;
					push_constant.step_size = s;
					RD::get_singleton()->compute_list_bind_uniform_set(compute_list, jump_flood_half_uniform_set[jf_us], 0);
					RD::get_singleton()->compute_list_set_push_constant(compute_list, &push_constant, sizeof(SDFGIShader::PreprocessPushConstant));
					RD::get_singleton()->compute_list_dispatch_threads(compute_list, cascade_half_size, cascade_half_size, cascade_half_size);
					RD::get_singleton()->compute_list_add_barrier(compute_list);
					jf_us = jf_us == 0 ? 1 : 0;
				}
			}

			// restore grid size for last passes
			push_constant.grid_size = cascade_size;

			RD::get_singleton()->compute_list_bind_compute_pipeline(compute_list, gi->sdfgi_shader.preprocess_pipeline[SDFGIShader::PRE_PROCESS_JUMP_FLOOD_UPSCALE]);
			RD::get_singleton()->compute_list_bind_uniform_set(compute_list, sdf_upscale_uniform_set, 0);
			RD::get_singleton()->compute_list_set_push_constant(compute_list, &push_constant, sizeof(SDFGIShader::PreprocessPushConstant));
			RD::get_singleton()->compute_list_dispatch_threads(compute_list, cascade_size, cascade_size, cascade_size);
			RD::get_singleton()->compute_list_add_barrier(compute_list);

			//run one pass of fullsize jumpflood to fix up half size artifacts

			push_constant.half_size = false;
			push_constant.step_size = 1;
			RD::get_singleton()->compute_list_bind_compute_pipeline(compute_list, gi->sdfgi_shader.preprocess_pipeline[SDFGIShader::PRE_PROCESS_JUMP_FLOOD_OPTIMIZED]);
			RD::get_singleton()->compute_list_bind_uniform_set(compute_list, jump_flood_uniform_set[upscale_jfa_uniform_set_index], 0);
			RD::get_singleton()->compute_list_set_push_constant(compute_list, &push_constant, sizeof(SDFGIShader::PreprocessPushConstant));
			RD::get_singleton()->compute_list_dispatch_threads(compute_list, cascade_size, cascade_size, cascade_size);
			RD::get_singleton()->compute_list_add_barrier(compute_list);

		} else {
			//full size jumpflood
			RENDER_TIMESTAMP("SDFGI Jump Flood (Full-Size)");

			RD::get_singleton()->compute_list_bind_compute_pipeline(compute_list, gi->sdfgi_shader.preprocess_pipeline[SDFGIShader::PRE_PROCESS_JUMP_FLOOD_INITIALIZE]);
			RD::get_singleton()->compute_list_bind_uniform_set(compute_list, sdf_initialize_uniform_set, 0);
			RD::get_singleton()->compute_list_set_push_constant(compute_list, &push_constant, sizeof(SDFGIShader::PreprocessPushConstant));
			RD::get_singleton()->compute_list_dispatch_threads(compute_list, cascade_size, cascade_size, cascade_size);

			RD::get_singleton()->compute_list_add_barrier(compute_list);

			push_constant.half_size = false;
			{
				uint32_t s = cascade_size;

				RD::get_singleton()->compute_list_bind_compute_pipeline(compute_list, gi->sdfgi_shader.preprocess_pipeline[SDFGIShader::PRE_PROCESS_JUMP_FLOOD]);

				int jf_us = 0;
				//start with regular jump flood for very coarse reads, as this is impossible to optimize
				while (s > 1) {
					s /= 2;
					push_constant.step_size = s;
					RD::get_singleton()->compute_list_bind_uniform_set(compute_list, jump_flood_uniform_set[jf_us], 0);
					RD::get_singleton()->compute_list_set_push_constant(compute_list, &push_constant, sizeof(SDFGIShader::PreprocessPushConstant));
					RD::get_singleton()->compute_list_dispatch_threads(compute_list, cascade_size, cascade_size, cascade_size);
					RD::get_singleton()->compute_list_add_barrier(compute_list);
					jf_us = jf_us == 0 ? 1 : 0;

					if (cascade_size / (s / 2) >= optimized_jf_group_size) {
						break;
					}
				}

				RENDER_TIMESTAMP("SDFGI Jump Flood Optimized (Full-Size)");

				//continue with optimized jump flood for smaller reads
				RD::get_singleton()->compute_list_bind_compute_pipeline(compute_list, gi->sdfgi_shader.preprocess_pipeline[SDFGIShader::PRE_PROCESS_JUMP_FLOOD_OPTIMIZED]);
				while (s > 1) {
					s /= 2;
					push_constant.step_size = s;
					RD::get_singleton()->compute_list_bind_uniform_set(compute_list, jump_flood_uniform_set[jf_us], 0);
					RD::get_singleton()->compute_list_set_push_constant(compute_list, &push_constant, sizeof(SDFGIShader::PreprocessPushConstant));
					RD::get_singleton()->compute_list_dispatch_threads(compute_list, cascade_size, cascade_size, cascade_size);
					RD::get_singleton()->compute_list_add_barrier(compute_list);
					jf_us = jf_us == 0 ? 1 : 0;
				}
			}
		}

		RENDER_TIMESTAMP("SDFGI Occlusion");

		// occlusion
		{
			uint32_t probe_size = cascade_size / SDFGI::PROBE_DIVISOR;
			Vector3i probe_global_pos = cascades[cascade].position / probe_size;

			RD::get_singleton()->compute_list_bind_compute_pipeline(compute_list, gi->sdfgi_shader.preprocess_pipeline[SDFGIShader::PRE_PROCESS_OCCLUSION]);
			RD::get_singleton()->compute_list_bind_uniform_set(compute_list, occlusion_uniform_set, 0);
			for (int i = 0; i < 8; i++) {
				//dispatch all at once for performance
				Vector3i offset(i & 1, (i >> 1) & 1, (i >> 2) & 1);

				if ((probe_global_pos.x & 1) != 0) {
					offset.x = (offset.x + 1) & 1;
				}
				if ((probe_global_pos.y & 1) != 0) {
					offset.y = (offset.y + 1) & 1;
				}
				if ((probe_global_pos.z & 1) != 0) {
					offset.z = (offset.z + 1) & 1;
				}
				push_constant.probe_offset[0] = offset.x;
				push_constant.probe_offset[1] = offset.y;
				push_constant.probe_offset[2] = offset.z;
				push_constant.occlusion_index = i;
				RD::get_singleton()->compute_list_set_push_constant(compute_list, &push_constant, sizeof(SDFGIShader::PreprocessPushConstant));

				Vector3i groups = Vector3i(probe_size + 1, probe_size + 1, probe_size + 1) - offset; //if offset, it's one less probe per axis to compute
				RD::get_singleton()->compute_list_dispatch(compute_list, groups.x, groups.y, groups.z);
			}
			RD::get_singleton()->compute_list_add_barrier(compute_list);
		}

		RENDER_TIMESTAMP("SDFGI Store");

		// store
		RD::get_singleton()->compute_list_bind_compute_pipeline(compute_list, gi->sdfgi_shader.preprocess_pipeline[SDFGIShader::PRE_PROCESS_STORE]);
		RD::get_singleton()->compute_list_bind_uniform_set(compute_list, cascades[cascade].sdf_store_uniform_set, 0);
		RD::get_singleton()->compute_list_set_push_constant(compute_list, &push_constant, sizeof(SDFGIShader::PreprocessPushConstant));
		RD::get_singleton()->compute_list_dispatch_threads(compute_list, cascade_size, cascade_size, cascade_size);

		RD::get_singleton()->compute_list_end();

		//clear these textures, as they will have previous garbage on next draw
		RD::get_singleton()->texture_clear(cascades[cascade].light_tex, Color(0, 0, 0, 0), 0, 1, 0, 1);
		RD::get_singleton()->texture_clear(cascades[cascade].light_aniso_0_tex, Color(0, 0, 0, 0), 0, 1, 0, 1);
		RD::get_singleton()->texture_clear(cascades[cascade].light_aniso_1_tex, Color(0, 0, 0, 0), 0, 1, 0, 1);

#if 0
		Vector<uint8_t> data = RD::get_singleton()->texture_get_data(cascades[cascade].sdf, 0);
		Ref<Image> img;
		img.instantiate();
		for (uint32_t i = 0; i < cascade_size; i++) {
			Vector<uint8_t> subarr = data.slice(128 * 128 * i, 128 * 128 * (i + 1));
			img->set_data(cascade_size, cascade_size, false, Image::FORMAT_L8, subarr);
			img->save_png("res://cascade_sdf_" + itos(cascade) + "_" + itos(i) + ".png");
		}

		//finalize render and update sdf
#endif

#if 0
		Vector<uint8_t> data = RD::get_singleton()->texture_get_data(render_albedo, 0);
		Ref<Image> img;
		img.instantiate();
		for (uint32_t i = 0; i < cascade_size; i++) {
			Vector<uint8_t> subarr = data.slice(128 * 128 * i * 2, 128 * 128 * (i + 1) * 2);
			img->createcascade_size, cascade_size, false, Image::FORMAT_RGB565, subarr);
			img->convert(Image::FORMAT_RGBA8);
			img->save_png("res://cascade_" + itos(cascade) + "_" + itos(i) + ".png");
		}

		//finalize render and update sdf
#endif

		RENDER_TIMESTAMP("< SDFGI Update SDF");
		RD::get_singleton()->draw_command_end_label();
	}
}

void GI::SDFGI::render_static_lights(RenderDataRD *p_render_data, Ref<RenderSceneBuffersRD> p_render_buffers, uint32_t p_cascade_count, const uint32_t *p_cascade_indices, const PagedArray<RID> *p_positional_light_cull_result) {
	ERR_FAIL_COND(p_render_buffers.is_null()); // we wouldn't be here if this failed but...

	RendererRD::LightStorage *light_storage = RendererRD::LightStorage::get_singleton();

	RD::get_singleton()->draw_command_begin_label("SDFGI Render Static Lights");

	update_cascades();

	SDFGIShader::Light lights[SDFGI::MAX_STATIC_LIGHTS];
	uint32_t light_count[SDFGI::MAX_STATIC_LIGHTS];

	for (uint32_t i = 0; i < p_cascade_count; i++) {
		ERR_CONTINUE(p_cascade_indices[i] >= cascades.size());

		SDFGI::Cascade &cc = cascades[p_cascade_indices[i]];

		{ //fill light buffer

			AABB cascade_aabb;
			cascade_aabb.position = Vector3((Vector3i(1, 1, 1) * -int32_t(cascade_size >> 1) + cc.position)) * cc.cell_size;
			cascade_aabb.size = Vector3(1, 1, 1) * cascade_size * cc.cell_size;

			int idx = 0;

			for (uint32_t j = 0; j < (uint32_t)p_positional_light_cull_result[i].size(); j++) {
				if (idx == SDFGI::MAX_STATIC_LIGHTS) {
					break;
				}

				RID light_instance = p_positional_light_cull_result[i][j];
				ERR_CONTINUE(!light_storage->owns_light_instance(light_instance));

				RID light = light_storage->light_instance_get_base_light(light_instance);
				AABB light_aabb = light_storage->light_instance_get_base_aabb(light_instance);
				Transform3D light_transform = light_storage->light_instance_get_base_transform(light_instance);

				uint32_t max_sdfgi_cascade = RSG::light_storage->light_get_max_sdfgi_cascade(light);
				if (p_cascade_indices[i] > max_sdfgi_cascade) {
					continue;
				}

				if (!cascade_aabb.intersects(light_aabb)) {
					continue;
				}

				lights[idx].type = RSG::light_storage->light_get_type(light);

				Vector3 dir = -light_transform.basis.get_column(Vector3::AXIS_Z);
				if (lights[idx].type == RS::LIGHT_DIRECTIONAL) {
					dir.y *= y_mult; //only makes sense for directional
					dir.normalize();
				}
				lights[idx].direction[0] = dir.x;
				lights[idx].direction[1] = dir.y;
				lights[idx].direction[2] = dir.z;
				Vector3 pos = light_transform.origin;
				pos.y *= y_mult;
				lights[idx].position[0] = pos.x;
				lights[idx].position[1] = pos.y;
				lights[idx].position[2] = pos.z;
				Color color = RSG::light_storage->light_get_color(light);
				color = color.srgb_to_linear();
				lights[idx].color[0] = color.r;
				lights[idx].color[1] = color.g;
				lights[idx].color[2] = color.b;

				lights[idx].energy = RSG::light_storage->light_get_param(light, RS::LIGHT_PARAM_ENERGY) * RSG::light_storage->light_get_param(light, RS::LIGHT_PARAM_INDIRECT_ENERGY);
				if (RendererSceneRenderRD::get_singleton()->is_using_physical_light_units()) {
					lights[idx].energy *= RSG::light_storage->light_get_param(light, RS::LIGHT_PARAM_INTENSITY);

					// Convert from Luminous Power to Luminous Intensity
					if (lights[idx].type == RS::LIGHT_OMNI) {
						lights[idx].energy *= 1.0 / (Math_PI * 4.0);
					} else if (lights[idx].type == RS::LIGHT_SPOT) {
						// Spot Lights are not physically accurate, Luminous Intensity should change in relation to the cone angle.
						// We make this assumption to keep them easy to control.
						lights[idx].energy *= 1.0 / Math_PI;
					}
				}

				if (p_render_data->camera_attributes.is_valid()) {
					lights[idx].energy *= RSG::camera_attributes->camera_attributes_get_exposure_normalization_factor(p_render_data->camera_attributes);
				}

				lights[idx].has_shadow = RSG::light_storage->light_has_shadow(light);
				lights[idx].attenuation = RSG::light_storage->light_get_param(light, RS::LIGHT_PARAM_ATTENUATION);
				lights[idx].radius = RSG::light_storage->light_get_param(light, RS::LIGHT_PARAM_RANGE);
				lights[idx].cos_spot_angle = Math::cos(Math::deg_to_rad(RSG::light_storage->light_get_param(light, RS::LIGHT_PARAM_SPOT_ANGLE)));
				lights[idx].inv_spot_attenuation = 1.0f / RSG::light_storage->light_get_param(light, RS::LIGHT_PARAM_SPOT_ATTENUATION);

				idx++;
			}

			if (idx > 0) {
				RD::get_singleton()->buffer_update(cc.lights_buffer, 0, idx * sizeof(SDFGIShader::Light), lights);
			}

			light_count[i] = idx;
		}
	}

	for (uint32_t i = 0; i < p_cascade_count; i++) {
		ERR_CONTINUE(p_cascade_indices[i] >= cascades.size());

		SDFGI::Cascade &cc = cascades[p_cascade_indices[i]];
		if (light_count[i] > 0) {
			RD::get_singleton()->buffer_copy(cc.solid_cell_dispatch_buffer_storage, cc.solid_cell_dispatch_buffer_call, 0, 0, sizeof(uint32_t) * 4);
		}
	}

	/* Static Lights */
	RD::ComputeListID compute_list = RD::get_singleton()->compute_list_begin();

	RD::get_singleton()->compute_list_bind_compute_pipeline(compute_list, gi->sdfgi_shader.direct_light_pipeline[SDFGIShader::DIRECT_LIGHT_MODE_STATIC]);

	SDFGIShader::DirectLightPushConstant dl_push_constant;

	dl_push_constant.grid_size[0] = cascade_size;
	dl_push_constant.grid_size[1] = cascade_size;
	dl_push_constant.grid_size[2] = cascade_size;
	dl_push_constant.max_cascades = cascades.size();
	dl_push_constant.probe_axis_size = probe_axis_count;
	dl_push_constant.bounce_feedback = 0.0; // this is static light, do not multibounce yet
	dl_push_constant.y_mult = y_mult;
	dl_push_constant.use_occlusion = uses_occlusion;

	//all must be processed
	dl_push_constant.process_offset = 0;
	dl_push_constant.process_increment = 1;

	for (uint32_t i = 0; i < p_cascade_count; i++) {
		ERR_CONTINUE(p_cascade_indices[i] >= cascades.size());

		SDFGI::Cascade &cc = cascades[p_cascade_indices[i]];

		dl_push_constant.light_count = light_count[i];
		dl_push_constant.cascade = p_cascade_indices[i];

		if (dl_push_constant.light_count > 0) {
			RD::get_singleton()->compute_list_bind_uniform_set(compute_list, cc.sdf_direct_light_static_uniform_set, 0);
			RD::get_singleton()->compute_list_set_push_constant(compute_list, &dl_push_constant, sizeof(SDFGIShader::DirectLightPushConstant));
			RD::get_singleton()->compute_list_dispatch_indirect(compute_list, cc.solid_cell_dispatch_buffer_call, 0);
		}
	}

	RD::get_singleton()->compute_list_end();

	RD::get_singleton()->draw_command_end_label();
}

////////////////////////////////////////////////////////////////////////////////
// VoxelGIInstance

void GI::VoxelGIInstance::update(bool p_update_light_instances, const Vector<RID> &p_light_instances, const PagedArray<RenderGeometryInstance *> &p_dynamic_objects) {
	RendererRD::LightStorage *light_storage = RendererRD::LightStorage::get_singleton();
	RendererRD::MaterialStorage *material_storage = RendererRD::MaterialStorage::get_singleton();

	uint32_t data_version = gi->voxel_gi_get_data_version(probe);

	// (RE)CREATE IF NEEDED

	if (last_probe_data_version != data_version) {
		//need to re-create everything
		free_resources();

		Vector3i octree_size = gi->voxel_gi_get_octree_size(probe);

		if (octree_size != Vector3i()) {
			//can create a 3D texture
			Vector<int> levels = gi->voxel_gi_get_level_counts(probe);

			RD::TextureFormat tf;
			tf.format = RD::DATA_FORMAT_R8G8B8A8_UNORM;
			tf.width = octree_size.x;
			tf.height = octree_size.y;
			tf.depth = octree_size.z;
			tf.texture_type = RD::TEXTURE_TYPE_3D;
			tf.mipmaps = levels.size();

			tf.usage_bits = RD::TEXTURE_USAGE_SAMPLING_BIT | RD::TEXTURE_USAGE_STORAGE_BIT | RD::TEXTURE_USAGE_CAN_COPY_TO_BIT;

			texture = RD::get_singleton()->texture_create(tf, RD::TextureView());
			RD::get_singleton()->set_resource_name(texture, "VoxelGI Instance Texture");

			RD::get_singleton()->texture_clear(texture, Color(0, 0, 0, 0), 0, levels.size(), 0, 1);

			{
				int total_elements = 0;
				for (int i = 0; i < levels.size(); i++) {
					total_elements += levels[i];
				}

				write_buffer = RD::get_singleton()->storage_buffer_create(total_elements * 16);
			}

			for (int i = 0; i < levels.size(); i++) {
				VoxelGIInstance::Mipmap mipmap;
				mipmap.texture = RD::get_singleton()->texture_create_shared_from_slice(RD::TextureView(), texture, 0, i, 1, RD::TEXTURE_SLICE_3D);
				mipmap.level = levels.size() - i - 1;
				mipmap.cell_offset = 0;
				for (uint32_t j = 0; j < mipmap.level; j++) {
					mipmap.cell_offset += levels[j];
				}
				mipmap.cell_count = levels[mipmap.level];

				Vector<RD::Uniform> uniforms;
				{
					RD::Uniform u;
					u.uniform_type = RD::UNIFORM_TYPE_STORAGE_BUFFER;
					u.binding = 1;
					u.append_id(gi->voxel_gi_get_octree_buffer(probe));
					uniforms.push_back(u);
				}
				{
					RD::Uniform u;
					u.uniform_type = RD::UNIFORM_TYPE_STORAGE_BUFFER;
					u.binding = 2;
					u.append_id(gi->voxel_gi_get_data_buffer(probe));
					uniforms.push_back(u);
				}

				{
					RD::Uniform u;
					u.uniform_type = RD::UNIFORM_TYPE_STORAGE_BUFFER;
					u.binding = 4;
					u.append_id(write_buffer);
					uniforms.push_back(u);
				}
				{
					RD::Uniform u;
					u.uniform_type = RD::UNIFORM_TYPE_TEXTURE;
					u.binding = 9;
					u.append_id(gi->voxel_gi_get_sdf_texture(probe));
					uniforms.push_back(u);
				}
				{
					RD::Uniform u;
					u.uniform_type = RD::UNIFORM_TYPE_SAMPLER;
					u.binding = 10;
					u.append_id(material_storage->sampler_rd_get_default(RS::CANVAS_ITEM_TEXTURE_FILTER_LINEAR_WITH_MIPMAPS, RS::CANVAS_ITEM_TEXTURE_REPEAT_DISABLED));
					uniforms.push_back(u);
				}

				{
					Vector<RD::Uniform> copy_uniforms = uniforms;
					if (i == 0) {
						{
							RD::Uniform u;
							u.uniform_type = RD::UNIFORM_TYPE_UNIFORM_BUFFER;
							u.binding = 3;
							u.append_id(gi->voxel_gi_lights_uniform);
							copy_uniforms.push_back(u);
						}

						mipmap.uniform_set = RD::get_singleton()->uniform_set_create(copy_uniforms, gi->voxel_gi_lighting_shader_version_shaders[VOXEL_GI_SHADER_VERSION_COMPUTE_LIGHT], 0);

						copy_uniforms = uniforms; //restore

						{
							RD::Uniform u;
							u.uniform_type = RD::UNIFORM_TYPE_TEXTURE;
							u.binding = 5;
							u.append_id(texture);
							copy_uniforms.push_back(u);
						}
						mipmap.second_bounce_uniform_set = RD::get_singleton()->uniform_set_create(copy_uniforms, gi->voxel_gi_lighting_shader_version_shaders[VOXEL_GI_SHADER_VERSION_COMPUTE_SECOND_BOUNCE], 0);
					} else {
						mipmap.uniform_set = RD::get_singleton()->uniform_set_create(copy_uniforms, gi->voxel_gi_lighting_shader_version_shaders[VOXEL_GI_SHADER_VERSION_COMPUTE_MIPMAP], 0);
					}
				}

				{
					RD::Uniform u;
					u.uniform_type = RD::UNIFORM_TYPE_IMAGE;
					u.binding = 5;
					u.append_id(mipmap.texture);
					uniforms.push_back(u);
				}

				mipmap.write_uniform_set = RD::get_singleton()->uniform_set_create(uniforms, gi->voxel_gi_lighting_shader_version_shaders[VOXEL_GI_SHADER_VERSION_WRITE_TEXTURE], 0);

				mipmaps.push_back(mipmap);
			}

			{
				uint32_t dynamic_map_size = MAX(MAX(octree_size.x, octree_size.y), octree_size.z);
				uint32_t oversample = nearest_power_of_2_templated(4);
				int mipmap_index = 0;

				while (mipmap_index < mipmaps.size()) {
					VoxelGIInstance::DynamicMap dmap;

					if (oversample > 0) {
						dmap.size = dynamic_map_size * (1 << oversample);
						dmap.mipmap = -1;
						oversample--;
					} else {
						dmap.size = dynamic_map_size >> mipmap_index;
						dmap.mipmap = mipmap_index;
						mipmap_index++;
					}

					RD::TextureFormat dtf;
					dtf.width = dmap.size;
					dtf.height = dmap.size;
					dtf.format = RD::DATA_FORMAT_R16G16B16A16_SFLOAT;
					dtf.usage_bits = RD::TEXTURE_USAGE_STORAGE_BIT;

					if (dynamic_maps.size() == 0) {
						dtf.usage_bits |= RD::TEXTURE_USAGE_COLOR_ATTACHMENT_BIT;
					}
					dmap.texture = RD::get_singleton()->texture_create(dtf, RD::TextureView());
					RD::get_singleton()->set_resource_name(dmap.texture, "VoxelGI Instance DMap Texture");

					if (dynamic_maps.size() == 0) {
						// Render depth for first one.
						// Use 16-bit depth when supported to improve performance.
						dtf.format = RD::get_singleton()->texture_is_format_supported_for_usage(RD::DATA_FORMAT_D16_UNORM, RD::TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) ? RD::DATA_FORMAT_D16_UNORM : RD::DATA_FORMAT_X8_D24_UNORM_PACK32;
						dtf.usage_bits = RD::TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
						dmap.fb_depth = RD::get_singleton()->texture_create(dtf, RD::TextureView());
						RD::get_singleton()->set_resource_name(dmap.fb_depth, "VoxelGI Instance DMap FB Depth");
					}

					//just use depth as-is
					dtf.format = RD::DATA_FORMAT_R32_SFLOAT;
					dtf.usage_bits = RD::TEXTURE_USAGE_STORAGE_BIT | RD::TEXTURE_USAGE_COLOR_ATTACHMENT_BIT;

					dmap.depth = RD::get_singleton()->texture_create(dtf, RD::TextureView());
					RD::get_singleton()->set_resource_name(dmap.depth, "VoxelGI Instance DMap Depth");

					if (dynamic_maps.size() == 0) {
						dtf.format = RD::DATA_FORMAT_R8G8B8A8_UNORM;
						dtf.usage_bits = RD::TEXTURE_USAGE_STORAGE_BIT | RD::TEXTURE_USAGE_COLOR_ATTACHMENT_BIT;
						dmap.albedo = RD::get_singleton()->texture_create(dtf, RD::TextureView());
						RD::get_singleton()->set_resource_name(dmap.albedo, "VoxelGI Instance DMap Albedo");
						dmap.normal = RD::get_singleton()->texture_create(dtf, RD::TextureView());
						RD::get_singleton()->set_resource_name(dmap.normal, "VoxelGI Instance DMap Normal");
						dmap.orm = RD::get_singleton()->texture_create(dtf, RD::TextureView());
						RD::get_singleton()->set_resource_name(dmap.orm, "VoxelGI Instance DMap ORM");

						Vector<RID> fb;
						fb.push_back(dmap.albedo);
						fb.push_back(dmap.normal);
						fb.push_back(dmap.orm);
						fb.push_back(dmap.texture); //emission
						fb.push_back(dmap.depth);
						fb.push_back(dmap.fb_depth);

						dmap.fb = RD::get_singleton()->framebuffer_create(fb);

						{
							Vector<RD::Uniform> uniforms;
							{
								RD::Uniform u;
								u.uniform_type = RD::UNIFORM_TYPE_UNIFORM_BUFFER;
								u.binding = 3;
								u.append_id(gi->voxel_gi_lights_uniform);
								uniforms.push_back(u);
							}

							{
								RD::Uniform u;
								u.uniform_type = RD::UNIFORM_TYPE_IMAGE;
								u.binding = 5;
								u.append_id(dmap.albedo);
								uniforms.push_back(u);
							}
							{
								RD::Uniform u;
								u.uniform_type = RD::UNIFORM_TYPE_IMAGE;
								u.binding = 6;
								u.append_id(dmap.normal);
								uniforms.push_back(u);
							}
							{
								RD::Uniform u;
								u.uniform_type = RD::UNIFORM_TYPE_IMAGE;
								u.binding = 7;
								u.append_id(dmap.orm);
								uniforms.push_back(u);
							}
							{
								RD::Uniform u;
								u.uniform_type = RD::UNIFORM_TYPE_TEXTURE;
								u.binding = 8;
								u.append_id(dmap.fb_depth);
								uniforms.push_back(u);
							}
							{
								RD::Uniform u;
								u.uniform_type = RD::UNIFORM_TYPE_TEXTURE;
								u.binding = 9;
								u.append_id(gi->voxel_gi_get_sdf_texture(probe));
								uniforms.push_back(u);
							}
							{
								RD::Uniform u;
								u.uniform_type = RD::UNIFORM_TYPE_SAMPLER;
								u.binding = 10;
								u.append_id(material_storage->sampler_rd_get_default(RS::CANVAS_ITEM_TEXTURE_FILTER_LINEAR_WITH_MIPMAPS, RS::CANVAS_ITEM_TEXTURE_REPEAT_DISABLED));
								uniforms.push_back(u);
							}
							{
								RD::Uniform u;
								u.uniform_type = RD::UNIFORM_TYPE_IMAGE;
								u.binding = 11;
								u.append_id(dmap.texture);
								uniforms.push_back(u);
							}
							{
								RD::Uniform u;
								u.uniform_type = RD::UNIFORM_TYPE_IMAGE;
								u.binding = 12;
								u.append_id(dmap.depth);
								uniforms.push_back(u);
							}

							dmap.uniform_set = RD::get_singleton()->uniform_set_create(uniforms, gi->voxel_gi_lighting_shader_version_shaders[VOXEL_GI_SHADER_VERSION_DYNAMIC_OBJECT_LIGHTING], 0);
						}
					} else {
						bool plot = dmap.mipmap >= 0;
						bool write = dmap.mipmap < (mipmaps.size() - 1);

						Vector<RD::Uniform> uniforms;

						{
							RD::Uniform u;
							u.uniform_type = RD::UNIFORM_TYPE_IMAGE;
							u.binding = 5;
							u.append_id(dynamic_maps[dynamic_maps.size() - 1].texture);
							uniforms.push_back(u);
						}
						{
							RD::Uniform u;
							u.uniform_type = RD::UNIFORM_TYPE_IMAGE;
							u.binding = 6;
							u.append_id(dynamic_maps[dynamic_maps.size() - 1].depth);
							uniforms.push_back(u);
						}

						if (write) {
							{
								RD::Uniform u;
								u.uniform_type = RD::UNIFORM_TYPE_IMAGE;
								u.binding = 7;
								u.append_id(dmap.texture);
								uniforms.push_back(u);
							}
							{
								RD::Uniform u;
								u.uniform_type = RD::UNIFORM_TYPE_IMAGE;
								u.binding = 8;
								u.append_id(dmap.depth);
								uniforms.push_back(u);
							}
						}

						{
							RD::Uniform u;
							u.uniform_type = RD::UNIFORM_TYPE_TEXTURE;
							u.binding = 9;
							u.append_id(gi->voxel_gi_get_sdf_texture(probe));
							uniforms.push_back(u);
						}
						{
							RD::Uniform u;
							u.uniform_type = RD::UNIFORM_TYPE_SAMPLER;
							u.binding = 10;
							u.append_id(material_storage->sampler_rd_get_default(RS::CANVAS_ITEM_TEXTURE_FILTER_LINEAR_WITH_MIPMAPS, RS::CANVAS_ITEM_TEXTURE_REPEAT_DISABLED));
							uniforms.push_back(u);
						}

						if (plot) {
							{
								RD::Uniform u;
								u.uniform_type = RD::UNIFORM_TYPE_IMAGE;
								u.binding = 11;
								u.append_id(mipmaps[dmap.mipmap].texture);
								uniforms.push_back(u);
							}
						}

						dmap.uniform_set = RD::get_singleton()->uniform_set_create(
								uniforms,
								gi->voxel_gi_lighting_shader_version_shaders[(write && plot) ? VOXEL_GI_SHADER_VERSION_DYNAMIC_SHRINK_WRITE_PLOT : (write ? VOXEL_GI_SHADER_VERSION_DYNAMIC_SHRINK_WRITE : VOXEL_GI_SHADER_VERSION_DYNAMIC_SHRINK_PLOT)],
								0);
					}

					dynamic_maps.push_back(dmap);
				}
			}
		}

		last_probe_data_version = data_version;
		p_update_light_instances = true; //just in case

		RendererSceneRenderRD::get_singleton()->base_uniforms_changed();
	}

	// UDPDATE TIME

	if (has_dynamic_object_data) {
		//if it has dynamic object data, it needs to be cleared
		RD::get_singleton()->texture_clear(texture, Color(0, 0, 0, 0), 0, mipmaps.size(), 0, 1);
	}

	uint32_t light_count = 0;

	if (p_update_light_instances || p_dynamic_objects.size() > 0) {
		light_count = MIN(gi->voxel_gi_max_lights, (uint32_t)p_light_instances.size());

		{
			Transform3D to_cell = gi->voxel_gi_get_to_cell_xform(probe);
			Transform3D to_probe_xform = to_cell * transform.affine_inverse();

			//update lights

			for (uint32_t i = 0; i < light_count; i++) {
				VoxelGILight &l = gi->voxel_gi_lights[i];
				RID light_instance = p_light_instances[i];
				RID light = light_storage->light_instance_get_base_light(light_instance);

				l.type = RSG::light_storage->light_get_type(light);
				if (l.type == RS::LIGHT_DIRECTIONAL && RSG::light_storage->light_directional_get_sky_mode(light) == RS::LIGHT_DIRECTIONAL_SKY_MODE_SKY_ONLY) {
					light_count--;
					continue;
				}

				l.attenuation = RSG::light_storage->light_get_param(light, RS::LIGHT_PARAM_ATTENUATION);
				l.energy = RSG::light_storage->light_get_param(light, RS::LIGHT_PARAM_ENERGY) * RSG::light_storage->light_get_param(light, RS::LIGHT_PARAM_INDIRECT_ENERGY);

				if (RendererSceneRenderRD::get_singleton()->is_using_physical_light_units()) {
					l.energy *= RSG::light_storage->light_get_param(light, RS::LIGHT_PARAM_INTENSITY);

					l.energy *= gi->voxel_gi_get_baked_exposure_normalization(probe);

					// Convert from Luminous Power to Luminous Intensity
					if (l.type == RS::LIGHT_OMNI) {
						l.energy *= 1.0 / (Math_PI * 4.0);
					} else if (l.type == RS::LIGHT_SPOT) {
						// Spot Lights are not physically accurate, Luminous Intensity should change in relation to the cone angle.
						// We make this assumption to keep them easy to control.
						l.energy *= 1.0 / Math_PI;
					}
				}

				l.radius = to_cell.basis.xform(Vector3(RSG::light_storage->light_get_param(light, RS::LIGHT_PARAM_RANGE), 0, 0)).length();
				Color color = RSG::light_storage->light_get_color(light).srgb_to_linear();
				l.color[0] = color.r;
				l.color[1] = color.g;
				l.color[2] = color.b;

				l.cos_spot_angle = Math::cos(Math::deg_to_rad(RSG::light_storage->light_get_param(light, RS::LIGHT_PARAM_SPOT_ANGLE)));
				l.inv_spot_attenuation = 1.0f / RSG::light_storage->light_get_param(light, RS::LIGHT_PARAM_SPOT_ATTENUATION);

				Transform3D xform = light_storage->light_instance_get_base_transform(light_instance);

				Vector3 pos = to_probe_xform.xform(xform.origin);
				Vector3 dir = to_probe_xform.basis.xform(-xform.basis.get_column(2)).normalized();

				l.position[0] = pos.x;
				l.position[1] = pos.y;
				l.position[2] = pos.z;

				l.direction[0] = dir.x;
				l.direction[1] = dir.y;
				l.direction[2] = dir.z;

				l.has_shadow = RSG::light_storage->light_has_shadow(light);
			}

			RD::get_singleton()->buffer_update(gi->voxel_gi_lights_uniform, 0, sizeof(VoxelGILight) * light_count, gi->voxel_gi_lights);
		}
	}

	if (has_dynamic_object_data || p_update_light_instances || p_dynamic_objects.size()) {
		// PROCESS MIPMAPS
		if (mipmaps.size()) {
			//can update mipmaps

			Vector3i probe_size = gi->voxel_gi_get_octree_size(probe);

			VoxelGIPushConstant push_constant;

			push_constant.limits[0] = probe_size.x;
			push_constant.limits[1] = probe_size.y;
			push_constant.limits[2] = probe_size.z;
			push_constant.stack_size = mipmaps.size();
			push_constant.emission_scale = 1.0;
			push_constant.propagation = gi->voxel_gi_get_propagation(probe);
			push_constant.dynamic_range = gi->voxel_gi_get_dynamic_range(probe);
			push_constant.light_count = light_count;
			push_constant.aniso_strength = 0;

			/*		print_line("probe update to version " + itos(last_probe_version));
			print_line("propagation " + rtos(push_constant.propagation));
			print_line("dynrange " + rtos(push_constant.dynamic_range));
	*/
			RD::ComputeListID compute_list = RD::get_singleton()->compute_list_begin();

			int passes;
			if (p_update_light_instances) {
				passes = gi->voxel_gi_is_using_two_bounces(probe) ? 2 : 1;
			} else {
				passes = 1; //only re-blitting is necessary
			}
			int wg_size = 64;
			int64_t wg_limit_x = (int64_t)RD::get_singleton()->limit_get(RD::LIMIT_MAX_COMPUTE_WORKGROUP_COUNT_X);

			for (int pass = 0; pass < passes; pass++) {
				if (p_update_light_instances) {
					for (int i = 0; i < mipmaps.size(); i++) {
						if (i == 0) {
							RD::get_singleton()->compute_list_bind_compute_pipeline(compute_list, gi->voxel_gi_lighting_shader_version_pipelines[pass == 0 ? VOXEL_GI_SHADER_VERSION_COMPUTE_LIGHT : VOXEL_GI_SHADER_VERSION_COMPUTE_SECOND_BOUNCE]);
						} else if (i == 1) {
							RD::get_singleton()->compute_list_bind_compute_pipeline(compute_list, gi->voxel_gi_lighting_shader_version_pipelines[VOXEL_GI_SHADER_VERSION_COMPUTE_MIPMAP]);
						}

						if (pass == 1 || i > 0) {
							RD::get_singleton()->compute_list_add_barrier(compute_list); //wait til previous step is done
						}
						if (pass == 0 || i > 0) {
							RD::get_singleton()->compute_list_bind_uniform_set(compute_list, mipmaps[i].uniform_set, 0);
						} else {
							RD::get_singleton()->compute_list_bind_uniform_set(compute_list, mipmaps[i].second_bounce_uniform_set, 0);
						}

						push_constant.cell_offset = mipmaps[i].cell_offset;
						push_constant.cell_count = mipmaps[i].cell_count;

						int64_t wg_todo = (mipmaps[i].cell_count + wg_size - 1) / wg_size;
						while (wg_todo) {
							int64_t wg_count = MIN(wg_todo, wg_limit_x);
							RD::get_singleton()->compute_list_set_push_constant(compute_list, &push_constant, sizeof(VoxelGIPushConstant));
							RD::get_singleton()->compute_list_dispatch(compute_list, wg_count, 1, 1);
							wg_todo -= wg_count;
							push_constant.cell_offset += wg_count * wg_size;
						}
					}

					RD::get_singleton()->compute_list_add_barrier(compute_list); //wait til previous step is done
				}

				RD::get_singleton()->compute_list_bind_compute_pipeline(compute_list, gi->voxel_gi_lighting_shader_version_pipelines[VOXEL_GI_SHADER_VERSION_WRITE_TEXTURE]);

				for (int i = 0; i < mipmaps.size(); i++) {
					RD::get_singleton()->compute_list_bind_uniform_set(compute_list, mipmaps[i].write_uniform_set, 0);

					push_constant.cell_offset = mipmaps[i].cell_offset;
					push_constant.cell_count = mipmaps[i].cell_count;

					int64_t wg_todo = (mipmaps[i].cell_count + wg_size - 1) / wg_size;
					while (wg_todo) {
						int64_t wg_count = MIN(wg_todo, wg_limit_x);
						RD::get_singleton()->compute_list_set_push_constant(compute_list, &push_constant, sizeof(VoxelGIPushConstant));
						RD::get_singleton()->compute_list_dispatch(compute_list, wg_count, 1, 1);
						wg_todo -= wg_count;
						push_constant.cell_offset += wg_count * wg_size;
					}
				}
			}

			RD::get_singleton()->compute_list_end();
		}
	}

	has_dynamic_object_data = false; //clear until dynamic object data is used again

	if (p_dynamic_objects.size() && dynamic_maps.size()) {
		Vector3i octree_size = gi->voxel_gi_get_octree_size(probe);
		int multiplier = dynamic_maps[0].size / MAX(MAX(octree_size.x, octree_size.y), octree_size.z);

		Transform3D oversample_scale;
		oversample_scale.basis.scale(Vector3(multiplier, multiplier, multiplier));

		Transform3D to_cell = oversample_scale * gi->voxel_gi_get_to_cell_xform(probe);
		Transform3D to_world_xform = transform * to_cell.affine_inverse();
		Transform3D to_probe_xform = to_world_xform.affine_inverse();

		AABB probe_aabb(Vector3(), octree_size);

		//this could probably be better parallelized in compute..
		for (int i = 0; i < (int)p_dynamic_objects.size(); i++) {
			RenderGeometryInstance *instance = p_dynamic_objects[i];

			//transform aabb to voxel_gi
			AABB aabb = (to_probe_xform * instance->get_transform()).xform(instance->get_aabb());

			//this needs to wrap to grid resolution to avoid jitter
			//also extend margin a bit just in case
			Vector3i begin = aabb.position - Vector3i(1, 1, 1);
			Vector3i end = aabb.position + aabb.size + Vector3i(1, 1, 1);

			for (int j = 0; j < 3; j++) {
				if ((end[j] - begin[j]) & 1) {
					end[j]++; //for half extents split, it needs to be even
				}
				begin[j] = MAX(begin[j], 0);
				end[j] = MIN(end[j], octree_size[j] * multiplier);
			}

			//aabb = aabb.intersection(probe_aabb); //intersect
			aabb.position = begin;
			aabb.size = end - begin;

			//print_line("aabb: " + aabb);

			for (int j = 0; j < 6; j++) {
				//if (j != 0 && j != 3) {
				//	continue;
				//}
				static const Vector3 render_z[6] = {
					Vector3(1, 0, 0),
					Vector3(0, 1, 0),
					Vector3(0, 0, 1),
					Vector3(-1, 0, 0),
					Vector3(0, -1, 0),
					Vector3(0, 0, -1),
				};
				static const Vector3 render_up[6] = {
					Vector3(0, 1, 0),
					Vector3(0, 0, 1),
					Vector3(0, 1, 0),
					Vector3(0, 1, 0),
					Vector3(0, 0, 1),
					Vector3(0, 1, 0),
				};

				Vector3 render_dir = render_z[j];
				Vector3 up_dir = render_up[j];

				Vector3 center = aabb.get_center();
				Transform3D xform;
				xform.set_look_at(center - aabb.size * 0.5 * render_dir, center, up_dir);

				Vector3 x_dir = xform.basis.get_column(0).abs();
				int x_axis = int(Vector3(0, 1, 2).dot(x_dir));
				Vector3 y_dir = xform.basis.get_column(1).abs();
				int y_axis = int(Vector3(0, 1, 2).dot(y_dir));
				Vector3 z_dir = -xform.basis.get_column(2);
				int z_axis = int(Vector3(0, 1, 2).dot(z_dir.abs()));

				Rect2i rect(aabb.position[x_axis], aabb.position[y_axis], aabb.size[x_axis], aabb.size[y_axis]);
				bool x_flip = bool(Vector3(1, 1, 1).dot(xform.basis.get_column(0)) < 0);
				bool y_flip = bool(Vector3(1, 1, 1).dot(xform.basis.get_column(1)) < 0);
				bool z_flip = bool(Vector3(1, 1, 1).dot(xform.basis.get_column(2)) > 0);

				Projection cm;
				cm.set_orthogonal(-rect.size.width / 2, rect.size.width / 2, -rect.size.height / 2, rect.size.height / 2, 0.0001, aabb.size[z_axis]);

				if (RendererSceneRenderRD::get_singleton()->cull_argument.size() == 0) {
					RendererSceneRenderRD::get_singleton()->cull_argument.push_back(nullptr);
				}
				RendererSceneRenderRD::get_singleton()->cull_argument[0] = instance;

				float exposure_normalization = 1.0;
				if (RendererSceneRenderRD::get_singleton()->is_using_physical_light_units()) {
					exposure_normalization = gi->voxel_gi_get_baked_exposure_normalization(probe);
				}

				RendererSceneRenderRD::get_singleton()->_render_material(to_world_xform * xform, cm, true, RendererSceneRenderRD::get_singleton()->cull_argument, dynamic_maps[0].fb, Rect2i(Vector2i(), rect.size), exposure_normalization);

				VoxelGIDynamicPushConstant push_constant;
				memset(&push_constant, 0, sizeof(VoxelGIDynamicPushConstant));
				push_constant.limits[0] = octree_size.x;
				push_constant.limits[1] = octree_size.y;
				push_constant.limits[2] = octree_size.z;
				push_constant.light_count = p_light_instances.size();
				push_constant.x_dir[0] = x_dir[0];
				push_constant.x_dir[1] = x_dir[1];
				push_constant.x_dir[2] = x_dir[2];
				push_constant.y_dir[0] = y_dir[0];
				push_constant.y_dir[1] = y_dir[1];
				push_constant.y_dir[2] = y_dir[2];
				push_constant.z_dir[0] = z_dir[0];
				push_constant.z_dir[1] = z_dir[1];
				push_constant.z_dir[2] = z_dir[2];
				push_constant.z_base = xform.origin[z_axis];
				push_constant.z_sign = (z_flip ? -1.0 : 1.0);
				push_constant.pos_multiplier = float(1.0) / multiplier;
				push_constant.dynamic_range = gi->voxel_gi_get_dynamic_range(probe);
				push_constant.flip_x = x_flip;
				push_constant.flip_y = y_flip;
				push_constant.rect_pos[0] = rect.position[0];
				push_constant.rect_pos[1] = rect.position[1];
				push_constant.rect_size[0] = rect.size[0];
				push_constant.rect_size[1] = rect.size[1];
				push_constant.prev_rect_ofs[0] = 0;
				push_constant.prev_rect_ofs[1] = 0;
				push_constant.prev_rect_size[0] = 0;
				push_constant.prev_rect_size[1] = 0;
				push_constant.on_mipmap = false;
				push_constant.propagation = gi->voxel_gi_get_propagation(probe);
				push_constant.pad[0] = 0;
				push_constant.pad[1] = 0;
				push_constant.pad[2] = 0;

				//process lighting
				RD::ComputeListID compute_list = RD::get_singleton()->compute_list_begin();
				RD::get_singleton()->compute_list_bind_compute_pipeline(compute_list, gi->voxel_gi_lighting_shader_version_pipelines[VOXEL_GI_SHADER_VERSION_DYNAMIC_OBJECT_LIGHTING]);
				RD::get_singleton()->compute_list_bind_uniform_set(compute_list, dynamic_maps[0].uniform_set, 0);
				RD::get_singleton()->compute_list_set_push_constant(compute_list, &push_constant, sizeof(VoxelGIDynamicPushConstant));
				RD::get_singleton()->compute_list_dispatch(compute_list, Math::division_round_up(rect.size.x, 8), Math::division_round_up(rect.size.y, 8), 1);
				//print_line("rect: " + itos(i) + ": " + rect);

				for (int k = 1; k < dynamic_maps.size(); k++) {
					// enlarge the rect if needed so all pixels fit when downscaled,
					// this ensures downsampling is smooth and optimal because no pixels are left behind

					//x
					if (rect.position.x & 1) {
						rect.size.x++;
						push_constant.prev_rect_ofs[0] = 1; //this is used to ensure reading is also optimal
					} else {
						push_constant.prev_rect_ofs[0] = 0;
					}
					if (rect.size.x & 1) {
						rect.size.x++;
					}

					rect.position.x >>= 1;
					rect.size.x = MAX(1, rect.size.x >> 1);

					//y
					if (rect.position.y & 1) {
						rect.size.y++;
						push_constant.prev_rect_ofs[1] = 1;
					} else {
						push_constant.prev_rect_ofs[1] = 0;
					}
					if (rect.size.y & 1) {
						rect.size.y++;
					}

					rect.position.y >>= 1;
					rect.size.y = MAX(1, rect.size.y >> 1);

					//shrink limits to ensure plot does not go outside map
					if (dynamic_maps[k].mipmap > 0) {
						for (int l = 0; l < 3; l++) {
							push_constant.limits[l] = MAX(1, push_constant.limits[l] >> 1);
						}
					}

					//print_line("rect: " + itos(i) + ": " + rect);
					push_constant.rect_pos[0] = rect.position[0];
					push_constant.rect_pos[1] = rect.position[1];
					push_constant.prev_rect_size[0] = push_constant.rect_size[0];
					push_constant.prev_rect_size[1] = push_constant.rect_size[1];
					push_constant.rect_size[0] = rect.size[0];
					push_constant.rect_size[1] = rect.size[1];
					push_constant.on_mipmap = dynamic_maps[k].mipmap > 0;

					RD::get_singleton()->compute_list_add_barrier(compute_list);

					if (dynamic_maps[k].mipmap < 0) {
						RD::get_singleton()->compute_list_bind_compute_pipeline(compute_list, gi->voxel_gi_lighting_shader_version_pipelines[VOXEL_GI_SHADER_VERSION_DYNAMIC_SHRINK_WRITE]);
					} else if (k < dynamic_maps.size() - 1) {
						RD::get_singleton()->compute_list_bind_compute_pipeline(compute_list, gi->voxel_gi_lighting_shader_version_pipelines[VOXEL_GI_SHADER_VERSION_DYNAMIC_SHRINK_WRITE_PLOT]);
					} else {
						RD::get_singleton()->compute_list_bind_compute_pipeline(compute_list, gi->voxel_gi_lighting_shader_version_pipelines[VOXEL_GI_SHADER_VERSION_DYNAMIC_SHRINK_PLOT]);
					}
					RD::get_singleton()->compute_list_bind_uniform_set(compute_list, dynamic_maps[k].uniform_set, 0);
					RD::get_singleton()->compute_list_set_push_constant(compute_list, &push_constant, sizeof(VoxelGIDynamicPushConstant));
					RD::get_singleton()->compute_list_dispatch(compute_list, Math::division_round_up(rect.size.x, 8), Math::division_round_up(rect.size.y, 8), 1);
				}

				RD::get_singleton()->compute_list_end();
			}
		}

		has_dynamic_object_data = true; //clear until dynamic object data is used again
	}

	last_probe_version = gi->voxel_gi_get_version(probe);
}

void GI::VoxelGIInstance::free_resources() {
	if (texture.is_valid()) {
		RD::get_singleton()->free(texture);
		RD::get_singleton()->free(write_buffer);

		texture = RID();
		write_buffer = RID();
		mipmaps.clear();
	}

	for (int i = 0; i < dynamic_maps.size(); i++) {
		RD::get_singleton()->free(dynamic_maps[i].texture);
		RD::get_singleton()->free(dynamic_maps[i].depth);

		// these only exist on the first level...
		if (dynamic_maps[i].fb_depth.is_valid()) {
			RD::get_singleton()->free(dynamic_maps[i].fb_depth);
		}
		if (dynamic_maps[i].albedo.is_valid()) {
			RD::get_singleton()->free(dynamic_maps[i].albedo);
		}
		if (dynamic_maps[i].normal.is_valid()) {
			RD::get_singleton()->free(dynamic_maps[i].normal);
		}
		if (dynamic_maps[i].orm.is_valid()) {
			RD::get_singleton()->free(dynamic_maps[i].orm);
		}
	}
	dynamic_maps.clear();
}

void GI::VoxelGIInstance::debug(RD::DrawListID p_draw_list, RID p_framebuffer, const Projection &p_camera_with_transform, bool p_lighting, bool p_emission, float p_alpha) {
	RendererRD::MaterialStorage *material_storage = RendererRD::MaterialStorage::get_singleton();

	if (mipmaps.size() == 0) {
		return;
	}

	Projection cam_transform = (p_camera_with_transform * Projection(transform)) * Projection(gi->voxel_gi_get_to_cell_xform(probe).affine_inverse());

	int level = 0;
	Vector3i octree_size = gi->voxel_gi_get_octree_size(probe);

	VoxelGIDebugPushConstant push_constant;
	push_constant.alpha = p_alpha;
	push_constant.dynamic_range = gi->voxel_gi_get_dynamic_range(probe);
	push_constant.cell_offset = mipmaps[level].cell_offset;
	push_constant.level = level;

	push_constant.bounds[0] = octree_size.x >> level;
	push_constant.bounds[1] = octree_size.y >> level;
	push_constant.bounds[2] = octree_size.z >> level;
	push_constant.pad = 0;

	for (int i = 0; i < 4; i++) {
		for (int j = 0; j < 4; j++) {
			push_constant.projection[i * 4 + j] = cam_transform.columns[i][j];
		}
	}

	if (gi->voxel_gi_debug_uniform_set.is_valid()) {
		RD::get_singleton()->free(gi->voxel_gi_debug_uniform_set);
	}
	Vector<RD::Uniform> uniforms;
	{
		RD::Uniform u;
		u.uniform_type = RD::UNIFORM_TYPE_STORAGE_BUFFER;
		u.binding = 1;
		u.append_id(gi->voxel_gi_get_data_buffer(probe));
		uniforms.push_back(u);
	}
	{
		RD::Uniform u;
		u.uniform_type = RD::UNIFORM_TYPE_TEXTURE;
		u.binding = 2;
		u.append_id(texture);
		uniforms.push_back(u);
	}
	{
		RD::Uniform u;
		u.uniform_type = RD::UNIFORM_TYPE_SAMPLER;
		u.binding = 3;
		u.append_id(material_storage->sampler_rd_get_default(RS::CANVAS_ITEM_TEXTURE_FILTER_NEAREST, RS::CANVAS_ITEM_TEXTURE_REPEAT_DISABLED));
		uniforms.push_back(u);
	}

	int cell_count;
	if (!p_emission && p_lighting && has_dynamic_object_data) {
		cell_count = push_constant.bounds[0] * push_constant.bounds[1] * push_constant.bounds[2];
	} else {
		cell_count = mipmaps[level].cell_count;
	}

	gi->voxel_gi_debug_uniform_set = RD::get_singleton()->uniform_set_create(uniforms, gi->voxel_gi_debug_shader_version_shaders[0], 0);

	int voxel_gi_debug_pipeline = VOXEL_GI_DEBUG_COLOR;
	if (p_emission) {
		voxel_gi_debug_pipeline = VOXEL_GI_DEBUG_EMISSION;
	} else if (p_lighting) {
		voxel_gi_debug_pipeline = has_dynamic_object_data ? VOXEL_GI_DEBUG_LIGHT_FULL : VOXEL_GI_DEBUG_LIGHT;
	}
	RD::get_singleton()->draw_list_bind_render_pipeline(
			p_draw_list,
			gi->voxel_gi_debug_shader_version_pipelines[voxel_gi_debug_pipeline].get_render_pipeline(RD::INVALID_ID, RD::get_singleton()->framebuffer_get_format(p_framebuffer)));
	RD::get_singleton()->draw_list_bind_uniform_set(p_draw_list, gi->voxel_gi_debug_uniform_set, 0);
	RD::get_singleton()->draw_list_set_push_constant(p_draw_list, &push_constant, sizeof(VoxelGIDebugPushConstant));
	RD::get_singleton()->draw_list_draw(p_draw_list, false, cell_count, 36);
}

////////////////////////////////////////////////////////////////////////////////
// GI

GI::GI() {
	singleton = this;

	sdfgi_ray_count = RS::EnvironmentSDFGIRayCount(CLAMP(int32_t(GLOBAL_GET("rendering/global_illumination/sdfgi/probe_ray_count")), 0, int32_t(RS::ENV_SDFGI_RAY_COUNT_MAX - 1)));
	sdfgi_frames_to_converge = RS::EnvironmentSDFGIFramesToConverge(CLAMP(int32_t(GLOBAL_GET("rendering/global_illumination/sdfgi/frames_to_converge")), 0, int32_t(RS::ENV_SDFGI_CONVERGE_MAX - 1)));
	sdfgi_frames_to_update_light = RS::EnvironmentSDFGIFramesToUpdateLight(CLAMP(int32_t(GLOBAL_GET("rendering/global_illumination/sdfgi/frames_to_update_lights")), 0, int32_t(RS::ENV_SDFGI_UPDATE_LIGHT_MAX - 1)));
}

GI::~GI() {
	singleton = nullptr;
}

void GI::init(SkyRD *p_sky) {
	RendererRD::TextureStorage *texture_storage = RendererRD::TextureStorage::get_singleton();
	RendererRD::MaterialStorage *material_storage = RendererRD::MaterialStorage::get_singleton();

	/* GI */

	{
		//kinda complicated to compute the amount of slots, we try to use as many as we can

		voxel_gi_lights = memnew_arr(VoxelGILight, voxel_gi_max_lights);
		voxel_gi_lights_uniform = RD::get_singleton()->uniform_buffer_create(voxel_gi_max_lights * sizeof(VoxelGILight));
		voxel_gi_quality = RS::VoxelGIQuality(CLAMP(int(GLOBAL_GET("rendering/global_illumination/voxel_gi/quality")), 0, 1));

		String defines = "\n#define MAX_LIGHTS " + itos(voxel_gi_max_lights) + "\n";

		Vector<String> versions;
		versions.push_back("\n#define MODE_COMPUTE_LIGHT\n");
		versions.push_back("\n#define MODE_SECOND_BOUNCE\n");
		versions.push_back("\n#define MODE_UPDATE_MIPMAPS\n");
		versions.push_back("\n#define MODE_WRITE_TEXTURE\n");
		versions.push_back("\n#define MODE_DYNAMIC\n#define MODE_DYNAMIC_LIGHTING\n");
		versions.push_back("\n#define MODE_DYNAMIC\n#define MODE_DYNAMIC_SHRINK\n#define MODE_DYNAMIC_SHRINK_WRITE\n");
		versions.push_back("\n#define MODE_DYNAMIC\n#define MODE_DYNAMIC_SHRINK\n#define MODE_DYNAMIC_SHRINK_PLOT\n");
		versions.push_back("\n#define MODE_DYNAMIC\n#define MODE_DYNAMIC_SHRINK\n#define MODE_DYNAMIC_SHRINK_PLOT\n#define MODE_DYNAMIC_SHRINK_WRITE\n");

		voxel_gi_shader.initialize(versions, defines);
		voxel_gi_lighting_shader_version = voxel_gi_shader.version_create();
		for (int i = 0; i < VOXEL_GI_SHADER_VERSION_MAX; i++) {
			voxel_gi_lighting_shader_version_shaders[i] = voxel_gi_shader.version_get_shader(voxel_gi_lighting_shader_version, i);
			voxel_gi_lighting_shader_version_pipelines[i] = RD::get_singleton()->compute_pipeline_create(voxel_gi_lighting_shader_version_shaders[i]);
		}
	}

	{
		String defines;
		Vector<String> versions;
		versions.push_back("\n#define MODE_DEBUG_COLOR\n");
		versions.push_back("\n#define MODE_DEBUG_LIGHT\n");
		versions.push_back("\n#define MODE_DEBUG_EMISSION\n");
		versions.push_back("\n#define MODE_DEBUG_LIGHT\n#define MODE_DEBUG_LIGHT_FULL\n");

		voxel_gi_debug_shader.initialize(versions, defines);
		voxel_gi_debug_shader_version = voxel_gi_debug_shader.version_create();
		for (int i = 0; i < VOXEL_GI_DEBUG_MAX; i++) {
			voxel_gi_debug_shader_version_shaders[i] = voxel_gi_debug_shader.version_get_shader(voxel_gi_debug_shader_version, i);

			RD::PipelineRasterizationState rs;
			rs.cull_mode = RD::POLYGON_CULL_FRONT;
			RD::PipelineDepthStencilState ds;
			ds.enable_depth_test = true;
			ds.enable_depth_write = true;
			ds.depth_compare_operator = RD::COMPARE_OP_GREATER_OR_EQUAL;

			voxel_gi_debug_shader_version_pipelines[i].setup(voxel_gi_debug_shader_version_shaders[i], RD::RENDER_PRIMITIVE_TRIANGLES, rs, RD::PipelineMultisampleState(), ds, RD::PipelineColorBlendState::create_disabled(), 0);
		}
	}

	/* SDGFI */

	{
		Vector<String> preprocess_modes;
		preprocess_modes.push_back("\n#define MODE_SCROLL\n");
		preprocess_modes.push_back("\n#define MODE_SCROLL_OCCLUSION\n");
		preprocess_modes.push_back("\n#define MODE_INITIALIZE_JUMP_FLOOD\n");
		preprocess_modes.push_back("\n#define MODE_INITIALIZE_JUMP_FLOOD_HALF\n");
		preprocess_modes.push_back("\n#define MODE_JUMPFLOOD\n");
		preprocess_modes.push_back("\n#define MODE_JUMPFLOOD_OPTIMIZED\n");
		preprocess_modes.push_back("\n#define MODE_UPSCALE_JUMP_FLOOD\n");
		preprocess_modes.push_back("\n#define MODE_OCCLUSION\n");
		preprocess_modes.push_back("\n#define MODE_STORE\n");
		String defines = "\n#define OCCLUSION_SIZE " + itos(SDFGI::CASCADE_SIZE / SDFGI::PROBE_DIVISOR) + "\n";
		sdfgi_shader.preprocess.initialize(preprocess_modes, defines);
		sdfgi_shader.preprocess_shader = sdfgi_shader.preprocess.version_create();
		for (int i = 0; i < SDFGIShader::PRE_PROCESS_MAX; i++) {
			sdfgi_shader.preprocess_pipeline[i] = RD::get_singleton()->compute_pipeline_create(sdfgi_shader.preprocess.version_get_shader(sdfgi_shader.preprocess_shader, i));
		}
	}

	{
		//calculate tables
		String defines = "\n#define OCT_SIZE " + itos(SDFGI::LIGHTPROBE_OCT_SIZE) + "\n";

		Vector<String> direct_light_modes;
		direct_light_modes.push_back("\n#define MODE_PROCESS_STATIC\n");
		direct_light_modes.push_back("\n#define MODE_PROCESS_DYNAMIC\n");
		sdfgi_shader.direct_light.initialize(direct_light_modes, defines);
		sdfgi_shader.direct_light_shader = sdfgi_shader.direct_light.version_create();
		for (int i = 0; i < SDFGIShader::DIRECT_LIGHT_MODE_MAX; i++) {
			sdfgi_shader.direct_light_pipeline[i] = RD::get_singleton()->compute_pipeline_create(sdfgi_shader.direct_light.version_get_shader(sdfgi_shader.direct_light_shader, i));
		}
	}

	{
		//calculate tables
		String defines = "\n#define OCT_SIZE " + itos(SDFGI::LIGHTPROBE_OCT_SIZE) + "\n";
		defines += "\n#define SH_SIZE " + itos(SDFGI::SH_SIZE) + "\n";
		if (p_sky->sky_use_cubemap_array) {
			defines += "\n#define USE_CUBEMAP_ARRAY\n";
		}

		Vector<String> integrate_modes;
		integrate_modes.push_back("\n#define MODE_PROCESS\n");
		integrate_modes.push_back("\n#define MODE_STORE\n");
		integrate_modes.push_back("\n#define MODE_SCROLL\n");
		integrate_modes.push_back("\n#define MODE_SCROLL_STORE\n");
		sdfgi_shader.integrate.initialize(integrate_modes, defines);
		sdfgi_shader.integrate_shader = sdfgi_shader.integrate.version_create();

		for (int i = 0; i < SDFGIShader::INTEGRATE_MODE_MAX; i++) {
			sdfgi_shader.integrate_pipeline[i] = RD::get_singleton()->compute_pipeline_create(sdfgi_shader.integrate.version_get_shader(sdfgi_shader.integrate_shader, i));
		}

		{
			Vector<RD::Uniform> uniforms;

			{
				RD::Uniform u;
				u.uniform_type = RD::UNIFORM_TYPE_TEXTURE;
				u.binding = 0;
				if (p_sky->sky_use_cubemap_array) {
					u.append_id(texture_storage->texture_rd_get_default(RendererRD::TextureStorage::DEFAULT_RD_TEXTURE_CUBEMAP_ARRAY_WHITE));
				} else {
					u.append_id(texture_storage->texture_rd_get_default(RendererRD::TextureStorage::DEFAULT_RD_TEXTURE_CUBEMAP_WHITE));
				}
				uniforms.push_back(u);
			}
			{
				RD::Uniform u;
				u.uniform_type = RD::UNIFORM_TYPE_SAMPLER;
				u.binding = 1;
				u.append_id(material_storage->sampler_rd_get_default(RS::CANVAS_ITEM_TEXTURE_FILTER_LINEAR_WITH_MIPMAPS, RS::CANVAS_ITEM_TEXTURE_REPEAT_DISABLED));
				uniforms.push_back(u);
			}

			sdfgi_shader.integrate_default_sky_uniform_set = RD::get_singleton()->uniform_set_create(uniforms, sdfgi_shader.integrate.version_get_shader(sdfgi_shader.integrate_shader, 0), 1);
		}
	}

	//GK
	{
		//calculate tables
		String defines = "\n#define SDFGI_OCT_SIZE " + itos(SDFGI::LIGHTPROBE_OCT_SIZE) + "\n";
		if (RendererSceneRenderRD::get_singleton()->is_vrs_supported()) {
			defines += "\n#define USE_VRS\n";
		}
		if (!RD::get_singleton()->sampler_is_format_supported_for_filter(RD::DATA_FORMAT_R8G8_UINT, RD::SAMPLER_FILTER_LINEAR)) {
			defines += "\n#define SAMPLE_VOXEL_GI_NEAREST\n";
		}

		Vector<String> gi_modes;

		gi_modes.push_back("\n#define USE_VOXEL_GI_INSTANCES\n"); // MODE_VOXEL_GI
		gi_modes.push_back("\n#define USE_SDFGI\n"); // MODE_SDFGI
		gi_modes.push_back("\n#define USE_SDFGI\n\n#define USE_VOXEL_GI_INSTANCES\n"); // MODE_COMBINED

		shader.initialize(gi_modes, defines);
		shader_version = shader.version_create();

		Vector<RD::PipelineSpecializationConstant> specialization_constants;

		{
			RD::PipelineSpecializationConstant sc;
			sc.type = RD::PIPELINE_SPECIALIZATION_CONSTANT_TYPE_BOOL;
			sc.constant_id = 0; // SHADER_SPECIALIZATION_HALF_RES
			sc.bool_value = false;
			specialization_constants.push_back(sc);

			sc.type = RD::PIPELINE_SPECIALIZATION_CONSTANT_TYPE_BOOL;
			sc.constant_id = 1; // SHADER_SPECIALIZATION_USE_FULL_PROJECTION_MATRIX
			sc.bool_value = false;
			specialization_constants.push_back(sc);

			sc.type = RD::PIPELINE_SPECIALIZATION_CONSTANT_TYPE_BOOL;
			sc.constant_id = 2; // SHADER_SPECIALIZATION_USE_VRS
			sc.bool_value = false;
			specialization_constants.push_back(sc);
		}

		for (int v = 0; v < SHADER_SPECIALIZATION_VARIATIONS; v++) {
			specialization_constants.ptrw()[0].bool_value = (v & SHADER_SPECIALIZATION_HALF_RES) ? true : false;
			specialization_constants.ptrw()[1].bool_value = (v & SHADER_SPECIALIZATION_USE_FULL_PROJECTION_MATRIX) ? true : false;
			specialization_constants.ptrw()[2].bool_value = (v & SHADER_SPECIALIZATION_USE_VRS) ? true : false;
			for (int i = 0; i < MODE_MAX; i++) {
				pipelines[v][i] = RD::get_singleton()->compute_pipeline_create(shader.version_get_shader(shader_version, i), specialization_constants);
			}
		}

		sdfgi_ubo = RD::get_singleton()->uniform_buffer_create(sizeof(SDFGIData));
	}
	{
		String defines = "\n#define OCT_SIZE " + itos(SDFGI::LIGHTPROBE_OCT_SIZE) + "\n";
		Vector<String> debug_modes;
		debug_modes.push_back("");
		sdfgi_shader.debug.initialize(debug_modes, defines);
		sdfgi_shader.debug_shader = sdfgi_shader.debug.version_create();
		sdfgi_shader.debug_shader_version = sdfgi_shader.debug.version_get_shader(sdfgi_shader.debug_shader, 0);
		sdfgi_shader.debug_pipeline = RD::get_singleton()->compute_pipeline_create(sdfgi_shader.debug_shader_version);
	}
	{
		String defines = "\n#define OCT_SIZE " + itos(SDFGI::LIGHTPROBE_OCT_SIZE) + "\n";

		Vector<String> versions;
		versions.push_back("\n#define MODE_PROBES\n");
		versions.push_back("\n#define MODE_PROBES\n#define USE_MULTIVIEW\n");
		versions.push_back("\n#define MODE_VISIBILITY\n");
		versions.push_back("\n#define MODE_VISIBILITY\n#define USE_MULTIVIEW\n");

		sdfgi_shader.debug_probes.initialize(versions, defines);

		// TODO disable multiview versions if turned off

		sdfgi_shader.debug_probes_shader = sdfgi_shader.debug_probes.version_create();

		{
			RD::PipelineRasterizationState rs;
			rs.cull_mode = RD::POLYGON_CULL_DISABLED;
			RD::PipelineDepthStencilState ds;
			ds.enable_depth_test = true;
			ds.enable_depth_write = true;
			ds.depth_compare_operator = RD::COMPARE_OP_GREATER_OR_EQUAL;
			for (int i = 0; i < SDFGIShader::PROBE_DEBUG_MAX; i++) {
				// TODO check if version is enabled

				RID debug_probes_shader_version = sdfgi_shader.debug_probes.version_get_shader(sdfgi_shader.debug_probes_shader, i);
				sdfgi_shader.debug_probes_pipeline[i].setup(debug_probes_shader_version, RD::RENDER_PRIMITIVE_TRIANGLE_STRIPS, rs, RD::PipelineMultisampleState(), ds, RD::PipelineColorBlendState::create_disabled(), 0);
			}
		}
	}
	default_voxel_gi_buffer = RD::get_singleton()->uniform_buffer_create(sizeof(VoxelGIData) * MAX_VOXEL_GI_INSTANCES);
	half_resolution = GLOBAL_GET("rendering/global_illumination/gi/use_half_resolution");
}

void GI::free() {
	if (default_voxel_gi_buffer.is_valid()) {
		RD::get_singleton()->free(default_voxel_gi_buffer);
	}
	if (voxel_gi_lights_uniform.is_valid()) {
		RD::get_singleton()->free(voxel_gi_lights_uniform);
	}
	if (sdfgi_ubo.is_valid()) {
		RD::get_singleton()->free(sdfgi_ubo);
	}

	if (voxel_gi_debug_shader_version.is_valid()) {
		voxel_gi_debug_shader.version_free(voxel_gi_debug_shader_version);
	}
	if (voxel_gi_lighting_shader_version.is_valid()) {
		voxel_gi_shader.version_free(voxel_gi_lighting_shader_version);
	}
	if (shader_version.is_valid()) {
		shader.version_free(shader_version);
	}
	if (sdfgi_shader.debug_probes_shader.is_valid()) {
		sdfgi_shader.debug_probes.version_free(sdfgi_shader.debug_probes_shader);
	}
	if (sdfgi_shader.debug_shader.is_valid()) {
		sdfgi_shader.debug.version_free(sdfgi_shader.debug_shader);
	}
	if (sdfgi_shader.direct_light_shader.is_valid()) {
		sdfgi_shader.direct_light.version_free(sdfgi_shader.direct_light_shader);
	}
	if (sdfgi_shader.integrate_shader.is_valid()) {
		sdfgi_shader.integrate.version_free(sdfgi_shader.integrate_shader);
	}
	if (sdfgi_shader.preprocess_shader.is_valid()) {
		sdfgi_shader.preprocess.version_free(sdfgi_shader.preprocess_shader);
	}

	if (voxel_gi_lights) {
		memdelete_arr(voxel_gi_lights);
	}
}

Ref<GI::SDFGI> GI::create_sdfgi(RID p_env, const Vector3 &p_world_position, uint32_t p_requested_history_size) {
	Ref<SDFGI> sdfgi;
	sdfgi.instantiate();

	sdfgi->create(p_env, p_world_position, p_requested_history_size, this);

	return sdfgi;
}

void GI::setup_voxel_gi_instances(RenderDataRD *p_render_data, Ref<RenderSceneBuffersRD> p_render_buffers, const Transform3D &p_transform, const PagedArray<RID> &p_voxel_gi_instances, uint32_t &r_voxel_gi_instances_used) {
	ERR_FAIL_COND(p_render_buffers.is_null());

	RendererRD::TextureStorage *texture_storage = RendererRD::TextureStorage::get_singleton();
	ERR_FAIL_NULL(texture_storage);

	r_voxel_gi_instances_used = 0;

	Ref<RenderBuffersGI> rbgi = p_render_buffers->get_custom_data(RB_SCOPE_GI);
	ERR_FAIL_COND(rbgi.is_null());

	RID voxel_gi_buffer = rbgi->get_voxel_gi_buffer();
	VoxelGIData voxel_gi_data[MAX_VOXEL_GI_INSTANCES];

	bool voxel_gi_instances_changed = false;

	Transform3D to_camera;
	to_camera.origin = p_transform.origin; //only translation, make local

	for (int i = 0; i < MAX_VOXEL_GI_INSTANCES; i++) {
		RID texture;
		if (i < (int)p_voxel_gi_instances.size()) {
			VoxelGIInstance *gipi = voxel_gi_instance_owner.get_or_null(p_voxel_gi_instances[i]);

			if (gipi) {
				texture = gipi->texture;
				VoxelGIData &gipd = voxel_gi_data[i];

				RID base_probe = gipi->probe;

				Transform3D to_cell = voxel_gi_get_to_cell_xform(gipi->probe) * gipi->transform.affine_inverse() * to_camera;

				gipd.xform[0] = to_cell.basis.rows[0][0];
				gipd.xform[1] = to_cell.basis.rows[1][0];
				gipd.xform[2] = to_cell.basis.rows[2][0];
				gipd.xform[3] = 0;
				gipd.xform[4] = to_cell.basis.rows[0][1];
				gipd.xform[5] = to_cell.basis.rows[1][1];
				gipd.xform[6] = to_cell.basis.rows[2][1];
				gipd.xform[7] = 0;
				gipd.xform[8] = to_cell.basis.rows[0][2];
				gipd.xform[9] = to_cell.basis.rows[1][2];
				gipd.xform[10] = to_cell.basis.rows[2][2];
				gipd.xform[11] = 0;
				gipd.xform[12] = to_cell.origin.x;
				gipd.xform[13] = to_cell.origin.y;
				gipd.xform[14] = to_cell.origin.z;
				gipd.xform[15] = 1;

				Vector3 bounds = voxel_gi_get_octree_size(base_probe);

				gipd.bounds[0] = bounds.x;
				gipd.bounds[1] = bounds.y;
				gipd.bounds[2] = bounds.z;

				gipd.dynamic_range = voxel_gi_get_dynamic_range(base_probe) * voxel_gi_get_energy(base_probe);
				gipd.bias = voxel_gi_get_bias(base_probe);
				gipd.normal_bias = voxel_gi_get_normal_bias(base_probe);
				gipd.blend_ambient = !voxel_gi_is_interior(base_probe);
				gipd.mipmaps = gipi->mipmaps.size();
				gipd.exposure_normalization = 1.0;
				if (p_render_data->camera_attributes.is_valid()) {
					float exposure_normalization = RSG::camera_attributes->camera_attributes_get_exposure_normalization_factor(p_render_data->camera_attributes);
					gipd.exposure_normalization = exposure_normalization / voxel_gi_get_baked_exposure_normalization(base_probe);
				}
			}

			r_voxel_gi_instances_used++;
		}

		if (texture == RID()) {
			texture = texture_storage->texture_rd_get_default(RendererRD::TextureStorage::DEFAULT_RD_TEXTURE_3D_WHITE);
		}

		if (texture != rbgi->voxel_gi_textures[i]) {
			voxel_gi_instances_changed = true;
			rbgi->voxel_gi_textures[i] = texture;
		}
	}

	if (voxel_gi_instances_changed) {
		for (uint32_t v = 0; v < RendererSceneRender::MAX_RENDER_VIEWS; v++) {
			if (RD::get_singleton()->uniform_set_is_valid(rbgi->uniform_set[v])) {
				RD::get_singleton()->free(rbgi->uniform_set[v]);
			}
			rbgi->uniform_set[v] = RID();
		}

		if (p_render_buffers->has_custom_data(RB_SCOPE_FOG)) {
			// VoxelGI instances have changed, so we need to update volumetric fog.
			Ref<RendererRD::Fog::VolumetricFog> fog = p_render_buffers->get_custom_data(RB_SCOPE_FOG);
			fog->sync_gi_dependent_sets_validity(true);
		}
	}

	if (p_voxel_gi_instances.size() > 0) {
		RD::get_singleton()->draw_command_begin_label("VoxelGIs Setup");

		RD::get_singleton()->buffer_update(voxel_gi_buffer, 0, sizeof(VoxelGIData) * MIN((uint64_t)MAX_VOXEL_GI_INSTANCES, p_voxel_gi_instances.size()), voxel_gi_data);

		RD::get_singleton()->draw_command_end_label();
	}
}

RID GI::RenderBuffersGI::get_voxel_gi_buffer() {
	if (voxel_gi_buffer.is_null()) {
		voxel_gi_buffer = RD::get_singleton()->uniform_buffer_create(sizeof(GI::VoxelGIData) * GI::MAX_VOXEL_GI_INSTANCES);
	}
	return voxel_gi_buffer;
}

void GI::RenderBuffersGI::free_data() {
	for (uint32_t v = 0; v < RendererSceneRender::MAX_RENDER_VIEWS; v++) {
		if (RD::get_singleton()->uniform_set_is_valid(uniform_set[v])) {
			RD::get_singleton()->free(uniform_set[v]);
		}
		uniform_set[v] = RID();
	}

	if (scene_data_ubo.is_valid()) {
		RD::get_singleton()->free(scene_data_ubo);
		scene_data_ubo = RID();
	}

	if (voxel_gi_buffer.is_valid()) {
		RD::get_singleton()->free(voxel_gi_buffer);
		voxel_gi_buffer = RID();
	}
}

void GI::process_gi(Ref<RenderSceneBuffersRD> p_render_buffers, const RID *p_normal_roughness_slices, RID p_voxel_gi_buffer, RID p_environment, uint32_t p_view_count, const Projection *p_projections, const Vector3 *p_eye_offsets, const Transform3D &p_cam_transform, const PagedArray<RID> &p_voxel_gi_instances) {
	RendererRD::TextureStorage *texture_storage = RendererRD::TextureStorage::get_singleton();
	RendererRD::MaterialStorage *material_storage = RendererRD::MaterialStorage::get_singleton();

	ERR_FAIL_COND_MSG(p_view_count > 2, "Maximum of 2 views supported for Processing GI.");

	RD::get_singleton()->draw_command_begin_label("GI Render");

	ERR_FAIL_COND(p_render_buffers.is_null());

	Ref<RenderBuffersGI> rbgi = p_render_buffers->get_custom_data(RB_SCOPE_GI);
	ERR_FAIL_COND(rbgi.is_null());

	Size2i internal_size = p_render_buffers->get_internal_size();

	if (rbgi->using_half_size_gi != half_resolution) {
		p_render_buffers->clear_context(RB_SCOPE_GI);
	}

	if (!p_render_buffers->has_texture(RB_SCOPE_GI, RB_TEX_AMBIENT)) {
		Size2i size = internal_size;
		uint32_t usage_bits = RD::TEXTURE_USAGE_SAMPLING_BIT | RD::TEXTURE_USAGE_STORAGE_BIT;

		if (half_resolution) {
			size.x >>= 1;
			size.y >>= 1;
		}

		p_render_buffers->create_texture(RB_SCOPE_GI, RB_TEX_AMBIENT, RD::DATA_FORMAT_R16G16B16A16_SFLOAT, usage_bits, RD::TEXTURE_SAMPLES_1, size);
		p_render_buffers->create_texture(RB_SCOPE_GI, RB_TEX_REFLECTION, RD::DATA_FORMAT_R16G16B16A16_SFLOAT, usage_bits, RD::TEXTURE_SAMPLES_1, size);

		rbgi->using_half_size_gi = half_resolution;
	}

	// Setup our scene data
	{
		SceneData scene_data;

		if (rbgi->scene_data_ubo.is_null()) {
			rbgi->scene_data_ubo = RD::get_singleton()->uniform_buffer_create(sizeof(SceneData));
		}

		Projection correction;
		correction.set_depth_correction(false);

		for (uint32_t v = 0; v < p_view_count; v++) {
			Projection temp = correction * p_projections[v];

			RendererRD::MaterialStorage::store_camera(temp.inverse(), scene_data.inv_projection[v]);
			scene_data.eye_offset[v][0] = p_eye_offsets[v].x;
			scene_data.eye_offset[v][1] = p_eye_offsets[v].y;
			scene_data.eye_offset[v][2] = p_eye_offsets[v].z;
			scene_data.eye_offset[v][3] = 0.0;
		}

		// Note that we will be ignoring the origin of this transform.
		RendererRD::MaterialStorage::store_transform(p_cam_transform, scene_data.cam_transform);

		scene_data.screen_size[0] = internal_size.x;
		scene_data.screen_size[1] = internal_size.y;

		RD::get_singleton()->buffer_update(rbgi->scene_data_ubo, 0, sizeof(SceneData), &scene_data);
	}

	// Now compute the contents of our buffers.
	RD::ComputeListID compute_list = RD::get_singleton()->compute_list_begin();

	// Render each eye separately.
	// We need to look into whether we can make our compute shader use Multiview but not sure that works or makes a difference..

	// setup our push constant

	PushConstant push_constant;

	push_constant.max_voxel_gi_instances = MIN((uint64_t)MAX_VOXEL_GI_INSTANCES, p_voxel_gi_instances.size());
	push_constant.high_quality_vct = voxel_gi_quality == RS::VOXEL_GI_QUALITY_HIGH;

	// these should be the same for all views
	push_constant.orthogonal = p_projections[0].is_orthogonal();
	push_constant.z_near = p_projections[0].get_z_near();
	push_constant.z_far = p_projections[0].get_z_far();

	// these are only used if we have 1 view, else we use the projections in our scene data
	push_constant.proj_info[0] = -2.0f / (internal_size.x * p_projections[0].columns[0][0]);
	push_constant.proj_info[1] = -2.0f / (internal_size.y * p_projections[0].columns[1][1]);
	push_constant.proj_info[2] = (1.0f - p_projections[0].columns[0][2]) / p_projections[0].columns[0][0];
	push_constant.proj_info[3] = (1.0f + p_projections[0].columns[1][2]) / p_projections[0].columns[1][1];

	bool use_sdfgi = p_render_buffers->has_custom_data(RB_SCOPE_SDFGI);
	bool use_voxel_gi_instances = push_constant.max_voxel_gi_instances > 0;

	Ref<SDFGI> sdfgi;
	if (use_sdfgi) {
		sdfgi = p_render_buffers->get_custom_data(RB_SCOPE_SDFGI);
	}

	uint32_t pipeline_specialization = 0;
	if (rbgi->using_half_size_gi) {
		pipeline_specialization |= SHADER_SPECIALIZATION_HALF_RES;
	}
	if (p_view_count > 1) {
		pipeline_specialization |= SHADER_SPECIALIZATION_USE_FULL_PROJECTION_MATRIX;
	}
	bool has_vrs_texture = p_render_buffers->has_texture(RB_SCOPE_VRS, RB_TEXTURE);
	if (has_vrs_texture) {
		pipeline_specialization |= SHADER_SPECIALIZATION_USE_VRS;
	}

	Mode mode = (use_sdfgi && use_voxel_gi_instances) ? MODE_COMBINED : (use_sdfgi ? MODE_SDFGI : MODE_VOXEL_GI);

	for (uint32_t v = 0; v < p_view_count; v++) {
		push_constant.view_index = v;

		// setup our uniform set
		if (rbgi->uniform_set[v].is_null() || !RD::get_singleton()->uniform_set_is_valid(rbgi->uniform_set[v])) {
			Vector<RD::Uniform> uniforms;
			{
				RD::Uniform u;
				u.binding = 1;
				u.uniform_type = RD::UNIFORM_TYPE_TEXTURE;
				for (uint32_t j = 0; j < SDFGI::MAX_CASCADES; j++) {
					if (use_sdfgi && j < sdfgi->cascades.size()) {
						u.append_id(sdfgi->cascades[j].sdf_tex);
					} else {
						u.append_id(texture_storage->texture_rd_get_default(RendererRD::TextureStorage::DEFAULT_RD_TEXTURE_3D_WHITE));
					}
				}
				uniforms.push_back(u);
			}
			{
				RD::Uniform u;
				u.binding = 2;
				u.uniform_type = RD::UNIFORM_TYPE_TEXTURE;
				for (uint32_t j = 0; j < SDFGI::MAX_CASCADES; j++) {
					if (use_sdfgi && j < sdfgi->cascades.size()) {
						u.append_id(sdfgi->cascades[j].light_tex);
					} else {
						u.append_id(texture_storage->texture_rd_get_default(RendererRD::TextureStorage::DEFAULT_RD_TEXTURE_3D_WHITE));
					}
				}
				uniforms.push_back(u);
			}
			{
				RD::Uniform u;
				u.binding = 3;
				u.uniform_type = RD::UNIFORM_TYPE_TEXTURE;
				for (uint32_t j = 0; j < SDFGI::MAX_CASCADES; j++) {
					if (use_sdfgi && j < sdfgi->cascades.size()) {
						u.append_id(sdfgi->cascades[j].light_aniso_0_tex);
					} else {
						u.append_id(texture_storage->texture_rd_get_default(RendererRD::TextureStorage::DEFAULT_RD_TEXTURE_3D_WHITE));
					}
				}
				uniforms.push_back(u);
			}
			{
				RD::Uniform u;
				u.binding = 4;
				u.uniform_type = RD::UNIFORM_TYPE_TEXTURE;
				for (uint32_t j = 0; j < SDFGI::MAX_CASCADES; j++) {
					if (use_sdfgi && j < sdfgi->cascades.size()) {
						u.append_id(sdfgi->cascades[j].light_aniso_1_tex);
					} else {
						u.append_id(texture_storage->texture_rd_get_default(RendererRD::TextureStorage::DEFAULT_RD_TEXTURE_3D_WHITE));
					}
				}
				uniforms.push_back(u);
			}
			{
				RD::Uniform u;
				u.uniform_type = RD::UNIFORM_TYPE_TEXTURE;
				u.binding = 5;
				if (use_sdfgi) {
					u.append_id(sdfgi->occlusion_texture);
				} else {
					u.append_id(texture_storage->texture_rd_get_default(RendererRD::TextureStorage::DEFAULT_RD_TEXTURE_3D_WHITE));
				}
				uniforms.push_back(u);
			}
			{
				RD::Uniform u;
				u.uniform_type = RD::UNIFORM_TYPE_SAMPLER;
				u.binding = 6;
				u.append_id(material_storage->sampler_rd_get_default(RS::CANVAS_ITEM_TEXTURE_FILTER_LINEAR, RS::CANVAS_ITEM_TEXTURE_REPEAT_DISABLED));
				uniforms.push_back(u);
			}
			{
				RD::Uniform u;
				u.uniform_type = RD::UNIFORM_TYPE_SAMPLER;
				u.binding = 7;
				u.append_id(material_storage->sampler_rd_get_default(RS::CANVAS_ITEM_TEXTURE_FILTER_LINEAR_WITH_MIPMAPS, RS::CANVAS_ITEM_TEXTURE_REPEAT_DISABLED));
				uniforms.push_back(u);
			}
			{
				RD::Uniform u;
				u.uniform_type = RD::UNIFORM_TYPE_IMAGE;
				u.binding = 9;
				u.append_id(p_render_buffers->get_texture_slice(RB_SCOPE_GI, RB_TEX_AMBIENT, v, 0));
				uniforms.push_back(u);
			}

			{
				RD::Uniform u;
				u.uniform_type = RD::UNIFORM_TYPE_IMAGE;
				u.binding = 10;
				u.append_id(p_render_buffers->get_texture_slice(RB_SCOPE_GI, RB_TEX_REFLECTION, v, 0));
				uniforms.push_back(u);
			}

			{
				RD::Uniform u;
				u.uniform_type = RD::UNIFORM_TYPE_TEXTURE;
				u.binding = 11;
				if (use_sdfgi) {
					u.append_id(sdfgi->lightprobe_texture);
				} else {
					u.append_id(texture_storage->texture_rd_get_default(RendererRD::TextureStorage::DEFAULT_RD_TEXTURE_2D_ARRAY_WHITE));
				}
				uniforms.push_back(u);
			}
			{
				RD::Uniform u;
				u.uniform_type = RD::UNIFORM_TYPE_TEXTURE;
				u.binding = 12;
				u.append_id(p_render_buffers->get_depth_texture(v));
				uniforms.push_back(u);
			}
			{
				RD::Uniform u;
				u.uniform_type = RD::UNIFORM_TYPE_TEXTURE;
				u.binding = 13;
				u.append_id(p_normal_roughness_slices[v]);
				uniforms.push_back(u);
			}
			{
				RD::Uniform u;
				u.uniform_type = RD::UNIFORM_TYPE_TEXTURE;
				u.binding = 14;
				RID buffer = p_voxel_gi_buffer.is_valid() ? p_voxel_gi_buffer : texture_storage->texture_rd_get_default(RendererRD::TextureStorage::DEFAULT_RD_TEXTURE_BLACK);
				u.append_id(buffer);
				uniforms.push_back(u);
			}
			{
				RD::Uniform u;
				u.uniform_type = RD::UNIFORM_TYPE_UNIFORM_BUFFER;
				u.binding = 15;
				u.append_id(sdfgi_ubo);
				uniforms.push_back(u);
			}
			{
				RD::Uniform u;
				u.uniform_type = RD::UNIFORM_TYPE_UNIFORM_BUFFER;
				u.binding = 16;
				u.append_id(rbgi->get_voxel_gi_buffer());
				uniforms.push_back(u);
			}
			{
				RD::Uniform u;
				u.uniform_type = RD::UNIFORM_TYPE_TEXTURE;
				u.binding = 17;
				for (int i = 0; i < MAX_VOXEL_GI_INSTANCES; i++) {
					u.append_id(rbgi->voxel_gi_textures[i]);
				}
				uniforms.push_back(u);
			}
			{
				RD::Uniform u;
				u.uniform_type = RD::UNIFORM_TYPE_UNIFORM_BUFFER;
				u.binding = 18;
				u.append_id(rbgi->scene_data_ubo);
				uniforms.push_back(u);
			}
			if (RendererSceneRenderRD::get_singleton()->is_vrs_supported()) {
				RD::Uniform u;
				u.uniform_type = RD::UNIFORM_TYPE_IMAGE;
				u.binding = 19;
				RID buffer = has_vrs_texture ? p_render_buffers->get_texture_slice(RB_SCOPE_VRS, RB_TEXTURE, v, 0) : texture_storage->texture_rd_get_default(RendererRD::TextureStorage::DEFAULT_RD_TEXTURE_VRS);
				u.append_id(buffer);
				uniforms.push_back(u);
			}

			rbgi->uniform_set[v] = RD::get_singleton()->uniform_set_create(uniforms, shader.version_get_shader(shader_version, 0), 0);
		}

		RD::get_singleton()->compute_list_bind_compute_pipeline(compute_list, pipelines[pipeline_specialization][mode]);
		RD::get_singleton()->compute_list_bind_uniform_set(compute_list, rbgi->uniform_set[v], 0);
		RD::get_singleton()->compute_list_set_push_constant(compute_list, &push_constant, sizeof(PushConstant));

		if (rbgi->using_half_size_gi) {
			RD::get_singleton()->compute_list_dispatch_threads(compute_list, internal_size.x >> 1, internal_size.y >> 1, 1);
		} else {
			RD::get_singleton()->compute_list_dispatch_threads(compute_list, internal_size.x, internal_size.y, 1);
		}
	}

	RD::get_singleton()->compute_list_end();
	RD::get_singleton()->draw_command_end_label();
}

RID GI::voxel_gi_instance_create(RID p_base) {
	VoxelGIInstance voxel_gi;
	voxel_gi.gi = this;
	voxel_gi.probe = p_base;
	RID rid = voxel_gi_instance_owner.make_rid(voxel_gi);
	return rid;
}

void GI::voxel_gi_instance_free(RID p_rid) {
	GI::VoxelGIInstance *voxel_gi = voxel_gi_instance_owner.get_or_null(p_rid);
	voxel_gi->free_resources();
	voxel_gi_instance_owner.free(p_rid);
}

void GI::voxel_gi_instance_set_transform_to_data(RID p_probe, const Transform3D &p_xform) {
	VoxelGIInstance *voxel_gi = voxel_gi_instance_owner.get_or_null(p_probe);
	ERR_FAIL_NULL(voxel_gi);

	voxel_gi->transform = p_xform;
}

bool GI::voxel_gi_needs_update(RID p_probe) const {
	VoxelGIInstance *voxel_gi = voxel_gi_instance_owner.get_or_null(p_probe);
	ERR_FAIL_NULL_V(voxel_gi, false);

	return voxel_gi->last_probe_version != voxel_gi_get_version(voxel_gi->probe);
}

void GI::voxel_gi_update(RID p_probe, bool p_update_light_instances, const Vector<RID> &p_light_instances, const PagedArray<RenderGeometryInstance *> &p_dynamic_objects) {
	VoxelGIInstance *voxel_gi = voxel_gi_instance_owner.get_or_null(p_probe);
	ERR_FAIL_NULL(voxel_gi);

	voxel_gi->update(p_update_light_instances, p_light_instances, p_dynamic_objects);
}

void GI::debug_voxel_gi(RID p_voxel_gi, RD::DrawListID p_draw_list, RID p_framebuffer, const Projection &p_camera_with_transform, bool p_lighting, bool p_emission, float p_alpha) {
	VoxelGIInstance *voxel_gi = voxel_gi_instance_owner.get_or_null(p_voxel_gi);
	ERR_FAIL_NULL(voxel_gi);

	voxel_gi->debug(p_draw_list, p_framebuffer, p_camera_with_transform, p_lighting, p_emission, p_alpha);
}

/**************************************************************************/
/*  fog.h                                                                 */
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

#include "core/templates/local_vector.h"
#include "core/templates/rid_owner.h"
#include "servers/rendering/environment/renderer_fog.h"
#include "servers/rendering/renderer_rd/cluster_builder_rd.h"
#include "servers/rendering/renderer_rd/environment/gi.h"
#include "servers/rendering/renderer_rd/shaders/environment/volumetric_fog.glsl.gen.h"
#include "servers/rendering/renderer_rd/shaders/environment/volumetric_fog_process.glsl.gen.h"
#include "servers/rendering/renderer_rd/storage_rd/render_buffer_custom_data_rd.h"
#include "servers/rendering/storage/utilities.h"

#define RB_SCOPE_FOG SNAME("Fog")

namespace RendererRD {

class Fog : public RendererFog {
private:
	static Fog *singleton;

	/* FOG VOLUMES */

	struct FogVolume {
		RID material;
		Vector3 size = Vector3(2, 2, 2);

		RS::FogVolumeShape shape = RS::FOG_VOLUME_SHAPE_BOX;

		Dependency dependency;
	};

	mutable RID_Owner<FogVolume, true> fog_volume_owner;

	struct FogVolumeInstance {
		RID volume;
		Transform3D transform;
		bool active = false;
	};

	mutable RID_Owner<FogVolumeInstance> fog_volume_instance_owner;

	const int SAMPLERS_BINDING_FIRST_INDEX = 3;

	/* Volumetric Fog */
	struct VolumetricFogShader {
		enum FogSet {
			FOG_SET_BASE,
			FOG_SET_UNIFORMS,
			FOG_SET_MATERIAL,
			FOG_SET_MAX,
		};

		struct FogPushConstant {
			float position[3];
			float pad;

			float size[3];
			float pad2;

			int32_t corner[3];
			uint32_t shape;

			float transform[16];
		};

		struct VolumeUBO {
			float fog_frustum_size_begin[2];
			float fog_frustum_size_end[2];

			float fog_frustum_end;
			float z_near;
			float z_far;
			float time;

			int32_t fog_volume_size[3];
			uint32_t directional_light_count;

			uint32_t use_temporal_reprojection;
			uint32_t temporal_frame;
			float detail_spread;
			float temporal_blend;

			float to_prev_view[16];
			float transform[16];
		};

		ShaderCompiler compiler;
		VolumetricFogShaderRD shader;
		RID volume_ubo;

		RID default_shader;
		RID default_material;
		RID default_shader_rd;

		RID base_uniform_set;

		RID params_ubo;

		enum {
			VOLUMETRIC_FOG_PROCESS_SHADER_DENSITY,
			VOLUMETRIC_FOG_PROCESS_SHADER_DENSITY_WITH_SDFGI,
			VOLUMETRIC_FOG_PROCESS_SHADER_FILTER,
			VOLUMETRIC_FOG_PROCESS_SHADER_FOG,
			VOLUMETRIC_FOG_PROCESS_SHADER_COPY,
			VOLUMETRIC_FOG_PROCESS_SHADER_MAX,
		};

		struct ParamsUBO {
			float fog_frustum_size_begin[2];
			float fog_frustum_size_end[2];

			float fog_frustum_end;
			float ambient_inject;
			float z_far;
			uint32_t filter_axis;

			float ambient_color[3];
			float sky_contribution;

			int32_t fog_volume_size[3];
			uint32_t directional_light_count;

			float base_emission[3];
			float base_density;

			float base_scattering[3];
			float phase_g;

			float detail_spread;
			float gi_inject;
			uint32_t max_voxel_gi_instances;
			uint32_t cluster_type_size;

			float screen_size[2];
			uint32_t cluster_shift;
			uint32_t cluster_width;

			uint32_t max_cluster_element_count_div_32;
			uint32_t use_temporal_reprojection;
			uint32_t temporal_frame;
			float temporal_blend;

			float cam_rotation[12];
			float to_prev_view[16];
			float radiance_inverse_xform[12];
		};

		VolumetricFogProcessShaderRD process_shader;

		RID process_shader_version;
		RID process_pipelines[VOLUMETRIC_FOG_PROCESS_SHADER_MAX];

	} volumetric_fog;

	Vector3i _point_get_position_in_froxel_volume(const Vector3 &p_point, float fog_end, const Vector2 &fog_near_size, const Vector2 &fog_far_size, float volumetric_fog_detail_spread, const Vector3 &fog_size, const Transform3D &p_cam_transform);

	struct FogShaderData : public RendererRD::MaterialStorage::ShaderData {
		bool valid = false;
		RID version;

		RID pipeline;
		Vector<ShaderCompiler::GeneratedCode::Texture> texture_uniforms;

		Vector<uint32_t> ubo_offsets;
		uint32_t ubo_size = 0;

		String code;

		bool uses_time = false;

		virtual void set_code(const String &p_Code);
		virtual bool is_animated() const;
		virtual bool casts_shadows() const;
		virtual RS::ShaderNativeSourceCode get_native_source_code() const;

		FogShaderData() {}
		virtual ~FogShaderData();
	};

	struct FogMaterialData : public RendererRD::MaterialStorage::MaterialData {
		FogShaderData *shader_data = nullptr;
		RID uniform_set;
		bool uniform_set_updated;

		virtual void set_render_priority(int p_priority) {}
		virtual void set_next_pass(RID p_pass) {}
		virtual bool update_parameters(const HashMap<StringName, Variant> &p_parameters, bool p_uniform_dirty, bool p_textures_dirty);
		virtual ~FogMaterialData();
	};

	RendererRD::MaterialStorage::ShaderData *_create_fog_shader_func();
	static RendererRD::MaterialStorage::ShaderData *_create_fog_shader_funcs();

	RendererRD::MaterialStorage::MaterialData *_create_fog_material_func(FogShaderData *p_shader);
	static RendererRD::MaterialStorage::MaterialData *_create_fog_material_funcs(RendererRD::MaterialStorage::ShaderData *p_shader);

public:
	static Fog *get_singleton() { return singleton; }

	Fog();
	~Fog();

	/* FOG VOLUMES */

	bool owns_fog_volume(RID p_rid) { return fog_volume_owner.owns(p_rid); }

	virtual RID fog_volume_allocate() override;
	virtual void fog_volume_initialize(RID p_rid) override;
	virtual void fog_volume_free(RID p_rid) override;
	Dependency *fog_volume_get_dependency(RID p_fog_volume) const;

	virtual void fog_volume_set_shape(RID p_fog_volume, RS::FogVolumeShape p_shape) override;
	virtual void fog_volume_set_size(RID p_fog_volume, const Vector3 &p_size) override;
	virtual void fog_volume_set_material(RID p_fog_volume, RID p_material) override;
	virtual RS::FogVolumeShape fog_volume_get_shape(RID p_fog_volume) const override;
	RID fog_volume_get_material(RID p_fog_volume) const;
	virtual AABB fog_volume_get_aabb(RID p_fog_volume) const override;
	Vector3 fog_volume_get_size(RID p_fog_volume) const;

	/* FOG VOLUMES INSTANCE */

	bool owns_fog_volume_instance(RID p_rid) { return fog_volume_instance_owner.owns(p_rid); }

	RID fog_volume_instance_create(RID p_fog_volume);
	void fog_instance_free(RID p_rid);

	void fog_volume_instance_set_transform(RID p_fog_volume_instance, const Transform3D &p_transform) {
		Fog::FogVolumeInstance *fvi = fog_volume_instance_owner.get_or_null(p_fog_volume_instance);
		ERR_FAIL_NULL(fvi);
		fvi->transform = p_transform;
	}

	void fog_volume_instance_set_active(RID p_fog_volume_instance, bool p_active) {
		Fog::FogVolumeInstance *fvi = fog_volume_instance_owner.get_or_null(p_fog_volume_instance);
		ERR_FAIL_NULL(fvi);
		fvi->active = p_active;
	}

	RID fog_volume_instance_get_volume(RID p_fog_volume_instance) const {
		Fog::FogVolumeInstance *fvi = fog_volume_instance_owner.get_or_null(p_fog_volume_instance);
		ERR_FAIL_NULL_V(fvi, RID());
		return fvi->volume;
	}

	Vector3 fog_volume_instance_get_position(RID p_fog_volume_instance) const {
		Fog::FogVolumeInstance *fvi = fog_volume_instance_owner.get_or_null(p_fog_volume_instance);
		ERR_FAIL_NULL_V(fvi, Vector3());
		return fvi->transform.get_origin();
	}

	/* Volumetric FOG */
	class VolumetricFog : public RenderBufferCustomDataRD {
		GDCLASS(VolumetricFog, RenderBufferCustomDataRD)

	public:
		enum {
			MAX_TEMPORAL_FRAMES = 16
		};

		uint32_t width = 0;
		uint32_t height = 0;
		uint32_t depth = 0;

		float length;
		float spread;

		RID light_density_map;
		RID prev_light_density_map;
		RID fog_map;
		RID density_map;
		RID light_map;
		RID emissive_map;

		RID fog_uniform_set;
		RID copy_uniform_set;

		struct {
			RID process_uniform_set_density;
			RID process_uniform_set;
			RID process_uniform_set2;
		} gi_dependent_sets;

		RID sdfgi_uniform_set;
		RID sky_uniform_set;

		int last_shadow_filter = -1;

		virtual void configure(RenderSceneBuffersRD *p_render_buffers) override {}
		virtual void free_data() override {}

		bool sync_gi_dependent_sets_validity(bool p_ensure_freed = false);

		void init(const Vector3i &fog_size, RID p_sky_shader);
		~VolumetricFog();
	};

	void init_fog_shader(uint32_t p_max_directional_lights, int p_roughness_layers, bool p_is_using_radiance_cubemap_array);
	void free_fog_shader();

	struct VolumetricFogSettings {
		Vector2i rb_size;
		double time;
		bool is_using_radiance_cubemap_array;
		uint32_t max_cluster_elements;
		bool volumetric_fog_filter_active;
		RID shadow_sampler;
		RID voxel_gi_buffer;
		RID shadow_atlas_depth;
		RID omni_light_buffer;
		RID spot_light_buffer;
		RID directional_shadow_depth;
		RID directional_light_buffer;

		// Objects related to our render buffer
		Ref<VolumetricFog> vfog;
		ClusterBuilderRD *cluster_builder;
		GI *gi;
		Ref<GI::SDFGI> sdfgi;
		Ref<GI::RenderBuffersGI> rbgi;
		RID env;
		SkyRD *sky;
	};
	void volumetric_fog_update(const VolumetricFogSettings &p_settings, const Projection &p_cam_projection, const Transform3D &p_cam_transform, const Transform3D &p_prev_cam_inv_transform, RID p_shadow_atlas, int p_directional_light_count, bool p_use_directional_shadows, int p_positional_light_count, int p_voxel_gi_count, const PagedArray<RID> &p_fog_volumes);
};

} // namespace RendererRD

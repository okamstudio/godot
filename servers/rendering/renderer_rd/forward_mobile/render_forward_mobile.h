/**************************************************************************/
/*  render_forward_mobile.h                                               */
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

#ifndef RENDER_FORWARD_MOBILE_H
#define RENDER_FORWARD_MOBILE_H

#include "core/templates/paged_allocator.h"
#include "servers/rendering/renderer_rd/forward_mobile/scene_shader_forward_mobile.h"
#include "servers/rendering/renderer_rd/renderer_scene_render_rd.h"

#define RB_SCOPE_MOBILE SNAME("mobile")

namespace RendererSceneRenderImplementation {

class RenderForwardMobile : public RendererSceneRenderRD {
	friend SceneShaderForwardMobile;

protected:
	struct GeometryInstanceSurfaceDataCache;

private:
	static RenderForwardMobile *singleton;

	/* Scene Shader */

	enum {
		SCENE_UNIFORM_SET = 0,
		RENDER_PASS_UNIFORM_SET = 1,
		TRANSFORMS_UNIFORM_SET = 2,
		MATERIAL_UNIFORM_SET = 3,
	};

	enum {
		MAX_LIGHTMAPS = 8,
		MAX_RDL_CULL = 8, // maximum number of reflection probes, decals or lights we can cull per geometry instance
		INSTANCE_DATA_BUFFER_MIN_SIZE = 4096
	};

	enum RenderListType {
		RENDER_LIST_OPAQUE, //used for opaque objects
		RENDER_LIST_ALPHA, //used for transparent objects
		RENDER_LIST_SECONDARY, //used for shadows and other objects
		RENDER_LIST_MAX
	};

	/* Scene Shader */

	SceneShaderForwardMobile scene_shader;

	/* Render Buffer */

	class RenderBufferDataForwardMobile : public RenderBufferCustomDataRD {
		GDCLASS(RenderBufferDataForwardMobile, RenderBufferCustomDataRD);

	public:
		enum FramebufferConfigType {
			FB_CONFIG_RENDER_PASS, // Single pass framebuffer for normal rendering.
			FB_CONFIG_RENDER_AND_POST_PASS, // Two subpasses, one for normal rendering, one for post processing.
			FB_CONFIG_MAX
		};

		RID get_color_fbs(FramebufferConfigType p_config_type);
		virtual void free_data() override;
		virtual void configure(RenderSceneBuffersRD *p_render_buffers) override;

	private:
		RenderSceneBuffersRD *render_buffers = nullptr;
	};

	virtual void setup_render_buffer_data(Ref<RenderSceneBuffersRD> p_render_buffers) override;

	/* Rendering */

	enum PassMode {
		PASS_MODE_COLOR,
		// PASS_MODE_COLOR_SPECULAR,
		PASS_MODE_COLOR_TRANSPARENT,
		PASS_MODE_SHADOW,
		PASS_MODE_SHADOW_DP,
		// PASS_MODE_DEPTH,
		// PASS_MODE_DEPTH_NORMAL_ROUGHNESS,
		// PASS_MODE_DEPTH_NORMAL_ROUGHNESS_VOXEL_GI,
		PASS_MODE_DEPTH_MATERIAL,
		// PASS_MODE_SDF,
	};

	struct RenderElementInfo;

	struct RenderListParameters {
		GeometryInstanceSurfaceDataCache **elements = nullptr;
		RenderElementInfo *element_info = nullptr;
		int element_count = 0;
		bool reverse_cull = false;
		PassMode pass_mode = PASS_MODE_COLOR;
		// bool no_gi = false;
		uint32_t view_count = 1;
		RID render_pass_uniform_set;
		bool force_wireframe = false;
		Vector2 uv_offset;
		SceneShaderForwardMobile::ShaderSpecialization base_specialization;
		float lod_distance_multiplier = 0.0;
		float screen_mesh_lod_threshold = 0.0;
		RD::FramebufferFormatID framebuffer_format = 0;
		uint32_t element_offset = 0;
		uint32_t subpass = 0;

		RenderListParameters(GeometryInstanceSurfaceDataCache **p_elements, RenderElementInfo *p_element_info, int p_element_count, bool p_reverse_cull, PassMode p_pass_mode, RID p_render_pass_uniform_set, SceneShaderForwardMobile::ShaderSpecialization p_base_specialization, bool p_force_wireframe = false, const Vector2 &p_uv_offset = Vector2(), float p_lod_distance_multiplier = 0.0, float p_screen_mesh_lod_threshold = 0.0, uint32_t p_view_count = 1, uint32_t p_element_offset = 0) {
			elements = p_elements;
			element_info = p_element_info;
			element_count = p_element_count;
			reverse_cull = p_reverse_cull;
			pass_mode = p_pass_mode;
			// no_gi = p_no_gi;
			view_count = p_view_count;
			render_pass_uniform_set = p_render_pass_uniform_set;
			force_wireframe = p_force_wireframe;
			uv_offset = p_uv_offset;
			lod_distance_multiplier = p_lod_distance_multiplier;
			screen_mesh_lod_threshold = p_screen_mesh_lod_threshold;
			element_offset = p_element_offset;
			base_specialization = p_base_specialization;
		}
	};

	/* Render shadows */

	void _render_shadow_pass(RID p_light, RID p_shadow_atlas, int p_pass, const PagedArray<RenderGeometryInstance *> &p_instances, float p_lod_distance_multiplier = 0, float p_screen_mesh_lod_threshold = 0.0, bool p_open_pass = true, bool p_close_pass = true, bool p_clear_region = true, RenderingMethod::RenderInfo *p_render_info = nullptr, const Transform3D &p_main_cam_transform = Transform3D());
	void _render_shadow_begin();
	void _render_shadow_append(RID p_framebuffer, const PagedArray<RenderGeometryInstance *> &p_instances, const Projection &p_projection, const Transform3D &p_transform, float p_zfar, float p_bias, float p_normal_bias, bool p_use_dp, bool p_use_dp_flip, bool p_use_pancake, float p_lod_distance_multiplier = 0.0, float p_screen_mesh_lod_threshold = 0.0, const Rect2i &p_rect = Rect2i(), bool p_flip_y = false, bool p_clear_region = true, bool p_begin = true, bool p_end = true, RenderingMethod::RenderInfo *p_render_info = nullptr, const Transform3D &p_main_cam_transform = Transform3D());
	void _render_shadow_process();
	void _render_shadow_end();

	/* Render Scene */

	RID _setup_render_pass_uniform_set(RenderListType p_render_list, const RenderDataRD *p_render_data, RID p_radiance_texture, const RendererRD::MaterialStorage::Samplers &p_samplers, bool p_use_directional_shadow_atlas = false, int p_index = 0);
	void _pre_opaque_render(RenderDataRD *p_render_data);

	uint64_t lightmap_texture_array_version = 0xFFFFFFFF;

	void _update_render_base_uniform_set();

	void _update_instance_data_buffer(RenderListType p_render_list);
	void _fill_instance_data(RenderListType p_render_list, uint32_t p_offset = 0, int32_t p_max_elements = -1, bool p_update_buffer = true);
	void _fill_render_list(RenderListType p_render_list, const RenderDataRD *p_render_data, PassMode p_pass_mode, bool p_append = false);

	void _setup_environment(const RenderDataRD *p_render_data, bool p_no_fog, const Size2i &p_screen_size, const Color &p_default_bg_color, bool p_opaque_render_buffers = false, bool p_pancake_shadows = false, int p_index = 0);
	void _setup_lightmaps(const RenderDataRD *p_render_data, const PagedArray<RID> &p_lightmaps, const Transform3D &p_cam_transform);

	RID render_base_uniform_set;
	LocalVector<RID> render_pass_uniform_sets;

	/* Light map */

	struct LightmapData {
		float normal_xform[12];
		float texture_size[2];
		float exposure_normalization;
		uint32_t flags;
	};

	struct LightmapCaptureData {
		float sh[9 * 4];
	};

	/* Scene state */

	struct SceneState {
		LocalVector<RID> uniform_buffers;

		struct PushConstantUbershader {
			SceneShaderForwardMobile::ShaderSpecialization specialization;
			SceneShaderForwardMobile::UbershaderConstants constants;
		};

		struct PushConstant {
			float uv_offset[2];
			uint32_t base_index;
			uint32_t pad;
			PushConstantUbershader ubershader;
		};

		struct InstanceData {
			float transform[16];
			uint32_t flags;
			uint32_t instance_uniforms_ofs; // Base offset in global buffer for instance variables.
			uint32_t gi_offset; // GI information when using lightmapping (VCT or lightmap index).
			uint32_t layer_mask = 1;
			float lightmap_uv_scale[4]; // Doubles as uv_offset when needed.
			uint32_t reflection_probes[2]; // Packed reflection probes.
			uint32_t omni_lights[2]; // Packed omni lights.
			uint32_t spot_lights[2]; // Packed spot lights.
			uint32_t decals[2]; // Packed spot lights.
			float compressed_aabb_position[4];
			float compressed_aabb_size[4];
			float uv_scale[4];
		};

		RID instance_buffer[RENDER_LIST_MAX];
		uint32_t instance_buffer_size[RENDER_LIST_MAX] = { 0, 0, 0 };
		LocalVector<InstanceData> instance_data[RENDER_LIST_MAX];

		// !BAS! We need to change lightmaps, we're not going to do this with a buffer but pushing the used lightmap in
		LightmapData lightmaps[MAX_LIGHTMAPS];
		RID lightmap_ids[MAX_LIGHTMAPS];
		bool lightmap_has_sh[MAX_LIGHTMAPS];
		uint32_t lightmaps_used = 0;
		uint32_t max_lightmaps;
		RID lightmap_buffer;

		LightmapCaptureData *lightmap_captures = nullptr;
		uint32_t max_lightmap_captures;
		RID lightmap_capture_buffer;

		bool used_screen_texture = false;
		bool used_normal_texture = false;
		bool used_depth_texture = false;
		bool used_sss = false;
		bool used_lightmap = false;

		struct ShadowPass {
			uint32_t element_from;
			uint32_t element_count;
			bool flip_cull;
			PassMode pass_mode;

			RID rp_uniform_set;
			float lod_distance_multiplier;
			float screen_mesh_lod_threshold;

			RID framebuffer;
			Rect2i rect;
			bool clear_depth;
		};

		LocalVector<ShadowPass> shadow_passes;
	} scene_state;

	/* Render List */

	// !BAS! Render list can probably be reused between clustered and mobile?
	struct RenderList {
		LocalVector<GeometryInstanceSurfaceDataCache *> elements;
		LocalVector<RenderElementInfo> element_info;

		void clear() {
			elements.clear();
			element_info.clear();
		}

		//should eventually be replaced by radix

		struct SortByKey {
			_FORCE_INLINE_ bool operator()(const GeometryInstanceSurfaceDataCache *A, const GeometryInstanceSurfaceDataCache *B) const {
				return (A->sort.sort_key2 == B->sort.sort_key2) ? (A->sort.sort_key1 < B->sort.sort_key1) : (A->sort.sort_key2 < B->sort.sort_key2);
			}
		};

		void sort_by_key() {
			SortArray<GeometryInstanceSurfaceDataCache *, SortByKey> sorter;
			sorter.sort(elements.ptr(), elements.size());
		}

		void sort_by_key_range(uint32_t p_from, uint32_t p_size) {
			SortArray<GeometryInstanceSurfaceDataCache *, SortByKey> sorter;
			sorter.sort(elements.ptr() + p_from, p_size);
		}

		struct SortByDepth {
			_FORCE_INLINE_ bool operator()(const GeometryInstanceSurfaceDataCache *A, const GeometryInstanceSurfaceDataCache *B) const {
				return (A->owner->depth < B->owner->depth);
			}
		};

		void sort_by_depth() { //used for shadows

			SortArray<GeometryInstanceSurfaceDataCache *, SortByDepth> sorter;
			sorter.sort(elements.ptr(), elements.size());
		}

		struct SortByReverseDepthAndPriority {
			_FORCE_INLINE_ bool operator()(const GeometryInstanceSurfaceDataCache *A, const GeometryInstanceSurfaceDataCache *B) const {
				return (A->sort.priority == B->sort.priority) ? (A->owner->depth > B->owner->depth) : (A->sort.priority < B->sort.priority);
			}
		};

		void sort_by_reverse_depth_and_priority() { //used for alpha

			SortArray<GeometryInstanceSurfaceDataCache *, SortByReverseDepthAndPriority> sorter;
			sorter.sort(elements.ptr(), elements.size());
		}

		_FORCE_INLINE_ void add_element(GeometryInstanceSurfaceDataCache *p_element) {
			elements.push_back(p_element);
		}
	};

	struct RenderElementInfo {
		uint32_t uses_lightmap : 1;
		uint32_t lod_index : 8;
		uint32_t reserved : 23;
	};

	template <PassMode p_pass_mode>
	_FORCE_INLINE_ void _render_list_template(RenderingDevice::DrawListID p_draw_list, RenderingDevice::FramebufferFormatID p_framebuffer_Format, RenderListParameters *p_params, uint32_t p_from_element, uint32_t p_to_element);
	void _render_list(RenderingDevice::DrawListID p_draw_list, RenderingDevice::FramebufferFormatID p_framebuffer_Format, RenderListParameters *p_params, uint32_t p_from_element, uint32_t p_to_element);
	void _render_list_with_draw_list(RenderListParameters *p_params, RID p_framebuffer, BitField<RD::DrawFlags> p_clear_colors = RD::DRAW_DEFAULT_ALL, const Vector<Color> &p_clear_color_values = Vector<Color>(), float p_clear_depth_value = 0.0, uint32_t p_clear_stencil_value = 0, const Rect2 &p_region = Rect2());

	RenderList render_list[RENDER_LIST_MAX];

protected:
	/* setup */
	virtual void _update_shader_quality_settings() override;

	virtual float _render_buffers_get_luminance_multiplier() override;
	virtual RD::DataFormat _render_buffers_get_color_format() override;
	virtual bool _render_buffers_can_be_storage() override;

	virtual RID _render_buffers_get_normal_texture(Ref<RenderSceneBuffersRD> p_render_buffers) override;
	virtual RID _render_buffers_get_velocity_texture(Ref<RenderSceneBuffersRD> p_render_buffers) override;

	virtual void environment_set_ssao_quality(RS::EnvironmentSSAOQuality p_quality, bool p_half_size, float p_adaptive_target, int p_blur_passes, float p_fadeout_from, float p_fadeout_to) override {}
	virtual void environment_set_ssil_quality(RS::EnvironmentSSILQuality p_quality, bool p_half_size, float p_adaptive_target, int p_blur_passes, float p_fadeout_from, float p_fadeout_to) override {}
	virtual void environment_set_ssr_roughness_quality(RS::EnvironmentSSRRoughnessQuality p_quality) override {}

	virtual void sub_surface_scattering_set_quality(RS::SubSurfaceScatteringQuality p_quality) override {}
	virtual void sub_surface_scattering_set_scale(float p_scale, float p_depth_scale) override {}

	/* Geometry instance */

	class GeometryInstanceForwardMobile;

	// When changing any of these enums, remember to change the corresponding enums in the shader files as well.
	enum {
		INSTANCE_DATA_FLAGS_DYNAMIC = 1 << 3,
		INSTANCE_DATA_FLAGS_NON_UNIFORM_SCALE = 1 << 4,
		INSTANCE_DATA_FLAG_USE_GI_BUFFERS = 1 << 5,
		INSTANCE_DATA_FLAG_USE_SDFGI = 1 << 6,
		INSTANCE_DATA_FLAG_USE_LIGHTMAP_CAPTURE = 1 << 7,
		INSTANCE_DATA_FLAG_USE_LIGHTMAP = 1 << 8,
		INSTANCE_DATA_FLAG_USE_SH_LIGHTMAP = 1 << 9,
		INSTANCE_DATA_FLAG_USE_VOXEL_GI = 1 << 10,
		INSTANCE_DATA_FLAG_PARTICLES = 1 << 11,
		INSTANCE_DATA_FLAG_MULTIMESH = 1 << 12,
		INSTANCE_DATA_FLAG_MULTIMESH_FORMAT_2D = 1 << 13,
		INSTANCE_DATA_FLAG_MULTIMESH_HAS_COLOR = 1 << 14,
		INSTANCE_DATA_FLAG_MULTIMESH_HAS_CUSTOM_DATA = 1 << 15,
		INSTANCE_DATA_FLAGS_PARTICLE_TRAIL_SHIFT = 16,
		INSTANCE_DATA_FLAGS_PARTICLE_TRAIL_MASK = 0xFF,
	};

	struct GeometryInstanceLightmapSH {
		Color sh[9];
	};

	// Cached data for drawing surfaces
	struct GeometryInstanceSurfaceDataCache {
		enum {
			FLAG_PASS_DEPTH = 1,
			FLAG_PASS_OPAQUE = 2,
			FLAG_PASS_ALPHA = 4,
			FLAG_PASS_SHADOW = 8,
			FLAG_USES_SHARED_SHADOW_MATERIAL = 128,
			FLAG_USES_SUBSURFACE_SCATTERING = 2048,
			FLAG_USES_SCREEN_TEXTURE = 4096,
			FLAG_USES_DEPTH_TEXTURE = 8192,
			FLAG_USES_NORMAL_TEXTURE = 16384,
			FLAG_USES_DOUBLE_SIDED_SHADOWS = 32768,
			FLAG_USES_PARTICLE_TRAILS = 65536,
		};

		union {
			struct {
				// !BAS! CHECK BITS!!!

				uint64_t surface_index : 10;
				uint64_t geometry_id : 32;
				uint64_t material_id_low : 16;

				uint64_t material_id_hi : 16;
				uint64_t shader_id : 32;
				uint64_t uses_lightmap : 4; // sort by lightmap id here, not whether its yes/no (is 4 bits enough?)
				uint64_t depth_layer : 4;
				uint64_t priority : 8;

				// uint64_t lod_index : 8; // no need to sort on LOD
				// uint64_t uses_forward_gi : 1; // no GI here, remove
			};
			struct {
				uint64_t sort_key1;
				uint64_t sort_key2;
			};
		} sort;

		RS::PrimitiveType primitive = RS::PRIMITIVE_MAX;
		uint32_t flags = 0;
		uint32_t surface_index = 0;
		uint32_t lod_index = 0;

		void *surface = nullptr;
		RID material_uniform_set;
		SceneShaderForwardMobile::ShaderData *shader = nullptr;
		SceneShaderForwardMobile::MaterialData *material = nullptr;

		void *surface_shadow = nullptr;
		RID material_uniform_set_shadow;
		SceneShaderForwardMobile::ShaderData *shader_shadow = nullptr;

		GeometryInstanceSurfaceDataCache *next = nullptr;
		GeometryInstanceForwardMobile *owner = nullptr;

		SelfList<GeometryInstanceSurfaceDataCache> compilation_dirty_element;
		SelfList<GeometryInstanceSurfaceDataCache> compilation_all_element;

		GeometryInstanceSurfaceDataCache() :
				compilation_dirty_element(this), compilation_all_element(this) {}
	};

	class GeometryInstanceForwardMobile : public RenderGeometryInstanceBase {
	public:
		//used during rendering
		RID transforms_uniform_set;
		bool use_projector = false;
		bool use_soft_shadow = false;
		bool store_transform_cache = true; // If true we copy our transform into our per-draw buffer, if false we use our transforms UBO and clear our per-draw transform.
		uint32_t instance_count = 0;
		uint32_t trail_steps = 1;

		// lightmap
		uint32_t gi_offset_cache = 0; // !BAS! Should rename this to lightmap_offset_cache, in forward clustered this was shared between gi and lightmap
		RID lightmap_instance;
		Rect2 lightmap_uv_scale;
		uint32_t lightmap_slice_index;
		GeometryInstanceLightmapSH *lightmap_sh = nullptr;

		// culled light info
		uint32_t reflection_probe_count = 0;
		RendererRD::ForwardID reflection_probes[MAX_RDL_CULL];
		uint32_t omni_light_count = 0;
		RendererRD::ForwardID omni_lights[MAX_RDL_CULL];
		uint32_t spot_light_count = 0;
		RendererRD::ForwardID spot_lights[MAX_RDL_CULL];
		uint32_t decals_count = 0;
		RendererRD::ForwardID decals[MAX_RDL_CULL];

		GeometryInstanceSurfaceDataCache *surface_caches = nullptr;

		// do we use this?
		SelfList<GeometryInstanceForwardMobile> dirty_list_element;

		GeometryInstanceForwardMobile() :
				dirty_list_element(this) {}

		virtual void _mark_dirty() override;

		virtual void set_use_lightmap(RID p_lightmap_instance, const Rect2 &p_lightmap_uv_scale, int p_lightmap_slice_index) override;
		virtual void set_lightmap_capture(const Color *p_sh9) override;

		virtual void pair_light_instances(const RID *p_light_instances, uint32_t p_light_instance_count) override;
		virtual void pair_reflection_probe_instances(const RID *p_reflection_probe_instances, uint32_t p_reflection_probe_instance_count) override;
		virtual void pair_decal_instances(const RID *p_decal_instances, uint32_t p_decal_instance_count) override;
		virtual void pair_voxel_gi_instances(const RID *p_voxel_gi_instances, uint32_t p_voxel_gi_instance_count) override {}

		virtual void set_softshadow_projector_pairing(bool p_softshadow, bool p_projector) override;
	};

	/* Rendering */

	virtual void _render_scene(RenderDataRD *p_render_data, const Color &p_default_bg_color) override;

	virtual void _render_material(const Transform3D &p_cam_transform, const Projection &p_cam_projection, bool p_cam_orthogonal, const PagedArray<RenderGeometryInstance *> &p_instances, RID p_framebuffer, const Rect2i &p_region, float p_exposure_normalization) override;
	virtual void _render_uv2(const PagedArray<RenderGeometryInstance *> &p_instances, RID p_framebuffer, const Rect2i &p_region) override;
	virtual void _render_sdfgi(Ref<RenderSceneBuffersRD> p_render_buffers, const Vector3i &p_from, const Vector3i &p_size, const AABB &p_bounds, const PagedArray<RenderGeometryInstance *> &p_instances, const RID &p_albedo_texture, const RID &p_emission_texture, const RID &p_emission_aniso_texture, const RID &p_geom_facing_texture, float p_exposure_normalization) override;
	virtual void _render_particle_collider_heightfield(RID p_fb, const Transform3D &p_cam_transform, const Projection &p_cam_projection, const PagedArray<RenderGeometryInstance *> &p_instances) override;

	/* Forward ID */

	class ForwardIDStorageMobile : public RendererRD::ForwardIDStorage {
	public:
		struct ForwardIDAllocator {
			LocalVector<bool> allocations;
			LocalVector<uint8_t> map;
			LocalVector<uint64_t> last_pass;
		};

		ForwardIDAllocator forward_id_allocators[RendererRD::FORWARD_ID_MAX];

	public:
		virtual RendererRD::ForwardID allocate_forward_id(RendererRD::ForwardIDType p_type) override;
		virtual void free_forward_id(RendererRD::ForwardIDType p_type, RendererRD::ForwardID p_id) override;
		virtual void map_forward_id(RendererRD::ForwardIDType p_type, RendererRD::ForwardID p_id, uint32_t p_index, uint64_t p_last_pass) override;
		virtual bool uses_forward_ids() const override { return true; }
	};

	ForwardIDStorageMobile *forward_id_storage_mobile = nullptr;

	void fill_push_constant_instance_indices(SceneState::InstanceData *p_instance_data, const GeometryInstanceForwardMobile *p_instance);

	virtual RendererRD::ForwardIDStorage *create_forward_id_storage() override {
		forward_id_storage_mobile = memnew(ForwardIDStorageMobile);
		return forward_id_storage_mobile;
	}

	struct ForwardIDByMapSort {
		uint8_t map;
		RendererRD::ForwardID forward_id;
		bool operator<(const ForwardIDByMapSort &p_sort) const {
			return map > p_sort.map;
		}
	};

public:
	static RenderForwardMobile *get_singleton() { return singleton; }

	virtual RID reflection_probe_create_framebuffer(RID p_color, RID p_depth) override;

	/* SDFGI UPDATE */

	virtual void sdfgi_update(const Ref<RenderSceneBuffers> &p_render_buffers, RID p_environment, const Vector3 &p_world_position) override {}
	virtual int sdfgi_get_pending_region_count(const Ref<RenderSceneBuffers> &p_render_buffers) const override { return 0; }
	virtual AABB sdfgi_get_pending_region_bounds(const Ref<RenderSceneBuffers> &p_render_buffers, int p_region) const override { return AABB(); }
	virtual uint32_t sdfgi_get_pending_region_cascade(const Ref<RenderSceneBuffers> &p_render_buffers, int p_region) const override { return 0; }

	/* GEOMETRY INSTANCE */

	static void _geometry_instance_dependency_changed(Dependency::DependencyChangedNotification p_notification, DependencyTracker *p_tracker);
	static void _geometry_instance_dependency_deleted(const RID &p_dependency, DependencyTracker *p_tracker);

	SelfList<GeometryInstanceForwardMobile>::List geometry_instance_dirty_list;
	SelfList<GeometryInstanceSurfaceDataCache>::List geometry_surface_compilation_dirty_list;
	SelfList<GeometryInstanceSurfaceDataCache>::List geometry_surface_compilation_all_list;

	PagedAllocator<GeometryInstanceForwardMobile> geometry_instance_alloc;
	PagedAllocator<GeometryInstanceSurfaceDataCache> geometry_instance_surface_alloc;
	PagedAllocator<GeometryInstanceLightmapSH> geometry_instance_lightmap_sh;

	struct SurfacePipelineData {
		void *mesh_surface = nullptr;
		void *mesh_surface_shadow = nullptr;
		SceneShaderForwardMobile::ShaderData *shader = nullptr;
		SceneShaderForwardMobile::ShaderData *shader_shadow = nullptr;
		bool instanced = false;
		bool uses_opaque = false;
		bool uses_transparent = false;
		bool uses_depth = false;
		bool can_use_lightmap = false;
	};

	struct GlobalPipelineData {
		union {
			struct {
				uint32_t texture_samples : 3;
				uint32_t target_samples : 3;
				uint32_t use_reflection_probes : 1;
				uint32_t use_lightmaps : 1;
				uint32_t use_multiview : 1;
				uint32_t use_16_bit_shadows : 1;
				uint32_t use_32_bit_shadows : 1;
				uint32_t use_shadow_cubemaps : 1;
				uint32_t use_shadow_dual_paraboloid : 1;
			};

			uint32_t key;
		};
	};

	GlobalPipelineData global_pipeline_data_compiled = {};
	GlobalPipelineData global_pipeline_data_required = {};

	typedef Pair<SceneShaderForwardMobile::ShaderData *, SceneShaderForwardMobile::ShaderData::PipelineKey> ShaderPipelinePair;

	void _update_global_pipeline_data_requirements_from_project();
	void _update_global_pipeline_data_requirements_from_light_storage();
	void _geometry_instance_add_surface_with_material(GeometryInstanceForwardMobile *ginstance, uint32_t p_surface, SceneShaderForwardMobile::MaterialData *p_material, uint32_t p_material_id, uint32_t p_shader_id, RID p_mesh);
	void _geometry_instance_add_surface_with_material_chain(GeometryInstanceForwardMobile *ginstance, uint32_t p_surface, SceneShaderForwardMobile::MaterialData *p_material, RID p_mat_src, RID p_mesh);
	void _geometry_instance_add_surface(GeometryInstanceForwardMobile *ginstance, uint32_t p_surface, RID p_material, RID p_mesh);
	void _geometry_instance_update(RenderGeometryInstance *p_geometry_instance);
	void _mesh_compile_pipeline_for_surface(SceneShaderForwardMobile::ShaderData *p_shader, void *p_mesh_surface, bool p_instanced_surface, RS::PipelineSource p_source, SceneShaderForwardMobile::ShaderData::PipelineKey &r_pipeline_key, Vector<ShaderPipelinePair> *r_pipeline_pairs = nullptr);
	void _mesh_compile_pipelines_for_surface(const SurfacePipelineData &p_surface, const GlobalPipelineData &p_global, RS::PipelineSource p_source, Vector<ShaderPipelinePair> *r_pipeline_pairs = nullptr);
	void _mesh_generate_all_pipelines_for_surface_cache(GeometryInstanceSurfaceDataCache *p_surface_cache, const GlobalPipelineData &p_global);
	void _update_dirty_geometry_instances();
	void _update_dirty_geometry_pipelines();

	virtual RenderGeometryInstance *geometry_instance_create(RID p_base) override;
	virtual void geometry_instance_free(RenderGeometryInstance *p_geometry_instance) override;

	virtual uint32_t geometry_instance_get_pair_mask() override;

	/* PIPELINES */

	virtual void mesh_generate_pipelines(RID p_mesh, bool p_background_compilation) override;
	virtual uint32_t get_pipeline_compilations(RS::PipelineSource p_source) override;

	virtual bool free(RID p_rid) override;

	virtual void update() override;

	virtual void base_uniforms_changed() override;

	virtual bool is_dynamic_gi_supported() const override;
	virtual bool is_volumetric_supported() const override;
	virtual uint32_t get_max_elements() const override;

	RenderForwardMobile();
	~RenderForwardMobile();
};
} // namespace RendererSceneRenderImplementation

#endif // RENDER_FORWARD_MOBILE_H

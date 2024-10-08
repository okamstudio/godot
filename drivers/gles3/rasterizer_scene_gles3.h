/**************************************************************************/
/*  rasterizer_scene_gles3.h                                              */
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

#ifndef RASTERIZER_SCENE_GLES3_H
#define RASTERIZER_SCENE_GLES3_H

#ifdef GLES3_ENABLED

#include "core/math/projection.h"
#include "core/templates/paged_allocator.h"
#include "core/templates/rid_owner.h"
#include "core/templates/self_list.h"
#include "drivers/gles3/shaders/effects/cubemap_filter.glsl.gen.h"
#include "drivers/gles3/shaders/sky.glsl.gen.h"
#include "scene/resources/mesh.h"
#include "servers/rendering/renderer_compositor.h"
#include "servers/rendering/renderer_scene_render.h"
#include "servers/rendering_server.h"
#include "shader_gles3.h"
#include "storage/light_storage.h"
#include "storage/material_storage.h"
#include "storage/render_scene_buffers_gles3.h"
#include "storage/utilities.h"

enum RenderListType {
	RENDER_LIST_OPAQUE, //used for opaque objects
	RENDER_LIST_ALPHA, //used for transparent objects
	RENDER_LIST_SECONDARY, //used for shadows and other objects
	RENDER_LIST_MAX
};

enum PassMode {
	PASS_MODE_COLOR,
	PASS_MODE_COLOR_TRANSPARENT,
	PASS_MODE_SHADOW,
	PASS_MODE_DEPTH,
	PASS_MODE_MATERIAL,
};

// These should share as much as possible with SkyUniform Location
enum SceneUniformLocation {
	SCENE_TONEMAP_UNIFORM_LOCATION,
	SCENE_GLOBALS_UNIFORM_LOCATION,
	SCENE_DATA_UNIFORM_LOCATION,
	SCENE_MATERIAL_UNIFORM_LOCATION,
	SCENE_EMPTY1, // Unused, put here to avoid conflicts with SKY_DIRECTIONAL_LIGHT_UNIFORM_LOCATION.
	SCENE_OMNILIGHT_UNIFORM_LOCATION,
	SCENE_SPOTLIGHT_UNIFORM_LOCATION,
	SCENE_DIRECTIONAL_LIGHT_UNIFORM_LOCATION,
	SCENE_MULTIVIEW_UNIFORM_LOCATION,
	SCENE_POSITIONAL_SHADOW_UNIFORM_LOCATION,
	SCENE_DIRECTIONAL_SHADOW_UNIFORM_LOCATION,
	SCENE_EMPTY2, // Unused, put here to avoid conflicts with SKY_MULTIVIEW_UNIFORM_LOCATION.
};

enum SkyUniformLocation {
	SKY_TONEMAP_UNIFORM_LOCATION,
	SKY_GLOBALS_UNIFORM_LOCATION,
	SKY_EMPTY1, // Unused, put here to avoid conflicts with SCENE_DATA_UNIFORM_LOCATION.
	SKY_MATERIAL_UNIFORM_LOCATION,
	SKY_DIRECTIONAL_LIGHT_UNIFORM_LOCATION,
	SKY_EMPTY2, // Unused, put here to avoid conflicts with SCENE_OMNILIGHT_UNIFORM_LOCATION.
	SKY_EMPTY3, // Unused, put here to avoid conflicts with SCENE_SPOTLIGHT_UNIFORM_LOCATION.
	SKY_EMPTY4, // Unused, put here to avoid conflicts with SCENE_DIRECTIONAL_LIGHT_UNIFORM_LOCATION.
	SKY_EMPTY5, // Unused, put here to avoid conflicts with SCENE_MULTIVIEW_UNIFORM_LOCATION.
	SKY_EMPTY6, // Unused, put here to avoid conflicts with SCENE_POSITIONAL_SHADOW_UNIFORM_LOCATION.
	SKY_EMPTY7, // Unused, put here to avoid conflicts with SCENE_DIRECTIONAL_SHADOW_UNIFORM_LOCATION.
	SKY_MULTIVIEW_UNIFORM_LOCATION,
};

struct RenderDataGLES3 {
	Ref<RenderSceneBuffersGLES3> render_buffers;
	bool transparent_bg = false;

	Transform3D cam_transform;
	Transform3D inv_cam_transform;
	Projection cam_projection;
	bool cam_orthogonal = false;
	uint32_t camera_visible_layers = 0xFFFFFFFF;

	// For billboards to cast correct shadows.
	Transform3D main_cam_transform;

	// For stereo rendering
	uint32_t view_count = 1;
	Vector3 view_eye_offset[RendererSceneRender::MAX_RENDER_VIEWS];
	Projection view_projection[RendererSceneRender::MAX_RENDER_VIEWS];

	float z_near = 0.0;
	float z_far = 0.0;

	const PagedArray<RenderGeometryInstance *> *instances = nullptr;
	const PagedArray<RID> *lights = nullptr;
	const PagedArray<RID> *reflection_probes = nullptr;
	RID environment;
	RID camera_attributes;
	RID shadow_atlas;
	RID reflection_probe;
	int reflection_probe_pass = 0;

	float lod_distance_multiplier = 0.0;
	float screen_mesh_lod_threshold = 0.0;

	uint32_t directional_light_count = 0;
	uint32_t directional_shadow_count = 0;

	uint32_t spot_light_count = 0;
	uint32_t omni_light_count = 0;

	float luminance_multiplier = 1.0;

	RenderingMethod::RenderInfo *render_info = nullptr;

	/* Shadow data */
	const RendererSceneRender::RenderShadowData *render_shadows = nullptr;
	int render_shadow_count = 0;
};

class RasterizerCanvasGLES3;

class RasterizerSceneGLES3 : public RendererSceneRender {
private:
	static RasterizerSceneGLES3 *singleton;
	RS::ViewportDebugDraw debug_draw = RS::VIEWPORT_DEBUG_DRAW_DISABLED;
	uint64_t scene_pass = 0;

	template <typename T>
	struct InstanceSort {
		float depth;
		T *instance = nullptr;
		bool operator<(const InstanceSort &p_sort) const {
			return depth < p_sort.depth;
		}
	};

	struct SceneGlobals {
		RID shader_default_version;
		RID default_material;
		RID default_shader;
		RID overdraw_material;
		RID overdraw_shader;
	} scene_globals;

	GLES3::SceneMaterialData *default_material_data_ptr = nullptr;
	GLES3::SceneMaterialData *overdraw_material_data_ptr = nullptr;

	/* LIGHT INSTANCE */

	struct LightData {
		float position[3];
		float inv_radius;

		float direction[3]; // Only used by SpotLight
		float size;

		float color[3];
		float attenuation;

		float inv_spot_attenuation;
		float cos_spot_angle;
		float specular_amount;
		float shadow_opacity;

		float pad[3];
		uint32_t bake_mode;
	};
	static_assert(sizeof(LightData) % 16 == 0, "LightData size must be a multiple of 16 bytes");

	struct DirectionalLightData {
		float direction[3];
		float energy;

		float color[3];
		float size;

		uint32_t enabled; // For use by SkyShaders
		uint32_t bake_mode;
		float shadow_opacity;
		float specular;
	};
	static_assert(sizeof(DirectionalLightData) % 16 == 0, "DirectionalLightData size must be a multiple of 16 bytes");

	struct ShadowData {
		float shadow_matrix[16];

		float light_position[3];
		float shadow_normal_bias;

		float pad[3];
		float shadow_atlas_pixel_size;
	};
	static_assert(sizeof(ShadowData) % 16 == 0, "ShadowData size must be a multiple of 16 bytes");

	struct DirectionalShadowData {
		float direction[3];
		float shadow_atlas_pixel_size;
		float shadow_normal_bias[4];
		float shadow_split_offsets[4];
		float shadow_matrices[4][16];
		float fade_from;
		float fade_to;
		uint32_t blend_splits; // Not exposed to the shader.
		uint32_t pad;
	};
	static_assert(sizeof(DirectionalShadowData) % 16 == 0, "DirectionalShadowData size must be a multiple of 16 bytes");

	class GeometryInstanceGLES3;

	// Cached data for drawing surfaces
	struct GeometryInstanceSurface {
		enum {
			FLAG_PASS_DEPTH = 1,
			FLAG_PASS_OPAQUE = 2,
			FLAG_PASS_ALPHA = 4,
			FLAG_PASS_SHADOW = 8,
			FLAG_USES_SHARED_SHADOW_MATERIAL = 128,
			FLAG_USES_SCREEN_TEXTURE = 2048,
			FLAG_USES_DEPTH_TEXTURE = 4096,
			FLAG_USES_NORMAL_TEXTURE = 8192,
			FLAG_USES_DOUBLE_SIDED_SHADOWS = 16384,
		};

		union {
			struct {
				uint64_t lod_index : 8;
				uint64_t surface_index : 8;
				uint64_t geometry_id : 32;
				uint64_t material_id_low : 16;

				uint64_t material_id_hi : 16;
				uint64_t shader_id : 32;
				uint64_t uses_softshadow : 1;
				uint64_t uses_projector : 1;
				uint64_t uses_forward_gi : 1;
				uint64_t uses_lightmap : 1;
				uint64_t depth_layer : 4;
				uint64_t priority : 8;
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
		uint32_t index_count = 0;
		int32_t light_pass_index = -1;
		bool finished_base_pass = false;

		void *surface = nullptr;
		GLES3::SceneShaderData *shader = nullptr;
		GLES3::SceneMaterialData *material = nullptr;

		void *surface_shadow = nullptr;
		GLES3::SceneShaderData *shader_shadow = nullptr;
		GLES3::SceneMaterialData *material_shadow = nullptr;

		GeometryInstanceSurface *next = nullptr;
		GeometryInstanceGLES3 *owner = nullptr;
	};

	struct GeometryInstanceLightmapSH {
		Color sh[9];
	};

	class GeometryInstanceGLES3 : public RenderGeometryInstanceBase {
	public:
		//used during rendering
		bool store_transform_cache = true;

		int32_t instance_count = 0;

		bool can_sdfgi = false;
		bool using_projectors = false;
		bool using_softshadows = false;

		struct LightPass {
			int32_t light_id = -1; // Position in the light uniform buffer.
			int32_t shadow_id = -1; // Position in the shadow uniform buffer.
			RID light_instance_rid;
			bool is_omni = false;
		};

		LocalVector<LightPass> light_passes;

		uint32_t paired_omni_light_count = 0;
		uint32_t paired_spot_light_count = 0;
		LocalVector<RID> paired_omni_lights;
		LocalVector<RID> paired_spot_lights;
		LocalVector<uint32_t> omni_light_gl_cache;
		LocalVector<uint32_t> spot_light_gl_cache;

		LocalVector<RID> paired_reflection_probes;
		LocalVector<RID> reflection_probe_rid_cache;
		LocalVector<Transform3D> reflection_probes_local_transform_cache;

		RID lightmap_instance;
		Rect2 lightmap_uv_scale;
		uint32_t lightmap_slice_index;
		GeometryInstanceLightmapSH *lightmap_sh = nullptr;

		// Used during setup.
		GeometryInstanceSurface *surface_caches = nullptr;
		SelfList<GeometryInstanceGLES3> dirty_list_element;

		GeometryInstanceGLES3() :
				dirty_list_element(this) {}

		virtual void _mark_dirty() override;
		virtual void set_use_lightmap(RID p_lightmap_instance, const Rect2 &p_lightmap_uv_scale, int p_lightmap_slice_index) override;
		virtual void set_lightmap_capture(const Color *p_sh9) override;

		virtual void pair_light_instances(const RID *p_light_instances, uint32_t p_light_instance_count) override;
		virtual void pair_reflection_probe_instances(const RID *p_reflection_probe_instances, uint32_t p_reflection_probe_instance_count) override;
		virtual void pair_decal_instances(const RID *p_decal_instances, uint32_t p_decal_instance_count) override {}
		virtual void pair_voxel_gi_instances(const RID *p_voxel_gi_instances, uint32_t p_voxel_gi_instance_count) override {}

		virtual void set_softshadow_projector_pairing(bool p_softshadow, bool p_projector) override {}
	};

	enum {
		INSTANCE_DATA_FLAGS_DYNAMIC = 1 << 3,
		INSTANCE_DATA_FLAGS_NON_UNIFORM_SCALE = 1 << 4,
		INSTANCE_DATA_FLAG_USE_GI_BUFFERS = 1 << 5,
		INSTANCE_DATA_FLAG_USE_LIGHTMAP_CAPTURE = 1 << 7,
		INSTANCE_DATA_FLAG_USE_LIGHTMAP = 1 << 8,
		INSTANCE_DATA_FLAG_USE_SH_LIGHTMAP = 1 << 9,
		INSTANCE_DATA_FLAG_USE_VOXEL_GI = 1 << 10,
		INSTANCE_DATA_FLAG_PARTICLES = 1 << 11,
		INSTANCE_DATA_FLAG_MULTIMESH = 1 << 12,
		INSTANCE_DATA_FLAG_MULTIMESH_FORMAT_2D = 1 << 13,
		INSTANCE_DATA_FLAG_MULTIMESH_HAS_COLOR = 1 << 14,
		INSTANCE_DATA_FLAG_MULTIMESH_HAS_CUSTOM_DATA = 1 << 15,
	};

	static void _geometry_instance_dependency_changed(Dependency::DependencyChangedNotification p_notification, DependencyTracker *p_tracker);
	static void _geometry_instance_dependency_deleted(const RID &p_dependency, DependencyTracker *p_tracker);

	SelfList<GeometryInstanceGLES3>::List geometry_instance_dirty_list;

	// Use PagedAllocator instead of RID to maximize performance
	PagedAllocator<GeometryInstanceGLES3> geometry_instance_alloc;
	PagedAllocator<GeometryInstanceSurface> geometry_instance_surface_alloc;

	void _geometry_instance_add_surface_with_material(GeometryInstanceGLES3 *ginstance, uint32_t p_surface, GLES3::SceneMaterialData *p_material, uint32_t p_material_id, uint32_t p_shader_id, RID p_mesh);
	void _geometry_instance_add_surface_with_material_chain(GeometryInstanceGLES3 *ginstance, uint32_t p_surface, GLES3::SceneMaterialData *p_material, RID p_mat_src, RID p_mesh);
	void _geometry_instance_add_surface(GeometryInstanceGLES3 *ginstance, uint32_t p_surface, RID p_material, RID p_mesh);
	void _geometry_instance_update(RenderGeometryInstance *p_geometry_instance);
	void _update_dirty_geometry_instances();

	struct SceneState {
		struct UBO {
			float projection_matrix[16];
			float inv_projection_matrix[16];
			float inv_view_matrix[16];
			float view_matrix[16];

			float main_cam_inv_view_matrix[16];

			float viewport_size[2];
			float screen_pixel_size[2];

			float ambient_light_color_energy[4];

			float ambient_color_sky_mix;
			uint32_t pad2;
			float emissive_exposure_normalization;
			uint32_t use_ambient_light = 0;

			uint32_t use_ambient_cubemap = 0;
			uint32_t use_reflection_cubemap = 0;
			float fog_aerial_perspective;
			float time;

			float radiance_inverse_xform[12];

			uint32_t directional_light_count;
			float z_far;
			float z_near;
			float IBL_exposure_normalization;

			uint32_t fog_enabled;
			uint32_t fog_mode;
			float fog_density;
			float fog_height;

			float fog_height_density;
			float fog_depth_curve;
			float fog_sun_scatter;
			float fog_depth_begin;

			float fog_light_color[3];
			float fog_depth_end;

			float shadow_bias;
			float luminance_multiplier;
			uint32_t camera_visible_layers;
			bool pancake_shadows;
		};
		static_assert(sizeof(UBO) % 16 == 0, "Scene UBO size must be a multiple of 16 bytes");
		static_assert(sizeof(UBO) < 16384, "Scene UBO size must be 16384 bytes or smaller");

		struct MultiviewUBO {
			float projection_matrix_view[RendererSceneRender::MAX_RENDER_VIEWS][16];
			float inv_projection_matrix_view[RendererSceneRender::MAX_RENDER_VIEWS][16];
			float eye_offset[RendererSceneRender::MAX_RENDER_VIEWS][4];
		};
		static_assert(sizeof(MultiviewUBO) % 16 == 0, "Multiview UBO size must be a multiple of 16 bytes");
		static_assert(sizeof(MultiviewUBO) < 16384, "MultiviewUBO size must be 16384 bytes or smaller");

		struct TonemapUBO {
			float exposure = 1.0;
			float white = 1.0;
			int32_t tonemapper = 0;
			int32_t pad = 0;

			int32_t pad2 = 0;
			float brightness = 1.0;
			float contrast = 1.0;
			float saturation = 1.0;
		};
		static_assert(sizeof(TonemapUBO) % 16 == 0, "Tonemap UBO size must be a multiple of 16 bytes");

		UBO ubo;
		GLuint ubo_buffer = 0;
		MultiviewUBO multiview_ubo;
		GLuint multiview_buffer = 0;
		GLuint tonemap_buffer = 0;

		bool used_depth_prepass = false;

		GLES3::SceneShaderData::BlendMode current_blend_mode = GLES3::SceneShaderData::BLEND_MODE_MIX;
		GLES3::SceneShaderData::Cull cull_mode = GLES3::SceneShaderData::CULL_BACK;

		bool current_blend_enabled = false;
		bool current_depth_draw_enabled = false;
		bool current_depth_test_enabled = false;
		bool current_scissor_test_enabled = false;

		void reset_gl_state() {
			glDisable(GL_BLEND);
			current_blend_enabled = false;

			glDisable(GL_SCISSOR_TEST);
			current_scissor_test_enabled = false;

			glCullFace(GL_BACK);
			glEnable(GL_CULL_FACE);
			cull_mode = GLES3::SceneShaderData::CULL_BACK;

			glDepthMask(GL_FALSE);
			current_depth_draw_enabled = false;
			glDisable(GL_DEPTH_TEST);
			current_depth_test_enabled = false;
		}

		void set_gl_cull_mode(GLES3::SceneShaderData::Cull p_mode) {
			if (cull_mode != p_mode) {
				if (p_mode == GLES3::SceneShaderData::CULL_DISABLED) {
					glDisable(GL_CULL_FACE);
				} else {
					if (cull_mode == GLES3::SceneShaderData::CULL_DISABLED) {
						// Last time was disabled, so enable and set proper face.
						glEnable(GL_CULL_FACE);
					}
					glCullFace(p_mode == GLES3::SceneShaderData::CULL_FRONT ? GL_FRONT : GL_BACK);
				}
				cull_mode = p_mode;
			}
		}

		void enable_gl_blend(bool p_enabled) {
			if (current_blend_enabled != p_enabled) {
				if (p_enabled) {
					glEnable(GL_BLEND);
				} else {
					glDisable(GL_BLEND);
				}
				current_blend_enabled = p_enabled;
			}
		}

		void enable_gl_scissor_test(bool p_enabled) {
			if (current_scissor_test_enabled != p_enabled) {
				if (p_enabled) {
					glEnable(GL_SCISSOR_TEST);
				} else {
					glDisable(GL_SCISSOR_TEST);
				}
				current_scissor_test_enabled = p_enabled;
			}
		}

		void enable_gl_depth_draw(bool p_enabled) {
			if (current_depth_draw_enabled != p_enabled) {
				glDepthMask(p_enabled ? GL_TRUE : GL_FALSE);
				current_depth_draw_enabled = p_enabled;
			}
		}

		void enable_gl_depth_test(bool p_enabled) {
			if (current_depth_test_enabled != p_enabled) {
				if (p_enabled) {
					glEnable(GL_DEPTH_TEST);
				} else {
					glDisable(GL_DEPTH_TEST);
				}
				current_depth_test_enabled = p_enabled;
			}
		}

		bool texscreen_copied = false;
		bool used_screen_texture = false;
		bool used_normal_texture = false;
		bool used_depth_texture = false;

		LightData *omni_lights = nullptr;
		LightData *spot_lights = nullptr;
		ShadowData *positional_shadows = nullptr;

		InstanceSort<GLES3::LightInstance> *omni_light_sort;
		InstanceSort<GLES3::LightInstance> *spot_light_sort;
		GLuint omni_light_buffer = 0;
		GLuint spot_light_buffer = 0;
		GLuint positional_shadow_buffer = 0;
		uint32_t omni_light_count = 0;
		uint32_t spot_light_count = 0;
		RS::ShadowQuality positional_shadow_quality = RS::ShadowQuality::SHADOW_QUALITY_SOFT_LOW;

		DirectionalLightData *directional_lights = nullptr;
		GLuint directional_light_buffer = 0;
		DirectionalShadowData *directional_shadows = nullptr;
		GLuint directional_shadow_buffer = 0;
		RS::ShadowQuality directional_shadow_quality = RS::ShadowQuality::SHADOW_QUALITY_SOFT_LOW;
	} scene_state;

	struct RenderListParameters {
		GeometryInstanceSurface **elements = nullptr;
		int element_count = 0;
		bool reverse_cull = false;
		uint64_t spec_constant_base_flags = 0;
		bool force_wireframe = false;
		Vector2 uv_offset = Vector2(0, 0);

		RenderListParameters(GeometryInstanceSurface **p_elements, int p_element_count, bool p_reverse_cull, uint64_t p_spec_constant_base_flags, bool p_force_wireframe = false, Vector2 p_uv_offset = Vector2()) {
			elements = p_elements;
			element_count = p_element_count;
			reverse_cull = p_reverse_cull;
			spec_constant_base_flags = p_spec_constant_base_flags;
			force_wireframe = p_force_wireframe;
			uv_offset = p_uv_offset;
		}
	};

	struct RenderList {
		LocalVector<GeometryInstanceSurface *> elements;

		void clear() {
			elements.clear();
		}

		//should eventually be replaced by radix

		struct SortByKey {
			_FORCE_INLINE_ bool operator()(const GeometryInstanceSurface *A, const GeometryInstanceSurface *B) const {
				return (A->sort.sort_key2 == B->sort.sort_key2) ? (A->sort.sort_key1 < B->sort.sort_key1) : (A->sort.sort_key2 < B->sort.sort_key2);
			}
		};

		void sort_by_key() {
			SortArray<GeometryInstanceSurface *, SortByKey> sorter;
			sorter.sort(elements.ptr(), elements.size());
		}

		void sort_by_key_range(uint32_t p_from, uint32_t p_size) {
			SortArray<GeometryInstanceSurface *, SortByKey> sorter;
			sorter.sort(elements.ptr() + p_from, p_size);
		}

		struct SortByDepth {
			_FORCE_INLINE_ bool operator()(const GeometryInstanceSurface *A, const GeometryInstanceSurface *B) const {
				return (A->owner->depth < B->owner->depth);
			}
		};

		void sort_by_depth() { //used for shadows

			SortArray<GeometryInstanceSurface *, SortByDepth> sorter;
			sorter.sort(elements.ptr(), elements.size());
		}

		struct SortByReverseDepthAndPriority {
			_FORCE_INLINE_ bool operator()(const GeometryInstanceSurface *A, const GeometryInstanceSurface *B) const {
				return (A->sort.priority == B->sort.priority) ? (A->owner->depth > B->owner->depth) : (A->sort.priority < B->sort.priority);
			}
		};

		void sort_by_reverse_depth_and_priority() { //used for alpha

			SortArray<GeometryInstanceSurface *, SortByReverseDepthAndPriority> sorter;
			sorter.sort(elements.ptr(), elements.size());
		}

		_FORCE_INLINE_ void add_element(GeometryInstanceSurface *p_element) {
			elements.push_back(p_element);
		}
	};

	RenderList render_list[RENDER_LIST_MAX];

	void _setup_lights(const RenderDataGLES3 *p_render_data, bool p_using_shadows, uint32_t &r_directional_light_count, uint32_t &r_omni_light_count, uint32_t &r_spot_light_count, uint32_t &r_directional_shadow_count);
	void _setup_environment(const RenderDataGLES3 *p_render_data, bool p_no_fog, const Size2i &p_screen_size, bool p_flip_y, const Color &p_default_bg_color, bool p_pancake_shadows, float p_shadow_bias = 0.0);
	void _fill_render_list(RenderListType p_render_list, const RenderDataGLES3 *p_render_data, PassMode p_pass_mode, bool p_append = false);
	void _render_shadows(const RenderDataGLES3 *p_render_data, const Size2i &p_viewport_size = Size2i(1, 1));
	void _render_shadow_pass(RID p_light, RID p_shadow_atlas, int p_pass, const PagedArray<RenderGeometryInstance *> &p_instances, float p_lod_distance_multiplier = 0, float p_screen_mesh_lod_threshold = 0.0, RenderingMethod::RenderInfo *p_render_info = nullptr, const Size2i &p_viewport_size = Size2i(1, 1), const Transform3D &p_main_cam_transform = Transform3D());
	void _render_post_processing(const RenderDataGLES3 *p_render_data);

	template <PassMode p_pass_mode>
	_FORCE_INLINE_ void _render_list_template(RenderListParameters *p_params, const RenderDataGLES3 *p_render_data, uint32_t p_from_element, uint32_t p_to_element, bool p_alpha_pass = false);

protected:
	double time;
	double time_step = 0;

	bool screen_space_roughness_limiter = false;
	float screen_space_roughness_limiter_amount = 0.25;
	float screen_space_roughness_limiter_limit = 0.18;

	void _render_buffers_debug_draw(Ref<RenderSceneBuffersGLES3> p_render_buffers, RID p_shadow_atlas, GLuint p_fbo);

	/* Camera Attributes */

	struct CameraAttributes {
		float exposure_multiplier = 1.0;
		float exposure_normalization = 1.0;
	};

	bool use_physical_light_units = false;
	mutable RID_Owner<CameraAttributes, true> camera_attributes_owner;

	/* Environment */

	RS::EnvironmentSSAOQuality ssao_quality = RS::ENV_SSAO_QUALITY_MEDIUM;
	bool ssao_half_size = false;
	float ssao_adaptive_target = 0.5;
	int ssao_blur_passes = 2;
	float ssao_fadeout_from = 50.0;
	float ssao_fadeout_to = 300.0;

	bool glow_bicubic_upscale = false;
	RS::EnvironmentSSRRoughnessQuality ssr_roughness_quality = RS::ENV_SSR_ROUGHNESS_QUALITY_LOW;

	bool lightmap_bicubic_upscale = false;

	/* Sky */

	struct SkyGlobals {
		float fog_aerial_perspective = 0.0;
		Color fog_light_color;
		float fog_sun_scatter = 0.0;
		bool fog_enabled = false;
		float fog_density = 0.0;
		float z_far = 0.0;
		uint32_t directional_light_count = 0;

		DirectionalLightData *directional_lights = nullptr;
		DirectionalLightData *last_frame_directional_lights = nullptr;
		uint32_t last_frame_directional_light_count = 0;
		GLuint directional_light_buffer = 0;

		RID shader_default_version;
		RID default_material;
		RID default_shader;
		RID fog_material;
		RID fog_shader;
		GLuint screen_triangle = 0;
		GLuint screen_triangle_array = 0;
		uint32_t max_directional_lights = 4;
		uint32_t roughness_layers = 8;
	} sky_globals;

	struct Sky {
		// Screen Buffers
		GLuint half_res_pass = 0;
		GLuint half_res_framebuffer = 0;
		GLuint quarter_res_pass = 0;
		GLuint quarter_res_framebuffer = 0;
		Size2i screen_size = Size2i(0, 0);

		// Radiance Cubemap
		GLuint radiance = 0;
		GLuint radiance_framebuffer = 0;
		GLuint raw_radiance = 0;

		RID material;
		GLuint uniform_buffer;

		int radiance_size = 256;
		int mipmap_count = 1;

		RS::SkyMode mode = RS::SKY_MODE_AUTOMATIC;

		//ReflectionData reflection;
		bool reflection_dirty = false;
		bool dirty = false;
		int processing_layer = 0;
		Sky *dirty_list = nullptr;
		float baked_exposure = 1.0;

		//State to track when radiance cubemap needs updating
		GLES3::SkyMaterialData *prev_material;
		Vector3 prev_position = Vector3(0.0, 0.0, 0.0);
		float prev_time = 0.0f;
	};

	Sky *dirty_sky_list = nullptr;
	mutable RID_Owner<Sky, true> sky_owner;

	void _setup_sky(const RenderDataGLES3 *p_render_data, const PagedArray<RID> &p_lights, const Projection &p_projection, const Transform3D &p_transform, const Size2i p_screen_size);
	void _invalidate_sky(Sky *p_sky);
	void _update_dirty_skys();
	void _update_sky_radiance(RID p_env, const Projection &p_projection, const Transform3D &p_transform, float p_sky_energy_multiplier);
	void _draw_sky(RID p_env, const Projection &p_projection, const Transform3D &p_transform, float p_sky_energy_multiplier, float p_luminance_multiplier, bool p_use_multiview, bool p_flip_y, bool p_apply_color_adjustments_in_post);
	void _free_sky_data(Sky *p_sky);

	// Needed for a single argument calls (material and uv2).
	PagedArrayPool<RenderGeometryInstance *> cull_argument_pool;
	PagedArray<RenderGeometryInstance *> cull_argument;

public:
	static RasterizerSceneGLES3 *get_singleton() { return singleton; }

	RasterizerCanvasGLES3 *canvas = nullptr;

	RenderGeometryInstance *geometry_instance_create(RID p_base) override;
	void geometry_instance_free(RenderGeometryInstance *p_geometry_instance) override;

	uint32_t geometry_instance_get_pair_mask() override;

	/* PIPELINES */

	virtual void mesh_generate_pipelines(RID p_mesh, bool p_background_compilation) override {}
	virtual uint32_t get_pipeline_compilations(RS::PipelineSource p_source) override { return 0; }

	/* SDFGI UPDATE */

	void sdfgi_update(const Ref<RenderSceneBuffers> &p_render_buffers, RID p_environment, const Vector3 &p_world_position) override {}
	int sdfgi_get_pending_region_count(const Ref<RenderSceneBuffers> &p_render_buffers) const override {
		return 0;
	}
	AABB sdfgi_get_pending_region_bounds(const Ref<RenderSceneBuffers> &p_render_buffers, int p_region) const override {
		return AABB();
	}
	uint32_t sdfgi_get_pending_region_cascade(const Ref<RenderSceneBuffers> &p_render_buffers, int p_region) const override {
		return 0;
	}

	/* SKY API */

	RID sky_allocate() override;
	void sky_initialize(RID p_rid) override;
	void sky_set_radiance_size(RID p_sky, int p_radiance_size) override;
	void sky_set_mode(RID p_sky, RS::SkyMode p_mode) override;
	void sky_set_material(RID p_sky, RID p_material) override;
	Ref<Image> sky_bake_panorama(RID p_sky, float p_energy, bool p_bake_irradiance, const Size2i &p_size) override;
	float sky_get_baked_exposure(RID p_sky) const;

	/* ENVIRONMENT API */

	void environment_glow_set_use_bicubic_upscale(bool p_enable) override;

	void environment_set_ssr_roughness_quality(RS::EnvironmentSSRRoughnessQuality p_quality) override;

	void environment_set_ssao_quality(RS::EnvironmentSSAOQuality p_quality, bool p_half_size, float p_adaptive_target, int p_blur_passes, float p_fadeout_from, float p_fadeout_to) override;

	void environment_set_ssil_quality(RS::EnvironmentSSILQuality p_quality, bool p_half_size, float p_adaptive_target, int p_blur_passes, float p_fadeout_from, float p_fadeout_to) override;

	void environment_set_sdfgi_ray_count(RS::EnvironmentSDFGIRayCount p_ray_count) override;
	void environment_set_sdfgi_frames_to_converge(RS::EnvironmentSDFGIFramesToConverge p_frames) override;
	void environment_set_sdfgi_frames_to_update_light(RS::EnvironmentSDFGIFramesToUpdateLight p_update) override;

	void environment_set_volumetric_fog_volume_size(int p_size, int p_depth) override;
	void environment_set_volumetric_fog_filter_active(bool p_enable) override;

	Ref<Image> environment_bake_panorama(RID p_env, bool p_bake_irradiance, const Size2i &p_size) override;

	_FORCE_INLINE_ bool is_using_physical_light_units() {
		return use_physical_light_units;
	}

	void positional_soft_shadow_filter_set_quality(RS::ShadowQuality p_quality) override;
	void directional_soft_shadow_filter_set_quality(RS::ShadowQuality p_quality) override;

	RID fog_volume_instance_create(RID p_fog_volume) override;
	void fog_volume_instance_set_transform(RID p_fog_volume_instance, const Transform3D &p_transform) override;
	void fog_volume_instance_set_active(RID p_fog_volume_instance, bool p_active) override;
	RID fog_volume_instance_get_volume(RID p_fog_volume_instance) const override;
	Vector3 fog_volume_instance_get_position(RID p_fog_volume_instance) const override;

	RID voxel_gi_instance_create(RID p_voxel_gi) override;
	void voxel_gi_instance_set_transform_to_data(RID p_probe, const Transform3D &p_xform) override;
	bool voxel_gi_needs_update(RID p_probe) const override;
	void voxel_gi_update(RID p_probe, bool p_update_light_instances, const Vector<RID> &p_light_instances, const PagedArray<RenderGeometryInstance *> &p_dynamic_objects) override;

	void voxel_gi_set_quality(RS::VoxelGIQuality) override;

	void render_scene(const Ref<RenderSceneBuffers> &p_render_buffers, const CameraData *p_camera_data, const CameraData *p_prev_camera_data, const PagedArray<RenderGeometryInstance *> &p_instances, const PagedArray<RID> &p_lights, const PagedArray<RID> &p_reflection_probes, const PagedArray<RID> &p_voxel_gi_instances, const PagedArray<RID> &p_decals, const PagedArray<RID> &p_lightmaps, const PagedArray<RID> &p_fog_volumes, RID p_environment, RID p_camera_attributes, RID p_compositor, RID p_shadow_atlas, RID p_occluder_debug_tex, RID p_reflection_atlas, RID p_reflection_probe, int p_reflection_probe_pass, float p_screen_mesh_lod_threshold, const RenderShadowData *p_render_shadows, int p_render_shadow_count, const RenderSDFGIData *p_render_sdfgi_regions, int p_render_sdfgi_region_count, const RenderSDFGIUpdateData *p_sdfgi_update_data = nullptr, RenderingMethod::RenderInfo *r_render_info = nullptr) override;
	void render_material(const Transform3D &p_cam_transform, const Projection &p_cam_projection, bool p_cam_orthogonal, const PagedArray<RenderGeometryInstance *> &p_instances, RID p_framebuffer, const Rect2i &p_region) override;
	void render_particle_collider_heightfield(RID p_collider, const Transform3D &p_transform, const PagedArray<RenderGeometryInstance *> &p_instances) override;

	void set_scene_pass(uint64_t p_pass) override {
		scene_pass = p_pass;
	}

	_FORCE_INLINE_ uint64_t get_scene_pass() {
		return scene_pass;
	}

	void set_time(double p_time, double p_step) override;
	void set_debug_draw_mode(RS::ViewportDebugDraw p_debug_draw) override;
	_FORCE_INLINE_ RS::ViewportDebugDraw get_debug_draw_mode() const {
		return debug_draw;
	}

	Ref<RenderSceneBuffers> render_buffers_create() override;
	void gi_set_use_half_resolution(bool p_enable) override;

	void screen_space_roughness_limiter_set_active(bool p_enable, float p_amount, float p_curve) override;
	bool screen_space_roughness_limiter_is_active() const override;

	void sub_surface_scattering_set_quality(RS::SubSurfaceScatteringQuality p_quality) override;
	void sub_surface_scattering_set_scale(float p_scale, float p_depth_scale) override;

	TypedArray<Image> bake_render_uv2(RID p_base, const TypedArray<RID> &p_material_overrides, const Size2i &p_image_size) override;
	void _render_uv2(const PagedArray<RenderGeometryInstance *> &p_instances, GLuint p_framebuffer, const Rect2i &p_region);

	bool free(RID p_rid) override;
	void update() override;
	void sdfgi_set_debug_probe_select(const Vector3 &p_position, const Vector3 &p_dir) override;

	void decals_set_filter(RS::DecalFilter p_filter) override;
	void light_projectors_set_filter(RS::LightProjectorFilter p_filter) override;
	virtual void lightmaps_set_bicubic_filter(bool p_enable) override;

	RasterizerSceneGLES3();
	~RasterizerSceneGLES3();
};

#endif // GLES3_ENABLED

#endif // RASTERIZER_SCENE_GLES3_H

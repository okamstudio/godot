/**************************************************************************/
/*  rasterizer_scene_gles2.h                                              */
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

#ifndef RASTERIZER_SCENE_GLES2_H
#define RASTERIZER_SCENE_GLES2_H

/* Must come before shaders or the Windows build fails... */
#include "rasterizer_storage_gles2.h"

#include "shaders/cube_to_dp.glsl.gen.h"
#include "shaders/effect_blur.glsl.gen.h"
#include "shaders/scene.glsl.gen.h"
#include "shaders/tonemap.glsl.gen.h"
/*

#include "drivers/gles3/shaders/exposure.glsl.gen.h"
#include "drivers/gles3/shaders/resolve.glsl.gen.h"
#include "drivers/gles3/shaders/scene.glsl.gen.h"
#include "drivers/gles3/shaders/screen_space_reflection.glsl.gen.h"
#include "drivers/gles3/shaders/ssao.glsl.gen.h"
#include "drivers/gles3/shaders/ssao_blur.glsl.gen.h"
#include "drivers/gles3/shaders/ssao_minify.glsl.gen.h"
#include "drivers/gles3/shaders/subsurf_scattering.glsl.gen.h"

*/

class RasterizerSceneGLES2 : public RasterizerScene {
public:
	enum ShadowFilterMode {
		SHADOW_FILTER_NEAREST,
		SHADOW_FILTER_PCF5,
		SHADOW_FILTER_PCF13,
	};

	enum {
		INSTANCE_ATTRIB_BASE = 8,
		INSTANCE_BONE_BASE = 13,
	};

	ShadowFilterMode shadow_filter_mode;

	RID default_material;
	RID default_material_twosided;
	RID default_shader;
	RID default_shader_twosided;

	RID default_worldcoord_material;
	RID default_worldcoord_material_twosided;
	RID default_worldcoord_shader;
	RID default_worldcoord_shader_twosided;

	RID default_overdraw_material;
	RID default_overdraw_shader;

	uint64_t render_pass;
	uint64_t scene_pass;
	uint32_t current_material_index;
	uint32_t current_geometry_index;
	uint32_t current_light_index;
	uint32_t current_refprobe_index;
	uint32_t current_shader_index;

private:
	uint32_t _light_counter;
	static const GLenum gl_primitive[];
	static const GLenum _cube_side_enum[6];

public:
	RasterizerStorageGLES2 *storage;
	struct State {
		bool texscreen_copied;
		int current_blend_mode;
		float current_line_width;
		int current_depth_draw;
		bool current_depth_test;
		GLuint current_main_tex;

		SceneShaderGLES2 scene_shader;
		CubeToDpShaderGLES2 cube_to_dp_shader;
		TonemapShaderGLES2 tonemap_shader;
		EffectBlurShaderGLES2 effect_blur_shader;

		GLuint sky_verts;

		GLuint immediate_buffer;
		Color default_ambient;
		Color default_bg;

		// ResolveShaderGLES3 resolve_shader;
		// ScreenSpaceReflectionShaderGLES3 ssr_shader;
		// EffectBlurShaderGLES3 effect_blur_shader;
		// SubsurfScatteringShaderGLES3 sss_shader;
		// SsaoMinifyShaderGLES3 ssao_minify_shader;
		// SsaoShaderGLES3 ssao_shader;
		// SsaoBlurShaderGLES3 ssao_blur_shader;
		// ExposureShaderGLES3 exposure_shader;

		/*
		struct SceneDataUBO {
			//this is a std140 compatible struct. Please read the OpenGL 3.3 Specification spec before doing any changes
			float projection_matrix[16];
			float inv_projection_matrix[16];
			float camera_inverse_matrix[16];
			float camera_matrix[16];
			float ambient_light_color[4];
			float bg_color[4];
			float fog_color_enabled[4];
			float fog_sun_color_amount[4];

			float ambient_energy;
			float bg_energy;
			float z_offset;
			float z_slope_scale;
			float shadow_dual_paraboloid_render_zfar;
			float shadow_dual_paraboloid_render_side;
			float viewport_size[2];
			float screen_pixel_size[2];
			float shadow_atlas_pixel_size[2];
			float shadow_directional_pixel_size[2];

			float time;
			float z_far;
			float reflection_multiplier;
			float subsurface_scatter_width;
			float ambient_occlusion_affect_light;

			uint32_t fog_depth_enabled;
			float fog_depth_begin;
			float fog_depth_curve;
			uint32_t fog_transmit_enabled;
			float fog_transmit_curve;
			uint32_t fog_height_enabled;
			float fog_height_min;
			float fog_height_max;
			float fog_height_curve;
			// make sure this struct is padded to be a multiple of 16 bytes for webgl

		} ubo_data;

		GLuint scene_ubo;

		struct EnvironmentRadianceUBO {

			float transform[16];
			float ambient_contribution;
			uint8_t padding[12];

		} env_radiance_data;

		GLuint env_radiance_ubo;

		GLuint sky_array;

		GLuint directional_ubo;

		GLuint spot_array_ubo;
		GLuint omni_array_ubo;
		GLuint reflection_array_ubo;

		GLuint immediate_buffer;
		GLuint immediate_array;

		uint32_t ubo_light_size;
		uint8_t *spot_array_tmp;
		uint8_t *omni_array_tmp;
		uint8_t *reflection_array_tmp;

		int max_ubo_lights;
		int max_forward_lights_per_object;
		int max_ubo_reflections;
		int max_skeleton_bones;

		bool used_contact_shadows;

		int spot_light_count;
		int omni_light_count;
		int directional_light_count;
		int reflection_probe_count;

		bool used_sss;
		bool using_contact_shadows;

		VS::ViewportDebugDraw debug_draw;
		*/

		bool cull_front;
		bool cull_disabled;

		bool used_screen_texture;
		bool shadow_is_dual_parabolloid;
		float dual_parbolloid_direction;
		float dual_parbolloid_zfar;

		bool render_no_shadows;

		Vector2 viewport_size;

		Vector2 screen_pixel_size;
	} state;

	/* SHADOW ATLAS API */

	uint64_t shadow_atlas_realloc_tolerance_msec;

	struct ShadowAtlas : public RID_Data {
		enum {
			QUADRANT_SHIFT = 27,
			SHADOW_INDEX_MASK = (1 << QUADRANT_SHIFT) - 1,
			SHADOW_INVALID = 0xFFFFFFFF,
		};

		struct Quadrant {
			uint32_t subdivision;

			struct Shadow {
				RID owner;
				uint64_t version;
				uint64_t alloc_tick;

				Shadow() {
					version = 0;
					alloc_tick = 0;
				}
			};

			Vector<Shadow> shadows;

			Quadrant() {
				subdivision = 0;
			}
		} quadrants[4];

		int size_order[4];
		uint32_t smallest_subdiv;

		int size;
		bool use_16_bits;

		GLuint fbo;
		GLuint depth;
		GLuint color;

		Map<RID, uint32_t> shadow_owners;
	};

	struct ShadowCubeMap {
		GLuint fbo[6];
		GLuint cubemap;
		uint32_t size;
	};

	Vector<ShadowCubeMap> shadow_cubemaps;

	RID_Owner<ShadowAtlas> shadow_atlas_owner;

	int directional_shadow_size;
	bool directional_shadow_16_bits;

	void directional_shadow_create();

	RID shadow_atlas_create();
	void shadow_atlas_set_size(RID p_atlas, int p_size, bool p_16_bits = true);
	void shadow_atlas_set_quadrant_subdivision(RID p_atlas, int p_quadrant, int p_subdivision);
	bool _shadow_atlas_find_shadow(ShadowAtlas *shadow_atlas, int *p_in_quadrants, int p_quadrant_count, int p_current_subdiv, uint64_t p_tick, int &r_quadrant, int &r_shadow);
	bool shadow_atlas_update_light(RID p_atlas, RID p_light_intance, float p_coverage, uint64_t p_light_version);

	struct DirectionalShadow {
		GLuint fbo = 0;
		GLuint depth = 0;
		GLuint color = 0;

		int light_count = 0;
		int size = 0;
		bool use_16_bits = true;
		int current_light = 0;
	} directional_shadow;

	virtual int get_directional_light_shadow_size(RID p_light_intance);
	virtual void set_directional_shadow_count(int p_count);

	/* REFLECTION PROBE ATLAS API */

	virtual RID reflection_atlas_create();
	virtual void reflection_atlas_set_size(RID p_ref_atlas, int p_size);
	virtual void reflection_atlas_set_subdivision(RID p_ref_atlas, int p_subdiv);

	/* REFLECTION CUBEMAPS */

	/* REFLECTION PROBE INSTANCE */

	struct ReflectionProbeInstance : public RID_Data {
		RasterizerStorageGLES2::ReflectionProbe *probe_ptr;
		RID probe;
		RID self;
		RID atlas;

		int reflection_atlas_index;

		int render_step;
		int reflection_index;

		GLuint fbo[6];
		GLuint color[6];
		GLuint depth;
		GLuint cubemap;

		int current_resolution;
		mutable bool dirty;

		uint64_t last_pass;
		uint32_t index;

		Transform transform;
	};

	mutable RID_Owner<ReflectionProbeInstance> reflection_probe_instance_owner;

	ReflectionProbeInstance **reflection_probe_instances;
	int reflection_probe_count;

	virtual RID reflection_probe_instance_create(RID p_probe);
	virtual void reflection_probe_instance_set_transform(RID p_instance, const Transform &p_transform);
	virtual void reflection_probe_release_atlas_index(RID p_instance);
	virtual bool reflection_probe_instance_needs_redraw(RID p_instance);
	virtual bool reflection_probe_instance_has_reflection(RID p_instance);
	virtual bool reflection_probe_instance_begin_render(RID p_instance, RID p_reflection_atlas);
	virtual bool reflection_probe_instance_postprocess_step(RID p_instance);

	/* ENVIRONMENT API */

	struct Environment : public RID_Data {
		VS::EnvironmentBG bg_mode;

		RID sky;
		float sky_custom_fov;
		Basis sky_orientation;

		Color bg_color;
		float bg_energy;
		float sky_ambient;

		int camera_feed_id;

		Color ambient_color;
		float ambient_energy;
		float ambient_sky_contribution;

		int canvas_max_layer;

		bool glow_enabled;
		int glow_levels;
		float glow_intensity;
		float glow_strength;
		float glow_bloom;
		VS::EnvironmentGlowBlendMode glow_blend_mode;
		float glow_hdr_bleed_threshold;
		float glow_hdr_bleed_scale;
		float glow_hdr_luminance_cap;
		bool glow_bicubic_upscale;
		bool glow_high_quality;

		bool dof_blur_far_enabled;
		float dof_blur_far_distance;
		float dof_blur_far_transition;
		float dof_blur_far_amount;
		VS::EnvironmentDOFBlurQuality dof_blur_far_quality;

		bool dof_blur_near_enabled;
		float dof_blur_near_distance;
		float dof_blur_near_transition;
		float dof_blur_near_amount;
		VS::EnvironmentDOFBlurQuality dof_blur_near_quality;

		bool adjustments_enabled;
		float adjustments_brightness;
		float adjustments_contrast;
		float adjustments_saturation;
		RID color_correction;

		bool fog_enabled;
		Color fog_color;
		Color fog_sun_color;
		float fog_sun_amount;

		bool fog_depth_enabled;
		float fog_depth_begin;
		float fog_depth_end;
		float fog_depth_curve;
		bool fog_transmit_enabled;
		float fog_transmit_curve;
		bool fog_height_enabled;
		float fog_height_min;
		float fog_height_max;
		float fog_height_curve;

		Environment() :
				bg_mode(VS::ENV_BG_CLEAR_COLOR),
				sky_custom_fov(0.0),
				bg_energy(1.0),
				sky_ambient(0),
				camera_feed_id(0),
				ambient_energy(1.0),
				ambient_sky_contribution(0.0),
				canvas_max_layer(0),
				glow_enabled(false),
				glow_levels((1 << 2) | (1 << 4)),
				glow_intensity(0.8),
				glow_strength(1.0),
				glow_bloom(0.0),
				glow_blend_mode(VS::GLOW_BLEND_MODE_SOFTLIGHT),
				glow_hdr_bleed_threshold(1.0),
				glow_hdr_bleed_scale(2.0),
				glow_hdr_luminance_cap(12.0),
				glow_bicubic_upscale(false),
				glow_high_quality(false),
				dof_blur_far_enabled(false),
				dof_blur_far_distance(10),
				dof_blur_far_transition(5),
				dof_blur_far_amount(0.1),
				dof_blur_far_quality(VS::ENV_DOF_BLUR_QUALITY_MEDIUM),
				dof_blur_near_enabled(false),
				dof_blur_near_distance(2),
				dof_blur_near_transition(1),
				dof_blur_near_amount(0.1),
				dof_blur_near_quality(VS::ENV_DOF_BLUR_QUALITY_MEDIUM),
				adjustments_enabled(false),
				adjustments_brightness(1.0),
				adjustments_contrast(1.0),
				adjustments_saturation(1.0),
				fog_enabled(false),
				fog_color(Color(0.5, 0.5, 0.5)),
				fog_sun_color(Color(0.8, 0.8, 0.0)),
				fog_sun_amount(0),
				fog_depth_enabled(true),
				fog_depth_begin(10),
				fog_depth_end(0),
				fog_depth_curve(1),
				fog_transmit_enabled(true),
				fog_transmit_curve(1),
				fog_height_enabled(false),
				fog_height_min(10),
				fog_height_max(0),
				fog_height_curve(1) {
		}
	};

	mutable RID_Owner<Environment> environment_owner;

	virtual RID environment_create();

	virtual void environment_set_background(RID p_env, VS::EnvironmentBG p_bg);
	virtual void environment_set_sky(RID p_env, RID p_sky);
	virtual void environment_set_sky_custom_fov(RID p_env, float p_scale);
	virtual void environment_set_sky_orientation(RID p_env, const Basis &p_orientation);
	virtual void environment_set_bg_color(RID p_env, const Color &p_color);
	virtual void environment_set_bg_energy(RID p_env, float p_energy);
	virtual void environment_set_canvas_max_layer(RID p_env, int p_max_layer);
	virtual void environment_set_ambient_light(RID p_env, const Color &p_color, float p_energy = 1.0, float p_sky_contribution = 0.0);
	virtual void environment_set_camera_feed_id(RID p_env, int p_camera_feed_id);

	virtual void environment_set_dof_blur_near(RID p_env, bool p_enable, float p_distance, float p_transition, float p_amount, VS::EnvironmentDOFBlurQuality p_quality);
	virtual void environment_set_dof_blur_far(RID p_env, bool p_enable, float p_distance, float p_transition, float p_amount, VS::EnvironmentDOFBlurQuality p_quality);

	virtual void environment_set_glow(RID p_env, bool p_enable, int p_level_flags, float p_intensity, float p_strength, float p_bloom_threshold, VS::EnvironmentGlowBlendMode p_blend_mode, float p_hdr_bleed_threshold, float p_hdr_bleed_scale, float p_hdr_luminance_cap, bool p_bicubic_upscale, bool p_high_quality);
	virtual void environment_set_glow_map(RID p_env, float p_glow_map_strength, RID p_glow_map);
	virtual void environment_set_fog(RID p_env, bool p_enable, float p_begin, float p_end, RID p_gradient_texture);

	virtual void environment_set_ssr(RID p_env, bool p_enable, int p_max_steps, float p_fade_in, float p_fade_out, float p_depth_tolerance, bool p_roughness);
	virtual void environment_set_ssao(RID p_env, bool p_enable, float p_radius, float p_intensity, float p_radius2, float p_intensity2, float p_bias, float p_light_affect, float p_ao_channel_affect, const Color &p_color, VS::EnvironmentSSAOQuality p_quality, VS::EnvironmentSSAOBlur p_blur, float p_bilateral_sharpness);

	virtual void environment_set_tonemap(RID p_env, VS::EnvironmentToneMapper p_tone_mapper, float p_exposure, float p_white, bool p_auto_exposure, float p_min_luminance, float p_max_luminance, float p_auto_exp_speed, float p_auto_exp_scale);

	virtual void environment_set_adjustment(RID p_env, bool p_enable, float p_brightness, float p_contrast, float p_saturation, RID p_ramp);

	virtual void environment_set_fog(RID p_env, bool p_enable, const Color &p_color, const Color &p_sun_color, float p_sun_amount);
	virtual void environment_set_fog_depth(RID p_env, bool p_enable, float p_depth_begin, float p_depth_end, float p_depth_curve, bool p_transmit, float p_transmit_curve);
	virtual void environment_set_fog_height(RID p_env, bool p_enable, float p_min_height, float p_max_height, float p_height_curve);

	virtual bool is_environment(RID p_env);

	virtual VS::EnvironmentBG environment_get_background(RID p_env);
	virtual int environment_get_canvas_max_layer(RID p_env);

	/* LIGHT INSTANCE */

	struct LightInstance : public RID_Data {
		struct ShadowTransform {
			CameraMatrix camera;
			Transform transform;
			float farplane;
			float split;
			float bias_scale;
		};

		ShadowTransform shadow_transform[4];

		RID self;
		RID light;

		RasterizerStorageGLES2::Light *light_ptr;
		Transform transform;

		Vector3 light_vector;
		Vector3 spot_vector;
		float linear_att;

		// TODO passes and all that stuff ?
		uint64_t last_scene_pass;
		uint64_t last_scene_shadow_pass;

		uint16_t light_index;
		uint16_t light_directional_index;

		Rect2 directional_rect;

		// an ever increasing counter for each light added,
		// used for sorting lights for a consistent render
		uint32_t light_counter;

		Set<RID> shadow_atlases; // atlases where this light is registered
	};

	mutable RID_Owner<LightInstance> light_instance_owner;

	virtual RID light_instance_create(RID p_light);
	virtual void light_instance_set_transform(RID p_light_instance, const Transform &p_transform);
	virtual void light_instance_set_shadow_transform(RID p_light_instance, const CameraMatrix &p_projection, const Transform &p_transform, float p_far, float p_split, int p_pass, float p_bias_scale = 1.0);
	virtual void light_instance_mark_visible(RID p_light_instance);
	virtual bool light_instances_can_render_shadow_cube() const { return storage->config.support_shadow_cubemaps; }

	LightInstance **render_light_instances;
	int render_directional_lights;
	int render_light_instance_count;

	/* REFLECTION INSTANCE */

	virtual RID gi_probe_instance_create();
	virtual void gi_probe_instance_set_light_data(RID p_probe, RID p_base, RID p_data);
	virtual void gi_probe_instance_set_transform_to_data(RID p_probe, const Transform &p_xform);
	virtual void gi_probe_instance_set_bounds(RID p_probe, const Vector3 &p_bounds);

	/* RENDER LIST */

	enum LightMode {
		LIGHTMODE_NORMAL,
		LIGHTMODE_UNSHADED,
		LIGHTMODE_LIGHTMAP,
		LIGHTMODE_LIGHTMAP_CAPTURE,
	};

	struct RenderList {
		enum {
			MAX_LIGHTS = 255,
			MAX_REFLECTION_PROBES = 255,
			DEFAULT_MAX_ELEMENTS = 65536
		};

		int max_elements;

		struct Element {
			RasterizerScene::InstanceBase *instance;

			RasterizerStorageGLES2::Geometry *geometry;
			RasterizerStorageGLES2::Material *material;
			RasterizerStorageGLES2::GeometryOwner *owner;

			bool use_accum; //is this an add pass for multipass
			bool *use_accum_ptr;
			bool front_facing;

			union {
				//TODO: should be endian swapped on big endian
				struct {
					int32_t depth_layer : 16;
					int32_t priority : 16;
				};

				uint32_t depth_key;
			};

			union {
				struct {
					//from least significant to most significant in sort, TODO: should be endian swapped on big endian

					uint64_t geometry_index : 14;
					uint64_t instancing : 1;
					uint64_t skeleton : 1;
					uint64_t shader_index : 10;
					uint64_t material_index : 10;
					uint64_t light_index : 8;
					uint64_t light_type2 : 1; // if 1==0 : nolight/directional, else omni/spot
					uint64_t refprobe_1_index : 8;
					uint64_t refprobe_0_index : 8;
					uint64_t light_type1 : 1; //no light, directional is 0, omni spot is 1
					uint64_t light_mode : 2; // LightMode enum
				};

				uint64_t sort_key;
			};
		};

		Element *base_elements;
		Element **elements;

		int element_count;
		int alpha_element_count;

		void clear() {
			element_count = 0;
			alpha_element_count = 0;
		}

		// sorts

		struct SortByKey {
			_FORCE_INLINE_ bool operator()(const Element *A, const Element *B) const {
				if (A->depth_key == B->depth_key) {
					return A->sort_key < B->sort_key;
				} else {
					return A->depth_key < B->depth_key;
				}
			}
		};

		void sort_by_key(bool p_alpha) {
			SortArray<Element *, SortByKey> sorter;

			if (p_alpha) {
				sorter.sort(&elements[max_elements - alpha_element_count], alpha_element_count);
			} else {
				sorter.sort(elements, element_count);
			}
		}

		struct SortByDepth {
			_FORCE_INLINE_ bool operator()(const Element *A, const Element *B) const {
				return A->instance->depth < B->instance->depth;
			}
		};

		void sort_by_depth(bool p_alpha) { //used for shadows

			SortArray<Element *, SortByDepth> sorter;
			if (p_alpha) {
				sorter.sort(&elements[max_elements - alpha_element_count], alpha_element_count);
			} else {
				sorter.sort(elements, element_count);
			}
		}

		struct SortByReverseDepthAndPriority {
			_FORCE_INLINE_ bool operator()(const Element *A, const Element *B) const {
				if (A->priority == B->priority) {
					return A->instance->depth > B->instance->depth;
				} else {
					return A->priority < B->priority;
				}
			}
		};

		void sort_by_reverse_depth_and_priority(bool p_alpha) { //used for alpha

			SortArray<Element *, SortByReverseDepthAndPriority> sorter;
			if (p_alpha) {
				sorter.sort(&elements[max_elements - alpha_element_count], alpha_element_count);
			} else {
				sorter.sort(elements, element_count);
			}
		}

		// element adding and stuff

		_FORCE_INLINE_ Element *add_element() {
			if (element_count + alpha_element_count >= max_elements) {
				return nullptr;
			}

			elements[element_count] = &base_elements[element_count];
			return elements[element_count++];
		}

		_FORCE_INLINE_ Element *add_alpha_element() {
			if (element_count + alpha_element_count >= max_elements) {
				return nullptr;
			}

			int idx = max_elements - alpha_element_count - 1;
			elements[idx] = &base_elements[idx];
			alpha_element_count++;
			return elements[idx];
		}

		void init() {
			element_count = 0;
			alpha_element_count = 0;

			elements = memnew_arr(Element *, max_elements);
			base_elements = memnew_arr(Element, max_elements);

			for (int i = 0; i < max_elements; i++) {
				elements[i] = &base_elements[i];
			}
		}

		RenderList() {
			max_elements = DEFAULT_MAX_ELEMENTS;
		}

		~RenderList() {
			memdelete_arr(elements);
			memdelete_arr(base_elements);
		}
	};

	RenderList render_list;

	void _add_geometry(RasterizerStorageGLES2::Geometry *p_geometry, InstanceBase *p_instance, RasterizerStorageGLES2::GeometryOwner *p_owner, int p_material, bool p_depth_pass, bool p_shadow_pass);
	void _add_geometry_with_material(RasterizerStorageGLES2::Geometry *p_geometry, InstanceBase *p_instance, RasterizerStorageGLES2::GeometryOwner *p_owner, RasterizerStorageGLES2::Material *p_material, bool p_depth_pass, bool p_shadow_pass);

	void _copy_texture_to_buffer(GLuint p_texture, GLuint p_buffer);
	void _fill_render_list(InstanceBase **p_cull_result, int p_cull_count, bool p_depth_pass, bool p_shadow_pass);
	void _render_render_list(RenderList::Element **p_elements, int p_element_count,
			const Transform &p_view_transform,
			const CameraMatrix &p_projection,
			const int p_eye,
			RID p_shadow_atlas,
			Environment *p_env,
			GLuint p_base_env,
			float p_shadow_bias,
			float p_shadow_normal_bias,
			bool p_reverse_cull,
			bool p_alpha_pass,
			bool p_shadow);

	void _draw_sky(RasterizerStorageGLES2::Sky *p_sky, const CameraMatrix &p_projection, const Transform &p_transform, bool p_vflip, float p_custom_fov, float p_energy, const Basis &p_sky_orientation);

	_FORCE_INLINE_ void _set_cull(bool p_front, bool p_disabled, bool p_reverse_cull);
	_FORCE_INLINE_ bool _setup_material(RasterizerStorageGLES2::Material *p_material, bool p_alpha_pass, Size2i p_skeleton_tex_size = Size2i(0, 0));
	_FORCE_INLINE_ void _setup_geometry(RenderList::Element *p_element, RasterizerStorageGLES2::Skeleton *p_skeleton);
	_FORCE_INLINE_ void _setup_light_type(LightInstance *p_light, ShadowAtlas *shadow_atlas);
	_FORCE_INLINE_ void _setup_light(LightInstance *p_light, ShadowAtlas *shadow_atlas, const Transform &p_view_transform, bool accum_pass);
	_FORCE_INLINE_ void _setup_refprobes(ReflectionProbeInstance *p_refprobe1, ReflectionProbeInstance *p_refprobe2, const Transform &p_view_transform, Environment *p_env);
	_FORCE_INLINE_ void _render_geometry(RenderList::Element *p_element);

	void _post_process(Environment *env, const CameraMatrix &p_cam_projection);

	virtual void render_scene(const Transform &p_cam_transform, const CameraMatrix &p_cam_projection, const int p_eye, bool p_cam_ortogonal, InstanceBase **p_cull_result, int p_cull_count, RID *p_light_cull_result, int p_light_cull_count, RID *p_reflection_probe_cull_result, int p_reflection_probe_cull_count, RID p_environment, RID p_shadow_atlas, RID p_reflection_atlas, RID p_reflection_probe, int p_reflection_probe_pass);
	virtual void render_shadow(RID p_light, RID p_shadow_atlas, int p_pass, InstanceBase **p_cull_result, int p_cull_count);
	virtual bool free(RID p_rid);

	virtual void set_scene_pass(uint64_t p_pass);
	virtual void set_debug_draw_mode(VS::ViewportDebugDraw p_debug_draw);

	void iteration();
	void initialize();
	void finalize();
	RasterizerSceneGLES2();
	~RasterizerSceneGLES2();
};

#endif // RASTERIZER_SCENE_GLES2_H

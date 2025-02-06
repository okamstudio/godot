/**************************************************************************/
/*  renderer_scene_render.h                                               */
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

#include "core/math/projection.h"
#include "core/templates/paged_array.h"
#include "servers/rendering/renderer_geometry_instance.h"
#include "servers/rendering/rendering_method.h"
#include "servers/rendering/storage/compositor_storage.h"
#include "servers/rendering/storage/environment_storage.h"
#include "storage/render_scene_buffers.h"

class RendererSceneRender {
private:
	RendererEnvironmentStorage environment_storage;
	RendererCompositorStorage compositor_storage;

public:
	enum {
		MAX_DIRECTIONAL_LIGHTS = 8,
		MAX_DIRECTIONAL_LIGHT_CASCADES = 4,
		MAX_RENDER_VIEWS = 2
	};

	/* Geometry Instance */

	virtual RenderGeometryInstance *geometry_instance_create(RID p_base) = 0;
	virtual void geometry_instance_free(RenderGeometryInstance *p_geometry_instance) = 0;
	virtual uint32_t geometry_instance_get_pair_mask() = 0;

	/* PIPELINES */

	virtual void mesh_generate_pipelines(RID p_mesh, bool p_background_compilation) = 0;
	virtual uint32_t get_pipeline_compilations(RS::PipelineSource p_source) = 0;

	/* SDFGI UPDATE */

	virtual void sdfgi_update(const Ref<RenderSceneBuffers> &p_render_buffers, RID p_environment, const Vector3 &p_world_position) = 0;
	virtual int sdfgi_get_pending_region_count(const Ref<RenderSceneBuffers> &p_render_buffers) const = 0;
	virtual AABB sdfgi_get_pending_region_bounds(const Ref<RenderSceneBuffers> &p_render_buffers, int p_region) const = 0;
	virtual uint32_t sdfgi_get_pending_region_cascade(const Ref<RenderSceneBuffers> &p_render_buffers, int p_region) const = 0;

	/* SKY API */

	virtual RID sky_allocate() = 0;
	virtual void sky_initialize(RID p_rid) = 0;

	virtual void sky_set_radiance_size(RID p_sky, int p_radiance_size) = 0;
	virtual void sky_set_mode(RID p_sky, RS::SkyMode p_samples) = 0;
	virtual void sky_set_material(RID p_sky, RID p_material) = 0;
	virtual Ref<Image> sky_bake_panorama(RID p_sky, float p_energy, bool p_bake_irradiance, const Size2i &p_size) = 0;

	/* COMPOSITOR EFFECT API */

	RID compositor_effect_allocate();
	void compositor_effect_initialize(RID p_rid);
	void compositor_effect_free(RID p_rid);

	bool is_compositor_effect(RID p_compositor) const;
	void compositor_effect_set_enabled(RID p_compositor, bool p_enabled);
	void compositor_effect_set_callback(RID p_compositor, RS::CompositorEffectCallbackType p_callback_type, const Callable &p_callback);
	void compositor_effect_set_flag(RID p_compositor, RS::CompositorEffectFlags p_flag, bool p_set);

	/* COMPOSITOR API */

	RID compositor_allocate();
	void compositor_initialize(RID p_rid);
	void compositor_free(RID p_rid);

	bool is_compositor(RID p_compositor) const;

	void compositor_set_compositor_effects(RID p_compositor, const TypedArray<RID> &p_effects);

	/* ENVIRONMENT API */

	RID environment_allocate();
	void environment_initialize(RID p_rid);
	void environment_free(RID p_rid);

	bool is_environment(RID p_env) const;

	// Background
	void environment_set_background(RID p_env, RS::EnvironmentBG p_bg);
	void environment_set_sky(RID p_env, RID p_sky);
	void environment_set_sky_custom_fov(RID p_env, float p_scale);
	void environment_set_sky_orientation(RID p_env, const Basis &p_orientation);
	void environment_set_bg_color(RID p_env, const Color &p_color);
	void environment_set_bg_energy(RID p_env, float p_multiplier, float p_exposure_value);
	void environment_set_canvas_max_layer(RID p_env, int p_max_layer);
	void environment_set_ambient_light(RID p_env, const Color &p_color, RS::EnvironmentAmbientSource p_ambient = RS::ENV_AMBIENT_SOURCE_BG, float p_energy = 1.0, float p_sky_contribution = 0.0, RS::EnvironmentReflectionSource p_reflection_source = RS::ENV_REFLECTION_SOURCE_BG);
	void environment_set_camera_feed_id(RID p_env, int p_camera_feed_id);

	RS::EnvironmentBG environment_get_background(RID p_env) const;
	RID environment_get_sky(RID p_env) const;
	float environment_get_sky_custom_fov(RID p_env) const;
	Basis environment_get_sky_orientation(RID p_env) const;
	Color environment_get_bg_color(RID p_env) const;
	float environment_get_bg_energy_multiplier(RID p_env) const;
	float environment_get_bg_intensity(RID p_env) const;
	int environment_get_canvas_max_layer(RID p_env) const;
	RS::EnvironmentAmbientSource environment_get_ambient_source(RID p_env) const;
	Color environment_get_ambient_light(RID p_env) const;
	float environment_get_ambient_light_energy(RID p_env) const;
	float environment_get_ambient_sky_contribution(RID p_env) const;
	RS::EnvironmentReflectionSource environment_get_reflection_source(RID p_env) const;
	int environment_get_camera_feed_id(RID p_env) const;

	// Tonemap
	void environment_set_tonemap(RID p_env, RS::EnvironmentToneMapper p_tone_mapper, float p_exposure, float p_white);
	RS::EnvironmentToneMapper environment_get_tone_mapper(RID p_env) const;
	float environment_get_exposure(RID p_env) const;
	float environment_get_white(RID p_env) const;

	// Fog
	void environment_set_fog(RID p_env, bool p_enable, const Color &p_light_color, float p_light_energy, float p_sun_scatter, float p_density, float p_height, float p_height_density, float p_aerial_perspective, float p_sky_affect, RS::EnvironmentFogMode p_mode);
	bool environment_get_fog_enabled(RID p_env) const;
	RS::EnvironmentFogMode environment_get_fog_mode(RID p_env) const;
	Color environment_get_fog_light_color(RID p_env) const;
	float environment_get_fog_light_energy(RID p_env) const;
	float environment_get_fog_sun_scatter(RID p_env) const;
	float environment_get_fog_density(RID p_env) const;
	float environment_get_fog_sky_affect(RID p_env) const;
	float environment_get_fog_height(RID p_env) const;
	float environment_get_fog_height_density(RID p_env) const;
	float environment_get_fog_aerial_perspective(RID p_env) const;

	// Depth Fog
	void environment_set_fog_depth(RID p_env, float p_curve, float p_begin, float p_end);
	float environment_get_fog_depth_curve(RID p_env) const;
	float environment_get_fog_depth_begin(RID p_env) const;
	float environment_get_fog_depth_end(RID p_env) const;

	// Volumetric Fog
	void environment_set_volumetric_fog(RID p_env, bool p_enable, float p_density, const Color &p_albedo, const Color &p_emission, float p_emission_energy, float p_anisotropy, float p_length, float p_detail_spread, float p_gi_inject, bool p_temporal_reprojection, float p_temporal_reprojection_amount, float p_ambient_inject, float p_sky_affect);
	bool environment_get_volumetric_fog_enabled(RID p_env) const;
	float environment_get_volumetric_fog_density(RID p_env) const;
	Color environment_get_volumetric_fog_scattering(RID p_env) const;
	Color environment_get_volumetric_fog_emission(RID p_env) const;
	float environment_get_volumetric_fog_emission_energy(RID p_env) const;
	float environment_get_volumetric_fog_anisotropy(RID p_env) const;
	float environment_get_volumetric_fog_length(RID p_env) const;
	float environment_get_volumetric_fog_detail_spread(RID p_env) const;
	float environment_get_volumetric_fog_gi_inject(RID p_env) const;
	float environment_get_volumetric_fog_sky_affect(RID p_env) const;
	bool environment_get_volumetric_fog_temporal_reprojection(RID p_env) const;
	float environment_get_volumetric_fog_temporal_reprojection_amount(RID p_env) const;
	float environment_get_volumetric_fog_ambient_inject(RID p_env) const;

	virtual void environment_set_volumetric_fog_volume_size(int p_size, int p_depth) = 0;
	virtual void environment_set_volumetric_fog_filter_active(bool p_enable) = 0;

	// GLOW
	void environment_set_glow(RID p_env, bool p_enable, Vector<float> p_levels, float p_intensity, float p_strength, float p_mix, float p_bloom_threshold, RS::EnvironmentGlowBlendMode p_blend_mode, float p_hdr_bleed_threshold, float p_hdr_bleed_scale, float p_hdr_luminance_cap, float p_glow_map_strength, RID p_glow_map);
	bool environment_get_glow_enabled(RID p_env) const;
	Vector<float> environment_get_glow_levels(RID p_env) const;
	float environment_get_glow_intensity(RID p_env) const;
	float environment_get_glow_strength(RID p_env) const;
	float environment_get_glow_bloom(RID p_env) const;
	float environment_get_glow_mix(RID p_env) const;
	RS::EnvironmentGlowBlendMode environment_get_glow_blend_mode(RID p_env) const;
	float environment_get_glow_hdr_bleed_threshold(RID p_env) const;
	float environment_get_glow_hdr_luminance_cap(RID p_env) const;
	float environment_get_glow_hdr_bleed_scale(RID p_env) const;
	float environment_get_glow_map_strength(RID p_env) const;
	RID environment_get_glow_map(RID p_env) const;

	virtual void environment_glow_set_use_bicubic_upscale(bool p_enable) = 0;

	// SSR
	void environment_set_ssr(RID p_env, bool p_enable, int p_max_steps, float p_fade_int, float p_fade_out, float p_depth_tolerance);
	bool environment_get_ssr_enabled(RID p_env) const;
	int environment_get_ssr_max_steps(RID p_env) const;
	float environment_get_ssr_fade_in(RID p_env) const;
	float environment_get_ssr_fade_out(RID p_env) const;
	float environment_get_ssr_depth_tolerance(RID p_env) const;

	virtual void environment_set_ssr_roughness_quality(RS::EnvironmentSSRRoughnessQuality p_quality) = 0;

	// SSAO
	void environment_set_ssao(RID p_env, bool p_enable, float p_radius, float p_intensity, float p_power, float p_detail, float p_horizon, float p_sharpness, float p_light_affect, float p_ao_channel_affect);
	bool environment_get_ssao_enabled(RID p_env) const;
	float environment_get_ssao_radius(RID p_env) const;
	float environment_get_ssao_intensity(RID p_env) const;
	float environment_get_ssao_power(RID p_env) const;
	float environment_get_ssao_detail(RID p_env) const;
	float environment_get_ssao_horizon(RID p_env) const;
	float environment_get_ssao_sharpness(RID p_env) const;
	float environment_get_ssao_direct_light_affect(RID p_env) const;
	float environment_get_ssao_ao_channel_affect(RID p_env) const;

	virtual void environment_set_ssao_quality(RS::EnvironmentSSAOQuality p_quality, bool p_half_size, float p_adaptive_target, int p_blur_passes, float p_fadeout_from, float p_fadeout_to) = 0;

	// SSIL
	void environment_set_ssil(RID p_env, bool p_enable, float p_radius, float p_intensity, float p_sharpness, float p_normal_rejection);
	bool environment_get_ssil_enabled(RID p_env) const;
	float environment_get_ssil_radius(RID p_env) const;
	float environment_get_ssil_intensity(RID p_env) const;
	float environment_get_ssil_sharpness(RID p_env) const;
	float environment_get_ssil_normal_rejection(RID p_env) const;

	virtual void environment_set_ssil_quality(RS::EnvironmentSSILQuality p_quality, bool p_half_size, float p_adaptive_target, int p_blur_passes, float p_fadeout_from, float p_fadeout_to) = 0;

	// SDFGI
	void environment_set_sdfgi(RID p_env, bool p_enable, int p_cascades, float p_min_cell_size, RS::EnvironmentSDFGIYScale p_y_scale, bool p_use_occlusion, float p_bounce_feedback, bool p_read_sky, float p_energy, float p_normal_bias, float p_probe_bias);
	bool environment_get_sdfgi_enabled(RID p_env) const;
	int environment_get_sdfgi_cascades(RID p_env) const;
	float environment_get_sdfgi_min_cell_size(RID p_env) const;
	bool environment_get_sdfgi_use_occlusion(RID p_env) const;
	float environment_get_sdfgi_bounce_feedback(RID p_env) const;
	bool environment_get_sdfgi_read_sky_light(RID p_env) const;
	float environment_get_sdfgi_energy(RID p_env) const;
	float environment_get_sdfgi_normal_bias(RID p_env) const;
	float environment_get_sdfgi_probe_bias(RID p_env) const;
	RS::EnvironmentSDFGIYScale environment_get_sdfgi_y_scale(RID p_env) const;

	virtual void environment_set_sdfgi_ray_count(RS::EnvironmentSDFGIRayCount p_ray_count) = 0;
	virtual void environment_set_sdfgi_frames_to_converge(RS::EnvironmentSDFGIFramesToConverge p_frames) = 0;
	virtual void environment_set_sdfgi_frames_to_update_light(RS::EnvironmentSDFGIFramesToUpdateLight p_update) = 0;

	// Adjustment
	void environment_set_adjustment(RID p_env, bool p_enable, float p_brightness, float p_contrast, float p_saturation, bool p_use_1d_color_correction, RID p_color_correction);
	bool environment_get_adjustments_enabled(RID p_env) const;
	float environment_get_adjustments_brightness(RID p_env) const;
	float environment_get_adjustments_contrast(RID p_env) const;
	float environment_get_adjustments_saturation(RID p_env) const;
	bool environment_get_use_1d_color_correction(RID p_env) const;
	RID environment_get_color_correction(RID p_env) const;

	virtual Ref<Image> environment_bake_panorama(RID p_env, bool p_bake_irradiance, const Size2i &p_size) = 0;

	virtual void positional_soft_shadow_filter_set_quality(RS::ShadowQuality p_quality) = 0;
	virtual void directional_soft_shadow_filter_set_quality(RS::ShadowQuality p_quality) = 0;

	virtual RID fog_volume_instance_create(RID p_fog_volume) = 0;
	virtual void fog_volume_instance_set_transform(RID p_fog_volume_instance, const Transform3D &p_transform) = 0;
	virtual void fog_volume_instance_set_active(RID p_fog_volume_instance, bool p_active) = 0;
	virtual RID fog_volume_instance_get_volume(RID p_fog_volume_instance) const = 0;
	virtual Vector3 fog_volume_instance_get_position(RID p_fog_volume_instance) const = 0;

	virtual RID voxel_gi_instance_create(RID p_voxel_gi) = 0;
	virtual void voxel_gi_instance_set_transform_to_data(RID p_probe, const Transform3D &p_xform) = 0;
	virtual bool voxel_gi_needs_update(RID p_probe) const = 0;
	virtual void voxel_gi_update(RID p_probe, bool p_update_light_instances, const Vector<RID> &p_light_instances, const PagedArray<RenderGeometryInstance *> &p_dynamic_objects) = 0;

	virtual void voxel_gi_set_quality(RS::VoxelGIQuality) = 0;

	struct RenderShadowData {
		RID light;
		int pass = 0;
		PagedArray<RenderGeometryInstance *> instances;
	};

	struct RenderSDFGIData {
		int region = 0;
		PagedArray<RenderGeometryInstance *> instances;
	};

	struct RenderSDFGIUpdateData {
		bool update_static = false;
		uint32_t static_cascade_count;
		uint32_t *static_cascade_indices = nullptr;
		PagedArray<RID> *static_positional_lights;

		const Vector<RID> *directional_lights;
		const RID *positional_light_instances;
		uint32_t positional_light_count;
	};

	struct CameraData {
		// flags
		uint32_t view_count;
		bool is_orthogonal;
		bool is_frustum;
		uint32_t visible_layers;
		bool vaspect;

		// Main/center projection
		Transform3D main_transform;
		Projection main_projection;

		Transform3D view_offset[RendererSceneRender::MAX_RENDER_VIEWS];
		Projection view_projection[RendererSceneRender::MAX_RENDER_VIEWS];
		Vector2 taa_jitter;
		float taa_frame_count = 0.0f;

		void set_camera(const Transform3D p_transform, const Projection p_projection, bool p_is_orthogonal, bool p_is_frustum, bool p_vaspect, const Vector2 &p_taa_jitter = Vector2(), float p_taa_frame_count = 0.0f, uint32_t p_visible_layers = 0xFFFFFFFF);
		void set_multiview_camera(uint32_t p_view_count, const Transform3D *p_transforms, const Projection *p_projections, bool p_is_orthogonal, bool p_is_frustum, bool p_vaspect);
	};

	virtual void render_scene(const Ref<RenderSceneBuffers> &p_render_buffers, const CameraData *p_camera_data, const CameraData *p_prev_camera_data, const PagedArray<RenderGeometryInstance *> &p_instances, const PagedArray<RID> &p_lights, const PagedArray<RID> &p_reflection_probes, const PagedArray<RID> &p_voxel_gi_instances, const PagedArray<RID> &p_decals, const PagedArray<RID> &p_lightmaps, const PagedArray<RID> &p_fog_volumes, RID p_environment, RID p_camera_attributes, RID p_compositor, RID p_shadow_atlas, RID p_occluder_debug_tex, RID p_reflection_atlas, RID p_reflection_probe, int p_reflection_probe_pass, float p_screen_mesh_lod_threshold, const RenderShadowData *p_render_shadows, int p_render_shadow_count, const RenderSDFGIData *p_render_sdfgi_regions, int p_render_sdfgi_region_count, const RenderSDFGIUpdateData *p_sdfgi_update_data = nullptr, RenderingMethod::RenderInfo *r_render_info = nullptr) = 0;

	virtual void render_material(const Transform3D &p_cam_transform, const Projection &p_cam_projection, bool p_cam_orthogonal, const PagedArray<RenderGeometryInstance *> &p_instances, RID p_framebuffer, const Rect2i &p_region) = 0;
	virtual void render_particle_collider_heightfield(RID p_collider, const Transform3D &p_transform, const PagedArray<RenderGeometryInstance *> &p_instances) = 0;

	virtual void set_scene_pass(uint64_t p_pass) = 0;
	virtual void set_time(double p_time, double p_step) = 0;
	virtual void set_debug_draw_mode(RS::ViewportDebugDraw p_debug_draw) = 0;

	virtual Ref<RenderSceneBuffers> render_buffers_create() = 0;
	virtual void gi_set_use_half_resolution(bool p_enable) = 0;

	virtual void screen_space_roughness_limiter_set_active(bool p_enable, float p_amount, float p_limit) = 0;
	virtual bool screen_space_roughness_limiter_is_active() const = 0;

	virtual void sub_surface_scattering_set_quality(RS::SubSurfaceScatteringQuality p_quality) = 0;
	virtual void sub_surface_scattering_set_scale(float p_scale, float p_depth_scale) = 0;

	virtual TypedArray<Image> bake_render_uv2(RID p_base, const TypedArray<RID> &p_material_overrides, const Size2i &p_image_size) = 0;

	virtual bool free(RID p_rid) = 0;

	virtual void sdfgi_set_debug_probe_select(const Vector3 &p_position, const Vector3 &p_dir) = 0;

	virtual void decals_set_filter(RS::DecalFilter p_filter) = 0;
	virtual void light_projectors_set_filter(RS::LightProjectorFilter p_filter) = 0;
	virtual void lightmaps_set_bicubic_filter(bool p_enable) = 0;

	virtual void update() = 0;
	virtual ~RendererSceneRender() {}
};

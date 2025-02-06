/**************************************************************************/
/*  environment_storage.h                                                 */
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

#ifndef ENVIRONMENT_STORAGE_H
#define ENVIRONMENT_STORAGE_H

#include "core/templates/rid_owner.h"
#include "servers/rendering_server.h"

class RendererEnvironmentStorage {
private:
	static RendererEnvironmentStorage *singleton;

	// Environment
	struct Environment {
		// Note, we capture and store all environment parameters received from Godot here.
		// Not all renderers support all effects and should just ignore the bits they don't support.

		// Background
		RS::EnvironmentBG background = RS::ENV_BG_CLEAR_COLOR;
		RID sky;
		float sky_custom_fov = 0.0;
		Basis sky_orientation;
		Color bg_color;
		float bg_energy_multiplier = 1.0;
		float bg_intensity = 1.0; // Measured in nits or candela/m^2. Default to 1.0 so this doesn't impact rendering when Physical Light Units disabled.
		int canvas_max_layer = 0;
		RS::EnvironmentAmbientSource ambient_source = RS::ENV_AMBIENT_SOURCE_BG;
		Color ambient_light;
		float ambient_light_energy = 1.0;
		float ambient_sky_contribution = 1.0;
		RS::EnvironmentReflectionSource reflection_source = RS::ENV_REFLECTION_SOURCE_BG;
		int camera_feed_id = 0;

		// Tonemap
		RS::EnvironmentToneMapper tone_mapper;
		float exposure = 1.0;
		float white = 1.0;
		RID tony_mc_mapface_lut;

		// Fog
		bool fog_enabled = false;
		RS::EnvironmentFogMode fog_mode = RS::EnvironmentFogMode::ENV_FOG_MODE_EXPONENTIAL;
		Color fog_light_color = Color(0.518, 0.553, 0.608);
		float fog_light_energy = 1.0;
		float fog_sun_scatter = 0.0;
		float fog_density = 0.01;
		float fog_sky_affect = 1.0;
		float fog_height = 0.0;
		float fog_height_density = 0.0; //can be negative to invert effect
		float fog_aerial_perspective = 0.0;

		// Depth Fog
		float fog_depth_curve = 1.0;
		float fog_depth_begin = 10.0;
		float fog_depth_end = 100.0;

		// Volumetric Fog
		bool volumetric_fog_enabled = false;
		float volumetric_fog_density = 0.01;
		Color volumetric_fog_scattering = Color(1, 1, 1);
		Color volumetric_fog_emission = Color(0, 0, 0);
		float volumetric_fog_emission_energy = 0.0;
		float volumetric_fog_anisotropy = 0.2;
		float volumetric_fog_length = 64.0;
		float volumetric_fog_detail_spread = 2.0;
		float volumetric_fog_gi_inject = 1.0;
		float volumetric_fog_ambient_inject = 0.0;
		float volumetric_fog_sky_affect = 1.0;
		bool volumetric_fog_temporal_reprojection = true;
		float volumetric_fog_temporal_reprojection_amount = 0.9;

		// Glow
		bool glow_enabled = false;
		Vector<float> glow_levels;
		float glow_intensity = 0.8;
		float glow_strength = 1.0;
		float glow_bloom = 0.0;
		float glow_mix = 0.01;
		RS::EnvironmentGlowBlendMode glow_blend_mode = RS::ENV_GLOW_BLEND_MODE_SOFTLIGHT;
		float glow_hdr_bleed_threshold = 1.0;
		float glow_hdr_luminance_cap = 12.0;
		float glow_hdr_bleed_scale = 2.0;
		float glow_map_strength = 0.0f; // 1.0f in GLES3 ??
		RID glow_map;

		// SSR
		bool ssr_enabled = false;
		int ssr_max_steps = 64;
		float ssr_fade_in = 0.15;
		float ssr_fade_out = 2.0;
		float ssr_depth_tolerance = 0.2;

		// SSAO
		bool ssao_enabled = false;
		float ssao_radius = 1.0;
		float ssao_intensity = 2.0;
		float ssao_power = 1.5;
		float ssao_detail = 0.5;
		float ssao_horizon = 0.06;
		float ssao_sharpness = 0.98;
		float ssao_direct_light_affect = 0.0;
		float ssao_ao_channel_affect = 0.0;

		// SSIL
		bool ssil_enabled = false;
		float ssil_radius = 5.0;
		float ssil_intensity = 1.0;
		float ssil_sharpness = 0.98;
		float ssil_normal_rejection = 1.0;

		// SDFGI
		bool sdfgi_enabled = false;
		int sdfgi_cascades = 4;
		float sdfgi_min_cell_size = 0.2;
		bool sdfgi_use_occlusion = false;
		float sdfgi_bounce_feedback = 0.5;
		bool sdfgi_read_sky_light = true;
		float sdfgi_energy = 1.0;
		float sdfgi_normal_bias = 1.1;
		float sdfgi_probe_bias = 1.1;
		RS::EnvironmentSDFGIYScale sdfgi_y_scale = RS::ENV_SDFGI_Y_SCALE_75_PERCENT;

		// Adjustments
		bool adjustments_enabled = false;
		float adjustments_brightness = 1.0f;
		float adjustments_contrast = 1.0f;
		float adjustments_saturation = 1.0f;
		bool use_1d_color_correction = false;
		RID color_correction;
	};

	mutable RID_Owner<Environment, true> environment_owner;

public:
	static RendererEnvironmentStorage *get_singleton() { return singleton; }

	RendererEnvironmentStorage();
	virtual ~RendererEnvironmentStorage();

	// Environment
	RID environment_allocate();
	void environment_initialize(RID p_rid);
	void environment_free(RID p_rid);

	bool is_environment(RID p_environment) const {
		return environment_owner.owns(p_environment);
	}

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
	int environment_get_camera_feed_id(RID p_env) const;

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

	// Tonemap
	void environment_set_tonemap(RID p_env, RS::EnvironmentToneMapper p_tone_mapper, float p_exposure, float p_white);
	RS::EnvironmentToneMapper environment_get_tone_mapper(RID p_env) const;
	float environment_get_exposure(RID p_env) const;
	float environment_get_white(RID p_env) const;
	RID environment_get_tony_mc_mapface_lut(RID p_env) const;

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

	// SSR
	void environment_set_ssr(RID p_env, bool p_enable, int p_max_steps, float p_fade_int, float p_fade_out, float p_depth_tolerance);
	bool environment_get_ssr_enabled(RID p_env) const;
	int environment_get_ssr_max_steps(RID p_env) const;
	float environment_get_ssr_fade_in(RID p_env) const;
	float environment_get_ssr_fade_out(RID p_env) const;
	float environment_get_ssr_depth_tolerance(RID p_env) const;

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

	// SSIL
	void environment_set_ssil(RID p_env, bool p_enable, float p_radius, float p_intensity, float p_sharpness, float p_normal_rejection);
	bool environment_get_ssil_enabled(RID p_env) const;
	float environment_get_ssil_radius(RID p_env) const;
	float environment_get_ssil_intensity(RID p_env) const;
	float environment_get_ssil_sharpness(RID p_env) const;
	float environment_get_ssil_normal_rejection(RID p_env) const;

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

	// Adjustment
	void environment_set_adjustment(RID p_env, bool p_enable, float p_brightness, float p_contrast, float p_saturation, bool p_use_1d_color_correction, RID p_color_correction);
	bool environment_get_adjustments_enabled(RID p_env) const;
	float environment_get_adjustments_brightness(RID p_env) const;
	float environment_get_adjustments_contrast(RID p_env) const;
	float environment_get_adjustments_saturation(RID p_env) const;
	bool environment_get_use_1d_color_correction(RID p_env) const;
	RID environment_get_color_correction(RID p_env) const;
};

#endif // ENVIRONMENT_STORAGE_H

/**************************************************************************/
/*  environment.h                                                         */
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

#include "core/io/resource.h"
#include "scene/resources/sky.h"
#include "scene/resources/texture.h"

class Environment : public Resource {
	GDCLASS(Environment, Resource);

public:
	enum BGMode {
		BG_CLEAR_COLOR,
		BG_COLOR,
		BG_SKY,
		BG_CANVAS,
		BG_KEEP,
		BG_CAMERA_FEED,
		BG_MAX
	};

	enum AmbientSource {
		AMBIENT_SOURCE_BG,
		AMBIENT_SOURCE_DISABLED,
		AMBIENT_SOURCE_COLOR,
		AMBIENT_SOURCE_SKY,
	};

	enum ReflectionSource {
		REFLECTION_SOURCE_BG,
		REFLECTION_SOURCE_DISABLED,
		REFLECTION_SOURCE_SKY,
	};

	enum ToneMapper {
		TONE_MAPPER_LINEAR,
		TONE_MAPPER_REINHARDT,
		TONE_MAPPER_FILMIC,
		TONE_MAPPER_ACES,
		TONE_MAPPER_AGX,
	};

	enum SDFGIYScale {
		SDFGI_Y_SCALE_50_PERCENT,
		SDFGI_Y_SCALE_75_PERCENT,
		SDFGI_Y_SCALE_100_PERCENT,
	};

	enum FogMode {
		FOG_MODE_EXPONENTIAL,
		FOG_MODE_DEPTH,
	};

	enum GlowBlendMode {
		GLOW_BLEND_MODE_ADDITIVE,
		GLOW_BLEND_MODE_SCREEN,
		GLOW_BLEND_MODE_SOFTLIGHT,
		GLOW_BLEND_MODE_REPLACE,
		GLOW_BLEND_MODE_MIX,
	};

private:
	RID environment;

	// Background
	BGMode bg_mode = BG_CLEAR_COLOR;
	Ref<Sky> bg_sky;
	float bg_sky_custom_fov = 0.0;
	Vector3 bg_sky_rotation;
	Color bg_color;
	int bg_canvas_max_layer = 0;
	int bg_camera_feed_id = 1;
	float bg_energy_multiplier = 1.0;
	float bg_intensity = 30000.0; // Measured in nits or candela/m^2
	void _update_bg_energy();

	// Ambient light
	Color ambient_color;
	AmbientSource ambient_source = AMBIENT_SOURCE_BG;
	float ambient_energy = 1.0;
	float ambient_sky_contribution = 1.0;
	ReflectionSource reflection_source = REFLECTION_SOURCE_BG;
	void _update_ambient_light();

	// Tonemap
	ToneMapper tone_mapper = TONE_MAPPER_LINEAR;
	float tonemap_exposure = 1.0;
	float tonemap_white = 1.0;
	void _update_tonemap();

	// SSR
	bool ssr_enabled = false;
	int ssr_max_steps = 64;
	float ssr_fade_in = 0.15;
	float ssr_fade_out = 2.0;
	float ssr_depth_tolerance = 0.2;
	void _update_ssr();

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
	void _update_ssao();

	// SSIL
	bool ssil_enabled = false;
	float ssil_radius = 5.0;
	float ssil_intensity = 1.0;
	float ssil_sharpness = 0.98;
	float ssil_normal_rejection = 1.0;

	void _update_ssil();

	// SDFGI
	bool sdfgi_enabled = false;
	int sdfgi_cascades = 4;
	float sdfgi_min_cell_size = 0.2;
	SDFGIYScale sdfgi_y_scale = SDFGI_Y_SCALE_75_PERCENT;
	bool sdfgi_use_occlusion = false;
	float sdfgi_bounce_feedback = 0.5;
	bool sdfgi_read_sky_light = true;
	float sdfgi_energy = 1.0;
	float sdfgi_normal_bias = 1.1;
	float sdfgi_probe_bias = 1.1;
	void _update_sdfgi();

	// Glow
	bool glow_enabled = false;
	Vector<float> glow_levels;
	bool glow_normalize_levels = false;
	float glow_intensity = 0.8;
	float glow_strength = 1.0;
	float glow_mix = 0.05;
	float glow_bloom = 0.0;
	GlowBlendMode glow_blend_mode = GLOW_BLEND_MODE_SOFTLIGHT;
	float glow_hdr_bleed_threshold = 1.0;
	float glow_hdr_bleed_scale = 2.0;
	float glow_hdr_luminance_cap = 12.0;
	float glow_map_strength = 0.8f;
	Ref<Texture> glow_map;
	void _update_glow();

	// Fog
	bool fog_enabled = false;
	FogMode fog_mode = FOG_MODE_EXPONENTIAL;
	Color fog_light_color = Color(0.518, 0.553, 0.608);
	float fog_light_energy = 1.0;
	float fog_sun_scatter = 0.0;
	float fog_density = 0.01;
	float fog_height = 0.0;
	float fog_height_density = 0.0; //can be negative to invert effect
	float fog_aerial_perspective = 0.0;
	float fog_sky_affect = 1.0;

	void _update_fog();

	// Depth Fog
	float fog_depth_curve = 1.0;
	float fog_depth_begin = 10.0;
	float fog_depth_end = 100.0;

	void _update_fog_depth();

	// Volumetric Fog
	bool volumetric_fog_enabled = false;
	float volumetric_fog_density = 0.05;
	Color volumetric_fog_albedo = Color(1.0, 1.0, 1.0);
	Color volumetric_fog_emission = Color(0.0, 0.0, 0.0);
	float volumetric_fog_emission_energy = 1.0;
	float volumetric_fog_anisotropy = 0.2;
	float volumetric_fog_length = 64.0;
	float volumetric_fog_detail_spread = 2.0;
	float volumetric_fog_gi_inject = 1.0;
	float volumetric_fog_ambient_inject = 0.0;
	float volumetric_fog_sky_affect = 1.0;
	bool volumetric_fog_temporal_reproject = true;
	float volumetric_fog_temporal_reproject_amount = 0.9;
	void _update_volumetric_fog();

	// Adjustment
	bool adjustment_enabled = false;
	float adjustment_brightness = 1.0;
	float adjustment_contrast = 1.0;
	float adjustment_saturation = 1.0;
	bool use_1d_color_correction = true;
	Ref<Texture> adjustment_color_correction;
	void _update_adjustment();

protected:
	static void _bind_methods();
	void _validate_property(PropertyInfo &p_property) const;
#ifndef DISABLE_DEPRECATED
	// Kept for compatibility from 3.x to 4.0.
	bool _set(const StringName &p_name, const Variant &p_value);
#endif

public:
	virtual RID get_rid() const override;

	// Background
	void set_background(BGMode p_bg);
	BGMode get_background() const;
	void set_sky(const Ref<Sky> &p_sky);
	Ref<Sky> get_sky() const;
	void set_sky_custom_fov(float p_scale);
	float get_sky_custom_fov() const;
	void set_sky_rotation(const Vector3 &p_rotation);
	Vector3 get_sky_rotation() const;
	void set_bg_color(const Color &p_color);
	Color get_bg_color() const;
	void set_bg_energy_multiplier(float p_energy);
	float get_bg_energy_multiplier() const;
	void set_bg_intensity(float p_energy);
	float get_bg_intensity() const;
	void set_canvas_max_layer(int p_max_layer);
	int get_canvas_max_layer() const;
	void set_camera_feed_id(int p_id);
	int get_camera_feed_id() const;

	// Ambient light
	void set_ambient_light_color(const Color &p_color);
	Color get_ambient_light_color() const;
	void set_ambient_source(AmbientSource p_source);
	AmbientSource get_ambient_source() const;
	void set_ambient_light_energy(float p_energy);
	float get_ambient_light_energy() const;
	void set_ambient_light_sky_contribution(float p_ratio);
	float get_ambient_light_sky_contribution() const;
	void set_reflection_source(ReflectionSource p_source);
	ReflectionSource get_reflection_source() const;

	// Tonemap
	void set_tonemapper(ToneMapper p_tone_mapper);
	ToneMapper get_tonemapper() const;
	void set_tonemap_exposure(float p_exposure);
	float get_tonemap_exposure() const;
	void set_tonemap_white(float p_white);
	float get_tonemap_white() const;

	// SSR
	void set_ssr_enabled(bool p_enabled);
	bool is_ssr_enabled() const;
	void set_ssr_max_steps(int p_steps);
	int get_ssr_max_steps() const;
	void set_ssr_fade_in(float p_fade_in);
	float get_ssr_fade_in() const;
	void set_ssr_fade_out(float p_fade_out);
	float get_ssr_fade_out() const;
	void set_ssr_depth_tolerance(float p_depth_tolerance);
	float get_ssr_depth_tolerance() const;

	// SSAO
	void set_ssao_enabled(bool p_enabled);
	bool is_ssao_enabled() const;
	void set_ssao_radius(float p_radius);
	float get_ssao_radius() const;
	void set_ssao_intensity(float p_intensity);
	float get_ssao_intensity() const;
	void set_ssao_power(float p_power);
	float get_ssao_power() const;
	void set_ssao_detail(float p_detail);
	float get_ssao_detail() const;
	void set_ssao_horizon(float p_horizon);
	float get_ssao_horizon() const;
	void set_ssao_sharpness(float p_sharpness);
	float get_ssao_sharpness() const;
	void set_ssao_direct_light_affect(float p_direct_light_affect);
	float get_ssao_direct_light_affect() const;
	void set_ssao_ao_channel_affect(float p_ao_channel_affect);
	float get_ssao_ao_channel_affect() const;

	// SSIL
	void set_ssil_enabled(bool p_enabled);
	bool is_ssil_enabled() const;
	void set_ssil_radius(float p_radius);
	float get_ssil_radius() const;
	void set_ssil_intensity(float p_intensity);
	float get_ssil_intensity() const;
	void set_ssil_sharpness(float p_sharpness);
	float get_ssil_sharpness() const;
	void set_ssil_normal_rejection(float p_normal_rejection);
	float get_ssil_normal_rejection() const;

	// SDFGI
	void set_sdfgi_enabled(bool p_enabled);
	bool is_sdfgi_enabled() const;
	void set_sdfgi_cascades(int p_cascades);
	int get_sdfgi_cascades() const;
	void set_sdfgi_min_cell_size(float p_size);
	float get_sdfgi_min_cell_size() const;
	void set_sdfgi_max_distance(float p_distance);
	float get_sdfgi_max_distance() const;
	void set_sdfgi_cascade0_distance(float p_distance);
	float get_sdfgi_cascade0_distance() const;
	void set_sdfgi_y_scale(SDFGIYScale p_y_scale);
	SDFGIYScale get_sdfgi_y_scale() const;
	void set_sdfgi_use_occlusion(bool p_enabled);
	bool is_sdfgi_using_occlusion() const;
	void set_sdfgi_bounce_feedback(float p_amount);
	float get_sdfgi_bounce_feedback() const;
	void set_sdfgi_read_sky_light(bool p_enabled);
	bool is_sdfgi_reading_sky_light() const;
	void set_sdfgi_energy(float p_energy);
	float get_sdfgi_energy() const;
	void set_sdfgi_normal_bias(float p_bias);
	float get_sdfgi_normal_bias() const;
	void set_sdfgi_probe_bias(float p_bias);
	float get_sdfgi_probe_bias() const;

	// Glow
	void set_glow_enabled(bool p_enabled);
	bool is_glow_enabled() const;
	void set_glow_level(int p_level, float p_intensity);
	float get_glow_level(int p_level) const;
	void set_glow_normalized(bool p_normalized);
	bool is_glow_normalized() const;
	void set_glow_intensity(float p_intensity);
	float get_glow_intensity() const;
	void set_glow_strength(float p_strength);
	float get_glow_strength() const;
	void set_glow_mix(float p_mix);
	float get_glow_mix() const;
	void set_glow_bloom(float p_threshold);
	float get_glow_bloom() const;
	void set_glow_blend_mode(GlowBlendMode p_mode);
	GlowBlendMode get_glow_blend_mode() const;
	void set_glow_hdr_bleed_threshold(float p_threshold);
	float get_glow_hdr_bleed_threshold() const;
	void set_glow_hdr_bleed_scale(float p_scale);
	float get_glow_hdr_bleed_scale() const;
	void set_glow_hdr_luminance_cap(float p_amount);
	float get_glow_hdr_luminance_cap() const;
	void set_glow_map_strength(float p_strength);
	float get_glow_map_strength() const;
	void set_glow_map(Ref<Texture> p_glow_map);
	Ref<Texture> get_glow_map() const;

	// Fog

	void set_fog_enabled(bool p_enabled);
	bool is_fog_enabled() const;
	void set_fog_mode(FogMode p_mode);
	FogMode get_fog_mode() const;
	void set_fog_light_color(const Color &p_light_color);
	Color get_fog_light_color() const;
	void set_fog_light_energy(float p_amount);
	float get_fog_light_energy() const;
	void set_fog_sun_scatter(float p_amount);
	float get_fog_sun_scatter() const;

	void set_fog_density(float p_amount);
	float get_fog_density() const;
	void set_fog_height(float p_amount);
	float get_fog_height() const;
	void set_fog_height_density(float p_amount);
	float get_fog_height_density() const;
	void set_fog_aerial_perspective(float p_aerial_perspective);
	float get_fog_aerial_perspective() const;
	void set_fog_sky_affect(float p_sky_affect);
	float get_fog_sky_affect() const;

	// Depth Fog
	void set_fog_depth_curve(float p_curve);
	float get_fog_depth_curve() const;
	void set_fog_depth_begin(float p_begin);
	float get_fog_depth_begin() const;
	void set_fog_depth_end(float p_end);
	float get_fog_depth_end() const;

	// Volumetric Fog
	void set_volumetric_fog_enabled(bool p_enable);
	bool is_volumetric_fog_enabled() const;
	void set_volumetric_fog_density(float p_density);
	float get_volumetric_fog_density() const;
	void set_volumetric_fog_albedo(Color p_color);
	Color get_volumetric_fog_albedo() const;
	void set_volumetric_fog_emission(Color p_color);
	Color get_volumetric_fog_emission() const;
	void set_volumetric_fog_emission_energy(float p_begin);
	float get_volumetric_fog_emission_energy() const;
	void set_volumetric_fog_anisotropy(float p_anisotropy);
	float get_volumetric_fog_anisotropy() const;
	void set_volumetric_fog_length(float p_length);
	float get_volumetric_fog_length() const;
	void set_volumetric_fog_detail_spread(float p_detail_spread);
	float get_volumetric_fog_detail_spread() const;
	void set_volumetric_fog_gi_inject(float p_gi_inject);
	float get_volumetric_fog_gi_inject() const;
	void set_volumetric_fog_ambient_inject(float p_ambient_inject);
	float get_volumetric_fog_ambient_inject() const;
	void set_volumetric_fog_sky_affect(float p_sky_affect);
	float get_volumetric_fog_sky_affect() const;
	void set_volumetric_fog_temporal_reprojection_enabled(bool p_enable);
	bool is_volumetric_fog_temporal_reprojection_enabled() const;
	void set_volumetric_fog_temporal_reprojection_amount(float p_amount);
	float get_volumetric_fog_temporal_reprojection_amount() const;

	// Adjustment
	void set_adjustment_enabled(bool p_enabled);
	bool is_adjustment_enabled() const;
	void set_adjustment_brightness(float p_brightness);
	float get_adjustment_brightness() const;
	void set_adjustment_contrast(float p_contrast);
	float get_adjustment_contrast() const;
	void set_adjustment_saturation(float p_saturation);
	float get_adjustment_saturation() const;
	void set_adjustment_color_correction(Ref<Texture> p_color_correction);
	Ref<Texture> get_adjustment_color_correction() const;

	Environment();
	~Environment();
};

VARIANT_ENUM_CAST(Environment::BGMode)
VARIANT_ENUM_CAST(Environment::AmbientSource)
VARIANT_ENUM_CAST(Environment::ReflectionSource)
VARIANT_ENUM_CAST(Environment::ToneMapper)
VARIANT_ENUM_CAST(Environment::SDFGIYScale)
VARIANT_ENUM_CAST(Environment::GlowBlendMode)
VARIANT_ENUM_CAST(Environment::FogMode)

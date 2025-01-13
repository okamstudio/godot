/**************************************************************************/
/*  environment.cpp                                                       */
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

#include "environment.h"

#include "core/config/project_settings.h"
#include "scene/resources/gradient_texture.h"
#include "servers/rendering_server.h"

RID Environment::get_rid() const {
	return environment;
}

// Background

void Environment::set_background(BGMode p_bg) {
	bg_mode = p_bg;
	RS::get_singleton()->environment_set_background(environment, RS::EnvironmentBG(p_bg));
	notify_property_list_changed();
	if (bg_mode != BG_SKY) {
		set_fog_aerial_perspective(0.0);
	}
}

Environment::BGMode Environment::get_background() const {
	return bg_mode;
}

void Environment::set_sky(const Ref<Sky> &p_sky) {
	bg_sky = p_sky;
	RID sb_rid;
	if (bg_sky.is_valid()) {
		sb_rid = bg_sky->get_rid();
	}
	RS::get_singleton()->environment_set_sky(environment, sb_rid);
}

Ref<Sky> Environment::get_sky() const {
	return bg_sky;
}

void Environment::set_sky_custom_fov(float p_scale) {
	bg_sky_custom_fov = p_scale;
	RS::get_singleton()->environment_set_sky_custom_fov(environment, p_scale);
}

float Environment::get_sky_custom_fov() const {
	return bg_sky_custom_fov;
}

void Environment::set_sky_rotation(const Vector3 &p_rotation) {
	bg_sky_rotation = p_rotation;
	RS::get_singleton()->environment_set_sky_orientation(environment, Basis::from_euler(p_rotation));
}

Vector3 Environment::get_sky_rotation() const {
	return bg_sky_rotation;
}

void Environment::set_bg_color(const Color &p_color) {
	bg_color = p_color;
	RS::get_singleton()->environment_set_bg_color(environment, p_color);
}

Color Environment::get_bg_color() const {
	return bg_color;
}

void Environment::set_bg_energy_multiplier(float p_multiplier) {
	bg_energy_multiplier = p_multiplier;
	_update_bg_energy();
}

float Environment::get_bg_energy_multiplier() const {
	return bg_energy_multiplier;
}

void Environment::set_bg_intensity(float p_exposure_value) {
	bg_intensity = p_exposure_value;
	_update_bg_energy();
}

float Environment::get_bg_intensity() const {
	return bg_intensity;
}

void Environment::_update_bg_energy() {
	if (GLOBAL_GET("rendering/lights_and_shadows/use_physical_light_units")) {
		RS::get_singleton()->environment_set_bg_energy(environment, bg_energy_multiplier, bg_intensity);
	} else {
		RS::get_singleton()->environment_set_bg_energy(environment, bg_energy_multiplier, 1.0);
	}
}

void Environment::set_canvas_max_layer(int p_max_layer) {
	bg_canvas_max_layer = p_max_layer;
	RS::get_singleton()->environment_set_canvas_max_layer(environment, p_max_layer);
}

int Environment::get_canvas_max_layer() const {
	return bg_canvas_max_layer;
}

void Environment::set_camera_feed_id(int p_id) {
	bg_camera_feed_id = p_id;
	RS::get_singleton()->environment_set_camera_feed_id(environment, bg_camera_feed_id);
}

int Environment::get_camera_feed_id() const {
	return bg_camera_feed_id;
}

// Ambient light

void Environment::set_ambient_light_color(const Color &p_color) {
	ambient_color = p_color;
	_update_ambient_light();
}

Color Environment::get_ambient_light_color() const {
	return ambient_color;
}

void Environment::set_ambient_source(AmbientSource p_source) {
	ambient_source = p_source;
	_update_ambient_light();
	notify_property_list_changed();
}

Environment::AmbientSource Environment::get_ambient_source() const {
	return ambient_source;
}

void Environment::set_ambient_light_energy(float p_energy) {
	ambient_energy = p_energy;
	_update_ambient_light();
}

float Environment::get_ambient_light_energy() const {
	return ambient_energy;
}

void Environment::set_ambient_light_sky_contribution(float p_ratio) {
	// Sky contribution values outside the [0.0; 1.0] range don't make sense and
	// can result in negative colors.
	ambient_sky_contribution = CLAMP(p_ratio, 0.0, 1.0);
	_update_ambient_light();
}

float Environment::get_ambient_light_sky_contribution() const {
	return ambient_sky_contribution;
}

void Environment::set_reflection_source(ReflectionSource p_source) {
	reflection_source = p_source;
	_update_ambient_light();
	notify_property_list_changed();
}

Environment::ReflectionSource Environment::get_reflection_source() const {
	return reflection_source;
}

void Environment::_update_ambient_light() {
	RS::get_singleton()->environment_set_ambient_light(
			environment,
			ambient_color,
			RS::EnvironmentAmbientSource(ambient_source),
			ambient_energy,
			ambient_sky_contribution, RS::EnvironmentReflectionSource(reflection_source));
}

// Tonemap

void Environment::set_tonemapper(ToneMapper p_tone_mapper) {
	tone_mapper = p_tone_mapper;
	_update_tonemap();
	notify_property_list_changed();
}

Environment::ToneMapper Environment::get_tonemapper() const {
	return tone_mapper;
}

void Environment::set_tonemap_exposure(float p_exposure) {
	tonemap_exposure = p_exposure;
	_update_tonemap();
}

float Environment::get_tonemap_exposure() const {
	return tonemap_exposure;
}

void Environment::set_tonemap_white(float p_white) {
	tonemap_white = p_white;
	_update_tonemap();
}

float Environment::get_tonemap_white() const {
	return tonemap_white;
}

void Environment::_update_tonemap() {
	RS::get_singleton()->environment_set_tonemap(
			environment,
			RS::EnvironmentToneMapper(tone_mapper),
			tonemap_exposure,
			tonemap_white);
}

// SSR

void Environment::set_ssr_enabled(bool p_enabled) {
	ssr_enabled = p_enabled;
	_update_ssr();
	notify_property_list_changed();
}

bool Environment::is_ssr_enabled() const {
	return ssr_enabled;
}

void Environment::set_ssr_max_steps(int p_steps) {
	ssr_max_steps = p_steps;
	_update_ssr();
}

int Environment::get_ssr_max_steps() const {
	return ssr_max_steps;
}

void Environment::set_ssr_fade_in(float p_fade_in) {
	ssr_fade_in = MAX(p_fade_in, 0.0f);
	_update_ssr();
}

float Environment::get_ssr_fade_in() const {
	return ssr_fade_in;
}

void Environment::set_ssr_fade_out(float p_fade_out) {
	ssr_fade_out = MAX(p_fade_out, 0.0f);
	_update_ssr();
}

float Environment::get_ssr_fade_out() const {
	return ssr_fade_out;
}

void Environment::set_ssr_depth_tolerance(float p_depth_tolerance) {
	ssr_depth_tolerance = p_depth_tolerance;
	_update_ssr();
}

float Environment::get_ssr_depth_tolerance() const {
	return ssr_depth_tolerance;
}

void Environment::_update_ssr() {
	RS::get_singleton()->environment_set_ssr(
			environment,
			ssr_enabled,
			ssr_max_steps,
			ssr_fade_in,
			ssr_fade_out,
			ssr_depth_tolerance);
}

// SSAO

void Environment::set_ssao_enabled(bool p_enabled) {
	ssao_enabled = p_enabled;
	_update_ssao();
	notify_property_list_changed();
}

bool Environment::is_ssao_enabled() const {
	return ssao_enabled;
}

void Environment::set_ssao_radius(float p_radius) {
	ssao_radius = p_radius;
	_update_ssao();
}

float Environment::get_ssao_radius() const {
	return ssao_radius;
}

void Environment::set_ssao_intensity(float p_intensity) {
	ssao_intensity = p_intensity;
	_update_ssao();
}

float Environment::get_ssao_intensity() const {
	return ssao_intensity;
}

void Environment::set_ssao_power(float p_power) {
	ssao_power = p_power;
	_update_ssao();
}

float Environment::get_ssao_power() const {
	return ssao_power;
}

void Environment::set_ssao_detail(float p_detail) {
	ssao_detail = p_detail;
	_update_ssao();
}

float Environment::get_ssao_detail() const {
	return ssao_detail;
}

void Environment::set_ssao_horizon(float p_horizon) {
	ssao_horizon = p_horizon;
	_update_ssao();
}

float Environment::get_ssao_horizon() const {
	return ssao_horizon;
}

void Environment::set_ssao_sharpness(float p_sharpness) {
	ssao_sharpness = p_sharpness;
	_update_ssao();
}

float Environment::get_ssao_sharpness() const {
	return ssao_sharpness;
}

void Environment::set_ssao_direct_light_affect(float p_direct_light_affect) {
	ssao_direct_light_affect = p_direct_light_affect;
	_update_ssao();
}

float Environment::get_ssao_direct_light_affect() const {
	return ssao_direct_light_affect;
}

void Environment::set_ssao_ao_channel_affect(float p_ao_channel_affect) {
	ssao_ao_channel_affect = p_ao_channel_affect;
	_update_ssao();
}

float Environment::get_ssao_ao_channel_affect() const {
	return ssao_ao_channel_affect;
}

void Environment::_update_ssao() {
	RS::get_singleton()->environment_set_ssao(
			environment,
			ssao_enabled,
			ssao_radius,
			ssao_intensity,
			ssao_power,
			ssao_detail,
			ssao_horizon,
			ssao_sharpness,
			ssao_direct_light_affect,
			ssao_ao_channel_affect);
}

// SSIL

void Environment::set_ssil_enabled(bool p_enabled) {
	ssil_enabled = p_enabled;
	_update_ssil();
	notify_property_list_changed();
}

bool Environment::is_ssil_enabled() const {
	return ssil_enabled;
}

void Environment::set_ssil_radius(float p_radius) {
	ssil_radius = p_radius;
	_update_ssil();
}

float Environment::get_ssil_radius() const {
	return ssil_radius;
}

void Environment::set_ssil_intensity(float p_intensity) {
	ssil_intensity = p_intensity;
	_update_ssil();
}

float Environment::get_ssil_intensity() const {
	return ssil_intensity;
}

void Environment::set_ssil_sharpness(float p_sharpness) {
	ssil_sharpness = p_sharpness;
	_update_ssil();
}

float Environment::get_ssil_sharpness() const {
	return ssil_sharpness;
}

void Environment::set_ssil_normal_rejection(float p_normal_rejection) {
	ssil_normal_rejection = p_normal_rejection;
	_update_ssil();
}

float Environment::get_ssil_normal_rejection() const {
	return ssil_normal_rejection;
}

void Environment::_update_ssil() {
	RS::get_singleton()->environment_set_ssil(
			environment,
			ssil_enabled,
			ssil_radius,
			ssil_intensity,
			ssil_sharpness,
			ssil_normal_rejection);
}

// SDFGI

void Environment::set_sdfgi_enabled(bool p_enabled) {
	sdfgi_enabled = p_enabled;
	_update_sdfgi();
	notify_property_list_changed();
}

bool Environment::is_sdfgi_enabled() const {
	return sdfgi_enabled;
}

void Environment::set_sdfgi_cascades(int p_cascades) {
	ERR_FAIL_COND_MSG(p_cascades < 1 || p_cascades > 8, "Invalid number of SDFGI cascades (must be between 1 and 8).");
	sdfgi_cascades = p_cascades;
	_update_sdfgi();
}

int Environment::get_sdfgi_cascades() const {
	return sdfgi_cascades;
}

void Environment::set_sdfgi_min_cell_size(float p_size) {
	sdfgi_min_cell_size = p_size;
	_update_sdfgi();
}

float Environment::get_sdfgi_min_cell_size() const {
	return sdfgi_min_cell_size;
}

void Environment::set_sdfgi_max_distance(float p_distance) {
	p_distance /= 64.0;
	for (int i = 0; i < sdfgi_cascades; i++) {
		p_distance *= 0.5; //halve for each cascade
	}
	sdfgi_min_cell_size = p_distance;
	_update_sdfgi();
}

float Environment::get_sdfgi_max_distance() const {
	float md = sdfgi_min_cell_size;
	md *= 64.0;
	for (int i = 0; i < sdfgi_cascades; i++) {
		md *= 2.0;
	}
	return md;
}

void Environment::set_sdfgi_cascade0_distance(float p_distance) {
	sdfgi_min_cell_size = p_distance / 64.0;
	_update_sdfgi();
}

float Environment::get_sdfgi_cascade0_distance() const {
	return sdfgi_min_cell_size * 64.0;
}

void Environment::set_sdfgi_y_scale(SDFGIYScale p_y_scale) {
	sdfgi_y_scale = p_y_scale;
	_update_sdfgi();
}

Environment::SDFGIYScale Environment::get_sdfgi_y_scale() const {
	return sdfgi_y_scale;
}

void Environment::set_sdfgi_use_occlusion(bool p_enabled) {
	sdfgi_use_occlusion = p_enabled;
	_update_sdfgi();
}

bool Environment::is_sdfgi_using_occlusion() const {
	return sdfgi_use_occlusion;
}

void Environment::set_sdfgi_bounce_feedback(float p_amount) {
	sdfgi_bounce_feedback = p_amount;
	_update_sdfgi();
}
float Environment::get_sdfgi_bounce_feedback() const {
	return sdfgi_bounce_feedback;
}

void Environment::set_sdfgi_read_sky_light(bool p_enabled) {
	sdfgi_read_sky_light = p_enabled;
	_update_sdfgi();
}

bool Environment::is_sdfgi_reading_sky_light() const {
	return sdfgi_read_sky_light;
}

void Environment::set_sdfgi_energy(float p_energy) {
	sdfgi_energy = p_energy;
	_update_sdfgi();
}

float Environment::get_sdfgi_energy() const {
	return sdfgi_energy;
}

void Environment::set_sdfgi_normal_bias(float p_bias) {
	sdfgi_normal_bias = p_bias;
	_update_sdfgi();
}

float Environment::get_sdfgi_normal_bias() const {
	return sdfgi_normal_bias;
}

void Environment::set_sdfgi_probe_bias(float p_bias) {
	sdfgi_probe_bias = p_bias;
	_update_sdfgi();
}

float Environment::get_sdfgi_probe_bias() const {
	return sdfgi_probe_bias;
}

void Environment::_update_sdfgi() {
	RS::get_singleton()->environment_set_sdfgi(
			environment,
			sdfgi_enabled,
			sdfgi_cascades,
			sdfgi_min_cell_size,
			RS::EnvironmentSDFGIYScale(sdfgi_y_scale),
			sdfgi_use_occlusion,
			sdfgi_bounce_feedback,
			sdfgi_read_sky_light,
			sdfgi_energy,
			sdfgi_normal_bias,
			sdfgi_probe_bias);
}

// Glow

void Environment::set_glow_enabled(bool p_enabled) {
	glow_enabled = p_enabled;
	_update_glow();
	notify_property_list_changed();
}

bool Environment::is_glow_enabled() const {
	return glow_enabled;
}

void Environment::set_glow_level(int p_level, float p_intensity) {
	ERR_FAIL_INDEX(p_level, RS::MAX_GLOW_LEVELS);

	glow_levels.write[p_level] = p_intensity;

	_update_glow();
}

float Environment::get_glow_level(int p_level) const {
	ERR_FAIL_INDEX_V(p_level, RS::MAX_GLOW_LEVELS, 0.0);

	return glow_levels[p_level];
}

void Environment::set_glow_normalized(bool p_normalized) {
	glow_normalize_levels = p_normalized;

	_update_glow();
}

bool Environment::is_glow_normalized() const {
	return glow_normalize_levels;
}

void Environment::set_glow_intensity(float p_intensity) {
	glow_intensity = p_intensity;
	_update_glow();
}

float Environment::get_glow_intensity() const {
	return glow_intensity;
}

void Environment::set_glow_strength(float p_strength) {
	glow_strength = p_strength;
	_update_glow();
}

float Environment::get_glow_strength() const {
	return glow_strength;
}

void Environment::set_glow_mix(float p_mix) {
	glow_mix = p_mix;
	_update_glow();
}

float Environment::get_glow_mix() const {
	return glow_mix;
}

void Environment::set_glow_bloom(float p_threshold) {
	glow_bloom = p_threshold;
	_update_glow();
}

float Environment::get_glow_bloom() const {
	return glow_bloom;
}

void Environment::set_glow_blend_mode(GlowBlendMode p_mode) {
	glow_blend_mode = p_mode;
	_update_glow();
	notify_property_list_changed();
}

Environment::GlowBlendMode Environment::get_glow_blend_mode() const {
	return glow_blend_mode;
}

void Environment::set_glow_hdr_bleed_threshold(float p_threshold) {
	glow_hdr_bleed_threshold = p_threshold;
	_update_glow();
}

float Environment::get_glow_hdr_bleed_threshold() const {
	return glow_hdr_bleed_threshold;
}

void Environment::set_glow_hdr_bleed_scale(float p_scale) {
	glow_hdr_bleed_scale = p_scale;
	_update_glow();
}

float Environment::get_glow_hdr_bleed_scale() const {
	return glow_hdr_bleed_scale;
}

void Environment::set_glow_hdr_luminance_cap(float p_amount) {
	glow_hdr_luminance_cap = p_amount;
	_update_glow();
}

float Environment::get_glow_hdr_luminance_cap() const {
	return glow_hdr_luminance_cap;
}

void Environment::set_glow_map_strength(float p_strength) {
	glow_map_strength = p_strength;
	_update_glow();
}

float Environment::get_glow_map_strength() const {
	return glow_map_strength;
}

void Environment::set_glow_map(Ref<Texture> p_glow_map) {
	glow_map = p_glow_map;
	_update_glow();
}

Ref<Texture> Environment::get_glow_map() const {
	return glow_map;
}

void Environment::_update_glow() {
	Vector<float> normalized_levels;
	if (glow_normalize_levels) {
		normalized_levels.resize(7);
		float size = 0.0;
		for (int i = 0; i < glow_levels.size(); i++) {
			size += glow_levels[i];
		}
		for (int i = 0; i < glow_levels.size(); i++) {
			normalized_levels.write[i] = glow_levels[i] / size;
		}
	} else {
		normalized_levels = glow_levels;
	}

	float _glow_map_strength = 0.0f;
	RID glow_map_rid;
	if (glow_map.is_valid()) {
		glow_map_rid = glow_map->get_rid();
		_glow_map_strength = glow_map_strength;
	} else {
		glow_map_rid = RID();
	}

	RS::get_singleton()->environment_set_glow(
			environment,
			glow_enabled,
			normalized_levels,
			glow_intensity,
			glow_strength,
			glow_mix,
			glow_bloom,
			RS::EnvironmentGlowBlendMode(glow_blend_mode),
			glow_hdr_bleed_threshold,
			glow_hdr_bleed_scale,
			glow_hdr_luminance_cap,
			_glow_map_strength,
			glow_map_rid);
}

// Fog

void Environment::set_fog_enabled(bool p_enabled) {
	fog_enabled = p_enabled;
	_update_fog();
	notify_property_list_changed();
}

bool Environment::is_fog_enabled() const {
	return fog_enabled;
}

void Environment::set_fog_mode(FogMode p_mode) {
	if (fog_mode != p_mode && p_mode == FogMode::FOG_MODE_EXPONENTIAL) {
		set_fog_density(0.01);
	} else {
		set_fog_density(1.0);
	}
	fog_mode = p_mode;
	_update_fog();
	notify_property_list_changed();
}

Environment::FogMode Environment::get_fog_mode() const {
	return fog_mode;
}

void Environment::set_fog_light_color(const Color &p_light_color) {
	fog_light_color = p_light_color;
	_update_fog();
}
Color Environment::get_fog_light_color() const {
	return fog_light_color;
}
void Environment::set_fog_light_energy(float p_amount) {
	fog_light_energy = p_amount;
	_update_fog();
}
float Environment::get_fog_light_energy() const {
	return fog_light_energy;
}
void Environment::set_fog_sun_scatter(float p_amount) {
	fog_sun_scatter = p_amount;
	_update_fog();
}
float Environment::get_fog_sun_scatter() const {
	return fog_sun_scatter;
}
void Environment::set_fog_density(float p_amount) {
	fog_density = p_amount;
	_update_fog();
}
float Environment::get_fog_density() const {
	return fog_density;
}
void Environment::set_fog_height(float p_amount) {
	fog_height = p_amount;
	_update_fog();
}
float Environment::get_fog_height() const {
	return fog_height;
}
void Environment::set_fog_height_density(float p_amount) {
	fog_height_density = p_amount;
	_update_fog();
}
float Environment::get_fog_height_density() const {
	return fog_height_density;
}

void Environment::set_fog_aerial_perspective(float p_aerial_perspective) {
	fog_aerial_perspective = p_aerial_perspective;
	_update_fog();
}
float Environment::get_fog_aerial_perspective() const {
	return fog_aerial_perspective;
}

void Environment::set_fog_sky_affect(float p_sky_affect) {
	fog_sky_affect = p_sky_affect;
	_update_fog();
}

float Environment::get_fog_sky_affect() const {
	return fog_sky_affect;
}

void Environment::_update_fog() {
	RS::get_singleton()->environment_set_fog(
			environment,
			fog_enabled,
			fog_light_color,
			fog_light_energy,
			fog_sun_scatter,
			fog_density,
			fog_height,
			fog_height_density,
			fog_aerial_perspective,
			fog_sky_affect,
			RS::EnvironmentFogMode(fog_mode));
}

// Depth Fog

void Environment::set_fog_depth_curve(float p_curve) {
	fog_depth_curve = p_curve;
	_update_fog_depth();
}

float Environment::get_fog_depth_curve() const {
	return fog_depth_curve;
}

void Environment::set_fog_depth_begin(float p_begin) {
	fog_depth_begin = p_begin;
	if (fog_depth_begin > fog_depth_end) {
		set_fog_depth_end(fog_depth_begin);
	}
	_update_fog_depth();
}

float Environment::get_fog_depth_begin() const {
	return fog_depth_begin;
}

void Environment::set_fog_depth_end(float p_end) {
	fog_depth_end = p_end;
	if (fog_depth_end < fog_depth_begin) {
		set_fog_depth_begin(fog_depth_end);
	}
	_update_fog_depth();
}

float Environment::get_fog_depth_end() const {
	return fog_depth_end;
}

void Environment::_update_fog_depth() {
	RS::get_singleton()->environment_set_fog_depth(
			environment,
			fog_depth_curve,
			fog_depth_begin,
			fog_depth_end);
}

// Volumetric Fog

void Environment::_update_volumetric_fog() {
	RS::get_singleton()->environment_set_volumetric_fog(
			environment,
			volumetric_fog_enabled,
			volumetric_fog_density,
			volumetric_fog_albedo,
			volumetric_fog_emission,
			volumetric_fog_emission_energy,
			volumetric_fog_anisotropy,
			volumetric_fog_length,
			volumetric_fog_detail_spread,
			volumetric_fog_gi_inject,
			volumetric_fog_temporal_reproject,
			volumetric_fog_temporal_reproject_amount,
			volumetric_fog_ambient_inject,
			volumetric_fog_sky_affect);
}

void Environment::set_volumetric_fog_enabled(bool p_enable) {
	volumetric_fog_enabled = p_enable;
	_update_volumetric_fog();
	notify_property_list_changed();
}

bool Environment::is_volumetric_fog_enabled() const {
	return volumetric_fog_enabled;
}
void Environment::set_volumetric_fog_density(float p_density) {
	volumetric_fog_density = p_density;
	_update_volumetric_fog();
}
float Environment::get_volumetric_fog_density() const {
	return volumetric_fog_density;
}
void Environment::set_volumetric_fog_albedo(Color p_color) {
	volumetric_fog_albedo = p_color;
	_update_volumetric_fog();
}
Color Environment::get_volumetric_fog_albedo() const {
	return volumetric_fog_albedo;
}
void Environment::set_volumetric_fog_emission(Color p_color) {
	volumetric_fog_emission = p_color;
	_update_volumetric_fog();
}
Color Environment::get_volumetric_fog_emission() const {
	return volumetric_fog_emission;
}
void Environment::set_volumetric_fog_emission_energy(float p_begin) {
	volumetric_fog_emission_energy = p_begin;
	_update_volumetric_fog();
}
float Environment::get_volumetric_fog_emission_energy() const {
	return volumetric_fog_emission_energy;
}
void Environment::set_volumetric_fog_anisotropy(float p_anisotropy) {
	volumetric_fog_anisotropy = p_anisotropy;
	_update_volumetric_fog();
}
float Environment::get_volumetric_fog_anisotropy() const {
	return volumetric_fog_anisotropy;
}
void Environment::set_volumetric_fog_length(float p_length) {
	volumetric_fog_length = p_length;
	_update_volumetric_fog();
}
float Environment::get_volumetric_fog_length() const {
	return volumetric_fog_length;
}
void Environment::set_volumetric_fog_detail_spread(float p_detail_spread) {
	p_detail_spread = CLAMP(p_detail_spread, 0.5, 6.0);
	volumetric_fog_detail_spread = p_detail_spread;
	_update_volumetric_fog();
}
float Environment::get_volumetric_fog_detail_spread() const {
	return volumetric_fog_detail_spread;
}

void Environment::set_volumetric_fog_gi_inject(float p_gi_inject) {
	volumetric_fog_gi_inject = p_gi_inject;
	_update_volumetric_fog();
}
float Environment::get_volumetric_fog_gi_inject() const {
	return volumetric_fog_gi_inject;
}
void Environment::set_volumetric_fog_ambient_inject(float p_ambient_inject) {
	volumetric_fog_ambient_inject = p_ambient_inject;
	_update_volumetric_fog();
}
float Environment::get_volumetric_fog_ambient_inject() const {
	return volumetric_fog_ambient_inject;
}

void Environment::set_volumetric_fog_sky_affect(float p_sky_affect) {
	volumetric_fog_sky_affect = p_sky_affect;
	_update_volumetric_fog();
}

float Environment::get_volumetric_fog_sky_affect() const {
	return volumetric_fog_sky_affect;
}

void Environment::set_volumetric_fog_temporal_reprojection_enabled(bool p_enable) {
	volumetric_fog_temporal_reproject = p_enable;
	_update_volumetric_fog();
}
bool Environment::is_volumetric_fog_temporal_reprojection_enabled() const {
	return volumetric_fog_temporal_reproject;
}
void Environment::set_volumetric_fog_temporal_reprojection_amount(float p_amount) {
	volumetric_fog_temporal_reproject_amount = p_amount;
	_update_volumetric_fog();
}

float Environment::get_volumetric_fog_temporal_reprojection_amount() const {
	return volumetric_fog_temporal_reproject_amount;
}

// Adjustment

void Environment::set_adjustment_enabled(bool p_enabled) {
	adjustment_enabled = p_enabled;
	_update_adjustment();
	notify_property_list_changed();
}

bool Environment::is_adjustment_enabled() const {
	return adjustment_enabled;
}

void Environment::set_adjustment_brightness(float p_brightness) {
	adjustment_brightness = p_brightness;
	_update_adjustment();
}

float Environment::get_adjustment_brightness() const {
	return adjustment_brightness;
}

void Environment::set_adjustment_contrast(float p_contrast) {
	adjustment_contrast = p_contrast;
	_update_adjustment();
}

float Environment::get_adjustment_contrast() const {
	return adjustment_contrast;
}

void Environment::set_adjustment_saturation(float p_saturation) {
	adjustment_saturation = p_saturation;
	_update_adjustment();
}

float Environment::get_adjustment_saturation() const {
	return adjustment_saturation;
}

void Environment::set_adjustment_color_correction(Ref<Texture> p_color_correction) {
	adjustment_color_correction = p_color_correction;
	Ref<GradientTexture1D> grad_tex = p_color_correction;
	if (grad_tex.is_valid()) {
		grad_tex->connect_changed(callable_mp(this, &Environment::_update_adjustment));
	}
	Ref<Texture2D> adjustment_texture_2d = adjustment_color_correction;
	if (adjustment_texture_2d.is_valid()) {
		use_1d_color_correction = true;
	} else {
		use_1d_color_correction = false;
	}
	_update_adjustment();
}

Ref<Texture> Environment::get_adjustment_color_correction() const {
	return adjustment_color_correction;
}

void Environment::_update_adjustment() {
	RID color_correction = adjustment_color_correction.is_valid() ? adjustment_color_correction->get_rid() : RID();

	RS::get_singleton()->environment_set_adjustment(
			environment,
			adjustment_enabled,
			adjustment_brightness,
			adjustment_contrast,
			adjustment_saturation,
			use_1d_color_correction,
			color_correction);
}

// Private methods, constructor and destructor

void Environment::_validate_property(PropertyInfo &p_property) const {
	if (p_property.name == "sky" || p_property.name == "sky_custom_fov" || p_property.name == "sky_rotation" || p_property.name == "ambient_light_sky_contribution") {
		if (bg_mode != BG_SKY && ambient_source != AMBIENT_SOURCE_SKY && reflection_source != REFLECTION_SOURCE_SKY) {
			p_property.usage = PROPERTY_USAGE_NO_EDITOR;
		}
	}

	if (p_property.name == "fog_depth_curve" || p_property.name == "fog_depth_begin" || p_property.name == "fog_depth_end") {
		if (fog_mode == FOG_MODE_EXPONENTIAL) {
			p_property.usage = PROPERTY_USAGE_NO_EDITOR;
		}
	}

	if (p_property.name == "ambient_light_color" || p_property.name == "ambient_light_energy") {
		if (ambient_source == AMBIENT_SOURCE_DISABLED) {
			p_property.usage = PROPERTY_USAGE_NO_EDITOR;
		}
	}

	if (p_property.name == "ambient_light_sky_contribution") {
		if (ambient_source == AMBIENT_SOURCE_DISABLED || ambient_source == AMBIENT_SOURCE_COLOR) {
			p_property.usage = PROPERTY_USAGE_NO_EDITOR;
		}
	}

	if (p_property.name == "fog_aerial_perspective") {
		if (bg_mode != BG_SKY) {
			p_property.usage = PROPERTY_USAGE_NO_EDITOR;
		}
	}

	if (p_property.name == "tonemap_white" && (tone_mapper == TONE_MAPPER_LINEAR || tone_mapper == TONE_MAPPER_AGX)) {
		// Whitepoint adjustment is not available with AgX or linear as it's hardcoded there.
		p_property.usage = PROPERTY_USAGE_NO_EDITOR;
	}

	if (p_property.name == "glow_intensity" && glow_blend_mode == GLOW_BLEND_MODE_MIX) {
		p_property.usage = PROPERTY_USAGE_NO_EDITOR;
	}

	if (OS::get_singleton()->get_current_rendering_method() == "gl_compatibility") {
		// Hide glow properties we do not support in GL Compatibility.
		if (p_property.name.begins_with("glow_levels") || p_property.name == "glow_normalized" || p_property.name == "glow_strength" || p_property.name == "glow_mix" || p_property.name == "glow_blend_mode" || p_property.name == "glow_map_strength" || p_property.name == "glow_map") {
			p_property.usage = PROPERTY_USAGE_NO_EDITOR;
		}
	} else {
		if (p_property.name == "glow_mix" && glow_blend_mode != GLOW_BLEND_MODE_MIX) {
			p_property.usage = PROPERTY_USAGE_NO_EDITOR;
		}
	}

	if (p_property.name == "background_color") {
		if (bg_mode != BG_COLOR && ambient_source != AMBIENT_SOURCE_COLOR) {
			p_property.usage = PROPERTY_USAGE_NO_EDITOR;
		}
	}

	if (p_property.name == "background_canvas_max_layer") {
		if (bg_mode != BG_CANVAS) {
			p_property.usage = PROPERTY_USAGE_NO_EDITOR;
		}
	}

	if (p_property.name == "background_camera_feed_id") {
		if (bg_mode != BG_CAMERA_FEED) {
			p_property.usage = PROPERTY_USAGE_NO_EDITOR;
		}
	}

	if (p_property.name == "background_intensity" && !GLOBAL_GET("rendering/lights_and_shadows/use_physical_light_units")) {
		p_property.usage = PROPERTY_USAGE_NO_EDITOR;
	}

	static const char *hide_prefixes[] = {
		"fog_",
		"volumetric_fog_",
		"ssr_",
		"ssao_",
		"ssil_",
		"sdfgi_",
		"glow_",
		"adjustment_",
		nullptr

	};

	const char **prefixes = hide_prefixes;
	while (*prefixes) {
		String prefix = String(*prefixes);

		String enabled = prefix + "enabled";
		if (p_property.name.begins_with(prefix) && p_property.name != enabled && !bool(get(enabled))) {
			p_property.usage = PROPERTY_USAGE_NO_EDITOR;
			return;
		}

		prefixes++;
	}
}

#ifndef DISABLE_DEPRECATED
// Kept for compatibility from 3.x to 4.0.
bool Environment::_set(const StringName &p_name, const Variant &p_value) {
	if (p_name == "background_sky") {
		set_sky(p_value);
		return true;
	} else if (p_name == "background_sky_custom_fov") {
		set_sky_custom_fov(p_value);
		return true;
	} else if (p_name == "background_sky_orientation") {
		Vector3 euler = p_value.operator Basis().get_euler();
		set_sky_rotation(euler);
		return true;
	} else {
		return false;
	}
}
#endif

void Environment::_bind_methods() {
	// Background

	ClassDB::bind_method(D_METHOD("set_background", "mode"), &Environment::set_background);
	ClassDB::bind_method(D_METHOD("get_background"), &Environment::get_background);
	ClassDB::bind_method(D_METHOD("set_sky", "sky"), &Environment::set_sky);
	ClassDB::bind_method(D_METHOD("get_sky"), &Environment::get_sky);
	ClassDB::bind_method(D_METHOD("set_sky_custom_fov", "scale"), &Environment::set_sky_custom_fov);
	ClassDB::bind_method(D_METHOD("get_sky_custom_fov"), &Environment::get_sky_custom_fov);
	ClassDB::bind_method(D_METHOD("set_sky_rotation", "euler_radians"), &Environment::set_sky_rotation);
	ClassDB::bind_method(D_METHOD("get_sky_rotation"), &Environment::get_sky_rotation);
	ClassDB::bind_method(D_METHOD("set_bg_color", "color"), &Environment::set_bg_color);
	ClassDB::bind_method(D_METHOD("get_bg_color"), &Environment::get_bg_color);
	ClassDB::bind_method(D_METHOD("set_bg_energy_multiplier", "energy"), &Environment::set_bg_energy_multiplier);
	ClassDB::bind_method(D_METHOD("get_bg_energy_multiplier"), &Environment::get_bg_energy_multiplier);
	ClassDB::bind_method(D_METHOD("set_bg_intensity", "energy"), &Environment::set_bg_intensity);
	ClassDB::bind_method(D_METHOD("get_bg_intensity"), &Environment::get_bg_intensity);
	ClassDB::bind_method(D_METHOD("set_canvas_max_layer", "layer"), &Environment::set_canvas_max_layer);
	ClassDB::bind_method(D_METHOD("get_canvas_max_layer"), &Environment::get_canvas_max_layer);
	ClassDB::bind_method(D_METHOD("set_camera_feed_id", "id"), &Environment::set_camera_feed_id);
	ClassDB::bind_method(D_METHOD("get_camera_feed_id"), &Environment::get_camera_feed_id);

	ADD_GROUP("Background", "background_");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "background_mode", PROPERTY_HINT_ENUM, "Clear Color,Custom Color,Sky,Canvas,Keep,Camera Feed"), "set_background", "get_background");
	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "background_color"), "set_bg_color", "get_bg_color");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "background_energy_multiplier", PROPERTY_HINT_RANGE, "0,16,0.01"), "set_bg_energy_multiplier", "get_bg_energy_multiplier");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "background_intensity", PROPERTY_HINT_RANGE, "0,100000,0.01,suffix:nt"), "set_bg_intensity", "get_bg_intensity");

	ADD_PROPERTY(PropertyInfo(Variant::INT, "background_canvas_max_layer", PROPERTY_HINT_RANGE, "-1000,1000,1"), "set_canvas_max_layer", "get_canvas_max_layer");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "background_camera_feed_id", PROPERTY_HINT_RANGE, "1,10,1"), "set_camera_feed_id", "get_camera_feed_id");

	ADD_GROUP("Sky", "sky_");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "sky", PROPERTY_HINT_RESOURCE_TYPE, "Sky"), "set_sky", "get_sky");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "sky_custom_fov", PROPERTY_HINT_RANGE, "0,180,0.1,degrees"), "set_sky_custom_fov", "get_sky_custom_fov");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "sky_rotation", PROPERTY_HINT_RANGE, "-360,360,0.1,or_less,or_greater,radians_as_degrees"), "set_sky_rotation", "get_sky_rotation");

	// Ambient light

	ClassDB::bind_method(D_METHOD("set_ambient_light_color", "color"), &Environment::set_ambient_light_color);
	ClassDB::bind_method(D_METHOD("get_ambient_light_color"), &Environment::get_ambient_light_color);
	ClassDB::bind_method(D_METHOD("set_ambient_source", "source"), &Environment::set_ambient_source);
	ClassDB::bind_method(D_METHOD("get_ambient_source"), &Environment::get_ambient_source);
	ClassDB::bind_method(D_METHOD("set_ambient_light_energy", "energy"), &Environment::set_ambient_light_energy);
	ClassDB::bind_method(D_METHOD("get_ambient_light_energy"), &Environment::get_ambient_light_energy);
	ClassDB::bind_method(D_METHOD("set_ambient_light_sky_contribution", "ratio"), &Environment::set_ambient_light_sky_contribution);
	ClassDB::bind_method(D_METHOD("get_ambient_light_sky_contribution"), &Environment::get_ambient_light_sky_contribution);
	ClassDB::bind_method(D_METHOD("set_reflection_source", "source"), &Environment::set_reflection_source);
	ClassDB::bind_method(D_METHOD("get_reflection_source"), &Environment::get_reflection_source);

	ADD_GROUP("Ambient Light", "ambient_light_");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "ambient_light_source", PROPERTY_HINT_ENUM, "Background,Disabled,Color,Sky"), "set_ambient_source", "get_ambient_source");
	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "ambient_light_color"), "set_ambient_light_color", "get_ambient_light_color");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "ambient_light_sky_contribution", PROPERTY_HINT_RANGE, "0,1,0.01"), "set_ambient_light_sky_contribution", "get_ambient_light_sky_contribution");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "ambient_light_energy", PROPERTY_HINT_RANGE, "0,16,0.01"), "set_ambient_light_energy", "get_ambient_light_energy");

	ADD_GROUP("Reflected Light", "reflected_light_");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "reflected_light_source", PROPERTY_HINT_ENUM, "Background,Disabled,Sky"), "set_reflection_source", "get_reflection_source");

	// Tonemap

	ClassDB::bind_method(D_METHOD("set_tonemapper", "mode"), &Environment::set_tonemapper);
	ClassDB::bind_method(D_METHOD("get_tonemapper"), &Environment::get_tonemapper);
	ClassDB::bind_method(D_METHOD("set_tonemap_exposure", "exposure"), &Environment::set_tonemap_exposure);
	ClassDB::bind_method(D_METHOD("get_tonemap_exposure"), &Environment::get_tonemap_exposure);
	ClassDB::bind_method(D_METHOD("set_tonemap_white", "white"), &Environment::set_tonemap_white);
	ClassDB::bind_method(D_METHOD("get_tonemap_white"), &Environment::get_tonemap_white);

	ADD_GROUP("Tonemap", "tonemap_");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "tonemap_mode", PROPERTY_HINT_ENUM, "Linear,Reinhard,Filmic,ACES,AgX"), "set_tonemapper", "get_tonemapper");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "tonemap_exposure", PROPERTY_HINT_RANGE, "0,16,0.01"), "set_tonemap_exposure", "get_tonemap_exposure");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "tonemap_white", PROPERTY_HINT_RANGE, "0,16,0.01"), "set_tonemap_white", "get_tonemap_white");

	// SSR

	ClassDB::bind_method(D_METHOD("set_ssr_enabled", "enabled"), &Environment::set_ssr_enabled);
	ClassDB::bind_method(D_METHOD("is_ssr_enabled"), &Environment::is_ssr_enabled);
	ClassDB::bind_method(D_METHOD("set_ssr_max_steps", "max_steps"), &Environment::set_ssr_max_steps);
	ClassDB::bind_method(D_METHOD("get_ssr_max_steps"), &Environment::get_ssr_max_steps);
	ClassDB::bind_method(D_METHOD("set_ssr_fade_in", "fade_in"), &Environment::set_ssr_fade_in);
	ClassDB::bind_method(D_METHOD("get_ssr_fade_in"), &Environment::get_ssr_fade_in);
	ClassDB::bind_method(D_METHOD("set_ssr_fade_out", "fade_out"), &Environment::set_ssr_fade_out);
	ClassDB::bind_method(D_METHOD("get_ssr_fade_out"), &Environment::get_ssr_fade_out);
	ClassDB::bind_method(D_METHOD("set_ssr_depth_tolerance", "depth_tolerance"), &Environment::set_ssr_depth_tolerance);
	ClassDB::bind_method(D_METHOD("get_ssr_depth_tolerance"), &Environment::get_ssr_depth_tolerance);

	ADD_GROUP("SSR", "ssr_");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "ssr_enabled"), "set_ssr_enabled", "is_ssr_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "ssr_max_steps", PROPERTY_HINT_RANGE, "1,512,1"), "set_ssr_max_steps", "get_ssr_max_steps");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "ssr_fade_in", PROPERTY_HINT_EXP_EASING, "positive_only"), "set_ssr_fade_in", "get_ssr_fade_in");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "ssr_fade_out", PROPERTY_HINT_EXP_EASING, "positive_only"), "set_ssr_fade_out", "get_ssr_fade_out");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "ssr_depth_tolerance", PROPERTY_HINT_RANGE, "0.01,128,0.1"), "set_ssr_depth_tolerance", "get_ssr_depth_tolerance");

	// SSAO
	ClassDB::bind_method(D_METHOD("set_ssao_enabled", "enabled"), &Environment::set_ssao_enabled);
	ClassDB::bind_method(D_METHOD("is_ssao_enabled"), &Environment::is_ssao_enabled);
	ClassDB::bind_method(D_METHOD("set_ssao_radius", "radius"), &Environment::set_ssao_radius);
	ClassDB::bind_method(D_METHOD("get_ssao_radius"), &Environment::get_ssao_radius);
	ClassDB::bind_method(D_METHOD("set_ssao_intensity", "intensity"), &Environment::set_ssao_intensity);
	ClassDB::bind_method(D_METHOD("get_ssao_intensity"), &Environment::get_ssao_intensity);
	ClassDB::bind_method(D_METHOD("set_ssao_power", "power"), &Environment::set_ssao_power);
	ClassDB::bind_method(D_METHOD("get_ssao_power"), &Environment::get_ssao_power);
	ClassDB::bind_method(D_METHOD("set_ssao_detail", "detail"), &Environment::set_ssao_detail);
	ClassDB::bind_method(D_METHOD("get_ssao_detail"), &Environment::get_ssao_detail);
	ClassDB::bind_method(D_METHOD("set_ssao_horizon", "horizon"), &Environment::set_ssao_horizon);
	ClassDB::bind_method(D_METHOD("get_ssao_horizon"), &Environment::get_ssao_horizon);
	ClassDB::bind_method(D_METHOD("set_ssao_sharpness", "sharpness"), &Environment::set_ssao_sharpness);
	ClassDB::bind_method(D_METHOD("get_ssao_sharpness"), &Environment::get_ssao_sharpness);
	ClassDB::bind_method(D_METHOD("set_ssao_direct_light_affect", "amount"), &Environment::set_ssao_direct_light_affect);
	ClassDB::bind_method(D_METHOD("get_ssao_direct_light_affect"), &Environment::get_ssao_direct_light_affect);
	ClassDB::bind_method(D_METHOD("set_ssao_ao_channel_affect", "amount"), &Environment::set_ssao_ao_channel_affect);
	ClassDB::bind_method(D_METHOD("get_ssao_ao_channel_affect"), &Environment::get_ssao_ao_channel_affect);

	ADD_GROUP("SSAO", "ssao_");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "ssao_enabled"), "set_ssao_enabled", "is_ssao_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "ssao_radius", PROPERTY_HINT_RANGE, "0.01,16,0.01,or_greater"), "set_ssao_radius", "get_ssao_radius");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "ssao_intensity", PROPERTY_HINT_RANGE, "0,16,0.01,or_greater"), "set_ssao_intensity", "get_ssao_intensity");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "ssao_power", PROPERTY_HINT_EXP_EASING, "positive_only"), "set_ssao_power", "get_ssao_power");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "ssao_detail", PROPERTY_HINT_RANGE, "0,5,0.01"), "set_ssao_detail", "get_ssao_detail");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "ssao_horizon", PROPERTY_HINT_RANGE, "0,1,0.01"), "set_ssao_horizon", "get_ssao_horizon");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "ssao_sharpness", PROPERTY_HINT_RANGE, "0,1,0.01"), "set_ssao_sharpness", "get_ssao_sharpness");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "ssao_light_affect", PROPERTY_HINT_RANGE, "0.00,1,0.01"), "set_ssao_direct_light_affect", "get_ssao_direct_light_affect");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "ssao_ao_channel_affect", PROPERTY_HINT_RANGE, "0.00,1,0.01"), "set_ssao_ao_channel_affect", "get_ssao_ao_channel_affect");

	// SSIL
	ClassDB::bind_method(D_METHOD("set_ssil_enabled", "enabled"), &Environment::set_ssil_enabled);
	ClassDB::bind_method(D_METHOD("is_ssil_enabled"), &Environment::is_ssil_enabled);
	ClassDB::bind_method(D_METHOD("set_ssil_radius", "radius"), &Environment::set_ssil_radius);
	ClassDB::bind_method(D_METHOD("get_ssil_radius"), &Environment::get_ssil_radius);
	ClassDB::bind_method(D_METHOD("set_ssil_intensity", "intensity"), &Environment::set_ssil_intensity);
	ClassDB::bind_method(D_METHOD("get_ssil_intensity"), &Environment::get_ssil_intensity);
	ClassDB::bind_method(D_METHOD("set_ssil_sharpness", "sharpness"), &Environment::set_ssil_sharpness);
	ClassDB::bind_method(D_METHOD("get_ssil_sharpness"), &Environment::get_ssil_sharpness);
	ClassDB::bind_method(D_METHOD("set_ssil_normal_rejection", "normal_rejection"), &Environment::set_ssil_normal_rejection);
	ClassDB::bind_method(D_METHOD("get_ssil_normal_rejection"), &Environment::get_ssil_normal_rejection);

	ADD_GROUP("SSIL", "ssil_");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "ssil_enabled"), "set_ssil_enabled", "is_ssil_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "ssil_radius", PROPERTY_HINT_RANGE, "0.01,16,0.01,or_greater,suffix:m"), "set_ssil_radius", "get_ssil_radius");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "ssil_intensity", PROPERTY_HINT_RANGE, "0,16,0.01,or_greater"), "set_ssil_intensity", "get_ssil_intensity");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "ssil_sharpness", PROPERTY_HINT_RANGE, "0,1,0.01"), "set_ssil_sharpness", "get_ssil_sharpness");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "ssil_normal_rejection", PROPERTY_HINT_RANGE, "0,1,0.01"), "set_ssil_normal_rejection", "get_ssil_normal_rejection");

	// SDFGI

	ClassDB::bind_method(D_METHOD("set_sdfgi_enabled", "enabled"), &Environment::set_sdfgi_enabled);
	ClassDB::bind_method(D_METHOD("is_sdfgi_enabled"), &Environment::is_sdfgi_enabled);
	ClassDB::bind_method(D_METHOD("set_sdfgi_cascades", "amount"), &Environment::set_sdfgi_cascades);
	ClassDB::bind_method(D_METHOD("get_sdfgi_cascades"), &Environment::get_sdfgi_cascades);
	ClassDB::bind_method(D_METHOD("set_sdfgi_min_cell_size", "size"), &Environment::set_sdfgi_min_cell_size);
	ClassDB::bind_method(D_METHOD("get_sdfgi_min_cell_size"), &Environment::get_sdfgi_min_cell_size);
	ClassDB::bind_method(D_METHOD("set_sdfgi_max_distance", "distance"), &Environment::set_sdfgi_max_distance);
	ClassDB::bind_method(D_METHOD("get_sdfgi_max_distance"), &Environment::get_sdfgi_max_distance);
	ClassDB::bind_method(D_METHOD("set_sdfgi_cascade0_distance", "distance"), &Environment::set_sdfgi_cascade0_distance);
	ClassDB::bind_method(D_METHOD("get_sdfgi_cascade0_distance"), &Environment::get_sdfgi_cascade0_distance);
	ClassDB::bind_method(D_METHOD("set_sdfgi_y_scale", "scale"), &Environment::set_sdfgi_y_scale);
	ClassDB::bind_method(D_METHOD("get_sdfgi_y_scale"), &Environment::get_sdfgi_y_scale);
	ClassDB::bind_method(D_METHOD("set_sdfgi_use_occlusion", "enable"), &Environment::set_sdfgi_use_occlusion);
	ClassDB::bind_method(D_METHOD("is_sdfgi_using_occlusion"), &Environment::is_sdfgi_using_occlusion);
	ClassDB::bind_method(D_METHOD("set_sdfgi_bounce_feedback", "amount"), &Environment::set_sdfgi_bounce_feedback);
	ClassDB::bind_method(D_METHOD("get_sdfgi_bounce_feedback"), &Environment::get_sdfgi_bounce_feedback);
	ClassDB::bind_method(D_METHOD("set_sdfgi_read_sky_light", "enable"), &Environment::set_sdfgi_read_sky_light);
	ClassDB::bind_method(D_METHOD("is_sdfgi_reading_sky_light"), &Environment::is_sdfgi_reading_sky_light);
	ClassDB::bind_method(D_METHOD("set_sdfgi_energy", "amount"), &Environment::set_sdfgi_energy);
	ClassDB::bind_method(D_METHOD("get_sdfgi_energy"), &Environment::get_sdfgi_energy);
	ClassDB::bind_method(D_METHOD("set_sdfgi_normal_bias", "bias"), &Environment::set_sdfgi_normal_bias);
	ClassDB::bind_method(D_METHOD("get_sdfgi_normal_bias"), &Environment::get_sdfgi_normal_bias);
	ClassDB::bind_method(D_METHOD("set_sdfgi_probe_bias", "bias"), &Environment::set_sdfgi_probe_bias);
	ClassDB::bind_method(D_METHOD("get_sdfgi_probe_bias"), &Environment::get_sdfgi_probe_bias);

	ADD_GROUP("SDFGI", "sdfgi_");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "sdfgi_enabled"), "set_sdfgi_enabled", "is_sdfgi_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "sdfgi_use_occlusion"), "set_sdfgi_use_occlusion", "is_sdfgi_using_occlusion");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "sdfgi_read_sky_light"), "set_sdfgi_read_sky_light", "is_sdfgi_reading_sky_light");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "sdfgi_bounce_feedback", PROPERTY_HINT_RANGE, "0,1.99,0.01"), "set_sdfgi_bounce_feedback", "get_sdfgi_bounce_feedback");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "sdfgi_cascades", PROPERTY_HINT_RANGE, "1,8,1"), "set_sdfgi_cascades", "get_sdfgi_cascades");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "sdfgi_min_cell_size", PROPERTY_HINT_RANGE, "0.01,64,0.01"), "set_sdfgi_min_cell_size", "get_sdfgi_min_cell_size");
	// Don't store the values of `sdfgi_cascade0_distance` and `sdfgi_max_distance`
	// as they're derived from `sdfgi_min_cell_size`.
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "sdfgi_cascade0_distance", PROPERTY_HINT_RANGE, "0.1,16384,0.1,or_greater", PROPERTY_USAGE_EDITOR), "set_sdfgi_cascade0_distance", "get_sdfgi_cascade0_distance");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "sdfgi_max_distance", PROPERTY_HINT_RANGE, "0.1,16384,0.1,or_greater", PROPERTY_USAGE_EDITOR), "set_sdfgi_max_distance", "get_sdfgi_max_distance");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "sdfgi_y_scale", PROPERTY_HINT_ENUM, "50% (Compact),75% (Balanced),100% (Sparse)"), "set_sdfgi_y_scale", "get_sdfgi_y_scale");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "sdfgi_energy"), "set_sdfgi_energy", "get_sdfgi_energy");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "sdfgi_normal_bias"), "set_sdfgi_normal_bias", "get_sdfgi_normal_bias");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "sdfgi_probe_bias"), "set_sdfgi_probe_bias", "get_sdfgi_probe_bias");

	// Glow

	ClassDB::bind_method(D_METHOD("set_glow_enabled", "enabled"), &Environment::set_glow_enabled);
	ClassDB::bind_method(D_METHOD("is_glow_enabled"), &Environment::is_glow_enabled);
	ClassDB::bind_method(D_METHOD("set_glow_level", "idx", "intensity"), &Environment::set_glow_level);
	ClassDB::bind_method(D_METHOD("get_glow_level", "idx"), &Environment::get_glow_level);
	ClassDB::bind_method(D_METHOD("set_glow_normalized", "normalize"), &Environment::set_glow_normalized);
	ClassDB::bind_method(D_METHOD("is_glow_normalized"), &Environment::is_glow_normalized);
	ClassDB::bind_method(D_METHOD("set_glow_intensity", "intensity"), &Environment::set_glow_intensity);
	ClassDB::bind_method(D_METHOD("get_glow_intensity"), &Environment::get_glow_intensity);
	ClassDB::bind_method(D_METHOD("set_glow_strength", "strength"), &Environment::set_glow_strength);
	ClassDB::bind_method(D_METHOD("get_glow_strength"), &Environment::get_glow_strength);
	ClassDB::bind_method(D_METHOD("set_glow_mix", "mix"), &Environment::set_glow_mix);
	ClassDB::bind_method(D_METHOD("get_glow_mix"), &Environment::get_glow_mix);
	ClassDB::bind_method(D_METHOD("set_glow_bloom", "amount"), &Environment::set_glow_bloom);
	ClassDB::bind_method(D_METHOD("get_glow_bloom"), &Environment::get_glow_bloom);
	ClassDB::bind_method(D_METHOD("set_glow_blend_mode", "mode"), &Environment::set_glow_blend_mode);
	ClassDB::bind_method(D_METHOD("get_glow_blend_mode"), &Environment::get_glow_blend_mode);
	ClassDB::bind_method(D_METHOD("set_glow_hdr_bleed_threshold", "threshold"), &Environment::set_glow_hdr_bleed_threshold);
	ClassDB::bind_method(D_METHOD("get_glow_hdr_bleed_threshold"), &Environment::get_glow_hdr_bleed_threshold);
	ClassDB::bind_method(D_METHOD("set_glow_hdr_bleed_scale", "scale"), &Environment::set_glow_hdr_bleed_scale);
	ClassDB::bind_method(D_METHOD("get_glow_hdr_bleed_scale"), &Environment::get_glow_hdr_bleed_scale);
	ClassDB::bind_method(D_METHOD("set_glow_hdr_luminance_cap", "amount"), &Environment::set_glow_hdr_luminance_cap);
	ClassDB::bind_method(D_METHOD("get_glow_hdr_luminance_cap"), &Environment::get_glow_hdr_luminance_cap);
	ClassDB::bind_method(D_METHOD("set_glow_map_strength", "strength"), &Environment::set_glow_map_strength);
	ClassDB::bind_method(D_METHOD("get_glow_map_strength"), &Environment::get_glow_map_strength);
	ClassDB::bind_method(D_METHOD("set_glow_map", "mode"), &Environment::set_glow_map);
	ClassDB::bind_method(D_METHOD("get_glow_map"), &Environment::get_glow_map);

	ADD_GROUP("Glow", "glow_");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "glow_enabled"), "set_glow_enabled", "is_glow_enabled");
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "glow_levels/1", PROPERTY_HINT_RANGE, "0,16,0.01,or_greater"), "set_glow_level", "get_glow_level", 0);
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "glow_levels/2", PROPERTY_HINT_RANGE, "0,16,0.01,or_greater"), "set_glow_level", "get_glow_level", 1);
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "glow_levels/3", PROPERTY_HINT_RANGE, "0,16,0.01,or_greater"), "set_glow_level", "get_glow_level", 2);
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "glow_levels/4", PROPERTY_HINT_RANGE, "0,16,0.01,or_greater"), "set_glow_level", "get_glow_level", 3);
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "glow_levels/5", PROPERTY_HINT_RANGE, "0,16,0.01,or_greater"), "set_glow_level", "get_glow_level", 4);
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "glow_levels/6", PROPERTY_HINT_RANGE, "0,16,0.01,or_greater"), "set_glow_level", "get_glow_level", 5);
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "glow_levels/7", PROPERTY_HINT_RANGE, "0,16,0.01,or_greater"), "set_glow_level", "get_glow_level", 6);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "glow_normalized"), "set_glow_normalized", "is_glow_normalized");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "glow_intensity", PROPERTY_HINT_RANGE, "0.0,8.0,0.01"), "set_glow_intensity", "get_glow_intensity");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "glow_strength", PROPERTY_HINT_RANGE, "0.0,2.0,0.01"), "set_glow_strength", "get_glow_strength");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "glow_mix", PROPERTY_HINT_RANGE, "0.0,1.0,0.001"), "set_glow_mix", "get_glow_mix");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "glow_bloom", PROPERTY_HINT_RANGE, "0.0,1.0,0.01"), "set_glow_bloom", "get_glow_bloom");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "glow_blend_mode", PROPERTY_HINT_ENUM, "Additive,Screen,Softlight,Replace,Mix"), "set_glow_blend_mode", "get_glow_blend_mode");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "glow_hdr_threshold", PROPERTY_HINT_RANGE, "0.0,4.0,0.01"), "set_glow_hdr_bleed_threshold", "get_glow_hdr_bleed_threshold");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "glow_hdr_scale", PROPERTY_HINT_RANGE, "0.0,4.0,0.01"), "set_glow_hdr_bleed_scale", "get_glow_hdr_bleed_scale");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "glow_hdr_luminance_cap", PROPERTY_HINT_RANGE, "0.0,256.0,0.01"), "set_glow_hdr_luminance_cap", "get_glow_hdr_luminance_cap");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "glow_map_strength", PROPERTY_HINT_RANGE, "0.0,1.0,0.01"), "set_glow_map_strength", "get_glow_map_strength");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "glow_map", PROPERTY_HINT_RESOURCE_TYPE, "Texture2D"), "set_glow_map", "get_glow_map");

	// Fog

	ClassDB::bind_method(D_METHOD("set_fog_enabled", "enabled"), &Environment::set_fog_enabled);
	ClassDB::bind_method(D_METHOD("is_fog_enabled"), &Environment::is_fog_enabled);
	ClassDB::bind_method(D_METHOD("set_fog_mode", "mode"), &Environment::set_fog_mode);
	ClassDB::bind_method(D_METHOD("get_fog_mode"), &Environment::get_fog_mode);
	ClassDB::bind_method(D_METHOD("set_fog_light_color", "light_color"), &Environment::set_fog_light_color);
	ClassDB::bind_method(D_METHOD("get_fog_light_color"), &Environment::get_fog_light_color);
	ClassDB::bind_method(D_METHOD("set_fog_light_energy", "light_energy"), &Environment::set_fog_light_energy);
	ClassDB::bind_method(D_METHOD("get_fog_light_energy"), &Environment::get_fog_light_energy);
	ClassDB::bind_method(D_METHOD("set_fog_sun_scatter", "sun_scatter"), &Environment::set_fog_sun_scatter);
	ClassDB::bind_method(D_METHOD("get_fog_sun_scatter"), &Environment::get_fog_sun_scatter);

	ClassDB::bind_method(D_METHOD("set_fog_density", "density"), &Environment::set_fog_density);
	ClassDB::bind_method(D_METHOD("get_fog_density"), &Environment::get_fog_density);

	ClassDB::bind_method(D_METHOD("set_fog_height", "height"), &Environment::set_fog_height);
	ClassDB::bind_method(D_METHOD("get_fog_height"), &Environment::get_fog_height);

	ClassDB::bind_method(D_METHOD("set_fog_height_density", "height_density"), &Environment::set_fog_height_density);
	ClassDB::bind_method(D_METHOD("get_fog_height_density"), &Environment::get_fog_height_density);

	ClassDB::bind_method(D_METHOD("set_fog_aerial_perspective", "aerial_perspective"), &Environment::set_fog_aerial_perspective);
	ClassDB::bind_method(D_METHOD("get_fog_aerial_perspective"), &Environment::get_fog_aerial_perspective);

	ClassDB::bind_method(D_METHOD("set_fog_sky_affect", "sky_affect"), &Environment::set_fog_sky_affect);
	ClassDB::bind_method(D_METHOD("get_fog_sky_affect"), &Environment::get_fog_sky_affect);

	ClassDB::bind_method(D_METHOD("set_fog_depth_curve", "curve"), &Environment::set_fog_depth_curve);
	ClassDB::bind_method(D_METHOD("get_fog_depth_curve"), &Environment::get_fog_depth_curve);
	ClassDB::bind_method(D_METHOD("set_fog_depth_begin", "begin"), &Environment::set_fog_depth_begin);
	ClassDB::bind_method(D_METHOD("get_fog_depth_begin"), &Environment::get_fog_depth_begin);
	ClassDB::bind_method(D_METHOD("set_fog_depth_end", "end"), &Environment::set_fog_depth_end);
	ClassDB::bind_method(D_METHOD("get_fog_depth_end"), &Environment::get_fog_depth_end);

	ADD_GROUP("Fog", "fog_");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "fog_enabled"), "set_fog_enabled", "is_fog_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "fog_mode", PROPERTY_HINT_ENUM, "Exponential,Depth"), "set_fog_mode", "get_fog_mode");
	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "fog_light_color", PROPERTY_HINT_COLOR_NO_ALPHA), "set_fog_light_color", "get_fog_light_color");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "fog_light_energy", PROPERTY_HINT_RANGE, "0,16,0.01,or_greater"), "set_fog_light_energy", "get_fog_light_energy");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "fog_sun_scatter", PROPERTY_HINT_RANGE, "0,1,0.01,or_greater"), "set_fog_sun_scatter", "get_fog_sun_scatter");

	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "fog_density", PROPERTY_HINT_RANGE, "0,1,0.0001,or_greater"), "set_fog_density", "get_fog_density");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "fog_aerial_perspective", PROPERTY_HINT_RANGE, "0,1,0.001"), "set_fog_aerial_perspective", "get_fog_aerial_perspective");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "fog_sky_affect", PROPERTY_HINT_RANGE, "0,1,0.001"), "set_fog_sky_affect", "get_fog_sky_affect");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "fog_height", PROPERTY_HINT_RANGE, "-1024,1024,0.01,or_less,or_greater,suffix:m"), "set_fog_height", "get_fog_height");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "fog_height_density", PROPERTY_HINT_RANGE, "-16,16,0.0001,or_less,or_greater"), "set_fog_height_density", "get_fog_height_density");

	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "fog_depth_curve", PROPERTY_HINT_EXP_EASING), "set_fog_depth_curve", "get_fog_depth_curve");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "fog_depth_begin", PROPERTY_HINT_RANGE, "0,4000,0.1,or_greater,or_less,suffix:m"), "set_fog_depth_begin", "get_fog_depth_begin");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "fog_depth_end", PROPERTY_HINT_RANGE, "0,4000,0.1,or_greater,or_less,suffix:m"), "set_fog_depth_end", "get_fog_depth_end");

	ClassDB::bind_method(D_METHOD("set_volumetric_fog_enabled", "enabled"), &Environment::set_volumetric_fog_enabled);
	ClassDB::bind_method(D_METHOD("is_volumetric_fog_enabled"), &Environment::is_volumetric_fog_enabled);
	ClassDB::bind_method(D_METHOD("set_volumetric_fog_emission", "color"), &Environment::set_volumetric_fog_emission);
	ClassDB::bind_method(D_METHOD("get_volumetric_fog_emission"), &Environment::get_volumetric_fog_emission);
	ClassDB::bind_method(D_METHOD("set_volumetric_fog_albedo", "color"), &Environment::set_volumetric_fog_albedo);
	ClassDB::bind_method(D_METHOD("get_volumetric_fog_albedo"), &Environment::get_volumetric_fog_albedo);
	ClassDB::bind_method(D_METHOD("set_volumetric_fog_density", "density"), &Environment::set_volumetric_fog_density);
	ClassDB::bind_method(D_METHOD("get_volumetric_fog_density"), &Environment::get_volumetric_fog_density);
	ClassDB::bind_method(D_METHOD("set_volumetric_fog_emission_energy", "begin"), &Environment::set_volumetric_fog_emission_energy);
	ClassDB::bind_method(D_METHOD("get_volumetric_fog_emission_energy"), &Environment::get_volumetric_fog_emission_energy);
	ClassDB::bind_method(D_METHOD("set_volumetric_fog_anisotropy", "anisotropy"), &Environment::set_volumetric_fog_anisotropy);
	ClassDB::bind_method(D_METHOD("get_volumetric_fog_anisotropy"), &Environment::get_volumetric_fog_anisotropy);
	ClassDB::bind_method(D_METHOD("set_volumetric_fog_length", "length"), &Environment::set_volumetric_fog_length);
	ClassDB::bind_method(D_METHOD("get_volumetric_fog_length"), &Environment::get_volumetric_fog_length);
	ClassDB::bind_method(D_METHOD("set_volumetric_fog_detail_spread", "detail_spread"), &Environment::set_volumetric_fog_detail_spread);
	ClassDB::bind_method(D_METHOD("get_volumetric_fog_detail_spread"), &Environment::get_volumetric_fog_detail_spread);
	ClassDB::bind_method(D_METHOD("set_volumetric_fog_gi_inject", "gi_inject"), &Environment::set_volumetric_fog_gi_inject);
	ClassDB::bind_method(D_METHOD("get_volumetric_fog_gi_inject"), &Environment::get_volumetric_fog_gi_inject);
	ClassDB::bind_method(D_METHOD("set_volumetric_fog_ambient_inject", "enabled"), &Environment::set_volumetric_fog_ambient_inject);
	ClassDB::bind_method(D_METHOD("get_volumetric_fog_ambient_inject"), &Environment::get_volumetric_fog_ambient_inject);
	ClassDB::bind_method(D_METHOD("set_volumetric_fog_sky_affect", "sky_affect"), &Environment::set_volumetric_fog_sky_affect);
	ClassDB::bind_method(D_METHOD("get_volumetric_fog_sky_affect"), &Environment::get_volumetric_fog_sky_affect);
	ClassDB::bind_method(D_METHOD("set_volumetric_fog_temporal_reprojection_enabled", "enabled"), &Environment::set_volumetric_fog_temporal_reprojection_enabled);
	ClassDB::bind_method(D_METHOD("is_volumetric_fog_temporal_reprojection_enabled"), &Environment::is_volumetric_fog_temporal_reprojection_enabled);
	ClassDB::bind_method(D_METHOD("set_volumetric_fog_temporal_reprojection_amount", "temporal_reprojection_amount"), &Environment::set_volumetric_fog_temporal_reprojection_amount);
	ClassDB::bind_method(D_METHOD("get_volumetric_fog_temporal_reprojection_amount"), &Environment::get_volumetric_fog_temporal_reprojection_amount);

	ADD_GROUP("Volumetric Fog", "volumetric_fog_");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "volumetric_fog_enabled"), "set_volumetric_fog_enabled", "is_volumetric_fog_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "volumetric_fog_density", PROPERTY_HINT_RANGE, "0,1,0.0001,or_greater"), "set_volumetric_fog_density", "get_volumetric_fog_density");
	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "volumetric_fog_albedo", PROPERTY_HINT_COLOR_NO_ALPHA), "set_volumetric_fog_albedo", "get_volumetric_fog_albedo");
	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "volumetric_fog_emission", PROPERTY_HINT_COLOR_NO_ALPHA), "set_volumetric_fog_emission", "get_volumetric_fog_emission");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "volumetric_fog_emission_energy", PROPERTY_HINT_RANGE, "0,1024,0.01,or_greater"), "set_volumetric_fog_emission_energy", "get_volumetric_fog_emission_energy");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "volumetric_fog_gi_inject", PROPERTY_HINT_RANGE, "0.0,16,0.01,exp"), "set_volumetric_fog_gi_inject", "get_volumetric_fog_gi_inject");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "volumetric_fog_anisotropy", PROPERTY_HINT_RANGE, "-0.9,0.9,0.01"), "set_volumetric_fog_anisotropy", "get_volumetric_fog_anisotropy");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "volumetric_fog_length", PROPERTY_HINT_RANGE, "0.01,1024,0.01,or_greater,suffix:m"), "set_volumetric_fog_length", "get_volumetric_fog_length");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "volumetric_fog_detail_spread", PROPERTY_HINT_EXP_EASING, "positive_only"), "set_volumetric_fog_detail_spread", "get_volumetric_fog_detail_spread");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "volumetric_fog_ambient_inject", PROPERTY_HINT_RANGE, "0.0,16,0.01,exp"), "set_volumetric_fog_ambient_inject", "get_volumetric_fog_ambient_inject");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "volumetric_fog_sky_affect", PROPERTY_HINT_RANGE, "0,1,0.001"), "set_volumetric_fog_sky_affect", "get_volumetric_fog_sky_affect");
	ADD_SUBGROUP("Temporal Reprojection", "volumetric_fog_temporal_reprojection_");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "volumetric_fog_temporal_reprojection_enabled"), "set_volumetric_fog_temporal_reprojection_enabled", "is_volumetric_fog_temporal_reprojection_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "volumetric_fog_temporal_reprojection_amount", PROPERTY_HINT_RANGE, "0.5,0.99,0.001"), "set_volumetric_fog_temporal_reprojection_amount", "get_volumetric_fog_temporal_reprojection_amount");

	// Adjustment

	ClassDB::bind_method(D_METHOD("set_adjustment_enabled", "enabled"), &Environment::set_adjustment_enabled);
	ClassDB::bind_method(D_METHOD("is_adjustment_enabled"), &Environment::is_adjustment_enabled);
	ClassDB::bind_method(D_METHOD("set_adjustment_brightness", "brightness"), &Environment::set_adjustment_brightness);
	ClassDB::bind_method(D_METHOD("get_adjustment_brightness"), &Environment::get_adjustment_brightness);
	ClassDB::bind_method(D_METHOD("set_adjustment_contrast", "contrast"), &Environment::set_adjustment_contrast);
	ClassDB::bind_method(D_METHOD("get_adjustment_contrast"), &Environment::get_adjustment_contrast);
	ClassDB::bind_method(D_METHOD("set_adjustment_saturation", "saturation"), &Environment::set_adjustment_saturation);
	ClassDB::bind_method(D_METHOD("get_adjustment_saturation"), &Environment::get_adjustment_saturation);
	ClassDB::bind_method(D_METHOD("set_adjustment_color_correction", "color_correction"), &Environment::set_adjustment_color_correction);
	ClassDB::bind_method(D_METHOD("get_adjustment_color_correction"), &Environment::get_adjustment_color_correction);

	ADD_GROUP("Adjustments", "adjustment_");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "adjustment_enabled"), "set_adjustment_enabled", "is_adjustment_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "adjustment_brightness", PROPERTY_HINT_RANGE, "0.01,8,0.01"), "set_adjustment_brightness", "get_adjustment_brightness");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "adjustment_contrast", PROPERTY_HINT_RANGE, "0.01,8,0.01"), "set_adjustment_contrast", "get_adjustment_contrast");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "adjustment_saturation", PROPERTY_HINT_RANGE, "0.01,8,0.01"), "set_adjustment_saturation", "get_adjustment_saturation");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "adjustment_color_correction", PROPERTY_HINT_RESOURCE_TYPE, "Texture2D,Texture3D"), "set_adjustment_color_correction", "get_adjustment_color_correction");

	// Constants

	BIND_ENUM_CONSTANT(BG_CLEAR_COLOR);
	BIND_ENUM_CONSTANT(BG_COLOR);
	BIND_ENUM_CONSTANT(BG_SKY);
	BIND_ENUM_CONSTANT(BG_CANVAS);
	BIND_ENUM_CONSTANT(BG_KEEP);
	BIND_ENUM_CONSTANT(BG_CAMERA_FEED);
	BIND_ENUM_CONSTANT(BG_MAX);

	BIND_ENUM_CONSTANT(AMBIENT_SOURCE_BG);
	BIND_ENUM_CONSTANT(AMBIENT_SOURCE_DISABLED);
	BIND_ENUM_CONSTANT(AMBIENT_SOURCE_COLOR);
	BIND_ENUM_CONSTANT(AMBIENT_SOURCE_SKY);

	BIND_ENUM_CONSTANT(REFLECTION_SOURCE_BG);
	BIND_ENUM_CONSTANT(REFLECTION_SOURCE_DISABLED);
	BIND_ENUM_CONSTANT(REFLECTION_SOURCE_SKY);

	BIND_ENUM_CONSTANT(TONE_MAPPER_LINEAR);
	BIND_ENUM_CONSTANT(TONE_MAPPER_REINHARDT);
	BIND_ENUM_CONSTANT(TONE_MAPPER_FILMIC);
	BIND_ENUM_CONSTANT(TONE_MAPPER_ACES);
	BIND_ENUM_CONSTANT(TONE_MAPPER_AGX);

	BIND_ENUM_CONSTANT(GLOW_BLEND_MODE_ADDITIVE);
	BIND_ENUM_CONSTANT(GLOW_BLEND_MODE_SCREEN);
	BIND_ENUM_CONSTANT(GLOW_BLEND_MODE_SOFTLIGHT);
	BIND_ENUM_CONSTANT(GLOW_BLEND_MODE_REPLACE);
	BIND_ENUM_CONSTANT(GLOW_BLEND_MODE_MIX);

	BIND_ENUM_CONSTANT(FOG_MODE_EXPONENTIAL);
	BIND_ENUM_CONSTANT(FOG_MODE_DEPTH);

	BIND_ENUM_CONSTANT(SDFGI_Y_SCALE_50_PERCENT);
	BIND_ENUM_CONSTANT(SDFGI_Y_SCALE_75_PERCENT);
	BIND_ENUM_CONSTANT(SDFGI_Y_SCALE_100_PERCENT);
}

Environment::Environment() {
	environment = RS::get_singleton()->environment_create();

	set_camera_feed_id(bg_camera_feed_id);

	glow_levels.resize(7);
	glow_levels.write[0] = 0.0;
	glow_levels.write[1] = 0.0;
	glow_levels.write[2] = 1.0;
	glow_levels.write[3] = 0.0;
	glow_levels.write[4] = 1.0;
	glow_levels.write[5] = 0.0;
	glow_levels.write[6] = 0.0;

	_update_ambient_light();
	_update_tonemap();
	_update_ssr();
	_update_ssao();
	_update_ssil();
	_update_sdfgi();
	_update_glow();
	_update_fog();
	_update_adjustment();
	_update_volumetric_fog();
	_update_bg_energy();
	notify_property_list_changed();
}

Environment::~Environment() {
	ERR_FAIL_NULL(RenderingServer::get_singleton());
	RS::get_singleton()->free(environment);
}

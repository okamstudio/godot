/**************************************************************************/
/*  environment_storage.cpp                                               */
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

#include "environment_storage.h"

// Storage

RendererEnvironmentStorage *RendererEnvironmentStorage::singleton = nullptr;

RendererEnvironmentStorage::RendererEnvironmentStorage() {
	singleton = this;
}

RendererEnvironmentStorage::~RendererEnvironmentStorage() {
	singleton = nullptr;
}

// Environment

RID RendererEnvironmentStorage::environment_allocate() {
	return environment_owner.allocate_rid();
}

void RendererEnvironmentStorage::environment_initialize(RID p_rid) {
	environment_owner.initialize_rid(p_rid, Environment());
}

void RendererEnvironmentStorage::environment_free(RID p_rid) {
	environment_owner.free(p_rid);
}

// Background

void RendererEnvironmentStorage::environment_set_background(RID p_env, RS::EnvironmentBG p_bg) {
	Environment *env = environment_owner.get_or_null(p_env);
	ERR_FAIL_NULL(env);
	env->background = p_bg;
}

void RendererEnvironmentStorage::environment_set_sky(RID p_env, RID p_sky) {
	Environment *env = environment_owner.get_or_null(p_env);
	ERR_FAIL_NULL(env);
	env->sky = p_sky;
}

void RendererEnvironmentStorage::environment_set_sky_custom_fov(RID p_env, float p_scale) {
	Environment *env = environment_owner.get_or_null(p_env);
	ERR_FAIL_NULL(env);
	env->sky_custom_fov = p_scale;
}

void RendererEnvironmentStorage::environment_set_sky_orientation(RID p_env, const Basis &p_orientation) {
	Environment *env = environment_owner.get_or_null(p_env);
	ERR_FAIL_NULL(env);
	env->sky_orientation = p_orientation;
}

void RendererEnvironmentStorage::environment_set_bg_color(RID p_env, const Color &p_color) {
	Environment *env = environment_owner.get_or_null(p_env);
	ERR_FAIL_NULL(env);
	env->bg_color = p_color;
}

void RendererEnvironmentStorage::environment_set_bg_energy(RID p_env, float p_multiplier, float p_intensity) {
	Environment *env = environment_owner.get_or_null(p_env);
	ERR_FAIL_NULL(env);
	env->bg_energy_multiplier = p_multiplier;
	env->bg_intensity = p_intensity;
}

void RendererEnvironmentStorage::environment_set_canvas_max_layer(RID p_env, int p_max_layer) {
	Environment *env = environment_owner.get_or_null(p_env);
	ERR_FAIL_NULL(env);
	env->canvas_max_layer = p_max_layer;
}

void RendererEnvironmentStorage::environment_set_ambient_light(RID p_env, const Color &p_color, RS::EnvironmentAmbientSource p_ambient, float p_energy, float p_sky_contribution, RS::EnvironmentReflectionSource p_reflection_source) {
	Environment *env = environment_owner.get_or_null(p_env);
	ERR_FAIL_NULL(env);
	env->ambient_light = p_color;
	env->ambient_source = p_ambient;
	env->ambient_light_energy = p_energy;
	env->ambient_sky_contribution = p_sky_contribution;
	env->reflection_source = p_reflection_source;
}

RS::EnvironmentBG RendererEnvironmentStorage::environment_get_background(RID p_env) const {
	Environment *env = environment_owner.get_or_null(p_env);
	ERR_FAIL_NULL_V(env, RS::ENV_BG_CLEAR_COLOR);
	return env->background;
}

RID RendererEnvironmentStorage::environment_get_sky(RID p_env) const {
	Environment *env = environment_owner.get_or_null(p_env);
	ERR_FAIL_NULL_V(env, RID());
	return env->sky;
}

float RendererEnvironmentStorage::environment_get_sky_custom_fov(RID p_env) const {
	Environment *env = environment_owner.get_or_null(p_env);
	ERR_FAIL_NULL_V(env, 0.0);
	return env->sky_custom_fov;
}

Basis RendererEnvironmentStorage::environment_get_sky_orientation(RID p_env) const {
	Environment *env = environment_owner.get_or_null(p_env);
	ERR_FAIL_NULL_V(env, Basis());
	return env->sky_orientation;
}

Color RendererEnvironmentStorage::environment_get_bg_color(RID p_env) const {
	Environment *env = environment_owner.get_or_null(p_env);
	ERR_FAIL_NULL_V(env, Color());
	return env->bg_color;
}

float RendererEnvironmentStorage::environment_get_bg_energy_multiplier(RID p_env) const {
	Environment *env = environment_owner.get_or_null(p_env);
	ERR_FAIL_NULL_V(env, 1.0);
	return env->bg_energy_multiplier;
}

float RendererEnvironmentStorage::environment_get_bg_intensity(RID p_env) const {
	Environment *env = environment_owner.get_or_null(p_env);
	ERR_FAIL_NULL_V(env, 1.0);
	return env->bg_intensity;
}

int RendererEnvironmentStorage::environment_get_canvas_max_layer(RID p_env) const {
	Environment *env = environment_owner.get_or_null(p_env);
	ERR_FAIL_NULL_V(env, 0);
	return env->canvas_max_layer;
}

RS::EnvironmentAmbientSource RendererEnvironmentStorage::environment_get_ambient_source(RID p_env) const {
	Environment *env = environment_owner.get_or_null(p_env);
	ERR_FAIL_NULL_V(env, RS::ENV_AMBIENT_SOURCE_BG);
	return env->ambient_source;
}

Color RendererEnvironmentStorage::environment_get_ambient_light(RID p_env) const {
	Environment *env = environment_owner.get_or_null(p_env);
	ERR_FAIL_NULL_V(env, Color());
	return env->ambient_light;
}

float RendererEnvironmentStorage::environment_get_ambient_light_energy(RID p_env) const {
	Environment *env = environment_owner.get_or_null(p_env);
	ERR_FAIL_NULL_V(env, 1.0);
	return env->ambient_light_energy;
}

float RendererEnvironmentStorage::environment_get_ambient_sky_contribution(RID p_env) const {
	Environment *env = environment_owner.get_or_null(p_env);
	ERR_FAIL_NULL_V(env, 1.0);
	return env->ambient_sky_contribution;
}

RS::EnvironmentReflectionSource RendererEnvironmentStorage::environment_get_reflection_source(RID p_env) const {
	Environment *env = environment_owner.get_or_null(p_env);
	ERR_FAIL_NULL_V(env, RS::ENV_REFLECTION_SOURCE_BG);
	return env->reflection_source;
}

void RendererEnvironmentStorage::environment_set_camera_feed_id(RID p_env, int p_camera_feed_id) {
	Environment *env = environment_owner.get_or_null(p_env);
	ERR_FAIL_NULL(env);
	env->camera_feed_id = p_camera_feed_id;
}

int RendererEnvironmentStorage::environment_get_camera_feed_id(RID p_env) const {
	Environment *env = environment_owner.get_or_null(p_env);
	ERR_FAIL_NULL_V(env, -1);
	return env->camera_feed_id;
}

// Tonemap

void RendererEnvironmentStorage::environment_set_tonemap(RID p_env, RS::EnvironmentToneMapper p_tone_mapper, float p_exposure, float p_white) {
	Environment *env = environment_owner.get_or_null(p_env);
	ERR_FAIL_NULL(env);
	env->exposure = p_exposure;
	env->tone_mapper = p_tone_mapper;
	env->white = p_white;
}

RS::EnvironmentToneMapper RendererEnvironmentStorage::environment_get_tone_mapper(RID p_env) const {
	Environment *env = environment_owner.get_or_null(p_env);
	ERR_FAIL_NULL_V(env, RS::ENV_TONE_MAPPER_LINEAR);
	return env->tone_mapper;
}

float RendererEnvironmentStorage::environment_get_exposure(RID p_env) const {
	Environment *env = environment_owner.get_or_null(p_env);
	ERR_FAIL_NULL_V(env, 1.0);
	return env->exposure;
}

float RendererEnvironmentStorage::environment_get_white(RID p_env) const {
	Environment *env = environment_owner.get_or_null(p_env);
	ERR_FAIL_NULL_V(env, 1.0);
	return env->white;
}

// Fog

void RendererEnvironmentStorage::environment_set_fog(RID p_env, bool p_enable, const Color &p_light_color, float p_light_energy, float p_sun_scatter, float p_density, float p_height, float p_height_density, float p_fog_aerial_perspective, float p_sky_affect, RS::EnvironmentFogMode p_mode) {
	Environment *env = environment_owner.get_or_null(p_env);
	ERR_FAIL_NULL(env);
	env->fog_enabled = p_enable;
	env->fog_mode = p_mode;
	env->fog_light_color = p_light_color;
	env->fog_light_energy = p_light_energy;
	env->fog_sun_scatter = p_sun_scatter;
	env->fog_density = p_density;
	env->fog_height = p_height;
	env->fog_height_density = p_height_density;
	env->fog_aerial_perspective = p_fog_aerial_perspective;
	env->fog_sky_affect = p_sky_affect;
}

bool RendererEnvironmentStorage::environment_get_fog_enabled(RID p_env) const {
	Environment *env = environment_owner.get_or_null(p_env);
	ERR_FAIL_NULL_V(env, false);
	return env->fog_enabled;
}

RS::EnvironmentFogMode RendererEnvironmentStorage::environment_get_fog_mode(RID p_env) const {
	Environment *env = environment_owner.get_or_null(p_env);
	ERR_FAIL_NULL_V(env, RS::ENV_FOG_MODE_EXPONENTIAL);
	return env->fog_mode;
}

Color RendererEnvironmentStorage::environment_get_fog_light_color(RID p_env) const {
	Environment *env = environment_owner.get_or_null(p_env);
	ERR_FAIL_NULL_V(env, Color(0.5, 0.6, 0.7));
	return env->fog_light_color;
}

float RendererEnvironmentStorage::environment_get_fog_light_energy(RID p_env) const {
	Environment *env = environment_owner.get_or_null(p_env);
	ERR_FAIL_NULL_V(env, 1.0);
	return env->fog_light_energy;
}

float RendererEnvironmentStorage::environment_get_fog_sun_scatter(RID p_env) const {
	Environment *env = environment_owner.get_or_null(p_env);
	ERR_FAIL_NULL_V(env, 0.0);
	return env->fog_sun_scatter;
}

float RendererEnvironmentStorage::environment_get_fog_density(RID p_env) const {
	Environment *env = environment_owner.get_or_null(p_env);
	ERR_FAIL_NULL_V(env, 0.001);
	return env->fog_density;
}

float RendererEnvironmentStorage::environment_get_fog_height(RID p_env) const {
	Environment *env = environment_owner.get_or_null(p_env);
	ERR_FAIL_NULL_V(env, 0.0);
	return env->fog_height;
}

float RendererEnvironmentStorage::environment_get_fog_height_density(RID p_env) const {
	Environment *env = environment_owner.get_or_null(p_env);
	ERR_FAIL_NULL_V(env, 0.0);
	return env->fog_height_density;
}

float RendererEnvironmentStorage::environment_get_fog_aerial_perspective(RID p_env) const {
	Environment *env = environment_owner.get_or_null(p_env);
	ERR_FAIL_NULL_V(env, 0.0);
	return env->fog_aerial_perspective;
}

float RendererEnvironmentStorage::environment_get_fog_sky_affect(RID p_env) const {
	Environment *env = environment_owner.get_or_null(p_env);
	ERR_FAIL_NULL_V(env, 0.0);
	return env->fog_sky_affect;
}

// Depth Fog

void RendererEnvironmentStorage::environment_set_fog_depth(RID p_env, float p_curve, float p_begin, float p_end) {
	Environment *env = environment_owner.get_or_null(p_env);
	ERR_FAIL_NULL(env);
	env->fog_depth_curve = p_curve;
	env->fog_depth_begin = p_begin;
	env->fog_depth_end = p_end;
}

float RendererEnvironmentStorage::environment_get_fog_depth_curve(RID p_env) const {
	Environment *env = environment_owner.get_or_null(p_env);
	ERR_FAIL_NULL_V(env, 0.0);
	return env->fog_depth_curve;
}

float RendererEnvironmentStorage::environment_get_fog_depth_begin(RID p_env) const {
	Environment *env = environment_owner.get_or_null(p_env);
	ERR_FAIL_NULL_V(env, 0.0);
	return env->fog_depth_begin;
}

float RendererEnvironmentStorage::environment_get_fog_depth_end(RID p_env) const {
	Environment *env = environment_owner.get_or_null(p_env);
	ERR_FAIL_NULL_V(env, 0.0);
	return env->fog_depth_end;
}

// Volumetric Fog

void RendererEnvironmentStorage::environment_set_volumetric_fog(RID p_env, bool p_enable, float p_density, const Color &p_albedo, const Color &p_emission, float p_emission_energy, float p_anisotropy, float p_length, float p_detail_spread, float p_gi_inject, bool p_temporal_reprojection, float p_temporal_reprojection_amount, float p_ambient_inject, float p_sky_affect) {
	Environment *env = environment_owner.get_or_null(p_env);
	ERR_FAIL_NULL(env);
#ifdef DEBUG_ENABLED
	if (OS::get_singleton()->get_current_rendering_method() != "forward_plus" && p_enable) {
		WARN_PRINT_ONCE_ED("Volumetric fog can only be enabled when using the Forward+ renderer.");
	}
#endif
	env->volumetric_fog_enabled = p_enable;
	env->volumetric_fog_density = p_density;
	env->volumetric_fog_scattering = p_albedo;
	env->volumetric_fog_emission = p_emission;
	env->volumetric_fog_emission_energy = p_emission_energy;
	env->volumetric_fog_anisotropy = p_anisotropy;
	env->volumetric_fog_length = p_length;
	env->volumetric_fog_detail_spread = p_detail_spread;
	env->volumetric_fog_gi_inject = p_gi_inject;
	env->volumetric_fog_temporal_reprojection = p_temporal_reprojection;
	env->volumetric_fog_temporal_reprojection_amount = p_temporal_reprojection_amount;
	env->volumetric_fog_ambient_inject = p_ambient_inject;
	env->volumetric_fog_sky_affect = p_sky_affect;
}

bool RendererEnvironmentStorage::environment_get_volumetric_fog_enabled(RID p_env) const {
	Environment *env = environment_owner.get_or_null(p_env);
	ERR_FAIL_NULL_V(env, false);
	return env->volumetric_fog_enabled;
}

float RendererEnvironmentStorage::environment_get_volumetric_fog_density(RID p_env) const {
	Environment *env = environment_owner.get_or_null(p_env);
	ERR_FAIL_NULL_V(env, 0.01);
	return env->volumetric_fog_density;
}

Color RendererEnvironmentStorage::environment_get_volumetric_fog_scattering(RID p_env) const {
	Environment *env = environment_owner.get_or_null(p_env);
	ERR_FAIL_NULL_V(env, Color(1, 1, 1));
	return env->volumetric_fog_scattering;
}

Color RendererEnvironmentStorage::environment_get_volumetric_fog_emission(RID p_env) const {
	Environment *env = environment_owner.get_or_null(p_env);
	ERR_FAIL_NULL_V(env, Color(0, 0, 0));
	return env->volumetric_fog_emission;
}

float RendererEnvironmentStorage::environment_get_volumetric_fog_emission_energy(RID p_env) const {
	Environment *env = environment_owner.get_or_null(p_env);
	ERR_FAIL_NULL_V(env, 0.0);
	return env->volumetric_fog_emission_energy;
}

float RendererEnvironmentStorage::environment_get_volumetric_fog_anisotropy(RID p_env) const {
	Environment *env = environment_owner.get_or_null(p_env);
	ERR_FAIL_NULL_V(env, 0.2);
	return env->volumetric_fog_anisotropy;
}

float RendererEnvironmentStorage::environment_get_volumetric_fog_length(RID p_env) const {
	Environment *env = environment_owner.get_or_null(p_env);
	ERR_FAIL_NULL_V(env, 64.0);
	return env->volumetric_fog_length;
}

float RendererEnvironmentStorage::environment_get_volumetric_fog_detail_spread(RID p_env) const {
	Environment *env = environment_owner.get_or_null(p_env);
	ERR_FAIL_NULL_V(env, 2.0);
	return env->volumetric_fog_detail_spread;
}

float RendererEnvironmentStorage::environment_get_volumetric_fog_gi_inject(RID p_env) const {
	Environment *env = environment_owner.get_or_null(p_env);
	ERR_FAIL_NULL_V(env, 0.0);
	return env->volumetric_fog_gi_inject;
}

float RendererEnvironmentStorage::environment_get_volumetric_fog_sky_affect(RID p_env) const {
	Environment *env = environment_owner.get_or_null(p_env);
	ERR_FAIL_NULL_V(env, 0.0);
	return env->volumetric_fog_sky_affect;
}

bool RendererEnvironmentStorage::environment_get_volumetric_fog_temporal_reprojection(RID p_env) const {
	Environment *env = environment_owner.get_or_null(p_env);
	ERR_FAIL_NULL_V(env, true);
	return env->volumetric_fog_temporal_reprojection;
}

float RendererEnvironmentStorage::environment_get_volumetric_fog_temporal_reprojection_amount(RID p_env) const {
	Environment *env = environment_owner.get_or_null(p_env);
	ERR_FAIL_NULL_V(env, 0.9);
	return env->volumetric_fog_temporal_reprojection_amount;
}

float RendererEnvironmentStorage::environment_get_volumetric_fog_ambient_inject(RID p_env) const {
	Environment *env = environment_owner.get_or_null(p_env);
	ERR_FAIL_NULL_V(env, 0.0);
	return env->volumetric_fog_ambient_inject;
}

// GLOW

void RendererEnvironmentStorage::environment_set_glow(RID p_env, bool p_enable, Vector<float> p_levels, float p_intensity, float p_strength, float p_mix, float p_bloom_threshold, RS::EnvironmentGlowBlendMode p_blend_mode, float p_hdr_bleed_threshold, float p_hdr_bleed_scale, float p_hdr_luminance_cap, float p_glow_map_strength, RID p_glow_map) {
	Environment *env = environment_owner.get_or_null(p_env);
	ERR_FAIL_NULL(env);
	ERR_FAIL_COND_MSG(p_levels.size() != 7, "Size of array of glow levels must be 7");
	env->glow_enabled = p_enable;
	env->glow_levels = p_levels;
	env->glow_intensity = p_intensity;
	env->glow_strength = p_strength;
	env->glow_mix = p_mix;
	env->glow_bloom = p_bloom_threshold;
	env->glow_blend_mode = p_blend_mode;
	env->glow_hdr_bleed_threshold = p_hdr_bleed_threshold;
	env->glow_hdr_bleed_scale = p_hdr_bleed_scale;
	env->glow_hdr_luminance_cap = p_hdr_luminance_cap;
	env->glow_map_strength = p_glow_map_strength;
	env->glow_map = p_glow_map;
}

bool RendererEnvironmentStorage::environment_get_glow_enabled(RID p_env) const {
	Environment *env = environment_owner.get_or_null(p_env);
	ERR_FAIL_NULL_V(env, false);
	return env->glow_enabled;
}

Vector<float> RendererEnvironmentStorage::environment_get_glow_levels(RID p_env) const {
	Environment *env = environment_owner.get_or_null(p_env);
	ERR_FAIL_NULL_V(env, Vector<float>());
	return env->glow_levels;
}

float RendererEnvironmentStorage::environment_get_glow_intensity(RID p_env) const {
	Environment *env = environment_owner.get_or_null(p_env);
	ERR_FAIL_NULL_V(env, 0.8);
	return env->glow_intensity;
}

float RendererEnvironmentStorage::environment_get_glow_strength(RID p_env) const {
	Environment *env = environment_owner.get_or_null(p_env);
	ERR_FAIL_NULL_V(env, 1.0);
	return env->glow_strength;
}

float RendererEnvironmentStorage::environment_get_glow_bloom(RID p_env) const {
	Environment *env = environment_owner.get_or_null(p_env);
	ERR_FAIL_NULL_V(env, 0.0);
	return env->glow_bloom;
}

float RendererEnvironmentStorage::environment_get_glow_mix(RID p_env) const {
	Environment *env = environment_owner.get_or_null(p_env);
	ERR_FAIL_NULL_V(env, 0.01);
	return env->glow_mix;
}

RS::EnvironmentGlowBlendMode RendererEnvironmentStorage::environment_get_glow_blend_mode(RID p_env) const {
	Environment *env = environment_owner.get_or_null(p_env);
	ERR_FAIL_NULL_V(env, RS::ENV_GLOW_BLEND_MODE_SOFTLIGHT);
	return env->glow_blend_mode;
}

float RendererEnvironmentStorage::environment_get_glow_hdr_bleed_threshold(RID p_env) const {
	Environment *env = environment_owner.get_or_null(p_env);
	ERR_FAIL_NULL_V(env, 1.0);
	return env->glow_hdr_bleed_threshold;
}

float RendererEnvironmentStorage::environment_get_glow_hdr_luminance_cap(RID p_env) const {
	Environment *env = environment_owner.get_or_null(p_env);
	ERR_FAIL_NULL_V(env, 12.0);
	return env->glow_hdr_luminance_cap;
}

float RendererEnvironmentStorage::environment_get_glow_hdr_bleed_scale(RID p_env) const {
	Environment *env = environment_owner.get_or_null(p_env);
	ERR_FAIL_NULL_V(env, 2.0);
	return env->glow_hdr_bleed_scale;
}

float RendererEnvironmentStorage::environment_get_glow_map_strength(RID p_env) const {
	Environment *env = environment_owner.get_or_null(p_env);
	ERR_FAIL_NULL_V(env, 0.0);
	return env->glow_map_strength;
}

RID RendererEnvironmentStorage::environment_get_glow_map(RID p_env) const {
	Environment *env = environment_owner.get_or_null(p_env);
	ERR_FAIL_NULL_V(env, RID());
	return env->glow_map;
}

// SSR

void RendererEnvironmentStorage::environment_set_ssr(RID p_env, bool p_enable, int p_max_steps, float p_fade_int, float p_fade_out, float p_depth_tolerance) {
	Environment *env = environment_owner.get_or_null(p_env);
	ERR_FAIL_NULL(env);
#ifdef DEBUG_ENABLED
	if (OS::get_singleton()->get_current_rendering_method() != "forward_plus" && p_enable) {
		WARN_PRINT_ONCE_ED("Screen-space reflections (SSR) can only be enabled when using the Forward+ renderer.");
	}
#endif
	env->ssr_enabled = p_enable;
	env->ssr_max_steps = p_max_steps;
	env->ssr_fade_in = p_fade_int;
	env->ssr_fade_out = p_fade_out;
	env->ssr_depth_tolerance = p_depth_tolerance;
}

bool RendererEnvironmentStorage::environment_get_ssr_enabled(RID p_env) const {
	Environment *env = environment_owner.get_or_null(p_env);
	ERR_FAIL_NULL_V(env, false);
	return env->ssr_enabled;
}

int RendererEnvironmentStorage::environment_get_ssr_max_steps(RID p_env) const {
	Environment *env = environment_owner.get_or_null(p_env);
	ERR_FAIL_NULL_V(env, 64);
	return env->ssr_max_steps;
}

float RendererEnvironmentStorage::environment_get_ssr_fade_in(RID p_env) const {
	Environment *env = environment_owner.get_or_null(p_env);
	ERR_FAIL_NULL_V(env, 0.15);
	return env->ssr_fade_in;
}

float RendererEnvironmentStorage::environment_get_ssr_fade_out(RID p_env) const {
	Environment *env = environment_owner.get_or_null(p_env);
	ERR_FAIL_NULL_V(env, 2.0);
	return env->ssr_fade_out;
}

float RendererEnvironmentStorage::environment_get_ssr_depth_tolerance(RID p_env) const {
	Environment *env = environment_owner.get_or_null(p_env);
	ERR_FAIL_NULL_V(env, 0.2);
	return env->ssr_depth_tolerance;
}

// SSAO

void RendererEnvironmentStorage::environment_set_ssao(RID p_env, bool p_enable, float p_radius, float p_intensity, float p_power, float p_detail, float p_horizon, float p_sharpness, float p_light_affect, float p_ao_channel_affect) {
	Environment *env = environment_owner.get_or_null(p_env);
	ERR_FAIL_NULL(env);
#ifdef DEBUG_ENABLED
	if (OS::get_singleton()->get_current_rendering_method() != "forward_plus" && p_enable) {
		WARN_PRINT_ONCE_ED("Screen-space ambient occlusion (SSAO) can only be enabled when using the Forward+ renderer.");
	}
#endif
	env->ssao_enabled = p_enable;
	env->ssao_radius = p_radius;
	env->ssao_intensity = p_intensity;
	env->ssao_power = p_power;
	env->ssao_detail = p_detail;
	env->ssao_horizon = p_horizon;
	env->ssao_sharpness = p_sharpness;
	env->ssao_direct_light_affect = p_light_affect;
	env->ssao_ao_channel_affect = p_ao_channel_affect;
}

bool RendererEnvironmentStorage::environment_get_ssao_enabled(RID p_env) const {
	Environment *env = environment_owner.get_or_null(p_env);
	ERR_FAIL_NULL_V(env, false);
	return env->ssao_enabled;
}

float RendererEnvironmentStorage::environment_get_ssao_radius(RID p_env) const {
	Environment *env = environment_owner.get_or_null(p_env);
	ERR_FAIL_NULL_V(env, 1.0);
	return env->ssao_radius;
}

float RendererEnvironmentStorage::environment_get_ssao_intensity(RID p_env) const {
	Environment *env = environment_owner.get_or_null(p_env);
	ERR_FAIL_NULL_V(env, 2.0);
	return env->ssao_intensity;
}

float RendererEnvironmentStorage::environment_get_ssao_power(RID p_env) const {
	Environment *env = environment_owner.get_or_null(p_env);
	ERR_FAIL_NULL_V(env, 1.5);
	return env->ssao_power;
}

float RendererEnvironmentStorage::environment_get_ssao_detail(RID p_env) const {
	Environment *env = environment_owner.get_or_null(p_env);
	ERR_FAIL_NULL_V(env, 0.5);
	return env->ssao_detail;
}

float RendererEnvironmentStorage::environment_get_ssao_horizon(RID p_env) const {
	Environment *env = environment_owner.get_or_null(p_env);
	ERR_FAIL_NULL_V(env, 0.06);
	return env->ssao_horizon;
}

float RendererEnvironmentStorage::environment_get_ssao_sharpness(RID p_env) const {
	Environment *env = environment_owner.get_or_null(p_env);
	ERR_FAIL_NULL_V(env, 0.98);
	return env->ssao_sharpness;
}

float RendererEnvironmentStorage::environment_get_ssao_direct_light_affect(RID p_env) const {
	Environment *env = environment_owner.get_or_null(p_env);
	ERR_FAIL_NULL_V(env, 0.0);
	return env->ssao_direct_light_affect;
}

float RendererEnvironmentStorage::environment_get_ssao_ao_channel_affect(RID p_env) const {
	Environment *env = environment_owner.get_or_null(p_env);
	ERR_FAIL_NULL_V(env, 0.0);
	return env->ssao_ao_channel_affect;
}

// SSIL

void RendererEnvironmentStorage::environment_set_ssil(RID p_env, bool p_enable, float p_radius, float p_intensity, float p_sharpness, float p_normal_rejection) {
	Environment *env = environment_owner.get_or_null(p_env);
	ERR_FAIL_NULL(env);
#ifdef DEBUG_ENABLED
	if (OS::get_singleton()->get_current_rendering_method() != "forward_plus" && p_enable) {
		WARN_PRINT_ONCE_ED("Screen-space indirect lighting (SSIL) can only be enabled when using the Forward+ renderer.");
	}
#endif
	env->ssil_enabled = p_enable;
	env->ssil_radius = p_radius;
	env->ssil_intensity = p_intensity;
	env->ssil_sharpness = p_sharpness;
	env->ssil_normal_rejection = p_normal_rejection;
}

bool RendererEnvironmentStorage::environment_get_ssil_enabled(RID p_env) const {
	Environment *env = environment_owner.get_or_null(p_env);
	ERR_FAIL_NULL_V(env, false);
	return env->ssil_enabled;
}

float RendererEnvironmentStorage::environment_get_ssil_radius(RID p_env) const {
	Environment *env = environment_owner.get_or_null(p_env);
	ERR_FAIL_NULL_V(env, 5.0);
	return env->ssil_radius;
}

float RendererEnvironmentStorage::environment_get_ssil_intensity(RID p_env) const {
	Environment *env = environment_owner.get_or_null(p_env);
	ERR_FAIL_NULL_V(env, 1.0);
	return env->ssil_intensity;
}

float RendererEnvironmentStorage::environment_get_ssil_sharpness(RID p_env) const {
	Environment *env = environment_owner.get_or_null(p_env);
	ERR_FAIL_NULL_V(env, 0.98);
	return env->ssil_sharpness;
}

float RendererEnvironmentStorage::environment_get_ssil_normal_rejection(RID p_env) const {
	Environment *env = environment_owner.get_or_null(p_env);
	ERR_FAIL_NULL_V(env, 1.0);
	return env->ssil_normal_rejection;
}

// SDFGI

void RendererEnvironmentStorage::environment_set_sdfgi(RID p_env, bool p_enable, int p_cascades, float p_min_cell_size, RS::EnvironmentSDFGIYScale p_y_scale, bool p_use_occlusion, float p_bounce_feedback, bool p_read_sky, float p_energy, float p_normal_bias, float p_probe_bias) {
	Environment *env = environment_owner.get_or_null(p_env);
	ERR_FAIL_NULL(env);
#ifdef DEBUG_ENABLED
	if (OS::get_singleton()->get_current_rendering_method() != "forward_plus" && p_enable) {
		WARN_PRINT_ONCE_ED("SDFGI can only be enabled when using the Forward+ renderer.");
	}
#endif
	env->sdfgi_enabled = p_enable;
	env->sdfgi_cascades = p_cascades;
	env->sdfgi_min_cell_size = p_min_cell_size;
	env->sdfgi_use_occlusion = p_use_occlusion;
	env->sdfgi_bounce_feedback = p_bounce_feedback;
	env->sdfgi_read_sky_light = p_read_sky;
	env->sdfgi_energy = p_energy;
	env->sdfgi_normal_bias = p_normal_bias;
	env->sdfgi_probe_bias = p_probe_bias;
	env->sdfgi_y_scale = p_y_scale;
}

bool RendererEnvironmentStorage::environment_get_sdfgi_enabled(RID p_env) const {
	Environment *env = environment_owner.get_or_null(p_env);
	ERR_FAIL_NULL_V(env, false);
	return env->sdfgi_enabled;
}

int RendererEnvironmentStorage::environment_get_sdfgi_cascades(RID p_env) const {
	Environment *env = environment_owner.get_or_null(p_env);
	ERR_FAIL_NULL_V(env, 4);
	return env->sdfgi_cascades;
}

float RendererEnvironmentStorage::environment_get_sdfgi_min_cell_size(RID p_env) const {
	Environment *env = environment_owner.get_or_null(p_env);
	ERR_FAIL_NULL_V(env, 0.2);
	return env->sdfgi_min_cell_size;
}

bool RendererEnvironmentStorage::environment_get_sdfgi_use_occlusion(RID p_env) const {
	Environment *env = environment_owner.get_or_null(p_env);
	ERR_FAIL_NULL_V(env, false);
	return env->sdfgi_use_occlusion;
}

float RendererEnvironmentStorage::environment_get_sdfgi_bounce_feedback(RID p_env) const {
	Environment *env = environment_owner.get_or_null(p_env);
	ERR_FAIL_NULL_V(env, 0.5);
	return env->sdfgi_bounce_feedback;
}

bool RendererEnvironmentStorage::environment_get_sdfgi_read_sky_light(RID p_env) const {
	Environment *env = environment_owner.get_or_null(p_env);
	ERR_FAIL_NULL_V(env, true);
	return env->sdfgi_read_sky_light;
}

float RendererEnvironmentStorage::environment_get_sdfgi_energy(RID p_env) const {
	Environment *env = environment_owner.get_or_null(p_env);
	ERR_FAIL_NULL_V(env, 1.0);
	return env->sdfgi_energy;
}

float RendererEnvironmentStorage::environment_get_sdfgi_normal_bias(RID p_env) const {
	Environment *env = environment_owner.get_or_null(p_env);
	ERR_FAIL_NULL_V(env, 1.1);
	return env->sdfgi_normal_bias;
}

float RendererEnvironmentStorage::environment_get_sdfgi_probe_bias(RID p_env) const {
	Environment *env = environment_owner.get_or_null(p_env);
	ERR_FAIL_NULL_V(env, 1.1);
	return env->sdfgi_probe_bias;
}

RS::EnvironmentSDFGIYScale RendererEnvironmentStorage::environment_get_sdfgi_y_scale(RID p_env) const {
	Environment *env = environment_owner.get_or_null(p_env);
	ERR_FAIL_NULL_V(env, RS::ENV_SDFGI_Y_SCALE_75_PERCENT);
	return env->sdfgi_y_scale;
}

// Adjustments

void RendererEnvironmentStorage::environment_set_adjustment(RID p_env, bool p_enable, float p_brightness, float p_contrast, float p_saturation, bool p_use_1d_color_correction, RID p_color_correction) {
	Environment *env = environment_owner.get_or_null(p_env);
	ERR_FAIL_NULL(env);

	env->adjustments_enabled = p_enable;
	env->adjustments_brightness = p_brightness;
	env->adjustments_contrast = p_contrast;
	env->adjustments_saturation = p_saturation;
	env->use_1d_color_correction = p_use_1d_color_correction;
	env->color_correction = p_color_correction;
}

bool RendererEnvironmentStorage::environment_get_adjustments_enabled(RID p_env) const {
	Environment *env = environment_owner.get_or_null(p_env);
	ERR_FAIL_NULL_V(env, false);
	return env->adjustments_enabled;
}

float RendererEnvironmentStorage::environment_get_adjustments_brightness(RID p_env) const {
	Environment *env = environment_owner.get_or_null(p_env);
	ERR_FAIL_NULL_V(env, 1.0);
	return env->adjustments_brightness;
}

float RendererEnvironmentStorage::environment_get_adjustments_contrast(RID p_env) const {
	Environment *env = environment_owner.get_or_null(p_env);
	ERR_FAIL_NULL_V(env, 1.0);
	return env->adjustments_contrast;
}

float RendererEnvironmentStorage::environment_get_adjustments_saturation(RID p_env) const {
	Environment *env = environment_owner.get_or_null(p_env);
	ERR_FAIL_NULL_V(env, 1.0);
	return env->adjustments_saturation;
}

bool RendererEnvironmentStorage::environment_get_use_1d_color_correction(RID p_env) const {
	Environment *env = environment_owner.get_or_null(p_env);
	ERR_FAIL_NULL_V(env, false);
	return env->use_1d_color_correction;
}

RID RendererEnvironmentStorage::environment_get_color_correction(RID p_env) const {
	Environment *env = environment_owner.get_or_null(p_env);
	ERR_FAIL_NULL_V(env, RID());
	return env->color_correction;
}

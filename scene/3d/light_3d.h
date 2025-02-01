/**************************************************************************/
/*  light_3d.h                                                            */
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

#include "scene/3d/visual_instance_3d.h"

class Light3D : public VisualInstance3D {
	GDCLASS(Light3D, VisualInstance3D);

public:
	enum Param {
		PARAM_ENERGY = RS::LIGHT_PARAM_ENERGY,
		PARAM_INDIRECT_ENERGY = RS::LIGHT_PARAM_INDIRECT_ENERGY,
		PARAM_VOLUMETRIC_FOG_ENERGY = RS::LIGHT_PARAM_VOLUMETRIC_FOG_ENERGY,
		PARAM_SPECULAR = RS::LIGHT_PARAM_SPECULAR,
		PARAM_RANGE = RS::LIGHT_PARAM_RANGE,
		PARAM_SIZE = RS::LIGHT_PARAM_SIZE,
		PARAM_ATTENUATION = RS::LIGHT_PARAM_ATTENUATION,
		PARAM_SPOT_ANGLE = RS::LIGHT_PARAM_SPOT_ANGLE,
		PARAM_SPOT_ATTENUATION = RS::LIGHT_PARAM_SPOT_ATTENUATION,
		PARAM_SHADOW_MAX_DISTANCE = RS::LIGHT_PARAM_SHADOW_MAX_DISTANCE,
		PARAM_SHADOW_SPLIT_1_OFFSET = RS::LIGHT_PARAM_SHADOW_SPLIT_1_OFFSET,
		PARAM_SHADOW_SPLIT_2_OFFSET = RS::LIGHT_PARAM_SHADOW_SPLIT_2_OFFSET,
		PARAM_SHADOW_SPLIT_3_OFFSET = RS::LIGHT_PARAM_SHADOW_SPLIT_3_OFFSET,
		PARAM_SHADOW_FADE_START = RS::LIGHT_PARAM_SHADOW_FADE_START,
		PARAM_SHADOW_NORMAL_BIAS = RS::LIGHT_PARAM_SHADOW_NORMAL_BIAS,
		PARAM_SHADOW_BIAS = RS::LIGHT_PARAM_SHADOW_BIAS,
		PARAM_SHADOW_PANCAKE_SIZE = RS::LIGHT_PARAM_SHADOW_PANCAKE_SIZE,
		PARAM_SHADOW_OPACITY = RS::LIGHT_PARAM_SHADOW_OPACITY,
		PARAM_SHADOW_BLUR = RS::LIGHT_PARAM_SHADOW_BLUR,
		PARAM_TRANSMITTANCE_BIAS = RS::LIGHT_PARAM_TRANSMITTANCE_BIAS,
		PARAM_INTENSITY = RS::LIGHT_PARAM_INTENSITY,
		PARAM_MAX = RS::LIGHT_PARAM_MAX
	};

	enum BakeMode {
		BAKE_DISABLED,
		BAKE_STATIC,
		BAKE_DYNAMIC,
	};

private:
	Color color;
	real_t param[PARAM_MAX] = {};
	bool shadow = false;
	bool negative = false;
	bool reverse_cull = false;
	uint32_t cull_mask = 0;
	uint32_t shadow_caster_mask = 0xFFFFFFFF;
	bool distance_fade_enabled = false;
	real_t distance_fade_begin = 40.0;
	real_t distance_fade_shadow = 50.0;
	real_t distance_fade_length = 10.0;
	RS::LightType type = RenderingServer::LIGHT_DIRECTIONAL;
	bool editor_only = false;
	void _update_visibility();
	BakeMode bake_mode = BAKE_DYNAMIC;
	Ref<Texture2D> projector;
	Color correlated_color = Color(1.0, 1.0, 1.0);
	float temperature = 6500.0;

	// bind helpers

	virtual void owner_changed_notify() override;

protected:
	RID light;

	static void _bind_methods();
	void _notification(int p_what);
	void _validate_property(PropertyInfo &p_property) const;

	Light3D(RenderingServer::LightType p_type);

public:
	RS::LightType get_light_type() const { return type; }

	void set_editor_only(bool p_editor_only);
	bool is_editor_only() const;

	void set_param(Param p_param, real_t p_value);
	real_t get_param(Param p_param) const;

	void set_shadow(bool p_enable);
	bool has_shadow() const;

	void set_negative(bool p_enable);
	bool is_negative() const;

	void set_enable_distance_fade(bool p_enable);
	bool is_distance_fade_enabled() const;

	void set_distance_fade_begin(real_t p_distance);
	real_t get_distance_fade_begin() const;

	void set_distance_fade_shadow(real_t p_distance);
	real_t get_distance_fade_shadow() const;

	void set_distance_fade_length(real_t p_length);
	real_t get_distance_fade_length() const;

	void set_cull_mask(uint32_t p_cull_mask);
	uint32_t get_cull_mask() const;

	void set_color(const Color &p_color);
	Color get_color() const;

	void set_shadow_reverse_cull_face(bool p_enable);
	bool get_shadow_reverse_cull_face() const;

	void set_shadow_caster_mask(uint32_t p_caster_mask);
	uint32_t get_shadow_caster_mask() const;

	void set_bake_mode(BakeMode p_mode);
	BakeMode get_bake_mode() const;

	void set_projector(const Ref<Texture2D> &p_texture);
	Ref<Texture2D> get_projector() const;

	void set_temperature(const float p_temperature);
	float get_temperature() const;
	Color get_correlated_color() const;

	virtual AABB get_aabb() const override;
	virtual PackedStringArray get_configuration_warnings() const override;

	Light3D();
	~Light3D();
};

VARIANT_ENUM_CAST(Light3D::Param);
VARIANT_ENUM_CAST(Light3D::BakeMode);

class DirectionalLight3D : public Light3D {
	GDCLASS(DirectionalLight3D, Light3D);

public:
	enum ShadowMode {
		SHADOW_ORTHOGONAL,
		SHADOW_PARALLEL_2_SPLITS,
		SHADOW_PARALLEL_4_SPLITS,
	};

	enum SkyMode {
		SKY_MODE_LIGHT_AND_SKY,
		SKY_MODE_LIGHT_ONLY,
		SKY_MODE_SKY_ONLY,
	};

private:
	bool blend_splits;
	ShadowMode shadow_mode;
	SkyMode sky_mode = SKY_MODE_LIGHT_AND_SKY;

protected:
	static void _bind_methods();
	void _validate_property(PropertyInfo &p_property) const;

public:
	void set_shadow_mode(ShadowMode p_mode);
	ShadowMode get_shadow_mode() const;

	void set_blend_splits(bool p_enable);
	bool is_blend_splits_enabled() const;

	void set_sky_mode(SkyMode p_mode);
	SkyMode get_sky_mode() const;

	DirectionalLight3D();
};

VARIANT_ENUM_CAST(DirectionalLight3D::ShadowMode)
VARIANT_ENUM_CAST(DirectionalLight3D::SkyMode)

class OmniLight3D : public Light3D {
	GDCLASS(OmniLight3D, Light3D);

public:
	// omni light
	enum ShadowMode {
		SHADOW_DUAL_PARABOLOID,
		SHADOW_CUBE,
	};

private:
	ShadowMode shadow_mode;

protected:
	static void _bind_methods();

public:
	void set_shadow_mode(ShadowMode p_mode);
	ShadowMode get_shadow_mode() const;

	PackedStringArray get_configuration_warnings() const override;

	OmniLight3D();
};

VARIANT_ENUM_CAST(OmniLight3D::ShadowMode)

class SpotLight3D : public Light3D {
	GDCLASS(SpotLight3D, Light3D);

protected:
	static void _bind_methods();

public:
	PackedStringArray get_configuration_warnings() const override;

	SpotLight3D();
};

/**************************************************************************/
/*  visual_instance_3d.h                                                  */
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

#include "scene/3d/node_3d.h"

class VisualInstance3D : public Node3D {
	GDCLASS(VisualInstance3D, Node3D);

	RID base;
	RID instance;
	uint32_t layers = 1;
	float sorting_offset = 0.0;
	bool sorting_use_aabb_center = true;

protected:
	void _update_visibility();

	virtual void _physics_interpolated_changed() override;
	void set_instance_use_identity_transform(bool p_enable);

	void _notification(int p_what);
	static void _bind_methods();
	void _validate_property(PropertyInfo &p_property) const;

	GDVIRTUAL0RC(AABB, _get_aabb)
public:
	enum GetFacesFlags {
		FACES_SOLID = 1, // solid geometry
		FACES_ENCLOSING = 2,
		FACES_DYNAMIC = 4 // dynamic object geometry

	};

	RID get_instance() const;
	virtual AABB get_aabb() const;

	void set_base(const RID &p_base);
	RID get_base() const;

	void set_layer_mask(uint32_t p_mask);
	uint32_t get_layer_mask() const;

	void set_layer_mask_value(int p_layer_number, bool p_enable);
	bool get_layer_mask_value(int p_layer_number) const;

	void set_sorting_offset(float p_offset);
	float get_sorting_offset() const;

	void set_sorting_use_aabb_center(bool p_enabled);
	bool is_sorting_use_aabb_center() const;

	VisualInstance3D();
	~VisualInstance3D();
};

class GeometryInstance3D : public VisualInstance3D {
	GDCLASS(GeometryInstance3D, VisualInstance3D);

public:
	enum ShadowCastingSetting {
		SHADOW_CASTING_SETTING_OFF = RS::SHADOW_CASTING_SETTING_OFF,
		SHADOW_CASTING_SETTING_ON = RS::SHADOW_CASTING_SETTING_ON,
		SHADOW_CASTING_SETTING_DOUBLE_SIDED = RS::SHADOW_CASTING_SETTING_DOUBLE_SIDED,
		SHADOW_CASTING_SETTING_SHADOWS_ONLY = RS::SHADOW_CASTING_SETTING_SHADOWS_ONLY
	};

	enum GIMode {
		GI_MODE_DISABLED,
		GI_MODE_STATIC,
		GI_MODE_DYNAMIC
	};

	enum LightmapScale {
		LIGHTMAP_SCALE_1X,
		LIGHTMAP_SCALE_2X,
		LIGHTMAP_SCALE_4X,
		LIGHTMAP_SCALE_8X,
		LIGHTMAP_SCALE_MAX,
	};

	enum VisibilityRangeFadeMode {
		VISIBILITY_RANGE_FADE_DISABLED = RS::VISIBILITY_RANGE_FADE_DISABLED,
		VISIBILITY_RANGE_FADE_SELF = RS::VISIBILITY_RANGE_FADE_SELF,
		VISIBILITY_RANGE_FADE_DEPENDENCIES = RS::VISIBILITY_RANGE_FADE_DEPENDENCIES,
	};

private:
	ShadowCastingSetting shadow_casting_setting = SHADOW_CASTING_SETTING_ON;
	Ref<Material> material_override;
	Ref<Material> material_overlay;

	float visibility_range_begin = 0.0;
	float visibility_range_end = 0.0;
	float visibility_range_begin_margin = 0.0;
	float visibility_range_end_margin = 0.0;
	VisibilityRangeFadeMode visibility_range_fade_mode = VISIBILITY_RANGE_FADE_DISABLED;

	float transparency = 0.0f;

	float lod_bias = 1.0;

	mutable HashMap<StringName, Variant> instance_shader_parameters;
	mutable HashMap<StringName, StringName> instance_shader_parameter_property_remap;

	float extra_cull_margin = 0.0;
	AABB custom_aabb;
	float lightmap_texel_scale = 1.0f;
	GIMode gi_mode = GI_MODE_STATIC;
	bool ignore_occlusion_culling = false;

	const StringName *_instance_uniform_get_remap(const StringName &p_name) const;

protected:
	bool _set(const StringName &p_name, const Variant &p_value);
	bool _get(const StringName &p_name, Variant &r_ret) const;
	void _get_property_list(List<PropertyInfo> *p_list) const;
	void _validate_property(PropertyInfo &p_property) const;

	static void _bind_methods();

public:
	void set_cast_shadows_setting(ShadowCastingSetting p_shadow_casting_setting);
	ShadowCastingSetting get_cast_shadows_setting() const;

	void set_transparency(float p_transparency);
	float get_transparency() const;

	void set_visibility_range_begin(float p_dist);
	float get_visibility_range_begin() const;

	void set_visibility_range_end(float p_dist);
	float get_visibility_range_end() const;

	void set_visibility_range_begin_margin(float p_dist);
	float get_visibility_range_begin_margin() const;

	void set_visibility_range_end_margin(float p_dist);
	float get_visibility_range_end_margin() const;

	void set_visibility_range_fade_mode(VisibilityRangeFadeMode p_mode);
	VisibilityRangeFadeMode get_visibility_range_fade_mode() const;

	void set_material_override(const Ref<Material> &p_material);
	Ref<Material> get_material_override() const;

	void set_material_overlay(const Ref<Material> &p_material);
	Ref<Material> get_material_overlay() const;

	void set_extra_cull_margin(float p_margin);
	float get_extra_cull_margin() const;

	void set_lod_bias(float p_bias);
	float get_lod_bias() const;

	void set_gi_mode(GIMode p_mode);
	GIMode get_gi_mode() const;

	void set_lightmap_texel_scale(float p_scale);
	float get_lightmap_texel_scale() const;

#ifndef DISABLE_DEPRECATED
	void set_lightmap_scale(GeometryInstance3D::LightmapScale p_scale);
	LightmapScale get_lightmap_scale() const;
#endif // DISABLE_DEPRECATED

	void set_instance_shader_parameter(const StringName &p_name, const Variant &p_value);
	Variant get_instance_shader_parameter(const StringName &p_name) const;

	void set_custom_aabb(AABB p_aabb);
	AABB get_custom_aabb() const;

	void set_ignore_occlusion_culling(bool p_enabled);
	bool is_ignoring_occlusion_culling();

	virtual Ref<TriangleMesh> generate_triangle_mesh() const;

	PackedStringArray get_configuration_warnings() const override;
	GeometryInstance3D();
	virtual ~GeometryInstance3D();
};

VARIANT_ENUM_CAST(GeometryInstance3D::ShadowCastingSetting);
VARIANT_ENUM_CAST(GeometryInstance3D::GIMode);
VARIANT_ENUM_CAST(GeometryInstance3D::LightmapScale);
VARIANT_ENUM_CAST(GeometryInstance3D::VisibilityRangeFadeMode);

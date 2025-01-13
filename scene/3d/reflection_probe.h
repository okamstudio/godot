/**************************************************************************/
/*  reflection_probe.h                                                    */
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

#ifndef REFLECTION_PROBE_H
#define REFLECTION_PROBE_H

#include "scene/3d/visual_instance_3d.h"

class ReflectionProbe : public VisualInstance3D {
	GDCLASS(ReflectionProbe, VisualInstance3D);

public:
	enum UpdateMode {
		UPDATE_ONCE,
		UPDATE_ALWAYS,
	};

	enum AmbientMode {
		AMBIENT_DISABLED,
		AMBIENT_ENVIRONMENT,
		AMBIENT_COLOR
	};

private:
	RID probe;
	float intensity = 1.0;
	float blend_distance = 1.0;
	float max_distance = 0.0;
	Vector3 size = Vector3(20, 20, 20);
	Vector3 origin_offset = Vector3(0, 0, 0);
	bool box_projection = false;
	bool enable_shadows = false;
	bool interior = false;
	AmbientMode ambient_mode = AMBIENT_ENVIRONMENT;
	Color ambient_color = Color(0, 0, 0);
	float ambient_color_energy = 1.0;
	float mesh_lod_threshold = 1.0;

	uint32_t cull_mask = (1 << 20) - 1;
	uint32_t reflection_mask = (1 << 20) - 1;
	UpdateMode update_mode = UPDATE_ONCE;

protected:
	static void _bind_methods();
	void _validate_property(PropertyInfo &p_property) const;
#ifndef DISABLE_DEPRECATED
	bool _set(const StringName &p_name, const Variant &p_value);
	bool _get(const StringName &p_name, Variant &r_property) const;
#endif // DISABLE_DEPRECATED

public:
	void set_intensity(float p_intensity);
	float get_intensity() const;

	void set_blend_distance(float p_blend_distance);
	float get_blend_distance() const;

	void set_ambient_mode(AmbientMode p_mode);
	AmbientMode get_ambient_mode() const;

	void set_ambient_color(Color p_ambient);
	Color get_ambient_color() const;

	void set_ambient_color_energy(float p_energy);
	float get_ambient_color_energy() const;

	void set_interior_ambient_probe_contribution(float p_contribution);
	float get_interior_ambient_probe_contribution() const;

	void set_max_distance(float p_distance);
	float get_max_distance() const;

	void set_mesh_lod_threshold(float p_pixels);
	float get_mesh_lod_threshold() const;

	void set_size(const Vector3 &p_size);
	Vector3 get_size() const;

	void set_origin_offset(const Vector3 &p_offset);
	Vector3 get_origin_offset() const;

	void set_as_interior(bool p_enable);
	bool is_set_as_interior() const;

	void set_enable_box_projection(bool p_enable);
	bool is_box_projection_enabled() const;

	void set_enable_shadows(bool p_enable);
	bool are_shadows_enabled() const;

	void set_cull_mask(uint32_t p_layers);
	uint32_t get_cull_mask() const;

	void set_reflection_mask(uint32_t p_layers);
	uint32_t get_reflection_mask() const;

	void set_update_mode(UpdateMode p_mode);
	UpdateMode get_update_mode() const;

	virtual AABB get_aabb() const override;

	ReflectionProbe();
	~ReflectionProbe();
};

VARIANT_ENUM_CAST(ReflectionProbe::AmbientMode);
VARIANT_ENUM_CAST(ReflectionProbe::UpdateMode);

#endif // REFLECTION_PROBE_H

/**************************************************************************/
/*  decal.h                                                               */
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

class Decal : public VisualInstance3D {
	GDCLASS(Decal, VisualInstance3D);

public:
	enum DecalTexture {
		TEXTURE_ALBEDO,
		TEXTURE_NORMAL,
		TEXTURE_ORM,
		TEXTURE_EMISSION,
		TEXTURE_MAX
	};

private:
	RID decal;
	Vector3 size = Vector3(2, 2, 2);
	Ref<Texture2D> textures[TEXTURE_MAX];
	real_t emission_energy = 1.0;
	real_t albedo_mix = 1.0;
	Color modulate = Color(1, 1, 1, 1);
	uint32_t cull_mask = (1 << 20) - 1;
	real_t normal_fade = 0.0;
	real_t upper_fade = 0.3;
	real_t lower_fade = 0.3;
	bool distance_fade_enabled = false;
	real_t distance_fade_begin = 40.0;
	real_t distance_fade_length = 10.0;

protected:
	static void _bind_methods();
	void _validate_property(PropertyInfo &p_property) const;
#ifndef DISABLE_DEPRECATED
	bool _set(const StringName &p_name, const Variant &p_value);
	bool _get(const StringName &p_name, Variant &r_property) const;
#endif // DISABLE_DEPRECATED

public:
	virtual PackedStringArray get_configuration_warnings() const override;

	void set_size(const Vector3 &p_size);
	Vector3 get_size() const;

	void set_texture(DecalTexture p_type, const Ref<Texture2D> &p_texture);
	Ref<Texture2D> get_texture(DecalTexture p_type) const;

	void set_emission_energy(real_t p_energy);
	real_t get_emission_energy() const;

	void set_albedo_mix(real_t p_mix);
	real_t get_albedo_mix() const;

	void set_modulate(Color p_modulate);
	Color get_modulate() const;

	void set_upper_fade(real_t p_energy);
	real_t get_upper_fade() const;

	void set_lower_fade(real_t p_fade);
	real_t get_lower_fade() const;

	void set_normal_fade(real_t p_fade);
	real_t get_normal_fade() const;

	void set_enable_distance_fade(bool p_enable);
	bool is_distance_fade_enabled() const;

	void set_distance_fade_begin(real_t p_distance);
	real_t get_distance_fade_begin() const;

	void set_distance_fade_length(real_t p_length);
	real_t get_distance_fade_length() const;

	void set_cull_mask(uint32_t p_layers);
	uint32_t get_cull_mask() const;

	virtual AABB get_aabb() const override;

	Decal();
	~Decal();
};

VARIANT_ENUM_CAST(Decal::DecalTexture);

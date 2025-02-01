/**************************************************************************/
/*  light_2d.h                                                            */
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

#include "scene/2d/node_2d.h"

class Light2D : public Node2D {
	GDCLASS(Light2D, Node2D);

public:
	enum ShadowFilter {
		SHADOW_FILTER_NONE,
		SHADOW_FILTER_PCF5,
		SHADOW_FILTER_PCF13,
		SHADOW_FILTER_MAX
	};

	enum BlendMode {
		BLEND_MODE_ADD,
		BLEND_MODE_SUB,
		BLEND_MODE_MIX,
	};

private:
	RID canvas_light;
	bool enabled = true;
	bool editor_only = false;
	bool shadow = false;
	Color color = Color(1, 1, 1);
	Color shadow_color = Color(0, 0, 0, 0);
	real_t height = 0.0;
	real_t energy = 1.0;
	int z_min = -1024;
	int z_max = 1024;
	int layer_min = 0;
	int layer_max = 0;
	int item_mask = 1;
	int item_shadow_mask = 1;
	real_t shadow_smooth = 0.0;
	Ref<Texture2D> texture;
	Vector2 texture_offset;
	ShadowFilter shadow_filter = SHADOW_FILTER_NONE;
	BlendMode blend_mode = BLEND_MODE_ADD;

	void _update_light_visibility();

	virtual void owner_changed_notify() override;
	virtual void _physics_interpolated_changed() override;

protected:
	_FORCE_INLINE_ RID _get_light() const { return canvas_light; }
	void _notification(int p_what);
	static void _bind_methods();
	void _validate_property(PropertyInfo &p_property) const;

public:
	void set_enabled(bool p_enabled);
	bool is_enabled() const;

	void set_editor_only(bool p_editor_only);
	bool is_editor_only() const;

	void set_color(const Color &p_color);
	Color get_color() const;

	void set_height(real_t p_height);
	real_t get_height() const;

	void set_energy(real_t p_energy);
	real_t get_energy() const;

	void set_z_range_min(int p_min_z);
	int get_z_range_min() const;

	void set_z_range_max(int p_max_z);
	int get_z_range_max() const;

	void set_layer_range_min(int p_min_layer);
	int get_layer_range_min() const;

	void set_layer_range_max(int p_max_layer);
	int get_layer_range_max() const;

	void set_item_cull_mask(int p_mask);
	int get_item_cull_mask() const;

	void set_item_shadow_cull_mask(int p_mask);
	int get_item_shadow_cull_mask() const;

	void set_shadow_enabled(bool p_enabled);
	bool is_shadow_enabled() const;

	void set_shadow_filter(ShadowFilter p_filter);
	ShadowFilter get_shadow_filter() const;

	void set_shadow_color(const Color &p_shadow_color);
	Color get_shadow_color() const;

	void set_shadow_smooth(real_t p_amount);
	real_t get_shadow_smooth() const;

	void set_blend_mode(BlendMode p_mode);
	BlendMode get_blend_mode() const;

	Light2D();
	~Light2D();
};

VARIANT_ENUM_CAST(Light2D::ShadowFilter);
VARIANT_ENUM_CAST(Light2D::BlendMode);

class PointLight2D : public Light2D {
	GDCLASS(PointLight2D, Light2D);

private:
	real_t _scale = 1.0;
	Ref<Texture2D> texture;
	Vector2 texture_offset;

protected:
#ifndef DISABLE_DEPRECATED
	bool _set(const StringName &p_name, const Variant &p_value);
#endif // DISABLE_DEPRECATED
	static void _bind_methods();

public:
#ifdef TOOLS_ENABLED
	virtual Dictionary _edit_get_state() const override;
	virtual void _edit_set_state(const Dictionary &p_state) override;

	virtual void _edit_set_pivot(const Point2 &p_pivot) override;
	virtual Point2 _edit_get_pivot() const override;
	virtual bool _edit_use_pivot() const override;
#endif // TOOLS_ENABLED

#ifdef DEBUG_ENABLED
	virtual Rect2 _edit_get_rect() const override;
	virtual bool _edit_use_rect() const override;
#endif // DEBUG_ENABLED

	virtual Rect2 get_anchorable_rect() const override;

	void set_texture(const Ref<Texture2D> &p_texture);
	Ref<Texture2D> get_texture() const;

	void set_texture_offset(const Vector2 &p_offset);
	Vector2 get_texture_offset() const;

	void set_texture_scale(real_t p_scale);
	real_t get_texture_scale() const;

	PackedStringArray get_configuration_warnings() const override;

	PointLight2D();
};

class DirectionalLight2D : public Light2D {
	GDCLASS(DirectionalLight2D, Light2D);

	real_t max_distance = 10000.0;

protected:
	static void _bind_methods();

public:
	void set_max_distance(real_t p_distance);
	real_t get_max_distance() const;

	DirectionalLight2D();
};

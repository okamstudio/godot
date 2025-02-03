/**************************************************************************/
/*  particles.h                                                           */
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

#ifndef PARTICLES_H
#define PARTICLES_H

#include "core/rid.h"
#include "scene/3d/visual_instance.h"
#include "scene/resources/material.h"

class Particles : public GeometryInstance {
private:
	GDCLASS(Particles, GeometryInstance);

public:
	enum DrawOrder {
		DRAW_ORDER_INDEX,
		DRAW_ORDER_LIFETIME,
		DRAW_ORDER_VIEW_DEPTH,
	};

	enum {
		MAX_DRAW_PASSES = 4
	};

private:
	RID particles;

	bool emitting = false;
	bool active = false;
	bool signal_canceled = false;
	bool one_shot = false;
	int amount;
	float lifetime;
	bool lifetime_infinite;
	float pre_process_time;
	float explosiveness_ratio;
	float randomness_ratio;
	float speed_scale;
	AABB visibility_aabb;
	bool local_coords;
	int fixed_fps;
	bool fractional_delta;

	Ref<Material> process_material;

	DrawOrder draw_order;

	Vector<Ref<Mesh>> draw_passes;

	double time = 0.0;
	double emission_time = 0.0;
	double active_time = 0.0;

protected:
	static void _bind_methods();
	void _notification(int p_what);
	virtual void _validate_property(PropertyInfo &property) const;

public:
	AABB get_aabb() const;
	PoolVector<Face3> get_faces(uint32_t p_usage_flags) const;

	void set_emitting(bool p_emitting);
	void set_amount(int p_amount);
	void set_lifetime(float p_lifetime);
	void set_lifetime_infinite(bool p_lifetime_infinite);
	void set_one_shot(bool p_one_shot);
	void set_pre_process_time(float p_time);
	void set_explosiveness_ratio(float p_ratio);
	void set_randomness_ratio(float p_ratio);
	void set_visibility_aabb(const AABB &p_aabb);
	void set_use_local_coordinates(bool p_enable);
	void set_process_material(const Ref<Material> &p_material);
	void set_speed_scale(float p_scale);

	bool is_emitting() const;
	int get_amount() const;
	float get_lifetime() const;
	bool is_lifetime_infinite() const;
	bool get_one_shot() const;
	float get_pre_process_time() const;
	float get_explosiveness_ratio() const;
	float get_randomness_ratio() const;
	AABB get_visibility_aabb() const;
	bool get_use_local_coordinates() const;
	Ref<Material> get_process_material() const;
	float get_speed_scale() const;

	void set_fixed_fps(int p_count);
	int get_fixed_fps() const;

	void set_fractional_delta(bool p_enable);
	bool get_fractional_delta() const;

	void set_draw_order(DrawOrder p_order);
	DrawOrder get_draw_order() const;

	void set_draw_passes(int p_count);
	int get_draw_passes() const;

	void set_draw_pass_mesh(int p_pass, const Ref<Mesh> &p_mesh);
	Ref<Mesh> get_draw_pass_mesh(int p_pass) const;

	virtual String get_configuration_warning() const;

	void restart();

	AABB capture_aabb() const;
	Particles();
	~Particles();
};

VARIANT_ENUM_CAST(Particles::DrawOrder)

#endif // PARTICLES_H

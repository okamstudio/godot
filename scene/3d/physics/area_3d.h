/**************************************************************************/
/*  area_3d.h                                                             */
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

#include "core/templates/vset.h"
#include "scene/3d/physics/collision_object_3d.h"

class Area3D : public CollisionObject3D {
	GDCLASS(Area3D, CollisionObject3D);

public:
	enum SpaceOverride {
		SPACE_OVERRIDE_DISABLED,
		SPACE_OVERRIDE_COMBINE,
		SPACE_OVERRIDE_COMBINE_REPLACE,
		SPACE_OVERRIDE_REPLACE,
		SPACE_OVERRIDE_REPLACE_COMBINE
	};

private:
	SpaceOverride gravity_space_override = SPACE_OVERRIDE_DISABLED;
	Vector3 gravity_vec;
	real_t gravity = 0.0;
	bool gravity_is_point = false;
	real_t gravity_point_unit_distance = 0.0;

	SpaceOverride linear_damp_space_override = SPACE_OVERRIDE_DISABLED;
	SpaceOverride angular_damp_space_override = SPACE_OVERRIDE_DISABLED;
	real_t angular_damp = 0.1;
	real_t linear_damp = 0.1;

	int priority = 0;

	real_t wind_force_magnitude = 0.0;
	real_t wind_attenuation_factor = 0.0;
	NodePath wind_source_path;

	bool monitoring = false;
	bool monitorable = false;
	bool locked = false;

	void _body_inout(int p_status, const RID &p_body, ObjectID p_instance, int p_body_shape, int p_area_shape);

	void _body_enter_tree(ObjectID p_id);
	void _body_exit_tree(ObjectID p_id);

	struct ShapePair {
		int body_shape = 0;
		int area_shape = 0;
		bool operator<(const ShapePair &p_sp) const {
			if (body_shape == p_sp.body_shape) {
				return area_shape < p_sp.area_shape;
			} else {
				return body_shape < p_sp.body_shape;
			}
		}

		ShapePair() {}
		ShapePair(int p_bs, int p_as) {
			body_shape = p_bs;
			area_shape = p_as;
		}
	};

	struct BodyState {
		RID rid;
		int rc = 0;
		bool in_tree = false;
		VSet<ShapePair> shapes;
	};

	HashMap<ObjectID, BodyState> body_map;

	void _area_inout(int p_status, const RID &p_area, ObjectID p_instance, int p_area_shape, int p_self_shape);

	void _area_enter_tree(ObjectID p_id);
	void _area_exit_tree(ObjectID p_id);

	struct AreaShapePair {
		int area_shape = 0;
		int self_shape = 0;
		bool operator<(const AreaShapePair &p_sp) const {
			if (area_shape == p_sp.area_shape) {
				return self_shape < p_sp.self_shape;
			} else {
				return area_shape < p_sp.area_shape;
			}
		}

		AreaShapePair() {}
		AreaShapePair(int p_bs, int p_as) {
			area_shape = p_bs;
			self_shape = p_as;
		}
	};

	struct AreaState {
		RID rid;
		int rc = 0;
		bool in_tree = false;
		VSet<AreaShapePair> shapes;
	};

	HashMap<ObjectID, AreaState> area_map;
	void _clear_monitoring();

	bool audio_bus_override = false;
	StringName audio_bus;

	bool use_reverb_bus = false;
	StringName reverb_bus;
	float reverb_amount = 0.0;
	float reverb_uniformity = 0.0;

	void _initialize_wind();

protected:
	void _notification(int p_what);
	static void _bind_methods();
	void _validate_property(PropertyInfo &p_property) const;

	virtual void _space_changed(const RID &p_new_space) override;

public:
	void set_gravity_space_override_mode(SpaceOverride p_mode);
	SpaceOverride get_gravity_space_override_mode() const;

	void set_gravity_is_point(bool p_enabled);
	bool is_gravity_a_point() const;

	void set_gravity_point_unit_distance(real_t p_scale);
	real_t get_gravity_point_unit_distance() const;

	void set_gravity_point_center(const Vector3 &p_center);
	const Vector3 &get_gravity_point_center() const;

	void set_gravity_direction(const Vector3 &p_direction);
	const Vector3 &get_gravity_direction() const;

	void set_gravity(real_t p_gravity);
	real_t get_gravity() const;

	void set_linear_damp_space_override_mode(SpaceOverride p_mode);
	SpaceOverride get_linear_damp_space_override_mode() const;

	void set_angular_damp_space_override_mode(SpaceOverride p_mode);
	SpaceOverride get_angular_damp_space_override_mode() const;

	void set_angular_damp(real_t p_angular_damp);
	real_t get_angular_damp() const;

	void set_linear_damp(real_t p_linear_damp);
	real_t get_linear_damp() const;

	void set_priority(int p_priority);
	int get_priority() const;

	void set_wind_force_magnitude(real_t p_wind_force_magnitude);
	real_t get_wind_force_magnitude() const;

	void set_wind_attenuation_factor(real_t p_wind_attenuation_factor);
	real_t get_wind_attenuation_factor() const;

	void set_wind_source_path(const NodePath &p_wind_source_path);
	const NodePath &get_wind_source_path() const;

	void set_monitoring(bool p_enable);
	bool is_monitoring() const;

	void set_monitorable(bool p_enable);
	bool is_monitorable() const;

	TypedArray<Node3D> get_overlapping_bodies() const;
	TypedArray<Area3D> get_overlapping_areas() const; //function for script

	bool has_overlapping_bodies() const;
	bool has_overlapping_areas() const;

	bool overlaps_area(Node *p_area) const;
	bool overlaps_body(Node *p_body) const;

	void set_audio_bus_override(bool p_override);
	bool is_overriding_audio_bus() const;

	void set_audio_bus_name(const StringName &p_audio_bus);
	StringName get_audio_bus_name() const;

	void set_use_reverb_bus(bool p_enable);
	bool is_using_reverb_bus() const;

	void set_reverb_bus_name(const StringName &p_audio_bus);
	StringName get_reverb_bus_name() const;

	void set_reverb_amount(float p_amount);
	float get_reverb_amount() const;

	void set_reverb_uniformity(float p_uniformity);
	float get_reverb_uniformity() const;

	Area3D();
	~Area3D();
};

VARIANT_ENUM_CAST(Area3D::SpaceOverride);

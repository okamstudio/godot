/**************************************************************************/
/*  godot_area_2d.h                                                       */
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

#include "godot_collision_object_2d.h"

#include "core/templates/self_list.h"
#include "servers/physics_server_2d.h"

class GodotSpace2D;
class GodotBody2D;
class GodotConstraint2D;

class GodotArea2D : public GodotCollisionObject2D {
	PhysicsServer2D::AreaSpaceOverrideMode gravity_override_mode = PhysicsServer2D::AREA_SPACE_OVERRIDE_DISABLED;
	PhysicsServer2D::AreaSpaceOverrideMode linear_damping_override_mode = PhysicsServer2D::AREA_SPACE_OVERRIDE_DISABLED;
	PhysicsServer2D::AreaSpaceOverrideMode angular_damping_override_mode = PhysicsServer2D::AREA_SPACE_OVERRIDE_DISABLED;

	real_t gravity = 9.80665;
	Vector2 gravity_vector = Vector2(0, -1);
	bool gravity_is_point = false;
	real_t gravity_point_unit_distance = 0.0;
	real_t linear_damp = 0.1;
	real_t angular_damp = 1.0;
	int priority = 0;
	bool monitorable = false;

	Callable monitor_callback;

	Callable area_monitor_callback;

	SelfList<GodotArea2D> monitor_query_list;
	SelfList<GodotArea2D> moved_list;

	struct BodyKey {
		RID rid;
		ObjectID instance_id;
		uint32_t body_shape = 0;
		uint32_t area_shape = 0;

		static uint32_t hash(const BodyKey &p_key) {
			uint32_t h = hash_one_uint64(p_key.rid.get_id());
			h = hash_murmur3_one_64(p_key.instance_id, h);
			h = hash_murmur3_one_32(p_key.area_shape, h);
			return hash_fmix32(hash_murmur3_one_32(p_key.body_shape, h));
		}

		_FORCE_INLINE_ bool operator==(const BodyKey &p_key) const {
			return rid == p_key.rid && instance_id == p_key.instance_id && body_shape == p_key.body_shape && area_shape == p_key.area_shape;
		}

		_FORCE_INLINE_ BodyKey() {}
		BodyKey(GodotBody2D *p_body, uint32_t p_body_shape, uint32_t p_area_shape);
		BodyKey(GodotArea2D *p_body, uint32_t p_body_shape, uint32_t p_area_shape);
	};

	struct BodyState {
		int state = 0;
		_FORCE_INLINE_ void inc() { state++; }
		_FORCE_INLINE_ void dec() { state--; }
	};

	HashMap<BodyKey, BodyState, BodyKey> monitored_bodies;
	HashMap<BodyKey, BodyState, BodyKey> monitored_areas;

	HashSet<GodotConstraint2D *> constraints;

	virtual void _shapes_changed() override;
	void _queue_monitor_update();

	void _set_space_override_mode(PhysicsServer2D::AreaSpaceOverrideMode &r_mode, PhysicsServer2D::AreaSpaceOverrideMode p_new_mode);

public:
	void set_monitor_callback(const Callable &p_callback);
	_FORCE_INLINE_ bool has_monitor_callback() const { return monitor_callback.is_valid(); }

	void set_area_monitor_callback(const Callable &p_callback);
	_FORCE_INLINE_ bool has_area_monitor_callback() const { return area_monitor_callback.is_valid(); }

	_FORCE_INLINE_ void add_body_to_query(GodotBody2D *p_body, uint32_t p_body_shape, uint32_t p_area_shape);
	_FORCE_INLINE_ void remove_body_from_query(GodotBody2D *p_body, uint32_t p_body_shape, uint32_t p_area_shape);

	_FORCE_INLINE_ void add_area_to_query(GodotArea2D *p_area, uint32_t p_area_shape, uint32_t p_self_shape);
	_FORCE_INLINE_ void remove_area_from_query(GodotArea2D *p_area, uint32_t p_area_shape, uint32_t p_self_shape);

	void set_param(PhysicsServer2D::AreaParameter p_param, const Variant &p_value);
	Variant get_param(PhysicsServer2D::AreaParameter p_param) const;

	_FORCE_INLINE_ void set_gravity(real_t p_gravity) { gravity = p_gravity; }
	_FORCE_INLINE_ real_t get_gravity() const { return gravity; }

	_FORCE_INLINE_ void set_gravity_vector(const Vector2 &p_gravity) { gravity_vector = p_gravity; }
	_FORCE_INLINE_ Vector2 get_gravity_vector() const { return gravity_vector; }

	_FORCE_INLINE_ void set_gravity_as_point(bool p_enable) { gravity_is_point = p_enable; }
	_FORCE_INLINE_ bool is_gravity_point() const { return gravity_is_point; }

	_FORCE_INLINE_ void set_gravity_point_unit_distance(real_t scale) { gravity_point_unit_distance = scale; }
	_FORCE_INLINE_ real_t get_gravity_point_unit_distance() const { return gravity_point_unit_distance; }

	_FORCE_INLINE_ void set_linear_damp(real_t p_linear_damp) { linear_damp = p_linear_damp; }
	_FORCE_INLINE_ real_t get_linear_damp() const { return linear_damp; }

	_FORCE_INLINE_ void set_angular_damp(real_t p_angular_damp) { angular_damp = p_angular_damp; }
	_FORCE_INLINE_ real_t get_angular_damp() const { return angular_damp; }

	_FORCE_INLINE_ void set_priority(int p_priority) { priority = p_priority; }
	_FORCE_INLINE_ int get_priority() const { return priority; }

	_FORCE_INLINE_ void add_constraint(GodotConstraint2D *p_constraint) { constraints.insert(p_constraint); }
	_FORCE_INLINE_ void remove_constraint(GodotConstraint2D *p_constraint) { constraints.erase(p_constraint); }
	_FORCE_INLINE_ const HashSet<GodotConstraint2D *> &get_constraints() const { return constraints; }
	_FORCE_INLINE_ void clear_constraints() { constraints.clear(); }

	void set_monitorable(bool p_monitorable);
	_FORCE_INLINE_ bool is_monitorable() const { return monitorable; }

	void set_transform(const Transform2D &p_transform);

	void set_space(GodotSpace2D *p_space) override;

	void call_queries();

	void compute_gravity(const Vector2 &p_position, Vector2 &r_gravity) const;

	GodotArea2D();
	~GodotArea2D();
};

void GodotArea2D::add_body_to_query(GodotBody2D *p_body, uint32_t p_body_shape, uint32_t p_area_shape) {
	BodyKey bk(p_body, p_body_shape, p_area_shape);
	monitored_bodies[bk].inc();
	if (!monitor_query_list.in_list()) {
		_queue_monitor_update();
	}
}

void GodotArea2D::remove_body_from_query(GodotBody2D *p_body, uint32_t p_body_shape, uint32_t p_area_shape) {
	BodyKey bk(p_body, p_body_shape, p_area_shape);
	monitored_bodies[bk].dec();
	if (get_space() && !monitor_query_list.in_list()) {
		_queue_monitor_update();
	}
}

void GodotArea2D::add_area_to_query(GodotArea2D *p_area, uint32_t p_area_shape, uint32_t p_self_shape) {
	BodyKey bk(p_area, p_area_shape, p_self_shape);
	monitored_areas[bk].inc();
	if (!monitor_query_list.in_list()) {
		_queue_monitor_update();
	}
}

void GodotArea2D::remove_area_from_query(GodotArea2D *p_area, uint32_t p_area_shape, uint32_t p_self_shape) {
	BodyKey bk(p_area, p_area_shape, p_self_shape);
	monitored_areas[bk].dec();
	if (get_space() && !monitor_query_list.in_list()) {
		_queue_monitor_update();
	}
}

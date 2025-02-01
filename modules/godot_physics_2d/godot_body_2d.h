/**************************************************************************/
/*  godot_body_2d.h                                                       */
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

#include "godot_area_2d.h"
#include "godot_collision_object_2d.h"

#include "core/templates/list.h"
#include "core/templates/pair.h"
#include "core/templates/vset.h"

class GodotConstraint2D;
class GodotPhysicsDirectBodyState2D;

class GodotBody2D : public GodotCollisionObject2D {
	PhysicsServer2D::BodyMode mode = PhysicsServer2D::BODY_MODE_RIGID;

	Vector2 biased_linear_velocity;
	real_t biased_angular_velocity = 0.0;

	Vector2 linear_velocity;
	real_t angular_velocity = 0.0;

	Vector2 prev_linear_velocity;
	real_t prev_angular_velocity = 0.0;

	Vector2 constant_linear_velocity;
	real_t constant_angular_velocity = 0.0;

	PhysicsServer2D::BodyDampMode linear_damp_mode = PhysicsServer2D::BODY_DAMP_MODE_COMBINE;
	PhysicsServer2D::BodyDampMode angular_damp_mode = PhysicsServer2D::BODY_DAMP_MODE_COMBINE;

	real_t linear_damp = 0.0;
	real_t angular_damp = 0.0;

	real_t total_linear_damp = 0.0;
	real_t total_angular_damp = 0.0;

	real_t gravity_scale = 1.0;

	real_t bounce = 0.0;
	real_t friction = 1.0;

	real_t mass = 1.0;
	real_t _inv_mass = 1.0;

	real_t inertia = 0.0;
	real_t _inv_inertia = 0.0;

	Vector2 center_of_mass_local;
	Vector2 center_of_mass;

	bool calculate_inertia = true;
	bool calculate_center_of_mass = true;

	Vector2 gravity;

	real_t still_time = 0.0;

	Vector2 applied_force;
	real_t applied_torque = 0.0;

	Vector2 constant_force;
	real_t constant_torque = 0.0;

	SelfList<GodotBody2D> active_list;
	SelfList<GodotBody2D> mass_properties_update_list;
	SelfList<GodotBody2D> direct_state_query_list;

	VSet<RID> exceptions;
	PhysicsServer2D::CCDMode continuous_cd_mode = PhysicsServer2D::CCD_MODE_DISABLED;
	bool omit_force_integration = false;
	bool active = true;
	bool can_sleep = true;
	bool first_time_kinematic = false;
	void _mass_properties_changed();
	virtual void _shapes_changed() override;
	Transform2D new_transform;

	List<Pair<GodotConstraint2D *, int>> constraint_list;

	struct AreaCMP {
		GodotArea2D *area = nullptr;
		int refCount = 0;
		_FORCE_INLINE_ bool operator==(const AreaCMP &p_cmp) const { return area->get_self() == p_cmp.area->get_self(); }
		_FORCE_INLINE_ bool operator<(const AreaCMP &p_cmp) const { return area->get_priority() < p_cmp.area->get_priority(); }
		_FORCE_INLINE_ AreaCMP() {}
		_FORCE_INLINE_ AreaCMP(GodotArea2D *p_area) {
			area = p_area;
			refCount = 1;
		}
	};

	Vector<AreaCMP> areas;

	struct Contact {
		Vector2 local_pos;
		Vector2 local_normal;
		Vector2 local_velocity_at_pos;
		real_t depth = 0.0;
		int local_shape = 0;
		Vector2 collider_pos;
		int collider_shape = 0;
		ObjectID collider_instance_id;
		RID collider;
		Vector2 collider_velocity_at_pos;
		Vector2 impulse;
	};

	Vector<Contact> contacts; //no contacts by default
	int contact_count = 0;

	Callable body_state_callback;

	struct ForceIntegrationCallbackData {
		Callable callable;
		Variant udata;
	};

	ForceIntegrationCallbackData *fi_callback_data = nullptr;

	GodotPhysicsDirectBodyState2D *direct_state = nullptr;

	uint64_t island_step = 0;

	void _update_transform_dependent();

	friend class GodotPhysicsDirectBodyState2D; // i give up, too many functions to expose

public:
	void set_state_sync_callback(const Callable &p_callable);
	void set_force_integration_callback(const Callable &p_callable, const Variant &p_udata = Variant());

	GodotPhysicsDirectBodyState2D *get_direct_state();

	_FORCE_INLINE_ void add_area(GodotArea2D *p_area) {
		int index = areas.find(AreaCMP(p_area));
		if (index > -1) {
			areas.write[index].refCount += 1;
		} else {
			areas.ordered_insert(AreaCMP(p_area));
		}
	}

	_FORCE_INLINE_ void remove_area(GodotArea2D *p_area) {
		int index = areas.find(AreaCMP(p_area));
		if (index > -1) {
			areas.write[index].refCount -= 1;
			if (areas[index].refCount < 1) {
				areas.remove_at(index);
			}
		}
	}

	_FORCE_INLINE_ void set_max_contacts_reported(int p_size) {
		contacts.resize(p_size);
		contact_count = 0;
		if (mode == PhysicsServer2D::BODY_MODE_KINEMATIC && p_size) {
			set_active(true);
		}
	}

	_FORCE_INLINE_ int get_max_contacts_reported() const { return contacts.size(); }

	_FORCE_INLINE_ bool can_report_contacts() const { return !contacts.is_empty(); }
	_FORCE_INLINE_ void add_contact(const Vector2 &p_local_pos, const Vector2 &p_local_normal, real_t p_depth, int p_local_shape, const Vector2 &p_local_velocity_at_pos, const Vector2 &p_collider_pos, int p_collider_shape, ObjectID p_collider_instance_id, const RID &p_collider, const Vector2 &p_collider_velocity_at_pos, const Vector2 &p_impulse);

	_FORCE_INLINE_ void add_exception(const RID &p_exception) { exceptions.insert(p_exception); }
	_FORCE_INLINE_ void remove_exception(const RID &p_exception) { exceptions.erase(p_exception); }
	_FORCE_INLINE_ bool has_exception(const RID &p_exception) const { return exceptions.has(p_exception); }
	_FORCE_INLINE_ const VSet<RID> &get_exceptions() const { return exceptions; }

	_FORCE_INLINE_ uint64_t get_island_step() const { return island_step; }
	_FORCE_INLINE_ void set_island_step(uint64_t p_step) { island_step = p_step; }

	_FORCE_INLINE_ void add_constraint(GodotConstraint2D *p_constraint, int p_pos) { constraint_list.push_back({ p_constraint, p_pos }); }
	_FORCE_INLINE_ void remove_constraint(GodotConstraint2D *p_constraint, int p_pos) { constraint_list.erase({ p_constraint, p_pos }); }
	const List<Pair<GodotConstraint2D *, int>> &get_constraint_list() const { return constraint_list; }
	_FORCE_INLINE_ void clear_constraint_list() { constraint_list.clear(); }

	_FORCE_INLINE_ void set_omit_force_integration(bool p_omit_force_integration) { omit_force_integration = p_omit_force_integration; }
	_FORCE_INLINE_ bool get_omit_force_integration() const { return omit_force_integration; }

	_FORCE_INLINE_ void set_linear_velocity(const Vector2 &p_velocity) { linear_velocity = p_velocity; }
	_FORCE_INLINE_ Vector2 get_linear_velocity() const { return linear_velocity; }

	_FORCE_INLINE_ void set_angular_velocity(real_t p_velocity) { angular_velocity = p_velocity; }
	_FORCE_INLINE_ real_t get_angular_velocity() const { return angular_velocity; }

	_FORCE_INLINE_ Vector2 get_prev_linear_velocity() const { return prev_linear_velocity; }
	_FORCE_INLINE_ real_t get_prev_angular_velocity() const { return prev_angular_velocity; }

	_FORCE_INLINE_ void set_biased_linear_velocity(const Vector2 &p_velocity) { biased_linear_velocity = p_velocity; }
	_FORCE_INLINE_ Vector2 get_biased_linear_velocity() const { return biased_linear_velocity; }

	_FORCE_INLINE_ void set_biased_angular_velocity(real_t p_velocity) { biased_angular_velocity = p_velocity; }
	_FORCE_INLINE_ real_t get_biased_angular_velocity() const { return biased_angular_velocity; }

	_FORCE_INLINE_ void apply_central_impulse(const Vector2 &p_impulse) {
		linear_velocity += p_impulse * _inv_mass;
	}

	_FORCE_INLINE_ void apply_impulse(const Vector2 &p_impulse, const Vector2 &p_position = Vector2()) {
		linear_velocity += p_impulse * _inv_mass;
		angular_velocity += _inv_inertia * (p_position - center_of_mass).cross(p_impulse);
	}

	_FORCE_INLINE_ void apply_torque_impulse(real_t p_torque) {
		angular_velocity += _inv_inertia * p_torque;
	}

	_FORCE_INLINE_ void apply_bias_impulse(const Vector2 &p_impulse, const Vector2 &p_position = Vector2(), real_t p_max_delta_av = -1.0) {
		biased_linear_velocity += p_impulse * _inv_mass;
		if (p_max_delta_av != 0.0) {
			real_t delta_av = _inv_inertia * (p_position - center_of_mass).cross(p_impulse);
			if (p_max_delta_av > 0 && delta_av > p_max_delta_av) {
				delta_av = p_max_delta_av;
			}
			biased_angular_velocity += delta_av;
		}
	}

	_FORCE_INLINE_ void apply_central_force(const Vector2 &p_force) {
		applied_force += p_force;
	}

	_FORCE_INLINE_ void apply_force(const Vector2 &p_force, const Vector2 &p_position = Vector2()) {
		applied_force += p_force;
		applied_torque += (p_position - center_of_mass).cross(p_force);
	}

	_FORCE_INLINE_ void apply_torque(real_t p_torque) {
		applied_torque += p_torque;
	}

	_FORCE_INLINE_ void add_constant_central_force(const Vector2 &p_force) {
		constant_force += p_force;
	}

	_FORCE_INLINE_ void add_constant_force(const Vector2 &p_force, const Vector2 &p_position = Vector2()) {
		constant_force += p_force;
		constant_torque += (p_position - center_of_mass).cross(p_force);
	}

	_FORCE_INLINE_ void add_constant_torque(real_t p_torque) {
		constant_torque += p_torque;
	}

	void set_constant_force(const Vector2 &p_force) { constant_force = p_force; }
	Vector2 get_constant_force() const { return constant_force; }

	void set_constant_torque(real_t p_torque) { constant_torque = p_torque; }
	real_t get_constant_torque() const { return constant_torque; }

	void set_active(bool p_active);
	_FORCE_INLINE_ bool is_active() const { return active; }

	_FORCE_INLINE_ void wakeup() {
		if ((!get_space()) || mode == PhysicsServer2D::BODY_MODE_STATIC || mode == PhysicsServer2D::BODY_MODE_KINEMATIC) {
			return;
		}
		set_active(true);
	}

	void set_param(PhysicsServer2D::BodyParameter p_param, const Variant &p_value);
	Variant get_param(PhysicsServer2D::BodyParameter p_param) const;

	void set_mode(PhysicsServer2D::BodyMode p_mode);
	PhysicsServer2D::BodyMode get_mode() const;

	void set_state(PhysicsServer2D::BodyState p_state, const Variant &p_variant);
	Variant get_state(PhysicsServer2D::BodyState p_state) const;

	_FORCE_INLINE_ void set_continuous_collision_detection_mode(PhysicsServer2D::CCDMode p_mode) { continuous_cd_mode = p_mode; }
	_FORCE_INLINE_ PhysicsServer2D::CCDMode get_continuous_collision_detection_mode() const { return continuous_cd_mode; }

	void set_space(GodotSpace2D *p_space) override;

	void update_mass_properties();
	void reset_mass_properties();

	_FORCE_INLINE_ const Vector2 &get_center_of_mass() const { return center_of_mass; }
	_FORCE_INLINE_ const Vector2 &get_center_of_mass_local() const { return center_of_mass_local; }
	_FORCE_INLINE_ real_t get_inv_mass() const { return _inv_mass; }
	_FORCE_INLINE_ real_t get_inv_inertia() const { return _inv_inertia; }
	_FORCE_INLINE_ real_t get_friction() const { return friction; }
	_FORCE_INLINE_ real_t get_bounce() const { return bounce; }

	void integrate_forces(real_t p_step);
	void integrate_velocities(real_t p_step);

	_FORCE_INLINE_ Vector2 get_velocity_in_local_point(const Vector2 &rel_pos) const {
		return linear_velocity + Vector2(-angular_velocity * rel_pos.y, angular_velocity * rel_pos.x);
	}

	_FORCE_INLINE_ Vector2 get_motion() const {
		if (mode > PhysicsServer2D::BODY_MODE_KINEMATIC) {
			return new_transform.get_origin() - get_transform().get_origin();
		} else if (mode == PhysicsServer2D::BODY_MODE_KINEMATIC) {
			return get_transform().get_origin() - new_transform.get_origin(); //kinematic simulates forward
		}
		return Vector2();
	}

	void call_queries();
	void wakeup_neighbours();

	bool sleep_test(real_t p_step);

	GodotBody2D();
	~GodotBody2D();
};

//add contact inline

void GodotBody2D::add_contact(const Vector2 &p_local_pos, const Vector2 &p_local_normal, real_t p_depth, int p_local_shape, const Vector2 &p_local_velocity_at_pos, const Vector2 &p_collider_pos, int p_collider_shape, ObjectID p_collider_instance_id, const RID &p_collider, const Vector2 &p_collider_velocity_at_pos, const Vector2 &p_impulse) {
	int c_max = contacts.size();

	if (c_max == 0) {
		return;
	}

	Contact *c = contacts.ptrw();

	int idx = -1;

	if (contact_count < c_max) {
		idx = contact_count++;
	} else {
		real_t least_depth = 1e20;
		int least_deep = -1;
		for (int i = 0; i < c_max; i++) {
			if (i == 0 || c[i].depth < least_depth) {
				least_deep = i;
				least_depth = c[i].depth;
			}
		}

		if (least_deep >= 0 && least_depth < p_depth) {
			idx = least_deep;
		}
		if (idx == -1) {
			return; //none least deepe than this
		}
	}

	c[idx].local_pos = p_local_pos;
	c[idx].local_normal = p_local_normal;
	c[idx].local_velocity_at_pos = p_local_velocity_at_pos;
	c[idx].depth = p_depth;
	c[idx].local_shape = p_local_shape;
	c[idx].collider_pos = p_collider_pos;
	c[idx].collider_shape = p_collider_shape;
	c[idx].collider_instance_id = p_collider_instance_id;
	c[idx].collider = p_collider;
	c[idx].collider_velocity_at_pos = p_collider_velocity_at_pos;
	c[idx].impulse = p_impulse;
}

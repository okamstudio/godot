/**************************************************************************/
/*  godot_body_3d.h                                                       */
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

#include "godot_area_3d.h"
#include "godot_collision_object_3d.h"

#include "core/templates/vset.h"

class GodotConstraint3D;
class GodotPhysicsDirectBodyState3D;

class GodotBody3D : public GodotCollisionObject3D {
	PhysicsServer3D::BodyMode mode = PhysicsServer3D::BODY_MODE_RIGID;

	Vector3 linear_velocity;
	Vector3 angular_velocity;

	Vector3 prev_linear_velocity;
	Vector3 prev_angular_velocity;

	Vector3 constant_linear_velocity;
	Vector3 constant_angular_velocity;

	Vector3 biased_linear_velocity;
	Vector3 biased_angular_velocity;
	real_t mass = 1.0;
	real_t bounce = 0.0;
	real_t friction = 1.0;
	Vector3 inertia;

	PhysicsServer3D::BodyDampMode linear_damp_mode = PhysicsServer3D::BODY_DAMP_MODE_COMBINE;
	PhysicsServer3D::BodyDampMode angular_damp_mode = PhysicsServer3D::BODY_DAMP_MODE_COMBINE;

	real_t linear_damp = 0.0;
	real_t angular_damp = 0.0;

	real_t total_linear_damp = 0.0;
	real_t total_angular_damp = 0.0;

	real_t gravity_scale = 1.0;

	uint16_t locked_axis = 0;

	real_t _inv_mass = 1.0;
	Vector3 _inv_inertia; // Relative to the principal axes of inertia

	// Relative to the local frame of reference
	Basis principal_inertia_axes_local;
	Vector3 center_of_mass_local;

	// In world orientation with local origin
	Basis _inv_inertia_tensor;
	Basis principal_inertia_axes;
	Vector3 center_of_mass;

	bool calculate_inertia = true;
	bool calculate_center_of_mass = true;

	Vector3 gravity;

	real_t still_time = 0.0;

	Vector3 applied_force;
	Vector3 applied_torque;

	Vector3 constant_force;
	Vector3 constant_torque;

	SelfList<GodotBody3D> active_list;
	SelfList<GodotBody3D> mass_properties_update_list;
	SelfList<GodotBody3D> direct_state_query_list;

	VSet<RID> exceptions;
	bool omit_force_integration = false;
	bool active = true;

	bool continuous_cd = false;
	bool can_sleep = true;
	bool first_time_kinematic = false;

	void _mass_properties_changed();
	virtual void _shapes_changed() override;
	Transform3D new_transform;

	HashMap<GodotConstraint3D *, int> constraint_map;

	Vector<AreaCMP> areas;

	struct Contact {
		Vector3 local_pos;
		Vector3 local_normal;
		Vector3 local_velocity_at_pos;
		real_t depth = 0.0;
		int local_shape = 0;
		Vector3 collider_pos;
		int collider_shape = 0;
		ObjectID collider_instance_id;
		RID collider;
		Vector3 collider_velocity_at_pos;
		Vector3 impulse;
	};

	Vector<Contact> contacts; //no contacts by default
	int contact_count = 0;

	Callable body_state_callback;

	struct ForceIntegrationCallbackData {
		Callable callable;
		Variant udata;
	};

	ForceIntegrationCallbackData *fi_callback_data = nullptr;

	GodotPhysicsDirectBodyState3D *direct_state = nullptr;

	uint64_t island_step = 0;

	void _update_transform_dependent();

	friend class GodotPhysicsDirectBodyState3D; // i give up, too many functions to expose

public:
	void set_state_sync_callback(const Callable &p_callable);
	void set_force_integration_callback(const Callable &p_callable, const Variant &p_udata = Variant());

	GodotPhysicsDirectBodyState3D *get_direct_state();

	_FORCE_INLINE_ void add_area(GodotArea3D *p_area) {
		int index = areas.find(AreaCMP(p_area));
		if (index > -1) {
			areas.write[index].refCount += 1;
		} else {
			areas.ordered_insert(AreaCMP(p_area));
		}
	}

	_FORCE_INLINE_ void remove_area(GodotArea3D *p_area) {
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
		if (mode == PhysicsServer3D::BODY_MODE_KINEMATIC && p_size) {
			set_active(true);
		}
	}
	_FORCE_INLINE_ int get_max_contacts_reported() const { return contacts.size(); }

	_FORCE_INLINE_ bool can_report_contacts() const { return !contacts.is_empty(); }
	_FORCE_INLINE_ void add_contact(const Vector3 &p_local_pos, const Vector3 &p_local_normal, real_t p_depth, int p_local_shape, const Vector3 &p_local_velocity_at_pos, const Vector3 &p_collider_pos, int p_collider_shape, ObjectID p_collider_instance_id, const RID &p_collider, const Vector3 &p_collider_velocity_at_pos, const Vector3 &p_impulse);

	_FORCE_INLINE_ void add_exception(const RID &p_exception) { exceptions.insert(p_exception); }
	_FORCE_INLINE_ void remove_exception(const RID &p_exception) { exceptions.erase(p_exception); }
	_FORCE_INLINE_ bool has_exception(const RID &p_exception) const { return exceptions.has(p_exception); }
	_FORCE_INLINE_ const VSet<RID> &get_exceptions() const { return exceptions; }

	_FORCE_INLINE_ uint64_t get_island_step() const { return island_step; }
	_FORCE_INLINE_ void set_island_step(uint64_t p_step) { island_step = p_step; }

	_FORCE_INLINE_ void add_constraint(GodotConstraint3D *p_constraint, int p_pos) { constraint_map[p_constraint] = p_pos; }
	_FORCE_INLINE_ void remove_constraint(GodotConstraint3D *p_constraint) { constraint_map.erase(p_constraint); }
	const HashMap<GodotConstraint3D *, int> &get_constraint_map() const { return constraint_map; }
	_FORCE_INLINE_ void clear_constraint_map() { constraint_map.clear(); }

	_FORCE_INLINE_ void set_omit_force_integration(bool p_omit_force_integration) { omit_force_integration = p_omit_force_integration; }
	_FORCE_INLINE_ bool get_omit_force_integration() const { return omit_force_integration; }

	_FORCE_INLINE_ Basis get_principal_inertia_axes() const { return principal_inertia_axes; }
	_FORCE_INLINE_ Vector3 get_center_of_mass() const { return center_of_mass; }
	_FORCE_INLINE_ Vector3 get_center_of_mass_local() const { return center_of_mass_local; }
	_FORCE_INLINE_ Vector3 xform_local_to_principal(const Vector3 &p_pos) const { return principal_inertia_axes_local.xform(p_pos - center_of_mass_local); }

	_FORCE_INLINE_ void set_linear_velocity(const Vector3 &p_velocity) { linear_velocity = p_velocity; }
	_FORCE_INLINE_ Vector3 get_linear_velocity() const { return linear_velocity; }

	_FORCE_INLINE_ void set_angular_velocity(const Vector3 &p_velocity) { angular_velocity = p_velocity; }
	_FORCE_INLINE_ Vector3 get_angular_velocity() const { return angular_velocity; }

	_FORCE_INLINE_ Vector3 get_prev_linear_velocity() const { return prev_linear_velocity; }
	_FORCE_INLINE_ Vector3 get_prev_angular_velocity() const { return prev_angular_velocity; }

	_FORCE_INLINE_ const Vector3 &get_biased_linear_velocity() const { return biased_linear_velocity; }
	_FORCE_INLINE_ const Vector3 &get_biased_angular_velocity() const { return biased_angular_velocity; }

	_FORCE_INLINE_ void apply_central_impulse(const Vector3 &p_impulse) {
		linear_velocity += p_impulse * _inv_mass;
	}

	_FORCE_INLINE_ void apply_impulse(const Vector3 &p_impulse, const Vector3 &p_position = Vector3()) {
		linear_velocity += p_impulse * _inv_mass;
		angular_velocity += _inv_inertia_tensor.xform((p_position - center_of_mass).cross(p_impulse));
	}

	_FORCE_INLINE_ void apply_torque_impulse(const Vector3 &p_impulse) {
		angular_velocity += _inv_inertia_tensor.xform(p_impulse);
	}

	_FORCE_INLINE_ void apply_bias_impulse(const Vector3 &p_impulse, const Vector3 &p_position = Vector3(), real_t p_max_delta_av = -1.0) {
		biased_linear_velocity += p_impulse * _inv_mass;
		if (p_max_delta_av != 0.0) {
			Vector3 delta_av = _inv_inertia_tensor.xform((p_position - center_of_mass).cross(p_impulse));
			if (p_max_delta_av > 0 && delta_av.length() > p_max_delta_av) {
				delta_av = delta_av.normalized() * p_max_delta_av;
			}
			biased_angular_velocity += delta_av;
		}
	}

	_FORCE_INLINE_ void apply_bias_torque_impulse(const Vector3 &p_impulse) {
		biased_angular_velocity += _inv_inertia_tensor.xform(p_impulse);
	}

	_FORCE_INLINE_ void apply_central_force(const Vector3 &p_force) {
		applied_force += p_force;
	}

	_FORCE_INLINE_ void apply_force(const Vector3 &p_force, const Vector3 &p_position = Vector3()) {
		applied_force += p_force;
		applied_torque += (p_position - center_of_mass).cross(p_force);
	}

	_FORCE_INLINE_ void apply_torque(const Vector3 &p_torque) {
		applied_torque += p_torque;
	}

	_FORCE_INLINE_ void add_constant_central_force(const Vector3 &p_force) {
		constant_force += p_force;
	}

	_FORCE_INLINE_ void add_constant_force(const Vector3 &p_force, const Vector3 &p_position = Vector3()) {
		constant_force += p_force;
		constant_torque += (p_position - center_of_mass).cross(p_force);
	}

	_FORCE_INLINE_ void add_constant_torque(const Vector3 &p_torque) {
		constant_torque += p_torque;
	}

	void set_constant_force(const Vector3 &p_force) { constant_force = p_force; }
	Vector3 get_constant_force() const { return constant_force; }

	void set_constant_torque(const Vector3 &p_torque) { constant_torque = p_torque; }
	Vector3 get_constant_torque() const { return constant_torque; }

	void set_active(bool p_active);
	_FORCE_INLINE_ bool is_active() const { return active; }

	_FORCE_INLINE_ void wakeup() {
		if ((!get_space()) || mode == PhysicsServer3D::BODY_MODE_STATIC || mode == PhysicsServer3D::BODY_MODE_KINEMATIC) {
			return;
		}
		set_active(true);
	}

	void set_param(PhysicsServer3D::BodyParameter p_param, const Variant &p_value);
	Variant get_param(PhysicsServer3D::BodyParameter p_param) const;

	void set_mode(PhysicsServer3D::BodyMode p_mode);
	PhysicsServer3D::BodyMode get_mode() const;

	void set_state(PhysicsServer3D::BodyState p_state, const Variant &p_variant);
	Variant get_state(PhysicsServer3D::BodyState p_state) const;

	_FORCE_INLINE_ void set_continuous_collision_detection(bool p_enable) { continuous_cd = p_enable; }
	_FORCE_INLINE_ bool is_continuous_collision_detection_enabled() const { return continuous_cd; }

	void set_space(GodotSpace3D *p_space) override;

	void update_mass_properties();
	void reset_mass_properties();

	_FORCE_INLINE_ real_t get_inv_mass() const { return _inv_mass; }
	_FORCE_INLINE_ const Vector3 &get_inv_inertia() const { return _inv_inertia; }
	_FORCE_INLINE_ const Basis &get_inv_inertia_tensor() const { return _inv_inertia_tensor; }
	_FORCE_INLINE_ real_t get_friction() const { return friction; }
	_FORCE_INLINE_ real_t get_bounce() const { return bounce; }

	void set_axis_lock(PhysicsServer3D::BodyAxis p_axis, bool lock);
	bool is_axis_locked(PhysicsServer3D::BodyAxis p_axis) const;

	void integrate_forces(real_t p_step);
	void integrate_velocities(real_t p_step);

	_FORCE_INLINE_ Vector3 get_velocity_in_local_point(const Vector3 &rel_pos) const {
		return linear_velocity + angular_velocity.cross(rel_pos - center_of_mass);
	}

	_FORCE_INLINE_ real_t compute_impulse_denominator(const Vector3 &p_pos, const Vector3 &p_normal) const {
		Vector3 r0 = p_pos - get_transform().origin - center_of_mass;

		Vector3 c0 = (r0).cross(p_normal);

		Vector3 vec = (_inv_inertia_tensor.xform_inv(c0)).cross(r0);

		return _inv_mass + p_normal.dot(vec);
	}

	_FORCE_INLINE_ real_t compute_angular_impulse_denominator(const Vector3 &p_axis) const {
		return p_axis.dot(_inv_inertia_tensor.xform_inv(p_axis));
	}

	//void simulate_motion(const Transform3D& p_xform,real_t p_step);
	void call_queries();
	void wakeup_neighbours();

	bool sleep_test(real_t p_step);

	GodotBody3D();
	~GodotBody3D();
};

//add contact inline

void GodotBody3D::add_contact(const Vector3 &p_local_pos, const Vector3 &p_local_normal, real_t p_depth, int p_local_shape, const Vector3 &p_local_velocity_at_pos, const Vector3 &p_collider_pos, int p_collider_shape, ObjectID p_collider_instance_id, const RID &p_collider, const Vector3 &p_collider_velocity_at_pos, const Vector3 &p_impulse) {
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

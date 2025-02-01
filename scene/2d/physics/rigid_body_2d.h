/**************************************************************************/
/*  rigid_body_2d.h                                                       */
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
#include "scene/2d/physics/physics_body_2d.h"

class RigidBody2D : public PhysicsBody2D {
	GDCLASS(RigidBody2D, PhysicsBody2D);

public:
	enum FreezeMode {
		FREEZE_MODE_STATIC,
		FREEZE_MODE_KINEMATIC,
	};

	enum CenterOfMassMode {
		CENTER_OF_MASS_MODE_AUTO,
		CENTER_OF_MASS_MODE_CUSTOM,
	};

	enum DampMode {
		DAMP_MODE_COMBINE,
		DAMP_MODE_REPLACE,
	};

	enum CCDMode {
		CCD_MODE_DISABLED,
		CCD_MODE_CAST_RAY,
		CCD_MODE_CAST_SHAPE,
	};

private:
	bool can_sleep = true;
	bool lock_rotation = false;
	bool freeze = false;
	FreezeMode freeze_mode = FREEZE_MODE_STATIC;

	real_t mass = 1.0;
	real_t inertia = 0.0;
	CenterOfMassMode center_of_mass_mode = CENTER_OF_MASS_MODE_AUTO;
	Vector2 center_of_mass;

	Ref<PhysicsMaterial> physics_material_override;
	real_t gravity_scale = 1.0;

	DampMode linear_damp_mode = DAMP_MODE_COMBINE;
	DampMode angular_damp_mode = DAMP_MODE_COMBINE;

	real_t linear_damp = 0.0;
	real_t angular_damp = 0.0;

	Vector2 linear_velocity;
	real_t angular_velocity = 0.0;
	bool sleeping = false;

	int max_contacts_reported = 0;
	int contact_count = 0;

	bool custom_integrator = false;

	CCDMode ccd_mode = CCD_MODE_DISABLED;

	struct ShapePair {
		int body_shape = 0;
		int local_shape = 0;
		bool tagged = false;
		bool operator<(const ShapePair &p_sp) const {
			if (body_shape == p_sp.body_shape) {
				return local_shape < p_sp.local_shape;
			}

			return body_shape < p_sp.body_shape;
		}

		ShapePair() {}
		ShapePair(int p_bs, int p_ls) {
			body_shape = p_bs;
			local_shape = p_ls;
		}
	};
	struct RigidBody2D_RemoveAction {
		RID rid;
		ObjectID body_id;
		ShapePair pair;
	};
	struct BodyState {
		RID rid;
		//int rc;
		bool in_scene = false;
		VSet<ShapePair> shapes;
	};

	struct ContactMonitor {
		bool locked = false;
		HashMap<ObjectID, BodyState> body_map;
	};

	ContactMonitor *contact_monitor = nullptr;
	void _body_enter_tree(ObjectID p_id);
	void _body_exit_tree(ObjectID p_id);

	void _body_inout(int p_status, const RID &p_body, ObjectID p_instance, int p_body_shape, int p_local_shape);

	static void _body_state_changed_callback(void *p_instance, PhysicsDirectBodyState2D *p_state);
	void _body_state_changed(PhysicsDirectBodyState2D *p_state);

	void _sync_body_state(PhysicsDirectBodyState2D *p_state);

protected:
	void _notification(int p_what);
	static void _bind_methods();

	void _validate_property(PropertyInfo &p_property) const;

	GDVIRTUAL1(_integrate_forces, PhysicsDirectBodyState2D *)

	void _apply_body_mode();

public:
	void set_lock_rotation_enabled(bool p_lock_rotation);
	bool is_lock_rotation_enabled() const;

	void set_freeze_enabled(bool p_freeze);
	bool is_freeze_enabled() const;

	void set_freeze_mode(FreezeMode p_freeze_mode);
	FreezeMode get_freeze_mode() const;

	void set_mass(real_t p_mass);
	real_t get_mass() const;

	void set_inertia(real_t p_inertia);
	real_t get_inertia() const;

	void set_center_of_mass_mode(CenterOfMassMode p_mode);
	CenterOfMassMode get_center_of_mass_mode() const;

	void set_center_of_mass(const Vector2 &p_center_of_mass);
	const Vector2 &get_center_of_mass() const;

	void set_physics_material_override(const Ref<PhysicsMaterial> &p_physics_material_override);
	Ref<PhysicsMaterial> get_physics_material_override() const;

	void set_gravity_scale(real_t p_gravity_scale);
	real_t get_gravity_scale() const;

	void set_linear_damp_mode(DampMode p_mode);
	DampMode get_linear_damp_mode() const;

	void set_angular_damp_mode(DampMode p_mode);
	DampMode get_angular_damp_mode() const;

	void set_linear_damp(real_t p_linear_damp);
	real_t get_linear_damp() const;

	void set_angular_damp(real_t p_angular_damp);
	real_t get_angular_damp() const;

	void set_linear_velocity(const Vector2 &p_velocity);
	Vector2 get_linear_velocity() const;

	void set_axis_velocity(const Vector2 &p_axis);

	void set_angular_velocity(real_t p_velocity);
	real_t get_angular_velocity() const;

	void set_use_custom_integrator(bool p_enable);
	bool is_using_custom_integrator();

	void set_sleeping(bool p_sleeping);
	bool is_sleeping() const;

	void set_can_sleep(bool p_active);
	bool is_able_to_sleep() const;

	void set_contact_monitor(bool p_enabled);
	bool is_contact_monitor_enabled() const;

	void set_max_contacts_reported(int p_amount);
	int get_max_contacts_reported() const;
	int get_contact_count() const;

	void set_continuous_collision_detection_mode(CCDMode p_mode);
	CCDMode get_continuous_collision_detection_mode() const;

	void apply_central_impulse(const Vector2 &p_impulse);
	void apply_impulse(const Vector2 &p_impulse, const Vector2 &p_position = Vector2());
	void apply_torque_impulse(real_t p_torque);

	void apply_central_force(const Vector2 &p_force);
	void apply_force(const Vector2 &p_force, const Vector2 &p_position = Vector2());
	void apply_torque(real_t p_torque);

	void add_constant_central_force(const Vector2 &p_force);
	void add_constant_force(const Vector2 &p_force, const Vector2 &p_position = Vector2());
	void add_constant_torque(real_t p_torque);

	void set_constant_force(const Vector2 &p_force);
	Vector2 get_constant_force() const;

	void set_constant_torque(real_t p_torque);
	real_t get_constant_torque() const;

	TypedArray<Node2D> get_colliding_bodies() const; //function for script

	virtual PackedStringArray get_configuration_warnings() const override;

	RigidBody2D();
	~RigidBody2D();

private:
	void _reload_physics_characteristics();
};

VARIANT_ENUM_CAST(RigidBody2D::FreezeMode);
VARIANT_ENUM_CAST(RigidBody2D::CenterOfMassMode);
VARIANT_ENUM_CAST(RigidBody2D::DampMode);
VARIANT_ENUM_CAST(RigidBody2D::CCDMode);

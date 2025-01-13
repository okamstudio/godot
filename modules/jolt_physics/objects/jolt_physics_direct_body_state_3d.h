/**************************************************************************/
/*  jolt_physics_direct_body_state_3d.h                                   */
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

#ifndef JOLT_PHYSICS_DIRECT_BODY_STATE_3D_H
#define JOLT_PHYSICS_DIRECT_BODY_STATE_3D_H

#include "servers/physics_server_3d.h"

class JoltBody3D;

class JoltPhysicsDirectBodyState3D final : public PhysicsDirectBodyState3D {
	GDCLASS(JoltPhysicsDirectBodyState3D, PhysicsDirectBodyState3D)

	JoltBody3D *body = nullptr;

	static void _bind_methods() {}

public:
	JoltPhysicsDirectBodyState3D() = default;

	explicit JoltPhysicsDirectBodyState3D(JoltBody3D *p_body);

	virtual Vector3 get_total_gravity() const override;
	virtual real_t get_total_linear_damp() const override;
	virtual real_t get_total_angular_damp() const override;

	virtual Vector3 get_center_of_mass() const override;
	virtual Vector3 get_center_of_mass_local() const override;
	virtual Basis get_principal_inertia_axes() const override;

	virtual real_t get_inverse_mass() const override;
	virtual Vector3 get_inverse_inertia() const override;
	virtual Basis get_inverse_inertia_tensor() const override;

	virtual void set_linear_velocity(const Vector3 &p_velocity) override;
	virtual Vector3 get_linear_velocity() const override;

	virtual void set_angular_velocity(const Vector3 &p_velocity) override;
	virtual Vector3 get_angular_velocity() const override;

	virtual void set_transform(const Transform3D &p_transform) override;
	virtual Transform3D get_transform() const override;

	virtual Vector3 get_velocity_at_local_position(const Vector3 &p_local_position) const override;

	virtual void apply_central_impulse(const Vector3 &p_impulse) override;
	virtual void apply_impulse(const Vector3 &p_impulse, const Vector3 &p_position) override;
	virtual void apply_torque_impulse(const Vector3 &p_impulse) override;

	virtual void apply_central_force(const Vector3 &p_force) override;
	virtual void apply_force(const Vector3 &p_force, const Vector3 &p_position) override;
	virtual void apply_torque(const Vector3 &p_torque) override;

	virtual void add_constant_central_force(const Vector3 &p_force) override;
	virtual void add_constant_force(const Vector3 &p_force, const Vector3 &p_position) override;
	virtual void add_constant_torque(const Vector3 &p_torque) override;

	virtual void set_constant_force(const Vector3 &p_force) override;
	virtual Vector3 get_constant_force() const override;

	virtual void set_constant_torque(const Vector3 &p_torque) override;
	virtual Vector3 get_constant_torque() const override;

	virtual void set_sleep_state(bool p_enabled) override;
	virtual bool is_sleeping() const override;

	virtual int get_contact_count() const override;

	virtual Vector3 get_contact_local_position(int p_contact_idx) const override;
	virtual Vector3 get_contact_local_normal(int p_contact_idx) const override;
	virtual Vector3 get_contact_impulse(int p_contact_idx) const override;
	virtual int get_contact_local_shape(int p_contact_idx) const override;
	virtual Vector3 get_contact_local_velocity_at_position(int p_contact_idx) const override;

	virtual RID get_contact_collider(int p_contact_idx) const override;
	virtual Vector3 get_contact_collider_position(int p_contact_idx) const override;
	virtual ObjectID get_contact_collider_id(int p_contact_idx) const override;
	virtual Object *get_contact_collider_object(int p_contact_idx) const override;
	virtual int get_contact_collider_shape(int p_contact_idx) const override;
	virtual Vector3 get_contact_collider_velocity_at_position(int p_contact_idx) const override;

	virtual real_t get_step() const override;

	virtual void integrate_forces() override;

	virtual PhysicsDirectSpaceState3D *get_space_state() override;
};

#endif // JOLT_PHYSICS_DIRECT_BODY_STATE_3D_H

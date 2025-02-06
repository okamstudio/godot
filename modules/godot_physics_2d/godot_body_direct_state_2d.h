/**************************************************************************/
/*  godot_body_direct_state_2d.h                                          */
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

#include "servers/physics_server_2d.h"

class GodotBody2D;

class GodotPhysicsDirectBodyState2D : public PhysicsDirectBodyState2D {
	GDCLASS(GodotPhysicsDirectBodyState2D, PhysicsDirectBodyState2D);

public:
	GodotBody2D *body = nullptr;

	virtual Vector2 get_total_gravity() const override;
	virtual real_t get_total_angular_damp() const override;
	virtual real_t get_total_linear_damp() const override;

	virtual Vector2 get_center_of_mass() const override;
	virtual Vector2 get_center_of_mass_local() const override;
	virtual real_t get_inverse_mass() const override;
	virtual real_t get_inverse_inertia() const override;

	virtual void set_linear_velocity(const Vector2 &p_velocity) override;
	virtual Vector2 get_linear_velocity() const override;

	virtual void set_angular_velocity(real_t p_velocity) override;
	virtual real_t get_angular_velocity() const override;

	virtual void set_transform(const Transform2D &p_transform) override;
	virtual Transform2D get_transform() const override;

	virtual Vector2 get_velocity_at_local_position(const Vector2 &p_position) const override;

	virtual void apply_central_impulse(const Vector2 &p_impulse) override;
	virtual void apply_impulse(const Vector2 &p_impulse, const Vector2 &p_position = Vector2()) override;
	virtual void apply_torque_impulse(real_t p_torque) override;

	virtual void apply_central_force(const Vector2 &p_force) override;
	virtual void apply_force(const Vector2 &p_force, const Vector2 &p_position = Vector2()) override;
	virtual void apply_torque(real_t p_torque) override;

	virtual void add_constant_central_force(const Vector2 &p_force) override;
	virtual void add_constant_force(const Vector2 &p_force, const Vector2 &p_position = Vector2()) override;
	virtual void add_constant_torque(real_t p_torque) override;

	virtual void set_constant_force(const Vector2 &p_force) override;
	virtual Vector2 get_constant_force() const override;

	virtual void set_constant_torque(real_t p_torque) override;
	virtual real_t get_constant_torque() const override;

	virtual void set_sleep_state(bool p_enable) override;
	virtual bool is_sleeping() const override;

	virtual int get_contact_count() const override;

	virtual Vector2 get_contact_local_position(int p_contact_idx) const override;
	virtual Vector2 get_contact_local_normal(int p_contact_idx) const override;
	virtual int get_contact_local_shape(int p_contact_idx) const override;
	virtual Vector2 get_contact_local_velocity_at_position(int p_contact_idx) const override;

	virtual RID get_contact_collider(int p_contact_idx) const override;
	virtual Vector2 get_contact_collider_position(int p_contact_idx) const override;
	virtual ObjectID get_contact_collider_id(int p_contact_idx) const override;
	virtual int get_contact_collider_shape(int p_contact_idx) const override;
	virtual Vector2 get_contact_collider_velocity_at_position(int p_contact_idx) const override;
	virtual Vector2 get_contact_impulse(int p_contact_idx) const override;

	virtual PhysicsDirectSpaceState2D *get_space_state() override;

	virtual real_t get_step() const override;
};

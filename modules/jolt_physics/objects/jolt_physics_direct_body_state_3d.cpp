/**************************************************************************/
/*  jolt_physics_direct_body_state_3d.cpp                                 */
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

#include "jolt_physics_direct_body_state_3d.h"

#include "../spaces/jolt_physics_direct_space_state_3d.h"
#include "../spaces/jolt_space_3d.h"
#include "jolt_body_3d.h"

JoltPhysicsDirectBodyState3D::JoltPhysicsDirectBodyState3D(JoltBody3D *p_body) :
		body(p_body) {
}

Vector3 JoltPhysicsDirectBodyState3D::get_total_gravity() const {
	return body->get_gravity();
}

real_t JoltPhysicsDirectBodyState3D::get_total_angular_damp() const {
	return (real_t)body->get_total_angular_damp();
}

real_t JoltPhysicsDirectBodyState3D::get_total_linear_damp() const {
	return (real_t)body->get_total_linear_damp();
}

Vector3 JoltPhysicsDirectBodyState3D::get_center_of_mass() const {
	return body->get_center_of_mass_relative();
}

Vector3 JoltPhysicsDirectBodyState3D::get_center_of_mass_local() const {
	return body->get_center_of_mass_local();
}

Basis JoltPhysicsDirectBodyState3D::get_principal_inertia_axes() const {
	return body->get_principal_inertia_axes();
}

real_t JoltPhysicsDirectBodyState3D::get_inverse_mass() const {
	return 1.0 / body->get_mass();
}

Vector3 JoltPhysicsDirectBodyState3D::get_inverse_inertia() const {
	return body->get_inverse_inertia();
}

Basis JoltPhysicsDirectBodyState3D::get_inverse_inertia_tensor() const {
	return body->get_inverse_inertia_tensor();
}

Vector3 JoltPhysicsDirectBodyState3D::get_linear_velocity() const {
	return body->get_linear_velocity();
}

void JoltPhysicsDirectBodyState3D::set_linear_velocity(const Vector3 &p_velocity) {
	return body->set_linear_velocity(p_velocity);
}

Vector3 JoltPhysicsDirectBodyState3D::get_angular_velocity() const {
	return body->get_angular_velocity();
}

void JoltPhysicsDirectBodyState3D::set_angular_velocity(const Vector3 &p_velocity) {
	return body->set_angular_velocity(p_velocity);
}

void JoltPhysicsDirectBodyState3D::set_transform(const Transform3D &p_transform) {
	return body->set_transform(p_transform);
}

Transform3D JoltPhysicsDirectBodyState3D::get_transform() const {
	return body->get_transform_scaled();
}

Vector3 JoltPhysicsDirectBodyState3D::get_velocity_at_local_position(const Vector3 &p_local_position) const {
	return body->get_velocity_at_position(body->get_position() + p_local_position);
}

void JoltPhysicsDirectBodyState3D::apply_central_impulse(const Vector3 &p_impulse) {
	return body->apply_central_impulse(p_impulse);
}

void JoltPhysicsDirectBodyState3D::apply_impulse(const Vector3 &p_impulse, const Vector3 &p_position) {
	return body->apply_impulse(p_impulse, p_position);
}

void JoltPhysicsDirectBodyState3D::apply_torque_impulse(const Vector3 &p_impulse) {
	return body->apply_torque_impulse(p_impulse);
}

void JoltPhysicsDirectBodyState3D::apply_central_force(const Vector3 &p_force) {
	return body->apply_central_force(p_force);
}

void JoltPhysicsDirectBodyState3D::apply_force(const Vector3 &p_force, const Vector3 &p_position) {
	return body->apply_force(p_force, p_position);
}

void JoltPhysicsDirectBodyState3D::apply_torque(const Vector3 &p_torque) {
	return body->apply_torque(p_torque);
}

void JoltPhysicsDirectBodyState3D::add_constant_central_force(const Vector3 &p_force) {
	return body->add_constant_central_force(p_force);
}

void JoltPhysicsDirectBodyState3D::add_constant_force(const Vector3 &p_force, const Vector3 &p_position) {
	return body->add_constant_force(p_force, p_position);
}

void JoltPhysicsDirectBodyState3D::add_constant_torque(const Vector3 &p_torque) {
	return body->add_constant_torque(p_torque);
}

Vector3 JoltPhysicsDirectBodyState3D::get_constant_force() const {
	return body->get_constant_force();
}

void JoltPhysicsDirectBodyState3D::set_constant_force(const Vector3 &p_force) {
	return body->set_constant_force(p_force);
}

Vector3 JoltPhysicsDirectBodyState3D::get_constant_torque() const {
	return body->get_constant_torque();
}

void JoltPhysicsDirectBodyState3D::set_constant_torque(const Vector3 &p_torque) {
	return body->set_constant_torque(p_torque);
}

bool JoltPhysicsDirectBodyState3D::is_sleeping() const {
	return body->is_sleeping();
}

void JoltPhysicsDirectBodyState3D::set_sleep_state(bool p_enabled) {
	body->set_is_sleeping(p_enabled);
}

int JoltPhysicsDirectBodyState3D::get_contact_count() const {
	return body->get_contact_count();
}

Vector3 JoltPhysicsDirectBodyState3D::get_contact_local_position(int p_contact_idx) const {
	ERR_FAIL_INDEX_V(p_contact_idx, (int)body->get_contact_count(), Vector3());
	return body->get_contact(p_contact_idx).position;
}

Vector3 JoltPhysicsDirectBodyState3D::get_contact_local_normal(int p_contact_idx) const {
	ERR_FAIL_INDEX_V(p_contact_idx, (int)body->get_contact_count(), Vector3());
	return body->get_contact(p_contact_idx).normal;
}

Vector3 JoltPhysicsDirectBodyState3D::get_contact_impulse(int p_contact_idx) const {
	ERR_FAIL_INDEX_V(p_contact_idx, (int)body->get_contact_count(), Vector3());
	return body->get_contact(p_contact_idx).impulse;
}

int JoltPhysicsDirectBodyState3D::get_contact_local_shape(int p_contact_idx) const {
	ERR_FAIL_INDEX_V(p_contact_idx, (int)body->get_contact_count(), 0);
	return body->get_contact(p_contact_idx).shape_index;
}

Vector3 JoltPhysicsDirectBodyState3D::get_contact_local_velocity_at_position(int p_contact_idx) const {
	ERR_FAIL_INDEX_V(p_contact_idx, (int)body->get_contact_count(), Vector3());
	return body->get_contact(p_contact_idx).velocity;
}

RID JoltPhysicsDirectBodyState3D::get_contact_collider(int p_contact_idx) const {
	ERR_FAIL_INDEX_V(p_contact_idx, (int)body->get_contact_count(), RID());
	return body->get_contact(p_contact_idx).collider_rid;
}

Vector3 JoltPhysicsDirectBodyState3D::get_contact_collider_position(int p_contact_idx) const {
	ERR_FAIL_INDEX_V(p_contact_idx, (int)body->get_contact_count(), Vector3());
	return body->get_contact(p_contact_idx).collider_position;
}

ObjectID JoltPhysicsDirectBodyState3D::get_contact_collider_id(int p_contact_idx) const {
	ERR_FAIL_INDEX_V(p_contact_idx, (int)body->get_contact_count(), ObjectID());
	return body->get_contact(p_contact_idx).collider_id;
}

Object *JoltPhysicsDirectBodyState3D::get_contact_collider_object(int p_contact_idx) const {
	ERR_FAIL_INDEX_V(p_contact_idx, (int)body->get_contact_count(), nullptr);
	return ObjectDB::get_instance(body->get_contact(p_contact_idx).collider_id);
}

int JoltPhysicsDirectBodyState3D::get_contact_collider_shape(int p_contact_idx) const {
	ERR_FAIL_INDEX_V(p_contact_idx, (int)body->get_contact_count(), 0);
	return body->get_contact(p_contact_idx).collider_shape_index;
}

Vector3 JoltPhysicsDirectBodyState3D::get_contact_collider_velocity_at_position(int p_contact_idx) const {
	ERR_FAIL_INDEX_V(p_contact_idx, (int)body->get_contact_count(), Vector3());
	return body->get_contact(p_contact_idx).collider_velocity;
}

real_t JoltPhysicsDirectBodyState3D::get_step() const {
	return (real_t)body->get_space()->get_last_step();
}

void JoltPhysicsDirectBodyState3D::integrate_forces() {
	const float step = (float)get_step();

	Vector3 linear_velocity = get_linear_velocity();
	Vector3 angular_velocity = get_angular_velocity();

	linear_velocity *= MAX(1.0f - (float)get_total_linear_damp() * step, 0.0f);
	angular_velocity *= MAX(1.0f - (float)get_total_angular_damp() * step, 0.0f);

	linear_velocity += get_total_gravity() * step;

	set_linear_velocity(linear_velocity);
	set_angular_velocity(angular_velocity);
}

PhysicsDirectSpaceState3D *JoltPhysicsDirectBodyState3D::get_space_state() {
	return body->get_space()->get_direct_state();
}

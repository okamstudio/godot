/**************************************************************************/
/*  vehicle_body_3d.cpp                                                   */
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

#include "vehicle_body_3d.h"

#define ROLLING_INFLUENCE_FIX

class btVehicleJacobianEntry {
public:
	Vector3 m_linearJointAxis;
	Vector3 m_aJ;
	Vector3 m_bJ;
	Vector3 m_0MinvJt;
	Vector3 m_1MinvJt;
	//Optimization: can be stored in the w/last component of one of the vectors
	real_t m_Adiag = 0.0;

	real_t getDiagonal() const { return m_Adiag; }

	btVehicleJacobianEntry() {}
	//constraint between two different rigidbodies
	btVehicleJacobianEntry(
			const Basis &world2A,
			const Basis &world2B,
			const Vector3 &rel_pos1,
			const Vector3 &rel_pos2,
			const Vector3 &jointAxis,
			const Vector3 &inertiaInvA,
			const real_t massInvA,
			const Vector3 &inertiaInvB,
			const real_t massInvB) :
			m_linearJointAxis(jointAxis) {
		m_aJ = world2A.xform(rel_pos1.cross(m_linearJointAxis));
		m_bJ = world2B.xform(rel_pos2.cross(-m_linearJointAxis));
		m_0MinvJt = inertiaInvA * m_aJ;
		m_1MinvJt = inertiaInvB * m_bJ;
		m_Adiag = massInvA + m_0MinvJt.dot(m_aJ) + massInvB + m_1MinvJt.dot(m_bJ);

		//btAssert(m_Adiag > real_t(0.0));
	}

	real_t getRelativeVelocity(const Vector3 &linvelA, const Vector3 &angvelA, const Vector3 &linvelB, const Vector3 &angvelB) {
		Vector3 linrel = linvelA - linvelB;
		Vector3 angvela = angvelA * m_aJ;
		Vector3 angvelb = angvelB * m_bJ;
		linrel *= m_linearJointAxis;
		angvela += angvelb;
		angvela += linrel;
		real_t rel_vel2 = angvela[0] + angvela[1] + angvela[2];
		return rel_vel2 + CMP_EPSILON;
	}
};

void VehicleWheel3D::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			VehicleBody3D *cb = Object::cast_to<VehicleBody3D>(get_parent());
			if (!cb) {
				return;
			}
			body = cb;
			local_xform = get_transform();
			cb->wheels.push_back(this);

			m_chassisConnectionPointCS = get_transform().origin;
			m_wheelDirectionCS = -get_transform().basis.get_column(Vector3::AXIS_Y).normalized();
			m_wheelAxleCS = get_transform().basis.get_column(Vector3::AXIS_X).normalized();
		} break;

		case NOTIFICATION_EXIT_TREE: {
			VehicleBody3D *cb = Object::cast_to<VehicleBody3D>(get_parent());
			if (!cb) {
				return;
			}
			cb->wheels.erase(this);
			body = nullptr;
		} break;
	}
}

PackedStringArray VehicleWheel3D::get_configuration_warnings() const {
	PackedStringArray warnings = Node3D::get_configuration_warnings();

	if (!Object::cast_to<VehicleBody3D>(get_parent())) {
		warnings.push_back(RTR("VehicleWheel3D serves to provide a wheel system to a VehicleBody3D. Please use it as a child of a VehicleBody3D."));
	}

	return warnings;
}

void VehicleWheel3D::_update(PhysicsDirectBodyState3D *s) {
	if (m_raycastInfo.m_isInContact) {
		real_t project = m_raycastInfo.m_contactNormalWS.dot(m_raycastInfo.m_wheelDirectionWS);
		Vector3 chassis_velocity_at_contactPoint;
		Vector3 relpos = m_raycastInfo.m_contactPointWS - s->get_transform().origin;

		chassis_velocity_at_contactPoint = s->get_linear_velocity() +
				(s->get_angular_velocity()).cross(relpos); // * mPos);

		real_t projVel = m_raycastInfo.m_contactNormalWS.dot(chassis_velocity_at_contactPoint);
		if (project >= real_t(-0.1)) {
			m_suspensionRelativeVelocity = real_t(0.0);
			m_clippedInvContactDotSuspension = real_t(1.0) / real_t(0.1);
		} else {
			real_t inv = real_t(-1.) / project;
			m_suspensionRelativeVelocity = projVel * inv;
			m_clippedInvContactDotSuspension = inv;
		}
	} else { // Not in contact : position wheel in a nice (rest length) position
		m_raycastInfo.m_suspensionLength = m_suspensionRestLength;
		m_suspensionRelativeVelocity = real_t(0.0);
		m_raycastInfo.m_contactNormalWS = -m_raycastInfo.m_wheelDirectionWS;
		m_clippedInvContactDotSuspension = real_t(1.0);
	}
}

void VehicleWheel3D::set_radius(real_t p_radius) {
	m_wheelRadius = p_radius;
	update_gizmos();
}

real_t VehicleWheel3D::get_radius() const {
	return m_wheelRadius;
}

void VehicleWheel3D::set_suspension_rest_length(real_t p_length) {
	m_suspensionRestLength = p_length;
	update_gizmos();
}

real_t VehicleWheel3D::get_suspension_rest_length() const {
	return m_suspensionRestLength;
}

void VehicleWheel3D::set_suspension_travel(real_t p_length) {
	m_maxSuspensionTravel = p_length;
}

real_t VehicleWheel3D::get_suspension_travel() const {
	return m_maxSuspensionTravel;
}

void VehicleWheel3D::set_suspension_stiffness(real_t p_value) {
	m_suspensionStiffness = p_value;
}

real_t VehicleWheel3D::get_suspension_stiffness() const {
	return m_suspensionStiffness;
}

void VehicleWheel3D::set_suspension_max_force(real_t p_value) {
	m_maxSuspensionForce = p_value;
}

real_t VehicleWheel3D::get_suspension_max_force() const {
	return m_maxSuspensionForce;
}

void VehicleWheel3D::set_damping_compression(real_t p_value) {
	m_wheelsDampingCompression = p_value;
}

real_t VehicleWheel3D::get_damping_compression() const {
	return m_wheelsDampingCompression;
}

void VehicleWheel3D::set_damping_relaxation(real_t p_value) {
	m_wheelsDampingRelaxation = p_value;
}

real_t VehicleWheel3D::get_damping_relaxation() const {
	return m_wheelsDampingRelaxation;
}

void VehicleWheel3D::set_friction_slip(real_t p_value) {
	m_frictionSlip = p_value;
}

real_t VehicleWheel3D::get_friction_slip() const {
	return m_frictionSlip;
}

void VehicleWheel3D::set_roll_influence(real_t p_value) {
	m_rollInfluence = p_value;
}

real_t VehicleWheel3D::get_roll_influence() const {
	return m_rollInfluence;
}

bool VehicleWheel3D::is_in_contact() const {
	return m_raycastInfo.m_isInContact;
}

Vector3 VehicleWheel3D::get_contact_point() const {
	return m_raycastInfo.m_contactPointWS;
}

Vector3 VehicleWheel3D::get_contact_normal() const {
	return m_raycastInfo.m_contactNormalWS;
}

Node3D *VehicleWheel3D::get_contact_body() const {
	return m_raycastInfo.m_groundObject;
}

void VehicleWheel3D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_radius", "length"), &VehicleWheel3D::set_radius);
	ClassDB::bind_method(D_METHOD("get_radius"), &VehicleWheel3D::get_radius);

	ClassDB::bind_method(D_METHOD("set_suspension_rest_length", "length"), &VehicleWheel3D::set_suspension_rest_length);
	ClassDB::bind_method(D_METHOD("get_suspension_rest_length"), &VehicleWheel3D::get_suspension_rest_length);

	ClassDB::bind_method(D_METHOD("set_suspension_travel", "length"), &VehicleWheel3D::set_suspension_travel);
	ClassDB::bind_method(D_METHOD("get_suspension_travel"), &VehicleWheel3D::get_suspension_travel);

	ClassDB::bind_method(D_METHOD("set_suspension_stiffness", "length"), &VehicleWheel3D::set_suspension_stiffness);
	ClassDB::bind_method(D_METHOD("get_suspension_stiffness"), &VehicleWheel3D::get_suspension_stiffness);

	ClassDB::bind_method(D_METHOD("set_suspension_max_force", "length"), &VehicleWheel3D::set_suspension_max_force);
	ClassDB::bind_method(D_METHOD("get_suspension_max_force"), &VehicleWheel3D::get_suspension_max_force);

	ClassDB::bind_method(D_METHOD("set_damping_compression", "length"), &VehicleWheel3D::set_damping_compression);
	ClassDB::bind_method(D_METHOD("get_damping_compression"), &VehicleWheel3D::get_damping_compression);

	ClassDB::bind_method(D_METHOD("set_damping_relaxation", "length"), &VehicleWheel3D::set_damping_relaxation);
	ClassDB::bind_method(D_METHOD("get_damping_relaxation"), &VehicleWheel3D::get_damping_relaxation);

	ClassDB::bind_method(D_METHOD("set_use_as_traction", "enable"), &VehicleWheel3D::set_use_as_traction);
	ClassDB::bind_method(D_METHOD("is_used_as_traction"), &VehicleWheel3D::is_used_as_traction);

	ClassDB::bind_method(D_METHOD("set_use_as_steering", "enable"), &VehicleWheel3D::set_use_as_steering);
	ClassDB::bind_method(D_METHOD("is_used_as_steering"), &VehicleWheel3D::is_used_as_steering);

	ClassDB::bind_method(D_METHOD("set_friction_slip", "length"), &VehicleWheel3D::set_friction_slip);
	ClassDB::bind_method(D_METHOD("get_friction_slip"), &VehicleWheel3D::get_friction_slip);

	ClassDB::bind_method(D_METHOD("is_in_contact"), &VehicleWheel3D::is_in_contact);
	ClassDB::bind_method(D_METHOD("get_contact_body"), &VehicleWheel3D::get_contact_body);
	ClassDB::bind_method(D_METHOD("get_contact_point"), &VehicleWheel3D::get_contact_point);
	ClassDB::bind_method(D_METHOD("get_contact_normal"), &VehicleWheel3D::get_contact_normal);

	ClassDB::bind_method(D_METHOD("set_roll_influence", "roll_influence"), &VehicleWheel3D::set_roll_influence);
	ClassDB::bind_method(D_METHOD("get_roll_influence"), &VehicleWheel3D::get_roll_influence);

	ClassDB::bind_method(D_METHOD("get_skidinfo"), &VehicleWheel3D::get_skidinfo);

	ClassDB::bind_method(D_METHOD("get_rpm"), &VehicleWheel3D::get_rpm);

	ClassDB::bind_method(D_METHOD("set_engine_force", "engine_force"), &VehicleWheel3D::set_engine_force);
	ClassDB::bind_method(D_METHOD("get_engine_force"), &VehicleWheel3D::get_engine_force);

	ClassDB::bind_method(D_METHOD("set_brake", "brake"), &VehicleWheel3D::set_brake);
	ClassDB::bind_method(D_METHOD("get_brake"), &VehicleWheel3D::get_brake);

	ClassDB::bind_method(D_METHOD("set_steering", "steering"), &VehicleWheel3D::set_steering);
	ClassDB::bind_method(D_METHOD("get_steering"), &VehicleWheel3D::get_steering);

	ADD_GROUP("Per-Wheel Motion", "");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "engine_force", PROPERTY_HINT_RANGE, U"-1024,1024,0.01,or_less,or_greater,suffix:kg\u22C5m/s\u00B2 (N)"), "set_engine_force", "get_engine_force");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "brake", PROPERTY_HINT_RANGE, U"-128,128,0.01,or_less,or_greater,suffix:kg\u22C5m/s\u00B2 (N)"), "set_brake", "get_brake");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "steering", PROPERTY_HINT_RANGE, "-180,180,0.01,radians_as_degrees"), "set_steering", "get_steering");
	ADD_GROUP("VehicleBody3D Motion", "");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "use_as_traction"), "set_use_as_traction", "is_used_as_traction");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "use_as_steering"), "set_use_as_steering", "is_used_as_steering");
	ADD_GROUP("Wheel", "wheel_");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "wheel_roll_influence"), "set_roll_influence", "get_roll_influence");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "wheel_radius", PROPERTY_HINT_NONE, "suffix:m"), "set_radius", "get_radius");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "wheel_rest_length", PROPERTY_HINT_NONE, "suffix:m"), "set_suspension_rest_length", "get_suspension_rest_length");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "wheel_friction_slip"), "set_friction_slip", "get_friction_slip");
	ADD_GROUP("Suspension", "suspension_");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "suspension_travel", PROPERTY_HINT_NONE, "suffix:m"), "set_suspension_travel", "get_suspension_travel");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "suspension_stiffness", PROPERTY_HINT_NONE, U"suffix:N/mm"), "set_suspension_stiffness", "get_suspension_stiffness");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "suspension_max_force", PROPERTY_HINT_NONE, U"suffix:kg\u22C5m/s\u00B2 (N)"), "set_suspension_max_force", "get_suspension_max_force");
	ADD_GROUP("Damping", "damping_");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "damping_compression", PROPERTY_HINT_NONE, U"suffix:N\u22C5s/mm"), "set_damping_compression", "get_damping_compression");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "damping_relaxation", PROPERTY_HINT_NONE, U"suffix:N\u22C5s/mm"), "set_damping_relaxation", "get_damping_relaxation");
}

void VehicleWheel3D::set_engine_force(real_t p_engine_force) {
	m_engineForce = p_engine_force;
}

real_t VehicleWheel3D::get_engine_force() const {
	return m_engineForce;
}

void VehicleWheel3D::set_brake(real_t p_brake) {
	m_brake = p_brake;
}

real_t VehicleWheel3D::get_brake() const {
	return m_brake;
}

void VehicleWheel3D::set_steering(real_t p_steering) {
	m_steering = p_steering;
}

real_t VehicleWheel3D::get_steering() const {
	return m_steering;
}

void VehicleWheel3D::set_use_as_traction(bool p_enable) {
	engine_traction = p_enable;
}

bool VehicleWheel3D::is_used_as_traction() const {
	return engine_traction;
}

void VehicleWheel3D::set_use_as_steering(bool p_enabled) {
	steers = p_enabled;
}

bool VehicleWheel3D::is_used_as_steering() const {
	return steers;
}

real_t VehicleWheel3D::get_skidinfo() const {
	return m_skidInfo;
}

real_t VehicleWheel3D::get_rpm() const {
	return m_rpm;
}

VehicleWheel3D::VehicleWheel3D() {
}

void VehicleBody3D::_update_wheel_transform(VehicleWheel3D &wheel, PhysicsDirectBodyState3D *s) {
	wheel.m_raycastInfo.m_isInContact = false;

	Transform3D chassisTrans = s->get_transform();
	/*
	if (interpolatedTransform && (getRigidBody()->getMotionState())) {
		getRigidBody()->getMotionState()->getWorldTransform(chassisTrans);
	}
	*/

	wheel.m_raycastInfo.m_hardPointWS = chassisTrans.xform(wheel.m_chassisConnectionPointCS);
	//wheel.m_raycastInfo.m_hardPointWS+=s->get_linear_velocity()*s->get_step();
	wheel.m_raycastInfo.m_wheelDirectionWS = chassisTrans.get_basis().xform(wheel.m_wheelDirectionCS).normalized();
	wheel.m_raycastInfo.m_wheelAxleWS = chassisTrans.get_basis().xform(wheel.m_wheelAxleCS).normalized();
}

void VehicleBody3D::_update_wheel(int p_idx, PhysicsDirectBodyState3D *s) {
	VehicleWheel3D &wheel = *wheels[p_idx];
	_update_wheel_transform(wheel, s);

	Vector3 up = -wheel.m_raycastInfo.m_wheelDirectionWS;
	const Vector3 &right = wheel.m_raycastInfo.m_wheelAxleWS;
	Vector3 fwd = up.cross(right);
	fwd = fwd.normalized();

	Basis steeringMat(up, wheel.m_steering);

	Basis rotatingMat(right, wheel.m_rotation);

	Basis basis2(
			right[0], up[0], fwd[0],
			right[1], up[1], fwd[1],
			right[2], up[2], fwd[2]);

	wheel.m_worldTransform.set_basis(steeringMat * rotatingMat * basis2);
	//wheel.m_worldTransform.set_basis(basis2 * (steeringMat * rotatingMat));
	wheel.m_worldTransform.set_origin(
			wheel.m_raycastInfo.m_hardPointWS + wheel.m_raycastInfo.m_wheelDirectionWS * wheel.m_raycastInfo.m_suspensionLength);
}

real_t VehicleBody3D::_ray_cast(int p_idx, PhysicsDirectBodyState3D *s) {
	VehicleWheel3D &wheel = *wheels[p_idx];

	_update_wheel_transform(wheel, s);

	real_t depth = -1;

	real_t raylen = wheel.m_suspensionRestLength + wheel.m_wheelRadius;

	Vector3 rayvector = wheel.m_raycastInfo.m_wheelDirectionWS * (raylen);
	Vector3 source = wheel.m_raycastInfo.m_hardPointWS;
	wheel.m_raycastInfo.m_contactPointWS = source + rayvector;
	const Vector3 &target = wheel.m_raycastInfo.m_contactPointWS;
	source -= wheel.m_wheelRadius * wheel.m_raycastInfo.m_wheelDirectionWS;

	real_t param = real_t(0.);

	PhysicsDirectSpaceState3D::RayResult rr;

	PhysicsDirectSpaceState3D *ss = s->get_space_state();

	PhysicsDirectSpaceState3D::RayParameters ray_params;
	ray_params.from = source;
	ray_params.to = target;
	ray_params.exclude = exclude;
	ray_params.collision_mask = get_collision_mask();

	wheel.m_raycastInfo.m_groundObject = nullptr;
	bool col = ss->intersect_ray(ray_params, rr);

	if (col) {
		param = source.distance_to(rr.position) / source.distance_to(target);
		depth = raylen * param;
		wheel.m_raycastInfo.m_contactNormalWS = rr.normal;

		wheel.m_raycastInfo.m_isInContact = true;
		if (rr.collider) {
			wheel.m_raycastInfo.m_groundObject = Object::cast_to<PhysicsBody3D>(rr.collider);
		}

		real_t hitDistance = param * raylen;
		wheel.m_raycastInfo.m_suspensionLength = hitDistance - wheel.m_wheelRadius;
		//clamp on max suspension travel

		real_t minSuspensionLength = wheel.m_suspensionRestLength - wheel.m_maxSuspensionTravel;
		real_t maxSuspensionLength = wheel.m_suspensionRestLength + wheel.m_maxSuspensionTravel;
		if (wheel.m_raycastInfo.m_suspensionLength < minSuspensionLength) {
			wheel.m_raycastInfo.m_suspensionLength = minSuspensionLength;
		}
		if (wheel.m_raycastInfo.m_suspensionLength > maxSuspensionLength) {
			wheel.m_raycastInfo.m_suspensionLength = maxSuspensionLength;
		}

		wheel.m_raycastInfo.m_contactPointWS = rr.position;

		real_t denominator = wheel.m_raycastInfo.m_contactNormalWS.dot(wheel.m_raycastInfo.m_wheelDirectionWS);

		Vector3 chassis_velocity_at_contactPoint;
		//Vector3 relpos = wheel.m_raycastInfo.m_contactPointWS-getRigidBody()->getCenterOfMassPosition();

		//chassis_velocity_at_contactPoint = getRigidBody()->getVelocityInLocalPoint(relpos);

		chassis_velocity_at_contactPoint = s->get_linear_velocity() +
				(s->get_angular_velocity()).cross(wheel.m_raycastInfo.m_contactPointWS - s->get_transform().origin); // * mPos);

		real_t projVel = wheel.m_raycastInfo.m_contactNormalWS.dot(chassis_velocity_at_contactPoint);

		if (denominator >= real_t(-0.1)) {
			wheel.m_suspensionRelativeVelocity = real_t(0.0);
			wheel.m_clippedInvContactDotSuspension = real_t(1.0) / real_t(0.1);
		} else {
			real_t inv = real_t(-1.) / denominator;
			wheel.m_suspensionRelativeVelocity = projVel * inv;
			wheel.m_clippedInvContactDotSuspension = inv;
		}

	} else {
		wheel.m_raycastInfo.m_isInContact = false;
		//put wheel info as in rest position
		wheel.m_raycastInfo.m_suspensionLength = wheel.m_suspensionRestLength;
		wheel.m_suspensionRelativeVelocity = real_t(0.0);
		wheel.m_raycastInfo.m_contactNormalWS = -wheel.m_raycastInfo.m_wheelDirectionWS;
		wheel.m_clippedInvContactDotSuspension = real_t(1.0);
	}

	return depth;
}

void VehicleBody3D::_update_suspension(PhysicsDirectBodyState3D *s) {
	real_t chassisMass = get_mass();

	for (int w_it = 0; w_it < wheels.size(); w_it++) {
		VehicleWheel3D &wheel_info = *wheels[w_it];

		if (wheel_info.m_raycastInfo.m_isInContact) {
			real_t force;
			//Spring
			{
				real_t susp_length = wheel_info.m_suspensionRestLength;
				real_t current_length = wheel_info.m_raycastInfo.m_suspensionLength;

				real_t length_diff = (susp_length - current_length);

				force = wheel_info.m_suspensionStiffness * length_diff * wheel_info.m_clippedInvContactDotSuspension;
			}

			// Damper
			{
				real_t projected_rel_vel = wheel_info.m_suspensionRelativeVelocity;
				{
					real_t susp_damping;
					if (projected_rel_vel < real_t(0.0)) {
						susp_damping = wheel_info.m_wheelsDampingCompression;
					} else {
						susp_damping = wheel_info.m_wheelsDampingRelaxation;
					}
					force -= susp_damping * projected_rel_vel;
				}
			}

			// RESULT
			wheel_info.m_wheelsSuspensionForce = force * chassisMass;
			if (wheel_info.m_wheelsSuspensionForce < real_t(0.)) {
				wheel_info.m_wheelsSuspensionForce = real_t(0.);
			}
		} else {
			wheel_info.m_wheelsSuspensionForce = real_t(0.0);
		}
	}
}

//bilateral constraint between two dynamic objects
void VehicleBody3D::_resolve_single_bilateral(PhysicsDirectBodyState3D *s, const Vector3 &pos1,
		PhysicsBody3D *body2, const Vector3 &pos2, const Vector3 &normal, real_t &impulse, const real_t p_rollInfluence) {
	real_t normalLenSqr = normal.length_squared();
	//ERR_FAIL_COND( normalLenSqr < real_t(1.1));

	if (normalLenSqr > real_t(1.1)) {
		impulse = real_t(0.);
		return;
	}

	Vector3 rel_pos1 = pos1 - s->get_transform().origin;
	Vector3 rel_pos2;
	if (body2) {
		rel_pos2 = pos2 - body2->get_global_transform().origin;
	}
	// This Jacobian entry could be reused for all iterations.

	Vector3 vel1 = s->get_linear_velocity() + (s->get_angular_velocity()).cross(rel_pos1); // * mPos);
	Vector3 vel2;

	if (body2) {
		vel2 = body2->get_linear_velocity() + body2->get_angular_velocity().cross(rel_pos2);
	}

	Vector3 vel = vel1 - vel2;

	Basis b2trans;
	real_t b2invmass = 0;
	Vector3 b2lv;
	Vector3 b2av;
	Vector3 b2invinertia; //todo

	if (body2) {
		b2trans = body2->get_global_transform().basis.transposed();
		b2invmass = body2->get_inverse_mass();
		b2lv = body2->get_linear_velocity();
		b2av = body2->get_angular_velocity();
	}

	btVehicleJacobianEntry jac(s->get_transform().basis.transposed(),
			b2trans,
			rel_pos1,
			rel_pos2,
			normal,
			s->get_inverse_inertia_tensor().get_main_diagonal(),
			1.0 / get_mass(),
			b2invinertia,
			b2invmass);

	// FIXME: rel_vel assignment here is overwritten by the following assignment.
	// What seems to be intended in the next assignment is: rel_vel = normal.dot(rel_vel);
	// Investigate why.
	real_t rel_vel = jac.getRelativeVelocity(
			s->get_linear_velocity(),
			s->get_transform().basis.transposed().xform(s->get_angular_velocity()),
			b2lv,
			b2trans.xform(b2av));

	rel_vel = normal.dot(vel);

	// !BAS! We had this set to 0.4, in bullet its 0.2
	real_t contactDamping = real_t(0.2);

	if (p_rollInfluence > 0.0) {
		// !BAS! But seeing we apply this frame by frame, makes more sense to me to make this time based
		// keeping in mind our anti roll factor if it is set
		contactDamping = MIN(contactDamping, s->get_step() / p_rollInfluence);
	}

#define ONLY_USE_LINEAR_MASS
#ifdef ONLY_USE_LINEAR_MASS
	real_t massTerm = real_t(1.) / ((1.0 / get_mass()) + b2invmass);
	impulse = -contactDamping * rel_vel * massTerm;
#else
	real_t velocityImpulse = -contactDamping * rel_vel * jacDiagABInv;
	impulse = velocityImpulse;
#endif
}

VehicleBody3D::btVehicleWheelContactPoint::btVehicleWheelContactPoint(PhysicsDirectBodyState3D *s, PhysicsBody3D *body1, const Vector3 &frictionPosWorld, const Vector3 &frictionDirectionWorld, real_t maxImpulse) :
		m_s(s),
		m_body1(body1),
		m_frictionPositionWorld(frictionPosWorld),
		m_frictionDirectionWorld(frictionDirectionWorld),
		m_maxImpulse(maxImpulse) {
	real_t denom0 = 0;
	real_t denom1 = 0;

	{
		Vector3 r0 = frictionPosWorld - s->get_transform().origin;
		Vector3 c0 = (r0).cross(frictionDirectionWorld);
		Vector3 vec = s->get_inverse_inertia_tensor().xform_inv(c0).cross(r0);
		denom0 = s->get_inverse_mass() + frictionDirectionWorld.dot(vec);
	}

	/* TODO: Why is this code unused?
	if (body1) {
		Vector3 r0 = frictionPosWorld - body1->get_global_transform().origin;
		Vector3 c0 = (r0).cross(frictionDirectionWorld);
		Vector3 vec = s->get_inverse_inertia_tensor().xform_inv(c0).cross(r0);
		//denom1= body1->get_inverse_mass() + frictionDirectionWorld.dot(vec);

	}
	*/

	real_t relaxation = 1.f;
	m_jacDiagABInv = relaxation / (denom0 + denom1);
}

real_t VehicleBody3D::_calc_rolling_friction(btVehicleWheelContactPoint &contactPoint) {
	real_t j1 = 0.f;

	const Vector3 &contactPosWorld = contactPoint.m_frictionPositionWorld;

	Vector3 rel_pos1 = contactPosWorld - contactPoint.m_s->get_transform().origin;
	Vector3 rel_pos2;
	if (contactPoint.m_body1) {
		rel_pos2 = contactPosWorld - contactPoint.m_body1->get_global_transform().origin;
	}

	real_t maxImpulse = contactPoint.m_maxImpulse;

	Vector3 vel1 = contactPoint.m_s->get_linear_velocity() + (contactPoint.m_s->get_angular_velocity()).cross(rel_pos1); // * mPos);

	Vector3 vel2;
	if (contactPoint.m_body1) {
		vel2 = contactPoint.m_body1->get_linear_velocity() + contactPoint.m_body1->get_angular_velocity().cross(rel_pos2);
	}

	Vector3 vel = vel1 - vel2;

	real_t vrel = contactPoint.m_frictionDirectionWorld.dot(vel);

	// calculate j that moves us to zero relative velocity
	j1 = -vrel * contactPoint.m_jacDiagABInv;

	return CLAMP(j1, -maxImpulse, maxImpulse);
}

static const real_t sideFrictionStiffness2 = real_t(1.0);
void VehicleBody3D::_update_friction(PhysicsDirectBodyState3D *s) {
	//calculate the impulse, so that the wheels don't move sidewards
	int numWheel = wheels.size();
	if (!numWheel) {
		return;
	}

	m_forwardWS.resize(numWheel);
	m_axle.resize(numWheel);
	m_forwardImpulse.resize(numWheel);
	m_sideImpulse.resize(numWheel);

	//collapse all those loops into one!
	for (int i = 0; i < wheels.size(); i++) {
		m_sideImpulse.write[i] = real_t(0.);
		m_forwardImpulse.write[i] = real_t(0.);
	}

	{
		for (int i = 0; i < wheels.size(); i++) {
			VehicleWheel3D &wheelInfo = *wheels[i];

			if (wheelInfo.m_raycastInfo.m_isInContact) {
				//const btTransform& wheelTrans = getWheelTransformWS( i );

				Basis wheelBasis0 = wheelInfo.m_worldTransform.basis; //get_global_transform().basis;

				m_axle.write[i] = wheelBasis0.get_column(Vector3::AXIS_X);
				//m_axle[i] = wheelInfo.m_raycastInfo.m_wheelAxleWS;

				const Vector3 &surfNormalWS = wheelInfo.m_raycastInfo.m_contactNormalWS;
				real_t proj = m_axle[i].dot(surfNormalWS);
				m_axle.write[i] -= surfNormalWS * proj;
				m_axle.write[i] = m_axle[i].normalized();

				m_forwardWS.write[i] = surfNormalWS.cross(m_axle[i]);
				m_forwardWS.write[i].normalize();

				_resolve_single_bilateral(s, wheelInfo.m_raycastInfo.m_contactPointWS,
						wheelInfo.m_raycastInfo.m_groundObject, wheelInfo.m_raycastInfo.m_contactPointWS,
						m_axle[i], m_sideImpulse.write[i], wheelInfo.m_rollInfluence);

				m_sideImpulse.write[i] *= sideFrictionStiffness2;
			}
		}
	}

	real_t sideFactor = real_t(1.);
	real_t fwdFactor = 0.5;

	bool sliding = false;
	{
		for (int wheel = 0; wheel < wheels.size(); wheel++) {
			VehicleWheel3D &wheelInfo = *wheels[wheel];

			//class btRigidBody* groundObject = (class btRigidBody*) wheelInfo.m_raycastInfo.m_groundObject;

			real_t rollingFriction = 0.f;

			if (wheelInfo.m_raycastInfo.m_isInContact) {
				if (wheelInfo.m_engineForce != 0.f) {
					rollingFriction = -wheelInfo.m_engineForce * s->get_step();
				} else {
					real_t defaultRollingFrictionImpulse = 0.f;
					real_t maxImpulse = wheelInfo.m_brake ? wheelInfo.m_brake : defaultRollingFrictionImpulse;
					btVehicleWheelContactPoint contactPt(s, wheelInfo.m_raycastInfo.m_groundObject, wheelInfo.m_raycastInfo.m_contactPointWS, m_forwardWS[wheel], maxImpulse);
					rollingFriction = _calc_rolling_friction(contactPt);
				}
			}

			//switch between active rolling (throttle), braking and non-active rolling friction (no throttle/break)

			m_forwardImpulse.write[wheel] = real_t(0.);
			wheelInfo.m_skidInfo = real_t(1.);

			if (wheelInfo.m_raycastInfo.m_isInContact) {
				wheelInfo.m_skidInfo = real_t(1.);

				real_t maximp = wheelInfo.m_wheelsSuspensionForce * s->get_step() * wheelInfo.m_frictionSlip;
				real_t maximpSide = maximp;

				real_t maximpSquared = maximp * maximpSide;

				m_forwardImpulse.write[wheel] = rollingFriction; //wheelInfo.m_engineForce* timeStep;

				real_t x = (m_forwardImpulse[wheel]) * fwdFactor;
				real_t y = (m_sideImpulse[wheel]) * sideFactor;

				real_t impulseSquared = (x * x + y * y);

				if (impulseSquared > maximpSquared) {
					sliding = true;

					real_t factor = maximp / Math::sqrt(impulseSquared);

					wheelInfo.m_skidInfo *= factor;
				}
			}
		}
	}

	if (sliding) {
		for (int wheel = 0; wheel < wheels.size(); wheel++) {
			if (m_sideImpulse[wheel] != real_t(0.)) {
				if (wheels[wheel]->m_skidInfo < real_t(1.)) {
					m_forwardImpulse.write[wheel] *= wheels[wheel]->m_skidInfo;
					m_sideImpulse.write[wheel] *= wheels[wheel]->m_skidInfo;
				}
			}
		}
	}

	// apply the impulses
	{
		for (int wheel = 0; wheel < wheels.size(); wheel++) {
			VehicleWheel3D &wheelInfo = *wheels[wheel];

			Vector3 rel_pos = wheelInfo.m_raycastInfo.m_contactPointWS -
					s->get_transform().origin;

			if (m_forwardImpulse[wheel] != real_t(0.)) {
				s->apply_impulse(m_forwardWS[wheel] * (m_forwardImpulse[wheel]), rel_pos);
			}
			if (m_sideImpulse[wheel] != real_t(0.)) {
				PhysicsBody3D *groundObject = wheelInfo.m_raycastInfo.m_groundObject;

				Vector3 rel_pos2;
				if (groundObject) {
					rel_pos2 = wheelInfo.m_raycastInfo.m_contactPointWS - groundObject->get_global_transform().origin;
				}

				Vector3 sideImp = m_axle[wheel] * m_sideImpulse[wheel];

#if defined ROLLING_INFLUENCE_FIX // fix. It only worked if car's up was along Y - VT.
				Vector3 vChassisWorldUp = s->get_transform().basis.transposed()[1]; //getRigidBody()->getCenterOfMassTransform3D().getBasis().getColumn(m_indexUpAxis);
				rel_pos -= vChassisWorldUp * (vChassisWorldUp.dot(rel_pos) * (1.f - wheelInfo.m_rollInfluence));
#else
				rel_pos[1] *= wheelInfo.m_rollInfluence; //?
#endif
				s->apply_impulse(sideImp, rel_pos);

				//apply friction impulse on the ground
				//todo
				//groundObject->applyImpulse(-sideImp,rel_pos2);
			}
		}
	}
}

void VehicleBody3D::_body_state_changed(PhysicsDirectBodyState3D *p_state) {
	RigidBody3D::_body_state_changed(p_state);

	real_t step = p_state->get_step();

	for (int i = 0; i < wheels.size(); i++) {
		_update_wheel(i, p_state);
	}

	for (int i = 0; i < wheels.size(); i++) {
		_ray_cast(i, p_state);
		wheels[i]->set_transform(p_state->get_transform().inverse() * wheels[i]->m_worldTransform);
	}

	_update_suspension(p_state);

	for (int i = 0; i < wheels.size(); i++) {
		//apply suspension force
		VehicleWheel3D &wheel = *wheels[i];

		real_t suspensionForce = wheel.m_wheelsSuspensionForce;

		if (suspensionForce > wheel.m_maxSuspensionForce) {
			suspensionForce = wheel.m_maxSuspensionForce;
		}
		Vector3 impulse = wheel.m_raycastInfo.m_contactNormalWS * suspensionForce * step;
		Vector3 relative_position = wheel.m_raycastInfo.m_contactPointWS - p_state->get_transform().origin;

		p_state->apply_impulse(impulse, relative_position);
	}

	_update_friction(p_state);

	for (int i = 0; i < wheels.size(); i++) {
		VehicleWheel3D &wheel = *wheels[i];
		Vector3 relpos = wheel.m_raycastInfo.m_hardPointWS - p_state->get_transform().origin;
		Vector3 vel = p_state->get_linear_velocity() + (p_state->get_angular_velocity()).cross(relpos); // * mPos);

		if (wheel.m_raycastInfo.m_isInContact) {
			const Transform3D &chassisWorldTransform = p_state->get_transform();

			Vector3 fwd(
					chassisWorldTransform.basis[0][Vector3::AXIS_Z],
					chassisWorldTransform.basis[1][Vector3::AXIS_Z],
					chassisWorldTransform.basis[2][Vector3::AXIS_Z]);

			real_t proj = fwd.dot(wheel.m_raycastInfo.m_contactNormalWS);
			fwd -= wheel.m_raycastInfo.m_contactNormalWS * proj;

			real_t proj2 = fwd.dot(vel);

			wheel.m_deltaRotation = (proj2 * step) / (wheel.m_wheelRadius);
		}

		wheel.m_rotation += wheel.m_deltaRotation;
		wheel.m_rpm = ((wheel.m_deltaRotation / step) * 60) / Math_TAU;

		wheel.m_deltaRotation *= real_t(0.99); //damping of rotation when not in contact
	}
}

void VehicleBody3D::set_engine_force(real_t p_engine_force) {
	engine_force = p_engine_force;
	for (int i = 0; i < wheels.size(); i++) {
		VehicleWheel3D &wheelInfo = *wheels[i];
		if (wheelInfo.engine_traction) {
			wheelInfo.m_engineForce = p_engine_force;
		}
	}
}

real_t VehicleBody3D::get_engine_force() const {
	return engine_force;
}

void VehicleBody3D::set_brake(real_t p_brake) {
	brake = p_brake;
	for (int i = 0; i < wheels.size(); i++) {
		VehicleWheel3D &wheelInfo = *wheels[i];
		wheelInfo.m_brake = p_brake;
	}
}

real_t VehicleBody3D::get_brake() const {
	return brake;
}

void VehicleBody3D::set_steering(real_t p_steering) {
	m_steeringValue = p_steering;
	for (int i = 0; i < wheels.size(); i++) {
		VehicleWheel3D &wheelInfo = *wheels[i];
		if (wheelInfo.steers) {
			wheelInfo.m_steering = p_steering;
		}
	}
}

real_t VehicleBody3D::get_steering() const {
	return m_steeringValue;
}

void VehicleBody3D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_engine_force", "engine_force"), &VehicleBody3D::set_engine_force);
	ClassDB::bind_method(D_METHOD("get_engine_force"), &VehicleBody3D::get_engine_force);

	ClassDB::bind_method(D_METHOD("set_brake", "brake"), &VehicleBody3D::set_brake);
	ClassDB::bind_method(D_METHOD("get_brake"), &VehicleBody3D::get_brake);

	ClassDB::bind_method(D_METHOD("set_steering", "steering"), &VehicleBody3D::set_steering);
	ClassDB::bind_method(D_METHOD("get_steering"), &VehicleBody3D::get_steering);

	ADD_GROUP("Motion", "");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "engine_force", PROPERTY_HINT_RANGE, U"-1024,1024,0.01,or_less,or_greater,suffix:kg\u22C5m/s\u00B2 (N)"), "set_engine_force", "get_engine_force");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "brake", PROPERTY_HINT_RANGE, U"-128,128,0.01,or_less,or_greater,suffix:kg\u22C5m/s\u00B2 (N)"), "set_brake", "get_brake");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "steering", PROPERTY_HINT_RANGE, "-180,180,0.01,radians_as_degrees"), "set_steering", "get_steering");
}

VehicleBody3D::VehicleBody3D() {
	exclude.insert(get_rid());
	set_mass(40);
}

/**************************************************************************/
/*  godot_joints_2d.cpp                                                   */
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

#include "godot_joints_2d.h"

#include "godot_space_2d.h"

//based on chipmunk joint constraints

/* Copyright (c) 2007 Scott Lembcke
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

void GodotJoint2D::copy_settings_from(GodotJoint2D *p_joint) {
	set_self(p_joint->get_self());
	set_max_force(p_joint->get_max_force());
	set_bias(p_joint->get_bias());
	set_max_bias(p_joint->get_max_bias());
	disable_collisions_between_bodies(p_joint->is_disabled_collisions_between_bodies());
}

static inline real_t k_scalar(GodotBody2D *a, GodotBody2D *b, const Vector2 &rA, const Vector2 &rB, const Vector2 &n) {
	real_t value = 0.0;

	{
		value += a->get_inv_mass();
		real_t rcn = (rA - a->get_center_of_mass()).cross(n);
		value += a->get_inv_inertia() * rcn * rcn;
	}

	if (b) {
		value += b->get_inv_mass();
		real_t rcn = (rB - b->get_center_of_mass()).cross(n);
		value += b->get_inv_inertia() * rcn * rcn;
	}

	return value;
}

static inline Vector2
relative_velocity(GodotBody2D *a, GodotBody2D *b, Vector2 rA, Vector2 rB) {
	Vector2 sum = a->get_linear_velocity() - (rA - a->get_center_of_mass()).orthogonal() * a->get_angular_velocity();
	if (b) {
		return (b->get_linear_velocity() - (rB - b->get_center_of_mass()).orthogonal() * b->get_angular_velocity()) - sum;
	} else {
		return -sum;
	}
}

static inline real_t
normal_relative_velocity(GodotBody2D *a, GodotBody2D *b, Vector2 rA, Vector2 rB, Vector2 n) {
	return relative_velocity(a, b, rA, rB).dot(n);
}

bool GodotPinJoint2D::setup(real_t p_step) {
	dynamic_A = (A->get_mode() > PhysicsServer2D::BODY_MODE_KINEMATIC);
	dynamic_B = (B->get_mode() > PhysicsServer2D::BODY_MODE_KINEMATIC);

	if (!dynamic_A && !dynamic_B) {
		return false;
	}

	GodotSpace2D *space = A->get_space();
	ERR_FAIL_NULL_V(space, false);

	rA = A->get_transform().basis_xform(anchor_A);
	rB = B ? B->get_transform().basis_xform(anchor_B) : anchor_B;

	real_t B_inv_mass = B ? B->get_inv_mass() : 0.0;

	Transform2D K1;
	K1[0].x = A->get_inv_mass() + B_inv_mass;
	K1[1].x = 0.0f;
	K1[0].y = 0.0f;
	K1[1].y = A->get_inv_mass() + B_inv_mass;

	Vector2 r1 = rA - A->get_center_of_mass();

	Transform2D K2;
	K2[0].x = A->get_inv_inertia() * r1.y * r1.y;
	K2[1].x = -A->get_inv_inertia() * r1.x * r1.y;
	K2[0].y = -A->get_inv_inertia() * r1.x * r1.y;
	K2[1].y = A->get_inv_inertia() * r1.x * r1.x;

	Transform2D K;
	K[0] = K1[0] + K2[0];
	K[1] = K1[1] + K2[1];

	if (B) {
		Vector2 r2 = rB - B->get_center_of_mass();

		Transform2D K3;
		K3[0].x = B->get_inv_inertia() * r2.y * r2.y;
		K3[1].x = -B->get_inv_inertia() * r2.x * r2.y;
		K3[0].y = -B->get_inv_inertia() * r2.x * r2.y;
		K3[1].y = B->get_inv_inertia() * r2.x * r2.x;

		K[0] += K3[0];
		K[1] += K3[1];
	}

	K[0].x += softness;
	K[1].y += softness;

	M = K.affine_inverse();

	Vector2 gA = rA + A->get_transform().get_origin();
	Vector2 gB = B ? rB + B->get_transform().get_origin() : rB;

	Vector2 delta = gB - gA;

	bias = delta * -(get_bias() == 0 ? space->get_constraint_bias() : get_bias()) * (1.0 / p_step);

	// Compute max impulse.
	jn_max = get_max_force() * p_step;

	return true;
}

inline Vector2 custom_cross(const Vector2 &p_vec, real_t p_other) {
	return Vector2(p_other * p_vec.y, -p_other * p_vec.x);
}

bool GodotPinJoint2D::pre_solve(real_t p_step) {
	// Apply accumulated impulse.
	if (dynamic_A) {
		A->apply_impulse(-P, rA);
	}
	if (B && dynamic_B) {
		B->apply_impulse(P, rB);
	}
	// Angle limits joint pre_solve step taken from https://github.com/slembcke/Chipmunk2D/blob/d0239ef4599b3688a5a336373f7d0a68426414ba/src/cpRotaryLimitJoint.c
	real_t i_sum_local = A->get_inv_inertia();
	if (B) {
		i_sum_local += B->get_inv_inertia();
	}
	i_sum = 1.0 / (i_sum_local);
	if (angular_limit_enabled && B) {
		Vector2 diff_vector = B->get_transform().get_origin() - A->get_transform().get_origin();
		diff_vector = diff_vector.rotated(-initial_angle);
		real_t dist = diff_vector.angle();
		real_t pdist = 0.0;
		if (dist > angular_limit_upper) {
			pdist = dist - angular_limit_upper;
		} else if (dist < angular_limit_lower) {
			pdist = dist - angular_limit_lower;
		}
		real_t error_bias = Math::pow(1.0 - 0.15, 60.0);
		// Calculate bias velocity.
		bias_velocity = -CLAMP((-1.0 - Math::pow(error_bias, p_step)) * pdist / p_step, -get_max_bias(), get_max_bias());
		// If the bias velocity is 0, the joint is not at a limit.
		if (bias_velocity >= -CMP_EPSILON && bias_velocity <= CMP_EPSILON) {
			j_acc = 0;
			is_joint_at_limit = false;
		} else {
			is_joint_at_limit = true;
		}
	} else {
		bias_velocity = 0.0;
	}

	return true;
}

void GodotPinJoint2D::solve(real_t p_step) {
	// Compute relative velocity.
	Vector2 vA = A->get_linear_velocity() - custom_cross(rA - A->get_center_of_mass(), A->get_angular_velocity());

	Vector2 rel_vel;
	if (B) {
		rel_vel = B->get_linear_velocity() - custom_cross(rB - B->get_center_of_mass(), B->get_angular_velocity()) - vA;
	} else {
		rel_vel = -vA;
	}
	// Angle limits joint solve step taken from https://github.com/slembcke/Chipmunk2D/blob/d0239ef4599b3688a5a336373f7d0a68426414ba/src/cpRotaryLimitJoint.c
	if ((angular_limit_enabled || motor_enabled) && B) {
		// Compute relative rotational velocity.
		real_t wr = B->get_angular_velocity() - A->get_angular_velocity();
		// Motor solve part taken from https://github.com/slembcke/Chipmunk2D/blob/d0239ef4599b3688a5a336373f7d0a68426414ba/src/cpSimpleMotor.c
		if (motor_enabled) {
			wr -= motor_target_velocity;
		}
		real_t j_max = jn_max;

		// Compute normal impulse.
		real_t j = -(bias_velocity + wr) * i_sum;
		real_t j_old = j_acc;
		// Only enable the limits if we have to.
		if (angular_limit_enabled && is_joint_at_limit) {
			if (bias_velocity < 0.0) {
				j_acc = CLAMP(j_old + j, 0.0, j_max);
			} else {
				j_acc = CLAMP(j_old + j, -j_max, 0.0);
			}
		} else {
			j_acc = CLAMP(j_old + j, -j_max, j_max);
		}
		j = j_acc - j_old;
		A->apply_torque_impulse(-j * A->get_inv_inertia());
		B->apply_torque_impulse(j * B->get_inv_inertia());
	}

	Vector2 impulse = M.basis_xform(bias - rel_vel - Vector2(softness, softness) * P);

	if (dynamic_A) {
		A->apply_impulse(-impulse, rA);
	}
	if (B && dynamic_B) {
		B->apply_impulse(impulse, rB);
	}

	P += impulse;
}

void GodotPinJoint2D::set_param(PhysicsServer2D::PinJointParam p_param, real_t p_value) {
	switch (p_param) {
		case PhysicsServer2D::PIN_JOINT_SOFTNESS: {
			softness = p_value;
		} break;
		case PhysicsServer2D::PIN_JOINT_LIMIT_UPPER: {
			angular_limit_upper = p_value;
		} break;
		case PhysicsServer2D::PIN_JOINT_LIMIT_LOWER: {
			angular_limit_lower = p_value;
		} break;
		case PhysicsServer2D::PIN_JOINT_MOTOR_TARGET_VELOCITY: {
			motor_target_velocity = p_value;
		} break;
	}
}

real_t GodotPinJoint2D::get_param(PhysicsServer2D::PinJointParam p_param) const {
	switch (p_param) {
		case PhysicsServer2D::PIN_JOINT_SOFTNESS: {
			return softness;
		}
		case PhysicsServer2D::PIN_JOINT_LIMIT_UPPER: {
			return angular_limit_upper;
		}
		case PhysicsServer2D::PIN_JOINT_LIMIT_LOWER: {
			return angular_limit_lower;
		}
		case PhysicsServer2D::PIN_JOINT_MOTOR_TARGET_VELOCITY: {
			return motor_target_velocity;
		}
	}
	ERR_FAIL_V(0);
}

void GodotPinJoint2D::set_flag(PhysicsServer2D::PinJointFlag p_flag, bool p_enabled) {
	switch (p_flag) {
		case PhysicsServer2D::PIN_JOINT_FLAG_ANGULAR_LIMIT_ENABLED: {
			angular_limit_enabled = p_enabled;
		} break;
		case PhysicsServer2D::PIN_JOINT_FLAG_MOTOR_ENABLED: {
			motor_enabled = p_enabled;
		} break;
	}
}

bool GodotPinJoint2D::get_flag(PhysicsServer2D::PinJointFlag p_flag) const {
	switch (p_flag) {
		case PhysicsServer2D::PIN_JOINT_FLAG_ANGULAR_LIMIT_ENABLED: {
			return angular_limit_enabled;
		}
		case PhysicsServer2D::PIN_JOINT_FLAG_MOTOR_ENABLED: {
			return motor_enabled;
		}
	}
	ERR_FAIL_V(false);
}

GodotPinJoint2D::GodotPinJoint2D(const Vector2 &p_pos, GodotBody2D *p_body_a, GodotBody2D *p_body_b) :
		GodotJoint2D(_arr, p_body_b ? 2 : 1) {
	A = p_body_a;
	B = p_body_b;
	anchor_A = p_body_a->get_inv_transform().xform(p_pos);
	anchor_B = p_body_b ? p_body_b->get_inv_transform().xform(p_pos) : p_pos;

	p_body_a->add_constraint(this, 0);
	if (p_body_b) {
		p_body_b->add_constraint(this, 1);
		initial_angle = A->get_transform().get_origin().angle_to_point(B->get_transform().get_origin());
	}
}

//////////////////////////////////////////////
//////////////////////////////////////////////
//////////////////////////////////////////////

static inline void
k_tensor(GodotBody2D *a, GodotBody2D *b, Vector2 r1, Vector2 r2, Vector2 *k1, Vector2 *k2) {
	// calculate mass matrix
	// If I wasn't lazy and wrote a proper matrix class, this wouldn't be so gross...
	real_t k11, k12, k21, k22;
	real_t m_sum = a->get_inv_mass() + b->get_inv_mass();

	// start with I*m_sum
	k11 = m_sum;
	k12 = 0.0f;
	k21 = 0.0f;
	k22 = m_sum;

	r1 -= a->get_center_of_mass();
	r2 -= b->get_center_of_mass();

	// add the influence from r1
	real_t a_i_inv = a->get_inv_inertia();
	real_t r1xsq = r1.x * r1.x * a_i_inv;
	real_t r1ysq = r1.y * r1.y * a_i_inv;
	real_t r1nxy = -r1.x * r1.y * a_i_inv;
	k11 += r1ysq;
	k12 += r1nxy;
	k21 += r1nxy;
	k22 += r1xsq;

	// add the influence from r2
	real_t b_i_inv = b->get_inv_inertia();
	real_t r2xsq = r2.x * r2.x * b_i_inv;
	real_t r2ysq = r2.y * r2.y * b_i_inv;
	real_t r2nxy = -r2.x * r2.y * b_i_inv;
	k11 += r2ysq;
	k12 += r2nxy;
	k21 += r2nxy;
	k22 += r2xsq;

	// invert
	real_t determinant = k11 * k22 - k12 * k21;
	ERR_FAIL_COND(determinant == 0.0);

	real_t det_inv = 1.0f / determinant;
	*k1 = Vector2(k22 * det_inv, -k12 * det_inv);
	*k2 = Vector2(-k21 * det_inv, k11 * det_inv);
}

static _FORCE_INLINE_ Vector2
mult_k(const Vector2 &vr, const Vector2 &k1, const Vector2 &k2) {
	return Vector2(vr.dot(k1), vr.dot(k2));
}

bool GodotGrooveJoint2D::setup(real_t p_step) {
	dynamic_A = (A->get_mode() > PhysicsServer2D::BODY_MODE_KINEMATIC);
	dynamic_B = (B->get_mode() > PhysicsServer2D::BODY_MODE_KINEMATIC);

	if (!dynamic_A && !dynamic_B) {
		return false;
	}

	GodotSpace2D *space = A->get_space();
	ERR_FAIL_NULL_V(space, false);

	// calculate endpoints in worldspace
	Vector2 ta = A->get_transform().xform(A_groove_1);
	Vector2 tb = A->get_transform().xform(A_groove_2);

	// calculate axis
	Vector2 n = -(tb - ta).orthogonal().normalized();
	real_t d = ta.dot(n);

	xf_normal = n;
	rB = B->get_transform().basis_xform(B_anchor);

	// calculate tangential distance along the axis of rB
	real_t td = (B->get_transform().get_origin() + rB).cross(n);
	// calculate clamping factor and rB
	if (td <= ta.cross(n)) {
		clamp = 1.0f;
		rA = ta - A->get_transform().get_origin();
	} else if (td >= tb.cross(n)) {
		clamp = -1.0f;
		rA = tb - A->get_transform().get_origin();
	} else {
		clamp = 0.0f;
		//joint->r1 = cpvsub(cpvadd(cpvmult(cpvperp(n), -td), cpvmult(n, d)), a->p);
		rA = ((-n.orthogonal() * -td) + n * d) - A->get_transform().get_origin();
	}

	// Calculate mass tensor
	k_tensor(A, B, rA, rB, &k1, &k2);

	// compute max impulse
	jn_max = get_max_force() * p_step;

	// calculate bias velocity
	//cpVect delta = cpvsub(cpvadd(b->p, joint->r2), cpvadd(a->p, joint->r1));
	//joint->bias = cpvclamp(cpvmult(delta, -joint->constraint.biasCoef*dt_inv), joint->constraint.maxBias);

	Vector2 delta = (B->get_transform().get_origin() + rB) - (A->get_transform().get_origin() + rA);

	real_t _b = get_bias();
	gbias = (delta * -(_b == 0 ? space->get_constraint_bias() : _b) * (1.0 / p_step)).limit_length(get_max_bias());

	correct = true;
	return true;
}

bool GodotGrooveJoint2D::pre_solve(real_t p_step) {
	// Apply accumulated impulse.
	if (dynamic_A) {
		A->apply_impulse(-jn_acc, rA);
	}
	if (dynamic_B) {
		B->apply_impulse(jn_acc, rB);
	}

	return true;
}

void GodotGrooveJoint2D::solve(real_t p_step) {
	// compute impulse
	Vector2 vr = relative_velocity(A, B, rA, rB);

	Vector2 j = mult_k(gbias - vr, k1, k2);
	Vector2 jOld = jn_acc;
	j += jOld;

	jn_acc = (((clamp * j.cross(xf_normal)) > 0) ? j : j.project(xf_normal)).limit_length(jn_max);

	j = jn_acc - jOld;

	if (dynamic_A) {
		A->apply_impulse(-j, rA);
	}
	if (dynamic_B) {
		B->apply_impulse(j, rB);
	}
}

GodotGrooveJoint2D::GodotGrooveJoint2D(const Vector2 &p_a_groove1, const Vector2 &p_a_groove2, const Vector2 &p_b_anchor, GodotBody2D *p_body_a, GodotBody2D *p_body_b) :
		GodotJoint2D(_arr, 2) {
	A = p_body_a;
	B = p_body_b;

	A_groove_1 = A->get_inv_transform().xform(p_a_groove1);
	A_groove_2 = A->get_inv_transform().xform(p_a_groove2);
	B_anchor = B->get_inv_transform().xform(p_b_anchor);
	A_groove_normal = -(A_groove_2 - A_groove_1).normalized().orthogonal();

	A->add_constraint(this, 0);
	B->add_constraint(this, 1);
}

//////////////////////////////////////////////
//////////////////////////////////////////////
//////////////////////////////////////////////

bool GodotDampedSpringJoint2D::setup(real_t p_step) {
	dynamic_A = (A->get_mode() > PhysicsServer2D::BODY_MODE_KINEMATIC);
	dynamic_B = (B->get_mode() > PhysicsServer2D::BODY_MODE_KINEMATIC);

	if (!dynamic_A && !dynamic_B) {
		return false;
	}

	rA = A->get_transform().basis_xform(anchor_A);
	rB = B->get_transform().basis_xform(anchor_B);

	Vector2 delta = (B->get_transform().get_origin() + rB) - (A->get_transform().get_origin() + rA);
	real_t dist = delta.length();

	if (dist) {
		n = delta / dist;
	} else {
		n = Vector2();
	}

	real_t k = k_scalar(A, B, rA, rB, n);
	n_mass = 1.0f / k;

	target_vrn = 0.0f;
	v_coef = 1.0f - Math::exp(-damping * (p_step)*k);

	// Calculate spring force.
	real_t f_spring = (rest_length - dist) * stiffness;
	j = n * f_spring * (p_step);

	return true;
}

bool GodotDampedSpringJoint2D::pre_solve(real_t p_step) {
	// Apply spring force.
	if (dynamic_A) {
		A->apply_impulse(-j, rA);
	}
	if (dynamic_B) {
		B->apply_impulse(j, rB);
	}

	return true;
}

void GodotDampedSpringJoint2D::solve(real_t p_step) {
	// compute relative velocity
	real_t vrn = normal_relative_velocity(A, B, rA, rB, n) - target_vrn;

	// compute velocity loss from drag
	// not 100% certain this is derived correctly, though it makes sense
	real_t v_damp = -vrn * v_coef;
	target_vrn = vrn + v_damp;
	Vector2 j_new = n * v_damp * n_mass;

	if (dynamic_A) {
		A->apply_impulse(-j_new, rA);
	}
	if (dynamic_B) {
		B->apply_impulse(j_new, rB);
	}
}

void GodotDampedSpringJoint2D::set_param(PhysicsServer2D::DampedSpringParam p_param, real_t p_value) {
	switch (p_param) {
		case PhysicsServer2D::DAMPED_SPRING_REST_LENGTH: {
			rest_length = p_value;
		} break;
		case PhysicsServer2D::DAMPED_SPRING_DAMPING: {
			damping = p_value;
		} break;
		case PhysicsServer2D::DAMPED_SPRING_STIFFNESS: {
			stiffness = p_value;
		} break;
	}
}

real_t GodotDampedSpringJoint2D::get_param(PhysicsServer2D::DampedSpringParam p_param) const {
	switch (p_param) {
		case PhysicsServer2D::DAMPED_SPRING_REST_LENGTH: {
			return rest_length;
		} break;
		case PhysicsServer2D::DAMPED_SPRING_DAMPING: {
			return damping;
		} break;
		case PhysicsServer2D::DAMPED_SPRING_STIFFNESS: {
			return stiffness;
		} break;
	}

	ERR_FAIL_V(0);
}

GodotDampedSpringJoint2D::GodotDampedSpringJoint2D(const Vector2 &p_anchor_a, const Vector2 &p_anchor_b, GodotBody2D *p_body_a, GodotBody2D *p_body_b) :
		GodotJoint2D(_arr, 2) {
	A = p_body_a;
	B = p_body_b;
	anchor_A = A->get_inv_transform().xform(p_anchor_a);
	anchor_B = B->get_inv_transform().xform(p_anchor_b);

	rest_length = p_anchor_a.distance_to(p_anchor_b);

	A->add_constraint(this, 0);
	B->add_constraint(this, 1);
}

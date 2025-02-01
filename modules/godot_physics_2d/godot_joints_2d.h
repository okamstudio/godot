/**************************************************************************/
/*  godot_joints_2d.h                                                     */
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

#include "godot_body_2d.h"
#include "godot_constraint_2d.h"

class GodotJoint2D : public GodotConstraint2D {
	real_t bias = 0;
	real_t max_bias = 3.40282e+38;
	real_t max_force = 3.40282e+38;

protected:
	bool dynamic_A = false;
	bool dynamic_B = false;

public:
	_FORCE_INLINE_ void set_max_force(real_t p_force) { max_force = p_force; }
	_FORCE_INLINE_ real_t get_max_force() const { return max_force; }

	_FORCE_INLINE_ void set_bias(real_t p_bias) { bias = p_bias; }
	_FORCE_INLINE_ real_t get_bias() const { return bias; }

	_FORCE_INLINE_ void set_max_bias(real_t p_bias) { max_bias = p_bias; }
	_FORCE_INLINE_ real_t get_max_bias() const { return max_bias; }

	virtual bool setup(real_t p_step) override { return false; }
	virtual bool pre_solve(real_t p_step) override { return false; }
	virtual void solve(real_t p_step) override {}

	void copy_settings_from(GodotJoint2D *p_joint);

	virtual PhysicsServer2D::JointType get_type() const { return PhysicsServer2D::JOINT_TYPE_MAX; }
	GodotJoint2D(GodotBody2D **p_body_ptr = nullptr, int p_body_count = 0) :
			GodotConstraint2D(p_body_ptr, p_body_count) {}

	virtual ~GodotJoint2D() {
		for (int i = 0; i < get_body_count(); i++) {
			GodotBody2D *body = get_body_ptr()[i];
			if (body) {
				body->remove_constraint(this, i);
			}
		}
	}
};

class GodotPinJoint2D : public GodotJoint2D {
	union {
		struct {
			GodotBody2D *A;
			GodotBody2D *B;
		};

		GodotBody2D *_arr[2] = { nullptr, nullptr };
	};

	Transform2D M;
	Vector2 rA, rB;
	Vector2 anchor_A;
	Vector2 anchor_B;
	Vector2 bias;
	real_t initial_angle = 0.0;
	real_t bias_velocity = 0.0;
	real_t jn_max = 0.0;
	real_t j_acc = 0.0;
	real_t i_sum = 0.0;
	Vector2 P;
	real_t softness = 0.0;
	real_t angular_limit_lower = 0.0;
	real_t angular_limit_upper = 0.0;
	real_t motor_target_velocity = 0.0;
	bool is_joint_at_limit = false;
	bool motor_enabled = false;
	bool angular_limit_enabled = false;

public:
	virtual PhysicsServer2D::JointType get_type() const override { return PhysicsServer2D::JOINT_TYPE_PIN; }

	virtual bool setup(real_t p_step) override;
	virtual bool pre_solve(real_t p_step) override;
	virtual void solve(real_t p_step) override;

	void set_param(PhysicsServer2D::PinJointParam p_param, real_t p_value);
	real_t get_param(PhysicsServer2D::PinJointParam p_param) const;

	void set_flag(PhysicsServer2D::PinJointFlag p_flag, bool p_enabled);
	bool get_flag(PhysicsServer2D::PinJointFlag p_flag) const;

	GodotPinJoint2D(const Vector2 &p_pos, GodotBody2D *p_body_a, GodotBody2D *p_body_b = nullptr);
};

class GodotGrooveJoint2D : public GodotJoint2D {
	union {
		struct {
			GodotBody2D *A;
			GodotBody2D *B;
		};

		GodotBody2D *_arr[2] = { nullptr, nullptr };
	};

	Vector2 A_groove_1;
	Vector2 A_groove_2;
	Vector2 A_groove_normal;
	Vector2 B_anchor;
	Vector2 jn_acc;
	Vector2 gbias;
	real_t jn_max = 0.0;
	real_t clamp = 0.0;
	Vector2 xf_normal;
	Vector2 rA, rB;
	Vector2 k1, k2;

	bool correct = false;

public:
	virtual PhysicsServer2D::JointType get_type() const override { return PhysicsServer2D::JOINT_TYPE_GROOVE; }

	virtual bool setup(real_t p_step) override;
	virtual bool pre_solve(real_t p_step) override;
	virtual void solve(real_t p_step) override;

	GodotGrooveJoint2D(const Vector2 &p_a_groove1, const Vector2 &p_a_groove2, const Vector2 &p_b_anchor, GodotBody2D *p_body_a, GodotBody2D *p_body_b);
};

class GodotDampedSpringJoint2D : public GodotJoint2D {
	union {
		struct {
			GodotBody2D *A;
			GodotBody2D *B;
		};

		GodotBody2D *_arr[2] = { nullptr, nullptr };
	};

	Vector2 anchor_A;
	Vector2 anchor_B;

	real_t rest_length = 0.0;
	real_t damping = 1.5;
	real_t stiffness = 20.0;

	Vector2 rA, rB;
	Vector2 n;
	Vector2 j;
	real_t n_mass = 0.0;
	real_t target_vrn = 0.0;
	real_t v_coef = 0.0;

public:
	virtual PhysicsServer2D::JointType get_type() const override { return PhysicsServer2D::JOINT_TYPE_DAMPED_SPRING; }

	virtual bool setup(real_t p_step) override;
	virtual bool pre_solve(real_t p_step) override;
	virtual void solve(real_t p_step) override;

	void set_param(PhysicsServer2D::DampedSpringParam p_param, real_t p_value);
	real_t get_param(PhysicsServer2D::DampedSpringParam p_param) const;

	GodotDampedSpringJoint2D(const Vector2 &p_anchor_a, const Vector2 &p_anchor_b, GodotBody2D *p_body_a, GodotBody2D *p_body_b);
};

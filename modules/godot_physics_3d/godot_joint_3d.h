/**************************************************************************/
/*  godot_joint_3d.h                                                      */
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

#include "godot_body_3d.h"
#include "godot_constraint_3d.h"

class GodotJoint3D : public GodotConstraint3D {
protected:
	bool dynamic_A = false;
	bool dynamic_B = false;

	void plane_space(const Vector3 &n, Vector3 &p, Vector3 &q) {
		if (Math::abs(n.z) > Math_SQRT12) {
			// choose p in y-z plane
			real_t a = n[1] * n[1] + n[2] * n[2];
			real_t k = 1.0 / Math::sqrt(a);
			p = Vector3(0, -n[2] * k, n[1] * k);
			// set q = n x p
			q = Vector3(a * k, -n[0] * p[2], n[0] * p[1]);
		} else {
			// choose p in x-y plane
			real_t a = n.x * n.x + n.y * n.y;
			real_t k = 1.0 / Math::sqrt(a);
			p = Vector3(-n.y * k, n.x * k, 0);
			// set q = n x p
			q = Vector3(-n.z * p.y, n.z * p.x, a * k);
		}
	}

	_FORCE_INLINE_ real_t atan2fast(real_t y, real_t x) {
		real_t coeff_1 = Math_PI / 4.0f;
		real_t coeff_2 = 3.0f * coeff_1;
		real_t abs_y = Math::abs(y);
		real_t angle;
		if (x >= 0.0f) {
			real_t r = (x - abs_y) / (x + abs_y);
			angle = coeff_1 - coeff_1 * r;
		} else {
			real_t r = (x + abs_y) / (abs_y - x);
			angle = coeff_2 - coeff_1 * r;
		}
		return (y < 0.0f) ? -angle : angle;
	}

public:
	virtual bool setup(real_t p_step) override { return false; }
	virtual bool pre_solve(real_t p_step) override { return true; }
	virtual void solve(real_t p_step) override {}

	void copy_settings_from(GodotJoint3D *p_joint) {
		set_self(p_joint->get_self());
		set_priority(p_joint->get_priority());
		disable_collisions_between_bodies(p_joint->is_disabled_collisions_between_bodies());
	}

	virtual PhysicsServer3D::JointType get_type() const { return PhysicsServer3D::JOINT_TYPE_MAX; }
	_FORCE_INLINE_ GodotJoint3D(GodotBody3D **p_body_ptr = nullptr, int p_body_count = 0) :
			GodotConstraint3D(p_body_ptr, p_body_count) {
	}

	virtual ~GodotJoint3D() {
		for (int i = 0; i < get_body_count(); i++) {
			GodotBody3D *body = get_body_ptr()[i];
			if (body) {
				body->remove_constraint(this);
			}
		}
	}
};

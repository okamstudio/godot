/**************************************************************************/
/*  pin_joint_2d.h                                                        */
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

#include "scene/2d/physics/joints/joint_2d.h"

class PhysicsBody2D;

class PinJoint2D : public Joint2D {
	GDCLASS(PinJoint2D, Joint2D);

	real_t softness = 0.0;
	real_t angular_limit_lower = 0.0;
	real_t angular_limit_upper = 0.0;
	real_t motor_target_velocity = 0.0;
	bool motor_enabled = false;
	bool angular_limit_enabled = false;

protected:
	void _notification(int p_what);
	virtual void _configure_joint(RID p_joint, PhysicsBody2D *body_a, PhysicsBody2D *body_b) override;
	static void _bind_methods();

public:
	void set_softness(real_t p_softness);
	real_t get_softness() const;
	void set_angular_limit_lower(real_t p_angular_limit_lower);
	real_t get_angular_limit_lower() const;
	void set_angular_limit_upper(real_t p_angular_limit_upper);
	real_t get_angular_limit_upper() const;
	void set_motor_target_velocity(real_t p_motor_target_velocity);
	real_t get_motor_target_velocity() const;

	void set_motor_enabled(bool p_motor_enabled);
	bool is_motor_enabled() const;
	void set_angular_limit_enabled(bool p_angular_limit_enabled);
	bool is_angular_limit_enabled() const;

	PinJoint2D();
};

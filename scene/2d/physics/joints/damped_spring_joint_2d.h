/**************************************************************************/
/*  damped_spring_joint_2d.h                                              */
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

class DampedSpringJoint2D : public Joint2D {
	GDCLASS(DampedSpringJoint2D, Joint2D);

	real_t stiffness = 20.0;
	real_t damping = 1.0;
	real_t rest_length = 0.0;
	real_t length = 50.0;

protected:
	void _notification(int p_what);
	virtual void _configure_joint(RID p_joint, PhysicsBody2D *body_a, PhysicsBody2D *body_b) override;
	static void _bind_methods();

public:
	void set_length(real_t p_length);
	real_t get_length() const;

	void set_rest_length(real_t p_rest_length);
	real_t get_rest_length() const;

	void set_damping(real_t p_damping);
	real_t get_damping() const;

	void set_stiffness(real_t p_stiffness);
	real_t get_stiffness() const;

	DampedSpringJoint2D();
};

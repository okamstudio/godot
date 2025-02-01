/**************************************************************************/
/*  slider_joint_3d.h                                                     */
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

#include "scene/3d/physics/joints/joint_3d.h"

class SliderJoint3D : public Joint3D {
	GDCLASS(SliderJoint3D, Joint3D);

public:
	enum Param {
		PARAM_LINEAR_LIMIT_UPPER = PhysicsServer3D::SLIDER_JOINT_LINEAR_LIMIT_UPPER,
		PARAM_LINEAR_LIMIT_LOWER = PhysicsServer3D::SLIDER_JOINT_LINEAR_LIMIT_LOWER,
		PARAM_LINEAR_LIMIT_SOFTNESS = PhysicsServer3D::SLIDER_JOINT_LINEAR_LIMIT_SOFTNESS,
		PARAM_LINEAR_LIMIT_RESTITUTION = PhysicsServer3D::SLIDER_JOINT_LINEAR_LIMIT_RESTITUTION,
		PARAM_LINEAR_LIMIT_DAMPING = PhysicsServer3D::SLIDER_JOINT_LINEAR_LIMIT_DAMPING,
		PARAM_LINEAR_MOTION_SOFTNESS = PhysicsServer3D::SLIDER_JOINT_LINEAR_MOTION_SOFTNESS,
		PARAM_LINEAR_MOTION_RESTITUTION = PhysicsServer3D::SLIDER_JOINT_LINEAR_MOTION_RESTITUTION,
		PARAM_LINEAR_MOTION_DAMPING = PhysicsServer3D::SLIDER_JOINT_LINEAR_MOTION_DAMPING,
		PARAM_LINEAR_ORTHOGONAL_SOFTNESS = PhysicsServer3D::SLIDER_JOINT_LINEAR_ORTHOGONAL_SOFTNESS,
		PARAM_LINEAR_ORTHOGONAL_RESTITUTION = PhysicsServer3D::SLIDER_JOINT_LINEAR_ORTHOGONAL_RESTITUTION,
		PARAM_LINEAR_ORTHOGONAL_DAMPING = PhysicsServer3D::SLIDER_JOINT_LINEAR_ORTHOGONAL_DAMPING,

		PARAM_ANGULAR_LIMIT_UPPER = PhysicsServer3D::SLIDER_JOINT_ANGULAR_LIMIT_UPPER,
		PARAM_ANGULAR_LIMIT_LOWER = PhysicsServer3D::SLIDER_JOINT_ANGULAR_LIMIT_LOWER,
		PARAM_ANGULAR_LIMIT_SOFTNESS = PhysicsServer3D::SLIDER_JOINT_ANGULAR_LIMIT_SOFTNESS,
		PARAM_ANGULAR_LIMIT_RESTITUTION = PhysicsServer3D::SLIDER_JOINT_ANGULAR_LIMIT_RESTITUTION,
		PARAM_ANGULAR_LIMIT_DAMPING = PhysicsServer3D::SLIDER_JOINT_ANGULAR_LIMIT_DAMPING,
		PARAM_ANGULAR_MOTION_SOFTNESS = PhysicsServer3D::SLIDER_JOINT_ANGULAR_MOTION_SOFTNESS,
		PARAM_ANGULAR_MOTION_RESTITUTION = PhysicsServer3D::SLIDER_JOINT_ANGULAR_MOTION_RESTITUTION,
		PARAM_ANGULAR_MOTION_DAMPING = PhysicsServer3D::SLIDER_JOINT_ANGULAR_MOTION_DAMPING,
		PARAM_ANGULAR_ORTHOGONAL_SOFTNESS = PhysicsServer3D::SLIDER_JOINT_ANGULAR_ORTHOGONAL_SOFTNESS,
		PARAM_ANGULAR_ORTHOGONAL_RESTITUTION = PhysicsServer3D::SLIDER_JOINT_ANGULAR_ORTHOGONAL_RESTITUTION,
		PARAM_ANGULAR_ORTHOGONAL_DAMPING = PhysicsServer3D::SLIDER_JOINT_ANGULAR_ORTHOGONAL_DAMPING,
		PARAM_MAX = PhysicsServer3D::SLIDER_JOINT_MAX

	};

protected:
	real_t params[PARAM_MAX];
	virtual void _configure_joint(RID p_joint, PhysicsBody3D *body_a, PhysicsBody3D *body_b) override;
	static void _bind_methods();

public:
	void set_param(Param p_param, real_t p_value);
	real_t get_param(Param p_param) const;

	SliderJoint3D();
};

VARIANT_ENUM_CAST(SliderJoint3D::Param);

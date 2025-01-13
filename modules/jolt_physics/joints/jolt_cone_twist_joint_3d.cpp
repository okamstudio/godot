/**************************************************************************/
/*  jolt_cone_twist_joint_3d.cpp                                          */
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

#include "jolt_cone_twist_joint_3d.h"

#include "../misc/jolt_type_conversions.h"
#include "../objects/jolt_body_3d.h"
#include "../spaces/jolt_space_3d.h"

#include "Jolt/Physics/Constraints/SwingTwistConstraint.h"

namespace {

constexpr double DEFAULT_BIAS = 0.3;
constexpr double DEFAULT_SOFTNESS = 0.8;
constexpr double DEFAULT_RELAXATION = 1.0;

} // namespace

JPH::Constraint *JoltConeTwistJoint3D::_build_swing_twist(JPH::Body *p_jolt_body_a, JPH::Body *p_jolt_body_b, const Transform3D &p_shifted_ref_a, const Transform3D &p_shifted_ref_b, float p_swing_limit_span, float p_twist_limit_span) const {
	JPH::SwingTwistConstraintSettings constraint_settings;

	const bool twist_span_valid = p_twist_limit_span >= 0 && p_twist_limit_span <= JPH::JPH_PI;
	const bool swing_span_valid = p_swing_limit_span >= 0 && p_swing_limit_span <= JPH::JPH_PI;

	if (twist_limit_enabled && twist_span_valid) {
		constraint_settings.mTwistMinAngle = -p_twist_limit_span;
		constraint_settings.mTwistMaxAngle = p_twist_limit_span;
	} else {
		constraint_settings.mTwistMinAngle = -JPH::JPH_PI;
		constraint_settings.mTwistMaxAngle = JPH::JPH_PI;
	}

	if (swing_limit_enabled && swing_span_valid) {
		constraint_settings.mNormalHalfConeAngle = p_swing_limit_span;
		constraint_settings.mPlaneHalfConeAngle = p_swing_limit_span;
	} else {
		constraint_settings.mNormalHalfConeAngle = JPH::JPH_PI;
		constraint_settings.mPlaneHalfConeAngle = JPH::JPH_PI;

		if (!swing_span_valid) {
			constraint_settings.mTwistMinAngle = -JPH::JPH_PI;
			constraint_settings.mTwistMaxAngle = JPH::JPH_PI;
		}
	}

	constraint_settings.mSpace = JPH::EConstraintSpace::LocalToBodyCOM;
	constraint_settings.mPosition1 = to_jolt_r(p_shifted_ref_a.origin);
	constraint_settings.mTwistAxis1 = to_jolt(p_shifted_ref_a.basis.get_column(Vector3::AXIS_X));
	constraint_settings.mPlaneAxis1 = to_jolt(p_shifted_ref_a.basis.get_column(Vector3::AXIS_Z));
	constraint_settings.mPosition2 = to_jolt_r(p_shifted_ref_b.origin);
	constraint_settings.mTwistAxis2 = to_jolt(p_shifted_ref_b.basis.get_column(Vector3::AXIS_X));
	constraint_settings.mPlaneAxis2 = to_jolt(p_shifted_ref_b.basis.get_column(Vector3::AXIS_Z));
	constraint_settings.mSwingType = JPH::ESwingType::Pyramid;

	if (p_jolt_body_a == nullptr) {
		return constraint_settings.Create(JPH::Body::sFixedToWorld, *p_jolt_body_b);
	} else if (p_jolt_body_b == nullptr) {
		return constraint_settings.Create(*p_jolt_body_a, JPH::Body::sFixedToWorld);
	} else {
		return constraint_settings.Create(*p_jolt_body_a, *p_jolt_body_b);
	}
}

void JoltConeTwistJoint3D::_update_swing_motor_state() {
	if (JPH::SwingTwistConstraint *constraint = static_cast<JPH::SwingTwistConstraint *>(jolt_ref.GetPtr())) {
		constraint->SetSwingMotorState(swing_motor_enabled ? JPH::EMotorState::Velocity : JPH::EMotorState::Off);
	}
}

void JoltConeTwistJoint3D::_update_twist_motor_state() {
	if (JPH::SwingTwistConstraint *constraint = static_cast<JPH::SwingTwistConstraint *>(jolt_ref.GetPtr())) {
		constraint->SetTwistMotorState(twist_motor_enabled ? JPH::EMotorState::Velocity : JPH::EMotorState::Off);
	}
}

void JoltConeTwistJoint3D::_update_motor_velocity() {
	if (JPH::SwingTwistConstraint *constraint = static_cast<JPH::SwingTwistConstraint *>(jolt_ref.GetPtr())) {
		// We flip the direction since Jolt is CCW but Godot is CW.
		constraint->SetTargetAngularVelocityCS({ (float)-twist_motor_target_speed, (float)-swing_motor_target_speed_y, (float)-swing_motor_target_speed_z });
	}
}

void JoltConeTwistJoint3D::_update_swing_motor_limit() {
	if (JPH::SwingTwistConstraint *constraint = static_cast<JPH::SwingTwistConstraint *>(jolt_ref.GetPtr())) {
		JPH::MotorSettings &motor_settings = constraint->GetSwingMotorSettings();
		motor_settings.mMinTorqueLimit = (float)-swing_motor_max_torque;
		motor_settings.mMaxTorqueLimit = (float)swing_motor_max_torque;
	}
}

void JoltConeTwistJoint3D::_update_twist_motor_limit() {
	if (JPH::SwingTwistConstraint *constraint = static_cast<JPH::SwingTwistConstraint *>(jolt_ref.GetPtr())) {
		JPH::MotorSettings &motor_settings = constraint->GetTwistMotorSettings();
		motor_settings.mMinTorqueLimit = (float)-twist_motor_max_torque;
		motor_settings.mMaxTorqueLimit = (float)twist_motor_max_torque;
	}
}

void JoltConeTwistJoint3D::_limits_changed() {
	rebuild();
	_wake_up_bodies();
}

void JoltConeTwistJoint3D::_swing_motor_state_changed() {
	_update_swing_motor_state();
	_wake_up_bodies();
}

void JoltConeTwistJoint3D::_twist_motor_state_changed() {
	_update_twist_motor_state();
	_wake_up_bodies();
}

void JoltConeTwistJoint3D::_motor_velocity_changed() {
	_update_motor_velocity();
	_wake_up_bodies();
}

void JoltConeTwistJoint3D::_swing_motor_limit_changed() {
	_update_swing_motor_limit();
	_wake_up_bodies();
}

void JoltConeTwistJoint3D::_twist_motor_limit_changed() {
	_update_twist_motor_limit();
	_wake_up_bodies();
}

JoltConeTwistJoint3D::JoltConeTwistJoint3D(const JoltJoint3D &p_old_joint, JoltBody3D *p_body_a, JoltBody3D *p_body_b, const Transform3D &p_local_ref_a, const Transform3D &p_local_ref_b) :
		JoltJoint3D(p_old_joint, p_body_a, p_body_b, p_local_ref_a, p_local_ref_b) {
	rebuild();
}

double JoltConeTwistJoint3D::get_param(PhysicsServer3D::ConeTwistJointParam p_param) const {
	switch (p_param) {
		case PhysicsServer3D::CONE_TWIST_JOINT_SWING_SPAN: {
			return swing_limit_span;
		}
		case PhysicsServer3D::CONE_TWIST_JOINT_TWIST_SPAN: {
			return twist_limit_span;
		}
		case PhysicsServer3D::CONE_TWIST_JOINT_BIAS: {
			return DEFAULT_BIAS;
		}
		case PhysicsServer3D::CONE_TWIST_JOINT_SOFTNESS: {
			return DEFAULT_SOFTNESS;
		}
		case PhysicsServer3D::CONE_TWIST_JOINT_RELAXATION: {
			return DEFAULT_RELAXATION;
		}
		default: {
			ERR_FAIL_V_MSG(0.0, vformat("Unhandled cone twist joint parameter: '%d'. This should not happen. Please report this.", p_param));
		}
	}
}

void JoltConeTwistJoint3D::set_param(PhysicsServer3D::ConeTwistJointParam p_param, double p_value) {
	switch (p_param) {
		case PhysicsServer3D::CONE_TWIST_JOINT_SWING_SPAN: {
			swing_limit_span = p_value;
			_limits_changed();
		} break;
		case PhysicsServer3D::CONE_TWIST_JOINT_TWIST_SPAN: {
			twist_limit_span = p_value;
			_limits_changed();
		} break;
		case PhysicsServer3D::CONE_TWIST_JOINT_BIAS: {
			if (!Math::is_equal_approx(p_value, DEFAULT_BIAS)) {
				WARN_PRINT(vformat("Cone twist joint bias is not supported when using Jolt Physics. Any such value will be ignored. This joint connects %s.", _bodies_to_string()));
			}
		} break;
		case PhysicsServer3D::CONE_TWIST_JOINT_SOFTNESS: {
			if (!Math::is_equal_approx(p_value, DEFAULT_SOFTNESS)) {
				WARN_PRINT(vformat("Cone twist joint softness is not supported when using Jolt Physics. Any such value will be ignored. This joint connects %s.", _bodies_to_string()));
			}
		} break;
		case PhysicsServer3D::CONE_TWIST_JOINT_RELAXATION: {
			if (!Math::is_equal_approx(p_value, DEFAULT_RELAXATION)) {
				WARN_PRINT(vformat("Cone twist joint relaxation is not supported when using Jolt Physics. Any such value will be ignored. This joint connects %s.", _bodies_to_string()));
			}
		} break;
		default: {
			ERR_FAIL_MSG(vformat("Unhandled cone twist joint parameter: '%d'. This should not happen. Please report this.", p_param));
		} break;
	}
}

double JoltConeTwistJoint3D::get_jolt_param(JoltParameter p_param) const {
	switch (p_param) {
		case JoltPhysicsServer3D::CONE_TWIST_JOINT_SWING_MOTOR_TARGET_VELOCITY_Y: {
			return swing_motor_target_speed_y;
		}
		case JoltPhysicsServer3D::CONE_TWIST_JOINT_SWING_MOTOR_TARGET_VELOCITY_Z: {
			return swing_motor_target_speed_z;
		}
		case JoltPhysicsServer3D::CONE_TWIST_JOINT_TWIST_MOTOR_TARGET_VELOCITY: {
			return twist_motor_target_speed;
		}
		case JoltPhysicsServer3D::CONE_TWIST_JOINT_SWING_MOTOR_MAX_TORQUE: {
			return swing_motor_max_torque;
		}
		case JoltPhysicsServer3D::CONE_TWIST_JOINT_TWIST_MOTOR_MAX_TORQUE: {
			return twist_motor_max_torque;
		}
		default: {
			ERR_FAIL_V_MSG(0.0, vformat("Unhandled parameter: '%d'. This should not happen. Please report this.", p_param));
		}
	}
}

void JoltConeTwistJoint3D::set_jolt_param(JoltParameter p_param, double p_value) {
	switch (p_param) {
		case JoltPhysicsServer3D::CONE_TWIST_JOINT_SWING_MOTOR_TARGET_VELOCITY_Y: {
			swing_motor_target_speed_y = p_value;
			_motor_velocity_changed();
		} break;
		case JoltPhysicsServer3D::CONE_TWIST_JOINT_SWING_MOTOR_TARGET_VELOCITY_Z: {
			swing_motor_target_speed_z = p_value;
			_motor_velocity_changed();
		} break;
		case JoltPhysicsServer3D::CONE_TWIST_JOINT_TWIST_MOTOR_TARGET_VELOCITY: {
			twist_motor_target_speed = p_value;
			_motor_velocity_changed();
		} break;
		case JoltPhysicsServer3D::CONE_TWIST_JOINT_SWING_MOTOR_MAX_TORQUE: {
			swing_motor_max_torque = p_value;
			_swing_motor_limit_changed();
		} break;
		case JoltPhysicsServer3D::CONE_TWIST_JOINT_TWIST_MOTOR_MAX_TORQUE: {
			twist_motor_max_torque = p_value;
			_twist_motor_limit_changed();
		} break;
		default: {
			ERR_FAIL_MSG(vformat("Unhandled parameter: '%d'. This should not happen. Please report this.", p_param));
		} break;
	}
}

bool JoltConeTwistJoint3D::get_jolt_flag(JoltFlag p_flag) const {
	switch (p_flag) {
		case JoltPhysicsServer3D::CONE_TWIST_JOINT_FLAG_USE_SWING_LIMIT: {
			return swing_limit_enabled;
		}
		case JoltPhysicsServer3D::CONE_TWIST_JOINT_FLAG_USE_TWIST_LIMIT: {
			return twist_limit_enabled;
		}
		case JoltPhysicsServer3D::CONE_TWIST_JOINT_FLAG_ENABLE_SWING_MOTOR: {
			return swing_motor_enabled;
		}
		case JoltPhysicsServer3D::CONE_TWIST_JOINT_FLAG_ENABLE_TWIST_MOTOR: {
			return twist_motor_enabled;
		}
		default: {
			ERR_FAIL_V_MSG(false, vformat("Unhandled flag: '%d'. This should not happen. Please report this.", p_flag));
		}
	}
}

void JoltConeTwistJoint3D::set_jolt_flag(JoltFlag p_flag, bool p_enabled) {
	switch (p_flag) {
		case JoltPhysicsServer3D::CONE_TWIST_JOINT_FLAG_USE_SWING_LIMIT: {
			swing_limit_enabled = p_enabled;
			_limits_changed();
		} break;
		case JoltPhysicsServer3D::CONE_TWIST_JOINT_FLAG_USE_TWIST_LIMIT: {
			twist_limit_enabled = p_enabled;
			_limits_changed();
		} break;
		case JoltPhysicsServer3D::CONE_TWIST_JOINT_FLAG_ENABLE_SWING_MOTOR: {
			swing_motor_enabled = p_enabled;
			_swing_motor_state_changed();
		} break;
		case JoltPhysicsServer3D::CONE_TWIST_JOINT_FLAG_ENABLE_TWIST_MOTOR: {
			twist_motor_enabled = p_enabled;
			_twist_motor_state_changed();
		} break;
		default: {
			ERR_FAIL_MSG(vformat("Unhandled flag: '%d'. This should not happen. Please report this.", p_flag));
		} break;
	}
}

float JoltConeTwistJoint3D::get_applied_force() const {
	JPH::SwingTwistConstraint *constraint = static_cast<JPH::SwingTwistConstraint *>(jolt_ref.GetPtr());
	ERR_FAIL_NULL_V(constraint, 0.0f);

	JoltSpace3D *space = get_space();
	ERR_FAIL_NULL_V(space, 0.0f);

	const float last_step = space->get_last_step();
	if (unlikely(last_step == 0.0f)) {
		return 0.0f;
	}

	return constraint->GetTotalLambdaPosition().Length() / last_step;
}

float JoltConeTwistJoint3D::get_applied_torque() const {
	JPH::SwingTwistConstraint *constraint = static_cast<JPH::SwingTwistConstraint *>(jolt_ref.GetPtr());
	ERR_FAIL_NULL_V(constraint, 0.0f);

	JoltSpace3D *space = get_space();
	ERR_FAIL_NULL_V(space, 0.0f);

	const float last_step = space->get_last_step();
	if (unlikely(last_step == 0.0f)) {
		return 0.0f;
	}

	const JPH::Vec3 swing_twist_lambda = JPH::Vec3(constraint->GetTotalLambdaTwist(), constraint->GetTotalLambdaSwingY(), constraint->GetTotalLambdaSwingZ());

	// Note that the motor lambda is in a different space than the swing twist lambda, and since the two forces can cancel each other it is
	// technically incorrect to just add them. The bodies themselves have moved, so we can't transform one into the space of the other anymore.
	const float total_lambda = swing_twist_lambda.Length() + constraint->GetTotalLambdaMotor().Length();

	return total_lambda / last_step;
}

void JoltConeTwistJoint3D::rebuild() {
	destroy();

	JoltSpace3D *space = get_space();

	if (space == nullptr) {
		return;
	}

	const JPH::BodyID body_ids[2] = {
		body_a != nullptr ? body_a->get_jolt_id() : JPH::BodyID(),
		body_b != nullptr ? body_b->get_jolt_id() : JPH::BodyID()
	};

	const JoltWritableBodies3D jolt_bodies = space->write_bodies(body_ids, 2);

	JPH::Body *jolt_body_a = static_cast<JPH::Body *>(jolt_bodies[0]);
	JPH::Body *jolt_body_b = static_cast<JPH::Body *>(jolt_bodies[1]);

	ERR_FAIL_COND(jolt_body_a == nullptr && jolt_body_b == nullptr);

	Transform3D shifted_ref_a;
	Transform3D shifted_ref_b;

	_shift_reference_frames(Vector3(), Vector3(), shifted_ref_a, shifted_ref_b);

	jolt_ref = _build_swing_twist(jolt_body_a, jolt_body_b, shifted_ref_a, shifted_ref_b, (float)swing_limit_span, (float)twist_limit_span);

	space->add_joint(this);

	_update_enabled();
	_update_iterations();
	_update_swing_motor_state();
	_update_twist_motor_state();
	_update_motor_velocity();
	_update_swing_motor_limit();
	_update_twist_motor_limit();
}

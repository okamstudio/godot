/**************************************************************************/
/*  godot_generic_6dof_joint_3d.h                                         */
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

/*
Adapted to Godot from the Bullet library.
*/

#include "../godot_joint_3d.h"
#include "godot_jacobian_entry_3d.h"

/*
Bullet Continuous Collision Detection and Physics Library
Copyright (c) 2003-2006 Erwin Coumans  http://continuousphysics.com/Bullet/

This software is provided 'as-is', without any express or implied warranty.
In no event will the authors be held liable for any damages arising from the use of this software.
Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it freely,
subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not claim that you wrote the original software. If you use this software in a product, an acknowledgment in the product documentation would be appreciated but is not required.
2. Altered source versions must be plainly marked as such, and must not be misrepresented as being the original software.
3. This notice may not be removed or altered from any source distribution.
*/

/*
2007-09-09
GodotGeneric6DOFJoint3D Refactored by Francisco Le?n
email: projectileman@yahoo.com
http://gimpact.sf.net
*/

//! Rotation Limit structure for generic joints
class GodotG6DOFRotationalLimitMotor3D {
public:
	//! limit_parameters
	//!@{
	real_t m_loLimit = -1e30; //!< joint limit
	real_t m_hiLimit = 1e30; //!< joint limit
	real_t m_targetVelocity = 0.0; //!< target motor velocity
	real_t m_maxMotorForce = 0.1; //!< max force on motor
	real_t m_maxLimitForce = 300.0; //!< max force on limit
	real_t m_damping = 1.0; //!< Damping.
	real_t m_limitSoftness = 0.5; //! Relaxation factor
	real_t m_ERP = 0.5; //!< Error tolerance factor when joint is at limit
	real_t m_bounce = 0.0; //!< restitution factor
	bool m_enableMotor = false;
	bool m_enableLimit = false;

	//!@}

	//! temp_variables
	//!@{
	real_t m_currentLimitError = 0.0; //!< How much is violated this limit
	int m_currentLimit = 0; //!< 0=free, 1=at lo limit, 2=at hi limit
	real_t m_accumulatedImpulse = 0.0;
	//!@}

	GodotG6DOFRotationalLimitMotor3D() {}

	bool isLimited() {
		return (m_loLimit < m_hiLimit);
	}

	// Need apply correction.
	bool needApplyTorques() {
		return (m_enableMotor || m_currentLimit != 0);
	}

	// Calculates m_currentLimit and m_currentLimitError.
	int testLimitValue(real_t test_value);

	// Apply the correction impulses for two bodies.
	real_t solveAngularLimits(real_t timeStep, Vector3 &axis, real_t jacDiagABInv, GodotBody3D *body0, GodotBody3D *body1, bool p_body0_dynamic, bool p_body1_dynamic);
};

class GodotG6DOFTranslationalLimitMotor3D {
public:
	Vector3 m_lowerLimit = Vector3(0.0, 0.0, 0.0); //!< the constraint lower limits
	Vector3 m_upperLimit = Vector3(0.0, 0.0, 0.0); //!< the constraint upper limits
	Vector3 m_accumulatedImpulse = Vector3(0.0, 0.0, 0.0);
	//! Linear_Limit_parameters
	//!@{
	Vector3 m_limitSoftness = Vector3(0.7, 0.7, 0.7); //!< Softness for linear limit
	Vector3 m_damping = Vector3(1.0, 1.0, 1.0); //!< Damping for linear limit
	Vector3 m_restitution = Vector3(0.5, 0.5, 0.5); //! Bounce parameter for linear limit
	//!@}
	bool enable_limit[3] = { true, true, true };

	//! Test limit
	/*!
	 * - free means upper < lower,
	 * - locked means upper == lower
	 * - limited means upper > lower
	 * - limitIndex: first 3 are linear, next 3 are angular
	 */
	inline bool isLimited(int limitIndex) {
		return (m_upperLimit[limitIndex] >= m_lowerLimit[limitIndex]);
	}

	real_t solveLinearAxis(
			real_t timeStep,
			real_t jacDiagABInv,
			GodotBody3D *body1, const Vector3 &pointInA,
			GodotBody3D *body2, const Vector3 &pointInB,
			bool p_body1_dynamic, bool p_body2_dynamic,
			int limit_index,
			const Vector3 &axis_normal_on_a,
			const Vector3 &anchorPos);
};

class GodotGeneric6DOFJoint3D : public GodotJoint3D {
protected:
	union {
		struct {
			GodotBody3D *A;
			GodotBody3D *B;
		};

		GodotBody3D *_arr[2] = { nullptr, nullptr };
	};

	//! relative_frames
	//!@{
	Transform3D m_frameInA; //!< the constraint space w.r.t body A
	Transform3D m_frameInB; //!< the constraint space w.r.t body B
	//!@}

	//! Jacobians
	//!@{
	GodotJacobianEntry3D m_jacLinear[3]; //!< 3 orthogonal linear constraints
	GodotJacobianEntry3D m_jacAng[3]; //!< 3 orthogonal angular constraints
	//!@}

	//! Linear_Limit_parameters
	//!@{
	GodotG6DOFTranslationalLimitMotor3D m_linearLimits;
	//!@}

	//! hinge_parameters
	//!@{
	GodotG6DOFRotationalLimitMotor3D m_angularLimits[3];
	//!@}

protected:
	//! temporal variables
	//!@{
	real_t m_timeStep = 0.0;
	Transform3D m_calculatedTransformA;
	Transform3D m_calculatedTransformB;
	Vector3 m_calculatedAxisAngleDiff;
	Vector3 m_calculatedAxis[3];

	Vector3 m_AnchorPos; // point between pivots of bodies A and B to solve linear axes

	bool m_useLinearReferenceFrameA = false;

	//!@}

	GodotGeneric6DOFJoint3D(GodotGeneric6DOFJoint3D const &) = delete;
	void operator=(GodotGeneric6DOFJoint3D const &) = delete;

	void buildLinearJacobian(
			GodotJacobianEntry3D &jacLinear, const Vector3 &normalWorld,
			const Vector3 &pivotAInW, const Vector3 &pivotBInW);

	void buildAngularJacobian(GodotJacobianEntry3D &jacAngular, const Vector3 &jointAxisW);

	//! calcs the euler angles between the two bodies.
	void calculateAngleInfo();

public:
	GodotGeneric6DOFJoint3D(GodotBody3D *rbA, GodotBody3D *rbB, const Transform3D &frameInA, const Transform3D &frameInB, bool useLinearReferenceFrameA);

	virtual PhysicsServer3D::JointType get_type() const override { return PhysicsServer3D::JOINT_TYPE_6DOF; }

	virtual bool setup(real_t p_step) override;
	virtual void solve(real_t p_step) override;

	// Calcs the global transform for the joint offset for body A an B, and also calcs the angle differences between the bodies.
	void calculateTransforms();

	// Gets the global transform of the offset for body A.
	const Transform3D &getCalculatedTransformA() const {
		return m_calculatedTransformA;
	}

	// Gets the global transform of the offset for body B.
	const Transform3D &getCalculatedTransformB() const {
		return m_calculatedTransformB;
	}

	const Transform3D &getFrameOffsetA() const {
		return m_frameInA;
	}

	const Transform3D &getFrameOffsetB() const {
		return m_frameInB;
	}

	Transform3D &getFrameOffsetA() {
		return m_frameInA;
	}

	Transform3D &getFrameOffsetB() {
		return m_frameInB;
	}

	// Performs Jacobian calculation, and also calculates angle differences and axis.
	void updateRHS(real_t timeStep);

	// Get the rotation axis in global coordinates.
	Vector3 getAxis(int axis_index) const;

	// Get the relative Euler angle.
	real_t getAngle(int axis_index) const;

	// Calculates angular correction and returns true if limit needs to be corrected.
	bool testAngularLimitMotor(int axis_index);

	void setLinearLowerLimit(const Vector3 &linearLower) {
		m_linearLimits.m_lowerLimit = linearLower;
	}

	void setLinearUpperLimit(const Vector3 &linearUpper) {
		m_linearLimits.m_upperLimit = linearUpper;
	}

	void setAngularLowerLimit(const Vector3 &angularLower) {
		m_angularLimits[0].m_loLimit = angularLower.x;
		m_angularLimits[1].m_loLimit = angularLower.y;
		m_angularLimits[2].m_loLimit = angularLower.z;
	}

	void setAngularUpperLimit(const Vector3 &angularUpper) {
		m_angularLimits[0].m_hiLimit = angularUpper.x;
		m_angularLimits[1].m_hiLimit = angularUpper.y;
		m_angularLimits[2].m_hiLimit = angularUpper.z;
	}

	// Retrieves the angular limit information.
	GodotG6DOFRotationalLimitMotor3D *getRotationalLimitMotor(int index) {
		return &m_angularLimits[index];
	}

	// Retrieves the limit information.
	GodotG6DOFTranslationalLimitMotor3D *getTranslationalLimitMotor() {
		return &m_linearLimits;
	}

	// First 3 are linear, next 3 are angular.
	void setLimit(int axis, real_t lo, real_t hi) {
		if (axis < 3) {
			m_linearLimits.m_lowerLimit[axis] = lo;
			m_linearLimits.m_upperLimit[axis] = hi;
		} else {
			m_angularLimits[axis - 3].m_loLimit = lo;
			m_angularLimits[axis - 3].m_hiLimit = hi;
		}
	}

	//! Test limit
	/*!
	 * - free means upper < lower,
	 * - locked means upper == lower
	 * - limited means upper > lower
	 * - limitIndex: first 3 are linear, next 3 are angular
	 */
	bool isLimited(int limitIndex) {
		if (limitIndex < 3) {
			return m_linearLimits.isLimited(limitIndex);
		}
		return m_angularLimits[limitIndex - 3].isLimited();
	}

	const GodotBody3D *getRigidBodyA() const {
		return A;
	}
	const GodotBody3D *getRigidBodyB() const {
		return B;
	}

	virtual void calcAnchorPos(); // overridable

	void set_param(Vector3::Axis p_axis, PhysicsServer3D::G6DOFJointAxisParam p_param, real_t p_value);
	real_t get_param(Vector3::Axis p_axis, PhysicsServer3D::G6DOFJointAxisParam p_param) const;

	void set_flag(Vector3::Axis p_axis, PhysicsServer3D::G6DOFJointAxisFlag p_flag, bool p_value);
	bool get_flag(Vector3::Axis p_axis, PhysicsServer3D::G6DOFJointAxisFlag p_flag) const;
};

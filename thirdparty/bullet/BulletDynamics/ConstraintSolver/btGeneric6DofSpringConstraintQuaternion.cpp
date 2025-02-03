/*
Bullet Continuous Collision Detection and Physics Library
Copyright (c) 2003-2006 Erwin Coumans  https://bulletphysics.org

This software is provided 'as-is', without any express or implied warranty.
In no event will the authors be held liable for any damages arising from the use of this software.
Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it freely,
subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not claim that you wrote the original software. If you use this software in a product, an acknowledgment in the product documentation would be appreciated but is not required.
2. Altered source versions must be plainly marked as such, and must not be misrepresented as being the original software.
3. This notice may not be removed or altered from any source distribution.
*/

///This file was created by PHOBOSS (Kim Obordo) on 2022 August
///The btGeneric6DofSpringConstraintQuaternion class is based on btGeneric6DofSpring2Constraint.
///This class was made to provide more control over spring joint "equilibrium" rotation using quaternions instead of Euler Axes.
///This makes it useful for "puppeteering" joint controlled active ragdolls.

///4 May: btGeneric6DofSpring2Constraint is created from the original (2.82.2712) btGeneric6DofConstraint by Gabor Puhr and Tamas UmenhofferPros:
///- Much more accurate and stable in a lot of situation. (Especially when a sleeping chain of RBs connected with 6dof2 is pulled)
///- Stable and accurate spring with minimal energy loss that works with all of the solvers. (latter is not true for the original 6dof spring)
///- Servo motor functionality
///- Much more accurate bouncing. 0 really means zero bouncing (not true for the original 6odf) and there is only a minimal energy loss when the value is 1 (because of the solvers' precision)
///- Rotation order for the Euler system can be set. (One axis' freedom is still limited to pi/2)
///
///Cons:
///- It is slower than the original 6dof. There is no exact ratio, but half speed is a good estimation.
///- At bouncing the correct velocity is calculated, but not the correct position. (it is because of the solver can correct position or velocity, but not both.)

/// 2009 March: btGeneric6DofConstraint refactored by Roman Ponomarev
/// Added support for generic constraint solver through getInfo1Q/getInfo2 methods

///2007-09-09
///btGeneric6DofConstraint Refactored by Francisco Le?n
///email: projectileman@yahoo.com
///http://gimpact.sf.net

#include "btGeneric6DofSpringConstraintQuaternion.h"
#include "btGeneric6DofSpring2Constraint.h"
#include "BulletDynamics/Dynamics/btRigidBody.h"
#include "LinearMath/btTransformUtil.h"
#include <cmath>
#include <new>

btGeneric6DofSpringConstraintQuaternion::btGeneric6DofSpringConstraintQuaternion(btRigidBody& rbA, btRigidBody& rbB, const btTransform& frameInA, const btTransform& frameInB, RotateOrder rotOrder)
	: btGeneric6DofSpring2Constraint(rbA, rbB, frameInA, frameInB, rotOrder)
{
}


btGeneric6DofSpringConstraintQuaternion::btGeneric6DofSpringConstraintQuaternion(btRigidBody & rbB, const btTransform& frameInB, RotateOrder rotOrder)
	: btGeneric6DofSpring2Constraint(rbB, frameInB, rotOrder)
{
}


void btGeneric6DofSpringConstraintQuaternion::set_use_global_rotation(bool p_value) {
	using_global_rotation = p_value;
}
bool btGeneric6DofSpringConstraintQuaternion::get_use_global_rotation() {
	return using_global_rotation;
}

void btGeneric6DofSpringConstraintQuaternion::set_use_quaternion_rotation_equilibrium(bool p_enable) {
	using_quaternion_rotation_equilibrium = p_enable;
}
bool btGeneric6DofSpringConstraintQuaternion::get_use_quaternion_rotation_equilibrium() {
	return using_quaternion_rotation_equilibrium;
}
void btGeneric6DofSpringConstraintQuaternion::set_quaternion_rotation_equilibrium(btQuaternion p_value) {
	quaternion_rotation_equilibrium = p_value;
}
btQuaternion btGeneric6DofSpringConstraintQuaternion::get_quaternion_rotation_equilibrium() {
	return quaternion_rotation_equilibrium;
}

void btGeneric6DofSpringConstraintQuaternion::getInfo2(btConstraintInfo2* info)
{
	const btTransform& transA = m_rbA.getCenterOfMassTransform();
	const btTransform& transB = m_rbB.getCenterOfMassTransform();
	const btVector3& linVelA = m_rbA.getLinearVelocity();
	const btVector3& linVelB = m_rbB.getLinearVelocity();
	const btVector3& angVelA = m_rbA.getAngularVelocity();
	const btVector3& angVelB = m_rbB.getAngularVelocity();

	int row = setAngularLimitsQuaternion(info, 0, transA, transB, linVelA, linVelB, angVelA, angVelB);
	setLinearLimits(info, row, transA, transB, linVelA, linVelB, angVelA, angVelB);
}

///PHOBOSS: this function is based from btGeneric6DofSpring2Constraint's "setAngularLimits" function
///Here, quaternions are used to compute for the rotation error used to compute for the constraints
int btGeneric6DofSpringConstraintQuaternion::setAngularLimitsQuaternion(btConstraintInfo2 *info, int row_offset, const btTransform &transA, const btTransform &transB, const btVector3 &linVelA, const btVector3 &linVelB, const btVector3 &angVelA, const btVector3 &angVelB) {
	int row = row_offset;

	int cIdx[] = { 2, 0, 1 }; ///PHOBOSS: arbitrary, order doesn't actually matter here

	///PHOBOSS: interpret equilibrium rotation as global rotation
	btVector3 body_axis[] = { btVector3(1.0, 0.0, 0.0), btVector3(0.0, 1.0, 0.0), btVector3(0.0, 0.0, 1.0) };
	btQuaternion current_rotation_quat = (transB).getRotation();
	if (!using_global_rotation) {
		////PHOBOSS: rotation axis should be body A's basis matrix
		body_axis[0] = transA.getBasis().getColumn(0);
		body_axis[1] = transA.getBasis().getColumn(1);
		body_axis[2] = transA.getBasis().getColumn(2);
		current_rotation_quat = (transA.inverse() * transB).getRotation(); ////PHOBOSS: body B's rotation relative to body A
	}
	btQuaternion equilibrium_rotation_quat = quaternion_rotation_equilibrium; ////PHOBOSS: faster
	if (!using_quaternion_rotation_equilibrium) {
		equilibrium_rotation_quat = btQuaternion(m_angularLimits[1].m_equilibriumPoint, m_angularLimits[0].m_equilibriumPoint, m_angularLimits[2].m_equilibriumPoint);
	}
	
	btQuaternion rotation_change = equilibrium_rotation_quat * current_rotation_quat.inverse();

	///btScalar angle = rotation_change.getAngleShortestPath();
	btScalar angle = 0.0; ///PHOBOSS: this is more robust for some reason
	if (abs(rotation_change[3]) <= 1.0) ///PHOBOSS: protection against NAN
	{
		angle = 2.0 * acos(rotation_change[3]);
		if (angle > SIMD_PI)
			angle -= 2.0 * SIMD_PI;
	}
	///PHOBOSS: I know, I should have passed the angle and calculated for the quaternion_axis (axis_q) separately inside "get_limit_motor_info_quaternion" but I think it wouldn't save much execution time anyways

	btVector3 axis_q = rotation_change.getAxis();
	btVector3 vec_angle_error = axis_q * angle;

	///PHOBOSS: tried to implement my own impulse calculation using critical damping, it didn't work out for me... here's a reference:https://www.gamedev.net/tutorials/programming/math-and-physics/towards-a-simpler-stiffer-and-more-stable-spring-r3227/ //
	///btMatrix3x3 reduced_moment_of_inertia = (m_rbA.getInvInertiaTensorWorld() + m_rbB.getInvInertiaTensorWorld()).inverse();
	///btVector3 kd = btVector3(m_angularLimits[0].m_springDamping, m_angularLimits[1].m_springDamping, m_angularLimits[2].m_springDamping);// values should be [0-1]
	///btVector3 ks = btVector3(m_angularLimits[0].m_springStiffness, m_angularLimits[1].m_springStiffness, m_angularLimits[2].m_springStiffness);// values should be [0-1]
	///btVector3 P = quat_angle_error;
	///btVector3 D = m_rbB.getAngularVelocity() - m_rbA.getAngularVelocity() * -1.0;
	///btVector3 calculated_spring_velocity = P + D;
	///btVector3 calculated_spring_impulse = reduced_moment_of_inertia * calculated_spring_velocity;

	for (int ii = 0; ii < 3; ii++) {
		int i = cIdx[ii];
		if (m_angularLimits[i].m_currentLimit || m_angularLimits[i].m_enableMotor || m_angularLimits[i].m_enableSpring) {
			btVector3 axis = body_axis[i];
			int flags = m_flags >> ((i + 3) * BT_6DOF_FLAGS_AXIS_SHIFT2);
			if (!(flags & BT_6DOF_FLAGS_CFM_STOP2)) {
				m_angularLimits[i].m_stopCFM = info->cfm[0];
			}
			if (!(flags & BT_6DOF_FLAGS_ERP_STOP2)) {
				m_angularLimits[i].m_stopERP = info->erp;
			}
			if (!(flags & BT_6DOF_FLAGS_CFM_MOTO2)) {
				m_angularLimits[i].m_motorCFM = info->cfm[0];
			}
			if (!(flags & BT_6DOF_FLAGS_ERP_MOTO2)) {
				m_angularLimits[i].m_motorERP = info->erp;
			}

			row += get_limit_motor_info_quaternion(&m_angularLimits[i], transA, transB, linVelA, linVelB, angVelA, angVelB, info, row, axis, vec_angle_error[i]);
		}
	}

	return row;
}

///PHOBOSS: this function is based from btGeneric6DofSpring2Constraint's "get_limit_motor_info2" function modified for the sole purpose of finding angular constraints
int btGeneric6DofSpringConstraintQuaternion::get_limit_motor_info_quaternion(
	btRotationalLimitMotor2* limot,
	const btTransform& transA, const btTransform& transB, const btVector3& linVelA, const btVector3& linVelB, const btVector3& angVelA, const btVector3& angVelB,
	btConstraintInfo2* info, int row, btVector3& ax1, btScalar& vec_rotation_error_element, int rotAllowed)
{
	int count = 0;
	int srow = row * info->rowskip;

	if (limot->m_currentLimit == 4)
	{
		btScalar vel = angVelA.dot(ax1) - angVelB.dot(ax1);

		calculateJacobi(limot, transA, transB, info, srow, ax1, 1, rotAllowed);

		info->m_constraintError[srow] = info->fps * limot->m_stopERP * limot->m_currentLimitError * -1;

		if (info->m_constraintError[srow] - vel * limot->m_stopERP > 0)
		{
			btScalar bounceerror = -limot->m_bounce * vel;
			if (bounceerror > info->m_constraintError[srow]) info->m_constraintError[srow] = bounceerror;
		}

		info->m_lowerLimit[srow] = 0;
		info->m_upperLimit[srow] = SIMD_INFINITY;
		info->cfm[srow] = limot->m_stopCFM;
		srow += info->rowskip;
		++count;


		calculateJacobi(limot, transA, transB, info, srow, ax1, 1, rotAllowed);
	
		info->m_constraintError[srow] = info->fps * limot->m_stopERP * limot->m_currentLimitErrorHi * -1;

		if (info->m_constraintError[srow] - vel * limot->m_stopERP < 0)
		{
			btScalar bounceerror = -limot->m_bounce * vel;
			if (bounceerror < info->m_constraintError[srow]) info->m_constraintError[srow] = bounceerror;
		}


		info->m_lowerLimit[srow] = -SIMD_INFINITY;
		info->m_upperLimit[srow] = 0;
		info->cfm[srow] = limot->m_stopCFM;
		srow += info->rowskip;
		++count;
	}
	else if (limot->m_currentLimit == 3)
	{

		calculateJacobi(limot, transA, transB, info, srow, ax1, 1, rotAllowed);
		info->m_constraintError[srow] = info->fps * limot->m_stopERP * limot->m_currentLimitError * -1;
		info->m_lowerLimit[srow] = -SIMD_INFINITY;
		info->m_upperLimit[srow] = SIMD_INFINITY;
		info->cfm[srow] = limot->m_stopCFM;
		srow += info->rowskip;
		++count;
	}

	if (limot->m_enableMotor && !limot->m_servoMotor)
	{

		calculateJacobi(limot, transA, transB, info, srow, ax1, 1, rotAllowed);
		btScalar tag_vel = limot->m_targetVelocity;
		btScalar mot_fact = getMotorFactor(limot->m_currentPosition,
										   limot->m_loLimit,
										   limot->m_hiLimit,
										   tag_vel,
										   info->fps * limot->m_motorERP);
		info->m_constraintError[srow] = mot_fact * limot->m_targetVelocity;
		info->m_lowerLimit[srow] = -limot->m_maxMotorForce / info->fps;
		info->m_upperLimit[srow] = limot->m_maxMotorForce / info->fps;
		info->cfm[srow] = limot->m_motorCFM;
		srow += info->rowskip;
		++count;
	}

	if (limot->m_enableMotor && limot->m_servoMotor)
	{
		btScalar error = limot->m_currentPosition - limot->m_servoTarget;
		btScalar curServoTarget = limot->m_servoTarget;

		if (error > SIMD_PI)
		{
			error -= SIMD_2_PI;
			curServoTarget += SIMD_2_PI;
		}
		if (error < -SIMD_PI)
		{
			error += SIMD_2_PI;
			curServoTarget -= SIMD_2_PI;
		}

		calculateJacobi(limot, transA, transB, info, srow, ax1, 1, rotAllowed);
		btScalar targetvelocity = error < 0 ? -limot->m_targetVelocity : limot->m_targetVelocity;
		btScalar tag_vel = -targetvelocity;
		btScalar mot_fact;
		if (error != 0)
		{
			btScalar lowLimit;
			btScalar hiLimit;
			if (limot->m_loLimit > limot->m_hiLimit)
			{
				lowLimit = error > 0 ? curServoTarget : -SIMD_INFINITY;
				hiLimit = error < 0 ? curServoTarget : SIMD_INFINITY;
			}
			else
			{
				lowLimit = error > 0 && curServoTarget > limot->m_loLimit ? curServoTarget : limot->m_loLimit;
				hiLimit = error < 0 && curServoTarget < limot->m_hiLimit ? curServoTarget : limot->m_hiLimit;
			}
			mot_fact = getMotorFactor(limot->m_currentPosition, lowLimit, hiLimit, tag_vel, info->fps * limot->m_motorERP);
		}
		else
		{
			mot_fact = 0;
		}

		info->m_constraintError[srow] = mot_fact * targetvelocity * -1;
		info->m_lowerLimit[srow] = -limot->m_maxMotorForce / info->fps;
		info->m_upperLimit[srow] = limot->m_maxMotorForce / info->fps;
		info->cfm[srow] = limot->m_motorCFM;
		srow += info->rowskip;
		++count;
	}

	if (limot->m_enableSpring)
	{

		btScalar error = vec_rotation_error_element;

		calculateJacobi(limot, transA, transB, info, srow, ax1, 1, rotAllowed);

		btScalar dt = BT_ONE / info->fps;
		btScalar kd = limot->m_springDamping;
		btScalar ks = limot->m_springStiffness;
		btScalar vel;

		vel = angVelA.dot(ax1) - angVelB.dot(ax1);

		btScalar cfm = BT_ZERO;
		btScalar mA = BT_ONE / m_rbA.getInvMass();
		btScalar mB = BT_ONE / m_rbB.getInvMass();

		btScalar rrA = (m_calculatedTransformA.getOrigin() - transA.getOrigin()).length2();
		btScalar rrB = (m_calculatedTransformB.getOrigin() - transB.getOrigin()).length2();
		if (m_rbA.getInvMass()) mA = mA * rrA + 1 / (m_rbA.getInvInertiaTensorWorld() * ax1).length();
		if (m_rbB.getInvMass()) mB = mB * rrB + 1 / (m_rbB.getInvInertiaTensorWorld() * ax1).length();

		btScalar m;
		if (m_rbA.getInvMass() == 0)
			m = mB;
		else if (m_rbB.getInvMass() == 0)
			m = mA;
		else
			m = mA * mB / (mA + mB);
		btScalar angularfreq = btSqrt(ks / m);

		if (limot->m_springStiffnessLimited && 0.25 < angularfreq * dt)
		{
			ks = BT_ONE / dt / dt / btScalar(16.0) * m;
		}

		if (limot->m_springDampingLimited && kd * dt > m)
		{
			kd = m / dt;
		}
		btScalar fs = ks * error * dt;
		btScalar fd = -kd * (vel) * -1* dt;
		btScalar f = (fs + fd);

		if (m_flags & BT_6DOF_FLAGS_USE_INFINITE_ERROR)
			info->m_constraintError[srow] = -1 * (f < 0 ? -SIMD_INFINITY : SIMD_INFINITY);
		else
			info->m_constraintError[srow] = vel + f / m * -1;


		btScalar minf = f < fd ? f : fd;
		btScalar maxf = f < fd ? fd : f;

		info->m_lowerLimit[srow] = -maxf > 0 ? 0 : -maxf;
		info->m_upperLimit[srow] = -minf < 0 ? 0 : -minf;

		info->cfm[srow] = cfm;
		srow += info->rowskip;
		++count;
	}

	return count;
}



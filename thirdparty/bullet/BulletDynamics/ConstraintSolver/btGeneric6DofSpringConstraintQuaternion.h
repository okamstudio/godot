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


#ifndef BT_GENERIC_6DOF_CONSTRAINT_QUAT_H
#define BT_GENERIC_6DOF_CONSTRAINT_QUAT_H

#include "btGeneric6DofSpring2Constraint.h"
#include "LinearMath/btVector3.h"
#include "btJacobianEntry.h"
#include "btTypedConstraint.h"

#include "LinearMath/btQuaternion.h"

ATTRIBUTE_ALIGNED16(class)
btGeneric6DofSpringConstraintQuaternion : public btGeneric6DofSpring2Constraint{
	public:
		BT_DECLARE_ALIGNED_ALLOCATOR()
		btGeneric6DofSpringConstraintQuaternion(btRigidBody & rbA, btRigidBody & rbB, const btTransform& frameInA, const btTransform& frameInB, RotateOrder rotOrder = RO_XYZ);
		btGeneric6DofSpringConstraintQuaternion(btRigidBody & rbB, const btTransform& frameInB, RotateOrder rotOrder = RO_XYZ);
		void getInfo2(btConstraintInfo2 * info);

		void set_use_global_rotation(bool p_value);
		bool get_use_global_rotation();

		void set_use_quaternion_rotation_equilibrium(bool p_enable);
		bool get_use_quaternion_rotation_equilibrium();
		void set_quaternion_rotation_equilibrium(btQuaternion p_value);
		btQuaternion get_quaternion_rotation_equilibrium();

	protected:
		bool using_global_rotation;
		bool using_quaternion_rotation_equilibrium;
		btQuaternion quaternion_rotation_equilibrium;
		
		int setAngularLimitsQuaternion(btConstraintInfo2 * info, int row_offset, const btTransform& transA, const btTransform& transB, const btVector3& linVelA, const btVector3& linVelB, const btVector3& angVelA, const btVector3& angVelB);
		int get_limit_motor_info_quaternion(
			btRotationalLimitMotor2 * limot,
			const btTransform& transA, const btTransform& transB, const btVector3& linVelA, const btVector3& linVelB, const btVector3& angVelA, const btVector3& angVelB,
			btConstraintInfo2* info, int row, btVector3& ax1, btScalar& vec_rotation_error_element, int rotAllowed = false);
};
#endif

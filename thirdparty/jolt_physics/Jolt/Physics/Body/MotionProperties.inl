// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

JPH_NAMESPACE_BEGIN

void MotionProperties::MoveKinematic(Vec3Arg inDeltaPosition, QuatArg inDeltaRotation, float inDeltaTime)
{
	JPH_ASSERT(BodyAccess::sCheckRights(BodyAccess::sVelocityAccess(), BodyAccess::EAccess::ReadWrite));
	JPH_ASSERT(BodyAccess::sCheckRights(BodyAccess::sPositionAccess(), BodyAccess::EAccess::Read));
	JPH_ASSERT(mCachedBodyType == EBodyType::RigidBody);
	JPH_ASSERT(mCachedMotionType != EMotionType::Static);

	// Calculate required linear velocity
	mLinearVelocity = LockTranslation(inDeltaPosition / inDeltaTime);

	// Calculate required angular velocity
	Vec3 axis;
	float angle;
	inDeltaRotation.GetAxisAngle(axis, angle);
	mAngularVelocity = LockAngular(axis * (angle / inDeltaTime));
}

void MotionProperties::ClampLinearVelocity()
{
	JPH_ASSERT(BodyAccess::sCheckRights(BodyAccess::sVelocityAccess(), BodyAccess::EAccess::ReadWrite));

	float len_sq = mLinearVelocity.LengthSq();
	JPH_ASSERT(isfinite(len_sq));
	if (len_sq > Square(mMaxLinearVelocity))
		mLinearVelocity *= mMaxLinearVelocity / sqrt(len_sq);
}

void MotionProperties::ClampAngularVelocity()
{
	JPH_ASSERT(BodyAccess::sCheckRights(BodyAccess::sVelocityAccess(), BodyAccess::EAccess::ReadWrite));

	float len_sq = mAngularVelocity.LengthSq();
	JPH_ASSERT(isfinite(len_sq));
	if (len_sq > Square(mMaxAngularVelocity))
		mAngularVelocity *= mMaxAngularVelocity / sqrt(len_sq);
}

inline Mat44 MotionProperties::GetLocalSpaceInverseInertiaUnchecked() const
{
	Mat44 rotation = Mat44::sRotation(mInertiaRotation);
	Mat44 rotation_mul_scale_transposed(mInvInertiaDiagonal.SplatX() * rotation.GetColumn4(0), mInvInertiaDiagonal.SplatY() * rotation.GetColumn4(1), mInvInertiaDiagonal.SplatZ() * rotation.GetColumn4(2), Vec4(0, 0, 0, 1));
	return rotation.Multiply3x3RightTransposed(rotation_mul_scale_transposed);
}

inline void MotionProperties::ScaleToMass(float inMass)
{
	JPH_ASSERT(mInvMass > 0.0f, "Body must have finite mass");
	JPH_ASSERT(inMass > 0.0f, "New mass cannot be zero");

	float new_inv_mass = 1.0f / inMass;
	mInvInertiaDiagonal *= new_inv_mass / mInvMass;
	mInvMass = new_inv_mass;
}

inline Mat44 MotionProperties::GetLocalSpaceInverseInertia() const
{
	JPH_ASSERT(mCachedMotionType == EMotionType::Dynamic);
	return GetLocalSpaceInverseInertiaUnchecked();
}

Mat44 MotionProperties::GetInverseInertiaForRotation(Mat44Arg inRotation) const
{
	JPH_ASSERT(mCachedMotionType == EMotionType::Dynamic);

	Mat44 rotation = inRotation.Multiply3x3(Mat44::sRotation(mInertiaRotation));
	Mat44 rotation_mul_scale_transposed(mInvInertiaDiagonal.SplatX() * rotation.GetColumn4(0), mInvInertiaDiagonal.SplatY() * rotation.GetColumn4(1), mInvInertiaDiagonal.SplatZ() * rotation.GetColumn4(2), Vec4(0, 0, 0, 1));
	Mat44 inverse_inertia = rotation.Multiply3x3RightTransposed(rotation_mul_scale_transposed);

	// We need to mask out both the rows and columns of DOFs that are not allowed
	Vec4 angular_dofs_mask = GetAngularDOFsMask().ReinterpretAsFloat();
	inverse_inertia.SetColumn4(0, Vec4::sAnd(inverse_inertia.GetColumn4(0), Vec4::sAnd(angular_dofs_mask, angular_dofs_mask.SplatX())));
	inverse_inertia.SetColumn4(1, Vec4::sAnd(inverse_inertia.GetColumn4(1), Vec4::sAnd(angular_dofs_mask, angular_dofs_mask.SplatY())));
	inverse_inertia.SetColumn4(2, Vec4::sAnd(inverse_inertia.GetColumn4(2), Vec4::sAnd(angular_dofs_mask, angular_dofs_mask.SplatZ())));

	return inverse_inertia;
}

Vec3 MotionProperties::MultiplyWorldSpaceInverseInertiaByVector(QuatArg inBodyRotation, Vec3Arg inV) const
{
	JPH_ASSERT(mCachedMotionType == EMotionType::Dynamic);

	// Mask out columns of DOFs that are not allowed
	Vec3 angular_dofs_mask = Vec3(GetAngularDOFsMask().ReinterpretAsFloat());
	Vec3 v = Vec3::sAnd(inV, angular_dofs_mask);

	// Multiply vector by inverse inertia
	Mat44 rotation = Mat44::sRotation(inBodyRotation * mInertiaRotation);
	Vec3 result = rotation.Multiply3x3(mInvInertiaDiagonal * rotation.Multiply3x3Transposed(v));

	// Mask out rows of DOFs that are not allowed
	return Vec3::sAnd(result, angular_dofs_mask);
}

void MotionProperties::ApplyGyroscopicForceInternal(QuatArg inBodyRotation, float inDeltaTime)
{
	JPH_ASSERT(BodyAccess::sCheckRights(BodyAccess::sVelocityAccess(), BodyAccess::EAccess::ReadWrite));
	JPH_ASSERT(mCachedBodyType == EBodyType::RigidBody);
	JPH_ASSERT(mCachedMotionType == EMotionType::Dynamic);

	// Calculate local space inertia tensor (a diagonal in local space)
	UVec4 is_zero = Vec3::sEquals(mInvInertiaDiagonal, Vec3::sZero());
	Vec3 denominator = Vec3::sSelect(mInvInertiaDiagonal, Vec3::sReplicate(1.0f), is_zero);
	Vec3 nominator = Vec3::sSelect(Vec3::sReplicate(1.0f), Vec3::sZero(), is_zero);
	Vec3 local_inertia = nominator / denominator; // Avoid dividing by zero, inertia in this axis will be zero

	// Calculate local space angular momentum
	Quat inertia_space_to_world_space = inBodyRotation * mInertiaRotation;
	Vec3 local_angular_velocity = inertia_space_to_world_space.Conjugated() * mAngularVelocity;
	Vec3 local_momentum = local_inertia * local_angular_velocity;

	// The gyroscopic force applies a torque: T = -w x I w where w is angular velocity and I the inertia tensor
	// Calculate the new angular momentum by applying the gyroscopic force and make sure the new magnitude is the same as the old one
	// to avoid introducing energy into the system due to the Euler step
	Vec3 new_local_momentum = local_momentum - inDeltaTime * local_angular_velocity.Cross(local_momentum);
	float new_local_momentum_len_sq = new_local_momentum.LengthSq();
	new_local_momentum = new_local_momentum_len_sq > 0.0f? new_local_momentum * sqrt(local_momentum.LengthSq() / new_local_momentum_len_sq) : Vec3::sZero();

	// Convert back to world space angular velocity
	mAngularVelocity = inertia_space_to_world_space * (mInvInertiaDiagonal * new_local_momentum);
}

void MotionProperties::ApplyForceTorqueAndDragInternal(QuatArg inBodyRotation, Vec3Arg inGravity, float inDeltaTime)
{
	JPH_ASSERT(BodyAccess::sCheckRights(BodyAccess::sVelocityAccess(), BodyAccess::EAccess::ReadWrite));
	JPH_ASSERT(mCachedBodyType == EBodyType::RigidBody);
	JPH_ASSERT(mCachedMotionType == EMotionType::Dynamic);

	// Update linear velocity
	mLinearVelocity = LockTranslation(mLinearVelocity + inDeltaTime * (mGravityFactor * inGravity + mInvMass * GetAccumulatedForce()));

	// Update angular velocity
	mAngularVelocity += inDeltaTime * MultiplyWorldSpaceInverseInertiaByVector(inBodyRotation, GetAccumulatedTorque());

	// Linear damping: dv/dt = -c * v
	// Solution: v(t) = v(0) * e^(-c * t) or v2 = v1 * e^(-c * dt)
	// Taylor expansion of e^(-c * dt) = 1 - c * dt + ...
	// Since dt is usually in the order of 1/60 and c is a low number too this approximation is good enough
	mLinearVelocity *= max(0.0f, 1.0f - mLinearDamping * inDeltaTime);
	mAngularVelocity *= max(0.0f, 1.0f - mAngularDamping * inDeltaTime);

	// Clamp velocities
	ClampLinearVelocity();
	ClampAngularVelocity();
}

void MotionProperties::ResetSleepTestSpheres(const RVec3 *inPoints)
{
#ifdef JPH_DOUBLE_PRECISION
	// Make spheres relative to the first point and initialize them to zero radius
	DVec3 offset = inPoints[0];
	offset.StoreDouble3(&mSleepTestOffset);
	mSleepTestSpheres[0] = Sphere(Vec3::sZero(), 0.0f);
	for (int i = 1; i < 3; ++i)
		mSleepTestSpheres[i] = Sphere(Vec3(inPoints[i] - offset), 0.0f);
#else
	// Initialize the spheres to zero radius around the supplied points
	for (int i = 0; i < 3; ++i)
		mSleepTestSpheres[i] = Sphere(inPoints[i], 0.0f);
#endif

	mSleepTestTimer = 0.0f;
}

ECanSleep MotionProperties::AccumulateSleepTime(float inDeltaTime, float inTimeBeforeSleep)
{
	mSleepTestTimer += inDeltaTime;
	return mSleepTestTimer >= inTimeBeforeSleep? ECanSleep::CanSleep : ECanSleep::CannotSleep;
}

JPH_NAMESPACE_END

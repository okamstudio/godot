// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Jolt/Physics/Constraints/TwoBodyConstraint.h>
#include <Jolt/Physics/Constraints/PathConstraintPath.h>
#include <Jolt/Physics/Constraints/MotorSettings.h>
#include <Jolt/Physics/Constraints/ConstraintPart/AxisConstraintPart.h>
#include <Jolt/Physics/Constraints/ConstraintPart/DualAxisConstraintPart.h>
#include <Jolt/Physics/Constraints/ConstraintPart/HingeRotationConstraintPart.h>
#include <Jolt/Physics/Constraints/ConstraintPart/RotationEulerConstraintPart.h>

JPH_NAMESPACE_BEGIN

JPH_SUPPRESS_WARNING_PUSH
JPH_GCC_SUPPRESS_WARNING("-Wshadow") // GCC complains about the 'Free' value conflicting with the 'Free' method

/// How to constrain the rotation of the body to a PathConstraint
enum class EPathRotationConstraintType
{
	Free,							///< Do not constrain the rotation of the body at all
	ConstrainAroundTangent,			///< Only allow rotation around the tangent vector (following the path)
	ConstrainAroundNormal,			///< Only allow rotation around the normal vector (perpendicular to the path)
	ConstrainAroundBinormal,		///< Only allow rotation around the binormal vector (perpendicular to the path)
	ConstrainToPath,				///< Fully constrain the rotation of body 2 to the path (following the tangent and normal of the path)
	FullyConstrained,				///< Fully constrain the rotation of the body 2 to the rotation of body 1
};

JPH_SUPPRESS_WARNING_POP

/// Path constraint settings, used to constrain the degrees of freedom between two bodies to a path
///
/// The requirements of the path are that:
/// * Tangent, normal and bi-normal form an orthonormal basis with: tangent cross bi-normal = normal
/// * The path points along the tangent vector
/// * The path is continuous so doesn't contain any sharp corners
///
/// The reason for all this is that the constraint acts like a slider constraint with the sliding axis being the tangent vector (the assumption here is that delta time will be small enough so that the path is linear for that delta time).
class JPH_EXPORT PathConstraintSettings final : public TwoBodyConstraintSettings
{
	JPH_DECLARE_SERIALIZABLE_VIRTUAL(JPH_EXPORT, PathConstraintSettings)

public:
	// See: ConstraintSettings::SaveBinaryState
	virtual void					SaveBinaryState(StreamOut &inStream) const override;

	/// Create an instance of this constraint
	virtual TwoBodyConstraint *		Create(Body &inBody1, Body &inBody2) const override;

	/// The path that constrains the two bodies
	RefConst<PathConstraintPath>	mPath;

	/// The position of the path start relative to world transform of body 1
	Vec3							mPathPosition = Vec3::sZero();

	/// The rotation of the path start relative to world transform of body 1
	Quat							mPathRotation = Quat::sIdentity();

	/// The fraction along the path that corresponds to the initial position of body 2. Usually this is 0, the beginning of the path. But if you want to start an object halfway the path you can calculate this with mPath->GetClosestPoint(point on path to attach body to).
	float							mPathFraction = 0.0f;

	/// Maximum amount of friction force to apply (N) when not driven by a motor.
	float							mMaxFrictionForce = 0.0f;

	/// In case the constraint is powered, this determines the motor settings along the path
	MotorSettings					mPositionMotorSettings;

	/// How to constrain the rotation of the body to the path
	EPathRotationConstraintType		mRotationConstraintType = EPathRotationConstraintType::Free;

protected:
	// See: ConstraintSettings::RestoreBinaryState
	virtual void					RestoreBinaryState(StreamIn &inStream) override;
};

/// Path constraint, used to constrain the degrees of freedom between two bodies to a path
class JPH_EXPORT PathConstraint final : public TwoBodyConstraint
{
public:
	JPH_OVERRIDE_NEW_DELETE

	/// Construct point constraint
									PathConstraint(Body &inBody1, Body &inBody2, const PathConstraintSettings &inSettings);

	// Generic interface of a constraint
	virtual EConstraintSubType		GetSubType() const override								{ return EConstraintSubType::Path; }
	virtual void					NotifyShapeChanged(const BodyID &inBodyID, Vec3Arg inDeltaCOM) override;
	virtual void					SetupVelocityConstraint(float inDeltaTime) override;
	virtual void					ResetWarmStart() override;
	virtual void					WarmStartVelocityConstraint(float inWarmStartImpulseRatio) override;
	virtual bool					SolveVelocityConstraint(float inDeltaTime) override;
	virtual bool					SolvePositionConstraint(float inDeltaTime, float inBaumgarte) override;
#ifdef JPH_DEBUG_RENDERER
	virtual void					DrawConstraint(DebugRenderer *inRenderer) const override;
#endif // JPH_DEBUG_RENDERER
	virtual void					SaveState(StateRecorder &inStream) const override;
	virtual void					RestoreState(StateRecorder &inStream) override;
	virtual bool					IsActive() const override								{ return TwoBodyConstraint::IsActive() && mPath != nullptr; }
	virtual Ref<ConstraintSettings> GetConstraintSettings() const override;

	// See: TwoBodyConstraint
	virtual Mat44					GetConstraintToBody1Matrix() const override				{ return mPathToBody1; }
	virtual Mat44					GetConstraintToBody2Matrix() const override				{ return mPathToBody2; }

	/// Update the path for this constraint
	void							SetPath(const PathConstraintPath *inPath, float inPathFraction);

	/// Access to the current path
	const PathConstraintPath *		GetPath() const											{ return mPath; }

	/// Access to the current fraction along the path e [0, GetPath()->GetMaxPathFraction()]
	float							GetPathFraction() const									{ return mPathFraction; }

	/// Friction control
	void							SetMaxFrictionForce(float inFrictionForce)				{ mMaxFrictionForce = inFrictionForce; }
	float							GetMaxFrictionForce() const								{ return mMaxFrictionForce; }

	/// Position motor settings
	MotorSettings &					GetPositionMotorSettings()								{ return mPositionMotorSettings; }
	const MotorSettings &			GetPositionMotorSettings() const						{ return mPositionMotorSettings; }

	// Position motor controls (drives body 2 along the path)
	void							SetPositionMotorState(EMotorState inState)				{ JPH_ASSERT(inState == EMotorState::Off || mPositionMotorSettings.IsValid()); mPositionMotorState = inState; }
	EMotorState						GetPositionMotorState() const							{ return mPositionMotorState; }
	void							SetTargetVelocity(float inVelocity)						{ mTargetVelocity = inVelocity; }
	float							GetTargetVelocity() const								{ return mTargetVelocity; }
	void							SetTargetPathFraction(float inFraction)					{ JPH_ASSERT(mPath->IsLooping() || (inFraction >= 0.0f && inFraction <= mPath->GetPathMaxFraction())); mTargetPathFraction = inFraction; }
	float							GetTargetPathFraction() const							{ return mTargetPathFraction; }

	///@name Get Lagrange multiplier from last physics update (the linear/angular impulse applied to satisfy the constraint)
	inline Vector<2>				GetTotalLambdaPosition() const							{ return mPositionConstraintPart.GetTotalLambda(); }
	inline float					GetTotalLambdaPositionLimits() const					{ return mPositionLimitsConstraintPart.GetTotalLambda(); }
	inline float					GetTotalLambdaMotor() const								{ return mPositionMotorConstraintPart.GetTotalLambda(); }
	inline Vector<2>				GetTotalLambdaRotationHinge() const						{ return mHingeConstraintPart.GetTotalLambda(); }
	inline Vec3						GetTotalLambdaRotation() const							{ return mRotationConstraintPart.GetTotalLambda(); }

private:
	// Internal helper function to calculate the values below
	void							CalculateConstraintProperties(float inDeltaTime);

	// CONFIGURATION PROPERTIES FOLLOW

	RefConst<PathConstraintPath>	mPath;													///< The path that attaches the two bodies
	Mat44							mPathToBody1;											///< Transform that takes a quantity from path space to body 1 center of mass space
	Mat44							mPathToBody2;											///< Transform that takes a quantity from path space to body 2 center of mass space
	EPathRotationConstraintType		mRotationConstraintType;								///< How to constrain the rotation of the path

	// Friction
	float							mMaxFrictionForce;

	// Motor controls
	MotorSettings					mPositionMotorSettings;
	EMotorState						mPositionMotorState = EMotorState::Off;
	float							mTargetVelocity = 0.0f;
	float							mTargetPathFraction = 0.0f;

	// RUN TIME PROPERTIES FOLLOW

	// Positions where the point constraint acts on in world space
	Vec3							mR1;
	Vec3							mR2;

	// X2 + R2 - X1 - R1
	Vec3							mU;

	// World space path tangent
	Vec3							mPathTangent;

	// Normals to the path tangent
	Vec3							mPathNormal;
	Vec3							mPathBinormal;

	// Inverse of initial rotation from body 1 to body 2 in body 1 space (only used when rotation constraint type is FullyConstrained)
	Quat							mInvInitialOrientation;

	// Current fraction along the path where body 2 is attached
	float							mPathFraction = 0.0f;

	// Translation constraint parts
	DualAxisConstraintPart			mPositionConstraintPart;								///< Constraint part that keeps the movement along the tangent of the path
	AxisConstraintPart				mPositionLimitsConstraintPart;							///< Constraint part that prevents movement beyond the beginning and end of the path
	AxisConstraintPart				mPositionMotorConstraintPart;							///< Constraint to drive the object along the path or to apply friction

	// Rotation constraint parts
	HingeRotationConstraintPart		mHingeConstraintPart;									///< Constraint part that removes 2 degrees of rotation freedom
	RotationEulerConstraintPart		mRotationConstraintPart;								///< Constraint part that removes all rotational freedom
};

JPH_NAMESPACE_END

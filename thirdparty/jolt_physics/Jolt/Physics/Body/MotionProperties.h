// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Jolt/Geometry/Sphere.h>
#include <Jolt/Physics/Body/AllowedDOFs.h>
#include <Jolt/Physics/Body/MotionQuality.h>
#include <Jolt/Physics/Body/BodyAccess.h>
#include <Jolt/Physics/Body/MotionType.h>
#include <Jolt/Physics/Body/BodyType.h>
#include <Jolt/Physics/Body/MassProperties.h>
#include <Jolt/Physics/DeterminismLog.h>

JPH_NAMESPACE_BEGIN

class StateRecorder;

/// Enum that determines if an object can go to sleep
enum class ECanSleep
{
	CannotSleep = 0,																		///< Object cannot go to sleep
	CanSleep = 1,																			///< Object can go to sleep
};

/// The Body class only keeps track of state for static bodies, the MotionProperties class keeps the additional state needed for a moving Body. It has a 1-on-1 relationship with the body.
class JPH_EXPORT MotionProperties
{
public:
	JPH_OVERRIDE_NEW_DELETE

	/// Motion quality, or how well it detects collisions when it has a high velocity
	EMotionQuality			GetMotionQuality() const										{ return mMotionQuality; }

	/// Get the allowed degrees of freedom that this body has (this can be changed by calling SetMassProperties)
	inline EAllowedDOFs		GetAllowedDOFs() const											{ return mAllowedDOFs; }

	/// If this body can go to sleep.
	inline bool				GetAllowSleeping() const										{ return mAllowSleeping; }

	/// Get world space linear velocity of the center of mass
	inline Vec3				GetLinearVelocity() const										{ JPH_ASSERT(BodyAccess::sCheckRights(BodyAccess::sVelocityAccess(), BodyAccess::EAccess::Read)); return mLinearVelocity; }

	/// Set world space linear velocity of the center of mass
	void					SetLinearVelocity(Vec3Arg inLinearVelocity)						{ JPH_ASSERT(BodyAccess::sCheckRights(BodyAccess::sVelocityAccess(), BodyAccess::EAccess::ReadWrite)); JPH_ASSERT(inLinearVelocity.Length() <= mMaxLinearVelocity); mLinearVelocity = LockTranslation(inLinearVelocity); }

	/// Set world space linear velocity of the center of mass, will make sure the value is clamped against the maximum linear velocity
	void					SetLinearVelocityClamped(Vec3Arg inLinearVelocity)				{ mLinearVelocity = LockTranslation(inLinearVelocity); ClampLinearVelocity(); }

	/// Get world space angular velocity of the center of mass
	inline Vec3				GetAngularVelocity() const										{ JPH_ASSERT(BodyAccess::sCheckRights(BodyAccess::sVelocityAccess(), BodyAccess::EAccess::Read)); return mAngularVelocity; }

	/// Set world space angular velocity of the center of mass
	void					SetAngularVelocity(Vec3Arg inAngularVelocity)					{ JPH_ASSERT(BodyAccess::sCheckRights(BodyAccess::sVelocityAccess(), BodyAccess::EAccess::ReadWrite)); JPH_ASSERT(inAngularVelocity.Length() <= mMaxAngularVelocity); mAngularVelocity = LockAngular(inAngularVelocity); }

	/// Set world space angular velocity of the center of mass, will make sure the value is clamped against the maximum angular velocity
	void					SetAngularVelocityClamped(Vec3Arg inAngularVelocity)			{ mAngularVelocity = LockAngular(inAngularVelocity); ClampAngularVelocity(); }

	/// Set velocity of body such that it will be rotate/translate by inDeltaPosition/Rotation in inDeltaTime seconds.
	inline void				MoveKinematic(Vec3Arg inDeltaPosition, QuatArg inDeltaRotation, float inDeltaTime);

	///@name Velocity limits
	///@{

	/// Maximum linear velocity that a body can achieve. Used to prevent the system from exploding.
	inline float			GetMaxLinearVelocity() const									{ return mMaxLinearVelocity; }
	inline void				SetMaxLinearVelocity(float inLinearVelocity)					{ JPH_ASSERT(inLinearVelocity >= 0.0f); mMaxLinearVelocity = inLinearVelocity; }

	/// Maximum angular velocity that a body can achieve. Used to prevent the system from exploding.
	inline float			GetMaxAngularVelocity() const									{ return mMaxAngularVelocity; }
	inline void				SetMaxAngularVelocity(float inAngularVelocity)					{ JPH_ASSERT(inAngularVelocity >= 0.0f); mMaxAngularVelocity = inAngularVelocity; }
	///@}

	/// Clamp velocity according to limit
	inline void				ClampLinearVelocity();
	inline void				ClampAngularVelocity();

	/// Get linear damping: dv/dt = -c * v. c must be between 0 and 1 but is usually close to 0.
	inline float			GetLinearDamping() const										{ return mLinearDamping; }
	void					SetLinearDamping(float inLinearDamping)							{ JPH_ASSERT(inLinearDamping >= 0.0f); mLinearDamping = inLinearDamping; }

	/// Get angular damping: dw/dt = -c * w. c must be between 0 and 1 but is usually close to 0.
	inline float			GetAngularDamping() const										{ return mAngularDamping; }
	void					SetAngularDamping(float inAngularDamping)						{ JPH_ASSERT(inAngularDamping >= 0.0f); mAngularDamping = inAngularDamping; }

	/// Get gravity factor (1 = normal gravity, 0 = no gravity)
	inline float			GetGravityFactor() const										{ return mGravityFactor; }
	void					SetGravityFactor(float inGravityFactor)							{ mGravityFactor = inGravityFactor; }

	/// Set the mass and inertia tensor
	void					SetMassProperties(EAllowedDOFs inAllowedDOFs, const MassProperties &inMassProperties);

	/// Get inverse mass (1 / mass). Should only be called on a dynamic object (static or kinematic bodies have infinite mass so should be treated as 1 / mass = 0)
	inline float			GetInverseMass() const											{ JPH_ASSERT(mCachedMotionType == EMotionType::Dynamic); return mInvMass; }
	inline float			GetInverseMassUnchecked() const									{ return mInvMass; }

	/// Set the inverse mass (1 / mass).
	/// Note that mass and inertia are linearly related (e.g. inertia of a sphere with mass m and radius r is \f$2/5 \: m \: r^2\f$).
	/// If you change mass, inertia should probably change as well. You can use ScaleToMass to update mass and inertia at the same time.
	/// If all your translation degrees of freedom are restricted, make sure this is zero (see EAllowedDOFs).
	void					SetInverseMass(float inInverseMass)								{ mInvMass = inInverseMass; }

	/// Diagonal of inverse inertia matrix: D. Should only be called on a dynamic object (static or kinematic bodies have infinite mass so should be treated as D = 0)
	inline Vec3				GetInverseInertiaDiagonal() const								{ JPH_ASSERT(mCachedMotionType == EMotionType::Dynamic); return mInvInertiaDiagonal; }

	/// Rotation (R) that takes inverse inertia diagonal to local space: \f$I_{body}^{-1} = R \: D \: R^{-1}\f$
	inline Quat				GetInertiaRotation() const										{ return mInertiaRotation; }

	/// Set the inverse inertia tensor in local space by setting the diagonal and the rotation: \f$I_{body}^{-1} = R \: D \: R^{-1}\f$.
	/// Note that mass and inertia are linearly related (e.g. inertia of a sphere with mass m and radius r is \f$2/5 \: m \: r^2\f$).
	/// If you change inertia, mass should probably change as well. You can use ScaleToMass to update mass and inertia at the same time.
	/// If all your rotation degrees of freedom are restricted, make sure this is zero (see EAllowedDOFs).
	void					SetInverseInertia(Vec3Arg inDiagonal, QuatArg inRot)			{ mInvInertiaDiagonal = inDiagonal; mInertiaRotation = inRot; }

	/// Sets the mass to inMass and scale the inertia tensor based on the ratio between the old and new mass.
	/// Note that this only works when the current mass is finite (i.e. the body is dynamic and translational degrees of freedom are not restricted).
	void					ScaleToMass(float inMass);

	/// Get inverse inertia matrix (\f$I_{body}^{-1}\f$). Will be a matrix of zeros for a static or kinematic object.
	inline Mat44			GetLocalSpaceInverseInertia() const;

	/// Same as GetLocalSpaceInverseInertia() but doesn't check if the body is dynamic
	inline Mat44			GetLocalSpaceInverseInertiaUnchecked() const;

	/// Get inverse inertia matrix (\f$I^{-1}\f$) for a given object rotation (translation will be ignored). Zero if object is static or kinematic.
	inline Mat44			GetInverseInertiaForRotation(Mat44Arg inRotation) const;

	/// Multiply a vector with the inverse world space inertia tensor (\f$I_{world}^{-1}\f$). Zero if object is static or kinematic.
	JPH_INLINE Vec3			MultiplyWorldSpaceInverseInertiaByVector(QuatArg inBodyRotation, Vec3Arg inV) const;

	/// Velocity of point inPoint (in center of mass space, e.g. on the surface of the body) of the body (unit: m/s)
	JPH_INLINE Vec3			GetPointVelocityCOM(Vec3Arg inPointRelativeToCOM) const			{ return mLinearVelocity + mAngularVelocity.Cross(inPointRelativeToCOM); }

	// Get the total amount of force applied to the center of mass this time step (through Body::AddForce calls). Note that it will reset to zero after PhysicsSystem::Update.
	JPH_INLINE Vec3			GetAccumulatedForce() const										{ return Vec3::sLoadFloat3Unsafe(mForce); }

	// Get the total amount of torque applied to the center of mass this time step (through Body::AddForce/Body::AddTorque calls). Note that it will reset to zero after PhysicsSystem::Update.
	JPH_INLINE Vec3			GetAccumulatedTorque() const									{ return Vec3::sLoadFloat3Unsafe(mTorque); }

	// Reset the total accumulated force, note that this will be done automatically after every time step.
	JPH_INLINE void			ResetForce()													{ mForce = Float3(0, 0, 0); }

	// Reset the total accumulated torque, note that this will be done automatically after every time step.
	JPH_INLINE void			ResetTorque()													{ mTorque = Float3(0, 0, 0); }

	// Reset the current velocity and accumulated force and torque.
	JPH_INLINE void			ResetMotion()
	{
		JPH_ASSERT(BodyAccess::sCheckRights(BodyAccess::sVelocityAccess(), BodyAccess::EAccess::ReadWrite));
		mLinearVelocity = mAngularVelocity = Vec3::sZero();
		mForce = mTorque = Float3(0, 0, 0);
	}

	/// Returns a vector where the linear components that are not allowed by mAllowedDOFs are set to 0 and the rest to 0xffffffff
	JPH_INLINE UVec4		GetLinearDOFsMask() const
	{
		UVec4 mask(uint32(EAllowedDOFs::TranslationX), uint32(EAllowedDOFs::TranslationY), uint32(EAllowedDOFs::TranslationZ), 0);
		return UVec4::sEquals(UVec4::sAnd(UVec4::sReplicate(uint32(mAllowedDOFs)), mask), mask);
	}

	/// Takes a translation vector inV and returns a vector where the components that are not allowed by mAllowedDOFs are set to 0
	JPH_INLINE Vec3			LockTranslation(Vec3Arg inV) const
	{
		return Vec3::sAnd(inV, Vec3(GetLinearDOFsMask().ReinterpretAsFloat()));
	}

	/// Returns a vector where the angular components that are not allowed by mAllowedDOFs are set to 0 and the rest to 0xffffffff
	JPH_INLINE UVec4		GetAngularDOFsMask() const
	{
		UVec4 mask(uint32(EAllowedDOFs::RotationX), uint32(EAllowedDOFs::RotationY), uint32(EAllowedDOFs::RotationZ), 0);
		return UVec4::sEquals(UVec4::sAnd(UVec4::sReplicate(uint32(mAllowedDOFs)), mask), mask);
	}

	/// Takes an angular velocity / torque vector inV and returns a vector where the components that are not allowed by mAllowedDOFs are set to 0
	JPH_INLINE Vec3			LockAngular(Vec3Arg inV) const
	{
		return Vec3::sAnd(inV, Vec3(GetAngularDOFsMask().ReinterpretAsFloat()));
	}

	/// Used only when this body is dynamic and colliding. Override for the number of solver velocity iterations to run, 0 means use the default in PhysicsSettings::mNumVelocitySteps. The number of iterations to use is the max of all contacts and constraints in the island.
	void					SetNumVelocityStepsOverride(uint inN)							{ JPH_ASSERT(inN < 256); mNumVelocityStepsOverride = uint8(inN); }
	uint					GetNumVelocityStepsOverride() const								{ return mNumVelocityStepsOverride; }

	/// Used only when this body is dynamic and colliding. Override for the number of solver position iterations to run, 0 means use the default in PhysicsSettings::mNumPositionSteps. The number of iterations to use is the max of all contacts and constraints in the island.
	void					SetNumPositionStepsOverride(uint inN)							{ JPH_ASSERT(inN < 256); mNumPositionStepsOverride = uint8(inN); }
	uint					GetNumPositionStepsOverride() const								{ return mNumPositionStepsOverride; }

	////////////////////////////////////////////////////////////
	// FUNCTIONS BELOW THIS LINE ARE FOR INTERNAL USE ONLY
	////////////////////////////////////////////////////////////

	///@name Update linear and angular velocity (used during constraint solving)
	///@{
	inline void				AddLinearVelocityStep(Vec3Arg inLinearVelocityChange)			{ JPH_DET_LOG("AddLinearVelocityStep: " << inLinearVelocityChange); JPH_ASSERT(BodyAccess::sCheckRights(BodyAccess::sVelocityAccess(), BodyAccess::EAccess::ReadWrite)); mLinearVelocity = LockTranslation(mLinearVelocity + inLinearVelocityChange); JPH_ASSERT(!mLinearVelocity.IsNaN()); }
	inline void				SubLinearVelocityStep(Vec3Arg inLinearVelocityChange)			{ JPH_DET_LOG("SubLinearVelocityStep: " << inLinearVelocityChange); JPH_ASSERT(BodyAccess::sCheckRights(BodyAccess::sVelocityAccess(), BodyAccess::EAccess::ReadWrite)); mLinearVelocity = LockTranslation(mLinearVelocity - inLinearVelocityChange); JPH_ASSERT(!mLinearVelocity.IsNaN()); }
	inline void				AddAngularVelocityStep(Vec3Arg inAngularVelocityChange)			{ JPH_DET_LOG("AddAngularVelocityStep: " << inAngularVelocityChange); JPH_ASSERT(BodyAccess::sCheckRights(BodyAccess::sVelocityAccess(), BodyAccess::EAccess::ReadWrite)); mAngularVelocity += inAngularVelocityChange; JPH_ASSERT(!mAngularVelocity.IsNaN()); }
	inline void				SubAngularVelocityStep(Vec3Arg inAngularVelocityChange)			{ JPH_DET_LOG("SubAngularVelocityStep: " << inAngularVelocityChange); JPH_ASSERT(BodyAccess::sCheckRights(BodyAccess::sVelocityAccess(), BodyAccess::EAccess::ReadWrite)); mAngularVelocity -= inAngularVelocityChange; JPH_ASSERT(!mAngularVelocity.IsNaN()); }
	///@}

	/// Apply the gyroscopic force (aka Dzhanibekov effect, see https://en.wikipedia.org/wiki/Tennis_racket_theorem)
	inline void				ApplyGyroscopicForceInternal(QuatArg inBodyRotation, float inDeltaTime);

	/// Apply all accumulated forces, torques and drag (should only be called by the PhysicsSystem)
	inline void				ApplyForceTorqueAndDragInternal(QuatArg inBodyRotation, Vec3Arg inGravity, float inDeltaTime);

	/// Access to the island index
	uint32					GetIslandIndexInternal() const									{ return mIslandIndex; }
	void					SetIslandIndexInternal(uint32 inIndex)							{ mIslandIndex = inIndex; }

	/// Access to the index in the active bodies array
	uint32					GetIndexInActiveBodiesInternal() const							{ return mIndexInActiveBodies; }

#ifdef JPH_DOUBLE_PRECISION
	inline DVec3			GetSleepTestOffset() const										{ return DVec3::sLoadDouble3Unsafe(mSleepTestOffset); }
#endif // JPH_DOUBLE_PRECISION

	/// Reset spheres to center around inPoints with radius 0
	inline void				ResetSleepTestSpheres(const RVec3 *inPoints);

	/// Reset the sleep test timer without resetting the sleep test spheres
	inline void				ResetSleepTestTimer()											{ mSleepTestTimer = 0.0f; }

	/// Accumulate sleep time and return if a body can go to sleep
	inline ECanSleep		AccumulateSleepTime(float inDeltaTime, float inTimeBeforeSleep);

	/// Saving state for replay
	void					SaveState(StateRecorder &inStream) const;

	/// Restoring state for replay
	void					RestoreState(StateRecorder &inStream);

	static constexpr uint32	cInactiveIndex = uint32(-1);									///< Constant indicating that body is not active

private:
	friend class BodyManager;
	friend class Body;

	// 1st cache line
	// 16 byte aligned
	Vec3					mLinearVelocity { Vec3::sZero() };								///< World space linear velocity of the center of mass (m/s)
	Vec3					mAngularVelocity { Vec3::sZero() };								///< World space angular velocity (rad/s)
	Vec3					mInvInertiaDiagonal;											///< Diagonal of inverse inertia matrix: D
	Quat					mInertiaRotation;												///< Rotation (R) that takes inverse inertia diagonal to local space: Ibody^-1 = R * D * R^-1

	// 2nd cache line
	// 4 byte aligned
	Float3					mForce { 0, 0, 0 };												///< Accumulated world space force (N). Note loaded through intrinsics so ensure that the 4 bytes after this are readable!
	Float3					mTorque { 0, 0, 0 };											///< Accumulated world space torque (N m). Note loaded through intrinsics so ensure that the 4 bytes after this are readable!
	float					mInvMass;														///< Inverse mass of the object (1/kg)
	float					mLinearDamping;													///< Linear damping: dv/dt = -c * v. c must be between 0 and 1 but is usually close to 0.
	float					mAngularDamping;												///< Angular damping: dw/dt = -c * w. c must be between 0 and 1 but is usually close to 0.
	float					mMaxLinearVelocity;												///< Maximum linear velocity that this body can reach (m/s)
	float					mMaxAngularVelocity;											///< Maximum angular velocity that this body can reach (rad/s)
	float					mGravityFactor;													///< Factor to multiply gravity with
	uint32					mIndexInActiveBodies = cInactiveIndex;							///< If the body is active, this is the index in the active body list or cInactiveIndex if it is not active (note that there are 2 lists, one for rigid and one for soft bodies)
	uint32					mIslandIndex = cInactiveIndex;									///< Index of the island that this body is part of, when the body has not yet been updated or is not active this is cInactiveIndex

	// 1 byte aligned
	EMotionQuality			mMotionQuality;													///< Motion quality, or how well it detects collisions when it has a high velocity
	bool					mAllowSleeping;													///< If this body can go to sleep
	EAllowedDOFs			mAllowedDOFs = EAllowedDOFs::All;								///< Allowed degrees of freedom for this body
	uint8					mNumVelocityStepsOverride = 0;									///< Used only when this body is dynamic and colliding. Override for the number of solver velocity iterations to run, 0 means use the default in PhysicsSettings::mNumVelocitySteps. The number of iterations to use is the max of all contacts and constraints in the island.
	uint8					mNumPositionStepsOverride = 0;									///< Used only when this body is dynamic and colliding. Override for the number of solver position iterations to run, 0 means use the default in PhysicsSettings::mNumPositionSteps. The number of iterations to use is the max of all contacts and constraints in the island.

	// 3rd cache line (least frequently used)
	// 4 byte aligned (or 8 byte if running in double precision)
#ifdef JPH_DOUBLE_PRECISION
	Double3					mSleepTestOffset;												///< mSleepTestSpheres are relative to this offset to prevent floating point inaccuracies. Warning: Loaded using sLoadDouble3Unsafe which will read 8 extra bytes.
#endif // JPH_DOUBLE_PRECISION
	Sphere					mSleepTestSpheres[3];											///< Measure motion for 3 points on the body to see if it is resting: COM, COM + largest bounding box axis, COM + second largest bounding box axis
	float					mSleepTestTimer;												///< How long this body has been within the movement tolerance

#ifdef JPH_ENABLE_ASSERTS
	EBodyType				mCachedBodyType;												///< Copied from Body::mBodyType and cached for asserting purposes
	EMotionType				mCachedMotionType;												///< Copied from Body::mMotionType and cached for asserting purposes
#endif
};

JPH_NAMESPACE_END

#include "MotionProperties.inl"

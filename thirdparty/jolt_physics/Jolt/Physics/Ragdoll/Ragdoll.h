// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Jolt/Core/Reference.h>
#include <Jolt/Core/Result.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Constraints/TwoBodyConstraint.h>
#include <Jolt/Skeleton/Skeleton.h>
#include <Jolt/Skeleton/SkeletonPose.h>
#include <Jolt/Physics/EActivation.h>

JPH_NAMESPACE_BEGIN

class Ragdoll;
class PhysicsSystem;

/// Contains the structure of a ragdoll
class JPH_EXPORT RagdollSettings : public RefTarget<RagdollSettings>
{
	JPH_DECLARE_SERIALIZABLE_NON_VIRTUAL(JPH_EXPORT, RagdollSettings)

public:
	/// Stabilize the constraints of the ragdoll
	/// @return True on success, false on failure.
	bool								Stabilize();

	/// After the ragdoll has been fully configured, call this function to automatically create and add a GroupFilterTable collision filter to all bodies
	/// and configure them so that parent and children don't collide.
	///
	/// This will:
	/// - Create a GroupFilterTable and assign it to all of the bodies in a ragdoll.
	/// - Each body in your ragdoll will get a SubGroupID that is equal to the joint index in the Skeleton that it is attached to.
	/// - Loop over all joints in the Skeleton and call GroupFilterTable::DisableCollision(joint index, parent joint index).
	/// - When a pose is provided through inJointMatrices the function will detect collisions between joints
	/// (they must be separated by more than inMinSeparationDistance to be treated as not colliding) and automatically disable collisions.
	///
	/// When you create an instance using Ragdoll::CreateRagdoll pass in a unique GroupID for each ragdoll (e.g. a simple counter), note that this number
	/// should be unique throughout the PhysicsSystem, so if you have different types of ragdolls they should not share the same GroupID.
	void								DisableParentChildCollisions(const Mat44 *inJointMatrices = nullptr, float inMinSeparationDistance = 0.0f);

	/// Saves the state of this object in binary form to inStream.
	/// @param inStream The stream to save the state to
	/// @param inSaveShapes If the shapes should be saved as well (these could be shared between ragdolls, in which case the calling application may want to write custom code to restore them)
	/// @param inSaveGroupFilter If the group filter should be saved as well (these could be shared)
	void								SaveBinaryState(StreamOut &inStream, bool inSaveShapes, bool inSaveGroupFilter) const;

	using RagdollResult = Result<Ref<RagdollSettings>>;

	/// Restore a saved ragdoll from inStream
	static RagdollResult				sRestoreFromBinaryState(StreamIn &inStream);

	/// Create ragdoll instance from these settings
	/// @return Newly created ragdoll or null when out of bodies
	Ragdoll *							CreateRagdoll(CollisionGroup::GroupID inCollisionGroup, uint64 inUserData, PhysicsSystem *inSystem) const;

	/// Access to the skeleton of this ragdoll
	const Skeleton *					GetSkeleton() const												{ return mSkeleton; }
	Skeleton *							GetSkeleton()													{ return mSkeleton; }

	/// Calculate the map needed for GetBodyIndexToConstraintIndex()
	void								CalculateBodyIndexToConstraintIndex();

	/// Get table that maps a body index to the constraint index with which it is connected to its parent. -1 if there is no constraint associated with the body.
	/// Note that this will only tell you which constraint connects the body to its parent, it will not look in the additional constraint list.
	const Array<int> &					GetBodyIndexToConstraintIndex() const							{ return mBodyIndexToConstraintIndex; }

	/// Map a single body index to a constraint index
	int									GetConstraintIndexForBodyIndex(int inBodyIndex) const			{ return mBodyIndexToConstraintIndex[inBodyIndex]; }

	/// Calculate the map needed for GetConstraintIndexToBodyIdxPair()
	void								CalculateConstraintIndexToBodyIdxPair();

	using BodyIdxPair = std::pair<int, int>;

	/// Table that maps a constraint index (index in mConstraints) to the indices of the bodies that the constraint is connected to (index in mBodyIDs)
	const Array<BodyIdxPair> &			GetConstraintIndexToBodyIdxPair() const							{ return mConstraintIndexToBodyIdxPair; }

	/// Map a single constraint index (index in mConstraints) to the indices of the bodies that the constraint is connected to (index in mBodyIDs)
	BodyIdxPair							GetBodyIndicesForConstraintIndex(int inConstraintIndex) const	{ return mConstraintIndexToBodyIdxPair[inConstraintIndex]; }

	/// A single rigid body sub part of the ragdoll
	class Part : public BodyCreationSettings
	{
		JPH_DECLARE_SERIALIZABLE_NON_VIRTUAL(JPH_EXPORT, Part)

	public:
		Ref<TwoBodyConstraintSettings>	mToParent;
	};

	/// List of ragdoll parts
	using PartVector = Array<Part>;																	///< The constraint that connects this part to its parent part (should be null for the root)

	/// A constraint that connects two bodies in a ragdoll (for non parent child related constraints)
	class AdditionalConstraint
	{
		JPH_DECLARE_SERIALIZABLE_NON_VIRTUAL(JPH_EXPORT, AdditionalConstraint)

	public:
		/// Constructors
										AdditionalConstraint() = default;
										AdditionalConstraint(int inBodyIdx1, int inBodyIdx2, TwoBodyConstraintSettings *inConstraint) : mBodyIdx { inBodyIdx1, inBodyIdx2 }, mConstraint(inConstraint) { }

		int								mBodyIdx[2];												///< Indices of the bodies that this constraint connects
		Ref<TwoBodyConstraintSettings>	mConstraint;												///< The constraint that connects these bodies
	};

	/// List of additional constraints
	using AdditionalConstraintVector = Array<AdditionalConstraint>;

	/// The skeleton for this ragdoll
	Ref<Skeleton>						mSkeleton;

	/// For each of the joints, the body and constraint attaching it to its parent body (1-on-1 with mSkeleton.GetJoints())
	PartVector							mParts;

	/// A list of constraints that connects two bodies in a ragdoll (for non parent child related constraints)
	AdditionalConstraintVector			mAdditionalConstraints;

private:
	/// Table that maps a body index (index in mBodyIDs) to the constraint index with which it is connected to its parent. -1 if there is no constraint associated with the body.
	Array<int>							mBodyIndexToConstraintIndex;

	/// Table that maps a constraint index (index in mConstraints) to the indices of the bodies that the constraint is connected to (index in mBodyIDs)
	Array<BodyIdxPair>					mConstraintIndexToBodyIdxPair;
};

/// Runtime ragdoll information
class JPH_EXPORT Ragdoll : public RefTarget<Ragdoll>, public NonCopyable
{
public:
	JPH_OVERRIDE_NEW_DELETE

	/// Constructor
	explicit							Ragdoll(PhysicsSystem *inSystem) : mSystem(inSystem) { }

	/// Destructor
										~Ragdoll();

	/// Add bodies and constraints to the system and optionally activate the bodies
	void								AddToPhysicsSystem(EActivation inActivationMode, bool inLockBodies = true);

	/// Remove bodies and constraints from the system
	void								RemoveFromPhysicsSystem(bool inLockBodies = true);

	/// Wake up all bodies in the ragdoll
	void								Activate(bool inLockBodies = true);

	/// Check if one or more of the bodies in the ragdoll are active.
	/// Note that this involves locking the bodies (if inLockBodies is true) and looping over them. An alternative and possibly faster
	/// way could be to install a BodyActivationListener and count the number of active bodies of a ragdoll as they're activated / deactivated
	/// (basically check if the body that activates / deactivates is in GetBodyIDs() and increment / decrement a counter).
	bool								IsActive(bool inLockBodies = true) const;

	/// Set the group ID on all bodies in the ragdoll
	void								SetGroupID(CollisionGroup::GroupID inGroupID, bool inLockBodies = true);

	/// Set the ragdoll to a pose (calls BodyInterface::SetPositionAndRotation to instantly move the ragdoll)
	void								SetPose(const SkeletonPose &inPose, bool inLockBodies = true);

	/// Lower level version of SetPose that directly takes the world space joint matrices
	void								SetPose(RVec3Arg inRootOffset, const Mat44 *inJointMatrices, bool inLockBodies = true);

	/// Get the ragdoll pose (uses the world transform of the bodies to calculate the pose)
	void								GetPose(SkeletonPose &outPose, bool inLockBodies = true);

	/// Lower level version of GetPose that directly returns the world space joint matrices
	void								GetPose(RVec3 &outRootOffset, Mat44 *outJointMatrices, bool inLockBodies = true);

	/// This function calls ResetWarmStart on all constraints. It can be used after calling SetPose to reset previous frames impulses. See: Constraint::ResetWarmStart.
	void								ResetWarmStart();

	/// Drive the ragdoll to a specific pose by setting velocities on each of the bodies so that it will reach inPose in inDeltaTime
	void								DriveToPoseUsingKinematics(const SkeletonPose &inPose, float inDeltaTime, bool inLockBodies = true);

	/// Lower level version of DriveToPoseUsingKinematics that directly takes the world space joint matrices
	void								DriveToPoseUsingKinematics(RVec3Arg inRootOffset, const Mat44 *inJointMatrices, float inDeltaTime, bool inLockBodies = true);

	/// Drive the ragdoll to a specific pose by activating the motors on each constraint
	void								DriveToPoseUsingMotors(const SkeletonPose &inPose);

	/// Control the linear and velocity of all bodies in the ragdoll
	void								SetLinearAndAngularVelocity(Vec3Arg inLinearVelocity, Vec3Arg inAngularVelocity, bool inLockBodies = true);

	/// Set the world space linear velocity of all bodies in the ragdoll.
	void								SetLinearVelocity(Vec3Arg inLinearVelocity, bool inLockBodies = true);

	/// Add a world space velocity (in m/s) to all bodies in the ragdoll.
	void								AddLinearVelocity(Vec3Arg inLinearVelocity, bool inLockBodies = true);

	/// Add impulse to all bodies of the ragdoll (center of mass of each of them)
	void								AddImpulse(Vec3Arg inImpulse, bool inLockBodies = true);

	/// Get the position and orientation of the root of the ragdoll
	void								GetRootTransform(RVec3 &outPosition, Quat &outRotation, bool inLockBodies = true) const;

	/// Get number of bodies in the ragdoll
	size_t								GetBodyCount() const									{ return mBodyIDs.size(); }

	/// Access a body ID
	BodyID								GetBodyID(int inBodyIndex) const						{ return mBodyIDs[inBodyIndex]; }

	/// Access to the array of body IDs
	const Array<BodyID> &				GetBodyIDs() const										{ return mBodyIDs; }

	/// Get number of constraints in the ragdoll
	size_t								GetConstraintCount() const								{ return mConstraints.size(); }

	/// Access a constraint by index
	TwoBodyConstraint *					GetConstraint(int inConstraintIndex)					{ return mConstraints[inConstraintIndex]; }

	/// Access a constraint by index
	const TwoBodyConstraint *			GetConstraint(int inConstraintIndex) const				{ return mConstraints[inConstraintIndex]; }

	/// Get world space bounding box for all bodies of the ragdoll
	AABox								GetWorldSpaceBounds(bool inLockBodies = true) const;

	/// Get the settings object that created this ragdoll
	const RagdollSettings *				GetRagdollSettings() const								{ return mRagdollSettings; }

private:
	/// For RagdollSettings::CreateRagdoll function
	friend class RagdollSettings;

	/// The settings that created this ragdoll
	RefConst<RagdollSettings>			mRagdollSettings;

	/// The bodies and constraints that this ragdoll consists of (1-on-1 with mRagdollSettings->mParts)
	Array<BodyID>						mBodyIDs;

	/// Array of constraints that connect the bodies together
	Array<Ref<TwoBodyConstraint>>		mConstraints;

	/// Cached physics system
	PhysicsSystem *						mSystem;
};

JPH_NAMESPACE_END

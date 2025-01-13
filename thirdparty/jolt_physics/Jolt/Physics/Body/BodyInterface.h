// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Jolt/Physics/Body/BodyID.h>
#include <Jolt/Physics/EActivation.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>
#include <Jolt/Physics/Body/MotionType.h>
#include <Jolt/Physics/Body/MotionQuality.h>
#include <Jolt/Physics/Body/BodyType.h>
#include <Jolt/Core/Reference.h>

JPH_NAMESPACE_BEGIN

class Body;
class BodyCreationSettings;
class SoftBodyCreationSettings;
class BodyLockInterface;
class BroadPhase;
class BodyManager;
class TransformedShape;
class PhysicsMaterial;
class SubShapeID;
class Shape;
class TwoBodyConstraintSettings;
class TwoBodyConstraint;
class BroadPhaseLayerFilter;
class AABox;

/// Class that provides operations on bodies using a body ID. Note that if you need to do multiple operations on a single body, it is more efficient to lock the body once and combine the operations.
/// All quantities are in world space unless otherwise specified.
class JPH_EXPORT BodyInterface : public NonCopyable
{
public:
	/// Initialize the interface (should only be called by PhysicsSystem)
	void						Init(BodyLockInterface &inBodyLockInterface, BodyManager &inBodyManager, BroadPhase &inBroadPhase) { mBodyLockInterface = &inBodyLockInterface; mBodyManager = &inBodyManager; mBroadPhase = &inBroadPhase; }

	/// Create a rigid body
	/// @return Created body or null when out of bodies
	Body *						CreateBody(const BodyCreationSettings &inSettings);

	/// Create a soft body
	/// @return Created body or null when out of bodies
	Body *						CreateSoftBody(const SoftBodyCreationSettings &inSettings);

	/// Create a rigid body with specified ID. This function can be used if a simulation is to run in sync between clients or if a simulation needs to be restored exactly.
	/// The ID created on the server can be replicated to the client and used to create a deterministic simulation.
	/// @return Created body or null when the body ID is invalid or a body of the same ID already exists.
	Body *						CreateBodyWithID(const BodyID &inBodyID, const BodyCreationSettings &inSettings);

	/// Create a soft body with specified ID. See comments at CreateBodyWithID.
	Body *						CreateSoftBodyWithID(const BodyID &inBodyID, const SoftBodyCreationSettings &inSettings);

	/// Advanced use only. Creates a rigid body without specifying an ID. This body cannot be added to the physics system until it has been assigned a body ID.
	/// This can be used to decouple allocation from registering the body. A call to CreateBodyWithoutID followed by AssignBodyID is equivalent to calling CreateBodyWithID.
	/// @return Created body
	Body *						CreateBodyWithoutID(const BodyCreationSettings &inSettings) const;

	/// Advanced use only. Creates a body without specifying an ID. See comments at CreateBodyWithoutID.
	Body *						CreateSoftBodyWithoutID(const SoftBodyCreationSettings &inSettings) const;

	/// Advanced use only. Destroy a body previously created with CreateBodyWithoutID that hasn't gotten an ID yet through the AssignBodyID function,
	/// or a body that has had its body ID unassigned through UnassignBodyIDs. Bodies that have an ID should be destroyed through DestroyBody.
	void						DestroyBodyWithoutID(Body *inBody) const;

	/// Advanced use only. Assigns the next available body ID to a body that was created using CreateBodyWithoutID. After this call, the body can be added to the physics system.
	/// @return false if the body already has an ID or out of body ids.
	bool						AssignBodyID(Body *ioBody);

	/// Advanced use only. Assigns a body ID to a body that was created using CreateBodyWithoutID. After this call, the body can be added to the physics system.
	/// @return false if the body already has an ID or if the ID is not valid.
	bool						AssignBodyID(Body *ioBody, const BodyID &inBodyID);

	/// Advanced use only. See UnassignBodyIDs. Unassigns the ID of a single body.
	Body *						UnassignBodyID(const BodyID &inBodyID);

	/// Advanced use only. Removes a number of body IDs from their bodies and returns the body pointers. Before calling this, the body should have been removed from the physics system.
	/// The body can be destroyed through DestroyBodyWithoutID. This can be used to decouple deallocation. A call to UnassignBodyIDs followed by calls to DestroyBodyWithoutID is equivalent to calling DestroyBodies.
	/// @param inBodyIDs A list of body IDs
	/// @param inNumber Number of bodies in the list
	/// @param outBodies If not null on input, this will contain a list of body pointers corresponding to inBodyIDs that can be destroyed afterwards (caller assumes ownership over these).
	void						UnassignBodyIDs(const BodyID *inBodyIDs, int inNumber, Body **outBodies);

	/// Destroy a body.
	/// Make sure that you remove the body from the physics system using BodyInterface::RemoveBody before calling this function.
	void						DestroyBody(const BodyID &inBodyID);

	/// Destroy multiple bodies
	/// Make sure that you remove the bodies from the physics system using BodyInterface::RemoveBody before calling this function.
	void						DestroyBodies(const BodyID *inBodyIDs, int inNumber);

	/// Add body to the physics system.
	/// Note that if you need to add multiple bodies, use the AddBodiesPrepare/AddBodiesFinalize function.
	/// Adding many bodies, one at a time, results in a really inefficient broadphase until PhysicsSystem::OptimizeBroadPhase is called or when PhysicsSystem::Update rebuilds the tree!
	/// After adding, to get a body by ID use the BodyLockRead or BodyLockWrite interface!
	void						AddBody(const BodyID &inBodyID, EActivation inActivationMode);

	/// Remove body from the physics system.
	void						RemoveBody(const BodyID &inBodyID);

	/// Check if a body has been added to the physics system.
	bool						IsAdded(const BodyID &inBodyID) const;

	/// Combines CreateBody and AddBody
	/// @return Created body ID or an invalid ID when out of bodies
	BodyID						CreateAndAddBody(const BodyCreationSettings &inSettings, EActivation inActivationMode);

	/// Combines CreateSoftBody and AddBody
	/// @return Created body ID or an invalid ID when out of bodies
	BodyID						CreateAndAddSoftBody(const SoftBodyCreationSettings &inSettings, EActivation inActivationMode);

	/// Add state handle, used to keep track of a batch of bodies while adding them to the PhysicsSystem.
	using AddState = void *;

	///@name Batch adding interface
	///@{

	/// Prepare adding inNumber bodies at ioBodies to the PhysicsSystem, returns a handle that should be used in AddBodiesFinalize/Abort.
	/// This can be done on a background thread without influencing the PhysicsSystem.
	/// ioBodies may be shuffled around by this function and should be kept that way until AddBodiesFinalize/Abort is called.
	AddState					AddBodiesPrepare(BodyID *ioBodies, int inNumber);

	/// Finalize adding bodies to the PhysicsSystem, supply the return value of AddBodiesPrepare in inAddState.
	/// Please ensure that the ioBodies array passed to AddBodiesPrepare is unmodified and passed again to this function.
	void						AddBodiesFinalize(BodyID *ioBodies, int inNumber, AddState inAddState, EActivation inActivationMode);

	/// Abort adding bodies to the PhysicsSystem, supply the return value of AddBodiesPrepare in inAddState.
	/// This can be done on a background thread without influencing the PhysicsSystem.
	/// Please ensure that the ioBodies array passed to AddBodiesPrepare is unmodified and passed again to this function.
	void						AddBodiesAbort(BodyID *ioBodies, int inNumber, AddState inAddState);

	/// Remove inNumber bodies in ioBodies from the PhysicsSystem.
	/// ioBodies may be shuffled around by this function.
	void						RemoveBodies(BodyID *ioBodies, int inNumber);
	///@}

	///@name Activate / deactivate a body
	///@{
	void						ActivateBody(const BodyID &inBodyID);
	void						ActivateBodies(const BodyID *inBodyIDs, int inNumber);
	void						ActivateBodiesInAABox(const AABox &inBox, const BroadPhaseLayerFilter &inBroadPhaseLayerFilter, const ObjectLayerFilter &inObjectLayerFilter);
	void						DeactivateBody(const BodyID &inBodyID);
	void						DeactivateBodies(const BodyID *inBodyIDs, int inNumber);
	bool						IsActive(const BodyID &inBodyID) const;
	void						ResetSleepTimer(const BodyID &inBodyID);
	///@}

	/// Create a two body constraint
	TwoBodyConstraint *			CreateConstraint(const TwoBodyConstraintSettings *inSettings, const BodyID &inBodyID1, const BodyID &inBodyID2);

	/// Activate non-static bodies attached to a constraint
	void						ActivateConstraint(const TwoBodyConstraint *inConstraint);

	///@name Access to the shape of a body
	///@{

	/// Get the current shape
	RefConst<Shape>				GetShape(const BodyID &inBodyID) const;

	/// Set a new shape on the body
	/// @param inBodyID Body ID of body that had its shape changed
	/// @param inShape The new shape
	/// @param inUpdateMassProperties When true, the mass and inertia tensor is recalculated
	/// @param inActivationMode Whether or not to activate the body
	void						SetShape(const BodyID &inBodyID, const Shape *inShape, bool inUpdateMassProperties, EActivation inActivationMode) const;

	/// Notify all systems to indicate that a shape has changed (usable for MutableCompoundShapes)
	/// @param inBodyID Body ID of body that had its shape changed
	/// @param inPreviousCenterOfMass Center of mass of the shape before the alterations
	/// @param inUpdateMassProperties When true, the mass and inertia tensor is recalculated
	/// @param inActivationMode Whether or not to activate the body
	void						NotifyShapeChanged(const BodyID &inBodyID, Vec3Arg inPreviousCenterOfMass, bool inUpdateMassProperties, EActivation inActivationMode) const;
	///@}

	///@name Object layer of a body
	///@{
	void						SetObjectLayer(const BodyID &inBodyID, ObjectLayer inLayer);
	ObjectLayer					GetObjectLayer(const BodyID &inBodyID) const;
	///@}

	///@name Position and rotation of a body
	///@{
	void						SetPositionAndRotation(const BodyID &inBodyID, RVec3Arg inPosition, QuatArg inRotation, EActivation inActivationMode);
	void						SetPositionAndRotationWhenChanged(const BodyID &inBodyID, RVec3Arg inPosition, QuatArg inRotation, EActivation inActivationMode); ///< Will only update the position/rotation and activate the body when the difference is larger than a very small number. This avoids updating the broadphase/waking up a body when the resulting position/orientation doesn't really change.
	void						GetPositionAndRotation(const BodyID &inBodyID, RVec3 &outPosition, Quat &outRotation) const;
	void						SetPosition(const BodyID &inBodyID, RVec3Arg inPosition, EActivation inActivationMode);
	RVec3						GetPosition(const BodyID &inBodyID) const;
	RVec3						GetCenterOfMassPosition(const BodyID &inBodyID) const;
	void						SetRotation(const BodyID &inBodyID, QuatArg inRotation, EActivation inActivationMode);
	Quat						GetRotation(const BodyID &inBodyID) const;
	RMat44						GetWorldTransform(const BodyID &inBodyID) const;
	RMat44						GetCenterOfMassTransform(const BodyID &inBodyID) const;
	///@}

	/// Set velocity of body such that it will be positioned at inTargetPosition/Rotation in inDeltaTime seconds (will activate body if needed)
	void						MoveKinematic(const BodyID &inBodyID, RVec3Arg inTargetPosition, QuatArg inTargetRotation, float inDeltaTime);

	/// Linear or angular velocity (functions will activate body if needed).
	/// Note that the linear velocity is the velocity of the center of mass, which may not coincide with the position of your object, to correct for this: \f$VelocityCOM = Velocity - AngularVelocity \times ShapeCOM\f$
	void						SetLinearAndAngularVelocity(const BodyID &inBodyID, Vec3Arg inLinearVelocity, Vec3Arg inAngularVelocity);
	void						GetLinearAndAngularVelocity(const BodyID &inBodyID, Vec3 &outLinearVelocity, Vec3 &outAngularVelocity) const;
	void						SetLinearVelocity(const BodyID &inBodyID, Vec3Arg inLinearVelocity);
	Vec3						GetLinearVelocity(const BodyID &inBodyID) const;
	void						AddLinearVelocity(const BodyID &inBodyID, Vec3Arg inLinearVelocity); ///< Add velocity to current velocity
	void						AddLinearAndAngularVelocity(const BodyID &inBodyID, Vec3Arg inLinearVelocity, Vec3Arg inAngularVelocity); ///< Add linear and angular to current velocities
	void						SetAngularVelocity(const BodyID &inBodyID, Vec3Arg inAngularVelocity);
	Vec3						GetAngularVelocity(const BodyID &inBodyID) const;
	Vec3						GetPointVelocity(const BodyID &inBodyID, RVec3Arg inPoint) const; ///< Velocity of point inPoint (in world space, e.g. on the surface of the body) of the body

	/// Set the complete motion state of a body.
	/// Note that the linear velocity is the velocity of the center of mass, which may not coincide with the position of your object, to correct for this: \f$VelocityCOM = Velocity - AngularVelocity \times ShapeCOM\f$
	void						SetPositionRotationAndVelocity(const BodyID &inBodyID, RVec3Arg inPosition, QuatArg inRotation, Vec3Arg inLinearVelocity, Vec3Arg inAngularVelocity);

	///@name Add forces to the body
	///@{
	void						AddForce(const BodyID &inBodyID, Vec3Arg inForce, EActivation inActivationMode = EActivation::Activate); ///< See Body::AddForce
	void						AddForce(const BodyID &inBodyID, Vec3Arg inForce, RVec3Arg inPoint, EActivation inActivationMode = EActivation::Activate); ///< Applied at inPoint
	void						AddTorque(const BodyID &inBodyID, Vec3Arg inTorque, EActivation inActivationMode = EActivation::Activate); ///< See Body::AddTorque
	void						AddForceAndTorque(const BodyID &inBodyID, Vec3Arg inForce, Vec3Arg inTorque, EActivation inActivationMode = EActivation::Activate); ///< A combination of Body::AddForce and Body::AddTorque
	///@}

	///@name Add an impulse to the body
	///@{
	void						AddImpulse(const BodyID &inBodyID, Vec3Arg inImpulse); ///< Applied at center of mass
	void						AddImpulse(const BodyID &inBodyID, Vec3Arg inImpulse, RVec3Arg inPoint); ///< Applied at inPoint
	void						AddAngularImpulse(const BodyID &inBodyID, Vec3Arg inAngularImpulse);
	bool						ApplyBuoyancyImpulse(const BodyID &inBodyID, RVec3Arg inSurfacePosition, Vec3Arg inSurfaceNormal, float inBuoyancy, float inLinearDrag, float inAngularDrag, Vec3Arg inFluidVelocity, Vec3Arg inGravity, float inDeltaTime);
	///@}

	///@name Body type
	///@{
	EBodyType					GetBodyType(const BodyID &inBodyID) const;
	///@}

	///@name Body motion type
	///@{
	void						SetMotionType(const BodyID &inBodyID, EMotionType inMotionType, EActivation inActivationMode);
	EMotionType					GetMotionType(const BodyID &inBodyID) const;
	///@}

	///@name Body motion quality
	///@{
	void						SetMotionQuality(const BodyID &inBodyID, EMotionQuality inMotionQuality);
	EMotionQuality				GetMotionQuality(const BodyID &inBodyID) const;
	///@}

	/// Get inverse inertia tensor in world space
	Mat44						GetInverseInertia(const BodyID &inBodyID) const;

	///@name Restitution
	///@{
	void						SetRestitution(const BodyID &inBodyID, float inRestitution);
	float						GetRestitution(const BodyID &inBodyID) const;
	///@}

	///@name Friction
	///@{
	void						SetFriction(const BodyID &inBodyID, float inFriction);
	float						GetFriction(const BodyID &inBodyID) const;
	///@}

	///@name Gravity factor
	///@{
	void						SetGravityFactor(const BodyID &inBodyID, float inGravityFactor);
	float						GetGravityFactor(const BodyID &inBodyID) const;
	///@}

	///@name Manifold reduction
	///@{
	void						SetUseManifoldReduction(const BodyID &inBodyID, bool inUseReduction);
	bool						GetUseManifoldReduction(const BodyID &inBodyID) const;
	///@}

	/// Get transform and shape for this body, used to perform collision detection
	TransformedShape			GetTransformedShape(const BodyID &inBodyID) const;

	/// Get the user data for a body
	uint64						GetUserData(const BodyID &inBodyID) const;
	void						SetUserData(const BodyID &inBodyID, uint64 inUserData) const;

	/// Get the material for a particular sub shape
	const PhysicsMaterial *		GetMaterial(const BodyID &inBodyID, const SubShapeID &inSubShapeID) const;

	/// Set the Body::EFlags::InvalidateContactCache flag for the specified body. This means that the collision cache is invalid for any body pair involving that body until the next physics step.
	void						InvalidateContactCache(const BodyID &inBodyID);

private:
	/// Helper function to activate a single body
	JPH_INLINE void				ActivateBodyInternal(Body &ioBody) const;

	BodyLockInterface *			mBodyLockInterface = nullptr;
	BodyManager *				mBodyManager = nullptr;
	BroadPhase *				mBroadPhase = nullptr;
};

JPH_NAMESPACE_END

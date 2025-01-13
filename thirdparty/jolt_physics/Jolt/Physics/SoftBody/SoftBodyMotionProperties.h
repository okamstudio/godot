// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2023 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Jolt/Geometry/AABox.h>
#include <Jolt/Physics/Body/BodyID.h>
#include <Jolt/Physics/Body/MotionProperties.h>
#include <Jolt/Physics/Collision/TransformedShape.h>
#include <Jolt/Physics/SoftBody/SoftBodySharedSettings.h>
#include <Jolt/Physics/SoftBody/SoftBodyVertex.h>
#include <Jolt/Physics/SoftBody/SoftBodyUpdateContext.h>

JPH_NAMESPACE_BEGIN

class PhysicsSystem;
class BodyInterface;
class BodyLockInterface;
struct PhysicsSettings;
class Body;
class Shape;
class SoftBodyCreationSettings;
class TempAllocator;
#ifdef JPH_DEBUG_RENDERER
class DebugRenderer;
enum class ESoftBodyConstraintColor;
#endif // JPH_DEBUG_RENDERER

/// This class contains the runtime information of a soft body.
//
// Based on: XPBD, Extended Position Based Dynamics, Matthias Muller, Ten Minute Physics
// See: https://matthias-research.github.io/pages/tenMinutePhysics/09-xpbd.pdf
class JPH_EXPORT SoftBodyMotionProperties : public MotionProperties
{
public:
	using Vertex = SoftBodyVertex;
	using Edge = SoftBodySharedSettings::Edge;
	using Face = SoftBodySharedSettings::Face;
	using DihedralBend = SoftBodySharedSettings::DihedralBend;
	using Volume = SoftBodySharedSettings::Volume;
	using InvBind = SoftBodySharedSettings::InvBind;
	using SkinWeight = SoftBodySharedSettings::SkinWeight;
	using Skinned = SoftBodySharedSettings::Skinned;
	using LRA = SoftBodySharedSettings::LRA;

	/// Initialize the soft body motion properties
	void								Initialize(const SoftBodyCreationSettings &inSettings);

	/// Get the shared settings of the soft body
	const SoftBodySharedSettings *		GetSettings() const							{ return mSettings; }

	/// Get the vertices of the soft body
	const Array<Vertex> &				GetVertices() const							{ return mVertices; }
	Array<Vertex> &						GetVertices()								{ return mVertices; }

	/// Access an individual vertex
	const Vertex &						GetVertex(uint inIndex) const				{ return mVertices[inIndex]; }
	Vertex &							GetVertex(uint inIndex)						{ return mVertices[inIndex]; }

	/// Get the materials of the soft body
	const PhysicsMaterialList &			GetMaterials() const						{ return mSettings->mMaterials; }

	/// Get the faces of the soft body
	const Array<Face> &					GetFaces() const							{ return mSettings->mFaces; }

	/// Access to an individual face
	const Face &						GetFace(uint inIndex) const					{ return mSettings->mFaces[inIndex]; }

	/// Get the number of solver iterations
	uint32								GetNumIterations() const					{ return mNumIterations; }
	void								SetNumIterations(uint32 inNumIterations)	{ mNumIterations = inNumIterations; }

	/// Get the pressure of the soft body
	float								GetPressure() const							{ return mPressure; }
	void								SetPressure(float inPressure)				{ mPressure = inPressure; }

	/// Update the position of the body while simulating (set to false for something that is attached to the static world)
	bool								GetUpdatePosition() const					{ return mUpdatePosition; }
	void								SetUpdatePosition(bool inUpdatePosition)	{ mUpdatePosition = inUpdatePosition; }

	/// Global setting to turn on/off skin constraints
	bool								GetEnableSkinConstraints() const			{ return mEnableSkinConstraints; }
	void								SetEnableSkinConstraints(bool inEnableSkinConstraints) { mEnableSkinConstraints = inEnableSkinConstraints; }

	/// Multiplier applied to Skinned::mMaxDistance to allow tightening or loosening of the skin constraints. 0 to hard skin all vertices.
	float								GetSkinnedMaxDistanceMultiplier() const		{ return mSkinnedMaxDistanceMultiplier; }
	void								SetSkinnedMaxDistanceMultiplier(float inSkinnedMaxDistanceMultiplier) { mSkinnedMaxDistanceMultiplier = inSkinnedMaxDistanceMultiplier; }

	/// Get local bounding box
	const AABox &						GetLocalBounds() const						{ return mLocalBounds; }

	/// Get the volume of the soft body. Note can become negative if the shape is inside out!
	float								GetVolume() const							{ return GetVolumeTimesSix() / 6.0f; }

	/// Calculate the total mass and inertia of this body based on the current state of the vertices
	void								CalculateMassAndInertia();

#ifdef JPH_DEBUG_RENDERER
	/// Draw the state of a soft body
	void								DrawVertices(DebugRenderer *inRenderer, RMat44Arg inCenterOfMassTransform) const;
	void								DrawVertexVelocities(DebugRenderer *inRenderer, RMat44Arg inCenterOfMassTransform) const;
	void								DrawEdgeConstraints(DebugRenderer *inRenderer, RMat44Arg inCenterOfMassTransform, ESoftBodyConstraintColor inConstraintColor) const;
	void								DrawBendConstraints(DebugRenderer *inRenderer, RMat44Arg inCenterOfMassTransform, ESoftBodyConstraintColor inConstraintColor) const;
	void								DrawVolumeConstraints(DebugRenderer *inRenderer, RMat44Arg inCenterOfMassTransform, ESoftBodyConstraintColor inConstraintColor) const;
	void								DrawSkinConstraints(DebugRenderer *inRenderer, RMat44Arg inCenterOfMassTransform, ESoftBodyConstraintColor inConstraintColor) const;
	void								DrawLRAConstraints(DebugRenderer *inRenderer, RMat44Arg inCenterOfMassTransform, ESoftBodyConstraintColor inConstraintColor) const;
	void								DrawPredictedBounds(DebugRenderer *inRenderer, RMat44Arg inCenterOfMassTransform) const;
#endif // JPH_DEBUG_RENDERER

	/// Saving state for replay
	void								SaveState(StateRecorder &inStream) const;

	/// Restoring state for replay
	void								RestoreState(StateRecorder &inStream);

	/// Skin vertices to supplied joints, information is used by the skinned constraints.
	/// @param inCenterOfMassTransform Value of Body::GetCenterOfMassTransform().
	/// @param inJointMatrices The joint matrices must be expressed relative to inCenterOfMassTransform.
	/// @param inNumJoints Indicates how large the inJointMatrices array is (used only for validating out of bounds).
	/// @param inHardSkinAll Can be used to position all vertices on the skinned vertices and can be used to hard reset the soft body.
	/// @param ioTempAllocator Allocator.
	void								SkinVertices(RMat44Arg inCenterOfMassTransform, const Mat44 *inJointMatrices, uint inNumJoints, bool inHardSkinAll, TempAllocator &ioTempAllocator);

	/// This function allows you to update the soft body immediately without going through the PhysicsSystem.
	/// This is useful if the soft body is teleported and needs to 'settle' or it can be used if a the soft body
	/// is not added to the PhysicsSystem and needs to be updated manually. One reason for not adding it to the
	/// PhyicsSystem is that you might want to update a soft body immediately after updating an animated object
	/// that has the soft body attached to it. If the soft body is added to the PhysicsSystem it will be updated
	/// by it, so calling this function will effectively update it twice. Note that when you use this function,
	/// only the current thread will be used, whereas if you update through the PhysicsSystem, multiple threads may
	/// be used.
	/// Note that this will bypass any sleep checks. Since the dynamic objects that the soft body touches
	/// will not move during this call, there can be simulation artifacts if you call this function multiple times
	/// without running the physics simulation step.
	void								CustomUpdate(float inDeltaTime, Body &ioSoftBody, PhysicsSystem &inSystem);

	////////////////////////////////////////////////////////////
	// FUNCTIONS BELOW THIS LINE ARE FOR INTERNAL USE ONLY
	////////////////////////////////////////////////////////////

	/// Initialize the update context. Not part of the public API.
	void								InitializeUpdateContext(float inDeltaTime, Body &inSoftBody, const PhysicsSystem &inSystem, SoftBodyUpdateContext &ioContext);

	/// Do a broad phase check and collect all bodies that can possibly collide with this soft body. Not part of the public API.
	void								DetermineCollidingShapes(const SoftBodyUpdateContext &inContext, const PhysicsSystem &inSystem, const BodyLockInterface &inBodyLockInterface);

	/// Return code for ParallelUpdate
	enum class EStatus
	{
		NoWork	= 1 << 0,				///< No work was done because other threads were still working on a batch that cannot run concurrently
		DidWork	= 1 << 1,				///< Work was done to progress the update
		Done	= 1 << 2,				///< All work is done
	};

	/// Update the soft body, will process a batch of work. Not part of the public API.
	EStatus								ParallelUpdate(SoftBodyUpdateContext &ioContext, const PhysicsSettings &inPhysicsSettings);

	/// Update the velocities of all rigid bodies that we collided with. Not part of the public API.
	void								UpdateRigidBodyVelocities(const SoftBodyUpdateContext &inContext, BodyInterface &inBodyInterface);

private:
	// SoftBodyManifold needs to have access to CollidingShape
	friend class SoftBodyManifold;

	// Information about a leaf shape that we're colliding with
	struct LeafShape
	{
										LeafShape() = default;
										LeafShape(Mat44Arg inTransform, Vec3Arg inScale, const Shape *inShape) : mTransform(inTransform), mScale(inScale), mShape(inShape) { }

		Mat44							mTransform;									///< Transform of the shape relative to the soft body
		Vec3							mScale;										///< Scale of the shape
		RefConst<Shape>					mShape;										///< Shape
	};

	// Collect information about the colliding bodies
	struct CollidingShape
	{
		/// Get the velocity of a point on this body
		Vec3							GetPointVelocity(Vec3Arg inPointRelativeToCOM) const
		{
			return mLinearVelocity + mAngularVelocity.Cross(inPointRelativeToCOM);
		}

		Mat44							mCenterOfMassTransform;						///< Transform of the body relative to the soft body
		Array<LeafShape>				mShapes;									///< Leaf shapes of the body we hit
		BodyID							mBodyID;									///< Body ID of the body we hit
		EMotionType						mMotionType;								///< Motion type of the body we hit
		float							mInvMass;									///< Inverse mass of the body we hit
		float							mFriction;									///< Combined friction of the two bodies
		float							mRestitution;								///< Combined restitution of the two bodies
		float							mSoftBodyInvMassScale;						///< Scale factor for the inverse mass of the soft body vertices
		bool							mUpdateVelocities;							///< If the linear/angular velocity changed and the body needs to be updated
		Mat44							mInvInertia;								///< Inverse inertia in local space to the soft body
		Vec3							mLinearVelocity;							///< Linear velocity of the body in local space to the soft body
		Vec3							mAngularVelocity;							///< Angular velocity of the body in local space to the soft body
		Vec3							mOriginalLinearVelocity;					///< Linear velocity of the body in local space to the soft body at start
		Vec3							mOriginalAngularVelocity;					///< Angular velocity of the body in local space to the soft body at start
	};

	// Collect information about the colliding sensors
	struct CollidingSensor
	{
		Mat44							mCenterOfMassTransform;						///< Transform of the body relative to the soft body
		Array<LeafShape>				mShapes;									///< Leaf shapes of the body we hit
		BodyID							mBodyID;									///< Body ID of the body we hit
		bool							mHasContact;								///< If the sensor collided with the soft body
	};

	// Information about the state of all skinned vertices
	struct SkinState
	{
		Vec3							mPreviousPosition = Vec3::sZero();			///< Previous position of the skinned vertex, used to interpolate between the previous and current position
		Vec3							mPosition = Vec3::sNaN();					///< Current position of the skinned vertex
		Vec3							mNormal = Vec3::sNaN();						///< Normal of the skinned vertex
	};

	/// Do a narrow phase check and determine the closest feature that we can collide with
	void								DetermineCollisionPlanes(uint inVertexStart, uint inNumVertices);

	/// Do a narrow phase check between a single sensor and the soft body
	void								DetermineSensorCollisions(CollidingSensor &ioSensor);

	/// Apply pressure force and update the vertex velocities
	void								ApplyPressure(const SoftBodyUpdateContext &inContext);

	/// Integrate the positions of all vertices by 1 sub step
	void								IntegratePositions(const SoftBodyUpdateContext &inContext);

	/// Enforce all bend constraints
	void								ApplyDihedralBendConstraints(const SoftBodyUpdateContext &inContext, uint inStartIndex, uint inEndIndex);

	/// Enforce all volume constraints
	void								ApplyVolumeConstraints(const SoftBodyUpdateContext &inContext, uint inStartIndex, uint inEndIndex);

	/// Enforce all skin constraints
	void								ApplySkinConstraints(const SoftBodyUpdateContext &inContext, uint inStartIndex, uint inEndIndex);

	/// Enforce all edge constraints
	void								ApplyEdgeConstraints(const SoftBodyUpdateContext &inContext, uint inStartIndex, uint inEndIndex);

	/// Enforce all LRA constraints
	void								ApplyLRAConstraints(uint inStartIndex, uint inEndIndex);

	/// Enforce all collision constraints & update all velocities according the XPBD algorithm
	void								ApplyCollisionConstraintsAndUpdateVelocities(const SoftBodyUpdateContext &inContext);

	/// Update the state of the soft body (position, velocity, bounds)
	void								UpdateSoftBodyState(SoftBodyUpdateContext &ioContext, const PhysicsSettings &inPhysicsSettings);

	/// Start the first solver iteration
	void								StartFirstIteration(SoftBodyUpdateContext &ioContext);

	/// Executes tasks that need to run on the start of an iteration (i.e. the stuff that can't run in parallel)
	void								StartNextIteration(const SoftBodyUpdateContext &ioContext);

	/// Helper function for ParallelUpdate that works on batches of collision planes
	EStatus								ParallelDetermineCollisionPlanes(SoftBodyUpdateContext &ioContext);

	/// Helper function for ParallelUpdate that works on sensor collisions
	EStatus								ParallelDetermineSensorCollisions(SoftBodyUpdateContext &ioContext);

	/// Helper function for ParallelUpdate that works on batches of constraints
	EStatus								ParallelApplyConstraints(SoftBodyUpdateContext &ioContext, const PhysicsSettings &inPhysicsSettings);

	/// Helper function to update a single group of constraints
	void								ProcessGroup(const SoftBodyUpdateContext &ioContext, uint inGroupIndex);

	/// Returns 6 times the volume of the soft body
	float								GetVolumeTimesSix() const;

#ifdef JPH_DEBUG_RENDERER
	/// Helper function to draw constraints
	template <typename GetEndIndex, typename DrawConstraint>
		inline void						DrawConstraints(ESoftBodyConstraintColor inConstraintColor, const GetEndIndex &inGetEndIndex, const DrawConstraint &inDrawConstraint, ColorArg inBaseColor) const;

	RMat44								mSkinStateTransform = RMat44::sIdentity();	///< The matrix that transforms mSkinState to world space
#endif // JPH_DEBUG_RENDERER

	RefConst<SoftBodySharedSettings>	mSettings;									///< Configuration of the particles and constraints
	Array<Vertex>						mVertices;									///< Current state of all vertices in the simulation
	Array<CollidingShape>				mCollidingShapes;							///< List of colliding shapes retrieved during the last update
	Array<CollidingSensor>				mCollidingSensors;							///< List of colliding sensors retrieved during the last update
	Array<SkinState>					mSkinState;									///< List of skinned positions (1-on-1 with mVertices but only those that are used by the skinning constraints are filled in)
	AABox								mLocalBounds;								///< Bounding box of all vertices
	AABox								mLocalPredictedBounds;						///< Predicted bounding box for all vertices using extrapolation of velocity by last step delta time
	uint32								mNumIterations;								///< Number of solver iterations
	float								mPressure;									///< n * R * T, amount of substance * ideal gas constant * absolute temperature, see https://en.wikipedia.org/wiki/Pressure
	float								mSkinnedMaxDistanceMultiplier = 1.0f;		///< Multiplier applied to Skinned::mMaxDistance to allow tightening or loosening of the skin constraints
	bool								mUpdatePosition;							///< Update the position of the body while simulating (set to false for something that is attached to the static world)
	bool								mNeedContactCallback = false;						///< True if the soft body has collided with anything in the last update
	bool								mEnableSkinConstraints = true;				///< If skin constraints are enabled
	bool								mSkinStatePreviousPositionValid = false;	///< True if the skinning was updated in the last update so that the previous position of the skin state is valid
};

JPH_NAMESPACE_END

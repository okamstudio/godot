// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2023 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Jolt/Core/NonCopyable.h>
#include <Jolt/Physics/Body/MotionProperties.h>

JPH_NAMESPACE_BEGIN

class Body;
class SoftBodyMotionProperties;
class SoftBodyContactListener;
class SimShapeFilter;

/// Temporary data used by the update of a soft body
class SoftBodyUpdateContext : public NonCopyable
{
public:
	static constexpr uint				cVertexCollisionBatch = 64;					///< Number of vertices to process in a batch in DetermineCollisionPlanes
	static constexpr uint				cVertexConstraintBatch = 256;				///< Number of vertices to group for processing batches of constraints in ApplyEdgeConstraints

	// Input
	Body *								mBody;										///< Body that is being updated
	SoftBodyMotionProperties *			mMotionProperties;							///< Motion properties of that body
	SoftBodyContactListener *			mContactListener;							///< Contact listener to fire callbacks to
	const SimShapeFilter *				mSimShapeFilter;							///< Shape filter to use for collision detection
	RMat44								mCenterOfMassTransform;						///< Transform of the body relative to the soft body
	Vec3								mGravity;									///< Gravity vector in local space of the soft body
	Vec3								mDisplacementDueToGravity;					///< Displacement of the center of mass due to gravity in the current time step
	float								mDeltaTime;									///< Delta time for the current time step
	float								mSubStepDeltaTime;							///< Delta time for each sub step

	/// Describes progress in the current update
	enum class EState
	{
		DetermineCollisionPlanes,													///< Determine collision planes for vertices in parallel
		DetermineSensorCollisions,													///< Determine collisions with sensors in parallel
		ApplyConstraints,															///< Apply constraints in parallel
		Done																		///< Update is finished
	};

	// State of the update
	atomic<EState>						mState { EState::DetermineCollisionPlanes };///< Current state of the update
	atomic<uint>						mNextCollisionVertex { 0 };					///< Next vertex to process for DetermineCollisionPlanes
	atomic<uint>						mNumCollisionVerticesProcessed { 0 };		///< Number of vertices processed by DetermineCollisionPlanes, used to determine if we can go to the next step
	atomic<uint>						mNextSensorIndex { 0 };						///< Next sensor to process for DetermineCollisionPlanes
	atomic<uint>						mNumSensorsProcessed { 0 };					///< Number of sensors processed by DetermineSensorCollisions, used to determine if we can go to the next step
	atomic<uint>						mNextIteration { 0 };						///< Next simulation iteration to process
	atomic<uint>						mNextConstraintGroup { 0 };					///< Next constraint group to process
	atomic<uint>						mNumConstraintGroupsProcessed { 0 };		///< Number of groups processed, used to determine if we can go to the next iteration

	// Output
	Vec3								mDeltaPosition;								///< Delta position of the body in the current time step, should be applied after the update
	ECanSleep							mCanSleep;									///< Can the body sleep? Should be applied after the update
};

JPH_NAMESPACE_END

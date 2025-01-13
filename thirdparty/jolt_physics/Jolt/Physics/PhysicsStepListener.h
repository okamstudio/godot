// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

JPH_NAMESPACE_BEGIN

class PhysicsSystem;

/// Context information for the step listener
class JPH_EXPORT PhysicsStepListenerContext
{
public:
	float					mDeltaTime;								///< Delta time of the current step
	bool					mIsFirstStep;							///< True if this is the first step
	bool					mIsLastStep;							///< True if this is the last step
	PhysicsSystem *			mPhysicsSystem;							///< The physics system that is being stepped
};

/// A listener class that receives a callback before every physics simulation step
class JPH_EXPORT PhysicsStepListener
{
public:
	/// Ensure virtual destructor
	virtual					~PhysicsStepListener() = default;

	/// Called before every simulation step (received inCollisionSteps times for every PhysicsSystem::Update(...) call)
	/// This is called while all body and constraint mutexes are locked. You can read/write bodies and constraints but not add/remove them.
	/// Multiple listeners can be executed in parallel and it is the responsibility of the listener to avoid race conditions.
	/// The best way to do this is to have each step listener operate on a subset of the bodies and constraints
	/// and making sure that these bodies and constraints are not touched by any other step listener.
	/// Note that this function is not called if there aren't any active bodies or when the physics system is updated with 0 delta time.
	virtual void			OnStep(const PhysicsStepListenerContext &inContext) = 0;
};

JPH_NAMESPACE_END

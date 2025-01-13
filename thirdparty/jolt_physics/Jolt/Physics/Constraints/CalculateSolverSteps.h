// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2023 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Jolt/Physics/PhysicsSettings.h>

JPH_NAMESPACE_BEGIN

/// Class used to calculate the total number of velocity and position steps
class CalculateSolverSteps
{
public:
	/// Constructor
	JPH_INLINE explicit			CalculateSolverSteps(const PhysicsSettings &inSettings) : mSettings(inSettings) { }

	/// Combine the number of velocity and position steps for this body/constraint with the current values
	template <class Type>
	JPH_INLINE void				operator () (const Type *inObject)
	{
		uint num_velocity_steps = inObject->GetNumVelocityStepsOverride();
		mNumVelocitySteps = max(mNumVelocitySteps, num_velocity_steps);
		mApplyDefaultVelocity |= num_velocity_steps == 0;

		uint num_position_steps = inObject->GetNumPositionStepsOverride();
		mNumPositionSteps = max(mNumPositionSteps, num_position_steps);
		mApplyDefaultPosition |= num_position_steps == 0;
	}

	/// Must be called after all bodies/constraints have been processed
	JPH_INLINE void				Finalize()
	{
		// If we have a default velocity/position step count, take the max of the default and the overrides
		if (mApplyDefaultVelocity)
			mNumVelocitySteps = max(mNumVelocitySteps, mSettings.mNumVelocitySteps);
		if (mApplyDefaultPosition)
			mNumPositionSteps = max(mNumPositionSteps, mSettings.mNumPositionSteps);
	}

	/// Get the results of the calculation
	JPH_INLINE uint				GetNumPositionSteps() const					{ return mNumPositionSteps; }
	JPH_INLINE uint				GetNumVelocitySteps() const					{ return mNumVelocitySteps; }

private:
	const PhysicsSettings &		mSettings;

	uint						mNumVelocitySteps = 0;
	uint						mNumPositionSteps = 0;

	bool						mApplyDefaultVelocity = false;
	bool						mApplyDefaultPosition = false;
};

/// Dummy class to replace the steps calculator when we don't need the result
class DummyCalculateSolverSteps
{
public:
	template <class Type>
	JPH_INLINE void				operator () (const Type *) const
	{
		/* Nothing to do */
	}
};

JPH_NAMESPACE_END

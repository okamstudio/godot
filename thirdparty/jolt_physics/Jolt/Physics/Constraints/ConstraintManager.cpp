// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#include <Jolt/Jolt.h>

#include <Jolt/Physics/Constraints/ConstraintManager.h>
#include <Jolt/Physics/Constraints/CalculateSolverSteps.h>
#include <Jolt/Physics/IslandBuilder.h>
#include <Jolt/Physics/StateRecorder.h>
#include <Jolt/Physics/PhysicsLock.h>
#include <Jolt/Core/Profiler.h>
#include <Jolt/Core/QuickSort.h>

JPH_NAMESPACE_BEGIN

void ConstraintManager::Add(Constraint **inConstraints, int inNumber)
{
	UniqueLock lock(mConstraintsMutex JPH_IF_ENABLE_ASSERTS(, mLockContext, EPhysicsLockTypes::ConstraintsList));

	mConstraints.reserve(mConstraints.size() + inNumber);

	for (Constraint **c = inConstraints, **c_end = inConstraints + inNumber; c < c_end; ++c)
	{
		Constraint *constraint = *c;

		// Assume this constraint has not been added yet
		JPH_ASSERT(constraint->mConstraintIndex == Constraint::cInvalidConstraintIndex);

		// Add to the list
		constraint->mConstraintIndex = uint32(mConstraints.size());
		mConstraints.push_back(constraint);
	}
}

void ConstraintManager::Remove(Constraint **inConstraints, int inNumber)
{
	UniqueLock lock(mConstraintsMutex JPH_IF_ENABLE_ASSERTS(, mLockContext, EPhysicsLockTypes::ConstraintsList));

	for (Constraint **c = inConstraints, **c_end = inConstraints + inNumber; c < c_end; ++c)
	{
		Constraint *constraint = *c;

		// Reset constraint index for this constraint
		uint32 this_constraint_idx = constraint->mConstraintIndex;
		constraint->mConstraintIndex = Constraint::cInvalidConstraintIndex;
		JPH_ASSERT(this_constraint_idx != Constraint::cInvalidConstraintIndex);

		// Check if this constraint is somewhere in the middle of the constraints, in this case we need to move the last constraint to this position
		uint32 last_constraint_idx = uint32(mConstraints.size() - 1);
		if (this_constraint_idx < last_constraint_idx)
		{
			Constraint *last_constraint = mConstraints[last_constraint_idx];
			last_constraint->mConstraintIndex = this_constraint_idx;
			mConstraints[this_constraint_idx] = last_constraint;
		}

		// Pop last constraint
		mConstraints.pop_back();
	}
}

Constraints ConstraintManager::GetConstraints() const
{
	UniqueLock lock(mConstraintsMutex JPH_IF_ENABLE_ASSERTS(, mLockContext, EPhysicsLockTypes::ConstraintsList));

	Constraints copy = mConstraints;
	return copy;
}

void ConstraintManager::GetActiveConstraints(uint32 inStartConstraintIdx, uint32 inEndConstraintIdx, Constraint **outActiveConstraints, uint32 &outNumActiveConstraints) const
{
	JPH_PROFILE_FUNCTION();

	JPH_ASSERT(inEndConstraintIdx <= mConstraints.size());

	uint32 num_active_constraints = 0;
	for (uint32 constraint_idx = inStartConstraintIdx; constraint_idx < inEndConstraintIdx; ++constraint_idx)
	{
		Constraint *c = mConstraints[constraint_idx];
		JPH_ASSERT(c->mConstraintIndex == constraint_idx);
		if (c->IsActive())
		{
			*(outActiveConstraints++) = c;
			num_active_constraints++;
		}
	}

	outNumActiveConstraints = num_active_constraints;
}

void ConstraintManager::sBuildIslands(Constraint **inActiveConstraints, uint32 inNumActiveConstraints, IslandBuilder &ioBuilder, BodyManager &inBodyManager)
{
	JPH_PROFILE_FUNCTION();

	for (uint32 constraint_idx = 0; constraint_idx < inNumActiveConstraints; ++constraint_idx)
	{
		Constraint *c = inActiveConstraints[constraint_idx];
		c->BuildIslands(constraint_idx, ioBuilder, inBodyManager);
	}
}

void ConstraintManager::sSortConstraints(Constraint **inActiveConstraints, uint32 *inConstraintIdxBegin, uint32 *inConstraintIdxEnd)
{
	JPH_PROFILE_FUNCTION();

	QuickSort(inConstraintIdxBegin, inConstraintIdxEnd, [inActiveConstraints](uint32 inLHS, uint32 inRHS) {
		const Constraint *lhs = inActiveConstraints[inLHS];
		const Constraint *rhs = inActiveConstraints[inRHS];

		if (lhs->GetConstraintPriority() != rhs->GetConstraintPriority())
			return lhs->GetConstraintPriority() < rhs->GetConstraintPriority();

		return lhs->mConstraintIndex < rhs->mConstraintIndex;
	});
}

void ConstraintManager::sSetupVelocityConstraints(Constraint **inActiveConstraints, uint32 inNumActiveConstraints, float inDeltaTime)
{
	JPH_PROFILE_FUNCTION();

	for (Constraint **c = inActiveConstraints, **c_end = inActiveConstraints + inNumActiveConstraints; c < c_end; ++c)
		(*c)->SetupVelocityConstraint(inDeltaTime);
}

template <class ConstraintCallback>
void ConstraintManager::sWarmStartVelocityConstraints(Constraint **inActiveConstraints, const uint32 *inConstraintIdxBegin, const uint32 *inConstraintIdxEnd, float inWarmStartImpulseRatio, ConstraintCallback &ioCallback)
{
	JPH_PROFILE_FUNCTION();

	for (const uint32 *constraint_idx = inConstraintIdxBegin; constraint_idx < inConstraintIdxEnd; ++constraint_idx)
	{
		Constraint *c = inActiveConstraints[*constraint_idx];
		ioCallback(c);
		c->WarmStartVelocityConstraint(inWarmStartImpulseRatio);
	}
}

// Specialize for the two constraint callback types
template void ConstraintManager::sWarmStartVelocityConstraints<CalculateSolverSteps>(Constraint **inActiveConstraints, const uint32 *inConstraintIdxBegin, const uint32 *inConstraintIdxEnd, float inWarmStartImpulseRatio, CalculateSolverSteps &ioCallback);
template void ConstraintManager::sWarmStartVelocityConstraints<DummyCalculateSolverSteps>(Constraint **inActiveConstraints, const uint32 *inConstraintIdxBegin, const uint32 *inConstraintIdxEnd, float inWarmStartImpulseRatio, DummyCalculateSolverSteps &ioCallback);

bool ConstraintManager::sSolveVelocityConstraints(Constraint **inActiveConstraints, const uint32 *inConstraintIdxBegin, const uint32 *inConstraintIdxEnd, float inDeltaTime)
{
	JPH_PROFILE_FUNCTION();

	bool any_impulse_applied = false;

	for (const uint32 *constraint_idx = inConstraintIdxBegin; constraint_idx < inConstraintIdxEnd; ++constraint_idx)
	{
		Constraint *c = inActiveConstraints[*constraint_idx];
		any_impulse_applied |= c->SolveVelocityConstraint(inDeltaTime);
	}

	return any_impulse_applied;
}

bool ConstraintManager::sSolvePositionConstraints(Constraint **inActiveConstraints, const uint32 *inConstraintIdxBegin, const uint32 *inConstraintIdxEnd, float inDeltaTime, float inBaumgarte)
{
	JPH_PROFILE_FUNCTION();

	bool any_impulse_applied = false;

	for (const uint32 *constraint_idx = inConstraintIdxBegin; constraint_idx < inConstraintIdxEnd; ++constraint_idx)
	{
		Constraint *c = inActiveConstraints[*constraint_idx];
		any_impulse_applied |= c->SolvePositionConstraint(inDeltaTime, inBaumgarte);
	}

	return any_impulse_applied;
}

#ifdef JPH_DEBUG_RENDERER
void ConstraintManager::DrawConstraints(DebugRenderer *inRenderer) const
{
	JPH_PROFILE_FUNCTION();

	UniqueLock lock(mConstraintsMutex JPH_IF_ENABLE_ASSERTS(, mLockContext, EPhysicsLockTypes::ConstraintsList));

	for (const Ref<Constraint> &c : mConstraints)
		c->DrawConstraint(inRenderer);
}

void ConstraintManager::DrawConstraintLimits(DebugRenderer *inRenderer) const
{
	JPH_PROFILE_FUNCTION();

	UniqueLock lock(mConstraintsMutex JPH_IF_ENABLE_ASSERTS(, mLockContext, EPhysicsLockTypes::ConstraintsList));

	for (const Ref<Constraint> &c : mConstraints)
		c->DrawConstraintLimits(inRenderer);
}

void ConstraintManager::DrawConstraintReferenceFrame(DebugRenderer *inRenderer) const
{
	JPH_PROFILE_FUNCTION();

	UniqueLock lock(mConstraintsMutex JPH_IF_ENABLE_ASSERTS(, mLockContext, EPhysicsLockTypes::ConstraintsList));

	for (const Ref<Constraint> &c : mConstraints)
		c->DrawConstraintReferenceFrame(inRenderer);
}
#endif // JPH_DEBUG_RENDERER

void ConstraintManager::SaveState(StateRecorder &inStream, const StateRecorderFilter *inFilter) const
{
	UniqueLock lock(mConstraintsMutex JPH_IF_ENABLE_ASSERTS(, mLockContext, EPhysicsLockTypes::ConstraintsList));

	// Write state of constraints
	if (inFilter != nullptr)
	{
		// Determine which constraints to save
		Array<Constraint *> constraints;
		constraints.reserve(mConstraints.size());
		for (const Ref<Constraint> &c : mConstraints)
			if (inFilter->ShouldSaveConstraint(*c))
				constraints.push_back(c);

		// Save them
		uint32 num_constraints = (uint32)constraints.size();
		inStream.Write(num_constraints);
		for (const Constraint *c : constraints)
		{
			inStream.Write(c->mConstraintIndex);
			c->SaveState(inStream);
		}
	}
	else
	{
		// Save all constraints
		uint32 num_constraints = (uint32)mConstraints.size();
		inStream.Write(num_constraints);
		for (const Ref<Constraint> &c : mConstraints)
		{
			inStream.Write(c->mConstraintIndex);
			c->SaveState(inStream);
		}
	}
}

bool ConstraintManager::RestoreState(StateRecorder &inStream)
{
	UniqueLock lock(mConstraintsMutex JPH_IF_ENABLE_ASSERTS(, mLockContext, EPhysicsLockTypes::ConstraintsList));

	if (inStream.IsValidating())
	{
		// Read state of constraints
		uint32 num_constraints = (uint32)mConstraints.size(); // Initialize to current value for validation
		inStream.Read(num_constraints);
		if (num_constraints != mConstraints.size())
		{
			JPH_ASSERT(false, "Cannot handle adding/removing constraints");
			return false;
		}
		for (const Ref<Constraint> &c : mConstraints)
		{
			uint32 constraint_index = c->mConstraintIndex;
			inStream.Read(constraint_index);
			if (constraint_index != c->mConstraintIndex)
			{
				JPH_ASSERT(false, "Unexpected constraint index");
				return false;
			}
			c->RestoreState(inStream);
		}
	}
	else
	{
		// Not validating, use more flexible reading, read number of constraints
		uint32 num_constraints = 0;
		inStream.Read(num_constraints);

		for (uint32 idx = 0; idx < num_constraints; ++idx)
		{
			uint32 constraint_index;
			inStream.Read(constraint_index);
			if (mConstraints.size() <= constraint_index)
			{
				JPH_ASSERT(false, "Restoring state for non-existing constraint");
				return false;
			}
			mConstraints[constraint_index]->RestoreState(inStream);
		}
	}

	return true;
}

JPH_NAMESPACE_END

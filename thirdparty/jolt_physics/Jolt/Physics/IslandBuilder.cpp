// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#include <Jolt/Jolt.h>

#include <Jolt/Physics/IslandBuilder.h>
#include <Jolt/Physics/Body/Body.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Core/Profiler.h>
#include <Jolt/Core/Atomics.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/QuickSort.h>

JPH_NAMESPACE_BEGIN

IslandBuilder::~IslandBuilder()
{
	JPH_ASSERT(mConstraintLinks == nullptr);
	JPH_ASSERT(mContactLinks == nullptr);
	JPH_ASSERT(mBodyIslands == nullptr);
	JPH_ASSERT(mBodyIslandEnds == nullptr);
	JPH_ASSERT(mConstraintIslands == nullptr);
	JPH_ASSERT(mConstraintIslandEnds == nullptr);
	JPH_ASSERT(mContactIslands == nullptr);
	JPH_ASSERT(mContactIslandEnds == nullptr);
	JPH_ASSERT(mIslandsSorted == nullptr);

	delete [] mBodyLinks;
}

void IslandBuilder::Init(uint32 inMaxActiveBodies)
{
	mMaxActiveBodies = inMaxActiveBodies;

	// Link each body to itself, BuildBodyIslands() will restore this so that we don't need to do this each step
	JPH_ASSERT(mBodyLinks == nullptr);
	mBodyLinks = new BodyLink [mMaxActiveBodies];
	for (uint32 i = 0; i < mMaxActiveBodies; ++i)
		mBodyLinks[i].mLinkedTo.store(i, memory_order_relaxed);
}

void IslandBuilder::PrepareContactConstraints(uint32 inMaxContacts, TempAllocator *inTempAllocator)
{
	JPH_PROFILE_FUNCTION();

	// Need to call Init first
	JPH_ASSERT(mBodyLinks != nullptr);

	// Check that the builder has been reset
	JPH_ASSERT(mNumContacts == 0);
	JPH_ASSERT(mNumIslands == 0);

	// Create contact link buffer, not initialized so each contact needs to be explicitly set
	JPH_ASSERT(mContactLinks == nullptr);
	mContactLinks = (uint32 *)inTempAllocator->Allocate(inMaxContacts * sizeof(uint32));
	mMaxContacts = inMaxContacts;

#ifdef JPH_VALIDATE_ISLAND_BUILDER
	// Create validation structures
	JPH_ASSERT(mLinkValidation == nullptr);
	mLinkValidation = (LinkValidation *)inTempAllocator->Allocate(inMaxContacts * sizeof(LinkValidation));
	mNumLinkValidation = 0;
#endif
}

void IslandBuilder::PrepareNonContactConstraints(uint32 inNumConstraints, TempAllocator *inTempAllocator)
{
	JPH_PROFILE_FUNCTION();

	// Need to call Init first
	JPH_ASSERT(mBodyLinks != nullptr);

	// Check that the builder has been reset
	JPH_ASSERT(mNumIslands == 0);

	// Store number of constraints
	mNumConstraints = inNumConstraints;

	// Create constraint link buffer, not initialized so each constraint needs to be explicitly set
	JPH_ASSERT(mConstraintLinks == nullptr);
	mConstraintLinks = (uint32 *)inTempAllocator->Allocate(inNumConstraints * sizeof(uint32));
}

uint32 IslandBuilder::GetLowestBodyIndex(uint32 inActiveBodyIndex) const
{
	uint32 index = inActiveBodyIndex;
	for (;;)
	{
		uint32 link_to = mBodyLinks[index].mLinkedTo.load(memory_order_relaxed);
		if (link_to == index)
			break;
		index = link_to;
	}
	return index;
}

void IslandBuilder::LinkBodies(uint32 inFirst, uint32 inSecond)
{
	JPH_PROFILE_FUNCTION();

	// Both need to be active, we don't want to create an island with static objects
	if (inFirst >= mMaxActiveBodies || inSecond >= mMaxActiveBodies)
		return;

#ifdef JPH_VALIDATE_ISLAND_BUILDER
	// Add link to the validation list
	if (mNumLinkValidation < uint32(mMaxContacts))
		mLinkValidation[mNumLinkValidation++] = { inFirst, inSecond };
	else
		JPH_ASSERT(false, "Out of links");
#endif

	// Start the algorithm with the two bodies
	uint32 first_link_to = inFirst;
	uint32 second_link_to = inSecond;

	for (;;)
	{
		// Follow the chain until we get to the body with lowest index
		// If the swap compare below fails, we'll keep searching from the lowest index for the new lowest index
		first_link_to = GetLowestBodyIndex(first_link_to);
		second_link_to = GetLowestBodyIndex(second_link_to);

		// If the targets are the same, the bodies are already connected
		if (first_link_to != second_link_to)
		{
			// We always link the highest to the lowest
			if (first_link_to < second_link_to)
			{
				// Attempt to link the second to the first
				// Since we found this body to be at the end of the chain it must point to itself, and if it
				// doesn't it has been reparented and we need to retry the algorithm
				if (!mBodyLinks[second_link_to].mLinkedTo.compare_exchange_weak(second_link_to, first_link_to, memory_order_relaxed))
					continue;
			}
			else
			{
				// Attempt to link the first to the second
				// Since we found this body to be at the end of the chain it must point to itself, and if it
				// doesn't it has been reparented and we need to retry the algorithm
				if (!mBodyLinks[first_link_to].mLinkedTo.compare_exchange_weak(first_link_to, second_link_to, memory_order_relaxed))
					continue;
			}
		}

		// Linking succeeded!
		// Chains of bodies can become really long, resulting in an O(N) loop to find the lowest body index
		// to prevent this we attempt to update the link of the bodies that were passed in to directly point
		// to the lowest index that we found. If the value became lower than our lowest link, some other
		// thread must have relinked these bodies in the mean time so we won't update the value.
		uint32 lowest_link_to = min(first_link_to, second_link_to);
		AtomicMin(mBodyLinks[inFirst].mLinkedTo, lowest_link_to, memory_order_relaxed);
		AtomicMin(mBodyLinks[inSecond].mLinkedTo, lowest_link_to, memory_order_relaxed);
		break;
	}
}

void IslandBuilder::LinkConstraint(uint32 inConstraintIndex, uint32 inFirst, uint32 inSecond)
{
	LinkBodies(inFirst, inSecond);

	JPH_ASSERT(inConstraintIndex < mNumConstraints);
	uint32 min_value = min(inFirst, inSecond); // Use fact that invalid index is 0xffffffff, we want the active body of two
	JPH_ASSERT(min_value != Body::cInactiveIndex); // At least one of the bodies must be active
	mConstraintLinks[inConstraintIndex] = min_value;
}

void IslandBuilder::LinkContact(uint32 inContactIndex, uint32 inFirst, uint32 inSecond)
{
	JPH_ASSERT(inContactIndex < mMaxContacts);
	mContactLinks[inContactIndex] = min(inFirst, inSecond); // Use fact that invalid index is 0xffffffff, we want the active body of two
}

#ifdef JPH_VALIDATE_ISLAND_BUILDER

void IslandBuilder::ValidateIslands(uint32 inNumActiveBodies) const
{
	JPH_PROFILE_FUNCTION();

	// Go through all links so far
	for (uint32 i = 0; i < mNumLinkValidation; ++i)
	{
		// If the bodies in this link ended up in different groups we have a problem
		if (mBodyLinks[mLinkValidation[i].mFirst].mIslandIndex != mBodyLinks[mLinkValidation[i].mSecond].mIslandIndex)
		{
			Trace("Fail: %u, %u", mLinkValidation[i].mFirst, mLinkValidation[i].mSecond);
			Trace("Num Active: %u", inNumActiveBodies);

			for (uint32 j = 0; j < mNumLinkValidation; ++j)
				Trace("builder.Link(%u, %u);", mLinkValidation[j].mFirst, mLinkValidation[j].mSecond);

			IslandBuilder tmp;
			tmp.Init(inNumActiveBodies);
			for (uint32 j = 0; j < mNumLinkValidation; ++j)
			{
				Trace("Link %u -> %u", mLinkValidation[j].mFirst, mLinkValidation[j].mSecond);
				tmp.LinkBodies(mLinkValidation[j].mFirst, mLinkValidation[j].mSecond);
				for (uint32 t = 0; t < inNumActiveBodies; ++t)
					Trace("%u -> %u", t, (uint32)tmp.mBodyLinks[t].mLinkedTo);
			}

			JPH_ASSERT(false, "IslandBuilder validation failed");
		}
	}
}

#endif

void IslandBuilder::BuildBodyIslands(const BodyID *inActiveBodies, uint32 inNumActiveBodies, TempAllocator *inTempAllocator)
{
	JPH_PROFILE_FUNCTION();

	// Store the amount of active bodies
	mNumActiveBodies = inNumActiveBodies;

	// Create output arrays for body ID's, don't call constructors
	JPH_ASSERT(mBodyIslands == nullptr);
	mBodyIslands = (BodyID *)inTempAllocator->Allocate(inNumActiveBodies * sizeof(BodyID));

	// Create output array for start index of each island. At this point we don't know how many islands there will be, but we know it cannot be more than inNumActiveBodies.
	// Note: We allocate 1 extra entry because we always increment the count of the next island.
	uint32 *body_island_starts = (uint32 *)inTempAllocator->Allocate((inNumActiveBodies + 1) * sizeof(uint32));

	// First island always starts at 0
	body_island_starts[0] = 0;

	// Calculate island index for all bodies
	JPH_ASSERT(mNumIslands == 0);
	for (uint32 i = 0; i < inNumActiveBodies; ++i)
	{
		BodyLink &link = mBodyLinks[i];
		uint32 s = link.mLinkedTo.load(memory_order_relaxed);
		if (s != i)
		{
			// Links to another body, take island index from other body (this must have been filled in already since we're looping from low to high)
			JPH_ASSERT(s < uint32(i));
			uint32 island_index = mBodyLinks[s].mIslandIndex;
			link.mIslandIndex = island_index;

			// Increment the start of the next island
			body_island_starts[island_index + 1]++;
		}
		else
		{
			// Does not link to other body, this is the start of a new island
			link.mIslandIndex = mNumIslands;
			++mNumIslands;

			// Set the start of the next island to 1
			body_island_starts[mNumIslands] = 1;
		}
	}

#ifdef JPH_VALIDATE_ISLAND_BUILDER
	ValidateIslands(inNumActiveBodies);
#endif

	// Make the start array absolute (so far we only counted)
	for (uint32 island = 1; island < mNumIslands; ++island)
		body_island_starts[island] += body_island_starts[island - 1];

	// Convert the to a linear list grouped by island
	for (uint32 i = 0; i < inNumActiveBodies; ++i)
	{
		BodyLink &link = mBodyLinks[i];

		// Copy the body to the correct location in the array and increment it
		uint32 &start = body_island_starts[link.mIslandIndex];
		mBodyIslands[start] = inActiveBodies[i];
		start++;

		// Reset linked to field for the next update
		link.mLinkedTo.store(i, memory_order_relaxed);
	}

	// We should now have a full array
	JPH_ASSERT(mNumIslands == 0 || body_island_starts[mNumIslands - 1] == inNumActiveBodies);

	// We've incremented all body indices so that they now point at the end instead of the starts
	JPH_ASSERT(mBodyIslandEnds == nullptr);
	mBodyIslandEnds = body_island_starts;
}

void IslandBuilder::BuildConstraintIslands(const uint32 *inConstraintToBody, uint32 inNumConstraints, uint32 *&outConstraints, uint32 *&outConstraintsEnd, TempAllocator *inTempAllocator) const
{
	JPH_PROFILE_FUNCTION();

	// Check if there's anything to do
	if (inNumConstraints == 0)
		return;

	// Create output arrays for constraints
	// Note: For the end indices we allocate 1 extra entry so we don't have to do an if in the inner loop
	uint32 *constraints = (uint32 *)inTempAllocator->Allocate(inNumConstraints * sizeof(uint32));
	uint32 *constraint_ends = (uint32 *)inTempAllocator->Allocate((mNumIslands + 1) * sizeof(uint32));

	// Reset sizes
	for (uint32 island = 0; island < mNumIslands; ++island)
		constraint_ends[island] = 0;

	// Loop over array and increment start relative position for the next island
	for (uint32 constraint = 0; constraint < inNumConstraints; ++constraint)
	{
		uint32 body_idx = inConstraintToBody[constraint];
		uint32 next_island_idx = mBodyLinks[body_idx].mIslandIndex + 1;
		JPH_ASSERT(next_island_idx <= mNumIslands);
		constraint_ends[next_island_idx]++;
	}

	// Make start positions absolute
	for (uint32 island = 1; island < mNumIslands; ++island)
		constraint_ends[island] += constraint_ends[island - 1];

	// Loop over array and collect constraints
	for (uint32 constraint = 0; constraint < inNumConstraints; ++constraint)
	{
		uint32 body_idx = inConstraintToBody[constraint];
		uint32 island_idx = mBodyLinks[body_idx].mIslandIndex;
		constraints[constraint_ends[island_idx]++] = constraint;
	}

	JPH_ASSERT(outConstraints == nullptr);
	outConstraints = constraints;
	JPH_ASSERT(outConstraintsEnd == nullptr);
	outConstraintsEnd = constraint_ends;
}

void IslandBuilder::SortIslands(TempAllocator *inTempAllocator)
{
	JPH_PROFILE_FUNCTION();

	if (mNumContacts > 0 || mNumConstraints > 0)
	{
		// Allocate mapping table
		JPH_ASSERT(mIslandsSorted == nullptr);
		mIslandsSorted = (uint32 *)inTempAllocator->Allocate(mNumIslands * sizeof(uint32));

		// Initialize index
		for (uint32 island = 0; island < mNumIslands; ++island)
			mIslandsSorted[island] = island;

		// Determine the sum of contact constraints / constraints per island
		uint32 *num_constraints = (uint32 *)inTempAllocator->Allocate(mNumIslands * sizeof(uint32));
		if (mNumContacts > 0 && mNumConstraints > 0)
		{
			num_constraints[0] = mConstraintIslandEnds[0] + mContactIslandEnds[0];
			for (uint32 island = 1; island < mNumIslands; ++island)
				num_constraints[island] = mConstraintIslandEnds[island] - mConstraintIslandEnds[island - 1]
										+ mContactIslandEnds[island] - mContactIslandEnds[island - 1];
		}
		else if (mNumContacts > 0)
		{
			num_constraints[0] = mContactIslandEnds[0];
			for (uint32 island = 1; island < mNumIslands; ++island)
				num_constraints[island] = mContactIslandEnds[island] - mContactIslandEnds[island - 1];
		}
		else
		{
			num_constraints[0] = mConstraintIslandEnds[0];
			for (uint32 island = 1; island < mNumIslands; ++island)
				num_constraints[island] = mConstraintIslandEnds[island] - mConstraintIslandEnds[island - 1];
		}

		// Sort so the biggest islands go first, this means that the jobs that take longest will be running
		// first which improves the chance that all jobs finish at the same time.
		QuickSort(mIslandsSorted, mIslandsSorted + mNumIslands, [num_constraints](uint32 inLHS, uint32 inRHS) {
			return num_constraints[inLHS] > num_constraints[inRHS];
		});

		inTempAllocator->Free(num_constraints, mNumIslands * sizeof(uint32));
	}
}

void IslandBuilder::Finalize(const BodyID *inActiveBodies, uint32 inNumActiveBodies, uint32 inNumContacts, TempAllocator *inTempAllocator)
{
	JPH_PROFILE_FUNCTION();

	mNumContacts = inNumContacts;

	BuildBodyIslands(inActiveBodies, inNumActiveBodies, inTempAllocator);
	BuildConstraintIslands(mConstraintLinks, mNumConstraints, mConstraintIslands, mConstraintIslandEnds, inTempAllocator);
	BuildConstraintIslands(mContactLinks, mNumContacts, mContactIslands, mContactIslandEnds, inTempAllocator);
	SortIslands(inTempAllocator);

	mNumPositionSteps = (uint8 *)inTempAllocator->Allocate(mNumIslands * sizeof(uint8));
}

void IslandBuilder::GetBodiesInIsland(uint32 inIslandIndex, BodyID *&outBodiesBegin, BodyID *&outBodiesEnd) const
{
	JPH_ASSERT(inIslandIndex < mNumIslands);
	uint32 sorted_index = mIslandsSorted != nullptr? mIslandsSorted[inIslandIndex] : inIslandIndex;
	outBodiesBegin = sorted_index > 0? mBodyIslands + mBodyIslandEnds[sorted_index - 1] : mBodyIslands;
	outBodiesEnd = mBodyIslands + mBodyIslandEnds[sorted_index];
}

bool IslandBuilder::GetConstraintsInIsland(uint32 inIslandIndex, uint32 *&outConstraintsBegin, uint32 *&outConstraintsEnd) const
{
	JPH_ASSERT(inIslandIndex < mNumIslands);
	if (mNumConstraints == 0)
	{
		outConstraintsBegin = nullptr;
		outConstraintsEnd = nullptr;
		return false;
	}
	else
	{
		uint32 sorted_index = mIslandsSorted[inIslandIndex];
		outConstraintsBegin = sorted_index > 0? mConstraintIslands + mConstraintIslandEnds[sorted_index - 1] : mConstraintIslands;
		outConstraintsEnd = mConstraintIslands + mConstraintIslandEnds[sorted_index];
		return outConstraintsBegin != outConstraintsEnd;
	}
}

bool IslandBuilder::GetContactsInIsland(uint32 inIslandIndex, uint32 *&outContactsBegin, uint32 *&outContactsEnd) const
{
	JPH_ASSERT(inIslandIndex < mNumIslands);
	if (mNumContacts == 0)
	{
		outContactsBegin = nullptr;
		outContactsEnd = nullptr;
		return false;
	}
	else
	{
		uint32 sorted_index = mIslandsSorted[inIslandIndex];
		outContactsBegin = sorted_index > 0? mContactIslands + mContactIslandEnds[sorted_index - 1] : mContactIslands;
		outContactsEnd = mContactIslands + mContactIslandEnds[sorted_index];
		return outContactsBegin != outContactsEnd;
	}
}

void IslandBuilder::ResetIslands(TempAllocator *inTempAllocator)
{
	JPH_PROFILE_FUNCTION();

	inTempAllocator->Free(mNumPositionSteps, mNumIslands * sizeof(uint8));

	if (mIslandsSorted != nullptr)
	{
		inTempAllocator->Free(mIslandsSorted, mNumIslands * sizeof(uint32));
		mIslandsSorted = nullptr;
	}

	if (mContactIslands != nullptr)
	{
		inTempAllocator->Free(mContactIslandEnds, (mNumIslands + 1) * sizeof(uint32));
		mContactIslandEnds = nullptr;
		inTempAllocator->Free(mContactIslands, mNumContacts * sizeof(uint32));
		mContactIslands = nullptr;
	}

	if (mConstraintIslands != nullptr)
	{
		inTempAllocator->Free(mConstraintIslandEnds, (mNumIslands + 1) * sizeof(uint32));
		mConstraintIslandEnds = nullptr;
		inTempAllocator->Free(mConstraintIslands, mNumConstraints * sizeof(uint32));
		mConstraintIslands = nullptr;
	}

	inTempAllocator->Free(mBodyIslandEnds, (mNumActiveBodies + 1) * sizeof(uint32));
	mBodyIslandEnds = nullptr;
	inTempAllocator->Free(mBodyIslands, mNumActiveBodies * sizeof(uint32));
	mBodyIslands = nullptr;

	inTempAllocator->Free(mConstraintLinks, mNumConstraints * sizeof(uint32));
	mConstraintLinks = nullptr;

#ifdef JPH_VALIDATE_ISLAND_BUILDER
	inTempAllocator->Free(mLinkValidation, mMaxContacts * sizeof(LinkValidation));
	mLinkValidation = nullptr;
#endif

	inTempAllocator->Free(mContactLinks, mMaxContacts * sizeof(uint32));
	mContactLinks = nullptr;

	mNumActiveBodies = 0;
	mNumConstraints = 0;
	mMaxContacts = 0;
	mNumContacts = 0;
	mNumIslands = 0;
}

JPH_NAMESPACE_END

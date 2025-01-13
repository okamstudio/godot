// SPDX-FileCopyrightText: 2023 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#include <Jolt/Jolt.h>

#include <Jolt/Physics/LargeIslandSplitter.h>
#include <Jolt/Physics/IslandBuilder.h>
#include <Jolt/Physics/Constraints/CalculateSolverSteps.h>
#include <Jolt/Physics/Constraints/Constraint.h>
#include <Jolt/Physics/Constraints/ContactConstraintManager.h>
#include <Jolt/Physics/Body/BodyManager.h>
#include <Jolt/Core/Profiler.h>
#include <Jolt/Core/TempAllocator.h>

//#define JPH_LARGE_ISLAND_SPLITTER_DEBUG

JPH_NAMESPACE_BEGIN

LargeIslandSplitter::EStatus LargeIslandSplitter::Splits::FetchNextBatch(uint32 &outConstraintsBegin, uint32 &outConstraintsEnd, uint32 &outContactsBegin, uint32 &outContactsEnd, bool &outFirstIteration)
{
	{
		// First check if we can get a new batch (doing a read to avoid hammering an atomic with an atomic subtract)
		// Note this also avoids overflowing the status counter if we're done but there's still one thread processing items
		uint64 status = mStatus.load(memory_order_acquire);

		// Check for special value that indicates that the splits are still being built
		// (note we do not check for this condition again below as we reset all splits before kicking off jobs that fetch batches of work)
		if (status == StatusItemMask)
			return EStatus::WaitingForBatch;

		// Next check if all items have been processed. Note that we do this after checking if the job can be started
		// as mNumIterations is not initialized until the split is started.
		if (sGetIteration(status) >= mNumIterations)
			return EStatus::AllBatchesDone;

		uint item = sGetItem(status);
		uint split_index = sGetSplit(status);
		if (split_index == cNonParallelSplitIdx)
		{
			// Non parallel split needs to be taken as a single batch, only the thread that takes element 0 will do it
			if (item != 0)
				return EStatus::WaitingForBatch;
		}
		else
		{
			// Parallel split is split into batches
			JPH_ASSERT(split_index < mNumSplits);
			const Split &split = mSplits[split_index];
			if (item >= split.GetNumItems())
				return EStatus::WaitingForBatch;
		}
	}

	// Then try to actually get the batch
	uint64 status = mStatus.fetch_add(cBatchSize, memory_order_acquire);
	int iteration = sGetIteration(status);
	if (iteration >= mNumIterations)
		return EStatus::AllBatchesDone;

	uint split_index = sGetSplit(status);
	JPH_ASSERT(split_index < mNumSplits || split_index == cNonParallelSplitIdx);
	const Split &split = mSplits[split_index];
	uint item_begin = sGetItem(status);
	if (split_index == cNonParallelSplitIdx)
	{
		if (item_begin == 0)
		{
			// Non-parallel split always goes as a single batch
			outConstraintsBegin = split.mConstraintBufferBegin;
			outConstraintsEnd = split.mConstraintBufferEnd;
			outContactsBegin = split.mContactBufferBegin;
			outContactsEnd = split.mContactBufferEnd;
			outFirstIteration = iteration == 0;
			return EStatus::BatchRetrieved;
		}
		else
		{
			// Otherwise we're done with this split
			return EStatus::WaitingForBatch;
		}
	}

	// Parallel split is split into batches
	uint num_constraints = split.GetNumConstraints();
	uint num_contacts = split.GetNumContacts();
	uint num_items = num_constraints + num_contacts;
	if (item_begin >= num_items)
		return EStatus::WaitingForBatch;

	uint item_end = min(item_begin + cBatchSize, num_items);
	if (item_end >= num_constraints)
	{
		if (item_begin < num_constraints)
		{
			// Partially from constraints and partially from contacts
			outConstraintsBegin = split.mConstraintBufferBegin + item_begin;
			outConstraintsEnd = split.mConstraintBufferEnd;
		}
		else
		{
			// Only contacts
			outConstraintsBegin = 0;
			outConstraintsEnd = 0;
		}

		outContactsBegin = split.mContactBufferBegin + (max(item_begin, num_constraints) - num_constraints);
		outContactsEnd = split.mContactBufferBegin + (item_end - num_constraints);
	}
	else
	{
		// Only constraints
		outConstraintsBegin = split.mConstraintBufferBegin + item_begin;
		outConstraintsEnd = split.mConstraintBufferBegin + item_end;

		outContactsBegin = 0;
		outContactsEnd = 0;
	}

	outFirstIteration = iteration == 0;
	return EStatus::BatchRetrieved;
}

void LargeIslandSplitter::Splits::MarkBatchProcessed(uint inNumProcessed, bool &outLastIteration, bool &outFinalBatch)
{
	// We fetched this batch, nobody should change the split and or iteration until we mark the last batch as processed so we can safely get the current status
	uint64 status = mStatus.load(memory_order_relaxed);
	uint split_index = sGetSplit(status);
	JPH_ASSERT(split_index < mNumSplits || split_index == cNonParallelSplitIdx);
	const Split &split = mSplits[split_index];
	uint num_items_in_split = split.GetNumItems();

	// Determine if this is the last iteration before possibly incrementing it
	int iteration = sGetIteration(status);
	outLastIteration = iteration == mNumIterations - 1;

	// Add the number of items we processed to the total number of items processed
	// Note: This needs to happen after we read the status as other threads may update the status after we mark items as processed
	JPH_ASSERT(inNumProcessed > 0); // Logic will break if we mark a block of 0 items as processed
	uint total_items_processed = mItemsProcessed.fetch_add(inNumProcessed, memory_order_acq_rel) + inNumProcessed;

	// Check if we're at the end of the split
	if (total_items_processed >= num_items_in_split)
	{
		JPH_ASSERT(total_items_processed == num_items_in_split); // Should not overflow, that means we're retiring more items than we should process

		// Set items processed back to 0 for the next split/iteration
		mItemsProcessed.store(0, memory_order_release);

		// Determine next split
		do
		{
			if (split_index == cNonParallelSplitIdx)
			{
				// At start of next iteration
				split_index = 0;
				++iteration;
			}
			else
			{
				// At start of next split
				++split_index;
			}

			// If we're beyond the end of splits, go to the non-parallel split
			if (split_index >= mNumSplits)
				split_index = cNonParallelSplitIdx;
		}
		while (iteration < mNumIterations
			&& mSplits[split_index].GetNumItems() == 0); // We don't support processing empty splits, skip to the next split in this case

		mStatus.store((uint64(iteration) << StatusIterationShift) | (uint64(split_index) << StatusSplitShift), memory_order_release);
	}

	// Track if this is the final batch
	outFinalBatch = iteration >= mNumIterations;
}

LargeIslandSplitter::~LargeIslandSplitter()
{
	JPH_ASSERT(mSplitMasks == nullptr);
	JPH_ASSERT(mContactAndConstraintsSplitIdx == nullptr);
	JPH_ASSERT(mContactAndConstraintIndices == nullptr);
	JPH_ASSERT(mSplitIslands == nullptr);
}

void LargeIslandSplitter::Prepare(const IslandBuilder &inIslandBuilder, uint32 inNumActiveBodies, TempAllocator *inTempAllocator)
{
	JPH_PROFILE_FUNCTION();

	// Count the total number of constraints and contacts that we will be putting in splits
	mContactAndConstraintsSize = 0;
	for (uint32 island = 0; island < inIslandBuilder.GetNumIslands(); ++island)
	{
		// Get the contacts in this island
		uint32 *contacts_start, *contacts_end;
		inIslandBuilder.GetContactsInIsland(island, contacts_start, contacts_end);
		uint num_contacts_in_island = uint(contacts_end - contacts_start);

		// Get the constraints in this island
		uint32 *constraints_start, *constraints_end;
		inIslandBuilder.GetConstraintsInIsland(island, constraints_start, constraints_end);
		uint num_constraints_in_island = uint(constraints_end - constraints_start);

		uint island_size = num_contacts_in_island + num_constraints_in_island;
		if (island_size >= cLargeIslandTreshold)
		{
			mNumSplitIslands++;
			mContactAndConstraintsSize += island_size;
		}
		else
			break; // If this island doesn't have enough constraints, the next islands won't either since they're sorted from big to small
	}

	if (mContactAndConstraintsSize > 0)
	{
		mNumActiveBodies = inNumActiveBodies;

		// Allocate split mask buffer
		mSplitMasks = (SplitMask *)inTempAllocator->Allocate(mNumActiveBodies * sizeof(SplitMask));

		// Allocate contact and constraint buffer
		uint contact_and_constraint_indices_size = mContactAndConstraintsSize * sizeof(uint32);
		mContactAndConstraintsSplitIdx = (uint32 *)inTempAllocator->Allocate(contact_and_constraint_indices_size);
		mContactAndConstraintIndices = (uint32 *)inTempAllocator->Allocate(contact_and_constraint_indices_size);

		// Allocate island split buffer
		mSplitIslands = (Splits *)inTempAllocator->Allocate(mNumSplitIslands * sizeof(Splits));

		// Prevent any of the splits from being picked up as work
		for (uint i = 0; i < mNumSplitIslands; ++i)
			mSplitIslands[i].ResetStatus();
	}
}

uint LargeIslandSplitter::AssignSplit(const Body *inBody1, const Body *inBody2)
{
	uint32 idx1 = inBody1->GetIndexInActiveBodiesInternal();
	uint32 idx2 = inBody2->GetIndexInActiveBodiesInternal();

	// Test if either index is negative
	if (idx1 == Body::cInactiveIndex || !inBody1->IsDynamic())
	{
		// Body 1 is not active or a kinematic body, so we only need to set 1 body
		JPH_ASSERT(idx2 < mNumActiveBodies);
		SplitMask &mask = mSplitMasks[idx2];
		uint split = min(CountTrailingZeros(~uint32(mask)), cNonParallelSplitIdx);
		mask |= SplitMask(1U << split);
		return split;
	}
	else if (idx2 == Body::cInactiveIndex || !inBody2->IsDynamic())
	{
		// Body 2 is not active or a kinematic body, so we only need to set 1 body
		JPH_ASSERT(idx1 < mNumActiveBodies);
		SplitMask &mask = mSplitMasks[idx1];
		uint split = min(CountTrailingZeros(~uint32(mask)), cNonParallelSplitIdx);
		mask |= SplitMask(1U << split);
		return split;
	}
	else
	{
		// If both bodies are active, we need to set 2 bodies
		JPH_ASSERT(idx1 < mNumActiveBodies);
		JPH_ASSERT(idx2 < mNumActiveBodies);
		SplitMask &mask1 = mSplitMasks[idx1];
		SplitMask &mask2 = mSplitMasks[idx2];
		uint split = min(CountTrailingZeros((~uint32(mask1)) & (~uint32(mask2))), cNonParallelSplitIdx);
		SplitMask mask = SplitMask(1U << split);
		mask1 |= mask;
		mask2 |= mask;
		return split;
	}
}

uint LargeIslandSplitter::AssignToNonParallelSplit(const Body *inBody)
{
	uint32 idx = inBody->GetIndexInActiveBodiesInternal();
	if (idx != Body::cInactiveIndex)
	{
		JPH_ASSERT(idx < mNumActiveBodies);
		mSplitMasks[idx] |= 1U << cNonParallelSplitIdx;
	}

	return cNonParallelSplitIdx;
}

bool LargeIslandSplitter::SplitIsland(uint32 inIslandIndex, const IslandBuilder &inIslandBuilder, const BodyManager &inBodyManager, const ContactConstraintManager &inContactManager, Constraint **inActiveConstraints, CalculateSolverSteps &ioStepsCalculator)
{
	JPH_PROFILE_FUNCTION();

	// Get the contacts in this island
	uint32 *contacts_start, *contacts_end;
	inIslandBuilder.GetContactsInIsland(inIslandIndex, contacts_start, contacts_end);
	uint num_contacts_in_island = uint(contacts_end - contacts_start);

	// Get the constraints in this island
	uint32 *constraints_start, *constraints_end;
	inIslandBuilder.GetConstraintsInIsland(inIslandIndex, constraints_start, constraints_end);
	uint num_constraints_in_island = uint(constraints_end - constraints_start);

	// Check if it exceeds the threshold
	uint island_size = num_contacts_in_island + num_constraints_in_island;
	if (island_size < cLargeIslandTreshold)
		return false;

	// Get bodies in this island
	BodyID *bodies_start, *bodies_end;
	inIslandBuilder.GetBodiesInIsland(inIslandIndex, bodies_start, bodies_end);

	// Reset the split mask for all bodies in this island
	Body const * const *bodies = inBodyManager.GetBodies().data();
	for (const BodyID *b = bodies_start; b < bodies_end; ++b)
		mSplitMasks[bodies[b->GetIndex()]->GetIndexInActiveBodiesInternal()] = 0;

	// Count the number of contacts and constraints per split
	uint num_contacts_in_split[cNumSplits] = { };
	uint num_constraints_in_split[cNumSplits] = { };

	// Get space to store split indices
	uint offset = mContactAndConstraintsNextFree.fetch_add(island_size, memory_order_relaxed);
	uint32 *contact_split_idx = mContactAndConstraintsSplitIdx + offset;
	uint32 *constraint_split_idx = contact_split_idx + num_contacts_in_island;

	// Assign the contacts to a split
	uint32 *cur_contact_split_idx = contact_split_idx;
	for (const uint32 *c = contacts_start; c < contacts_end; ++c)
	{
		const Body *body1, *body2;
		inContactManager.GetAffectedBodies(*c, body1, body2);
		uint split = AssignSplit(body1, body2);
		num_contacts_in_split[split]++;
		*cur_contact_split_idx++ = split;

		if (body1->IsDynamic())
			ioStepsCalculator(body1->GetMotionPropertiesUnchecked());
		if (body2->IsDynamic())
			ioStepsCalculator(body2->GetMotionPropertiesUnchecked());
	}

	// Assign the constraints to a split
	uint32 *cur_constraint_split_idx = constraint_split_idx;
	for (const uint32 *c = constraints_start; c < constraints_end; ++c)
	{
		const Constraint *constraint = inActiveConstraints[*c];
		uint split = constraint->BuildIslandSplits(*this);
		num_constraints_in_split[split]++;
		*cur_constraint_split_idx++ = split;

		ioStepsCalculator(constraint);
	}

	ioStepsCalculator.Finalize();

	// Start with 0 splits
	uint split_remap_table[cNumSplits];
	uint new_split_idx = mNextSplitIsland.fetch_add(1, memory_order_relaxed);
	JPH_ASSERT(new_split_idx < mNumSplitIslands);
	Splits &splits = mSplitIslands[new_split_idx];
	splits.mIslandIndex = inIslandIndex;
	splits.mNumSplits = 0;
	splits.mNumIterations = ioStepsCalculator.GetNumVelocitySteps() + 1; // Iteration 0 is used for warm starting
	splits.mNumVelocitySteps = ioStepsCalculator.GetNumVelocitySteps();
	splits.mNumPositionSteps = ioStepsCalculator.GetNumPositionSteps();
	splits.mItemsProcessed.store(0, memory_order_release);

	// Allocate space to store the sorted constraint and contact indices per split
	uint32 *constraint_buffer_cur[cNumSplits], *contact_buffer_cur[cNumSplits];
	for (uint s = 0; s < cNumSplits; ++s)
	{
		// If this split doesn't contain enough constraints and contacts, we will combine it with the non parallel split
		if (num_constraints_in_split[s] + num_contacts_in_split[s] < cSplitCombineTreshold
			&& s < cNonParallelSplitIdx) // The non-parallel split cannot merge into itself
		{
			// Remap it
			split_remap_table[s] = cNonParallelSplitIdx;

			// Add the counts to the non parallel split
			num_contacts_in_split[cNonParallelSplitIdx] += num_contacts_in_split[s];
			num_constraints_in_split[cNonParallelSplitIdx] += num_constraints_in_split[s];
		}
		else
		{
			// This split is valid, map it to the next empty slot
			uint target_split;
			if (s < cNonParallelSplitIdx)
				target_split = splits.mNumSplits++;
			else
				target_split = cNonParallelSplitIdx;
			Split &split = splits.mSplits[target_split];
			split_remap_table[s] = target_split;

			// Allocate space for contacts
			split.mContactBufferBegin = offset;
			split.mContactBufferEnd = split.mContactBufferBegin + num_contacts_in_split[s];

			// Allocate space for constraints
			split.mConstraintBufferBegin = split.mContactBufferEnd;
			split.mConstraintBufferEnd = split.mConstraintBufferBegin + num_constraints_in_split[s];

			// Store start for each split
			contact_buffer_cur[target_split] = mContactAndConstraintIndices + split.mContactBufferBegin;
			constraint_buffer_cur[target_split] = mContactAndConstraintIndices + split.mConstraintBufferBegin;

			// Update offset
			offset = split.mConstraintBufferEnd;
		}
	}

	// Split the contacts
	for (uint c = 0; c < num_contacts_in_island; ++c)
	{
		uint split = split_remap_table[contact_split_idx[c]];
		*contact_buffer_cur[split]++ = contacts_start[c];
	}

	// Split the constraints
	for (uint c = 0; c < num_constraints_in_island; ++c)
	{
		uint split = split_remap_table[constraint_split_idx[c]];
		*constraint_buffer_cur[split]++ = constraints_start[c];
	}

#ifdef JPH_LARGE_ISLAND_SPLITTER_DEBUG
	// Trace the size of all splits
	uint sum = 0;
	String stats;
	for (uint s = 0; s < cNumSplits; ++s)
	{
		// If we've processed all splits, jump to the non-parallel split
		if (s >= splits.GetNumSplits())
			s = cNonParallelSplitIdx;

		const Split &split = splits.mSplits[s];
		stats += StringFormat("g:%d:%d:%d, ", s, split.GetNumContacts(), split.GetNumConstraints());
		sum += split.GetNumItems();
	}
	stats += StringFormat("sum: %d", sum);
	Trace(stats.c_str());
#endif // JPH_LARGE_ISLAND_SPLITTER_DEBUG

#ifdef JPH_ENABLE_ASSERTS
	for (uint s = 0; s < cNumSplits; ++s)
	{
		// If there are no more splits, process the non-parallel split
		if (s >= splits.mNumSplits)
			s = cNonParallelSplitIdx;

		// Check that we wrote all elements
		Split &split = splits.mSplits[s];
		JPH_ASSERT(contact_buffer_cur[s] == mContactAndConstraintIndices + split.mContactBufferEnd);
		JPH_ASSERT(constraint_buffer_cur[s] == mContactAndConstraintIndices + split.mConstraintBufferEnd);
	}

#ifdef JPH_DEBUG
	// Validate that the splits are indeed not touching the same body
	for (uint s = 0; s < splits.mNumSplits; ++s)
	{
		Array<bool> body_used(mNumActiveBodies, false);

		// Validate contacts
		uint32 split_contacts_begin, split_contacts_end;
		splits.GetContactsInSplit(s, split_contacts_begin, split_contacts_end);
		for (uint32 *c = mContactAndConstraintIndices + split_contacts_begin; c < mContactAndConstraintIndices + split_contacts_end; ++c)
		{
			const Body *body1, *body2;
			inContactManager.GetAffectedBodies(*c, body1, body2);

			uint32 idx1 = body1->GetIndexInActiveBodiesInternal();
			if (idx1 != Body::cInactiveIndex && body1->IsDynamic())
			{
				JPH_ASSERT(!body_used[idx1]);
				body_used[idx1] = true;
			}

			uint32 idx2 = body2->GetIndexInActiveBodiesInternal();
			if (idx2 != Body::cInactiveIndex && body2->IsDynamic())
			{
				JPH_ASSERT(!body_used[idx2]);
				body_used[idx2] = true;
			}
		}
	}
#endif // JPH_DEBUG
#endif // JPH_ENABLE_ASSERTS

	// Allow other threads to pick up this split island now
	splits.StartFirstBatch();
	return true;
}

LargeIslandSplitter::EStatus LargeIslandSplitter::FetchNextBatch(uint &outSplitIslandIndex, uint32 *&outConstraintsBegin, uint32 *&outConstraintsEnd, uint32 *&outContactsBegin, uint32 *&outContactsEnd, bool &outFirstIteration)
{
	// We can't be done when all islands haven't been submitted yet
	uint num_splits_created = mNextSplitIsland.load(memory_order_acquire);
	bool all_done = num_splits_created == mNumSplitIslands;

	// Loop over all split islands to find work
	uint32 constraints_begin, constraints_end, contacts_begin, contacts_end;
	for (Splits *s = mSplitIslands; s < mSplitIslands + num_splits_created; ++s)
		switch (s->FetchNextBatch(constraints_begin, constraints_end, contacts_begin, contacts_end, outFirstIteration))
		{
		case EStatus::AllBatchesDone:
			break;

		case EStatus::WaitingForBatch:
			all_done = false;
			break;

		case EStatus::BatchRetrieved:
			outSplitIslandIndex = uint(s - mSplitIslands);
			outConstraintsBegin = mContactAndConstraintIndices + constraints_begin;
			outConstraintsEnd = mContactAndConstraintIndices + constraints_end;
			outContactsBegin = mContactAndConstraintIndices + contacts_begin;
			outContactsEnd = mContactAndConstraintIndices + contacts_end;
			return EStatus::BatchRetrieved;
		}

	return all_done? EStatus::AllBatchesDone : EStatus::WaitingForBatch;
}

void LargeIslandSplitter::MarkBatchProcessed(uint inSplitIslandIndex, const uint32 *inConstraintsBegin, const uint32 *inConstraintsEnd, const uint32 *inContactsBegin, const uint32 *inContactsEnd, bool &outLastIteration, bool &outFinalBatch)
{
	uint num_items_processed = uint(inConstraintsEnd - inConstraintsBegin) + uint(inContactsEnd - inContactsBegin);

	JPH_ASSERT(inSplitIslandIndex < mNextSplitIsland.load(memory_order_relaxed));
	Splits &splits = mSplitIslands[inSplitIslandIndex];
	splits.MarkBatchProcessed(num_items_processed, outLastIteration, outFinalBatch);
}

void LargeIslandSplitter::PrepareForSolvePositions()
{
	for (Splits *s = mSplitIslands, *s_end = mSplitIslands + mNumSplitIslands; s < s_end; ++s)
	{
		// Set the number of iterations to the number of position steps
		s->mNumIterations = s->mNumPositionSteps;

		// We can start again from the first batch
		s->StartFirstBatch();
	}
}

void LargeIslandSplitter::Reset(TempAllocator *inTempAllocator)
{
	JPH_PROFILE_FUNCTION();

	// Everything should have been used
	JPH_ASSERT(mContactAndConstraintsNextFree.load(memory_order_relaxed) == mContactAndConstraintsSize);
	JPH_ASSERT(mNextSplitIsland.load(memory_order_relaxed) == mNumSplitIslands);

	// Free split islands
	if (mNumSplitIslands > 0)
	{
		inTempAllocator->Free(mSplitIslands, mNumSplitIslands * sizeof(Splits));
		mSplitIslands = nullptr;

		mNumSplitIslands = 0;
		mNextSplitIsland.store(0, memory_order_relaxed);
	}

	// Free contact and constraint buffers
	if (mContactAndConstraintsSize > 0)
	{
		inTempAllocator->Free(mContactAndConstraintIndices, mContactAndConstraintsSize * sizeof(uint32));
		mContactAndConstraintIndices = nullptr;

		inTempAllocator->Free(mContactAndConstraintsSplitIdx, mContactAndConstraintsSize * sizeof(uint32));
		mContactAndConstraintsSplitIdx = nullptr;

		mContactAndConstraintsSize = 0;
		mContactAndConstraintsNextFree.store(0, memory_order_relaxed);
	}

	// Free split masks
	if (mSplitMasks != nullptr)
	{
		inTempAllocator->Free(mSplitMasks, mNumActiveBodies * sizeof(SplitMask));
		mSplitMasks = nullptr;

		mNumActiveBodies = 0;
	}
}

JPH_NAMESPACE_END

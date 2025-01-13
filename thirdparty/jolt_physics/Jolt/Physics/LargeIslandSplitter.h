// SPDX-FileCopyrightText: 2023 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Jolt/Core/NonCopyable.h>
#include <Jolt/Core/Atomics.h>

JPH_NAMESPACE_BEGIN

class Body;
class BodyID;
class IslandBuilder;
class TempAllocator;
class Constraint;
class BodyManager;
class ContactConstraintManager;
class CalculateSolverSteps;

/// Assigns bodies in large islands to multiple groups that can run in parallel
///
/// This basically implements what is described in: High-Performance Physical Simulations on Next-Generation Architecture with Many Cores by Chen et al.
/// See: http://web.eecs.umich.edu/~msmelyan/papers/physsim_onmanycore_itj.pdf section "PARALLELIZATION METHODOLOGY"
class LargeIslandSplitter : public NonCopyable
{
private:
	using					SplitMask = uint32;

public:
	static constexpr uint	cNumSplits = sizeof(SplitMask) * 8;
	static constexpr uint	cNonParallelSplitIdx = cNumSplits - 1;
	static constexpr uint	cLargeIslandTreshold = 128;							///< If the number of constraints + contacts in an island is larger than this, we will try to split the island

	/// Status code for retrieving a batch
	enum class EStatus
	{
		WaitingForBatch,														///< Work is expected to be available later
		BatchRetrieved,															///< Work is being returned
		AllBatchesDone,															///< No further work is expected from this
	};

	/// Describes a split of constraints and contacts
	struct Split
	{
		inline uint			GetNumContacts() const								{ return mContactBufferEnd - mContactBufferBegin; }
		inline uint			GetNumConstraints() const							{ return mConstraintBufferEnd - mConstraintBufferBegin; }
		inline uint			GetNumItems() const									{ return GetNumContacts() + GetNumConstraints(); }

		uint32				mContactBufferBegin;								///< Begin of the contact buffer (offset relative to mContactAndConstraintIndices)
		uint32				mContactBufferEnd;									///< End of the contact buffer

		uint32				mConstraintBufferBegin;								///< Begin of the constraint buffer (offset relative to mContactAndConstraintIndices)
		uint32				mConstraintBufferEnd;								///< End of the constraint buffer
	};

	/// Structure that describes the resulting splits from the large island splitter
	class Splits
	{
	public:
		inline uint			GetNumSplits() const
		{
			return mNumSplits;
		}

		inline void			GetConstraintsInSplit(uint inSplitIndex, uint32 &outConstraintsBegin, uint32 &outConstraintsEnd) const
		{
			const Split &split = mSplits[inSplitIndex];
			outConstraintsBegin = split.mConstraintBufferBegin;
			outConstraintsEnd = split.mConstraintBufferEnd;
		}

		inline void			GetContactsInSplit(uint inSplitIndex, uint32 &outContactsBegin, uint32 &outContactsEnd) const
		{
			const Split &split = mSplits[inSplitIndex];
			outContactsBegin = split.mContactBufferBegin;
			outContactsEnd = split.mContactBufferEnd;
		}

		/// Reset current status so that no work can be picked up from this split
		inline void			ResetStatus()
		{
			mStatus.store(StatusItemMask, memory_order_relaxed);
		}

		/// Make the first batch available to other threads
		inline void			StartFirstBatch()
		{
			uint split_index = mNumSplits > 0? 0 : cNonParallelSplitIdx;
			mStatus.store(uint64(split_index) << StatusSplitShift, memory_order_release);
		}

		/// Fetch the next batch to process
		EStatus				FetchNextBatch(uint32 &outConstraintsBegin, uint32 &outConstraintsEnd, uint32 &outContactsBegin, uint32 &outContactsEnd, bool &outFirstIteration);

		/// Mark a batch as processed
		void				MarkBatchProcessed(uint inNumProcessed, bool &outLastIteration, bool &outFinalBatch);

		enum EIterationStatus : uint64
		{
			StatusIterationMask		= 0xffff000000000000,
			StatusIterationShift	= 48,
			StatusSplitMask			= 0x0000ffff00000000,
			StatusSplitShift		= 32,
			StatusItemMask			= 0x00000000ffffffff,
		};

		static inline int	sGetIteration(uint64 inStatus)
		{
			return int((inStatus & StatusIterationMask) >> StatusIterationShift);
		}

		static inline uint	sGetSplit(uint64 inStatus)
		{
			return uint((inStatus & StatusSplitMask) >> StatusSplitShift);
		}

		static inline uint	sGetItem(uint64 inStatus)
		{
			return uint(inStatus & StatusItemMask);
		}

		Split				mSplits[cNumSplits];								///< Data per split
		uint32				mIslandIndex;										///< Index of the island that was split
		uint				mNumSplits;											///< Number of splits that were created (excluding the non-parallel split)
		int					mNumIterations;										///< Number of iterations to do
		int					mNumVelocitySteps;									///< Number of velocity steps to do (cached for 2nd sub step)
		int					mNumPositionSteps;									///< Number of position steps to do
		atomic<uint64>		mStatus;											///< Status of the split, see EIterationStatus
		atomic<uint>		mItemsProcessed;									///< Number of items that have been marked as processed
	};

public:
	/// Destructor
							~LargeIslandSplitter();

	/// Prepare the island splitter by allocating memory
	void					Prepare(const IslandBuilder &inIslandBuilder, uint32 inNumActiveBodies, TempAllocator *inTempAllocator);

	/// Assign two bodies to a split. Returns the split index.
	uint					AssignSplit(const Body *inBody1, const Body *inBody2);

	/// Force a body to be in a non parallel split. Returns the split index.
	uint					AssignToNonParallelSplit(const Body *inBody);

	/// Splits up an island, the created splits will be added to the list of batches and can be fetched with FetchNextBatch. Returns false if the island did not need splitting.
	bool					SplitIsland(uint32 inIslandIndex, const IslandBuilder &inIslandBuilder, const BodyManager &inBodyManager, const ContactConstraintManager &inContactManager, Constraint **inActiveConstraints, CalculateSolverSteps &ioStepsCalculator);

	/// Fetch the next batch to process, returns a handle in outSplitIslandIndex that must be provided to MarkBatchProcessed when complete
	EStatus					FetchNextBatch(uint &outSplitIslandIndex, uint32 *&outConstraintsBegin, uint32 *&outConstraintsEnd, uint32 *&outContactsBegin, uint32 *&outContactsEnd, bool &outFirstIteration);

	/// Mark a batch as processed
	void					MarkBatchProcessed(uint inSplitIslandIndex, const uint32 *inConstraintsBegin, const uint32 *inConstraintsEnd, const uint32 *inContactsBegin, const uint32 *inContactsEnd, bool &outLastIteration, bool &outFinalBatch);

	/// Get the island index of the island that was split for a particular split island index
	inline uint32			GetIslandIndex(uint inSplitIslandIndex) const
	{
		JPH_ASSERT(inSplitIslandIndex < mNumSplitIslands);
		return mSplitIslands[inSplitIslandIndex].mIslandIndex;
	}

	/// Prepare the island splitter for iterating over the split islands again for position solving. Marks all batches as startable.
	void					PrepareForSolvePositions();

	/// Reset the island splitter
	void					Reset(TempAllocator *inTempAllocator);

private:
	static constexpr uint	cSplitCombineTreshold = 32;							///< If the number of constraints + contacts in a split is lower than this, we will merge this split into the 'non-parallel split'
	static constexpr uint	cBatchSize = 16;									///< Number of items to process in a constraint batch

	uint32					mNumActiveBodies = 0;								///< Cached number of active bodies

	SplitMask *				mSplitMasks = nullptr;								///< Bits that indicate for each body in the BodyManager::mActiveBodies list which split they already belong to

	uint32 *				mContactAndConstraintsSplitIdx = nullptr;			///< Buffer to store the split index per constraint or contact
	uint32 *				mContactAndConstraintIndices = nullptr;				///< Buffer to store the ordered constraint indices per split
	uint					mContactAndConstraintsSize = 0;						///< Total size of mContactAndConstraintsSplitIdx and mContactAndConstraintIndices
	atomic<uint>			mContactAndConstraintsNextFree { 0 };				///< Next element that is free in both buffers

	uint					mNumSplitIslands = 0;								///< Total number of islands that required splitting
	Splits *				mSplitIslands = nullptr;							///< List of islands that required splitting
	atomic<uint>			mNextSplitIsland = 0;								///< Next split island to pick from mSplitIslands
};

JPH_NAMESPACE_END

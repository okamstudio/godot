// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Jolt/Physics/Body/BodyID.h>
#include <Jolt/Core/NonCopyable.h>
#include <Jolt/Core/Atomics.h>

JPH_NAMESPACE_BEGIN

class TempAllocator;

//#define JPH_VALIDATE_ISLAND_BUILDER

/// Keeps track of connected bodies and builds islands for multithreaded velocity/position update
class IslandBuilder : public NonCopyable
{
public:
	/// Destructor
							~IslandBuilder();

	/// Initialize the island builder with the maximum amount of bodies that could be active
	void					Init(uint32 inMaxActiveBodies);

	/// Prepare for simulation step by allocating space for the contact constraints
	void					PrepareContactConstraints(uint32 inMaxContactConstraints, TempAllocator *inTempAllocator);

	/// Prepare for simulation step by allocating space for the non-contact constraints
	void					PrepareNonContactConstraints(uint32 inNumConstraints, TempAllocator *inTempAllocator);

	/// Link two bodies by their index in the BodyManager::mActiveBodies list to form islands
	void					LinkBodies(uint32 inFirst, uint32 inSecond);

	/// Link a constraint to a body by their index in the BodyManager::mActiveBodies
	void					LinkConstraint(uint32 inConstraintIndex, uint32 inFirst, uint32 inSecond);

	/// Link a contact to a body by their index in the BodyManager::mActiveBodies
	void					LinkContact(uint32 inContactIndex, uint32 inFirst, uint32 inSecond);

	/// Finalize the islands after all bodies have been Link()-ed
	void					Finalize(const BodyID *inActiveBodies, uint32 inNumActiveBodies, uint32 inNumContacts, TempAllocator *inTempAllocator);

	/// Get the amount of islands formed
	uint32					GetNumIslands() const							{ return mNumIslands; }

	/// Get iterator for a particular island, return false if there are no constraints
	void					GetBodiesInIsland(uint32 inIslandIndex, BodyID *&outBodiesBegin, BodyID *&outBodiesEnd) const;
	bool					GetConstraintsInIsland(uint32 inIslandIndex, uint32 *&outConstraintsBegin, uint32 *&outConstraintsEnd) const;
	bool					GetContactsInIsland(uint32 inIslandIndex, uint32 *&outContactsBegin, uint32 *&outContactsEnd) const;

	/// The number of position iterations for each island
	void					SetNumPositionSteps(uint32 inIslandIndex, uint inNumPositionSteps)	{ JPH_ASSERT(inIslandIndex < mNumIslands); JPH_ASSERT(inNumPositionSteps < 256); mNumPositionSteps[inIslandIndex] = uint8(inNumPositionSteps); }
	uint					GetNumPositionSteps(uint32 inIslandIndex) const						{ JPH_ASSERT(inIslandIndex < mNumIslands); return mNumPositionSteps[inIslandIndex]; }

	/// After you're done calling the three functions above, call this function to free associated data
	void					ResetIslands(TempAllocator *inTempAllocator);

private:
	/// Returns the index of the lowest body in the group
	uint32					GetLowestBodyIndex(uint32 inActiveBodyIndex) const;

#ifdef JPH_VALIDATE_ISLAND_BUILDER
	/// Helper function to validate all islands so far generated
	void					ValidateIslands(uint32 inNumActiveBodies) const;
#endif

	// Helper functions to build various islands
	void					BuildBodyIslands(const BodyID *inActiveBodies, uint32 inNumActiveBodies, TempAllocator *inTempAllocator);
	void					BuildConstraintIslands(const uint32 *inConstraintToBody, uint32 inNumConstraints, uint32 *&outConstraints, uint32 *&outConstraintsEnd, TempAllocator *inTempAllocator) const;

	/// Sorts the islands so that the islands with most constraints go first
	void					SortIslands(TempAllocator *inTempAllocator);

	/// Intermediate data structure that for each body keeps track what the lowest index of the body is that it is connected to
	struct BodyLink
	{
		JPH_OVERRIDE_NEW_DELETE

		atomic<uint32>		mLinkedTo;										///< An index in mBodyLinks pointing to another body in this island with a lower index than this body
		uint32				mIslandIndex;									///< The island index of this body (filled in during Finalize)
	};

	// Intermediate data
	BodyLink *				mBodyLinks = nullptr;							///< Maps bodies to the first body in the island
	uint32 *				mConstraintLinks = nullptr;						///< Maps constraint index to body index (which maps to island index)
	uint32 *				mContactLinks = nullptr;						///< Maps contact constraint index to body index (which maps to island index)

	// Final data
	BodyID *				mBodyIslands = nullptr;							///< Bodies ordered by island
	uint32 *				mBodyIslandEnds = nullptr;						///< End index of each body island

	uint32 *				mConstraintIslands = nullptr;					///< Constraints ordered by island
	uint32 *				mConstraintIslandEnds = nullptr;				///< End index of each constraint island

	uint32 *				mContactIslands = nullptr;						///< Contacts ordered by island
	uint32 *				mContactIslandEnds = nullptr;					///< End index of each contact island

	uint32 *				mIslandsSorted = nullptr;						///< A list of island indices in order of most constraints first

	uint8 *					mNumPositionSteps = nullptr;					///< Number of position steps for each island

	// Counters
	uint32					mMaxActiveBodies;								///< Maximum size of the active bodies list (see BodyManager::mActiveBodies)
	uint32					mNumActiveBodies = 0;							///< Number of active bodies passed to
	uint32					mNumConstraints = 0;							///< Size of the constraint list (see ConstraintManager::mConstraints)
	uint32					mMaxContacts = 0;								///< Maximum amount of contacts supported
	uint32					mNumContacts = 0;								///< Size of the contacts list (see ContactConstraintManager::mNumConstraints)
	uint32					mNumIslands = 0;								///< Final number of islands

#ifdef JPH_VALIDATE_ISLAND_BUILDER
	/// Structure to keep track of all added links to validate that islands were generated correctly
	struct LinkValidation
	{
		uint32				mFirst;
		uint32				mSecond;
	};

	LinkValidation *		mLinkValidation = nullptr;
	atomic<uint32>			mNumLinkValidation;
#endif
};

JPH_NAMESPACE_END

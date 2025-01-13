// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Jolt/Physics/Collision/GroupFilter.h>
#include <Jolt/Physics/Collision/CollisionGroup.h>

JPH_NAMESPACE_BEGIN

/// Implementation of GroupFilter that stores a bit table with one bit per sub shape ID pair to determine if they collide or not
///
/// The collision rules:
/// - If one of the objects is in the cInvalidGroup the objects will collide.
/// - If the objects are in different groups they will collide.
/// - If they're in the same group but their collision filter is different they will not collide.
/// - If they're in the same group and their collision filters match, we'll use the SubGroupID and the table below.
///
/// For N = 6 sub groups the table will look like:
///
///		            sub group 1 --->
///		sub group 2 x.....
///		     |      ox....
///		     |      oox...
///		     V      ooox..
///		            oooox.
///		            ooooox
///
/// * 'x' means sub group 1 == sub group 2 and we define this to never collide.
/// * 'o' is a bit that we have to store that defines if the sub groups collide or not.
/// * '.' is a bit we don't need to store because the table is symmetric, we take care that group 2 > group 1 by swapping sub group 1 and sub group 2 if needed.
///
/// The total number of bits we need to store is (N * (N - 1)) / 2
class JPH_EXPORT GroupFilterTable final : public GroupFilter
{
	JPH_DECLARE_SERIALIZABLE_VIRTUAL(JPH_EXPORT, GroupFilterTable)

private:
	using GroupID = CollisionGroup::GroupID;
	using SubGroupID = CollisionGroup::SubGroupID;

	/// Get which bit corresponds to the pair (inSubGroup1, inSubGroup2)
	int						GetBit(SubGroupID inSubGroup1, SubGroupID inSubGroup2) const
	{
		JPH_ASSERT(inSubGroup1 != inSubGroup2);

		// We store the lower left half only, so swap the inputs when trying to access the top right half
		if (inSubGroup1 > inSubGroup2)
			std::swap(inSubGroup1, inSubGroup2);

		JPH_ASSERT(inSubGroup2 < mNumSubGroups);

		// Calculate at which bit the entry for this pair resides
		// We use the fact that a row always starts at inSubGroup2 * (inSubGroup2 - 1) / 2
		// (this is the amount of bits needed to store a table of inSubGroup2 entries)
		return (inSubGroup2 * (inSubGroup2 - 1)) / 2 + inSubGroup1;
	}

public:
	/// Constructs the table with inNumSubGroups subgroups, initially all collision pairs are enabled except when the sub group ID is the same
	explicit				GroupFilterTable(uint inNumSubGroups = 0) :
		mNumSubGroups(inNumSubGroups)
	{
		// By default everything collides
		int table_size = ((inNumSubGroups * (inNumSubGroups - 1)) / 2 + 7) / 8;
		mTable.resize(table_size, 0xff);
	}

	/// Copy constructor
							GroupFilterTable(const GroupFilterTable &inRHS) : mNumSubGroups(inRHS.mNumSubGroups), mTable(inRHS.mTable) { }

	/// Disable collision between two sub groups
	void					DisableCollision(SubGroupID inSubGroup1, SubGroupID inSubGroup2)
	{
		int bit = GetBit(inSubGroup1, inSubGroup2);
		mTable[bit >> 3] &= (0xff ^ (1 << (bit & 0b111)));
	}

	/// Enable collision between two sub groups
	void					EnableCollision(SubGroupID inSubGroup1, SubGroupID inSubGroup2)
	{
		int bit = GetBit(inSubGroup1, inSubGroup2);
		mTable[bit >> 3] |= 1 << (bit & 0b111);
	}

	/// Check if the collision between two subgroups is enabled
	inline bool				IsCollisionEnabled(SubGroupID inSubGroup1, SubGroupID inSubGroup2) const
	{
		// Test if the bit is set for this group pair
		int bit = GetBit(inSubGroup1, inSubGroup2);
		return (mTable[bit >> 3] & (1 << (bit & 0b111))) != 0;
	}

	/// Checks if two CollisionGroups collide
	virtual bool			CanCollide(const CollisionGroup &inGroup1, const CollisionGroup &inGroup2) const override
	{
		// If one of the groups is cInvalidGroup the objects will collide (note that the if following this if will ensure that group2 is not cInvalidGroup)
		if (inGroup1.GetGroupID() == CollisionGroup::cInvalidGroup)
			return true;

		// If the objects are in different groups, they collide
		if (inGroup1.GetGroupID() != inGroup2.GetGroupID())
			return true;

		// If the collision filters do not match, but they're in the same group we ignore the collision
		if (inGroup1.GetGroupFilter() != inGroup2.GetGroupFilter())
			return false;

		// If they are in the same sub group, they don't collide
		if (inGroup1.GetSubGroupID() == inGroup2.GetSubGroupID())
			return false;

		// Check the bit table
		return IsCollisionEnabled(inGroup1.GetSubGroupID(), inGroup2.GetSubGroupID());
	}

	// See: GroupFilter::SaveBinaryState
	virtual void			SaveBinaryState(StreamOut &inStream) const override;

protected:
	// See: GroupFilter::RestoreBinaryState
	virtual void			RestoreBinaryState(StreamIn &inStream) override;

private:
	uint					mNumSubGroups;									///< The number of subgroups that this group filter supports
	Array<uint8>			mTable;											///< The table of bits that indicates which pairs collide
};

JPH_NAMESPACE_END

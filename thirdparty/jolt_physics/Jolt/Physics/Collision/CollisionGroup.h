// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Jolt/Physics/Collision/GroupFilter.h>
#include <Jolt/ObjectStream/SerializableObject.h>

JPH_NAMESPACE_BEGIN

class StreamIn;
class StreamOut;

/// Two objects collide with each other if:
/// - Both don't have a group filter
/// - The first group filter says that the objects can collide
/// - Or if there's no filter for the first object, the second group filter says the objects can collide
class JPH_EXPORT CollisionGroup
{
	JPH_DECLARE_SERIALIZABLE_NON_VIRTUAL(JPH_EXPORT, CollisionGroup)

public:
	using GroupID			= uint32;
	using SubGroupID		= uint32;

	static const GroupID	cInvalidGroup = ~GroupID(0);
	static const SubGroupID	cInvalidSubGroup = ~SubGroupID(0);

	/// Default constructor
							CollisionGroup() = default;

	/// Construct with all properties
							CollisionGroup(const GroupFilter *inFilter, GroupID inGroupID, SubGroupID inSubGroupID) : mGroupFilter(inFilter), mGroupID(inGroupID), mSubGroupID(inSubGroupID) { }

	/// Set the collision group filter
	inline void				SetGroupFilter(const GroupFilter *inFilter)
	{
		mGroupFilter = inFilter;
	}

	/// Get the collision group filter
	inline const GroupFilter *GetGroupFilter() const
	{
		return mGroupFilter;
	}

	/// Set the main group id for this object
	inline void				SetGroupID(GroupID inID)
	{
		mGroupID = inID;
	}

	inline GroupID			GetGroupID() const
	{
		return mGroupID;
	}

	/// Add this object to a sub group
	inline void				SetSubGroupID(SubGroupID inID)
	{
		mSubGroupID = inID;
	}

	inline SubGroupID		GetSubGroupID() const
	{
		return mSubGroupID;
	}

	/// Check if this object collides with another object
	bool					CanCollide(const CollisionGroup &inOther) const
	{
		// Call the CanCollide function of the first group filter that's not null
		if (mGroupFilter != nullptr)
			return mGroupFilter->CanCollide(*this, inOther);
		else if (inOther.mGroupFilter != nullptr)
			return inOther.mGroupFilter->CanCollide(inOther, *this);
		else
			return true;
	}

	/// Saves the state of this object in binary form to inStream. Does not save group filter.
	void					SaveBinaryState(StreamOut &inStream) const;

	/// Restore the state of this object from inStream. Does not save group filter.
	void					RestoreBinaryState(StreamIn &inStream);

private:
	RefConst<GroupFilter>	mGroupFilter;
	GroupID					mGroupID = cInvalidGroup;
	SubGroupID				mSubGroupID = cInvalidSubGroup;
};

JPH_NAMESPACE_END

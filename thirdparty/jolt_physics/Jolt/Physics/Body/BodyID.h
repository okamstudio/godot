// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Jolt/Core/HashCombine.h>

JPH_NAMESPACE_BEGIN

/// ID of a body. This is a way of reasoning about bodies in a multithreaded simulation while avoiding race conditions.
class BodyID
{
public:
	JPH_OVERRIDE_NEW_DELETE

	static constexpr uint32	cInvalidBodyID = 0xffffffff;	///< The value for an invalid body ID
	static constexpr uint32	cBroadPhaseBit = 0x00800000;	///< This bit is used by the broadphase
	static constexpr uint32	cMaxBodyIndex = 0x7fffff;		///< Maximum value for body index (also the maximum amount of bodies supported - 1)
	static constexpr uint8	cMaxSequenceNumber = 0xff;		///< Maximum value for the sequence number

	/// Construct invalid body ID
							BodyID() :
		mID(cInvalidBodyID)
	{
	}

	/// Construct from index and sequence number combined in a single uint32 (use with care!)
	explicit				BodyID(uint32 inID) :
		mID(inID)
	{
		JPH_ASSERT((inID & cBroadPhaseBit) == 0 || inID == cInvalidBodyID); // Check bit used by broadphase
	}

	/// Construct from index and sequence number
	explicit				BodyID(uint32 inID, uint8 inSequenceNumber) :
		mID((uint32(inSequenceNumber) << 24) | inID)
	{
		JPH_ASSERT(inID < cMaxBodyIndex); // Should not use bit pattern for invalid ID and should not use the broadphase bit
	}

	/// Get index in body array
	inline uint32			GetIndex() const
	{
		return mID & cMaxBodyIndex;
	}

	/// Get sequence number of body.
	/// The sequence number can be used to check if a body ID with the same body index has been reused by another body.
	/// It is mainly used in multi threaded situations where a body is removed and its body index is immediately reused by a body created from another thread.
	/// Functions querying the broadphase can (after acquiring a body lock) detect that the body has been removed (we assume that this won't happen more than 128 times in a row).
	inline uint8			GetSequenceNumber() const
	{
		return uint8(mID >> 24);
	}

	/// Returns the index and sequence number combined in an uint32
	inline uint32			GetIndexAndSequenceNumber() const
	{
		return mID;
	}

	/// Check if the ID is valid
	inline bool				IsInvalid() const
	{
		return mID == cInvalidBodyID;
	}

	/// Equals check
	inline bool				operator == (const BodyID &inRHS) const
	{
		return mID == inRHS.mID;
	}

	/// Not equals check
	inline bool				operator != (const BodyID &inRHS) const
	{
		return mID != inRHS.mID;
	}

	/// Smaller than operator, can be used for sorting bodies
	inline bool				operator < (const BodyID &inRHS) const
	{
		return mID < inRHS.mID;
	}

	/// Greater than operator, can be used for sorting bodies
	inline bool				operator > (const BodyID &inRHS) const
	{
		return mID > inRHS.mID;
	}

private:
	uint32					mID;
};

JPH_NAMESPACE_END

// Create a std::hash/JPH::Hash for BodyID
JPH_MAKE_HASHABLE(JPH::BodyID, t.GetIndexAndSequenceNumber())

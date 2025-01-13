// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

JPH_NAMESPACE_BEGIN

/// @brief A sub shape id contains a path to an element (usually a triangle or other primitive type) of a compound shape
///
/// Each sub shape knows how many bits it needs to encode its ID, so knows how many bits to take from the sub shape ID.
///
/// For example:
/// * We have a CompoundShape A with 5 child shapes (identify sub shape using 3 bits AAA)
/// * One of its child shapes is CompoundShape B which has 3 child shapes (identify sub shape using 2 bits BB)
/// * One of its child shapes is MeshShape C which contains enough triangles to need 7 bits to identify a triangle (identify sub shape using 7 bits CCCCCCC, note that MeshShape is block based and sorts triangles spatially, you can't assume that the first triangle will have bit pattern 0000000).
///
/// The bit pattern of the sub shape ID to identify a triangle in MeshShape C will then be CCCCCCCBBAAA.
///
/// A sub shape ID will become invalid when the structure of the shape changes. For example, if a child shape is removed from a compound shape, the sub shape ID will no longer be valid.
/// This can be a problem when caching sub shape IDs from one frame to the next. See comments at ContactListener::OnContactPersisted / OnContactRemoved.
class SubShapeID
{
public:
	JPH_OVERRIDE_NEW_DELETE

	/// Underlying storage type
	using Type = uint32;

	/// Type that is bigger than the underlying storage type for operations that would otherwise overflow
	using BiggerType = uint64;

	static_assert(sizeof(BiggerType) > sizeof(Type), "The calculation below assumes BiggerType is a bigger type than Type");

	/// How many bits we can store in this ID
	static constexpr uint MaxBits = 8 * sizeof(Type);

	/// Constructor
						SubShapeID() = default;

	/// Get the next id in the chain of ids (pops parents before children)
	Type				PopID(uint inBits, SubShapeID &outRemainder) const
	{
		Type mask_bits = Type((BiggerType(1) << inBits) - 1);
		Type fill_bits = Type(BiggerType(cEmpty) << (MaxBits - inBits)); // Fill left side bits with 1 so that if there's no remainder all bits will be set, note that we do this using a BiggerType since on intel 0xffffffff << 32 == 0xffffffff
		Type v = mValue & mask_bits;
		outRemainder = SubShapeID(Type(BiggerType(mValue) >> inBits) | fill_bits);
		return v;
	}

	/// Get the value of the path to the sub shape ID
	inline Type			GetValue() const
	{
		return mValue;
	}

	/// Set the value of the sub shape ID (use with care!)
	inline void			SetValue(Type inValue)
	{
		mValue = inValue;
	}

	/// Check if there is any bits of subshape ID left.
	/// Note that this is not a 100% guarantee as the subshape ID could consist of all 1 bits. Use for asserts only.
	inline bool			IsEmpty() const
	{
		return mValue == cEmpty;
	}

	/// Check equal
	inline bool			operator == (const SubShapeID &inRHS) const
	{
		return mValue == inRHS.mValue;
	}

	/// Check not-equal
	inline bool			operator != (const SubShapeID &inRHS) const
	{
		return mValue != inRHS.mValue;
	}

private:
	friend class SubShapeIDCreator;

	/// An empty SubShapeID has all bits set
	static constexpr Type cEmpty = ~Type(0);

	/// Constructor
	explicit			SubShapeID(const Type &inValue) : mValue(inValue) { }

	/// Adds an id at a particular position in the chain
	/// (this should really only be called by the SubShapeIDCreator)
	void				PushID(Type inValue, uint inFirstBit, uint inBits)
	{
		// First clear the bits
		mValue &= ~(Type((BiggerType(1) << inBits) - 1) << inFirstBit);

		// Then set them to the new value
		mValue |= inValue << inFirstBit;
	}

	Type				mValue = cEmpty;
};

/// A sub shape id creator can be used to create a new sub shape id by recursing through the shape
/// hierarchy and pushing new ID's onto the chain
class SubShapeIDCreator
{
public:
	/// Add a new id to the chain of id's and return it
	SubShapeIDCreator	PushID(uint inValue, uint inBits) const
	{
		JPH_ASSERT(inValue < (SubShapeID::BiggerType(1) << inBits));
		SubShapeIDCreator copy = *this;
		copy.mID.PushID(inValue, mCurrentBit, inBits);
		copy.mCurrentBit += inBits;
		JPH_ASSERT(copy.mCurrentBit <= SubShapeID::MaxBits);
		return copy;
	}

	// Get the resulting sub shape ID
	const SubShapeID &	GetID() const
	{
		return mID;
	}

	/// Get the number of bits that have been written to the sub shape ID so far
	inline uint			GetNumBitsWritten() const
	{
		return mCurrentBit;
	}

private:
	SubShapeID			mID;
	uint				mCurrentBit = 0;
};

JPH_NAMESPACE_END

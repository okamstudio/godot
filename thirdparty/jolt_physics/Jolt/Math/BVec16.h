// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2024 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

JPH_NAMESPACE_BEGIN

/// A vector consisting of 16 bytes
class [[nodiscard]] alignas(JPH_VECTOR_ALIGNMENT) BVec16
{
public:
	JPH_OVERRIDE_NEW_DELETE

	// Underlying vector type
#if defined(JPH_USE_SSE)
	using Type = __m128i;
#elif defined(JPH_USE_NEON)
	using Type = uint8x16_t;
#else
	using Type = struct { uint64 mData[2]; };
#endif

	/// Constructor
								BVec16() = default; ///< Intentionally not initialized for performance reasons
								BVec16(const BVec16 &inRHS) = default;
	BVec16 &					operator = (const BVec16 &inRHS) = default;
	JPH_INLINE					BVec16(Type inRHS) : mValue(inRHS)					{ }

	/// Create a vector from 16 bytes
	JPH_INLINE					BVec16(uint8 inB0, uint8 inB1, uint8 inB2, uint8 inB3, uint8 inB4, uint8 inB5, uint8 inB6, uint8 inB7, uint8 inB8, uint8 inB9, uint8 inB10, uint8 inB11, uint8 inB12, uint8 inB13, uint8 inB14, uint8 inB15);

	/// Create a vector from two uint64's
	JPH_INLINE					BVec16(uint64 inV0, uint64 inV1);

	/// Comparison
	JPH_INLINE bool				operator == (BVec16Arg inV2) const;
	JPH_INLINE bool				operator != (BVec16Arg inV2) const					{ return !(*this == inV2); }

	/// Vector with all zeros
	static JPH_INLINE BVec16	sZero();

	/// Replicate int inV across all components
	static JPH_INLINE BVec16	sReplicate(uint8 inV);

	/// Load 16 bytes from memory
	static JPH_INLINE BVec16	sLoadByte16(const uint8 *inV);

	/// Equals (component wise), highest bit of each component that is set is considered true
	static JPH_INLINE BVec16	sEquals(BVec16Arg inV1, BVec16Arg inV2);

	/// Logical or (component wise)
	static JPH_INLINE BVec16	sOr(BVec16Arg inV1, BVec16Arg inV2);

	/// Logical xor (component wise)
	static JPH_INLINE BVec16	sXor(BVec16Arg inV1, BVec16Arg inV2);

	/// Logical and (component wise)
	static JPH_INLINE BVec16	sAnd(BVec16Arg inV1, BVec16Arg inV2);

	/// Logical not (component wise)
	static JPH_INLINE BVec16	sNot(BVec16Arg inV1);

	/// Get component by index
	JPH_INLINE uint8			operator [] (uint inCoordinate) const				{ JPH_ASSERT(inCoordinate < 16); return mU8[inCoordinate]; }
	JPH_INLINE uint8 &			operator [] (uint inCoordinate)						{ JPH_ASSERT(inCoordinate < 16); return mU8[inCoordinate]; }

	/// Test if any of the components are true (true is when highest bit of component is set)
	JPH_INLINE bool				TestAnyTrue() const;

	/// Test if all components are true (true is when highest bit of component is set)
	JPH_INLINE bool				TestAllTrue() const;

	/// Store if mU8[0] is true in bit 0, mU8[1] in bit 1, etc. (true is when highest bit of component is set)
	JPH_INLINE int				GetTrues() const;

	/// To String
	friend ostream &			operator << (ostream &inStream, BVec16Arg inV)
	{
		inStream << uint(inV.mU8[0]) << ", " << uint(inV.mU8[1]) << ", " << uint(inV.mU8[2]) << ", " << uint(inV.mU8[3]) << ", "
				 << uint(inV.mU8[4]) << ", " << uint(inV.mU8[5]) << ", " << uint(inV.mU8[6]) << ", " << uint(inV.mU8[7]) << ", "
				 << uint(inV.mU8[8]) << ", " << uint(inV.mU8[9]) << ", " << uint(inV.mU8[10]) << ", " << uint(inV.mU8[11]) << ", "
				 << uint(inV.mU8[12]) << ", " << uint(inV.mU8[13]) << ", " << uint(inV.mU8[14]) << ", " << uint(inV.mU8[15]);
		return inStream;
	}

	union
	{
		Type					mValue;
		uint8					mU8[16];
		uint64					mU64[2];
	};
};

static_assert(std::is_trivial<BVec16>(), "Is supposed to be a trivial type!");

JPH_NAMESPACE_END

#include "BVec16.inl"

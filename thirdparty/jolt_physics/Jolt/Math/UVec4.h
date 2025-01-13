// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Jolt/Math/Vec4.h>

JPH_NAMESPACE_BEGIN

class [[nodiscard]] alignas(JPH_VECTOR_ALIGNMENT) UVec4
{
public:
	JPH_OVERRIDE_NEW_DELETE

	// Underlying vector type
#if defined(JPH_USE_SSE)
	using Type = __m128i;
#elif defined(JPH_USE_NEON)
	using Type = uint32x4_t;
#else
	using Type = struct { uint32 mData[4]; };
#endif

	/// Constructor
								UVec4() = default; ///< Intentionally not initialized for performance reasons
								UVec4(const UVec4 &inRHS) = default;
	UVec4 &						operator = (const UVec4 &inRHS) = default;
	JPH_INLINE					UVec4(Type inRHS) : mValue(inRHS)					{ }

	/// Create a vector from 4 integer components
	JPH_INLINE					UVec4(uint32 inX, uint32 inY, uint32 inZ, uint32 inW);

	/// Comparison
	JPH_INLINE bool				operator == (UVec4Arg inV2) const;
	JPH_INLINE bool				operator != (UVec4Arg inV2) const					{ return !(*this == inV2); }

	/// Swizzle the elements in inV
	template<uint32 SwizzleX, uint32 SwizzleY, uint32 SwizzleZ, uint32 SwizzleW>
	JPH_INLINE UVec4			Swizzle() const;

	/// Vector with all zeros
	static JPH_INLINE UVec4		sZero();

	/// Replicate int inV across all components
	static JPH_INLINE UVec4		sReplicate(uint32 inV);

	/// Load 1 int from memory and place it in the X component, zeros Y, Z and W
	static JPH_INLINE UVec4		sLoadInt(const uint32 *inV);

	/// Load 4 ints from memory
	static JPH_INLINE UVec4		sLoadInt4(const uint32 *inV);

	/// Load 4 ints from memory, aligned to 16 bytes
	static JPH_INLINE UVec4		sLoadInt4Aligned(const uint32 *inV);

	/// Gather 4 ints from memory at inBase + inOffsets[i] * Scale
	template <const int Scale>
	static JPH_INLINE UVec4		sGatherInt4(const uint32 *inBase, UVec4Arg inOffsets);

	/// Return the minimum value of each of the components
	static JPH_INLINE UVec4		sMin(UVec4Arg inV1, UVec4Arg inV2);

	/// Return the maximum of each of the components
	static JPH_INLINE UVec4		sMax(UVec4Arg inV1, UVec4Arg inV2);

	/// Equals (component wise)
	static JPH_INLINE UVec4		sEquals(UVec4Arg inV1, UVec4Arg inV2);

	/// Component wise select, returns inNotSet when highest bit of inControl = 0 and inSet when highest bit of inControl = 1
	static JPH_INLINE UVec4		sSelect(UVec4Arg inNotSet, UVec4Arg inSet, UVec4Arg inControl);

	/// Logical or (component wise)
	static JPH_INLINE UVec4		sOr(UVec4Arg inV1, UVec4Arg inV2);

	/// Logical xor (component wise)
	static JPH_INLINE UVec4		sXor(UVec4Arg inV1, UVec4Arg inV2);

	/// Logical and (component wise)
	static JPH_INLINE UVec4		sAnd(UVec4Arg inV1, UVec4Arg inV2);

	/// Logical not (component wise)
	static JPH_INLINE UVec4		sNot(UVec4Arg inV1);

	/// Sorts the elements in inIndex so that the values that correspond to trues in inValue are the first elements.
	/// The remaining elements will be set to inValue.w.
	/// I.e. if inValue = (true, false, true, false) and inIndex = (1, 2, 3, 4) the function returns (1, 3, 4, 4).
	static JPH_INLINE UVec4		sSort4True(UVec4Arg inValue, UVec4Arg inIndex);

	/// Get individual components
#if defined(JPH_USE_SSE)
	JPH_INLINE uint32			GetX() const										{ return uint32(_mm_cvtsi128_si32(mValue)); }
	JPH_INLINE uint32			GetY() const										{ return mU32[1]; }
	JPH_INLINE uint32			GetZ() const										{ return mU32[2]; }
	JPH_INLINE uint32			GetW() const										{ return mU32[3]; }
#elif defined(JPH_USE_NEON)
	JPH_INLINE uint32			GetX() const										{ return vgetq_lane_u32(mValue, 0); }
	JPH_INLINE uint32			GetY() const										{ return vgetq_lane_u32(mValue, 1); }
	JPH_INLINE uint32			GetZ() const										{ return vgetq_lane_u32(mValue, 2); }
	JPH_INLINE uint32			GetW() const										{ return vgetq_lane_u32(mValue, 3); }
#else
	JPH_INLINE uint32			GetX() const										{ return mU32[0]; }
	JPH_INLINE uint32			GetY() const										{ return mU32[1]; }
	JPH_INLINE uint32			GetZ() const										{ return mU32[2]; }
	JPH_INLINE uint32			GetW() const										{ return mU32[3]; }
#endif

	/// Set individual components
	JPH_INLINE void				SetX(uint32 inX)									{ mU32[0] = inX; }
	JPH_INLINE void				SetY(uint32 inY)									{ mU32[1] = inY; }
	JPH_INLINE void				SetZ(uint32 inZ)									{ mU32[2] = inZ; }
	JPH_INLINE void				SetW(uint32 inW)									{ mU32[3] = inW; }

	/// Get component by index
	JPH_INLINE uint32			operator [] (uint inCoordinate) const				{ JPH_ASSERT(inCoordinate < 4); return mU32[inCoordinate]; }
	JPH_INLINE uint32 &			operator [] (uint inCoordinate)						{ JPH_ASSERT(inCoordinate < 4); return mU32[inCoordinate]; }

	/// Multiplies each of the 4 integer components with an integer (discards any overflow)
	JPH_INLINE UVec4			operator * (UVec4Arg inV2) const;

	/// Adds an integer value to all integer components (discards any overflow)
	JPH_INLINE UVec4			operator + (UVec4Arg inV2);

	/// Add two integer vectors (component wise)
	JPH_INLINE UVec4 &			operator += (UVec4Arg inV2);

	/// Replicate the X component to all components
	JPH_INLINE UVec4			SplatX() const;

	/// Replicate the Y component to all components
	JPH_INLINE UVec4			SplatY() const;

	/// Replicate the Z component to all components
	JPH_INLINE UVec4			SplatZ() const;

	/// Replicate the W component to all components
	JPH_INLINE UVec4			SplatW() const;

	/// Convert each component from an int to a float
	JPH_INLINE Vec4				ToFloat() const;

	/// Reinterpret UVec4 as a Vec4 (doesn't change the bits)
	JPH_INLINE Vec4				ReinterpretAsFloat() const;

	/// Store 4 ints to memory
	JPH_INLINE void				StoreInt4(uint32 *outV) const;

	/// Store 4 ints to memory, aligned to 16 bytes
	JPH_INLINE void				StoreInt4Aligned(uint32 *outV) const;

	/// Test if any of the components are true (true is when highest bit of component is set)
	JPH_INLINE bool				TestAnyTrue() const;

	/// Test if any of X, Y or Z components are true (true is when highest bit of component is set)
	JPH_INLINE bool				TestAnyXYZTrue() const;

	/// Test if all components are true (true is when highest bit of component is set)
	JPH_INLINE bool				TestAllTrue() const;

	/// Test if X, Y and Z components are true (true is when highest bit of component is set)
	JPH_INLINE bool				TestAllXYZTrue() const;

	/// Count the number of components that are true (true is when highest bit of component is set)
	JPH_INLINE int				CountTrues() const;

	/// Store if X is true in bit 0, Y in bit 1, Z in bit 2 and W in bit 3 (true is when highest bit of component is set)
	JPH_INLINE int				GetTrues() const;

	/// Shift all components by Count bits to the left (filling with zeros from the left)
	template <const uint Count>
	JPH_INLINE UVec4			LogicalShiftLeft() const;

	/// Shift all components by Count bits to the right (filling with zeros from the right)
	template <const uint Count>
	JPH_INLINE UVec4			LogicalShiftRight() const;

	/// Shift all components by Count bits to the right (shifting in the value of the highest bit)
	template <const uint Count>
	JPH_INLINE UVec4			ArithmeticShiftRight() const;

	/// Takes the lower 4 16 bits and expands them to X, Y, Z and W
	JPH_INLINE UVec4			Expand4Uint16Lo() const;

	/// Takes the upper 4 16 bits and expands them to X, Y, Z and W
	JPH_INLINE UVec4			Expand4Uint16Hi() const;

	/// Takes byte 0 .. 3 and expands them to X, Y, Z and W
	JPH_INLINE UVec4			Expand4Byte0() const;

	/// Takes byte 4 .. 7 and expands them to X, Y, Z and W
	JPH_INLINE UVec4			Expand4Byte4() const;

	/// Takes byte 8 .. 11 and expands them to X, Y, Z and W
	JPH_INLINE UVec4			Expand4Byte8() const;

	/// Takes byte 12 .. 15 and expands them to X, Y, Z and W
	JPH_INLINE UVec4			Expand4Byte12() const;

	/// Shift vector components by 4 - Count floats to the left, so if Count = 1 the resulting vector is (W, 0, 0, 0), when Count = 3 the resulting vector is (Y, Z, W, 0)
	JPH_INLINE UVec4			ShiftComponents4Minus(int inCount) const;

	/// To String
	friend ostream &			operator << (ostream &inStream, UVec4Arg inV)
	{
		inStream << inV.mU32[0] << ", " << inV.mU32[1] << ", " << inV.mU32[2] << ", " << inV.mU32[3];
		return inStream;
	}

	union
	{
		Type					mValue;
		uint32					mU32[4];
	};
};

static_assert(std::is_trivial<UVec4>(), "Is supposed to be a trivial type!");

JPH_NAMESPACE_END

#include "UVec4.inl"

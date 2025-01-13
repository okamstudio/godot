// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Jolt/Math/Float4.h>
#include <Jolt/Math/Swizzle.h>
#include <Jolt/Math/MathTypes.h>

JPH_NAMESPACE_BEGIN

class [[nodiscard]] alignas(JPH_VECTOR_ALIGNMENT) Vec4
{
public:
	JPH_OVERRIDE_NEW_DELETE

	// Underlying vector type
#if defined(JPH_USE_SSE)
	using Type = __m128;
#elif defined(JPH_USE_NEON)
	using Type = float32x4_t;
#else
	using Type = struct { float mData[4]; };
#endif

	/// Constructor
								Vec4() = default; ///< Intentionally not initialized for performance reasons
								Vec4(const Vec4 &inRHS) = default;
	Vec4 &						operator = (const Vec4 &inRHS) = default;
	explicit JPH_INLINE			Vec4(Vec3Arg inRHS);							///< WARNING: W component undefined!
	JPH_INLINE					Vec4(Vec3Arg inRHS, float inW);
	JPH_INLINE					Vec4(Type inRHS) : mValue(inRHS)				{ }

	/// Create a vector from 4 components
	JPH_INLINE					Vec4(float inX, float inY, float inZ, float inW);

	/// Vector with all zeros
	static JPH_INLINE Vec4		sZero();

	/// Vector with all NaN's
	static JPH_INLINE Vec4		sNaN();

	/// Replicate inV across all components
	static JPH_INLINE Vec4		sReplicate(float inV);

	/// Load 4 floats from memory
	static JPH_INLINE Vec4		sLoadFloat4(const Float4 *inV);

	/// Load 4 floats from memory, 16 bytes aligned
	static JPH_INLINE Vec4		sLoadFloat4Aligned(const Float4 *inV);

	/// Gather 4 floats from memory at inBase + inOffsets[i] * Scale
	template <const int Scale>
	static JPH_INLINE Vec4		sGatherFloat4(const float *inBase, UVec4Arg inOffsets);

	/// Return the minimum value of each of the components
	static JPH_INLINE Vec4		sMin(Vec4Arg inV1, Vec4Arg inV2);

	/// Return the maximum of each of the components
	static JPH_INLINE Vec4		sMax(Vec4Arg inV1, Vec4Arg inV2);

	/// Equals (component wise)
	static JPH_INLINE UVec4		sEquals(Vec4Arg inV1, Vec4Arg inV2);

	/// Less than (component wise)
	static JPH_INLINE UVec4		sLess(Vec4Arg inV1, Vec4Arg inV2);

	/// Less than or equal (component wise)
	static JPH_INLINE UVec4		sLessOrEqual(Vec4Arg inV1, Vec4Arg inV2);

	/// Greater than (component wise)
	static JPH_INLINE UVec4		sGreater(Vec4Arg inV1, Vec4Arg inV2);

	/// Greater than or equal (component wise)
	static JPH_INLINE UVec4		sGreaterOrEqual(Vec4Arg inV1, Vec4Arg inV2);

	/// Calculates inMul1 * inMul2 + inAdd
	static JPH_INLINE Vec4		sFusedMultiplyAdd(Vec4Arg inMul1, Vec4Arg inMul2, Vec4Arg inAdd);

	/// Component wise select, returns inNotSet when highest bit of inControl = 0 and inSet when highest bit of inControl = 1
	static JPH_INLINE Vec4		sSelect(Vec4Arg inNotSet, Vec4Arg inSet, UVec4Arg inControl);

	/// Logical or (component wise)
	static JPH_INLINE Vec4		sOr(Vec4Arg inV1, Vec4Arg inV2);

	/// Logical xor (component wise)
	static JPH_INLINE Vec4		sXor(Vec4Arg inV1, Vec4Arg inV2);

	/// Logical and (component wise)
	static JPH_INLINE Vec4		sAnd(Vec4Arg inV1, Vec4Arg inV2);

	/// Sort the four elements of ioValue and sort ioIndex at the same time.
	/// Based on a sorting network: http://en.wikipedia.org/wiki/Sorting_network
	static JPH_INLINE void		sSort4(Vec4 &ioValue, UVec4 &ioIndex);

	/// Reverse sort the four elements of ioValue (highest first) and sort ioIndex at the same time.
	/// Based on a sorting network: http://en.wikipedia.org/wiki/Sorting_network
	static JPH_INLINE void		sSort4Reverse(Vec4 &ioValue, UVec4 &ioIndex);

	/// Get individual components
#if defined(JPH_USE_SSE)
	JPH_INLINE float			GetX() const									{ return _mm_cvtss_f32(mValue); }
	JPH_INLINE float			GetY() const									{ return mF32[1]; }
	JPH_INLINE float			GetZ() const									{ return mF32[2]; }
	JPH_INLINE float			GetW() const									{ return mF32[3]; }
#elif defined(JPH_USE_NEON)
	JPH_INLINE float			GetX() const									{ return vgetq_lane_f32(mValue, 0); }
	JPH_INLINE float			GetY() const									{ return vgetq_lane_f32(mValue, 1); }
	JPH_INLINE float			GetZ() const									{ return vgetq_lane_f32(mValue, 2); }
	JPH_INLINE float			GetW() const									{ return vgetq_lane_f32(mValue, 3); }
#else
	JPH_INLINE float			GetX() const									{ return mF32[0]; }
	JPH_INLINE float			GetY() const									{ return mF32[1]; }
	JPH_INLINE float			GetZ() const									{ return mF32[2]; }
	JPH_INLINE float			GetW() const									{ return mF32[3]; }
#endif

	/// Set individual components
	JPH_INLINE void				SetX(float inX)									{ mF32[0] = inX; }
	JPH_INLINE void				SetY(float inY)									{ mF32[1] = inY; }
	JPH_INLINE void				SetZ(float inZ)									{ mF32[2] = inZ; }
	JPH_INLINE void				SetW(float inW)									{ mF32[3] = inW; }

	/// Set all components
	JPH_INLINE void				Set(float inX, float inY, float inZ, float inW)	{ *this = Vec4(inX, inY, inZ, inW); }

	/// Get float component by index
	JPH_INLINE float			operator [] (uint inCoordinate) const			{ JPH_ASSERT(inCoordinate < 4); return mF32[inCoordinate]; }
	JPH_INLINE float &			operator [] (uint inCoordinate)					{ JPH_ASSERT(inCoordinate < 4); return mF32[inCoordinate]; }

	/// Comparison
	JPH_INLINE bool				operator == (Vec4Arg inV2) const;
	JPH_INLINE bool				operator != (Vec4Arg inV2) const			{ return !(*this == inV2); }

	/// Test if two vectors are close
	JPH_INLINE bool				IsClose(Vec4Arg inV2, float inMaxDistSq = 1.0e-12f) const;

	/// Test if vector is normalized
	JPH_INLINE bool				IsNormalized(float inTolerance = 1.0e-6f) const;

	/// Test if vector contains NaN elements
	JPH_INLINE bool				IsNaN() const;

	/// Multiply two float vectors (component wise)
	JPH_INLINE Vec4				operator * (Vec4Arg inV2) const;

	/// Multiply vector with float
	JPH_INLINE Vec4				operator * (float inV2) const;

	/// Multiply vector with float
	friend JPH_INLINE Vec4		operator * (float inV1, Vec4Arg inV2);

	/// Divide vector by float
	JPH_INLINE Vec4				operator / (float inV2) const;

	/// Multiply vector with float
	JPH_INLINE Vec4 &			operator *= (float inV2);

	/// Multiply vector with vector
	JPH_INLINE Vec4 &			operator *= (Vec4Arg inV2);

	/// Divide vector by float
	JPH_INLINE Vec4 &			operator /= (float inV2);

	/// Add two float vectors (component wise)
	JPH_INLINE Vec4				operator + (Vec4Arg inV2) const;

	/// Add two float vectors (component wise)
	JPH_INLINE Vec4 &			operator += (Vec4Arg inV2);

	/// Negate
	JPH_INLINE Vec4				operator - () const;

	/// Subtract two float vectors (component wise)
	JPH_INLINE Vec4				operator - (Vec4Arg inV2) const;

	/// Subtract two float vectors (component wise)
	JPH_INLINE Vec4 &			operator -= (Vec4Arg inV2);

	/// Divide (component wise)
	JPH_INLINE Vec4				operator / (Vec4Arg inV2) const;

	/// Swizzle the elements in inV
	template<uint32 SwizzleX, uint32 SwizzleY, uint32 SwizzleZ, uint32 SwizzleW>
	JPH_INLINE Vec4				Swizzle() const;

	/// Replicate the X component to all components
	JPH_INLINE Vec4				SplatX() const;

	/// Replicate the Y component to all components
	JPH_INLINE Vec4				SplatY() const;

	/// Replicate the Z component to all components
	JPH_INLINE Vec4				SplatZ() const;

	/// Replicate the W component to all components
	JPH_INLINE Vec4				SplatW() const;

	/// Return the absolute value of each of the components
	JPH_INLINE Vec4				Abs() const;

	/// Reciprocal vector (1 / value) for each of the components
	JPH_INLINE Vec4				Reciprocal() const;

	/// Dot product, returns the dot product in X, Y and Z components
	JPH_INLINE Vec4				DotV(Vec4Arg inV2) const;

	/// Dot product
	JPH_INLINE float			Dot(Vec4Arg inV2) const;

	/// Squared length of vector
	JPH_INLINE float			LengthSq() const;

	/// Length of vector
	JPH_INLINE float			Length() const;

	/// Normalize vector
	JPH_INLINE Vec4				Normalized() const;

	/// Store 4 floats to memory
	JPH_INLINE void				StoreFloat4(Float4 *outV) const;

	/// Convert each component from a float to an int
	JPH_INLINE UVec4			ToInt() const;

	/// Reinterpret Vec4 as a UVec4 (doesn't change the bits)
	JPH_INLINE UVec4			ReinterpretAsInt() const;

	/// Store if X is negative in bit 0, Y in bit 1, Z in bit 2 and W in bit 3
	JPH_INLINE int				GetSignBits() const;

	/// Get the minimum of X, Y, Z and W
	JPH_INLINE float			ReduceMin() const;

	/// Get the maximum of X, Y, Z and W
	JPH_INLINE float			ReduceMax() const;

	/// Component wise square root
	JPH_INLINE Vec4				Sqrt() const;

	/// Get vector that contains the sign of each element (returns 1.0f if positive, -1.0f if negative)
	JPH_INLINE Vec4				GetSign() const;

	/// Calculate the sine and cosine for each element of this vector (input in radians)
	inline void					SinCos(Vec4 &outSin, Vec4 &outCos) const;

	/// Calculate the tangent for each element of this vector (input in radians)
	inline Vec4					Tan() const;

	/// Calculate the arc sine for each element of this vector (returns value in the range [-PI / 2, PI / 2])
	/// Note that all input values will be clamped to the range [-1, 1] and this function will not return NaNs like std::asin
	inline Vec4					ASin() const;

	/// Calculate the arc cosine for each element of this vector (returns value in the range [0, PI])
	/// Note that all input values will be clamped to the range [-1, 1] and this function will not return NaNs like std::acos
	inline Vec4					ACos() const;

	/// Calculate the arc tangent for each element of this vector (returns value in the range [-PI / 2, PI / 2])
	inline Vec4					ATan() const;

	/// Calculate the arc tangent of y / x using the signs of the arguments to determine the correct quadrant (returns value in the range [-PI, PI])
	inline static Vec4			sATan2(Vec4Arg inY, Vec4Arg inX);

	/// To String
	friend ostream &			operator << (ostream &inStream, Vec4Arg inV)
	{
		inStream << inV.mF32[0] << ", " << inV.mF32[1] << ", " << inV.mF32[2] << ", " << inV.mF32[3];
		return inStream;
	}

	union
	{
		Type					mValue;
		float					mF32[4];
	};
};

static_assert(std::is_trivial<Vec4>(), "Is supposed to be a trivial type!");

JPH_NAMESPACE_END

#include "Vec4.inl"

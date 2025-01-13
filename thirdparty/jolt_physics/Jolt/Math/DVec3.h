// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Jolt/Math/Double3.h>

JPH_NAMESPACE_BEGIN

/// 3 component vector of doubles (stored as 4 vectors).
/// Note that we keep the 4th component the same as the 3rd component to avoid divisions by zero when JPH_FLOATING_POINT_EXCEPTIONS_ENABLED defined
class [[nodiscard]] alignas(JPH_DVECTOR_ALIGNMENT) DVec3
{
public:
	JPH_OVERRIDE_NEW_DELETE

	// Underlying vector type
#if defined(JPH_USE_AVX)
	using Type = __m256d;
	using TypeArg = __m256d;
#elif defined(JPH_USE_SSE)
	using Type = struct { __m128d mLow, mHigh; };
	using TypeArg = const Type &;
#elif defined(JPH_USE_NEON)
	using Type = float64x2x2_t;
	using TypeArg = const Type &;
#else
	using Type = struct { double mData[4]; };
	using TypeArg = const Type &;
#endif

	// Argument type
	using ArgType = DVec3Arg;

	/// Constructor
								DVec3() = default; ///< Intentionally not initialized for performance reasons
								DVec3(const DVec3 &inRHS) = default;
	DVec3 &						operator = (const DVec3 &inRHS) = default;
	JPH_INLINE explicit			DVec3(Vec3Arg inRHS);
	JPH_INLINE explicit			DVec3(Vec4Arg inRHS);
	JPH_INLINE					DVec3(TypeArg inRHS) : mValue(inRHS)			{ CheckW(); }

	/// Create a vector from 3 components
	JPH_INLINE					DVec3(double inX, double inY, double inZ);

	/// Load 3 doubles from memory
	explicit JPH_INLINE			DVec3(const Double3 &inV);

	/// Vector with all zeros
	static JPH_INLINE DVec3		sZero();

	/// Vectors with the principal axis
	static JPH_INLINE DVec3		sAxisX()										{ return DVec3(1, 0, 0); }
	static JPH_INLINE DVec3		sAxisY()										{ return DVec3(0, 1, 0); }
	static JPH_INLINE DVec3		sAxisZ()										{ return DVec3(0, 0, 1); }

	/// Replicate inV across all components
	static JPH_INLINE DVec3		sReplicate(double inV);

	/// Vector with all NaN's
	static JPH_INLINE DVec3		sNaN();

	/// Load 3 doubles from memory (reads 64 bits extra which it doesn't use)
	static JPH_INLINE DVec3		sLoadDouble3Unsafe(const Double3 &inV);

	/// Store 3 doubles to memory
	JPH_INLINE void				StoreDouble3(Double3 *outV) const;

	/// Convert to float vector 3 rounding to nearest
	JPH_INLINE explicit			operator Vec3() const;

	/// Prepare to convert to float vector 3 rounding towards zero (returns DVec3 that can be converted to a Vec3 to get the rounding)
	JPH_INLINE DVec3			PrepareRoundToZero() const;

	/// Prepare to convert to float vector 3 rounding towards positive/negative inf (returns DVec3 that can be converted to a Vec3 to get the rounding)
	JPH_INLINE DVec3			PrepareRoundToInf() const;

	/// Convert to float vector 3 rounding down
	JPH_INLINE Vec3				ToVec3RoundDown() const;

	/// Convert to float vector 3 rounding up
	JPH_INLINE Vec3				ToVec3RoundUp() const;

	/// Return the minimum value of each of the components
	static JPH_INLINE DVec3		sMin(DVec3Arg inV1, DVec3Arg inV2);

	/// Return the maximum of each of the components
	static JPH_INLINE DVec3		sMax(DVec3Arg inV1, DVec3Arg inV2);

	/// Clamp a vector between min and max (component wise)
	static JPH_INLINE DVec3		sClamp(DVec3Arg inV, DVec3Arg inMin, DVec3Arg inMax);

	/// Equals (component wise)
	static JPH_INLINE DVec3		sEquals(DVec3Arg inV1, DVec3Arg inV2);

	/// Less than (component wise)
	static JPH_INLINE DVec3		sLess(DVec3Arg inV1, DVec3Arg inV2);

	/// Less than or equal (component wise)
	static JPH_INLINE DVec3		sLessOrEqual(DVec3Arg inV1, DVec3Arg inV2);

	/// Greater than (component wise)
	static JPH_INLINE DVec3		sGreater(DVec3Arg inV1, DVec3Arg inV2);

	/// Greater than or equal (component wise)
	static JPH_INLINE DVec3		sGreaterOrEqual(DVec3Arg inV1, DVec3Arg inV2);

	/// Calculates inMul1 * inMul2 + inAdd
	static JPH_INLINE DVec3		sFusedMultiplyAdd(DVec3Arg inMul1, DVec3Arg inMul2, DVec3Arg inAdd);

	/// Component wise select, returns inNotSet when highest bit of inControl = 0 and inSet when highest bit of inControl = 1
	static JPH_INLINE DVec3		sSelect(DVec3Arg inNotSet, DVec3Arg inSet, DVec3Arg inControl);

	/// Logical or (component wise)
	static JPH_INLINE DVec3		sOr(DVec3Arg inV1, DVec3Arg inV2);

	/// Logical xor (component wise)
	static JPH_INLINE DVec3		sXor(DVec3Arg inV1, DVec3Arg inV2);

	/// Logical and (component wise)
	static JPH_INLINE DVec3		sAnd(DVec3Arg inV1, DVec3Arg inV2);

	/// Store if X is true in bit 0, Y in bit 1, Z in bit 2 and W in bit 3 (true is when highest bit of component is set)
	JPH_INLINE int				GetTrues() const;

	/// Test if any of the components are true (true is when highest bit of component is set)
	JPH_INLINE bool				TestAnyTrue() const;

	/// Test if all components are true (true is when highest bit of component is set)
	JPH_INLINE bool				TestAllTrue() const;

	/// Get individual components
#if defined(JPH_USE_AVX)
	JPH_INLINE double			GetX() const									{ return _mm_cvtsd_f64(_mm256_castpd256_pd128(mValue)); }
	JPH_INLINE double			GetY() const									{ return mF64[1]; }
	JPH_INLINE double			GetZ() const									{ return mF64[2]; }
#elif defined(JPH_USE_SSE)
	JPH_INLINE double			GetX() const									{ return _mm_cvtsd_f64(mValue.mLow); }
	JPH_INLINE double			GetY() const									{ return mF64[1]; }
	JPH_INLINE double			GetZ() const									{ return _mm_cvtsd_f64(mValue.mHigh); }
#elif defined(JPH_USE_NEON)
	JPH_INLINE double			GetX() const									{ return vgetq_lane_f64(mValue.val[0], 0); }
	JPH_INLINE double			GetY() const									{ return vgetq_lane_f64(mValue.val[0], 1); }
	JPH_INLINE double			GetZ() const									{ return vgetq_lane_f64(mValue.val[1], 0); }
#else
	JPH_INLINE double			GetX() const									{ return mF64[0]; }
	JPH_INLINE double			GetY() const									{ return mF64[1]; }
	JPH_INLINE double			GetZ() const									{ return mF64[2]; }
#endif

	/// Set individual components
	JPH_INLINE void				SetX(double inX)								{ mF64[0] = inX; }
	JPH_INLINE void				SetY(double inY)								{ mF64[1] = inY; }
	JPH_INLINE void				SetZ(double inZ)								{ mF64[2] = mF64[3] = inZ; } // Assure Z and W are the same

	/// Set all components
	JPH_INLINE void				Set(double inX, double inY, double inZ)			{ *this = DVec3(inX, inY, inZ); }

	/// Get double component by index
	JPH_INLINE double			operator [] (uint inCoordinate) const			{ JPH_ASSERT(inCoordinate < 3); return mF64[inCoordinate]; }

	/// Set double component by index
	JPH_INLINE void				SetComponent(uint inCoordinate, double inValue)	{ JPH_ASSERT(inCoordinate < 3); mF64[inCoordinate] = inValue; mValue = sFixW(mValue); } // Assure Z and W are the same

	/// Comparison
	JPH_INLINE bool				operator == (DVec3Arg inV2) const;
	JPH_INLINE bool				operator != (DVec3Arg inV2) const				{ return !(*this == inV2); }

	/// Test if two vectors are close
	JPH_INLINE bool				IsClose(DVec3Arg inV2, double inMaxDistSq = 1.0e-24) const;

	/// Test if vector is near zero
	JPH_INLINE bool				IsNearZero(double inMaxDistSq = 1.0e-24) const;

	/// Test if vector is normalized
	JPH_INLINE bool				IsNormalized(double inTolerance = 1.0e-12) const;

	/// Test if vector contains NaN elements
	JPH_INLINE bool				IsNaN() const;

	/// Multiply two double vectors (component wise)
	JPH_INLINE DVec3			operator * (DVec3Arg inV2) const;

	/// Multiply vector with double
	JPH_INLINE DVec3			operator * (double inV2) const;

	/// Multiply vector with double
	friend JPH_INLINE DVec3		operator * (double inV1, DVec3Arg inV2);

	/// Divide vector by double
	JPH_INLINE DVec3			operator / (double inV2) const;

	/// Multiply vector with double
	JPH_INLINE DVec3 &			operator *= (double inV2);

	/// Multiply vector with vector
	JPH_INLINE DVec3 &			operator *= (DVec3Arg inV2);

	/// Divide vector by double
	JPH_INLINE DVec3 &			operator /= (double inV2);

	/// Add two vectors (component wise)
	JPH_INLINE DVec3			operator + (Vec3Arg inV2) const;

	/// Add two double vectors (component wise)
	JPH_INLINE DVec3			operator + (DVec3Arg inV2) const;

	/// Add two vectors (component wise)
	JPH_INLINE DVec3 &			operator += (Vec3Arg inV2);

	/// Add two double vectors (component wise)
	JPH_INLINE DVec3 &			operator += (DVec3Arg inV2);

	/// Negate
	JPH_INLINE DVec3			operator - () const;

	/// Subtract two vectors (component wise)
	JPH_INLINE DVec3			operator - (Vec3Arg inV2) const;

	/// Subtract two double vectors (component wise)
	JPH_INLINE DVec3			operator - (DVec3Arg inV2) const;

	/// Subtract two vectors (component wise)
	JPH_INLINE DVec3 &			operator -= (Vec3Arg inV2);

	/// Subtract two vectors (component wise)
	JPH_INLINE DVec3 &			operator -= (DVec3Arg inV2);

	/// Divide (component wise)
	JPH_INLINE DVec3			operator / (DVec3Arg inV2) const;

	/// Return the absolute value of each of the components
	JPH_INLINE DVec3			Abs() const;

	/// Reciprocal vector (1 / value) for each of the components
	JPH_INLINE DVec3			Reciprocal() const;

	/// Cross product
	JPH_INLINE DVec3			Cross(DVec3Arg inV2) const;

	/// Dot product
	JPH_INLINE double			Dot(DVec3Arg inV2) const;

	/// Squared length of vector
	JPH_INLINE double			LengthSq() const;

	/// Length of vector
	JPH_INLINE double			Length() const;

	/// Normalize vector
	JPH_INLINE DVec3			Normalized() const;

	/// Component wise square root
	JPH_INLINE DVec3			Sqrt() const;

	/// Get vector that contains the sign of each element (returns 1 if positive, -1 if negative)
	JPH_INLINE DVec3			GetSign() const;

	/// To String
	friend ostream &			operator << (ostream &inStream, DVec3Arg inV)
	{
		inStream << inV.mF64[0] << ", " << inV.mF64[1] << ", " << inV.mF64[2];
		return inStream;
	}

	/// Internal helper function that checks that W is equal to Z, so e.g. dividing by it should not generate div by 0
	JPH_INLINE void				CheckW() const;

	/// Internal helper function that ensures that the Z component is replicated to the W component to prevent divisions by zero
	static JPH_INLINE Type		sFixW(TypeArg inValue);

	/// Representations of true and false for boolean operations
	inline static const double	cTrue = BitCast<double>(~uint64(0));
	inline static const double	cFalse = 0.0;

	union
	{
		Type					mValue;
		double					mF64[4];
	};
};

static_assert(std::is_trivial<DVec3>(), "Is supposed to be a trivial type!");

JPH_NAMESPACE_END

#include "DVec3.inl"

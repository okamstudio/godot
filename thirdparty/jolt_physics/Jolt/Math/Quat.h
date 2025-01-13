// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Jolt/Math/Vec3.h>
#include <Jolt/Math/Vec4.h>

JPH_NAMESPACE_BEGIN

/// Quaternion class, quaternions are 4 dimensional vectors which can describe rotations in 3 dimensional
/// space if their length is 1.
///
/// They are written as:
///
/// \f$q = w + x \: i + y \: j + z \: k\f$
///
/// or in vector notation:
///
/// \f$q = [w, v] = [w, x, y, z]\f$
///
/// Where:
///
/// w = the real part
/// v = the imaginary part, (x, y, z)
///
/// Note that we store the quaternion in a Vec4 as [x, y, z, w] because that makes
/// it easy to extract the rotation axis of the quaternion:
///
/// q = [cos(angle / 2), sin(angle / 2) * rotation_axis]
class [[nodiscard]] alignas(JPH_VECTOR_ALIGNMENT) Quat
{
public:
	JPH_OVERRIDE_NEW_DELETE

	///@name Constructors
	///@{
	inline						Quat() = default; ///< Intentionally not initialized for performance reasons
								Quat(const Quat &inRHS) = default;
	Quat &						operator = (const Quat &inRHS) = default;
	inline						Quat(float inX, float inY, float inZ, float inW)				: mValue(inX, inY, inZ, inW) { }
	inline explicit				Quat(Vec4Arg inV)												: mValue(inV) { }
	///@}

	///@name Tests
	///@{

	/// Check if two quaternions are exactly equal
	inline bool					operator == (QuatArg inRHS) const								{ return mValue == inRHS.mValue; }

	/// Check if two quaternions are different
	inline bool					operator != (QuatArg inRHS) const								{ return mValue != inRHS.mValue; }

	/// If this quaternion is close to inRHS. Note that q and -q represent the same rotation, this is not checked here.
	inline bool					IsClose(QuatArg inRHS, float inMaxDistSq = 1.0e-12f) const		{ return mValue.IsClose(inRHS.mValue, inMaxDistSq); }

	/// If the length of this quaternion is 1 +/- inTolerance
	inline bool					IsNormalized(float inTolerance = 1.0e-5f) const					{ return mValue.IsNormalized(inTolerance); }

	/// If any component of this quaternion is a NaN (not a number)
	inline bool					IsNaN() const													{ return mValue.IsNaN(); }

	///@}
	///@name Get components
	///@{

	/// Get X component (imaginary part i)
	JPH_INLINE float			GetX() const													{ return mValue.GetX(); }

	/// Get Y component (imaginary part j)
	JPH_INLINE float			GetY() const													{ return mValue.GetY(); }

	/// Get Z component (imaginary part k)
	JPH_INLINE float			GetZ() const													{ return mValue.GetZ(); }

	/// Get W component (real part)
	JPH_INLINE float			GetW() const													{ return mValue.GetW(); }

	/// Get the imaginary part of the quaternion
	JPH_INLINE Vec3				GetXYZ() const													{ return Vec3(mValue); }

	/// Get the quaternion as a Vec4
	JPH_INLINE Vec4				GetXYZW() const													{ return mValue; }

	/// Set individual components
	JPH_INLINE void				SetX(float inX)													{ mValue.SetX(inX); }
	JPH_INLINE void				SetY(float inY)													{ mValue.SetY(inY); }
	JPH_INLINE void				SetZ(float inZ)													{ mValue.SetZ(inZ); }
	JPH_INLINE void				SetW(float inW)													{ mValue.SetW(inW); }

	/// Set all components
	JPH_INLINE void				Set(float inX, float inY, float inZ, float inW)					{ mValue.Set(inX, inY, inZ, inW); }

	///@}
	///@name Default quaternions
	///@{

	/// @return [0, 0, 0, 0]
	JPH_INLINE static Quat		sZero()															{ return Quat(Vec4::sZero()); }

	/// @return [1, 0, 0, 0] (or in storage format Quat(0, 0, 0, 1))
	JPH_INLINE static Quat		sIdentity()														{ return Quat(0, 0, 0, 1); }

	///@}

	/// Rotation from axis and angle
	JPH_INLINE static Quat		sRotation(Vec3Arg inAxis, float inAngle);

	/// Get axis and angle that represents this quaternion, outAngle will always be in the range \f$[0, \pi]\f$
	JPH_INLINE void				GetAxisAngle(Vec3 &outAxis, float &outAngle) const;

	/// Create quaternion that rotates a vector from the direction of inFrom to the direction of inTo along the shortest path
	/// @see https://www.euclideanspace.com/maths/algebra/vectors/angleBetween/index.htm
	JPH_INLINE static Quat		sFromTo(Vec3Arg inFrom, Vec3Arg inTo);

	/// Random unit quaternion
	template <class Random>
	inline static Quat			sRandom(Random &inRandom);

	/// Conversion from Euler angles. Rotation order is X then Y then Z (RotZ * RotY * RotX). Angles in radians.
	inline static Quat			sEulerAngles(Vec3Arg inAngles);

	/// Conversion to Euler angles. Rotation order is X then Y then Z (RotZ * RotY * RotX). Angles in radians.
	inline Vec3					GetEulerAngles() const;

	///@name Length / normalization operations
	///@{

	/// Squared length of quaternion.
	/// @return Squared length of quaternion (\f$|v|^2\f$)
	JPH_INLINE float			LengthSq() const												{ return mValue.LengthSq(); }

	/// Length of quaternion.
	/// @return Length of quaternion (\f$|v|\f$)
	JPH_INLINE float			Length() const													{ return mValue.Length(); }

	/// Normalize the quaternion (make it length 1)
	JPH_INLINE Quat				Normalized() const												{ return Quat(mValue.Normalized()); }

	///@}
	///@name Additions / multiplications
	///@{

	JPH_INLINE void				operator += (QuatArg inRHS)										{ mValue += inRHS.mValue; }
	JPH_INLINE void				operator -= (QuatArg inRHS)										{ mValue -= inRHS.mValue; }
	JPH_INLINE void				operator *= (float inValue)										{ mValue *= inValue; }
	JPH_INLINE void				operator /= (float inValue)										{ mValue /= inValue; }
	JPH_INLINE Quat				operator - () const												{ return Quat(-mValue); }
	JPH_INLINE Quat				operator + (QuatArg inRHS) const								{ return Quat(mValue + inRHS.mValue); }
	JPH_INLINE Quat				operator - (QuatArg inRHS) const								{ return Quat(mValue - inRHS.mValue); }
	JPH_INLINE Quat				operator * (QuatArg inRHS) const;
	JPH_INLINE Quat				operator * (float inValue) const								{ return Quat(mValue * inValue); }
	inline friend Quat			operator * (float inValue, QuatArg inRHS)						{ return Quat(inRHS.mValue * inValue); }
	JPH_INLINE Quat				operator / (float inValue) const								{ return Quat(mValue / inValue); }

	///@}

	/// Rotate a vector by this quaternion
	JPH_INLINE Vec3				operator * (Vec3Arg inValue) const;

	/// Rotate a vector by the inverse of this quaternion
	JPH_INLINE Vec3				InverseRotate(Vec3Arg inValue) const;

	/// Rotate a the vector (1, 0, 0) with this quaternion
	JPH_INLINE Vec3				RotateAxisX() const;

	/// Rotate a the vector (0, 1, 0) with this quaternion
	JPH_INLINE Vec3				RotateAxisY() const;

	/// Rotate a the vector (0, 0, 1) with this quaternion
	JPH_INLINE Vec3				RotateAxisZ() const;

	/// Dot product
	JPH_INLINE float			Dot(QuatArg inRHS) const										{ return mValue.Dot(inRHS.mValue); }

	/// The conjugate [w, -x, -y, -z] is the same as the inverse for unit quaternions
	JPH_INLINE Quat				Conjugated() const												{ return Quat(Vec4::sXor(mValue, UVec4(0x80000000, 0x80000000, 0x80000000, 0).ReinterpretAsFloat())); }

	/// Get inverse quaternion
	JPH_INLINE Quat				Inversed() const												{ return Conjugated() / Length(); }

	/// Ensures that the W component is positive by negating the entire quaternion if it is not. This is useful when you want to store a quaternion as a 3 vector by discarding W and reconstructing it as sqrt(1 - x^2 - y^2 - z^2).
	JPH_INLINE Quat				EnsureWPositive() const											{ return Quat(Vec4::sXor(mValue, Vec4::sAnd(mValue.SplatW(), UVec4::sReplicate(0x80000000).ReinterpretAsFloat()))); }

	/// Get a quaternion that is perpendicular to this quaternion
	JPH_INLINE Quat				GetPerpendicular() const										{ return Quat(Vec4(1, -1, 1, -1) * mValue.Swizzle<SWIZZLE_Y, SWIZZLE_X, SWIZZLE_W, SWIZZLE_Z>()); }

	/// Get rotation angle around inAxis (uses Swing Twist Decomposition to get the twist quaternion and uses q(axis, angle) = [cos(angle / 2), axis * sin(angle / 2)])
	JPH_INLINE float			GetRotationAngle(Vec3Arg inAxis) const							{ return GetW() == 0.0f? JPH_PI : 2.0f * ATan(GetXYZ().Dot(inAxis) / GetW()); }

	/// Swing Twist Decomposition: any quaternion can be split up as:
	///
	/// \f[q = q_{swing} \: q_{twist}\f]
	///
	/// where \f$q_{twist}\f$ rotates only around axis v.
	///
	/// \f$q_{twist}\f$ is:
	///
	/// \f[q_{twist} = \frac{[q_w, q_{ijk} \cdot v \: v]}{\left|[q_w, q_{ijk} \cdot v \: v]\right|}\f]
	///
	/// where q_w is the real part of the quaternion and q_i the imaginary part (a 3 vector).
	///
	/// The swing can then be calculated as:
	///
	/// \f[q_{swing} = q \: q_{twist}^* \f]
	///
	/// Where \f$q_{twist}^*\f$ = complex conjugate of \f$q_{twist}\f$
	JPH_INLINE Quat				GetTwist(Vec3Arg inAxis) const;

	/// Decomposes quaternion into swing and twist component:
	///
	/// \f$q = q_{swing} \: q_{twist}\f$
	///
	/// where \f$q_{swing} \: \hat{x} = q_{twist} \: \hat{y} = q_{twist} \: \hat{z} = 0\f$
	///
	/// In other words:
	///
	/// - \f$q_{twist}\f$ only rotates around the X-axis.
	/// - \f$q_{swing}\f$ only rotates around the Y and Z-axis.
	///
	/// @see Gino van den Bergen - Rotational Joint Limits in Quaternion Space - GDC 2016
	JPH_INLINE void				GetSwingTwist(Quat &outSwing, Quat &outTwist) const;

	/// Linear interpolation between two quaternions (for small steps).
	/// @param inFraction is in the range [0, 1]
	/// @param inDestination The destination quaternion
	/// @return (1 - inFraction) * this + fraction * inDestination
	JPH_INLINE Quat				LERP(QuatArg inDestination, float inFraction) const;

	/// Spherical linear interpolation between two quaternions.
	/// @param inFraction is in the range [0, 1]
	/// @param inDestination The destination quaternion
	/// @return When fraction is zero this quaternion is returned, when fraction is 1 inDestination is returned.
	/// When fraction is between 0 and 1 an interpolation along the shortest path is returned.
	JPH_INLINE Quat				SLERP(QuatArg inDestination, float inFraction) const;

	/// Load 3 floats from memory (X, Y and Z component and then calculates W) reads 32 bits extra which it doesn't use
	static JPH_INLINE Quat		sLoadFloat3Unsafe(const Float3 &inV);

	/// Store 3 as floats to memory (X, Y and Z component)
	JPH_INLINE void				StoreFloat3(Float3 *outV) const;

	/// To String
	friend ostream &			operator << (ostream &inStream, QuatArg inQ)					{ inStream << inQ.mValue; return inStream; }

	/// 4 vector that stores [x, y, z, w] parts of the quaternion
	Vec4						mValue;
};

static_assert(std::is_trivial<Quat>(), "Is supposed to be a trivial type!");

JPH_NAMESPACE_END

#include "Quat.inl"

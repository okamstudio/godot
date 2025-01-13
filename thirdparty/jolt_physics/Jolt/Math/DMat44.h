// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2022 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Jolt/Math/MathTypes.h>

JPH_NAMESPACE_BEGIN

/// Holds a 4x4 matrix of floats with the last column consisting of doubles
class [[nodiscard]] alignas(JPH_DVECTOR_ALIGNMENT) DMat44
{
public:
	JPH_OVERRIDE_NEW_DELETE

	// Underlying column type
	using Type = Vec4::Type;
	using DType = DVec3::Type;
	using DTypeArg = DVec3::TypeArg;

	// Argument type
	using ArgType = DMat44Arg;

	/// Constructor
								DMat44() = default; ///< Intentionally not initialized for performance reasons
	JPH_INLINE					DMat44(Vec4Arg inC1, Vec4Arg inC2, Vec4Arg inC3, DVec3Arg inC4);
								DMat44(const DMat44 &inM2) = default;
	DMat44 &					operator = (const DMat44 &inM2) = default;
	JPH_INLINE explicit			DMat44(Mat44Arg inM);
	JPH_INLINE					DMat44(Mat44Arg inRot, DVec3Arg inT);
	JPH_INLINE					DMat44(Type inC1, Type inC2, Type inC3, DTypeArg inC4);

	/// Zero matrix
	static JPH_INLINE DMat44	sZero();

	/// Identity matrix
	static JPH_INLINE DMat44	sIdentity();

	/// Rotate from quaternion
	static JPH_INLINE DMat44	sRotation(QuatArg inQuat)								{ return DMat44(Mat44::sRotation(inQuat), DVec3::sZero()); }

	/// Get matrix that translates
	static JPH_INLINE DMat44	sTranslation(DVec3Arg inV)								{ return DMat44(Vec4(1, 0, 0, 0), Vec4(0, 1, 0, 0), Vec4(0, 0, 1, 0), inV); }

	/// Get matrix that rotates and translates
	static JPH_INLINE DMat44	sRotationTranslation(QuatArg inR, DVec3Arg inT)			{ return DMat44(Mat44::sRotation(inR), inT); }

	/// Get inverse matrix of sRotationTranslation
	static JPH_INLINE DMat44	sInverseRotationTranslation(QuatArg inR, DVec3Arg inT);

	/// Get matrix that scales (produces a matrix with (inV, 1) on its diagonal)
	static JPH_INLINE DMat44	sScale(Vec3Arg inV)										{ return DMat44(Mat44::sScale(inV), DVec3::sZero()); }

	/// Convert to Mat44 rounding to nearest
	JPH_INLINE Mat44			ToMat44() const											{ return Mat44(mCol[0], mCol[1], mCol[2], Vec3(mCol3)); }

	/// Comparison
	JPH_INLINE bool				operator == (DMat44Arg inM2) const;
	JPH_INLINE bool				operator != (DMat44Arg inM2) const						{ return !(*this == inM2); }

	/// Test if two matrices are close
	JPH_INLINE bool				IsClose(DMat44Arg inM2, float inMaxDistSq = 1.0e-12f) const;

	/// Multiply matrix by matrix
	JPH_INLINE DMat44			operator * (Mat44Arg inM) const;

	/// Multiply matrix by matrix
	JPH_INLINE DMat44			operator * (DMat44Arg inM) const;

	/// Multiply vector by matrix
	JPH_INLINE DVec3			operator * (Vec3Arg inV) const;

	/// Multiply vector by matrix
	JPH_INLINE DVec3			operator * (DVec3Arg inV) const;

	/// Multiply vector by only 3x3 part of the matrix
	JPH_INLINE Vec3				Multiply3x3(Vec3Arg inV) const							{ return GetRotation().Multiply3x3(inV); }

	/// Multiply vector by only 3x3 part of the matrix
	JPH_INLINE DVec3			Multiply3x3(DVec3Arg inV) const;

	/// Multiply vector by only 3x3 part of the transpose of the matrix (\f$result = this^T \: inV\f$)
	JPH_INLINE Vec3				Multiply3x3Transposed(Vec3Arg inV) const				{ return GetRotation().Multiply3x3Transposed(inV); }

	/// Scale a matrix: result = this * Mat44::sScale(inScale)
	JPH_INLINE DMat44			PreScaled(Vec3Arg inScale) const;

	/// Scale a matrix: result = Mat44::sScale(inScale) * this
	JPH_INLINE DMat44			PostScaled(Vec3Arg inScale) const;

	/// Pre multiply by translation matrix: result = this * Mat44::sTranslation(inTranslation)
	JPH_INLINE DMat44			PreTranslated(Vec3Arg inTranslation) const;

	/// Pre multiply by translation matrix: result = this * Mat44::sTranslation(inTranslation)
	JPH_INLINE DMat44			PreTranslated(DVec3Arg inTranslation) const;

	/// Post multiply by translation matrix: result = Mat44::sTranslation(inTranslation) * this (i.e. add inTranslation to the 4-th column)
	JPH_INLINE DMat44			PostTranslated(Vec3Arg inTranslation) const;

	/// Post multiply by translation matrix: result = Mat44::sTranslation(inTranslation) * this (i.e. add inTranslation to the 4-th column)
	JPH_INLINE DMat44			PostTranslated(DVec3Arg inTranslation) const;

	/// Access to the columns
	JPH_INLINE Vec3				GetAxisX() const										{ return Vec3(mCol[0]); }
	JPH_INLINE void				SetAxisX(Vec3Arg inV)									{ mCol[0] = Vec4(inV, 0.0f); }
	JPH_INLINE Vec3				GetAxisY() const										{ return Vec3(mCol[1]); }
	JPH_INLINE void				SetAxisY(Vec3Arg inV)									{ mCol[1] = Vec4(inV, 0.0f); }
	JPH_INLINE Vec3				GetAxisZ() const										{ return Vec3(mCol[2]); }
	JPH_INLINE void				SetAxisZ(Vec3Arg inV)									{ mCol[2] = Vec4(inV, 0.0f); }
	JPH_INLINE DVec3			GetTranslation() const									{ return mCol3; }
	JPH_INLINE void				SetTranslation(DVec3Arg inV)							{ mCol3 = inV; }
	JPH_INLINE Vec3				GetColumn3(uint inCol) const							{ JPH_ASSERT(inCol < 3); return Vec3(mCol[inCol]); }
	JPH_INLINE void				SetColumn3(uint inCol, Vec3Arg inV)						{ JPH_ASSERT(inCol < 3); mCol[inCol] = Vec4(inV, 0.0f); }
	JPH_INLINE Vec4				GetColumn4(uint inCol) const							{ JPH_ASSERT(inCol < 3); return mCol[inCol]; }
	JPH_INLINE void				SetColumn4(uint inCol, Vec4Arg inV)						{ JPH_ASSERT(inCol < 3); mCol[inCol] = inV; }

	/// Transpose 3x3 subpart of matrix
	JPH_INLINE Mat44			Transposed3x3() const									{ return GetRotation().Transposed3x3(); }

	/// Inverse 4x4 matrix
	JPH_INLINE DMat44			Inversed() const;

	/// Inverse 4x4 matrix when it only contains rotation and translation
	JPH_INLINE DMat44			InversedRotationTranslation() const;

	/// Get rotation part only (note: retains the first 3 values from the bottom row)
	JPH_INLINE Mat44			GetRotation() const										{ return Mat44(mCol[0], mCol[1], mCol[2], Vec4(0, 0, 0, 1)); }

	/// Updates the rotation part of this matrix (the first 3 columns)
	JPH_INLINE void				SetRotation(Mat44Arg inRotation);

	/// Convert to quaternion
	JPH_INLINE Quat				GetQuaternion() const									{ return GetRotation().GetQuaternion(); }

	/// Get matrix that transforms a direction with the same transform as this matrix (length is not preserved)
	JPH_INLINE Mat44			GetDirectionPreservingMatrix() const					{ return GetRotation().Inversed3x3().Transposed3x3(); }

	/// Works identical to Mat44::Decompose
	JPH_INLINE DMat44			Decompose(Vec3 &outScale) const							{ return DMat44(GetRotation().Decompose(outScale), mCol3); }

	/// To String
	friend ostream &			operator << (ostream &inStream, DMat44Arg inM)
	{
		inStream << inM.mCol[0] << ", " << inM.mCol[1] << ", " << inM.mCol[2] << ", " << inM.mCol3;
		return inStream;
	}

private:
	Vec4						mCol[3];												///< Rotation columns
	DVec3						mCol3;													///< Translation column, 4th element is assumed to be 1
};

static_assert(std::is_trivial<DMat44>(), "Is supposed to be a trivial type!");

JPH_NAMESPACE_END

#include "DMat44.inl"

// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Jolt/Physics/Constraints/PathConstraintPath.h>

JPH_NAMESPACE_BEGIN

/// A path that follows a Hermite spline
class JPH_EXPORT PathConstraintPathHermite final : public PathConstraintPath
{
	JPH_DECLARE_SERIALIZABLE_VIRTUAL(JPH_EXPORT, PathConstraintPathHermite)

public:
	// See PathConstraintPath::GetPathMaxFraction
	virtual float		GetPathMaxFraction() const override									{ return float(IsLooping()? mPoints.size() : mPoints.size() - 1); }

	// See PathConstraintPath::GetClosestPoint
	virtual float		GetClosestPoint(Vec3Arg inPosition, float inFractionHint) const override;

	// See PathConstraintPath::GetPointOnPath
	virtual void		GetPointOnPath(float inFraction, Vec3 &outPathPosition, Vec3 &outPathTangent, Vec3 &outPathNormal, Vec3 &outPathBinormal) const override;

	/// Adds a point to the path
	void				AddPoint(Vec3Arg inPosition, Vec3Arg inTangent, Vec3Arg inNormal)	{ mPoints.push_back({ inPosition, inTangent, inNormal}); }

	// See: PathConstraintPath::SaveBinaryState
	virtual void		SaveBinaryState(StreamOut &inStream) const override;

	struct Point
	{
		JPH_DECLARE_SERIALIZABLE_NON_VIRTUAL(JPH_EXPORT, Point)

		Vec3			mPosition;															///< Position on the path
		Vec3			mTangent;															///< Tangent of the path, does not need to be normalized (in the direction of the path)
		Vec3			mNormal;															///< Normal of the path (together with the tangent along the curve this forms a basis for the constraint)
	};

protected:
	// See: PathConstraintPath::RestoreBinaryState
	virtual void		RestoreBinaryState(StreamIn &inStream) override;

private:
	/// Helper function that returns the index of the path segment and the fraction t on the path segment based on the full path fraction
	inline void			GetIndexAndT(float inFraction, int &outIndex, float &outT) const;

	using Points = Array<Point>;

	Points				mPoints;															///< Points on the Hermite spline
};

JPH_NAMESPACE_END

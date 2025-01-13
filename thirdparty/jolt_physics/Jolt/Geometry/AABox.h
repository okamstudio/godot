// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Jolt/Geometry/Triangle.h>
#include <Jolt/Geometry/IndexedTriangle.h>
#include <Jolt/Geometry/Plane.h>
#include <Jolt/Math/Mat44.h>

JPH_NAMESPACE_BEGIN

/// Axis aligned box
class [[nodiscard]] AABox
{
public:
	JPH_OVERRIDE_NEW_DELETE

	/// Constructor
					AABox()												: mMin(Vec3::sReplicate(FLT_MAX)), mMax(Vec3::sReplicate(-FLT_MAX)) { }
					AABox(Vec3Arg inMin, Vec3Arg inMax)					: mMin(inMin), mMax(inMax) { }
					AABox(DVec3Arg inMin, DVec3Arg inMax)				: mMin(inMin.ToVec3RoundDown()), mMax(inMax.ToVec3RoundUp()) { }
					AABox(Vec3Arg inCenter, float inRadius)				: mMin(inCenter - Vec3::sReplicate(inRadius)), mMax(inCenter + Vec3::sReplicate(inRadius)) { }

	/// Create box from 2 points
	static AABox	sFromTwoPoints(Vec3Arg inP1, Vec3Arg inP2)			{ return AABox(Vec3::sMin(inP1, inP2), Vec3::sMax(inP1, inP2)); }

	/// Create box from indexed triangle
	static AABox	sFromTriangle(const VertexList &inVertices, const IndexedTriangle &inTriangle)
	{
		AABox box = sFromTwoPoints(Vec3(inVertices[inTriangle.mIdx[0]]), Vec3(inVertices[inTriangle.mIdx[1]]));
		box.Encapsulate(Vec3(inVertices[inTriangle.mIdx[2]]));
		return box;
	}

	/// Get bounding box of size 2 * FLT_MAX
	static AABox	sBiggest()
	{
		return AABox(Vec3::sReplicate(-FLT_MAX), Vec3::sReplicate(FLT_MAX));
	}

	/// Comparison operators
	bool			operator == (const AABox &inRHS) const				{ return mMin == inRHS.mMin && mMax == inRHS.mMax; }
	bool			operator != (const AABox &inRHS) const				{ return mMin != inRHS.mMin || mMax != inRHS.mMax; }

	/// Reset the bounding box to an empty bounding box
	void			SetEmpty()
	{
		mMin = Vec3::sReplicate(FLT_MAX);
		mMax = Vec3::sReplicate(-FLT_MAX);
	}

	/// Check if the bounding box is valid (max >= min)
	bool			IsValid() const
	{
		return mMin.GetX() <= mMax.GetX() && mMin.GetY() <= mMax.GetY() && mMin.GetZ() <= mMax.GetZ();
	}

	/// Encapsulate point in bounding box
	void			Encapsulate(Vec3Arg inPos)
	{
		mMin = Vec3::sMin(mMin, inPos);
		mMax = Vec3::sMax(mMax, inPos);
	}

	/// Encapsulate bounding box in bounding box
	void			Encapsulate(const AABox &inRHS)
	{
		mMin = Vec3::sMin(mMin, inRHS.mMin);
		mMax = Vec3::sMax(mMax, inRHS.mMax);
	}

	/// Encapsulate triangle in bounding box
	void			Encapsulate(const Triangle &inRHS)
	{
		Vec3 v = Vec3::sLoadFloat3Unsafe(inRHS.mV[0]);
		Encapsulate(v);
		v = Vec3::sLoadFloat3Unsafe(inRHS.mV[1]);
		Encapsulate(v);
		v = Vec3::sLoadFloat3Unsafe(inRHS.mV[2]);
		Encapsulate(v);
	}

	/// Encapsulate triangle in bounding box
	void			Encapsulate(const VertexList &inVertices, const IndexedTriangle &inTriangle)
	{
		for (uint32 idx : inTriangle.mIdx)
			Encapsulate(Vec3(inVertices[idx]));
	}

	/// Intersect this bounding box with inOther, returns the intersection
	AABox			Intersect(const AABox &inOther) const
	{
		return AABox(Vec3::sMax(mMin, inOther.mMin), Vec3::sMin(mMax, inOther.mMax));
	}

	/// Make sure that each edge of the bounding box has a minimal length
	void			EnsureMinimalEdgeLength(float inMinEdgeLength)
	{
		Vec3 min_length = Vec3::sReplicate(inMinEdgeLength);
		mMax = Vec3::sSelect(mMax, mMin + min_length, Vec3::sLess(mMax - mMin, min_length));
	}

	/// Widen the box on both sides by inVector
	void			ExpandBy(Vec3Arg inVector)
	{
		mMin -= inVector;
		mMax += inVector;
	}

	/// Get center of bounding box
	Vec3			GetCenter() const
	{
		return 0.5f * (mMin + mMax);
	}

	/// Get extent of bounding box (half of the size)
	Vec3			GetExtent() const
	{
		return 0.5f * (mMax - mMin);
	}

	/// Get size of bounding box
	Vec3			GetSize() const
	{
		return mMax - mMin;
	}

	/// Get surface area of bounding box
	float			GetSurfaceArea() const
	{
		Vec3 extent = mMax - mMin;
		return 2.0f * (extent.GetX() * extent.GetY() + extent.GetX() * extent.GetZ() + extent.GetY() * extent.GetZ());
	}

	/// Get volume of bounding box
	float			GetVolume() const
	{
		Vec3 extent = mMax - mMin;
		return extent.GetX() * extent.GetY() * extent.GetZ();
	}

	/// Check if this box contains another box
	bool			Contains(const AABox &inOther) const
	{
		return UVec4::sAnd(Vec3::sLessOrEqual(mMin, inOther.mMin), Vec3::sGreaterOrEqual(mMax, inOther.mMax)).TestAllXYZTrue();
	}

	/// Check if this box contains a point
	bool			Contains(Vec3Arg inOther) const
	{
		return UVec4::sAnd(Vec3::sLessOrEqual(mMin, inOther), Vec3::sGreaterOrEqual(mMax, inOther)).TestAllXYZTrue();
	}

	/// Check if this box contains a point
	bool			Contains(DVec3Arg inOther) const
	{
		return Contains(Vec3(inOther));
	}

	/// Check if this box overlaps with another box
	bool			Overlaps(const AABox &inOther) const
	{
		return !UVec4::sOr(Vec3::sGreater(mMin, inOther.mMax), Vec3::sLess(mMax, inOther.mMin)).TestAnyXYZTrue();
	}

	/// Check if this box overlaps with a plane
	bool			Overlaps(const Plane &inPlane) const
	{
		Vec3 normal = inPlane.GetNormal();
		float dist_normal = inPlane.SignedDistance(GetSupport(normal));
		float dist_min_normal = inPlane.SignedDistance(GetSupport(-normal));
		return dist_normal * dist_min_normal <= 0.0f; // If both support points are on the same side of the plane we don't overlap
	}

	/// Translate bounding box
	void			Translate(Vec3Arg inTranslation)
	{
		mMin += inTranslation;
		mMax += inTranslation;
	}

	/// Translate bounding box
	void			Translate(DVec3Arg inTranslation)
	{
		mMin = (DVec3(mMin) + inTranslation).ToVec3RoundDown();
		mMax = (DVec3(mMax) + inTranslation).ToVec3RoundUp();
	}

	/// Transform bounding box
	AABox			Transformed(Mat44Arg inMatrix) const
	{
		// Start with the translation of the matrix
		Vec3 new_min, new_max;
		new_min = new_max = inMatrix.GetTranslation();

		// Now find the extreme points by considering the product of the min and max with each column of inMatrix
		for (int c = 0; c < 3; ++c)
		{
			Vec3 col = inMatrix.GetColumn3(c);

			Vec3 a = col * mMin[c];
			Vec3 b = col * mMax[c];

			new_min += Vec3::sMin(a, b);
			new_max += Vec3::sMax(a, b);
		}

		// Return the new bounding box
		return AABox(new_min, new_max);
	}

	/// Transform bounding box
	AABox			Transformed(DMat44Arg inMatrix) const
	{
		AABox transformed = Transformed(inMatrix.GetRotation());
		transformed.Translate(inMatrix.GetTranslation());
		return transformed;
	}

	/// Scale this bounding box, can handle non-uniform and negative scaling
	AABox			Scaled(Vec3Arg inScale) const
	{
		return AABox::sFromTwoPoints(mMin * inScale, mMax * inScale);
	}

	/// Calculate the support vector for this convex shape.
	Vec3			GetSupport(Vec3Arg inDirection) const
	{
		return Vec3::sSelect(mMax, mMin, Vec3::sLess(inDirection, Vec3::sZero()));
	}

	/// Get the vertices of the face that faces inDirection the most
	template <class VERTEX_ARRAY>
	void			GetSupportingFace(Vec3Arg inDirection, VERTEX_ARRAY &outVertices) const
	{
		outVertices.resize(4);

		int axis = inDirection.Abs().GetHighestComponentIndex();
		if (inDirection[axis] < 0.0f)
		{
			switch (axis)
			{
			case 0:
				outVertices[0] = Vec3(mMax.GetX(), mMin.GetY(), mMin.GetZ());
				outVertices[1] = Vec3(mMax.GetX(), mMax.GetY(), mMin.GetZ());
				outVertices[2] = Vec3(mMax.GetX(), mMax.GetY(), mMax.GetZ());
				outVertices[3] = Vec3(mMax.GetX(), mMin.GetY(), mMax.GetZ());
				break;

			case 1:
				outVertices[0] = Vec3(mMin.GetX(), mMax.GetY(), mMin.GetZ());
				outVertices[1] = Vec3(mMin.GetX(), mMax.GetY(), mMax.GetZ());
				outVertices[2] = Vec3(mMax.GetX(), mMax.GetY(), mMax.GetZ());
				outVertices[3] = Vec3(mMax.GetX(), mMax.GetY(), mMin.GetZ());
				break;

			case 2:
				outVertices[0] = Vec3(mMin.GetX(), mMin.GetY(), mMax.GetZ());
				outVertices[1] = Vec3(mMax.GetX(), mMin.GetY(), mMax.GetZ());
				outVertices[2] = Vec3(mMax.GetX(), mMax.GetY(), mMax.GetZ());
				outVertices[3] = Vec3(mMin.GetX(), mMax.GetY(), mMax.GetZ());
				break;
			}
		}
		else
		{
			switch (axis)
			{
			case 0:
				outVertices[0] = Vec3(mMin.GetX(), mMin.GetY(), mMin.GetZ());
				outVertices[1] = Vec3(mMin.GetX(), mMin.GetY(), mMax.GetZ());
				outVertices[2] = Vec3(mMin.GetX(), mMax.GetY(), mMax.GetZ());
				outVertices[3] = Vec3(mMin.GetX(), mMax.GetY(), mMin.GetZ());
				break;

			case 1:
				outVertices[0] = Vec3(mMin.GetX(), mMin.GetY(), mMin.GetZ());
				outVertices[1] = Vec3(mMax.GetX(), mMin.GetY(), mMin.GetZ());
				outVertices[2] = Vec3(mMax.GetX(), mMin.GetY(), mMax.GetZ());
				outVertices[3] = Vec3(mMin.GetX(), mMin.GetY(), mMax.GetZ());
				break;

			case 2:
				outVertices[0] = Vec3(mMin.GetX(), mMin.GetY(), mMin.GetZ());
				outVertices[1] = Vec3(mMin.GetX(), mMax.GetY(), mMin.GetZ());
				outVertices[2] = Vec3(mMax.GetX(), mMax.GetY(), mMin.GetZ());
				outVertices[3] = Vec3(mMax.GetX(), mMin.GetY(), mMin.GetZ());
				break;
			}
		}
	}

	/// Get the closest point on or in this box to inPoint
	Vec3			GetClosestPoint(Vec3Arg inPoint) const
	{
		return Vec3::sMin(Vec3::sMax(inPoint, mMin), mMax);
	}

	/// Get the squared distance between inPoint and this box (will be 0 if in Point is inside the box)
	inline float	GetSqDistanceTo(Vec3Arg inPoint) const
	{
		return (GetClosestPoint(inPoint) - inPoint).LengthSq();
	}

	/// Bounding box min and max
	Vec3			mMin;
	Vec3			mMax;
};

JPH_NAMESPACE_END

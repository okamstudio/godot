// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Jolt/Math/Mat44.h>

JPH_NAMESPACE_BEGIN

/// Helper functions to get the support point for a convex object
/// Structure that transforms a convex object (supports only uniform scaling)
template <typename ConvexObject>
struct TransformedConvexObject
{
	/// Create transformed convex object.
	TransformedConvexObject(Mat44Arg inTransform, const ConvexObject &inObject) :
		mTransform(inTransform),
		mObject(inObject)
	{
	}

	/// Calculate the support vector for this convex shape.
	Vec3					GetSupport(Vec3Arg inDirection) const
	{
		return mTransform * mObject.GetSupport(mTransform.Multiply3x3Transposed(inDirection));
	}

	/// Get the vertices of the face that faces inDirection the most
	template <class VERTEX_ARRAY>
	void					GetSupportingFace(Vec3Arg inDirection, VERTEX_ARRAY &outVertices) const
	{
		mObject.GetSupportingFace(mTransform.Multiply3x3Transposed(inDirection), outVertices);

		for (Vec3 &v : outVertices)
			v = mTransform * v;
	}

	Mat44					mTransform;
	const ConvexObject &	mObject;
};

/// Structure that adds a convex radius
template <typename ConvexObject>
struct AddConvexRadius
{
	AddConvexRadius(const ConvexObject &inObject, float inRadius) :
		mObject(inObject),
		mRadius(inRadius)
	{
	}

	/// Calculate the support vector for this convex shape.
	Vec3					GetSupport(Vec3Arg inDirection) const
	{
		float length = inDirection.Length();
		return length > 0.0f ? mObject.GetSupport(inDirection) + (mRadius / length) * inDirection : mObject.GetSupport(inDirection);
	}

	const ConvexObject &	mObject;
	float					mRadius;
};

/// Structure that performs a Minkowski difference A - B
template <typename ConvexObjectA, typename ConvexObjectB>
struct MinkowskiDifference
{
	MinkowskiDifference(const ConvexObjectA &inObjectA, const ConvexObjectB &inObjectB) :
		mObjectA(inObjectA),
		mObjectB(inObjectB)
	{
	}

	/// Calculate the support vector for this convex shape.
	Vec3					GetSupport(Vec3Arg inDirection) const
	{
		return mObjectA.GetSupport(inDirection) - mObjectB.GetSupport(-inDirection);
	}

	const ConvexObjectA &	mObjectA;
	const ConvexObjectB &	mObjectB;
};

/// Class that wraps a point so that it can be used with convex collision detection
struct PointConvexSupport
{
	/// Calculate the support vector for this convex shape.
	Vec3					GetSupport([[maybe_unused]] Vec3Arg inDirection) const
	{
		return mPoint;
	}

	Vec3					mPoint;
};

/// Class that wraps a triangle so that it can used with convex collision detection
struct TriangleConvexSupport
{
	/// Constructor
							TriangleConvexSupport(Vec3Arg inV1, Vec3Arg inV2, Vec3Arg inV3) :
		mV1(inV1),
		mV2(inV2),
		mV3(inV3)
	{
	}

	/// Calculate the support vector for this convex shape.
	Vec3					GetSupport(Vec3Arg inDirection) const
	{
		// Project vertices on inDirection
		float d1 = mV1.Dot(inDirection);
		float d2 = mV2.Dot(inDirection);
		float d3 = mV3.Dot(inDirection);

		// Return vertex with biggest projection
		if (d1 > d2)
		{
			if (d1 > d3)
				return mV1;
			else
				return mV3;
		}
		else
		{
			if (d2 > d3)
				return mV2;
			else
				return mV3;
		}
	}

	/// Get the vertices of the face that faces inDirection the most
	template <class VERTEX_ARRAY>
	void					GetSupportingFace([[maybe_unused]] Vec3Arg inDirection, VERTEX_ARRAY &outVertices) const
	{
		outVertices.push_back(mV1);
		outVertices.push_back(mV2);
		outVertices.push_back(mV3);
	}

	/// The three vertices of the triangle
	Vec3					mV1;
	Vec3					mV2;
	Vec3					mV3;
};

/// Class that wraps a polygon so that it can used with convex collision detection
template <class VERTEX_ARRAY>
struct PolygonConvexSupport
{
	/// Constructor
	explicit				PolygonConvexSupport(const VERTEX_ARRAY &inVertices) :
		mVertices(inVertices)
	{
	}

	/// Calculate the support vector for this convex shape.
	Vec3					GetSupport(Vec3Arg inDirection) const
	{
		Vec3 support_point = mVertices[0];
		float best_dot = mVertices[0].Dot(inDirection);

		for (typename VERTEX_ARRAY::const_iterator v = mVertices.begin() + 1; v < mVertices.end(); ++v)
		{
			float dot = v->Dot(inDirection);
			if (dot > best_dot)
			{
				best_dot = dot;
				support_point = *v;
			}
		}

		return support_point;
	}

	/// Get the vertices of the face that faces inDirection the most
	template <class VERTEX_ARRAY_ARG>
	void					GetSupportingFace([[maybe_unused]] Vec3Arg inDirection, VERTEX_ARRAY_ARG &outVertices) const
	{
		for (Vec3 v : mVertices)
			outVertices.push_back(v);
	}

	/// The vertices of the polygon
	const VERTEX_ARRAY &	mVertices;
};

JPH_NAMESPACE_END

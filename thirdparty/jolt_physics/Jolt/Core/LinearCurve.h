// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Jolt/ObjectStream/SerializableObject.h>
#include <Jolt/Core/QuickSort.h>

JPH_NAMESPACE_BEGIN

class StreamOut;
class StreamIn;

// A set of points (x, y) that form a linear curve
class JPH_EXPORT LinearCurve
{
	JPH_DECLARE_SERIALIZABLE_NON_VIRTUAL(JPH_EXPORT, LinearCurve)

public:
	/// A point on the curve
	class Point
	{
		JPH_DECLARE_SERIALIZABLE_NON_VIRTUAL(JPH_EXPORT, Point)

	public:
		float			mX = 0.0f;
		float			mY = 0.0f;
	};

	/// Remove all points
	void				Clear()											{ mPoints.clear(); }

	/// Reserve memory for inNumPoints points
	void				Reserve(uint inNumPoints)						{ mPoints.reserve(inNumPoints); }

	/// Add a point to the curve. Points must be inserted in ascending X or Sort() needs to be called when all points have been added.
	/// @param inX X value
	/// @param inY Y value
	void				AddPoint(float inX, float inY)					{ mPoints.push_back({ inX, inY }); }

	/// Sort the points on X ascending
	void				Sort()											{ QuickSort(mPoints.begin(), mPoints.end(), [](const Point &inLHS, const Point &inRHS) { return inLHS.mX < inRHS.mX; }); }

	/// Get the lowest X value
	float				GetMinX() const									{ return mPoints.empty()? 0.0f : mPoints.front().mX; }

	/// Get the highest X value
	float				GetMaxX() const									{ return mPoints.empty()? 0.0f : mPoints.back().mX; }

	/// Sample value on the curve
	/// @param inX X value to sample at
	/// @return Interpolated Y value
	float				GetValue(float inX) const;

	/// Saves the state of this object in binary form to inStream.
	void				SaveBinaryState(StreamOut &inStream) const;

	/// Restore the state of this object from inStream.
	void				RestoreBinaryState(StreamIn &inStream);

	/// The points on the curve, should be sorted ascending by x
	using Points = Array<Point>;
	Points				mPoints;
};

JPH_NAMESPACE_END

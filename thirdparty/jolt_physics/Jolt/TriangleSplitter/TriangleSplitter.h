// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Jolt/Geometry/IndexedTriangle.h>
#include <Jolt/Core/NonCopyable.h>

JPH_NAMESPACE_BEGIN

/// A class that splits a triangle list into two parts for building a tree
class JPH_EXPORT TriangleSplitter : public NonCopyable
{
public:
	/// Constructor
								TriangleSplitter(const VertexList &inVertices, const IndexedTriangleList &inTriangles);

	/// Virtual destructor
	virtual						~TriangleSplitter() = default;

	struct Stats
	{
		const char *			mSplitterName = nullptr;
		int						mLeafSize = 0;
	};

	/// Get stats of splitter
	virtual void				GetStats(Stats &outStats) const = 0;

	/// Helper struct to indicate triangle range before and after the split
	struct Range
	{
		/// Constructor
								Range() = default;
								Range(uint inBegin, uint inEnd) : mBegin(inBegin), mEnd(inEnd) { }

		/// Get number of triangles in range
		uint					Count() const
		{
			return mEnd - mBegin;
		}

		/// Start and end index (end = 1 beyond end)
		uint					mBegin;
		uint					mEnd;
	};

	/// Range of triangles to start with
	Range						GetInitialRange() const
	{
		return Range(0, (uint)mSortedTriangleIdx.size());
	}

	/// Split triangles into two groups left and right, returns false if no split could be made
	/// @param inTriangles The range of triangles (in mSortedTriangleIdx) to process
	/// @param outLeft On return this will contain the ranges for the left subpart. mSortedTriangleIdx may have been shuffled.
	/// @param outRight On return this will contain the ranges for the right subpart. mSortedTriangleIdx may have been shuffled.
	/// @return Returns true when a split was found
	virtual bool				Split(const Range &inTriangles, Range &outLeft, Range &outRight) = 0;

	/// Get the list of vertices
	const VertexList &			GetVertices() const
	{
		return mVertices;
	}

	/// Get triangle by index
	const IndexedTriangle &		GetTriangle(uint inIdx) const
	{
		return mTriangles[mSortedTriangleIdx[inIdx]];
	}

protected:
	/// Helper function to split triangles based on dimension and split value
	bool						SplitInternal(const Range &inTriangles, uint inDimension, float inSplit, Range &outLeft, Range &outRight);

	const VertexList &			mVertices;				///< Vertices of the indexed triangles
	const IndexedTriangleList &	mTriangles;				///< Unsorted triangles
	Array<Float3>				mCentroids;				///< Unsorted centroids of triangles
	Array<uint>					mSortedTriangleIdx;		///< Indices to sort triangles
};

JPH_NAMESPACE_END

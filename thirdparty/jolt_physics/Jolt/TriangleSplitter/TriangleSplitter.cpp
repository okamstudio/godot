// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#include <Jolt/Jolt.h>

#include <Jolt/TriangleSplitter/TriangleSplitter.h>

JPH_NAMESPACE_BEGIN

TriangleSplitter::TriangleSplitter(const VertexList &inVertices, const IndexedTriangleList &inTriangles) :
	mVertices(inVertices),
	mTriangles(inTriangles)
{
	mSortedTriangleIdx.resize(inTriangles.size());
	mCentroids.resize(inTriangles.size());

	for (uint t = 0; t < inTriangles.size(); ++t)
	{
		// Initially triangles start unsorted
		mSortedTriangleIdx[t] = t;

		// Calculate centroid
		inTriangles[t].GetCentroid(inVertices).StoreFloat3(&mCentroids[t]);
	}
}

bool TriangleSplitter::SplitInternal(const Range &inTriangles, uint inDimension, float inSplit, Range &outLeft, Range &outRight)
{
	// Divide triangles
	uint start = inTriangles.mBegin, end = inTriangles.mEnd;
	while (start < end)
	{
		// Search for first element that is on the right hand side of the split plane
		while (start < end && mCentroids[mSortedTriangleIdx[start]][inDimension] < inSplit)
			++start;

		// Search for the first element that is on the left hand side of the split plane
		while (start < end && mCentroids[mSortedTriangleIdx[end - 1]][inDimension] >= inSplit)
			--end;

		if (start < end)
		{
			// Swap the two elements
			std::swap(mSortedTriangleIdx[start], mSortedTriangleIdx[end - 1]);
			++start;
			--end;
		}
	}
	JPH_ASSERT(start == end);

#ifdef JPH_ENABLE_ASSERTS
	// Validate division algorithm
	JPH_ASSERT(inTriangles.mBegin <= start);
	JPH_ASSERT(start <= inTriangles.mEnd);
	for (uint i = inTriangles.mBegin; i < start; ++i)
		JPH_ASSERT(mCentroids[mSortedTriangleIdx[i]][inDimension] < inSplit);
	for (uint i = start; i < inTriangles.mEnd; ++i)
		JPH_ASSERT(mCentroids[mSortedTriangleIdx[i]][inDimension] >= inSplit);
#endif

	outLeft = Range(inTriangles.mBegin, start);
	outRight = Range(start, inTriangles.mEnd);
	return outLeft.Count() > 0 && outRight.Count() > 0;
}

JPH_NAMESPACE_END

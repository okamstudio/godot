// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#include <Jolt/Jolt.h>

#include <Jolt/TriangleSplitter/TriangleSplitterFixedLeafSize.h>
#include <Jolt/TriangleGrouper/TriangleGrouperClosestCentroid.h>

JPH_NAMESPACE_BEGIN

TriangleSplitterFixedLeafSize::TriangleSplitterFixedLeafSize(const VertexList &inVertices, const IndexedTriangleList &inTriangles, uint inLeafSize, uint inMinNumBins, uint inMaxNumBins, uint inNumTrianglesPerBin) :
	TriangleSplitter(inVertices, inTriangles),
	mLeafSize(inLeafSize),
	mMinNumBins(inMinNumBins),
	mMaxNumBins(inMaxNumBins),
	mNumTrianglesPerBin(inNumTrianglesPerBin)
{
	// Group the triangles
	TriangleGrouperClosestCentroid grouper;
	grouper.Group(inVertices, inTriangles, mLeafSize, mSortedTriangleIdx);

	// Pad triangles so that we have a multiple of mLeafSize
	const uint num_triangles = (uint)inTriangles.size();
	const uint num_groups = (num_triangles + mLeafSize - 1) / mLeafSize;
	const uint last_triangle_idx = mSortedTriangleIdx.back();
	for (uint t = num_triangles, t_end = num_groups * mLeafSize; t < t_end; ++t)
		mSortedTriangleIdx.push_back(last_triangle_idx);
}

Vec3 TriangleSplitterFixedLeafSize::GetCentroidForGroup(uint inFirstTriangleInGroup)
{
	JPH_ASSERT(inFirstTriangleInGroup % mLeafSize == 0);
	AABox box;
	for (uint g = 0; g < mLeafSize; ++g)
		box.Encapsulate(mVertices, GetTriangle(inFirstTriangleInGroup + g));
	return box.GetCenter();
}

bool TriangleSplitterFixedLeafSize::Split(const Range &inTriangles, Range &outLeft, Range &outRight)
{
	// Cannot split anything smaller than leaf size
	JPH_ASSERT(inTriangles.Count() > mLeafSize);
	JPH_ASSERT(inTriangles.Count() % mLeafSize == 0);

	// Calculate bounds for this range
	AABox centroid_bounds;
	for (uint t = inTriangles.mBegin; t < inTriangles.mEnd; t += mLeafSize)
		centroid_bounds.Encapsulate(GetCentroidForGroup(t));

	float best_cp = FLT_MAX;
	uint best_dim = 0xffffffff;
	float best_split = 0;

	// Bin in all dimensions
	uint num_bins = Clamp(inTriangles.Count() / mNumTrianglesPerBin, mMinNumBins, mMaxNumBins);
	Array<Bin> bins(num_bins);
	for (uint dim = 0; dim < 3; ++dim)
	{
		float bounds_min = centroid_bounds.mMin[dim];
		float bounds_size = centroid_bounds.mMax[dim] - bounds_min;

		// Skip axis if too small
		if (bounds_size < 1.0e-5f)
			continue;

		// Initialize bins
		for (uint b = 0; b < num_bins; ++b)
		{
			Bin &bin = bins[b];
			bin.mBounds.SetEmpty();
			bin.mMinCentroid = bounds_min + bounds_size * (b + 1) / num_bins;
			bin.mNumTriangles = 0;
		}

		// Bin all triangles
		for (uint t = inTriangles.mBegin; t < inTriangles.mEnd; t += mLeafSize)
		{
			// Calculate average centroid for group
			float centroid_pos = GetCentroidForGroup(t)[dim];

			// Select bin
			uint bin_no = min(uint((centroid_pos - bounds_min) / bounds_size * num_bins), num_bins - 1);
			Bin &bin = bins[bin_no];

			// Put all triangles of group in same bin
			for (uint g = 0; g < mLeafSize; ++g)
				bin.mBounds.Encapsulate(mVertices, GetTriangle(t + g));
			bin.mMinCentroid = min(bin.mMinCentroid, centroid_pos);
			bin.mNumTriangles += mLeafSize;
		}

		// Calculate totals left to right
		AABox prev_bounds;
		int prev_triangles = 0;
		for (uint b = 0; b < num_bins; ++b)
		{
			Bin &bin = bins[b];
			bin.mBoundsAccumulatedLeft = prev_bounds; // Don't include this node as we'll take a split on the left side of the bin
			bin.mNumTrianglesAccumulatedLeft = prev_triangles;
			prev_bounds.Encapsulate(bin.mBounds);
			prev_triangles += bin.mNumTriangles;
		}

		// Calculate totals right to left
		prev_bounds.SetEmpty();
		prev_triangles = 0;
		for (int b = num_bins - 1; b >= 0; --b)
		{
			Bin &bin = bins[b];
			prev_bounds.Encapsulate(bin.mBounds);
			prev_triangles += bin.mNumTriangles;
			bin.mBoundsAccumulatedRight = prev_bounds;
			bin.mNumTrianglesAccumulatedRight = prev_triangles;
		}

		// Get best splitting plane
		for (uint b = 1; b < num_bins; ++b) // Start at 1 since selecting bin 0 would result in everything ending up on the right side
		{
			// Calculate surface area heuristic and see if it is better than the current best
			const Bin &bin = bins[b];
			float cp = bin.mBoundsAccumulatedLeft.GetSurfaceArea() * bin.mNumTrianglesAccumulatedLeft + bin.mBoundsAccumulatedRight.GetSurfaceArea() * bin.mNumTrianglesAccumulatedRight;
			if (cp < best_cp)
			{
				best_cp = cp;
				best_dim = dim;
				best_split = bin.mMinCentroid;
			}
		}
	}

	// No split found?
	if (best_dim == 0xffffffff)
		return false;

	// Divide triangles
	uint start = inTriangles.mBegin, end = inTriangles.mEnd;
	while (start < end)
	{
		// Search for first element that is on the right hand side of the split plane
		while (start < end && GetCentroidForGroup(start)[best_dim] < best_split)
			start += mLeafSize;

		// Search for the first element that is on the left hand side of the split plane
		while (start < end && GetCentroidForGroup(end - mLeafSize)[best_dim] >= best_split)
			end -= mLeafSize;

		if (start < end)
		{
			// Swap the two elements
			for (uint g = 0; g < mLeafSize; ++g)
				std::swap(mSortedTriangleIdx[start + g], mSortedTriangleIdx[end - mLeafSize + g]);
			start += mLeafSize;
			end -= mLeafSize;
		}
	}
	JPH_ASSERT(start == end);

	// No suitable split found, doing random split in half
	if (start == inTriangles.mBegin || start == inTriangles.mEnd)
		start = inTriangles.mBegin + (inTriangles.Count() / mLeafSize + 1) / 2 * mLeafSize;

	outLeft = Range(inTriangles.mBegin, start);
	outRight = Range(start, inTriangles.mEnd);
	JPH_ASSERT(outLeft.mEnd > outLeft.mBegin && outRight.mEnd > outRight.mBegin);
	JPH_ASSERT(outLeft.Count() % mLeafSize == 0 && outRight.Count() % mLeafSize == 0);
	return true;
}

JPH_NAMESPACE_END

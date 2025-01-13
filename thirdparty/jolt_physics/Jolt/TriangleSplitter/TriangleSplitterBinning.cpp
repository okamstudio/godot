// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#include <Jolt/Jolt.h>

#include <Jolt/TriangleSplitter/TriangleSplitterBinning.h>

 JPH_NAMESPACE_BEGIN

TriangleSplitterBinning::TriangleSplitterBinning(const VertexList &inVertices, const IndexedTriangleList &inTriangles, uint inMinNumBins, uint inMaxNumBins, uint inNumTrianglesPerBin) :
	TriangleSplitter(inVertices, inTriangles),
	mMinNumBins(inMinNumBins),
	mMaxNumBins(inMaxNumBins),
	mNumTrianglesPerBin(inNumTrianglesPerBin)
{
	mBins.resize(mMaxNumBins * 3); // mMaxNumBins per dimension
}

bool TriangleSplitterBinning::Split(const Range &inTriangles, Range &outLeft, Range &outRight)
{
	// Calculate bounds for this range
	AABox centroid_bounds;
	for (uint t = inTriangles.mBegin; t < inTriangles.mEnd; ++t)
		centroid_bounds.Encapsulate(Vec3(mCentroids[mSortedTriangleIdx[t]]));

	// Convert bounds to min coordinate and size
	// Prevent division by zero if one of the dimensions is zero
	constexpr float cMinSize = 1.0e-5f;
	Vec3 bounds_min = centroid_bounds.mMin;
	Vec3 bounds_size = Vec3::sMax(centroid_bounds.mMax - bounds_min, Vec3::sReplicate(cMinSize));

	float best_cp = FLT_MAX;
	uint best_dim = 0xffffffff;
	float best_split = 0;

	// Bin in all dimensions
	uint num_bins = Clamp(inTriangles.Count() / mNumTrianglesPerBin, mMinNumBins, mMaxNumBins);

	// Initialize bins
	for (uint dim = 0; dim < 3; ++dim)
	{
		// Get bounding box size for this dimension
		float bounds_min_dim = bounds_min[dim];
		float bounds_size_dim = bounds_size[dim];

		// Get the bins for this dimension
		Bin *bins_dim = &mBins[num_bins * dim];

		for (uint b = 0; b < num_bins; ++b)
		{
			Bin &bin = bins_dim[b];
			bin.mBounds.SetEmpty();
			bin.mMinCentroid = bounds_min_dim + bounds_size_dim * (b + 1) / num_bins;
			bin.mNumTriangles = 0;
		}
	}

	// Bin all triangles in all dimensions at once
	for (uint t = inTriangles.mBegin; t < inTriangles.mEnd; ++t)
	{
		Vec3 centroid_pos(mCentroids[mSortedTriangleIdx[t]]);

		AABox triangle_bounds = AABox::sFromTriangle(mVertices, GetTriangle(t));

		Vec3 bin_no_f = (centroid_pos - bounds_min) / bounds_size * float(num_bins);
		UVec4 bin_no = UVec4::sMin(bin_no_f.ToInt(), UVec4::sReplicate(num_bins - 1));

		for (uint dim = 0; dim < 3; ++dim)
		{
			// Select bin
			Bin &bin = mBins[num_bins * dim + bin_no[dim]];

			// Accumulate triangle in bin
			bin.mBounds.Encapsulate(triangle_bounds);
			bin.mMinCentroid = min(bin.mMinCentroid, centroid_pos[dim]);
			bin.mNumTriangles++;
		}
	}

	for (uint dim = 0; dim < 3; ++dim)
	{
		// Skip axis if too small
		if (bounds_size[dim] <= cMinSize)
			continue;

		// Get the bins for this dimension
		Bin *bins_dim = &mBins[num_bins * dim];

		// Calculate totals left to right
		AABox prev_bounds;
		int prev_triangles = 0;
		for (uint b = 0; b < num_bins; ++b)
		{
			Bin &bin = bins_dim[b];
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
			Bin &bin = bins_dim[b];
			prev_bounds.Encapsulate(bin.mBounds);
			prev_triangles += bin.mNumTriangles;
			bin.mBoundsAccumulatedRight = prev_bounds;
			bin.mNumTrianglesAccumulatedRight = prev_triangles;
		}

		// Get best splitting plane
		for (uint b = 1; b < num_bins; ++b) // Start at 1 since selecting bin 0 would result in everything ending up on the right side
		{
			// Calculate surface area heuristic and see if it is better than the current best
			const Bin &bin = bins_dim[b];
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

	return SplitInternal(inTriangles, best_dim, best_split, outLeft, outRight);
}

JPH_NAMESPACE_END

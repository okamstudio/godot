// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Jolt/TriangleSplitter/TriangleSplitter.h>
#include <Jolt/Geometry/AABox.h>

JPH_NAMESPACE_BEGIN

/// Same as TriangleSplitterBinning, but ensuring that leaves have a fixed amount of triangles
/// The resulting tree should be suitable for processing on GPU where we want all threads to process an equal amount of triangles
class JPH_EXPORT TriangleSplitterFixedLeafSize : public TriangleSplitter
{
public:
	/// Constructor
							TriangleSplitterFixedLeafSize(const VertexList &inVertices, const IndexedTriangleList &inTriangles, uint inLeafSize, uint inMinNumBins = 8, uint inMaxNumBins = 128, uint inNumTrianglesPerBin = 6);

	// See TriangleSplitter::GetStats
	virtual void			GetStats(Stats &outStats) const override
	{
		outStats.mSplitterName = "TriangleSplitterFixedLeafSize";
		outStats.mLeafSize = mLeafSize;
	}

	// See TriangleSplitter::Split
	virtual bool			Split(const Range &inTriangles, Range &outLeft, Range &outRight) override;

private:
	/// Get centroid for group
	Vec3					GetCentroidForGroup(uint inFirstTriangleInGroup);

	// Configuration
	const uint				mLeafSize;
	const uint				mMinNumBins;
	const uint				mMaxNumBins;
	const uint				mNumTrianglesPerBin;

	struct Bin
	{
		// Properties of this bin
		AABox				mBounds;
		float				mMinCentroid;
		uint				mNumTriangles;

		// Accumulated data from left most / right most bin to current (including this bin)
		AABox				mBoundsAccumulatedLeft;
		AABox				mBoundsAccumulatedRight;
		uint				mNumTrianglesAccumulatedLeft;
		uint				mNumTrianglesAccumulatedRight;
	};
};

JPH_NAMESPACE_END

// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Jolt/TriangleGrouper/TriangleGrouper.h>

JPH_NAMESPACE_BEGIN

/// A class that groups triangles in batches of N.
/// Starts with centroid with lowest X coordinate and finds N closest centroids, this repeats until all groups have been found.
/// Time complexity: O(N^2)
class JPH_EXPORT TriangleGrouperClosestCentroid : public TriangleGrouper
{
public:
	// See: TriangleGrouper::Group
	virtual void			Group(const VertexList &inVertices, const IndexedTriangleList &inTriangles, int inGroupSize, Array<uint> &outGroupedTriangleIndices) override;
};

JPH_NAMESPACE_END

// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Jolt/Geometry/IndexedTriangle.h>
#include <Jolt/Core/NonCopyable.h>

JPH_NAMESPACE_BEGIN

/// A class that groups triangles in batches of N (according to closeness)
class JPH_EXPORT TriangleGrouper : public NonCopyable
{
public:
	/// Virtual destructor
	virtual					~TriangleGrouper() = default;

	/// Group a batch of indexed triangles
	/// @param inVertices The list of vertices
	/// @param inTriangles The list of indexed triangles (indexes into inVertices)
	/// @param inGroupSize How big each group should be
	/// @param outGroupedTriangleIndices An ordered list of indices (indexing into inTriangles), contains groups of inGroupSize large worth of indices to triangles that are grouped together. If the triangle count is not an exact multiple of inGroupSize the last batch will be smaller.
	virtual void			Group(const VertexList &inVertices, const IndexedTriangleList &inTriangles, int inGroupSize, Array<uint> &outGroupedTriangleIndices) = 0;
};

JPH_NAMESPACE_END

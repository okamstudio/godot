// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#include <Jolt/Jolt.h>

#include <Jolt/TriangleSplitter/TriangleSplitterLongestAxis.h>
#include <Jolt/Geometry/AABox.h>

JPH_NAMESPACE_BEGIN

TriangleSplitterLongestAxis::TriangleSplitterLongestAxis(const VertexList &inVertices, const IndexedTriangleList &inTriangles) :
	TriangleSplitter(inVertices, inTriangles)
{
}

bool TriangleSplitterLongestAxis::Split(const Range &inTriangles, Range &outLeft, Range &outRight)
{
	// Calculate bounding box for triangles
	AABox bounds;
	for (uint t = inTriangles.mBegin; t < inTriangles.mEnd; ++t)
		bounds.Encapsulate(mVertices, GetTriangle(t));

	// Calculate split plane
	uint dimension = bounds.GetExtent().GetHighestComponentIndex();
	float split = bounds.GetCenter()[dimension];

	return SplitInternal(inTriangles, dimension, split, outLeft, outRight);
}

JPH_NAMESPACE_END

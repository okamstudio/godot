// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Jolt/TriangleSplitter/TriangleSplitter.h>

JPH_NAMESPACE_BEGIN

/// Splitter using center of bounding box with longest axis
class JPH_EXPORT TriangleSplitterLongestAxis : public TriangleSplitter
{
public:
	/// Constructor
							TriangleSplitterLongestAxis(const VertexList &inVertices, const IndexedTriangleList &inTriangles);

	// See TriangleSplitter::GetStats
	virtual void			GetStats(Stats &outStats) const override
	{
		outStats.mSplitterName = "TriangleSplitterLongestAxis";
	}

	// See TriangleSplitter::Split
	virtual bool			Split(const Range &inTriangles, Range &outLeft, Range &outRight) override;
};

JPH_NAMESPACE_END

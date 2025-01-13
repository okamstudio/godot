// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#include <Jolt/Jolt.h>

#ifdef JPH_DEBUG_RENDERER

#include <Jolt/Renderer/DebugRendererRecorder.h>

JPH_NAMESPACE_BEGIN

void DebugRendererRecorder::DrawLine(RVec3Arg inFrom, RVec3Arg inTo, ColorArg inColor)
{
	lock_guard lock(mMutex);

	mCurrentFrame.mLines.push_back({ inFrom, inTo, inColor });
}

void DebugRendererRecorder::DrawTriangle(RVec3Arg inV1, RVec3Arg inV2, RVec3Arg inV3, ColorArg inColor, ECastShadow inCastShadow)
{
	lock_guard lock(mMutex);

	mCurrentFrame.mTriangles.push_back({ inV1, inV2, inV3, inColor, inCastShadow });
}

DebugRenderer::Batch DebugRendererRecorder::CreateTriangleBatch(const Triangle *inTriangles, int inTriangleCount)
{
	if (inTriangles == nullptr || inTriangleCount == 0)
		return new BatchImpl(0);

	lock_guard lock(mMutex);

	mStream.Write(ECommand::CreateBatch);

	uint32 batch_id = mNextBatchID++;
	JPH_ASSERT(batch_id != 0);
	mStream.Write(batch_id);
	mStream.Write((uint32)inTriangleCount);
	mStream.WriteBytes(inTriangles, inTriangleCount * sizeof(Triangle));

	return new BatchImpl(batch_id);
}

DebugRenderer::Batch DebugRendererRecorder::CreateTriangleBatch(const Vertex *inVertices, int inVertexCount, const uint32 *inIndices, int inIndexCount)
{
	if (inVertices == nullptr || inVertexCount == 0 || inIndices == nullptr || inIndexCount == 0)
		return new BatchImpl(0);

	lock_guard lock(mMutex);

	mStream.Write(ECommand::CreateBatchIndexed);

	uint32 batch_id = mNextBatchID++;
	JPH_ASSERT(batch_id != 0);
	mStream.Write(batch_id);
	mStream.Write((uint32)inVertexCount);
	mStream.WriteBytes(inVertices, inVertexCount * sizeof(Vertex));
	mStream.Write((uint32)inIndexCount);
	mStream.WriteBytes(inIndices, inIndexCount * sizeof(uint32));

	return new BatchImpl(batch_id);
}

void DebugRendererRecorder::DrawGeometry(RMat44Arg inModelMatrix, const AABox &inWorldSpaceBounds, float inLODScaleSq, ColorArg inModelColor, const GeometryRef &inGeometry, ECullMode inCullMode, ECastShadow inCastShadow, EDrawMode inDrawMode)
{
	lock_guard lock(mMutex);

	// See if this geometry was used before
	uint32 &geometry_id = mGeometries[inGeometry];
	if (geometry_id == 0)
	{
		mStream.Write(ECommand::CreateGeometry);

		// Create a new ID
		geometry_id = mNextGeometryID++;
		JPH_ASSERT(geometry_id != 0);
		mStream.Write(geometry_id);

		// Save bounds
		mStream.Write(inGeometry->mBounds.mMin);
		mStream.Write(inGeometry->mBounds.mMax);

		// Save the LODs
		mStream.Write((uint32)inGeometry->mLODs.size());
		for (const LOD & lod : inGeometry->mLODs)
		{
			mStream.Write(lod.mDistance);
			mStream.Write(static_cast<const BatchImpl *>(lod.mTriangleBatch.GetPtr())->mID);
		}
	}

	mCurrentFrame.mGeometries.push_back({ inModelMatrix, inModelColor, geometry_id, inCullMode, inCastShadow, inDrawMode });
}

void DebugRendererRecorder::DrawText3D(RVec3Arg inPosition, const string_view &inString, ColorArg inColor, float inHeight)
{
	lock_guard lock(mMutex);

	mCurrentFrame.mTexts.push_back({ inPosition, inString, inColor, inHeight });
}

void DebugRendererRecorder::EndFrame()
{
	lock_guard lock(mMutex);

	mStream.Write(ECommand::EndFrame);

	// Write all lines
	mStream.Write((uint32)mCurrentFrame.mLines.size());
	for (const LineBlob &line : mCurrentFrame.mLines)
	{
		mStream.Write(line.mFrom);
		mStream.Write(line.mTo);
		mStream.Write(line.mColor);
	}
	mCurrentFrame.mLines.clear();

	// Write all triangles
	mStream.Write((uint32)mCurrentFrame.mTriangles.size());
	for (const TriangleBlob &triangle : mCurrentFrame.mTriangles)
	{
		mStream.Write(triangle.mV1);
		mStream.Write(triangle.mV2);
		mStream.Write(triangle.mV3);
		mStream.Write(triangle.mColor);
		mStream.Write(triangle.mCastShadow);
	}
	mCurrentFrame.mTriangles.clear();

	// Write all texts
	mStream.Write((uint32)mCurrentFrame.mTexts.size());
	for (const TextBlob &text : mCurrentFrame.mTexts)
	{
		mStream.Write(text.mPosition);
		mStream.Write(text.mString);
		mStream.Write(text.mColor);
		mStream.Write(text.mHeight);
	}
	mCurrentFrame.mTexts.clear();

	// Write all geometries
	mStream.Write((uint32)mCurrentFrame.mGeometries.size());
	for (const GeometryBlob &geom : mCurrentFrame.mGeometries)
	{
		mStream.Write(geom.mModelMatrix);
		mStream.Write(geom.mModelColor);
		mStream.Write(geom.mGeometryID);
		mStream.Write(geom.mCullMode);
		mStream.Write(geom.mCastShadow);
		mStream.Write(geom.mDrawMode);
	}
	mCurrentFrame.mGeometries.clear();
}

JPH_NAMESPACE_END

#endif // JPH_DEBUG_RENDERER

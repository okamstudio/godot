// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2024 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#include <Jolt/Jolt.h>

#ifdef JPH_DEBUG_RENDERER

#include <Jolt/Renderer/DebugRendererSimple.h>

JPH_NAMESPACE_BEGIN

DebugRendererSimple::DebugRendererSimple()
{
	Initialize();
}

DebugRenderer::Batch DebugRendererSimple::CreateTriangleBatch(const Triangle *inTriangles, int inTriangleCount)
{
	BatchImpl *batch = new BatchImpl;
	if (inTriangles == nullptr || inTriangleCount == 0)
		return batch;

	batch->mTriangles.assign(inTriangles, inTriangles + inTriangleCount);
	return batch;
}

DebugRenderer::Batch DebugRendererSimple::CreateTriangleBatch(const Vertex *inVertices, int inVertexCount, const uint32 *inIndices, int inIndexCount)
{
	BatchImpl *batch = new BatchImpl;
	if (inVertices == nullptr || inVertexCount == 0 || inIndices == nullptr || inIndexCount == 0)
		return batch;

	// Convert indexed triangle list to triangle list
	batch->mTriangles.resize(inIndexCount / 3);
	for (size_t t = 0; t < batch->mTriangles.size(); ++t)
	{
		Triangle &triangle = batch->mTriangles[t];
		triangle.mV[0] = inVertices[inIndices[t * 3 + 0]];
		triangle.mV[1] = inVertices[inIndices[t * 3 + 1]];
		triangle.mV[2] = inVertices[inIndices[t * 3 + 2]];
	}

	return batch;
}

void DebugRendererSimple::DrawGeometry(RMat44Arg inModelMatrix, const AABox &inWorldSpaceBounds, float inLODScaleSq, ColorArg inModelColor, const GeometryRef &inGeometry, ECullMode inCullMode, ECastShadow inCastShadow, EDrawMode inDrawMode)
{
	// Figure out which LOD to use
	const LOD *lod = inGeometry->mLODs.data();
	if (mCameraPosSet)
		lod = &inGeometry->GetLOD(Vec3(mCameraPos), inWorldSpaceBounds, inLODScaleSq);

	// Draw the batch
	const BatchImpl *batch = static_cast<const BatchImpl *>(lod->mTriangleBatch.GetPtr());
	for (const Triangle &triangle : batch->mTriangles)
	{
		RVec3 v0 = inModelMatrix * Vec3(triangle.mV[0].mPosition);
		RVec3 v1 = inModelMatrix * Vec3(triangle.mV[1].mPosition);
		RVec3 v2 = inModelMatrix * Vec3(triangle.mV[2].mPosition);
		Color color = inModelColor * triangle.mV[0].mColor;

		switch (inDrawMode)
		{
		case EDrawMode::Wireframe:
			DrawLine(v0, v1, color);
			DrawLine(v1, v2, color);
			DrawLine(v2, v0, color);
			break;

		case EDrawMode::Solid:
			DrawTriangle(v0, v1, v2, color, inCastShadow);
			break;
		}
	}
}

JPH_NAMESPACE_END

#endif // JPH_DEBUG_RENDERER

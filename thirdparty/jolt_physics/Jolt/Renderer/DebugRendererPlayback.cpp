// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#include <Jolt/Jolt.h>

#ifdef JPH_DEBUG_RENDERER

#include <Jolt/Renderer/DebugRendererPlayback.h>

JPH_NAMESPACE_BEGIN

void DebugRendererPlayback::Parse(StreamIn &inStream)
{
	using ECommand = DebugRendererRecorder::ECommand;

	for (;;)
	{
		// Read the next command
		ECommand command;
		inStream.Read(command);

		if (inStream.IsEOF() || inStream.IsFailed())
			return;

		if (command == ECommand::CreateBatch)
		{
			uint32 id;
			inStream.Read(id);

			uint32 triangle_count;
			inStream.Read(triangle_count);

			DebugRenderer::Triangle *triangles = new DebugRenderer::Triangle [triangle_count];
			inStream.ReadBytes(triangles, triangle_count * sizeof(DebugRenderer::Triangle));

			mBatches.insert({ id, mRenderer.CreateTriangleBatch(triangles, triangle_count) });

			delete [] triangles;
		}
		else if (command == ECommand::CreateBatchIndexed)
		{
			uint32 id;
			inStream.Read(id);

			uint32 vertex_count;
			inStream.Read(vertex_count);

			DebugRenderer::Vertex *vertices = new DebugRenderer::Vertex [vertex_count];
			inStream.ReadBytes(vertices, vertex_count * sizeof(DebugRenderer::Vertex));

			uint32 index_count;
			inStream.Read(index_count);

			uint32 *indices = new uint32 [index_count];
			inStream.ReadBytes(indices, index_count * sizeof(uint32));

			mBatches.insert({ id, mRenderer.CreateTriangleBatch(vertices, vertex_count, indices, index_count) });

			delete [] indices;
			delete [] vertices;
		}
		else if (command == ECommand::CreateGeometry)
		{
			uint32 geometry_id;
			inStream.Read(geometry_id);

			AABox bounds;
			inStream.Read(bounds.mMin);
			inStream.Read(bounds.mMax);

			DebugRenderer::GeometryRef geometry = new DebugRenderer::Geometry(bounds);
			mGeometries[geometry_id] = geometry;

			uint32 num_lods;
			inStream.Read(num_lods);
			for (uint32 l = 0; l < num_lods; ++l)
			{
				DebugRenderer::LOD lod;
				inStream.Read(lod.mDistance);

				uint32 batch_id;
				inStream.Read(batch_id);
				lod.mTriangleBatch = mBatches.find(batch_id)->second;

				geometry->mLODs.push_back(lod);
			}
		}
		else if (command == ECommand::EndFrame)
		{
			mFrames.push_back({});
			Frame &frame = mFrames.back();

			// Read all lines
			uint32 num_lines = 0;
			inStream.Read(num_lines);
			frame.mLines.resize(num_lines);
			for (DebugRendererRecorder::LineBlob &line : frame.mLines)
			{
				inStream.Read(line.mFrom);
				inStream.Read(line.mTo);
				inStream.Read(line.mColor);
			}

			// Read all triangles
			uint32 num_triangles = 0;
			inStream.Read(num_triangles);
			frame.mTriangles.resize(num_triangles);
			for (DebugRendererRecorder::TriangleBlob &triangle : frame.mTriangles)
			{
				inStream.Read(triangle.mV1);
				inStream.Read(triangle.mV2);
				inStream.Read(triangle.mV3);
				inStream.Read(triangle.mColor);
				inStream.Read(triangle.mCastShadow);
			}

			// Read all texts
			uint32 num_texts = 0;
			inStream.Read(num_texts);
			frame.mTexts.resize(num_texts);
			for (DebugRendererRecorder::TextBlob &text : frame.mTexts)
			{
				inStream.Read(text.mPosition);
				inStream.Read(text.mString);
				inStream.Read(text.mColor);
				inStream.Read(text.mHeight);
			}

			// Read all geometries
			uint32 num_geometries = 0;
			inStream.Read(num_geometries);
			frame.mGeometries.resize(num_geometries);
			for (DebugRendererRecorder::GeometryBlob &geom : frame.mGeometries)
			{
				inStream.Read(geom.mModelMatrix);
				inStream.Read(geom.mModelColor);
				inStream.Read(geom.mGeometryID);
				inStream.Read(geom.mCullMode);
				inStream.Read(geom.mCastShadow);
				inStream.Read(geom.mDrawMode);
			}
		}
		else
			JPH_ASSERT(false);
	}
}

void DebugRendererPlayback::DrawFrame(uint inFrameNumber) const
{
	const Frame &frame = mFrames[inFrameNumber];

	for (const DebugRendererRecorder::LineBlob &line : frame.mLines)
		mRenderer.DrawLine(line.mFrom, line.mTo, line.mColor);

	for (const DebugRendererRecorder::TriangleBlob &triangle : frame.mTriangles)
		mRenderer.DrawTriangle(triangle.mV1, triangle.mV2, triangle.mV3, triangle.mColor, triangle.mCastShadow);

	for (const DebugRendererRecorder::TextBlob &text : frame.mTexts)
		mRenderer.DrawText3D(text.mPosition, text.mString, text.mColor, text.mHeight);

	for (const DebugRendererRecorder::GeometryBlob &geom : frame.mGeometries)
		mRenderer.DrawGeometry(geom.mModelMatrix, geom.mModelColor, mGeometries.find(geom.mGeometryID)->second, geom.mCullMode, geom.mCastShadow, geom.mDrawMode);
}

JPH_NAMESPACE_END

#endif // JPH_DEBUG_RENDERER

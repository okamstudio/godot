// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#ifndef JPH_DEBUG_RENDERER
	#error This file should only be included when JPH_DEBUG_RENDERER is defined
#endif // !JPH_DEBUG_RENDERER

#include <Jolt/Renderer/DebugRenderer.h>
#include <Jolt/Core/StreamOut.h>
#include <Jolt/Core/Mutex.h>
#include <Jolt/Core/UnorderedMap.h>

JPH_NAMESPACE_BEGIN

/// Implementation of DebugRenderer that records the API invocations to be played back later
class JPH_DEBUG_RENDERER_EXPORT DebugRendererRecorder final : public DebugRenderer
{
public:
	JPH_OVERRIDE_NEW_DELETE

	/// Constructor
										DebugRendererRecorder(StreamOut &inStream) : mStream(inStream) { Initialize(); }

	/// Implementation of DebugRenderer interface
	virtual void						DrawLine(RVec3Arg inFrom, RVec3Arg inTo, ColorArg inColor) override;
	virtual void						DrawTriangle(RVec3Arg inV1, RVec3Arg inV2, RVec3Arg inV3, ColorArg inColor, ECastShadow inCastShadow) override;
	virtual Batch						CreateTriangleBatch(const Triangle *inTriangles, int inTriangleCount) override;
	virtual Batch						CreateTriangleBatch(const Vertex *inVertices, int inVertexCount, const uint32 *inIndices, int inIndexCount) override;
	virtual void						DrawGeometry(RMat44Arg inModelMatrix, const AABox &inWorldSpaceBounds, float inLODScaleSq, ColorArg inModelColor, const GeometryRef &inGeometry, ECullMode inCullMode, ECastShadow inCastShadow, EDrawMode inDrawMode) override;
	virtual void						DrawText3D(RVec3Arg inPosition, const string_view &inString, ColorArg inColor, float inHeight) override;

	/// Mark the end of a frame
	void								EndFrame();

	/// Control commands written into the stream
	enum class ECommand : uint8
	{
		CreateBatch,
		CreateBatchIndexed,
		CreateGeometry,
		EndFrame
	};

	/// Holds a single line segment
	struct LineBlob
	{
		RVec3							mFrom;
		RVec3							mTo;
		Color							mColor;
	};

	/// Holds a single triangle
	struct TriangleBlob
	{
		RVec3							mV1;
		RVec3							mV2;
		RVec3							mV3;
		Color							mColor;
		ECastShadow						mCastShadow;
	};

	/// Holds a single text entry
	struct TextBlob
	{
										TextBlob() = default;
										TextBlob(RVec3Arg inPosition, const string_view &inString, ColorArg inColor, float inHeight) : mPosition(inPosition), mString(inString), mColor(inColor), mHeight(inHeight) { }

		RVec3							mPosition;
		String							mString;
		Color							mColor;
		float							mHeight;
	};

	/// Holds a single geometry draw call
	struct GeometryBlob
	{
		RMat44							mModelMatrix;
		Color							mModelColor;
		uint32							mGeometryID;
		ECullMode						mCullMode;
		ECastShadow						mCastShadow;
		EDrawMode						mDrawMode;
	};

	/// All information for a single frame
	struct Frame
	{
		Array<LineBlob>					mLines;
		Array<TriangleBlob>				mTriangles;
		Array<TextBlob>					mTexts;
		Array<GeometryBlob>				mGeometries;
	};

private:
	/// Implementation specific batch object
	class BatchImpl : public RefTargetVirtual
	{
	public:
		JPH_OVERRIDE_NEW_DELETE

										BatchImpl(uint32 inID)		: mID(inID) {  }

		virtual void					AddRef() override			{ ++mRefCount; }
		virtual void					Release() override			{ if (--mRefCount == 0) delete this; }

		atomic<uint32>					mRefCount = 0;
		uint32							mID;
	};

	/// Lock that prevents concurrent access to the internal structures
	Mutex								mMutex;

	/// Stream that recorded data will be sent to
	StreamOut &							mStream;

	/// Next available ID
	uint32								mNextBatchID = 1;
	uint32								mNextGeometryID = 1;

	/// Cached geometries and their IDs
	UnorderedMap<GeometryRef, uint32>	mGeometries;

	/// Data that is being accumulated for the current frame
	Frame								mCurrentFrame;
};

JPH_NAMESPACE_END

// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#ifndef JPH_DEBUG_RENDERER
	#error This file should only be included when JPH_DEBUG_RENDERER is defined
#endif // !JPH_DEBUG_RENDERER

#include <Jolt/Renderer/DebugRendererRecorder.h>
#include <Jolt/Core/StreamIn.h>
#include <Jolt/Core/UnorderedMap.h>

JPH_NAMESPACE_BEGIN

/// Class that can read a recorded stream from DebugRendererRecorder and plays it back trough a DebugRenderer
class JPH_DEBUG_RENDERER_EXPORT DebugRendererPlayback
{
public:
	/// Constructor
										DebugRendererPlayback(DebugRenderer &inRenderer) : mRenderer(inRenderer) { }

	/// Parse a stream of frames
	void								Parse(StreamIn &inStream);

	/// Get the number of parsed frames
	uint								GetNumFrames() const				{ return (uint)mFrames.size(); }

	/// Draw a frame
	void								DrawFrame(uint inFrameNumber) const;

private:
	/// The debug renderer we're using to do the actual rendering
	DebugRenderer &						mRenderer;

	/// Mapping of ID to batch
	UnorderedMap<uint32, DebugRenderer::Batch> mBatches;

	/// Mapping of ID to geometry
	UnorderedMap<uint32, DebugRenderer::GeometryRef> mGeometries;

	/// The list of parsed frames
	using Frame = DebugRendererRecorder::Frame;
	Array<Frame>						mFrames;
};

JPH_NAMESPACE_END

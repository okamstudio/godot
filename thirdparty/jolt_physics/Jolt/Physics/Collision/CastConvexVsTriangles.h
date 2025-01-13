// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Jolt/Physics/Collision/Shape/ConvexShape.h>
#include <Jolt/Physics/Collision/ShapeCast.h>

JPH_NAMESPACE_BEGIN

/// Collision detection helper that casts a convex object vs one or more triangles
class JPH_EXPORT CastConvexVsTriangles
{
public:
	/// Constructor
	/// @param inShapeCast The shape to cast against the triangles and its start and direction
	/// @param inShapeCastSettings Settings for performing the cast
	/// @param inScale Local space scale for the shape to cast against (scales relative to its center of mass).
	/// @param inCenterOfMassTransform2 Is the center of mass transform of shape 2 (excluding scale), this is used to provide a transform to the shape cast result so that local quantities can be transformed into world space.
	/// @param inSubShapeIDCreator1 Class that tracks the current sub shape ID for the casting shape
	/// @param ioCollector The collector that receives the results.
									CastConvexVsTriangles(const ShapeCast &inShapeCast, const ShapeCastSettings &inShapeCastSettings, Vec3Arg inScale, Mat44Arg inCenterOfMassTransform2, const SubShapeIDCreator &inSubShapeIDCreator1, CastShapeCollector &ioCollector);

	/// Cast convex object with a single triangle
	/// @param inV0 , inV1 , inV2: CCW triangle vertices
	/// @param inActiveEdges bit 0 = edge v0..v1 is active, bit 1 = edge v1..v2 is active, bit 2 = edge v2..v0 is active
	/// An active edge is an edge that is not connected to another triangle in such a way that it is impossible to collide with the edge
	/// @param inSubShapeID2 The sub shape ID for the triangle
	void							Cast(Vec3Arg inV0, Vec3Arg inV1, Vec3Arg inV2, uint8 inActiveEdges, const SubShapeID &inSubShapeID2);

protected:
	const ShapeCast &				mShapeCast;
	const ShapeCastSettings &		mShapeCastSettings;
	const Mat44 &					mCenterOfMassTransform2;
	Vec3							mScale;
	SubShapeIDCreator				mSubShapeIDCreator1;
	CastShapeCollector &			mCollector;

private:
	ConvexShape::SupportBuffer		mSupportBuffer;						///< Buffer that holds the support function of the cast shape
	const ConvexShape::Support *	mSupport = nullptr;					///< Support function of the cast shape
	float							mScaleSign;							///< Sign of the scale, -1 if object is inside out, 1 if not
};

JPH_NAMESPACE_END

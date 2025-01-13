// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Jolt/Physics/Collision/ShapeCast.h>

JPH_NAMESPACE_BEGIN

/// Collision detection helper that casts a sphere vs one or more triangles
class JPH_EXPORT CastSphereVsTriangles
{
public:
	/// Constructor
	/// @param inShapeCast The sphere to cast against the triangles and its start and direction
	/// @param inShapeCastSettings Settings for performing the cast
	/// @param inScale Local space scale for the shape to cast against (scales relative to its center of mass).
	/// @param inCenterOfMassTransform2 Is the center of mass transform of shape 2 (excluding scale), this is used to provide a transform to the shape cast result so that local quantities can be transformed into world space.
	/// @param inSubShapeIDCreator1 Class that tracks the current sub shape ID for the casting shape
	/// @param ioCollector The collector that receives the results.
									CastSphereVsTriangles(const ShapeCast &inShapeCast, const ShapeCastSettings &inShapeCastSettings, Vec3Arg inScale, Mat44Arg inCenterOfMassTransform2, const SubShapeIDCreator &inSubShapeIDCreator1, CastShapeCollector &ioCollector);

	/// Cast sphere with a single triangle
	/// @param inV0 , inV1 , inV2: CCW triangle vertices
	/// @param inActiveEdges bit 0 = edge v0..v1 is active, bit 1 = edge v1..v2 is active, bit 2 = edge v2..v0 is active
	/// An active edge is an edge that is not connected to another triangle in such a way that it is impossible to collide with the edge
	/// @param inSubShapeID2 The sub shape ID for the triangle
	void							Cast(Vec3Arg inV0, Vec3Arg inV1, Vec3Arg inV2, uint8 inActiveEdges, const SubShapeID &inSubShapeID2);

protected:
	Vec3							mStart;								///< Starting location of the sphere
	Vec3							mDirection;							///< Direction and length of movement of sphere
	float							mRadius;							///< Scaled radius of sphere
	const ShapeCastSettings &		mShapeCastSettings;
	const Mat44 &					mCenterOfMassTransform2;
	Vec3							mScale;
	SubShapeIDCreator				mSubShapeIDCreator1;
	CastShapeCollector &			mCollector;

private:
	void							AddHit(bool inBackFacing, const SubShapeID &inSubShapeID2, float inFraction, Vec3Arg inContactPointA, Vec3Arg inContactPointB, Vec3Arg inContactNormal);
	void							AddHitWithActiveEdgeDetection(Vec3Arg inV0, Vec3Arg inV1, Vec3Arg inV2, bool inBackFacing, Vec3Arg inTriangleNormal, uint8 inActiveEdges, const SubShapeID &inSubShapeID2, float inFraction, Vec3Arg inContactPointA, Vec3Arg inContactPointB, Vec3Arg inContactNormal);
	float							RayCylinder(Vec3Arg inRayDirection, Vec3Arg inCylinderA, Vec3Arg inCylinderB, float inRadius) const;

	float							mScaleSign;							///< Sign of the scale, -1 if object is inside out, 1 if not
};

JPH_NAMESPACE_END

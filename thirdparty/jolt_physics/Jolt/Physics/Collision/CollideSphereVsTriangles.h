// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Jolt/Physics/Collision/Shape/Shape.h>
#include <Jolt/Physics/Collision/Shape/SubShapeID.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>

JPH_NAMESPACE_BEGIN

class CollideShapeSettings;

/// Collision detection helper that collides a sphere vs one or more triangles
class JPH_EXPORT CollideSphereVsTriangles
{
public:
	/// Constructor
	/// @param inShape1 The sphere to collide against triangles
	/// @param inScale1 Local space scale for the sphere (scales relative to its center of mass)
	/// @param inScale2 Local space scale for the triangles
	/// @param inCenterOfMassTransform1 Transform that takes the center of mass of 1 into world space
	/// @param inCenterOfMassTransform2 Transform that takes the center of mass of 2 into world space
	/// @param inSubShapeID1 Sub shape ID of the convex object
	/// @param inCollideShapeSettings Settings for the collide shape query
	/// @param ioCollector The collector that will receive the results
									CollideSphereVsTriangles(const SphereShape *inShape1, Vec3Arg inScale1, Vec3Arg inScale2, Mat44Arg inCenterOfMassTransform1, Mat44Arg inCenterOfMassTransform2, const SubShapeID &inSubShapeID1, const CollideShapeSettings &inCollideShapeSettings, CollideShapeCollector &ioCollector);

	/// Collide sphere with a single triangle
	/// @param inV0 , inV1 , inV2: CCW triangle vertices
	/// @param inActiveEdges bit 0 = edge v0..v1 is active, bit 1 = edge v1..v2 is active, bit 2 = edge v2..v0 is active
	/// An active edge is an edge that is not connected to another triangle in such a way that it is impossible to collide with the edge
	/// @param inSubShapeID2 The sub shape ID for the triangle
	void							Collide(Vec3Arg inV0, Vec3Arg inV1, Vec3Arg inV2, uint8 inActiveEdges, const SubShapeID &inSubShapeID2);

protected:
	const CollideShapeSettings &	mCollideShapeSettings;					///< Settings for this collision operation
	CollideShapeCollector &			mCollector;								///< The collector that will receive the results
	const SphereShape *				mShape1;								///< The shape that we're colliding with
	Vec3							mScale2;								///< The scale of the shape (in shape local space) of the shape we're colliding against
	Mat44							mTransform2;							///< Transform of the shape we're colliding against
	Vec3							mSphereCenterIn2;						///< The center of the sphere in the space of 2
	SubShapeID						mSubShapeID1;							///< Sub shape ID of colliding shape
	float							mScaleSign2;							///< Sign of the scale of object 2, -1 if object is inside out, 1 if not
	float							mRadius;								///< Radius of the sphere
	float							mRadiusPlusMaxSeparationSq;				///< (Radius + Max SeparationDistance)^2
};

JPH_NAMESPACE_END

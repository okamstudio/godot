// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Jolt/Physics/Body/BodyFilter.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Body/BodyLockInterface.h>
#include <Jolt/Physics/Collision/ShapeFilter.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseQuery.h>
#include <Jolt/Physics/Collision/BackFaceMode.h>

JPH_NAMESPACE_BEGIN

class Shape;
class CollideShapeSettings;
class RayCastResult;

/// Class that provides an interface for doing precise collision detection against the broad and then the narrow phase.
/// Unlike a BroadPhaseQuery, the NarrowPhaseQuery will test against shapes and will return collision information against triangles, spheres etc.
class JPH_EXPORT NarrowPhaseQuery : public NonCopyable
{
public:
	/// Initialize the interface (should only be called by PhysicsSystem)
	void						Init(BodyLockInterface &inBodyLockInterface, BroadPhaseQuery &inBroadPhaseQuery) { mBodyLockInterface = &inBodyLockInterface; mBroadPhaseQuery = &inBroadPhaseQuery; }

	/// Cast a ray and find the closest hit. Returns true if it finds a hit. Hits further than ioHit.mFraction will not be considered and in this case ioHit will remain unmodified (and the function will return false).
	/// Convex objects will be treated as solid (meaning if the ray starts inside, you'll get a hit fraction of 0) and back face hits against triangles are returned.
	/// If you want the surface normal of the hit use Body::GetWorldSpaceSurfaceNormal(ioHit.mSubShapeID2, inRay.GetPointOnRay(ioHit.mFraction)) on body with ID ioHit.mBodyID.
	bool						CastRay(const RRayCast &inRay, RayCastResult &ioHit, const BroadPhaseLayerFilter &inBroadPhaseLayerFilter = { }, const ObjectLayerFilter &inObjectLayerFilter = { }, const BodyFilter &inBodyFilter = { }) const;

	/// Cast a ray, allows collecting multiple hits. Note that this version is more flexible but also slightly slower than the CastRay function that returns only a single hit.
	/// If you want the surface normal of the hit use Body::GetWorldSpaceSurfaceNormal(collected sub shape ID, inRay.GetPointOnRay(collected fraction)) on body with collected body ID.
	void						CastRay(const RRayCast &inRay, const RayCastSettings &inRayCastSettings, CastRayCollector &ioCollector, const BroadPhaseLayerFilter &inBroadPhaseLayerFilter = { }, const ObjectLayerFilter &inObjectLayerFilter = { }, const BodyFilter &inBodyFilter = { }, const ShapeFilter &inShapeFilter = { }) const;

	/// Check if inPoint is inside any shapes. For this tests all shapes are treated as if they were solid.
	/// For a mesh shape, this test will only provide sensible information if the mesh is a closed manifold.
	/// For each shape that collides, ioCollector will receive a hit
	void						CollidePoint(RVec3Arg inPoint, CollidePointCollector &ioCollector, const BroadPhaseLayerFilter &inBroadPhaseLayerFilter = { }, const ObjectLayerFilter &inObjectLayerFilter = { }, const BodyFilter &inBodyFilter = { }, const ShapeFilter &inShapeFilter = { }) const;

	/// Collide a shape with the system
	/// @param inShape Shape to test
	/// @param inShapeScale Scale in local space of shape
	/// @param inCenterOfMassTransform Center of mass transform for the shape
	/// @param inCollideShapeSettings Settings
	/// @param inBaseOffset All hit results will be returned relative to this offset, can be zero to get results in world position, but when you're testing far from the origin you get better precision by picking a position that's closer e.g. inCenterOfMassTransform.GetTranslation() since floats are most accurate near the origin
	/// @param ioCollector Collector that receives the hits
	/// @param inBroadPhaseLayerFilter Filter that filters at broadphase level
	/// @param inObjectLayerFilter Filter that filters at layer level
	/// @param inBodyFilter Filter that filters at body level
	/// @param inShapeFilter Filter that filters at shape level
	void						CollideShape(const Shape *inShape, Vec3Arg inShapeScale, RMat44Arg inCenterOfMassTransform, const CollideShapeSettings &inCollideShapeSettings, RVec3Arg inBaseOffset, CollideShapeCollector &ioCollector, const BroadPhaseLayerFilter &inBroadPhaseLayerFilter = { }, const ObjectLayerFilter &inObjectLayerFilter = { }, const BodyFilter &inBodyFilter = { }, const ShapeFilter &inShapeFilter = { }) const;

	/// Same as CollideShape, but uses InternalEdgeRemovingCollector to remove internal edges from the collision results (a.k.a. ghost collisions)
	void						CollideShapeWithInternalEdgeRemoval(const Shape *inShape, Vec3Arg inShapeScale, RMat44Arg inCenterOfMassTransform, const CollideShapeSettings &inCollideShapeSettings, RVec3Arg inBaseOffset, CollideShapeCollector &ioCollector, const BroadPhaseLayerFilter &inBroadPhaseLayerFilter = { }, const ObjectLayerFilter &inObjectLayerFilter = { }, const BodyFilter &inBodyFilter = { }, const ShapeFilter &inShapeFilter = { }) const;

	/// Cast a shape and report any hits to ioCollector
	/// @param inShapeCast The shape cast and its position and direction
	/// @param inShapeCastSettings Settings for the shape cast
	/// @param inBaseOffset All hit results will be returned relative to this offset, can be zero to get results in world position, but when you're testing far from the origin you get better precision by picking a position that's closer e.g. inShapeCast.mCenterOfMassStart.GetTranslation() since floats are most accurate near the origin
	/// @param ioCollector Collector that receives the hits
	/// @param inBroadPhaseLayerFilter Filter that filters at broadphase level
	/// @param inObjectLayerFilter Filter that filters at layer level
	/// @param inBodyFilter Filter that filters at body level
	/// @param inShapeFilter Filter that filters at shape level
	void						CastShape(const RShapeCast &inShapeCast, const ShapeCastSettings &inShapeCastSettings, RVec3Arg inBaseOffset, CastShapeCollector &ioCollector, const BroadPhaseLayerFilter &inBroadPhaseLayerFilter = { }, const ObjectLayerFilter &inObjectLayerFilter = { }, const BodyFilter &inBodyFilter = { }, const ShapeFilter &inShapeFilter = { }) const;

	/// Collect all leaf transformed shapes that fall inside world space box inBox
	void						CollectTransformedShapes(const AABox &inBox, TransformedShapeCollector &ioCollector, const BroadPhaseLayerFilter &inBroadPhaseLayerFilter = { }, const ObjectLayerFilter &inObjectLayerFilter = { }, const BodyFilter &inBodyFilter = { }, const ShapeFilter &inShapeFilter = { }) const;

private:
	BodyLockInterface *			mBodyLockInterface = nullptr;
	BroadPhaseQuery *			mBroadPhaseQuery = nullptr;
};

JPH_NAMESPACE_END

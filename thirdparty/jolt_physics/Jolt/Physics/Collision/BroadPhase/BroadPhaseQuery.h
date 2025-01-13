// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>
#include <Jolt/Physics/Collision/CollisionCollector.h>
#include <Jolt/Physics/Body/BodyID.h>
#include <Jolt/Core/NonCopyable.h>

JPH_NAMESPACE_BEGIN

struct RayCast;
class BroadPhaseCastResult;
class AABox;
class OrientedBox;
struct AABoxCast;

// Various collector configurations
using RayCastBodyCollector = CollisionCollector<BroadPhaseCastResult, CollisionCollectorTraitsCastRay>;
using CastShapeBodyCollector = CollisionCollector<BroadPhaseCastResult, CollisionCollectorTraitsCastShape>;
using CollideShapeBodyCollector = CollisionCollector<BodyID, CollisionCollectorTraitsCollideShape>;

/// Interface to the broadphase that can perform collision queries. These queries will only test the bounding box of the body to quickly determine a potential set of colliding bodies.
/// The shapes of the bodies are not tested, if you want this then you should use the NarrowPhaseQuery interface.
class JPH_EXPORT BroadPhaseQuery : public NonCopyable
{
public:
	/// Virtual destructor
	virtual				~BroadPhaseQuery() = default;

	/// Cast a ray and add any hits to ioCollector
	virtual void		CastRay(const RayCast &inRay, RayCastBodyCollector &ioCollector, const BroadPhaseLayerFilter &inBroadPhaseLayerFilter = { }, const ObjectLayerFilter &inObjectLayerFilter = { }) const = 0;

	/// Get bodies intersecting with inBox and any hits to ioCollector
	virtual void		CollideAABox(const AABox &inBox, CollideShapeBodyCollector &ioCollector, const BroadPhaseLayerFilter &inBroadPhaseLayerFilter = { }, const ObjectLayerFilter &inObjectLayerFilter = { }) const = 0;

	/// Get bodies intersecting with a sphere and any hits to ioCollector
	virtual void		CollideSphere(Vec3Arg inCenter, float inRadius, CollideShapeBodyCollector &ioCollector, const BroadPhaseLayerFilter &inBroadPhaseLayerFilter = { }, const ObjectLayerFilter &inObjectLayerFilter = { }) const = 0;

	/// Get bodies intersecting with a point and any hits to ioCollector
	virtual void		CollidePoint(Vec3Arg inPoint, CollideShapeBodyCollector &ioCollector, const BroadPhaseLayerFilter &inBroadPhaseLayerFilter = { }, const ObjectLayerFilter &inObjectLayerFilter = { }) const = 0;

	/// Get bodies intersecting with an oriented box and any hits to ioCollector
	virtual void		CollideOrientedBox(const OrientedBox &inBox, CollideShapeBodyCollector &ioCollector, const BroadPhaseLayerFilter &inBroadPhaseLayerFilter = { }, const ObjectLayerFilter &inObjectLayerFilter = { }) const = 0;

	/// Cast a box and add any hits to ioCollector
	virtual void		CastAABox(const AABoxCast &inBox, CastShapeBodyCollector &ioCollector, const BroadPhaseLayerFilter &inBroadPhaseLayerFilter = { }, const ObjectLayerFilter &inObjectLayerFilter = { }) const = 0;
};

JPH_NAMESPACE_END

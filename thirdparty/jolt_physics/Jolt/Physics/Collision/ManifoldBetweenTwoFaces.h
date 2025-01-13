// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Jolt/Physics/Collision/Shape/ConvexShape.h>
#include <Jolt/Physics/Collision/ContactListener.h>

JPH_NAMESPACE_BEGIN

/// Remove contact points if there are > 4 (no more than 4 are needed for a stable solution)
/// @param inPenetrationAxis is the world space penetration axis (must be normalized)
/// @param ioContactPointsOn1 The contact points on shape 1 relative to inCenterOfMass
/// @param ioContactPointsOn2 The contact points on shape 2 relative to inCenterOfMass
/// On output ioContactPointsOn1/2 are reduced to 4 or less points
#ifdef JPH_DEBUG_RENDERER
/// @param inCenterOfMass Center of mass position of body 1
#endif
JPH_EXPORT void PruneContactPoints(Vec3Arg inPenetrationAxis, ContactPoints &ioContactPointsOn1, ContactPoints &ioContactPointsOn2
#ifdef JPH_DEBUG_RENDERER
	, RVec3Arg inCenterOfMass
#endif
	);

/// Determine contact points between 2 faces of 2 shapes and return them in outContactPoints 1 & 2
/// @param inContactPoint1 The contact point on shape 1 relative to inCenterOfMass
/// @param inContactPoint2 The contact point on shape 2 relative to inCenterOfMass
/// @param inPenetrationAxis The local space penetration axis in world space
/// @param inMaxContactDistanceSq After face 2 is clipped against face 1, each remaining point on face 2 is tested against the plane of face 1. If the distance^2 on the positive side of the plane is larger than this distance, the point will be discarded as a contact point.
/// @param inShape1Face The supporting faces on shape 1 relative to inCenterOfMass
/// @param inShape2Face The supporting faces on shape 2 relative to inCenterOfMass
/// @param outContactPoints1 Returns the contact points between the two shapes for shape 1 relative to inCenterOfMass (any existing points in the output array are left as is)
/// @param outContactPoints2 Returns the contact points between the two shapes for shape 2 relative to inCenterOfMass (any existing points in the output array are left as is)
#ifdef JPH_DEBUG_RENDERER
/// @param inCenterOfMass Center of mass position of body 1
#endif
JPH_EXPORT void ManifoldBetweenTwoFaces(Vec3Arg inContactPoint1, Vec3Arg inContactPoint2, Vec3Arg inPenetrationAxis, float inMaxContactDistanceSq, const ConvexShape::SupportingFace &inShape1Face, const ConvexShape::SupportingFace &inShape2Face, ContactPoints &outContactPoints1, ContactPoints &outContactPoints2
#ifdef JPH_DEBUG_RENDERER
	, RVec3Arg inCenterOfMass
#endif
	);

JPH_NAMESPACE_END

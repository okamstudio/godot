// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Jolt/Geometry/ClosestPoint.h>

JPH_NAMESPACE_BEGIN

/// An active edge is an edge that either has no neighbouring edge or if the angle between the two connecting faces is too large.
namespace ActiveEdges
{
	/// Helper function to check if an edge is active or not
	/// @param inNormal1 Triangle normal of triangle on the left side of the edge (when looking along the edge from the top)
	/// @param inNormal2 Triangle normal of triangle on the right side of the edge
	/// @param inEdgeDirection Vector that points along the edge
	/// @param inCosThresholdAngle Cosine of the threshold angle (if the angle between the two triangles is bigger than this, the edge is active, note that a concave edge is always inactive)
	inline static bool					IsEdgeActive(Vec3Arg inNormal1, Vec3Arg inNormal2, Vec3Arg inEdgeDirection, float inCosThresholdAngle)
	{
		// If normals are opposite the edges are active (the triangles are back to back)
		float cos_angle_normals = inNormal1.Dot(inNormal2);
		if (cos_angle_normals < -0.999848f) // cos(179 degrees)
			return true;

		// Check if concave edge, if so we are not active
		if (inNormal1.Cross(inNormal2).Dot(inEdgeDirection) < 0.0f)
			return false;

		// Convex edge, active when angle bigger than threshold
		return cos_angle_normals < inCosThresholdAngle;
	}

	/// Replace normal by triangle normal if a hit is hitting an inactive edge
	/// @param inV0 , inV1 , inV2 form the triangle
	/// @param inTriangleNormal is the normal of the provided triangle (does not need to be normalized)
	/// @param inActiveEdges bit 0 = edge v0..v1 is active, bit 1 = edge v1..v2 is active, bit 2 = edge v2..v0 is active
	/// @param inPoint Collision point on the triangle
	/// @param inNormal Collision normal on the triangle (does not need to be normalized)
	/// @param inMovementDirection Can be zero. This gives an indication of in which direction the motion is to determine if when we hit an inactive edge/triangle we should return the triangle normal.
	/// @return Returns inNormal if an active edge was hit, otherwise returns inTriangleNormal
	inline static Vec3					FixNormal(Vec3Arg inV0, Vec3Arg inV1, Vec3Arg inV2, Vec3Arg inTriangleNormal, uint8 inActiveEdges, Vec3Arg inPoint, Vec3Arg inNormal, Vec3Arg inMovementDirection)
	{
		// Check: All of the edges are active, we have the correct normal already. No need to call this function!
		JPH_ASSERT(inActiveEdges != 0b111);

		// If inNormal would affect movement less than inTriangleNormal use inNormal
		// This is done since it is really hard to make a distinction between sliding over a horizontal triangulated grid and hitting an edge (in this case you want to use the triangle normal)
		// and sliding over a triangulated grid and grazing a vertical triangle with an inactive edge (in this case using the triangle normal will cause the object to bounce back so we want to use the calculated normal).
		// To solve this we take a movement hint to give an indication of what direction our object is moving. If the edge normal results in less motion difference than the triangle normal we use the edge normal.
		float normal_length = inNormal.Length();
		float triangle_normal_length = inTriangleNormal.Length();
		if (inMovementDirection.Dot(inNormal) * triangle_normal_length < inMovementDirection.Dot(inTriangleNormal) * normal_length)
			return inNormal;

		// Check: None of the edges are active, we need to use the triangle normal
		if (inActiveEdges == 0)
			return inTriangleNormal;

		// Some edges are active.
		// If normal is parallel to the triangle normal we don't need to check the active edges.
		if (inTriangleNormal.Dot(inNormal) > 0.999848f * normal_length * triangle_normal_length) // cos(1 degree)
			return inNormal;

		const float cEpsilon = 1.0e-4f;
		const float cOneMinusEpsilon = 1.0f - cEpsilon;

		uint colliding_edge;

		// Test where the contact point is in the triangle
		float u, v, w;
		ClosestPoint::GetBaryCentricCoordinates(inV0 - inPoint, inV1 - inPoint, inV2 - inPoint, u, v, w);
		if (u > cOneMinusEpsilon)
		{
			// Colliding with v0, edge 0 or 2 needs to be active
			colliding_edge = 0b101;
		}
		else if (v > cOneMinusEpsilon)
		{
			// Colliding with v1, edge 0 or 1 needs to be active
			colliding_edge = 0b011;
		}
		else if (w > cOneMinusEpsilon)
		{
			// Colliding with v2, edge 1 or 2 needs to be active
			colliding_edge = 0b110;
		}
		else if (u < cEpsilon)
		{
			// Colliding with edge v1, v2, edge 1 needs to be active
			colliding_edge = 0b010;
		}
		else if (v < cEpsilon)
		{
			// Colliding with edge v0, v2, edge 2 needs to be active
			colliding_edge = 0b100;
		}
		else if (w < cEpsilon)
		{
			// Colliding with edge v0, v1, edge 0 needs to be active
			colliding_edge = 0b001;
		}
		else
		{
			// Interior hit
			return inTriangleNormal;
		}

		// If this edge is active, use the provided normal instead of the triangle normal
		return (inActiveEdges & colliding_edge) != 0? inNormal : inTriangleNormal;
	}
}

JPH_NAMESPACE_END

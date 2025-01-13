// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#include <Jolt/Jolt.h>

#include <Jolt/Physics/Collision/ManifoldBetweenTwoFaces.h>
#include <Jolt/Physics/Constraints/ContactConstraintManager.h>
#include <Jolt/Geometry/ClipPoly.h>
#ifdef JPH_DEBUG_RENDERER
	#include <Jolt/Renderer/DebugRenderer.h>
#endif // JPH_DEBUG_RENDERER

JPH_NAMESPACE_BEGIN

void PruneContactPoints(Vec3Arg inPenetrationAxis, ContactPoints &ioContactPointsOn1, ContactPoints &ioContactPointsOn2 JPH_IF_DEBUG_RENDERER(, RVec3Arg inCenterOfMass))
{
	// Makes no sense to call this with 4 or less points
	JPH_ASSERT(ioContactPointsOn1.size() > 4);

	// Both arrays should have the same size
	JPH_ASSERT(ioContactPointsOn1.size() == ioContactPointsOn2.size());

	// Penetration axis must be normalized
	JPH_ASSERT(inPenetrationAxis.IsNormalized());

	// We use a heuristic of (distance to center of mass) * (penetration depth) to find the contact point that we should keep
	// Neither of those two terms should ever become zero, so we clamp against this minimum value
	constexpr float cMinDistanceSq = 1.0e-6f; // 1 mm

	ContactPoints projected;
	StaticArray<float, 64> penetration_depth_sq;
	for (ContactPoints::size_type i = 0; i < ioContactPointsOn1.size(); ++i)
	{
		// Project contact points on the plane through inCenterOfMass with normal inPenetrationAxis and center around the center of mass of body 1
		// (note that since all points are relative to inCenterOfMass we can project onto the plane through the origin)
		Vec3 v1 = ioContactPointsOn1[i];
		projected.push_back(v1 - v1.Dot(inPenetrationAxis) * inPenetrationAxis);

		// Calculate penetration depth^2 of each point and clamp against the minimal distance
		Vec3 v2 = ioContactPointsOn2[i];
		penetration_depth_sq.push_back(max(cMinDistanceSq, (v2 - v1).LengthSq()));
	}

	// Find the point that is furthest away from the center of mass (its torque will have the biggest influence)
	// and the point that has the deepest penetration depth. Use the heuristic (distance to center of mass) * (penetration depth) for this.
	uint point1 = 0;
	float val = max(cMinDistanceSq, projected[0].LengthSq()) * penetration_depth_sq[0];
	for (uint i = 0; i < projected.size(); ++i)
	{
		float v = max(cMinDistanceSq, projected[i].LengthSq()) * penetration_depth_sq[i];
		if (v > val)
		{
			val = v;
			point1 = i;
		}
	}
	Vec3 point1v = projected[point1];

	// Find point furthest from the first point forming a line segment with point1. Again combine this with the heuristic
	// for deepest point as per above.
	uint point2 = uint(-1);
	val = -FLT_MAX;
	for (uint i = 0; i < projected.size(); ++i)
		if (i != point1)
		{
			float v = max(cMinDistanceSq, (projected[i] - point1v).LengthSq()) * penetration_depth_sq[i];
			if (v > val)
			{
				val = v;
				point2 = i;
			}
		}
	JPH_ASSERT(point2 != uint(-1));
	Vec3 point2v = projected[point2];

	// Find furthest points on both sides of the line segment in order to maximize the area
	uint point3 = uint(-1);
	uint point4 = uint(-1);
	float min_val = 0.0f;
	float max_val = 0.0f;
	Vec3 perp = (point2v - point1v).Cross(inPenetrationAxis);
	for (uint i = 0; i < projected.size(); ++i)
		if (i != point1 && i != point2)
		{
			float v = perp.Dot(projected[i] - point1v);
			if (v < min_val)
			{
				min_val = v;
				point3 = i;
			}
			else if (v > max_val)
			{
				max_val = v;
				point4 = i;
			}
		}

	// Add points to array (in order so they form a polygon)
	StaticArray<Vec3, 4> points_to_keep_on_1, points_to_keep_on_2;
	points_to_keep_on_1.push_back(ioContactPointsOn1[point1]);
	points_to_keep_on_2.push_back(ioContactPointsOn2[point1]);
	if (point3 != uint(-1))
	{
		points_to_keep_on_1.push_back(ioContactPointsOn1[point3]);
		points_to_keep_on_2.push_back(ioContactPointsOn2[point3]);
	}
	points_to_keep_on_1.push_back(ioContactPointsOn1[point2]);
	points_to_keep_on_2.push_back(ioContactPointsOn2[point2]);
	if (point4 != uint(-1))
	{
		JPH_ASSERT(point3 != point4);
		points_to_keep_on_1.push_back(ioContactPointsOn1[point4]);
		points_to_keep_on_2.push_back(ioContactPointsOn2[point4]);
	}

#ifdef JPH_DEBUG_RENDERER
	if (ContactConstraintManager::sDrawContactPointReduction)
	{
		// Draw input polygon
		DebugRenderer::sInstance->DrawWirePolygon(RMat44::sTranslation(inCenterOfMass), ioContactPointsOn1, Color::sOrange, 0.05f);

		// Draw primary axis
		DebugRenderer::sInstance->DrawArrow(inCenterOfMass + ioContactPointsOn1[point1], inCenterOfMass + ioContactPointsOn1[point2], Color::sRed, 0.05f);

		// Draw contact points we kept
		for (Vec3 p : points_to_keep_on_1)
			DebugRenderer::sInstance->DrawMarker(inCenterOfMass + p, Color::sGreen, 0.1f);
	}
#endif // JPH_DEBUG_RENDERER

	// Copy the points back to the input buffer
	ioContactPointsOn1 = points_to_keep_on_1;
	ioContactPointsOn2 = points_to_keep_on_2;
}

void ManifoldBetweenTwoFaces(Vec3Arg inContactPoint1, Vec3Arg inContactPoint2, Vec3Arg inPenetrationAxis, float inMaxContactDistanceSq	, const ConvexShape::SupportingFace &inShape1Face, const ConvexShape::SupportingFace &inShape2Face, ContactPoints &outContactPoints1, ContactPoints &outContactPoints2 JPH_IF_DEBUG_RENDERER(, RVec3Arg inCenterOfMass))
{
#ifdef JPH_DEBUG_RENDERER
	if (ContactConstraintManager::sDrawContactPoint)
	{
		RVec3 cp1 = inCenterOfMass + inContactPoint1;
		RVec3 cp2 = inCenterOfMass + inContactPoint2;

		// Draw contact points
		DebugRenderer::sInstance->DrawMarker(cp1, Color::sRed, 0.1f);
		DebugRenderer::sInstance->DrawMarker(cp2, Color::sGreen, 0.1f);

		// Draw contact normal
		DebugRenderer::sInstance->DrawArrow(cp1, cp1 + inPenetrationAxis.Normalized(), Color::sRed, 0.05f);
	}
#endif // JPH_DEBUG_RENDERER

	// Remember size before adding new points, to check at the end if we added some
	ContactPoints::size_type old_size = outContactPoints1.size();

	// Check if both shapes have polygon faces
	if (inShape1Face.size() >= 2 // The dynamic shape needs to have at least 2 points or else there can never be more than 1 contact point
		&& inShape2Face.size() >= 3) // The dynamic/static shape needs to have at least 3 points (in the case that it has 2 points only if the edges match exactly you can have 2 contact points, but this situation is unstable anyhow)
	{
		// Clip the polygon of face 2 against that of 1
		ConvexShape::SupportingFace clipped_face;
		if (inShape1Face.size() >= 3)
			ClipPolyVsPoly(inShape2Face, inShape1Face, inPenetrationAxis, clipped_face);
		else if (inShape1Face.size() == 2)
			ClipPolyVsEdge(inShape2Face, inShape1Face[0], inShape1Face[1], inPenetrationAxis, clipped_face);

		// Project the points back onto the plane of shape 1 face and only keep those that are behind the plane
		Vec3 plane_origin = inShape1Face[0];
		Vec3 plane_normal;
		Vec3 first_edge = inShape1Face[1] - plane_origin;
		if (inShape1Face.size() >= 3)
		{
			// Three vertices, can just calculate the normal
			plane_normal = first_edge.Cross(inShape1Face[2] - plane_origin);
		}
		else
		{
			// Two vertices, first find a perpendicular to the edge and penetration axis and then use the perpendicular together with the edge to form a normal
			plane_normal = first_edge.Cross(inPenetrationAxis).Cross(first_edge);
		}

		// Check if the plane normal has any length, if not the clipped shape is so small that we'll just use the contact points
		float plane_normal_len_sq = plane_normal.LengthSq();
		if (plane_normal_len_sq > 0.0f)
		{
			// Discard points of faces that are too far away to collide
			for (Vec3 p2 : clipped_face)
			{
				float distance = (p2 - plane_origin).Dot(plane_normal); // Note should divide by length of plane_normal (unnormalized here)
				if (distance <= 0.0f || Square(distance) < inMaxContactDistanceSq * plane_normal_len_sq) // Must be close enough to plane, note we correct for not dividing by plane normal length here
				{
					// Project point back on shape 1 using the normal, note we correct for not dividing by plane normal length here:
					// p1 = p2 - (distance / sqrt(plane_normal_len_sq)) * (plane_normal / sqrt(plane_normal_len_sq));
					Vec3 p1 = p2 - (distance / plane_normal_len_sq) * plane_normal;

					outContactPoints1.push_back(p1);
					outContactPoints2.push_back(p2);
				}
			}
		}

	#ifdef JPH_DEBUG_RENDERER
		if (ContactConstraintManager::sDrawSupportingFaces)
		{
			RMat44 com = RMat44::sTranslation(inCenterOfMass);

			// Draw clipped poly
			DebugRenderer::sInstance->DrawWirePolygon(com, clipped_face, Color::sOrange);

			// Draw supporting faces
			DebugRenderer::sInstance->DrawWirePolygon(com, inShape1Face, Color::sRed, 0.05f);
			DebugRenderer::sInstance->DrawWirePolygon(com, inShape2Face, Color::sGreen, 0.05f);

			// Draw normal
			if (plane_normal_len_sq > 0.0f)
			{
				RVec3 plane_origin_ws = inCenterOfMass + plane_origin;
				DebugRenderer::sInstance->DrawArrow(plane_origin_ws, plane_origin_ws + plane_normal / sqrt(plane_normal_len_sq), Color::sYellow, 0.05f);
			}

			// Draw contact points that remain after distance check
			for (ContactPoints::size_type p = old_size; p < outContactPoints1.size(); ++p)
				DebugRenderer::sInstance->DrawMarker(inCenterOfMass + outContactPoints1[p], Color::sYellow, 0.1f);
		}
	#endif // JPH_DEBUG_RENDERER
	}

	// If the clipping result is empty, use the contact point itself
	if (outContactPoints1.size() == old_size)
	{
		outContactPoints1.push_back(inContactPoint1);
		outContactPoints2.push_back(inContactPoint2);
	}
}

JPH_NAMESPACE_END

// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Jolt/Geometry/AABox.h>

JPH_NAMESPACE_BEGIN

/// Clip inPolygonToClip against the positive halfspace of plane defined by inPlaneOrigin and inPlaneNormal.
/// inPlaneNormal does not need to be normalized.
template <class VERTEX_ARRAY>
void ClipPolyVsPlane(const VERTEX_ARRAY &inPolygonToClip, Vec3Arg inPlaneOrigin, Vec3Arg inPlaneNormal, VERTEX_ARRAY &outClippedPolygon)
{
	JPH_ASSERT(inPolygonToClip.size() >= 2);
	JPH_ASSERT(outClippedPolygon.empty());

	// Determine state of last point
	Vec3 e1 = inPolygonToClip[inPolygonToClip.size() - 1];
	float prev_num = (inPlaneOrigin - e1).Dot(inPlaneNormal);
	bool prev_inside = prev_num < 0.0f;

	// Loop through all vertices
	for (typename VERTEX_ARRAY::size_type j = 0; j < inPolygonToClip.size(); ++j)
	{
		// Check if second point is inside
		Vec3Arg e2 = inPolygonToClip[j];
		float num = (inPlaneOrigin - e2).Dot(inPlaneNormal);
		bool cur_inside = num < 0.0f;

		// In -> Out or Out -> In: Add point on clipping plane
		if (cur_inside != prev_inside)
		{
			// Solve: (X - inPlaneOrigin) . inPlaneNormal = 0 and X = e1 + t * (e2 - e1) for X
			Vec3 e12 = e2 - e1;
			float denom = e12.Dot(inPlaneNormal);
			if (denom != 0.0f)
				outClippedPolygon.push_back(e1 + (prev_num / denom) * e12);
			else
				cur_inside = prev_inside; // Edge is parallel to plane, treat point as if it were on the same side as the last point
		}

		// Point inside, add it
		if (cur_inside)
			outClippedPolygon.push_back(e2);

		// Update previous state
		prev_num = num;
		prev_inside = cur_inside;
		e1 = e2;
	}
}

/// Clip polygon versus polygon.
/// Both polygons are assumed to be in counter clockwise order.
/// @param inClippingPolygonNormal is used to create planes of all edges in inClippingPolygon against which inPolygonToClip is clipped, inClippingPolygonNormal does not need to be normalized
/// @param inClippingPolygon is the polygon which inClippedPolygon is clipped against
/// @param inPolygonToClip is the polygon that is clipped
/// @param outClippedPolygon will contain clipped polygon when function returns
template <class VERTEX_ARRAY>
void ClipPolyVsPoly(const VERTEX_ARRAY &inPolygonToClip, const VERTEX_ARRAY &inClippingPolygon, Vec3Arg inClippingPolygonNormal, VERTEX_ARRAY &outClippedPolygon)
{
	JPH_ASSERT(inPolygonToClip.size() >= 2);
	JPH_ASSERT(inClippingPolygon.size() >= 3);

	VERTEX_ARRAY tmp_vertices[2];
	int tmp_vertices_idx = 0;

	for (typename VERTEX_ARRAY::size_type i = 0; i < inClippingPolygon.size(); ++i)
	{
		// Get edge to clip against
		Vec3 clip_e1 = inClippingPolygon[i];
		Vec3 clip_e2 = inClippingPolygon[(i + 1) % inClippingPolygon.size()];
		Vec3 clip_normal = inClippingPolygonNormal.Cross(clip_e2 - clip_e1); // Pointing inward to the clipping polygon

		// Get source and target polygon
		const VERTEX_ARRAY &src_polygon = (i == 0)? inPolygonToClip : tmp_vertices[tmp_vertices_idx];
		tmp_vertices_idx ^= 1;
		VERTEX_ARRAY &tgt_polygon = (i == inClippingPolygon.size() - 1)? outClippedPolygon : tmp_vertices[tmp_vertices_idx];
		tgt_polygon.clear();

		// Clip against the edge
		ClipPolyVsPlane(src_polygon, clip_e1, clip_normal, tgt_polygon);

		// Break out if no polygon left
		if (tgt_polygon.size() < 3)
		{
			outClippedPolygon.clear();
			break;
		}
	}
}

/// Clip inPolygonToClip against an edge, the edge is projected on inPolygonToClip using inClippingEdgeNormal.
/// The positive half space (the side on the edge in the direction of inClippingEdgeNormal) is cut away.
template <class VERTEX_ARRAY>
void ClipPolyVsEdge(const VERTEX_ARRAY &inPolygonToClip, Vec3Arg inEdgeVertex1, Vec3Arg inEdgeVertex2, Vec3Arg inClippingEdgeNormal, VERTEX_ARRAY &outClippedPolygon)
{
	JPH_ASSERT(inPolygonToClip.size() >= 3);
	JPH_ASSERT(outClippedPolygon.empty());

	// Get normal that is perpendicular to the edge and the clipping edge normal
	Vec3 edge = inEdgeVertex2 - inEdgeVertex1;
	Vec3 edge_normal = inClippingEdgeNormal.Cross(edge);

	// Project vertices of edge on inPolygonToClip
	Vec3 polygon_normal = (inPolygonToClip[2] - inPolygonToClip[0]).Cross(inPolygonToClip[1] - inPolygonToClip[0]);
	float polygon_normal_len_sq = polygon_normal.LengthSq();
	Vec3 v1 = inEdgeVertex1 + polygon_normal.Dot(inPolygonToClip[0] - inEdgeVertex1) * polygon_normal / polygon_normal_len_sq;
	Vec3 v2 = inEdgeVertex2 + polygon_normal.Dot(inPolygonToClip[0] - inEdgeVertex2) * polygon_normal / polygon_normal_len_sq;
	Vec3 v12 = v2 - v1;
	float v12_len_sq = v12.LengthSq();

	// Determine state of last point
	Vec3 e1 = inPolygonToClip[inPolygonToClip.size() - 1];
	float prev_num = (inEdgeVertex1 - e1).Dot(edge_normal);
	bool prev_inside = prev_num < 0.0f;

	// Loop through all vertices
	for (typename VERTEX_ARRAY::size_type j = 0; j < inPolygonToClip.size(); ++j)
	{
		// Check if second point is inside
		Vec3 e2 = inPolygonToClip[j];
		float num = (inEdgeVertex1 - e2).Dot(edge_normal);
		bool cur_inside = num < 0.0f;

		// In -> Out or Out -> In: Add point on clipping plane
		if (cur_inside != prev_inside)
		{
			// Solve: (inEdgeVertex1 - X) . edge_normal = 0 and X = e1 + t * (e2 - e1) for X
			Vec3 e12 = e2 - e1;
			float denom = e12.Dot(edge_normal);
			Vec3 clipped_point = denom != 0.0f? e1 + (prev_num / denom) * e12 : e1;

			// Project point on line segment v1, v2 so see if it falls outside if the edge
			float projection = (clipped_point - v1).Dot(v12);
			if (projection < 0.0f)
				outClippedPolygon.push_back(v1);
			else if (projection > v12_len_sq)
				outClippedPolygon.push_back(v2);
			else
				outClippedPolygon.push_back(clipped_point);
		}

		// Update previous state
		prev_num = num;
		prev_inside = cur_inside;
		e1 = e2;
	}
}

/// Clip polygon vs axis aligned box, inPolygonToClip is assume to be in counter clockwise order.
/// Output will be stored in outClippedPolygon. Everything inside inAABox will be kept.
template <class VERTEX_ARRAY>
void ClipPolyVsAABox(const VERTEX_ARRAY &inPolygonToClip, const AABox &inAABox, VERTEX_ARRAY &outClippedPolygon)
{
	JPH_ASSERT(inPolygonToClip.size() >= 2);

	VERTEX_ARRAY tmp_vertices[2];
	int tmp_vertices_idx = 0;

	for (int coord = 0; coord < 3; ++coord)
		for (int side = 0; side < 2; ++side)
		{
			// Get plane to clip against
			Vec3 origin = Vec3::sZero(), normal = Vec3::sZero();
			if (side == 0)
			{
				normal.SetComponent(coord, 1.0f);
				origin.SetComponent(coord, inAABox.mMin[coord]);
			}
			else
			{
				normal.SetComponent(coord, -1.0f);
				origin.SetComponent(coord, inAABox.mMax[coord]);
			}

			// Get source and target polygon
			const VERTEX_ARRAY &src_polygon = tmp_vertices_idx == 0? inPolygonToClip : tmp_vertices[tmp_vertices_idx & 1];
			tmp_vertices_idx++;
			VERTEX_ARRAY &tgt_polygon = tmp_vertices_idx == 6? outClippedPolygon : tmp_vertices[tmp_vertices_idx & 1];
			tgt_polygon.clear();

			// Clip against the edge
			ClipPolyVsPlane(src_polygon, origin, normal, tgt_polygon);

			// Break out if no polygon left
			if (tgt_polygon.size() < 3)
			{
				outClippedPolygon.clear();
				return;
			}

			// Flip normal
			normal = -normal;
		}
}

JPH_NAMESPACE_END

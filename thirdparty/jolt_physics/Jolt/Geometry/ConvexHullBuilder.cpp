// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#include <Jolt/Jolt.h>

#include <Jolt/Geometry/ConvexHullBuilder.h>
#include <Jolt/Geometry/ConvexHullBuilder2D.h>
#include <Jolt/Geometry/ClosestPoint.h>
#include <Jolt/Core/StringTools.h>
#include <Jolt/Core/UnorderedSet.h>

#ifdef JPH_CONVEX_BUILDER_DUMP_SHAPE
JPH_SUPPRESS_WARNINGS_STD_BEGIN
#include <fstream>
JPH_SUPPRESS_WARNINGS_STD_END
#endif // JPH_CONVEX_BUILDER_DUMP_SHAPE

#ifdef JPH_CONVEX_BUILDER_DEBUG
	#include <Jolt/Renderer/DebugRenderer.h>
#endif

JPH_NAMESPACE_BEGIN

ConvexHullBuilder::Face::~Face()
{
	// Free all edges
	Edge *e = mFirstEdge;
	if (e != nullptr)
	{
		do
		{
			Edge *next = e->mNextEdge;
			delete e;
			e = next;
		} while (e != mFirstEdge);
	}
}

void ConvexHullBuilder::Face::CalculateNormalAndCentroid(const Vec3 *inPositions)
{
	// Get point that we use to construct a triangle fan
	Edge *e = mFirstEdge;
	Vec3 y0 = inPositions[e->mStartIdx];

	// Get the 2nd point
	e = e->mNextEdge;
	Vec3 y1 = inPositions[e->mStartIdx];

	// Start accumulating the centroid
	mCentroid = y0 + y1;
	int n = 2;

	// Start accumulating the normal
	mNormal = Vec3::sZero();

	// Loop over remaining edges accumulating normals in a triangle fan fashion
	for (e = e->mNextEdge; e != mFirstEdge; e = e->mNextEdge)
	{
		// Get the 3rd point
		Vec3 y2 = inPositions[e->mStartIdx];

		// Calculate edges (counter clockwise)
		Vec3 e0 = y1 - y0;
		Vec3 e1 = y2 - y1;
		Vec3 e2 = y0 - y2;

		// The best normal is calculated by using the two shortest edges
		// See: https://box2d.org/posts/2014/01/troublesome-triangle/
		// The difference in normals is most pronounced when one edge is much smaller than the others (in which case the others must have roughly the same length).
		// Therefore we can suffice by just picking the shortest from 2 edges and use that with the 3rd edge to calculate the normal.
		// We first check which of the edges is shorter: e1 or e2
		UVec4 e1_shorter_than_e2 = Vec4::sLess(e1.DotV4(e1), e2.DotV4(e2));

		// We calculate both normals and then select the one that had the shortest edge for our normal (this avoids branching)
		Vec3 normal_e01 = e0.Cross(e1);
		Vec3 normal_e02 = e2.Cross(e0);
		mNormal += Vec3::sSelect(normal_e02, normal_e01, e1_shorter_than_e2);

		// Accumulate centroid
		mCentroid += y2;
		n++;

		// Update y1 for next triangle
		y1 = y2;
	}

	// Finalize centroid
	mCentroid /= float(n);
}

void ConvexHullBuilder::Face::Initialize(int inIdx0, int inIdx1, int inIdx2, const Vec3 *inPositions)
{
	JPH_ASSERT(mFirstEdge == nullptr);
	JPH_ASSERT(inIdx0 != inIdx1 && inIdx0 != inIdx2 && inIdx1 != inIdx2);

	// Create 3 edges
	Edge *e0 = new Edge(this, inIdx0);
	Edge *e1 = new Edge(this, inIdx1);
	Edge *e2 = new Edge(this, inIdx2);

	// Link edges
	e0->mNextEdge = e1;
	e1->mNextEdge = e2;
	e2->mNextEdge = e0;
	mFirstEdge = e0;

	CalculateNormalAndCentroid(inPositions);
}

ConvexHullBuilder::ConvexHullBuilder(const Positions &inPositions) :
	mPositions(inPositions)
{
#ifdef JPH_CONVEX_BUILDER_DEBUG
	mIteration = 0;

	// Center the drawing of the first hull around the origin and calculate the delta offset between states
	mOffset = RVec3::sZero();
	if (mPositions.empty())
	{
		// No hull will be generated
		mDelta = Vec3::sZero();
	}
	else
	{
		Vec3 maxv = Vec3::sReplicate(-FLT_MAX), minv = Vec3::sReplicate(FLT_MAX);
		for (Vec3 v : mPositions)
		{
			minv = Vec3::sMin(minv, v);
			maxv = Vec3::sMax(maxv, v);
			mOffset -= v;
		}
		mOffset /= Real(mPositions.size());
		mDelta = Vec3((maxv - minv).GetX() + 0.5f, 0, 0);
		mOffset += mDelta; // Don't start at origin, we're already drawing the final hull there
	}
#endif
}

void ConvexHullBuilder::FreeFaces()
{
	for (Face *f : mFaces)
		delete f;
	mFaces.clear();
}

void ConvexHullBuilder::GetFaceForPoint(Vec3Arg inPoint, const Faces &inFaces, Face *&outFace, float &outDistSq) const
{
	outFace = nullptr;
	outDistSq = 0.0f;

	for (Face *f : inFaces)
		if (!f->mRemoved)
		{
			// Determine distance to face
			float dot = f->mNormal.Dot(inPoint - f->mCentroid);
			if (dot > 0.0f)
			{
				float dist_sq = dot * dot / f->mNormal.LengthSq();
				if (dist_sq > outDistSq)
				{
					outFace = f;
					outDistSq = dist_sq;
				}
			}
		}
}

float ConvexHullBuilder::GetDistanceToEdgeSq(Vec3Arg inPoint, const Face *inFace) const
{
	bool all_inside = true;
	float edge_dist_sq = FLT_MAX;

	// Test if it is inside the edges of the polygon
	Edge *edge = inFace->mFirstEdge;
	Vec3 p1 = mPositions[edge->GetPreviousEdge()->mStartIdx];
	do
	{
		Vec3 p2 = mPositions[edge->mStartIdx];
		if ((p2 - p1).Cross(inPoint - p1).Dot(inFace->mNormal) < 0.0f)
		{
			// It is outside
			all_inside = false;

			// Measure distance to this edge
			uint32 s;
			edge_dist_sq = min(edge_dist_sq, ClosestPoint::GetClosestPointOnLine(p1 - inPoint, p2 - inPoint, s).LengthSq());
		}
		p1 = p2;
		edge = edge->mNextEdge;
	} while (edge != inFace->mFirstEdge);

	return all_inside? 0.0f : edge_dist_sq;
}

bool ConvexHullBuilder::AssignPointToFace(int inPositionIdx, const Faces &inFaces, float inToleranceSq)
{
	Vec3 point = mPositions[inPositionIdx];

	// Find the face for which the point is furthest away
	Face *best_face;
	float best_dist_sq;
	GetFaceForPoint(point, inFaces, best_face, best_dist_sq);

	if (best_face != nullptr)
	{
		// Check if this point is within the tolerance margin to the plane
		if (best_dist_sq <= inToleranceSq)
		{
			// Check distance to edges
			float dist_to_edge_sq = GetDistanceToEdgeSq(point, best_face);
			if (dist_to_edge_sq > inToleranceSq)
			{
				// Point is outside of the face and too far away to discard
				mCoplanarList.push_back({ inPositionIdx, dist_to_edge_sq });
			}
		}
		else
		{
			// This point is in front of the face, add it to the conflict list
			if (best_dist_sq > best_face->mFurthestPointDistanceSq)
			{
				// This point is further away than any others, update the distance and add point as last point
				best_face->mFurthestPointDistanceSq = best_dist_sq;
				best_face->mConflictList.push_back(inPositionIdx);
			}
			else
			{
				// Not the furthest point, add it as the before last point
				best_face->mConflictList.insert(best_face->mConflictList.begin() + best_face->mConflictList.size() - 1, inPositionIdx);
			}

			return true;
		}
	}

	return false;
}

float ConvexHullBuilder::DetermineCoplanarDistance() const
{
	// Formula as per: Implementing Quickhull - Dirk Gregorius.
	Vec3 vmax = Vec3::sZero();
	for (Vec3 v : mPositions)
		vmax = Vec3::sMax(vmax, v.Abs());
	return 3.0f * FLT_EPSILON * (vmax.GetX() + vmax.GetY() + vmax.GetZ());
}

int ConvexHullBuilder::GetNumVerticesUsed() const
{
	UnorderedSet<int> used_verts;
	used_verts.reserve(UnorderedSet<int>::size_type(mPositions.size()));
	for (Face *f : mFaces)
	{
		Edge *e = f->mFirstEdge;
		do
		{
			used_verts.insert(e->mStartIdx);
			e = e->mNextEdge;
		} while (e != f->mFirstEdge);
	}
	return (int)used_verts.size();
}

bool ConvexHullBuilder::ContainsFace(const Array<int> &inIndices) const
{
	for (Face *f : mFaces)
	{
		Edge *e = f->mFirstEdge;
		Array<int>::const_iterator index = std::find(inIndices.begin(), inIndices.end(), e->mStartIdx);
		if (index != inIndices.end())
		{
			size_t matches = 0;

			do
			{
				// Check if index matches
				if (*index != e->mStartIdx)
					break;

				// Increment number of matches
				matches++;

				// Next index in list of inIndices
				index++;
				if (index == inIndices.end())
					index = inIndices.begin();

				// Next edge
				e = e->mNextEdge;
			} while (e != f->mFirstEdge);

			if (matches == inIndices.size())
				return true;
		}
	}

	return false;
}

ConvexHullBuilder::EResult ConvexHullBuilder::Initialize(int inMaxVertices, float inTolerance, const char *&outError)
{
	// Free the faces possibly left over from an earlier hull
	FreeFaces();

	// Test that we have at least 3 points
	if (mPositions.size() < 3)
	{
		outError = "Need at least 3 points to make a hull";
		return EResult::TooFewPoints;
	}

	// Determine a suitable tolerance for detecting that points are coplanar
	float coplanar_tolerance_sq = Square(DetermineCoplanarDistance());

	// Increase desired tolerance if accuracy doesn't allow it
	float tolerance_sq = max(coplanar_tolerance_sq, Square(inTolerance));

	// Find point furthest from the origin
	int idx1 = -1;
	float max_dist_sq = -1.0f;
	for (int i = 0; i < (int)mPositions.size(); ++i)
	{
		float dist_sq = mPositions[i].LengthSq();
		if (dist_sq > max_dist_sq)
		{
			max_dist_sq = dist_sq;
			idx1 = i;
		}
	}
	JPH_ASSERT(idx1 >= 0);

	// Find point that is furthest away from this point
	int idx2 = -1;
	max_dist_sq = -1.0f;
	for (int i = 0; i < (int)mPositions.size(); ++i)
		if (i != idx1)
		{
			float dist_sq = (mPositions[i] - mPositions[idx1]).LengthSq();
			if (dist_sq > max_dist_sq)
			{
				max_dist_sq = dist_sq;
				idx2 = i;
			}
		}
	JPH_ASSERT(idx2 >= 0);

	// Find point that forms the biggest triangle
	int idx3 = -1;
	float best_triangle_area_sq = -1.0f;
	for (int i = 0; i < (int)mPositions.size(); ++i)
		if (i != idx1 && i != idx2)
		{
			float triangle_area_sq = (mPositions[idx1] - mPositions[i]).Cross(mPositions[idx2] - mPositions[i]).LengthSq();
			if (triangle_area_sq > best_triangle_area_sq)
			{
				best_triangle_area_sq = triangle_area_sq;
				idx3 = i;
			}
		}
	JPH_ASSERT(idx3 >= 0);
	if (best_triangle_area_sq < cMinTriangleAreaSq)
	{
		outError = "Could not find a suitable initial triangle because its area was too small";
		return EResult::Degenerate;
	}

	// Check if we have only 3 vertices
	if (mPositions.size() == 3)
	{
		// Create two triangles (back to back)
		Face *t1 = CreateTriangle(idx1, idx2, idx3);
		Face *t2 = CreateTriangle(idx1, idx3, idx2);

		// Link faces edges
		sLinkFace(t1->mFirstEdge, t2->mFirstEdge->mNextEdge->mNextEdge);
		sLinkFace(t1->mFirstEdge->mNextEdge, t2->mFirstEdge->mNextEdge);
		sLinkFace(t1->mFirstEdge->mNextEdge->mNextEdge, t2->mFirstEdge);

#ifdef JPH_CONVEX_BUILDER_DEBUG
		// Draw current state
		DrawState();
#endif

		return EResult::Success;
	}

	// Find point that forms the biggest tetrahedron
	Vec3 initial_plane_normal = (mPositions[idx2] - mPositions[idx1]).Cross(mPositions[idx3] - mPositions[idx1]).Normalized();
	Vec3 initial_plane_centroid = (mPositions[idx1] + mPositions[idx2] + mPositions[idx3]) / 3.0f;
	int idx4 = -1;
	float max_dist = 0.0f;
	for (int i = 0; i < (int)mPositions.size(); ++i)
		if (i != idx1 && i != idx2 && i != idx3)
		{
			float dist = (mPositions[i] - initial_plane_centroid).Dot(initial_plane_normal);
			if (abs(dist) > abs(max_dist))
			{
				max_dist = dist;
				idx4 = i;
			}
		}

	// Check if the hull is coplanar
	if (Square(max_dist) <= 25.0f * coplanar_tolerance_sq)
	{
		// First project all points in 2D space
		Vec3 base1 = initial_plane_normal.GetNormalizedPerpendicular();
		Vec3 base2 = initial_plane_normal.Cross(base1);
		Array<Vec3> positions_2d;
		positions_2d.reserve(mPositions.size());
		for (Vec3 v : mPositions)
			positions_2d.emplace_back(base1.Dot(v), base2.Dot(v), 0.0f);

		// Build hull
		Array<int> edges_2d;
		ConvexHullBuilder2D builder_2d(positions_2d);
		ConvexHullBuilder2D::EResult result = builder_2d.Initialize(idx1, idx2, idx3, inMaxVertices, inTolerance, edges_2d);

		// Create faces (back to back)
		Face *f1 = CreateFace();
		Face *f2 = CreateFace();

		// Create edges for face 1
		Array<Edge *> edges_f1;
		edges_f1.reserve(edges_2d.size());
		for (int start_idx : edges_2d)
		{
			Edge *edge = new Edge(f1, start_idx);
			if (edges_f1.empty())
				f1->mFirstEdge = edge;
			else
				edges_f1.back()->mNextEdge = edge;
			edges_f1.push_back(edge);
		}
		edges_f1.back()->mNextEdge = f1->mFirstEdge;

		// Create edges for face 2
		Array<Edge *> edges_f2;
		edges_f2.reserve(edges_2d.size());
		for (int i = (int)edges_2d.size() - 1; i >= 0; --i)
		{
			Edge *edge = new Edge(f2, edges_2d[i]);
			if (edges_f2.empty())
				f2->mFirstEdge = edge;
			else
				edges_f2.back()->mNextEdge = edge;
			edges_f2.push_back(edge);
		}
		edges_f2.back()->mNextEdge = f2->mFirstEdge;

		// Link edges
		for (size_t i = 0; i < edges_2d.size(); ++i)
			sLinkFace(edges_f1[i], edges_f2[(2 * edges_2d.size() - 2 - i) % edges_2d.size()]);

		// Calculate the plane for both faces
		f1->CalculateNormalAndCentroid(mPositions.data());
		f2->mNormal = -f1->mNormal;
		f2->mCentroid = f1->mCentroid;

#ifdef JPH_CONVEX_BUILDER_DEBUG
		// Draw current state
		DrawState();
#endif

		return result == ConvexHullBuilder2D::EResult::MaxVerticesReached? EResult::MaxVerticesReached : EResult::Success;
	}

	// Ensure the planes are facing outwards
	if (max_dist < 0.0f)
		std::swap(idx2, idx3);

	// Create tetrahedron
	Face *t1 = CreateTriangle(idx1, idx2, idx4);
	Face *t2 = CreateTriangle(idx2, idx3, idx4);
	Face *t3 = CreateTriangle(idx3, idx1, idx4);
	Face *t4 = CreateTriangle(idx1, idx3, idx2);

	// Link face edges
	sLinkFace(t1->mFirstEdge, t4->mFirstEdge->mNextEdge->mNextEdge);
	sLinkFace(t1->mFirstEdge->mNextEdge, t2->mFirstEdge->mNextEdge->mNextEdge);
	sLinkFace(t1->mFirstEdge->mNextEdge->mNextEdge, t3->mFirstEdge->mNextEdge);
	sLinkFace(t2->mFirstEdge, t4->mFirstEdge->mNextEdge);
	sLinkFace(t2->mFirstEdge->mNextEdge, t3->mFirstEdge->mNextEdge->mNextEdge);
	sLinkFace(t3->mFirstEdge, t4->mFirstEdge);

	// Build the initial conflict lists
	Faces faces { t1, t2, t3, t4 };
	for (int idx = 0; idx < (int)mPositions.size(); ++idx)
		if (idx != idx1 && idx != idx2 && idx != idx3 && idx != idx4)
			AssignPointToFace(idx, faces, tolerance_sq);

#ifdef JPH_CONVEX_BUILDER_DEBUG
	// Draw current state including conflict list
	DrawState(true);

	// Increment iteration counter
	++mIteration;
#endif

	// Overestimate of the actual amount of vertices we use, for limiting the amount of vertices in the hull
	int num_vertices_used = 4;

	// Loop through the remainder of the points and add them
	for (;;)
	{
		// Find the face with the furthest point on it
		Face *face_with_furthest_point = nullptr;
		float furthest_dist_sq = 0.0f;
		for (Face *f : mFaces)
			if (f->mFurthestPointDistanceSq > furthest_dist_sq)
			{
				furthest_dist_sq = f->mFurthestPointDistanceSq;
				face_with_furthest_point = f;
			}

		int furthest_point_idx;
		if (face_with_furthest_point != nullptr)
		{
			// Take the furthest point
			furthest_point_idx = face_with_furthest_point->mConflictList.back();
			face_with_furthest_point->mConflictList.pop_back();
		}
		else if (!mCoplanarList.empty())
		{
			// Try to assign points to faces (this also recalculates the distance to the hull for the coplanar vertices)
			CoplanarList coplanar;
			mCoplanarList.swap(coplanar);
			bool added = false;
			for (const Coplanar &c : coplanar)
				added |= AssignPointToFace(c.mPositionIdx, mFaces, tolerance_sq);

			// If we were able to assign a point, loop again to pick it up
			if (added)
				continue;

			// If the coplanar list is empty, there are no points left and we're done
			if (mCoplanarList.empty())
				break;

			do
			{
				// Find the vertex that is furthest from the hull
				CoplanarList::size_type best_idx = 0;
				float best_dist_sq = mCoplanarList.front().mDistanceSq;
				for (CoplanarList::size_type idx = 1; idx < mCoplanarList.size(); ++idx)
				{
					const Coplanar &c = mCoplanarList[idx];
					if (c.mDistanceSq > best_dist_sq)
					{
						best_idx = idx;
						best_dist_sq = c.mDistanceSq;
					}
				}

				// Swap it to the end
				std::swap(mCoplanarList[best_idx], mCoplanarList.back());

				// Remove it
				furthest_point_idx = mCoplanarList.back().mPositionIdx;
				mCoplanarList.pop_back();

				// Find the face for which the point is furthest away
				GetFaceForPoint(mPositions[furthest_point_idx], mFaces, face_with_furthest_point, best_dist_sq);
			} while (!mCoplanarList.empty() && face_with_furthest_point == nullptr);

			if (face_with_furthest_point == nullptr)
				break;
		}
		else
		{
			// If there are no more vertices, we're done
			break;
		}

		// Check if we have a limit on the max vertices that we should produce
		if (num_vertices_used >= inMaxVertices)
		{
			// Count the actual amount of used vertices (we did not take the removal of any vertices into account)
			num_vertices_used = GetNumVerticesUsed();

			// Check if there are too many
			if (num_vertices_used >= inMaxVertices)
				return EResult::MaxVerticesReached;
		}

		// We're about to add another vertex
		++num_vertices_used;

		// Add the point to the hull
		Faces new_faces;
		AddPoint(face_with_furthest_point, furthest_point_idx, coplanar_tolerance_sq, new_faces);

		// Redistribute points on conflict lists belonging to removed faces
		for (const Face *face : mFaces)
			if (face->mRemoved)
				for (int idx : face->mConflictList)
					AssignPointToFace(idx, new_faces, tolerance_sq);

		// Permanently delete faces that we removed in AddPoint()
		GarbageCollectFaces();

#ifdef JPH_CONVEX_BUILDER_DEBUG
		// Draw state at the end of this step including conflict list
		DrawState(true);

		// Increment iteration counter
		++mIteration;
#endif
	}

	// Check if we are left with a hull. It is possible that hull building fails if the points are nearly coplanar.
	if (mFaces.size() < 2)
	{
		outError = "Too few faces in hull";
		return EResult::TooFewFaces;
	}

	return EResult::Success;
}

void ConvexHullBuilder::AddPoint(Face *inFacingFace, int inIdx, float inCoplanarToleranceSq, Faces &outNewFaces)
{
	// Get position
	Vec3 pos = mPositions[inIdx];

#ifdef JPH_CONVEX_BUILDER_DEBUG
	// Draw point to be added
	DebugRenderer::sInstance->DrawMarker(cDrawScale * (mOffset + pos), Color::sYellow, 0.1f);
	DebugRenderer::sInstance->DrawText3D(cDrawScale * (mOffset + pos), ConvertToString(inIdx), Color::sWhite);
#endif

#ifdef JPH_ENABLE_ASSERTS
	// Check if structure is intact
	ValidateFaces();
#endif

	// Find edge of convex hull of faces that are not facing the new vertex
	FullEdges edges;
	FindEdge(inFacingFace, pos, edges);
	JPH_ASSERT(edges.size() >= 3);

	// Create new faces
	outNewFaces.reserve(edges.size());
	for (const FullEdge &e : edges)
	{
		JPH_ASSERT(e.mStartIdx != e.mEndIdx);
		Face *f = CreateTriangle(e.mStartIdx, e.mEndIdx, inIdx);
		outNewFaces.push_back(f);
	}

	// Link edges
	for (Faces::size_type i = 0; i < outNewFaces.size(); ++i)
	{
		sLinkFace(outNewFaces[i]->mFirstEdge, edges[i].mNeighbourEdge);
		sLinkFace(outNewFaces[i]->mFirstEdge->mNextEdge, outNewFaces[(i + 1) % outNewFaces.size()]->mFirstEdge->mNextEdge->mNextEdge);
	}

	// Loop on faces that were modified until nothing needs to be checked anymore
	Faces affected_faces = outNewFaces;
	while (!affected_faces.empty())
	{
		// Take the next face
		Face *face = affected_faces.back();
		affected_faces.pop_back();

		if (!face->mRemoved)
		{
			// Merge with neighbour if this is a degenerate face
			MergeDegenerateFace(face, affected_faces);

			// Merge with coplanar neighbours (or when the neighbour forms a concave edge)
			if (!face->mRemoved)
				MergeCoplanarOrConcaveFaces(face, inCoplanarToleranceSq, affected_faces);
		}
	}

#ifdef JPH_ENABLE_ASSERTS
	// Check if structure is intact
	ValidateFaces();
#endif
}

void ConvexHullBuilder::GarbageCollectFaces()
{
	for (int i = (int)mFaces.size() - 1; i >= 0; --i)
	{
		Face *f = mFaces[i];
		if (f->mRemoved)
		{
			FreeFace(f);
			mFaces.erase(mFaces.begin() + i);
		}
	}
}

ConvexHullBuilder::Face *ConvexHullBuilder::CreateFace()
{
	// Call provider to create face
	Face *f = new Face();

#ifdef JPH_CONVEX_BUILDER_DEBUG
	// Remember iteration counter
	f->mIteration = mIteration;
#endif

	// Add to list
	mFaces.push_back(f);
	return f;
}

ConvexHullBuilder::Face *ConvexHullBuilder::CreateTriangle(int inIdx1, int inIdx2, int inIdx3)
{
	Face *f = CreateFace();
	f->Initialize(inIdx1, inIdx2, inIdx3, mPositions.data());
	return f;
}

void ConvexHullBuilder::FreeFace(Face *inFace)
{
	JPH_ASSERT(inFace->mRemoved);

#ifdef JPH_ENABLE_ASSERTS
	// Make sure that this face is not connected
	Edge *e = inFace->mFirstEdge;
	if (e != nullptr)
		do
		{
			JPH_ASSERT(e->mNeighbourEdge == nullptr);
			e = e->mNextEdge;
		} while (e != inFace->mFirstEdge);
#endif

	// Free the face
	delete inFace;
}

void ConvexHullBuilder::sLinkFace(Edge *inEdge1, Edge *inEdge2)
{
	// Check not connected yet
	JPH_ASSERT(inEdge1->mNeighbourEdge == nullptr);
	JPH_ASSERT(inEdge2->mNeighbourEdge == nullptr);
	JPH_ASSERT(inEdge1->mFace != inEdge2->mFace);

	// Check vertices match
	JPH_ASSERT(inEdge1->mStartIdx == inEdge2->mNextEdge->mStartIdx);
	JPH_ASSERT(inEdge2->mStartIdx == inEdge1->mNextEdge->mStartIdx);

	// Link up
	inEdge1->mNeighbourEdge = inEdge2;
	inEdge2->mNeighbourEdge = inEdge1;
}

void ConvexHullBuilder::sUnlinkFace(Face *inFace)
{
	// Unlink from neighbours
	Edge *e = inFace->mFirstEdge;
	do
	{
		if (e->mNeighbourEdge != nullptr)
		{
			// Validate that neighbour points to us
			JPH_ASSERT(e->mNeighbourEdge->mNeighbourEdge == e);

			// Unlink
			e->mNeighbourEdge->mNeighbourEdge = nullptr;
			e->mNeighbourEdge = nullptr;
		}
		e = e->mNextEdge;
	} while (e != inFace->mFirstEdge);
}

void ConvexHullBuilder::FindEdge(Face *inFacingFace, Vec3Arg inVertex, FullEdges &outEdges) const
{
	// Assert that we were given an empty array
	JPH_ASSERT(outEdges.empty());

	// Should start with a facing face
	JPH_ASSERT(inFacingFace->IsFacing(inVertex));

	// Flag as removed
	inFacingFace->mRemoved = true;

	// Instead of recursing, we build our own stack with the information we need
	struct StackEntry
	{
		Edge *		mFirstEdge;
		Edge *		mCurrentEdge;
	};
	constexpr int cMaxEdgeLength = 128;
	StackEntry stack[cMaxEdgeLength];
	int cur_stack_pos = 0;

	static_assert(alignof(Edge) >= 2, "Need lowest bit to indicate to tell if we completed the loop");

	// Start with the face / edge provided
	stack[0].mFirstEdge = inFacingFace->mFirstEdge;
	stack[0].mCurrentEdge = reinterpret_cast<Edge *>(reinterpret_cast<uintptr_t>(inFacingFace->mFirstEdge) | 1); // Set lowest bit of pointer to make it different from the first edge

	for (;;)
	{
		StackEntry &cur_entry = stack[cur_stack_pos];

		// Next edge
		Edge *raw_e = cur_entry.mCurrentEdge;
		Edge *e = reinterpret_cast<Edge *>(reinterpret_cast<uintptr_t>(raw_e) & ~uintptr_t(1)); // Remove the lowest bit which was used to indicate that this is the first edge we're testing
		cur_entry.mCurrentEdge = e->mNextEdge;

		// If we're back at the first edge we've completed the face and we're done
		if (raw_e == cur_entry.mFirstEdge)
		{
			// This face needs to be removed, unlink it now, caller will free
			sUnlinkFace(e->mFace);

			// Pop from stack
			if (--cur_stack_pos < 0)
				break;
		}
		else
		{
			// Visit neighbour face
			Edge *ne = e->mNeighbourEdge;
			if (ne != nullptr)
			{
				Face *n = ne->mFace;
				if (!n->mRemoved)
				{
					// Check if vertex is on the front side of this face
					if (n->IsFacing(inVertex))
					{
						// Vertex on front, this face needs to be removed
						n->mRemoved = true;

						// Add element to the stack of elements to visit
						cur_stack_pos++;
						JPH_ASSERT(cur_stack_pos < cMaxEdgeLength);
						StackEntry &new_entry = stack[cur_stack_pos];
						new_entry.mFirstEdge = ne;
						new_entry.mCurrentEdge = ne->mNextEdge; // We don't need to test this edge again since we came from it
					}
					else
					{
						// Vertex behind, keep edge
						FullEdge full;
						full.mNeighbourEdge = ne;
						full.mStartIdx = e->mStartIdx;
						full.mEndIdx = ne->mStartIdx;
						outEdges.push_back(full);
					}
				}
			}
		}
	}

	// Assert that we have a fully connected loop
#ifdef JPH_ENABLE_ASSERTS
	for (int i = 0; i < (int)outEdges.size(); ++i)
		JPH_ASSERT(outEdges[i].mEndIdx == outEdges[(i + 1) % outEdges.size()].mStartIdx);
#endif

#ifdef JPH_CONVEX_BUILDER_DEBUG
	// Draw edge of facing faces
	for (int i = 0; i < (int)outEdges.size(); ++i)
		DebugRenderer::sInstance->DrawArrow(cDrawScale * (mOffset + mPositions[outEdges[i].mStartIdx]), cDrawScale * (mOffset + mPositions[outEdges[i].mEndIdx]), Color::sWhite, 0.01f);
	DrawState();
#endif
}

void ConvexHullBuilder::MergeFaces(Edge *inEdge)
{
	// Get the face
	Face *face = inEdge->mFace;

	// Find the previous and next edge
	Edge *next_edge = inEdge->mNextEdge;
	Edge *prev_edge = inEdge->GetPreviousEdge();

	// Get the other face
	Edge *other_edge = inEdge->mNeighbourEdge;
	Face *other_face = other_edge->mFace;

	// Check if attempting to merge with self
	JPH_ASSERT(face != other_face);

#ifdef JPH_CONVEX_BUILDER_DEBUG
	DrawWireFace(face, Color::sGreen);
	DrawWireFace(other_face, Color::sRed);
	DrawState();
#endif

	// Loop over the edges of the other face and make them belong to inFace
	Edge *edge = other_edge->mNextEdge;
	prev_edge->mNextEdge = edge;
	for (;;)
	{
		edge->mFace = face;
		if (edge->mNextEdge == other_edge)
		{
			// Terminate when we are back at other_edge
			edge->mNextEdge = next_edge;
			break;
		}
		edge = edge->mNextEdge;
	}

	// If the first edge happens to be inEdge we need to fix it because this edge is no longer part of the face.
	// Note that we replace it with the first edge of the merged face so that if the MergeFace function is called
	// from a loop that loops around the face that it will still terminate after visiting all edges once.
	if (face->mFirstEdge == inEdge)
		face->mFirstEdge = prev_edge->mNextEdge;

	// Free the edges
	delete inEdge;
	delete other_edge;

	// Mark the other face as removed
	other_face->mFirstEdge = nullptr;
	other_face->mRemoved = true;

	// Recalculate plane
	face->CalculateNormalAndCentroid(mPositions.data());

	// Merge conflict lists
	if (face->mFurthestPointDistanceSq > other_face->mFurthestPointDistanceSq)
	{
		// This face has a point that's further away, make sure it remains the last one as we add the other points to this faces list
		face->mConflictList.insert(face->mConflictList.end() - 1, other_face->mConflictList.begin(), other_face->mConflictList.end());
	}
	else
	{
		// The other face has a point that's furthest away, add that list at the end.
		face->mConflictList.insert(face->mConflictList.end(), other_face->mConflictList.begin(), other_face->mConflictList.end());
		face->mFurthestPointDistanceSq = other_face->mFurthestPointDistanceSq;
	}
	other_face->mConflictList.clear();

#ifdef JPH_CONVEX_BUILDER_DEBUG
	DrawWireFace(face, Color::sWhite);
	DrawState();
#endif
}

void ConvexHullBuilder::MergeDegenerateFace(Face *inFace, Faces &ioAffectedFaces)
{
	// Check area of face
	if (inFace->mNormal.LengthSq() < cMinTriangleAreaSq)
	{
		// Find longest edge, since this face is a sliver this should keep the face convex
		float max_length_sq = 0.0f;
		Edge *longest_edge = nullptr;
		Edge *e = inFace->mFirstEdge;
		Vec3 p1 = mPositions[e->mStartIdx];
		do
		{
			Edge *next = e->mNextEdge;
			Vec3 p2 = mPositions[next->mStartIdx];
			float length_sq = (p2 - p1).LengthSq();
			if (length_sq >= max_length_sq)
			{
				max_length_sq = length_sq;
				longest_edge = e;
			}
			p1 = p2;
			e = next;
		} while (e != inFace->mFirstEdge);

		// Merge with face on longest edge
		MergeFaces(longest_edge);

		// Remove any invalid edges
		RemoveInvalidEdges(inFace, ioAffectedFaces);
	}
}

void ConvexHullBuilder::MergeCoplanarOrConcaveFaces(Face *inFace, float inCoplanarToleranceSq, Faces &ioAffectedFaces)
{
	bool merged = false;

	Edge *edge = inFace->mFirstEdge;
	do
	{
		// Store next edge since this edge can be removed
		Edge *next_edge = edge->mNextEdge;

		// Test if centroid of one face is above plane of the other face by inCoplanarToleranceSq.
		// If so we need to merge other face into inFace.
		const Face *other_face = edge->mNeighbourEdge->mFace;
		Vec3 delta_centroid = other_face->mCentroid - inFace->mCentroid;
		float dist_other_face_centroid = inFace->mNormal.Dot(delta_centroid);
		float signed_dist_other_face_centroid_sq = abs(dist_other_face_centroid) * dist_other_face_centroid;
		float dist_face_centroid = -other_face->mNormal.Dot(delta_centroid);
		float signed_dist_face_centroid_sq = abs(dist_face_centroid) * dist_face_centroid;
		float face_normal_len_sq = inFace->mNormal.LengthSq();
		float other_face_normal_len_sq = other_face->mNormal.LengthSq();
		if ((signed_dist_other_face_centroid_sq > -inCoplanarToleranceSq * face_normal_len_sq
			|| signed_dist_face_centroid_sq > -inCoplanarToleranceSq * other_face_normal_len_sq)
			&& inFace->mNormal.Dot(other_face->mNormal) > 0.0f) // Never merge faces that are back to back
		{
			MergeFaces(edge);
			merged = true;
		}

		edge = next_edge;
	} while (edge != inFace->mFirstEdge);

	if (merged)
		RemoveInvalidEdges(inFace, ioAffectedFaces);
}

void ConvexHullBuilder::sMarkAffected(Face *inFace, Faces &ioAffectedFaces)
{
	if (std::find(ioAffectedFaces.begin(), ioAffectedFaces.end(), inFace) == ioAffectedFaces.end())
		ioAffectedFaces.push_back(inFace);
}

void ConvexHullBuilder::RemoveInvalidEdges(Face *inFace, Faces &ioAffectedFaces)
{
	// This marks that the plane needs to be recalculated (we delay this until the end of the
	// function since we don't use the plane and we want to avoid calculating it multiple times)
	bool recalculate_plane = false;

	// We keep going through this loop until no more edges were removed
	bool removed;
	do
	{
		removed = false;

		// Loop over all edges in this face
		Edge *edge = inFace->mFirstEdge;
		Face *neighbour_face = edge->mNeighbourEdge->mFace;
		do
		{
			Edge *next_edge = edge->mNextEdge;
			Face *next_neighbour_face = next_edge->mNeighbourEdge->mFace;

			if (neighbour_face == inFace)
			{
				// We only remove 1 edge at a time, check if this edge's next edge is our neighbour.
				// If this check fails, we will continue to scan along the edge until we find an edge where this is the case.
				if (edge->mNeighbourEdge == next_edge)
				{
					// This edge leads back to the starting point, this means the edge is interior and needs to be removed
#ifdef JPH_CONVEX_BUILDER_DEBUG
					DrawWireFace(inFace, Color::sBlue);
					DrawState();
#endif

					// Remove edge
					Edge *prev_edge = edge->GetPreviousEdge();
					prev_edge->mNextEdge = next_edge->mNextEdge;
					if (inFace->mFirstEdge == edge || inFace->mFirstEdge == next_edge)
						inFace->mFirstEdge = prev_edge;
					delete edge;
					delete next_edge;

#ifdef JPH_CONVEX_BUILDER_DEBUG
					DrawWireFace(inFace, Color::sGreen);
					DrawState();
#endif

					// Check if inFace now has only 2 edges left
					if (RemoveTwoEdgeFace(inFace, ioAffectedFaces))
						return; // Bail if face no longer exists

					// Restart the loop
					recalculate_plane = true;
					removed = true;
					break;
				}
			}
			else if (neighbour_face == next_neighbour_face)
			{
				// There are two edges that connect to the same face, we will remove the second one
#ifdef JPH_CONVEX_BUILDER_DEBUG
				DrawWireFace(inFace, Color::sYellow);
				DrawWireFace(neighbour_face, Color::sRed);
				DrawState();
#endif

				// First merge the neighbours edges
				Edge *neighbour_edge = next_edge->mNeighbourEdge;
				Edge *next_neighbour_edge = neighbour_edge->mNextEdge;
				if (neighbour_face->mFirstEdge == next_neighbour_edge)
					neighbour_face->mFirstEdge = neighbour_edge;
				neighbour_edge->mNextEdge = next_neighbour_edge->mNextEdge;
				neighbour_edge->mNeighbourEdge = edge;
				delete next_neighbour_edge;

				// Then merge my own edges
				if (inFace->mFirstEdge == next_edge)
					inFace->mFirstEdge = edge;
				edge->mNextEdge = next_edge->mNextEdge;
				edge->mNeighbourEdge = neighbour_edge;
				delete next_edge;

#ifdef JPH_CONVEX_BUILDER_DEBUG
				DrawWireFace(inFace, Color::sYellow);
				DrawWireFace(neighbour_face, Color::sGreen);
				DrawState();
#endif

				// Check if neighbour has only 2 edges left
				if (!RemoveTwoEdgeFace(neighbour_face, ioAffectedFaces))
				{
					// No, we need to recalculate its plane
					neighbour_face->CalculateNormalAndCentroid(mPositions.data());

					// Mark neighbour face as affected
					sMarkAffected(neighbour_face, ioAffectedFaces);
				}

				// Check if inFace now has only 2 edges left
				if (RemoveTwoEdgeFace(inFace, ioAffectedFaces))
					return; // Bail if face no longer exists

				// Restart loop
				recalculate_plane = true;
				removed = true;
				break;
			}

			// This edge is ok, go to the next edge
			edge = next_edge;
			neighbour_face = next_neighbour_face;

		} while (edge != inFace->mFirstEdge);
	} while (removed);

	// Recalculate plane?
	if (recalculate_plane)
		inFace->CalculateNormalAndCentroid(mPositions.data());
}

bool ConvexHullBuilder::RemoveTwoEdgeFace(Face *inFace, Faces &ioAffectedFaces) const
{
	// Check if this face contains only 2 edges
	Edge *edge = inFace->mFirstEdge;
	Edge *next_edge = edge->mNextEdge;
	JPH_ASSERT(edge != next_edge); // 1 edge faces should not exist
	if (next_edge->mNextEdge == edge)
	{
#ifdef JPH_CONVEX_BUILDER_DEBUG
		DrawWireFace(inFace, Color::sRed);
		DrawState();
#endif

		// Schedule both neighbours for re-checking
		Edge *neighbour_edge = edge->mNeighbourEdge;
		Face *neighbour_face = neighbour_edge->mFace;
		Edge *next_neighbour_edge = next_edge->mNeighbourEdge;
		Face *next_neighbour_face = next_neighbour_edge->mFace;
		sMarkAffected(neighbour_face, ioAffectedFaces);
		sMarkAffected(next_neighbour_face, ioAffectedFaces);

		// Link my neighbours to each other
		neighbour_edge->mNeighbourEdge = next_neighbour_edge;
		next_neighbour_edge->mNeighbourEdge = neighbour_edge;

		// Unlink my edges
		edge->mNeighbourEdge = nullptr;
		next_edge->mNeighbourEdge = nullptr;

		// Mark this face as removed
		inFace->mRemoved = true;

		return true;
	}

	return false;
}

#ifdef JPH_ENABLE_ASSERTS

void ConvexHullBuilder::DumpFace(const Face *inFace) const
{
	Trace("f:0x%p", inFace);

	const Edge *e = inFace->mFirstEdge;
	do
	{
		Trace("\te:0x%p { i:%d e:0x%p f:0x%p }", e, e->mStartIdx, e->mNeighbourEdge, e->mNeighbourEdge->mFace);
		e = e->mNextEdge;
	} while (e != inFace->mFirstEdge);
}

void ConvexHullBuilder::DumpFaces() const
{
	Trace("Dump Faces:");

	for (const Face *f : mFaces)
		if (!f->mRemoved)
			DumpFace(f);
}

void ConvexHullBuilder::ValidateFace(const Face *inFace) const
{
	if (inFace->mRemoved)
	{
		const Edge *e = inFace->mFirstEdge;
		if (e != nullptr)
			do
			{
				JPH_ASSERT(e->mNeighbourEdge == nullptr);
				e = e->mNextEdge;
			} while (e != inFace->mFirstEdge);
	}
	else
	{
		int edge_count = 0;

		const Edge *e = inFace->mFirstEdge;
		do
		{
			// Count edge
			++edge_count;

			// Validate that adjacent faces are all different
			if (mFaces.size() > 2)
				for (const Edge *other_edge = e->mNextEdge; other_edge != inFace->mFirstEdge; other_edge = other_edge->mNextEdge)
					JPH_ASSERT(e->mNeighbourEdge->mFace != other_edge->mNeighbourEdge->mFace);

			// Assert that the face is correct
			JPH_ASSERT(e->mFace == inFace);

			// Assert that we have a neighbour
			const Edge *nb_edge = e->mNeighbourEdge;
			JPH_ASSERT(nb_edge != nullptr);
			if (nb_edge != nullptr)
			{
				// Assert that our neighbours edge points to us
				JPH_ASSERT(nb_edge->mNeighbourEdge == e);

				// Assert that it belongs to a different face
				JPH_ASSERT(nb_edge->mFace != inFace);

				// Assert that the next edge of the neighbour points to the same vertex as this edge's vertex
				JPH_ASSERT(nb_edge->mNextEdge->mStartIdx == e->mStartIdx);

				// Assert that my next edge points to the same vertex as my neighbours vertex
				JPH_ASSERT(e->mNextEdge->mStartIdx == nb_edge->mStartIdx);
			}
			e = e->mNextEdge;
		} while (e != inFace->mFirstEdge);

		// Assert that we have 3 or more edges
		JPH_ASSERT(edge_count >= 3);
	}
}

void ConvexHullBuilder::ValidateFaces() const
{
	for (const Face *f : mFaces)
		ValidateFace(f);
}

#endif // JPH_ENABLE_ASSERTS

void ConvexHullBuilder::GetCenterOfMassAndVolume(Vec3 &outCenterOfMass, float &outVolume) const
{
	// Fourth point is the average of all face centroids
	Vec3 v4 = Vec3::sZero();
	for (const Face *f : mFaces)
		v4 += f->mCentroid;
	v4 /= float(mFaces.size());

	// Calculate mass and center of mass of this convex hull by summing all tetrahedrons
	outVolume = 0.0f;
	outCenterOfMass = Vec3::sZero();
	for (const Face *f : mFaces)
	{
		// Get the first vertex that we'll use to create a triangle fan
		Edge *e = f->mFirstEdge;
		Vec3 v1 = mPositions[e->mStartIdx];

		// Get the second vertex
		e = e->mNextEdge;
		Vec3 v2 = mPositions[e->mStartIdx];

		for (e = e->mNextEdge; e != f->mFirstEdge; e = e->mNextEdge)
		{
			// Fetch the last point of the triangle
			Vec3 v3 = mPositions[e->mStartIdx];

			// Calculate center of mass and mass of this tetrahedron,
			// see: https://en.wikipedia.org/wiki/Tetrahedron#Volume
			float volume_tetrahedron = (v1 - v4).Dot((v2 - v4).Cross(v3 - v4)); // Needs to be divided by 6, postpone this until the end of the loop
			Vec3 center_of_mass_tetrahedron = v1 + v2 + v3 + v4; // Needs to be divided by 4, postpone this until the end of the loop

			// Accumulate results
			outVolume += volume_tetrahedron;
			outCenterOfMass += volume_tetrahedron * center_of_mass_tetrahedron;

			// Update v2 for next triangle
			v2 = v3;
		}
	}

	// Calculate center of mass, fall back to average point in case there is no volume (everything is on a plane in this case)
	if (outVolume > FLT_EPSILON)
		outCenterOfMass /= 4.0f * outVolume;
	else
		outCenterOfMass = v4;

	outVolume /= 6.0f;
}

void ConvexHullBuilder::DetermineMaxError(Face *&outFaceWithMaxError, float &outMaxError, int &outMaxErrorPositionIdx, float &outCoplanarDistance) const
{
	outCoplanarDistance = DetermineCoplanarDistance();

	// This measures the distance from a polygon to the furthest point outside of the hull
	float max_error = 0.0f;
	Face *max_error_face = nullptr;
	int max_error_point = -1;

	for (int i = 0; i < (int)mPositions.size(); ++i)
	{
		Vec3 v = mPositions[i];

		// This measures the closest edge from all faces to point v
		// Note that we take the min of all faces since there may be multiple near coplanar faces so if we were to test this per face
		// we may find that a point is outside of a polygon and mark it as an error, while it is actually inside a nearly coplanar
		// polygon.
		float min_edge_dist_sq = FLT_MAX;
		Face *min_edge_dist_face = nullptr;

		for (Face *f : mFaces)
		{
			// Check if point is on or in front of plane
			float normal_len = f->mNormal.Length();
			JPH_ASSERT(normal_len > 0.0f);
			float plane_dist = f->mNormal.Dot(v - f->mCentroid) / normal_len;
			if (plane_dist > -outCoplanarDistance)
			{
				// Check distance to the edges of this face
				float edge_dist_sq = GetDistanceToEdgeSq(v, f);
				if (edge_dist_sq < min_edge_dist_sq)
				{
					min_edge_dist_sq = edge_dist_sq;
					min_edge_dist_face = f;
				}

				// If the point is inside the polygon and the point is in front of the plane, measure the distance
				if (edge_dist_sq == 0.0f && plane_dist > max_error)
				{
					max_error = plane_dist;
					max_error_face = f;
					max_error_point = i;
				}
			}
		}

		// If the minimum distance to an edge is further than our current max error, we use that as max error
		float min_edge_dist = sqrt(min_edge_dist_sq);
		if (min_edge_dist_face != nullptr && min_edge_dist > max_error)
		{
			max_error = min_edge_dist;
			max_error_face = min_edge_dist_face;
			max_error_point = i;
		}
	}

	outFaceWithMaxError = max_error_face;
	outMaxError = max_error;
	outMaxErrorPositionIdx = max_error_point;
}

#ifdef JPH_CONVEX_BUILDER_DEBUG

void ConvexHullBuilder::DrawState(bool inDrawConflictList) const
{
	// Draw origin
	DebugRenderer::sInstance->DrawMarker(cDrawScale * mOffset, Color::sRed, 0.2f);

	int face_idx = 0;

	// Draw faces
	for (const Face *f : mFaces)
		if (!f->mRemoved)
		{
			Color iteration_color = Color::sGetDistinctColor(f->mIteration);
			Color face_color = Color::sGetDistinctColor(face_idx++);

			// First point
			const Edge *e = f->mFirstEdge;
			RVec3 p1 = cDrawScale * (mOffset + mPositions[e->mStartIdx]);

			// Second point
			e = e->mNextEdge;
			RVec3 p2 = cDrawScale * (mOffset + mPositions[e->mStartIdx]);

			// First line
			DebugRenderer::sInstance->DrawLine(p1, p2, Color::sGrey);

			do
			{
				// Third point
				e = e->mNextEdge;
				RVec3 p3 = cDrawScale * (mOffset + mPositions[e->mStartIdx]);

				DebugRenderer::sInstance->DrawTriangle(p1, p2, p3, iteration_color);

				DebugRenderer::sInstance->DrawLine(p2, p3, Color::sGrey);

				p2 = p3;
			}
			while (e != f->mFirstEdge);

			// Draw normal
			RVec3 centroid = cDrawScale * (mOffset + f->mCentroid);
			DebugRenderer::sInstance->DrawArrow(centroid, centroid + f->mNormal.NormalizedOr(Vec3::sZero()), face_color, 0.01f);

			// Draw conflict list
			if (inDrawConflictList)
				for (int idx : f->mConflictList)
					DebugRenderer::sInstance->DrawMarker(cDrawScale * (mOffset + mPositions[idx]), face_color, 0.05f);
		}

	// Offset to the right
	mOffset += mDelta;
}

void ConvexHullBuilder::DrawWireFace(const Face *inFace, ColorArg inColor) const
{
	const Edge *e = inFace->mFirstEdge;
	RVec3 prev = cDrawScale * (mOffset + mPositions[e->mStartIdx]);
	do
	{
		const Edge *next = e->mNextEdge;
		RVec3 cur = cDrawScale * (mOffset + mPositions[next->mStartIdx]);
		DebugRenderer::sInstance->DrawArrow(prev, cur, inColor, 0.01f);
		DebugRenderer::sInstance->DrawText3D(prev, ConvertToString(e->mStartIdx), inColor);
		e = next;
		prev = cur;
	} while (e != inFace->mFirstEdge);
}

void ConvexHullBuilder::DrawEdge(const Edge *inEdge, ColorArg inColor) const
{
	RVec3 p1 = cDrawScale * (mOffset + mPositions[inEdge->mStartIdx]);
	RVec3 p2 = cDrawScale * (mOffset + mPositions[inEdge->mNextEdge->mStartIdx]);
	DebugRenderer::sInstance->DrawArrow(p1, p2, inColor, 0.01f);
}

#endif // JPH_CONVEX_BUILDER_DEBUG

#ifdef JPH_CONVEX_BUILDER_DUMP_SHAPE

void ConvexHullBuilder::DumpShape() const
{
	static atomic<int> sShapeNo = 1;
	int shape_no = sShapeNo++;

	std::ofstream f;
	f.open(StringFormat("dumped_shape%d.cpp", shape_no).c_str(), std::ofstream::out | std::ofstream::trunc);
	if (!f.is_open())
		return;

	f << "{\n";
	for (Vec3 v : mPositions)
		f << StringFormat("\tVec3(%.9gf, %.9gf, %.9gf),\n", (double)v.GetX(), (double)v.GetY(), (double)v.GetZ());
	f << "},\n";
}

#endif // JPH_CONVEX_BUILDER_DUMP_SHAPE

JPH_NAMESPACE_END

// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

// Define to validate the integrity of the hull structure
//#define JPH_EPA_CONVEX_BUILDER_VALIDATE

// Define to draw the building of the hull for debugging purposes
//#define JPH_EPA_CONVEX_BUILDER_DRAW

#include <Jolt/Core/NonCopyable.h>
#include <Jolt/Core/BinaryHeap.h>

#ifdef JPH_EPA_CONVEX_BUILDER_DRAW
	#include <Jolt/Renderer/DebugRenderer.h>
	#include <Jolt/Core/StringTools.h>
#endif

JPH_NAMESPACE_BEGIN

/// A convex hull builder specifically made for the EPA penetration depth calculation. It trades accuracy for speed and will simply abort of the hull forms defects due to numerical precision problems.
class EPAConvexHullBuilder : public NonCopyable
{
private:
#ifdef JPH_EPA_CONVEX_BUILDER_DRAW
	/// Factor to scale convex hull when debug drawing the construction process
	static constexpr Real cDrawScale = 10;
#endif

public:
	// Due to the Euler characteristic (https://en.wikipedia.org/wiki/Euler_characteristic) we know that Vertices - Edges + Faces = 2
	// In our case we only have triangles and they are always fully connected, so each edge is shared exactly between 2 faces: Edges = Faces * 3 / 2
	// Substituting: Vertices = Faces / 2 + 2 which is approximately Faces / 2.
	static constexpr int cMaxTriangles = 256;				///< Max triangles in hull
	static constexpr int cMaxPoints = cMaxTriangles / 2;	///< Max number of points in hull

	// Constants
	static constexpr int cMaxEdgeLength = 128;				///< Max number of edges in FindEdge
	static constexpr float cMinTriangleArea = 1.0e-10f;		///< Minimum area of a triangle before, if smaller than this it will not be added to the priority queue
	static constexpr float cBarycentricEpsilon = 1.0e-3f;	///< Epsilon value used to determine if a point is in the interior of a triangle

	// Forward declare
	class Triangle;

	/// Class that holds the information of an edge
	class Edge
	{
	public:
		/// Information about neighbouring triangle
		Triangle *		mNeighbourTriangle;					///< Triangle that neighbours this triangle
		int				mNeighbourEdge;						///< Index in mEdge that specifies edge that this Edge is connected to

		int				mStartIdx;							///< Vertex index in mPositions that indicates the start vertex of this edge
	};

	using Edges = StaticArray<Edge, cMaxEdgeLength>;
	using NewTriangles = StaticArray<Triangle *, cMaxEdgeLength>;

	/// Class that holds the information of one triangle
	class Triangle : public NonCopyable
	{
	public:
		/// Constructor
		inline			Triangle(int inIdx0, int inIdx1, int inIdx2, const Vec3 *inPositions);

		/// Check if triangle is facing inPosition
		inline bool		IsFacing(Vec3Arg inPosition) const
		{
			JPH_ASSERT(!mRemoved);
			return mNormal.Dot(inPosition - mCentroid) > 0.0f;
		}

		/// Check if triangle is facing the origin
		inline bool		IsFacingOrigin() const
		{
			JPH_ASSERT(!mRemoved);
			return mNormal.Dot(mCentroid) < 0.0f;
		}

		/// Get the next edge of edge inIndex
		inline const Edge & GetNextEdge(int inIndex) const
		{
			return mEdge[(inIndex + 1) % 3];
		}

		Edge			mEdge[3];							///< 3 edges of this triangle
		Vec3			mNormal;							///< Normal of this triangle, length is 2 times area of triangle
		Vec3			mCentroid;							///< Center of the triangle
		float			mClosestLenSq = FLT_MAX;			///< Closest distance^2 from origin to triangle
		float			mLambda[2];							///< Barycentric coordinates of closest point to origin on triangle
		bool			mLambdaRelativeTo0;					///< How to calculate the closest point, true: y0 + l0 * (y1 - y0) + l1 * (y2 - y0), false: y1 + l0 * (y0 - y1) + l1 * (y2 - y1)
		bool			mClosestPointInterior = false;		///< Flag that indicates that the closest point from this triangle to the origin is an interior point
		bool			mRemoved = false;					///< Flag that indicates that triangle has been removed
		bool			mInQueue = false;					///< Flag that indicates that this triangle was placed in the sorted heap (stays true after it is popped because the triangle is freed by the main EPA algorithm loop)
#ifdef JPH_EPA_CONVEX_BUILDER_DRAW
		int				mIteration;							///< Iteration that this triangle was created
#endif
	};

	/// Factory that creates triangles in a fixed size buffer
	class TriangleFactory : public NonCopyable
	{
	private:
		/// Struct that stores both a triangle or a next pointer in case the triangle is unused
		union alignas(Triangle) Block
		{
			uint8		mTriangle[sizeof(Triangle)];
			Block *		mNextFree;
		};

		/// Storage for triangle data
		Block			mTriangles[cMaxTriangles];			///< Storage for triangles
		Block *			mNextFree = nullptr;				///< List of free triangles
		int				mHighWatermark = 0;					///< High water mark for used triangles (if mNextFree == nullptr we can take one from here)

	public:
		/// Return all triangles to the free pool
		void			Clear()
		{
			mNextFree = nullptr;
			mHighWatermark = 0;
		}

		/// Allocate a new triangle with 3 indexes
		Triangle *		CreateTriangle(int inIdx0, int inIdx1, int inIdx2, const Vec3 *inPositions)
		{
			Triangle *t;
			if (mNextFree != nullptr)
			{
				// Entry available from the free list
				t = reinterpret_cast<Triangle *>(&mNextFree->mTriangle);
				mNextFree = mNextFree->mNextFree;
			}
			else
			{
				// Allocate from never used before triangle store
				if (mHighWatermark >= cMaxTriangles)
					return nullptr; // Buffer full
				t = reinterpret_cast<Triangle *>(&mTriangles[mHighWatermark].mTriangle);
				++mHighWatermark;
			}

			// Call constructor
			new (t) Triangle(inIdx0, inIdx1, inIdx2, inPositions);

			return t;
		}

		/// Free a triangle
		void			FreeTriangle(Triangle *inT)
		{
			// Destruct triangle
			inT->~Triangle();
#ifdef JPH_DEBUG
			memset(inT, 0xcd, sizeof(Triangle));
#endif

			// Add triangle to the free list
			Block *tu = reinterpret_cast<Block *>(inT);
			tu->mNextFree = mNextFree;
			mNextFree = tu;
		}
	};

	// Typedefs
	using PointsBase = StaticArray<Vec3, cMaxPoints>;
	using Triangles = StaticArray<Triangle *, cMaxTriangles>;

	/// Specialized points list that allows direct access to the size
	class Points : public PointsBase
	{
	public:
		size_type &		GetSizeRef()
		{
			return mSize;
		}
	};

	/// Specialized triangles list that keeps them sorted on closest distance to origin
	class TriangleQueue : public Triangles
	{
	public:
		/// Function to sort triangles on closest distance to origin
		static bool		sTriangleSorter(const Triangle *inT1, const Triangle *inT2)
		{
			return inT1->mClosestLenSq > inT2->mClosestLenSq;
		}

		/// Add triangle to the list
		void			push_back(Triangle *inT)
		{
			// Add to base
			Triangles::push_back(inT);

			// Mark in queue
			inT->mInQueue = true;

			// Resort heap
			BinaryHeapPush(begin(), end(), sTriangleSorter);
		}

		/// Peek the next closest triangle without removing it
		Triangle *		PeekClosest()
		{
			return front();
		}

		/// Get next closest triangle
		Triangle *		PopClosest()
		{
			// Move closest to end
			BinaryHeapPop(begin(), end(), sTriangleSorter);

			// Remove last triangle
			Triangle *t = back();
			pop_back();
			return t;
		}
	};

	/// Constructor
	explicit			EPAConvexHullBuilder(const Points &inPositions) :
		mPositions(inPositions)
	{
#ifdef JPH_EPA_CONVEX_BUILDER_DRAW
		mIteration = 0;
		mOffset = RVec3::sZero();
#endif
	}

	/// Initialize the hull with 3 points
	void				Initialize(int inIdx1, int inIdx2, int inIdx3)
	{
		// Release triangles
		mFactory.Clear();

		// Create triangles (back to back)
		Triangle *t1 = CreateTriangle(inIdx1, inIdx2, inIdx3);
		Triangle *t2 = CreateTriangle(inIdx1, inIdx3, inIdx2);

		// Link triangles edges
		sLinkTriangle(t1, 0, t2, 2);
		sLinkTriangle(t1, 1, t2, 1);
		sLinkTriangle(t1, 2, t2, 0);

		// Always add both triangles to the priority queue
		mTriangleQueue.push_back(t1);
		mTriangleQueue.push_back(t2);

#ifdef JPH_EPA_CONVEX_BUILDER_DRAW
		// Draw current state
		DrawState();

		// Increment iteration counter
		++mIteration;
#endif
	}

	/// Check if there's another triangle to process from the queue
	bool				HasNextTriangle() const
	{
		return !mTriangleQueue.empty();
	}

	/// Access to the next closest triangle to the origin (won't remove it from the queue).
	Triangle *			PeekClosestTriangleInQueue()
	{
		return mTriangleQueue.PeekClosest();
	}

	/// Access to the next closest triangle to the origin and remove it from the queue.
	Triangle *			PopClosestTriangleFromQueue()
	{
		return mTriangleQueue.PopClosest();
	}

	/// Find the triangle on which inPosition is the furthest to the front
	/// Note this function works as long as all points added have been added with AddPoint(..., FLT_MAX).
	Triangle *			FindFacingTriangle(Vec3Arg inPosition, float &outBestDistSq)
	{
		Triangle *best = nullptr;
		float best_dist_sq = 0.0f;

		for (Triangle *t : mTriangleQueue)
			if (!t->mRemoved)
			{
				float dot = t->mNormal.Dot(inPosition - t->mCentroid);
				if (dot > 0.0f)
				{
					float dist_sq = dot * dot / t->mNormal.LengthSq();
					if (dist_sq > best_dist_sq)
					{
						best = t;
						best_dist_sq = dist_sq;
					}
				}
			}

		outBestDistSq = best_dist_sq;
		return best;
	}

	/// Add a new point to the convex hull
	bool				AddPoint(Triangle *inFacingTriangle, int inIdx, float inClosestDistSq, NewTriangles &outTriangles)
	{
		// Get position
		Vec3 pos = mPositions[inIdx];

#ifdef JPH_EPA_CONVEX_BUILDER_DRAW
		// Draw new support point
		DrawMarker(pos, Color::sYellow, 1.0f);
#endif

#ifdef JPH_EPA_CONVEX_BUILDER_VALIDATE
		// Check if structure is intact
		ValidateTriangles();
#endif

		// Find edge of convex hull of triangles that are not facing the new vertex w
		Edges edges;
		if (!FindEdge(inFacingTriangle, pos, edges))
			return false;

		// Create new triangles
		int num_edges = edges.size();
		for (int i = 0; i < num_edges; ++i)
		{
			// Create new triangle
			Triangle *nt = CreateTriangle(edges[i].mStartIdx, edges[(i + 1) % num_edges].mStartIdx, inIdx);
			if (nt == nullptr)
				return false;
			outTriangles.push_back(nt);

			// Check if we need to put this triangle in the priority queue
			if ((nt->mClosestPointInterior && nt->mClosestLenSq < inClosestDistSq)	// For the main algorithm
				|| nt->mClosestLenSq < 0.0f)										// For when the origin is not inside the hull yet
				mTriangleQueue.push_back(nt);
		}

		// Link edges
		for (int i = 0; i < num_edges; ++i)
		{
			sLinkTriangle(outTriangles[i], 0, edges[i].mNeighbourTriangle, edges[i].mNeighbourEdge);
			sLinkTriangle(outTriangles[i], 1, outTriangles[(i + 1) % num_edges], 2);
		}

#ifdef JPH_EPA_CONVEX_BUILDER_VALIDATE
		// Check if structure is intact
		ValidateTriangles();
#endif

#ifdef JPH_EPA_CONVEX_BUILDER_DRAW
		// Draw state of the hull
		DrawState();

		// Increment iteration counter
		++mIteration;
#endif

		return true;
	}

	/// Free a triangle
	void				FreeTriangle(Triangle *inT)
	{
#ifdef JPH_ENABLE_ASSERTS
		// Make sure that this triangle is not connected
		JPH_ASSERT(inT->mRemoved);
		for (const Edge &e : inT->mEdge)
			JPH_ASSERT(e.mNeighbourTriangle == nullptr);
#endif

#if defined(JPH_EPA_CONVEX_BUILDER_VALIDATE) || defined(JPH_EPA_CONVEX_BUILDER_DRAW)
		// Remove from list of all triangles
		Triangles::iterator i = std::find(mTriangles.begin(), mTriangles.end(), inT);
		JPH_ASSERT(i != mTriangles.end());
		mTriangles.erase(i);
#endif

		mFactory.FreeTriangle(inT);
	}

private:
	/// Create a new triangle
	Triangle *			CreateTriangle(int inIdx1, int inIdx2, int inIdx3)
	{
		// Call provider to create triangle
		Triangle *t = mFactory.CreateTriangle(inIdx1, inIdx2, inIdx3, mPositions.data());
		if (t == nullptr)
			return nullptr;

#ifdef JPH_EPA_CONVEX_BUILDER_DRAW
		// Remember iteration counter
		t->mIteration = mIteration;
#endif

#if defined(JPH_EPA_CONVEX_BUILDER_VALIDATE) || defined(JPH_EPA_CONVEX_BUILDER_DRAW)
		// Add to list of triangles for debugging purposes
		mTriangles.push_back(t);
#endif

		return t;
	}

	/// Link triangle edge to other triangle edge
	static void			sLinkTriangle(Triangle *inT1, int inEdge1, Triangle *inT2, int inEdge2)
	{
		JPH_ASSERT(inEdge1 >= 0 && inEdge1 < 3);
		JPH_ASSERT(inEdge2 >= 0 && inEdge2 < 3);
		Edge &e1 = inT1->mEdge[inEdge1];
		Edge &e2 = inT2->mEdge[inEdge2];

		// Check not connected yet
		JPH_ASSERT(e1.mNeighbourTriangle == nullptr);
		JPH_ASSERT(e2.mNeighbourTriangle == nullptr);

		// Check vertices match
		JPH_ASSERT(e1.mStartIdx == inT2->GetNextEdge(inEdge2).mStartIdx);
		JPH_ASSERT(e2.mStartIdx == inT1->GetNextEdge(inEdge1).mStartIdx);

		// Link up
		e1.mNeighbourTriangle = inT2;
		e1.mNeighbourEdge = inEdge2;
		e2.mNeighbourTriangle = inT1;
		e2.mNeighbourEdge = inEdge1;
	}

	/// Unlink this triangle
	void				UnlinkTriangle(Triangle *inT)
	{
		// Unlink from neighbours
		for (int i = 0; i < 3; ++i)
		{
			Edge &edge = inT->mEdge[i];
			if (edge.mNeighbourTriangle != nullptr)
			{
				Edge &neighbour_edge = edge.mNeighbourTriangle->mEdge[edge.mNeighbourEdge];

				// Validate that neighbour points to us
				JPH_ASSERT(neighbour_edge.mNeighbourTriangle == inT);
				JPH_ASSERT(neighbour_edge.mNeighbourEdge == i);

				// Unlink
				neighbour_edge.mNeighbourTriangle = nullptr;
				edge.mNeighbourTriangle = nullptr;
			}
		}

		// If this triangle is not in the priority queue, we can delete it now
		if (!inT->mInQueue)
			FreeTriangle(inT);
	}

	/// Given one triangle that faces inVertex, find the edges of the triangles that are not facing inVertex.
	/// Will flag all those triangles for removal.
	bool				FindEdge(Triangle *inFacingTriangle, Vec3Arg inVertex, Edges &outEdges)
	{
		// Assert that we were given an empty array
		JPH_ASSERT(outEdges.empty());

		// Should start with a facing triangle
		JPH_ASSERT(inFacingTriangle->IsFacing(inVertex));

		// Flag as removed
		inFacingTriangle->mRemoved = true;

		// Instead of recursing, we build our own stack with the information we need
		struct StackEntry
		{
			Triangle *	mTriangle;
			int			mEdge;
			int			mIter;
		};
		StackEntry stack[cMaxEdgeLength];
		int cur_stack_pos = 0;

		// Start with the triangle / edge provided
		stack[0].mTriangle = inFacingTriangle;
		stack[0].mEdge = 0;
		stack[0].mIter = -1; // Start with edge 0 (is incremented below before use)

		// Next index that we expect to find, if we don't then there are 'islands'
		int next_expected_start_idx = -1;

		for (;;)
		{
			StackEntry &cur_entry = stack[cur_stack_pos];

			// Next iteration
			if (++cur_entry.mIter >= 3)
			{
				// This triangle needs to be removed, unlink it now
				UnlinkTriangle(cur_entry.mTriangle);

				// Pop from stack
				if (--cur_stack_pos < 0)
					break;
			}
			else
			{
				// Visit neighbour
				Edge &e = cur_entry.mTriangle->mEdge[(cur_entry.mEdge + cur_entry.mIter) % 3];
				Triangle *n = e.mNeighbourTriangle;
				if (n != nullptr && !n->mRemoved)
				{
					// Check if vertex is on the front side of this triangle
					if (n->IsFacing(inVertex))
					{
						// Vertex on front, this triangle needs to be removed
						n->mRemoved = true;

						// Add element to the stack of elements to visit
						cur_stack_pos++;
						JPH_ASSERT(cur_stack_pos < cMaxEdgeLength);
						StackEntry &new_entry = stack[cur_stack_pos];
						new_entry.mTriangle = n;
						new_entry.mEdge = e.mNeighbourEdge;
						new_entry.mIter = 0; // Is incremented before use, we don't need to test this edge again since we came from it
					}
					else
					{
						// Detect if edge doesn't connect to previous edge, if this happens we have found and 'island' which means
						// the newly added point is so close to the triangles of the hull that we classified some (nearly) coplanar
						// triangles as before and some behind the point. At this point we just abort adding the point because
						// we've reached numerical precision.
						// Note that we do not need to test if the first and last edge connect, since when there are islands
						// there should be at least 2 disconnects.
						if (e.mStartIdx != next_expected_start_idx && next_expected_start_idx != -1)
							return false;

						// Next expected index is the start index of our neighbour's edge
						next_expected_start_idx = n->mEdge[e.mNeighbourEdge].mStartIdx;

						// Vertex behind, keep edge
						outEdges.push_back(e);
					}
				}
			}
		}

		// Assert that we have a fully connected loop
		JPH_ASSERT(outEdges.empty() || outEdges[0].mStartIdx == next_expected_start_idx);

#ifdef JPH_EPA_CONVEX_BUILDER_DRAW
		// Draw edge of facing triangles
		for (int i = 0; i < (int)outEdges.size(); ++i)
		{
			RVec3 edge_start = cDrawScale * (mOffset + mPositions[outEdges[i].mStartIdx]);
			DebugRenderer::sInstance->DrawArrow(edge_start, cDrawScale * (mOffset + mPositions[outEdges[(i + 1) % outEdges.size()].mStartIdx]), Color::sYellow, 0.01f);
			DebugRenderer::sInstance->DrawText3D(edge_start, ConvertToString(outEdges[i].mStartIdx), Color::sWhite);
		}

		// Draw the state with the facing triangles removed
		DrawState();
#endif

		// When we start with two triangles facing away from each other and adding a point that is on the plane,
		// sometimes we consider the point in front of both causing both triangles to be removed resulting in an empty edge list.
		// In this case we fail to add the point which will result in no collision reported (the shapes are contacting in 1 point so there's 0 penetration)
		return outEdges.size() >= 3;
	}

#ifdef JPH_EPA_CONVEX_BUILDER_VALIDATE
	/// Check consistency of 1 triangle
	void				ValidateTriangle(const Triangle *inT) const
	{
		if (inT->mRemoved)
		{
			// Validate that removed triangles are not connected to anything
			for (const Edge &my_edge : inT->mEdge)
				JPH_ASSERT(my_edge.mNeighbourTriangle == nullptr);
		}
		else
		{
			for (int i = 0; i < 3; ++i)
			{
				const Edge &my_edge = inT->mEdge[i];

				// Assert that we have a neighbour
				const Triangle *nb = my_edge.mNeighbourTriangle;
				JPH_ASSERT(nb != nullptr);

				if (nb != nullptr)
				{
					// Assert that our neighbours edge points to us
					const Edge &nb_edge = nb->mEdge[my_edge.mNeighbourEdge];
					JPH_ASSERT(nb_edge.mNeighbourTriangle == inT);
					JPH_ASSERT(nb_edge.mNeighbourEdge == i);

					// Assert that the next edge of the neighbour points to the same vertex as this edge's vertex
					const Edge &nb_next_edge = nb->GetNextEdge(my_edge.mNeighbourEdge);
					JPH_ASSERT(nb_next_edge.mStartIdx == my_edge.mStartIdx);

					// Assert that my next edge points to the same vertex as my neighbours vertex
					const Edge &my_next_edge = inT->GetNextEdge(i);
					JPH_ASSERT(my_next_edge.mStartIdx == nb_edge.mStartIdx);
				}
			}
		}
	}

	/// Check consistency of all triangles
	void				ValidateTriangles() const
	{
		for (const Triangle *t : mTriangles)
			ValidateTriangle(t);
	}
#endif

#ifdef JPH_EPA_CONVEX_BUILDER_DRAW
public:
	/// Draw state of algorithm
	void				DrawState()
	{
		// Draw origin
		DebugRenderer::sInstance->DrawCoordinateSystem(RMat44::sTranslation(cDrawScale * mOffset), 1.0f);

		// Draw triangles
		for (const Triangle *t : mTriangles)
			if (!t->mRemoved)
			{
				// Calculate the triangle vertices
				RVec3 p1 = cDrawScale * (mOffset + mPositions[t->mEdge[0].mStartIdx]);
				RVec3 p2 = cDrawScale * (mOffset + mPositions[t->mEdge[1].mStartIdx]);
				RVec3 p3 = cDrawScale * (mOffset + mPositions[t->mEdge[2].mStartIdx]);

				// Draw triangle
				DebugRenderer::sInstance->DrawTriangle(p1, p2, p3, Color::sGetDistinctColor(t->mIteration));
				DebugRenderer::sInstance->DrawWireTriangle(p1, p2, p3, Color::sGrey);

				// Draw normal
				RVec3 centroid = cDrawScale * (mOffset + t->mCentroid);
				float len = t->mNormal.Length();
				if (len > 0.0f)
					DebugRenderer::sInstance->DrawArrow(centroid, centroid + t->mNormal / len, Color::sDarkGreen, 0.01f);
			}

		// Determine max position
		float min_x = FLT_MAX;
		float max_x = -FLT_MAX;
		for (Vec3 p : mPositions)
		{
			min_x = min(min_x, p.GetX());
			max_x = max(max_x, p.GetX());
		}

		// Offset to the right
		mOffset += Vec3(max_x - min_x + 0.5f, 0.0f, 0.0f);
	}

	/// Draw a label to indicate the next stage in the algorithm
	void				DrawLabel(const string_view &inText)
	{
		DebugRenderer::sInstance->DrawText3D(cDrawScale * mOffset, inText, Color::sWhite, 0.1f * cDrawScale);

		mOffset += Vec3(5.0f, 0.0f, 0.0f);
	}

	/// Draw geometry for debugging purposes
	void				DrawGeometry(const DebugRenderer::GeometryRef &inGeometry, ColorArg inColor)
	{
		RMat44 origin = RMat44::sScale(Vec3::sReplicate(cDrawScale)) * RMat44::sTranslation(mOffset);
		DebugRenderer::sInstance->DrawGeometry(origin, inGeometry->mBounds.Transformed(origin), inGeometry->mBounds.GetExtent().LengthSq(), inColor, inGeometry);

		mOffset += Vec3(inGeometry->mBounds.GetSize().GetX(), 0, 0);
	}

	/// Draw a triangle for debugging purposes
	void				DrawWireTriangle(const Triangle &inTriangle, ColorArg inColor)
	{
		RVec3 prev = cDrawScale * (mOffset + mPositions[inTriangle.mEdge[2].mStartIdx]);
		for (const Edge &edge : inTriangle.mEdge)
		{
			RVec3 cur = cDrawScale * (mOffset + mPositions[edge.mStartIdx]);
			DebugRenderer::sInstance->DrawArrow(prev, cur, inColor, 0.01f);
			prev = cur;
		}
	}

	/// Draw a marker for debugging purposes
	void				DrawMarker(Vec3Arg inPosition, ColorArg inColor, float inSize)
	{
		DebugRenderer::sInstance->DrawMarker(cDrawScale * (mOffset + inPosition), inColor, inSize);
	}

	/// Draw an arrow for debugging purposes
	void				DrawArrow(Vec3Arg inFrom, Vec3Arg inTo, ColorArg inColor, float inArrowSize)
	{
		DebugRenderer::sInstance->DrawArrow(cDrawScale * (mOffset + inFrom), cDrawScale * (mOffset + inTo), inColor, inArrowSize);
	}
#endif

private:
	TriangleFactory		mFactory;							///< Factory to create new triangles and remove old ones
	const Points &		mPositions;							///< List of positions (some of them are part of the hull)
	TriangleQueue		mTriangleQueue;						///< List of triangles that are part of the hull that still need to be checked (if !mRemoved)

#if defined(JPH_EPA_CONVEX_BUILDER_VALIDATE) || defined(JPH_EPA_CONVEX_BUILDER_DRAW)
	Triangles			mTriangles;							///< The list of all triangles in this hull (for debug purposes)
#endif

#ifdef JPH_EPA_CONVEX_BUILDER_DRAW
	int					mIteration;							///< Number of iterations we've had so far (for debug purposes)
	RVec3				mOffset;							///< Offset to use for state drawing
#endif
};

// The determinant that is calculated in the Triangle constructor is really sensitive
// to numerical round off, disable the fmadd instructions to maintain precision.
JPH_PRECISE_MATH_ON

EPAConvexHullBuilder::Triangle::Triangle(int inIdx0, int inIdx1, int inIdx2, const Vec3 *inPositions)
{
	// Fill in indexes
	JPH_ASSERT(inIdx0 != inIdx1 && inIdx0 != inIdx2 && inIdx1 != inIdx2);
	mEdge[0].mStartIdx = inIdx0;
	mEdge[1].mStartIdx = inIdx1;
	mEdge[2].mStartIdx = inIdx2;

	// Clear links
	mEdge[0].mNeighbourTriangle = nullptr;
	mEdge[1].mNeighbourTriangle = nullptr;
	mEdge[2].mNeighbourTriangle = nullptr;

	// Get vertex positions
	Vec3 y0 = inPositions[inIdx0];
	Vec3 y1 = inPositions[inIdx1];
	Vec3 y2 = inPositions[inIdx2];

	// Calculate centroid
	mCentroid = (y0 + y1 + y2) / 3.0f;

	// Calculate edges
	Vec3 y10 = y1 - y0;
	Vec3 y20 = y2 - y0;
	Vec3 y21 = y2 - y1;

	// The most accurate normal is calculated by using the two shortest edges
	// See: https://box2d.org/posts/2014/01/troublesome-triangle/
	// The difference in normals is most pronounced when one edge is much smaller than the others (in which case the other 2 must have roughly the same length).
	// Therefore we can suffice by just picking the shortest from 2 edges and use that with the 3rd edge to calculate the normal.
	// We first check which of the edges is shorter.
	float y20_dot_y20 = y20.Dot(y20);
	float y21_dot_y21 = y21.Dot(y21);
	if (y20_dot_y20 < y21_dot_y21)
	{
		// We select the edges y10 and y20
		mNormal = y10.Cross(y20);

		// Check if triangle is degenerate
		float normal_len_sq = mNormal.LengthSq();
		if (normal_len_sq > cMinTriangleArea)
		{
			// Determine distance between triangle and origin: distance = (centroid - origin) . normal / |normal|
			// Note that this way of calculating the closest point is much more accurate than first calculating barycentric coordinates and then calculating the closest
			// point based on those coordinates. Note that we preserve the sign of the distance to check on which side the origin is.
			float c_dot_n = mCentroid.Dot(mNormal);
			mClosestLenSq = abs(c_dot_n) * c_dot_n / normal_len_sq;

			// Calculate closest point to origin using barycentric coordinates:
			//
			// v = y0 + l0 * (y1 - y0) + l1 * (y2 - y0)
			// v . (y1 - y0) = 0
			// v . (y2 - y0) = 0
			//
			// Written in matrix form:
			//
			// | y10.y10  y20.y10 | | l0 | = | -y0.y10 |
			// | y10.y20  y20.y20 | | l1 |   | -y0.y20 |
			//
			// (y10 = y1 - y0 etc.)
			//
			// Cramers rule to invert matrix:
			float y10_dot_y10 = y10.LengthSq();
			float y10_dot_y20 = y10.Dot(y20);
			float determinant = y10_dot_y10 * y20_dot_y20 - y10_dot_y20 * y10_dot_y20;
			if (determinant > 0.0f) // If determinant == 0 then the system is linearly dependent and the triangle is degenerate, since y10.10 * y20.y20 > y10.y20^2 it should also be > 0
			{
				float y0_dot_y10 = y0.Dot(y10);
				float y0_dot_y20 = y0.Dot(y20);
				float l0 = (y10_dot_y20 * y0_dot_y20 - y20_dot_y20 * y0_dot_y10) / determinant;
				float l1 = (y10_dot_y20 * y0_dot_y10 - y10_dot_y10 * y0_dot_y20) / determinant;
				mLambda[0] = l0;
				mLambda[1] = l1;
				mLambdaRelativeTo0 = true;

				// Check if closest point is interior to the triangle. For a convex hull which contains the origin each face must contain the origin, but because
				// our faces are triangles, we can have multiple coplanar triangles and only 1 will have the origin as an interior point. We want to use this triangle
				// to calculate the contact points because it gives the most accurate results, so we will only add these triangles to the priority queue.
				if (l0 > -cBarycentricEpsilon && l1 > -cBarycentricEpsilon && l0 + l1 < 1.0f + cBarycentricEpsilon)
					mClosestPointInterior = true;
			}
		}
	}
	else
	{
		// We select the edges y10 and y21
		mNormal = y10.Cross(y21);

		// Check if triangle is degenerate
		float normal_len_sq = mNormal.LengthSq();
		if (normal_len_sq > cMinTriangleArea)
		{
			// Again calculate distance between triangle and origin
			float c_dot_n = mCentroid.Dot(mNormal);
			mClosestLenSq = abs(c_dot_n) * c_dot_n / normal_len_sq;

			// Calculate closest point to origin using barycentric coordinates but this time using y1 as the reference vertex
			//
			// v = y1 + l0 * (y0 - y1) + l1 * (y2 - y1)
			// v . (y0 - y1) = 0
			// v . (y2 - y1) = 0
			//
			// Written in matrix form:
			//
			// | y10.y10  -y21.y10 | | l0 | = | y1.y10 |
			// | -y10.y21  y21.y21 | | l1 |   | -y1.y21 |
			//
			// Cramers rule to invert matrix:
			float y10_dot_y10 = y10.LengthSq();
			float y10_dot_y21 = y10.Dot(y21);
			float determinant = y10_dot_y10 * y21_dot_y21 - y10_dot_y21 * y10_dot_y21;
			if (determinant > 0.0f)
			{
				float y1_dot_y10 = y1.Dot(y10);
				float y1_dot_y21 = y1.Dot(y21);
				float l0 = (y21_dot_y21 * y1_dot_y10 - y10_dot_y21 * y1_dot_y21) / determinant;
				float l1 = (y10_dot_y21 * y1_dot_y10 - y10_dot_y10 * y1_dot_y21) / determinant;
				mLambda[0] = l0;
				mLambda[1] = l1;
				mLambdaRelativeTo0 = false;

				// Again check if the closest point is inside the triangle
				if (l0 > -cBarycentricEpsilon && l1 > -cBarycentricEpsilon && l0 + l1 < 1.0f + cBarycentricEpsilon)
					mClosestPointInterior = true;
			}
		}
	}
}

JPH_PRECISE_MATH_OFF

JPH_NAMESPACE_END

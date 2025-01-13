// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Jolt/Core/NonCopyable.h>

//#define JPH_CONVEX_BUILDER_2D_DEBUG

JPH_NAMESPACE_BEGIN

/// A convex hull builder that tries to create 2D hulls as accurately as possible. Used for offline processing.
class JPH_EXPORT ConvexHullBuilder2D : public NonCopyable
{
public:
	using Positions = Array<Vec3>;
	using Edges = Array<int>;

	/// Constructor
	/// @param inPositions Positions used to make the hull. Uses X and Y component of Vec3 only!
	explicit			ConvexHullBuilder2D(const Positions &inPositions);

	/// Destructor
						~ConvexHullBuilder2D();

	/// Result enum that indicates how the hull got created
	enum class EResult
	{
		Success,													///< Hull building finished successfully
		MaxVerticesReached,											///< Hull building finished successfully, but the desired accuracy was not reached because the max vertices limit was reached
	};

	/// Takes all positions as provided by the constructor and use them to build a hull
	/// Any points that are closer to the hull than inTolerance will be discarded
	/// @param inIdx1 , inIdx2 , inIdx3 The indices to use as initial hull (in any order)
	/// @param inMaxVertices Max vertices to allow in the hull. Specify INT_MAX if there is no limit.
	/// @param inTolerance Max distance that a point is allowed to be outside of the hull
	/// @param outEdges On success this will contain the list of indices that form the hull (counter clockwise)
	/// @return Status code that reports if the hull was created or not
	EResult				Initialize(int inIdx1, int inIdx2, int inIdx3, int inMaxVertices, float inTolerance, Edges &outEdges);

private:
#ifdef JPH_CONVEX_BUILDER_2D_DEBUG
	/// Factor to scale convex hull when debug drawing the construction process
	static constexpr Real cDrawScale = 10;
#endif

	class Edge;

	/// Frees all edges
	void				FreeEdges();

	/// Assigns a position to one of the supplied edges based on which edge is closest.
	/// @param inPositionIdx Index of the position to add
	/// @param inEdges List of edges to consider
	void				AssignPointToEdge(int inPositionIdx, const Array<Edge *> &inEdges) const;

#ifdef JPH_CONVEX_BUILDER_2D_DEBUG
	/// Draw state of algorithm
	void				DrawState();
#endif

#ifdef JPH_ENABLE_ASSERTS
	/// Validate that the edge structure is intact
	void				ValidateEdges() const;
#endif

	using ConflictList = Array<int>;

	/// Linked list of edges
	class Edge
	{
	public:
		JPH_OVERRIDE_NEW_DELETE

		/// Constructor
		explicit		Edge(int inStartIdx)						: mStartIdx(inStartIdx) { }

		/// Calculate the center of the edge and the edge normal
		void			CalculateNormalAndCenter(const Vec3 *inPositions);

		/// Check if this edge is facing inPosition
		inline bool		IsFacing(Vec3Arg inPosition) const			{ return mNormal.Dot(inPosition - mCenter) > 0.0f; }

		Vec3			mNormal;									///< Normal of the edge (not normalized)
		Vec3			mCenter;									///< Center of the edge
		ConflictList	mConflictList;								///< Positions associated with this edge (that are closest to this edge). Last entry is the one furthest away from the edge, remainder is unsorted.
		Edge *			mPrevEdge = nullptr;						///< Previous edge in circular list
		Edge *			mNextEdge = nullptr;						///< Next edge in circular list
		int				mStartIdx;									///< Position index of start of this edge
		float			mFurthestPointDistanceSq = 0.0f;			///< Squared distance of furthest point from the conflict list to the edge
	};

	const Positions &	mPositions;									///< List of positions (some of them are part of the hull)
	Edge *				mFirstEdge = nullptr;						///< First edge of the hull
	int					mNumEdges = 0;								///< Number of edges in hull

#ifdef JPH_CONVEX_BUILDER_2D_DEBUG
	RVec3				mOffset;									///< Offset to use for state drawing
	Vec3				mDelta;										///< Delta offset between next states
#endif
};

JPH_NAMESPACE_END

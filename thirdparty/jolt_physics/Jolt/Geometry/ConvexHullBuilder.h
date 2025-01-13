// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

//#define JPH_CONVEX_BUILDER_DEBUG
//#define JPH_CONVEX_BUILDER_DUMP_SHAPE

#ifdef JPH_CONVEX_BUILDER_DEBUG
	#include <Jolt/Core/Color.h>
#endif

#include <Jolt/Core/StaticArray.h>
#include <Jolt/Core/NonCopyable.h>

JPH_NAMESPACE_BEGIN

/// A convex hull builder that tries to create hulls as accurately as possible. Used for offline processing.
class JPH_EXPORT ConvexHullBuilder : public NonCopyable
{
public:
	// Forward declare
	class Face;

	/// Class that holds the information of an edge
	class Edge : public NonCopyable
	{
	public:
		JPH_OVERRIDE_NEW_DELETE

		/// Constructor
						Edge(Face *inFace, int inStartIdx)	: mFace(inFace), mStartIdx(inStartIdx) { }

		/// Get the previous edge
		inline Edge *	GetPreviousEdge()
		{
			Edge *prev_edge = this;
			while (prev_edge->mNextEdge != this)
				prev_edge = prev_edge->mNextEdge;
			return prev_edge;
		}

		Face *			mFace;								///< Face that this edge belongs to
		Edge *			mNextEdge = nullptr;				///< Next edge of this face
		Edge *			mNeighbourEdge = nullptr;			///< Edge that this edge is connected to
		int				mStartIdx;							///< Vertex index in mPositions that indicates the start vertex of this edge
	};

	using ConflictList = Array<int>;

	/// Class that holds the information of one face
	class Face : public NonCopyable
	{
	public:
		JPH_OVERRIDE_NEW_DELETE

		/// Destructor
						~Face();

		/// Initialize a face with three indices
		void			Initialize(int inIdx0, int inIdx1, int inIdx2, const Vec3 *inPositions);

		/// Calculates the centroid and normal for this face
		void			CalculateNormalAndCentroid(const Vec3 *inPositions);

		/// Check if face inFace is facing inPosition
		inline bool		IsFacing(Vec3Arg inPosition) const
		{
			JPH_ASSERT(!mRemoved);
			return mNormal.Dot(inPosition - mCentroid) > 0.0f;
		}

		Vec3			mNormal;							///< Normal of this face, length is 2 times area of face
		Vec3			mCentroid;							///< Center of the face
		ConflictList	mConflictList;						///< Positions associated with this edge (that are closest to this edge). The last position in the list is the point that is furthest away from the face.
		Edge *			mFirstEdge = nullptr;				///< First edge of this face
		float			mFurthestPointDistanceSq = 0.0f;	///< Squared distance of furthest point from the conflict list to the face
		bool			mRemoved = false;					///< Flag that indicates that face has been removed (face will be freed later)
#ifdef JPH_CONVEX_BUILDER_DEBUG
		int				mIteration;							///< Iteration that this face was created
#endif
	};

	// Typedefs
	using Positions = Array<Vec3>;
	using Faces = Array<Face *>;

	/// Constructor
	explicit			ConvexHullBuilder(const Positions &inPositions);

	/// Destructor
						~ConvexHullBuilder()				{ FreeFaces(); }

	/// Result enum that indicates how the hull got created
	enum class EResult
	{
		Success,											///< Hull building finished successfully
		MaxVerticesReached,									///< Hull building finished successfully, but the desired accuracy was not reached because the max vertices limit was reached
		TooFewPoints,										///< Too few points to create a hull
		TooFewFaces,										///< Too few faces in the created hull (signifies precision errors during building)
		Degenerate,											///< Degenerate hull detected
	};

	/// Takes all positions as provided by the constructor and use them to build a hull
	/// Any points that are closer to the hull than inTolerance will be discarded
	/// @param inMaxVertices Max vertices to allow in the hull. Specify INT_MAX if there is no limit.
	/// @param inTolerance Max distance that a point is allowed to be outside of the hull
	/// @param outError Error message when building fails
	/// @return Status code that reports if the hull was created or not
	EResult				Initialize(int inMaxVertices, float inTolerance, const char *&outError);

	/// Returns the amount of vertices that are currently used by the hull
	int					GetNumVerticesUsed() const;

	/// Returns true if the hull contains a polygon with inIndices (counter clockwise indices in mPositions)
	bool				ContainsFace(const Array<int> &inIndices) const;

	/// Calculate the center of mass and the volume of the current convex hull
	void				GetCenterOfMassAndVolume(Vec3 &outCenterOfMass, float &outVolume) const;

	/// Determines the point that is furthest outside of the hull and reports how far it is outside of the hull (which indicates a failure during hull building)
	/// @param outFaceWithMaxError The face that caused the error
	/// @param outMaxError The maximum distance of a point to the hull
	/// @param outMaxErrorPositionIdx The index of the point that had this distance
	/// @param outCoplanarDistance Points that are less than this distance from the hull are considered on the hull. This should be used as a lowerbound for the allowed error.
	void				DetermineMaxError(Face *&outFaceWithMaxError, float &outMaxError, int &outMaxErrorPositionIdx, float &outCoplanarDistance) const;

	/// Access to the created faces. Memory is owned by the convex hull builder.
	const Faces &		GetFaces() const					{ return mFaces; }

private:
	/// Minimal square area of a triangle (used for merging and checking if a triangle is degenerate)
	static constexpr float cMinTriangleAreaSq = 1.0e-12f;

#ifdef JPH_CONVEX_BUILDER_DEBUG
	/// Factor to scale convex hull when debug drawing the construction process
	static constexpr Real cDrawScale = 10;
#endif

	/// Class that holds an edge including start and end index
	class FullEdge
	{
	public:
		Edge *			mNeighbourEdge;						///< Edge that this edge is connected to
		int				mStartIdx;							///< Vertex index in mPositions that indicates the start vertex of this edge
		int				mEndIdx;							///< Vertex index in mPosition that indicates the end vertex of this edge
	};

	// Private typedefs
	using FullEdges = Array<FullEdge>;

	// Determine a suitable tolerance for detecting that points are coplanar
	float				DetermineCoplanarDistance() const;

	/// Find the face for which inPoint is furthest to the front
	/// @param inPoint Point to test
	/// @param inFaces List of faces to test
	/// @param outFace Returns the best face
	/// @param outDistSq Returns the squared distance how much inPoint is in front of the plane of the face
	void				GetFaceForPoint(Vec3Arg inPoint, const Faces &inFaces, Face *&outFace, float &outDistSq) const;

	/// @brief Calculates the distance between inPoint and inFace
	/// @param inFace Face to test
	/// @param inPoint Point to test
	/// @return If the projection of the point on the plane is interior to the face 0, otherwise the squared distance to the closest edge
	float				GetDistanceToEdgeSq(Vec3Arg inPoint, const Face *inFace) const;

	/// Assigns a position to one of the supplied faces based on which face is closest.
	/// @param inPositionIdx Index of the position to add
	/// @param inFaces List of faces to consider
	/// @param inToleranceSq Tolerance of the hull, if the point is closer to the face than this, we ignore it
	/// @return True if point was assigned, false if it was discarded or added to the coplanar list
	bool				AssignPointToFace(int inPositionIdx, const Faces &inFaces, float inToleranceSq);

	/// Add a new point to the convex hull
	void				AddPoint(Face *inFacingFace, int inIdx, float inToleranceSq, Faces &outNewFaces);

	/// Remove all faces that have been marked 'removed' from mFaces list
	void				GarbageCollectFaces();

	/// Create a new face
	Face *				CreateFace();

	/// Create a new triangle
	Face *				CreateTriangle(int inIdx1, int inIdx2, int inIdx3);

	/// Delete a face (checking that it is not connected to any other faces)
	void				FreeFace(Face *inFace);

	/// Release all faces and edges
	void				FreeFaces();

	/// Link face edge to other face edge
	static void			sLinkFace(Edge *inEdge1, Edge *inEdge2);

	/// Unlink this face from all of its neighbours
	static void			sUnlinkFace(Face *inFace);

	/// Given one face that faces inVertex, find the edges of the faces that are not facing inVertex.
	/// Will flag all those faces for removal.
	void				FindEdge(Face *inFacingFace, Vec3Arg inVertex, FullEdges &outEdges) const;

	/// Merges the two faces that share inEdge into the face inEdge->mFace
	void				MergeFaces(Edge *inEdge);

	/// Merges inFace with a neighbour if it is degenerate (a sliver)
	void				MergeDegenerateFace(Face *inFace, Faces &ioAffectedFaces);

	/// Merges any coplanar as well as neighbours that form a non-convex edge into inFace.
	/// Faces are considered coplanar if the distance^2 of the other face's centroid is smaller than inToleranceSq.
	void				MergeCoplanarOrConcaveFaces(Face *inFace, float inToleranceSq, Faces &ioAffectedFaces);

	/// Mark face as affected if it is not already in the list
	static void			sMarkAffected(Face *inFace, Faces &ioAffectedFaces);

	/// Removes all invalid edges.
	/// 1. Merges inFace with faces that share two edges with it since this means inFace or the other face cannot be convex or the edge is colinear.
	/// 2. Removes edges that are interior to inFace (that have inFace on both sides)
	/// Any faces that need to be checked for validity will be added to ioAffectedFaces.
	void				RemoveInvalidEdges(Face *inFace, Faces &ioAffectedFaces);

	/// Removes inFace if it consists of only 2 edges, linking its neighbouring faces together
	/// Any faces that need to be checked for validity will be added to ioAffectedFaces.
	/// @return True if face was removed.
	bool				RemoveTwoEdgeFace(Face *inFace, Faces &ioAffectedFaces) const;

#ifdef JPH_ENABLE_ASSERTS
	/// Dumps the text representation of a face to the TTY
	void				DumpFace(const Face *inFace) const;

	/// Dumps the text representation of all faces to the TTY
	void				DumpFaces() const;

	/// Check consistency of 1 face
	void				ValidateFace(const Face *inFace) const;

	/// Check consistency of all faces
	void				ValidateFaces() const;
#endif

#ifdef JPH_CONVEX_BUILDER_DEBUG
	/// Draw state of algorithm
	void				DrawState(bool inDrawConflictList = false) const;

	/// Draw a face for debugging purposes
	void				DrawWireFace(const Face *inFace, ColorArg inColor) const;

	/// Draw an edge for debugging purposes
	void				DrawEdge(const Edge *inEdge, ColorArg inColor) const;
#endif

#ifdef JPH_CONVEX_BUILDER_DUMP_SHAPE
	void				DumpShape() const;
#endif

	const Positions &	mPositions;							///< List of positions (some of them are part of the hull)
	Faces				mFaces;								///< List of faces that are part of the hull (if !mRemoved)

	struct Coplanar
	{
		int				mPositionIdx;						///< Index in mPositions
		float			mDistanceSq;						///< Distance to the edge of closest face (should be > 0)
	};
	using CoplanarList = Array<Coplanar>;

	CoplanarList		mCoplanarList;						///< List of positions that are coplanar to a face but outside of the face, these are added to the hull at the end

#ifdef JPH_CONVEX_BUILDER_DEBUG
	int					mIteration;							///< Number of iterations we've had so far (for debug purposes)
	mutable RVec3		mOffset;							///< Offset to use for state drawing
	Vec3				mDelta;								///< Delta offset between next states
#endif
};

JPH_NAMESPACE_END

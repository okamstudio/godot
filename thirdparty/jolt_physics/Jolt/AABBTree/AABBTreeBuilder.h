// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Jolt/TriangleSplitter/TriangleSplitter.h>
#include <Jolt/Geometry/AABox.h>
#include <Jolt/Core/NonCopyable.h>

JPH_NAMESPACE_BEGIN

struct AABBTreeBuilderStats
{
	///@name Splitter stats
	TriangleSplitter::Stats	mSplitterStats;							///< Stats returned by the triangle splitter algorithm

	///@name Tree structure
	float					mSAHCost = 0.0f;						///< Surface Area Heuristic cost of this tree
	int						mMinDepth = 0;							///< Minimal depth of tree (number of nodes)
	int						mMaxDepth = 0;							///< Maximum depth of tree (number of nodes)
	int						mNodeCount = 0;							///< Number of nodes in the tree
	int						mLeafNodeCount = 0;						///< Number of leaf nodes (that contain triangles)

	///@name Configured stats
	int						mMaxTrianglesPerLeaf = 0;				///< Configured max triangles per leaf

	///@name Actual stats
	int						mTreeMinTrianglesPerLeaf = 0;			///< Minimal amount of triangles in a leaf
	int						mTreeMaxTrianglesPerLeaf = 0;			///< Maximal amount of triangles in a leaf
	float					mTreeAvgTrianglesPerLeaf = 0.0f;		///< Average amount of triangles in leaf nodes
};

/// Helper class to build an AABB tree
class JPH_EXPORT AABBTreeBuilder
{
public:
	/// A node in the tree, contains the AABox for the tree and any child nodes or triangles
	class Node
	{
	public:
		JPH_OVERRIDE_NEW_DELETE

		/// Indicates that there is no child
		static constexpr uint cInvalidNodeIndex = ~uint(0);

		/// Get number of triangles in this node
		inline uint			GetTriangleCount() const				{ return mNumTriangles; }

		/// Check if this node has any children
		inline bool			HasChildren() const						{ return mChild[0] != cInvalidNodeIndex || mChild[1] != cInvalidNodeIndex; }

		/// Min depth of tree
		uint				GetMinDepth(const Array<Node> &inNodes) const;

		/// Max depth of tree
		uint				GetMaxDepth(const Array<Node> &inNodes) const;

		/// Number of nodes in tree
		uint				GetNodeCount(const Array<Node> &inNodes) const;

		/// Number of leaf nodes in tree
		uint				GetLeafNodeCount(const Array<Node> &inNodes) const;

		/// Get triangle count in tree
		uint				GetTriangleCountInTree(const Array<Node> &inNodes) const;

		/// Calculate min and max triangles per node
		void				GetTriangleCountPerNode(const Array<Node> &inNodes, float &outAverage, uint &outMin, uint &outMax) const;

		/// Calculate the total cost of the tree using the surface area heuristic
		float				CalculateSAHCost(const Array<Node> &inNodes, float inCostTraversal, float inCostLeaf) const;

		/// Recursively get children (breadth first) to get in total inN children (or less if there are no more)
		void				GetNChildren(const Array<Node> &inNodes, uint inN, Array<const Node *> &outChildren) const;

		/// Bounding box
		AABox				mBounds;

		/// Triangles (if no child nodes)
		uint				mTrianglesBegin; // Index into mTriangles
		uint				mNumTriangles = 0;

		/// Child node indices (if no triangles)
		uint				mChild[2] = { cInvalidNodeIndex, cInvalidNodeIndex };

	private:
		friend class AABBTreeBuilder;

		/// Recursive helper function to calculate cost of the tree
		float				CalculateSAHCostInternal(const Array<Node> &inNodes, float inCostTraversalDivSurfaceArea, float inCostLeafDivSurfaceArea) const;

		/// Recursive helper function to calculate min and max triangles per node
		void				GetTriangleCountPerNodeInternal(const Array<Node> &inNodes, float &outAverage, uint &outAverageDivisor, uint &outMin, uint &outMax) const;
	};

	/// Constructor
							AABBTreeBuilder(TriangleSplitter &inSplitter, uint inMaxTrianglesPerLeaf = 16);

	/// Recursively build tree, returns the root node of the tree
	Node *					Build(AABBTreeBuilderStats &outStats);

	/// Get all nodes
	const Array<Node> &		GetNodes() const						{ return mNodes; }

	/// Get all triangles
	const Array<IndexedTriangle> &GetTriangles() const				{ return mTriangles; }

private:
	uint					BuildInternal(const TriangleSplitter::Range &inTriangles);

	TriangleSplitter &		mTriangleSplitter;
	const uint				mMaxTrianglesPerLeaf;
	Array<Node>				mNodes;
	Array<IndexedTriangle>	mTriangles;
};

JPH_NAMESPACE_END

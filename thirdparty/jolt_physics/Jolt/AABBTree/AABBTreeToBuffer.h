// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Jolt/AABBTree/AABBTreeBuilder.h>
#include <Jolt/Core/ByteBuffer.h>
#include <Jolt/Geometry/IndexedTriangle.h>

JPH_NAMESPACE_BEGIN

/// Conversion algorithm that converts an AABB tree to an optimized binary buffer
template <class TriangleCodec, class NodeCodec>
class AABBTreeToBuffer
{
public:
	/// Header for the tree
	using NodeHeader = typename NodeCodec::Header;

	/// Size in bytes of the header of the tree
	static const int HeaderSize = NodeCodec::HeaderSize;

	/// Maximum number of children per node in the tree
	static const int NumChildrenPerNode = NodeCodec::NumChildrenPerNode;

	/// Header for the triangles
	using TriangleHeader = typename TriangleCodec::TriangleHeader;

	/// Size in bytes of the header for the triangles
	static const int TriangleHeaderSize = TriangleCodec::TriangleHeaderSize;

	/// Convert AABB tree. Returns false if failed.
	bool							Convert(const Array<IndexedTriangle> &inTriangles, const Array<AABBTreeBuilder::Node> &inNodes, const VertexList &inVertices, const AABBTreeBuilder::Node *inRoot, bool inStoreUserData, const char *&outError)
	{
		typename NodeCodec::EncodingContext node_ctx;
		typename TriangleCodec::EncodingContext tri_ctx(inVertices);

		// Child nodes out of loop so we don't constantly realloc it
		Array<const AABBTreeBuilder::Node *> child_nodes;
		child_nodes.reserve(NumChildrenPerNode);

		// First calculate how big the tree is going to be.
		// Since the tree can be huge for very large meshes, we don't want
		// to reallocate the buffer as it may cause out of memory situations.
		// This loop mimics the construction loop below.
		uint64 total_size = HeaderSize + TriangleHeaderSize;
		size_t node_count = 1; // Start with root node
		size_t to_process_max_size = 1; // Track size of queues so we can do a single reserve below
		size_t to_process_triangles_max_size = 0;
		{	// A scope to free the memory associated with to_estimate and to_estimate_triangles
			Array<const AABBTreeBuilder::Node *> to_estimate;
			Array<const AABBTreeBuilder::Node *> to_estimate_triangles;
			to_estimate.push_back(inRoot);
			for (;;)
			{
				while (!to_estimate.empty())
				{
					// Get the next node to process
					const AABBTreeBuilder::Node *node = to_estimate.back();
					to_estimate.pop_back();

					// Update total size
					node_ctx.PrepareNodeAllocate(node, total_size);

					if (node->HasChildren())
					{
						// Collect the first NumChildrenPerNode sub-nodes in the tree
						child_nodes.clear(); // Won't free the memory
						node->GetNChildren(inNodes, NumChildrenPerNode, child_nodes);

						// Increment the number of nodes we're going to store
						node_count += child_nodes.size();

						// Insert in reverse order so we estimate left child first when taking nodes from the back
						for (int idx = int(child_nodes.size()) - 1; idx >= 0; --idx)
						{
							// Store triangles in separate list so we process them last
							const AABBTreeBuilder::Node *child = child_nodes[idx];
							if (child->HasChildren())
							{
								to_estimate.push_back(child);
								to_process_max_size = max(to_estimate.size(), to_process_max_size);
							}
							else
							{
								to_estimate_triangles.push_back(child);
								to_process_triangles_max_size = max(to_estimate_triangles.size(), to_process_triangles_max_size);
							}
						}
					}
					else
					{
						// Update total size
						tri_ctx.PreparePack(&inTriangles[node->mTrianglesBegin], node->mNumTriangles, inStoreUserData, total_size);
					}
				}

				// If we've got triangles to estimate, loop again with just the triangles
				if (to_estimate_triangles.empty())
					break;
				else
					to_estimate.swap(to_estimate_triangles);
			}
		}

		// Finalize the prepare stage for the triangle context
		tri_ctx.FinalizePreparePack(total_size);

		// Reserve the buffer
		if (size_t(total_size) != total_size)
		{
			outError = "AABBTreeToBuffer: Out of memory!";
			return false;
		}
		mTree.reserve(size_t(total_size));

		// Add headers
		NodeHeader *header = HeaderSize > 0? mTree.Allocate<NodeHeader>() : nullptr;
		TriangleHeader *triangle_header = TriangleHeaderSize > 0? mTree.Allocate<TriangleHeader>() : nullptr;

		struct NodeData
		{
			const AABBTreeBuilder::Node *	mNode = nullptr;							// Node that this entry belongs to
			Vec3							mNodeBoundsMin;								// Quantized node bounds
			Vec3							mNodeBoundsMax;
			size_t							mNodeStart = size_t(-1);					// Start of node in mTree
			size_t							mTriangleStart = size_t(-1);				// Start of the triangle data in mTree
			size_t							mChildNodeStart[NumChildrenPerNode];		// Start of the children of the node in mTree
			size_t							mChildTrianglesStart[NumChildrenPerNode];	// Start of the triangle data in mTree
			size_t *						mParentChildNodeStart = nullptr;			// Where to store mNodeStart (to patch mChildNodeStart of my parent)
			size_t *						mParentTrianglesStart = nullptr;			// Where to store mTriangleStart (to patch mChildTrianglesStart of my parent)
			uint							mNumChildren = 0;							// Number of children
		};

		Array<NodeData *> to_process;
		to_process.reserve(to_process_max_size);
		Array<NodeData *> to_process_triangles;
		to_process_triangles.reserve(to_process_triangles_max_size);
		Array<NodeData> node_list;
		node_list.reserve(node_count); // Needed to ensure that array is not reallocated, so we can keep pointers in the array

		NodeData root;
		root.mNode = inRoot;
		root.mNodeBoundsMin = inRoot->mBounds.mMin;
		root.mNodeBoundsMax = inRoot->mBounds.mMax;
		node_list.push_back(root);
		to_process.push_back(&node_list.back());

		for (;;)
		{
			while (!to_process.empty())
			{
				// Get the next node to process
				NodeData *node_data = to_process.back();
				to_process.pop_back();

				// Due to quantization box could have become bigger, not smaller
				JPH_ASSERT(AABox(node_data->mNodeBoundsMin, node_data->mNodeBoundsMax).Contains(node_data->mNode->mBounds), "AABBTreeToBuffer: Bounding box became smaller!");

				// Collect the first NumChildrenPerNode sub-nodes in the tree
				child_nodes.clear(); // Won't free the memory
				node_data->mNode->GetNChildren(inNodes, NumChildrenPerNode, child_nodes);
				node_data->mNumChildren = (uint)child_nodes.size();

				// Fill in default child bounds
				Vec3 child_bounds_min[NumChildrenPerNode], child_bounds_max[NumChildrenPerNode];
				for (size_t i = 0; i < NumChildrenPerNode; ++i)
					if (i < child_nodes.size())
					{
						child_bounds_min[i] = child_nodes[i]->mBounds.mMin;
						child_bounds_max[i] = child_nodes[i]->mBounds.mMax;
					}
					else
					{
						child_bounds_min[i] = Vec3::sZero();
						child_bounds_max[i] = Vec3::sZero();
					}

				// Start a new node
				node_data->mNodeStart = node_ctx.NodeAllocate(node_data->mNode, node_data->mNodeBoundsMin, node_data->mNodeBoundsMax, child_nodes, child_bounds_min, child_bounds_max, mTree, outError);
				if (node_data->mNodeStart == size_t(-1))
					return false;

				if (node_data->mNode->HasChildren())
				{
					// Insert in reverse order so we process left child first when taking nodes from the back
					for (int idx = int(child_nodes.size()) - 1; idx >= 0; --idx)
					{
						const AABBTreeBuilder::Node *child_node = child_nodes[idx];

						// Due to quantization box could have become bigger, not smaller
						JPH_ASSERT(AABox(child_bounds_min[idx], child_bounds_max[idx]).Contains(child_node->mBounds), "AABBTreeToBuffer: Bounding box became smaller!");

						// Add child to list of nodes to be processed
						NodeData child;
						child.mNode = child_node;
						child.mNodeBoundsMin = child_bounds_min[idx];
						child.mNodeBoundsMax = child_bounds_max[idx];
						child.mParentChildNodeStart = &node_data->mChildNodeStart[idx];
						child.mParentTrianglesStart = &node_data->mChildTrianglesStart[idx];
						node_list.push_back(child);

						// Store triangles in separate list so we process them last
						if (child_node->HasChildren())
							to_process.push_back(&node_list.back());
						else
							to_process_triangles.push_back(&node_list.back());
					}
				}
				else
				{
					// Add triangles
					node_data->mTriangleStart = tri_ctx.Pack(&inTriangles[node_data->mNode->mTrianglesBegin], node_data->mNode->mNumTriangles, inStoreUserData, mTree, outError);
					if (node_data->mTriangleStart == size_t(-1))
						return false;
				}

				// Patch offset into parent
				if (node_data->mParentChildNodeStart != nullptr)
				{
					*node_data->mParentChildNodeStart = node_data->mNodeStart;
					*node_data->mParentTrianglesStart = node_data->mTriangleStart;
				}
			}

			// If we've got triangles to process, loop again with just the triangles
			if (to_process_triangles.empty())
				break;
			else
				to_process.swap(to_process_triangles);
		}

		// Assert that our reservation was correct (we don't know if we swapped the arrays or not)
		JPH_ASSERT(to_process_max_size == to_process.capacity() || to_process_triangles_max_size == to_process.capacity());
		JPH_ASSERT(to_process_max_size == to_process_triangles.capacity() || to_process_triangles_max_size == to_process_triangles.capacity());

		// Finalize all nodes
		for (NodeData &n : node_list)
			if (!node_ctx.NodeFinalize(n.mNode, n.mNodeStart, n.mNumChildren, n.mChildNodeStart, n.mChildTrianglesStart, mTree, outError))
				return false;

		// Finalize the triangles
		tri_ctx.Finalize(inVertices, triangle_header, mTree);

		// Validate that our reservations were correct
		if (node_count != node_list.size())
		{
			outError = "Internal Error: Node memory estimate was incorrect, memory corruption!";
			return false;
		}
		if (total_size != mTree.size())
		{
			outError = "Internal Error: Tree memory estimate was incorrect, memory corruption!";
			return false;
		}

		// Finalize the nodes
		return node_ctx.Finalize(header, inRoot, node_list[0].mNodeStart, node_list[0].mTriangleStart, outError);
	}

	/// Get resulting data
	inline const ByteBuffer &		GetBuffer() const
	{
		return mTree;
	}

	/// Get resulting data
	inline ByteBuffer &				GetBuffer()
	{
		return mTree;
	}

	/// Get header for tree
	inline const NodeHeader *		GetNodeHeader() const
	{
		return mTree.Get<NodeHeader>(0);
	}

	/// Get header for triangles
	inline const TriangleHeader *	GetTriangleHeader() const
	{
		return mTree.Get<TriangleHeader>(HeaderSize);
	}

	/// Get root of resulting tree
	inline const void *				GetRoot() const
	{
		return mTree.Get<void>(HeaderSize + TriangleHeaderSize);
	}

private:
	ByteBuffer						mTree;									///< Resulting tree structure
};

JPH_NAMESPACE_END

// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Jolt/Core/ByteBuffer.h>
#include <Jolt/Math/HalfFloat.h>
#include <Jolt/AABBTree/AABBTreeBuilder.h>

JPH_NAMESPACE_BEGIN

class NodeCodecQuadTreeHalfFloat
{
public:
	/// Number of child nodes of this node
	static constexpr int				NumChildrenPerNode = 4;

	/// Header for the tree
	struct Header
	{
		Float3							mRootBoundsMin;
		Float3							mRootBoundsMax;
		uint32							mRootProperties;
		uint8							mBlockIDBits;			///< Number of bits to address a triangle block
		uint8							mPadding[3] = { 0 };
	};

	/// Size of the header (an empty struct is always > 0 bytes so this needs a separate variable)
	static constexpr int				HeaderSize = sizeof(Header);

	/// Stack size to use during DecodingContext::sWalkTree
	static constexpr int				StackSize = 128;

	/// Node properties
	enum : uint32
	{
		TRIANGLE_COUNT_BITS				= 4,
		TRIANGLE_COUNT_SHIFT			= 28,
		TRIANGLE_COUNT_MASK				= (1 << TRIANGLE_COUNT_BITS) - 1,
		OFFSET_BITS						= 28,
		OFFSET_MASK						= (1 << OFFSET_BITS) - 1,
		OFFSET_NON_SIGNIFICANT_BITS		= 2,
		OFFSET_NON_SIGNIFICANT_MASK		= (1 << OFFSET_NON_SIGNIFICANT_BITS) - 1,
	};

	/// Node structure
	struct Node
	{
		HalfFloat						mBoundsMinX[4];			///< 4 child bounding boxes
		HalfFloat						mBoundsMinY[4];
		HalfFloat						mBoundsMinZ[4];
		HalfFloat						mBoundsMaxX[4];
		HalfFloat						mBoundsMaxY[4];
		HalfFloat						mBoundsMaxZ[4];
		uint32							mNodeProperties[4];		///< 4 child node properties
	};

	static_assert(sizeof(Node) == 64, "Node should be 64 bytes");

	/// This class encodes and compresses quad tree nodes
	class EncodingContext
	{
	public:
		/// Mimics the size a call to NodeAllocate() would add to the buffer
		void							PrepareNodeAllocate(const AABBTreeBuilder::Node *inNode, uint64 &ioBufferSize) const
		{
			// We don't emit nodes for leafs
			if (!inNode->HasChildren())
				return;

			// Add size of node
			ioBufferSize += sizeof(Node);
		}

		/// Allocate a new node for inNode.
		/// Algorithm can modify the order of ioChildren to indicate in which order children should be compressed
		/// Algorithm can enlarge the bounding boxes of the children during compression and returns these in outChildBoundsMin, outChildBoundsMax
		/// inNodeBoundsMin, inNodeBoundsMax is the bounding box if inNode possibly widened by compressing the parent node
		/// Returns size_t(-1) on error and reports the error in outError
		size_t							NodeAllocate(const AABBTreeBuilder::Node *inNode, Vec3Arg inNodeBoundsMin, Vec3Arg inNodeBoundsMax, Array<const AABBTreeBuilder::Node *> &ioChildren, Vec3 outChildBoundsMin[NumChildrenPerNode], Vec3 outChildBoundsMax[NumChildrenPerNode], ByteBuffer &ioBuffer, const char *&outError) const
		{
			// We don't emit nodes for leafs
			if (!inNode->HasChildren())
				return ioBuffer.size();

			// Remember the start of the node
			size_t node_start = ioBuffer.size();

			// Fill in bounds
			Node *node = ioBuffer.Allocate<Node>();

			for (size_t i = 0; i < 4; ++i)
			{
				if (i < ioChildren.size())
				{
					const AABBTreeBuilder::Node *this_node = ioChildren[i];

					// Copy bounding box
					node->mBoundsMinX[i] = HalfFloatConversion::FromFloat<HalfFloatConversion::ROUND_TO_NEG_INF>(this_node->mBounds.mMin.GetX());
					node->mBoundsMinY[i] = HalfFloatConversion::FromFloat<HalfFloatConversion::ROUND_TO_NEG_INF>(this_node->mBounds.mMin.GetY());
					node->mBoundsMinZ[i] = HalfFloatConversion::FromFloat<HalfFloatConversion::ROUND_TO_NEG_INF>(this_node->mBounds.mMin.GetZ());
					node->mBoundsMaxX[i] = HalfFloatConversion::FromFloat<HalfFloatConversion::ROUND_TO_POS_INF>(this_node->mBounds.mMax.GetX());
					node->mBoundsMaxY[i] = HalfFloatConversion::FromFloat<HalfFloatConversion::ROUND_TO_POS_INF>(this_node->mBounds.mMax.GetY());
					node->mBoundsMaxZ[i] = HalfFloatConversion::FromFloat<HalfFloatConversion::ROUND_TO_POS_INF>(this_node->mBounds.mMax.GetZ());

					// Store triangle count
					node->mNodeProperties[i] = this_node->GetTriangleCount() << TRIANGLE_COUNT_SHIFT;
					if (this_node->GetTriangleCount() >= TRIANGLE_COUNT_MASK)
					{
						outError = "NodeCodecQuadTreeHalfFloat: Too many triangles";
						return size_t(-1);
					}
				}
				else
				{
					// Make this an invalid triangle node
					node->mNodeProperties[i] = uint32(TRIANGLE_COUNT_MASK) << TRIANGLE_COUNT_SHIFT;

					// Make bounding box invalid
					node->mBoundsMinX[i] = HALF_FLT_MAX;
					node->mBoundsMinY[i] = HALF_FLT_MAX;
					node->mBoundsMinZ[i] = HALF_FLT_MAX;
					node->mBoundsMaxX[i] = HALF_FLT_MAX;
					node->mBoundsMaxY[i] = HALF_FLT_MAX;
					node->mBoundsMaxZ[i] = HALF_FLT_MAX;
				}
			}

			// Since we don't keep track of the bounding box while descending the tree, we keep the root bounds at all levels for triangle compression
			for (int i = 0; i < NumChildrenPerNode; ++i)
			{
				outChildBoundsMin[i] = inNodeBoundsMin;
				outChildBoundsMax[i] = inNodeBoundsMax;
			}

			return node_start;
		}

		/// Once all nodes have been added, this call finalizes all nodes by patching in the offsets of the child nodes (that were added after the node itself was added)
		bool						NodeFinalize(const AABBTreeBuilder::Node *inNode, size_t inNodeStart, uint inNumChildren, const size_t *inChildrenNodeStart, const size_t *inChildrenTrianglesStart, ByteBuffer &ioBuffer, const char *&outError)
		{
			if (!inNode->HasChildren())
				return true;

			Node *node = ioBuffer.Get<Node>(inNodeStart);
			for (uint i = 0; i < inNumChildren; ++i)
			{
				size_t offset;
				if (node->mNodeProperties[i] != 0)
				{
					// This is a triangle block
					offset = inChildrenTrianglesStart[i];

					// Store highest block with triangles so we can count the number of bits we need
					mHighestTriangleBlock = max(mHighestTriangleBlock, offset);
				}
				else
				{
					// This is a node block
					offset = inChildrenNodeStart[i];
				}

				// Store offset of next node / triangles
				if (offset & OFFSET_NON_SIGNIFICANT_MASK)
				{
					outError = "NodeCodecQuadTreeHalfFloat: Internal Error: Offset has non-significant bits set";
					return false;
				}
				offset >>= OFFSET_NON_SIGNIFICANT_BITS;
				if (offset > OFFSET_MASK)
				{
					outError = "NodeCodecQuadTreeHalfFloat: Offset too large. Too much data.";
					return false;
				}
				node->mNodeProperties[i] |= uint32(offset);
			}

			return true;
		}

		/// Once all nodes have been finalized, this will finalize the header of the nodes
		bool						Finalize(Header *outHeader, const AABBTreeBuilder::Node *inRoot, size_t inRootNodeStart, size_t inRootTrianglesStart, const char *&outError) const
		{
			// Check if we can address the root node
			size_t offset = inRoot->HasChildren()? inRootNodeStart : inRootTrianglesStart;
			if (offset & OFFSET_NON_SIGNIFICANT_MASK)
			{
				outError = "NodeCodecQuadTreeHalfFloat: Internal Error: Offset has non-significant bits set";
				return false;
			}
			offset >>= OFFSET_NON_SIGNIFICANT_BITS;
			if (offset > OFFSET_MASK)
			{
				outError = "NodeCodecQuadTreeHalfFloat: Offset too large. Too much data.";
				return false;
			}

			// If the root has triangles, we need to take that offset instead since the mHighestTriangleBlock will be zero
			size_t highest_triangle_block = inRootTrianglesStart != size_t(-1)? inRootTrianglesStart : mHighestTriangleBlock;
			highest_triangle_block >>= OFFSET_NON_SIGNIFICANT_BITS;

			inRoot->mBounds.mMin.StoreFloat3(&outHeader->mRootBoundsMin);
			inRoot->mBounds.mMax.StoreFloat3(&outHeader->mRootBoundsMax);
			outHeader->mRootProperties = uint32(offset) + (inRoot->GetTriangleCount() << TRIANGLE_COUNT_SHIFT);
			outHeader->mBlockIDBits = uint8(32 - CountLeadingZeros(uint32(highest_triangle_block)));
			if (inRoot->GetTriangleCount() >= TRIANGLE_COUNT_MASK)
			{
				outError = "NodeCodecQuadTreeHalfFloat: Too many triangles";
				return false;
			}

			return true;
		}

	private:
		size_t						mHighestTriangleBlock = 0;
	};

	/// This class decodes and decompresses quad tree nodes
	class DecodingContext
	{
	public:
		/// Get the amount of bits needed to store an ID to a triangle block
		inline static uint			sTriangleBlockIDBits(const Header *inHeader)
		{
			return inHeader->mBlockIDBits;
		}

		/// Convert a triangle block ID to the start of the triangle buffer
		inline static const void *	sGetTriangleBlockStart(const uint8 *inBufferStart, uint inTriangleBlockID)
		{
			return inBufferStart + (inTriangleBlockID << OFFSET_NON_SIGNIFICANT_BITS);
		}

		/// Constructor
		JPH_INLINE explicit			DecodingContext(const Header *inHeader)
		{
			// Start with the root node on the stack
			mNodeStack[0] = inHeader->mRootProperties;
		}

		/// Walk the node tree calling the Visitor::VisitNodes for each node encountered and Visitor::VisitTriangles for each triangle encountered
		template <class TriangleContext, class Visitor>
		JPH_INLINE void				WalkTree(const uint8 *inBufferStart, const TriangleContext &inTriangleContext, Visitor &ioVisitor)
		{
			do
			{
				// Test if node contains triangles
				uint32 node_properties = mNodeStack[mTop];
				uint32 tri_count = node_properties >> TRIANGLE_COUNT_SHIFT;
				if (tri_count == 0)
				{
					const Node *node = reinterpret_cast<const Node *>(inBufferStart + (node_properties << OFFSET_NON_SIGNIFICANT_BITS));

					// Unpack bounds
					UVec4 bounds_minxy = UVec4::sLoadInt4(reinterpret_cast<const uint32 *>(&node->mBoundsMinX[0]));
					Vec4 bounds_minx = HalfFloatConversion::ToFloat(bounds_minxy);
					Vec4 bounds_miny = HalfFloatConversion::ToFloat(bounds_minxy.Swizzle<SWIZZLE_Z, SWIZZLE_W, SWIZZLE_UNUSED, SWIZZLE_UNUSED>());

					UVec4 bounds_minzmaxx = UVec4::sLoadInt4(reinterpret_cast<const uint32 *>(&node->mBoundsMinZ[0]));
					Vec4 bounds_minz = HalfFloatConversion::ToFloat(bounds_minzmaxx);
					Vec4 bounds_maxx = HalfFloatConversion::ToFloat(bounds_minzmaxx.Swizzle<SWIZZLE_Z, SWIZZLE_W, SWIZZLE_UNUSED, SWIZZLE_UNUSED>());

					UVec4 bounds_maxyz = UVec4::sLoadInt4(reinterpret_cast<const uint32 *>(&node->mBoundsMaxY[0]));
					Vec4 bounds_maxy = HalfFloatConversion::ToFloat(bounds_maxyz);
					Vec4 bounds_maxz = HalfFloatConversion::ToFloat(bounds_maxyz.Swizzle<SWIZZLE_Z, SWIZZLE_W, SWIZZLE_UNUSED, SWIZZLE_UNUSED>());

					// Load properties for 4 children
					UVec4 properties = UVec4::sLoadInt4(&node->mNodeProperties[0]);

					// Check which sub nodes to visit
					int num_results = ioVisitor.VisitNodes(bounds_minx, bounds_miny, bounds_minz, bounds_maxx, bounds_maxy, bounds_maxz, properties, mTop);

					// Push them onto the stack
					JPH_ASSERT(mTop + 4 < StackSize);
					properties.StoreInt4(&mNodeStack[mTop]);
					mTop += num_results;
				}
				else if (tri_count != TRIANGLE_COUNT_MASK) // TRIANGLE_COUNT_MASK indicates a padding node, normally we shouldn't visit these nodes but when querying with a big enough box you could touch HALF_FLT_MAX (about 65K)
				{
					// Node contains triangles, do individual tests
					uint32 triangle_block_id = node_properties & OFFSET_MASK;
					const void *triangles = sGetTriangleBlockStart(inBufferStart, triangle_block_id);

					ioVisitor.VisitTriangles(inTriangleContext, triangles, tri_count, triangle_block_id);
				}

				// Check if we're done
				if (ioVisitor.ShouldAbort())
					break;

				// Fetch next node until we find one that the visitor wants to see
				do
					--mTop;
				while (mTop >= 0 && !ioVisitor.ShouldVisitNode(mTop));
			}
			while (mTop >= 0);
		}

		/// This can be used to have the visitor early out (ioVisitor.ShouldAbort() returns true) and later continue again (call WalkTree() again)
		bool						IsDoneWalking() const
		{
			return mTop < 0;
		}

	private:
		uint32						mNodeStack[StackSize];
		int							mTop = 0;
	};
};

JPH_NAMESPACE_END

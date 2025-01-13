// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#include <Jolt/Jolt.h>

#include <Jolt/Physics/Collision/Shape/MeshShape.h>
#include <Jolt/Physics/Collision/Shape/ConvexShape.h>
#include <Jolt/Physics/Collision/Shape/ScaleHelpers.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/ShapeCast.h>
#include <Jolt/Physics/Collision/ShapeFilter.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/CollideConvexVsTriangles.h>
#include <Jolt/Physics/Collision/CollideSphereVsTriangles.h>
#include <Jolt/Physics/Collision/CastConvexVsTriangles.h>
#include <Jolt/Physics/Collision/CastSphereVsTriangles.h>
#include <Jolt/Physics/Collision/TransformedShape.h>
#include <Jolt/Physics/Collision/ActiveEdges.h>
#include <Jolt/Physics/Collision/CollisionDispatch.h>
#include <Jolt/Physics/Collision/SortReverseAndStore.h>
#include <Jolt/Physics/Collision/CollideSoftBodyVerticesVsTriangles.h>
#include <Jolt/Core/StringTools.h>
#include <Jolt/Core/StreamIn.h>
#include <Jolt/Core/StreamOut.h>
#include <Jolt/Core/Profiler.h>
#include <Jolt/Core/UnorderedMap.h>
#include <Jolt/Geometry/AABox4.h>
#include <Jolt/Geometry/RayAABox.h>
#include <Jolt/Geometry/Indexify.h>
#include <Jolt/Geometry/Plane.h>
#include <Jolt/Geometry/OrientedBox.h>
#include <Jolt/TriangleSplitter/TriangleSplitterBinning.h>
#include <Jolt/AABBTree/AABBTreeBuilder.h>
#include <Jolt/AABBTree/AABBTreeToBuffer.h>
#include <Jolt/AABBTree/TriangleCodec/TriangleCodecIndexed8BitPackSOA4Flags.h>
#include <Jolt/AABBTree/NodeCodec/NodeCodecQuadTreeHalfFloat.h>
#include <Jolt/ObjectStream/TypeDeclarations.h>

JPH_NAMESPACE_BEGIN

#ifdef JPH_DEBUG_RENDERER
bool MeshShape::sDrawTriangleGroups = false;
bool MeshShape::sDrawTriangleOutlines = false;
#endif // JPH_DEBUG_RENDERER

JPH_IMPLEMENT_SERIALIZABLE_VIRTUAL(MeshShapeSettings)
{
	JPH_ADD_BASE_CLASS(MeshShapeSettings, ShapeSettings)

	JPH_ADD_ATTRIBUTE(MeshShapeSettings, mTriangleVertices)
	JPH_ADD_ATTRIBUTE(MeshShapeSettings, mIndexedTriangles)
	JPH_ADD_ATTRIBUTE(MeshShapeSettings, mMaterials)
	JPH_ADD_ATTRIBUTE(MeshShapeSettings, mMaxTrianglesPerLeaf)
	JPH_ADD_ATTRIBUTE(MeshShapeSettings, mActiveEdgeCosThresholdAngle)
	JPH_ADD_ATTRIBUTE(MeshShapeSettings, mPerTriangleUserData)
}

// Codecs this mesh shape is using
using TriangleCodec = TriangleCodecIndexed8BitPackSOA4Flags;
using NodeCodec = NodeCodecQuadTreeHalfFloat;

// Get header for tree
static JPH_INLINE const NodeCodec::Header *sGetNodeHeader(const ByteBuffer &inTree)
{
	return inTree.Get<NodeCodec::Header>(0);
}

// Get header for triangles
static JPH_INLINE const TriangleCodec::TriangleHeader *sGetTriangleHeader(const ByteBuffer &inTree)
{
	return inTree.Get<TriangleCodec::TriangleHeader>(NodeCodec::HeaderSize);
}

MeshShapeSettings::MeshShapeSettings(const TriangleList &inTriangles, PhysicsMaterialList inMaterials) :
	mMaterials(std::move(inMaterials))
{
	Indexify(inTriangles, mTriangleVertices, mIndexedTriangles);

	Sanitize();
}

MeshShapeSettings::MeshShapeSettings(VertexList inVertices, IndexedTriangleList inTriangles, PhysicsMaterialList inMaterials) :
	mTriangleVertices(std::move(inVertices)),
	mIndexedTriangles(std::move(inTriangles)),
	mMaterials(std::move(inMaterials))
{
	Sanitize();
}

void MeshShapeSettings::Sanitize()
{
	// Remove degenerate and duplicate triangles
	UnorderedSet<IndexedTriangle> triangles;
	triangles.reserve(UnorderedSet<IndexedTriangle>::size_type(mIndexedTriangles.size()));
	TriangleCodec::ValidationContext validation_ctx(mIndexedTriangles, mTriangleVertices);
	for (int t = (int)mIndexedTriangles.size() - 1; t >= 0; --t)
	{
		const IndexedTriangle &tri = mIndexedTriangles[t];

		if (tri.IsDegenerate(mTriangleVertices)						// Degenerate triangle
			|| validation_ctx.IsDegenerate(tri)						// Triangle is degenerate in the quantized space
			|| !triangles.insert(tri.GetLowestIndexFirst()).second) // Duplicate triangle
		{
			// The order of triangles doesn't matter (gets reordered while building the tree), so we can just swap the last triangle into this slot
			mIndexedTriangles[t] = mIndexedTriangles.back();
			mIndexedTriangles.pop_back();
		}
	}
}

ShapeSettings::ShapeResult MeshShapeSettings::Create() const
{
	if (mCachedResult.IsEmpty())
		Ref<Shape> shape = new MeshShape(*this, mCachedResult);
	return mCachedResult;
}

MeshShape::MeshShape(const MeshShapeSettings &inSettings, ShapeResult &outResult) :
	Shape(EShapeType::Mesh, EShapeSubType::Mesh, inSettings, outResult)
{
	// Check if there are any triangles
	if (inSettings.mIndexedTriangles.empty())
	{
		outResult.SetError("Need triangles to create a mesh shape!");
		return;
	}

	// Check triangles
	TriangleCodec::ValidationContext validation_ctx(inSettings.mIndexedTriangles, inSettings.mTriangleVertices);
	for (int t = (int)inSettings.mIndexedTriangles.size() - 1; t >= 0; --t)
	{
		const IndexedTriangle &triangle = inSettings.mIndexedTriangles[t];
		if (triangle.IsDegenerate(inSettings.mTriangleVertices)
			|| validation_ctx.IsDegenerate(triangle))
		{
			outResult.SetError(StringFormat("Triangle %d is degenerate!", t));
			return;
		}
		else
		{
			// Check vertex indices
			for (uint32 idx : triangle.mIdx)
				if (idx >= inSettings.mTriangleVertices.size())
				{
					outResult.SetError(StringFormat("Vertex index %u is beyond vertex list (size: %u)", idx, (uint)inSettings.mTriangleVertices.size()));
					return;
				}
		}
	}

	// Copy materials
	mMaterials = inSettings.mMaterials;
	if (!mMaterials.empty())
	{
		// Validate materials
		if (mMaterials.size() > (1 << FLAGS_MATERIAL_BITS))
		{
			outResult.SetError(StringFormat("Supporting max %d materials per mesh", 1 << FLAGS_MATERIAL_BITS));
			return;
		}
		for (const IndexedTriangle &t : inSettings.mIndexedTriangles)
			if (t.mMaterialIndex >= mMaterials.size())
			{
				outResult.SetError(StringFormat("Triangle material %u is beyond material list (size: %u)", t.mMaterialIndex, (uint)mMaterials.size()));
				return;
			}
	}
	else
	{
		// No materials assigned, validate that all triangles use material index 0
		for (const IndexedTriangle &t : inSettings.mIndexedTriangles)
			if (t.mMaterialIndex != 0)
			{
				outResult.SetError("No materials present, all triangles should have material index 0");
				return;
			}
	}

	// Check max triangles
	if (inSettings.mMaxTrianglesPerLeaf < 1 || inSettings.mMaxTrianglesPerLeaf > MaxTrianglesPerLeaf)
	{
		outResult.SetError("Invalid max triangles per leaf");
		return;
	}

	// Fill in active edge bits
	IndexedTriangleList indexed_triangles = inSettings.mIndexedTriangles; // Copy indices since we're adding the 'active edge' flag
	sFindActiveEdges(inSettings, indexed_triangles);

	// Create triangle splitter
	TriangleSplitterBinning splitter(inSettings.mTriangleVertices, indexed_triangles);

	// Build tree
	AABBTreeBuilder builder(splitter, inSettings.mMaxTrianglesPerLeaf);
	AABBTreeBuilderStats builder_stats;
	const AABBTreeBuilder::Node *root = builder.Build(builder_stats);

	// Convert to buffer
	AABBTreeToBuffer<TriangleCodec, NodeCodec> buffer;
	const char *error = nullptr;
	if (!buffer.Convert(builder.GetTriangles(), builder.GetNodes(), inSettings.mTriangleVertices, root, inSettings.mPerTriangleUserData, error))
	{
		outResult.SetError(error);
		return;
	}

	// Move data to this class
	mTree.swap(buffer.GetBuffer());

	// Check if we're not exceeding the amount of sub shape id bits
	if (GetSubShapeIDBitsRecursive() > SubShapeID::MaxBits)
	{
		outResult.SetError("Mesh is too big and exceeds the amount of available sub shape ID bits");
		return;
	}

	outResult.Set(this);
}

void MeshShape::sFindActiveEdges(const MeshShapeSettings &inSettings, IndexedTriangleList &ioIndices)
{
	// A struct to hold the two vertex indices of an edge
	struct Edge
	{
				Edge(int inIdx1, int inIdx2) : mIdx1(min(inIdx1, inIdx2)), mIdx2(max(inIdx1, inIdx2)) { }

		uint	GetIndexInTriangle(const IndexedTriangle &inTriangle) const
		{
			for (uint edge_idx = 0; edge_idx < 3; ++edge_idx)
			{
				Edge edge(inTriangle.mIdx[edge_idx], inTriangle.mIdx[(edge_idx + 1) % 3]);
				if (*this == edge)
					return edge_idx;
			}

			JPH_ASSERT(false);
			return ~uint(0);
		}

		bool	operator == (const Edge &inRHS) const
		{
			return mIdx1 == inRHS.mIdx1 && mIdx2 == inRHS.mIdx2;
		}

		uint64	GetHash() const
		{
			static_assert(sizeof(*this) == 2 * sizeof(int), "No padding expected");
			return HashBytes(this, sizeof(*this));
		}

		int		mIdx1;
		int		mIdx2;
	};

	// A struct to hold the triangles that are connected to an edge
	struct TriangleIndices
	{
		uint	mNumTriangles = 0;
		uint	mTriangleIndices[2];
	};

	// Build a list of edge to triangles
	using EdgeToTriangle = UnorderedMap<Edge, TriangleIndices>;
	EdgeToTriangle edge_to_triangle;
	edge_to_triangle.reserve(EdgeToTriangle::size_type(ioIndices.size() * 3));
	for (uint triangle_idx = 0; triangle_idx < ioIndices.size(); ++triangle_idx)
	{
		IndexedTriangle &triangle = ioIndices[triangle_idx];
		for (uint edge_idx = 0; edge_idx < 3; ++edge_idx)
		{
			Edge edge(triangle.mIdx[edge_idx], triangle.mIdx[(edge_idx + 1) % 3]);
			EdgeToTriangle::iterator edge_to_triangle_it = edge_to_triangle.try_emplace(edge, TriangleIndices()).first;
			TriangleIndices &indices = edge_to_triangle_it->second;
			if (indices.mNumTriangles < 2)
			{
				// Store index of triangle that connects to this edge
				indices.mTriangleIndices[indices.mNumTriangles] = triangle_idx;
				indices.mNumTriangles++;
			}
			else
			{
				// 3 or more triangles share an edge, mark this edge as active
				uint32 mask = 1 << (edge_idx + FLAGS_ACTIVE_EGDE_SHIFT);
				JPH_ASSERT((triangle.mMaterialIndex & mask) == 0);
				triangle.mMaterialIndex |= mask;
			}
		}
	}

	// Walk over all edges and determine which ones are active
	for (const EdgeToTriangle::value_type &edge : edge_to_triangle)
	{
		uint num_active = 0;
		if (edge.second.mNumTriangles == 1)
		{
			// Edge is not shared, it is an active edge
			num_active = 1;
		}
		else if (edge.second.mNumTriangles == 2)
		{
			// Simple shared edge, determine if edge is active based on the two adjacent triangles
			const IndexedTriangle &triangle1 = ioIndices[edge.second.mTriangleIndices[0]];
			const IndexedTriangle &triangle2 = ioIndices[edge.second.mTriangleIndices[1]];

			// Find which edge this is for both triangles
			uint edge_idx1 = edge.first.GetIndexInTriangle(triangle1);
			uint edge_idx2 = edge.first.GetIndexInTriangle(triangle2);

			// Construct a plane for triangle 1 (e1 = edge vertex 1, e2 = edge vertex 2, op = opposing vertex)
			Vec3 triangle1_e1 = Vec3(inSettings.mTriangleVertices[triangle1.mIdx[edge_idx1]]);
			Vec3 triangle1_e2 = Vec3(inSettings.mTriangleVertices[triangle1.mIdx[(edge_idx1 + 1) % 3]]);
			Vec3 triangle1_op = Vec3(inSettings.mTriangleVertices[triangle1.mIdx[(edge_idx1 + 2) % 3]]);
			Plane triangle1_plane = Plane::sFromPointsCCW(triangle1_e1, triangle1_e2, triangle1_op);

			// Construct a plane for triangle 2
			Vec3 triangle2_e1 = Vec3(inSettings.mTriangleVertices[triangle2.mIdx[edge_idx2]]);
			Vec3 triangle2_e2 = Vec3(inSettings.mTriangleVertices[triangle2.mIdx[(edge_idx2 + 1) % 3]]);
			Vec3 triangle2_op = Vec3(inSettings.mTriangleVertices[triangle2.mIdx[(edge_idx2 + 2) % 3]]);
			Plane triangle2_plane = Plane::sFromPointsCCW(triangle2_e1, triangle2_e2, triangle2_op);

			// Determine if the edge is active
			num_active = ActiveEdges::IsEdgeActive(triangle1_plane.GetNormal(), triangle2_plane.GetNormal(), triangle1_e2 - triangle1_e1, inSettings.mActiveEdgeCosThresholdAngle)? 2 : 0;
		}
		else
		{
			// More edges incoming, we've already marked all edges beyond the 2nd as active
			num_active = 2;
		}

		// Mark edges of all original triangles active
		for (uint i = 0; i < num_active; ++i)
		{
			uint triangle_idx = edge.second.mTriangleIndices[i];
			IndexedTriangle &triangle = ioIndices[triangle_idx];
			uint edge_idx = edge.first.GetIndexInTriangle(triangle);
			uint32 mask = 1 << (edge_idx + FLAGS_ACTIVE_EGDE_SHIFT);
			JPH_ASSERT((triangle.mMaterialIndex & mask) == 0);
			triangle.mMaterialIndex |= mask;
		}
	}
}

MassProperties MeshShape::GetMassProperties() const
{
	// We cannot calculate the volume for an arbitrary mesh, so we return invalid mass properties.
	// If you want your mesh to be dynamic, then you should provide the mass properties yourself when
	// creating a Body:
	//
	// BodyCreationSettings::mOverrideMassProperties = EOverrideMassProperties::MassAndInertiaProvided;
	// BodyCreationSettings::mMassPropertiesOverride.SetMassAndInertiaOfSolidBox(Vec3::sReplicate(1.0f), 1000.0f);
	//
	// Note that for a mesh shape to simulate properly, it is best if the mesh is manifold
	// (i.e. closed, all edges shared by only two triangles, consistent winding order).
	return MassProperties();
}

void MeshShape::DecodeSubShapeID(const SubShapeID &inSubShapeID, const void *&outTriangleBlock, uint32 &outTriangleIndex) const
{
	// Get block
	SubShapeID triangle_idx_subshape_id;
	uint32 block_id = inSubShapeID.PopID(NodeCodec::DecodingContext::sTriangleBlockIDBits(sGetNodeHeader(mTree)), triangle_idx_subshape_id);
	outTriangleBlock = NodeCodec::DecodingContext::sGetTriangleBlockStart(&mTree[0], block_id);

	// Fetch the triangle index
	SubShapeID remainder;
	outTriangleIndex = triangle_idx_subshape_id.PopID(NumTriangleBits, remainder);
	JPH_ASSERT(remainder.IsEmpty(), "Invalid subshape ID");
}

uint MeshShape::GetMaterialIndex(const SubShapeID &inSubShapeID) const
{
	// Decode ID
	const void *block_start;
	uint32 triangle_idx;
	DecodeSubShapeID(inSubShapeID, block_start, triangle_idx);

	// Fetch the flags
	uint8 flags = TriangleCodec::DecodingContext::sGetFlags(block_start, triangle_idx);
	return flags & FLAGS_MATERIAL_MASK;
}

const PhysicsMaterial *MeshShape::GetMaterial(const SubShapeID &inSubShapeID) const
{
	// Return the default material if there are no materials on this shape
	if (mMaterials.empty())
		return PhysicsMaterial::sDefault;

	return mMaterials[GetMaterialIndex(inSubShapeID)];
}

Vec3 MeshShape::GetSurfaceNormal(const SubShapeID &inSubShapeID, Vec3Arg inLocalSurfacePosition) const
{
	// Decode ID
	const void *block_start;
	uint32 triangle_idx;
	DecodeSubShapeID(inSubShapeID, block_start, triangle_idx);

	// Decode triangle
	Vec3 v1, v2, v3;
	const TriangleCodec::DecodingContext triangle_ctx(sGetTriangleHeader(mTree));
	triangle_ctx.GetTriangle(block_start, triangle_idx, v1, v2, v3);

	// Calculate normal
	return (v3 - v2).Cross(v1 - v2).Normalized();
}

void MeshShape::GetSupportingFace(const SubShapeID &inSubShapeID, Vec3Arg inDirection, Vec3Arg inScale, Mat44Arg inCenterOfMassTransform, SupportingFace &outVertices) const
{
	// Decode ID
	const void *block_start;
	uint32 triangle_idx;
	DecodeSubShapeID(inSubShapeID, block_start, triangle_idx);

	// Decode triangle
	const TriangleCodec::DecodingContext triangle_ctx(sGetTriangleHeader(mTree));
	outVertices.resize(3);
	triangle_ctx.GetTriangle(block_start, triangle_idx, outVertices[0], outVertices[1], outVertices[2]);

	// Flip triangle if scaled inside out
	if (ScaleHelpers::IsInsideOut(inScale))
		std::swap(outVertices[1], outVertices[2]);

	// Calculate transform with scale
	Mat44 transform = inCenterOfMassTransform.PreScaled(inScale);

	// Transform to world space
	for (Vec3 &v : outVertices)
		v = transform * v;
}

AABox MeshShape::GetLocalBounds() const
{
	const NodeCodec::Header *header = sGetNodeHeader(mTree);
	return AABox(Vec3::sLoadFloat3Unsafe(header->mRootBoundsMin), Vec3::sLoadFloat3Unsafe(header->mRootBoundsMax));
}

uint MeshShape::GetSubShapeIDBitsRecursive() const
{
	return NodeCodec::DecodingContext::sTriangleBlockIDBits(sGetNodeHeader(mTree)) + NumTriangleBits;
}

template <class Visitor>
JPH_INLINE void MeshShape::WalkTree(Visitor &ioVisitor) const
{
	const NodeCodec::Header *header = sGetNodeHeader(mTree);
	NodeCodec::DecodingContext node_ctx(header);

	const TriangleCodec::DecodingContext triangle_ctx(sGetTriangleHeader(mTree));
	const uint8 *buffer_start = &mTree[0];
	node_ctx.WalkTree(buffer_start, triangle_ctx, ioVisitor);
}

template <class Visitor>
JPH_INLINE void MeshShape::WalkTreePerTriangle(const SubShapeIDCreator &inSubShapeIDCreator2, Visitor &ioVisitor) const
{
	struct ChainedVisitor
	{
		JPH_INLINE			ChainedVisitor(Visitor &ioVisitor, const SubShapeIDCreator &inSubShapeIDCreator2, uint inTriangleBlockIDBits) :
			mVisitor(ioVisitor),
			mSubShapeIDCreator2(inSubShapeIDCreator2),
			mTriangleBlockIDBits(inTriangleBlockIDBits)
		{
		}

		JPH_INLINE bool		ShouldAbort() const
		{
			return mVisitor.ShouldAbort();
		}

		JPH_INLINE bool		ShouldVisitNode(int inStackTop) const
		{
			return mVisitor.ShouldVisitNode(inStackTop);
		}

		JPH_INLINE int		VisitNodes(Vec4Arg inBoundsMinX, Vec4Arg inBoundsMinY, Vec4Arg inBoundsMinZ, Vec4Arg inBoundsMaxX, Vec4Arg inBoundsMaxY, Vec4Arg inBoundsMaxZ, UVec4 &ioProperties, int inStackTop)
		{
			return mVisitor.VisitNodes(inBoundsMinX, inBoundsMinY, inBoundsMinZ, inBoundsMaxX, inBoundsMaxY, inBoundsMaxZ, ioProperties, inStackTop);
		}

		JPH_INLINE void		VisitTriangles(const TriangleCodec::DecodingContext &ioContext, const void *inTriangles, int inNumTriangles, uint32 inTriangleBlockID)
		{
			// Create ID for triangle block
			SubShapeIDCreator block_sub_shape_id = mSubShapeIDCreator2.PushID(inTriangleBlockID, mTriangleBlockIDBits);

			// Decode vertices and flags
			JPH_ASSERT(inNumTriangles <= MaxTrianglesPerLeaf);
			Vec3 vertices[MaxTrianglesPerLeaf * 3];
			uint8 flags[MaxTrianglesPerLeaf];
			ioContext.Unpack(inTriangles, inNumTriangles, vertices, flags);

			int triangle_idx = 0;
			for (const Vec3 *v = vertices, *v_end = vertices + inNumTriangles * 3; v < v_end; v += 3, triangle_idx++)
			{
				// Determine active edges
				uint8 active_edges = (flags[triangle_idx] >> FLAGS_ACTIVE_EGDE_SHIFT) & FLAGS_ACTIVE_EDGE_MASK;

				// Create ID for triangle
				SubShapeIDCreator triangle_sub_shape_id = block_sub_shape_id.PushID(triangle_idx, NumTriangleBits);

				mVisitor.VisitTriangle(v[0], v[1], v[2], active_edges, triangle_sub_shape_id.GetID());

				// Check if we should early out now
				if (mVisitor.ShouldAbort())
					break;
			}
		}

		Visitor &			mVisitor;
		SubShapeIDCreator	mSubShapeIDCreator2;
		uint				mTriangleBlockIDBits;
	};

	ChainedVisitor visitor(ioVisitor, inSubShapeIDCreator2, NodeCodec::DecodingContext::sTriangleBlockIDBits(sGetNodeHeader(mTree)));
	WalkTree(visitor);
}

#ifdef JPH_DEBUG_RENDERER
void MeshShape::Draw(DebugRenderer *inRenderer, RMat44Arg inCenterOfMassTransform, Vec3Arg inScale, ColorArg inColor, bool inUseMaterialColors, bool inDrawWireframe) const
{
	// Reset the batch if we switch coloring mode
	if (mCachedTrianglesColoredPerGroup != sDrawTriangleGroups || mCachedUseMaterialColors != inUseMaterialColors)
	{
		mGeometry = nullptr;
		mCachedTrianglesColoredPerGroup = sDrawTriangleGroups;
		mCachedUseMaterialColors = inUseMaterialColors;
	}

	if (mGeometry == nullptr)
	{
		struct Visitor
		{
			JPH_INLINE bool		ShouldAbort() const
			{
				return false;
			}

			JPH_INLINE bool		ShouldVisitNode(int inStackTop) const
			{
				return true;
			}

			JPH_INLINE int		VisitNodes(Vec4Arg inBoundsMinX, Vec4Arg inBoundsMinY, Vec4Arg inBoundsMinZ, Vec4Arg inBoundsMaxX, Vec4Arg inBoundsMaxY, Vec4Arg inBoundsMaxZ, UVec4 &ioProperties, int inStackTop)
			{
				UVec4 valid = UVec4::sOr(UVec4::sOr(Vec4::sLess(inBoundsMinX, inBoundsMaxX), Vec4::sLess(inBoundsMinY, inBoundsMaxY)), Vec4::sLess(inBoundsMinZ, inBoundsMaxZ));
				return CountAndSortTrues(valid, ioProperties);
			}

			JPH_INLINE void		VisitTriangles(const TriangleCodec::DecodingContext &ioContext, const void *inTriangles, int inNumTriangles, [[maybe_unused]] uint32 inTriangleBlockID)
			{
				JPH_ASSERT(inNumTriangles <= MaxTrianglesPerLeaf);
				Vec3 vertices[MaxTrianglesPerLeaf * 3];
				ioContext.Unpack(inTriangles, inNumTriangles, vertices);

				if (mDrawTriangleGroups || !mUseMaterialColors || mMaterials.empty())
				{
					// Single color for mesh
					Color color = mDrawTriangleGroups? Color::sGetDistinctColor(mColorIdx++) : (mUseMaterialColors? PhysicsMaterial::sDefault->GetDebugColor() : Color::sWhite);
					for (const Vec3 *v = vertices, *v_end = vertices + inNumTriangles * 3; v < v_end; v += 3)
						mTriangles.push_back({ v[0], v[1], v[2], color });
				}
				else
				{
					// Per triangle color
					uint8 flags[MaxTrianglesPerLeaf];
					TriangleCodec::DecodingContext::sGetFlags(inTriangles, inNumTriangles, flags);

					const uint8 *f = flags;
					for (const Vec3 *v = vertices, *v_end = vertices + inNumTriangles * 3; v < v_end; v += 3, f++)
						mTriangles.push_back({ v[0], v[1], v[2], mMaterials[*f & FLAGS_MATERIAL_MASK]->GetDebugColor() });
				}
			}

			Array<DebugRenderer::Triangle> &		mTriangles;
			const PhysicsMaterialList &				mMaterials;
			bool									mUseMaterialColors;
			bool									mDrawTriangleGroups;
			int										mColorIdx = 0;
		};

		Array<DebugRenderer::Triangle> triangles;
		Visitor visitor { triangles, mMaterials, mCachedUseMaterialColors, mCachedTrianglesColoredPerGroup };
		WalkTree(visitor);
		mGeometry = new DebugRenderer::Geometry(inRenderer->CreateTriangleBatch(triangles), GetLocalBounds());
	}

	// Test if the shape is scaled inside out
	DebugRenderer::ECullMode cull_mode = ScaleHelpers::IsInsideOut(inScale)? DebugRenderer::ECullMode::CullFrontFace : DebugRenderer::ECullMode::CullBackFace;

	// Determine the draw mode
	DebugRenderer::EDrawMode draw_mode = inDrawWireframe? DebugRenderer::EDrawMode::Wireframe : DebugRenderer::EDrawMode::Solid;

	// Draw the geometry
	inRenderer->DrawGeometry(inCenterOfMassTransform * Mat44::sScale(inScale), inColor, mGeometry, cull_mode, DebugRenderer::ECastShadow::On, draw_mode);

	if (sDrawTriangleOutlines)
	{
		struct Visitor
		{
			JPH_INLINE			Visitor(DebugRenderer *inRenderer, RMat44Arg inTransform) :
				mRenderer(inRenderer),
				mTransform(inTransform)
			{
			}

			JPH_INLINE bool		ShouldAbort() const
			{
				return false;
			}

			JPH_INLINE bool		ShouldVisitNode(int inStackTop) const
			{
				return true;
			}

			JPH_INLINE int		VisitNodes(Vec4Arg inBoundsMinX, Vec4Arg inBoundsMinY, Vec4Arg inBoundsMinZ, Vec4Arg inBoundsMaxX, Vec4Arg inBoundsMaxY, Vec4Arg inBoundsMaxZ, UVec4 &ioProperties, int inStackTop)
			{
				UVec4 valid = UVec4::sOr(UVec4::sOr(Vec4::sLess(inBoundsMinX, inBoundsMaxX), Vec4::sLess(inBoundsMinY, inBoundsMaxY)), Vec4::sLess(inBoundsMinZ, inBoundsMaxZ));
				return CountAndSortTrues(valid, ioProperties);
			}

			JPH_INLINE void		VisitTriangles(const TriangleCodec::DecodingContext &ioContext, const void *inTriangles, int inNumTriangles, uint32 inTriangleBlockID)
			{
				// Decode vertices and flags
				JPH_ASSERT(inNumTriangles <= MaxTrianglesPerLeaf);
				Vec3 vertices[MaxTrianglesPerLeaf * 3];
				uint8 flags[MaxTrianglesPerLeaf];
				ioContext.Unpack(inTriangles, inNumTriangles, vertices, flags);

				// Loop through triangles
				const uint8 *f = flags;
				for (Vec3 *v = vertices, *v_end = vertices + inNumTriangles * 3; v < v_end; v += 3, ++f)
				{
					// Loop through edges
					for (uint edge_idx = 0; edge_idx < 3; ++edge_idx)
					{
						RVec3 v1 = mTransform * v[edge_idx];
						RVec3 v2 = mTransform * v[(edge_idx + 1) % 3];

						// Draw active edge as a green arrow, other edges as grey
						if (*f & (1 << (edge_idx + FLAGS_ACTIVE_EGDE_SHIFT)))
							mRenderer->DrawArrow(v1, v2, Color::sGreen, 0.01f);
						else
							mRenderer->DrawLine(v1, v2, Color::sGrey);
					}
				}
			}

			DebugRenderer *	mRenderer;
			RMat44			mTransform;
		};

		Visitor visitor { inRenderer, inCenterOfMassTransform.PreScaled(inScale) };
		WalkTree(visitor);
	}
}
#endif // JPH_DEBUG_RENDERER

bool MeshShape::CastRay(const RayCast &inRay, const SubShapeIDCreator &inSubShapeIDCreator, RayCastResult &ioHit) const
{
	JPH_PROFILE_FUNCTION();

	struct Visitor
	{
		JPH_INLINE explicit	Visitor(RayCastResult &ioHit) :
			mHit(ioHit)
		{
		}

		JPH_INLINE bool		ShouldAbort() const
		{
			return mHit.mFraction <= 0.0f;
		}

		JPH_INLINE bool		ShouldVisitNode(int inStackTop) const
		{
			return mDistanceStack[inStackTop] < mHit.mFraction;
		}

		JPH_INLINE int		VisitNodes(Vec4Arg inBoundsMinX, Vec4Arg inBoundsMinY, Vec4Arg inBoundsMinZ, Vec4Arg inBoundsMaxX, Vec4Arg inBoundsMaxY, Vec4Arg inBoundsMaxZ, UVec4 &ioProperties, int inStackTop)
		{
			// Test bounds of 4 children
			Vec4 distance = RayAABox4(mRayOrigin, mRayInvDirection, inBoundsMinX, inBoundsMinY, inBoundsMinZ, inBoundsMaxX, inBoundsMaxY, inBoundsMaxZ);

			// Sort so that highest values are first (we want to first process closer hits and we process stack top to bottom)
			return SortReverseAndStore(distance, mHit.mFraction, ioProperties, &mDistanceStack[inStackTop]);
		}

		JPH_INLINE void		VisitTriangles(const TriangleCodec::DecodingContext &ioContext, const void *inTriangles, int inNumTriangles, uint32 inTriangleBlockID)
		{
			// Test against triangles
			uint32 triangle_idx;
			float fraction = ioContext.TestRay(mRayOrigin, mRayDirection, inTriangles, inNumTriangles, mHit.mFraction, triangle_idx);
			if (fraction < mHit.mFraction)
			{
				mHit.mFraction = fraction;
				mHit.mSubShapeID2 = mSubShapeIDCreator.PushID(inTriangleBlockID, mTriangleBlockIDBits).PushID(triangle_idx, NumTriangleBits).GetID();
				mReturnValue = true;
			}
		}

		RayCastResult &		mHit;
		Vec3				mRayOrigin;
		Vec3				mRayDirection;
		RayInvDirection		mRayInvDirection;
		uint				mTriangleBlockIDBits;
		SubShapeIDCreator	mSubShapeIDCreator;
		bool				mReturnValue = false;
		float				mDistanceStack[NodeCodec::StackSize];
	};

	Visitor visitor(ioHit);
	visitor.mRayOrigin = inRay.mOrigin;
	visitor.mRayDirection = inRay.mDirection;
	visitor.mRayInvDirection.Set(inRay.mDirection);
	visitor.mTriangleBlockIDBits = NodeCodec::DecodingContext::sTriangleBlockIDBits(sGetNodeHeader(mTree));
	visitor.mSubShapeIDCreator = inSubShapeIDCreator;
	WalkTree(visitor);

	return visitor.mReturnValue;
}

void MeshShape::CastRay(const RayCast &inRay, const RayCastSettings &inRayCastSettings, const SubShapeIDCreator &inSubShapeIDCreator, CastRayCollector &ioCollector, const ShapeFilter &inShapeFilter) const
{
	JPH_PROFILE_FUNCTION();

	// Test shape filter
	if (!inShapeFilter.ShouldCollide(this, inSubShapeIDCreator.GetID()))
		return;

	struct Visitor
	{
		JPH_INLINE explicit	Visitor(CastRayCollector &ioCollector) :
			mCollector(ioCollector)
		{
		}

		JPH_INLINE bool		ShouldAbort() const
		{
			return mCollector.ShouldEarlyOut();
		}

		JPH_INLINE bool		ShouldVisitNode(int inStackTop) const
		{
			return mDistanceStack[inStackTop] < mCollector.GetEarlyOutFraction();
		}

		JPH_INLINE int		VisitNodes(Vec4Arg inBoundsMinX, Vec4Arg inBoundsMinY, Vec4Arg inBoundsMinZ, Vec4Arg inBoundsMaxX, Vec4Arg inBoundsMaxY, Vec4Arg inBoundsMaxZ, UVec4 &ioProperties, int inStackTop)
		{
			// Test bounds of 4 children
			Vec4 distance = RayAABox4(mRayOrigin, mRayInvDirection, inBoundsMinX, inBoundsMinY, inBoundsMinZ, inBoundsMaxX, inBoundsMaxY, inBoundsMaxZ);

			// Sort so that highest values are first (we want to first process closer hits and we process stack top to bottom)
			return SortReverseAndStore(distance, mCollector.GetEarlyOutFraction(), ioProperties, &mDistanceStack[inStackTop]);
		}

		JPH_INLINE void		VisitTriangle(Vec3Arg inV0, Vec3Arg inV1, Vec3Arg inV2, [[maybe_unused]] uint8 inActiveEdges, SubShapeID inSubShapeID2)
		{
			// Back facing check
			if (mBackFaceMode == EBackFaceMode::IgnoreBackFaces && (inV2 - inV0).Cross(inV1 - inV0).Dot(mRayDirection) < 0)
				return;

			// Check the triangle
			float fraction = RayTriangle(mRayOrigin, mRayDirection, inV0, inV1, inV2);
			if (fraction < mCollector.GetEarlyOutFraction())
			{
				RayCastResult hit;
				hit.mBodyID = TransformedShape::sGetBodyID(mCollector.GetContext());
				hit.mFraction = fraction;
				hit.mSubShapeID2 = inSubShapeID2;
				mCollector.AddHit(hit);
			}
		}

		CastRayCollector &	mCollector;
		Vec3				mRayOrigin;
		Vec3				mRayDirection;
		RayInvDirection		mRayInvDirection;
		EBackFaceMode		mBackFaceMode;
		float				mDistanceStack[NodeCodec::StackSize];
	};

	Visitor visitor(ioCollector);
	visitor.mBackFaceMode = inRayCastSettings.mBackFaceModeTriangles;
	visitor.mRayOrigin = inRay.mOrigin;
	visitor.mRayDirection = inRay.mDirection;
	visitor.mRayInvDirection.Set(inRay.mDirection);
	WalkTreePerTriangle(inSubShapeIDCreator, visitor);
}

void MeshShape::CollidePoint(Vec3Arg inPoint, const SubShapeIDCreator &inSubShapeIDCreator, CollidePointCollector &ioCollector, const ShapeFilter &inShapeFilter) const
{
	sCollidePointUsingRayCast(*this, inPoint, inSubShapeIDCreator, ioCollector, inShapeFilter);
}

void MeshShape::CollideSoftBodyVertices(Mat44Arg inCenterOfMassTransform, Vec3Arg inScale, const CollideSoftBodyVertexIterator &inVertices, uint inNumVertices, int inCollidingShapeIndex) const
{
	JPH_PROFILE_FUNCTION();

	struct Visitor : public CollideSoftBodyVerticesVsTriangles
	{
		using CollideSoftBodyVerticesVsTriangles::CollideSoftBodyVerticesVsTriangles;

		JPH_INLINE bool	ShouldAbort() const
		{
			return false;
		}

		JPH_INLINE bool	ShouldVisitNode([[maybe_unused]] int inStackTop) const
		{
			return mDistanceStack[inStackTop] < mClosestDistanceSq;
		}

		JPH_INLINE int	VisitNodes(Vec4Arg inBoundsMinX, Vec4Arg inBoundsMinY, Vec4Arg inBoundsMinZ, Vec4Arg inBoundsMaxX, Vec4Arg inBoundsMaxY, Vec4Arg inBoundsMaxZ, UVec4 &ioProperties, int inStackTop)
		{
			// Get distance to vertex
			Vec4 dist_sq = AABox4DistanceSqToPoint(mLocalPosition, inBoundsMinX, inBoundsMinY, inBoundsMinZ, inBoundsMaxX, inBoundsMaxY, inBoundsMaxZ);

			// Sort so that highest values are first (we want to first process closer hits and we process stack top to bottom)
			return SortReverseAndStore(dist_sq, mClosestDistanceSq, ioProperties, &mDistanceStack[inStackTop]);
		}

		JPH_INLINE void	VisitTriangle(Vec3Arg inV0, Vec3Arg inV1, Vec3Arg inV2, [[maybe_unused]] uint8 inActiveEdges, [[maybe_unused]] SubShapeID inSubShapeID2)
		{
			ProcessTriangle(inV0, inV1, inV2);
		}

		float			mDistanceStack[NodeCodec::StackSize];
	};

	Visitor visitor(inCenterOfMassTransform, inScale);

	for (CollideSoftBodyVertexIterator v = inVertices, sbv_end = inVertices + inNumVertices; v != sbv_end; ++v)
		if (v.GetInvMass() > 0.0f)
		{
			visitor.StartVertex(v);
			WalkTreePerTriangle(SubShapeIDCreator(), visitor);
			visitor.FinishVertex(v, inCollidingShapeIndex);
		}
}

void MeshShape::sCastConvexVsMesh(const ShapeCast &inShapeCast, const ShapeCastSettings &inShapeCastSettings, const Shape *inShape, Vec3Arg inScale, [[maybe_unused]] const ShapeFilter &inShapeFilter, Mat44Arg inCenterOfMassTransform2, const SubShapeIDCreator &inSubShapeIDCreator1, const SubShapeIDCreator &inSubShapeIDCreator2, CastShapeCollector &ioCollector)
{
	JPH_PROFILE_FUNCTION();

	struct Visitor : public CastConvexVsTriangles
	{
		using CastConvexVsTriangles::CastConvexVsTriangles;

		JPH_INLINE bool		ShouldAbort() const
		{
			return mCollector.ShouldEarlyOut();
		}

		JPH_INLINE bool		ShouldVisitNode(int inStackTop) const
		{
			return mDistanceStack[inStackTop] < mCollector.GetPositiveEarlyOutFraction();
		}

		JPH_INLINE int		VisitNodes(Vec4Arg inBoundsMinX, Vec4Arg inBoundsMinY, Vec4Arg inBoundsMinZ, Vec4Arg inBoundsMaxX, Vec4Arg inBoundsMaxY, Vec4Arg inBoundsMaxZ, UVec4 &ioProperties, int inStackTop)
		{
			// Scale the bounding boxes of this node
			Vec4 bounds_min_x, bounds_min_y, bounds_min_z, bounds_max_x, bounds_max_y, bounds_max_z;
			AABox4Scale(mScale, inBoundsMinX, inBoundsMinY, inBoundsMinZ, inBoundsMaxX, inBoundsMaxY, inBoundsMaxZ, bounds_min_x, bounds_min_y, bounds_min_z, bounds_max_x, bounds_max_y, bounds_max_z);

			// Enlarge them by the casted shape's box extents
			AABox4EnlargeWithExtent(mBoxExtent, bounds_min_x, bounds_min_y, bounds_min_z, bounds_max_x, bounds_max_y, bounds_max_z);

			// Test bounds of 4 children
			Vec4 distance = RayAABox4(mBoxCenter, mInvDirection, bounds_min_x, bounds_min_y, bounds_min_z, bounds_max_x, bounds_max_y, bounds_max_z);

			// Sort so that highest values are first (we want to first process closer hits and we process stack top to bottom)
			return SortReverseAndStore(distance, mCollector.GetPositiveEarlyOutFraction(), ioProperties, &mDistanceStack[inStackTop]);
		}

		JPH_INLINE void		VisitTriangle(Vec3Arg inV0, Vec3Arg inV1, Vec3Arg inV2, uint8 inActiveEdges, SubShapeID inSubShapeID2)
		{
			Cast(inV0, inV1, inV2, inActiveEdges, inSubShapeID2);
		}

		RayInvDirection		mInvDirection;
		Vec3				mBoxCenter;
		Vec3				mBoxExtent;
		float				mDistanceStack[NodeCodec::StackSize];
	};

	JPH_ASSERT(inShape->GetSubType() == EShapeSubType::Mesh);
	const MeshShape *shape = static_cast<const MeshShape *>(inShape);

	Visitor visitor(inShapeCast, inShapeCastSettings, inScale, inCenterOfMassTransform2, inSubShapeIDCreator1, ioCollector);
	visitor.mInvDirection.Set(inShapeCast.mDirection);
	visitor.mBoxCenter = inShapeCast.mShapeWorldBounds.GetCenter();
	visitor.mBoxExtent = inShapeCast.mShapeWorldBounds.GetExtent();
	shape->WalkTreePerTriangle(inSubShapeIDCreator2, visitor);
}

void MeshShape::sCastSphereVsMesh(const ShapeCast &inShapeCast, const ShapeCastSettings &inShapeCastSettings, const Shape *inShape, Vec3Arg inScale, [[maybe_unused]] const ShapeFilter &inShapeFilter, Mat44Arg inCenterOfMassTransform2, const SubShapeIDCreator &inSubShapeIDCreator1, const SubShapeIDCreator &inSubShapeIDCreator2, CastShapeCollector &ioCollector)
{
	JPH_PROFILE_FUNCTION();

	struct Visitor : public CastSphereVsTriangles
	{
		using CastSphereVsTriangles::CastSphereVsTriangles;

		JPH_INLINE bool		ShouldAbort() const
		{
			return mCollector.ShouldEarlyOut();
		}

		JPH_INLINE bool		ShouldVisitNode(int inStackTop) const
		{
			return mDistanceStack[inStackTop] < mCollector.GetPositiveEarlyOutFraction();
		}

		JPH_INLINE int		VisitNodes(Vec4Arg inBoundsMinX, Vec4Arg inBoundsMinY, Vec4Arg inBoundsMinZ, Vec4Arg inBoundsMaxX, Vec4Arg inBoundsMaxY, Vec4Arg inBoundsMaxZ, UVec4 &ioProperties, int inStackTop)
		{
			// Scale the bounding boxes of this node
			Vec4 bounds_min_x, bounds_min_y, bounds_min_z, bounds_max_x, bounds_max_y, bounds_max_z;
			AABox4Scale(mScale, inBoundsMinX, inBoundsMinY, inBoundsMinZ, inBoundsMaxX, inBoundsMaxY, inBoundsMaxZ, bounds_min_x, bounds_min_y, bounds_min_z, bounds_max_x, bounds_max_y, bounds_max_z);

			// Enlarge them by the radius of the sphere
			AABox4EnlargeWithExtent(Vec3::sReplicate(mRadius), bounds_min_x, bounds_min_y, bounds_min_z, bounds_max_x, bounds_max_y, bounds_max_z);

			// Test bounds of 4 children
			Vec4 distance = RayAABox4(mStart, mInvDirection, bounds_min_x, bounds_min_y, bounds_min_z, bounds_max_x, bounds_max_y, bounds_max_z);

			// Sort so that highest values are first (we want to first process closer hits and we process stack top to bottom)
			return SortReverseAndStore(distance, mCollector.GetPositiveEarlyOutFraction(), ioProperties, &mDistanceStack[inStackTop]);
		}

		JPH_INLINE void		VisitTriangle(Vec3Arg inV0, Vec3Arg inV1, Vec3Arg inV2, uint8 inActiveEdges, SubShapeID inSubShapeID2)
		{
			Cast(inV0, inV1, inV2, inActiveEdges, inSubShapeID2);
		}

		RayInvDirection		mInvDirection;
		float				mDistanceStack[NodeCodec::StackSize];
	};

	JPH_ASSERT(inShape->GetSubType() == EShapeSubType::Mesh);
	const MeshShape *shape = static_cast<const MeshShape *>(inShape);

	Visitor visitor(inShapeCast, inShapeCastSettings, inScale, inCenterOfMassTransform2, inSubShapeIDCreator1, ioCollector);
	visitor.mInvDirection.Set(inShapeCast.mDirection);
	shape->WalkTreePerTriangle(inSubShapeIDCreator2, visitor);
}

struct MeshShape::MSGetTrianglesContext
{
	JPH_INLINE		MSGetTrianglesContext(const MeshShape *inShape, const AABox &inBox, Vec3Arg inPositionCOM, QuatArg inRotation, Vec3Arg inScale) :
		mDecodeCtx(sGetNodeHeader(inShape->mTree)),
		mShape(inShape),
		mLocalBox(Mat44::sInverseRotationTranslation(inRotation, inPositionCOM), inBox),
		mMeshScale(inScale),
		mLocalToWorld(Mat44::sRotationTranslation(inRotation, inPositionCOM) * Mat44::sScale(inScale)),
		mIsInsideOut(ScaleHelpers::IsInsideOut(inScale))
	{
	}

	JPH_INLINE bool	ShouldAbort() const
	{
		return mShouldAbort;
	}

	JPH_INLINE bool	ShouldVisitNode([[maybe_unused]] int inStackTop) const
	{
		return true;
	}

	JPH_INLINE int	VisitNodes(Vec4Arg inBoundsMinX, Vec4Arg inBoundsMinY, Vec4Arg inBoundsMinZ, Vec4Arg inBoundsMaxX, Vec4Arg inBoundsMaxY, Vec4Arg inBoundsMaxZ, UVec4 &ioProperties, [[maybe_unused]] int inStackTop) const
	{
		// Scale the bounding boxes of this node
		Vec4 bounds_min_x, bounds_min_y, bounds_min_z, bounds_max_x, bounds_max_y, bounds_max_z;
		AABox4Scale(mMeshScale, inBoundsMinX, inBoundsMinY, inBoundsMinZ, inBoundsMaxX, inBoundsMaxY, inBoundsMaxZ, bounds_min_x, bounds_min_y, bounds_min_z, bounds_max_x, bounds_max_y, bounds_max_z);

		// Test which nodes collide
		UVec4 collides = AABox4VsBox(mLocalBox, bounds_min_x, bounds_min_y, bounds_min_z, bounds_max_x, bounds_max_y, bounds_max_z);
		return CountAndSortTrues(collides, ioProperties);
	}

	JPH_INLINE void	VisitTriangles(const TriangleCodec::DecodingContext &ioContext, const void *inTriangles, int inNumTriangles, [[maybe_unused]] uint32 inTriangleBlockID)
	{
		// When the buffer is full and we cannot process the triangles, abort the tree walk. The next time GetTrianglesNext is called we will continue here.
		if (mNumTrianglesFound + inNumTriangles > mMaxTrianglesRequested)
		{
			mShouldAbort = true;
			return;
		}

		// Decode vertices
		JPH_ASSERT(inNumTriangles <= MaxTrianglesPerLeaf);
		Vec3 vertices[MaxTrianglesPerLeaf * 3];
		ioContext.Unpack(inTriangles, inNumTriangles, vertices);

		// Store vertices as Float3
		if (mIsInsideOut)
		{
			// Scaled inside out, flip the triangles
			for (const Vec3 *v = vertices, *v_end = v + 3 * inNumTriangles; v < v_end; v += 3)
			{
				(mLocalToWorld * v[0]).StoreFloat3(mTriangleVertices++);
				(mLocalToWorld * v[2]).StoreFloat3(mTriangleVertices++);
				(mLocalToWorld * v[1]).StoreFloat3(mTriangleVertices++);
			}
		}
		else
		{
			// Normal scale
			for (const Vec3 *v = vertices, *v_end = v + 3 * inNumTriangles; v < v_end; ++v)
				(mLocalToWorld * *v).StoreFloat3(mTriangleVertices++);
		}

		if (mMaterials != nullptr)
		{
			if (mShape->mMaterials.empty())
			{
				// No materials, output default
				const PhysicsMaterial *default_material = PhysicsMaterial::sDefault;
				for (int m = 0; m < inNumTriangles; ++m)
					*mMaterials++ = default_material;
			}
			else
			{
				// Decode triangle flags
				uint8 flags[MaxTrianglesPerLeaf];
				TriangleCodec::DecodingContext::sGetFlags(inTriangles, inNumTriangles, flags);

				// Store materials
				for (const uint8 *f = flags, *f_end = f + inNumTriangles; f < f_end; ++f)
					*mMaterials++ = mShape->mMaterials[*f & FLAGS_MATERIAL_MASK].GetPtr();
			}
		}

		// Accumulate triangles found
		mNumTrianglesFound += inNumTriangles;
	}

	NodeCodec::DecodingContext	mDecodeCtx;
	const MeshShape *			mShape;
	OrientedBox					mLocalBox;
	Vec3						mMeshScale;
	Mat44						mLocalToWorld;
	int							mMaxTrianglesRequested;
	Float3 *					mTriangleVertices;
	int							mNumTrianglesFound;
	const PhysicsMaterial **	mMaterials;
	bool						mShouldAbort;
	bool						mIsInsideOut;
};

void MeshShape::GetTrianglesStart(GetTrianglesContext &ioContext, const AABox &inBox, Vec3Arg inPositionCOM, QuatArg inRotation, Vec3Arg inScale) const
{
	static_assert(sizeof(MSGetTrianglesContext) <= sizeof(GetTrianglesContext), "GetTrianglesContext too small");
	JPH_ASSERT(IsAligned(&ioContext, alignof(MSGetTrianglesContext)));

	new (&ioContext) MSGetTrianglesContext(this, inBox, inPositionCOM, inRotation, inScale);
}

int MeshShape::GetTrianglesNext(GetTrianglesContext &ioContext, int inMaxTrianglesRequested, Float3 *outTriangleVertices, const PhysicsMaterial **outMaterials) const
{
	static_assert(cGetTrianglesMinTrianglesRequested >= MaxTrianglesPerLeaf, "cGetTrianglesMinTrianglesRequested is too small");
	JPH_ASSERT(inMaxTrianglesRequested >= cGetTrianglesMinTrianglesRequested);

	// Check if we're done
	MSGetTrianglesContext &context = (MSGetTrianglesContext &)ioContext;
	if (context.mDecodeCtx.IsDoneWalking())
		return 0;

	// Store parameters on context
	context.mMaxTrianglesRequested = inMaxTrianglesRequested;
	context.mTriangleVertices = outTriangleVertices;
	context.mMaterials = outMaterials;
	context.mShouldAbort = false; // Reset the abort flag
	context.mNumTrianglesFound = 0;

	// Continue (or start) walking the tree
	const TriangleCodec::DecodingContext triangle_ctx(sGetTriangleHeader(mTree));
	const uint8 *buffer_start = &mTree[0];
	context.mDecodeCtx.WalkTree(buffer_start, triangle_ctx, context);
	return context.mNumTrianglesFound;
}

void MeshShape::sCollideConvexVsMesh(const Shape *inShape1, const Shape *inShape2, Vec3Arg inScale1, Vec3Arg inScale2, Mat44Arg inCenterOfMassTransform1, Mat44Arg inCenterOfMassTransform2, const SubShapeIDCreator &inSubShapeIDCreator1, const SubShapeIDCreator &inSubShapeIDCreator2, const CollideShapeSettings &inCollideShapeSettings, CollideShapeCollector &ioCollector, [[maybe_unused]] const ShapeFilter &inShapeFilter)
{
	JPH_PROFILE_FUNCTION();

	// Get the shapes
	JPH_ASSERT(inShape1->GetType() == EShapeType::Convex);
	JPH_ASSERT(inShape2->GetType() == EShapeType::Mesh);
	const ConvexShape *shape1 = static_cast<const ConvexShape *>(inShape1);
	const MeshShape *shape2 = static_cast<const MeshShape *>(inShape2);

	struct Visitor : public CollideConvexVsTriangles
	{
		using CollideConvexVsTriangles::CollideConvexVsTriangles;

		JPH_INLINE bool	ShouldAbort() const
		{
			return mCollector.ShouldEarlyOut();
		}

		JPH_INLINE bool	ShouldVisitNode([[maybe_unused]] int inStackTop) const
		{
			return true;
		}

		JPH_INLINE int	VisitNodes(Vec4Arg inBoundsMinX, Vec4Arg inBoundsMinY, Vec4Arg inBoundsMinZ, Vec4Arg inBoundsMaxX, Vec4Arg inBoundsMaxY, Vec4Arg inBoundsMaxZ, UVec4 &ioProperties, [[maybe_unused]] int inStackTop) const
		{
			// Scale the bounding boxes of this node
			Vec4 bounds_min_x, bounds_min_y, bounds_min_z, bounds_max_x, bounds_max_y, bounds_max_z;
			AABox4Scale(mScale2, inBoundsMinX, inBoundsMinY, inBoundsMinZ, inBoundsMaxX, inBoundsMaxY, inBoundsMaxZ, bounds_min_x, bounds_min_y, bounds_min_z, bounds_max_x, bounds_max_y, bounds_max_z);

			// Test which nodes collide
			UVec4 collides = AABox4VsBox(mBoundsOf1InSpaceOf2, bounds_min_x, bounds_min_y, bounds_min_z, bounds_max_x, bounds_max_y, bounds_max_z);
			return CountAndSortTrues(collides, ioProperties);
		}

		JPH_INLINE void	VisitTriangle(Vec3Arg inV0, Vec3Arg inV1, Vec3Arg inV2, uint8 inActiveEdges, SubShapeID inSubShapeID2)
		{
			Collide(inV0, inV1, inV2, inActiveEdges, inSubShapeID2);
		}
	};

	Visitor visitor(shape1, inScale1, inScale2, inCenterOfMassTransform1, inCenterOfMassTransform2, inSubShapeIDCreator1.GetID(), inCollideShapeSettings, ioCollector);
	shape2->WalkTreePerTriangle(inSubShapeIDCreator2, visitor);
}

void MeshShape::sCollideSphereVsMesh(const Shape *inShape1, const Shape *inShape2, Vec3Arg inScale1, Vec3Arg inScale2, Mat44Arg inCenterOfMassTransform1, Mat44Arg inCenterOfMassTransform2, const SubShapeIDCreator &inSubShapeIDCreator1, const SubShapeIDCreator &inSubShapeIDCreator2, const CollideShapeSettings &inCollideShapeSettings, CollideShapeCollector &ioCollector, [[maybe_unused]] const ShapeFilter &inShapeFilter)
{
	JPH_PROFILE_FUNCTION();

	// Get the shapes
	JPH_ASSERT(inShape1->GetSubType() == EShapeSubType::Sphere);
	JPH_ASSERT(inShape2->GetType() == EShapeType::Mesh);
	const SphereShape *shape1 = static_cast<const SphereShape *>(inShape1);
	const MeshShape *shape2 = static_cast<const MeshShape *>(inShape2);

	struct Visitor : public CollideSphereVsTriangles
	{
		using CollideSphereVsTriangles::CollideSphereVsTriangles;

		JPH_INLINE bool	ShouldAbort() const
		{
			return mCollector.ShouldEarlyOut();
		}

		JPH_INLINE bool	ShouldVisitNode([[maybe_unused]] int inStackTop) const
		{
			return true;
		}

		JPH_INLINE int	VisitNodes(Vec4Arg inBoundsMinX, Vec4Arg inBoundsMinY, Vec4Arg inBoundsMinZ, Vec4Arg inBoundsMaxX, Vec4Arg inBoundsMaxY, Vec4Arg inBoundsMaxZ, UVec4 &ioProperties, [[maybe_unused]] int inStackTop) const
		{
			// Scale the bounding boxes of this node
			Vec4 bounds_min_x, bounds_min_y, bounds_min_z, bounds_max_x, bounds_max_y, bounds_max_z;
			AABox4Scale(mScale2, inBoundsMinX, inBoundsMinY, inBoundsMinZ, inBoundsMaxX, inBoundsMaxY, inBoundsMaxZ, bounds_min_x, bounds_min_y, bounds_min_z, bounds_max_x, bounds_max_y, bounds_max_z);

			// Test which nodes collide
			UVec4 collides = AABox4VsSphere(mSphereCenterIn2, mRadiusPlusMaxSeparationSq, bounds_min_x, bounds_min_y, bounds_min_z, bounds_max_x, bounds_max_y, bounds_max_z);
			return CountAndSortTrues(collides, ioProperties);
		}

		JPH_INLINE void	VisitTriangle(Vec3Arg inV0, Vec3Arg inV1, Vec3Arg inV2, uint8 inActiveEdges, SubShapeID inSubShapeID2)
		{
			Collide(inV0, inV1, inV2, inActiveEdges, inSubShapeID2);
		}
	};

	Visitor visitor(shape1, inScale1, inScale2, inCenterOfMassTransform1, inCenterOfMassTransform2, inSubShapeIDCreator1.GetID(), inCollideShapeSettings, ioCollector);
	shape2->WalkTreePerTriangle(inSubShapeIDCreator2, visitor);
}

void MeshShape::SaveBinaryState(StreamOut &inStream) const
{
	Shape::SaveBinaryState(inStream);

	inStream.Write(static_cast<const ByteBufferVector &>(mTree)); // Make sure we use the Array<> overload
}

void MeshShape::RestoreBinaryState(StreamIn &inStream)
{
	Shape::RestoreBinaryState(inStream);

	inStream.Read(static_cast<ByteBufferVector &>(mTree)); // Make sure we use the Array<> overload
}

void MeshShape::SaveMaterialState(PhysicsMaterialList &outMaterials) const
{
	outMaterials = mMaterials;
}

void MeshShape::RestoreMaterialState(const PhysicsMaterialRefC *inMaterials, uint inNumMaterials)
{
	mMaterials.assign(inMaterials, inMaterials + inNumMaterials);
}

Shape::Stats MeshShape::GetStats() const
{
	// Walk the tree to count the triangles
	struct Visitor
	{
		JPH_INLINE bool		ShouldAbort() const
		{
			return false;
		}

		JPH_INLINE bool		ShouldVisitNode([[maybe_unused]] int inStackTop) const
		{
			return true;
		}

		JPH_INLINE int		VisitNodes(Vec4Arg inBoundsMinX, Vec4Arg inBoundsMinY, Vec4Arg inBoundsMinZ, Vec4Arg inBoundsMaxX, Vec4Arg inBoundsMaxY, Vec4Arg inBoundsMaxZ, UVec4 &ioProperties, [[maybe_unused]] int inStackTop) const
		{
			// Visit all valid children
			UVec4 valid = UVec4::sOr(UVec4::sOr(Vec4::sLess(inBoundsMinX, inBoundsMaxX), Vec4::sLess(inBoundsMinY, inBoundsMaxY)), Vec4::sLess(inBoundsMinZ, inBoundsMaxZ));
			return CountAndSortTrues(valid, ioProperties);
		}

		JPH_INLINE void		VisitTriangles([[maybe_unused]] const TriangleCodec::DecodingContext &ioContext, [[maybe_unused]] const void *inTriangles, int inNumTriangles, [[maybe_unused]] uint32 inTriangleBlockID)
		{
			mNumTriangles += inNumTriangles;
		}

		uint				mNumTriangles = 0;
	};

	Visitor visitor;
	WalkTree(visitor);

	return Stats(sizeof(*this) + mMaterials.size() * sizeof(Ref<PhysicsMaterial>) + mTree.size() * sizeof(uint8), visitor.mNumTriangles);
}

uint32 MeshShape::GetTriangleUserData(const SubShapeID &inSubShapeID) const
{
	// Decode ID
	const void *block_start;
	uint32 triangle_idx;
	DecodeSubShapeID(inSubShapeID, block_start, triangle_idx);

	// Decode triangle
	const TriangleCodec::DecodingContext triangle_ctx(sGetTriangleHeader(mTree));
	return triangle_ctx.GetUserData(block_start, triangle_idx);
}

void MeshShape::sRegister()
{
	ShapeFunctions &f = ShapeFunctions::sGet(EShapeSubType::Mesh);
	f.mConstruct = []() -> Shape * { return new MeshShape; };
	f.mColor = Color::sRed;

	for (EShapeSubType s : sConvexSubShapeTypes)
	{
		CollisionDispatch::sRegisterCollideShape(s, EShapeSubType::Mesh, sCollideConvexVsMesh);
		CollisionDispatch::sRegisterCastShape(s, EShapeSubType::Mesh, sCastConvexVsMesh);

		CollisionDispatch::sRegisterCastShape(EShapeSubType::Mesh, s, CollisionDispatch::sReversedCastShape);
		CollisionDispatch::sRegisterCollideShape(EShapeSubType::Mesh, s, CollisionDispatch::sReversedCollideShape);
	}

	// Specialized collision functions
	CollisionDispatch::sRegisterCollideShape(EShapeSubType::Sphere, EShapeSubType::Mesh, sCollideSphereVsMesh);
	CollisionDispatch::sRegisterCastShape(EShapeSubType::Sphere, EShapeSubType::Mesh, sCastSphereVsMesh);
}

JPH_NAMESPACE_END

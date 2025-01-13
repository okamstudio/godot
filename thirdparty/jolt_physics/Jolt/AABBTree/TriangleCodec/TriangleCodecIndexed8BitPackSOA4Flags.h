// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Jolt/Geometry/RayTriangle.h>

JPH_NAMESPACE_BEGIN

/// Store vertices in 64 bits and indices in 8 bits + 8 bit of flags per triangle like this:
///
/// TriangleBlockHeader,
/// TriangleBlock (4 triangles and their flags in 16 bytes),
/// TriangleBlock...
/// [Optional] UserData (4 bytes per triangle)
///
/// Vertices are stored:
///
/// VertexData (1 vertex in 64 bits),
/// VertexData...
///
/// They're compressed relative to the bounding box as provided by the node codec.
class TriangleCodecIndexed8BitPackSOA4Flags
{
public:
	class TriangleHeader
	{
	public:
		Float3						mOffset;			///< Offset of all vertices
		Float3						mScale;				///< Scale of all vertices, vertex_position = mOffset + mScale * compressed_vertex_position
	};

	/// Size of the header (an empty struct is always > 0 bytes so this needs a separate variable)
	static constexpr int			TriangleHeaderSize = sizeof(TriangleHeader);

	/// If this codec could return a different offset than the current buffer size when calling Pack()
	static constexpr bool			ChangesOffsetOnPack = false;

	/// Amount of bits per component
	enum EComponentData : uint32
	{
		COMPONENT_BITS = 21,
		COMPONENT_MASK = (1 << COMPONENT_BITS) - 1,
	};

	/// Packed X and Y coordinate
	enum EVertexXY : uint32
	{
		COMPONENT_X = 0,
		COMPONENT_Y1 = COMPONENT_BITS,
		COMPONENT_Y1_BITS = 32 - COMPONENT_BITS,
	};

	/// Packed Z and Y coordinate
	enum EVertexZY : uint32
	{
		COMPONENT_Z = 0,
		COMPONENT_Y2 = COMPONENT_BITS,
		COMPONENT_Y2_BITS = 31 - COMPONENT_BITS,
	};

	/// A single packed vertex
	struct VertexData
	{
		uint32						mVertexXY;
		uint32						mVertexZY;
	};

	static_assert(sizeof(VertexData) == 8, "Compiler added padding");

	/// A block of 4 triangles
	struct TriangleBlock
	{
		uint8						mIndices[3][4];				///< 8 bit indices to triangle vertices for 4 triangles in the form mIndices[vertex][triangle] where vertex in [0, 2] and triangle in [0, 3]
		uint8						mFlags[4];					///< Triangle flags (could contain material and active edges)
	};

	static_assert(sizeof(TriangleBlock) == 16, "Compiler added padding");

	enum ETriangleBlockHeaderFlags : uint32
	{
		OFFSET_TO_VERTICES_BITS = 29,							///< Offset from current block to start of vertices in bytes
		OFFSET_TO_VERTICES_MASK = (1 << OFFSET_TO_VERTICES_BITS) - 1,
		OFFSET_NON_SIGNIFICANT_BITS = 2,						///< The offset from the current block to the start of the vertices must be a multiple of 4 bytes
		OFFSET_NON_SIGNIFICANT_MASK = (1 << OFFSET_NON_SIGNIFICANT_BITS) - 1,
		OFFSET_TO_USERDATA_BITS = 3,							///< When user data is stored, this is the number of blocks to skip to get to the user data (0 = no user data)
		OFFSET_TO_USERDATA_MASK = (1 << OFFSET_TO_USERDATA_BITS) - 1,
	};

	/// A triangle header, will be followed by one or more TriangleBlocks
	struct TriangleBlockHeader
	{
		const VertexData *			GetVertexData() const		{ return reinterpret_cast<const VertexData *>(reinterpret_cast<const uint8 *>(this) + ((mFlags & OFFSET_TO_VERTICES_MASK) << OFFSET_NON_SIGNIFICANT_BITS)); }
		const TriangleBlock *		GetTriangleBlock() const	{ return reinterpret_cast<const TriangleBlock *>(reinterpret_cast<const uint8 *>(this) + sizeof(TriangleBlockHeader)); }
		const uint32 *				GetUserData() const			{ uint32 offset = mFlags >> OFFSET_TO_VERTICES_BITS; return offset == 0? nullptr : reinterpret_cast<const uint32 *>(GetTriangleBlock() + offset); }

		uint32						mFlags;
	};

	static_assert(sizeof(TriangleBlockHeader) == 4, "Compiler added padding");

	/// This class is used to validate that the triangle data will not be degenerate after compression
	class ValidationContext
	{
	public:
		/// Constructor
									ValidationContext(const IndexedTriangleList &inTriangles, const VertexList &inVertices) :
			mVertices(inVertices)
		{
			// Only used the referenced triangles, just like EncodingContext::Finalize does
			for (const IndexedTriangle &i : inTriangles)
				for (uint32 idx : i.mIdx)
					mBounds.Encapsulate(Vec3(inVertices[idx]));
		}

		/// Test if a triangle will be degenerate after quantization
		bool						IsDegenerate(const IndexedTriangle &inTriangle) const
		{
			// Quantize the triangle in the same way as EncodingContext::Finalize does
			UVec4 quantized_vertex[3];
			Vec3 compress_scale = Vec3::sReplicate(COMPONENT_MASK) / Vec3::sMax(mBounds.GetSize(), Vec3::sReplicate(1.0e-20f));
			for (int i = 0; i < 3; ++i)
				quantized_vertex[i] = ((Vec3(mVertices[inTriangle.mIdx[i]]) - mBounds.mMin) * compress_scale + Vec3::sReplicate(0.5f)).ToInt();
			return quantized_vertex[0] == quantized_vertex[1] || quantized_vertex[1] == quantized_vertex[2] || quantized_vertex[0] == quantized_vertex[2];
		}

	private:
		const VertexList &			mVertices;
		AABox						mBounds;
	};

	/// This class is used to encode and compress triangle data into a byte buffer
	class EncodingContext
	{
	public:
		/// Indicates a vertex hasn't been seen yet in the triangle list
		static constexpr uint32		cNotFound = 0xffffffff;

		/// Construct the encoding context
		explicit					EncodingContext(const VertexList &inVertices) :
			mVertexMap(inVertices.size(), cNotFound)
		{
		}

		/// Mimics the size a call to Pack() would add to the buffer
		void						PreparePack(const IndexedTriangle *inTriangles, uint inNumTriangles, bool inStoreUserData, uint64 &ioBufferSize)
		{
			// Add triangle block header
			ioBufferSize += sizeof(TriangleBlockHeader);

			// Compute first vertex that this batch will use (ensuring there's enough room if none of the vertices are shared)
			uint start_vertex = Clamp((int)mVertexCount - 256 + (int)inNumTriangles * 3, 0, (int)mVertexCount);

			// Pack vertices
			uint padded_triangle_count = AlignUp(inNumTriangles, 4);
			for (uint t = 0; t < padded_triangle_count; t += 4)
			{
				// Add triangle block header
				ioBufferSize += sizeof(TriangleBlock);

				for (uint vertex_nr = 0; vertex_nr < 3; ++vertex_nr)
					for (uint block_tri_idx = 0; block_tri_idx < 4; ++block_tri_idx)
					{
						// Fetch vertex index. Create degenerate triangles for padding triangles.
						bool triangle_available = t + block_tri_idx < inNumTriangles;
						uint32 src_vertex_index = triangle_available? inTriangles[t + block_tri_idx].mIdx[vertex_nr] : inTriangles[inNumTriangles - 1].mIdx[0];

						// Check if we've seen this vertex before and if it is in the range that we can encode
						uint32 &vertex_index = mVertexMap[src_vertex_index];
						if (vertex_index == cNotFound || vertex_index < start_vertex)
						{
							// Add vertex
							vertex_index = mVertexCount;
							mVertexCount++;
						}
					}
			}

			// Add user data
			if (inStoreUserData)
				ioBufferSize += inNumTriangles * sizeof(uint32);
		}

		/// Mimics the size the Finalize() call would add to ioBufferSize
		void						FinalizePreparePack(uint64 &ioBufferSize)
		{
			// Remember where the vertices are going to start in the output buffer
			JPH_ASSERT(IsAligned(ioBufferSize, 4));
			mVerticesStartIdx = size_t(ioBufferSize);

			// Add vertices to buffer
			ioBufferSize += uint64(mVertexCount) * sizeof(VertexData);

			// Reserve the amount of memory we need for the vertices
			mVertices.reserve(mVertexCount);

			// Set vertex map back to 'not found'
			for (uint32 &v : mVertexMap)
				v = cNotFound;
		}

		/// Pack the triangles in inContainer to ioBuffer. This stores the mMaterialIndex of a triangle in the 8 bit flags.
		/// Returns size_t(-1) on error.
		size_t						Pack(const IndexedTriangle *inTriangles, uint inNumTriangles, bool inStoreUserData, ByteBuffer &ioBuffer, const char *&outError)
		{
			JPH_ASSERT(inNumTriangles > 0);

			// Determine position of triangles start
			size_t triangle_block_start = ioBuffer.size();

			// Allocate triangle block header
			TriangleBlockHeader *header = ioBuffer.Allocate<TriangleBlockHeader>();

			// Compute first vertex that this batch will use (ensuring there's enough room if none of the vertices are shared)
			uint start_vertex = Clamp((int)mVertices.size() - 256 + (int)inNumTriangles * 3, 0, (int)mVertices.size());

			// Store the start vertex offset relative to TriangleBlockHeader
			size_t offset_to_vertices = mVerticesStartIdx - triangle_block_start + size_t(start_vertex) * sizeof(VertexData);
			if (offset_to_vertices & OFFSET_NON_SIGNIFICANT_MASK)
			{
				outError = "TriangleCodecIndexed8BitPackSOA4Flags: Internal Error: Offset has non-significant bits set";
				return size_t(-1);
			}
			offset_to_vertices >>= OFFSET_NON_SIGNIFICANT_BITS;
			if (offset_to_vertices > OFFSET_TO_VERTICES_MASK)
			{
				outError = "TriangleCodecIndexed8BitPackSOA4Flags: Offset to vertices doesn't fit. Too much data.";
				return size_t(-1);
			}
			header->mFlags = uint32(offset_to_vertices);

			// When we store user data we need to store the offset to the user data in TriangleBlocks
			uint padded_triangle_count = AlignUp(inNumTriangles, 4);
			if (inStoreUserData)
			{
				uint32 num_blocks = padded_triangle_count >> 2;
				JPH_ASSERT(num_blocks <= OFFSET_TO_USERDATA_MASK);
				header->mFlags |= num_blocks << OFFSET_TO_VERTICES_BITS;
			}

			// Pack vertices
			for (uint t = 0; t < padded_triangle_count; t += 4)
			{
				TriangleBlock *block = ioBuffer.Allocate<TriangleBlock>();
				for (uint vertex_nr = 0; vertex_nr < 3; ++vertex_nr)
					for (uint block_tri_idx = 0; block_tri_idx < 4; ++block_tri_idx)
					{
						// Fetch vertex index. Create degenerate triangles for padding triangles.
						bool triangle_available = t + block_tri_idx < inNumTriangles;
						uint32 src_vertex_index = triangle_available? inTriangles[t + block_tri_idx].mIdx[vertex_nr] : inTriangles[inNumTriangles - 1].mIdx[0];

						// Check if we've seen this vertex before and if it is in the range that we can encode
						uint32 &vertex_index = mVertexMap[src_vertex_index];
						if (vertex_index == cNotFound || vertex_index < start_vertex)
						{
							// Add vertex
							vertex_index = (uint32)mVertices.size();
							mVertices.push_back(src_vertex_index);
						}

						// Store vertex index
						uint32 vertex_offset = vertex_index - start_vertex;
						if (vertex_offset > 0xff)
						{
							outError = "TriangleCodecIndexed8BitPackSOA4Flags: Offset doesn't fit in 8 bit";
							return size_t(-1);
						}
						block->mIndices[vertex_nr][block_tri_idx] = (uint8)vertex_offset;

						// Store flags
						uint32 flags = triangle_available? inTriangles[t + block_tri_idx].mMaterialIndex : 0;
						if (flags > 0xff)
						{
							outError = "TriangleCodecIndexed8BitPackSOA4Flags: Material index doesn't fit in 8 bit";
							return size_t(-1);
						}
						block->mFlags[block_tri_idx] = (uint8)flags;
					}
			}

			// Store user data
			if (inStoreUserData)
			{
				uint32 *user_data = ioBuffer.Allocate<uint32>(inNumTriangles);
				for (uint t = 0; t < inNumTriangles; ++t)
					user_data[t] = inTriangles[t].mUserData;
			}

			return triangle_block_start;
		}

		/// After all triangles have been packed, this finalizes the header and triangle buffer
		void						Finalize(const VertexList &inVertices, TriangleHeader *ioHeader, ByteBuffer &ioBuffer) const
		{
			// Assert that our reservations were correct
			JPH_ASSERT(mVertices.size() == mVertexCount);
			JPH_ASSERT(ioBuffer.size() == mVerticesStartIdx);

			// Check if anything to do
			if (mVertices.empty())
				return;

			// Calculate bounding box
			AABox bounds;
			for (uint32 v : mVertices)
				bounds.Encapsulate(Vec3(inVertices[v]));

			// Compress vertices
			VertexData *vertices = ioBuffer.Allocate<VertexData>(mVertices.size());
			Vec3 compress_scale = Vec3::sReplicate(COMPONENT_MASK) / Vec3::sMax(bounds.GetSize(), Vec3::sReplicate(1.0e-20f));
			for (uint32 v : mVertices)
			{
				UVec4 c = ((Vec3(inVertices[v]) - bounds.mMin) * compress_scale + Vec3::sReplicate(0.5f)).ToInt();
				JPH_ASSERT(c.GetX() <= COMPONENT_MASK);
				JPH_ASSERT(c.GetY() <= COMPONENT_MASK);
				JPH_ASSERT(c.GetZ() <= COMPONENT_MASK);
				vertices->mVertexXY = c.GetX() + (c.GetY() << COMPONENT_Y1);
				vertices->mVertexZY = c.GetZ() + ((c.GetY() >> COMPONENT_Y1_BITS) << COMPONENT_Y2);
				++vertices;
			}

			// Store decompression information
			bounds.mMin.StoreFloat3(&ioHeader->mOffset);
			(bounds.GetSize() / Vec3::sReplicate(COMPONENT_MASK)).StoreFloat3(&ioHeader->mScale);
		}

	private:
		using VertexMap = Array<uint32>;

		uint32						mVertexCount = 0;			///< Number of vertices calculated during PreparePack
		size_t						mVerticesStartIdx = 0;		///< Start of the vertices in the output buffer, calculated during PreparePack
		Array<uint32>				mVertices;					///< Output vertices as an index into the original vertex list (inVertices), sorted according to occurrence
		VertexMap					mVertexMap;					///< Maps from the original mesh vertex index (inVertices) to the index in our output vertices (mVertices)
	};

	/// This class is used to decode and decompress triangle data packed by the EncodingContext
	class DecodingContext
	{
	private:
		/// Private helper function to unpack the 1 vertex of 4 triangles (outX contains the x coordinate of triangle 0 .. 3 etc.)
		JPH_INLINE void				Unpack(const VertexData *inVertices, UVec4Arg inIndex, Vec4 &outX, Vec4 &outY, Vec4 &outZ) const
		{
			// Get compressed data
			UVec4 c1 = UVec4::sGatherInt4<8>(&inVertices->mVertexXY, inIndex);
			UVec4 c2 = UVec4::sGatherInt4<8>(&inVertices->mVertexZY, inIndex);

			// Unpack the x y and z component
			UVec4 xc = UVec4::sAnd(c1, UVec4::sReplicate(COMPONENT_MASK));
			UVec4 yc = UVec4::sOr(c1.LogicalShiftRight<COMPONENT_Y1>(), c2.LogicalShiftRight<COMPONENT_Y2>().LogicalShiftLeft<COMPONENT_Y1_BITS>());
			UVec4 zc = UVec4::sAnd(c2, UVec4::sReplicate(COMPONENT_MASK));

			// Convert to float
			outX = Vec4::sFusedMultiplyAdd(xc.ToFloat(), mScaleX, mOffsetX);
			outY = Vec4::sFusedMultiplyAdd(yc.ToFloat(), mScaleY, mOffsetY);
			outZ = Vec4::sFusedMultiplyAdd(zc.ToFloat(), mScaleZ, mOffsetZ);
		}

		/// Private helper function to unpack 4 triangles from a triangle block
		JPH_INLINE void				Unpack(const TriangleBlock *inBlock, const VertexData *inVertices, Vec4 &outX1, Vec4 &outY1, Vec4 &outZ1, Vec4 &outX2, Vec4 &outY2, Vec4 &outZ2, Vec4 &outX3, Vec4 &outY3, Vec4 &outZ3) const
		{
			// Get the indices for the three vertices (reads 4 bytes extra, but these are the flags so that's ok)
			UVec4 indices = UVec4::sLoadInt4(reinterpret_cast<const uint32 *>(&inBlock->mIndices[0]));
			UVec4 iv1 = indices.Expand4Byte0();
			UVec4 iv2 = indices.Expand4Byte4();
			UVec4 iv3 = indices.Expand4Byte8();

		#ifdef JPH_CPU_BIG_ENDIAN
			// On big endian systems we need to reverse the bytes
			iv1 = iv1.Swizzle<SWIZZLE_W, SWIZZLE_Z, SWIZZLE_Y, SWIZZLE_X>();
			iv2 = iv2.Swizzle<SWIZZLE_W, SWIZZLE_Z, SWIZZLE_Y, SWIZZLE_X>();
			iv3 = iv3.Swizzle<SWIZZLE_W, SWIZZLE_Z, SWIZZLE_Y, SWIZZLE_X>();
		#endif

			// Decompress the triangle data
			Unpack(inVertices, iv1, outX1, outY1, outZ1);
			Unpack(inVertices, iv2, outX2, outY2, outZ2);
			Unpack(inVertices, iv3, outX3, outY3, outZ3);
		}

	public:
		JPH_INLINE explicit			DecodingContext(const TriangleHeader *inHeader) :
			mOffsetX(Vec4::sReplicate(inHeader->mOffset.x)),
			mOffsetY(Vec4::sReplicate(inHeader->mOffset.y)),
			mOffsetZ(Vec4::sReplicate(inHeader->mOffset.z)),
			mScaleX(Vec4::sReplicate(inHeader->mScale.x)),
			mScaleY(Vec4::sReplicate(inHeader->mScale.y)),
			mScaleZ(Vec4::sReplicate(inHeader->mScale.z))
		{
		}

		/// Unpacks triangles in the format t1v1,t1v2,t1v3, t2v1,t2v2,t2v3, ...
		JPH_INLINE void				Unpack(const void *inTriangleStart, uint32 inNumTriangles, Vec3 *outTriangles) const
		{
			JPH_ASSERT(inNumTriangles > 0);
			const TriangleBlockHeader *header = reinterpret_cast<const TriangleBlockHeader *>(inTriangleStart);
			const VertexData *vertices = header->GetVertexData();
			const TriangleBlock *t = header->GetTriangleBlock();
			const TriangleBlock *end = t + ((inNumTriangles + 3) >> 2);

			int triangles_left = inNumTriangles;

			do
			{
				// Unpack the vertices for 4 triangles
				Vec4 v1x, v1y, v1z, v2x, v2y, v2z, v3x, v3y, v3z;
				Unpack(t, vertices, v1x, v1y, v1z, v2x, v2y, v2z, v3x, v3y, v3z);

				// Transpose it so we get normal vectors
				Mat44 v1 = Mat44(v1x, v1y, v1z, Vec4::sZero()).Transposed();
				Mat44 v2 = Mat44(v2x, v2y, v2z, Vec4::sZero()).Transposed();
				Mat44 v3 = Mat44(v3x, v3y, v3z, Vec4::sZero()).Transposed();

				// Store triangle data
				for (int i = 0; i < 4 && triangles_left > 0; ++i, --triangles_left)
				{
					*outTriangles++ = v1.GetColumn3(i);
					*outTriangles++ = v2.GetColumn3(i);
					*outTriangles++ = v3.GetColumn3(i);
				}

				++t;
			}
			while (t < end);
		}

		/// Tests a ray against the packed triangles
		JPH_INLINE float			TestRay(Vec3Arg inRayOrigin, Vec3Arg inRayDirection, const void *inTriangleStart, uint32 inNumTriangles, float inClosest, uint32 &outClosestTriangleIndex) const
		{
			JPH_ASSERT(inNumTriangles > 0);
			const TriangleBlockHeader *header = reinterpret_cast<const TriangleBlockHeader *>(inTriangleStart);
			const VertexData *vertices = header->GetVertexData();
			const TriangleBlock *t = header->GetTriangleBlock();
			const TriangleBlock *end = t + ((inNumTriangles + 3) >> 2);

			Vec4 closest = Vec4::sReplicate(inClosest);
			UVec4 closest_triangle_idx = UVec4::sZero();

			UVec4 start_triangle_idx = UVec4::sZero();
			do
			{
				// Unpack the vertices for 4 triangles
				Vec4 v1x, v1y, v1z, v2x, v2y, v2z, v3x, v3y, v3z;
				Unpack(t, vertices, v1x, v1y, v1z, v2x, v2y, v2z, v3x, v3y, v3z);

				// Perform ray vs triangle test
				Vec4 distance = RayTriangle4(inRayOrigin, inRayDirection, v1x, v1y, v1z, v2x, v2y, v2z, v3x, v3y, v3z);

				// Update closest with the smaller values
				UVec4 smaller = Vec4::sLess(distance, closest);
				closest = Vec4::sSelect(closest, distance, smaller);

				// Update triangle index with the smallest values
				UVec4 triangle_idx = start_triangle_idx + UVec4(0, 1, 2, 3);
				closest_triangle_idx = UVec4::sSelect(closest_triangle_idx, triangle_idx, smaller);

				// Next block
				++t;
				start_triangle_idx += UVec4::sReplicate(4);
			}
			while (t < end);

			// Get the smallest component
			Vec4::sSort4(closest, closest_triangle_idx);
			outClosestTriangleIndex = closest_triangle_idx.GetX();
			return closest.GetX();
		}

		/// Decode a single triangle
		inline void					GetTriangle(const void *inTriangleStart, uint32 inTriangleIdx, Vec3 &outV1, Vec3 &outV2, Vec3 &outV3) const
		{
			const TriangleBlockHeader *header = reinterpret_cast<const TriangleBlockHeader *>(inTriangleStart);
			const VertexData *vertices = header->GetVertexData();
			const TriangleBlock *block = header->GetTriangleBlock() + (inTriangleIdx >> 2);
			uint32 block_triangle_idx = inTriangleIdx & 0b11;

			// Get the 3 vertices
			const VertexData &v1 = vertices[block->mIndices[0][block_triangle_idx]];
			const VertexData &v2 = vertices[block->mIndices[1][block_triangle_idx]];
			const VertexData &v3 = vertices[block->mIndices[2][block_triangle_idx]];

			// Pack the vertices
			UVec4 c1(v1.mVertexXY, v2.mVertexXY, v3.mVertexXY, 0);
			UVec4 c2(v1.mVertexZY, v2.mVertexZY, v3.mVertexZY, 0);

			// Unpack the x y and z component
			UVec4 xc = UVec4::sAnd(c1, UVec4::sReplicate(COMPONENT_MASK));
			UVec4 yc = UVec4::sOr(c1.LogicalShiftRight<COMPONENT_Y1>(), c2.LogicalShiftRight<COMPONENT_Y2>().LogicalShiftLeft<COMPONENT_Y1_BITS>());
			UVec4 zc = UVec4::sAnd(c2, UVec4::sReplicate(COMPONENT_MASK));

			// Convert to float
			Vec4 vx = Vec4::sFusedMultiplyAdd(xc.ToFloat(), mScaleX, mOffsetX);
			Vec4 vy = Vec4::sFusedMultiplyAdd(yc.ToFloat(), mScaleY, mOffsetY);
			Vec4 vz = Vec4::sFusedMultiplyAdd(zc.ToFloat(), mScaleZ, mOffsetZ);

			// Transpose it so we get normal vectors
			Mat44 trans = Mat44(vx, vy, vz, Vec4::sZero()).Transposed();
			outV1 = trans.GetAxisX();
			outV2 = trans.GetAxisY();
			outV3 = trans.GetAxisZ();
		}

		/// Get user data for a triangle
		JPH_INLINE uint32			GetUserData(const void *inTriangleStart, uint32 inTriangleIdx) const
		{
			const TriangleBlockHeader *header = reinterpret_cast<const TriangleBlockHeader *>(inTriangleStart);
			const uint32 *user_data = header->GetUserData();
			return user_data != nullptr? user_data[inTriangleIdx] : 0;
		}

		/// Get flags for entire triangle block
		JPH_INLINE static void		sGetFlags(const void *inTriangleStart, uint32 inNumTriangles, uint8 *outTriangleFlags)
		{
			JPH_ASSERT(inNumTriangles > 0);
			const TriangleBlockHeader *header = reinterpret_cast<const TriangleBlockHeader *>(inTriangleStart);
			const TriangleBlock *t = header->GetTriangleBlock();
			const TriangleBlock *end = t + ((inNumTriangles + 3) >> 2);

			int triangles_left = inNumTriangles;
			do
			{
				for (int i = 0; i < 4 && triangles_left > 0; ++i, --triangles_left)
					*outTriangleFlags++ = t->mFlags[i];

				++t;
			}
			while (t < end);
		}

		/// Get flags for a particular triangle
		JPH_INLINE static uint8		sGetFlags(const void *inTriangleStart, int inTriangleIndex)
		{
			const TriangleBlockHeader *header = reinterpret_cast<const TriangleBlockHeader *>(inTriangleStart);
			const TriangleBlock *first_block = header->GetTriangleBlock();
			return first_block[inTriangleIndex >> 2].mFlags[inTriangleIndex & 0b11];
		}

		/// Unpacks triangles and flags, convenience function
		JPH_INLINE void				Unpack(const void *inTriangleStart, uint32 inNumTriangles, Vec3 *outTriangles, uint8 *outTriangleFlags) const
		{
			Unpack(inTriangleStart, inNumTriangles, outTriangles);
			sGetFlags(inTriangleStart, inNumTriangles, outTriangleFlags);
		}

	private:
		Vec4						mOffsetX;
		Vec4						mOffsetY;
		Vec4						mOffsetZ;
		Vec4						mScaleX;
		Vec4						mScaleY;
		Vec4						mScaleZ;
	};
};

JPH_NAMESPACE_END

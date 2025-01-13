// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#include <Jolt/Jolt.h>

#include <Jolt/Physics/Collision/Shape/HeightFieldShape.h>
#include <Jolt/Physics/Collision/Shape/ConvexShape.h>
#include <Jolt/Physics/Collision/Shape/ScaleHelpers.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/ShapeCast.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/CollidePointResult.h>
#include <Jolt/Physics/Collision/ShapeFilter.h>
#include <Jolt/Physics/Collision/CastConvexVsTriangles.h>
#include <Jolt/Physics/Collision/CastSphereVsTriangles.h>
#include <Jolt/Physics/Collision/CollideConvexVsTriangles.h>
#include <Jolt/Physics/Collision/CollideSphereVsTriangles.h>
#include <Jolt/Physics/Collision/TransformedShape.h>
#include <Jolt/Physics/Collision/ActiveEdges.h>
#include <Jolt/Physics/Collision/CollisionDispatch.h>
#include <Jolt/Physics/Collision/SortReverseAndStore.h>
#include <Jolt/Physics/Collision/CollideSoftBodyVerticesVsTriangles.h>
#include <Jolt/Core/Profiler.h>
#include <Jolt/Core/StringTools.h>
#include <Jolt/Core/StreamIn.h>
#include <Jolt/Core/StreamOut.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/ScopeExit.h>
#include <Jolt/Geometry/AABox4.h>
#include <Jolt/Geometry/RayTriangle.h>
#include <Jolt/Geometry/RayAABox.h>
#include <Jolt/Geometry/OrientedBox.h>
#include <Jolt/ObjectStream/TypeDeclarations.h>

//#define JPH_DEBUG_HEIGHT_FIELD

JPH_NAMESPACE_BEGIN

#ifdef JPH_DEBUG_RENDERER
bool HeightFieldShape::sDrawTriangleOutlines = false;
#endif // JPH_DEBUG_RENDERER

using namespace HeightFieldShapeConstants;

JPH_IMPLEMENT_SERIALIZABLE_VIRTUAL(HeightFieldShapeSettings)
{
	JPH_ADD_BASE_CLASS(HeightFieldShapeSettings, ShapeSettings)

	JPH_ADD_ATTRIBUTE(HeightFieldShapeSettings, mHeightSamples)
	JPH_ADD_ATTRIBUTE(HeightFieldShapeSettings, mOffset)
	JPH_ADD_ATTRIBUTE(HeightFieldShapeSettings, mScale)
	JPH_ADD_ATTRIBUTE(HeightFieldShapeSettings, mMinHeightValue)
	JPH_ADD_ATTRIBUTE(HeightFieldShapeSettings, mMaxHeightValue)
	JPH_ADD_ATTRIBUTE(HeightFieldShapeSettings, mMaterialsCapacity)
	JPH_ADD_ATTRIBUTE(HeightFieldShapeSettings, mSampleCount)
	JPH_ADD_ATTRIBUTE(HeightFieldShapeSettings, mBlockSize)
	JPH_ADD_ATTRIBUTE(HeightFieldShapeSettings, mBitsPerSample)
	JPH_ADD_ATTRIBUTE(HeightFieldShapeSettings, mMaterialIndices)
	JPH_ADD_ATTRIBUTE(HeightFieldShapeSettings, mMaterials)
	JPH_ADD_ATTRIBUTE(HeightFieldShapeSettings, mActiveEdgeCosThresholdAngle)
}

const uint HeightFieldShape::sGridOffsets[] =
{
	0,			// level:  0, max x/y:     0, offset: 0
	1,			// level:  1, max x/y:     1, offset: 1
	5,			// level:  2, max x/y:     3, offset: 1 + 4
	21,			// level:  3, max x/y:     7, offset: 1 + 4 + 16
	85,			// level:  4, max x/y:    15, offset: 1 + 4 + 16 + 64
	341,		// level:  5, max x/y:    31, offset: 1 + 4 + 16 + 64 + 256
	1365,		// level:  6, max x/y:    63, offset: 1 + 4 + 16 + 64 + 256 + 1024
	5461,		// level:  7, max x/y:   127, offset: 1 + 4 + 16 + 64 + 256 + 1024 + 4096
	21845,		// level:  8, max x/y:   255, offset: 1 + 4 + 16 + 64 + 256 + 1024 + 4096 + ...
	87381,		// level:  9, max x/y:   511, offset: 1 + 4 + 16 + 64 + 256 + 1024 + 4096 + ...
	349525,		// level: 10, max x/y:  1023, offset: 1 + 4 + 16 + 64 + 256 + 1024 + 4096 + ...
	1398101,	// level: 11, max x/y:  2047, offset: 1 + 4 + 16 + 64 + 256 + 1024 + 4096 + ...
	5592405,	// level: 12, max x/y:  4095, offset: 1 + 4 + 16 + 64 + 256 + 1024 + 4096 + ...
	22369621,	// level: 13, max x/y:  8191, offset: 1 + 4 + 16 + 64 + 256 + 1024 + 4096 + ...
	89478485,	// level: 14, max x/y: 16383, offset: 1 + 4 + 16 + 64 + 256 + 1024 + 4096 + ...
};

HeightFieldShapeSettings::HeightFieldShapeSettings(const float *inSamples, Vec3Arg inOffset, Vec3Arg inScale, uint32 inSampleCount, const uint8 *inMaterialIndices, const PhysicsMaterialList &inMaterialList) :
	mOffset(inOffset),
	mScale(inScale),
	mSampleCount(inSampleCount)
{
	mHeightSamples.assign(inSamples, inSamples + Square(inSampleCount));

	if (!inMaterialList.empty() && inMaterialIndices != nullptr)
	{
		mMaterialIndices.assign(inMaterialIndices, inMaterialIndices + Square(inSampleCount - 1));
		mMaterials = inMaterialList;
	}
	else
	{
		JPH_ASSERT(inMaterialList.empty());
		JPH_ASSERT(inMaterialIndices == nullptr);
	}
}

ShapeSettings::ShapeResult HeightFieldShapeSettings::Create() const
{
	if (mCachedResult.IsEmpty())
		Ref<Shape> shape = new HeightFieldShape(*this, mCachedResult);
	return mCachedResult;
}

void HeightFieldShapeSettings::DetermineMinAndMaxSample(float &outMinValue, float &outMaxValue, float &outQuantizationScale) const
{
	// Determine min and max value
	outMinValue = mMinHeightValue;
	outMaxValue = mMaxHeightValue;
	for (float h : mHeightSamples)
		if (h != cNoCollisionValue)
		{
			outMinValue = min(outMinValue, h);
			outMaxValue = max(outMaxValue, h);
		}

	// Prevent dividing by zero by setting a minimal height difference
	float height_diff = max(outMaxValue - outMinValue, 1.0e-6f);

	// Calculate the scale factor to quantize to 16 bits
	outQuantizationScale = float(cMaxHeightValue16) / height_diff;
}

uint32 HeightFieldShapeSettings::CalculateBitsPerSampleForError(float inMaxError) const
{
	// Start with 1 bit per sample
	uint32 bits_per_sample = 1;

	// Determine total range
	float min_value, max_value, scale;
	DetermineMinAndMaxSample(min_value, max_value, scale);
	if (min_value < max_value)
	{
		// Loop over all blocks
		for (uint y = 0; y < mSampleCount; y += mBlockSize)
			for (uint x = 0; x < mSampleCount; x += mBlockSize)
			{
				// Determine min and max block value + take 1 sample border just like we do while building the hierarchical grids
				float block_min_value = FLT_MAX, block_max_value = -FLT_MAX;
				for (uint bx = x; bx < min(x + mBlockSize + 1, mSampleCount); ++bx)
					for (uint by = y; by < min(y + mBlockSize + 1, mSampleCount); ++by)
					{
						float h = mHeightSamples[by * mSampleCount + bx];
						if (h != cNoCollisionValue)
						{
							block_min_value = min(block_min_value, h);
							block_max_value = max(block_max_value, h);
						}
					}

				if (block_min_value < block_max_value)
				{
					// Quantize then dequantize block min/max value
					block_min_value = min_value + floor((block_min_value - min_value) * scale) / scale;
					block_max_value = min_value + ceil((block_max_value - min_value) * scale) / scale;
					float block_height = block_max_value - block_min_value;

					// Loop over the block again
					for (uint bx = x; bx < x + mBlockSize; ++bx)
						for (uint by = y; by < y + mBlockSize; ++by)
						{
							// Get the height
							float height = mHeightSamples[by * mSampleCount + bx];
							if (height != cNoCollisionValue)
							{
								for (;;)
								{
									// Determine bitmask for sample
									uint32 sample_mask = (1 << bits_per_sample) - 1;

									// Quantize
									float quantized_height = floor((height - block_min_value) * float(sample_mask) / block_height);
									quantized_height = Clamp(quantized_height, 0.0f, float(sample_mask - 1));

									// Dequantize and check error
									float dequantized_height = block_min_value + (quantized_height + 0.5f) * block_height / float(sample_mask);
									if (abs(dequantized_height - height) <= inMaxError)
										break;

									// Not accurate enough, increase bits per sample
									bits_per_sample++;

									// Don't go above 8 bits per sample
									if (bits_per_sample == 8)
										return bits_per_sample;
								}
							}
						}
				}
			}

	}

	return bits_per_sample;
}

void HeightFieldShape::CalculateActiveEdges(uint inX, uint inY, uint inSizeX, uint inSizeY, const float *inHeights, uint inHeightsStartX, uint inHeightsStartY, intptr_t inHeightsStride, float inHeightsScale, float inActiveEdgeCosThresholdAngle, TempAllocator &inAllocator)
{
	// Allocate temporary buffer for normals
	uint normals_size = 2 * inSizeX * inSizeY * sizeof(Vec3);
	Vec3 *normals = (Vec3 *)inAllocator.Allocate(normals_size);
	JPH_SCOPE_EXIT([&inAllocator, normals, normals_size]{ inAllocator.Free(normals, normals_size); });

	// Calculate triangle normals and make normals zero for triangles that are missing
	Vec3 *out_normal = normals;
	for (uint y = 0; y < inSizeY; ++y)
		for (uint x = 0; x < inSizeX; ++x)
		{
			// Get height on diagonal
			const float *height_samples = inHeights + (inY - inHeightsStartY + y) * inHeightsStride + (inX - inHeightsStartX + x);
			float x1y1_h = height_samples[0];
			float x2y2_h = height_samples[inHeightsStride + 1];
			if (x1y1_h != cNoCollisionValue && x2y2_h != cNoCollisionValue)
			{
				// Calculate normal for lower left triangle (e.g. T1A)
				float x1y2_h = height_samples[inHeightsStride];
				if (x1y2_h != cNoCollisionValue)
				{
					Vec3 x2y2_minus_x1y2(mScale.GetX(), inHeightsScale * (x2y2_h - x1y2_h), 0);
					Vec3 x1y1_minus_x1y2(0, inHeightsScale * (x1y1_h - x1y2_h), -mScale.GetZ());
					out_normal[0] = x2y2_minus_x1y2.Cross(x1y1_minus_x1y2).Normalized();
				}
				else
					out_normal[0] = Vec3::sZero();

				// Calculate normal for upper right triangle (e.g. T1B)
				float x2y1_h = height_samples[1];
				if (x2y1_h != cNoCollisionValue)
				{
					Vec3 x1y1_minus_x2y1(-mScale.GetX(), inHeightsScale * (x1y1_h - x2y1_h), 0);
					Vec3 x2y2_minus_x2y1(0, inHeightsScale * (x2y2_h - x2y1_h), mScale.GetZ());
					out_normal[1] = x1y1_minus_x2y1.Cross(x2y2_minus_x2y1).Normalized();
				}
				else
					out_normal[1] = Vec3::sZero();
			}
			else
			{
				out_normal[0] = Vec3::sZero();
				out_normal[1] = Vec3::sZero();
			}

			out_normal += 2;
		}

	// Calculate active edges
	const Vec3 *in_normal = normals;
	uint global_bit_pos = 3 * (inY * (mSampleCount - 1) + inX);
	for (uint y = 0; y < inSizeY; ++y)
	{
		for (uint x = 0; x < inSizeX; ++x)
		{
			// Get vertex heights
			const float *height_samples = inHeights + (inY - inHeightsStartY + y) * inHeightsStride + (inX - inHeightsStartX + x);
			float x1y1_h = height_samples[0];
			float x1y2_h = height_samples[inHeightsStride];
			float x2y2_h = height_samples[inHeightsStride + 1];
			bool x1y1_valid = x1y1_h != cNoCollisionValue;
			bool x1y2_valid = x1y2_h != cNoCollisionValue;
			bool x2y2_valid = x2y2_h != cNoCollisionValue;

			// Calculate the edge flags (3 bits)
			// See diagram in the next function for the edge numbering
			uint16 edge_mask = 0b111;
			uint16 edge_flags = 0;

			// Edge 0
			if (x == 0)
				edge_mask &= 0b110; // We need normal x - 1 which we didn't calculate, don't update this edge
			else if (x1y1_valid && x1y2_valid)
			{
				Vec3 edge0_direction(0, inHeightsScale * (x1y2_h - x1y1_h), mScale.GetZ());
				if (ActiveEdges::IsEdgeActive(in_normal[0], in_normal[-1], edge0_direction, inActiveEdgeCosThresholdAngle))
					edge_flags |= 0b001;
			}

			// Edge 1
			if (y == inSizeY - 1)
				edge_mask &= 0b101; // We need normal y + 1 which we didn't calculate, don't update this edge
			else if (x1y2_valid && x2y2_valid)
			{
				Vec3 edge1_direction(mScale.GetX(), inHeightsScale * (x2y2_h - x1y2_h), 0);
				if (ActiveEdges::IsEdgeActive(in_normal[0], in_normal[2 * inSizeX + 1], edge1_direction, inActiveEdgeCosThresholdAngle))
					edge_flags |= 0b010;
			}

			// Edge 2
			if (x1y1_valid && x2y2_valid)
			{
				Vec3 edge2_direction(-mScale.GetX(), inHeightsScale * (x1y1_h - x2y2_h), -mScale.GetZ());
				if (ActiveEdges::IsEdgeActive(in_normal[0], in_normal[1], edge2_direction, inActiveEdgeCosThresholdAngle))
					edge_flags |= 0b100;
			}

			// Store the edge flags in the array
			uint byte_pos = global_bit_pos >> 3;
			uint bit_pos = global_bit_pos & 0b111;
			JPH_ASSERT(byte_pos < mActiveEdgesSize);
			uint8 *edge_flags_ptr = &mActiveEdges[byte_pos];
			uint16 combined_edge_flags = uint16(edge_flags_ptr[0]) | uint16(uint16(edge_flags_ptr[1]) << 8);
			combined_edge_flags &= ~(edge_mask << bit_pos);
			combined_edge_flags |= edge_flags << bit_pos;
			edge_flags_ptr[0] = uint8(combined_edge_flags);
			edge_flags_ptr[1] = uint8(combined_edge_flags >> 8);

			in_normal += 2;
			global_bit_pos += 3;
		}

		global_bit_pos += 3 * (mSampleCount - 1 - inSizeX);
	}
}

void HeightFieldShape::CalculateActiveEdges(const HeightFieldShapeSettings &inSettings)
{
	/*
		Store active edges. The triangles are organized like this:
			x --->

		y   +       +
			| \ T1B | \ T2B
		|  e0   e2  |   \
		|   | T1A \ | T2A \
		V   +--e1---+-------+
			| \ T3B | \ T4B
			|   \   |   \
			| T3A \ | T4A \
			+-------+-------+
		We store active edges e0 .. e2 as bits 0 .. 2.
		We store triangles horizontally then vertically (order T1A, T2A, T3A and T4A).
		The top edge and right edge of the heightfield are always active so we do not need to store them,
		therefore we only need to store (mSampleCount - 1)^2 * 3-bit
		The triangles T1B, T2B, T3B and T4B do not need to be stored, their active edges can be constructed from adjacent triangles.
		Add 1 byte padding so we can always read 1 uint16 to get the bits that cross an 8 bit boundary
	*/

	// Make all edges active (if mSampleCount is bigger than inSettings.mSampleCount we need to fill up the padding,
	// also edges at x = 0 and y = inSettings.mSampleCount - 1 are not updated)
	memset(mActiveEdges, 0xff, mActiveEdgesSize);

	// Now clear the edges that are not active
	TempAllocatorMalloc allocator;
	CalculateActiveEdges(0, 0, inSettings.mSampleCount - 1, inSettings.mSampleCount - 1, inSettings.mHeightSamples.data(), 0, 0, inSettings.mSampleCount, inSettings.mScale.GetY(), inSettings.mActiveEdgeCosThresholdAngle, allocator);
}

void HeightFieldShape::StoreMaterialIndices(const HeightFieldShapeSettings &inSettings)
{
	// We need to account for any rounding of the sample count to the nearest block size
	uint in_count_min_1 = inSettings.mSampleCount - 1;
	uint out_count_min_1 = mSampleCount - 1;

	mNumBitsPerMaterialIndex = 32 - CountLeadingZeros(max((uint32)mMaterials.size(), inSettings.mMaterialsCapacity) - 1);
	mMaterialIndices.resize(((Square(out_count_min_1) * mNumBitsPerMaterialIndex + 7) >> 3) + 1, 0); // Add 1 byte so we don't read out of bounds when reading an uint16

	if (mMaterials.size() > 1)
		for (uint y = 0; y < out_count_min_1; ++y)
			for (uint x = 0; x < out_count_min_1; ++x)
			{
				// Read material
				uint16 material_index = x < in_count_min_1 && y < in_count_min_1? uint16(inSettings.mMaterialIndices[x + y * in_count_min_1]) : 0;

				// Calculate byte and bit position where the material index needs to go
				uint sample_pos = x + y * out_count_min_1;
				uint bit_pos = sample_pos * mNumBitsPerMaterialIndex;
				uint byte_pos = bit_pos >> 3;
				bit_pos &= 0b111;

				// Write the material index
				material_index <<= bit_pos;
				JPH_ASSERT(byte_pos + 1 < mMaterialIndices.size());
				mMaterialIndices[byte_pos] |= uint8(material_index);
				mMaterialIndices[byte_pos + 1] |= uint8(material_index >> 8);
			}
}

void HeightFieldShape::CacheValues()
{
	mSampleMask = uint8((uint32(1) << mBitsPerSample) - 1);
}

void HeightFieldShape::AllocateBuffers()
{
	uint num_blocks = GetNumBlocks();
	uint max_stride = (num_blocks + 1) >> 1;
	mRangeBlocksSize = sGridOffsets[sGetMaxLevel(num_blocks) - 1] + Square(max_stride);
	mHeightSamplesSize = (mSampleCount * mSampleCount * mBitsPerSample + 7) / 8 + 1;
	mActiveEdgesSize = (Square(mSampleCount - 1) * 3 + 7) / 8 + 1; // See explanation at HeightFieldShape::CalculateActiveEdges

	JPH_ASSERT(mRangeBlocks == nullptr && mHeightSamples == nullptr && mActiveEdges == nullptr);
	void *data = AlignedAllocate(mRangeBlocksSize * sizeof(RangeBlock) + mHeightSamplesSize + mActiveEdgesSize, alignof(RangeBlock));
	mRangeBlocks = reinterpret_cast<RangeBlock *>(data);
	mHeightSamples = reinterpret_cast<uint8 *>(mRangeBlocks + mRangeBlocksSize);
	mActiveEdges = mHeightSamples + mHeightSamplesSize;
}

HeightFieldShape::HeightFieldShape(const HeightFieldShapeSettings &inSettings, ShapeResult &outResult) :
	Shape(EShapeType::HeightField, EShapeSubType::HeightField, inSettings, outResult),
	mOffset(inSettings.mOffset),
	mScale(inSettings.mScale),
	mSampleCount(((inSettings.mSampleCount + inSettings.mBlockSize - 1) / inSettings.mBlockSize) * inSettings.mBlockSize), // Round sample count to nearest block size
	mBlockSize(inSettings.mBlockSize),
	mBitsPerSample(uint8(inSettings.mBitsPerSample))
{
	CacheValues();

	// Reserve a bigger materials list if requested
	if (inSettings.mMaterialsCapacity > 0)
		mMaterials.reserve(inSettings.mMaterialsCapacity);
	mMaterials = inSettings.mMaterials;

	// Check block size
	if (mBlockSize < 2 || mBlockSize > 8)
	{
		outResult.SetError("HeightFieldShape: Block size must be in the range [2, 8]!");
		return;
	}

	// Check bits per sample
	if (inSettings.mBitsPerSample < 1 || inSettings.mBitsPerSample > 8)
	{
		outResult.SetError("HeightFieldShape: Bits per sample must be in the range [1, 8]!");
		return;
	}

	// We stop at mBlockSize x mBlockSize height sample blocks
	uint num_blocks = GetNumBlocks();

	// We want at least 1 grid layer
	if (num_blocks < 2)
	{
		outResult.SetError("HeightFieldShape: Sample count too low!");
		return;
	}

	// Check that we don't overflow our 32 bit 'properties'
	if (num_blocks > (1 << cNumBitsXY))
	{
		outResult.SetError("HeightFieldShape: Sample count too high!");
		return;
	}

	// Check if we're not exceeding the amount of sub shape id bits
	if (GetSubShapeIDBitsRecursive() > SubShapeID::MaxBits)
	{
		outResult.SetError("HeightFieldShape: Size exceeds the amount of available sub shape ID bits!");
		return;
	}

	if (!mMaterials.empty())
	{
		// Validate materials
		if (mMaterials.size() > 256)
		{
			outResult.SetError("Supporting max 256 materials per height field");
			return;
		}
		for (uint8 s : inSettings.mMaterialIndices)
			if (s >= mMaterials.size())
			{
				outResult.SetError(StringFormat("Material %u is beyond material list (size: %u)", s, (uint)mMaterials.size()));
				return;
			}
	}
	else
	{
		// No materials assigned, validate that no materials have been specified
		if (!inSettings.mMaterialIndices.empty())
		{
			outResult.SetError("No materials present, mMaterialIndices should be empty");
			return;
		}
	}

	// Determine range
	float min_value, max_value, scale;
	inSettings.DetermineMinAndMaxSample(min_value, max_value, scale);
	if (min_value > max_value)
	{
		// If there is no collision with this heightmap, leave everything empty
		mMaterials.clear();
		outResult.Set(this);
		return;
	}

	// Allocate space for this shape
	AllocateBuffers();

	// Quantize to uint16
	Array<uint16> quantized_samples;
	quantized_samples.reserve(mSampleCount * mSampleCount);
	for (uint y = 0; y < inSettings.mSampleCount; ++y)
	{
		for (uint x = 0; x < inSettings.mSampleCount; ++x)
		{
			float h = inSettings.mHeightSamples[x + y * inSettings.mSampleCount];
			if (h == cNoCollisionValue)
			{
				quantized_samples.push_back(cNoCollisionValue16);
			}
			else
			{
				// Floor the quantized height to get a lower bound for the quantized value
				int quantized_height = (int)floor(scale * (h - min_value));

				// Ensure that the height says below the max height value so we can safely add 1 to get the upper bound for the quantized value
				quantized_height = Clamp(quantized_height, 0, int(cMaxHeightValue16 - 1));

				quantized_samples.push_back(uint16(quantized_height));
			}
		}
		// Pad remaining columns with no collision
		for (uint x = inSettings.mSampleCount; x < mSampleCount; ++x)
			quantized_samples.push_back(cNoCollisionValue16);
	}
	// Pad remaining rows with no collision
	for (uint y = inSettings.mSampleCount; y < mSampleCount; ++y)
		for (uint x = 0; x < mSampleCount; ++x)
			quantized_samples.push_back(cNoCollisionValue16);

	// Update offset and scale to account for the compression to uint16
	if (min_value <= max_value) // Only when there was collision
	{
		// In GetPosition we always add 0.5 to the quantized sample in order to reduce the average error.
		// We want to be able to exactly quantize min_value (this is important in case the heightfield is entirely flat) so we subtract that value from min_value.
		min_value -= 0.5f / (scale * mSampleMask);

		mOffset.SetY(mOffset.GetY() + mScale.GetY() * min_value);
	}
	mScale.SetY(mScale.GetY() / scale);

	// Calculate amount of grids
	uint max_level = sGetMaxLevel(num_blocks);

	// Temporary data structure used during creating of a hierarchy of grids
	struct Range
	{
		uint16	mMin;
		uint16	mMax;
	};

	// Reserve size for temporary range data + reserve 1 extra for a 1x1 grid that we won't store but use for calculating the bounding box
	Array<Array<Range>> ranges;
	ranges.resize(max_level + 1);

	// Calculate highest detail grid by combining mBlockSize x mBlockSize height samples
	Array<Range> *cur_range_vector = &ranges.back();
	uint num_blocks_pow2 = GetNextPowerOf2(num_blocks); // We calculate the range blocks as if the heightfield was a power of 2, when we save the range blocks we'll ignore the extra samples (this makes downsampling easier)
	cur_range_vector->resize(num_blocks_pow2 * num_blocks_pow2);
	Range *range_dst = &cur_range_vector->front();
	for (uint y = 0; y < num_blocks_pow2; ++y)
		for (uint x = 0; x < num_blocks_pow2; ++x)
		{
			range_dst->mMin = 0xffff;
			range_dst->mMax = 0;
			uint max_bx = x == num_blocks_pow2 - 1? mBlockSize : mBlockSize + 1; // for interior blocks take 1 more because the triangles connect to the next block so we must include their height too
			uint max_by = y == num_blocks_pow2 - 1? mBlockSize : mBlockSize + 1;
			for (uint by = 0; by < max_by; ++by)
				for (uint bx = 0; bx < max_bx; ++bx)
				{
					uint sx = x * mBlockSize + bx;
					uint sy = y * mBlockSize + by;
					if (sx < mSampleCount && sy < mSampleCount)
					{
						uint16 h = quantized_samples[sy * mSampleCount + sx];
						if (h != cNoCollisionValue16)
						{
							range_dst->mMin = min(range_dst->mMin, h);
							range_dst->mMax = max(range_dst->mMax, uint16(h + 1)); // Add 1 to the max so we know the real value is between mMin and mMax
						}
					}
				}
			++range_dst;
		}

	// Calculate remaining grids
	for (uint n = num_blocks_pow2 >> 1; n >= 1; n >>= 1)
	{
		// Get source buffer
		const Range *range_src = &cur_range_vector->front();

		// Previous array element
		--cur_range_vector;

		// Make space for this grid
		cur_range_vector->resize(n * n);

		// Get target buffer
		range_dst = &cur_range_vector->front();

		// Combine the results of 2x2 ranges
		for (uint y = 0; y < n; ++y)
			for (uint x = 0; x < n; ++x)
			{
				range_dst->mMin = 0xffff;
				range_dst->mMax = 0;
				for (uint by = 0; by < 2; ++by)
					for (uint bx = 0; bx < 2; ++bx)
					{
						const Range &r = range_src[(y * 2 + by) * n * 2 + x * 2 + bx];
						range_dst->mMin = min(range_dst->mMin, r.mMin);
						range_dst->mMax = max(range_dst->mMax, r.mMax);
					}
				++range_dst;
			}
	}
	JPH_ASSERT(cur_range_vector == &ranges.front());

	// Store global range for bounding box calculation
	mMinSample = ranges[0][0].mMin;
	mMaxSample = ranges[0][0].mMax;

#ifdef JPH_ENABLE_ASSERTS
	// Validate that we did not lose range along the way
	uint16 minv = 0xffff, maxv = 0;
	for (uint16 v : quantized_samples)
		if (v != cNoCollisionValue16)
		{
			minv = min(minv, v);
			maxv = max(maxv, uint16(v + 1));
		}
	JPH_ASSERT(mMinSample == minv && mMaxSample == maxv);
#endif

	// Now erase the first element, we need a 2x2 grid to start with
	ranges.erase(ranges.begin());

	// Create blocks
	uint max_stride = (num_blocks + 1) >> 1;
	RangeBlock *current_block = mRangeBlocks;
	for (uint level = 0; level < ranges.size(); ++level)
	{
		JPH_ASSERT(uint(current_block - mRangeBlocks) == sGridOffsets[level]);

		uint in_n = 1 << level;
		uint out_n = min(in_n, max_stride); // At the most detailed level we store a non-power of 2 number of blocks

		for (uint y = 0; y < out_n; ++y)
			for (uint x = 0; x < out_n; ++x)
			{
				// Convert from 2x2 Range structure to 1 RangeBlock structure
				RangeBlock &rb = *current_block++;
				for (uint by = 0; by < 2; ++by)
					for (uint bx = 0; bx < 2; ++bx)
					{
						uint src_pos = (y * 2 + by) * 2 * in_n + (x * 2 + bx);
						uint dst_pos = by * 2 + bx;
						rb.mMin[dst_pos] = ranges[level][src_pos].mMin;
						rb.mMax[dst_pos] = ranges[level][src_pos].mMax;
					}
			}
	}
	JPH_ASSERT(uint32(current_block - mRangeBlocks) == mRangeBlocksSize);

	// Quantize height samples
	memset(mHeightSamples, 0, mHeightSamplesSize);
	int sample = 0;
	for (uint y = 0; y < mSampleCount; ++y)
		for (uint x = 0; x < mSampleCount; ++x)
		{
			uint32 output_value;

			float h = x < inSettings.mSampleCount && y < inSettings.mSampleCount? inSettings.mHeightSamples[x + y * inSettings.mSampleCount] : cNoCollisionValue;
			if (h == cNoCollisionValue)
			{
				// No collision
				output_value = mSampleMask;
			}
			else
			{
				// Get range of block so we know what range to compress to
				uint bx = x / mBlockSize;
				uint by = y / mBlockSize;
				const Range &range = ranges.back()[by * num_blocks_pow2 + bx];
				JPH_ASSERT(range.mMin < range.mMax);

				// Quantize to mBitsPerSample bits, note that mSampleMask is reserved for indicating that there's no collision.
				// We divide the range into mSampleMask segments and use the mid points of these segments as the quantized values.
				// This results in a lower error than if we had quantized our data using the lowest point of all these segments.
				float h_min = min_value + range.mMin / scale;
				float h_delta = float(range.mMax - range.mMin) / scale;
				float quantized_height = floor((h - h_min) * float(mSampleMask) / h_delta);
				output_value = uint32(Clamp((int)quantized_height, 0, int(mSampleMask) - 1)); // mSampleMask is reserved as 'no collision value'
			}

			// Store the sample
			uint byte_pos = sample >> 3;
			uint bit_pos = sample & 0b111;
			output_value <<= bit_pos;
			JPH_ASSERT(byte_pos + 1 < mHeightSamplesSize);
			mHeightSamples[byte_pos] |= uint8(output_value);
			mHeightSamples[byte_pos + 1] |= uint8(output_value >> 8);
			sample += inSettings.mBitsPerSample;
		}

	// Calculate the active edges
	CalculateActiveEdges(inSettings);

	// Compress material indices
	if (mMaterials.size() > 1 || inSettings.mMaterialsCapacity > 1)
		StoreMaterialIndices(inSettings);

	outResult.Set(this);
}

HeightFieldShape::~HeightFieldShape()
{
	if (mRangeBlocks != nullptr)
		AlignedFree(mRangeBlocks);
}

Ref<HeightFieldShape> HeightFieldShape::Clone() const
{
	Ref<HeightFieldShape> clone = new HeightFieldShape;
	clone->SetUserData(GetUserData());

	clone->mOffset = mOffset;
	clone->mScale = mScale;
	clone->mSampleCount = mSampleCount;
	clone->mBlockSize = mBlockSize;
	clone->mBitsPerSample = mBitsPerSample;
	clone->mSampleMask = mSampleMask;
	clone->mMinSample = mMinSample;
	clone->mMaxSample = mMaxSample;

	clone->AllocateBuffers();
	memcpy(clone->mRangeBlocks, mRangeBlocks, mRangeBlocksSize * sizeof(RangeBlock) + mHeightSamplesSize + mActiveEdgesSize); // Copy the entire buffer in 1 go

	clone->mMaterials.reserve(mMaterials.capacity()); // Ensure we keep the capacity of the original
	clone->mMaterials = mMaterials;
	clone->mMaterialIndices = mMaterialIndices;
	clone->mNumBitsPerMaterialIndex = mNumBitsPerMaterialIndex;

#ifdef JPH_DEBUG_RENDERER
	clone->mGeometry = mGeometry;
	clone->mCachedUseMaterialColors = mCachedUseMaterialColors;
#endif // JPH_DEBUG_RENDERER

	return clone;
}

inline void HeightFieldShape::sGetRangeBlockOffsetAndStride(uint inNumBlocks, uint inMaxLevel, uint &outRangeBlockOffset, uint &outRangeBlockStride)
{
	outRangeBlockOffset = sGridOffsets[inMaxLevel - 1];
	outRangeBlockStride = (inNumBlocks + 1) >> 1;
}

inline void HeightFieldShape::GetRangeBlock(uint inBlockX, uint inBlockY, uint inRangeBlockOffset, uint inRangeBlockStride, RangeBlock *&outBlock, uint &outIndexInBlock)
{
	JPH_ASSERT(inBlockX < GetNumBlocks() && inBlockY < GetNumBlocks());

	// Convert to location of range block
	uint rbx = inBlockX >> 1;
	uint rby = inBlockY >> 1;
	outIndexInBlock = ((inBlockY & 1) << 1) + (inBlockX & 1);

	uint offset = inRangeBlockOffset + rby * inRangeBlockStride + rbx;
	JPH_ASSERT(offset < mRangeBlocksSize);
	outBlock = mRangeBlocks + offset;
}

inline void HeightFieldShape::GetBlockOffsetAndScale(uint inBlockX, uint inBlockY, uint inRangeBlockOffset, uint inRangeBlockStride, float &outBlockOffset, float &outBlockScale) const
{
	JPH_ASSERT(inBlockX < GetNumBlocks() && inBlockY < GetNumBlocks());

	// Convert to location of range block
	uint rbx = inBlockX >> 1;
	uint rby = inBlockY >> 1;
	uint n = ((inBlockY & 1) << 1) + (inBlockX & 1);

	// Calculate offset and scale
	uint offset = inRangeBlockOffset + rby * inRangeBlockStride + rbx;
	JPH_ASSERT(offset < mRangeBlocksSize);
	const RangeBlock &block = mRangeBlocks[offset];
	outBlockOffset = float(block.mMin[n]);
	outBlockScale = float(block.mMax[n] - block.mMin[n]) / float(mSampleMask);
}

inline uint8 HeightFieldShape::GetHeightSample(uint inX, uint inY) const
{
	JPH_ASSERT(inX < mSampleCount);
	JPH_ASSERT(inY < mSampleCount);

	// Determine bit position of sample
	uint sample = (inY * mSampleCount + inX) * uint(mBitsPerSample);
	uint byte_pos = sample >> 3;
	uint bit_pos = sample & 0b111;

	// Fetch the height sample value
	JPH_ASSERT(byte_pos + 1 < mHeightSamplesSize);
	const uint8 *height_samples = mHeightSamples + byte_pos;
	uint16 height_sample = uint16(height_samples[0]) | uint16(uint16(height_samples[1]) << 8);
	return uint8(height_sample >> bit_pos) & mSampleMask;
}

inline Vec3 HeightFieldShape::GetPosition(uint inX, uint inY, float inBlockOffset, float inBlockScale, bool &outNoCollision) const
{
	// Get quantized value
	uint8 height_sample = GetHeightSample(inX, inY);
	outNoCollision = height_sample == mSampleMask;

	// Add 0.5 to the quantized value to minimize the error (see constructor)
	return mOffset + mScale * Vec3(float(inX), inBlockOffset + (0.5f + height_sample) * inBlockScale, float(inY));
}

Vec3 HeightFieldShape::GetPosition(uint inX, uint inY) const
{
	// Test if there are any samples
	if (mHeightSamplesSize == 0)
		return mOffset + mScale * Vec3(float(inX), 0.0f, float(inY));

	// Get block location
	uint bx = inX / mBlockSize;
	uint by = inY / mBlockSize;

	// Calculate offset and stride
	uint num_blocks = GetNumBlocks();
	uint range_block_offset, range_block_stride;
	sGetRangeBlockOffsetAndStride(num_blocks, sGetMaxLevel(num_blocks), range_block_offset, range_block_stride);

	float offset, scale;
	GetBlockOffsetAndScale(bx, by, range_block_offset, range_block_stride, offset, scale);

	bool no_collision;
	return GetPosition(inX, inY, offset, scale, no_collision);
}

bool HeightFieldShape::IsNoCollision(uint inX, uint inY) const
{
	return mHeightSamplesSize == 0 || GetHeightSample(inX, inY) == mSampleMask;
}

bool HeightFieldShape::ProjectOntoSurface(Vec3Arg inLocalPosition, Vec3 &outSurfacePosition, SubShapeID &outSubShapeID) const
{
	// Check if we have collision
	if (mHeightSamplesSize == 0)
		return false;

	// Convert coordinate to integer space
	Vec3 integer_space = (inLocalPosition - mOffset) / mScale;

	// Get x coordinate and fraction
	float x_frac = integer_space.GetX();
	if (x_frac < 0.0f || x_frac >= mSampleCount - 1)
		return false;
	uint x = (uint)floor(x_frac);
	x_frac -= x;

	// Get y coordinate and fraction
	float y_frac = integer_space.GetZ();
	if (y_frac < 0.0f || y_frac >= mSampleCount - 1)
		return false;
	uint y = (uint)floor(y_frac);
	y_frac -= y;

	// If one of the diagonal points doesn't have collision, we don't have a height at this location
	if (IsNoCollision(x, y) || IsNoCollision(x + 1, y + 1))
		return false;

	if (y_frac >= x_frac)
	{
		// Left bottom triangle, test the 3rd point
		if (IsNoCollision(x, y + 1))
			return false;

		// Interpolate height value
		Vec3 v1 = GetPosition(x, y);
		Vec3 v2 = GetPosition(x, y + 1);
		Vec3 v3 = GetPosition(x + 1, y + 1);
		outSurfacePosition = v1 + y_frac * (v2 - v1) + x_frac * (v3 - v2);
		SubShapeIDCreator creator;
		outSubShapeID = EncodeSubShapeID(creator, x, y, 0);
		return true;
	}
	else
	{
		// Right top triangle, test the third point
		if (IsNoCollision(x + 1, y))
			return false;

		// Interpolate height value
		Vec3 v1 = GetPosition(x, y);
		Vec3 v2 = GetPosition(x + 1, y + 1);
		Vec3 v3 = GetPosition(x + 1, y);
		outSurfacePosition = v1 + y_frac * (v2 - v3) + x_frac * (v3 - v1);
		SubShapeIDCreator creator;
		outSubShapeID = EncodeSubShapeID(creator, x, y, 1);
		return true;
	}
}

void HeightFieldShape::GetHeights(uint inX, uint inY, uint inSizeX, uint inSizeY, float *outHeights, intptr_t inHeightsStride) const
{
	if (inSizeX == 0 || inSizeY == 0)
		return;

	JPH_ASSERT(inX % mBlockSize == 0 && inY % mBlockSize == 0);
	JPH_ASSERT(inX < mSampleCount && inY < mSampleCount);
	JPH_ASSERT(inX + inSizeX <= mSampleCount && inY + inSizeY <= mSampleCount);

	// Test if there are any samples
	if (mHeightSamplesSize == 0)
	{
		// No samples, return the offset
		float offset = mOffset.GetY();
		for (uint y = 0; y < inSizeY; ++y, outHeights += inHeightsStride)
			for (uint x = 0; x < inSizeX; ++x)
				outHeights[x] = offset;
	}
	else
	{
		// Calculate offset and stride
		uint num_blocks = GetNumBlocks();
		uint range_block_offset, range_block_stride;
		sGetRangeBlockOffsetAndStride(num_blocks, sGetMaxLevel(num_blocks), range_block_offset, range_block_stride);

		// Loop over blocks
		uint block_start_x = inX / mBlockSize;
		uint block_start_y = inY / mBlockSize;
		uint num_blocks_x = inSizeX / mBlockSize;
		uint num_blocks_y = inSizeY / mBlockSize;
		for (uint block_y = 0; block_y < num_blocks_y; ++block_y)
			for (uint block_x = 0; block_x < num_blocks_x; ++block_x)
			{
				// Get offset and scale for block
				float offset, scale;
				GetBlockOffsetAndScale(block_start_x + block_x, block_start_y + block_y, range_block_offset, range_block_stride, offset, scale);

				// Adjust by global offset and scale
				// Note: This is the math applied in GetPosition() written out to reduce calculations in the inner loop
				scale *= mScale.GetY();
				offset = mOffset.GetY() + mScale.GetY() * offset + 0.5f * scale;

				// Loop over samples in block
				for (uint sample_y = 0; sample_y < mBlockSize; ++sample_y)
					for (uint sample_x = 0; sample_x < mBlockSize; ++sample_x)
					{
						// Calculate output coordinate
						uint output_x = block_x * mBlockSize + sample_x;
						uint output_y = block_y * mBlockSize + sample_y;

						// Get quantized value
						uint8 height_sample = GetHeightSample(inX + output_x, inY + output_y);

						// Dequantize
						float h = height_sample != mSampleMask? offset + height_sample * scale : cNoCollisionValue;
						outHeights[output_y * inHeightsStride + output_x] = h;
					}
			}
	}
}

void HeightFieldShape::SetHeights(uint inX, uint inY, uint inSizeX, uint inSizeY, const float *inHeights, intptr_t inHeightsStride, TempAllocator &inAllocator, float inActiveEdgeCosThresholdAngle)
{
	if (inSizeX == 0 || inSizeY == 0)
		return;

	JPH_ASSERT(mHeightSamplesSize > 0);
	JPH_ASSERT(inX % mBlockSize == 0 && inY % mBlockSize == 0);
	JPH_ASSERT(inX < mSampleCount && inY < mSampleCount);
	JPH_ASSERT(inX + inSizeX <= mSampleCount && inY + inSizeY <= mSampleCount);

	// If we have a block in negative x/y direction, we will affect its range so we need to take it into account
	bool need_temp_heights = false;
	uint affected_x = inX;
	uint affected_y = inY;
	uint affected_size_x = inSizeX;
	uint affected_size_y = inSizeY;
	if (inX > 0) { affected_x -= mBlockSize; affected_size_x += mBlockSize; need_temp_heights = true; }
	if (inY > 0) { affected_y -= mBlockSize; affected_size_y += mBlockSize; need_temp_heights = true; }

	// If we have a block in positive x/y direction, our ranges are affected by it so we need to take it into account
	uint heights_size_x = affected_size_x;
	uint heights_size_y = affected_size_y;
	if (inX + inSizeX < mSampleCount) { heights_size_x += mBlockSize; need_temp_heights = true; }
	if (inY + inSizeY < mSampleCount) { heights_size_y += mBlockSize; need_temp_heights = true; }

	// Get heights for affected area
	const float *heights;
	intptr_t heights_stride;
	float *temp_heights;
	if (need_temp_heights)
	{
		// Fetch the surrounding height data (note we're forced to recompress this data with a potentially different range so there will be some precision loss here)
		temp_heights = (float *)inAllocator.Allocate(heights_size_x * heights_size_y * sizeof(float));
		heights = temp_heights;
		heights_stride = heights_size_x;

		// We need to fill in the following areas:
		//
		// +-----------------+
		// |        2        |
		// |---+---------+---|
		// |   |         |   |
		// | 3 |    1    | 4 |
		// |   |         |   |
		// |---+---------+---|
		// |        5        |
		// +-----------------+
		//
		// 1. The area that is affected by the new heights (we just copy these)
		// 2-5. These areas are either needed to calculate the range of the affected blocks or they need to be recompressed with a different range
		uint offset_x = inX - affected_x;
		uint offset_y = inY - affected_y;

		// Area 2
		GetHeights(affected_x, affected_y, heights_size_x, offset_y, temp_heights, heights_size_x);
		float *area3_start = temp_heights + offset_y * heights_size_x;

		// Area 3
		GetHeights(affected_x, inY, offset_x, inSizeY, area3_start, heights_size_x);

		// Area 1
		float *area1_start = area3_start + offset_x;
		for (uint y = 0; y < inSizeY; ++y, area1_start += heights_size_x, inHeights += inHeightsStride)
			memcpy(area1_start, inHeights, inSizeX * sizeof(float));

		// Area 4
		uint area4_x = inX + inSizeX;
		GetHeights(area4_x, inY, affected_x + heights_size_x - area4_x, inSizeY, area3_start + area4_x - affected_x, heights_size_x);

		// Area 5
		uint area5_y = inY + inSizeY;
		float *area5_start = temp_heights + (area5_y - affected_y) * heights_size_x;
		GetHeights(affected_x, area5_y, heights_size_x, affected_y + heights_size_y - area5_y, area5_start, heights_size_x);
	}
	else
	{
		// We can directly use the input buffer because there are no extra edges to take into account
		heights = inHeights;
		heights_stride = inHeightsStride;
		temp_heights = nullptr;
	}

	// Calculate offset and stride
	uint num_blocks = GetNumBlocks();
	uint range_block_offset, range_block_stride;
	uint max_level = sGetMaxLevel(num_blocks);
	sGetRangeBlockOffsetAndStride(num_blocks, max_level, range_block_offset, range_block_stride);

	// Loop over blocks
	uint block_start_x = affected_x / mBlockSize;
	uint block_start_y = affected_y / mBlockSize;
	uint num_blocks_x = affected_size_x / mBlockSize;
	uint num_blocks_y = affected_size_y / mBlockSize;
	for (uint block_y = 0, sample_start_y = 0; block_y < num_blocks_y; ++block_y, sample_start_y += mBlockSize)
		for (uint block_x = 0, sample_start_x = 0; block_x < num_blocks_x; ++block_x, sample_start_x += mBlockSize)
		{
			// Determine quantized min and max value for block
			// Note that we need to include 1 extra row in the positive x/y direction to account for connecting triangles
			int min_value = 0xffff;
			int max_value = 0;
			uint sample_x_end = min(sample_start_x + mBlockSize + 1, mSampleCount - affected_x);
			uint sample_y_end = min(sample_start_y + mBlockSize + 1, mSampleCount - affected_y);
			for (uint sample_y = sample_start_y; sample_y < sample_y_end; ++sample_y)
				for (uint sample_x = sample_start_x; sample_x < sample_x_end; ++sample_x)
				{
					float h = heights[sample_y * heights_stride + sample_x];
					if (h != cNoCollisionValue)
					{
						int quantized_height = Clamp((int)floor((h - mOffset.GetY()) / mScale.GetY()), 0, int(cMaxHeightValue16 - 1));
						min_value = min(min_value, quantized_height);
						max_value = max(max_value, quantized_height + 1);
					}
				}
			if (min_value > max_value)
				min_value = max_value = cNoCollisionValue16;

			// Update range for block
			RangeBlock *range_block;
			uint index_in_block;
			GetRangeBlock(block_start_x + block_x, block_start_y + block_y, range_block_offset, range_block_stride, range_block, index_in_block);
			range_block->mMin[index_in_block] = uint16(min_value);
			range_block->mMax[index_in_block] = uint16(max_value);

			// Get offset and scale for block
			float offset_block = float(min_value);
			float scale_block = float(max_value - min_value) / float(mSampleMask);

			// Calculate scale and offset using the formula used in GetPosition() solved for the quantized height (excluding 0.5 because we round down while quantizing)
			float scale = scale_block * mScale.GetY();
			float offset = mOffset.GetY() + offset_block * mScale.GetY();

			// Loop over samples in block
			sample_x_end = sample_start_x + mBlockSize;
			sample_y_end = sample_start_y + mBlockSize;
			for (uint sample_y = sample_start_y; sample_y < sample_y_end; ++sample_y)
				for (uint sample_x = sample_start_x; sample_x < sample_x_end; ++sample_x)
				{
					// Quantize height
					float h = heights[sample_y * heights_stride + sample_x];
					uint8 quantized_height = h != cNoCollisionValue? uint8(Clamp((int)floor((h - offset) / scale), 0, int(mSampleMask) - 1)) : mSampleMask;

					// Determine bit position of sample
					uint sample = ((affected_y + sample_y) * mSampleCount + affected_x + sample_x) * uint(mBitsPerSample);
					uint byte_pos = sample >> 3;
					uint bit_pos = sample & 0b111;

					// Update the height value sample
					JPH_ASSERT(byte_pos + 1 < mHeightSamplesSize);
					uint8 *height_samples = mHeightSamples + byte_pos;
					uint16 height_sample = uint16(height_samples[0]) | uint16(uint16(height_samples[1]) << 8);
					height_sample &= ~(uint16(mSampleMask) << bit_pos);
					height_sample |= uint16(quantized_height) << bit_pos;
					height_samples[0] = uint8(height_sample);
					height_samples[1] = uint8(height_sample >> 8);
				}
		}

	// Update active edges
	// Note that we must take an extra row on all sides to account for connecting triangles
	uint ae_x = inX > 1? inX - 2 : 0;
	uint ae_y = inY > 1? inY - 2 : 0;
	uint ae_sx = min(inX + inSizeX + 1, mSampleCount - 1) - ae_x;
	uint ae_sy = min(inY + inSizeY + 1, mSampleCount - 1) - ae_y;
	CalculateActiveEdges(ae_x, ae_y, ae_sx, ae_sy, heights, affected_x, affected_y, heights_stride, 1.0f, inActiveEdgeCosThresholdAngle, inAllocator);

	// Free temporary buffer
	if (temp_heights != nullptr)
		inAllocator.Free(temp_heights, heights_size_x * heights_size_y * sizeof(float));

	// Update hierarchy of range blocks
	while (max_level > 1)
	{
		// Get offset and stride for destination blocks
		uint dst_range_block_offset, dst_range_block_stride;
		sGetRangeBlockOffsetAndStride(num_blocks >> 1, max_level - 1, dst_range_block_offset, dst_range_block_stride);

		// We'll be processing 2x2 blocks below so we need the start coordinates to be even and we extend the number of blocks to correct for that
		if (block_start_x & 1) { --block_start_x; ++num_blocks_x; }
		if (block_start_y & 1) { --block_start_y; ++num_blocks_y; }

		// Loop over all affected blocks
		uint block_end_x = block_start_x + num_blocks_x;
		uint block_end_y = block_start_y + num_blocks_y;
		for (uint block_y = block_start_y; block_y < block_end_y; block_y += 2)
			for (uint block_x = block_start_x; block_x < block_end_x; block_x += 2)
			{
				// Get source range block
				RangeBlock *src_range_block;
				uint index_in_src_block;
				GetRangeBlock(block_x, block_y, range_block_offset, range_block_stride, src_range_block, index_in_src_block);

				// Determine quantized min and max value for the entire 2x2 block
				uint16 min_value = 0xffff;
				uint16 max_value = 0;
				for (uint i = 0; i < 4; ++i)
					if (src_range_block->mMin[i] != cNoCollisionValue16)
					{
						min_value = min(min_value, src_range_block->mMin[i]);
						max_value = max(max_value, src_range_block->mMax[i]);
					}

				// Write to destination block
				RangeBlock *dst_range_block;
				uint index_in_dst_block;
				GetRangeBlock(block_x >> 1, block_y >> 1, dst_range_block_offset, dst_range_block_stride, dst_range_block, index_in_dst_block);
				dst_range_block->mMin[index_in_dst_block] = uint16(min_value);
				dst_range_block->mMax[index_in_dst_block] = uint16(max_value);
			}

		// Go up one level
		--max_level;
		num_blocks >>= 1;
		block_start_x >>= 1;
		block_start_y >>= 1;
		num_blocks_x = min((num_blocks_x + 1) >> 1, num_blocks);
		num_blocks_y = min((num_blocks_y + 1) >> 1, num_blocks);

		// Update stride and offset for source to old destination
		range_block_offset = dst_range_block_offset;
		range_block_stride = dst_range_block_stride;
	}

	// Calculate new min and max sample for the entire height field
	mMinSample = 0xffff;
	mMaxSample = 0;
	for (uint i = 0; i < 4; ++i)
		if (mRangeBlocks[0].mMin[i] != cNoCollisionValue16)
		{
			mMinSample = min(mMinSample, mRangeBlocks[0].mMin[i]);
			mMaxSample = max(mMaxSample, mRangeBlocks[0].mMax[i]);
		}

#ifdef JPH_DEBUG_RENDERER
	// Invalidate temporary rendering data
	mGeometry.clear();
#endif
}

void HeightFieldShape::GetMaterials(uint inX, uint inY, uint inSizeX, uint inSizeY, uint8 *outMaterials, intptr_t inMaterialsStride) const
{
	if (inSizeX == 0 || inSizeY == 0)
		return;

	if (mMaterialIndices.empty())
	{
		// Return all 0's
		for (uint y = 0; y < inSizeY; ++y)
		{
			uint8 *out_indices = outMaterials + y * inMaterialsStride;
			for (uint x = 0; x < inSizeX; ++x)
				*out_indices++ = 0;
		}
		return;
	}

	JPH_ASSERT(inX < mSampleCount && inY < mSampleCount);
	JPH_ASSERT(inX + inSizeX < mSampleCount && inY + inSizeY < mSampleCount);

	uint count_min_1 = mSampleCount - 1;
	uint16 material_index_mask = uint16((1 << mNumBitsPerMaterialIndex) - 1);

	for (uint y = 0; y < inSizeY; ++y)
	{
		// Calculate input position
		uint bit_pos = (inX + (inY + y) * count_min_1) * mNumBitsPerMaterialIndex;
		const uint8 *in_indices = mMaterialIndices.data() + (bit_pos >> 3);
		bit_pos &= 0b111;

		// Calculate output position
		uint8 *out_indices = outMaterials + y * inMaterialsStride;

		for (uint x = 0; x < inSizeX; ++x)
		{
			// Get material index
			uint16 material_index = uint16(in_indices[0]) + uint16(uint16(in_indices[1]) << 8);
			material_index >>= bit_pos;
			material_index &= material_index_mask;
			*out_indices = uint8(material_index);

			// Go to the next index
			bit_pos += mNumBitsPerMaterialIndex;
			in_indices += bit_pos >> 3;
			bit_pos &= 0b111;
			++out_indices;
		}
	}
}

bool HeightFieldShape::SetMaterials(uint inX, uint inY, uint inSizeX, uint inSizeY, const uint8 *inMaterials, intptr_t inMaterialsStride, const PhysicsMaterialList *inMaterialList, TempAllocator &inAllocator)
{
	if (inSizeX == 0 || inSizeY == 0)
		return true;

	JPH_ASSERT(inX < mSampleCount && inY < mSampleCount);
	JPH_ASSERT(inX + inSizeX < mSampleCount && inY + inSizeY < mSampleCount);

	// Remap materials
	uint material_remap_table_size = uint(inMaterialList != nullptr? inMaterialList->size() : mMaterials.size());
	uint8 *material_remap_table = (uint8 *)inAllocator.Allocate(material_remap_table_size);
	JPH_SCOPE_EXIT([&inAllocator, material_remap_table, material_remap_table_size]{ inAllocator.Free(material_remap_table, material_remap_table_size); });
	if (inMaterialList != nullptr)
	{
		// Conservatively reserve more space if the incoming material list is bigger
		if (inMaterialList->size() > mMaterials.size())
			mMaterials.reserve(inMaterialList->size());

		// Create a remap table
		uint8 *remap_entry = material_remap_table;
		for (const PhysicsMaterial *material : *inMaterialList)
		{
			// Try to find it in the existing list
			PhysicsMaterialList::const_iterator it = std::find(mMaterials.begin(), mMaterials.end(), material);
			if (it != mMaterials.end())
			{
				// Found it, calculate index
				*remap_entry = uint8(it - mMaterials.begin());
			}
			else
			{
				// Not found, add it
				if (mMaterials.size() >= 256)
				{
					// We can't have more than 256 materials since we use uint8 as indices
					return false;
				}
				*remap_entry = uint8(mMaterials.size());
				mMaterials.push_back(material);
			}
			++remap_entry;
		}
	}
	else
	{
		// No remapping
		for (uint i = 0; i < material_remap_table_size; ++i)
			material_remap_table[i] = uint8(i);
	}

	if (mMaterials.size() == 1)
	{
		// Only 1 material, we don't need to store the material indices
		return true;
	}

	// Check if we need to resize the material indices array
	uint count_min_1 = mSampleCount - 1;
	uint32 new_bits_per_material_index = 32 - CountLeadingZeros((uint32)mMaterials.size() - 1);
	JPH_ASSERT(mNumBitsPerMaterialIndex <= 8 && new_bits_per_material_index <= 8);
	if (new_bits_per_material_index > mNumBitsPerMaterialIndex)
	{
		// Resize the material indices array
		mMaterialIndices.resize(((Square(count_min_1) * new_bits_per_material_index + 7) >> 3) + 1, 0); // Add 1 byte so we don't read out of bounds when reading an uint16

		// Calculate old and new mask
		uint16 old_material_index_mask = uint16((1 << mNumBitsPerMaterialIndex) - 1);
		uint16 new_material_index_mask = uint16((1 << new_bits_per_material_index) - 1);

		// Loop through the array backwards to avoid overwriting data
		int in_bit_pos = (count_min_1 * count_min_1 - 1) * mNumBitsPerMaterialIndex;
		const uint8 *in_indices = mMaterialIndices.data() + (in_bit_pos >> 3);
		in_bit_pos &= 0b111;
		int out_bit_pos = (count_min_1 * count_min_1 - 1) * new_bits_per_material_index;
		uint8 *out_indices = mMaterialIndices.data() + (out_bit_pos >> 3);
		out_bit_pos &= 0b111;

		while (out_indices >= mMaterialIndices.data())
		{
			// Read the material index
			uint16 material_index = uint16(in_indices[0]) + uint16(uint16(in_indices[1]) << 8);
			material_index >>= in_bit_pos;
			material_index &= old_material_index_mask;

			// Write the material index
			uint16 output_data = uint16(out_indices[0]) + uint16(uint16(out_indices[1]) << 8);
			output_data &= ~(new_material_index_mask << out_bit_pos);
			output_data |= material_index << out_bit_pos;
			out_indices[0] = uint8(output_data);
			out_indices[1] = uint8(output_data >> 8);

			// Go to the previous index
			in_bit_pos -= int(mNumBitsPerMaterialIndex);
			in_indices += in_bit_pos >> 3;
			in_bit_pos &= 0b111;
			out_bit_pos -= int(new_bits_per_material_index);
			out_indices += out_bit_pos >> 3;
			out_bit_pos &= 0b111;
		}

		// Accept the new bits per material index
		mNumBitsPerMaterialIndex = new_bits_per_material_index;
	}

	uint16 material_index_mask = uint16((1 << mNumBitsPerMaterialIndex) - 1);
	for (uint y = 0; y < inSizeY; ++y)
	{
		// Calculate input position
		const uint8 *in_indices = inMaterials + y * inMaterialsStride;

		// Calculate output position
		uint bit_pos = (inX + (inY + y) * count_min_1) * mNumBitsPerMaterialIndex;
		uint8 *out_indices = mMaterialIndices.data() + (bit_pos >> 3);
		bit_pos &= 0b111;

		for (uint x = 0; x < inSizeX; ++x)
		{
			// Update material
			uint16 output_data = uint16(out_indices[0]) + uint16(uint16(out_indices[1]) << 8);
			output_data &= ~(material_index_mask << bit_pos);
			output_data |= material_remap_table[*in_indices] << bit_pos;
			out_indices[0] = uint8(output_data);
			out_indices[1] = uint8(output_data >> 8);

			// Go to the next index
			in_indices++;
			bit_pos += mNumBitsPerMaterialIndex;
			out_indices += bit_pos >> 3;
			bit_pos &= 0b111;
		}
	}

	return true;
}

MassProperties HeightFieldShape::GetMassProperties() const
{
	// Object should always be static, return default mass properties
	return MassProperties();
}

const PhysicsMaterial *HeightFieldShape::GetMaterial(uint inX, uint inY) const
{
	if (mMaterials.empty())
		return PhysicsMaterial::sDefault;
	if (mMaterials.size() == 1)
		return mMaterials[0];

	uint count_min_1 = mSampleCount - 1;
	JPH_ASSERT(inX < count_min_1);
	JPH_ASSERT(inY < count_min_1);

	// Calculate at which bit the material index starts
	uint bit_pos = (inX + inY * count_min_1) * mNumBitsPerMaterialIndex;
	uint byte_pos = bit_pos >> 3;
	bit_pos &= 0b111;

	// Read the material index
	JPH_ASSERT(byte_pos + 1 < mMaterialIndices.size());
	const uint8 *material_indices = mMaterialIndices.data() + byte_pos;
	uint16 material_index = uint16(material_indices[0]) + uint16(uint16(material_indices[1]) << 8);
	material_index >>= bit_pos;
	material_index &= (1 << mNumBitsPerMaterialIndex) - 1;

	// Return the material
	return mMaterials[material_index];
}

uint HeightFieldShape::GetSubShapeIDBits() const
{
	// Need to store X, Y and 1 extra bit to specify the triangle number in the quad
	return 2 * (32 - CountLeadingZeros(mSampleCount - 1)) + 1;
}

SubShapeID HeightFieldShape::EncodeSubShapeID(const SubShapeIDCreator &inCreator, uint inX, uint inY, uint inTriangle) const
{
	return inCreator.PushID((inX + inY * mSampleCount) * 2 + inTriangle, GetSubShapeIDBits()).GetID();
}

void HeightFieldShape::DecodeSubShapeID(const SubShapeID &inSubShapeID, uint &outX, uint &outY, uint &outTriangle) const
{
	// Decode sub shape id
	SubShapeID remainder;
	uint32 id = inSubShapeID.PopID(GetSubShapeIDBits(), remainder);
	JPH_ASSERT(remainder.IsEmpty(), "Invalid subshape ID");

	// Get triangle index
	outTriangle = id & 1;
	id >>= 1;

	// Fetch the x and y coordinate
	outX = id % mSampleCount;
	outY = id / mSampleCount;
}

void HeightFieldShape::GetSubShapeCoordinates(const SubShapeID &inSubShapeID, uint &outX, uint &outY, uint &outTriangleIndex) const
{
	DecodeSubShapeID(inSubShapeID, outX, outY, outTriangleIndex);
}

const PhysicsMaterial *HeightFieldShape::GetMaterial(const SubShapeID &inSubShapeID) const
{
	// Decode ID
	uint x, y, triangle;
	DecodeSubShapeID(inSubShapeID, x, y, triangle);

	// Fetch the material
	return GetMaterial(x, y);
}

Vec3 HeightFieldShape::GetSurfaceNormal(const SubShapeID &inSubShapeID, Vec3Arg inLocalSurfacePosition) const
{
	// Decode ID
	uint x, y, triangle;
	DecodeSubShapeID(inSubShapeID, x, y, triangle);

	// Fetch vertices that both triangles share
	Vec3 x1y1 = GetPosition(x, y);
	Vec3 x2y2 = GetPosition(x + 1, y + 1);

	// Get normal depending on which triangle was selected
	Vec3 normal;
	if (triangle == 0)
	{
		Vec3 x1y2 = GetPosition(x, y + 1);
		normal = (x2y2 - x1y2).Cross(x1y1 - x1y2);
	}
	else
	{
		Vec3 x2y1 = GetPosition(x + 1, y);
		normal = (x1y1 - x2y1).Cross(x2y2 - x2y1);
	}

	return normal.Normalized();
}

void HeightFieldShape::GetSupportingFace(const SubShapeID &inSubShapeID, Vec3Arg inDirection, Vec3Arg inScale, Mat44Arg inCenterOfMassTransform, SupportingFace &outVertices) const
{
	// Decode ID
	uint x, y, triangle;
	DecodeSubShapeID(inSubShapeID, x, y, triangle);

	// Fetch the triangle
	outVertices.resize(3);
	outVertices[0] = GetPosition(x, y);
	Vec3 v2 = GetPosition(x + 1, y + 1);
	if (triangle == 0)
	{
		outVertices[1] = GetPosition(x, y + 1);
		outVertices[2] = v2;
	}
	else
	{
		outVertices[1] = v2;
		outVertices[2] = GetPosition(x + 1, y);
	}

	// Flip triangle if scaled inside out
	if (ScaleHelpers::IsInsideOut(inScale))
		std::swap(outVertices[1], outVertices[2]);

	// Transform to world space
	Mat44 transform = inCenterOfMassTransform.PreScaled(inScale);
	for (Vec3 &v : outVertices)
		v = transform * v;
}

inline uint8 HeightFieldShape::GetEdgeFlags(uint inX, uint inY, uint inTriangle) const
{
	JPH_ASSERT(inX < mSampleCount - 1 && inY < mSampleCount - 1);

	if (inTriangle == 0)
	{
		// The edge flags for this triangle are directly stored, find the right 3 bits
		uint bit_pos = 3 * (inX + inY * (mSampleCount - 1));
		uint byte_pos = bit_pos >> 3;
		bit_pos &= 0b111;
		JPH_ASSERT(byte_pos + 1 < mActiveEdgesSize);
		const uint8 *active_edges = mActiveEdges + byte_pos;
		uint16 edge_flags = uint16(active_edges[0]) + uint16(uint16(active_edges[1]) << 8);
		return uint8(edge_flags >> bit_pos) & 0b111;
	}
	else
	{
		// We don't store this triangle directly, we need to look at our three neighbours to construct the edge flags
		uint8 edge0 = (GetEdgeFlags(inX, inY, 0) & 0b100) != 0? 0b001 : 0; // Diagonal edge
		uint8 edge1 = inX == mSampleCount - 2 || (GetEdgeFlags(inX + 1, inY, 0) & 0b001) != 0? 0b010 : 0; // Vertical edge
		uint8 edge2 = inY == 0 || (GetEdgeFlags(inX, inY - 1, 0) & 0b010) != 0? 0b100 : 0; // Horizontal edge
		return edge0 | edge1 | edge2;
	}
}

AABox HeightFieldShape::GetLocalBounds() const
{
	if (mMinSample == cNoCollisionValue16)
	{
		// This whole height field shape doesn't have any collision, return the center point
		Vec3 center = mOffset + 0.5f * mScale * Vec3(float(mSampleCount - 1), 0.0f, float(mSampleCount - 1));
		return AABox(center, center);
	}
	else
	{
		// Bounding box based on min and max sample height
		Vec3 bmin = mOffset + mScale * Vec3(0.0f, float(mMinSample), 0.0f);
		Vec3 bmax = mOffset + mScale * Vec3(float(mSampleCount - 1), float(mMaxSample), float(mSampleCount - 1));
		return AABox(bmin, bmax);
	}
}

#ifdef JPH_DEBUG_RENDERER
void HeightFieldShape::Draw(DebugRenderer *inRenderer, RMat44Arg inCenterOfMassTransform, Vec3Arg inScale, ColorArg inColor, bool inUseMaterialColors, bool inDrawWireframe) const
{
	// Don't draw anything if we don't have any collision
	if (mHeightSamplesSize == 0)
		return;

	// Reset the batch if we switch coloring mode
	if (mCachedUseMaterialColors != inUseMaterialColors)
	{
		mGeometry.clear();
		mCachedUseMaterialColors = inUseMaterialColors;
	}

	if (mGeometry.empty())
	{
		// Divide terrain in triangle batches of max 64x64x2 triangles to allow better culling of the terrain
		uint32 block_size = min<uint32>(mSampleCount, 64);
		for (uint32 by = 0; by < mSampleCount; by += block_size)
			for (uint32 bx = 0; bx < mSampleCount; bx += block_size)
			{
				// Create vertices for a block
				Array<DebugRenderer::Triangle> triangles;
				triangles.resize(block_size * block_size * 2);
				DebugRenderer::Triangle *out_tri = &triangles[0];
				for (uint32 y = by, max_y = min(by + block_size, mSampleCount - 1); y < max_y; ++y)
					for (uint32 x = bx, max_x = min(bx + block_size, mSampleCount - 1); x < max_x; ++x)
						if (!IsNoCollision(x, y) && !IsNoCollision(x + 1, y + 1))
						{
							Vec3 x1y1 = GetPosition(x, y);
							Vec3 x2y2 = GetPosition(x + 1, y + 1);
							Color color = inUseMaterialColors? GetMaterial(x, y)->GetDebugColor() : Color::sWhite;

							if (!IsNoCollision(x, y + 1))
							{
								Vec3 x1y2 = GetPosition(x, y + 1);

								x1y1.StoreFloat3(&out_tri->mV[0].mPosition);
								x1y2.StoreFloat3(&out_tri->mV[1].mPosition);
								x2y2.StoreFloat3(&out_tri->mV[2].mPosition);

								Vec3 normal = (x2y2 - x1y2).Cross(x1y1 - x1y2).Normalized();
								for (DebugRenderer::Vertex &v : out_tri->mV)
								{
									v.mColor = color;
									v.mUV = Float2(0, 0);
									normal.StoreFloat3(&v.mNormal);
								}

								++out_tri;
							}

							if (!IsNoCollision(x + 1, y))
							{
								Vec3 x2y1 = GetPosition(x + 1, y);

								x1y1.StoreFloat3(&out_tri->mV[0].mPosition);
								x2y2.StoreFloat3(&out_tri->mV[1].mPosition);
								x2y1.StoreFloat3(&out_tri->mV[2].mPosition);

								Vec3 normal = (x1y1 - x2y1).Cross(x2y2 - x2y1).Normalized();
								for (DebugRenderer::Vertex &v : out_tri->mV)
								{
									v.mColor = color;
									v.mUV = Float2(0, 0);
									normal.StoreFloat3(&v.mNormal);
								}

								++out_tri;
							}
						}

				// Resize triangles array to actual amount of triangles written
				size_t num_triangles = out_tri - &triangles[0];
				triangles.resize(num_triangles);

				// Create batch
				if (num_triangles > 0)
					mGeometry.push_back(new DebugRenderer::Geometry(inRenderer->CreateTriangleBatch(triangles), DebugRenderer::sCalculateBounds(&triangles[0].mV[0], int(3 * num_triangles))));
			}
	}

	// Get transform including scale
	RMat44 transform = inCenterOfMassTransform.PreScaled(inScale);

	// Test if the shape is scaled inside out
	DebugRenderer::ECullMode cull_mode = ScaleHelpers::IsInsideOut(inScale)? DebugRenderer::ECullMode::CullFrontFace : DebugRenderer::ECullMode::CullBackFace;

	// Determine the draw mode
	DebugRenderer::EDrawMode draw_mode = inDrawWireframe? DebugRenderer::EDrawMode::Wireframe : DebugRenderer::EDrawMode::Solid;

	// Draw the geometry
	for (const DebugRenderer::GeometryRef &b : mGeometry)
		inRenderer->DrawGeometry(transform, inColor, b, cull_mode, DebugRenderer::ECastShadow::On, draw_mode);

	if (sDrawTriangleOutlines)
	{
		struct Visitor
		{
			JPH_INLINE explicit		Visitor(const HeightFieldShape *inShape, DebugRenderer *inRenderer, RMat44Arg inTransform) :
				mShape(inShape),
				mRenderer(inRenderer),
				mTransform(inTransform)
			{
			}

			JPH_INLINE bool			ShouldAbort() const
			{
				return false;
			}

			JPH_INLINE bool			ShouldVisitRangeBlock([[maybe_unused]] int inStackTop) const
			{
				return true;
			}

			JPH_INLINE int			VisitRangeBlock(Vec4Arg inBoundsMinX, Vec4Arg inBoundsMinY, Vec4Arg inBoundsMinZ, Vec4Arg inBoundsMaxX, Vec4Arg inBoundsMaxY, Vec4Arg inBoundsMaxZ, UVec4 &ioProperties, [[maybe_unused]] int inStackTop) const
			{
				UVec4 valid = Vec4::sLessOrEqual(inBoundsMinY, inBoundsMaxY);
				return CountAndSortTrues(valid, ioProperties);
			}

			JPH_INLINE void			VisitTriangle(uint inX, uint inY, uint inTriangle, Vec3Arg inV0, Vec3Arg inV1, Vec3Arg inV2) const
			{
				// Determine active edges
				uint8 active_edges = mShape->GetEdgeFlags(inX, inY, inTriangle);

				// Loop through edges
				Vec3 v[] = { inV0, inV1, inV2 };
				for (uint edge_idx = 0; edge_idx < 3; ++edge_idx)
				{
					RVec3 v1 = mTransform * v[edge_idx];
					RVec3 v2 = mTransform * v[(edge_idx + 1) % 3];

					// Draw active edge as a green arrow, other edges as grey
					if (active_edges & (1 << edge_idx))
						mRenderer->DrawArrow(v1, v2, Color::sGreen, 0.01f);
					else
						mRenderer->DrawLine(v1, v2, Color::sGrey);
				}
			}

			const HeightFieldShape *mShape;
			DebugRenderer *			mRenderer;
			RMat44					mTransform;
		};

		Visitor visitor(this, inRenderer, inCenterOfMassTransform.PreScaled(inScale));
		WalkHeightField(visitor);
	}
}
#endif // JPH_DEBUG_RENDERER

class HeightFieldShape::DecodingContext
{
public:
	JPH_INLINE explicit			DecodingContext(const HeightFieldShape *inShape) :
		mShape(inShape)
	{
		static_assert(sizeof(sGridOffsets) / sizeof(uint) == cNumBitsXY + 1, "Offsets array is not long enough");

		// Construct root stack entry
		mPropertiesStack[0] = 0; // level: 0, x: 0, y: 0
	}

	template <class Visitor>
	JPH_INLINE void				WalkHeightField(Visitor &ioVisitor)
	{
		// Early out if there's no collision
		if (mShape->mHeightSamplesSize == 0)
			return;

		// Assert that an inside-out bounding box does not collide
		JPH_IF_ENABLE_ASSERTS(UVec4 dummy = UVec4::sReplicate(0);)
		JPH_ASSERT(ioVisitor.VisitRangeBlock(Vec4::sReplicate(-1.0e6f), Vec4::sReplicate(1.0e6f), Vec4::sReplicate(-1.0e6f), Vec4::sReplicate(1.0e6f), Vec4::sReplicate(-1.0e6f), Vec4::sReplicate(1.0e6f), dummy, 0) == 0);

		// Precalculate values relating to sample count
		uint32 sample_count = mShape->mSampleCount;
		UVec4 sample_count_min_1 = UVec4::sReplicate(sample_count - 1);

		// Precalculate values relating to block size
		uint32 block_size = mShape->mBlockSize;
		uint32 block_size_plus_1 = block_size + 1;
		uint num_blocks = mShape->GetNumBlocks();
		uint num_blocks_min_1 = num_blocks - 1;
		uint max_level = HeightFieldShape::sGetMaxLevel(num_blocks);
		uint32 max_stride = (num_blocks + 1) >> 1;

		// Precalculate range block offset and stride for GetBlockOffsetAndScale
		uint range_block_offset, range_block_stride;
		sGetRangeBlockOffsetAndStride(num_blocks, max_level, range_block_offset, range_block_stride);

		// Allocate space for vertices and 'no collision' flags
		int array_size = Square(block_size_plus_1);
		Vec3 *vertices = reinterpret_cast<Vec3 *>(JPH_STACK_ALLOC(array_size * sizeof(Vec3)));
		bool *no_collision = reinterpret_cast<bool *>(JPH_STACK_ALLOC(array_size * sizeof(bool)));

		// Splat offsets
		Vec4 ox = mShape->mOffset.SplatX();
		Vec4 oy = mShape->mOffset.SplatY();
		Vec4 oz = mShape->mOffset.SplatZ();

		// Splat scales
		Vec4 sx = mShape->mScale.SplatX();
		Vec4 sy = mShape->mScale.SplatY();
		Vec4 sz = mShape->mScale.SplatZ();

		do
		{
			// Decode properties
			uint32 properties_top = mPropertiesStack[mTop];
			uint32 x = properties_top & cMaskBitsXY;
			uint32 y = (properties_top >> cNumBitsXY) & cMaskBitsXY;
			uint32 level = properties_top >> cLevelShift;

			if (level >= max_level)
			{
				// Determine actual range of samples (minus one because we eventually want to iterate over the triangles, not the samples)
				uint32 min_x = x * block_size;
				uint32 max_x = min_x + block_size;
				uint32 min_y = y * block_size;
				uint32 max_y = min_y + block_size;

				// Decompress vertices of block at (x, y)
				Vec3 *dst_vertex = vertices;
				bool *dst_no_collision = no_collision;
				float block_offset, block_scale;
				mShape->GetBlockOffsetAndScale(x, y, range_block_offset, range_block_stride, block_offset, block_scale);
				for (uint32 v_y = min_y; v_y < max_y; ++v_y)
				{
					for (uint32 v_x = min_x; v_x < max_x; ++v_x)
					{
						*dst_vertex = mShape->GetPosition(v_x, v_y, block_offset, block_scale, *dst_no_collision);
						++dst_vertex;
						++dst_no_collision;
					}

					// Skip last column, these values come from a different block
					++dst_vertex;
					++dst_no_collision;
				}

				// Decompress block (x + 1, y)
				uint32 max_x_decrement = 0;
				if (x < num_blocks_min_1)
				{
					dst_vertex = vertices + block_size;
					dst_no_collision = no_collision + block_size;
					mShape->GetBlockOffsetAndScale(x + 1, y, range_block_offset, range_block_stride, block_offset, block_scale);
					for (uint32 v_y = min_y; v_y < max_y; ++v_y)
					{
						*dst_vertex = mShape->GetPosition(max_x, v_y, block_offset, block_scale, *dst_no_collision);
						dst_vertex += block_size_plus_1;
						dst_no_collision += block_size_plus_1;
					}
				}
				else
					max_x_decrement = 1; // We don't have a next block, one less triangle to test

				// Decompress block (x, y + 1)
				if (y < num_blocks_min_1)
				{
					uint start = block_size * block_size_plus_1;
					dst_vertex = vertices + start;
					dst_no_collision = no_collision + start;
					mShape->GetBlockOffsetAndScale(x, y + 1, range_block_offset, range_block_stride, block_offset, block_scale);
					for (uint32 v_x = min_x; v_x < max_x; ++v_x)
					{
						*dst_vertex = mShape->GetPosition(v_x, max_y, block_offset, block_scale, *dst_no_collision);
						++dst_vertex;
						++dst_no_collision;
					}

					// Decompress single sample of block at (x + 1, y + 1)
					if (x < num_blocks_min_1)
					{
						mShape->GetBlockOffsetAndScale(x + 1, y + 1, range_block_offset, range_block_stride, block_offset, block_scale);
						*dst_vertex = mShape->GetPosition(max_x, max_y, block_offset, block_scale, *dst_no_collision);
					}
				}
				else
					--max_y; // We don't have a next block, one less triangle to test

				// Update max_x (we've been using it so we couldn't update it earlier)
				max_x -= max_x_decrement;

				// We're going to divide the vertices in 4 blocks to do one more runtime sub-division, calculate the ranges of those blocks
				struct Range
				{
					uint32 mMinX, mMinY, mNumTrianglesX, mNumTrianglesY;
				};
				uint32 half_block_size = block_size >> 1;
				uint32 block_size_x = max_x - min_x - half_block_size;
				uint32 block_size_y = max_y - min_y - half_block_size;
				Range ranges[] =
				{
					{ 0, 0,									half_block_size, half_block_size },
					{ half_block_size, 0,					block_size_x, half_block_size },
					{ 0, half_block_size,					half_block_size, block_size_y },
					{ half_block_size, half_block_size,		block_size_x, block_size_y },
				};

				// Calculate the min and max of each of the blocks
				Mat44 block_min, block_max;
				for (int block = 0; block < 4; ++block)
				{
					// Get the range for this block
					const Range &range = ranges[block];
					uint32 start = range.mMinX + range.mMinY * block_size_plus_1;
					uint32 size_x_plus_1 = range.mNumTrianglesX + 1;
					uint32 size_y_plus_1 = range.mNumTrianglesY + 1;

					// Calculate where to start reading
					const Vec3 *src_vertex = vertices + start;
					const bool *src_no_collision = no_collision + start;
					uint32 stride = block_size_plus_1 - size_x_plus_1;

					// Start range with a very large inside-out box
					Vec3 value_min = Vec3::sReplicate(1.0e30f);
					Vec3 value_max = Vec3::sReplicate(-1.0e30f);

					// Loop over the samples to determine the min and max of this block
					for (uint32 block_y = 0; block_y < size_y_plus_1; ++block_y)
					{
						for (uint32 block_x = 0; block_x < size_x_plus_1; ++block_x)
						{
							if (!*src_no_collision)
							{
								value_min = Vec3::sMin(value_min, *src_vertex);
								value_max = Vec3::sMax(value_max, *src_vertex);
							}
							++src_vertex;
							++src_no_collision;
						}
						src_vertex += stride;
						src_no_collision += stride;
					}
					block_min.SetColumn4(block, Vec4(value_min));
					block_max.SetColumn4(block, Vec4(value_max));
				}

			#ifdef JPH_DEBUG_HEIGHT_FIELD
				// Draw the bounding boxes of the sub-nodes
				for (int block = 0; block < 4; ++block)
				{
					AABox bounds(block_min.GetColumn3(block), block_max.GetColumn3(block));
					if (bounds.IsValid())
						DebugRenderer::sInstance->DrawWireBox(bounds, Color::sYellow);
				}
			#endif // JPH_DEBUG_HEIGHT_FIELD

				// Transpose so we have the mins and maxes of each of the blocks in rows instead of columns
				Mat44 transposed_min = block_min.Transposed();
				Mat44 transposed_max = block_max.Transposed();

				// Check which blocks collide
				// Note: At this point we don't use our own stack but we do allow the visitor to use its own stack
				// to store collision distances so that we can still early out when no closer hits have been found.
				UVec4 colliding_blocks(0, 1, 2, 3);
				int num_results = ioVisitor.VisitRangeBlock(transposed_min.GetColumn4(0), transposed_min.GetColumn4(1), transposed_min.GetColumn4(2), transposed_max.GetColumn4(0), transposed_max.GetColumn4(1), transposed_max.GetColumn4(2), colliding_blocks, mTop);

				// Loop through the results backwards (closest first)
				int result = num_results - 1;
				while (result >= 0)
				{
					// Calculate the min and max of this block
					uint32 block = colliding_blocks[result];
					const Range &range = ranges[block];
					uint32 block_min_x = min_x + range.mMinX;
					uint32 block_max_x = block_min_x + range.mNumTrianglesX;
					uint32 block_min_y = min_y + range.mMinY;
					uint32 block_max_y = block_min_y + range.mNumTrianglesY;

					// Loop triangles
					for (uint32 v_y = block_min_y; v_y < block_max_y; ++v_y)
						for (uint32 v_x = block_min_x; v_x < block_max_x; ++v_x)
						{
							// Get first vertex
							const int offset = (v_y - min_y) * block_size_plus_1 + (v_x - min_x);
							const Vec3 *start_vertex = vertices + offset;
							const bool *start_no_collision = no_collision + offset;

							// Check if vertices shared by both triangles have collision
							if (!start_no_collision[0] && !start_no_collision[block_size_plus_1 + 1])
							{
								// Loop 2 triangles
								for (uint t = 0; t < 2; ++t)
								{
									// Determine triangle vertices
									Vec3 v0, v1, v2;
									if (t == 0)
									{
										// Check third vertex
										if (start_no_collision[block_size_plus_1])
											continue;

										// Get vertices for triangle
										v0 = start_vertex[0];
										v1 = start_vertex[block_size_plus_1];
										v2 = start_vertex[block_size_plus_1 + 1];
									}
									else
									{
										// Check third vertex
										if (start_no_collision[1])
											continue;

										// Get vertices for triangle
										v0 = start_vertex[0];
										v1 = start_vertex[block_size_plus_1 + 1];
										v2 = start_vertex[1];
									}

								#ifdef JPH_DEBUG_HEIGHT_FIELD
									DebugRenderer::sInstance->DrawWireTriangle(RVec3(v0), RVec3(v1), RVec3(v2), Color::sWhite);
								#endif

									// Call visitor
									ioVisitor.VisitTriangle(v_x, v_y, t, v0, v1, v2);

									// Check if we're done
									if (ioVisitor.ShouldAbort())
										return;
								}
							}
						}

					// Fetch next block until we find one that the visitor wants to see
					do
						--result;
					while (result >= 0 && !ioVisitor.ShouldVisitRangeBlock(mTop + result));
				}
			}
			else
			{
				// Visit child grid
				uint32 stride = min(1U << level, max_stride); // At the most detailed level we store a non-power of 2 number of blocks
				uint32 offset = sGridOffsets[level] + stride * y + x;

				// Decode min/max height
				JPH_ASSERT(offset < mShape->mRangeBlocksSize);
				UVec4 block = UVec4::sLoadInt4Aligned(reinterpret_cast<const uint32 *>(&mShape->mRangeBlocks[offset]));
				Vec4 bounds_miny = oy + sy * block.Expand4Uint16Lo().ToFloat();
				Vec4 bounds_maxy = oy + sy * block.Expand4Uint16Hi().ToFloat();

				// Calculate size of one cell at this grid level
				UVec4 internal_cell_size = UVec4::sReplicate(block_size << (max_level - level - 1)); // subtract 1 from level because we have an internal grid of 2x2

				// Calculate min/max x and z
				UVec4 two_x = UVec4::sReplicate(2 * x); // multiply by two because we have an internal grid of 2x2
				Vec4 bounds_minx = ox + sx * (internal_cell_size * (two_x + UVec4(0, 1, 0, 1))).ToFloat();
				Vec4 bounds_maxx = ox + sx * UVec4::sMin(internal_cell_size * (two_x + UVec4(1, 2, 1, 2)), sample_count_min_1).ToFloat();

				UVec4 two_y = UVec4::sReplicate(2 * y);
				Vec4 bounds_minz = oz + sz * (internal_cell_size * (two_y + UVec4(0, 0, 1, 1))).ToFloat();
				Vec4 bounds_maxz = oz + sz * UVec4::sMin(internal_cell_size * (two_y + UVec4(1, 1, 2, 2)), sample_count_min_1).ToFloat();

				// Calculate properties of child blocks
				UVec4 properties = UVec4::sReplicate(((level + 1) << cLevelShift) + (y << (cNumBitsXY + 1)) + (x << 1)) + UVec4(0, 1, 1 << cNumBitsXY, (1 << cNumBitsXY) + 1);

			#ifdef JPH_DEBUG_HEIGHT_FIELD
				// Draw boxes
				for (int i = 0; i < 4; ++i)
				{
					AABox b(Vec3(bounds_minx[i], bounds_miny[i], bounds_minz[i]), Vec3(bounds_maxx[i], bounds_maxy[i], bounds_maxz[i]));
					if (b.IsValid())
						DebugRenderer::sInstance->DrawWireBox(b, Color::sGreen);
				}
			#endif

				// Check which sub nodes to visit
				int num_results = ioVisitor.VisitRangeBlock(bounds_minx, bounds_miny, bounds_minz, bounds_maxx, bounds_maxy, bounds_maxz, properties, mTop);

				// Push them onto the stack
				JPH_ASSERT(mTop + 4 < cStackSize);
				properties.StoreInt4(&mPropertiesStack[mTop]);
				mTop += num_results;
			}

			// Check if we're done
			if (ioVisitor.ShouldAbort())
				return;

			// Fetch next node until we find one that the visitor wants to see
			do
				--mTop;
			while (mTop >= 0 && !ioVisitor.ShouldVisitRangeBlock(mTop));
		}
		while (mTop >= 0);
	}

	// This can be used to have the visitor early out (ioVisitor.ShouldAbort() returns true) and later continue again (call WalkHeightField() again)
	JPH_INLINE bool				IsDoneWalking() const
	{
		return mTop < 0;
	}

private:
	const HeightFieldShape *	mShape;
	int							mTop = 0;
	uint32						mPropertiesStack[cStackSize];
};

template <class Visitor>
void HeightFieldShape::WalkHeightField(Visitor &ioVisitor) const
{
	DecodingContext ctx(this);
	ctx.WalkHeightField(ioVisitor);
}

bool HeightFieldShape::CastRay(const RayCast &inRay, const SubShapeIDCreator &inSubShapeIDCreator, RayCastResult &ioHit) const
{
	JPH_PROFILE_FUNCTION();

	struct Visitor
	{
		JPH_INLINE explicit		Visitor(const HeightFieldShape *inShape, const RayCast &inRay, const SubShapeIDCreator &inSubShapeIDCreator, RayCastResult &ioHit) :
			mHit(ioHit),
			mRayOrigin(inRay.mOrigin),
			mRayDirection(inRay.mDirection),
			mRayInvDirection(inRay.mDirection),
			mShape(inShape),
			mSubShapeIDCreator(inSubShapeIDCreator)
		{
		}

		JPH_INLINE bool			ShouldAbort() const
		{
			return mHit.mFraction <= 0.0f;
		}

		JPH_INLINE bool			ShouldVisitRangeBlock(int inStackTop) const
		{
			return mDistanceStack[inStackTop] < mHit.mFraction;
		}

		JPH_INLINE int			VisitRangeBlock(Vec4Arg inBoundsMinX, Vec4Arg inBoundsMinY, Vec4Arg inBoundsMinZ, Vec4Arg inBoundsMaxX, Vec4Arg inBoundsMaxY, Vec4Arg inBoundsMaxZ, UVec4 &ioProperties, int inStackTop)
		{
			// Test bounds of 4 children
			Vec4 distance = RayAABox4(mRayOrigin, mRayInvDirection, inBoundsMinX, inBoundsMinY, inBoundsMinZ, inBoundsMaxX, inBoundsMaxY, inBoundsMaxZ);

			// Sort so that highest values are first (we want to first process closer hits and we process stack top to bottom)
			return SortReverseAndStore(distance, mHit.mFraction, ioProperties, &mDistanceStack[inStackTop]);
		}

		JPH_INLINE void			VisitTriangle(uint inX, uint inY, uint inTriangle, Vec3Arg inV0, Vec3Arg inV1, Vec3Arg inV2)
		{
			float fraction = RayTriangle(mRayOrigin, mRayDirection, inV0, inV1, inV2);
			if (fraction < mHit.mFraction)
			{
				// It's a closer hit
				mHit.mFraction = fraction;
				mHit.mSubShapeID2 = mShape->EncodeSubShapeID(mSubShapeIDCreator, inX, inY, inTriangle);
				mReturnValue = true;
			}
		}

		RayCastResult &			mHit;
		Vec3					mRayOrigin;
		Vec3					mRayDirection;
		RayInvDirection			mRayInvDirection;
		const HeightFieldShape *mShape;
		SubShapeIDCreator		mSubShapeIDCreator;
		bool					mReturnValue = false;
		float					mDistanceStack[cStackSize];
	};

	Visitor visitor(this, inRay, inSubShapeIDCreator, ioHit);
	WalkHeightField(visitor);

	return visitor.mReturnValue;
}

void HeightFieldShape::CastRay(const RayCast &inRay, const RayCastSettings &inRayCastSettings, const SubShapeIDCreator &inSubShapeIDCreator, CastRayCollector &ioCollector, const ShapeFilter &inShapeFilter) const
{
	JPH_PROFILE_FUNCTION();

	// Test shape filter
	if (!inShapeFilter.ShouldCollide(this, inSubShapeIDCreator.GetID()))
		return;

	struct Visitor
	{
		JPH_INLINE explicit		Visitor(const HeightFieldShape *inShape, const RayCast &inRay, const RayCastSettings &inRayCastSettings, const SubShapeIDCreator &inSubShapeIDCreator, CastRayCollector &ioCollector) :
			mCollector(ioCollector),
			mRayOrigin(inRay.mOrigin),
			mRayDirection(inRay.mDirection),
			mRayInvDirection(inRay.mDirection),
			mBackFaceMode(inRayCastSettings.mBackFaceModeTriangles),
			mShape(inShape),
			mSubShapeIDCreator(inSubShapeIDCreator)
		{
		}

		JPH_INLINE bool			ShouldAbort() const
		{
			return mCollector.ShouldEarlyOut();
		}

		JPH_INLINE bool			ShouldVisitRangeBlock(int inStackTop) const
		{
			return mDistanceStack[inStackTop] < mCollector.GetEarlyOutFraction();
		}

		JPH_INLINE int			VisitRangeBlock(Vec4Arg inBoundsMinX, Vec4Arg inBoundsMinY, Vec4Arg inBoundsMinZ, Vec4Arg inBoundsMaxX, Vec4Arg inBoundsMaxY, Vec4Arg inBoundsMaxZ, UVec4 &ioProperties, int inStackTop)
		{
			// Test bounds of 4 children
			Vec4 distance = RayAABox4(mRayOrigin, mRayInvDirection, inBoundsMinX, inBoundsMinY, inBoundsMinZ, inBoundsMaxX, inBoundsMaxY, inBoundsMaxZ);

			// Sort so that highest values are first (we want to first process closer hits and we process stack top to bottom)
			return SortReverseAndStore(distance, mCollector.GetEarlyOutFraction(), ioProperties, &mDistanceStack[inStackTop]);
		}

		JPH_INLINE void			VisitTriangle(uint inX, uint inY, uint inTriangle, Vec3Arg inV0, Vec3Arg inV1, Vec3Arg inV2) const
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
				hit.mSubShapeID2 = mShape->EncodeSubShapeID(mSubShapeIDCreator, inX, inY, inTriangle);
				mCollector.AddHit(hit);
			}
		}

		CastRayCollector &		mCollector;
		Vec3					mRayOrigin;
		Vec3					mRayDirection;
		RayInvDirection			mRayInvDirection;
		EBackFaceMode			mBackFaceMode;
		const HeightFieldShape *mShape;
		SubShapeIDCreator		mSubShapeIDCreator;
		float					mDistanceStack[cStackSize];
	};

	Visitor visitor(this, inRay, inRayCastSettings, inSubShapeIDCreator, ioCollector);
	WalkHeightField(visitor);
}

void HeightFieldShape::CollidePoint(Vec3Arg inPoint, const SubShapeIDCreator &inSubShapeIDCreator, CollidePointCollector &ioCollector, const ShapeFilter &inShapeFilter) const
{
	// A height field doesn't have volume, so we can't test insideness
}

void HeightFieldShape::CollideSoftBodyVertices(Mat44Arg inCenterOfMassTransform, Vec3Arg inScale, const CollideSoftBodyVertexIterator &inVertices, uint inNumVertices, int inCollidingShapeIndex) const
{
	JPH_PROFILE_FUNCTION();

	struct Visitor : public CollideSoftBodyVerticesVsTriangles
	{
		using CollideSoftBodyVerticesVsTriangles::CollideSoftBodyVerticesVsTriangles;

		JPH_INLINE bool	ShouldAbort() const
		{
			return false;
		}

		JPH_INLINE bool	ShouldVisitRangeBlock([[maybe_unused]] int inStackTop) const
		{
			return mDistanceStack[inStackTop] < mClosestDistanceSq;
		}

		JPH_INLINE int	VisitRangeBlock(Vec4Arg inBoundsMinX, Vec4Arg inBoundsMinY, Vec4Arg inBoundsMinZ, Vec4Arg inBoundsMaxX, Vec4Arg inBoundsMaxY, Vec4Arg inBoundsMaxZ, UVec4 &ioProperties, int inStackTop)
		{
			// Get distance to vertex
			Vec4 dist_sq = AABox4DistanceSqToPoint(mLocalPosition, inBoundsMinX, inBoundsMinY, inBoundsMinZ, inBoundsMaxX, inBoundsMaxY, inBoundsMaxZ);

			// Clear distance for invalid bounds
			dist_sq = Vec4::sSelect(Vec4::sReplicate(FLT_MAX), dist_sq, Vec4::sLessOrEqual(inBoundsMinY, inBoundsMaxY));

			// Sort so that highest values are first (we want to first process closer hits and we process stack top to bottom)
			return SortReverseAndStore(dist_sq, mClosestDistanceSq, ioProperties, &mDistanceStack[inStackTop]);
		}

		JPH_INLINE void	VisitTriangle([[maybe_unused]] uint inX, [[maybe_unused]] uint inY, [[maybe_unused]] uint inTriangle, Vec3Arg inV0, Vec3Arg inV1, Vec3Arg inV2)
		{
			ProcessTriangle(inV0, inV1, inV2);
		}

		float			mDistanceStack[cStackSize];
	};

	Visitor visitor(inCenterOfMassTransform, inScale);

	for (CollideSoftBodyVertexIterator v = inVertices, sbv_end = inVertices + inNumVertices; v != sbv_end; ++v)
		if (v.GetInvMass() > 0.0f)
		{
			visitor.StartVertex(v);
			WalkHeightField(visitor);
			visitor.FinishVertex(v, inCollidingShapeIndex);
		}
}

void HeightFieldShape::sCastConvexVsHeightField(const ShapeCast &inShapeCast, const ShapeCastSettings &inShapeCastSettings, const Shape *inShape, Vec3Arg inScale, [[maybe_unused]] const ShapeFilter &inShapeFilter, Mat44Arg inCenterOfMassTransform2, const SubShapeIDCreator &inSubShapeIDCreator1, const SubShapeIDCreator &inSubShapeIDCreator2, CastShapeCollector &ioCollector)
{
	JPH_PROFILE_FUNCTION();

	struct Visitor : public CastConvexVsTriangles
	{
		using CastConvexVsTriangles::CastConvexVsTriangles;

		JPH_INLINE bool				ShouldAbort() const
		{
			return mCollector.ShouldEarlyOut();
		}

		JPH_INLINE bool				ShouldVisitRangeBlock(int inStackTop) const
		{
			return mDistanceStack[inStackTop] < mCollector.GetPositiveEarlyOutFraction();
		}

		JPH_INLINE int				VisitRangeBlock(Vec4Arg inBoundsMinX, Vec4Arg inBoundsMinY, Vec4Arg inBoundsMinZ, Vec4Arg inBoundsMaxX, Vec4Arg inBoundsMaxY, Vec4Arg inBoundsMaxZ, UVec4 &ioProperties, int inStackTop)
		{
			// Scale the bounding boxes of this node
			Vec4 bounds_min_x, bounds_min_y, bounds_min_z, bounds_max_x, bounds_max_y, bounds_max_z;
			AABox4Scale(mScale, inBoundsMinX, inBoundsMinY, inBoundsMinZ, inBoundsMaxX, inBoundsMaxY, inBoundsMaxZ, bounds_min_x, bounds_min_y, bounds_min_z, bounds_max_x, bounds_max_y, bounds_max_z);

			// Enlarge them by the casted shape's box extents
			AABox4EnlargeWithExtent(mBoxExtent, bounds_min_x, bounds_min_y, bounds_min_z, bounds_max_x, bounds_max_y, bounds_max_z);

			// Test bounds of 4 children
			Vec4 distance = RayAABox4(mBoxCenter, mInvDirection, bounds_min_x, bounds_min_y, bounds_min_z, bounds_max_x, bounds_max_y, bounds_max_z);

			// Clear distance for invalid bounds
			distance = Vec4::sSelect(Vec4::sReplicate(FLT_MAX), distance, Vec4::sLessOrEqual(inBoundsMinY, inBoundsMaxY));

			// Sort so that highest values are first (we want to first process closer hits and we process stack top to bottom)
			return SortReverseAndStore(distance, mCollector.GetPositiveEarlyOutFraction(), ioProperties, &mDistanceStack[inStackTop]);
		}

		JPH_INLINE void				VisitTriangle(uint inX, uint inY, uint inTriangle, Vec3Arg inV0, Vec3Arg inV1, Vec3Arg inV2)
		{
			// Create sub shape id for this part
			SubShapeID triangle_sub_shape_id = mShape2->EncodeSubShapeID(mSubShapeIDCreator2, inX, inY, inTriangle);

			// Determine active edges
			uint8 active_edges = mShape2->GetEdgeFlags(inX, inY, inTriangle);

			Cast(inV0, inV1, inV2, active_edges, triangle_sub_shape_id);
		}

		const HeightFieldShape *	mShape2;
		RayInvDirection				mInvDirection;
		Vec3						mBoxCenter;
		Vec3						mBoxExtent;
		SubShapeIDCreator			mSubShapeIDCreator2;
		float						mDistanceStack[cStackSize];
	};

	JPH_ASSERT(inShape->GetSubType() == EShapeSubType::HeightField);
	const HeightFieldShape *shape = static_cast<const HeightFieldShape *>(inShape);

	Visitor visitor(inShapeCast, inShapeCastSettings, inScale, inCenterOfMassTransform2, inSubShapeIDCreator1, ioCollector);
	visitor.mShape2 = shape;
	visitor.mInvDirection.Set(inShapeCast.mDirection);
	visitor.mBoxCenter = inShapeCast.mShapeWorldBounds.GetCenter();
	visitor.mBoxExtent = inShapeCast.mShapeWorldBounds.GetExtent();
	visitor.mSubShapeIDCreator2 = inSubShapeIDCreator2;
	shape->WalkHeightField(visitor);
}

void HeightFieldShape::sCastSphereVsHeightField(const ShapeCast &inShapeCast, const ShapeCastSettings &inShapeCastSettings, const Shape *inShape, Vec3Arg inScale, [[maybe_unused]] const ShapeFilter &inShapeFilter, Mat44Arg inCenterOfMassTransform2, const SubShapeIDCreator &inSubShapeIDCreator1, const SubShapeIDCreator &inSubShapeIDCreator2, CastShapeCollector &ioCollector)
{
	JPH_PROFILE_FUNCTION();

	struct Visitor : public CastSphereVsTriangles
	{
		using CastSphereVsTriangles::CastSphereVsTriangles;

		JPH_INLINE bool				ShouldAbort() const
		{
			return mCollector.ShouldEarlyOut();
		}

		JPH_INLINE bool				ShouldVisitRangeBlock(int inStackTop) const
		{
			return mDistanceStack[inStackTop] < mCollector.GetPositiveEarlyOutFraction();
		}

		JPH_INLINE int				VisitRangeBlock(Vec4Arg inBoundsMinX, Vec4Arg inBoundsMinY, Vec4Arg inBoundsMinZ, Vec4Arg inBoundsMaxX, Vec4Arg inBoundsMaxY, Vec4Arg inBoundsMaxZ, UVec4 &ioProperties, int inStackTop)
		{
			// Scale the bounding boxes of this node
			Vec4 bounds_min_x, bounds_min_y, bounds_min_z, bounds_max_x, bounds_max_y, bounds_max_z;
			AABox4Scale(mScale, inBoundsMinX, inBoundsMinY, inBoundsMinZ, inBoundsMaxX, inBoundsMaxY, inBoundsMaxZ, bounds_min_x, bounds_min_y, bounds_min_z, bounds_max_x, bounds_max_y, bounds_max_z);

			// Enlarge them by the radius of the sphere
			AABox4EnlargeWithExtent(Vec3::sReplicate(mRadius), bounds_min_x, bounds_min_y, bounds_min_z, bounds_max_x, bounds_max_y, bounds_max_z);

			// Test bounds of 4 children
			Vec4 distance = RayAABox4(mStart, mInvDirection, bounds_min_x, bounds_min_y, bounds_min_z, bounds_max_x, bounds_max_y, bounds_max_z);

			// Clear distance for invalid bounds
			distance = Vec4::sSelect(Vec4::sReplicate(FLT_MAX), distance, Vec4::sLessOrEqual(inBoundsMinY, inBoundsMaxY));

			// Sort so that highest values are first (we want to first process closer hits and we process stack top to bottom)
			return SortReverseAndStore(distance, mCollector.GetPositiveEarlyOutFraction(), ioProperties, &mDistanceStack[inStackTop]);
		}

		JPH_INLINE void				VisitTriangle(uint inX, uint inY, uint inTriangle, Vec3Arg inV0, Vec3Arg inV1, Vec3Arg inV2)
		{
			// Create sub shape id for this part
			SubShapeID triangle_sub_shape_id = mShape2->EncodeSubShapeID(mSubShapeIDCreator2, inX, inY, inTriangle);

			// Determine active edges
			uint8 active_edges = mShape2->GetEdgeFlags(inX, inY, inTriangle);

			Cast(inV0, inV1, inV2, active_edges, triangle_sub_shape_id);
		}

		const HeightFieldShape *	mShape2;
		RayInvDirection				mInvDirection;
		SubShapeIDCreator			mSubShapeIDCreator2;
		float						mDistanceStack[cStackSize];
	};

	JPH_ASSERT(inShape->GetSubType() == EShapeSubType::HeightField);
	const HeightFieldShape *shape = static_cast<const HeightFieldShape *>(inShape);

	Visitor visitor(inShapeCast, inShapeCastSettings, inScale, inCenterOfMassTransform2, inSubShapeIDCreator1, ioCollector);
	visitor.mShape2 = shape;
	visitor.mInvDirection.Set(inShapeCast.mDirection);
	visitor.mSubShapeIDCreator2 = inSubShapeIDCreator2;
	shape->WalkHeightField(visitor);
}

struct HeightFieldShape::HSGetTrianglesContext
{
			HSGetTrianglesContext(const HeightFieldShape *inShape, const AABox &inBox, Vec3Arg inPositionCOM, QuatArg inRotation, Vec3Arg inScale) :
		mDecodeCtx(inShape),
		mShape(inShape),
		mLocalBox(Mat44::sInverseRotationTranslation(inRotation, inPositionCOM), inBox),
		mHeightFieldScale(inScale),
		mLocalToWorld(Mat44::sRotationTranslation(inRotation, inPositionCOM) * Mat44::sScale(inScale)),
		mIsInsideOut(ScaleHelpers::IsInsideOut(inScale))
	{
	}

	bool	ShouldAbort() const
	{
		return mShouldAbort;
	}

	bool	ShouldVisitRangeBlock([[maybe_unused]] int inStackTop) const
	{
		return true;
	}

	int		VisitRangeBlock(Vec4Arg inBoundsMinX, Vec4Arg inBoundsMinY, Vec4Arg inBoundsMinZ, Vec4Arg inBoundsMaxX, Vec4Arg inBoundsMaxY, Vec4Arg inBoundsMaxZ, UVec4 &ioProperties, [[maybe_unused]] int inStackTop) const
	{
		// Scale the bounding boxes of this node
		Vec4 bounds_min_x, bounds_min_y, bounds_min_z, bounds_max_x, bounds_max_y, bounds_max_z;
		AABox4Scale(mHeightFieldScale, inBoundsMinX, inBoundsMinY, inBoundsMinZ, inBoundsMaxX, inBoundsMaxY, inBoundsMaxZ, bounds_min_x, bounds_min_y, bounds_min_z, bounds_max_x, bounds_max_y, bounds_max_z);

		// Test which nodes collide
		UVec4 collides = AABox4VsBox(mLocalBox, bounds_min_x, bounds_min_y, bounds_min_z, bounds_max_x, bounds_max_y, bounds_max_z);

		// Filter out invalid bounding boxes
		collides = UVec4::sAnd(collides, Vec4::sLessOrEqual(inBoundsMinY, inBoundsMaxY));

		return CountAndSortTrues(collides, ioProperties);
	}

	void	VisitTriangle(uint inX, uint inY, [[maybe_unused]] uint inTriangle, Vec3Arg inV0, Vec3Arg inV1, Vec3Arg inV2)
	{
		// When the buffer is full and we cannot process the triangles, abort the height field walk. The next time GetTrianglesNext is called we will continue here.
		if (mNumTrianglesFound + 1 > mMaxTrianglesRequested)
		{
			mShouldAbort = true;
			return;
		}

		// Store vertices as Float3
		if (mIsInsideOut)
		{
			// Reverse vertices
			(mLocalToWorld * inV0).StoreFloat3(mTriangleVertices++);
			(mLocalToWorld * inV2).StoreFloat3(mTriangleVertices++);
			(mLocalToWorld * inV1).StoreFloat3(mTriangleVertices++);
		}
		else
		{
			// Normal scale
			(mLocalToWorld * inV0).StoreFloat3(mTriangleVertices++);
			(mLocalToWorld * inV1).StoreFloat3(mTriangleVertices++);
			(mLocalToWorld * inV2).StoreFloat3(mTriangleVertices++);
		}

		// Decode material
		if (mMaterials != nullptr)
			*mMaterials++ = mShape->GetMaterial(inX, inY);

		// Accumulate triangles found
		mNumTrianglesFound++;
	}

	DecodingContext				mDecodeCtx;
	const HeightFieldShape *	mShape;
	OrientedBox					mLocalBox;
	Vec3						mHeightFieldScale;
	Mat44						mLocalToWorld;
	int							mMaxTrianglesRequested;
	Float3 *					mTriangleVertices;
	int							mNumTrianglesFound;
	const PhysicsMaterial **	mMaterials;
	bool						mShouldAbort;
	bool						mIsInsideOut;
};

void HeightFieldShape::GetTrianglesStart(GetTrianglesContext &ioContext, const AABox &inBox, Vec3Arg inPositionCOM, QuatArg inRotation, Vec3Arg inScale) const
{
	static_assert(sizeof(HSGetTrianglesContext) <= sizeof(GetTrianglesContext), "GetTrianglesContext too small");
	JPH_ASSERT(IsAligned(&ioContext, alignof(HSGetTrianglesContext)));

	new (&ioContext) HSGetTrianglesContext(this, inBox, inPositionCOM, inRotation, inScale);
}

int HeightFieldShape::GetTrianglesNext(GetTrianglesContext &ioContext, int inMaxTrianglesRequested, Float3 *outTriangleVertices, const PhysicsMaterial **outMaterials) const
{
	static_assert(cGetTrianglesMinTrianglesRequested >= 1, "cGetTrianglesMinTrianglesRequested is too small");
	JPH_ASSERT(inMaxTrianglesRequested >= cGetTrianglesMinTrianglesRequested);

	// Check if we're done
	HSGetTrianglesContext &context = (HSGetTrianglesContext &)ioContext;
	if (context.mDecodeCtx.IsDoneWalking())
		return 0;

	// Store parameters on context
	context.mMaxTrianglesRequested = inMaxTrianglesRequested;
	context.mTriangleVertices = outTriangleVertices;
	context.mMaterials = outMaterials;
	context.mShouldAbort = false; // Reset the abort flag
	context.mNumTrianglesFound = 0;

	// Continue (or start) walking the height field
	context.mDecodeCtx.WalkHeightField(context);
	return context.mNumTrianglesFound;
}

void HeightFieldShape::sCollideConvexVsHeightField(const Shape *inShape1, const Shape *inShape2, Vec3Arg inScale1, Vec3Arg inScale2, Mat44Arg inCenterOfMassTransform1, Mat44Arg inCenterOfMassTransform2, const SubShapeIDCreator &inSubShapeIDCreator1, const SubShapeIDCreator &inSubShapeIDCreator2, const CollideShapeSettings &inCollideShapeSettings, CollideShapeCollector &ioCollector, [[maybe_unused]] const ShapeFilter &inShapeFilter)
{
	JPH_PROFILE_FUNCTION();

	// Get the shapes
	JPH_ASSERT(inShape1->GetType() == EShapeType::Convex);
	JPH_ASSERT(inShape2->GetType() == EShapeType::HeightField);
	const ConvexShape *shape1 = static_cast<const ConvexShape *>(inShape1);
	const HeightFieldShape *shape2 = static_cast<const HeightFieldShape *>(inShape2);

	struct Visitor : public CollideConvexVsTriangles
	{
		using CollideConvexVsTriangles::CollideConvexVsTriangles;

		JPH_INLINE bool				ShouldAbort() const
		{
			return mCollector.ShouldEarlyOut();
		}

		JPH_INLINE bool				ShouldVisitRangeBlock([[maybe_unused]] int inStackTop) const
		{
			return true;
		}

		JPH_INLINE int				VisitRangeBlock(Vec4Arg inBoundsMinX, Vec4Arg inBoundsMinY, Vec4Arg inBoundsMinZ, Vec4Arg inBoundsMaxX, Vec4Arg inBoundsMaxY, Vec4Arg inBoundsMaxZ, UVec4 &ioProperties, [[maybe_unused]] int inStackTop) const
		{
			// Scale the bounding boxes of this node
			Vec4 bounds_min_x, bounds_min_y, bounds_min_z, bounds_max_x, bounds_max_y, bounds_max_z;
			AABox4Scale(mScale2, inBoundsMinX, inBoundsMinY, inBoundsMinZ, inBoundsMaxX, inBoundsMaxY, inBoundsMaxZ, bounds_min_x, bounds_min_y, bounds_min_z, bounds_max_x, bounds_max_y, bounds_max_z);

			// Test which nodes collide
			UVec4 collides = AABox4VsBox(mBoundsOf1InSpaceOf2, bounds_min_x, bounds_min_y, bounds_min_z, bounds_max_x, bounds_max_y, bounds_max_z);

			// Filter out invalid bounding boxes
			collides = UVec4::sAnd(collides, Vec4::sLessOrEqual(inBoundsMinY, inBoundsMaxY));

			return CountAndSortTrues(collides, ioProperties);
		}

		JPH_INLINE void				VisitTriangle(uint inX, uint inY, uint inTriangle, Vec3Arg inV0, Vec3Arg inV1, Vec3Arg inV2)
		{
			// Create ID for triangle
			SubShapeID triangle_sub_shape_id = mShape2->EncodeSubShapeID(mSubShapeIDCreator2, inX, inY, inTriangle);

			// Determine active edges
			uint8 active_edges = mShape2->GetEdgeFlags(inX, inY, inTriangle);

			Collide(inV0, inV1, inV2, active_edges, triangle_sub_shape_id);
		}

		const HeightFieldShape *	mShape2;
		SubShapeIDCreator			mSubShapeIDCreator2;
	};

	Visitor visitor(shape1, inScale1, inScale2, inCenterOfMassTransform1, inCenterOfMassTransform2, inSubShapeIDCreator1.GetID(), inCollideShapeSettings, ioCollector);
	visitor.mShape2 = shape2;
	visitor.mSubShapeIDCreator2 = inSubShapeIDCreator2;
	shape2->WalkHeightField(visitor);
}

void HeightFieldShape::sCollideSphereVsHeightField(const Shape *inShape1, const Shape *inShape2, Vec3Arg inScale1, Vec3Arg inScale2, Mat44Arg inCenterOfMassTransform1, Mat44Arg inCenterOfMassTransform2, const SubShapeIDCreator &inSubShapeIDCreator1, const SubShapeIDCreator &inSubShapeIDCreator2, const CollideShapeSettings &inCollideShapeSettings, CollideShapeCollector &ioCollector, [[maybe_unused]] const ShapeFilter &inShapeFilter)
{
	JPH_PROFILE_FUNCTION();

	// Get the shapes
	JPH_ASSERT(inShape1->GetSubType() == EShapeSubType::Sphere);
	JPH_ASSERT(inShape2->GetType() == EShapeType::HeightField);
	const SphereShape *shape1 = static_cast<const SphereShape *>(inShape1);
	const HeightFieldShape *shape2 = static_cast<const HeightFieldShape *>(inShape2);

	struct Visitor : public CollideSphereVsTriangles
	{
		using CollideSphereVsTriangles::CollideSphereVsTriangles;

		JPH_INLINE bool				ShouldAbort() const
		{
			return mCollector.ShouldEarlyOut();
		}

		JPH_INLINE bool				ShouldVisitRangeBlock([[maybe_unused]] int inStackTop) const
		{
			return true;
		}

		JPH_INLINE int				VisitRangeBlock(Vec4Arg inBoundsMinX, Vec4Arg inBoundsMinY, Vec4Arg inBoundsMinZ, Vec4Arg inBoundsMaxX, Vec4Arg inBoundsMaxY, Vec4Arg inBoundsMaxZ, UVec4 &ioProperties, [[maybe_unused]] int inStackTop) const
		{
			// Scale the bounding boxes of this node
			Vec4 bounds_min_x, bounds_min_y, bounds_min_z, bounds_max_x, bounds_max_y, bounds_max_z;
			AABox4Scale(mScale2, inBoundsMinX, inBoundsMinY, inBoundsMinZ, inBoundsMaxX, inBoundsMaxY, inBoundsMaxZ, bounds_min_x, bounds_min_y, bounds_min_z, bounds_max_x, bounds_max_y, bounds_max_z);

			// Test which nodes collide
			UVec4 collides = AABox4VsSphere(mSphereCenterIn2, mRadiusPlusMaxSeparationSq, bounds_min_x, bounds_min_y, bounds_min_z, bounds_max_x, bounds_max_y, bounds_max_z);

			// Filter out invalid bounding boxes
			collides = UVec4::sAnd(collides, Vec4::sLessOrEqual(inBoundsMinY, inBoundsMaxY));

			return CountAndSortTrues(collides, ioProperties);
		}

		JPH_INLINE void				VisitTriangle(uint inX, uint inY, uint inTriangle, Vec3Arg inV0, Vec3Arg inV1, Vec3Arg inV2)
		{
			// Create ID for triangle
			SubShapeID triangle_sub_shape_id = mShape2->EncodeSubShapeID(mSubShapeIDCreator2, inX, inY, inTriangle);

			// Determine active edges
			uint8 active_edges = mShape2->GetEdgeFlags(inX, inY, inTriangle);

			Collide(inV0, inV1, inV2, active_edges, triangle_sub_shape_id);
		}

		const HeightFieldShape *	mShape2;
		SubShapeIDCreator			mSubShapeIDCreator2;
	};

	Visitor visitor(shape1, inScale1, inScale2, inCenterOfMassTransform1, inCenterOfMassTransform2, inSubShapeIDCreator1.GetID(), inCollideShapeSettings, ioCollector);
	visitor.mShape2 = shape2;
	visitor.mSubShapeIDCreator2 = inSubShapeIDCreator2;
	shape2->WalkHeightField(visitor);
}

void HeightFieldShape::SaveBinaryState(StreamOut &inStream) const
{
	Shape::SaveBinaryState(inStream);

	inStream.Write(mOffset);
	inStream.Write(mScale);
	inStream.Write(mSampleCount);
	inStream.Write(mBlockSize);
	inStream.Write(mBitsPerSample);
	inStream.Write(mMinSample);
	inStream.Write(mMaxSample);
	inStream.Write(mMaterialIndices);
	inStream.Write(mNumBitsPerMaterialIndex);

	if (mRangeBlocks != nullptr)
	{
		inStream.Write(true);
		inStream.WriteBytes(mRangeBlocks, mRangeBlocksSize * sizeof(RangeBlock) + mHeightSamplesSize + mActiveEdgesSize);
	}
	else
	{
		inStream.Write(false);
	}
}

void HeightFieldShape::RestoreBinaryState(StreamIn &inStream)
{
	Shape::RestoreBinaryState(inStream);

	inStream.Read(mOffset);
	inStream.Read(mScale);
	inStream.Read(mSampleCount);
	inStream.Read(mBlockSize);
	inStream.Read(mBitsPerSample);
	inStream.Read(mMinSample);
	inStream.Read(mMaxSample);
	inStream.Read(mMaterialIndices);
	inStream.Read(mNumBitsPerMaterialIndex);

	// We don't have the exact number of reserved materials anymore, but ensure that our array is big enough
	// TODO: Next time when we bump the binary serialization format of this class we should store the capacity and allocate the right amount, for now we accept a little bit of waste
	mMaterials.reserve(PhysicsMaterialList::size_type(1) << mNumBitsPerMaterialIndex);

	CacheValues();

	bool has_heights = false;
	inStream.Read(has_heights);
	if (has_heights)
	{
		AllocateBuffers();
		inStream.ReadBytes(mRangeBlocks, mRangeBlocksSize * sizeof(RangeBlock) + mHeightSamplesSize + mActiveEdgesSize);
	}
}

void HeightFieldShape::SaveMaterialState(PhysicsMaterialList &outMaterials) const
{
	outMaterials = mMaterials;
}

void HeightFieldShape::RestoreMaterialState(const PhysicsMaterialRefC *inMaterials, uint inNumMaterials)
{
	mMaterials.assign(inMaterials, inMaterials + inNumMaterials);
}

Shape::Stats HeightFieldShape::GetStats() const
{
	return Stats(
		sizeof(*this)
			+ mMaterials.size() * sizeof(Ref<PhysicsMaterial>)
			+ mRangeBlocksSize * sizeof(RangeBlock)
			+ mHeightSamplesSize * sizeof(uint8)
			+ mActiveEdgesSize * sizeof(uint8)
			+ mMaterialIndices.size() * sizeof(uint8),
		mHeightSamplesSize == 0? 0 : Square(mSampleCount - 1) * 2);
}

void HeightFieldShape::sRegister()
{
	ShapeFunctions &f = ShapeFunctions::sGet(EShapeSubType::HeightField);
	f.mConstruct = []() -> Shape * { return new HeightFieldShape; };
	f.mColor = Color::sPurple;

	for (EShapeSubType s : sConvexSubShapeTypes)
	{
		CollisionDispatch::sRegisterCollideShape(s, EShapeSubType::HeightField, sCollideConvexVsHeightField);
		CollisionDispatch::sRegisterCastShape(s, EShapeSubType::HeightField, sCastConvexVsHeightField);

		CollisionDispatch::sRegisterCastShape(EShapeSubType::HeightField, s, CollisionDispatch::sReversedCastShape);
		CollisionDispatch::sRegisterCollideShape(EShapeSubType::HeightField, s, CollisionDispatch::sReversedCollideShape);
	}

	// Specialized collision functions
	CollisionDispatch::sRegisterCollideShape(EShapeSubType::Sphere, EShapeSubType::HeightField, sCollideSphereVsHeightField);
	CollisionDispatch::sRegisterCastShape(EShapeSubType::Sphere, EShapeSubType::HeightField, sCastSphereVsHeightField);
}

JPH_NAMESPACE_END

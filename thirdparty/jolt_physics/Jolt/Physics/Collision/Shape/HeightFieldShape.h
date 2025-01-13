// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Jolt/Physics/Collision/Shape/Shape.h>
#include <Jolt/Physics/Collision/PhysicsMaterial.h>
#ifdef JPH_DEBUG_RENDERER
	#include <Jolt/Renderer/DebugRenderer.h>
#endif // JPH_DEBUG_RENDERER

JPH_NAMESPACE_BEGIN

class ConvexShape;
class CollideShapeSettings;
class TempAllocator;

/// Constants for HeightFieldShape, this was moved out of the HeightFieldShape because of a linker bug
namespace HeightFieldShapeConstants
{
	/// Value used to create gaps in the height field
	constexpr float					cNoCollisionValue = FLT_MAX;

	/// Stack size to use during WalkHeightField
	constexpr int					cStackSize = 128;

	/// A position in the hierarchical grid is defined by a level (which grid), x and y position. We encode this in a single uint32 as: level << 28 | y << 14 | x
	constexpr uint					cNumBitsXY = 14;
	constexpr uint					cMaskBitsXY = (1 << cNumBitsXY) - 1;
	constexpr uint					cLevelShift = 2 * cNumBitsXY;

	/// When height samples are converted to 16 bit:
	constexpr uint16				cNoCollisionValue16 = 0xffff;				///< This is the magic value for 'no collision'
	constexpr uint16				cMaxHeightValue16 = 0xfffe;					///< This is the maximum allowed height value
};

/// Class that constructs a HeightFieldShape
class JPH_EXPORT HeightFieldShapeSettings final : public ShapeSettings
{
	JPH_DECLARE_SERIALIZABLE_VIRTUAL(JPH_EXPORT, HeightFieldShapeSettings)

public:
	/// Default constructor for deserialization
									HeightFieldShapeSettings() = default;

	/// Create a height field shape of inSampleCount * inSampleCount vertices.
	/// The height field is a surface defined by: inOffset + inScale * (x, inSamples[y * inSampleCount + x], y).
	/// where x and y are integers in the range x and y e [0, inSampleCount - 1].
	/// inSampleCount: inSampleCount / mBlockSize must be minimally 2 and a power of 2 is the most efficient in terms of performance and storage.
	/// inSamples: inSampleCount^2 vertices.
	/// inMaterialIndices: (inSampleCount - 1)^2 indices that index into inMaterialList.
									HeightFieldShapeSettings(const float *inSamples, Vec3Arg inOffset, Vec3Arg inScale, uint32 inSampleCount, const uint8 *inMaterialIndices = nullptr, const PhysicsMaterialList &inMaterialList = PhysicsMaterialList());

	// See: ShapeSettings
	virtual ShapeResult				Create() const override;

	/// Determine the minimal and maximal value of mHeightSamples (will ignore cNoCollisionValue)
	/// @param outMinValue The minimal value of mHeightSamples or FLT_MAX if no samples have collision
	/// @param outMaxValue The maximal value of mHeightSamples or -FLT_MAX if no samples have collision
	/// @param outQuantizationScale (value - outMinValue) * outQuantizationScale quantizes a height sample to 16 bits
	void							DetermineMinAndMaxSample(float &outMinValue, float &outMaxValue, float &outQuantizationScale) const;

	/// Given mBlockSize, mSampleCount and mHeightSamples, calculate the amount of bits needed to stay below absolute error inMaxError
	/// @param inMaxError Maximum allowed error in mHeightSamples after compression (note that this does not take mScale.Y into account)
	/// @return Needed bits per sample in the range [1, 8].
	uint32							CalculateBitsPerSampleForError(float inMaxError) const;

	/// The height field is a surface defined by: mOffset + mScale * (x, mHeightSamples[y * mSampleCount + x], y).
	/// where x and y are integers in the range x and y e [0, mSampleCount - 1].
	Vec3							mOffset = Vec3::sZero();
	Vec3							mScale = Vec3::sReplicate(1.0f);
	uint32							mSampleCount = 0;

	/// Artificial minimal value of mHeightSamples, used for compression and can be used to update the terrain after creating with lower height values. If there are any lower values in mHeightSamples, this value will be ignored.
	float							mMinHeightValue = FLT_MAX;

	/// Artificial maximum value of mHeightSamples, used for compression and can be used to update the terrain after creating with higher height values. If there are any higher values in mHeightSamples, this value will be ignored.
	float							mMaxHeightValue = -FLT_MAX;

	/// When bigger than mMaterials.size() the internal material list will be preallocated to support this number of materials.
	/// This avoids reallocations when calling HeightFieldShape::SetMaterials with new materials later.
	uint32							mMaterialsCapacity = 0;

	/// The heightfield is divided in blocks of mBlockSize * mBlockSize * 2 triangles and the acceleration structure culls blocks only,
	/// bigger block sizes reduce memory consumption but also reduce query performance. Sensible values are [2, 8], does not need to be
	/// a power of 2. Note that at run-time we'll perform one more grid subdivision, so the effective block size is half of what is provided here.
	uint32							mBlockSize = 2;

	/// How many bits per sample to use to compress the height field. Can be in the range [1, 8].
	/// Note that each sample is compressed relative to the min/max value of its block of mBlockSize * mBlockSize pixels so the effective precision is higher.
	/// Also note that increasing mBlockSize saves more memory than reducing the amount of bits per sample.
	uint32							mBitsPerSample = 8;

	/// An array of mSampleCount^2 height samples. Samples are stored in row major order, so the sample at (x, y) is at index y * mSampleCount + x.
	Array<float>					mHeightSamples;

	/// An array of (mSampleCount - 1)^2 material indices.
	Array<uint8>					mMaterialIndices;

	/// The materials of square at (x, y) is: mMaterials[mMaterialIndices[x + y * (mSampleCount - 1)]]
	PhysicsMaterialList				mMaterials;

	/// Cosine of the threshold angle (if the angle between the two triangles is bigger than this, the edge is active, note that a concave edge is always inactive).
	/// Setting this value too small can cause ghost collisions with edges, setting it too big can cause depenetration artifacts (objects not depenetrating quickly).
	/// Valid ranges are between cos(0 degrees) and cos(90 degrees). The default value is cos(5 degrees).
	float							mActiveEdgeCosThresholdAngle = 0.996195f;	// cos(5 degrees)
};

/// A height field shape. Cannot be used as a dynamic object.
///
/// Note: If you're using HeightFieldShape and are querying data while modifying the shape you'll have a race condition.
/// In this case it is best to create a new HeightFieldShape using the Clone function. You replace the shape on a body using BodyInterface::SetShape.
/// If a query is still working on the old shape, it will have taken a reference and keep the old shape alive until the query finishes.
class JPH_EXPORT HeightFieldShape final : public Shape
{
public:
	JPH_OVERRIDE_NEW_DELETE

	/// Constructor
									HeightFieldShape() : Shape(EShapeType::HeightField, EShapeSubType::HeightField) { }
									HeightFieldShape(const HeightFieldShapeSettings &inSettings, ShapeResult &outResult);
	virtual							~HeightFieldShape() override;

	/// Clone this shape. Can be used to avoid race conditions. See the documentation of this class for more information.
	Ref<HeightFieldShape>			Clone() const;

	// See Shape::MustBeStatic
	virtual bool					MustBeStatic() const override				{ return true; }

	/// Get the size of the height field. Note that this will always be rounded up to the nearest multiple of GetBlockSize().
	inline uint						GetSampleCount() const						{ return mSampleCount; }

	/// Get the size of a block
	inline uint						GetBlockSize() const						{ return mBlockSize; }

	// See Shape::GetLocalBounds
	virtual AABox					GetLocalBounds() const override;

	// See Shape::GetSubShapeIDBitsRecursive
	virtual uint					GetSubShapeIDBitsRecursive() const override	{ return GetSubShapeIDBits(); }

	// See Shape::GetInnerRadius
	virtual float					GetInnerRadius() const override				{ return 0.0f; }

	// See Shape::GetMassProperties
	virtual MassProperties			GetMassProperties() const override;

	// See Shape::GetMaterial
	virtual const PhysicsMaterial *	GetMaterial(const SubShapeID &inSubShapeID) const override;

	/// Overload to get the material at a particular location
	const PhysicsMaterial *			GetMaterial(uint inX, uint inY) const;

	// See Shape::GetSurfaceNormal
	virtual Vec3					GetSurfaceNormal(const SubShapeID &inSubShapeID, Vec3Arg inLocalSurfacePosition) const override;

	// See Shape::GetSupportingFace
	virtual void					GetSupportingFace(const SubShapeID &inSubShapeID, Vec3Arg inDirection, Vec3Arg inScale, Mat44Arg inCenterOfMassTransform, SupportingFace &outVertices) const override;

	// See Shape::GetSubmergedVolume
	virtual void					GetSubmergedVolume(Mat44Arg inCenterOfMassTransform, Vec3Arg inScale, const Plane &inSurface, float &outTotalVolume, float &outSubmergedVolume, Vec3 &outCenterOfBuoyancy JPH_IF_DEBUG_RENDERER(, RVec3Arg inBaseOffset)) const override { JPH_ASSERT(false, "Not supported"); }

#ifdef JPH_DEBUG_RENDERER
	// See Shape::Draw
	virtual void					Draw(DebugRenderer *inRenderer, RMat44Arg inCenterOfMassTransform, Vec3Arg inScale, ColorArg inColor, bool inUseMaterialColors, bool inDrawWireframe) const override;
#endif // JPH_DEBUG_RENDERER

	// See Shape::CastRay
	virtual bool					CastRay(const RayCast &inRay, const SubShapeIDCreator &inSubShapeIDCreator, RayCastResult &ioHit) const override;
	virtual void					CastRay(const RayCast &inRay, const RayCastSettings &inRayCastSettings, const SubShapeIDCreator &inSubShapeIDCreator, CastRayCollector &ioCollector, const ShapeFilter &inShapeFilter = { }) const override;

	// See: Shape::CollidePoint
	virtual void					CollidePoint(Vec3Arg inPoint, const SubShapeIDCreator &inSubShapeIDCreator, CollidePointCollector &ioCollector, const ShapeFilter &inShapeFilter = { }) const override;

	// See: Shape::CollideSoftBodyVertices
	virtual void					CollideSoftBodyVertices(Mat44Arg inCenterOfMassTransform, Vec3Arg inScale, const CollideSoftBodyVertexIterator &inVertices, uint inNumVertices, int inCollidingShapeIndex) const override;

	// See Shape::GetTrianglesStart
	virtual void					GetTrianglesStart(GetTrianglesContext &ioContext, const AABox &inBox, Vec3Arg inPositionCOM, QuatArg inRotation, Vec3Arg inScale) const override;

	// See Shape::GetTrianglesNext
	virtual int						GetTrianglesNext(GetTrianglesContext &ioContext, int inMaxTrianglesRequested, Float3 *outTriangleVertices, const PhysicsMaterial **outMaterials = nullptr) const override;

	/// Get height field position at sampled location (inX, inY).
	/// where inX and inY are integers in the range inX e [0, mSampleCount - 1] and inY e [0, mSampleCount - 1].
	Vec3							GetPosition(uint inX, uint inY) const;

	/// Check if height field at sampled location (inX, inY) has collision (has a hole or not)
	bool							IsNoCollision(uint inX, uint inY) const;

	/// Projects inLocalPosition (a point in the space of the shape) along the Y axis onto the surface and returns it in outSurfacePosition.
	/// When there is no surface position (because of a hole or because the point is outside the heightfield) the function will return false.
	bool							ProjectOntoSurface(Vec3Arg inLocalPosition, Vec3 &outSurfacePosition, SubShapeID &outSubShapeID) const;

	/// Returns the coordinates of the triangle that a sub shape ID represents
	/// @param inSubShapeID The sub shape ID to decode
	/// @param outX X coordinate of the triangle (in the range [0, mSampleCount - 2])
	/// @param outY Y coordinate of the triangle (in the range [0, mSampleCount - 2])
	/// @param outTriangleIndex Triangle within the quad (0 = lower triangle or 1 = upper triangle)
	void							GetSubShapeCoordinates(const SubShapeID &inSubShapeID, uint &outX, uint &outY, uint &outTriangleIndex) const;

	/// Get the range of height values that this height field can encode. Can be used to determine the allowed range when setting the height values with SetHeights.
	float							GetMinHeightValue() const					{ return mOffset.GetY(); }
	float							GetMaxHeightValue() const					{ return mOffset.GetY() + mScale.GetY() * HeightFieldShapeConstants::cMaxHeightValue16; }

	/// Get the height values of a block of data.
	/// Note that the height values are decompressed so will be slightly different from what the shape was originally created with.
	/// @param inX Start X position, must be a multiple of mBlockSize and in the range [0, mSampleCount - 1]
	/// @param inY Start Y position, must be a multiple of mBlockSize and in the range [0, mSampleCount - 1]
	/// @param inSizeX Number of samples in X direction, must be a multiple of mBlockSize and in the range [0, mSampleCount - inX]
	/// @param inSizeY Number of samples in Y direction, must be a multiple of mBlockSize and in the range [0, mSampleCount - inY]
	/// @param outHeights Returned height values, must be at least inSizeX * inSizeY floats. Values are returned in x-major order and can be cNoCollisionValue.
	/// @param inHeightsStride Stride in floats between two consecutive rows of outHeights (can be negative if the data is upside down).
	void							GetHeights(uint inX, uint inY, uint inSizeX, uint inSizeY, float *outHeights, intptr_t inHeightsStride) const;

	/// Set the height values of a block of data.
	/// Note that this requires decompressing and recompressing a border of size mBlockSize in the negative x/y direction so will cause some precision loss.
	/// Beware this can create a race condition if you're running collision queries in parallel. See class documentation for more information.
	/// @param inX Start X position, must be a multiple of mBlockSize and in the range [0, mSampleCount - 1]
	/// @param inY Start Y position, must be a multiple of mBlockSize and in the range [0, mSampleCount - 1]
	/// @param inSizeX Number of samples in X direction, must be a multiple of mBlockSize and in the range [0, mSampleCount - inX]
	/// @param inSizeY Number of samples in Y direction, must be a multiple of mBlockSize and in the range [0, mSampleCount - inY]
	/// @param inHeights The new height values to set, must be an array of inSizeX * inSizeY floats, can be cNoCollisionValue. Values outside of the range [GetMinHeightValue(), GetMaxHeightValue()] will be clamped.
	/// @param inHeightsStride Stride in floats between two consecutive rows of inHeights (can be negative if the data is upside down).
	/// @param inAllocator Allocator to use for temporary memory
	/// @param inActiveEdgeCosThresholdAngle Cosine of the threshold angle (if the angle between the two triangles is bigger than this, the edge is active, note that a concave edge is always inactive).
	void							SetHeights(uint inX, uint inY, uint inSizeX, uint inSizeY, const float *inHeights, intptr_t inHeightsStride, TempAllocator &inAllocator, float inActiveEdgeCosThresholdAngle = 0.996195f);

	/// Get the current list of materials, the indices returned by GetMaterials() will index into this list.
	const PhysicsMaterialList &		GetMaterialList() const						{ return mMaterials; }

	/// Get the material indices of a block of data.
	/// @param inX Start X position, must in the range [0, mSampleCount - 1]
	/// @param inY Start Y position, must in the range [0, mSampleCount - 1]
	/// @param inSizeX Number of samples in X direction
	/// @param inSizeY Number of samples in Y direction
	/// @param outMaterials Returned material indices, must be at least inSizeX * inSizeY uint8s. Values are returned in x-major order.
	/// @param inMaterialsStride Stride in uint8s between two consecutive rows of outMaterials (can be negative if the data is upside down).
	void							GetMaterials(uint inX, uint inY, uint inSizeX, uint inSizeY, uint8 *outMaterials, intptr_t inMaterialsStride) const;

	/// Set the material indices of a block of data.
	/// Beware this can create a race condition if you're running collision queries in parallel. See class documentation for more information.
	/// @param inX Start X position, must in the range [0, mSampleCount - 1]
	/// @param inY Start Y position, must in the range [0, mSampleCount - 1]
	/// @param inSizeX Number of samples in X direction
	/// @param inSizeY Number of samples in Y direction
	/// @param inMaterials The new material indices, must be at least inSizeX * inSizeY uint8s. Values are returned in x-major order.
	/// @param inMaterialsStride Stride in uint8s between two consecutive rows of inMaterials (can be negative if the data is upside down).
	/// @param inMaterialList The material list to use for the new material indices or nullptr if the material list should not be updated
	/// @param inAllocator Allocator to use for temporary memory
	/// @return True if the material indices were set, false if the total number of materials exceeded 256
	bool							SetMaterials(uint inX, uint inY, uint inSizeX, uint inSizeY, const uint8 *inMaterials, intptr_t inMaterialsStride, const PhysicsMaterialList *inMaterialList, TempAllocator &inAllocator);

	// See Shape
	virtual void					SaveBinaryState(StreamOut &inStream) const override;
	virtual void					SaveMaterialState(PhysicsMaterialList &outMaterials) const override;
	virtual void					RestoreMaterialState(const PhysicsMaterialRefC *inMaterials, uint inNumMaterials) override;

	// See Shape::GetStats
	virtual Stats					GetStats() const override;

	// See Shape::GetVolume
	virtual float					GetVolume() const override					{ return 0; }

#ifdef JPH_DEBUG_RENDERER
	// Settings
	static bool						sDrawTriangleOutlines;
#endif // JPH_DEBUG_RENDERER

	// Register shape functions with the registry
	static void						sRegister();

protected:
	// See: Shape::RestoreBinaryState
	virtual void					RestoreBinaryState(StreamIn &inStream) override;

private:
	class							DecodingContext;							///< Context class for walking through all nodes of a heightfield
	struct							HSGetTrianglesContext;						///< Context class for GetTrianglesStart/Next

	/// Calculate commonly used values and store them in the shape
	void							CacheValues();

	/// Allocate the mRangeBlocks, mHeightSamples and mActiveEdges buffers as a single data block
	void							AllocateBuffers();

	/// Calculate bit mask for all active edges in the heightfield for a specific region
	void							CalculateActiveEdges(uint inX, uint inY, uint inSizeX, uint inSizeY, const float *inHeights, uint inHeightsStartX, uint inHeightsStartY, intptr_t inHeightsStride, float inHeightsScale, float inActiveEdgeCosThresholdAngle, TempAllocator &inAllocator);

	/// Calculate bit mask for all active edges in the heightfield
	void							CalculateActiveEdges(const HeightFieldShapeSettings &inSettings);

	/// Store material indices in the least amount of bits per index possible
	void							StoreMaterialIndices(const HeightFieldShapeSettings &inSettings);

	/// Get the amount of horizontal/vertical blocks
	inline uint						GetNumBlocks() const						{ return mSampleCount / mBlockSize; }

	/// Get the maximum level (amount of grids) of the tree
	static inline uint				sGetMaxLevel(uint inNumBlocks)				{ return 32 - CountLeadingZeros(inNumBlocks - 1); }

	/// Get the range block offset and stride for GetBlockOffsetAndScale
	static inline void				sGetRangeBlockOffsetAndStride(uint inNumBlocks, uint inMaxLevel, uint &outRangeBlockOffset, uint &outRangeBlockStride);

	/// For block (inBlockX, inBlockY) get the offset and scale needed to decode a uint8 height sample to a uint16
	inline void						GetBlockOffsetAndScale(uint inBlockX, uint inBlockY, uint inRangeBlockOffset, uint inRangeBlockStride, float &outBlockOffset, float &outBlockScale) const;

	/// Get the height sample at position (inX, inY)
	inline uint8					GetHeightSample(uint inX, uint inY) const;

	/// Faster version of GetPosition when block offset and scale are already known
	inline Vec3						GetPosition(uint inX, uint inY, float inBlockOffset, float inBlockScale, bool &outNoCollision) const;

	/// Determine amount of bits needed to encode sub shape id
	uint							GetSubShapeIDBits() const;

	/// En/decode a sub shape ID. inX and inY specify the coordinate of the triangle. inTriangle == 0 is the lower triangle, inTriangle == 1 is the upper triangle.
	inline SubShapeID				EncodeSubShapeID(const SubShapeIDCreator &inCreator, uint inX, uint inY, uint inTriangle) const;
	inline void						DecodeSubShapeID(const SubShapeID &inSubShapeID, uint &outX, uint &outY, uint &outTriangle) const;

	/// Get the edge flags for a triangle
	inline uint8					GetEdgeFlags(uint inX, uint inY, uint inTriangle) const;

	// Helper functions called by CollisionDispatch
	static void						sCollideConvexVsHeightField(const Shape *inShape1, const Shape *inShape2, Vec3Arg inScale1, Vec3Arg inScale2, Mat44Arg inCenterOfMassTransform1, Mat44Arg inCenterOfMassTransform2, const SubShapeIDCreator &inSubShapeIDCreator1, const SubShapeIDCreator &inSubShapeIDCreator2, const CollideShapeSettings &inCollideShapeSettings, CollideShapeCollector &ioCollector, const ShapeFilter &inShapeFilter);
	static void						sCollideSphereVsHeightField(const Shape *inShape1, const Shape *inShape2, Vec3Arg inScale1, Vec3Arg inScale2, Mat44Arg inCenterOfMassTransform1, Mat44Arg inCenterOfMassTransform2, const SubShapeIDCreator &inSubShapeIDCreator1, const SubShapeIDCreator &inSubShapeIDCreator2, const CollideShapeSettings &inCollideShapeSettings, CollideShapeCollector &ioCollector, const ShapeFilter &inShapeFilter);
	static void						sCastConvexVsHeightField(const ShapeCast &inShapeCast, const ShapeCastSettings &inShapeCastSettings, const Shape *inShape, Vec3Arg inScale, const ShapeFilter &inShapeFilter, Mat44Arg inCenterOfMassTransform2, const SubShapeIDCreator &inSubShapeIDCreator1, const SubShapeIDCreator &inSubShapeIDCreator2, CastShapeCollector &ioCollector);
	static void						sCastSphereVsHeightField(const ShapeCast &inShapeCast, const ShapeCastSettings &inShapeCastSettings, const Shape *inShape, Vec3Arg inScale, const ShapeFilter &inShapeFilter, Mat44Arg inCenterOfMassTransform2, const SubShapeIDCreator &inSubShapeIDCreator1, const SubShapeIDCreator &inSubShapeIDCreator2, CastShapeCollector &ioCollector);

	/// Visit the entire height field using a visitor pattern
	/// Note: Used to be inlined but this triggers a bug in MSVC where it will not free the memory allocated by alloca which causes a stack overflow when WalkHeightField is called in a loop (clang does it correct)
	template <class Visitor>
	void							WalkHeightField(Visitor &ioVisitor) const;

	/// A block of 2x2 ranges used to form a hierarchical grid, ordered left top, right top, left bottom, right bottom
	struct alignas(16) RangeBlock
	{
		uint16						mMin[4];
		uint16						mMax[4];
	};

	/// For block (inBlockX, inBlockY) get the range block and the entry in the range block
	inline void						GetRangeBlock(uint inBlockX, uint inBlockY, uint inRangeBlockOffset, uint inRangeBlockStride, RangeBlock *&outBlock, uint &outIndexInBlock);

	/// Offset of first RangedBlock in grid per level
	static const uint				sGridOffsets[];

	/// The height field is a surface defined by: mOffset + mScale * (x, mHeightSamples[y * mSampleCount + x], y).
	/// where x and y are integers in the range x and y e [0, mSampleCount - 1].
	Vec3							mOffset = Vec3::sZero();
	Vec3							mScale = Vec3::sReplicate(1.0f);

	/// Height data
	uint32							mSampleCount = 0;							///< See HeightFieldShapeSettings::mSampleCount
	uint32							mBlockSize = 2;								///< See HeightFieldShapeSettings::mBlockSize
	uint32							mHeightSamplesSize = 0;						///< Size of mHeightSamples in bytes
	uint32							mRangeBlocksSize = 0;						///< Size of mRangeBlocks in elements
	uint32							mActiveEdgesSize = 0;						///< Size of mActiveEdges in bytes
	uint8							mBitsPerSample = 8;							///< See HeightFieldShapeSettings::mBitsPerSample
	uint8							mSampleMask = 0xff;							///< All bits set for a sample: (1 << mBitsPerSample) - 1, used to indicate that there's no collision
	uint16							mMinSample = HeightFieldShapeConstants::cNoCollisionValue16; ///< Min and max value in mHeightSamples quantized to 16 bit, for calculating bounding box
	uint16							mMaxSample = HeightFieldShapeConstants::cNoCollisionValue16;
	RangeBlock *					mRangeBlocks = nullptr;						///< Hierarchical grid of range data describing the height variations within 1 block. The grid for level <level> starts at offset sGridOffsets[<level>]
	uint8 *							mHeightSamples = nullptr;					///< mBitsPerSample-bit height samples. Value [0, mMaxHeightValue] maps to highest detail grid in mRangeBlocks [mMin, mMax]. mNoCollisionValue is reserved to indicate no collision.
	uint8 *							mActiveEdges = nullptr;						///< (mSampleCount - 1)^2 * 3-bit active edge flags.

	/// Materials
	PhysicsMaterialList				mMaterials;									///< The materials of square at (x, y) is: mMaterials[mMaterialIndices[x + y * (mSampleCount - 1)]]
	Array<uint8>					mMaterialIndices;							///< Compressed to the minimum amount of bits per material index (mSampleCount - 1) * (mSampleCount - 1) * mNumBitsPerMaterialIndex bits of data
	uint32							mNumBitsPerMaterialIndex = 0;				///< Number of bits per material index

#ifdef JPH_DEBUG_RENDERER
	/// Temporary rendering data
	mutable Array<DebugRenderer::GeometryRef> mGeometry;
	mutable bool					mCachedUseMaterialColors = false;			///< This is used to regenerate the triangle batch if the drawing settings change
#endif // JPH_DEBUG_RENDERER
};

JPH_NAMESPACE_END

// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Jolt/Physics/Collision/Shape/CompoundShape.h>

JPH_NAMESPACE_BEGIN

class CollideShapeSettings;

/// Class that constructs a MutableCompoundShape.
class JPH_EXPORT MutableCompoundShapeSettings final : public CompoundShapeSettings
{
	JPH_DECLARE_SERIALIZABLE_VIRTUAL(JPH_EXPORT, MutableCompoundShapeSettings)

public:
	// See: ShapeSettings
	virtual ShapeResult				Create() const override;
};

/// A compound shape, sub shapes can be rotated and translated.
/// This shape is optimized for adding / removing and changing the rotation / translation of sub shapes but is less efficient in querying.
/// Shifts all child objects so that they're centered around the center of mass (which needs to be kept up to date by calling AdjustCenterOfMass).
///
/// Note: If you're using MutableCompoundShape and are querying data while modifying the shape you'll have a race condition.
/// In this case it is best to create a new MutableCompoundShape using the Clone function. You replace the shape on a body using BodyInterface::SetShape.
/// If a query is still working on the old shape, it will have taken a reference and keep the old shape alive until the query finishes.
class JPH_EXPORT MutableCompoundShape final : public CompoundShape
{
public:
	JPH_OVERRIDE_NEW_DELETE

	/// Constructor
									MutableCompoundShape() : CompoundShape(EShapeSubType::MutableCompound) { }
									MutableCompoundShape(const MutableCompoundShapeSettings &inSettings, ShapeResult &outResult);

	/// Clone this shape. Can be used to avoid race conditions. See the documentation of this class for more information.
	Ref<MutableCompoundShape>		Clone() const;

	// See Shape::CastRay
	virtual bool					CastRay(const RayCast &inRay, const SubShapeIDCreator &inSubShapeIDCreator, RayCastResult &ioHit) const override;
	virtual void					CastRay(const RayCast &inRay, const RayCastSettings &inRayCastSettings, const SubShapeIDCreator &inSubShapeIDCreator, CastRayCollector &ioCollector, const ShapeFilter &inShapeFilter = { }) const override;

	// See: Shape::CollidePoint
	virtual void					CollidePoint(Vec3Arg inPoint, const SubShapeIDCreator &inSubShapeIDCreator, CollidePointCollector &ioCollector, const ShapeFilter &inShapeFilter = { }) const override;

	// See Shape::CollectTransformedShapes
	virtual void					CollectTransformedShapes(const AABox &inBox, Vec3Arg inPositionCOM, QuatArg inRotation, Vec3Arg inScale, const SubShapeIDCreator &inSubShapeIDCreator, TransformedShapeCollector &ioCollector, const ShapeFilter &inShapeFilter) const override;

	// See: CompoundShape::GetIntersectingSubShapes
	virtual int						GetIntersectingSubShapes(const AABox &inBox, uint *outSubShapeIndices, int inMaxSubShapeIndices) const override;

	// See: CompoundShape::GetIntersectingSubShapes
	virtual int						GetIntersectingSubShapes(const OrientedBox &inBox, uint *outSubShapeIndices, int inMaxSubShapeIndices) const override;

	// See Shape
	virtual void					SaveBinaryState(StreamOut &inStream) const override;

	// See Shape::GetStats
	virtual Stats					GetStats() const override								{ return Stats(sizeof(*this) + mSubShapes.size() * sizeof(SubShape) + mSubShapeBounds.size() * sizeof(Bounds), 0); }

	///@{
	/// @name Mutating shapes. Note that this is not thread safe, so you need to ensure that any bodies that use this shape are locked at the time of modification using BodyLockWrite. After modification you need to call BodyInterface::NotifyShapeChanged to update the broadphase and collision caches.

	/// Adding a new shape.
	/// Beware this can create a race condition if you're running collision queries in parallel. See class documentation for more information.
	/// @return The index of the newly added shape
	uint							AddShape(Vec3Arg inPosition, QuatArg inRotation, const Shape *inShape, uint32 inUserData = 0);

	/// Remove a shape by index.
	/// Beware this can create a race condition if you're running collision queries in parallel. See class documentation for more information.
	void							RemoveShape(uint inIndex);

	/// Modify the position / orientation of a shape.
	/// Beware this can create a race condition if you're running collision queries in parallel. See class documentation for more information.
	void							ModifyShape(uint inIndex, Vec3Arg inPosition, QuatArg inRotation);

	/// Modify the position / orientation and shape at the same time.
	/// Beware this can create a race condition if you're running collision queries in parallel. See class documentation for more information.
	void							ModifyShape(uint inIndex, Vec3Arg inPosition, QuatArg inRotation, const Shape *inShape);

	/// @brief Batch set positions / orientations, this avoids duplicate work due to bounding box calculation.
	/// Beware this can create a race condition if you're running collision queries in parallel. See class documentation for more information.
	/// @param inStartIndex Index of first shape to update
	/// @param inNumber Number of shapes to update
	/// @param inPositions A list of positions with arbitrary stride
	/// @param inRotations A list of orientations with arbitrary stride
	/// @param inPositionStride The position stride (the number of bytes between the first and second element)
	/// @param inRotationStride The orientation stride (the number of bytes between the first and second element)
	void							ModifyShapes(uint inStartIndex, uint inNumber, const Vec3 *inPositions, const Quat *inRotations, uint inPositionStride = sizeof(Vec3), uint inRotationStride = sizeof(Quat));

	/// Recalculate the center of mass and shift all objects so they're centered around it
	/// (this needs to be done of dynamic bodies and if the center of mass changes significantly due to adding / removing / repositioning sub shapes or else the simulation will look unnatural)
	/// Note that after adjusting the center of mass of an object you need to call BodyInterface::NotifyShapeChanged and Constraint::NotifyShapeChanged on the relevant bodies / constraints.
	/// Beware this can create a race condition if you're running collision queries in parallel. See class documentation for more information.
	void							AdjustCenterOfMass();

	///@}

	// Register shape functions with the registry
	static void						sRegister();

protected:
	// See: Shape::RestoreBinaryState
	virtual void					RestoreBinaryState(StreamIn &inStream) override;

private:
	// Visitor for GetIntersectingSubShapes
	template <class BoxType>
	struct GetIntersectingSubShapesVisitorMC : public GetIntersectingSubShapesVisitor<BoxType>
	{
		using GetIntersectingSubShapesVisitor<BoxType>::GetIntersectingSubShapesVisitor;

		using Result = UVec4;

		JPH_INLINE Result			TestBlock(Vec4Arg inBoundsMinX, Vec4Arg inBoundsMinY, Vec4Arg inBoundsMinZ, Vec4Arg inBoundsMaxX, Vec4Arg inBoundsMaxY, Vec4Arg inBoundsMaxZ) const
		{
			return GetIntersectingSubShapesVisitor<BoxType>::TestBounds(inBoundsMinX, inBoundsMinY, inBoundsMinZ, inBoundsMaxX, inBoundsMaxY, inBoundsMaxZ);
		}

		JPH_INLINE bool				ShouldVisitBlock(UVec4Arg inResult) const
		{
			return inResult.TestAnyTrue();
		}

		JPH_INLINE bool				ShouldVisitSubShape(UVec4Arg inResult, uint inIndexInBlock) const
		{
			return inResult[inIndexInBlock] != 0;
		}
	};

	/// Get the number of blocks of 4 bounding boxes
	inline uint						GetNumBlocks() const										{ return ((uint)mSubShapes.size() + 3) >> 2; }

	/// Ensure that the mSubShapeBounds has enough space to store bounding boxes equivalent to the number of shapes in mSubShapes
	void							EnsureSubShapeBoundsCapacity();

	/// Update mSubShapeBounds
	/// @param inStartIdx First sub shape to update
	/// @param inNumber Number of shapes to update
	void							CalculateSubShapeBounds(uint inStartIdx, uint inNumber);

	/// Calculate mLocalBounds from mSubShapeBounds
	void							CalculateLocalBounds();

	template <class Visitor>
	JPH_INLINE void					WalkSubShapes(Visitor &ioVisitor) const;					///< Walk the sub shapes and call Visitor::VisitShape for each sub shape encountered

	// Helper functions called by CollisionDispatch
	static void						sCollideCompoundVsShape(const Shape *inShape1, const Shape *inShape2, Vec3Arg inScale1, Vec3Arg inScale2, Mat44Arg inCenterOfMassTransform1, Mat44Arg inCenterOfMassTransform2, const SubShapeIDCreator &inSubShapeIDCreator1, const SubShapeIDCreator &inSubShapeIDCreator2, const CollideShapeSettings &inCollideShapeSettings, CollideShapeCollector &ioCollector, const ShapeFilter &inShapeFilter);
	static void						sCollideShapeVsCompound(const Shape *inShape1, const Shape *inShape2, Vec3Arg inScale1, Vec3Arg inScale2, Mat44Arg inCenterOfMassTransform1, Mat44Arg inCenterOfMassTransform2, const SubShapeIDCreator &inSubShapeIDCreator1, const SubShapeIDCreator &inSubShapeIDCreator2, const CollideShapeSettings &inCollideShapeSettings, CollideShapeCollector &ioCollector, const ShapeFilter &inShapeFilter);
	static void						sCastShapeVsCompound(const ShapeCast &inShapeCast, const ShapeCastSettings &inShapeCastSettings, const Shape *inShape, Vec3Arg inScale, const ShapeFilter &inShapeFilter, Mat44Arg inCenterOfMassTransform2, const SubShapeIDCreator &inSubShapeIDCreator1, const SubShapeIDCreator &inSubShapeIDCreator2, CastShapeCollector &ioCollector);

	struct Bounds
	{
		Vec4						mMinX;
		Vec4						mMinY;
		Vec4						mMinZ;
		Vec4						mMaxX;
		Vec4						mMaxY;
		Vec4						mMaxZ;
	};

	Array<Bounds>					mSubShapeBounds;											///< Bounding boxes of all sub shapes in SOA format (in blocks of 4 boxes), MinX 0..3, MinY 0..3, MinZ 0..3, MaxX 0..3, MaxY 0..3, MaxZ 0..3, MinX 4..7, MinY 4..7, ...
};

JPH_NAMESPACE_END

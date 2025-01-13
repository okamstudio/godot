// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Jolt/Physics/Collision/Shape/CompoundShape.h>
#include <Jolt/Physics/Collision/SortReverseAndStore.h>
#include <Jolt/Math/HalfFloat.h>

JPH_NAMESPACE_BEGIN

class CollideShapeSettings;
class TempAllocator;

/// Class that constructs a StaticCompoundShape. Note that if you only want a compound of 1 shape, use a RotatedTranslatedShape instead.
class JPH_EXPORT StaticCompoundShapeSettings final : public CompoundShapeSettings
{
	JPH_DECLARE_SERIALIZABLE_VIRTUAL(JPH_EXPORT, StaticCompoundShapeSettings)

public:
	// See: ShapeSettings
	virtual ShapeResult				Create() const override;

	/// Specialization of Create() function that allows specifying a temp allocator to avoid temporary memory allocations on the heap
	ShapeResult						Create(TempAllocator &inTempAllocator) const;
};

/// A compound shape, sub shapes can be rotated and translated.
/// Sub shapes cannot be modified once the shape is constructed.
/// Shifts all child objects so that they're centered around the center of mass.
class JPH_EXPORT StaticCompoundShape final : public CompoundShape
{
public:
	JPH_OVERRIDE_NEW_DELETE

	/// Constructor
									StaticCompoundShape() : CompoundShape(EShapeSubType::StaticCompound) { }
									StaticCompoundShape(const StaticCompoundShapeSettings &inSettings, TempAllocator &inTempAllocator, ShapeResult &outResult);

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
	virtual Stats					GetStats() const override								{ return Stats(sizeof(*this) + mSubShapes.size() * sizeof(SubShape) + mNodes.size() * sizeof(Node), 0); }

	// Register shape functions with the registry
	static void						sRegister();

protected:
	// See: Shape::RestoreBinaryState
	virtual void					RestoreBinaryState(StreamIn &inStream) override;

private:
	// Visitor for GetIntersectingSubShapes
	template <class BoxType>
	struct GetIntersectingSubShapesVisitorSC : public GetIntersectingSubShapesVisitor<BoxType>
	{
		using GetIntersectingSubShapesVisitor<BoxType>::GetIntersectingSubShapesVisitor;

		JPH_INLINE bool				ShouldVisitNode(int inStackTop) const
		{
			return true;
		}

		JPH_INLINE int				VisitNodes(Vec4Arg inBoundsMinX, Vec4Arg inBoundsMinY, Vec4Arg inBoundsMinZ, Vec4Arg inBoundsMaxX, Vec4Arg inBoundsMaxY, Vec4Arg inBoundsMaxZ, UVec4 &ioProperties, int inStackTop)
		{
			// Test if point overlaps with box
			UVec4 collides = GetIntersectingSubShapesVisitor<BoxType>::TestBounds(inBoundsMinX, inBoundsMinY, inBoundsMinZ, inBoundsMaxX, inBoundsMaxY, inBoundsMaxZ);
			return CountAndSortTrues(collides, ioProperties);
		}
	};

	/// Sorts ioBodyIdx spatially into 2 groups. Second groups starts at ioBodyIdx + outMidPoint.
	/// After the function returns ioBodyIdx and ioBounds will be shuffled
	static void						sPartition(uint *ioBodyIdx, AABox *ioBounds, int inNumber, int &outMidPoint);

	/// Sorts ioBodyIdx from inBegin to (but excluding) inEnd spatially into 4 groups.
	/// outSplit needs to be 5 ints long, when the function returns each group runs from outSplit[i] to (but excluding) outSplit[i + 1]
	/// After the function returns ioBodyIdx and ioBounds will be shuffled
	static void						sPartition4(uint *ioBodyIdx, AABox *ioBounds, int inBegin, int inEnd, int *outSplit);

	// Helper functions called by CollisionDispatch
	static void						sCollideCompoundVsShape(const Shape *inShape1, const Shape *inShape2, Vec3Arg inScale1, Vec3Arg inScale2, Mat44Arg inCenterOfMassTransform1, Mat44Arg inCenterOfMassTransform2, const SubShapeIDCreator &inSubShapeIDCreator1, const SubShapeIDCreator &inSubShapeIDCreator2, const CollideShapeSettings &inCollideShapeSettings, CollideShapeCollector &ioCollector, const ShapeFilter &inShapeFilter);
	static void						sCollideShapeVsCompound(const Shape *inShape1, const Shape *inShape2, Vec3Arg inScale1, Vec3Arg inScale2, Mat44Arg inCenterOfMassTransform1, Mat44Arg inCenterOfMassTransform2, const SubShapeIDCreator &inSubShapeIDCreator1, const SubShapeIDCreator &inSubShapeIDCreator2, const CollideShapeSettings &inCollideShapeSettings, CollideShapeCollector &ioCollector, const ShapeFilter &inShapeFilter);
	static void						sCastShapeVsCompound(const ShapeCast &inShapeCast, const ShapeCastSettings &inShapeCastSettings, const Shape *inShape, Vec3Arg inScale, const ShapeFilter &inShapeFilter, Mat44Arg inCenterOfMassTransform2, const SubShapeIDCreator &inSubShapeIDCreator1, const SubShapeIDCreator &inSubShapeIDCreator2, CastShapeCollector &ioCollector);

	// Maximum size of the stack during tree walk
	static constexpr int			cStackSize = 128;

	template <class Visitor>
	JPH_INLINE void					WalkTree(Visitor &ioVisitor) const;						///< Walk the node tree calling the Visitor::VisitNodes for each node encountered and Visitor::VisitShape for each sub shape encountered

	/// Bits used in Node::mNodeProperties
	enum : uint32
	{
		IS_SUBSHAPE					= 0x80000000,											///< If this bit is set, the other bits index in mSubShape, otherwise in mNodes
		INVALID_NODE				= 0x7fffffff,											///< Signifies an invalid node
	};

	/// Node structure
	struct Node
	{
		void						SetChildBounds(uint inIndex, const AABox &inBounds);	///< Set bounding box for child inIndex to inBounds
		void						SetChildInvalid(uint inIndex);							///< Mark the child inIndex as invalid and set its bounding box to invalid

		HalfFloat					mBoundsMinX[4];											///< 4 child bounding boxes
		HalfFloat					mBoundsMinY[4];
		HalfFloat					mBoundsMinZ[4];
		HalfFloat					mBoundsMaxX[4];
		HalfFloat					mBoundsMaxY[4];
		HalfFloat					mBoundsMaxZ[4];
		uint32						mNodeProperties[4];										///< 4 child node properties
	};

	static_assert(sizeof(Node) == 64, "Node should be 64 bytes");

	using Nodes = Array<Node>;

	Nodes							mNodes;													///< Quad tree node structure
};

JPH_NAMESPACE_END

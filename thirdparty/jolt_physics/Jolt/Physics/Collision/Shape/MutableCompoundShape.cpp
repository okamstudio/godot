// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#include <Jolt/Jolt.h>

#include <Jolt/Physics/Collision/Shape/MutableCompoundShape.h>
#include <Jolt/Physics/Collision/Shape/CompoundShapeVisitors.h>
#include <Jolt/Core/Profiler.h>
#include <Jolt/Core/StreamIn.h>
#include <Jolt/Core/StreamOut.h>
#include <Jolt/ObjectStream/TypeDeclarations.h>

JPH_NAMESPACE_BEGIN

JPH_IMPLEMENT_SERIALIZABLE_VIRTUAL(MutableCompoundShapeSettings)
{
	JPH_ADD_BASE_CLASS(MutableCompoundShapeSettings, CompoundShapeSettings)
}

ShapeSettings::ShapeResult MutableCompoundShapeSettings::Create() const
{
	// Build a mutable compound shape
	if (mCachedResult.IsEmpty())
		Ref<Shape> shape = new MutableCompoundShape(*this, mCachedResult);

	return mCachedResult;
}

MutableCompoundShape::MutableCompoundShape(const MutableCompoundShapeSettings &inSettings, ShapeResult &outResult) :
	CompoundShape(EShapeSubType::MutableCompound, inSettings, outResult)
{
	mSubShapes.reserve(inSettings.mSubShapes.size());
	for (const CompoundShapeSettings::SubShapeSettings &shape : inSettings.mSubShapes)
	{
		// Start constructing the runtime sub shape
		SubShape out_shape;
		if (!out_shape.FromSettings(shape, outResult))
			return;

		mSubShapes.push_back(out_shape);
	}

	AdjustCenterOfMass();

	CalculateSubShapeBounds(0, (uint)mSubShapes.size());

	// Check if we're not exceeding the amount of sub shape id bits
	if (GetSubShapeIDBitsRecursive() > SubShapeID::MaxBits)
	{
		outResult.SetError("Compound hierarchy is too deep and exceeds the amount of available sub shape ID bits");
		return;
	}

	outResult.Set(this);
}

Ref<MutableCompoundShape> MutableCompoundShape::Clone() const
{
	Ref<MutableCompoundShape> clone = new MutableCompoundShape();
	clone->SetUserData(GetUserData());

	clone->mCenterOfMass = mCenterOfMass;
	clone->mLocalBounds = mLocalBounds;
	clone->mSubShapes = mSubShapes;
	clone->mInnerRadius = mInnerRadius;
	clone->mSubShapeBounds = mSubShapeBounds;

	return clone;
}

void MutableCompoundShape::AdjustCenterOfMass()
{
	// First calculate the delta of the center of mass
	float mass = 0.0f;
	Vec3 center_of_mass = Vec3::sZero();
	for (const CompoundShape::SubShape &sub_shape : mSubShapes)
	{
		MassProperties child = sub_shape.mShape->GetMassProperties();
		mass += child.mMass;
		center_of_mass += sub_shape.GetPositionCOM() * child.mMass;
	}
	if (mass > 0.0f)
		center_of_mass /= mass;

	// Now adjust all shapes to recenter around center of mass
	for (CompoundShape::SubShape &sub_shape : mSubShapes)
		sub_shape.SetPositionCOM(sub_shape.GetPositionCOM() - center_of_mass);

	// Update bounding boxes
	for (Bounds &bounds : mSubShapeBounds)
	{
		Vec4 xxxx = center_of_mass.SplatX();
		Vec4 yyyy = center_of_mass.SplatY();
		Vec4 zzzz = center_of_mass.SplatZ();
		bounds.mMinX -= xxxx;
		bounds.mMinY -= yyyy;
		bounds.mMinZ -= zzzz;
		bounds.mMaxX -= xxxx;
		bounds.mMaxY -= yyyy;
		bounds.mMaxZ -= zzzz;
	}
	mLocalBounds.Translate(-center_of_mass);

	// And adjust the center of mass for this shape in the opposite direction
	mCenterOfMass += center_of_mass;
}

void MutableCompoundShape::CalculateLocalBounds()
{
	uint num_blocks = GetNumBlocks();
	if (num_blocks > 0)
	{
		// Initialize min/max for first block
		const Bounds *bounds = mSubShapeBounds.data();
		Vec4 min_x = bounds->mMinX;
		Vec4 min_y = bounds->mMinY;
		Vec4 min_z = bounds->mMinZ;
		Vec4 max_x = bounds->mMaxX;
		Vec4 max_y = bounds->mMaxY;
		Vec4 max_z = bounds->mMaxZ;

		// Accumulate other blocks
		const Bounds *bounds_end = bounds + num_blocks;
		for (++bounds; bounds < bounds_end; ++bounds)
		{
			min_x = Vec4::sMin(min_x, bounds->mMinX);
			min_y = Vec4::sMin(min_y, bounds->mMinY);
			min_z = Vec4::sMin(min_z, bounds->mMinZ);
			max_x = Vec4::sMax(max_x, bounds->mMaxX);
			max_y = Vec4::sMax(max_y, bounds->mMaxY);
			max_z = Vec4::sMax(max_z, bounds->mMaxZ);
		}

		// Calculate resulting bounding box
		mLocalBounds.mMin.SetX(min_x.ReduceMin());
		mLocalBounds.mMin.SetY(min_y.ReduceMin());
		mLocalBounds.mMin.SetZ(min_z.ReduceMin());
		mLocalBounds.mMax.SetX(max_x.ReduceMax());
		mLocalBounds.mMax.SetY(max_y.ReduceMax());
		mLocalBounds.mMax.SetZ(max_z.ReduceMax());
	}
	else
	{
		// There are no subshapes, set the bounding box to invalid
		mLocalBounds.SetEmpty();
	}

	// Cache the inner radius as it can take a while to recursively iterate over all sub shapes
	CalculateInnerRadius();
}

void MutableCompoundShape::EnsureSubShapeBoundsCapacity()
{
	// Check if we have enough space
	uint new_capacity = ((uint)mSubShapes.size() + 3) >> 2;
	if (mSubShapeBounds.size() < new_capacity)
		mSubShapeBounds.resize(new_capacity);
}

void MutableCompoundShape::CalculateSubShapeBounds(uint inStartIdx, uint inNumber)
{
	// Ensure that we have allocated the required space for mSubShapeBounds
	EnsureSubShapeBoundsCapacity();

	// Loop over blocks of 4 sub shapes
	for (uint sub_shape_idx_start = inStartIdx & ~uint(3), sub_shape_idx_end = inStartIdx + inNumber; sub_shape_idx_start < sub_shape_idx_end; sub_shape_idx_start += 4)
	{
		Mat44 bounds_min;
		Mat44 bounds_max;

		AABox sub_shape_bounds;
		for (uint col = 0; col < 4; ++col)
		{
			uint sub_shape_idx = sub_shape_idx_start + col;
			if (sub_shape_idx < mSubShapes.size()) // else reuse sub_shape_bounds from previous iteration
			{
				const SubShape &sub_shape = mSubShapes[sub_shape_idx];

				// Transform the shape's bounds into our local space
				Mat44 transform = Mat44::sRotationTranslation(sub_shape.GetRotation(), sub_shape.GetPositionCOM());

				// Get the bounding box
				sub_shape_bounds = sub_shape.mShape->GetWorldSpaceBounds(transform, Vec3::sReplicate(1.0f));
			}

			// Put the bounds as columns in a matrix
			bounds_min.SetColumn3(col, sub_shape_bounds.mMin);
			bounds_max.SetColumn3(col, sub_shape_bounds.mMax);
		}

		// Transpose to go to structure of arrays format
		Mat44 bounds_min_t = bounds_min.Transposed();
		Mat44 bounds_max_t = bounds_max.Transposed();

		// Store in our bounds array
		Bounds &bounds = mSubShapeBounds[sub_shape_idx_start >> 2];
		bounds.mMinX = bounds_min_t.GetColumn4(0);
		bounds.mMinY = bounds_min_t.GetColumn4(1);
		bounds.mMinZ = bounds_min_t.GetColumn4(2);
		bounds.mMaxX = bounds_max_t.GetColumn4(0);
		bounds.mMaxY = bounds_max_t.GetColumn4(1);
		bounds.mMaxZ = bounds_max_t.GetColumn4(2);
	}

	CalculateLocalBounds();
}

uint MutableCompoundShape::AddShape(Vec3Arg inPosition, QuatArg inRotation, const Shape *inShape, uint32 inUserData)
{
	SubShape sub_shape;
	sub_shape.mShape = inShape;
	sub_shape.mUserData = inUserData;
	sub_shape.SetTransform(inPosition, inRotation, mCenterOfMass);
	mSubShapes.push_back(sub_shape);
	uint shape_idx = (uint)mSubShapes.size() - 1;

	CalculateSubShapeBounds(shape_idx, 1);

	return shape_idx;
}

void MutableCompoundShape::RemoveShape(uint inIndex)
{
	mSubShapes.erase(mSubShapes.begin() + inIndex);

	uint num_bounds = (uint)mSubShapes.size() - inIndex;
	if (num_bounds > 0)
		CalculateSubShapeBounds(inIndex, num_bounds);
	else
		CalculateLocalBounds();
}

void MutableCompoundShape::ModifyShape(uint inIndex, Vec3Arg inPosition, QuatArg inRotation)
{
	SubShape &sub_shape = mSubShapes[inIndex];
	sub_shape.SetTransform(inPosition, inRotation, mCenterOfMass);

	CalculateSubShapeBounds(inIndex, 1);
}

void MutableCompoundShape::ModifyShape(uint inIndex, Vec3Arg inPosition, QuatArg inRotation, const Shape *inShape)
{
	SubShape &sub_shape = mSubShapes[inIndex];
	sub_shape.mShape = inShape;
	sub_shape.SetTransform(inPosition, inRotation, mCenterOfMass);

	CalculateSubShapeBounds(inIndex, 1);
}

void MutableCompoundShape::ModifyShapes(uint inStartIndex, uint inNumber, const Vec3 *inPositions, const Quat *inRotations, uint inPositionStride, uint inRotationStride)
{
	JPH_ASSERT(inStartIndex + inNumber <= mSubShapes.size());

	const Vec3 *pos = inPositions;
	const Quat *rot = inRotations;
	for (SubShape *dest = &mSubShapes[inStartIndex], *dest_end = dest + inNumber; dest < dest_end; ++dest)
	{
		// Update transform
		dest->SetTransform(*pos, *rot, mCenterOfMass);

		// Advance pointer in position / rotation buffer
		pos = reinterpret_cast<const Vec3 *>(reinterpret_cast<const uint8 *>(pos) + inPositionStride);
		rot = reinterpret_cast<const Quat *>(reinterpret_cast<const uint8 *>(rot) + inRotationStride);
	}

	CalculateSubShapeBounds(inStartIndex, inNumber);
}

template <class Visitor>
inline void MutableCompoundShape::WalkSubShapes(Visitor &ioVisitor) const
{
	// Loop over all blocks of 4 bounding boxes
	for (uint block = 0, num_blocks = GetNumBlocks(); block < num_blocks; ++block)
	{
		// Test the bounding boxes
		const Bounds &bounds = mSubShapeBounds[block];
		typename Visitor::Result result = ioVisitor.TestBlock(bounds.mMinX, bounds.mMinY, bounds.mMinZ, bounds.mMaxX, bounds.mMaxY, bounds.mMaxZ);

		// Check if any of the bounding boxes collided
		if (ioVisitor.ShouldVisitBlock(result))
		{
			// Go through the individual boxes
			uint sub_shape_start_idx = block << 2;
			for (uint col = 0, max_col = min<uint>(4, (uint)mSubShapes.size() - sub_shape_start_idx); col < max_col; ++col) // Don't read beyond the end of the subshapes array
				if (ioVisitor.ShouldVisitSubShape(result, col)) // Because the early out fraction can change, we need to retest every shape
				{
					// Test sub shape
					uint sub_shape_idx = sub_shape_start_idx + col;
					const SubShape &sub_shape = mSubShapes[sub_shape_idx];
					ioVisitor.VisitShape(sub_shape, sub_shape_idx);

					// If no better collision is available abort
					if (ioVisitor.ShouldAbort())
						break;
				}
		}
	}
}

bool MutableCompoundShape::CastRay(const RayCast &inRay, const SubShapeIDCreator &inSubShapeIDCreator, RayCastResult &ioHit) const
{
	JPH_PROFILE_FUNCTION();

	struct Visitor : public CastRayVisitor
	{
		using CastRayVisitor::CastRayVisitor;

		using Result = Vec4;

		JPH_INLINE Result	TestBlock(Vec4Arg inBoundsMinX, Vec4Arg inBoundsMinY, Vec4Arg inBoundsMinZ, Vec4Arg inBoundsMaxX, Vec4Arg inBoundsMaxY, Vec4Arg inBoundsMaxZ) const
		{
			return TestBounds(inBoundsMinX, inBoundsMinY, inBoundsMinZ, inBoundsMaxX, inBoundsMaxY, inBoundsMaxZ);
		}

		JPH_INLINE bool		ShouldVisitBlock(Vec4Arg inResult) const
		{
			UVec4 closer = Vec4::sLess(inResult, Vec4::sReplicate(mHit.mFraction));
			return closer.TestAnyTrue();
		}

		JPH_INLINE bool		ShouldVisitSubShape(Vec4Arg inResult, uint inIndexInBlock) const
		{
			return inResult[inIndexInBlock] < mHit.mFraction;
		}
	};

	Visitor visitor(inRay, this, inSubShapeIDCreator, ioHit);
	WalkSubShapes(visitor);
	return visitor.mReturnValue;
}

void MutableCompoundShape::CastRay(const RayCast &inRay, const RayCastSettings &inRayCastSettings, const SubShapeIDCreator &inSubShapeIDCreator, CastRayCollector &ioCollector, const ShapeFilter &inShapeFilter) const
{
	JPH_PROFILE_FUNCTION();

	// Test shape filter
	if (!inShapeFilter.ShouldCollide(this, inSubShapeIDCreator.GetID()))
		return;

	struct Visitor : public CastRayVisitorCollector
	{
		using CastRayVisitorCollector::CastRayVisitorCollector;

		using Result = Vec4;

		JPH_INLINE Result	TestBlock(Vec4Arg inBoundsMinX, Vec4Arg inBoundsMinY, Vec4Arg inBoundsMinZ, Vec4Arg inBoundsMaxX, Vec4Arg inBoundsMaxY, Vec4Arg inBoundsMaxZ) const
		{
			return TestBounds(inBoundsMinX, inBoundsMinY, inBoundsMinZ, inBoundsMaxX, inBoundsMaxY, inBoundsMaxZ);
		}

		JPH_INLINE bool		ShouldVisitBlock(Vec4Arg inResult) const
		{
			UVec4 closer = Vec4::sLess(inResult, Vec4::sReplicate(mCollector.GetEarlyOutFraction()));
			return closer.TestAnyTrue();
		}

		JPH_INLINE bool		ShouldVisitSubShape(Vec4Arg inResult, uint inIndexInBlock) const
		{
			return inResult[inIndexInBlock] < mCollector.GetEarlyOutFraction();
		}
	};

	Visitor visitor(inRay, inRayCastSettings, this, inSubShapeIDCreator, ioCollector, inShapeFilter);
	WalkSubShapes(visitor);
}

void MutableCompoundShape::CollidePoint(Vec3Arg inPoint, const SubShapeIDCreator &inSubShapeIDCreator, CollidePointCollector &ioCollector, const ShapeFilter &inShapeFilter) const
{
	JPH_PROFILE_FUNCTION();

	// Test shape filter
	if (!inShapeFilter.ShouldCollide(this, inSubShapeIDCreator.GetID()))
		return;

	struct Visitor : public CollidePointVisitor
	{
		using CollidePointVisitor::CollidePointVisitor;

		using Result = UVec4;

		JPH_INLINE Result	TestBlock(Vec4Arg inBoundsMinX, Vec4Arg inBoundsMinY, Vec4Arg inBoundsMinZ, Vec4Arg inBoundsMaxX, Vec4Arg inBoundsMaxY, Vec4Arg inBoundsMaxZ) const
		{
			return TestBounds(inBoundsMinX, inBoundsMinY, inBoundsMinZ, inBoundsMaxX, inBoundsMaxY, inBoundsMaxZ);
		}

		JPH_INLINE bool		ShouldVisitBlock(UVec4Arg inResult) const
		{
			return inResult.TestAnyTrue();
		}

		JPH_INLINE bool		ShouldVisitSubShape(UVec4Arg inResult, uint inIndexInBlock) const
		{
			return inResult[inIndexInBlock] != 0;
		}
	};

	Visitor visitor(inPoint, this, inSubShapeIDCreator, ioCollector, inShapeFilter);
	WalkSubShapes(visitor);
}

void MutableCompoundShape::sCastShapeVsCompound(const ShapeCast &inShapeCast, const ShapeCastSettings &inShapeCastSettings, const Shape *inShape, Vec3Arg inScale, const ShapeFilter &inShapeFilter, Mat44Arg inCenterOfMassTransform2, const SubShapeIDCreator &inSubShapeIDCreator1, const SubShapeIDCreator &inSubShapeIDCreator2, CastShapeCollector &ioCollector)
{
	JPH_PROFILE_FUNCTION();

	struct Visitor : public CastShapeVisitor
	{
		using CastShapeVisitor::CastShapeVisitor;

		using Result = Vec4;

		JPH_INLINE Result	TestBlock(Vec4Arg inBoundsMinX, Vec4Arg inBoundsMinY, Vec4Arg inBoundsMinZ, Vec4Arg inBoundsMaxX, Vec4Arg inBoundsMaxY, Vec4Arg inBoundsMaxZ) const
		{
			return TestBounds(inBoundsMinX, inBoundsMinY, inBoundsMinZ, inBoundsMaxX, inBoundsMaxY, inBoundsMaxZ);
		}

		JPH_INLINE bool		ShouldVisitBlock(Vec4Arg inResult) const
		{
			UVec4 closer = Vec4::sLess(inResult, Vec4::sReplicate(mCollector.GetPositiveEarlyOutFraction()));
			return closer.TestAnyTrue();
		}

		JPH_INLINE bool		ShouldVisitSubShape(Vec4Arg inResult, uint inIndexInBlock) const
		{
			return inResult[inIndexInBlock] < mCollector.GetPositiveEarlyOutFraction();
		}
	};

	JPH_ASSERT(inShape->GetSubType() == EShapeSubType::MutableCompound);
	const MutableCompoundShape *shape = static_cast<const MutableCompoundShape *>(inShape);

	Visitor visitor(inShapeCast, inShapeCastSettings, shape, inScale, inShapeFilter, inCenterOfMassTransform2, inSubShapeIDCreator1, inSubShapeIDCreator2, ioCollector);
	shape->WalkSubShapes(visitor);
}

void MutableCompoundShape::CollectTransformedShapes(const AABox &inBox, Vec3Arg inPositionCOM, QuatArg inRotation, Vec3Arg inScale, const SubShapeIDCreator &inSubShapeIDCreator, TransformedShapeCollector &ioCollector, const ShapeFilter &inShapeFilter) const
{
	JPH_PROFILE_FUNCTION();

	// Test shape filter
	if (!inShapeFilter.ShouldCollide(this, inSubShapeIDCreator.GetID()))
		return;

	struct Visitor : public CollectTransformedShapesVisitor
	{
		using CollectTransformedShapesVisitor::CollectTransformedShapesVisitor;

		using Result = UVec4;

		JPH_INLINE Result	TestBlock(Vec4Arg inBoundsMinX, Vec4Arg inBoundsMinY, Vec4Arg inBoundsMinZ, Vec4Arg inBoundsMaxX, Vec4Arg inBoundsMaxY, Vec4Arg inBoundsMaxZ) const
		{
			return TestBounds(inBoundsMinX, inBoundsMinY, inBoundsMinZ, inBoundsMaxX, inBoundsMaxY, inBoundsMaxZ);
		}

		JPH_INLINE bool		ShouldVisitBlock(UVec4Arg inResult) const
		{
			return inResult.TestAnyTrue();
		}

		JPH_INLINE bool		ShouldVisitSubShape(UVec4Arg inResult, uint inIndexInBlock) const
		{
			return inResult[inIndexInBlock] != 0;
		}
	};

	Visitor visitor(inBox, this, inPositionCOM, inRotation, inScale, inSubShapeIDCreator, ioCollector, inShapeFilter);
	WalkSubShapes(visitor);
}

int MutableCompoundShape::GetIntersectingSubShapes(const AABox &inBox, uint *outSubShapeIndices, int inMaxSubShapeIndices) const
{
	JPH_PROFILE_FUNCTION();

	GetIntersectingSubShapesVisitorMC<AABox> visitor(inBox, outSubShapeIndices, inMaxSubShapeIndices);
	WalkSubShapes(visitor);
	return visitor.GetNumResults();
}

int MutableCompoundShape::GetIntersectingSubShapes(const OrientedBox &inBox, uint *outSubShapeIndices, int inMaxSubShapeIndices) const
{
	JPH_PROFILE_FUNCTION();

	GetIntersectingSubShapesVisitorMC<OrientedBox> visitor(inBox, outSubShapeIndices, inMaxSubShapeIndices);
	WalkSubShapes(visitor);
	return visitor.GetNumResults();
}

void MutableCompoundShape::sCollideCompoundVsShape(const Shape *inShape1, const Shape *inShape2, Vec3Arg inScale1, Vec3Arg inScale2, Mat44Arg inCenterOfMassTransform1, Mat44Arg inCenterOfMassTransform2, const SubShapeIDCreator &inSubShapeIDCreator1, const SubShapeIDCreator &inSubShapeIDCreator2, const CollideShapeSettings &inCollideShapeSettings, CollideShapeCollector &ioCollector, const ShapeFilter &inShapeFilter)
{
	JPH_PROFILE_FUNCTION();

	JPH_ASSERT(inShape1->GetSubType() == EShapeSubType::MutableCompound);
	const MutableCompoundShape *shape1 = static_cast<const MutableCompoundShape *>(inShape1);

	struct Visitor : public CollideCompoundVsShapeVisitor
	{
		using CollideCompoundVsShapeVisitor::CollideCompoundVsShapeVisitor;

		using Result = UVec4;

		JPH_INLINE Result	TestBlock(Vec4Arg inBoundsMinX, Vec4Arg inBoundsMinY, Vec4Arg inBoundsMinZ, Vec4Arg inBoundsMaxX, Vec4Arg inBoundsMaxY, Vec4Arg inBoundsMaxZ) const
		{
			return TestBounds(inBoundsMinX, inBoundsMinY, inBoundsMinZ, inBoundsMaxX, inBoundsMaxY, inBoundsMaxZ);
		}

		JPH_INLINE bool		ShouldVisitBlock(UVec4Arg inResult) const
		{
			return inResult.TestAnyTrue();
		}

		JPH_INLINE bool		ShouldVisitSubShape(UVec4Arg inResult, uint inIndexInBlock) const
		{
			return inResult[inIndexInBlock] != 0;
		}
	};

	Visitor visitor(shape1, inShape2, inScale1, inScale2, inCenterOfMassTransform1, inCenterOfMassTransform2, inSubShapeIDCreator1, inSubShapeIDCreator2, inCollideShapeSettings, ioCollector, inShapeFilter);
	shape1->WalkSubShapes(visitor);
}

void MutableCompoundShape::sCollideShapeVsCompound(const Shape *inShape1, const Shape *inShape2, Vec3Arg inScale1, Vec3Arg inScale2, Mat44Arg inCenterOfMassTransform1, Mat44Arg inCenterOfMassTransform2, const SubShapeIDCreator &inSubShapeIDCreator1, const SubShapeIDCreator &inSubShapeIDCreator2, const CollideShapeSettings &inCollideShapeSettings, CollideShapeCollector &ioCollector, const ShapeFilter &inShapeFilter)
{
	JPH_PROFILE_FUNCTION();

	JPH_ASSERT(inShape2->GetSubType() == EShapeSubType::MutableCompound);
	const MutableCompoundShape *shape2 = static_cast<const MutableCompoundShape *>(inShape2);

	struct Visitor : public CollideShapeVsCompoundVisitor
	{
		using CollideShapeVsCompoundVisitor::CollideShapeVsCompoundVisitor;

		using Result = UVec4;

		JPH_INLINE Result	TestBlock(Vec4Arg inBoundsMinX, Vec4Arg inBoundsMinY, Vec4Arg inBoundsMinZ, Vec4Arg inBoundsMaxX, Vec4Arg inBoundsMaxY, Vec4Arg inBoundsMaxZ) const
		{
			return TestBounds(inBoundsMinX, inBoundsMinY, inBoundsMinZ, inBoundsMaxX, inBoundsMaxY, inBoundsMaxZ);
		}

		JPH_INLINE bool		ShouldVisitBlock(UVec4Arg inResult) const
		{
			return inResult.TestAnyTrue();
		}

		JPH_INLINE bool		ShouldVisitSubShape(UVec4Arg inResult, uint inIndexInBlock) const
		{
			return inResult[inIndexInBlock] != 0;
		}
	};

	Visitor visitor(inShape1, shape2, inScale1, inScale2, inCenterOfMassTransform1, inCenterOfMassTransform2, inSubShapeIDCreator1, inSubShapeIDCreator2, inCollideShapeSettings, ioCollector, inShapeFilter);
	shape2->WalkSubShapes(visitor);
}

void MutableCompoundShape::SaveBinaryState(StreamOut &inStream) const
{
	CompoundShape::SaveBinaryState(inStream);

	// Write bounds
	uint bounds_size = (((uint)mSubShapes.size() + 3) >> 2) * sizeof(Bounds);
	inStream.WriteBytes(mSubShapeBounds.data(), bounds_size);
}

void MutableCompoundShape::RestoreBinaryState(StreamIn &inStream)
{
	CompoundShape::RestoreBinaryState(inStream);

	// Ensure that we have allocated the required space for mSubShapeBounds
	EnsureSubShapeBoundsCapacity();

	// Read bounds
	uint bounds_size = (((uint)mSubShapes.size() + 3) >> 2) * sizeof(Bounds);
	inStream.ReadBytes(mSubShapeBounds.data(), bounds_size);
}

void MutableCompoundShape::sRegister()
{
	ShapeFunctions &f = ShapeFunctions::sGet(EShapeSubType::MutableCompound);
	f.mConstruct = []() -> Shape * { return new MutableCompoundShape; };
	f.mColor = Color::sDarkOrange;

	for (EShapeSubType s : sAllSubShapeTypes)
	{
		CollisionDispatch::sRegisterCollideShape(EShapeSubType::MutableCompound, s, sCollideCompoundVsShape);
		CollisionDispatch::sRegisterCollideShape(s, EShapeSubType::MutableCompound, sCollideShapeVsCompound);
		CollisionDispatch::sRegisterCastShape(s, EShapeSubType::MutableCompound, sCastShapeVsCompound);
	}
}

JPH_NAMESPACE_END

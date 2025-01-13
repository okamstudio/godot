// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Jolt/Physics/Collision/Shape/CompoundShape.h>
#include <Jolt/Physics/Collision/Shape/SubShapeID.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/ShapeCast.h>
#include <Jolt/Physics/Collision/TransformedShape.h>
#include <Jolt/Physics/Collision/CollisionDispatch.h>
#include <Jolt/Geometry/RayAABox.h>
#include <Jolt/Geometry/AABox4.h>
#include <Jolt/Geometry/OrientedBox.h>

JPH_NAMESPACE_BEGIN

struct CompoundShape::CastRayVisitor
{
	JPH_INLINE			CastRayVisitor(const RayCast &inRay, const CompoundShape *inShape, const SubShapeIDCreator &inSubShapeIDCreator, RayCastResult &ioHit) :
		mRay(inRay),
		mHit(ioHit),
		mSubShapeIDCreator(inSubShapeIDCreator),
		mSubShapeBits(inShape->GetSubShapeIDBits())
	{
		// Determine ray properties of cast
		mInvDirection.Set(inRay.mDirection);
	}

	/// Returns true when collision detection should abort because it's not possible to find a better hit
	JPH_INLINE bool		ShouldAbort() const
	{
		return mHit.mFraction <= 0.0f;
	}

	/// Test ray against 4 bounding boxes and returns the distance where the ray enters the bounding box
	JPH_INLINE Vec4		TestBounds(Vec4Arg inBoundsMinX, Vec4Arg inBoundsMinY, Vec4Arg inBoundsMinZ, Vec4Arg inBoundsMaxX, Vec4Arg inBoundsMaxY, Vec4Arg inBoundsMaxZ) const
	{
		return RayAABox4(mRay.mOrigin, mInvDirection, inBoundsMinX, inBoundsMinY, inBoundsMinZ, inBoundsMaxX, inBoundsMaxY, inBoundsMaxZ);
	}

	/// Test the ray against a single subshape
	JPH_INLINE void		VisitShape(const SubShape &inSubShape, uint32 inSubShapeIndex)
	{
		// Create ID for sub shape
		SubShapeIDCreator shape2_sub_shape_id = mSubShapeIDCreator.PushID(inSubShapeIndex, mSubShapeBits);

		// Transform the ray
		Mat44 transform = Mat44::sInverseRotationTranslation(inSubShape.GetRotation(), inSubShape.GetPositionCOM());
		RayCast ray = mRay.Transformed(transform);
		if (inSubShape.mShape->CastRay(ray, shape2_sub_shape_id, mHit))
			mReturnValue = true;
	}

	RayInvDirection		mInvDirection;
	const RayCast &		mRay;
	RayCastResult &		mHit;
	SubShapeIDCreator	mSubShapeIDCreator;
	uint				mSubShapeBits;
	bool				mReturnValue = false;
};

struct CompoundShape::CastRayVisitorCollector
{
	JPH_INLINE			CastRayVisitorCollector(const RayCast &inRay, const RayCastSettings &inRayCastSettings, const CompoundShape *inShape, const SubShapeIDCreator &inSubShapeIDCreator, CastRayCollector &ioCollector, const ShapeFilter &inShapeFilter) :
		mRay(inRay),
		mCollector(ioCollector),
		mSubShapeIDCreator(inSubShapeIDCreator),
		mSubShapeBits(inShape->GetSubShapeIDBits()),
		mRayCastSettings(inRayCastSettings),
		mShapeFilter(inShapeFilter)
	{
		// Determine ray properties of cast
		mInvDirection.Set(inRay.mDirection);
	}

	/// Returns true when collision detection should abort because it's not possible to find a better hit
	JPH_INLINE bool		ShouldAbort() const
	{
		return mCollector.ShouldEarlyOut();
	}

	/// Test ray against 4 bounding boxes and returns the distance where the ray enters the bounding box
	JPH_INLINE Vec4		TestBounds(Vec4Arg inBoundsMinX, Vec4Arg inBoundsMinY, Vec4Arg inBoundsMinZ, Vec4Arg inBoundsMaxX, Vec4Arg inBoundsMaxY, Vec4Arg inBoundsMaxZ) const
	{
		return RayAABox4(mRay.mOrigin, mInvDirection, inBoundsMinX, inBoundsMinY, inBoundsMinZ, inBoundsMaxX, inBoundsMaxY, inBoundsMaxZ);
	}

	/// Test the ray against a single subshape
	JPH_INLINE void		VisitShape(const SubShape &inSubShape, uint32 inSubShapeIndex)
	{
		// Create ID for sub shape
		SubShapeIDCreator shape2_sub_shape_id = mSubShapeIDCreator.PushID(inSubShapeIndex, mSubShapeBits);

		// Transform the ray
		Mat44 transform = Mat44::sInverseRotationTranslation(inSubShape.GetRotation(), inSubShape.GetPositionCOM());
		RayCast ray = mRay.Transformed(transform);
		inSubShape.mShape->CastRay(ray, mRayCastSettings, shape2_sub_shape_id, mCollector, mShapeFilter);
	}

	RayInvDirection		mInvDirection;
	const RayCast &		mRay;
	CastRayCollector &	mCollector;
	SubShapeIDCreator	mSubShapeIDCreator;
	uint				mSubShapeBits;
	RayCastSettings		mRayCastSettings;
	const ShapeFilter &	mShapeFilter;
};

struct CompoundShape::CollidePointVisitor
{
	JPH_INLINE			CollidePointVisitor(Vec3Arg inPoint, const CompoundShape *inShape, const SubShapeIDCreator &inSubShapeIDCreator, CollidePointCollector &ioCollector, const ShapeFilter &inShapeFilter) :
		mPoint(inPoint),
		mSubShapeIDCreator(inSubShapeIDCreator),
		mCollector(ioCollector),
		mSubShapeBits(inShape->GetSubShapeIDBits()),
		mShapeFilter(inShapeFilter)
	{
	}

	/// Returns true when collision detection should abort because it's not possible to find a better hit
	JPH_INLINE bool		ShouldAbort() const
	{
		return mCollector.ShouldEarlyOut();
	}

	/// Test if point overlaps with 4 boxes, returns true for the ones that do
	JPH_INLINE UVec4	TestBounds(Vec4Arg inBoundsMinX, Vec4Arg inBoundsMinY, Vec4Arg inBoundsMinZ, Vec4Arg inBoundsMaxX, Vec4Arg inBoundsMaxY, Vec4Arg inBoundsMaxZ) const
	{
		return AABox4VsPoint(mPoint, inBoundsMinX, inBoundsMinY, inBoundsMinZ, inBoundsMaxX, inBoundsMaxY, inBoundsMaxZ);
	}

	/// Test the point against a single subshape
	JPH_INLINE void		VisitShape(const SubShape &inSubShape, uint32 inSubShapeIndex)
	{
		// Create ID for sub shape
		SubShapeIDCreator shape2_sub_shape_id = mSubShapeIDCreator.PushID(inSubShapeIndex, mSubShapeBits);

		// Transform the point
		Mat44 transform = Mat44::sInverseRotationTranslation(inSubShape.GetRotation(), inSubShape.GetPositionCOM());
		inSubShape.mShape->CollidePoint(transform * mPoint, shape2_sub_shape_id, mCollector, mShapeFilter);
	}

	Vec3						mPoint;
	SubShapeIDCreator			mSubShapeIDCreator;
	CollidePointCollector &		mCollector;
	uint						mSubShapeBits;
	const ShapeFilter &			mShapeFilter;
};

struct CompoundShape::CastShapeVisitor
{
	JPH_INLINE			CastShapeVisitor(const ShapeCast &inShapeCast, const ShapeCastSettings &inShapeCastSettings, const CompoundShape *inShape, Vec3Arg inScale, const ShapeFilter &inShapeFilter, Mat44Arg inCenterOfMassTransform2, const SubShapeIDCreator &inSubShapeIDCreator1, const SubShapeIDCreator &inSubShapeIDCreator2, CastShapeCollector &ioCollector) :
		mBoxCenter(inShapeCast.mShapeWorldBounds.GetCenter()),
		mBoxExtent(inShapeCast.mShapeWorldBounds.GetExtent()),
		mScale(inScale),
		mShapeCast(inShapeCast),
		mShapeCastSettings(inShapeCastSettings),
		mShapeFilter(inShapeFilter),
		mCollector(ioCollector),
		mCenterOfMassTransform2(inCenterOfMassTransform2),
		mSubShapeIDCreator1(inSubShapeIDCreator1),
		mSubShapeIDCreator2(inSubShapeIDCreator2),
		mSubShapeBits(inShape->GetSubShapeIDBits())
	{
		// Determine ray properties of cast
		mInvDirection.Set(inShapeCast.mDirection);
	}

	/// Returns true when collision detection should abort because it's not possible to find a better hit
	JPH_INLINE bool		ShouldAbort() const
	{
		return mCollector.ShouldEarlyOut();
	}

	/// Tests the shape cast against 4 bounding boxes, returns the distance along the shape cast where the shape first enters the bounding box
	JPH_INLINE Vec4		TestBounds(Vec4Arg inBoundsMinX, Vec4Arg inBoundsMinY, Vec4Arg inBoundsMinZ, Vec4Arg inBoundsMaxX, Vec4Arg inBoundsMaxY, Vec4Arg inBoundsMaxZ) const
	{
		// Scale the bounding boxes
		Vec4 bounds_min_x, bounds_min_y, bounds_min_z, bounds_max_x, bounds_max_y, bounds_max_z;
		AABox4Scale(mScale, inBoundsMinX, inBoundsMinY, inBoundsMinZ, inBoundsMaxX, inBoundsMaxY, inBoundsMaxZ, bounds_min_x, bounds_min_y, bounds_min_z, bounds_max_x, bounds_max_y, bounds_max_z);

		// Enlarge them by the casted shape's box extents
		AABox4EnlargeWithExtent(mBoxExtent, bounds_min_x, bounds_min_y, bounds_min_z, bounds_max_x, bounds_max_y, bounds_max_z);

		// Test ray against the bounding boxes
		return RayAABox4(mBoxCenter, mInvDirection, bounds_min_x, bounds_min_y, bounds_min_z, bounds_max_x, bounds_max_y, bounds_max_z);
	}

	/// Test the cast shape against a single subshape
	JPH_INLINE void		VisitShape(const SubShape &inSubShape, uint32 inSubShapeIndex)
	{
		JPH_ASSERT(inSubShape.IsValidScale(mScale));

		// Create ID for sub shape
		SubShapeIDCreator shape2_sub_shape_id = mSubShapeIDCreator2.PushID(inSubShapeIndex, mSubShapeBits);

		// Calculate the local transform for this sub shape
		Mat44 local_transform = Mat44::sRotationTranslation(inSubShape.GetRotation(), mScale * inSubShape.GetPositionCOM());

		// Transform the center of mass of 2
		Mat44 center_of_mass_transform2 = mCenterOfMassTransform2 * local_transform;

		// Transform the shape cast
		ShapeCast shape_cast = mShapeCast.PostTransformed(local_transform.InversedRotationTranslation());

		CollisionDispatch::sCastShapeVsShapeLocalSpace(shape_cast, mShapeCastSettings, inSubShape.mShape, inSubShape.TransformScale(mScale), mShapeFilter, center_of_mass_transform2, mSubShapeIDCreator1, shape2_sub_shape_id, mCollector);
	}

	RayInvDirection				mInvDirection;
	Vec3						mBoxCenter;
	Vec3						mBoxExtent;
	Vec3						mScale;
	const ShapeCast &			mShapeCast;
	const ShapeCastSettings &	mShapeCastSettings;
	const ShapeFilter &			mShapeFilter;
	CastShapeCollector &		mCollector;
	Mat44						mCenterOfMassTransform2;
	SubShapeIDCreator			mSubShapeIDCreator1;
	SubShapeIDCreator			mSubShapeIDCreator2;
	uint						mSubShapeBits;
};

struct CompoundShape::CollectTransformedShapesVisitor
{
	JPH_INLINE			CollectTransformedShapesVisitor(const AABox &inBox, const CompoundShape *inShape, Vec3Arg inPositionCOM, QuatArg inRotation, Vec3Arg inScale, const SubShapeIDCreator &inSubShapeIDCreator, TransformedShapeCollector &ioCollector, const ShapeFilter &inShapeFilter) :
		mBox(inBox),
		mLocalBox(Mat44::sInverseRotationTranslation(inRotation, inPositionCOM), inBox),
		mPositionCOM(inPositionCOM),
		mRotation(inRotation),
		mScale(inScale),
		mSubShapeIDCreator(inSubShapeIDCreator),
		mCollector(ioCollector),
		mSubShapeBits(inShape->GetSubShapeIDBits()),
		mShapeFilter(inShapeFilter)
	{
	}

	/// Returns true when collision detection should abort because it's not possible to find a better hit
	JPH_INLINE bool		ShouldAbort() const
	{
		return mCollector.ShouldEarlyOut();
	}

	/// Tests 4 bounding boxes against the query box, returns true for the ones that collide
	JPH_INLINE UVec4	TestBounds(Vec4Arg inBoundsMinX, Vec4Arg inBoundsMinY, Vec4Arg inBoundsMinZ, Vec4Arg inBoundsMaxX, Vec4Arg inBoundsMaxY, Vec4Arg inBoundsMaxZ) const
	{
		// Scale the bounding boxes of this node
		Vec4 bounds_min_x, bounds_min_y, bounds_min_z, bounds_max_x, bounds_max_y, bounds_max_z;
		AABox4Scale(mScale, inBoundsMinX, inBoundsMinY, inBoundsMinZ, inBoundsMaxX, inBoundsMaxY, inBoundsMaxZ, bounds_min_x, bounds_min_y, bounds_min_z, bounds_max_x, bounds_max_y, bounds_max_z);

		// Test which nodes collide
		return AABox4VsBox(mLocalBox, bounds_min_x, bounds_min_y, bounds_min_z, bounds_max_x, bounds_max_y, bounds_max_z);
	}

	/// Collect the transformed sub shapes for a single subshape
	JPH_INLINE void		VisitShape(const SubShape &inSubShape, uint32 inSubShapeIndex)
	{
		JPH_ASSERT(inSubShape.IsValidScale(mScale));

		// Create ID for sub shape
		SubShapeIDCreator sub_shape_id = mSubShapeIDCreator.PushID(inSubShapeIndex, mSubShapeBits);

		// Calculate world transform for sub shape
		Vec3 position = mPositionCOM + mRotation * (mScale * inSubShape.GetPositionCOM());
		Quat rotation = mRotation * inSubShape.GetRotation();

		// Recurse to sub shape
		inSubShape.mShape->CollectTransformedShapes(mBox, position, rotation, inSubShape.TransformScale(mScale), sub_shape_id, mCollector, mShapeFilter);
	}

	AABox							mBox;
	OrientedBox						mLocalBox;
	Vec3							mPositionCOM;
	Quat							mRotation;
	Vec3							mScale;
	SubShapeIDCreator				mSubShapeIDCreator;
	TransformedShapeCollector &		mCollector;
	uint							mSubShapeBits;
	const ShapeFilter &				mShapeFilter;
};

struct CompoundShape::CollideCompoundVsShapeVisitor
{
	JPH_INLINE			CollideCompoundVsShapeVisitor(const CompoundShape *inShape1, const Shape *inShape2, Vec3Arg inScale1, Vec3Arg inScale2, Mat44Arg inCenterOfMassTransform1, Mat44Arg inCenterOfMassTransform2, const SubShapeIDCreator &inSubShapeIDCreator1, const SubShapeIDCreator &inSubShapeIDCreator2, const CollideShapeSettings &inCollideShapeSettings, CollideShapeCollector &ioCollector, const ShapeFilter &inShapeFilter) :
		mCollideShapeSettings(inCollideShapeSettings),
		mCollector(ioCollector),
		mShape2(inShape2),
		mScale1(inScale1),
		mScale2(inScale2),
		mTransform1(inCenterOfMassTransform1),
		mTransform2(inCenterOfMassTransform2),
		mSubShapeIDCreator1(inSubShapeIDCreator1),
		mSubShapeIDCreator2(inSubShapeIDCreator2),
		mSubShapeBits(inShape1->GetSubShapeIDBits()),
		mShapeFilter(inShapeFilter)
	{
		// Get transform from shape 2 to shape 1
		Mat44 transform2_to_1 = inCenterOfMassTransform1.InversedRotationTranslation() * inCenterOfMassTransform2;

		// Convert bounding box of 2 into space of 1
		mBoundsOf2InSpaceOf1 = inShape2->GetLocalBounds().Scaled(inScale2).Transformed(transform2_to_1);
	}

	/// Returns true when collision detection should abort because it's not possible to find a better hit
	JPH_INLINE bool		ShouldAbort() const
	{
		return mCollector.ShouldEarlyOut();
	}

	/// Tests the bounds of shape 2 vs 4 bounding boxes, returns true for the ones that intersect
	JPH_INLINE UVec4	TestBounds(Vec4Arg inBoundsMinX, Vec4Arg inBoundsMinY, Vec4Arg inBoundsMinZ, Vec4Arg inBoundsMaxX, Vec4Arg inBoundsMaxY, Vec4Arg inBoundsMaxZ) const
	{
		// Scale the bounding boxes
		Vec4 bounds_min_x, bounds_min_y, bounds_min_z, bounds_max_x, bounds_max_y, bounds_max_z;
		AABox4Scale(mScale1, inBoundsMinX, inBoundsMinY, inBoundsMinZ, inBoundsMaxX, inBoundsMaxY, inBoundsMaxZ, bounds_min_x, bounds_min_y, bounds_min_z, bounds_max_x, bounds_max_y, bounds_max_z);

		// Test which boxes collide
		return AABox4VsBox(mBoundsOf2InSpaceOf1, bounds_min_x, bounds_min_y, bounds_min_z, bounds_max_x, bounds_max_y, bounds_max_z);
	}

	/// Test the shape against a single subshape
	JPH_INLINE void		VisitShape(const SubShape &inSubShape, uint32 inSubShapeIndex)
	{
		// Get world transform of 1
		Mat44 transform1 = mTransform1 * inSubShape.GetLocalTransformNoScale(mScale1);

		// Create ID for sub shape
		SubShapeIDCreator shape1_sub_shape_id = mSubShapeIDCreator1.PushID(inSubShapeIndex, mSubShapeBits);

		CollisionDispatch::sCollideShapeVsShape(inSubShape.mShape, mShape2, inSubShape.TransformScale(mScale1), mScale2, transform1, mTransform2, shape1_sub_shape_id, mSubShapeIDCreator2, mCollideShapeSettings, mCollector, mShapeFilter);
	}

	const CollideShapeSettings &	mCollideShapeSettings;
	CollideShapeCollector &			mCollector;
	const Shape *					mShape2;
	Vec3							mScale1;
	Vec3							mScale2;
	Mat44							mTransform1;
	Mat44							mTransform2;
	AABox							mBoundsOf2InSpaceOf1;
	SubShapeIDCreator				mSubShapeIDCreator1;
	SubShapeIDCreator				mSubShapeIDCreator2;
	uint							mSubShapeBits;
	const ShapeFilter &				mShapeFilter;
};

struct CompoundShape::CollideShapeVsCompoundVisitor
{
	JPH_INLINE			CollideShapeVsCompoundVisitor(const Shape *inShape1, const CompoundShape *inShape2, Vec3Arg inScale1, Vec3Arg inScale2, Mat44Arg inCenterOfMassTransform1, Mat44Arg inCenterOfMassTransform2, const SubShapeIDCreator &inSubShapeIDCreator1, const SubShapeIDCreator &inSubShapeIDCreator2, const CollideShapeSettings &inCollideShapeSettings, CollideShapeCollector &ioCollector, const ShapeFilter &inShapeFilter) :
		mCollideShapeSettings(inCollideShapeSettings),
		mCollector(ioCollector),
		mShape1(inShape1),
		mScale1(inScale1),
		mScale2(inScale2),
		mTransform1(inCenterOfMassTransform1),
		mTransform2(inCenterOfMassTransform2),
		mSubShapeIDCreator1(inSubShapeIDCreator1),
		mSubShapeIDCreator2(inSubShapeIDCreator2),
		mSubShapeBits(inShape2->GetSubShapeIDBits()),
		mShapeFilter(inShapeFilter)
	{
		// Get transform from shape 1 to shape 2
		Mat44 transform1_to_2 = inCenterOfMassTransform2.InversedRotationTranslation() * inCenterOfMassTransform1;

		// Convert bounding box of 1 into space of 2
		mBoundsOf1InSpaceOf2 = inShape1->GetLocalBounds().Scaled(inScale1).Transformed(transform1_to_2);
		mBoundsOf1InSpaceOf2.ExpandBy(Vec3::sReplicate(inCollideShapeSettings.mMaxSeparationDistance));
	}

	/// Returns true when collision detection should abort because it's not possible to find a better hit
	JPH_INLINE bool		ShouldAbort() const
	{
		return mCollector.ShouldEarlyOut();
	}

	/// Tests the bounds of shape 1 vs 4 bounding boxes, returns true for the ones that intersect
	JPH_INLINE UVec4	TestBounds(Vec4Arg inBoundsMinX, Vec4Arg inBoundsMinY, Vec4Arg inBoundsMinZ, Vec4Arg inBoundsMaxX, Vec4Arg inBoundsMaxY, Vec4Arg inBoundsMaxZ) const
	{
		// Scale the bounding boxes
		Vec4 bounds_min_x, bounds_min_y, bounds_min_z, bounds_max_x, bounds_max_y, bounds_max_z;
		AABox4Scale(mScale2, inBoundsMinX, inBoundsMinY, inBoundsMinZ, inBoundsMaxX, inBoundsMaxY, inBoundsMaxZ, bounds_min_x, bounds_min_y, bounds_min_z, bounds_max_x, bounds_max_y, bounds_max_z);

		// Test which bounding boxes collide
		return AABox4VsBox(mBoundsOf1InSpaceOf2, bounds_min_x, bounds_min_y, bounds_min_z, bounds_max_x, bounds_max_y, bounds_max_z);
	}

	/// Test the shape against a single subshape
	JPH_INLINE void		VisitShape(const SubShape &inSubShape, uint32 inSubShapeIndex)
	{
		// Create ID for sub shape
		SubShapeIDCreator shape2_sub_shape_id = mSubShapeIDCreator2.PushID(inSubShapeIndex, mSubShapeBits);

		// Get world transform of 2
		Mat44 transform2 = mTransform2 * inSubShape.GetLocalTransformNoScale(mScale2);

		CollisionDispatch::sCollideShapeVsShape(mShape1, inSubShape.mShape, mScale1, inSubShape.TransformScale(mScale2), mTransform1, transform2, mSubShapeIDCreator1, shape2_sub_shape_id, mCollideShapeSettings, mCollector, mShapeFilter);
	}

	const CollideShapeSettings &	mCollideShapeSettings;
	CollideShapeCollector &			mCollector;
	const Shape *					mShape1;
	Vec3							mScale1;
	Vec3							mScale2;
	Mat44							mTransform1;
	Mat44							mTransform2;
	AABox							mBoundsOf1InSpaceOf2;
	SubShapeIDCreator				mSubShapeIDCreator1;
	SubShapeIDCreator				mSubShapeIDCreator2;
	uint							mSubShapeBits;
	const ShapeFilter &				mShapeFilter;
};

template <class BoxType>
struct CompoundShape::GetIntersectingSubShapesVisitor
{
	JPH_INLINE			GetIntersectingSubShapesVisitor(const BoxType &inBox, uint *outSubShapeIndices, int inMaxSubShapeIndices) :
		mBox(inBox),
		mSubShapeIndices(outSubShapeIndices),
		mMaxSubShapeIndices(inMaxSubShapeIndices)
	{
	}

	/// Returns true when collision detection should abort because the buffer is full
	JPH_INLINE bool		ShouldAbort() const
	{
		return mNumResults >= mMaxSubShapeIndices;
	}

	/// Tests the box vs 4 bounding boxes, returns true for the ones that intersect
	JPH_INLINE UVec4	TestBounds(Vec4Arg inBoundsMinX, Vec4Arg inBoundsMinY, Vec4Arg inBoundsMinZ, Vec4Arg inBoundsMaxX, Vec4Arg inBoundsMaxY, Vec4Arg inBoundsMaxZ) const
	{
		// Test which bounding boxes collide
		return AABox4VsBox(mBox, inBoundsMinX, inBoundsMinY, inBoundsMinZ, inBoundsMaxX, inBoundsMaxY, inBoundsMaxZ);
	}

	/// Records a hit
	JPH_INLINE void		VisitShape([[maybe_unused]] const SubShape &inSubShape, uint32 inSubShapeIndex)
	{
		JPH_ASSERT(mNumResults < mMaxSubShapeIndices);
		*mSubShapeIndices++ = inSubShapeIndex;
		mNumResults++;
	}

	/// Get the number of indices that were found
	JPH_INLINE int		GetNumResults() const
	{
		return mNumResults;
	}

private:
	BoxType				mBox;
	uint *				mSubShapeIndices;
	int					mMaxSubShapeIndices;
	int					mNumResults = 0;
};

JPH_NAMESPACE_END

// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Jolt/Geometry/AABox.h>
#include <Jolt/Physics/Collision/CollideShape.h>
#include <Jolt/Physics/Collision/Shape/Shape.h>

JPH_NAMESPACE_BEGIN

/// Structure that holds a single shape cast (a shape moving along a linear path in 3d space with no rotation)
template <class Vec, class Mat, class ShapeCastType>
struct ShapeCastT
{
	JPH_OVERRIDE_NEW_DELETE

	/// Constructor
								ShapeCastT(const Shape *inShape, Vec3Arg inScale, typename Mat::ArgType inCenterOfMassStart, Vec3Arg inDirection, const AABox &inWorldSpaceBounds) :
		mShape(inShape),
		mScale(inScale),
		mCenterOfMassStart(inCenterOfMassStart),
		mDirection(inDirection),
		mShapeWorldBounds(inWorldSpaceBounds)
	{
	}

	/// Constructor
								ShapeCastT(const Shape *inShape, Vec3Arg inScale, typename Mat::ArgType inCenterOfMassStart, Vec3Arg inDirection) :
		ShapeCastT<Vec, Mat, ShapeCastType>(inShape, inScale, inCenterOfMassStart, inDirection, inShape->GetWorldSpaceBounds(inCenterOfMassStart, inScale))
	{
	}

	/// Construct a shape cast using a world transform for a shape instead of a center of mass transform
	static inline ShapeCastType	sFromWorldTransform(const Shape *inShape, Vec3Arg inScale, typename Mat::ArgType inWorldTransform, Vec3Arg inDirection)
	{
		return ShapeCastType(inShape, inScale, inWorldTransform.PreTranslated(inShape->GetCenterOfMass()), inDirection);
	}

	/// Transform this shape cast using inTransform. Multiply transform on the left left hand side.
	ShapeCastType				PostTransformed(typename Mat::ArgType inTransform) const
	{
		Mat44 start = inTransform * mCenterOfMassStart;
		Vec3 direction = inTransform.Multiply3x3(mDirection);
		return { mShape, mScale, start, direction };
	}

	/// Translate this shape cast by inTranslation.
	ShapeCastType				PostTranslated(typename Vec::ArgType inTranslation) const
	{
		return { mShape, mScale, mCenterOfMassStart.PostTranslated(inTranslation), mDirection };
	}

	/// Get point with fraction inFraction on ray from mCenterOfMassStart to mCenterOfMassStart + mDirection (0 = start of ray, 1 = end of ray)
	inline Vec					GetPointOnRay(float inFraction) const
	{
		return mCenterOfMassStart.GetTranslation() + inFraction * mDirection;
	}

	const Shape *				mShape;								///< Shape that's being cast (cannot be mesh shape). Note that this structure does not assume ownership over the shape for performance reasons.
	const Vec3					mScale;								///< Scale in local space of the shape being cast (scales relative to its center of mass)
	const Mat					mCenterOfMassStart;					///< Start position and orientation of the center of mass of the shape (construct using sFromWorldTransform if you have a world transform for your shape)
	const Vec3					mDirection;							///< Direction and length of the cast (anything beyond this length will not be reported as a hit)
	const AABox					mShapeWorldBounds;					///< Cached shape's world bounds, calculated in constructor
};

struct ShapeCast : public ShapeCastT<Vec3, Mat44, ShapeCast>
{
	using ShapeCastT<Vec3, Mat44, ShapeCast>::ShapeCastT;
};

struct RShapeCast : public ShapeCastT<RVec3, RMat44, RShapeCast>
{
	using ShapeCastT<RVec3, RMat44, RShapeCast>::ShapeCastT;

	/// Convert from ShapeCast, converts single to double precision
	explicit					RShapeCast(const ShapeCast &inCast) :
		RShapeCast(inCast.mShape, inCast.mScale, RMat44(inCast.mCenterOfMassStart), inCast.mDirection, inCast.mShapeWorldBounds)
	{
	}

	/// Convert to ShapeCast, which implies casting from double precision to single precision
	explicit					operator ShapeCast() const
	{
		return ShapeCast(mShape, mScale, mCenterOfMassStart.ToMat44(), mDirection, mShapeWorldBounds);
	}
};

/// Settings to be passed with a shape cast
class ShapeCastSettings : public CollideSettingsBase
{
public:
	JPH_OVERRIDE_NEW_DELETE

	/// Set the backfacing mode for all shapes
	void						SetBackFaceMode(EBackFaceMode inMode) { mBackFaceModeTriangles = mBackFaceModeConvex = inMode; }

	/// How backfacing triangles should be treated (should we report moving from back to front for triangle based shapes, e.g. for MeshShape/HeightFieldShape?)
	EBackFaceMode				mBackFaceModeTriangles = EBackFaceMode::IgnoreBackFaces;

	/// How backfacing convex objects should be treated (should we report starting inside an object and moving out?)
	EBackFaceMode				mBackFaceModeConvex = EBackFaceMode::IgnoreBackFaces;

	/// Indicates if we want to shrink the shape by the convex radius and then expand it again. This speeds up collision detection and gives a more accurate normal at the cost of a more 'rounded' shape.
	bool						mUseShrunkenShapeAndConvexRadius = false;

	/// When true, and the shape is intersecting at the beginning of the cast (fraction = 0) then this will calculate the deepest penetration point (costing additional CPU time)
	bool						mReturnDeepestPoint = false;
};

/// Result of a shape cast test
class ShapeCastResult : public CollideShapeResult
{
public:
	JPH_OVERRIDE_NEW_DELETE

	/// Default constructor
								ShapeCastResult() = default;

	/// Constructor
	/// @param inFraction Fraction at which the cast hit
	/// @param inContactPoint1 Contact point on shape 1
	/// @param inContactPoint2 Contact point on shape 2
	/// @param inContactNormalOrPenetrationDepth Contact normal pointing from shape 1 to 2 or penetration depth vector when the objects are penetrating (also from 1 to 2)
	/// @param inBackFaceHit If this hit was a back face hit
	/// @param inSubShapeID1 Sub shape id for shape 1
	/// @param inSubShapeID2 Sub shape id for shape 2
	/// @param inBodyID2 BodyID that was hit
								ShapeCastResult(float inFraction, Vec3Arg inContactPoint1, Vec3Arg inContactPoint2, Vec3Arg inContactNormalOrPenetrationDepth, bool inBackFaceHit, const SubShapeID &inSubShapeID1, const SubShapeID &inSubShapeID2, const BodyID &inBodyID2) :
		CollideShapeResult(inContactPoint1, inContactPoint2, inContactNormalOrPenetrationDepth, (inContactPoint2 - inContactPoint1).Length(), inSubShapeID1, inSubShapeID2, inBodyID2),
		mFraction(inFraction),
		mIsBackFaceHit(inBackFaceHit)
	{
	}

	/// Function required by the CollisionCollector. A smaller fraction is considered to be a 'better hit'. For rays/cast shapes we can just use the collision fraction. The fraction and penetration depth are combined in such a way that deeper hits at fraction 0 go first.
	inline float				GetEarlyOutFraction() const			{ return mFraction > 0.0f? mFraction : -mPenetrationDepth; }

	/// Reverses the hit result, swapping contact point 1 with contact point 2 etc.
	/// @param inWorldSpaceCastDirection Direction of the shape cast in world space
	ShapeCastResult				Reversed(Vec3Arg inWorldSpaceCastDirection) const
	{
		// Calculate by how much to shift the contact points
		Vec3 delta = mFraction * inWorldSpaceCastDirection;

		ShapeCastResult result;
		result.mContactPointOn2 = mContactPointOn1 - delta;
		result.mContactPointOn1 = mContactPointOn2 - delta;
		result.mPenetrationAxis = -mPenetrationAxis;
		result.mPenetrationDepth = mPenetrationDepth;
		result.mSubShapeID2 = mSubShapeID1;
		result.mSubShapeID1 = mSubShapeID2;
		result.mBodyID2 = mBodyID2;
		result.mFraction = mFraction;
		result.mIsBackFaceHit = mIsBackFaceHit;

		result.mShape2Face.resize(mShape1Face.size());
		for (Face::size_type i = 0; i < mShape1Face.size(); ++i)
			result.mShape2Face[i] = mShape1Face[i] - delta;

		result.mShape1Face.resize(mShape2Face.size());
		for (Face::size_type i = 0; i < mShape2Face.size(); ++i)
			result.mShape1Face[i] = mShape2Face[i] - delta;

		return result;
	}

	float						mFraction;							///< This is the fraction where the shape hit the other shape: CenterOfMassOnHit = Start + value * (End - Start)
	bool						mIsBackFaceHit;						///< True if the shape was hit from the back side
};

JPH_NAMESPACE_END

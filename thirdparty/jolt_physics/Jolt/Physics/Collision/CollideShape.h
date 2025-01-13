// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Jolt/Core/StaticArray.h>
#include <Jolt/Physics/Collision/BackFaceMode.h>
#include <Jolt/Physics/Collision/ActiveEdgeMode.h>
#include <Jolt/Physics/Collision/CollectFacesMode.h>
#include <Jolt/Physics/Collision/Shape/SubShapeID.h>
#include <Jolt/Physics/Body/BodyID.h>
#include <Jolt/Physics/PhysicsSettings.h>

JPH_NAMESPACE_BEGIN

/// Class that contains all information of two colliding shapes
class CollideShapeResult
{
public:
	JPH_OVERRIDE_NEW_DELETE

	/// Default constructor
								CollideShapeResult() = default;

	/// Constructor
								CollideShapeResult(Vec3Arg inContactPointOn1, Vec3Arg inContactPointOn2, Vec3Arg inPenetrationAxis, float inPenetrationDepth, const SubShapeID &inSubShapeID1, const SubShapeID &inSubShapeID2, const BodyID &inBodyID2) :
		mContactPointOn1(inContactPointOn1),
		mContactPointOn2(inContactPointOn2),
		mPenetrationAxis(inPenetrationAxis),
		mPenetrationDepth(inPenetrationDepth),
		mSubShapeID1(inSubShapeID1),
		mSubShapeID2(inSubShapeID2),
		mBodyID2(inBodyID2)
	{
	}

	/// Function required by the CollisionCollector. A smaller fraction is considered to be a 'better hit'. We use -penetration depth to get the hit with the biggest penetration depth
	inline float				GetEarlyOutFraction() const	{ return -mPenetrationDepth; }

	/// Reverses the hit result, swapping contact point 1 with contact point 2 etc.
	inline CollideShapeResult	Reversed() const
	{
		CollideShapeResult result;
		result.mContactPointOn2 = mContactPointOn1;
		result.mContactPointOn1 = mContactPointOn2;
		result.mPenetrationAxis = -mPenetrationAxis;
		result.mPenetrationDepth = mPenetrationDepth;
		result.mSubShapeID2 = mSubShapeID1;
		result.mSubShapeID1 = mSubShapeID2;
		result.mBodyID2 = mBodyID2;
		result.mShape2Face = mShape1Face;
		result.mShape1Face = mShape2Face;
		return result;
	}

	using Face = StaticArray<Vec3, 32>;

	Vec3						mContactPointOn1;			///< Contact point on the surface of shape 1 (in world space or relative to base offset)
	Vec3						mContactPointOn2;			///< Contact point on the surface of shape 2 (in world space or relative to base offset). If the penetration depth is 0, this will be the same as mContactPointOn1.
	Vec3						mPenetrationAxis;			///< Direction to move shape 2 out of collision along the shortest path (magnitude is meaningless, in world space). You can use -mPenetrationAxis.Normalized() as contact normal.
	float						mPenetrationDepth;			///< Penetration depth (move shape 2 by this distance to resolve the collision)
	SubShapeID					mSubShapeID1;				///< Sub shape ID that identifies the face on shape 1
	SubShapeID					mSubShapeID2;				///< Sub shape ID that identifies the face on shape 2
	BodyID						mBodyID2;					///< BodyID to which shape 2 belongs to
	Face						mShape1Face;				///< Colliding face on shape 1 (optional result, in world space or relative to base offset)
	Face						mShape2Face;				///< Colliding face on shape 2 (optional result, in world space or relative to base offset)
};

/// Settings to be passed with a collision query
class CollideSettingsBase
{
public:
	JPH_OVERRIDE_NEW_DELETE

	/// How active edges (edges that a moving object should bump into) are handled
	EActiveEdgeMode				mActiveEdgeMode				= EActiveEdgeMode::CollideOnlyWithActive;

	/// If colliding faces should be collected or only the collision point
	ECollectFacesMode			mCollectFacesMode			= ECollectFacesMode::NoFaces;

	/// If objects are closer than this distance, they are considered to be colliding (used for GJK) (unit: meter)
	float						mCollisionTolerance			= cDefaultCollisionTolerance;

	/// A factor that determines the accuracy of the penetration depth calculation. If the change of the squared distance is less than tolerance * current_penetration_depth^2 the algorithm will terminate. (unit: dimensionless)
	float						mPenetrationTolerance		= cDefaultPenetrationTolerance;

	/// When mActiveEdgeMode is CollideOnlyWithActive a movement direction can be provided. When hitting an inactive edge, the system will select the triangle normal as penetration depth only if it impedes the movement less than with the calculated penetration depth.
	Vec3						mActiveEdgeMovementDirection = Vec3::sZero();
};

/// Settings to be passed with a collision query
class CollideShapeSettings : public CollideSettingsBase
{
public:
	JPH_OVERRIDE_NEW_DELETE

	/// When > 0 contacts in the vicinity of the query shape can be found. All nearest contacts that are not further away than this distance will be found (unit: meter)
	float						mMaxSeparationDistance		= 0.0f;

	/// How backfacing triangles should be treated
	EBackFaceMode				mBackFaceMode				= EBackFaceMode::IgnoreBackFaces;
};

JPH_NAMESPACE_END

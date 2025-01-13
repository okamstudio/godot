// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Jolt/Physics/Body/BodyID.h>
#include <Jolt/Physics/Collision/Shape/SubShapeID.h>

JPH_NAMESPACE_BEGIN

/// Structure that holds a ray cast or other object cast hit
class BroadPhaseCastResult
{
public:
	JPH_OVERRIDE_NEW_DELETE

	/// Function required by the CollisionCollector. A smaller fraction is considered to be a 'better hit'. For rays/cast shapes we can just use the collision fraction.
	inline float	GetEarlyOutFraction() const			{ return mFraction; }

	/// Reset this result so it can be reused for a new cast.
	inline void		Reset()								{ mBodyID = BodyID(); mFraction = 1.0f + FLT_EPSILON; }

	BodyID			mBodyID;							///< Body that was hit
	float			mFraction = 1.0f + FLT_EPSILON;		///< Hit fraction of the ray/object [0, 1], HitPoint = Start + mFraction * (End - Start)
};

/// Specialization of cast result against a shape
class RayCastResult : public BroadPhaseCastResult
{
public:
	JPH_OVERRIDE_NEW_DELETE

	SubShapeID		mSubShapeID2;						///< Sub shape ID of shape that we collided against
};

JPH_NAMESPACE_END

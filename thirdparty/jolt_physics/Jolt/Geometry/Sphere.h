// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Jolt/Geometry/AABox.h>

JPH_NAMESPACE_BEGIN

class [[nodiscard]] Sphere
{
public:
	JPH_OVERRIDE_NEW_DELETE

	/// Constructor
	inline				Sphere() = default;
	inline				Sphere(const Float3 &inCenter, float inRadius)			: mCenter(inCenter), mRadius(inRadius) { }
	inline				Sphere(Vec3Arg inCenter, float inRadius)				: mRadius(inRadius) { inCenter.StoreFloat3(&mCenter); }

	/// Calculate the support vector for this convex shape.
	inline Vec3			GetSupport(Vec3Arg inDirection) const
	{
		float length = inDirection.Length();
		return length > 0.0f ? Vec3::sLoadFloat3Unsafe(mCenter) + (mRadius/ length) * inDirection : Vec3::sLoadFloat3Unsafe(mCenter);
	}

	// Properties
	inline Vec3			GetCenter() const										{ return Vec3::sLoadFloat3Unsafe(mCenter); }
	inline float		GetRadius() const										{ return mRadius; }

	/// Test if two spheres overlap
	inline bool			Overlaps(const Sphere &inB) const
	{
		return (Vec3::sLoadFloat3Unsafe(mCenter) - Vec3::sLoadFloat3Unsafe(inB.mCenter)).LengthSq() <= Square(mRadius + inB.mRadius);
	}

	/// Check if this sphere overlaps with a box
	inline bool			Overlaps(const AABox &inOther) const
	{
		return inOther.GetSqDistanceTo(GetCenter()) <= Square(mRadius);
	}

	/// Create the minimal sphere that encapsulates this sphere and inPoint
	inline void			EncapsulatePoint(Vec3Arg inPoint)
	{
		// Calculate distance between point and center
		Vec3 center = GetCenter();
		Vec3 d_vec = inPoint - center;
		float d_sq = d_vec.LengthSq();
		if (d_sq > Square(mRadius))
		{
			// It is further away than radius, we need to widen the sphere
			// The diameter of the new sphere is radius + d, so the new radius is half of that
			float d = sqrt(d_sq);
			float radius = 0.5f * (mRadius + d);

			// The center needs to shift by new radius - old radius in the direction of d
			center += (radius - mRadius) / d * d_vec;

			// Store new sphere
			center.StoreFloat3(&mCenter);
			mRadius = radius;
		}
	}

private:
	Float3				mCenter;
	float				mRadius;
};

JPH_NAMESPACE_END

// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#include <Jolt/Jolt.h>

#include <Jolt/Physics/Vehicle/VehicleCollisionTester.h>
#include <Jolt/Physics/Vehicle/VehicleConstraint.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/ShapeCast.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/CylinderShape.h>
#include <Jolt/Physics/Collision/CollisionCollectorImpl.h>
#include <Jolt/Physics/PhysicsSystem.h>

JPH_NAMESPACE_BEGIN

bool VehicleCollisionTesterRay::Collide(PhysicsSystem &inPhysicsSystem, const VehicleConstraint &inVehicleConstraint, uint inWheelIndex, RVec3Arg inOrigin, Vec3Arg inDirection, const BodyID &inVehicleBodyID, Body *&outBody, SubShapeID &outSubShapeID, RVec3 &outContactPosition, Vec3 &outContactNormal, float &outSuspensionLength) const
{
	const DefaultBroadPhaseLayerFilter default_broadphase_layer_filter = inPhysicsSystem.GetDefaultBroadPhaseLayerFilter(mObjectLayer);
	const BroadPhaseLayerFilter &broadphase_layer_filter = mBroadPhaseLayerFilter != nullptr? *mBroadPhaseLayerFilter : default_broadphase_layer_filter;

	const DefaultObjectLayerFilter default_object_layer_filter = inPhysicsSystem.GetDefaultLayerFilter(mObjectLayer);
	const ObjectLayerFilter &object_layer_filter = mObjectLayerFilter != nullptr? *mObjectLayerFilter : default_object_layer_filter;

	const IgnoreSingleBodyFilter default_body_filter(inVehicleBodyID);
	const BodyFilter &body_filter = mBodyFilter != nullptr? *mBodyFilter : default_body_filter;

	const WheelSettings *wheel_settings = inVehicleConstraint.GetWheel(inWheelIndex)->GetSettings();
	float wheel_radius = wheel_settings->mRadius;
	float ray_length = wheel_settings->mSuspensionMaxLength + wheel_radius;
	RRayCast ray { inOrigin, ray_length * inDirection };

	class MyCollector : public CastRayCollector
	{
	public:
							MyCollector(PhysicsSystem &inPhysicsSystem, const RRayCast &inRay, Vec3Arg inUpDirection, float inCosMaxSlopeAngle) :
			mPhysicsSystem(inPhysicsSystem),
			mRay(inRay),
			mUpDirection(inUpDirection),
			mCosMaxSlopeAngle(inCosMaxSlopeAngle)
		{
		}

		virtual void		AddHit(const RayCastResult &inResult) override
		{
			// Test if this collision is closer than the previous one
			if (inResult.mFraction < GetEarlyOutFraction())
			{
				// Lock the body
				BodyLockRead lock(mPhysicsSystem.GetBodyLockInterfaceNoLock(), inResult.mBodyID);
				JPH_ASSERT(lock.Succeeded()); // When this runs all bodies are locked so this should not fail
				const Body *body = &lock.GetBody();

				if (body->IsSensor())
					return;

				// Test that we're not hitting a vertical wall
				RVec3 contact_pos = mRay.GetPointOnRay(inResult.mFraction);
				Vec3 normal = body->GetWorldSpaceSurfaceNormal(inResult.mSubShapeID2, contact_pos);
				if (normal.Dot(mUpDirection) > mCosMaxSlopeAngle)
				{
					// Update early out fraction to this hit
					UpdateEarlyOutFraction(inResult.mFraction);

					// Get the contact properties
					mBody = body;
					mSubShapeID2 = inResult.mSubShapeID2;
					mContactPosition = contact_pos;
					mContactNormal = normal;
				}
			}
		}

		// Configuration
		PhysicsSystem &		mPhysicsSystem;
		RRayCast			mRay;
		Vec3				mUpDirection;
		float				mCosMaxSlopeAngle;

		// Resulting closest collision
		const Body *		mBody = nullptr;
		SubShapeID			mSubShapeID2;
		RVec3				mContactPosition;
		Vec3				mContactNormal;
	};

	RayCastSettings settings;

	MyCollector collector(inPhysicsSystem, ray, mUp, mCosMaxSlopeAngle);
	inPhysicsSystem.GetNarrowPhaseQueryNoLock().CastRay(ray, settings, collector, broadphase_layer_filter, object_layer_filter, body_filter);
	if (collector.mBody == nullptr)
		return false;

	outBody = const_cast<Body *>(collector.mBody);
	outSubShapeID = collector.mSubShapeID2;
	outContactPosition = collector.mContactPosition;
	outContactNormal = collector.mContactNormal;
	outSuspensionLength = max(0.0f, ray_length * collector.GetEarlyOutFraction() - wheel_radius);

	return true;
}

void VehicleCollisionTesterRay::PredictContactProperties(PhysicsSystem &inPhysicsSystem, const VehicleConstraint &inVehicleConstraint, uint inWheelIndex, RVec3Arg inOrigin, Vec3Arg inDirection, const BodyID &inVehicleBodyID, Body *&ioBody, SubShapeID &ioSubShapeID, RVec3 &ioContactPosition, Vec3 &ioContactNormal, float &ioSuspensionLength) const
{
	// Recalculate the contact points assuming the contact point is on an infinite plane
	const WheelSettings *wheel_settings = inVehicleConstraint.GetWheel(inWheelIndex)->GetSettings();
	float d_dot_n = inDirection.Dot(ioContactNormal);
	if (d_dot_n < -1.0e-6f)
	{
		// Reproject the contact position using the suspension ray and the plane formed by the contact position and normal
		ioContactPosition = inOrigin + Vec3(ioContactPosition - inOrigin).Dot(ioContactNormal) / d_dot_n * inDirection;

		// The suspension length is simply the distance between the contact position and the suspension origin excluding the wheel radius
		ioSuspensionLength = Clamp(Vec3(ioContactPosition - inOrigin).Dot(inDirection) - wheel_settings->mRadius, 0.0f, wheel_settings->mSuspensionMaxLength);
	}
	else
	{
		// If the normal is pointing away we assume there's no collision anymore
		ioSuspensionLength = wheel_settings->mSuspensionMaxLength;
	}
}

bool VehicleCollisionTesterCastSphere::Collide(PhysicsSystem &inPhysicsSystem, const VehicleConstraint &inVehicleConstraint, uint inWheelIndex, RVec3Arg inOrigin, Vec3Arg inDirection, const BodyID &inVehicleBodyID, Body *&outBody, SubShapeID &outSubShapeID, RVec3 &outContactPosition, Vec3 &outContactNormal, float &outSuspensionLength) const
{
	const DefaultBroadPhaseLayerFilter default_broadphase_layer_filter = inPhysicsSystem.GetDefaultBroadPhaseLayerFilter(mObjectLayer);
	const BroadPhaseLayerFilter &broadphase_layer_filter = mBroadPhaseLayerFilter != nullptr? *mBroadPhaseLayerFilter : default_broadphase_layer_filter;

	const DefaultObjectLayerFilter default_object_layer_filter = inPhysicsSystem.GetDefaultLayerFilter(mObjectLayer);
	const ObjectLayerFilter &object_layer_filter = mObjectLayerFilter != nullptr? *mObjectLayerFilter : default_object_layer_filter;

	const IgnoreSingleBodyFilter default_body_filter(inVehicleBodyID);
	const BodyFilter &body_filter = mBodyFilter != nullptr? *mBodyFilter : default_body_filter;

	SphereShape sphere(mRadius);
	sphere.SetEmbedded();

	const WheelSettings *wheel_settings = inVehicleConstraint.GetWheel(inWheelIndex)->GetSettings();
	float wheel_radius = wheel_settings->mRadius;
	float shape_cast_length = wheel_settings->mSuspensionMaxLength + wheel_radius - mRadius;
	RShapeCast shape_cast(&sphere, Vec3::sReplicate(1.0f), RMat44::sTranslation(inOrigin), inDirection * shape_cast_length);

	ShapeCastSettings settings;
	settings.mUseShrunkenShapeAndConvexRadius = true;
	settings.mReturnDeepestPoint = true;

	class MyCollector : public CastShapeCollector
	{
	public:
							MyCollector(PhysicsSystem &inPhysicsSystem, const RShapeCast &inShapeCast, Vec3Arg inUpDirection, float inCosMaxSlopeAngle) :
			mPhysicsSystem(inPhysicsSystem),
			mShapeCast(inShapeCast),
			mUpDirection(inUpDirection),
			mCosMaxSlopeAngle(inCosMaxSlopeAngle)
		{
		}

		virtual void		AddHit(const ShapeCastResult &inResult) override
		{
			// Test if this collision is closer/deeper than the previous one
			float early_out = inResult.GetEarlyOutFraction();
			if (early_out < GetEarlyOutFraction())
			{
				// Lock the body
				BodyLockRead lock(mPhysicsSystem.GetBodyLockInterfaceNoLock(), inResult.mBodyID2);
				JPH_ASSERT(lock.Succeeded()); // When this runs all bodies are locked so this should not fail
				const Body *body = &lock.GetBody();

				if (body->IsSensor())
					return;

				// Test that we're not hitting a vertical wall
				Vec3 normal = -inResult.mPenetrationAxis.Normalized();
				if (normal.Dot(mUpDirection) > mCosMaxSlopeAngle)
				{
					// Update early out fraction to this hit
					UpdateEarlyOutFraction(early_out);

					// Get the contact properties
					mBody = body;
					mSubShapeID2 = inResult.mSubShapeID2;
					mContactPosition = mShapeCast.mCenterOfMassStart.GetTranslation() + inResult.mContactPointOn2;
					mContactNormal = normal;
					mFraction = inResult.mFraction;
				}
			}
		}

		// Configuration
		PhysicsSystem &		mPhysicsSystem;
		const RShapeCast &	mShapeCast;
		Vec3				mUpDirection;
		float				mCosMaxSlopeAngle;

		// Resulting closest collision
		const Body *		mBody = nullptr;
		SubShapeID			mSubShapeID2;
		RVec3				mContactPosition;
		Vec3				mContactNormal;
		float				mFraction;
	};

	MyCollector collector(inPhysicsSystem, shape_cast, mUp, mCosMaxSlopeAngle);
	inPhysicsSystem.GetNarrowPhaseQueryNoLock().CastShape(shape_cast, settings, shape_cast.mCenterOfMassStart.GetTranslation(), collector, broadphase_layer_filter, object_layer_filter, body_filter);
	if (collector.mBody == nullptr)
		return false;

	outBody = const_cast<Body *>(collector.mBody);
	outSubShapeID = collector.mSubShapeID2;
	outContactPosition = collector.mContactPosition;
	outContactNormal = collector.mContactNormal;
	outSuspensionLength = max(0.0f, shape_cast_length * collector.mFraction + mRadius - wheel_radius);

	return true;
}

void VehicleCollisionTesterCastSphere::PredictContactProperties(PhysicsSystem &inPhysicsSystem, const VehicleConstraint &inVehicleConstraint, uint inWheelIndex, RVec3Arg inOrigin, Vec3Arg inDirection, const BodyID &inVehicleBodyID, Body *&ioBody, SubShapeID &ioSubShapeID, RVec3 &ioContactPosition, Vec3 &ioContactNormal, float &ioSuspensionLength) const
{
	// Recalculate the contact points assuming the contact point is on an infinite plane
	const WheelSettings *wheel_settings = inVehicleConstraint.GetWheel(inWheelIndex)->GetSettings();
	float d_dot_n = inDirection.Dot(ioContactNormal);
	if (d_dot_n < -1.0e-6f)
	{
		// Reproject the contact position using the suspension cast sphere and the plane formed by the contact position and normal
		// This solves x = inOrigin + fraction * inDirection and (x - ioContactPosition) . ioContactNormal = mRadius for fraction
		float oc_dot_n = Vec3(ioContactPosition - inOrigin).Dot(ioContactNormal);
		float fraction = (mRadius + oc_dot_n) / d_dot_n;
		ioContactPosition = inOrigin + fraction * inDirection - mRadius * ioContactNormal;

		// Calculate the new suspension length in the same way as the cast sphere normally does
		ioSuspensionLength = Clamp(fraction + mRadius - wheel_settings->mRadius, 0.0f, wheel_settings->mSuspensionMaxLength);
	}
	else
	{
		// If the normal is pointing away we assume there's no collision anymore
		ioSuspensionLength = wheel_settings->mSuspensionMaxLength;
	}
}

bool VehicleCollisionTesterCastCylinder::Collide(PhysicsSystem &inPhysicsSystem, const VehicleConstraint &inVehicleConstraint, uint inWheelIndex, RVec3Arg inOrigin, Vec3Arg inDirection, const BodyID &inVehicleBodyID, Body *&outBody, SubShapeID &outSubShapeID, RVec3 &outContactPosition, Vec3 &outContactNormal, float &outSuspensionLength) const
{
	const DefaultBroadPhaseLayerFilter default_broadphase_layer_filter = inPhysicsSystem.GetDefaultBroadPhaseLayerFilter(mObjectLayer);
	const BroadPhaseLayerFilter &broadphase_layer_filter = mBroadPhaseLayerFilter != nullptr? *mBroadPhaseLayerFilter : default_broadphase_layer_filter;

	const DefaultObjectLayerFilter default_object_layer_filter = inPhysicsSystem.GetDefaultLayerFilter(mObjectLayer);
	const ObjectLayerFilter &object_layer_filter = mObjectLayerFilter != nullptr? *mObjectLayerFilter : default_object_layer_filter;

	const IgnoreSingleBodyFilter default_body_filter(inVehicleBodyID);
	const BodyFilter &body_filter = mBodyFilter != nullptr? *mBodyFilter : default_body_filter;

	const WheelSettings *wheel_settings = inVehicleConstraint.GetWheel(inWheelIndex)->GetSettings();
	float max_suspension_length = wheel_settings->mSuspensionMaxLength;

	// Get the wheel transform given that the cylinder rotates around the Y axis
	RMat44 shape_cast_start = inVehicleConstraint.GetWheelWorldTransform(inWheelIndex, Vec3::sAxisY(), Vec3::sAxisX());
	shape_cast_start.SetTranslation(inOrigin);

	// Construct a cylinder with the dimensions of the wheel
	float wheel_half_width = 0.5f * wheel_settings->mWidth;
	CylinderShape cylinder(wheel_half_width, wheel_settings->mRadius, min(wheel_half_width, wheel_settings->mRadius) * mConvexRadiusFraction);
	cylinder.SetEmbedded();

	RShapeCast shape_cast(&cylinder, Vec3::sReplicate(1.0f), shape_cast_start, inDirection * max_suspension_length);

	ShapeCastSettings settings;
	settings.mUseShrunkenShapeAndConvexRadius = true;
	settings.mReturnDeepestPoint = true;

	class MyCollector : public CastShapeCollector
	{
	public:
							MyCollector(PhysicsSystem &inPhysicsSystem, const RShapeCast &inShapeCast) :
			mPhysicsSystem(inPhysicsSystem),
			mShapeCast(inShapeCast)
		{
		}

		virtual void		AddHit(const ShapeCastResult &inResult) override
		{
			// Test if this collision is closer/deeper than the previous one
			float early_out = inResult.GetEarlyOutFraction();
			if (early_out < GetEarlyOutFraction())
			{
				// Lock the body
				BodyLockRead lock(mPhysicsSystem.GetBodyLockInterfaceNoLock(), inResult.mBodyID2);
				JPH_ASSERT(lock.Succeeded()); // When this runs all bodies are locked so this should not fail
				const Body *body = &lock.GetBody();

				if (body->IsSensor())
					return;

				// Update early out fraction to this hit
				UpdateEarlyOutFraction(early_out);

				// Get the contact properties
				mBody = body;
				mSubShapeID2 = inResult.mSubShapeID2;
				mContactPosition = mShapeCast.mCenterOfMassStart.GetTranslation() + inResult.mContactPointOn2;
				mContactNormal = -inResult.mPenetrationAxis.Normalized();
				mFraction = inResult.mFraction;
			}
		}

		// Configuration
		PhysicsSystem &		mPhysicsSystem;
		const RShapeCast &	mShapeCast;

		// Resulting closest collision
		const Body *		mBody = nullptr;
		SubShapeID			mSubShapeID2;
		RVec3				mContactPosition;
		Vec3				mContactNormal;
		float				mFraction;
	};

	MyCollector collector(inPhysicsSystem, shape_cast);
	inPhysicsSystem.GetNarrowPhaseQueryNoLock().CastShape(shape_cast, settings, shape_cast.mCenterOfMassStart.GetTranslation(), collector, broadphase_layer_filter, object_layer_filter, body_filter);
	if (collector.mBody == nullptr)
		return false;

	outBody = const_cast<Body *>(collector.mBody);
	outSubShapeID = collector.mSubShapeID2;
	outContactPosition = collector.mContactPosition;
	outContactNormal = collector.mContactNormal;
	outSuspensionLength = max_suspension_length * collector.mFraction;

	return true;
}

void VehicleCollisionTesterCastCylinder::PredictContactProperties(PhysicsSystem &inPhysicsSystem, const VehicleConstraint &inVehicleConstraint, uint inWheelIndex, RVec3Arg inOrigin, Vec3Arg inDirection, const BodyID &inVehicleBodyID, Body *&ioBody, SubShapeID &ioSubShapeID, RVec3 &ioContactPosition, Vec3 &ioContactNormal, float &ioSuspensionLength) const
{
	// Recalculate the contact points assuming the contact point is on an infinite plane
	const WheelSettings *wheel_settings = inVehicleConstraint.GetWheel(inWheelIndex)->GetSettings();
	float d_dot_n = inDirection.Dot(ioContactNormal);
	if (d_dot_n < -1.0e-6f)
	{
		// Wheel size
		float half_width = 0.5f * wheel_settings->mWidth;
		float radius = wheel_settings->mRadius;

		// Get the inverse local space contact normal for a cylinder pointing along Y
		RMat44 wheel_transform = inVehicleConstraint.GetWheelWorldTransform(inWheelIndex, Vec3::sAxisY(), Vec3::sAxisX());
		Vec3 inverse_local_normal = -wheel_transform.Multiply3x3Transposed(ioContactNormal);

		// Get the support point of this normal in local space of the cylinder
		// See CylinderShape::Cylinder::GetSupport
		float x = inverse_local_normal.GetX(), y = inverse_local_normal.GetY(), z = inverse_local_normal.GetZ();
		float o = sqrt(Square(x) + Square(z));
		Vec3 support_point;
		if (o > 0.0f)
			support_point = Vec3((radius * x) / o, Sign(y) * half_width, (radius * z) / o);
		else
			support_point = Vec3(0, Sign(y) * half_width, 0);

		// Rotate back to world space
		support_point = wheel_transform.Multiply3x3(support_point);

		// Now we can use inOrigin + support_point as the start of a ray of our suspension to the contact plane
		// as know that it is the first point on the wheel that will hit the plane
		RVec3 origin = inOrigin + support_point;

		// Calculate contact position and suspension length, the is the same as VehicleCollisionTesterRay
		// but we don't need to take the radius into account anymore
		Vec3 oc(ioContactPosition - origin);
		ioContactPosition = origin + oc.Dot(ioContactNormal) / d_dot_n * inDirection;
		ioSuspensionLength = Clamp(oc.Dot(inDirection), 0.0f, wheel_settings->mSuspensionMaxLength);
	}
	else
	{
		// If the normal is pointing away we assume there's no collision anymore
		ioSuspensionLength = wheel_settings->mSuspensionMaxLength;
	}
}

JPH_NAMESPACE_END

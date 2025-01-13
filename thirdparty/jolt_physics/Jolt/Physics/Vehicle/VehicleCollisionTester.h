// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Jolt/Physics/Body/Body.h>
#include <Jolt/Core/NonCopyable.h>

JPH_NAMESPACE_BEGIN

class PhysicsSystem;
class VehicleConstraint;
class BroadPhaseLayerFilter;
class ObjectLayerFilter;
class BodyFilter;

/// Class that does collision detection between wheels and ground
class JPH_EXPORT VehicleCollisionTester : public RefTarget<VehicleCollisionTester>, public NonCopyable
{
public:
	JPH_OVERRIDE_NEW_DELETE

	/// Constructors
									VehicleCollisionTester() = default;
	explicit						VehicleCollisionTester(ObjectLayer inObjectLayer) : mObjectLayer(inObjectLayer) { }

	/// Virtual destructor
	virtual							~VehicleCollisionTester() = default;

	/// Object layer to use for collision detection, this is used when the filters are not overridden
	ObjectLayer						GetObjectLayer() const												{ return mObjectLayer; }
	void							SetObjectLayer(ObjectLayer inObjectLayer)							{ mObjectLayer = inObjectLayer; }

	/// Access to the broad phase layer filter, when set this overrides the object layer supplied in the constructor
	void							SetBroadPhaseLayerFilter(const BroadPhaseLayerFilter *inFilter)		{ mBroadPhaseLayerFilter = inFilter; }
	const BroadPhaseLayerFilter *	GetBroadPhaseLayerFilter() const									{ return mBroadPhaseLayerFilter; }

	/// Access to the object layer filter, when set this overrides the object layer supplied in the constructor
	void							SetObjectLayerFilter(const ObjectLayerFilter *inFilter)				{ mObjectLayerFilter = inFilter; }
	const ObjectLayerFilter *		GetObjectLayerFilter() const										{ return mObjectLayerFilter; }

	/// Access to the body filter, when set this overrides the default filter that filters out the vehicle body
	void							SetBodyFilter(const BodyFilter *inFilter)							{ mBodyFilter = inFilter; }
	const BodyFilter *				GetBodyFilter() const												{ return mBodyFilter; }

	/// Do a collision test with the world
	/// @param inPhysicsSystem The physics system that should be tested against
	/// @param inVehicleConstraint The vehicle constraint
	/// @param inWheelIndex Index of the wheel that we're testing collision for
	/// @param inOrigin Origin for the test, corresponds to the world space position for the suspension attachment point
	/// @param inDirection Direction for the test (unit vector, world space)
	/// @param inVehicleBodyID This body should be filtered out during collision detection to avoid self collisions
	/// @param outBody Body that the wheel collided with
	/// @param outSubShapeID Sub shape ID that the wheel collided with
	/// @param outContactPosition Contact point between wheel and floor, in world space
	/// @param outContactNormal Contact normal between wheel and floor, pointing away from the floor
	/// @param outSuspensionLength New length of the suspension [0, inSuspensionMaxLength]
	/// @return True when collision found, false if not
	virtual bool					Collide(PhysicsSystem &inPhysicsSystem, const VehicleConstraint &inVehicleConstraint, uint inWheelIndex, RVec3Arg inOrigin, Vec3Arg inDirection, const BodyID &inVehicleBodyID, Body *&outBody, SubShapeID &outSubShapeID, RVec3 &outContactPosition, Vec3 &outContactNormal, float &outSuspensionLength) const = 0;

	/// Do a cheap contact properties prediction based on the contact properties from the last collision test (provided as input parameters)
	/// @param inPhysicsSystem The physics system that should be tested against
	/// @param inVehicleConstraint The vehicle constraint
	/// @param inWheelIndex Index of the wheel that we're testing collision for
	/// @param inOrigin Origin for the test, corresponds to the world space position for the suspension attachment point
	/// @param inDirection Direction for the test (unit vector, world space)
	/// @param inVehicleBodyID The body ID for the vehicle itself
	/// @param ioBody Body that the wheel previously collided with
	/// @param ioSubShapeID Sub shape ID that the wheel collided with during the last check
	/// @param ioContactPosition Contact point between wheel and floor during the last check, in world space
	/// @param ioContactNormal Contact normal between wheel and floor during the last check, pointing away from the floor
	/// @param ioSuspensionLength New length of the suspension [0, inSuspensionMaxLength]
	virtual void					PredictContactProperties(PhysicsSystem &inPhysicsSystem, const VehicleConstraint &inVehicleConstraint, uint inWheelIndex, RVec3Arg inOrigin, Vec3Arg inDirection, const BodyID &inVehicleBodyID, Body *&ioBody, SubShapeID &ioSubShapeID, RVec3 &ioContactPosition, Vec3 &ioContactNormal, float &ioSuspensionLength) const = 0;

protected:
	const BroadPhaseLayerFilter	*	mBroadPhaseLayerFilter = nullptr;
	const ObjectLayerFilter *		mObjectLayerFilter = nullptr;
	const BodyFilter *				mBodyFilter = nullptr;
	ObjectLayer						mObjectLayer = cObjectLayerInvalid;
};

/// Collision tester that tests collision using a raycast
class JPH_EXPORT VehicleCollisionTesterRay : public VehicleCollisionTester
{
public:
	JPH_OVERRIDE_NEW_DELETE

	/// Constructor
	/// @param inObjectLayer Object layer to test collision with
	/// @param inUp World space up vector, used to avoid colliding with vertical walls.
	/// @param inMaxSlopeAngle Max angle (rad) that is considered for colliding wheels. This is to avoid colliding with vertical walls.
									VehicleCollisionTesterRay(ObjectLayer inObjectLayer, Vec3Arg inUp = Vec3::sAxisY(), float inMaxSlopeAngle = DegreesToRadians(80.0f)) : VehicleCollisionTester(inObjectLayer), mUp(inUp), mCosMaxSlopeAngle(Cos(inMaxSlopeAngle)) { }

	// See: VehicleCollisionTester
	virtual bool					Collide(PhysicsSystem &inPhysicsSystem, const VehicleConstraint &inVehicleConstraint, uint inWheelIndex, RVec3Arg inOrigin, Vec3Arg inDirection, const BodyID &inVehicleBodyID, Body *&outBody, SubShapeID &outSubShapeID, RVec3 &outContactPosition, Vec3 &outContactNormal, float &outSuspensionLength) const override;
	virtual void					PredictContactProperties(PhysicsSystem &inPhysicsSystem, const VehicleConstraint &inVehicleConstraint, uint inWheelIndex, RVec3Arg inOrigin, Vec3Arg inDirection, const BodyID &inVehicleBodyID, Body *&ioBody, SubShapeID &ioSubShapeID, RVec3 &ioContactPosition, Vec3 &ioContactNormal, float &ioSuspensionLength) const override;

private:
	Vec3							mUp;
	float							mCosMaxSlopeAngle;
};

/// Collision tester that tests collision using a sphere cast
class JPH_EXPORT VehicleCollisionTesterCastSphere : public VehicleCollisionTester
{
public:
	JPH_OVERRIDE_NEW_DELETE

	/// Constructor
	/// @param inObjectLayer Object layer to test collision with
	/// @param inUp World space up vector, used to avoid colliding with vertical walls.
	/// @param inRadius Radius of sphere
	/// @param inMaxSlopeAngle Max angle (rad) that is considered for colliding wheels. This is to avoid colliding with vertical walls.
									VehicleCollisionTesterCastSphere(ObjectLayer inObjectLayer, float inRadius, Vec3Arg inUp = Vec3::sAxisY(), float inMaxSlopeAngle = DegreesToRadians(80.0f)) : VehicleCollisionTester(inObjectLayer), mRadius(inRadius), mUp(inUp), mCosMaxSlopeAngle(Cos(inMaxSlopeAngle)) { }

	// See: VehicleCollisionTester
	virtual bool					Collide(PhysicsSystem &inPhysicsSystem, const VehicleConstraint &inVehicleConstraint, uint inWheelIndex, RVec3Arg inOrigin, Vec3Arg inDirection, const BodyID &inVehicleBodyID, Body *&outBody, SubShapeID &outSubShapeID, RVec3 &outContactPosition, Vec3 &outContactNormal, float &outSuspensionLength) const override;
	virtual void					PredictContactProperties(PhysicsSystem &inPhysicsSystem, const VehicleConstraint &inVehicleConstraint, uint inWheelIndex, RVec3Arg inOrigin, Vec3Arg inDirection, const BodyID &inVehicleBodyID, Body *&ioBody, SubShapeID &ioSubShapeID, RVec3 &ioContactPosition, Vec3 &ioContactNormal, float &ioSuspensionLength) const override;

private:
	float							mRadius;
	Vec3							mUp;
	float							mCosMaxSlopeAngle;
};

/// Collision tester that tests collision using a cylinder shape
class JPH_EXPORT VehicleCollisionTesterCastCylinder : public VehicleCollisionTester
{
public:
	JPH_OVERRIDE_NEW_DELETE

	/// Constructor
	/// @param inObjectLayer Object layer to test collision with
	/// @param inConvexRadiusFraction Fraction of half the wheel width (or wheel radius if it is smaller) that is used as the convex radius
									VehicleCollisionTesterCastCylinder(ObjectLayer inObjectLayer, float inConvexRadiusFraction = 0.1f) : VehicleCollisionTester(inObjectLayer), mConvexRadiusFraction(inConvexRadiusFraction) { JPH_ASSERT(mConvexRadiusFraction >= 0.0f && mConvexRadiusFraction <= 1.0f); }

	// See: VehicleCollisionTester
	virtual bool					Collide(PhysicsSystem &inPhysicsSystem, const VehicleConstraint &inVehicleConstraint, uint inWheelIndex, RVec3Arg inOrigin, Vec3Arg inDirection, const BodyID &inVehicleBodyID, Body *&outBody, SubShapeID &outSubShapeID, RVec3 &outContactPosition, Vec3 &outContactNormal, float &outSuspensionLength) const override;
	virtual void					PredictContactProperties(PhysicsSystem &inPhysicsSystem, const VehicleConstraint &inVehicleConstraint, uint inWheelIndex, RVec3Arg inOrigin, Vec3Arg inDirection, const BodyID &inVehicleBodyID, Body *&ioBody, SubShapeID &ioSubShapeID, RVec3 &ioContactPosition, Vec3 &ioContactNormal, float &ioSuspensionLength) const override;

private:
	float							mConvexRadiusFraction;
};

JPH_NAMESPACE_END

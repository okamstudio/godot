// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#include <Jolt/Jolt.h>

#include <Jolt/Physics/Body/Body.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/SoftBody/SoftBodyCreationSettings.h>
#include <Jolt/Physics/SoftBody/SoftBodyMotionProperties.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/StateRecorder.h>
#include <Jolt/Physics/Collision/Shape/EmptyShape.h>
#include <Jolt/Core/StringTools.h>
#include <Jolt/Core/Profiler.h>
#ifdef JPH_DEBUG_RENDERER
	#include <Jolt/Renderer/DebugRenderer.h>
#endif // JPH_DEBUG_RENDERER

JPH_NAMESPACE_BEGIN

static const EmptyShape sFixedToWorldShape;
Body Body::sFixedToWorld(false);

Body::Body(bool) :
	mPosition(Vec3::sZero()),
	mRotation(Quat::sIdentity()),
	mShape(&sFixedToWorldShape), // Dummy shape
	mFriction(0.0f),
	mRestitution(0.0f),
	mObjectLayer(cObjectLayerInvalid),
	mMotionType(EMotionType::Static)
{
	sFixedToWorldShape.SetEmbedded();
}

void Body::SetMotionType(EMotionType inMotionType)
{
	if (mMotionType == inMotionType)
		return;

	JPH_ASSERT(inMotionType == EMotionType::Static || mMotionProperties != nullptr, "Body needs to be created with mAllowDynamicOrKinematic set tot true");
	JPH_ASSERT(inMotionType != EMotionType::Static || !IsActive(), "Deactivate body first");
	JPH_ASSERT(inMotionType == EMotionType::Dynamic || !IsSoftBody(), "Soft bodies can only be dynamic, you can make individual vertices kinematic by setting their inverse mass to 0");

	// Store new motion type
	mMotionType = inMotionType;

	if (mMotionProperties != nullptr)
	{
		// Update cache
		JPH_IF_ENABLE_ASSERTS(mMotionProperties->mCachedMotionType = inMotionType;)

		switch (inMotionType)
		{
		case EMotionType::Static:
			// Stop the object
			mMotionProperties->mLinearVelocity = Vec3::sZero();
			mMotionProperties->mAngularVelocity = Vec3::sZero();
			[[fallthrough]];

		case EMotionType::Kinematic:
			// Cancel forces
			mMotionProperties->ResetForce();
			mMotionProperties->ResetTorque();
			break;

		case EMotionType::Dynamic:
			break;
		}
	}
}

void Body::SetAllowSleeping(bool inAllow)
{
	mMotionProperties->mAllowSleeping = inAllow;
	if (inAllow)
		ResetSleepTimer();
}

void Body::MoveKinematic(RVec3Arg inTargetPosition, QuatArg inTargetRotation, float inDeltaTime)
{
	JPH_ASSERT(IsRigidBody()); // Only valid for rigid bodies
	JPH_ASSERT(!IsStatic());
	JPH_ASSERT(BodyAccess::sCheckRights(BodyAccess::sPositionAccess(), BodyAccess::EAccess::Read));

	// Calculate center of mass at end situation
	RVec3 new_com = inTargetPosition + inTargetRotation * mShape->GetCenterOfMass();

	// Calculate delta position and rotation
	Vec3 delta_pos = Vec3(new_com - mPosition);
	Quat delta_rotation = inTargetRotation * mRotation.Conjugated();

	mMotionProperties->MoveKinematic(delta_pos, delta_rotation, inDeltaTime);
}

void Body::CalculateWorldSpaceBoundsInternal()
{
	mBounds = mShape->GetWorldSpaceBounds(GetCenterOfMassTransform(), Vec3::sReplicate(1.0f));
}

void Body::SetPositionAndRotationInternal(RVec3Arg inPosition, QuatArg inRotation, bool inResetSleepTimer)
{
	JPH_ASSERT(BodyAccess::sCheckRights(BodyAccess::sPositionAccess(), BodyAccess::EAccess::ReadWrite));

	mPosition = inPosition + inRotation * mShape->GetCenterOfMass();
	mRotation = inRotation;

	// Initialize bounding box
	CalculateWorldSpaceBoundsInternal();

	// Reset sleeping test
	if (inResetSleepTimer && mMotionProperties != nullptr)
		ResetSleepTimer();
}

void Body::UpdateCenterOfMassInternal(Vec3Arg inPreviousCenterOfMass, bool inUpdateMassProperties)
{
	// Update center of mass position so the world position for this body stays the same
	mPosition += mRotation * (mShape->GetCenterOfMass() - inPreviousCenterOfMass);

	// Recalculate mass and inertia if requested
	if (inUpdateMassProperties && mMotionProperties != nullptr)
		mMotionProperties->SetMassProperties(mMotionProperties->GetAllowedDOFs(), mShape->GetMassProperties());
}

void Body::SetShapeInternal(const Shape *inShape, bool inUpdateMassProperties)
{
	JPH_ASSERT(IsRigidBody()); // Only valid for rigid bodies
	JPH_ASSERT(BodyAccess::sCheckRights(BodyAccess::sPositionAccess(), BodyAccess::EAccess::ReadWrite));

	// Get the old center of mass
	Vec3 old_com = mShape->GetCenterOfMass();

	// Update the shape
	mShape = inShape;

	// Update center of mass
	UpdateCenterOfMassInternal(old_com, inUpdateMassProperties);

	// Recalculate bounding box
	CalculateWorldSpaceBoundsInternal();
}

ECanSleep Body::UpdateSleepStateInternal(float inDeltaTime, float inMaxMovement, float inTimeBeforeSleep)
{
	// Check override & sensors will never go to sleep (they would stop detecting collisions with sleeping bodies)
	if (!mMotionProperties->mAllowSleeping || IsSensor())
		return ECanSleep::CannotSleep;

	// Get the points to test
	RVec3 points[3];
	GetSleepTestPoints(points);

#ifdef JPH_DOUBLE_PRECISION
	// Get base offset for spheres
	DVec3 offset = mMotionProperties->GetSleepTestOffset();
#endif // JPH_DOUBLE_PRECISION

	for (int i = 0; i < 3; ++i)
	{
		Sphere &sphere = mMotionProperties->mSleepTestSpheres[i];

		// Make point relative to base offset
#ifdef JPH_DOUBLE_PRECISION
		Vec3 p = Vec3(points[i] - offset);
#else
		Vec3 p = points[i];
#endif // JPH_DOUBLE_PRECISION

		// Encapsulate the point in a sphere
		sphere.EncapsulatePoint(p);

		// Test if it exceeded the max movement
		if (sphere.GetRadius() > inMaxMovement)
		{
			// Body is not sleeping, reset test
			mMotionProperties->ResetSleepTestSpheres(points);
			return ECanSleep::CannotSleep;
		}
	}

	return mMotionProperties->AccumulateSleepTime(inDeltaTime, inTimeBeforeSleep);
}

void Body::GetSubmergedVolume(RVec3Arg inSurfacePosition, Vec3Arg inSurfaceNormal, float &outTotalVolume, float &outSubmergedVolume, Vec3 &outRelativeCenterOfBuoyancy) const
{
	// For GetSubmergedVolume we transform the surface relative to the body position for increased precision
	Mat44 rotation = Mat44::sRotation(mRotation);
	Plane surface_relative_to_body = Plane::sFromPointAndNormal(inSurfacePosition - mPosition, inSurfaceNormal);

	// Calculate amount of volume that is submerged and what the center of buoyancy is
	mShape->GetSubmergedVolume(rotation, Vec3::sReplicate(1.0f), surface_relative_to_body, outTotalVolume, outSubmergedVolume, outRelativeCenterOfBuoyancy JPH_IF_DEBUG_RENDERER(, mPosition));
}

bool Body::ApplyBuoyancyImpulse(float inTotalVolume, float inSubmergedVolume, Vec3Arg inRelativeCenterOfBuoyancy, float inBuoyancy, float inLinearDrag, float inAngularDrag, Vec3Arg inFluidVelocity, Vec3Arg inGravity, float inDeltaTime)
{
	JPH_ASSERT(IsRigidBody()); // Only implemented for rigid bodies currently

	// We follow the approach from 'Game Programming Gems 6' 2.5 Exact Buoyancy for Polyhedra
	// All quantities below are in world space

	// If we're not submerged, there's no point in doing the rest of the calculations
	if (inSubmergedVolume > 0.0f)
	{
	#ifdef JPH_DEBUG_RENDERER
		// Draw submerged volume properties
		if (Shape::sDrawSubmergedVolumes)
		{
			RVec3 center_of_buoyancy = mPosition + inRelativeCenterOfBuoyancy;
			DebugRenderer::sInstance->DrawMarker(center_of_buoyancy, Color::sWhite, 2.0f);
			DebugRenderer::sInstance->DrawText3D(center_of_buoyancy, StringFormat("%.3f / %.3f", (double)inSubmergedVolume, (double)inTotalVolume));
		}
	#endif // JPH_DEBUG_RENDERER

		// When buoyancy is 1 we want neutral buoyancy, this means that the density of the liquid is the same as the density of the body at that point.
		// Buoyancy > 1 should make the object float, < 1 should make it sink.
		float inverse_mass = mMotionProperties->GetInverseMass();
		float fluid_density = inBuoyancy / (inTotalVolume * inverse_mass);

		// Buoyancy force = Density of Fluid * Submerged volume * Magnitude of gravity * Up direction (eq 2.5.1)
		// Impulse = Force * Delta time
		// We should apply this at the center of buoyancy (= center of mass of submerged volume)
		Vec3 buoyancy_impulse = -fluid_density * inSubmergedVolume * mMotionProperties->GetGravityFactor() * inGravity * inDeltaTime;

		// Calculate the velocity of the center of buoyancy relative to the fluid
		Vec3 linear_velocity = mMotionProperties->GetLinearVelocity();
		Vec3 angular_velocity = mMotionProperties->GetAngularVelocity();
		Vec3 center_of_buoyancy_velocity = linear_velocity + angular_velocity.Cross(inRelativeCenterOfBuoyancy);
		Vec3 relative_center_of_buoyancy_velocity = inFluidVelocity - center_of_buoyancy_velocity;

		// Here we deviate from the article, instead of eq 2.5.14 we use a quadratic drag formula: https://en.wikipedia.org/wiki/Drag_%28physics%29
		// Drag force = 0.5 * Fluid Density * (Velocity of fluid - Velocity of center of buoyancy)^2 * Linear Drag * Area Facing the Relative Fluid Velocity
		// Again Impulse = Force * Delta Time
		// We should apply this at the center of buoyancy (= center of mass for submerged volume with no center of mass offset)

		// Get size of local bounding box
		Vec3 size = mShape->GetLocalBounds().GetSize();

		// Determine area of the local space bounding box in the direction of the relative velocity between the fluid and the center of buoyancy
		float area = 0.0f;
		float relative_center_of_buoyancy_velocity_len_sq = relative_center_of_buoyancy_velocity.LengthSq();
		if (relative_center_of_buoyancy_velocity_len_sq > 1.0e-12f)
		{
			Vec3 local_relative_center_of_buoyancy_velocity = GetRotation().Conjugated() * relative_center_of_buoyancy_velocity;
			area = local_relative_center_of_buoyancy_velocity.Abs().Dot(size.Swizzle<SWIZZLE_Y, SWIZZLE_Z, SWIZZLE_X>() * size.Swizzle<SWIZZLE_Z, SWIZZLE_X, SWIZZLE_Y>()) / sqrt(relative_center_of_buoyancy_velocity_len_sq);
		}

		// Calculate the impulse
		Vec3 drag_impulse = (0.5f * fluid_density * inLinearDrag * area * inDeltaTime) * relative_center_of_buoyancy_velocity * relative_center_of_buoyancy_velocity.Length();

		// Clamp magnitude against current linear velocity to prevent overshoot
		float linear_velocity_len_sq = linear_velocity.LengthSq();
		float drag_delta_linear_velocity_len_sq = (drag_impulse * inverse_mass).LengthSq();
		if (drag_delta_linear_velocity_len_sq > linear_velocity_len_sq)
			drag_impulse *= sqrt(linear_velocity_len_sq / drag_delta_linear_velocity_len_sq);

		// Calculate the resulting delta linear velocity due to buoyancy and drag
		Vec3 delta_linear_velocity = (drag_impulse + buoyancy_impulse) * inverse_mass;
		mMotionProperties->AddLinearVelocityStep(delta_linear_velocity);

		// Determine average width of the body (across the three axis)
		float l = (size.GetX() + size.GetY() + size.GetZ()) / 3.0f;

		// Drag torque = -Angular Drag * Mass * Submerged volume / Total volume * (Average width of body)^2 * Angular velocity (eq 2.5.15)
		Vec3 drag_angular_impulse = (-inAngularDrag * inSubmergedVolume / inTotalVolume * inDeltaTime * Square(l) / inverse_mass) * angular_velocity;
		Mat44 inv_inertia = GetInverseInertia();
		Vec3 drag_delta_angular_velocity = inv_inertia * drag_angular_impulse;

		// Clamp magnitude against the current angular velocity to prevent overshoot
		float angular_velocity_len_sq = angular_velocity.LengthSq();
		float drag_delta_angular_velocity_len_sq = drag_delta_angular_velocity.LengthSq();
		if (drag_delta_angular_velocity_len_sq > angular_velocity_len_sq)
			drag_delta_angular_velocity *= sqrt(angular_velocity_len_sq / drag_delta_angular_velocity_len_sq);

		// Calculate total delta angular velocity due to drag and buoyancy
		Vec3 delta_angular_velocity = drag_delta_angular_velocity + inv_inertia * inRelativeCenterOfBuoyancy.Cross(buoyancy_impulse + drag_impulse);
		mMotionProperties->AddAngularVelocityStep(delta_angular_velocity);
		return true;
	}

	return false;
}

bool Body::ApplyBuoyancyImpulse(RVec3Arg inSurfacePosition, Vec3Arg inSurfaceNormal, float inBuoyancy, float inLinearDrag, float inAngularDrag, Vec3Arg inFluidVelocity, Vec3Arg inGravity, float inDeltaTime)
{
	JPH_PROFILE_FUNCTION();

	float total_volume, submerged_volume;
	Vec3 relative_center_of_buoyancy;
	GetSubmergedVolume(inSurfacePosition, inSurfaceNormal, total_volume, submerged_volume, relative_center_of_buoyancy);

	return ApplyBuoyancyImpulse(total_volume, submerged_volume, relative_center_of_buoyancy, inBuoyancy, inLinearDrag, inAngularDrag, inFluidVelocity, inGravity, inDeltaTime);
}

void Body::SaveState(StateRecorder &inStream) const
{
	// Only write properties that can change at runtime
	inStream.Write(mPosition);
	inStream.Write(mRotation);

	if (mMotionProperties != nullptr)
	{
		if (IsSoftBody())
			static_cast<const SoftBodyMotionProperties *>(mMotionProperties)->SaveState(inStream);
		else
			mMotionProperties->SaveState(inStream);
	}
}

void Body::RestoreState(StateRecorder &inStream)
{
	inStream.Read(mPosition);
	inStream.Read(mRotation);

	if (mMotionProperties != nullptr)
	{
		if (IsSoftBody())
			static_cast<SoftBodyMotionProperties *>(mMotionProperties)->RestoreState(inStream);
		else
			mMotionProperties->RestoreState(inStream);

		JPH_IF_ENABLE_ASSERTS(mMotionProperties->mCachedMotionType = mMotionType);
	}

	// Initialize bounding box
	CalculateWorldSpaceBoundsInternal();
}

BodyCreationSettings Body::GetBodyCreationSettings() const
{
	JPH_ASSERT(IsRigidBody());

	BodyCreationSettings result;

	result.mPosition = GetPosition();
	result.mRotation = GetRotation();
	result.mLinearVelocity = mMotionProperties != nullptr? mMotionProperties->GetLinearVelocity() : Vec3::sZero();
	result.mAngularVelocity = mMotionProperties != nullptr? mMotionProperties->GetAngularVelocity() : Vec3::sZero();
	result.mObjectLayer = GetObjectLayer();
	result.mUserData = mUserData;
	result.mCollisionGroup = GetCollisionGroup();
	result.mMotionType = GetMotionType();
	result.mAllowedDOFs = mMotionProperties != nullptr? mMotionProperties->GetAllowedDOFs() : EAllowedDOFs::All;
	result.mAllowDynamicOrKinematic = mMotionProperties != nullptr;
	result.mIsSensor = IsSensor();
	result.mCollideKinematicVsNonDynamic = GetCollideKinematicVsNonDynamic();
	result.mUseManifoldReduction = GetUseManifoldReduction();
	result.mApplyGyroscopicForce = GetApplyGyroscopicForce();
	result.mMotionQuality = mMotionProperties != nullptr? mMotionProperties->GetMotionQuality() : EMotionQuality::Discrete;
	result.mEnhancedInternalEdgeRemoval = GetEnhancedInternalEdgeRemoval();
	result.mAllowSleeping = mMotionProperties != nullptr? GetAllowSleeping() : true;
	result.mFriction = GetFriction();
	result.mRestitution = GetRestitution();
	result.mLinearDamping = mMotionProperties != nullptr? mMotionProperties->GetLinearDamping() : 0.0f;
	result.mAngularDamping = mMotionProperties != nullptr? mMotionProperties->GetAngularDamping() : 0.0f;
	result.mMaxLinearVelocity = mMotionProperties != nullptr? mMotionProperties->GetMaxLinearVelocity() : 0.0f;
	result.mMaxAngularVelocity = mMotionProperties != nullptr? mMotionProperties->GetMaxAngularVelocity() : 0.0f;
	result.mGravityFactor = mMotionProperties != nullptr? mMotionProperties->GetGravityFactor() : 1.0f;
	result.mNumVelocityStepsOverride = mMotionProperties != nullptr? mMotionProperties->GetNumVelocityStepsOverride() : 0;
	result.mNumPositionStepsOverride = mMotionProperties != nullptr? mMotionProperties->GetNumPositionStepsOverride() : 0;
	result.mOverrideMassProperties = EOverrideMassProperties::MassAndInertiaProvided;

	// Invert inertia and mass
	if (mMotionProperties != nullptr)
	{
		float inv_mass = mMotionProperties->GetInverseMassUnchecked();
		Mat44 inv_inertia = mMotionProperties->GetLocalSpaceInverseInertiaUnchecked();

		// Get mass
		result.mMassPropertiesOverride.mMass = inv_mass != 0.0f? 1.0f / inv_mass : FLT_MAX;

		// Get inertia
		Mat44 inertia;
		if (inertia.SetInversed3x3(inv_inertia))
		{
			// Inertia was invertible, we can use it
			result.mMassPropertiesOverride.mInertia = inertia;
		}
		else
		{
			// Prevent division by zero
			Vec3 diagonal = Vec3::sMax(inv_inertia.GetDiagonal3(), Vec3::sReplicate(FLT_MIN));
			result.mMassPropertiesOverride.mInertia = Mat44::sScale(diagonal.Reciprocal());
		}
	}
	else
	{
		result.mMassPropertiesOverride.mMass = FLT_MAX;
		result.mMassPropertiesOverride.mInertia = Mat44::sScale(Vec3::sReplicate(FLT_MAX));
	}

	result.SetShape(GetShape());

	return result;
}

SoftBodyCreationSettings Body::GetSoftBodyCreationSettings() const
{
	JPH_ASSERT(IsSoftBody());

	SoftBodyCreationSettings result;

	result.mPosition = GetPosition();
	result.mRotation = GetRotation();
	result.mUserData = mUserData;
	result.mObjectLayer = GetObjectLayer();
	result.mCollisionGroup = GetCollisionGroup();
	result.mFriction = GetFriction();
	result.mRestitution = GetRestitution();
	const SoftBodyMotionProperties *mp = static_cast<const SoftBodyMotionProperties *>(mMotionProperties);
	result.mNumIterations = mp->GetNumIterations();
	result.mLinearDamping = mp->GetLinearDamping();
	result.mMaxLinearVelocity = mp->GetMaxLinearVelocity();
	result.mGravityFactor = mp->GetGravityFactor();
	result.mPressure = mp->GetPressure();
	result.mUpdatePosition = mp->GetUpdatePosition();
	result.mSettings = mp->GetSettings();

	return result;
}

JPH_NAMESPACE_END

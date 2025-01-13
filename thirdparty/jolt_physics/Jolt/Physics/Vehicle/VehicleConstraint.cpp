// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#include <Jolt/Jolt.h>

#include <Jolt/Physics/Vehicle/VehicleConstraint.h>
#include <Jolt/Physics/Vehicle/VehicleController.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/ObjectStream/TypeDeclarations.h>
#include <Jolt/Core/StreamIn.h>
#include <Jolt/Core/StreamOut.h>
#include <Jolt/Core/Factory.h>

JPH_NAMESPACE_BEGIN

JPH_IMPLEMENT_SERIALIZABLE_VIRTUAL(VehicleConstraintSettings)
{
	JPH_ADD_BASE_CLASS(VehicleConstraintSettings, ConstraintSettings)

	JPH_ADD_ATTRIBUTE(VehicleConstraintSettings, mUp)
	JPH_ADD_ATTRIBUTE(VehicleConstraintSettings, mForward)
	JPH_ADD_ATTRIBUTE(VehicleConstraintSettings, mMaxPitchRollAngle)
	JPH_ADD_ATTRIBUTE(VehicleConstraintSettings, mWheels)
	JPH_ADD_ATTRIBUTE(VehicleConstraintSettings, mAntiRollBars)
	JPH_ADD_ATTRIBUTE(VehicleConstraintSettings, mController)
}

void VehicleConstraintSettings::SaveBinaryState(StreamOut &inStream) const
{
	ConstraintSettings::SaveBinaryState(inStream);

	inStream.Write(mUp);
	inStream.Write(mForward);
	inStream.Write(mMaxPitchRollAngle);

	uint32 num_anti_rollbars = (uint32)mAntiRollBars.size();
	inStream.Write(num_anti_rollbars);
	for (const VehicleAntiRollBar &r : mAntiRollBars)
		r.SaveBinaryState(inStream);

	uint32 num_wheels = (uint32)mWheels.size();
	inStream.Write(num_wheels);
	for (const WheelSettings *w : mWheels)
		w->SaveBinaryState(inStream);

	inStream.Write(mController->GetRTTI()->GetHash());
	mController->SaveBinaryState(inStream);
}

void VehicleConstraintSettings::RestoreBinaryState(StreamIn &inStream)
{
	ConstraintSettings::RestoreBinaryState(inStream);

	inStream.Read(mUp);
	inStream.Read(mForward);
	inStream.Read(mMaxPitchRollAngle);

	uint32 num_anti_rollbars = 0;
	inStream.Read(num_anti_rollbars);
	mAntiRollBars.resize(num_anti_rollbars);
	for (VehicleAntiRollBar &r : mAntiRollBars)
		r.RestoreBinaryState(inStream);

	uint32 num_wheels = 0;
	inStream.Read(num_wheels);
	mWheels.resize(num_wheels);
	for (WheelSettings *w : mWheels)
		w->RestoreBinaryState(inStream);

	uint32 hash = 0;
	inStream.Read(hash);
	const RTTI *rtti = Factory::sInstance->Find(hash);
	mController = reinterpret_cast<VehicleControllerSettings *>(rtti->CreateObject());
	mController->RestoreBinaryState(inStream);
}

VehicleConstraint::VehicleConstraint(Body &inVehicleBody, const VehicleConstraintSettings &inSettings) :
	Constraint(inSettings),
	mBody(&inVehicleBody),
	mForward(inSettings.mForward),
	mUp(inSettings.mUp),
	mWorldUp(inSettings.mUp)
{
	// Check sanity of incoming settings
	JPH_ASSERT(inSettings.mUp.IsNormalized());
	JPH_ASSERT(inSettings.mForward.IsNormalized());
	JPH_ASSERT(!inSettings.mWheels.empty());

	// Store max pitch/roll angle
	SetMaxPitchRollAngle(inSettings.mMaxPitchRollAngle);

	// Copy anti-rollbar settings
	mAntiRollBars.resize(inSettings.mAntiRollBars.size());
	for (uint i = 0; i < mAntiRollBars.size(); ++i)
	{
		const VehicleAntiRollBar &r = inSettings.mAntiRollBars[i];
		mAntiRollBars[i] = r;
		JPH_ASSERT(r.mStiffness >= 0.0f);
	}

	// Construct our controller class
	mController = inSettings.mController->ConstructController(*this);

	// Create wheels
	mWheels.resize(inSettings.mWheels.size());
	for (uint i = 0; i < mWheels.size(); ++i)
		mWheels[i] = mController->ConstructWheel(*inSettings.mWheels[i]);

	// Use the body ID as a seed for the step counter so that not all vehicles will update at the same time
	mCurrentStep = uint32(Hash64(inVehicleBody.GetID().GetIndex()));
}

VehicleConstraint::~VehicleConstraint()
{
	// Destroy controller
	delete mController;

	// Destroy our wheels
	for (Wheel *w : mWheels)
		delete w;
}

void VehicleConstraint::GetWheelLocalBasis(const Wheel *inWheel, Vec3 &outForward, Vec3 &outUp, Vec3 &outRight) const
{
	const WheelSettings *settings = inWheel->mSettings;

	Quat steer_rotation = Quat::sRotation(settings->mSteeringAxis, inWheel->mSteerAngle);
	outUp = steer_rotation * settings->mWheelUp;
	outForward = steer_rotation * settings->mWheelForward;
	outRight = outForward.Cross(outUp).Normalized();
	outForward = outUp.Cross(outRight).Normalized();
}

Mat44 VehicleConstraint::GetWheelLocalTransform(uint inWheelIndex, Vec3Arg inWheelRight, Vec3Arg inWheelUp) const
{
	JPH_ASSERT(inWheelIndex < mWheels.size());

	const Wheel *wheel = mWheels[inWheelIndex];
	const WheelSettings *settings = wheel->mSettings;

	// Use the two vectors provided to calculate a matrix that takes us from wheel model space to X = right, Y = up, Z = forward (the space where we will rotate the wheel)
	Mat44 wheel_to_rotational = Mat44(Vec4(inWheelRight, 0), Vec4(inWheelUp, 0), Vec4(inWheelUp.Cross(inWheelRight), 0), Vec4(0, 0, 0, 1)).Transposed();

	// Calculate the matrix that takes us from the rotational space to vehicle local space
	Vec3 local_forward, local_up, local_right;
	GetWheelLocalBasis(wheel, local_forward, local_up, local_right);
	Vec3 local_wheel_pos = settings->mPosition + settings->mSuspensionDirection * wheel->mSuspensionLength;
	Mat44 rotational_to_local(Vec4(local_right, 0), Vec4(local_up, 0), Vec4(local_forward, 0), Vec4(local_wheel_pos, 1));

	// Calculate transform of rotated wheel
	return rotational_to_local * Mat44::sRotationX(wheel->mAngle) * wheel_to_rotational;
}

RMat44 VehicleConstraint::GetWheelWorldTransform(uint inWheelIndex, Vec3Arg inWheelRight, Vec3Arg inWheelUp) const
{
	return mBody->GetWorldTransform() * GetWheelLocalTransform(inWheelIndex, inWheelRight, inWheelUp);
}

void VehicleConstraint::OnStep(const PhysicsStepListenerContext &inContext)
{
	JPH_PROFILE_FUNCTION();

	// Callback to higher-level systems. We do it before PreCollide, in case steering changes.
	if (mPreStepCallback != nullptr)
		mPreStepCallback(*this, inContext);

	if (mIsGravityOverridden)
	{
		// If gravity is overridden, we replace the normal gravity calculations
		if (mBody->IsActive())
		{
			MotionProperties *mp = mBody->GetMotionProperties();
			mp->SetGravityFactor(0.0f);
			mBody->AddForce(mGravityOverride / mp->GetInverseMass());
		}

		// And we calculate the world up using the custom gravity
		mWorldUp = (-mGravityOverride).NormalizedOr(mWorldUp);
	}
	else
	{
		// Calculate new world up vector by inverting gravity
		mWorldUp = (-inContext.mPhysicsSystem->GetGravity()).NormalizedOr(mWorldUp);
	}

	// Callback on our controller
	mController->PreCollide(inContext.mDeltaTime, *inContext.mPhysicsSystem);

	// Calculate if this constraint is active by checking if our main vehicle body is active or any of the bodies we touch are active
	mIsActive = mBody->IsActive();

	// Test how often we need to update the wheels
	uint num_steps_between_collisions = mIsActive? mNumStepsBetweenCollisionTestActive : mNumStepsBetweenCollisionTestInactive;

	RMat44 body_transform = mBody->GetWorldTransform();

	// Test collision for wheels
	for (uint wheel_index = 0; wheel_index < mWheels.size(); ++wheel_index)
	{
		Wheel *w = mWheels[wheel_index];
		const WheelSettings *settings = w->mSettings;

		// Calculate suspension origin and direction
		RVec3 ws_origin = body_transform * settings->mPosition;
		Vec3 ws_direction = body_transform.Multiply3x3(settings->mSuspensionDirection);

		// Test if we need to update this wheel
		if (num_steps_between_collisions == 0
			|| (mCurrentStep + wheel_index) % num_steps_between_collisions != 0)
		{
			// Simplified wheel contact test
			if (!w->mContactBodyID.IsInvalid())
			{
				// Test if the body is still valid
				w->mContactBody = inContext.mPhysicsSystem->GetBodyLockInterfaceNoLock().TryGetBody(w->mContactBodyID);
				if (w->mContactBody == nullptr)
				{
					// It's not, forget the contact
					w->mContactBodyID = BodyID();
					w->mContactSubShapeID = SubShapeID();
					w->mSuspensionLength = settings->mSuspensionMaxLength;
				}
				else
				{
					// Extrapolate the wheel contact properties
					mVehicleCollisionTester->PredictContactProperties(*inContext.mPhysicsSystem, *this, wheel_index, ws_origin, ws_direction, mBody->GetID(), w->mContactBody, w->mContactSubShapeID, w->mContactPosition, w->mContactNormal, w->mSuspensionLength);
				}
			}
		}
		else
		{
			// Full wheel contact test, start by resetting the contact data
			w->mContactBodyID = BodyID();
			w->mContactBody = nullptr;
			w->mContactSubShapeID = SubShapeID();
			w->mSuspensionLength = settings->mSuspensionMaxLength;

			// Test collision to find the floor
			if (mVehicleCollisionTester->Collide(*inContext.mPhysicsSystem, *this, wheel_index, ws_origin, ws_direction, mBody->GetID(), w->mContactBody, w->mContactSubShapeID, w->mContactPosition, w->mContactNormal, w->mSuspensionLength))
			{
				// Store ID (pointer is not valid outside of the simulation step)
				w->mContactBodyID = w->mContactBody->GetID();
			}
		}

		if (w->mContactBody != nullptr)
		{
			// Store contact velocity, cache this as the contact body may be removed
			w->mContactPointVelocity = w->mContactBody->GetPointVelocity(w->mContactPosition);

			// Determine plane constant for axle contact plane
			w->mAxlePlaneConstant = RVec3(w->mContactNormal).Dot(ws_origin + w->mSuspensionLength * ws_direction);

			// Check if body is active, if so the entire vehicle should be active
			mIsActive |= w->mContactBody->IsActive();

			// Determine world space forward using steering angle and body rotation
			Vec3 forward, up, right;
			GetWheelLocalBasis(w, forward, up, right);
			forward = body_transform.Multiply3x3(forward);
			right = body_transform.Multiply3x3(right);

			// The longitudinal axis is in the up/forward plane
			w->mContactLongitudinal = w->mContactNormal.Cross(right);

			// Make sure that the longitudinal axis is aligned with the forward axis
			if (w->mContactLongitudinal.Dot(forward) < 0.0f)
				w->mContactLongitudinal = -w->mContactLongitudinal;

			// Normalize it
			w->mContactLongitudinal = w->mContactLongitudinal.NormalizedOr(w->mContactNormal.GetNormalizedPerpendicular());

			// The lateral axis is perpendicular to contact normal and longitudinal axis
			w->mContactLateral = w->mContactLongitudinal.Cross(w->mContactNormal).Normalized();
		}
	}

	// Callback to higher-level systems. We do it immediately after wheel collision.
	if (mPostCollideCallback != nullptr)
		mPostCollideCallback(*this, inContext);

	// Calculate anti-rollbar impulses
	for (const VehicleAntiRollBar &r : mAntiRollBars)
	{
		Wheel *lw = mWheels[r.mLeftWheel];
		Wheel *rw = mWheels[r.mRightWheel];

		if (lw->mContactBody != nullptr && rw->mContactBody != nullptr)
		{
			// Calculate the impulse to apply based on the difference in suspension length
			float difference = rw->mSuspensionLength - lw->mSuspensionLength;
			float impulse = difference * r.mStiffness * inContext.mDeltaTime;
			lw->mAntiRollBarImpulse = -impulse;
			rw->mAntiRollBarImpulse = impulse;
		}
		else
		{
			// When one of the wheels is not on the ground we don't apply any impulses
			lw->mAntiRollBarImpulse = rw->mAntiRollBarImpulse = 0.0f;
		}
	}

	// Callback on our controller
	mController->PostCollide(inContext.mDeltaTime, *inContext.mPhysicsSystem);

	// Callback to higher-level systems. We do it before the sleep section, in case velocities change.
	if (mPostStepCallback != nullptr)
		mPostStepCallback(*this, inContext);

	// If the wheels are rotating, we don't want to go to sleep yet
	bool allow_sleep = mController->AllowSleep();
	if (allow_sleep)
		for (const Wheel *w : mWheels)
			if (abs(w->mAngularVelocity) > DegreesToRadians(10.0f))
			{
				allow_sleep = false;
				break;
			}
	if (mBody->GetAllowSleeping() != allow_sleep)
		mBody->SetAllowSleeping(allow_sleep);

	// Increment step counter
	++mCurrentStep;
}

void VehicleConstraint::BuildIslands(uint32 inConstraintIndex, IslandBuilder &ioBuilder, BodyManager &inBodyManager)
{
	// Find dynamic bodies that our wheels are touching
	BodyID *body_ids = (BodyID *)JPH_STACK_ALLOC((mWheels.size() + 1) * sizeof(BodyID));
	int num_bodies = 0;
	bool needs_to_activate = false;
	for (const Wheel *w : mWheels)
		if (w->mContactBody != nullptr)
		{
			// Avoid adding duplicates
			bool duplicate = false;
			BodyID id = w->mContactBody->GetID();
			for (int i = 0; i < num_bodies; ++i)
				if (body_ids[i] == id)
				{
					duplicate = true;
					break;
				}
			if (duplicate)
				continue;

			if (w->mContactBody->IsDynamic())
			{
				body_ids[num_bodies++] = id;
				needs_to_activate |= !w->mContactBody->IsActive();
			}
		}

	// Activate bodies, note that if we get here we have already told the system that we're active so that means our main body needs to be active too
	if (!mBody->IsActive())
	{
		// Our main body is not active, activate it too
		body_ids[num_bodies] = mBody->GetID();
		inBodyManager.ActivateBodies(body_ids, num_bodies + 1);
	}
	else if (needs_to_activate)
	{
		// Only activate bodies the wheels are touching
		inBodyManager.ActivateBodies(body_ids, num_bodies);
	}

	// Link the bodies into the same island
	uint32 min_active_index = Body::cInactiveIndex;
	for (int i = 0; i < num_bodies; ++i)
	{
		const Body &body = inBodyManager.GetBody(body_ids[i]);
		min_active_index = min(min_active_index, body.GetIndexInActiveBodiesInternal());
		ioBuilder.LinkBodies(mBody->GetIndexInActiveBodiesInternal(), body.GetIndexInActiveBodiesInternal());
	}

	// Link the constraint in the island
	ioBuilder.LinkConstraint(inConstraintIndex, mBody->GetIndexInActiveBodiesInternal(), min_active_index);
}

uint VehicleConstraint::BuildIslandSplits(LargeIslandSplitter &ioSplitter) const
{
	return ioSplitter.AssignToNonParallelSplit(mBody);
}

void VehicleConstraint::CalculateSuspensionForcePoint(const Wheel &inWheel, Vec3 &outR1PlusU, Vec3 &outR2) const
{
	// Determine point to apply force to
	RVec3 force_point;
	if (inWheel.mSettings->mEnableSuspensionForcePoint)
		force_point = mBody->GetWorldTransform() * inWheel.mSettings->mSuspensionForcePoint;
	else
		force_point = inWheel.mContactPosition;

	// Calculate r1 + u and r2
	outR1PlusU = Vec3(force_point - mBody->GetCenterOfMassPosition());
	outR2 = Vec3(force_point - inWheel.mContactBody->GetCenterOfMassPosition());
}

void VehicleConstraint::CalculatePitchRollConstraintProperties(RMat44Arg inBodyTransform)
{
	// Check if a limit was specified
	if (mCosMaxPitchRollAngle > -1.0f)
	{
		// Calculate cos of angle between world up vector and vehicle up vector
		Vec3 vehicle_up = inBodyTransform.Multiply3x3(mUp);
		mCosPitchRollAngle = mWorldUp.Dot(vehicle_up);
		if (mCosPitchRollAngle < mCosMaxPitchRollAngle)
		{
			// Calculate rotation axis to rotate vehicle towards up
			Vec3 rotation_axis = mWorldUp.Cross(vehicle_up);
			float len = rotation_axis.Length();
			if (len > 0.0f)
				mPitchRollRotationAxis = rotation_axis / len;

			mPitchRollPart.CalculateConstraintProperties(*mBody, Body::sFixedToWorld, mPitchRollRotationAxis);
		}
		else
			mPitchRollPart.Deactivate();
	}
	else
		mPitchRollPart.Deactivate();
}

void VehicleConstraint::SetupVelocityConstraint(float inDeltaTime)
{
	RMat44 body_transform = mBody->GetWorldTransform();

	for (Wheel *w : mWheels)
		if (w->mContactBody != nullptr)
		{
			const WheelSettings *settings = w->mSettings;

			Vec3 neg_contact_normal = -w->mContactNormal;

			Vec3 r1_plus_u, r2;
			CalculateSuspensionForcePoint(*w, r1_plus_u, r2);

			// Suspension spring
			if (settings->mSuspensionMaxLength > settings->mSuspensionMinLength)
			{
				float stiffness, damping;
				if (settings->mSuspensionSpring.mMode == ESpringMode::FrequencyAndDamping)
				{
					// Calculate effective mass based on vehicle configuration (the stiffness of the spring should not be affected by the dynamics of the vehicle): K = 1 / (J M^-1 J^T)
					// Note that if no suspension force point is supplied we don't know where the force is applied so we assume it is applied at average suspension length
					Vec3 force_point = settings->mEnableSuspensionForcePoint? settings->mSuspensionForcePoint : settings->mPosition + 0.5f * (settings->mSuspensionMinLength + settings->mSuspensionMaxLength) * settings->mSuspensionDirection;
					Vec3 force_point_x_neg_up = force_point.Cross(-mUp);
					const MotionProperties *mp = mBody->GetMotionProperties();
					float effective_mass = 1.0f / (mp->GetInverseMass() + force_point_x_neg_up.Dot(mp->GetLocalSpaceInverseInertia().Multiply3x3(force_point_x_neg_up)));

					// Convert frequency and damping to stiffness and damping
					float omega = 2.0f * JPH_PI * settings->mSuspensionSpring.mFrequency;
					stiffness = effective_mass * Square(omega);
					damping = 2.0f * effective_mass * settings->mSuspensionSpring.mDamping * omega;
				}
				else
				{
					// In this case we can simply copy the properties
					stiffness = settings->mSuspensionSpring.mStiffness;
					damping = settings->mSuspensionSpring.mDamping;
				}

				// Calculate the damping and frequency of the suspension spring given the angle between the suspension direction and the contact normal
				// If the angle between the suspension direction and the inverse of the contact normal is alpha then the force on the spring relates to the force along the contact normal as:
				//
				// Fspring = Fnormal * cos(alpha)
				//
				// The spring force is:
				//
				// Fspring = -k * x
				//
				// where k is the spring constant and x is the displacement of the spring. So we have:
				//
				// Fnormal * cos(alpha) = -k * x <=> Fnormal = -k / cos(alpha) * x
				//
				// So we can see this as a spring with spring constant:
				//
				// k' = k / cos(alpha)
				//
				// In the same way the velocity relates like:
				//
				// Vspring = Vnormal * cos(alpha)
				//
				// Which results in the modified damping constant c:
				//
				// c' = c / cos(alpha)
				//
				// Note that we clamp 1 / cos(alpha) to the range [0.1, 1] in order not to increase the stiffness / damping by too much.
				Vec3 ws_direction = body_transform.Multiply3x3(settings->mSuspensionDirection);
				float cos_angle = max(0.1f, ws_direction.Dot(neg_contact_normal));
				stiffness /= cos_angle;
				damping /= cos_angle;

				// Get the value of the constraint equation
				float c = w->mSuspensionLength - settings->mSuspensionMaxLength - settings->mSuspensionPreloadLength;

				w->mSuspensionPart.CalculateConstraintPropertiesWithStiffnessAndDamping(inDeltaTime, *mBody, r1_plus_u, *w->mContactBody, r2, neg_contact_normal, w->mAntiRollBarImpulse, c, stiffness, damping);
			}
			else
				w->mSuspensionPart.Deactivate();

			// Check if we reached the 'max up' position and if so add a hard velocity constraint that stops any further movement in the normal direction
			if (w->mSuspensionLength < settings->mSuspensionMinLength)
				w->mSuspensionMaxUpPart.CalculateConstraintProperties(*mBody, r1_plus_u, *w->mContactBody, r2, neg_contact_normal);
			else
				w->mSuspensionMaxUpPart.Deactivate();

			// Friction and propulsion
			w->mLongitudinalPart.CalculateConstraintProperties(*mBody, r1_plus_u, *w->mContactBody, r2, -w->mContactLongitudinal);
			w->mLateralPart.CalculateConstraintProperties(*mBody, r1_plus_u, *w->mContactBody, r2, -w->mContactLateral);
		}
		else
		{
			// No contact -> disable everything
			w->mSuspensionPart.Deactivate();
			w->mSuspensionMaxUpPart.Deactivate();
			w->mLongitudinalPart.Deactivate();
			w->mLateralPart.Deactivate();
		}

	CalculatePitchRollConstraintProperties(body_transform);
}

void VehicleConstraint::ResetWarmStart()
{
	for (Wheel *w : mWheels)
	{
		w->mSuspensionPart.Deactivate();
		w->mSuspensionMaxUpPart.Deactivate();
		w->mLongitudinalPart.Deactivate();
		w->mLateralPart.Deactivate();
	}

	mPitchRollPart.Deactivate();
}

void VehicleConstraint::WarmStartVelocityConstraint(float inWarmStartImpulseRatio)
{
	for (Wheel *w : mWheels)
		if (w->mContactBody != nullptr)
		{
			Vec3 neg_contact_normal = -w->mContactNormal;

			w->mSuspensionPart.WarmStart(*mBody, *w->mContactBody, neg_contact_normal, inWarmStartImpulseRatio);
			w->mSuspensionMaxUpPart.WarmStart(*mBody, *w->mContactBody, neg_contact_normal, inWarmStartImpulseRatio);
			w->mLongitudinalPart.WarmStart(*mBody, *w->mContactBody, -w->mContactLongitudinal, 0.0f); // Don't warm start the longitudinal part (the engine/brake force, we don't want to preserve anything from the last frame)
			w->mLateralPart.WarmStart(*mBody, *w->mContactBody, -w->mContactLateral, inWarmStartImpulseRatio);
		}

	mPitchRollPart.WarmStart(*mBody, Body::sFixedToWorld, inWarmStartImpulseRatio);
}

bool VehicleConstraint::SolveVelocityConstraint(float inDeltaTime)
{
	bool impulse = false;

	// Solve suspension
	for (Wheel *w : mWheels)
		if (w->mContactBody != nullptr)
		{
			Vec3 neg_contact_normal = -w->mContactNormal;

			// Suspension spring, note that it can only push and not pull
			if (w->mSuspensionPart.IsActive())
				impulse |= w->mSuspensionPart.SolveVelocityConstraint(*mBody, *w->mContactBody, neg_contact_normal, 0.0f, FLT_MAX);

			// When reaching the minimal suspension length only allow forces pushing the bodies away
			if (w->mSuspensionMaxUpPart.IsActive())
				impulse |= w->mSuspensionMaxUpPart.SolveVelocityConstraint(*mBody, *w->mContactBody, neg_contact_normal, 0.0f, FLT_MAX);
		}

	// Solve the horizontal movement of the vehicle
	impulse |= mController->SolveLongitudinalAndLateralConstraints(inDeltaTime);

	// Apply the pitch / roll constraint to avoid the vehicle from toppling over
	if (mPitchRollPart.IsActive())
		impulse |= mPitchRollPart.SolveVelocityConstraint(*mBody, Body::sFixedToWorld, mPitchRollRotationAxis, 0, FLT_MAX);

	return impulse;
}

bool VehicleConstraint::SolvePositionConstraint(float inDeltaTime, float inBaumgarte)
{
	bool impulse = false;

	RMat44 body_transform = mBody->GetWorldTransform();

	for (Wheel *w : mWheels)
		if (w->mContactBody != nullptr)
		{
			const WheelSettings *settings = w->mSettings;

			// Check if we reached the 'max up' position now that the body has possibly moved
			// We do this by calculating the axle position at minimum suspension length and making sure it does not go through the
			// plane defined by the contact normal and the axle position when the contact happened
			// TODO: This assumes that only the vehicle moved and not the ground as we kept the axle contact plane in world space
			Vec3 ws_direction = body_transform.Multiply3x3(settings->mSuspensionDirection);
			RVec3 ws_position = body_transform * settings->mPosition;
			RVec3 min_suspension_pos = ws_position + settings->mSuspensionMinLength * ws_direction;
			float max_up_error = float(RVec3(w->mContactNormal).Dot(min_suspension_pos) - w->mAxlePlaneConstant);
			if (max_up_error < 0.0f)
			{
				Vec3 neg_contact_normal = -w->mContactNormal;

				// Recalculate constraint properties since the body may have moved
				Vec3 r1_plus_u, r2;
				CalculateSuspensionForcePoint(*w, r1_plus_u, r2);
				w->mSuspensionMaxUpPart.CalculateConstraintProperties(*mBody, r1_plus_u, *w->mContactBody, r2, neg_contact_normal);

				impulse |= w->mSuspensionMaxUpPart.SolvePositionConstraint(*mBody, *w->mContactBody, neg_contact_normal, max_up_error, inBaumgarte);
			}
		}

	// Apply the pitch / roll constraint to avoid the vehicle from toppling over
	CalculatePitchRollConstraintProperties(body_transform);
	if (mPitchRollPart.IsActive())
		impulse |= mPitchRollPart.SolvePositionConstraint(*mBody, Body::sFixedToWorld, mCosPitchRollAngle - mCosMaxPitchRollAngle, inBaumgarte);

	return impulse;
}

#ifdef JPH_DEBUG_RENDERER

void VehicleConstraint::DrawConstraint(DebugRenderer *inRenderer) const
{
	mController->Draw(inRenderer);
}

void VehicleConstraint::DrawConstraintLimits(DebugRenderer *inRenderer) const
{
}

#endif // JPH_DEBUG_RENDERER

void VehicleConstraint::SaveState(StateRecorder &inStream) const
{
	Constraint::SaveState(inStream);

	mController->SaveState(inStream);

	for (const Wheel *w : mWheels)
	{
		inStream.Write(w->mAngularVelocity);
		inStream.Write(w->mAngle);
		inStream.Write(w->mContactBodyID); // Used by MotorcycleController::PreCollide
		inStream.Write(w->mContactPosition); // Used by VehicleCollisionTester::PredictContactProperties
		inStream.Write(w->mContactNormal); // Used by MotorcycleController::PreCollide
		inStream.Write(w->mContactLateral); // Used by MotorcycleController::PreCollide
		inStream.Write(w->mSuspensionLength); // Used by VehicleCollisionTester::PredictContactProperties

		w->mSuspensionPart.SaveState(inStream);
		w->mSuspensionMaxUpPart.SaveState(inStream);
		w->mLongitudinalPart.SaveState(inStream);
		w->mLateralPart.SaveState(inStream);
	}

	inStream.Write(mPitchRollRotationAxis); // When rotation is too small we use last frame so we need to store it
	mPitchRollPart.SaveState(inStream);
	inStream.Write(mCurrentStep);
}

void VehicleConstraint::RestoreState(StateRecorder &inStream)
{
	Constraint::RestoreState(inStream);

	mController->RestoreState(inStream);

	for (Wheel *w : mWheels)
	{
		inStream.Read(w->mAngularVelocity);
		inStream.Read(w->mAngle);
		inStream.Read(w->mContactBodyID);
		inStream.Read(w->mContactPosition);
		inStream.Read(w->mContactNormal);
		inStream.Read(w->mContactLateral);
		inStream.Read(w->mSuspensionLength);
		w->mContactBody = nullptr; // No longer valid

		w->mSuspensionPart.RestoreState(inStream);
		w->mSuspensionMaxUpPart.RestoreState(inStream);
		w->mLongitudinalPart.RestoreState(inStream);
		w->mLateralPart.RestoreState(inStream);
	}

	inStream.Read(mPitchRollRotationAxis);
	mPitchRollPart.RestoreState(inStream);
	inStream.Read(mCurrentStep);
}

Ref<ConstraintSettings> VehicleConstraint::GetConstraintSettings() const
{
	JPH_ASSERT(false); // Not implemented yet
	return nullptr;
}

JPH_NAMESPACE_END

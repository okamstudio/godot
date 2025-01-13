// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#include <Jolt/Jolt.h>

#include <Jolt/Physics/Vehicle/TrackedVehicleController.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/ObjectStream/TypeDeclarations.h>
#include <Jolt/Core/StreamIn.h>
#include <Jolt/Core/StreamOut.h>
#ifdef JPH_DEBUG_RENDERER
	#include <Jolt/Renderer/DebugRenderer.h>
#endif // JPH_DEBUG_RENDERER

JPH_NAMESPACE_BEGIN

JPH_IMPLEMENT_SERIALIZABLE_VIRTUAL(TrackedVehicleControllerSettings)
{
	JPH_ADD_BASE_CLASS(TrackedVehicleControllerSettings, VehicleControllerSettings)

	JPH_ADD_ATTRIBUTE(TrackedVehicleControllerSettings, mEngine)
	JPH_ADD_ATTRIBUTE(TrackedVehicleControllerSettings, mTransmission)
	JPH_ADD_ATTRIBUTE(TrackedVehicleControllerSettings, mTracks)
}

JPH_IMPLEMENT_SERIALIZABLE_VIRTUAL(WheelSettingsTV)
{
	JPH_ADD_ATTRIBUTE(WheelSettingsTV, mLongitudinalFriction)
	JPH_ADD_ATTRIBUTE(WheelSettingsTV, mLateralFriction)
}

void WheelSettingsTV::SaveBinaryState(StreamOut &inStream) const
{
	inStream.Write(mLongitudinalFriction);
	inStream.Write(mLateralFriction);
}

void WheelSettingsTV::RestoreBinaryState(StreamIn &inStream)
{
	inStream.Read(mLongitudinalFriction);
	inStream.Read(mLateralFriction);
}

WheelTV::WheelTV(const WheelSettingsTV &inSettings) :
	Wheel(inSettings)
{
}

void WheelTV::CalculateAngularVelocity(const VehicleConstraint &inConstraint)
{
	const WheelSettingsTV *settings = GetSettings();
	const Wheels &wheels = inConstraint.GetWheels();
	const VehicleTrack &track = static_cast<const TrackedVehicleController *>(inConstraint.GetController())->GetTracks()[mTrackIndex];

	// Calculate angular velocity of this wheel
	mAngularVelocity = track.mAngularVelocity * wheels[track.mDrivenWheel]->GetSettings()->mRadius / settings->mRadius;
}

void WheelTV::Update(uint inWheelIndex, float inDeltaTime, const VehicleConstraint &inConstraint)
{
	CalculateAngularVelocity(inConstraint);

	// Update rotation of wheel
	mAngle = fmod(mAngle + mAngularVelocity * inDeltaTime, 2.0f * JPH_PI);

	// Reset brake impulse, will be set during post collision again
	mBrakeImpulse = 0.0f;

	if (mContactBody != nullptr)
	{
		// Friction at the point of this wheel between track and floor
		const WheelSettingsTV *settings = GetSettings();
		VehicleConstraint::CombineFunction combine_friction = inConstraint.GetCombineFriction();
		mCombinedLongitudinalFriction = settings->mLongitudinalFriction;
		mCombinedLateralFriction = settings->mLateralFriction;
		combine_friction(inWheelIndex, mCombinedLongitudinalFriction, mCombinedLateralFriction, *mContactBody, mContactSubShapeID);
	}
	else
	{
		// No collision
		mCombinedLongitudinalFriction = mCombinedLateralFriction = 0.0f;
	}
}

VehicleController *TrackedVehicleControllerSettings::ConstructController(VehicleConstraint &inConstraint) const
{
	return new TrackedVehicleController(*this, inConstraint);
}

TrackedVehicleControllerSettings::TrackedVehicleControllerSettings()
{
	// Numbers guestimated from: https://en.wikipedia.org/wiki/M1_Abrams
	mEngine.mMinRPM = 500.0f;
	mEngine.mMaxRPM = 4000.0f;
	mEngine.mMaxTorque = 500.0f; // Note actual torque for M1 is around 5000 but we need a reduced mass in order to keep the simulation sane

	mTransmission.mShiftDownRPM = 1000.0f;
	mTransmission.mShiftUpRPM = 3500.0f;
	mTransmission.mGearRatios = { 4.0f, 3.0f, 2.0f, 1.0f };
	mTransmission.mReverseGearRatios = { -4.0f, -3.0f };
}

void TrackedVehicleControllerSettings::SaveBinaryState(StreamOut &inStream) const
{
	mEngine.SaveBinaryState(inStream);

	mTransmission.SaveBinaryState(inStream);

	for (const VehicleTrackSettings &t : mTracks)
		t.SaveBinaryState(inStream);
}

void TrackedVehicleControllerSettings::RestoreBinaryState(StreamIn &inStream)
{
	mEngine.RestoreBinaryState(inStream);

	mTransmission.RestoreBinaryState(inStream);

	for (VehicleTrackSettings &t : mTracks)
		t.RestoreBinaryState(inStream);
}

TrackedVehicleController::TrackedVehicleController(const TrackedVehicleControllerSettings &inSettings, VehicleConstraint &inConstraint) :
	VehicleController(inConstraint)
{
	// Copy engine settings
	static_cast<VehicleEngineSettings &>(mEngine) = inSettings.mEngine;
	JPH_ASSERT(inSettings.mEngine.mMinRPM >= 0.0f);
	JPH_ASSERT(inSettings.mEngine.mMinRPM <= inSettings.mEngine.mMaxRPM);
	mEngine.SetCurrentRPM(mEngine.mMinRPM);

	// Copy transmission settings
	static_cast<VehicleTransmissionSettings &>(mTransmission) = inSettings.mTransmission;
#ifdef JPH_ENABLE_ASSERTS
	for (float r : inSettings.mTransmission.mGearRatios)
		JPH_ASSERT(r > 0.0f);
	for (float r : inSettings.mTransmission.mReverseGearRatios)
		JPH_ASSERT(r < 0.0f);
#endif // JPH_ENABLE_ASSERTS
	JPH_ASSERT(inSettings.mTransmission.mSwitchTime >= 0.0f);
	JPH_ASSERT(inSettings.mTransmission.mShiftDownRPM > 0.0f);
	JPH_ASSERT(inSettings.mTransmission.mMode != ETransmissionMode::Auto || inSettings.mTransmission.mShiftUpRPM < inSettings.mEngine.mMaxRPM);
	JPH_ASSERT(inSettings.mTransmission.mShiftUpRPM > inSettings.mTransmission.mShiftDownRPM);

	// Copy track settings
	for (uint i = 0; i < std::size(mTracks); ++i)
	{
		const VehicleTrackSettings &d = inSettings.mTracks[i];
		static_cast<VehicleTrackSettings &>(mTracks[i]) = d;
		JPH_ASSERT(d.mInertia >= 0.0f);
		JPH_ASSERT(d.mAngularDamping >= 0.0f);
		JPH_ASSERT(d.mMaxBrakeTorque >= 0.0f);
		JPH_ASSERT(d.mDifferentialRatio > 0.0f);
	}
}

bool TrackedVehicleController::AllowSleep() const
{
	return mForwardInput == 0.0f								// No user input
		&& mTransmission.AllowSleep()							// Transmission is not shifting
		&& mEngine.AllowSleep();								// Engine is idling
}

void TrackedVehicleController::PreCollide(float inDeltaTime, PhysicsSystem &inPhysicsSystem)
{
	Wheels &wheels = mConstraint.GetWheels();

	// Fill in track index
	for (size_t t = 0; t < std::size(mTracks); ++t)
		for (uint w : mTracks[t].mWheels)
			static_cast<WheelTV *>(wheels[w])->mTrackIndex = (uint)t;

	// Angular damping: dw/dt = -c * w
	// Solution: w(t) = w(0) * e^(-c * t) or w2 = w1 * e^(-c * dt)
	// Taylor expansion of e^(-c * dt) = 1 - c * dt + ...
	// Since dt is usually in the order of 1/60 and c is a low number too this approximation is good enough
	for (VehicleTrack &t : mTracks)
		t.mAngularVelocity *= max(0.0f, 1.0f - t.mAngularDamping * inDeltaTime);
}

void TrackedVehicleController::SyncLeftRightTracks()
{
	// Apply left to right ratio according to track inertias
	VehicleTrack &tl = mTracks[(int)ETrackSide::Left];
	VehicleTrack &tr = mTracks[(int)ETrackSide::Right];

	if (mLeftRatio * mRightRatio > 0.0f)
	{
		// Solve: (tl.mAngularVelocity + dl) / (tr.mAngularVelocity + dr) = mLeftRatio / mRightRatio and dl * tr.mInertia = -dr * tl.mInertia, where dl/dr are the delta angular velocities for left and right tracks
		float impulse = (mLeftRatio * tr.mAngularVelocity - mRightRatio * tl.mAngularVelocity) / (mLeftRatio * tr.mInertia + mRightRatio * tl.mInertia);
		tl.mAngularVelocity += impulse * tl.mInertia;
		tr.mAngularVelocity -= impulse * tr.mInertia;
	}
	else
	{
		// Solve: (tl.mAngularVelocity + dl) / (tr.mAngularVelocity + dr) = mLeftRatio / mRightRatio and dl * tr.mInertia = dr * tl.mInertia, where dl/dr are the delta angular velocities for left and right tracks
		float impulse = (mLeftRatio * tr.mAngularVelocity - mRightRatio * tl.mAngularVelocity) / (mRightRatio * tl.mInertia - mLeftRatio * tr.mInertia);
		tl.mAngularVelocity += impulse * tl.mInertia;
		tr.mAngularVelocity += impulse * tr.mInertia;
	}
}

void TrackedVehicleController::PostCollide(float inDeltaTime, PhysicsSystem &inPhysicsSystem)
{
	JPH_PROFILE_FUNCTION();

	Wheels &wheels = mConstraint.GetWheels();

	// Update wheel angle, do this before applying torque to the wheels (as friction will slow them down again)
	for (uint wheel_index = 0, num_wheels = (uint)wheels.size(); wheel_index < num_wheels; ++wheel_index)
	{
		WheelTV *w = static_cast<WheelTV *>(wheels[wheel_index]);
		w->Update(wheel_index, inDeltaTime, mConstraint);
	}

	// First calculate engine speed based on speed of all wheels
	bool can_engine_apply_torque = false;
	if (mTransmission.GetCurrentGear() != 0 && mTransmission.GetClutchFriction() > 1.0e-3f)
	{
		float transmission_ratio = mTransmission.GetCurrentRatio();
		bool forward = transmission_ratio >= 0.0f;
		float fastest_wheel_speed = forward? -FLT_MAX : FLT_MAX;
		for (const VehicleTrack &t : mTracks)
		{
			if (forward)
				fastest_wheel_speed = max(fastest_wheel_speed, t.mAngularVelocity * t.mDifferentialRatio);
			else
				fastest_wheel_speed = min(fastest_wheel_speed, t.mAngularVelocity * t.mDifferentialRatio);
			for (uint w : t.mWheels)
				if (wheels[w]->HasContact())
				{
					can_engine_apply_torque = true;
					break;
				}
		}

		// Update RPM only if the tracks are connected to the engine
		if (fastest_wheel_speed > -FLT_MAX && fastest_wheel_speed < FLT_MAX)
			mEngine.SetCurrentRPM(fastest_wheel_speed * mTransmission.GetCurrentRatio() * VehicleEngine::cAngularVelocityToRPM);
	}
	else
	{
		// Update engine with damping
		mEngine.ApplyDamping(inDeltaTime);

		// In auto transmission mode, don't accelerate the engine when switching gears
		float forward_input = mTransmission.mMode == ETransmissionMode::Manual? abs(mForwardInput) : 0.0f;

		// Engine not connected to wheels, update RPM based on engine inertia alone
		mEngine.ApplyTorque(mEngine.GetTorque(forward_input), inDeltaTime);
	}

	// Update transmission
	// Note: only allow switching gears up when the tracks are rolling in the same direction
	mTransmission.Update(inDeltaTime, mEngine.GetCurrentRPM(), mForwardInput, mLeftRatio * mRightRatio > 0.0f && can_engine_apply_torque);

	// Calculate the amount of torque the transmission gives to the differentials
	float transmission_ratio = mTransmission.GetCurrentRatio();
	float transmission_torque = mTransmission.GetClutchFriction() * transmission_ratio * mEngine.GetTorque(abs(mForwardInput));
	if (transmission_torque != 0.0f)
	{
		// Apply the transmission torque to the wheels
		for (uint i = 0; i < std::size(mTracks); ++i)
		{
			VehicleTrack &t = mTracks[i];

			// Get wheel rotation ratio for this track
			float ratio = i == 0? mLeftRatio : mRightRatio;

			// Calculate the max angular velocity of the driven wheel of the track given current engine RPM
			// Note this adds 0.1% slop to avoid numerical accuracy issues
			float track_max_angular_velocity = mEngine.GetCurrentRPM() / (transmission_ratio * t.mDifferentialRatio * ratio * VehicleEngine::cAngularVelocityToRPM) * 1.001f;

			// Calculate torque on the driven wheel
			float differential_torque = t.mDifferentialRatio * ratio * transmission_torque;

			// Apply torque to driven wheel
			if (t.mAngularVelocity * track_max_angular_velocity < 0.0f || abs(t.mAngularVelocity) < abs(track_max_angular_velocity))
				t.mAngularVelocity += differential_torque * inDeltaTime / t.mInertia;
		}
	}

	// Ensure that we have the correct ratio between the two tracks
	SyncLeftRightTracks();

	// Braking
	for (VehicleTrack &t : mTracks)
	{
		// Calculate brake torque
		float brake_torque = mBrakeInput * t.mMaxBrakeTorque;
		if (brake_torque > 0.0f)
		{
			// Calculate how much torque is needed to stop the track from rotating in this time step
			float brake_torque_to_lock_track = abs(t.mAngularVelocity) * t.mInertia / inDeltaTime;
			if (brake_torque > brake_torque_to_lock_track)
			{
				// Wheels are locked
				t.mAngularVelocity = 0.0f;
				brake_torque -= brake_torque_to_lock_track;
			}
			else
			{
				// Slow down the track
				t.mAngularVelocity -= Sign(t.mAngularVelocity) * brake_torque * inDeltaTime / t.mInertia;
			}
		}

		if (brake_torque > 0.0f)
		{
			// Sum the radius of all wheels touching the floor
			float total_radius = 0.0f;
			for (uint wheel_index : t.mWheels)
			{
				const WheelTV *w = static_cast<WheelTV *>(wheels[wheel_index]);

				if (w->HasContact())
					total_radius += w->GetSettings()->mRadius;
			}

			if (total_radius > 0.0f)
			{
				brake_torque /= total_radius;
				for (uint wheel_index : t.mWheels)
				{
					WheelTV *w = static_cast<WheelTV *>(wheels[wheel_index]);
					if (w->HasContact())
					{
						// Impulse: p = F * dt = Torque / Wheel_Radius * dt, Torque = Total_Torque * Wheel_Radius / Summed_Radius => p = Total_Torque * dt / Summed_Radius
						w->mBrakeImpulse = brake_torque * inDeltaTime;
					}
				}
			}
		}
	}

	// Update wheel angular velocity based on that of the track
	for (Wheel *w_base : wheels)
	{
		WheelTV *w = static_cast<WheelTV *>(w_base);
		w->CalculateAngularVelocity(mConstraint);
	}
}

bool TrackedVehicleController::SolveLongitudinalAndLateralConstraints(float inDeltaTime)
{
	bool impulse = false;

	for (Wheel *w_base : mConstraint.GetWheels())
		if (w_base->HasContact())
		{
			WheelTV *w = static_cast<WheelTV *>(w_base);
			const WheelSettingsTV *settings = w->GetSettings();
			VehicleTrack &track = mTracks[w->mTrackIndex];

			// Calculate max impulse that we can apply on the ground
			float max_longitudinal_friction_impulse = w->mCombinedLongitudinalFriction * w->GetSuspensionLambda();

			// Calculate relative velocity between wheel contact point and floor in longitudinal direction
			Vec3 relative_velocity = mConstraint.GetVehicleBody()->GetPointVelocity(w->GetContactPosition()) - w->GetContactPointVelocity();
			float relative_longitudinal_velocity = relative_velocity.Dot(w->GetContactLongitudinal());

			// Calculate brake force to apply
			float min_longitudinal_impulse, max_longitudinal_impulse;
			if (w->mBrakeImpulse != 0.0f)
			{
				// Limit brake force by max tire friction
				float brake_impulse = min(w->mBrakeImpulse, max_longitudinal_friction_impulse);

				// Check which direction the brakes should be applied (we don't want to apply an impulse that would accelerate the vehicle)
				if (relative_longitudinal_velocity >= 0.0f)
				{
					min_longitudinal_impulse = -brake_impulse;
					max_longitudinal_impulse = 0.0f;
				}
				else
				{
					min_longitudinal_impulse = 0.0f;
					max_longitudinal_impulse = brake_impulse;
				}

				// Longitudinal impulse, note that we assume that once the wheels are locked that the brakes have more than enough torque to keep the wheels locked so we exclude any rotation deltas
				impulse |= w->SolveLongitudinalConstraintPart(mConstraint, min_longitudinal_impulse, max_longitudinal_impulse);
			}
			else
			{
				// Assume we want to apply an angular impulse that makes the delta velocity between track and ground zero in one time step, calculate the amount of linear impulse needed to do that
				float desired_angular_velocity = relative_longitudinal_velocity / settings->mRadius;
				float linear_impulse = (track.mAngularVelocity - desired_angular_velocity) * track.mInertia / settings->mRadius;

				// Limit the impulse by max track friction
				float prev_lambda = w->GetLongitudinalLambda();
				min_longitudinal_impulse = max_longitudinal_impulse = Clamp(prev_lambda + linear_impulse, -max_longitudinal_friction_impulse, max_longitudinal_friction_impulse);

				// Longitudinal impulse
				impulse |= w->SolveLongitudinalConstraintPart(mConstraint, min_longitudinal_impulse, max_longitudinal_impulse);

				// Update the angular velocity of the track according to the lambda that was applied
				track.mAngularVelocity -= (w->GetLongitudinalLambda() - prev_lambda) * settings->mRadius / track.mInertia;
				SyncLeftRightTracks();
			}
		}

	for (Wheel *w_base : mConstraint.GetWheels())
		if (w_base->HasContact())
		{
			WheelTV *w = static_cast<WheelTV *>(w_base);

			// Update angular velocity of wheel for the next iteration
			w->CalculateAngularVelocity(mConstraint);

			// Lateral friction
			float max_lateral_friction_impulse = w->mCombinedLateralFriction * w->GetSuspensionLambda();
			impulse |= w->SolveLateralConstraintPart(mConstraint, -max_lateral_friction_impulse, max_lateral_friction_impulse);
		}

	return impulse;
}

#ifdef JPH_DEBUG_RENDERER

void TrackedVehicleController::Draw(DebugRenderer *inRenderer) const
{
	float constraint_size = mConstraint.GetDrawConstraintSize();

	// Draw RPM
	Body *body = mConstraint.GetVehicleBody();
	Vec3 rpm_meter_up = body->GetRotation() * mConstraint.GetLocalUp();
	RVec3 rpm_meter_pos = body->GetPosition() + body->GetRotation() * mRPMMeterPosition;
	Vec3 rpm_meter_fwd = body->GetRotation() * mConstraint.GetLocalForward();
	mEngine.DrawRPM(inRenderer, rpm_meter_pos, rpm_meter_fwd, rpm_meter_up, mRPMMeterSize, mTransmission.mShiftDownRPM, mTransmission.mShiftUpRPM);

	// Draw current vehicle state
	String status = StringFormat("Forward: %.1f, LRatio: %.1f, RRatio: %.1f, Brake: %.1f\n"
								 "Gear: %d, Clutch: %.1f, EngineRPM: %.0f, V: %.1f km/h",
								 (double)mForwardInput, (double)mLeftRatio, (double)mRightRatio, (double)mBrakeInput,
								 mTransmission.GetCurrentGear(), (double)mTransmission.GetClutchFriction(), (double)mEngine.GetCurrentRPM(), (double)body->GetLinearVelocity().Length() * 3.6);
	inRenderer->DrawText3D(body->GetPosition(), status, Color::sWhite, constraint_size);

	for (const VehicleTrack &t : mTracks)
	{
		const WheelTV *w = static_cast<const WheelTV *>(mConstraint.GetWheels()[t.mDrivenWheel]);
		const WheelSettings *settings = w->GetSettings();

		// Calculate where the suspension attaches to the body in world space
		RVec3 ws_position = body->GetCenterOfMassPosition() + body->GetRotation() * (settings->mPosition - body->GetShape()->GetCenterOfMass());

		DebugRenderer::sInstance->DrawText3D(ws_position, StringFormat("W: %.1f", (double)t.mAngularVelocity), Color::sWhite, constraint_size);
	}

	RMat44 body_transform = body->GetWorldTransform();

	for (const Wheel *w_base : mConstraint.GetWheels())
	{
		const WheelTV *w = static_cast<const WheelTV *>(w_base);
		const WheelSettings *settings = w->GetSettings();

		// Calculate where the suspension attaches to the body in world space
		RVec3 ws_position = body_transform * settings->mPosition;
		Vec3 ws_direction = body_transform.Multiply3x3(settings->mSuspensionDirection);

		// Draw suspension
		RVec3 min_suspension_pos = ws_position + ws_direction * settings->mSuspensionMinLength;
		RVec3 max_suspension_pos = ws_position + ws_direction * settings->mSuspensionMaxLength;
		inRenderer->DrawLine(ws_position, min_suspension_pos, Color::sRed);
		inRenderer->DrawLine(min_suspension_pos, max_suspension_pos, Color::sGreen);

		// Draw current length
		RVec3 wheel_pos = ws_position + ws_direction * w->GetSuspensionLength();
		inRenderer->DrawMarker(wheel_pos, w->GetSuspensionLength() < settings->mSuspensionMinLength? Color::sRed : Color::sGreen, constraint_size);

		// Draw wheel basis
		Vec3 wheel_forward, wheel_up, wheel_right;
		mConstraint.GetWheelLocalBasis(w, wheel_forward, wheel_up, wheel_right);
		wheel_forward = body_transform.Multiply3x3(wheel_forward);
		wheel_up = body_transform.Multiply3x3(wheel_up);
		wheel_right = body_transform.Multiply3x3(wheel_right);
		Vec3 steering_axis = body_transform.Multiply3x3(settings->mSteeringAxis);
		inRenderer->DrawLine(wheel_pos, wheel_pos + wheel_forward, Color::sRed);
		inRenderer->DrawLine(wheel_pos, wheel_pos + wheel_up, Color::sGreen);
		inRenderer->DrawLine(wheel_pos, wheel_pos + wheel_right, Color::sBlue);
		inRenderer->DrawLine(wheel_pos, wheel_pos + steering_axis, Color::sYellow);

		// Draw wheel
		RMat44 wheel_transform(Vec4(wheel_up, 0.0f), Vec4(wheel_right, 0.0f), Vec4(wheel_forward, 0.0f), wheel_pos);
		wheel_transform.SetRotation(wheel_transform.GetRotation() * Mat44::sRotationY(-w->GetRotationAngle()));
		inRenderer->DrawCylinder(wheel_transform, settings->mWidth * 0.5f, settings->mRadius, w->GetSuspensionLength() <= settings->mSuspensionMinLength? Color::sRed : Color::sGreen, DebugRenderer::ECastShadow::Off, DebugRenderer::EDrawMode::Wireframe);

		if (w->HasContact())
		{
			// Draw contact
			inRenderer->DrawLine(w->GetContactPosition(), w->GetContactPosition() + w->GetContactNormal(), Color::sYellow);
			inRenderer->DrawLine(w->GetContactPosition(), w->GetContactPosition() + w->GetContactLongitudinal(), Color::sRed);
			inRenderer->DrawLine(w->GetContactPosition(), w->GetContactPosition() + w->GetContactLateral(), Color::sBlue);

			DebugRenderer::sInstance->DrawText3D(w->GetContactPosition(), StringFormat("S: %.2f", (double)w->GetSuspensionLength()), Color::sWhite, constraint_size);
		}
	}
}

#endif // JPH_DEBUG_RENDERER

void TrackedVehicleController::SaveState(StateRecorder &inStream) const
{
	inStream.Write(mForwardInput);
	inStream.Write(mLeftRatio);
	inStream.Write(mRightRatio);
	inStream.Write(mBrakeInput);

	mEngine.SaveState(inStream);
	mTransmission.SaveState(inStream);

	for (const VehicleTrack &t : mTracks)
		t.SaveState(inStream);
}

void TrackedVehicleController::RestoreState(StateRecorder &inStream)
{
	inStream.Read(mForwardInput);
	inStream.Read(mLeftRatio);
	inStream.Read(mRightRatio);
	inStream.Read(mBrakeInput);

	mEngine.RestoreState(inStream);
	mTransmission.RestoreState(inStream);

	for (VehicleTrack &t : mTracks)
		t.RestoreState(inStream);
}

JPH_NAMESPACE_END

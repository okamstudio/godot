// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Jolt/ObjectStream/SerializableObject.h>
#include <Jolt/Core/StreamIn.h>
#include <Jolt/Core/StreamOut.h>
#ifdef JPH_DEBUG_RENDERER
	#include <Jolt/Renderer/DebugRenderer.h>
#endif // JPH_DEBUG_RENDERER

JPH_NAMESPACE_BEGIN

class PhysicsSystem;
class VehicleController;
class VehicleConstraint;
class WheelSettings;
class Wheel;
class StateRecorder;

/// Basic settings object for interface that controls acceleration / deceleration of the vehicle
class JPH_EXPORT VehicleControllerSettings : public SerializableObject, public RefTarget<VehicleControllerSettings>
{
	JPH_DECLARE_SERIALIZABLE_ABSTRACT(JPH_EXPORT, VehicleControllerSettings)

public:
	/// Saves the contents of the controller settings in binary form to inStream.
	virtual void				SaveBinaryState(StreamOut &inStream) const = 0;

	/// Restore the contents of the controller settings in binary form from inStream.
	virtual void				RestoreBinaryState(StreamIn &inStream) = 0;

	/// Create an instance of the vehicle controller class
	virtual VehicleController *	ConstructController(VehicleConstraint &inConstraint) const = 0;
};

/// Runtime data for interface that controls acceleration / deceleration of the vehicle
class JPH_EXPORT VehicleController : public RefTarget<VehicleController>, public NonCopyable
{
public:
	JPH_OVERRIDE_NEW_DELETE

	/// Constructor / destructor
	explicit					VehicleController(VehicleConstraint &inConstraint) : mConstraint(inConstraint) { }
	virtual						~VehicleController() = default;

	/// Access the vehicle constraint that this controller is part of
	VehicleConstraint &			GetConstraint()								{ return mConstraint; }
	const VehicleConstraint &	GetConstraint() const						{ return mConstraint; }

protected:
	// The functions below are only for the VehicleConstraint
	friend class VehicleConstraint;

	// Create a new instance of wheel
	virtual Wheel *				ConstructWheel(const WheelSettings &inWheel) const = 0;

	// If the vehicle is allowed to go to sleep
	virtual bool				AllowSleep() const = 0;

	// Called before the wheel probes have been done
	virtual void				PreCollide(float inDeltaTime, PhysicsSystem &inPhysicsSystem) = 0;

	// Called after the wheel probes have been done
	virtual void				PostCollide(float inDeltaTime, PhysicsSystem &inPhysicsSystem) = 0;

	// Solve longitudinal and lateral constraint parts for all of the wheels
	virtual bool				SolveLongitudinalAndLateralConstraints(float inDeltaTime) = 0;

	// Saving state for replay
	virtual void				SaveState(StateRecorder &inStream) const = 0;
	virtual void				RestoreState(StateRecorder &inStream) = 0;

#ifdef JPH_DEBUG_RENDERER
	// Drawing interface
	virtual void				Draw(DebugRenderer *inRenderer) const = 0;
#endif // JPH_DEBUG_RENDERER

	VehicleConstraint &			mConstraint;								///< The vehicle constraint we belong to
};

JPH_NAMESPACE_END

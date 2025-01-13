// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

JPH_NAMESPACE_BEGIN

class BodyID;

/// A listener class that receives events when a body activates or deactivates.
/// It can be registered with the BodyManager (or PhysicsSystem).
class BodyActivationListener
{
public:
	/// Ensure virtual destructor
	virtual					~BodyActivationListener() = default;

	/// Called whenever a body activates, note this can be called from any thread so make sure your code is thread safe.
	/// At the time of the callback the body inBodyID will be locked and no bodies can be written/activated/deactivated from the callback.
	virtual void			OnBodyActivated(const BodyID &inBodyID, uint64 inBodyUserData) = 0;

	/// Called whenever a body deactivates, note this can be called from any thread so make sure your code is thread safe.
	/// At the time of the callback the body inBodyID will be locked and no bodies can be written/activated/deactivated from the callback.
	virtual void			OnBodyDeactivated(const BodyID &inBodyID, uint64 inBodyUserData) = 0;
};

JPH_NAMESPACE_END

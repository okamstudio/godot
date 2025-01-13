// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Jolt/ObjectStream/SerializableObject.h>
#include <Jolt/Core/StreamIn.h>
#include <Jolt/Core/StreamOut.h>

JPH_NAMESPACE_BEGIN

/// An anti rollbar is a stiff spring that connects two wheels to reduce the amount of roll the vehicle makes in sharp corners
/// See: https://en.wikipedia.org/wiki/Anti-roll_bar
class JPH_EXPORT VehicleAntiRollBar
{
	JPH_DECLARE_SERIALIZABLE_NON_VIRTUAL(JPH_EXPORT, VehicleAntiRollBar)

public:
	/// Saves the contents in binary form to inStream.
	void					SaveBinaryState(StreamOut &inStream) const;

	/// Restores the contents in binary form to inStream.
	void					RestoreBinaryState(StreamIn &inStream);

	int						mLeftWheel = 0;								///< Index (in mWheels) that represents the left wheel of this anti-rollbar
	int						mRightWheel = 1;							///< Index (in mWheels) that represents the right wheel of this anti-rollbar
	float					mStiffness = 1000.0f;						///< Stiffness (spring constant in N/m) of anti rollbar, can be 0 to disable the anti-rollbar
};

JPH_NAMESPACE_END

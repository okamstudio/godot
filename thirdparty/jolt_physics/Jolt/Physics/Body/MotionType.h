// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

JPH_NAMESPACE_BEGIN

/// Motion type of a physics body
enum class EMotionType : uint8
{
	Static,						///< Non movable
	Kinematic,					///< Movable using velocities only, does not respond to forces
	Dynamic,					///< Responds to forces as a normal physics object
};

JPH_NAMESPACE_END

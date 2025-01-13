// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Jolt/Geometry/AABox.h>

JPH_NAMESPACE_BEGIN

/// Structure that holds AABox moving linearly through 3d space
struct AABoxCast
{
	JPH_OVERRIDE_NEW_DELETE

	AABox						mBox;						///< Axis aligned box at starting location
	Vec3						mDirection;					///< Direction and length of the cast (anything beyond this length will not be reported as a hit)
};

JPH_NAMESPACE_END

// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

JPH_NAMESPACE_BEGIN

/// Enum indicating which component to use when swizzling
enum
{
	SWIZZLE_X = 0,			///< Use the X component
	SWIZZLE_Y = 1,			///< Use the Y component
	SWIZZLE_Z = 2,			///< Use the Z component
	SWIZZLE_W = 3,			///< Use the W component
	SWIZZLE_UNUSED = 2,		///< We always use the Z component when we don't specifically want to initialize a value, this is consistent with what is done in Vec3(x, y, z), Vec3(Float3 &) and Vec3::sLoadFloat3Unsafe
};

JPH_NAMESPACE_END

// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Jolt/Core/HashCombine.h>

JPH_NAMESPACE_BEGIN

/// Class that holds 3 floats. Used as a storage class. Convert to Vec3 for calculations.
class [[nodiscard]] Float3
{
public:
	JPH_OVERRIDE_NEW_DELETE

				Float3() = default; ///< Intentionally not initialized for performance reasons
				Float3(const Float3 &inRHS) = default;
	Float3 &	operator = (const Float3 &inRHS) = default;
	constexpr	Float3(float inX, float inY, float inZ) : x(inX), y(inY), z(inZ) { }

	float		operator [] (int inCoordinate) const
	{
		JPH_ASSERT(inCoordinate < 3);
		return *(&x + inCoordinate);
	}

	bool		operator == (const Float3 &inRHS) const
	{
		return x == inRHS.x && y == inRHS.y && z == inRHS.z;
	}

	bool		operator != (const Float3 &inRHS) const
	{
		return x != inRHS.x || y != inRHS.y || z != inRHS.z;
	}

	float		x;
	float		y;
	float		z;
};

using VertexList = Array<Float3>;

static_assert(std::is_trivial<Float3>(), "Is supposed to be a trivial type!");

JPH_NAMESPACE_END

// Create a std::hash/JPH::Hash for Float3
JPH_MAKE_HASHABLE(JPH::Float3, t.x, t.y, t.z)

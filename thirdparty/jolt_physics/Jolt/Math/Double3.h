// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Jolt/Core/HashCombine.h>

JPH_NAMESPACE_BEGIN

/// Class that holds 3 doubles. Used as a storage class. Convert to DVec3 for calculations.
class [[nodiscard]] Double3
{
public:
	JPH_OVERRIDE_NEW_DELETE

				Double3() = default; ///< Intentionally not initialized for performance reasons
				Double3(const Double3 &inRHS) = default;
	Double3 &	operator = (const Double3 &inRHS) = default;
				Double3(double inX, double inY, double inZ) : x(inX), y(inY), z(inZ) { }

	double		operator [] (int inCoordinate) const
	{
		JPH_ASSERT(inCoordinate < 3);
		return *(&x + inCoordinate);
	}

	bool		operator == (const Double3 &inRHS) const
	{
		return x == inRHS.x && y == inRHS.y && z == inRHS.z;
	}

	bool		operator != (const Double3 &inRHS) const
	{
		return x != inRHS.x || y != inRHS.y || z != inRHS.z;
	}

	double		x;
	double		y;
	double		z;
};

static_assert(std::is_trivial<Double3>(), "Is supposed to be a trivial type!");

JPH_NAMESPACE_END

// Create a std::hash/JPH::Hash for Double3
JPH_MAKE_HASHABLE(JPH::Double3, t.x, t.y, t.z)

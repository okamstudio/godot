// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

JPH_NAMESPACE_BEGIN

/// Class that holds 4 float values. Convert to Vec4 to perform calculations.
class [[nodiscard]] Float4
{
public:
	JPH_OVERRIDE_NEW_DELETE

				Float4() = default; ///< Intentionally not initialized for performance reasons
				Float4(const Float4 &inRHS) = default;
				Float4(float inX, float inY, float inZ, float inW) : x(inX), y(inY), z(inZ), w(inW) { }

	float		operator [] (int inCoordinate) const
	{
		JPH_ASSERT(inCoordinate < 4);
		return *(&x + inCoordinate);
	}

	float		x;
	float		y;
	float		z;
	float		w;
};

static_assert(std::is_trivial<Float4>(), "Is supposed to be a trivial type!");

JPH_NAMESPACE_END

// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

JPH_NAMESPACE_BEGIN

/// Class that holds 2 floats, used as a storage class mainly.
class [[nodiscard]] Float2
{
public:
	JPH_OVERRIDE_NEW_DELETE

						Float2() = default; ///< Intentionally not initialized for performance reasons
						Float2(const Float2 &inRHS) = default;
	Float2 &			operator = (const Float2 &inRHS) = default;
						Float2(float inX, float inY)					: x(inX), y(inY) { }

	bool				operator == (const Float2 &inRHS) const			{ return x == inRHS.x && y == inRHS.y; }
	bool				operator != (const Float2 &inRHS) const			{ return x != inRHS.x || y != inRHS.y; }

	/// To String
	friend ostream &	operator << (ostream &inStream, const Float2 &inV)
	{
		inStream << inV.x << ", " << inV.y;
		return inStream;
	}

	float				x;
	float				y;
};

static_assert(std::is_trivial<Float2>(), "Is supposed to be a trivial type!");

JPH_NAMESPACE_END

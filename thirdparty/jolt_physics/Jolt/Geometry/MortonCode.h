// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Jolt/Geometry/AABox.h>

JPH_NAMESPACE_BEGIN

class MortonCode
{
public:
	/// First converts a floating point value in the range [0, 1] to a 10 bit fixed point integer.
	/// Then expands a 10-bit integer into 30 bits by inserting 2 zeros after each bit.
	static uint32 sExpandBits(float inV)
	{
		JPH_ASSERT(inV >= 0.0f && inV <= 1.0f);
		uint32 v = uint32(inV * 1023.0f + 0.5f);
		JPH_ASSERT(v < 1024);
		v = (v * 0x00010001u) & 0xFF0000FFu;
		v = (v * 0x00000101u) & 0x0F00F00Fu;
		v = (v * 0x00000011u) & 0xC30C30C3u;
		v = (v * 0x00000005u) & 0x49249249u;
		return v;
	}

	/// Calculate the morton code for inVector, given that all vectors lie in inVectorBounds
	static uint32 sGetMortonCode(Vec3Arg inVector, const AABox &inVectorBounds)
	{
		// Convert to 10 bit fixed point
		Vec3 scaled = (inVector - inVectorBounds.mMin) / inVectorBounds.GetSize();
		uint x = sExpandBits(scaled.GetX());
		uint y = sExpandBits(scaled.GetY());
		uint z = sExpandBits(scaled.GetZ());
		return (x << 2) + (y << 1) + z;
	}
};

JPH_NAMESPACE_END

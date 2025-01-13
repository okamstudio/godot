// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Jolt/Physics/Body/BodyID.h>
#include <Jolt/Core/HashCombine.h>

JPH_NAMESPACE_BEGIN

/// Structure that holds a body pair
struct alignas(uint64) BodyPair
{
	JPH_OVERRIDE_NEW_DELETE

	/// Constructor
							BodyPair() = default;
							BodyPair(BodyID inA, BodyID inB)							: mBodyA(inA), mBodyB(inB) { }

	/// Equals operator
	bool					operator == (const BodyPair &inRHS) const					{ return *reinterpret_cast<const uint64 *>(this) == *reinterpret_cast<const uint64 *>(&inRHS); }

	/// Smaller than operator, used for consistently ordering body pairs
	bool					operator < (const BodyPair &inRHS) const					{ return *reinterpret_cast<const uint64 *>(this) < *reinterpret_cast<const uint64 *>(&inRHS); }

	/// Get the hash value of this object
	uint64					GetHash() const												{ return Hash64(*reinterpret_cast<const uint64 *>(this)); }

	BodyID					mBodyA;
	BodyID					mBodyB;
};

static_assert(sizeof(BodyPair) == sizeof(uint64), "Mismatch in class size");

JPH_NAMESPACE_END

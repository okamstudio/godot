// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2023 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

JPH_NAMESPACE_BEGIN

/// Enum used by PhysicsSystem to report error conditions during the PhysicsSystem::Update call. This is a bit field, multiple errors can trigger in the same update.
enum class EPhysicsUpdateError : uint32
{
	None					= 0,			///< No errors
	ManifoldCacheFull		= 1 << 0,		///< The manifold cache is full, this means that the total number of contacts between bodies is too high. Some contacts were ignored. Increase inMaxContactConstraints in PhysicsSystem::Init.
	BodyPairCacheFull		= 1 << 1,		///< The body pair cache is full, this means that too many bodies contacted. Some contacts were ignored. Increase inMaxBodyPairs in PhysicsSystem::Init.
	ContactConstraintsFull	= 1 << 2,		///< The contact constraints buffer is full. Some contacts were ignored. Increase inMaxContactConstraints in PhysicsSystem::Init.
};

/// OR operator for EPhysicsUpdateError
inline EPhysicsUpdateError operator | (EPhysicsUpdateError inA, EPhysicsUpdateError inB)
{
	return static_cast<EPhysicsUpdateError>(static_cast<uint32>(inA) | static_cast<uint32>(inB));
}

/// OR operator for EPhysicsUpdateError
inline EPhysicsUpdateError operator |= (EPhysicsUpdateError &ioA, EPhysicsUpdateError inB)
{
	ioA = ioA | inB;
	return ioA;
}

/// AND operator for EPhysicsUpdateError
inline EPhysicsUpdateError operator & (EPhysicsUpdateError inA, EPhysicsUpdateError inB)
{
	return static_cast<EPhysicsUpdateError>(static_cast<uint32>(inA) & static_cast<uint32>(inB));
}

JPH_NAMESPACE_END

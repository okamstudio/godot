// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2023 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

JPH_NAMESPACE_BEGIN

/// Enum used in BodyCreationSettings and MotionProperties to indicate which degrees of freedom a body has
enum class EAllowedDOFs : uint8
{
	None				= 0b000000,									///< No degrees of freedom are allowed. Note that this is not valid and will crash. Use a static body instead.
	All					= 0b111111,									///< All degrees of freedom are allowed
	TranslationX		= 0b000001,									///< Body can move in world space X axis
	TranslationY		= 0b000010,									///< Body can move in world space Y axis
	TranslationZ		= 0b000100,									///< Body can move in world space Z axis
	RotationX			= 0b001000,									///< Body can rotate around world space X axis
	RotationY			= 0b010000,									///< Body can rotate around world space Y axis
	RotationZ			= 0b100000,									///< Body can rotate around world space Z axis
	Plane2D				= TranslationX | TranslationY | RotationZ,	///< Body can only move in X and Y axis and rotate around Z axis
};

/// Bitwise OR operator for EAllowedDOFs
constexpr EAllowedDOFs operator | (EAllowedDOFs inLHS, EAllowedDOFs inRHS)
{
	return EAllowedDOFs(uint8(inLHS) | uint8(inRHS));
}

/// Bitwise AND operator for EAllowedDOFs
constexpr EAllowedDOFs operator & (EAllowedDOFs inLHS, EAllowedDOFs inRHS)
{
	return EAllowedDOFs(uint8(inLHS) & uint8(inRHS));
}

/// Bitwise XOR operator for EAllowedDOFs
constexpr EAllowedDOFs operator ^ (EAllowedDOFs inLHS, EAllowedDOFs inRHS)
{
	return EAllowedDOFs(uint8(inLHS) ^ uint8(inRHS));
}

/// Bitwise NOT operator for EAllowedDOFs
constexpr EAllowedDOFs operator ~ (EAllowedDOFs inAllowedDOFs)
{
	return EAllowedDOFs(~uint8(inAllowedDOFs));
}

/// Bitwise OR assignment operator for EAllowedDOFs
constexpr EAllowedDOFs & operator |= (EAllowedDOFs &ioLHS, EAllowedDOFs inRHS)
{
	ioLHS = ioLHS | inRHS;
	return ioLHS;
}

/// Bitwise AND assignment operator for EAllowedDOFs
constexpr EAllowedDOFs & operator &= (EAllowedDOFs &ioLHS, EAllowedDOFs inRHS)
{
	ioLHS = ioLHS & inRHS;
	return ioLHS;
}

/// Bitwise XOR assignment operator for EAllowedDOFs
constexpr EAllowedDOFs & operator ^= (EAllowedDOFs &ioLHS, EAllowedDOFs inRHS)
{
	ioLHS = ioLHS ^ inRHS;
	return ioLHS;
}

JPH_NAMESPACE_END

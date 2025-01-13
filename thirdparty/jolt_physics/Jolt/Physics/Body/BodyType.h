// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2023 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

JPH_NAMESPACE_BEGIN

/// Type of body
enum class EBodyType : uint8
{
	RigidBody,				///< Rigid body consisting of a rigid shape
	SoftBody,				///< Soft body consisting of a deformable shape
};

/// How many types of bodies there are
static constexpr uint cBodyTypeCount = 2;

JPH_NAMESPACE_END

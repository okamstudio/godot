// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

JPH_NAMESPACE_BEGIN

/// Enum used by AddBody to determine if the body needs to be initially active
enum class EActivation
{
	Activate,				///< Activate the body, making it part of the simulation
	DontActivate			///< Leave activation state as it is (will not deactivate an active body)
};

JPH_NAMESPACE_END

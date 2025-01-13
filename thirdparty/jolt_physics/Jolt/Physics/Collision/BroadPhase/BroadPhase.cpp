// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#include <Jolt/Jolt.h>

#include <Jolt/Physics/Collision/BroadPhase/BroadPhase.h>

JPH_NAMESPACE_BEGIN

void BroadPhase::Init(BodyManager *inBodyManager, const BroadPhaseLayerInterface &inLayerInterface)
{
	mBodyManager = inBodyManager;
}

JPH_NAMESPACE_END

// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2023 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayerInterfaceMask.h>

JPH_NAMESPACE_BEGIN

/// Class that determines if an object layer can collide with a broadphase layer.
/// This implementation works together with BroadPhaseLayerInterfaceMask and ObjectLayerPairFilterMask
class ObjectVsBroadPhaseLayerFilterMask : public ObjectVsBroadPhaseLayerFilter
{
public:
	JPH_OVERRIDE_NEW_DELETE

/// Constructor
					ObjectVsBroadPhaseLayerFilterMask(const BroadPhaseLayerInterfaceMask &inBroadPhaseLayerInterface) :
		mBroadPhaseLayerInterface(inBroadPhaseLayerInterface)
	{
	}

	/// Returns true if an object layer should collide with a broadphase layer
	virtual bool	ShouldCollide(ObjectLayer inLayer1, BroadPhaseLayer inLayer2) const override
	{
		// Just defer to BroadPhaseLayerInterface
		return mBroadPhaseLayerInterface.ShouldCollide(inLayer1, inLayer2);
	}

private:
	const BroadPhaseLayerInterfaceMask &mBroadPhaseLayerInterface;
};

JPH_NAMESPACE_END

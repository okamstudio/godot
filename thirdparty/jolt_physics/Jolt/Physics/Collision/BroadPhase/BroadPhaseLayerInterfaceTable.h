// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2023 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>

JPH_NAMESPACE_BEGIN

/// BroadPhaseLayerInterface implementation.
/// This defines a mapping between object and broadphase layers.
/// This implementation uses a simple table
class BroadPhaseLayerInterfaceTable : public BroadPhaseLayerInterface
{
public:
	JPH_OVERRIDE_NEW_DELETE

							BroadPhaseLayerInterfaceTable(uint inNumObjectLayers, uint inNumBroadPhaseLayers) :
		mNumBroadPhaseLayers(inNumBroadPhaseLayers)
	{
		mObjectToBroadPhase.resize(inNumObjectLayers, BroadPhaseLayer(0));
#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
		mBroadPhaseLayerNames.resize(inNumBroadPhaseLayers, "Undefined");
#endif // JPH_EXTERNAL_PROFILE || JPH_PROFILE_ENABLED
	}

	void					MapObjectToBroadPhaseLayer(ObjectLayer inObjectLayer, BroadPhaseLayer inBroadPhaseLayer)
	{
		JPH_ASSERT((BroadPhaseLayer::Type)inBroadPhaseLayer < mNumBroadPhaseLayers);
		mObjectToBroadPhase[inObjectLayer] = inBroadPhaseLayer;
	}

	virtual uint			GetNumBroadPhaseLayers() const override
	{
		return mNumBroadPhaseLayers;
	}

	virtual BroadPhaseLayer	GetBroadPhaseLayer(ObjectLayer inLayer) const override
	{
		return mObjectToBroadPhase[inLayer];
	}

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
	void					SetBroadPhaseLayerName(BroadPhaseLayer inLayer, const char *inName)
	{
		mBroadPhaseLayerNames[(BroadPhaseLayer::Type)inLayer] = inName;
	}

	virtual const char *	GetBroadPhaseLayerName(BroadPhaseLayer inLayer) const override
	{
		return mBroadPhaseLayerNames[(BroadPhaseLayer::Type)inLayer];
	}
#endif // JPH_EXTERNAL_PROFILE || JPH_PROFILE_ENABLED

private:
	uint					mNumBroadPhaseLayers;
	Array<BroadPhaseLayer>	mObjectToBroadPhase;
#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
	Array<const char *>		mBroadPhaseLayerNames;
#endif // JPH_EXTERNAL_PROFILE || JPH_PROFILE_ENABLED
};

JPH_NAMESPACE_END

// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2023 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>

JPH_NAMESPACE_BEGIN

/// Class that determines if an object layer can collide with a broadphase layer.
/// This implementation uses a table and constructs itself from an ObjectLayerPairFilter and a BroadPhaseLayerInterface.
class ObjectVsBroadPhaseLayerFilterTable : public ObjectVsBroadPhaseLayerFilter
{
private:
	/// Get which bit corresponds to the pair (inLayer1, inLayer2)
	uint					GetBit(ObjectLayer inLayer1, BroadPhaseLayer inLayer2) const
	{
		// Calculate at which bit the entry for this pair resides
		return inLayer1 * mNumBroadPhaseLayers + (BroadPhaseLayer::Type)inLayer2;
	}

public:
	JPH_OVERRIDE_NEW_DELETE

	/// Construct the table
	/// @param inBroadPhaseLayerInterface The broad phase layer interface that maps object layers to broad phase layers
	/// @param inNumBroadPhaseLayers Number of broad phase layers
	/// @param inObjectLayerPairFilter The object layer pair filter that determines which object layers can collide
	/// @param inNumObjectLayers Number of object layers
							ObjectVsBroadPhaseLayerFilterTable(const BroadPhaseLayerInterface &inBroadPhaseLayerInterface, uint inNumBroadPhaseLayers, const ObjectLayerPairFilter &inObjectLayerPairFilter, uint inNumObjectLayers) :
		mNumBroadPhaseLayers(inNumBroadPhaseLayers)
	{
		// Resize table and set all entries to false
		mTable.resize((inNumBroadPhaseLayers * inNumObjectLayers + 7) / 8, 0);

		// Loop over all object layer pairs
		for (ObjectLayer o1 = 0; o1 < inNumObjectLayers; ++o1)
			for (ObjectLayer o2 = 0; o2 < inNumObjectLayers; ++o2)
			{
				// Get the broad phase layer for the second object layer
				BroadPhaseLayer b2 = inBroadPhaseLayerInterface.GetBroadPhaseLayer(o2);
				JPH_ASSERT((BroadPhaseLayer::Type)b2 < inNumBroadPhaseLayers);

				// If the object layers collide then so should the object and broadphase layer
				if (inObjectLayerPairFilter.ShouldCollide(o1, o2))
				{
					uint bit = GetBit(o1, b2);
					mTable[bit >> 3] |= 1 << (bit & 0b111);
				}
			}
	}

	/// Returns true if an object layer should collide with a broadphase layer
	virtual bool			ShouldCollide(ObjectLayer inLayer1, BroadPhaseLayer inLayer2) const override
	{
		uint bit = GetBit(inLayer1, inLayer2);
		return (mTable[bit >> 3] & (1 << (bit & 0b111))) != 0;
	}

private:
	uint					mNumBroadPhaseLayers;						///< The total number of broadphase layers
	Array<uint8>			mTable;										///< The table of bits that indicates which layers collide
};

JPH_NAMESPACE_END

// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Jolt/Core/NonCopyable.h>

JPH_NAMESPACE_BEGIN

/// Layer that objects can be in, determines which other objects it can collide with
#ifndef JPH_OBJECT_LAYER_BITS
	#define JPH_OBJECT_LAYER_BITS 16
#endif // JPH_OBJECT_LAYER_BITS
#if JPH_OBJECT_LAYER_BITS == 16
	using ObjectLayer = uint16;
#elif JPH_OBJECT_LAYER_BITS == 32
	using ObjectLayer = uint32;
#else
	#error "JPH_OBJECT_LAYER_BITS must be 16 or 32"
#endif

/// Constant value used to indicate an invalid object layer
static constexpr ObjectLayer cObjectLayerInvalid = ObjectLayer(~ObjectLayer(0U));

/// Filter class for object layers
class JPH_EXPORT ObjectLayerFilter : public NonCopyable
{
public:
	/// Destructor
	virtual					~ObjectLayerFilter() = default;

	/// Function to filter out object layers when doing collision query test (return true to allow testing against objects with this layer)
	virtual bool			ShouldCollide([[maybe_unused]] ObjectLayer inLayer) const
	{
		return true;
	}

#ifdef JPH_TRACK_BROADPHASE_STATS
	/// Get a string that describes this filter for stat tracking purposes
	virtual String			GetDescription() const
	{
		return "No Description";
	}
#endif // JPH_TRACK_BROADPHASE_STATS
};

/// Filter class to test if two objects can collide based on their object layer. Used while finding collision pairs.
class JPH_EXPORT ObjectLayerPairFilter : public NonCopyable
{
public:
	/// Destructor
	virtual					~ObjectLayerPairFilter() = default;

	/// Returns true if two layers can collide
	virtual bool			ShouldCollide([[maybe_unused]] ObjectLayer inLayer1, [[maybe_unused]] ObjectLayer inLayer2) const
	{
		return true;
	}
};

/// Default filter class that uses the pair filter in combination with a specified layer to filter layers
class JPH_EXPORT DefaultObjectLayerFilter : public ObjectLayerFilter
{
public:
	/// Constructor
							DefaultObjectLayerFilter(const ObjectLayerPairFilter &inObjectLayerPairFilter, ObjectLayer inLayer) :
		mObjectLayerPairFilter(inObjectLayerPairFilter),
		mLayer(inLayer)
	{
	}

	/// Copy constructor
							DefaultObjectLayerFilter(const DefaultObjectLayerFilter &inRHS) :
		mObjectLayerPairFilter(inRHS.mObjectLayerPairFilter),
		mLayer(inRHS.mLayer)
	{
	}

	// See ObjectLayerFilter::ShouldCollide
	virtual bool			ShouldCollide(ObjectLayer inLayer) const override
	{
		return mObjectLayerPairFilter.ShouldCollide(mLayer, inLayer);
	}

private:
	const ObjectLayerPairFilter & mObjectLayerPairFilter;
	ObjectLayer				mLayer;
};

/// Allows objects from a specific layer only
class JPH_EXPORT SpecifiedObjectLayerFilter : public ObjectLayerFilter
{
public:
	/// Constructor
	explicit				SpecifiedObjectLayerFilter(ObjectLayer inLayer) :
		mLayer(inLayer)
	{
	}

	// See ObjectLayerFilter::ShouldCollide
	virtual bool			ShouldCollide(ObjectLayer inLayer) const override
	{
		return mLayer == inLayer;
	}

private:
	ObjectLayer				mLayer;
};

JPH_NAMESPACE_END

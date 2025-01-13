// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Jolt/Core/NonCopyable.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>

JPH_NAMESPACE_BEGIN

/// An object layer can be mapped to a broadphase layer. Objects with the same broadphase layer will end up in the same sub structure (usually a tree) of the broadphase.
/// When there are many layers, this reduces the total amount of sub structures the broad phase needs to manage. Usually you want objects that don't collide with each other
/// in different broad phase layers, but there could be exceptions if objects layers only contain a minor amount of objects so it is not beneficial to give each layer its
/// own sub structure in the broadphase.
/// Note: This class requires explicit casting from and to Type to avoid confusion with ObjectLayer
class BroadPhaseLayer
{
public:
	using Type = uint8;

	JPH_INLINE						BroadPhaseLayer() = default;
	JPH_INLINE explicit constexpr	BroadPhaseLayer(Type inValue) : mValue(inValue) { }
	JPH_INLINE constexpr			BroadPhaseLayer(const BroadPhaseLayer &) = default;
	JPH_INLINE BroadPhaseLayer &	operator = (const BroadPhaseLayer &) = default;

	JPH_INLINE constexpr bool		operator == (const BroadPhaseLayer &inRHS) const
	{
		return mValue == inRHS.mValue;
	}

	JPH_INLINE constexpr bool		operator != (const BroadPhaseLayer &inRHS) const
	{
		return mValue != inRHS.mValue;
	}

	JPH_INLINE constexpr bool		operator < (const BroadPhaseLayer &inRHS) const
	{
		return mValue < inRHS.mValue;
	}

	JPH_INLINE explicit constexpr	operator Type() const
	{
		return mValue;
	}

	JPH_INLINE Type					GetValue() const
	{
		return mValue;
	}

private:
	Type							mValue;
};

/// Constant value used to indicate an invalid broad phase layer
static constexpr BroadPhaseLayer cBroadPhaseLayerInvalid(0xff);

/// Interface that the application should implement to allow mapping object layers to broadphase layers
class JPH_EXPORT BroadPhaseLayerInterface : public NonCopyable
{
public:
	/// Destructor
	virtual							~BroadPhaseLayerInterface() = default;

	/// Return the number of broadphase layers there are
	virtual uint					GetNumBroadPhaseLayers() const = 0;

	/// Convert an object layer to the corresponding broadphase layer
	virtual BroadPhaseLayer			GetBroadPhaseLayer(ObjectLayer inLayer) const = 0;

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
	/// Get the user readable name of a broadphase layer (debugging purposes)
	virtual const char *			GetBroadPhaseLayerName(BroadPhaseLayer inLayer) const = 0;
#endif // JPH_EXTERNAL_PROFILE || JPH_PROFILE_ENABLED
};

/// Class to test if an object can collide with a broadphase layer. Used while finding collision pairs.
class JPH_EXPORT ObjectVsBroadPhaseLayerFilter : public NonCopyable
{
public:
	/// Destructor
	virtual							~ObjectVsBroadPhaseLayerFilter() = default;

	/// Returns true if an object layer should collide with a broadphase layer
	virtual bool					ShouldCollide([[maybe_unused]] ObjectLayer inLayer1, [[maybe_unused]] BroadPhaseLayer inLayer2) const
	{
		return true;
	}
};

/// Filter class for broadphase layers
class JPH_EXPORT BroadPhaseLayerFilter : public NonCopyable
{
public:
	/// Destructor
	virtual							~BroadPhaseLayerFilter() = default;

	/// Function to filter out broadphase layers when doing collision query test (return true to allow testing against objects with this layer)
	virtual bool					ShouldCollide([[maybe_unused]] BroadPhaseLayer inLayer) const
	{
		return true;
	}
};

/// Default filter class that uses the pair filter in combination with a specified layer to filter layers
class JPH_EXPORT DefaultBroadPhaseLayerFilter : public BroadPhaseLayerFilter
{
public:
	/// Constructor
									DefaultBroadPhaseLayerFilter(const ObjectVsBroadPhaseLayerFilter &inObjectVsBroadPhaseLayerFilter, ObjectLayer inLayer) :
		mObjectVsBroadPhaseLayerFilter(inObjectVsBroadPhaseLayerFilter),
		mLayer(inLayer)
	{
	}

	// See BroadPhaseLayerFilter::ShouldCollide
	virtual bool					ShouldCollide(BroadPhaseLayer inLayer) const override
	{
		return mObjectVsBroadPhaseLayerFilter.ShouldCollide(mLayer, inLayer);
	}

private:
	const ObjectVsBroadPhaseLayerFilter &mObjectVsBroadPhaseLayerFilter;
	ObjectLayer						mLayer;
};

/// Allows objects from a specific broad phase layer only
class JPH_EXPORT SpecifiedBroadPhaseLayerFilter : public BroadPhaseLayerFilter
{
public:
	/// Constructor
	explicit						SpecifiedBroadPhaseLayerFilter(BroadPhaseLayer inLayer) :
		mLayer(inLayer)
	{
	}

	// See BroadPhaseLayerFilter::ShouldCollide
	virtual bool					ShouldCollide(BroadPhaseLayer inLayer) const override
	{
		return mLayer == inLayer;
	}

private:
	BroadPhaseLayer					mLayer;
};

JPH_NAMESPACE_END

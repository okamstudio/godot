// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Jolt/Physics/Collision/PhysicsMaterial.h>

JPH_NAMESPACE_BEGIN

/// Sample implementation of PhysicsMaterial that just holds the needed properties directly
class JPH_EXPORT PhysicsMaterialSimple : public PhysicsMaterial
{
	JPH_DECLARE_SERIALIZABLE_VIRTUAL(JPH_EXPORT, PhysicsMaterialSimple)

public:
	/// Constructor
											PhysicsMaterialSimple() = default;
											PhysicsMaterialSimple(const string_view &inName, ColorArg inColor) : mDebugName(inName), mDebugColor(inColor) { }

	// Properties
	virtual const char *					GetDebugName() const override		{ return mDebugName.c_str(); }
	virtual Color							GetDebugColor() const override		{ return mDebugColor; }

	// See: PhysicsMaterial::SaveBinaryState
	virtual void							SaveBinaryState(StreamOut &inStream) const override;

protected:
	// See: PhysicsMaterial::RestoreBinaryState
	virtual void							RestoreBinaryState(StreamIn &inStream) override;

private:
	String									mDebugName;							///< Name of the material, used for debugging purposes
	Color									mDebugColor = Color::sGrey;			///< Color of the material, used to render the shapes
};

JPH_NAMESPACE_END

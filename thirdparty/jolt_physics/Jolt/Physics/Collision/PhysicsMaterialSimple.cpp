// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#include <Jolt/Jolt.h>

#include <Jolt/Physics/Collision/PhysicsMaterialSimple.h>
#include <Jolt/ObjectStream/TypeDeclarations.h>
#include <Jolt/Core/StreamIn.h>
#include <Jolt/Core/StreamOut.h>

JPH_NAMESPACE_BEGIN

JPH_IMPLEMENT_SERIALIZABLE_VIRTUAL(PhysicsMaterialSimple)
{
	JPH_ADD_BASE_CLASS(PhysicsMaterialSimple, PhysicsMaterial)

	JPH_ADD_ATTRIBUTE(PhysicsMaterialSimple, mDebugName)
	JPH_ADD_ATTRIBUTE(PhysicsMaterialSimple, mDebugColor)
}

void PhysicsMaterialSimple::SaveBinaryState(StreamOut &inStream) const
{
	PhysicsMaterial::SaveBinaryState(inStream);

	inStream.Write(mDebugName);
	inStream.Write(mDebugColor);
}

void PhysicsMaterialSimple::RestoreBinaryState(StreamIn &inStream)
{
	PhysicsMaterial::RestoreBinaryState(inStream);

	inStream.Read(mDebugName);
	inStream.Read(mDebugColor);
}

JPH_NAMESPACE_END

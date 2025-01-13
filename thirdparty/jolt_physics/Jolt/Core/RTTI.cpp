// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#include <Jolt/Jolt.h>

#include <Jolt/Core/RTTI.h>
#include <Jolt/Core/StringTools.h>

JPH_NAMESPACE_BEGIN

//////////////////////////////////////////////////////////////////////////////////////////
// RTTI
//////////////////////////////////////////////////////////////////////////////////////////

RTTI::RTTI(const char *inName, int inSize, pCreateObjectFunction inCreateObject, pDestructObjectFunction inDestructObject) :
	mName(inName),
	mSize(inSize),
	mCreate(inCreateObject),
	mDestruct(inDestructObject)
{
	JPH_ASSERT(inDestructObject != nullptr, "Object cannot be destructed");
}

RTTI::RTTI(const char *inName, int inSize, pCreateObjectFunction inCreateObject, pDestructObjectFunction inDestructObject, pCreateRTTIFunction inCreateRTTI) :
	mName(inName),
	mSize(inSize),
	mCreate(inCreateObject),
	mDestruct(inDestructObject)
{
	JPH_ASSERT(inDestructObject != nullptr, "Object cannot be destructed");

	inCreateRTTI(*this);
}

int RTTI::GetBaseClassCount() const
{
	return (int)mBaseClasses.size();
}

const RTTI *RTTI::GetBaseClass(int inIdx) const
{
	return mBaseClasses[inIdx].mRTTI;
}

uint32 RTTI::GetHash() const
{
	// Perform diffusion step to get from 64 to 32 bits (see https://en.wikipedia.org/wiki/Fowler%E2%80%93Noll%E2%80%93Vo_hash_function)
	uint64 hash = HashString(mName);
	return (uint32)(hash ^ (hash >> 32));
}

void *RTTI::CreateObject() const
{
	return IsAbstract()? nullptr : mCreate();
}

void RTTI::DestructObject(void *inObject) const
{
	mDestruct(inObject);
}

void RTTI::AddBaseClass(const RTTI *inRTTI, int inOffset)
{
	JPH_ASSERT(inOffset >= 0 && inOffset < mSize, "Base class not contained in derived class");

	// Add base class
	BaseClass base;
	base.mRTTI = inRTTI;
	base.mOffset = inOffset;
	mBaseClasses.push_back(base);

#ifdef JPH_OBJECT_STREAM
	// Add attributes of base class
	for (const SerializableAttribute &a : inRTTI->mAttributes)
		mAttributes.push_back(SerializableAttribute(a, inOffset));
#endif // JPH_OBJECT_STREAM
}

bool RTTI::operator == (const RTTI &inRHS) const
{
	// Compare addresses
	if (this == &inRHS)
		return true;

	// Check that the names differ (if that is the case we probably have two instances
	// of the same attribute info across the program, probably the second is in a DLL)
	JPH_ASSERT(strcmp(mName, inRHS.mName) != 0);
	return false;
}

bool RTTI::IsKindOf(const RTTI *inRTTI) const
{
	// Check if this is the same type
	if (this == inRTTI)
		return true;

	// Check all base classes
	for (const BaseClass &b : mBaseClasses)
		if (b.mRTTI->IsKindOf(inRTTI))
			return true;

	return false;
}

const void *RTTI::CastTo(const void *inObject, const RTTI *inRTTI) const
{
	JPH_ASSERT(inObject != nullptr);

	// Check if this is the same type
	if (this == inRTTI)
		return inObject;

	// Check all base classes
	for (const BaseClass &b : mBaseClasses)
	{
		// Cast the pointer to the base class
		const void *casted = (const void *)(((const uint8 *)inObject) + b.mOffset);

		// Test base class
		const void *rv = b.mRTTI->CastTo(casted, inRTTI);
		if (rv != nullptr)
			return rv;
	}

	// Not possible to cast
	return nullptr;
}

#ifdef JPH_OBJECT_STREAM

void RTTI::AddAttribute(const SerializableAttribute &inAttribute)
{
	mAttributes.push_back(inAttribute);
}

int RTTI::GetAttributeCount() const
{
	return (int)mAttributes.size();
}

const SerializableAttribute &RTTI::GetAttribute(int inIdx) const
{
	return mAttributes[inIdx];
}

#endif // JPH_OBJECT_STREAM

JPH_NAMESPACE_END

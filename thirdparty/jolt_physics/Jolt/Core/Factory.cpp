// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#include <Jolt/Jolt.h>

#include <Jolt/Core/Factory.h>

JPH_NAMESPACE_BEGIN

Factory *Factory::sInstance = nullptr;

void *Factory::CreateObject(const char *inName)
{
	const RTTI *ci = Find(inName);
	return ci != nullptr? ci->CreateObject() : nullptr;
}

const RTTI *Factory::Find(const char *inName)
{
	ClassNameMap::iterator c = mClassNameMap.find(inName);
	return c != mClassNameMap.end()? c->second : nullptr;
}

const RTTI *Factory::Find(uint32 inHash)
{
	ClassHashMap::iterator c = mClassHashMap.find(inHash);
	return c != mClassHashMap.end()? c->second : nullptr;
}

bool Factory::Register(const RTTI *inRTTI)
{
	// Check if we already know the type
	if (Find(inRTTI->GetName()) != nullptr)
		return true;

	// Insert this class by name
	mClassNameMap.try_emplace(inRTTI->GetName(), inRTTI);

	// Insert this class by hash
	if (!mClassHashMap.try_emplace(inRTTI->GetHash(), inRTTI).second)
	{
		JPH_ASSERT(false, "Hash collision registering type!");
		return false;
	}

	// Register base classes
	for (int i = 0; i < inRTTI->GetBaseClassCount(); ++i)
		if (!Register(inRTTI->GetBaseClass(i)))
			return false;

#ifdef JPH_OBJECT_STREAM
	// Register attribute classes
	for (int i = 0; i < inRTTI->GetAttributeCount(); ++i)
	{
		const RTTI *rtti = inRTTI->GetAttribute(i).GetMemberPrimitiveType();
		if (rtti != nullptr && !Register(rtti))
			return false;
	}
#endif // JPH_OBJECT_STREAM

	return true;
}

bool Factory::Register(const RTTI **inRTTIs, uint inNumber)
{
	mClassHashMap.reserve(mClassHashMap.size() + inNumber);
	mClassNameMap.reserve(mClassNameMap.size() + inNumber);

	for (const RTTI **rtti = inRTTIs; rtti < inRTTIs + inNumber; ++rtti)
		if (!Register(*rtti))
			return false;

	return true;
}

void Factory::Clear()
{
	mClassNameMap.clear();
	mClassHashMap.clear();
}

Array<const RTTI *> Factory::GetAllClasses() const
{
	Array<const RTTI *> all_classes;
	all_classes.reserve(mClassNameMap.size());
	for (const ClassNameMap::value_type &c : mClassNameMap)
		all_classes.push_back(c.second);
	return all_classes;
}

JPH_NAMESPACE_END

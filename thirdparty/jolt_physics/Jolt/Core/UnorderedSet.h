// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2024 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Jolt/Core/HashTable.h>

JPH_NAMESPACE_BEGIN

/// Internal helper class to provide context for UnorderedSet
template <class Key>
class UnorderedSetDetail
{
public:
	/// The key is the key, just return it
	static const Key &		sGetKey(const Key &inKey)
	{
		return inKey;
	}
};

/// Hash Set class
/// @tparam Key Key type
/// @tparam Hash Hash function (note should be 64-bits)
/// @tparam KeyEqual Equality comparison function
template <class Key, class Hash = JPH::Hash<Key>, class KeyEqual = std::equal_to<Key>>
class UnorderedSet : public HashTable<Key, Key, UnorderedSetDetail<Key>, Hash, KeyEqual>
{
};

JPH_NAMESPACE_END

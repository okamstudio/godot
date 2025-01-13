// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2024 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Jolt/Core/HashTable.h>

JPH_NAMESPACE_BEGIN

/// Internal helper class to provide context for UnorderedMap
template <class Key, class Value>
class UnorderedMapDetail
{
public:
	/// Get key from key value pair
	static const Key &			sGetKey(const std::pair<Key, Value> &inKeyValue)
	{
		return inKeyValue.first;
	}
};

/// Hash Map class
/// @tparam Key Key type
/// @tparam Value Value type
/// @tparam Hash Hash function (note should be 64-bits)
/// @tparam KeyEqual Equality comparison function
template <class Key, class Value, class Hash = JPH::Hash<Key>, class KeyEqual = std::equal_to<Key>>
class UnorderedMap : public HashTable<Key, std::pair<Key, Value>, UnorderedMapDetail<Key, Value>, Hash, KeyEqual>
{
	using Base = HashTable<Key, std::pair<Key, Value>, UnorderedMapDetail<Key, Value>, Hash, KeyEqual>;

public:
	using size_type = typename Base::size_type;
	using iterator = typename Base::iterator;
	using const_iterator = typename Base::const_iterator;
	using value_type = typename Base::value_type;

	Value &						operator [] (const Key &inKey)
	{
		size_type index;
		bool inserted = this->InsertKey(inKey, index);
		value_type &key_value = this->GetElement(index);
		if (inserted)
			::new (&key_value) value_type(inKey, Value());
		return key_value.second;
	}

	template<class... Args>
	std::pair<iterator, bool>	try_emplace(const Key &inKey, Args &&...inArgs)
	{
		size_type index;
		bool inserted = this->InsertKey(inKey, index);
		if (inserted)
			::new (&this->GetElement(index)) value_type(std::piecewise_construct, std::forward_as_tuple(inKey), std::forward_as_tuple(std::forward<Args>(inArgs)...));
		return std::make_pair(iterator(this, index), inserted);
	}

	template<class... Args>
	std::pair<iterator, bool>	try_emplace(Key &&inKey, Args &&...inArgs)
	{
		size_type index;
		bool inserted = this->InsertKey(inKey, index);
		if (inserted)
			::new (&this->GetElement(index)) value_type(std::piecewise_construct, std::forward_as_tuple(std::move(inKey)), std::forward_as_tuple(std::forward<Args>(inArgs)...));
		return std::make_pair(iterator(this, index), inserted);
	}

	/// Const version of find
	using Base::find;

	/// Non-const version of find
	iterator					find(const Key &inKey)
	{
		const_iterator it = Base::find(inKey);
		return iterator(this, it.mIndex);
	}
};

JPH_NAMESPACE_END

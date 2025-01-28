// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

JPH_NAMESPACE_BEGIN

/// Implements the FNV-1a hash algorithm
/// @see https://en.wikipedia.org/wiki/Fowler%E2%80%93Noll%E2%80%93Vo_hash_function
/// @param inData Data block of bytes
/// @param inSize Number of bytes
/// @param inSeed Seed of the hash (can be used to pass in the hash of a previous operation, otherwise leave default)
/// @return Hash
inline uint64 HashBytes(const void *inData, uint inSize, uint64 inSeed = 0xcbf29ce484222325UL)
{
	uint64 hash = inSeed;
	for (const uint8 *data = reinterpret_cast<const uint8 *>(inData); data < reinterpret_cast<const uint8 *>(inData) + inSize; ++data)
	{
		hash ^= uint64(*data);
		hash *= 0x100000001b3UL;
	}
	return hash;
}

/// Calculate the FNV-1a hash of inString.
/// @see https://en.wikipedia.org/wiki/Fowler%E2%80%93Noll%E2%80%93Vo_hash_function
constexpr uint64 HashString(const char *inString, uint64 inSeed = 0xcbf29ce484222325UL)
{
	uint64 hash = inSeed;
	for (const char *c = inString; *c != 0; ++c)
	{
		hash ^= uint64(*c);
		hash *= 0x100000001b3UL;
	}
	return hash;
}

/// A 64 bit hash function by Thomas Wang, Jan 1997
/// See: http://web.archive.org/web/20071223173210/http://www.concentric.net/~Ttwang/tech/inthash.htm
/// @param inValue Value to hash
/// @return Hash
inline uint64 Hash64(uint64 inValue)
{
	uint64 hash = inValue;
	hash = (~hash) + (hash << 21); // hash = (hash << 21) - hash - 1;
	hash = hash ^ (hash >> 24);
	hash = (hash + (hash << 3)) + (hash << 8); // hash * 265
	hash = hash ^ (hash >> 14);
	hash = (hash + (hash << 2)) + (hash << 4); // hash * 21
	hash = hash ^ (hash >> 28);
	hash = hash + (hash << 31);
	return hash;
}

/// Fallback hash function that calls T::GetHash()
template <class T>
struct Hash
{
	uint64		operator () (const T &inValue) const
	{
		return inValue.GetHash();
	}
};

/// A hash function for floats
template <>
struct Hash<float>
{
	uint64		operator () (float inValue) const
	{
		float value = inValue == 0.0f? 0.0f : inValue; // Convert -0.0f to 0.0f
		return HashBytes(&value, sizeof(value));
	}
};

/// A hash function for doubles
template <>
struct Hash<double>
{
	uint64		operator () (double inValue) const
	{
		double value = inValue == 0.0? 0.0 : inValue; // Convert -0.0 to 0.0
		return HashBytes(&value, sizeof(value));
	}
};

/// A hash function for character pointers
template <>
struct Hash<const char *>
{
	uint64		operator () (const char *inValue) const
	{
		return HashString(inValue);
	}
};

/// A hash function for std::string_view
template <>
struct Hash<std::string_view>
{
	uint64		operator () (const std::string_view &inValue) const
	{
		return HashBytes(inValue.data(), uint(inValue.size()));
	}
};

/// A hash function for String
template <>
struct Hash<String>
{
	uint64		operator () (const String &inValue) const
	{
		return HashBytes(inValue.data(), uint(inValue.size()));
	}
};

/// A fallback function for generic pointers
template <class T>
struct Hash<T *>
{
	uint64		operator () (T *inValue) const
	{
		return HashBytes(&inValue, sizeof(inValue));
	}
};

/// Helper macro to define a hash function for trivial types
#define JPH_DEFINE_TRIVIAL_HASH(type)						\
template <>													\
struct Hash<type>											\
{															\
	uint64		operator () (const type &inValue) const		\
	{														\
		return HashBytes(&inValue, sizeof(inValue));		\
	}														\
};

/// Commonly used types
JPH_DEFINE_TRIVIAL_HASH(char)
JPH_DEFINE_TRIVIAL_HASH(int)
JPH_DEFINE_TRIVIAL_HASH(uint32)
JPH_DEFINE_TRIVIAL_HASH(uint64)

/// Helper function that hashes a single value into ioSeed
/// Based on https://github.com/jonmaiga/mx3 by Jon Maiga
template <typename T>
inline void HashCombine(uint64 &ioSeed, const T &inValue)
{
	constexpr uint64 c = 0xbea225f9eb34556dUL;

	uint64 h = ioSeed;
	uint64 x = Hash<T> { } (inValue);

	// See: https://github.com/jonmaiga/mx3/blob/master/mx3.h
	// mix_stream(h, x)
	x *= c;
	x ^= x >> 39;
	h += x * c;
	h *= c;

	// mix(h)
	h ^= h >> 32;
	h *= c;
	h ^= h >> 29;
	h *= c;
	h ^= h >> 32;
	h *= c;
	h ^= h >> 29;

	ioSeed = h;
}

/// Hash combiner to use a custom struct in an unordered map or set
///
/// Usage:
///
///		struct SomeHashKey
///		{
///			std::string key1;
///			std::string key2;
///			bool key3;
///		};
///
///		JPH_MAKE_HASHABLE(SomeHashKey, t.key1, t.key2, t.key3)
template <typename FirstValue, typename... Values>
inline uint64 HashCombineArgs(const FirstValue &inFirstValue, Values... inValues)
{
	// Prime the seed by hashing the first value
	uint64 seed = Hash<FirstValue> { } (inFirstValue);

	// Hash all remaining values together using a fold expression
	(HashCombine(seed, inValues), ...);

	return seed;
}

#define JPH_MAKE_HASH_STRUCT(type, name, ...)				\
	struct [[nodiscard]] name								\
	{														\
		::JPH::uint64 operator()(const type &t) const		\
		{													\
			return ::JPH::HashCombineArgs(__VA_ARGS__);		\
		}													\
	};

#define JPH_MAKE_STD_HASH(type)								\
	JPH_SUPPRESS_WARNING_PUSH								\
	JPH_SUPPRESS_WARNINGS									\
	namespace std											\
	{														\
		template<>											\
		struct [[nodiscard]] hash<type>						\
		{													\
			size_t operator()(const type &t) const			\
			{												\
				return size_t(::JPH::Hash<type>{ }(t));		\
			}												\
		};													\
	}														\
	JPH_SUPPRESS_WARNING_POP

#define JPH_MAKE_HASHABLE(type, ...)						\
	JPH_SUPPRESS_WARNING_PUSH								\
	JPH_SUPPRESS_WARNINGS									\
	namespace JPH											\
	{														\
		template<>											\
		JPH_MAKE_HASH_STRUCT(type, Hash<type>, __VA_ARGS__) \
	}														\
	JPH_SUPPRESS_WARNING_POP								\
	JPH_MAKE_STD_HASH(type)

JPH_NAMESPACE_END

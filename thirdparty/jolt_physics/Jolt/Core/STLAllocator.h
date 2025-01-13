// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

JPH_NAMESPACE_BEGIN

/// Default implementation of AllocatorHasReallocate which tells if an allocator has a reallocate function
template <class T> struct AllocatorHasReallocate { static constexpr bool sValue = false; };

#ifndef JPH_DISABLE_CUSTOM_ALLOCATOR

/// STL allocator that forwards to our allocation functions
template <typename T>
class STLAllocator
{
public:
	using value_type = T;

	/// Pointer to type
	using pointer = T *;
	using const_pointer = const T *;

	/// Reference to type.
	/// Can be removed in C++20.
	using reference = T &;
	using const_reference = const T &;

	using size_type = size_t;
	using difference_type = ptrdiff_t;

	/// The allocator is stateless
	using is_always_equal = std::true_type;

	/// Allocator supports moving
	using propagate_on_container_move_assignment = std::true_type;

	/// Constructor
	inline					STLAllocator() = default;

	/// Constructor from other allocator
	template <typename T2>
	inline					STLAllocator(const STLAllocator<T2> &) { }

	/// If this allocator needs to fall back to aligned allocations because the type requires it
	static constexpr bool	needs_aligned_allocate = alignof(T) > (JPH_CPU_ADDRESS_BITS == 32? 8 : 16);

	/// Allocate memory
	inline pointer			allocate(size_type inN)
	{
		if constexpr (needs_aligned_allocate)
			return pointer(AlignedAllocate(inN * sizeof(value_type), alignof(T)));
		else
			return pointer(Allocate(inN * sizeof(value_type)));
	}

	/// Should we expose a reallocate function?
	static constexpr bool	has_reallocate = std::is_trivially_copyable<T>() && !needs_aligned_allocate;

	/// Reallocate memory
	template <bool has_reallocate_v = has_reallocate, typename = std::enable_if_t<has_reallocate_v>>
	inline pointer			reallocate(pointer inOldPointer, size_type inOldSize, size_type inNewSize)
	{
		JPH_ASSERT(inNewSize > 0); // Reallocating to zero size is implementation dependent, so we don't allow it
		return pointer(Reallocate(inOldPointer, inOldSize * sizeof(value_type), inNewSize * sizeof(value_type)));
	}

	/// Free memory
	inline void				deallocate(pointer inPointer, size_type)
	{
		if constexpr (needs_aligned_allocate)
			AlignedFree(inPointer);
		else
			Free(inPointer);
	}

	/// Allocators are stateless so assumed to be equal
	inline bool				operator == (const STLAllocator<T> &) const
	{
		return true;
	}

	inline bool				operator != (const STLAllocator<T> &) const
	{
		return false;
	}

	/// Converting to allocator for other type
	template <typename T2>
	struct rebind
	{
		using other = STLAllocator<T2>;
	};
};

/// The STLAllocator implements the reallocate function if the alignment of the class is smaller or equal to the default alignment for the platform
template <class T> struct AllocatorHasReallocate<STLAllocator<T>> { static constexpr bool sValue = STLAllocator<T>::has_reallocate; };

#else

template <typename T> using STLAllocator = std::allocator<T>;

#endif // !JPH_DISABLE_CUSTOM_ALLOCATOR

// Declare STL containers that use our allocator
using String = std::basic_string<char, std::char_traits<char>, STLAllocator<char>>;
using IStringStream = std::basic_istringstream<char, std::char_traits<char>, STLAllocator<char>>;

JPH_NAMESPACE_END

#if (!defined(JPH_PLATFORM_WINDOWS) || defined(JPH_COMPILER_MINGW)) && !defined(JPH_DISABLE_CUSTOM_ALLOCATOR)

namespace std
{
	/// Declare std::hash for String, for some reason on Linux based platforms template deduction takes the wrong variant
	template <>
	struct hash<JPH::String>
	{
		inline size_t operator () (const JPH::String &inRHS) const
		{
			return hash<string_view> { } (inRHS);
		}
	};
}

#endif // (!JPH_PLATFORM_WINDOWS || JPH_COMPILER_MINGW) && !JPH_DISABLE_CUSTOM_ALLOCATOR

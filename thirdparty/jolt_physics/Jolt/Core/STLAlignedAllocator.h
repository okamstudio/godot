// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

JPH_NAMESPACE_BEGIN

/// STL allocator that takes care that memory is aligned to N bytes
template <typename T, size_t N>
class STLAlignedAllocator
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
	inline					STLAlignedAllocator() = default;

	/// Constructor from other allocator
	template <typename T2>
	inline explicit			STLAlignedAllocator(const STLAlignedAllocator<T2, N> &) { }

	/// Allocate memory
	inline pointer			allocate(size_type inN)
	{
		return (pointer)AlignedAllocate(inN * sizeof(value_type), N);
	}

	/// Free memory
	inline void				deallocate(pointer inPointer, size_type)
	{
		AlignedFree(inPointer);
	}

	/// Allocators are stateless so assumed to be equal
	inline bool				operator == (const STLAlignedAllocator<T, N> &) const
	{
		return true;
	}

	inline bool				operator != (const STLAlignedAllocator<T, N> &) const
	{
		return false;
	}

	/// Converting to allocator for other type
	template <typename T2>
	struct rebind
	{
		using other = STLAlignedAllocator<T2, N>;
	};
};

JPH_NAMESPACE_END

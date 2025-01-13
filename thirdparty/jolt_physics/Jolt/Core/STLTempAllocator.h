// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Jolt/Core/TempAllocator.h>

JPH_NAMESPACE_BEGIN

/// STL allocator that wraps around TempAllocator
template <typename T>
class STLTempAllocator
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

	/// The allocator is not stateless (depends on the temp allocator)
	using is_always_equal = std::false_type;

	/// Constructor
	inline					STLTempAllocator(TempAllocator &inAllocator) : mAllocator(inAllocator) { }

	/// Constructor from other allocator
	template <typename T2>
	inline explicit			STLTempAllocator(const STLTempAllocator<T2> &inRHS) : mAllocator(inRHS.GetAllocator()) { }

	/// Allocate memory
	inline pointer			allocate(size_type inN)
	{
		return pointer(mAllocator.Allocate(uint(inN * sizeof(value_type))));
	}

	/// Free memory
	inline void				deallocate(pointer inPointer, size_type inN)
	{
		mAllocator.Free(inPointer, uint(inN * sizeof(value_type)));
	}

	/// Allocators are not-stateless, assume if allocator address matches that the allocators are the same
	inline bool				operator == (const STLTempAllocator<T> &inRHS) const
	{
		return &mAllocator == &inRHS.mAllocator;
	}

	inline bool				operator != (const STLTempAllocator<T> &inRHS) const
	{
		return &mAllocator != &inRHS.mAllocator;
	}

	/// Converting to allocator for other type
	template <typename T2>
	struct rebind
	{
		using other = STLTempAllocator<T2>;
	};

	/// Get our temp allocator
	TempAllocator &			GetAllocator() const
	{
		return mAllocator;
	}

private:
	TempAllocator &			mAllocator;
};

JPH_NAMESPACE_END

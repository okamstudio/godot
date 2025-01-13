// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2024 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Jolt/Core/STLAllocator.h>
#include <Jolt/Core/HashCombine.h>

#ifdef JPH_USE_STD_VECTOR

JPH_SUPPRESS_WARNINGS_STD_BEGIN
#include <vector>
JPH_SUPPRESS_WARNINGS_STD_END

JPH_NAMESPACE_BEGIN

template <class T, class Allocator = STLAllocator<T>> using Array = std::vector<T, Allocator>;

JPH_NAMESPACE_END

#else

JPH_NAMESPACE_BEGIN

/// Simple replacement for std::vector
///
/// Major differences:
/// - Memory is not initialized to zero (this was causing a lot of page faults when deserializing large MeshShapes / HeightFieldShapes)
/// - Iterators are simple pointers (for now)
/// - No exception safety
/// - No specialization like std::vector<bool> has
/// - Not all functions have been implemented
template <class T, class Allocator = STLAllocator<T>>
class [[nodiscard]] Array : private Allocator
{
public:
	using value_type = T;
	using allocator_type = Allocator;
	using size_type = size_t;
	using difference_type = typename Allocator::difference_type;
	using pointer = T *;
	using const_pointer = const T *;
	using reference = T &;
	using const_reference = const T &;

	using const_iterator = const T *;
	using iterator = T *;

private:
	/// Move elements from one location to another
	inline void				move(pointer inDestination, pointer inSource, size_type inCount)
	{
		if constexpr (std::is_trivially_copyable<T>())
			memmove(inDestination, inSource, inCount * sizeof(T));
		else
		{
			if (inDestination < inSource)
			{
				for (T *destination_end = inDestination + inCount; inDestination < destination_end; ++inDestination, ++inSource)
				{
					::new (inDestination) T(std::move(*inSource));
					inSource->~T();
				}
			}
			else
			{
				for (T *destination = inDestination + inCount - 1, *source = inSource + inCount - 1; destination >= inDestination; --destination, --source)
				{
					::new (destination) T(std::move(*source));
					source->~T();
				}
			}
		}
	}

	/// Reallocate the data block to inNewCapacity
	inline void				reallocate(size_type inNewCapacity)
	{
		JPH_ASSERT(inNewCapacity > 0 && inNewCapacity >= mSize);

		pointer ptr;
		if constexpr (AllocatorHasReallocate<Allocator>::sValue)
		{
			// Reallocate data block
			ptr = get_allocator().reallocate(mElements, mCapacity, inNewCapacity);
		}
		else
		{
			// Copy data to a new location
			ptr = get_allocator().allocate(inNewCapacity);
			if (mElements != nullptr)
			{
				move(ptr, mElements, mSize);
				get_allocator().deallocate(mElements, mCapacity);
			}
		}
		mElements = ptr;
		mCapacity = inNewCapacity;
	}

	/// Destruct elements [inStart, inEnd - 1]
	inline void				destruct(size_type inStart, size_type inEnd)
	{
		if constexpr (!std::is_trivially_destructible<T>())
			if (inStart < inEnd)
				for (T *element = mElements + inStart, *element_end = mElements + inEnd; element < element_end; ++element)
					element->~T();
	}

public:
	/// Reserve array space
	inline void				reserve(size_type inNewSize)
	{
		if (mCapacity < inNewSize)
			reallocate(inNewSize);
	}

	/// Resize array to new length
	inline void				resize(size_type inNewSize)
	{
		destruct(inNewSize, mSize);
		reserve(inNewSize);

		if constexpr (!std::is_trivially_constructible<T>())
			for (T *element = mElements + mSize, *element_end = mElements + inNewSize; element < element_end; ++element)
				::new (element) T;
		mSize = inNewSize;
	}

	/// Resize array to new length and initialize all elements with inValue
	inline void				resize(size_type inNewSize, const T &inValue)
	{
		JPH_ASSERT(&inValue < mElements || &inValue >= mElements + mSize, "Can't pass an element from the array to resize");

		destruct(inNewSize, mSize);
		reserve(inNewSize);

		for (T *element = mElements + mSize, *element_end = mElements + inNewSize; element < element_end; ++element)
			::new (element) T(inValue);
		mSize = inNewSize;
	}

	/// Destruct all elements and set length to zero
	inline void				clear()
	{
		destruct(0, mSize);
		mSize = 0;
	}

private:
	/// Grow the array by at least inAmount elements
	inline void				grow(size_type inAmount = 1)
	{
		size_type min_size = mSize + inAmount;
		if (min_size > mCapacity)
		{
			size_type new_capacity = max(min_size, mCapacity * 2);
			reserve(new_capacity);
		}
	}

	/// Free memory
	inline void				free()
	{
		get_allocator().deallocate(mElements, mCapacity);
		mElements = nullptr;
		mCapacity = 0;
	}

	/// Destroy all elements and free memory
	inline void				destroy()
	{
		if (mElements != nullptr)
		{
			clear();
			free();
		}
	}

public:
	/// Replace the contents of this array with inBegin .. inEnd
	template <class Iterator>
	inline void				assign(Iterator inBegin, Iterator inEnd)
	{
		clear();
		reserve(size_type(std::distance(inBegin, inEnd)));

		for (Iterator element = inBegin; element != inEnd; ++element)
			::new (&mElements[mSize++]) T(*element);
	}

	/// Replace the contents of this array with inList
	inline void				assign(std::initializer_list<T> inList)
	{
		clear();
		reserve(size_type(inList.size()));

		for (const T &v : inList)
			::new (&mElements[mSize++]) T(v);
	}

	/// Default constructor
							Array() = default;

	/// Constructor with allocator
	explicit inline			Array(const Allocator &inAllocator) :
		Allocator(inAllocator)
	{
	}

	/// Constructor with length
	explicit inline			Array(size_type inLength, const Allocator &inAllocator = { }) :
		Allocator(inAllocator)
	{
		resize(inLength);
	}

	/// Constructor with length and value
	inline					Array(size_type inLength, const T &inValue, const Allocator &inAllocator = { }) :
		Allocator(inAllocator)
	{
		resize(inLength, inValue);
	}

	/// Constructor from initializer list
	inline					Array(std::initializer_list<T> inList, const Allocator &inAllocator = { }) :
		Allocator(inAllocator)
	{
		assign(inList);
	}

	/// Constructor from iterator
	inline					Array(const_iterator inBegin, const_iterator inEnd, const Allocator &inAllocator = { }) :
		Allocator(inAllocator)
	{
		assign(inBegin, inEnd);
	}

	/// Copy constructor
	inline					Array(const Array<T, Allocator> &inRHS) :
		Allocator(inRHS.get_allocator())
	{
		assign(inRHS.begin(), inRHS.end());
	}

	/// Move constructor
	inline					Array(Array<T, Allocator> &&inRHS) noexcept :
		Allocator(std::move(inRHS.get_allocator())),
		mSize(inRHS.mSize),
		mCapacity(inRHS.mCapacity),
		mElements(inRHS.mElements)
	{
		inRHS.mSize = 0;
		inRHS.mCapacity = 0;
		inRHS.mElements = nullptr;
	}

	/// Destruct all elements
	inline					~Array()
	{
		destroy();
	}

	/// Get the allocator
	inline Allocator &		get_allocator()
	{
		return *this;
	}

	inline const Allocator &get_allocator() const
	{
		return *this;
	}

	/// Add element to the back of the array
	inline void				push_back(const T &inValue)
	{
		JPH_ASSERT(&inValue < mElements || &inValue >= mElements + mSize, "Can't pass an element from the array to push_back");

		grow();

		T *element = mElements + mSize++;
		::new (element) T(inValue);
	}

	inline void				push_back(T &&inValue)
	{
		grow();

		T *element = mElements + mSize++;
		::new (element) T(std::move(inValue));
	}

	/// Construct element at the back of the array
	template <class... A>
	inline T &				emplace_back(A &&... inValue)
	{
		grow();

		T *element = mElements + mSize++;
		::new (element) T(std::forward<A>(inValue)...);
		return *element;
	}

	/// Remove element from the back of the array
	inline void				pop_back()
	{
		JPH_ASSERT(mSize > 0);
		mElements[--mSize].~T();
	}

	/// Returns true if there are no elements in the array
	inline bool				empty() const
	{
		return mSize == 0;
	}

	/// Returns amount of elements in the array
	inline size_type		size() const
	{
		return mSize;
	}

	/// Returns maximum amount of elements the array can hold
	inline size_type		capacity() const
	{
		return mCapacity;
	}

	/// Reduce the capacity of the array to match its size
	void					shrink_to_fit()
	{
		if (mElements != nullptr)
		{
			if (mSize == 0)
				free();
			else if (mCapacity > mSize)
				reallocate(mSize);
		}
	}

	/// Swap the contents of two arrays
	void					swap(Array<T, Allocator> &inRHS) noexcept
	{
		std::swap(get_allocator(), inRHS.get_allocator());
		std::swap(mSize, inRHS.mSize);
		std::swap(mCapacity, inRHS.mCapacity);
		std::swap(mElements, inRHS.mElements);
	}

	template <class Iterator>
	void					insert(const_iterator inPos, Iterator inBegin, Iterator inEnd)
	{
		size_type num_elements = size_type(std::distance(inBegin, inEnd));
		if (num_elements > 0)
		{
			// After grow() inPos may be invalid
			size_type first_element = inPos - mElements;

			grow(num_elements);

			T *element_begin = mElements + first_element;
			T *element_end = element_begin + num_elements;
			move(element_end, element_begin, mSize - first_element);

			for (T *element = element_begin; element < element_end; ++element, ++inBegin)
				::new (element) T(*inBegin);

			mSize += num_elements;
		}
	}

	void					insert(const_iterator inPos, const T &inValue)
	{
		JPH_ASSERT(&inValue < mElements || &inValue >= mElements + mSize, "Can't pass an element from the array to insert");

		// After grow() inPos may be invalid
		size_type first_element = inPos - mElements;

		grow();

		T *element = mElements + first_element;
		move(element + 1, element, mSize - first_element);

		::new (element) T(inValue);
		mSize++;
	}

	/// Remove one element from the array
	void					erase(const_iterator inIter)
	{
		size_type p = size_type(inIter - begin());
		JPH_ASSERT(p < mSize);
		mElements[p].~T();
		if (p + 1 < mSize)
			move(mElements + p, mElements + p + 1, mSize - p - 1);
		--mSize;
	}

	/// Remove multiple element from the array
	void					erase(const_iterator inBegin, const_iterator inEnd)
	{
		size_type p = size_type(inBegin - begin());
		size_type n = size_type(inEnd - inBegin);
		JPH_ASSERT(inEnd <= end());
		destruct(p, p + n);
		if (p + n < mSize)
			move(mElements + p, mElements + p + n, mSize - p - n);
		mSize -= n;
	}

	/// Iterators
	inline const_iterator	begin() const
	{
		return mElements;
	}

	inline const_iterator	end() const
	{
		return mElements + mSize;
	}

	inline const_iterator	cbegin() const
	{
		return mElements;
	}

	inline const_iterator	cend() const
	{
		return mElements + mSize;
	}

	inline iterator			begin()
	{
		return mElements;
	}

	inline iterator			end()
	{
		return mElements + mSize;
	}

	inline const T *		data() const
	{
		return mElements;
	}

	inline T *				data()
	{
		return mElements;
	}

	/// Access element
	inline T &				operator [] (size_type inIdx)
	{
		JPH_ASSERT(inIdx < mSize);
		return mElements[inIdx];
	}

	inline const T &		operator [] (size_type inIdx) const
	{
		JPH_ASSERT(inIdx < mSize);
		return mElements[inIdx];
	}

	/// Access element
	inline T &				at(size_type inIdx)
	{
		JPH_ASSERT(inIdx < mSize);
		return mElements[inIdx];
	}

	inline const T &		at(size_type inIdx) const
	{
		JPH_ASSERT(inIdx < mSize);
		return mElements[inIdx];
	}

	/// First element in the array
	inline const T &		front() const
	{
		JPH_ASSERT(mSize > 0);
		return mElements[0];
	}

	inline T &				front()
	{
		JPH_ASSERT(mSize > 0);
		return mElements[0];
	}

	/// Last element in the array
	inline const T &		back() const
	{
		JPH_ASSERT(mSize > 0);
		return mElements[mSize - 1];
	}

	inline T &				back()
	{
		JPH_ASSERT(mSize > 0);
		return mElements[mSize - 1];
	}

	/// Assignment operator
	Array<T, Allocator> &	operator = (const Array<T, Allocator> &inRHS)
	{
		if (static_cast<const void *>(this) != static_cast<const void *>(&inRHS))
			assign(inRHS.begin(), inRHS.end());

		return *this;
	}

	/// Assignment move operator
	Array<T, Allocator> &	operator = (Array<T, Allocator> &&inRHS) noexcept
	{
		if (static_cast<const void *>(this) != static_cast<const void *>(&inRHS))
		{
			destroy();

			get_allocator() = std::move(inRHS.get_allocator());

			mSize = inRHS.mSize;
			mCapacity = inRHS.mCapacity;
			mElements = inRHS.mElements;

			inRHS.mSize = 0;
			inRHS.mCapacity = 0;
			inRHS.mElements = nullptr;
		}

		return *this;
	}

	/// Assignment operator
	Array<T, Allocator> &	operator = (std::initializer_list<T> inRHS)
	{
		assign(inRHS);

		return *this;
	}

	/// Comparing arrays
	bool					operator == (const Array<T, Allocator> &inRHS) const
	{
		if (mSize != inRHS.mSize)
			return false;
		for (size_type i = 0; i < mSize; ++i)
			if (!(mElements[i] == inRHS.mElements[i]))
				return false;
		return true;
	}

	bool					operator != (const Array<T, Allocator> &inRHS) const
	{
		if (mSize != inRHS.mSize)
			return true;
		for (size_type i = 0; i < mSize; ++i)
			if (mElements[i] != inRHS.mElements[i])
				return true;
		return false;
	}

	/// Get hash for this array
	uint64					GetHash() const
	{
		// Hash length first
		uint64 ret = Hash<uint32> { } (uint32(size()));

		// Then hash elements
		for (const T *element = mElements, *element_end = mElements + mSize; element < element_end; ++element)
			HashCombine(ret, *element);

		return ret;
	}

private:
	size_type				mSize = 0;
	size_type				mCapacity = 0;
	T *						mElements = nullptr;
};

JPH_NAMESPACE_END

JPH_SUPPRESS_WARNING_PUSH
JPH_CLANG_SUPPRESS_WARNING("-Wc++98-compat")

namespace std
{
	/// Declare std::hash for Array
	template <class T, class Allocator>
	struct hash<JPH::Array<T, Allocator>>
	{
		size_t operator () (const JPH::Array<T, Allocator> &inRHS) const
		{
			return std::size_t(inRHS.GetHash());
		}
	};
}

JPH_SUPPRESS_WARNING_POP

#endif // JPH_USE_STD_VECTOR

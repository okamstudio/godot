// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Jolt/Core/NonCopyable.h>

JPH_NAMESPACE_BEGIN

/// Allocator for temporary allocations.
/// This allocator works as a stack: The blocks must always be freed in the reverse order as they are allocated.
/// Note that allocations and frees can take place from different threads, but the order is guaranteed though
/// job dependencies, so it is not needed to use any form of locking.
class JPH_EXPORT TempAllocator : public NonCopyable
{
public:
	JPH_OVERRIDE_NEW_DELETE

	/// Destructor
	virtual							~TempAllocator() = default;

	/// Allocates inSize bytes of memory, returned memory address must be JPH_RVECTOR_ALIGNMENT byte aligned
	virtual void *					Allocate(uint inSize) = 0;

	/// Frees inSize bytes of memory located at inAddress
	virtual void					Free(void *inAddress, uint inSize) = 0;
};

/// Default implementation of the temp allocator that allocates a large block through malloc upfront
class JPH_EXPORT TempAllocatorImpl final : public TempAllocator
{
public:
	JPH_OVERRIDE_NEW_DELETE

	/// Constructs the allocator with a maximum allocatable size of inSize
	explicit						TempAllocatorImpl(uint inSize) :
		mBase(static_cast<uint8 *>(AlignedAllocate(inSize, JPH_RVECTOR_ALIGNMENT))),
		mSize(inSize)
	{
	}

	/// Destructor, frees the block
	virtual							~TempAllocatorImpl() override
	{
		JPH_ASSERT(mTop == 0);
		AlignedFree(mBase);
	}

	// See: TempAllocator
	virtual void *					Allocate(uint inSize) override
	{
		if (inSize == 0)
		{
			return nullptr;
		}
		else
		{
			uint new_top = mTop + AlignUp(inSize, JPH_RVECTOR_ALIGNMENT);
			if (new_top > mSize)
			{
				Trace("TempAllocator: Out of memory");
				std::abort();
			}
			void *address = mBase + mTop;
			mTop = new_top;
			return address;
		}
	}

	// See: TempAllocator
	virtual void					Free(void *inAddress, uint inSize) override
	{
		if (inAddress == nullptr)
		{
			JPH_ASSERT(inSize == 0);
		}
		else
		{
			mTop -= AlignUp(inSize, JPH_RVECTOR_ALIGNMENT);
			if (mBase + mTop != inAddress)
			{
				Trace("TempAllocator: Freeing in the wrong order");
				std::abort();
			}
		}
	}

	/// Check if no allocations have been made
	bool							IsEmpty() const
	{
		return mTop == 0;
	}

	/// Get the total size of the fixed buffer
	uint							GetSize() const
	{
		return mSize;
	}

	/// Get current usage in bytes of the buffer
	uint							GetUsage() const
	{
		return mTop;
	}

	/// Check if an allocation of inSize can be made in this fixed buffer allocator
	bool							CanAllocate(uint inSize) const
	{
		return mTop + AlignUp(inSize, JPH_RVECTOR_ALIGNMENT) <= mSize;
	}

	/// Check if memory block at inAddress is owned by this allocator
	bool							OwnsMemory(const void *inAddress) const
	{
		return inAddress >= mBase && inAddress < mBase + mSize;
	}

private:
	uint8 *							mBase;							///< Base address of the memory block
	uint							mSize;							///< Size of the memory block
	uint							mTop = 0;						///< End of currently allocated area
};

/// Implementation of the TempAllocator that just falls back to malloc/free
/// Note: This can be quite slow when running in the debugger as large memory blocks need to be initialized with 0xcd
class JPH_EXPORT TempAllocatorMalloc final : public TempAllocator
{
public:
	JPH_OVERRIDE_NEW_DELETE

	// See: TempAllocator
	virtual void *					Allocate(uint inSize) override
	{
		return inSize > 0? AlignedAllocate(inSize, JPH_RVECTOR_ALIGNMENT) : nullptr;
	}

	// See: TempAllocator
	virtual void					Free(void *inAddress, [[maybe_unused]] uint inSize) override
	{
		if (inAddress != nullptr)
			AlignedFree(inAddress);
	}
};

/// Implementation of the TempAllocator that tries to allocate from a large preallocated block, but falls back to malloc when it is exhausted
class JPH_EXPORT TempAllocatorImplWithMallocFallback final : public TempAllocator
{
public:
	JPH_OVERRIDE_NEW_DELETE

	/// Constructs the allocator with an initial fixed block if inSize
	explicit						TempAllocatorImplWithMallocFallback(uint inSize) :
		mAllocator(inSize)
	{
	}

	// See: TempAllocator
	virtual void *					Allocate(uint inSize) override
	{
		if (mAllocator.CanAllocate(inSize))
			return mAllocator.Allocate(inSize);
		else
			return mFallbackAllocator.Allocate(inSize);
	}

	// See: TempAllocator
	virtual void					Free(void *inAddress, uint inSize) override
	{
		if (inAddress == nullptr)
		{
			JPH_ASSERT(inSize == 0);
		}
		else
		{
			if (mAllocator.OwnsMemory(inAddress))
				mAllocator.Free(inAddress, inSize);
			else
				mFallbackAllocator.Free(inAddress, inSize);
		}
	}

private:
	TempAllocatorImpl				mAllocator;
	TempAllocatorMalloc				mFallbackAllocator;
};

JPH_NAMESPACE_END

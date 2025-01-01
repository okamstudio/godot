// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

JPH_NAMESPACE_BEGIN

template <typename Object>
FixedSizeFreeList<Object>::~FixedSizeFreeList()
{
	// Check if we got our Init call
	if (mPages != nullptr)
	{
		// Ensure everything is freed before the freelist is destructed
		JPH_ASSERT(mNumFreeObjects.load(memory_order_relaxed) == mNumPages * mPageSize);

		// Free memory for pages
		uint32 num_pages = mNumObjectsAllocated / mPageSize;
		for (uint32 page = 0; page < num_pages; ++page)
			AlignedFree(mPages[page]);
		Free(mPages);
	}
}

template <typename Object>
void FixedSizeFreeList<Object>::Init(uint inMaxObjects, uint inPageSize)
{
	// Check sanity
	JPH_ASSERT(inPageSize > 0 && IsPowerOf2(inPageSize));
	JPH_ASSERT(mPages == nullptr);

	// Store configuration parameters
	mNumPages = (inMaxObjects + inPageSize - 1) / inPageSize;
	mPageSize = inPageSize;
	mPageShift = CountTrailingZeros(inPageSize);
	mObjectMask = inPageSize - 1;
	JPH_IF_ENABLE_ASSERTS(mNumFreeObjects = mNumPages * inPageSize;)

	// Allocate page table
	mPages = reinterpret_cast<ObjectStorage **>(Allocate(mNumPages * sizeof(ObjectStorage *)));

	// We didn't yet use any objects of any page
	mNumObjectsAllocated = 0;
	mFirstFreeObjectInNewPage = 0;

	// Start with 1 as the first tag
	mAllocationTag = 1;

	// Set first free object (with tag 0)
	mFirstFreeObjectAndTag = cInvalidObjectIndex;
}

template <typename Object>
template <typename... Parameters>
uint32 FixedSizeFreeList<Object>::ConstructObject(Parameters &&... inParameters)
{
	for (;;)
	{
		// Get first object from the linked list
		uint64 first_free_object_and_tag = mFirstFreeObjectAndTag.load(memory_order_acquire);
		uint32 first_free = uint32(first_free_object_and_tag);
		if (first_free == cInvalidObjectIndex)
		{
			// The free list is empty, we take an object from the page that has never been used before
			first_free = mFirstFreeObjectInNewPage.fetch_add(1, memory_order_relaxed);
			if (first_free >= mNumObjectsAllocated)
			{
				// Allocate new page
				lock_guard lock(mPageMutex);
				while (first_free >= mNumObjectsAllocated)
				{
					uint32 next_page = mNumObjectsAllocated / mPageSize;
					if (next_page == mNumPages)
						return cInvalidObjectIndex; // Out of space!
					mPages[next_page] = reinterpret_cast<ObjectStorage *>(AlignedAllocate(mPageSize * sizeof(ObjectStorage), max<size_t>(alignof(ObjectStorage), JPH_CACHE_LINE_SIZE)));
					mNumObjectsAllocated += mPageSize;
				}
			}

			// Allocation successful
			JPH_IF_ENABLE_ASSERTS(mNumFreeObjects.fetch_sub(1, memory_order_relaxed);)
			ObjectStorage &storage = GetStorage(first_free);
			new (&storage.mObject) Object(std::forward<Parameters>(inParameters)...);
			storage.mNextFreeObject.store(first_free, memory_order_release);
			return first_free;
		}
		else
		{
			// Load next pointer
			uint32 new_first_free = GetStorage(first_free).mNextFreeObject.load(memory_order_acquire);

			// Construct a new first free object tag
			uint64 new_first_free_object_and_tag = uint64(new_first_free) + (uint64(mAllocationTag.fetch_add(1, memory_order_relaxed)) << 32);

			// Compare and swap
			if (mFirstFreeObjectAndTag.compare_exchange_weak(first_free_object_and_tag, new_first_free_object_and_tag, memory_order_release))
			{
				// Allocation successful
				JPH_IF_ENABLE_ASSERTS(mNumFreeObjects.fetch_sub(1, memory_order_relaxed);)
				ObjectStorage &storage = GetStorage(first_free);
				new (&storage.mObject) Object(std::forward<Parameters>(inParameters)...);
				storage.mNextFreeObject.store(first_free, memory_order_release);
				return first_free;
			}
		}
	}
}

template <typename Object>
void FixedSizeFreeList<Object>::AddObjectToBatch(Batch &ioBatch, uint32 inObjectIndex)
{
	JPH_ASSERT(ioBatch.mNumObjects != uint32(-1), "Trying to reuse a batch that has already been freed");

	// Reset next index
	atomic<uint32> &next_free_object = GetStorage(inObjectIndex).mNextFreeObject;
	JPH_ASSERT(next_free_object.load(memory_order_relaxed) == inObjectIndex, "Trying to add a object to the batch that is already in a free list");
	next_free_object.store(cInvalidObjectIndex, memory_order_release);

	// Link object in batch to free
	if (ioBatch.mFirstObjectIndex == cInvalidObjectIndex)
		ioBatch.mFirstObjectIndex = inObjectIndex;
	else
		GetStorage(ioBatch.mLastObjectIndex).mNextFreeObject.store(inObjectIndex, memory_order_release);
	ioBatch.mLastObjectIndex = inObjectIndex;
	ioBatch.mNumObjects++;
}

template <typename Object>
void FixedSizeFreeList<Object>::DestructObjectBatch(Batch &ioBatch)
{
	if (ioBatch.mFirstObjectIndex != cInvalidObjectIndex)
	{
		// Call destructors
		if constexpr (!std::is_trivially_destructible<Object>())
		{
			uint32 object_idx = ioBatch.mFirstObjectIndex;
			do
			{
				ObjectStorage &storage = GetStorage(object_idx);
				storage.mObject.~Object();
				object_idx = storage.mNextFreeObject.load(memory_order_relaxed);
			}
			while (object_idx != cInvalidObjectIndex);
		}

		// Add to objects free list
		ObjectStorage &storage = GetStorage(ioBatch.mLastObjectIndex);
		for (;;)
		{
			// Get first object from the list
			uint64 first_free_object_and_tag = mFirstFreeObjectAndTag.load(memory_order_acquire);
			uint32 first_free = uint32(first_free_object_and_tag);

			// Make it the next pointer of the last object in the batch that is to be freed
			storage.mNextFreeObject.store(first_free, memory_order_release);

			// Construct a new first free object tag
			uint64 new_first_free_object_and_tag = uint64(ioBatch.mFirstObjectIndex) + (uint64(mAllocationTag.fetch_add(1, memory_order_relaxed)) << 32);

			// Compare and swap
			if (mFirstFreeObjectAndTag.compare_exchange_weak(first_free_object_and_tag, new_first_free_object_and_tag, memory_order_release))
			{
				// Free successful
				JPH_IF_ENABLE_ASSERTS(mNumFreeObjects.fetch_add(ioBatch.mNumObjects, memory_order_relaxed);)

				// Mark the batch as freed
#ifdef JPH_ENABLE_ASSERTS
				ioBatch.mNumObjects = uint32(-1);
#endif
				return;
			}
		}
	}
}

template <typename Object>
void FixedSizeFreeList<Object>::DestructObject(uint32 inObjectIndex)
{
	JPH_ASSERT(inObjectIndex != cInvalidObjectIndex);

	// Call destructor
	ObjectStorage &storage = GetStorage(inObjectIndex);
	storage.mObject.~Object();

	// Add to object free list
	for (;;)
	{
		// Get first object from the list
		uint64 first_free_object_and_tag = mFirstFreeObjectAndTag.load(memory_order_acquire);
		uint32 first_free = uint32(first_free_object_and_tag);

		// Make it the next pointer of the last object in the batch that is to be freed
		storage.mNextFreeObject.store(first_free, memory_order_release);

		// Construct a new first free object tag
		uint64 new_first_free_object_and_tag = uint64(inObjectIndex) + (uint64(mAllocationTag.fetch_add(1, memory_order_relaxed)) << 32);

		// Compare and swap
		if (mFirstFreeObjectAndTag.compare_exchange_weak(first_free_object_and_tag, new_first_free_object_and_tag, memory_order_release))
		{
			// Free successful
			JPH_IF_ENABLE_ASSERTS(mNumFreeObjects.fetch_add(1, memory_order_relaxed);)
			return;
		}
	}
}

template<typename Object>
inline void FixedSizeFreeList<Object>::DestructObject(Object *inObject)
{
	uint32 index = reinterpret_cast<ObjectStorage *>(inObject)->mNextFreeObject.load(memory_order_relaxed);
	JPH_ASSERT(index < mNumObjectsAllocated);
	DestructObject(index);
}

JPH_NAMESPACE_END

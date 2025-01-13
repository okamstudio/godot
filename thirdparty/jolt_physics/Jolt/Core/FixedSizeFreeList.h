// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Jolt/Core/NonCopyable.h>
#include <Jolt/Core/Mutex.h>
#include <Jolt/Core/Atomics.h>

JPH_NAMESPACE_BEGIN

/// Class that allows lock free creation / destruction of objects (unless a new page of objects needs to be allocated)
/// It contains a fixed pool of objects and also allows batching up a lot of objects to be destroyed
/// and doing the actual free in a single atomic operation
template <typename Object>
class FixedSizeFreeList : public NonCopyable
{
private:
	/// Storage type for an Object
	struct ObjectStorage
	{
		/// The object we're storing
		Object				mObject;

		/// When the object is freed (or in the process of being freed as a batch) this will contain the next free object
		/// When an object is in use it will contain the object's index in the free list
		atomic<uint32>		mNextFreeObject;
	};

	static_assert(alignof(ObjectStorage) == alignof(Object), "Object not properly aligned");

	/// Access the object storage given the object index
	const ObjectStorage &	GetStorage(uint32 inObjectIndex) const	{ return mPages[inObjectIndex >> mPageShift][inObjectIndex & mObjectMask]; }
	ObjectStorage &			GetStorage(uint32 inObjectIndex)		{ return mPages[inObjectIndex >> mPageShift][inObjectIndex & mObjectMask]; }

	/// Size (in objects) of a single page
	uint32					mPageSize;

	/// Number of bits to shift an object index to the right to get the page number
	uint32					mPageShift;

	/// Mask to and an object index with to get the page number
	uint32					mObjectMask;

	/// Total number of pages that are usable
	uint32					mNumPages;

	/// Total number of objects that have been allocated
	uint32					mNumObjectsAllocated;

	/// Array of pages of objects
	ObjectStorage **		mPages = nullptr;

	/// Mutex that is used to allocate a new page if the storage runs out
	/// This variable is aligned to the cache line to prevent false sharing with
	/// the constants used to index into the list via `Get()`.
	alignas(JPH_CACHE_LINE_SIZE) Mutex mPageMutex;

	/// Number of objects that we currently have in the free list / new pages
#ifdef JPH_ENABLE_ASSERTS
	atomic<uint32>			mNumFreeObjects;
#endif // JPH_ENABLE_ASSERTS

	/// Simple counter that makes the first free object pointer update with every CAS so that we don't suffer from the ABA problem
	atomic<uint32>			mAllocationTag;

	/// Index of first free object, the first 32 bits of an object are used to point to the next free object
	atomic<uint64>			mFirstFreeObjectAndTag;

	/// The first free object to use when the free list is empty (may need to allocate a new page)
	atomic<uint32>			mFirstFreeObjectInNewPage;

public:
	/// Invalid index
	static const uint32		cInvalidObjectIndex = 0xffffffff;

	/// Size of an object + bookkeeping for the freelist
	static const int		ObjectStorageSize = sizeof(ObjectStorage);

	/// Destructor
	inline					~FixedSizeFreeList();

	/// Initialize the free list, up to inMaxObjects can be allocated
	inline void				Init(uint inMaxObjects, uint inPageSize);

	/// Lockless construct a new object, inParameters are passed on to the constructor
	template <typename... Parameters>
	inline uint32			ConstructObject(Parameters &&... inParameters);

	/// Lockless destruct an object and return it to the free pool
	inline void				DestructObject(uint32 inObjectIndex);

	/// Lockless destruct an object and return it to the free pool
	inline void				DestructObject(Object *inObject);

	/// A batch of objects that can be destructed
	struct Batch
	{
		uint32				mFirstObjectIndex = cInvalidObjectIndex;
		uint32				mLastObjectIndex = cInvalidObjectIndex;
		uint32				mNumObjects = 0;
	};

	/// Add a object to an existing batch to be destructed.
	/// Adding objects to a batch does not destroy or modify the objects, this will merely link them
	/// so that the entire batch can be returned to the free list in a single atomic operation
	inline void				AddObjectToBatch(Batch &ioBatch, uint32 inObjectIndex);

	/// Lockless destruct batch of objects
	inline void				DestructObjectBatch(Batch &ioBatch);

	/// Access an object by index.
	inline Object &			Get(uint32 inObjectIndex)				{ return GetStorage(inObjectIndex).mObject; }

	/// Access an object by index.
	inline const Object &	Get(uint32 inObjectIndex) const			{ return GetStorage(inObjectIndex).mObject; }
};

JPH_NAMESPACE_END

#include "FixedSizeFreeList.inl"

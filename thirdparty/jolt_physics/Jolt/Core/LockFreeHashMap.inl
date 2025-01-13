// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

JPH_NAMESPACE_BEGIN

///////////////////////////////////////////////////////////////////////////////////
// LFHMAllocator
///////////////////////////////////////////////////////////////////////////////////

inline LFHMAllocator::~LFHMAllocator()
{
	AlignedFree(mObjectStore);
}

inline void LFHMAllocator::Init(uint inObjectStoreSizeBytes)
{
	JPH_ASSERT(mObjectStore == nullptr);

	mObjectStoreSizeBytes = inObjectStoreSizeBytes;
	mObjectStore = reinterpret_cast<uint8 *>(JPH::AlignedAllocate(inObjectStoreSizeBytes, 16));
}

inline void LFHMAllocator::Clear()
{
	mWriteOffset = 0;
}

inline void LFHMAllocator::Allocate(uint32 inBlockSize, uint32 &ioBegin, uint32 &ioEnd)
{
	// If we're already beyond the end of our buffer then don't do an atomic add.
	// It's possible that many keys are inserted after the allocator is full, making it possible
	// for mWriteOffset (uint32) to wrap around to zero. When this happens, there will be a memory corruption.
	// This way, we will be able to progress the write offset beyond the size of the buffer
	// worst case by max <CPU count> * inBlockSize.
	if (mWriteOffset.load(memory_order_relaxed) >= mObjectStoreSizeBytes)
		return;

	// Atomically fetch a block from the pool
	uint32 begin = mWriteOffset.fetch_add(inBlockSize, memory_order_relaxed);
	uint32 end = min(begin + inBlockSize, mObjectStoreSizeBytes);

	if (ioEnd == begin)
	{
		// Block is allocated straight after our previous block
		begin = ioBegin;
	}
	else
	{
		// Block is a new block
		begin = min(begin, mObjectStoreSizeBytes);
	}

	// Store the begin and end of the resulting block
	ioBegin = begin;
	ioEnd = end;
}

template <class T>
inline uint32 LFHMAllocator::ToOffset(const T *inData) const
{
	const uint8 *data = reinterpret_cast<const uint8 *>(inData);
	JPH_ASSERT(data >= mObjectStore && data < mObjectStore + mObjectStoreSizeBytes);
	return uint32(data - mObjectStore);
}

template <class T>
inline T *LFHMAllocator::FromOffset(uint32 inOffset) const
{
	JPH_ASSERT(inOffset < mObjectStoreSizeBytes);
	return reinterpret_cast<T *>(mObjectStore + inOffset);
}

///////////////////////////////////////////////////////////////////////////////////
// LFHMAllocatorContext
///////////////////////////////////////////////////////////////////////////////////

inline LFHMAllocatorContext::LFHMAllocatorContext(LFHMAllocator &inAllocator, uint32 inBlockSize) :
	mAllocator(inAllocator),
	mBlockSize(inBlockSize)
{
}

inline bool LFHMAllocatorContext::Allocate(uint32 inSize, uint32 inAlignment, uint32 &outWriteOffset)
{
	// Calculate needed bytes for alignment
	JPH_ASSERT(IsPowerOf2(inAlignment));
	uint32 alignment_mask = inAlignment - 1;
	uint32 alignment = (inAlignment - (mBegin & alignment_mask)) & alignment_mask;

	// Check if we have space
	if (mEnd - mBegin < inSize + alignment)
	{
		// Allocate a new block
		mAllocator.Allocate(mBlockSize, mBegin, mEnd);

		// Update alignment
		alignment = (inAlignment - (mBegin & alignment_mask)) & alignment_mask;

		// Check if we have space again
		if (mEnd - mBegin < inSize + alignment)
			return false;
	}

	// Make the allocation
	mBegin += alignment;
	outWriteOffset = mBegin;
	mBegin += inSize;
	return true;
}

///////////////////////////////////////////////////////////////////////////////////
// LockFreeHashMap
///////////////////////////////////////////////////////////////////////////////////

template <class Key, class Value>
void LockFreeHashMap<Key, Value>::Init(uint32 inMaxBuckets)
{
	JPH_ASSERT(inMaxBuckets >= 4 && IsPowerOf2(inMaxBuckets));
	JPH_ASSERT(mBuckets == nullptr);

	mNumBuckets = inMaxBuckets;
	mMaxBuckets = inMaxBuckets;

	mBuckets = reinterpret_cast<atomic<uint32> *>(AlignedAllocate(inMaxBuckets * sizeof(atomic<uint32>), 16));

	Clear();
}

template <class Key, class Value>
LockFreeHashMap<Key, Value>::~LockFreeHashMap()
{
	AlignedFree(mBuckets);
}

template <class Key, class Value>
void LockFreeHashMap<Key, Value>::Clear()
{
#ifdef JPH_ENABLE_ASSERTS
	// Reset number of key value pairs
	mNumKeyValues = 0;
#endif // JPH_ENABLE_ASSERTS

	// Reset buckets 4 at a time
	static_assert(sizeof(atomic<uint32>) == sizeof(uint32));
	UVec4 invalid_handle = UVec4::sReplicate(cInvalidHandle);
	uint32 *start = reinterpret_cast<uint32 *>(mBuckets);
	const uint32 *end = start + mNumBuckets;
	JPH_ASSERT(IsAligned(start, 16));
	while (start < end)
	{
		invalid_handle.StoreInt4Aligned(start);
		start += 4;
	}
}

template <class Key, class Value>
void LockFreeHashMap<Key, Value>::SetNumBuckets(uint32 inNumBuckets)
{
	JPH_ASSERT(mNumKeyValues == 0);
	JPH_ASSERT(inNumBuckets <= mMaxBuckets);
	JPH_ASSERT(inNumBuckets >= 4 && IsPowerOf2(inNumBuckets));

	mNumBuckets = inNumBuckets;
}

template <class Key, class Value>
template <class... Params>
inline typename LockFreeHashMap<Key, Value>::KeyValue *LockFreeHashMap<Key, Value>::Create(LFHMAllocatorContext &ioContext, const Key &inKey, uint64 inKeyHash, int inExtraBytes, Params &&... inConstructorParams)
{
	// This is not a multi map, test the key hasn't been inserted yet
	JPH_ASSERT(Find(inKey, inKeyHash) == nullptr);

	// Calculate total size
	uint size = sizeof(KeyValue) + inExtraBytes;

	// Get the write offset for this key value pair
	uint32 write_offset;
	if (!ioContext.Allocate(size, alignof(KeyValue), write_offset))
		return nullptr;

#ifdef JPH_ENABLE_ASSERTS
	// Increment amount of entries in map
	mNumKeyValues.fetch_add(1, memory_order_relaxed);
#endif // JPH_ENABLE_ASSERTS

	// Construct the key/value pair
	KeyValue *kv = mAllocator.template FromOffset<KeyValue>(write_offset);
	JPH_ASSERT(intptr_t(kv) % alignof(KeyValue) == 0);
#ifdef JPH_DEBUG
	memset(kv, 0xcd, size);
#endif
	kv->mKey = inKey;
	new (&kv->mValue) Value(std::forward<Params>(inConstructorParams)...);

	// Get the offset to the first object from the bucket with corresponding hash
	atomic<uint32> &offset = mBuckets[inKeyHash & (mNumBuckets - 1)];

	// Add this entry as the first element in the linked list
	uint32 old_offset = offset.load(memory_order_relaxed);
	for (;;)
	{
		kv->mNextOffset = old_offset;
		if (offset.compare_exchange_weak(old_offset, write_offset, memory_order_release))
			break;
	}

	return kv;
}

template <class Key, class Value>
inline const typename LockFreeHashMap<Key, Value>::KeyValue *LockFreeHashMap<Key, Value>::Find(const Key &inKey, uint64 inKeyHash) const
{
	// Get the offset to the keyvalue object from the bucket with corresponding hash
	uint32 offset = mBuckets[inKeyHash & (mNumBuckets - 1)].load(memory_order_acquire);
	while (offset != cInvalidHandle)
	{
		// Loop through linked list of values until the right one is found
		const KeyValue *kv = mAllocator.template FromOffset<const KeyValue>(offset);
		if (kv->mKey == inKey)
			return kv;
		offset = kv->mNextOffset;
	}

	// Not found
	return nullptr;
}

template <class Key, class Value>
inline uint32 LockFreeHashMap<Key, Value>::ToHandle(const KeyValue *inKeyValue) const
{
	return mAllocator.ToOffset(inKeyValue);
}

template <class Key, class Value>
inline const typename LockFreeHashMap<Key, Value>::KeyValue *LockFreeHashMap<Key, Value>::FromHandle(uint32 inHandle) const
{
	return mAllocator.template FromOffset<const KeyValue>(inHandle);
}

template <class Key, class Value>
inline void LockFreeHashMap<Key, Value>::GetAllKeyValues(Array<const KeyValue *> &outAll) const
{
	for (const atomic<uint32> *bucket = mBuckets; bucket < mBuckets + mNumBuckets; ++bucket)
	{
		uint32 offset = *bucket;
		while (offset != cInvalidHandle)
		{
			const KeyValue *kv = mAllocator.template FromOffset<const KeyValue>(offset);
			outAll.push_back(kv);
			offset = kv->mNextOffset;
		}
	}
}

template <class Key, class Value>
typename LockFreeHashMap<Key, Value>::Iterator LockFreeHashMap<Key, Value>::begin()
{
	// Start with the first bucket
	Iterator it { this, 0, mBuckets[0] };

	// If it doesn't contain a valid entry, use the ++ operator to find the first valid entry
	if (it.mOffset == cInvalidHandle)
		++it;

	return it;
}

template <class Key, class Value>
typename LockFreeHashMap<Key, Value>::Iterator LockFreeHashMap<Key, Value>::end()
{
	return { this, mNumBuckets, cInvalidHandle };
}

template <class Key, class Value>
typename LockFreeHashMap<Key, Value>::KeyValue &LockFreeHashMap<Key, Value>::Iterator::operator* ()
{
	JPH_ASSERT(mOffset != cInvalidHandle);

	return *mMap->mAllocator.template FromOffset<KeyValue>(mOffset);
}

template <class Key, class Value>
typename LockFreeHashMap<Key, Value>::Iterator &LockFreeHashMap<Key, Value>::Iterator::operator++ ()
{
	JPH_ASSERT(mBucket < mMap->mNumBuckets);

	// Find the next key value in this bucket
	if (mOffset != cInvalidHandle)
	{
		const KeyValue *kv = mMap->mAllocator.template FromOffset<const KeyValue>(mOffset);
		mOffset = kv->mNextOffset;
		if (mOffset != cInvalidHandle)
			return *this;
	}

	// Loop over next buckets
	for (;;)
	{
		// Next bucket
		++mBucket;
		if (mBucket >= mMap->mNumBuckets)
			return *this;

		// Fetch the first entry in the bucket
		mOffset = mMap->mBuckets[mBucket];
		if (mOffset != cInvalidHandle)
			return *this;
	}
}

#ifdef JPH_DEBUG

template <class Key, class Value>
void LockFreeHashMap<Key, Value>::TraceStats() const
{
	const int cMaxPerBucket = 256;

	int max_objects_per_bucket = 0;
	int num_objects = 0;
	int histogram[cMaxPerBucket];
	for (int i = 0; i < cMaxPerBucket; ++i)
		histogram[i] = 0;

	for (atomic<uint32> *bucket = mBuckets, *bucket_end = mBuckets + mNumBuckets; bucket < bucket_end; ++bucket)
	{
		int objects_in_bucket = 0;
		uint32 offset = *bucket;
		while (offset != cInvalidHandle)
		{
			const KeyValue *kv = mAllocator.template FromOffset<const KeyValue>(offset);
			offset = kv->mNextOffset;
			++objects_in_bucket;
			++num_objects;
		}
		max_objects_per_bucket = max(objects_in_bucket, max_objects_per_bucket);
		histogram[min(objects_in_bucket, cMaxPerBucket - 1)]++;
	}

	Trace("max_objects_per_bucket = %d, num_buckets = %u, num_objects = %d", max_objects_per_bucket, mNumBuckets, num_objects);

	for (int i = 0; i < cMaxPerBucket; ++i)
		if (histogram[i] != 0)
			Trace("%d: %d", i, histogram[i]);
}

#endif

JPH_NAMESPACE_END

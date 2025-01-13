// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Jolt/Core/NonCopyable.h>
#include <Jolt/Core/Atomics.h>

JPH_NAMESPACE_BEGIN

/// Allocator for a lock free hash map
class LFHMAllocator : public NonCopyable
{
public:
	/// Destructor
	inline					~LFHMAllocator();

	/// Initialize the allocator
	/// @param inObjectStoreSizeBytes Number of bytes to reserve for all key value pairs
	inline void				Init(uint inObjectStoreSizeBytes);

	/// Clear all allocations
	inline void				Clear();

	/// Allocate a new block of data
	/// @param inBlockSize Size of block to allocate (will potentially return a smaller block if memory is full).
	/// @param ioBegin Should be the start of the first free byte in current memory block on input, will contain the start of the first free byte in allocated block on return.
	/// @param ioEnd Should be the byte beyond the current memory block on input, will contain the byte beyond the allocated block on return.
	inline void				Allocate(uint32 inBlockSize, uint32 &ioBegin, uint32 &ioEnd);

	/// Convert a pointer to an offset
	template <class T>
	inline uint32			ToOffset(const T *inData) const;

	/// Convert an offset to a pointer
	template <class T>
	inline T *				FromOffset(uint32 inOffset) const;

private:
	uint8 *					mObjectStore = nullptr;			///< This contains a contiguous list of objects (possibly of varying size)
	uint32					mObjectStoreSizeBytes = 0;		///< The size of mObjectStore in bytes
	atomic<uint32>			mWriteOffset { 0 };				///< Next offset to write to in mObjectStore
};

/// Allocator context object for a lock free hash map that allocates a larger memory block at once and hands it out in smaller portions.
/// This avoids contention on the atomic LFHMAllocator::mWriteOffset.
class LFHMAllocatorContext : public NonCopyable
{
public:
	/// Construct a new allocator context
	inline					LFHMAllocatorContext(LFHMAllocator &inAllocator, uint32 inBlockSize);

	/// @brief Allocate data block
	/// @param inSize Size of block to allocate.
	/// @param inAlignment Alignment of block to allocate.
	/// @param outWriteOffset Offset in buffer where block is located
	/// @return True if allocation succeeded
	inline bool				Allocate(uint32 inSize, uint32 inAlignment, uint32 &outWriteOffset);

private:
	LFHMAllocator &			mAllocator;
	uint32					mBlockSize;
	uint32					mBegin = 0;
	uint32					mEnd = 0;
};

/// Very simple lock free hash map that only allows insertion, retrieval and provides a fixed amount of buckets and fixed storage.
/// Note: This class currently assumes key and value are simple types that need no calls to the destructor.
template <class Key, class Value>
class LockFreeHashMap : public NonCopyable
{
public:
	using MapType = LockFreeHashMap<Key, Value>;

	/// Destructor
	explicit				LockFreeHashMap(LFHMAllocator &inAllocator) : mAllocator(inAllocator) { }
							~LockFreeHashMap();

	/// Initialization
	/// @param inMaxBuckets Max amount of buckets to use in the hashmap. Must be power of 2.
	void					Init(uint32 inMaxBuckets);

	/// Remove all elements.
	/// Note that this cannot happen simultaneously with adding new elements.
	void					Clear();

	/// Get the current amount of buckets that the map is using
	uint32					GetNumBuckets() const			{ return mNumBuckets; }

	/// Get the maximum amount of buckets that this map supports
	uint32					GetMaxBuckets() const			{ return mMaxBuckets; }

	/// Update the number of buckets. This must be done after clearing the map and cannot be done concurrently with any other operations on the map.
	/// Note that the number of buckets can never become bigger than the specified max buckets during initialization and that it must be a power of 2.
	void					SetNumBuckets(uint32 inNumBuckets);

	/// A key / value pair that is inserted in the map
	class KeyValue
	{
	public:
		const Key &			GetKey() const					{ return mKey; }
		Value &				GetValue()						{ return mValue; }
		const Value &		GetValue() const				{ return mValue; }

	private:
		template <class K, class V> friend class LockFreeHashMap;

		Key					mKey;							///< Key for this entry
		uint32				mNextOffset;					///< Offset in mObjectStore of next KeyValue entry with same hash
		Value				mValue;							///< Value for this entry + optionally extra bytes
	};

	/// Insert a new element, returns null if map full.
	/// Multiple threads can be inserting in the map at the same time.
	template <class... Params>
	inline KeyValue *		Create(LFHMAllocatorContext &ioContext, const Key &inKey, uint64 inKeyHash, int inExtraBytes, Params &&... inConstructorParams);

	/// Find an element, returns null if not found
	inline const KeyValue *	Find(const Key &inKey, uint64 inKeyHash) const;

	/// Value of an invalid handle
	const static uint32		cInvalidHandle = uint32(-1);

	/// Get convert key value pair to uint32 handle
	inline uint32			ToHandle(const KeyValue *inKeyValue) const;

	/// Convert uint32 handle back to key and value
	inline const KeyValue *	FromHandle(uint32 inHandle) const;

#ifdef JPH_ENABLE_ASSERTS
	/// Get the number of key value pairs that this map currently contains.
	/// Available only when asserts are enabled because adding elements creates contention on this atomic and negatively affects performance.
	inline uint32			GetNumKeyValues() const			{ return mNumKeyValues; }
#endif // JPH_ENABLE_ASSERTS

	/// Get all key/value pairs
	inline void				GetAllKeyValues(Array<const KeyValue *> &outAll) const;

	/// Non-const iterator
	struct Iterator
	{
		/// Comparison
		bool				operator == (const Iterator &inRHS) const	{ return mMap == inRHS.mMap && mBucket == inRHS.mBucket && mOffset == inRHS.mOffset; }
		bool				operator != (const Iterator &inRHS) const	{ return !(*this == inRHS); }

		/// Convert to key value pair
		KeyValue &			operator * ();

		/// Next item
		Iterator &			operator ++ ();

		MapType *			mMap;
		uint32				mBucket;
		uint32				mOffset;
	};

	/// Iterate over the map, note that it is not safe to do this in parallel to Clear().
	/// It is safe to do this while adding elements to the map, but newly added elements may or may not be returned by the iterator.
	Iterator				begin();
	Iterator				end();

#ifdef JPH_DEBUG
	/// Output stats about this map to the log
	void					TraceStats() const;
#endif

private:
	LFHMAllocator &			mAllocator;						///< Allocator used to allocate key value pairs

#ifdef JPH_ENABLE_ASSERTS
	atomic<uint32>			mNumKeyValues = 0;				///< Number of key value pairs in the store
#endif // JPH_ENABLE_ASSERTS

	atomic<uint32> *		mBuckets = nullptr;				///< This contains the offset in mObjectStore of the first object with a particular hash
	uint32					mNumBuckets = 0;				///< Current number of buckets
	uint32					mMaxBuckets = 0;				///< Maximum number of buckets
};

JPH_NAMESPACE_END

#include "LockFreeHashMap.inl"

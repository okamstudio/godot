// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2024 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Jolt/Math/BVec16.h>

JPH_NAMESPACE_BEGIN

/// Helper class for implementing an UnorderedSet or UnorderedMap
/// Based on CppCon 2017: Matt Kulukundis "Designing a Fast, Efficient, Cache-friendly Hash Table, Step by Step"
/// See: https://www.youtube.com/watch?v=ncHmEUmJZf4
template <class Key, class KeyValue, class HashTableDetail, class Hash, class KeyEqual>
class HashTable
{
public:
	/// Properties
	using value_type = KeyValue;
	using size_type = uint32;
	using difference_type = ptrdiff_t;

private:
	/// Base class for iterators
	template <class Table, class Iterator>
	class IteratorBase
	{
	public:
        /// Properties
		using difference_type = typename Table::difference_type;
        using value_type = typename Table::value_type;
        using iterator_category = std::forward_iterator_tag;

		/// Copy constructor
							IteratorBase(const IteratorBase &inRHS) = default;

		/// Assignment operator
		IteratorBase &		operator = (const IteratorBase &inRHS) = default;

		/// Iterator at start of table
		explicit			IteratorBase(Table *inTable) :
			mTable(inTable),
			mIndex(0)
		{
			while (mIndex < mTable->mMaxSize && (mTable->mControl[mIndex] & cBucketUsed) == 0)
				++mIndex;
		}

		/// Iterator at specific index
							IteratorBase(Table *inTable, size_type inIndex) :
			mTable(inTable),
			mIndex(inIndex)
		{
		}

		/// Prefix increment
		Iterator &			operator ++ ()
		{
			JPH_ASSERT(IsValid());

			do
			{
				++mIndex;
			}
			while (mIndex < mTable->mMaxSize && (mTable->mControl[mIndex] & cBucketUsed) == 0);

			return static_cast<Iterator &>(*this);
		}

		/// Postfix increment
		Iterator			operator ++ (int)
		{
			Iterator result(mTable, mIndex);
			++(*this);
			return result;
		}

		/// Access to key value pair
		const KeyValue &	operator * () const
		{
			JPH_ASSERT(IsValid());
			return mTable->mData[mIndex];
		}

		/// Access to key value pair
		const KeyValue *	operator -> () const
		{
			JPH_ASSERT(IsValid());
			return mTable->mData + mIndex;
		}

		/// Equality operator
		bool				operator == (const Iterator &inRHS) const
		{
			return mIndex == inRHS.mIndex && mTable == inRHS.mTable;
		}

		/// Inequality operator
		bool				operator != (const Iterator &inRHS) const
		{
			return !(*this == inRHS);
		}

		/// Check that the iterator is valid
		bool				IsValid() const
		{
			return mIndex < mTable->mMaxSize
				&& (mTable->mControl[mIndex] & cBucketUsed) != 0;
		}

		Table *				mTable;
		size_type			mIndex;
	};

	/// Get the maximum number of elements that we can support given a number of buckets
	static constexpr size_type sGetMaxLoad(size_type inBucketCount)
	{
		return uint32((cMaxLoadFactorNumerator * inBucketCount) / cMaxLoadFactorDenominator);
	}

	/// Update the control value for a bucket
	JPH_INLINE void			SetControlValue(size_type inIndex, uint8 inValue)
	{
		JPH_ASSERT(inIndex < mMaxSize);
		mControl[inIndex] = inValue;

		// Mirror the first 15 bytes to the 15 bytes beyond mMaxSize
		// Note that this is equivalent to:
		// if (inIndex < 15)
		//   mControl[inIndex + mMaxSize] = inValue
		// else
		//   mControl[inIndex] = inValue
		// Which performs a needless write if inIndex >= 15 but at least it is branch-less
		mControl[((inIndex - 15) & (mMaxSize - 1)) + 15] = inValue;
	}

	/// Get the index and control value for a particular key
	JPH_INLINE void			GetIndexAndControlValue(const Key &inKey, size_type &outIndex, uint8 &outControl) const
	{
		// Calculate hash
		uint64 hash_value = Hash { } (inKey);

		// Split hash into index and control value
		outIndex = size_type(hash_value >> 7) & (mMaxSize - 1);
		outControl = cBucketUsed | uint8(hash_value);
	}

	/// Allocate space for the hash table
	void					AllocateTable(size_type inMaxSize)
	{
		JPH_ASSERT(mData == nullptr);

		mMaxSize = inMaxSize;
		mLoadLeft = sGetMaxLoad(inMaxSize);
		size_t required_size = size_t(mMaxSize) * (sizeof(KeyValue) + 1) + 15; // Add 15 bytes to mirror the first 15 bytes of the control values
		if constexpr (cNeedsAlignedAllocate)
			mData = reinterpret_cast<KeyValue *>(AlignedAllocate(required_size, alignof(KeyValue)));
		else
			mData = reinterpret_cast<KeyValue *>(Allocate(required_size));
		mControl = reinterpret_cast<uint8 *>(mData + mMaxSize);
	}

	/// Copy the contents of another hash table
	void					CopyTable(const HashTable &inRHS)
	{
		if (inRHS.empty())
			return;

		AllocateTable(inRHS.mMaxSize);

		// Copy control bytes
		memcpy(mControl, inRHS.mControl, mMaxSize + 15);

		// Copy elements
		uint index = 0;
		for (const uint8 *control = mControl, *control_end = mControl + mMaxSize; control != control_end; ++control, ++index)
			if (*control & cBucketUsed)
				::new (mData + index) KeyValue(inRHS.mData[index]);
		mSize = inRHS.mSize;
	}

	/// Grow the table to the next power of 2
	void					GrowTable()
	{
		// Calculate new size
		size_type new_max_size = max<size_type>(mMaxSize << 1, 16);
		if (new_max_size < mMaxSize)
		{
			JPH_ASSERT(false, "Overflow in hash table size, can't grow!");
			return;
		}

		// Move the old table to a temporary structure
		size_type old_max_size = mMaxSize;
		KeyValue *old_data = mData;
		const uint8 *old_control = mControl;
		mData = nullptr;
		mControl = nullptr;
		mSize = 0;
		mMaxSize = 0;
		mLoadLeft = 0;

		// Allocate new table
		AllocateTable(new_max_size);

		// Reset all control bytes
		memset(mControl, cBucketEmpty, mMaxSize + 15);

		if (old_data != nullptr)
		{
			// Copy all elements from the old table
			for (size_type i = 0; i < old_max_size; ++i)
				if (old_control[i] & cBucketUsed)
				{
					size_type index;
					KeyValue *element = old_data + i;
					JPH_IF_ENABLE_ASSERTS(bool inserted =) InsertKey</* InsertAfterGrow= */ true>(HashTableDetail::sGetKey(*element), index);
					JPH_ASSERT(inserted);
					::new (mData + index) KeyValue(std::move(*element));
					element->~KeyValue();
				}

			// Free memory
			if constexpr (cNeedsAlignedAllocate)
				AlignedFree(old_data);
			else
				Free(old_data);
		}
	}

protected:
	/// Get an element by index
	KeyValue &				GetElement(size_type inIndex) const
	{
		return mData[inIndex];
	}

	/// Insert a key into the map, returns true if the element was inserted, false if it already existed.
	/// outIndex is the index at which the element should be constructed / where it is located.
	template <bool InsertAfterGrow = false>
	bool					InsertKey(const Key &inKey, size_type &outIndex)
	{
		// Ensure we have enough space
		if (mLoadLeft == 0)
		{
			// Should not be growing if we're already growing!
			if constexpr (InsertAfterGrow)
				JPH_ASSERT(false);

			// Decide if we need to clean up all tombstones or if we need to grow the map
			size_type num_deleted = sGetMaxLoad(mMaxSize) - mSize;
			if (num_deleted * cMaxDeletedElementsDenominator > mMaxSize * cMaxDeletedElementsNumerator)
				rehash(0);
			else
				GrowTable();
		}

		// Split hash into index and control value
		size_type index;
		uint8 control;
		GetIndexAndControlValue(inKey, index, control);

		// Keeps track of the index of the first deleted bucket we found
		constexpr size_type cNoDeleted = ~size_type(0);
		size_type first_deleted_index = cNoDeleted;

		// Linear probing
		KeyEqual equal;
		size_type bucket_mask = mMaxSize - 1;
		BVec16 control16 = BVec16::sReplicate(control);
		BVec16 bucket_empty = BVec16::sZero();
		BVec16 bucket_deleted = BVec16::sReplicate(cBucketDeleted);
		for (;;)
		{
			// Read 16 control values (note that we added 15 bytes at the end of the control values that mirror the first 15 bytes)
			BVec16 control_bytes = BVec16::sLoadByte16(mControl + index);

			// Check if we must find the element before we can insert
			if constexpr (!InsertAfterGrow)
			{
				// Check for the control value we're looking for
				// Note that when deleting we can create empty buckets instead of deleted buckets.
				// This means we must unconditionally check all buckets in this batch for equality
				// (also beyond the first empty bucket).
				uint32 control_equal = uint32(BVec16::sEquals(control_bytes, control16).GetTrues());

				// Index within the 16 buckets
				size_type local_index = index;

				// Loop while there's still buckets to process
				while (control_equal != 0)
				{
					// Get the first equal bucket
					uint first_equal = CountTrailingZeros(control_equal);

					// Skip to the bucket
					local_index += first_equal;

					// Make sure that our index is not beyond the end of the table
					local_index &= bucket_mask;

					// We found a bucket with same control value
					if (equal(HashTableDetail::sGetKey(mData[local_index]), inKey))
					{
						// Element already exists
						outIndex = local_index;
						return false;
					}

					// Skip past this bucket
					control_equal >>= first_equal + 1;
					local_index++;
				}

				// Check if we're still scanning for deleted buckets
				if (first_deleted_index == cNoDeleted)
				{
					// Check if any buckets have been deleted, if so store the first one
					uint32 control_deleted = uint32(BVec16::sEquals(control_bytes, bucket_deleted).GetTrues());
					if (control_deleted != 0)
						first_deleted_index = index + CountTrailingZeros(control_deleted);
				}
			}

			// Check for empty buckets
			uint32 control_empty = uint32(BVec16::sEquals(control_bytes, bucket_empty).GetTrues());
			if (control_empty != 0)
			{
				// If we found a deleted bucket, use it.
				// It doesn't matter if it is before or after the first empty bucket we found
				// since we will always be scanning in batches of 16 buckets.
				if (first_deleted_index == cNoDeleted || InsertAfterGrow)
				{
					index += CountTrailingZeros(control_empty);
					--mLoadLeft; // Using an empty bucket decreases the load left
				}
				else
				{
					index = first_deleted_index;
				}

				// Make sure that our index is not beyond the end of the table
				index &= bucket_mask;

				// Update control byte
				SetControlValue(index, control);
				++mSize;

				// Return index to newly allocated bucket
				outIndex = index;
				return true;
			}

			// Move to next batch of 16 buckets
			index = (index + 16) & bucket_mask;
		}
	}

public:
	/// Non-const iterator
	class iterator : public IteratorBase<HashTable, iterator>
	{
		using Base = IteratorBase<HashTable, iterator>;

	public:
        /// Properties
        using reference = typename Base::value_type &;
        using pointer = typename Base::value_type *;

		/// Constructors
		explicit			iterator(HashTable *inTable) : Base(inTable) { }
							iterator(HashTable *inTable, size_type inIndex) : Base(inTable, inIndex) { }
							iterator(const iterator &inIterator) : Base(inIterator) { }

		/// Assignment
		iterator &			operator = (const iterator &inRHS) { Base::operator = (inRHS); return *this; }

		using Base::operator *;

		/// Non-const access to key value pair
		KeyValue &			operator * ()
		{
			JPH_ASSERT(this->IsValid());
			return this->mTable->mData[this->mIndex];
		}

		using Base::operator ->;

		/// Non-const access to key value pair
		KeyValue *			operator -> ()
		{
			JPH_ASSERT(this->IsValid());
			return this->mTable->mData + this->mIndex;
		}
	};

	/// Const iterator
	class const_iterator : public IteratorBase<const HashTable, const_iterator>
	{
		using Base = IteratorBase<const HashTable, const_iterator>;

	public:
        /// Properties
        using reference = const typename Base::value_type &;
        using pointer = const typename Base::value_type *;

		/// Constructors
		explicit			const_iterator(const HashTable *inTable) : Base(inTable) { }
							const_iterator(const HashTable *inTable, size_type inIndex) : Base(inTable, inIndex) { }
							const_iterator(const const_iterator &inRHS) : Base(inRHS) { }
							const_iterator(const iterator &inIterator) : Base(inIterator.mTable, inIterator.mIndex) { }

		/// Assignment
		const_iterator &	operator = (const iterator &inRHS) { this->mTable = inRHS.mTable; this->mIndex = inRHS.mIndex; return *this; }
		const_iterator &	operator = (const const_iterator &inRHS) { Base::operator = (inRHS); return *this; }
	};

	/// Default constructor
							HashTable() = default;

	/// Copy constructor
							HashTable(const HashTable &inRHS)
	{
		CopyTable(inRHS);
	}

	/// Move constructor
							HashTable(HashTable &&ioRHS) noexcept :
		mData(ioRHS.mData),
		mControl(ioRHS.mControl),
		mSize(ioRHS.mSize),
		mMaxSize(ioRHS.mMaxSize),
		mLoadLeft(ioRHS.mLoadLeft)
	{
		ioRHS.mData = nullptr;
		ioRHS.mControl = nullptr;
		ioRHS.mSize = 0;
		ioRHS.mMaxSize = 0;
		ioRHS.mLoadLeft = 0;
	}

	/// Assignment operator
	HashTable &				operator = (const HashTable &inRHS)
	{
		if (this != &inRHS)
		{
			clear();

			CopyTable(inRHS);
		}

		return *this;
	}

	/// Move assignment operator
	HashTable &				operator = (HashTable &&ioRHS) noexcept
	{
		if (this != &ioRHS)
		{
			clear();

			mData = ioRHS.mData;
			mControl = ioRHS.mControl;
			mSize = ioRHS.mSize;
			mMaxSize = ioRHS.mMaxSize;
			mLoadLeft = ioRHS.mLoadLeft;

			ioRHS.mData = nullptr;
			ioRHS.mControl = nullptr;
			ioRHS.mSize = 0;
			ioRHS.mMaxSize = 0;
			ioRHS.mLoadLeft = 0;
		}

		return *this;
	}

	/// Destructor
							~HashTable()
	{
		clear();
	}

	/// Reserve memory for a certain number of elements
	void					reserve(size_type inMaxSize)
	{
		// Calculate max size based on load factor
		size_type max_size = GetNextPowerOf2(max<uint32>((cMaxLoadFactorDenominator * inMaxSize) / cMaxLoadFactorNumerator, 16));
		if (max_size <= mMaxSize)
			return;

		// Allocate buffers
		AllocateTable(max_size);

		// Reset all control bytes
		memset(mControl, cBucketEmpty, mMaxSize + 15);
	}

	/// Destroy the entire hash table
	void					clear()
	{
		// Delete all elements
		if constexpr (!std::is_trivially_destructible<KeyValue>())
			if (!empty())
				for (size_type i = 0; i < mMaxSize; ++i)
					if (mControl[i] & cBucketUsed)
						mData[i].~KeyValue();

		if (mData != nullptr)
		{
			// Free memory
			if constexpr (cNeedsAlignedAllocate)
				AlignedFree(mData);
			else
				Free(mData);

			// Reset members
			mData = nullptr;
			mControl = nullptr;
			mSize = 0;
			mMaxSize = 0;
			mLoadLeft = 0;
		}
	}

	/// Iterator to first element
	iterator				begin()
	{
		return iterator(this);
	}

	/// Iterator to one beyond last element
	iterator				end()
	{
		return iterator(this, mMaxSize);
	}

	/// Iterator to first element
	const_iterator			begin() const
	{
		return const_iterator(this);
	}

	/// Iterator to one beyond last element
	const_iterator			end() const
	{
		return const_iterator(this, mMaxSize);
	}

	/// Iterator to first element
	const_iterator			cbegin() const
	{
		return const_iterator(this);
	}

	/// Iterator to one beyond last element
	const_iterator			cend() const
	{
		return const_iterator(this, mMaxSize);
	}

	/// Number of buckets in the table
	size_type				bucket_count() const
	{
		return mMaxSize;
	}

	/// Max number of buckets that the table can have
	constexpr size_type		max_bucket_count() const
	{
		return size_type(1) << (sizeof(size_type) * 8 - 1);
	}

	/// Check if there are no elements in the table
	bool					empty() const
	{
		return mSize == 0;
	}

	/// Number of elements in the table
	size_type				size() const
	{
		return mSize;
	}

	/// Max number of elements that the table can hold
	constexpr size_type		max_size() const
	{
		return size_type((uint64(max_bucket_count()) * cMaxLoadFactorNumerator) / cMaxLoadFactorDenominator);
	}

	/// Get the max load factor for this table (max number of elements / number of buckets)
	constexpr float			max_load_factor() const
	{
		return float(cMaxLoadFactorNumerator) / float(cMaxLoadFactorDenominator);
	}

	/// Insert a new element, returns iterator and if the element was inserted
	std::pair<iterator, bool> insert(const value_type &inValue)
	{
		size_type index;
		bool inserted = InsertKey(HashTableDetail::sGetKey(inValue), index);
		if (inserted)
			::new (mData + index) KeyValue(inValue);
		return std::make_pair(iterator(this, index), inserted);
	}

	/// Find an element, returns iterator to element or end() if not found
	const_iterator			find(const Key &inKey) const
	{
		// Check if we have any data
		if (empty())
			return cend();

		// Split hash into index and control value
		size_type index;
		uint8 control;
		GetIndexAndControlValue(inKey, index, control);

		// Linear probing
		KeyEqual equal;
		size_type bucket_mask = mMaxSize - 1;
		BVec16 control16 = BVec16::sReplicate(control);
		BVec16 bucket_empty = BVec16::sZero();
		for (;;)
		{
			// Read 16 control values
			// (note that we added 15 bytes at the end of the control values that mirror the first 15 bytes)
			BVec16 control_bytes = BVec16::sLoadByte16(mControl + index);

			// Check for the control value we're looking for
			// Note that when deleting we can create empty buckets instead of deleted buckets.
			// This means we must unconditionally check all buckets in this batch for equality
			// (also beyond the first empty bucket).
			uint32 control_equal = uint32(BVec16::sEquals(control_bytes, control16).GetTrues());

			// Index within the 16 buckets
			size_type local_index = index;

			// Loop while there's still buckets to process
			while (control_equal != 0)
			{
				// Get the first equal bucket
				uint first_equal = CountTrailingZeros(control_equal);

				// Skip to the bucket
				local_index += first_equal;

				// Make sure that our index is not beyond the end of the table
				local_index &= bucket_mask;

				// We found a bucket with same control value
				if (equal(HashTableDetail::sGetKey(mData[local_index]), inKey))
				{
					// Element found
					return const_iterator(this, local_index);
				}

				// Skip past this bucket
				control_equal >>= first_equal + 1;
				local_index++;
			}

			// Check for empty buckets
			uint32 control_empty = uint32(BVec16::sEquals(control_bytes, bucket_empty).GetTrues());
			if (control_empty != 0)
			{
				// An empty bucket was found, we didn't find the element
				return cend();
			}

			// Move to next batch of 16 buckets
			index = (index + 16) & bucket_mask;
		}
	}

	/// @brief Erase an element by iterator
	void					erase(const const_iterator &inIterator)
	{
		JPH_ASSERT(inIterator.IsValid());

		// Read 16 control values before and after the current index
		// (note that we added 15 bytes at the end of the control values that mirror the first 15 bytes)
		BVec16 control_bytes_before = BVec16::sLoadByte16(mControl + ((inIterator.mIndex - 16) & (mMaxSize - 1)));
		BVec16 control_bytes_after = BVec16::sLoadByte16(mControl + inIterator.mIndex);
		BVec16 bucket_empty = BVec16::sZero();
		uint32 control_empty_before = uint32(BVec16::sEquals(control_bytes_before, bucket_empty).GetTrues());
		uint32 control_empty_after = uint32(BVec16::sEquals(control_bytes_after, bucket_empty).GetTrues());

		// If (this index including) there exist 16 consecutive non-empty slots (represented by a bit being 0) then
		// a probe looking for some element needs to continue probing so we cannot mark the bucket as empty
		// but must mark it as deleted instead.
		// Note that we use: CountLeadingZeros(uint16) = CountLeadingZeros(uint32) - 16.
		uint8 control_value = CountLeadingZeros(control_empty_before) - 16 + CountTrailingZeros(control_empty_after) < 16? cBucketEmpty : cBucketDeleted;

		// Mark the bucket as empty/deleted
		SetControlValue(inIterator.mIndex, control_value);

		// Destruct the element
		mData[inIterator.mIndex].~KeyValue();

		// If we marked the bucket as empty we can increase the load left
		if (control_value == cBucketEmpty)
			++mLoadLeft;

		// Decrease size
		--mSize;
	}

	/// @brief Erase an element by key
	size_type				erase(const Key &inKey)
	{
		const_iterator it = find(inKey);
		if (it == cend())
			return 0;

		erase(it);
		return 1;
	}

	/// Swap the contents of two hash tables
	void					swap(HashTable &ioRHS) noexcept
	{
		std::swap(mData, ioRHS.mData);
		std::swap(mControl, ioRHS.mControl);
		std::swap(mSize, ioRHS.mSize);
		std::swap(mMaxSize, ioRHS.mMaxSize);
		std::swap(mLoadLeft, ioRHS.mLoadLeft);
	}

	/// In place re-hashing of all elements in the table. Removes all cBucketDeleted elements
	/// The std version takes a bucket count, but we just re-hash to the same size.
	void					rehash(size_type)
	{
		// Update the control value for all buckets
		for (size_type i = 0; i < mMaxSize; ++i)
		{
			uint8 &control = mControl[i];
			switch (control)
			{
			case cBucketDeleted:
				// Deleted buckets become empty
				control = cBucketEmpty;
				break;
			case cBucketEmpty:
				// Remains empty
				break;
			default:
				// Mark all occupied as deleted, to indicate it needs to move to the correct place
				control = cBucketDeleted;
				break;
			}
		}

		// Replicate control values to the last 15 entries
		for (size_type i = 0; i < 15; ++i)
			mControl[mMaxSize + i] = mControl[i];

		// Loop over all elements that have been 'deleted' and move them to their new spot
		BVec16 bucket_used = BVec16::sReplicate(cBucketUsed);
		size_type bucket_mask = mMaxSize - 1;
		uint32 probe_mask = bucket_mask & ~uint32(0b1111); // Mask out lower 4 bits because we test 16 buckets at a time
		for (size_type src = 0; src < mMaxSize; ++src)
			if (mControl[src] == cBucketDeleted)
				for (;;)
				{
					// Split hash into index and control value
					size_type src_index;
					uint8 src_control;
					GetIndexAndControlValue(HashTableDetail::sGetKey(mData[src]), src_index, src_control);

					// Linear probing
					size_type dst = src_index;
					for (;;)
					{
						// Check if any buckets are free
						BVec16 control_bytes = BVec16::sLoadByte16(mControl + dst);
						uint32 control_free = uint32(BVec16::sAnd(control_bytes, bucket_used).GetTrues()) ^ 0xffff;
						if (control_free != 0)
						{
							// Select this bucket as destination
							dst += CountTrailingZeros(control_free);
							dst &= bucket_mask;
							break;
						}

						// Move to next batch of 16 buckets
						dst = (dst + 16) & bucket_mask;
					}

					// Check if we stay in the same probe group
					if (((dst - src_index) & probe_mask) == ((src - src_index) & probe_mask))
					{
						// We stay in the same group, we can stay where we are
						SetControlValue(src, src_control);
						break;
					}
					else if (mControl[dst] == cBucketEmpty)
					{
						// There's an empty bucket, move us there
						SetControlValue(dst, src_control);
						SetControlValue(src, cBucketEmpty);
						::new (mData + dst) KeyValue(std::move(mData[src]));
						mData[src].~KeyValue();
						break;
					}
					else
					{
						// There's an element in the bucket we want to move to, swap them
						JPH_ASSERT(mControl[dst] == cBucketDeleted);
						SetControlValue(dst, src_control);
						std::swap(mData[src], mData[dst]);
						// Iterate again with the same source bucket
					}
				}

		// Reinitialize load left
		mLoadLeft = sGetMaxLoad(mMaxSize) - mSize;
	}

private:
	/// If this allocator needs to fall back to aligned allocations because the type requires it
	static constexpr bool	cNeedsAlignedAllocate = alignof(KeyValue) > (JPH_CPU_ADDRESS_BITS == 32? 8 : 16);

	/// Max load factor is cMaxLoadFactorNumerator / cMaxLoadFactorDenominator
	static constexpr uint64	cMaxLoadFactorNumerator = 7;
	static constexpr uint64	cMaxLoadFactorDenominator = 8;

	/// If we can recover this fraction of deleted elements, we'll reshuffle the buckets in place rather than growing the table
	static constexpr uint64 cMaxDeletedElementsNumerator = 1;
	static constexpr uint64 cMaxDeletedElementsDenominator = 8;

	/// Values that the control bytes can have
	static constexpr uint8	cBucketEmpty = 0;
	static constexpr uint8	cBucketDeleted = 0x7f;
	static constexpr uint8	cBucketUsed = 0x80;	// Lowest 7 bits are lowest 7 bits of the hash value

	/// The buckets, an array of size mMaxSize
	KeyValue *				mData = nullptr;

	/// Control bytes, an array of size mMaxSize + 15
	uint8 *					mControl = nullptr;

	/// Number of elements in the table
	size_type				mSize = 0;

	/// Max number of elements that can be stored in the table
	size_type				mMaxSize = 0;

	/// Number of elements we can add to the table before we need to grow
	size_type				mLoadLeft = 0;
};

JPH_NAMESPACE_END

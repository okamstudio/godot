// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Jolt/Core/NonCopyable.h>

JPH_NAMESPACE_BEGIN

/// A mutex array protects a number of resources with a limited amount of mutexes.
/// It uses hashing to find the mutex of a particular object.
/// The idea is that if the amount of threads is much smaller than the amount of mutexes
/// that there is a relatively small chance that two different objects map to the same mutex.
template <class MutexType>
class MutexArray : public NonCopyable
{
public:
	/// Constructor, constructs an empty mutex array that you need to initialize with Init()
							MutexArray() = default;

	/// Constructor, constructs an array with inNumMutexes entries
	explicit				MutexArray(uint inNumMutexes) { Init(inNumMutexes); }

	/// Destructor
							~MutexArray() { delete [] mMutexStorage; }

	/// Initialization
	/// @param inNumMutexes The amount of mutexes to allocate
	void					Init(uint inNumMutexes)
	{
		JPH_ASSERT(mMutexStorage == nullptr);
		JPH_ASSERT(inNumMutexes > 0 && IsPowerOf2(inNumMutexes));

		mMutexStorage = new MutexStorage[inNumMutexes];
		mNumMutexes = inNumMutexes;
	}

	/// Get the number of mutexes that were allocated
	inline uint				GetNumMutexes() const
	{
		return mNumMutexes;
	}

	/// Convert an object index to a mutex index
	inline uint32			GetMutexIndex(uint32 inObjectIndex) const
	{
		Hash<uint32> hasher;
		return hasher(inObjectIndex) & (mNumMutexes - 1);
	}

	/// Get the mutex belonging to a certain object by index
	inline MutexType &		GetMutexByObjectIndex(uint32 inObjectIndex)
	{
		return mMutexStorage[GetMutexIndex(inObjectIndex)].mMutex;
	}

	/// Get a mutex by index in the array
	inline MutexType &		GetMutexByIndex(uint32 inMutexIndex)
	{
		return mMutexStorage[inMutexIndex].mMutex;
	}

	/// Lock all mutexes
	void					LockAll()
	{
		JPH_PROFILE_FUNCTION();

		MutexStorage *end = mMutexStorage + mNumMutexes;
		for (MutexStorage *m = mMutexStorage; m < end; ++m)
			m->mMutex.lock();
	}

	/// Unlock all mutexes
	void					UnlockAll()
	{
		JPH_PROFILE_FUNCTION();

		MutexStorage *end = mMutexStorage + mNumMutexes;
		for (MutexStorage *m = mMutexStorage; m < end; ++m)
			m->mMutex.unlock();
	}

private:
	/// Align the mutex to a cache line to ensure there is no false sharing (this is platform dependent, we do this to be safe)
	struct alignas(JPH_CACHE_LINE_SIZE) MutexStorage
	{
		JPH_OVERRIDE_NEW_DELETE

		MutexType			mMutex;
	};

	MutexStorage *			mMutexStorage = nullptr;
	uint					mNumMutexes = 0;
};

JPH_NAMESPACE_END


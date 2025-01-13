// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Jolt/Core/Mutex.h>

JPH_NAMESPACE_BEGIN

#ifdef JPH_ENABLE_ASSERTS

/// This is the list of locks used by the physics engine, they need to be locked in a particular order (from top of the list to bottom of the list) in order to prevent deadlocks
enum class EPhysicsLockTypes
{
	BroadPhaseQuery			= 1 << 0,
	PerBody					= 1 << 1,
	BodiesList				= 1 << 2,
	BroadPhaseUpdate		= 1 << 3,
	ConstraintsList			= 1 << 4,
	ActiveBodiesList		= 1 << 5,
};

/// A token that indicates the context of a lock (we use 1 per physics system and we use the body manager pointer because it's convenient)
class BodyManager;
using PhysicsLockContext = const BodyManager *;

#endif // !JPH_ENABLE_ASSERTS

/// Helpers to safely lock the different mutexes that are part of the physics system while preventing deadlock
/// Class that keeps track per thread which lock are taken and if the order of locking is correct
class JPH_EXPORT PhysicsLock
{
public:
#ifdef JPH_ENABLE_ASSERTS
	/// Call before taking the lock
	static inline void			sCheckLock(PhysicsLockContext inContext, EPhysicsLockTypes inType)
	{
		uint32 &mutexes = sGetLockedMutexes(inContext);
		JPH_ASSERT(uint32(inType) > mutexes, "A lock of same or higher priority was already taken, this can create a deadlock!");
		mutexes = mutexes | uint32(inType);
	}

	/// Call after releasing the lock
	static inline void			sCheckUnlock(PhysicsLockContext inContext, EPhysicsLockTypes inType)
	{
		uint32 &mutexes = sGetLockedMutexes(inContext);
		JPH_ASSERT((mutexes & uint32(inType)) != 0, "Mutex was not locked!");
		mutexes = mutexes & ~uint32(inType);
	}
#endif // !JPH_ENABLE_ASSERTS

	template <class LockType>
	static inline void			sLock(LockType &inMutex JPH_IF_ENABLE_ASSERTS(, PhysicsLockContext inContext, EPhysicsLockTypes inType))
	{
		JPH_IF_ENABLE_ASSERTS(sCheckLock(inContext, inType);)
		inMutex.lock();
	}

	template <class LockType>
	static inline void			sUnlock(LockType &inMutex JPH_IF_ENABLE_ASSERTS(, PhysicsLockContext inContext, EPhysicsLockTypes inType))
	{
		JPH_IF_ENABLE_ASSERTS(sCheckUnlock(inContext, inType);)
		inMutex.unlock();
	}

	template <class LockType>
	static inline void			sLockShared(LockType &inMutex JPH_IF_ENABLE_ASSERTS(, PhysicsLockContext inContext, EPhysicsLockTypes inType))
	{
		JPH_IF_ENABLE_ASSERTS(sCheckLock(inContext, inType);)
		inMutex.lock_shared();
	}

	template <class LockType>
	static inline void			sUnlockShared(LockType &inMutex JPH_IF_ENABLE_ASSERTS(, PhysicsLockContext inContext, EPhysicsLockTypes inType))
	{
		JPH_IF_ENABLE_ASSERTS(sCheckUnlock(inContext, inType);)
		inMutex.unlock_shared();
	}

#ifdef JPH_ENABLE_ASSERTS
private:
	struct LockData
	{
		uint32					mLockedMutexes = 0;
		PhysicsLockContext		mContext = nullptr;
	};

	// Helper function to find the locked mutexes for a particular context
	static uint32 &				sGetLockedMutexes(PhysicsLockContext inContext)
	{
		static thread_local LockData sLocks[4];

		// If we find a matching context we can use it
		for (LockData &l : sLocks)
			if (l.mContext == inContext)
				return l.mLockedMutexes;

		// Otherwise we look for an entry that is not in use
		for (LockData &l : sLocks)
			if (l.mLockedMutexes == 0)
			{
				l.mContext = inContext;
				return l.mLockedMutexes;
			}

		JPH_ASSERT(false, "Too many physics systems locked at the same time!");
		return sLocks[0].mLockedMutexes;
	}
#endif // !JPH_ENABLE_ASSERTS
};

/// Helper class that is similar to std::unique_lock
template <class LockType>
class UniqueLock : public NonCopyable
{
public:
	explicit					UniqueLock(LockType &inLock JPH_IF_ENABLE_ASSERTS(, PhysicsLockContext inContext, EPhysicsLockTypes inType)) :
		mLock(inLock)
#ifdef JPH_ENABLE_ASSERTS
		, mContext(inContext),
		mType(inType)
#endif // JPH_ENABLE_ASSERTS
	{
		PhysicsLock::sLock(mLock JPH_IF_ENABLE_ASSERTS(, mContext, mType));
	}

								~UniqueLock()
	{
		PhysicsLock::sUnlock(mLock JPH_IF_ENABLE_ASSERTS(, mContext, mType));
	}

private:
	LockType &					mLock;
#ifdef JPH_ENABLE_ASSERTS
	PhysicsLockContext			mContext;
	EPhysicsLockTypes			mType;
#endif // JPH_ENABLE_ASSERTS
};

/// Helper class that is similar to std::shared_lock
template <class LockType>
class SharedLock : public NonCopyable
{
public:
	explicit					SharedLock(LockType &inLock JPH_IF_ENABLE_ASSERTS(, PhysicsLockContext inContext, EPhysicsLockTypes inType)) :
		mLock(inLock)
#ifdef JPH_ENABLE_ASSERTS
		, mContext(inContext)
		, mType(inType)
#endif // JPH_ENABLE_ASSERTS
	{
		PhysicsLock::sLockShared(mLock JPH_IF_ENABLE_ASSERTS(, mContext, mType));
	}

								~SharedLock()
	{
		PhysicsLock::sUnlockShared(mLock JPH_IF_ENABLE_ASSERTS(, mContext, mType));
	}

private:
	LockType &					mLock;
#ifdef JPH_ENABLE_ASSERTS
	PhysicsLockContext			mContext;
	EPhysicsLockTypes			mType;
#endif // JPH_ENABLE_ASSERTS
};

JPH_NAMESPACE_END

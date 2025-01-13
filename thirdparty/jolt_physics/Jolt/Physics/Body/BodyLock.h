// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Jolt/Physics/Body/BodyLockInterface.h>

JPH_NAMESPACE_BEGIN

/// Base class for locking bodies for the duration of the scope of this class (do not use directly)
template <bool Write, class BodyType>
class BodyLockBase : public NonCopyable
{
public:
	/// Constructor will lock the body
								BodyLockBase(const BodyLockInterface &inBodyLockInterface, const BodyID &inBodyID) :
		mBodyLockInterface(inBodyLockInterface)
	{
		if (inBodyID == BodyID())
		{
			// Invalid body id
			mBodyLockMutex = nullptr;
			mBody = nullptr;
		}
		else
		{
			// Get mutex
			mBodyLockMutex = Write? inBodyLockInterface.LockWrite(inBodyID) : inBodyLockInterface.LockRead(inBodyID);

			// Get a reference to the body or nullptr when it is no longer valid
			mBody = inBodyLockInterface.TryGetBody(inBodyID);
		}
	}

	/// Explicitly release the lock (normally this is done in the destructor)
	inline void					ReleaseLock()
	{
		if (mBodyLockMutex != nullptr)
		{
			if (Write)
				mBodyLockInterface.UnlockWrite(mBodyLockMutex);
			else
				mBodyLockInterface.UnlockRead(mBodyLockMutex);

			mBodyLockMutex = nullptr;
			mBody = nullptr;
		}
	}

	/// Destructor will unlock the body
								~BodyLockBase()
	{
		ReleaseLock();
	}

	/// Test if the lock was successful (if the body ID was valid)
	inline bool					Succeeded() const
	{
		return mBody != nullptr;
	}

	/// Test if the lock was successful (if the body ID was valid) and the body is still in the broad phase
	inline bool					SucceededAndIsInBroadPhase() const
	{
		return mBody != nullptr && mBody->IsInBroadPhase();
	}

	/// Access the body
	inline BodyType &			GetBody() const
	{
		JPH_ASSERT(mBody != nullptr, "Should check Succeeded() first");
		return *mBody;
	}

private:
	const BodyLockInterface &	mBodyLockInterface;
	SharedMutex *				mBodyLockMutex;
	BodyType *					mBody;
};

/// A body lock takes a body ID and locks the underlying body so that other threads cannot access its members
///
/// The common usage pattern is:
///
///		BodyLockInterface lock_interface = physics_system.GetBodyLockInterface(); // Or non-locking interface if the lock is already taken
///		BodyID body_id = ...; // Obtain ID to body
///
///		// Scoped lock
///		{
///			BodyLockRead lock(lock_interface, body_id);
///			if (lock.Succeeded()) // body_id may no longer be valid
///			{
///				const Body &body = lock.GetBody();
///
///				// Do something with body
///				...
///			}
///		}
class BodyLockRead : public BodyLockBase<false, const Body>
{
	using BodyLockBase::BodyLockBase;
};

/// Specialization that locks a body for writing to. @see BodyLockRead for usage patterns.
class BodyLockWrite : public BodyLockBase<true, Body>
{
	using BodyLockBase::BodyLockBase;
};

JPH_NAMESPACE_END

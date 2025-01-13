// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Jolt/Physics/Body/BodyLockInterface.h>

JPH_NAMESPACE_BEGIN

/// Base class for locking multiple bodies for the duration of the scope of this class (do not use directly)
template <bool Write, class BodyType>
class BodyLockMultiBase : public NonCopyable
{
public:
	/// Redefine MutexMask
	using MutexMask = BodyLockInterface::MutexMask;

	/// Constructor will lock the bodies
								BodyLockMultiBase(const BodyLockInterface &inBodyLockInterface, const BodyID *inBodyIDs, int inNumber) :
		mBodyLockInterface(inBodyLockInterface),
		mMutexMask(inBodyLockInterface.GetMutexMask(inBodyIDs, inNumber)),
		mBodyIDs(inBodyIDs),
		mNumBodyIDs(inNumber)
	{
		if (mMutexMask != 0)
		{
			// Get mutex
			if (Write)
				inBodyLockInterface.LockWrite(mMutexMask);
			else
				inBodyLockInterface.LockRead(mMutexMask);
		}
	}

	/// Destructor will unlock the bodies
								~BodyLockMultiBase()
	{
		if (mMutexMask != 0)
		{
			if (Write)
				mBodyLockInterface.UnlockWrite(mMutexMask);
			else
				mBodyLockInterface.UnlockRead(mMutexMask);
		}
	}

	/// Access the body (returns null if body was not properly locked)
	inline BodyType *			GetBody(int inBodyIndex) const
	{
		// Range check
		JPH_ASSERT(inBodyIndex >= 0 && inBodyIndex < mNumBodyIDs);

		// Get body ID
		const BodyID &body_id = mBodyIDs[inBodyIndex];
		if (body_id.IsInvalid())
			return nullptr;

		// Get a reference to the body or nullptr when it is no longer valid
		return mBodyLockInterface.TryGetBody(body_id);
	}

private:
	const BodyLockInterface &	mBodyLockInterface;
	MutexMask					mMutexMask;
	const BodyID *				mBodyIDs;
	int							mNumBodyIDs;
};

/// A multi body lock takes a number of body IDs and locks the underlying bodies so that other threads cannot access its members
///
/// The common usage pattern is:
///
///		BodyLockInterface lock_interface = physics_system.GetBodyLockInterface(); // Or non-locking interface if the lock is already taken
///		const BodyID *body_id = ...; // Obtain IDs to bodies
///		int num_body_ids = ...;
///
///		// Scoped lock
///		{
///			BodyLockMultiRead lock(lock_interface, body_ids, num_body_ids);
///			for (int i = 0; i < num_body_ids; ++i)
///			{
///				const Body *body = lock.GetBody(i);
///				if (body != nullptr)
///				{
///					const Body &body = lock.Body();
///
///					// Do something with body
///					...
///				}
///			}
///		}
class BodyLockMultiRead : public BodyLockMultiBase<false, const Body>
{
	using BodyLockMultiBase::BodyLockMultiBase;
};

/// Specialization that locks multiple bodies for writing to. @see BodyLockMultiRead for usage patterns.
class BodyLockMultiWrite : public BodyLockMultiBase<true, Body>
{
	using BodyLockMultiBase::BodyLockMultiBase;
};

JPH_NAMESPACE_END

// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#include <Jolt/Jolt.h>

#include <Jolt/Physics/Body/BodyManager.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Body/BodyActivationListener.h>
#include <Jolt/Physics/SoftBody/SoftBodyMotionProperties.h>
#include <Jolt/Physics/SoftBody/SoftBodyCreationSettings.h>
#include <Jolt/Physics/SoftBody/SoftBodyShape.h>
#include <Jolt/Physics/StateRecorder.h>
#include <Jolt/Core/StringTools.h>
#include <Jolt/Core/QuickSort.h>
#ifdef JPH_DEBUG_RENDERER
	#include <Jolt/Renderer/DebugRenderer.h>
	#include <Jolt/Physics/Body/BodyFilter.h>
#endif // JPH_DEBUG_RENDERER

JPH_NAMESPACE_BEGIN

#ifdef JPH_ENABLE_ASSERTS
	static thread_local bool sOverrideAllowActivation = false;
	static thread_local bool sOverrideAllowDeactivation = false;

	bool BodyManager::sGetOverrideAllowActivation()
	{
		return sOverrideAllowActivation;
	}

	void BodyManager::sSetOverrideAllowActivation(bool inValue)
	{
		sOverrideAllowActivation = inValue;
	}

	bool BodyManager::sGetOverrideAllowDeactivation()
	{
		return sOverrideAllowDeactivation;
	}

	void BodyManager::sSetOverrideAllowDeactivation(bool inValue)
	{
		sOverrideAllowDeactivation = inValue;
	}
#endif

// Helper class that combines a body and its motion properties
class BodyWithMotionProperties : public Body
{
public:
	JPH_OVERRIDE_NEW_DELETE

	MotionProperties			mMotionProperties;
};

// Helper class that combines a soft body its motion properties and shape
class SoftBodyWithMotionPropertiesAndShape : public Body
{
public:
								SoftBodyWithMotionPropertiesAndShape()
	{
		mShape.SetEmbedded();
	}

	SoftBodyMotionProperties	mMotionProperties;
	SoftBodyShape				mShape;
};

inline void BodyManager::sDeleteBody(Body *inBody)
{
	if (inBody->mMotionProperties != nullptr)
	{
		JPH_IF_ENABLE_ASSERTS(inBody->mMotionProperties = nullptr;)
		if (inBody->IsSoftBody())
		{
			inBody->mShape = nullptr; // Release the shape to avoid assertion on shape destruction because of embedded object with refcount > 0
			delete static_cast<SoftBodyWithMotionPropertiesAndShape *>(inBody);
		}
		else
			delete static_cast<BodyWithMotionProperties *>(inBody);
	}
	else
		delete inBody;
}

BodyManager::~BodyManager()
{
	UniqueLock lock(mBodiesMutex JPH_IF_ENABLE_ASSERTS(, this, EPhysicsLockTypes::BodiesList));

	// Destroy any bodies that are still alive
	for (Body *b : mBodies)
		if (sIsValidBodyPointer(b))
			sDeleteBody(b);

	for (BodyID *active_bodies : mActiveBodies)
		delete [] active_bodies;
}

void BodyManager::Init(uint inMaxBodies, uint inNumBodyMutexes, const BroadPhaseLayerInterface &inLayerInterface)
{
	UniqueLock lock(mBodiesMutex JPH_IF_ENABLE_ASSERTS(, this, EPhysicsLockTypes::BodiesList));

	// Num body mutexes must be a power of two and not bigger than our MutexMask
	uint num_body_mutexes = Clamp<uint>(GetNextPowerOf2(inNumBodyMutexes == 0? 2 * thread::hardware_concurrency() : inNumBodyMutexes), 1, sizeof(MutexMask) * 8);
#ifdef JPH_TSAN_ENABLED
	num_body_mutexes = min(num_body_mutexes, 32U); // TSAN errors out when locking too many mutexes on the same thread, see: https://github.com/google/sanitizers/issues/950
#endif

	// Allocate the body mutexes
	mBodyMutexes.Init(num_body_mutexes);

	// Allocate space for bodies
	mBodies.reserve(inMaxBodies);

	// Allocate space for active bodies
	for (BodyID *&active_bodies : mActiveBodies)
	{
		JPH_ASSERT(active_bodies == nullptr);
		active_bodies = new BodyID [inMaxBodies];
	}

	// Allocate space for sequence numbers
	mBodySequenceNumbers.resize(inMaxBodies, 0);

	// Keep layer interface
	mBroadPhaseLayerInterface = &inLayerInterface;
}

uint BodyManager::GetNumBodies() const
{
	UniqueLock lock(mBodiesMutex JPH_IF_ENABLE_ASSERTS(, this, EPhysicsLockTypes::BodiesList));

	return mNumBodies;
}

BodyManager::BodyStats BodyManager::GetBodyStats() const
{
	UniqueLock lock(mBodiesMutex JPH_IF_ENABLE_ASSERTS(, this, EPhysicsLockTypes::BodiesList));

	BodyStats stats;
	stats.mNumBodies = mNumBodies;
	stats.mMaxBodies = uint(mBodies.capacity());

	for (const Body *body : mBodies)
		if (sIsValidBodyPointer(body))
		{
			if (body->IsSoftBody())
			{
				stats.mNumSoftBodies++;
				if (body->IsActive())
					stats.mNumActiveSoftBodies++;
			}
			else
			{
				switch (body->GetMotionType())
				{
				case EMotionType::Static:
					stats.mNumBodiesStatic++;
					break;

				case EMotionType::Dynamic:
					stats.mNumBodiesDynamic++;
					if (body->IsActive())
						stats.mNumActiveBodiesDynamic++;
					break;

				case EMotionType::Kinematic:
					stats.mNumBodiesKinematic++;
					if (body->IsActive())
						stats.mNumActiveBodiesKinematic++;
					break;
				}
			}
		}

	return stats;
}

Body *BodyManager::AllocateBody(const BodyCreationSettings &inBodyCreationSettings) const
{
	// Fill in basic properties
	Body *body;
	if (inBodyCreationSettings.HasMassProperties())
	{
		BodyWithMotionProperties *bmp = new BodyWithMotionProperties;
		body = bmp;
		body->mMotionProperties = &bmp->mMotionProperties;
	}
	else
	{
		body = new Body;
	}
	body->mBodyType = EBodyType::RigidBody;
	body->mShape = inBodyCreationSettings.GetShape();
	body->mUserData = inBodyCreationSettings.mUserData;
	body->SetFriction(inBodyCreationSettings.mFriction);
	body->SetRestitution(inBodyCreationSettings.mRestitution);
	body->mMotionType = inBodyCreationSettings.mMotionType;
	if (inBodyCreationSettings.mIsSensor)
		body->SetIsSensor(true);
	if (inBodyCreationSettings.mCollideKinematicVsNonDynamic)
		body->SetCollideKinematicVsNonDynamic(true);
	if (inBodyCreationSettings.mUseManifoldReduction)
		body->SetUseManifoldReduction(true);
	if (inBodyCreationSettings.mApplyGyroscopicForce)
		body->SetApplyGyroscopicForce(true);
	if (inBodyCreationSettings.mEnhancedInternalEdgeRemoval)
		body->SetEnhancedInternalEdgeRemoval(true);
	SetBodyObjectLayerInternal(*body, inBodyCreationSettings.mObjectLayer);
	body->mObjectLayer = inBodyCreationSettings.mObjectLayer;
	body->mCollisionGroup = inBodyCreationSettings.mCollisionGroup;

	if (inBodyCreationSettings.HasMassProperties())
	{
		MotionProperties *mp = body->mMotionProperties;
		mp->SetLinearDamping(inBodyCreationSettings.mLinearDamping);
		mp->SetAngularDamping(inBodyCreationSettings.mAngularDamping);
		mp->SetMaxLinearVelocity(inBodyCreationSettings.mMaxLinearVelocity);
		mp->SetMaxAngularVelocity(inBodyCreationSettings.mMaxAngularVelocity);
		mp->SetMassProperties(inBodyCreationSettings.mAllowedDOFs, inBodyCreationSettings.GetMassProperties());
		mp->SetLinearVelocity(inBodyCreationSettings.mLinearVelocity); // Needs to happen after setting the max linear/angular velocity and setting allowed DOFs
		mp->SetAngularVelocity(inBodyCreationSettings.mAngularVelocity);
		mp->SetGravityFactor(inBodyCreationSettings.mGravityFactor);
		mp->SetNumVelocityStepsOverride(inBodyCreationSettings.mNumVelocityStepsOverride);
		mp->SetNumPositionStepsOverride(inBodyCreationSettings.mNumPositionStepsOverride);
		mp->mMotionQuality = inBodyCreationSettings.mMotionQuality;
		mp->mAllowSleeping = inBodyCreationSettings.mAllowSleeping;
		JPH_IF_ENABLE_ASSERTS(mp->mCachedBodyType = body->mBodyType;)
		JPH_IF_ENABLE_ASSERTS(mp->mCachedMotionType = body->mMotionType;)
	}

	// Position body
	body->SetPositionAndRotationInternal(inBodyCreationSettings.mPosition, inBodyCreationSettings.mRotation);

	return body;
}

/// Create a soft body using creation settings. The returned body will not be part of the body manager yet.
Body *BodyManager::AllocateSoftBody(const SoftBodyCreationSettings &inSoftBodyCreationSettings) const
{
	// Fill in basic properties
	SoftBodyWithMotionPropertiesAndShape *bmp = new SoftBodyWithMotionPropertiesAndShape;
	SoftBodyMotionProperties *mp = &bmp->mMotionProperties;
	SoftBodyShape *shape = &bmp->mShape;
	Body *body = bmp;
	shape->mSoftBodyMotionProperties = mp;
	body->mBodyType = EBodyType::SoftBody;
	body->mMotionProperties = mp;
	body->mShape = shape;
	body->mUserData = inSoftBodyCreationSettings.mUserData;
	body->SetFriction(inSoftBodyCreationSettings.mFriction);
	body->SetRestitution(inSoftBodyCreationSettings.mRestitution);
	body->mMotionType = EMotionType::Dynamic;
	SetBodyObjectLayerInternal(*body, inSoftBodyCreationSettings.mObjectLayer);
	body->mObjectLayer = inSoftBodyCreationSettings.mObjectLayer;
	body->mCollisionGroup = inSoftBodyCreationSettings.mCollisionGroup;
	mp->SetLinearDamping(inSoftBodyCreationSettings.mLinearDamping);
	mp->SetAngularDamping(0);
	mp->SetMaxLinearVelocity(inSoftBodyCreationSettings.mMaxLinearVelocity);
	mp->SetMaxAngularVelocity(FLT_MAX);
	mp->SetLinearVelocity(Vec3::sZero());
	mp->SetAngularVelocity(Vec3::sZero());
	mp->SetGravityFactor(inSoftBodyCreationSettings.mGravityFactor);
	mp->mMotionQuality = EMotionQuality::Discrete;
	mp->mAllowSleeping = inSoftBodyCreationSettings.mAllowSleeping;
	JPH_IF_ENABLE_ASSERTS(mp->mCachedBodyType = body->mBodyType;)
	JPH_IF_ENABLE_ASSERTS(mp->mCachedMotionType = body->mMotionType;)
	mp->Initialize(inSoftBodyCreationSettings);

	body->SetPositionAndRotationInternal(inSoftBodyCreationSettings.mPosition, inSoftBodyCreationSettings.mMakeRotationIdentity? Quat::sIdentity() : inSoftBodyCreationSettings.mRotation);

	return body;
}

void BodyManager::FreeBody(Body *inBody) const
{
	JPH_ASSERT(inBody->GetID().IsInvalid(), "This function should only be called on a body that doesn't have an ID yet, use DestroyBody otherwise");

	sDeleteBody(inBody);
}

bool BodyManager::AddBody(Body *ioBody)
{
	// Return error when body was already added
	if (!ioBody->GetID().IsInvalid())
		return false;

	// Determine next free index
	uint32 idx;
	{
		UniqueLock lock(mBodiesMutex JPH_IF_ENABLE_ASSERTS(, this, EPhysicsLockTypes::BodiesList));

		if (mBodyIDFreeListStart != cBodyIDFreeListEnd)
		{
			// Pop an item from the freelist
			JPH_ASSERT(mBodyIDFreeListStart & cIsFreedBody);
			idx = uint32(mBodyIDFreeListStart >> cFreedBodyIndexShift);
			JPH_ASSERT(!sIsValidBodyPointer(mBodies[idx]));
			mBodyIDFreeListStart = uintptr_t(mBodies[idx]);
			mBodies[idx] = ioBody;
		}
		else
		{
			if (mBodies.size() < mBodies.capacity())
			{
				// Allocate a new entry, note that the array should not actually resize since we've reserved it at init time
				idx = uint32(mBodies.size());
				mBodies.push_back(ioBody);
			}
			else
			{
				// Out of bodies
				return false;
			}
		}

		// Update cached number of bodies
		mNumBodies++;
	}

	// Get next sequence number and assign the ID
	uint8 seq_no = GetNextSequenceNumber(idx);
	ioBody->mID = BodyID(idx, seq_no);
	return true;
}

bool BodyManager::AddBodyWithCustomID(Body *ioBody, const BodyID &inBodyID)
{
	// Return error when body was already added
	if (!ioBody->GetID().IsInvalid())
		return false;

	{
		UniqueLock lock(mBodiesMutex JPH_IF_ENABLE_ASSERTS(, this, EPhysicsLockTypes::BodiesList));

		// Check if index is beyond the max body ID
		uint32 idx = inBodyID.GetIndex();
		if (idx >= mBodies.capacity())
			return false; // Return error

		if (idx < mBodies.size())
		{
			// Body array entry has already been allocated, check if there's a free body here
			if (sIsValidBodyPointer(mBodies[idx]))
				return false; // Return error

			// Remove the entry from the freelist
			uintptr_t idx_start = mBodyIDFreeListStart >> cFreedBodyIndexShift;
			if (idx == idx_start)
			{
				// First entry, easy to remove, the start of the list is our next
				mBodyIDFreeListStart = uintptr_t(mBodies[idx]);
			}
			else
			{
				// Loop over the freelist and find the entry in the freelist pointing to our index
				// TODO: This is O(N), see if this becomes a performance problem (don't want to put the freed bodies in a double linked list)
				uintptr_t cur, next;
				for (cur = idx_start; cur != cBodyIDFreeListEnd >> cFreedBodyIndexShift; cur = next)
				{
					next = uintptr_t(mBodies[cur]) >> cFreedBodyIndexShift;
					if (next == idx)
					{
						mBodies[cur] = mBodies[idx];
						break;
					}
				}
				JPH_ASSERT(cur != cBodyIDFreeListEnd >> cFreedBodyIndexShift);
			}

			// Put the body in the slot
			mBodies[idx] = ioBody;
		}
		else
		{
			// Ensure that all body IDs up to this body ID have been allocated and added to the free list
			while (idx > mBodies.size())
			{
				// Push the id onto the freelist
				mBodies.push_back((Body *)mBodyIDFreeListStart);
				mBodyIDFreeListStart = (uintptr_t(mBodies.size() - 1) << cFreedBodyIndexShift) | cIsFreedBody;
			}

			// Add the element to the list
			mBodies.push_back(ioBody);
		}

		// Update cached number of bodies
		mNumBodies++;
	}

	// Assign the ID
	ioBody->mID = inBodyID;
	return true;
}

Body *BodyManager::RemoveBodyInternal(const BodyID &inBodyID)
{
	// Get body
	uint32 idx = inBodyID.GetIndex();
	Body *body = mBodies[idx];

	// Validate that it can be removed
	JPH_ASSERT(body->GetID() == inBodyID);
	JPH_ASSERT(!body->IsActive());
	JPH_ASSERT(!body->IsInBroadPhase(), "Use BodyInterface::RemoveBody to remove this body first!");

	// Push the id onto the freelist
	mBodies[idx] = (Body *)mBodyIDFreeListStart;
	mBodyIDFreeListStart = (uintptr_t(idx) << cFreedBodyIndexShift) | cIsFreedBody;

	return body;
}

#if defined(JPH_DEBUG) && defined(JPH_ENABLE_ASSERTS)

void BodyManager::ValidateFreeList() const
{
	// Check that the freelist is correct
	size_t num_freed = 0;
	for (uintptr_t start = mBodyIDFreeListStart; start != cBodyIDFreeListEnd; start = uintptr_t(mBodies[start >> cFreedBodyIndexShift]))
	{
		JPH_ASSERT(start & cIsFreedBody);
		num_freed++;
	}
	JPH_ASSERT(mNumBodies == mBodies.size() - num_freed);
}

#endif // defined(JPH_DEBUG) && _defined(JPH_ENABLE_ASSERTS)

void BodyManager::RemoveBodies(const BodyID *inBodyIDs, int inNumber, Body **outBodies)
{
	// Don't take lock if no bodies are to be destroyed
	if (inNumber <= 0)
		return;

	UniqueLock lock(mBodiesMutex JPH_IF_ENABLE_ASSERTS(, this, EPhysicsLockTypes::BodiesList));

	// Update cached number of bodies
	JPH_ASSERT(mNumBodies >= (uint)inNumber);
	mNumBodies -= inNumber;

	for (const BodyID *b = inBodyIDs, *b_end = inBodyIDs + inNumber; b < b_end; b++)
	{
		// Remove body
		Body *body = RemoveBodyInternal(*b);

		// Clear the ID
		body->mID = BodyID();

		// Return the body to the caller
		if (outBodies != nullptr)
		{
			*outBodies = body;
			++outBodies;
		}
	}

#if defined(JPH_DEBUG) && defined(JPH_ENABLE_ASSERTS)
	ValidateFreeList();
#endif // defined(JPH_DEBUG) && _defined(JPH_ENABLE_ASSERTS)
}

void BodyManager::DestroyBodies(const BodyID *inBodyIDs, int inNumber)
{
	// Don't take lock if no bodies are to be destroyed
	if (inNumber <= 0)
		return;

	UniqueLock lock(mBodiesMutex JPH_IF_ENABLE_ASSERTS(, this, EPhysicsLockTypes::BodiesList));

	// Update cached number of bodies
	JPH_ASSERT(mNumBodies >= (uint)inNumber);
	mNumBodies -= inNumber;

	for (const BodyID *b = inBodyIDs, *b_end = inBodyIDs + inNumber; b < b_end; b++)
	{
		// Remove body
		Body *body = RemoveBodyInternal(*b);

		// Free the body
		sDeleteBody(body);
	}

#if defined(JPH_DEBUG) && defined(JPH_ENABLE_ASSERTS)
	ValidateFreeList();
#endif // defined(JPH_DEBUG) && _defined(JPH_ENABLE_ASSERTS)
}

void BodyManager::AddBodyToActiveBodies(Body &ioBody)
{
	// Select the correct array to use
	int type = (int)ioBody.GetBodyType();
	atomic<uint32> &num_active_bodies = mNumActiveBodies[type];
	BodyID *active_bodies = mActiveBodies[type];

	MotionProperties *mp = ioBody.mMotionProperties;
	uint32 num_active_bodies_val = num_active_bodies.load(memory_order_relaxed);
	mp->mIndexInActiveBodies = num_active_bodies_val;
	JPH_ASSERT(num_active_bodies_val < GetMaxBodies());
	active_bodies[num_active_bodies_val] = ioBody.GetID();
	num_active_bodies.fetch_add(1, memory_order_release); // Increment atomic after setting the body ID so that PhysicsSystem::JobFindCollisions (which doesn't lock the mActiveBodiesMutex) will only read valid IDs

	// Count CCD bodies
	if (mp->GetMotionQuality() == EMotionQuality::LinearCast)
		mNumActiveCCDBodies++;
}

void BodyManager::RemoveBodyFromActiveBodies(Body &ioBody)
{
	// Select the correct array to use
	int type = (int)ioBody.GetBodyType();
	atomic<uint32> &num_active_bodies = mNumActiveBodies[type];
	BodyID *active_bodies = mActiveBodies[type];

	uint32 last_body_index = num_active_bodies.load(memory_order_relaxed) - 1;
	MotionProperties *mp = ioBody.mMotionProperties;
	if (mp->mIndexInActiveBodies != last_body_index)
	{
		// This is not the last body, use the last body to fill the hole
		BodyID last_body_id = active_bodies[last_body_index];
		active_bodies[mp->mIndexInActiveBodies] = last_body_id;

		// Update that body's index in the active list
		Body &last_body = *mBodies[last_body_id.GetIndex()];
		JPH_ASSERT(last_body.mMotionProperties->mIndexInActiveBodies == last_body_index);
		last_body.mMotionProperties->mIndexInActiveBodies = mp->mIndexInActiveBodies;
	}

	// Mark this body as no longer active
	mp->mIndexInActiveBodies = Body::cInactiveIndex;

	// Remove unused element from active bodies list
	num_active_bodies.fetch_sub(1, memory_order_release);

	// Count CCD bodies
	if (mp->GetMotionQuality() == EMotionQuality::LinearCast)
		mNumActiveCCDBodies--;
}

void BodyManager::ActivateBodies(const BodyID *inBodyIDs, int inNumber)
{
	// Don't take lock if no bodies are to be activated
	if (inNumber <= 0)
		return;

	UniqueLock lock(mActiveBodiesMutex JPH_IF_ENABLE_ASSERTS(, this, EPhysicsLockTypes::ActiveBodiesList));

	JPH_ASSERT(!mActiveBodiesLocked || sOverrideAllowActivation);

	for (const BodyID *b = inBodyIDs, *b_end = inBodyIDs + inNumber; b < b_end; b++)
		if (!b->IsInvalid())
		{
			BodyID body_id = *b;
			Body &body = *mBodies[body_id.GetIndex()];

			JPH_ASSERT(body.GetID() == body_id);
			JPH_ASSERT(body.IsInBroadPhase(), "Use BodyInterface::AddBody to add the body first!");

			if (!body.IsStatic())
			{
				// Reset sleeping timer so that we don't immediately go to sleep again
				body.ResetSleepTimer();

				// Check if we're sleeping
				if (body.mMotionProperties->mIndexInActiveBodies == Body::cInactiveIndex)
				{
					AddBodyToActiveBodies(body);

					// Call activation listener
					if (mActivationListener != nullptr)
						mActivationListener->OnBodyActivated(body_id, body.GetUserData());
				}
			}
		}
}

void BodyManager::DeactivateBodies(const BodyID *inBodyIDs, int inNumber)
{
	// Don't take lock if no bodies are to be deactivated
	if (inNumber <= 0)
		return;

	UniqueLock lock(mActiveBodiesMutex JPH_IF_ENABLE_ASSERTS(, this, EPhysicsLockTypes::ActiveBodiesList));

	JPH_ASSERT(!mActiveBodiesLocked || sOverrideAllowDeactivation);

	for (const BodyID *b = inBodyIDs, *b_end = inBodyIDs + inNumber; b < b_end; b++)
		if (!b->IsInvalid())
		{
			BodyID body_id = *b;
			Body &body = *mBodies[body_id.GetIndex()];

			JPH_ASSERT(body.GetID() == body_id);
			JPH_ASSERT(body.IsInBroadPhase(), "Use BodyInterface::AddBody to add the body first!");

			if (body.mMotionProperties != nullptr
				&& body.mMotionProperties->mIndexInActiveBodies != Body::cInactiveIndex)
			{
				// Remove the body from the active bodies list
				RemoveBodyFromActiveBodies(body);

				// Mark this body as no longer active
				body.mMotionProperties->mIslandIndex = Body::cInactiveIndex;

				// Reset velocity
				body.mMotionProperties->mLinearVelocity = Vec3::sZero();
				body.mMotionProperties->mAngularVelocity = Vec3::sZero();

				// Call activation listener
				if (mActivationListener != nullptr)
					mActivationListener->OnBodyDeactivated(body_id, body.GetUserData());
			}
		}
}

void BodyManager::SetMotionQuality(Body &ioBody, EMotionQuality inMotionQuality)
{
	MotionProperties *mp = ioBody.GetMotionPropertiesUnchecked();
	if (mp != nullptr && mp->GetMotionQuality() != inMotionQuality)
	{
		UniqueLock lock(mActiveBodiesMutex JPH_IF_ENABLE_ASSERTS(, this, EPhysicsLockTypes::ActiveBodiesList));

		JPH_ASSERT(!mActiveBodiesLocked);

		bool is_active = ioBody.IsActive();
		if (is_active && mp->GetMotionQuality() == EMotionQuality::LinearCast)
			--mNumActiveCCDBodies;

		mp->mMotionQuality = inMotionQuality;

		if (is_active && mp->GetMotionQuality() == EMotionQuality::LinearCast)
			++mNumActiveCCDBodies;
	}
}

void BodyManager::GetActiveBodies(EBodyType inType, BodyIDVector &outBodyIDs) const
{
	JPH_PROFILE_FUNCTION();

	UniqueLock lock(mActiveBodiesMutex JPH_IF_ENABLE_ASSERTS(, this, EPhysicsLockTypes::ActiveBodiesList));

	const BodyID *active_bodies = mActiveBodies[(int)inType];
	outBodyIDs.assign(active_bodies, active_bodies + mNumActiveBodies[(int)inType].load(memory_order_relaxed));
}

void BodyManager::GetBodyIDs(BodyIDVector &outBodies) const
{
	JPH_PROFILE_FUNCTION();

	UniqueLock lock(mBodiesMutex JPH_IF_ENABLE_ASSERTS(, this, EPhysicsLockTypes::BodiesList));

	// Reserve space for all bodies
	outBodies.clear();
	outBodies.reserve(mNumBodies);

	// Iterate the list and find the bodies that are not null
	for (const Body *b : mBodies)
		if (sIsValidBodyPointer(b))
			outBodies.push_back(b->GetID());

	// Validate that our reservation was correct
	JPH_ASSERT(outBodies.size() == mNumBodies);
}

void BodyManager::SetBodyActivationListener(BodyActivationListener *inListener)
{
	UniqueLock lock(mActiveBodiesMutex JPH_IF_ENABLE_ASSERTS(, this, EPhysicsLockTypes::ActiveBodiesList));

	mActivationListener = inListener;
}

BodyManager::MutexMask BodyManager::GetMutexMask(const BodyID *inBodies, int inNumber) const
{
	JPH_ASSERT(sizeof(MutexMask) * 8 >= mBodyMutexes.GetNumMutexes(), "MutexMask must have enough bits");

	if (inNumber >= (int)mBodyMutexes.GetNumMutexes())
	{
		// Just lock everything if there are too many bodies
		return GetAllBodiesMutexMask();
	}
	else
	{
		MutexMask mask = 0;
		for (const BodyID *b = inBodies, *b_end = inBodies + inNumber; b < b_end; ++b)
			if (!b->IsInvalid())
			{
				uint32 index = mBodyMutexes.GetMutexIndex(b->GetIndex());
				mask |= (MutexMask(1) << index);
			}
		return mask;
	}
}

void BodyManager::LockRead(MutexMask inMutexMask) const
{
	JPH_IF_ENABLE_ASSERTS(PhysicsLock::sCheckLock(this, EPhysicsLockTypes::PerBody));

	int index = 0;
	for (MutexMask mask = inMutexMask; mask != 0; mask >>= 1, index++)
		if (mask & 1)
			mBodyMutexes.GetMutexByIndex(index).lock_shared();
}

void BodyManager::UnlockRead(MutexMask inMutexMask) const
{
	JPH_IF_ENABLE_ASSERTS(PhysicsLock::sCheckUnlock(this, EPhysicsLockTypes::PerBody));

	int index = 0;
	for (MutexMask mask = inMutexMask; mask != 0; mask >>= 1, index++)
		if (mask & 1)
			mBodyMutexes.GetMutexByIndex(index).unlock_shared();
}

void BodyManager::LockWrite(MutexMask inMutexMask) const
{
	JPH_IF_ENABLE_ASSERTS(PhysicsLock::sCheckLock(this, EPhysicsLockTypes::PerBody));

	int index = 0;
	for (MutexMask mask = inMutexMask; mask != 0; mask >>= 1, index++)
		if (mask & 1)
			mBodyMutexes.GetMutexByIndex(index).lock();
}

void BodyManager::UnlockWrite(MutexMask inMutexMask) const
{
	JPH_IF_ENABLE_ASSERTS(PhysicsLock::sCheckUnlock(this, EPhysicsLockTypes::PerBody));

	int index = 0;
	for (MutexMask mask = inMutexMask; mask != 0; mask >>= 1, index++)
		if (mask & 1)
			mBodyMutexes.GetMutexByIndex(index).unlock();
}

void BodyManager::LockAllBodies() const
{
	JPH_IF_ENABLE_ASSERTS(PhysicsLock::sCheckLock(this, EPhysicsLockTypes::PerBody));
	mBodyMutexes.LockAll();

	PhysicsLock::sLock(mBodiesMutex JPH_IF_ENABLE_ASSERTS(, this, EPhysicsLockTypes::BodiesList));
}

void BodyManager::UnlockAllBodies() const
{
	PhysicsLock::sUnlock(mBodiesMutex JPH_IF_ENABLE_ASSERTS(, this, EPhysicsLockTypes::BodiesList));

	JPH_IF_ENABLE_ASSERTS(PhysicsLock::sCheckUnlock(this, EPhysicsLockTypes::PerBody));
	mBodyMutexes.UnlockAll();
}

void BodyManager::SaveState(StateRecorder &inStream, const StateRecorderFilter *inFilter) const
{
	{
		LockAllBodies();

		// Determine which bodies to save
		Array<const Body *> bodies;
		bodies.reserve(mNumBodies);
		for (const Body *b : mBodies)
			if (sIsValidBodyPointer(b) && b->IsInBroadPhase() && (inFilter == nullptr || inFilter->ShouldSaveBody(*b)))
				bodies.push_back(b);

		// Write state of bodies
		uint32 num_bodies = (uint32)bodies.size();
		inStream.Write(num_bodies);
		for (const Body *b : bodies)
		{
			inStream.Write(b->GetID());
			inStream.Write(b->IsActive());
			b->SaveState(inStream);
		}

		UnlockAllBodies();
	}
}

bool BodyManager::RestoreState(StateRecorder &inStream)
{
	BodyIDVector bodies_to_activate, bodies_to_deactivate;

	{
		LockAllBodies();

		if (inStream.IsValidating())
		{
			// Read state of bodies, note this reads it in a way to be consistent with validation
			uint32 old_num_bodies = 0;
			for (const Body *b : mBodies)
				if (sIsValidBodyPointer(b) && b->IsInBroadPhase())
					++old_num_bodies;
			uint32 num_bodies = old_num_bodies; // Initialize to current value for validation
			inStream.Read(num_bodies);
			if (num_bodies != old_num_bodies)
			{
				JPH_ASSERT(false, "Cannot handle adding/removing bodies");
				UnlockAllBodies();
				return false;
			}

			for (Body *b : mBodies)
				if (sIsValidBodyPointer(b) && b->IsInBroadPhase())
				{
					BodyID body_id = b->GetID(); // Initialize to current value for validation
					inStream.Read(body_id);
					if (body_id != b->GetID())
					{
						JPH_ASSERT(false, "Cannot handle adding/removing bodies");
						UnlockAllBodies();
						return false;
					}
					bool is_active = b->IsActive(); // Initialize to current value for validation
					inStream.Read(is_active);
					if (is_active != b->IsActive())
					{
						if (is_active)
							bodies_to_activate.push_back(body_id);
						else
							bodies_to_deactivate.push_back(body_id);
					}
					b->RestoreState(inStream);
				}
		}
		else
		{
			// Not validating, we can be a bit more loose, read number of bodies
			uint32 num_bodies = 0;
			inStream.Read(num_bodies);

			// Iterate over the stored bodies and restore their state
			for (uint32 idx = 0; idx < num_bodies; ++idx)
			{
				BodyID body_id;
				inStream.Read(body_id);
				Body *b = TryGetBody(body_id);
				if (b == nullptr)
				{
					JPH_ASSERT(false, "Restoring state for non-existing body");
					UnlockAllBodies();
					return false;
				}
				bool is_active;
				inStream.Read(is_active);
				if (is_active != b->IsActive())
				{
					if (is_active)
						bodies_to_activate.push_back(body_id);
					else
						bodies_to_deactivate.push_back(body_id);
				}
				b->RestoreState(inStream);
			}
		}

		UnlockAllBodies();
	}

	{
		UniqueLock lock(mActiveBodiesMutex JPH_IF_ENABLE_ASSERTS(, this, EPhysicsLockTypes::ActiveBodiesList));

		for (BodyID body_id : bodies_to_activate)
		{
			Body *body = TryGetBody(body_id);
			AddBodyToActiveBodies(*body);
		}

		for (BodyID body_id : bodies_to_deactivate)
		{
			Body *body = TryGetBody(body_id);
			RemoveBodyFromActiveBodies(*body);
		}
	}

	return true;
}

void BodyManager::SaveBodyState(const Body &inBody, StateRecorder &inStream) const
{
	inStream.Write(inBody.IsActive());

	inBody.SaveState(inStream);
}

void BodyManager::RestoreBodyState(Body &ioBody, StateRecorder &inStream)
{
	bool is_active = ioBody.IsActive();
	inStream.Read(is_active);

	ioBody.RestoreState(inStream);

	if (is_active != ioBody.IsActive())
	{
		UniqueLock lock(mActiveBodiesMutex JPH_IF_ENABLE_ASSERTS(, this, EPhysicsLockTypes::ActiveBodiesList));

		JPH_ASSERT(!mActiveBodiesLocked || sOverrideAllowActivation);

		if (is_active)
			AddBodyToActiveBodies(ioBody);
		else
			RemoveBodyFromActiveBodies(ioBody);
	}
}

#ifdef JPH_DEBUG_RENDERER
void BodyManager::Draw(const DrawSettings &inDrawSettings, const PhysicsSettings &inPhysicsSettings, DebugRenderer *inRenderer, const BodyDrawFilter *inBodyFilter)
{
	JPH_PROFILE_FUNCTION();

	LockAllBodies();

	for (const Body *body : mBodies)
		if (sIsValidBodyPointer(body) && body->IsInBroadPhase() && (!inBodyFilter || inBodyFilter->ShouldDraw(*body)))
		{
			JPH_ASSERT(mBodies[body->GetID().GetIndex()] == body);

			bool is_sensor = body->IsSensor();

			// Determine drawing mode
			Color color;
			if (is_sensor)
				color = Color::sYellow;
			else
				switch (inDrawSettings.mDrawShapeColor)
				{
				case EShapeColor::InstanceColor:
					// Each instance has own color
					color = Color::sGetDistinctColor(body->mID.GetIndex());
					break;

				case EShapeColor::ShapeTypeColor:
					color = ShapeFunctions::sGet(body->GetShape()->GetSubType()).mColor;
					break;

				case EShapeColor::MotionTypeColor:
					// Determine color based on motion type
					switch (body->mMotionType)
					{
					case EMotionType::Static:
						color = Color::sGrey;
						break;

					case EMotionType::Kinematic:
						color = Color::sGreen;
						break;

					case EMotionType::Dynamic:
						color = Color::sGetDistinctColor(body->mID.GetIndex());
						break;

					default:
						JPH_ASSERT(false);
						color = Color::sBlack;
						break;
					}
					break;

				case EShapeColor::SleepColor:
					// Determine color based on motion type
					switch (body->mMotionType)
					{
					case EMotionType::Static:
						color = Color::sGrey;
						break;

					case EMotionType::Kinematic:
						color = body->IsActive()? Color::sGreen : Color::sRed;
						break;

					case EMotionType::Dynamic:
						color = body->IsActive()? Color::sYellow : Color::sRed;
						break;

					default:
						JPH_ASSERT(false);
						color = Color::sBlack;
						break;
					}
					break;

				case EShapeColor::IslandColor:
					// Determine color based on motion type
					switch (body->mMotionType)
					{
					case EMotionType::Static:
						color = Color::sGrey;
						break;

					case EMotionType::Kinematic:
					case EMotionType::Dynamic:
						{
							uint32 idx = body->GetMotionProperties()->GetIslandIndexInternal();
							color = idx != Body::cInactiveIndex? Color::sGetDistinctColor(idx) : Color::sLightGrey;
						}
						break;

					default:
						JPH_ASSERT(false);
						color = Color::sBlack;
						break;
					}
					break;

				case EShapeColor::MaterialColor:
					color = Color::sWhite;
					break;

				default:
					JPH_ASSERT(false);
					color = Color::sBlack;
					break;
				}

			// Draw the results of GetSupportFunction
			if (inDrawSettings.mDrawGetSupportFunction)
				body->mShape->DrawGetSupportFunction(inRenderer, body->GetCenterOfMassTransform(), Vec3::sReplicate(1.0f), color, inDrawSettings.mDrawSupportDirection);

			// Draw the results of GetSupportingFace
			if (inDrawSettings.mDrawGetSupportingFace)
				body->mShape->DrawGetSupportingFace(inRenderer, body->GetCenterOfMassTransform(), Vec3::sReplicate(1.0f));

			// Draw the shape
			if (inDrawSettings.mDrawShape)
				body->mShape->Draw(inRenderer, body->GetCenterOfMassTransform(), Vec3::sReplicate(1.0f), color, inDrawSettings.mDrawShapeColor == EShapeColor::MaterialColor, inDrawSettings.mDrawShapeWireframe || is_sensor);

			// Draw bounding box
			if (inDrawSettings.mDrawBoundingBox)
				inRenderer->DrawWireBox(body->mBounds, color);

			// Draw center of mass transform
			if (inDrawSettings.mDrawCenterOfMassTransform)
				inRenderer->DrawCoordinateSystem(body->GetCenterOfMassTransform(), 0.2f);

			// Draw world transform
			if (inDrawSettings.mDrawWorldTransform)
				inRenderer->DrawCoordinateSystem(body->GetWorldTransform(), 0.2f);

			// Draw world space linear and angular velocity
			if (inDrawSettings.mDrawVelocity)
			{
				RVec3 pos = body->GetCenterOfMassPosition();
				inRenderer->DrawArrow(pos, pos + body->GetLinearVelocity(), Color::sGreen, 0.1f);
				inRenderer->DrawArrow(pos, pos + body->GetAngularVelocity(), Color::sRed, 0.1f);
			}

			if (inDrawSettings.mDrawMassAndInertia && body->IsDynamic())
			{
				const MotionProperties *mp = body->GetMotionProperties();
				if (mp->GetInverseMass() > 0.0f
					&& !Vec3::sEquals(mp->GetInverseInertiaDiagonal(), Vec3::sZero()).TestAnyXYZTrue())
				{
					// Invert mass again
					float mass = 1.0f / mp->GetInverseMass();

					// Invert diagonal again
					Vec3 diagonal = mp->GetInverseInertiaDiagonal().Reciprocal();

					// Determine how big of a box has the equivalent inertia
					Vec3 box_size = MassProperties::sGetEquivalentSolidBoxSize(mass, diagonal);

					// Draw box with equivalent inertia
					inRenderer->DrawWireBox(body->GetCenterOfMassTransform() * Mat44::sRotation(mp->GetInertiaRotation()), AABox(-0.5f * box_size, 0.5f * box_size), Color::sOrange);

					// Draw mass
					inRenderer->DrawText3D(body->GetCenterOfMassPosition(), StringFormat("%.2f", (double)mass), Color::sOrange, 0.2f);
				}
			}

			if (inDrawSettings.mDrawSleepStats && body->IsDynamic() && body->IsActive())
			{
				// Draw stats to know which bodies could go to sleep
				String text = StringFormat("t: %.1f", (double)body->mMotionProperties->mSleepTestTimer);
				uint8 g = uint8(Clamp(255.0f * body->mMotionProperties->mSleepTestTimer / inPhysicsSettings.mTimeBeforeSleep, 0.0f, 255.0f));
				Color sleep_color = Color(0, 255 - g, g);
				inRenderer->DrawText3D(body->GetCenterOfMassPosition(), text, sleep_color, 0.2f);
				for (int i = 0; i < 3; ++i)
					inRenderer->DrawWireSphere(JPH_IF_DOUBLE_PRECISION(body->mMotionProperties->GetSleepTestOffset() +) body->mMotionProperties->mSleepTestSpheres[i].GetCenter(), body->mMotionProperties->mSleepTestSpheres[i].GetRadius(), sleep_color);
			}

			if (body->IsSoftBody())
			{
				const SoftBodyMotionProperties *mp = static_cast<const SoftBodyMotionProperties *>(body->GetMotionProperties());
				RMat44 com = body->GetCenterOfMassTransform();

				if (inDrawSettings.mDrawSoftBodyVertices)
					mp->DrawVertices(inRenderer, com);

				if (inDrawSettings.mDrawSoftBodyVertexVelocities)
					mp->DrawVertexVelocities(inRenderer, com);

				if (inDrawSettings.mDrawSoftBodyEdgeConstraints)
					mp->DrawEdgeConstraints(inRenderer, com, inDrawSettings.mDrawSoftBodyConstraintColor);

				if (inDrawSettings.mDrawSoftBodyBendConstraints)
					mp->DrawBendConstraints(inRenderer, com, inDrawSettings.mDrawSoftBodyConstraintColor);

				if (inDrawSettings.mDrawSoftBodyVolumeConstraints)
					mp->DrawVolumeConstraints(inRenderer, com, inDrawSettings.mDrawSoftBodyConstraintColor);

				if (inDrawSettings.mDrawSoftBodySkinConstraints)
					mp->DrawSkinConstraints(inRenderer, com, inDrawSettings.mDrawSoftBodyConstraintColor);

				if (inDrawSettings.mDrawSoftBodyLRAConstraints)
					mp->DrawLRAConstraints(inRenderer, com, inDrawSettings.mDrawSoftBodyConstraintColor);

				if (inDrawSettings.mDrawSoftBodyPredictedBounds)
					mp->DrawPredictedBounds(inRenderer, com);
			}
		}

	UnlockAllBodies();
}
#endif // JPH_DEBUG_RENDERER

void BodyManager::InvalidateContactCacheForBody(Body &ioBody)
{
	// If this is the first time we flip the collision cache invalid flag, we need to add it to an internal list to ensure we reset the flag at the end of the physics update
	if (ioBody.InvalidateContactCacheInternal())
	{
		lock_guard lock(mBodiesCacheInvalidMutex);
		mBodiesCacheInvalid.push_back(ioBody.GetID());
	}
}

void BodyManager::ValidateContactCacheForAllBodies()
{
	lock_guard lock(mBodiesCacheInvalidMutex);

	for (const BodyID &b : mBodiesCacheInvalid)
	{
		// The body may have been removed between the call to InvalidateContactCacheForBody and this call, so check if it still exists
		Body *body = TryGetBody(b);
		if (body != nullptr)
			body->ValidateContactCacheInternal();
	}
	mBodiesCacheInvalid.clear();
}

#ifdef JPH_DEBUG
void BodyManager::ValidateActiveBodyBounds()
{
	UniqueLock lock(mActiveBodiesMutex JPH_IF_ENABLE_ASSERTS(, this, EPhysicsLockTypes::ActiveBodiesList));

	for (uint type = 0; type < cBodyTypeCount; ++type)
		for (BodyID *id = mActiveBodies[type], *id_end = mActiveBodies[type] + mNumActiveBodies[type].load(memory_order_relaxed); id < id_end; ++id)
		{
			const Body *body = mBodies[id->GetIndex()];
			AABox cached = body->GetWorldSpaceBounds();
			AABox calculated = body->GetShape()->GetWorldSpaceBounds(body->GetCenterOfMassTransform(), Vec3::sReplicate(1.0f));
			JPH_ASSERT(cached == calculated);
		}
}
#endif // JPH_DEBUG

JPH_NAMESPACE_END

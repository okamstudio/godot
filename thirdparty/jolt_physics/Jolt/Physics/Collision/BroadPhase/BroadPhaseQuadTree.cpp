// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseQuadTree.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/AABoxCast.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Core/QuickSort.h>

JPH_NAMESPACE_BEGIN

BroadPhaseQuadTree::~BroadPhaseQuadTree()
{
	delete [] mLayers;
}

void BroadPhaseQuadTree::Init(BodyManager *inBodyManager, const BroadPhaseLayerInterface &inLayerInterface)
{
	BroadPhase::Init(inBodyManager, inLayerInterface);

	// Store input parameters
	mBroadPhaseLayerInterface = &inLayerInterface;
	mNumLayers = inLayerInterface.GetNumBroadPhaseLayers();
	JPH_ASSERT(mNumLayers < (BroadPhaseLayer::Type)cBroadPhaseLayerInvalid);

#ifdef JPH_ENABLE_ASSERTS
	// Store lock context
	mLockContext = inBodyManager;
#endif // JPH_ENABLE_ASSERTS

	// Store max bodies
	mMaxBodies = inBodyManager->GetMaxBodies();

	// Initialize tracking data
	mTracking.resize(mMaxBodies);

	// Init allocator
	// Estimate the amount of nodes we're going to need
	uint32 num_leaves = (uint32)(mMaxBodies + 1) / 2; // Assume 50% fill
	uint32 num_leaves_plus_internal_nodes = num_leaves + (num_leaves + 2) / 3; // = Sum(num_leaves * 4^-i) with i = [0, Inf].
	mAllocator.Init(2 * num_leaves_plus_internal_nodes, 256); // We use double the amount of nodes while rebuilding the tree during Update()

	// Init sub trees
	mLayers = new QuadTree [mNumLayers];
	for (uint l = 0; l < mNumLayers; ++l)
	{
		mLayers[l].Init(mAllocator);

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
		// Set the name of the layer
		mLayers[l].SetName(inLayerInterface.GetBroadPhaseLayerName(BroadPhaseLayer(BroadPhaseLayer::Type(l))));
#endif // JPH_EXTERNAL_PROFILE || JPH_PROFILE_ENABLED
	}
}

void BroadPhaseQuadTree::FrameSync()
{
	JPH_PROFILE_FUNCTION();

	// Take a unique lock on the old query lock so that we know no one is using the old nodes anymore.
	// Note that nothing should be locked at this point to avoid risking a lock inversion deadlock.
	// Note that in other places where we lock this mutex we don't use SharedLock to detect lock inversions. As long as
	// nothing else is locked this is safe. This is why BroadPhaseQuery should be the highest priority lock.
	UniqueLock root_lock(mQueryLocks[mQueryLockIdx ^ 1] JPH_IF_ENABLE_ASSERTS(, mLockContext, EPhysicsLockTypes::BroadPhaseQuery));

	for (BroadPhaseLayer::Type l = 0; l < mNumLayers; ++l)
		mLayers[l].DiscardOldTree();
}

void BroadPhaseQuadTree::Optimize()
{
	JPH_PROFILE_FUNCTION();

	FrameSync();

	LockModifications();

	for (uint l = 0; l < mNumLayers; ++l)
	{
		QuadTree &tree = mLayers[l];
		if (tree.HasBodies())
		{
			QuadTree::UpdateState update_state;
			tree.UpdatePrepare(mBodyManager->GetBodies(), mTracking, update_state, true);
			tree.UpdateFinalize(mBodyManager->GetBodies(), mTracking, update_state);
		}
	}

	UnlockModifications();

	mNextLayerToUpdate = 0;
}

void BroadPhaseQuadTree::LockModifications()
{
	// From this point on we prevent modifications to the tree
	PhysicsLock::sLock(mUpdateMutex JPH_IF_ENABLE_ASSERTS(, mLockContext, EPhysicsLockTypes::BroadPhaseUpdate));
}

BroadPhase::UpdateState BroadPhaseQuadTree::UpdatePrepare()
{
	// LockModifications should have been called
	JPH_ASSERT(mUpdateMutex.is_locked());

	// Create update state
	UpdateState update_state;
	UpdateStateImpl *update_state_impl = reinterpret_cast<UpdateStateImpl *>(&update_state);

	// Loop until we've seen all layers
	for (uint iteration = 0; iteration < mNumLayers; ++iteration)
	{
		// Get the layer
		QuadTree &tree = mLayers[mNextLayerToUpdate];
		mNextLayerToUpdate = (mNextLayerToUpdate + 1) % mNumLayers;

		// If it is dirty we update this one
		if (tree.HasBodies() && tree.IsDirty() && tree.CanBeUpdated())
		{
			update_state_impl->mTree = &tree;
			tree.UpdatePrepare(mBodyManager->GetBodies(), mTracking, update_state_impl->mUpdateState, false);
			return update_state;
		}
	}

	// Nothing to update
	update_state_impl->mTree = nullptr;
	return update_state;
}

void BroadPhaseQuadTree::UpdateFinalize(const UpdateState &inUpdateState)
{
	// LockModifications should have been called
	JPH_ASSERT(mUpdateMutex.is_locked());

	// Test if a tree was updated
	const UpdateStateImpl *update_state_impl = reinterpret_cast<const UpdateStateImpl *>(&inUpdateState);
	if (update_state_impl->mTree == nullptr)
		return;

	update_state_impl->mTree->UpdateFinalize(mBodyManager->GetBodies(), mTracking, update_state_impl->mUpdateState);

	// Make all queries from now on use the new lock
	mQueryLockIdx = mQueryLockIdx ^ 1;
}

void BroadPhaseQuadTree::UnlockModifications()
{
	// From this point on we allow modifications to the tree again
	PhysicsLock::sUnlock(mUpdateMutex JPH_IF_ENABLE_ASSERTS(, mLockContext, EPhysicsLockTypes::BroadPhaseUpdate));
}

BroadPhase::AddState BroadPhaseQuadTree::AddBodiesPrepare(BodyID *ioBodies, int inNumber)
{
	JPH_PROFILE_FUNCTION();

	JPH_ASSERT(inNumber > 0);

	const BodyVector &bodies = mBodyManager->GetBodies();
	JPH_ASSERT(mMaxBodies == mBodyManager->GetMaxBodies());

	LayerState *state = new LayerState [mNumLayers];

	// Sort bodies on layer
	Body * const * const bodies_ptr = bodies.data(); // C pointer or else sort is incredibly slow in debug mode
	QuickSort(ioBodies, ioBodies + inNumber, [bodies_ptr](BodyID inLHS, BodyID inRHS) { return bodies_ptr[inLHS.GetIndex()]->GetBroadPhaseLayer() < bodies_ptr[inRHS.GetIndex()]->GetBroadPhaseLayer(); });

	BodyID *b_start = ioBodies, *b_end = ioBodies + inNumber;
	while (b_start < b_end)
	{
		// Get broadphase layer
		BroadPhaseLayer::Type broadphase_layer = (BroadPhaseLayer::Type)bodies[b_start->GetIndex()]->GetBroadPhaseLayer();
		JPH_ASSERT(broadphase_layer < mNumLayers);

		// Find first body with different layer
		BodyID *b_mid = std::upper_bound(b_start, b_end, broadphase_layer, [bodies_ptr](BroadPhaseLayer::Type inLayer, BodyID inBodyID) { return inLayer < (BroadPhaseLayer::Type)bodies_ptr[inBodyID.GetIndex()]->GetBroadPhaseLayer(); });

		// Keep track of state for this layer
		LayerState &layer_state = state[broadphase_layer];
		layer_state.mBodyStart = b_start;
		layer_state.mBodyEnd = b_mid;

		// Insert all bodies of the same layer
		mLayers[broadphase_layer].AddBodiesPrepare(bodies, mTracking, b_start, int(b_mid - b_start), layer_state.mAddState);

		// Keep track in which tree we placed the object
		for (const BodyID *b = b_start; b < b_mid; ++b)
		{
			uint32 index = b->GetIndex();
			JPH_ASSERT(bodies[index]->GetID() == *b, "Provided BodyID doesn't match BodyID in body manager");
			JPH_ASSERT(!bodies[index]->IsInBroadPhase());
			Tracking &t = mTracking[index];
			JPH_ASSERT(t.mBroadPhaseLayer == (BroadPhaseLayer::Type)cBroadPhaseLayerInvalid);
			t.mBroadPhaseLayer = broadphase_layer;
			JPH_ASSERT(t.mObjectLayer == cObjectLayerInvalid);
			t.mObjectLayer = bodies[index]->GetObjectLayer();
		}

		// Repeat
		b_start = b_mid;
	}

	return state;
}

void BroadPhaseQuadTree::AddBodiesFinalize(BodyID *ioBodies, int inNumber, AddState inAddState)
{
	JPH_PROFILE_FUNCTION();

	// This cannot run concurrently with UpdatePrepare()/UpdateFinalize()
	SharedLock lock(mUpdateMutex JPH_IF_ENABLE_ASSERTS(, mLockContext, EPhysicsLockTypes::BroadPhaseUpdate));

	BodyVector &bodies = mBodyManager->GetBodies();
	JPH_ASSERT(mMaxBodies == mBodyManager->GetMaxBodies());

	LayerState *state = (LayerState *)inAddState;

	for (BroadPhaseLayer::Type broadphase_layer = 0; broadphase_layer < mNumLayers; broadphase_layer++)
	{
		const LayerState &l = state[broadphase_layer];
		if (l.mBodyStart != nullptr)
		{
			// Insert all bodies of the same layer
			mLayers[broadphase_layer].AddBodiesFinalize(mTracking, int(l.mBodyEnd - l.mBodyStart), l.mAddState);

			// Mark added to broadphase
			for (const BodyID *b = l.mBodyStart; b < l.mBodyEnd; ++b)
			{
				uint32 index = b->GetIndex();
				JPH_ASSERT(bodies[index]->GetID() == *b, "Provided BodyID doesn't match BodyID in body manager");
				JPH_ASSERT(mTracking[index].mBroadPhaseLayer == broadphase_layer);
				JPH_ASSERT(mTracking[index].mObjectLayer == bodies[index]->GetObjectLayer());
				JPH_ASSERT(!bodies[index]->IsInBroadPhase());
				bodies[index]->SetInBroadPhaseInternal(true);
			}
		}
	}

	delete [] state;
}

void BroadPhaseQuadTree::AddBodiesAbort(BodyID *ioBodies, int inNumber, AddState inAddState)
{
	JPH_PROFILE_FUNCTION();

	JPH_IF_ENABLE_ASSERTS(const BodyVector &bodies = mBodyManager->GetBodies();)
	JPH_ASSERT(mMaxBodies == mBodyManager->GetMaxBodies());

	LayerState *state = (LayerState *)inAddState;

	for (BroadPhaseLayer::Type broadphase_layer = 0; broadphase_layer < mNumLayers; broadphase_layer++)
	{
		const LayerState &l = state[broadphase_layer];
		if (l.mBodyStart != nullptr)
		{
			// Insert all bodies of the same layer
			mLayers[broadphase_layer].AddBodiesAbort(mTracking, l.mAddState);

			// Reset bookkeeping
			for (const BodyID *b = l.mBodyStart; b < l.mBodyEnd; ++b)
			{
				uint32 index = b->GetIndex();
				JPH_ASSERT(bodies[index]->GetID() == *b, "Provided BodyID doesn't match BodyID in body manager");
				JPH_ASSERT(!bodies[index]->IsInBroadPhase());
				Tracking &t = mTracking[index];
				JPH_ASSERT(t.mBroadPhaseLayer == broadphase_layer);
				t.mBroadPhaseLayer = (BroadPhaseLayer::Type)cBroadPhaseLayerInvalid;
				t.mObjectLayer = cObjectLayerInvalid;
			}
		}
	}

	delete [] state;
}

void BroadPhaseQuadTree::RemoveBodies(BodyID *ioBodies, int inNumber)
{
	JPH_PROFILE_FUNCTION();

	// This cannot run concurrently with UpdatePrepare()/UpdateFinalize()
	SharedLock lock(mUpdateMutex JPH_IF_ENABLE_ASSERTS(, mLockContext, EPhysicsLockTypes::BroadPhaseUpdate));

	JPH_ASSERT(inNumber > 0);

	BodyVector &bodies = mBodyManager->GetBodies();
	JPH_ASSERT(mMaxBodies == mBodyManager->GetMaxBodies());

	// Sort bodies on layer
	Tracking *tracking = mTracking.data(); // C pointer or else sort is incredibly slow in debug mode
	QuickSort(ioBodies, ioBodies + inNumber, [tracking](BodyID inLHS, BodyID inRHS) { return tracking[inLHS.GetIndex()].mBroadPhaseLayer < tracking[inRHS.GetIndex()].mBroadPhaseLayer; });

	BodyID *b_start = ioBodies, *b_end = ioBodies + inNumber;
	while (b_start < b_end)
	{
		// Get broad phase layer
		BroadPhaseLayer::Type broadphase_layer = mTracking[b_start->GetIndex()].mBroadPhaseLayer;
		JPH_ASSERT(broadphase_layer != (BroadPhaseLayer::Type)cBroadPhaseLayerInvalid);

		// Find first body with different layer
		BodyID *b_mid = std::upper_bound(b_start, b_end, broadphase_layer, [tracking](BroadPhaseLayer::Type inLayer, BodyID inBodyID) { return inLayer < tracking[inBodyID.GetIndex()].mBroadPhaseLayer; });

		// Remove all bodies of the same layer
		mLayers[broadphase_layer].RemoveBodies(bodies, mTracking, b_start, int(b_mid - b_start));

		for (const BodyID *b = b_start; b < b_mid; ++b)
		{
			// Reset bookkeeping
			uint32 index = b->GetIndex();
			Tracking &t = tracking[index];
			t.mBroadPhaseLayer = (BroadPhaseLayer::Type)cBroadPhaseLayerInvalid;
			t.mObjectLayer = cObjectLayerInvalid;

			// Mark removed from broadphase
			JPH_ASSERT(bodies[index]->IsInBroadPhase());
			bodies[index]->SetInBroadPhaseInternal(false);
		}

		// Repeat
		b_start = b_mid;
	}
}

void BroadPhaseQuadTree::NotifyBodiesAABBChanged(BodyID *ioBodies, int inNumber, bool inTakeLock)
{
	JPH_PROFILE_FUNCTION();

	JPH_ASSERT(inNumber > 0);

	// This cannot run concurrently with UpdatePrepare()/UpdateFinalize()
	if (inTakeLock)
		PhysicsLock::sLockShared(mUpdateMutex JPH_IF_ENABLE_ASSERTS(, mLockContext, EPhysicsLockTypes::BroadPhaseUpdate));
	else
		JPH_ASSERT(mUpdateMutex.is_locked());

	const BodyVector &bodies = mBodyManager->GetBodies();
	JPH_ASSERT(mMaxBodies == mBodyManager->GetMaxBodies());

	// Sort bodies on layer
	const Tracking *tracking = mTracking.data(); // C pointer or else sort is incredibly slow in debug mode
	QuickSort(ioBodies, ioBodies + inNumber, [tracking](BodyID inLHS, BodyID inRHS) { return tracking[inLHS.GetIndex()].mBroadPhaseLayer < tracking[inRHS.GetIndex()].mBroadPhaseLayer; });

	BodyID *b_start = ioBodies, *b_end = ioBodies + inNumber;
	while (b_start < b_end)
	{
		// Get broadphase layer
		BroadPhaseLayer::Type broadphase_layer = tracking[b_start->GetIndex()].mBroadPhaseLayer;
		JPH_ASSERT(broadphase_layer != (BroadPhaseLayer::Type)cBroadPhaseLayerInvalid);

		// Find first body with different layer
		BodyID *b_mid = std::upper_bound(b_start, b_end, broadphase_layer, [tracking](BroadPhaseLayer::Type inLayer, BodyID inBodyID) { return inLayer < tracking[inBodyID.GetIndex()].mBroadPhaseLayer; });

		// Notify all bodies of the same layer changed
		mLayers[broadphase_layer].NotifyBodiesAABBChanged(bodies, mTracking, b_start, int(b_mid - b_start));

		// Repeat
		b_start = b_mid;
	}

	if (inTakeLock)
		PhysicsLock::sUnlockShared(mUpdateMutex JPH_IF_ENABLE_ASSERTS(, mLockContext, EPhysicsLockTypes::BroadPhaseUpdate));
}

void BroadPhaseQuadTree::NotifyBodiesLayerChanged(BodyID *ioBodies, int inNumber)
{
	JPH_PROFILE_FUNCTION();

	JPH_ASSERT(inNumber > 0);

	// First sort the bodies that actually changed layer to beginning of the array
	const BodyVector &bodies = mBodyManager->GetBodies();
	JPH_ASSERT(mMaxBodies == mBodyManager->GetMaxBodies());
	for (BodyID *body_id = ioBodies + inNumber - 1; body_id >= ioBodies; --body_id)
	{
		uint32 index = body_id->GetIndex();
		JPH_ASSERT(bodies[index]->GetID() == *body_id, "Provided BodyID doesn't match BodyID in body manager");
		const Body *body = bodies[index];
		BroadPhaseLayer::Type broadphase_layer = (BroadPhaseLayer::Type)body->GetBroadPhaseLayer();
		JPH_ASSERT(broadphase_layer < mNumLayers);
		if (mTracking[index].mBroadPhaseLayer == broadphase_layer)
		{
			// Update tracking information
			mTracking[index].mObjectLayer = body->GetObjectLayer();

			// Move the body to the end, layer didn't change
			std::swap(*body_id, ioBodies[inNumber - 1]);
			--inNumber;
		}
	}

	if (inNumber > 0)
	{
		// Changing layer requires us to remove from one tree and add to another, so this is equivalent to removing all bodies first and then adding them again
		RemoveBodies(ioBodies, inNumber);
		AddState add_state = AddBodiesPrepare(ioBodies, inNumber);
		AddBodiesFinalize(ioBodies, inNumber, add_state);
	}
}

void BroadPhaseQuadTree::CastRay(const RayCast &inRay, RayCastBodyCollector &ioCollector, const BroadPhaseLayerFilter &inBroadPhaseLayerFilter, const ObjectLayerFilter &inObjectLayerFilter) const
{
	JPH_PROFILE_FUNCTION();

	JPH_ASSERT(mMaxBodies == mBodyManager->GetMaxBodies());

	// Prevent this from running in parallel with node deletion in FrameSync(), see notes there
	shared_lock lock(mQueryLocks[mQueryLockIdx]);

	// Loop over all layers and test the ones that could hit
	for (BroadPhaseLayer::Type l = 0; l < mNumLayers; ++l)
	{
		const QuadTree &tree = mLayers[l];
		if (tree.HasBodies() && inBroadPhaseLayerFilter.ShouldCollide(BroadPhaseLayer(l)))
		{
			JPH_PROFILE(tree.GetName());
			tree.CastRay(inRay, ioCollector, inObjectLayerFilter, mTracking);
			if (ioCollector.ShouldEarlyOut())
				break;
		}
	}
}

void BroadPhaseQuadTree::CollideAABox(const AABox &inBox, CollideShapeBodyCollector &ioCollector, const BroadPhaseLayerFilter &inBroadPhaseLayerFilter, const ObjectLayerFilter &inObjectLayerFilter) const
{
	JPH_PROFILE_FUNCTION();

	JPH_ASSERT(mMaxBodies == mBodyManager->GetMaxBodies());

	// Prevent this from running in parallel with node deletion in FrameSync(), see notes there
	shared_lock lock(mQueryLocks[mQueryLockIdx]);

	// Loop over all layers and test the ones that could hit
	for (BroadPhaseLayer::Type l = 0; l < mNumLayers; ++l)
	{
		const QuadTree &tree = mLayers[l];
		if (tree.HasBodies() && inBroadPhaseLayerFilter.ShouldCollide(BroadPhaseLayer(l)))
		{
			JPH_PROFILE(tree.GetName());
			tree.CollideAABox(inBox, ioCollector, inObjectLayerFilter, mTracking);
			if (ioCollector.ShouldEarlyOut())
				break;
		}
	}
}

void BroadPhaseQuadTree::CollideSphere(Vec3Arg inCenter, float inRadius, CollideShapeBodyCollector &ioCollector, const BroadPhaseLayerFilter &inBroadPhaseLayerFilter, const ObjectLayerFilter &inObjectLayerFilter) const
{
	JPH_PROFILE_FUNCTION();

	JPH_ASSERT(mMaxBodies == mBodyManager->GetMaxBodies());

	// Prevent this from running in parallel with node deletion in FrameSync(), see notes there
	shared_lock lock(mQueryLocks[mQueryLockIdx]);

	// Loop over all layers and test the ones that could hit
	for (BroadPhaseLayer::Type l = 0; l < mNumLayers; ++l)
	{
		const QuadTree &tree = mLayers[l];
		if (tree.HasBodies() && inBroadPhaseLayerFilter.ShouldCollide(BroadPhaseLayer(l)))
		{
			JPH_PROFILE(tree.GetName());
			tree.CollideSphere(inCenter, inRadius, ioCollector, inObjectLayerFilter, mTracking);
			if (ioCollector.ShouldEarlyOut())
				break;
		}
	}
}

void BroadPhaseQuadTree::CollidePoint(Vec3Arg inPoint, CollideShapeBodyCollector &ioCollector, const BroadPhaseLayerFilter &inBroadPhaseLayerFilter, const ObjectLayerFilter &inObjectLayerFilter) const
{
	JPH_PROFILE_FUNCTION();

	JPH_ASSERT(mMaxBodies == mBodyManager->GetMaxBodies());

	// Prevent this from running in parallel with node deletion in FrameSync(), see notes there
	shared_lock lock(mQueryLocks[mQueryLockIdx]);

	// Loop over all layers and test the ones that could hit
	for (BroadPhaseLayer::Type l = 0; l < mNumLayers; ++l)
	{
		const QuadTree &tree = mLayers[l];
		if (tree.HasBodies() && inBroadPhaseLayerFilter.ShouldCollide(BroadPhaseLayer(l)))
		{
			JPH_PROFILE(tree.GetName());
			tree.CollidePoint(inPoint, ioCollector, inObjectLayerFilter, mTracking);
			if (ioCollector.ShouldEarlyOut())
				break;
		}
	}
}

void BroadPhaseQuadTree::CollideOrientedBox(const OrientedBox &inBox, CollideShapeBodyCollector &ioCollector, const BroadPhaseLayerFilter &inBroadPhaseLayerFilter, const ObjectLayerFilter &inObjectLayerFilter) const
{
	JPH_PROFILE_FUNCTION();

	JPH_ASSERT(mMaxBodies == mBodyManager->GetMaxBodies());

	// Prevent this from running in parallel with node deletion in FrameSync(), see notes there
	shared_lock lock(mQueryLocks[mQueryLockIdx]);

	// Loop over all layers and test the ones that could hit
	for (BroadPhaseLayer::Type l = 0; l < mNumLayers; ++l)
	{
		const QuadTree &tree = mLayers[l];
		if (tree.HasBodies() && inBroadPhaseLayerFilter.ShouldCollide(BroadPhaseLayer(l)))
		{
			JPH_PROFILE(tree.GetName());
			tree.CollideOrientedBox(inBox, ioCollector, inObjectLayerFilter, mTracking);
			if (ioCollector.ShouldEarlyOut())
				break;
		}
	}
}

void BroadPhaseQuadTree::CastAABoxNoLock(const AABoxCast &inBox, CastShapeBodyCollector &ioCollector, const BroadPhaseLayerFilter &inBroadPhaseLayerFilter, const ObjectLayerFilter &inObjectLayerFilter) const
{
	JPH_PROFILE_FUNCTION();

	JPH_ASSERT(mMaxBodies == mBodyManager->GetMaxBodies());

	// Loop over all layers and test the ones that could hit
	for (BroadPhaseLayer::Type l = 0; l < mNumLayers; ++l)
	{
		const QuadTree &tree = mLayers[l];
		if (tree.HasBodies() && inBroadPhaseLayerFilter.ShouldCollide(BroadPhaseLayer(l)))
		{
			JPH_PROFILE(tree.GetName());
			tree.CastAABox(inBox, ioCollector, inObjectLayerFilter, mTracking);
			if (ioCollector.ShouldEarlyOut())
				break;
		}
	}
}

void BroadPhaseQuadTree::CastAABox(const AABoxCast &inBox, CastShapeBodyCollector &ioCollector, const BroadPhaseLayerFilter &inBroadPhaseLayerFilter, const ObjectLayerFilter &inObjectLayerFilter) const
{
	// Prevent this from running in parallel with node deletion in FrameSync(), see notes there
	shared_lock lock(mQueryLocks[mQueryLockIdx]);

	CastAABoxNoLock(inBox, ioCollector, inBroadPhaseLayerFilter, inObjectLayerFilter);
}

void BroadPhaseQuadTree::FindCollidingPairs(BodyID *ioActiveBodies, int inNumActiveBodies, float inSpeculativeContactDistance, const ObjectVsBroadPhaseLayerFilter &inObjectVsBroadPhaseLayerFilter, const ObjectLayerPairFilter &inObjectLayerPairFilter, BodyPairCollector &ioPairCollector) const
{
	JPH_PROFILE_FUNCTION();

	const BodyVector &bodies = mBodyManager->GetBodies();
	JPH_ASSERT(mMaxBodies == mBodyManager->GetMaxBodies());

	// Note that we don't take any locks at this point. We know that the tree is not going to be swapped or deleted while finding collision pairs due to the way the jobs are scheduled in the PhysicsSystem::Update.

	// Sort bodies on layer
	const Tracking *tracking = mTracking.data(); // C pointer or else sort is incredibly slow in debug mode
	QuickSort(ioActiveBodies, ioActiveBodies + inNumActiveBodies, [tracking](BodyID inLHS, BodyID inRHS) { return tracking[inLHS.GetIndex()].mObjectLayer < tracking[inRHS.GetIndex()].mObjectLayer; });

	BodyID *b_start = ioActiveBodies, *b_end = ioActiveBodies + inNumActiveBodies;
	while (b_start < b_end)
	{
		// Get broadphase layer
		ObjectLayer object_layer = tracking[b_start->GetIndex()].mObjectLayer;
		JPH_ASSERT(object_layer != cObjectLayerInvalid);

		// Find first body with different layer
		BodyID *b_mid = std::upper_bound(b_start, b_end, object_layer, [tracking](ObjectLayer inLayer, BodyID inBodyID) { return inLayer < tracking[inBodyID.GetIndex()].mObjectLayer; });

		// Loop over all layers and test the ones that could hit
		for (BroadPhaseLayer::Type l = 0; l < mNumLayers; ++l)
		{
			const QuadTree &tree = mLayers[l];
			if (tree.HasBodies() && inObjectVsBroadPhaseLayerFilter.ShouldCollide(object_layer, BroadPhaseLayer(l)))
			{
				JPH_PROFILE(tree.GetName());
				tree.FindCollidingPairs(bodies, b_start, int(b_mid - b_start), inSpeculativeContactDistance, ioPairCollector, inObjectLayerPairFilter);
			}
		}

		// Repeat
		b_start = b_mid;
	}
}

AABox BroadPhaseQuadTree::GetBounds() const
{
	// Prevent this from running in parallel with node deletion in FrameSync(), see notes there
	shared_lock lock(mQueryLocks[mQueryLockIdx]);

	AABox bounds;
	for (BroadPhaseLayer::Type l = 0; l < mNumLayers; ++l)
		bounds.Encapsulate(mLayers[l].GetBounds());
	return bounds;
}

#ifdef JPH_TRACK_BROADPHASE_STATS

void BroadPhaseQuadTree::ReportStats()
{
	Trace("Query Type, Filter Description, Tree Name, Num Queries, Total Time (%%), Total Time Excl. Collector (%%), Nodes Visited, Bodies Visited, Hits Reported, Hits Reported vs Bodies Visited (%%), Hits Reported vs Nodes Visited");

	uint64 total_ticks = 0;
	for (BroadPhaseLayer::Type l = 0; l < mNumLayers; ++l)
		total_ticks += mLayers[l].GetTicks100Pct();

	for (BroadPhaseLayer::Type l = 0; l < mNumLayers; ++l)
		mLayers[l].ReportStats(total_ticks);
}

#endif // JPH_TRACK_BROADPHASE_STATS

JPH_NAMESPACE_END

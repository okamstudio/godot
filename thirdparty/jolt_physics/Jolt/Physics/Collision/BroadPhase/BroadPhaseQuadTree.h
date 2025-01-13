// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Jolt/Physics/Collision/BroadPhase/QuadTree.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhase.h>
#include <Jolt/Physics/PhysicsLock.h>

JPH_NAMESPACE_BEGIN

/// Fast SIMD based quad tree BroadPhase that is multithreading aware and tries to do a minimal amount of locking.
class JPH_EXPORT BroadPhaseQuadTree final : public BroadPhase
{
public:
	JPH_OVERRIDE_NEW_DELETE

	/// Destructor
	virtual					~BroadPhaseQuadTree() override;

	// Implementing interface of BroadPhase (see BroadPhase for documentation)
	virtual void			Init(BodyManager *inBodyManager, const BroadPhaseLayerInterface &inLayerInterface) override;
	virtual void			Optimize() override;
	virtual void			FrameSync() override;
	virtual void			LockModifications() override;
	virtual	UpdateState		UpdatePrepare() override;
	virtual void			UpdateFinalize(const UpdateState &inUpdateState) override;
	virtual void			UnlockModifications() override;
	virtual AddState		AddBodiesPrepare(BodyID *ioBodies, int inNumber) override;
	virtual void			AddBodiesFinalize(BodyID *ioBodies, int inNumber, AddState inAddState) override;
	virtual void			AddBodiesAbort(BodyID *ioBodies, int inNumber, AddState inAddState) override;
	virtual void			RemoveBodies(BodyID *ioBodies, int inNumber) override;
	virtual void			NotifyBodiesAABBChanged(BodyID *ioBodies, int inNumber, bool inTakeLock) override;
	virtual void			NotifyBodiesLayerChanged(BodyID *ioBodies, int inNumber) override;
	virtual void			CastRay(const RayCast &inRay, RayCastBodyCollector &ioCollector, const BroadPhaseLayerFilter &inBroadPhaseLayerFilter, const ObjectLayerFilter &inObjectLayerFilter) const override;
	virtual void			CollideAABox(const AABox &inBox, CollideShapeBodyCollector &ioCollector, const BroadPhaseLayerFilter &inBroadPhaseLayerFilter, const ObjectLayerFilter &inObjectLayerFilter) const override;
	virtual void			CollideSphere(Vec3Arg inCenter, float inRadius, CollideShapeBodyCollector &ioCollector, const BroadPhaseLayerFilter &inBroadPhaseLayerFilter, const ObjectLayerFilter &inObjectLayerFilter) const override;
	virtual void			CollidePoint(Vec3Arg inPoint, CollideShapeBodyCollector &ioCollector, const BroadPhaseLayerFilter &inBroadPhaseLayerFilter, const ObjectLayerFilter &inObjectLayerFilter) const override;
	virtual void			CollideOrientedBox(const OrientedBox &inBox, CollideShapeBodyCollector &ioCollector, const BroadPhaseLayerFilter &inBroadPhaseLayerFilter, const ObjectLayerFilter &inObjectLayerFilter) const override;
	virtual void			CastAABoxNoLock(const AABoxCast &inBox, CastShapeBodyCollector &ioCollector, const BroadPhaseLayerFilter &inBroadPhaseLayerFilter, const ObjectLayerFilter &inObjectLayerFilter) const override;
	virtual void			CastAABox(const AABoxCast &inBox, CastShapeBodyCollector &ioCollector, const BroadPhaseLayerFilter &inBroadPhaseLayerFilter, const ObjectLayerFilter &inObjectLayerFilter) const override;
	virtual void			FindCollidingPairs(BodyID *ioActiveBodies, int inNumActiveBodies, float inSpeculativeContactDistance, const ObjectVsBroadPhaseLayerFilter &inObjectVsBroadPhaseLayerFilter, const ObjectLayerPairFilter &inObjectLayerPairFilter, BodyPairCollector &ioPairCollector) const override;
	virtual AABox			GetBounds() const override;
#ifdef JPH_TRACK_BROADPHASE_STATS
	virtual void			ReportStats() override;
#endif // JPH_TRACK_BROADPHASE_STATS

private:
	/// Helper struct for AddBodies handle
	struct LayerState
	{
		JPH_OVERRIDE_NEW_DELETE

		BodyID *			mBodyStart = nullptr;
		BodyID *			mBodyEnd;
		QuadTree::AddState	mAddState;
	};

	using Tracking = QuadTree::Tracking;
	using TrackingVector = QuadTree::TrackingVector;

#ifdef JPH_ENABLE_ASSERTS
	/// Context used to lock a physics lock
	PhysicsLockContext		mLockContext = nullptr;
#endif // JPH_ENABLE_ASSERTS

	/// Max amount of bodies we support
	size_t					mMaxBodies = 0;

	/// Array that for each BodyID keeps track of where it is located in which tree
	TrackingVector			mTracking;

	/// Node allocator for all trees
	QuadTree::Allocator		mAllocator;

	/// Information about broad phase layers
	const BroadPhaseLayerInterface *mBroadPhaseLayerInterface = nullptr;

	/// One tree per object layer
	QuadTree *				mLayers;
	uint					mNumLayers;

	/// UpdateState implementation for this tree used during UpdatePrepare/Finalize()
	struct UpdateStateImpl
	{
		QuadTree *				mTree;
		QuadTree::UpdateState	mUpdateState;
	};

	static_assert(sizeof(UpdateStateImpl) <= sizeof(UpdateState));
	static_assert(alignof(UpdateStateImpl) <= alignof(UpdateState));

	/// Mutex that prevents object modification during UpdatePrepare/Finalize()
	SharedMutex				mUpdateMutex;

	/// We double buffer all trees so that we can query while building the next one and we destroy the old tree the next physics update.
	/// This structure ensures that we wait for queries that are still using the old tree.
	mutable SharedMutex		mQueryLocks[2];

	/// This index indicates which lock is currently active, it alternates between 0 and 1
	atomic<uint32>			mQueryLockIdx { 0 };

	/// This is the next tree to update in UpdatePrepare()
	uint32					mNextLayerToUpdate = 0;
};

JPH_NAMESPACE_END

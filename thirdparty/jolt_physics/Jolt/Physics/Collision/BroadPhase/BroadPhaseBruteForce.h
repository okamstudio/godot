// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Jolt/Physics/Collision/BroadPhase/BroadPhase.h>
#include <Jolt/Core/Mutex.h>

JPH_NAMESPACE_BEGIN

/// Test BroadPhase implementation that does not do anything to speed up the operations. Can be used as a reference implementation.
class JPH_EXPORT BroadPhaseBruteForce final : public BroadPhase
{
public:
	JPH_OVERRIDE_NEW_DELETE

	// Implementing interface of BroadPhase (see BroadPhase for documentation)
	virtual void		AddBodiesFinalize(BodyID *ioBodies, int inNumber, AddState inAddState) override;
	virtual void		RemoveBodies(BodyID *ioBodies, int inNumber) override;
	virtual void		NotifyBodiesAABBChanged(BodyID *ioBodies, int inNumber, bool inTakeLock) override;
	virtual void		NotifyBodiesLayerChanged(BodyID *ioBodies, int inNumber) override;
	virtual void		CastRay(const RayCast &inRay, RayCastBodyCollector &ioCollector, const BroadPhaseLayerFilter &inBroadPhaseLayerFilter, const ObjectLayerFilter &inObjectLayerFilter) const override;
	virtual void		CollideAABox(const AABox &inBox, CollideShapeBodyCollector &ioCollector, const BroadPhaseLayerFilter &inBroadPhaseLayerFilter, const ObjectLayerFilter &inObjectLayerFilter) const override;
	virtual void		CollideSphere(Vec3Arg inCenter, float inRadius, CollideShapeBodyCollector &ioCollector, const BroadPhaseLayerFilter &inBroadPhaseLayerFilter, const ObjectLayerFilter &inObjectLayerFilter) const override;
	virtual void		CollidePoint(Vec3Arg inPoint, CollideShapeBodyCollector &ioCollector, const BroadPhaseLayerFilter &inBroadPhaseLayerFilter, const ObjectLayerFilter &inObjectLayerFilter) const override;
	virtual void		CollideOrientedBox(const OrientedBox &inBox, CollideShapeBodyCollector &ioCollector, const BroadPhaseLayerFilter &inBroadPhaseLayerFilter, const ObjectLayerFilter &inObjectLayerFilter) const override;
	virtual void		CastAABoxNoLock(const AABoxCast &inBox, CastShapeBodyCollector &ioCollector, const BroadPhaseLayerFilter &inBroadPhaseLayerFilter, const ObjectLayerFilter &inObjectLayerFilter) const override;
	virtual void		CastAABox(const AABoxCast &inBox, CastShapeBodyCollector &ioCollector, const BroadPhaseLayerFilter &inBroadPhaseLayerFilter, const ObjectLayerFilter &inObjectLayerFilter) const override;
	virtual void		FindCollidingPairs(BodyID *ioActiveBodies, int inNumActiveBodies, float inSpeculativeContactDistance, const ObjectVsBroadPhaseLayerFilter &inObjectVsBroadPhaseLayerFilter, const ObjectLayerPairFilter &inObjectLayerPairFilter, BodyPairCollector &ioPairCollector) const override;
	virtual AABox		GetBounds() const override;

private:
	Array<BodyID>		mBodyIDs;
	mutable SharedMutex	mMutex;
};

JPH_NAMESPACE_END

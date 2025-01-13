// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#include <Jolt/Jolt.h>

#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhase.h>
#include <Jolt/Physics/Collision/CollisionCollectorImpl.h>
#include <Jolt/Physics/Body/Body.h>
#include <Jolt/Physics/Body/BodyManager.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Body/BodyLockMulti.h>
#include <Jolt/Physics/Collision/PhysicsMaterial.h>
#include <Jolt/Physics/Constraints/TwoBodyConstraint.h>

JPH_NAMESPACE_BEGIN

void BodyInterface::ActivateBodyInternal(Body &ioBody) const
{
	// Activate body or reset its sleep timer.
	// Note that BodyManager::ActivateBodies also resets the sleep timer internally, but we avoid a mutex lock if the body is already active by calling ResetSleepTimer directly.
	if (!ioBody.IsActive())
		mBodyManager->ActivateBodies(&ioBody.GetID(), 1);
	else
		ioBody.ResetSleepTimer();
}

Body *BodyInterface::CreateBody(const BodyCreationSettings &inSettings)
{
	Body *body = mBodyManager->AllocateBody(inSettings);
	if (!mBodyManager->AddBody(body))
	{
		mBodyManager->FreeBody(body);
		return nullptr;
	}
	return body;
}

Body *BodyInterface::CreateSoftBody(const SoftBodyCreationSettings &inSettings)
{
	Body *body = mBodyManager->AllocateSoftBody(inSettings);
	if (!mBodyManager->AddBody(body))
	{
		mBodyManager->FreeBody(body);
		return nullptr;
	}
	return body;
}

Body *BodyInterface::CreateBodyWithID(const BodyID &inBodyID, const BodyCreationSettings &inSettings)
{
	Body *body = mBodyManager->AllocateBody(inSettings);
	if (!mBodyManager->AddBodyWithCustomID(body, inBodyID))
	{
		mBodyManager->FreeBody(body);
		return nullptr;
	}
	return body;
}

Body *BodyInterface::CreateSoftBodyWithID(const BodyID &inBodyID, const SoftBodyCreationSettings &inSettings)
{
	Body *body = mBodyManager->AllocateSoftBody(inSettings);
	if (!mBodyManager->AddBodyWithCustomID(body, inBodyID))
	{
		mBodyManager->FreeBody(body);
		return nullptr;
	}
	return body;
}

Body *BodyInterface::CreateBodyWithoutID(const BodyCreationSettings &inSettings) const
{
	return mBodyManager->AllocateBody(inSettings);
}

Body *BodyInterface::CreateSoftBodyWithoutID(const SoftBodyCreationSettings &inSettings) const
{
	return mBodyManager->AllocateSoftBody(inSettings);
}

void BodyInterface::DestroyBodyWithoutID(Body *inBody) const
{
	mBodyManager->FreeBody(inBody);
}

bool BodyInterface::AssignBodyID(Body *ioBody)
{
	return mBodyManager->AddBody(ioBody);
}

bool BodyInterface::AssignBodyID(Body *ioBody, const BodyID &inBodyID)
{
	return mBodyManager->AddBodyWithCustomID(ioBody, inBodyID);
}

Body *BodyInterface::UnassignBodyID(const BodyID &inBodyID)
{
	Body *body = nullptr;
	mBodyManager->RemoveBodies(&inBodyID, 1, &body);
	return body;
}

void BodyInterface::UnassignBodyIDs(const BodyID *inBodyIDs, int inNumber, Body **outBodies)
{
	mBodyManager->RemoveBodies(inBodyIDs, inNumber, outBodies);
}

void BodyInterface::DestroyBody(const BodyID &inBodyID)
{
	mBodyManager->DestroyBodies(&inBodyID, 1);
}

void BodyInterface::DestroyBodies(const BodyID *inBodyIDs, int inNumber)
{
	mBodyManager->DestroyBodies(inBodyIDs, inNumber);
}

void BodyInterface::AddBody(const BodyID &inBodyID, EActivation inActivationMode)
{
	BodyLockWrite lock(*mBodyLockInterface, inBodyID);
	if (lock.Succeeded())
	{
		const Body &body = lock.GetBody();

		// Add to broadphase
		BodyID id = inBodyID;
		BroadPhase::AddState add_state = mBroadPhase->AddBodiesPrepare(&id, 1);
		mBroadPhase->AddBodiesFinalize(&id, 1, add_state);

		// Optionally activate body
		if (inActivationMode == EActivation::Activate && !body.IsStatic())
			mBodyManager->ActivateBodies(&inBodyID, 1);
	}
}

void BodyInterface::RemoveBody(const BodyID &inBodyID)
{
	BodyLockWrite lock(*mBodyLockInterface, inBodyID);
	if (lock.Succeeded())
	{
		const Body &body = lock.GetBody();

		// Deactivate body
		if (body.IsActive())
			mBodyManager->DeactivateBodies(&inBodyID, 1);

		// Remove from broadphase
		BodyID id = inBodyID;
		mBroadPhase->RemoveBodies(&id, 1);
	}
}

bool BodyInterface::IsAdded(const BodyID &inBodyID) const
{
	BodyLockRead lock(*mBodyLockInterface, inBodyID);
	return lock.SucceededAndIsInBroadPhase();
}

BodyID BodyInterface::CreateAndAddBody(const BodyCreationSettings &inSettings, EActivation inActivationMode)
{
	const Body *b = CreateBody(inSettings);
	if (b == nullptr)
		return BodyID(); // Out of bodies
	AddBody(b->GetID(), inActivationMode);
	return b->GetID();
}

BodyID BodyInterface::CreateAndAddSoftBody(const SoftBodyCreationSettings &inSettings, EActivation inActivationMode)
{
	const Body *b = CreateSoftBody(inSettings);
	if (b == nullptr)
		return BodyID(); // Out of bodies
	AddBody(b->GetID(), inActivationMode);
	return b->GetID();
}

BodyInterface::AddState BodyInterface::AddBodiesPrepare(BodyID *ioBodies, int inNumber)
{
	return mBroadPhase->AddBodiesPrepare(ioBodies, inNumber);
}

void BodyInterface::AddBodiesFinalize(BodyID *ioBodies, int inNumber, AddState inAddState, EActivation inActivationMode)
{
	BodyLockMultiWrite lock(*mBodyLockInterface, ioBodies, inNumber);

	// Add to broadphase
	mBroadPhase->AddBodiesFinalize(ioBodies, inNumber, inAddState);

	// Optionally activate bodies
	if (inActivationMode == EActivation::Activate)
		mBodyManager->ActivateBodies(ioBodies, inNumber);
}

void BodyInterface::AddBodiesAbort(BodyID *ioBodies, int inNumber, AddState inAddState)
{
	mBroadPhase->AddBodiesAbort(ioBodies, inNumber, inAddState);
}

void BodyInterface::RemoveBodies(BodyID *ioBodies, int inNumber)
{
	BodyLockMultiWrite lock(*mBodyLockInterface, ioBodies, inNumber);

	// Deactivate bodies
	mBodyManager->DeactivateBodies(ioBodies, inNumber);

	// Remove from broadphase
	mBroadPhase->RemoveBodies(ioBodies, inNumber);
}

void BodyInterface::ActivateBody(const BodyID &inBodyID)
{
	BodyLockWrite lock(*mBodyLockInterface, inBodyID);
	if (lock.Succeeded())
	{
		Body &body = lock.GetBody();
		ActivateBodyInternal(body);
	}
}

void BodyInterface::ActivateBodies(const BodyID *inBodyIDs, int inNumber)
{
	BodyLockMultiWrite lock(*mBodyLockInterface, inBodyIDs, inNumber);

	mBodyManager->ActivateBodies(inBodyIDs, inNumber);
}

void BodyInterface::ActivateBodiesInAABox(const AABox &inBox, const BroadPhaseLayerFilter &inBroadPhaseLayerFilter, const ObjectLayerFilter &inObjectLayerFilter)
{
	AllHitCollisionCollector<CollideShapeBodyCollector> collector;
	mBroadPhase->CollideAABox(inBox, collector, inBroadPhaseLayerFilter, inObjectLayerFilter);
	ActivateBodies(collector.mHits.data(), (int)collector.mHits.size());
}

void BodyInterface::DeactivateBody(const BodyID &inBodyID)
{
	BodyLockWrite lock(*mBodyLockInterface, inBodyID);
	if (lock.Succeeded())
	{
		const Body &body = lock.GetBody();

		if (body.IsActive())
			mBodyManager->DeactivateBodies(&inBodyID, 1);
	}
}

void BodyInterface::DeactivateBodies(const BodyID *inBodyIDs, int inNumber)
{
	BodyLockMultiWrite lock(*mBodyLockInterface, inBodyIDs, inNumber);

	mBodyManager->DeactivateBodies(inBodyIDs, inNumber);
}

bool BodyInterface::IsActive(const BodyID &inBodyID) const
{
	BodyLockRead lock(*mBodyLockInterface, inBodyID);
	return lock.Succeeded() && lock.GetBody().IsActive();
}

void BodyInterface::ResetSleepTimer(const BodyID &inBodyID)
{
	BodyLockWrite lock(*mBodyLockInterface, inBodyID);
	if (lock.Succeeded())
		lock.GetBody().ResetSleepTimer();
}

TwoBodyConstraint *BodyInterface::CreateConstraint(const TwoBodyConstraintSettings *inSettings, const BodyID &inBodyID1, const BodyID &inBodyID2)
{
	BodyID constraint_bodies[] = { inBodyID1, inBodyID2 };
	BodyLockMultiWrite lock(*mBodyLockInterface, constraint_bodies, 2);

	Body *body1 = lock.GetBody(0);
	Body *body2 = lock.GetBody(1);

	JPH_ASSERT(body1 != body2);
	JPH_ASSERT(body1 != nullptr || body2 != nullptr);

	return inSettings->Create(body1 != nullptr? *body1 : Body::sFixedToWorld, body2 != nullptr? *body2 : Body::sFixedToWorld);
}

void BodyInterface::ActivateConstraint(const TwoBodyConstraint *inConstraint)
{
	BodyID bodies[] = { inConstraint->GetBody1()->GetID(), inConstraint->GetBody2()->GetID() };
	ActivateBodies(bodies, 2);
}

RefConst<Shape> BodyInterface::GetShape(const BodyID &inBodyID) const
{
	RefConst<Shape> shape;
	BodyLockRead lock(*mBodyLockInterface, inBodyID);
	if (lock.Succeeded())
		shape = lock.GetBody().GetShape();
	return shape;
}

void BodyInterface::SetShape(const BodyID &inBodyID, const Shape *inShape, bool inUpdateMassProperties, EActivation inActivationMode) const
{
	BodyLockWrite lock(*mBodyLockInterface, inBodyID);
	if (lock.Succeeded())
	{
		Body &body = lock.GetBody();

		// Check if shape actually changed
		if (body.GetShape() != inShape)
		{
			// Update the shape
			body.SetShapeInternal(inShape, inUpdateMassProperties);

			// Flag collision cache invalid for this body
			mBodyManager->InvalidateContactCacheForBody(body);

			// Notify broadphase of change
			if (body.IsInBroadPhase())
			{
				BodyID id = body.GetID();
				mBroadPhase->NotifyBodiesAABBChanged(&id, 1);
			}

			// Optionally activate body
			if (inActivationMode == EActivation::Activate && !body.IsStatic())
				ActivateBodyInternal(body);
		}
	}
}

void BodyInterface::NotifyShapeChanged(const BodyID &inBodyID, Vec3Arg inPreviousCenterOfMass, bool inUpdateMassProperties, EActivation inActivationMode) const
{
	BodyLockWrite lock(*mBodyLockInterface, inBodyID);
	if (lock.Succeeded())
	{
		Body &body = lock.GetBody();

		// Update center of mass, mass and inertia
		body.UpdateCenterOfMassInternal(inPreviousCenterOfMass, inUpdateMassProperties);

		// Recalculate bounding box
		body.CalculateWorldSpaceBoundsInternal();

		// Flag collision cache invalid for this body
		mBodyManager->InvalidateContactCacheForBody(body);

		// Notify broadphase of change
		if (body.IsInBroadPhase())
		{
			BodyID id = body.GetID();
			mBroadPhase->NotifyBodiesAABBChanged(&id, 1);
		}

		// Optionally activate body
		if (inActivationMode == EActivation::Activate && !body.IsStatic())
			ActivateBodyInternal(body);
	}
}

void BodyInterface::SetObjectLayer(const BodyID &inBodyID, ObjectLayer inLayer)
{
	BodyLockWrite lock(*mBodyLockInterface, inBodyID);
	if (lock.Succeeded())
	{
		Body &body = lock.GetBody();

		// Check if layer actually changed, updating the broadphase is rather expensive
		if (body.GetObjectLayer() != inLayer)
		{
			// Update the layer on the body
			mBodyManager->SetBodyObjectLayerInternal(body, inLayer);

			// Notify broadphase of change
			if (body.IsInBroadPhase())
			{
				BodyID id = body.GetID();
				mBroadPhase->NotifyBodiesLayerChanged(&id, 1);
			}
		}
	}
}

ObjectLayer BodyInterface::GetObjectLayer(const BodyID &inBodyID) const
{
	BodyLockRead lock(*mBodyLockInterface, inBodyID);
	if (lock.Succeeded())
		return lock.GetBody().GetObjectLayer();
	else
		return cObjectLayerInvalid;
}

void BodyInterface::SetPositionAndRotation(const BodyID &inBodyID, RVec3Arg inPosition, QuatArg inRotation, EActivation inActivationMode)
{
	BodyLockWrite lock(*mBodyLockInterface, inBodyID);
	if (lock.Succeeded())
	{
		Body &body = lock.GetBody();

		// Update the position
		body.SetPositionAndRotationInternal(inPosition, inRotation);

		// Notify broadphase of change
		if (body.IsInBroadPhase())
		{
			BodyID id = body.GetID();
			mBroadPhase->NotifyBodiesAABBChanged(&id, 1);
		}

		// Optionally activate body
		if (inActivationMode == EActivation::Activate && !body.IsStatic())
			ActivateBodyInternal(body);
	}
}

void BodyInterface::SetPositionAndRotationWhenChanged(const BodyID &inBodyID, RVec3Arg inPosition, QuatArg inRotation, EActivation inActivationMode)
{
	BodyLockWrite lock(*mBodyLockInterface, inBodyID);
	if (lock.Succeeded())
	{
		Body &body = lock.GetBody();

		// Check if there is enough change
		if (!body.GetPosition().IsClose(inPosition)
			|| !body.GetRotation().IsClose(inRotation))
		{
			// Update the position
			body.SetPositionAndRotationInternal(inPosition, inRotation);

			// Notify broadphase of change
			if (body.IsInBroadPhase())
			{
				BodyID id = body.GetID();
				mBroadPhase->NotifyBodiesAABBChanged(&id, 1);
			}

			// Optionally activate body
			if (inActivationMode == EActivation::Activate && !body.IsStatic())
				ActivateBodyInternal(body);
		}
	}
}

void BodyInterface::GetPositionAndRotation(const BodyID &inBodyID, RVec3 &outPosition, Quat &outRotation) const
{
	BodyLockRead lock(*mBodyLockInterface, inBodyID);
	if (lock.Succeeded())
	{
		const Body &body = lock.GetBody();
		outPosition = body.GetPosition();
		outRotation = body.GetRotation();
	}
	else
	{
		outPosition = RVec3::sZero();
		outRotation = Quat::sIdentity();
	}
}

void BodyInterface::SetPosition(const BodyID &inBodyID, RVec3Arg inPosition, EActivation inActivationMode)
{
	BodyLockWrite lock(*mBodyLockInterface, inBodyID);
	if (lock.Succeeded())
	{
		Body &body = lock.GetBody();

		// Update the position
		body.SetPositionAndRotationInternal(inPosition, body.GetRotation());

		// Notify broadphase of change
		if (body.IsInBroadPhase())
		{
			BodyID id = body.GetID();
			mBroadPhase->NotifyBodiesAABBChanged(&id, 1);
		}

		// Optionally activate body
		if (inActivationMode == EActivation::Activate && !body.IsStatic())
			ActivateBodyInternal(body);
	}
}

RVec3 BodyInterface::GetPosition(const BodyID &inBodyID) const
{
	BodyLockRead lock(*mBodyLockInterface, inBodyID);
	if (lock.Succeeded())
		return lock.GetBody().GetPosition();
	else
		return RVec3::sZero();
}

RVec3 BodyInterface::GetCenterOfMassPosition(const BodyID &inBodyID) const
{
	BodyLockRead lock(*mBodyLockInterface, inBodyID);
	if (lock.Succeeded())
		return lock.GetBody().GetCenterOfMassPosition();
	else
		return RVec3::sZero();
}

void BodyInterface::SetRotation(const BodyID &inBodyID, QuatArg inRotation, EActivation inActivationMode)
{
	BodyLockWrite lock(*mBodyLockInterface, inBodyID);
	if (lock.Succeeded())
	{
		Body &body = lock.GetBody();

		// Update the position
		body.SetPositionAndRotationInternal(body.GetPosition(), inRotation);

		// Notify broadphase of change
		if (body.IsInBroadPhase())
		{
			BodyID id = body.GetID();
			mBroadPhase->NotifyBodiesAABBChanged(&id, 1);
		}

		// Optionally activate body
		if (inActivationMode == EActivation::Activate && !body.IsStatic())
			ActivateBodyInternal(body);
	}
}

Quat BodyInterface::GetRotation(const BodyID &inBodyID) const
{
	BodyLockRead lock(*mBodyLockInterface, inBodyID);
	if (lock.Succeeded())
		return lock.GetBody().GetRotation();
	else
		return Quat::sIdentity();
}

RMat44 BodyInterface::GetWorldTransform(const BodyID &inBodyID) const
{
	BodyLockRead lock(*mBodyLockInterface, inBodyID);
	if (lock.Succeeded())
		return lock.GetBody().GetWorldTransform();
	else
		return RMat44::sIdentity();
}

RMat44 BodyInterface::GetCenterOfMassTransform(const BodyID &inBodyID) const
{
	BodyLockRead lock(*mBodyLockInterface, inBodyID);
	if (lock.Succeeded())
		return lock.GetBody().GetCenterOfMassTransform();
	else
		return RMat44::sIdentity();
}

void BodyInterface::MoveKinematic(const BodyID &inBodyID, RVec3Arg inTargetPosition, QuatArg inTargetRotation, float inDeltaTime)
{
	BodyLockWrite lock(*mBodyLockInterface, inBodyID);
	if (lock.Succeeded())
	{
		Body &body = lock.GetBody();

		body.MoveKinematic(inTargetPosition, inTargetRotation, inDeltaTime);

		if (!body.IsActive() && (!body.GetLinearVelocity().IsNearZero() || !body.GetAngularVelocity().IsNearZero()))
			mBodyManager->ActivateBodies(&inBodyID, 1);
	}
}

void BodyInterface::SetLinearAndAngularVelocity(const BodyID &inBodyID, Vec3Arg inLinearVelocity, Vec3Arg inAngularVelocity)
{
	BodyLockWrite lock(*mBodyLockInterface, inBodyID);
	if (lock.Succeeded())
	{
		Body &body = lock.GetBody();
		if (!body.IsStatic())
		{
			body.SetLinearVelocityClamped(inLinearVelocity);
			body.SetAngularVelocityClamped(inAngularVelocity);

			if (!body.IsActive() && (!inLinearVelocity.IsNearZero() || !inAngularVelocity.IsNearZero()))
				mBodyManager->ActivateBodies(&inBodyID, 1);
		}
	}
}

void BodyInterface::GetLinearAndAngularVelocity(const BodyID &inBodyID, Vec3 &outLinearVelocity, Vec3 &outAngularVelocity) const
{
	BodyLockRead lock(*mBodyLockInterface, inBodyID);
	if (lock.Succeeded())
	{
		const Body &body = lock.GetBody();
		if (!body.IsStatic())
		{
			outLinearVelocity = body.GetLinearVelocity();
			outAngularVelocity = body.GetAngularVelocity();
			return;
		}
	}

	outLinearVelocity = outAngularVelocity = Vec3::sZero();
}

void BodyInterface::SetLinearVelocity(const BodyID &inBodyID, Vec3Arg inLinearVelocity)
{
	BodyLockWrite lock(*mBodyLockInterface, inBodyID);
	if (lock.Succeeded())
	{
		Body &body = lock.GetBody();
		if (!body.IsStatic())
		{
			body.SetLinearVelocityClamped(inLinearVelocity);

			if (!body.IsActive() && !inLinearVelocity.IsNearZero())
				mBodyManager->ActivateBodies(&inBodyID, 1);
		}
	}
}

Vec3 BodyInterface::GetLinearVelocity(const BodyID &inBodyID) const
{
	BodyLockRead lock(*mBodyLockInterface, inBodyID);
	if (lock.Succeeded())
	{
		const Body &body = lock.GetBody();
		if (!body.IsStatic())
			return body.GetLinearVelocity();
	}

	return Vec3::sZero();
}

void BodyInterface::AddLinearVelocity(const BodyID &inBodyID, Vec3Arg inLinearVelocity)
{
	BodyLockWrite lock(*mBodyLockInterface, inBodyID);
	if (lock.Succeeded())
	{
		Body &body = lock.GetBody();
		if (!body.IsStatic())
		{
			body.SetLinearVelocityClamped(body.GetLinearVelocity() + inLinearVelocity);

			if (!body.IsActive() && !body.GetLinearVelocity().IsNearZero())
				mBodyManager->ActivateBodies(&inBodyID, 1);
		}
	}
}

void BodyInterface::AddLinearAndAngularVelocity(const BodyID &inBodyID, Vec3Arg inLinearVelocity, Vec3Arg inAngularVelocity)
{
	BodyLockWrite lock(*mBodyLockInterface, inBodyID);
	if (lock.Succeeded())
	{
		Body &body = lock.GetBody();
		if (!body.IsStatic())
		{
			body.SetLinearVelocityClamped(body.GetLinearVelocity() + inLinearVelocity);
			body.SetAngularVelocityClamped(body.GetAngularVelocity() + inAngularVelocity);

			if (!body.IsActive() && (!body.GetLinearVelocity().IsNearZero() || !body.GetAngularVelocity().IsNearZero()))
				mBodyManager->ActivateBodies(&inBodyID, 1);
		}
	}
}

void BodyInterface::SetAngularVelocity(const BodyID &inBodyID, Vec3Arg inAngularVelocity)
{
	BodyLockWrite lock(*mBodyLockInterface, inBodyID);
	if (lock.Succeeded())
	{
		Body &body = lock.GetBody();
		if (!body.IsStatic())
		{
			body.SetAngularVelocityClamped(inAngularVelocity);

			if (!body.IsActive() && !inAngularVelocity.IsNearZero())
				mBodyManager->ActivateBodies(&inBodyID, 1);
		}
	}
}

Vec3 BodyInterface::GetAngularVelocity(const BodyID &inBodyID) const
{
	BodyLockRead lock(*mBodyLockInterface, inBodyID);
	if (lock.Succeeded())
	{
		const Body &body = lock.GetBody();
		if (!body.IsStatic())
			return body.GetAngularVelocity();
	}

	return Vec3::sZero();
}

Vec3 BodyInterface::GetPointVelocity(const BodyID &inBodyID, RVec3Arg inPoint) const
{
	BodyLockRead lock(*mBodyLockInterface, inBodyID);
	if (lock.Succeeded())
	{
		const Body &body = lock.GetBody();
		if (!body.IsStatic())
			return body.GetPointVelocity(inPoint);
	}

	return Vec3::sZero();
}

void BodyInterface::AddForce(const BodyID &inBodyID, Vec3Arg inForce, EActivation inActivationMode)
{
	BodyLockWrite lock(*mBodyLockInterface, inBodyID);
	if (lock.Succeeded())
	{
		Body &body = lock.GetBody();
		if (body.IsDynamic() && (inActivationMode == EActivation::Activate || body.IsActive()))
		{
			body.AddForce(inForce);

			if (inActivationMode == EActivation::Activate)
				ActivateBodyInternal(body);
		}
	}
}

void BodyInterface::AddForce(const BodyID &inBodyID, Vec3Arg inForce, RVec3Arg inPoint, EActivation inActivationMode)
{
	BodyLockWrite lock(*mBodyLockInterface, inBodyID);
	if (lock.Succeeded())
	{
		Body &body = lock.GetBody();
		if (body.IsDynamic() && (inActivationMode == EActivation::Activate || body.IsActive()))
		{
			body.AddForce(inForce, inPoint);

			if (inActivationMode == EActivation::Activate)
				ActivateBodyInternal(body);
		}
	}
}

void BodyInterface::AddTorque(const BodyID &inBodyID, Vec3Arg inTorque, EActivation inActivationMode)
{
	BodyLockWrite lock(*mBodyLockInterface, inBodyID);
	if (lock.Succeeded())
	{
		Body &body = lock.GetBody();
		if (body.IsDynamic() && (inActivationMode == EActivation::Activate || body.IsActive()))
		{
			body.AddTorque(inTorque);

			if (inActivationMode == EActivation::Activate)
				ActivateBodyInternal(body);
		}
	}
}

void BodyInterface::AddForceAndTorque(const BodyID &inBodyID, Vec3Arg inForce, Vec3Arg inTorque, EActivation inActivationMode)
{
	BodyLockWrite lock(*mBodyLockInterface, inBodyID);
	if (lock.Succeeded())
	{
		Body &body = lock.GetBody();
		if (body.IsDynamic() && (inActivationMode == EActivation::Activate || body.IsActive()))
		{
			body.AddForce(inForce);
			body.AddTorque(inTorque);

			if (inActivationMode == EActivation::Activate)
				ActivateBodyInternal(body);
		}
	}
}

void BodyInterface::AddImpulse(const BodyID &inBodyID, Vec3Arg inImpulse)
{
	BodyLockWrite lock(*mBodyLockInterface, inBodyID);
	if (lock.Succeeded())
	{
		Body &body = lock.GetBody();
		if (body.IsDynamic())
		{
			body.AddImpulse(inImpulse);

			if (!body.IsActive())
				mBodyManager->ActivateBodies(&inBodyID, 1);
		}
	}
}

void BodyInterface::AddImpulse(const BodyID &inBodyID, Vec3Arg inImpulse, RVec3Arg inPoint)
{
	BodyLockWrite lock(*mBodyLockInterface, inBodyID);
	if (lock.Succeeded())
	{
		Body &body = lock.GetBody();
		if (body.IsDynamic())
		{
			body.AddImpulse(inImpulse, inPoint);

			if (!body.IsActive())
				mBodyManager->ActivateBodies(&inBodyID, 1);
		}
	}
}

void BodyInterface::AddAngularImpulse(const BodyID &inBodyID, Vec3Arg inAngularImpulse)
{
	BodyLockWrite lock(*mBodyLockInterface, inBodyID);
	if (lock.Succeeded())
	{
		Body &body = lock.GetBody();
		if (body.IsDynamic())
		{
			body.AddAngularImpulse(inAngularImpulse);

			if (!body.IsActive())
				mBodyManager->ActivateBodies(&inBodyID, 1);
		}
	}
}

bool BodyInterface::ApplyBuoyancyImpulse(const BodyID &inBodyID, RVec3Arg inSurfacePosition, Vec3Arg inSurfaceNormal, float inBuoyancy, float inLinearDrag, float inAngularDrag, Vec3Arg inFluidVelocity, Vec3Arg inGravity, float inDeltaTime)
{
	BodyLockWrite lock(*mBodyLockInterface, inBodyID);
	if (lock.Succeeded())
	{
		Body &body = lock.GetBody();
		if (body.IsDynamic()
			&& body.ApplyBuoyancyImpulse(inSurfacePosition, inSurfaceNormal, inBuoyancy, inLinearDrag, inAngularDrag, inFluidVelocity, inGravity, inDeltaTime))
		{
			ActivateBodyInternal(body);
			return true;
		}
	}

	return false;
}

void BodyInterface::SetPositionRotationAndVelocity(const BodyID &inBodyID, RVec3Arg inPosition, QuatArg inRotation, Vec3Arg inLinearVelocity, Vec3Arg inAngularVelocity)
{
	BodyLockWrite lock(*mBodyLockInterface, inBodyID);
	if (lock.Succeeded())
	{
		Body &body = lock.GetBody();

		// Update the position
		body.SetPositionAndRotationInternal(inPosition, inRotation);

		// Notify broadphase of change
		if (body.IsInBroadPhase())
		{
			BodyID id = body.GetID();
			mBroadPhase->NotifyBodiesAABBChanged(&id, 1);
		}

		if (!body.IsStatic())
		{
			body.SetLinearVelocityClamped(inLinearVelocity);
			body.SetAngularVelocityClamped(inAngularVelocity);

			// Optionally activate body
			if (!body.IsActive() && (!inLinearVelocity.IsNearZero() || !inAngularVelocity.IsNearZero()))
				mBodyManager->ActivateBodies(&inBodyID, 1);
		}
	}
}

void BodyInterface::SetMotionType(const BodyID &inBodyID, EMotionType inMotionType, EActivation inActivationMode)
{
	BodyLockWrite lock(*mBodyLockInterface, inBodyID);
	if (lock.Succeeded())
	{
		Body &body = lock.GetBody();

		// Deactivate if we're making the body static
		if (body.IsActive() && inMotionType == EMotionType::Static)
			mBodyManager->DeactivateBodies(&inBodyID, 1);

		body.SetMotionType(inMotionType);

		// Activate body if requested
		if (inMotionType != EMotionType::Static && inActivationMode == EActivation::Activate)
			ActivateBodyInternal(body);
	}
}

EBodyType BodyInterface::GetBodyType(const BodyID &inBodyID) const
{
	BodyLockRead lock(*mBodyLockInterface, inBodyID);
	if (lock.Succeeded())
		return lock.GetBody().GetBodyType();
	else
		return EBodyType::RigidBody;
}

EMotionType BodyInterface::GetMotionType(const BodyID &inBodyID) const
{
	BodyLockRead lock(*mBodyLockInterface, inBodyID);
	if (lock.Succeeded())
		return lock.GetBody().GetMotionType();
	else
		return EMotionType::Static;
}

void BodyInterface::SetMotionQuality(const BodyID &inBodyID, EMotionQuality inMotionQuality)
{
	BodyLockWrite lock(*mBodyLockInterface, inBodyID);
	if (lock.Succeeded())
		mBodyManager->SetMotionQuality(lock.GetBody(), inMotionQuality);
}

EMotionQuality BodyInterface::GetMotionQuality(const BodyID &inBodyID) const
{
	BodyLockRead lock(*mBodyLockInterface, inBodyID);
	if (lock.Succeeded() && !lock.GetBody().IsStatic())
		return lock.GetBody().GetMotionProperties()->GetMotionQuality();
	else
		return EMotionQuality::Discrete;
}

Mat44 BodyInterface::GetInverseInertia(const BodyID &inBodyID) const
{
	BodyLockRead lock(*mBodyLockInterface, inBodyID);
	if (lock.Succeeded())
		return lock.GetBody().GetInverseInertia();
	else
		return Mat44::sIdentity();
}

void BodyInterface::SetRestitution(const BodyID &inBodyID, float inRestitution)
{
	BodyLockWrite lock(*mBodyLockInterface, inBodyID);
	if (lock.Succeeded())
		lock.GetBody().SetRestitution(inRestitution);
}

float BodyInterface::GetRestitution(const BodyID &inBodyID) const
{
	BodyLockRead lock(*mBodyLockInterface, inBodyID);
	if (lock.Succeeded())
		return lock.GetBody().GetRestitution();
	else
		return 0.0f;
}

void BodyInterface::SetFriction(const BodyID &inBodyID, float inFriction)
{
	BodyLockWrite lock(*mBodyLockInterface, inBodyID);
	if (lock.Succeeded())
		lock.GetBody().SetFriction(inFriction);
}

float BodyInterface::GetFriction(const BodyID &inBodyID) const
{
	BodyLockRead lock(*mBodyLockInterface, inBodyID);
	if (lock.Succeeded())
		return lock.GetBody().GetFriction();
	else
		return 0.0f;
}

void BodyInterface::SetGravityFactor(const BodyID &inBodyID, float inGravityFactor)
{
	BodyLockWrite lock(*mBodyLockInterface, inBodyID);
	if (lock.Succeeded() && lock.GetBody().GetMotionPropertiesUnchecked() != nullptr)
		lock.GetBody().GetMotionPropertiesUnchecked()->SetGravityFactor(inGravityFactor);
}

float BodyInterface::GetGravityFactor(const BodyID &inBodyID) const
{
	BodyLockRead lock(*mBodyLockInterface, inBodyID);
	if (lock.Succeeded() && lock.GetBody().GetMotionPropertiesUnchecked() != nullptr)
		return lock.GetBody().GetMotionPropertiesUnchecked()->GetGravityFactor();
	else
		return 1.0f;
}

void BodyInterface::SetUseManifoldReduction(const BodyID &inBodyID, bool inUseReduction)
{
	BodyLockWrite lock(*mBodyLockInterface, inBodyID);
	if (lock.Succeeded())
	{
		Body &body = lock.GetBody();
		if (body.GetUseManifoldReduction() != inUseReduction)
		{
			body.SetUseManifoldReduction(inUseReduction);

			// Flag collision cache invalid for this body
			mBodyManager->InvalidateContactCacheForBody(body);
		}
	}
}

bool BodyInterface::GetUseManifoldReduction(const BodyID &inBodyID) const
{
	BodyLockRead lock(*mBodyLockInterface, inBodyID);
	if (lock.Succeeded())
		return lock.GetBody().GetUseManifoldReduction();
	else
		return true;
}

TransformedShape BodyInterface::GetTransformedShape(const BodyID &inBodyID) const
{
	BodyLockRead lock(*mBodyLockInterface, inBodyID);
	if (lock.Succeeded())
		return lock.GetBody().GetTransformedShape();
	else
		return TransformedShape();
}

uint64 BodyInterface::GetUserData(const BodyID &inBodyID) const
{
	BodyLockRead lock(*mBodyLockInterface, inBodyID);
	if (lock.Succeeded())
		return lock.GetBody().GetUserData();
	else
		return 0;
}

void BodyInterface::SetUserData(const BodyID &inBodyID, uint64 inUserData) const
{
	BodyLockWrite lock(*mBodyLockInterface, inBodyID);
	if (lock.Succeeded())
		lock.GetBody().SetUserData(inUserData);
}

const PhysicsMaterial *BodyInterface::GetMaterial(const BodyID &inBodyID, const SubShapeID &inSubShapeID) const
{
	BodyLockRead lock(*mBodyLockInterface, inBodyID);
	if (lock.Succeeded())
		return lock.GetBody().GetShape()->GetMaterial(inSubShapeID);
	else
		return PhysicsMaterial::sDefault;
}

void BodyInterface::InvalidateContactCache(const BodyID &inBodyID)
{
	BodyLockWrite lock(*mBodyLockInterface, inBodyID);
	if (lock.Succeeded())
		mBodyManager->InvalidateContactCacheForBody(lock.GetBody());
}

JPH_NAMESPACE_END

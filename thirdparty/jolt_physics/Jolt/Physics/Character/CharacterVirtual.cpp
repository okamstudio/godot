// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#include <Jolt/Jolt.h>

#include <Jolt/Physics/Character/CharacterVirtual.h>
#include <Jolt/Physics/Body/Body.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Collision/ShapeCast.h>
#include <Jolt/Physics/Collision/CollideShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>
#include <Jolt/Physics/Collision/Shape/ScaledShape.h>
#include <Jolt/Physics/Collision/CollisionDispatch.h>
#include <Jolt/Core/QuickSort.h>
#include <Jolt/Geometry/ConvexSupport.h>
#include <Jolt/Geometry/GJKClosestPoint.h>
#ifdef JPH_DEBUG_RENDERER
	#include <Jolt/Renderer/DebugRenderer.h>
#endif // JPH_DEBUG_RENDERER

JPH_NAMESPACE_BEGIN

void CharacterVsCharacterCollisionSimple::Remove(const CharacterVirtual *inCharacter)
{
	Array<CharacterVirtual *>::iterator i = std::find(mCharacters.begin(), mCharacters.end(), inCharacter);
	if (i != mCharacters.end())
		mCharacters.erase(i);
}

void CharacterVsCharacterCollisionSimple::CollideCharacter(const CharacterVirtual *inCharacter, RMat44Arg inCenterOfMassTransform, const CollideShapeSettings &inCollideShapeSettings, RVec3Arg inBaseOffset, CollideShapeCollector &ioCollector) const
{
	// Make shape 1 relative to inBaseOffset
	Mat44 transform1 = inCenterOfMassTransform.PostTranslated(-inBaseOffset).ToMat44();

	const Shape *shape = inCharacter->GetShape();
	CollideShapeSettings settings = inCollideShapeSettings;

	// Iterate over all characters
	for (const CharacterVirtual *c : mCharacters)
		if (c != inCharacter
			&& !ioCollector.ShouldEarlyOut())
		{
			// Collector needs to know which character we're colliding with
			ioCollector.SetUserData(reinterpret_cast<uint64>(c));

			// Make shape 2 relative to inBaseOffset
			Mat44 transform2 = c->GetCenterOfMassTransform().PostTranslated(-inBaseOffset).ToMat44();

			// We need to add the padding of character 2 so that we will detect collision with its outer shell
			settings.mMaxSeparationDistance = inCollideShapeSettings.mMaxSeparationDistance + c->GetCharacterPadding();

			// Note that this collides against the character's shape without padding, this will be corrected for in CharacterVirtual::GetContactsAtPosition
			CollisionDispatch::sCollideShapeVsShape(shape, c->GetShape(), Vec3::sReplicate(1.0f), Vec3::sReplicate(1.0f), transform1, transform2, SubShapeIDCreator(), SubShapeIDCreator(), settings, ioCollector);
		}

	// Reset the user data
	ioCollector.SetUserData(0);
}

void CharacterVsCharacterCollisionSimple::CastCharacter(const CharacterVirtual *inCharacter, RMat44Arg inCenterOfMassTransform, Vec3Arg inDirection, const ShapeCastSettings &inShapeCastSettings, RVec3Arg inBaseOffset, CastShapeCollector &ioCollector) const
{
	// Convert shape cast relative to inBaseOffset
	Mat44 transform1 = inCenterOfMassTransform.PostTranslated(-inBaseOffset).ToMat44();
	ShapeCast shape_cast(inCharacter->GetShape(), Vec3::sReplicate(1.0f), transform1, inDirection);

	// Iterate over all characters
	for (const CharacterVirtual *c : mCharacters)
		if (c != inCharacter
			&& !ioCollector.ShouldEarlyOut())
		{
			// Collector needs to know which character we're colliding with
			ioCollector.SetUserData(reinterpret_cast<uint64>(c));

			// Make shape 2 relative to inBaseOffset
			Mat44 transform2 = c->GetCenterOfMassTransform().PostTranslated(-inBaseOffset).ToMat44();

			// Note that this collides against the character's shape without padding, this will be corrected for in CharacterVirtual::GetFirstContactForSweep
			CollisionDispatch::sCastShapeVsShapeWorldSpace(shape_cast, inShapeCastSettings, c->GetShape(), Vec3::sReplicate(1.0f), { }, transform2, SubShapeIDCreator(), SubShapeIDCreator(), ioCollector);
		}

	// Reset the user data
	ioCollector.SetUserData(0);
}

CharacterVirtual::CharacterVirtual(const CharacterVirtualSettings *inSettings, RVec3Arg inPosition, QuatArg inRotation, uint64 inUserData, PhysicsSystem *inSystem) :
	CharacterBase(inSettings, inSystem),
	mBackFaceMode(inSettings->mBackFaceMode),
	mPredictiveContactDistance(inSettings->mPredictiveContactDistance),
	mMaxCollisionIterations(inSettings->mMaxCollisionIterations),
	mMaxConstraintIterations(inSettings->mMaxConstraintIterations),
	mMinTimeRemaining(inSettings->mMinTimeRemaining),
	mCollisionTolerance(inSettings->mCollisionTolerance),
	mCharacterPadding(inSettings->mCharacterPadding),
	mMaxNumHits(inSettings->mMaxNumHits),
	mHitReductionCosMaxAngle(inSettings->mHitReductionCosMaxAngle),
	mPenetrationRecoverySpeed(inSettings->mPenetrationRecoverySpeed),
	mEnhancedInternalEdgeRemoval(inSettings->mEnhancedInternalEdgeRemoval),
	mShapeOffset(inSettings->mShapeOffset),
	mPosition(inPosition),
	mRotation(inRotation),
	mUserData(inUserData)
{
	// Copy settings
	SetMaxStrength(inSettings->mMaxStrength);
	SetMass(inSettings->mMass);

	// Create an inner rigid body if requested
	if (inSettings->mInnerBodyShape != nullptr)
	{
		BodyCreationSettings settings(inSettings->mInnerBodyShape, GetInnerBodyPosition(), mRotation, EMotionType::Kinematic, inSettings->mInnerBodyLayer);
		settings.mAllowSleeping = false; // Disable sleeping so that we will receive sensor callbacks
		settings.mUserData = inUserData;
		mInnerBodyID = inSystem->GetBodyInterface().CreateAndAddBody(settings, EActivation::Activate);
	}
}

CharacterVirtual::~CharacterVirtual()
{
	if (!mInnerBodyID.IsInvalid())
	{
		mSystem->GetBodyInterface().RemoveBody(mInnerBodyID);
		mSystem->GetBodyInterface().DestroyBody(mInnerBodyID);
	}
}

void CharacterVirtual::UpdateInnerBodyTransform()
{
	if (!mInnerBodyID.IsInvalid())
		mSystem->GetBodyInterface().SetPositionAndRotation(mInnerBodyID, GetInnerBodyPosition(), mRotation, EActivation::DontActivate);
}

void CharacterVirtual::GetAdjustedBodyVelocity(const Body& inBody, Vec3 &outLinearVelocity, Vec3 &outAngularVelocity) const
{
	// Get real velocity of body
	if (!inBody.IsStatic())
	{
		const MotionProperties *mp = inBody.GetMotionPropertiesUnchecked();
		outLinearVelocity = mp->GetLinearVelocity();
		outAngularVelocity = mp->GetAngularVelocity();
	}
	else
	{
		outLinearVelocity = outAngularVelocity = Vec3::sZero();
	}

	// Allow application to override
	if (mListener != nullptr)
		mListener->OnAdjustBodyVelocity(this, inBody, outLinearVelocity, outAngularVelocity);
}

Vec3 CharacterVirtual::CalculateCharacterGroundVelocity(RVec3Arg inCenterOfMass, Vec3Arg inLinearVelocity, Vec3Arg inAngularVelocity, float inDeltaTime) const
{
	// Get angular velocity
	float angular_velocity_len_sq = inAngularVelocity.LengthSq();
	if (angular_velocity_len_sq < 1.0e-12f)
		return inLinearVelocity;
	float angular_velocity_len = sqrt(angular_velocity_len_sq);

	// Calculate the rotation that the object will make in the time step
	Quat rotation = Quat::sRotation(inAngularVelocity / angular_velocity_len, angular_velocity_len * inDeltaTime);

	// Calculate where the new character position will be
	RVec3 new_position = inCenterOfMass + rotation * Vec3(mPosition - inCenterOfMass);

	// Calculate the velocity
	return inLinearVelocity + Vec3(new_position - mPosition) / inDeltaTime;
}

template <class taCollector>
void CharacterVirtual::sFillContactProperties(const CharacterVirtual *inCharacter, Contact &outContact, const Body &inBody, Vec3Arg inUp, RVec3Arg inBaseOffset, const taCollector &inCollector, const CollideShapeResult &inResult)
{
	// Get adjusted body velocity
	Vec3 linear_velocity, angular_velocity;
	inCharacter->GetAdjustedBodyVelocity(inBody, linear_velocity, angular_velocity);

	outContact.mPosition = inBaseOffset + inResult.mContactPointOn2;
	outContact.mLinearVelocity = linear_velocity + angular_velocity.Cross(Vec3(outContact.mPosition - inBody.GetCenterOfMassPosition())); // Calculate point velocity
	outContact.mContactNormal = -inResult.mPenetrationAxis.NormalizedOr(Vec3::sZero());
	outContact.mSurfaceNormal = inCollector.GetContext()->GetWorldSpaceSurfaceNormal(inResult.mSubShapeID2, outContact.mPosition);
	if (outContact.mContactNormal.Dot(outContact.mSurfaceNormal) < 0.0f)
		outContact.mSurfaceNormal = -outContact.mSurfaceNormal; // Flip surface normal if we're hitting a back face
	if (outContact.mContactNormal.Dot(inUp) > outContact.mSurfaceNormal.Dot(inUp))
		outContact.mSurfaceNormal = outContact.mContactNormal; // Replace surface normal with contact normal if the contact normal is pointing more upwards
	outContact.mDistance = -inResult.mPenetrationDepth;
	outContact.mBodyB = inResult.mBodyID2;
	outContact.mSubShapeIDB = inResult.mSubShapeID2;
	outContact.mMotionTypeB = inBody.GetMotionType();
	outContact.mIsSensorB = inBody.IsSensor();
	outContact.mUserData = inBody.GetUserData();
	outContact.mMaterial = inCollector.GetContext()->GetMaterial(inResult.mSubShapeID2);
}

void CharacterVirtual::sFillCharacterContactProperties(Contact &outContact, CharacterVirtual *inOtherCharacter, RVec3Arg inBaseOffset, const CollideShapeResult &inResult)
{
	outContact.mPosition = inBaseOffset + inResult.mContactPointOn2;
	outContact.mLinearVelocity = inOtherCharacter->GetLinearVelocity();
	outContact.mSurfaceNormal = outContact.mContactNormal = -inResult.mPenetrationAxis.NormalizedOr(Vec3::sZero());
	outContact.mDistance = -inResult.mPenetrationDepth;
	outContact.mCharacterB = inOtherCharacter;
	outContact.mSubShapeIDB = inResult.mSubShapeID2;
	outContact.mMotionTypeB = EMotionType::Kinematic; // Other character is kinematic, we can't directly move it
	outContact.mIsSensorB = false;
	outContact.mUserData = inOtherCharacter->GetUserData();
	outContact.mMaterial = PhysicsMaterial::sDefault;
}

void CharacterVirtual::ContactCollector::AddHit(const CollideShapeResult &inResult)
{
	// If we exceed our contact limit, try to clean up near-duplicate contacts
	if (mContacts.size() == mMaxHits)
	{
		// Flag that we hit this code path
		mMaxHitsExceeded = true;

		// Check if we can do reduction
		if (mHitReductionCosMaxAngle > -1.0f)
		{
			// Loop all contacts and find similar contacts
			for (int i = (int)mContacts.size() - 1; i >= 0; --i)
			{
				Contact &contact_i = mContacts[i];
				for (int j = i - 1; j >= 0; --j)
				{
					Contact &contact_j = mContacts[j];
					if (contact_i.IsSameBody(contact_j)
						&& contact_i.mContactNormal.Dot(contact_j.mContactNormal) > mHitReductionCosMaxAngle) // Very similar contact normals
					{
						// Remove the contact with the biggest distance
						bool i_is_last = i == (int)mContacts.size() - 1;
						if (contact_i.mDistance > contact_j.mDistance)
						{
							// Remove i
							if (!i_is_last)
								contact_i = mContacts.back();
							mContacts.pop_back();

							// Break out of the loop, i is now an element that we already processed
							break;
						}
						else
						{
							// Remove j
							contact_j = mContacts.back();
							mContacts.pop_back();

							// If i was the last element, we just moved it into position j. Break out of the loop, we'll see it again later.
							if (i_is_last)
								break;
						}
					}
				}
			}
		}

		if (mContacts.size() == mMaxHits)
		{
			// There are still too many hits, give up!
			ForceEarlyOut();
			return;
		}
	}

	if (inResult.mBodyID2.IsInvalid())
	{
		// Assuming this is a hit against another character
		JPH_ASSERT(mOtherCharacter != nullptr);

		// Create contact with other character
		mContacts.emplace_back();
		Contact &contact = mContacts.back();
		sFillCharacterContactProperties(contact, mOtherCharacter, mBaseOffset, inResult);
		contact.mFraction = 0.0f;
	}
	else
	{
		// Create contact with other body
		BodyLockRead lock(mSystem->GetBodyLockInterface(), inResult.mBodyID2);
		if (lock.SucceededAndIsInBroadPhase())
		{
			mContacts.emplace_back();
			Contact &contact = mContacts.back();
			sFillContactProperties(mCharacter, contact, lock.GetBody(), mUp, mBaseOffset, *this, inResult);
			contact.mFraction = 0.0f;
		}
	}
}

void CharacterVirtual::ContactCastCollector::AddHit(const ShapeCastResult &inResult)
{
	if (inResult.mFraction < mContact.mFraction // Since we're doing checks against the world and against characters, we may get a hit with a higher fraction than the previous hit
		&& inResult.mFraction > 0.0f // Ignore collisions at fraction = 0
		&& inResult.mPenetrationAxis.Dot(mDisplacement) > 0.0f) // Ignore penetrations that we're moving away from
	{
		// Test if this contact should be ignored
		for (const IgnoredContact &c : mIgnoredContacts)
			if (c.mBodyID == inResult.mBodyID2 && c.mSubShapeID == inResult.mSubShapeID2)
				return;

		Contact contact;

		if (inResult.mBodyID2.IsInvalid())
		{
			// Assuming this is a hit against another character
			JPH_ASSERT(mOtherCharacter != nullptr);

			// Create contact with other character
			sFillCharacterContactProperties(contact, mOtherCharacter, mBaseOffset, inResult);
		}
		else
		{
			// Lock body only while we fetch contact properties
			BodyLockRead lock(mSystem->GetBodyLockInterface(), inResult.mBodyID2);
			if (!lock.SucceededAndIsInBroadPhase())
				return;

			// Sweeps don't result in OnContactAdded callbacks so we can ignore sensors here
			const Body &body = lock.GetBody();
			if (body.IsSensor())
				return;

			// Convert the hit result into a contact
			sFillContactProperties(mCharacter, contact, body, mUp, mBaseOffset, *this, inResult);
		}

		contact.mFraction = inResult.mFraction;

		// Check if the contact that will make us penetrate more than the allowed tolerance
		if (contact.mDistance + contact.mContactNormal.Dot(mDisplacement) < -mCharacter->mCollisionTolerance
			&& mCharacter->ValidateContact(contact))
		{
			mContact = contact;
			UpdateEarlyOutFraction(contact.mFraction);
		}
	}
}

void CharacterVirtual::CheckCollision(RVec3Arg inPosition, QuatArg inRotation, Vec3Arg inMovementDirection, float inMaxSeparationDistance, const Shape *inShape, RVec3Arg inBaseOffset, CollideShapeCollector &ioCollector, const BroadPhaseLayerFilter &inBroadPhaseLayerFilter, const ObjectLayerFilter &inObjectLayerFilter, const BodyFilter &inBodyFilter, const ShapeFilter &inShapeFilter) const
{
	// Query shape transform
	RMat44 transform = GetCenterOfMassTransform(inPosition, inRotation, inShape);

	// Settings for collide shape
	CollideShapeSettings settings;
	settings.mBackFaceMode = mBackFaceMode;
	settings.mActiveEdgeMovementDirection = inMovementDirection;
	settings.mMaxSeparationDistance = mCharacterPadding + inMaxSeparationDistance;
	settings.mActiveEdgeMode = EActiveEdgeMode::CollideOnlyWithActive;

	// Body filter
	IgnoreSingleBodyFilterChained body_filter(mInnerBodyID, inBodyFilter);

	// Select the right function
	auto collide_shape_function = mEnhancedInternalEdgeRemoval? &NarrowPhaseQuery::CollideShapeWithInternalEdgeRemoval : &NarrowPhaseQuery::CollideShape;

	// Collide shape
	(mSystem->GetNarrowPhaseQuery().*collide_shape_function)(inShape, Vec3::sReplicate(1.0f), transform, settings, inBaseOffset, ioCollector, inBroadPhaseLayerFilter, inObjectLayerFilter, body_filter, inShapeFilter);

	// Also collide with other characters
	if (mCharacterVsCharacterCollision != nullptr)
	{
		ioCollector.SetContext(nullptr); // We're no longer colliding with a transformed shape, reset
		mCharacterVsCharacterCollision->CollideCharacter(this, transform, settings, inBaseOffset, ioCollector);
	}
}

void CharacterVirtual::GetContactsAtPosition(RVec3Arg inPosition, Vec3Arg inMovementDirection, const Shape *inShape, TempContactList &outContacts, const BroadPhaseLayerFilter &inBroadPhaseLayerFilter, const ObjectLayerFilter &inObjectLayerFilter, const BodyFilter &inBodyFilter, const ShapeFilter &inShapeFilter) const
{
	// Remove previous results
	outContacts.clear();

	// Body filter
	IgnoreSingleBodyFilterChained body_filter(mInnerBodyID, inBodyFilter);

	// Collide shape
	ContactCollector collector(mSystem, this, mMaxNumHits, mHitReductionCosMaxAngle, mUp, mPosition, outContacts);
	CheckCollision(inPosition, mRotation, inMovementDirection, mPredictiveContactDistance, inShape, mPosition, collector, inBroadPhaseLayerFilter, inObjectLayerFilter, body_filter, inShapeFilter);

	// The broadphase bounding boxes will not be deterministic, which means that the order in which the contacts are received by the collector is not deterministic.
	// Therefore we need to sort the contacts to preserve determinism. Note that currently this will fail if we exceed mMaxNumHits hits.
	QuickSort(outContacts.begin(), outContacts.end(), ContactOrderingPredicate());

	// Flag if we exceeded the max number of hits
	mMaxHitsExceeded = collector.mMaxHitsExceeded;

	// Reduce distance to contact by padding to ensure we stay away from the object by a little margin
	// (this will make collision detection cheaper - especially for sweep tests as they won't hit the surface if we're properly sliding)
	for (Contact &c : outContacts)
	{
		c.mDistance -= mCharacterPadding;

		if (c.mCharacterB != nullptr)
			c.mDistance -= c.mCharacterB->mCharacterPadding;
	}
}

void CharacterVirtual::RemoveConflictingContacts(TempContactList &ioContacts, IgnoredContactList &outIgnoredContacts) const
{
	// Only use this algorithm if we're penetrating further than this (due to numerical precision issues we can always penetrate a little bit and we don't want to discard contacts if they just have a tiny penetration)
	// We do need to account for padding (see GetContactsAtPosition) that is removed from the contact distances, to compensate we add it to the cMinRequiredPenetration
	const float cMinRequiredPenetration = 1.25f * mCharacterPadding;

	// Discard conflicting penetrating contacts
	for (size_t c1 = 0; c1 < ioContacts.size(); c1++)
	{
		Contact &contact1 = ioContacts[c1];
		if (contact1.mDistance <= -cMinRequiredPenetration) // Only for penetrations
			for (size_t c2 = c1 + 1; c2 < ioContacts.size(); c2++)
			{
				Contact &contact2 = ioContacts[c2];
				if (contact1.IsSameBody(contact2)
					&& contact2.mDistance <= -cMinRequiredPenetration // Only for penetrations
					&& contact1.mContactNormal.Dot(contact2.mContactNormal) < 0.0f) // Only opposing normals
				{
					// Discard contacts with the least amount of penetration
					if (contact1.mDistance < contact2.mDistance)
					{
						// Discard the 2nd contact
						outIgnoredContacts.emplace_back(contact2.mBodyB, contact2.mSubShapeIDB);
						ioContacts.erase(ioContacts.begin() + c2);
						c2--;
					}
					else
					{
						// Discard the first contact
						outIgnoredContacts.emplace_back(contact1.mBodyB, contact1.mSubShapeIDB);
						ioContacts.erase(ioContacts.begin() + c1);
						c1--;
						break;
					}
				}
			}
	}
}

bool CharacterVirtual::ValidateContact(const Contact &inContact) const
{
	if (mListener == nullptr)
		return true;

	if (inContact.mCharacterB != nullptr)
		return mListener->OnCharacterContactValidate(this, inContact.mCharacterB, inContact.mSubShapeIDB);
	else
		return mListener->OnContactValidate(this, inContact.mBodyB, inContact.mSubShapeIDB);
}

void CharacterVirtual::ContactAdded(const Contact &inContact, CharacterContactSettings &ioSettings) const
{
	if (mListener != nullptr)
	{
		if (inContact.mCharacterB != nullptr)
			mListener->OnCharacterContactAdded(this, inContact.mCharacterB, inContact.mSubShapeIDB, inContact.mPosition, -inContact.mContactNormal, ioSettings);
		else
			mListener->OnContactAdded(this, inContact.mBodyB, inContact.mSubShapeIDB, inContact.mPosition, -inContact.mContactNormal, ioSettings);
	}
}

template <class T>
inline static bool sCorrectFractionForCharacterPadding(const Shape *inShape, Mat44Arg inStart, Vec3Arg inDisplacement, Vec3Arg inScale, const T &inPolygon, float &ioFraction)
{
	if (inShape->GetType() == EShapeType::Convex)
	{
		// Get the support function for the shape we're casting
		const ConvexShape *convex_shape = static_cast<const ConvexShape *>(inShape);
		ConvexShape::SupportBuffer buffer;
		const ConvexShape::Support *support = convex_shape->GetSupportFunction(ConvexShape::ESupportMode::IncludeConvexRadius, buffer, inScale);

		// Cast the shape against the polygon
		GJKClosestPoint gjk;
		return gjk.CastShape(inStart, inDisplacement, cDefaultCollisionTolerance, *support, inPolygon, ioFraction);
	}
	else if (inShape->GetSubType() == EShapeSubType::RotatedTranslated)
	{
		const RotatedTranslatedShape *rt_shape = static_cast<const RotatedTranslatedShape *>(inShape);
		return sCorrectFractionForCharacterPadding(rt_shape->GetInnerShape(), inStart * Mat44::sRotation(rt_shape->GetRotation()), inDisplacement, rt_shape->TransformScale(inScale), inPolygon, ioFraction);
	}
	else if (inShape->GetSubType() == EShapeSubType::Scaled)
	{
		const ScaledShape *scaled_shape = static_cast<const ScaledShape *>(inShape);
		return sCorrectFractionForCharacterPadding(scaled_shape->GetInnerShape(), inStart, inDisplacement, inScale * scaled_shape->GetScale(), inPolygon, ioFraction);
	}
	else
	{
		JPH_ASSERT(false, "Not supported yet!");
		return false;
	}
}

bool CharacterVirtual::GetFirstContactForSweep(RVec3Arg inPosition, Vec3Arg inDisplacement, Contact &outContact, const IgnoredContactList &inIgnoredContacts, const BroadPhaseLayerFilter &inBroadPhaseLayerFilter, const ObjectLayerFilter &inObjectLayerFilter, const BodyFilter &inBodyFilter, const ShapeFilter &inShapeFilter) const
{
	// Too small distance -> skip checking
	float displacement_len_sq = inDisplacement.LengthSq();
	if (displacement_len_sq < 1.0e-8f)
		return false;

	// Calculate start transform
	RMat44 start = GetCenterOfMassTransform(inPosition, mRotation, mShape);

	// Settings for the cast
	ShapeCastSettings settings;
	settings.mBackFaceModeTriangles = mBackFaceMode;
	settings.mBackFaceModeConvex = EBackFaceMode::IgnoreBackFaces;
	settings.mActiveEdgeMode = EActiveEdgeMode::CollideOnlyWithActive;
	settings.mUseShrunkenShapeAndConvexRadius = true;
	settings.mReturnDeepestPoint = false;

	// Calculate how much extra fraction we need to add to the cast to account for the character padding
	float character_padding_fraction = mCharacterPadding / sqrt(displacement_len_sq);

	// Body filter
	IgnoreSingleBodyFilterChained body_filter(mInnerBodyID, inBodyFilter);

	// Cast shape
	Contact contact;
	contact.mFraction = 1.0f + character_padding_fraction;
	RVec3 base_offset = start.GetTranslation();
	ContactCastCollector collector(mSystem, this, inDisplacement, mUp, inIgnoredContacts, base_offset, contact);
	collector.ResetEarlyOutFraction(contact.mFraction);
	RShapeCast shape_cast(mShape, Vec3::sReplicate(1.0f), start, inDisplacement);
	mSystem->GetNarrowPhaseQuery().CastShape(shape_cast, settings, base_offset, collector, inBroadPhaseLayerFilter, inObjectLayerFilter, body_filter, inShapeFilter);

	// Also collide with other characters
	if (mCharacterVsCharacterCollision != nullptr)
	{
		collector.SetContext(nullptr); // We're no longer colliding with a transformed shape, reset
		mCharacterVsCharacterCollision->CastCharacter(this, start, inDisplacement, settings, base_offset, collector);
	}

	if (contact.mBodyB.IsInvalid() && contact.mCharacterB == nullptr)
		return false;

	// Store contact
	outContact = contact;

	TransformedShape ts;
	float character_padding = mCharacterPadding;
	if (outContact.mCharacterB != nullptr)
	{
		// Create a transformed shape for the character
		RMat44 com = outContact.mCharacterB->GetCenterOfMassTransform();
		ts = TransformedShape(com.GetTranslation(), com.GetQuaternion(), outContact.mCharacterB->GetShape(), BodyID(), SubShapeIDCreator());

		// We need to take the other character's padding into account as well
		character_padding += outContact.mCharacterB->mCharacterPadding;
	}
	else
	{
		// Create a transformed shape for the body
		ts = mSystem->GetBodyInterface().GetTransformedShape(outContact.mBodyB);
	}

	// Fetch the face we're colliding with
	Shape::SupportingFace face;
	ts.GetSupportingFace(outContact.mSubShapeIDB, -outContact.mContactNormal, base_offset, face);

	bool corrected = false;
	if (face.size() >= 2)
	{
		// Inflate the colliding face by the character padding
		PolygonConvexSupport polygon(face);
		AddConvexRadius add_cvx(polygon, character_padding);

		// Correct fraction to hit this inflated face instead of the inner shape
		corrected = sCorrectFractionForCharacterPadding(mShape, start.GetRotation(), inDisplacement, Vec3::sReplicate(1.0f), add_cvx, outContact.mFraction);
	}
	if (!corrected)
	{
		// When there's only a single contact point or when we were unable to correct the fraction,
		// we can just move the fraction back so that the character and its padding don't hit the contact point anymore
		outContact.mFraction = max(0.0f, outContact.mFraction - character_padding_fraction);
	}

	// Ensure that we never return a fraction that's bigger than 1 (which could happen due to float precision issues).
	outContact.mFraction = min(outContact.mFraction, 1.0f);

	return true;
}

void CharacterVirtual::DetermineConstraints(TempContactList &inContacts, float inDeltaTime, ConstraintList &outConstraints) const
{
	for (Contact &c : inContacts)
	{
		Vec3 contact_velocity = c.mLinearVelocity;

		// Penetrating contact: Add a contact velocity that pushes the character out at the desired speed
		if (c.mDistance < 0.0f)
			contact_velocity -= c.mContactNormal * c.mDistance * mPenetrationRecoverySpeed / inDeltaTime;

		// Convert to a constraint
		outConstraints.emplace_back();
		Constraint &constraint = outConstraints.back();
		constraint.mContact = &c;
		constraint.mLinearVelocity = contact_velocity;
		constraint.mPlane = Plane(c.mContactNormal, c.mDistance);

		// Next check if the angle is too steep and if it is add an additional constraint that holds the character back
		if (IsSlopeTooSteep(c.mSurfaceNormal))
		{
			// Only take planes that point up.
			// Note that we use the contact normal to allow for better sliding as the surface normal may be in the opposite direction of movement.
			float dot = c.mContactNormal.Dot(mUp);
			if (dot > 1.0e-3f) // Add a little slack, if the normal is perfectly horizontal we already have our vertical plane.
			{
				// Mark the slope constraint as steep
				constraint.mIsSteepSlope = true;

				// Make horizontal normal
				Vec3 normal = (c.mContactNormal - dot * mUp).Normalized();

				// Create a secondary constraint that blocks horizontal movement
				outConstraints.emplace_back();
				Constraint &vertical_constraint = outConstraints.back();
				vertical_constraint.mContact = &c;
				vertical_constraint.mLinearVelocity = contact_velocity.Dot(normal) * normal; // Project the contact velocity on the new normal so that both planes push at an equal rate
				vertical_constraint.mPlane = Plane(normal, c.mDistance / normal.Dot(c.mContactNormal)); // Calculate the distance we have to travel horizontally to hit the contact plane
			}
		}
	}
}

bool CharacterVirtual::HandleContact(Vec3Arg inVelocity, Constraint &ioConstraint, float inDeltaTime) const
{
	Contact &contact = *ioConstraint.mContact;

	// Validate the contact point
	if (!ValidateContact(contact))
		return false;

	// Send contact added event
	CharacterContactSettings settings;
	ContactAdded(contact, settings);
	contact.mCanPushCharacter = settings.mCanPushCharacter;

	// We don't have any further interaction with sensors beyond an OnContactAdded notification
	if (contact.mIsSensorB)
		return false;

	// If body B cannot receive an impulse, we're done
	if (!settings.mCanReceiveImpulses || contact.mMotionTypeB != EMotionType::Dynamic)
		return true;

	// Lock the body we're colliding with
	BodyLockWrite lock(mSystem->GetBodyLockInterface(), contact.mBodyB);
	if (!lock.SucceededAndIsInBroadPhase())
		return false; // Body has been removed, we should not collide with it anymore
	const Body &body = lock.GetBody();

	// Calculate the velocity that we want to apply at B so that it will start moving at the character's speed at the contact point
	constexpr float cDamping = 0.9f;
	constexpr float cPenetrationResolution = 0.4f;
	Vec3 relative_velocity = inVelocity - contact.mLinearVelocity;
	float projected_velocity = relative_velocity.Dot(contact.mContactNormal);
	float delta_velocity = -projected_velocity * cDamping - min(contact.mDistance, 0.0f) * cPenetrationResolution / inDeltaTime;

	// Don't apply impulses if we're separating
	if (delta_velocity < 0.0f)
		return true;

	// Determine mass properties of the body we're colliding with
	const MotionProperties *motion_properties = body.GetMotionProperties();
	RVec3 center_of_mass = body.GetCenterOfMassPosition();
	Mat44 inverse_inertia = body.GetInverseInertia();
	float inverse_mass = motion_properties->GetInverseMass();

	// Calculate the inverse of the mass of body B as seen at the contact point in the direction of the contact normal
	Vec3 jacobian = Vec3(contact.mPosition - center_of_mass).Cross(contact.mContactNormal);
	float inv_effective_mass = inverse_inertia.Multiply3x3(jacobian).Dot(jacobian) + inverse_mass;

	// Impulse P = M dv
	float impulse = delta_velocity / inv_effective_mass;

	// Clamp the impulse according to the character strength, character strength is a force in newtons, P = F dt
	float max_impulse = mMaxStrength * inDeltaTime;
	impulse = min(impulse, max_impulse);

	// Calculate the world space impulse to apply
	Vec3 world_impulse = -impulse * contact.mContactNormal;

	// Cancel impulse in down direction (we apply gravity later)
	float impulse_dot_up = world_impulse.Dot(mUp);
	if (impulse_dot_up < 0.0f)
		world_impulse -= impulse_dot_up * mUp;

	// Now apply the impulse (body is already locked so we use the no-lock interface)
	mSystem->GetBodyInterfaceNoLock().AddImpulse(contact.mBodyB, world_impulse, contact.mPosition);
	return true;
}

void CharacterVirtual::SolveConstraints(Vec3Arg inVelocity, float inDeltaTime, float inTimeRemaining, ConstraintList &ioConstraints, IgnoredContactList &ioIgnoredContacts, float &outTimeSimulated, Vec3 &outDisplacement, TempAllocator &inAllocator
#ifdef JPH_DEBUG_RENDERER
	, bool inDrawConstraints
#endif // JPH_DEBUG_RENDERER
	) const
{
	// If there are no constraints we can immediately move to our target
	if (ioConstraints.empty())
	{
		outDisplacement = inVelocity * inTimeRemaining;
		outTimeSimulated = inTimeRemaining;
		return;
	}

	// Create array that holds the constraints in order of time of impact (sort will happen later)
	Array<Constraint *, STLTempAllocator<Constraint *>> sorted_constraints(inAllocator);
	sorted_constraints.resize(ioConstraints.size());
	for (size_t index = 0; index < sorted_constraints.size(); index++)
		sorted_constraints[index] = &ioConstraints[index];

	// This is the velocity we use for the displacement, if we hit something it will be shortened
	Vec3 velocity = inVelocity;

	// Keep track of the last velocity that was applied to the character so that we can detect when the velocity reverses
	Vec3 last_velocity = inVelocity;

	// Start with no displacement
	outDisplacement = Vec3::sZero();
	outTimeSimulated = 0.0f;

	// These are the contacts that we hit previously without moving a significant distance
	Array<Constraint *, STLTempAllocator<Constraint *>> previous_contacts(inAllocator);
	previous_contacts.resize(mMaxConstraintIterations);
	int num_previous_contacts = 0;

	// Loop for a max amount of iterations
	for (uint iteration = 0; iteration < mMaxConstraintIterations; iteration++)
	{
		// Calculate time of impact for all constraints
		for (Constraint &c : ioConstraints)
		{
			// Project velocity on plane direction
			c.mProjectedVelocity = c.mPlane.GetNormal().Dot(c.mLinearVelocity - velocity);
			if (c.mProjectedVelocity < 1.0e-6f)
			{
				c.mTOI = FLT_MAX;
			}
			else
			{
				// Distance to plane
				float dist = c.mPlane.SignedDistance(outDisplacement);

				if (dist - c.mProjectedVelocity * inTimeRemaining > -1.0e-4f)
				{
					// Too little penetration, accept the movement
					c.mTOI = FLT_MAX;
				}
				else
				{
					// Calculate time of impact
					c.mTOI = max(0.0f, dist / c.mProjectedVelocity);
				}
			}
		}

		// Sort constraints on proximity
		QuickSort(sorted_constraints.begin(), sorted_constraints.end(), [](const Constraint *inLHS, const Constraint *inRHS) {
				// If both constraints hit at t = 0 then order the one that will push the character furthest first
				// Note that because we add velocity to penetrating contacts, this will also resolve contacts that penetrate the most
				if (inLHS->mTOI <= 0.0f && inRHS->mTOI <= 0.0f)
					return inLHS->mProjectedVelocity > inRHS->mProjectedVelocity;

				// Then sort on time of impact
				if (inLHS->mTOI != inRHS->mTOI)
					return inLHS->mTOI < inRHS->mTOI;

				// As a tie breaker sort static first so it has the most influence
				return inLHS->mContact->mMotionTypeB > inRHS->mContact->mMotionTypeB;
			});

		// Find the first valid constraint
		Constraint *constraint = nullptr;
		for (Constraint *c : sorted_constraints)
		{
			// Take the first contact and see if we can reach it
			if (c->mTOI >= inTimeRemaining)
			{
				// We can reach our goal!
				outDisplacement += velocity * inTimeRemaining;
				outTimeSimulated += inTimeRemaining;
				return;
			}

			// Test if this contact was discarded by the contact callback before
			if (c->mContact->mWasDiscarded)
				continue;

			// Check if we made contact with this before
			if (!c->mContact->mHadCollision)
			{
				// Handle the contact
				if (!HandleContact(velocity, *c, inDeltaTime))
				{
					// Constraint should be ignored, remove it from the list
					c->mContact->mWasDiscarded = true;

					// Mark it as ignored for GetFirstContactForSweep
					ioIgnoredContacts.emplace_back(c->mContact->mBodyB, c->mContact->mSubShapeIDB);
					continue;
				}

				c->mContact->mHadCollision = true;
			}

			// Cancel velocity of constraint if it cannot push the character
			if (!c->mContact->mCanPushCharacter)
				c->mLinearVelocity = Vec3::sZero();

			// We found the first constraint that we want to collide with
			constraint = c;
			break;
		}

		if (constraint == nullptr)
		{
			// All constraints were discarded, we can reach our goal!
			outDisplacement += velocity * inTimeRemaining;
			outTimeSimulated += inTimeRemaining;
			return;
		}

		// Move to the contact
		outDisplacement += velocity * constraint->mTOI;
		inTimeRemaining -= constraint->mTOI;
		outTimeSimulated += constraint->mTOI;

		// If there's not enough time left to be simulated, bail
		if (inTimeRemaining < mMinTimeRemaining)
			return;

		// If we've moved significantly, clear all previous contacts
		if (constraint->mTOI > 1.0e-4f)
			num_previous_contacts = 0;

		// Get the normal of the plane we're hitting
		Vec3 plane_normal = constraint->mPlane.GetNormal();

		// If we're hitting a steep slope we cancel the velocity towards the slope first so that we don't end up sliding up the slope
		// (we may hit the slope before the vertical wall constraint we added which will result in a small movement up causing jitter in the character movement)
		if (constraint->mIsSteepSlope)
		{
			// We're hitting a steep slope, create a vertical plane that blocks any further movement up the slope (note: not normalized)
			Vec3 vertical_plane_normal = plane_normal - plane_normal.Dot(mUp) * mUp;

			// Get the relative velocity between the character and the constraint
			Vec3 relative_velocity = velocity - constraint->mLinearVelocity;

			// Remove velocity towards the slope
			velocity = velocity - min(0.0f, relative_velocity.Dot(vertical_plane_normal)) * vertical_plane_normal / vertical_plane_normal.LengthSq();
		}

		// Get the relative velocity between the character and the constraint
		Vec3 relative_velocity = velocity - constraint->mLinearVelocity;

		// Calculate new velocity if we cancel the relative velocity in the normal direction
		Vec3 new_velocity = velocity - relative_velocity.Dot(plane_normal) * plane_normal;

		// Find the normal of the previous contact that we will violate the most if we move in this new direction
		float highest_penetration = 0.0f;
		Constraint *other_constraint = nullptr;
		for (Constraint **c = previous_contacts.data(); c < previous_contacts.data() + num_previous_contacts; ++c)
			if (*c != constraint)
			{
				// Calculate how much we will penetrate if we move in this direction
				Vec3 other_normal = (*c)->mPlane.GetNormal();
				float penetration = ((*c)->mLinearVelocity - new_velocity).Dot(other_normal);
				if (penetration > highest_penetration)
				{
					// We don't want parallel or anti-parallel normals as that will cause our cross product below to become zero. Slack is approx 10 degrees.
					float dot = other_normal.Dot(plane_normal);
					if (dot < 0.984f && dot > -0.984f)
					{
						highest_penetration = penetration;
						other_constraint = *c;
					}
				}
			}

		// Check if we found a 2nd constraint
		if (other_constraint != nullptr)
		{
			// Calculate the sliding direction and project the new velocity onto that sliding direction
			Vec3 other_normal = other_constraint->mPlane.GetNormal();
			Vec3 slide_dir = plane_normal.Cross(other_normal).Normalized();
			Vec3 velocity_in_slide_dir = new_velocity.Dot(slide_dir) * slide_dir;

			// Cancel the constraint velocity in the other constraint plane's direction so that we won't try to apply it again and keep ping ponging between planes
			constraint->mLinearVelocity -= min(0.0f, constraint->mLinearVelocity.Dot(other_normal)) * other_normal;

			// Cancel the other constraints velocity in this constraint plane's direction so that we won't try to apply it again and keep ping ponging between planes
			other_constraint->mLinearVelocity -= min(0.0f, other_constraint->mLinearVelocity.Dot(plane_normal)) * plane_normal;

			// Calculate the velocity of this constraint perpendicular to the slide direction
			Vec3 perpendicular_velocity = constraint->mLinearVelocity - constraint->mLinearVelocity.Dot(slide_dir) * slide_dir;

			// Calculate the velocity of the other constraint perpendicular to the slide direction
			Vec3 other_perpendicular_velocity = other_constraint->mLinearVelocity - other_constraint->mLinearVelocity.Dot(slide_dir) * slide_dir;

			// Add all components together
			new_velocity = velocity_in_slide_dir + perpendicular_velocity + other_perpendicular_velocity;
		}

		// Allow application to modify calculated velocity
		if (mListener != nullptr)
		{
			if (constraint->mContact->mCharacterB != nullptr)
				mListener->OnCharacterContactSolve(this, constraint->mContact->mCharacterB, constraint->mContact->mSubShapeIDB, constraint->mContact->mPosition, constraint->mContact->mContactNormal, constraint->mContact->mLinearVelocity, constraint->mContact->mMaterial, velocity, new_velocity);
			else
				mListener->OnContactSolve(this, constraint->mContact->mBodyB, constraint->mContact->mSubShapeIDB, constraint->mContact->mPosition, constraint->mContact->mContactNormal, constraint->mContact->mLinearVelocity, constraint->mContact->mMaterial, velocity, new_velocity);
		}

#ifdef JPH_DEBUG_RENDERER
		if (inDrawConstraints)
		{
			// Calculate where to draw
			RVec3 offset = mPosition + Vec3(0, 0, 2.5f * (iteration + 1));

			// Draw constraint plane
			DebugRenderer::sInstance->DrawPlane(offset, constraint->mPlane.GetNormal(), Color::sCyan, 1.0f);

			// Draw 2nd constraint plane
			if (other_constraint != nullptr)
				DebugRenderer::sInstance->DrawPlane(offset, other_constraint->mPlane.GetNormal(), Color::sBlue, 1.0f);

			// Draw starting velocity
			DebugRenderer::sInstance->DrawArrow(offset, offset + velocity, Color::sGreen, 0.05f);

			// Draw resulting velocity
			DebugRenderer::sInstance->DrawArrow(offset, offset + new_velocity, Color::sRed, 0.05f);
		}
#endif // JPH_DEBUG_RENDERER

		// Update the velocity
		velocity = new_velocity;

		// Add the contact to the list so that next iteration we can avoid violating it again
		previous_contacts[num_previous_contacts] = constraint;
		num_previous_contacts++;

		// Check early out
		if (constraint->mProjectedVelocity < 1.0e-8f // Constraint should not be pushing, otherwise there may be other constraints that are pushing us
			&& velocity.LengthSq() < 1.0e-8f) // There's not enough velocity left
			return;

		// If the constraint has velocity we accept the new velocity, otherwise check that we didn't reverse velocity
		if (!constraint->mLinearVelocity.IsNearZero(1.0e-8f))
			last_velocity = constraint->mLinearVelocity;
		else if (velocity.Dot(last_velocity) < 0.0f)
			return;
	}
}

void CharacterVirtual::UpdateSupportingContact(bool inSkipContactVelocityCheck, TempAllocator &inAllocator)
{
	// Flag contacts as having a collision if they're close enough but ignore contacts we're moving away from.
	// Note that if we did MoveShape before we want to preserve any contacts that it marked as colliding
	for (Contact &c : mActiveContacts)
		if (!c.mWasDiscarded
			&& !c.mHadCollision
			&& c.mDistance < mCollisionTolerance
			&& (inSkipContactVelocityCheck || c.mSurfaceNormal.Dot(mLinearVelocity - c.mLinearVelocity) <= 1.0e-4f))
		{
			if (ValidateContact(c) && !c.mIsSensorB)
				c.mHadCollision = true;
			else
				c.mWasDiscarded = true;
		}

	// Calculate transform that takes us to character local space
	RMat44 inv_transform = RMat44::sInverseRotationTranslation(mRotation, mPosition);

	// Determine if we're supported or not
	int num_supported = 0;
	int num_sliding = 0;
	int num_avg_normal = 0;
	Vec3 avg_normal = Vec3::sZero();
	Vec3 avg_velocity = Vec3::sZero();
	const Contact *supporting_contact = nullptr;
	float max_cos_angle = -FLT_MAX;
	const Contact *deepest_contact = nullptr;
	float smallest_distance = FLT_MAX;
	for (const Contact &c : mActiveContacts)
		if (c.mHadCollision)
		{
			// Calculate the angle between the plane normal and the up direction
			float cos_angle = c.mSurfaceNormal.Dot(mUp);

			// Find the deepest contact
			if (c.mDistance < smallest_distance)
			{
				deepest_contact = &c;
				smallest_distance = c.mDistance;
			}

			// If this contact is in front of our plane, we cannot be supported by it
			if (mSupportingVolume.SignedDistance(Vec3(inv_transform * c.mPosition)) > 0.0f)
				continue;

			// Find the contact with the normal that is pointing most upwards and store it
			if (max_cos_angle < cos_angle)
			{
				supporting_contact = &c;
				max_cos_angle = cos_angle;
			}

			// Check if this is a sliding or supported contact
			bool is_supported = mCosMaxSlopeAngle > cNoMaxSlopeAngle || cos_angle >= mCosMaxSlopeAngle;
			if (is_supported)
				num_supported++;
			else
				num_sliding++;

			// If the angle between the two is less than 85 degrees we also use it to calculate the average normal
			if (cos_angle >= 0.08f)
			{
				avg_normal += c.mSurfaceNormal;
				num_avg_normal++;

				// For static or dynamic objects or for contacts that don't support us just take the contact velocity
				if (c.mMotionTypeB != EMotionType::Kinematic || !is_supported)
					avg_velocity += c.mLinearVelocity;
				else
				{
					// For keyframed objects that support us calculate the velocity at our position rather than at the contact position so that we properly follow the object
					BodyLockRead lock(mSystem->GetBodyLockInterface(), c.mBodyB);
					if (lock.SucceededAndIsInBroadPhase())
					{
						const Body &body = lock.GetBody();

						// Get adjusted body velocity
						Vec3 linear_velocity, angular_velocity;
						GetAdjustedBodyVelocity(body, linear_velocity, angular_velocity);

						// Calculate the ground velocity
						avg_velocity += CalculateCharacterGroundVelocity(body.GetCenterOfMassPosition(), linear_velocity, angular_velocity, mLastDeltaTime);
					}
					else
					{
						// Fall back to contact velocity
						avg_velocity += c.mLinearVelocity;
					}
				}
			}
		}

	// Take either the most supporting contact or the deepest contact
	const Contact *best_contact = supporting_contact != nullptr? supporting_contact : deepest_contact;

	// Calculate average normal and velocity
	if (num_avg_normal >= 1)
	{
		mGroundNormal = avg_normal.Normalized();
		mGroundVelocity = avg_velocity / float(num_avg_normal);
	}
	else if (best_contact != nullptr)
	{
		mGroundNormal = best_contact->mSurfaceNormal;
		mGroundVelocity = best_contact->mLinearVelocity;
	}
	else
	{
		mGroundNormal = Vec3::sZero();
		mGroundVelocity = Vec3::sZero();
	}

	// Copy contact properties
	if (best_contact != nullptr)
	{
		mGroundBodyID = best_contact->mBodyB;
		mGroundBodySubShapeID = best_contact->mSubShapeIDB;
		mGroundPosition = best_contact->mPosition;
		mGroundMaterial = best_contact->mMaterial;
		mGroundUserData = best_contact->mUserData;
	}
	else
	{
		mGroundBodyID = BodyID();
		mGroundBodySubShapeID = SubShapeID();
		mGroundPosition = RVec3::sZero();
		mGroundMaterial = PhysicsMaterial::sDefault;
		mGroundUserData = 0;
	}

	// Determine ground state
	if (num_supported > 0)
	{
		// We made contact with something that supports us
		mGroundState = EGroundState::OnGround;
	}
	else if (num_sliding > 0)
	{
		if ((mLinearVelocity - deepest_contact->mLinearVelocity).Dot(mUp) > 1.0e-4f)
		{
			// We cannot be on ground if we're moving upwards relative to the ground
			mGroundState = EGroundState::OnSteepGround;
		}
		else
		{
			// If we're sliding down, we may actually be standing on multiple sliding contacts in such a way that we can't slide off, in this case we're also supported

			// Convert the contacts into constraints
			TempContactList contacts(mActiveContacts.begin(), mActiveContacts.end(), inAllocator);
			ConstraintList constraints(inAllocator);
			constraints.reserve(contacts.size() * 2);
			DetermineConstraints(contacts, mLastDeltaTime, constraints);

			// Solve the displacement using these constraints, this is used to check if we didn't move at all because we are supported
			Vec3 displacement;
			float time_simulated;
			IgnoredContactList ignored_contacts(inAllocator);
			ignored_contacts.reserve(contacts.size());
			SolveConstraints(-mUp, 1.0f, 1.0f, constraints, ignored_contacts, time_simulated, displacement, inAllocator);

			// If we're blocked then we're supported, otherwise we're sliding
			float min_required_displacement_sq = Square(0.6f * mLastDeltaTime);
			if (time_simulated < 0.001f || displacement.LengthSq() < min_required_displacement_sq)
				mGroundState = EGroundState::OnGround;
			else
				mGroundState = EGroundState::OnSteepGround;
		}
	}
	else
	{
		// Not supported by anything
		mGroundState = best_contact != nullptr? EGroundState::NotSupported : EGroundState::InAir;
	}
}

void CharacterVirtual::StoreActiveContacts(const TempContactList &inContacts, TempAllocator &inAllocator)
{
	mActiveContacts.assign(inContacts.begin(), inContacts.end());

	UpdateSupportingContact(true, inAllocator);
}

void CharacterVirtual::MoveShape(RVec3 &ioPosition, Vec3Arg inVelocity, float inDeltaTime, ContactList *outActiveContacts, const BroadPhaseLayerFilter &inBroadPhaseLayerFilter, const ObjectLayerFilter &inObjectLayerFilter, const BodyFilter &inBodyFilter, const ShapeFilter &inShapeFilter, TempAllocator &inAllocator
#ifdef JPH_DEBUG_RENDERER
	, bool inDrawConstraints
#endif // JPH_DEBUG_RENDERER
	) const
{
	JPH_DET_LOG("CharacterVirtual::MoveShape: pos: " << ioPosition << " vel: " << inVelocity << " dt: " << inDeltaTime);

	Vec3 movement_direction = inVelocity.NormalizedOr(Vec3::sZero());

	float time_remaining = inDeltaTime;
	for (uint iteration = 0; iteration < mMaxCollisionIterations && time_remaining >= mMinTimeRemaining; iteration++)
	{
		JPH_DET_LOG("iter: " << iteration << " time: " << time_remaining);

		// Determine contacts in the neighborhood
		TempContactList contacts(inAllocator);
		contacts.reserve(mMaxNumHits);
		GetContactsAtPosition(ioPosition, movement_direction, mShape, contacts, inBroadPhaseLayerFilter, inObjectLayerFilter, inBodyFilter, inShapeFilter);

#ifdef JPH_ENABLE_DETERMINISM_LOG
		for (const Contact &c : contacts)
			JPH_DET_LOG("contact: " << c.mPosition << " vel: " << c.mLinearVelocity << " cnormal: " << c.mContactNormal << " snormal: " << c.mSurfaceNormal << " dist: " << c.mDistance << " fraction: " << c.mFraction << " body: " << c.mBodyB << " subshape: " << c.mSubShapeIDB);
#endif // JPH_ENABLE_DETERMINISM_LOG

		// Remove contacts with the same body that have conflicting normals
		IgnoredContactList ignored_contacts(inAllocator);
		ignored_contacts.reserve(contacts.size());
		RemoveConflictingContacts(contacts, ignored_contacts);

		// Convert contacts into constraints
		ConstraintList constraints(inAllocator);
		constraints.reserve(contacts.size() * 2);
		DetermineConstraints(contacts, inDeltaTime, constraints);

#ifdef JPH_DEBUG_RENDERER
		bool draw_constraints = inDrawConstraints && iteration == 0;
		if (draw_constraints)
		{
			for (const Constraint &c : constraints)
			{
				// Draw contact point
				DebugRenderer::sInstance->DrawMarker(c.mContact->mPosition, Color::sYellow, 0.05f);
				Vec3 dist_to_plane = -c.mPlane.GetConstant() * c.mPlane.GetNormal();

				// Draw arrow towards surface that we're hitting
				DebugRenderer::sInstance->DrawArrow(c.mContact->mPosition, c.mContact->mPosition - dist_to_plane, Color::sYellow, 0.05f);

				// Draw plane around the player position indicating the space that we can move
				DebugRenderer::sInstance->DrawPlane(mPosition + dist_to_plane, c.mPlane.GetNormal(), Color::sCyan, 1.0f);
				DebugRenderer::sInstance->DrawArrow(mPosition + dist_to_plane, mPosition + dist_to_plane + c.mContact->mSurfaceNormal, Color::sRed, 0.05f);
			}
		}
#endif // JPH_DEBUG_RENDERER

		// Solve the displacement using these constraints
		Vec3 displacement;
		float time_simulated;
		SolveConstraints(inVelocity, inDeltaTime, time_remaining, constraints, ignored_contacts, time_simulated, displacement, inAllocator
		#ifdef JPH_DEBUG_RENDERER
			, draw_constraints
		#endif // JPH_DEBUG_RENDERER
			);

		// Store the contacts now that the colliding ones have been marked
		if (outActiveContacts != nullptr)
			outActiveContacts->assign(contacts.begin(), contacts.end());

		// Do a sweep to test if the path is really unobstructed
		Contact cast_contact;
		if (GetFirstContactForSweep(ioPosition, displacement, cast_contact, ignored_contacts, inBroadPhaseLayerFilter, inObjectLayerFilter, inBodyFilter, inShapeFilter))
		{
			displacement *= cast_contact.mFraction;
			time_simulated *= cast_contact.mFraction;
		}

		// Update the position
		ioPosition += displacement;
		time_remaining -= time_simulated;

		// If the displacement during this iteration was too small we assume we cannot further progress this update
		if (displacement.LengthSq() < 1.0e-8f)
			break;
	}
}

void CharacterVirtual::SetUserData(uint64 inUserData)
{
	mUserData = inUserData;

	if (!mInnerBodyID.IsInvalid())
		mSystem->GetBodyInterface().SetUserData(mInnerBodyID, inUserData);
}

Vec3 CharacterVirtual::CancelVelocityTowardsSteepSlopes(Vec3Arg inDesiredVelocity) const
{
	// If we're not pushing against a steep slope, return the desired velocity
	// Note: This is important as WalkStairs overrides the ground state to OnGround when its first check fails but the second succeeds
	if (mGroundState == CharacterVirtual::EGroundState::OnGround
		|| mGroundState == CharacterVirtual::EGroundState::InAir)
		return inDesiredVelocity;

	Vec3 desired_velocity = inDesiredVelocity;
	for (const Contact &c : mActiveContacts)
		if (c.mHadCollision
			&& IsSlopeTooSteep(c.mSurfaceNormal))
		{
			// Note that we use the contact normal to allow for better sliding as the surface normal may be in the opposite direction of movement.
			Vec3 normal = c.mContactNormal;

			// Remove normal vertical component
			normal -= normal.Dot(mUp) * mUp;

			// Cancel horizontal movement in opposite direction
			float dot = normal.Dot(desired_velocity);
			if (dot < 0.0f)
				desired_velocity -= (dot * normal) / normal.LengthSq();
		}
	return desired_velocity;
}

void CharacterVirtual::Update(float inDeltaTime, Vec3Arg inGravity, const BroadPhaseLayerFilter &inBroadPhaseLayerFilter, const ObjectLayerFilter &inObjectLayerFilter, const BodyFilter &inBodyFilter, const ShapeFilter &inShapeFilter, TempAllocator &inAllocator)
{
	// If there's no delta time, we don't need to do anything
	if (inDeltaTime <= 0.0f)
		return;

	// Remember delta time for checking if we're supported by the ground
	mLastDeltaTime = inDeltaTime;

	// Slide the shape through the world
	MoveShape(mPosition, mLinearVelocity, inDeltaTime, &mActiveContacts, inBroadPhaseLayerFilter, inObjectLayerFilter, inBodyFilter, inShapeFilter, inAllocator
	#ifdef JPH_DEBUG_RENDERER
		, sDrawConstraints
	#endif // JPH_DEBUG_RENDERER
		);

	// Determine the object that we're standing on
	UpdateSupportingContact(false, inAllocator);

	// Ensure that the rigid body ends up at the new position
	UpdateInnerBodyTransform();

	// If we're on the ground
	if (!mGroundBodyID.IsInvalid() && mMass > 0.0f)
	{
		// Add the impulse to the ground due to gravity: P = F dt = M g dt
		float normal_dot_gravity = mGroundNormal.Dot(inGravity);
		if (normal_dot_gravity < 0.0f)
		{
			Vec3 world_impulse = -(mMass * normal_dot_gravity / inGravity.Length() * inDeltaTime) * inGravity;
			mSystem->GetBodyInterface().AddImpulse(mGroundBodyID, world_impulse, mGroundPosition);
		}
	}
}

void CharacterVirtual::RefreshContacts(const BroadPhaseLayerFilter &inBroadPhaseLayerFilter, const ObjectLayerFilter &inObjectLayerFilter, const BodyFilter &inBodyFilter, const ShapeFilter &inShapeFilter, TempAllocator &inAllocator)
{
	// Determine the contacts
	TempContactList contacts(inAllocator);
	contacts.reserve(mMaxNumHits);
	GetContactsAtPosition(mPosition, mLinearVelocity.NormalizedOr(Vec3::sZero()), mShape, contacts, inBroadPhaseLayerFilter, inObjectLayerFilter, inBodyFilter, inShapeFilter);

	StoreActiveContacts(contacts, inAllocator);
}

void CharacterVirtual::UpdateGroundVelocity()
{
	BodyLockRead lock(mSystem->GetBodyLockInterface(), mGroundBodyID);
	if (lock.SucceededAndIsInBroadPhase())
	{
		const Body &body = lock.GetBody();

		// Get adjusted body velocity
		Vec3 linear_velocity, angular_velocity;
		GetAdjustedBodyVelocity(body, linear_velocity, angular_velocity);

		// Calculate the ground velocity
		mGroundVelocity = CalculateCharacterGroundVelocity(body.GetCenterOfMassPosition(), linear_velocity, angular_velocity, mLastDeltaTime);
	}
}

void CharacterVirtual::MoveToContact(RVec3Arg inPosition, const Contact &inContact, const BroadPhaseLayerFilter &inBroadPhaseLayerFilter, const ObjectLayerFilter &inObjectLayerFilter, const BodyFilter &inBodyFilter, const ShapeFilter &inShapeFilter, TempAllocator &inAllocator)
{
	// Set the new position
	SetPosition(inPosition);

	// Trigger contact added callback
	CharacterContactSettings dummy;
	ContactAdded(inContact, dummy);

	// Determine the contacts
	TempContactList contacts(inAllocator);
	contacts.reserve(mMaxNumHits + 1); // +1 because we can add one extra below
	GetContactsAtPosition(mPosition, mLinearVelocity.NormalizedOr(Vec3::sZero()), mShape, contacts, inBroadPhaseLayerFilter, inObjectLayerFilter, inBodyFilter, inShapeFilter);

	// Ensure that we mark inContact as colliding
	bool found_contact = false;
	for (Contact &c : contacts)
		if (c.mBodyB == inContact.mBodyB
			&& c.mSubShapeIDB == inContact.mSubShapeIDB)
		{
			c.mHadCollision = true;
			found_contact = true;
		}
	if (!found_contact)
	{
		contacts.push_back(inContact);

		Contact &copy = contacts.back();
		copy.mHadCollision = true;
	}

	StoreActiveContacts(contacts, inAllocator);
	JPH_ASSERT(mGroundState != EGroundState::InAir);

	// Ensure that the rigid body ends up at the new position
	UpdateInnerBodyTransform();
}

bool CharacterVirtual::SetShape(const Shape *inShape, float inMaxPenetrationDepth, const BroadPhaseLayerFilter &inBroadPhaseLayerFilter, const ObjectLayerFilter &inObjectLayerFilter, const BodyFilter &inBodyFilter, const ShapeFilter &inShapeFilter, TempAllocator &inAllocator)
{
	if (mShape == nullptr || mSystem == nullptr)
	{
		// It hasn't been initialized yet
		mShape = inShape;
		return true;
	}

	if (inShape != mShape && inShape != nullptr)
	{
		if (inMaxPenetrationDepth < FLT_MAX)
		{
			// Check collision around the new shape
			TempContactList contacts(inAllocator);
			contacts.reserve(mMaxNumHits);
			GetContactsAtPosition(mPosition, mLinearVelocity.NormalizedOr(Vec3::sZero()), inShape, contacts, inBroadPhaseLayerFilter, inObjectLayerFilter, inBodyFilter, inShapeFilter);

			// Test if this results in penetration, if so cancel the transition
			for (const Contact &c : contacts)
				if (c.mDistance < -inMaxPenetrationDepth
					&& !c.mIsSensorB)
					return false;

			StoreActiveContacts(contacts, inAllocator);
		}

		// Set new shape
		mShape = inShape;
	}

	return mShape == inShape;
}

void CharacterVirtual::SetInnerBodyShape(const Shape *inShape)
{
	mSystem->GetBodyInterface().SetShape(mInnerBodyID, inShape, false, EActivation::DontActivate);
}

bool CharacterVirtual::CanWalkStairs(Vec3Arg inLinearVelocity) const
{
	// We can only walk stairs if we're supported
	if (!IsSupported())
		return false;

	// Check if there's enough horizontal velocity to trigger a stair walk
	Vec3 horizontal_velocity = inLinearVelocity - inLinearVelocity.Dot(mUp) * mUp;
	if (horizontal_velocity.IsNearZero(1.0e-6f))
		return false;

	// Check contacts for steep slopes
	for (const Contact &c : mActiveContacts)
		if (c.mHadCollision
			&& c.mSurfaceNormal.Dot(horizontal_velocity - c.mLinearVelocity) < 0.0f // Pushing into the contact
			&& IsSlopeTooSteep(c.mSurfaceNormal)) // Slope too steep
			return true;

	return false;
}

bool CharacterVirtual::WalkStairs(float inDeltaTime, Vec3Arg inStepUp, Vec3Arg inStepForward, Vec3Arg inStepForwardTest, Vec3Arg inStepDownExtra, const BroadPhaseLayerFilter &inBroadPhaseLayerFilter, const ObjectLayerFilter &inObjectLayerFilter, const BodyFilter &inBodyFilter, const ShapeFilter &inShapeFilter, TempAllocator &inAllocator)
{
	// Move up
	Vec3 up = inStepUp;
	Contact contact;
	IgnoredContactList dummy_ignored_contacts(inAllocator);
	if (GetFirstContactForSweep(mPosition, up, contact, dummy_ignored_contacts, inBroadPhaseLayerFilter, inObjectLayerFilter, inBodyFilter, inShapeFilter))
	{
		if (contact.mFraction < 1.0e-6f)
			return false; // No movement, cancel

		// Limit up movement to the first contact point
		up *= contact.mFraction;
	}
	RVec3 up_position = mPosition + up;

#ifdef JPH_DEBUG_RENDERER
	// Draw sweep up
	if (sDrawWalkStairs)
		DebugRenderer::sInstance->DrawArrow(mPosition, up_position, Color::sWhite, 0.01f);
#endif // JPH_DEBUG_RENDERER

	// Collect normals of steep slopes that we would like to walk stairs on.
	// We need to do this before calling MoveShape because it will update mActiveContacts.
	Vec3 character_velocity = inStepForward / inDeltaTime;
	Vec3 horizontal_velocity = character_velocity - character_velocity.Dot(mUp) * mUp;
	Array<Vec3, STLTempAllocator<Vec3>> steep_slope_normals(inAllocator);
	steep_slope_normals.reserve(mActiveContacts.size());
	for (const Contact &c : mActiveContacts)
		if (c.mHadCollision
			&& c.mSurfaceNormal.Dot(horizontal_velocity - c.mLinearVelocity) < 0.0f // Pushing into the contact
			&& IsSlopeTooSteep(c.mSurfaceNormal)) // Slope too steep
			steep_slope_normals.push_back(c.mSurfaceNormal);
	if (steep_slope_normals.empty())
		return false; // No steep slopes, cancel

	// Horizontal movement
	RVec3 new_position = up_position;
	MoveShape(new_position, character_velocity, inDeltaTime, nullptr, inBroadPhaseLayerFilter, inObjectLayerFilter, inBodyFilter, inShapeFilter, inAllocator);
	Vec3 horizontal_movement = Vec3(new_position - up_position);
	float horizontal_movement_sq = horizontal_movement.LengthSq();
	if (horizontal_movement_sq < 1.0e-8f)
		return false; // No movement, cancel

	// Check if we made any progress towards any of the steep slopes, if not we just slid along the slope
	// so we need to cancel the stair walk or else we will move faster than we should as we've done
	// normal movement first and then stair walk.
	bool made_progress = false;
	float max_dot = -0.05f * inStepForward.Length();
	for (const Vec3 &normal : steep_slope_normals)
		if (normal.Dot(horizontal_movement) < max_dot)
		{
			// We moved more than 5% of the forward step against a steep slope, accept this as progress
			made_progress = true;
			break;
		}
	if (!made_progress)
		return false;

#ifdef JPH_DEBUG_RENDERER
	// Draw horizontal sweep
	if (sDrawWalkStairs)
		DebugRenderer::sInstance->DrawArrow(up_position, new_position, Color::sWhite, 0.01f);
#endif // JPH_DEBUG_RENDERER

	// Move down towards the floor.
	// Note that we travel the same amount down as we traveled up with the specified extra
	Vec3 down = -up + inStepDownExtra;
	if (!GetFirstContactForSweep(new_position, down, contact, dummy_ignored_contacts, inBroadPhaseLayerFilter, inObjectLayerFilter, inBodyFilter, inShapeFilter))
		return false; // No floor found, we're in mid air, cancel stair walk

#ifdef JPH_DEBUG_RENDERER
	// Draw sweep down
	if (sDrawWalkStairs)
	{
		RVec3 debug_pos = new_position + contact.mFraction * down;
		DebugRenderer::sInstance->DrawArrow(new_position, debug_pos, Color::sWhite, 0.01f);
		DebugRenderer::sInstance->DrawArrow(contact.mPosition, contact.mPosition + contact.mSurfaceNormal, Color::sWhite, 0.01f);
		mShape->Draw(DebugRenderer::sInstance, GetCenterOfMassTransform(debug_pos, mRotation, mShape), Vec3::sReplicate(1.0f), Color::sWhite, false, true);
	}
#endif // JPH_DEBUG_RENDERER

	// Test for floor that will support the character
	if (IsSlopeTooSteep(contact.mSurfaceNormal))
	{
		// If no test position was provided, we cancel the stair walk
		if (inStepForwardTest.IsNearZero())
			return false;

		// Delta time may be very small, so it may be that we hit the edge of a step and the normal is too horizontal.
		// In order to judge if the floor is flat further along the sweep, we test again for a floor at inStepForwardTest
		// and check if the normal is valid there.
		RVec3 test_position = up_position;
		MoveShape(test_position, inStepForwardTest / inDeltaTime, inDeltaTime, nullptr, inBroadPhaseLayerFilter, inObjectLayerFilter, inBodyFilter, inShapeFilter, inAllocator);
		float test_horizontal_movement_sq = Vec3(test_position - up_position).LengthSq();
		if (test_horizontal_movement_sq <= horizontal_movement_sq + 1.0e-8f)
			return false; // We didn't move any further than in the previous test

	#ifdef JPH_DEBUG_RENDERER
		// Draw 2nd sweep horizontal
		if (sDrawWalkStairs)
			DebugRenderer::sInstance->DrawArrow(up_position, test_position, Color::sCyan, 0.01f);
	#endif // JPH_DEBUG_RENDERER

		// Then sweep down
		Contact test_contact;
		if (!GetFirstContactForSweep(test_position, down, test_contact, dummy_ignored_contacts, inBroadPhaseLayerFilter, inObjectLayerFilter, inBodyFilter, inShapeFilter))
			return false;

	#ifdef JPH_DEBUG_RENDERER
		// Draw 2nd sweep down
		if (sDrawWalkStairs)
		{
			RVec3 debug_pos = test_position + test_contact.mFraction * down;
			DebugRenderer::sInstance->DrawArrow(test_position, debug_pos, Color::sCyan, 0.01f);
			DebugRenderer::sInstance->DrawArrow(test_contact.mPosition, test_contact.mPosition + test_contact.mSurfaceNormal, Color::sCyan, 0.01f);
			mShape->Draw(DebugRenderer::sInstance, GetCenterOfMassTransform(debug_pos, mRotation, mShape), Vec3::sReplicate(1.0f), Color::sCyan, false, true);
		}
	#endif // JPH_DEBUG_RENDERER

		if (IsSlopeTooSteep(test_contact.mSurfaceNormal))
			return false;
	}

	// Calculate new down position
	down *= contact.mFraction;
	new_position += down;

	// Move the character to the new location
	MoveToContact(new_position, contact, inBroadPhaseLayerFilter, inObjectLayerFilter, inBodyFilter, inShapeFilter, inAllocator);

	// Override ground state to 'on ground', it is possible that the contact normal is too steep, but in this case the inStepForwardTest has found a contact normal that is not too steep
	mGroundState = EGroundState::OnGround;

	return true;
}

bool CharacterVirtual::StickToFloor(Vec3Arg inStepDown, const BroadPhaseLayerFilter &inBroadPhaseLayerFilter, const ObjectLayerFilter &inObjectLayerFilter, const BodyFilter &inBodyFilter, const ShapeFilter &inShapeFilter, TempAllocator &inAllocator)
{
	// Try to find the floor
	Contact contact;
	IgnoredContactList dummy_ignored_contacts(inAllocator);
	if (!GetFirstContactForSweep(mPosition, inStepDown, contact, dummy_ignored_contacts, inBroadPhaseLayerFilter, inObjectLayerFilter, inBodyFilter, inShapeFilter))
		return false; // If no floor found, don't update our position

	// Calculate new position
	RVec3 new_position = mPosition + contact.mFraction * inStepDown;

#ifdef JPH_DEBUG_RENDERER
	// Draw sweep down
	if (sDrawStickToFloor)
	{
		DebugRenderer::sInstance->DrawArrow(mPosition, new_position, Color::sOrange, 0.01f);
		mShape->Draw(DebugRenderer::sInstance, GetCenterOfMassTransform(new_position, mRotation, mShape), Vec3::sReplicate(1.0f), Color::sOrange, false, true);
	}
#endif // JPH_DEBUG_RENDERER

	// Move the character to the new location
	MoveToContact(new_position, contact, inBroadPhaseLayerFilter, inObjectLayerFilter, inBodyFilter, inShapeFilter, inAllocator);
	return true;
}

void CharacterVirtual::ExtendedUpdate(float inDeltaTime, Vec3Arg inGravity, const ExtendedUpdateSettings &inSettings, const BroadPhaseLayerFilter &inBroadPhaseLayerFilter, const ObjectLayerFilter &inObjectLayerFilter, const BodyFilter &inBodyFilter, const ShapeFilter &inShapeFilter, TempAllocator &inAllocator)
{
	// Update the velocity
	Vec3 desired_velocity = mLinearVelocity;
	mLinearVelocity = CancelVelocityTowardsSteepSlopes(desired_velocity);

	// Remember old position
	RVec3 old_position = mPosition;

	// Track if on ground before the update
	bool ground_to_air = IsSupported();

	// Update the character position (instant, do not have to wait for physics update)
	Update(inDeltaTime, inGravity, inBroadPhaseLayerFilter, inObjectLayerFilter, inBodyFilter, inShapeFilter, inAllocator);

	// ... and that we got into air after
	if (IsSupported())
		ground_to_air = false;

	// If stick to floor enabled and we're going from supported to not supported
	if (ground_to_air && !inSettings.mStickToFloorStepDown.IsNearZero())
	{
		// If we're not moving up, stick to the floor
		float velocity = Vec3(mPosition - old_position).Dot(mUp) / inDeltaTime;
		if (velocity <= 1.0e-6f)
			StickToFloor(inSettings.mStickToFloorStepDown, inBroadPhaseLayerFilter, inObjectLayerFilter, inBodyFilter, inShapeFilter, inAllocator);
	}

	// If walk stairs enabled
	if (!inSettings.mWalkStairsStepUp.IsNearZero())
	{
		// Calculate how much we wanted to move horizontally
		Vec3 desired_horizontal_step = desired_velocity * inDeltaTime;
		desired_horizontal_step -= desired_horizontal_step.Dot(mUp) * mUp;
		float desired_horizontal_step_len = desired_horizontal_step.Length();
		if (desired_horizontal_step_len > 0.0f)
		{
			// Calculate how much we moved horizontally
			Vec3 achieved_horizontal_step = Vec3(mPosition - old_position);
			achieved_horizontal_step -= achieved_horizontal_step.Dot(mUp) * mUp;

			// Only count movement in the direction of the desired movement
			// (otherwise we find it ok if we're sliding downhill while we're trying to climb uphill)
			Vec3 step_forward_normalized = desired_horizontal_step / desired_horizontal_step_len;
			achieved_horizontal_step = max(0.0f, achieved_horizontal_step.Dot(step_forward_normalized)) * step_forward_normalized;
			float achieved_horizontal_step_len = achieved_horizontal_step.Length();

			// If we didn't move as far as we wanted and we're against a slope that's too steep
			if (achieved_horizontal_step_len + 1.0e-4f < desired_horizontal_step_len
				&& CanWalkStairs(desired_velocity))
			{
				// Calculate how much we should step forward
				// Note that we clamp the step forward to a minimum distance. This is done because at very high frame rates the delta time
				// may be very small, causing a very small step forward. If the step becomes small enough, we may not move far enough
				// horizontally to actually end up at the top of the step.
				Vec3 step_forward = step_forward_normalized * max(inSettings.mWalkStairsMinStepForward, desired_horizontal_step_len - achieved_horizontal_step_len);

				// Calculate how far to scan ahead for a floor. This is only used in case the floor normal at step_forward is too steep.
				// In that case an additional check will be performed at this distance to check if that normal is not too steep.
				// Start with the ground normal in the horizontal plane and normalizing it
				Vec3 step_forward_test = -mGroundNormal;
				step_forward_test -= step_forward_test.Dot(mUp) * mUp;
				step_forward_test = step_forward_test.NormalizedOr(step_forward_normalized);

				// If this normalized vector and the character forward vector is bigger than a preset angle, we use the character forward vector instead of the ground normal
				// to do our forward test
				if (step_forward_test.Dot(step_forward_normalized) < inSettings.mWalkStairsCosAngleForwardContact)
					step_forward_test = step_forward_normalized;

				// Calculate the correct magnitude for the test vector
				step_forward_test *= inSettings.mWalkStairsStepForwardTest;

				WalkStairs(inDeltaTime, inSettings.mWalkStairsStepUp, step_forward, step_forward_test, inSettings.mWalkStairsStepDownExtra, inBroadPhaseLayerFilter, inObjectLayerFilter, inBodyFilter, inShapeFilter, inAllocator);
			}
		}
	}
}

void CharacterVirtual::Contact::SaveState(StateRecorder &inStream) const
{
	inStream.Write(mPosition);
	inStream.Write(mLinearVelocity);
	inStream.Write(mContactNormal);
	inStream.Write(mSurfaceNormal);
	inStream.Write(mDistance);
	inStream.Write(mFraction);
	inStream.Write(mBodyB);
	inStream.Write(mSubShapeIDB);
	inStream.Write(mMotionTypeB);
	inStream.Write(mHadCollision);
	inStream.Write(mWasDiscarded);
	inStream.Write(mCanPushCharacter);
	// Cannot store user data (may be a pointer) and material
}

void CharacterVirtual::Contact::RestoreState(StateRecorder &inStream)
{
	inStream.Read(mPosition);
	inStream.Read(mLinearVelocity);
	inStream.Read(mContactNormal);
	inStream.Read(mSurfaceNormal);
	inStream.Read(mDistance);
	inStream.Read(mFraction);
	inStream.Read(mBodyB);
	inStream.Read(mSubShapeIDB);
	inStream.Read(mMotionTypeB);
	inStream.Read(mHadCollision);
	inStream.Read(mWasDiscarded);
	inStream.Read(mCanPushCharacter);
	mUserData = 0; // Cannot restore user data
	mMaterial = PhysicsMaterial::sDefault; // Cannot restore material
}

void CharacterVirtual::SaveState(StateRecorder &inStream) const
{
	CharacterBase::SaveState(inStream);

	inStream.Write(mPosition);
	inStream.Write(mRotation);
	inStream.Write(mLinearVelocity);
	inStream.Write(mLastDeltaTime);
	inStream.Write(mMaxHitsExceeded);

	// Store contacts that had collision, we're using it at the beginning of the step in CancelVelocityTowardsSteepSlopes
	uint32 num_contacts = 0;
	for (const Contact &c : mActiveContacts)
		if (c.mHadCollision)
			++num_contacts;
	inStream.Write(num_contacts);
	for (const Contact &c : mActiveContacts)
		if (c.mHadCollision)
			c.SaveState(inStream);
}

void CharacterVirtual::RestoreState(StateRecorder &inStream)
{
	CharacterBase::RestoreState(inStream);

	inStream.Read(mPosition);
	inStream.Read(mRotation);
	inStream.Read(mLinearVelocity);
	inStream.Read(mLastDeltaTime);
	inStream.Read(mMaxHitsExceeded);

	// When validating remove contacts that don't have collision since we didn't save them
	if (inStream.IsValidating())
		for (int i = (int)mActiveContacts.size() - 1; i >= 0; --i)
			if (!mActiveContacts[i].mHadCollision)
				mActiveContacts.erase(mActiveContacts.begin() + i);

	uint32 num_contacts = (uint32)mActiveContacts.size();
	inStream.Read(num_contacts);
	mActiveContacts.resize(num_contacts);
	for (Contact &c : mActiveContacts)
		c.RestoreState(inStream);
}

JPH_NAMESPACE_END

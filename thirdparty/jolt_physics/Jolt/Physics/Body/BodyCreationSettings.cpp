// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#include <Jolt/Jolt.h>

#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/ObjectStream/TypeDeclarations.h>
#include <Jolt/Core/StreamIn.h>
#include <Jolt/Core/StreamOut.h>

JPH_NAMESPACE_BEGIN

JPH_IMPLEMENT_SERIALIZABLE_NON_VIRTUAL(BodyCreationSettings)
{
	JPH_ADD_ATTRIBUTE(BodyCreationSettings, mPosition)
	JPH_ADD_ATTRIBUTE(BodyCreationSettings, mRotation)
	JPH_ADD_ATTRIBUTE(BodyCreationSettings, mLinearVelocity)
	JPH_ADD_ATTRIBUTE(BodyCreationSettings, mAngularVelocity)
	JPH_ADD_ATTRIBUTE(BodyCreationSettings, mUserData)
	JPH_ADD_ATTRIBUTE(BodyCreationSettings, mShape)
	JPH_ADD_ATTRIBUTE(BodyCreationSettings, mCollisionGroup)
	JPH_ADD_ENUM_ATTRIBUTE(BodyCreationSettings, mObjectLayer)
	JPH_ADD_ENUM_ATTRIBUTE(BodyCreationSettings, mMotionType)
	JPH_ADD_ENUM_ATTRIBUTE(BodyCreationSettings, mAllowedDOFs)
	JPH_ADD_ATTRIBUTE(BodyCreationSettings, mAllowDynamicOrKinematic)
	JPH_ADD_ATTRIBUTE(BodyCreationSettings, mIsSensor)
	JPH_ADD_ATTRIBUTE_WITH_ALIAS(BodyCreationSettings, mCollideKinematicVsNonDynamic, "mSensorDetectsStatic") // This is the old name to keep backwards compatibility
	JPH_ADD_ATTRIBUTE(BodyCreationSettings, mUseManifoldReduction)
	JPH_ADD_ATTRIBUTE(BodyCreationSettings, mApplyGyroscopicForce)
	JPH_ADD_ENUM_ATTRIBUTE(BodyCreationSettings, mMotionQuality)
	JPH_ADD_ATTRIBUTE(BodyCreationSettings, mEnhancedInternalEdgeRemoval)
	JPH_ADD_ATTRIBUTE(BodyCreationSettings, mAllowSleeping)
	JPH_ADD_ATTRIBUTE(BodyCreationSettings, mFriction)
	JPH_ADD_ATTRIBUTE(BodyCreationSettings, mRestitution)
	JPH_ADD_ATTRIBUTE(BodyCreationSettings, mLinearDamping)
	JPH_ADD_ATTRIBUTE(BodyCreationSettings, mAngularDamping)
	JPH_ADD_ATTRIBUTE(BodyCreationSettings, mMaxLinearVelocity)
	JPH_ADD_ATTRIBUTE(BodyCreationSettings, mMaxAngularVelocity)
	JPH_ADD_ATTRIBUTE(BodyCreationSettings, mGravityFactor)
	JPH_ADD_ATTRIBUTE(BodyCreationSettings, mNumVelocityStepsOverride)
	JPH_ADD_ATTRIBUTE(BodyCreationSettings, mNumPositionStepsOverride)
	JPH_ADD_ENUM_ATTRIBUTE(BodyCreationSettings, mOverrideMassProperties)
	JPH_ADD_ATTRIBUTE(BodyCreationSettings, mInertiaMultiplier)
	JPH_ADD_ATTRIBUTE(BodyCreationSettings, mMassPropertiesOverride)
}

void BodyCreationSettings::SaveBinaryState(StreamOut &inStream) const
{
	inStream.Write(mPosition);
	inStream.Write(mRotation);
	inStream.Write(mLinearVelocity);
	inStream.Write(mAngularVelocity);
	mCollisionGroup.SaveBinaryState(inStream);
	inStream.Write(mObjectLayer);
	inStream.Write(mMotionType);
	inStream.Write(mAllowedDOFs);
	inStream.Write(mAllowDynamicOrKinematic);
	inStream.Write(mIsSensor);
	inStream.Write(mCollideKinematicVsNonDynamic);
	inStream.Write(mUseManifoldReduction);
	inStream.Write(mApplyGyroscopicForce);
	inStream.Write(mMotionQuality);
	inStream.Write(mEnhancedInternalEdgeRemoval);
	inStream.Write(mAllowSleeping);
	inStream.Write(mFriction);
	inStream.Write(mRestitution);
	inStream.Write(mLinearDamping);
	inStream.Write(mAngularDamping);
	inStream.Write(mMaxLinearVelocity);
	inStream.Write(mMaxAngularVelocity);
	inStream.Write(mGravityFactor);
	inStream.Write(mNumVelocityStepsOverride);
	inStream.Write(mNumPositionStepsOverride);
	inStream.Write(mOverrideMassProperties);
	inStream.Write(mInertiaMultiplier);
	mMassPropertiesOverride.SaveBinaryState(inStream);
}

void BodyCreationSettings::RestoreBinaryState(StreamIn &inStream)
{
	inStream.Read(mPosition);
	inStream.Read(mRotation);
	inStream.Read(mLinearVelocity);
	inStream.Read(mAngularVelocity);
	mCollisionGroup.RestoreBinaryState(inStream);
	inStream.Read(mObjectLayer);
	inStream.Read(mMotionType);
	inStream.Read(mAllowedDOFs);
	inStream.Read(mAllowDynamicOrKinematic);
	inStream.Read(mIsSensor);
	inStream.Read(mCollideKinematicVsNonDynamic);
	inStream.Read(mUseManifoldReduction);
	inStream.Read(mApplyGyroscopicForce);
	inStream.Read(mMotionQuality);
	inStream.Read(mEnhancedInternalEdgeRemoval);
	inStream.Read(mAllowSleeping);
	inStream.Read(mFriction);
	inStream.Read(mRestitution);
	inStream.Read(mLinearDamping);
	inStream.Read(mAngularDamping);
	inStream.Read(mMaxLinearVelocity);
	inStream.Read(mMaxAngularVelocity);
	inStream.Read(mGravityFactor);
	inStream.Read(mNumVelocityStepsOverride);
	inStream.Read(mNumPositionStepsOverride);
	inStream.Read(mOverrideMassProperties);
	inStream.Read(mInertiaMultiplier);
	mMassPropertiesOverride.RestoreBinaryState(inStream);
}

Shape::ShapeResult BodyCreationSettings::ConvertShapeSettings()
{
	// If we already have a shape, return it
	if (mShapePtr != nullptr)
	{
		mShape = nullptr;

		Shape::ShapeResult result;
		result.Set(const_cast<Shape *>(mShapePtr.GetPtr()));
		return result;
	}

	// Check if we have shape settings
	if (mShape == nullptr)
	{
		Shape::ShapeResult result;
		result.SetError("No shape present!");
		return result;
	}

	// Create the shape
	Shape::ShapeResult result = mShape->Create();
	if (result.IsValid())
		mShapePtr = result.Get();
	mShape = nullptr;
	return result;
}

const Shape *BodyCreationSettings::GetShape() const
{
	// If we already have a shape, return it
	if (mShapePtr != nullptr)
		return mShapePtr;

	// Check if we have shape settings
	if (mShape == nullptr)
		return nullptr;

	// Create the shape
	Shape::ShapeResult result = mShape->Create();
	if (result.IsValid())
		return result.Get();

	Trace("Error: %s", result.GetError().c_str());
	JPH_ASSERT(false, "An error occurred during shape creation. Use ConvertShapeSettings() to convert the shape and get the error!");
	return nullptr;
}

MassProperties BodyCreationSettings::GetMassProperties() const
{
	// Calculate mass properties
	MassProperties mass_properties;
	switch (mOverrideMassProperties)
	{
	case EOverrideMassProperties::CalculateMassAndInertia:
		mass_properties = GetShape()->GetMassProperties();
		mass_properties.mInertia *= mInertiaMultiplier;
		mass_properties.mInertia(3, 3) = 1.0f;
		break;
	case EOverrideMassProperties::CalculateInertia:
		mass_properties = GetShape()->GetMassProperties();
		mass_properties.ScaleToMass(mMassPropertiesOverride.mMass);
		mass_properties.mInertia *= mInertiaMultiplier;
		mass_properties.mInertia(3, 3) = 1.0f;
		break;
	case EOverrideMassProperties::MassAndInertiaProvided:
		mass_properties = mMassPropertiesOverride;
		break;
	}
	return mass_properties;
}

void BodyCreationSettings::SaveWithChildren(StreamOut &inStream, ShapeToIDMap *ioShapeMap, MaterialToIDMap *ioMaterialMap, GroupFilterToIDMap *ioGroupFilterMap) const
{
	// Save creation settings
	SaveBinaryState(inStream);

	// Save shape
	if (ioShapeMap != nullptr && ioMaterialMap != nullptr)
		GetShape()->SaveWithChildren(inStream, *ioShapeMap, *ioMaterialMap);
	else
		inStream.Write(~uint32(0));

	// Save group filter
	StreamUtils::SaveObjectReference(inStream, mCollisionGroup.GetGroupFilter(), ioGroupFilterMap);
}

BodyCreationSettings::BCSResult BodyCreationSettings::sRestoreWithChildren(StreamIn &inStream, IDToShapeMap &ioShapeMap, IDToMaterialMap &ioMaterialMap, IDToGroupFilterMap &ioGroupFilterMap)
{
	BCSResult result;

	// Read creation settings
	BodyCreationSettings settings;
	settings.RestoreBinaryState(inStream);
	if (inStream.IsEOF() || inStream.IsFailed())
	{
		result.SetError("Error reading body creation settings");
		return result;
	}

	// Read shape
	Shape::ShapeResult shape_result = Shape::sRestoreWithChildren(inStream, ioShapeMap, ioMaterialMap);
	if (shape_result.HasError())
	{
		result.SetError(shape_result.GetError());
		return result;
	}
	settings.SetShape(shape_result.Get());

	// Read group filter
	Result gfresult = StreamUtils::RestoreObjectReference(inStream, ioGroupFilterMap);
	if (gfresult.HasError())
	{
		result.SetError(gfresult.GetError());
		return result;
	}
	settings.mCollisionGroup.SetGroupFilter(gfresult.Get());

	result.Set(settings);
	return result;
}

JPH_NAMESPACE_END

// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#include <Jolt/Jolt.h>

#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/RTTI.h>
#include <Jolt/Core/TickCounter.h>
#include <Jolt/Physics/Collision/CollisionDispatch.h>
#include <Jolt/Physics/Collision/Shape/TriangleShape.h>
#include <Jolt/Physics/Collision/Shape/PlaneShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/TaperedCapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/CylinderShape.h>
#include <Jolt/Physics/Collision/Shape/TaperedCylinderShape.h>
#include <Jolt/Physics/Collision/Shape/ScaledShape.h>
#include <Jolt/Physics/Collision/Shape/MeshShape.h>
#include <Jolt/Physics/Collision/Shape/ConvexHullShape.h>
#include <Jolt/Physics/Collision/Shape/HeightFieldShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>
#include <Jolt/Physics/Collision/Shape/OffsetCenterOfMassShape.h>
#include <Jolt/Physics/Collision/Shape/MutableCompoundShape.h>
#include <Jolt/Physics/Collision/Shape/StaticCompoundShape.h>
#include <Jolt/Physics/Collision/Shape/EmptyShape.h>
#include <Jolt/Physics/Collision/PhysicsMaterialSimple.h>
#include <Jolt/Physics/SoftBody/SoftBodyShape.h>

JPH_DECLARE_RTTI_WITH_NAMESPACE_FOR_FACTORY(JPH_EXPORT, JPH, Skeleton)
JPH_DECLARE_RTTI_WITH_NAMESPACE_FOR_FACTORY(JPH_EXPORT, JPH, SkeletalAnimation)
JPH_DECLARE_RTTI_WITH_NAMESPACE_FOR_FACTORY(JPH_EXPORT, JPH, RagdollSettings)
JPH_DECLARE_RTTI_WITH_NAMESPACE_FOR_FACTORY(JPH_EXPORT, JPH, PointConstraintSettings)
JPH_DECLARE_RTTI_WITH_NAMESPACE_FOR_FACTORY(JPH_EXPORT, JPH, SixDOFConstraintSettings)
JPH_DECLARE_RTTI_WITH_NAMESPACE_FOR_FACTORY(JPH_EXPORT, JPH, SliderConstraintSettings)
JPH_DECLARE_RTTI_WITH_NAMESPACE_FOR_FACTORY(JPH_EXPORT, JPH, SwingTwistConstraintSettings)
JPH_DECLARE_RTTI_WITH_NAMESPACE_FOR_FACTORY(JPH_EXPORT, JPH, DistanceConstraintSettings)
JPH_DECLARE_RTTI_WITH_NAMESPACE_FOR_FACTORY(JPH_EXPORT, JPH, HingeConstraintSettings)
JPH_DECLARE_RTTI_WITH_NAMESPACE_FOR_FACTORY(JPH_EXPORT, JPH, FixedConstraintSettings)
JPH_DECLARE_RTTI_WITH_NAMESPACE_FOR_FACTORY(JPH_EXPORT, JPH, ConeConstraintSettings)
JPH_DECLARE_RTTI_WITH_NAMESPACE_FOR_FACTORY(JPH_EXPORT, JPH, PathConstraintSettings)
JPH_DECLARE_RTTI_WITH_NAMESPACE_FOR_FACTORY(JPH_EXPORT, JPH, PathConstraintPath)
JPH_DECLARE_RTTI_WITH_NAMESPACE_FOR_FACTORY(JPH_EXPORT, JPH, PathConstraintPathHermite)
JPH_DECLARE_RTTI_WITH_NAMESPACE_FOR_FACTORY(JPH_EXPORT, JPH, VehicleConstraintSettings)
JPH_DECLARE_RTTI_WITH_NAMESPACE_FOR_FACTORY(JPH_EXPORT, JPH, WheeledVehicleControllerSettings)
JPH_DECLARE_RTTI_WITH_NAMESPACE_FOR_FACTORY(JPH_EXPORT, JPH, RackAndPinionConstraintSettings)
JPH_DECLARE_RTTI_WITH_NAMESPACE_FOR_FACTORY(JPH_EXPORT, JPH, GearConstraintSettings)
JPH_DECLARE_RTTI_WITH_NAMESPACE_FOR_FACTORY(JPH_EXPORT, JPH, PulleyConstraintSettings)
JPH_DECLARE_RTTI_WITH_NAMESPACE_FOR_FACTORY(JPH_EXPORT, JPH, MotorSettings)
JPH_DECLARE_RTTI_WITH_NAMESPACE_FOR_FACTORY(JPH_EXPORT, JPH, PhysicsScene)
JPH_DECLARE_RTTI_WITH_NAMESPACE_FOR_FACTORY(JPH_EXPORT, JPH, PhysicsMaterial)
JPH_DECLARE_RTTI_WITH_NAMESPACE_FOR_FACTORY(JPH_EXPORT, JPH, GroupFilter)
JPH_DECLARE_RTTI_WITH_NAMESPACE_FOR_FACTORY(JPH_EXPORT, JPH, GroupFilterTable)
JPH_DECLARE_RTTI_WITH_NAMESPACE_FOR_FACTORY(JPH_EXPORT, JPH, BodyCreationSettings)
JPH_DECLARE_RTTI_WITH_NAMESPACE_FOR_FACTORY(JPH_EXPORT, JPH, SoftBodyCreationSettings)

JPH_NAMESPACE_BEGIN

bool VerifyJoltVersionIDInternal(uint64 inVersionID)
{
	return inVersionID == JPH_VERSION_ID;
}

void RegisterTypesInternal(uint64 inVersionID)
{
	// Version check
	if (!VerifyJoltVersionIDInternal(inVersionID))
	{
		Trace("Version mismatch, make sure you compile the client code with the same Jolt version and compiler definitions!");
		uint64 mismatch = JPH_VERSION_ID ^ inVersionID;
		auto check_bit = [mismatch](int inBit, const char *inLabel) { if (mismatch & (uint64(1) << (inBit + 23))) Trace("Mismatching define %s.", inLabel); };
		check_bit(1, "JPH_DOUBLE_PRECISION");
		check_bit(2, "JPH_CROSS_PLATFORM_DETERMINISTIC");
		check_bit(3, "JPH_FLOATING_POINT_EXCEPTIONS_ENABLED");
		check_bit(4, "JPH_PROFILE_ENABLED");
		check_bit(5, "JPH_EXTERNAL_PROFILE");
		check_bit(6, "JPH_DEBUG_RENDERER");
		check_bit(7, "JPH_DISABLE_TEMP_ALLOCATOR");
		check_bit(8, "JPH_DISABLE_CUSTOM_ALLOCATOR");
		check_bit(9, "JPH_OBJECT_LAYER_BITS");
		check_bit(10, "JPH_ENABLE_ASSERTS");
		check_bit(11, "JPH_OBJECT_STREAM");
		std::abort();
	}

#ifndef JPH_DISABLE_CUSTOM_ALLOCATOR
	JPH_ASSERT(Allocate != nullptr && Reallocate != nullptr && Free != nullptr && AlignedAllocate != nullptr && AlignedFree != nullptr, "Need to supply an allocator first or call RegisterDefaultAllocator()");
#endif // !JPH_DISABLE_CUSTOM_ALLOCATOR

	JPH_ASSERT(Factory::sInstance != nullptr, "Need to create a factory first!");

	// Initialize dispatcher
	CollisionDispatch::sInit();

	// Register base classes first so that we can specialize them later
	CompoundShape::sRegister();
	ConvexShape::sRegister();

	// Register compounds before others so that we can specialize them later (register them in reverse order of collision complexity)
	MutableCompoundShape::sRegister();
	StaticCompoundShape::sRegister();

	// Leaf classes
	TriangleShape::sRegister();
	PlaneShape::sRegister();
	SphereShape::sRegister();
	BoxShape::sRegister();
	CapsuleShape::sRegister();
	TaperedCapsuleShape::sRegister();
	CylinderShape::sRegister();
	TaperedCylinderShape::sRegister();
	MeshShape::sRegister();
	ConvexHullShape::sRegister();
	HeightFieldShape::sRegister();
	SoftBodyShape::sRegister();

	// Register these last because their collision functions are simple so we want to execute them first (register them in reverse order of collision complexity)
	RotatedTranslatedShape::sRegister();
	OffsetCenterOfMassShape::sRegister();
	ScaledShape::sRegister();
	EmptyShape::sRegister();

	// Create list of all types
	const RTTI *types[] = {
		JPH_RTTI(SkeletalAnimation),
		JPH_RTTI(Skeleton),
		JPH_RTTI(CompoundShapeSettings),
		JPH_RTTI(StaticCompoundShapeSettings),
		JPH_RTTI(MutableCompoundShapeSettings),
		JPH_RTTI(TriangleShapeSettings),
		JPH_RTTI(PlaneShapeSettings),
		JPH_RTTI(SphereShapeSettings),
		JPH_RTTI(BoxShapeSettings),
		JPH_RTTI(CapsuleShapeSettings),
		JPH_RTTI(TaperedCapsuleShapeSettings),
		JPH_RTTI(CylinderShapeSettings),
		JPH_RTTI(TaperedCylinderShapeSettings),
		JPH_RTTI(ScaledShapeSettings),
		JPH_RTTI(MeshShapeSettings),
		JPH_RTTI(ConvexHullShapeSettings),
		JPH_RTTI(HeightFieldShapeSettings),
		JPH_RTTI(RotatedTranslatedShapeSettings),
		JPH_RTTI(OffsetCenterOfMassShapeSettings),
		JPH_RTTI(EmptyShapeSettings),
		JPH_RTTI(RagdollSettings),
		JPH_RTTI(PointConstraintSettings),
		JPH_RTTI(SixDOFConstraintSettings),
		JPH_RTTI(SliderConstraintSettings),
		JPH_RTTI(SwingTwistConstraintSettings),
		JPH_RTTI(DistanceConstraintSettings),
		JPH_RTTI(HingeConstraintSettings),
		JPH_RTTI(FixedConstraintSettings),
		JPH_RTTI(ConeConstraintSettings),
		JPH_RTTI(PathConstraintSettings),
		JPH_RTTI(VehicleConstraintSettings),
		JPH_RTTI(WheeledVehicleControllerSettings),
		JPH_RTTI(PathConstraintPath),
		JPH_RTTI(PathConstraintPathHermite),
		JPH_RTTI(RackAndPinionConstraintSettings),
		JPH_RTTI(GearConstraintSettings),
		JPH_RTTI(PulleyConstraintSettings),
		JPH_RTTI(MotorSettings),
		JPH_RTTI(PhysicsScene),
		JPH_RTTI(PhysicsMaterial),
		JPH_RTTI(PhysicsMaterialSimple),
		JPH_RTTI(GroupFilter),
		JPH_RTTI(GroupFilterTable),
		JPH_RTTI(BodyCreationSettings),
		JPH_RTTI(SoftBodyCreationSettings)
	};

	// Register them all
	Factory::sInstance->Register(types, (uint)std::size(types));

	// Initialize default physics material
	if (PhysicsMaterial::sDefault == nullptr)
		PhysicsMaterial::sDefault = new PhysicsMaterialSimple("Default", Color::sGrey);
}

void UnregisterTypes()
{
	// Unregister all types
	if (Factory::sInstance != nullptr)
		Factory::sInstance->Clear();

	// Delete default physics material
	PhysicsMaterial::sDefault = nullptr;
}

JPH_NAMESPACE_END

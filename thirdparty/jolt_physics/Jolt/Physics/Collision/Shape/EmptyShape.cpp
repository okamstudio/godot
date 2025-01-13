// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2024 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#include <Jolt/Jolt.h>

#include <Jolt/Physics/Collision/Shape/EmptyShape.h>
#include <Jolt/Physics/Collision/CollisionDispatch.h>
#include <Jolt/ObjectStream/TypeDeclarations.h>
#ifdef JPH_DEBUG_RENDERER
	#include <Jolt/Renderer/DebugRenderer.h>
#endif // JPH_DEBUG_RENDERER

JPH_NAMESPACE_BEGIN

JPH_IMPLEMENT_SERIALIZABLE_VIRTUAL(EmptyShapeSettings)
{
	JPH_ADD_BASE_CLASS(EmptyShapeSettings, ShapeSettings)

	JPH_ADD_ATTRIBUTE(EmptyShapeSettings, mCenterOfMass)
}

ShapeSettings::ShapeResult EmptyShapeSettings::Create() const
{
	if (mCachedResult.IsEmpty())
		new EmptyShape(*this, mCachedResult);

	return mCachedResult;
}

MassProperties EmptyShape::GetMassProperties() const
{
	MassProperties mass_properties;
	mass_properties.mMass = 1.0f;
	mass_properties.mInertia = Mat44::sIdentity();
	return mass_properties;
}

#ifdef JPH_DEBUG_RENDERER
void EmptyShape::Draw(DebugRenderer *inRenderer, RMat44Arg inCenterOfMassTransform, Vec3Arg inScale, ColorArg inColor, [[maybe_unused]] bool inUseMaterialColors, [[maybe_unused]] bool inDrawWireframe) const
{
	inRenderer->DrawMarker(inCenterOfMassTransform.GetTranslation(), inColor, abs(inScale.GetX()) * 0.1f);
}
#endif // JPH_DEBUG_RENDERER

void EmptyShape::sRegister()
{
	ShapeFunctions &f = ShapeFunctions::sGet(EShapeSubType::Empty);
	f.mConstruct = []() -> Shape * { return new EmptyShape; };
	f.mColor = Color::sBlack;

	auto collide_empty = []([[maybe_unused]] const Shape *inShape1, [[maybe_unused]] const Shape *inShape2, [[maybe_unused]] Vec3Arg inScale1, [[maybe_unused]] Vec3Arg inScale2, [[maybe_unused]] Mat44Arg inCenterOfMassTransform1, [[maybe_unused]] Mat44Arg inCenterOfMassTransform2, [[maybe_unused]] const SubShapeIDCreator &inSubShapeIDCreator1, [[maybe_unused]] const SubShapeIDCreator &inSubShapeIDCreator2, [[maybe_unused]] const CollideShapeSettings &inCollideShapeSettings, [[maybe_unused]] CollideShapeCollector &ioCollector, [[maybe_unused]] const ShapeFilter &inShapeFilter) { /* Do Nothing */ };
	auto cast_empty = []([[maybe_unused]] const ShapeCast &inShapeCast, [[maybe_unused]] const ShapeCastSettings &inShapeCastSettings, [[maybe_unused]] const Shape *inShape, [[maybe_unused]] Vec3Arg inScale, [[maybe_unused]] const ShapeFilter &inShapeFilter, [[maybe_unused]] Mat44Arg inCenterOfMassTransform2, [[maybe_unused]] const SubShapeIDCreator &inSubShapeIDCreator1, [[maybe_unused]] const SubShapeIDCreator &inSubShapeIDCreator2, [[maybe_unused]] CastShapeCollector &ioCollector) { /* Do nothing */ };

	for (const EShapeSubType s : sAllSubShapeTypes)
	{
		CollisionDispatch::sRegisterCollideShape(EShapeSubType::Empty, s, collide_empty);
		CollisionDispatch::sRegisterCollideShape(s, EShapeSubType::Empty, collide_empty);

		CollisionDispatch::sRegisterCastShape(EShapeSubType::Empty, s, cast_empty);
		CollisionDispatch::sRegisterCastShape(s, EShapeSubType::Empty, cast_empty);
	}
}

JPH_NAMESPACE_END

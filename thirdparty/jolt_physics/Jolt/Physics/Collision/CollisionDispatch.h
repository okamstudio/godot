// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Jolt/Physics/Collision/Shape/Shape.h>
#include <Jolt/Physics/Collision/Shape/SubShapeID.h>
#include <Jolt/Physics/Collision/ShapeCast.h>
#include <Jolt/Physics/Collision/ShapeFilter.h>
#include <Jolt/Physics/Collision/NarrowPhaseStats.h>

JPH_NAMESPACE_BEGIN

class CollideShapeSettings;

/// Dispatch function, main function to handle collisions between shapes
class JPH_EXPORT CollisionDispatch
{
public:
	/// Collide 2 shapes and pass any collision on to ioCollector
	/// @param inShape1 The first shape
	/// @param inShape2 The second shape
	/// @param inScale1 Local space scale of shape 1 (scales relative to its center of mass)
	/// @param inScale2 Local space scale of shape 2 (scales relative to its center of mass)
	/// @param inCenterOfMassTransform1 Transform to transform center of mass of shape 1 into world space
	/// @param inCenterOfMassTransform2 Transform to transform center of mass of shape 2 into world space
	/// @param inSubShapeIDCreator1 Class that tracks the current sub shape ID for shape 1
	/// @param inSubShapeIDCreator2 Class that tracks the current sub shape ID for shape 2
	/// @param inCollideShapeSettings Options for the CollideShape test
	/// @param ioCollector The collector that receives the results.
	/// @param inShapeFilter allows selectively disabling collisions between pairs of (sub) shapes.
	static inline void		sCollideShapeVsShape(const Shape *inShape1, const Shape *inShape2, Vec3Arg inScale1, Vec3Arg inScale2, Mat44Arg inCenterOfMassTransform1, Mat44Arg inCenterOfMassTransform2, const SubShapeIDCreator &inSubShapeIDCreator1, const SubShapeIDCreator &inSubShapeIDCreator2, const CollideShapeSettings &inCollideShapeSettings, CollideShapeCollector &ioCollector, const ShapeFilter &inShapeFilter = { })
	{
		JPH_IF_TRACK_NARROWPHASE_STATS(TrackNarrowPhaseStat track(NarrowPhaseStat::sCollideShape[(int)inShape1->GetSubType()][(int)inShape2->GetSubType()]);)

		// Only test shape if it passes the shape filter
		if (inShapeFilter.ShouldCollide(inShape1, inSubShapeIDCreator1.GetID(), inShape2, inSubShapeIDCreator2.GetID()))
			sCollideShape[(int)inShape1->GetSubType()][(int)inShape2->GetSubType()](inShape1, inShape2, inScale1, inScale2, inCenterOfMassTransform1, inCenterOfMassTransform2, inSubShapeIDCreator1, inSubShapeIDCreator2, inCollideShapeSettings, ioCollector, inShapeFilter);
	}

	/// Cast a shape against this shape, passes any hits found to ioCollector.
	/// Note: This version takes the shape cast in local space relative to the center of mass of inShape, take a look at sCastShapeVsShapeWorldSpace if you have a shape cast in world space.
	/// @param inShapeCastLocal The shape to cast against the other shape and its start and direction.
	/// @param inShapeCastSettings Settings for performing the cast
	/// @param inShape The shape to cast against.
	/// @param inScale Local space scale for the shape to cast against (scales relative to its center of mass).
	/// @param inShapeFilter allows selectively disabling collisions between pairs of (sub) shapes.
	/// @param inCenterOfMassTransform2 Is the center of mass transform of shape 2 (excluding scale), this is used to provide a transform to the shape cast result so that local hit result quantities can be transformed into world space.
	/// @param inSubShapeIDCreator1 Class that tracks the current sub shape ID for the casting shape
	/// @param inSubShapeIDCreator2 Class that tracks the current sub shape ID for the shape we're casting against
	/// @param ioCollector The collector that receives the results.
	static inline void		sCastShapeVsShapeLocalSpace(const ShapeCast &inShapeCastLocal, const ShapeCastSettings &inShapeCastSettings, const Shape *inShape, Vec3Arg inScale, const ShapeFilter &inShapeFilter, Mat44Arg inCenterOfMassTransform2, const SubShapeIDCreator &inSubShapeIDCreator1, const SubShapeIDCreator &inSubShapeIDCreator2, CastShapeCollector &ioCollector)
	{
		JPH_IF_TRACK_NARROWPHASE_STATS(TrackNarrowPhaseStat track(NarrowPhaseStat::sCastShape[(int)inShapeCastLocal.mShape->GetSubType()][(int)inShape->GetSubType()]);)

		// Only test shape if it passes the shape filter
		if (inShapeFilter.ShouldCollide(inShapeCastLocal.mShape, inSubShapeIDCreator1.GetID(), inShape, inSubShapeIDCreator2.GetID()))
			sCastShape[(int)inShapeCastLocal.mShape->GetSubType()][(int)inShape->GetSubType()](inShapeCastLocal, inShapeCastSettings, inShape, inScale, inShapeFilter, inCenterOfMassTransform2, inSubShapeIDCreator1, inSubShapeIDCreator2, ioCollector);
	}

	/// See: sCastShapeVsShapeLocalSpace.
	/// The only difference is that the shape cast (inShapeCastWorld) is provided in world space.
	/// Note: A shape cast contains the center of mass start of the shape, if you have the world transform of the shape you probably want to construct it using ShapeCast::sFromWorldTransform.
	static inline void		sCastShapeVsShapeWorldSpace(const ShapeCast &inShapeCastWorld, const ShapeCastSettings &inShapeCastSettings, const Shape *inShape, Vec3Arg inScale, const ShapeFilter &inShapeFilter, Mat44Arg inCenterOfMassTransform2, const SubShapeIDCreator &inSubShapeIDCreator1, const SubShapeIDCreator &inSubShapeIDCreator2, CastShapeCollector &ioCollector)
	{
		ShapeCast local_shape_cast = inShapeCastWorld.PostTransformed(inCenterOfMassTransform2.InversedRotationTranslation());
		sCastShapeVsShapeLocalSpace(local_shape_cast, inShapeCastSettings, inShape, inScale, inShapeFilter, inCenterOfMassTransform2, inSubShapeIDCreator1, inSubShapeIDCreator2, ioCollector);
	}

	/// Function that collides 2 shapes (see sCollideShapeVsShape)
	using CollideShape = void (*)(const Shape *inShape1, const Shape *inShape2, Vec3Arg inScale1, Vec3Arg inScale2, Mat44Arg inCenterOfMassTransform1, Mat44Arg inCenterOfMassTransform2, const SubShapeIDCreator &inSubShapeIDCreator1, const SubShapeIDCreator &inSubShapeIDCreator2, const CollideShapeSettings &inCollideShapeSettings, CollideShapeCollector &ioCollector, const ShapeFilter &inShapeFilter);

	/// Function that casts a shape vs another shape (see sCastShapeVsShapeLocalSpace)
	using CastShape = void (*)(const ShapeCast &inShapeCast, const ShapeCastSettings &inShapeCastSettings, const Shape *inShape, Vec3Arg inScale, const ShapeFilter &inShapeFilter, Mat44Arg inCenterOfMassTransform2, const SubShapeIDCreator &inSubShapeIDCreator1, const SubShapeIDCreator &inSubShapeIDCreator2, CastShapeCollector &ioCollector);

	/// Initialize all collision functions with a function that asserts and returns no collision
	static void				sInit();

	/// Register a collide shape function in the collision table
	static void				sRegisterCollideShape(EShapeSubType inType1, EShapeSubType inType2, CollideShape inFunction)	{ sCollideShape[(int)inType1][(int)inType2] = inFunction; }

	/// Register a cast shape function in the collision table
	static void				sRegisterCastShape(EShapeSubType inType1, EShapeSubType inType2, CastShape inFunction)			{ sCastShape[(int)inType1][(int)inType2] = inFunction; }

	/// An implementation of CollideShape that swaps inShape1 and inShape2 and swaps the result back, can be registered if the collision function only exists the other way around
	static void				sReversedCollideShape(const Shape *inShape1, const Shape *inShape2, Vec3Arg inScale1, Vec3Arg inScale2, Mat44Arg inCenterOfMassTransform1, Mat44Arg inCenterOfMassTransform2, const SubShapeIDCreator &inSubShapeIDCreator1, const SubShapeIDCreator &inSubShapeIDCreator2, const CollideShapeSettings &inCollideShapeSettings, CollideShapeCollector &ioCollector, const ShapeFilter &inShapeFilter);

	/// An implementation of CastShape that swaps inShape1 and inShape2 and swaps the result back, can be registered if the collision function only exists the other way around
	static void				sReversedCastShape(const ShapeCast &inShapeCast, const ShapeCastSettings &inShapeCastSettings, const Shape *inShape, Vec3Arg inScale, const ShapeFilter &inShapeFilter, Mat44Arg inCenterOfMassTransform2, const SubShapeIDCreator &inSubShapeIDCreator1, const SubShapeIDCreator &inSubShapeIDCreator2, CastShapeCollector &ioCollector);

private:
	static CollideShape		sCollideShape[NumSubShapeTypes][NumSubShapeTypes];
	static CastShape		sCastShape[NumSubShapeTypes][NumSubShapeTypes];
};

JPH_NAMESPACE_END

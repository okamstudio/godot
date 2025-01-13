// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Jolt/Physics/Collision/Shape/Shape.h>

JPH_NAMESPACE_BEGIN

/// Class that constructs a DecoratedShape
class JPH_EXPORT DecoratedShapeSettings : public ShapeSettings
{
	JPH_DECLARE_SERIALIZABLE_VIRTUAL(JPH_EXPORT, DecoratedShapeSettings)

public:
	/// Default constructor for deserialization
									DecoratedShapeSettings() = default;

	/// Constructor that decorates another shape
	explicit						DecoratedShapeSettings(const ShapeSettings *inShape)	: mInnerShape(inShape) { }
	explicit						DecoratedShapeSettings(const Shape *inShape)			: mInnerShapePtr(inShape) { }

	RefConst<ShapeSettings>			mInnerShape;											///< Sub shape (either this or mShapePtr needs to be filled up)
	RefConst<Shape>					mInnerShapePtr;											///< Sub shape (either this or mShape needs to be filled up)
};

/// Base class for shapes that decorate another shape with extra functionality (e.g. scale, translation etc.)
class JPH_EXPORT DecoratedShape : public Shape
{
public:
	JPH_OVERRIDE_NEW_DELETE

	/// Constructor
	explicit						DecoratedShape(EShapeSubType inSubType) : Shape(EShapeType::Decorated, inSubType) { }
									DecoratedShape(EShapeSubType inSubType, const Shape *inInnerShape) : Shape(EShapeType::Decorated, inSubType), mInnerShape(inInnerShape) { }
									DecoratedShape(EShapeSubType inSubType, const DecoratedShapeSettings &inSettings, ShapeResult &outResult);

	/// Access to the decorated inner shape
	const Shape *					GetInnerShape() const									{ return mInnerShape; }

	// See Shape::MustBeStatic
	virtual bool					MustBeStatic() const override							{ return mInnerShape->MustBeStatic(); }

	// See Shape::GetCenterOfMass
	virtual Vec3					GetCenterOfMass() const override						{ return mInnerShape->GetCenterOfMass(); }

	// See Shape::GetSubShapeIDBitsRecursive
	virtual uint					GetSubShapeIDBitsRecursive() const override				{ return mInnerShape->GetSubShapeIDBitsRecursive(); }

	// See Shape::GetLeafShape
	virtual const Shape *			GetLeafShape(const SubShapeID &inSubShapeID, SubShapeID &outRemainder) const override { return mInnerShape->GetLeafShape(inSubShapeID, outRemainder); }

	// See Shape::GetMaterial
	virtual const PhysicsMaterial *	GetMaterial(const SubShapeID &inSubShapeID) const override;

	// See Shape::GetSupportingFace
	virtual void					GetSupportingFace(const SubShapeID &inSubShapeID, Vec3Arg inDirection, Vec3Arg inScale, Mat44Arg inCenterOfMassTransform, SupportingFace &outVertices) const override;

	// See Shape::GetSubShapeUserData
	virtual uint64					GetSubShapeUserData(const SubShapeID &inSubShapeID) const override;

	// See Shape
	virtual void					SaveSubShapeState(ShapeList &outSubShapes) const override;
	virtual void					RestoreSubShapeState(const ShapeRefC *inSubShapes, uint inNumShapes) override;

	// See Shape::GetStatsRecursive
	virtual Stats					GetStatsRecursive(VisitedShapes &ioVisitedShapes) const override;

	// See Shape::IsValidScale
	virtual bool					IsValidScale(Vec3Arg inScale) const override			{ return mInnerShape->IsValidScale(inScale); }

	// See Shape::MakeScaleValid
	virtual Vec3					MakeScaleValid(Vec3Arg inScale) const override			{ return mInnerShape->MakeScaleValid(inScale); }

protected:
	RefConst<Shape>					mInnerShape;
};

JPH_NAMESPACE_END

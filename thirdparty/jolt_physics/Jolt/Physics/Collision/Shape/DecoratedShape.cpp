// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#include <Jolt/Jolt.h>

#include <Jolt/Physics/Collision/Shape/DecoratedShape.h>
#include <Jolt/ObjectStream/TypeDeclarations.h>

JPH_NAMESPACE_BEGIN

JPH_IMPLEMENT_SERIALIZABLE_ABSTRACT(DecoratedShapeSettings)
{
	JPH_ADD_BASE_CLASS(DecoratedShapeSettings, ShapeSettings)

	JPH_ADD_ATTRIBUTE(DecoratedShapeSettings, mInnerShape)
}

DecoratedShape::DecoratedShape(EShapeSubType inSubType, const DecoratedShapeSettings &inSettings, ShapeResult &outResult) :
	Shape(EShapeType::Decorated, inSubType, inSettings, outResult)
{
	// Check that there's a shape
	if (inSettings.mInnerShape == nullptr && inSettings.mInnerShapePtr == nullptr)
	{
		outResult.SetError("Inner shape is null!");
		return;
	}

	if (inSettings.mInnerShapePtr != nullptr)
	{
		// Use provided shape
		mInnerShape = inSettings.mInnerShapePtr;
	}
	else
	{
		// Create child shape
		ShapeResult child_result = inSettings.mInnerShape->Create();
		if (!child_result.IsValid())
		{
			outResult = child_result;
			return;
		}
		mInnerShape = child_result.Get();
	}
}

const PhysicsMaterial *DecoratedShape::GetMaterial(const SubShapeID &inSubShapeID) const
{
	return mInnerShape->GetMaterial(inSubShapeID);
}

void DecoratedShape::GetSupportingFace(const SubShapeID &inSubShapeID, Vec3Arg inDirection, Vec3Arg inScale, Mat44Arg inCenterOfMassTransform, SupportingFace &outVertices) const
{
	mInnerShape->GetSupportingFace(inSubShapeID, inDirection, inScale, inCenterOfMassTransform, outVertices);
}

uint64 DecoratedShape::GetSubShapeUserData(const SubShapeID &inSubShapeID) const
{
	return mInnerShape->GetSubShapeUserData(inSubShapeID);
}

void DecoratedShape::SaveSubShapeState(ShapeList &outSubShapes) const
{
	outSubShapes.clear();
	outSubShapes.push_back(mInnerShape);
}

void DecoratedShape::RestoreSubShapeState(const ShapeRefC *inSubShapes, uint inNumShapes)
{
	JPH_ASSERT(inNumShapes == 1);
	mInnerShape = inSubShapes[0];
}

Shape::Stats DecoratedShape::GetStatsRecursive(VisitedShapes &ioVisitedShapes) const
{
	// Get own stats
	Stats stats = Shape::GetStatsRecursive(ioVisitedShapes);

	// Add child stats
	Stats child_stats = mInnerShape->GetStatsRecursive(ioVisitedShapes);
	stats.mSizeBytes += child_stats.mSizeBytes;
	stats.mNumTriangles += child_stats.mNumTriangles;

	return stats;
}

JPH_NAMESPACE_END

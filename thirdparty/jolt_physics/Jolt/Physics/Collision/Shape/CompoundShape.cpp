// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#include <Jolt/Jolt.h>

#include <Jolt/Physics/Collision/Shape/CompoundShape.h>
#include <Jolt/Physics/Collision/CollisionDispatch.h>
#include <Jolt/Physics/Collision/ShapeCast.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/TransformedShape.h>
#include <Jolt/Core/Profiler.h>
#include <Jolt/Core/StreamIn.h>
#include <Jolt/Core/StreamOut.h>
#include <Jolt/ObjectStream/TypeDeclarations.h>
#ifdef JPH_DEBUG_RENDERER
	#include <Jolt/Renderer/DebugRenderer.h>
#endif // JPH_DEBUG_RENDERER

JPH_NAMESPACE_BEGIN

JPH_IMPLEMENT_SERIALIZABLE_ABSTRACT(CompoundShapeSettings)
{
	JPH_ADD_BASE_CLASS(CompoundShapeSettings, ShapeSettings)

	JPH_ADD_ATTRIBUTE(CompoundShapeSettings, mSubShapes)
}

JPH_IMPLEMENT_SERIALIZABLE_NON_VIRTUAL(CompoundShapeSettings::SubShapeSettings)
{
	JPH_ADD_ATTRIBUTE(CompoundShapeSettings::SubShapeSettings, mShape)
	JPH_ADD_ATTRIBUTE(CompoundShapeSettings::SubShapeSettings, mPosition)
	JPH_ADD_ATTRIBUTE(CompoundShapeSettings::SubShapeSettings, mRotation)
	JPH_ADD_ATTRIBUTE(CompoundShapeSettings::SubShapeSettings, mUserData)
}

void CompoundShapeSettings::AddShape(Vec3Arg inPosition, QuatArg inRotation, const ShapeSettings *inShape, uint32 inUserData)
{
	// Add shape
	SubShapeSettings shape;
	shape.mPosition = inPosition;
	shape.mRotation = inRotation;
	shape.mShape = inShape;
	shape.mUserData = inUserData;
	mSubShapes.push_back(shape);
}

void CompoundShapeSettings::AddShape(Vec3Arg inPosition, QuatArg inRotation, const Shape *inShape, uint32 inUserData)
{
	// Add shape
	SubShapeSettings shape;
	shape.mPosition = inPosition;
	shape.mRotation = inRotation;
	shape.mShapePtr = inShape;
	shape.mUserData = inUserData;
	mSubShapes.push_back(shape);
}

bool CompoundShape::MustBeStatic() const
{
	for (const SubShape &shape : mSubShapes)
		if (shape.mShape->MustBeStatic())
			return true;

	return false;
}

MassProperties CompoundShape::GetMassProperties() const
{
	MassProperties p;

	// Calculate mass and inertia
	p.mMass = 0.0f;
	p.mInertia = Mat44::sZero();
	for (const SubShape &shape : mSubShapes)
	{
		// Rotate and translate inertia of child into place
		MassProperties child = shape.mShape->GetMassProperties();
		child.Rotate(Mat44::sRotation(shape.GetRotation()));
		child.Translate(shape.GetPositionCOM());

		// Accumulate mass and inertia
		p.mMass += child.mMass;
		p.mInertia += child.mInertia;
	}

	// Ensure that inertia is a 3x3 matrix, adding inertias causes the bottom right element to change
	p.mInertia.SetColumn4(3, Vec4(0, 0, 0, 1));

	return p;
}

AABox CompoundShape::GetWorldSpaceBounds(Mat44Arg inCenterOfMassTransform, Vec3Arg inScale) const
{
	if (mSubShapes.size() <= 10)
	{
		AABox bounds;
		for (const SubShape &shape : mSubShapes)
		{
			Mat44 transform = inCenterOfMassTransform * shape.GetLocalTransformNoScale(inScale);
			bounds.Encapsulate(shape.mShape->GetWorldSpaceBounds(transform, shape.TransformScale(inScale)));
		}
		return bounds;
	}
	else
	{
		// If there are too many shapes, use the base class function (this will result in a slightly wider bounding box)
		return Shape::GetWorldSpaceBounds(inCenterOfMassTransform, inScale);
	}
}

uint CompoundShape::GetSubShapeIDBitsRecursive() const
{
	// Add max of child bits to our bits
	uint child_bits = 0;
	for (const SubShape &shape : mSubShapes)
		child_bits = max(child_bits, shape.mShape->GetSubShapeIDBitsRecursive());
	return child_bits + GetSubShapeIDBits();
}

const PhysicsMaterial *CompoundShape::GetMaterial(const SubShapeID &inSubShapeID) const
{
	// Decode sub shape index
	SubShapeID remainder;
	uint32 index = GetSubShapeIndexFromID(inSubShapeID, remainder);

	// Pass call on
	return mSubShapes[index].mShape->GetMaterial(remainder);
}

const Shape *CompoundShape::GetLeafShape(const SubShapeID &inSubShapeID, SubShapeID &outRemainder) const
{
	// Decode sub shape index
	SubShapeID remainder;
	uint32 index = GetSubShapeIndexFromID(inSubShapeID, remainder);
	if (index >= mSubShapes.size())
	{
		// No longer valid index
		outRemainder = SubShapeID();
		return nullptr;
	}

	// Pass call on
	return mSubShapes[index].mShape->GetLeafShape(remainder, outRemainder);
}

uint64 CompoundShape::GetSubShapeUserData(const SubShapeID &inSubShapeID) const
{
	// Decode sub shape index
	SubShapeID remainder;
	uint32 index = GetSubShapeIndexFromID(inSubShapeID, remainder);
	if (index >= mSubShapes.size())
		return 0; // No longer valid index

	// Pass call on
	return mSubShapes[index].mShape->GetSubShapeUserData(remainder);
}

TransformedShape CompoundShape::GetSubShapeTransformedShape(const SubShapeID &inSubShapeID, Vec3Arg inPositionCOM, QuatArg inRotation, Vec3Arg inScale, SubShapeID &outRemainder) const
{
	// Get the sub shape
	const SubShape &sub_shape = mSubShapes[GetSubShapeIndexFromID(inSubShapeID, outRemainder)];

	// Calculate transform for sub shape
	Vec3 position = inPositionCOM + inRotation * (inScale * sub_shape.GetPositionCOM());
	Quat rotation = inRotation * sub_shape.GetRotation();
	Vec3 scale = sub_shape.TransformScale(inScale);

	// Return transformed shape
	TransformedShape ts(RVec3(position), rotation, sub_shape.mShape, BodyID());
	ts.SetShapeScale(scale);
	return ts;
}

Vec3 CompoundShape::GetSurfaceNormal(const SubShapeID &inSubShapeID, Vec3Arg inLocalSurfacePosition) const
{
	// Decode sub shape index
	SubShapeID remainder;
	uint32 index = GetSubShapeIndexFromID(inSubShapeID, remainder);

	// Transform surface position to local space and pass call on
	const SubShape &shape = mSubShapes[index];
	Mat44 transform = Mat44::sInverseRotationTranslation(shape.GetRotation(), shape.GetPositionCOM());
	Vec3 normal = shape.mShape->GetSurfaceNormal(remainder, transform * inLocalSurfacePosition);

	// Transform normal to this shape's space
	return transform.Multiply3x3Transposed(normal);
}

void CompoundShape::GetSupportingFace(const SubShapeID &inSubShapeID, Vec3Arg inDirection, Vec3Arg inScale, Mat44Arg inCenterOfMassTransform, SupportingFace &outVertices) const
{
	// Decode sub shape index
	SubShapeID remainder;
	uint32 index = GetSubShapeIndexFromID(inSubShapeID, remainder);

	// Apply transform and pass on to sub shape
	const SubShape &shape = mSubShapes[index];
	Mat44 transform = shape.GetLocalTransformNoScale(inScale);
	shape.mShape->GetSupportingFace(remainder, transform.Multiply3x3Transposed(inDirection), shape.TransformScale(inScale), inCenterOfMassTransform * transform, outVertices);
}

void CompoundShape::GetSubmergedVolume(Mat44Arg inCenterOfMassTransform, Vec3Arg inScale, const Plane &inSurface, float &outTotalVolume, float &outSubmergedVolume, Vec3 &outCenterOfBuoyancy JPH_IF_DEBUG_RENDERER(, RVec3Arg inBaseOffset)) const
{
	outTotalVolume = 0.0f;
	outSubmergedVolume = 0.0f;
	outCenterOfBuoyancy = Vec3::sZero();

	for (const SubShape &shape : mSubShapes)
	{
		// Get center of mass transform of child
		Mat44 transform = inCenterOfMassTransform * shape.GetLocalTransformNoScale(inScale);

		// Recurse to child
		float total_volume, submerged_volume;
		Vec3 center_of_buoyancy;
		shape.mShape->GetSubmergedVolume(transform, shape.TransformScale(inScale), inSurface, total_volume, submerged_volume, center_of_buoyancy JPH_IF_DEBUG_RENDERER(, inBaseOffset));

		// Accumulate volumes
		outTotalVolume += total_volume;
		outSubmergedVolume += submerged_volume;

		// The center of buoyancy is the weighted average of the center of buoyancy of our child shapes
		outCenterOfBuoyancy += submerged_volume * center_of_buoyancy;
	}

	if (outSubmergedVolume > 0.0f)
		outCenterOfBuoyancy /= outSubmergedVolume;

#ifdef JPH_DEBUG_RENDERER
	// Draw center of buoyancy
	if (sDrawSubmergedVolumes)
		DebugRenderer::sInstance->DrawWireSphere(inBaseOffset + outCenterOfBuoyancy, 0.05f, Color::sRed, 1);
#endif // JPH_DEBUG_RENDERER
}

#ifdef JPH_DEBUG_RENDERER
void CompoundShape::Draw(DebugRenderer *inRenderer, RMat44Arg inCenterOfMassTransform, Vec3Arg inScale, ColorArg inColor, bool inUseMaterialColors, bool inDrawWireframe) const
{
	for (const SubShape &shape : mSubShapes)
	{
		Mat44 transform = shape.GetLocalTransformNoScale(inScale);
		shape.mShape->Draw(inRenderer, inCenterOfMassTransform * transform, shape.TransformScale(inScale), inColor, inUseMaterialColors, inDrawWireframe);
	}
}

void CompoundShape::DrawGetSupportFunction(DebugRenderer *inRenderer, RMat44Arg inCenterOfMassTransform, Vec3Arg inScale, ColorArg inColor, bool inDrawSupportDirection) const
{
	for (const SubShape &shape : mSubShapes)
	{
		Mat44 transform = shape.GetLocalTransformNoScale(inScale);
		shape.mShape->DrawGetSupportFunction(inRenderer, inCenterOfMassTransform * transform, shape.TransformScale(inScale), inColor, inDrawSupportDirection);
	}
}

void CompoundShape::DrawGetSupportingFace(DebugRenderer *inRenderer, RMat44Arg inCenterOfMassTransform, Vec3Arg inScale) const
{
	for (const SubShape &shape : mSubShapes)
	{
		Mat44 transform = shape.GetLocalTransformNoScale(inScale);
		shape.mShape->DrawGetSupportingFace(inRenderer, inCenterOfMassTransform * transform, shape.TransformScale(inScale));
	}
}
#endif // JPH_DEBUG_RENDERER

void CompoundShape::CollideSoftBodyVertices(Mat44Arg inCenterOfMassTransform, Vec3Arg inScale, const CollideSoftBodyVertexIterator &inVertices, uint inNumVertices, int inCollidingShapeIndex) const
{
	for (const SubShape &shape : mSubShapes)
	{
		Mat44 transform = shape.GetLocalTransformNoScale(inScale);
		shape.mShape->CollideSoftBodyVertices(inCenterOfMassTransform * transform, shape.TransformScale(inScale), inVertices, inNumVertices, inCollidingShapeIndex);
	}
}

void CompoundShape::TransformShape(Mat44Arg inCenterOfMassTransform, TransformedShapeCollector &ioCollector) const
{
	for (const SubShape &shape : mSubShapes)
		shape.mShape->TransformShape(inCenterOfMassTransform * Mat44::sRotationTranslation(shape.GetRotation(), shape.GetPositionCOM()), ioCollector);
}

void CompoundShape::sCastCompoundVsShape(const ShapeCast &inShapeCast, const ShapeCastSettings &inShapeCastSettings, const Shape *inShape, Vec3Arg inScale, const ShapeFilter &inShapeFilter, Mat44Arg inCenterOfMassTransform2, const SubShapeIDCreator &inSubShapeIDCreator1, const SubShapeIDCreator &inSubShapeIDCreator2, CastShapeCollector &ioCollector)
{
	JPH_PROFILE_FUNCTION();

	// Fetch compound shape from cast shape
	JPH_ASSERT(inShapeCast.mShape->GetType() == EShapeType::Compound);
	const CompoundShape *compound = static_cast<const CompoundShape *>(inShapeCast.mShape);

	// Number of sub shapes
	int n = (int)compound->mSubShapes.size();

	// Determine amount of bits for sub shape
	uint sub_shape_bits = compound->GetSubShapeIDBits();

	// Recurse to sub shapes
	for (int i = 0; i < n; ++i)
	{
		const SubShape &shape = compound->mSubShapes[i];

		// Create ID for sub shape
		SubShapeIDCreator shape1_sub_shape_id = inSubShapeIDCreator1.PushID(i, sub_shape_bits);

		// Transform the shape cast and update the shape
		Mat44 transform = inShapeCast.mCenterOfMassStart * shape.GetLocalTransformNoScale(inShapeCast.mScale);
		Vec3 scale = shape.TransformScale(inShapeCast.mScale);
		ShapeCast shape_cast(shape.mShape, scale, transform, inShapeCast.mDirection);

		CollisionDispatch::sCastShapeVsShapeLocalSpace(shape_cast, inShapeCastSettings, inShape, inScale, inShapeFilter, inCenterOfMassTransform2, shape1_sub_shape_id, inSubShapeIDCreator2, ioCollector);

		if (ioCollector.ShouldEarlyOut())
			break;
	}
}

void CompoundShape::SaveBinaryState(StreamOut &inStream) const
{
	Shape::SaveBinaryState(inStream);

	inStream.Write(mCenterOfMass);
	inStream.Write(mLocalBounds.mMin);
	inStream.Write(mLocalBounds.mMax);
	inStream.Write(mInnerRadius);

	// Write sub shapes
	inStream.Write(mSubShapes, [](const SubShape &inElement, StreamOut &inS) {
		inS.Write(inElement.mUserData);
		inS.Write(inElement.mPositionCOM);
		inS.Write(inElement.mRotation);
	});
}

void CompoundShape::RestoreBinaryState(StreamIn &inStream)
{
	Shape::RestoreBinaryState(inStream);

	inStream.Read(mCenterOfMass);
	inStream.Read(mLocalBounds.mMin);
	inStream.Read(mLocalBounds.mMax);
	inStream.Read(mInnerRadius);

	// Read sub shapes
	inStream.Read(mSubShapes, [](StreamIn &inS, SubShape &outElement) {
		inS.Read(outElement.mUserData);
		inS.Read(outElement.mPositionCOM);
		inS.Read(outElement.mRotation);
		outElement.mIsRotationIdentity = outElement.mRotation == Float3(0, 0, 0);
	});
}

void CompoundShape::SaveSubShapeState(ShapeList &outSubShapes) const
{
	outSubShapes.clear();
	outSubShapes.reserve(mSubShapes.size());
	for (const SubShape &shape : mSubShapes)
		outSubShapes.push_back(shape.mShape);
}

void CompoundShape::RestoreSubShapeState(const ShapeRefC *inSubShapes, uint inNumShapes)
{
	JPH_ASSERT(mSubShapes.size() == inNumShapes);
	for (uint i = 0; i < inNumShapes; ++i)
		mSubShapes[i].mShape = inSubShapes[i];
}

Shape::Stats CompoundShape::GetStatsRecursive(VisitedShapes &ioVisitedShapes) const
{
	// Get own stats
	Stats stats = Shape::GetStatsRecursive(ioVisitedShapes);

	// Add child stats
	for (const SubShape &shape : mSubShapes)
	{
		Stats child_stats = shape.mShape->GetStatsRecursive(ioVisitedShapes);
		stats.mSizeBytes += child_stats.mSizeBytes;
		stats.mNumTriangles += child_stats.mNumTriangles;
	}

	return stats;
}

float CompoundShape::GetVolume() const
{
	float volume = 0.0f;
	for (const SubShape &shape : mSubShapes)
		volume += shape.mShape->GetVolume();
	return volume;
}

bool CompoundShape::IsValidScale(Vec3Arg inScale) const
{
	if (!Shape::IsValidScale(inScale))
		return false;

	for (const SubShape &shape : mSubShapes)
	{
		// Test if the scale is non-uniform and the shape is rotated
		if (!shape.IsValidScale(inScale))
			return false;

		// Test the child shape
		if (!shape.mShape->IsValidScale(shape.TransformScale(inScale)))
			return false;
	}

	return true;
}

Vec3 CompoundShape::MakeScaleValid(Vec3Arg inScale) const
{
	Vec3 scale = ScaleHelpers::MakeNonZeroScale(inScale);
	if (CompoundShape::IsValidScale(scale))
		return scale;

	Vec3 abs_uniform_scale = ScaleHelpers::MakeUniformScale(scale.Abs());
	Vec3 uniform_scale = scale.GetSign() * abs_uniform_scale;
	if (CompoundShape::IsValidScale(uniform_scale))
		return uniform_scale;

	return Sign(scale.GetX()) * abs_uniform_scale;
}

void CompoundShape::sRegister()
{
	for (EShapeSubType s1 : sCompoundSubShapeTypes)
		for (EShapeSubType s2 : sAllSubShapeTypes)
			CollisionDispatch::sRegisterCastShape(s1, s2, sCastCompoundVsShape);
}

JPH_NAMESPACE_END

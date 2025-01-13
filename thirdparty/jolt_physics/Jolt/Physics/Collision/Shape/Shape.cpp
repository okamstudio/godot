// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#include <Jolt/Jolt.h>

#include <Jolt/Physics/Collision/Shape/Shape.h>
#include <Jolt/Physics/Collision/Shape/ScaledShape.h>
#include <Jolt/Physics/Collision/Shape/StaticCompoundShape.h>
#include <Jolt/Physics/Collision/TransformedShape.h>
#include <Jolt/Physics/Collision/PhysicsMaterial.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/CollidePointResult.h>
#include <Jolt/Core/StreamIn.h>
#include <Jolt/Core/StreamOut.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/ObjectStream/TypeDeclarations.h>

JPH_NAMESPACE_BEGIN

JPH_IMPLEMENT_SERIALIZABLE_ABSTRACT_BASE(ShapeSettings)
{
	JPH_ADD_BASE_CLASS(ShapeSettings, SerializableObject)

	JPH_ADD_ATTRIBUTE(ShapeSettings, mUserData)
}

#ifdef JPH_DEBUG_RENDERER
bool Shape::sDrawSubmergedVolumes = false;
#endif // JPH_DEBUG_RENDERER

ShapeFunctions ShapeFunctions::sRegistry[NumSubShapeTypes];

const Shape *Shape::GetLeafShape([[maybe_unused]] const SubShapeID &inSubShapeID, SubShapeID &outRemainder) const
{
	outRemainder = inSubShapeID;
	return this;
}

TransformedShape Shape::GetSubShapeTransformedShape(const SubShapeID &inSubShapeID, Vec3Arg inPositionCOM, QuatArg inRotation, Vec3Arg inScale, SubShapeID &outRemainder) const
{
	// We have reached the leaf shape so there is no remainder
	outRemainder = SubShapeID();

	// Just return the transformed shape for this shape
	TransformedShape ts(RVec3(inPositionCOM), inRotation, this, BodyID());
	ts.SetShapeScale(inScale);
	return ts;
}

void Shape::CollectTransformedShapes(const AABox &inBox, Vec3Arg inPositionCOM, QuatArg inRotation, Vec3Arg inScale, const SubShapeIDCreator &inSubShapeIDCreator, TransformedShapeCollector &ioCollector, const ShapeFilter &inShapeFilter) const
{
	// Test shape filter
	if (!inShapeFilter.ShouldCollide(this, inSubShapeIDCreator.GetID()))
		return;

	TransformedShape ts(RVec3(inPositionCOM), inRotation, this, TransformedShape::sGetBodyID(ioCollector.GetContext()), inSubShapeIDCreator);
	ts.SetShapeScale(inScale);
	ioCollector.AddHit(ts);
}

void Shape::TransformShape(Mat44Arg inCenterOfMassTransform, TransformedShapeCollector &ioCollector) const
{
	Vec3 scale;
	Mat44 transform = inCenterOfMassTransform.Decompose(scale);
	TransformedShape ts(RVec3(transform.GetTranslation()), transform.GetQuaternion(), this, BodyID(), SubShapeIDCreator());
	ts.SetShapeScale(MakeScaleValid(scale));
	ioCollector.AddHit(ts);
}

void Shape::SaveBinaryState(StreamOut &inStream) const
{
	inStream.Write(mShapeSubType);
	inStream.Write(mUserData);
}

void Shape::RestoreBinaryState(StreamIn &inStream)
{
	// Type hash read by sRestoreFromBinaryState
	inStream.Read(mUserData);
}

Shape::ShapeResult Shape::sRestoreFromBinaryState(StreamIn &inStream)
{
	ShapeResult result;

	// Read the type of the shape
	EShapeSubType shape_sub_type;
	inStream.Read(shape_sub_type);
	if (inStream.IsEOF() || inStream.IsFailed())
	{
		result.SetError("Failed to read type id");
		return result;
	}

	// Construct and read the data of the shape
	Ref<Shape> shape = ShapeFunctions::sGet(shape_sub_type).mConstruct();
	shape->RestoreBinaryState(inStream);
	if (inStream.IsEOF() || inStream.IsFailed())
	{
		result.SetError("Failed to restore shape");
		return result;
	}

	result.Set(shape);
	return result;
}

void Shape::SaveWithChildren(StreamOut &inStream, ShapeToIDMap &ioShapeMap, MaterialToIDMap &ioMaterialMap) const
{
	ShapeToIDMap::const_iterator shape_id_iter = ioShapeMap.find(this);
	if (shape_id_iter == ioShapeMap.end())
	{
		// Write shape ID of this shape
		uint32 shape_id = ioShapeMap.size();
		ioShapeMap[this] = shape_id;
		inStream.Write(shape_id);

		// Write the shape itself
		SaveBinaryState(inStream);

		// Write the ID's of all sub shapes
		ShapeList sub_shapes;
		SaveSubShapeState(sub_shapes);
		inStream.Write(uint32(sub_shapes.size()));
		for (const Shape *shape : sub_shapes)
		{
			if (shape == nullptr)
				inStream.Write(~uint32(0));
			else
				shape->SaveWithChildren(inStream, ioShapeMap, ioMaterialMap);
		}

		// Write the materials
		PhysicsMaterialList materials;
		SaveMaterialState(materials);
		StreamUtils::SaveObjectArray(inStream, materials, &ioMaterialMap);
	}
	else
	{
		// Known shape, just write the ID
		inStream.Write(shape_id_iter->second);
	}
}

Shape::ShapeResult Shape::sRestoreWithChildren(StreamIn &inStream, IDToShapeMap &ioShapeMap, IDToMaterialMap &ioMaterialMap)
{
	ShapeResult result;

	// Read ID of this shape
	uint32 shape_id;
	inStream.Read(shape_id);
	if (inStream.IsEOF() || inStream.IsFailed())
	{
		result.SetError("Failed to read shape id");
		return result;
	}

	// Check nullptr shape
	if (shape_id == ~uint32(0))
	{
		result.Set(nullptr);
		return result;
	}

	// Check if we already read this shape
	if (shape_id < ioShapeMap.size())
	{
		result.Set(ioShapeMap[shape_id]);
		return result;
	}

	// Read the shape
	result = sRestoreFromBinaryState(inStream);
	if (result.HasError())
		return result;
	JPH_ASSERT(ioShapeMap.size() == shape_id); // Assert that this is the next ID in the map
	ioShapeMap.push_back(result.Get());

	// Read the sub shapes
	uint32 len;
	inStream.Read(len);
	if (inStream.IsEOF() || inStream.IsFailed())
	{
		result.SetError("Failed to read stream");
		return result;
	}
	ShapeList sub_shapes;
	sub_shapes.reserve(len);
	for (size_t i = 0; i < len; ++i)
	{
		ShapeResult sub_shape_result = sRestoreWithChildren(inStream, ioShapeMap, ioMaterialMap);
		if (sub_shape_result.HasError())
			return sub_shape_result;
		sub_shapes.push_back(sub_shape_result.Get());
	}
	result.Get()->RestoreSubShapeState(sub_shapes.data(), (uint)sub_shapes.size());

	// Read the materials
	Result mlresult = StreamUtils::RestoreObjectArray<PhysicsMaterialList>(inStream, ioMaterialMap);
	if (mlresult.HasError())
	{
		result.SetError(mlresult.GetError());
		return result;
	}
	const PhysicsMaterialList &materials = mlresult.Get();
	result.Get()->RestoreMaterialState(materials.data(), (uint)materials.size());

	return result;
}

Shape::Stats Shape::GetStatsRecursive(VisitedShapes &ioVisitedShapes) const
{
	Stats stats = GetStats();

	// If shape is already visited, don't count its size again
	if (!ioVisitedShapes.insert(this).second)
		stats.mSizeBytes = 0;

	return stats;
}

bool Shape::IsValidScale(Vec3Arg inScale) const
{
	return !ScaleHelpers::IsZeroScale(inScale);
}

Vec3 Shape::MakeScaleValid(Vec3Arg inScale) const
{
	return ScaleHelpers::MakeNonZeroScale(inScale);
}

Shape::ShapeResult Shape::ScaleShape(Vec3Arg inScale) const
{
	const Vec3 unit_scale = Vec3::sReplicate(1.0f);

	if (inScale.IsNearZero())
	{
		ShapeResult result;
		result.SetError("Can't use zero scale!");
		return result;
	}

	// First test if we can just wrap this shape in a scaled shape
	if (IsValidScale(inScale))
	{
		// Test if the scale is near unit
		ShapeResult result;
		if (inScale.IsClose(unit_scale))
			result.Set(const_cast<Shape *>(this));
		else
			result.Set(new ScaledShape(this, inScale));
		return result;
	}

	// Collect the leaf shapes and their transforms
	struct Collector : TransformedShapeCollector
	{
		virtual void				AddHit(const ResultType &inResult) override
		{
			mShapes.push_back(inResult);
		}

		Array<TransformedShape>		mShapes;
	};
	Collector collector;
	TransformShape(Mat44::sScale(inScale) * Mat44::sTranslation(GetCenterOfMass()), collector);

	// Construct a compound shape
	StaticCompoundShapeSettings compound;
	compound.mSubShapes.reserve(collector.mShapes.size());
	for (const TransformedShape &ts : collector.mShapes)
	{
		const Shape *shape = ts.mShape;

		// Construct a scaled shape if scale is not unit
		Vec3 scale = ts.GetShapeScale();
		if (!scale.IsClose(unit_scale))
			shape = new ScaledShape(shape, scale);

		// Add the shape
		compound.AddShape(Vec3(ts.mShapePositionCOM) - ts.mShapeRotation * shape->GetCenterOfMass(), ts.mShapeRotation, shape);
	}

	return compound.Create();
}

void Shape::sCollidePointUsingRayCast(const Shape &inShape, Vec3Arg inPoint, const SubShapeIDCreator &inSubShapeIDCreator, CollidePointCollector &ioCollector, const ShapeFilter &inShapeFilter)
{
	// First test if we're inside our bounding box
	AABox bounds = inShape.GetLocalBounds();
	if (bounds.Contains(inPoint))
	{
		// A collector that just counts the number of hits
		class HitCountCollector : public CastRayCollector
		{
		public:
			virtual void	AddHit(const RayCastResult &inResult) override
			{
				// Store the last sub shape ID so that we can provide something to our outer hit collector
				mSubShapeID = inResult.mSubShapeID2;

				++mHitCount;
			}

			int				mHitCount = 0;
			SubShapeID		mSubShapeID;
		};
		HitCountCollector collector;

		// Configure the raycast
		RayCastSettings settings;
		settings.SetBackFaceMode(EBackFaceMode::CollideWithBackFaces);

		// Cast a ray that's 10% longer than the height of our bounding box
		inShape.CastRay(RayCast { inPoint, 1.1f * bounds.GetSize().GetY() * Vec3::sAxisY() }, settings, inSubShapeIDCreator, collector, inShapeFilter);

		// Odd amount of hits means inside
		if ((collector.mHitCount & 1) == 1)
			ioCollector.AddHit({ TransformedShape::sGetBodyID(ioCollector.GetContext()), collector.mSubShapeID });
	}
}

JPH_NAMESPACE_END

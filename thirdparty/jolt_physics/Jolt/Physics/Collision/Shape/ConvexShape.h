// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Jolt/Core/StaticArray.h>
#include <Jolt/Physics/Collision/Shape/Shape.h>
#include <Jolt/Physics/Collision/Shape/SubShapeID.h>
#include <Jolt/Physics/Collision/PhysicsMaterial.h>

JPH_NAMESPACE_BEGIN

class CollideShapeSettings;

/// Class that constructs a ConvexShape (abstract)
class JPH_EXPORT ConvexShapeSettings : public ShapeSettings
{
	JPH_DECLARE_SERIALIZABLE_ABSTRACT(JPH_EXPORT, ConvexShapeSettings)

public:
	/// Constructor
									ConvexShapeSettings() = default;
	explicit						ConvexShapeSettings(const PhysicsMaterial *inMaterial)		: mMaterial(inMaterial) { }

	/// Set the density of the object in kg / m^3
	void							SetDensity(float inDensity)									{ mDensity = inDensity; }

	// Properties
	RefConst<PhysicsMaterial>		mMaterial;													///< Material assigned to this shape
	float							mDensity = 1000.0f;											///< Uniform density of the interior of the convex object (kg / m^3)
};

/// Base class for all convex shapes. Defines a virtual interface.
class JPH_EXPORT ConvexShape : public Shape
{
public:
	JPH_OVERRIDE_NEW_DELETE

	/// Constructor
	explicit						ConvexShape(EShapeSubType inSubType) : Shape(EShapeType::Convex, inSubType) { }
									ConvexShape(EShapeSubType inSubType, const ConvexShapeSettings &inSettings, ShapeResult &outResult) : Shape(EShapeType::Convex, inSubType, inSettings, outResult), mMaterial(inSettings.mMaterial), mDensity(inSettings.mDensity) { }
									ConvexShape(EShapeSubType inSubType, const PhysicsMaterial *inMaterial) : Shape(EShapeType::Convex, inSubType), mMaterial(inMaterial) { }

	// See Shape::GetSubShapeIDBitsRecursive
	virtual uint					GetSubShapeIDBitsRecursive() const override					{ return 0; } // Convex shapes don't have sub shapes

	// See Shape::GetMaterial
	virtual const PhysicsMaterial *	GetMaterial([[maybe_unused]] const SubShapeID &inSubShapeID) const override	{ JPH_ASSERT(inSubShapeID.IsEmpty(), "Invalid subshape ID"); return GetMaterial(); }

	// See Shape::CastRay
	virtual bool					CastRay(const RayCast &inRay, const SubShapeIDCreator &inSubShapeIDCreator, RayCastResult &ioHit) const override;
	virtual void					CastRay(const RayCast &inRay, const RayCastSettings &inRayCastSettings, const SubShapeIDCreator &inSubShapeIDCreator, CastRayCollector &ioCollector, const ShapeFilter &inShapeFilter = { }) const override;

	// See: Shape::CollidePoint
	virtual void					CollidePoint(Vec3Arg inPoint, const SubShapeIDCreator &inSubShapeIDCreator, CollidePointCollector &ioCollector, const ShapeFilter &inShapeFilter = { }) const override;

	// See Shape::GetTrianglesStart
	virtual void					GetTrianglesStart(GetTrianglesContext &ioContext, const AABox &inBox, Vec3Arg inPositionCOM, QuatArg inRotation, Vec3Arg inScale) const override;

	// See Shape::GetTrianglesNext
	virtual int						GetTrianglesNext(GetTrianglesContext &ioContext, int inMaxTrianglesRequested, Float3 *outTriangleVertices, const PhysicsMaterial **outMaterials = nullptr) const override;

	// See Shape::GetSubmergedVolume
	virtual void					GetSubmergedVolume(Mat44Arg inCenterOfMassTransform, Vec3Arg inScale, const Plane &inSurface, float &outTotalVolume, float &outSubmergedVolume, Vec3 &outCenterOfBuoyancy JPH_IF_DEBUG_RENDERER(, RVec3Arg inBaseOffset)) const override;

	/// Function that provides an interface for GJK
	class Support
	{
	public:
		/// Warning: Virtual destructor will not be called on this object!
		virtual						~Support() = default;

		/// Calculate the support vector for this convex shape (includes / excludes the convex radius depending on how this was obtained).
		/// Support vector is relative to the center of mass of the shape.
		virtual Vec3				GetSupport(Vec3Arg inDirection) const = 0;

		/// Convex radius of shape. Collision detection on penetrating shapes is much more expensive,
		/// so you can add a radius around objects to increase the shape. This makes it far less likely that they will actually penetrate.
		virtual float				GetConvexRadius() const = 0;
	};

	/// Buffer to hold a Support object, used to avoid dynamic memory allocations
	class alignas(16) SupportBuffer
	{
	public:
		uint8						mData[4160];
	};

	/// How the GetSupport function should behave
	enum class ESupportMode
	{
		ExcludeConvexRadius,		///< Return the shape excluding the convex radius, Support::GetConvexRadius will return the convex radius if there is one, but adding this radius may not result in the most accurate/efficient representation of shapes with sharp edges
		IncludeConvexRadius,		///< Return the shape including the convex radius, Support::GetSupport includes the convex radius if there is one, Support::GetConvexRadius will return 0
		Default,					///< Use both Support::GetSupport add Support::GetConvexRadius to get a support point that matches the original shape as accurately/efficiently as possible
	};

	/// Returns an object that provides the GetSupport function for this shape.
	/// inMode determines if this support function includes or excludes the convex radius.
	/// of the values returned by the GetSupport function. This improves numerical accuracy of the results.
	/// inScale scales this shape in local space.
	virtual const Support *			GetSupportFunction(ESupportMode inMode, SupportBuffer &inBuffer, Vec3Arg inScale) const = 0;

	/// Material of the shape
	void							SetMaterial(const PhysicsMaterial *inMaterial)				{ mMaterial = inMaterial; }
	const PhysicsMaterial *			GetMaterial() const											{ return mMaterial != nullptr? mMaterial : PhysicsMaterial::sDefault; }

	/// Set density of the shape (kg / m^3)
	void							SetDensity(float inDensity)									{ mDensity = inDensity; }

	/// Get density of the shape (kg / m^3)
	float							GetDensity() const											{ return mDensity; }

#ifdef JPH_DEBUG_RENDERER
	// See Shape::DrawGetSupportFunction
	virtual void					DrawGetSupportFunction(DebugRenderer *inRenderer, RMat44Arg inCenterOfMassTransform, Vec3Arg inScale, ColorArg inColor, bool inDrawSupportDirection) const override;

	// See Shape::DrawGetSupportingFace
	virtual void					DrawGetSupportingFace(DebugRenderer *inRenderer, RMat44Arg inCenterOfMassTransform, Vec3Arg inScale) const override;
#endif // JPH_DEBUG_RENDERER

	// See Shape
	virtual void					SaveBinaryState(StreamOut &inStream) const override;
	virtual void					SaveMaterialState(PhysicsMaterialList &outMaterials) const override;
	virtual void					RestoreMaterialState(const PhysicsMaterialRefC *inMaterials, uint inNumMaterials) override;

	// Register shape functions with the registry
	static void						sRegister();

protected:
	// See: Shape::RestoreBinaryState
	virtual void					RestoreBinaryState(StreamIn &inStream) override;

	/// Vertex list that forms a unit sphere
	static const StaticArray<Vec3, 384> sUnitSphereTriangles;

private:
	// Class for GetTrianglesStart/Next
	class							CSGetTrianglesContext;

	// Helper functions called by CollisionDispatch
	static void						sCollideConvexVsConvex(const Shape *inShape1, const Shape *inShape2, Vec3Arg inScale1, Vec3Arg inScale2, Mat44Arg inCenterOfMassTransform1, Mat44Arg inCenterOfMassTransform2, const SubShapeIDCreator &inSubShapeIDCreator1, const SubShapeIDCreator &inSubShapeIDCreator2, const CollideShapeSettings &inCollideShapeSettings, CollideShapeCollector &ioCollector, const ShapeFilter &inShapeFilter);
	static void						sCastConvexVsConvex(const ShapeCast &inShapeCast, const ShapeCastSettings &inShapeCastSettings, const Shape *inShape, Vec3Arg inScale, const ShapeFilter &inShapeFilter, Mat44Arg inCenterOfMassTransform2, const SubShapeIDCreator &inSubShapeIDCreator1, const SubShapeIDCreator &inSubShapeIDCreator2, CastShapeCollector &ioCollector);

	// Properties
	RefConst<PhysicsMaterial>		mMaterial;													///< Material assigned to this shape
	float							mDensity = 1000.0f;											///< Uniform density of the interior of the convex object (kg / m^3)
};

JPH_NAMESPACE_END

// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Jolt/Physics/Body/MassProperties.h>
#include <Jolt/Physics/Collision/BackFaceMode.h>
#include <Jolt/Physics/Collision/CollisionCollector.h>
#include <Jolt/Physics/Collision/ShapeFilter.h>
#include <Jolt/Geometry/AABox.h>
#include <Jolt/Core/Reference.h>
#include <Jolt/Core/Color.h>
#include <Jolt/Core/Result.h>
#include <Jolt/Core/NonCopyable.h>
#include <Jolt/Core/UnorderedMap.h>
#include <Jolt/Core/UnorderedSet.h>
#include <Jolt/Core/StreamUtils.h>
#include <Jolt/ObjectStream/SerializableObject.h>

JPH_NAMESPACE_BEGIN

struct RayCast;
class RayCastSettings;
struct ShapeCast;
class ShapeCastSettings;
class RayCastResult;
class ShapeCastResult;
class CollidePointResult;
class CollideShapeResult;
class SubShapeIDCreator;
class SubShapeID;
class PhysicsMaterial;
class TransformedShape;
class Plane;
class CollideSoftBodyVertexIterator;
class Shape;
class StreamOut;
class StreamIn;
#ifdef JPH_DEBUG_RENDERER
class DebugRenderer;
#endif // JPH_DEBUG_RENDERER

using CastRayCollector = CollisionCollector<RayCastResult, CollisionCollectorTraitsCastRay>;
using CastShapeCollector = CollisionCollector<ShapeCastResult, CollisionCollectorTraitsCastShape>;
using CollidePointCollector = CollisionCollector<CollidePointResult, CollisionCollectorTraitsCollidePoint>;
using CollideShapeCollector = CollisionCollector<CollideShapeResult, CollisionCollectorTraitsCollideShape>;
using TransformedShapeCollector = CollisionCollector<TransformedShape, CollisionCollectorTraitsCollideShape>;

using ShapeRefC = RefConst<Shape>;
using ShapeList = Array<ShapeRefC>;
using PhysicsMaterialRefC = RefConst<PhysicsMaterial>;
using PhysicsMaterialList = Array<PhysicsMaterialRefC>;

/// Shapes are categorized in groups, each shape can return which group it belongs to through its Shape::GetType function.
enum class EShapeType : uint8
{
	Convex,							///< Used by ConvexShape, all shapes that use the generic convex vs convex collision detection system (box, sphere, capsule, tapered capsule, cylinder, triangle)
	Compound,						///< Used by CompoundShape
	Decorated,						///< Used by DecoratedShape
	Mesh,							///< Used by MeshShape
	HeightField,					///< Used by HeightFieldShape
	SoftBody,						///< Used by SoftBodyShape

	// User defined shapes
	User1,
	User2,
	User3,
	User4,

	Plane,							///< Used by PlaneShape
	Empty,							///< Used by EmptyShape
};

/// This enumerates all shape types, each shape can return its type through Shape::GetSubType
enum class EShapeSubType : uint8
{
	// Convex shapes
	Sphere,
	Box,
	Triangle,
	Capsule,
	TaperedCapsule,
	Cylinder,
	ConvexHull,

	// Compound shapes
	StaticCompound,
	MutableCompound,

	// Decorated shapes
	RotatedTranslated,
	Scaled,
	OffsetCenterOfMass,

	// Other shapes
	Mesh,
	HeightField,
	SoftBody,

	// User defined shapes
	User1,
	User2,
	User3,
	User4,
	User5,
	User6,
	User7,
	User8,

	// User defined convex shapes
	UserConvex1,
	UserConvex2,
	UserConvex3,
	UserConvex4,
	UserConvex5,
	UserConvex6,
	UserConvex7,
	UserConvex8,

	// Other shapes
	Plane,
	TaperedCylinder,
	Empty,
};

// Sets of shape sub types
static constexpr EShapeSubType sAllSubShapeTypes[] = { EShapeSubType::Sphere, EShapeSubType::Box, EShapeSubType::Triangle, EShapeSubType::Capsule, EShapeSubType::TaperedCapsule, EShapeSubType::Cylinder, EShapeSubType::ConvexHull, EShapeSubType::StaticCompound, EShapeSubType::MutableCompound, EShapeSubType::RotatedTranslated, EShapeSubType::Scaled, EShapeSubType::OffsetCenterOfMass, EShapeSubType::Mesh, EShapeSubType::HeightField, EShapeSubType::SoftBody, EShapeSubType::User1, EShapeSubType::User2, EShapeSubType::User3, EShapeSubType::User4, EShapeSubType::User5, EShapeSubType::User6, EShapeSubType::User7, EShapeSubType::User8, EShapeSubType::UserConvex1, EShapeSubType::UserConvex2, EShapeSubType::UserConvex3, EShapeSubType::UserConvex4, EShapeSubType::UserConvex5, EShapeSubType::UserConvex6, EShapeSubType::UserConvex7, EShapeSubType::UserConvex8, EShapeSubType::Plane, EShapeSubType::TaperedCylinder, EShapeSubType::Empty };
static constexpr EShapeSubType sConvexSubShapeTypes[] = { EShapeSubType::Sphere, EShapeSubType::Box, EShapeSubType::Triangle, EShapeSubType::Capsule, EShapeSubType::TaperedCapsule, EShapeSubType::Cylinder, EShapeSubType::ConvexHull, EShapeSubType::TaperedCylinder, EShapeSubType::UserConvex1, EShapeSubType::UserConvex2, EShapeSubType::UserConvex3, EShapeSubType::UserConvex4, EShapeSubType::UserConvex5, EShapeSubType::UserConvex6, EShapeSubType::UserConvex7, EShapeSubType::UserConvex8 };
static constexpr EShapeSubType sCompoundSubShapeTypes[] = { EShapeSubType::StaticCompound, EShapeSubType::MutableCompound };
static constexpr EShapeSubType sDecoratorSubShapeTypes[] = { EShapeSubType::RotatedTranslated, EShapeSubType::Scaled, EShapeSubType::OffsetCenterOfMass };

/// How many shape types we support
static constexpr uint NumSubShapeTypes = uint(std::size(sAllSubShapeTypes));

/// Names of sub shape types
static constexpr const char *sSubShapeTypeNames[] = { "Sphere", "Box", "Triangle", "Capsule", "TaperedCapsule", "Cylinder", "ConvexHull", "StaticCompound", "MutableCompound", "RotatedTranslated", "Scaled", "OffsetCenterOfMass", "Mesh", "HeightField", "SoftBody", "User1", "User2", "User3", "User4", "User5", "User6", "User7", "User8", "UserConvex1", "UserConvex2", "UserConvex3", "UserConvex4", "UserConvex5", "UserConvex6", "UserConvex7", "UserConvex8", "Plane", "TaperedCylinder", "Empty" };
static_assert(std::size(sSubShapeTypeNames) == NumSubShapeTypes);

/// Class that can construct shapes and that is serializable using the ObjectStream system.
/// Can be used to store shape data in 'uncooked' form (i.e. in a form that is still human readable and authorable).
/// Once the shape has been created using the Create() function, the data will be moved into the Shape class
/// in a form that is optimized for collision detection. After this, the ShapeSettings object is no longer needed
/// and can be destroyed. Each shape class has a derived class of the ShapeSettings object to store shape specific
/// data.
class JPH_EXPORT ShapeSettings : public SerializableObject, public RefTarget<ShapeSettings>
{
	JPH_DECLARE_SERIALIZABLE_ABSTRACT(JPH_EXPORT, ShapeSettings)

public:
	using ShapeResult = Result<Ref<Shape>>;

	/// Create a shape according to the settings specified by this object.
	virtual ShapeResult				Create() const = 0;

	/// When creating a shape, the result is cached so that calling Create() again will return the same shape.
	/// If you make changes to the ShapeSettings you need to call this function to clear the cached result to allow Create() to build a new shape.
	void							ClearCachedResult()													{ mCachedResult.Clear(); }

	/// User data (to be used freely by the application)
	uint64							mUserData = 0;

protected:
	mutable ShapeResult				mCachedResult;
};

/// Function table for functions on shapes
class JPH_EXPORT ShapeFunctions
{
public:
	/// Construct a shape
	Shape *							(*mConstruct)() = nullptr;

	/// Color of the shape when drawing
	Color							mColor = Color::sBlack;

	/// Get an entry in the registry for a particular sub type
	static inline ShapeFunctions &	sGet(EShapeSubType inSubType)										{ return sRegistry[int(inSubType)]; }

private:
	static ShapeFunctions			sRegistry[NumSubShapeTypes];
};

/// Base class for all shapes (collision volume of a body). Defines a virtual interface for collision detection.
class JPH_EXPORT Shape : public RefTarget<Shape>, public NonCopyable
{
public:
	JPH_OVERRIDE_NEW_DELETE

	using ShapeResult = ShapeSettings::ShapeResult;

	/// Constructor
									Shape(EShapeType inType, EShapeSubType inSubType) : mShapeType(inType), mShapeSubType(inSubType) { }
									Shape(EShapeType inType, EShapeSubType inSubType, const ShapeSettings &inSettings, [[maybe_unused]] ShapeResult &outResult) : mUserData(inSettings.mUserData), mShapeType(inType), mShapeSubType(inSubType) { }

	/// Destructor
	virtual							~Shape() = default;

	/// Get type
	inline EShapeType				GetType() const														{ return mShapeType; }
	inline EShapeSubType			GetSubType() const													{ return mShapeSubType; }

	/// User data (to be used freely by the application)
	uint64							GetUserData() const													{ return mUserData; }
	void							SetUserData(uint64 inUserData)										{ mUserData = inUserData; }

	/// Check if this shape can only be used to create a static body or if it can also be dynamic/kinematic
	virtual bool					MustBeStatic() const												{ return false; }

	/// All shapes are centered around their center of mass. This function returns the center of mass position that needs to be applied to transform the shape to where it was created.
	virtual Vec3					GetCenterOfMass() const												{ return Vec3::sZero(); }

	/// Get local bounding box including convex radius, this box is centered around the center of mass rather than the world transform
	virtual AABox					GetLocalBounds() const = 0;

	/// Get the max number of sub shape ID bits that are needed to be able to address any leaf shape in this shape. Used mainly for checking that it is smaller or equal than SubShapeID::MaxBits.
	virtual uint					GetSubShapeIDBitsRecursive() const = 0;

	/// Get world space bounds including convex radius.
	/// This shape is scaled by inScale in local space first.
	/// This function can be overridden to return a closer fitting world space bounding box, by default it will just transform what GetLocalBounds() returns.
	virtual AABox					GetWorldSpaceBounds(Mat44Arg inCenterOfMassTransform, Vec3Arg inScale) const { return GetLocalBounds().Scaled(inScale).Transformed(inCenterOfMassTransform); }

	/// Get world space bounds including convex radius.
	AABox							GetWorldSpaceBounds(DMat44Arg inCenterOfMassTransform, Vec3Arg inScale) const
	{
		// Use single precision version using the rotation only
		AABox bounds = GetWorldSpaceBounds(inCenterOfMassTransform.GetRotation(), inScale);

		// Apply translation
		bounds.Translate(inCenterOfMassTransform.GetTranslation());

		return bounds;
	}

	/// Returns the radius of the biggest sphere that fits entirely in the shape. In case this shape consists of multiple sub shapes, it returns the smallest sphere of the parts.
	/// This can be used as a measure of how far the shape can be moved without risking going through geometry.
	virtual float					GetInnerRadius() const = 0;

	/// Calculate the mass and inertia of this shape
	virtual MassProperties			GetMassProperties() const = 0;

	/// Get the leaf shape for a particular sub shape ID.
	/// @param inSubShapeID The full sub shape ID that indicates the path to the leaf shape
	/// @param outRemainder What remains of the sub shape ID after removing the path to the leaf shape (could e.g. refer to a triangle within a MeshShape)
	/// @return The shape or null if the sub shape ID is invalid
	virtual const Shape *			GetLeafShape([[maybe_unused]] const SubShapeID &inSubShapeID, SubShapeID &outRemainder) const;

	/// Get the material assigned to a particular sub shape ID
	virtual const PhysicsMaterial *	GetMaterial(const SubShapeID &inSubShapeID) const = 0;

	/// Get the surface normal of a particular sub shape ID and point on surface (all vectors are relative to center of mass for this shape).
	/// Note: When you have a CollideShapeResult or ShapeCastResult you should use -mPenetrationAxis.Normalized() as contact normal as GetSurfaceNormal will only return face normals (and not vertex or edge normals).
	virtual Vec3					GetSurfaceNormal(const SubShapeID &inSubShapeID, Vec3Arg inLocalSurfacePosition) const = 0;

	/// Type definition for a supporting face
	using SupportingFace = StaticArray<Vec3, 32>;

	/// Get the vertices of the face that faces inDirection the most (includes any convex radius). Note that this function can only return faces of
	/// convex shapes or triangles, which is why a sub shape ID to get to that leaf must be provided.
	/// @param inSubShapeID Sub shape ID of target shape
	/// @param inDirection Direction that the face should be facing (in local space to this shape)
	/// @param inCenterOfMassTransform Transform to transform outVertices with
	/// @param inScale Scale in local space of the shape (scales relative to its center of mass)
	/// @param outVertices Resulting face. The returned face can be empty if the shape doesn't have polygons to return (e.g. because it's a sphere). The face will be returned in world space.
	virtual void					GetSupportingFace([[maybe_unused]] const SubShapeID &inSubShapeID, [[maybe_unused]] Vec3Arg inDirection, [[maybe_unused]] Vec3Arg inScale, [[maybe_unused]] Mat44Arg inCenterOfMassTransform, [[maybe_unused]] SupportingFace &outVertices) const { /* Nothing */ }

	/// Get the user data of a particular sub shape ID. Corresponds with the value stored in Shape::GetUserData of the leaf shape pointed to by inSubShapeID.
	virtual uint64					GetSubShapeUserData([[maybe_unused]] const SubShapeID &inSubShapeID) const			{ return mUserData; }

	/// Get the direct child sub shape and its transform for a sub shape ID.
	/// @param inSubShapeID Sub shape ID that indicates the path to the leaf shape
	/// @param inPositionCOM The position of the center of mass of this shape
	/// @param inRotation The orientation of this shape
	/// @param inScale Scale in local space of the shape (scales relative to its center of mass)
	/// @param outRemainder The remainder of the sub shape ID after removing the sub shape
	/// @return Direct child sub shape and its transform, note that the body ID and sub shape ID will be invalid
	virtual TransformedShape		GetSubShapeTransformedShape(const SubShapeID &inSubShapeID, Vec3Arg inPositionCOM, QuatArg inRotation, Vec3Arg inScale, SubShapeID &outRemainder) const;

	/// Gets the properties needed to do buoyancy calculations for a body using this shape
	/// @param inCenterOfMassTransform Transform that takes this shape (centered around center of mass) to world space (or a desired other space)
	/// @param inScale Scale in local space of the shape (scales relative to its center of mass)
	/// @param inSurface The surface plane of the liquid relative to inCenterOfMassTransform
	/// @param outTotalVolume On return this contains the total volume of the shape
	/// @param outSubmergedVolume On return this contains the submerged volume of the shape
	/// @param outCenterOfBuoyancy On return this contains the world space center of mass of the submerged volume
#ifdef JPH_DEBUG_RENDERER
	/// @param inBaseOffset The offset to transform inCenterOfMassTransform to world space (in double precision mode this can be used to shift the whole operation closer to the origin). Only used for debug drawing.
#endif
	virtual void					GetSubmergedVolume(Mat44Arg inCenterOfMassTransform, Vec3Arg inScale, const Plane &inSurface, float &outTotalVolume, float &outSubmergedVolume, Vec3 &outCenterOfBuoyancy
#ifdef JPH_DEBUG_RENDERER // Not using JPH_IF_DEBUG_RENDERER for Doxygen
		, RVec3Arg inBaseOffset
#endif
		) const = 0;

#ifdef JPH_DEBUG_RENDERER
	/// Draw the shape at a particular location with a particular color (debugging purposes)
	virtual void					Draw(DebugRenderer *inRenderer, RMat44Arg inCenterOfMassTransform, Vec3Arg inScale, ColorArg inColor, bool inUseMaterialColors, bool inDrawWireframe) const = 0;

	/// Draw the results of the GetSupportFunction with the convex radius added back on to show any errors introduced by this process (only relevant for convex shapes)
	virtual void					DrawGetSupportFunction([[maybe_unused]] DebugRenderer *inRenderer, [[maybe_unused]] RMat44Arg inCenterOfMassTransform, [[maybe_unused]] Vec3Arg inScale, [[maybe_unused]] ColorArg inColor, [[maybe_unused]] bool inDrawSupportDirection) const { /* Only implemented for convex shapes */ }

	/// Draw the results of the GetSupportingFace function to show any errors introduced by this process (only relevant for convex shapes)
	virtual void					DrawGetSupportingFace([[maybe_unused]] DebugRenderer *inRenderer, [[maybe_unused]] RMat44Arg inCenterOfMassTransform, [[maybe_unused]] Vec3Arg inScale) const { /* Only implemented for convex shapes */ }
#endif // JPH_DEBUG_RENDERER

	/// Cast a ray against this shape, returns true if it finds a hit closer than ioHit.mFraction and updates that fraction. Otherwise ioHit is left untouched and the function returns false.
	/// Note that the ray should be relative to the center of mass of this shape (i.e. subtract Shape::GetCenterOfMass() from RayCast::mOrigin if you want to cast against the shape in the space it was created).
	/// Convex objects will be treated as solid (meaning if the ray starts inside, you'll get a hit fraction of 0) and back face hits against triangles are returned.
	/// If you want the surface normal of the hit use GetSurfaceNormal(ioHit.mSubShapeID2, inRay.GetPointOnRay(ioHit.mFraction)).
	virtual bool					CastRay(const RayCast &inRay, const SubShapeIDCreator &inSubShapeIDCreator, RayCastResult &ioHit) const = 0;

	/// Cast a ray against this shape. Allows returning multiple hits through ioCollector. Note that this version is more flexible but also slightly slower than the CastRay function that returns only a single hit.
	/// If you want the surface normal of the hit use GetSurfaceNormal(collected sub shape ID, inRay.GetPointOnRay(collected faction)).
	virtual void					CastRay(const RayCast &inRay, const RayCastSettings &inRayCastSettings, const SubShapeIDCreator &inSubShapeIDCreator, CastRayCollector &ioCollector, const ShapeFilter &inShapeFilter = { }) const = 0;

	/// Check if inPoint is inside this shape. For this tests all shapes are treated as if they were solid.
	/// Note that inPoint should be relative to the center of mass of this shape (i.e. subtract Shape::GetCenterOfMass() from inPoint if you want to test against the shape in the space it was created).
	/// For a mesh shape, this test will only provide sensible information if the mesh is a closed manifold.
	/// For each shape that collides, ioCollector will receive a hit.
	virtual void					CollidePoint(Vec3Arg inPoint, const SubShapeIDCreator &inSubShapeIDCreator, CollidePointCollector &ioCollector, const ShapeFilter &inShapeFilter = { }) const = 0;

	/// Collides all vertices of a soft body with this shape and updates SoftBodyVertex::mCollisionPlane, SoftBodyVertex::mCollidingShapeIndex and SoftBodyVertex::mLargestPenetration if a collision with more penetration was found.
	/// @param inCenterOfMassTransform Center of mass transform for this shape relative to the vertices.
	/// @param inScale Scale in local space of the shape (scales relative to its center of mass)
	/// @param inVertices The vertices of the soft body
	/// @param inNumVertices The number of vertices in inVertices
	/// @param inCollidingShapeIndex Value to store in CollideSoftBodyVertexIterator::mCollidingShapeIndex when a collision was found
	virtual void					CollideSoftBodyVertices(Mat44Arg inCenterOfMassTransform, Vec3Arg inScale, const CollideSoftBodyVertexIterator &inVertices, uint inNumVertices, int inCollidingShapeIndex) const = 0;

	/// Collect the leaf transformed shapes of all leaf shapes of this shape.
	/// inBox is the world space axis aligned box which leaf shapes should collide with.
	/// inPositionCOM/inRotation/inScale describes the transform of this shape.
	/// inSubShapeIDCreator represents the current sub shape ID of this shape.
	virtual void					CollectTransformedShapes(const AABox &inBox, Vec3Arg inPositionCOM, QuatArg inRotation, Vec3Arg inScale, const SubShapeIDCreator &inSubShapeIDCreator, TransformedShapeCollector &ioCollector, const ShapeFilter &inShapeFilter) const;

	/// Transforms this shape and all of its children with inTransform, resulting shape(s) are passed to ioCollector.
	/// Note that not all shapes support all transforms (especially true for scaling), the resulting shape will try to match the transform as accurately as possible.
	/// @param inCenterOfMassTransform The transform (rotation, translation, scale) that the center of mass of the shape should get
	/// @param ioCollector The transformed shapes will be passed to this collector
	virtual void					TransformShape(Mat44Arg inCenterOfMassTransform, TransformedShapeCollector &ioCollector) const;

	/// Scale this shape. Note that not all shapes support all scales, this will return a shape that matches the scale as accurately as possible. See Shape::IsValidScale for more information.
	/// @param inScale The scale to use for this shape (note: this scale is applied to the entire shape in the space it was created, most other functions apply the scale in the space of the leaf shapes and from the center of mass!)
	ShapeResult						ScaleShape(Vec3Arg inScale) const;

	/// An opaque buffer that holds shape specific information during GetTrianglesStart/Next.
	struct alignas(16)				GetTrianglesContext { uint8 mData[4288]; };

	/// This is the minimum amount of triangles that should be requested through GetTrianglesNext.
	static constexpr int			cGetTrianglesMinTrianglesRequested = 32;

	/// To start iterating over triangles, call this function first.
	/// ioContext is a temporary buffer and should remain untouched until the last call to GetTrianglesNext.
	/// inBox is the world space bounding in which you want to get the triangles.
	/// inPositionCOM/inRotation/inScale describes the transform of this shape.
	/// To get the actual triangles call GetTrianglesNext.
	virtual void					GetTrianglesStart(GetTrianglesContext &ioContext, const AABox &inBox, Vec3Arg inPositionCOM, QuatArg inRotation, Vec3Arg inScale) const = 0;

	/// Call this repeatedly to get all triangles in the box.
	/// outTriangleVertices should be large enough to hold 3 * inMaxTriangleRequested entries.
	/// outMaterials (if it is not null) should contain inMaxTrianglesRequested entries.
	/// The function returns the amount of triangles that it found (which will be <= inMaxTrianglesRequested), or 0 if there are no more triangles.
	/// Note that the function can return a value < inMaxTrianglesRequested and still have more triangles to process (triangles can be returned in blocks).
	/// Note that the function may return triangles outside of the requested box, only coarse culling is performed on the returned triangles.
	virtual int						GetTrianglesNext(GetTrianglesContext &ioContext, int inMaxTrianglesRequested, Float3 *outTriangleVertices, const PhysicsMaterial **outMaterials = nullptr) const = 0;

	///@name Binary serialization of the shape. Note that this saves the 'cooked' shape in a format which will not be backwards compatible for newer library versions.
	/// In this case you need to recreate the shape from the ShapeSettings object and save it again. The user is expected to call SaveBinaryState followed by SaveMaterialState and SaveSubShapeState.
	/// The stream should be stored as is and the material and shape list should be saved using the applications own serialization system (e.g. by assigning an ID to each pointer).
	/// When restoring data, call sRestoreFromBinaryState to get the shape and then call RestoreMaterialState and RestoreSubShapeState to restore the pointers to the external objects.
	/// Alternatively you can use SaveWithChildren and sRestoreWithChildren to save and restore the shape and all its child shapes and materials in a single stream.
	///@{

	/// Saves the contents of the shape in binary form to inStream.
	virtual void					SaveBinaryState(StreamOut &inStream) const;

	/// Creates a Shape of the correct type and restores its contents from the binary stream inStream.
	static ShapeResult				sRestoreFromBinaryState(StreamIn &inStream);

	/// Outputs the material references that this shape has to outMaterials.
	virtual void					SaveMaterialState([[maybe_unused]] PhysicsMaterialList &outMaterials) const			{ /* By default do nothing */ }

	/// Restore the material references after calling sRestoreFromBinaryState. Note that the exact same materials need to be provided in the same order as returned by SaveMaterialState.
	virtual void					RestoreMaterialState([[maybe_unused]] const PhysicsMaterialRefC *inMaterials, [[maybe_unused]] uint inNumMaterials) { JPH_ASSERT(inNumMaterials == 0); }

	/// Outputs the shape references that this shape has to outSubShapes.
	virtual void					SaveSubShapeState([[maybe_unused]] ShapeList &outSubShapes) const					{ /* By default do nothing */ }

	/// Restore the shape references after calling sRestoreFromBinaryState. Note that the exact same shapes need to be provided in the same order as returned by SaveSubShapeState.
	virtual void					RestoreSubShapeState([[maybe_unused]] const ShapeRefC *inSubShapes, [[maybe_unused]] uint inNumShapes) { JPH_ASSERT(inNumShapes == 0); }

	using ShapeToIDMap = StreamUtils::ObjectToIDMap<Shape>;
	using IDToShapeMap = StreamUtils::IDToObjectMap<Shape>;
	using MaterialToIDMap = StreamUtils::ObjectToIDMap<PhysicsMaterial>;
	using IDToMaterialMap = StreamUtils::IDToObjectMap<PhysicsMaterial>;

	/// Save this shape, all its children and its materials. Pass in an empty map in ioShapeMap / ioMaterialMap or reuse the same map while saving multiple shapes to the same stream in order to avoid writing duplicates.
	void							SaveWithChildren(StreamOut &inStream, ShapeToIDMap &ioShapeMap, MaterialToIDMap &ioMaterialMap) const;

	/// Restore a shape, all its children and materials. Pass in an empty map in ioShapeMap / ioMaterialMap or reuse the same map while reading multiple shapes from the same stream in order to restore duplicates.
	static ShapeResult				sRestoreWithChildren(StreamIn &inStream, IDToShapeMap &ioShapeMap, IDToMaterialMap &ioMaterialMap);

	///@}

	/// Class that holds information about the shape that can be used for logging / data collection purposes
	struct Stats
	{
									Stats(size_t inSizeBytes, uint inNumTriangles) : mSizeBytes(inSizeBytes), mNumTriangles(inNumTriangles) { }

		size_t						mSizeBytes;				///< Amount of memory used by this shape (size in bytes)
		uint						mNumTriangles;			///< Number of triangles in this shape (when applicable)
	};

	/// Get stats of this shape. Use for logging / data collection purposes only. Does not add values from child shapes, use GetStatsRecursive for this.
	virtual Stats					GetStats() const = 0;

	using VisitedShapes = UnorderedSet<const Shape *>;

	/// Get the combined stats of this shape and its children.
	/// @param ioVisitedShapes is used to track which shapes have already been visited, to avoid calculating the wrong memory size.
	virtual Stats					GetStatsRecursive(VisitedShapes &ioVisitedShapes) const;

	///< Volume of this shape (m^3). Note that for compound shapes the volume may be incorrect since child shapes can overlap which is not accounted for.
	virtual float					GetVolume() const = 0;

	/// Test if inScale is a valid scale for this shape. Some shapes can only be scaled uniformly, compound shapes cannot handle shapes
	/// being rotated and scaled (this would cause shearing), scale can never be zero. When the scale is invalid, the function will return false.
	///
	/// Here's a list of supported scales:
	/// * SphereShape: Scale must be uniform (signs of scale are ignored).
	/// * BoxShape: Any scale supported (signs of scale are ignored).
	/// * TriangleShape: Any scale supported when convex radius is zero, otherwise only uniform scale supported.
	/// * CapsuleShape: Scale must be uniform (signs of scale are ignored).
	/// * TaperedCapsuleShape: Scale must be uniform (sign of Y scale can be used to flip the capsule).
	/// * CylinderShape: Scale must be uniform in XZ plane, Y can scale independently (signs of scale are ignored).
	/// * RotatedTranslatedShape: Scale must not cause shear in the child shape.
	/// * CompoundShape: Scale must not cause shear in any of the child shapes.
	virtual bool					IsValidScale(Vec3Arg inScale) const;

	/// This function will make sure that if you wrap this shape in a ScaledShape that the scale is valid.
	/// Note that this involves discarding components of the scale that are invalid, so the resulting scaled shape may be different than the requested scale.
	/// Compare the return value of this function with the scale you passed in to detect major inconsistencies and possibly warn the user.
	/// @param inScale Local space scale for this shape.
	/// @return Scale that can be used to wrap this shape in a ScaledShape. IsValidScale will return true for this scale.
	virtual Vec3					MakeScaleValid(Vec3Arg inScale) const;

#ifdef JPH_DEBUG_RENDERER
	/// Debug helper which draws the intersection between water and the shapes, the center of buoyancy and the submerged volume
	static bool						sDrawSubmergedVolumes;
#endif // JPH_DEBUG_RENDERER

protected:
	/// This function should not be called directly, it is used by sRestoreFromBinaryState.
	virtual void					RestoreBinaryState(StreamIn &inStream);

	/// A fallback version of CollidePoint that uses a ray cast and counts the number of hits to determine if the point is inside the shape. Odd number of hits means inside, even number of hits means outside.
	static void						sCollidePointUsingRayCast(const Shape &inShape, Vec3Arg inPoint, const SubShapeIDCreator &inSubShapeIDCreator, CollidePointCollector &ioCollector, const ShapeFilter &inShapeFilter);

private:
	uint64							mUserData = 0;
	EShapeType						mShapeType;
	EShapeSubType					mShapeSubType;
};

JPH_NAMESPACE_END

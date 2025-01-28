// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Jolt/Physics/Collision/ObjectLayer.h>
#include <Jolt/Physics/Collision/ShapeFilter.h>
#include <Jolt/Physics/Collision/Shape/Shape.h>
#include <Jolt/Physics/Collision/Shape/SubShapeID.h>
#include <Jolt/Physics/Collision/BackFaceMode.h>
#include <Jolt/Physics/Body/BodyID.h>

JPH_NAMESPACE_BEGIN

struct RRayCast;
struct RShapeCast;
class CollideShapeSettings;
class RayCastResult;

/// Temporary data structure that contains a shape and a transform.
/// This structure can be obtained from a body (e.g. after a broad phase query) under lock protection.
/// The lock can then be released and collision detection operations can be safely performed since
/// the class takes a reference on the shape and does not use anything from the body anymore.
class JPH_EXPORT TransformedShape
{
public:
	JPH_OVERRIDE_NEW_DELETE

	/// Constructor
								TransformedShape() = default;
								TransformedShape(RVec3Arg inPositionCOM, QuatArg inRotation, const Shape *inShape, const BodyID &inBodyID, const SubShapeIDCreator &inSubShapeIDCreator = SubShapeIDCreator()) : mShapePositionCOM(inPositionCOM), mShapeRotation(inRotation), mShape(inShape), mBodyID(inBodyID), mSubShapeIDCreator(inSubShapeIDCreator) { }

	/// Cast a ray and find the closest hit. Returns true if it finds a hit. Hits further than ioHit.mFraction will not be considered and in this case ioHit will remain unmodified (and the function will return false).
	/// Convex objects will be treated as solid (meaning if the ray starts inside, you'll get a hit fraction of 0) and back face hits are returned.
	/// If you want the surface normal of the hit use GetWorldSpaceSurfaceNormal(ioHit.mSubShapeID2, inRay.GetPointOnRay(ioHit.mFraction)) on this object.
	bool						CastRay(const RRayCast &inRay, RayCastResult &ioHit) const;

	/// Cast a ray, allows collecting multiple hits. Note that this version is more flexible but also slightly slower than the CastRay function that returns only a single hit.
	/// If you want the surface normal of the hit use GetWorldSpaceSurfaceNormal(collected sub shape ID, inRay.GetPointOnRay(collected fraction)) on this object.
	void						CastRay(const RRayCast &inRay, const RayCastSettings &inRayCastSettings, CastRayCollector &ioCollector, const ShapeFilter &inShapeFilter = { }) const;

	/// Check if inPoint is inside any shapes. For this tests all shapes are treated as if they were solid.
	/// For a mesh shape, this test will only provide sensible information if the mesh is a closed manifold.
	/// For each shape that collides, ioCollector will receive a hit
	void						CollidePoint(RVec3Arg inPoint, CollidePointCollector &ioCollector, const ShapeFilter &inShapeFilter = { }) const;

	/// Collide a shape and report any hits to ioCollector
	/// @param inShape Shape to test
	/// @param inShapeScale Scale in local space of shape
	/// @param inCenterOfMassTransform Center of mass transform for the shape
	/// @param inCollideShapeSettings Settings
	/// @param inBaseOffset All hit results will be returned relative to this offset, can be zero to get results in world position, but when you're testing far from the origin you get better precision by picking a position that's closer e.g. mShapePositionCOM since floats are most accurate near the origin
	/// @param ioCollector Collector that receives the hits
	/// @param inShapeFilter Filter that allows you to reject collisions
	void						CollideShape(const Shape *inShape, Vec3Arg inShapeScale, RMat44Arg inCenterOfMassTransform, const CollideShapeSettings &inCollideShapeSettings, RVec3Arg inBaseOffset, CollideShapeCollector &ioCollector, const ShapeFilter &inShapeFilter = { }) const;

	/// Cast a shape and report any hits to ioCollector
	/// @param inShapeCast The shape cast and its position and direction
	/// @param inShapeCastSettings Settings for the shape cast
	/// @param inBaseOffset All hit results will be returned relative to this offset, can be zero to get results in world position, but when you're testing far from the origin you get better precision by picking a position that's closer e.g. mShapePositionCOM or inShapeCast.mCenterOfMassStart.GetTranslation() since floats are most accurate near the origin
	/// @param ioCollector Collector that receives the hits
	/// @param inShapeFilter Filter that allows you to reject collisions
	void						CastShape(const RShapeCast &inShapeCast, const ShapeCastSettings &inShapeCastSettings, RVec3Arg inBaseOffset, CastShapeCollector &ioCollector, const ShapeFilter &inShapeFilter = { }) const;

	/// Collect the leaf transformed shapes of all leaf shapes of this shape
	/// inBox is the world space axis aligned box which leaf shapes should collide with
	void						CollectTransformedShapes(const AABox &inBox, TransformedShapeCollector &ioCollector, const ShapeFilter &inShapeFilter = { }) const;

	/// Use the context from Shape
	using GetTrianglesContext = Shape::GetTrianglesContext;

	/// To start iterating over triangles, call this function first.
	/// To get the actual triangles call GetTrianglesNext.
	/// @param ioContext A temporary buffer and should remain untouched until the last call to GetTrianglesNext.
	/// @param inBox The world space bounding in which you want to get the triangles.
	/// @param inBaseOffset All hit results will be returned relative to this offset, can be zero to get results in world position, but when you're testing far from the origin you get better precision by picking a position that's closer e.g. inBox.GetCenter() since floats are most accurate near the origin
	void						GetTrianglesStart(GetTrianglesContext &ioContext, const AABox &inBox, RVec3Arg inBaseOffset) const;

	/// Call this repeatedly to get all triangles in the box.
	/// outTriangleVertices should be large enough to hold 3 * inMaxTriangleRequested entries
	/// outMaterials (if it is not null) should contain inMaxTrianglesRequested entries
	/// The function returns the amount of triangles that it found (which will be <= inMaxTrianglesRequested), or 0 if there are no more triangles.
	/// Note that the function can return a value < inMaxTrianglesRequested and still have more triangles to process (triangles can be returned in blocks)
	/// Note that the function may return triangles outside of the requested box, only coarse culling is performed on the returned triangles
	int							GetTrianglesNext(GetTrianglesContext &ioContext, int inMaxTrianglesRequested, Float3 *outTriangleVertices, const PhysicsMaterial **outMaterials = nullptr) const;

	/// Get/set the scale of the shape as a Vec3
	inline Vec3					GetShapeScale() const						{ return Vec3::sLoadFloat3Unsafe(mShapeScale); }
	inline void					SetShapeScale(Vec3Arg inScale)				{ inScale.StoreFloat3(&mShapeScale); }

	/// Calculates the transform for this shape's center of mass (excluding scale)
	inline RMat44				GetCenterOfMassTransform() const			{ return RMat44::sRotationTranslation(mShapeRotation, mShapePositionCOM); }

	/// Calculates the inverse of the transform for this shape's center of mass (excluding scale)
	inline RMat44				GetInverseCenterOfMassTransform() const		{ return RMat44::sInverseRotationTranslation(mShapeRotation, mShapePositionCOM); }

	/// Sets the world transform (including scale) of this transformed shape (not from the center of mass but in the space the shape was created)
	inline void					SetWorldTransform(RVec3Arg inPosition, QuatArg inRotation, Vec3Arg inScale)
	{
		mShapePositionCOM = inPosition + inRotation * (inScale * mShape->GetCenterOfMass());
		mShapeRotation = inRotation;
		SetShapeScale(inScale);
	}

	/// Sets the world transform (including scale) of this transformed shape (not from the center of mass but in the space the shape was created)
	inline void					SetWorldTransform(RMat44Arg inTransform)
	{
		Vec3 scale;
		RMat44 rot_trans = inTransform.Decompose(scale);
		SetWorldTransform(rot_trans.GetTranslation(), rot_trans.GetQuaternion(), scale);
	}

	/// Calculates the world transform including scale of this shape (not from the center of mass but in the space the shape was created)
	inline RMat44				GetWorldTransform() const
	{
		RMat44 transform = RMat44::sRotation(mShapeRotation).PreScaled(GetShapeScale());
		transform.SetTranslation(mShapePositionCOM - transform.Multiply3x3(mShape->GetCenterOfMass()));
		return transform;
	}

	/// Get the world space bounding box for this transformed shape
	AABox						GetWorldSpaceBounds() const					{ return mShape != nullptr? mShape->GetWorldSpaceBounds(GetCenterOfMassTransform(), GetShapeScale()) : AABox(); }

	/// Make inSubShapeID relative to mShape. When mSubShapeIDCreator is not empty, this is needed in order to get the correct path to the sub shape.
	inline SubShapeID			MakeSubShapeIDRelativeToShape(const SubShapeID &inSubShapeID) const
	{
		// Take off the sub shape ID part that comes from mSubShapeIDCreator and validate that it is the same
		SubShapeID sub_shape_id;
		uint num_bits_written = mSubShapeIDCreator.GetNumBitsWritten();
		JPH_IF_ENABLE_ASSERTS(uint32 root_id =) inSubShapeID.PopID(num_bits_written, sub_shape_id);
		JPH_ASSERT(root_id == (mSubShapeIDCreator.GetID().GetValue() & ((1 << num_bits_written) - 1)));
		return sub_shape_id;
	}

	/// Get surface normal of a particular sub shape and its world space surface position on this body.
	/// Note: When you have a CollideShapeResult or ShapeCastResult you should use -mPenetrationAxis.Normalized() as contact normal as GetWorldSpaceSurfaceNormal will only return face normals (and not vertex or edge normals).
	inline Vec3					GetWorldSpaceSurfaceNormal(const SubShapeID &inSubShapeID, RVec3Arg inPosition) const
	{
		RMat44 inv_com = GetInverseCenterOfMassTransform();
		Vec3 scale = GetShapeScale(); // See comment at ScaledShape::GetSurfaceNormal for the math behind the scaling of the normal
		return inv_com.Multiply3x3Transposed(mShape->GetSurfaceNormal(MakeSubShapeIDRelativeToShape(inSubShapeID), Vec3(inv_com * inPosition) / scale) / scale).Normalized();
	}

	/// Get the vertices of the face that faces inDirection the most (includes any convex radius). Note that this function can only return faces of
	/// convex shapes or triangles, which is why a sub shape ID to get to that leaf must be provided.
	/// @param inSubShapeID Sub shape ID of target shape
	/// @param inDirection Direction that the face should be facing (in world space)
	/// @param inBaseOffset The vertices will be returned relative to this offset, can be zero to get results in world position, but when you're testing far from the origin you get better precision by picking a position that's closer e.g. mShapePositionCOM since floats are most accurate near the origin
	/// @param outVertices Resulting face. Note the returned face can have a single point if the shape doesn't have polygons to return (e.g. because it's a sphere). The face will be returned in world space.
	void						GetSupportingFace(const SubShapeID &inSubShapeID, Vec3Arg inDirection, RVec3Arg inBaseOffset, Shape::SupportingFace &outVertices) const
	{
		Mat44 com = GetCenterOfMassTransform().PostTranslated(-inBaseOffset).ToMat44();
		mShape->GetSupportingFace(MakeSubShapeIDRelativeToShape(inSubShapeID), com.Multiply3x3Transposed(inDirection), GetShapeScale(), com, outVertices);
	}

	/// Get material of a particular sub shape
	inline const PhysicsMaterial *GetMaterial(const SubShapeID &inSubShapeID) const
	{
		return mShape->GetMaterial(MakeSubShapeIDRelativeToShape(inSubShapeID));
	}

	/// Get the user data of a particular sub shape
	inline uint64				GetSubShapeUserData(const SubShapeID &inSubShapeID) const
	{
		return mShape->GetSubShapeUserData(MakeSubShapeIDRelativeToShape(inSubShapeID));
	}

	/// Get the direct child sub shape and its transform for a sub shape ID.
	/// @param inSubShapeID Sub shape ID that indicates the path to the leaf shape
	/// @param outRemainder The remainder of the sub shape ID after removing the sub shape
	/// @return Direct child sub shape and its transform, note that the body ID and sub shape ID will be invalid
	TransformedShape			GetSubShapeTransformedShape(const SubShapeID &inSubShapeID, SubShapeID &outRemainder) const
	{
		TransformedShape ts = mShape->GetSubShapeTransformedShape(inSubShapeID, Vec3::sZero(), mShapeRotation, GetShapeScale(), outRemainder);
		ts.mShapePositionCOM += mShapePositionCOM;
		return ts;
	}

	/// Helper function to return the body id from a transformed shape. If the transformed shape is null an invalid body ID will be returned.
	inline static BodyID		sGetBodyID(const TransformedShape *inTS)	{ return inTS != nullptr? inTS->mBodyID : BodyID(); }

	RVec3						mShapePositionCOM;							///< Center of mass world position of the shape
	Quat						mShapeRotation;								///< Rotation of the shape
	RefConst<Shape>				mShape;										///< The shape itself
	Float3						mShapeScale { 1, 1, 1 };					///< Not stored as Vec3 to get a nicely packed structure
	BodyID						mBodyID;									///< Optional body ID from which this shape comes
	SubShapeIDCreator			mSubShapeIDCreator;							///< Optional sub shape ID creator for the shape (can be used when expanding compound shapes into multiple transformed shapes)
};

static_assert(JPH_CPU_ADDRESS_BITS != 64 || sizeof(TransformedShape) == JPH_IF_SINGLE_PRECISION_ELSE(64, 96), "Not properly packed");
static_assert(alignof(TransformedShape) == JPH_RVECTOR_ALIGNMENT, "Not properly aligned");

JPH_NAMESPACE_END

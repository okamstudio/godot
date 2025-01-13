// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#ifndef JPH_DEBUG_RENDERER
	#error This file should only be included when JPH_DEBUG_RENDERER is defined
#endif // !JPH_DEBUG_RENDERER

#ifndef JPH_DEBUG_RENDERER_EXPORT
	// By default export the debug renderer
	#define JPH_DEBUG_RENDERER_EXPORT JPH_EXPORT
#endif // !JPH_DEBUG_RENDERER_EXPORT

#include <Jolt/Core/Color.h>
#include <Jolt/Core/Reference.h>
#include <Jolt/Core/HashCombine.h>
#include <Jolt/Core/UnorderedMap.h>
#include <Jolt/Core/NonCopyable.h>
#include <Jolt/Math/Float2.h>
#include <Jolt/Geometry/IndexedTriangle.h>
#include <Jolt/Geometry/AABox.h>

JPH_NAMESPACE_BEGIN

class OrientedBox;

/// Simple triangle renderer for debugging purposes.
///
/// Inherit from this class to provide your own implementation.
///
/// Implement the following virtual functions:
/// - DrawLine
/// - DrawTriangle
/// - DrawText3D
/// - CreateTriangleBatch
/// - DrawGeometry
///
/// Make sure you call Initialize() from the constructor of your implementation.
///
/// The CreateTriangleBatch is used to prepare a batch of triangles to be drawn by a single DrawGeometry call,
/// which means that Jolt can render a complex scene much more efficiently than when each triangle in that scene would have been drawn through DrawTriangle.
///
/// Note that an implementation that implements CreateTriangleBatch and DrawGeometry is provided by DebugRendererSimple which can be used to start quickly.
class JPH_DEBUG_RENDERER_EXPORT DebugRenderer : public NonCopyable
{
public:
	JPH_OVERRIDE_NEW_DELETE

	/// Constructor
							DebugRenderer();
	virtual					~DebugRenderer();

	/// Call once after frame is complete. Releases unused dynamically generated geometry assets.
	void					NextFrame();

	/// Draw line
	virtual void			DrawLine(RVec3Arg inFrom, RVec3Arg inTo, ColorArg inColor) = 0;

	/// Draw wireframe box
	void					DrawWireBox(const AABox &inBox, ColorArg inColor);
	void					DrawWireBox(const OrientedBox &inBox, ColorArg inColor);
	void					DrawWireBox(RMat44Arg inMatrix, const AABox &inBox, ColorArg inColor);

	/// Draw a marker on a position
	void					DrawMarker(RVec3Arg inPosition, ColorArg inColor, float inSize);

	/// Draw an arrow
	void					DrawArrow(RVec3Arg inFrom, RVec3Arg inTo, ColorArg inColor, float inSize);

	/// Draw coordinate system (3 arrows, x = red, y = green, z = blue)
	void					DrawCoordinateSystem(RMat44Arg inTransform, float inSize = 1.0f);

	/// Draw a plane through inPoint with normal inNormal
	void					DrawPlane(RVec3Arg inPoint, Vec3Arg inNormal, ColorArg inColor, float inSize);

	/// Draw wireframe triangle
	void					DrawWireTriangle(RVec3Arg inV1, RVec3Arg inV2, RVec3Arg inV3, ColorArg inColor);

	/// Draw a wireframe polygon
	template <class VERTEX_ARRAY>
	void					DrawWirePolygon(RMat44Arg inTransform, const VERTEX_ARRAY &inVertices, ColorArg inColor, float inArrowSize = 0.0f) { for (typename VERTEX_ARRAY::size_type i = 0; i < inVertices.size(); ++i) DrawArrow(inTransform * inVertices[i], inTransform * inVertices[(i + 1) % inVertices.size()], inColor, inArrowSize); }

	/// Draw wireframe sphere
	void					DrawWireSphere(RVec3Arg inCenter, float inRadius, ColorArg inColor, int inLevel = 3);
	void					DrawWireUnitSphere(RMat44Arg inMatrix, ColorArg inColor, int inLevel = 3);

	/// Enum that determines if a shadow should be cast or not
	enum class ECastShadow
	{
		On,					///< This shape should cast a shadow
		Off					///< This shape should not cast a shadow
	};

	/// Determines how triangles are drawn
	enum class EDrawMode
	{
		Solid,				///< Draw as a solid shape
		Wireframe,			///< Draw as wireframe
	};

	/// Draw a single back face culled triangle
	virtual void			DrawTriangle(RVec3Arg inV1, RVec3Arg inV2, RVec3Arg inV3, ColorArg inColor, ECastShadow inCastShadow = ECastShadow::Off) = 0;

	/// Draw a box
	void					DrawBox(const AABox &inBox, ColorArg inColor, ECastShadow inCastShadow = ECastShadow::On, EDrawMode inDrawMode = EDrawMode::Solid);
	void					DrawBox(RMat44Arg inMatrix, const AABox &inBox, ColorArg inColor, ECastShadow inCastShadow = ECastShadow::On, EDrawMode inDrawMode = EDrawMode::Solid);

	/// Draw a sphere
	void					DrawSphere(RVec3Arg inCenter, float inRadius, ColorArg inColor, ECastShadow inCastShadow = ECastShadow::On, EDrawMode inDrawMode = EDrawMode::Solid);
	void					DrawUnitSphere(RMat44Arg inMatrix, ColorArg inColor, ECastShadow inCastShadow = ECastShadow::On, EDrawMode inDrawMode = EDrawMode::Solid);

	/// Draw a capsule with one half sphere at (0, -inHalfHeightOfCylinder, 0) and the other half sphere at (0, inHalfHeightOfCylinder, 0) and radius inRadius.
	/// The capsule will be transformed by inMatrix.
	void					DrawCapsule(RMat44Arg inMatrix, float inHalfHeightOfCylinder, float inRadius, ColorArg inColor, ECastShadow inCastShadow = ECastShadow::On, EDrawMode inDrawMode = EDrawMode::Solid);

	/// Draw a cylinder with top (0, inHalfHeight, 0) and bottom (0, -inHalfHeight, 0) and radius inRadius.
	/// The cylinder will be transformed by inMatrix
	void					DrawCylinder(RMat44Arg inMatrix, float inHalfHeight, float inRadius, ColorArg inColor, ECastShadow inCastShadow = ECastShadow::On, EDrawMode inDrawMode = EDrawMode::Solid);

	/// Draw a bottomless cone.
	/// @param inTop Top of cone, center of base is at inTop + inAxis.
	/// @param inAxis Height and direction of cone
	/// @param inPerpendicular Perpendicular vector to inAxis.
	/// @param inHalfAngle Specifies the cone angle in radians (angle measured between inAxis and cone surface).
	/// @param inLength The length of the cone.
	/// @param inColor Color to use for drawing the cone.
	/// @param inCastShadow determines if this geometry should cast a shadow or not.
	/// @param inDrawMode determines if we draw the geometry solid or in wireframe.
	void					DrawOpenCone(RVec3Arg inTop, Vec3Arg inAxis, Vec3Arg inPerpendicular, float inHalfAngle, float inLength, ColorArg inColor, ECastShadow inCastShadow = ECastShadow::On, EDrawMode inDrawMode = EDrawMode::Solid);

	/// Draws cone rotation limits as used by the SwingTwistConstraintPart.
	/// @param inMatrix Matrix that transforms from constraint space to world space
	/// @param inSwingYHalfAngle See SwingTwistConstraintPart
	/// @param inSwingZHalfAngle See SwingTwistConstraintPart
	/// @param inEdgeLength Size of the edge of the cone shape
	/// @param inColor Color to use for drawing the cone.
	/// @param inCastShadow determines if this geometry should cast a shadow or not.
	/// @param inDrawMode determines if we draw the geometry solid or in wireframe.
	void					DrawSwingConeLimits(RMat44Arg inMatrix, float inSwingYHalfAngle, float inSwingZHalfAngle, float inEdgeLength, ColorArg inColor, ECastShadow inCastShadow = ECastShadow::On, EDrawMode inDrawMode = EDrawMode::Solid);

	/// Draws rotation limits as used by the SwingTwistConstraintPart.
	/// @param inMatrix Matrix that transforms from constraint space to world space
	/// @param inMinSwingYAngle See SwingTwistConstraintPart
	/// @param inMaxSwingYAngle See SwingTwistConstraintPart
	/// @param inMinSwingZAngle See SwingTwistConstraintPart
	/// @param inMaxSwingZAngle See SwingTwistConstraintPart
	/// @param inEdgeLength Size of the edge of the cone shape
	/// @param inColor Color to use for drawing the cone.
	/// @param inCastShadow determines if this geometry should cast a shadow or not.
	/// @param inDrawMode determines if we draw the geometry solid or in wireframe.
	void					DrawSwingPyramidLimits(RMat44Arg inMatrix, float inMinSwingYAngle, float inMaxSwingYAngle, float inMinSwingZAngle, float inMaxSwingZAngle, float inEdgeLength, ColorArg inColor, ECastShadow inCastShadow = ECastShadow::On, EDrawMode inDrawMode = EDrawMode::Solid);

	/// Draw a pie (part of a circle).
	/// @param inCenter The center of the circle.
	/// @param inRadius Radius of the circle.
	/// @param inNormal The plane normal in which the pie resides.
	/// @param inAxis The axis that defines an angle of 0 radians.
	/// @param inMinAngle The pie will be drawn between [inMinAngle, inMaxAngle] (in radians).
	/// @param inMaxAngle The pie will be drawn between [inMinAngle, inMaxAngle] (in radians).
	/// @param inColor Color to use for drawing the pie.
	/// @param inCastShadow determines if this geometry should cast a shadow or not.
	/// @param inDrawMode determines if we draw the geometry solid or in wireframe.
	void					DrawPie(RVec3Arg inCenter, float inRadius, Vec3Arg inNormal, Vec3Arg inAxis, float inMinAngle, float inMaxAngle, ColorArg inColor, ECastShadow inCastShadow = ECastShadow::On, EDrawMode inDrawMode = EDrawMode::Solid);

	/// Draw a tapered cylinder
	/// @param inMatrix Matrix that transforms the cylinder to world space.
	/// @param inTop Top of cylinder (along Y axis)
	/// @param inBottom Bottom of cylinder (along Y axis)
	/// @param inTopRadius Radius at the top
	/// @param inBottomRadius Radius at the bottom
	/// @param inColor Color to use for drawing the pie.
	/// @param inCastShadow determines if this geometry should cast a shadow or not.
	/// @param inDrawMode determines if we draw the geometry solid or in wireframe.
	void					DrawTaperedCylinder(RMat44Arg inMatrix, float inTop, float inBottom, float inTopRadius, float inBottomRadius, ColorArg inColor, ECastShadow inCastShadow = ECastShadow::On, EDrawMode inDrawMode = EDrawMode::Solid);

	/// Singleton instance
	static DebugRenderer *	sInstance;

	/// Vertex format used by the triangle renderer
	class Vertex
	{
	public:
		Float3				mPosition;
		Float3				mNormal;
		Float2				mUV;
		Color				mColor;
	};

	/// A single triangle
	class JPH_DEBUG_RENDERER_EXPORT Triangle
	{
	public:
							Triangle() = default;
							Triangle(Vec3Arg inV1, Vec3Arg inV2, Vec3Arg inV3, ColorArg inColor);
							Triangle(Vec3Arg inV1, Vec3Arg inV2, Vec3Arg inV3, ColorArg inColor, Vec3Arg inUVOrigin, Vec3Arg inUVDirection);

		Vertex				mV[3];
	};

	/// Handle for a batch of triangles
	using Batch = Ref<RefTargetVirtual>;

	/// A single level of detail
	class LOD
	{
	public:
		Batch				mTriangleBatch;
		float				mDistance;
	};

	/// A geometry primitive containing triangle batches for various lods
	class Geometry : public RefTarget<Geometry>
	{
	public:
		JPH_OVERRIDE_NEW_DELETE

		/// Constructor
							Geometry(const AABox &inBounds) : mBounds(inBounds) { }
							Geometry(const Batch &inBatch, const AABox &inBounds) : mBounds(inBounds) { mLODs.push_back({ inBatch, FLT_MAX }); }

		/// Determine which LOD to render
		/// @param inCameraPosition Current position of the camera
		/// @param inWorldSpaceBounds World space bounds for this geometry (transform mBounds by model space matrix)
		/// @param inLODScaleSq is the squared scale of the model matrix, it is multiplied with the LOD distances in inGeometry to calculate the real LOD distance (so a number > 1 will force a higher LOD).
		/// @return The selected LOD.
		const LOD &			GetLOD(Vec3Arg inCameraPosition, const AABox &inWorldSpaceBounds, float inLODScaleSq) const
		{
			float dist_sq = inWorldSpaceBounds.GetSqDistanceTo(inCameraPosition);
			for (const LOD &lod : mLODs)
				if (dist_sq <= inLODScaleSq * Square(lod.mDistance))
					return lod;

			return mLODs.back();
		}

		/// All level of details for this mesh
		Array<LOD>			mLODs;

		/// Bounding box that encapsulates all LODs
		AABox				mBounds;
	};

	/// Handle for a lodded triangle batch
	using GeometryRef = Ref<Geometry>;

	/// Calculate bounding box for a batch of triangles
	static AABox			sCalculateBounds(const Vertex *inVertices, int inVertexCount);

	/// Create a batch of triangles that can be drawn efficiently
	virtual Batch			CreateTriangleBatch(const Triangle *inTriangles, int inTriangleCount) = 0;
	virtual Batch			CreateTriangleBatch(const Vertex *inVertices, int inVertexCount, const uint32 *inIndices, int inIndexCount) = 0;
	Batch					CreateTriangleBatch(const Array<Triangle> &inTriangles) { return CreateTriangleBatch(inTriangles.empty()? nullptr : &inTriangles[0], int(inTriangles.size())); }
	Batch					CreateTriangleBatch(const Array<Vertex> &inVertices, const Array<uint32> &inIndices) { return CreateTriangleBatch(inVertices.empty()? nullptr : &inVertices[0], int(inVertices.size()), inIndices.empty()? nullptr : &inIndices[0], int(inIndices.size())); }
	Batch					CreateTriangleBatch(const VertexList &inVertices, const IndexedTriangleNoMaterialList &inTriangles);

	/// Create a primitive for a convex shape using its support function
	using SupportFunction = function<Vec3 (Vec3Arg inDirection)>;
	Batch					CreateTriangleBatchForConvex(SupportFunction inGetSupport, int inLevel, AABox *outBounds = nullptr);
	GeometryRef				CreateTriangleGeometryForConvex(SupportFunction inGetSupport);

	/// Determines which polygons are culled
	enum class ECullMode
	{
		CullBackFace,		///< Don't draw backfacing polygons
		CullFrontFace,		///< Don't draw front facing polygons
		Off					///< Don't do culling and draw both sides
	};

	/// Draw some geometry
	/// @param inModelMatrix is the matrix that transforms the geometry to world space.
	/// @param inWorldSpaceBounds is the bounding box of the geometry after transforming it into world space.
	/// @param inLODScaleSq is the squared scale of the model matrix, it is multiplied with the LOD distances in inGeometry to calculate the real LOD distance (so a number > 1 will force a higher LOD).
	/// @param inModelColor is the color with which to multiply the vertex colors in inGeometry.
	/// @param inGeometry The geometry to draw.
	/// @param inCullMode determines which polygons are culled.
	/// @param inCastShadow determines if this geometry should cast a shadow or not.
	/// @param inDrawMode determines if we draw the geometry solid or in wireframe.
	virtual void			DrawGeometry(RMat44Arg inModelMatrix, const AABox &inWorldSpaceBounds, float inLODScaleSq, ColorArg inModelColor, const GeometryRef &inGeometry, ECullMode inCullMode = ECullMode::CullBackFace, ECastShadow inCastShadow = ECastShadow::On, EDrawMode inDrawMode = EDrawMode::Solid) = 0;
	void					DrawGeometry(RMat44Arg inModelMatrix, ColorArg inModelColor, const GeometryRef &inGeometry, ECullMode inCullMode = ECullMode::CullBackFace, ECastShadow inCastShadow = ECastShadow::On, EDrawMode inDrawMode = EDrawMode::Solid) { DrawGeometry(inModelMatrix, inGeometry->mBounds.Transformed(inModelMatrix), max(max(inModelMatrix.GetAxisX().LengthSq(), inModelMatrix.GetAxisY().LengthSq()), inModelMatrix.GetAxisZ().LengthSq()), inModelColor, inGeometry, inCullMode, inCastShadow, inDrawMode); }

	/// Draw text
	virtual void			DrawText3D(RVec3Arg inPosition, const string_view &inString, ColorArg inColor = Color::sWhite, float inHeight = 0.5f) = 0;

protected:
	/// Initialize the system, must be called from the constructor of the DebugRenderer implementation
	void					Initialize();

private:
	/// Recursive helper function for DrawWireUnitSphere
	void					DrawWireUnitSphereRecursive(RMat44Arg inMatrix, ColorArg inColor, Vec3Arg inDir1, Vec3Arg inDir2, Vec3Arg inDir3, int inLevel);

	/// Helper functions to create a box
	void					CreateQuad(Array<uint32> &ioIndices, Array<Vertex> &ioVertices, Vec3Arg inV1, Vec3Arg inV2, Vec3Arg inV3, Vec3Arg inV4);

	/// Helper functions to create a vertex and index buffer for a sphere
	void					Create8thSphereRecursive(Array<uint32> &ioIndices, Array<Vertex> &ioVertices, Vec3Arg inDir1, uint32 &ioIdx1, Vec3Arg inDir2, uint32 &ioIdx2, Vec3Arg inDir3, uint32 &ioIdx3, const Float2 &inUV, SupportFunction inGetSupport, int inLevel);
	void					Create8thSphere(Array<uint32> &ioIndices, Array<Vertex> &ioVertices, Vec3Arg inDir1, Vec3Arg inDir2, Vec3Arg inDir3, const Float2 &inUV, SupportFunction inGetSupport, int inLevel);

	/// Helper functions to create a vertex and index buffer for a cylinder
	Batch					CreateCylinder(float inTop, float inBottom, float inTopRadius, float inBottomRadius, int inLevel);

	/// Helper function for DrawSwingConeLimits and DrawSwingPyramidLimits
	Geometry *				CreateSwingLimitGeometry(int inNumSegments, const Vec3 *inVertices);

	// Predefined shapes
	GeometryRef				mBox;
	GeometryRef				mSphere;
	GeometryRef				mCapsuleTop;
	GeometryRef				mCapsuleMid;
	GeometryRef				mCapsuleBottom;
	GeometryRef				mOpenCone;
	GeometryRef				mCylinder;

	struct SwingConeLimits
	{
		bool				operator == (const SwingConeLimits &inRHS) const
		{
			return mSwingYHalfAngle == inRHS.mSwingYHalfAngle
				&& mSwingZHalfAngle == inRHS.mSwingZHalfAngle;
		}

		float				mSwingYHalfAngle;
		float				mSwingZHalfAngle;
	};

	JPH_MAKE_HASH_STRUCT(SwingConeLimits, SwingConeLimitsHasher, t.mSwingYHalfAngle, t.mSwingZHalfAngle)

	using SwingConeBatches = UnorderedMap<SwingConeLimits, GeometryRef, SwingConeLimitsHasher>;
	SwingConeBatches		mSwingConeLimits;
	SwingConeBatches		mPrevSwingConeLimits;

	struct SwingPyramidLimits
	{
		bool				operator == (const SwingPyramidLimits &inRHS) const
		{
			return mMinSwingYAngle == inRHS.mMinSwingYAngle
				&& mMaxSwingYAngle == inRHS.mMaxSwingYAngle
				&& mMinSwingZAngle == inRHS.mMinSwingZAngle
				&& mMaxSwingZAngle == inRHS.mMaxSwingZAngle;
		}

		float				mMinSwingYAngle;
		float				mMaxSwingYAngle;
		float				mMinSwingZAngle;
		float				mMaxSwingZAngle;
	};

	JPH_MAKE_HASH_STRUCT(SwingPyramidLimits, SwingPyramidLimitsHasher, t.mMinSwingYAngle, t.mMaxSwingYAngle, t.mMinSwingZAngle, t.mMaxSwingZAngle)

	using SwingPyramidBatches = UnorderedMap<SwingPyramidLimits, GeometryRef, SwingPyramidLimitsHasher>;
	SwingPyramidBatches		mSwingPyramidLimits;
	SwingPyramidBatches		mPrevSwingPyramidLimits;

	using PieBatces = UnorderedMap<float, GeometryRef>;
	PieBatces				mPieLimits;
	PieBatces				mPrevPieLimits;

	struct TaperedCylinder
	{
		bool				operator == (const TaperedCylinder &inRHS) const
		{
			return mTop == inRHS.mTop
				&& mBottom == inRHS.mBottom
				&& mTopRadius == inRHS.mTopRadius
				&& mBottomRadius == inRHS.mBottomRadius;
		}

		float				mTop;
		float				mBottom;
		float				mTopRadius;
		float				mBottomRadius;
	};

	JPH_MAKE_HASH_STRUCT(TaperedCylinder, TaperedCylinderHasher, t.mTop, t.mBottom, t.mTopRadius, t.mBottomRadius)

	using TaperedCylinderBatces = UnorderedMap<TaperedCylinder, GeometryRef, TaperedCylinderHasher>;
	TaperedCylinderBatces	mTaperedCylinders;
	TaperedCylinderBatces	mPrevTaperedCylinders;
};

JPH_NAMESPACE_END

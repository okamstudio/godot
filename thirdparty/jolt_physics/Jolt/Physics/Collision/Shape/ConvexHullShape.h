// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Jolt/Physics/Collision/Shape/ConvexShape.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Geometry/Plane.h>
#ifdef JPH_DEBUG_RENDERER
	#include <Jolt/Renderer/DebugRenderer.h>
#endif // JPH_DEBUG_RENDERER

JPH_NAMESPACE_BEGIN

/// Class that constructs a ConvexHullShape
class JPH_EXPORT ConvexHullShapeSettings final : public ConvexShapeSettings
{
	JPH_DECLARE_SERIALIZABLE_VIRTUAL(JPH_EXPORT, ConvexHullShapeSettings)

public:
	/// Default constructor for deserialization
							ConvexHullShapeSettings() = default;

	/// Create a convex hull from inPoints and maximum convex radius inMaxConvexRadius, the radius is automatically lowered if the hull requires it.
	/// (internally this will be subtracted so the total size will not grow with the convex radius).
							ConvexHullShapeSettings(const Vec3 *inPoints, int inNumPoints, float inMaxConvexRadius = cDefaultConvexRadius, const PhysicsMaterial *inMaterial = nullptr) : ConvexShapeSettings(inMaterial), mPoints(inPoints, inPoints + inNumPoints), mMaxConvexRadius(inMaxConvexRadius) { }
							ConvexHullShapeSettings(const Array<Vec3> &inPoints, float inConvexRadius = cDefaultConvexRadius, const PhysicsMaterial *inMaterial = nullptr) : ConvexShapeSettings(inMaterial), mPoints(inPoints), mMaxConvexRadius(inConvexRadius) { }

	// See: ShapeSettings
	virtual ShapeResult		Create() const override;

	Array<Vec3>				mPoints;															///< Points to create the hull from
	float					mMaxConvexRadius = 0.0f;											///< Convex radius as supplied by the constructor. Note that during hull creation the convex radius can be made smaller if the value is too big for the hull.
	float					mMaxErrorConvexRadius = 0.05f;										///< Maximum distance between the shrunk hull + convex radius and the actual hull.
	float					mHullTolerance = 1.0e-3f;											///< Points are allowed this far outside of the hull (increasing this yields a hull with less vertices). Note that the actual used value can be larger if the points of the hull are far apart.
};

/// A convex hull
class JPH_EXPORT ConvexHullShape final : public ConvexShape
{
public:
	JPH_OVERRIDE_NEW_DELETE

	/// Maximum amount of points supported in a convex hull. Note that while constructing a hull, interior points are discarded so you can provide more points.
	/// The ConvexHullShapeSettings::Create function will return an error when too many points are provided.
	static constexpr int	cMaxPointsInHull = 256;

	/// Constructor
							ConvexHullShape() : ConvexShape(EShapeSubType::ConvexHull) { }
							ConvexHullShape(const ConvexHullShapeSettings &inSettings, ShapeResult &outResult);

	// See Shape::GetCenterOfMass
	virtual Vec3			GetCenterOfMass() const override									{ return mCenterOfMass; }

	// See Shape::GetLocalBounds
	virtual AABox			GetLocalBounds() const override										{ return mLocalBounds; }

	// See Shape::GetInnerRadius
	virtual float			GetInnerRadius() const override										{ return mInnerRadius; }

	// See Shape::GetMassProperties
	virtual MassProperties	GetMassProperties() const override;

	// See Shape::GetSurfaceNormal
	virtual Vec3			GetSurfaceNormal(const SubShapeID &inSubShapeID, Vec3Arg inLocalSurfacePosition) const override;

	// See Shape::GetSupportingFace
	virtual void			GetSupportingFace(const SubShapeID &inSubShapeID, Vec3Arg inDirection, Vec3Arg inScale, Mat44Arg inCenterOfMassTransform, SupportingFace &outVertices) const override;

	// See ConvexShape::GetSupportFunction
	virtual const Support *	GetSupportFunction(ESupportMode inMode, SupportBuffer &inBuffer, Vec3Arg inScale) const override;

	// See Shape::GetSubmergedVolume
	virtual void			GetSubmergedVolume(Mat44Arg inCenterOfMassTransform, Vec3Arg inScale, const Plane &inSurface, float &outTotalVolume, float &outSubmergedVolume, Vec3 &outCenterOfBuoyancy JPH_IF_DEBUG_RENDERER(, RVec3Arg inBaseOffset)) const override;

#ifdef JPH_DEBUG_RENDERER
	// See Shape::Draw
	virtual void			Draw(DebugRenderer *inRenderer, RMat44Arg inCenterOfMassTransform, Vec3Arg inScale, ColorArg inColor, bool inUseMaterialColors, bool inDrawWireframe) const override;

	/// Debugging helper draw function that draws how all points are moved when a shape is shrunk by the convex radius
	void					DrawShrunkShape(DebugRenderer *inRenderer, RMat44Arg inCenterOfMassTransform, Vec3Arg inScale) const;
#endif // JPH_DEBUG_RENDERER

	// See Shape::CastRay
	virtual bool			CastRay(const RayCast &inRay, const SubShapeIDCreator &inSubShapeIDCreator, RayCastResult &ioHit) const override;
	virtual void			CastRay(const RayCast &inRay, const RayCastSettings &inRayCastSettings, const SubShapeIDCreator &inSubShapeIDCreator, CastRayCollector &ioCollector, const ShapeFilter &inShapeFilter = { }) const override;

	// See: Shape::CollidePoint
	virtual void			CollidePoint(Vec3Arg inPoint, const SubShapeIDCreator &inSubShapeIDCreator, CollidePointCollector &ioCollector, const ShapeFilter &inShapeFilter = { }) const override;

	// See: Shape::CollideSoftBodyVertices
	virtual void			CollideSoftBodyVertices(Mat44Arg inCenterOfMassTransform, Vec3Arg inScale, const CollideSoftBodyVertexIterator &inVertices, uint inNumVertices, int inCollidingShapeIndex) const override;

	// See Shape::GetTrianglesStart
	virtual void			GetTrianglesStart(GetTrianglesContext &ioContext, const AABox &inBox, Vec3Arg inPositionCOM, QuatArg inRotation, Vec3Arg inScale) const override;

	// See Shape::GetTrianglesNext
	virtual int				GetTrianglesNext(GetTrianglesContext &ioContext, int inMaxTrianglesRequested, Float3 *outTriangleVertices, const PhysicsMaterial **outMaterials = nullptr) const override;

	// See Shape
	virtual void			SaveBinaryState(StreamOut &inStream) const override;

	// See Shape::GetStats
	virtual Stats			GetStats() const override;

	// See Shape::GetVolume
	virtual float			GetVolume() const override											{ return mVolume; }

	/// Get the convex radius of this convex hull
	float					GetConvexRadius() const												{ return mConvexRadius; }

	/// Get the planes of this convex hull
	const Array<Plane> &	GetPlanes() const													{ return mPlanes; }

	/// Get the number of vertices in this convex hull
	inline uint				GetNumPoints() const												{ return uint(mPoints.size()); }

	/// Get a vertex of this convex hull relative to the center of mass
	inline Vec3				GetPoint(uint inIndex) const										{ return mPoints[inIndex].mPosition; }

	/// Get the number of faces in this convex hull
	inline uint				GetNumFaces() const													{ return uint(mFaces.size()); }

	/// Get the number of vertices in a face
	inline uint				GetNumVerticesInFace(uint inFaceIndex) const						{ return mFaces[inFaceIndex].mNumVertices; }

	/// Get the vertices indices of a face
	/// @param inFaceIndex Index of the face.
	/// @param inMaxVertices Maximum number of vertices to return.
	/// @param outVertices Array of vertices indices, must be at least inMaxVertices in size, the vertices are returned in counter clockwise order and the positions can be obtained using GetPoint(index).
	/// @return Number of vertices in face, if this is bigger than inMaxVertices, not all vertices were retrieved.
	inline uint				GetFaceVertices(uint inFaceIndex, uint inMaxVertices, uint *outVertices) const
	{
		const Face &face = mFaces[inFaceIndex];
		const uint8 *first_vertex = mVertexIdx.data() + face.mFirstVertex;
		uint num_vertices = min<uint>(face.mNumVertices, inMaxVertices);
		for (uint i = 0; i < num_vertices; ++i)
			outVertices[i] = first_vertex[i];
		return face.mNumVertices;
	}

	// Register shape functions with the registry
	static void				sRegister();

#ifdef JPH_DEBUG_RENDERER
	/// Draw the outlines of the faces of the convex hull when drawing the shape
	inline static bool		sDrawFaceOutlines = false;
#endif // JPH_DEBUG_RENDERER

protected:
	// See: Shape::RestoreBinaryState
	virtual void			RestoreBinaryState(StreamIn &inStream) override;

private:
	/// Helper function that returns the min and max fraction along the ray that hits the convex hull. Returns false if there is no hit.
	bool					CastRayHelper(const RayCast &inRay, float &outMinFraction, float &outMaxFraction) const;

	/// Class for GetTrianglesStart/Next
	class					CHSGetTrianglesContext;

	/// Classes for GetSupportFunction
	class					HullNoConvex;
	class					HullWithConvex;
	class					HullWithConvexScaled;

	struct Face
	{
		uint16				mFirstVertex;				///< First index in mVertexIdx to use
		uint16				mNumVertices = 0;			///< Number of vertices in the mVertexIdx to use
	};

	static_assert(sizeof(Face) == 4, "Unexpected size");
	static_assert(alignof(Face) == 2, "Unexpected alignment");

	struct Point
	{
		Vec3				mPosition;					///< Position of vertex
		int					mNumFaces = 0;				///< Number of faces in the face array below
		int					mFaces[3] = { -1, -1, -1 };	///< Indices of 3 neighboring faces with the biggest difference in normal (used to shift vertices for convex radius)
	};

	static_assert(sizeof(Point) == 32, "Unexpected size");
	static_assert(alignof(Point) == JPH_VECTOR_ALIGNMENT, "Unexpected alignment");

	Vec3					mCenterOfMass;				///< Center of mass of this convex hull
	Mat44					mInertia;					///< Inertia matrix assuming density is 1 (needs to be multiplied by density)
	AABox					mLocalBounds;				///< Local bounding box for the convex hull
	Array<Point>			mPoints;					///< Points on the convex hull surface
	Array<Face>				mFaces;						///< Faces of the convex hull surface
	Array<Plane>			mPlanes;					///< Planes for the faces (1-on-1 with mFaces array, separate because they need to be 16 byte aligned)
	Array<uint8>			mVertexIdx;					///< A list of vertex indices (indexing in mPoints) for each of the faces
	float					mConvexRadius = 0.0f;		///< Convex radius
	float					mVolume;					///< Total volume of the convex hull
	float					mInnerRadius = FLT_MAX;		///< Radius of the biggest sphere that fits entirely in the convex hull

#ifdef JPH_DEBUG_RENDERER
	mutable DebugRenderer::GeometryRef mGeometry;
#endif // JPH_DEBUG_RENDERER
};

JPH_NAMESPACE_END

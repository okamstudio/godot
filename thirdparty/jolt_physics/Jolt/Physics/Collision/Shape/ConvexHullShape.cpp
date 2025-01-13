// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#include <Jolt/Jolt.h>

#include <Jolt/Physics/Collision/Shape/ConvexHullShape.h>
#include <Jolt/Physics/Collision/Shape/ScaleHelpers.h>
#include <Jolt/Physics/Collision/Shape/PolyhedronSubmergedVolumeCalculator.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/CollidePointResult.h>
#include <Jolt/Physics/Collision/TransformedShape.h>
#include <Jolt/Physics/Collision/CollideSoftBodyVertexIterator.h>
#include <Jolt/Geometry/ConvexHullBuilder.h>
#include <Jolt/Geometry/ClosestPoint.h>
#include <Jolt/ObjectStream/TypeDeclarations.h>
#include <Jolt/Core/StringTools.h>
#include <Jolt/Core/StreamIn.h>
#include <Jolt/Core/StreamOut.h>
#include <Jolt/Core/UnorderedMap.h>

JPH_NAMESPACE_BEGIN

JPH_IMPLEMENT_SERIALIZABLE_VIRTUAL(ConvexHullShapeSettings)
{
	JPH_ADD_BASE_CLASS(ConvexHullShapeSettings, ConvexShapeSettings)

	JPH_ADD_ATTRIBUTE(ConvexHullShapeSettings, mPoints)
	JPH_ADD_ATTRIBUTE(ConvexHullShapeSettings, mMaxConvexRadius)
	JPH_ADD_ATTRIBUTE(ConvexHullShapeSettings, mMaxErrorConvexRadius)
	JPH_ADD_ATTRIBUTE(ConvexHullShapeSettings, mHullTolerance)
}

ShapeSettings::ShapeResult ConvexHullShapeSettings::Create() const
{
	if (mCachedResult.IsEmpty())
		Ref<Shape> shape = new ConvexHullShape(*this, mCachedResult);
	return mCachedResult;
}

ConvexHullShape::ConvexHullShape(const ConvexHullShapeSettings &inSettings, ShapeResult &outResult) :
	ConvexShape(EShapeSubType::ConvexHull, inSettings, outResult),
	mConvexRadius(inSettings.mMaxConvexRadius)
{
	using BuilderFace = ConvexHullBuilder::Face;
	using Edge = ConvexHullBuilder::Edge;
	using Faces = Array<BuilderFace *>;

	// Check convex radius
	if (mConvexRadius < 0.0f)
	{
		outResult.SetError("Invalid convex radius");
		return;
	}

	// Build convex hull
	const char *error = nullptr;
	ConvexHullBuilder builder(inSettings.mPoints);
	ConvexHullBuilder::EResult result = builder.Initialize(cMaxPointsInHull, inSettings.mHullTolerance, error);
	if (result != ConvexHullBuilder::EResult::Success && result != ConvexHullBuilder::EResult::MaxVerticesReached)
	{
		outResult.SetError(error);
		return;
	}
	const Faces &builder_faces = builder.GetFaces();

	// Check the consistency of the resulting hull if we fully built it
	if (result == ConvexHullBuilder::EResult::Success)
	{
		ConvexHullBuilder::Face *max_error_face;
		float max_error_distance, coplanar_distance;
		int max_error_idx;
		builder.DetermineMaxError(max_error_face, max_error_distance, max_error_idx, coplanar_distance);
		if (max_error_distance > 4.0f * max(coplanar_distance, inSettings.mHullTolerance)) // Coplanar distance could be bigger than the allowed tolerance if the points are far apart
		{
			outResult.SetError(StringFormat("Hull building failed, point %d had an error of %g (relative to tolerance: %g)", max_error_idx, (double)max_error_distance, double(max_error_distance / inSettings.mHullTolerance)));
			return;
		}
	}

	// Calculate center of mass and volume
	builder.GetCenterOfMassAndVolume(mCenterOfMass, mVolume);

	// Calculate covariance matrix
	// See:
	// - Why the inertia tensor is the inertia tensor - Jonathan Blow (http://number-none.com/blow/inertia/deriving_i.html)
	// - How to find the inertia tensor (or other mass properties) of a 3D solid body represented by a triangle mesh (Draft) - Jonathan Blow, Atman J Binstock (http://number-none.com/blow/inertia/bb_inertia.doc)
	Mat44 covariance_canonical(Vec4(1.0f / 60.0f, 1.0f / 120.0f, 1.0f / 120.0f, 0), Vec4(1.0f / 120.0f, 1.0f / 60.0f, 1.0f / 120.0f, 0), Vec4(1.0f / 120.0f, 1.0f / 120.0f, 1.0f / 60.0f, 0), Vec4(0, 0, 0, 1));
	Mat44 covariance_matrix = Mat44::sZero();
	for (BuilderFace *f : builder_faces)
	{
		// Fourth point of the tetrahedron is at the center of mass, we subtract it from the other points so we get a tetrahedron with one vertex at zero
		// The first point on the face will be used to form a triangle fan
		Edge *e = f->mFirstEdge;
		Vec3 v1 = inSettings.mPoints[e->mStartIdx] - mCenterOfMass;

		// Get the 2nd point
		e = e->mNextEdge;
		Vec3 v2 = inSettings.mPoints[e->mStartIdx] - mCenterOfMass;

		// Loop over the triangle fan
		for (e = e->mNextEdge; e != f->mFirstEdge; e = e->mNextEdge)
		{
			Vec3 v3 = inSettings.mPoints[e->mStartIdx] - mCenterOfMass;

			// Affine transform that transforms a unit tetrahedron (with vertices (0, 0, 0), (1, 0, 0), (0, 1, 0) and (0, 0, 1) to this tetrahedron
			Mat44 a(Vec4(v1, 0), Vec4(v2, 0), Vec4(v3, 0), Vec4(0, 0, 0, 1));

			// Calculate covariance matrix for this tetrahedron
			float det_a = a.GetDeterminant3x3();
			Mat44 c = det_a * (a * covariance_canonical * a.Transposed());

			// Add it
			covariance_matrix += c;

			// Prepare for next triangle
			v2 = v3;
		}
	}

	// Calculate inertia matrix assuming density is 1, note that element (3, 3) is garbage
	mInertia = Mat44::sIdentity() * (covariance_matrix(0, 0) + covariance_matrix(1, 1) + covariance_matrix(2, 2)) - covariance_matrix;

	// Convert polygons from the builder to our internal representation
	using VtxMap = UnorderedMap<int, uint8>;
	VtxMap vertex_map;
	vertex_map.reserve(VtxMap::size_type(inSettings.mPoints.size()));
	for (BuilderFace *builder_face : builder_faces)
	{
		// Determine where the vertices go
		JPH_ASSERT(mVertexIdx.size() <= 0xFFFF);
		uint16 first_vertex = (uint16)mVertexIdx.size();
		uint16 num_vertices = 0;

		// Loop over vertices in face
		Edge *edge = builder_face->mFirstEdge;
		do
		{
			// Remap to new index, not all points in the original input set are required to form the hull
			uint8 new_idx;
			int original_idx = edge->mStartIdx;
			VtxMap::iterator m = vertex_map.find(original_idx);
			if (m != vertex_map.end())
			{
				// Found, reuse
				new_idx = m->second;
			}
			else
			{
				// This is a new point
				// Make relative to center of mass
				Vec3 p = inSettings.mPoints[original_idx] - mCenterOfMass;

				// Update local bounds
				mLocalBounds.Encapsulate(p);

				// Add to point list
				JPH_ASSERT(mPoints.size() <= 0xff);
				new_idx = (uint8)mPoints.size();
				mPoints.push_back({ p });
				vertex_map[original_idx] = new_idx;
			}

			// Append to vertex list
			JPH_ASSERT(mVertexIdx.size() < 0xffff);
			mVertexIdx.push_back(new_idx);
			num_vertices++;

			edge = edge->mNextEdge;
		} while (edge != builder_face->mFirstEdge);

		// Add face
		mFaces.push_back({ first_vertex, num_vertices });

		// Add plane
		Plane plane = Plane::sFromPointAndNormal(builder_face->mCentroid - mCenterOfMass, builder_face->mNormal.Normalized());
		mPlanes.push_back(plane);
	}

	// Test if GetSupportFunction can support this many points
	if (mPoints.size() > cMaxPointsInHull)
	{
		outResult.SetError(StringFormat("Internal error: Too many points in hull (%u), max allowed %d", (uint)mPoints.size(), cMaxPointsInHull));
		return;
	}

	for (int p = 0; p < (int)mPoints.size(); ++p)
	{
		// For each point, find faces that use the point
		Array<int> faces;
		for (int f = 0; f < (int)mFaces.size(); ++f)
		{
			const Face &face = mFaces[f];
			for (int v = 0; v < face.mNumVertices; ++v)
				if (mVertexIdx[face.mFirstVertex + v] == p)
				{
					faces.push_back(f);
					break;
				}
		}

		if (faces.size() < 2)
		{
			outResult.SetError("A point must be connected to 2 or more faces!");
			return;
		}

		// Find the 3 normals that form the largest tetrahedron
		// The largest tetrahedron we can get is ((1, 0, 0) x (0, 1, 0)) . (0, 0, 1) = 1, if the volume is only 5% of that,
		// the three vectors are too coplanar and we fall back to using only 2 plane normals
		float biggest_volume = 0.05f;
		int best3[3] = { -1, -1, -1 };

		// When using 2 normals, we get the two with the biggest angle between them with a minimal difference of 1 degree
		// otherwise we fall back to just using 1 plane normal
		float smallest_dot = Cos(DegreesToRadians(1.0f));
		int best2[2] = { -1, -1 };

		for (int face1 = 0; face1 < (int)faces.size(); ++face1)
		{
			Vec3 normal1 = mPlanes[faces[face1]].GetNormal();
			for (int face2 = face1 + 1; face2 < (int)faces.size(); ++face2)
			{
				Vec3 normal2 = mPlanes[faces[face2]].GetNormal();
				Vec3 cross = normal1.Cross(normal2);

				// Determine the 2 face normals that are most apart
				float dot = normal1.Dot(normal2);
				if (dot < smallest_dot)
				{
					smallest_dot = dot;
					best2[0] = faces[face1];
					best2[1] = faces[face2];
				}

				// Determine the 3 face normals that form the largest tetrahedron
				for (int face3 = face2 + 1; face3 < (int)faces.size(); ++face3)
				{
					Vec3 normal3 = mPlanes[faces[face3]].GetNormal();
					float volume = abs(cross.Dot(normal3));
					if (volume > biggest_volume)
					{
						biggest_volume = volume;
						best3[0] = faces[face1];
						best3[1] = faces[face2];
						best3[2] = faces[face3];
					}
				}
			}
		}

		// If we didn't find 3 planes, use 2, if we didn't find 2 use 1
		if (best3[0] != -1)
			faces = { best3[0], best3[1], best3[2] };
		else if (best2[0] != -1)
			faces = { best2[0], best2[1] };
		else
			faces = { faces[0] };

		// Copy the faces to the points buffer
		Point &point = mPoints[p];
		point.mNumFaces = (int)faces.size();
		for (int i = 0; i < (int)faces.size(); ++i)
			point.mFaces[i] = faces[i];
	}

	// If the convex radius is already zero, there's no point in further reducing it
	if (mConvexRadius > 0.0f)
	{
		// Find out how thin the hull is by walking over all planes and checking the thickness of the hull in that direction
		float min_size = FLT_MAX;
		for (const Plane &plane : mPlanes)
		{
			// Take the point that is furthest away from the plane as thickness of this hull
			float max_dist = 0.0f;
			for (const Point &point : mPoints)
			{
				float dist = -plane.SignedDistance(point.mPosition); // Point is always behind plane, so we need to negate
				if (dist > max_dist)
					max_dist = dist;
			}
			min_size = min(min_size, max_dist);
		}

		// We need to fit in 2x the convex radius in min_size, so reduce the convex radius if it's bigger than that
		mConvexRadius = min(mConvexRadius, 0.5f * min_size);
	}

	// Now walk over all points and see if we have to further reduce the convex radius because of sharp edges
	if (mConvexRadius > 0.0f)
	{
		for (const Point &point : mPoints)
			if (point.mNumFaces != 1) // If we have a single face, shifting back is easy and we don't need to reduce the convex radius
			{
				// Get first two planes
				Plane p1 = mPlanes[point.mFaces[0]];
				Plane p2 = mPlanes[point.mFaces[1]];
				Plane p3;
				Vec3 offset_mask;

				if (point.mNumFaces == 3)
				{
					// Get third plane
					p3 = mPlanes[point.mFaces[2]];

					// All 3 planes will be offset by the convex radius
					offset_mask = Vec3::sReplicate(1);
				}
				else
				{
					// Third plane has normal perpendicular to the other two planes and goes through the vertex position
					JPH_ASSERT(point.mNumFaces == 2);
					p3 = Plane::sFromPointAndNormal(point.mPosition, p1.GetNormal().Cross(p2.GetNormal()));

					// Only the first and 2nd plane will be offset, the 3rd plane is only there to guide the intersection point
					offset_mask = Vec3(1, 1, 0);
				}

				// Plane equation: point . normal + constant = 0
				// Offsetting the plane backwards with convex radius r: point . normal + constant + r = 0
				// To find the intersection 'point' of 3 planes we solve:
				// |n1x n1y n1z| |x|     | r + c1 |
				// |n2x n2y n2z| |y| = - | r + c2 | <=> n point = -r (1, 1, 1) - (c1, c2, c3)
				// |n3x n3y n3z| |z|     | r + c3 |
				// Where point = (x, y, z), n1x is the x component of the first plane, c1 = plane constant of plane 1, etc.
				// The relation between how much the intersection point shifts as a function of r is: -r * n^-1 (1, 1, 1) = r * offset
				// Where offset = -n^-1 (1, 1, 1) or -n^-1 (1, 1, 0) in case only the first 2 planes are offset
				// The error that is introduced by a convex radius r is: error = r * |offset| - r
				// So the max convex radius given error is: r = error / (|offset| - 1)
				Mat44 n = Mat44(Vec4(p1.GetNormal(), 0), Vec4(p2.GetNormal(), 0), Vec4(p3.GetNormal(), 0), Vec4(0, 0, 0, 1)).Transposed();
				float det_n = n.GetDeterminant3x3();
				if (det_n == 0.0f)
				{
					// If the determinant is zero, the matrix is not invertible so no solution exists to move the point backwards and we have to choose a convex radius of zero
					mConvexRadius = 0.0f;
					break;
				}
				Mat44 adj_n = n.Adjointed3x3();
				float offset = ((adj_n * offset_mask) / det_n).Length();
				JPH_ASSERT(offset > 1.0f);
				float max_convex_radius = inSettings.mMaxErrorConvexRadius / (offset - 1.0f);
				mConvexRadius = min(mConvexRadius, max_convex_radius);
			}
		}

	// Calculate the inner radius by getting the minimum distance from the origin to the planes of the hull
	mInnerRadius = FLT_MAX;
	for (const Plane &p : mPlanes)
		mInnerRadius = min(mInnerRadius, -p.GetConstant());
	mInnerRadius = max(0.0f, mInnerRadius); // Clamp against zero, this should do nothing as the shape is centered around the center of mass but for flat convex hulls there may be numerical round off issues

	outResult.Set(this);
}

MassProperties ConvexHullShape::GetMassProperties() const
{
	MassProperties p;

	float density = GetDensity();

	// Calculate mass
	p.mMass = density * mVolume;

	// Calculate inertia matrix
	p.mInertia = density * mInertia;
	p.mInertia(3, 3) = 1.0f;

	return p;
}

Vec3 ConvexHullShape::GetSurfaceNormal(const SubShapeID &inSubShapeID, Vec3Arg inLocalSurfacePosition) const
{
	JPH_ASSERT(inSubShapeID.IsEmpty(), "Invalid subshape ID");

	const Plane &first_plane = mPlanes[0];
	Vec3 best_normal = first_plane.GetNormal();
	float best_dist = abs(first_plane.SignedDistance(inLocalSurfacePosition));

	// Find the face that has the shortest distance to the surface point
	for (Array<Face>::size_type i = 1; i < mFaces.size(); ++i)
	{
		const Plane &plane = mPlanes[i];
		Vec3 plane_normal = plane.GetNormal();
		float dist = abs(plane.SignedDistance(inLocalSurfacePosition));
		if (dist < best_dist)
		{
			best_dist = dist;
			best_normal = plane_normal;
		}
	}

	return best_normal;
}

class ConvexHullShape::HullNoConvex final : public Support
{
public:
	explicit				HullNoConvex(float inConvexRadius) :
		mConvexRadius(inConvexRadius)
	{
		static_assert(sizeof(HullNoConvex) <= sizeof(SupportBuffer), "Buffer size too small");
		JPH_ASSERT(IsAligned(this, alignof(HullNoConvex)));
	}

	virtual Vec3			GetSupport(Vec3Arg inDirection) const override
	{
		// Find the point with the highest projection on inDirection
		float best_dot = -FLT_MAX;
		Vec3 best_point = Vec3::sZero();

		for (Vec3 point : mPoints)
		{
			// Check if its support is bigger than the current max
			float dot = point.Dot(inDirection);
			if (dot > best_dot)
			{
				best_dot = dot;
				best_point = point;
			}
		}

		return best_point;
	}

	virtual float			GetConvexRadius() const override
	{
		return mConvexRadius;
	}

	using PointsArray = StaticArray<Vec3, cMaxPointsInHull>;

	inline PointsArray &	GetPoints()
	{
		return mPoints;
	}

	const PointsArray &		GetPoints() const
	{
		return mPoints;
	}

private:
	float					mConvexRadius;
	PointsArray				mPoints;
};

class ConvexHullShape::HullWithConvex final : public Support
{
public:
	explicit				HullWithConvex(const ConvexHullShape *inShape) :
		mShape(inShape)
	{
		static_assert(sizeof(HullWithConvex) <= sizeof(SupportBuffer), "Buffer size too small");
		JPH_ASSERT(IsAligned(this, alignof(HullWithConvex)));
	}

	virtual Vec3			GetSupport(Vec3Arg inDirection) const override
	{
		// Find the point with the highest projection on inDirection
		float best_dot = -FLT_MAX;
		Vec3 best_point = Vec3::sZero();

		for (const Point &point : mShape->mPoints)
		{
			// Check if its support is bigger than the current max
			float dot = point.mPosition.Dot(inDirection);
			if (dot > best_dot)
			{
				best_dot = dot;
				best_point = point.mPosition;
			}
		}

		return best_point;
	}

	virtual float			GetConvexRadius() const override
	{
		return 0.0f;
	}

private:
	const ConvexHullShape *	mShape;
};

class ConvexHullShape::HullWithConvexScaled final : public Support
{
public:
							HullWithConvexScaled(const ConvexHullShape *inShape, Vec3Arg inScale) :
		mShape(inShape),
		mScale(inScale)
	{
		static_assert(sizeof(HullWithConvexScaled) <= sizeof(SupportBuffer), "Buffer size too small");
		JPH_ASSERT(IsAligned(this, alignof(HullWithConvexScaled)));
	}

	virtual Vec3			GetSupport(Vec3Arg inDirection) const override
	{
		// Find the point with the highest projection on inDirection
		float best_dot = -FLT_MAX;
		Vec3 best_point = Vec3::sZero();

		for (const Point &point : mShape->mPoints)
		{
			// Calculate scaled position
			Vec3 pos = mScale * point.mPosition;

			// Check if its support is bigger than the current max
			float dot = pos.Dot(inDirection);
			if (dot > best_dot)
			{
				best_dot = dot;
				best_point = pos;
			}
		}

		return best_point;
	}

	virtual float			GetConvexRadius() const override
	{
		return 0.0f;
	}

private:
	const ConvexHullShape *	mShape;
	Vec3					mScale;
};

const ConvexShape::Support *ConvexHullShape::GetSupportFunction(ESupportMode inMode, SupportBuffer &inBuffer, Vec3Arg inScale) const
{
	// If there's no convex radius, we don't need to shrink the hull
	if (mConvexRadius == 0.0f)
	{
		if (ScaleHelpers::IsNotScaled(inScale))
			return new (&inBuffer) HullWithConvex(this);
		else
			return new (&inBuffer) HullWithConvexScaled(this, inScale);
	}

	switch (inMode)
	{
	case ESupportMode::IncludeConvexRadius:
	case ESupportMode::Default:
		if (ScaleHelpers::IsNotScaled(inScale))
			return new (&inBuffer) HullWithConvex(this);
		else
			return new (&inBuffer) HullWithConvexScaled(this, inScale);

	case ESupportMode::ExcludeConvexRadius:
		if (ScaleHelpers::IsNotScaled(inScale))
		{
			// Create support function
			HullNoConvex *hull = new (&inBuffer) HullNoConvex(mConvexRadius);
			HullNoConvex::PointsArray &transformed_points = hull->GetPoints();
			JPH_ASSERT(mPoints.size() <= cMaxPointsInHull, "Not enough space, this should have been caught during shape creation!");

			for (const Point &point : mPoints)
			{
				Vec3 new_point;

				if (point.mNumFaces == 1)
				{
					// Simply shift back by the convex radius using our 1 plane
					new_point = point.mPosition - mPlanes[point.mFaces[0]].GetNormal() * mConvexRadius;
				}
				else
				{
					// Get first two planes and offset inwards by convex radius
					Plane p1 = mPlanes[point.mFaces[0]].Offset(-mConvexRadius);
					Plane p2 = mPlanes[point.mFaces[1]].Offset(-mConvexRadius);
					Plane p3;

					if (point.mNumFaces == 3)
					{
						// Get third plane and offset inwards by convex radius
						p3 = mPlanes[point.mFaces[2]].Offset(-mConvexRadius);
					}
					else
					{
						// Third plane has normal perpendicular to the other two planes and goes through the vertex position
						JPH_ASSERT(point.mNumFaces == 2);
						p3 = Plane::sFromPointAndNormal(point.mPosition, p1.GetNormal().Cross(p2.GetNormal()));
					}

					// Find intersection point between the three planes
					if (!Plane::sIntersectPlanes(p1, p2, p3, new_point))
					{
						// Fallback: Just push point back using the first plane
						new_point = point.mPosition - p1.GetNormal() * mConvexRadius;
					}
				}

				// Add point
				transformed_points.push_back(new_point);
			}

			return hull;
		}
		else
		{
			// Calculate scaled convex radius
			float convex_radius = ScaleHelpers::ScaleConvexRadius(mConvexRadius, inScale);

			// Create new support function
			HullNoConvex *hull = new (&inBuffer) HullNoConvex(convex_radius);
			HullNoConvex::PointsArray &transformed_points = hull->GetPoints();
			JPH_ASSERT(mPoints.size() <= cMaxPointsInHull, "Not enough space, this should have been caught during shape creation!");

			// Precalculate inverse scale
			Vec3 inv_scale = inScale.Reciprocal();

			for (const Point &point : mPoints)
			{
				// Calculate scaled position
				Vec3 pos = inScale * point.mPosition;

				// Transform normals for plane 1 with scale
				Vec3 n1 = (inv_scale * mPlanes[point.mFaces[0]].GetNormal()).Normalized();

				Vec3 new_point;

				if (point.mNumFaces == 1)
				{
					// Simply shift back by the convex radius using our 1 plane
					new_point = pos - n1 * convex_radius;
				}
				else
				{
					// Transform normals for plane 2 with scale
					Vec3 n2 = (inv_scale * mPlanes[point.mFaces[1]].GetNormal()).Normalized();

					// Get first two planes and offset inwards by convex radius
					Plane p1 = Plane::sFromPointAndNormal(pos, n1).Offset(-convex_radius);
					Plane p2 = Plane::sFromPointAndNormal(pos, n2).Offset(-convex_radius);
					Plane p3;

					if (point.mNumFaces == 3)
					{
						// Transform last normal with scale
						Vec3 n3 = (inv_scale * mPlanes[point.mFaces[2]].GetNormal()).Normalized();

						// Get third plane and offset inwards by convex radius
						p3 = Plane::sFromPointAndNormal(pos, n3).Offset(-convex_radius);
					}
					else
					{
						// Third plane has normal perpendicular to the other two planes and goes through the vertex position
						JPH_ASSERT(point.mNumFaces == 2);
						p3 = Plane::sFromPointAndNormal(pos, n1.Cross(n2));
					}

					// Find intersection point between the three planes
					if (!Plane::sIntersectPlanes(p1, p2, p3, new_point))
					{
						// Fallback: Just push point back using the first plane
						new_point = pos - n1 * convex_radius;
					}
				}

				// Add point
				transformed_points.push_back(new_point);
			}

			return hull;
		}
	}

	JPH_ASSERT(false);
	return nullptr;
}

void ConvexHullShape::GetSupportingFace(const SubShapeID &inSubShapeID, Vec3Arg inDirection, Vec3Arg inScale, Mat44Arg inCenterOfMassTransform, SupportingFace &outVertices) const
{
	JPH_ASSERT(inSubShapeID.IsEmpty(), "Invalid subshape ID");

	Vec3 inv_scale = inScale.Reciprocal();

	// Need to transform the plane normals using inScale
	// Transforming a direction with matrix M is done through multiplying by (M^-1)^T
	// In this case M is a diagonal matrix with the scale vector, so we need to multiply our normal by 1 / scale and renormalize afterwards
	Vec3 plane0_normal = inv_scale * mPlanes[0].GetNormal();
	float best_dot = plane0_normal.Dot(inDirection) / plane0_normal.Length();
	int best_face_idx = 0;

	for (Array<Plane>::size_type i = 1; i < mPlanes.size(); ++i)
	{
		Vec3 plane_normal = inv_scale * mPlanes[i].GetNormal();
		float dot = plane_normal.Dot(inDirection) / plane_normal.Length();
		if (dot < best_dot)
		{
			best_dot = dot;
			best_face_idx = (int)i;
		}
	}

	// Get vertices
	const Face &best_face = mFaces[best_face_idx];
	const uint8 *first_vtx = mVertexIdx.data() + best_face.mFirstVertex;
	const uint8 *end_vtx = first_vtx + best_face.mNumVertices;

	// If we have more than 1/2 the capacity of outVertices worth of vertices, we start skipping vertices (note we can't fill the buffer completely since extra edges will be generated by clipping).
	// TODO: This really needs a better algorithm to determine which vertices are important!
	int max_vertices_to_return = outVertices.capacity() / 2;
	int delta_vtx = (int(best_face.mNumVertices) + max_vertices_to_return) / max_vertices_to_return;

	// Calculate transform with scale
	Mat44 transform = inCenterOfMassTransform.PreScaled(inScale);

	if (ScaleHelpers::IsInsideOut(inScale))
	{
		// Flip winding of supporting face
		for (const uint8 *v = end_vtx - 1; v >= first_vtx; v -= delta_vtx)
			outVertices.push_back(transform * mPoints[*v].mPosition);
	}
	else
	{
		// Normal winding of supporting face
		for (const uint8 *v = first_vtx; v < end_vtx; v += delta_vtx)
			outVertices.push_back(transform * mPoints[*v].mPosition);
	}
}

void ConvexHullShape::GetSubmergedVolume(Mat44Arg inCenterOfMassTransform, Vec3Arg inScale, const Plane &inSurface, float &outTotalVolume, float &outSubmergedVolume, Vec3 &outCenterOfBuoyancy JPH_IF_DEBUG_RENDERER(, RVec3Arg inBaseOffset)) const
{
	// Trivially calculate total volume
	Vec3 abs_scale = inScale.Abs();
	outTotalVolume = mVolume * abs_scale.GetX() * abs_scale.GetY() * abs_scale.GetZ();

	// Check if shape has been scaled inside out
	bool is_inside_out = ScaleHelpers::IsInsideOut(inScale);

	// Convert the points to world space and determine the distance to the surface
	int num_points = int(mPoints.size());
	PolyhedronSubmergedVolumeCalculator::Point *buffer = (PolyhedronSubmergedVolumeCalculator::Point *)JPH_STACK_ALLOC(num_points * sizeof(PolyhedronSubmergedVolumeCalculator::Point));
	PolyhedronSubmergedVolumeCalculator submerged_vol_calc(inCenterOfMassTransform * Mat44::sScale(inScale), &mPoints[0].mPosition, sizeof(Point), num_points, inSurface, buffer JPH_IF_DEBUG_RENDERER(, inBaseOffset));

	if (submerged_vol_calc.AreAllAbove())
	{
		// We're above the water
		outSubmergedVolume = 0.0f;
		outCenterOfBuoyancy = Vec3::sZero();
	}
	else if (submerged_vol_calc.AreAllBelow())
	{
		// We're fully submerged
		outSubmergedVolume = outTotalVolume;
		outCenterOfBuoyancy = inCenterOfMassTransform.GetTranslation();
	}
	else
	{
		// Calculate submerged volume
		int reference_point_idx = submerged_vol_calc.GetReferencePointIdx();
		for (const Face &f : mFaces)
		{
			const uint8 *first_vtx = mVertexIdx.data() + f.mFirstVertex;
			const uint8 *end_vtx = first_vtx + f.mNumVertices;

			// If any of the vertices of this face are the reference point, the volume will be zero so we can skip this face
			bool degenerate = false;
			for (const uint8 *v = first_vtx; v < end_vtx; ++v)
				if (*v == reference_point_idx)
				{
					degenerate = true;
					break;
				}
			if (degenerate)
				continue;

			// Triangulate the face
			int i1 = *first_vtx;
			if (is_inside_out)
			{
				// Reverse winding
				for (const uint8 *v = first_vtx + 2; v < end_vtx; ++v)
				{
					int i2 = *(v - 1);
					int i3 = *v;
					submerged_vol_calc.AddFace(i1, i3, i2);
				}
			}
			else
			{
				// Normal winding
				for (const uint8 *v = first_vtx + 2; v < end_vtx; ++v)
				{
					int i2 = *(v - 1);
					int i3 = *v;
					submerged_vol_calc.AddFace(i1, i2, i3);
				}
			}
		}

		// Get the results
		submerged_vol_calc.GetResult(outSubmergedVolume, outCenterOfBuoyancy);
	}

#ifdef JPH_DEBUG_RENDERER
	// Draw center of buoyancy
	if (sDrawSubmergedVolumes)
		DebugRenderer::sInstance->DrawWireSphere(inBaseOffset + outCenterOfBuoyancy, 0.05f, Color::sRed, 1);
#endif // JPH_DEBUG_RENDERER
}

#ifdef JPH_DEBUG_RENDERER
void ConvexHullShape::Draw(DebugRenderer *inRenderer, RMat44Arg inCenterOfMassTransform, Vec3Arg inScale, ColorArg inColor, bool inUseMaterialColors, bool inDrawWireframe) const
{
	if (mGeometry == nullptr)
	{
		Array<DebugRenderer::Triangle> triangles;
		for (const Face &f : mFaces)
		{
			const uint8 *first_vtx = mVertexIdx.data() + f.mFirstVertex;
			const uint8 *end_vtx = first_vtx + f.mNumVertices;

			// Draw first triangle of polygon
			Vec3 v0 = mPoints[first_vtx[0]].mPosition;
			Vec3 v1 = mPoints[first_vtx[1]].mPosition;
			Vec3 v2 = mPoints[first_vtx[2]].mPosition;
			Vec3 uv_direction = (v1 - v0).Normalized();
			triangles.push_back({ v0, v1, v2, Color::sWhite, v0, uv_direction });

			// Draw any other triangles in this polygon
			for (const uint8 *v = first_vtx + 3; v < end_vtx; ++v)
				triangles.push_back({ v0, mPoints[*(v - 1)].mPosition, mPoints[*v].mPosition, Color::sWhite, v0, uv_direction });
		}
		mGeometry = new DebugRenderer::Geometry(inRenderer->CreateTriangleBatch(triangles), GetLocalBounds());
	}

	// Test if the shape is scaled inside out
	DebugRenderer::ECullMode cull_mode = ScaleHelpers::IsInsideOut(inScale)? DebugRenderer::ECullMode::CullFrontFace : DebugRenderer::ECullMode::CullBackFace;

	// Determine the draw mode
	DebugRenderer::EDrawMode draw_mode = inDrawWireframe? DebugRenderer::EDrawMode::Wireframe : DebugRenderer::EDrawMode::Solid;

	// Draw the geometry
	Color color = inUseMaterialColors? GetMaterial()->GetDebugColor() : inColor;
	RMat44 transform = inCenterOfMassTransform.PreScaled(inScale);
	inRenderer->DrawGeometry(transform, color, mGeometry, cull_mode, DebugRenderer::ECastShadow::On, draw_mode);

	// Draw the outline if requested
	if (sDrawFaceOutlines)
		for (const Face &f : mFaces)
		{
			const uint8 *first_vtx = mVertexIdx.data() + f.mFirstVertex;
			const uint8 *end_vtx = first_vtx + f.mNumVertices;

			// Draw edges of face
			inRenderer->DrawLine(transform * mPoints[*(end_vtx - 1)].mPosition, transform * mPoints[*first_vtx].mPosition, Color::sGrey);
			for (const uint8 *v = first_vtx + 1; v < end_vtx; ++v)
				inRenderer->DrawLine(transform * mPoints[*(v - 1)].mPosition, transform * mPoints[*v].mPosition, Color::sGrey);
		}
}

void ConvexHullShape::DrawShrunkShape(DebugRenderer *inRenderer, RMat44Arg inCenterOfMassTransform, Vec3Arg inScale) const
{
	// Get the shrunk points
	SupportBuffer buffer;
	const HullNoConvex *support = mConvexRadius > 0.0f? static_cast<const HullNoConvex *>(GetSupportFunction(ESupportMode::ExcludeConvexRadius, buffer, inScale)) : nullptr;

	RMat44 transform = inCenterOfMassTransform * Mat44::sScale(inScale);

	for (int p = 0; p < (int)mPoints.size(); ++p)
	{
		const Point &point = mPoints[p];
		RVec3 position = transform * point.mPosition;
		RVec3 shrunk_point = support != nullptr? transform * support->GetPoints()[p] : position;

		// Draw difference between shrunk position and position
		inRenderer->DrawLine(position, shrunk_point, Color::sGreen);

		// Draw face normals that are contributing
		for (int i = 0; i < point.mNumFaces; ++i)
			inRenderer->DrawLine(position, position + 0.1f * mPlanes[point.mFaces[i]].GetNormal(), Color::sYellow);

		// Draw point index
		inRenderer->DrawText3D(position, ConvertToString(p), Color::sWhite, 0.1f);
	}
}
#endif // JPH_DEBUG_RENDERER

bool ConvexHullShape::CastRayHelper(const RayCast &inRay, float &outMinFraction, float &outMaxFraction) const
{
	if (mFaces.size() == 2)
	{
		// If we have only 2 faces, we're a flat convex hull and we need to test edges instead of planes

		// Check if plane is parallel to ray
		const Plane &p = mPlanes.front();
		Vec3 plane_normal = p.GetNormal();
		float direction_projection = inRay.mDirection.Dot(plane_normal);
		if (abs(direction_projection) >= 1.0e-12f)
		{
			// Calculate intersection point
			float distance_to_plane = inRay.mOrigin.Dot(plane_normal) + p.GetConstant();
			float fraction = -distance_to_plane / direction_projection;
			if (fraction < 0.0f || fraction > 1.0f)
			{
				// Does not hit plane, no hit
				outMinFraction = 0.0f;
				outMaxFraction = 1.0f + FLT_EPSILON;
				return false;
			}
			Vec3 intersection_point = inRay.mOrigin + fraction * inRay.mDirection;

			// Test all edges to see if point is inside polygon
			const Face &f = mFaces.front();
			const uint8 *first_vtx = mVertexIdx.data() + f.mFirstVertex;
			const uint8 *end_vtx = first_vtx + f.mNumVertices;
			Vec3 p1 = mPoints[*end_vtx].mPosition;
			for (const uint8 *v = first_vtx; v < end_vtx; ++v)
			{
				Vec3 p2 = mPoints[*v].mPosition;
				if ((p2 - p1).Cross(intersection_point - p1).Dot(plane_normal) < 0.0f)
				{
					// Outside polygon, no hit
					outMinFraction = 0.0f;
					outMaxFraction = 1.0f + FLT_EPSILON;
					return false;
				}
				p1 = p2;
			}

			// Inside polygon, a hit
			outMinFraction = fraction;
			outMaxFraction = fraction;
			return true;
		}
		else
		{
			// Parallel ray doesn't hit
			outMinFraction = 0.0f;
			outMaxFraction = 1.0f + FLT_EPSILON;
			return false;
		}
	}
	else
	{
		// Clip ray against all planes
		int fractions_set = 0;
		bool all_inside = true;
		float min_fraction = 0.0f, max_fraction = 1.0f + FLT_EPSILON;
		for (const Plane &p : mPlanes)
		{
			// Check if the ray origin is behind this plane
			Vec3 plane_normal = p.GetNormal();
			float distance_to_plane = inRay.mOrigin.Dot(plane_normal) + p.GetConstant();
			bool is_outside = distance_to_plane > 0.0f;
			all_inside &= !is_outside;

			// Check if plane is parallel to ray
			float direction_projection = inRay.mDirection.Dot(plane_normal);
			if (abs(direction_projection) >= 1.0e-12f)
			{
				// Get intersection fraction between ray and plane
				float fraction = -distance_to_plane / direction_projection;

				// Update interval of ray that is inside the hull
				if (direction_projection < 0.0f)
				{
					min_fraction = max(fraction, min_fraction);
					fractions_set |= 1;
				}
				else
				{
					max_fraction = min(fraction, max_fraction);
					fractions_set |= 2;
				}
			}
			else if (is_outside)
				return false; // Outside the plane and parallel, no hit!
		}

		// Test if both min and max have been set
		if (fractions_set == 3)
		{
			// Output fractions
			outMinFraction = min_fraction;
			outMaxFraction = max_fraction;

			// Test if the infinite ray intersects with the hull (the length will be checked later)
			return min_fraction <= max_fraction && max_fraction >= 0.0f;
		}
		else
		{
			// Degenerate case, either the ray is parallel to all planes or the ray has zero length
			outMinFraction = 0.0f;
			outMaxFraction = 1.0f + FLT_EPSILON;

			// Return if the origin is inside the hull
			return all_inside;
		}
	}
}

bool ConvexHullShape::CastRay(const RayCast &inRay, const SubShapeIDCreator &inSubShapeIDCreator, RayCastResult &ioHit) const
{
	// Determine if ray hits the shape
	float min_fraction, max_fraction;
	if (CastRayHelper(inRay, min_fraction, max_fraction)
		&& min_fraction < ioHit.mFraction) // Check if this is a closer hit
	{
		// Better hit than the current hit
		ioHit.mFraction = min_fraction;
		ioHit.mSubShapeID2 = inSubShapeIDCreator.GetID();
		return true;
	}
	return false;
}

void ConvexHullShape::CastRay(const RayCast &inRay, const RayCastSettings &inRayCastSettings, const SubShapeIDCreator &inSubShapeIDCreator, CastRayCollector &ioCollector, const ShapeFilter &inShapeFilter) const
{
	// Test shape filter
	if (!inShapeFilter.ShouldCollide(this, inSubShapeIDCreator.GetID()))
		return;

	// Determine if ray hits the shape
	float min_fraction, max_fraction;
	if (CastRayHelper(inRay, min_fraction, max_fraction)
		&& min_fraction < ioCollector.GetEarlyOutFraction()) // Check if this is closer than the early out fraction
	{
		// Better hit than the current hit
		RayCastResult hit;
		hit.mBodyID = TransformedShape::sGetBodyID(ioCollector.GetContext());
		hit.mSubShapeID2 = inSubShapeIDCreator.GetID();

		// Check front side hit
		if (inRayCastSettings.mTreatConvexAsSolid || min_fraction > 0.0f)
		{
			hit.mFraction = min_fraction;
			ioCollector.AddHit(hit);
		}

		// Check back side hit
		if (inRayCastSettings.mBackFaceModeConvex == EBackFaceMode::CollideWithBackFaces
			&& max_fraction < ioCollector.GetEarlyOutFraction())
		{
			hit.mFraction = max_fraction;
			ioCollector.AddHit(hit);
		}
	}
}

void ConvexHullShape::CollidePoint(Vec3Arg inPoint, const SubShapeIDCreator &inSubShapeIDCreator, CollidePointCollector &ioCollector, const ShapeFilter &inShapeFilter) const
{
	// Test shape filter
	if (!inShapeFilter.ShouldCollide(this, inSubShapeIDCreator.GetID()))
		return;

	// Check if point is behind all planes
	for (const Plane &p : mPlanes)
		if (p.SignedDistance(inPoint) > 0.0f)
			return;

	// Point is inside
	ioCollector.AddHit({ TransformedShape::sGetBodyID(ioCollector.GetContext()), inSubShapeIDCreator.GetID() });
}

void ConvexHullShape::CollideSoftBodyVertices(Mat44Arg inCenterOfMassTransform, Vec3Arg inScale, const CollideSoftBodyVertexIterator &inVertices, uint inNumVertices, int inCollidingShapeIndex) const
{
	Mat44 inverse_transform = inCenterOfMassTransform.InversedRotationTranslation();

	Vec3 inv_scale = inScale.Reciprocal();
	bool is_not_scaled = ScaleHelpers::IsNotScaled(inScale);
	float scale_flip = ScaleHelpers::IsInsideOut(inScale)? -1.0f : 1.0f;

	for (CollideSoftBodyVertexIterator v = inVertices, sbv_end = inVertices + inNumVertices; v != sbv_end; ++v)
		if (v.GetInvMass() > 0.0f)
		{
			Vec3 local_pos = inverse_transform * v.GetPosition();

			// Find most facing plane
			float max_distance = -FLT_MAX;
			Vec3 max_plane_normal = Vec3::sZero();
			uint max_plane_idx = 0;
			if (is_not_scaled)
			{
				// Without scale, it is trivial to calculate the distance to the hull
				for (const Plane &p : mPlanes)
				{
					float distance = p.SignedDistance(local_pos);
					if (distance > max_distance)
					{
						max_distance = distance;
						max_plane_normal = p.GetNormal();
						max_plane_idx = uint(&p - mPlanes.data());
					}
				}
			}
			else
			{
				// When there's scale we need to calculate the planes first
				for (uint i = 0; i < (uint)mPlanes.size(); ++i)
				{
					// Calculate plane normal and point by scaling the original plane
					Vec3 plane_normal = (inv_scale * mPlanes[i].GetNormal()).Normalized();
					Vec3 plane_point = inScale * mPoints[mVertexIdx[mFaces[i].mFirstVertex]].mPosition;

					float distance = plane_normal.Dot(local_pos - plane_point);
					if (distance > max_distance)
					{
						max_distance = distance;
						max_plane_normal = plane_normal;
						max_plane_idx = i;
					}
				}
			}
			bool is_outside = max_distance > 0.0f;

			// Project point onto that plane
			Vec3 closest_point = local_pos - max_distance * max_plane_normal;

			// Check edges if we're outside the hull (when inside we know the closest face is also the closest point to the surface)
			if (is_outside)
			{
				// Loop over edges
				float closest_point_dist_sq = FLT_MAX;
				const Face &face = mFaces[max_plane_idx];
				for (const uint8 *v_start = &mVertexIdx[face.mFirstVertex], *v1 = v_start, *v_end = v_start + face.mNumVertices; v1 < v_end; ++v1)
				{
					// Find second point
					const uint8 *v2 = v1 + 1;
					if (v2 == v_end)
						v2 = v_start;

					// Get edge points
					Vec3 p1 = inScale * mPoints[*v1].mPosition;
					Vec3 p2 = inScale * mPoints[*v2].mPosition;

					// Check if the position is outside the edge (if not, the face will be closer)
					Vec3 edge_normal = (p2 - p1).Cross(max_plane_normal);
					if (scale_flip * edge_normal.Dot(local_pos - p1) > 0.0f)
					{
						// Get closest point on edge
						uint32 set;
						Vec3 closest = ClosestPoint::GetClosestPointOnLine(p1 - local_pos, p2 - local_pos, set);
						float distance_sq = closest.LengthSq();
						if (distance_sq < closest_point_dist_sq)
							closest_point = local_pos + closest;
					}
				}
			}

			// Check if this is the largest penetration
			Vec3 normal = local_pos - closest_point;
			float normal_length = normal.Length();
			float penetration = normal_length;
			if (is_outside)
				penetration = -penetration;
			else
				normal = -normal;
			if (v.UpdatePenetration(penetration))
			{
				// Calculate contact plane
				normal = normal_length > 0.0f? normal / normal_length : max_plane_normal;
				Plane plane = Plane::sFromPointAndNormal(closest_point, normal);

				// Store collision
				v.SetCollision(plane.GetTransformed(inCenterOfMassTransform), inCollidingShapeIndex);
			}
		}
}

class ConvexHullShape::CHSGetTrianglesContext
{
public:
				CHSGetTrianglesContext(Mat44Arg inTransform, bool inIsInsideOut) : mTransform(inTransform), mIsInsideOut(inIsInsideOut) { }

	Mat44		mTransform;
	bool		mIsInsideOut;
	size_t		mCurrentFace = 0;
};

void ConvexHullShape::GetTrianglesStart(GetTrianglesContext &ioContext, const AABox &inBox, Vec3Arg inPositionCOM, QuatArg inRotation, Vec3Arg inScale) const
{
	static_assert(sizeof(CHSGetTrianglesContext) <= sizeof(GetTrianglesContext), "GetTrianglesContext too small");
	JPH_ASSERT(IsAligned(&ioContext, alignof(CHSGetTrianglesContext)));

	new (&ioContext) CHSGetTrianglesContext(Mat44::sRotationTranslation(inRotation, inPositionCOM) * Mat44::sScale(inScale), ScaleHelpers::IsInsideOut(inScale));
}

int ConvexHullShape::GetTrianglesNext(GetTrianglesContext &ioContext, int inMaxTrianglesRequested, Float3 *outTriangleVertices, const PhysicsMaterial **outMaterials) const
{
	static_assert(cGetTrianglesMinTrianglesRequested >= 12, "cGetTrianglesMinTrianglesRequested is too small");
	JPH_ASSERT(inMaxTrianglesRequested >= cGetTrianglesMinTrianglesRequested);

	CHSGetTrianglesContext &context = (CHSGetTrianglesContext &)ioContext;

	int total_num_triangles = 0;
	for (; context.mCurrentFace < mFaces.size(); ++context.mCurrentFace)
	{
		const Face &f = mFaces[context.mCurrentFace];

		const uint8 *first_vtx = mVertexIdx.data() + f.mFirstVertex;
		const uint8 *end_vtx = first_vtx + f.mNumVertices;

		// Check if there is still room in the output buffer for this face
		int num_triangles = f.mNumVertices - 2;
		inMaxTrianglesRequested -= num_triangles;
		if (inMaxTrianglesRequested < 0)
			break;
		total_num_triangles += num_triangles;

		// Get first triangle of polygon
		Vec3 v0 = context.mTransform * mPoints[first_vtx[0]].mPosition;
		Vec3 v1 = context.mTransform * mPoints[first_vtx[1]].mPosition;
		Vec3 v2 = context.mTransform * mPoints[first_vtx[2]].mPosition;
		v0.StoreFloat3(outTriangleVertices++);
		if (context.mIsInsideOut)
		{
			// Store first triangle in this polygon flipped
			v2.StoreFloat3(outTriangleVertices++);
			v1.StoreFloat3(outTriangleVertices++);

			// Store other triangles in this polygon flipped
			for (const uint8 *v = first_vtx + 3; v < end_vtx; ++v)
			{
				v0.StoreFloat3(outTriangleVertices++);
				(context.mTransform * mPoints[*v].mPosition).StoreFloat3(outTriangleVertices++);
				(context.mTransform * mPoints[*(v - 1)].mPosition).StoreFloat3(outTriangleVertices++);
			}
		}
		else
		{
			// Store first triangle in this polygon
			v1.StoreFloat3(outTriangleVertices++);
			v2.StoreFloat3(outTriangleVertices++);

			// Store other triangles in this polygon
			for (const uint8 *v = first_vtx + 3; v < end_vtx; ++v)
			{
				v0.StoreFloat3(outTriangleVertices++);
				(context.mTransform * mPoints[*(v - 1)].mPosition).StoreFloat3(outTriangleVertices++);
				(context.mTransform * mPoints[*v].mPosition).StoreFloat3(outTriangleVertices++);
			}
		}
	}

	// Store materials
	if (outMaterials != nullptr)
	{
		const PhysicsMaterial *material = GetMaterial();
		for (const PhysicsMaterial **m = outMaterials, **m_end = outMaterials + total_num_triangles; m < m_end; ++m)
			*m = material;
	}

	return total_num_triangles;
}

void ConvexHullShape::SaveBinaryState(StreamOut &inStream) const
{
	ConvexShape::SaveBinaryState(inStream);

	inStream.Write(mCenterOfMass);
	inStream.Write(mInertia);
	inStream.Write(mLocalBounds.mMin);
	inStream.Write(mLocalBounds.mMax);
	inStream.Write(mPoints);
	inStream.Write(mFaces);
	inStream.Write(mPlanes);
	inStream.Write(mVertexIdx);
	inStream.Write(mConvexRadius);
	inStream.Write(mVolume);
	inStream.Write(mInnerRadius);
}

void ConvexHullShape::RestoreBinaryState(StreamIn &inStream)
{
	ConvexShape::RestoreBinaryState(inStream);

	inStream.Read(mCenterOfMass);
	inStream.Read(mInertia);
	inStream.Read(mLocalBounds.mMin);
	inStream.Read(mLocalBounds.mMax);
	inStream.Read(mPoints);
	inStream.Read(mFaces);
	inStream.Read(mPlanes);
	inStream.Read(mVertexIdx);
	inStream.Read(mConvexRadius);
	inStream.Read(mVolume);
	inStream.Read(mInnerRadius);
}

Shape::Stats ConvexHullShape::GetStats() const
{
	// Count number of triangles
	uint triangle_count = 0;
	for (const Face &f : mFaces)
		triangle_count += f.mNumVertices - 2;

	return Stats(
		sizeof(*this)
			+ mPoints.size() * sizeof(Point)
			+ mFaces.size() * sizeof(Face)
			+ mPlanes.size() * sizeof(Plane)
			+ mVertexIdx.size() * sizeof(uint8),
		triangle_count);
}

void ConvexHullShape::sRegister()
{
	ShapeFunctions &f = ShapeFunctions::sGet(EShapeSubType::ConvexHull);
	f.mConstruct = []() -> Shape * { return new ConvexHullShape; };
	f.mColor = Color::sGreen;
}

JPH_NAMESPACE_END

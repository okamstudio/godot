// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Jolt/Physics/Collision/Shape/Shape.h>

JPH_NAMESPACE_BEGIN

class PhysicsMaterial;

/// Implementation of GetTrianglesStart/Next that uses a fixed list of vertices for the triangles. These are transformed into world space when getting the triangles.
class GetTrianglesContextVertexList
{
public:
	/// Constructor, to be called in GetTrianglesStart
					GetTrianglesContextVertexList(Vec3Arg inPositionCOM, QuatArg inRotation, Vec3Arg inScale, Mat44Arg inLocalTransform, const Vec3 *inTriangleVertices, size_t inNumTriangleVertices, const PhysicsMaterial *inMaterial) :
		mLocalToWorld(Mat44::sRotationTranslation(inRotation, inPositionCOM) * Mat44::sScale(inScale) * inLocalTransform),
		mTriangleVertices(inTriangleVertices),
		mNumTriangleVertices(inNumTriangleVertices),
		mMaterial(inMaterial),
		mIsInsideOut(ScaleHelpers::IsInsideOut(inScale))
	{
		static_assert(sizeof(GetTrianglesContextVertexList) <= sizeof(Shape::GetTrianglesContext), "GetTrianglesContext too small");
		JPH_ASSERT(IsAligned(this, alignof(GetTrianglesContextVertexList)));
		JPH_ASSERT(inNumTriangleVertices % 3 == 0);
	}

	/// @see Shape::GetTrianglesNext
	int				GetTrianglesNext(int inMaxTrianglesRequested, Float3 *outTriangleVertices, const PhysicsMaterial **outMaterials)
	{
		JPH_ASSERT(inMaxTrianglesRequested >= Shape::cGetTrianglesMinTrianglesRequested);

		int total_num_vertices = min(inMaxTrianglesRequested * 3, int(mNumTriangleVertices - mCurrentVertex));

		if (mIsInsideOut)
		{
			// Store triangles flipped
			for (const Vec3 *v = mTriangleVertices + mCurrentVertex, *v_end = v + total_num_vertices; v < v_end; v += 3)
			{
				(mLocalToWorld * v[0]).StoreFloat3(outTriangleVertices++);
				(mLocalToWorld * v[2]).StoreFloat3(outTriangleVertices++);
				(mLocalToWorld * v[1]).StoreFloat3(outTriangleVertices++);
			}
		}
		else
		{
			// Store triangles
			for (const Vec3 *v = mTriangleVertices + mCurrentVertex, *v_end = v + total_num_vertices; v < v_end; v += 3)
			{
				(mLocalToWorld * v[0]).StoreFloat3(outTriangleVertices++);
				(mLocalToWorld * v[1]).StoreFloat3(outTriangleVertices++);
				(mLocalToWorld * v[2]).StoreFloat3(outTriangleVertices++);
			}
		}

		// Update the current vertex to point to the next vertex to get
		mCurrentVertex += total_num_vertices;
		int total_num_triangles = total_num_vertices / 3;

		// Store materials
		if (outMaterials != nullptr)
			for (const PhysicsMaterial **m = outMaterials, **m_end = outMaterials + total_num_triangles; m < m_end; ++m)
				*m = mMaterial;

		return total_num_triangles;
	}

	/// Helper function that creates a vertex list of a half unit sphere (top part)
	template <class A>
	static void		sCreateHalfUnitSphereTop(A &ioVertices, int inDetailLevel)
	{
		sCreateUnitSphereHelper(ioVertices,  Vec3::sAxisX(),  Vec3::sAxisY(),  Vec3::sAxisZ(), inDetailLevel);
		sCreateUnitSphereHelper(ioVertices,  Vec3::sAxisY(), -Vec3::sAxisX(),  Vec3::sAxisZ(), inDetailLevel);
		sCreateUnitSphereHelper(ioVertices,  Vec3::sAxisY(),  Vec3::sAxisX(), -Vec3::sAxisZ(), inDetailLevel);
		sCreateUnitSphereHelper(ioVertices, -Vec3::sAxisX(),  Vec3::sAxisY(), -Vec3::sAxisZ(), inDetailLevel);
	}

	/// Helper function that creates a vertex list of a half unit sphere (bottom part)
	template <class A>
	static void		sCreateHalfUnitSphereBottom(A &ioVertices, int inDetailLevel)
	{
		sCreateUnitSphereHelper(ioVertices, -Vec3::sAxisX(), -Vec3::sAxisY(),  Vec3::sAxisZ(), inDetailLevel);
		sCreateUnitSphereHelper(ioVertices, -Vec3::sAxisY(),  Vec3::sAxisX(),  Vec3::sAxisZ(), inDetailLevel);
		sCreateUnitSphereHelper(ioVertices,  Vec3::sAxisX(), -Vec3::sAxisY(), -Vec3::sAxisZ(), inDetailLevel);
		sCreateUnitSphereHelper(ioVertices, -Vec3::sAxisY(), -Vec3::sAxisX(), -Vec3::sAxisZ(), inDetailLevel);
	}

	/// Helper function that creates an open cylinder of half height 1 and radius 1
	template <class A>
	static void		sCreateUnitOpenCylinder(A &ioVertices, int inDetailLevel)
	{
		const Vec3 bottom_offset(0.0f, -2.0f, 0.0f);
		int num_verts = 4 * (1 << inDetailLevel);
		for (int i = 0; i < num_verts; ++i)
		{
			float angle1 = 2.0f * JPH_PI * (float(i) / num_verts);
			float angle2 = 2.0f * JPH_PI * (float(i + 1) / num_verts);

			Vec3 t1(Sin(angle1), 1.0f, Cos(angle1));
			Vec3 t2(Sin(angle2), 1.0f, Cos(angle2));
			Vec3 b1 = t1 + bottom_offset;
			Vec3 b2 = t2 + bottom_offset;

			ioVertices.push_back(t1);
			ioVertices.push_back(b1);
			ioVertices.push_back(t2);

			ioVertices.push_back(t2);
			ioVertices.push_back(b1);
			ioVertices.push_back(b2);
		}
	}

private:
	/// Recursive helper function for creating a sphere
	template <class A>
	static void		sCreateUnitSphereHelper(A &ioVertices, Vec3Arg inV1, Vec3Arg inV2, Vec3Arg inV3, int inLevel)
	{
		Vec3 center1 = (inV1 + inV2).Normalized();
		Vec3 center2 = (inV2 + inV3).Normalized();
		Vec3 center3 = (inV3 + inV1).Normalized();

		if (inLevel > 0)
		{
			int new_level = inLevel - 1;
			sCreateUnitSphereHelper(ioVertices, inV1, center1, center3, new_level);
			sCreateUnitSphereHelper(ioVertices, center1, center2, center3, new_level);
			sCreateUnitSphereHelper(ioVertices, center1, inV2, center2, new_level);
			sCreateUnitSphereHelper(ioVertices, center3, center2, inV3, new_level);
		}
		else
		{
			ioVertices.push_back(inV1);
			ioVertices.push_back(inV2);
			ioVertices.push_back(inV3);
		}
	}

	Mat44					mLocalToWorld;
	const Vec3 *			mTriangleVertices;
	size_t					mNumTriangleVertices;
	size_t					mCurrentVertex = 0;
	const PhysicsMaterial *	mMaterial;
	bool					mIsInsideOut;
};

/// Implementation of GetTrianglesStart/Next that uses a multiple fixed lists of vertices for the triangles. These are transformed into world space when getting the triangles.
class GetTrianglesContextMultiVertexList
{
public:
	/// Constructor, to be called in GetTrianglesStart
					GetTrianglesContextMultiVertexList(bool inIsInsideOut, const PhysicsMaterial *inMaterial) :
		mMaterial(inMaterial),
		mIsInsideOut(inIsInsideOut)
	{
		static_assert(sizeof(GetTrianglesContextMultiVertexList) <= sizeof(Shape::GetTrianglesContext), "GetTrianglesContext too small");
		JPH_ASSERT(IsAligned(this, alignof(GetTrianglesContextMultiVertexList)));
	}

	/// Add a mesh part and its transform
	void			AddPart(Mat44Arg inLocalToWorld, const Vec3 *inTriangleVertices, size_t inNumTriangleVertices)
	{
		JPH_ASSERT(inNumTriangleVertices % 3 == 0);

		mParts.push_back({ inLocalToWorld, inTriangleVertices, inNumTriangleVertices });
	}

	/// @see Shape::GetTrianglesNext
	int				GetTrianglesNext(int inMaxTrianglesRequested, Float3 *outTriangleVertices, const PhysicsMaterial **outMaterials)
	{
		JPH_ASSERT(inMaxTrianglesRequested >= Shape::cGetTrianglesMinTrianglesRequested);

		int total_num_vertices = 0;
		int max_vertices_requested = inMaxTrianglesRequested * 3;

		// Loop over parts
		for (; mCurrentPart < mParts.size(); ++mCurrentPart)
		{
			const Part &part = mParts[mCurrentPart];

			// Calculate how many vertices to take from this part
			int part_num_vertices = min(max_vertices_requested, int(part.mNumTriangleVertices - mCurrentVertex));
			if (part_num_vertices == 0)
				break;

			max_vertices_requested -= part_num_vertices;
			total_num_vertices += part_num_vertices;

			if (mIsInsideOut)
			{
				// Store triangles flipped
				for (const Vec3 *v = part.mTriangleVertices + mCurrentVertex, *v_end = v + part_num_vertices; v < v_end; v += 3)
				{
					(part.mLocalToWorld * v[0]).StoreFloat3(outTriangleVertices++);
					(part.mLocalToWorld * v[2]).StoreFloat3(outTriangleVertices++);
					(part.mLocalToWorld * v[1]).StoreFloat3(outTriangleVertices++);
				}
			}
			else
			{
				// Store triangles
				for (const Vec3 *v = part.mTriangleVertices + mCurrentVertex, *v_end = v + part_num_vertices; v < v_end; v += 3)
				{
					(part.mLocalToWorld * v[0]).StoreFloat3(outTriangleVertices++);
					(part.mLocalToWorld * v[1]).StoreFloat3(outTriangleVertices++);
					(part.mLocalToWorld * v[2]).StoreFloat3(outTriangleVertices++);
				}
			}

			// Update the current vertex to point to the next vertex to get
			mCurrentVertex += part_num_vertices;

			// Check if we completed this part
			if (mCurrentVertex < part.mNumTriangleVertices)
				break;

			// Reset current vertex for the next part
			mCurrentVertex = 0;
		}

		int total_num_triangles = total_num_vertices / 3;

		// Store materials
		if (outMaterials != nullptr)
			for (const PhysicsMaterial **m = outMaterials, **m_end = outMaterials + total_num_triangles; m < m_end; ++m)
				*m = mMaterial;

		return total_num_triangles;
	}

private:
	struct Part
	{
		Mat44				mLocalToWorld;
		const Vec3 *		mTriangleVertices;
		size_t				mNumTriangleVertices;
	};

	StaticArray<Part, 3>	mParts;
	uint					mCurrentPart = 0;
	size_t					mCurrentVertex = 0;
	const PhysicsMaterial *	mMaterial;
	bool					mIsInsideOut;
};

JPH_NAMESPACE_END

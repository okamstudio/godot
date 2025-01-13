// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Jolt/Core/HashCombine.h>

JPH_NAMESPACE_BEGIN

/// Triangle with 32-bit indices
class IndexedTriangleNoMaterial
{
public:
	JPH_OVERRIDE_NEW_DELETE

	/// Constructor
					IndexedTriangleNoMaterial() = default;
	constexpr		IndexedTriangleNoMaterial(uint32 inI1, uint32 inI2, uint32 inI3) : mIdx { inI1, inI2, inI3 } { }

	/// Check if two triangles are identical
	bool			operator == (const IndexedTriangleNoMaterial &inRHS) const
	{
		return mIdx[0] == inRHS.mIdx[0] && mIdx[1] == inRHS.mIdx[1] && mIdx[2] == inRHS.mIdx[2];
	}

	/// Check if two triangles are equivalent (using the same vertices)
	bool			IsEquivalent(const IndexedTriangleNoMaterial &inRHS) const
	{
		return (mIdx[0] == inRHS.mIdx[0] && mIdx[1] == inRHS.mIdx[1] && mIdx[2] == inRHS.mIdx[2])
			|| (mIdx[0] == inRHS.mIdx[1] && mIdx[1] == inRHS.mIdx[2] && mIdx[2] == inRHS.mIdx[0])
			|| (mIdx[0] == inRHS.mIdx[2] && mIdx[1] == inRHS.mIdx[0] && mIdx[2] == inRHS.mIdx[1]);
	}

	/// Check if two triangles are opposite (using the same vertices but in opposing order)
	bool			IsOpposite(const IndexedTriangleNoMaterial &inRHS) const
	{
		return (mIdx[0] == inRHS.mIdx[0] && mIdx[1] == inRHS.mIdx[2] && mIdx[2] == inRHS.mIdx[1])
			|| (mIdx[0] == inRHS.mIdx[1] && mIdx[1] == inRHS.mIdx[0] && mIdx[2] == inRHS.mIdx[2])
			|| (mIdx[0] == inRHS.mIdx[2] && mIdx[1] == inRHS.mIdx[1] && mIdx[2] == inRHS.mIdx[0]);
	}

	/// Check if triangle is degenerate
	bool			IsDegenerate(const VertexList &inVertices) const
	{
		Vec3 v0(inVertices[mIdx[0]]);
		Vec3 v1(inVertices[mIdx[1]]);
		Vec3 v2(inVertices[mIdx[2]]);

		return (v1 - v0).Cross(v2 - v0).IsNearZero();
	}

	/// Rotate the vertices so that the second vertex becomes first etc. This does not change the represented triangle.
	void			Rotate()
	{
		uint32 tmp = mIdx[0];
		mIdx[0] = mIdx[1];
		mIdx[1] = mIdx[2];
		mIdx[2] = tmp;
	}

	/// Get center of triangle
	Vec3			GetCentroid(const VertexList &inVertices) const
	{
		return (Vec3(inVertices[mIdx[0]]) + Vec3(inVertices[mIdx[1]]) + Vec3(inVertices[mIdx[2]])) / 3.0f;
	}

	/// Get the hash value of this structure
	uint64			GetHash() const
	{
		static_assert(sizeof(IndexedTriangleNoMaterial) == 3 * sizeof(uint32), "Class should have no padding");
		return HashBytes(this, sizeof(IndexedTriangleNoMaterial));
	}

	uint32			mIdx[3];
};

/// Triangle with 32-bit indices and material index
class IndexedTriangle : public IndexedTriangleNoMaterial
{
public:
	using IndexedTriangleNoMaterial::IndexedTriangleNoMaterial;

	/// Constructor
	constexpr		IndexedTriangle(uint32 inI1, uint32 inI2, uint32 inI3, uint32 inMaterialIndex, uint inUserData = 0) : IndexedTriangleNoMaterial(inI1, inI2, inI3), mMaterialIndex(inMaterialIndex), mUserData(inUserData) { }

	/// Check if two triangles are identical
	bool			operator == (const IndexedTriangle &inRHS) const
	{
		return mMaterialIndex == inRHS.mMaterialIndex && mUserData == inRHS.mUserData && IndexedTriangleNoMaterial::operator==(inRHS);
	}

	/// Rotate the vertices so that the lowest vertex becomes the first. This does not change the represented triangle.
	IndexedTriangle	GetLowestIndexFirst() const
	{
		if (mIdx[0] < mIdx[1])
		{
			if (mIdx[0] < mIdx[2])
				return IndexedTriangle(mIdx[0], mIdx[1], mIdx[2], mMaterialIndex, mUserData); // 0 is smallest
			else
				return IndexedTriangle(mIdx[2], mIdx[0], mIdx[1], mMaterialIndex, mUserData); // 2 is smallest
		}
		else
		{
			if (mIdx[1] < mIdx[2])
				return IndexedTriangle(mIdx[1], mIdx[2], mIdx[0], mMaterialIndex, mUserData); // 1 is smallest
			else
				return IndexedTriangle(mIdx[2], mIdx[0], mIdx[1], mMaterialIndex, mUserData); // 2 is smallest
		}
	}

	/// Get the hash value of this structure
	uint64			GetHash() const
	{
		static_assert(sizeof(IndexedTriangle) == 5 * sizeof(uint32), "Class should have no padding");
		return HashBytes(this, sizeof(IndexedTriangle));
	}

	uint32			mMaterialIndex = 0;
	uint32			mUserData = 0;				///< User data that can be used for anything by the application, e.g. for tracking the original index of the triangle
};

using IndexedTriangleNoMaterialList = Array<IndexedTriangleNoMaterial>;
using IndexedTriangleList = Array<IndexedTriangle>;

JPH_NAMESPACE_END

// Create a std::hash for IndexedTriangleNoMaterial and IndexedTriangle
JPH_MAKE_STD_HASH(JPH::IndexedTriangleNoMaterial)
JPH_MAKE_STD_HASH(JPH::IndexedTriangle)

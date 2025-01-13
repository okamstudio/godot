// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Jolt/Geometry/Plane.h>
#ifdef JPH_DEBUG_RENDERER
	#include <Jolt/Renderer/DebugRenderer.h>
#endif // JPH_DEBUG_RENDERER

JPH_NAMESPACE_BEGIN

/// This class calculates the intersection between a fluid surface and a polyhedron and returns the submerged volume and its center of buoyancy
/// Construct this class and then one by one add all faces of the polyhedron using the AddFace function. After all faces have been added the result
/// can be gotten through GetResult.
class PolyhedronSubmergedVolumeCalculator
{
private:
	// Calculate submerged volume * 6 and center of mass * 4 for a tetrahedron with 4 vertices submerged
	// inV1 .. inV4 are submerged
	inline static void	sTetrahedronVolume4(Vec3Arg inV1, Vec3Arg inV2, Vec3Arg inV3, Vec3Arg inV4, float &outVolumeTimes6, Vec3 &outCenterTimes4)
	{
		// Calculate center of mass and mass of this tetrahedron,
		// see: https://en.wikipedia.org/wiki/Tetrahedron#Volume
		outVolumeTimes6 = max((inV1 - inV4).Dot((inV2 - inV4).Cross(inV3 - inV4)), 0.0f); // All contributions should be positive because we use a reference point that is on the surface of the hull
		outCenterTimes4 = inV1 + inV2 + inV3 + inV4;
	}

	// Get the intersection point with a plane.
	// inV1 is inD1 distance away from the plane, inV2 is inD2 distance away from the plane
	inline static Vec3	sGetPlaneIntersection(Vec3Arg inV1, float inD1, Vec3Arg inV2, float inD2)
	{
		JPH_ASSERT(Sign(inD1) != Sign(inD2), "Assuming both points are on opposite ends of the plane");
		float delta = inD1 - inD2;
		if (abs(delta) < 1.0e-6f)
			return inV1; // Parallel to plane, just pick a point
		else
			return inV1 + inD1 * (inV2 - inV1) / delta;
	}

	// Calculate submerged volume * 6 and center of mass * 4 for a tetrahedron with 1 vertex submerged
	// inV1 is submerged, inV2 .. inV4 are not
	// inD1 .. inD4 are the distances from the points to the plane
	inline JPH_IF_NOT_DEBUG_RENDERER(static) void sTetrahedronVolume1(Vec3Arg inV1, float inD1, Vec3Arg inV2, float inD2, Vec3Arg inV3, float inD3, Vec3Arg inV4, float inD4, float &outVolumeTimes6, Vec3 &outCenterTimes4)
	{
		// A tetrahedron with 1 point submerged is cut along 3 edges forming a new tetrahedron
		Vec3 v2 = sGetPlaneIntersection(inV1, inD1, inV2, inD2);
		Vec3 v3 = sGetPlaneIntersection(inV1, inD1, inV3, inD3);
		Vec3 v4 = sGetPlaneIntersection(inV1, inD1, inV4, inD4);

	#ifdef JPH_DEBUG_RENDERER
		// Draw intersection between tetrahedron and surface
		if (Shape::sDrawSubmergedVolumes)
		{
			RVec3 v2w = mBaseOffset + v2;
			RVec3 v3w = mBaseOffset + v3;
			RVec3 v4w = mBaseOffset + v4;

			DebugRenderer::sInstance->DrawTriangle(v4w, v3w, v2w, Color::sGreen);
			DebugRenderer::sInstance->DrawWireTriangle(v4w, v3w, v2w, Color::sWhite);
		}
	#endif // JPH_DEBUG_RENDERER

		sTetrahedronVolume4(inV1, v2, v3, v4, outVolumeTimes6, outCenterTimes4);
	}

	// Calculate submerged volume * 6 and center of mass * 4 for a tetrahedron with 2 vertices submerged
	// inV1, inV2 are submerged, inV3, inV4 are not
	// inD1 .. inD4 are the distances from the points to the plane
	inline JPH_IF_NOT_DEBUG_RENDERER(static) void sTetrahedronVolume2(Vec3Arg inV1, float inD1, Vec3Arg inV2, float inD2, Vec3Arg inV3, float inD3, Vec3Arg inV4, float inD4, float &outVolumeTimes6, Vec3 &outCenterTimes4)
	{
		// A tetrahedron with 2 points submerged is cut along 4 edges forming a quad
		Vec3 c = sGetPlaneIntersection(inV1, inD1, inV3, inD3);
		Vec3 d = sGetPlaneIntersection(inV1, inD1, inV4, inD4);
		Vec3 e = sGetPlaneIntersection(inV2, inD2, inV4, inD4);
		Vec3 f = sGetPlaneIntersection(inV2, inD2, inV3, inD3);

	#ifdef JPH_DEBUG_RENDERER
		// Draw intersection between tetrahedron and surface
		if (Shape::sDrawSubmergedVolumes)
		{
			RVec3 cw = mBaseOffset + c;
			RVec3 dw = mBaseOffset + d;
			RVec3 ew = mBaseOffset + e;
			RVec3 fw = mBaseOffset + f;

			DebugRenderer::sInstance->DrawTriangle(cw, ew, dw, Color::sGreen);
			DebugRenderer::sInstance->DrawTriangle(cw, fw, ew, Color::sGreen);
			DebugRenderer::sInstance->DrawWireTriangle(cw, ew, dw, Color::sWhite);
			DebugRenderer::sInstance->DrawWireTriangle(cw, fw, ew, Color::sWhite);
		}
	#endif // JPH_DEBUG_RENDERER

		// We pick point c as reference (which is on the cut off surface)
		// This leaves us with three tetrahedrons to sum up (any faces that are in the same plane as c will have zero volume)
		Vec3 center1, center2, center3;
		float volume1, volume2, volume3;
		sTetrahedronVolume4(e, f, inV2, c, volume1, center1);
		sTetrahedronVolume4(e, inV1, d, c, volume2, center2);
		sTetrahedronVolume4(e, inV2, inV1, c, volume3, center3);

		// Tally up the totals
		outVolumeTimes6 = volume1 + volume2 + volume3;
		outCenterTimes4 = outVolumeTimes6 > 0.0f? (volume1 * center1 + volume2 * center2 + volume3 * center3) / outVolumeTimes6 : Vec3::sZero();
	}

	// Calculate submerged volume * 6 and center of mass * 4 for a tetrahedron with 3 vertices submerged
	// inV1, inV2, inV3 are submerged, inV4 is not
	// inD1 .. inD4 are the distances from the points to the plane
	inline JPH_IF_NOT_DEBUG_RENDERER(static) void sTetrahedronVolume3(Vec3Arg inV1, float inD1, Vec3Arg inV2, float inD2, Vec3Arg inV3, float inD3, Vec3Arg inV4, float inD4, float &outVolumeTimes6, Vec3 &outCenterTimes4)
	{
		// A tetrahedron with 1 point above the surface is cut along 3 edges forming a new tetrahedron
		Vec3 v1 = sGetPlaneIntersection(inV1, inD1, inV4, inD4);
		Vec3 v2 = sGetPlaneIntersection(inV2, inD2, inV4, inD4);
		Vec3 v3 = sGetPlaneIntersection(inV3, inD3, inV4, inD4);

	#ifdef JPH_DEBUG_RENDERER
		// Draw intersection between tetrahedron and surface
		if (Shape::sDrawSubmergedVolumes)
		{
			RVec3 v1w = mBaseOffset + v1;
			RVec3 v2w = mBaseOffset + v2;
			RVec3 v3w = mBaseOffset + v3;

			DebugRenderer::sInstance->DrawTriangle(v3w, v2w, v1w, Color::sGreen);
			DebugRenderer::sInstance->DrawWireTriangle(v3w, v2w, v1w, Color::sWhite);
		}
	#endif // JPH_DEBUG_RENDERER

		Vec3 dry_center, total_center;
		float dry_volume, total_volume;

		// We first calculate the part that is above the surface
		sTetrahedronVolume4(v1, v2, v3, inV4, dry_volume, dry_center);

		// Calculate the total volume
		sTetrahedronVolume4(inV1, inV2, inV3, inV4, total_volume, total_center);

		// From this we can calculate the center and volume of the submerged part
		outVolumeTimes6 = max(total_volume - dry_volume, 0.0f);
		outCenterTimes4 = outVolumeTimes6 > 0.0f? (total_center * total_volume - dry_center * dry_volume) / outVolumeTimes6 : Vec3::sZero();
	}

public:
	/// A helper class that contains cached information about a polyhedron vertex
	class Point
	{
	public:
		Vec3			mPosition;						///< World space position of vertex
		float			mDistanceToSurface;				///< Signed distance to the surface (> 0 is above, < 0 is below)
		bool			mAboveSurface;					///< If the point is above the surface (mDistanceToSurface > 0)
	};

	/// Constructor
	/// @param inTransform Transform to transform all incoming points with
	/// @param inPoints Array of points that are part of the polyhedron
	/// @param inPointStride Amount of bytes between each point (should usually be sizeof(Vec3))
	/// @param inNumPoints The amount of points
	/// @param inSurface The plane that forms the fluid surface (normal should point up)
	/// @param ioBuffer A temporary buffer of Point's that should have inNumPoints entries and should stay alive while this class is alive
#ifdef JPH_DEBUG_RENDERER
	/// @param inBaseOffset The offset to transform inTransform to world space (in double precision mode this can be used to shift the whole operation closer to the origin). Only used for debug drawing.
#endif // JPH_DEBUG_RENDERER
						PolyhedronSubmergedVolumeCalculator(const Mat44 &inTransform, const Vec3 *inPoints, int inPointStride, int inNumPoints, const Plane &inSurface, Point *ioBuffer
#ifdef JPH_DEBUG_RENDERER // Not using JPH_IF_DEBUG_RENDERER for Doxygen
		, RVec3 inBaseOffset
#endif // JPH_DEBUG_RENDERER
		) :
		mPoints(ioBuffer)
#ifdef JPH_DEBUG_RENDERER
		, mBaseOffset(inBaseOffset)
#endif // JPH_DEBUG_RENDERER
	{
		// Convert the points to world space and determine the distance to the surface
		float reference_dist = FLT_MAX;
		for (int p = 0; p < inNumPoints; ++p)
		{
			// Calculate values
			Vec3 transformed_point = inTransform * *reinterpret_cast<const Vec3 *>(reinterpret_cast<const uint8 *>(inPoints) + p * inPointStride);
			float dist = inSurface.SignedDistance(transformed_point);
			bool above = dist >= 0.0f;

			// Keep track if all are above or below
			mAllAbove &= above;
			mAllBelow &= !above;

			// Calculate lowest point, we use this to create tetrahedrons out of all faces
			if (reference_dist > dist)
			{
				mReferencePointIdx = p;
				reference_dist = dist;
			}

			// Store values
			ioBuffer->mPosition = transformed_point;
			ioBuffer->mDistanceToSurface = dist;
			ioBuffer->mAboveSurface = above;
			++ioBuffer;
		}
	}

	/// Check if all points are above the surface. Should be used as early out.
	inline bool			AreAllAbove() const
	{
		return mAllAbove;
	}

	/// Check if all points are below the surface. Should be used as early out.
	inline bool			AreAllBelow() const
	{
		return mAllBelow;
	}

	/// Get the lowest point of the polyhedron. Used to form the 4th vertex to make a tetrahedron out of a polyhedron face.
	inline int			GetReferencePointIdx() const
	{
		return mReferencePointIdx;
	}

	/// Add a polyhedron face. Supply the indices of the points that form the face (in counter clockwise order).
	void				AddFace(int inIdx1, int inIdx2, int inIdx3)
	{
		JPH_ASSERT(inIdx1 != mReferencePointIdx && inIdx2 != mReferencePointIdx && inIdx3 != mReferencePointIdx, "A face using the reference point will not contribute to the volume");

		// Find the points
		const Point &ref = mPoints[mReferencePointIdx];
		const Point &p1 = mPoints[inIdx1];
		const Point &p2 = mPoints[inIdx2];
		const Point &p3 = mPoints[inIdx3];

		// Determine which vertices are submerged
		uint code = (p1.mAboveSurface? 0 : 0b001) | (p2.mAboveSurface? 0 : 0b010) | (p3.mAboveSurface? 0 : 0b100);

		float volume;
		Vec3 center;
		switch (code)
		{
		case 0b000:
			// One point submerged
			sTetrahedronVolume1(ref.mPosition, ref.mDistanceToSurface, p3.mPosition, p3.mDistanceToSurface, p2.mPosition, p2.mDistanceToSurface, p1.mPosition, p1.mDistanceToSurface, volume, center);
			break;

		case 0b001:
			// Two points submerged
			sTetrahedronVolume2(ref.mPosition, ref.mDistanceToSurface, p1.mPosition, p1.mDistanceToSurface, p3.mPosition, p3.mDistanceToSurface, p2.mPosition, p2.mDistanceToSurface, volume, center);
			break;

		case 0b010:
			// Two points submerged
			sTetrahedronVolume2(ref.mPosition, ref.mDistanceToSurface, p2.mPosition, p2.mDistanceToSurface, p1.mPosition, p1.mDistanceToSurface, p3.mPosition, p3.mDistanceToSurface, volume, center);
			break;

		case 0b100:
			// Two points submerged
			sTetrahedronVolume2(ref.mPosition, ref.mDistanceToSurface, p3.mPosition, p3.mDistanceToSurface, p2.mPosition, p2.mDistanceToSurface, p1.mPosition, p1.mDistanceToSurface, volume, center);
			break;

		case 0b011:
			// Three points submerged
			sTetrahedronVolume3(ref.mPosition, ref.mDistanceToSurface, p2.mPosition, p2.mDistanceToSurface, p1.mPosition, p1.mDistanceToSurface, p3.mPosition, p3.mDistanceToSurface, volume, center);
			break;

		case 0b101:
			// Three points submerged
			sTetrahedronVolume3(ref.mPosition, ref.mDistanceToSurface, p1.mPosition, p1.mDistanceToSurface, p3.mPosition, p3.mDistanceToSurface, p2.mPosition, p2.mDistanceToSurface, volume, center);
			break;

		case 0b110:
			// Three points submerged
			sTetrahedronVolume3(ref.mPosition, ref.mDistanceToSurface, p3.mPosition, p3.mDistanceToSurface, p2.mPosition, p2.mDistanceToSurface, p1.mPosition, p1.mDistanceToSurface, volume, center);
			break;

		case 0b111:
			// Four points submerged
			sTetrahedronVolume4(ref.mPosition, p3.mPosition, p2.mPosition, p1.mPosition, volume, center);
			break;

		default:
			// Should not be possible
			JPH_ASSERT(false);
			volume = 0.0f;
			center = Vec3::sZero();
			break;
		}

		mSubmergedVolume += volume;
		mCenterOfBuoyancy += volume * center;
	}

	/// Call after all faces have been added. Returns the submerged volume and the center of buoyancy for the submerged volume.
	void				GetResult(float &outSubmergedVolume, Vec3 &outCenterOfBuoyancy) const
	{
		outCenterOfBuoyancy = mSubmergedVolume > 0.0f? mCenterOfBuoyancy / (4.0f * mSubmergedVolume) : Vec3::sZero(); // Do this before dividing submerged volume by 6 to get correct weight factor
		outSubmergedVolume = mSubmergedVolume / 6.0f;
	}

private:
	// The precalculated points for this polyhedron
	const Point *		mPoints;

	// If all points are above/below the surface
	bool				mAllBelow = true;
	bool				mAllAbove = true;

	// The lowest point
	int					mReferencePointIdx = 0;

	// Aggregator for submerged volume and center of buoyancy
	float				mSubmergedVolume = 0.0f;
	Vec3				mCenterOfBuoyancy = Vec3::sZero();

#ifdef JPH_DEBUG_RENDERER
	// Base offset used for drawing
	RVec3				mBaseOffset;
#endif
};

JPH_NAMESPACE_END

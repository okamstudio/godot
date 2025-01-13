// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2024 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Jolt/Core/QuickSort.h>
#include <Jolt/Physics/Collision/CollisionDispatch.h>

//#define JPH_INTERNAL_EDGE_REMOVING_COLLECTOR_DEBUG

#ifdef JPH_INTERNAL_EDGE_REMOVING_COLLECTOR_DEBUG
#include <Jolt/Renderer/DebugRenderer.h>
#endif // JPH_INTERNAL_EDGE_REMOVING_COLLECTOR_DEBUG

JPH_NAMESPACE_BEGIN

/// Removes internal edges from collision results. Can be used to filter out 'ghost collisions'.
/// Based on: Contact generation for meshes - Pierre Terdiman (https://www.codercorner.com/MeshContacts.pdf)
class InternalEdgeRemovingCollector : public CollideShapeCollector
{
	static constexpr uint cMaxDelayedResults = 16;
	static constexpr uint cMaxVoidedFeatures = 128;

	/// Check if a vertex is voided
	inline bool				IsVoided(const SubShapeID &inSubShapeID, Vec3 inV) const
	{
		for (const Voided &vf : mVoidedFeatures)
			if (vf.mSubShapeID == inSubShapeID
				&& inV.IsClose(Vec3::sLoadFloat3Unsafe(vf.mFeature), 1.0e-8f))
				return true;
		return false;
	}

	/// Add all vertices of a face to the voided features
	inline void				VoidFeatures(const CollideShapeResult &inResult)
	{
		if (mVoidedFeatures.size() < cMaxVoidedFeatures)
			for (const Vec3 &v : inResult.mShape2Face)
				if (!IsVoided(inResult.mSubShapeID1, v))
				{
					Voided vf;
					v.StoreFloat3(&vf.mFeature);
					vf.mSubShapeID = inResult.mSubShapeID1;
					mVoidedFeatures.push_back(vf);
					if (mVoidedFeatures.size() == cMaxVoidedFeatures)
						break;
				}
	}

	/// Call the chained collector
	inline void				Chain(const CollideShapeResult &inResult)
	{
		// Make sure the chained collector has the same context as we do
		mChainedCollector.SetContext(GetContext());

		// Forward the hit
		mChainedCollector.AddHit(inResult);

		// If our chained collector updated its early out fraction, we need to follow
		UpdateEarlyOutFraction(mChainedCollector.GetEarlyOutFraction());
	}

	/// Call the chained collector and void all features of inResult
	inline void				ChainAndVoid(const CollideShapeResult &inResult)
	{
		Chain(inResult);
		VoidFeatures(inResult);

	#ifdef JPH_INTERNAL_EDGE_REMOVING_COLLECTOR_DEBUG
		DebugRenderer::sInstance->DrawWirePolygon(RMat44::sIdentity(), inResult.mShape2Face, Color::sGreen);
		DebugRenderer::sInstance->DrawArrow(RVec3(inResult.mContactPointOn2), RVec3(inResult.mContactPointOn2) + inResult.mPenetrationAxis.NormalizedOr(Vec3::sZero()), Color::sGreen, 0.1f);
	#endif // JPH_INTERNAL_EDGE_REMOVING_COLLECTOR_DEBUG
	}

public:
	/// Constructor, configures a collector to be called with all the results that do not hit internal edges
	explicit				InternalEdgeRemovingCollector(CollideShapeCollector &inChainedCollector) :
		mChainedCollector(inChainedCollector)
	{
	}

	// See: CollideShapeCollector::Reset
	virtual void			Reset() override
	{
		CollideShapeCollector::Reset();

		mChainedCollector.Reset();

		mVoidedFeatures.clear();
		mDelayedResults.clear();
	}

	// See: CollideShapeCollector::OnBody
	virtual void			OnBody(const Body &inBody) override
	{
		// Just forward the call to our chained collector
		mChainedCollector.OnBody(inBody);
	}

	// See: CollideShapeCollector::AddHit
	virtual void			AddHit(const CollideShapeResult &inResult) override
	{
		// We only support welding when the shape is a triangle or has more vertices so that we can calculate a normal
		if (inResult.mShape2Face.size() < 3)
			return ChainAndVoid(inResult);

		// Get the triangle normal of shape 2 face
		Vec3 triangle_normal = (inResult.mShape2Face[1] - inResult.mShape2Face[0]).Cross(inResult.mShape2Face[2] - inResult.mShape2Face[0]);
		float triangle_normal_len = triangle_normal.Length();
		if (triangle_normal_len < 1e-6f)
			return ChainAndVoid(inResult);

		// If the triangle normal matches the contact normal within 1 degree, we can process the contact immediately
		// We make the assumption here that if the contact normal and the triangle normal align that the we're dealing with a 'face contact'
		Vec3 contact_normal = -inResult.mPenetrationAxis;
		float contact_normal_len = inResult.mPenetrationAxis.Length();
		if (triangle_normal.Dot(contact_normal) > 0.999848f * contact_normal_len * triangle_normal_len) // cos(1 degree)
			return ChainAndVoid(inResult);

		// Delayed processing
		if (mDelayedResults.size() == cMaxDelayedResults)
			return ChainAndVoid(inResult);
		mDelayedResults.push_back(inResult);
	}

	/// After all hits have been added, call this function to process the delayed results
	void					Flush()
	{
		// Sort on biggest penetration depth first
		uint sorted_indices[cMaxDelayedResults];
		for (uint i = 0; i < uint(mDelayedResults.size()); ++i)
			sorted_indices[i] = i;
		QuickSort(sorted_indices, sorted_indices + mDelayedResults.size(), [this](uint inLHS, uint inRHS) { return mDelayedResults[inLHS].mPenetrationDepth > mDelayedResults[inRHS].mPenetrationDepth; });

		// Loop over all results
		for (uint i = 0; i < uint(mDelayedResults.size()); ++i)
		{
			const CollideShapeResult &r = mDelayedResults[sorted_indices[i]];

			// Determine which vertex or which edge is the closest to the contact point
			float best_dist_sq = FLT_MAX;
			uint best_v1_idx = 0;
			uint best_v2_idx = 0;
			uint num_v = uint(r.mShape2Face.size());
			uint v1_idx = num_v - 1;
			Vec3 v1 = r.mShape2Face[v1_idx] - r.mContactPointOn2;
			for (uint v2_idx = 0; v2_idx < num_v; ++v2_idx)
			{
				Vec3 v2 = r.mShape2Face[v2_idx] - r.mContactPointOn2;
				Vec3 v1_v2 = v2 - v1;
				float denominator = v1_v2.LengthSq();
				if (denominator < Square(FLT_EPSILON))
				{
					// Degenerate, assume v1 is closest, v2 will be tested in a later iteration
					float v1_len_sq = v1.LengthSq();
					if (v1_len_sq < best_dist_sq)
					{
						best_dist_sq = v1_len_sq;
						best_v1_idx = v1_idx;
						best_v2_idx = v1_idx;
					}
				}
				else
				{
					// Taken from ClosestPoint::GetBaryCentricCoordinates
					float fraction = -v1.Dot(v1_v2) / denominator;
					if (fraction < 1.0e-6f)
					{
						// Closest lies on v1
						float v1_len_sq = v1.LengthSq();
						if (v1_len_sq < best_dist_sq)
						{
							best_dist_sq = v1_len_sq;
							best_v1_idx = v1_idx;
							best_v2_idx = v1_idx;
						}
					}
					else if (fraction < 1.0f - 1.0e-6f)
					{
						// Closest lies on the line segment v1, v2
						Vec3 closest = v1 + fraction * v1_v2;
						float closest_len_sq = closest.LengthSq();
						if (closest_len_sq < best_dist_sq)
						{
							best_dist_sq = closest_len_sq;
							best_v1_idx = v1_idx;
							best_v2_idx = v2_idx;
						}
					}
					// else closest is v2, but v2 will be tested in a later iteration
				}

				v1_idx = v2_idx;
				v1 = v2;
			}

			// Check if this vertex/edge is voided
			bool voided = IsVoided(r.mSubShapeID1, r.mShape2Face[best_v1_idx])
				&& (best_v1_idx == best_v2_idx || IsVoided(r.mSubShapeID1, r.mShape2Face[best_v2_idx]));

		#ifdef JPH_INTERNAL_EDGE_REMOVING_COLLECTOR_DEBUG
			Color color = voided? Color::sRed : Color::sYellow;
			DebugRenderer::sInstance->DrawText3D(RVec3(r.mContactPointOn2), StringFormat("%d: %g", i, r.mPenetrationDepth), color, 0.1f);
			DebugRenderer::sInstance->DrawWirePolygon(RMat44::sIdentity(), r.mShape2Face, color);
			DebugRenderer::sInstance->DrawArrow(RVec3(r.mContactPointOn2), RVec3(r.mContactPointOn2) + r.mPenetrationAxis.NormalizedOr(Vec3::sZero()), color, 0.1f);
			DebugRenderer::sInstance->DrawMarker(RVec3(r.mShape2Face[best_v1_idx]), IsVoided(r.mSubShapeID1, r.mShape2Face[best_v1_idx])? Color::sRed : Color::sYellow, 0.1f);
			DebugRenderer::sInstance->DrawMarker(RVec3(r.mShape2Face[best_v2_idx]), IsVoided(r.mSubShapeID1, r.mShape2Face[best_v2_idx])? Color::sRed : Color::sYellow, 0.1f);
		#endif // JPH_INTERNAL_EDGE_REMOVING_COLLECTOR_DEBUG

			// No voided features, accept the contact
			if (!voided)
				Chain(r);

			// Void the features of this face
			VoidFeatures(r);
		}

		// All delayed results have been processed
		mVoidedFeatures.clear();
		mDelayedResults.clear();
	}

	/// Version of CollisionDispatch::sCollideShapeVsShape that removes internal edges
	static void				sCollideShapeVsShape(const Shape *inShape1, const Shape *inShape2, Vec3Arg inScale1, Vec3Arg inScale2, Mat44Arg inCenterOfMassTransform1, Mat44Arg inCenterOfMassTransform2, const SubShapeIDCreator &inSubShapeIDCreator1, const SubShapeIDCreator &inSubShapeIDCreator2, const CollideShapeSettings &inCollideShapeSettings, CollideShapeCollector &ioCollector, const ShapeFilter &inShapeFilter = { })
	{
		JPH_ASSERT(inCollideShapeSettings.mActiveEdgeMode == EActiveEdgeMode::CollideWithAll); // Won't work without colliding with all edges
		JPH_ASSERT(inCollideShapeSettings.mCollectFacesMode == ECollectFacesMode::CollectFaces); // Won't work without collecting faces

		InternalEdgeRemovingCollector wrapper(ioCollector);
		CollisionDispatch::sCollideShapeVsShape(inShape1, inShape2, inScale1, inScale2, inCenterOfMassTransform1, inCenterOfMassTransform2, inSubShapeIDCreator1, inSubShapeIDCreator2, inCollideShapeSettings, wrapper, inShapeFilter);
		wrapper.Flush();
	}

private:
	// This algorithm tests a convex shape (shape 1) against a set of polygons (shape 2).
	// This assumption doesn't hold if the shape we're testing is a compound shape, so we must also
	// store the sub shape ID and ignore voided features that belong to another sub shape ID.
	struct Voided
	{
		Float3				mFeature;				// Feature that is voided (of shape 2). Read with Vec3::sLoadFloat3Unsafe so must not be the last member.
		SubShapeID			mSubShapeID;			// Sub shape ID of the shape that is colliding against the feature (of shape 1).
	};

	CollideShapeCollector &	mChainedCollector;
	StaticArray<Voided, cMaxVoidedFeatures> mVoidedFeatures;
	StaticArray<CollideShapeResult, cMaxDelayedResults> mDelayedResults;
};

JPH_NAMESPACE_END

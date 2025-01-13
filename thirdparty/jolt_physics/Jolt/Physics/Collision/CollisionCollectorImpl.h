// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Jolt/Physics/Collision/CollisionCollector.h>
#include <Jolt/Core/QuickSort.h>

JPH_NAMESPACE_BEGIN

/// Simple implementation that collects all hits and optionally sorts them on distance
template <class CollectorType>
class AllHitCollisionCollector : public CollectorType
{
public:
	/// Redeclare ResultType
	using ResultType = typename CollectorType::ResultType;

	// See: CollectorType::Reset
	virtual void		Reset() override
	{
		CollectorType::Reset();

		mHits.clear();
	}

	// See: CollectorType::AddHit
	virtual void		AddHit(const ResultType &inResult) override
	{
		mHits.push_back(inResult);
	}

	/// Order hits on closest first
	void				Sort()
	{
		QuickSort(mHits.begin(), mHits.end(), [](const ResultType &inLHS, const ResultType &inRHS) { return inLHS.GetEarlyOutFraction() < inRHS.GetEarlyOutFraction(); });
	}

	/// Check if any hits were collected
	inline bool			HadHit() const
	{
		return !mHits.empty();
	}

	Array<ResultType>	mHits;
};

/// Simple implementation that collects the closest / deepest hit
template <class CollectorType>
class ClosestHitCollisionCollector : public CollectorType
{
public:
	/// Redeclare ResultType
	using ResultType = typename CollectorType::ResultType;

	// See: CollectorType::Reset
	virtual void		Reset() override
	{
		CollectorType::Reset();

		mHadHit = false;
	}

	// See: CollectorType::AddHit
	virtual void		AddHit(const ResultType &inResult) override
	{
		float early_out = inResult.GetEarlyOutFraction();
		if (!mHadHit || early_out < mHit.GetEarlyOutFraction())
		{
			// Update early out fraction
			CollectorType::UpdateEarlyOutFraction(early_out);

			// Store hit
			mHit = inResult;
			mHadHit = true;
		}
	}

	/// Check if this collector has had a hit
	inline bool			HadHit() const
	{
		return mHadHit;
	}

	ResultType			mHit;

private:
	bool				mHadHit = false;
};

/// Simple implementation that collects any hit
template <class CollectorType>
class AnyHitCollisionCollector : public CollectorType
{
public:
	/// Redeclare ResultType
	using ResultType = typename CollectorType::ResultType;

	// See: CollectorType::Reset
	virtual void		Reset() override
	{
		CollectorType::Reset();

		mHadHit = false;
	}

	// See: CollectorType::AddHit
	virtual void		AddHit(const ResultType &inResult) override
	{
		// Test that the collector is not collecting more hits after forcing an early out
		JPH_ASSERT(!mHadHit);

		// Abort any further testing
		CollectorType::ForceEarlyOut();

		// Store hit
		mHit = inResult;
		mHadHit = true;
	}

	/// Check if this collector has had a hit
	inline bool			HadHit() const
	{
		return mHadHit;
	}

	ResultType			mHit;

private:
	bool				mHadHit = false;
};

JPH_NAMESPACE_END

// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

JPH_NAMESPACE_BEGIN

class Body;
class TransformedShape;

/// Traits to use for CastRay
class CollisionCollectorTraitsCastRay
{
public:
	/// For rays the early out fraction is the fraction along the line to order hits.
	static constexpr float InitialEarlyOutFraction = 1.0f + FLT_EPSILON;	///< Furthest hit: Fraction is 1 + epsilon
	static constexpr float ShouldEarlyOutFraction = 0.0f;					///< Closest hit: Fraction is 0
};

/// Traits to use for CastShape
class CollisionCollectorTraitsCastShape
{
public:
	/// For rays the early out fraction is the fraction along the line to order hits.
	static constexpr float InitialEarlyOutFraction = 1.0f + FLT_EPSILON;	///< Furthest hit: Fraction is 1 + epsilon
	static constexpr float ShouldEarlyOutFraction = -FLT_MAX;				///< Deepest hit: Penetration is infinite
};

/// Traits to use for CollideShape
class CollisionCollectorTraitsCollideShape
{
public:
	/// For shape collisions we use -penetration depth to order hits.
	static constexpr float InitialEarlyOutFraction = FLT_MAX;				///< Most shallow hit: Separation is infinite
	static constexpr float ShouldEarlyOutFraction = -FLT_MAX;				///< Deepest hit: Penetration is infinite
};

/// Traits to use for CollidePoint
using CollisionCollectorTraitsCollidePoint = CollisionCollectorTraitsCollideShape;

/// Virtual interface that allows collecting multiple collision results
template <class ResultTypeArg, class TraitsType>
class CollisionCollector
{
public:
	/// Declare ResultType so that derived classes can use it
	using ResultType = ResultTypeArg;

	/// Default constructor
							CollisionCollector() = default;

	/// Constructor to initialize from another collector
	template <class ResultTypeArg2>
	explicit				CollisionCollector(const CollisionCollector<ResultTypeArg2, TraitsType> &inRHS) : mEarlyOutFraction(inRHS.GetEarlyOutFraction()), mContext(inRHS.GetContext()) { }
							CollisionCollector(const CollisionCollector<ResultTypeArg, TraitsType> &inRHS) = default;

	/// Destructor
	virtual					~CollisionCollector() = default;

	/// If you want to reuse this collector, call Reset()
	virtual void			Reset()											{ mEarlyOutFraction = TraitsType::InitialEarlyOutFraction; }

	/// When running a query through the NarrowPhaseQuery class, this will be called for every body that is potentially colliding.
	/// It allows collecting additional information needed by the collision collector implementation from the body under lock protection
	/// before AddHit is called (e.g. the user data pointer or the velocity of the body).
	virtual void			OnBody([[maybe_unused]] const Body &inBody)		{ /* Collects nothing by default */ }

	/// Set by the collision detection functions to the current TransformedShape that we're colliding against before calling the AddHit function
	void					SetContext(const TransformedShape *inContext)	{ mContext = inContext; }
	const TransformedShape *GetContext() const								{ return mContext; }

	/// This function can be used to set some user data on the collision collector
	virtual void			SetUserData(uint64 inUserData)					{ /* Does nothing by default */ }

	/// This function will be called for every hit found, it's up to the application to decide how to store the hit
	virtual void			AddHit(const ResultType &inResult) = 0;

	/// Update the early out fraction (should be lower than before)
	inline void				UpdateEarlyOutFraction(float inFraction)		{ JPH_ASSERT(inFraction <= mEarlyOutFraction); mEarlyOutFraction = inFraction; }

	/// Reset the early out fraction to a specific value
	inline void				ResetEarlyOutFraction(float inFraction = TraitsType::InitialEarlyOutFraction) { mEarlyOutFraction = inFraction; }

	/// Force the collision detection algorithm to terminate as soon as possible. Call this from the AddHit function when a satisfying hit is found.
	inline void				ForceEarlyOut()									{ mEarlyOutFraction = TraitsType::ShouldEarlyOutFraction; }

	/// When true, the collector will no longer accept any additional hits and the collision detection routine should early out as soon as possible
	inline bool				ShouldEarlyOut() const							{ return mEarlyOutFraction <= TraitsType::ShouldEarlyOutFraction; }

	/// Get the current early out value
	inline float			GetEarlyOutFraction() const						{ return mEarlyOutFraction; }

	/// Get the current early out value but make sure it's bigger than zero, this is used for shape casting as negative values are used for penetration
	inline float			GetPositiveEarlyOutFraction() const				{ return max(FLT_MIN, mEarlyOutFraction); }

private:
	/// The early out fraction determines the fraction below which the collector is still accepting a hit (can be used to reduce the amount of work)
	float					mEarlyOutFraction = TraitsType::InitialEarlyOutFraction;

	/// Set by the collision detection functions to the current TransformedShape of the body that we're colliding against before calling the AddHit function
	const TransformedShape *mContext = nullptr;
};

JPH_NAMESPACE_END

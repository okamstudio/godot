// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Jolt/Core/TickCounter.h>
#include <Jolt/Physics/Collision/Shape/Shape.h>

JPH_SUPPRESS_WARNING_PUSH
JPH_CLANG_SUPPRESS_WARNING("-Wc++98-compat-pedantic")

// Shorthand function to ifdef out code if narrow phase stats tracking is off
#ifdef JPH_TRACK_NARROWPHASE_STATS
	#define JPH_IF_TRACK_NARROWPHASE_STATS(...) __VA_ARGS__
#else
	#define JPH_IF_TRACK_NARROWPHASE_STATS(...)
#endif // JPH_TRACK_NARROWPHASE_STATS

JPH_SUPPRESS_WARNING_POP

#ifdef JPH_TRACK_NARROWPHASE_STATS

JPH_NAMESPACE_BEGIN

/// Structure that tracks narrow phase timing information for a particular combination of shapes
class NarrowPhaseStat
{
public:
	/// Trace an individual stat in CSV form.
	void					ReportStats(const char *inName, EShapeSubType inType1, EShapeSubType inType2, uint64 inTicks100Pct) const;

	/// Trace the collected broadphase stats in CSV form.
	/// This report can be used to judge and tweak the efficiency of the broadphase.
	static void				sReportStats();

	atomic<uint64>			mNumQueries = 0;
	atomic<uint64>			mHitsReported = 0;
	atomic<uint64>			mTotalTicks = 0;
	atomic<uint64>			mChildTicks = 0;

	static NarrowPhaseStat	sCollideShape[NumSubShapeTypes][NumSubShapeTypes];
	static NarrowPhaseStat	sCastShape[NumSubShapeTypes][NumSubShapeTypes];
};

/// Object that tracks the start and end of a narrow phase operation
class TrackNarrowPhaseStat
{
public:
							TrackNarrowPhaseStat(NarrowPhaseStat &inStat) :
		mStat(inStat),
		mParent(sRoot),
		mStart(GetProcessorTickCount())
	{
		// Make this the new root of the chain
		sRoot = this;
	}

							~TrackNarrowPhaseStat()
	{
		uint64 delta_ticks = GetProcessorTickCount() - mStart;

		// Notify parent of time spent in child
		if (mParent != nullptr)
			mParent->mStat.mChildTicks += delta_ticks;

		// Increment stats at this level
		mStat.mNumQueries++;
		mStat.mTotalTicks += delta_ticks;

		// Restore root pointer
		JPH_ASSERT(sRoot == this);
		sRoot = mParent;
	}

	NarrowPhaseStat &		mStat;
	TrackNarrowPhaseStat *	mParent;
	uint64					mStart;

	static thread_local TrackNarrowPhaseStat *sRoot;
};

/// Object that tracks the start and end of a hit being processed by a collision collector
class TrackNarrowPhaseCollector
{
public:
							TrackNarrowPhaseCollector() :
		mStart(GetProcessorTickCount())
	{
	}

							~TrackNarrowPhaseCollector()
	{
		// Mark time spent in collector as 'child' time for the parent
		uint64 delta_ticks = GetProcessorTickCount() - mStart;
		if (TrackNarrowPhaseStat::sRoot != nullptr)
			TrackNarrowPhaseStat::sRoot->mStat.mChildTicks += delta_ticks;

		// Notify all parents of a hit
		for (TrackNarrowPhaseStat *track = TrackNarrowPhaseStat::sRoot; track != nullptr; track = track->mParent)
			track->mStat.mHitsReported++;
	}

private:
	uint64					mStart;
};

JPH_NAMESPACE_END

#endif // JPH_TRACK_NARROWPHASE_STATS

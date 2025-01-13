// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#include <Jolt/Jolt.h>

#include <Jolt/Physics/Collision/NarrowPhaseStats.h>

#ifdef JPH_TRACK_NARROWPHASE_STATS

JPH_NAMESPACE_BEGIN

NarrowPhaseStat	NarrowPhaseStat::sCollideShape[NumSubShapeTypes][NumSubShapeTypes];
NarrowPhaseStat	NarrowPhaseStat::sCastShape[NumSubShapeTypes][NumSubShapeTypes];

thread_local TrackNarrowPhaseStat *TrackNarrowPhaseStat::sRoot = nullptr;

void NarrowPhaseStat::ReportStats(const char *inName, EShapeSubType inType1, EShapeSubType inType2, uint64 inTicks100Pct) const
{
	double total_pct = 100.0 * double(mTotalTicks) / double(inTicks100Pct);
	double total_pct_excl_children = 100.0 * double(mTotalTicks - mChildTicks) / double(inTicks100Pct);

	std::stringstream str;
	str << inName << ", " << sSubShapeTypeNames[(int)inType1] << ", " << sSubShapeTypeNames[(int)inType2] << ", " << mNumQueries << ", " << total_pct << ", " << total_pct_excl_children << ", " << total_pct_excl_children / mNumQueries << ", " << mHitsReported;
	Trace(str.str().c_str());
}

void NarrowPhaseStat::sReportStats()
{
	Trace("Query Type, Shape Type 1, Shape Type 2, Num Queries, Total Time (%%), Total Time Excl Children (%%), Total Time Excl. Children / Query (%%), Hits Reported");

	uint64 total_ticks = 0;
	for (EShapeSubType t1 : sAllSubShapeTypes)
		for (EShapeSubType t2 : sAllSubShapeTypes)
		{
			const NarrowPhaseStat &collide_stat = sCollideShape[(int)t1][(int)t2];
			total_ticks += collide_stat.mTotalTicks - collide_stat.mChildTicks;

			const NarrowPhaseStat &cast_stat = sCastShape[(int)t1][(int)t2];
			total_ticks += cast_stat.mTotalTicks - cast_stat.mChildTicks;
		}

	for (EShapeSubType t1 : sAllSubShapeTypes)
		for (EShapeSubType t2 : sAllSubShapeTypes)
		{
			const NarrowPhaseStat &stat = sCollideShape[(int)t1][(int)t2];
			if (stat.mNumQueries > 0)
				stat.ReportStats("CollideShape", t1, t2, total_ticks);
		}

	for (EShapeSubType t1 : sAllSubShapeTypes)
		for (EShapeSubType t2 : sAllSubShapeTypes)
		{
			const NarrowPhaseStat &stat = sCastShape[(int)t1][(int)t2];
			if (stat.mNumQueries > 0)
				stat.ReportStats("CastShape", t1, t2, total_ticks);
		}
}

JPH_NAMESPACE_END

#endif // JPH_TRACK_NARROWPHASE_STATS

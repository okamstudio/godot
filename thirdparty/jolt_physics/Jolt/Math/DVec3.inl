// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Jolt/Core/HashCombine.h>

// Create a std::hash/JPH::Hash for DVec3
JPH_MAKE_HASHABLE(JPH::DVec3, t.GetX(), t.GetY(), t.GetZ())

JPH_NAMESPACE_BEGIN

DVec3::DVec3(Vec3Arg inRHS)
{
#if defined(JPH_USE_AVX)
	mValue = _mm256_cvtps_pd(inRHS.mValue);
#elif defined(JPH_USE_SSE)
	mValue.mLow = _mm_cvtps_pd(inRHS.mValue);
	mValue.mHigh = _mm_cvtps_pd(_mm_shuffle_ps(inRHS.mValue, inRHS.mValue, _MM_SHUFFLE(2, 2, 2, 2)));
#elif defined(JPH_USE_NEON)
	mValue.val[0] = vcvt_f64_f32(vget_low_f32(inRHS.mValue));
	mValue.val[1] = vcvt_high_f64_f32(inRHS.mValue);
#else
	mF64[0] = (double)inRHS.GetX();
	mF64[1] = (double)inRHS.GetY();
	mF64[2] = (double)inRHS.GetZ();
	#ifdef JPH_FLOATING_POINT_EXCEPTIONS_ENABLED
		mF64[3] = mF64[2];
	#endif
#endif
}

DVec3::DVec3(Vec4Arg inRHS) :
	DVec3(Vec3(inRHS))
{
}

DVec3::DVec3(double inX, double inY, double inZ)
{
#if defined(JPH_USE_AVX)
	mValue = _mm256_set_pd(inZ, inZ, inY, inX); // Assure Z and W are the same
#elif defined(JPH_USE_SSE)
	mValue.mLow = _mm_set_pd(inY, inX);
	mValue.mHigh = _mm_set1_pd(inZ);
#elif defined(JPH_USE_NEON)
	mValue.val[0] = vcombine_f64(vcreate_f64(BitCast<uint64>(inX)), vcreate_f64(BitCast<uint64>(inY)));
	mValue.val[1] = vdupq_n_f64(inZ);
#else
	mF64[0] = inX;
	mF64[1] = inY;
	mF64[2] = inZ;
	#ifdef JPH_FLOATING_POINT_EXCEPTIONS_ENABLED
		mF64[3] = mF64[2];
	#endif
#endif
}

DVec3::DVec3(const Double3 &inV)
{
#if defined(JPH_USE_AVX)
	Type x = _mm256_castpd128_pd256(_mm_load_sd(&inV.x));
	Type y = _mm256_castpd128_pd256(_mm_load_sd(&inV.y));
	Type z = _mm256_broadcast_sd(&inV.z);
	Type xy = _mm256_unpacklo_pd(x, y);
	mValue = _mm256_blend_pd(xy, z, 0b1100); // Assure Z and W are the same
#elif defined(JPH_USE_SSE)
	mValue.mLow = _mm_loadu_pd(&inV.x);
	mValue.mHigh = _mm_set1_pd(inV.z);
#elif defined(JPH_USE_NEON)
	mValue.val[0] = vld1q_f64(&inV.x);
	mValue.val[1] = vdupq_n_f64(inV.z);
#else
	mF64[0] = inV.x;
	mF64[1] = inV.y;
	mF64[2] = inV.z;
	#ifdef JPH_FLOATING_POINT_EXCEPTIONS_ENABLED
		mF64[3] = mF64[2];
	#endif
#endif
}

void DVec3::CheckW() const
{
#ifdef JPH_FLOATING_POINT_EXCEPTIONS_ENABLED
	// Avoid asserts when both components are NaN
	JPH_ASSERT(reinterpret_cast<const uint64 *>(mF64)[2] == reinterpret_cast<const uint64 *>(mF64)[3]);
#endif // JPH_FLOATING_POINT_EXCEPTIONS_ENABLED
}

/// Internal helper function that ensures that the Z component is replicated to the W component to prevent divisions by zero
DVec3::Type DVec3::sFixW(TypeArg inValue)
{
#ifdef JPH_FLOATING_POINT_EXCEPTIONS_ENABLED
	#if defined(JPH_USE_AVX)
		return _mm256_shuffle_pd(inValue, inValue, 2);
	#elif defined(JPH_USE_SSE)
		Type value;
		value.mLow = inValue.mLow;
		value.mHigh = _mm_shuffle_pd(inValue.mHigh, inValue.mHigh, 0);
		return value;
	#elif defined(JPH_USE_NEON)
		Type value;
		value.val[0] = inValue.val[0];
		value.val[1] = vdupq_laneq_f64(inValue.val[1], 0);
		return value;
	#else
		Type value;
		value.mData[0] = inValue.mData[0];
		value.mData[1] = inValue.mData[1];
		value.mData[2] = inValue.mData[2];
		value.mData[3] = inValue.mData[2];
		return value;
	#endif
#else
	return inValue;
#endif // JPH_FLOATING_POINT_EXCEPTIONS_ENABLED
}

DVec3 DVec3::sZero()
{
#if defined(JPH_USE_AVX)
	return _mm256_setzero_pd();
#elif defined(JPH_USE_SSE)
	__m128d zero = _mm_setzero_pd();
	return DVec3({ zero, zero });
#elif defined(JPH_USE_NEON)
	float64x2_t zero = vdupq_n_f64(0.0);
	return DVec3({ zero, zero });
#else
	return DVec3(0, 0, 0);
#endif
}

DVec3 DVec3::sReplicate(double inV)
{
#if defined(JPH_USE_AVX)
	return _mm256_set1_pd(inV);
#elif defined(JPH_USE_SSE)
	__m128d value = _mm_set1_pd(inV);
	return DVec3({ value, value });
#elif defined(JPH_USE_NEON)
	float64x2_t value = vdupq_n_f64(inV);
	return DVec3({ value, value });
#else
	return DVec3(inV, inV, inV);
#endif
}

DVec3 DVec3::sNaN()
{
	return sReplicate(numeric_limits<double>::quiet_NaN());
}

DVec3 DVec3::sLoadDouble3Unsafe(const Double3 &inV)
{
#if defined(JPH_USE_AVX)
	Type v = _mm256_loadu_pd(&inV.x);
#elif defined(JPH_USE_SSE)
	Type v;
	v.mLow = _mm_loadu_pd(&inV.x);
	v.mHigh = _mm_set1_pd(inV.z);
#elif defined(JPH_USE_NEON)
	Type v = vld1q_f64_x2(&inV.x);
#else
	Type v = { inV.x, inV.y, inV.z };
#endif
	return sFixW(v);
}

void DVec3::StoreDouble3(Double3 *outV) const
{
	outV->x = mF64[0];
	outV->y = mF64[1];
	outV->z = mF64[2];
}

DVec3::operator Vec3() const
{
#if defined(JPH_USE_AVX)
	return _mm256_cvtpd_ps(mValue);
#elif defined(JPH_USE_SSE)
	__m128 low = _mm_cvtpd_ps(mValue.mLow);
	__m128 high = _mm_cvtpd_ps(mValue.mHigh);
	return _mm_shuffle_ps(low, high, _MM_SHUFFLE(1, 0, 1, 0));
#elif defined(JPH_USE_NEON)
	return vcvt_high_f32_f64(vcvtx_f32_f64(mValue.val[0]), mValue.val[1]);
#else
	return Vec3((float)GetX(), (float)GetY(), (float)GetZ());
#endif
}

DVec3 DVec3::sMin(DVec3Arg inV1, DVec3Arg inV2)
{
#if defined(JPH_USE_AVX)
	return _mm256_min_pd(inV1.mValue, inV2.mValue);
#elif defined(JPH_USE_SSE)
	return DVec3({ _mm_min_pd(inV1.mValue.mLow, inV2.mValue.mLow), _mm_min_pd(inV1.mValue.mHigh, inV2.mValue.mHigh) });
#elif defined(JPH_USE_NEON)
	return DVec3({ vminq_f64(inV1.mValue.val[0], inV2.mValue.val[0]), vminq_f64(inV1.mValue.val[1], inV2.mValue.val[1]) });
#else
	return DVec3(min(inV1.mF64[0], inV2.mF64[0]),
				 min(inV1.mF64[1], inV2.mF64[1]),
				 min(inV1.mF64[2], inV2.mF64[2]));
#endif
}

DVec3 DVec3::sMax(DVec3Arg inV1, DVec3Arg inV2)
{
#if defined(JPH_USE_AVX)
	return _mm256_max_pd(inV1.mValue, inV2.mValue);
#elif defined(JPH_USE_SSE)
	return DVec3({ _mm_max_pd(inV1.mValue.mLow, inV2.mValue.mLow), _mm_max_pd(inV1.mValue.mHigh, inV2.mValue.mHigh) });
#elif defined(JPH_USE_NEON)
	return DVec3({ vmaxq_f64(inV1.mValue.val[0], inV2.mValue.val[0]), vmaxq_f64(inV1.mValue.val[1], inV2.mValue.val[1]) });
#else
	return DVec3(max(inV1.mF64[0], inV2.mF64[0]),
				 max(inV1.mF64[1], inV2.mF64[1]),
				 max(inV1.mF64[2], inV2.mF64[2]));
#endif
}

DVec3 DVec3::sClamp(DVec3Arg inV, DVec3Arg inMin, DVec3Arg inMax)
{
	return sMax(sMin(inV, inMax), inMin);
}

DVec3 DVec3::sEquals(DVec3Arg inV1, DVec3Arg inV2)
{
#if defined(JPH_USE_AVX)
	return _mm256_cmp_pd(inV1.mValue, inV2.mValue, _CMP_EQ_OQ);
#elif defined(JPH_USE_SSE)
	return DVec3({ _mm_cmpeq_pd(inV1.mValue.mLow, inV2.mValue.mLow), _mm_cmpeq_pd(inV1.mValue.mHigh, inV2.mValue.mHigh) });
#elif defined(JPH_USE_NEON)
	return DVec3({ vreinterpretq_f64_u64(vceqq_f64(inV1.mValue.val[0], inV2.mValue.val[0])), vreinterpretq_f64_u64(vceqq_f64(inV1.mValue.val[1], inV2.mValue.val[1])) });
#else
	return DVec3(inV1.mF64[0] == inV2.mF64[0]? cTrue : cFalse,
				 inV1.mF64[1] == inV2.mF64[1]? cTrue : cFalse,
				 inV1.mF64[2] == inV2.mF64[2]? cTrue : cFalse);
#endif
}

DVec3 DVec3::sLess(DVec3Arg inV1, DVec3Arg inV2)
{
#if defined(JPH_USE_AVX)
	return _mm256_cmp_pd(inV1.mValue, inV2.mValue, _CMP_LT_OQ);
#elif defined(JPH_USE_SSE)
	return DVec3({ _mm_cmplt_pd(inV1.mValue.mLow, inV2.mValue.mLow), _mm_cmplt_pd(inV1.mValue.mHigh, inV2.mValue.mHigh) });
#elif defined(JPH_USE_NEON)
	return DVec3({ vreinterpretq_f64_u64(vcltq_f64(inV1.mValue.val[0], inV2.mValue.val[0])), vreinterpretq_f64_u64(vcltq_f64(inV1.mValue.val[1], inV2.mValue.val[1])) });
#else
	return DVec3(inV1.mF64[0] < inV2.mF64[0]? cTrue : cFalse,
				 inV1.mF64[1] < inV2.mF64[1]? cTrue : cFalse,
				 inV1.mF64[2] < inV2.mF64[2]? cTrue : cFalse);
#endif
}

DVec3 DVec3::sLessOrEqual(DVec3Arg inV1, DVec3Arg inV2)
{
#if defined(JPH_USE_AVX)
	return _mm256_cmp_pd(inV1.mValue, inV2.mValue, _CMP_LE_OQ);
#elif defined(JPH_USE_SSE)
	return DVec3({ _mm_cmple_pd(inV1.mValue.mLow, inV2.mValue.mLow), _mm_cmple_pd(inV1.mValue.mHigh, inV2.mValue.mHigh) });
#elif defined(JPH_USE_NEON)
	return DVec3({ vreinterpretq_f64_u64(vcleq_f64(inV1.mValue.val[0], inV2.mValue.val[0])), vreinterpretq_f64_u64(vcleq_f64(inV1.mValue.val[1], inV2.mValue.val[1])) });
#else
	return DVec3(inV1.mF64[0] <= inV2.mF64[0]? cTrue : cFalse,
				 inV1.mF64[1] <= inV2.mF64[1]? cTrue : cFalse,
				 inV1.mF64[2] <= inV2.mF64[2]? cTrue : cFalse);
#endif
}

DVec3 DVec3::sGreater(DVec3Arg inV1, DVec3Arg inV2)
{
#if defined(JPH_USE_AVX)
	return _mm256_cmp_pd(inV1.mValue, inV2.mValue, _CMP_GT_OQ);
#elif defined(JPH_USE_SSE)
	return DVec3({ _mm_cmpgt_pd(inV1.mValue.mLow, inV2.mValue.mLow), _mm_cmpgt_pd(inV1.mValue.mHigh, inV2.mValue.mHigh) });
#elif defined(JPH_USE_NEON)
	return DVec3({ vreinterpretq_f64_u64(vcgtq_f64(inV1.mValue.val[0], inV2.mValue.val[0])), vreinterpretq_f64_u64(vcgtq_f64(inV1.mValue.val[1], inV2.mValue.val[1])) });
#else
	return DVec3(inV1.mF64[0] > inV2.mF64[0]? cTrue : cFalse,
				 inV1.mF64[1] > inV2.mF64[1]? cTrue : cFalse,
				 inV1.mF64[2] > inV2.mF64[2]? cTrue : cFalse);
#endif
}

DVec3 DVec3::sGreaterOrEqual(DVec3Arg inV1, DVec3Arg inV2)
{
#if defined(JPH_USE_AVX)
	return _mm256_cmp_pd(inV1.mValue, inV2.mValue, _CMP_GE_OQ);
#elif defined(JPH_USE_SSE)
	return DVec3({ _mm_cmpge_pd(inV1.mValue.mLow, inV2.mValue.mLow), _mm_cmpge_pd(inV1.mValue.mHigh, inV2.mValue.mHigh) });
#elif defined(JPH_USE_NEON)
	return DVec3({ vreinterpretq_f64_u64(vcgeq_f64(inV1.mValue.val[0], inV2.mValue.val[0])), vreinterpretq_f64_u64(vcgeq_f64(inV1.mValue.val[1], inV2.mValue.val[1])) });
#else
	return DVec3(inV1.mF64[0] >= inV2.mF64[0]? cTrue : cFalse,
				 inV1.mF64[1] >= inV2.mF64[1]? cTrue : cFalse,
				 inV1.mF64[2] >= inV2.mF64[2]? cTrue : cFalse);
#endif
}

DVec3 DVec3::sFusedMultiplyAdd(DVec3Arg inMul1, DVec3Arg inMul2, DVec3Arg inAdd)
{
#if defined(JPH_USE_AVX)
	#ifdef JPH_USE_FMADD
		return _mm256_fmadd_pd(inMul1.mValue, inMul2.mValue, inAdd.mValue);
	#else
		return _mm256_add_pd(_mm256_mul_pd(inMul1.mValue, inMul2.mValue), inAdd.mValue);
	#endif
#elif defined(JPH_USE_NEON)
	return DVec3({ vmlaq_f64(inAdd.mValue.val[0], inMul1.mValue.val[0], inMul2.mValue.val[0]), vmlaq_f64(inAdd.mValue.val[1], inMul1.mValue.val[1], inMul2.mValue.val[1]) });
#else
	return inMul1 * inMul2 + inAdd;
#endif
}

DVec3 DVec3::sSelect(DVec3Arg inNotSet, DVec3Arg inSet, DVec3Arg inControl)
{
#if defined(JPH_USE_AVX)
	return _mm256_blendv_pd(inNotSet.mValue, inSet.mValue, inControl.mValue);
#elif defined(JPH_USE_SSE4_1)
	Type v = { _mm_blendv_pd(inNotSet.mValue.mLow, inSet.mValue.mLow, inControl.mValue.mLow), _mm_blendv_pd(inNotSet.mValue.mHigh, inSet.mValue.mHigh, inControl.mValue.mHigh) };
	return sFixW(v);
#elif defined(JPH_USE_NEON)
	Type v = { vbslq_f64(vreinterpretq_u64_s64(vshrq_n_s64(vreinterpretq_s64_f64(inControl.mValue.val[0]), 63)), inSet.mValue.val[0], inNotSet.mValue.val[0]),
			   vbslq_f64(vreinterpretq_u64_s64(vshrq_n_s64(vreinterpretq_s64_f64(inControl.mValue.val[1]), 63)), inSet.mValue.val[1], inNotSet.mValue.val[1]) };
	return sFixW(v);
#else
	DVec3 result;
	for (int i = 0; i < 3; i++)
		result.mF64[i] = (BitCast<uint64>(inControl.mF64[i]) & (uint64(1) << 63))? inSet.mF64[i] : inNotSet.mF64[i];
#ifdef JPH_FLOATING_POINT_EXCEPTIONS_ENABLED
	result.mF64[3] = result.mF64[2];
#endif // JPH_FLOATING_POINT_EXCEPTIONS_ENABLED
	return result;
#endif
}

DVec3 DVec3::sOr(DVec3Arg inV1, DVec3Arg inV2)
{
#if defined(JPH_USE_AVX)
	return _mm256_or_pd(inV1.mValue, inV2.mValue);
#elif defined(JPH_USE_SSE)
	return DVec3({ _mm_or_pd(inV1.mValue.mLow, inV2.mValue.mLow), _mm_or_pd(inV1.mValue.mHigh, inV2.mValue.mHigh) });
#elif defined(JPH_USE_NEON)
	return DVec3({ vreinterpretq_f64_u64(vorrq_u64(vreinterpretq_u64_f64(inV1.mValue.val[0]), vreinterpretq_u64_f64(inV2.mValue.val[0]))),
				   vreinterpretq_f64_u64(vorrq_u64(vreinterpretq_u64_f64(inV1.mValue.val[1]), vreinterpretq_u64_f64(inV2.mValue.val[1]))) });
#else
	return DVec3(BitCast<double>(BitCast<uint64>(inV1.mF64[0]) | BitCast<uint64>(inV2.mF64[0])),
				 BitCast<double>(BitCast<uint64>(inV1.mF64[1]) | BitCast<uint64>(inV2.mF64[1])),
				 BitCast<double>(BitCast<uint64>(inV1.mF64[2]) | BitCast<uint64>(inV2.mF64[2])));
#endif
}

DVec3 DVec3::sXor(DVec3Arg inV1, DVec3Arg inV2)
{
#if defined(JPH_USE_AVX)
	return _mm256_xor_pd(inV1.mValue, inV2.mValue);
#elif defined(JPH_USE_SSE)
	return DVec3({ _mm_xor_pd(inV1.mValue.mLow, inV2.mValue.mLow), _mm_xor_pd(inV1.mValue.mHigh, inV2.mValue.mHigh) });
#elif defined(JPH_USE_NEON)
	return DVec3({ vreinterpretq_f64_u64(veorq_u64(vreinterpretq_u64_f64(inV1.mValue.val[0]), vreinterpretq_u64_f64(inV2.mValue.val[0]))),
				   vreinterpretq_f64_u64(veorq_u64(vreinterpretq_u64_f64(inV1.mValue.val[1]), vreinterpretq_u64_f64(inV2.mValue.val[1]))) });
#else
	return DVec3(BitCast<double>(BitCast<uint64>(inV1.mF64[0]) ^ BitCast<uint64>(inV2.mF64[0])),
				 BitCast<double>(BitCast<uint64>(inV1.mF64[1]) ^ BitCast<uint64>(inV2.mF64[1])),
				 BitCast<double>(BitCast<uint64>(inV1.mF64[2]) ^ BitCast<uint64>(inV2.mF64[2])));
#endif
}

DVec3 DVec3::sAnd(DVec3Arg inV1, DVec3Arg inV2)
{
#if defined(JPH_USE_AVX)
	return _mm256_and_pd(inV1.mValue, inV2.mValue);
#elif defined(JPH_USE_SSE)
	return DVec3({ _mm_and_pd(inV1.mValue.mLow, inV2.mValue.mLow), _mm_and_pd(inV1.mValue.mHigh, inV2.mValue.mHigh) });
#elif defined(JPH_USE_NEON)
	return DVec3({ vreinterpretq_f64_u64(vandq_u64(vreinterpretq_u64_f64(inV1.mValue.val[0]), vreinterpretq_u64_f64(inV2.mValue.val[0]))),
				   vreinterpretq_f64_u64(vandq_u64(vreinterpretq_u64_f64(inV1.mValue.val[1]), vreinterpretq_u64_f64(inV2.mValue.val[1]))) });
#else
	return DVec3(BitCast<double>(BitCast<uint64>(inV1.mF64[0]) & BitCast<uint64>(inV2.mF64[0])),
				 BitCast<double>(BitCast<uint64>(inV1.mF64[1]) & BitCast<uint64>(inV2.mF64[1])),
				 BitCast<double>(BitCast<uint64>(inV1.mF64[2]) & BitCast<uint64>(inV2.mF64[2])));
#endif
}

int DVec3::GetTrues() const
{
#if defined(JPH_USE_AVX)
	return _mm256_movemask_pd(mValue) & 0x7;
#elif defined(JPH_USE_SSE)
	return (_mm_movemask_pd(mValue.mLow) + (_mm_movemask_pd(mValue.mHigh) << 2)) & 0x7;
#else
	return int((BitCast<uint64>(mF64[0]) >> 63) | ((BitCast<uint64>(mF64[1]) >> 63) << 1) | ((BitCast<uint64>(mF64[2]) >> 63) << 2));
#endif
}

bool DVec3::TestAnyTrue() const
{
	return GetTrues() != 0;
}

bool DVec3::TestAllTrue() const
{
	return GetTrues() == 0x7;
}

bool DVec3::operator == (DVec3Arg inV2) const
{
	return sEquals(*this, inV2).TestAllTrue();
}

bool DVec3::IsClose(DVec3Arg inV2, double inMaxDistSq) const
{
	return (inV2 - *this).LengthSq() <= inMaxDistSq;
}

bool DVec3::IsNearZero(double inMaxDistSq) const
{
	return LengthSq() <= inMaxDistSq;
}

DVec3 DVec3::operator * (DVec3Arg inV2) const
{
#if defined(JPH_USE_AVX)
	return _mm256_mul_pd(mValue, inV2.mValue);
#elif defined(JPH_USE_SSE)
	return DVec3({ _mm_mul_pd(mValue.mLow, inV2.mValue.mLow), _mm_mul_pd(mValue.mHigh, inV2.mValue.mHigh) });
#elif defined(JPH_USE_NEON)
	return DVec3({ vmulq_f64(mValue.val[0], inV2.mValue.val[0]), vmulq_f64(mValue.val[1], inV2.mValue.val[1]) });
#else
	return DVec3(mF64[0] * inV2.mF64[0], mF64[1] * inV2.mF64[1], mF64[2] * inV2.mF64[2]);
#endif
}

DVec3 DVec3::operator * (double inV2) const
{
#if defined(JPH_USE_AVX)
	return _mm256_mul_pd(mValue, _mm256_set1_pd(inV2));
#elif defined(JPH_USE_SSE)
	__m128d v = _mm_set1_pd(inV2);
	return DVec3({ _mm_mul_pd(mValue.mLow, v), _mm_mul_pd(mValue.mHigh, v) });
#elif defined(JPH_USE_NEON)
	return DVec3({ vmulq_n_f64(mValue.val[0], inV2), vmulq_n_f64(mValue.val[1], inV2) });
#else
	return DVec3(mF64[0] * inV2, mF64[1] * inV2, mF64[2] * inV2);
#endif
}

DVec3 operator * (double inV1, DVec3Arg inV2)
{
#if defined(JPH_USE_AVX)
	return _mm256_mul_pd(_mm256_set1_pd(inV1), inV2.mValue);
#elif defined(JPH_USE_SSE)
	__m128d v = _mm_set1_pd(inV1);
	return DVec3({ _mm_mul_pd(v, inV2.mValue.mLow), _mm_mul_pd(v, inV2.mValue.mHigh) });
#elif defined(JPH_USE_NEON)
	return DVec3({ vmulq_n_f64(inV2.mValue.val[0], inV1), vmulq_n_f64(inV2.mValue.val[1], inV1) });
#else
	return DVec3(inV1 * inV2.mF64[0], inV1 * inV2.mF64[1], inV1 * inV2.mF64[2]);
#endif
}

DVec3 DVec3::operator / (double inV2) const
{
#if defined(JPH_USE_AVX)
	return _mm256_div_pd(mValue, _mm256_set1_pd(inV2));
#elif defined(JPH_USE_SSE)
	__m128d v = _mm_set1_pd(inV2);
	return DVec3({ _mm_div_pd(mValue.mLow, v), _mm_div_pd(mValue.mHigh, v) });
#elif defined(JPH_USE_NEON)
	float64x2_t v = vdupq_n_f64(inV2);
	return DVec3({ vdivq_f64(mValue.val[0], v), vdivq_f64(mValue.val[1], v) });
#else
	return DVec3(mF64[0] / inV2, mF64[1] / inV2, mF64[2] / inV2);
#endif
}

DVec3 &DVec3::operator *= (double inV2)
{
#if defined(JPH_USE_AVX)
	mValue = _mm256_mul_pd(mValue, _mm256_set1_pd(inV2));
#elif defined(JPH_USE_SSE)
	__m128d v = _mm_set1_pd(inV2);
	mValue.mLow = _mm_mul_pd(mValue.mLow, v);
	mValue.mHigh = _mm_mul_pd(mValue.mHigh, v);
#elif defined(JPH_USE_NEON)
	mValue.val[0] = vmulq_n_f64(mValue.val[0], inV2);
	mValue.val[1] = vmulq_n_f64(mValue.val[1], inV2);
#else
	for (int i = 0; i < 3; ++i)
		mF64[i] *= inV2;
	#ifdef JPH_FLOATING_POINT_EXCEPTIONS_ENABLED
		mF64[3] = mF64[2];
	#endif
#endif
	return *this;
}

DVec3 &DVec3::operator *= (DVec3Arg inV2)
{
#if defined(JPH_USE_AVX)
	mValue = _mm256_mul_pd(mValue, inV2.mValue);
#elif defined(JPH_USE_SSE)
	mValue.mLow = _mm_mul_pd(mValue.mLow, inV2.mValue.mLow);
	mValue.mHigh = _mm_mul_pd(mValue.mHigh, inV2.mValue.mHigh);
#elif defined(JPH_USE_NEON)
	mValue.val[0] = vmulq_f64(mValue.val[0], inV2.mValue.val[0]);
	mValue.val[1] = vmulq_f64(mValue.val[1], inV2.mValue.val[1]);
#else
	for (int i = 0; i < 3; ++i)
		mF64[i] *= inV2.mF64[i];
	#ifdef JPH_FLOATING_POINT_EXCEPTIONS_ENABLED
		mF64[3] = mF64[2];
	#endif
#endif
	return *this;
}

DVec3 &DVec3::operator /= (double inV2)
{
#if defined(JPH_USE_AVX)
	mValue = _mm256_div_pd(mValue, _mm256_set1_pd(inV2));
#elif defined(JPH_USE_SSE)
	__m128d v = _mm_set1_pd(inV2);
	mValue.mLow = _mm_div_pd(mValue.mLow, v);
	mValue.mHigh = _mm_div_pd(mValue.mHigh, v);
#elif defined(JPH_USE_NEON)
	float64x2_t v = vdupq_n_f64(inV2);
	mValue.val[0] = vdivq_f64(mValue.val[0], v);
	mValue.val[1] = vdivq_f64(mValue.val[1], v);
#else
	for (int i = 0; i < 3; ++i)
		mF64[i] /= inV2;
	#ifdef JPH_FLOATING_POINT_EXCEPTIONS_ENABLED
		mF64[3] = mF64[2];
	#endif
#endif
	return *this;
}

DVec3 DVec3::operator + (Vec3Arg inV2) const
{
#if defined(JPH_USE_AVX)
	return _mm256_add_pd(mValue, _mm256_cvtps_pd(inV2.mValue));
#elif defined(JPH_USE_SSE)
	return DVec3({ _mm_add_pd(mValue.mLow, _mm_cvtps_pd(inV2.mValue)), _mm_add_pd(mValue.mHigh, _mm_cvtps_pd(_mm_shuffle_ps(inV2.mValue, inV2.mValue, _MM_SHUFFLE(2, 2, 2, 2)))) });
#elif defined(JPH_USE_NEON)
	return DVec3({ vaddq_f64(mValue.val[0], vcvt_f64_f32(vget_low_f32(inV2.mValue))), vaddq_f64(mValue.val[1], vcvt_high_f64_f32(inV2.mValue)) });
#else
	return DVec3(mF64[0] + inV2.mF32[0], mF64[1] + inV2.mF32[1], mF64[2] + inV2.mF32[2]);
#endif
}

DVec3 DVec3::operator + (DVec3Arg inV2) const
{
#if defined(JPH_USE_AVX)
	return _mm256_add_pd(mValue, inV2.mValue);
#elif defined(JPH_USE_SSE)
	return DVec3({ _mm_add_pd(mValue.mLow, inV2.mValue.mLow), _mm_add_pd(mValue.mHigh, inV2.mValue.mHigh) });
#elif defined(JPH_USE_NEON)
	return DVec3({ vaddq_f64(mValue.val[0], inV2.mValue.val[0]), vaddq_f64(mValue.val[1], inV2.mValue.val[1]) });
#else
	return DVec3(mF64[0] + inV2.mF64[0], mF64[1] + inV2.mF64[1], mF64[2] + inV2.mF64[2]);
#endif
}

DVec3 &DVec3::operator += (Vec3Arg inV2)
{
#if defined(JPH_USE_AVX)
	mValue = _mm256_add_pd(mValue, _mm256_cvtps_pd(inV2.mValue));
#elif defined(JPH_USE_SSE)
	mValue.mLow = _mm_add_pd(mValue.mLow, _mm_cvtps_pd(inV2.mValue));
	mValue.mHigh = _mm_add_pd(mValue.mHigh, _mm_cvtps_pd(_mm_shuffle_ps(inV2.mValue, inV2.mValue, _MM_SHUFFLE(2, 2, 2, 2))));
#elif defined(JPH_USE_NEON)
	mValue.val[0] = vaddq_f64(mValue.val[0], vcvt_f64_f32(vget_low_f32(inV2.mValue)));
	mValue.val[1] = vaddq_f64(mValue.val[1], vcvt_high_f64_f32(inV2.mValue));
#else
	for (int i = 0; i < 3; ++i)
		mF64[i] += inV2.mF32[i];
	#ifdef JPH_FLOATING_POINT_EXCEPTIONS_ENABLED
		mF64[3] = mF64[2];
	#endif
#endif
	return *this;
}

DVec3 &DVec3::operator += (DVec3Arg inV2)
{
#if defined(JPH_USE_AVX)
	mValue = _mm256_add_pd(mValue, inV2.mValue);
#elif defined(JPH_USE_SSE)
	mValue.mLow = _mm_add_pd(mValue.mLow, inV2.mValue.mLow);
	mValue.mHigh = _mm_add_pd(mValue.mHigh, inV2.mValue.mHigh);
#elif defined(JPH_USE_NEON)
	mValue.val[0] = vaddq_f64(mValue.val[0], inV2.mValue.val[0]);
	mValue.val[1] = vaddq_f64(mValue.val[1], inV2.mValue.val[1]);
#else
	for (int i = 0; i < 3; ++i)
		mF64[i] += inV2.mF64[i];
	#ifdef JPH_FLOATING_POINT_EXCEPTIONS_ENABLED
		mF64[3] = mF64[2];
	#endif
#endif
	return *this;
}

DVec3 DVec3::operator - () const
{
#if defined(JPH_USE_AVX)
	return _mm256_sub_pd(_mm256_setzero_pd(), mValue);
#elif defined(JPH_USE_SSE)
	__m128d zero = _mm_setzero_pd();
	return DVec3({ _mm_sub_pd(zero, mValue.mLow), _mm_sub_pd(zero, mValue.mHigh) });
#elif defined(JPH_USE_NEON)
	#ifdef JPH_CROSS_PLATFORM_DETERMINISTIC
		float64x2_t zero = vdupq_n_f64(0);
		return DVec3({ vsubq_f64(zero, mValue.val[0]), vsubq_f64(zero, mValue.val[1]) });
	#else
		return DVec3({ vnegq_f64(mValue.val[0]), vnegq_f64(mValue.val[1]) });
	#endif
#else
	#ifdef JPH_CROSS_PLATFORM_DETERMINISTIC
		return DVec3(0.0 - mF64[0], 0.0 - mF64[1], 0.0 - mF64[2]);
	#else
		return DVec3(-mF64[0], -mF64[1], -mF64[2]);
	#endif
#endif
}

DVec3 DVec3::operator - (Vec3Arg inV2) const
{
#if defined(JPH_USE_AVX)
	return _mm256_sub_pd(mValue, _mm256_cvtps_pd(inV2.mValue));
#elif defined(JPH_USE_SSE)
	return DVec3({ _mm_sub_pd(mValue.mLow, _mm_cvtps_pd(inV2.mValue)), _mm_sub_pd(mValue.mHigh, _mm_cvtps_pd(_mm_shuffle_ps(inV2.mValue, inV2.mValue, _MM_SHUFFLE(2, 2, 2, 2)))) });
#elif defined(JPH_USE_NEON)
	return DVec3({ vsubq_f64(mValue.val[0], vcvt_f64_f32(vget_low_f32(inV2.mValue))), vsubq_f64(mValue.val[1], vcvt_high_f64_f32(inV2.mValue)) });
#else
	return DVec3(mF64[0] - inV2.mF32[0], mF64[1] - inV2.mF32[1], mF64[2] - inV2.mF32[2]);
#endif
}

DVec3 DVec3::operator - (DVec3Arg inV2) const
{
#if defined(JPH_USE_AVX)
	return _mm256_sub_pd(mValue, inV2.mValue);
#elif defined(JPH_USE_SSE)
	return DVec3({ _mm_sub_pd(mValue.mLow, inV2.mValue.mLow), _mm_sub_pd(mValue.mHigh, inV2.mValue.mHigh) });
#elif defined(JPH_USE_NEON)
	return DVec3({ vsubq_f64(mValue.val[0], inV2.mValue.val[0]), vsubq_f64(mValue.val[1], inV2.mValue.val[1]) });
#else
	return DVec3(mF64[0] - inV2.mF64[0], mF64[1] - inV2.mF64[1], mF64[2] - inV2.mF64[2]);
#endif
}

DVec3 &DVec3::operator -= (Vec3Arg inV2)
{
#if defined(JPH_USE_AVX)
	mValue = _mm256_sub_pd(mValue, _mm256_cvtps_pd(inV2.mValue));
#elif defined(JPH_USE_SSE)
	mValue.mLow = _mm_sub_pd(mValue.mLow, _mm_cvtps_pd(inV2.mValue));
	mValue.mHigh = _mm_sub_pd(mValue.mHigh, _mm_cvtps_pd(_mm_shuffle_ps(inV2.mValue, inV2.mValue, _MM_SHUFFLE(2, 2, 2, 2))));
#elif defined(JPH_USE_NEON)
	mValue.val[0] = vsubq_f64(mValue.val[0], vcvt_f64_f32(vget_low_f32(inV2.mValue)));
	mValue.val[1] = vsubq_f64(mValue.val[1], vcvt_high_f64_f32(inV2.mValue));
#else
	for (int i = 0; i < 3; ++i)
		mF64[i] -= inV2.mF32[i];
	#ifdef JPH_FLOATING_POINT_EXCEPTIONS_ENABLED
		mF64[3] = mF64[2];
	#endif
#endif
	return *this;
}

DVec3 &DVec3::operator -= (DVec3Arg inV2)
{
#if defined(JPH_USE_AVX)
	mValue = _mm256_sub_pd(mValue, inV2.mValue);
#elif defined(JPH_USE_SSE)
	mValue.mLow = _mm_sub_pd(mValue.mLow, inV2.mValue.mLow);
	mValue.mHigh = _mm_sub_pd(mValue.mHigh, inV2.mValue.mHigh);
#elif defined(JPH_USE_NEON)
	mValue.val[0] = vsubq_f64(mValue.val[0], inV2.mValue.val[0]);
	mValue.val[1] = vsubq_f64(mValue.val[1], inV2.mValue.val[1]);
#else
	for (int i = 0; i < 3; ++i)
		mF64[i] -= inV2.mF64[i];
	#ifdef JPH_FLOATING_POINT_EXCEPTIONS_ENABLED
		mF64[3] = mF64[2];
	#endif
#endif
	return *this;
}

DVec3 DVec3::operator / (DVec3Arg inV2) const
{
	inV2.CheckW();
#if defined(JPH_USE_AVX)
	return _mm256_div_pd(mValue, inV2.mValue);
#elif defined(JPH_USE_SSE)
	return DVec3({ _mm_div_pd(mValue.mLow, inV2.mValue.mLow), _mm_div_pd(mValue.mHigh, inV2.mValue.mHigh) });
#elif defined(JPH_USE_NEON)
	return DVec3({ vdivq_f64(mValue.val[0], inV2.mValue.val[0]), vdivq_f64(mValue.val[1], inV2.mValue.val[1]) });
#else
	return DVec3(mF64[0] / inV2.mF64[0], mF64[1] / inV2.mF64[1], mF64[2] / inV2.mF64[2]);
#endif
}

DVec3 DVec3::Abs() const
{
#if defined(JPH_USE_AVX512)
	return _mm256_range_pd(mValue, mValue, 0b1000);
#elif defined(JPH_USE_AVX)
	return _mm256_max_pd(_mm256_sub_pd(_mm256_setzero_pd(), mValue), mValue);
#elif defined(JPH_USE_SSE)
	__m128d zero = _mm_setzero_pd();
	return DVec3({ _mm_max_pd(_mm_sub_pd(zero, mValue.mLow), mValue.mLow), _mm_max_pd(_mm_sub_pd(zero, mValue.mHigh), mValue.mHigh) });
#elif defined(JPH_USE_NEON)
	return DVec3({ vabsq_f64(mValue.val[0]), vabsq_f64(mValue.val[1]) });
#else
	return DVec3(abs(mF64[0]), abs(mF64[1]), abs(mF64[2]));
#endif
}

DVec3 DVec3::Reciprocal() const
{
	return sReplicate(1.0) / mValue;
}

DVec3 DVec3::Cross(DVec3Arg inV2) const
{
#if defined(JPH_USE_AVX2)
	__m256d t1 = _mm256_permute4x64_pd(inV2.mValue, _MM_SHUFFLE(0, 0, 2, 1)); // Assure Z and W are the same
	t1 = _mm256_mul_pd(t1, mValue);
	__m256d t2 = _mm256_permute4x64_pd(mValue, _MM_SHUFFLE(0, 0, 2, 1)); // Assure Z and W are the same
	t2 = _mm256_mul_pd(t2, inV2.mValue);
	__m256d t3 = _mm256_sub_pd(t1, t2);
	return _mm256_permute4x64_pd(t3, _MM_SHUFFLE(0, 0, 2, 1)); // Assure Z and W are the same
#else
	return DVec3(mF64[1] * inV2.mF64[2] - mF64[2] * inV2.mF64[1],
				 mF64[2] * inV2.mF64[0] - mF64[0] * inV2.mF64[2],
				 mF64[0] * inV2.mF64[1] - mF64[1] * inV2.mF64[0]);
#endif
}

double DVec3::Dot(DVec3Arg inV2) const
{
#if defined(JPH_USE_AVX)
	__m256d mul = _mm256_mul_pd(mValue, inV2.mValue);
	__m128d xy = _mm256_castpd256_pd128(mul);
	__m128d yx = _mm_shuffle_pd(xy, xy, 1);
	__m128d sum = _mm_add_pd(xy, yx);
	__m128d zw = _mm256_extractf128_pd(mul, 1);
	sum = _mm_add_pd(sum, zw);
	return _mm_cvtsd_f64(sum);
#elif defined(JPH_USE_SSE)
	__m128d xy = _mm_mul_pd(mValue.mLow, inV2.mValue.mLow);
	__m128d yx = _mm_shuffle_pd(xy, xy, 1);
	__m128d sum = _mm_add_pd(xy, yx);
	__m128d z = _mm_mul_sd(mValue.mHigh, inV2.mValue.mHigh);
	sum = _mm_add_pd(sum, z);
	return _mm_cvtsd_f64(sum);
#elif defined(JPH_USE_NEON)
	float64x2_t mul_low = vmulq_f64(mValue.val[0], inV2.mValue.val[0]);
	float64x2_t mul_high = vmulq_f64(mValue.val[1], inV2.mValue.val[1]);
	return vaddvq_f64(mul_low) + vgetq_lane_f64(mul_high, 0);
#else
	double dot = 0.0;
	for (int i = 0; i < 3; i++)
		dot += mF64[i] * inV2.mF64[i];
	return dot;
#endif
}

double DVec3::LengthSq() const
{
	return Dot(*this);
}

DVec3 DVec3::Sqrt() const
{
#if defined(JPH_USE_AVX)
	return _mm256_sqrt_pd(mValue);
#elif defined(JPH_USE_SSE)
	return DVec3({ _mm_sqrt_pd(mValue.mLow), _mm_sqrt_pd(mValue.mHigh) });
#elif defined(JPH_USE_NEON)
	return DVec3({ vsqrtq_f64(mValue.val[0]), vsqrtq_f64(mValue.val[1]) });
#else
	return DVec3(sqrt(mF64[0]), sqrt(mF64[1]), sqrt(mF64[2]));
#endif
}

double DVec3::Length() const
{
	return sqrt(Dot(*this));
}

DVec3 DVec3::Normalized() const
{
	return *this / Length();
}

bool DVec3::IsNormalized(double inTolerance) const
{
	return abs(LengthSq() - 1.0) <= inTolerance;
}

bool DVec3::IsNaN() const
{
#if defined(JPH_USE_AVX512)
	return (_mm256_fpclass_pd_mask(mValue, 0b10000001) & 0x7) != 0;
#elif defined(JPH_USE_AVX)
	return (_mm256_movemask_pd(_mm256_cmp_pd(mValue, mValue, _CMP_UNORD_Q)) & 0x7) != 0;
#elif defined(JPH_USE_SSE)
	return ((_mm_movemask_pd(_mm_cmpunord_pd(mValue.mLow, mValue.mLow)) + (_mm_movemask_pd(_mm_cmpunord_pd(mValue.mHigh, mValue.mHigh)) << 2)) & 0x7) != 0;
#else
	return isnan(mF64[0]) || isnan(mF64[1]) || isnan(mF64[2]);
#endif
}

DVec3 DVec3::GetSign() const
{
#if defined(JPH_USE_AVX512)
	return _mm256_fixupimm_pd(mValue, mValue, _mm256_set1_epi32(0xA9A90A00), 0);
#elif defined(JPH_USE_AVX)
	__m256d minus_one = _mm256_set1_pd(-1.0);
	__m256d one = _mm256_set1_pd(1.0);
	return _mm256_or_pd(_mm256_and_pd(mValue, minus_one), one);
#elif defined(JPH_USE_SSE)
	__m128d minus_one = _mm_set1_pd(-1.0);
	__m128d one = _mm_set1_pd(1.0);
	return DVec3({ _mm_or_pd(_mm_and_pd(mValue.mLow, minus_one), one), _mm_or_pd(_mm_and_pd(mValue.mHigh, minus_one), one) });
#elif defined(JPH_USE_NEON)
	uint64x2_t minus_one = vreinterpretq_u64_f64(vdupq_n_f64(-1.0f));
	uint64x2_t one = vreinterpretq_u64_f64(vdupq_n_f64(1.0f));
	return DVec3({ vreinterpretq_f64_u64(vorrq_u64(vandq_u64(vreinterpretq_u64_f64(mValue.val[0]), minus_one), one)),
				   vreinterpretq_f64_u64(vorrq_u64(vandq_u64(vreinterpretq_u64_f64(mValue.val[1]), minus_one), one)) });
#else
	return DVec3(std::signbit(mF64[0])? -1.0 : 1.0,
				 std::signbit(mF64[1])? -1.0 : 1.0,
				 std::signbit(mF64[2])? -1.0 : 1.0);
#endif
}

DVec3 DVec3::PrepareRoundToZero() const
{
	// Float has 23 bit mantissa, double 52 bit mantissa => we lose 29 bits when converting from double to float
	constexpr uint64 cDoubleToFloatMantissaLoss = (1U << 29) - 1;

#if defined(JPH_USE_AVX)
	return _mm256_and_pd(mValue, _mm256_castsi256_pd(_mm256_set1_epi64x(int64_t(~cDoubleToFloatMantissaLoss))));
#elif defined(JPH_USE_SSE)
	__m128d mask = _mm_castsi128_pd(_mm_set1_epi64x(int64_t(~cDoubleToFloatMantissaLoss)));
	return DVec3({ _mm_and_pd(mValue.mLow, mask), _mm_and_pd(mValue.mHigh, mask) });
#elif defined(JPH_USE_NEON)
	uint64x2_t mask = vdupq_n_u64(~cDoubleToFloatMantissaLoss);
	return DVec3({ vreinterpretq_f64_u64(vandq_u64(vreinterpretq_u64_f64(mValue.val[0]), mask)),
				   vreinterpretq_f64_u64(vandq_u64(vreinterpretq_u64_f64(mValue.val[1]), mask)) });
#else
	double x = BitCast<double>(BitCast<uint64>(mF64[0]) & ~cDoubleToFloatMantissaLoss);
	double y = BitCast<double>(BitCast<uint64>(mF64[1]) & ~cDoubleToFloatMantissaLoss);
	double z = BitCast<double>(BitCast<uint64>(mF64[2]) & ~cDoubleToFloatMantissaLoss);

	return DVec3(x, y, z);
#endif
}

DVec3 DVec3::PrepareRoundToInf() const
{
	// Float has 23 bit mantissa, double 52 bit mantissa => we lose 29 bits when converting from double to float
	constexpr uint64 cDoubleToFloatMantissaLoss = (1U << 29) - 1;

#if defined(JPH_USE_AVX512)
	__m256i mantissa_loss = _mm256_set1_epi64x(cDoubleToFloatMantissaLoss);
	__mmask8 is_zero = _mm256_testn_epi64_mask(_mm256_castpd_si256(mValue), mantissa_loss);
	__m256d value_or_mantissa_loss = _mm256_or_pd(mValue, _mm256_castsi256_pd(mantissa_loss));
	return _mm256_mask_blend_pd(is_zero, value_or_mantissa_loss, mValue);
#elif defined(JPH_USE_AVX)
	__m256i mantissa_loss = _mm256_set1_epi64x(cDoubleToFloatMantissaLoss);
	__m256d value_and_mantissa_loss = _mm256_and_pd(mValue, _mm256_castsi256_pd(mantissa_loss));
	__m256d is_zero = _mm256_cmp_pd(value_and_mantissa_loss, _mm256_setzero_pd(), _CMP_EQ_OQ);
	__m256d value_or_mantissa_loss = _mm256_or_pd(mValue, _mm256_castsi256_pd(mantissa_loss));
	return _mm256_blendv_pd(value_or_mantissa_loss, mValue, is_zero);
#elif defined(JPH_USE_SSE4_1)
	__m128i mantissa_loss = _mm_set1_epi64x(cDoubleToFloatMantissaLoss);
	__m128d zero = _mm_setzero_pd();
	__m128d value_and_mantissa_loss_low = _mm_and_pd(mValue.mLow, _mm_castsi128_pd(mantissa_loss));
	__m128d is_zero_low = _mm_cmpeq_pd(value_and_mantissa_loss_low, zero);
	__m128d value_or_mantissa_loss_low = _mm_or_pd(mValue.mLow, _mm_castsi128_pd(mantissa_loss));
	__m128d value_and_mantissa_loss_high = _mm_and_pd(mValue.mHigh, _mm_castsi128_pd(mantissa_loss));
	__m128d is_zero_high = _mm_cmpeq_pd(value_and_mantissa_loss_high, zero);
	__m128d value_or_mantissa_loss_high = _mm_or_pd(mValue.mHigh, _mm_castsi128_pd(mantissa_loss));
	return DVec3({ _mm_blendv_pd(value_or_mantissa_loss_low, mValue.mLow, is_zero_low), _mm_blendv_pd(value_or_mantissa_loss_high, mValue.mHigh, is_zero_high) });
#elif defined(JPH_USE_NEON)
	uint64x2_t mantissa_loss = vdupq_n_u64(cDoubleToFloatMantissaLoss);
	float64x2_t zero = vdupq_n_f64(0.0);
	float64x2_t value_and_mantissa_loss_low = vreinterpretq_f64_u64(vandq_u64(vreinterpretq_u64_f64(mValue.val[0]), mantissa_loss));
	uint64x2_t is_zero_low = vceqq_f64(value_and_mantissa_loss_low, zero);
	float64x2_t value_or_mantissa_loss_low = vreinterpretq_f64_u64(vorrq_u64(vreinterpretq_u64_f64(mValue.val[0]), mantissa_loss));
	float64x2_t value_and_mantissa_loss_high = vreinterpretq_f64_u64(vandq_u64(vreinterpretq_u64_f64(mValue.val[1]), mantissa_loss));
	float64x2_t value_low = vbslq_f64(is_zero_low, mValue.val[0], value_or_mantissa_loss_low);
	uint64x2_t is_zero_high = vceqq_f64(value_and_mantissa_loss_high, zero);
	float64x2_t value_or_mantissa_loss_high = vreinterpretq_f64_u64(vorrq_u64(vreinterpretq_u64_f64(mValue.val[1]), mantissa_loss));
	float64x2_t value_high = vbslq_f64(is_zero_high, mValue.val[1], value_or_mantissa_loss_high);
	return DVec3({ value_low, value_high });
#else
	uint64 ux = BitCast<uint64>(mF64[0]);
	uint64 uy = BitCast<uint64>(mF64[1]);
	uint64 uz = BitCast<uint64>(mF64[2]);

	double x = BitCast<double>((ux & cDoubleToFloatMantissaLoss) == 0? ux : (ux | cDoubleToFloatMantissaLoss));
	double y = BitCast<double>((uy & cDoubleToFloatMantissaLoss) == 0? uy : (uy | cDoubleToFloatMantissaLoss));
	double z = BitCast<double>((uz & cDoubleToFloatMantissaLoss) == 0? uz : (uz | cDoubleToFloatMantissaLoss));

	return DVec3(x, y, z);
#endif
}

Vec3 DVec3::ToVec3RoundDown() const
{
	DVec3 to_zero = PrepareRoundToZero();
	DVec3 to_inf = PrepareRoundToInf();
	return Vec3(DVec3::sSelect(to_zero, to_inf, DVec3::sLess(*this, DVec3::sZero())));
}

Vec3 DVec3::ToVec3RoundUp() const
{
	DVec3 to_zero = PrepareRoundToZero();
	DVec3 to_inf = PrepareRoundToInf();
	return Vec3(DVec3::sSelect(to_inf, to_zero, DVec3::sLess(*this, DVec3::sZero())));
}

JPH_NAMESPACE_END

// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#include <Jolt/Math/Trigonometry.h>
#include <Jolt/Math/Vec3.h>
#include <Jolt/Math/UVec4.h>

JPH_NAMESPACE_BEGIN

// Constructor
Vec4::Vec4(Vec3Arg inRHS) :
	mValue(inRHS.mValue)
{
}

Vec4::Vec4(Vec3Arg inRHS, float inW)
{
#if defined(JPH_USE_SSE4_1)
	mValue = _mm_blend_ps(inRHS.mValue, _mm_set1_ps(inW), 8);
#elif defined(JPH_USE_NEON)
	mValue = vsetq_lane_f32(inW, inRHS.mValue, 3);
#else
	for (int i = 0; i < 3; i++)
		mF32[i] = inRHS.mF32[i];
	mF32[3] = inW;
#endif
}

Vec4::Vec4(float inX, float inY, float inZ, float inW)
{
#if defined(JPH_USE_SSE)
	mValue = _mm_set_ps(inW, inZ, inY, inX);
#elif defined(JPH_USE_NEON)
	uint32x2_t xy = vcreate_u32(static_cast<uint64>(BitCast<uint32>(inX)) | (static_cast<uint64>(BitCast<uint32>(inY)) << 32));
	uint32x2_t zw = vcreate_u32(static_cast<uint64>(BitCast<uint32>(inZ)) | (static_cast<uint64>(BitCast<uint32>(inW)) << 32));
	mValue = vreinterpretq_f32_u32(vcombine_u32(xy, zw));
#else
	mF32[0] = inX;
	mF32[1] = inY;
	mF32[2] = inZ;
	mF32[3] = inW;
#endif
}

template<uint32 SwizzleX, uint32 SwizzleY, uint32 SwizzleZ, uint32 SwizzleW>
Vec4 Vec4::Swizzle() const
{
	static_assert(SwizzleX <= 3, "SwizzleX template parameter out of range");
	static_assert(SwizzleY <= 3, "SwizzleY template parameter out of range");
	static_assert(SwizzleZ <= 3, "SwizzleZ template parameter out of range");
	static_assert(SwizzleW <= 3, "SwizzleW template parameter out of range");

#if defined(JPH_USE_SSE)
	return _mm_shuffle_ps(mValue, mValue, _MM_SHUFFLE(SwizzleW, SwizzleZ, SwizzleY, SwizzleX));
#elif defined(JPH_USE_NEON)
	return JPH_NEON_SHUFFLE_F32x4(mValue, mValue, SwizzleX, SwizzleY, SwizzleZ, SwizzleW);
#else
	return Vec4(mF32[SwizzleX], mF32[SwizzleY], mF32[SwizzleZ], mF32[SwizzleW]);
#endif
}

Vec4 Vec4::sZero()
{
#if defined(JPH_USE_SSE)
	return _mm_setzero_ps();
#elif defined(JPH_USE_NEON)
	return vdupq_n_f32(0);
#else
	return Vec4(0, 0, 0, 0);
#endif
}

Vec4 Vec4::sReplicate(float inV)
{
#if defined(JPH_USE_SSE)
	return _mm_set1_ps(inV);
#elif defined(JPH_USE_NEON)
	return vdupq_n_f32(inV);
#else
	return Vec4(inV, inV, inV, inV);
#endif
}

Vec4 Vec4::sNaN()
{
	return sReplicate(numeric_limits<float>::quiet_NaN());
}

Vec4 Vec4::sLoadFloat4(const Float4 *inV)
{
#if defined(JPH_USE_SSE)
	return _mm_loadu_ps(&inV->x);
#elif defined(JPH_USE_NEON)
	return vld1q_f32(&inV->x);
#else
	return Vec4(inV->x, inV->y, inV->z, inV->w);
#endif
}

Vec4 Vec4::sLoadFloat4Aligned(const Float4 *inV)
{
#if defined(JPH_USE_SSE)
	return _mm_load_ps(&inV->x);
#elif defined(JPH_USE_NEON)
	return vld1q_f32(&inV->x);
#else
	return Vec4(inV->x, inV->y, inV->z, inV->w);
#endif
}

template <const int Scale>
Vec4 Vec4::sGatherFloat4(const float *inBase, UVec4Arg inOffsets)
{
#if defined(JPH_USE_SSE)
	#ifdef JPH_USE_AVX2
		return _mm_i32gather_ps(inBase, inOffsets.mValue, Scale);
	#else
		const uint8 *base = reinterpret_cast<const uint8 *>(inBase);
		Type x = _mm_load_ss(reinterpret_cast<const float *>(base + inOffsets.GetX() * Scale));
		Type y = _mm_load_ss(reinterpret_cast<const float *>(base + inOffsets.GetY() * Scale));
		Type xy = _mm_unpacklo_ps(x, y);
		Type z = _mm_load_ss(reinterpret_cast<const float *>(base + inOffsets.GetZ() * Scale));
		Type w = _mm_load_ss(reinterpret_cast<const float *>(base + inOffsets.GetW() * Scale));
		Type zw = _mm_unpacklo_ps(z, w);
		return _mm_movelh_ps(xy, zw);
	#endif
#else
	const uint8 *base = reinterpret_cast<const uint8 *>(inBase);
	float x = *reinterpret_cast<const float *>(base + inOffsets.GetX() * Scale);
	float y = *reinterpret_cast<const float *>(base + inOffsets.GetY() * Scale);
	float z = *reinterpret_cast<const float *>(base + inOffsets.GetZ() * Scale);
	float w = *reinterpret_cast<const float *>(base + inOffsets.GetW() * Scale);
	return Vec4(x, y, z, w);
#endif
}

Vec4 Vec4::sMin(Vec4Arg inV1, Vec4Arg inV2)
{
#if defined(JPH_USE_SSE)
	return _mm_min_ps(inV1.mValue, inV2.mValue);
#elif defined(JPH_USE_NEON)
	return vminq_f32(inV1.mValue, inV2.mValue);
#else
	return Vec4(min(inV1.mF32[0], inV2.mF32[0]),
				min(inV1.mF32[1], inV2.mF32[1]),
				min(inV1.mF32[2], inV2.mF32[2]),
				min(inV1.mF32[3], inV2.mF32[3]));
#endif
}

Vec4 Vec4::sMax(Vec4Arg inV1, Vec4Arg inV2)
{
#if defined(JPH_USE_SSE)
	return _mm_max_ps(inV1.mValue, inV2.mValue);
#elif defined(JPH_USE_NEON)
	return vmaxq_f32(inV1.mValue, inV2.mValue);
#else
	return Vec4(max(inV1.mF32[0], inV2.mF32[0]),
				max(inV1.mF32[1], inV2.mF32[1]),
				max(inV1.mF32[2], inV2.mF32[2]),
				max(inV1.mF32[3], inV2.mF32[3]));
#endif
}

UVec4 Vec4::sEquals(Vec4Arg inV1, Vec4Arg inV2)
{
#if defined(JPH_USE_SSE)
	return _mm_castps_si128(_mm_cmpeq_ps(inV1.mValue, inV2.mValue));
#elif defined(JPH_USE_NEON)
	return vceqq_f32(inV1.mValue, inV2.mValue);
#else
	return UVec4(inV1.mF32[0] == inV2.mF32[0]? 0xffffffffu : 0,
				 inV1.mF32[1] == inV2.mF32[1]? 0xffffffffu : 0,
				 inV1.mF32[2] == inV2.mF32[2]? 0xffffffffu : 0,
				 inV1.mF32[3] == inV2.mF32[3]? 0xffffffffu : 0);
#endif
}

UVec4 Vec4::sLess(Vec4Arg inV1, Vec4Arg inV2)
{
#if defined(JPH_USE_SSE)
	return _mm_castps_si128(_mm_cmplt_ps(inV1.mValue, inV2.mValue));
#elif defined(JPH_USE_NEON)
	return vcltq_f32(inV1.mValue, inV2.mValue);
#else
	return UVec4(inV1.mF32[0] < inV2.mF32[0]? 0xffffffffu : 0,
				 inV1.mF32[1] < inV2.mF32[1]? 0xffffffffu : 0,
				 inV1.mF32[2] < inV2.mF32[2]? 0xffffffffu : 0,
				 inV1.mF32[3] < inV2.mF32[3]? 0xffffffffu : 0);
#endif
}

UVec4 Vec4::sLessOrEqual(Vec4Arg inV1, Vec4Arg inV2)
{
#if defined(JPH_USE_SSE)
	return _mm_castps_si128(_mm_cmple_ps(inV1.mValue, inV2.mValue));
#elif defined(JPH_USE_NEON)
	return vcleq_f32(inV1.mValue, inV2.mValue);
#else
	return UVec4(inV1.mF32[0] <= inV2.mF32[0]? 0xffffffffu : 0,
				 inV1.mF32[1] <= inV2.mF32[1]? 0xffffffffu : 0,
				 inV1.mF32[2] <= inV2.mF32[2]? 0xffffffffu : 0,
				 inV1.mF32[3] <= inV2.mF32[3]? 0xffffffffu : 0);
#endif
}

UVec4 Vec4::sGreater(Vec4Arg inV1, Vec4Arg inV2)
{
#if defined(JPH_USE_SSE)
	return _mm_castps_si128(_mm_cmpgt_ps(inV1.mValue, inV2.mValue));
#elif defined(JPH_USE_NEON)
	return vcgtq_f32(inV1.mValue, inV2.mValue);
#else
	return UVec4(inV1.mF32[0] > inV2.mF32[0]? 0xffffffffu : 0,
				 inV1.mF32[1] > inV2.mF32[1]? 0xffffffffu : 0,
				 inV1.mF32[2] > inV2.mF32[2]? 0xffffffffu : 0,
				 inV1.mF32[3] > inV2.mF32[3]? 0xffffffffu : 0);
#endif
}

UVec4 Vec4::sGreaterOrEqual(Vec4Arg inV1, Vec4Arg inV2)
{
#if defined(JPH_USE_SSE)
	return _mm_castps_si128(_mm_cmpge_ps(inV1.mValue, inV2.mValue));
#elif defined(JPH_USE_NEON)
	return vcgeq_f32(inV1.mValue, inV2.mValue);
#else
	return UVec4(inV1.mF32[0] >= inV2.mF32[0]? 0xffffffffu : 0,
				 inV1.mF32[1] >= inV2.mF32[1]? 0xffffffffu : 0,
				 inV1.mF32[2] >= inV2.mF32[2]? 0xffffffffu : 0,
				 inV1.mF32[3] >= inV2.mF32[3]? 0xffffffffu : 0);
#endif
}

Vec4 Vec4::sFusedMultiplyAdd(Vec4Arg inMul1, Vec4Arg inMul2, Vec4Arg inAdd)
{
#if defined(JPH_USE_SSE)
	#ifdef JPH_USE_FMADD
		return _mm_fmadd_ps(inMul1.mValue, inMul2.mValue, inAdd.mValue);
	#else
		return _mm_add_ps(_mm_mul_ps(inMul1.mValue, inMul2.mValue), inAdd.mValue);
	#endif
#elif defined(JPH_USE_NEON)
	return vmlaq_f32(inAdd.mValue, inMul1.mValue, inMul2.mValue);
#else
	return Vec4(inMul1.mF32[0] * inMul2.mF32[0] + inAdd.mF32[0],
				inMul1.mF32[1] * inMul2.mF32[1] + inAdd.mF32[1],
				inMul1.mF32[2] * inMul2.mF32[2] + inAdd.mF32[2],
				inMul1.mF32[3] * inMul2.mF32[3] + inAdd.mF32[3]);
#endif
}

Vec4 Vec4::sSelect(Vec4Arg inNotSet, Vec4Arg inSet, UVec4Arg inControl)
{
#if defined(JPH_USE_SSE4_1) && !defined(JPH_PLATFORM_WASM) // _mm_blendv_ps has problems on FireFox
	return _mm_blendv_ps(inNotSet.mValue, inSet.mValue, _mm_castsi128_ps(inControl.mValue));
#elif defined(JPH_USE_SSE)
	__m128 is_set = _mm_castsi128_ps(_mm_srai_epi32(inControl.mValue, 31));
	return _mm_or_ps(_mm_and_ps(is_set, inSet.mValue), _mm_andnot_ps(is_set, inNotSet.mValue));
#elif defined(JPH_USE_NEON)
	return vbslq_f32(vreinterpretq_u32_s32(vshrq_n_s32(vreinterpretq_s32_u32(inControl.mValue), 31)), inSet.mValue, inNotSet.mValue);
#else
	Vec4 result;
	for (int i = 0; i < 4; i++)
		result.mF32[i] = (inControl.mU32[i] & 0x80000000u) ? inSet.mF32[i] : inNotSet.mF32[i];
	return result;
#endif
}

Vec4 Vec4::sOr(Vec4Arg inV1, Vec4Arg inV2)
{
#if defined(JPH_USE_SSE)
	return _mm_or_ps(inV1.mValue, inV2.mValue);
#elif defined(JPH_USE_NEON)
	return vreinterpretq_f32_u32(vorrq_u32(vreinterpretq_u32_f32(inV1.mValue), vreinterpretq_u32_f32(inV2.mValue)));
#else
	return UVec4::sOr(inV1.ReinterpretAsInt(), inV2.ReinterpretAsInt()).ReinterpretAsFloat();
#endif
}

Vec4 Vec4::sXor(Vec4Arg inV1, Vec4Arg inV2)
{
#if defined(JPH_USE_SSE)
	return _mm_xor_ps(inV1.mValue, inV2.mValue);
#elif defined(JPH_USE_NEON)
	return vreinterpretq_f32_u32(veorq_u32(vreinterpretq_u32_f32(inV1.mValue), vreinterpretq_u32_f32(inV2.mValue)));
#else
	return UVec4::sXor(inV1.ReinterpretAsInt(), inV2.ReinterpretAsInt()).ReinterpretAsFloat();
#endif
}

Vec4 Vec4::sAnd(Vec4Arg inV1, Vec4Arg inV2)
{
#if defined(JPH_USE_SSE)
	return _mm_and_ps(inV1.mValue, inV2.mValue);
#elif defined(JPH_USE_NEON)
	return vreinterpretq_f32_u32(vandq_u32(vreinterpretq_u32_f32(inV1.mValue), vreinterpretq_u32_f32(inV2.mValue)));
#else
	return UVec4::sAnd(inV1.ReinterpretAsInt(), inV2.ReinterpretAsInt()).ReinterpretAsFloat();
#endif
}

void Vec4::sSort4(Vec4 &ioValue, UVec4 &ioIndex)
{
	// Pass 1, test 1st vs 3rd, 2nd vs 4th
	Vec4 v1 = ioValue.Swizzle<SWIZZLE_Z, SWIZZLE_W, SWIZZLE_X, SWIZZLE_Y>();
	UVec4 i1 = ioIndex.Swizzle<SWIZZLE_Z, SWIZZLE_W, SWIZZLE_X, SWIZZLE_Y>();
	UVec4 c1 = sLess(ioValue, v1).Swizzle<SWIZZLE_Z, SWIZZLE_W, SWIZZLE_Z, SWIZZLE_W>();
	ioValue = sSelect(ioValue, v1, c1);
	ioIndex = UVec4::sSelect(ioIndex, i1, c1);

	// Pass 2, test 1st vs 2nd, 3rd vs 4th
	Vec4 v2 = ioValue.Swizzle<SWIZZLE_Y, SWIZZLE_X, SWIZZLE_W, SWIZZLE_Z>();
	UVec4 i2 = ioIndex.Swizzle<SWIZZLE_Y, SWIZZLE_X, SWIZZLE_W, SWIZZLE_Z>();
	UVec4 c2 = sLess(ioValue, v2).Swizzle<SWIZZLE_Y, SWIZZLE_Y, SWIZZLE_W, SWIZZLE_W>();
	ioValue = sSelect(ioValue, v2, c2);
	ioIndex = UVec4::sSelect(ioIndex, i2, c2);

	// Pass 3, test 2nd vs 3rd component
	Vec4 v3 = ioValue.Swizzle<SWIZZLE_X, SWIZZLE_Z, SWIZZLE_Y, SWIZZLE_W>();
	UVec4 i3 = ioIndex.Swizzle<SWIZZLE_X, SWIZZLE_Z, SWIZZLE_Y, SWIZZLE_W>();
	UVec4 c3 = sLess(ioValue, v3).Swizzle<SWIZZLE_X, SWIZZLE_Z, SWIZZLE_Z, SWIZZLE_W>();
	ioValue = sSelect(ioValue, v3, c3);
	ioIndex = UVec4::sSelect(ioIndex, i3, c3);
}

void Vec4::sSort4Reverse(Vec4 &ioValue, UVec4 &ioIndex)
{
	// Pass 1, test 1st vs 3rd, 2nd vs 4th
	Vec4 v1 = ioValue.Swizzle<SWIZZLE_Z, SWIZZLE_W, SWIZZLE_X, SWIZZLE_Y>();
	UVec4 i1 = ioIndex.Swizzle<SWIZZLE_Z, SWIZZLE_W, SWIZZLE_X, SWIZZLE_Y>();
	UVec4 c1 = sGreater(ioValue, v1).Swizzle<SWIZZLE_Z, SWIZZLE_W, SWIZZLE_Z, SWIZZLE_W>();
	ioValue = sSelect(ioValue, v1, c1);
	ioIndex = UVec4::sSelect(ioIndex, i1, c1);

	// Pass 2, test 1st vs 2nd, 3rd vs 4th
	Vec4 v2 = ioValue.Swizzle<SWIZZLE_Y, SWIZZLE_X, SWIZZLE_W, SWIZZLE_Z>();
	UVec4 i2 = ioIndex.Swizzle<SWIZZLE_Y, SWIZZLE_X, SWIZZLE_W, SWIZZLE_Z>();
	UVec4 c2 = sGreater(ioValue, v2).Swizzle<SWIZZLE_Y, SWIZZLE_Y, SWIZZLE_W, SWIZZLE_W>();
	ioValue = sSelect(ioValue, v2, c2);
	ioIndex = UVec4::sSelect(ioIndex, i2, c2);

	// Pass 3, test 2nd vs 3rd component
	Vec4 v3 = ioValue.Swizzle<SWIZZLE_X, SWIZZLE_Z, SWIZZLE_Y, SWIZZLE_W>();
	UVec4 i3 = ioIndex.Swizzle<SWIZZLE_X, SWIZZLE_Z, SWIZZLE_Y, SWIZZLE_W>();
	UVec4 c3 = sGreater(ioValue, v3).Swizzle<SWIZZLE_X, SWIZZLE_Z, SWIZZLE_Z, SWIZZLE_W>();
	ioValue = sSelect(ioValue, v3, c3);
	ioIndex = UVec4::sSelect(ioIndex, i3, c3);
}

bool Vec4::operator == (Vec4Arg inV2) const
{
	return sEquals(*this, inV2).TestAllTrue();
}

bool Vec4::IsClose(Vec4Arg inV2, float inMaxDistSq) const
{
	return (inV2 - *this).LengthSq() <= inMaxDistSq;
}

bool Vec4::IsNormalized(float inTolerance) const
{
	return abs(LengthSq() - 1.0f) <= inTolerance;
}

bool Vec4::IsNaN() const
{
#if defined(JPH_USE_AVX512)
	return _mm_fpclass_ps_mask(mValue, 0b10000001) != 0;
#elif defined(JPH_USE_SSE)
	return _mm_movemask_ps(_mm_cmpunord_ps(mValue, mValue)) != 0;
#elif defined(JPH_USE_NEON)
	uint32x4_t is_equal = vceqq_f32(mValue, mValue); // If a number is not equal to itself it's a NaN
	return vaddvq_u32(vshrq_n_u32(is_equal, 31)) != 4;
#else
	return isnan(mF32[0]) || isnan(mF32[1]) || isnan(mF32[2]) || isnan(mF32[3]);
#endif
}

Vec4 Vec4::operator * (Vec4Arg inV2) const
{
#if defined(JPH_USE_SSE)
	return _mm_mul_ps(mValue, inV2.mValue);
#elif defined(JPH_USE_NEON)
	return vmulq_f32(mValue, inV2.mValue);
#else
	return Vec4(mF32[0] * inV2.mF32[0],
				mF32[1] * inV2.mF32[1],
				mF32[2] * inV2.mF32[2],
				mF32[3] * inV2.mF32[3]);
#endif
}

Vec4 Vec4::operator * (float inV2) const
{
#if defined(JPH_USE_SSE)
	return _mm_mul_ps(mValue, _mm_set1_ps(inV2));
#elif defined(JPH_USE_NEON)
	return vmulq_n_f32(mValue, inV2);
#else
	return Vec4(mF32[0] * inV2, mF32[1] * inV2, mF32[2] * inV2, mF32[3] * inV2);
#endif
}

/// Multiply vector with float
Vec4 operator * (float inV1, Vec4Arg inV2)
{
#if defined(JPH_USE_SSE)
	return _mm_mul_ps(_mm_set1_ps(inV1), inV2.mValue);
#elif defined(JPH_USE_NEON)
	return vmulq_n_f32(inV2.mValue, inV1);
#else
	return Vec4(inV1 * inV2.mF32[0],
				inV1 * inV2.mF32[1],
				inV1 * inV2.mF32[2],
				inV1 * inV2.mF32[3]);
#endif
}

Vec4 Vec4::operator / (float inV2) const
{
#if defined(JPH_USE_SSE)
	return _mm_div_ps(mValue, _mm_set1_ps(inV2));
#elif defined(JPH_USE_NEON)
	return vdivq_f32(mValue, vdupq_n_f32(inV2));
#else
	return Vec4(mF32[0] / inV2, mF32[1] / inV2, mF32[2] / inV2, mF32[3] / inV2);
#endif
}

Vec4 &Vec4::operator *= (float inV2)
{
#if defined(JPH_USE_SSE)
	mValue = _mm_mul_ps(mValue, _mm_set1_ps(inV2));
#elif defined(JPH_USE_NEON)
	mValue = vmulq_n_f32(mValue, inV2);
#else
	for (int i = 0; i < 4; ++i)
		mF32[i] *= inV2;
#endif
	return *this;
}

Vec4 &Vec4::operator *= (Vec4Arg inV2)
{
#if defined(JPH_USE_SSE)
	mValue = _mm_mul_ps(mValue, inV2.mValue);
#elif defined(JPH_USE_NEON)
	mValue = vmulq_f32(mValue, inV2.mValue);
#else
	for (int i = 0; i < 4; ++i)
		mF32[i] *= inV2.mF32[i];
#endif
	return *this;
}

Vec4 &Vec4::operator /= (float inV2)
{
#if defined(JPH_USE_SSE)
	mValue = _mm_div_ps(mValue, _mm_set1_ps(inV2));
#elif defined(JPH_USE_NEON)
	mValue = vdivq_f32(mValue, vdupq_n_f32(inV2));
#else
	for (int i = 0; i < 4; ++i)
		mF32[i] /= inV2;
#endif
	return *this;
}

Vec4 Vec4::operator + (Vec4Arg inV2) const
{
#if defined(JPH_USE_SSE)
	return _mm_add_ps(mValue, inV2.mValue);
#elif defined(JPH_USE_NEON)
	return vaddq_f32(mValue, inV2.mValue);
#else
	return Vec4(mF32[0] + inV2.mF32[0],
				mF32[1] + inV2.mF32[1],
				mF32[2] + inV2.mF32[2],
				mF32[3] + inV2.mF32[3]);
#endif
}

Vec4 &Vec4::operator += (Vec4Arg inV2)
{
#if defined(JPH_USE_SSE)
	mValue = _mm_add_ps(mValue, inV2.mValue);
#elif defined(JPH_USE_NEON)
	mValue = vaddq_f32(mValue, inV2.mValue);
#else
	for (int i = 0; i < 4; ++i)
		mF32[i] += inV2.mF32[i];
#endif
	return *this;
}

Vec4 Vec4::operator - () const
{
#if defined(JPH_USE_SSE)
	return _mm_sub_ps(_mm_setzero_ps(), mValue);
#elif defined(JPH_USE_NEON)
	#ifdef JPH_CROSS_PLATFORM_DETERMINISTIC
		return vsubq_f32(vdupq_n_f32(0), mValue);
	#else
		return vnegq_f32(mValue);
	#endif
#else
	#ifdef JPH_CROSS_PLATFORM_DETERMINISTIC
		return Vec4(0.0f - mF32[0], 0.0f - mF32[1], 0.0f - mF32[2], 0.0f - mF32[3]);
	#else
		return Vec4(-mF32[0], -mF32[1], -mF32[2], -mF32[3]);
	#endif
#endif
}

Vec4 Vec4::operator - (Vec4Arg inV2) const
{
#if defined(JPH_USE_SSE)
	return _mm_sub_ps(mValue, inV2.mValue);
#elif defined(JPH_USE_NEON)
	return vsubq_f32(mValue, inV2.mValue);
#else
	return Vec4(mF32[0] - inV2.mF32[0],
				mF32[1] - inV2.mF32[1],
				mF32[2] - inV2.mF32[2],
				mF32[3] - inV2.mF32[3]);
#endif
}

Vec4 &Vec4::operator -= (Vec4Arg inV2)
{
#if defined(JPH_USE_SSE)
	mValue = _mm_sub_ps(mValue, inV2.mValue);
#elif defined(JPH_USE_NEON)
	mValue = vsubq_f32(mValue, inV2.mValue);
#else
	for (int i = 0; i < 4; ++i)
		mF32[i] -= inV2.mF32[i];
#endif
	return *this;
}

Vec4 Vec4::operator / (Vec4Arg inV2) const
{
#if defined(JPH_USE_SSE)
	return _mm_div_ps(mValue, inV2.mValue);
#elif defined(JPH_USE_NEON)
	return vdivq_f32(mValue, inV2.mValue);
#else
	return Vec4(mF32[0] / inV2.mF32[0],
				mF32[1] / inV2.mF32[1],
				mF32[2] / inV2.mF32[2],
				mF32[3] / inV2.mF32[3]);
#endif
}

Vec4 Vec4::SplatX() const
{
#if defined(JPH_USE_SSE)
	return _mm_shuffle_ps(mValue, mValue, _MM_SHUFFLE(0, 0, 0, 0));
#elif defined(JPH_USE_NEON)
	return vdupq_laneq_f32(mValue, 0);
#else
	return Vec4(mF32[0], mF32[0], mF32[0], mF32[0]);
#endif
}

Vec4 Vec4::SplatY() const
{
#if defined(JPH_USE_SSE)
	return _mm_shuffle_ps(mValue, mValue, _MM_SHUFFLE(1, 1, 1, 1));
#elif defined(JPH_USE_NEON)
	return vdupq_laneq_f32(mValue, 1);
#else
	return Vec4(mF32[1], mF32[1], mF32[1], mF32[1]);
#endif
}

Vec4 Vec4::SplatZ() const
{
#if defined(JPH_USE_SSE)
	return _mm_shuffle_ps(mValue, mValue, _MM_SHUFFLE(2, 2, 2, 2));
#elif defined(JPH_USE_NEON)
	return vdupq_laneq_f32(mValue, 2);
#else
	return Vec4(mF32[2], mF32[2], mF32[2], mF32[2]);
#endif
}

Vec4 Vec4::SplatW() const
{
#if defined(JPH_USE_SSE)
	return _mm_shuffle_ps(mValue, mValue, _MM_SHUFFLE(3, 3, 3, 3));
#elif defined(JPH_USE_NEON)
	return vdupq_laneq_f32(mValue, 3);
#else
	return Vec4(mF32[3], mF32[3], mF32[3], mF32[3]);
#endif
}

Vec4 Vec4::Abs() const
{
#if defined(JPH_USE_AVX512)
	return _mm_range_ps(mValue, mValue, 0b1000);
#elif defined(JPH_USE_SSE)
	return _mm_max_ps(_mm_sub_ps(_mm_setzero_ps(), mValue), mValue);
#elif defined(JPH_USE_NEON)
	return vabsq_f32(mValue);
#else
	return Vec4(abs(mF32[0]), abs(mF32[1]), abs(mF32[2]), abs(mF32[3]));
#endif
}

Vec4 Vec4::Reciprocal() const
{
	return sReplicate(1.0f) / mValue;
}

Vec4 Vec4::DotV(Vec4Arg inV2) const
{
#if defined(JPH_USE_SSE4_1)
	return _mm_dp_ps(mValue, inV2.mValue, 0xff);
#elif defined(JPH_USE_NEON)
	float32x4_t mul = vmulq_f32(mValue, inV2.mValue);
	return vdupq_n_f32(vaddvq_f32(mul));
#else
	// Brackets placed so that the order is consistent with the vectorized version
	return Vec4::sReplicate((mF32[0] * inV2.mF32[0] + mF32[1] * inV2.mF32[1]) + (mF32[2] * inV2.mF32[2] + mF32[3] * inV2.mF32[3]));
#endif
}

float Vec4::Dot(Vec4Arg inV2) const
{
#if defined(JPH_USE_SSE4_1)
	return _mm_cvtss_f32(_mm_dp_ps(mValue, inV2.mValue, 0xff));
#elif defined(JPH_USE_NEON)
	float32x4_t mul = vmulq_f32(mValue, inV2.mValue);
	return vaddvq_f32(mul);
#else
	// Brackets placed so that the order is consistent with the vectorized version
	return (mF32[0] * inV2.mF32[0] + mF32[1] * inV2.mF32[1]) + (mF32[2] * inV2.mF32[2] + mF32[3] * inV2.mF32[3]);
#endif
}

float Vec4::LengthSq() const
{
#if defined(JPH_USE_SSE4_1)
	return _mm_cvtss_f32(_mm_dp_ps(mValue, mValue, 0xff));
#elif defined(JPH_USE_NEON)
	float32x4_t mul = vmulq_f32(mValue, mValue);
	return vaddvq_f32(mul);
#else
	// Brackets placed so that the order is consistent with the vectorized version
	return (mF32[0] * mF32[0] + mF32[1] * mF32[1]) + (mF32[2] * mF32[2] + mF32[3] * mF32[3]);
#endif
}

float Vec4::Length() const
{
#if defined(JPH_USE_SSE4_1)
	return _mm_cvtss_f32(_mm_sqrt_ss(_mm_dp_ps(mValue, mValue, 0xff)));
#elif defined(JPH_USE_NEON)
	float32x4_t mul = vmulq_f32(mValue, mValue);
	float32x2_t sum = vdup_n_f32(vaddvq_f32(mul));
	return vget_lane_f32(vsqrt_f32(sum), 0);
#else
	// Brackets placed so that the order is consistent with the vectorized version
	return sqrt((mF32[0] * mF32[0] + mF32[1] * mF32[1]) + (mF32[2] * mF32[2] + mF32[3] * mF32[3]));
#endif
}

Vec4 Vec4::Sqrt() const
{
#if defined(JPH_USE_SSE)
	return _mm_sqrt_ps(mValue);
#elif defined(JPH_USE_NEON)
	return vsqrtq_f32(mValue);
#else
	return Vec4(sqrt(mF32[0]), sqrt(mF32[1]), sqrt(mF32[2]), sqrt(mF32[3]));
#endif
}


Vec4 Vec4::GetSign() const
{
#if defined(JPH_USE_AVX512)
	return _mm_fixupimm_ps(mValue, mValue, _mm_set1_epi32(0xA9A90A00), 0);
#elif defined(JPH_USE_SSE)
	Type minus_one = _mm_set1_ps(-1.0f);
	Type one = _mm_set1_ps(1.0f);
	return _mm_or_ps(_mm_and_ps(mValue, minus_one), one);
#elif defined(JPH_USE_NEON)
	Type minus_one = vdupq_n_f32(-1.0f);
	Type one = vdupq_n_f32(1.0f);
	return vreinterpretq_f32_u32(vorrq_u32(vandq_u32(vreinterpretq_u32_f32(mValue), vreinterpretq_u32_f32(minus_one)), vreinterpretq_u32_f32(one)));
#else
	return Vec4(std::signbit(mF32[0])? -1.0f : 1.0f,
				std::signbit(mF32[1])? -1.0f : 1.0f,
				std::signbit(mF32[2])? -1.0f : 1.0f,
				std::signbit(mF32[3])? -1.0f : 1.0f);
#endif
}

Vec4 Vec4::Normalized() const
{
#if defined(JPH_USE_SSE4_1)
	return _mm_div_ps(mValue, _mm_sqrt_ps(_mm_dp_ps(mValue, mValue, 0xff)));
#elif defined(JPH_USE_NEON)
	float32x4_t mul = vmulq_f32(mValue, mValue);
	float32x4_t sum = vdupq_n_f32(vaddvq_f32(mul));
	return vdivq_f32(mValue, vsqrtq_f32(sum));
#else
	return *this / Length();
#endif
}

void Vec4::StoreFloat4(Float4 *outV) const
{
#if defined(JPH_USE_SSE)
	_mm_storeu_ps(&outV->x, mValue);
#elif defined(JPH_USE_NEON)
	vst1q_f32(&outV->x, mValue);
#else
	for (int i = 0; i < 4; ++i)
		(&outV->x)[i] = mF32[i];
#endif
}

UVec4 Vec4::ToInt() const
{
#if defined(JPH_USE_SSE)
	return _mm_cvttps_epi32(mValue);
#elif defined(JPH_USE_NEON)
	return vcvtq_u32_f32(mValue);
#else
	return UVec4(uint32(mF32[0]), uint32(mF32[1]), uint32(mF32[2]), uint32(mF32[3]));
#endif
}

UVec4 Vec4::ReinterpretAsInt() const
{
#if defined(JPH_USE_SSE)
	return UVec4(_mm_castps_si128(mValue));
#elif defined(JPH_USE_NEON)
	return vreinterpretq_u32_f32(mValue);
#else
	return *reinterpret_cast<const UVec4 *>(this);
#endif
}

int Vec4::GetSignBits() const
{
#if defined(JPH_USE_SSE)
	return _mm_movemask_ps(mValue);
#elif defined(JPH_USE_NEON)
	int32x4_t shift = JPH_NEON_INT32x4(0, 1, 2, 3);
	return vaddvq_u32(vshlq_u32(vshrq_n_u32(vreinterpretq_u32_f32(mValue), 31), shift));
#else
	return (std::signbit(mF32[0])? 1 : 0) | (std::signbit(mF32[1])? 2 : 0) | (std::signbit(mF32[2])? 4 : 0) | (std::signbit(mF32[3])? 8 : 0);
#endif
}

float Vec4::ReduceMin() const
{
	Vec4 v = sMin(mValue, Swizzle<SWIZZLE_Y, SWIZZLE_UNUSED, SWIZZLE_W, SWIZZLE_UNUSED>());
	v = sMin(v, v.Swizzle<SWIZZLE_Z, SWIZZLE_UNUSED, SWIZZLE_UNUSED, SWIZZLE_UNUSED>());
	return v.GetX();
}

float Vec4::ReduceMax() const
{
	Vec4 v = sMax(mValue, Swizzle<SWIZZLE_Y, SWIZZLE_UNUSED, SWIZZLE_W, SWIZZLE_UNUSED>());
	v = sMax(v, v.Swizzle<SWIZZLE_Z, SWIZZLE_UNUSED, SWIZZLE_UNUSED, SWIZZLE_UNUSED>());
	return v.GetX();
}

void Vec4::SinCos(Vec4 &outSin, Vec4 &outCos) const
{
	// Implementation based on sinf.c from the cephes library, combines sinf and cosf in a single function, changes octants to quadrants and vectorizes it
	// Original implementation by Stephen L. Moshier (See: http://www.moshier.net/)

	// Make argument positive and remember sign for sin only since cos is symmetric around x (highest bit of a float is the sign bit)
	UVec4 sin_sign = UVec4::sAnd(ReinterpretAsInt(), UVec4::sReplicate(0x80000000U));
	Vec4 x = Vec4::sXor(*this, sin_sign.ReinterpretAsFloat());

	// x / (PI / 2) rounded to nearest int gives us the quadrant closest to x
	UVec4 quadrant = (0.6366197723675814f * x + Vec4::sReplicate(0.5f)).ToInt();

	// Make x relative to the closest quadrant.
	// This does x = x - quadrant * PI / 2 using a two step Cody-Waite argument reduction.
	// This improves the accuracy of the result by avoiding loss of significant bits in the subtraction.
	// We start with x = x - quadrant * PI / 2, PI / 2 in hexadecimal notation is 0x3fc90fdb, we remove the lowest 16 bits to
	// get 0x3fc90000 (= 1.5703125) this means we can now multiply with a number of up to 2^16 without losing any bits.
	// This leaves us with: x = (x - quadrant * 1.5703125) - quadrant * (PI / 2 - 1.5703125).
	// PI / 2 - 1.5703125 in hexadecimal is 0x39fdaa22, stripping the lowest 12 bits we get 0x39fda000 (= 0.0004837512969970703125)
	// This leaves uw with: x = ((x - quadrant * 1.5703125) - quadrant * 0.0004837512969970703125) - quadrant * (PI / 2 - 1.5703125 - 0.0004837512969970703125)
	// See: https://stackoverflow.com/questions/42455143/sine-cosine-modular-extended-precision-arithmetic
	// After this we have x in the range [-PI / 4, PI / 4].
	Vec4 float_quadrant = quadrant.ToFloat();
	x = ((x - float_quadrant * 1.5703125f) - float_quadrant * 0.0004837512969970703125f) - float_quadrant * 7.549789948768648e-8f;

	// Calculate x2 = x^2
	Vec4 x2 = x * x;

	// Taylor expansion:
	// Cos(x) = 1 - x^2/2! + x^4/4! - x^6/6! + x^8/8! + ... = (((x2/8!- 1/6!) * x2 + 1/4!) * x2 - 1/2!) * x2 + 1
	Vec4 taylor_cos = ((2.443315711809948e-5f * x2 - Vec4::sReplicate(1.388731625493765e-3f)) * x2 + Vec4::sReplicate(4.166664568298827e-2f)) * x2 * x2 - 0.5f * x2 + Vec4::sReplicate(1.0f);
	// Sin(x) = x - x^3/3! + x^5/5! - x^7/7! + ... = ((-x2/7! + 1/5!) * x2 - 1/3!) * x2 * x + x
	Vec4 taylor_sin = ((-1.9515295891e-4f * x2 + Vec4::sReplicate(8.3321608736e-3f)) * x2 - Vec4::sReplicate(1.6666654611e-1f)) * x2 * x + x;

	// The lowest 2 bits of quadrant indicate the quadrant that we are in.
	// Let x be the original input value and x' our value that has been mapped to the range [-PI / 4, PI / 4].
	// since cos(x) = sin(x - PI / 2) and since we want to use the Taylor expansion as close as possible to 0,
	// we can alternate between using the Taylor expansion for sin and cos according to the following table:
	//
	// quadrant	 sin(x)		 cos(x)
	// XXX00b	 sin(x')	 cos(x')
	// XXX01b	 cos(x')	-sin(x')
	// XXX10b	-sin(x')	-cos(x')
	// XXX11b	-cos(x')	 sin(x')
	//
	// So: sin_sign = bit2, cos_sign = bit1 ^ bit2, bit1 determines if we use sin or cos Taylor expansion
	UVec4 bit1 = quadrant.LogicalShiftLeft<31>();
	UVec4 bit2 = UVec4::sAnd(quadrant.LogicalShiftLeft<30>(), UVec4::sReplicate(0x80000000U));

	// Select which one of the results is sin and which one is cos
	Vec4 s = Vec4::sSelect(taylor_sin, taylor_cos, bit1);
	Vec4 c = Vec4::sSelect(taylor_cos, taylor_sin, bit1);

	// Update the signs
	sin_sign = UVec4::sXor(sin_sign, bit2);
	UVec4 cos_sign = UVec4::sXor(bit1, bit2);

	// Correct the signs
	outSin = Vec4::sXor(s, sin_sign.ReinterpretAsFloat());
	outCos = Vec4::sXor(c, cos_sign.ReinterpretAsFloat());
}

Vec4 Vec4::Tan() const
{
	// Implementation based on tanf.c from the cephes library, see Vec4::SinCos for further details
	// Original implementation by Stephen L. Moshier (See: http://www.moshier.net/)

	// Make argument positive
	UVec4 tan_sign = UVec4::sAnd(ReinterpretAsInt(), UVec4::sReplicate(0x80000000U));
	Vec4 x = Vec4::sXor(*this, tan_sign.ReinterpretAsFloat());

	// x / (PI / 2) rounded to nearest int gives us the quadrant closest to x
	UVec4 quadrant = (0.6366197723675814f * x + Vec4::sReplicate(0.5f)).ToInt();

	// Remap x to range [-PI / 4, PI / 4], see Vec4::SinCos
	Vec4 float_quadrant = quadrant.ToFloat();
	x = ((x - float_quadrant * 1.5703125f) - float_quadrant * 0.0004837512969970703125f) - float_quadrant * 7.549789948768648e-8f;

	// Calculate x2 = x^2
	Vec4 x2 = x * x;

	// Roughly equivalent to the Taylor expansion:
	// Tan(x) = x + x^3/3 + 2*x^5/15 + 17*x^7/315 + 62*x^9/2835 + ...
	Vec4 tan =
		(((((9.38540185543e-3f * x2 + Vec4::sReplicate(3.11992232697e-3f)) * x2 + Vec4::sReplicate(2.44301354525e-2f)) * x2
		+ Vec4::sReplicate(5.34112807005e-2f)) * x2 + Vec4::sReplicate(1.33387994085e-1f)) * x2 + Vec4::sReplicate(3.33331568548e-1f)) * x2 * x + x;

	// For the 2nd and 4th quadrant we need to invert the value
	UVec4 bit1 = quadrant.LogicalShiftLeft<31>();
	tan = Vec4::sSelect(tan, Vec4::sReplicate(-1.0f) / (tan JPH_IF_FLOATING_POINT_EXCEPTIONS_ENABLED(+ Vec4::sReplicate(FLT_MIN))), bit1); // Add small epsilon to prevent div by zero, works because tan is always positive

	// Put the sign back
	return Vec4::sXor(tan, tan_sign.ReinterpretAsFloat());
}

Vec4 Vec4::ASin() const
{
	// Implementation based on asinf.c from the cephes library
	// Original implementation by Stephen L. Moshier (See: http://www.moshier.net/)

	// Make argument positive
	UVec4 asin_sign = UVec4::sAnd(ReinterpretAsInt(), UVec4::sReplicate(0x80000000U));
	Vec4 a = Vec4::sXor(*this, asin_sign.ReinterpretAsFloat());

	// ASin is not defined outside the range [-1, 1] but it often happens that a value is slightly above 1 so we just clamp here
	a = Vec4::sMin(a, Vec4::sReplicate(1.0f));

	// When |x| <= 0.5 we use the asin approximation as is
	Vec4 z1 = a * a;
	Vec4 x1 = a;

	// When |x| > 0.5 we use the identity asin(x) = PI / 2 - 2 * asin(sqrt((1 - x) / 2))
	Vec4 z2 = 0.5f * (Vec4::sReplicate(1.0f) - a);
	Vec4 x2 = z2.Sqrt();

	// Select which of the two situations we have
	UVec4 greater = Vec4::sGreater(a, Vec4::sReplicate(0.5f));
	Vec4 z = Vec4::sSelect(z1, z2, greater);
	Vec4 x = Vec4::sSelect(x1, x2, greater);

	// Polynomial approximation of asin
	z = ((((4.2163199048e-2f * z + Vec4::sReplicate(2.4181311049e-2f)) * z + Vec4::sReplicate(4.5470025998e-2f)) * z + Vec4::sReplicate(7.4953002686e-2f)) * z + Vec4::sReplicate(1.6666752422e-1f)) * z * x + x;

	// If |x| > 0.5 we need to apply the remainder of the identity above
	z = Vec4::sSelect(z, Vec4::sReplicate(0.5f * JPH_PI) - (z + z), greater);

	// Put the sign back
	return Vec4::sXor(z, asin_sign.ReinterpretAsFloat());
}

Vec4 Vec4::ACos() const
{
	// Not the most accurate, but simple
	return Vec4::sReplicate(0.5f * JPH_PI) - ASin();
}

Vec4 Vec4::ATan() const
{
	// Implementation based on atanf.c from the cephes library
	// Original implementation by Stephen L. Moshier (See: http://www.moshier.net/)

	// Make argument positive
	UVec4 atan_sign = UVec4::sAnd(ReinterpretAsInt(), UVec4::sReplicate(0x80000000U));
	Vec4 x = Vec4::sXor(*this, atan_sign.ReinterpretAsFloat());
	Vec4 y = Vec4::sZero();

	// If x > Tan(PI / 8)
	UVec4 greater1 = Vec4::sGreater(x, Vec4::sReplicate(0.4142135623730950f));
	Vec4 x1 = (x - Vec4::sReplicate(1.0f)) / (x + Vec4::sReplicate(1.0f));

	// If x > Tan(3 * PI / 8)
	UVec4 greater2 = Vec4::sGreater(x, Vec4::sReplicate(2.414213562373095f));
	Vec4 x2 = Vec4::sReplicate(-1.0f) / (x JPH_IF_FLOATING_POINT_EXCEPTIONS_ENABLED(+ Vec4::sReplicate(FLT_MIN))); // Add small epsilon to prevent div by zero, works because x is always positive

	// Apply first if
	x = Vec4::sSelect(x, x1, greater1);
	y = Vec4::sSelect(y, Vec4::sReplicate(0.25f * JPH_PI), greater1);

	// Apply second if
	x = Vec4::sSelect(x, x2, greater2);
	y = Vec4::sSelect(y, Vec4::sReplicate(0.5f * JPH_PI), greater2);

	// Polynomial approximation
	Vec4 z = x * x;
	y += (((8.05374449538e-2f * z - Vec4::sReplicate(1.38776856032e-1f)) * z + Vec4::sReplicate(1.99777106478e-1f)) * z - Vec4::sReplicate(3.33329491539e-1f)) * z * x + x;

	// Put the sign back
	return Vec4::sXor(y, atan_sign.ReinterpretAsFloat());
}

Vec4 Vec4::sATan2(Vec4Arg inY, Vec4Arg inX)
{
	UVec4 sign_mask = UVec4::sReplicate(0x80000000U);

	// Determine absolute value and sign of y
	UVec4 y_sign = UVec4::sAnd(inY.ReinterpretAsInt(), sign_mask);
	Vec4 y_abs = Vec4::sXor(inY, y_sign.ReinterpretAsFloat());

	// Determine absolute value and sign of x
	UVec4 x_sign = UVec4::sAnd(inX.ReinterpretAsInt(), sign_mask);
	Vec4 x_abs = Vec4::sXor(inX, x_sign.ReinterpretAsFloat());

	// Always divide smallest / largest to avoid dividing by zero
	UVec4 x_is_numerator = Vec4::sLess(x_abs, y_abs);
	Vec4 numerator = Vec4::sSelect(y_abs, x_abs, x_is_numerator);
	Vec4 denominator = Vec4::sSelect(x_abs, y_abs, x_is_numerator);
	Vec4 atan = (numerator / denominator).ATan();

	// If we calculated x / y instead of y / x the result is PI / 2 - result (note that this is true because we know the result is positive because the input was positive)
	atan = Vec4::sSelect(atan, Vec4::sReplicate(0.5f * JPH_PI) - atan, x_is_numerator);

	// Now we need to map to the correct quadrant
	// x_sign	y_sign	result
	// +1		+1		atan
	// -1		+1		-atan + PI
	// -1		-1		atan - PI
	// +1		-1		-atan
	// This can be written as: x_sign * y_sign * (atan - (x_sign < 0? PI : 0))
	atan -= Vec4::sAnd(x_sign.ArithmeticShiftRight<31>().ReinterpretAsFloat(), Vec4::sReplicate(JPH_PI));
	atan = Vec4::sXor(atan, UVec4::sXor(x_sign, y_sign).ReinterpretAsFloat());
	return atan;
}

JPH_NAMESPACE_END

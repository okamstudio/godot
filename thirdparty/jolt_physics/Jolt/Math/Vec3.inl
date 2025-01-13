// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#include <Jolt/Math/Vec4.h>
#include <Jolt/Math/UVec4.h>
#include <Jolt/Core/HashCombine.h>

JPH_SUPPRESS_WARNINGS_STD_BEGIN
#include <random>
JPH_SUPPRESS_WARNINGS_STD_END

// Create a std::hash/JPH::Hash for Vec3
JPH_MAKE_HASHABLE(JPH::Vec3, t.GetX(), t.GetY(), t.GetZ())

JPH_NAMESPACE_BEGIN

void Vec3::CheckW() const
{
#ifdef JPH_FLOATING_POINT_EXCEPTIONS_ENABLED
	// Avoid asserts when both components are NaN
	JPH_ASSERT(reinterpret_cast<const uint32 *>(mF32)[2] == reinterpret_cast<const uint32 *>(mF32)[3]);
#endif // JPH_FLOATING_POINT_EXCEPTIONS_ENABLED
}

JPH_INLINE Vec3::Type Vec3::sFixW(Type inValue)
{
#ifdef JPH_FLOATING_POINT_EXCEPTIONS_ENABLED
	#if defined(JPH_USE_SSE)
		return _mm_shuffle_ps(inValue, inValue, _MM_SHUFFLE(2, 2, 1, 0));
	#elif defined(JPH_USE_NEON)
		return JPH_NEON_SHUFFLE_F32x4(inValue, inValue, 0, 1, 2, 2);
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

Vec3::Vec3(Vec4Arg inRHS) :
	mValue(sFixW(inRHS.mValue))
{
}

Vec3::Vec3(const Float3 &inV)
{
#if defined(JPH_USE_SSE)
	Type x = _mm_load_ss(&inV.x);
	Type y = _mm_load_ss(&inV.y);
	Type z = _mm_load_ss(&inV.z);
	Type xy = _mm_unpacklo_ps(x, y);
	mValue = _mm_shuffle_ps(xy, z, _MM_SHUFFLE(0, 0, 1, 0)); // Assure Z and W are the same
#elif defined(JPH_USE_NEON)
	float32x2_t xy = vld1_f32(&inV.x);
	float32x2_t zz = vdup_n_f32(inV.z); // Assure Z and W are the same
	mValue = vcombine_f32(xy, zz);
#else
	mF32[0] = inV[0];
	mF32[1] = inV[1];
	mF32[2] = inV[2];
	#ifdef JPH_FLOATING_POINT_EXCEPTIONS_ENABLED
		mF32[3] = inV[2];
	#endif
#endif
}

Vec3::Vec3(float inX, float inY, float inZ)
{
#if defined(JPH_USE_SSE)
	mValue = _mm_set_ps(inZ, inZ, inY, inX);
#elif defined(JPH_USE_NEON)
	uint32x2_t xy = vcreate_u32(static_cast<uint64>(BitCast<uint32>(inX)) | (static_cast<uint64>(BitCast<uint32>(inY)) << 32));
	uint32x2_t zz = vreinterpret_u32_f32(vdup_n_f32(inZ));
	mValue = vreinterpretq_f32_u32(vcombine_u32(xy, zz));
#else
	mF32[0] = inX;
	mF32[1] = inY;
	mF32[2] = inZ;
	#ifdef JPH_FLOATING_POINT_EXCEPTIONS_ENABLED
		mF32[3] = inZ;
	#endif
#endif
}

template<uint32 SwizzleX, uint32 SwizzleY, uint32 SwizzleZ>
Vec3 Vec3::Swizzle() const
{
	static_assert(SwizzleX <= 3, "SwizzleX template parameter out of range");
	static_assert(SwizzleY <= 3, "SwizzleY template parameter out of range");
	static_assert(SwizzleZ <= 3, "SwizzleZ template parameter out of range");

#if defined(JPH_USE_SSE)
	return _mm_shuffle_ps(mValue, mValue, _MM_SHUFFLE(SwizzleZ, SwizzleZ, SwizzleY, SwizzleX)); // Assure Z and W are the same
#elif defined(JPH_USE_NEON)
	return JPH_NEON_SHUFFLE_F32x4(mValue, mValue, SwizzleX, SwizzleY, SwizzleZ, SwizzleZ);
#else
	return Vec3(mF32[SwizzleX], mF32[SwizzleY], mF32[SwizzleZ]);
#endif
}

Vec3 Vec3::sZero()
{
#if defined(JPH_USE_SSE)
	return _mm_setzero_ps();
#elif defined(JPH_USE_NEON)
	return vdupq_n_f32(0);
#else
	return Vec3(0, 0, 0);
#endif
}

Vec3 Vec3::sReplicate(float inV)
{
#if defined(JPH_USE_SSE)
	return _mm_set1_ps(inV);
#elif defined(JPH_USE_NEON)
	return vdupq_n_f32(inV);
#else
	return Vec3(inV, inV, inV);
#endif
}

Vec3 Vec3::sNaN()
{
	return sReplicate(numeric_limits<float>::quiet_NaN());
}

Vec3 Vec3::sLoadFloat3Unsafe(const Float3 &inV)
{
#if defined(JPH_USE_SSE)
	Type v = _mm_loadu_ps(&inV.x);
#elif defined(JPH_USE_NEON)
	Type v = vld1q_f32(&inV.x);
#else
	Type v = { inV.x, inV.y, inV.z };
#endif
	return sFixW(v);
}

Vec3 Vec3::sMin(Vec3Arg inV1, Vec3Arg inV2)
{
#if defined(JPH_USE_SSE)
	return _mm_min_ps(inV1.mValue, inV2.mValue);
#elif defined(JPH_USE_NEON)
	return vminq_f32(inV1.mValue, inV2.mValue);
#else
	return Vec3(min(inV1.mF32[0], inV2.mF32[0]),
				min(inV1.mF32[1], inV2.mF32[1]),
				min(inV1.mF32[2], inV2.mF32[2]));
#endif
}

Vec3 Vec3::sMax(Vec3Arg inV1, Vec3Arg inV2)
{
#if defined(JPH_USE_SSE)
	return _mm_max_ps(inV1.mValue, inV2.mValue);
#elif defined(JPH_USE_NEON)
	return vmaxq_f32(inV1.mValue, inV2.mValue);
#else
	return Vec3(max(inV1.mF32[0], inV2.mF32[0]),
				max(inV1.mF32[1], inV2.mF32[1]),
				max(inV1.mF32[2], inV2.mF32[2]));
#endif
}

Vec3 Vec3::sClamp(Vec3Arg inV, Vec3Arg inMin, Vec3Arg inMax)
{
	return sMax(sMin(inV, inMax), inMin);
}

UVec4 Vec3::sEquals(Vec3Arg inV1, Vec3Arg inV2)
{
#if defined(JPH_USE_SSE)
	return _mm_castps_si128(_mm_cmpeq_ps(inV1.mValue, inV2.mValue));
#elif defined(JPH_USE_NEON)
	return vceqq_f32(inV1.mValue, inV2.mValue);
#else
	uint32 z = inV1.mF32[2] == inV2.mF32[2]? 0xffffffffu : 0;
	return UVec4(inV1.mF32[0] == inV2.mF32[0]? 0xffffffffu : 0,
				 inV1.mF32[1] == inV2.mF32[1]? 0xffffffffu : 0,
				 z,
				 z);
#endif
}

UVec4 Vec3::sLess(Vec3Arg inV1, Vec3Arg inV2)
{
#if defined(JPH_USE_SSE)
	return _mm_castps_si128(_mm_cmplt_ps(inV1.mValue, inV2.mValue));
#elif defined(JPH_USE_NEON)
	return vcltq_f32(inV1.mValue, inV2.mValue);
#else
	uint32 z = inV1.mF32[2] < inV2.mF32[2]? 0xffffffffu : 0;
	return UVec4(inV1.mF32[0] < inV2.mF32[0]? 0xffffffffu : 0,
				 inV1.mF32[1] < inV2.mF32[1]? 0xffffffffu : 0,
				 z,
				 z);
#endif
}

UVec4 Vec3::sLessOrEqual(Vec3Arg inV1, Vec3Arg inV2)
{
#if defined(JPH_USE_SSE)
	return _mm_castps_si128(_mm_cmple_ps(inV1.mValue, inV2.mValue));
#elif defined(JPH_USE_NEON)
	return vcleq_f32(inV1.mValue, inV2.mValue);
#else
	uint32 z = inV1.mF32[2] <= inV2.mF32[2]? 0xffffffffu : 0;
	return UVec4(inV1.mF32[0] <= inV2.mF32[0]? 0xffffffffu : 0,
				 inV1.mF32[1] <= inV2.mF32[1]? 0xffffffffu : 0,
				 z,
				 z);
#endif
}

UVec4 Vec3::sGreater(Vec3Arg inV1, Vec3Arg inV2)
{
#if defined(JPH_USE_SSE)
	return _mm_castps_si128(_mm_cmpgt_ps(inV1.mValue, inV2.mValue));
#elif defined(JPH_USE_NEON)
	return vcgtq_f32(inV1.mValue, inV2.mValue);
#else
	uint32 z = inV1.mF32[2] > inV2.mF32[2]? 0xffffffffu : 0;
	return UVec4(inV1.mF32[0] > inV2.mF32[0]? 0xffffffffu : 0,
				 inV1.mF32[1] > inV2.mF32[1]? 0xffffffffu : 0,
				 z,
				 z);
#endif
}

UVec4 Vec3::sGreaterOrEqual(Vec3Arg inV1, Vec3Arg inV2)
{
#if defined(JPH_USE_SSE)
	return _mm_castps_si128(_mm_cmpge_ps(inV1.mValue, inV2.mValue));
#elif defined(JPH_USE_NEON)
	return vcgeq_f32(inV1.mValue, inV2.mValue);
#else
	uint32 z = inV1.mF32[2] >= inV2.mF32[2]? 0xffffffffu : 0;
	return UVec4(inV1.mF32[0] >= inV2.mF32[0]? 0xffffffffu : 0,
				 inV1.mF32[1] >= inV2.mF32[1]? 0xffffffffu : 0,
				 z,
				 z);
#endif
}

Vec3 Vec3::sFusedMultiplyAdd(Vec3Arg inMul1, Vec3Arg inMul2, Vec3Arg inAdd)
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
	return Vec3(inMul1.mF32[0] * inMul2.mF32[0] + inAdd.mF32[0],
				inMul1.mF32[1] * inMul2.mF32[1] + inAdd.mF32[1],
				inMul1.mF32[2] * inMul2.mF32[2] + inAdd.mF32[2]);
#endif
}

Vec3 Vec3::sSelect(Vec3Arg inNotSet, Vec3Arg inSet, UVec4Arg inControl)
{
#if defined(JPH_USE_SSE4_1) && !defined(JPH_PLATFORM_WASM) // _mm_blendv_ps has problems on FireFox
	Type v = _mm_blendv_ps(inNotSet.mValue, inSet.mValue, _mm_castsi128_ps(inControl.mValue));
	return sFixW(v);
#elif defined(JPH_USE_SSE)
	__m128 is_set = _mm_castsi128_ps(_mm_srai_epi32(inControl.mValue, 31));
	Type v = _mm_or_ps(_mm_and_ps(is_set, inSet.mValue), _mm_andnot_ps(is_set, inNotSet.mValue));
	return sFixW(v);
#elif defined(JPH_USE_NEON)
	Type v = vbslq_f32(vreinterpretq_u32_s32(vshrq_n_s32(vreinterpretq_s32_u32(inControl.mValue), 31)), inSet.mValue, inNotSet.mValue);
	return sFixW(v);
#else
	Vec3 result;
	for (int i = 0; i < 3; i++)
		result.mF32[i] = (inControl.mU32[i] & 0x80000000u) ? inSet.mF32[i] : inNotSet.mF32[i];
#ifdef JPH_FLOATING_POINT_EXCEPTIONS_ENABLED
	result.mF32[3] = result.mF32[2];
#endif // JPH_FLOATING_POINT_EXCEPTIONS_ENABLED
	return result;
#endif
}

Vec3 Vec3::sOr(Vec3Arg inV1, Vec3Arg inV2)
{
#if defined(JPH_USE_SSE)
	return _mm_or_ps(inV1.mValue, inV2.mValue);
#elif defined(JPH_USE_NEON)
	return vreinterpretq_f32_u32(vorrq_u32(vreinterpretq_u32_f32(inV1.mValue), vreinterpretq_u32_f32(inV2.mValue)));
#else
	return Vec3(UVec4::sOr(inV1.ReinterpretAsInt(), inV2.ReinterpretAsInt()).ReinterpretAsFloat());
#endif
}

Vec3 Vec3::sXor(Vec3Arg inV1, Vec3Arg inV2)
{
#if defined(JPH_USE_SSE)
	return _mm_xor_ps(inV1.mValue, inV2.mValue);
#elif defined(JPH_USE_NEON)
	return vreinterpretq_f32_u32(veorq_u32(vreinterpretq_u32_f32(inV1.mValue), vreinterpretq_u32_f32(inV2.mValue)));
#else
	return Vec3(UVec4::sXor(inV1.ReinterpretAsInt(), inV2.ReinterpretAsInt()).ReinterpretAsFloat());
#endif
}

Vec3 Vec3::sAnd(Vec3Arg inV1, Vec3Arg inV2)
{
#if defined(JPH_USE_SSE)
	return _mm_and_ps(inV1.mValue, inV2.mValue);
#elif defined(JPH_USE_NEON)
	return vreinterpretq_f32_u32(vandq_u32(vreinterpretq_u32_f32(inV1.mValue), vreinterpretq_u32_f32(inV2.mValue)));
#else
	return Vec3(UVec4::sAnd(inV1.ReinterpretAsInt(), inV2.ReinterpretAsInt()).ReinterpretAsFloat());
#endif
}

Vec3 Vec3::sUnitSpherical(float inTheta, float inPhi)
{
	Vec4 s, c;
	Vec4(inTheta, inPhi, 0, 0).SinCos(s, c);
	return Vec3(s.GetX() * c.GetY(), s.GetX() * s.GetY(), c.GetX());
}

template <class Random>
Vec3 Vec3::sRandom(Random &inRandom)
{
	std::uniform_real_distribution<float> zero_to_one(0.0f, 1.0f);
	float theta = JPH_PI * zero_to_one(inRandom);
	float phi = 2.0f * JPH_PI * zero_to_one(inRandom);
	return sUnitSpherical(theta, phi);
}

bool Vec3::operator == (Vec3Arg inV2) const
{
	return sEquals(*this, inV2).TestAllXYZTrue();
}

bool Vec3::IsClose(Vec3Arg inV2, float inMaxDistSq) const
{
	return (inV2 - *this).LengthSq() <= inMaxDistSq;
}

bool Vec3::IsNearZero(float inMaxDistSq) const
{
	return LengthSq() <= inMaxDistSq;
}

Vec3 Vec3::operator * (Vec3Arg inV2) const
{
#if defined(JPH_USE_SSE)
	return _mm_mul_ps(mValue, inV2.mValue);
#elif defined(JPH_USE_NEON)
	return vmulq_f32(mValue, inV2.mValue);
#else
	return Vec3(mF32[0] * inV2.mF32[0], mF32[1] * inV2.mF32[1], mF32[2] * inV2.mF32[2]);
#endif
}

Vec3 Vec3::operator * (float inV2) const
{
#if defined(JPH_USE_SSE)
	return _mm_mul_ps(mValue, _mm_set1_ps(inV2));
#elif defined(JPH_USE_NEON)
	return vmulq_n_f32(mValue, inV2);
#else
	return Vec3(mF32[0] * inV2, mF32[1] * inV2, mF32[2] * inV2);
#endif
}

Vec3 operator * (float inV1, Vec3Arg inV2)
{
#if defined(JPH_USE_SSE)
	return _mm_mul_ps(_mm_set1_ps(inV1), inV2.mValue);
#elif defined(JPH_USE_NEON)
	return vmulq_n_f32(inV2.mValue, inV1);
#else
	return Vec3(inV1 * inV2.mF32[0], inV1 * inV2.mF32[1], inV1 * inV2.mF32[2]);
#endif
}

Vec3 Vec3::operator / (float inV2) const
{
#if defined(JPH_USE_SSE)
	return _mm_div_ps(mValue, _mm_set1_ps(inV2));
#elif defined(JPH_USE_NEON)
	return vdivq_f32(mValue, vdupq_n_f32(inV2));
#else
	return Vec3(mF32[0] / inV2, mF32[1] / inV2, mF32[2] / inV2);
#endif
}

Vec3 &Vec3::operator *= (float inV2)
{
#if defined(JPH_USE_SSE)
	mValue = _mm_mul_ps(mValue, _mm_set1_ps(inV2));
#elif defined(JPH_USE_NEON)
	mValue = vmulq_n_f32(mValue, inV2);
#else
	for (int i = 0; i < 3; ++i)
		mF32[i] *= inV2;
	#ifdef JPH_FLOATING_POINT_EXCEPTIONS_ENABLED
		mF32[3] = mF32[2];
	#endif
#endif
	return *this;
}

Vec3 &Vec3::operator *= (Vec3Arg inV2)
{
#if defined(JPH_USE_SSE)
	mValue = _mm_mul_ps(mValue, inV2.mValue);
#elif defined(JPH_USE_NEON)
	mValue = vmulq_f32(mValue, inV2.mValue);
#else
	for (int i = 0; i < 3; ++i)
		mF32[i] *= inV2.mF32[i];
	#ifdef JPH_FLOATING_POINT_EXCEPTIONS_ENABLED
		mF32[3] = mF32[2];
	#endif
#endif
	return *this;
}

Vec3 &Vec3::operator /= (float inV2)
{
#if defined(JPH_USE_SSE)
	mValue = _mm_div_ps(mValue, _mm_set1_ps(inV2));
#elif defined(JPH_USE_NEON)
	mValue = vdivq_f32(mValue, vdupq_n_f32(inV2));
#else
	for (int i = 0; i < 3; ++i)
		mF32[i] /= inV2;
	#ifdef JPH_FLOATING_POINT_EXCEPTIONS_ENABLED
		mF32[3] = mF32[2];
	#endif
#endif
	return *this;
}

Vec3 Vec3::operator + (Vec3Arg inV2) const
{
#if defined(JPH_USE_SSE)
	return _mm_add_ps(mValue, inV2.mValue);
#elif defined(JPH_USE_NEON)
	return vaddq_f32(mValue, inV2.mValue);
#else
	return Vec3(mF32[0] + inV2.mF32[0], mF32[1] + inV2.mF32[1], mF32[2] + inV2.mF32[2]);
#endif
}

Vec3 &Vec3::operator += (Vec3Arg inV2)
{
#if defined(JPH_USE_SSE)
	mValue = _mm_add_ps(mValue, inV2.mValue);
#elif defined(JPH_USE_NEON)
	mValue = vaddq_f32(mValue, inV2.mValue);
#else
	for (int i = 0; i < 3; ++i)
		mF32[i] += inV2.mF32[i];
	#ifdef JPH_FLOATING_POINT_EXCEPTIONS_ENABLED
		mF32[3] = mF32[2];
	#endif
#endif
	return *this;
}

Vec3 Vec3::operator - () const
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
		return Vec3(0.0f - mF32[0], 0.0f - mF32[1], 0.0f - mF32[2]);
	#else
		return Vec3(-mF32[0], -mF32[1], -mF32[2]);
	#endif
#endif
}

Vec3 Vec3::operator - (Vec3Arg inV2) const
{
#if defined(JPH_USE_SSE)
	return _mm_sub_ps(mValue, inV2.mValue);
#elif defined(JPH_USE_NEON)
	return vsubq_f32(mValue, inV2.mValue);
#else
	return Vec3(mF32[0] - inV2.mF32[0], mF32[1] - inV2.mF32[1], mF32[2] - inV2.mF32[2]);
#endif
}

Vec3 &Vec3::operator -= (Vec3Arg inV2)
{
#if defined(JPH_USE_SSE)
	mValue = _mm_sub_ps(mValue, inV2.mValue);
#elif defined(JPH_USE_NEON)
	mValue = vsubq_f32(mValue, inV2.mValue);
#else
	for (int i = 0; i < 3; ++i)
		mF32[i] -= inV2.mF32[i];
	#ifdef JPH_FLOATING_POINT_EXCEPTIONS_ENABLED
		mF32[3] = mF32[2];
	#endif
#endif
	return *this;
}

Vec3 Vec3::operator / (Vec3Arg inV2) const
{
	inV2.CheckW(); // Check W equals Z to avoid div by zero
#if defined(JPH_USE_SSE)
	return _mm_div_ps(mValue, inV2.mValue);
#elif defined(JPH_USE_NEON)
	return vdivq_f32(mValue, inV2.mValue);
#else
	return Vec3(mF32[0] / inV2.mF32[0], mF32[1] / inV2.mF32[1], mF32[2] / inV2.mF32[2]);
#endif
}

Vec4 Vec3::SplatX() const
{
#if defined(JPH_USE_SSE)
	return _mm_shuffle_ps(mValue, mValue, _MM_SHUFFLE(0, 0, 0, 0));
#elif defined(JPH_USE_NEON)
	return vdupq_laneq_f32(mValue, 0);
#else
	return Vec4(mF32[0], mF32[0], mF32[0], mF32[0]);
#endif
}

Vec4 Vec3::SplatY() const
{
#if defined(JPH_USE_SSE)
	return _mm_shuffle_ps(mValue, mValue, _MM_SHUFFLE(1, 1, 1, 1));
#elif defined(JPH_USE_NEON)
	return vdupq_laneq_f32(mValue, 1);
#else
	return Vec4(mF32[1], mF32[1], mF32[1], mF32[1]);
#endif
}

Vec4 Vec3::SplatZ() const
{
#if defined(JPH_USE_SSE)
	return _mm_shuffle_ps(mValue, mValue, _MM_SHUFFLE(2, 2, 2, 2));
#elif defined(JPH_USE_NEON)
	return vdupq_laneq_f32(mValue, 2);
#else
	return Vec4(mF32[2], mF32[2], mF32[2], mF32[2]);
#endif
}

int Vec3::GetLowestComponentIndex() const
{
	return GetX() < GetY() ? (GetZ() < GetX() ? 2 : 0) : (GetZ() < GetY() ? 2 : 1);
}

int Vec3::GetHighestComponentIndex() const
{
	return GetX() > GetY() ? (GetZ() > GetX() ? 2 : 0) : (GetZ() > GetY() ? 2 : 1);
}

Vec3 Vec3::Abs() const
{
#if defined(JPH_USE_AVX512)
	return _mm_range_ps(mValue, mValue, 0b1000);
#elif defined(JPH_USE_SSE)
	return _mm_max_ps(_mm_sub_ps(_mm_setzero_ps(), mValue), mValue);
#elif defined(JPH_USE_NEON)
	return vabsq_f32(mValue);
#else
	return Vec3(abs(mF32[0]), abs(mF32[1]), abs(mF32[2]));
#endif
}

Vec3 Vec3::Reciprocal() const
{
	return sReplicate(1.0f) / mValue;
}

Vec3 Vec3::Cross(Vec3Arg inV2) const
{
#if defined(JPH_USE_SSE)
	Type t1 = _mm_shuffle_ps(inV2.mValue, inV2.mValue, _MM_SHUFFLE(0, 0, 2, 1)); // Assure Z and W are the same
	t1 = _mm_mul_ps(t1, mValue);
	Type t2 = _mm_shuffle_ps(mValue, mValue, _MM_SHUFFLE(0, 0, 2, 1)); // Assure Z and W are the same
	t2 = _mm_mul_ps(t2, inV2.mValue);
	Type t3 = _mm_sub_ps(t1, t2);
	return _mm_shuffle_ps(t3, t3, _MM_SHUFFLE(0, 0, 2, 1)); // Assure Z and W are the same
#elif defined(JPH_USE_NEON)
	Type t1 = JPH_NEON_SHUFFLE_F32x4(inV2.mValue, inV2.mValue, 1, 2, 0, 0); // Assure Z and W are the same
	t1 = vmulq_f32(t1, mValue);
	Type t2 = JPH_NEON_SHUFFLE_F32x4(mValue, mValue, 1, 2, 0, 0); // Assure Z and W are the same
	t2 = vmulq_f32(t2, inV2.mValue);
	Type t3 = vsubq_f32(t1, t2);
	return JPH_NEON_SHUFFLE_F32x4(t3, t3, 1, 2, 0, 0); // Assure Z and W are the same
#else
	return Vec3(mF32[1] * inV2.mF32[2] - mF32[2] * inV2.mF32[1],
				mF32[2] * inV2.mF32[0] - mF32[0] * inV2.mF32[2],
				mF32[0] * inV2.mF32[1] - mF32[1] * inV2.mF32[0]);
#endif
}

Vec3 Vec3::DotV(Vec3Arg inV2) const
{
#if defined(JPH_USE_SSE4_1)
	return _mm_dp_ps(mValue, inV2.mValue, 0x7f);
#elif defined(JPH_USE_NEON)
	float32x4_t mul = vmulq_f32(mValue, inV2.mValue);
	mul = vsetq_lane_f32(0, mul, 3);
	return vdupq_n_f32(vaddvq_f32(mul));
#else
	float dot = 0.0f;
	for (int i = 0; i < 3; i++)
		dot += mF32[i] * inV2.mF32[i];
	return Vec3::sReplicate(dot);
#endif
}

Vec4 Vec3::DotV4(Vec3Arg inV2) const
{
#if defined(JPH_USE_SSE4_1)
	return _mm_dp_ps(mValue, inV2.mValue, 0x7f);
#elif defined(JPH_USE_NEON)
	float32x4_t mul = vmulq_f32(mValue, inV2.mValue);
	mul = vsetq_lane_f32(0, mul, 3);
	return vdupq_n_f32(vaddvq_f32(mul));
#else
	float dot = 0.0f;
	for (int i = 0; i < 3; i++)
		dot += mF32[i] * inV2.mF32[i];
	return Vec4::sReplicate(dot);
#endif
}

float Vec3::Dot(Vec3Arg inV2) const
{
#if defined(JPH_USE_SSE4_1)
	return _mm_cvtss_f32(_mm_dp_ps(mValue, inV2.mValue, 0x7f));
#elif defined(JPH_USE_NEON)
	float32x4_t mul = vmulq_f32(mValue, inV2.mValue);
	mul = vsetq_lane_f32(0, mul, 3);
	return vaddvq_f32(mul);
#else
	float dot = 0.0f;
	for (int i = 0; i < 3; i++)
		dot += mF32[i] * inV2.mF32[i];
	return dot;
#endif
}

float Vec3::LengthSq() const
{
#if defined(JPH_USE_SSE4_1)
	return _mm_cvtss_f32(_mm_dp_ps(mValue, mValue, 0x7f));
#elif defined(JPH_USE_NEON)
	float32x4_t mul = vmulq_f32(mValue, mValue);
	mul = vsetq_lane_f32(0, mul, 3);
	return vaddvq_f32(mul);
#else
	float len_sq = 0.0f;
	for (int i = 0; i < 3; i++)
		len_sq += mF32[i] * mF32[i];
	return len_sq;
#endif
}

float Vec3::Length() const
{
#if defined(JPH_USE_SSE4_1)
	return _mm_cvtss_f32(_mm_sqrt_ss(_mm_dp_ps(mValue, mValue, 0x7f)));
#elif defined(JPH_USE_NEON)
	float32x4_t mul = vmulq_f32(mValue, mValue);
	mul = vsetq_lane_f32(0, mul, 3);
	float32x2_t sum = vdup_n_f32(vaddvq_f32(mul));
	return vget_lane_f32(vsqrt_f32(sum), 0);
#else
	return sqrt(LengthSq());
#endif
}

Vec3 Vec3::Sqrt() const
{
#if defined(JPH_USE_SSE)
	return _mm_sqrt_ps(mValue);
#elif defined(JPH_USE_NEON)
	return vsqrtq_f32(mValue);
#else
	return Vec3(sqrt(mF32[0]), sqrt(mF32[1]), sqrt(mF32[2]));
#endif
}

Vec3 Vec3::Normalized() const
{
#if defined(JPH_USE_SSE4_1)
	return _mm_div_ps(mValue, _mm_sqrt_ps(_mm_dp_ps(mValue, mValue, 0x7f)));
#elif defined(JPH_USE_NEON)
	float32x4_t mul = vmulq_f32(mValue, mValue);
	mul = vsetq_lane_f32(0, mul, 3);
	float32x4_t sum = vdupq_n_f32(vaddvq_f32(mul));
	return vdivq_f32(mValue, vsqrtq_f32(sum));
#else
	return *this / Length();
#endif
}

Vec3 Vec3::NormalizedOr(Vec3Arg inZeroValue) const
{
#if defined(JPH_USE_SSE4_1) && !defined(JPH_PLATFORM_WASM) // _mm_blendv_ps has problems on FireFox
	Type len_sq = _mm_dp_ps(mValue, mValue, 0x7f);
	// clang with '-ffast-math' (which you should not use!) can generate _mm_rsqrt_ps
	// instructions which produce INFs/NaNs when they get a denormal float as input.
	// We therefore treat denormals as zero here.
	Type is_zero = _mm_cmple_ps(len_sq, _mm_set1_ps(FLT_MIN));
#ifdef JPH_FLOATING_POINT_EXCEPTIONS_ENABLED
	if (_mm_movemask_ps(is_zero) == 0xf)
		return inZeroValue;
	else
		return _mm_div_ps(mValue, _mm_sqrt_ps(len_sq));
#else
	return _mm_blendv_ps(_mm_div_ps(mValue, _mm_sqrt_ps(len_sq)), inZeroValue.mValue, is_zero);
#endif // JPH_FLOATING_POINT_EXCEPTIONS_ENABLED
#elif defined(JPH_USE_NEON)
	float32x4_t mul = vmulq_f32(mValue, mValue);
	mul = vsetq_lane_f32(0, mul, 3);
	float32x4_t len_sq = vdupq_n_f32(vaddvq_f32(mul));
	uint32x4_t is_zero = vcleq_f32(len_sq, vdupq_n_f32(FLT_MIN));
	return vbslq_f32(is_zero, inZeroValue.mValue, vdivq_f32(mValue, vsqrtq_f32(len_sq)));
#else
	float len_sq = LengthSq();
	if (len_sq <= FLT_MIN)
		return inZeroValue;
	else
		return *this / sqrt(len_sq);
#endif
}

bool Vec3::IsNormalized(float inTolerance) const
{
	return abs(LengthSq() - 1.0f) <= inTolerance;
}

bool Vec3::IsNaN() const
{
#if defined(JPH_USE_AVX512)
	return (_mm_fpclass_ps_mask(mValue, 0b10000001) & 0x7) != 0;
#elif defined(JPH_USE_SSE)
	return (_mm_movemask_ps(_mm_cmpunord_ps(mValue, mValue)) & 0x7) != 0;
#elif defined(JPH_USE_NEON)
	uint32x4_t mask = JPH_NEON_UINT32x4(1, 1, 1, 0);
	uint32x4_t is_equal = vceqq_f32(mValue, mValue); // If a number is not equal to itself it's a NaN
	return vaddvq_u32(vandq_u32(is_equal, mask)) != 3;
#else
	return isnan(mF32[0]) || isnan(mF32[1]) || isnan(mF32[2]);
#endif
}

void Vec3::StoreFloat3(Float3 *outV) const
{
#if defined(JPH_USE_SSE)
	_mm_store_ss(&outV->x, mValue);
	Vec3 t = Swizzle<SWIZZLE_Y, SWIZZLE_Z, SWIZZLE_UNUSED>();
	_mm_store_ss(&outV->y, t.mValue);
	t = t.Swizzle<SWIZZLE_Y, SWIZZLE_UNUSED, SWIZZLE_UNUSED>();
	_mm_store_ss(&outV->z, t.mValue);
#elif defined(JPH_USE_NEON)
	float32x2_t xy = vget_low_f32(mValue);
	vst1_f32(&outV->x, xy);
	vst1q_lane_f32(&outV->z, mValue, 2);
#else
	outV->x = mF32[0];
	outV->y = mF32[1];
	outV->z = mF32[2];
#endif
}

UVec4 Vec3::ToInt() const
{
#if defined(JPH_USE_SSE)
	return _mm_cvttps_epi32(mValue);
#elif defined(JPH_USE_NEON)
	return vcvtq_u32_f32(mValue);
#else
	return UVec4(uint32(mF32[0]), uint32(mF32[1]), uint32(mF32[2]), uint32(mF32[3]));
#endif
}

UVec4 Vec3::ReinterpretAsInt() const
{
#if defined(JPH_USE_SSE)
	return UVec4(_mm_castps_si128(mValue));
#elif defined(JPH_USE_NEON)
	return vreinterpretq_u32_f32(mValue);
#else
	return *reinterpret_cast<const UVec4 *>(this);
#endif
}

float Vec3::ReduceMin() const
{
	Vec3 v = sMin(mValue, Swizzle<SWIZZLE_Y, SWIZZLE_UNUSED, SWIZZLE_Z>());
	v = sMin(v, v.Swizzle<SWIZZLE_Z, SWIZZLE_UNUSED, SWIZZLE_UNUSED>());
	return v.GetX();
}

float Vec3::ReduceMax() const
{
	Vec3 v = sMax(mValue, Swizzle<SWIZZLE_Y, SWIZZLE_UNUSED, SWIZZLE_Z>());
	v = sMax(v, v.Swizzle<SWIZZLE_Z, SWIZZLE_UNUSED, SWIZZLE_UNUSED>());
	return v.GetX();
}

Vec3 Vec3::GetNormalizedPerpendicular() const
{
	if (abs(mF32[0]) > abs(mF32[1]))
	{
		float len = sqrt(mF32[0] * mF32[0] + mF32[2] * mF32[2]);
		return Vec3(mF32[2], 0.0f, -mF32[0]) / len;
	}
	else
	{
		float len = sqrt(mF32[1] * mF32[1] + mF32[2] * mF32[2]);
		return Vec3(0.0f, mF32[2], -mF32[1]) / len;
	}
}

Vec3 Vec3::GetSign() const
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
	return Vec3(std::signbit(mF32[0])? -1.0f : 1.0f,
				std::signbit(mF32[1])? -1.0f : 1.0f,
				std::signbit(mF32[2])? -1.0f : 1.0f);
#endif
}

JPH_NAMESPACE_END

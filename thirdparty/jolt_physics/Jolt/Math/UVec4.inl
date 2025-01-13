// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

JPH_NAMESPACE_BEGIN

UVec4::UVec4(uint32 inX, uint32 inY, uint32 inZ, uint32 inW)
{
#if defined(JPH_USE_SSE)
	mValue = _mm_set_epi32(int(inW), int(inZ), int(inY), int(inX));
#elif defined(JPH_USE_NEON)
	uint32x2_t xy = vcreate_u32(static_cast<uint64>(inX) | (static_cast<uint64>(inY) << 32));
	uint32x2_t zw = vcreate_u32(static_cast<uint64>(inZ) | (static_cast<uint64>(inW) << 32));
	mValue = vcombine_u32(xy, zw);
#else
	mU32[0] = inX;
	mU32[1] = inY;
	mU32[2] = inZ;
	mU32[3] = inW;
#endif
}

bool UVec4::operator == (UVec4Arg inV2) const
{
	return sEquals(*this, inV2).TestAllTrue();
}

template<uint32 SwizzleX, uint32 SwizzleY, uint32 SwizzleZ, uint32 SwizzleW>
UVec4 UVec4::Swizzle() const
{
	static_assert(SwizzleX <= 3, "SwizzleX template parameter out of range");
	static_assert(SwizzleY <= 3, "SwizzleY template parameter out of range");
	static_assert(SwizzleZ <= 3, "SwizzleZ template parameter out of range");
	static_assert(SwizzleW <= 3, "SwizzleW template parameter out of range");

#if defined(JPH_USE_SSE)
	return _mm_shuffle_epi32(mValue, _MM_SHUFFLE(SwizzleW, SwizzleZ, SwizzleY, SwizzleX));
#elif defined(JPH_USE_NEON)
	return JPH_NEON_SHUFFLE_U32x4(mValue, mValue, SwizzleX, SwizzleY, SwizzleZ, SwizzleW);
#else
	return UVec4(mU32[SwizzleX], mU32[SwizzleY], mU32[SwizzleZ], mU32[SwizzleW]);
#endif
}

UVec4 UVec4::sZero()
{
#if defined(JPH_USE_SSE)
	return _mm_setzero_si128();
#elif defined(JPH_USE_NEON)
	return vdupq_n_u32(0);
#else
	return UVec4(0, 0, 0, 0);
#endif
}

UVec4 UVec4::sReplicate(uint32 inV)
{
#if defined(JPH_USE_SSE)
	return _mm_set1_epi32(int(inV));
#elif defined(JPH_USE_NEON)
	return vdupq_n_u32(inV);
#else
	return UVec4(inV, inV, inV, inV);
#endif
}

UVec4 UVec4::sLoadInt(const uint32 *inV)
{
#if defined(JPH_USE_SSE)
	return _mm_castps_si128(_mm_load_ss(reinterpret_cast<const float*>(inV)));
#elif defined(JPH_USE_NEON)
	return vsetq_lane_u32(*inV, vdupq_n_u32(0), 0);
#else
	return UVec4(*inV, 0, 0, 0);
#endif
}

UVec4 UVec4::sLoadInt4(const uint32 *inV)
{
#if defined(JPH_USE_SSE)
	return _mm_loadu_si128(reinterpret_cast<const __m128i *>(inV));
#elif defined(JPH_USE_NEON)
	return vld1q_u32(inV);
#else
	return UVec4(inV[0], inV[1], inV[2], inV[3]);
#endif
}

UVec4 UVec4::sLoadInt4Aligned(const uint32 *inV)
{
#if defined(JPH_USE_SSE)
	return _mm_load_si128(reinterpret_cast<const __m128i *>(inV));
#elif defined(JPH_USE_NEON)
	return vld1q_u32(inV); // ARM doesn't make distinction between aligned or not
#else
	return UVec4(inV[0], inV[1], inV[2], inV[3]);
#endif
}

template <const int Scale>
UVec4 UVec4::sGatherInt4(const uint32 *inBase, UVec4Arg inOffsets)
{
#ifdef JPH_USE_AVX2
	return _mm_i32gather_epi32(reinterpret_cast<const int *>(inBase), inOffsets.mValue, Scale);
#else
	const uint8 *base = reinterpret_cast<const uint8 *>(inBase);
	uint32 x = *reinterpret_cast<const uint32 *>(base + inOffsets.GetX() * Scale);
	uint32 y = *reinterpret_cast<const uint32 *>(base + inOffsets.GetY() * Scale);
	uint32 z = *reinterpret_cast<const uint32 *>(base + inOffsets.GetZ() * Scale);
	uint32 w = *reinterpret_cast<const uint32 *>(base + inOffsets.GetW() * Scale);
	return UVec4(x, y, z, w);
#endif
}

UVec4 UVec4::sMin(UVec4Arg inV1, UVec4Arg inV2)
{
#if defined(JPH_USE_SSE4_1)
	return _mm_min_epu32(inV1.mValue, inV2.mValue);
#elif defined(JPH_USE_NEON)
	return vminq_u32(inV1.mValue, inV2.mValue);
#else
	UVec4 result;
	for (int i = 0; i < 4; i++)
		result.mU32[i] = min(inV1.mU32[i], inV2.mU32[i]);
	return result;
#endif
}

UVec4 UVec4::sMax(UVec4Arg inV1, UVec4Arg inV2)
{
#if defined(JPH_USE_SSE4_1)
	return _mm_max_epu32(inV1.mValue, inV2.mValue);
#elif defined(JPH_USE_NEON)
	return vmaxq_u32(inV1.mValue, inV2.mValue);
#else
	UVec4 result;
	for (int i = 0; i < 4; i++)
		result.mU32[i] = max(inV1.mU32[i], inV2.mU32[i]);
	return result;
#endif
}

UVec4 UVec4::sEquals(UVec4Arg inV1, UVec4Arg inV2)
{
#if defined(JPH_USE_SSE)
	return _mm_cmpeq_epi32(inV1.mValue, inV2.mValue);
#elif defined(JPH_USE_NEON)
	return vceqq_u32(inV1.mValue, inV2.mValue);
#else
	return UVec4(inV1.mU32[0] == inV2.mU32[0]? 0xffffffffu : 0,
				 inV1.mU32[1] == inV2.mU32[1]? 0xffffffffu : 0,
				 inV1.mU32[2] == inV2.mU32[2]? 0xffffffffu : 0,
				 inV1.mU32[3] == inV2.mU32[3]? 0xffffffffu : 0);
#endif
}

UVec4 UVec4::sSelect(UVec4Arg inNotSet, UVec4Arg inSet, UVec4Arg inControl)
{
#if defined(JPH_USE_SSE4_1) && !defined(JPH_PLATFORM_WASM) // _mm_blendv_ps has problems on FireFox
	return _mm_castps_si128(_mm_blendv_ps(_mm_castsi128_ps(inNotSet.mValue), _mm_castsi128_ps(inSet.mValue), _mm_castsi128_ps(inControl.mValue)));
#elif defined(JPH_USE_SSE)
	__m128 is_set = _mm_castsi128_ps(_mm_srai_epi32(inControl.mValue, 31));
	return _mm_castps_si128(_mm_or_ps(_mm_and_ps(is_set, _mm_castsi128_ps(inSet.mValue)), _mm_andnot_ps(is_set, _mm_castsi128_ps(inNotSet.mValue))));
#elif defined(JPH_USE_NEON)
	return vbslq_u32(vreinterpretq_u32_s32(vshrq_n_s32(vreinterpretq_s32_u32(inControl.mValue), 31)), inSet.mValue, inNotSet.mValue);
#else
	UVec4 result;
	for (int i = 0; i < 4; i++)
		result.mU32[i] = (inControl.mU32[i] & 0x80000000u) ? inSet.mU32[i] : inNotSet.mU32[i];
	return result;
#endif
}

UVec4 UVec4::sOr(UVec4Arg inV1, UVec4Arg inV2)
{
#if defined(JPH_USE_SSE)
	return _mm_or_si128(inV1.mValue, inV2.mValue);
#elif defined(JPH_USE_NEON)
	return vorrq_u32(inV1.mValue, inV2.mValue);
#else
	return UVec4(inV1.mU32[0] | inV2.mU32[0],
				 inV1.mU32[1] | inV2.mU32[1],
				 inV1.mU32[2] | inV2.mU32[2],
				 inV1.mU32[3] | inV2.mU32[3]);
#endif
}

UVec4 UVec4::sXor(UVec4Arg inV1, UVec4Arg inV2)
{
#if defined(JPH_USE_SSE)
	return _mm_xor_si128(inV1.mValue, inV2.mValue);
#elif defined(JPH_USE_NEON)
	return veorq_u32(inV1.mValue, inV2.mValue);
#else
	return UVec4(inV1.mU32[0] ^ inV2.mU32[0],
				 inV1.mU32[1] ^ inV2.mU32[1],
				 inV1.mU32[2] ^ inV2.mU32[2],
				 inV1.mU32[3] ^ inV2.mU32[3]);
#endif
}

UVec4 UVec4::sAnd(UVec4Arg inV1, UVec4Arg inV2)
{
#if defined(JPH_USE_SSE)
	return _mm_and_si128(inV1.mValue, inV2.mValue);
#elif defined(JPH_USE_NEON)
	return vandq_u32(inV1.mValue, inV2.mValue);
#else
	return UVec4(inV1.mU32[0] & inV2.mU32[0],
				 inV1.mU32[1] & inV2.mU32[1],
				 inV1.mU32[2] & inV2.mU32[2],
				 inV1.mU32[3] & inV2.mU32[3]);
#endif
}


UVec4 UVec4::sNot(UVec4Arg inV1)
{
#if defined(JPH_USE_AVX512)
	return _mm_ternarylogic_epi32(inV1.mValue, inV1.mValue, inV1.mValue, 0b01010101);
#elif defined(JPH_USE_SSE)
	return sXor(inV1, sReplicate(0xffffffff));
#elif defined(JPH_USE_NEON)
	return vmvnq_u32(inV1.mValue);
#else
	return UVec4(~inV1.mU32[0], ~inV1.mU32[1], ~inV1.mU32[2], ~inV1.mU32[3]);
#endif
}

UVec4 UVec4::sSort4True(UVec4Arg inValue, UVec4Arg inIndex)
{
	// If inValue.z is false then shift W to Z
	UVec4 v = UVec4::sSelect(inIndex.Swizzle<SWIZZLE_X, SWIZZLE_Y, SWIZZLE_W, SWIZZLE_W>(), inIndex, inValue.SplatZ());

	// If inValue.y is false then shift Z and further to Y and further
	v = UVec4::sSelect(v.Swizzle<SWIZZLE_X, SWIZZLE_Z, SWIZZLE_W, SWIZZLE_W>(), v, inValue.SplatY());

	// If inValue.x is false then shift X and further to Y and further
	v = UVec4::sSelect(v.Swizzle<SWIZZLE_Y, SWIZZLE_Z, SWIZZLE_W, SWIZZLE_W>(), v, inValue.SplatX());

	return v;
}

UVec4 UVec4::operator * (UVec4Arg inV2) const
{
#if defined(JPH_USE_SSE4_1)
	return _mm_mullo_epi32(mValue, inV2.mValue);
#elif defined(JPH_USE_NEON)
	return vmulq_u32(mValue, inV2.mValue);
#else
	UVec4 result;
	for (int i = 0; i < 4; i++)
		result.mU32[i] = mU32[i] * inV2.mU32[i];
	return result;
#endif
}

UVec4 UVec4::operator + (UVec4Arg inV2)
{
#if defined(JPH_USE_SSE)
	return _mm_add_epi32(mValue, inV2.mValue);
#elif defined(JPH_USE_NEON)
	return vaddq_u32(mValue, inV2.mValue);
#else
	return UVec4(mU32[0] + inV2.mU32[0],
				 mU32[1] + inV2.mU32[1],
				 mU32[2] + inV2.mU32[2],
				 mU32[3] + inV2.mU32[3]);
#endif
}

UVec4 &UVec4::operator += (UVec4Arg inV2)
{
#if defined(JPH_USE_SSE)
	mValue = _mm_add_epi32(mValue, inV2.mValue);
#elif defined(JPH_USE_NEON)
	mValue = vaddq_u32(mValue, inV2.mValue);
#else
	for (int i = 0; i < 4; ++i)
		mU32[i] += inV2.mU32[i];
#endif
	return *this;
}

UVec4 UVec4::SplatX() const
{
#if defined(JPH_USE_SSE)
	return _mm_shuffle_epi32(mValue, _MM_SHUFFLE(0, 0, 0, 0));
#elif defined(JPH_USE_NEON)
	return vdupq_laneq_u32(mValue, 0);
#else
	return UVec4(mU32[0], mU32[0], mU32[0], mU32[0]);
#endif
}

UVec4 UVec4::SplatY() const
{
#if defined(JPH_USE_SSE)
	return _mm_shuffle_epi32(mValue, _MM_SHUFFLE(1, 1, 1, 1));
#elif defined(JPH_USE_NEON)
	return vdupq_laneq_u32(mValue, 1);
#else
	return UVec4(mU32[1], mU32[1], mU32[1], mU32[1]);
#endif
}

UVec4 UVec4::SplatZ() const
{
#if defined(JPH_USE_SSE)
	return _mm_shuffle_epi32(mValue, _MM_SHUFFLE(2, 2, 2, 2));
#elif defined(JPH_USE_NEON)
	return vdupq_laneq_u32(mValue, 2);
#else
	return UVec4(mU32[2], mU32[2], mU32[2], mU32[2]);
#endif
}

UVec4 UVec4::SplatW() const
{
#if defined(JPH_USE_SSE)
	return _mm_shuffle_epi32(mValue, _MM_SHUFFLE(3, 3, 3, 3));
#elif defined(JPH_USE_NEON)
	return vdupq_laneq_u32(mValue, 3);
#else
	return UVec4(mU32[3], mU32[3], mU32[3], mU32[3]);
#endif
}

Vec4 UVec4::ToFloat() const
{
#if defined(JPH_USE_SSE)
	return _mm_cvtepi32_ps(mValue);
#elif defined(JPH_USE_NEON)
	return vcvtq_f32_u32(mValue);
#else
	return Vec4((float)mU32[0], (float)mU32[1], (float)mU32[2], (float)mU32[3]);
#endif
}

Vec4 UVec4::ReinterpretAsFloat() const
{
#if defined(JPH_USE_SSE)
	return Vec4(_mm_castsi128_ps(mValue));
#elif defined(JPH_USE_NEON)
	return vreinterpretq_f32_u32(mValue);
#else
	return *reinterpret_cast<const Vec4 *>(this);
#endif
}

void UVec4::StoreInt4(uint32 *outV) const
{
#if defined(JPH_USE_SSE)
	_mm_storeu_si128(reinterpret_cast<__m128i *>(outV), mValue);
#elif defined(JPH_USE_NEON)
	vst1q_u32(outV, mValue);
#else
	for (int i = 0; i < 4; ++i)
		outV[i] = mU32[i];
#endif
}

void UVec4::StoreInt4Aligned(uint32 *outV) const
{
#if defined(JPH_USE_SSE)
	_mm_store_si128(reinterpret_cast<__m128i *>(outV), mValue);
#elif defined(JPH_USE_NEON)
	vst1q_u32(outV, mValue); // ARM doesn't make distinction between aligned or not
#else
	for (int i = 0; i < 4; ++i)
		outV[i] = mU32[i];
#endif
}

int UVec4::CountTrues() const
{
#if defined(JPH_USE_SSE)
	return CountBits(_mm_movemask_ps(_mm_castsi128_ps(mValue)));
#elif defined(JPH_USE_NEON)
	return vaddvq_u32(vshrq_n_u32(mValue, 31));
#else
	return (mU32[0] >> 31) + (mU32[1] >> 31) + (mU32[2] >> 31) + (mU32[3] >> 31);
#endif
}

int UVec4::GetTrues() const
{
#if defined(JPH_USE_SSE)
	return _mm_movemask_ps(_mm_castsi128_ps(mValue));
#elif defined(JPH_USE_NEON)
	int32x4_t shift = JPH_NEON_INT32x4(0, 1, 2, 3);
	return vaddvq_u32(vshlq_u32(vshrq_n_u32(mValue, 31), shift));
#else
	return (mU32[0] >> 31) | ((mU32[1] >> 31) << 1) | ((mU32[2] >> 31) << 2) | ((mU32[3] >> 31) << 3);
#endif
}

bool UVec4::TestAnyTrue() const
{
	return GetTrues() != 0;
}

bool UVec4::TestAnyXYZTrue() const
{
	return (GetTrues() & 0b111) != 0;
}

bool UVec4::TestAllTrue() const
{
	return GetTrues() == 0b1111;
}

bool UVec4::TestAllXYZTrue() const
{
	return (GetTrues() & 0b111) == 0b111;
}

template <const uint Count>
UVec4 UVec4::LogicalShiftLeft() const
{
	static_assert(Count <= 31, "Invalid shift");

#if defined(JPH_USE_SSE)
	return _mm_slli_epi32(mValue, Count);
#elif defined(JPH_USE_NEON)
	return vshlq_n_u32(mValue, Count);
#else
	return UVec4(mU32[0] << Count, mU32[1] << Count, mU32[2] << Count, mU32[3] << Count);
#endif
}

template <const uint Count>
UVec4 UVec4::LogicalShiftRight() const
{
	static_assert(Count <= 31, "Invalid shift");

#if defined(JPH_USE_SSE)
	return _mm_srli_epi32(mValue, Count);
#elif defined(JPH_USE_NEON)
	return vshrq_n_u32(mValue, Count);
#else
	return UVec4(mU32[0] >> Count, mU32[1] >> Count, mU32[2] >> Count, mU32[3] >> Count);
#endif
}

template <const uint Count>
UVec4 UVec4::ArithmeticShiftRight() const
{
	static_assert(Count <= 31, "Invalid shift");

#if defined(JPH_USE_SSE)
	return _mm_srai_epi32(mValue, Count);
#elif defined(JPH_USE_NEON)
	return vreinterpretq_u32_s32(vshrq_n_s32(vreinterpretq_s32_u32(mValue), Count));
#else
	return UVec4(uint32(int32_t(mU32[0]) >> Count),
				 uint32(int32_t(mU32[1]) >> Count),
				 uint32(int32_t(mU32[2]) >> Count),
				 uint32(int32_t(mU32[3]) >> Count));
#endif
}

UVec4 UVec4::Expand4Uint16Lo() const
{
#if defined(JPH_USE_SSE)
	return _mm_unpacklo_epi16(mValue, _mm_castps_si128(_mm_setzero_ps()));
#elif defined(JPH_USE_NEON)
	uint16x4_t value = vget_low_u16(vreinterpretq_u16_u32(mValue));
	uint16x4_t zero = vdup_n_u16(0);
	return vreinterpretq_u32_u16(vcombine_u16(vzip1_u16(value, zero), vzip2_u16(value, zero)));
#else
	return UVec4(mU32[0] & 0xffff,
				 (mU32[0] >> 16) & 0xffff,
				 mU32[1] & 0xffff,
				 (mU32[1] >> 16) & 0xffff);
#endif
}

UVec4 UVec4::Expand4Uint16Hi() const
{
#if defined(JPH_USE_SSE)
	return _mm_unpackhi_epi16(mValue, _mm_castps_si128(_mm_setzero_ps()));
#elif defined(JPH_USE_NEON)
	uint16x4_t value = vget_high_u16(vreinterpretq_u16_u32(mValue));
	uint16x4_t zero = vdup_n_u16(0);
	return vreinterpretq_u32_u16(vcombine_u16(vzip1_u16(value, zero), vzip2_u16(value, zero)));
#else
	return UVec4(mU32[2] & 0xffff,
				 (mU32[2] >> 16) & 0xffff,
				 mU32[3] & 0xffff,
				 (mU32[3] >> 16) & 0xffff);
#endif
}

UVec4 UVec4::Expand4Byte0() const
{
#if defined(JPH_USE_SSE4_1)
	return _mm_shuffle_epi8(mValue, _mm_set_epi32(int(0xffffff03), int(0xffffff02), int(0xffffff01), int(0xffffff00)));
#elif defined(JPH_USE_NEON)
	uint8x16_t idx = JPH_NEON_UINT8x16(0x00, 0x7f, 0x7f, 0x7f, 0x01, 0x7f, 0x7f, 0x7f, 0x02, 0x7f, 0x7f, 0x7f, 0x03, 0x7f, 0x7f, 0x7f);
	return vreinterpretq_u32_s8(vqtbl1q_s8(vreinterpretq_s8_u32(mValue), idx));
#else
	UVec4 result;
	for (int i = 0; i < 4; i++)
		result.mU32[i] = (mU32[0] >> (i * 8)) & 0xff;
	return result;
#endif
}

UVec4 UVec4::Expand4Byte4() const
{
#if defined(JPH_USE_SSE4_1)
	return _mm_shuffle_epi8(mValue, _mm_set_epi32(int(0xffffff07), int(0xffffff06), int(0xffffff05), int(0xffffff04)));
#elif defined(JPH_USE_NEON)
	uint8x16_t idx = JPH_NEON_UINT8x16(0x04, 0x7f, 0x7f, 0x7f, 0x05, 0x7f, 0x7f, 0x7f, 0x06, 0x7f, 0x7f, 0x7f, 0x07, 0x7f, 0x7f, 0x7f);
	return vreinterpretq_u32_s8(vqtbl1q_s8(vreinterpretq_s8_u32(mValue), idx));
#else
	UVec4 result;
	for (int i = 0; i < 4; i++)
		result.mU32[i] = (mU32[1] >> (i * 8)) & 0xff;
	return result;
#endif
}

UVec4 UVec4::Expand4Byte8() const
{
#if defined(JPH_USE_SSE4_1)
	return _mm_shuffle_epi8(mValue, _mm_set_epi32(int(0xffffff0b), int(0xffffff0a), int(0xffffff09), int(0xffffff08)));
#elif defined(JPH_USE_NEON)
	uint8x16_t idx = JPH_NEON_UINT8x16(0x08, 0x7f, 0x7f, 0x7f, 0x09, 0x7f, 0x7f, 0x7f, 0x0a, 0x7f, 0x7f, 0x7f, 0x0b, 0x7f, 0x7f, 0x7f);
	return vreinterpretq_u32_s8(vqtbl1q_s8(vreinterpretq_s8_u32(mValue), idx));
#else
	UVec4 result;
	for (int i = 0; i < 4; i++)
		result.mU32[i] = (mU32[2] >> (i * 8)) & 0xff;
	return result;
#endif
}

UVec4 UVec4::Expand4Byte12() const
{
#if defined(JPH_USE_SSE4_1)
	return _mm_shuffle_epi8(mValue, _mm_set_epi32(int(0xffffff0f), int(0xffffff0e), int(0xffffff0d), int(0xffffff0c)));
#elif defined(JPH_USE_NEON)
	uint8x16_t idx = JPH_NEON_UINT8x16(0x0c, 0x7f, 0x7f, 0x7f, 0x0d, 0x7f, 0x7f, 0x7f, 0x0e, 0x7f, 0x7f, 0x7f, 0x0f, 0x7f, 0x7f, 0x7f);
	return vreinterpretq_u32_s8(vqtbl1q_s8(vreinterpretq_s8_u32(mValue), idx));
#else
	UVec4 result;
	for (int i = 0; i < 4; i++)
		result.mU32[i] = (mU32[3] >> (i * 8)) & 0xff;
	return result;
#endif
}

UVec4 UVec4::ShiftComponents4Minus(int inCount) const
{
#if defined(JPH_USE_SSE4_1) || defined(JPH_USE_NEON)
	alignas(UVec4) static constexpr uint32 sFourMinusXShuffle[5][4] =
	{
		{ 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff },
		{ 0x0f0e0d0c, 0xffffffff, 0xffffffff, 0xffffffff },
		{ 0x0b0a0908, 0x0f0e0d0c, 0xffffffff, 0xffffffff },
		{ 0x07060504, 0x0b0a0908, 0x0f0e0d0c, 0xffffffff },
		{ 0x03020100, 0x07060504, 0x0b0a0908, 0x0f0e0d0c }
	};
#endif

#if defined(JPH_USE_SSE4_1)
	return _mm_shuffle_epi8(mValue, *reinterpret_cast<const UVec4::Type *>(sFourMinusXShuffle[inCount]));
#elif defined(JPH_USE_NEON)
	uint8x16_t idx = vreinterpretq_u8_u32(*reinterpret_cast<const UVec4::Type *>(sFourMinusXShuffle[inCount]));
	return vreinterpretq_u32_s8(vqtbl1q_s8(vreinterpretq_s8_u32(mValue), idx));
#else
	UVec4 result = UVec4::sZero();
	for (int i = 0; i < inCount; i++)
		result.mU32[i] = mU32[i + 4 - inCount];
	return result;
#endif
}

JPH_NAMESPACE_END

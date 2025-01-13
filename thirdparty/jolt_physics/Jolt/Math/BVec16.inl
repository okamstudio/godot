// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2024 Jorrit Rouwe
// SPDX-License-Identifier: MIT

JPH_NAMESPACE_BEGIN

BVec16::BVec16(uint8 inB0, uint8 inB1, uint8 inB2, uint8 inB3, uint8 inB4, uint8 inB5, uint8 inB6, uint8 inB7, uint8 inB8, uint8 inB9, uint8 inB10, uint8 inB11, uint8 inB12, uint8 inB13, uint8 inB14, uint8 inB15)
{
#if defined(JPH_USE_SSE)
	mValue = _mm_set_epi8(char(inB15), char(inB14), char(inB13), char(inB12), char(inB11), char(inB10), char(inB9), char(inB8), char(inB7), char(inB6), char(inB5), char(inB4), char(inB3), char(inB2), char(inB1), char(inB0));
#elif defined(JPH_USE_NEON)
	uint8x8_t v1 = vcreate_u8(uint64(inB0) | (uint64(inB1) << 8) | (uint64(inB2) << 16) | (uint64(inB3) << 24) | (uint64(inB4) << 32) | (uint64(inB5) << 40) | (uint64(inB6) << 48) | (uint64(inB7) << 56));
	uint8x8_t v2 = vcreate_u8(uint64(inB8) | (uint64(inB9) << 8) | (uint64(inB10) << 16) | (uint64(inB11) << 24) | (uint64(inB12) << 32) | (uint64(inB13) << 40) | (uint64(inB14) << 48) | (uint64(inB15) << 56));
	mValue = vcombine_u8(v1, v2);
#else
	mU8[0] = inB0;
	mU8[1] = inB1;
	mU8[2] = inB2;
	mU8[3] = inB3;
	mU8[4] = inB4;
	mU8[5] = inB5;
	mU8[6] = inB6;
	mU8[7] = inB7;
	mU8[8] = inB8;
	mU8[9] = inB9;
	mU8[10] = inB10;
	mU8[11] = inB11;
	mU8[12] = inB12;
	mU8[13] = inB13;
	mU8[14] = inB14;
	mU8[15] = inB15;
#endif
}

BVec16::BVec16(uint64 inV0, uint64 inV1)
{
	mU64[0] = inV0;
	mU64[1] = inV1;
}

bool BVec16::operator == (BVec16Arg inV2) const
{
	return sEquals(*this, inV2).TestAllTrue();
}

BVec16 BVec16::sZero()
{
#if defined(JPH_USE_SSE)
	return _mm_setzero_si128();
#elif defined(JPH_USE_NEON)
	return vdupq_n_u8(0);
#else
	return BVec16(0, 0);
#endif
}

BVec16 BVec16::sReplicate(uint8 inV)
{
#if defined(JPH_USE_SSE)
	return _mm_set1_epi8(char(inV));
#elif defined(JPH_USE_NEON)
	return vdupq_n_u8(inV);
#else
	uint64 v(inV);
	v |= v << 8;
	v |= v << 16;
	v |= v << 32;
	return BVec16(v, v);
#endif
}

BVec16 BVec16::sLoadByte16(const uint8 *inV)
{
#if defined(JPH_USE_SSE)
	return _mm_loadu_si128(reinterpret_cast<const __m128i *>(inV));
#elif defined(JPH_USE_NEON)
	return vld1q_u8(inV);
#else
	return BVec16(inV[0], inV[1], inV[2], inV[3], inV[4], inV[5], inV[6], inV[7], inV[8], inV[9], inV[10], inV[11], inV[12], inV[13], inV[14], inV[15]);
#endif
}

BVec16 BVec16::sEquals(BVec16Arg inV1, BVec16Arg inV2)
{
#if defined(JPH_USE_SSE)
	return _mm_cmpeq_epi8(inV1.mValue, inV2.mValue);
#elif defined(JPH_USE_NEON)
	return vceqq_u8(inV1.mValue, inV2.mValue);
#else
	auto equals = [](uint64 inV1, uint64 inV2) {
		uint64 r = inV1 ^ ~inV2; // Bits that are equal are 1
		r &= r << 1; // Combine bit 0 through 1
		r &= r << 2; // Combine bit 0 through 3
		r &= r << 4; // Combine bit 0 through 7
		r &= 0x8080808080808080UL; // Keep only the highest bit of each byte
		return r;
	};
	return BVec16(equals(inV1.mU64[0], inV2.mU64[0]), equals(inV1.mU64[1], inV2.mU64[1]));
#endif
}

BVec16 BVec16::sOr(BVec16Arg inV1, BVec16Arg inV2)
{
#if defined(JPH_USE_SSE)
	return _mm_or_si128(inV1.mValue, inV2.mValue);
#elif defined(JPH_USE_NEON)
	return vorrq_u8(inV1.mValue, inV2.mValue);
#else
	return BVec16(inV1.mU64[0] | inV2.mU64[0], inV1.mU64[1] | inV2.mU64[1]);
#endif
}

BVec16 BVec16::sXor(BVec16Arg inV1, BVec16Arg inV2)
{
#if defined(JPH_USE_SSE)
	return _mm_xor_si128(inV1.mValue, inV2.mValue);
#elif defined(JPH_USE_NEON)
	return veorq_u8(inV1.mValue, inV2.mValue);
#else
	return BVec16(inV1.mU64[0] ^ inV2.mU64[0], inV1.mU64[1] ^ inV2.mU64[1]);
#endif
}

BVec16 BVec16::sAnd(BVec16Arg inV1, BVec16Arg inV2)
{
#if defined(JPH_USE_SSE)
	return _mm_and_si128(inV1.mValue, inV2.mValue);
#elif defined(JPH_USE_NEON)
	return vandq_u8(inV1.mValue, inV2.mValue);
#else
	return BVec16(inV1.mU64[0] & inV2.mU64[0], inV1.mU64[1] & inV2.mU64[1]);
#endif
}


BVec16 BVec16::sNot(BVec16Arg inV1)
{
#if defined(JPH_USE_SSE)
	return sXor(inV1, sReplicate(0xff));
#elif defined(JPH_USE_NEON)
	return vmvnq_u8(inV1.mValue);
#else
	return BVec16(~inV1.mU64[0], ~inV1.mU64[1]);
#endif
}

int BVec16::GetTrues() const
{
#if defined(JPH_USE_SSE)
	return _mm_movemask_epi8(mValue);
#else
	int result = 0;
	for (int i = 0; i < 16; ++i)
		result |= int(mU8[i] >> 7) << i;
	return result;
#endif
}

bool BVec16::TestAnyTrue() const
{
#if defined(JPH_USE_SSE)
	return _mm_movemask_epi8(mValue) != 0;
#else
	return ((mU64[0] | mU64[1]) & 0x8080808080808080UL) != 0;
#endif
}

bool BVec16::TestAllTrue() const
{
#if defined(JPH_USE_SSE)
	return _mm_movemask_epi8(mValue) == 0b1111111111111111;
#else
	return ((mU64[0] & mU64[1]) & 0x8080808080808080UL) == 0x8080808080808080UL;
#endif
}

JPH_NAMESPACE_END

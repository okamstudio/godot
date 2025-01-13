// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Jolt/Math/Vec4.h>

JPH_NAMESPACE_BEGIN

using HalfFloat = uint16;

// Define half float constant values
static constexpr HalfFloat HALF_FLT_MAX				= 0x7bff;
static constexpr HalfFloat HALF_FLT_MAX_NEGATIVE	= 0xfbff;
static constexpr HalfFloat HALF_FLT_INF				= 0x7c00;
static constexpr HalfFloat HALF_FLT_INF_NEGATIVE	= 0xfc00;
static constexpr HalfFloat HALF_FLT_NANQ			= 0x7e00;
static constexpr HalfFloat HALF_FLT_NANQ_NEGATIVE	= 0xfe00;

namespace HalfFloatConversion {

// Layout of a float
static constexpr int FLOAT_SIGN_POS = 31;
static constexpr int FLOAT_EXPONENT_POS = 23;
static constexpr int FLOAT_EXPONENT_BITS = 8;
static constexpr int FLOAT_EXPONENT_MASK = (1 << FLOAT_EXPONENT_BITS) - 1;
static constexpr int FLOAT_EXPONENT_BIAS = 127;
static constexpr int FLOAT_MANTISSA_BITS = 23;
static constexpr int FLOAT_MANTISSA_MASK = (1 << FLOAT_MANTISSA_BITS) - 1;
static constexpr int FLOAT_EXPONENT_AND_MANTISSA_MASK = FLOAT_MANTISSA_MASK + (FLOAT_EXPONENT_MASK << FLOAT_EXPONENT_POS);

// Layout of half float
static constexpr int HALF_FLT_SIGN_POS = 15;
static constexpr int HALF_FLT_EXPONENT_POS = 10;
static constexpr int HALF_FLT_EXPONENT_BITS = 5;
static constexpr int HALF_FLT_EXPONENT_MASK = (1 << HALF_FLT_EXPONENT_BITS) - 1;
static constexpr int HALF_FLT_EXPONENT_BIAS = 15;
static constexpr int HALF_FLT_MANTISSA_BITS = 10;
static constexpr int HALF_FLT_MANTISSA_MASK = (1 << HALF_FLT_MANTISSA_BITS) - 1;
static constexpr int HALF_FLT_EXPONENT_AND_MANTISSA_MASK = HALF_FLT_MANTISSA_MASK + (HALF_FLT_EXPONENT_MASK << HALF_FLT_EXPONENT_POS);

/// Define half-float rounding modes
enum ERoundingMode
{
	ROUND_TO_NEG_INF,				///< Round to negative infinity
	ROUND_TO_POS_INF,				///< Round to positive infinity
	ROUND_TO_NEAREST,				///< Round to nearest value
};

/// Convert a float (32-bits) to a half float (16-bits), fallback version when no intrinsics available
template <int RoundingMode>
inline HalfFloat FromFloatFallback(float inV)
{
	// Reinterpret the float as an uint32
	uint32 value = BitCast<uint32>(inV);

	// Extract exponent
	uint32 exponent = (value >> FLOAT_EXPONENT_POS) & FLOAT_EXPONENT_MASK;

	// Extract mantissa
	uint32 mantissa = value & FLOAT_MANTISSA_MASK;

	// Extract the sign and move it into the right spot for the half float (so we can just or it in at the end)
	HalfFloat hf_sign = HalfFloat(value >> (FLOAT_SIGN_POS - HALF_FLT_SIGN_POS)) & (1 << HALF_FLT_SIGN_POS);

	// Check NaN or INF
	if (exponent == FLOAT_EXPONENT_MASK) // NaN or INF
		return hf_sign | (mantissa == 0? HALF_FLT_INF : HALF_FLT_NANQ);

	// Rebias the exponent for half floats
	int rebiased_exponent = int(exponent) - FLOAT_EXPONENT_BIAS + HALF_FLT_EXPONENT_BIAS;

	// Check overflow to infinity
	if (rebiased_exponent >= HALF_FLT_EXPONENT_MASK)
	{
		bool round_up = RoundingMode == ROUND_TO_NEAREST || (hf_sign == 0) == (RoundingMode == ROUND_TO_POS_INF);
		return hf_sign | (round_up? HALF_FLT_INF : HALF_FLT_MAX);
	}

	// Check underflow to zero
	if (rebiased_exponent < -HALF_FLT_MANTISSA_BITS)
	{
		bool round_up = RoundingMode != ROUND_TO_NEAREST && (hf_sign == 0) == (RoundingMode == ROUND_TO_POS_INF) && (value & FLOAT_EXPONENT_AND_MANTISSA_MASK) != 0;
		return hf_sign | (round_up? 1 : 0);
	}

	HalfFloat hf_exponent;
	int shift;
	if (rebiased_exponent <= 0)
	{
		// Underflow to denormalized number
		hf_exponent = 0;
		mantissa |= 1 << FLOAT_MANTISSA_BITS; // Add the implicit 1 bit to the mantissa
		shift = FLOAT_MANTISSA_BITS - HALF_FLT_MANTISSA_BITS + 1 - rebiased_exponent;
	}
	else
	{
		// Normal half float
		hf_exponent = HalfFloat(rebiased_exponent << HALF_FLT_EXPONENT_POS);
		shift = FLOAT_MANTISSA_BITS - HALF_FLT_MANTISSA_BITS;
	}

	// Compose the half float
	HalfFloat hf_mantissa = HalfFloat(mantissa >> shift);
	HalfFloat hf = hf_sign | hf_exponent | hf_mantissa;

	// Calculate the remaining bits that we're discarding
	uint remainder = mantissa & ((1 << shift) - 1);

	if constexpr (RoundingMode == ROUND_TO_NEAREST)
	{
		// Round to nearest
		uint round_threshold = 1 << (shift - 1);
		if (remainder > round_threshold // Above threshold, we must always round
			|| (remainder == round_threshold && (hf_mantissa & 1))) // When equal, round to nearest even
			hf++; // May overflow to infinity
	}
	else
	{
		// Round up or down (truncate) depending on the rounding mode
		bool round_up = (hf_sign == 0) == (RoundingMode == ROUND_TO_POS_INF) && remainder != 0;
		if (round_up)
			hf++; // May overflow to infinity
	}

	return hf;
}

/// Convert a float (32-bits) to a half float (16-bits)
template <int RoundingMode>
JPH_INLINE HalfFloat FromFloat(float inV)
{
#ifdef JPH_USE_F16C
	union
	{
		__m128i		u128;
		HalfFloat	u16[8];
	} hf;
	__m128 val = _mm_load_ss(&inV);
	switch (RoundingMode)
	{
	case ROUND_TO_NEG_INF:
		hf.u128 = _mm_cvtps_ph(val, _MM_FROUND_TO_NEG_INF);
		break;
	case ROUND_TO_POS_INF:
		hf.u128 = _mm_cvtps_ph(val, _MM_FROUND_TO_POS_INF);
		break;
	case ROUND_TO_NEAREST:
		hf.u128 = _mm_cvtps_ph(val, _MM_FROUND_TO_NEAREST_INT);
		break;
	}
	return hf.u16[0];
#else
	return FromFloatFallback<RoundingMode>(inV);
#endif
}

/// Convert 4 half floats (lower 64 bits) to floats, fallback version when no intrinsics available
inline Vec4 ToFloatFallback(UVec4Arg inValue)
{
	// Unpack half floats to 4 uint32's
	UVec4 value = inValue.Expand4Uint16Lo();

	// Normal half float path, extract the exponent and mantissa, shift them into place and update the exponent bias
	UVec4 exponent_mantissa = UVec4::sAnd(value, UVec4::sReplicate(HALF_FLT_EXPONENT_AND_MANTISSA_MASK)).LogicalShiftLeft<FLOAT_EXPONENT_POS - HALF_FLT_EXPONENT_POS>() + UVec4::sReplicate((FLOAT_EXPONENT_BIAS - HALF_FLT_EXPONENT_BIAS) << FLOAT_EXPONENT_POS);

	// Denormalized half float path, renormalize the float
	UVec4 exponent_mantissa_denormalized = ((exponent_mantissa + UVec4::sReplicate(1 << FLOAT_EXPONENT_POS)).ReinterpretAsFloat() - UVec4::sReplicate((FLOAT_EXPONENT_BIAS - HALF_FLT_EXPONENT_BIAS + 1) << FLOAT_EXPONENT_POS).ReinterpretAsFloat()).ReinterpretAsInt();

	// NaN / INF path, set all exponent bits
	UVec4 exponent_mantissa_nan_inf = UVec4::sOr(exponent_mantissa, UVec4::sReplicate(FLOAT_EXPONENT_MASK << FLOAT_EXPONENT_POS));

	// Get the exponent to determine which of the paths we should take
	UVec4 exponent_mask = UVec4::sReplicate(HALF_FLT_EXPONENT_MASK << HALF_FLT_EXPONENT_POS);
	UVec4 exponent = UVec4::sAnd(value, exponent_mask);
	UVec4 is_denormalized = UVec4::sEquals(exponent, UVec4::sZero());
	UVec4 is_nan_inf = UVec4::sEquals(exponent, exponent_mask);

	// Select the correct result
	UVec4 result_exponent_mantissa = UVec4::sSelect(UVec4::sSelect(exponent_mantissa, exponent_mantissa_nan_inf, is_nan_inf), exponent_mantissa_denormalized, is_denormalized);

	// Extract the sign bit and shift it to the left
	UVec4 sign = UVec4::sAnd(value, UVec4::sReplicate(1 << HALF_FLT_SIGN_POS)).LogicalShiftLeft<FLOAT_SIGN_POS - HALF_FLT_SIGN_POS>();

	// Construct the float
	return UVec4::sOr(sign, result_exponent_mantissa).ReinterpretAsFloat();
}

/// Convert 4 half floats (lower 64 bits) to floats
JPH_INLINE Vec4 ToFloat(UVec4Arg inValue)
{
#if defined(JPH_USE_F16C)
	return _mm_cvtph_ps(inValue.mValue);
#elif defined(JPH_USE_NEON)
	return vcvt_f32_f16(vreinterpret_f16_u32(vget_low_u32(inValue.mValue)));
#else
	return ToFloatFallback(inValue);
#endif
}

} // HalfFloatConversion

JPH_NAMESPACE_END

// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

JPH_NAMESPACE_BEGIN

/// The constant \f$\pi\f$
static constexpr float JPH_PI = 3.14159265358979323846f;

/// Convert a value from degrees to radians
JPH_INLINE constexpr float DegreesToRadians(float inV)
{
	return inV * (JPH_PI / 180.0f);
}

/// Convert a value from radians to degrees
JPH_INLINE constexpr float RadiansToDegrees(float inV)
{
	return inV * (180.0f / JPH_PI);
}

/// Convert angle in radians to the range \f$[-\pi, \pi]\f$
inline float CenterAngleAroundZero(float inV)
{
	if (inV < -JPH_PI)
	{
		do
			inV += 2.0f * JPH_PI;
		while (inV < -JPH_PI);
	}
	else if (inV > JPH_PI)
	{
		do
			inV -= 2.0f * JPH_PI;
		while (inV > JPH_PI);
	}
	JPH_ASSERT(inV >= -JPH_PI && inV <= JPH_PI);
	return inV;
}

/// Clamp a value between two values
template <typename T>
JPH_INLINE constexpr T Clamp(T inV, T inMin, T inMax)
{
	return min(max(inV, inMin), inMax);
}

/// Square a value
template <typename T>
JPH_INLINE constexpr T Square(T inV)
{
	return inV * inV;
}

/// Returns \f$inV^3\f$.
template <typename T>
JPH_INLINE constexpr T Cubed(T inV)
{
	return inV * inV * inV;
}

/// Get the sign of a value
template <typename T>
JPH_INLINE constexpr T Sign(T inV)
{
	return inV < 0? T(-1) : T(1);
}

/// Check if inV is a power of 2
template <typename T>
constexpr bool IsPowerOf2(T inV)
{
	return inV > 0 && (inV & (inV - 1)) == 0;
}

/// Align inV up to the next inAlignment bytes
template <typename T>
inline T AlignUp(T inV, uint64 inAlignment)
{
	JPH_ASSERT(IsPowerOf2(inAlignment));
	return T((uint64(inV) + inAlignment - 1) & ~(inAlignment - 1));
}

/// Check if inV is inAlignment aligned
template <typename T>
inline bool IsAligned(T inV, uint64 inAlignment)
{
	JPH_ASSERT(IsPowerOf2(inAlignment));
	return (uint64(inV) & (inAlignment - 1)) == 0;
}

/// Compute number of trailing zero bits (how many low bits are zero)
inline uint CountTrailingZeros(uint32 inValue)
{
#if defined(JPH_CPU_X86) || defined(JPH_CPU_WASM)
	#if defined(JPH_USE_TZCNT)
		return _tzcnt_u32(inValue);
	#elif defined(JPH_COMPILER_MSVC)
		if (inValue == 0)
			return 32;
		unsigned long result;
		_BitScanForward(&result, inValue);
		return result;
	#else
		if (inValue == 0)
			return 32;
		return __builtin_ctz(inValue);
	#endif
#elif defined(JPH_CPU_ARM)
	#if defined(JPH_COMPILER_MSVC)
		if (inValue == 0)
			return 32;
		unsigned long result;
		_BitScanForward(&result, inValue);
		return result;
	#else
		if (inValue == 0)
			return 32;
		return __builtin_ctz(inValue);
	#endif
#elif defined(JPH_CPU_E2K) || defined(JPH_CPU_RISCV) || defined(JPH_CPU_PPC) || defined(JPH_CPU_LOONGARCH)
	return inValue ? __builtin_ctz(inValue) : 32;
#else
	#error Undefined
#endif
}

/// Compute the number of leading zero bits (how many high bits are zero)
inline uint CountLeadingZeros(uint32 inValue)
{
#if defined(JPH_CPU_X86) || defined(JPH_CPU_WASM)
	#if defined(JPH_USE_LZCNT)
		return _lzcnt_u32(inValue);
	#elif defined(JPH_COMPILER_MSVC)
		if (inValue == 0)
			return 32;
		unsigned long result;
		_BitScanReverse(&result, inValue);
		return 31 - result;
	#else
		if (inValue == 0)
			return 32;
		return __builtin_clz(inValue);
	#endif
#elif defined(JPH_CPU_ARM)
	#if defined(JPH_COMPILER_MSVC)
		return _CountLeadingZeros(inValue);
	#else
		return __builtin_clz(inValue);
	#endif
#elif defined(JPH_CPU_E2K) || defined(JPH_CPU_RISCV) || defined(JPH_CPU_PPC) || defined(JPH_CPU_LOONGARCH)
	return inValue ? __builtin_clz(inValue) : 32;
#else
	#error Undefined
#endif
}

/// Count the number of 1 bits in a value
inline uint CountBits(uint32 inValue)
{
#if defined(JPH_COMPILER_CLANG) || defined(JPH_COMPILER_GCC)
	return __builtin_popcount(inValue);
#elif defined(JPH_COMPILER_MSVC)
	#if defined(JPH_USE_SSE4_2)
		return _mm_popcnt_u32(inValue);
	#elif defined(JPH_USE_NEON) && (_MSC_VER >= 1930) // _CountOneBits not available on MSVC2019
		return _CountOneBits(inValue);
	#else
		inValue = inValue - ((inValue >> 1) & 0x55555555);
		inValue = (inValue & 0x33333333) + ((inValue >> 2) & 0x33333333);
		inValue = (inValue + (inValue >> 4)) & 0x0F0F0F0F;
		return (inValue * 0x01010101) >> 24;
	#endif
#else
	#error Undefined
#endif
}

/// Get the next higher power of 2 of a value, or the value itself if the value is already a power of 2
inline uint32 GetNextPowerOf2(uint32 inValue)
{
	return inValue <= 1? uint32(1) : uint32(1) << (32 - CountLeadingZeros(inValue - 1));
}

// Simple implementation of C++20 std::bit_cast (unfortunately not constexpr)
template <class To, class From>
JPH_INLINE To BitCast(const From &inValue)
{
	static_assert(std::is_trivially_constructible_v<To>);
	static_assert(sizeof(From) == sizeof(To));

	union FromTo
	{
		To			mTo;
		From		mFrom;
	};

	FromTo convert;
	convert.mFrom = inValue;
	return convert.mTo;
}

JPH_NAMESPACE_END

// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2023 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

JPH_NAMESPACE_BEGIN

/// Construct a string that lists the most important configuration settings
inline const char *GetConfigurationString()
{
	return JPH_IF_SINGLE_PRECISION_ELSE("Single", "Double") " precision "
#if defined(JPH_CPU_X86)
		"x86 "
#elif defined(JPH_CPU_ARM)
		"ARM "
#elif defined(JPH_CPU_RISCV)
		"RISC-V "
#elif defined(JPH_CPU_PPC)
		"PowerPC "
	#ifdef JPH_CPU_BIG_ENDIAN
		"(Big Endian) "
	#else
		"(Little Endian) "
	#endif
#elif defined(JPH_CPU_LOONGARCH)
		"LoongArch "
#elif defined(JPH_CPU_E2K)
		"E2K "
#elif defined(JPH_CPU_WASM)
		"WASM "
#else
	#error Unknown CPU architecture
#endif
#if JPH_CPU_ADDRESS_BITS == 64
		"64-bit "
#elif JPH_CPU_ADDRESS_BITS == 32
		"32-bit "
#endif
		"with instructions: "
#ifdef JPH_USE_NEON
		"NEON "
#endif
#ifdef JPH_USE_SSE
		"SSE2 "
#endif
#ifdef JPH_USE_SSE4_1
		"SSE4.1 "
#endif
#ifdef JPH_USE_SSE4_2
		"SSE4.2 "
#endif
#ifdef JPH_USE_AVX
		"AVX "
#endif
#ifdef JPH_USE_AVX2
		"AVX2 "
#endif
#ifdef JPH_USE_AVX512
		"AVX512 "
#endif
#ifdef JPH_USE_F16C
		"F16C "
#endif
#ifdef JPH_USE_LZCNT
		"LZCNT "
#endif
#ifdef JPH_USE_TZCNT
		"TZCNT "
#endif
#ifdef JPH_USE_FMADD
		"FMADD "
#endif
#ifdef JPH_CROSS_PLATFORM_DETERMINISTIC
		"(Cross Platform Deterministic) "
#endif
#ifdef JPH_FLOATING_POINT_EXCEPTIONS_ENABLED
		"(FP Exceptions) "
#endif
#ifdef JPH_DEBUG_RENDERER
		"(Debug Renderer) "
#endif
#ifdef JPH_PROFILE_ENABLED
		"(Profile) "
#endif
#if defined(JPH_OBJECT_LAYER_BITS) && JPH_OBJECT_LAYER_BITS == 32
		"(32-bit ObjectLayer) "
#else
		"(16-bit ObjectLayer) "
#endif
#ifdef JPH_ENABLE_ASSERTS
		"(Assertions) "
#endif
#ifdef JPH_OBJECT_STREAM
		"(ObjectStream) "
#endif
#ifdef JPH_DEBUG
		"(Debug) "
#endif
#if defined(__cpp_rtti) && __cpp_rtti
		"(C++ RTTI) "
#endif
#if defined(__cpp_exceptions) && __cpp_exceptions
		"(C++ Exceptions) "
#endif
		;
}

JPH_NAMESPACE_END

// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

// Include for __rdtsc
#if defined(JPH_PLATFORM_WINDOWS)
	#include <intrin.h>
#elif defined(JPH_CPU_X86) && defined(JPH_COMPILER_GCC)
	#include <x86intrin.h>
#elif defined(JPH_CPU_E2K)
	#include <x86intrin.h>
#endif

JPH_NAMESPACE_BEGIN

#if defined(JPH_PLATFORM_WINDOWS_UWP) || (defined(JPH_PLATFORM_WINDOWS) && defined(JPH_CPU_ARM))

/// Functionality to get the processors cycle counter
uint64 GetProcessorTickCount(); // Not inline to avoid having to include Windows.h

#else

/// Functionality to get the processors cycle counter
JPH_INLINE uint64 GetProcessorTickCount()
{
#if defined(JPH_PLATFORM_BLUE)
	return JPH_PLATFORM_BLUE_GET_TICKS();
#elif defined(JPH_CPU_X86)
	return __rdtsc();
#elif defined(JPH_CPU_E2K)
	return __rdtsc();
#elif defined(JPH_CPU_ARM) && defined(JPH_USE_NEON)
	uint64 val;
	asm volatile("mrs %0, cntvct_el0" : "=r" (val));
	return val;
#elif defined(JPH_CPU_ARM) || defined(JPH_CPU_RISCV) || defined(JPH_CPU_WASM) || defined(JPH_CPU_PPC) || defined(JPH_CPU_LOONGARCH)
	return 0; // Not supported
#else
	#error Undefined
#endif
}

#endif // JPH_PLATFORM_WINDOWS_UWP || (JPH_PLATFORM_WINDOWS && JPH_CPU_ARM)

JPH_NAMESPACE_END

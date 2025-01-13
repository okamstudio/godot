// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#include <Jolt/Jolt.h>

#include <Jolt/Core/TickCounter.h>

#if defined(JPH_PLATFORM_WINDOWS)
	JPH_SUPPRESS_WARNING_PUSH
	JPH_MSVC_SUPPRESS_WARNING(5039) // winbase.h(13179): warning C5039: 'TpSetCallbackCleanupGroup': pointer or reference to potentially throwing function passed to 'extern "C"' function under -EHc. Undefined behavior may occur if this function throws an exception.
	#ifndef WIN32_LEAN_AND_MEAN
		#define WIN32_LEAN_AND_MEAN
	#endif
#ifndef JPH_COMPILER_MINGW
	#include <Windows.h>
#else
	#include <windows.h>
#endif
	JPH_SUPPRESS_WARNING_POP
#endif

JPH_NAMESPACE_BEGIN

#if defined(JPH_PLATFORM_WINDOWS_UWP) || (defined(JPH_PLATFORM_WINDOWS) && defined(JPH_CPU_ARM))

uint64 GetProcessorTickCount()
{
	LARGE_INTEGER count;
	QueryPerformanceCounter(&count);
	return uint64(count.QuadPart);
}

#endif // JPH_PLATFORM_WINDOWS_UWP || (JPH_PLATFORM_WINDOWS && JPH_CPU_ARM)

JPH_NAMESPACE_END

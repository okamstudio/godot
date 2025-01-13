// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

// Jolt library version
#define JPH_VERSION_MAJOR 5
#define JPH_VERSION_MINOR 2
#define JPH_VERSION_PATCH 1

// Determine which features the library was compiled with
#ifdef JPH_DOUBLE_PRECISION
	#define JPH_VERSION_FEATURE_BIT_1 1
#else
	#define JPH_VERSION_FEATURE_BIT_1 0
#endif
#ifdef JPH_CROSS_PLATFORM_DETERMINISTIC
	#define JPH_VERSION_FEATURE_BIT_2 1
#else
	#define JPH_VERSION_FEATURE_BIT_2 0
#endif
#ifdef JPH_FLOATING_POINT_EXCEPTIONS_ENABLED
	#define JPH_VERSION_FEATURE_BIT_3 1
#else
	#define JPH_VERSION_FEATURE_BIT_3 0
#endif
#ifdef JPH_PROFILE_ENABLED
	#define JPH_VERSION_FEATURE_BIT_4 1
#else
	#define JPH_VERSION_FEATURE_BIT_4 0
#endif
#ifdef JPH_EXTERNAL_PROFILE
	#define JPH_VERSION_FEATURE_BIT_5 1
#else
	#define JPH_VERSION_FEATURE_BIT_5 0
#endif
#ifdef JPH_DEBUG_RENDERER
	#define JPH_VERSION_FEATURE_BIT_6 1
#else
	#define JPH_VERSION_FEATURE_BIT_6 0
#endif
#ifdef JPH_DISABLE_TEMP_ALLOCATOR
	#define JPH_VERSION_FEATURE_BIT_7 1
#else
	#define JPH_VERSION_FEATURE_BIT_7 0
#endif
#ifdef JPH_DISABLE_CUSTOM_ALLOCATOR
	#define JPH_VERSION_FEATURE_BIT_8 1
#else
	#define JPH_VERSION_FEATURE_BIT_8 0
#endif
#if defined(JPH_OBJECT_LAYER_BITS) && JPH_OBJECT_LAYER_BITS == 32
	#define JPH_VERSION_FEATURE_BIT_9 1
#else
	#define JPH_VERSION_FEATURE_BIT_9 0
#endif
#ifdef JPH_ENABLE_ASSERTS
	#define JPH_VERSION_FEATURE_BIT_10 1
#else
	#define JPH_VERSION_FEATURE_BIT_10 0
#endif
#ifdef JPH_OBJECT_STREAM
	#define JPH_VERSION_FEATURE_BIT_11 1
#else
	#define JPH_VERSION_FEATURE_BIT_11 0
#endif
#define JPH_VERSION_FEATURES (uint64(JPH_VERSION_FEATURE_BIT_1) | (JPH_VERSION_FEATURE_BIT_2 << 1) | (JPH_VERSION_FEATURE_BIT_3 << 2) | (JPH_VERSION_FEATURE_BIT_4 << 3) | (JPH_VERSION_FEATURE_BIT_5 << 4) | (JPH_VERSION_FEATURE_BIT_6 << 5) | (JPH_VERSION_FEATURE_BIT_7 << 6) | (JPH_VERSION_FEATURE_BIT_8 << 7) | (JPH_VERSION_FEATURE_BIT_9 << 8) | (JPH_VERSION_FEATURE_BIT_10 << 9) | (JPH_VERSION_FEATURE_BIT_11 << 10))

// Combine the version and features in a single ID
#define JPH_VERSION_ID ((JPH_VERSION_FEATURES << 24) | (JPH_VERSION_MAJOR << 16) | (JPH_VERSION_MINOR << 8) | JPH_VERSION_PATCH)

// Determine platform
#if defined(JPH_PLATFORM_BLUE)
	// Correct define already defined, this overrides everything else
#elif defined(_WIN32) || defined(_WIN64)
	#include <winapifamily.h>
	#if WINAPI_FAMILY == WINAPI_FAMILY_APP
		#define JPH_PLATFORM_WINDOWS_UWP // Building for Universal Windows Platform
	#endif
	#define JPH_PLATFORM_WINDOWS
#elif defined(__ANDROID__) // Android is linux too, so that's why we check it first
	#define JPH_PLATFORM_ANDROID
#elif defined(__linux__)
	#define JPH_PLATFORM_LINUX
#elif defined(__FreeBSD__)
	#define JPH_PLATFORM_FREEBSD
#elif defined(__APPLE__)
	#include <TargetConditionals.h>
	#if defined(TARGET_OS_IPHONE) && !TARGET_OS_IPHONE
		#define JPH_PLATFORM_MACOS
	#else
		#define JPH_PLATFORM_IOS
	#endif
#elif defined(__EMSCRIPTEN__)
	#define JPH_PLATFORM_WASM
#endif

// Platform helper macros
#ifdef JPH_PLATFORM_ANDROID
	#define JPH_IF_NOT_ANDROID(x)
#else
	#define JPH_IF_NOT_ANDROID(x) x
#endif

// Determine compiler
#if defined(__clang__)
	#define JPH_COMPILER_CLANG
#elif defined(__GNUC__)
	#define JPH_COMPILER_GCC
#elif defined(_MSC_VER)
	#define JPH_COMPILER_MSVC
#endif

#if defined(__MINGW64__) || defined (__MINGW32__)
	#define JPH_COMPILER_MINGW
#endif

// Detect CPU architecture
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
	// X86 CPU architecture
	#define JPH_CPU_X86
	#if defined(__x86_64__) || defined(_M_X64)
		#define JPH_CPU_ADDRESS_BITS 64
	#else
		#define JPH_CPU_ADDRESS_BITS 32
	#endif
	#define JPH_USE_SSE
	#define JPH_VECTOR_ALIGNMENT 16
	#define JPH_DVECTOR_ALIGNMENT 32

	// Detect enabled instruction sets
	#if defined(__AVX512F__) && defined(__AVX512VL__) && defined(__AVX512DQ__) && !defined(JPH_USE_AVX512)
		#define JPH_USE_AVX512
	#endif
	#if (defined(__AVX2__) || defined(JPH_USE_AVX512)) && !defined(JPH_USE_AVX2)
		#define JPH_USE_AVX2
	#endif
	#if (defined(__AVX__) || defined(JPH_USE_AVX2)) && !defined(JPH_USE_AVX)
		#define JPH_USE_AVX
	#endif
	#if (defined(__SSE4_2__) || defined(JPH_USE_AVX)) && !defined(JPH_USE_SSE4_2)
		#define JPH_USE_SSE4_2
	#endif
	#if (defined(__SSE4_1__) || defined(JPH_USE_SSE4_2)) && !defined(JPH_USE_SSE4_1)
		#define JPH_USE_SSE4_1
	#endif
	#if (defined(__F16C__) || defined(JPH_USE_AVX2)) && !defined(JPH_USE_F16C)
		#define JPH_USE_F16C
	#endif
	#if (defined(__LZCNT__) || defined(JPH_USE_AVX2)) && !defined(JPH_USE_LZCNT)
		#define JPH_USE_LZCNT
	#endif
	#if (defined(__BMI__) || defined(JPH_USE_AVX2)) && !defined(JPH_USE_TZCNT)
		#define JPH_USE_TZCNT
	#endif
	#ifndef JPH_CROSS_PLATFORM_DETERMINISTIC // FMA is not compatible with cross platform determinism
		#if defined(JPH_COMPILER_CLANG) || defined(JPH_COMPILER_GCC)
			#if defined(__FMA__) && !defined(JPH_USE_FMADD)
				#define JPH_USE_FMADD
			#endif
		#elif defined(JPH_COMPILER_MSVC)
			#if defined(__AVX2__) && !defined(JPH_USE_FMADD) // AVX2 also enables fused multiply add
				#define JPH_USE_FMADD
			#endif
		#else
			#error Undefined compiler
		#endif
	#endif
#elif defined(__aarch64__) || defined(_M_ARM64) || defined(__arm__) || defined(_M_ARM)
	// ARM CPU architecture
	#define JPH_CPU_ARM
	#if defined(__aarch64__) || defined(_M_ARM64)
		#define JPH_CPU_ADDRESS_BITS 64
		#define JPH_USE_NEON
		#define JPH_VECTOR_ALIGNMENT 16
		#define JPH_DVECTOR_ALIGNMENT 32
	#else
		#define JPH_CPU_ADDRESS_BITS 32
		#define JPH_VECTOR_ALIGNMENT 8 // 32-bit ARM does not support aligning on the stack on 16 byte boundaries
		#define JPH_DVECTOR_ALIGNMENT 8
	#endif
#elif defined(JPH_PLATFORM_WASM)
	// WebAssembly CPU architecture
	#define JPH_CPU_WASM
	#define JPH_CPU_ADDRESS_BITS 32
	#define JPH_VECTOR_ALIGNMENT 16
	#define JPH_DVECTOR_ALIGNMENT 32
	#ifdef __wasm_simd128__
		#define JPH_USE_SSE
		#define JPH_USE_SSE4_1
		#define JPH_USE_SSE4_2
	#endif
#elif defined(__e2k__)
	// E2K CPU architecture (MCST Elbrus 2000)
	#define JPH_CPU_E2K
	#define JPH_CPU_ADDRESS_BITS 64
	#define JPH_VECTOR_ALIGNMENT 16
	#define JPH_DVECTOR_ALIGNMENT 32

	// Compiler flags on e2k arch determine CPU features
	#if defined(__SSE__) && !defined(JPH_USE_SSE)
		#define JPH_USE_SSE
	#endif
#else
	#error Unsupported CPU architecture
#endif

// If this define is set, Jolt is compiled as a shared library
#ifdef JPH_SHARED_LIBRARY
	#ifdef JPH_BUILD_SHARED_LIBRARY
		// While building the shared library, we must export these symbols
		#if defined(JPH_PLATFORM_WINDOWS) && !defined(JPH_COMPILER_MINGW)
			#define JPH_EXPORT __declspec(dllexport)
		#else
			#define JPH_EXPORT __attribute__ ((visibility ("default")))
			#if defined(JPH_COMPILER_GCC)
				// Prevents an issue with GCC attribute parsing (see https://gcc.gnu.org/bugzilla/show_bug.cgi?id=69585)
				#define JPH_EXPORT_GCC_BUG_WORKAROUND [[gnu::visibility("default")]]
			#endif
		#endif
	#else
		// When linking against Jolt, we must import these symbols
		#if defined(JPH_PLATFORM_WINDOWS) && !defined(JPH_COMPILER_MINGW)
			#define JPH_EXPORT __declspec(dllimport)
		#else
			#define JPH_EXPORT __attribute__ ((visibility ("default")))
			#if defined(JPH_COMPILER_GCC)
				// Prevents an issue with GCC attribute parsing (see https://gcc.gnu.org/bugzilla/show_bug.cgi?id=69585)
				#define JPH_EXPORT_GCC_BUG_WORKAROUND [[gnu::visibility("default")]]
			#endif
		#endif
	#endif
#else
	// If the define is not set, we use static linking and symbols don't need to be imported or exported
	#define JPH_EXPORT
#endif

#ifndef JPH_EXPORT_GCC_BUG_WORKAROUND
	#define JPH_EXPORT_GCC_BUG_WORKAROUND JPH_EXPORT
#endif

// Macro used by the RTTI macros to not export a function
#define JPH_NO_EXPORT

// Pragmas to store / restore the warning state and to disable individual warnings
#ifdef JPH_COMPILER_CLANG
#define JPH_PRAGMA(x)					_Pragma(#x)
#define JPH_SUPPRESS_WARNING_PUSH		JPH_PRAGMA(clang diagnostic push)
#define JPH_SUPPRESS_WARNING_POP		JPH_PRAGMA(clang diagnostic pop)
#define JPH_CLANG_SUPPRESS_WARNING(w)	JPH_PRAGMA(clang diagnostic ignored w)
#if __clang_major__ >= 13
	#define JPH_CLANG_13_PLUS_SUPPRESS_WARNING(w) JPH_CLANG_SUPPRESS_WARNING(w)
#else
	#define JPH_CLANG_13_PLUS_SUPPRESS_WARNING(w)
#endif
#if __clang_major__ >= 16
	#define JPH_CLANG_16_PLUS_SUPPRESS_WARNING(w) JPH_CLANG_SUPPRESS_WARNING(w)
#else
	#define JPH_CLANG_16_PLUS_SUPPRESS_WARNING(w)
#endif
#else
#define JPH_CLANG_SUPPRESS_WARNING(w)
#define JPH_CLANG_13_PLUS_SUPPRESS_WARNING(w)
#define JPH_CLANG_16_PLUS_SUPPRESS_WARNING(w)
#endif
#ifdef JPH_COMPILER_GCC
#define JPH_PRAGMA(x)					_Pragma(#x)
#define JPH_SUPPRESS_WARNING_PUSH		JPH_PRAGMA(GCC diagnostic push)
#define JPH_SUPPRESS_WARNING_POP		JPH_PRAGMA(GCC diagnostic pop)
#define JPH_GCC_SUPPRESS_WARNING(w)		JPH_PRAGMA(GCC diagnostic ignored w)
#else
#define JPH_GCC_SUPPRESS_WARNING(w)
#endif
#ifdef JPH_COMPILER_MSVC
#define JPH_PRAGMA(x)					__pragma(x)
#define JPH_SUPPRESS_WARNING_PUSH		JPH_PRAGMA(warning (push))
#define JPH_SUPPRESS_WARNING_POP		JPH_PRAGMA(warning (pop))
#define JPH_MSVC_SUPPRESS_WARNING(w)	JPH_PRAGMA(warning (disable : w))
#if _MSC_VER >= 1920 && _MSC_VER < 1930
	#define JPH_MSVC2019_SUPPRESS_WARNING(w) JPH_MSVC_SUPPRESS_WARNING(w)
#else
	#define JPH_MSVC2019_SUPPRESS_WARNING(w)
#endif
#else
#define JPH_MSVC_SUPPRESS_WARNING(w)
#define JPH_MSVC2019_SUPPRESS_WARNING(w)
#endif

// Disable common warnings triggered by Jolt when compiling with -Wall
#define JPH_SUPPRESS_WARNINGS																	\
	JPH_CLANG_SUPPRESS_WARNING("-Wc++98-compat")												\
	JPH_CLANG_SUPPRESS_WARNING("-Wc++98-compat-pedantic")										\
	JPH_CLANG_SUPPRESS_WARNING("-Wfloat-equal")													\
	JPH_CLANG_SUPPRESS_WARNING("-Wsign-conversion")												\
	JPH_CLANG_SUPPRESS_WARNING("-Wold-style-cast")												\
	JPH_CLANG_SUPPRESS_WARNING("-Wgnu-anonymous-struct")										\
	JPH_CLANG_SUPPRESS_WARNING("-Wnested-anon-types")											\
	JPH_CLANG_SUPPRESS_WARNING("-Wglobal-constructors")											\
	JPH_CLANG_SUPPRESS_WARNING("-Wexit-time-destructors")										\
	JPH_CLANG_SUPPRESS_WARNING("-Wnonportable-system-include-path")								\
	JPH_CLANG_SUPPRESS_WARNING("-Wlanguage-extension-token")									\
	JPH_CLANG_SUPPRESS_WARNING("-Wunused-parameter")											\
	JPH_CLANG_SUPPRESS_WARNING("-Wformat-nonliteral")											\
	JPH_CLANG_SUPPRESS_WARNING("-Wcovered-switch-default")										\
	JPH_CLANG_SUPPRESS_WARNING("-Wcast-align")													\
	JPH_CLANG_SUPPRESS_WARNING("-Winvalid-offsetof")											\
	JPH_CLANG_SUPPRESS_WARNING("-Wgnu-zero-variadic-macro-arguments")							\
	JPH_CLANG_SUPPRESS_WARNING("-Wdocumentation-unknown-command")								\
	JPH_CLANG_SUPPRESS_WARNING("-Wctad-maybe-unsupported")										\
	JPH_CLANG_SUPPRESS_WARNING("-Wswitch-default")												\
	JPH_CLANG_13_PLUS_SUPPRESS_WARNING("-Wdeprecated-copy")										\
	JPH_CLANG_13_PLUS_SUPPRESS_WARNING("-Wdeprecated-copy-with-dtor")							\
	JPH_CLANG_16_PLUS_SUPPRESS_WARNING("-Wunsafe-buffer-usage")									\
	JPH_IF_NOT_ANDROID(JPH_CLANG_SUPPRESS_WARNING("-Wimplicit-int-float-conversion"))			\
																								\
	JPH_GCC_SUPPRESS_WARNING("-Wcomment")														\
	JPH_GCC_SUPPRESS_WARNING("-Winvalid-offsetof")												\
	JPH_GCC_SUPPRESS_WARNING("-Wclass-memaccess")												\
	JPH_GCC_SUPPRESS_WARNING("-Wpedantic")														\
	JPH_GCC_SUPPRESS_WARNING("-Wunused-parameter")												\
	JPH_GCC_SUPPRESS_WARNING("-Wmaybe-uninitialized")											\
																								\
	JPH_MSVC_SUPPRESS_WARNING(4619) /* #pragma warning: there is no warning number 'XXXX' */	\
	JPH_MSVC_SUPPRESS_WARNING(4514) /* 'X' : unreferenced inline function has been removed */	\
	JPH_MSVC_SUPPRESS_WARNING(4710) /* 'X' : function not inlined */							\
	JPH_MSVC_SUPPRESS_WARNING(4711) /* function 'X' selected for automatic inline expansion */	\
	JPH_MSVC_SUPPRESS_WARNING(4820) /* 'X': 'Y' bytes padding added after data member 'Z' */	\
	JPH_MSVC_SUPPRESS_WARNING(4100) /* 'X' : unreferenced formal parameter */					\
	JPH_MSVC_SUPPRESS_WARNING(4626) /* 'X' : assignment operator was implicitly defined as deleted because a base class assignment operator is inaccessible or deleted */ \
	JPH_MSVC_SUPPRESS_WARNING(5027) /* 'X' : move assignment operator was implicitly defined as deleted because a base class move assignment operator is inaccessible or deleted */ \
	JPH_MSVC_SUPPRESS_WARNING(4365) /* 'argument' : conversion from 'X' to 'Y', signed / unsigned mismatch */ \
	JPH_MSVC_SUPPRESS_WARNING(4324) /* 'X' : structure was padded due to alignment specifier */ \
	JPH_MSVC_SUPPRESS_WARNING(4625) /* 'X' : copy constructor was implicitly defined as deleted because a base class copy constructor is inaccessible or deleted */ \
	JPH_MSVC_SUPPRESS_WARNING(5026) /* 'X': move constructor was implicitly defined as deleted because a base class move constructor is inaccessible or deleted */ \
	JPH_MSVC_SUPPRESS_WARNING(4623) /* 'X' : default constructor was implicitly defined as deleted */ \
	JPH_MSVC_SUPPRESS_WARNING(4201) /* nonstandard extension used: nameless struct/union */		\
	JPH_MSVC_SUPPRESS_WARNING(4371) /* 'X': layout of class may have changed from a previous version of the compiler due to better packing of member 'Y' */ \
	JPH_MSVC_SUPPRESS_WARNING(5045) /* Compiler will insert Spectre mitigation for memory load if /Qspectre switch specified */ \
	JPH_MSVC_SUPPRESS_WARNING(4583) /* 'X': destructor is not implicitly called */				\
	JPH_MSVC_SUPPRESS_WARNING(4582) /* 'X': constructor is not implicitly called */				\
	JPH_MSVC_SUPPRESS_WARNING(5219) /* implicit conversion from 'X' to 'Y', possible loss of data  */ \
	JPH_MSVC_SUPPRESS_WARNING(4826) /* Conversion from 'X *' to 'JPH::uint64' is sign-extended. This may cause unexpected runtime behavior. (32-bit) */ \
	JPH_MSVC_SUPPRESS_WARNING(5264) /* 'X': 'const' variable is not used */						\
	JPH_MSVC_SUPPRESS_WARNING(4251) /* class 'X' needs to have DLL-interface to be used by clients of class 'Y' */ \
	JPH_MSVC_SUPPRESS_WARNING(4738) /* storing 32-bit float result in memory, possible loss of performance */ \
	JPH_MSVC2019_SUPPRESS_WARNING(5246) /* the initialization of a subobject should be wrapped in braces */

// OS-specific includes
#if defined(JPH_PLATFORM_WINDOWS)
	#define JPH_BREAKPOINT		__debugbreak()
#elif defined(JPH_PLATFORM_BLUE)
	// Configuration for a popular game console.
	// This file is not distributed because it would violate an NDA.
	// Creating one should only be a couple of minutes of work if you have the documentation for the platform
	// (you only need to define JPH_BREAKPOINT, JPH_PLATFORM_BLUE_GET_TICKS, JPH_PLATFORM_BLUE_MUTEX*, JPH_PLATFORM_BLUE_RWLOCK* and include the right header).
	#include <Jolt/Core/PlatformBlue.h>
#elif defined(JPH_PLATFORM_LINUX) || defined(JPH_PLATFORM_ANDROID) || defined(JPH_PLATFORM_MACOS) || defined(JPH_PLATFORM_IOS) || defined(JPH_PLATFORM_FREEBSD)
	#if defined(JPH_CPU_X86)
		#define JPH_BREAKPOINT	__asm volatile ("int $0x3")
	#elif defined(JPH_CPU_ARM)
		#define JPH_BREAKPOINT	__builtin_trap()
	#elif defined(JPH_CPU_E2K)
		#define JPH_BREAKPOINT	__builtin_trap()
	#endif
#elif defined(JPH_PLATFORM_WASM)
	#define JPH_BREAKPOINT		do { } while (false) // Not supported
#else
	#error Unknown platform
#endif

// Begin the JPH namespace
#define JPH_NAMESPACE_BEGIN																		\
	JPH_SUPPRESS_WARNING_PUSH																	\
	JPH_SUPPRESS_WARNINGS																		\
	namespace JPH {

// End the JPH namespace
#define JPH_NAMESPACE_END																		\
	}																							\
	JPH_SUPPRESS_WARNING_POP

// Suppress warnings generated by the standard template library
#define JPH_SUPPRESS_WARNINGS_STD_BEGIN															\
	JPH_SUPPRESS_WARNING_PUSH																	\
	JPH_MSVC_SUPPRESS_WARNING(4365)																\
	JPH_MSVC_SUPPRESS_WARNING(4619)																\
	JPH_MSVC_SUPPRESS_WARNING(4710)																\
	JPH_MSVC_SUPPRESS_WARNING(4711)																\
	JPH_MSVC_SUPPRESS_WARNING(4820)																\
	JPH_MSVC_SUPPRESS_WARNING(4514)																\
	JPH_MSVC_SUPPRESS_WARNING(5262)																\
	JPH_MSVC_SUPPRESS_WARNING(5264)																\
	JPH_MSVC_SUPPRESS_WARNING(4738)

#define JPH_SUPPRESS_WARNINGS_STD_END															\
	JPH_SUPPRESS_WARNING_POP

// Standard C++ includes
JPH_SUPPRESS_WARNINGS_STD_BEGIN
#include <float.h>
#include <limits.h>
#include <string.h>
#include <utility>
#include <cmath>
#include <sstream>
#include <functional>
#include <algorithm>
#include <cstdint>
#ifdef JPH_COMPILER_MSVC
	#include <malloc.h> // for alloca
#endif
#if defined(JPH_USE_SSE)
	#include <immintrin.h>
#elif defined(JPH_USE_NEON)
	#ifdef JPH_COMPILER_MSVC
		#include <intrin.h>
		#include <arm64_neon.h>
	#else
		#include <arm_neon.h>
	#endif
#endif
JPH_SUPPRESS_WARNINGS_STD_END

JPH_NAMESPACE_BEGIN

// Commonly used STL types
using std::min;
using std::max;
using std::abs;
using std::sqrt;
using std::ceil;
using std::floor;
using std::trunc;
using std::round;
using std::fmod;
using std::string_view;
using std::function;
using std::numeric_limits;
using std::isfinite;
using std::isnan;
using std::ostream;
using std::istream;

// Standard types
using uint = unsigned int;
using uint8 = std::uint8_t;
using uint16 = std::uint16_t;
using uint32 = std::uint32_t;
using uint64 = std::uint64_t;

// Assert sizes of types
static_assert(sizeof(uint) >= 4, "Invalid size of uint");
static_assert(sizeof(uint8) == 1, "Invalid size of uint8");
static_assert(sizeof(uint16) == 2, "Invalid size of uint16");
static_assert(sizeof(uint32) == 4, "Invalid size of uint32");
static_assert(sizeof(uint64) == 8, "Invalid size of uint64");
static_assert(sizeof(void *) == (JPH_CPU_ADDRESS_BITS == 64? 8 : 4), "Invalid size of pointer" );

// Determine if we want extra debugging code to be active
#if !defined(NDEBUG) && !defined(JPH_NO_DEBUG)
	#define JPH_DEBUG
#endif

// Define inline macro
#if defined(JPH_NO_FORCE_INLINE)
	#define JPH_INLINE inline
#elif defined(JPH_COMPILER_CLANG)
	#define JPH_INLINE __inline__ __attribute__((always_inline))
#elif defined(JPH_COMPILER_GCC)
	// On gcc 14 using always_inline in debug mode causes error: "inlining failed in call to 'always_inline' 'XXX': function not considered for inlining"
	// See: https://github.com/jrouwe/JoltPhysics/issues/1096
	#if __GNUC__ >= 14 && defined(JPH_DEBUG)
		#define JPH_INLINE inline
	#else
		#define JPH_INLINE __inline__ __attribute__((always_inline))
	#endif
#elif defined(JPH_COMPILER_MSVC)
	#define JPH_INLINE __forceinline
#else
	#error Undefined
#endif

// Cache line size (used for aligning to cache line)
#ifndef JPH_CACHE_LINE_SIZE
	#define JPH_CACHE_LINE_SIZE 64
#endif

// Define macro to get current function name
#if defined(JPH_COMPILER_CLANG) || defined(JPH_COMPILER_GCC)
	#define JPH_FUNCTION_NAME	__PRETTY_FUNCTION__
#elif defined(JPH_COMPILER_MSVC)
	#define JPH_FUNCTION_NAME	__FUNCTION__
#else
	#error Undefined
#endif

// Stack allocation
#define JPH_STACK_ALLOC(n)		alloca(n)

// Shorthand for #ifdef JPH_DEBUG / #endif
#ifdef JPH_DEBUG
	#define JPH_IF_DEBUG(...)	__VA_ARGS__
	#define JPH_IF_NOT_DEBUG(...)
#else
	#define JPH_IF_DEBUG(...)
	#define JPH_IF_NOT_DEBUG(...) __VA_ARGS__
#endif

// Shorthand for #ifdef JPH_FLOATING_POINT_EXCEPTIONS_ENABLED / #endif
#ifdef JPH_FLOATING_POINT_EXCEPTIONS_ENABLED
	#define JPH_IF_FLOATING_POINT_EXCEPTIONS_ENABLED(...)	__VA_ARGS__
#else
	#define JPH_IF_FLOATING_POINT_EXCEPTIONS_ENABLED(...)
#endif

// Helper macros to detect if we're running in single or double precision mode
#ifdef JPH_DOUBLE_PRECISION
	#define JPH_IF_SINGLE_PRECISION(...)
	#define JPH_IF_SINGLE_PRECISION_ELSE(s, d) d
	#define JPH_IF_DOUBLE_PRECISION(...) __VA_ARGS__
#else
	#define JPH_IF_SINGLE_PRECISION(...) __VA_ARGS__
	#define JPH_IF_SINGLE_PRECISION_ELSE(s, d) s
	#define JPH_IF_DOUBLE_PRECISION(...)
#endif

// Helper macro to detect if the debug renderer is active
#ifdef JPH_DEBUG_RENDERER
	#define JPH_IF_DEBUG_RENDERER(...) __VA_ARGS__
	#define JPH_IF_NOT_DEBUG_RENDERER(...)
#else
	#define JPH_IF_DEBUG_RENDERER(...)
	#define JPH_IF_NOT_DEBUG_RENDERER(...) __VA_ARGS__
#endif

// Macro to indicate that a parameter / variable is unused
#define JPH_UNUSED(x)			(void)x

// Macro to enable floating point precise mode and to disable fused multiply add instructions
#if defined(JPH_COMPILER_GCC) || defined(JPH_CROSS_PLATFORM_DETERMINISTIC)
	// We compile without -ffast-math and -ffp-contract=fast, so we don't need to disable anything
	#define JPH_PRECISE_MATH_ON
	#define JPH_PRECISE_MATH_OFF
#elif defined(JPH_COMPILER_CLANG)
	// We compile without -ffast-math because pragma float_control(precise, on) doesn't seem to actually negate all of the -ffast-math effects and causes the unit tests to fail (even if the pragma is added to all files)
	// On clang 14 and later we can turn off float contraction through a pragma (before it was buggy), so if FMA is on we can disable it through this macro
	#if (defined(JPH_CPU_ARM) && !defined(JPH_PLATFORM_ANDROID) && __clang_major__ >= 16) || (defined(JPH_CPU_X86) && __clang_major__ >= 14)
		#define JPH_PRECISE_MATH_ON						\
			_Pragma("float_control(precise, on, push)")	\
			_Pragma("clang fp contract(off)")
		#define JPH_PRECISE_MATH_OFF					\
			_Pragma("float_control(pop)")
	#elif __clang_major__ >= 14 && (defined(JPH_USE_FMADD) || defined(FP_FAST_FMA))
		#define JPH_PRECISE_MATH_ON						\
			_Pragma("clang fp contract(off)")
		#define JPH_PRECISE_MATH_OFF					\
			_Pragma("clang fp contract(on)")
	#else
		#define JPH_PRECISE_MATH_ON
		#define JPH_PRECISE_MATH_OFF
	#endif
#elif defined(JPH_COMPILER_MSVC)
	// Unfortunately there is no way to push the state of fp_contract, so we have to assume it was turned on before JPH_PRECISE_MATH_ON
	#define JPH_PRECISE_MATH_ON							\
		__pragma(float_control(precise, on, push))		\
		__pragma(fp_contract(off))
	#define JPH_PRECISE_MATH_OFF						\
		__pragma(fp_contract(on))						\
		__pragma(float_control(pop))
#else
	#error Undefined
#endif

// Check if Thread Sanitizer is enabled
#ifdef __has_feature
	#if __has_feature(thread_sanitizer)
		#define JPH_TSAN_ENABLED
	#endif
#else
	#ifdef __SANITIZE_THREAD__
		#define JPH_TSAN_ENABLED
	#endif
#endif

// Attribute to disable Thread Sanitizer for a particular function
#ifdef JPH_TSAN_ENABLED
	#define JPH_TSAN_NO_SANITIZE __attribute__((no_sanitize("thread")))
#else
	#define JPH_TSAN_NO_SANITIZE
#endif

JPH_NAMESPACE_END

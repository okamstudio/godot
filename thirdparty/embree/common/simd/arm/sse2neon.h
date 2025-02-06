#ifndef SSE2NEON_H
#define SSE2NEON_H

// This header file provides a simple API translation layer
// between SSE intrinsics to their corresponding Arm/Aarch64 NEON versions
//
// Contributors to this work are:
//   John W. Ratcliff <jratcliffscarab@gmail.com>
//   Brandon Rowlett <browlett@nvidia.com>
//   Ken Fast <kfast@gdeb.com>
//   Eric van Beurden <evanbeurden@nvidia.com>
//   Alexander Potylitsin <apotylitsin@nvidia.com>
//   Hasindu Gamaarachchi <hasindu2008@gmail.com>
//   Jim Huang <jserv@ccns.ncku.edu.tw>
//   Mark Cheng <marktwtn@gmail.com>
//   Malcolm James MacLeod <malcolm@gulden.com>
//   Devin Hussey (easyaspi314) <husseydevin@gmail.com>
//   Sebastian Pop <spop@amazon.com>
//   Developer Ecosystem Engineering <DeveloperEcosystemEngineering@apple.com>
//   Danila Kutenin <danilak@google.com>
//   François Turban (JishinMaster) <francois.turban@gmail.com>
//   Pei-Hsuan Hung <afcidk@gmail.com>
//   Yang-Hao Yuan <yuanyanghau@gmail.com>
//   Syoyo Fujita <syoyo@lighttransport.com>
//   Brecht Van Lommel <brecht@blender.org>
//   Jonathan Hue <jhue@adobe.com>
//   Cuda Chen <clh960524@gmail.com>
//   Aymen Qader <aymen.qader@arm.com>

/*
 * sse2neon is freely redistributable under the MIT License.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/* Tunable configurations */

/* Enable precise implementation of math operations
 * This would slow down the computation a bit, but gives consistent result with
 * x86 SSE. (e.g. would solve a hole or NaN pixel in the rendering result)
 */
/* _mm_min|max_ps|ss|pd|sd */
#ifndef SSE2NEON_PRECISE_MINMAX
#define SSE2NEON_PRECISE_MINMAX (0)
#endif
/* _mm_rcp_ps and _mm_div_ps */
#ifndef SSE2NEON_PRECISE_DIV
#define SSE2NEON_PRECISE_DIV (0)
#endif
/* _mm_sqrt_ps and _mm_rsqrt_ps */
#ifndef SSE2NEON_PRECISE_SQRT
#define SSE2NEON_PRECISE_SQRT (0)
#endif
/* _mm_dp_pd */
#ifndef SSE2NEON_PRECISE_DP
#define SSE2NEON_PRECISE_DP (0)
#endif

/* compiler specific definitions */
#if defined(__GNUC__) || defined(__clang__)
#pragma push_macro("FORCE_INLINE")
#pragma push_macro("ALIGN_STRUCT")
#define FORCE_INLINE static inline __attribute__((always_inline))
#define ALIGN_STRUCT(x) __attribute__((aligned(x)))
#define _sse2neon_likely(x) __builtin_expect(!!(x), 1)
#define _sse2neon_unlikely(x) __builtin_expect(!!(x), 0)
#else /* non-GNU / non-clang compilers */
#warning "Macro name collisions may happen with unsupported compiler."
#ifndef FORCE_INLINE
#define FORCE_INLINE static inline
#endif
#ifndef ALIGN_STRUCT
#define ALIGN_STRUCT(x) __declspec(align(x))
#endif
#define _sse2neon_likely(x) (x)
#define _sse2neon_unlikely(x) (x)
#endif

/* C language does not allow initializing a variable with a function call. */
#ifdef __cplusplus
#define _sse2neon_const static const
#else
#define _sse2neon_const const
#endif

#include <stdint.h>
#include <stdlib.h>


// -- GODOT start --
#if defined(_WIN32) && !defined(__MINGW32__)
// -- GODOT end --
/* Definitions for _mm_{malloc,free} are provided by <malloc.h>
 * from both MinGW-w64 and MSVC.
 */
#define SSE2NEON_ALLOC_DEFINED
#endif

/* If using MSVC */
#ifdef _MSC_VER
#include <intrin.h>
#if (defined(_M_AMD64) || defined(__x86_64__)) || \
    (defined(_M_ARM) || defined(__arm__))
#define SSE2NEON_HAS_BITSCAN64
#endif
#endif

/* Compiler barrier */
#define SSE2NEON_BARRIER()                     \
    do {                                       \
        __asm__ __volatile__("" ::: "memory"); \
        (void) 0;                              \
    } while (0)

/* Memory barriers
 * __atomic_thread_fence does not include a compiler barrier; instead,
 * the barrier is part of __atomic_load/__atomic_store's "volatile-like"
 * semantics.
 */
#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L)
#include <stdatomic.h>
#endif

FORCE_INLINE void _sse2neon_smp_mb(void)
{
    SSE2NEON_BARRIER();
#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L) && \
    !defined(__STDC_NO_ATOMICS__)
    atomic_thread_fence(memory_order_seq_cst);
#elif defined(__GNUC__) || defined(__clang__)
    __atomic_thread_fence(__ATOMIC_SEQ_CST);
#else
    /* FIXME: MSVC support */
#endif
}

/* Architecture-specific build options */
/* FIXME: #pragma GCC push_options is only available on GCC */
#if defined(__GNUC__)
#if defined(__arm__) && __ARM_ARCH == 7
/* According to ARM C Language Extensions Architecture specification,
 * __ARM_NEON is defined to a value indicating the Advanced SIMD (NEON)
 * architecture supported.
 */
#if !defined(__ARM_NEON) || !defined(__ARM_NEON__)
#error "You must enable NEON instructions (e.g. -mfpu=neon) to use SSE2NEON."
#endif
#if !defined(__clang__)
#pragma GCC push_options
#pragma GCC target("fpu=neon")
#endif
#elif defined(__aarch64__)
#if !defined(__clang__)
#pragma GCC push_options
#pragma GCC target("+simd")
#endif
#elif __ARM_ARCH == 8
#if !defined(__ARM_NEON) || !defined(__ARM_NEON__)
#error \
    "You must enable NEON instructions (e.g. -mfpu=neon-fp-armv8) to use SSE2NEON."
#endif
#if !defined(__clang__)
#pragma GCC push_options
#endif
#else
#error "Unsupported target. Must be either ARMv7-A+NEON or ARMv8-A."
#endif
#endif

#include <arm_neon.h>
#if !defined(__aarch64__) && (__ARM_ARCH == 8)
#if defined __has_include && __has_include(<arm_acle.h>)
#include <arm_acle.h>
#endif
#endif

/* Apple Silicon cache lines are double of what is commonly used by Intel, AMD
 * and other Arm microarchtectures use.
 * From sysctl -a on Apple M1:
 * hw.cachelinesize: 128
 */
#if defined(__APPLE__) && (defined(__aarch64__) || defined(__arm64__))
#define SSE2NEON_CACHELINE_SIZE 128
#else
#define SSE2NEON_CACHELINE_SIZE 64
#endif

/* Rounding functions require either Aarch64 instructions or libm failback */
#if !defined(__aarch64__)
#include <math.h>
#endif

/* On ARMv7, some registers, such as PMUSERENR and PMCCNTR, are read-only
 * or even not accessible in user mode.
 * To write or access to these registers in user mode,
 * we have to perform syscall instead.
 */
#if !defined(__aarch64__)
#include <sys/time.h>
#endif

/* "__has_builtin" can be used to query support for built-in functions
 * provided by gcc/clang and other compilers that support it.
 */
#ifndef __has_builtin /* GCC prior to 10 or non-clang compilers */
/* Compatibility with gcc <= 9 */
#if defined(__GNUC__) && (__GNUC__ <= 9)
#define __has_builtin(x) HAS##x
#define HAS__builtin_popcount 1
#define HAS__builtin_popcountll 1

// __builtin_shuffle introduced in GCC 4.7.0
#if (__GNUC__ >= 5) || ((__GNUC__ == 4) && (__GNUC_MINOR__ >= 7))
#define HAS__builtin_shuffle 1
#else
#define HAS__builtin_shuffle 0
#endif

#define HAS__builtin_shufflevector 0
#define HAS__builtin_nontemporal_store 0
#else
#define __has_builtin(x) 0
#endif
#endif

/**
 * MACRO for shuffle parameter for _mm_shuffle_ps().
 * Argument fp3 is a digit[0123] that represents the fp from argument "b"
 * of mm_shuffle_ps that will be placed in fp3 of result. fp2 is the same
 * for fp2 in result. fp1 is a digit[0123] that represents the fp from
 * argument "a" of mm_shuffle_ps that will be places in fp1 of result.
 * fp0 is the same for fp0 of result.
 */
#if defined(__aarch64__)
#define _MN_SHUFFLE(fp3,fp2,fp1,fp0) ( (uint8x16_t){ (((fp3)*4)+0), (((fp3)*4)+1), (((fp3)*4)+2), (((fp3)*4)+3),  (((fp2)*4)+0), (((fp2)*4)+1), (((fp2)*4)+\
2), (((fp2)*4)+3),  (((fp1)*4)+0), (((fp1)*4)+1), (((fp1)*4)+2), (((fp1)*4)+3),  (((fp0)*4)+0), (((fp0)*4)+1), (((fp0)*4)+2), (((fp0)*4)+3) } )
#define _MF_SHUFFLE(fp3,fp2,fp1,fp0) ( (uint8x16_t){ (((fp3)*4)+0), (((fp3)*4)+1), (((fp3)*4)+2), (((fp3)*4)+3),  (((fp2)*4)+0), (((fp2)*4)+1), (((fp2)*4)+\
2), (((fp2)*4)+3),  (((fp1)*4)+16+0), (((fp1)*4)+16+1), (((fp1)*4)+16+2), (((fp1)*4)+16+3),  (((fp0)*4)+16+0), (((fp0)*4)+16+1), (((fp0)*4)+16+2), (((fp0)*\
4)+16+3) } )
#endif

#define _MM_SHUFFLE(fp3, fp2, fp1, fp0) \
    (((fp3) << 6) | ((fp2) << 4) | ((fp1) << 2) | ((fp0)))

#if __has_builtin(__builtin_shufflevector)
#define _sse2neon_shuffle(type, a, b, ...) \
    __builtin_shufflevector(a, b, __VA_ARGS__)
#elif __has_builtin(__builtin_shuffle)
#define _sse2neon_shuffle(type, a, b, ...) \
    __extension__({                        \
        type tmp = {__VA_ARGS__};          \
        __builtin_shuffle(a, b, tmp);      \
    })
#endif

#ifdef _sse2neon_shuffle
#define vshuffle_s16(a, b, ...) _sse2neon_shuffle(int16x4_t, a, b, __VA_ARGS__)
#define vshuffleq_s16(a, b, ...) _sse2neon_shuffle(int16x8_t, a, b, __VA_ARGS__)
#define vshuffle_s32(a, b, ...) _sse2neon_shuffle(int32x2_t, a, b, __VA_ARGS__)
#define vshuffleq_s32(a, b, ...) _sse2neon_shuffle(int32x4_t, a, b, __VA_ARGS__)
#define vshuffle_s64(a, b, ...) _sse2neon_shuffle(int64x1_t, a, b, __VA_ARGS__)
#define vshuffleq_s64(a, b, ...) _sse2neon_shuffle(int64x2_t, a, b, __VA_ARGS__)
#endif

/* Rounding mode macros. */
#define _MM_FROUND_TO_NEAREST_INT 0x00
#define _MM_FROUND_TO_NEG_INF 0x01
#define _MM_FROUND_TO_POS_INF 0x02
#define _MM_FROUND_TO_ZERO 0x03
#define _MM_FROUND_CUR_DIRECTION 0x04
#define _MM_FROUND_NO_EXC 0x08
#define _MM_FROUND_RAISE_EXC 0x00
#define _MM_FROUND_NINT (_MM_FROUND_TO_NEAREST_INT | _MM_FROUND_RAISE_EXC)
#define _MM_FROUND_FLOOR (_MM_FROUND_TO_NEG_INF | _MM_FROUND_RAISE_EXC)
#define _MM_FROUND_CEIL (_MM_FROUND_TO_POS_INF | _MM_FROUND_RAISE_EXC)
#define _MM_FROUND_TRUNC (_MM_FROUND_TO_ZERO | _MM_FROUND_RAISE_EXC)
#define _MM_FROUND_RINT (_MM_FROUND_CUR_DIRECTION | _MM_FROUND_RAISE_EXC)
#define _MM_FROUND_NEARBYINT (_MM_FROUND_CUR_DIRECTION | _MM_FROUND_NO_EXC)
#define _MM_ROUND_NEAREST 0x0000
#define _MM_ROUND_DOWN 0x2000
#define _MM_ROUND_UP 0x4000
#define _MM_ROUND_TOWARD_ZERO 0x6000
/* Flush zero mode macros. */
#define _MM_FLUSH_ZERO_MASK 0x8000
#define _MM_FLUSH_ZERO_ON 0x8000
#define _MM_FLUSH_ZERO_OFF 0x0000
/* Denormals are zeros mode macros. */
#define _MM_DENORMALS_ZERO_MASK 0x0040
#define _MM_DENORMALS_ZERO_ON 0x0040
#define _MM_DENORMALS_ZERO_OFF 0x0000

/* indicate immediate constant argument in a given range */
#define __constrange(a, b) const

/* A few intrinsics accept traditional data types like ints or floats, but
 * most operate on data types that are specific to SSE.
 * If a vector type ends in d, it contains doubles, and if it does not have
 * a suffix, it contains floats. An integer vector type can contain any type
 * of integer, from chars to shorts to unsigned long longs.
 */
typedef int64x1_t __m64;
typedef float32x4_t __m128; /* 128-bit vector containing 4 floats */
// On ARM 32-bit architecture, the float64x2_t is not supported.
// The data type __m128d should be represented in a different way for related
// intrinsic conversion.
#if defined(__aarch64__)
typedef float64x2_t __m128d; /* 128-bit vector containing 2 doubles */
#else
typedef float32x4_t __m128d;
#endif
typedef int64x2_t __m128i; /* 128-bit vector containing integers */

// __int64 is defined in the Intrinsics Guide which maps to different datatype
// in different data model
#if !(defined(_WIN32) || defined(_WIN64) || defined(__int64))
#if (defined(__x86_64__) || defined(__i386__))
#define __int64 long long
#else
#define __int64 int64_t
#endif
#endif

/* type-safe casting between types */

#define vreinterpretq_m128_f16(x) vreinterpretq_f32_f16(x)
#define vreinterpretq_m128_f32(x) (x)
#define vreinterpretq_m128_f64(x) vreinterpretq_f32_f64(x)

#define vreinterpretq_m128_u8(x) vreinterpretq_f32_u8(x)
#define vreinterpretq_m128_u16(x) vreinterpretq_f32_u16(x)
#define vreinterpretq_m128_u32(x) vreinterpretq_f32_u32(x)
#define vreinterpretq_m128_u64(x) vreinterpretq_f32_u64(x)

#define vreinterpretq_m128_s8(x) vreinterpretq_f32_s8(x)
#define vreinterpretq_m128_s16(x) vreinterpretq_f32_s16(x)
#define vreinterpretq_m128_s32(x) vreinterpretq_f32_s32(x)
#define vreinterpretq_m128_s64(x) vreinterpretq_f32_s64(x)

#define vreinterpretq_f16_m128(x) vreinterpretq_f16_f32(x)
#define vreinterpretq_f32_m128(x) (x)
#define vreinterpretq_f64_m128(x) vreinterpretq_f64_f32(x)

#define vreinterpretq_u8_m128(x) vreinterpretq_u8_f32(x)
#define vreinterpretq_u16_m128(x) vreinterpretq_u16_f32(x)
#define vreinterpretq_u32_m128(x) vreinterpretq_u32_f32(x)
#define vreinterpretq_u64_m128(x) vreinterpretq_u64_f32(x)

#define vreinterpretq_s8_m128(x) vreinterpretq_s8_f32(x)
#define vreinterpretq_s16_m128(x) vreinterpretq_s16_f32(x)
#define vreinterpretq_s32_m128(x) vreinterpretq_s32_f32(x)
#define vreinterpretq_s64_m128(x) vreinterpretq_s64_f32(x)

#define vreinterpretq_m128i_s8(x) vreinterpretq_s64_s8(x)
#define vreinterpretq_m128i_s16(x) vreinterpretq_s64_s16(x)
#define vreinterpretq_m128i_s32(x) vreinterpretq_s64_s32(x)
#define vreinterpretq_m128i_s64(x) (x)

#define vreinterpretq_m128i_u8(x) vreinterpretq_s64_u8(x)
#define vreinterpretq_m128i_u16(x) vreinterpretq_s64_u16(x)
#define vreinterpretq_m128i_u32(x) vreinterpretq_s64_u32(x)
#define vreinterpretq_m128i_u64(x) vreinterpretq_s64_u64(x)

#define vreinterpretq_f32_m128i(x) vreinterpretq_f32_s64(x)
#define vreinterpretq_f64_m128i(x) vreinterpretq_f64_s64(x)

#define vreinterpretq_s8_m128i(x) vreinterpretq_s8_s64(x)
#define vreinterpretq_s16_m128i(x) vreinterpretq_s16_s64(x)
#define vreinterpretq_s32_m128i(x) vreinterpretq_s32_s64(x)
#define vreinterpretq_s64_m128i(x) (x)

#define vreinterpretq_u8_m128i(x) vreinterpretq_u8_s64(x)
#define vreinterpretq_u16_m128i(x) vreinterpretq_u16_s64(x)
#define vreinterpretq_u32_m128i(x) vreinterpretq_u32_s64(x)
#define vreinterpretq_u64_m128i(x) vreinterpretq_u64_s64(x)

#define vreinterpret_m64_s8(x) vreinterpret_s64_s8(x)
#define vreinterpret_m64_s16(x) vreinterpret_s64_s16(x)
#define vreinterpret_m64_s32(x) vreinterpret_s64_s32(x)
#define vreinterpret_m64_s64(x) (x)

#define vreinterpret_m64_u8(x) vreinterpret_s64_u8(x)
#define vreinterpret_m64_u16(x) vreinterpret_s64_u16(x)
#define vreinterpret_m64_u32(x) vreinterpret_s64_u32(x)
#define vreinterpret_m64_u64(x) vreinterpret_s64_u64(x)

#define vreinterpret_m64_f16(x) vreinterpret_s64_f16(x)
#define vreinterpret_m64_f32(x) vreinterpret_s64_f32(x)
#define vreinterpret_m64_f64(x) vreinterpret_s64_f64(x)

#define vreinterpret_u8_m64(x) vreinterpret_u8_s64(x)
#define vreinterpret_u16_m64(x) vreinterpret_u16_s64(x)
#define vreinterpret_u32_m64(x) vreinterpret_u32_s64(x)
#define vreinterpret_u64_m64(x) vreinterpret_u64_s64(x)

#define vreinterpret_s8_m64(x) vreinterpret_s8_s64(x)
#define vreinterpret_s16_m64(x) vreinterpret_s16_s64(x)
#define vreinterpret_s32_m64(x) vreinterpret_s32_s64(x)
#define vreinterpret_s64_m64(x) (x)

#define vreinterpret_f32_m64(x) vreinterpret_f32_s64(x)

#if defined(__aarch64__)
#define vreinterpretq_m128d_s32(x) vreinterpretq_f64_s32(x)
#define vreinterpretq_m128d_s64(x) vreinterpretq_f64_s64(x)

#define vreinterpretq_m128d_u64(x) vreinterpretq_f64_u64(x)

#define vreinterpretq_m128d_f32(x) vreinterpretq_f64_f32(x)
#define vreinterpretq_m128d_f64(x) (x)

#define vreinterpretq_s64_m128d(x) vreinterpretq_s64_f64(x)

#define vreinterpretq_u32_m128d(x) vreinterpretq_u32_f64(x)
#define vreinterpretq_u64_m128d(x) vreinterpretq_u64_f64(x)

#define vreinterpretq_f64_m128d(x) (x)
#define vreinterpretq_f32_m128d(x) vreinterpretq_f32_f64(x)
#else
#define vreinterpretq_m128d_s32(x) vreinterpretq_f32_s32(x)
#define vreinterpretq_m128d_s64(x) vreinterpretq_f32_s64(x)

#define vreinterpretq_m128d_u32(x) vreinterpretq_f32_u32(x)
#define vreinterpretq_m128d_u64(x) vreinterpretq_f32_u64(x)

#define vreinterpretq_m128d_f32(x) (x)

#define vreinterpretq_s64_m128d(x) vreinterpretq_s64_f32(x)

#define vreinterpretq_u32_m128d(x) vreinterpretq_u32_f32(x)
#define vreinterpretq_u64_m128d(x) vreinterpretq_u64_f32(x)

#define vreinterpretq_f32_m128d(x) (x)
#endif

// A struct is defined in this header file called 'SIMDVec' which can be used
// by applications which attempt to access the contents of an __m128 struct
// directly.  It is important to note that accessing the __m128 struct directly
// is bad coding practice by Microsoft: @see:
// https://docs.microsoft.com/en-us/cpp/cpp/m128
//
// However, some legacy source code may try to access the contents of an __m128
// struct directly so the developer can use the SIMDVec as an alias for it.  Any
// casting must be done manually by the developer, as you cannot cast or
// otherwise alias the base NEON data type for intrinsic operations.
//
// union intended to allow direct access to an __m128 variable using the names
// that the MSVC compiler provides.  This union should really only be used when
// trying to access the members of the vector as integer values.  GCC/clang
// allow native access to the float members through a simple array access
// operator (in C since 4.6, in C++ since 4.8).
//
// Ideally direct accesses to SIMD vectors should not be used since it can cause
// a performance hit.  If it really is needed however, the original __m128
// variable can be aliased with a pointer to this union and used to access
// individual components.  The use of this union should be hidden behind a macro
// that is used throughout the codebase to access the members instead of always
// declaring this type of variable.
typedef union ALIGN_STRUCT(16) SIMDVec {
    float m128_f32[4];     // as floats - DON'T USE. Added for convenience.
    int8_t m128_i8[16];    // as signed 8-bit integers.
    int16_t m128_i16[8];   // as signed 16-bit integers.
    int32_t m128_i32[4];   // as signed 32-bit integers.
    int64_t m128_i64[2];   // as signed 64-bit integers.
    uint8_t m128_u8[16];   // as unsigned 8-bit integers.
    uint16_t m128_u16[8];  // as unsigned 16-bit integers.
    uint32_t m128_u32[4];  // as unsigned 32-bit integers.
    uint64_t m128_u64[2];  // as unsigned 64-bit integers.
} SIMDVec;

// casting using SIMDVec
#define vreinterpretq_nth_u64_m128i(x, n) (((SIMDVec *) &x)->m128_u64[n])
#define vreinterpretq_nth_u32_m128i(x, n) (((SIMDVec *) &x)->m128_u32[n])
#define vreinterpretq_nth_u8_m128i(x, n) (((SIMDVec *) &x)->m128_u8[n])

/* SSE macros */
#define _MM_GET_FLUSH_ZERO_MODE _sse2neon_mm_get_flush_zero_mode
#define _MM_SET_FLUSH_ZERO_MODE _sse2neon_mm_set_flush_zero_mode
#define _MM_GET_DENORMALS_ZERO_MODE _sse2neon_mm_get_denormals_zero_mode
#define _MM_SET_DENORMALS_ZERO_MODE _sse2neon_mm_set_denormals_zero_mode

// Function declaration
// SSE
FORCE_INLINE unsigned int _MM_GET_ROUNDING_MODE();
FORCE_INLINE __m128 _mm_move_ss(__m128, __m128);
FORCE_INLINE __m128 _mm_or_ps(__m128, __m128);
FORCE_INLINE __m128 _mm_set_ps1(float);
FORCE_INLINE __m128 _mm_setzero_ps(void);
// SSE2
FORCE_INLINE __m128i _mm_and_si128(__m128i, __m128i);
FORCE_INLINE __m128i _mm_castps_si128(__m128);
FORCE_INLINE __m128i _mm_cmpeq_epi32(__m128i, __m128i);
FORCE_INLINE __m128i _mm_cvtps_epi32(__m128);
FORCE_INLINE __m128d _mm_move_sd(__m128d, __m128d);
FORCE_INLINE __m128i _mm_or_si128(__m128i, __m128i);
FORCE_INLINE __m128i _mm_set_epi32(int, int, int, int);
FORCE_INLINE __m128i _mm_set_epi64x(int64_t, int64_t);
FORCE_INLINE __m128d _mm_set_pd(double, double);
FORCE_INLINE __m128i _mm_set1_epi32(int);
FORCE_INLINE __m128i _mm_setzero_si128();
// SSE4.1
FORCE_INLINE __m128d _mm_ceil_pd(__m128d);
FORCE_INLINE __m128 _mm_ceil_ps(__m128);
FORCE_INLINE __m128d _mm_floor_pd(__m128d);
FORCE_INLINE __m128 _mm_floor_ps(__m128);
FORCE_INLINE __m128d _mm_round_pd(__m128d, int);
FORCE_INLINE __m128 _mm_round_ps(__m128, int);
// SSE4.2
FORCE_INLINE uint32_t _mm_crc32_u8(uint32_t, uint8_t);

/* Backwards compatibility for compilers with lack of specific type support */

// Older gcc does not define vld1q_u8_x4 type
#if defined(__GNUC__) && !defined(__clang__) &&                        \
    ((__GNUC__ <= 12 && defined(__arm__)) ||                           \
     (__GNUC__ == 10 && __GNUC_MINOR__ < 3 && defined(__aarch64__)) || \
     (__GNUC__ <= 9 && defined(__aarch64__)))
FORCE_INLINE uint8x16x4_t _sse2neon_vld1q_u8_x4(const uint8_t *p)
{
    uint8x16x4_t ret;
    ret.val[0] = vld1q_u8(p + 0);
    ret.val[1] = vld1q_u8(p + 16);
    ret.val[2] = vld1q_u8(p + 32);
    ret.val[3] = vld1q_u8(p + 48);
    return ret;
}
#else
// Wraps vld1q_u8_x4
FORCE_INLINE uint8x16x4_t _sse2neon_vld1q_u8_x4(const uint8_t *p)
{
    return vld1q_u8_x4(p);
}
#endif

#if !defined(__aarch64__)
/* emulate vaddv u8 variant */
FORCE_INLINE uint8_t _sse2neon_vaddv_u8(uint8x8_t v8)
{
    const uint64x1_t v1 = vpaddl_u32(vpaddl_u16(vpaddl_u8(v8)));
    return vget_lane_u8(vreinterpret_u8_u64(v1), 0);
}
#else
// Wraps vaddv_u8
FORCE_INLINE uint8_t _sse2neon_vaddv_u8(uint8x8_t v8)
{
    return vaddv_u8(v8);
}
#endif

#if !defined(__aarch64__)
/* emulate vaddvq u8 variant */
FORCE_INLINE uint8_t _sse2neon_vaddvq_u8(uint8x16_t a)
{
    uint8x8_t tmp = vpadd_u8(vget_low_u8(a), vget_high_u8(a));
    uint8_t res = 0;
    for (int i = 0; i < 8; ++i)
        res += tmp[i];
    return res;
}
#else
// Wraps vaddvq_u8
FORCE_INLINE uint8_t _sse2neon_vaddvq_u8(uint8x16_t a)
{
    return vaddvq_u8(a);
}
#endif

#if !defined(__aarch64__)
/* emulate vaddvq u16 variant */
FORCE_INLINE uint16_t _sse2neon_vaddvq_u16(uint16x8_t a)
{
    uint32x4_t m = vpaddlq_u16(a);
    uint64x2_t n = vpaddlq_u32(m);
    uint64x1_t o = vget_low_u64(n) + vget_high_u64(n);

    return vget_lane_u32((uint32x2_t) o, 0);
}
#else
// Wraps vaddvq_u16
FORCE_INLINE uint16_t _sse2neon_vaddvq_u16(uint16x8_t a)
{
    return vaddvq_u16(a);
}
#endif

/* Function Naming Conventions
 * The naming convention of SSE intrinsics is straightforward. A generic SSE
 * intrinsic function is given as follows:
 *   _mm_<name>_<data_type>
 *
 * The parts of this format are given as follows:
 * 1. <name> describes the operation performed by the intrinsic
 * 2. <data_type> identifies the data type of the function's primary arguments
 *
 * This last part, <data_type>, is a little complicated. It identifies the
 * content of the input values, and can be set to any of the following values:
 * + ps - vectors contain floats (ps stands for packed single-precision)
 * + pd - vectors cantain doubles (pd stands for packed double-precision)
 * + epi8/epi16/epi32/epi64 - vectors contain 8-bit/16-bit/32-bit/64-bit
 *                            signed integers
 * + epu8/epu16/epu32/epu64 - vectors contain 8-bit/16-bit/32-bit/64-bit
 *                            unsigned integers
 * + si128 - unspecified 128-bit vector or 256-bit vector
 * + m128/m128i/m128d - identifies input vector types when they are different
 *                      than the type of the returned vector
 *
 * For example, _mm_setzero_ps. The _mm implies that the function returns
 * a 128-bit vector. The _ps at the end implies that the argument vectors
 * contain floats.
 *
 * A complete example: Byte Shuffle - pshufb (_mm_shuffle_epi8)
 *   // Set packed 16-bit integers. 128 bits, 8 short, per 16 bits
 *   __m128i v_in = _mm_setr_epi16(1, 2, 3, 4, 5, 6, 7, 8);
 *   // Set packed 8-bit integers
 *   // 128 bits, 16 chars, per 8 bits
 *   __m128i v_perm = _mm_setr_epi8(1, 0,  2,  3, 8, 9, 10, 11,
 *                                  4, 5, 12, 13, 6, 7, 14, 15);
 *   // Shuffle packed 8-bit integers
 *   __m128i v_out = _mm_shuffle_epi8(v_in, v_perm); // pshufb
 *
 * Data (Number, Binary, Byte Index):
    +------+------+-------------+------+------+-------------+
    |      1      |      2      |      3      |      4      | Number
    +------+------+------+------+------+------+------+------+
    | 0000 | 0001 | 0000 | 0010 | 0000 | 0011 | 0000 | 0100 | Binary
    +------+------+------+------+------+------+------+------+
    |    0 |    1 |    2 |    3 |    4 |    5 |    6 |    7 | Index
    +------+------+------+------+------+------+------+------+

    +------+------+------+------+------+------+------+------+
    |      5      |      6      |      7      |      8      | Number
    +------+------+------+------+------+------+------+------+
    | 0000 | 0101 | 0000 | 0110 | 0000 | 0111 | 0000 | 1000 | Binary
    +------+------+------+------+------+------+------+------+
    |    8 |    9 |   10 |   11 |   12 |   13 |   14 |   15 | Index
    +------+------+------+------+------+------+------+------+
 * Index (Byte Index):
    +------+------+------+------+------+------+------+------+
    |    1 |    0 |    2 |    3 |    8 |    9 |   10 |   11 |
    +------+------+------+------+------+------+------+------+

    +------+------+------+------+------+------+------+------+
    |    4 |    5 |   12 |   13 |    6 |    7 |   14 |   15 |
    +------+------+------+------+------+------+------+------+
 * Result:
    +------+------+------+------+------+------+------+------+
    |    1 |    0 |    2 |    3 |    8 |    9 |   10 |   11 | Index
    +------+------+------+------+------+------+------+------+
    | 0001 | 0000 | 0000 | 0010 | 0000 | 0101 | 0000 | 0110 | Binary
    +------+------+------+------+------+------+------+------+
    |     256     |      2      |      5      |      6      | Number
    +------+------+------+------+------+------+------+------+

    +------+------+------+------+------+------+------+------+
    |    4 |    5 |   12 |   13 |    6 |    7 |   14 |   15 | Index
    +------+------+------+------+------+------+------+------+
    | 0000 | 0011 | 0000 | 0111 | 0000 | 0100 | 0000 | 1000 | Binary
    +------+------+------+------+------+------+------+------+
    |      3      |      7      |      4      |      8      | Number
    +------+------+------+------+------+------+-------------+
 */

/* Constants for use with _mm_prefetch. */
enum _mm_hint {
    _MM_HINT_NTA = 0, /* load data to L1 and L2 cache, mark it as NTA */
    _MM_HINT_T0 = 1,  /* load data to L1 and L2 cache */
    _MM_HINT_T1 = 2,  /* load data to L2 cache only */
    _MM_HINT_T2 = 3,  /* load data to L2 cache only, mark it as NTA */
};

// The bit field mapping to the FPCR(floating-point control register)
typedef struct {
    uint16_t res0;
    uint8_t res1 : 6;
    uint8_t bit22 : 1;
    uint8_t bit23 : 1;
    uint8_t bit24 : 1;
    uint8_t res2 : 7;
#if defined(__aarch64__)
    uint32_t res3;
#endif
} fpcr_bitfield;

// Takes the upper 64 bits of a and places it in the low end of the result
// Takes the lower 64 bits of b and places it into the high end of the result.
FORCE_INLINE __m128 _mm_shuffle_ps_1032(__m128 a, __m128 b)
{
    float32x2_t a32 = vget_high_f32(vreinterpretq_f32_m128(a));
    float32x2_t b10 = vget_low_f32(vreinterpretq_f32_m128(b));
    return vreinterpretq_m128_f32(vcombine_f32(a32, b10));
}

// takes the lower two 32-bit values from a and swaps them and places in high
// end of result takes the higher two 32 bit values from b and swaps them and
// places in low end of result.
FORCE_INLINE __m128 _mm_shuffle_ps_2301(__m128 a, __m128 b)
{
    float32x2_t a01 = vrev64_f32(vget_low_f32(vreinterpretq_f32_m128(a)));
    float32x2_t b23 = vrev64_f32(vget_high_f32(vreinterpretq_f32_m128(b)));
    return vreinterpretq_m128_f32(vcombine_f32(a01, b23));
}

FORCE_INLINE __m128 _mm_shuffle_ps_0321(__m128 a, __m128 b)
{
    float32x2_t a21 = vget_high_f32(
        vextq_f32(vreinterpretq_f32_m128(a), vreinterpretq_f32_m128(a), 3));
    float32x2_t b03 = vget_low_f32(
        vextq_f32(vreinterpretq_f32_m128(b), vreinterpretq_f32_m128(b), 3));
    return vreinterpretq_m128_f32(vcombine_f32(a21, b03));
}

FORCE_INLINE __m128 _mm_shuffle_ps_2103(__m128 a, __m128 b)
{
    float32x2_t a03 = vget_low_f32(
        vextq_f32(vreinterpretq_f32_m128(a), vreinterpretq_f32_m128(a), 3));
    float32x2_t b21 = vget_high_f32(
        vextq_f32(vreinterpretq_f32_m128(b), vreinterpretq_f32_m128(b), 3));
    return vreinterpretq_m128_f32(vcombine_f32(a03, b21));
}

FORCE_INLINE __m128 _mm_shuffle_ps_1010(__m128 a, __m128 b)
{
    float32x2_t a10 = vget_low_f32(vreinterpretq_f32_m128(a));
    float32x2_t b10 = vget_low_f32(vreinterpretq_f32_m128(b));
    return vreinterpretq_m128_f32(vcombine_f32(a10, b10));
}

FORCE_INLINE __m128 _mm_shuffle_ps_1001(__m128 a, __m128 b)
{
    float32x2_t a01 = vrev64_f32(vget_low_f32(vreinterpretq_f32_m128(a)));
    float32x2_t b10 = vget_low_f32(vreinterpretq_f32_m128(b));
    return vreinterpretq_m128_f32(vcombine_f32(a01, b10));
}

FORCE_INLINE __m128 _mm_shuffle_ps_0101(__m128 a, __m128 b)
{
    float32x2_t a01 = vrev64_f32(vget_low_f32(vreinterpretq_f32_m128(a)));
    float32x2_t b01 = vrev64_f32(vget_low_f32(vreinterpretq_f32_m128(b)));
    return vreinterpretq_m128_f32(vcombine_f32(a01, b01));
}

// keeps the low 64 bits of b in the low and puts the high 64 bits of a in the
// high
FORCE_INLINE __m128 _mm_shuffle_ps_3210(__m128 a, __m128 b)
{
    float32x2_t a10 = vget_low_f32(vreinterpretq_f32_m128(a));
    float32x2_t b32 = vget_high_f32(vreinterpretq_f32_m128(b));
    return vreinterpretq_m128_f32(vcombine_f32(a10, b32));
}

FORCE_INLINE __m128 _mm_shuffle_ps_0011(__m128 a, __m128 b)
{
    float32x2_t a11 = vdup_lane_f32(vget_low_f32(vreinterpretq_f32_m128(a)), 1);
    float32x2_t b00 = vdup_lane_f32(vget_low_f32(vreinterpretq_f32_m128(b)), 0);
    return vreinterpretq_m128_f32(vcombine_f32(a11, b00));
}

FORCE_INLINE __m128 _mm_shuffle_ps_0022(__m128 a, __m128 b)
{
    float32x2_t a22 =
        vdup_lane_f32(vget_high_f32(vreinterpretq_f32_m128(a)), 0);
    float32x2_t b00 = vdup_lane_f32(vget_low_f32(vreinterpretq_f32_m128(b)), 0);
    return vreinterpretq_m128_f32(vcombine_f32(a22, b00));
}

FORCE_INLINE __m128 _mm_shuffle_ps_2200(__m128 a, __m128 b)
{
    float32x2_t a00 = vdup_lane_f32(vget_low_f32(vreinterpretq_f32_m128(a)), 0);
    float32x2_t b22 =
        vdup_lane_f32(vget_high_f32(vreinterpretq_f32_m128(b)), 0);
    return vreinterpretq_m128_f32(vcombine_f32(a00, b22));
}

FORCE_INLINE __m128 _mm_shuffle_ps_3202(__m128 a, __m128 b)
{
    float32_t a0 = vgetq_lane_f32(vreinterpretq_f32_m128(a), 0);
    float32x2_t a22 =
        vdup_lane_f32(vget_high_f32(vreinterpretq_f32_m128(a)), 0);
    float32x2_t a02 = vset_lane_f32(a0, a22, 1); /* TODO: use vzip ?*/
    float32x2_t b32 = vget_high_f32(vreinterpretq_f32_m128(b));
    return vreinterpretq_m128_f32(vcombine_f32(a02, b32));
}

FORCE_INLINE __m128 _mm_shuffle_ps_1133(__m128 a, __m128 b)
{
    float32x2_t a33 =
        vdup_lane_f32(vget_high_f32(vreinterpretq_f32_m128(a)), 1);
    float32x2_t b11 = vdup_lane_f32(vget_low_f32(vreinterpretq_f32_m128(b)), 1);
    return vreinterpretq_m128_f32(vcombine_f32(a33, b11));
}

FORCE_INLINE __m128 _mm_shuffle_ps_2010(__m128 a, __m128 b)
{
    float32x2_t a10 = vget_low_f32(vreinterpretq_f32_m128(a));
    float32_t b2 = vgetq_lane_f32(vreinterpretq_f32_m128(b), 2);
    float32x2_t b00 = vdup_lane_f32(vget_low_f32(vreinterpretq_f32_m128(b)), 0);
    float32x2_t b20 = vset_lane_f32(b2, b00, 1);
    return vreinterpretq_m128_f32(vcombine_f32(a10, b20));
}

FORCE_INLINE __m128 _mm_shuffle_ps_2001(__m128 a, __m128 b)
{
    float32x2_t a01 = vrev64_f32(vget_low_f32(vreinterpretq_f32_m128(a)));
    float32_t b2 = vgetq_lane_f32(b, 2);
    float32x2_t b00 = vdup_lane_f32(vget_low_f32(vreinterpretq_f32_m128(b)), 0);
    float32x2_t b20 = vset_lane_f32(b2, b00, 1);
    return vreinterpretq_m128_f32(vcombine_f32(a01, b20));
}

FORCE_INLINE __m128 _mm_shuffle_ps_2032(__m128 a, __m128 b)
{
    float32x2_t a32 = vget_high_f32(vreinterpretq_f32_m128(a));
    float32_t b2 = vgetq_lane_f32(b, 2);
    float32x2_t b00 = vdup_lane_f32(vget_low_f32(vreinterpretq_f32_m128(b)), 0);
    float32x2_t b20 = vset_lane_f32(b2, b00, 1);
    return vreinterpretq_m128_f32(vcombine_f32(a32, b20));
}

// Kahan summation for accurate summation of floating-point numbers.
// http://blog.zachbjornson.com/2019/08/11/fast-float-summation.html
FORCE_INLINE void _sse2neon_kadd_f32(float *sum, float *c, float y)
{
    y -= *c;
    float t = *sum + y;
    *c = (t - *sum) - y;
    *sum = t;
}

#if defined(__ARM_FEATURE_CRYPTO) && \
    (defined(__aarch64__) || __has_builtin(__builtin_arm_crypto_vmullp64))
// Wraps vmull_p64
FORCE_INLINE uint64x2_t _sse2neon_vmull_p64(uint64x1_t _a, uint64x1_t _b)
{
    poly64_t a = vget_lane_p64(vreinterpret_p64_u64(_a), 0);
    poly64_t b = vget_lane_p64(vreinterpret_p64_u64(_b), 0);
    return vreinterpretq_u64_p128(vmull_p64(a, b));
}
#else  // ARMv7 polyfill
// ARMv7/some A64 lacks vmull_p64, but it has vmull_p8.
//
// vmull_p8 calculates 8 8-bit->16-bit polynomial multiplies, but we need a
// 64-bit->128-bit polynomial multiply.
//
// It needs some work and is somewhat slow, but it is still faster than all
// known scalar methods.
//
// Algorithm adapted to C from
// https://www.workofard.com/2017/07/ghash-for-low-end-cores/, which is adapted
// from "Fast Software Polynomial Multiplication on ARM Processors Using the
// NEON Engine" by Danilo Camara, Conrado Gouvea, Julio Lopez and Ricardo Dahab
// (https://hal.inria.fr/hal-01506572)
static uint64x2_t _sse2neon_vmull_p64(uint64x1_t _a, uint64x1_t _b)
{
    poly8x8_t a = vreinterpret_p8_u64(_a);
    poly8x8_t b = vreinterpret_p8_u64(_b);

    // Masks
    uint8x16_t k48_32 = vcombine_u8(vcreate_u8(0x0000ffffffffffff),
                                    vcreate_u8(0x00000000ffffffff));
    uint8x16_t k16_00 = vcombine_u8(vcreate_u8(0x000000000000ffff),
                                    vcreate_u8(0x0000000000000000));

    // Do the multiplies, rotating with vext to get all combinations
    uint8x16_t d = vreinterpretq_u8_p16(vmull_p8(a, b));  // D = A0 * B0
    uint8x16_t e =
        vreinterpretq_u8_p16(vmull_p8(a, vext_p8(b, b, 1)));  // E = A0 * B1
    uint8x16_t f =
        vreinterpretq_u8_p16(vmull_p8(vext_p8(a, a, 1), b));  // F = A1 * B0
    uint8x16_t g =
        vreinterpretq_u8_p16(vmull_p8(a, vext_p8(b, b, 2)));  // G = A0 * B2
    uint8x16_t h =
        vreinterpretq_u8_p16(vmull_p8(vext_p8(a, a, 2), b));  // H = A2 * B0
    uint8x16_t i =
        vreinterpretq_u8_p16(vmull_p8(a, vext_p8(b, b, 3)));  // I = A0 * B3
    uint8x16_t j =
        vreinterpretq_u8_p16(vmull_p8(vext_p8(a, a, 3), b));  // J = A3 * B0
    uint8x16_t k =
        vreinterpretq_u8_p16(vmull_p8(a, vext_p8(b, b, 4)));  // L = A0 * B4

    // Add cross products
    uint8x16_t l = veorq_u8(e, f);  // L = E + F
    uint8x16_t m = veorq_u8(g, h);  // M = G + H
    uint8x16_t n = veorq_u8(i, j);  // N = I + J

    // Interleave. Using vzip1 and vzip2 prevents Clang from emitting TBL
    // instructions.
#if defined(__aarch64__)
    uint8x16_t lm_p0 = vreinterpretq_u8_u64(
        vzip1q_u64(vreinterpretq_u64_u8(l), vreinterpretq_u64_u8(m)));
    uint8x16_t lm_p1 = vreinterpretq_u8_u64(
        vzip2q_u64(vreinterpretq_u64_u8(l), vreinterpretq_u64_u8(m)));
    uint8x16_t nk_p0 = vreinterpretq_u8_u64(
        vzip1q_u64(vreinterpretq_u64_u8(n), vreinterpretq_u64_u8(k)));
    uint8x16_t nk_p1 = vreinterpretq_u8_u64(
        vzip2q_u64(vreinterpretq_u64_u8(n), vreinterpretq_u64_u8(k)));
#else
    uint8x16_t lm_p0 = vcombine_u8(vget_low_u8(l), vget_low_u8(m));
    uint8x16_t lm_p1 = vcombine_u8(vget_high_u8(l), vget_high_u8(m));
    uint8x16_t nk_p0 = vcombine_u8(vget_low_u8(n), vget_low_u8(k));
    uint8x16_t nk_p1 = vcombine_u8(vget_high_u8(n), vget_high_u8(k));
#endif
    // t0 = (L) (P0 + P1) << 8
    // t1 = (M) (P2 + P3) << 16
    uint8x16_t t0t1_tmp = veorq_u8(lm_p0, lm_p1);
    uint8x16_t t0t1_h = vandq_u8(lm_p1, k48_32);
    uint8x16_t t0t1_l = veorq_u8(t0t1_tmp, t0t1_h);

    // t2 = (N) (P4 + P5) << 24
    // t3 = (K) (P6 + P7) << 32
    uint8x16_t t2t3_tmp = veorq_u8(nk_p0, nk_p1);
    uint8x16_t t2t3_h = vandq_u8(nk_p1, k16_00);
    uint8x16_t t2t3_l = veorq_u8(t2t3_tmp, t2t3_h);

    // De-interleave
#if defined(__aarch64__)
    uint8x16_t t0 = vreinterpretq_u8_u64(
        vuzp1q_u64(vreinterpretq_u64_u8(t0t1_l), vreinterpretq_u64_u8(t0t1_h)));
    uint8x16_t t1 = vreinterpretq_u8_u64(
        vuzp2q_u64(vreinterpretq_u64_u8(t0t1_l), vreinterpretq_u64_u8(t0t1_h)));
    uint8x16_t t2 = vreinterpretq_u8_u64(
        vuzp1q_u64(vreinterpretq_u64_u8(t2t3_l), vreinterpretq_u64_u8(t2t3_h)));
    uint8x16_t t3 = vreinterpretq_u8_u64(
        vuzp2q_u64(vreinterpretq_u64_u8(t2t3_l), vreinterpretq_u64_u8(t2t3_h)));
#else
    uint8x16_t t1 = vcombine_u8(vget_high_u8(t0t1_l), vget_high_u8(t0t1_h));
    uint8x16_t t0 = vcombine_u8(vget_low_u8(t0t1_l), vget_low_u8(t0t1_h));
    uint8x16_t t3 = vcombine_u8(vget_high_u8(t2t3_l), vget_high_u8(t2t3_h));
    uint8x16_t t2 = vcombine_u8(vget_low_u8(t2t3_l), vget_low_u8(t2t3_h));
#endif
    // Shift the cross products
    uint8x16_t t0_shift = vextq_u8(t0, t0, 15);  // t0 << 8
    uint8x16_t t1_shift = vextq_u8(t1, t1, 14);  // t1 << 16
    uint8x16_t t2_shift = vextq_u8(t2, t2, 13);  // t2 << 24
    uint8x16_t t3_shift = vextq_u8(t3, t3, 12);  // t3 << 32

    // Accumulate the products
    uint8x16_t cross1 = veorq_u8(t0_shift, t1_shift);
    uint8x16_t cross2 = veorq_u8(t2_shift, t3_shift);
    uint8x16_t mix = veorq_u8(d, cross1);
    uint8x16_t r = veorq_u8(mix, cross2);
    return vreinterpretq_u64_u8(r);
}
#endif  // ARMv7 polyfill

// C equivalent:
//   __m128i _mm_shuffle_epi32_default(__m128i a,
//                                     __constrange(0, 255) int imm) {
//       __m128i ret;
//       ret[0] = a[imm        & 0x3];   ret[1] = a[(imm >> 2) & 0x3];
//       ret[2] = a[(imm >> 4) & 0x03];  ret[3] = a[(imm >> 6) & 0x03];
//       return ret;
//   }
#define _mm_shuffle_epi32_default(a, imm)                                   \
    __extension__({                                                         \
        int32x4_t ret;                                                      \
        ret = vmovq_n_s32(                                                  \
            vgetq_lane_s32(vreinterpretq_s32_m128i(a), (imm) & (0x3)));     \
        ret = vsetq_lane_s32(                                               \
            vgetq_lane_s32(vreinterpretq_s32_m128i(a), ((imm) >> 2) & 0x3), \
            ret, 1);                                                        \
        ret = vsetq_lane_s32(                                               \
            vgetq_lane_s32(vreinterpretq_s32_m128i(a), ((imm) >> 4) & 0x3), \
            ret, 2);                                                        \
        ret = vsetq_lane_s32(                                               \
            vgetq_lane_s32(vreinterpretq_s32_m128i(a), ((imm) >> 6) & 0x3), \
            ret, 3);                                                        \
        vreinterpretq_m128i_s32(ret);                                       \
    })

// Takes the upper 64 bits of a and places it in the low end of the result
// Takes the lower 64 bits of a and places it into the high end of the result.
FORCE_INLINE __m128i _mm_shuffle_epi_1032(__m128i a)
{
    int32x2_t a32 = vget_high_s32(vreinterpretq_s32_m128i(a));
    int32x2_t a10 = vget_low_s32(vreinterpretq_s32_m128i(a));
    return vreinterpretq_m128i_s32(vcombine_s32(a32, a10));
}

// takes the lower two 32-bit values from a and swaps them and places in low end
// of result takes the higher two 32 bit values from a and swaps them and places
// in high end of result.
FORCE_INLINE __m128i _mm_shuffle_epi_2301(__m128i a)
{
    int32x2_t a01 = vrev64_s32(vget_low_s32(vreinterpretq_s32_m128i(a)));
    int32x2_t a23 = vrev64_s32(vget_high_s32(vreinterpretq_s32_m128i(a)));
    return vreinterpretq_m128i_s32(vcombine_s32(a01, a23));
}

// rotates the least significant 32 bits into the most significant 32 bits, and
// shifts the rest down
FORCE_INLINE __m128i _mm_shuffle_epi_0321(__m128i a)
{
    return vreinterpretq_m128i_s32(
        vextq_s32(vreinterpretq_s32_m128i(a), vreinterpretq_s32_m128i(a), 1));
}

// rotates the most significant 32 bits into the least significant 32 bits, and
// shifts the rest up
FORCE_INLINE __m128i _mm_shuffle_epi_2103(__m128i a)
{
    return vreinterpretq_m128i_s32(
        vextq_s32(vreinterpretq_s32_m128i(a), vreinterpretq_s32_m128i(a), 3));
}

// gets the lower 64 bits of a, and places it in the upper 64 bits
// gets the lower 64 bits of a and places it in the lower 64 bits
FORCE_INLINE __m128i _mm_shuffle_epi_1010(__m128i a)
{
    int32x2_t a10 = vget_low_s32(vreinterpretq_s32_m128i(a));
    return vreinterpretq_m128i_s32(vcombine_s32(a10, a10));
}

// gets the lower 64 bits of a, swaps the 0 and 1 elements, and places it in the
// lower 64 bits gets the lower 64 bits of a, and places it in the upper 64 bits
FORCE_INLINE __m128i _mm_shuffle_epi_1001(__m128i a)
{
    int32x2_t a01 = vrev64_s32(vget_low_s32(vreinterpretq_s32_m128i(a)));
    int32x2_t a10 = vget_low_s32(vreinterpretq_s32_m128i(a));
    return vreinterpretq_m128i_s32(vcombine_s32(a01, a10));
}

// gets the lower 64 bits of a, swaps the 0 and 1 elements and places it in the
// upper 64 bits gets the lower 64 bits of a, swaps the 0 and 1 elements, and
// places it in the lower 64 bits
FORCE_INLINE __m128i _mm_shuffle_epi_0101(__m128i a)
{
    int32x2_t a01 = vrev64_s32(vget_low_s32(vreinterpretq_s32_m128i(a)));
    return vreinterpretq_m128i_s32(vcombine_s32(a01, a01));
}

FORCE_INLINE __m128i _mm_shuffle_epi_2211(__m128i a)
{
    int32x2_t a11 = vdup_lane_s32(vget_low_s32(vreinterpretq_s32_m128i(a)), 1);
    int32x2_t a22 = vdup_lane_s32(vget_high_s32(vreinterpretq_s32_m128i(a)), 0);
    return vreinterpretq_m128i_s32(vcombine_s32(a11, a22));
}

FORCE_INLINE __m128i _mm_shuffle_epi_0122(__m128i a)
{
    int32x2_t a22 = vdup_lane_s32(vget_high_s32(vreinterpretq_s32_m128i(a)), 0);
    int32x2_t a01 = vrev64_s32(vget_low_s32(vreinterpretq_s32_m128i(a)));
    return vreinterpretq_m128i_s32(vcombine_s32(a22, a01));
}

FORCE_INLINE __m128i _mm_shuffle_epi_3332(__m128i a)
{
    int32x2_t a32 = vget_high_s32(vreinterpretq_s32_m128i(a));
    int32x2_t a33 = vdup_lane_s32(vget_high_s32(vreinterpretq_s32_m128i(a)), 1);
    return vreinterpretq_m128i_s32(vcombine_s32(a32, a33));
}

// FORCE_INLINE __m128i _mm_shuffle_epi32_splat(__m128i a, __constrange(0,255)
// int imm)
#if defined(__aarch64__)
#define _mm_shuffle_epi32_splat(a, imm)                          \
    __extension__({                                              \
        vreinterpretq_m128i_s32(                                 \
            vdupq_laneq_s32(vreinterpretq_s32_m128i(a), (imm))); \
    })
#else
#define _mm_shuffle_epi32_splat(a, imm)                                      \
    __extension__({                                                          \
        vreinterpretq_m128i_s32(                                             \
            vdupq_n_s32(vgetq_lane_s32(vreinterpretq_s32_m128i(a), (imm)))); \
    })
#endif

// NEON does not support a general purpose permute intrinsic
// Selects four specific single-precision, floating-point values from a and b,
// based on the mask i.
//
// C equivalent:
//   __m128 _mm_shuffle_ps_default(__m128 a, __m128 b,
//                                 __constrange(0, 255) int imm) {
//       __m128 ret;
//       ret[0] = a[imm        & 0x3];   ret[1] = a[(imm >> 2) & 0x3];
//       ret[2] = b[(imm >> 4) & 0x03];  ret[3] = b[(imm >> 6) & 0x03];
//       return ret;
//   }
//
// https://msdn.microsoft.com/en-us/library/vstudio/5f0858x0(v=vs.100).aspx
#define _mm_shuffle_ps_default(a, b, imm)                                  \
    __extension__({                                                        \
        float32x4_t ret;                                                   \
        ret = vmovq_n_f32(                                                 \
            vgetq_lane_f32(vreinterpretq_f32_m128(a), (imm) & (0x3)));     \
        ret = vsetq_lane_f32(                                              \
            vgetq_lane_f32(vreinterpretq_f32_m128(a), ((imm) >> 2) & 0x3), \
            ret, 1);                                                       \
        ret = vsetq_lane_f32(                                              \
            vgetq_lane_f32(vreinterpretq_f32_m128(b), ((imm) >> 4) & 0x3), \
            ret, 2);                                                       \
        ret = vsetq_lane_f32(                                              \
            vgetq_lane_f32(vreinterpretq_f32_m128(b), ((imm) >> 6) & 0x3), \
            ret, 3);                                                       \
        vreinterpretq_m128_f32(ret);                                       \
    })

// Shuffles the lower 4 signed or unsigned 16-bit integers in a as specified
// by imm.
// https://docs.microsoft.com/en-us/previous-versions/visualstudio/visual-studio-2010/y41dkk37(v=vs.100)
// FORCE_INLINE __m128i _mm_shufflelo_epi16_function(__m128i a,
//                                                   __constrange(0,255) int
//                                                   imm)
#define _mm_shufflelo_epi16_function(a, imm)                                  \
    __extension__({                                                           \
        int16x8_t ret = vreinterpretq_s16_m128i(a);                           \
        int16x4_t lowBits = vget_low_s16(ret);                                \
        ret = vsetq_lane_s16(vget_lane_s16(lowBits, (imm) & (0x3)), ret, 0);  \
        ret = vsetq_lane_s16(vget_lane_s16(lowBits, ((imm) >> 2) & 0x3), ret, \
                             1);                                              \
        ret = vsetq_lane_s16(vget_lane_s16(lowBits, ((imm) >> 4) & 0x3), ret, \
                             2);                                              \
        ret = vsetq_lane_s16(vget_lane_s16(lowBits, ((imm) >> 6) & 0x3), ret, \
                             3);                                              \
        vreinterpretq_m128i_s16(ret);                                         \
    })

// Shuffles the upper 4 signed or unsigned 16-bit integers in a as specified
// by imm.
// https://msdn.microsoft.com/en-us/library/13ywktbs(v=vs.100).aspx
// FORCE_INLINE __m128i _mm_shufflehi_epi16_function(__m128i a,
//                                                   __constrange(0,255) int
//                                                   imm)
#define _mm_shufflehi_epi16_function(a, imm)                                   \
    __extension__({                                                            \
        int16x8_t ret = vreinterpretq_s16_m128i(a);                            \
        int16x4_t highBits = vget_high_s16(ret);                               \
        ret = vsetq_lane_s16(vget_lane_s16(highBits, (imm) & (0x3)), ret, 4);  \
        ret = vsetq_lane_s16(vget_lane_s16(highBits, ((imm) >> 2) & 0x3), ret, \
                             5);                                               \
        ret = vsetq_lane_s16(vget_lane_s16(highBits, ((imm) >> 4) & 0x3), ret, \
                             6);                                               \
        ret = vsetq_lane_s16(vget_lane_s16(highBits, ((imm) >> 6) & 0x3), ret, \
                             7);                                               \
        vreinterpretq_m128i_s16(ret);                                          \
    })

/* MMX */

//_mm_empty is a no-op on arm
FORCE_INLINE void _mm_empty(void) {}

/* SSE */

// Adds the four single-precision, floating-point values of a and b.
//
//   r0 := a0 + b0
//   r1 := a1 + b1
//   r2 := a2 + b2
//   r3 := a3 + b3
//
// https://msdn.microsoft.com/en-us/library/vstudio/c9848chc(v=vs.100).aspx
FORCE_INLINE __m128 _mm_add_ps(__m128 a, __m128 b)
{
    return vreinterpretq_m128_f32(
        vaddq_f32(vreinterpretq_f32_m128(a), vreinterpretq_f32_m128(b)));
}

// adds the scalar single-precision floating point values of a and b.
// https://msdn.microsoft.com/en-us/library/be94x2y6(v=vs.100).aspx
FORCE_INLINE __m128 _mm_add_ss(__m128 a, __m128 b)
{
    float32_t b0 = vgetq_lane_f32(vreinterpretq_f32_m128(b), 0);
    float32x4_t value = vsetq_lane_f32(b0, vdupq_n_f32(0), 0);
    // the upper values in the result must be the remnants of <a>.
    return vreinterpretq_m128_f32(vaddq_f32(a, value));
}

// Computes the bitwise AND of the four single-precision, floating-point values
// of a and b.
//
//   r0 := a0 & b0
//   r1 := a1 & b1
//   r2 := a2 & b2
//   r3 := a3 & b3
//
// https://msdn.microsoft.com/en-us/library/vstudio/73ck1xc5(v=vs.100).aspx
FORCE_INLINE __m128 _mm_and_ps(__m128 a, __m128 b)
{
    return vreinterpretq_m128_s32(
        vandq_s32(vreinterpretq_s32_m128(a), vreinterpretq_s32_m128(b)));
}

// Computes the bitwise AND-NOT of the four single-precision, floating-point
// values of a and b.
//
//   r0 := ~a0 & b0
//   r1 := ~a1 & b1
//   r2 := ~a2 & b2
//   r3 := ~a3 & b3
//
// https://msdn.microsoft.com/en-us/library/vstudio/68h7wd02(v=vs.100).aspx
FORCE_INLINE __m128 _mm_andnot_ps(__m128 a, __m128 b)
{
    return vreinterpretq_m128_s32(
        vbicq_s32(vreinterpretq_s32_m128(b),
                  vreinterpretq_s32_m128(a)));  // *NOTE* argument swap
}

// Average packed unsigned 16-bit integers in a and b, and store the results in
// dst.
//
//   FOR j := 0 to 3
//     i := j*16
//     dst[i+15:i] := (a[i+15:i] + b[i+15:i] + 1) >> 1
//   ENDFOR
//
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_avg_pu16
FORCE_INLINE __m64 _mm_avg_pu16(__m64 a, __m64 b)
{
    return vreinterpret_m64_u16(
        vrhadd_u16(vreinterpret_u16_m64(a), vreinterpret_u16_m64(b)));
}

// Average packed unsigned 8-bit integers in a and b, and store the results in
// dst.
//
//   FOR j := 0 to 7
//     i := j*8
//     dst[i+7:i] := (a[i+7:i] + b[i+7:i] + 1) >> 1
//   ENDFOR
//
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_avg_pu8
FORCE_INLINE __m64 _mm_avg_pu8(__m64 a, __m64 b)
{
    return vreinterpret_m64_u8(
        vrhadd_u8(vreinterpret_u8_m64(a), vreinterpret_u8_m64(b)));
}

// Compares for equality.
// https://msdn.microsoft.com/en-us/library/vstudio/36aectz5(v=vs.100).aspx
FORCE_INLINE __m128 _mm_cmpeq_ps(__m128 a, __m128 b)
{
    return vreinterpretq_m128_u32(
        vceqq_f32(vreinterpretq_f32_m128(a), vreinterpretq_f32_m128(b)));
}

// Compares for equality.
// https://docs.microsoft.com/en-us/previous-versions/visualstudio/visual-studio-2010/k423z28e(v=vs.100)
FORCE_INLINE __m128 _mm_cmpeq_ss(__m128 a, __m128 b)
{
    return _mm_move_ss(a, _mm_cmpeq_ps(a, b));
}

// Compares for greater than or equal.
// https://msdn.microsoft.com/en-us/library/vstudio/fs813y2t(v=vs.100).aspx
FORCE_INLINE __m128 _mm_cmpge_ps(__m128 a, __m128 b)
{
    return vreinterpretq_m128_u32(
        vcgeq_f32(vreinterpretq_f32_m128(a), vreinterpretq_f32_m128(b)));
}

// Compares for greater than or equal.
// https://docs.microsoft.com/en-us/previous-versions/visualstudio/visual-studio-2010/kesh3ddc(v=vs.100)
FORCE_INLINE __m128 _mm_cmpge_ss(__m128 a, __m128 b)
{
    return _mm_move_ss(a, _mm_cmpge_ps(a, b));
}

// Compares for greater than.
//
//   r0 := (a0 > b0) ? 0xffffffff : 0x0
//   r1 := (a1 > b1) ? 0xffffffff : 0x0
//   r2 := (a2 > b2) ? 0xffffffff : 0x0
//   r3 := (a3 > b3) ? 0xffffffff : 0x0
//
// https://msdn.microsoft.com/en-us/library/vstudio/11dy102s(v=vs.100).aspx
FORCE_INLINE __m128 _mm_cmpgt_ps(__m128 a, __m128 b)
{
    return vreinterpretq_m128_u32(
        vcgtq_f32(vreinterpretq_f32_m128(a), vreinterpretq_f32_m128(b)));
}

// Compares for greater than.
// https://docs.microsoft.com/en-us/previous-versions/visualstudio/visual-studio-2010/1xyyyy9e(v=vs.100)
FORCE_INLINE __m128 _mm_cmpgt_ss(__m128 a, __m128 b)
{
    return _mm_move_ss(a, _mm_cmpgt_ps(a, b));
}

// Compares for less than or equal.
//
//   r0 := (a0 <= b0) ? 0xffffffff : 0x0
//   r1 := (a1 <= b1) ? 0xffffffff : 0x0
//   r2 := (a2 <= b2) ? 0xffffffff : 0x0
//   r3 := (a3 <= b3) ? 0xffffffff : 0x0
//
// https://msdn.microsoft.com/en-us/library/vstudio/1s75w83z(v=vs.100).aspx
FORCE_INLINE __m128 _mm_cmple_ps(__m128 a, __m128 b)
{
    return vreinterpretq_m128_u32(
        vcleq_f32(vreinterpretq_f32_m128(a), vreinterpretq_f32_m128(b)));
}

// Compares for less than or equal.
// https://docs.microsoft.com/en-us/previous-versions/visualstudio/visual-studio-2010/a7x0hbhw(v=vs.100)
FORCE_INLINE __m128 _mm_cmple_ss(__m128 a, __m128 b)
{
    return _mm_move_ss(a, _mm_cmple_ps(a, b));
}

// Compares for less than
// https://msdn.microsoft.com/en-us/library/vstudio/f330yhc8(v=vs.100).aspx
FORCE_INLINE __m128 _mm_cmplt_ps(__m128 a, __m128 b)
{
    return vreinterpretq_m128_u32(
        vcltq_f32(vreinterpretq_f32_m128(a), vreinterpretq_f32_m128(b)));
}

// Compares for less than
// https://docs.microsoft.com/en-us/previous-versions/visualstudio/visual-studio-2010/fy94wye7(v=vs.100)
FORCE_INLINE __m128 _mm_cmplt_ss(__m128 a, __m128 b)
{
    return _mm_move_ss(a, _mm_cmplt_ps(a, b));
}

// Compares for inequality.
// https://msdn.microsoft.com/en-us/library/sf44thbx(v=vs.100).aspx
FORCE_INLINE __m128 _mm_cmpneq_ps(__m128 a, __m128 b)
{
    return vreinterpretq_m128_u32(vmvnq_u32(
        vceqq_f32(vreinterpretq_f32_m128(a), vreinterpretq_f32_m128(b))));
}

// Compares for inequality.
// https://docs.microsoft.com/en-us/previous-versions/visualstudio/visual-studio-2010/ekya8fh4(v=vs.100)
FORCE_INLINE __m128 _mm_cmpneq_ss(__m128 a, __m128 b)
{
    return _mm_move_ss(a, _mm_cmpneq_ps(a, b));
}

// Compares for not greater than or equal.
// https://docs.microsoft.com/en-us/previous-versions/visualstudio/visual-studio-2010/wsexys62(v=vs.100)
FORCE_INLINE __m128 _mm_cmpnge_ps(__m128 a, __m128 b)
{
    return vreinterpretq_m128_u32(vmvnq_u32(
        vcgeq_f32(vreinterpretq_f32_m128(a), vreinterpretq_f32_m128(b))));
}

// Compares for not greater than or equal.
// https://docs.microsoft.com/en-us/previous-versions/visualstudio/visual-studio-2010/fk2y80s8(v=vs.100)
FORCE_INLINE __m128 _mm_cmpnge_ss(__m128 a, __m128 b)
{
    return _mm_move_ss(a, _mm_cmpnge_ps(a, b));
}

// Compares for not greater than.
// https://docs.microsoft.com/en-us/previous-versions/visualstudio/visual-studio-2010/d0xh7w0s(v=vs.100)
FORCE_INLINE __m128 _mm_cmpngt_ps(__m128 a, __m128 b)
{
    return vreinterpretq_m128_u32(vmvnq_u32(
        vcgtq_f32(vreinterpretq_f32_m128(a), vreinterpretq_f32_m128(b))));
}

// Compares for not greater than.
// https://docs.microsoft.com/en-us/previous-versions/visualstudio/visual-studio-2010/z7x9ydwh(v=vs.100)
FORCE_INLINE __m128 _mm_cmpngt_ss(__m128 a, __m128 b)
{
    return _mm_move_ss(a, _mm_cmpngt_ps(a, b));
}

// Compares for not less than or equal.
// https://docs.microsoft.com/en-us/previous-versions/visualstudio/visual-studio-2010/6a330kxw(v=vs.100)
FORCE_INLINE __m128 _mm_cmpnle_ps(__m128 a, __m128 b)
{
    return vreinterpretq_m128_u32(vmvnq_u32(
        vcleq_f32(vreinterpretq_f32_m128(a), vreinterpretq_f32_m128(b))));
}

// Compares for not less than or equal.
// https://docs.microsoft.com/en-us/previous-versions/visualstudio/visual-studio-2010/z7x9ydwh(v=vs.100)
FORCE_INLINE __m128 _mm_cmpnle_ss(__m128 a, __m128 b)
{
    return _mm_move_ss(a, _mm_cmpnle_ps(a, b));
}

// Compares for not less than.
// https://docs.microsoft.com/en-us/previous-versions/visualstudio/visual-studio-2010/4686bbdw(v=vs.100)
FORCE_INLINE __m128 _mm_cmpnlt_ps(__m128 a, __m128 b)
{
    return vreinterpretq_m128_u32(vmvnq_u32(
        vcltq_f32(vreinterpretq_f32_m128(a), vreinterpretq_f32_m128(b))));
}

// Compares for not less than.
// https://docs.microsoft.com/en-us/previous-versions/visualstudio/visual-studio-2010/56b9z2wf(v=vs.100)
FORCE_INLINE __m128 _mm_cmpnlt_ss(__m128 a, __m128 b)
{
    return _mm_move_ss(a, _mm_cmpnlt_ps(a, b));
}

// Compares the four 32-bit floats in a and b to check if any values are NaN.
// Ordered compare between each value returns true for "orderable" and false for
// "not orderable" (NaN).
// https://msdn.microsoft.com/en-us/library/vstudio/0h9w00fx(v=vs.100).aspx see
// also:
// http://stackoverflow.com/questions/8627331/what-does-ordered-unordered-comparison-mean
// http://stackoverflow.com/questions/29349621/neon-isnanval-intrinsics
FORCE_INLINE __m128 _mm_cmpord_ps(__m128 a, __m128 b)
{
    // Note: NEON does not have ordered compare builtin
    // Need to compare a eq a and b eq b to check for NaN
    // Do AND of results to get final
    uint32x4_t ceqaa =
        vceqq_f32(vreinterpretq_f32_m128(a), vreinterpretq_f32_m128(a));
    uint32x4_t ceqbb =
        vceqq_f32(vreinterpretq_f32_m128(b), vreinterpretq_f32_m128(b));
    return vreinterpretq_m128_u32(vandq_u32(ceqaa, ceqbb));
}

// Compares for ordered.
// https://docs.microsoft.com/en-us/previous-versions/visualstudio/visual-studio-2010/343t62da(v=vs.100)
FORCE_INLINE __m128 _mm_cmpord_ss(__m128 a, __m128 b)
{
    return _mm_move_ss(a, _mm_cmpord_ps(a, b));
}

// Compares for unordered.
// https://docs.microsoft.com/en-us/previous-versions/visualstudio/visual-studio-2010/khy6fk1t(v=vs.100)
FORCE_INLINE __m128 _mm_cmpunord_ps(__m128 a, __m128 b)
{
    uint32x4_t f32a =
        vceqq_f32(vreinterpretq_f32_m128(a), vreinterpretq_f32_m128(a));
    uint32x4_t f32b =
        vceqq_f32(vreinterpretq_f32_m128(b), vreinterpretq_f32_m128(b));
    return vreinterpretq_m128_u32(vmvnq_u32(vandq_u32(f32a, f32b)));
}

// Compares for unordered.
// https://docs.microsoft.com/en-us/previous-versions/visualstudio/visual-studio-2010/2as2387b(v=vs.100)
FORCE_INLINE __m128 _mm_cmpunord_ss(__m128 a, __m128 b)
{
    return _mm_move_ss(a, _mm_cmpunord_ps(a, b));
}

// Compares the lower single-precision floating point scalar values of a and b
// using an equality operation. :
// https://msdn.microsoft.com/en-us/library/93yx2h2b(v=vs.100).aspx
FORCE_INLINE int _mm_comieq_ss(__m128 a, __m128 b)
{
    uint32x4_t a_eq_b =
        vceqq_f32(vreinterpretq_f32_m128(a), vreinterpretq_f32_m128(b));
    return vgetq_lane_u32(a_eq_b, 0) & 0x1;
}

// Compares the lower single-precision floating point scalar values of a and b
// using a greater than or equal operation. :
// https://msdn.microsoft.com/en-us/library/8t80des6(v=vs.100).aspx
FORCE_INLINE int _mm_comige_ss(__m128 a, __m128 b)
{
    uint32x4_t a_ge_b =
        vcgeq_f32(vreinterpretq_f32_m128(a), vreinterpretq_f32_m128(b));
    return vgetq_lane_u32(a_ge_b, 0) & 0x1;
}

// Compares the lower single-precision floating point scalar values of a and b
// using a greater than operation. :
// https://msdn.microsoft.com/en-us/library/b0738e0t(v=vs.100).aspx
FORCE_INLINE int _mm_comigt_ss(__m128 a, __m128 b)
{
    uint32x4_t a_gt_b =
        vcgtq_f32(vreinterpretq_f32_m128(a), vreinterpretq_f32_m128(b));
    return vgetq_lane_u32(a_gt_b, 0) & 0x1;
}

// Compares the lower single-precision floating point scalar values of a and b
// using a less than or equal operation. :
// https://msdn.microsoft.com/en-us/library/1w4t7c57(v=vs.90).aspx
FORCE_INLINE int _mm_comile_ss(__m128 a, __m128 b)
{
    uint32x4_t a_le_b =
        vcleq_f32(vreinterpretq_f32_m128(a), vreinterpretq_f32_m128(b));
    return vgetq_lane_u32(a_le_b, 0) & 0x1;
}

// Compares the lower single-precision floating point scalar values of a and b
// using a less than operation. :
// https://msdn.microsoft.com/en-us/library/2kwe606b(v=vs.90).aspx Important
// note!! The documentation on MSDN is incorrect!  If either of the values is a
// NAN the docs say you will get a one, but in fact, it will return a zero!!
FORCE_INLINE int _mm_comilt_ss(__m128 a, __m128 b)
{
    uint32x4_t a_lt_b =
        vcltq_f32(vreinterpretq_f32_m128(a), vreinterpretq_f32_m128(b));
    return vgetq_lane_u32(a_lt_b, 0) & 0x1;
}

// Compares the lower single-precision floating point scalar values of a and b
// using an inequality operation. :
// https://msdn.microsoft.com/en-us/library/bafh5e0a(v=vs.90).aspx
FORCE_INLINE int _mm_comineq_ss(__m128 a, __m128 b)
{
    return !_mm_comieq_ss(a, b);
}

// Convert packed signed 32-bit integers in b to packed single-precision
// (32-bit) floating-point elements, store the results in the lower 2 elements
// of dst, and copy the upper 2 packed elements from a to the upper elements of
// dst.
//
//   dst[31:0] := Convert_Int32_To_FP32(b[31:0])
//   dst[63:32] := Convert_Int32_To_FP32(b[63:32])
//   dst[95:64] := a[95:64]
//   dst[127:96] := a[127:96]
//
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_cvt_pi2ps
FORCE_INLINE __m128 _mm_cvt_pi2ps(__m128 a, __m64 b)
{
    return vreinterpretq_m128_f32(
        vcombine_f32(vcvt_f32_s32(vreinterpret_s32_m64(b)),
                     vget_high_f32(vreinterpretq_f32_m128(a))));
}

// Convert packed single-precision (32-bit) floating-point elements in a to
// packed 32-bit integers, and store the results in dst.
//
//   FOR j := 0 to 1
//       i := 32*j
//       dst[i+31:i] := Convert_FP32_To_Int32(a[i+31:i])
//   ENDFOR
//
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_cvt_ps2pi
FORCE_INLINE __m64 _mm_cvt_ps2pi(__m128 a)
{
#if defined(__aarch64__) || defined(__ARM_FEATURE_DIRECTED_ROUNDING)
    return vreinterpret_m64_s32(
        vget_low_s32(vcvtnq_s32_f32(vrndiq_f32(vreinterpretq_f32_m128(a)))));
#else
    return vreinterpret_m64_s32(vcvt_s32_f32(vget_low_f32(
        vreinterpretq_f32_m128(_mm_round_ps(a, _MM_FROUND_CUR_DIRECTION)))));
#endif
}

// Convert the signed 32-bit integer b to a single-precision (32-bit)
// floating-point element, store the result in the lower element of dst, and
// copy the upper 3 packed elements from a to the upper elements of dst.
//
//   dst[31:0] := Convert_Int32_To_FP32(b[31:0])
//   dst[127:32] := a[127:32]
//
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_cvt_si2ss
FORCE_INLINE __m128 _mm_cvt_si2ss(__m128 a, int b)
{
    return vreinterpretq_m128_f32(
        vsetq_lane_f32((float) b, vreinterpretq_f32_m128(a), 0));
}

// Convert the lower single-precision (32-bit) floating-point element in a to a
// 32-bit integer, and store the result in dst.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_cvt_ss2si
FORCE_INLINE int _mm_cvt_ss2si(__m128 a)
{
#if defined(__aarch64__) || defined(__ARM_FEATURE_DIRECTED_ROUNDING)
    return vgetq_lane_s32(vcvtnq_s32_f32(vrndiq_f32(vreinterpretq_f32_m128(a))),
                          0);
#else
    float32_t data = vgetq_lane_f32(
        vreinterpretq_f32_m128(_mm_round_ps(a, _MM_FROUND_CUR_DIRECTION)), 0);
    return (int32_t) data;
#endif
}

// Convert packed 16-bit integers in a to packed single-precision (32-bit)
// floating-point elements, and store the results in dst.
//
//   FOR j := 0 to 3
//      i := j*16
//      m := j*32
//      dst[m+31:m] := Convert_Int16_To_FP32(a[i+15:i])
//   ENDFOR
//
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_cvtpi16_ps
FORCE_INLINE __m128 _mm_cvtpi16_ps(__m64 a)
{
    return vreinterpretq_m128_f32(
        vcvtq_f32_s32(vmovl_s16(vreinterpret_s16_m64(a))));
}

// Convert packed 32-bit integers in b to packed single-precision (32-bit)
// floating-point elements, store the results in the lower 2 elements of dst,
// and copy the upper 2 packed elements from a to the upper elements of dst.
//
//   dst[31:0] := Convert_Int32_To_FP32(b[31:0])
//   dst[63:32] := Convert_Int32_To_FP32(b[63:32])
//   dst[95:64] := a[95:64]
//   dst[127:96] := a[127:96]
//
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_cvtpi32_ps
FORCE_INLINE __m128 _mm_cvtpi32_ps(__m128 a, __m64 b)
{
    return vreinterpretq_m128_f32(
        vcombine_f32(vcvt_f32_s32(vreinterpret_s32_m64(b)),
                     vget_high_f32(vreinterpretq_f32_m128(a))));
}

// Convert packed signed 32-bit integers in a to packed single-precision
// (32-bit) floating-point elements, store the results in the lower 2 elements
// of dst, then convert the packed signed 32-bit integers in b to
// single-precision (32-bit) floating-point element, and store the results in
// the upper 2 elements of dst.
//
//   dst[31:0] := Convert_Int32_To_FP32(a[31:0])
//   dst[63:32] := Convert_Int32_To_FP32(a[63:32])
//   dst[95:64] := Convert_Int32_To_FP32(b[31:0])
//   dst[127:96] := Convert_Int32_To_FP32(b[63:32])
//
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_cvtpi32x2_ps
FORCE_INLINE __m128 _mm_cvtpi32x2_ps(__m64 a, __m64 b)
{
    return vreinterpretq_m128_f32(vcvtq_f32_s32(
        vcombine_s32(vreinterpret_s32_m64(a), vreinterpret_s32_m64(b))));
}

// Convert the lower packed 8-bit integers in a to packed single-precision
// (32-bit) floating-point elements, and store the results in dst.
//
//   FOR j := 0 to 3
//      i := j*8
//      m := j*32
//      dst[m+31:m] := Convert_Int8_To_FP32(a[i+7:i])
//   ENDFOR
//
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_cvtpi8_ps
FORCE_INLINE __m128 _mm_cvtpi8_ps(__m64 a)
{
    return vreinterpretq_m128_f32(vcvtq_f32_s32(
        vmovl_s16(vget_low_s16(vmovl_s8(vreinterpret_s8_m64(a))))));
}

// Convert packed single-precision (32-bit) floating-point elements in a to
// packed 16-bit integers, and store the results in dst. Note: this intrinsic
// will generate 0x7FFF, rather than 0x8000, for input values between 0x7FFF and
// 0x7FFFFFFF.
//
//   FOR j := 0 to 3
//     i := 16*j
//     k := 32*j
//     IF a[k+31:k] >= FP32(0x7FFF) && a[k+31:k] <= FP32(0x7FFFFFFF)
//       dst[i+15:i] := 0x7FFF
//     ELSE
//       dst[i+15:i] := Convert_FP32_To_Int16(a[k+31:k])
//     FI
//   ENDFOR
//
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_cvtps_pi16
FORCE_INLINE __m64 _mm_cvtps_pi16(__m128 a)
{
    return vreinterpret_m64_s16(
        vqmovn_s32(vreinterpretq_s32_m128i(_mm_cvtps_epi32(a))));
}

// Convert packed single-precision (32-bit) floating-point elements in a to
// packed 32-bit integers, and store the results in dst.
//
//   FOR j := 0 to 1
//       i := 32*j
//       dst[i+31:i] := Convert_FP32_To_Int32(a[i+31:i])
//   ENDFOR
//
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_cvtps_pi32
#define _mm_cvtps_pi32(a) _mm_cvt_ps2pi(a)

// Convert packed single-precision (32-bit) floating-point elements in a to
// packed 8-bit integers, and store the results in lower 4 elements of dst.
// Note: this intrinsic will generate 0x7F, rather than 0x80, for input values
// between 0x7F and 0x7FFFFFFF.
//
//   FOR j := 0 to 3
//     i := 8*j
//     k := 32*j
//     IF a[k+31:k] >= FP32(0x7F) && a[k+31:k] <= FP32(0x7FFFFFFF)
//       dst[i+7:i] := 0x7F
//     ELSE
//       dst[i+7:i] := Convert_FP32_To_Int8(a[k+31:k])
//     FI
//   ENDFOR
//
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_cvtps_pi8
FORCE_INLINE __m64 _mm_cvtps_pi8(__m128 a)
{
    return vreinterpret_m64_s8(vqmovn_s16(
        vcombine_s16(vreinterpret_s16_m64(_mm_cvtps_pi16(a)), vdup_n_s16(0))));
}

// Convert packed unsigned 16-bit integers in a to packed single-precision
// (32-bit) floating-point elements, and store the results in dst.
//
//   FOR j := 0 to 3
//      i := j*16
//      m := j*32
//      dst[m+31:m] := Convert_UInt16_To_FP32(a[i+15:i])
//   ENDFOR
//
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_cvtpu16_ps
FORCE_INLINE __m128 _mm_cvtpu16_ps(__m64 a)
{
    return vreinterpretq_m128_f32(
        vcvtq_f32_u32(vmovl_u16(vreinterpret_u16_m64(a))));
}

// Convert the lower packed unsigned 8-bit integers in a to packed
// single-precision (32-bit) floating-point elements, and store the results in
// dst.
//
//   FOR j := 0 to 3
//      i := j*8
//      m := j*32
//      dst[m+31:m] := Convert_UInt8_To_FP32(a[i+7:i])
//   ENDFOR
//
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_cvtpu8_ps
FORCE_INLINE __m128 _mm_cvtpu8_ps(__m64 a)
{
    return vreinterpretq_m128_f32(vcvtq_f32_u32(
        vmovl_u16(vget_low_u16(vmovl_u8(vreinterpret_u8_m64(a))))));
}

// Convert the signed 32-bit integer b to a single-precision (32-bit)
// floating-point element, store the result in the lower element of dst, and
// copy the upper 3 packed elements from a to the upper elements of dst.
//
//   dst[31:0] := Convert_Int32_To_FP32(b[31:0])
//   dst[127:32] := a[127:32]
//
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_cvtsi32_ss
#define _mm_cvtsi32_ss(a, b) _mm_cvt_si2ss(a, b)

// Convert the signed 64-bit integer b to a single-precision (32-bit)
// floating-point element, store the result in the lower element of dst, and
// copy the upper 3 packed elements from a to the upper elements of dst.
//
//   dst[31:0] := Convert_Int64_To_FP32(b[63:0])
//   dst[127:32] := a[127:32]
//
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_cvtsi64_ss
FORCE_INLINE __m128 _mm_cvtsi64_ss(__m128 a, int64_t b)
{
    return vreinterpretq_m128_f32(
        vsetq_lane_f32((float) b, vreinterpretq_f32_m128(a), 0));
}

// Copy the lower single-precision (32-bit) floating-point element of a to dst.
//
//   dst[31:0] := a[31:0]
//
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_cvtss_f32
FORCE_INLINE float _mm_cvtss_f32(__m128 a)
{
    return vgetq_lane_f32(vreinterpretq_f32_m128(a), 0);
}

// Convert the lower single-precision (32-bit) floating-point element in a to a
// 32-bit integer, and store the result in dst.
//
//   dst[31:0] := Convert_FP32_To_Int32(a[31:0])
//
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_cvtss_si32
#define _mm_cvtss_si32(a) _mm_cvt_ss2si(a)

// Convert the lower single-precision (32-bit) floating-point element in a to a
// 64-bit integer, and store the result in dst.
//
//   dst[63:0] := Convert_FP32_To_Int64(a[31:0])
//
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_cvtss_si64
FORCE_INLINE int64_t _mm_cvtss_si64(__m128 a)
{
#if defined(__aarch64__) || defined(__ARM_FEATURE_DIRECTED_ROUNDING)
    return (int64_t) vgetq_lane_f32(vrndiq_f32(vreinterpretq_f32_m128(a)), 0);
#else
    float32_t data = vgetq_lane_f32(
        vreinterpretq_f32_m128(_mm_round_ps(a, _MM_FROUND_CUR_DIRECTION)), 0);
    return (int64_t) data;
#endif
}

// Convert packed single-precision (32-bit) floating-point elements in a to
// packed 32-bit integers with truncation, and store the results in dst.
//
//   FOR j := 0 to 1
//      i := 32*j
//      dst[i+31:i] := Convert_FP32_To_Int32_Truncate(a[i+31:i])
//   ENDFOR
//
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_cvtt_ps2pi
FORCE_INLINE __m64 _mm_cvtt_ps2pi(__m128 a)
{
    return vreinterpret_m64_s32(
        vget_low_s32(vcvtq_s32_f32(vreinterpretq_f32_m128(a))));
}

// Convert the lower single-precision (32-bit) floating-point element in a to a
// 32-bit integer with truncation, and store the result in dst.
//
//   dst[31:0] := Convert_FP32_To_Int32_Truncate(a[31:0])
//
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_cvtt_ss2si
FORCE_INLINE int _mm_cvtt_ss2si(__m128 a)
{
    return vgetq_lane_s32(vcvtq_s32_f32(vreinterpretq_f32_m128(a)), 0);
}

// Convert packed single-precision (32-bit) floating-point elements in a to
// packed 32-bit integers with truncation, and store the results in dst.
//
//   FOR j := 0 to 1
//      i := 32*j
//      dst[i+31:i] := Convert_FP32_To_Int32_Truncate(a[i+31:i])
//   ENDFOR
//
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_cvttps_pi32
#define _mm_cvttps_pi32(a) _mm_cvtt_ps2pi(a)

// Convert the lower single-precision (32-bit) floating-point element in a to a
// 32-bit integer with truncation, and store the result in dst.
//
//   dst[31:0] := Convert_FP32_To_Int32_Truncate(a[31:0])
//
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_cvttss_si32
#define _mm_cvttss_si32(a) _mm_cvtt_ss2si(a)

// Convert the lower single-precision (32-bit) floating-point element in a to a
// 64-bit integer with truncation, and store the result in dst.
//
//   dst[63:0] := Convert_FP32_To_Int64_Truncate(a[31:0])
//
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_cvttss_si64
FORCE_INLINE int64_t _mm_cvttss_si64(__m128 a)
{
    return (int64_t) vgetq_lane_f32(vreinterpretq_f32_m128(a), 0);
}

// Divides the four single-precision, floating-point values of a and b.
//
//   r0 := a0 / b0
//   r1 := a1 / b1
//   r2 := a2 / b2
//   r3 := a3 / b3
//
// https://msdn.microsoft.com/en-us/library/edaw8147(v=vs.100).aspx
FORCE_INLINE __m128 _mm_div_ps(__m128 a, __m128 b)
{
#if defined(__aarch64__) && !SSE2NEON_PRECISE_DIV
    return vreinterpretq_m128_f32(
        vdivq_f32(vreinterpretq_f32_m128(a), vreinterpretq_f32_m128(b)));
#else
    float32x4_t recip = vrecpeq_f32(vreinterpretq_f32_m128(b));
    recip = vmulq_f32(recip, vrecpsq_f32(recip, vreinterpretq_f32_m128(b)));
#if SSE2NEON_PRECISE_DIV
    // Additional Netwon-Raphson iteration for accuracy
    recip = vmulq_f32(recip, vrecpsq_f32(recip, vreinterpretq_f32_m128(b)));
#endif
    return vreinterpretq_m128_f32(vmulq_f32(vreinterpretq_f32_m128(a), recip));
#endif
}

// Divides the scalar single-precision floating point value of a by b.
// https://msdn.microsoft.com/en-us/library/4y73xa49(v=vs.100).aspx
FORCE_INLINE __m128 _mm_div_ss(__m128 a, __m128 b)
{
    float32_t value =
        vgetq_lane_f32(vreinterpretq_f32_m128(_mm_div_ps(a, b)), 0);
    return vreinterpretq_m128_f32(
        vsetq_lane_f32(value, vreinterpretq_f32_m128(a), 0));
}

// Extract a 16-bit integer from a, selected with imm8, and store the result in
// the lower element of dst.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_extract_pi16
#define _mm_extract_pi16(a, imm) \
    (int32_t) vget_lane_u16(vreinterpret_u16_m64(a), (imm))

// Free aligned memory that was allocated with _mm_malloc.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_free
#if !defined(SSE2NEON_ALLOC_DEFINED)
FORCE_INLINE void _mm_free(void *addr)
{
// -- GODOT start --
#if defined(_WIN32)
    _aligned_free(addr);
#else
    free(addr);
#endif
// -- GODOT end --
}
#endif

// Macro: Get the flush zero bits from the MXCSR control and status register.
// The flush zero may contain any of the following flags: _MM_FLUSH_ZERO_ON or
// _MM_FLUSH_ZERO_OFF
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_MM_GET_FLUSH_ZERO_MODE
FORCE_INLINE unsigned int _sse2neon_mm_get_flush_zero_mode()
{
    union {
        fpcr_bitfield field;
#if defined(__aarch64__)
        uint64_t value;
#else
        uint32_t value;
#endif
    } r;

#if defined(__aarch64__)
    __asm__ __volatile__("mrs %0, FPCR" : "=r"(r.value)); /* read */
#else
    __asm__ __volatile__("vmrs %0, FPSCR" : "=r"(r.value)); /* read */
#endif

    return r.field.bit24 ? _MM_FLUSH_ZERO_ON : _MM_FLUSH_ZERO_OFF;
}

// Macro: Get the rounding mode bits from the MXCSR control and status register.
// The rounding mode may contain any of the following flags: _MM_ROUND_NEAREST,
// _MM_ROUND_DOWN, _MM_ROUND_UP, _MM_ROUND_TOWARD_ZERO
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_MM_GET_ROUNDING_MODE
FORCE_INLINE unsigned int _MM_GET_ROUNDING_MODE()
{
    union {
        fpcr_bitfield field;
#if defined(__aarch64__)
        uint64_t value;
#else
        uint32_t value;
#endif
    } r;

#if defined(__aarch64__)
    __asm__ __volatile__("mrs %0, FPCR" : "=r"(r.value)); /* read */
#else
    __asm__ __volatile__("vmrs %0, FPSCR" : "=r"(r.value)); /* read */
#endif

    if (r.field.bit22) {
        return r.field.bit23 ? _MM_ROUND_TOWARD_ZERO : _MM_ROUND_UP;
    } else {
        return r.field.bit23 ? _MM_ROUND_DOWN : _MM_ROUND_NEAREST;
    }
}

// Copy a to dst, and insert the 16-bit integer i into dst at the location
// specified by imm8.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_insert_pi16
#define _mm_insert_pi16(a, b, imm)                               \
    __extension__({                                              \
        vreinterpret_m64_s16(                                    \
            vset_lane_s16((b), vreinterpret_s16_m64(a), (imm))); \
    })

// Loads four single-precision, floating-point values.
// https://msdn.microsoft.com/en-us/library/vstudio/zzd50xxt(v=vs.100).aspx
FORCE_INLINE __m128 _mm_load_ps(const float *p)
{
    return vreinterpretq_m128_f32(vld1q_f32(p));
}

// Load a single-precision (32-bit) floating-point element from memory into all
// elements of dst.
//
//   dst[31:0] := MEM[mem_addr+31:mem_addr]
//   dst[63:32] := MEM[mem_addr+31:mem_addr]
//   dst[95:64] := MEM[mem_addr+31:mem_addr]
//   dst[127:96] := MEM[mem_addr+31:mem_addr]
//
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_load_ps1
#define _mm_load_ps1 _mm_load1_ps

// Loads an single - precision, floating - point value into the low word and
// clears the upper three words.
// https://msdn.microsoft.com/en-us/library/548bb9h4%28v=vs.90%29.aspx
FORCE_INLINE __m128 _mm_load_ss(const float *p)
{
    return vreinterpretq_m128_f32(vsetq_lane_f32(*p, vdupq_n_f32(0), 0));
}

// Loads a single single-precision, floating-point value, copying it into all
// four words
// https://msdn.microsoft.com/en-us/library/vstudio/5cdkf716(v=vs.100).aspx
FORCE_INLINE __m128 _mm_load1_ps(const float *p)
{
    return vreinterpretq_m128_f32(vld1q_dup_f32(p));
}

// Sets the upper two single-precision, floating-point values with 64
// bits of data loaded from the address p; the lower two values are passed
// through from a.
//
//   r0 := a0
//   r1 := a1
//   r2 := *p0
//   r3 := *p1
//
// https://msdn.microsoft.com/en-us/library/w92wta0x(v%3dvs.100).aspx
FORCE_INLINE __m128 _mm_loadh_pi(__m128 a, __m64 const *p)
{
    return vreinterpretq_m128_f32(
        vcombine_f32(vget_low_f32(a), vld1_f32((const float32_t *) p)));
}

// Sets the lower two single-precision, floating-point values with 64
// bits of data loaded from the address p; the upper two values are passed
// through from a.
//
// Return Value
//   r0 := *p0
//   r1 := *p1
//   r2 := a2
//   r3 := a3
//
// https://msdn.microsoft.com/en-us/library/s57cyak2(v=vs.100).aspx
FORCE_INLINE __m128 _mm_loadl_pi(__m128 a, __m64 const *p)
{
    return vreinterpretq_m128_f32(
        vcombine_f32(vld1_f32((const float32_t *) p), vget_high_f32(a)));
}

// Load 4 single-precision (32-bit) floating-point elements from memory into dst
// in reverse order. mem_addr must be aligned on a 16-byte boundary or a
// general-protection exception may be generated.
//
//   dst[31:0] := MEM[mem_addr+127:mem_addr+96]
//   dst[63:32] := MEM[mem_addr+95:mem_addr+64]
//   dst[95:64] := MEM[mem_addr+63:mem_addr+32]
//   dst[127:96] := MEM[mem_addr+31:mem_addr]
//
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_loadr_ps
FORCE_INLINE __m128 _mm_loadr_ps(const float *p)
{
    float32x4_t v = vrev64q_f32(vld1q_f32(p));
    return vreinterpretq_m128_f32(vextq_f32(v, v, 2));
}

// Loads four single-precision, floating-point values.
// https://msdn.microsoft.com/en-us/library/x1b16s7z%28v=vs.90%29.aspx
FORCE_INLINE __m128 _mm_loadu_ps(const float *p)
{
    // for neon, alignment doesn't matter, so _mm_load_ps and _mm_loadu_ps are
    // equivalent for neon
    return vreinterpretq_m128_f32(vld1q_f32(p));
}

// Load unaligned 16-bit integer from memory into the first element of dst.
//
//   dst[15:0] := MEM[mem_addr+15:mem_addr]
//   dst[MAX:16] := 0
//
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_loadu_si16
FORCE_INLINE __m128i _mm_loadu_si16(const void *p)
{
    return vreinterpretq_m128i_s16(
        vsetq_lane_s16(*(const int16_t *) p, vdupq_n_s16(0), 0));
}

// Load unaligned 64-bit integer from memory into the first element of dst.
//
//   dst[63:0] := MEM[mem_addr+63:mem_addr]
//   dst[MAX:64] := 0
//
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_loadu_si64
FORCE_INLINE __m128i _mm_loadu_si64(const void *p)
{
    return vreinterpretq_m128i_s64(
        vcombine_s64(vld1_s64((const int64_t *) p), vdup_n_s64(0)));
}

// Allocate aligned blocks of memory.
// https://software.intel.com/en-us/
//         cpp-compiler-developer-guide-and-reference-allocating-and-freeing-aligned-memory-blocks
#if !defined(SSE2NEON_ALLOC_DEFINED)
FORCE_INLINE void *_mm_malloc(size_t size, size_t align)
{
    void *ptr;
    if (align == 1)
        return malloc(size);
    if (align == 2 || (sizeof(void *) == 8 && align == 4))
        align = sizeof(void *);
// -- GODOT start --
#if defined(_WIN32)
    ptr = _aligned_malloc(size, align);
    if (ptr)
        return ptr;
#else
    if (!posix_memalign(&ptr, align, size))
        return ptr;
#endif
// -- GODOT end --
    return NULL;
}
#endif

// Conditionally store 8-bit integer elements from a into memory using mask
// (elements are not stored when the highest bit is not set in the corresponding
// element) and a non-temporal memory hint.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_maskmove_si64
FORCE_INLINE void _mm_maskmove_si64(__m64 a, __m64 mask, char *mem_addr)
{
    int8x8_t shr_mask = vshr_n_s8(vreinterpret_s8_m64(mask), 7);
    __m128 b = _mm_load_ps((const float *) mem_addr);
    int8x8_t masked =
        vbsl_s8(vreinterpret_u8_s8(shr_mask), vreinterpret_s8_m64(a),
                vreinterpret_s8_u64(vget_low_u64(vreinterpretq_u64_m128(b))));
    vst1_s8((int8_t *) mem_addr, masked);
}

// Conditionally store 8-bit integer elements from a into memory using mask
// (elements are not stored when the highest bit is not set in the corresponding
// element) and a non-temporal memory hint.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_m_maskmovq
#define _m_maskmovq(a, mask, mem_addr) _mm_maskmove_si64(a, mask, mem_addr)

// Compare packed signed 16-bit integers in a and b, and store packed maximum
// values in dst.
//
//   FOR j := 0 to 3
//      i := j*16
//      dst[i+15:i] := MAX(a[i+15:i], b[i+15:i])
//   ENDFOR
//
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_max_pi16
FORCE_INLINE __m64 _mm_max_pi16(__m64 a, __m64 b)
{
    return vreinterpret_m64_s16(
        vmax_s16(vreinterpret_s16_m64(a), vreinterpret_s16_m64(b)));
}

// Computes the maximums of the four single-precision, floating-point values of
// a and b.
// https://msdn.microsoft.com/en-us/library/vstudio/ff5d607a(v=vs.100).aspx
FORCE_INLINE __m128 _mm_max_ps(__m128 a, __m128 b)
{
#if SSE2NEON_PRECISE_MINMAX
    float32x4_t _a = vreinterpretq_f32_m128(a);
    float32x4_t _b = vreinterpretq_f32_m128(b);
    return vreinterpretq_m128_f32(vbslq_f32(vcgtq_f32(_a, _b), _a, _b));
#else
    return vreinterpretq_m128_f32(
        vmaxq_f32(vreinterpretq_f32_m128(a), vreinterpretq_f32_m128(b)));
#endif
}

// Compare packed unsigned 8-bit integers in a and b, and store packed maximum
// values in dst.
//
//   FOR j := 0 to 7
//      i := j*8
//      dst[i+7:i] := MAX(a[i+7:i], b[i+7:i])
//   ENDFOR
//
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_max_pu8
FORCE_INLINE __m64 _mm_max_pu8(__m64 a, __m64 b)
{
    return vreinterpret_m64_u8(
        vmax_u8(vreinterpret_u8_m64(a), vreinterpret_u8_m64(b)));
}

// Computes the maximum of the two lower scalar single-precision floating point
// values of a and b.
// https://msdn.microsoft.com/en-us/library/s6db5esz(v=vs.100).aspx
FORCE_INLINE __m128 _mm_max_ss(__m128 a, __m128 b)
{
    float32_t value = vgetq_lane_f32(_mm_max_ps(a, b), 0);
    return vreinterpretq_m128_f32(
        vsetq_lane_f32(value, vreinterpretq_f32_m128(a), 0));
}

// Compare packed signed 16-bit integers in a and b, and store packed minimum
// values in dst.
//
//   FOR j := 0 to 3
//      i := j*16
//      dst[i+15:i] := MIN(a[i+15:i], b[i+15:i])
//   ENDFOR
//
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_min_pi16
FORCE_INLINE __m64 _mm_min_pi16(__m64 a, __m64 b)
{
    return vreinterpret_m64_s16(
        vmin_s16(vreinterpret_s16_m64(a), vreinterpret_s16_m64(b)));
}

// Computes the minima of the four single-precision, floating-point values of a
// and b.
// https://msdn.microsoft.com/en-us/library/vstudio/wh13kadz(v=vs.100).aspx
FORCE_INLINE __m128 _mm_min_ps(__m128 a, __m128 b)
{
#if SSE2NEON_PRECISE_MINMAX
    float32x4_t _a = vreinterpretq_f32_m128(a);
    float32x4_t _b = vreinterpretq_f32_m128(b);
    return vreinterpretq_m128_f32(vbslq_f32(vcltq_f32(_a, _b), _a, _b));
#else
    return vreinterpretq_m128_f32(
        vminq_f32(vreinterpretq_f32_m128(a), vreinterpretq_f32_m128(b)));
#endif
}

// Compare packed unsigned 8-bit integers in a and b, and store packed minimum
// values in dst.
//
//   FOR j := 0 to 7
//      i := j*8
//      dst[i+7:i] := MIN(a[i+7:i], b[i+7:i])
//   ENDFOR
//
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_min_pu8
FORCE_INLINE __m64 _mm_min_pu8(__m64 a, __m64 b)
{
    return vreinterpret_m64_u8(
        vmin_u8(vreinterpret_u8_m64(a), vreinterpret_u8_m64(b)));
}

// Computes the minimum of the two lower scalar single-precision floating point
// values of a and b.
// https://msdn.microsoft.com/en-us/library/0a9y7xaa(v=vs.100).aspx
FORCE_INLINE __m128 _mm_min_ss(__m128 a, __m128 b)
{
    float32_t value = vgetq_lane_f32(_mm_min_ps(a, b), 0);
    return vreinterpretq_m128_f32(
        vsetq_lane_f32(value, vreinterpretq_f32_m128(a), 0));
}

// Sets the low word to the single-precision, floating-point value of b
// https://docs.microsoft.com/en-us/previous-versions/visualstudio/visual-studio-2010/35hdzazd(v=vs.100)
FORCE_INLINE __m128 _mm_move_ss(__m128 a, __m128 b)
{
    return vreinterpretq_m128_f32(
        vsetq_lane_f32(vgetq_lane_f32(vreinterpretq_f32_m128(b), 0),
                       vreinterpretq_f32_m128(a), 0));
}

// Moves the upper two values of B into the lower two values of A.
//
//   r3 := a3
//   r2 := a2
//   r1 := b3
//   r0 := b2
FORCE_INLINE __m128 _mm_movehl_ps(__m128 __A, __m128 __B)
{
    float32x2_t a32 = vget_high_f32(vreinterpretq_f32_m128(__A));
    float32x2_t b32 = vget_high_f32(vreinterpretq_f32_m128(__B));
    return vreinterpretq_m128_f32(vcombine_f32(b32, a32));
}

// Moves the lower two values of B into the upper two values of A.
//
//   r3 := b1
//   r2 := b0
//   r1 := a1
//   r0 := a0
FORCE_INLINE __m128 _mm_movelh_ps(__m128 __A, __m128 __B)
{
    float32x2_t a10 = vget_low_f32(vreinterpretq_f32_m128(__A));
    float32x2_t b10 = vget_low_f32(vreinterpretq_f32_m128(__B));
    return vreinterpretq_m128_f32(vcombine_f32(a10, b10));
}

// Create mask from the most significant bit of each 8-bit element in a, and
// store the result in dst.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_movemask_pi8
FORCE_INLINE int _mm_movemask_pi8(__m64 a)
{
    uint8x8_t input = vreinterpret_u8_m64(a);
#if defined(__aarch64__)
    static const int8x8_t shift = {0, 1, 2, 3, 4, 5, 6, 7};
    uint8x8_t tmp = vshr_n_u8(input, 7);
    return vaddv_u8(vshl_u8(tmp, shift));
#else
    // Refer the implementation of `_mm_movemask_epi8`
    uint16x4_t high_bits = vreinterpret_u16_u8(vshr_n_u8(input, 7));
    uint32x2_t paired16 =
        vreinterpret_u32_u16(vsra_n_u16(high_bits, high_bits, 7));
    uint8x8_t paired32 =
        vreinterpret_u8_u32(vsra_n_u32(paired16, paired16, 14));
    return vget_lane_u8(paired32, 0) | ((int) vget_lane_u8(paired32, 4) << 4);
#endif
}

// NEON does not provide this method
// Creates a 4-bit mask from the most significant bits of the four
// single-precision, floating-point values.
// https://msdn.microsoft.com/en-us/library/vstudio/4490ys29(v=vs.100).aspx
FORCE_INLINE int _mm_movemask_ps(__m128 a)
{
    uint32x4_t input = vreinterpretq_u32_m128(a);
#if defined(__aarch64__)
    static const int32x4_t shift = {0, 1, 2, 3};
    uint32x4_t tmp = vshrq_n_u32(input, 31);
    return vaddvq_u32(vshlq_u32(tmp, shift));
#else
    // Uses the exact same method as _mm_movemask_epi8, see that for details.
    // Shift out everything but the sign bits with a 32-bit unsigned shift
    // right.
    uint64x2_t high_bits = vreinterpretq_u64_u32(vshrq_n_u32(input, 31));
    // Merge the two pairs together with a 64-bit unsigned shift right + add.
    uint8x16_t paired =
        vreinterpretq_u8_u64(vsraq_n_u64(high_bits, high_bits, 31));
    // Extract the result.
    return vgetq_lane_u8(paired, 0) | (vgetq_lane_u8(paired, 8) << 2);
#endif
}

// Multiplies the four single-precision, floating-point values of a and b.
//
//   r0 := a0 * b0
//   r1 := a1 * b1
//   r2 := a2 * b2
//   r3 := a3 * b3
//
// https://msdn.microsoft.com/en-us/library/vstudio/22kbk6t9(v=vs.100).aspx
FORCE_INLINE __m128 _mm_mul_ps(__m128 a, __m128 b)
{
    return vreinterpretq_m128_f32(
        vmulq_f32(vreinterpretq_f32_m128(a), vreinterpretq_f32_m128(b)));
}

// Multiply the lower single-precision (32-bit) floating-point element in a and
// b, store the result in the lower element of dst, and copy the upper 3 packed
// elements from a to the upper elements of dst.
//
//   dst[31:0] := a[31:0] * b[31:0]
//   dst[127:32] := a[127:32]
//
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_mul_ss
FORCE_INLINE __m128 _mm_mul_ss(__m128 a, __m128 b)
{
    return _mm_move_ss(a, _mm_mul_ps(a, b));
}

// Multiply the packed unsigned 16-bit integers in a and b, producing
// intermediate 32-bit integers, and store the high 16 bits of the intermediate
// integers in dst.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_mulhi_pu16
FORCE_INLINE __m64 _mm_mulhi_pu16(__m64 a, __m64 b)
{
    return vreinterpret_m64_u16(vshrn_n_u32(
        vmull_u16(vreinterpret_u16_m64(a), vreinterpret_u16_m64(b)), 16));
}

// Computes the bitwise OR of the four single-precision, floating-point values
// of a and b.
// https://msdn.microsoft.com/en-us/library/vstudio/7ctdsyy0(v=vs.100).aspx
FORCE_INLINE __m128 _mm_or_ps(__m128 a, __m128 b)
{
    return vreinterpretq_m128_s32(
        vorrq_s32(vreinterpretq_s32_m128(a), vreinterpretq_s32_m128(b)));
}

// Average packed unsigned 8-bit integers in a and b, and store the results in
// dst.
//
//   FOR j := 0 to 7
//     i := j*8
//     dst[i+7:i] := (a[i+7:i] + b[i+7:i] + 1) >> 1
//   ENDFOR
//
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_m_pavgb
#define _m_pavgb(a, b) _mm_avg_pu8(a, b)

// Average packed unsigned 16-bit integers in a and b, and store the results in
// dst.
//
//   FOR j := 0 to 3
//     i := j*16
//     dst[i+15:i] := (a[i+15:i] + b[i+15:i] + 1) >> 1
//   ENDFOR
//
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_m_pavgw
#define _m_pavgw(a, b) _mm_avg_pu16(a, b)

// Extract a 16-bit integer from a, selected with imm8, and store the result in
// the lower element of dst.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_m_pextrw
#define _m_pextrw(a, imm) _mm_extract_pi16(a, imm)

// Copy a to dst, and insert the 16-bit integer i into dst at the location
// specified by imm8.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=m_pinsrw
#define _m_pinsrw(a, i, imm) _mm_insert_pi16(a, i, imm)

// Compare packed signed 16-bit integers in a and b, and store packed maximum
// values in dst.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_m_pmaxsw
#define _m_pmaxsw(a, b) _mm_max_pi16(a, b)

// Compare packed unsigned 8-bit integers in a and b, and store packed maximum
// values in dst.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_m_pmaxub
#define _m_pmaxub(a, b) _mm_max_pu8(a, b)

// Compare packed signed 16-bit integers in a and b, and store packed minimum
// values in dst.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_m_pminsw
#define _m_pminsw(a, b) _mm_min_pi16(a, b)

// Compare packed unsigned 8-bit integers in a and b, and store packed minimum
// values in dst.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_m_pminub
#define _m_pminub(a, b) _mm_min_pu8(a, b)

// Create mask from the most significant bit of each 8-bit element in a, and
// store the result in dst.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_m_pmovmskb
#define _m_pmovmskb(a) _mm_movemask_pi8(a)

// Multiply the packed unsigned 16-bit integers in a and b, producing
// intermediate 32-bit integers, and store the high 16 bits of the intermediate
// integers in dst.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_m_pmulhuw
#define _m_pmulhuw(a, b) _mm_mulhi_pu16(a, b)

// Fetch the line of data from memory that contains address p to a location in
// the cache heirarchy specified by the locality hint i.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_prefetch
FORCE_INLINE void _mm_prefetch(char const *p, int i)
{
    switch (i) {
    case _MM_HINT_NTA:
        __builtin_prefetch(p, 0, 0);
        break;
    case _MM_HINT_T0:
        __builtin_prefetch(p, 0, 3);
        break;
    case _MM_HINT_T1:
        __builtin_prefetch(p, 0, 2);
        break;
    case _MM_HINT_T2:
        __builtin_prefetch(p, 0, 1);
        break;
    }
}

// Compute the absolute differences of packed unsigned 8-bit integers in a and
// b, then horizontally sum each consecutive 8 differences to produce four
// unsigned 16-bit integers, and pack these unsigned 16-bit integers in the low
// 16 bits of dst.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=m_psadbw
#define _m_psadbw(a, b) _mm_sad_pu8(a, b)

// Shuffle 16-bit integers in a using the control in imm8, and store the results
// in dst.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_m_pshufw
#define _m_pshufw(a, imm) _mm_shuffle_pi16(a, imm)

// Compute the approximate reciprocal of packed single-precision (32-bit)
// floating-point elements in a, and store the results in dst. The maximum
// relative error for this approximation is less than 1.5*2^-12.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_rcp_ps
FORCE_INLINE __m128 _mm_rcp_ps(__m128 in)
{
    float32x4_t recip = vrecpeq_f32(vreinterpretq_f32_m128(in));
    recip = vmulq_f32(recip, vrecpsq_f32(recip, vreinterpretq_f32_m128(in)));
#if SSE2NEON_PRECISE_DIV
    // Additional Netwon-Raphson iteration for accuracy
    recip = vmulq_f32(recip, vrecpsq_f32(recip, vreinterpretq_f32_m128(in)));
#endif
    return vreinterpretq_m128_f32(recip);
}

// Compute the approximate reciprocal of the lower single-precision (32-bit)
// floating-point element in a, store the result in the lower element of dst,
// and copy the upper 3 packed elements from a to the upper elements of dst. The
// maximum relative error for this approximation is less than 1.5*2^-12.
//
//   dst[31:0] := (1.0 / a[31:0])
//   dst[127:32] := a[127:32]
//
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_rcp_ss
FORCE_INLINE __m128 _mm_rcp_ss(__m128 a)
{
    return _mm_move_ss(a, _mm_rcp_ps(a));
}

// Computes the approximations of the reciprocal square roots of the four
// single-precision floating point values of in.
// The current precision is 1% error.
// https://msdn.microsoft.com/en-us/library/22hfsh53(v=vs.100).aspx
FORCE_INLINE __m128 _mm_rsqrt_ps(__m128 in)
{
    float32x4_t out = vrsqrteq_f32(vreinterpretq_f32_m128(in));
#if SSE2NEON_PRECISE_SQRT
    // Additional Netwon-Raphson iteration for accuracy
    out = vmulq_f32(
        out, vrsqrtsq_f32(vmulq_f32(vreinterpretq_f32_m128(in), out), out));
    out = vmulq_f32(
        out, vrsqrtsq_f32(vmulq_f32(vreinterpretq_f32_m128(in), out), out));
#endif
    return vreinterpretq_m128_f32(out);
}

// Compute the approximate reciprocal square root of the lower single-precision
// (32-bit) floating-point element in a, store the result in the lower element
// of dst, and copy the upper 3 packed elements from a to the upper elements of
// dst.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_rsqrt_ss
FORCE_INLINE __m128 _mm_rsqrt_ss(__m128 in)
{
    return vsetq_lane_f32(vgetq_lane_f32(_mm_rsqrt_ps(in), 0), in, 0);
}

// Compute the absolute differences of packed unsigned 8-bit integers in a and
// b, then horizontally sum each consecutive 8 differences to produce four
// unsigned 16-bit integers, and pack these unsigned 16-bit integers in the low
// 16 bits of dst.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_sad_pu8
FORCE_INLINE __m64 _mm_sad_pu8(__m64 a, __m64 b)
{
    uint64x1_t t = vpaddl_u32(vpaddl_u16(
        vpaddl_u8(vabd_u8(vreinterpret_u8_m64(a), vreinterpret_u8_m64(b)))));
    return vreinterpret_m64_u16(
        vset_lane_u16(vget_lane_u64(t, 0), vdup_n_u16(0), 0));
}

// Macro: Set the flush zero bits of the MXCSR control and status register to
// the value in unsigned 32-bit integer a. The flush zero may contain any of the
// following flags: _MM_FLUSH_ZERO_ON or _MM_FLUSH_ZERO_OFF
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_MM_SET_FLUSH_ZERO_MODE
FORCE_INLINE void _sse2neon_mm_set_flush_zero_mode(unsigned int flag)
{
    // AArch32 Advanced SIMD arithmetic always uses the Flush-to-zero setting,
    // regardless of the value of the FZ bit.
    union {
        fpcr_bitfield field;
#if defined(__aarch64__)
        uint64_t value;
#else
        uint32_t value;
#endif
    } r;

#if defined(__aarch64__)
    __asm__ __volatile__("mrs %0, FPCR" : "=r"(r.value)); /* read */
#else
    __asm__ __volatile__("vmrs %0, FPSCR" : "=r"(r.value)); /* read */
#endif

    r.field.bit24 = (flag & _MM_FLUSH_ZERO_MASK) == _MM_FLUSH_ZERO_ON;

#if defined(__aarch64__)
    __asm__ __volatile__("msr FPCR, %0" ::"r"(r)); /* write */
#else
    __asm__ __volatile__("vmsr FPSCR, %0" ::"r"(r));        /* write */
#endif
}

// Sets the four single-precision, floating-point values to the four inputs.
// https://msdn.microsoft.com/en-us/library/vstudio/afh0zf75(v=vs.100).aspx
FORCE_INLINE __m128 _mm_set_ps(float w, float z, float y, float x)
{
    float ALIGN_STRUCT(16) data[4] = {x, y, z, w};
    return vreinterpretq_m128_f32(vld1q_f32(data));
}

// Sets the four single-precision, floating-point values to w.
// https://msdn.microsoft.com/en-us/library/vstudio/2x1se8ha(v=vs.100).aspx
FORCE_INLINE __m128 _mm_set_ps1(float _w)
{
    return vreinterpretq_m128_f32(vdupq_n_f32(_w));
}

// Macro: Set the rounding mode bits of the MXCSR control and status register to
// the value in unsigned 32-bit integer a. The rounding mode may contain any of
// the following flags: _MM_ROUND_NEAREST, _MM_ROUND_DOWN, _MM_ROUND_UP,
// _MM_ROUND_TOWARD_ZERO
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_MM_SET_ROUNDING_MODE
FORCE_INLINE void _MM_SET_ROUNDING_MODE(int rounding)
{
    union {
        fpcr_bitfield field;
#if defined(__aarch64__)
        uint64_t value;
#else
        uint32_t value;
#endif
    } r;

#if defined(__aarch64__)
    __asm__ __volatile__("mrs %0, FPCR" : "=r"(r.value)); /* read */
#else
    __asm__ __volatile__("vmrs %0, FPSCR" : "=r"(r.value)); /* read */
#endif

    switch (rounding) {
    case _MM_ROUND_TOWARD_ZERO:
        r.field.bit22 = 1;
        r.field.bit23 = 1;
        break;
    case _MM_ROUND_DOWN:
        r.field.bit22 = 0;
        r.field.bit23 = 1;
        break;
    case _MM_ROUND_UP:
        r.field.bit22 = 1;
        r.field.bit23 = 0;
        break;
    default:  //_MM_ROUND_NEAREST
        r.field.bit22 = 0;
        r.field.bit23 = 0;
    }

#if defined(__aarch64__)
    __asm__ __volatile__("msr FPCR, %0" ::"r"(r)); /* write */
#else
    __asm__ __volatile__("vmsr FPSCR, %0" ::"r"(r));        /* write */
#endif
}

// Copy single-precision (32-bit) floating-point element a to the lower element
// of dst, and zero the upper 3 elements.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_set_ss
FORCE_INLINE __m128 _mm_set_ss(float a)
{
    return vreinterpretq_m128_f32(vsetq_lane_f32(a, vdupq_n_f32(0), 0));
}

// Sets the four single-precision, floating-point values to w.
//
//   r0 := r1 := r2 := r3 := w
//
// https://msdn.microsoft.com/en-us/library/vstudio/2x1se8ha(v=vs.100).aspx
FORCE_INLINE __m128 _mm_set1_ps(float _w)
{
    return vreinterpretq_m128_f32(vdupq_n_f32(_w));
}

// FIXME: _mm_setcsr() implementation supports changing the rounding mode only.
FORCE_INLINE void _mm_setcsr(unsigned int a)
{
    _MM_SET_ROUNDING_MODE(a);
}

// FIXME: _mm_getcsr() implementation supports reading the rounding mode only.
FORCE_INLINE unsigned int _mm_getcsr()
{
    return _MM_GET_ROUNDING_MODE();
}

// Sets the four single-precision, floating-point values to the four inputs in
// reverse order.
// https://msdn.microsoft.com/en-us/library/vstudio/d2172ct3(v=vs.100).aspx
FORCE_INLINE __m128 _mm_setr_ps(float w, float z, float y, float x)
{
    float ALIGN_STRUCT(16) data[4] = {w, z, y, x};
    return vreinterpretq_m128_f32(vld1q_f32(data));
}

// Clears the four single-precision, floating-point values.
// https://msdn.microsoft.com/en-us/library/vstudio/tk1t2tbz(v=vs.100).aspx
FORCE_INLINE __m128 _mm_setzero_ps(void)
{
    return vreinterpretq_m128_f32(vdupq_n_f32(0));
}

// Shuffle 16-bit integers in a using the control in imm8, and store the results
// in dst.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_shuffle_pi16
#ifdef _sse2neon_shuffle
#define _mm_shuffle_pi16(a, imm)                                           \
    __extension__({                                                        \
        vreinterpret_m64_s16(vshuffle_s16(                                 \
            vreinterpret_s16_m64(a), vreinterpret_s16_m64(a), (imm & 0x3), \
            ((imm >> 2) & 0x3), ((imm >> 4) & 0x3), ((imm >> 6) & 0x3)));  \
    })
#else
#define _mm_shuffle_pi16(a, imm)                                               \
    __extension__({                                                            \
        int16x4_t ret;                                                         \
        ret =                                                                  \
            vmov_n_s16(vget_lane_s16(vreinterpret_s16_m64(a), (imm) & (0x3))); \
        ret = vset_lane_s16(                                                   \
            vget_lane_s16(vreinterpret_s16_m64(a), ((imm) >> 2) & 0x3), ret,   \
            1);                                                                \
        ret = vset_lane_s16(                                                   \
            vget_lane_s16(vreinterpret_s16_m64(a), ((imm) >> 4) & 0x3), ret,   \
            2);                                                                \
        ret = vset_lane_s16(                                                   \
            vget_lane_s16(vreinterpret_s16_m64(a), ((imm) >> 6) & 0x3), ret,   \
            3);                                                                \
        vreinterpret_m64_s16(ret);                                             \
    })
#endif

// Perform a serializing operation on all store-to-memory instructions that were
// issued prior to this instruction. Guarantees that every store instruction
// that precedes, in program order, is globally visible before any store
// instruction which follows the fence in program order.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_sfence
FORCE_INLINE void _mm_sfence(void)
{
    _sse2neon_smp_mb();
}

// Perform a serializing operation on all load-from-memory and store-to-memory
// instructions that were issued prior to this instruction. Guarantees that
// every memory access that precedes, in program order, the memory fence
// instruction is globally visible before any memory instruction which follows
// the fence in program order.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_mfence
FORCE_INLINE void _mm_mfence(void)
{
    _sse2neon_smp_mb();
}

// Perform a serializing operation on all load-from-memory instructions that
// were issued prior to this instruction. Guarantees that every load instruction
// that precedes, in program order, is globally visible before any load
// instruction which follows the fence in program order.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_lfence
FORCE_INLINE void _mm_lfence(void)
{
    _sse2neon_smp_mb();
}

// FORCE_INLINE __m128 _mm_shuffle_ps(__m128 a, __m128 b, __constrange(0,255)
// int imm)
#ifdef _sse2neon_shuffle
#define _mm_shuffle_ps(a, b, imm)                                              \
    __extension__({                                                            \
        float32x4_t _input1 = vreinterpretq_f32_m128(a);                       \
        float32x4_t _input2 = vreinterpretq_f32_m128(b);                       \
        float32x4_t _shuf =                                                    \
            vshuffleq_s32(_input1, _input2, (imm) & (0x3), ((imm) >> 2) & 0x3, \
                          (((imm) >> 4) & 0x3) + 4, (((imm) >> 6) & 0x3) + 4); \
        vreinterpretq_m128_f32(_shuf);                                         \
    })
#else  // generic
#define _mm_shuffle_ps(a, b, imm)                          \
    __extension__({                                        \
        __m128 ret;                                        \
        switch (imm) {                                     \
        case _MM_SHUFFLE(1, 0, 3, 2):                      \
            ret = _mm_shuffle_ps_1032((a), (b));           \
            break;                                         \
        case _MM_SHUFFLE(2, 3, 0, 1):                      \
            ret = _mm_shuffle_ps_2301((a), (b));           \
            break;                                         \
        case _MM_SHUFFLE(0, 3, 2, 1):                      \
            ret = _mm_shuffle_ps_0321((a), (b));           \
            break;                                         \
        case _MM_SHUFFLE(2, 1, 0, 3):                      \
            ret = _mm_shuffle_ps_2103((a), (b));           \
            break;                                         \
        case _MM_SHUFFLE(1, 0, 1, 0):                      \
            ret = _mm_movelh_ps((a), (b));                 \
            break;                                         \
        case _MM_SHUFFLE(1, 0, 0, 1):                      \
            ret = _mm_shuffle_ps_1001((a), (b));           \
            break;                                         \
        case _MM_SHUFFLE(0, 1, 0, 1):                      \
            ret = _mm_shuffle_ps_0101((a), (b));           \
            break;                                         \
        case _MM_SHUFFLE(3, 2, 1, 0):                      \
            ret = _mm_shuffle_ps_3210((a), (b));           \
            break;                                         \
        case _MM_SHUFFLE(0, 0, 1, 1):                      \
            ret = _mm_shuffle_ps_0011((a), (b));           \
            break;                                         \
        case _MM_SHUFFLE(0, 0, 2, 2):                      \
            ret = _mm_shuffle_ps_0022((a), (b));           \
            break;                                         \
        case _MM_SHUFFLE(2, 2, 0, 0):                      \
            ret = _mm_shuffle_ps_2200((a), (b));           \
            break;                                         \
        case _MM_SHUFFLE(3, 2, 0, 2):                      \
            ret = _mm_shuffle_ps_3202((a), (b));           \
            break;                                         \
        case _MM_SHUFFLE(3, 2, 3, 2):                      \
            ret = _mm_movehl_ps((b), (a));                 \
            break;                                         \
        case _MM_SHUFFLE(1, 1, 3, 3):                      \
            ret = _mm_shuffle_ps_1133((a), (b));           \
            break;                                         \
        case _MM_SHUFFLE(2, 0, 1, 0):                      \
            ret = _mm_shuffle_ps_2010((a), (b));           \
            break;                                         \
        case _MM_SHUFFLE(2, 0, 0, 1):                      \
            ret = _mm_shuffle_ps_2001((a), (b));           \
            break;                                         \
        case _MM_SHUFFLE(2, 0, 3, 2):                      \
            ret = _mm_shuffle_ps_2032((a), (b));           \
            break;                                         \
        default:                                           \
            ret = _mm_shuffle_ps_default((a), (b), (imm)); \
            break;                                         \
        }                                                  \
        ret;                                               \
    })
#endif

// Computes the approximations of square roots of the four single-precision,
// floating-point values of a. First computes reciprocal square roots and then
// reciprocals of the four values.
//
//   r0 := sqrt(a0)
//   r1 := sqrt(a1)
//   r2 := sqrt(a2)
//   r3 := sqrt(a3)
//
// https://msdn.microsoft.com/en-us/library/vstudio/8z67bwwk(v=vs.100).aspx
FORCE_INLINE __m128 _mm_sqrt_ps(__m128 in)
{
#if SSE2NEON_PRECISE_SQRT
    float32x4_t recip = vrsqrteq_f32(vreinterpretq_f32_m128(in));

    // Test for vrsqrteq_f32(0) -> positive infinity case.
    // Change to zero, so that s * 1/sqrt(s) result is zero too.
    const uint32x4_t pos_inf = vdupq_n_u32(0x7F800000);
    const uint32x4_t div_by_zero =
        vceqq_u32(pos_inf, vreinterpretq_u32_f32(recip));
    recip = vreinterpretq_f32_u32(
        vandq_u32(vmvnq_u32(div_by_zero), vreinterpretq_u32_f32(recip)));

    // Additional Netwon-Raphson iteration for accuracy
    recip = vmulq_f32(
        vrsqrtsq_f32(vmulq_f32(recip, recip), vreinterpretq_f32_m128(in)),
        recip);
    recip = vmulq_f32(
        vrsqrtsq_f32(vmulq_f32(recip, recip), vreinterpretq_f32_m128(in)),
        recip);

    // sqrt(s) = s * 1/sqrt(s)
    return vreinterpretq_m128_f32(vmulq_f32(vreinterpretq_f32_m128(in), recip));
#elif defined(__aarch64__)
    return vreinterpretq_m128_f32(vsqrtq_f32(vreinterpretq_f32_m128(in)));
#else
    float32x4_t recipsq = vrsqrteq_f32(vreinterpretq_f32_m128(in));
    float32x4_t sq = vrecpeq_f32(recipsq);
    return vreinterpretq_m128_f32(sq);
#endif
}

// Computes the approximation of the square root of the scalar single-precision
// floating point value of in.
// https://msdn.microsoft.com/en-us/library/ahfsc22d(v=vs.100).aspx
FORCE_INLINE __m128 _mm_sqrt_ss(__m128 in)
{
    float32_t value =
        vgetq_lane_f32(vreinterpretq_f32_m128(_mm_sqrt_ps(in)), 0);
    return vreinterpretq_m128_f32(
        vsetq_lane_f32(value, vreinterpretq_f32_m128(in), 0));
}

// Stores four single-precision, floating-point values.
// https://msdn.microsoft.com/en-us/library/vstudio/s3h4ay6y(v=vs.100).aspx
FORCE_INLINE void _mm_store_ps(float *p, __m128 a)
{
    vst1q_f32(p, vreinterpretq_f32_m128(a));
}

// Store the lower single-precision (32-bit) floating-point element from a into
// 4 contiguous elements in memory. mem_addr must be aligned on a 16-byte
// boundary or a general-protection exception may be generated.
//
//   MEM[mem_addr+31:mem_addr] := a[31:0]
//   MEM[mem_addr+63:mem_addr+32] := a[31:0]
//   MEM[mem_addr+95:mem_addr+64] := a[31:0]
//   MEM[mem_addr+127:mem_addr+96] := a[31:0]
//
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_store_ps1
FORCE_INLINE void _mm_store_ps1(float *p, __m128 a)
{
    float32_t a0 = vgetq_lane_f32(vreinterpretq_f32_m128(a), 0);
    vst1q_f32(p, vdupq_n_f32(a0));
}

// Stores the lower single - precision, floating - point value.
// https://msdn.microsoft.com/en-us/library/tzz10fbx(v=vs.100).aspx
FORCE_INLINE void _mm_store_ss(float *p, __m128 a)
{
    vst1q_lane_f32(p, vreinterpretq_f32_m128(a), 0);
}

// Store the lower single-precision (32-bit) floating-point element from a into
// 4 contiguous elements in memory. mem_addr must be aligned on a 16-byte
// boundary or a general-protection exception may be generated.
//
//   MEM[mem_addr+31:mem_addr] := a[31:0]
//   MEM[mem_addr+63:mem_addr+32] := a[31:0]
//   MEM[mem_addr+95:mem_addr+64] := a[31:0]
//   MEM[mem_addr+127:mem_addr+96] := a[31:0]
//
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_store1_ps
#define _mm_store1_ps _mm_store_ps1

// Stores the upper two single-precision, floating-point values of a to the
// address p.
//
//   *p0 := a2
//   *p1 := a3
//
// https://msdn.microsoft.com/en-us/library/a7525fs8(v%3dvs.90).aspx
FORCE_INLINE void _mm_storeh_pi(__m64 *p, __m128 a)
{
    *p = vreinterpret_m64_f32(vget_high_f32(a));
}

// Stores the lower two single-precision floating point values of a to the
// address p.
//
//   *p0 := a0
//   *p1 := a1
//
// https://msdn.microsoft.com/en-us/library/h54t98ks(v=vs.90).aspx
FORCE_INLINE void _mm_storel_pi(__m64 *p, __m128 a)
{
    *p = vreinterpret_m64_f32(vget_low_f32(a));
}

// Store 4 single-precision (32-bit) floating-point elements from a into memory
// in reverse order. mem_addr must be aligned on a 16-byte boundary or a
// general-protection exception may be generated.
//
//   MEM[mem_addr+31:mem_addr] := a[127:96]
//   MEM[mem_addr+63:mem_addr+32] := a[95:64]
//   MEM[mem_addr+95:mem_addr+64] := a[63:32]
//   MEM[mem_addr+127:mem_addr+96] := a[31:0]
//
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_storer_ps
FORCE_INLINE void _mm_storer_ps(float *p, __m128 a)
{
    float32x4_t tmp = vrev64q_f32(vreinterpretq_f32_m128(a));
    float32x4_t rev = vextq_f32(tmp, tmp, 2);
    vst1q_f32(p, rev);
}

// Stores four single-precision, floating-point values.
// https://msdn.microsoft.com/en-us/library/44e30x22(v=vs.100).aspx
FORCE_INLINE void _mm_storeu_ps(float *p, __m128 a)
{
    vst1q_f32(p, vreinterpretq_f32_m128(a));
}

// Stores 16-bits of integer data a at the address p.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_storeu_si16
FORCE_INLINE void _mm_storeu_si16(void *p, __m128i a)
{
    vst1q_lane_s16((int16_t *) p, vreinterpretq_s16_m128i(a), 0);
}

// Stores 64-bits of integer data a at the address p.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_storeu_si64
FORCE_INLINE void _mm_storeu_si64(void *p, __m128i a)
{
    vst1q_lane_s64((int64_t *) p, vreinterpretq_s64_m128i(a), 0);
}

// Store 64-bits of integer data from a into memory using a non-temporal memory
// hint.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_stream_pi
FORCE_INLINE void _mm_stream_pi(__m64 *p, __m64 a)
{
    vst1_s64((int64_t *) p, vreinterpret_s64_m64(a));
}

// Store 128-bits (composed of 4 packed single-precision (32-bit) floating-
// point elements) from a into memory using a non-temporal memory hint.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_stream_ps
FORCE_INLINE void _mm_stream_ps(float *p, __m128 a)
{
#if __has_builtin(__builtin_nontemporal_store)
    __builtin_nontemporal_store(reinterpret_cast<float32x4_t>(a), (float32x4_t *) p);
#else
    vst1q_f32(p, vreinterpretq_f32_m128(a));
#endif
}

// Subtracts the four single-precision, floating-point values of a and b.
//
//   r0 := a0 - b0
//   r1 := a1 - b1
//   r2 := a2 - b2
//   r3 := a3 - b3
//
// https://msdn.microsoft.com/en-us/library/vstudio/1zad2k61(v=vs.100).aspx
FORCE_INLINE __m128 _mm_sub_ps(__m128 a, __m128 b)
{
    return vreinterpretq_m128_f32(
        vsubq_f32(vreinterpretq_f32_m128(a), vreinterpretq_f32_m128(b)));
}

// Subtract the lower single-precision (32-bit) floating-point element in b from
// the lower single-precision (32-bit) floating-point element in a, store the
// result in the lower element of dst, and copy the upper 3 packed elements from
// a to the upper elements of dst.
//
//   dst[31:0] := a[31:0] - b[31:0]
//   dst[127:32] := a[127:32]
//
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_sub_ss
FORCE_INLINE __m128 _mm_sub_ss(__m128 a, __m128 b)
{
    return _mm_move_ss(a, _mm_sub_ps(a, b));
}

// Macro: Transpose the 4x4 matrix formed by the 4 rows of single-precision
// (32-bit) floating-point elements in row0, row1, row2, and row3, and store the
// transposed matrix in these vectors (row0 now contains column 0, etc.).
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=MM_TRANSPOSE4_PS
#define _MM_TRANSPOSE4_PS(row0, row1, row2, row3)         \
    do {                                                  \
        float32x4x2_t ROW01 = vtrnq_f32(row0, row1);      \
        float32x4x2_t ROW23 = vtrnq_f32(row2, row3);      \
        row0 = vcombine_f32(vget_low_f32(ROW01.val[0]),   \
                            vget_low_f32(ROW23.val[0]));  \
        row1 = vcombine_f32(vget_low_f32(ROW01.val[1]),   \
                            vget_low_f32(ROW23.val[1]));  \
        row2 = vcombine_f32(vget_high_f32(ROW01.val[0]),  \
                            vget_high_f32(ROW23.val[0])); \
        row3 = vcombine_f32(vget_high_f32(ROW01.val[1]),  \
                            vget_high_f32(ROW23.val[1])); \
    } while (0)

// according to the documentation, these intrinsics behave the same as the
// non-'u' versions.  We'll just alias them here.
#define _mm_ucomieq_ss _mm_comieq_ss
#define _mm_ucomige_ss _mm_comige_ss
#define _mm_ucomigt_ss _mm_comigt_ss
#define _mm_ucomile_ss _mm_comile_ss
#define _mm_ucomilt_ss _mm_comilt_ss
#define _mm_ucomineq_ss _mm_comineq_ss

// Return vector of type __m128i with undefined elements.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=mm_undefined_si128
FORCE_INLINE __m128i _mm_undefined_si128(void)
{
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wuninitialized"
#endif
    __m128i a;
    return a;
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif
}

// Return vector of type __m128 with undefined elements.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_undefined_ps
FORCE_INLINE __m128 _mm_undefined_ps(void)
{
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wuninitialized"
#endif
    __m128 a;
    return a;
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif
}

// Selects and interleaves the upper two single-precision, floating-point values
// from a and b.
//
//   r0 := a2
//   r1 := b2
//   r2 := a3
//   r3 := b3
//
// https://msdn.microsoft.com/en-us/library/skccxx7d%28v=vs.90%29.aspx
FORCE_INLINE __m128 _mm_unpackhi_ps(__m128 a, __m128 b)
{
#if defined(__aarch64__)
    return vreinterpretq_m128_f32(
        vzip2q_f32(vreinterpretq_f32_m128(a), vreinterpretq_f32_m128(b)));
#else
    float32x2_t a1 = vget_high_f32(vreinterpretq_f32_m128(a));
    float32x2_t b1 = vget_high_f32(vreinterpretq_f32_m128(b));
    float32x2x2_t result = vzip_f32(a1, b1);
    return vreinterpretq_m128_f32(vcombine_f32(result.val[0], result.val[1]));
#endif
}

// Selects and interleaves the lower two single-precision, floating-point values
// from a and b.
//
//   r0 := a0
//   r1 := b0
//   r2 := a1
//   r3 := b1
//
// https://msdn.microsoft.com/en-us/library/25st103b%28v=vs.90%29.aspx
FORCE_INLINE __m128 _mm_unpacklo_ps(__m128 a, __m128 b)
{
#if defined(__aarch64__)
    return vreinterpretq_m128_f32(
        vzip1q_f32(vreinterpretq_f32_m128(a), vreinterpretq_f32_m128(b)));
#else
    float32x2_t a1 = vget_low_f32(vreinterpretq_f32_m128(a));
    float32x2_t b1 = vget_low_f32(vreinterpretq_f32_m128(b));
    float32x2x2_t result = vzip_f32(a1, b1);
    return vreinterpretq_m128_f32(vcombine_f32(result.val[0], result.val[1]));
#endif
}

// Computes bitwise EXOR (exclusive-or) of the four single-precision,
// floating-point values of a and b.
// https://msdn.microsoft.com/en-us/library/ss6k3wk8(v=vs.100).aspx
FORCE_INLINE __m128 _mm_xor_ps(__m128 a, __m128 b)
{
    return vreinterpretq_m128_s32(
        veorq_s32(vreinterpretq_s32_m128(a), vreinterpretq_s32_m128(b)));
}

/* SSE2 */

// Adds the 8 signed or unsigned 16-bit integers in a to the 8 signed or
// unsigned 16-bit integers in b.
// https://msdn.microsoft.com/en-us/library/fceha5k4(v=vs.100).aspx
FORCE_INLINE __m128i _mm_add_epi16(__m128i a, __m128i b)
{
    return vreinterpretq_m128i_s16(
        vaddq_s16(vreinterpretq_s16_m128i(a), vreinterpretq_s16_m128i(b)));
}

// Adds the 4 signed or unsigned 32-bit integers in a to the 4 signed or
// unsigned 32-bit integers in b.
//
//   r0 := a0 + b0
//   r1 := a1 + b1
//   r2 := a2 + b2
//   r3 := a3 + b3
//
// https://msdn.microsoft.com/en-us/library/vstudio/09xs4fkk(v=vs.100).aspx
FORCE_INLINE __m128i _mm_add_epi32(__m128i a, __m128i b)
{
    return vreinterpretq_m128i_s32(
        vaddq_s32(vreinterpretq_s32_m128i(a), vreinterpretq_s32_m128i(b)));
}

// Adds the 4 signed or unsigned 64-bit integers in a to the 4 signed or
// unsigned 32-bit integers in b.
// https://msdn.microsoft.com/en-us/library/vstudio/09xs4fkk(v=vs.100).aspx
FORCE_INLINE __m128i _mm_add_epi64(__m128i a, __m128i b)
{
    return vreinterpretq_m128i_s64(
        vaddq_s64(vreinterpretq_s64_m128i(a), vreinterpretq_s64_m128i(b)));
}

// Adds the 16 signed or unsigned 8-bit integers in a to the 16 signed or
// unsigned 8-bit integers in b.
// https://technet.microsoft.com/en-us/subscriptions/yc7tcyzs(v=vs.90)
FORCE_INLINE __m128i _mm_add_epi8(__m128i a, __m128i b)
{
    return vreinterpretq_m128i_s8(
        vaddq_s8(vreinterpretq_s8_m128i(a), vreinterpretq_s8_m128i(b)));
}

// Add packed double-precision (64-bit) floating-point elements in a and b, and
// store the results in dst.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_add_pd
FORCE_INLINE __m128d _mm_add_pd(__m128d a, __m128d b)
{
#if defined(__aarch64__)
    return vreinterpretq_m128d_f64(
        vaddq_f64(vreinterpretq_f64_m128d(a), vreinterpretq_f64_m128d(b)));
#else
    double *da = (double *) &a;
    double *db = (double *) &b;
    double c[2];
    c[0] = da[0] + db[0];
    c[1] = da[1] + db[1];
    return vld1q_f32((float32_t *) c);
#endif
}

// Add the lower double-precision (64-bit) floating-point element in a and b,
// store the result in the lower element of dst, and copy the upper element from
// a to the upper element of dst.
//
//   dst[63:0] := a[63:0] + b[63:0]
//   dst[127:64] := a[127:64]
//
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_add_sd
FORCE_INLINE __m128d _mm_add_sd(__m128d a, __m128d b)
{
#if defined(__aarch64__)
    return _mm_move_sd(a, _mm_add_pd(a, b));
#else
    double *da = (double *) &a;
    double *db = (double *) &b;
    double c[2];
    c[0] = da[0] + db[0];
    c[1] = da[1];
    return vld1q_f32((float32_t *) c);
#endif
}

// Add 64-bit integers a and b, and store the result in dst.
//
//   dst[63:0] := a[63:0] + b[63:0]
//
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_add_si64
FORCE_INLINE __m64 _mm_add_si64(__m64 a, __m64 b)
{
    return vreinterpret_m64_s64(
        vadd_s64(vreinterpret_s64_m64(a), vreinterpret_s64_m64(b)));
}

// Adds the 8 signed 16-bit integers in a to the 8 signed 16-bit integers in b
// and saturates.
//
//   r0 := SignedSaturate(a0 + b0)
//   r1 := SignedSaturate(a1 + b1)
//   ...
//   r7 := SignedSaturate(a7 + b7)
//
// https://msdn.microsoft.com/en-us/library/1a306ef8(v=vs.100).aspx
FORCE_INLINE __m128i _mm_adds_epi16(__m128i a, __m128i b)
{
    return vreinterpretq_m128i_s16(
        vqaddq_s16(vreinterpretq_s16_m128i(a), vreinterpretq_s16_m128i(b)));
}

// Add packed signed 8-bit integers in a and b using saturation, and store the
// results in dst.
//
//   FOR j := 0 to 15
//     i := j*8
//     dst[i+7:i] := Saturate8( a[i+7:i] + b[i+7:i] )
//   ENDFOR
//
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_adds_epi8
FORCE_INLINE __m128i _mm_adds_epi8(__m128i a, __m128i b)
{
    return vreinterpretq_m128i_s8(
        vqaddq_s8(vreinterpretq_s8_m128i(a), vreinterpretq_s8_m128i(b)));
}

// Add packed unsigned 16-bit integers in a and b using saturation, and store
// the results in dst.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_adds_epu16
FORCE_INLINE __m128i _mm_adds_epu16(__m128i a, __m128i b)
{
    return vreinterpretq_m128i_u16(
        vqaddq_u16(vreinterpretq_u16_m128i(a), vreinterpretq_u16_m128i(b)));
}

// Adds the 16 unsigned 8-bit integers in a to the 16 unsigned 8-bit integers in
// b and saturates..
// https://msdn.microsoft.com/en-us/library/9hahyddy(v=vs.100).aspx
FORCE_INLINE __m128i _mm_adds_epu8(__m128i a, __m128i b)
{
    return vreinterpretq_m128i_u8(
        vqaddq_u8(vreinterpretq_u8_m128i(a), vreinterpretq_u8_m128i(b)));
}

// Compute the bitwise AND of packed double-precision (64-bit) floating-point
// elements in a and b, and store the results in dst.
//
//   FOR j := 0 to 1
//     i := j*64
//     dst[i+63:i] := a[i+63:i] AND b[i+63:i]
//   ENDFOR
//
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_and_pd
FORCE_INLINE __m128d _mm_and_pd(__m128d a, __m128d b)
{
    return vreinterpretq_m128d_s64(
        vandq_s64(vreinterpretq_s64_m128d(a), vreinterpretq_s64_m128d(b)));
}

// Computes the bitwise AND of the 128-bit value in a and the 128-bit value in
// b.
//
//   r := a & b
//
// https://msdn.microsoft.com/en-us/library/vstudio/6d1txsa8(v=vs.100).aspx
FORCE_INLINE __m128i _mm_and_si128(__m128i a, __m128i b)
{
    return vreinterpretq_m128i_s32(
        vandq_s32(vreinterpretq_s32_m128i(a), vreinterpretq_s32_m128i(b)));
}

// Compute the bitwise NOT of packed double-precision (64-bit) floating-point
// elements in a and then AND with b, and store the results in dst.
//
//   FOR j := 0 to 1
// 	     i := j*64
// 	     dst[i+63:i] := ((NOT a[i+63:i]) AND b[i+63:i])
//   ENDFOR
//
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_andnot_pd
FORCE_INLINE __m128d _mm_andnot_pd(__m128d a, __m128d b)
{
    // *NOTE* argument swap
    return vreinterpretq_m128d_s64(
        vbicq_s64(vreinterpretq_s64_m128d(b), vreinterpretq_s64_m128d(a)));
}

// Computes the bitwise AND of the 128-bit value in b and the bitwise NOT of the
// 128-bit value in a.
//
//   r := (~a) & b
//
// https://msdn.microsoft.com/en-us/library/vstudio/1beaceh8(v=vs.100).aspx
FORCE_INLINE __m128i _mm_andnot_si128(__m128i a, __m128i b)
{
    return vreinterpretq_m128i_s32(
        vbicq_s32(vreinterpretq_s32_m128i(b),
                  vreinterpretq_s32_m128i(a)));  // *NOTE* argument swap
}

// Computes the average of the 8 unsigned 16-bit integers in a and the 8
// unsigned 16-bit integers in b and rounds.
//
//   r0 := (a0 + b0) / 2
//   r1 := (a1 + b1) / 2
//   ...
//   r7 := (a7 + b7) / 2
//
// https://msdn.microsoft.com/en-us/library/vstudio/y13ca3c8(v=vs.90).aspx
FORCE_INLINE __m128i _mm_avg_epu16(__m128i a, __m128i b)
{
    return (__m128i) vrhaddq_u16(vreinterpretq_u16_m128i(a),
                                 vreinterpretq_u16_m128i(b));
}

// Computes the average of the 16 unsigned 8-bit integers in a and the 16
// unsigned 8-bit integers in b and rounds.
//
//   r0 := (a0 + b0) / 2
//   r1 := (a1 + b1) / 2
//   ...
//   r15 := (a15 + b15) / 2
//
// https://msdn.microsoft.com/en-us/library/vstudio/8zwh554a(v%3dvs.90).aspx
FORCE_INLINE __m128i _mm_avg_epu8(__m128i a, __m128i b)
{
    return vreinterpretq_m128i_u8(
        vrhaddq_u8(vreinterpretq_u8_m128i(a), vreinterpretq_u8_m128i(b)));
}

// Shift a left by imm8 bytes while shifting in zeros, and store the results in
// dst.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_bslli_si128
#define _mm_bslli_si128(a, imm) _mm_slli_si128(a, imm)

// Shift a right by imm8 bytes while shifting in zeros, and store the results in
// dst.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_bsrli_si128
#define _mm_bsrli_si128(a, imm) _mm_srli_si128(a, imm)

// Cast vector of type __m128d to type __m128. This intrinsic is only used for
// compilation and does not generate any instructions, thus it has zero latency.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_castpd_ps
FORCE_INLINE __m128 _mm_castpd_ps(__m128d a)
{
    return vreinterpretq_m128_s64(vreinterpretq_s64_m128d(a));
}

// Cast vector of type __m128d to type __m128i. This intrinsic is only used for
// compilation and does not generate any instructions, thus it has zero latency.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_castpd_si128
FORCE_INLINE __m128i _mm_castpd_si128(__m128d a)
{
    return vreinterpretq_m128i_s64(vreinterpretq_s64_m128d(a));
}

// Cast vector of type __m128 to type __m128d. This intrinsic is only used for
// compilation and does not generate any instructions, thus it has zero latency.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_castps_pd
FORCE_INLINE __m128d _mm_castps_pd(__m128 a)
{
    return vreinterpretq_m128d_s32(vreinterpretq_s32_m128(a));
}

// Applies a type cast to reinterpret four 32-bit floating point values passed
// in as a 128-bit parameter as packed 32-bit integers.
// https://msdn.microsoft.com/en-us/library/bb514099.aspx
FORCE_INLINE __m128i _mm_castps_si128(__m128 a)
{
    return vreinterpretq_m128i_s32(vreinterpretq_s32_m128(a));
}

// Cast vector of type __m128i to type __m128d. This intrinsic is only used for
// compilation and does not generate any instructions, thus it has zero latency.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_castsi128_pd
FORCE_INLINE __m128d _mm_castsi128_pd(__m128i a)
{
#if defined(__aarch64__)
    return vreinterpretq_m128d_f64(vreinterpretq_f64_m128i(a));
#else
    return vreinterpretq_m128d_f32(vreinterpretq_f32_m128i(a));
#endif
}

// Applies a type cast to reinterpret four 32-bit integers passed in as a
// 128-bit parameter as packed 32-bit floating point values.
// https://msdn.microsoft.com/en-us/library/bb514029.aspx
FORCE_INLINE __m128 _mm_castsi128_ps(__m128i a)
{
    return vreinterpretq_m128_s32(vreinterpretq_s32_m128i(a));
}

// Invalidate and flush the cache line that contains p from all levels of the
// cache hierarchy.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_clflush
#if defined(__APPLE__)
#include <libkern/OSCacheControl.h>
#endif
FORCE_INLINE void _mm_clflush(void const *p)
{
    (void) p;

    /* sys_icache_invalidate is supported since macOS 10.5.
     * However, it does not work on non-jailbroken iOS devices, although the
     * compilation is successful.
     */
#if defined(__APPLE__)
    sys_icache_invalidate((void *) (uintptr_t) p, SSE2NEON_CACHELINE_SIZE);
#elif defined(__GNUC__) || defined(__clang__)
    uintptr_t ptr = (uintptr_t) p;
    __builtin___clear_cache((char *) ptr,
                            (char *) ptr + SSE2NEON_CACHELINE_SIZE);
#else
    /* FIXME: MSVC support */
#endif
}

// Compares the 8 signed or unsigned 16-bit integers in a and the 8 signed or
// unsigned 16-bit integers in b for equality.
// https://msdn.microsoft.com/en-us/library/2ay060te(v=vs.100).aspx
FORCE_INLINE __m128i _mm_cmpeq_epi16(__m128i a, __m128i b)
{
    return vreinterpretq_m128i_u16(
        vceqq_s16(vreinterpretq_s16_m128i(a), vreinterpretq_s16_m128i(b)));
}

// Compare packed 32-bit integers in a and b for equality, and store the results
// in dst
FORCE_INLINE __m128i _mm_cmpeq_epi32(__m128i a, __m128i b)
{
    return vreinterpretq_m128i_u32(
        vceqq_s32(vreinterpretq_s32_m128i(a), vreinterpretq_s32_m128i(b)));
}

// Compares the 16 signed or unsigned 8-bit integers in a and the 16 signed or
// unsigned 8-bit integers in b for equality.
// https://msdn.microsoft.com/en-us/library/windows/desktop/bz5xk21a(v=vs.90).aspx
FORCE_INLINE __m128i _mm_cmpeq_epi8(__m128i a, __m128i b)
{
    return vreinterpretq_m128i_u8(
        vceqq_s8(vreinterpretq_s8_m128i(a), vreinterpretq_s8_m128i(b)));
}

// Compare packed double-precision (64-bit) floating-point elements in a and b
// for equality, and store the results in dst.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_cmpeq_pd
FORCE_INLINE __m128d _mm_cmpeq_pd(__m128d a, __m128d b)
{
#if defined(__aarch64__)
    return vreinterpretq_m128d_u64(
        vceqq_f64(vreinterpretq_f64_m128d(a), vreinterpretq_f64_m128d(b)));
#else
    // (a == b) -> (a_lo == b_lo) && (a_hi == b_hi)
    uint32x4_t cmp =
        vceqq_u32(vreinterpretq_u32_m128d(a), vreinterpretq_u32_m128d(b));
    uint32x4_t swapped = vrev64q_u32(cmp);
    return vreinterpretq_m128d_u32(vandq_u32(cmp, swapped));
#endif
}

// Compare the lower double-precision (64-bit) floating-point elements in a and
// b for equality, store the result in the lower element of dst, and copy the
// upper element from a to the upper element of dst.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_cmpeq_sd
FORCE_INLINE __m128d _mm_cmpeq_sd(__m128d a, __m128d b)
{
    return _mm_move_sd(a, _mm_cmpeq_pd(a, b));
}

// Compare packed double-precision (64-bit) floating-point elements in a and b
// for greater-than-or-equal, and store the results in dst.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_cmpge_pd
FORCE_INLINE __m128d _mm_cmpge_pd(__m128d a, __m128d b)
{
#if defined(__aarch64__)
    return vreinterpretq_m128d_u64(
        vcgeq_f64(vreinterpretq_f64_m128d(a), vreinterpretq_f64_m128d(b)));
#else
    uint64_t a0 = (uint64_t) vget_low_u64(vreinterpretq_u64_m128d(a));
    uint64_t a1 = (uint64_t) vget_high_u64(vreinterpretq_u64_m128d(a));
    uint64_t b0 = (uint64_t) vget_low_u64(vreinterpretq_u64_m128d(b));
    uint64_t b1 = (uint64_t) vget_high_u64(vreinterpretq_u64_m128d(b));
    uint64_t d[2];
    d[0] = (*(double *) &a0) >= (*(double *) &b0) ? ~UINT64_C(0) : UINT64_C(0);
    d[1] = (*(double *) &a1) >= (*(double *) &b1) ? ~UINT64_C(0) : UINT64_C(0);

    return vreinterpretq_m128d_u64(vld1q_u64(d));
#endif
}

// Compare the lower double-precision (64-bit) floating-point elements in a and
// b for greater-than-or-equal, store the result in the lower element of dst,
// and copy the upper element from a to the upper element of dst.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_cmpge_sd
FORCE_INLINE __m128d _mm_cmpge_sd(__m128d a, __m128d b)
{
#if defined(__aarch64__)
    return _mm_move_sd(a, _mm_cmpge_pd(a, b));
#else
    // expand "_mm_cmpge_pd()" to reduce unnecessary operations
    uint64_t a0 = (uint64_t) vget_low_u64(vreinterpretq_u64_m128d(a));
    uint64_t a1 = (uint64_t) vget_high_u64(vreinterpretq_u64_m128d(a));
    uint64_t b0 = (uint64_t) vget_low_u64(vreinterpretq_u64_m128d(b));
    uint64_t d[2];
    d[0] = (*(double *) &a0) >= (*(double *) &b0) ? ~UINT64_C(0) : UINT64_C(0);
    d[1] = a1;

    return vreinterpretq_m128d_u64(vld1q_u64(d));
#endif
}

// Compares the 8 signed 16-bit integers in a and the 8 signed 16-bit integers
// in b for greater than.
//
//   r0 := (a0 > b0) ? 0xffff : 0x0
//   r1 := (a1 > b1) ? 0xffff : 0x0
//   ...
//   r7 := (a7 > b7) ? 0xffff : 0x0
//
// https://technet.microsoft.com/en-us/library/xd43yfsa(v=vs.100).aspx
FORCE_INLINE __m128i _mm_cmpgt_epi16(__m128i a, __m128i b)
{
    return vreinterpretq_m128i_u16(
        vcgtq_s16(vreinterpretq_s16_m128i(a), vreinterpretq_s16_m128i(b)));
}

// Compares the 4 signed 32-bit integers in a and the 4 signed 32-bit integers
// in b for greater than.
// https://msdn.microsoft.com/en-us/library/vstudio/1s9f2z0y(v=vs.100).aspx
FORCE_INLINE __m128i _mm_cmpgt_epi32(__m128i a, __m128i b)
{
    return vreinterpretq_m128i_u32(
        vcgtq_s32(vreinterpretq_s32_m128i(a), vreinterpretq_s32_m128i(b)));
}

// Compares the 16 signed 8-bit integers in a and the 16 signed 8-bit integers
// in b for greater than.
//
//   r0 := (a0 > b0) ? 0xff : 0x0
//   r1 := (a1 > b1) ? 0xff : 0x0
//   ...
//   r15 := (a15 > b15) ? 0xff : 0x0
//
// https://msdn.microsoft.com/zh-tw/library/wf45zt2b(v=vs.100).aspx
FORCE_INLINE __m128i _mm_cmpgt_epi8(__m128i a, __m128i b)
{
    return vreinterpretq_m128i_u8(
        vcgtq_s8(vreinterpretq_s8_m128i(a), vreinterpretq_s8_m128i(b)));
}

// Compare packed double-precision (64-bit) floating-point elements in a and b
// for greater-than, and store the results in dst.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_cmpgt_pd
FORCE_INLINE __m128d _mm_cmpgt_pd(__m128d a, __m128d b)
{
#if defined(__aarch64__)
    return vreinterpretq_m128d_u64(
        vcgtq_f64(vreinterpretq_f64_m128d(a), vreinterpretq_f64_m128d(b)));
#else
    uint64_t a0 = (uint64_t) vget_low_u64(vreinterpretq_u64_m128d(a));
    uint64_t a1 = (uint64_t) vget_high_u64(vreinterpretq_u64_m128d(a));
    uint64_t b0 = (uint64_t) vget_low_u64(vreinterpretq_u64_m128d(b));
    uint64_t b1 = (uint64_t) vget_high_u64(vreinterpretq_u64_m128d(b));
    uint64_t d[2];
    d[0] = (*(double *) &a0) > (*(double *) &b0) ? ~UINT64_C(0) : UINT64_C(0);
    d[1] = (*(double *) &a1) > (*(double *) &b1) ? ~UINT64_C(0) : UINT64_C(0);

    return vreinterpretq_m128d_u64(vld1q_u64(d));
#endif
}

// Compare the lower double-precision (64-bit) floating-point elements in a and
// b for greater-than, store the result in the lower element of dst, and copy
// the upper element from a to the upper element of dst.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_cmpgt_sd
FORCE_INLINE __m128d _mm_cmpgt_sd(__m128d a, __m128d b)
{
#if defined(__aarch64__)
    return _mm_move_sd(a, _mm_cmpgt_pd(a, b));
#else
    // expand "_mm_cmpge_pd()" to reduce unnecessary operations
    uint64_t a0 = (uint64_t) vget_low_u64(vreinterpretq_u64_m128d(a));
    uint64_t a1 = (uint64_t) vget_high_u64(vreinterpretq_u64_m128d(a));
    uint64_t b0 = (uint64_t) vget_low_u64(vreinterpretq_u64_m128d(b));
    uint64_t d[2];
    d[0] = (*(double *) &a0) > (*(double *) &b0) ? ~UINT64_C(0) : UINT64_C(0);
    d[1] = a1;

    return vreinterpretq_m128d_u64(vld1q_u64(d));
#endif
}

// Compare packed double-precision (64-bit) floating-point elements in a and b
// for less-than-or-equal, and store the results in dst.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_cmple_pd
FORCE_INLINE __m128d _mm_cmple_pd(__m128d a, __m128d b)
{
#if defined(__aarch64__)
    return vreinterpretq_m128d_u64(
        vcleq_f64(vreinterpretq_f64_m128d(a), vreinterpretq_f64_m128d(b)));
#else
    uint64_t a0 = (uint64_t) vget_low_u64(vreinterpretq_u64_m128d(a));
    uint64_t a1 = (uint64_t) vget_high_u64(vreinterpretq_u64_m128d(a));
    uint64_t b0 = (uint64_t) vget_low_u64(vreinterpretq_u64_m128d(b));
    uint64_t b1 = (uint64_t) vget_high_u64(vreinterpretq_u64_m128d(b));
    uint64_t d[2];
    d[0] = (*(double *) &a0) <= (*(double *) &b0) ? ~UINT64_C(0) : UINT64_C(0);
    d[1] = (*(double *) &a1) <= (*(double *) &b1) ? ~UINT64_C(0) : UINT64_C(0);

    return vreinterpretq_m128d_u64(vld1q_u64(d));
#endif
}

// Compare the lower double-precision (64-bit) floating-point elements in a and
// b for less-than-or-equal, store the result in the lower element of dst, and
// copy the upper element from a to the upper element of dst.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_cmple_sd
FORCE_INLINE __m128d _mm_cmple_sd(__m128d a, __m128d b)
{
#if defined(__aarch64__)
    return _mm_move_sd(a, _mm_cmple_pd(a, b));
#else
    // expand "_mm_cmpge_pd()" to reduce unnecessary operations
    uint64_t a0 = (uint64_t) vget_low_u64(vreinterpretq_u64_m128d(a));
    uint64_t a1 = (uint64_t) vget_high_u64(vreinterpretq_u64_m128d(a));
    uint64_t b0 = (uint64_t) vget_low_u64(vreinterpretq_u64_m128d(b));
    uint64_t d[2];
    d[0] = (*(double *) &a0) <= (*(double *) &b0) ? ~UINT64_C(0) : UINT64_C(0);
    d[1] = a1;

    return vreinterpretq_m128d_u64(vld1q_u64(d));
#endif
}

// Compares the 8 signed 16-bit integers in a and the 8 signed 16-bit integers
// in b for less than.
//
//   r0 := (a0 < b0) ? 0xffff : 0x0
//   r1 := (a1 < b1) ? 0xffff : 0x0
//   ...
//   r7 := (a7 < b7) ? 0xffff : 0x0
//
// https://technet.microsoft.com/en-us/library/t863edb2(v=vs.100).aspx
FORCE_INLINE __m128i _mm_cmplt_epi16(__m128i a, __m128i b)
{
    return vreinterpretq_m128i_u16(
        vcltq_s16(vreinterpretq_s16_m128i(a), vreinterpretq_s16_m128i(b)));
}


// Compares the 4 signed 32-bit integers in a and the 4 signed 32-bit integers
// in b for less than.
// https://msdn.microsoft.com/en-us/library/vstudio/4ak0bf5d(v=vs.100).aspx
FORCE_INLINE __m128i _mm_cmplt_epi32(__m128i a, __m128i b)
{
    return vreinterpretq_m128i_u32(
        vcltq_s32(vreinterpretq_s32_m128i(a), vreinterpretq_s32_m128i(b)));
}

// Compares the 16 signed 8-bit integers in a and the 16 signed 8-bit integers
// in b for lesser than.
// https://msdn.microsoft.com/en-us/library/windows/desktop/9s46csht(v=vs.90).aspx
FORCE_INLINE __m128i _mm_cmplt_epi8(__m128i a, __m128i b)
{
    return vreinterpretq_m128i_u8(
        vcltq_s8(vreinterpretq_s8_m128i(a), vreinterpretq_s8_m128i(b)));
}

// Compare packed double-precision (64-bit) floating-point elements in a and b
// for less-than, and store the results in dst.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_cmplt_pd
FORCE_INLINE __m128d _mm_cmplt_pd(__m128d a, __m128d b)
{
#if defined(__aarch64__)
    return vreinterpretq_m128d_u64(
        vcltq_f64(vreinterpretq_f64_m128d(a), vreinterpretq_f64_m128d(b)));
#else
    uint64_t a0 = (uint64_t) vget_low_u64(vreinterpretq_u64_m128d(a));
    uint64_t a1 = (uint64_t) vget_high_u64(vreinterpretq_u64_m128d(a));
    uint64_t b0 = (uint64_t) vget_low_u64(vreinterpretq_u64_m128d(b));
    uint64_t b1 = (uint64_t) vget_high_u64(vreinterpretq_u64_m128d(b));
    uint64_t d[2];
    d[0] = (*(double *) &a0) < (*(double *) &b0) ? ~UINT64_C(0) : UINT64_C(0);
    d[1] = (*(double *) &a1) < (*(double *) &b1) ? ~UINT64_C(0) : UINT64_C(0);

    return vreinterpretq_m128d_u64(vld1q_u64(d));
#endif
}

// Compare the lower double-precision (64-bit) floating-point elements in a and
// b for less-than, store the result in the lower element of dst, and copy the
// upper element from a to the upper element of dst.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_cmplt_sd
FORCE_INLINE __m128d _mm_cmplt_sd(__m128d a, __m128d b)
{
#if defined(__aarch64__)
    return _mm_move_sd(a, _mm_cmplt_pd(a, b));
#else
    uint64_t a0 = (uint64_t) vget_low_u64(vreinterpretq_u64_m128d(a));
    uint64_t a1 = (uint64_t) vget_high_u64(vreinterpretq_u64_m128d(a));
    uint64_t b0 = (uint64_t) vget_low_u64(vreinterpretq_u64_m128d(b));
    uint64_t d[2];
    d[0] = (*(double *) &a0) < (*(double *) &b0) ? ~UINT64_C(0) : UINT64_C(0);
    d[1] = a1;

    return vreinterpretq_m128d_u64(vld1q_u64(d));
#endif
}

// Compare packed double-precision (64-bit) floating-point elements in a and b
// for not-equal, and store the results in dst.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_cmpneq_pd
FORCE_INLINE __m128d _mm_cmpneq_pd(__m128d a, __m128d b)
{
#if defined(__aarch64__)
    return vreinterpretq_m128d_s32(vmvnq_s32(vreinterpretq_s32_u64(
        vceqq_f64(vreinterpretq_f64_m128d(a), vreinterpretq_f64_m128d(b)))));
#else
    // (a == b) -> (a_lo == b_lo) && (a_hi == b_hi)
    uint32x4_t cmp =
        vceqq_u32(vreinterpretq_u32_m128d(a), vreinterpretq_u32_m128d(b));
    uint32x4_t swapped = vrev64q_u32(cmp);
    return vreinterpretq_m128d_u32(vmvnq_u32(vandq_u32(cmp, swapped)));
#endif
}

// Compare the lower double-precision (64-bit) floating-point elements in a and
// b for not-equal, store the result in the lower element of dst, and copy the
// upper element from a to the upper element of dst.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_cmpneq_sd
FORCE_INLINE __m128d _mm_cmpneq_sd(__m128d a, __m128d b)
{
    return _mm_move_sd(a, _mm_cmpneq_pd(a, b));
}

// Compare packed double-precision (64-bit) floating-point elements in a and b
// for not-greater-than-or-equal, and store the results in dst.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_cmpnge_pd
FORCE_INLINE __m128d _mm_cmpnge_pd(__m128d a, __m128d b)
{
#if defined(__aarch64__)
    return vreinterpretq_m128d_u64(veorq_u64(
        vcgeq_f64(vreinterpretq_f64_m128d(a), vreinterpretq_f64_m128d(b)),
        vdupq_n_u64(UINT64_MAX)));
#else
    uint64_t a0 = (uint64_t) vget_low_u64(vreinterpretq_u64_m128d(a));
    uint64_t a1 = (uint64_t) vget_high_u64(vreinterpretq_u64_m128d(a));
    uint64_t b0 = (uint64_t) vget_low_u64(vreinterpretq_u64_m128d(b));
    uint64_t b1 = (uint64_t) vget_high_u64(vreinterpretq_u64_m128d(b));
    uint64_t d[2];
    d[0] =
        !((*(double *) &a0) >= (*(double *) &b0)) ? ~UINT64_C(0) : UINT64_C(0);
    d[1] =
        !((*(double *) &a1) >= (*(double *) &b1)) ? ~UINT64_C(0) : UINT64_C(0);

    return vreinterpretq_m128d_u64(vld1q_u64(d));
#endif
}

// Compare the lower double-precision (64-bit) floating-point elements in a and
// b for not-greater-than-or-equal, store the result in the lower element of
// dst, and copy the upper element from a to the upper element of dst.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_cmpnge_sd
FORCE_INLINE __m128d _mm_cmpnge_sd(__m128d a, __m128d b)
{
    return _mm_move_sd(a, _mm_cmpnge_pd(a, b));
}

// Compare packed double-precision (64-bit) floating-point elements in a and b
// for not-greater-than, and store the results in dst.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_cmpngt_pd
FORCE_INLINE __m128d _mm_cmpngt_pd(__m128d a, __m128d b)
{
#if defined(__aarch64__)
    return vreinterpretq_m128d_u64(veorq_u64(
        vcgtq_f64(vreinterpretq_f64_m128d(a), vreinterpretq_f64_m128d(b)),
        vdupq_n_u64(UINT64_MAX)));
#else
    uint64_t a0 = (uint64_t) vget_low_u64(vreinterpretq_u64_m128d(a));
    uint64_t a1 = (uint64_t) vget_high_u64(vreinterpretq_u64_m128d(a));
    uint64_t b0 = (uint64_t) vget_low_u64(vreinterpretq_u64_m128d(b));
    uint64_t b1 = (uint64_t) vget_high_u64(vreinterpretq_u64_m128d(b));
    uint64_t d[2];
    d[0] =
        !((*(double *) &a0) > (*(double *) &b0)) ? ~UINT64_C(0) : UINT64_C(0);
    d[1] =
        !((*(double *) &a1) > (*(double *) &b1)) ? ~UINT64_C(0) : UINT64_C(0);

    return vreinterpretq_m128d_u64(vld1q_u64(d));
#endif
}

// Compare the lower double-precision (64-bit) floating-point elements in a and
// b for not-greater-than, store the result in the lower element of dst, and
// copy the upper element from a to the upper element of dst.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_cmpngt_sd
FORCE_INLINE __m128d _mm_cmpngt_sd(__m128d a, __m128d b)
{
    return _mm_move_sd(a, _mm_cmpngt_pd(a, b));
}

// Compare packed double-precision (64-bit) floating-point elements in a and b
// for not-less-than-or-equal, and store the results in dst.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_cmpnle_pd
FORCE_INLINE __m128d _mm_cmpnle_pd(__m128d a, __m128d b)
{
#if defined(__aarch64__)
    return vreinterpretq_m128d_u64(veorq_u64(
        vcleq_f64(vreinterpretq_f64_m128d(a), vreinterpretq_f64_m128d(b)),
        vdupq_n_u64(UINT64_MAX)));
#else
    uint64_t a0 = (uint64_t) vget_low_u64(vreinterpretq_u64_m128d(a));
    uint64_t a1 = (uint64_t) vget_high_u64(vreinterpretq_u64_m128d(a));
    uint64_t b0 = (uint64_t) vget_low_u64(vreinterpretq_u64_m128d(b));
    uint64_t b1 = (uint64_t) vget_high_u64(vreinterpretq_u64_m128d(b));
    uint64_t d[2];
    d[0] =
        !((*(double *) &a0) <= (*(double *) &b0)) ? ~UINT64_C(0) : UINT64_C(0);
    d[1] =
        !((*(double *) &a1) <= (*(double *) &b1)) ? ~UINT64_C(0) : UINT64_C(0);

    return vreinterpretq_m128d_u64(vld1q_u64(d));
#endif
}

// Compare the lower double-precision (64-bit) floating-point elements in a and
// b for not-less-than-or-equal, store the result in the lower element of dst,
// and copy the upper element from a to the upper element of dst.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_cmpnle_sd
FORCE_INLINE __m128d _mm_cmpnle_sd(__m128d a, __m128d b)
{
    return _mm_move_sd(a, _mm_cmpnle_pd(a, b));
}

// Compare packed double-precision (64-bit) floating-point elements in a and b
// for not-less-than, and store the results in dst.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_cmpnlt_pd
FORCE_INLINE __m128d _mm_cmpnlt_pd(__m128d a, __m128d b)
{
#if defined(__aarch64__)
    return vreinterpretq_m128d_u64(veorq_u64(
        vcltq_f64(vreinterpretq_f64_m128d(a), vreinterpretq_f64_m128d(b)),
        vdupq_n_u64(UINT64_MAX)));
#else
    uint64_t a0 = (uint64_t) vget_low_u64(vreinterpretq_u64_m128d(a));
    uint64_t a1 = (uint64_t) vget_high_u64(vreinterpretq_u64_m128d(a));
    uint64_t b0 = (uint64_t) vget_low_u64(vreinterpretq_u64_m128d(b));
    uint64_t b1 = (uint64_t) vget_high_u64(vreinterpretq_u64_m128d(b));
    uint64_t d[2];
    d[0] =
        !((*(double *) &a0) < (*(double *) &b0)) ? ~UINT64_C(0) : UINT64_C(0);
    d[1] =
        !((*(double *) &a1) < (*(double *) &b1)) ? ~UINT64_C(0) : UINT64_C(0);

    return vreinterpretq_m128d_u64(vld1q_u64(d));
#endif
}

// Compare the lower double-precision (64-bit) floating-point elements in a and
// b for not-less-than, store the result in the lower element of dst, and copy
// the upper element from a to the upper element of dst.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_cmpnlt_sd
FORCE_INLINE __m128d _mm_cmpnlt_sd(__m128d a, __m128d b)
{
    return _mm_move_sd(a, _mm_cmpnlt_pd(a, b));
}

// Compare packed double-precision (64-bit) floating-point elements in a and b
// to see if neither is NaN, and store the results in dst.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_cmpord_pd
FORCE_INLINE __m128d _mm_cmpord_pd(__m128d a, __m128d b)
{
#if defined(__aarch64__)
    // Excluding NaNs, any two floating point numbers can be compared.
    uint64x2_t not_nan_a =
        vceqq_f64(vreinterpretq_f64_m128d(a), vreinterpretq_f64_m128d(a));
    uint64x2_t not_nan_b =
        vceqq_f64(vreinterpretq_f64_m128d(b), vreinterpretq_f64_m128d(b));
    return vreinterpretq_m128d_u64(vandq_u64(not_nan_a, not_nan_b));
#else
    uint64_t a0 = (uint64_t) vget_low_u64(vreinterpretq_u64_m128d(a));
    uint64_t a1 = (uint64_t) vget_high_u64(vreinterpretq_u64_m128d(a));
    uint64_t b0 = (uint64_t) vget_low_u64(vreinterpretq_u64_m128d(b));
    uint64_t b1 = (uint64_t) vget_high_u64(vreinterpretq_u64_m128d(b));
    uint64_t d[2];
    d[0] = ((*(double *) &a0) == (*(double *) &a0) &&
            (*(double *) &b0) == (*(double *) &b0))
               ? ~UINT64_C(0)
               : UINT64_C(0);
    d[1] = ((*(double *) &a1) == (*(double *) &a1) &&
            (*(double *) &b1) == (*(double *) &b1))
               ? ~UINT64_C(0)
               : UINT64_C(0);

    return vreinterpretq_m128d_u64(vld1q_u64(d));
#endif
}

// Compare the lower double-precision (64-bit) floating-point elements in a and
// b to see if neither is NaN, store the result in the lower element of dst, and
// copy the upper element from a to the upper element of dst.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_cmpord_sd
FORCE_INLINE __m128d _mm_cmpord_sd(__m128d a, __m128d b)
{
#if defined(__aarch64__)
    return _mm_move_sd(a, _mm_cmpord_pd(a, b));
#else
    uint64_t a0 = (uint64_t) vget_low_u64(vreinterpretq_u64_m128d(a));
    uint64_t b0 = (uint64_t) vget_low_u64(vreinterpretq_u64_m128d(b));
    uint64_t a1 = (uint64_t) vget_high_u64(vreinterpretq_u64_m128d(a));
    uint64_t d[2];
    d[0] = ((*(double *) &a0) == (*(double *) &a0) &&
            (*(double *) &b0) == (*(double *) &b0))
               ? ~UINT64_C(0)
               : UINT64_C(0);
    d[1] = a1;

    return vreinterpretq_m128d_u64(vld1q_u64(d));
#endif
}

// Compare packed double-precision (64-bit) floating-point elements in a and b
// to see if either is NaN, and store the results in dst.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_cmpunord_pd
FORCE_INLINE __m128d _mm_cmpunord_pd(__m128d a, __m128d b)
{
#if defined(__aarch64__)
    // Two NaNs are not equal in comparison operation.
    uint64x2_t not_nan_a =
        vceqq_f64(vreinterpretq_f64_m128d(a), vreinterpretq_f64_m128d(a));
    uint64x2_t not_nan_b =
        vceqq_f64(vreinterpretq_f64_m128d(b), vreinterpretq_f64_m128d(b));
    return vreinterpretq_m128d_s32(
        vmvnq_s32(vreinterpretq_s32_u64(vandq_u64(not_nan_a, not_nan_b))));
#else
    uint64_t a0 = (uint64_t) vget_low_u64(vreinterpretq_u64_m128d(a));
    uint64_t a1 = (uint64_t) vget_high_u64(vreinterpretq_u64_m128d(a));
    uint64_t b0 = (uint64_t) vget_low_u64(vreinterpretq_u64_m128d(b));
    uint64_t b1 = (uint64_t) vget_high_u64(vreinterpretq_u64_m128d(b));
    uint64_t d[2];
    d[0] = ((*(double *) &a0) == (*(double *) &a0) &&
            (*(double *) &b0) == (*(double *) &b0))
               ? UINT64_C(0)
               : ~UINT64_C(0);
    d[1] = ((*(double *) &a1) == (*(double *) &a1) &&
            (*(double *) &b1) == (*(double *) &b1))
               ? UINT64_C(0)
               : ~UINT64_C(0);

    return vreinterpretq_m128d_u64(vld1q_u64(d));
#endif
}

// Compare the lower double-precision (64-bit) floating-point elements in a and
// b to see if either is NaN, store the result in the lower element of dst, and
// copy the upper element from a to the upper element of dst.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_cmpunord_sd
FORCE_INLINE __m128d _mm_cmpunord_sd(__m128d a, __m128d b)
{
#if defined(__aarch64__)
    return _mm_move_sd(a, _mm_cmpunord_pd(a, b));
#else
    uint64_t a0 = (uint64_t) vget_low_u64(vreinterpretq_u64_m128d(a));
    uint64_t b0 = (uint64_t) vget_low_u64(vreinterpretq_u64_m128d(b));
    uint64_t a1 = (uint64_t) vget_high_u64(vreinterpretq_u64_m128d(a));
    uint64_t d[2];
    d[0] = ((*(double *) &a0) == (*(double *) &a0) &&
            (*(double *) &b0) == (*(double *) &b0))
               ? UINT64_C(0)
               : ~UINT64_C(0);
    d[1] = a1;

    return vreinterpretq_m128d_u64(vld1q_u64(d));
#endif
}

// Compare the lower double-precision (64-bit) floating-point element in a and b
// for greater-than-or-equal, and return the boolean result (0 or 1).
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_comige_sd
FORCE_INLINE int _mm_comige_sd(__m128d a, __m128d b)
{
#if defined(__aarch64__)
    return vgetq_lane_u64(vcgeq_f64(a, b), 0) & 0x1;
#else
    uint64_t a0 = (uint64_t) vget_low_u64(vreinterpretq_u64_m128d(a));
    uint64_t b0 = (uint64_t) vget_low_u64(vreinterpretq_u64_m128d(b));

    return (*(double *) &a0 >= *(double *) &b0);
#endif
}

// Compare the lower double-precision (64-bit) floating-point element in a and b
// for greater-than, and return the boolean result (0 or 1).
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_comigt_sd
FORCE_INLINE int _mm_comigt_sd(__m128d a, __m128d b)
{
#if defined(__aarch64__)
    return vgetq_lane_u64(vcgtq_f64(a, b), 0) & 0x1;
#else
    uint64_t a0 = (uint64_t) vget_low_u64(vreinterpretq_u64_m128d(a));
    uint64_t b0 = (uint64_t) vget_low_u64(vreinterpretq_u64_m128d(b));

    return (*(double *) &a0 > *(double *) &b0);
#endif
}

// Compare the lower double-precision (64-bit) floating-point element in a and b
// for less-than-or-equal, and return the boolean result (0 or 1).
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_comile_sd
FORCE_INLINE int _mm_comile_sd(__m128d a, __m128d b)
{
#if defined(__aarch64__)
    return vgetq_lane_u64(vcleq_f64(a, b), 0) & 0x1;
#else
    uint64_t a0 = (uint64_t) vget_low_u64(vreinterpretq_u64_m128d(a));
    uint64_t b0 = (uint64_t) vget_low_u64(vreinterpretq_u64_m128d(b));

    return (*(double *) &a0 <= *(double *) &b0);
#endif
}

// Compare the lower double-precision (64-bit) floating-point element in a and b
// for less-than, and return the boolean result (0 or 1).
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_comilt_sd
FORCE_INLINE int _mm_comilt_sd(__m128d a, __m128d b)
{
#if defined(__aarch64__)
    return vgetq_lane_u64(vcltq_f64(a, b), 0) & 0x1;
#else
    uint64_t a0 = (uint64_t) vget_low_u64(vreinterpretq_u64_m128d(a));
    uint64_t b0 = (uint64_t) vget_low_u64(vreinterpretq_u64_m128d(b));

    return (*(double *) &a0 < *(double *) &b0);
#endif
}

// Compare the lower double-precision (64-bit) floating-point element in a and b
// for equality, and return the boolean result (0 or 1).
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_comieq_sd
FORCE_INLINE int _mm_comieq_sd(__m128d a, __m128d b)
{
#if defined(__aarch64__)
    return vgetq_lane_u64(vceqq_f64(a, b), 0) & 0x1;
#else
    uint32x4_t a_not_nan =
        vceqq_u32(vreinterpretq_u32_m128d(a), vreinterpretq_u32_m128d(a));
    uint32x4_t b_not_nan =
        vceqq_u32(vreinterpretq_u32_m128d(b), vreinterpretq_u32_m128d(b));
    uint32x4_t a_and_b_not_nan = vandq_u32(a_not_nan, b_not_nan);
    uint32x4_t a_eq_b =
        vceqq_u32(vreinterpretq_u32_m128d(a), vreinterpretq_u32_m128d(b));
    uint64x2_t and_results = vandq_u64(vreinterpretq_u64_u32(a_and_b_not_nan),
                                       vreinterpretq_u64_u32(a_eq_b));
    return vgetq_lane_u64(and_results, 0) & 0x1;
#endif
}

// Compare the lower double-precision (64-bit) floating-point element in a and b
// for not-equal, and return the boolean result (0 or 1).
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_comineq_sd
FORCE_INLINE int _mm_comineq_sd(__m128d a, __m128d b)
{
    return !_mm_comieq_sd(a, b);
}

// Convert packed signed 32-bit integers in a to packed double-precision
// (64-bit) floating-point elements, and store the results in dst.
//
//   FOR j := 0 to 1
//     i := j*32
//     m := j*64
//     dst[m+63:m] := Convert_Int32_To_FP64(a[i+31:i])
//   ENDFOR
//
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_cvtepi32_pd
FORCE_INLINE __m128d _mm_cvtepi32_pd(__m128i a)
{
#if defined(__aarch64__)
    return vreinterpretq_m128d_f64(
        vcvtq_f64_s64(vmovl_s32(vget_low_s32(vreinterpretq_s32_m128i(a)))));
#else
    double a0 = (double) vgetq_lane_s32(vreinterpretq_s32_m128i(a), 0);
    double a1 = (double) vgetq_lane_s32(vreinterpretq_s32_m128i(a), 1);
    return _mm_set_pd(a1, a0);
#endif
}

// Converts the four signed 32-bit integer values of a to single-precision,
// floating-point values
// https://msdn.microsoft.com/en-us/library/vstudio/36bwxcx5(v=vs.100).aspx
FORCE_INLINE __m128 _mm_cvtepi32_ps(__m128i a)
{
    return vreinterpretq_m128_f32(vcvtq_f32_s32(vreinterpretq_s32_m128i(a)));
}

// Convert packed double-precision (64-bit) floating-point elements in a to
// packed 32-bit integers, and store the results in dst.
//
//   FOR j := 0 to 1
//      i := 32*j
//      k := 64*j
//      dst[i+31:i] := Convert_FP64_To_Int32(a[k+63:k])
//   ENDFOR
//
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_cvtpd_epi32
FORCE_INLINE __m128i _mm_cvtpd_epi32(__m128d a)
{
// vrnd32xq_f64 not supported on clang
#if defined(__ARM_FEATURE_FRINT) && !defined(__clang__)
    float64x2_t rounded = vrnd32xq_f64(vreinterpretq_f64_m128d(a));
    int64x2_t integers = vcvtq_s64_f64(rounded);
    return vreinterpretq_m128i_s32(
        vcombine_s32(vmovn_s64(integers), vdup_n_s32(0)));
#else
    __m128d rnd = _mm_round_pd(a, _MM_FROUND_CUR_DIRECTION);
    double d0 = ((double *) &rnd)[0];
    double d1 = ((double *) &rnd)[1];
    return _mm_set_epi32(0, 0, (int32_t) d1, (int32_t) d0);
#endif
}

// Convert packed double-precision (64-bit) floating-point elements in a to
// packed 32-bit integers, and store the results in dst.
//
//   FOR j := 0 to 1
//      i := 32*j
//      k := 64*j
//      dst[i+31:i] := Convert_FP64_To_Int32(a[k+63:k])
//   ENDFOR
//
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_cvtpd_pi32
FORCE_INLINE __m64 _mm_cvtpd_pi32(__m128d a)
{
    __m128d rnd = _mm_round_pd(a, _MM_FROUND_CUR_DIRECTION);
    double d0 = ((double *) &rnd)[0];
    double d1 = ((double *) &rnd)[1];
    int32_t ALIGN_STRUCT(16) data[2] = {(int32_t) d0, (int32_t) d1};
    return vreinterpret_m64_s32(vld1_s32(data));
}

// Convert packed double-precision (64-bit) floating-point elements in a to
// packed single-precision (32-bit) floating-point elements, and store the
// results in dst.
//
//   FOR j := 0 to 1
//     i := 32*j
//     k := 64*j
//     dst[i+31:i] := Convert_FP64_To_FP32(a[k+64:k])
//   ENDFOR
//   dst[127:64] := 0
//
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_cvtpd_ps
FORCE_INLINE __m128 _mm_cvtpd_ps(__m128d a)
{
#if defined(__aarch64__)
    float32x2_t tmp = vcvt_f32_f64(vreinterpretq_f64_m128d(a));
    return vreinterpretq_m128_f32(vcombine_f32(tmp, vdup_n_f32(0)));
#else
    float a0 = (float) ((double *) &a)[0];
    float a1 = (float) ((double *) &a)[1];
    return _mm_set_ps(0, 0, a1, a0);
#endif
}

// Convert packed signed 32-bit integers in a to packed double-precision
// (64-bit) floating-point elements, and store the results in dst.
//
//   FOR j := 0 to 1
//     i := j*32
//     m := j*64
//     dst[m+63:m] := Convert_Int32_To_FP64(a[i+31:i])
//   ENDFOR
//
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_cvtpi32_pd
FORCE_INLINE __m128d _mm_cvtpi32_pd(__m64 a)
{
#if defined(__aarch64__)
    return vreinterpretq_m128d_f64(
        vcvtq_f64_s64(vmovl_s32(vreinterpret_s32_m64(a))));
#else
    double a0 = (double) vget_lane_s32(vreinterpret_s32_m64(a), 0);
    double a1 = (double) vget_lane_s32(vreinterpret_s32_m64(a), 1);
    return _mm_set_pd(a1, a0);
#endif
}

// Converts the four single-precision, floating-point values of a to signed
// 32-bit integer values.
//
//   r0 := (int) a0
//   r1 := (int) a1
//   r2 := (int) a2
//   r3 := (int) a3
//
// https://msdn.microsoft.com/en-us/library/vstudio/xdc42k5e(v=vs.100).aspx
// *NOTE*. The default rounding mode on SSE is 'round to even', which ARMv7-A
// does not support! It is supported on ARMv8-A however.
FORCE_INLINE __m128i _mm_cvtps_epi32(__m128 a)
{
#if defined(__ARM_FEATURE_FRINT)
    return vreinterpretq_m128i_s32(vcvtq_s32_f32(vrnd32xq_f32(a)));
#elif defined(__aarch64__) || defined(__ARM_FEATURE_DIRECTED_ROUNDING)
    switch (_MM_GET_ROUNDING_MODE()) {
    case _MM_ROUND_NEAREST:
        return vreinterpretq_m128i_s32(vcvtnq_s32_f32(a));
    case _MM_ROUND_DOWN:
        return vreinterpretq_m128i_s32(vcvtmq_s32_f32(a));
    case _MM_ROUND_UP:
        return vreinterpretq_m128i_s32(vcvtpq_s32_f32(a));
    default:  // _MM_ROUND_TOWARD_ZERO
        return vreinterpretq_m128i_s32(vcvtq_s32_f32(a));
    }
#else
    float *f = (float *) &a;
    switch (_MM_GET_ROUNDING_MODE()) {
    case _MM_ROUND_NEAREST: {
        uint32x4_t signmask = vdupq_n_u32(0x80000000);
        float32x4_t half = vbslq_f32(signmask, vreinterpretq_f32_m128(a),
                                     vdupq_n_f32(0.5f)); /* +/- 0.5 */
        int32x4_t r_normal = vcvtq_s32_f32(vaddq_f32(
            vreinterpretq_f32_m128(a), half)); /* round to integer: [a + 0.5]*/
        int32x4_t r_trunc = vcvtq_s32_f32(
            vreinterpretq_f32_m128(a)); /* truncate to integer: [a] */
        int32x4_t plusone = vreinterpretq_s32_u32(vshrq_n_u32(
            vreinterpretq_u32_s32(vnegq_s32(r_trunc)), 31)); /* 1 or 0 */
        int32x4_t r_even = vbicq_s32(vaddq_s32(r_trunc, plusone),
                                     vdupq_n_s32(1)); /* ([a] + {0,1}) & ~1 */
        float32x4_t delta = vsubq_f32(
            vreinterpretq_f32_m128(a),
            vcvtq_f32_s32(r_trunc)); /* compute delta: delta = (a - [a]) */
        uint32x4_t is_delta_half =
            vceqq_f32(delta, half); /* delta == +/- 0.5 */
        return vreinterpretq_m128i_s32(
            vbslq_s32(is_delta_half, r_even, r_normal));
    }
    case _MM_ROUND_DOWN:
        return _mm_set_epi32(floorf(f[3]), floorf(f[2]), floorf(f[1]),
                             floorf(f[0]));
    case _MM_ROUND_UP:
        return _mm_set_epi32(ceilf(f[3]), ceilf(f[2]), ceilf(f[1]),
                             ceilf(f[0]));
    default:  // _MM_ROUND_TOWARD_ZERO
        return _mm_set_epi32((int32_t) f[3], (int32_t) f[2], (int32_t) f[1],
                             (int32_t) f[0]);
    }
#endif
}

// Convert packed single-precision (32-bit) floating-point elements in a to
// packed double-precision (64-bit) floating-point elements, and store the
// results in dst.
//
//   FOR j := 0 to 1
//     i := 64*j
//     k := 32*j
//     dst[i+63:i] := Convert_FP32_To_FP64(a[k+31:k])
//   ENDFOR
//
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_cvtps_pd
FORCE_INLINE __m128d _mm_cvtps_pd(__m128 a)
{
#if defined(__aarch64__)
    return vreinterpretq_m128d_f64(
        vcvt_f64_f32(vget_low_f32(vreinterpretq_f32_m128(a))));
#else
    double a0 = (double) vgetq_lane_f32(vreinterpretq_f32_m128(a), 0);
    double a1 = (double) vgetq_lane_f32(vreinterpretq_f32_m128(a), 1);
    return _mm_set_pd(a1, a0);
#endif
}

// Copy the lower double-precision (64-bit) floating-point element of a to dst.
//
//   dst[63:0] := a[63:0]
//
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_cvtsd_f64
FORCE_INLINE double _mm_cvtsd_f64(__m128d a)
{
#if defined(__aarch64__)
    return (double) vgetq_lane_f64(vreinterpretq_f64_m128d(a), 0);
#else
    return ((double *) &a)[0];
#endif
}

// Convert the lower double-precision (64-bit) floating-point element in a to a
// 32-bit integer, and store the result in dst.
//
//   dst[31:0] := Convert_FP64_To_Int32(a[63:0])
//
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_cvtsd_si32
FORCE_INLINE int32_t _mm_cvtsd_si32(__m128d a)
{
#if defined(__aarch64__)
    return (int32_t) vgetq_lane_f64(vrndiq_f64(vreinterpretq_f64_m128d(a)), 0);
#else
    __m128d rnd = _mm_round_pd(a, _MM_FROUND_CUR_DIRECTION);
    double ret = ((double *) &rnd)[0];
    return (int32_t) ret;
#endif
}

// Convert the lower double-precision (64-bit) floating-point element in a to a
// 64-bit integer, and store the result in dst.
//
//   dst[63:0] := Convert_FP64_To_Int64(a[63:0])
//
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_cvtsd_si64
FORCE_INLINE int64_t _mm_cvtsd_si64(__m128d a)
{
#if defined(__aarch64__)
    return (int64_t) vgetq_lane_f64(vrndiq_f64(vreinterpretq_f64_m128d(a)), 0);
#else
    __m128d rnd = _mm_round_pd(a, _MM_FROUND_CUR_DIRECTION);
    double ret = ((double *) &rnd)[0];
    return (int64_t) ret;
#endif
}

// Convert the lower double-precision (64-bit) floating-point element in a to a
// 64-bit integer, and store the result in dst.
//
//   dst[63:0] := Convert_FP64_To_Int64(a[63:0])
//
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_cvtsd_si64x
#define _mm_cvtsd_si64x _mm_cvtsd_si64

// Convert the lower double-precision (64-bit) floating-point element in b to a
// single-precision (32-bit) floating-point element, store the result in the
// lower element of dst, and copy the upper 3 packed elements from a to the
// upper elements of dst.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_cvtsd_ss
FORCE_INLINE __m128 _mm_cvtsd_ss(__m128 a, __m128d b)
{
#if defined(__aarch64__)
    return vreinterpretq_m128_f32(vsetq_lane_f32(
        vget_lane_f32(vcvt_f32_f64(vreinterpretq_f64_m128d(b)), 0),
        vreinterpretq_f32_m128(a), 0));
#else
    return vreinterpretq_m128_f32(vsetq_lane_f32((float) ((double *) &b)[0],
                                                 vreinterpretq_f32_m128(a), 0));
#endif
}

// Copy the lower 32-bit integer in a to dst.
//
//   dst[31:0] := a[31:0]
//
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_cvtsi128_si32
FORCE_INLINE int _mm_cvtsi128_si32(__m128i a)
{
    return vgetq_lane_s32(vreinterpretq_s32_m128i(a), 0);
}

// Copy the lower 64-bit integer in a to dst.
//
//   dst[63:0] := a[63:0]
//
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_cvtsi128_si64
FORCE_INLINE int64_t _mm_cvtsi128_si64(__m128i a)
{
    return vgetq_lane_s64(vreinterpretq_s64_m128i(a), 0);
}

// Copy the lower 64-bit integer in a to dst.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_cvtsi128_si64x
#define _mm_cvtsi128_si64x(a) _mm_cvtsi128_si64(a)

// Convert the signed 32-bit integer b to a double-precision (64-bit)
// floating-point element, store the result in the lower element of dst, and
// copy the upper element from a to the upper element of dst.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_cvtsi32_sd
FORCE_INLINE __m128d _mm_cvtsi32_sd(__m128d a, int32_t b)
{
#if defined(__aarch64__)
    return vreinterpretq_m128d_f64(
        vsetq_lane_f64((double) b, vreinterpretq_f64_m128d(a), 0));
#else
    double bf = (double) b;
    return vreinterpretq_m128d_s64(
        vsetq_lane_s64(*(int64_t *) &bf, vreinterpretq_s64_m128d(a), 0));
#endif
}

// Copy the lower 64-bit integer in a to dst.
//
//   dst[63:0] := a[63:0]
//
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_cvtsi128_si64x
#define _mm_cvtsi128_si64x(a) _mm_cvtsi128_si64(a)

// Moves 32-bit integer a to the least significant 32 bits of an __m128 object,
// zero extending the upper bits.
//
//   r0 := a
//   r1 := 0x0
//   r2 := 0x0
//   r3 := 0x0
//
// https://msdn.microsoft.com/en-us/library/ct3539ha%28v=vs.90%29.aspx
FORCE_INLINE __m128i _mm_cvtsi32_si128(int a)
{
    return vreinterpretq_m128i_s32(vsetq_lane_s32(a, vdupq_n_s32(0), 0));
}

// Convert the signed 64-bit integer b to a double-precision (64-bit)
// floating-point element, store the result in the lower element of dst, and
// copy the upper element from a to the upper element of dst.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_cvtsi64_sd
FORCE_INLINE __m128d _mm_cvtsi64_sd(__m128d a, int64_t b)
{
#if defined(__aarch64__)
    return vreinterpretq_m128d_f64(
        vsetq_lane_f64((double) b, vreinterpretq_f64_m128d(a), 0));
#else
    double bf = (double) b;
    return vreinterpretq_m128d_s64(
        vsetq_lane_s64(*(int64_t *) &bf, vreinterpretq_s64_m128d(a), 0));
#endif
}

// Moves 64-bit integer a to the least significant 64 bits of an __m128 object,
// zero extending the upper bits.
//
//   r0 := a
//   r1 := 0x0
FORCE_INLINE __m128i _mm_cvtsi64_si128(int64_t a)
{
    return vreinterpretq_m128i_s64(vsetq_lane_s64(a, vdupq_n_s64(0), 0));
}

// Copy 64-bit integer a to the lower element of dst, and zero the upper
// element.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_cvtsi64x_si128
#define _mm_cvtsi64x_si128(a) _mm_cvtsi64_si128(a)

// Convert the signed 64-bit integer b to a double-precision (64-bit)
// floating-point element, store the result in the lower element of dst, and
// copy the upper element from a to the upper element of dst.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_cvtsi64x_sd
#define _mm_cvtsi64x_sd(a, b) _mm_cvtsi64_sd(a, b)

// Convert the lower single-precision (32-bit) floating-point element in b to a
// double-precision (64-bit) floating-point element, store the result in the
// lower element of dst, and copy the upper element from a to the upper element
// of dst.
//
//   dst[63:0] := Convert_FP32_To_FP64(b[31:0])
//   dst[127:64] := a[127:64]
//
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_cvtss_sd
FORCE_INLINE __m128d _mm_cvtss_sd(__m128d a, __m128 b)
{
    double d = (double) vgetq_lane_f32(vreinterpretq_f32_m128(b), 0);
#if defined(__aarch64__)
    return vreinterpretq_m128d_f64(
        vsetq_lane_f64(d, vreinterpretq_f64_m128d(a), 0));
#else
    return vreinterpretq_m128d_s64(
        vsetq_lane_s64(*(int64_t *) &d, vreinterpretq_s64_m128d(a), 0));
#endif
}

// Convert packed double-precision (64-bit) floating-point elements in a to
// packed 32-bit integers with truncation, and store the results in dst.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_cvttpd_epi32
FORCE_INLINE __m128i _mm_cvttpd_epi32(__m128d a)
{
    double a0 = ((double *) &a)[0];
    double a1 = ((double *) &a)[1];
    return _mm_set_epi32(0, 0, (int32_t) a1, (int32_t) a0);
}

// Convert packed double-precision (64-bit) floating-point elements in a to
// packed 32-bit integers with truncation, and store the results in dst.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_cvttpd_pi32
FORCE_INLINE __m64 _mm_cvttpd_pi32(__m128d a)
{
    double a0 = ((double *) &a)[0];
    double a1 = ((double *) &a)[1];
    int32_t ALIGN_STRUCT(16) data[2] = {(int32_t) a0, (int32_t) a1};
    return vreinterpret_m64_s32(vld1_s32(data));
}

// Converts the four single-precision, floating-point values of a to signed
// 32-bit integer values using truncate.
// https://msdn.microsoft.com/en-us/library/vstudio/1h005y6x(v=vs.100).aspx
FORCE_INLINE __m128i _mm_cvttps_epi32(__m128 a)
{
    return vreinterpretq_m128i_s32(vcvtq_s32_f32(vreinterpretq_f32_m128(a)));
}

// Convert the lower double-precision (64-bit) floating-point element in a to a
// 32-bit integer with truncation, and store the result in dst.
//
//   dst[63:0] := Convert_FP64_To_Int32_Truncate(a[63:0])
//
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_cvttsd_si32
FORCE_INLINE int32_t _mm_cvttsd_si32(__m128d a)
{
    double ret = *((double *) &a);
    return (int32_t) ret;
}

// Convert the lower double-precision (64-bit) floating-point element in a to a
// 64-bit integer with truncation, and store the result in dst.
//
//   dst[63:0] := Convert_FP64_To_Int64_Truncate(a[63:0])
//
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_cvttsd_si64
FORCE_INLINE int64_t _mm_cvttsd_si64(__m128d a)
{
#if defined(__aarch64__)
    return vgetq_lane_s64(vcvtq_s64_f64(vreinterpretq_f64_m128d(a)), 0);
#else
    double ret = *((double *) &a);
    return (int64_t) ret;
#endif
}

// Convert the lower double-precision (64-bit) floating-point element in a to a
// 64-bit integer with truncation, and store the result in dst.
//
//   dst[63:0] := Convert_FP64_To_Int64_Truncate(a[63:0])
//
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_cvttsd_si64x
#define _mm_cvttsd_si64x(a) _mm_cvttsd_si64(a)

// Divide packed double-precision (64-bit) floating-point elements in a by
// packed elements in b, and store the results in dst.
//
//  FOR j := 0 to 1
//    i := 64*j
//    dst[i+63:i] := a[i+63:i] / b[i+63:i]
//  ENDFOR
//
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_div_pd
FORCE_INLINE __m128d _mm_div_pd(__m128d a, __m128d b)
{
#if defined(__aarch64__)
    return vreinterpretq_m128d_f64(
        vdivq_f64(vreinterpretq_f64_m128d(a), vreinterpretq_f64_m128d(b)));
#else
    double *da = (double *) &a;
    double *db = (double *) &b;
    double c[2];
    c[0] = da[0] / db[0];
    c[1] = da[1] / db[1];
    return vld1q_f32((float32_t *) c);
#endif
}

// Divide the lower double-precision (64-bit) floating-point element in a by the
// lower double-precision (64-bit) floating-point element in b, store the result
// in the lower element of dst, and copy the upper element from a to the upper
// element of dst.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_div_sd
FORCE_INLINE __m128d _mm_div_sd(__m128d a, __m128d b)
{
#if defined(__aarch64__)
    float64x2_t tmp =
        vdivq_f64(vreinterpretq_f64_m128d(a), vreinterpretq_f64_m128d(b));
    return vreinterpretq_m128d_f64(
        vsetq_lane_f64(vgetq_lane_f64(vreinterpretq_f64_m128d(a), 1), tmp, 1));
#else
    return _mm_move_sd(a, _mm_div_pd(a, b));
#endif
}

// Extracts the selected signed or unsigned 16-bit integer from a and zero
// extends.
// https://msdn.microsoft.com/en-us/library/6dceta0c(v=vs.100).aspx
// FORCE_INLINE int _mm_extract_epi16(__m128i a, __constrange(0,8) int imm)
#define _mm_extract_epi16(a, imm) \
    vgetq_lane_u16(vreinterpretq_u16_m128i(a), (imm))

// Inserts the least significant 16 bits of b into the selected 16-bit integer
// of a.
// https://msdn.microsoft.com/en-us/library/kaze8hz1%28v=vs.100%29.aspx
// FORCE_INLINE __m128i _mm_insert_epi16(__m128i a, int b,
//                                       __constrange(0,8) int imm)
#define _mm_insert_epi16(a, b, imm)                                  \
    __extension__({                                                  \
        vreinterpretq_m128i_s16(                                     \
            vsetq_lane_s16((b), vreinterpretq_s16_m128i(a), (imm))); \
    })

// Loads two double-precision from 16-byte aligned memory, floating-point
// values.
//
//   dst[127:0] := MEM[mem_addr+127:mem_addr]
//
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_load_pd
FORCE_INLINE __m128d _mm_load_pd(const double *p)
{
#if defined(__aarch64__)
    return vreinterpretq_m128d_f64(vld1q_f64(p));
#else
    const float *fp = (const float *) p;
    float ALIGN_STRUCT(16) data[4] = {fp[0], fp[1], fp[2], fp[3]};
    return vreinterpretq_m128d_f32(vld1q_f32(data));
#endif
}

// Load a double-precision (64-bit) floating-point element from memory into both
// elements of dst.
//
//   dst[63:0] := MEM[mem_addr+63:mem_addr]
//   dst[127:64] := MEM[mem_addr+63:mem_addr]
//
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_load_pd1
#define _mm_load_pd1 _mm_load1_pd

// Load a double-precision (64-bit) floating-point element from memory into the
// lower of dst, and zero the upper element. mem_addr does not need to be
// aligned on any particular boundary.
//
//   dst[63:0] := MEM[mem_addr+63:mem_addr]
//   dst[127:64] := 0
//
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_load_sd
FORCE_INLINE __m128d _mm_load_sd(const double *p)
{
#if defined(__aarch64__)
    return vreinterpretq_m128d_f64(vsetq_lane_f64(*p, vdupq_n_f64(0), 0));
#else
    const float *fp = (const float *) p;
    float ALIGN_STRUCT(16) data[4] = {fp[0], fp[1], 0, 0};
    return vreinterpretq_m128d_f32(vld1q_f32(data));
#endif
}

// Loads 128-bit value. :
// https://msdn.microsoft.com/en-us/library/atzzad1h(v=vs.80).aspx
FORCE_INLINE __m128i _mm_load_si128(const __m128i *p)
{
    return vreinterpretq_m128i_s32(vld1q_s32((const int32_t *) p));
}

// Load a double-precision (64-bit) floating-point element from memory into both
// elements of dst.
//
//   dst[63:0] := MEM[mem_addr+63:mem_addr]
//   dst[127:64] := MEM[mem_addr+63:mem_addr]
//
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_load1_pd
FORCE_INLINE __m128d _mm_load1_pd(const double *p)
{
#if defined(__aarch64__)
    return vreinterpretq_m128d_f64(vld1q_dup_f64(p));
#else
    return vreinterpretq_m128d_s64(vdupq_n_s64(*(const int64_t *) p));
#endif
}

// Load a double-precision (64-bit) floating-point element from memory into the
// upper element of dst, and copy the lower element from a to dst. mem_addr does
// not need to be aligned on any particular boundary.
//
//   dst[63:0] := a[63:0]
//   dst[127:64] := MEM[mem_addr+63:mem_addr]
//
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_loadh_pd
FORCE_INLINE __m128d _mm_loadh_pd(__m128d a, const double *p)
{
#if defined(__aarch64__)
    return vreinterpretq_m128d_f64(
        vcombine_f64(vget_low_f64(vreinterpretq_f64_m128d(a)), vld1_f64(p)));
#else
    return vreinterpretq_m128d_f32(vcombine_f32(
        vget_low_f32(vreinterpretq_f32_m128d(a)), vld1_f32((const float *) p)));
#endif
}

// Load 64-bit integer from memory into the first element of dst.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_loadl_epi64
FORCE_INLINE __m128i _mm_loadl_epi64(__m128i const *p)
{
    /* Load the lower 64 bits of the value pointed to by p into the
     * lower 64 bits of the result, zeroing the upper 64 bits of the result.
     */
    return vreinterpretq_m128i_s32(
        vcombine_s32(vld1_s32((int32_t const *) p), vcreate_s32(0)));
}

// Load a double-precision (64-bit) floating-point element from memory into the
// lower element of dst, and copy the upper element from a to dst. mem_addr does
// not need to be aligned on any particular boundary.
//
//   dst[63:0] := MEM[mem_addr+63:mem_addr]
//   dst[127:64] := a[127:64]
//
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_loadl_pd
FORCE_INLINE __m128d _mm_loadl_pd(__m128d a, const double *p)
{
#if defined(__aarch64__)
    return vreinterpretq_m128d_f64(
        vcombine_f64(vld1_f64(p), vget_high_f64(vreinterpretq_f64_m128d(a))));
#else
    return vreinterpretq_m128d_f32(
        vcombine_f32(vld1_f32((const float *) p),
                     vget_high_f32(vreinterpretq_f32_m128d(a))));
#endif
}

// Load 2 double-precision (64-bit) floating-point elements from memory into dst
// in reverse order. mem_addr must be aligned on a 16-byte boundary or a
// general-protection exception may be generated.
//
//   dst[63:0] := MEM[mem_addr+127:mem_addr+64]
//   dst[127:64] := MEM[mem_addr+63:mem_addr]
//
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_loadr_pd
FORCE_INLINE __m128d _mm_loadr_pd(const double *p)
{
#if defined(__aarch64__)
    float64x2_t v = vld1q_f64(p);
    return vreinterpretq_m128d_f64(vextq_f64(v, v, 1));
#else
    int64x2_t v = vld1q_s64((const int64_t *) p);
    return vreinterpretq_m128d_s64(vextq_s64(v, v, 1));
#endif
}

// Loads two double-precision from unaligned memory, floating-point values.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_loadu_pd
FORCE_INLINE __m128d _mm_loadu_pd(const double *p)
{
    return _mm_load_pd(p);
}

// Loads 128-bit value. :
// https://msdn.microsoft.com/zh-cn/library/f4k12ae8(v=vs.90).aspx
FORCE_INLINE __m128i _mm_loadu_si128(const __m128i *p)
{
    return vreinterpretq_m128i_s32(vld1q_s32((const int32_t *) p));
}

// Load unaligned 32-bit integer from memory into the first element of dst.
//
//   dst[31:0] := MEM[mem_addr+31:mem_addr]
//   dst[MAX:32] := 0
//
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_loadu_si32
FORCE_INLINE __m128i _mm_loadu_si32(const void *p)
{
    return vreinterpretq_m128i_s32(
        vsetq_lane_s32(*(const int32_t *) p, vdupq_n_s32(0), 0));
}

// Multiplies the 8 signed 16-bit integers from a by the 8 signed 16-bit
// integers from b.
//
//   r0 := (a0 * b0) + (a1 * b1)
//   r1 := (a2 * b2) + (a3 * b3)
//   r2 := (a4 * b4) + (a5 * b5)
//   r3 := (a6 * b6) + (a7 * b7)
// https://msdn.microsoft.com/en-us/library/yht36sa6(v=vs.90).aspx
FORCE_INLINE __m128i _mm_madd_epi16(__m128i a, __m128i b)
{
    int32x4_t low = vmull_s16(vget_low_s16(vreinterpretq_s16_m128i(a)),
                              vget_low_s16(vreinterpretq_s16_m128i(b)));
#if defined(__aarch64__)
    int32x4_t high =
        vmull_high_s16(vreinterpretq_s16_m128i(a), vreinterpretq_s16_m128i(b));

    return vreinterpretq_m128i_s32(vpaddq_s32(low, high));
#else
    int32x4_t high = vmull_s16(vget_high_s16(vreinterpretq_s16_m128i(a)),
                               vget_high_s16(vreinterpretq_s16_m128i(b)));

    int32x2_t low_sum = vpadd_s32(vget_low_s32(low), vget_high_s32(low));
    int32x2_t high_sum = vpadd_s32(vget_low_s32(high), vget_high_s32(high));

    return vreinterpretq_m128i_s32(vcombine_s32(low_sum, high_sum));
#endif
}

// Conditionally store 8-bit integer elements from a into memory using mask
// (elements are not stored when the highest bit is not set in the corresponding
// element) and a non-temporal memory hint. mem_addr does not need to be aligned
// on any particular boundary.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_maskmoveu_si128
FORCE_INLINE void _mm_maskmoveu_si128(__m128i a, __m128i mask, char *mem_addr)
{
    int8x16_t shr_mask = vshrq_n_s8(vreinterpretq_s8_m128i(mask), 7);
    __m128 b = _mm_load_ps((const float *) mem_addr);
    int8x16_t masked =
        vbslq_s8(vreinterpretq_u8_s8(shr_mask), vreinterpretq_s8_m128i(a),
                 vreinterpretq_s8_m128(b));
    vst1q_s8((int8_t *) mem_addr, masked);
}

// Computes the pairwise maxima of the 8 signed 16-bit integers from a and the 8
// signed 16-bit integers from b.
// https://msdn.microsoft.com/en-us/LIBRary/3x060h7c(v=vs.100).aspx
FORCE_INLINE __m128i _mm_max_epi16(__m128i a, __m128i b)
{
    return vreinterpretq_m128i_s16(
        vmaxq_s16(vreinterpretq_s16_m128i(a), vreinterpretq_s16_m128i(b)));
}

// Computes the pairwise maxima of the 16 unsigned 8-bit integers from a and the
// 16 unsigned 8-bit integers from b.
// https://msdn.microsoft.com/en-us/library/st6634za(v=vs.100).aspx
FORCE_INLINE __m128i _mm_max_epu8(__m128i a, __m128i b)
{
    return vreinterpretq_m128i_u8(
        vmaxq_u8(vreinterpretq_u8_m128i(a), vreinterpretq_u8_m128i(b)));
}

// Compare packed double-precision (64-bit) floating-point elements in a and b,
// and store packed maximum values in dst.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_max_pd
FORCE_INLINE __m128d _mm_max_pd(__m128d a, __m128d b)
{
#if defined(__aarch64__)
#if SSE2NEON_PRECISE_MINMAX
    float64x2_t _a = vreinterpretq_f64_m128d(a);
    float64x2_t _b = vreinterpretq_f64_m128d(b);
    return vreinterpretq_m128d_f64(vbslq_f64(vcgtq_f64(_a, _b), _a, _b));
#else
    return vreinterpretq_m128d_f64(
        vmaxq_f64(vreinterpretq_f64_m128d(a), vreinterpretq_f64_m128d(b)));
#endif
#else
    uint64_t a0 = (uint64_t) vget_low_u64(vreinterpretq_u64_m128d(a));
    uint64_t a1 = (uint64_t) vget_high_u64(vreinterpretq_u64_m128d(a));
    uint64_t b0 = (uint64_t) vget_low_u64(vreinterpretq_u64_m128d(b));
    uint64_t b1 = (uint64_t) vget_high_u64(vreinterpretq_u64_m128d(b));
    uint64_t d[2];
    d[0] = (*(double *) &a0) > (*(double *) &b0) ? a0 : b0;
    d[1] = (*(double *) &a1) > (*(double *) &b1) ? a1 : b1;

    return vreinterpretq_m128d_u64(vld1q_u64(d));
#endif
}

// Compare the lower double-precision (64-bit) floating-point elements in a and
// b, store the maximum value in the lower element of dst, and copy the upper
// element from a to the upper element of dst.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_max_sd
FORCE_INLINE __m128d _mm_max_sd(__m128d a, __m128d b)
{
#if defined(__aarch64__)
    return _mm_move_sd(a, _mm_max_pd(a, b));
#else
    double *da = (double *) &a;
    double *db = (double *) &b;
    double c[2] = {da[0] > db[0] ? da[0] : db[0], da[1]};
    return vreinterpretq_m128d_f32(vld1q_f32((float32_t *) c));
#endif
}

// Computes the pairwise minima of the 8 signed 16-bit integers from a and the 8
// signed 16-bit integers from b.
// https://msdn.microsoft.com/en-us/library/vstudio/6te997ew(v=vs.100).aspx
FORCE_INLINE __m128i _mm_min_epi16(__m128i a, __m128i b)
{
    return vreinterpretq_m128i_s16(
        vminq_s16(vreinterpretq_s16_m128i(a), vreinterpretq_s16_m128i(b)));
}

// Computes the pairwise minima of the 16 unsigned 8-bit integers from a and the
// 16 unsigned 8-bit integers from b.
// https://msdn.microsoft.com/ko-kr/library/17k8cf58(v=vs.100).aspxx
FORCE_INLINE __m128i _mm_min_epu8(__m128i a, __m128i b)
{
    return vreinterpretq_m128i_u8(
        vminq_u8(vreinterpretq_u8_m128i(a), vreinterpretq_u8_m128i(b)));
}

// Compare packed double-precision (64-bit) floating-point elements in a and b,
// and store packed minimum values in dst.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_min_pd
FORCE_INLINE __m128d _mm_min_pd(__m128d a, __m128d b)
{
#if defined(__aarch64__)
#if SSE2NEON_PRECISE_MINMAX
    float64x2_t _a = vreinterpretq_f64_m128d(a);
    float64x2_t _b = vreinterpretq_f64_m128d(b);
    return vreinterpretq_m128d_f64(vbslq_f64(vcltq_f64(_a, _b), _a, _b));
#else
    return vreinterpretq_m128d_f64(
        vminq_f64(vreinterpretq_f64_m128d(a), vreinterpretq_f64_m128d(b)));
#endif
#else
    uint64_t a0 = (uint64_t) vget_low_u64(vreinterpretq_u64_m128d(a));
    uint64_t a1 = (uint64_t) vget_high_u64(vreinterpretq_u64_m128d(a));
    uint64_t b0 = (uint64_t) vget_low_u64(vreinterpretq_u64_m128d(b));
    uint64_t b1 = (uint64_t) vget_high_u64(vreinterpretq_u64_m128d(b));
    uint64_t d[2];
    d[0] = (*(double *) &a0) < (*(double *) &b0) ? a0 : b0;
    d[1] = (*(double *) &a1) < (*(double *) &b1) ? a1 : b1;
    return vreinterpretq_m128d_u64(vld1q_u64(d));
#endif
}

// Compare the lower double-precision (64-bit) floating-point elements in a and
// b, store the minimum value in the lower element of dst, and copy the upper
// element from a to the upper element of dst.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_min_sd
FORCE_INLINE __m128d _mm_min_sd(__m128d a, __m128d b)
{
#if defined(__aarch64__)
    return _mm_move_sd(a, _mm_min_pd(a, b));
#else
    double *da = (double *) &a;
    double *db = (double *) &b;
    double c[2] = {da[0] < db[0] ? da[0] : db[0], da[1]};
    return vreinterpretq_m128d_f32(vld1q_f32((float32_t *) c));
#endif
}

// Copy the lower 64-bit integer in a to the lower element of dst, and zero the
// upper element.
//
//   dst[63:0] := a[63:0]
//   dst[127:64] := 0
//
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_move_epi64
FORCE_INLINE __m128i _mm_move_epi64(__m128i a)
{
    return vreinterpretq_m128i_s64(
        vsetq_lane_s64(0, vreinterpretq_s64_m128i(a), 1));
}

// Move the lower double-precision (64-bit) floating-point element from b to the
// lower element of dst, and copy the upper element from a to the upper element
// of dst.
//
//   dst[63:0] := b[63:0]
//   dst[127:64] := a[127:64]
//
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_move_sd
FORCE_INLINE __m128d _mm_move_sd(__m128d a, __m128d b)
{
    return vreinterpretq_m128d_f32(
        vcombine_f32(vget_low_f32(vreinterpretq_f32_m128d(b)),
                     vget_high_f32(vreinterpretq_f32_m128d(a))));
}

// NEON does not provide a version of this function.
// Creates a 16-bit mask from the most significant bits of the 16 signed or
// unsigned 8-bit integers in a and zero extends the upper bits.
// https://msdn.microsoft.com/en-us/library/vstudio/s090c8fk(v=vs.100).aspx
FORCE_INLINE int _mm_movemask_epi8(__m128i a)
{
    // Use increasingly wide shifts+adds to collect the sign bits
    // together.
    // Since the widening shifts would be rather confusing to follow in little
    // endian, everything will be illustrated in big endian order instead. This
    // has a different result - the bits would actually be reversed on a big
    // endian machine.

    // Starting input (only half the elements are shown):
    // 89 ff 1d c0 00 10 99 33
    uint8x16_t input = vreinterpretq_u8_m128i(a);

    // Shift out everything but the sign bits with an unsigned shift right.
    //
    // Bytes of the vector::
    // 89 ff 1d c0 00 10 99 33
    // \  \  \  \  \  \  \  \    high_bits = (uint16x4_t)(input >> 7)
    //  |  |  |  |  |  |  |  |
    // 01 01 00 01 00 00 01 00
    //
    // Bits of first important lane(s):
    // 10001001 (89)
    // \______
    //        |
    // 00000001 (01)
    uint16x8_t high_bits = vreinterpretq_u16_u8(vshrq_n_u8(input, 7));

    // Merge the even lanes together with a 16-bit unsigned shift right + add.
    // 'xx' represents garbage data which will be ignored in the final result.
    // In the important bytes, the add functions like a binary OR.
    //
    // 01 01 00 01 00 00 01 00
    //  \_ |  \_ |  \_ |  \_ |   paired16 = (uint32x4_t)(input + (input >> 7))
    //    \|    \|    \|    \|
    // xx 03 xx 01 xx 00 xx 02
    //
    // 00000001 00000001 (01 01)
    //        \_______ |
    //                \|
    // xxxxxxxx xxxxxx11 (xx 03)
    uint32x4_t paired16 =
        vreinterpretq_u32_u16(vsraq_n_u16(high_bits, high_bits, 7));

    // Repeat with a wider 32-bit shift + add.
    // xx 03 xx 01 xx 00 xx 02
    //     \____ |     \____ |  paired32 = (uint64x1_t)(paired16 + (paired16 >>
    //     14))
    //          \|          \|
    // xx xx xx 0d xx xx xx 02
    //
    // 00000011 00000001 (03 01)
    //        \\_____ ||
    //         '----.\||
    // xxxxxxxx xxxx1101 (xx 0d)
    uint64x2_t paired32 =
        vreinterpretq_u64_u32(vsraq_n_u32(paired16, paired16, 14));

    // Last, an even wider 64-bit shift + add to get our result in the low 8 bit
    // lanes. xx xx xx 0d xx xx xx 02
    //            \_________ |   paired64 = (uint8x8_t)(paired32 + (paired32 >>
    //            28))
    //                      \|
    // xx xx xx xx xx xx xx d2
    //
    // 00001101 00000010 (0d 02)
    //     \   \___ |  |
    //      '---.  \|  |
    // xxxxxxxx 11010010 (xx d2)
    uint8x16_t paired64 =
        vreinterpretq_u8_u64(vsraq_n_u64(paired32, paired32, 28));

    // Extract the low 8 bits from each 64-bit lane with 2 8-bit extracts.
    // xx xx xx xx xx xx xx d2
    //                      ||  return paired64[0]
    //                      d2
    // Note: Little endian would return the correct value 4b (01001011) instead.
    return vgetq_lane_u8(paired64, 0) | ((int) vgetq_lane_u8(paired64, 8) << 8);
}

// Set each bit of mask dst based on the most significant bit of the
// corresponding packed double-precision (64-bit) floating-point element in a.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_movemask_pd
FORCE_INLINE int _mm_movemask_pd(__m128d a)
{
    uint64x2_t input = vreinterpretq_u64_m128d(a);
    uint64x2_t high_bits = vshrq_n_u64(input, 63);
    return vgetq_lane_u64(high_bits, 0) | (vgetq_lane_u64(high_bits, 1) << 1);
}

// Copy the lower 64-bit integer in a to dst.
//
//   dst[63:0] := a[63:0]
//
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_movepi64_pi64
FORCE_INLINE __m64 _mm_movepi64_pi64(__m128i a)
{
    return vreinterpret_m64_s64(vget_low_s64(vreinterpretq_s64_m128i(a)));
}

// Copy the 64-bit integer a to the lower element of dst, and zero the upper
// element.
//
//   dst[63:0] := a[63:0]
//   dst[127:64] := 0
//
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_movpi64_epi64
FORCE_INLINE __m128i _mm_movpi64_epi64(__m64 a)
{
    return vreinterpretq_m128i_s64(
        vcombine_s64(vreinterpret_s64_m64(a), vdup_n_s64(0)));
}

// Multiply the low unsigned 32-bit integers from each packed 64-bit element in
// a and b, and store the unsigned 64-bit results in dst.
//
//   r0 :=  (a0 & 0xFFFFFFFF) * (b0 & 0xFFFFFFFF)
//   r1 :=  (a2 & 0xFFFFFFFF) * (b2 & 0xFFFFFFFF)
FORCE_INLINE __m128i _mm_mul_epu32(__m128i a, __m128i b)
{
    // vmull_u32 upcasts instead of masking, so we downcast.
    uint32x2_t a_lo = vmovn_u64(vreinterpretq_u64_m128i(a));
    uint32x2_t b_lo = vmovn_u64(vreinterpretq_u64_m128i(b));
    return vreinterpretq_m128i_u64(vmull_u32(a_lo, b_lo));
}

// Multiply packed double-precision (64-bit) floating-point elements in a and b,
// and store the results in dst.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_mul_pd
FORCE_INLINE __m128d _mm_mul_pd(__m128d a, __m128d b)
{
#if defined(__aarch64__)
    return vreinterpretq_m128d_f64(
        vmulq_f64(vreinterpretq_f64_m128d(a), vreinterpretq_f64_m128d(b)));
#else
    double *da = (double *) &a;
    double *db = (double *) &b;
    double c[2];
    c[0] = da[0] * db[0];
    c[1] = da[1] * db[1];
    return vld1q_f32((float32_t *) c);
#endif
}

// Multiply the lower double-precision (64-bit) floating-point element in a and
// b, store the result in the lower element of dst, and copy the upper element
// from a to the upper element of dst.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=mm_mul_sd
FORCE_INLINE __m128d _mm_mul_sd(__m128d a, __m128d b)
{
    return _mm_move_sd(a, _mm_mul_pd(a, b));
}

// Multiply the low unsigned 32-bit integers from a and b, and store the
// unsigned 64-bit result in dst.
//
//   dst[63:0] := a[31:0] * b[31:0]
//
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_mul_su32
FORCE_INLINE __m64 _mm_mul_su32(__m64 a, __m64 b)
{
    return vreinterpret_m64_u64(vget_low_u64(
        vmull_u32(vreinterpret_u32_m64(a), vreinterpret_u32_m64(b))));
}

// Multiplies the 8 signed 16-bit integers from a by the 8 signed 16-bit
// integers from b.
//
//   r0 := (a0 * b0)[31:16]
//   r1 := (a1 * b1)[31:16]
//   ...
//   r7 := (a7 * b7)[31:16]
//
// https://msdn.microsoft.com/en-us/library/vstudio/59hddw1d(v=vs.100).aspx
FORCE_INLINE __m128i _mm_mulhi_epi16(__m128i a, __m128i b)
{
    /* FIXME: issue with large values because of result saturation */
    // int16x8_t ret = vqdmulhq_s16(vreinterpretq_s16_m128i(a),
    // vreinterpretq_s16_m128i(b)); /* =2*a*b */ return
    // vreinterpretq_m128i_s16(vshrq_n_s16(ret, 1));
    int16x4_t a3210 = vget_low_s16(vreinterpretq_s16_m128i(a));
    int16x4_t b3210 = vget_low_s16(vreinterpretq_s16_m128i(b));
    int32x4_t ab3210 = vmull_s16(a3210, b3210); /* 3333222211110000 */
    int16x4_t a7654 = vget_high_s16(vreinterpretq_s16_m128i(a));
    int16x4_t b7654 = vget_high_s16(vreinterpretq_s16_m128i(b));
    int32x4_t ab7654 = vmull_s16(a7654, b7654); /* 7777666655554444 */
    uint16x8x2_t r =
        vuzpq_u16(vreinterpretq_u16_s32(ab3210), vreinterpretq_u16_s32(ab7654));
    return vreinterpretq_m128i_u16(r.val[1]);
}

// Multiply the packed unsigned 16-bit integers in a and b, producing
// intermediate 32-bit integers, and store the high 16 bits of the intermediate
// integers in dst.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_mulhi_epu16
FORCE_INLINE __m128i _mm_mulhi_epu16(__m128i a, __m128i b)
{
    uint16x4_t a3210 = vget_low_u16(vreinterpretq_u16_m128i(a));
    uint16x4_t b3210 = vget_low_u16(vreinterpretq_u16_m128i(b));
    uint32x4_t ab3210 = vmull_u16(a3210, b3210);
#if defined(__aarch64__)
    uint32x4_t ab7654 =
        vmull_high_u16(vreinterpretq_u16_m128i(a), vreinterpretq_u16_m128i(b));
    uint16x8_t r = vuzp2q_u16(vreinterpretq_u16_u32(ab3210),
                              vreinterpretq_u16_u32(ab7654));
    return vreinterpretq_m128i_u16(r);
#else
    uint16x4_t a7654 = vget_high_u16(vreinterpretq_u16_m128i(a));
    uint16x4_t b7654 = vget_high_u16(vreinterpretq_u16_m128i(b));
    uint32x4_t ab7654 = vmull_u16(a7654, b7654);
    uint16x8x2_t r =
        vuzpq_u16(vreinterpretq_u16_u32(ab3210), vreinterpretq_u16_u32(ab7654));
    return vreinterpretq_m128i_u16(r.val[1]);
#endif
}

// Multiplies the 8 signed or unsigned 16-bit integers from a by the 8 signed or
// unsigned 16-bit integers from b.
//
//   r0 := (a0 * b0)[15:0]
//   r1 := (a1 * b1)[15:0]
//   ...
//   r7 := (a7 * b7)[15:0]
//
// https://msdn.microsoft.com/en-us/library/vstudio/9ks1472s(v=vs.100).aspx
FORCE_INLINE __m128i _mm_mullo_epi16(__m128i a, __m128i b)
{
    return vreinterpretq_m128i_s16(
        vmulq_s16(vreinterpretq_s16_m128i(a), vreinterpretq_s16_m128i(b)));
}

// Compute the bitwise OR of packed double-precision (64-bit) floating-point
// elements in a and b, and store the results in dst.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=mm_or_pd
FORCE_INLINE __m128d _mm_or_pd(__m128d a, __m128d b)
{
    return vreinterpretq_m128d_s64(
        vorrq_s64(vreinterpretq_s64_m128d(a), vreinterpretq_s64_m128d(b)));
}

// Computes the bitwise OR of the 128-bit value in a and the 128-bit value in b.
//
//   r := a | b
//
// https://msdn.microsoft.com/en-us/library/vstudio/ew8ty0db(v=vs.100).aspx
FORCE_INLINE __m128i _mm_or_si128(__m128i a, __m128i b)
{
    return vreinterpretq_m128i_s32(
        vorrq_s32(vreinterpretq_s32_m128i(a), vreinterpretq_s32_m128i(b)));
}

// Packs the 16 signed 16-bit integers from a and b into 8-bit integers and
// saturates.
// https://msdn.microsoft.com/en-us/library/k4y4f7w5%28v=vs.90%29.aspx
FORCE_INLINE __m128i _mm_packs_epi16(__m128i a, __m128i b)
{
    return vreinterpretq_m128i_s8(
        vcombine_s8(vqmovn_s16(vreinterpretq_s16_m128i(a)),
                    vqmovn_s16(vreinterpretq_s16_m128i(b))));
}

// Packs the 8 signed 32-bit integers from a and b into signed 16-bit integers
// and saturates.
//
//   r0 := SignedSaturate(a0)
//   r1 := SignedSaturate(a1)
//   r2 := SignedSaturate(a2)
//   r3 := SignedSaturate(a3)
//   r4 := SignedSaturate(b0)
//   r5 := SignedSaturate(b1)
//   r6 := SignedSaturate(b2)
//   r7 := SignedSaturate(b3)
//
// https://msdn.microsoft.com/en-us/library/393t56f9%28v=vs.90%29.aspx
FORCE_INLINE __m128i _mm_packs_epi32(__m128i a, __m128i b)
{
    return vreinterpretq_m128i_s16(
        vcombine_s16(vqmovn_s32(vreinterpretq_s32_m128i(a)),
                     vqmovn_s32(vreinterpretq_s32_m128i(b))));
}

// Packs the 16 signed 16 - bit integers from a and b into 8 - bit unsigned
// integers and saturates.
//
//   r0 := UnsignedSaturate(a0)
//   r1 := UnsignedSaturate(a1)
//   ...
//   r7 := UnsignedSaturate(a7)
//   r8 := UnsignedSaturate(b0)
//   r9 := UnsignedSaturate(b1)
//   ...
//   r15 := UnsignedSaturate(b7)
//
// https://msdn.microsoft.com/en-us/library/07ad1wx4(v=vs.100).aspx
FORCE_INLINE __m128i _mm_packus_epi16(const __m128i a, const __m128i b)
{
    return vreinterpretq_m128i_u8(
        vcombine_u8(vqmovun_s16(vreinterpretq_s16_m128i(a)),
                    vqmovun_s16(vreinterpretq_s16_m128i(b))));
}

// Pause the processor. This is typically used in spin-wait loops and depending
// on the x86 processor typical values are in the 40-100 cycle range. The
// 'yield' instruction isn't a good fit because it's effectively a nop on most
// Arm cores. Experience with several databases has shown has shown an 'isb' is
// a reasonable approximation.
FORCE_INLINE void _mm_pause()
{
    __asm__ __volatile__("isb\n");
}

// Compute the absolute differences of packed unsigned 8-bit integers in a and
// b, then horizontally sum each consecutive 8 differences to produce two
// unsigned 16-bit integers, and pack these unsigned 16-bit integers in the low
// 16 bits of 64-bit elements in dst.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_sad_epu8
FORCE_INLINE __m128i _mm_sad_epu8(__m128i a, __m128i b)
{
    uint16x8_t t = vpaddlq_u8(vabdq_u8((uint8x16_t) a, (uint8x16_t) b));
    return vreinterpretq_m128i_u64(vpaddlq_u32(vpaddlq_u16(t)));
}

// Sets the 8 signed 16-bit integer values.
// https://msdn.microsoft.com/en-au/library/3e0fek84(v=vs.90).aspx
FORCE_INLINE __m128i _mm_set_epi16(short i7,
                                   short i6,
                                   short i5,
                                   short i4,
                                   short i3,
                                   short i2,
                                   short i1,
                                   short i0)
{
    int16_t ALIGN_STRUCT(16) data[8] = {i0, i1, i2, i3, i4, i5, i6, i7};
    return vreinterpretq_m128i_s16(vld1q_s16(data));
}

// Sets the 4 signed 32-bit integer values.
// https://msdn.microsoft.com/en-us/library/vstudio/019beekt(v=vs.100).aspx
FORCE_INLINE __m128i _mm_set_epi32(int i3, int i2, int i1, int i0)
{
    int32_t ALIGN_STRUCT(16) data[4] = {i0, i1, i2, i3};
    return vreinterpretq_m128i_s32(vld1q_s32(data));
}

// Returns the __m128i structure with its two 64-bit integer values
// initialized to the values of the two 64-bit integers passed in.
// https://msdn.microsoft.com/en-us/library/dk2sdw0h(v=vs.120).aspx
FORCE_INLINE __m128i _mm_set_epi64(__m64 i1, __m64 i2)
{
    return _mm_set_epi64x((int64_t) i1, (int64_t) i2);
}

// Returns the __m128i structure with its two 64-bit integer values
// initialized to the values of the two 64-bit integers passed in.
// https://msdn.microsoft.com/en-us/library/dk2sdw0h(v=vs.120).aspx
FORCE_INLINE __m128i _mm_set_epi64x(int64_t i1, int64_t i2)
{
    return vreinterpretq_m128i_s64(
        vcombine_s64(vcreate_s64(i2), vcreate_s64(i1)));
}

// Sets the 16 signed 8-bit integer values.
// https://msdn.microsoft.com/en-us/library/x0cx8zd3(v=vs.90).aspx
FORCE_INLINE __m128i _mm_set_epi8(signed char b15,
                                  signed char b14,
                                  signed char b13,
                                  signed char b12,
                                  signed char b11,
                                  signed char b10,
                                  signed char b9,
                                  signed char b8,
                                  signed char b7,
                                  signed char b6,
                                  signed char b5,
                                  signed char b4,
                                  signed char b3,
                                  signed char b2,
                                  signed char b1,
                                  signed char b0)
{
    int8_t ALIGN_STRUCT(16)
        data[16] = {(int8_t) b0,  (int8_t) b1,  (int8_t) b2,  (int8_t) b3,
                    (int8_t) b4,  (int8_t) b5,  (int8_t) b6,  (int8_t) b7,
                    (int8_t) b8,  (int8_t) b9,  (int8_t) b10, (int8_t) b11,
                    (int8_t) b12, (int8_t) b13, (int8_t) b14, (int8_t) b15};
    return (__m128i) vld1q_s8(data);
}

// Set packed double-precision (64-bit) floating-point elements in dst with the
// supplied values.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_set_pd
FORCE_INLINE __m128d _mm_set_pd(double e1, double e0)
{
    double ALIGN_STRUCT(16) data[2] = {e0, e1};
#if defined(__aarch64__)
    return vreinterpretq_m128d_f64(vld1q_f64((float64_t *) data));
#else
    return vreinterpretq_m128d_f32(vld1q_f32((float32_t *) data));
#endif
}

// Broadcast double-precision (64-bit) floating-point value a to all elements of
// dst.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_set_pd1
#define _mm_set_pd1 _mm_set1_pd

// Copy double-precision (64-bit) floating-point element a to the lower element
// of dst, and zero the upper element.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_set_sd
FORCE_INLINE __m128d _mm_set_sd(double a)
{
#if defined(__aarch64__)
    return vreinterpretq_m128d_f64(vsetq_lane_f64(a, vdupq_n_f64(0), 0));
#else
    return _mm_set_pd(0, a);
#endif
}

// Sets the 8 signed 16-bit integer values to w.
//
//   r0 := w
//   r1 := w
//   ...
//   r7 := w
//
// https://msdn.microsoft.com/en-us/library/k0ya3x0e(v=vs.90).aspx
FORCE_INLINE __m128i _mm_set1_epi16(short w)
{
    return vreinterpretq_m128i_s16(vdupq_n_s16(w));
}

// Sets the 4 signed 32-bit integer values to i.
//
//   r0 := i
//   r1 := i
//   r2 := i
//   r3 := I
//
// https://msdn.microsoft.com/en-us/library/vstudio/h4xscxat(v=vs.100).aspx
FORCE_INLINE __m128i _mm_set1_epi32(int _i)
{
    return vreinterpretq_m128i_s32(vdupq_n_s32(_i));
}

// Sets the 2 signed 64-bit integer values to i.
// https://docs.microsoft.com/en-us/previous-versions/visualstudio/visual-studio-2010/whtfzhzk(v=vs.100)
FORCE_INLINE __m128i _mm_set1_epi64(__m64 _i)
{
    return vreinterpretq_m128i_s64(vdupq_n_s64((int64_t) _i));
}

// Sets the 2 signed 64-bit integer values to i.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_set1_epi64x
FORCE_INLINE __m128i _mm_set1_epi64x(int64_t _i)
{
    return vreinterpretq_m128i_s64(vdupq_n_s64(_i));
}

// Sets the 16 signed 8-bit integer values to b.
//
//   r0 := b
//   r1 := b
//   ...
//   r15 := b
//
// https://msdn.microsoft.com/en-us/library/6e14xhyf(v=vs.100).aspx
FORCE_INLINE __m128i _mm_set1_epi8(signed char w)
{
    return vreinterpretq_m128i_s8(vdupq_n_s8(w));
}

// Broadcast double-precision (64-bit) floating-point value a to all elements of
// dst.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_set1_pd
FORCE_INLINE __m128d _mm_set1_pd(double d)
{
#if defined(__aarch64__)
    return vreinterpretq_m128d_f64(vdupq_n_f64(d));
#else
    return vreinterpretq_m128d_s64(vdupq_n_s64(*(int64_t *) &d));
#endif
}

// Sets the 8 signed 16-bit integer values in reverse order.
//
// Return Value
//   r0 := w0
//   r1 := w1
//   ...
//   r7 := w7
FORCE_INLINE __m128i _mm_setr_epi16(short w0,
                                    short w1,
                                    short w2,
                                    short w3,
                                    short w4,
                                    short w5,
                                    short w6,
                                    short w7)
{
    int16_t ALIGN_STRUCT(16) data[8] = {w0, w1, w2, w3, w4, w5, w6, w7};
    return vreinterpretq_m128i_s16(vld1q_s16((int16_t *) data));
}

// Sets the 4 signed 32-bit integer values in reverse order
// https://technet.microsoft.com/en-us/library/security/27yb3ee5(v=vs.90).aspx
FORCE_INLINE __m128i _mm_setr_epi32(int i3, int i2, int i1, int i0)
{
    int32_t ALIGN_STRUCT(16) data[4] = {i3, i2, i1, i0};
    return vreinterpretq_m128i_s32(vld1q_s32(data));
}

// Set packed 64-bit integers in dst with the supplied values in reverse order.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_setr_epi64
FORCE_INLINE __m128i _mm_setr_epi64(__m64 e1, __m64 e0)
{
    return vreinterpretq_m128i_s64(vcombine_s64(e1, e0));
}

// Sets the 16 signed 8-bit integer values in reverse order.
// https://msdn.microsoft.com/en-us/library/2khb9c7k(v=vs.90).aspx
FORCE_INLINE __m128i _mm_setr_epi8(signed char b0,
                                   signed char b1,
                                   signed char b2,
                                   signed char b3,
                                   signed char b4,
                                   signed char b5,
                                   signed char b6,
                                   signed char b7,
                                   signed char b8,
                                   signed char b9,
                                   signed char b10,
                                   signed char b11,
                                   signed char b12,
                                   signed char b13,
                                   signed char b14,
                                   signed char b15)
{
    int8_t ALIGN_STRUCT(16)
        data[16] = {(int8_t) b0,  (int8_t) b1,  (int8_t) b2,  (int8_t) b3,
                    (int8_t) b4,  (int8_t) b5,  (int8_t) b6,  (int8_t) b7,
                    (int8_t) b8,  (int8_t) b9,  (int8_t) b10, (int8_t) b11,
                    (int8_t) b12, (int8_t) b13, (int8_t) b14, (int8_t) b15};
    return (__m128i) vld1q_s8(data);
}

// Set packed double-precision (64-bit) floating-point elements in dst with the
// supplied values in reverse order.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_setr_pd
FORCE_INLINE __m128d _mm_setr_pd(double e1, double e0)
{
    return _mm_set_pd(e0, e1);
}

// Return vector of type __m128d with all elements set to zero.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_setzero_pd
FORCE_INLINE __m128d _mm_setzero_pd(void)
{
#if defined(__aarch64__)
    return vreinterpretq_m128d_f64(vdupq_n_f64(0));
#else
    return vreinterpretq_m128d_f32(vdupq_n_f32(0));
#endif
}

// Sets the 128-bit value to zero
// https://msdn.microsoft.com/en-us/library/vstudio/ys7dw0kh(v=vs.100).aspx
FORCE_INLINE __m128i _mm_setzero_si128(void)
{
    return vreinterpretq_m128i_s32(vdupq_n_s32(0));
}

// Shuffles the 4 signed or unsigned 32-bit integers in a as specified by imm.
// https://msdn.microsoft.com/en-us/library/56f67xbk%28v=vs.90%29.aspx
// FORCE_INLINE __m128i _mm_shuffle_epi32(__m128i a,
//                                        __constrange(0,255) int imm)
#ifdef _sse2neon_shuffle
#define _mm_shuffle_epi32(a, imm)                                            \
    __extension__({                                                          \
        int32x4_t _input = vreinterpretq_s32_m128i(a);                       \
        int32x4_t _shuf =                                                    \
            vshuffleq_s32(_input, _input, (imm) & (0x3), ((imm) >> 2) & 0x3, \
                          ((imm) >> 4) & 0x3, ((imm) >> 6) & 0x3);           \
        vreinterpretq_m128i_s32(_shuf);                                      \
    })
#else  // generic
#define _mm_shuffle_epi32(a, imm)                        \
    __extension__({                                      \
        __m128i ret;                                     \
        switch (imm) {                                   \
        case _MM_SHUFFLE(1, 0, 3, 2):                    \
            ret = _mm_shuffle_epi_1032((a));             \
            break;                                       \
        case _MM_SHUFFLE(2, 3, 0, 1):                    \
            ret = _mm_shuffle_epi_2301((a));             \
            break;                                       \
        case _MM_SHUFFLE(0, 3, 2, 1):                    \
            ret = _mm_shuffle_epi_0321((a));             \
            break;                                       \
        case _MM_SHUFFLE(2, 1, 0, 3):                    \
            ret = _mm_shuffle_epi_2103((a));             \
            break;                                       \
        case _MM_SHUFFLE(1, 0, 1, 0):                    \
            ret = _mm_shuffle_epi_1010((a));             \
            break;                                       \
        case _MM_SHUFFLE(1, 0, 0, 1):                    \
            ret = _mm_shuffle_epi_1001((a));             \
            break;                                       \
        case _MM_SHUFFLE(0, 1, 0, 1):                    \
            ret = _mm_shuffle_epi_0101((a));             \
            break;                                       \
        case _MM_SHUFFLE(2, 2, 1, 1):                    \
            ret = _mm_shuffle_epi_2211((a));             \
            break;                                       \
        case _MM_SHUFFLE(0, 1, 2, 2):                    \
            ret = _mm_shuffle_epi_0122((a));             \
            break;                                       \
        case _MM_SHUFFLE(3, 3, 3, 2):                    \
            ret = _mm_shuffle_epi_3332((a));             \
            break;                                       \
        case _MM_SHUFFLE(0, 0, 0, 0):                    \
            ret = _mm_shuffle_epi32_splat((a), 0);       \
            break;                                       \
        case _MM_SHUFFLE(1, 1, 1, 1):                    \
            ret = _mm_shuffle_epi32_splat((a), 1);       \
            break;                                       \
        case _MM_SHUFFLE(2, 2, 2, 2):                    \
            ret = _mm_shuffle_epi32_splat((a), 2);       \
            break;                                       \
        case _MM_SHUFFLE(3, 3, 3, 3):                    \
            ret = _mm_shuffle_epi32_splat((a), 3);       \
            break;                                       \
        default:                                         \
            ret = _mm_shuffle_epi32_default((a), (imm)); \
            break;                                       \
        }                                                \
        ret;                                             \
    })
#endif

// Shuffle double-precision (64-bit) floating-point elements using the control
// in imm8, and store the results in dst.
//
//   dst[63:0] := (imm8[0] == 0) ? a[63:0] : a[127:64]
//   dst[127:64] := (imm8[1] == 0) ? b[63:0] : b[127:64]
//
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_shuffle_pd
#ifdef _sse2neon_shuffle
#define _mm_shuffle_pd(a, b, imm8)                                            \
    vreinterpretq_m128d_s64(                                                  \
        vshuffleq_s64(vreinterpretq_s64_m128d(a), vreinterpretq_s64_m128d(b), \
                      imm8 & 0x1, ((imm8 & 0x2) >> 1) + 2))
#else
#define _mm_shuffle_pd(a, b, imm8)                                     \
    _mm_castsi128_pd(_mm_set_epi64x(                                   \
        vgetq_lane_s64(vreinterpretq_s64_m128d(b), (imm8 & 0x2) >> 1), \
        vgetq_lane_s64(vreinterpretq_s64_m128d(a), imm8 & 0x1)))
#endif

// FORCE_INLINE __m128i _mm_shufflehi_epi16(__m128i a,
//                                          __constrange(0,255) int imm)
#ifdef _sse2neon_shuffle
#define _mm_shufflehi_epi16(a, imm)                                           \
    __extension__({                                                           \
        int16x8_t _input = vreinterpretq_s16_m128i(a);                        \
        int16x8_t _shuf =                                                     \
            vshuffleq_s16(_input, _input, 0, 1, 2, 3, ((imm) & (0x3)) + 4,    \
                          (((imm) >> 2) & 0x3) + 4, (((imm) >> 4) & 0x3) + 4, \
                          (((imm) >> 6) & 0x3) + 4);                          \
        vreinterpretq_m128i_s16(_shuf);                                       \
    })
#else  // generic
#define _mm_shufflehi_epi16(a, imm) _mm_shufflehi_epi16_function((a), (imm))
#endif

// FORCE_INLINE __m128i _mm_shufflelo_epi16(__m128i a,
//                                          __constrange(0,255) int imm)
#ifdef _sse2neon_shuffle
#define _mm_shufflelo_epi16(a, imm)                                  \
    __extension__({                                                  \
        int16x8_t _input = vreinterpretq_s16_m128i(a);               \
        int16x8_t _shuf = vshuffleq_s16(                             \
            _input, _input, ((imm) & (0x3)), (((imm) >> 2) & 0x3),   \
            (((imm) >> 4) & 0x3), (((imm) >> 6) & 0x3), 4, 5, 6, 7); \
        vreinterpretq_m128i_s16(_shuf);                              \
    })
#else  // generic
#define _mm_shufflelo_epi16(a, imm) _mm_shufflelo_epi16_function((a), (imm))
#endif

// Shift packed 16-bit integers in a left by count while shifting in zeros, and
// store the results in dst.
//
//   FOR j := 0 to 7
//     i := j*16
//     IF count[63:0] > 15
//       dst[i+15:i] := 0
//     ELSE
//       dst[i+15:i] := ZeroExtend16(a[i+15:i] << count[63:0])
//     FI
//   ENDFOR
//
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_sll_epi16
FORCE_INLINE __m128i _mm_sll_epi16(__m128i a, __m128i count)
{
    uint64_t c = vreinterpretq_nth_u64_m128i(count, 0);
    if (_sse2neon_unlikely(c & ~15))
        return _mm_setzero_si128();

    int16x8_t vc = vdupq_n_s16((int16_t) c);
    return vreinterpretq_m128i_s16(vshlq_s16(vreinterpretq_s16_m128i(a), vc));
}

// Shift packed 32-bit integers in a left by count while shifting in zeros, and
// store the results in dst.
//
//   FOR j := 0 to 3
//     i := j*32
//     IF count[63:0] > 31
//       dst[i+31:i] := 0
//     ELSE
//       dst[i+31:i] := ZeroExtend32(a[i+31:i] << count[63:0])
//     FI
//   ENDFOR
//
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_sll_epi32
FORCE_INLINE __m128i _mm_sll_epi32(__m128i a, __m128i count)
{
    uint64_t c = vreinterpretq_nth_u64_m128i(count, 0);
    if (_sse2neon_unlikely(c & ~31))
        return _mm_setzero_si128();

    int32x4_t vc = vdupq_n_s32((int32_t) c);
    return vreinterpretq_m128i_s32(vshlq_s32(vreinterpretq_s32_m128i(a), vc));
}

// Shift packed 64-bit integers in a left by count while shifting in zeros, and
// store the results in dst.
//
//   FOR j := 0 to 1
//     i := j*64
//     IF count[63:0] > 63
//       dst[i+63:i] := 0
//     ELSE
//       dst[i+63:i] := ZeroExtend64(a[i+63:i] << count[63:0])
//     FI
//   ENDFOR
//
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_sll_epi64
FORCE_INLINE __m128i _mm_sll_epi64(__m128i a, __m128i count)
{
    uint64_t c = vreinterpretq_nth_u64_m128i(count, 0);
    if (_sse2neon_unlikely(c & ~63))
        return _mm_setzero_si128();

    int64x2_t vc = vdupq_n_s64((int64_t) c);
    return vreinterpretq_m128i_s64(vshlq_s64(vreinterpretq_s64_m128i(a), vc));
}

// Shift packed 16-bit integers in a left by imm8 while shifting in zeros, and
// store the results in dst.
//
//   FOR j := 0 to 7
//     i := j*16
//     IF imm8[7:0] > 15
//       dst[i+15:i] := 0
//     ELSE
//       dst[i+15:i] := ZeroExtend16(a[i+15:i] << imm8[7:0])
//     FI
//   ENDFOR
//
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_slli_epi16
FORCE_INLINE __m128i _mm_slli_epi16(__m128i a, int imm)
{
    if (_sse2neon_unlikely(imm & ~15))
        return _mm_setzero_si128();
    return vreinterpretq_m128i_s16(
        vshlq_s16(vreinterpretq_s16_m128i(a), vdupq_n_s16(imm)));
}

// Shift packed 32-bit integers in a left by imm8 while shifting in zeros, and
// store the results in dst.
//
//   FOR j := 0 to 3
//     i := j*32
//     IF imm8[7:0] > 31
//       dst[i+31:i] := 0
//     ELSE
//       dst[i+31:i] := ZeroExtend32(a[i+31:i] << imm8[7:0])
//     FI
//   ENDFOR
//
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_slli_epi32
FORCE_INLINE __m128i _mm_slli_epi32(__m128i a, int imm)
{
    if (_sse2neon_unlikely(imm & ~31))
        return _mm_setzero_si128();
    return vreinterpretq_m128i_s32(
        vshlq_s32(vreinterpretq_s32_m128i(a), vdupq_n_s32(imm)));
}

// Shift packed 64-bit integers in a left by imm8 while shifting in zeros, and
// store the results in dst.
//
//   FOR j := 0 to 1
//     i := j*64
//     IF imm8[7:0] > 63
//       dst[i+63:i] := 0
//     ELSE
//       dst[i+63:i] := ZeroExtend64(a[i+63:i] << imm8[7:0])
//     FI
//   ENDFOR
//
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_slli_epi64
FORCE_INLINE __m128i _mm_slli_epi64(__m128i a, int imm)
{
    if (_sse2neon_unlikely(imm & ~63))
        return _mm_setzero_si128();
    return vreinterpretq_m128i_s64(
        vshlq_s64(vreinterpretq_s64_m128i(a), vdupq_n_s64(imm)));
}

// Shift a left by imm8 bytes while shifting in zeros, and store the results in
// dst.
//
//   tmp := imm8[7:0]
//   IF tmp > 15
//     tmp := 16
//   FI
//   dst[127:0] := a[127:0] << (tmp*8)
//
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_slli_si128
#define _mm_slli_si128(a, imm)                                         \
    __extension__({                                                    \
        int8x16_t ret;                                                 \
        if (_sse2neon_unlikely(imm == 0))                              \
            ret = vreinterpretq_s8_m128i(a);                           \
        else if (_sse2neon_unlikely((imm) & ~15))                      \
            ret = vdupq_n_s8(0);                                       \
        else                                                           \
            ret = vextq_s8(vdupq_n_s8(0), vreinterpretq_s8_m128i(a),   \
                           ((imm <= 0 || imm > 15) ? 0 : (16 - imm))); \
        vreinterpretq_m128i_s8(ret);                                   \
    })

// Compute the square root of packed double-precision (64-bit) floating-point
// elements in a, and store the results in dst.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_sqrt_pd
FORCE_INLINE __m128d _mm_sqrt_pd(__m128d a)
{
#if defined(__aarch64__)
    return vreinterpretq_m128d_f64(vsqrtq_f64(vreinterpretq_f64_m128d(a)));
#else
    double a0 = sqrt(((double *) &a)[0]);
    double a1 = sqrt(((double *) &a)[1]);
    return _mm_set_pd(a1, a0);
#endif
}

// Compute the square root of the lower double-precision (64-bit) floating-point
// element in b, store the result in the lower element of dst, and copy the
// upper element from a to the upper element of dst.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_sqrt_sd
FORCE_INLINE __m128d _mm_sqrt_sd(__m128d a, __m128d b)
{
#if defined(__aarch64__)
    return _mm_move_sd(a, _mm_sqrt_pd(b));
#else
    return _mm_set_pd(((double *) &a)[1], sqrt(((double *) &b)[0]));
#endif
}

// Shift packed 16-bit integers in a right by count while shifting in sign bits,
// and store the results in dst.
//
//   FOR j := 0 to 7
//     i := j*16
//     IF count[63:0] > 15
//       dst[i+15:i] := (a[i+15] ? 0xFFFF : 0x0)
//     ELSE
//       dst[i+15:i] := SignExtend16(a[i+15:i] >> count[63:0])
//     FI
//  ENDFOR
//
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_sra_epi16
FORCE_INLINE __m128i _mm_sra_epi16(__m128i a, __m128i count)
{
    int64_t c = (int64_t) vget_low_s64((int64x2_t) count);
    if (_sse2neon_unlikely(c & ~15))
        return _mm_cmplt_epi16(a, _mm_setzero_si128());
    return vreinterpretq_m128i_s16(vshlq_s16((int16x8_t) a, vdupq_n_s16(-c)));
}

// Shift packed 32-bit integers in a right by count while shifting in sign bits,
// and store the results in dst.
//
//   FOR j := 0 to 3
//     i := j*32
//     IF count[63:0] > 31
//       dst[i+31:i] := (a[i+31] ? 0xFFFFFFFF : 0x0)
//     ELSE
//       dst[i+31:i] := SignExtend32(a[i+31:i] >> count[63:0])
//     FI
//  ENDFOR
//
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_sra_epi32
FORCE_INLINE __m128i _mm_sra_epi32(__m128i a, __m128i count)
{
    int64_t c = (int64_t) vget_low_s64((int64x2_t) count);
    if (_sse2neon_unlikely(c & ~31))
        return _mm_cmplt_epi32(a, _mm_setzero_si128());
    return vreinterpretq_m128i_s32(vshlq_s32((int32x4_t) a, vdupq_n_s32(-c)));
}

// Shift packed 16-bit integers in a right by imm8 while shifting in sign
// bits, and store the results in dst.
//
//   FOR j := 0 to 7
//     i := j*16
//     IF imm8[7:0] > 15
//       dst[i+15:i] := (a[i+15] ? 0xFFFF : 0x0)
//     ELSE
//       dst[i+15:i] := SignExtend16(a[i+15:i] >> imm8[7:0])
//     FI
//   ENDFOR
//
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_srai_epi16
FORCE_INLINE __m128i _mm_srai_epi16(__m128i a, int imm)
{
    const int count = (imm & ~15) ? 15 : imm;
    return (__m128i) vshlq_s16((int16x8_t) a, vdupq_n_s16(-count));
}

// Shift packed 32-bit integers in a right by imm8 while shifting in sign bits,
// and store the results in dst.
//
//   FOR j := 0 to 3
//     i := j*32
//     IF imm8[7:0] > 31
//       dst[i+31:i] := (a[i+31] ? 0xFFFFFFFF : 0x0)
//     ELSE
//       dst[i+31:i] := SignExtend32(a[i+31:i] >> imm8[7:0])
//     FI
//   ENDFOR
//
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_srai_epi32
// FORCE_INLINE __m128i _mm_srai_epi32(__m128i a, __constrange(0,255) int imm)
#define _mm_srai_epi32(a, imm)                                               \
    __extension__({                                                          \
        __m128i ret;                                                         \
        if (_sse2neon_unlikely((imm) == 0)) {                                \
            ret = a;                                                         \
        } else if (_sse2neon_likely(0 < (imm) && (imm) < 32)) {              \
            ret = vreinterpretq_m128i_s32(                                   \
                vshlq_s32(vreinterpretq_s32_m128i(a), vdupq_n_s32(-(imm)))); \
        } else {                                                             \
            ret = vreinterpretq_m128i_s32(                                   \
                vshrq_n_s32(vreinterpretq_s32_m128i(a), 31));                \
        }                                                                    \
        ret;                                                                 \
    })

// Shift packed 16-bit integers in a right by count while shifting in zeros, and
// store the results in dst.
//
//   FOR j := 0 to 7
//     i := j*16
//     IF count[63:0] > 15
//       dst[i+15:i] := 0
//     ELSE
//       dst[i+15:i] := ZeroExtend16(a[i+15:i] >> count[63:0])
//     FI
//   ENDFOR
//
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_srl_epi16
FORCE_INLINE __m128i _mm_srl_epi16(__m128i a, __m128i count)
{
    uint64_t c = vreinterpretq_nth_u64_m128i(count, 0);
    if (_sse2neon_unlikely(c & ~15))
        return _mm_setzero_si128();

    int16x8_t vc = vdupq_n_s16(-(int16_t) c);
    return vreinterpretq_m128i_u16(vshlq_u16(vreinterpretq_u16_m128i(a), vc));
}

// Shift packed 32-bit integers in a right by count while shifting in zeros, and
// store the results in dst.
//
//   FOR j := 0 to 3
//     i := j*32
//     IF count[63:0] > 31
//       dst[i+31:i] := 0
//     ELSE
//       dst[i+31:i] := ZeroExtend32(a[i+31:i] >> count[63:0])
//     FI
//   ENDFOR
//
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_srl_epi32
FORCE_INLINE __m128i _mm_srl_epi32(__m128i a, __m128i count)
{
    uint64_t c = vreinterpretq_nth_u64_m128i(count, 0);
    if (_sse2neon_unlikely(c & ~31))
        return _mm_setzero_si128();

    int32x4_t vc = vdupq_n_s32(-(int32_t) c);
    return vreinterpretq_m128i_u32(vshlq_u32(vreinterpretq_u32_m128i(a), vc));
}

// Shift packed 64-bit integers in a right by count while shifting in zeros, and
// store the results in dst.
//
//   FOR j := 0 to 1
//     i := j*64
//     IF count[63:0] > 63
//       dst[i+63:i] := 0
//     ELSE
//       dst[i+63:i] := ZeroExtend64(a[i+63:i] >> count[63:0])
//     FI
//   ENDFOR
//
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_srl_epi64
FORCE_INLINE __m128i _mm_srl_epi64(__m128i a, __m128i count)
{
    uint64_t c = vreinterpretq_nth_u64_m128i(count, 0);
    if (_sse2neon_unlikely(c & ~63))
        return _mm_setzero_si128();

    int64x2_t vc = vdupq_n_s64(-(int64_t) c);
    return vreinterpretq_m128i_u64(vshlq_u64(vreinterpretq_u64_m128i(a), vc));
}

// Shift packed 16-bit integers in a right by imm8 while shifting in zeros, and
// store the results in dst.
//
//   FOR j := 0 to 7
//     i := j*16
//     IF imm8[7:0] > 15
//       dst[i+15:i] := 0
//     ELSE
//       dst[i+15:i] := ZeroExtend16(a[i+15:i] >> imm8[7:0])
//     FI
//   ENDFOR
//
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_srli_epi16
#define _mm_srli_epi16(a, imm)                                               \
    __extension__({                                                          \
        __m128i ret;                                                         \
        if (_sse2neon_unlikely((imm) & ~15)) {                               \
            ret = _mm_setzero_si128();                                       \
        } else {                                                             \
            ret = vreinterpretq_m128i_u16(                                   \
                vshlq_u16(vreinterpretq_u16_m128i(a), vdupq_n_s16(-(imm)))); \
        }                                                                    \
        ret;                                                                 \
    })

// Shift packed 32-bit integers in a right by imm8 while shifting in zeros, and
// store the results in dst.
//
//   FOR j := 0 to 3
//     i := j*32
//     IF imm8[7:0] > 31
//       dst[i+31:i] := 0
//     ELSE
//       dst[i+31:i] := ZeroExtend32(a[i+31:i] >> imm8[7:0])
//     FI
//   ENDFOR
//
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_srli_epi32
// FORCE_INLINE __m128i _mm_srli_epi32(__m128i a, __constrange(0,255) int imm)
#define _mm_srli_epi32(a, imm)                                               \
    __extension__({                                                          \
        __m128i ret;                                                         \
        if (_sse2neon_unlikely((imm) & ~31)) {                               \
            ret = _mm_setzero_si128();                                       \
        } else {                                                             \
            ret = vreinterpretq_m128i_u32(                                   \
                vshlq_u32(vreinterpretq_u32_m128i(a), vdupq_n_s32(-(imm)))); \
        }                                                                    \
        ret;                                                                 \
    })

// Shift packed 64-bit integers in a right by imm8 while shifting in zeros, and
// store the results in dst.
//
//   FOR j := 0 to 1
//     i := j*64
//     IF imm8[7:0] > 63
//       dst[i+63:i] := 0
//     ELSE
//       dst[i+63:i] := ZeroExtend64(a[i+63:i] >> imm8[7:0])
//     FI
//   ENDFOR
//
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_srli_epi64
#define _mm_srli_epi64(a, imm)                                               \
    __extension__({                                                          \
        __m128i ret;                                                         \
        if (_sse2neon_unlikely((imm) & ~63)) {                               \
            ret = _mm_setzero_si128();                                       \
        } else {                                                             \
            ret = vreinterpretq_m128i_u64(                                   \
                vshlq_u64(vreinterpretq_u64_m128i(a), vdupq_n_s64(-(imm)))); \
        }                                                                    \
        ret;                                                                 \
    })

// Shift a right by imm8 bytes while shifting in zeros, and store the results in
// dst.
//
//   tmp := imm8[7:0]
//   IF tmp > 15
//     tmp := 16
//   FI
//   dst[127:0] := a[127:0] >> (tmp*8)
//
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_srli_si128
#define _mm_srli_si128(a, imm)                                       \
    __extension__({                                                  \
        int8x16_t ret;                                               \
        if (_sse2neon_unlikely((imm) & ~15))                         \
            ret = vdupq_n_s8(0);                                     \
        else                                                         \
            ret = vextq_s8(vreinterpretq_s8_m128i(a), vdupq_n_s8(0), \
                           (imm > 15 ? 0 : imm));                    \
        vreinterpretq_m128i_s8(ret);                                 \
    })

// Store 128-bits (composed of 2 packed double-precision (64-bit) floating-point
// elements) from a into memory. mem_addr must be aligned on a 16-byte boundary
// or a general-protection exception may be generated.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_store_pd
FORCE_INLINE void _mm_store_pd(double *mem_addr, __m128d a)
{
#if defined(__aarch64__)
    vst1q_f64((float64_t *) mem_addr, vreinterpretq_f64_m128d(a));
#else
    vst1q_f32((float32_t *) mem_addr, vreinterpretq_f32_m128d(a));
#endif
}

// Store the lower double-precision (64-bit) floating-point element from a into
// 2 contiguous elements in memory. mem_addr must be aligned on a 16-byte
// boundary or a general-protection exception may be generated.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_store_pd1
FORCE_INLINE void _mm_store_pd1(double *mem_addr, __m128d a)
{
#if defined(__aarch64__)
    float64x1_t a_low = vget_low_f64(vreinterpretq_f64_m128d(a));
    vst1q_f64((float64_t *) mem_addr,
              vreinterpretq_f64_m128d(vcombine_f64(a_low, a_low)));
#else
    float32x2_t a_low = vget_low_f32(vreinterpretq_f32_m128d(a));
    vst1q_f32((float32_t *) mem_addr,
              vreinterpretq_f32_m128d(vcombine_f32(a_low, a_low)));
#endif
}

// Store the lower double-precision (64-bit) floating-point element from a into
// memory. mem_addr does not need to be aligned on any particular boundary.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=mm_store_sd
FORCE_INLINE void _mm_store_sd(double *mem_addr, __m128d a)
{
#if defined(__aarch64__)
    vst1_f64((float64_t *) mem_addr, vget_low_f64(vreinterpretq_f64_m128d(a)));
#else
    vst1_u64((uint64_t *) mem_addr, vget_low_u64(vreinterpretq_u64_m128d(a)));
#endif
}

// Stores four 32-bit integer values as (as a __m128i value) at the address p.
// https://msdn.microsoft.com/en-us/library/vstudio/edk11s13(v=vs.100).aspx
FORCE_INLINE void _mm_store_si128(__m128i *p, __m128i a)
{
    vst1q_s32((int32_t *) p, vreinterpretq_s32_m128i(a));
}

// Store the lower double-precision (64-bit) floating-point element from a into
// 2 contiguous elements in memory. mem_addr must be aligned on a 16-byte
// boundary or a general-protection exception may be generated.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#expand=9,526,5601&text=_mm_store1_pd
#define _mm_store1_pd _mm_store_pd1

// Store the upper double-precision (64-bit) floating-point element from a into
// memory.
//
//   MEM[mem_addr+63:mem_addr] := a[127:64]
//
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_storeh_pd
FORCE_INLINE void _mm_storeh_pd(double *mem_addr, __m128d a)
{
#if defined(__aarch64__)
    vst1_f64((float64_t *) mem_addr, vget_high_f64(vreinterpretq_f64_m128d(a)));
#else
    vst1_f32((float32_t *) mem_addr, vget_high_f32(vreinterpretq_f32_m128d(a)));
#endif
}

// Reads the lower 64 bits of b and stores them into the lower 64 bits of a.
// https://msdn.microsoft.com/en-us/library/hhwf428f%28v=vs.90%29.aspx
FORCE_INLINE void _mm_storel_epi64(__m128i *a, __m128i b)
{
    vst1_u64((uint64_t *) a, vget_low_u64(vreinterpretq_u64_m128i(b)));
}

// Store the lower double-precision (64-bit) floating-point element from a into
// memory.
//
//   MEM[mem_addr+63:mem_addr] := a[63:0]
//
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_storel_pd
FORCE_INLINE void _mm_storel_pd(double *mem_addr, __m128d a)
{
#if defined(__aarch64__)
    vst1_f64((float64_t *) mem_addr, vget_low_f64(vreinterpretq_f64_m128d(a)));
#else
    vst1_f32((float32_t *) mem_addr, vget_low_f32(vreinterpretq_f32_m128d(a)));
#endif
}

// Store 2 double-precision (64-bit) floating-point elements from a into memory
// in reverse order. mem_addr must be aligned on a 16-byte boundary or a
// general-protection exception may be generated.
//
//   MEM[mem_addr+63:mem_addr] := a[127:64]
//   MEM[mem_addr+127:mem_addr+64] := a[63:0]
//
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_storer_pd
FORCE_INLINE void _mm_storer_pd(double *mem_addr, __m128d a)
{
    float32x4_t f = vreinterpretq_f32_m128d(a);
    _mm_store_pd(mem_addr, vreinterpretq_m128d_f32(vextq_f32(f, f, 2)));
}

// Store 128-bits (composed of 2 packed double-precision (64-bit) floating-point
// elements) from a into memory. mem_addr does not need to be aligned on any
// particular boundary.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_storeu_pd
FORCE_INLINE void _mm_storeu_pd(double *mem_addr, __m128d a)
{
    _mm_store_pd(mem_addr, a);
}

// Stores 128-bits of integer data a at the address p.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_storeu_si128
FORCE_INLINE void _mm_storeu_si128(__m128i *p, __m128i a)
{
    vst1q_s32((int32_t *) p, vreinterpretq_s32_m128i(a));
}

// Stores 32-bits of integer data a at the address p.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_storeu_si32
FORCE_INLINE void _mm_storeu_si32(void *p, __m128i a)
{
    vst1q_lane_s32((int32_t *) p, vreinterpretq_s32_m128i(a), 0);
}

// Store 128-bits (composed of 2 packed double-precision (64-bit) floating-point
// elements) from a into memory using a non-temporal memory hint. mem_addr must
// be aligned on a 16-byte boundary or a general-protection exception may be
// generated.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_stream_pd
FORCE_INLINE void _mm_stream_pd(double *p, __m128d a)
{
#if __has_builtin(__builtin_nontemporal_store)
    __builtin_nontemporal_store(reinterpret_cast<float32x4_t>(a), (float32x4_t *) p);
#elif defined(__aarch64__)
    vst1q_f64(p, vreinterpretq_f64_m128d(a));
#else
    vst1q_s64((int64_t *) p, vreinterpretq_s64_m128d(a));
#endif
}

// Stores the data in a to the address p without polluting the caches.  If the
// cache line containing address p is already in the cache, the cache will be
// updated.
// https://msdn.microsoft.com/en-us/library/ba08y07y%28v=vs.90%29.aspx
FORCE_INLINE void _mm_stream_si128(__m128i *p, __m128i a)
{
#if __has_builtin(__builtin_nontemporal_store)
    __builtin_nontemporal_store(a, p);
#else
    vst1q_s64((int64_t *) p, vreinterpretq_s64_m128i(a));
#endif
}

// Store 32-bit integer a into memory using a non-temporal hint to minimize
// cache pollution. If the cache line containing address mem_addr is already in
// the cache, the cache will be updated.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_stream_si32
FORCE_INLINE void _mm_stream_si32(int *p, int a)
{
    vst1q_lane_s32((int32_t *) p, vdupq_n_s32(a), 0);
}

// Store 64-bit integer a into memory using a non-temporal hint to minimize
// cache pollution. If the cache line containing address mem_addr is already in
// the cache, the cache will be updated.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_stream_si64
FORCE_INLINE void _mm_stream_si64(__int64 *p, __int64 a)
{
    vst1_s64((int64_t *) p, vdup_n_s64((int64_t) a));
}

// Subtract packed 16-bit integers in b from packed 16-bit integers in a, and
// store the results in dst.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_sub_epi16
FORCE_INLINE __m128i _mm_sub_epi16(__m128i a, __m128i b)
{
    return vreinterpretq_m128i_s16(
        vsubq_s16(vreinterpretq_s16_m128i(a), vreinterpretq_s16_m128i(b)));
}

// Subtracts the 4 signed or unsigned 32-bit integers of b from the 4 signed or
// unsigned 32-bit integers of a.
//
//   r0 := a0 - b0
//   r1 := a1 - b1
//   r2 := a2 - b2
//   r3 := a3 - b3
//
// https://msdn.microsoft.com/en-us/library/vstudio/fhh866h0(v=vs.100).aspx
FORCE_INLINE __m128i _mm_sub_epi32(__m128i a, __m128i b)
{
    return vreinterpretq_m128i_s32(
        vsubq_s32(vreinterpretq_s32_m128i(a), vreinterpretq_s32_m128i(b)));
}

// Subtract 2 packed 64-bit integers in b from 2 packed 64-bit integers in a,
// and store the results in dst.
//    r0 := a0 - b0
//    r1 := a1 - b1
FORCE_INLINE __m128i _mm_sub_epi64(__m128i a, __m128i b)
{
    return vreinterpretq_m128i_s64(
        vsubq_s64(vreinterpretq_s64_m128i(a), vreinterpretq_s64_m128i(b)));
}

// Subtract packed 8-bit integers in b from packed 8-bit integers in a, and
// store the results in dst.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_sub_epi8
FORCE_INLINE __m128i _mm_sub_epi8(__m128i a, __m128i b)
{
    return vreinterpretq_m128i_s8(
        vsubq_s8(vreinterpretq_s8_m128i(a), vreinterpretq_s8_m128i(b)));
}

// Subtract packed double-precision (64-bit) floating-point elements in b from
// packed double-precision (64-bit) floating-point elements in a, and store the
// results in dst.
//
//   FOR j := 0 to 1
//     i := j*64
//     dst[i+63:i] := a[i+63:i] - b[i+63:i]
//   ENDFOR
//
//  https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=mm_sub_pd
FORCE_INLINE __m128d _mm_sub_pd(__m128d a, __m128d b)
{
#if defined(__aarch64__)
    return vreinterpretq_m128d_f64(
        vsubq_f64(vreinterpretq_f64_m128d(a), vreinterpretq_f64_m128d(b)));
#else
    double *da = (double *) &a;
    double *db = (double *) &b;
    double c[2];
    c[0] = da[0] - db[0];
    c[1] = da[1] - db[1];
    return vld1q_f32((float32_t *) c);
#endif
}

// Subtract the lower double-precision (64-bit) floating-point element in b from
// the lower double-precision (64-bit) floating-point element in a, store the
// result in the lower element of dst, and copy the upper element from a to the
// upper element of dst.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_sub_sd
FORCE_INLINE __m128d _mm_sub_sd(__m128d a, __m128d b)
{
    return _mm_move_sd(a, _mm_sub_pd(a, b));
}

// Subtract 64-bit integer b from 64-bit integer a, and store the result in dst.
//
//   dst[63:0] := a[63:0] - b[63:0]
//
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_sub_si64
FORCE_INLINE __m64 _mm_sub_si64(__m64 a, __m64 b)
{
    return vreinterpret_m64_s64(
        vsub_s64(vreinterpret_s64_m64(a), vreinterpret_s64_m64(b)));
}

// Subtracts the 8 signed 16-bit integers of b from the 8 signed 16-bit integers
// of a and saturates.
//
//   r0 := SignedSaturate(a0 - b0)
//   r1 := SignedSaturate(a1 - b1)
//   ...
//   r7 := SignedSaturate(a7 - b7)
//
// https://technet.microsoft.com/en-us/subscriptions/3247z5b8(v=vs.90)
FORCE_INLINE __m128i _mm_subs_epi16(__m128i a, __m128i b)
{
    return vreinterpretq_m128i_s16(
        vqsubq_s16(vreinterpretq_s16_m128i(a), vreinterpretq_s16_m128i(b)));
}

// Subtracts the 16 signed 8-bit integers of b from the 16 signed 8-bit integers
// of a and saturates.
//
//   r0 := SignedSaturate(a0 - b0)
//   r1 := SignedSaturate(a1 - b1)
//   ...
//   r15 := SignedSaturate(a15 - b15)
//
// https://technet.microsoft.com/en-us/subscriptions/by7kzks1(v=vs.90)
FORCE_INLINE __m128i _mm_subs_epi8(__m128i a, __m128i b)
{
    return vreinterpretq_m128i_s8(
        vqsubq_s8(vreinterpretq_s8_m128i(a), vreinterpretq_s8_m128i(b)));
}

// Subtracts the 8 unsigned 16-bit integers of bfrom the 8 unsigned 16-bit
// integers of a and saturates..
// https://technet.microsoft.com/en-us/subscriptions/index/f44y0s19(v=vs.90).aspx
FORCE_INLINE __m128i _mm_subs_epu16(__m128i a, __m128i b)
{
    return vreinterpretq_m128i_u16(
        vqsubq_u16(vreinterpretq_u16_m128i(a), vreinterpretq_u16_m128i(b)));
}

// Subtracts the 16 unsigned 8-bit integers of b from the 16 unsigned 8-bit
// integers of a and saturates.
//
//   r0 := UnsignedSaturate(a0 - b0)
//   r1 := UnsignedSaturate(a1 - b1)
//   ...
//   r15 := UnsignedSaturate(a15 - b15)
//
// https://technet.microsoft.com/en-us/subscriptions/yadkxc18(v=vs.90)
FORCE_INLINE __m128i _mm_subs_epu8(__m128i a, __m128i b)
{
    return vreinterpretq_m128i_u8(
        vqsubq_u8(vreinterpretq_u8_m128i(a), vreinterpretq_u8_m128i(b)));
}

#define _mm_ucomieq_sd _mm_comieq_sd
#define _mm_ucomige_sd _mm_comige_sd
#define _mm_ucomigt_sd _mm_comigt_sd
#define _mm_ucomile_sd _mm_comile_sd
#define _mm_ucomilt_sd _mm_comilt_sd
#define _mm_ucomineq_sd _mm_comineq_sd

// Return vector of type __m128d with undefined elements.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_undefined_pd
FORCE_INLINE __m128d _mm_undefined_pd(void)
{
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wuninitialized"
#endif
    __m128d a;
    return a;
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif
}

// Interleaves the upper 4 signed or unsigned 16-bit integers in a with the
// upper 4 signed or unsigned 16-bit integers in b.
//
//   r0 := a4
//   r1 := b4
//   r2 := a5
//   r3 := b5
//   r4 := a6
//   r5 := b6
//   r6 := a7
//   r7 := b7
//
// https://msdn.microsoft.com/en-us/library/03196cz7(v=vs.100).aspx
FORCE_INLINE __m128i _mm_unpackhi_epi16(__m128i a, __m128i b)
{
#if defined(__aarch64__)
    return vreinterpretq_m128i_s16(
        vzip2q_s16(vreinterpretq_s16_m128i(a), vreinterpretq_s16_m128i(b)));
#else
    int16x4_t a1 = vget_high_s16(vreinterpretq_s16_m128i(a));
    int16x4_t b1 = vget_high_s16(vreinterpretq_s16_m128i(b));
    int16x4x2_t result = vzip_s16(a1, b1);
    return vreinterpretq_m128i_s16(vcombine_s16(result.val[0], result.val[1]));
#endif
}

// Interleaves the upper 2 signed or unsigned 32-bit integers in a with the
// upper 2 signed or unsigned 32-bit integers in b.
// https://msdn.microsoft.com/en-us/library/65sa7cbs(v=vs.100).aspx
FORCE_INLINE __m128i _mm_unpackhi_epi32(__m128i a, __m128i b)
{
#if defined(__aarch64__)
    return vreinterpretq_m128i_s32(
        vzip2q_s32(vreinterpretq_s32_m128i(a), vreinterpretq_s32_m128i(b)));
#else
    int32x2_t a1 = vget_high_s32(vreinterpretq_s32_m128i(a));
    int32x2_t b1 = vget_high_s32(vreinterpretq_s32_m128i(b));
    int32x2x2_t result = vzip_s32(a1, b1);
    return vreinterpretq_m128i_s32(vcombine_s32(result.val[0], result.val[1]));
#endif
}

// Interleaves the upper signed or unsigned 64-bit integer in a with the
// upper signed or unsigned 64-bit integer in b.
//
//   r0 := a1
//   r1 := b1
FORCE_INLINE __m128i _mm_unpackhi_epi64(__m128i a, __m128i b)
{
    int64x1_t a_h = vget_high_s64(vreinterpretq_s64_m128i(a));
    int64x1_t b_h = vget_high_s64(vreinterpretq_s64_m128i(b));
    return vreinterpretq_m128i_s64(vcombine_s64(a_h, b_h));
}

// Interleaves the upper 8 signed or unsigned 8-bit integers in a with the upper
// 8 signed or unsigned 8-bit integers in b.
//
//   r0 := a8
//   r1 := b8
//   r2 := a9
//   r3 := b9
//   ...
//   r14 := a15
//   r15 := b15
//
// https://msdn.microsoft.com/en-us/library/t5h7783k(v=vs.100).aspx
FORCE_INLINE __m128i _mm_unpackhi_epi8(__m128i a, __m128i b)
{
#if defined(__aarch64__)
    return vreinterpretq_m128i_s8(
        vzip2q_s8(vreinterpretq_s8_m128i(a), vreinterpretq_s8_m128i(b)));
#else
    int8x8_t a1 =
        vreinterpret_s8_s16(vget_high_s16(vreinterpretq_s16_m128i(a)));
    int8x8_t b1 =
        vreinterpret_s8_s16(vget_high_s16(vreinterpretq_s16_m128i(b)));
    int8x8x2_t result = vzip_s8(a1, b1);
    return vreinterpretq_m128i_s8(vcombine_s8(result.val[0], result.val[1]));
#endif
}

// Unpack and interleave double-precision (64-bit) floating-point elements from
// the high half of a and b, and store the results in dst.
//
//   DEFINE INTERLEAVE_HIGH_QWORDS(src1[127:0], src2[127:0]) {
//     dst[63:0] := src1[127:64]
//     dst[127:64] := src2[127:64]
//     RETURN dst[127:0]
//   }
//   dst[127:0] := INTERLEAVE_HIGH_QWORDS(a[127:0], b[127:0])
//
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_unpackhi_pd
FORCE_INLINE __m128d _mm_unpackhi_pd(__m128d a, __m128d b)
{
#if defined(__aarch64__)
    return vreinterpretq_m128d_f64(
        vzip2q_f64(vreinterpretq_f64_m128d(a), vreinterpretq_f64_m128d(b)));
#else
    return vreinterpretq_m128d_s64(
        vcombine_s64(vget_high_s64(vreinterpretq_s64_m128d(a)),
                     vget_high_s64(vreinterpretq_s64_m128d(b))));
#endif
}

// Interleaves the lower 4 signed or unsigned 16-bit integers in a with the
// lower 4 signed or unsigned 16-bit integers in b.
//
//   r0 := a0
//   r1 := b0
//   r2 := a1
//   r3 := b1
//   r4 := a2
//   r5 := b2
//   r6 := a3
//   r7 := b3
//
// https://msdn.microsoft.com/en-us/library/btxb17bw%28v=vs.90%29.aspx
FORCE_INLINE __m128i _mm_unpacklo_epi16(__m128i a, __m128i b)
{
#if defined(__aarch64__)
    return vreinterpretq_m128i_s16(
        vzip1q_s16(vreinterpretq_s16_m128i(a), vreinterpretq_s16_m128i(b)));
#else
    int16x4_t a1 = vget_low_s16(vreinterpretq_s16_m128i(a));
    int16x4_t b1 = vget_low_s16(vreinterpretq_s16_m128i(b));
    int16x4x2_t result = vzip_s16(a1, b1);
    return vreinterpretq_m128i_s16(vcombine_s16(result.val[0], result.val[1]));
#endif
}

// Interleaves the lower 2 signed or unsigned 32 - bit integers in a with the
// lower 2 signed or unsigned 32 - bit integers in b.
//
//   r0 := a0
//   r1 := b0
//   r2 := a1
//   r3 := b1
//
// https://msdn.microsoft.com/en-us/library/x8atst9d(v=vs.100).aspx
FORCE_INLINE __m128i _mm_unpacklo_epi32(__m128i a, __m128i b)
{
#if defined(__aarch64__)
    return vreinterpretq_m128i_s32(
        vzip1q_s32(vreinterpretq_s32_m128i(a), vreinterpretq_s32_m128i(b)));
#else
    int32x2_t a1 = vget_low_s32(vreinterpretq_s32_m128i(a));
    int32x2_t b1 = vget_low_s32(vreinterpretq_s32_m128i(b));
    int32x2x2_t result = vzip_s32(a1, b1);
    return vreinterpretq_m128i_s32(vcombine_s32(result.val[0], result.val[1]));
#endif
}

FORCE_INLINE __m128i _mm_unpacklo_epi64(__m128i a, __m128i b)
{
    int64x1_t a_l = vget_low_s64(vreinterpretq_s64_m128i(a));
    int64x1_t b_l = vget_low_s64(vreinterpretq_s64_m128i(b));
    return vreinterpretq_m128i_s64(vcombine_s64(a_l, b_l));
}

// Interleaves the lower 8 signed or unsigned 8-bit integers in a with the lower
// 8 signed or unsigned 8-bit integers in b.
//
//   r0 := a0
//   r1 := b0
//   r2 := a1
//   r3 := b1
//   ...
//   r14 := a7
//   r15 := b7
//
// https://msdn.microsoft.com/en-us/library/xf7k860c%28v=vs.90%29.aspx
FORCE_INLINE __m128i _mm_unpacklo_epi8(__m128i a, __m128i b)
{
#if defined(__aarch64__)
    return vreinterpretq_m128i_s8(
        vzip1q_s8(vreinterpretq_s8_m128i(a), vreinterpretq_s8_m128i(b)));
#else
    int8x8_t a1 = vreinterpret_s8_s16(vget_low_s16(vreinterpretq_s16_m128i(a)));
    int8x8_t b1 = vreinterpret_s8_s16(vget_low_s16(vreinterpretq_s16_m128i(b)));
    int8x8x2_t result = vzip_s8(a1, b1);
    return vreinterpretq_m128i_s8(vcombine_s8(result.val[0], result.val[1]));
#endif
}

// Unpack and interleave double-precision (64-bit) floating-point elements from
// the low half of a and b, and store the results in dst.
//
//   DEFINE INTERLEAVE_QWORDS(src1[127:0], src2[127:0]) {
//     dst[63:0] := src1[63:0]
//     dst[127:64] := src2[63:0]
//     RETURN dst[127:0]
//   }
//   dst[127:0] := INTERLEAVE_QWORDS(a[127:0], b[127:0])
//
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_unpacklo_pd
FORCE_INLINE __m128d _mm_unpacklo_pd(__m128d a, __m128d b)
{
#if defined(__aarch64__)
    return vreinterpretq_m128d_f64(
        vzip1q_f64(vreinterpretq_f64_m128d(a), vreinterpretq_f64_m128d(b)));
#else
    return vreinterpretq_m128d_s64(
        vcombine_s64(vget_low_s64(vreinterpretq_s64_m128d(a)),
                     vget_low_s64(vreinterpretq_s64_m128d(b))));
#endif
}

// Compute the bitwise XOR of packed double-precision (64-bit) floating-point
// elements in a and b, and store the results in dst.
//
//   FOR j := 0 to 1
//      i := j*64
//      dst[i+63:i] := a[i+63:i] XOR b[i+63:i]
//   ENDFOR
//
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_xor_pd
FORCE_INLINE __m128d _mm_xor_pd(__m128d a, __m128d b)
{
    return vreinterpretq_m128d_s64(
        veorq_s64(vreinterpretq_s64_m128d(a), vreinterpretq_s64_m128d(b)));
}

// Computes the bitwise XOR of the 128-bit value in a and the 128-bit value in
// b.  https://msdn.microsoft.com/en-us/library/fzt08www(v=vs.100).aspx
FORCE_INLINE __m128i _mm_xor_si128(__m128i a, __m128i b)
{
    return vreinterpretq_m128i_s32(
        veorq_s32(vreinterpretq_s32_m128i(a), vreinterpretq_s32_m128i(b)));
}

/* SSE3 */

// Alternatively add and subtract packed double-precision (64-bit)
// floating-point elements in a to/from packed elements in b, and store the
// results in dst.
//
// FOR j := 0 to 1
//   i := j*64
//   IF ((j & 1) == 0)
//     dst[i+63:i] := a[i+63:i] - b[i+63:i]
//   ELSE
//     dst[i+63:i] := a[i+63:i] + b[i+63:i]
//   FI
// ENDFOR
//
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_addsub_pd
FORCE_INLINE __m128d _mm_addsub_pd(__m128d a, __m128d b)
{
    _sse2neon_const __m128d mask = _mm_set_pd(1.0f, -1.0f);
#if defined(__aarch64__)
    return vreinterpretq_m128d_f64(vfmaq_f64(vreinterpretq_f64_m128d(a),
                                             vreinterpretq_f64_m128d(b),
                                             vreinterpretq_f64_m128d(mask)));
#else
    return _mm_add_pd(_mm_mul_pd(b, mask), a);
#endif
}

// Alternatively add and subtract packed single-precision (32-bit)
// floating-point elements in a to/from packed elements in b, and store the
// results in dst.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=addsub_ps
FORCE_INLINE __m128 _mm_addsub_ps(__m128 a, __m128 b)
{
    _sse2neon_const __m128 mask = _mm_setr_ps(-1.0f, 1.0f, -1.0f, 1.0f);
#if defined(__aarch64__) || defined(__ARM_FEATURE_FMA) /* VFPv4+ */
    return vreinterpretq_m128_f32(vfmaq_f32(vreinterpretq_f32_m128(a),
                                            vreinterpretq_f32_m128(mask),
                                            vreinterpretq_f32_m128(b)));
#else
    return _mm_add_ps(_mm_mul_ps(b, mask), a);
#endif
}

// Horizontally add adjacent pairs of double-precision (64-bit) floating-point
// elements in a and b, and pack the results in dst.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_hadd_pd
FORCE_INLINE __m128d _mm_hadd_pd(__m128d a, __m128d b)
{
#if defined(__aarch64__)
    return vreinterpretq_m128d_f64(
        vpaddq_f64(vreinterpretq_f64_m128d(a), vreinterpretq_f64_m128d(b)));
#else
    double *da = (double *) &a;
    double *db = (double *) &b;
    double c[] = {da[0] + da[1], db[0] + db[1]};
    return vreinterpretq_m128d_u64(vld1q_u64((uint64_t *) c));
#endif
}

// Computes pairwise add of each argument as single-precision, floating-point
// values a and b.
// https://msdn.microsoft.com/en-us/library/yd9wecaa.aspx
FORCE_INLINE __m128 _mm_hadd_ps(__m128 a, __m128 b)
{
#if defined(__aarch64__)
    return vreinterpretq_m128_f32(
        vpaddq_f32(vreinterpretq_f32_m128(a), vreinterpretq_f32_m128(b)));
#else
    float32x2_t a10 = vget_low_f32(vreinterpretq_f32_m128(a));
    float32x2_t a32 = vget_high_f32(vreinterpretq_f32_m128(a));
    float32x2_t b10 = vget_low_f32(vreinterpretq_f32_m128(b));
    float32x2_t b32 = vget_high_f32(vreinterpretq_f32_m128(b));
    return vreinterpretq_m128_f32(
        vcombine_f32(vpadd_f32(a10, a32), vpadd_f32(b10, b32)));
#endif
}

// Horizontally subtract adjacent pairs of double-precision (64-bit)
// floating-point elements in a and b, and pack the results in dst.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_hsub_pd
FORCE_INLINE __m128d _mm_hsub_pd(__m128d _a, __m128d _b)
{
#if defined(__aarch64__)
    float64x2_t a = vreinterpretq_f64_m128d(_a);
    float64x2_t b = vreinterpretq_f64_m128d(_b);
    return vreinterpretq_m128d_f64(
        vsubq_f64(vuzp1q_f64(a, b), vuzp2q_f64(a, b)));
#else
    double *da = (double *) &_a;
    double *db = (double *) &_b;
    double c[] = {da[0] - da[1], db[0] - db[1]};
    return vreinterpretq_m128d_u64(vld1q_u64((uint64_t *) c));
#endif
}

// Horizontally subtract adjacent pairs of single-precision (32-bit)
// floating-point elements in a and b, and pack the results in dst.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_hsub_ps
FORCE_INLINE __m128 _mm_hsub_ps(__m128 _a, __m128 _b)
{
    float32x4_t a = vreinterpretq_f32_m128(_a);
    float32x4_t b = vreinterpretq_f32_m128(_b);
#if defined(__aarch64__)
    return vreinterpretq_m128_f32(
        vsubq_f32(vuzp1q_f32(a, b), vuzp2q_f32(a, b)));
#else
    float32x4x2_t c = vuzpq_f32(a, b);
    return vreinterpretq_m128_f32(vsubq_f32(c.val[0], c.val[1]));
#endif
}

// Load 128-bits of integer data from unaligned memory into dst. This intrinsic
// may perform better than _mm_loadu_si128 when the data crosses a cache line
// boundary.
//
//   dst[127:0] := MEM[mem_addr+127:mem_addr]
//
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_lddqu_si128
#define _mm_lddqu_si128 _mm_loadu_si128

// Load a double-precision (64-bit) floating-point element from memory into both
// elements of dst.
//
//   dst[63:0] := MEM[mem_addr+63:mem_addr]
//   dst[127:64] := MEM[mem_addr+63:mem_addr]
//
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_loaddup_pd
#define _mm_loaddup_pd _mm_load1_pd

// Duplicate the low double-precision (64-bit) floating-point element from a,
// and store the results in dst.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_movedup_pd
FORCE_INLINE __m128d _mm_movedup_pd(__m128d a)
{
#if defined(__aarch64__)
    return vreinterpretq_m128d_f64(
        vdupq_laneq_f64(vreinterpretq_f64_m128d(a), 0));
#else
    return vreinterpretq_m128d_u64(
        vdupq_n_u64(vgetq_lane_u64(vreinterpretq_u64_m128d(a), 0)));
#endif
}

// Duplicate odd-indexed single-precision (32-bit) floating-point elements
// from a, and store the results in dst.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_movehdup_ps
FORCE_INLINE __m128 _mm_movehdup_ps(__m128 a)
{
#if defined(__aarch64__)
    return vreinterpretq_m128_f32(
        vtrn2q_f32(vreinterpretq_f32_m128(a), vreinterpretq_f32_m128(a)));
#elif defined(_sse2neon_shuffle)
    return vreinterpretq_m128_f32(vshuffleq_s32(
        vreinterpretq_f32_m128(a), vreinterpretq_f32_m128(a), 1, 1, 3, 3));
#else
    float32_t a1 = vgetq_lane_f32(vreinterpretq_f32_m128(a), 1);
    float32_t a3 = vgetq_lane_f32(vreinterpretq_f32_m128(a), 3);
    float ALIGN_STRUCT(16) data[4] = {a1, a1, a3, a3};
    return vreinterpretq_m128_f32(vld1q_f32(data));
#endif
}

// Duplicate even-indexed single-precision (32-bit) floating-point elements
// from a, and store the results in dst.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_moveldup_ps
FORCE_INLINE __m128 _mm_moveldup_ps(__m128 a)
{
#if defined(__aarch64__)
    return vreinterpretq_m128_f32(
        vtrn1q_f32(vreinterpretq_f32_m128(a), vreinterpretq_f32_m128(a)));
#elif defined(_sse2neon_shuffle)
    return vreinterpretq_m128_f32(vshuffleq_s32(
        vreinterpretq_f32_m128(a), vreinterpretq_f32_m128(a), 0, 0, 2, 2));
#else
    float32_t a0 = vgetq_lane_f32(vreinterpretq_f32_m128(a), 0);
    float32_t a2 = vgetq_lane_f32(vreinterpretq_f32_m128(a), 2);
    float ALIGN_STRUCT(16) data[4] = {a0, a0, a2, a2};
    return vreinterpretq_m128_f32(vld1q_f32(data));
#endif
}

/* SSSE3 */

// Compute the absolute value of packed signed 16-bit integers in a, and store
// the unsigned results in dst.
//
//   FOR j := 0 to 7
//     i := j*16
//     dst[i+15:i] := ABS(a[i+15:i])
//   ENDFOR
//
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_abs_epi16
FORCE_INLINE __m128i _mm_abs_epi16(__m128i a)
{
    return vreinterpretq_m128i_s16(vabsq_s16(vreinterpretq_s16_m128i(a)));
}

// Compute the absolute value of packed signed 32-bit integers in a, and store
// the unsigned results in dst.
//
//   FOR j := 0 to 3
//     i := j*32
//     dst[i+31:i] := ABS(a[i+31:i])
//   ENDFOR
//
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_abs_epi32
FORCE_INLINE __m128i _mm_abs_epi32(__m128i a)
{
    return vreinterpretq_m128i_s32(vabsq_s32(vreinterpretq_s32_m128i(a)));
}

// Compute the absolute value of packed signed 8-bit integers in a, and store
// the unsigned results in dst.
//
//   FOR j := 0 to 15
//     i := j*8
//     dst[i+7:i] := ABS(a[i+7:i])
//   ENDFOR
//
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_abs_epi8
FORCE_INLINE __m128i _mm_abs_epi8(__m128i a)
{
    return vreinterpretq_m128i_s8(vabsq_s8(vreinterpretq_s8_m128i(a)));
}

// Compute the absolute value of packed signed 16-bit integers in a, and store
// the unsigned results in dst.
//
//   FOR j := 0 to 3
//     i := j*16
//     dst[i+15:i] := ABS(a[i+15:i])
//   ENDFOR
//
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_abs_pi16
FORCE_INLINE __m64 _mm_abs_pi16(__m64 a)
{
    return vreinterpret_m64_s16(vabs_s16(vreinterpret_s16_m64(a)));
}

// Compute the absolute value of packed signed 32-bit integers in a, and store
// the unsigned results in dst.
//
//   FOR j := 0 to 1
//     i := j*32
//     dst[i+31:i] := ABS(a[i+31:i])
//   ENDFOR
//
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_abs_pi32
FORCE_INLINE __m64 _mm_abs_pi32(__m64 a)
{
    return vreinterpret_m64_s32(vabs_s32(vreinterpret_s32_m64(a)));
}

// Compute the absolute value of packed signed 8-bit integers in a, and store
// the unsigned results in dst.
//
//   FOR j := 0 to 7
//     i := j*8
//     dst[i+7:i] := ABS(a[i+7:i])
//   ENDFOR
//
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_abs_pi8
FORCE_INLINE __m64 _mm_abs_pi8(__m64 a)
{
    return vreinterpret_m64_s8(vabs_s8(vreinterpret_s8_m64(a)));
}

// Concatenate 16-byte blocks in a and b into a 32-byte temporary result, shift
// the result right by imm8 bytes, and store the low 16 bytes in dst.
//
//   tmp[255:0] := ((a[127:0] << 128)[255:0] OR b[127:0]) >> (imm8*8)
//   dst[127:0] := tmp[127:0]
//
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_alignr_epi8
#define _mm_alignr_epi8(a, b, imm)                                            \
    __extension__({                                                           \
        uint8x16_t _a = vreinterpretq_u8_m128i(a);                            \
        uint8x16_t _b = vreinterpretq_u8_m128i(b);                            \
        __m128i ret;                                                          \
        if (_sse2neon_unlikely((imm) & ~31))                                  \
            ret = vreinterpretq_m128i_u8(vdupq_n_u8(0));                      \
        else if (imm >= 16)                                                   \
            ret = _mm_srli_si128(a, imm >= 16 ? imm - 16 : 0);                \
        else                                                                  \
            ret =                                                             \
                vreinterpretq_m128i_u8(vextq_u8(_b, _a, imm < 16 ? imm : 0)); \
        ret;                                                                  \
    })

// Concatenate 8-byte blocks in a and b into a 16-byte temporary result, shift
// the result right by imm8 bytes, and store the low 8 bytes in dst.
//
//   tmp[127:0] := ((a[63:0] << 64)[127:0] OR b[63:0]) >> (imm8*8)
//   dst[63:0] := tmp[63:0]
//
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_alignr_pi8
#define _mm_alignr_pi8(a, b, imm)                                           \
    __extension__({                                                         \
        __m64 ret;                                                          \
        if (_sse2neon_unlikely((imm) >= 16)) {                              \
            ret = vreinterpret_m64_s8(vdup_n_s8(0));                        \
        } else {                                                            \
            uint8x8_t tmp_low, tmp_high;                                    \
            if ((imm) >= 8) {                                               \
                const int idx = (imm) -8;                                   \
                tmp_low = vreinterpret_u8_m64(a);                           \
                tmp_high = vdup_n_u8(0);                                    \
                ret = vreinterpret_m64_u8(vext_u8(tmp_low, tmp_high, idx)); \
            } else {                                                        \
                const int idx = (imm);                                      \
                tmp_low = vreinterpret_u8_m64(b);                           \
                tmp_high = vreinterpret_u8_m64(a);                          \
                ret = vreinterpret_m64_u8(vext_u8(tmp_low, tmp_high, idx)); \
            }                                                               \
        }                                                                   \
        ret;                                                                \
    })

// Computes pairwise add of each argument as a 16-bit signed or unsigned integer
// values a and b.
FORCE_INLINE __m128i _mm_hadd_epi16(__m128i _a, __m128i _b)
{
    int16x8_t a = vreinterpretq_s16_m128i(_a);
    int16x8_t b = vreinterpretq_s16_m128i(_b);
#if defined(__aarch64__)
    return vreinterpretq_m128i_s16(vpaddq_s16(a, b));
#else
    return vreinterpretq_m128i_s16(
        vcombine_s16(vpadd_s16(vget_low_s16(a), vget_high_s16(a)),
                     vpadd_s16(vget_low_s16(b), vget_high_s16(b))));
#endif
}

// Computes pairwise add of each argument as a 32-bit signed or unsigned integer
// values a and b.
FORCE_INLINE __m128i _mm_hadd_epi32(__m128i _a, __m128i _b)
{
    int32x4_t a = vreinterpretq_s32_m128i(_a);
    int32x4_t b = vreinterpretq_s32_m128i(_b);
#if defined(__aarch64__)
    return vreinterpretq_m128i_s32(vpaddq_s32(a, b));
#else
    return vreinterpretq_m128i_s32(
        vcombine_s32(vpadd_s32(vget_low_s32(a), vget_high_s32(a)),
                     vpadd_s32(vget_low_s32(b), vget_high_s32(b))));
#endif
}

// Horizontally add adjacent pairs of 16-bit integers in a and b, and pack the
// signed 16-bit results in dst.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_hadd_pi16
FORCE_INLINE __m64 _mm_hadd_pi16(__m64 a, __m64 b)
{
    return vreinterpret_m64_s16(
        vpadd_s16(vreinterpret_s16_m64(a), vreinterpret_s16_m64(b)));
}

// Horizontally add adjacent pairs of 32-bit integers in a and b, and pack the
// signed 32-bit results in dst.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_hadd_pi32
FORCE_INLINE __m64 _mm_hadd_pi32(__m64 a, __m64 b)
{
    return vreinterpret_m64_s32(
        vpadd_s32(vreinterpret_s32_m64(a), vreinterpret_s32_m64(b)));
}

// Computes saturated pairwise sub of each argument as a 16-bit signed
// integer values a and b.
FORCE_INLINE __m128i _mm_hadds_epi16(__m128i _a, __m128i _b)
{
#if defined(__aarch64__)
    int16x8_t a = vreinterpretq_s16_m128i(_a);
    int16x8_t b = vreinterpretq_s16_m128i(_b);
    return vreinterpretq_s64_s16(
        vqaddq_s16(vuzp1q_s16(a, b), vuzp2q_s16(a, b)));
#else
    int32x4_t a = vreinterpretq_s32_m128i(_a);
    int32x4_t b = vreinterpretq_s32_m128i(_b);
    // Interleave using vshrn/vmovn
    // [a0|a2|a4|a6|b0|b2|b4|b6]
    // [a1|a3|a5|a7|b1|b3|b5|b7]
    int16x8_t ab0246 = vcombine_s16(vmovn_s32(a), vmovn_s32(b));
    int16x8_t ab1357 = vcombine_s16(vshrn_n_s32(a, 16), vshrn_n_s32(b, 16));
    // Saturated add
    return vreinterpretq_m128i_s16(vqaddq_s16(ab0246, ab1357));
#endif
}

// Horizontally add adjacent pairs of signed 16-bit integers in a and b using
// saturation, and pack the signed 16-bit results in dst.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_hadds_pi16
FORCE_INLINE __m64 _mm_hadds_pi16(__m64 _a, __m64 _b)
{
    int16x4_t a = vreinterpret_s16_m64(_a);
    int16x4_t b = vreinterpret_s16_m64(_b);
#if defined(__aarch64__)
    return vreinterpret_s64_s16(vqadd_s16(vuzp1_s16(a, b), vuzp2_s16(a, b)));
#else
    int16x4x2_t res = vuzp_s16(a, b);
    return vreinterpret_s64_s16(vqadd_s16(res.val[0], res.val[1]));
#endif
}

// Horizontally subtract adjacent pairs of 16-bit integers in a and b, and pack
// the signed 16-bit results in dst.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_hsub_epi16
FORCE_INLINE __m128i _mm_hsub_epi16(__m128i _a, __m128i _b)
{
    int16x8_t a = vreinterpretq_s16_m128i(_a);
    int16x8_t b = vreinterpretq_s16_m128i(_b);
#if defined(__aarch64__)
    return vreinterpretq_m128i_s16(
        vsubq_s16(vuzp1q_s16(a, b), vuzp2q_s16(a, b)));
#else
    int16x8x2_t c = vuzpq_s16(a, b);
    return vreinterpretq_m128i_s16(vsubq_s16(c.val[0], c.val[1]));
#endif
}

// Horizontally subtract adjacent pairs of 32-bit integers in a and b, and pack
// the signed 32-bit results in dst.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_hsub_epi32
FORCE_INLINE __m128i _mm_hsub_epi32(__m128i _a, __m128i _b)
{
    int32x4_t a = vreinterpretq_s32_m128i(_a);
    int32x4_t b = vreinterpretq_s32_m128i(_b);
#if defined(__aarch64__)
    return vreinterpretq_m128i_s32(
        vsubq_s32(vuzp1q_s32(a, b), vuzp2q_s32(a, b)));
#else
    int32x4x2_t c = vuzpq_s32(a, b);
    return vreinterpretq_m128i_s32(vsubq_s32(c.val[0], c.val[1]));
#endif
}

// Horizontally subtract adjacent pairs of 16-bit integers in a and b, and pack
// the signed 16-bit results in dst.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_hsub_pi16
FORCE_INLINE __m64 _mm_hsub_pi16(__m64 _a, __m64 _b)
{
    int16x4_t a = vreinterpret_s16_m64(_a);
    int16x4_t b = vreinterpret_s16_m64(_b);
#if defined(__aarch64__)
    return vreinterpret_m64_s16(vsub_s16(vuzp1_s16(a, b), vuzp2_s16(a, b)));
#else
    int16x4x2_t c = vuzp_s16(a, b);
    return vreinterpret_m64_s16(vsub_s16(c.val[0], c.val[1]));
#endif
}

// Horizontally subtract adjacent pairs of 32-bit integers in a and b, and pack
// the signed 32-bit results in dst.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=mm_hsub_pi32
FORCE_INLINE __m64 _mm_hsub_pi32(__m64 _a, __m64 _b)
{
    int32x2_t a = vreinterpret_s32_m64(_a);
    int32x2_t b = vreinterpret_s32_m64(_b);
#if defined(__aarch64__)
    return vreinterpret_m64_s32(vsub_s32(vuzp1_s32(a, b), vuzp2_s32(a, b)));
#else
    int32x2x2_t c = vuzp_s32(a, b);
    return vreinterpret_m64_s32(vsub_s32(c.val[0], c.val[1]));
#endif
}

// Computes saturated pairwise difference of each argument as a 16-bit signed
// integer values a and b.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_hsubs_epi16
FORCE_INLINE __m128i _mm_hsubs_epi16(__m128i _a, __m128i _b)
{
    int16x8_t a = vreinterpretq_s16_m128i(_a);
    int16x8_t b = vreinterpretq_s16_m128i(_b);
#if defined(__aarch64__)
    return vreinterpretq_m128i_s16(
        vqsubq_s16(vuzp1q_s16(a, b), vuzp2q_s16(a, b)));
#else
    int16x8x2_t c = vuzpq_s16(a, b);
    return vreinterpretq_m128i_s16(vqsubq_s16(c.val[0], c.val[1]));
#endif
}

// Horizontally subtract adjacent pairs of signed 16-bit integers in a and b
// using saturation, and pack the signed 16-bit results in dst.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_hsubs_pi16
FORCE_INLINE __m64 _mm_hsubs_pi16(__m64 _a, __m64 _b)
{
    int16x4_t a = vreinterpret_s16_m64(_a);
    int16x4_t b = vreinterpret_s16_m64(_b);
#if defined(__aarch64__)
    return vreinterpret_m64_s16(vqsub_s16(vuzp1_s16(a, b), vuzp2_s16(a, b)));
#else
    int16x4x2_t c = vuzp_s16(a, b);
    return vreinterpret_m64_s16(vqsub_s16(c.val[0], c.val[1]));
#endif
}

// Vertically multiply each unsigned 8-bit integer from a with the corresponding
// signed 8-bit integer from b, producing intermediate signed 16-bit integers.
// Horizontally add adjacent pairs of intermediate signed 16-bit integers,
// and pack the saturated results in dst.
//
//   FOR j := 0 to 7
//      i := j*16
//      dst[i+15:i] := Saturate_To_Int16( a[i+15:i+8]*b[i+15:i+8] +
//      a[i+7:i]*b[i+7:i] )
//   ENDFOR
FORCE_INLINE __m128i _mm_maddubs_epi16(__m128i _a, __m128i _b)
{
#if defined(__aarch64__)
    uint8x16_t a = vreinterpretq_u8_m128i(_a);
    int8x16_t b = vreinterpretq_s8_m128i(_b);
    int16x8_t tl = vmulq_s16(vreinterpretq_s16_u16(vmovl_u8(vget_low_u8(a))),
                             vmovl_s8(vget_low_s8(b)));
    int16x8_t th = vmulq_s16(vreinterpretq_s16_u16(vmovl_u8(vget_high_u8(a))),
                             vmovl_s8(vget_high_s8(b)));
    return vreinterpretq_m128i_s16(
        vqaddq_s16(vuzp1q_s16(tl, th), vuzp2q_s16(tl, th)));
#else
    // This would be much simpler if x86 would choose to zero extend OR sign
    // extend, not both. This could probably be optimized better.
    uint16x8_t a = vreinterpretq_u16_m128i(_a);
    int16x8_t b = vreinterpretq_s16_m128i(_b);

    // Zero extend a
    int16x8_t a_odd = vreinterpretq_s16_u16(vshrq_n_u16(a, 8));
    int16x8_t a_even = vreinterpretq_s16_u16(vbicq_u16(a, vdupq_n_u16(0xff00)));

    // Sign extend by shifting left then shifting right.
    int16x8_t b_even = vshrq_n_s16(vshlq_n_s16(b, 8), 8);
    int16x8_t b_odd = vshrq_n_s16(b, 8);

    // multiply
    int16x8_t prod1 = vmulq_s16(a_even, b_even);
    int16x8_t prod2 = vmulq_s16(a_odd, b_odd);

    // saturated add
    return vreinterpretq_m128i_s16(vqaddq_s16(prod1, prod2));
#endif
}

// Vertically multiply each unsigned 8-bit integer from a with the corresponding
// signed 8-bit integer from b, producing intermediate signed 16-bit integers.
// Horizontally add adjacent pairs of intermediate signed 16-bit integers, and
// pack the saturated results in dst.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_maddubs_pi16
FORCE_INLINE __m64 _mm_maddubs_pi16(__m64 _a, __m64 _b)
{
    uint16x4_t a = vreinterpret_u16_m64(_a);
    int16x4_t b = vreinterpret_s16_m64(_b);

    // Zero extend a
    int16x4_t a_odd = vreinterpret_s16_u16(vshr_n_u16(a, 8));
    int16x4_t a_even = vreinterpret_s16_u16(vand_u16(a, vdup_n_u16(0xff)));

    // Sign extend by shifting left then shifting right.
    int16x4_t b_even = vshr_n_s16(vshl_n_s16(b, 8), 8);
    int16x4_t b_odd = vshr_n_s16(b, 8);

    // multiply
    int16x4_t prod1 = vmul_s16(a_even, b_even);
    int16x4_t prod2 = vmul_s16(a_odd, b_odd);

    // saturated add
    return vreinterpret_m64_s16(vqadd_s16(prod1, prod2));
}

// Multiply packed signed 16-bit integers in a and b, producing intermediate
// signed 32-bit integers. Shift right by 15 bits while rounding up, and store
// the packed 16-bit integers in dst.
//
//   r0 := Round(((int32_t)a0 * (int32_t)b0) >> 15)
//   r1 := Round(((int32_t)a1 * (int32_t)b1) >> 15)
//   r2 := Round(((int32_t)a2 * (int32_t)b2) >> 15)
//   ...
//   r7 := Round(((int32_t)a7 * (int32_t)b7) >> 15)
FORCE_INLINE __m128i _mm_mulhrs_epi16(__m128i a, __m128i b)
{
    // Has issues due to saturation
    // return vreinterpretq_m128i_s16(vqrdmulhq_s16(a, b));

    // Multiply
    int32x4_t mul_lo = vmull_s16(vget_low_s16(vreinterpretq_s16_m128i(a)),
                                 vget_low_s16(vreinterpretq_s16_m128i(b)));
    int32x4_t mul_hi = vmull_s16(vget_high_s16(vreinterpretq_s16_m128i(a)),
                                 vget_high_s16(vreinterpretq_s16_m128i(b)));

    // Rounding narrowing shift right
    // narrow = (int16_t)((mul + 16384) >> 15);
    int16x4_t narrow_lo = vrshrn_n_s32(mul_lo, 15);
    int16x4_t narrow_hi = vrshrn_n_s32(mul_hi, 15);

    // Join together
    return vreinterpretq_m128i_s16(vcombine_s16(narrow_lo, narrow_hi));
}

// Multiply packed signed 16-bit integers in a and b, producing intermediate
// signed 32-bit integers. Truncate each intermediate integer to the 18 most
// significant bits, round by adding 1, and store bits [16:1] to dst.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_mulhrs_pi16
FORCE_INLINE __m64 _mm_mulhrs_pi16(__m64 a, __m64 b)
{
    int32x4_t mul_extend =
        vmull_s16((vreinterpret_s16_m64(a)), (vreinterpret_s16_m64(b)));

    // Rounding narrowing shift right
    return vreinterpret_m64_s16(vrshrn_n_s32(mul_extend, 15));
}

// Shuffle packed 8-bit integers in a according to shuffle control mask in the
// corresponding 8-bit element of b, and store the results in dst.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_shuffle_epi8
FORCE_INLINE __m128i _mm_shuffle_epi8(__m128i a, __m128i b)
{
    int8x16_t tbl = vreinterpretq_s8_m128i(a);   // input a
    uint8x16_t idx = vreinterpretq_u8_m128i(b);  // input b
    uint8x16_t idx_masked =
        vandq_u8(idx, vdupq_n_u8(0x8F));  // avoid using meaningless bits
#if defined(__aarch64__)
    return vreinterpretq_m128i_s8(vqtbl1q_s8(tbl, idx_masked));
#elif defined(__GNUC__)
    int8x16_t ret;
    // %e and %f represent the even and odd D registers
    // respectively.
    __asm__ __volatile__(
        "vtbl.8  %e[ret], {%e[tbl], %f[tbl]}, %e[idx]\n"
        "vtbl.8  %f[ret], {%e[tbl], %f[tbl]}, %f[idx]\n"
        : [ret] "=&w"(ret)
        : [tbl] "w"(tbl), [idx] "w"(idx_masked));
    return vreinterpretq_m128i_s8(ret);
#else
    // use this line if testing on aarch64
    int8x8x2_t a_split = {vget_low_s8(tbl), vget_high_s8(tbl)};
    return vreinterpretq_m128i_s8(
        vcombine_s8(vtbl2_s8(a_split, vget_low_u8(idx_masked)),
                    vtbl2_s8(a_split, vget_high_u8(idx_masked))));
#endif
}

// Shuffle packed 8-bit integers in a according to shuffle control mask in the
// corresponding 8-bit element of b, and store the results in dst.
//
//   FOR j := 0 to 7
//     i := j*8
//     IF b[i+7] == 1
//       dst[i+7:i] := 0
//     ELSE
//       index[2:0] := b[i+2:i]
//       dst[i+7:i] := a[index*8+7:index*8]
//     FI
//   ENDFOR
//
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_shuffle_pi8
FORCE_INLINE __m64 _mm_shuffle_pi8(__m64 a, __m64 b)
{
    const int8x8_t controlMask =
        vand_s8(vreinterpret_s8_m64(b), vdup_n_s8((int8_t) (0x1 << 7 | 0x07)));
    int8x8_t res = vtbl1_s8(vreinterpret_s8_m64(a), controlMask);
    return vreinterpret_m64_s8(res);
}

// Negate packed 16-bit integers in a when the corresponding signed
// 16-bit integer in b is negative, and store the results in dst.
// Element in dst are zeroed out when the corresponding element
// in b is zero.
//
//   for i in 0..7
//     if b[i] < 0
//       r[i] := -a[i]
//     else if b[i] == 0
//       r[i] := 0
//     else
//       r[i] := a[i]
//     fi
//   done
FORCE_INLINE __m128i _mm_sign_epi16(__m128i _a, __m128i _b)
{
    int16x8_t a = vreinterpretq_s16_m128i(_a);
    int16x8_t b = vreinterpretq_s16_m128i(_b);

    // signed shift right: faster than vclt
    // (b < 0) ? 0xFFFF : 0
    uint16x8_t ltMask = vreinterpretq_u16_s16(vshrq_n_s16(b, 15));
    // (b == 0) ? 0xFFFF : 0
#if defined(__aarch64__)
    int16x8_t zeroMask = vreinterpretq_s16_u16(vceqzq_s16(b));
#else
    int16x8_t zeroMask = vreinterpretq_s16_u16(vceqq_s16(b, vdupq_n_s16(0)));
#endif

    // bitwise select either a or negative 'a' (vnegq_s16(a) equals to negative
    // 'a') based on ltMask
    int16x8_t masked = vbslq_s16(ltMask, vnegq_s16(a), a);
    // res = masked & (~zeroMask)
    int16x8_t res = vbicq_s16(masked, zeroMask);
    return vreinterpretq_m128i_s16(res);
}

// Negate packed 32-bit integers in a when the corresponding signed
// 32-bit integer in b is negative, and store the results in dst.
// Element in dst are zeroed out when the corresponding element
// in b is zero.
//
//   for i in 0..3
//     if b[i] < 0
//       r[i] := -a[i]
//     else if b[i] == 0
//       r[i] := 0
//     else
//       r[i] := a[i]
//     fi
//   done
FORCE_INLINE __m128i _mm_sign_epi32(__m128i _a, __m128i _b)
{
    int32x4_t a = vreinterpretq_s32_m128i(_a);
    int32x4_t b = vreinterpretq_s32_m128i(_b);

    // signed shift right: faster than vclt
    // (b < 0) ? 0xFFFFFFFF : 0
    uint32x4_t ltMask = vreinterpretq_u32_s32(vshrq_n_s32(b, 31));

    // (b == 0) ? 0xFFFFFFFF : 0
#if defined(__aarch64__)
    int32x4_t zeroMask = vreinterpretq_s32_u32(vceqzq_s32(b));
#else
    int32x4_t zeroMask = vreinterpretq_s32_u32(vceqq_s32(b, vdupq_n_s32(0)));
#endif

    // bitwise select either a or negative 'a' (vnegq_s32(a) equals to negative
    // 'a') based on ltMask
    int32x4_t masked = vbslq_s32(ltMask, vnegq_s32(a), a);
    // res = masked & (~zeroMask)
    int32x4_t res = vbicq_s32(masked, zeroMask);
    return vreinterpretq_m128i_s32(res);
}

// Negate packed 8-bit integers in a when the corresponding signed
// 8-bit integer in b is negative, and store the results in dst.
// Element in dst are zeroed out when the corresponding element
// in b is zero.
//
//   for i in 0..15
//     if b[i] < 0
//       r[i] := -a[i]
//     else if b[i] == 0
//       r[i] := 0
//     else
//       r[i] := a[i]
//     fi
//   done
FORCE_INLINE __m128i _mm_sign_epi8(__m128i _a, __m128i _b)
{
    int8x16_t a = vreinterpretq_s8_m128i(_a);
    int8x16_t b = vreinterpretq_s8_m128i(_b);

    // signed shift right: faster than vclt
    // (b < 0) ? 0xFF : 0
    uint8x16_t ltMask = vreinterpretq_u8_s8(vshrq_n_s8(b, 7));

    // (b == 0) ? 0xFF : 0
#if defined(__aarch64__)
    int8x16_t zeroMask = vreinterpretq_s8_u8(vceqzq_s8(b));
#else
    int8x16_t zeroMask = vreinterpretq_s8_u8(vceqq_s8(b, vdupq_n_s8(0)));
#endif

    // bitwise select either a or negative 'a' (vnegq_s8(a) return negative 'a')
    // based on ltMask
    int8x16_t masked = vbslq_s8(ltMask, vnegq_s8(a), a);
    // res = masked & (~zeroMask)
    int8x16_t res = vbicq_s8(masked, zeroMask);

    return vreinterpretq_m128i_s8(res);
}

// Negate packed 16-bit integers in a when the corresponding signed 16-bit
// integer in b is negative, and store the results in dst. Element in dst are
// zeroed out when the corresponding element in b is zero.
//
//   FOR j := 0 to 3
//      i := j*16
//      IF b[i+15:i] < 0
//        dst[i+15:i] := -(a[i+15:i])
//      ELSE IF b[i+15:i] == 0
//        dst[i+15:i] := 0
//      ELSE
//        dst[i+15:i] := a[i+15:i]
//      FI
//   ENDFOR
//
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_sign_pi16
FORCE_INLINE __m64 _mm_sign_pi16(__m64 _a, __m64 _b)
{
    int16x4_t a = vreinterpret_s16_m64(_a);
    int16x4_t b = vreinterpret_s16_m64(_b);

    // signed shift right: faster than vclt
    // (b < 0) ? 0xFFFF : 0
    uint16x4_t ltMask = vreinterpret_u16_s16(vshr_n_s16(b, 15));

    // (b == 0) ? 0xFFFF : 0
#if defined(__aarch64__)
    int16x4_t zeroMask = vreinterpret_s16_u16(vceqz_s16(b));
#else
    int16x4_t zeroMask = vreinterpret_s16_u16(vceq_s16(b, vdup_n_s16(0)));
#endif

    // bitwise select either a or negative 'a' (vneg_s16(a) return negative 'a')
    // based on ltMask
    int16x4_t masked = vbsl_s16(ltMask, vneg_s16(a), a);
    // res = masked & (~zeroMask)
    int16x4_t res = vbic_s16(masked, zeroMask);

    return vreinterpret_m64_s16(res);
}

// Negate packed 32-bit integers in a when the corresponding signed 32-bit
// integer in b is negative, and store the results in dst. Element in dst are
// zeroed out when the corresponding element in b is zero.
//
//   FOR j := 0 to 1
//      i := j*32
//      IF b[i+31:i] < 0
//        dst[i+31:i] := -(a[i+31:i])
//      ELSE IF b[i+31:i] == 0
//        dst[i+31:i] := 0
//      ELSE
//        dst[i+31:i] := a[i+31:i]
//      FI
//   ENDFOR
//
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_sign_pi32
FORCE_INLINE __m64 _mm_sign_pi32(__m64 _a, __m64 _b)
{
    int32x2_t a = vreinterpret_s32_m64(_a);
    int32x2_t b = vreinterpret_s32_m64(_b);

    // signed shift right: faster than vclt
    // (b < 0) ? 0xFFFFFFFF : 0
    uint32x2_t ltMask = vreinterpret_u32_s32(vshr_n_s32(b, 31));

    // (b == 0) ? 0xFFFFFFFF : 0
#if defined(__aarch64__)
    int32x2_t zeroMask = vreinterpret_s32_u32(vceqz_s32(b));
#else
    int32x2_t zeroMask = vreinterpret_s32_u32(vceq_s32(b, vdup_n_s32(0)));
#endif

    // bitwise select either a or negative 'a' (vneg_s32(a) return negative 'a')
    // based on ltMask
    int32x2_t masked = vbsl_s32(ltMask, vneg_s32(a), a);
    // res = masked & (~zeroMask)
    int32x2_t res = vbic_s32(masked, zeroMask);

    return vreinterpret_m64_s32(res);
}

// Negate packed 8-bit integers in a when the corresponding signed 8-bit integer
// in b is negative, and store the results in dst. Element in dst are zeroed out
// when the corresponding element in b is zero.
//
//   FOR j := 0 to 7
//      i := j*8
//      IF b[i+7:i] < 0
//        dst[i+7:i] := -(a[i+7:i])
//      ELSE IF b[i+7:i] == 0
//        dst[i+7:i] := 0
//      ELSE
//        dst[i+7:i] := a[i+7:i]
//      FI
//   ENDFOR
//
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_sign_pi8
FORCE_INLINE __m64 _mm_sign_pi8(__m64 _a, __m64 _b)
{
    int8x8_t a = vreinterpret_s8_m64(_a);
    int8x8_t b = vreinterpret_s8_m64(_b);

    // signed shift right: faster than vclt
    // (b < 0) ? 0xFF : 0
    uint8x8_t ltMask = vreinterpret_u8_s8(vshr_n_s8(b, 7));

    // (b == 0) ? 0xFF : 0
#if defined(__aarch64__)
    int8x8_t zeroMask = vreinterpret_s8_u8(vceqz_s8(b));
#else
    int8x8_t zeroMask = vreinterpret_s8_u8(vceq_s8(b, vdup_n_s8(0)));
#endif

    // bitwise select either a or negative 'a' (vneg_s8(a) return negative 'a')
    // based on ltMask
    int8x8_t masked = vbsl_s8(ltMask, vneg_s8(a), a);
    // res = masked & (~zeroMask)
    int8x8_t res = vbic_s8(masked, zeroMask);

    return vreinterpret_m64_s8(res);
}

/* SSE4.1 */

// Blend packed 16-bit integers from a and b using control mask imm8, and store
// the results in dst.
//
//   FOR j := 0 to 7
//       i := j*16
//       IF imm8[j]
//           dst[i+15:i] := b[i+15:i]
//       ELSE
//           dst[i+15:i] := a[i+15:i]
//       FI
//   ENDFOR
// FORCE_INLINE __m128i _mm_blend_epi16(__m128i a, __m128i b,
//                                      __constrange(0,255) int imm)
#define _mm_blend_epi16(a, b, imm)                                            \
    __extension__({                                                           \
        const uint16_t ones = 0xffff;                                         \
        const uint16_t zeros = 0x0000;                                        \
        const uint16_t _mask[8] = {((imm) & (1 << 0)) ? ones : zeros,         \
                                   ((imm) & (1 << 1)) ? ones : zeros,         \
                                   ((imm) & (1 << 2)) ? ones : zeros,         \
                                   ((imm) & (1 << 3)) ? ones : zeros,         \
                                   ((imm) & (1 << 4)) ? ones : zeros,         \
                                   ((imm) & (1 << 5)) ? ones : zeros,         \
                                   ((imm) & (1 << 6)) ? ones : zeros,         \
                                   ((imm) & (1 << 7)) ? ones : zeros};        \
        uint16x8_t _mask_vec = vld1q_u16(_mask);                              \
        uint16x8_t _a = vreinterpretq_u16_m128i(a);                           \
        uint16x8_t _b = vreinterpretq_u16_m128i(b);                           \
        vreinterpretq_m128i_u16(vbslq_u16(_mask_vec, _b, _a));                \
    })

// Blend packed double-precision (64-bit) floating-point elements from a and b
// using control mask imm8, and store the results in dst.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_blend_pd
#define _mm_blend_pd(a, b, imm)                                \
    __extension__({                                            \
        const uint64_t _mask[2] = {                            \
            ((imm) & (1 << 0)) ? ~UINT64_C(0) : UINT64_C(0),   \
            ((imm) & (1 << 1)) ? ~UINT64_C(0) : UINT64_C(0)};  \
        uint64x2_t _mask_vec = vld1q_u64(_mask);               \
        uint64x2_t _a = vreinterpretq_u64_m128d(a);            \
        uint64x2_t _b = vreinterpretq_u64_m128d(b);            \
        vreinterpretq_m128d_u64(vbslq_u64(_mask_vec, _b, _a)); \
    })

// Blend packed single-precision (32-bit) floating-point elements from a and b
// using mask, and store the results in dst.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_blend_ps
FORCE_INLINE __m128 _mm_blend_ps(__m128 _a, __m128 _b, const char imm8)
{
    const uint32_t ALIGN_STRUCT(16)
        data[4] = {((imm8) & (1 << 0)) ? UINT32_MAX : 0,
                   ((imm8) & (1 << 1)) ? UINT32_MAX : 0,
                   ((imm8) & (1 << 2)) ? UINT32_MAX : 0,
                   ((imm8) & (1 << 3)) ? UINT32_MAX : 0};
    uint32x4_t mask = vld1q_u32(data);
    float32x4_t a = vreinterpretq_f32_m128(_a);
    float32x4_t b = vreinterpretq_f32_m128(_b);
    return vreinterpretq_m128_f32(vbslq_f32(mask, b, a));
}

// Blend packed 8-bit integers from a and b using mask, and store the results in
// dst.
//
//   FOR j := 0 to 15
//       i := j*8
//       IF mask[i+7]
//           dst[i+7:i] := b[i+7:i]
//       ELSE
//           dst[i+7:i] := a[i+7:i]
//       FI
//   ENDFOR
FORCE_INLINE __m128i _mm_blendv_epi8(__m128i _a, __m128i _b, __m128i _mask)
{
    // Use a signed shift right to create a mask with the sign bit
    uint8x16_t mask =
        vreinterpretq_u8_s8(vshrq_n_s8(vreinterpretq_s8_m128i(_mask), 7));
    uint8x16_t a = vreinterpretq_u8_m128i(_a);
    uint8x16_t b = vreinterpretq_u8_m128i(_b);
    return vreinterpretq_m128i_u8(vbslq_u8(mask, b, a));
}

// Blend packed double-precision (64-bit) floating-point elements from a and b
// using mask, and store the results in dst.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_blendv_pd
FORCE_INLINE __m128d _mm_blendv_pd(__m128d _a, __m128d _b, __m128d _mask)
{
    uint64x2_t mask =
        vreinterpretq_u64_s64(vshrq_n_s64(vreinterpretq_s64_m128d(_mask), 63));
#if defined(__aarch64__)
    float64x2_t a = vreinterpretq_f64_m128d(_a);
    float64x2_t b = vreinterpretq_f64_m128d(_b);
    return vreinterpretq_m128d_f64(vbslq_f64(mask, b, a));
#else
    uint64x2_t a = vreinterpretq_u64_m128d(_a);
    uint64x2_t b = vreinterpretq_u64_m128d(_b);
    return vreinterpretq_m128d_u64(vbslq_u64(mask, b, a));
#endif
}

// Blend packed single-precision (32-bit) floating-point elements from a and b
// using mask, and store the results in dst.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_blendv_ps
FORCE_INLINE __m128 _mm_blendv_ps(__m128 _a, __m128 _b, __m128 _mask)
{
    // Use a signed shift right to create a mask with the sign bit
    uint32x4_t mask =
        vreinterpretq_u32_s32(vshrq_n_s32(vreinterpretq_s32_m128(_mask), 31));
    float32x4_t a = vreinterpretq_f32_m128(_a);
    float32x4_t b = vreinterpretq_f32_m128(_b);
    return vreinterpretq_m128_f32(vbslq_f32(mask, b, a));
}

// Round the packed double-precision (64-bit) floating-point elements in a up
// to an integer value, and store the results as packed double-precision
// floating-point elements in dst.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_ceil_pd
FORCE_INLINE __m128d _mm_ceil_pd(__m128d a)
{
#if defined(__aarch64__)
    return vreinterpretq_m128d_f64(vrndpq_f64(vreinterpretq_f64_m128d(a)));
#else
    double *f = (double *) &a;
    return _mm_set_pd(ceil(f[1]), ceil(f[0]));
#endif
}

// Round the packed single-precision (32-bit) floating-point elements in a up to
// an integer value, and store the results as packed single-precision
// floating-point elements in dst.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_ceil_ps
FORCE_INLINE __m128 _mm_ceil_ps(__m128 a)
{
#if defined(__aarch64__) || defined(__ARM_FEATURE_DIRECTED_ROUNDING)
    return vreinterpretq_m128_f32(vrndpq_f32(vreinterpretq_f32_m128(a)));
#else
    float *f = (float *) &a;
    return _mm_set_ps(ceilf(f[3]), ceilf(f[2]), ceilf(f[1]), ceilf(f[0]));
#endif
}

// Round the lower double-precision (64-bit) floating-point element in b up to
// an integer value, store the result as a double-precision floating-point
// element in the lower element of dst, and copy the upper element from a to the
// upper element of dst.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_ceil_sd
FORCE_INLINE __m128d _mm_ceil_sd(__m128d a, __m128d b)
{
    return _mm_move_sd(a, _mm_ceil_pd(b));
}

// Round the lower single-precision (32-bit) floating-point element in b up to
// an integer value, store the result as a single-precision floating-point
// element in the lower element of dst, and copy the upper 3 packed elements
// from a to the upper elements of dst.
//
//   dst[31:0] := CEIL(b[31:0])
//   dst[127:32] := a[127:32]
//
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_ceil_ss
FORCE_INLINE __m128 _mm_ceil_ss(__m128 a, __m128 b)
{
    return _mm_move_ss(a, _mm_ceil_ps(b));
}

// Compare packed 64-bit integers in a and b for equality, and store the results
// in dst
FORCE_INLINE __m128i _mm_cmpeq_epi64(__m128i a, __m128i b)
{
#if defined(__aarch64__)
    return vreinterpretq_m128i_u64(
        vceqq_u64(vreinterpretq_u64_m128i(a), vreinterpretq_u64_m128i(b)));
#else
    // ARMv7 lacks vceqq_u64
    // (a == b) -> (a_lo == b_lo) && (a_hi == b_hi)
    uint32x4_t cmp =
        vceqq_u32(vreinterpretq_u32_m128i(a), vreinterpretq_u32_m128i(b));
    uint32x4_t swapped = vrev64q_u32(cmp);
    return vreinterpretq_m128i_u32(vandq_u32(cmp, swapped));
#endif
}

// Converts the four signed 16-bit integers in the lower 64 bits to four signed
// 32-bit integers.
FORCE_INLINE __m128i _mm_cvtepi16_epi32(__m128i a)
{
    return vreinterpretq_m128i_s32(
        vmovl_s16(vget_low_s16(vreinterpretq_s16_m128i(a))));
}

// Converts the two signed 16-bit integers in the lower 32 bits two signed
// 32-bit integers.
FORCE_INLINE __m128i _mm_cvtepi16_epi64(__m128i a)
{
    int16x8_t s16x8 = vreinterpretq_s16_m128i(a);     /* xxxx xxxx xxxx 0B0A */
    int32x4_t s32x4 = vmovl_s16(vget_low_s16(s16x8)); /* 000x 000x 000B 000A */
    int64x2_t s64x2 = vmovl_s32(vget_low_s32(s32x4)); /* 0000 000B 0000 000A */
    return vreinterpretq_m128i_s64(s64x2);
}

// Converts the two signed 32-bit integers in the lower 64 bits to two signed
// 64-bit integers.
FORCE_INLINE __m128i _mm_cvtepi32_epi64(__m128i a)
{
    return vreinterpretq_m128i_s64(
        vmovl_s32(vget_low_s32(vreinterpretq_s32_m128i(a))));
}

// Converts the four unsigned 8-bit integers in the lower 16 bits to four
// unsigned 32-bit integers.
FORCE_INLINE __m128i _mm_cvtepi8_epi16(__m128i a)
{
    int8x16_t s8x16 = vreinterpretq_s8_m128i(a);    /* xxxx xxxx xxxx DCBA */
    int16x8_t s16x8 = vmovl_s8(vget_low_s8(s8x16)); /* 0x0x 0x0x 0D0C 0B0A */
    return vreinterpretq_m128i_s16(s16x8);
}

// Converts the four unsigned 8-bit integers in the lower 32 bits to four
// unsigned 32-bit integers.
FORCE_INLINE __m128i _mm_cvtepi8_epi32(__m128i a)
{
    int8x16_t s8x16 = vreinterpretq_s8_m128i(a);      /* xxxx xxxx xxxx DCBA */
    int16x8_t s16x8 = vmovl_s8(vget_low_s8(s8x16));   /* 0x0x 0x0x 0D0C 0B0A */
    int32x4_t s32x4 = vmovl_s16(vget_low_s16(s16x8)); /* 000D 000C 000B 000A */
    return vreinterpretq_m128i_s32(s32x4);
}

// Converts the two signed 8-bit integers in the lower 32 bits to four
// signed 64-bit integers.
FORCE_INLINE __m128i _mm_cvtepi8_epi64(__m128i a)
{
    int8x16_t s8x16 = vreinterpretq_s8_m128i(a);      /* xxxx xxxx xxxx xxBA */
    int16x8_t s16x8 = vmovl_s8(vget_low_s8(s8x16));   /* 0x0x 0x0x 0x0x 0B0A */
    int32x4_t s32x4 = vmovl_s16(vget_low_s16(s16x8)); /* 000x 000x 000B 000A */
    int64x2_t s64x2 = vmovl_s32(vget_low_s32(s32x4)); /* 0000 000B 0000 000A */
    return vreinterpretq_m128i_s64(s64x2);
}

// Converts the four unsigned 16-bit integers in the lower 64 bits to four
// unsigned 32-bit integers.
FORCE_INLINE __m128i _mm_cvtepu16_epi32(__m128i a)
{
    return vreinterpretq_m128i_u32(
        vmovl_u16(vget_low_u16(vreinterpretq_u16_m128i(a))));
}

// Converts the two unsigned 16-bit integers in the lower 32 bits to two
// unsigned 64-bit integers.
FORCE_INLINE __m128i _mm_cvtepu16_epi64(__m128i a)
{
    uint16x8_t u16x8 = vreinterpretq_u16_m128i(a);     /* xxxx xxxx xxxx 0B0A */
    uint32x4_t u32x4 = vmovl_u16(vget_low_u16(u16x8)); /* 000x 000x 000B 000A */
    uint64x2_t u64x2 = vmovl_u32(vget_low_u32(u32x4)); /* 0000 000B 0000 000A */
    return vreinterpretq_m128i_u64(u64x2);
}

// Converts the two unsigned 32-bit integers in the lower 64 bits to two
// unsigned 64-bit integers.
FORCE_INLINE __m128i _mm_cvtepu32_epi64(__m128i a)
{
    return vreinterpretq_m128i_u64(
        vmovl_u32(vget_low_u32(vreinterpretq_u32_m128i(a))));
}

// Zero extend packed unsigned 8-bit integers in a to packed 16-bit integers,
// and store the results in dst.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_cvtepu8_epi16
FORCE_INLINE __m128i _mm_cvtepu8_epi16(__m128i a)
{
    uint8x16_t u8x16 = vreinterpretq_u8_m128i(a);    /* xxxx xxxx HGFE DCBA */
    uint16x8_t u16x8 = vmovl_u8(vget_low_u8(u8x16)); /* 0H0G 0F0E 0D0C 0B0A */
    return vreinterpretq_m128i_u16(u16x8);
}

// Converts the four unsigned 8-bit integers in the lower 32 bits to four
// unsigned 32-bit integers.
// https://msdn.microsoft.com/en-us/library/bb531467%28v=vs.100%29.aspx
FORCE_INLINE __m128i _mm_cvtepu8_epi32(__m128i a)
{
    uint8x16_t u8x16 = vreinterpretq_u8_m128i(a);      /* xxxx xxxx xxxx DCBA */
    uint16x8_t u16x8 = vmovl_u8(vget_low_u8(u8x16));   /* 0x0x 0x0x 0D0C 0B0A */
    uint32x4_t u32x4 = vmovl_u16(vget_low_u16(u16x8)); /* 000D 000C 000B 000A */
    return vreinterpretq_m128i_u32(u32x4);
}

// Converts the two unsigned 8-bit integers in the lower 16 bits to two
// unsigned 64-bit integers.
FORCE_INLINE __m128i _mm_cvtepu8_epi64(__m128i a)
{
    uint8x16_t u8x16 = vreinterpretq_u8_m128i(a);      /* xxxx xxxx xxxx xxBA */
    uint16x8_t u16x8 = vmovl_u8(vget_low_u8(u8x16));   /* 0x0x 0x0x 0x0x 0B0A */
    uint32x4_t u32x4 = vmovl_u16(vget_low_u16(u16x8)); /* 000x 000x 000B 000A */
    uint64x2_t u64x2 = vmovl_u32(vget_low_u32(u32x4)); /* 0000 000B 0000 000A */
    return vreinterpretq_m128i_u64(u64x2);
}

// Conditionally multiply the packed double-precision (64-bit) floating-point
// elements in a and b using the high 4 bits in imm8, sum the four products, and
// conditionally store the sum in dst using the low 4 bits of imm8.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_dp_pd
FORCE_INLINE __m128d _mm_dp_pd(__m128d a, __m128d b, const int imm)
{
    // Generate mask value from constant immediate bit value
    const int64_t bit0Mask = imm & 0x01 ? UINT64_MAX : 0;
    const int64_t bit1Mask = imm & 0x02 ? UINT64_MAX : 0;
#if !SSE2NEON_PRECISE_DP
    const int64_t bit4Mask = imm & 0x10 ? UINT64_MAX : 0;
    const int64_t bit5Mask = imm & 0x20 ? UINT64_MAX : 0;
#endif
    // Conditional multiplication
#if !SSE2NEON_PRECISE_DP
    __m128d mul = _mm_mul_pd(a, b);
    const __m128d mulMask =
        _mm_castsi128_pd(_mm_set_epi64x(bit5Mask, bit4Mask));
    __m128d tmp = _mm_and_pd(mul, mulMask);
#else
#if defined(__aarch64__)
    double d0 = (imm & 0x10) ? vgetq_lane_f64(vreinterpretq_f64_m128d(a), 0) *
                                   vgetq_lane_f64(vreinterpretq_f64_m128d(b), 0)
                             : 0;
    double d1 = (imm & 0x20) ? vgetq_lane_f64(vreinterpretq_f64_m128d(a), 1) *
                                   vgetq_lane_f64(vreinterpretq_f64_m128d(b), 1)
                             : 0;
#else
    double d0 = (imm & 0x10) ? ((double *) &a)[0] * ((double *) &b)[0] : 0;
    double d1 = (imm & 0x20) ? ((double *) &a)[1] * ((double *) &b)[1] : 0;
#endif
    __m128d tmp = _mm_set_pd(d1, d0);
#endif
    // Sum the products
#if defined(__aarch64__)
    double sum = vpaddd_f64(vreinterpretq_f64_m128d(tmp));
#else
    double sum = *((double *) &tmp) + *(((double *) &tmp) + 1);
#endif
    // Conditionally store the sum
    const __m128d sumMask =
        _mm_castsi128_pd(_mm_set_epi64x(bit1Mask, bit0Mask));
    __m128d res = _mm_and_pd(_mm_set_pd1(sum), sumMask);
    return res;
}

// Conditionally multiply the packed single-precision (32-bit) floating-point
// elements in a and b using the high 4 bits in imm8, sum the four products,
// and conditionally store the sum in dst using the low 4 bits of imm.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_dp_ps
FORCE_INLINE __m128 _mm_dp_ps(__m128 a, __m128 b, const int imm)
{
#if defined(__aarch64__)
    /* shortcuts */
    if (imm == 0xFF) {
        return _mm_set1_ps(vaddvq_f32(_mm_mul_ps(a, b)));
    }
    if (imm == 0x7F) {
        float32x4_t m = _mm_mul_ps(a, b);
        m[3] = 0;
        return _mm_set1_ps(vaddvq_f32(m));
    }
#endif

    float s = 0, c = 0;
    float32x4_t f32a = vreinterpretq_f32_m128(a);
    float32x4_t f32b = vreinterpretq_f32_m128(b);

    /* To improve the accuracy of floating-point summation, Kahan algorithm
     * is used for each operation.
     */
    if (imm & (1 << 4))
        _sse2neon_kadd_f32(&s, &c, f32a[0] * f32b[0]);
    if (imm & (1 << 5))
        _sse2neon_kadd_f32(&s, &c, f32a[1] * f32b[1]);
    if (imm & (1 << 6))
        _sse2neon_kadd_f32(&s, &c, f32a[2] * f32b[2]);
    if (imm & (1 << 7))
        _sse2neon_kadd_f32(&s, &c, f32a[3] * f32b[3]);
    s += c;

    float32x4_t res = {
        (imm & 0x1) ? s : 0,
        (imm & 0x2) ? s : 0,
        (imm & 0x4) ? s : 0,
        (imm & 0x8) ? s : 0,
    };
    return vreinterpretq_m128_f32(res);
}

// Extracts the selected signed or unsigned 32-bit integer from a and zero
// extends.
// FORCE_INLINE int _mm_extract_epi32(__m128i a, __constrange(0,4) int imm)
#define _mm_extract_epi32(a, imm) \
    vgetq_lane_s32(vreinterpretq_s32_m128i(a), (imm))

// Extracts the selected signed or unsigned 64-bit integer from a and zero
// extends.
// FORCE_INLINE __int64 _mm_extract_epi64(__m128i a, __constrange(0,2) int imm)
#define _mm_extract_epi64(a, imm) \
    vgetq_lane_s64(vreinterpretq_s64_m128i(a), (imm))

// Extracts the selected signed or unsigned 8-bit integer from a and zero
// extends.
// FORCE_INLINE int _mm_extract_epi8(__m128i a, __constrange(0,16) int imm)
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_extract_epi8
#define _mm_extract_epi8(a, imm) vgetq_lane_u8(vreinterpretq_u8_m128i(a), (imm))

// Extracts the selected single-precision (32-bit) floating-point from a.
// FORCE_INLINE int _mm_extract_ps(__m128 a, __constrange(0,4) int imm)
#define _mm_extract_ps(a, imm) vgetq_lane_s32(vreinterpretq_s32_m128(a), (imm))

// Round the packed double-precision (64-bit) floating-point elements in a down
// to an integer value, and store the results as packed double-precision
// floating-point elements in dst.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_floor_pd
FORCE_INLINE __m128d _mm_floor_pd(__m128d a)
{
#if defined(__aarch64__)
    return vreinterpretq_m128d_f64(vrndmq_f64(vreinterpretq_f64_m128d(a)));
#else
    double *f = (double *) &a;
    return _mm_set_pd(floor(f[1]), floor(f[0]));
#endif
}

// Round the packed single-precision (32-bit) floating-point elements in a down
// to an integer value, and store the results as packed single-precision
// floating-point elements in dst.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_floor_ps
FORCE_INLINE __m128 _mm_floor_ps(__m128 a)
{
#if defined(__aarch64__) || defined(__ARM_FEATURE_DIRECTED_ROUNDING)
    return vreinterpretq_m128_f32(vrndmq_f32(vreinterpretq_f32_m128(a)));
#else
    float *f = (float *) &a;
    return _mm_set_ps(floorf(f[3]), floorf(f[2]), floorf(f[1]), floorf(f[0]));
#endif
}

// Round the lower double-precision (64-bit) floating-point element in b down to
// an integer value, store the result as a double-precision floating-point
// element in the lower element of dst, and copy the upper element from a to the
// upper element of dst.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_floor_sd
FORCE_INLINE __m128d _mm_floor_sd(__m128d a, __m128d b)
{
    return _mm_move_sd(a, _mm_floor_pd(b));
}

// Round the lower single-precision (32-bit) floating-point element in b down to
// an integer value, store the result as a single-precision floating-point
// element in the lower element of dst, and copy the upper 3 packed elements
// from a to the upper elements of dst.
//
//   dst[31:0] := FLOOR(b[31:0])
//   dst[127:32] := a[127:32]
//
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_floor_ss
FORCE_INLINE __m128 _mm_floor_ss(__m128 a, __m128 b)
{
    return _mm_move_ss(a, _mm_floor_ps(b));
}

// Inserts the least significant 32 bits of b into the selected 32-bit integer
// of a.
// FORCE_INLINE __m128i _mm_insert_epi32(__m128i a, int b,
//                                       __constrange(0,4) int imm)
#define _mm_insert_epi32(a, b, imm)                                  \
    __extension__({                                                  \
        vreinterpretq_m128i_s32(                                     \
            vsetq_lane_s32((b), vreinterpretq_s32_m128i(a), (imm))); \
    })

// Inserts the least significant 64 bits of b into the selected 64-bit integer
// of a.
// FORCE_INLINE __m128i _mm_insert_epi64(__m128i a, __int64 b,
//                                       __constrange(0,2) int imm)
#define _mm_insert_epi64(a, b, imm)                                  \
    __extension__({                                                  \
        vreinterpretq_m128i_s64(                                     \
            vsetq_lane_s64((b), vreinterpretq_s64_m128i(a), (imm))); \
    })

// Inserts the least significant 8 bits of b into the selected 8-bit integer
// of a.
// FORCE_INLINE __m128i _mm_insert_epi8(__m128i a, int b,
//                                      __constrange(0,16) int imm)
#define _mm_insert_epi8(a, b, imm)                                 \
    __extension__({                                                \
        vreinterpretq_m128i_s8(                                    \
            vsetq_lane_s8((b), vreinterpretq_s8_m128i(a), (imm))); \
    })

// Copy a to tmp, then insert a single-precision (32-bit) floating-point
// element from b into tmp using the control in imm8. Store tmp to dst using
// the mask in imm8 (elements are zeroed out when the corresponding bit is set).
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=insert_ps
#define _mm_insert_ps(a, b, imm8)                                              \
    __extension__({                                                            \
        float32x4_t tmp1 =                                                     \
            vsetq_lane_f32(vgetq_lane_f32(b, (imm8 >> 6) & 0x3),               \
                           vreinterpretq_f32_m128(a), 0);                      \
        float32x4_t tmp2 =                                                     \
            vsetq_lane_f32(vgetq_lane_f32(tmp1, 0), vreinterpretq_f32_m128(a), \
                           ((imm8 >> 4) & 0x3));                               \
        const uint32_t data[4] = {((imm8) & (1 << 0)) ? UINT32_MAX : 0,        \
                                  ((imm8) & (1 << 1)) ? UINT32_MAX : 0,        \
                                  ((imm8) & (1 << 2)) ? UINT32_MAX : 0,        \
                                  ((imm8) & (1 << 3)) ? UINT32_MAX : 0};       \
        uint32x4_t mask = vld1q_u32(data);                                     \
        float32x4_t all_zeros = vdupq_n_f32(0);                                \
                                                                               \
        vreinterpretq_m128_f32(                                                \
            vbslq_f32(mask, all_zeros, vreinterpretq_f32_m128(tmp2)));         \
    })

// epi versions of min/max
// Computes the pariwise maximums of the four signed 32-bit integer values of a
// and b.
//
// A 128-bit parameter that can be defined with the following equations:
//   r0 := (a0 > b0) ? a0 : b0
//   r1 := (a1 > b1) ? a1 : b1
//   r2 := (a2 > b2) ? a2 : b2
//   r3 := (a3 > b3) ? a3 : b3
//
// https://msdn.microsoft.com/en-us/library/vstudio/bb514055(v=vs.100).aspx
FORCE_INLINE __m128i _mm_max_epi32(__m128i a, __m128i b)
{
    return vreinterpretq_m128i_s32(
        vmaxq_s32(vreinterpretq_s32_m128i(a), vreinterpretq_s32_m128i(b)));
}

// Compare packed signed 8-bit integers in a and b, and store packed maximum
// values in dst.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_max_epi8
FORCE_INLINE __m128i _mm_max_epi8(__m128i a, __m128i b)
{
    return vreinterpretq_m128i_s8(
        vmaxq_s8(vreinterpretq_s8_m128i(a), vreinterpretq_s8_m128i(b)));
}

// Compare packed unsigned 16-bit integers in a and b, and store packed maximum
// values in dst.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_max_epu16
FORCE_INLINE __m128i _mm_max_epu16(__m128i a, __m128i b)
{
    return vreinterpretq_m128i_u16(
        vmaxq_u16(vreinterpretq_u16_m128i(a), vreinterpretq_u16_m128i(b)));
}

// Compare packed unsigned 32-bit integers in a and b, and store packed maximum
// values in dst.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_max_epu32
FORCE_INLINE __m128i _mm_max_epu32(__m128i a, __m128i b)
{
    return vreinterpretq_m128i_u32(
        vmaxq_u32(vreinterpretq_u32_m128i(a), vreinterpretq_u32_m128i(b)));
}

// Computes the pariwise minima of the four signed 32-bit integer values of a
// and b.
//
// A 128-bit parameter that can be defined with the following equations:
//   r0 := (a0 < b0) ? a0 : b0
//   r1 := (a1 < b1) ? a1 : b1
//   r2 := (a2 < b2) ? a2 : b2
//   r3 := (a3 < b3) ? a3 : b3
//
// https://msdn.microsoft.com/en-us/library/vstudio/bb531476(v=vs.100).aspx
FORCE_INLINE __m128i _mm_min_epi32(__m128i a, __m128i b)
{
    return vreinterpretq_m128i_s32(
        vminq_s32(vreinterpretq_s32_m128i(a), vreinterpretq_s32_m128i(b)));
}

// Compare packed signed 8-bit integers in a and b, and store packed minimum
// values in dst.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_min_epi8
FORCE_INLINE __m128i _mm_min_epi8(__m128i a, __m128i b)
{
    return vreinterpretq_m128i_s8(
        vminq_s8(vreinterpretq_s8_m128i(a), vreinterpretq_s8_m128i(b)));
}

// Compare packed unsigned 16-bit integers in a and b, and store packed minimum
// values in dst.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_min_epu16
FORCE_INLINE __m128i _mm_min_epu16(__m128i a, __m128i b)
{
    return vreinterpretq_m128i_u16(
        vminq_u16(vreinterpretq_u16_m128i(a), vreinterpretq_u16_m128i(b)));
}

// Compare packed unsigned 32-bit integers in a and b, and store packed minimum
// values in dst.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_max_epu32
FORCE_INLINE __m128i _mm_min_epu32(__m128i a, __m128i b)
{
    return vreinterpretq_m128i_u32(
        vminq_u32(vreinterpretq_u32_m128i(a), vreinterpretq_u32_m128i(b)));
}

// Horizontally compute the minimum amongst the packed unsigned 16-bit integers
// in a, store the minimum and index in dst, and zero the remaining bits in dst.
//
//   index[2:0] := 0
//   min[15:0] := a[15:0]
//   FOR j := 0 to 7
//       i := j*16
//       IF a[i+15:i] < min[15:0]
//           index[2:0] := j
//           min[15:0] := a[i+15:i]
//       FI
//   ENDFOR
//   dst[15:0] := min[15:0]
//   dst[18:16] := index[2:0]
//   dst[127:19] := 0
//
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_minpos_epu16
FORCE_INLINE __m128i _mm_minpos_epu16(__m128i a)
{
    __m128i dst;
    uint16_t min, idx = 0;
#if defined(__aarch64__)
    // Find the minimum value
    min = vminvq_u16(vreinterpretq_u16_m128i(a));

    // Get the index of the minimum value
    static const uint16_t idxv[] = {0, 1, 2, 3, 4, 5, 6, 7};
    uint16x8_t minv = vdupq_n_u16(min);
    uint16x8_t cmeq = vceqq_u16(minv, vreinterpretq_u16_m128i(a));
    idx = vminvq_u16(vornq_u16(vld1q_u16(idxv), cmeq));
#else
    // Find the minimum value
    __m64 tmp;
    tmp = vreinterpret_m64_u16(
        vmin_u16(vget_low_u16(vreinterpretq_u16_m128i(a)),
                 vget_high_u16(vreinterpretq_u16_m128i(a))));
    tmp = vreinterpret_m64_u16(
        vpmin_u16(vreinterpret_u16_m64(tmp), vreinterpret_u16_m64(tmp)));
    tmp = vreinterpret_m64_u16(
        vpmin_u16(vreinterpret_u16_m64(tmp), vreinterpret_u16_m64(tmp)));
    min = vget_lane_u16(vreinterpret_u16_m64(tmp), 0);
    // Get the index of the minimum value
    int i;
    for (i = 0; i < 8; i++) {
        if (min == vgetq_lane_u16(vreinterpretq_u16_m128i(a), 0)) {
            idx = (uint16_t) i;
            break;
        }
        a = _mm_srli_si128(a, 2);
    }
#endif
    // Generate result
    dst = _mm_setzero_si128();
    dst = vreinterpretq_m128i_u16(
        vsetq_lane_u16(min, vreinterpretq_u16_m128i(dst), 0));
    dst = vreinterpretq_m128i_u16(
        vsetq_lane_u16(idx, vreinterpretq_u16_m128i(dst), 1));
    return dst;
}

// Compute the sum of absolute differences (SADs) of quadruplets of unsigned
// 8-bit integers in a compared to those in b, and store the 16-bit results in
// dst. Eight SADs are performed using one quadruplet from b and eight
// quadruplets from a. One quadruplet is selected from b starting at on the
// offset specified in imm8. Eight quadruplets are formed from sequential 8-bit
// integers selected from a starting at the offset specified in imm8.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_mpsadbw_epu8
FORCE_INLINE __m128i _mm_mpsadbw_epu8(__m128i a, __m128i b, const int imm)
{
    uint8x16_t _a, _b;

    switch (imm & 0x4) {
    case 0:
        // do nothing
        _a = vreinterpretq_u8_m128i(a);
        break;
    case 4:
        _a = vreinterpretq_u8_u32(vextq_u32(vreinterpretq_u32_m128i(a),
                                            vreinterpretq_u32_m128i(a), 1));
        break;
    default:
#if defined(__GNUC__) || defined(__clang__)
        __builtin_unreachable();
#endif
        break;
    }

    switch (imm & 0x3) {
    case 0:
        _b = vreinterpretq_u8_u32(
            vdupq_n_u32(vgetq_lane_u32(vreinterpretq_u32_m128i(b), 0)));
        break;
    case 1:
        _b = vreinterpretq_u8_u32(
            vdupq_n_u32(vgetq_lane_u32(vreinterpretq_u32_m128i(b), 1)));
        break;
    case 2:
        _b = vreinterpretq_u8_u32(
            vdupq_n_u32(vgetq_lane_u32(vreinterpretq_u32_m128i(b), 2)));
        break;
    case 3:
        _b = vreinterpretq_u8_u32(
            vdupq_n_u32(vgetq_lane_u32(vreinterpretq_u32_m128i(b), 3)));
        break;
    default:
#if defined(__GNUC__) || defined(__clang__)
        __builtin_unreachable();
#endif
        break;
    }

    int16x8_t c04, c15, c26, c37;
    uint8x8_t low_b = vget_low_u8(_b);
    c04 = vreinterpretq_s16_u16(vabdl_u8(vget_low_u8(_a), low_b));
    uint8x16_t _a_1 = vextq_u8(_a, _a, 1);
    c15 = vreinterpretq_s16_u16(vabdl_u8(vget_low_u8(_a_1), low_b));
    uint8x16_t _a_2 = vextq_u8(_a, _a, 2);
    c26 = vreinterpretq_s16_u16(vabdl_u8(vget_low_u8(_a_2), low_b));
    uint8x16_t _a_3 = vextq_u8(_a, _a, 3);
    c37 = vreinterpretq_s16_u16(vabdl_u8(vget_low_u8(_a_3), low_b));
#if defined(__aarch64__)
    // |0|4|2|6|
    c04 = vpaddq_s16(c04, c26);
    // |1|5|3|7|
    c15 = vpaddq_s16(c15, c37);

    int32x4_t trn1_c =
        vtrn1q_s32(vreinterpretq_s32_s16(c04), vreinterpretq_s32_s16(c15));
    int32x4_t trn2_c =
        vtrn2q_s32(vreinterpretq_s32_s16(c04), vreinterpretq_s32_s16(c15));
    return vreinterpretq_m128i_s16(vpaddq_s16(vreinterpretq_s16_s32(trn1_c),
                                              vreinterpretq_s16_s32(trn2_c)));
#else
    int16x4_t c01, c23, c45, c67;
    c01 = vpadd_s16(vget_low_s16(c04), vget_low_s16(c15));
    c23 = vpadd_s16(vget_low_s16(c26), vget_low_s16(c37));
    c45 = vpadd_s16(vget_high_s16(c04), vget_high_s16(c15));
    c67 = vpadd_s16(vget_high_s16(c26), vget_high_s16(c37));

    return vreinterpretq_m128i_s16(
        vcombine_s16(vpadd_s16(c01, c23), vpadd_s16(c45, c67)));
#endif
}

// Multiply the low signed 32-bit integers from each packed 64-bit element in
// a and b, and store the signed 64-bit results in dst.
//
//   r0 :=  (int64_t)(int32_t)a0 * (int64_t)(int32_t)b0
//   r1 :=  (int64_t)(int32_t)a2 * (int64_t)(int32_t)b2
FORCE_INLINE __m128i _mm_mul_epi32(__m128i a, __m128i b)
{
    // vmull_s32 upcasts instead of masking, so we downcast.
    int32x2_t a_lo = vmovn_s64(vreinterpretq_s64_m128i(a));
    int32x2_t b_lo = vmovn_s64(vreinterpretq_s64_m128i(b));
    return vreinterpretq_m128i_s64(vmull_s32(a_lo, b_lo));
}

// Multiplies the 4 signed or unsigned 32-bit integers from a by the 4 signed or
// unsigned 32-bit integers from b.
// https://msdn.microsoft.com/en-us/library/vstudio/bb531409(v=vs.100).aspx
FORCE_INLINE __m128i _mm_mullo_epi32(__m128i a, __m128i b)
{
    return vreinterpretq_m128i_s32(
        vmulq_s32(vreinterpretq_s32_m128i(a), vreinterpretq_s32_m128i(b)));
}

// Packs the 8 unsigned 32-bit integers from a and b into unsigned 16-bit
// integers and saturates.
//
//   r0 := UnsignedSaturate(a0)
//   r1 := UnsignedSaturate(a1)
//   r2 := UnsignedSaturate(a2)
//   r3 := UnsignedSaturate(a3)
//   r4 := UnsignedSaturate(b0)
//   r5 := UnsignedSaturate(b1)
//   r6 := UnsignedSaturate(b2)
//   r7 := UnsignedSaturate(b3)
FORCE_INLINE __m128i _mm_packus_epi32(__m128i a, __m128i b)
{
    return vreinterpretq_m128i_u16(
        vcombine_u16(vqmovun_s32(vreinterpretq_s32_m128i(a)),
                     vqmovun_s32(vreinterpretq_s32_m128i(b))));
}

// Round the packed double-precision (64-bit) floating-point elements in a using
// the rounding parameter, and store the results as packed double-precision
// floating-point elements in dst.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_round_pd
FORCE_INLINE __m128d _mm_round_pd(__m128d a, int rounding)
{
#if defined(__aarch64__)
    switch (rounding) {
    case (_MM_FROUND_TO_NEAREST_INT | _MM_FROUND_NO_EXC):
        return vreinterpretq_m128d_f64(vrndnq_f64(vreinterpretq_f64_m128d(a)));
    case (_MM_FROUND_TO_NEG_INF | _MM_FROUND_NO_EXC):
        return _mm_floor_pd(a);
    case (_MM_FROUND_TO_POS_INF | _MM_FROUND_NO_EXC):
        return _mm_ceil_pd(a);
    case (_MM_FROUND_TO_ZERO | _MM_FROUND_NO_EXC):
        return vreinterpretq_m128d_f64(vrndq_f64(vreinterpretq_f64_m128d(a)));
    default:  //_MM_FROUND_CUR_DIRECTION
        return vreinterpretq_m128d_f64(vrndiq_f64(vreinterpretq_f64_m128d(a)));
    }
#else
    double *v_double = (double *) &a;

    if (rounding == (_MM_FROUND_TO_NEAREST_INT | _MM_FROUND_NO_EXC) ||
        (rounding == _MM_FROUND_CUR_DIRECTION &&
         _MM_GET_ROUNDING_MODE() == _MM_ROUND_NEAREST)) {
        double res[2], tmp;
        for (int i = 0; i < 2; i++) {
            tmp = (v_double[i] < 0) ? -v_double[i] : v_double[i];
            double roundDown = floor(tmp);  // Round down value
            double roundUp = ceil(tmp);     // Round up value
            double diffDown = tmp - roundDown;
            double diffUp = roundUp - tmp;
            if (diffDown < diffUp) {
                /* If it's closer to the round down value, then use it */
                res[i] = roundDown;
            } else if (diffDown > diffUp) {
                /* If it's closer to the round up value, then use it */
                res[i] = roundUp;
            } else {
                /* If it's equidistant between round up and round down value,
                 * pick the one which is an even number */
                double half = roundDown / 2;
                if (half != floor(half)) {
                    /* If the round down value is odd, return the round up value
                     */
                    res[i] = roundUp;
                } else {
                    /* If the round up value is odd, return the round down value
                     */
                    res[i] = roundDown;
                }
            }
            res[i] = (v_double[i] < 0) ? -res[i] : res[i];
        }
        return _mm_set_pd(res[1], res[0]);
    } else if (rounding == (_MM_FROUND_TO_NEG_INF | _MM_FROUND_NO_EXC) ||
               (rounding == _MM_FROUND_CUR_DIRECTION &&
                _MM_GET_ROUNDING_MODE() == _MM_ROUND_DOWN)) {
        return _mm_floor_pd(a);
    } else if (rounding == (_MM_FROUND_TO_POS_INF | _MM_FROUND_NO_EXC) ||
               (rounding == _MM_FROUND_CUR_DIRECTION &&
                _MM_GET_ROUNDING_MODE() == _MM_ROUND_UP)) {
        return _mm_ceil_pd(a);
    }
    return _mm_set_pd(v_double[1] > 0 ? floor(v_double[1]) : ceil(v_double[1]),
                      v_double[0] > 0 ? floor(v_double[0]) : ceil(v_double[0]));
#endif
}

// Round the packed single-precision (32-bit) floating-point elements in a using
// the rounding parameter, and store the results as packed single-precision
// floating-point elements in dst.
// software.intel.com/sites/landingpage/IntrinsicsGuide/#text=_mm_round_ps
FORCE_INLINE __m128 _mm_round_ps(__m128 a, int rounding)
{
#if defined(__aarch64__) || defined(__ARM_FEATURE_DIRECTED_ROUNDING)
    switch (rounding) {
    case (_MM_FROUND_TO_NEAREST_INT | _MM_FROUND_NO_EXC):
        return vreinterpretq_m128_f32(vrndnq_f32(vreinterpretq_f32_m128(a)));
    case (_MM_FROUND_TO_NEG_INF | _MM_FROUND_NO_EXC):
        return _mm_floor_ps(a);
    case (_MM_FROUND_TO_POS_INF | _MM_FROUND_NO_EXC):
        return _mm_ceil_ps(a);
    case (_MM_FROUND_TO_ZERO | _MM_FROUND_NO_EXC):
        return vreinterpretq_m128_f32(vrndq_f32(vreinterpretq_f32_m128(a)));
    default:  //_MM_FROUND_CUR_DIRECTION
        return vreinterpretq_m128_f32(vrndiq_f32(vreinterpretq_f32_m128(a)));
    }
#else
    float *v_float = (float *) &a;

    if (rounding == (_MM_FROUND_TO_NEAREST_INT | _MM_FROUND_NO_EXC) ||
        (rounding == _MM_FROUND_CUR_DIRECTION &&
         _MM_GET_ROUNDING_MODE() == _MM_ROUND_NEAREST)) {
        uint32x4_t signmask = vdupq_n_u32(0x80000000);
        float32x4_t half = vbslq_f32(signmask, vreinterpretq_f32_m128(a),
                                     vdupq_n_f32(0.5f)); /* +/- 0.5 */
        int32x4_t r_normal = vcvtq_s32_f32(vaddq_f32(
            vreinterpretq_f32_m128(a), half)); /* round to integer: [a + 0.5]*/
        int32x4_t r_trunc = vcvtq_s32_f32(
            vreinterpretq_f32_m128(a)); /* truncate to integer: [a] */
        int32x4_t plusone = vreinterpretq_s32_u32(vshrq_n_u32(
            vreinterpretq_u32_s32(vnegq_s32(r_trunc)), 31)); /* 1 or 0 */
        int32x4_t r_even = vbicq_s32(vaddq_s32(r_trunc, plusone),
                                     vdupq_n_s32(1)); /* ([a] + {0,1}) & ~1 */
        float32x4_t delta = vsubq_f32(
            vreinterpretq_f32_m128(a),
            vcvtq_f32_s32(r_trunc)); /* compute delta: delta = (a - [a]) */
        uint32x4_t is_delta_half =
            vceqq_f32(delta, half); /* delta == +/- 0.5 */
        return vreinterpretq_m128_f32(
            vcvtq_f32_s32(vbslq_s32(is_delta_half, r_even, r_normal)));
    } else if (rounding == (_MM_FROUND_TO_NEG_INF | _MM_FROUND_NO_EXC) ||
               (rounding == _MM_FROUND_CUR_DIRECTION &&
                _MM_GET_ROUNDING_MODE() == _MM_ROUND_DOWN)) {
        return _mm_floor_ps(a);
    } else if (rounding == (_MM_FROUND_TO_POS_INF | _MM_FROUND_NO_EXC) ||
               (rounding == _MM_FROUND_CUR_DIRECTION &&
                _MM_GET_ROUNDING_MODE() == _MM_ROUND_UP)) {
        return _mm_ceil_ps(a);
    }
    return _mm_set_ps(v_float[3] > 0 ? floorf(v_float[3]) : ceilf(v_float[3]),
                      v_float[2] > 0 ? floorf(v_float[2]) : ceilf(v_float[2]),
                      v_float[1] > 0 ? floorf(v_float[1]) : ceilf(v_float[1]),
                      v_float[0] > 0 ? floorf(v_float[0]) : ceilf(v_float[0]));
#endif
}

// Round the lower double-precision (64-bit) floating-point element in b using
// the rounding parameter, store the result as a double-precision floating-point
// element in the lower element of dst, and copy the upper element from a to the
// upper element of dst.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_round_sd
FORCE_INLINE __m128d _mm_round_sd(__m128d a, __m128d b, int rounding)
{
    return _mm_move_sd(a, _mm_round_pd(b, rounding));
}

// Round the lower single-precision (32-bit) floating-point element in b using
// the rounding parameter, store the result as a single-precision floating-point
// element in the lower element of dst, and copy the upper 3 packed elements
// from a to the upper elements of dst. Rounding is done according to the
// rounding[3:0] parameter, which can be one of:
//     (_MM_FROUND_TO_NEAREST_INT |_MM_FROUND_NO_EXC) // round to nearest, and
//     suppress exceptions
//     (_MM_FROUND_TO_NEG_INF |_MM_FROUND_NO_EXC)     // round down, and
//     suppress exceptions
//     (_MM_FROUND_TO_POS_INF |_MM_FROUND_NO_EXC)     // round up, and suppress
//     exceptions
//     (_MM_FROUND_TO_ZERO |_MM_FROUND_NO_EXC)        // truncate, and suppress
//     exceptions _MM_FROUND_CUR_DIRECTION // use MXCSR.RC; see
//     _MM_SET_ROUNDING_MODE
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_round_ss
FORCE_INLINE __m128 _mm_round_ss(__m128 a, __m128 b, int rounding)
{
    return _mm_move_ss(a, _mm_round_ps(b, rounding));
}

// Load 128-bits of integer data from memory into dst using a non-temporal
// memory hint. mem_addr must be aligned on a 16-byte boundary or a
// general-protection exception may be generated.
//
//   dst[127:0] := MEM[mem_addr+127:mem_addr]
//
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_stream_load_si128
FORCE_INLINE __m128i _mm_stream_load_si128(__m128i *p)
{
#if __has_builtin(__builtin_nontemporal_store)
    return __builtin_nontemporal_load(p);
#else
    return vreinterpretq_m128i_s64(vld1q_s64((int64_t *) p));
#endif
}

// Compute the bitwise NOT of a and then AND with a 128-bit vector containing
// all 1's, and return 1 if the result is zero, otherwise return 0.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_test_all_ones
FORCE_INLINE int _mm_test_all_ones(__m128i a)
{
    return (uint64_t) (vgetq_lane_s64(a, 0) & vgetq_lane_s64(a, 1)) ==
           ~(uint64_t) 0;
}

// Compute the bitwise AND of 128 bits (representing integer data) in a and
// mask, and return 1 if the result is zero, otherwise return 0.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_test_all_zeros
FORCE_INLINE int _mm_test_all_zeros(__m128i a, __m128i mask)
{
    int64x2_t a_and_mask =
        vandq_s64(vreinterpretq_s64_m128i(a), vreinterpretq_s64_m128i(mask));
    return !(vgetq_lane_s64(a_and_mask, 0) | vgetq_lane_s64(a_and_mask, 1));
}

// Compute the bitwise AND of 128 bits (representing integer data) in a and
// mask, and set ZF to 1 if the result is zero, otherwise set ZF to 0. Compute
// the bitwise NOT of a and then AND with mask, and set CF to 1 if the result is
// zero, otherwise set CF to 0. Return 1 if both the ZF and CF values are zero,
// otherwise return 0.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=mm_test_mix_ones_zero
FORCE_INLINE int _mm_test_mix_ones_zeros(__m128i a, __m128i mask)
{
    uint64x2_t zf =
        vandq_u64(vreinterpretq_u64_m128i(mask), vreinterpretq_u64_m128i(a));
    uint64x2_t cf =
        vbicq_u64(vreinterpretq_u64_m128i(mask), vreinterpretq_u64_m128i(a));
    uint64x2_t result = vandq_u64(zf, cf);
    return !(vgetq_lane_u64(result, 0) | vgetq_lane_u64(result, 1));
}

// Compute the bitwise AND of 128 bits (representing integer data) in a and b,
// and set ZF to 1 if the result is zero, otherwise set ZF to 0. Compute the
// bitwise NOT of a and then AND with b, and set CF to 1 if the result is zero,
// otherwise set CF to 0. Return the CF value.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_testc_si128
FORCE_INLINE int _mm_testc_si128(__m128i a, __m128i b)
{
    int64x2_t s64 =
        vbicq_s64(vreinterpretq_s64_m128i(b), vreinterpretq_s64_m128i(a));
    return !(vgetq_lane_s64(s64, 0) | vgetq_lane_s64(s64, 1));
}

// Compute the bitwise AND of 128 bits (representing integer data) in a and b,
// and set ZF to 1 if the result is zero, otherwise set ZF to 0. Compute the
// bitwise NOT of a and then AND with b, and set CF to 1 if the result is zero,
// otherwise set CF to 0. Return 1 if both the ZF and CF values are zero,
// otherwise return 0.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_testnzc_si128
#define _mm_testnzc_si128(a, b) _mm_test_mix_ones_zeros(a, b)

// Compute the bitwise AND of 128 bits (representing integer data) in a and b,
// and set ZF to 1 if the result is zero, otherwise set ZF to 0. Compute the
// bitwise NOT of a and then AND with b, and set CF to 1 if the result is zero,
// otherwise set CF to 0. Return the ZF value.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_testz_si128
FORCE_INLINE int _mm_testz_si128(__m128i a, __m128i b)
{
    int64x2_t s64 =
        vandq_s64(vreinterpretq_s64_m128i(a), vreinterpretq_s64_m128i(b));
    return !(vgetq_lane_s64(s64, 0) | vgetq_lane_s64(s64, 1));
}

/* SSE4.2 */

const static uint16_t _sse2neon_cmpestr_mask16b[8] ALIGN_STRUCT(16) = {
    0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80,
};
const static uint8_t _sse2neon_cmpestr_mask8b[16] ALIGN_STRUCT(16) = {
    0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80,
    0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80,
};

/* specify the source data format */
#define _SIDD_UBYTE_OPS 0x00 /* unsigned 8-bit characters */
#define _SIDD_UWORD_OPS 0x01 /* unsigned 16-bit characters */
#define _SIDD_SBYTE_OPS 0x02 /* signed 8-bit characters */
#define _SIDD_SWORD_OPS 0x03 /* signed 16-bit characters */

/* specify the comparison operation */
#define _SIDD_CMP_EQUAL_ANY 0x00     /* compare equal any: strchr */
#define _SIDD_CMP_RANGES 0x04        /* compare ranges */
#define _SIDD_CMP_EQUAL_EACH 0x08    /* compare equal each: strcmp */
#define _SIDD_CMP_EQUAL_ORDERED 0x0C /* compare equal ordered */

/* specify the polarity */
#define _SIDD_POSITIVE_POLARITY 0x00
#define _SIDD_MASKED_POSITIVE_POLARITY 0x20
#define _SIDD_NEGATIVE_POLARITY 0x10 /* negate results */
#define _SIDD_MASKED_NEGATIVE_POLARITY \
    0x30 /* negate results only before end of string */

/* specify the output selection in _mm_cmpXstri */
#define _SIDD_LEAST_SIGNIFICANT 0x00
#define _SIDD_MOST_SIGNIFICANT 0x40

/* specify the output selection in _mm_cmpXstrm */
#define _SIDD_BIT_MASK 0x00
#define _SIDD_UNIT_MASK 0x40

/* Pattern Matching for C macros.
 * https://github.com/pfultz2/Cloak/wiki/C-Preprocessor-tricks,-tips,-and-idioms
 */

/* catenate */
#define SSE2NEON_PRIMITIVE_CAT(a, ...) a##__VA_ARGS__
#define SSE2NEON_CAT(a, b) SSE2NEON_PRIMITIVE_CAT(a, b)

#define SSE2NEON_IIF(c) SSE2NEON_PRIMITIVE_CAT(SSE2NEON_IIF_, c)
/* run the 2nd parameter */
#define SSE2NEON_IIF_0(t, ...) __VA_ARGS__
/* run the 1st parameter */
#define SSE2NEON_IIF_1(t, ...) t

#define SSE2NEON_COMPL(b) SSE2NEON_PRIMITIVE_CAT(SSE2NEON_COMPL_, b)
#define SSE2NEON_COMPL_0 1
#define SSE2NEON_COMPL_1 0

#define SSE2NEON_DEC(x) SSE2NEON_PRIMITIVE_CAT(SSE2NEON_DEC_, x)
#define SSE2NEON_DEC_1 0
#define SSE2NEON_DEC_2 1
#define SSE2NEON_DEC_3 2
#define SSE2NEON_DEC_4 3
#define SSE2NEON_DEC_5 4
#define SSE2NEON_DEC_6 5
#define SSE2NEON_DEC_7 6
#define SSE2NEON_DEC_8 7
#define SSE2NEON_DEC_9 8
#define SSE2NEON_DEC_10 9
#define SSE2NEON_DEC_11 10
#define SSE2NEON_DEC_12 11
#define SSE2NEON_DEC_13 12
#define SSE2NEON_DEC_14 13
#define SSE2NEON_DEC_15 14
#define SSE2NEON_DEC_16 15

/* detection */
#define SSE2NEON_CHECK_N(x, n, ...) n
#define SSE2NEON_CHECK(...) SSE2NEON_CHECK_N(__VA_ARGS__, 0, )
#define SSE2NEON_PROBE(x) x, 1,

#define SSE2NEON_NOT(x) SSE2NEON_CHECK(SSE2NEON_PRIMITIVE_CAT(SSE2NEON_NOT_, x))
#define SSE2NEON_NOT_0 SSE2NEON_PROBE(~)

#define SSE2NEON_BOOL(x) SSE2NEON_COMPL(SSE2NEON_NOT(x))
#define SSE2NEON_IF(c) SSE2NEON_IIF(SSE2NEON_BOOL(c))

#define SSE2NEON_EAT(...)
#define SSE2NEON_EXPAND(...) __VA_ARGS__
#define SSE2NEON_WHEN(c) SSE2NEON_IF(c)(SSE2NEON_EXPAND, SSE2NEON_EAT)

/* recursion */
/* deferred expression */
#define SSE2NEON_EMPTY()
#define SSE2NEON_DEFER(id) id SSE2NEON_EMPTY()
#define SSE2NEON_OBSTRUCT(...) __VA_ARGS__ SSE2NEON_DEFER(SSE2NEON_EMPTY)()
#define SSE2NEON_EXPAND(...) __VA_ARGS__

#define SSE2NEON_EVAL(...) \
    SSE2NEON_EVAL1(SSE2NEON_EVAL1(SSE2NEON_EVAL1(__VA_ARGS__)))
#define SSE2NEON_EVAL1(...) \
    SSE2NEON_EVAL2(SSE2NEON_EVAL2(SSE2NEON_EVAL2(__VA_ARGS__)))
#define SSE2NEON_EVAL2(...) \
    SSE2NEON_EVAL3(SSE2NEON_EVAL3(SSE2NEON_EVAL3(__VA_ARGS__)))
#define SSE2NEON_EVAL3(...) __VA_ARGS__

#define SSE2NEON_REPEAT(count, macro, ...)                         \
    SSE2NEON_WHEN(count)                                           \
    (SSE2NEON_OBSTRUCT(SSE2NEON_REPEAT_INDIRECT)()(                \
        SSE2NEON_DEC(count), macro,                                \
        __VA_ARGS__) SSE2NEON_OBSTRUCT(macro)(SSE2NEON_DEC(count), \
                                              __VA_ARGS__))
#define SSE2NEON_REPEAT_INDIRECT() SSE2NEON_REPEAT

#define SSE2NEON_SIZE_OF_byte 8
#define SSE2NEON_NUMBER_OF_LANES_byte 16
#define SSE2NEON_SIZE_OF_word 16
#define SSE2NEON_NUMBER_OF_LANES_word 8

#define SSE2NEON_COMPARE_EQUAL_THEN_FILL_LANE(i, type)                         \
    mtx[i] = vreinterpretq_m128i_##type(vceqq_##type(                          \
        vdupq_n_##type(vgetq_lane_##type(vreinterpretq_##type##_m128i(b), i)), \
        vreinterpretq_##type##_m128i(a)));

#define SSE2NEON_FILL_LANE(i, type) \
    vec_b[i] =                      \
        vdupq_n_##type(vgetq_lane_##type(vreinterpretq_##type##_m128i(b), i));

#define PCMPSTR_RANGES(a, b, mtx, data_type_prefix, type_prefix, size,        \
                       number_of_lanes, byte_or_word)                         \
    do {                                                                      \
        SSE2NEON_CAT(                                                         \
            data_type_prefix,                                                 \
            SSE2NEON_CAT(size,                                                \
                         SSE2NEON_CAT(x, SSE2NEON_CAT(number_of_lanes, _t)))) \
        vec_b[number_of_lanes];                                               \
        __m128i mask = SSE2NEON_IIF(byte_or_word)(                            \
            vreinterpretq_m128i_u16(vdupq_n_u16(0xff)),                       \
            vreinterpretq_m128i_u32(vdupq_n_u32(0xffff)));                    \
        SSE2NEON_EVAL(SSE2NEON_REPEAT(number_of_lanes, SSE2NEON_FILL_LANE,    \
                                      SSE2NEON_CAT(type_prefix, size)))       \
        for (int i = 0; i < number_of_lanes; i++) {                           \
            mtx[i] = SSE2NEON_CAT(vreinterpretq_m128i_u,                      \
                                  size)(SSE2NEON_CAT(vbslq_u, size)(          \
                SSE2NEON_CAT(vreinterpretq_u,                                 \
                             SSE2NEON_CAT(size, _m128i))(mask),               \
                SSE2NEON_CAT(vcgeq_, SSE2NEON_CAT(type_prefix, size))(        \
                    vec_b[i],                                                 \
                    SSE2NEON_CAT(                                             \
                        vreinterpretq_,                                       \
                        SSE2NEON_CAT(type_prefix,                             \
                                     SSE2NEON_CAT(size, _m128i(a))))),        \
                SSE2NEON_CAT(vcleq_, SSE2NEON_CAT(type_prefix, size))(        \
                    vec_b[i],                                                 \
                    SSE2NEON_CAT(                                             \
                        vreinterpretq_,                                       \
                        SSE2NEON_CAT(type_prefix,                             \
                                     SSE2NEON_CAT(size, _m128i(a)))))));      \
        }                                                                     \
    } while (0)

#define PCMPSTR_EQ(a, b, mtx, size, number_of_lanes)                         \
    do {                                                                     \
        SSE2NEON_EVAL(SSE2NEON_REPEAT(number_of_lanes,                       \
                                      SSE2NEON_COMPARE_EQUAL_THEN_FILL_LANE, \
                                      SSE2NEON_CAT(u, size)))                \
    } while (0)

#define SSE2NEON_CMP_EQUAL_ANY_IMPL(type)                                     \
    static int _sse2neon_cmp_##type##_equal_any(__m128i a, int la, __m128i b, \
                                                int lb)                       \
    {                                                                         \
        __m128i mtx[16];                                                      \
        PCMPSTR_EQ(a, b, mtx, SSE2NEON_CAT(SSE2NEON_SIZE_OF_, type),          \
                   SSE2NEON_CAT(SSE2NEON_NUMBER_OF_LANES_, type));            \
        return SSE2NEON_CAT(                                                  \
            _sse2neon_aggregate_equal_any_,                                   \
            SSE2NEON_CAT(                                                     \
                SSE2NEON_CAT(SSE2NEON_SIZE_OF_, type),                        \
                SSE2NEON_CAT(x, SSE2NEON_CAT(SSE2NEON_NUMBER_OF_LANES_,       \
                                             type))))(la, lb, mtx);           \
    }

#define SSE2NEON_CMP_RANGES_IMPL(type, data_type, us, byte_or_word)            \
    static int _sse2neon_cmp_##us##type##_ranges(__m128i a, int la, __m128i b, \
                                                 int lb)                       \
    {                                                                          \
        __m128i mtx[16];                                                       \
        PCMPSTR_RANGES(                                                        \
            a, b, mtx, data_type, us, SSE2NEON_CAT(SSE2NEON_SIZE_OF_, type),   \
            SSE2NEON_CAT(SSE2NEON_NUMBER_OF_LANES_, type), byte_or_word);      \
        return SSE2NEON_CAT(                                                   \
            _sse2neon_aggregate_ranges_,                                       \
            SSE2NEON_CAT(                                                      \
                SSE2NEON_CAT(SSE2NEON_SIZE_OF_, type),                         \
                SSE2NEON_CAT(x, SSE2NEON_CAT(SSE2NEON_NUMBER_OF_LANES_,        \
                                             type))))(la, lb, mtx);            \
    }

#define SSE2NEON_CMP_EQUAL_ORDERED_IMPL(type)                                  \
    static int _sse2neon_cmp_##type##_equal_ordered(__m128i a, int la,         \
                                                    __m128i b, int lb)         \
    {                                                                          \
        __m128i mtx[16];                                                       \
        PCMPSTR_EQ(a, b, mtx, SSE2NEON_CAT(SSE2NEON_SIZE_OF_, type),           \
                   SSE2NEON_CAT(SSE2NEON_NUMBER_OF_LANES_, type));             \
        return SSE2NEON_CAT(                                                   \
            _sse2neon_aggregate_equal_ordered_,                                \
            SSE2NEON_CAT(                                                      \
                SSE2NEON_CAT(SSE2NEON_SIZE_OF_, type),                         \
                SSE2NEON_CAT(x,                                                \
                             SSE2NEON_CAT(SSE2NEON_NUMBER_OF_LANES_, type))))( \
            SSE2NEON_CAT(SSE2NEON_NUMBER_OF_LANES_, type), la, lb, mtx);       \
    }

static int _sse2neon_aggregate_equal_any_8x16(int la, int lb, __m128i mtx[16])
{
    int res = 0;
    int m = (1 << la) - 1;
    uint8x8_t vec_mask = vld1_u8(_sse2neon_cmpestr_mask8b);
    uint8x8_t t_lo = vtst_u8(vdup_n_u8(m & 0xff), vec_mask);
    uint8x8_t t_hi = vtst_u8(vdup_n_u8(m >> 8), vec_mask);
    uint8x16_t vec = vcombine_u8(t_lo, t_hi);
    for (int j = 0; j < lb; j++) {
        mtx[j] = vreinterpretq_m128i_u8(
            vandq_u8(vec, vreinterpretq_u8_m128i(mtx[j])));
        mtx[j] = vreinterpretq_m128i_u8(
            vshrq_n_u8(vreinterpretq_u8_m128i(mtx[j]), 7));
        int tmp = _sse2neon_vaddvq_u8(vreinterpretq_u8_m128i(mtx[j])) ? 1 : 0;
        res |= (tmp << j);
    }
    return res;
}

static int _sse2neon_aggregate_equal_any_16x8(int la, int lb, __m128i mtx[16])
{
    int res = 0;
    int m = (1 << la) - 1;
    uint16x8_t vec =
        vtstq_u16(vdupq_n_u16(m), vld1q_u16(_sse2neon_cmpestr_mask16b));
    for (int j = 0; j < lb; j++) {
        mtx[j] = vreinterpretq_m128i_u16(
            vandq_u16(vec, vreinterpretq_u16_m128i(mtx[j])));
        mtx[j] = vreinterpretq_m128i_u16(
            vshrq_n_u16(vreinterpretq_u16_m128i(mtx[j]), 15));
        int tmp = _sse2neon_vaddvq_u16(vreinterpretq_u16_m128i(mtx[j])) ? 1 : 0;
        res |= (tmp << j);
    }
    return res;
}

/* clang-format off */
#define SSE2NEON_GENERATE_CMP_EQUAL_ANY(prefix) \
    prefix##IMPL(byte) \
    prefix##IMPL(word)
/* clang-format on */

SSE2NEON_GENERATE_CMP_EQUAL_ANY(SSE2NEON_CMP_EQUAL_ANY_)

static int _sse2neon_aggregate_ranges_16x8(int la, int lb, __m128i mtx[16])
{
    int res = 0;
    int m = (1 << la) - 1;
    uint16x8_t vec =
        vtstq_u16(vdupq_n_u16(m), vld1q_u16(_sse2neon_cmpestr_mask16b));
    for (int j = 0; j < lb; j++) {
        mtx[j] = vreinterpretq_m128i_u16(
            vandq_u16(vec, vreinterpretq_u16_m128i(mtx[j])));
        mtx[j] = vreinterpretq_m128i_u16(
            vshrq_n_u16(vreinterpretq_u16_m128i(mtx[j]), 15));
        __m128i tmp = vreinterpretq_m128i_u32(
            vshrq_n_u32(vreinterpretq_u32_m128i(mtx[j]), 16));
        uint32x4_t vec_res = vandq_u32(vreinterpretq_u32_m128i(mtx[j]),
                                       vreinterpretq_u32_m128i(tmp));
#if defined(__aarch64__)
        int t = vaddvq_u32(vec_res) ? 1 : 0;
#else
        uint64x2_t sumh = vpaddlq_u32(vec_res);
        int t = vgetq_lane_u64(sumh, 0) + vgetq_lane_u64(sumh, 1);
#endif
        res |= (t << j);
    }
    return res;
}

static int _sse2neon_aggregate_ranges_8x16(int la, int lb, __m128i mtx[16])
{
    int res = 0;
    int m = (1 << la) - 1;
    uint8x8_t vec_mask = vld1_u8(_sse2neon_cmpestr_mask8b);
    uint8x8_t t_lo = vtst_u8(vdup_n_u8(m & 0xff), vec_mask);
    uint8x8_t t_hi = vtst_u8(vdup_n_u8(m >> 8), vec_mask);
    uint8x16_t vec = vcombine_u8(t_lo, t_hi);
    for (int j = 0; j < lb; j++) {
        mtx[j] = vreinterpretq_m128i_u8(
            vandq_u8(vec, vreinterpretq_u8_m128i(mtx[j])));
        mtx[j] = vreinterpretq_m128i_u8(
            vshrq_n_u8(vreinterpretq_u8_m128i(mtx[j]), 7));
        __m128i tmp = vreinterpretq_m128i_u16(
            vshrq_n_u16(vreinterpretq_u16_m128i(mtx[j]), 8));
        uint16x8_t vec_res = vandq_u16(vreinterpretq_u16_m128i(mtx[j]),
                                       vreinterpretq_u16_m128i(tmp));
        int t = _sse2neon_vaddvq_u16(vec_res) ? 1 : 0;
        res |= (t << j);
    }
    return res;
}

#define SSE2NEON_CMP_RANGES_IS_BYTE 1
#define SSE2NEON_CMP_RANGES_IS_WORD 0

/* clang-format off */
#define SSE2NEON_GENERATE_CMP_RANGES(prefix)             \
    prefix##IMPL(byte, uint, u, prefix##IS_BYTE)         \
    prefix##IMPL(byte, int, s, prefix##IS_BYTE)          \
    prefix##IMPL(word, uint, u, prefix##IS_WORD)         \
    prefix##IMPL(word, int, s, prefix##IS_WORD)
/* clang-format on */

SSE2NEON_GENERATE_CMP_RANGES(SSE2NEON_CMP_RANGES_)

#undef SSE2NEON_CMP_RANGES_IS_BYTE
#undef SSE2NEON_CMP_RANGES_IS_WORD

static int _sse2neon_cmp_byte_equal_each(__m128i a, int la, __m128i b, int lb)
{
    uint8x16_t mtx =
        vceqq_u8(vreinterpretq_u8_m128i(a), vreinterpretq_u8_m128i(b));
    int m0 = (la < lb) ? 0 : ((1 << la) - (1 << lb));
    int m1 = 0x10000 - (1 << la);
    int tb = 0x10000 - (1 << lb);
    uint8x8_t vec_mask, vec0_lo, vec0_hi, vec1_lo, vec1_hi;
    uint8x8_t tmp_lo, tmp_hi, res_lo, res_hi;
    vec_mask = vld1_u8(_sse2neon_cmpestr_mask8b);
    vec0_lo = vtst_u8(vdup_n_u8(m0), vec_mask);
    vec0_hi = vtst_u8(vdup_n_u8(m0 >> 8), vec_mask);
    vec1_lo = vtst_u8(vdup_n_u8(m1), vec_mask);
    vec1_hi = vtst_u8(vdup_n_u8(m1 >> 8), vec_mask);
    tmp_lo = vtst_u8(vdup_n_u8(tb), vec_mask);
    tmp_hi = vtst_u8(vdup_n_u8(tb >> 8), vec_mask);

    res_lo = vbsl_u8(vec0_lo, vdup_n_u8(0), vget_low_u8(mtx));
    res_hi = vbsl_u8(vec0_hi, vdup_n_u8(0), vget_high_u8(mtx));
    res_lo = vbsl_u8(vec1_lo, tmp_lo, res_lo);
    res_hi = vbsl_u8(vec1_hi, tmp_hi, res_hi);
    res_lo = vand_u8(res_lo, vec_mask);
    res_hi = vand_u8(res_hi, vec_mask);

    int res = _sse2neon_vaddv_u8(res_lo) + (_sse2neon_vaddv_u8(res_hi) << 8);
    return res;
}

static int _sse2neon_cmp_word_equal_each(__m128i a, int la, __m128i b, int lb)
{
    uint16x8_t mtx =
        vceqq_u16(vreinterpretq_u16_m128i(a), vreinterpretq_u16_m128i(b));
    int m0 = (la < lb) ? 0 : ((1 << la) - (1 << lb));
    int m1 = 0x100 - (1 << la);
    int tb = 0x100 - (1 << lb);
    uint16x8_t vec_mask = vld1q_u16(_sse2neon_cmpestr_mask16b);
    uint16x8_t vec0 = vtstq_u16(vdupq_n_u16(m0), vec_mask);
    uint16x8_t vec1 = vtstq_u16(vdupq_n_u16(m1), vec_mask);
    uint16x8_t tmp = vtstq_u16(vdupq_n_u16(tb), vec_mask);
    mtx = vbslq_u16(vec0, vdupq_n_u16(0), mtx);
    mtx = vbslq_u16(vec1, tmp, mtx);
    mtx = vandq_u16(mtx, vec_mask);
    return _sse2neon_vaddvq_u16(mtx);
}

#define SSE2NEON_AGGREGATE_EQUAL_ORDER_IS_UBYTE 1
#define SSE2NEON_AGGREGATE_EQUAL_ORDER_IS_UWORD 0

#define SSE2NEON_AGGREGATE_EQUAL_ORDER_IMPL(size, number_of_lanes, data_type)  \
    static int _sse2neon_aggregate_equal_ordered_##size##x##number_of_lanes(   \
        int bound, int la, int lb, __m128i mtx[16])                            \
    {                                                                          \
        int res = 0;                                                           \
        int m1 = SSE2NEON_IIF(data_type)(0x10000, 0x100) - (1 << la);          \
        uint##size##x8_t vec_mask = SSE2NEON_IIF(data_type)(                   \
            vld1_u##size(_sse2neon_cmpestr_mask##size##b),                     \
            vld1q_u##size(_sse2neon_cmpestr_mask##size##b));                   \
        uint##size##x##number_of_lanes##_t vec1 = SSE2NEON_IIF(data_type)(     \
            vcombine_u##size(vtst_u##size(vdup_n_u##size(m1), vec_mask),       \
                             vtst_u##size(vdup_n_u##size(m1 >> 8), vec_mask)), \
            vtstq_u##size(vdupq_n_u##size(m1), vec_mask));                     \
        uint##size##x##number_of_lanes##_t vec_minusone = vdupq_n_u##size(-1); \
        uint##size##x##number_of_lanes##_t vec_zero = vdupq_n_u##size(0);      \
        for (int j = 0; j < lb; j++) {                                         \
            mtx[j] = vreinterpretq_m128i_u##size(vbslq_u##size(                \
                vec1, vec_minusone, vreinterpretq_u##size##_m128i(mtx[j])));   \
        }                                                                      \
        for (int j = lb; j < bound; j++) {                                     \
            mtx[j] = vreinterpretq_m128i_u##size(                              \
                vbslq_u##size(vec1, vec_minusone, vec_zero));                  \
        }                                                                      \
        unsigned SSE2NEON_IIF(data_type)(char, short) *ptr =                   \
            (unsigned SSE2NEON_IIF(data_type)(char, short) *) mtx;             \
        for (int i = 0; i < bound; i++) {                                      \
            int val = 1;                                                       \
            for (int j = 0, k = i; j < bound - i && k < bound; j++, k++)       \
                val &= ptr[k * bound + j];                                     \
            res += val << i;                                                   \
        }                                                                      \
        return res;                                                            \
    }

/* clang-format off */
#define SSE2NEON_GENERATE_AGGREGATE_EQUAL_ORDER(prefix) \
    prefix##IMPL(8, 16, prefix##IS_UBYTE)               \
    prefix##IMPL(16, 8, prefix##IS_UWORD)
/* clang-format on */

SSE2NEON_GENERATE_AGGREGATE_EQUAL_ORDER(SSE2NEON_AGGREGATE_EQUAL_ORDER_)

#undef SSE2NEON_AGGREGATE_EQUAL_ORDER_IS_UBYTE
#undef SSE2NEON_AGGREGATE_EQUAL_ORDER_IS_UWORD

/* clang-format off */
#define SSE2NEON_GENERATE_CMP_EQUAL_ORDERED(prefix) \
    prefix##IMPL(byte)                              \
    prefix##IMPL(word)
/* clang-format on */

SSE2NEON_GENERATE_CMP_EQUAL_ORDERED(SSE2NEON_CMP_EQUAL_ORDERED_)

#define SSE2NEON_CMPESTR_LIST                          \
    _(CMP_UBYTE_EQUAL_ANY, cmp_byte_equal_any)         \
    _(CMP_UWORD_EQUAL_ANY, cmp_word_equal_any)         \
    _(CMP_SBYTE_EQUAL_ANY, cmp_byte_equal_any)         \
    _(CMP_SWORD_EQUAL_ANY, cmp_word_equal_any)         \
    _(CMP_UBYTE_RANGES, cmp_ubyte_ranges)              \
    _(CMP_UWORD_RANGES, cmp_uword_ranges)              \
    _(CMP_SBYTE_RANGES, cmp_sbyte_ranges)              \
    _(CMP_SWORD_RANGES, cmp_sword_ranges)              \
    _(CMP_UBYTE_EQUAL_EACH, cmp_byte_equal_each)       \
    _(CMP_UWORD_EQUAL_EACH, cmp_word_equal_each)       \
    _(CMP_SBYTE_EQUAL_EACH, cmp_byte_equal_each)       \
    _(CMP_SWORD_EQUAL_EACH, cmp_word_equal_each)       \
    _(CMP_UBYTE_EQUAL_ORDERED, cmp_byte_equal_ordered) \
    _(CMP_UWORD_EQUAL_ORDERED, cmp_word_equal_ordered) \
    _(CMP_SBYTE_EQUAL_ORDERED, cmp_byte_equal_ordered) \
    _(CMP_SWORD_EQUAL_ORDERED, cmp_word_equal_ordered)

enum {
#define _(name, func_suffix) name,
    SSE2NEON_CMPESTR_LIST
#undef _
};
typedef int (*cmpestr_func_t)(__m128i a, int la, __m128i b, int lb);
static cmpestr_func_t _sse2neon_cmpfunc_table[] = {
#define _(name, func_suffix) _sse2neon_##func_suffix,
    SSE2NEON_CMPESTR_LIST
#undef _
};

FORCE_INLINE int _sse2neon_sido_negative(int res, int lb, int imm8, int bound)
{
    switch (imm8 & 0x30) {
    case _SIDD_NEGATIVE_POLARITY:
        res ^= 0xffffffff;
        break;
    case _SIDD_MASKED_NEGATIVE_POLARITY:
        res ^= (1 << lb) - 1;
        break;
    default:
        break;
    }

    return res & ((bound == 8) ? 0xFF : 0xFFFF);
}

FORCE_INLINE int _sse2neon_clz(unsigned int x)
{
#if _MSC_VER
    DWORD cnt = 0;
    if (_BitScanForward(&cnt, x))
        return cnt;
    return 32;
#else
    return x != 0 ? __builtin_clz(x) : 32;
#endif
}

FORCE_INLINE int _sse2neon_ctz(unsigned int x)
{
#if _MSC_VER
    DWORD cnt = 0;
    if (_BitScanReverse(&cnt, x))
        return 31 - cnt;
    return 32;
#else
    return x != 0 ? __builtin_ctz(x) : 32;
#endif
}

FORCE_INLINE int _sse2neon_ctzll(unsigned long long x)
{
#if _MSC_VER
    unsigned long cnt;
#ifdef defined(SSE2NEON_HAS_BITSCAN64)
    (defined(_M_AMD64) || defined(__x86_64__))
        if((_BitScanForward64(&cnt, x))
            return (int)(cnt);
#else
    if (_BitScanForward(&cnt, (unsigned long) (x)))
        return (int) cnt;
    if (_BitScanForward(&cnt, (unsigned long) (x >> 32)))
        return (int) (cnt + 32);
#endif
    return 64;
#else
    return x != 0 ? __builtin_ctzll(x) : 64;
#endif
}

#define SSE2NEON_MIN(x, y) (x) < (y) ? (x) : (y)

#define SSE2NEON_CMPSTR_SET_UPPER(var, imm) \
    const int var = (imm & 0x01) ? 8 : 16

#define SSE2NEON_CMPESTRX_LEN_PAIR(a, b, la, lb) \
    int tmp1 = la ^ (la >> 31);                  \
    la = tmp1 - (la >> 31);                      \
    int tmp2 = lb ^ (lb >> 31);                  \
    lb = tmp2 - (lb >> 31);                      \
    la = SSE2NEON_MIN(la, bound);                \
    lb = SSE2NEON_MIN(lb, bound)

// Compare all pairs of character in string a and b,
// then aggregate the result.
// As the only difference of PCMPESTR* and PCMPISTR* is the way to calculate the
// length of string, we use SSE2NEON_CMP{I,E}STRX_GET_LEN to get the length of
// string a and b.
#define SSE2NEON_COMP_AGG(a, b, la, lb, imm8, IE)                  \
    SSE2NEON_CMPSTR_SET_UPPER(bound, imm8);                        \
    SSE2NEON_##IE##_LEN_PAIR(a, b, la, lb);                        \
    int r2 = (_sse2neon_cmpfunc_table[imm8 & 0x0f])(a, la, b, lb); \
    r2 = _sse2neon_sido_negative(r2, lb, imm8, bound)

#define SSE2NEON_CMPSTR_GENERATE_INDEX(r2, bound, imm8)          \
    return (r2 == 0) ? bound                                     \
                     : ((imm8 & 0x40) ? (31 - _sse2neon_clz(r2)) \
                                      : _sse2neon_ctz(r2))

#define SSE2NEON_CMPSTR_GENERATE_MASK(dst)                                     \
    __m128i dst = vreinterpretq_m128i_u8(vdupq_n_u8(0));                       \
    if (imm8 & 0x40) {                                                         \
        if (bound == 8) {                                                      \
            uint16x8_t tmp = vtstq_u16(vdupq_n_u16(r2),                        \
                                       vld1q_u16(_sse2neon_cmpestr_mask16b));  \
            dst = vreinterpretq_m128i_u16(vbslq_u16(                           \
                tmp, vdupq_n_u16(-1), vreinterpretq_u16_m128i(dst)));          \
        } else {                                                               \
            uint8x16_t vec_r2 =                                                \
                vcombine_u8(vdup_n_u8(r2), vdup_n_u8(r2 >> 8));                \
            uint8x16_t tmp =                                                   \
                vtstq_u8(vec_r2, vld1q_u8(_sse2neon_cmpestr_mask8b));          \
            dst = vreinterpretq_m128i_u8(                                      \
                vbslq_u8(tmp, vdupq_n_u8(-1), vreinterpretq_u8_m128i(dst)));   \
        }                                                                      \
    } else {                                                                   \
        if (bound == 16) {                                                     \
            dst = vreinterpretq_m128i_u16(                                     \
                vsetq_lane_u16(r2 & 0xffff, vreinterpretq_u16_m128i(dst), 0)); \
        } else {                                                               \
            dst = vreinterpretq_m128i_u8(                                      \
                vsetq_lane_u8(r2 & 0xff, vreinterpretq_u8_m128i(dst), 0));     \
        }                                                                      \
    }                                                                          \
    return dst

// Compare packed strings in a and b with lengths la and lb using the control
// in imm8, and returns 1 if b did not contain a null character and the
// resulting mask was zero, and 0 otherwise.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_cmpestra
FORCE_INLINE int _mm_cmpestra(__m128i a,
                              int la,
                              __m128i b,
                              int lb,
                              const int imm8)
{
    int lb_cpy = lb;
    SSE2NEON_COMP_AGG(a, b, la, lb, imm8, CMPESTRX);
    return !r2 & (lb_cpy > bound);
}

// Compare packed strings in a and b with lengths la and lb using the control in
// imm8, and returns 1 if the resulting mask was non-zero, and 0 otherwise.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_cmpestrc
FORCE_INLINE int _mm_cmpestrc(__m128i a,
                              int la,
                              __m128i b,
                              int lb,
                              const int imm8)
{
    SSE2NEON_COMP_AGG(a, b, la, lb, imm8, CMPESTRX);
    return r2 != 0;
}

// Compare packed strings in a and b with lengths la and lb using the control
// in imm8, and store the generated index in dst.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_cmpestri
FORCE_INLINE int _mm_cmpestri(__m128i a,
                              int la,
                              __m128i b,
                              int lb,
                              const int imm8)
{
    SSE2NEON_COMP_AGG(a, b, la, lb, imm8, CMPESTRX);
    SSE2NEON_CMPSTR_GENERATE_INDEX(r2, bound, imm8);
}

// Compare packed strings in a and b with lengths la and lb using the control
// in imm8, and store the generated mask in dst.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_cmpestrm
FORCE_INLINE __m128i
_mm_cmpestrm(__m128i a, int la, __m128i b, int lb, const int imm8)
{
    SSE2NEON_COMP_AGG(a, b, la, lb, imm8, CMPESTRX);
    SSE2NEON_CMPSTR_GENERATE_MASK(dst);
}

// Compare packed strings in a and b with lengths la and lb using the control in
// imm8, and returns bit 0 of the resulting bit mask.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_cmpestro
FORCE_INLINE int _mm_cmpestro(__m128i a,
                              int la,
                              __m128i b,
                              int lb,
                              const int imm8)
{
    SSE2NEON_COMP_AGG(a, b, la, lb, imm8, CMPESTRX);
    return r2 & 1;
}

// Compare packed strings in a and b with lengths la and lb using the control in
// imm8, and returns 1 if any character in a was null, and 0 otherwise.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_cmpestrs
FORCE_INLINE int _mm_cmpestrs(__m128i a,
                              int la,
                              __m128i b,
                              int lb,
                              const int imm8)
{
    SSE2NEON_CMPSTR_SET_UPPER(bound, imm8);
    return la <= (bound - 1);
}

// Compare packed strings in a and b with lengths la and lb using the control in
// imm8, and returns 1 if any character in b was null, and 0 otherwise.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_cmpestrz
FORCE_INLINE int _mm_cmpestrz(__m128i a,
                              int la,
                              __m128i b,
                              int lb,
                              const int imm8)
{
    SSE2NEON_CMPSTR_SET_UPPER(bound, imm8);
    return lb <= (bound - 1);
}

#define SSE2NEON_CMPISTRX_LENGTH(str, len, imm8)                         \
    do {                                                                 \
        if (imm8 & 0x01) {                                               \
            uint16x8_t equal_mask_##str =                                \
                vceqq_u16(vreinterpretq_u16_m128i(str), vdupq_n_u16(0)); \
            uint8x8_t res_##str = vshrn_n_u16(equal_mask_##str, 4);      \
            uint64_t matches_##str =                                     \
                vget_lane_u64(vreinterpret_u64_u8(res_##str), 0);        \
            len = _sse2neon_ctzll(matches_##str) >> 3;                   \
        } else {                                                         \
            uint16x8_t equal_mask_##str = vreinterpretq_u16_u8(          \
                vceqq_u8(vreinterpretq_u8_m128i(str), vdupq_n_u8(0)));   \
            uint8x8_t res_##str = vshrn_n_u16(equal_mask_##str, 4);      \
            uint64_t matches_##str =                                     \
                vget_lane_u64(vreinterpret_u64_u8(res_##str), 0);        \
            len = _sse2neon_ctzll(matches_##str) >> 2;                   \
        }                                                                \
    } while (0)

#define SSE2NEON_CMPISTRX_LEN_PAIR(a, b, la, lb) \
    int la, lb;                                  \
    do {                                         \
        SSE2NEON_CMPISTRX_LENGTH(a, la, imm8);   \
        SSE2NEON_CMPISTRX_LENGTH(b, lb, imm8);   \
    } while (0)

// Compare packed strings with implicit lengths in a and b using the control in
// imm8, and returns 1 if b did not contain a null character and the resulting
// mask was zero, and 0 otherwise.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_cmpistra
FORCE_INLINE int _mm_cmpistra(__m128i a, __m128i b, const int imm8)
{
    SSE2NEON_COMP_AGG(a, b, la, lb, imm8, CMPISTRX);
    return !r2 & (lb >= bound);
}

// Compare packed strings with implicit lengths in a and b using the control in
// imm8, and returns 1 if the resulting mask was non-zero, and 0 otherwise.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_cmpistrc
FORCE_INLINE int _mm_cmpistrc(__m128i a, __m128i b, const int imm8)
{
    SSE2NEON_COMP_AGG(a, b, la, lb, imm8, CMPISTRX);
    return r2 != 0;
}

// Compare packed strings with implicit lengths in a and b using the control in
// imm8, and store the generated index in dst.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_cmpistri
FORCE_INLINE int _mm_cmpistri(__m128i a, __m128i b, const int imm8)
{
    SSE2NEON_COMP_AGG(a, b, la, lb, imm8, CMPISTRX);
    SSE2NEON_CMPSTR_GENERATE_INDEX(r2, bound, imm8);
}

// Compare packed strings with implicit lengths in a and b using the control in
// imm8, and store the generated mask in dst.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_cmpistrm
FORCE_INLINE __m128i _mm_cmpistrm(__m128i a, __m128i b, const int imm8)
{
    SSE2NEON_COMP_AGG(a, b, la, lb, imm8, CMPISTRX);
    SSE2NEON_CMPSTR_GENERATE_MASK(dst);
}

// Compare packed strings with implicit lengths in a and b using the control in
// imm8, and returns bit 0 of the resulting bit mask.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_cmpistro
FORCE_INLINE int _mm_cmpistro(__m128i a, __m128i b, const int imm8)
{
    SSE2NEON_COMP_AGG(a, b, la, lb, imm8, CMPISTRX);
    return r2 & 1;
}

// Compare packed strings with implicit lengths in a and b using the control in
// imm8, and returns 1 if any character in a was null, and 0 otherwise.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_cmpistrs
FORCE_INLINE int _mm_cmpistrs(__m128i a, __m128i b, const int imm8)
{
    SSE2NEON_CMPSTR_SET_UPPER(bound, imm8);
    int la;
    SSE2NEON_CMPISTRX_LENGTH(a, la, imm8);
    return la <= (bound - 1);
}

// Compare packed strings with implicit lengths in a and b using the control in
// imm8, and returns 1 if any character in b was null, and 0 otherwise.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_cmpistrz
FORCE_INLINE int _mm_cmpistrz(__m128i a, __m128i b, const int imm8)
{
    SSE2NEON_CMPSTR_SET_UPPER(bound, imm8);
    int lb;
    SSE2NEON_CMPISTRX_LENGTH(b, lb, imm8);
    return lb <= (bound - 1);
}

// Compares the 2 signed 64-bit integers in a and the 2 signed 64-bit integers
// in b for greater than.
FORCE_INLINE __m128i _mm_cmpgt_epi64(__m128i a, __m128i b)
{
#if defined(__aarch64__)
    return vreinterpretq_m128i_u64(
        vcgtq_s64(vreinterpretq_s64_m128i(a), vreinterpretq_s64_m128i(b)));
#else
    return vreinterpretq_m128i_s64(vshrq_n_s64(
        vqsubq_s64(vreinterpretq_s64_m128i(b), vreinterpretq_s64_m128i(a)),
        63));
#endif
}

// Starting with the initial value in crc, accumulates a CRC32 value for
// unsigned 16-bit integer v.
// https://msdn.microsoft.com/en-us/library/bb531411(v=vs.100)
FORCE_INLINE uint32_t _mm_crc32_u16(uint32_t crc, uint16_t v)
{
#if defined(__aarch64__) && defined(__ARM_FEATURE_CRC32)
    __asm__ __volatile__("crc32ch %w[c], %w[c], %w[v]\n\t"
                         : [c] "+r"(crc)
                         : [v] "r"(v));
#elif (__ARM_ARCH == 8) && defined(__ARM_FEATURE_CRC32)
    crc = __crc32ch(crc, v);
#else
    crc = _mm_crc32_u8(crc, v & 0xff);
    crc = _mm_crc32_u8(crc, (v >> 8) & 0xff);
#endif
    return crc;
}

// Starting with the initial value in crc, accumulates a CRC32 value for
// unsigned 32-bit integer v.
// https://msdn.microsoft.com/en-us/library/bb531394(v=vs.100)
FORCE_INLINE uint32_t _mm_crc32_u32(uint32_t crc, uint32_t v)
{
#if defined(__aarch64__) && defined(__ARM_FEATURE_CRC32)
    __asm__ __volatile__("crc32cw %w[c], %w[c], %w[v]\n\t"
                         : [c] "+r"(crc)
                         : [v] "r"(v));
#elif (__ARM_ARCH == 8) && defined(__ARM_FEATURE_CRC32)
    crc = __crc32cw(crc, v);
#else
    crc = _mm_crc32_u16(crc, v & 0xffff);
    crc = _mm_crc32_u16(crc, (v >> 16) & 0xffff);
#endif
    return crc;
}

// Starting with the initial value in crc, accumulates a CRC32 value for
// unsigned 64-bit integer v.
// https://msdn.microsoft.com/en-us/library/bb514033(v=vs.100)
FORCE_INLINE uint64_t _mm_crc32_u64(uint64_t crc, uint64_t v)
{
#if defined(__aarch64__) && defined(__ARM_FEATURE_CRC32)
    __asm__ __volatile__("crc32cx %w[c], %w[c], %x[v]\n\t"
                         : [c] "+r"(crc)
                         : [v] "r"(v));
#else
    crc = _mm_crc32_u32((uint32_t) (crc), v & 0xffffffff);
    crc = _mm_crc32_u32((uint32_t) (crc), (v >> 32) & 0xffffffff);
#endif
    return crc;
}

// Starting with the initial value in crc, accumulates a CRC32 value for
// unsigned 8-bit integer v.
// https://msdn.microsoft.com/en-us/library/bb514036(v=vs.100)
FORCE_INLINE uint32_t _mm_crc32_u8(uint32_t crc, uint8_t v)
{
#if defined(__aarch64__) && defined(__ARM_FEATURE_CRC32)
    __asm__ __volatile__("crc32cb %w[c], %w[c], %w[v]\n\t"
                         : [c] "+r"(crc)
                         : [v] "r"(v));
#elif (__ARM_ARCH == 8) && defined(__ARM_FEATURE_CRC32)
    crc = __crc32cb(crc, v);
#else
    crc ^= v;
    for (int bit = 0; bit < 8; bit++) {
        if (crc & 1)
            crc = (crc >> 1) ^ UINT32_C(0x82f63b78);
        else
            crc = (crc >> 1);
    }
#endif
    return crc;
}

/* AES */

#if !defined(__ARM_FEATURE_CRYPTO)
/* clang-format off */
#define SSE2NEON_AES_SBOX(w)                                           \
    {                                                                  \
        w(0x63), w(0x7c), w(0x77), w(0x7b), w(0xf2), w(0x6b), w(0x6f), \
        w(0xc5), w(0x30), w(0x01), w(0x67), w(0x2b), w(0xfe), w(0xd7), \
        w(0xab), w(0x76), w(0xca), w(0x82), w(0xc9), w(0x7d), w(0xfa), \
        w(0x59), w(0x47), w(0xf0), w(0xad), w(0xd4), w(0xa2), w(0xaf), \
        w(0x9c), w(0xa4), w(0x72), w(0xc0), w(0xb7), w(0xfd), w(0x93), \
        w(0x26), w(0x36), w(0x3f), w(0xf7), w(0xcc), w(0x34), w(0xa5), \
        w(0xe5), w(0xf1), w(0x71), w(0xd8), w(0x31), w(0x15), w(0x04), \
        w(0xc7), w(0x23), w(0xc3), w(0x18), w(0x96), w(0x05), w(0x9a), \
        w(0x07), w(0x12), w(0x80), w(0xe2), w(0xeb), w(0x27), w(0xb2), \
        w(0x75), w(0x09), w(0x83), w(0x2c), w(0x1a), w(0x1b), w(0x6e), \
        w(0x5a), w(0xa0), w(0x52), w(0x3b), w(0xd6), w(0xb3), w(0x29), \
        w(0xe3), w(0x2f), w(0x84), w(0x53), w(0xd1), w(0x00), w(0xed), \
        w(0x20), w(0xfc), w(0xb1), w(0x5b), w(0x6a), w(0xcb), w(0xbe), \
        w(0x39), w(0x4a), w(0x4c), w(0x58), w(0xcf), w(0xd0), w(0xef), \
        w(0xaa), w(0xfb), w(0x43), w(0x4d), w(0x33), w(0x85), w(0x45), \
        w(0xf9), w(0x02), w(0x7f), w(0x50), w(0x3c), w(0x9f), w(0xa8), \
        w(0x51), w(0xa3), w(0x40), w(0x8f), w(0x92), w(0x9d), w(0x38), \
        w(0xf5), w(0xbc), w(0xb6), w(0xda), w(0x21), w(0x10), w(0xff), \
        w(0xf3), w(0xd2), w(0xcd), w(0x0c), w(0x13), w(0xec), w(0x5f), \
        w(0x97), w(0x44), w(0x17), w(0xc4), w(0xa7), w(0x7e), w(0x3d), \
        w(0x64), w(0x5d), w(0x19), w(0x73), w(0x60), w(0x81), w(0x4f), \
        w(0xdc), w(0x22), w(0x2a), w(0x90), w(0x88), w(0x46), w(0xee), \
        w(0xb8), w(0x14), w(0xde), w(0x5e), w(0x0b), w(0xdb), w(0xe0), \
        w(0x32), w(0x3a), w(0x0a), w(0x49), w(0x06), w(0x24), w(0x5c), \
        w(0xc2), w(0xd3), w(0xac), w(0x62), w(0x91), w(0x95), w(0xe4), \
        w(0x79), w(0xe7), w(0xc8), w(0x37), w(0x6d), w(0x8d), w(0xd5), \
        w(0x4e), w(0xa9), w(0x6c), w(0x56), w(0xf4), w(0xea), w(0x65), \
        w(0x7a), w(0xae), w(0x08), w(0xba), w(0x78), w(0x25), w(0x2e), \
        w(0x1c), w(0xa6), w(0xb4), w(0xc6), w(0xe8), w(0xdd), w(0x74), \
        w(0x1f), w(0x4b), w(0xbd), w(0x8b), w(0x8a), w(0x70), w(0x3e), \
        w(0xb5), w(0x66), w(0x48), w(0x03), w(0xf6), w(0x0e), w(0x61), \
        w(0x35), w(0x57), w(0xb9), w(0x86), w(0xc1), w(0x1d), w(0x9e), \
        w(0xe1), w(0xf8), w(0x98), w(0x11), w(0x69), w(0xd9), w(0x8e), \
        w(0x94), w(0x9b), w(0x1e), w(0x87), w(0xe9), w(0xce), w(0x55), \
        w(0x28), w(0xdf), w(0x8c), w(0xa1), w(0x89), w(0x0d), w(0xbf), \
        w(0xe6), w(0x42), w(0x68), w(0x41), w(0x99), w(0x2d), w(0x0f), \
        w(0xb0), w(0x54), w(0xbb), w(0x16)                             \
    }
#define SSE2NEON_AES_RSBOX(w)                                          \
    {                                                                  \
        w(0x52), w(0x09), w(0x6a), w(0xd5), w(0x30), w(0x36), w(0xa5), \
        w(0x38), w(0xbf), w(0x40), w(0xa3), w(0x9e), w(0x81), w(0xf3), \
        w(0xd7), w(0xfb), w(0x7c), w(0xe3), w(0x39), w(0x82), w(0x9b), \
        w(0x2f), w(0xff), w(0x87), w(0x34), w(0x8e), w(0x43), w(0x44), \
        w(0xc4), w(0xde), w(0xe9), w(0xcb), w(0x54), w(0x7b), w(0x94), \
        w(0x32), w(0xa6), w(0xc2), w(0x23), w(0x3d), w(0xee), w(0x4c), \
        w(0x95), w(0x0b), w(0x42), w(0xfa), w(0xc3), w(0x4e), w(0x08), \
        w(0x2e), w(0xa1), w(0x66), w(0x28), w(0xd9), w(0x24), w(0xb2), \
        w(0x76), w(0x5b), w(0xa2), w(0x49), w(0x6d), w(0x8b), w(0xd1), \
        w(0x25), w(0x72), w(0xf8), w(0xf6), w(0x64), w(0x86), w(0x68), \
        w(0x98), w(0x16), w(0xd4), w(0xa4), w(0x5c), w(0xcc), w(0x5d), \
        w(0x65), w(0xb6), w(0x92), w(0x6c), w(0x70), w(0x48), w(0x50), \
        w(0xfd), w(0xed), w(0xb9), w(0xda), w(0x5e), w(0x15), w(0x46), \
        w(0x57), w(0xa7), w(0x8d), w(0x9d), w(0x84), w(0x90), w(0xd8), \
        w(0xab), w(0x00), w(0x8c), w(0xbc), w(0xd3), w(0x0a), w(0xf7), \
        w(0xe4), w(0x58), w(0x05), w(0xb8), w(0xb3), w(0x45), w(0x06), \
        w(0xd0), w(0x2c), w(0x1e), w(0x8f), w(0xca), w(0x3f), w(0x0f), \
        w(0x02), w(0xc1), w(0xaf), w(0xbd), w(0x03), w(0x01), w(0x13), \
        w(0x8a), w(0x6b), w(0x3a), w(0x91), w(0x11), w(0x41), w(0x4f), \
        w(0x67), w(0xdc), w(0xea), w(0x97), w(0xf2), w(0xcf), w(0xce), \
        w(0xf0), w(0xb4), w(0xe6), w(0x73), w(0x96), w(0xac), w(0x74), \
        w(0x22), w(0xe7), w(0xad), w(0x35), w(0x85), w(0xe2), w(0xf9), \
        w(0x37), w(0xe8), w(0x1c), w(0x75), w(0xdf), w(0x6e), w(0x47), \
        w(0xf1), w(0x1a), w(0x71), w(0x1d), w(0x29), w(0xc5), w(0x89), \
        w(0x6f), w(0xb7), w(0x62), w(0x0e), w(0xaa), w(0x18), w(0xbe), \
        w(0x1b), w(0xfc), w(0x56), w(0x3e), w(0x4b), w(0xc6), w(0xd2), \
        w(0x79), w(0x20), w(0x9a), w(0xdb), w(0xc0), w(0xfe), w(0x78), \
        w(0xcd), w(0x5a), w(0xf4), w(0x1f), w(0xdd), w(0xa8), w(0x33), \
        w(0x88), w(0x07), w(0xc7), w(0x31), w(0xb1), w(0x12), w(0x10), \
        w(0x59), w(0x27), w(0x80), w(0xec), w(0x5f), w(0x60), w(0x51), \
        w(0x7f), w(0xa9), w(0x19), w(0xb5), w(0x4a), w(0x0d), w(0x2d), \
        w(0xe5), w(0x7a), w(0x9f), w(0x93), w(0xc9), w(0x9c), w(0xef), \
        w(0xa0), w(0xe0), w(0x3b), w(0x4d), w(0xae), w(0x2a), w(0xf5), \
        w(0xb0), w(0xc8), w(0xeb), w(0xbb), w(0x3c), w(0x83), w(0x53), \
        w(0x99), w(0x61), w(0x17), w(0x2b), w(0x04), w(0x7e), w(0xba), \
        w(0x77), w(0xd6), w(0x26), w(0xe1), w(0x69), w(0x14), w(0x63), \
        w(0x55), w(0x21), w(0x0c), w(0x7d)                             \
    }
/* clang-format on */

/* X Macro trick. See https://en.wikipedia.org/wiki/X_Macro */
#define SSE2NEON_AES_H0(x) (x)
static const uint8_t _sse2neon_sbox[256] = SSE2NEON_AES_SBOX(SSE2NEON_AES_H0);
static const uint8_t _sse2neon_rsbox[256] = SSE2NEON_AES_RSBOX(SSE2NEON_AES_H0);
#undef SSE2NEON_AES_H0

/* x_time function and matrix multiply function */
#if !defined(__aarch64__)
#define SSE2NEON_XT(x) (((x) << 1) ^ ((((x) >> 7) & 1) * 0x1b))
#define SSE2NEON_MULTIPLY(x, y)                                  \
    (((y & 1) * x) ^ ((y >> 1 & 1) * SSE2NEON_XT(x)) ^           \
     ((y >> 2 & 1) * SSE2NEON_XT(SSE2NEON_XT(x))) ^              \
     ((y >> 3 & 1) * SSE2NEON_XT(SSE2NEON_XT(SSE2NEON_XT(x)))) ^ \
     ((y >> 4 & 1) * SSE2NEON_XT(SSE2NEON_XT(SSE2NEON_XT(SSE2NEON_XT(x))))))
#endif

// In the absence of crypto extensions, implement aesenc using regular neon
// intrinsics instead. See:
// https://www.workofard.com/2017/01/accelerated-aes-for-the-arm64-linux-kernel/
// https://www.workofard.com/2017/07/ghash-for-low-end-cores/ and
// https://github.com/ColinIanKing/linux-next-mirror/blob/b5f466091e130caaf0735976648f72bd5e09aa84/crypto/aegis128-neon-inner.c#L52
// for more information Reproduced with permission of the author.
FORCE_INLINE __m128i _mm_aesenc_si128(__m128i a, __m128i RoundKey)
{
#if defined(__aarch64__)
    static const uint8_t shift_rows[] = {
        0x0, 0x5, 0xa, 0xf, 0x4, 0x9, 0xe, 0x3,
        0x8, 0xd, 0x2, 0x7, 0xc, 0x1, 0x6, 0xb,
    };
    static const uint8_t ror32by8[] = {
        0x1, 0x2, 0x3, 0x0, 0x5, 0x6, 0x7, 0x4,
        0x9, 0xa, 0xb, 0x8, 0xd, 0xe, 0xf, 0xc,
    };

    uint8x16_t v;
    uint8x16_t w = vreinterpretq_u8_m128i(a);

    /* shift rows */
    w = vqtbl1q_u8(w, vld1q_u8(shift_rows));

    /* sub bytes */
    // Here, we separate the whole 256-bytes table into 4 64-bytes tables, and
    // look up each of the table. After each lookup, we load the next table
    // which locates at the next 64-bytes. In the meantime, the index in the
    // table would be smaller than it was, so the index parameters of
    // `vqtbx4q_u8()` need to be added the same constant as the loaded tables.
    v = vqtbl4q_u8(_sse2neon_vld1q_u8_x4(_sse2neon_sbox), w);
    // 'w-0x40' equals to 'vsubq_u8(w, vdupq_n_u8(0x40))'
    v = vqtbx4q_u8(v, _sse2neon_vld1q_u8_x4(_sse2neon_sbox + 0x40), w - 0x40);
    v = vqtbx4q_u8(v, _sse2neon_vld1q_u8_x4(_sse2neon_sbox + 0x80), w - 0x80);
    v = vqtbx4q_u8(v, _sse2neon_vld1q_u8_x4(_sse2neon_sbox + 0xc0), w - 0xc0);

    /* mix columns */
    w = (v << 1) ^ (uint8x16_t) (((int8x16_t) v >> 7) & 0x1b);
    w ^= (uint8x16_t) vrev32q_u16((uint16x8_t) v);
    w ^= vqtbl1q_u8(v ^ w, vld1q_u8(ror32by8));

    /* add round key */
    return vreinterpretq_m128i_u8(w) ^ RoundKey;

#else /* ARMv7-A implementation for a table-based AES */
#define SSE2NEON_AES_B2W(b0, b1, b2, b3)                 \
    (((uint32_t) (b3) << 24) | ((uint32_t) (b2) << 16) | \
     ((uint32_t) (b1) << 8) | (uint32_t) (b0))
// muliplying 'x' by 2 in GF(2^8)
#define SSE2NEON_AES_F2(x) ((x << 1) ^ (((x >> 7) & 1) * 0x011b /* WPOLY */))
// muliplying 'x' by 3 in GF(2^8)
#define SSE2NEON_AES_F3(x) (SSE2NEON_AES_F2(x) ^ x)
#define SSE2NEON_AES_U0(p) \
    SSE2NEON_AES_B2W(SSE2NEON_AES_F2(p), p, p, SSE2NEON_AES_F3(p))
#define SSE2NEON_AES_U1(p) \
    SSE2NEON_AES_B2W(SSE2NEON_AES_F3(p), SSE2NEON_AES_F2(p), p, p)
#define SSE2NEON_AES_U2(p) \
    SSE2NEON_AES_B2W(p, SSE2NEON_AES_F3(p), SSE2NEON_AES_F2(p), p)
#define SSE2NEON_AES_U3(p) \
    SSE2NEON_AES_B2W(p, p, SSE2NEON_AES_F3(p), SSE2NEON_AES_F2(p))

    // this generates a table containing every possible permutation of
    // shift_rows() and sub_bytes() with mix_columns().
    static const uint32_t ALIGN_STRUCT(16) aes_table[4][256] = {
        SSE2NEON_AES_SBOX(SSE2NEON_AES_U0),
        SSE2NEON_AES_SBOX(SSE2NEON_AES_U1),
        SSE2NEON_AES_SBOX(SSE2NEON_AES_U2),
        SSE2NEON_AES_SBOX(SSE2NEON_AES_U3),
    };
#undef SSE2NEON_AES_B2W
#undef SSE2NEON_AES_F2
#undef SSE2NEON_AES_F3
#undef SSE2NEON_AES_U0
#undef SSE2NEON_AES_U1
#undef SSE2NEON_AES_U2
#undef SSE2NEON_AES_U3

    uint32_t x0 = _mm_cvtsi128_si32(a);  // get a[31:0]
    uint32_t x1 =
        _mm_cvtsi128_si32(_mm_shuffle_epi32(a, 0x55));  // get a[63:32]
    uint32_t x2 =
        _mm_cvtsi128_si32(_mm_shuffle_epi32(a, 0xAA));  // get a[95:64]
    uint32_t x3 =
        _mm_cvtsi128_si32(_mm_shuffle_epi32(a, 0xFF));  // get a[127:96]

    // finish the modulo addition step in mix_columns()
    __m128i out = _mm_set_epi32(
        (aes_table[0][x3 & 0xff] ^ aes_table[1][(x0 >> 8) & 0xff] ^
         aes_table[2][(x1 >> 16) & 0xff] ^ aes_table[3][x2 >> 24]),
        (aes_table[0][x2 & 0xff] ^ aes_table[1][(x3 >> 8) & 0xff] ^
         aes_table[2][(x0 >> 16) & 0xff] ^ aes_table[3][x1 >> 24]),
        (aes_table[0][x1 & 0xff] ^ aes_table[1][(x2 >> 8) & 0xff] ^
         aes_table[2][(x3 >> 16) & 0xff] ^ aes_table[3][x0 >> 24]),
        (aes_table[0][x0 & 0xff] ^ aes_table[1][(x1 >> 8) & 0xff] ^
         aes_table[2][(x2 >> 16) & 0xff] ^ aes_table[3][x3 >> 24]));

    return _mm_xor_si128(out, RoundKey);
#endif
}

// Perform one round of an AES decryption flow on data (state) in a using the
// round key in RoundKey, and store the result in dst.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_aesdec_si128
FORCE_INLINE __m128i _mm_aesdec_si128(__m128i a, __m128i RoundKey)
{
#if defined(__aarch64__)
    static const uint8_t inv_shift_rows[] = {
        0x0, 0xd, 0xa, 0x7, 0x4, 0x1, 0xe, 0xb,
        0x8, 0x5, 0x2, 0xf, 0xc, 0x9, 0x6, 0x3,
    };
    static const uint8_t ror32by8[] = {
        0x1, 0x2, 0x3, 0x0, 0x5, 0x6, 0x7, 0x4,
        0x9, 0xa, 0xb, 0x8, 0xd, 0xe, 0xf, 0xc,
    };

    uint8x16_t v;
    uint8x16_t w = vreinterpretq_u8_m128i(a);

    // inverse shift rows
    w = vqtbl1q_u8(w, vld1q_u8(inv_shift_rows));

    // inverse sub bytes
    v = vqtbl4q_u8(_sse2neon_vld1q_u8_x4(_sse2neon_rsbox), w);
    v = vqtbx4q_u8(v, _sse2neon_vld1q_u8_x4(_sse2neon_rsbox + 0x40), w - 0x40);
    v = vqtbx4q_u8(v, _sse2neon_vld1q_u8_x4(_sse2neon_rsbox + 0x80), w - 0x80);
    v = vqtbx4q_u8(v, _sse2neon_vld1q_u8_x4(_sse2neon_rsbox + 0xc0), w - 0xc0);

    // inverse mix columns
    // muliplying 'v' by 4 in GF(2^8)
    w = (v << 1) ^ (uint8x16_t) (((int8x16_t) v >> 7) & 0x1b);
    w = (w << 1) ^ (uint8x16_t) (((int8x16_t) w >> 7) & 0x1b);
    v ^= w;
    v ^= (uint8x16_t) vrev32q_u16((uint16x8_t) w);

    w = (v << 1) ^ (uint8x16_t) (((int8x16_t) v >> 7) &
                                 0x1b);  // muliplying 'v' by 2 in GF(2^8)
    w ^= (uint8x16_t) vrev32q_u16((uint16x8_t) v);
    w ^= vqtbl1q_u8(v ^ w, vld1q_u8(ror32by8));

    // add round key
    return vreinterpretq_m128i_u8(w) ^ RoundKey;

#else /* ARMv7-A NEON implementation */
    /* FIXME: optimized for NEON */
    uint8_t i, e, f, g, h, v[4][4];
    uint8_t *_a = (uint8_t *) &a;
    for (i = 0; i < 16; ++i) {
        v[((i / 4) + (i % 4)) % 4][i % 4] = _sse2neon_rsbox[_a[i]];
    }

    // inverse mix columns
    for (i = 0; i < 4; ++i) {
        e = v[i][0];
        f = v[i][1];
        g = v[i][2];
        h = v[i][3];

        v[i][0] = SSE2NEON_MULTIPLY(e, 0x0e) ^ SSE2NEON_MULTIPLY(f, 0x0b) ^
                  SSE2NEON_MULTIPLY(g, 0x0d) ^ SSE2NEON_MULTIPLY(h, 0x09);
        v[i][1] = SSE2NEON_MULTIPLY(e, 0x09) ^ SSE2NEON_MULTIPLY(f, 0x0e) ^
                  SSE2NEON_MULTIPLY(g, 0x0b) ^ SSE2NEON_MULTIPLY(h, 0x0d);
        v[i][2] = SSE2NEON_MULTIPLY(e, 0x0d) ^ SSE2NEON_MULTIPLY(f, 0x09) ^
                  SSE2NEON_MULTIPLY(g, 0x0e) ^ SSE2NEON_MULTIPLY(h, 0x0b);
        v[i][3] = SSE2NEON_MULTIPLY(e, 0x0b) ^ SSE2NEON_MULTIPLY(f, 0x0d) ^
                  SSE2NEON_MULTIPLY(g, 0x09) ^ SSE2NEON_MULTIPLY(h, 0x0e);
    }

    return vreinterpretq_m128i_u8(vld1q_u8((uint8_t *) v)) ^ RoundKey;
#endif
}

// Perform the last round of an AES encryption flow on data (state) in a using
// the round key in RoundKey, and store the result in dst.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_aesenclast_si128
FORCE_INLINE __m128i _mm_aesenclast_si128(__m128i a, __m128i RoundKey)
{
#if defined(__aarch64__)
    static const uint8_t shift_rows[] = {
        0x0, 0x5, 0xa, 0xf, 0x4, 0x9, 0xe, 0x3,
        0x8, 0xd, 0x2, 0x7, 0xc, 0x1, 0x6, 0xb,
    };

    uint8x16_t v;
    uint8x16_t w = vreinterpretq_u8_m128i(a);

    // shift rows
    w = vqtbl1q_u8(w, vld1q_u8(shift_rows));

    // sub bytes
    v = vqtbl4q_u8(_sse2neon_vld1q_u8_x4(_sse2neon_sbox), w);
    v = vqtbx4q_u8(v, _sse2neon_vld1q_u8_x4(_sse2neon_sbox + 0x40), w - 0x40);
    v = vqtbx4q_u8(v, _sse2neon_vld1q_u8_x4(_sse2neon_sbox + 0x80), w - 0x80);
    v = vqtbx4q_u8(v, _sse2neon_vld1q_u8_x4(_sse2neon_sbox + 0xc0), w - 0xc0);

    // add round key
    return vreinterpretq_m128i_u8(v) ^ RoundKey;

#else /* ARMv7-A implementation */
    uint8_t v[16] = {
        _sse2neon_sbox[vgetq_lane_u8(vreinterpretq_u8_m128i(a), 0)],
        _sse2neon_sbox[vgetq_lane_u8(vreinterpretq_u8_m128i(a), 5)],
        _sse2neon_sbox[vgetq_lane_u8(vreinterpretq_u8_m128i(a), 10)],
        _sse2neon_sbox[vgetq_lane_u8(vreinterpretq_u8_m128i(a), 15)],
        _sse2neon_sbox[vgetq_lane_u8(vreinterpretq_u8_m128i(a), 4)],
        _sse2neon_sbox[vgetq_lane_u8(vreinterpretq_u8_m128i(a), 9)],
        _sse2neon_sbox[vgetq_lane_u8(vreinterpretq_u8_m128i(a), 14)],
        _sse2neon_sbox[vgetq_lane_u8(vreinterpretq_u8_m128i(a), 3)],
        _sse2neon_sbox[vgetq_lane_u8(vreinterpretq_u8_m128i(a), 8)],
        _sse2neon_sbox[vgetq_lane_u8(vreinterpretq_u8_m128i(a), 13)],
        _sse2neon_sbox[vgetq_lane_u8(vreinterpretq_u8_m128i(a), 2)],
        _sse2neon_sbox[vgetq_lane_u8(vreinterpretq_u8_m128i(a), 7)],
        _sse2neon_sbox[vgetq_lane_u8(vreinterpretq_u8_m128i(a), 12)],
        _sse2neon_sbox[vgetq_lane_u8(vreinterpretq_u8_m128i(a), 1)],
        _sse2neon_sbox[vgetq_lane_u8(vreinterpretq_u8_m128i(a), 6)],
        _sse2neon_sbox[vgetq_lane_u8(vreinterpretq_u8_m128i(a), 11)],
    };

    return vreinterpretq_m128i_u8(vld1q_u8(v)) ^ RoundKey;
#endif
}

// Perform the last round of an AES decryption flow on data (state) in a using
// the round key in RoundKey, and store the result in dst.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_aesdeclast_si128
FORCE_INLINE __m128i _mm_aesdeclast_si128(__m128i a, __m128i RoundKey)
{
#if defined(__aarch64__)
    static const uint8_t inv_shift_rows[] = {
        0x0, 0xd, 0xa, 0x7, 0x4, 0x1, 0xe, 0xb,
        0x8, 0x5, 0x2, 0xf, 0xc, 0x9, 0x6, 0x3,
    };

    uint8x16_t v;
    uint8x16_t w = vreinterpretq_u8_m128i(a);

    // inverse shift rows
    w = vqtbl1q_u8(w, vld1q_u8(inv_shift_rows));

    // inverse sub bytes
    v = vqtbl4q_u8(_sse2neon_vld1q_u8_x4(_sse2neon_rsbox), w);
    v = vqtbx4q_u8(v, _sse2neon_vld1q_u8_x4(_sse2neon_rsbox + 0x40), w - 0x40);
    v = vqtbx4q_u8(v, _sse2neon_vld1q_u8_x4(_sse2neon_rsbox + 0x80), w - 0x80);
    v = vqtbx4q_u8(v, _sse2neon_vld1q_u8_x4(_sse2neon_rsbox + 0xc0), w - 0xc0);

    // add round key
    return vreinterpretq_m128i_u8(v) ^ RoundKey;

#else /* ARMv7-A NEON implementation */
    /* FIXME: optimized for NEON */
    uint8_t v[4][4];
    uint8_t *_a = (uint8_t *) &a;
    for (int i = 0; i < 16; ++i) {
        v[((i / 4) + (i % 4)) % 4][i % 4] = _sse2neon_rsbox[_a[i]];
    }

    return vreinterpretq_m128i_u8(vld1q_u8((uint8_t *) v)) ^ RoundKey;
#endif
}

// Perform the InvMixColumns transformation on a and store the result in dst.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_aesimc_si128
FORCE_INLINE __m128i _mm_aesimc_si128(__m128i a)
{
#if defined(__aarch64__)
    static const uint8_t ror32by8[] = {
        0x1, 0x2, 0x3, 0x0, 0x5, 0x6, 0x7, 0x4,
        0x9, 0xa, 0xb, 0x8, 0xd, 0xe, 0xf, 0xc,
    };
    uint8x16_t v = vreinterpretq_u8_m128i(a);
    uint8x16_t w;

    // multiplying 'v' by 4 in GF(2^8)
    w = (v << 1) ^ (uint8x16_t) (((int8x16_t) v >> 7) & 0x1b);
    w = (w << 1) ^ (uint8x16_t) (((int8x16_t) w >> 7) & 0x1b);
    v ^= w;
    v ^= (uint8x16_t) vrev32q_u16((uint16x8_t) w);

    // multiplying 'v' by 2 in GF(2^8)
    w = (v << 1) ^ (uint8x16_t) (((int8x16_t) v >> 7) & 0x1b);
    w ^= (uint8x16_t) vrev32q_u16((uint16x8_t) v);
    w ^= vqtbl1q_u8(v ^ w, vld1q_u8(ror32by8));
    return vreinterpretq_m128i_u8(w);

#else /* ARMv7-A NEON implementation */
    uint8_t i, e, f, g, h, v[4][4];
    vst1q_u8((uint8_t *) v, vreinterpretq_u8_m128i(a));
    for (i = 0; i < 4; ++i) {
        e = v[i][0];
        f = v[i][1];
        g = v[i][2];
        h = v[i][3];

        v[i][0] = SSE2NEON_MULTIPLY(e, 0x0e) ^ SSE2NEON_MULTIPLY(f, 0x0b) ^
                  SSE2NEON_MULTIPLY(g, 0x0d) ^ SSE2NEON_MULTIPLY(h, 0x09);
        v[i][1] = SSE2NEON_MULTIPLY(e, 0x09) ^ SSE2NEON_MULTIPLY(f, 0x0e) ^
                  SSE2NEON_MULTIPLY(g, 0x0b) ^ SSE2NEON_MULTIPLY(h, 0x0d);
        v[i][2] = SSE2NEON_MULTIPLY(e, 0x0d) ^ SSE2NEON_MULTIPLY(f, 0x09) ^
                  SSE2NEON_MULTIPLY(g, 0x0e) ^ SSE2NEON_MULTIPLY(h, 0x0b);
        v[i][3] = SSE2NEON_MULTIPLY(e, 0x0b) ^ SSE2NEON_MULTIPLY(f, 0x0d) ^
                  SSE2NEON_MULTIPLY(g, 0x09) ^ SSE2NEON_MULTIPLY(h, 0x0e);
    }

    return vreinterpretq_m128i_u8(vld1q_u8((uint8_t *) v));
#endif
}

// Emits the Advanced Encryption Standard (AES) instruction aeskeygenassist.
// This instruction generates a round key for AES encryption. See
// https://kazakov.life/2017/11/01/cryptocurrency-mining-on-ios-devices/
// for details.
//
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_aeskeygenassist_si128
FORCE_INLINE __m128i _mm_aeskeygenassist_si128(__m128i a, const int rcon)
{
#if defined(__aarch64__)
    uint8x16_t _a = vreinterpretq_u8_m128i(a);
    uint8x16_t v = vqtbl4q_u8(_sse2neon_vld1q_u8_x4(_sse2neon_sbox), _a);
    v = vqtbx4q_u8(v, _sse2neon_vld1q_u8_x4(_sse2neon_sbox + 0x40), _a - 0x40);
    v = vqtbx4q_u8(v, _sse2neon_vld1q_u8_x4(_sse2neon_sbox + 0x80), _a - 0x80);
    v = vqtbx4q_u8(v, _sse2neon_vld1q_u8_x4(_sse2neon_sbox + 0xc0), _a - 0xc0);

    uint32x4_t select_mask = {0xffffffff, 0x0, 0xffffffff, 0x0};
    uint64x2_t v_mask = vshrq_n_u64(vreinterpretq_u64_u8(v), 32);
    uint32x4_t x = vbslq_u32(select_mask, vreinterpretq_u32_u64(v_mask),
                             vreinterpretq_u32_u8(v));
    uint32x4_t ror_x = vorrq_u32(vshrq_n_u32(x, 8), vshlq_n_u32(x, 24));
    uint32x4_t ror_xor_x = veorq_u32(ror_x, vdupq_n_u32(rcon));

    return vreinterpretq_m128i_u32(vbslq_u32(select_mask, x, ror_xor_x));

#else /* ARMv7-A NEON implementation */
    uint32_t X1 = _mm_cvtsi128_si32(_mm_shuffle_epi32(a, 0x55));
    uint32_t X3 = _mm_cvtsi128_si32(_mm_shuffle_epi32(a, 0xFF));
    for (int i = 0; i < 4; ++i) {
        ((uint8_t *) &X1)[i] = _sse2neon_sbox[((uint8_t *) &X1)[i]];
        ((uint8_t *) &X3)[i] = _sse2neon_sbox[((uint8_t *) &X3)[i]];
    }
    return _mm_set_epi32(((X3 >> 8) | (X3 << 24)) ^ rcon, X3,
                         ((X1 >> 8) | (X1 << 24)) ^ rcon, X1);
#endif
}
#undef SSE2NEON_AES_SBOX
#undef SSE2NEON_AES_RSBOX

#if defined(__aarch64__)
#undef SSE2NEON_XT
#undef SSE2NEON_MULTIPLY
#endif

#else /* __ARM_FEATURE_CRYPTO */
// Implements equivalent of 'aesenc' by combining AESE (with an empty key) and
// AESMC and then manually applying the real key as an xor operation. This
// unfortunately means an additional xor op; the compiler should be able to
// optimize this away for repeated calls however. See
// https://blog.michaelbrase.com/2018/05/08/emulating-x86-aes-intrinsics-on-armv8-a
// for more details.
FORCE_INLINE __m128i _mm_aesenc_si128(__m128i a, __m128i b)
{
    return vreinterpretq_m128i_u8(
        vaesmcq_u8(vaeseq_u8(vreinterpretq_u8_m128i(a), vdupq_n_u8(0))) ^
        vreinterpretq_u8_m128i(b));
}

// Perform one round of an AES decryption flow on data (state) in a using the
// round key in RoundKey, and store the result in dst.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_aesdec_si128
FORCE_INLINE __m128i _mm_aesdec_si128(__m128i a, __m128i RoundKey)
{
    return vreinterpretq_m128i_u8(veorq_u8(
        vaesimcq_u8(vaesdq_u8(vreinterpretq_u8_m128i(a), vdupq_n_u8(0))),
        vreinterpretq_u8_m128i(RoundKey)));
}

// Perform the last round of an AES encryption flow on data (state) in a using
// the round key in RoundKey, and store the result in dst.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_aesenclast_si128
FORCE_INLINE __m128i _mm_aesenclast_si128(__m128i a, __m128i RoundKey)
{
    return _mm_xor_si128(vreinterpretq_m128i_u8(vaeseq_u8(
                             vreinterpretq_u8_m128i(a), vdupq_n_u8(0))),
                         RoundKey);
}

// Perform the last round of an AES decryption flow on data (state) in a using
// the round key in RoundKey, and store the result in dst.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_aesdeclast_si128
FORCE_INLINE __m128i _mm_aesdeclast_si128(__m128i a, __m128i RoundKey)
{
    return vreinterpretq_m128i_u8(
               vaesdq_u8(vreinterpretq_u8_m128i(a), vdupq_n_u8(0))) ^
           vreinterpretq_u8_m128i(RoundKey);
}

// Perform the InvMixColumns transformation on a and store the result in dst.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_aesimc_si128
FORCE_INLINE __m128i _mm_aesimc_si128(__m128i a)
{
    return vreinterpretq_m128i_u8(vaesimcq_u8(a));
}

// Assist in expanding the AES cipher key by computing steps towards generating
// a round key for encryption cipher using data from a and an 8-bit round
// constant specified in imm8, and store the result in dst."
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_aeskeygenassist_si128
FORCE_INLINE __m128i _mm_aeskeygenassist_si128(__m128i a, const int rcon)
{
    // AESE does ShiftRows and SubBytes on A
    uint8x16_t u8 = vaeseq_u8(vreinterpretq_u8_m128i(a), vdupq_n_u8(0));

    uint8x16_t dest = {
        // Undo ShiftRows step from AESE and extract X1 and X3
        u8[0x4], u8[0x1], u8[0xE], u8[0xB],  // SubBytes(X1)
        u8[0x1], u8[0xE], u8[0xB], u8[0x4],  // ROT(SubBytes(X1))
        u8[0xC], u8[0x9], u8[0x6], u8[0x3],  // SubBytes(X3)
        u8[0x9], u8[0x6], u8[0x3], u8[0xC],  // ROT(SubBytes(X3))
    };
    uint32x4_t r = {0, (unsigned) rcon, 0, (unsigned) rcon};
    return vreinterpretq_m128i_u8(dest) ^ vreinterpretq_m128i_u32(r);
}
#endif

/* Others */

// Perform a carry-less multiplication of two 64-bit integers, selected from a
// and b according to imm8, and store the results in dst.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_clmulepi64_si128
FORCE_INLINE __m128i _mm_clmulepi64_si128(__m128i _a, __m128i _b, const int imm)
{
    uint64x2_t a = vreinterpretq_u64_m128i(_a);
    uint64x2_t b = vreinterpretq_u64_m128i(_b);
    switch (imm & 0x11) {
    case 0x00:
        return vreinterpretq_m128i_u64(
            _sse2neon_vmull_p64(vget_low_u64(a), vget_low_u64(b)));
    case 0x01:
        return vreinterpretq_m128i_u64(
            _sse2neon_vmull_p64(vget_high_u64(a), vget_low_u64(b)));
    case 0x10:
        return vreinterpretq_m128i_u64(
            _sse2neon_vmull_p64(vget_low_u64(a), vget_high_u64(b)));
    case 0x11:
        return vreinterpretq_m128i_u64(
            _sse2neon_vmull_p64(vget_high_u64(a), vget_high_u64(b)));
    default:
        abort();
    }
}

FORCE_INLINE unsigned int _sse2neon_mm_get_denormals_zero_mode()
{
    union {
        fpcr_bitfield field;
#if defined(__aarch64__)
        uint64_t value;
#else
        uint32_t value;
#endif
    } r;

#if defined(__aarch64__)
    __asm__ __volatile__("mrs %0, FPCR" : "=r"(r.value)); /* read */
#else
    __asm__ __volatile__("vmrs %0, FPSCR" : "=r"(r.value)); /* read */
#endif

    return r.field.bit24 ? _MM_DENORMALS_ZERO_ON : _MM_DENORMALS_ZERO_OFF;
}

// Count the number of bits set to 1 in unsigned 32-bit integer a, and
// return that count in dst.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_popcnt_u32
FORCE_INLINE int _mm_popcnt_u32(unsigned int a)
{
#if defined(__aarch64__)
#if __has_builtin(__builtin_popcount)
    return __builtin_popcount(a);
#else
    return (int) vaddlv_u8(vcnt_u8(vcreate_u8((uint64_t) a)));
#endif
#else
    uint32_t count = 0;
    uint8x8_t input_val, count8x8_val;
    uint16x4_t count16x4_val;
    uint32x2_t count32x2_val;

    input_val = vld1_u8((uint8_t *) &a);
    count8x8_val = vcnt_u8(input_val);
    count16x4_val = vpaddl_u8(count8x8_val);
    count32x2_val = vpaddl_u16(count16x4_val);

    vst1_u32(&count, count32x2_val);
    return count;
#endif
}

// Count the number of bits set to 1 in unsigned 64-bit integer a, and
// return that count in dst.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_popcnt_u64
FORCE_INLINE int64_t _mm_popcnt_u64(uint64_t a)
{
#if defined(__aarch64__)
#if __has_builtin(__builtin_popcountll)
    return __builtin_popcountll(a);
#else
    return (int64_t) vaddlv_u8(vcnt_u8(vcreate_u8(a)));
#endif
#else
    uint64_t count = 0;
    uint8x8_t input_val, count8x8_val;
    uint16x4_t count16x4_val;
    uint32x2_t count32x2_val;
    uint64x1_t count64x1_val;

    input_val = vld1_u8((uint8_t *) &a);
    count8x8_val = vcnt_u8(input_val);
    count16x4_val = vpaddl_u8(count8x8_val);
    count32x2_val = vpaddl_u16(count16x4_val);
    count64x1_val = vpaddl_u32(count32x2_val);
    vst1_u64(&count, count64x1_val);
    return count;
#endif
}

FORCE_INLINE void _sse2neon_mm_set_denormals_zero_mode(unsigned int flag)
{
    // AArch32 Advanced SIMD arithmetic always uses the Flush-to-zero setting,
    // regardless of the value of the FZ bit.
    union {
        fpcr_bitfield field;
#if defined(__aarch64__)
        uint64_t value;
#else
        uint32_t value;
#endif
    } r;

#if defined(__aarch64__)
    __asm__ __volatile__("mrs %0, FPCR" : "=r"(r.value)); /* read */
#else
    __asm__ __volatile__("vmrs %0, FPSCR" : "=r"(r.value)); /* read */
#endif

    r.field.bit24 = (flag & _MM_DENORMALS_ZERO_MASK) == _MM_DENORMALS_ZERO_ON;

#if defined(__aarch64__)
    __asm__ __volatile__("msr FPCR, %0" ::"r"(r)); /* write */
#else
    __asm__ __volatile__("vmsr FPSCR, %0" ::"r"(r));        /* write */
#endif
}

// Return the current 64-bit value of the processor's time-stamp counter.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=rdtsc

FORCE_INLINE uint64_t _rdtsc(void)
{
#if defined(__aarch64__)
    uint64_t val;

    /* According to ARM DDI 0487F.c, from Armv8.0 to Armv8.5 inclusive, the
     * system counter is at least 56 bits wide; from Armv8.6, the counter
     * must be 64 bits wide.  So the system counter could be less than 64
     * bits wide and it is attributed with the flag 'cap_user_time_short'
     * is true.
     */
    __asm__ __volatile__("mrs %0, cntvct_el0" : "=r"(val));

    return val;
#else
    uint32_t pmccntr, pmuseren, pmcntenset;
    // Read the user mode Performance Monitoring Unit (PMU)
    // User Enable Register (PMUSERENR) access permissions.
    __asm__ __volatile__("mrc p15, 0, %0, c9, c14, 0" : "=r"(pmuseren));
    if (pmuseren & 1) {  // Allows reading PMUSERENR for user mode code.
        __asm__ __volatile__("mrc p15, 0, %0, c9, c12, 1" : "=r"(pmcntenset));
        if (pmcntenset & 0x80000000UL) {  // Is it counting?
            __asm__ __volatile__("mrc p15, 0, %0, c9, c13, 0" : "=r"(pmccntr));
            // The counter is set up to count every 64th cycle
            return (uint64_t) (pmccntr) << 6;
        }
    }

    // Fallback to syscall as we can't enable PMUSERENR in user mode.
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t) (tv.tv_sec) * 1000000 + tv.tv_usec;
#endif
}

#if defined(__GNUC__) || defined(__clang__)
#pragma pop_macro("ALIGN_STRUCT")
#pragma pop_macro("FORCE_INLINE")
#endif

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC pop_options
#endif

#endif

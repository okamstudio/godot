//
//  m3_math_utils.h
//
//  Created by Volodymyr Shymanksyy on 8/10/19.
//  Copyright © 2019 Volodymyr Shymanskyy. All rights reserved.
//

#ifndef m3_math_utils_h
#define m3_math_utils_h

#include "m3_core.h"

#include <limits.h>

#if defined(M3_COMPILER_MSVC)

#include <intrin.h>

#define __builtin_popcount    __popcnt

static inline
int __builtin_ctz(uint32_t x) {
    unsigned long ret;
    _BitScanForward(&ret, x);
    return (int)ret;
}

static inline
int __builtin_clz(uint32_t x) {
    unsigned long ret;
    _BitScanReverse(&ret, x);
    return (int)(31 ^ ret);
}



#ifdef _WIN64

#define __builtin_popcountll  __popcnt64

static inline
int __builtin_ctzll(uint64_t value) {
    unsigned long ret;
     _BitScanForward64(&ret, value);
    return (int)ret;
}

static inline
int __builtin_clzll(uint64_t value) {
    unsigned long ret;
    _BitScanReverse64(&ret, value);
    return (int)(63 ^ ret);
}

#else // _WIN64

#define __builtin_popcountll(x)  (__popcnt((x) & 0xFFFFFFFF) + __popcnt((x) >> 32))

static inline
int __builtin_ctzll(uint64_t value) {
    //if (value == 0) return 64; // Note: ctz(0) result is undefined anyway
    uint32_t msh = (uint32_t)(value >> 32);
    uint32_t lsh = (uint32_t)(value & 0xFFFFFFFF);
    if (lsh != 0) return __builtin_ctz(lsh);
    return 32 + __builtin_ctz(msh);
}

static inline
int __builtin_clzll(uint64_t value) {
    //if (value == 0) return 64; // Note: clz(0) result is undefined anyway
    uint32_t msh = (uint32_t)(value >> 32);
    uint32_t lsh = (uint32_t)(value & 0xFFFFFFFF);
    if (msh != 0) return __builtin_clz(msh);
    return 32 + __builtin_clz(lsh);
}

#endif // _WIN64

#endif // defined(M3_COMPILER_MSVC)


// TODO: not sure why, signbit is actually defined in math.h
#if (defined(ESP8266) || defined(ESP32)) && !defined(signbit)
    #define signbit(__x) \
            ((sizeof(__x) == sizeof(float))  ?  __signbitf(__x) : __signbitd(__x))
#endif

#if defined(__AVR__)

static inline
float rintf( float arg ) {
  union { float f; uint32_t i; } u;
  u.f = arg;
  uint32_t ux = u.i & 0x7FFFFFFF;
  if (M3_UNLIKELY(ux == 0 || ux > 0x5A000000)) {
    return arg;
  }
  return (float)lrint(arg);
}

static inline
double rint( double arg ) {
  union { double f; uint32_t i[2]; } u;
  u.f = arg;
  uint32_t ux = u.i[1] & 0x7FFFFFFF;
  if (M3_UNLIKELY((ux == 0 && u.i[0] == 0) || ux > 0x433FFFFF)) {
    return arg;
  }
  return (double)lrint(arg);
}

//TODO
static inline
uint64_t strtoull(const char* str, char** endptr, int base) {
  return 0;
}

#endif

/*
 * Rotr, Rotl
 */

static inline
u32 rotl32(u32 n, unsigned c) {
    const unsigned mask = CHAR_BIT * sizeof(n) - 1;
    c &= mask & 31;
    return (n << c) | (n >> ((-c) & mask));
}

static inline
u32 rotr32(u32 n, unsigned c) {
    const unsigned mask = CHAR_BIT * sizeof(n) - 1;
    c &= mask & 31;
    return (n >> c) | (n << ((-c) & mask));
}

static inline
u64 rotl64(u64 n, unsigned c) {
    const unsigned mask = CHAR_BIT * sizeof(n) - 1;
    c &= mask & 63;
    return (n << c) | (n >> ((-c) & mask));
}

static inline
u64 rotr64(u64 n, unsigned c) {
    const unsigned mask = CHAR_BIT * sizeof(n) - 1;
    c &= mask & 63;
    return (n >> c) | (n << ((-c) & mask));
}

/*
 * Integer Div, Rem
 */

#define OP_DIV_U(RES, A, B)                                      \
    if (M3_UNLIKELY(B == 0)) newTrap (m3Err_trapDivisionByZero);    \
    RES = A / B;

#define OP_REM_U(RES, A, B)                                      \
    if (M3_UNLIKELY(B == 0)) newTrap (m3Err_trapDivisionByZero);    \
    RES = A % B;

// 2's complement detection
#if (INT_MIN != -INT_MAX)

    #define OP_DIV_S(RES, A, B, TYPE_MIN)                         \
        if (M3_UNLIKELY(B == 0)) newTrap (m3Err_trapDivisionByZero); \
        if (M3_UNLIKELY(B == -1 and A == TYPE_MIN)) {                \
            newTrap (m3Err_trapIntegerOverflow);                  \
        }                                                         \
        RES = A / B;

    #define OP_REM_S(RES, A, B, TYPE_MIN)                         \
        if (M3_UNLIKELY(B == 0)) newTrap (m3Err_trapDivisionByZero); \
        if (M3_UNLIKELY(B == -1 and A == TYPE_MIN)) RES = 0;         \
        else RES = A % B;

#else

    #define OP_DIV_S(RES, A, B, TYPE_MIN) OP_DIV_U(RES, A, B)
    #define OP_REM_S(RES, A, B, TYPE_MIN) OP_REM_U(RES, A, B)

#endif

/*
 * Trunc
 */

#define OP_TRUNC(RES, A, TYPE, RMIN, RMAX)                  \
    if (M3_UNLIKELY(isnan(A))) {                               \
        newTrap (m3Err_trapIntegerConversion);              \
    }                                                       \
    if (M3_UNLIKELY(A <= RMIN or A >= RMAX)) {                 \
        newTrap (m3Err_trapIntegerOverflow);                \
    }                                                       \
    RES = (TYPE)A;


#define OP_I32_TRUNC_F32(RES, A)    OP_TRUNC(RES, A, i32, -2147483904.0f, 2147483648.0f)
#define OP_U32_TRUNC_F32(RES, A)    OP_TRUNC(RES, A, u32,          -1.0f, 4294967296.0f)
#define OP_I32_TRUNC_F64(RES, A)    OP_TRUNC(RES, A, i32, -2147483649.0 , 2147483648.0 )
#define OP_U32_TRUNC_F64(RES, A)    OP_TRUNC(RES, A, u32,          -1.0 , 4294967296.0 )

#define OP_I64_TRUNC_F32(RES, A)    OP_TRUNC(RES, A, i64, -9223373136366403584.0f,  9223372036854775808.0f)
#define OP_U64_TRUNC_F32(RES, A)    OP_TRUNC(RES, A, u64,                   -1.0f, 18446744073709551616.0f)
#define OP_I64_TRUNC_F64(RES, A)    OP_TRUNC(RES, A, i64, -9223372036854777856.0 ,  9223372036854775808.0 )
#define OP_U64_TRUNC_F64(RES, A)    OP_TRUNC(RES, A, u64,                   -1.0 , 18446744073709551616.0 )

#define OP_TRUNC_SAT(RES, A, TYPE, RMIN, RMAX, IMIN, IMAX)  \
    if (M3_UNLIKELY(isnan(A))) {                               \
        RES = 0;                                            \
    } else if (M3_UNLIKELY(A <= RMIN)) {                       \
        RES = IMIN;                                         \
    } else if (M3_UNLIKELY(A >= RMAX)) {                       \
        RES = IMAX;                                         \
    } else {                                                \
        RES = (TYPE)A;                                      \
    }

#define OP_I32_TRUNC_SAT_F32(RES, A)    OP_TRUNC_SAT(RES, A, i32, -2147483904.0f, 2147483648.0f,   INT32_MIN,  INT32_MAX)
#define OP_U32_TRUNC_SAT_F32(RES, A)    OP_TRUNC_SAT(RES, A, u32,          -1.0f, 4294967296.0f,         0UL, UINT32_MAX)
#define OP_I32_TRUNC_SAT_F64(RES, A)    OP_TRUNC_SAT(RES, A, i32, -2147483649.0 , 2147483648.0,    INT32_MIN,  INT32_MAX)
#define OP_U32_TRUNC_SAT_F64(RES, A)    OP_TRUNC_SAT(RES, A, u32,          -1.0 , 4294967296.0,          0UL, UINT32_MAX)

#define OP_I64_TRUNC_SAT_F32(RES, A)    OP_TRUNC_SAT(RES, A, i64, -9223373136366403584.0f,  9223372036854775808.0f, INT64_MIN,  INT64_MAX)
#define OP_U64_TRUNC_SAT_F32(RES, A)    OP_TRUNC_SAT(RES, A, u64,                   -1.0f, 18446744073709551616.0f,      0ULL, UINT64_MAX)
#define OP_I64_TRUNC_SAT_F64(RES, A)    OP_TRUNC_SAT(RES, A, i64, -9223372036854777856.0 ,  9223372036854775808.0,  INT64_MIN,  INT64_MAX)
#define OP_U64_TRUNC_SAT_F64(RES, A)    OP_TRUNC_SAT(RES, A, u64,                   -1.0 , 18446744073709551616.0,       0ULL, UINT64_MAX)

/*
 * Min, Max
 */

#if d_m3HasFloat

#include <math.h>

static inline
f32 min_f32(f32 a, f32 b) {
    if (M3_UNLIKELY(isnan(a) or isnan(b))) return NAN;
    if (M3_UNLIKELY(a == 0 and a == b)) return signbit(a) ? a : b;
    return a > b ? b : a;
}

static inline
f32 max_f32(f32 a, f32 b) {
    if (M3_UNLIKELY(isnan(a) or isnan(b))) return NAN;
    if (M3_UNLIKELY(a == 0 and a == b)) return signbit(a) ? b : a;
    return a > b ? a : b;
}

static inline
f64 min_f64(f64 a, f64 b) {
    if (M3_UNLIKELY(isnan(a) or isnan(b))) return NAN;
    if (M3_UNLIKELY(a == 0 and a == b)) return signbit(a) ? a : b;
    return a > b ? b : a;
}

static inline
f64 max_f64(f64 a, f64 b) {
    if (M3_UNLIKELY(isnan(a) or isnan(b))) return NAN;
    if (M3_UNLIKELY(a == 0 and a == b)) return signbit(a) ? b : a;
    return a > b ? a : b;
}
#endif

#endif // m3_math_utils_h

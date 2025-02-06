//
//  wasm3_defs.h
//
//  Created by Volodymyr Shymanskyy on 11/20/19.
//  Copyright © 2019 Volodymyr Shymanskyy. All rights reserved.
//

#ifndef wasm3_defs_h
#define wasm3_defs_h

#define M3_STR__(x) #x
#define M3_STR(x)   M3_STR__(x)

#define M3_CONCAT__(a,b) a##b
#define M3_CONCAT(a,b)   M3_CONCAT__(a,b)

/*
 * Detect compiler
 */

# if defined(__clang__)
#  define M3_COMPILER_CLANG 1
# elif defined(__INTEL_COMPILER)
#  define M3_COMPILER_ICC 1
# elif defined(__GNUC__) || defined(__GNUG__)
#  define M3_COMPILER_GCC 1
# elif defined(_MSC_VER)
#  define M3_COMPILER_MSVC 1
# else
#  warning "Compiler not detected"
# endif

# if defined(M3_COMPILER_CLANG)
#  if defined(WIN32)
#   define M3_COMPILER_VER __VERSION__ " for Windows"
#  else
#   define M3_COMPILER_VER __VERSION__
#  endif
# elif defined(M3_COMPILER_GCC)
#  define M3_COMPILER_VER "GCC " __VERSION__
# elif defined(M3_COMPILER_ICC)
#  define M3_COMPILER_VER __VERSION__
# elif defined(M3_COMPILER_MSVC)
#  define M3_COMPILER_VER "MSVC " M3_STR(_MSC_VER)
# else
#  define M3_COMPILER_VER "unknown"
# endif

# ifdef __has_feature
#  define M3_COMPILER_HAS_FEATURE(x) __has_feature(x)
# else
#  define M3_COMPILER_HAS_FEATURE(x) 0
# endif

# ifdef __has_builtin
#  define M3_COMPILER_HAS_BUILTIN(x) __has_builtin(x)
# else
#  define M3_COMPILER_HAS_BUILTIN(x) 0
# endif

# ifdef __has_attribute
#  define M3_COMPILER_HAS_ATTRIBUTE(x) __has_attribute(x)
# else
#  define M3_COMPILER_HAS_ATTRIBUTE(x) 0
# endif

/*
 * Detect endianness
 */

# if defined(M3_COMPILER_MSVC)
#  define M3_LITTLE_ENDIAN
# elif defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#  define M3_LITTLE_ENDIAN
# elif defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#  define M3_BIG_ENDIAN
# else
#  error "Byte order not detected"
# endif

/*
 * Detect platform
 */

# if defined(M3_COMPILER_CLANG) || defined(M3_COMPILER_GCC) || defined(M3_COMPILER_ICC)
#  if defined(__wasm__)
#   define M3_ARCH "wasm"

#  elif defined(__x86_64__)
#   define M3_ARCH "x86_64"

#  elif defined(__i386__)
#   define M3_ARCH "i386"

#  elif defined(__aarch64__)
#   define M3_ARCH "arm64-v8a"

#  elif defined(__arm__)
#   if defined(__ARM_ARCH_7A__)
#    if defined(__ARM_NEON__)
#     if defined(__ARM_PCS_VFP)
#      define M3_ARCH "arm-v7a/NEON hard-float"
#     else
#      define M3_ARCH "arm-v7a/NEON"
#     endif
#    else
#     if defined(__ARM_PCS_VFP)
#      define M3_ARCH "arm-v7a hard-float"
#     else
#      define M3_ARCH "arm-v7a"
#     endif
#    endif
#   else
#    define M3_ARCH "arm"
#   endif

#  elif defined(__riscv)
#   if defined(__riscv_32e)
#    define _M3_ARCH_RV "rv32e"
#   elif __riscv_xlen == 128
#    define _M3_ARCH_RV "rv128i"
#   elif __riscv_xlen == 64
#    define _M3_ARCH_RV "rv64i"
#   elif __riscv_xlen == 32
#    define _M3_ARCH_RV "rv32i"
#   endif
#   if defined(__riscv_muldiv)
#    define _M3_ARCH_RV_M _M3_ARCH_RV "m"
#   else
#    define _M3_ARCH_RV_M _M3_ARCH_RV
#   endif
#   if defined(__riscv_atomic)
#    define _M3_ARCH_RV_A _M3_ARCH_RV_M "a"
#   else
#    define _M3_ARCH_RV_A _M3_ARCH_RV_M
#   endif
#   if defined(__riscv_flen)
#    define _M3_ARCH_RV_F _M3_ARCH_RV_A "f"
#   else
#    define _M3_ARCH_RV_F _M3_ARCH_RV_A
#   endif
#   if defined(__riscv_flen) && __riscv_flen >= 64
#    define _M3_ARCH_RV_D _M3_ARCH_RV_F "d"
#   else
#    define _M3_ARCH_RV_D _M3_ARCH_RV_F
#   endif
#   if defined(__riscv_compressed)
#    define _M3_ARCH_RV_C _M3_ARCH_RV_D "c"
#   else
#    define _M3_ARCH_RV_C _M3_ARCH_RV_D
#   endif
#   define M3_ARCH _M3_ARCH_RV_C

#  elif defined(__mips__)
#   if defined(__MIPSEB__) && defined(__mips64)
#    define M3_ARCH "mips64 " _MIPS_ARCH
#   elif defined(__MIPSEL__) && defined(__mips64)
#    define M3_ARCH "mips64el " _MIPS_ARCH
#   elif defined(__MIPSEB__)
#    define M3_ARCH "mips " _MIPS_ARCH
#   elif defined(__MIPSEL__)
#    define M3_ARCH "mipsel " _MIPS_ARCH
#   endif

#  elif defined(__PPC__)
#   if defined(__PPC64__) && defined(__LITTLE_ENDIAN__)
#    define M3_ARCH "ppc64le"
#   elif defined(__PPC64__)
#    define M3_ARCH "ppc64"
#   else
#    define M3_ARCH "ppc"
#   endif

#  elif defined(__sparc__)
#   if defined(__arch64__)
#    define M3_ARCH "sparc64"
#   else
#    define M3_ARCH "sparc"
#   endif

#  elif defined(__s390x__)
#   define M3_ARCH "s390x"

#  elif defined(__alpha__)
#   define M3_ARCH "alpha"

#  elif defined(__m68k__)
#   define M3_ARCH "m68k"

#  elif defined(__xtensa__)
#   define M3_ARCH "xtensa"

#  elif defined(__arc__)
#   define M3_ARCH "arc32"

#  elif defined(__AVR__)
#   define M3_ARCH "avr"
#  endif
# endif

# if defined(M3_COMPILER_MSVC)
#  if defined(_M_X64)
#   define M3_ARCH "x86_64"
#  elif defined(_M_IX86)
#   define M3_ARCH "i386"
#  elif defined(_M_ARM64)
#   define M3_ARCH "arm64"
#  elif defined(_M_ARM)
#   define M3_ARCH "arm"
#  endif
# endif

# if !defined(M3_ARCH)
#  warning "Architecture not detected"
#  define M3_ARCH "unknown"
# endif

/*
 * Byte swapping (for Big-Endian systems only)
 */

# if defined(M3_COMPILER_MSVC)
#  define m3_bswap16(x)     _byteswap_ushort((x))
#  define m3_bswap32(x)     _byteswap_ulong((x))
#  define m3_bswap64(x)     _byteswap_uint64((x))
# elif defined(M3_COMPILER_GCC) && ((__GNUC__ > 4) || (__GNUC__ == 4 && __GNUC_MINOR__ >= 8))
// __builtin_bswap32/64 added in gcc 4.3, __builtin_bswap16 added in gcc 4.8
#  define m3_bswap16(x)     __builtin_bswap16((x))
#  define m3_bswap32(x)     __builtin_bswap32((x))
#  define m3_bswap64(x)     __builtin_bswap64((x))
# elif defined(M3_COMPILER_CLANG) && M3_COMPILER_HAS_BUILTIN(__builtin_bswap16)
#  define m3_bswap16(x)     __builtin_bswap16((x))
#  define m3_bswap32(x)     __builtin_bswap32((x))
#  define m3_bswap64(x)     __builtin_bswap64((x))
# elif defined(M3_COMPILER_ICC)
#  define m3_bswap16(x)     __builtin_bswap16((x))
#  define m3_bswap32(x)     __builtin_bswap32((x))
#  define m3_bswap64(x)     __builtin_bswap64((x))
# else
#  ifdef __linux__
#   include <endian.h>
#  else
#   include <stdint.h>
#  endif
#  if defined(__bswap_16)
#   define m3_bswap16(x)     __bswap_16((x))
#   define m3_bswap32(x)     __bswap_32((x))
#   define m3_bswap64(x)     __bswap_64((x))
#  else
#   warning "Using naive (probably slow) bswap operations"
    static inline
    uint16_t m3_bswap16(uint16_t x) {
      return ((( x  >> 8 ) & 0xffu ) | (( x  & 0xffu ) << 8 ));
    }
    static inline
    uint32_t m3_bswap32(uint32_t x) {
      return ((( x & 0xff000000u ) >> 24 ) |
              (( x & 0x00ff0000u ) >> 8  ) |
              (( x & 0x0000ff00u ) << 8  ) |
              (( x & 0x000000ffu ) << 24 ));
    }
    static inline
    uint64_t m3_bswap64(uint64_t x) {
      return ((( x & 0xff00000000000000ull ) >> 56 ) |
              (( x & 0x00ff000000000000ull ) >> 40 ) |
              (( x & 0x0000ff0000000000ull ) >> 24 ) |
              (( x & 0x000000ff00000000ull ) >> 8  ) |
              (( x & 0x00000000ff000000ull ) << 8  ) |
              (( x & 0x0000000000ff0000ull ) << 24 ) |
              (( x & 0x000000000000ff00ull ) << 40 ) |
              (( x & 0x00000000000000ffull ) << 56 ));
    }
#  endif
# endif

/*
 * Bit ops
 */
#define m3_isBitSet(val, pos)           ((val & (1 << pos)) != 0)

/*
 * Other
 */

# if defined(M3_COMPILER_GCC) || defined(M3_COMPILER_CLANG) || defined(M3_COMPILER_ICC)
#  define M3_UNLIKELY(x) __builtin_expect(!!(x), 0)
#  define M3_LIKELY(x)   __builtin_expect(!!(x), 1)
# else
#  define M3_UNLIKELY(x) (x)
#  define M3_LIKELY(x)   (x)
# endif

#endif // wasm3_defs_h

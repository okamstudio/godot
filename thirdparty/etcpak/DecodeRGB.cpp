#include "DecodeRGB.hpp"
#include "Tables.hpp"
#include "Math.hpp"

#include <string.h>

#ifdef __ARM_NEON
#  include <arm_neon.h>
#endif

#if defined __SSE4_1__ || defined __AVX2__ || defined _MSC_VER
#  ifdef _MSC_VER
#    include <intrin.h>
#    include <Windows.h>
#    define _bswap(x) _byteswap_ulong(x)
#    define _bswap64(x) _byteswap_uint64(x)
#  else
#    include <x86intrin.h>
#  endif
#endif

#ifndef _bswap
#  define _bswap(x) __builtin_bswap32(x)
#  define _bswap64(x) __builtin_bswap64(x)
#endif

static uint8_t table59T58H[8] = { 3,6,11,16,23,32,41,64 };

namespace
{

static etcpak_force_inline int32_t expand6(uint32_t value)
{
    return (value << 2) | (value >> 4);
}

static etcpak_force_inline int32_t expand7(uint32_t value)
{
    return (value << 1) | (value >> 6);
}

static etcpak_force_inline void DecodeT( uint64_t block, uint32_t* dst, uint32_t w )
{
    const auto r0 = ( block >> 24 ) & 0x1B;
    const auto rh0 = ( r0 >> 3 ) & 0x3;
    const auto rl0 = r0 & 0x3;
    const auto g0 = ( block >> 20 ) & 0xF;
    const auto b0 = ( block >> 16 ) & 0xF;

    const auto r1 = ( block >> 12 ) & 0xF;
    const auto g1 = ( block >> 8 ) & 0xF;
    const auto b1 = ( block >> 4 ) & 0xF;

    const auto cr0 = ( ( rh0 << 6 ) | ( rl0 << 4 ) | ( rh0 << 2 ) | rl0);
    const auto cg0 = ( g0 << 4 ) | g0;
    const auto cb0 = ( b0 << 4 ) | b0;

    const auto cr1 = ( r1 << 4 ) | r1;
    const auto cg1 = ( g1 << 4 ) | g1;
    const auto cb1 = ( b1 << 4 ) | b1;

    const auto codeword_hi = ( block >> 2 ) & 0x3;
    const auto codeword_lo = block & 0x1;
    const auto codeword = ( codeword_hi << 1 ) | codeword_lo;

    const auto c2r = clampu8( cr1 + table59T58H[codeword] );
    const auto c2g = clampu8( cg1 + table59T58H[codeword] );
    const auto c2b = clampu8( cb1 + table59T58H[codeword] );

    const auto c3r = clampu8( cr1 - table59T58H[codeword] );
    const auto c3g = clampu8( cg1 - table59T58H[codeword] );
    const auto c3b = clampu8( cb1 - table59T58H[codeword] );

    const uint32_t col_tab[4] = {
        uint32_t( cr0 | ( cg0 << 8 ) | ( cb0 << 16 ) | 0xFF000000 ),
        uint32_t( c2r | ( c2g << 8 ) | ( c2b << 16 ) | 0xFF000000 ),
        uint32_t( cr1 | ( cg1 << 8 ) | ( cb1 << 16 ) | 0xFF000000 ),
        uint32_t( c3r | ( c3g << 8 ) | ( c3b << 16 ) | 0xFF000000 )
    };

    const uint32_t indexes = ( block >> 32 ) & 0xFFFFFFFF;
    for( uint8_t j = 0; j < 4; j++ )
    {
        for( uint8_t i = 0; i < 4; i++ )
        {
            //2bit indices distributed on two lane 16bit numbers
            const uint8_t index = ( ( ( indexes >> ( j + i * 4 + 16 ) ) & 0x1 ) << 1) | ( ( indexes >> ( j + i * 4 ) ) & 0x1);
            dst[j * w + i] = col_tab[index];
        }
    }
}

static etcpak_force_inline void DecodeTAlpha( uint64_t block, uint64_t alpha, uint32_t* dst, uint32_t w )
{
    const auto r0 = ( block >> 24 ) & 0x1B;
    const auto rh0 = ( r0 >> 3 ) & 0x3;
    const auto rl0 = r0 & 0x3;
    const auto g0 = ( block >> 20 ) & 0xF;
    const auto b0 = ( block >> 16 ) & 0xF;

    const auto r1 = ( block >> 12 ) & 0xF;
    const auto g1 = ( block >> 8 ) & 0xF;
    const auto b1 = ( block >> 4 ) & 0xF;

    const auto cr0 = ( ( rh0 << 6 ) | ( rl0 << 4 ) | ( rh0 << 2 ) | rl0);
    const auto cg0 = ( g0 << 4 ) | g0;
    const auto cb0 = ( b0 << 4 ) | b0;

    const auto cr1 = ( r1 << 4 ) | r1;
    const auto cg1 = ( g1 << 4 ) | g1;
    const auto cb1 = ( b1 << 4 ) | b1;

    const auto codeword_hi = ( block >> 2 ) & 0x3;
    const auto codeword_lo = block & 0x1;
    const auto codeword = (codeword_hi << 1) | codeword_lo;

    const int32_t base = alpha >> 56;
    const int32_t mul = ( alpha >> 52 ) & 0xF;
    const auto tbl = g_alpha[( alpha >> 48 ) & 0xF];

    const auto c2r = clampu8( cr1 + table59T58H[codeword] );
    const auto c2g = clampu8( cg1 + table59T58H[codeword] );
    const auto c2b = clampu8( cb1 + table59T58H[codeword] );

    const auto c3r = clampu8( cr1 - table59T58H[codeword] );
    const auto c3g = clampu8( cg1 - table59T58H[codeword] );
    const auto c3b = clampu8( cb1 - table59T58H[codeword] );

    const uint32_t col_tab[4] = {
        uint32_t( cr0 | ( cg0 << 8 ) | ( cb0 << 16 ) ),
        uint32_t( c2r | ( c2g << 8 ) | ( c2b << 16 ) ),
        uint32_t( cr1 | ( cg1 << 8 ) | ( cb1 << 16 ) ),
        uint32_t( c3r | ( c3g << 8 ) | ( c3b << 16 ) )
    };

    const uint32_t indexes = ( block >> 32 ) & 0xFFFFFFFF;
    for( uint8_t j = 0; j < 4; j++ )
    {
        for( uint8_t i = 0; i < 4; i++ )
        {
            //2bit indices distributed on two lane 16bit numbers
            const uint8_t index = ( ( ( indexes >> ( j + i * 4 + 16 ) ) & 0x1 ) << 1 ) | ( ( indexes >> ( j + i * 4 ) ) & 0x1 );
            const auto amod = tbl[( alpha >> ( 45 - j * 3 - i * 12 ) ) & 0x7];
            const uint32_t a = clampu8( base + amod * mul );
            dst[j * w + i] = col_tab[index] | ( a << 24 );
        }
    }
}

static etcpak_force_inline void DecodeH( uint64_t block, uint32_t* dst, uint32_t w )
{
    const uint32_t indexes = ( block >> 32 ) & 0xFFFFFFFF;

    const auto r0444 = ( block >> 27 ) & 0xF;
    const auto g0444 = ( ( block >> 20 ) & 0x1 ) | ( ( ( block >> 24 ) & 0x7 ) << 1 );
    const auto b0444 = ( ( block >> 15 ) & 0x7 ) | ( ( ( block >> 19 ) & 0x1 ) << 3 );

    const auto r1444 = ( block >> 11 ) & 0xF;
    const auto g1444 = ( block >> 7 ) & 0xF;
    const auto b1444 = ( block >> 3 ) & 0xF;

    const auto r0 = ( r0444 << 4 ) | r0444;
    const auto g0 = ( g0444 << 4 ) | g0444;
    const auto b0 = ( b0444 << 4 ) | b0444;

    const auto r1 = ( r1444 << 4 ) | r1444;
    const auto g1 = ( g1444 << 4 ) | g1444;
    const auto b1 = ( b1444 << 4 ) | b1444;

    const auto codeword_hi = ( ( block & 0x1 ) << 1 ) | ( ( block & 0x4 ) );
    const auto c0 = ( r0444 << 8 ) | ( g0444 << 4 ) | ( b0444 << 0 );
    const auto c1 = ( block >> 3 ) & ( ( 1 << 12 ) - 1 );
    const auto codeword_lo = ( c0 >= c1 ) ? 1 : 0;
    const auto codeword = codeword_hi | codeword_lo;

    const uint32_t col_tab[] = {
        uint32_t( clampu8( r0 + table59T58H[codeword] ) | ( clampu8( g0 + table59T58H[codeword] ) << 8 ) | ( clampu8( b0 + table59T58H[codeword] ) << 16 ) ),
        uint32_t( clampu8( r0 - table59T58H[codeword] ) | ( clampu8( g0 - table59T58H[codeword] ) << 8 ) | ( clampu8( b0 - table59T58H[codeword] ) << 16 ) ),
        uint32_t( clampu8( r1 + table59T58H[codeword] ) | ( clampu8( g1 + table59T58H[codeword] ) << 8 ) | ( clampu8( b1 + table59T58H[codeword] ) << 16 ) ),
        uint32_t( clampu8( r1 - table59T58H[codeword] ) | ( clampu8( g1 - table59T58H[codeword] ) << 8 ) | ( clampu8( b1 - table59T58H[codeword] ) << 16 ) )
    };

    for( uint8_t j = 0; j < 4; j++ )
    {
        for( uint8_t i = 0; i < 4; i++ )
        {
            const uint8_t index = ( ( ( indexes >> ( j + i * 4 + 16 ) ) & 0x1 ) << 1 ) | ( ( indexes >> ( j + i * 4 ) ) & 0x1 );
            dst[j * w + i] = col_tab[index] | 0xFF000000;
        }
    }
}

static etcpak_force_inline void DecodeHAlpha( uint64_t block, uint64_t alpha, uint32_t* dst, uint32_t w )
{
    const uint32_t indexes = ( block >> 32 ) & 0xFFFFFFFF;

    const auto r0444 = ( block >> 27 ) & 0xF;
    const auto g0444 = ( ( block >> 20 ) & 0x1 ) | ( ( ( block >> 24 ) & 0x7 ) << 1 );
    const auto b0444 = ( ( block >> 15 ) & 0x7 ) | ( ( ( block >> 19 ) & 0x1 ) << 3 );

    const auto r1444 = ( block >> 11 ) & 0xF;
    const auto g1444 = ( block >> 7 ) & 0xF;
    const auto b1444 = ( block >> 3 ) & 0xF;

    const auto r0 = ( r0444 << 4 ) | r0444;
    const auto g0 = ( g0444 << 4 ) | g0444;
    const auto b0 = ( b0444 << 4 ) | b0444;

    const auto r1 = ( r1444 << 4 ) | r1444;
    const auto g1 = ( g1444 << 4 ) | g1444;
    const auto b1 = ( b1444 << 4 ) | b1444;

    const auto codeword_hi = ( ( block & 0x1 ) << 1 ) | ( ( block & 0x4 ) );
    const auto c0 = ( r0444 << 8 ) | ( g0444 << 4 ) | ( b0444 << 0 );
    const auto c1 = ( block >> 3 ) & ( ( 1 << 12 ) - 1 );
    const auto codeword_lo = ( c0 >= c1 ) ? 1 : 0;
    const auto codeword = codeword_hi | codeword_lo;

    const int32_t base = alpha >> 56;
    const int32_t mul = ( alpha >> 52 ) & 0xF;
    const auto tbl = g_alpha[(alpha >> 48) & 0xF];

    const uint32_t col_tab[] = {
        uint32_t( clampu8( r0 + table59T58H[codeword] ) | ( clampu8( g0 + table59T58H[codeword] ) << 8 ) | ( clampu8( b0 + table59T58H[codeword] ) << 16 ) ),
        uint32_t( clampu8( r0 - table59T58H[codeword] ) | ( clampu8( g0 - table59T58H[codeword] ) << 8 ) | ( clampu8( b0 - table59T58H[codeword] ) << 16 ) ),
        uint32_t( clampu8( r1 + table59T58H[codeword] ) | ( clampu8( g1 + table59T58H[codeword] ) << 8 ) | ( clampu8( b1 + table59T58H[codeword] ) << 16 ) ),
        uint32_t( clampu8( r1 - table59T58H[codeword] ) | ( clampu8( g1 - table59T58H[codeword] ) << 8 ) | ( clampu8( b1 - table59T58H[codeword] ) << 16 ) )
    };

    for( uint8_t j = 0; j < 4; j++ )
    {
        for( uint8_t i = 0; i < 4; i++ )
        {
            const uint8_t index = ( ( ( indexes >> ( j + i * 4 + 16 ) ) & 0x1 ) << 1 ) | ( ( indexes >> ( j + i * 4 ) ) & 0x1 );
            const auto amod = tbl[( alpha >> ( 45 - j * 3 - i * 12) ) & 0x7];
            const uint32_t a = clampu8( base + amod * mul );
            dst[j * w + i] = col_tab[index] | ( a << 24 );
        }
    }
}

static etcpak_force_inline void DecodePlanar( uint64_t block, uint32_t* dst, uint32_t w )
{
    const auto bv = expand6((block >> ( 0 + 32)) & 0x3F);
    const auto gv = expand7((block >> ( 6 + 32)) & 0x7F);
    const auto rv = expand6((block >> (13 + 32)) & 0x3F);

    const auto bh = expand6((block >> (19 + 32)) & 0x3F);
    const auto gh = expand7((block >> (25 + 32)) & 0x7F);

    const auto rh0 = (block >> (32 - 32)) & 0x01;
    const auto rh1 = ((block >> (34 - 32)) & 0x1F) << 1;
    const auto rh = expand6(rh0 | rh1);

    const auto bo0 = (block >> (39 - 32)) & 0x07;
    const auto bo1 = ((block >> (43 - 32)) & 0x3) << 3;
    const auto bo2 = ((block >> (48 - 32)) & 0x1) << 5;
    const auto bo = expand6(bo0 | bo1 | bo2);
    const auto go0 = (block >> (49 - 32)) & 0x3F;
    const auto go1 = ((block >> (56 - 32)) & 0x01) << 6;
    const auto go = expand7(go0 | go1);
    const auto ro = expand6((block >> (57 - 32)) & 0x3F);

#ifdef __ARM_NEON
    uint64_t init = uint64_t(uint16_t(rh-ro)) | ( uint64_t(uint16_t(gh-go)) << 16 ) | ( uint64_t(uint16_t(bh-bo)) << 32 );
    int16x8_t chco = vreinterpretq_s16_u64( vdupq_n_u64( init ) );
    init = uint64_t(uint16_t( (rv-ro) - 4 * (rh-ro) )) | ( uint64_t(uint16_t( (gv-go) - 4 * (gh-go) )) << 16 ) | ( uint64_t(uint16_t( (bv-bo) - 4 * (bh-bo) )) << 32 );
    int16x8_t cvco = vreinterpretq_s16_u64( vdupq_n_u64( init ) );
    init = uint64_t(4*ro+2) | ( uint64_t(4*go+2) << 16 ) | ( uint64_t(4*bo+2) << 32 ) | ( uint64_t(0xFFF) << 48 );
    int16x8_t col = vreinterpretq_s16_u64( vdupq_n_u64( init ) );

    for( int j=0; j<4; j++ )
    {
        for( int i=0; i<4; i++ )
        {
            uint8x8_t c = vqshrun_n_s16( col, 2 );
            vst1_lane_u32( dst+j*w+i, vreinterpret_u32_u8( c ), 0 );
            col = vaddq_s16( col, chco );
        }
        col = vaddq_s16( col, cvco );
    }
#elif defined __AVX2__
    const auto R0 = 4*ro+2;
    const auto G0 = 4*go+2;
    const auto B0 = 4*bo+2;
    const auto RHO = rh-ro;
    const auto GHO = gh-go;
    const auto BHO = bh-bo;

    __m256i cvco = _mm256_setr_epi16( rv - ro, gv - go, bv - bo, 0, rv - ro, gv - go, bv - bo, 0, rv - ro, gv - go, bv - bo, 0, rv - ro, gv - go, bv - bo, 0 );
    __m256i col = _mm256_setr_epi16( R0, G0, B0, 0xFFF, R0+RHO, G0+GHO, B0+BHO, 0xFFF, R0+2*RHO, G0+2*GHO, B0+2*BHO, 0xFFF, R0+3*RHO, G0+3*GHO, B0+3*BHO, 0xFFF );

    for( int j=0; j<4; j++ )
    {
        __m256i c = _mm256_srai_epi16( col, 2 );
        __m128i s = _mm_packus_epi16( _mm256_castsi256_si128( c ), _mm256_extracti128_si256( c, 1 ) );
        _mm_storeu_si128( (__m128i*)(dst+j*w), s );
        col = _mm256_add_epi16( col, cvco );
    }
#elif defined __SSE4_1__
    __m128i chco = _mm_setr_epi16( rh - ro, gh - go, bh - bo, 0, 0, 0, 0, 0 );
    __m128i cvco = _mm_setr_epi16( (rv - ro) - 4 * (rh - ro), (gv - go) - 4 * (gh - go), (bv - bo) - 4 * (bh - bo), 0, 0, 0, 0, 0 );
    __m128i col = _mm_setr_epi16( 4*ro+2, 4*go+2, 4*bo+2, 0xFFF, 0, 0, 0, 0 );

    for( int j=0; j<4; j++ )
    {
        for( int i=0; i<4; i++ )
        {
            __m128i c = _mm_srai_epi16( col, 2 );
            __m128i s = _mm_packus_epi16( c, c );
            dst[j*w+i] = _mm_cvtsi128_si32( s );
            col = _mm_add_epi16( col, chco );
        }
        col = _mm_add_epi16( col, cvco );
    }
#else
    for( int j=0; j<4; j++ )
    {
        for( int i=0; i<4; i++ )
        {
            const uint32_t r = (i * (rh - ro) + j * (rv - ro) + 4 * ro + 2) >> 2;
            const uint32_t g = (i * (gh - go) + j * (gv - go) + 4 * go + 2) >> 2;
            const uint32_t b = (i * (bh - bo) + j * (bv - bo) + 4 * bo + 2) >> 2;
            if( ( ( r | g | b ) & ~0xFF ) == 0 )
            {
                dst[j*w+i] = r | ( g << 8 ) | ( b << 16 ) | 0xFF000000;
            }
            else
            {
                const auto rc = clampu8( r );
                const auto gc = clampu8( g );
                const auto bc = clampu8( b );
                dst[j*w+i] = rc | ( gc << 8 ) | ( bc << 16 ) | 0xFF000000;
            }
        }
    }
#endif
}

static etcpak_force_inline void DecodePlanarAlpha( uint64_t block, uint64_t alpha, uint32_t* dst, uint32_t w )
{
    const auto bv = expand6((block >> ( 0 + 32)) & 0x3F);
    const auto gv = expand7((block >> ( 6 + 32)) & 0x7F);
    const auto rv = expand6((block >> (13 + 32)) & 0x3F);

    const auto bh = expand6((block >> (19 + 32)) & 0x3F);
    const auto gh = expand7((block >> (25 + 32)) & 0x7F);

    const auto rh0 = (block >> (32 - 32)) & 0x01;
    const auto rh1 = ((block >> (34 - 32)) & 0x1F) << 1;
    const auto rh = expand6(rh0 | rh1);

    const auto bo0 = (block >> (39 - 32)) & 0x07;
    const auto bo1 = ((block >> (43 - 32)) & 0x3) << 3;
    const auto bo2 = ((block >> (48 - 32)) & 0x1) << 5;
    const auto bo = expand6(bo0 | bo1 | bo2);
    const auto go0 = (block >> (49 - 32)) & 0x3F;
    const auto go1 = ((block >> (56 - 32)) & 0x01) << 6;
    const auto go = expand7(go0 | go1);
    const auto ro = expand6((block >> (57 - 32)) & 0x3F);

    const int32_t base = alpha >> 56;
    const int32_t mul = ( alpha >> 52 ) & 0xF;
    const auto tbl = g_alpha[( alpha >> 48 ) & 0xF];

#ifdef __ARM_NEON
    uint64_t init = uint64_t(uint16_t(rh-ro)) | ( uint64_t(uint16_t(gh-go)) << 16 ) | ( uint64_t(uint16_t(bh-bo)) << 32 );
    int16x8_t chco = vreinterpretq_s16_u64( vdupq_n_u64( init ) );
    init = uint64_t(uint16_t( (rv-ro) - 4 * (rh-ro) )) | ( uint64_t(uint16_t( (gv-go) - 4 * (gh-go) )) << 16 ) | ( uint64_t(uint16_t( (bv-bo) - 4 * (bh-bo) )) << 32 );
    int16x8_t cvco = vreinterpretq_s16_u64( vdupq_n_u64( init ) );
    init = uint64_t(4*ro+2) | ( uint64_t(4*go+2) << 16 ) | ( uint64_t(4*bo+2) << 32 );
    int16x8_t col = vreinterpretq_s16_u64( vdupq_n_u64( init ) );

    for( int j=0; j<4; j++ )
    {
        for( int i=0; i<4; i++ )
        {
            const auto amod = tbl[(alpha >> ( 45 - j*3 - i*12 )) & 0x7];
            const uint32_t a = clampu8( base + amod * mul );
            uint8x8_t c = vqshrun_n_s16( col, 2 );
            dst[j*w+i] = vget_lane_u32( vreinterpret_u32_u8( c ), 0 ) | ( a << 24 );
            col = vaddq_s16( col, chco );
        }
        col = vaddq_s16( col, cvco );
    }
#elif defined __SSE4_1__
    __m128i chco = _mm_setr_epi16( rh - ro, gh - go, bh - bo, 0, 0, 0, 0, 0 );
    __m128i cvco = _mm_setr_epi16( (rv - ro) - 4 * (rh - ro), (gv - go) - 4 * (gh - go), (bv - bo) - 4 * (bh - bo), 0, 0, 0, 0, 0 );
    __m128i col = _mm_setr_epi16( 4*ro+2, 4*go+2, 4*bo+2, 0, 0, 0, 0, 0 );

    for( int j=0; j<4; j++ )
    {
        for( int i=0; i<4; i++ )
        {
            const auto amod = tbl[(alpha >> ( 45 - j*3 - i*12 )) & 0x7];
            const uint32_t a = clampu8( base + amod * mul );
            __m128i c = _mm_srai_epi16( col, 2 );
            __m128i s = _mm_packus_epi16( c, c );
            dst[j*w+i] = _mm_cvtsi128_si32( s ) | ( a << 24 );
            col = _mm_add_epi16( col, chco );
        }
        col = _mm_add_epi16( col, cvco );
    }
#else
    for (auto j = 0; j < 4; j++)
    {
        for (auto i = 0; i < 4; i++)
        {
            const uint32_t r = (i * (rh - ro) + j * (rv - ro) + 4 * ro + 2) >> 2;
            const uint32_t g = (i * (gh - go) + j * (gv - go) + 4 * go + 2) >> 2;
            const uint32_t b = (i * (bh - bo) + j * (bv - bo) + 4 * bo + 2) >> 2;
            const auto amod = tbl[(alpha >> ( 45 - j*3 - i*12 )) & 0x7];
            const uint32_t a = clampu8( base + amod * mul );
            if( ( ( r | g | b ) & ~0xFF ) == 0 )
            {
                dst[j*w+i] = r | ( g << 8 ) | ( b << 16 ) | ( a << 24 );
            }
            else
            {
                const auto rc = clampu8( r );
                const auto gc = clampu8( g );
                const auto bc = clampu8( b );
                dst[j*w+i] = rc | ( gc << 8 ) | ( bc << 16 ) | ( a << 24 );
            }
        }
    }
#endif
}
}

static etcpak_force_inline uint64_t ConvertByteOrder( uint64_t d )
{
    uint32_t word[2];
    memcpy( word, &d, 8 );
    word[0] = _bswap( word[0] );
    word[1] = _bswap( word[1] );
    memcpy( &d, word, 8 );
    return d;
}

static etcpak_force_inline void DecodeRGBPart( uint64_t d, uint32_t* dst, uint32_t w )
{
    d = ConvertByteOrder( d );

    uint32_t br[2], bg[2], bb[2];

    if( d & 0x2 )
    {
        int32_t dr, dg, db;

        uint32_t r0 = ( d & 0xF8000000 ) >> 27;
        uint32_t g0 = ( d & 0x00F80000 ) >> 19;
        uint32_t b0 = ( d & 0x0000F800 ) >> 11;

        dr = ( int32_t(d) << 5 ) >> 29;
        dg = ( int32_t(d) << 13 ) >> 29;
        db = ( int32_t(d) << 21 ) >> 29;

        int32_t r1 = int32_t(r0) + dr;
        int32_t g1 = int32_t(g0) + dg;
        int32_t b1 = int32_t(b0) + db;

        // T mode
        if ( (r1 < 0) || (r1 > 31) )
        {
            DecodeT( d, dst, w );
            return;
        }

        // H mode
        if ((g1 < 0) || (g1 > 31))
        {
            DecodeH( d, dst, w );
            return;
        }

        // P mode
        if( (b1 < 0) || (b1 > 31) )
        {
            DecodePlanar( d, dst, w );
            return;
        }

        br[0] = ( r0 << 3 ) | ( r0 >> 2 );
        br[1] = ( r1 << 3 ) | ( r1 >> 2 );
        bg[0] = ( g0 << 3 ) | ( g0 >> 2 );
        bg[1] = ( g1 << 3 ) | ( g1 >> 2 );
        bb[0] = ( b0 << 3 ) | ( b0 >> 2 );
        bb[1] = ( b1 << 3 ) | ( b1 >> 2 );
    }
    else
    {
        br[0] = ( ( d & 0xF0000000 ) >> 24 ) | ( ( d & 0xF0000000 ) >> 28 );
        br[1] = ( ( d & 0x0F000000 ) >> 20 ) | ( ( d & 0x0F000000 ) >> 24 );
        bg[0] = ( ( d & 0x00F00000 ) >> 16 ) | ( ( d & 0x00F00000 ) >> 20 );
        bg[1] = ( ( d & 0x000F0000 ) >> 12 ) | ( ( d & 0x000F0000 ) >> 16 );
        bb[0] = ( ( d & 0x0000F000 ) >> 8  ) | ( ( d & 0x0000F000 ) >> 12 );
        bb[1] = ( ( d & 0x00000F00 ) >> 4  ) | ( ( d & 0x00000F00 ) >> 8  );
    }

    unsigned int tcw[2];
    tcw[0] = ( d & 0xE0 ) >> 5;
    tcw[1] = ( d & 0x1C ) >> 2;

    uint32_t b1 = ( d >> 32 ) & 0xFFFF;
    uint32_t b2 = ( d >> 48 );

    b1 = ( b1 | ( b1 << 8 ) ) & 0x00FF00FF;
    b1 = ( b1 | ( b1 << 4 ) ) & 0x0F0F0F0F;
    b1 = ( b1 | ( b1 << 2 ) ) & 0x33333333;
    b1 = ( b1 | ( b1 << 1 ) ) & 0x55555555;

    b2 = ( b2 | ( b2 << 8 ) ) & 0x00FF00FF;
    b2 = ( b2 | ( b2 << 4 ) ) & 0x0F0F0F0F;
    b2 = ( b2 | ( b2 << 2 ) ) & 0x33333333;
    b2 = ( b2 | ( b2 << 1 ) ) & 0x55555555;

    uint32_t idx = b1 | ( b2 << 1 );

    if( d & 0x1 )
    {
        for( int i=0; i<4; i++ )
        {
            for( int j=0; j<4; j++ )
            {
                const auto mod = g_table[tcw[j/2]][idx & 0x3];
                const auto r = br[j/2] + mod;
                const auto g = bg[j/2] + mod;
                const auto b = bb[j/2] + mod;
                if( ( ( r | g | b ) & ~0xFF ) == 0 )
                {
                    dst[j*w+i] = r | ( g << 8 ) | ( b << 16 ) | 0xFF000000;
                }
                else
                {
                    const auto rc = clampu8( r );
                    const auto gc = clampu8( g );
                    const auto bc = clampu8( b );
                    dst[j*w+i] = rc | ( gc << 8 ) | ( bc << 16 ) | 0xFF000000;
                }
                idx >>= 2;
            }
        }
    }
    else
    {
        for( int i=0; i<4; i++ )
        {
            const auto tbl = g_table[tcw[i/2]];
            const auto cr = br[i/2];
            const auto cg = bg[i/2];
            const auto cb = bb[i/2];

            for( int j=0; j<4; j++ )
            {
                const auto mod = tbl[idx & 0x3];
                const auto r = cr + mod;
                const auto g = cg + mod;
                const auto b = cb + mod;
                if( ( ( r | g | b ) & ~0xFF ) == 0 )
                {
                    dst[j*w+i] = r | ( g << 8 ) | ( b << 16 ) | 0xFF000000;
                }
                else
                {
                    const auto rc = clampu8( r );
                    const auto gc = clampu8( g );
                    const auto bc = clampu8( b );
                    dst[j*w+i] = rc | ( gc << 8 ) | ( bc << 16 ) | 0xFF000000;
                }
                idx >>= 2;
            }
        }
    }
}

static etcpak_force_inline void DecodeRGBAPart( uint64_t d, uint64_t alpha, uint32_t* dst, uint32_t w )
{
    d = ConvertByteOrder( d );
    alpha = _bswap64( alpha );

    uint32_t br[2], bg[2], bb[2];

    if( d & 0x2 )
    {
        int32_t dr, dg, db;

        uint32_t r0 = ( d & 0xF8000000 ) >> 27;
        uint32_t g0 = ( d & 0x00F80000 ) >> 19;
        uint32_t b0 = ( d & 0x0000F800 ) >> 11;

        dr = ( int32_t(d) << 5 ) >> 29;
        dg = ( int32_t(d) << 13 ) >> 29;
        db = ( int32_t(d) << 21 ) >> 29;

        int32_t r1 = int32_t(r0) + dr;
        int32_t g1 = int32_t(g0) + dg;
        int32_t b1 = int32_t(b0) + db;

        // T mode
        if ( (r1 < 0) || (r1 > 31) )
        {
            DecodeTAlpha( d, alpha, dst, w );
            return;
        }

        // H mode
        if ( (g1 < 0) || (g1 > 31) )
        {
            DecodeHAlpha( d, alpha, dst, w );
            return;
        }

        // P mode
        if ( (b1 < 0) || (b1 > 31) )
        {
            DecodePlanarAlpha( d, alpha, dst, w );
            return;
        }

        br[0] = ( r0 << 3 ) | ( r0 >> 2 );
        br[1] = ( r1 << 3 ) | ( r1 >> 2 );
        bg[0] = ( g0 << 3 ) | ( g0 >> 2 );
        bg[1] = ( g1 << 3 ) | ( g1 >> 2 );
        bb[0] = ( b0 << 3 ) | ( b0 >> 2 );
        bb[1] = ( b1 << 3 ) | ( b1 >> 2 );
    }
    else
    {
        br[0] = ( ( d & 0xF0000000 ) >> 24 ) | ( ( d & 0xF0000000 ) >> 28 );
        br[1] = ( ( d & 0x0F000000 ) >> 20 ) | ( ( d & 0x0F000000 ) >> 24 );
        bg[0] = ( ( d & 0x00F00000 ) >> 16 ) | ( ( d & 0x00F00000 ) >> 20 );
        bg[1] = ( ( d & 0x000F0000 ) >> 12 ) | ( ( d & 0x000F0000 ) >> 16 );
        bb[0] = ( ( d & 0x0000F000 ) >> 8  ) | ( ( d & 0x0000F000 ) >> 12 );
        bb[1] = ( ( d & 0x00000F00 ) >> 4  ) | ( ( d & 0x00000F00 ) >> 8  );
    }

    unsigned int tcw[2];
    tcw[0] = ( d & 0xE0 ) >> 5;
    tcw[1] = ( d & 0x1C ) >> 2;

    uint32_t b1 = ( d >> 32 ) & 0xFFFF;
    uint32_t b2 = ( d >> 48 );

    b1 = ( b1 | ( b1 << 8 ) ) & 0x00FF00FF;
    b1 = ( b1 | ( b1 << 4 ) ) & 0x0F0F0F0F;
    b1 = ( b1 | ( b1 << 2 ) ) & 0x33333333;
    b1 = ( b1 | ( b1 << 1 ) ) & 0x55555555;

    b2 = ( b2 | ( b2 << 8 ) ) & 0x00FF00FF;
    b2 = ( b2 | ( b2 << 4 ) ) & 0x0F0F0F0F;
    b2 = ( b2 | ( b2 << 2 ) ) & 0x33333333;
    b2 = ( b2 | ( b2 << 1 ) ) & 0x55555555;

    uint32_t idx = b1 | ( b2 << 1 );

    const int32_t base = alpha >> 56;
    const int32_t mul = ( alpha >> 52 ) & 0xF;
    const auto atbl = g_alpha[( alpha >> 48 ) & 0xF];

    if( d & 0x1 )
    {
        for( int i=0; i<4; i++ )
        {
            for( int j=0; j<4; j++ )
            {
                const auto mod = g_table[tcw[j/2]][idx & 0x3];
                const auto r = br[j/2] + mod;
                const auto g = bg[j/2] + mod;
                const auto b = bb[j/2] + mod;
                const auto amod = atbl[(alpha >> ( 45 - j*3 - i*12 )) & 0x7];
                const uint32_t a = clampu8( base + amod * mul );
                if( ( ( r | g | b ) & ~0xFF ) == 0 )
                {
                    dst[j*w+i] = r | ( g << 8 ) | ( b << 16 ) | ( a << 24 );
                }
                else
                {
                    const auto rc = clampu8( r );
                    const auto gc = clampu8( g );
                    const auto bc = clampu8( b );
                    dst[j*w+i] = rc | ( gc << 8 ) | ( bc << 16 ) | ( a << 24 );
                }
                idx >>= 2;
            }
        }
    }
    else
    {
        for( int i=0; i<4; i++ )
        {
            const auto tbl = g_table[tcw[i/2]];
            const auto cr = br[i/2];
            const auto cg = bg[i/2];
            const auto cb = bb[i/2];

            for( int j=0; j<4; j++ )
            {
                const auto mod = tbl[idx & 0x3];
                const auto r = cr + mod;
                const auto g = cg + mod;
                const auto b = cb + mod;
                const auto amod = atbl[(alpha >> ( 45 - j*3 - i*12 )) & 0x7];
                const uint32_t a = clampu8( base + amod * mul );
                if( ( ( r | g | b ) & ~0xFF ) == 0 )
                {
                    dst[j*w+i] = r | ( g << 8 ) | ( b << 16 ) | ( a << 24 );
                }
                else
                {
                    const auto rc = clampu8( r );
                    const auto gc = clampu8( g );
                    const auto bc = clampu8( b );
                    dst[j*w+i] = rc | ( gc << 8 ) | ( bc << 16 ) | ( a << 24 );
                }
                idx >>= 2;
            }
        }
    }
}

static etcpak_force_inline void DecodeRPart( uint64_t r, uint32_t* dst, uint32_t w )
{
    r = _bswap64( r );

    const int32_t base = ( r >> 56 )*8+4;
    const int32_t mul = ( r >> 52 ) & 0xF;
    const auto atbl = g_alpha[( r >> 48 ) & 0xF];

    for( int i=0; i<4; i++ )
    {
        for ( int j=0; j<4; j++ )
        {
            const auto amod = atbl[(r >> ( 45 - j*3 - i*12 )) & 0x7];
            const uint32_t rc = clampu8( ( base + amod * g_alpha11Mul[mul] )/8 );
            dst[j*w+i] = rc | 0xFF000000;
        }
    }
}

static etcpak_force_inline void DecodeRGPart( uint64_t r, uint64_t g, uint32_t* dst, uint32_t w )
{
    r = _bswap64( r );
    g = _bswap64( g );

    const int32_t rbase = ( r >> 56 )*8+4;
    const int32_t rmul = ( r >> 52 ) & 0xF;
    const auto rtbl = g_alpha[( r >> 48 ) & 0xF];

    const int32_t gbase = ( g >> 56 )*8+4;
    const int32_t gmul = ( g >> 52 ) & 0xF;
    const auto gtbl = g_alpha[( g >> 48 ) & 0xF];

    for( int i=0; i<4; i++ )
    {
        for( int j=0; j<4; j++ )
        {
            const auto rmod = rtbl[(r >> ( 45 - j*3 - i*12 )) & 0x7];
            const uint32_t rc = clampu8( ( rbase + rmod * g_alpha11Mul[rmul] )/8 );

            const auto gmod = gtbl[(g >> ( 45 - j*3 - i*12 )) & 0x7];
            const uint32_t gc = clampu8( ( gbase + gmod * g_alpha11Mul[gmul] )/8 );

            dst[j*w+i] = rc | (gc << 8) | 0xFF000000;
        }
    }
}

void DecodeRBlock( const void* src, void* dst, size_t width )
{
	uint64_t* srcPtr = (uint64_t*)src;
    uint64_t r = *srcPtr++;
    DecodeRPart( r, (uint32_t*)dst, width );
}

void DecodeRGBlock( const void* src, void* dst, size_t width )
{
	uint64_t* srcPtr = (uint64_t*)src;
    uint64_t r = *srcPtr++;
    uint64_t g = *srcPtr++;
    DecodeRGPart( r, g, (uint32_t*)dst, width );
}

void DecodeRGBBlock( const void* src, void* dst, size_t width )
{
	uint64_t* srcPtr = (uint64_t*)src;
    uint64_t d = *srcPtr++;
    DecodeRGBPart( d, (uint32_t*)dst, width );
}

void DecodeRGBABlock( const void* src, void* dst, size_t width )
{
	uint64_t* srcPtr = (uint64_t*)src;
    uint64_t a = *srcPtr++;
    uint64_t d = *srcPtr++;
    DecodeRGBAPart( d, a, (uint32_t*)dst, width );
}

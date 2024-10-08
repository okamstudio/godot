/* bcdec.h - v0.97
   provides functions to decompress blocks of BC compressed images
   written by Sergii "iOrange" Kudlai in 2022

   This library does not allocate memory and is trying to use as less stack as possible

   The library was never optimized specifically for speed but for the overall size
   it has zero external dependencies and is not using any runtime functions

   Supported BC formats:
   BC1 (also known as DXT1) + it's "binary alpha" variant BC1A (DXT1A)
   BC2 (also known as DXT3)
   BC3 (also known as DXT5)
   BC4 (also known as ATI1N)
   BC5 (also known as ATI2N)
   BC6H (HDR format)
   BC7

   BC1/BC2/BC3/BC7 are expected to decompress into 4*4 RGBA blocks 8bit per component (32bit pixel)
   BC4/BC5 are expected to decompress into 4*4 R/RG blocks 8bit per component (8bit and 16bit pixel)
   BC6H is expected to decompress into 4*4 RGB blocks of either 32bit float or 16bit "half" per
   component (96bit or 48bit pixel)

   For more info, issues and suggestions please visit https://github.com/iOrange/bcdec

   CREDITS:
      Aras Pranckevicius (@aras-p)      - BC1/BC3 decoders optimizations (up to 3x the speed)
                                        - BC6H/BC7 bits pulling routines optimizations
                                        - optimized BC6H by moving unquantize out of the loop
                                        - Split BC6H decompression function into 'half' and
                                          'float' variants

      Michael Schmidt (@RunDevelopment) - Found better "magic" coefficients for integer interpolation
                                          of reference colors in BC1 color block, that match with
                                          the floating point interpolation. This also made it faster
                                          than integer division by 3!

   bugfixes:
      @linkmauve

   LICENSE: See end of file for license information.
*/

#ifndef BCDEC_HEADER_INCLUDED
#define BCDEC_HEADER_INCLUDED

#define BCDEC_VERSION_MAJOR 0
#define BCDEC_VERSION_MINOR 97

/* if BCDEC_STATIC causes problems, try defining BCDECDEF to 'inline' or 'static inline' */
#ifndef BCDECDEF
#ifdef BCDEC_STATIC
#define BCDECDEF    static
#else
#ifdef __cplusplus
#define BCDECDEF    extern "C"
#else
#define BCDECDEF    extern
#endif
#endif
#endif

/*  Used information sources:
    https://docs.microsoft.com/en-us/windows/win32/direct3d10/d3d10-graphics-programming-guide-resources-block-compression
    https://docs.microsoft.com/en-us/windows/win32/direct3d11/bc6h-format
    https://docs.microsoft.com/en-us/windows/win32/direct3d11/bc7-format
    https://docs.microsoft.com/en-us/windows/win32/direct3d11/bc7-format-mode-reference

    ! WARNING ! Khronos's BPTC partitions tables contain mistakes, do not use them!
    https://www.khronos.org/registry/DataFormat/specs/1.1/dataformat.1.1.html#BPTC

    ! Use tables from here instead !
    https://www.khronos.org/registry/OpenGL/extensions/ARB/ARB_texture_compression_bptc.txt

    Leaving it here as it's a nice read
    https://fgiesen.wordpress.com/2021/10/04/gpu-bcn-decoding/

    Fast half to float function from here
    https://gist.github.com/rygorous/2144712
*/

#define BCDEC_BC1_BLOCK_SIZE    8
#define BCDEC_BC2_BLOCK_SIZE    16
#define BCDEC_BC3_BLOCK_SIZE    16
#define BCDEC_BC4_BLOCK_SIZE    8
#define BCDEC_BC5_BLOCK_SIZE    16
#define BCDEC_BC6H_BLOCK_SIZE   16
#define BCDEC_BC7_BLOCK_SIZE    16

#define BCDEC_BC1_COMPRESSED_SIZE(w, h)     ((((w)>>2)*((h)>>2))*BCDEC_BC1_BLOCK_SIZE)
#define BCDEC_BC2_COMPRESSED_SIZE(w, h)     ((((w)>>2)*((h)>>2))*BCDEC_BC2_BLOCK_SIZE)
#define BCDEC_BC3_COMPRESSED_SIZE(w, h)     ((((w)>>2)*((h)>>2))*BCDEC_BC3_BLOCK_SIZE)
#define BCDEC_BC4_COMPRESSED_SIZE(w, h)     ((((w)>>2)*((h)>>2))*BCDEC_BC4_BLOCK_SIZE)
#define BCDEC_BC5_COMPRESSED_SIZE(w, h)     ((((w)>>2)*((h)>>2))*BCDEC_BC5_BLOCK_SIZE)
#define BCDEC_BC6H_COMPRESSED_SIZE(w, h)    ((((w)>>2)*((h)>>2))*BCDEC_BC6H_BLOCK_SIZE)
#define BCDEC_BC7_COMPRESSED_SIZE(w, h)     ((((w)>>2)*((h)>>2))*BCDEC_BC7_BLOCK_SIZE)

BCDECDEF void bcdec_bc1(const void* compressedBlock, void* decompressedBlock, int destinationPitch);
BCDECDEF void bcdec_bc2(const void* compressedBlock, void* decompressedBlock, int destinationPitch);
BCDECDEF void bcdec_bc3(const void* compressedBlock, void* decompressedBlock, int destinationPitch);
BCDECDEF void bcdec_bc4(const void* compressedBlock, void* decompressedBlock, int destinationPitch);
BCDECDEF void bcdec_bc5(const void* compressedBlock, void* decompressedBlock, int destinationPitch);
BCDECDEF void bcdec_bc6h_float(const void* compressedBlock, void* decompressedBlock, int destinationPitch, int isSigned);
BCDECDEF void bcdec_bc6h_half(const void* compressedBlock, void* decompressedBlock, int destinationPitch, int isSigned);
BCDECDEF void bcdec_bc7(const void* compressedBlock, void* decompressedBlock, int destinationPitch);

#endif /* BCDEC_HEADER_INCLUDED */

#ifdef BCDEC_IMPLEMENTATION

static void bcdec__color_block(const void* compressedBlock, void* decompressedBlock, int destinationPitch, int onlyOpaqueMode) {
    unsigned short c0, c1;
    unsigned int refColors[4]; /* 0xAABBGGRR */
    unsigned char* dstColors;
    unsigned int colorIndices;
    int i, j, idx;
    unsigned int r0, g0, b0, r1, g1, b1, r, g, b;

    c0 = ((unsigned short*)compressedBlock)[0];
    c1 = ((unsigned short*)compressedBlock)[1];

    /* Unpack 565 ref colors */
    r0 = (c0 >> 11) & 0x1F;
    g0 = (c0 >> 5)  & 0x3F;
    b0 =  c0        & 0x1F;

    r1 = (c1 >> 11) & 0x1F;
    g1 = (c1 >> 5)  & 0x3F;
    b1 =  c1        & 0x1F;

    /* Expand 565 ref colors to 888 */
    r = (r0 * 527 + 23) >> 6;
    g = (g0 * 259 + 33) >> 6;
    b = (b0 * 527 + 23) >> 6;
    refColors[0] = 0xFF000000 | (b << 16) | (g << 8) | r;

    r = (r1 * 527 + 23) >> 6;
    g = (g1 * 259 + 33) >> 6;
    b = (b1 * 527 + 23) >> 6;
    refColors[1] = 0xFF000000 | (b << 16) | (g << 8) | r;

    if (c0 > c1 || onlyOpaqueMode) {    /* Standard BC1 mode (also BC3 color block uses ONLY this mode) */
        /* color_2 = 2/3*color_0 + 1/3*color_1
           color_3 = 1/3*color_0 + 2/3*color_1 */
        r = ((2 * r0 + r1) *  351 +   61) >>  7;
        g = ((2 * g0 + g1) * 2763 + 1039) >> 11;
        b = ((2 * b0 + b1) *  351 +   61) >>  7;
        refColors[2] = 0xFF000000 | (b << 16) | (g << 8) | r;

        r = ((r0 + r1 * 2) *  351 +   61) >>  7;
        g = ((g0 + g1 * 2) * 2763 + 1039) >> 11;
        b = ((b0 + b1 * 2) *  351 +   61) >>  7;
        refColors[3] = 0xFF000000 | (b << 16) | (g << 8) | r;
    } else {                            /* Quite rare BC1A mode */
        /* color_2 = 1/2*color_0 + 1/2*color_1;
           color_3 = 0;                         */
        r = ((r0 + r1) * 1053 +  125) >>  8;
        g = ((g0 + g1) * 4145 + 1019) >> 11;
        b = ((b0 + b1) * 1053 +  125) >>  8;
        refColors[2] = 0xFF000000 | (b << 16) | (g << 8) | r;

        refColors[3] = 0x00000000;
    }

    colorIndices = ((unsigned int*)compressedBlock)[1];

    /* Fill out the decompressed color block */
    dstColors = (unsigned char*)decompressedBlock;
    for (i = 0; i < 4; ++i) {
        for (j = 0; j < 4; ++j) {
            idx = colorIndices & 0x03;
            ((unsigned int*)dstColors)[j] = refColors[idx];
            colorIndices >>= 2;
        }

        dstColors += destinationPitch;
    }
}

static void bcdec__sharp_alpha_block(const void* compressedBlock, void* decompressedBlock, int destinationPitch) {
    unsigned short* alpha;
    unsigned char* decompressed;
    int i, j;

    alpha = (unsigned short*)compressedBlock;
    decompressed = (unsigned char*)decompressedBlock;

    for (i = 0; i < 4; ++i) {
        for (j = 0; j < 4; ++j) {
            decompressed[j * 4] = ((alpha[i] >> (4 * j)) & 0x0F) * 17;
        }

        decompressed += destinationPitch;
    }
}

static void bcdec__smooth_alpha_block(const void* compressedBlock, void* decompressedBlock, int destinationPitch, int pixelSize) {
    unsigned char* decompressed;
    unsigned char alpha[8];
    int i, j;
    unsigned long long block, indices;

    block = *(unsigned long long*)compressedBlock;
    decompressed = (unsigned char*)decompressedBlock;

    alpha[0] = block & 0xFF;
    alpha[1] = (block >> 8) & 0xFF;

    if (alpha[0] > alpha[1]) {
        /* 6 interpolated alpha values. */
        alpha[2] = (6 * alpha[0] +     alpha[1] + 1) / 7;   /* 6/7*alpha_0 + 1/7*alpha_1 */
        alpha[3] = (5 * alpha[0] + 2 * alpha[1] + 1) / 7;   /* 5/7*alpha_0 + 2/7*alpha_1 */
        alpha[4] = (4 * alpha[0] + 3 * alpha[1] + 1) / 7;   /* 4/7*alpha_0 + 3/7*alpha_1 */
        alpha[5] = (3 * alpha[0] + 4 * alpha[1] + 1) / 7;   /* 3/7*alpha_0 + 4/7*alpha_1 */
        alpha[6] = (2 * alpha[0] + 5 * alpha[1] + 1) / 7;   /* 2/7*alpha_0 + 5/7*alpha_1 */
        alpha[7] = (    alpha[0] + 6 * alpha[1] + 1) / 7;   /* 1/7*alpha_0 + 6/7*alpha_1 */
    }
    else {
        /* 4 interpolated alpha values. */
        alpha[2] = (4 * alpha[0] +     alpha[1] + 1) / 5;   /* 4/5*alpha_0 + 1/5*alpha_1 */
        alpha[3] = (3 * alpha[0] + 2 * alpha[1] + 1) / 5;   /* 3/5*alpha_0 + 2/5*alpha_1 */
        alpha[4] = (2 * alpha[0] + 3 * alpha[1] + 1) / 5;   /* 2/5*alpha_0 + 3/5*alpha_1 */
        alpha[5] = (    alpha[0] + 4 * alpha[1] + 1) / 5;   /* 1/5*alpha_0 + 4/5*alpha_1 */
        alpha[6] = 0x00;
        alpha[7] = 0xFF;
    }

    indices = block >> 16;
    for (i = 0; i < 4; ++i) {
        for (j = 0; j < 4; ++j) {
            decompressed[j * pixelSize] = alpha[indices & 0x07];
            indices >>= 3;
        }

        decompressed += destinationPitch;
    }
}

typedef struct bcdec__bitstream {
    unsigned long long low;
    unsigned long long high;
} bcdec__bitstream_t;

static int bcdec__bitstream_read_bits(bcdec__bitstream_t* bstream, int numBits) {
    unsigned int mask = (1 << numBits) - 1;
    /* Read the low N bits */
    unsigned int bits = (bstream->low & mask);

    bstream->low >>= numBits;
    /* Put the low N bits of "high" into the high 64-N bits of "low". */
    bstream->low |= (bstream->high & mask) << (sizeof(bstream->high) * 8 - numBits);
    bstream->high >>= numBits;
    
    return bits;
}

static int bcdec__bitstream_read_bit(bcdec__bitstream_t* bstream) {
    return bcdec__bitstream_read_bits(bstream, 1);
}

/*  reversed bits pulling, used in BC6H decoding
    why ?? just why ??? */
static int bcdec__bitstream_read_bits_r(bcdec__bitstream_t* bstream, int numBits) {
    int bits = bcdec__bitstream_read_bits(bstream, numBits);
    /* Reverse the bits. */
    int result = 0;
    while (numBits--) {
        result <<= 1;
        result |= (bits & 1);
        bits >>= 1;
    }
    return result;
}



BCDECDEF void bcdec_bc1(const void* compressedBlock, void* decompressedBlock, int destinationPitch) {
    bcdec__color_block(compressedBlock, decompressedBlock, destinationPitch, 0);
}

BCDECDEF void bcdec_bc2(const void* compressedBlock, void* decompressedBlock, int destinationPitch) {
    bcdec__color_block(((char*)compressedBlock) + 8, decompressedBlock, destinationPitch, 1);
    bcdec__sharp_alpha_block(compressedBlock, ((char*)decompressedBlock) + 3, destinationPitch);
}

BCDECDEF void bcdec_bc3(const void* compressedBlock, void* decompressedBlock, int destinationPitch) {
    bcdec__color_block(((char*)compressedBlock) + 8, decompressedBlock, destinationPitch, 1);
    bcdec__smooth_alpha_block(compressedBlock, ((char*)decompressedBlock) + 3, destinationPitch, 4);
}

BCDECDEF void bcdec_bc4(const void* compressedBlock, void* decompressedBlock, int destinationPitch) {
    bcdec__smooth_alpha_block(compressedBlock, decompressedBlock, destinationPitch, 1);
}

BCDECDEF void bcdec_bc5(const void* compressedBlock, void* decompressedBlock, int destinationPitch) {
    bcdec__smooth_alpha_block(compressedBlock, decompressedBlock, destinationPitch, 2);
    bcdec__smooth_alpha_block(((char*)compressedBlock) + 8, ((char*)decompressedBlock) + 1, destinationPitch, 2);
}

/* http://graphics.stanford.edu/~seander/bithacks.html#VariableSignExtend */
static int bcdec__extend_sign(int val, int bits) {
    return (val << (32 - bits)) >> (32 - bits);
}

static int bcdec__transform_inverse(int val, int a0, int bits, int isSigned) {
    /* If the precision of A0 is "p" bits, then the transform algorithm is:
       B0 = (B0 + A0) & ((1 << p) - 1) */
    val = (val + a0) & ((1 << bits) - 1);
    if (isSigned) {
        val = bcdec__extend_sign(val, bits);
    }
    return val;
}

/* pretty much copy-paste from documentation */
static int bcdec__unquantize(int val, int bits, int isSigned) {
    int unq, s = 0;

    if (!isSigned) {
        if (bits >= 15) {
            unq = val;
        } else if (!val) {
            unq = 0;
        } else if (val == ((1 << bits) - 1)) {
            unq = 0xFFFF;
        } else {
            unq = ((val << 16) + 0x8000) >> bits;
        }
    } else {
        if (bits >= 16) {
            unq = val;
        } else {
            if (val < 0) {
                s = 1;
                val = -val;
            }

            if (val == 0) {
                unq = 0;
            } else if (val >= ((1 << (bits - 1)) - 1)) {
                unq = 0x7FFF;
            } else {
                unq = ((val << 15) + 0x4000) >> (bits - 1);
            }

            if (s) {
                unq = -unq;
            }
        }
    }
    return unq;
}

static int bcdec__interpolate(int a, int b, int* weights, int index) {
    return (a * (64 - weights[index]) + b * weights[index] + 32) >> 6;
}

static unsigned short bcdec__finish_unquantize(int val, int isSigned) {
    int s;

    if (!isSigned) {
        return (unsigned short)((val * 31) >> 6);                   /* scale the magnitude by 31 / 64 */
    } else {
        val = (val < 0) ? -(((-val) * 31) >> 5) : (val * 31) >> 5;  /* scale the magnitude by 31 / 32 */
        s = 0;
        if (val < 0) {
            s = 0x8000;
            val = -val;
        }
        return (unsigned short)(s | val);
    }
}

/* modified half_to_float_fast4 from https://gist.github.com/rygorous/2144712 */
static float bcdec__half_to_float_quick(unsigned short half) {
    typedef union {
        unsigned int u;
        float f;
    } FP32;

    static const FP32 magic = { 113 << 23 };
    static const unsigned int shifted_exp = 0x7c00 << 13;   /* exponent mask after shift */
    FP32 o;
    unsigned int exp;

    o.u = (half & 0x7fff) << 13;                            /* exponent/mantissa bits */
    exp = shifted_exp & o.u;                                /* just the exponent */
    o.u += (127 - 15) << 23;                                /* exponent adjust */

    /* handle exponent special cases */
    if (exp == shifted_exp) {                               /* Inf/NaN? */
        o.u += (128 - 16) << 23;                            /* extra exp adjust */
    } else if (exp == 0) {                                  /* Zero/Denormal? */
        o.u += 1 << 23;                                     /* extra exp adjust */
        o.f -= magic.f;                                     /* renormalize */
    }

    o.u |= (half & 0x8000) << 16;                           /* sign bit */
    return o.f;
}

BCDECDEF void bcdec_bc6h_half(const void* compressedBlock, void* decompressedBlock, int destinationPitch, int isSigned) {
    static char actual_bits_count[4][14] = {
        { 10, 7, 11, 11, 11, 9, 8, 8, 8, 6, 10, 11, 12, 16 },   /*  W */
        {  5, 6,  5,  4,  4, 5, 6, 5, 5, 6, 10,  9,  8,  4 },   /* dR */
        {  5, 6,  4,  5,  4, 5, 5, 6, 5, 6, 10,  9,  8,  4 },   /* dG */
        {  5, 6,  4,  4,  5, 5, 5, 5, 6, 6, 10,  9,  8,  4 }    /* dB */
    };

    /* There are 32 possible partition sets for a two-region tile.
       Each 4x4 block represents a single shape.
       Here also every fix-up index has MSB bit set. */
    static unsigned char partition_sets[32][4][4] = {
        { {128, 0,   1, 1}, {0, 0, 1, 1}, {  0, 0, 1, 1}, {0, 0, 1, 129} },   /*  0 */
        { {128, 0,   0, 1}, {0, 0, 0, 1}, {  0, 0, 0, 1}, {0, 0, 0, 129} },   /*  1 */
        { {128, 1,   1, 1}, {0, 1, 1, 1}, {  0, 1, 1, 1}, {0, 1, 1, 129} },   /*  2 */
        { {128, 0,   0, 1}, {0, 0, 1, 1}, {  0, 0, 1, 1}, {0, 1, 1, 129} },   /*  3 */
        { {128, 0,   0, 0}, {0, 0, 0, 1}, {  0, 0, 0, 1}, {0, 0, 1, 129} },   /*  4 */
        { {128, 0,   1, 1}, {0, 1, 1, 1}, {  0, 1, 1, 1}, {1, 1, 1, 129} },   /*  5 */
        { {128, 0,   0, 1}, {0, 0, 1, 1}, {  0, 1, 1, 1}, {1, 1, 1, 129} },   /*  6 */
        { {128, 0,   0, 0}, {0, 0, 0, 1}, {  0, 0, 1, 1}, {0, 1, 1, 129} },   /*  7 */
        { {128, 0,   0, 0}, {0, 0, 0, 0}, {  0, 0, 0, 1}, {0, 0, 1, 129} },   /*  8 */
        { {128, 0,   1, 1}, {0, 1, 1, 1}, {  1, 1, 1, 1}, {1, 1, 1, 129} },   /*  9 */
        { {128, 0,   0, 0}, {0, 0, 0, 1}, {  0, 1, 1, 1}, {1, 1, 1, 129} },   /* 10 */
        { {128, 0,   0, 0}, {0, 0, 0, 0}, {  0, 0, 0, 1}, {0, 1, 1, 129} },   /* 11 */
        { {128, 0,   0, 1}, {0, 1, 1, 1}, {  1, 1, 1, 1}, {1, 1, 1, 129} },   /* 12 */
        { {128, 0,   0, 0}, {0, 0, 0, 0}, {  1, 1, 1, 1}, {1, 1, 1, 129} },   /* 13 */
        { {128, 0,   0, 0}, {1, 1, 1, 1}, {  1, 1, 1, 1}, {1, 1, 1, 129} },   /* 14 */
        { {128, 0,   0, 0}, {0, 0, 0, 0}, {  0, 0, 0, 0}, {1, 1, 1, 129} },   /* 15 */
        { {128, 0,   0, 0}, {1, 0, 0, 0}, {  1, 1, 1, 0}, {1, 1, 1, 129} },   /* 16 */
        { {128, 1, 129, 1}, {0, 0, 0, 1}, {  0, 0, 0, 0}, {0, 0, 0,   0} },   /* 17 */
        { {128, 0,   0, 0}, {0, 0, 0, 0}, {129, 0, 0, 0}, {1, 1, 1,   0} },   /* 18 */
        { {128, 1, 129, 1}, {0, 0, 1, 1}, {  0, 0, 0, 1}, {0, 0, 0,   0} },   /* 19 */
        { {128, 0, 129, 1}, {0, 0, 0, 1}, {  0, 0, 0, 0}, {0, 0, 0,   0} },   /* 20 */
        { {128, 0,   0, 0}, {1, 0, 0, 0}, {129, 1, 0, 0}, {1, 1, 1,   0} },   /* 21 */
        { {128, 0,   0, 0}, {0, 0, 0, 0}, {129, 0, 0, 0}, {1, 1, 0,   0} },   /* 22 */
        { {128, 1,   1, 1}, {0, 0, 1, 1}, {  0, 0, 1, 1}, {0, 0, 0, 129} },   /* 23 */
        { {128, 0, 129, 1}, {0, 0, 0, 1}, {  0, 0, 0, 1}, {0, 0, 0,   0} },   /* 24 */
        { {128, 0,   0, 0}, {1, 0, 0, 0}, {129, 0, 0, 0}, {1, 1, 0,   0} },   /* 25 */
        { {128, 1, 129, 0}, {0, 1, 1, 0}, {  0, 1, 1, 0}, {0, 1, 1,   0} },   /* 26 */
        { {128, 0, 129, 1}, {0, 1, 1, 0}, {  0, 1, 1, 0}, {1, 1, 0,   0} },   /* 27 */
        { {128, 0,   0, 1}, {0, 1, 1, 1}, {129, 1, 1, 0}, {1, 0, 0,   0} },   /* 28 */
        { {128, 0,   0, 0}, {1, 1, 1, 1}, {129, 1, 1, 1}, {0, 0, 0,   0} },   /* 29 */
        { {128, 1, 129, 1}, {0, 0, 0, 1}, {  1, 0, 0, 0}, {1, 1, 1,   0} },   /* 30 */
        { {128, 0, 129, 1}, {1, 0, 0, 1}, {  1, 0, 0, 1}, {1, 1, 0,   0} }    /* 31 */
    };

    static int aWeight3[8] = { 0, 9, 18, 27, 37, 46, 55, 64 };
    static int aWeight4[16] = { 0, 4, 9, 13, 17, 21, 26, 30, 34, 38, 43, 47, 51, 55, 60, 64 };

    bcdec__bitstream_t bstream;
    int mode, partition, numPartitions, i, j, partitionSet, indexBits, index, ep_i, actualBits0Mode;
    int r[4], g[4], b[4];       /* wxyz */
    unsigned short* decompressed;
    int* weights;

    decompressed = (unsigned short*)decompressedBlock;

    bstream.low = ((unsigned long long*)compressedBlock)[0];
    bstream.high = ((unsigned long long*)compressedBlock)[1];

    r[0] = r[1] = r[2] = r[3] = 0;
    g[0] = g[1] = g[2] = g[3] = 0;
    b[0] = b[1] = b[2] = b[3] = 0;

    mode = bcdec__bitstream_read_bits(&bstream, 2);
    if (mode > 1) {
        mode |= (bcdec__bitstream_read_bits(&bstream, 3) << 2);
    }

    /* modes >= 11 (10 in my code) are using 0 one, others will read it from the bitstream */
    partition = 0;

    switch (mode) {
        /* mode 1 */
        case 0b00: {
            /* Partitition indices: 46 bits
               Partition: 5 bits
               Color Endpoints: 75 bits (10.555, 10.555, 10.555) */
            g[2] |= bcdec__bitstream_read_bit(&bstream) << 4;       /* gy[4]   */
            b[2] |= bcdec__bitstream_read_bit(&bstream) << 4;       /* by[4]   */
            b[3] |= bcdec__bitstream_read_bit(&bstream) << 4;       /* bz[4]   */
            r[0] |= bcdec__bitstream_read_bits(&bstream, 10);       /* rw[9:0] */
            g[0] |= bcdec__bitstream_read_bits(&bstream, 10);       /* gw[9:0] */
            b[0] |= bcdec__bitstream_read_bits(&bstream, 10);       /* bw[9:0] */
            r[1] |= bcdec__bitstream_read_bits(&bstream, 5);        /* rx[4:0] */
            g[3] |= bcdec__bitstream_read_bit(&bstream) << 4;       /* gz[4]   */
            g[2] |= bcdec__bitstream_read_bits(&bstream, 4);        /* gy[3:0] */
            g[1] |= bcdec__bitstream_read_bits(&bstream, 5);        /* gx[4:0] */
            b[3] |= bcdec__bitstream_read_bit(&bstream);            /* bz[0]   */
            g[3] |= bcdec__bitstream_read_bits(&bstream, 4);        /* gz[3:0] */
            b[1] |= bcdec__bitstream_read_bits(&bstream, 5);        /* bx[4:0] */
            b[3] |= bcdec__bitstream_read_bit(&bstream) << 1;       /* bz[1]   */
            b[2] |= bcdec__bitstream_read_bits(&bstream, 4);        /* by[3:0] */
            r[2] |= bcdec__bitstream_read_bits(&bstream, 5);        /* ry[4:0] */
            b[3] |= bcdec__bitstream_read_bit(&bstream) << 2;       /* bz[2]   */
            r[3] |= bcdec__bitstream_read_bits(&bstream, 5);        /* rz[4:0] */
            b[3] |= bcdec__bitstream_read_bit(&bstream) << 3;       /* bz[3]   */
            partition = bcdec__bitstream_read_bits(&bstream, 5);    /* d[4:0]  */
            mode = 0;
        } break;

        /* mode 2 */
        case 0b01: {
            /* Partitition indices: 46 bits
               Partition: 5 bits
               Color Endpoints: 75 bits (7666, 7666, 7666) */
            g[2] |= bcdec__bitstream_read_bit(&bstream) << 5;       /* gy[5]   */
            g[3] |= bcdec__bitstream_read_bit(&bstream) << 4;       /* gz[4]   */
            g[3] |= bcdec__bitstream_read_bit(&bstream) << 5;       /* gz[5]   */
            r[0] |= bcdec__bitstream_read_bits(&bstream, 7);        /* rw[6:0] */
            b[3] |= bcdec__bitstream_read_bit(&bstream);            /* bz[0]   */
            b[3] |= bcdec__bitstream_read_bit(&bstream) << 1;       /* bz[1]   */
            b[2] |= bcdec__bitstream_read_bit(&bstream) << 4;       /* by[4]   */
            g[0] |= bcdec__bitstream_read_bits(&bstream, 7);        /* gw[6:0] */
            b[2] |= bcdec__bitstream_read_bit(&bstream) << 5;       /* by[5]   */
            b[3] |= bcdec__bitstream_read_bit(&bstream) << 2;       /* bz[2]   */
            g[2] |= bcdec__bitstream_read_bit(&bstream) << 4;       /* gy[4]   */
            b[0] |= bcdec__bitstream_read_bits(&bstream, 7);        /* bw[6:0] */
            b[3] |= bcdec__bitstream_read_bit(&bstream) << 3;       /* bz[3]   */
            b[3] |= bcdec__bitstream_read_bit(&bstream) << 5;       /* bz[5]   */
            b[3] |= bcdec__bitstream_read_bit(&bstream) << 4;       /* bz[4]   */
            r[1] |= bcdec__bitstream_read_bits(&bstream, 6);        /* rx[5:0] */
            g[2] |= bcdec__bitstream_read_bits(&bstream, 4);        /* gy[3:0] */
            g[1] |= bcdec__bitstream_read_bits(&bstream, 6);        /* gx[5:0] */
            g[3] |= bcdec__bitstream_read_bits(&bstream, 4);        /* gz[3:0] */
            b[1] |= bcdec__bitstream_read_bits(&bstream, 6);        /* bx[5:0] */
            b[2] |= bcdec__bitstream_read_bits(&bstream, 4);        /* by[3:0] */
            r[2] |= bcdec__bitstream_read_bits(&bstream, 6);        /* ry[5:0] */
            r[3] |= bcdec__bitstream_read_bits(&bstream, 6);        /* rz[5:0] */
            partition = bcdec__bitstream_read_bits(&bstream, 5);    /* d[4:0]  */
            mode = 1;
        } break;

        /* mode 3 */
        case 0b00010: {
            /* Partitition indices: 46 bits
               Partition: 5 bits
               Color Endpoints: 72 bits (11.555, 11.444, 11.444) */
            r[0] |= bcdec__bitstream_read_bits(&bstream, 10);       /* rw[9:0] */
            g[0] |= bcdec__bitstream_read_bits(&bstream, 10);       /* gw[9:0] */
            b[0] |= bcdec__bitstream_read_bits(&bstream, 10);       /* bw[9:0] */
            r[1] |= bcdec__bitstream_read_bits(&bstream, 5);        /* rx[4:0] */
            r[0] |= bcdec__bitstream_read_bit(&bstream) << 10;      /* rw[10]  */
            g[2] |= bcdec__bitstream_read_bits(&bstream, 4);        /* gy[3:0] */
            g[1] |= bcdec__bitstream_read_bits(&bstream, 4);        /* gx[3:0] */
            g[0] |= bcdec__bitstream_read_bit(&bstream) << 10;      /* gw[10]  */
            b[3] |= bcdec__bitstream_read_bit(&bstream);            /* bz[0]   */
            g[3] |= bcdec__bitstream_read_bits(&bstream, 4);        /* gz[3:0] */
            b[1] |= bcdec__bitstream_read_bits(&bstream, 4);        /* bx[3:0] */
            b[0] |= bcdec__bitstream_read_bit(&bstream) << 10;      /* bw[10]  */
            b[3] |= bcdec__bitstream_read_bit(&bstream) << 1;       /* bz[1]   */
            b[2] |= bcdec__bitstream_read_bits(&bstream, 4);        /* by[3:0] */
            r[2] |= bcdec__bitstream_read_bits(&bstream, 5);        /* ry[4:0] */
            b[3] |= bcdec__bitstream_read_bit(&bstream) << 2;       /* bz[2]   */
            r[3] |= bcdec__bitstream_read_bits(&bstream, 5);        /* rz[4:0] */
            b[3] |= bcdec__bitstream_read_bit(&bstream) << 3;       /* bz[3]   */
            partition = bcdec__bitstream_read_bits(&bstream, 5);    /* d[4:0]  */
            mode = 2;
        } break;

        /* mode 4 */
        case 0b00110: {
            /* Partitition indices: 46 bits
               Partition: 5 bits
               Color Endpoints: 72 bits (11.444, 11.555, 11.444) */
            r[0] |= bcdec__bitstream_read_bits(&bstream, 10);       /* rw[9:0] */
            g[0] |= bcdec__bitstream_read_bits(&bstream, 10);       /* gw[9:0] */
            b[0] |= bcdec__bitstream_read_bits(&bstream, 10);       /* bw[9:0] */
            r[1] |= bcdec__bitstream_read_bits(&bstream, 4);        /* rx[3:0] */
            r[0] |= bcdec__bitstream_read_bit(&bstream) << 10;      /* rw[10]  */
            g[3] |= bcdec__bitstream_read_bit(&bstream) << 4;       /* gz[4]   */
            g[2] |= bcdec__bitstream_read_bits(&bstream, 4);        /* gy[3:0] */
            g[1] |= bcdec__bitstream_read_bits(&bstream, 5);        /* gx[4:0] */
            g[0] |= bcdec__bitstream_read_bit(&bstream) << 10;      /* gw[10]  */
            g[3] |= bcdec__bitstream_read_bits(&bstream, 4);        /* gz[3:0] */
            b[1] |= bcdec__bitstream_read_bits(&bstream, 4);        /* bx[3:0] */
            b[0] |= bcdec__bitstream_read_bit(&bstream) << 10;      /* bw[10]  */
            b[3] |= bcdec__bitstream_read_bit(&bstream) << 1;       /* bz[1]   */
            b[2] |= bcdec__bitstream_read_bits(&bstream, 4);        /* by[3:0] */
            r[2] |= bcdec__bitstream_read_bits(&bstream, 4);        /* ry[3:0] */
            b[3] |= bcdec__bitstream_read_bit(&bstream);            /* bz[0]   */
            b[3] |= bcdec__bitstream_read_bit(&bstream) << 2;       /* bz[2]   */
            r[3] |= bcdec__bitstream_read_bits(&bstream, 4);        /* rz[3:0] */
            g[2] |= bcdec__bitstream_read_bit(&bstream) << 4;       /* gy[4]   */
            b[3] |= bcdec__bitstream_read_bit(&bstream) << 3;       /* bz[3]   */
            partition = bcdec__bitstream_read_bits(&bstream, 5);    /* d[4:0]  */
            mode = 3;
        } break;

        /* mode 5 */
        case 0b01010: {
            /* Partitition indices: 46 bits
               Partition: 5 bits
               Color Endpoints: 72 bits (11.444, 11.444, 11.555) */
            r[0] |= bcdec__bitstream_read_bits(&bstream, 10);       /* rw[9:0] */
            g[0] |= bcdec__bitstream_read_bits(&bstream, 10);       /* gw[9:0] */
            b[0] |= bcdec__bitstream_read_bits(&bstream, 10);       /* bw[9:0] */
            r[1] |= bcdec__bitstream_read_bits(&bstream, 4);        /* rx[3:0] */
            r[0] |= bcdec__bitstream_read_bit(&bstream) << 10;      /* rw[10]  */
            b[2] |= bcdec__bitstream_read_bit(&bstream) << 4;       /* by[4]   */
            g[2] |= bcdec__bitstream_read_bits(&bstream, 4);        /* gy[3:0] */
            g[1] |= bcdec__bitstream_read_bits(&bstream, 4);        /* gx[3:0] */
            g[0] |= bcdec__bitstream_read_bit(&bstream) << 10;      /* gw[10]  */
            b[3] |= bcdec__bitstream_read_bit(&bstream);            /* bz[0]   */
            g[3] |= bcdec__bitstream_read_bits(&bstream, 4);        /* gz[3:0] */
            b[1] |= bcdec__bitstream_read_bits(&bstream, 5);        /* bx[4:0] */
            b[0] |= bcdec__bitstream_read_bit(&bstream) << 10;      /* bw[10]  */
            b[2] |= bcdec__bitstream_read_bits(&bstream, 4);        /* by[3:0] */
            r[2] |= bcdec__bitstream_read_bits(&bstream, 4);        /* ry[3:0] */
            b[3] |= bcdec__bitstream_read_bit(&bstream) << 1;       /* bz[1]   */
            b[3] |= bcdec__bitstream_read_bit(&bstream) << 2;       /* bz[2]   */
            r[3] |= bcdec__bitstream_read_bits(&bstream, 4);        /* rz[3:0] */
            b[3] |= bcdec__bitstream_read_bit(&bstream) << 4;       /* bz[4]   */ 
            b[3] |= bcdec__bitstream_read_bit(&bstream) << 3;       /* bz[3]   */
            partition = bcdec__bitstream_read_bits(&bstream, 5);    /* d[4:0]  */
            mode = 4;
        } break;

        /* mode 6 */
        case 0b01110: {
            /* Partitition indices: 46 bits
               Partition: 5 bits
               Color Endpoints: 72 bits (9555, 9555, 9555) */
            r[0] |= bcdec__bitstream_read_bits(&bstream, 9);        /* rw[8:0] */
            b[2] |= bcdec__bitstream_read_bit(&bstream) << 4;       /* by[4]   */
            g[0] |= bcdec__bitstream_read_bits(&bstream, 9);        /* gw[8:0] */
            g[2] |= bcdec__bitstream_read_bit(&bstream) << 4;       /* gy[4]   */
            b[0] |= bcdec__bitstream_read_bits(&bstream, 9);        /* bw[8:0] */
            b[3] |= bcdec__bitstream_read_bit(&bstream) << 4;       /* bz[4]   */
            r[1] |= bcdec__bitstream_read_bits(&bstream, 5);        /* rx[4:0] */
            g[3] |= bcdec__bitstream_read_bit(&bstream) << 4;       /* gz[4]   */
            g[2] |= bcdec__bitstream_read_bits(&bstream, 4);        /* gy[3:0] */
            g[1] |= bcdec__bitstream_read_bits(&bstream, 5);        /* gx[4:0] */
            b[3] |= bcdec__bitstream_read_bit(&bstream);            /* bz[0]   */
            g[3] |= bcdec__bitstream_read_bits(&bstream, 4);        /* gx[3:0] */
            b[1] |= bcdec__bitstream_read_bits(&bstream, 5);        /* bx[4:0] */
            b[3] |= bcdec__bitstream_read_bit(&bstream) << 1;       /* bz[1]   */
            b[2] |= bcdec__bitstream_read_bits(&bstream, 4);        /* by[3:0] */
            r[2] |= bcdec__bitstream_read_bits(&bstream, 5);        /* ry[4:0] */
            b[3] |= bcdec__bitstream_read_bit(&bstream) << 2;       /* bz[2]   */
            r[3] |= bcdec__bitstream_read_bits(&bstream, 5);        /* rz[4:0] */
            b[3] |= bcdec__bitstream_read_bit(&bstream) << 3;       /* bz[3]   */
            partition = bcdec__bitstream_read_bits(&bstream, 5);    /* d[4:0]  */
            mode = 5;
        } break;

        /* mode 7 */
        case 0b10010: {
            /* Partitition indices: 46 bits
               Partition: 5 bits
               Color Endpoints: 72 bits (8666, 8555, 8555) */
            r[0] |= bcdec__bitstream_read_bits(&bstream, 8);        /* rw[7:0] */
            g[3] |= bcdec__bitstream_read_bit(&bstream) << 4;       /* gz[4]   */
            b[2] |= bcdec__bitstream_read_bit(&bstream) << 4;       /* by[4]   */
            g[0] |= bcdec__bitstream_read_bits(&bstream, 8);        /* gw[7:0] */
            b[3] |= bcdec__bitstream_read_bit(&bstream) << 2;       /* bz[2]   */
            g[2] |= bcdec__bitstream_read_bit(&bstream) << 4;       /* gy[4]   */
            b[0] |= bcdec__bitstream_read_bits(&bstream, 8);        /* bw[7:0] */
            b[3] |= bcdec__bitstream_read_bit(&bstream) << 3;       /* bz[3]   */
            b[3] |= bcdec__bitstream_read_bit(&bstream) << 4;       /* bz[4]   */
            r[1] |= bcdec__bitstream_read_bits(&bstream, 6);        /* rx[5:0] */
            g[2] |= bcdec__bitstream_read_bits(&bstream, 4);        /* gy[3:0] */
            g[1] |= bcdec__bitstream_read_bits(&bstream, 5);        /* gx[4:0] */
            b[3] |= bcdec__bitstream_read_bit(&bstream);            /* bz[0]   */
            g[3] |= bcdec__bitstream_read_bits(&bstream, 4);        /* gz[3:0] */
            b[1] |= bcdec__bitstream_read_bits(&bstream, 5);        /* bx[4:0] */
            b[3] |= bcdec__bitstream_read_bit(&bstream) << 1;       /* bz[1]   */
            b[2] |= bcdec__bitstream_read_bits(&bstream, 4);        /* by[3:0] */
            r[2] |= bcdec__bitstream_read_bits(&bstream, 6);        /* ry[5:0] */
            r[3] |= bcdec__bitstream_read_bits(&bstream, 6);        /* rz[5:0] */
            partition = bcdec__bitstream_read_bits(&bstream, 5);    /* d[4:0]  */
            mode = 6;
        } break;

        /* mode 8 */
        case 0b10110: {
            /* Partitition indices: 46 bits
               Partition: 5 bits
               Color Endpoints: 72 bits (8555, 8666, 8555) */
            r[0] |= bcdec__bitstream_read_bits(&bstream, 8);        /* rw[7:0] */
            b[3] |= bcdec__bitstream_read_bit(&bstream);            /* bz[0]   */
            b[2] |= bcdec__bitstream_read_bit(&bstream) << 4;       /* by[4]   */
            g[0] |= bcdec__bitstream_read_bits(&bstream, 8);        /* gw[7:0] */
            g[2] |= bcdec__bitstream_read_bit(&bstream) << 5;       /* gy[5]   */
            g[2] |= bcdec__bitstream_read_bit(&bstream) << 4;       /* gy[4]   */
            b[0] |= bcdec__bitstream_read_bits(&bstream, 8);        /* bw[7:0] */
            g[3] |= bcdec__bitstream_read_bit(&bstream) << 5;       /* gz[5]   */
            b[3] |= bcdec__bitstream_read_bit(&bstream) << 4;       /* bz[4]   */
            r[1] |= bcdec__bitstream_read_bits(&bstream, 5);        /* rx[4:0] */
            g[3] |= bcdec__bitstream_read_bit(&bstream) << 4;       /* gz[4]   */
            g[2] |= bcdec__bitstream_read_bits(&bstream, 4);        /* gy[3:0] */
            g[1] |= bcdec__bitstream_read_bits(&bstream, 6);        /* gx[5:0] */
            g[3] |= bcdec__bitstream_read_bits(&bstream, 4);        /* zx[3:0] */
            b[1] |= bcdec__bitstream_read_bits(&bstream, 5);        /* bx[4:0] */
            b[3] |= bcdec__bitstream_read_bit(&bstream) << 1;       /* bz[1]   */
            b[2] |= bcdec__bitstream_read_bits(&bstream, 4);        /* by[3:0] */
            r[2] |= bcdec__bitstream_read_bits(&bstream, 5);        /* ry[4:0] */
            b[3] |= bcdec__bitstream_read_bit(&bstream) << 2;       /* bz[2]   */
            r[3] |= bcdec__bitstream_read_bits(&bstream, 5);        /* rz[4:0] */
            b[3] |= bcdec__bitstream_read_bit(&bstream) << 3;       /* bz[3]   */
            partition = bcdec__bitstream_read_bits(&bstream, 5);    /* d[4:0]  */
            mode = 7;
        } break;

        /* mode 9 */
        case 0b11010: {
            /* Partitition indices: 46 bits
               Partition: 5 bits
               Color Endpoints: 72 bits (8555, 8555, 8666) */
            r[0] |= bcdec__bitstream_read_bits(&bstream, 8);        /* rw[7:0] */
            b[3] |= bcdec__bitstream_read_bit(&bstream) << 1;       /* bz[1]   */
            b[2] |= bcdec__bitstream_read_bit(&bstream) << 4;       /* by[4]   */
            g[0] |= bcdec__bitstream_read_bits(&bstream, 8);        /* gw[7:0] */
            b[2] |= bcdec__bitstream_read_bit(&bstream) << 5;       /* by[5]   */
            g[2] |= bcdec__bitstream_read_bit(&bstream) << 4;       /* gy[4]   */
            b[0] |= bcdec__bitstream_read_bits(&bstream, 8);        /* bw[7:0] */
            b[3] |= bcdec__bitstream_read_bit(&bstream) << 5;       /* bz[5]   */
            b[3] |= bcdec__bitstream_read_bit(&bstream) << 4;       /* bz[4]   */
            r[1] |= bcdec__bitstream_read_bits(&bstream, 5);        /* bw[4:0] */
            g[3] |= bcdec__bitstream_read_bit(&bstream) << 4;       /* gz[4]   */
            g[2] |= bcdec__bitstream_read_bits(&bstream, 4);        /* gy[3:0] */
            g[1] |= bcdec__bitstream_read_bits(&bstream, 5);        /* gx[4:0] */
            b[3] |= bcdec__bitstream_read_bit(&bstream);            /* bz[0]   */
            g[3] |= bcdec__bitstream_read_bits(&bstream, 4);        /* gz[3:0] */
            b[1] |= bcdec__bitstream_read_bits(&bstream, 6);        /* bx[5:0] */
            b[2] |= bcdec__bitstream_read_bits(&bstream, 4);        /* by[3:0] */
            r[2] |= bcdec__bitstream_read_bits(&bstream, 5);        /* ry[4:0] */
            b[3] |= bcdec__bitstream_read_bit(&bstream) << 2;       /* bz[2]   */
            r[3] |= bcdec__bitstream_read_bits(&bstream, 5);        /* rz[4:0] */
            b[3] |= bcdec__bitstream_read_bit(&bstream) << 3;       /* bz[3]   */
            partition = bcdec__bitstream_read_bits(&bstream, 5);    /* d[4:0]  */
            mode = 8;
        } break;

        /* mode 10 */
        case 0b11110: {
            /* Partitition indices: 46 bits
               Partition: 5 bits
               Color Endpoints: 72 bits (6666, 6666, 6666) */
            r[0] |= bcdec__bitstream_read_bits(&bstream, 6);        /* rw[5:0] */
            g[3] |= bcdec__bitstream_read_bit(&bstream) << 4;       /* gz[4]   */
            b[3] |= bcdec__bitstream_read_bit(&bstream);            /* bz[0]   */
            b[3] |= bcdec__bitstream_read_bit(&bstream) << 1;       /* bz[1]   */
            b[2] |= bcdec__bitstream_read_bit(&bstream) << 4;       /* by[4]   */
            g[0] |= bcdec__bitstream_read_bits(&bstream, 6);        /* gw[5:0] */
            g[2] |= bcdec__bitstream_read_bit(&bstream) << 5;       /* gy[5]   */
            b[2] |= bcdec__bitstream_read_bit(&bstream) << 5;       /* by[5]   */
            b[3] |= bcdec__bitstream_read_bit(&bstream) << 2;       /* bz[2]   */
            g[2] |= bcdec__bitstream_read_bit(&bstream) << 4;       /* gy[4]   */
            b[0] |= bcdec__bitstream_read_bits(&bstream, 6);        /* bw[5:0] */
            g[3] |= bcdec__bitstream_read_bit(&bstream) << 5;       /* gz[5]   */
            b[3] |= bcdec__bitstream_read_bit(&bstream) << 3;       /* bz[3]   */
            b[3] |= bcdec__bitstream_read_bit(&bstream) << 5;       /* bz[5]   */
            b[3] |= bcdec__bitstream_read_bit(&bstream) << 4;       /* bz[4]   */
            r[1] |= bcdec__bitstream_read_bits(&bstream, 6);        /* rx[5:0] */
            g[2] |= bcdec__bitstream_read_bits(&bstream, 4);        /* gy[3:0] */
            g[1] |= bcdec__bitstream_read_bits(&bstream, 6);        /* gx[5:0] */
            g[3] |= bcdec__bitstream_read_bits(&bstream, 4);        /* gz[3:0] */
            b[1] |= bcdec__bitstream_read_bits(&bstream, 6);        /* bx[5:0] */
            b[2] |= bcdec__bitstream_read_bits(&bstream, 4);        /* by[3:0] */
            r[2] |= bcdec__bitstream_read_bits(&bstream, 6);        /* ry[5:0] */
            r[3] |= bcdec__bitstream_read_bits(&bstream, 6);        /* rz[5:0] */
            partition = bcdec__bitstream_read_bits(&bstream, 5);    /* d[4:0]  */
            mode = 9;
        } break;

        /* mode 11 */
        case 0b00011: {
            /* Partitition indices: 63 bits
               Partition: 0 bits
               Color Endpoints: 60 bits (10.10, 10.10, 10.10) */
            r[0] |= bcdec__bitstream_read_bits(&bstream, 10);       /* rw[9:0] */
            g[0] |= bcdec__bitstream_read_bits(&bstream, 10);       /* gw[9:0] */
            b[0] |= bcdec__bitstream_read_bits(&bstream, 10);       /* bw[9:0] */
            r[1] |= bcdec__bitstream_read_bits(&bstream, 10);       /* rx[9:0] */
            g[1] |= bcdec__bitstream_read_bits(&bstream, 10);       /* gx[9:0] */
            b[1] |= bcdec__bitstream_read_bits(&bstream, 10);       /* bx[9:0] */
            mode = 10;
        } break;

        /* mode 12 */
        case 0b00111: {
            /* Partitition indices: 63 bits
               Partition: 0 bits
               Color Endpoints: 60 bits (11.9, 11.9, 11.9) */
            r[0] |= bcdec__bitstream_read_bits(&bstream, 10);       /* rw[9:0] */
            g[0] |= bcdec__bitstream_read_bits(&bstream, 10);       /* gw[9:0] */
            b[0] |= bcdec__bitstream_read_bits(&bstream, 10);       /* bw[9:0] */
            r[1] |= bcdec__bitstream_read_bits(&bstream, 9);        /* rx[8:0] */
            r[0] |= bcdec__bitstream_read_bit(&bstream) << 10;      /* rw[10]  */
            g[1] |= bcdec__bitstream_read_bits(&bstream, 9);        /* gx[8:0] */
            g[0] |= bcdec__bitstream_read_bit(&bstream) << 10;      /* gw[10]  */
            b[1] |= bcdec__bitstream_read_bits(&bstream, 9);        /* bx[8:0] */
            b[0] |= bcdec__bitstream_read_bit(&bstream) << 10;      /* bw[10]  */
            mode = 11;
        } break;

        /* mode 13 */
        case 0b01011: {
            /* Partitition indices: 63 bits
               Partition: 0 bits
               Color Endpoints: 60 bits (12.8, 12.8, 12.8) */
            r[0] |= bcdec__bitstream_read_bits(&bstream, 10);       /* rw[9:0] */
            g[0] |= bcdec__bitstream_read_bits(&bstream, 10);       /* gw[9:0] */
            b[0] |= bcdec__bitstream_read_bits(&bstream, 10);       /* bw[9:0] */
            r[1] |= bcdec__bitstream_read_bits(&bstream, 8);        /* rx[7:0] */
            r[0] |= bcdec__bitstream_read_bits_r(&bstream, 2) << 10;/* rx[10:11] */
            g[1] |= bcdec__bitstream_read_bits(&bstream, 8);        /* gx[7:0] */
            g[0] |= bcdec__bitstream_read_bits_r(&bstream, 2) << 10;/* gx[10:11] */
            b[1] |= bcdec__bitstream_read_bits(&bstream, 8);        /* bx[7:0] */
            b[0] |= bcdec__bitstream_read_bits_r(&bstream, 2) << 10;/* bx[10:11] */
            mode = 12;
        } break;

        /* mode 14 */
        case 0b01111: {
            /* Partitition indices: 63 bits
               Partition: 0 bits
               Color Endpoints: 60 bits (16.4, 16.4, 16.4) */
            r[0] |= bcdec__bitstream_read_bits(&bstream, 10);       /* rw[9:0] */
            g[0] |= bcdec__bitstream_read_bits(&bstream, 10);       /* gw[9:0] */
            b[0] |= bcdec__bitstream_read_bits(&bstream, 10);       /* bw[9:0] */
            r[1] |= bcdec__bitstream_read_bits(&bstream, 4);        /* rx[3:0] */
            r[0] |= bcdec__bitstream_read_bits_r(&bstream, 6) << 10;/* rw[10:15] */
            g[1] |= bcdec__bitstream_read_bits(&bstream, 4);        /* gx[3:0] */
            g[0] |= bcdec__bitstream_read_bits_r(&bstream, 6) << 10;/* gw[10:15] */
            b[1] |= bcdec__bitstream_read_bits(&bstream, 4);        /* bx[3:0] */
            b[0] |= bcdec__bitstream_read_bits_r(&bstream, 6) << 10;/* bw[10:15] */
            mode = 13;
        } break;

        default: {
            /* Modes 10011, 10111, 11011, and 11111 (not shown) are reserved.
               Do not use these in your encoder. If the hardware is passed blocks
               with one of these modes specified, the resulting decompressed block
               must contain all zeroes in all channels except for the alpha channel. */
            for (i = 0; i < 4; ++i) {
                for (j = 0; j < 4; ++j) {
                    decompressed[j * 3 + 0] = 0;
                    decompressed[j * 3 + 1] = 0;
                    decompressed[j * 3 + 2] = 0;
                }
                decompressed += destinationPitch;
            }

            return;
        }
    }

    numPartitions = (mode >= 10) ? 0 : 1;

    actualBits0Mode = actual_bits_count[0][mode];
    if (isSigned) {
        r[0] = bcdec__extend_sign(r[0], actualBits0Mode);
        g[0] = bcdec__extend_sign(g[0], actualBits0Mode);
        b[0] = bcdec__extend_sign(b[0], actualBits0Mode);
    }

    /* Mode 11 (like Mode 10) does not use delta compression,
       and instead stores both color endpoints explicitly.  */
    if ((mode != 9 && mode != 10) || isSigned) {
        for (i = 1; i < (numPartitions + 1) * 2; ++i) {
            r[i] = bcdec__extend_sign(r[i], actual_bits_count[1][mode]);
            g[i] = bcdec__extend_sign(g[i], actual_bits_count[2][mode]);
            b[i] = bcdec__extend_sign(b[i], actual_bits_count[3][mode]);
        }
    }

    if (mode != 9 && mode != 10) {
        for (i = 1; i < (numPartitions + 1) * 2; ++i) {
            r[i] = bcdec__transform_inverse(r[i], r[0], actualBits0Mode, isSigned);
            g[i] = bcdec__transform_inverse(g[i], g[0], actualBits0Mode, isSigned);
            b[i] = bcdec__transform_inverse(b[i], b[0], actualBits0Mode, isSigned);
        }
    }

    for (i = 0; i < (numPartitions + 1) * 2; ++i) {
        r[i] = bcdec__unquantize(r[i], actualBits0Mode, isSigned);
        g[i] = bcdec__unquantize(g[i], actualBits0Mode, isSigned);
        b[i] = bcdec__unquantize(b[i], actualBits0Mode, isSigned);
    }

    weights = (mode >= 10) ? aWeight4 : aWeight3;
    for (i = 0; i < 4; ++i) {
        for (j = 0; j < 4; ++j) {
            partitionSet = (mode >= 10) ? ((i|j) ? 0 : 128) : partition_sets[partition][i][j];

            indexBits = (mode >= 10) ? 4 : 3;
            /* fix-up index is specified with one less bit */
            /* The fix-up index for subset 0 is always index 0 */
            if (partitionSet & 0x80) {
                indexBits--;
            }
            partitionSet &= 0x01;

            index = bcdec__bitstream_read_bits(&bstream, indexBits);

            ep_i = partitionSet * 2;
            decompressed[j * 3 + 0] = bcdec__finish_unquantize(
                                            bcdec__interpolate(r[ep_i], r[ep_i+1], weights, index), isSigned);
            decompressed[j * 3 + 1] = bcdec__finish_unquantize(
                                            bcdec__interpolate(g[ep_i], g[ep_i+1], weights, index), isSigned);
            decompressed[j * 3 + 2] = bcdec__finish_unquantize(
                                            bcdec__interpolate(b[ep_i], b[ep_i+1], weights, index), isSigned);
        }

        decompressed += destinationPitch;
    }
}

BCDECDEF void bcdec_bc6h_float(const void* compressedBlock, void* decompressedBlock, int destinationPitch, int isSigned) {
    unsigned short block[16*3];
    float* decompressed;
    const unsigned short* b;
    int i, j;

    bcdec_bc6h_half(compressedBlock, block, 4*3, isSigned);
    b = block;
    decompressed = (float*)decompressedBlock;
    for (i = 0; i < 4; ++i) {
        for (j = 0; j < 4; ++j) {
            decompressed[j * 3 + 0] = bcdec__half_to_float_quick(*b++);
            decompressed[j * 3 + 1] = bcdec__half_to_float_quick(*b++);
            decompressed[j * 3 + 2] = bcdec__half_to_float_quick(*b++);
        }
        decompressed += destinationPitch;
    }
}

static void bcdec__swap_values(int* a, int* b) {
    a[0] ^= b[0], b[0] ^= a[0], a[0] ^= b[0];
}

BCDECDEF void bcdec_bc7(const void* compressedBlock, void* decompressedBlock, int destinationPitch) {
    static char actual_bits_count[2][8] = {
        { 4, 6, 5, 7, 5, 7, 7, 5 },     /* RGBA  */
        { 0, 0, 0, 0, 6, 8, 7, 5 },     /* Alpha */
    };

    /* There are 64 possible partition sets for a two-region tile.
       Each 4x4 block represents a single shape.
       Here also every fix-up index has MSB bit set. */
    static unsigned char partition_sets[2][64][4][4] = {
        {   /* Partition table for 2-subset BPTC */
            { {128, 0,   1, 1}, {0, 0,   1, 1}, {  0, 0, 1, 1}, {0, 0, 1, 129} }, /*  0 */
            { {128, 0,   0, 1}, {0, 0,   0, 1}, {  0, 0, 0, 1}, {0, 0, 0, 129} }, /*  1 */
            { {128, 1,   1, 1}, {0, 1,   1, 1}, {  0, 1, 1, 1}, {0, 1, 1, 129} }, /*  2 */
            { {128, 0,   0, 1}, {0, 0,   1, 1}, {  0, 0, 1, 1}, {0, 1, 1, 129} }, /*  3 */
            { {128, 0,   0, 0}, {0, 0,   0, 1}, {  0, 0, 0, 1}, {0, 0, 1, 129} }, /*  4 */
            { {128, 0,   1, 1}, {0, 1,   1, 1}, {  0, 1, 1, 1}, {1, 1, 1, 129} }, /*  5 */
            { {128, 0,   0, 1}, {0, 0,   1, 1}, {  0, 1, 1, 1}, {1, 1, 1, 129} }, /*  6 */
            { {128, 0,   0, 0}, {0, 0,   0, 1}, {  0, 0, 1, 1}, {0, 1, 1, 129} }, /*  7 */
            { {128, 0,   0, 0}, {0, 0,   0, 0}, {  0, 0, 0, 1}, {0, 0, 1, 129} }, /*  8 */
            { {128, 0,   1, 1}, {0, 1,   1, 1}, {  1, 1, 1, 1}, {1, 1, 1, 129} }, /*  9 */
            { {128, 0,   0, 0}, {0, 0,   0, 1}, {  0, 1, 1, 1}, {1, 1, 1, 129} }, /* 10 */
            { {128, 0,   0, 0}, {0, 0,   0, 0}, {  0, 0, 0, 1}, {0, 1, 1, 129} }, /* 11 */
            { {128, 0,   0, 1}, {0, 1,   1, 1}, {  1, 1, 1, 1}, {1, 1, 1, 129} }, /* 12 */
            { {128, 0,   0, 0}, {0, 0,   0, 0}, {  1, 1, 1, 1}, {1, 1, 1, 129} }, /* 13 */
            { {128, 0,   0, 0}, {1, 1,   1, 1}, {  1, 1, 1, 1}, {1, 1, 1, 129} }, /* 14 */
            { {128, 0,   0, 0}, {0, 0,   0, 0}, {  0, 0, 0, 0}, {1, 1, 1, 129} }, /* 15 */
            { {128, 0,   0, 0}, {1, 0,   0, 0}, {  1, 1, 1, 0}, {1, 1, 1, 129} }, /* 16 */
            { {128, 1, 129, 1}, {0, 0,   0, 1}, {  0, 0, 0, 0}, {0, 0, 0,   0} }, /* 17 */
            { {128, 0,   0, 0}, {0, 0,   0, 0}, {129, 0, 0, 0}, {1, 1, 1,   0} }, /* 18 */
            { {128, 1, 129, 1}, {0, 0,   1, 1}, {  0, 0, 0, 1}, {0, 0, 0,   0} }, /* 19 */
            { {128, 0, 129, 1}, {0, 0,   0, 1}, {  0, 0, 0, 0}, {0, 0, 0,   0} }, /* 20 */
            { {128, 0,   0, 0}, {1, 0,   0, 0}, {129, 1, 0, 0}, {1, 1, 1,   0} }, /* 21 */
            { {128, 0,   0, 0}, {0, 0,   0, 0}, {129, 0, 0, 0}, {1, 1, 0,   0} }, /* 22 */
            { {128, 1,   1, 1}, {0, 0,   1, 1}, {  0, 0, 1, 1}, {0, 0, 0, 129} }, /* 23 */
            { {128, 0, 129, 1}, {0, 0,   0, 1}, {  0, 0, 0, 1}, {0, 0, 0,   0} }, /* 24 */
            { {128, 0,   0, 0}, {1, 0,   0, 0}, {129, 0, 0, 0}, {1, 1, 0,   0} }, /* 25 */
            { {128, 1, 129, 0}, {0, 1,   1, 0}, {  0, 1, 1, 0}, {0, 1, 1,   0} }, /* 26 */
            { {128, 0, 129, 1}, {0, 1,   1, 0}, {  0, 1, 1, 0}, {1, 1, 0,   0} }, /* 27 */
            { {128, 0,   0, 1}, {0, 1,   1, 1}, {129, 1, 1, 0}, {1, 0, 0,   0} }, /* 28 */
            { {128, 0,   0, 0}, {1, 1,   1, 1}, {129, 1, 1, 1}, {0, 0, 0,   0} }, /* 29 */
            { {128, 1, 129, 1}, {0, 0,   0, 1}, {  1, 0, 0, 0}, {1, 1, 1,   0} }, /* 30 */
            { {128, 0, 129, 1}, {1, 0,   0, 1}, {  1, 0, 0, 1}, {1, 1, 0,   0} }, /* 31 */
            { {128, 1,   0, 1}, {0, 1,   0, 1}, {  0, 1, 0, 1}, {0, 1, 0, 129} }, /* 32 */
            { {128, 0,   0, 0}, {1, 1,   1, 1}, {  0, 0, 0, 0}, {1, 1, 1, 129} }, /* 33 */
            { {128, 1,   0, 1}, {1, 0, 129, 0}, {  0, 1, 0, 1}, {1, 0, 1,   0} }, /* 34 */
            { {128, 0,   1, 1}, {0, 0,   1, 1}, {129, 1, 0, 0}, {1, 1, 0,   0} }, /* 35 */
            { {128, 0, 129, 1}, {1, 1,   0, 0}, {  0, 0, 1, 1}, {1, 1, 0,   0} }, /* 36 */
            { {128, 1,   0, 1}, {0, 1,   0, 1}, {129, 0, 1, 0}, {1, 0, 1,   0} }, /* 37 */
            { {128, 1,   1, 0}, {1, 0,   0, 1}, {  0, 1, 1, 0}, {1, 0, 0, 129} }, /* 38 */
            { {128, 1,   0, 1}, {1, 0,   1, 0}, {  1, 0, 1, 0}, {0, 1, 0, 129} }, /* 39 */
            { {128, 1, 129, 1}, {0, 0,   1, 1}, {  1, 1, 0, 0}, {1, 1, 1,   0} }, /* 40 */
            { {128, 0,   0, 1}, {0, 0,   1, 1}, {129, 1, 0, 0}, {1, 0, 0,   0} }, /* 41 */
            { {128, 0, 129, 1}, {0, 0,   1, 0}, {  0, 1, 0, 0}, {1, 1, 0,   0} }, /* 42 */
            { {128, 0, 129, 1}, {1, 0,   1, 1}, {  1, 1, 0, 1}, {1, 1, 0,   0} }, /* 43 */
            { {128, 1, 129, 0}, {1, 0,   0, 1}, {  1, 0, 0, 1}, {0, 1, 1,   0} }, /* 44 */
            { {128, 0,   1, 1}, {1, 1,   0, 0}, {  1, 1, 0, 0}, {0, 0, 1, 129} }, /* 45 */
            { {128, 1,   1, 0}, {0, 1,   1, 0}, {  1, 0, 0, 1}, {1, 0, 0, 129} }, /* 46 */
            { {128, 0,   0, 0}, {0, 1, 129, 0}, {  0, 1, 1, 0}, {0, 0, 0,   0} }, /* 47 */
            { {128, 1,   0, 0}, {1, 1, 129, 0}, {  0, 1, 0, 0}, {0, 0, 0,   0} }, /* 48 */
            { {128, 0, 129, 0}, {0, 1,   1, 1}, {  0, 0, 1, 0}, {0, 0, 0,   0} }, /* 49 */
            { {128, 0,   0, 0}, {0, 0, 129, 0}, {  0, 1, 1, 1}, {0, 0, 1,   0} }, /* 50 */
            { {128, 0,   0, 0}, {0, 1,   0, 0}, {129, 1, 1, 0}, {0, 1, 0,   0} }, /* 51 */
            { {128, 1,   1, 0}, {1, 1,   0, 0}, {  1, 0, 0, 1}, {0, 0, 1, 129} }, /* 52 */
            { {128, 0,   1, 1}, {0, 1,   1, 0}, {  1, 1, 0, 0}, {1, 0, 0, 129} }, /* 53 */
            { {128, 1, 129, 0}, {0, 0,   1, 1}, {  1, 0, 0, 1}, {1, 1, 0,   0} }, /* 54 */
            { {128, 0, 129, 1}, {1, 0,   0, 1}, {  1, 1, 0, 0}, {0, 1, 1,   0} }, /* 55 */
            { {128, 1,   1, 0}, {1, 1,   0, 0}, {  1, 1, 0, 0}, {1, 0, 0, 129} }, /* 56 */
            { {128, 1,   1, 0}, {0, 0,   1, 1}, {  0, 0, 1, 1}, {1, 0, 0, 129} }, /* 57 */
            { {128, 1,   1, 1}, {1, 1,   1, 0}, {  1, 0, 0, 0}, {0, 0, 0, 129} }, /* 58 */
            { {128, 0,   0, 1}, {1, 0,   0, 0}, {  1, 1, 1, 0}, {0, 1, 1, 129} }, /* 59 */
            { {128, 0,   0, 0}, {1, 1,   1, 1}, {  0, 0, 1, 1}, {0, 0, 1, 129} }, /* 60 */
            { {128, 0, 129, 1}, {0, 0,   1, 1}, {  1, 1, 1, 1}, {0, 0, 0,   0} }, /* 61 */
            { {128, 0, 129, 0}, {0, 0,   1, 0}, {  1, 1, 1, 0}, {1, 1, 1,   0} }, /* 62 */
            { {128, 1,   0, 0}, {0, 1,   0, 0}, {  0, 1, 1, 1}, {0, 1, 1, 129} }  /* 63 */
        },
        {   /* Partition table for 3-subset BPTC */
            { {128, 0, 1, 129}, {0,   0,   1, 1}, {  0,   2,   2, 1}, {  2,   2, 2, 130} }, /*  0 */
            { {128, 0, 0, 129}, {0,   0,   1, 1}, {130,   2,   1, 1}, {  2,   2, 2,   1} }, /*  1 */
            { {128, 0, 0,   0}, {2,   0,   0, 1}, {130,   2,   1, 1}, {  2,   2, 1, 129} }, /*  2 */
            { {128, 2, 2, 130}, {0,   0,   2, 2}, {  0,   0,   1, 1}, {  0,   1, 1, 129} }, /*  3 */
            { {128, 0, 0,   0}, {0,   0,   0, 0}, {129,   1,   2, 2}, {  1,   1, 2, 130} }, /*  4 */
            { {128, 0, 1, 129}, {0,   0,   1, 1}, {  0,   0,   2, 2}, {  0,   0, 2, 130} }, /*  5 */
            { {128, 0, 2, 130}, {0,   0,   2, 2}, {  1,   1,   1, 1}, {  1,   1, 1, 129} }, /*  6 */
            { {128, 0, 1,   1}, {0,   0,   1, 1}, {130,   2,   1, 1}, {  2,   2, 1, 129} }, /*  7 */
            { {128, 0, 0,   0}, {0,   0,   0, 0}, {129,   1,   1, 1}, {  2,   2, 2, 130} }, /*  8 */
            { {128, 0, 0,   0}, {1,   1,   1, 1}, {129,   1,   1, 1}, {  2,   2, 2, 130} }, /*  9 */
            { {128, 0, 0,   0}, {1,   1, 129, 1}, {  2,   2,   2, 2}, {  2,   2, 2, 130} }, /* 10 */
            { {128, 0, 1,   2}, {0,   0, 129, 2}, {  0,   0,   1, 2}, {  0,   0, 1, 130} }, /* 11 */
            { {128, 1, 1,   2}, {0,   1, 129, 2}, {  0,   1,   1, 2}, {  0,   1, 1, 130} }, /* 12 */
            { {128, 1, 2,   2}, {0, 129,   2, 2}, {  0,   1,   2, 2}, {  0,   1, 2, 130} }, /* 13 */
            { {128, 0, 1, 129}, {0,   1,   1, 2}, {  1,   1,   2, 2}, {  1,   2, 2, 130} }, /* 14 */
            { {128, 0, 1, 129}, {2,   0,   0, 1}, {130,   2,   0, 0}, {  2,   2, 2,   0} }, /* 15 */
            { {128, 0, 0, 129}, {0,   0,   1, 1}, {  0,   1,   1, 2}, {  1,   1, 2, 130} }, /* 16 */
            { {128, 1, 1, 129}, {0,   0,   1, 1}, {130,   0,   0, 1}, {  2,   2, 0,   0} }, /* 17 */
            { {128, 0, 0,   0}, {1,   1,   2, 2}, {129,   1,   2, 2}, {  1,   1, 2, 130} }, /* 18 */
            { {128, 0, 2, 130}, {0,   0,   2, 2}, {  0,   0,   2, 2}, {  1,   1, 1, 129} }, /* 19 */
            { {128, 1, 1, 129}, {0,   1,   1, 1}, {  0,   2,   2, 2}, {  0,   2, 2, 130} }, /* 20 */
            { {128, 0, 0, 129}, {0,   0,   0, 1}, {130,   2,   2, 1}, {  2,   2, 2,   1} }, /* 21 */
            { {128, 0, 0,   0}, {0,   0, 129, 1}, {  0,   1,   2, 2}, {  0,   1, 2, 130} }, /* 22 */
            { {128, 0, 0,   0}, {1,   1,   0, 0}, {130,   2, 129, 0}, {  2,   2, 1,   0} }, /* 23 */
            { {128, 1, 2, 130}, {0, 129,   2, 2}, {  0,   0,   1, 1}, {  0,   0, 0,   0} }, /* 24 */
            { {128, 0, 1,   2}, {0,   0,   1, 2}, {129,   1,   2, 2}, {  2,   2, 2, 130} }, /* 25 */
            { {128, 1, 1,   0}, {1,   2, 130, 1}, {129,   2,   2, 1}, {  0,   1, 1,   0} }, /* 26 */
            { {128, 0, 0,   0}, {0,   1, 129, 0}, {  1,   2, 130, 1}, {  1,   2, 2,   1} }, /* 27 */
            { {128, 0, 2,   2}, {1,   1,   0, 2}, {129,   1,   0, 2}, {  0,   0, 2, 130} }, /* 28 */
            { {128, 1, 1,   0}, {0, 129,   1, 0}, {  2,   0,   0, 2}, {  2,   2, 2, 130} }, /* 29 */
            { {128, 0, 1,   1}, {0,   1,   2, 2}, {  0,   1, 130, 2}, {  0,   0, 1, 129} }, /* 30 */
            { {128, 0, 0,   0}, {2,   0,   0, 0}, {130,   2,   1, 1}, {  2,   2, 2, 129} }, /* 31 */
            { {128, 0, 0,   0}, {0,   0,   0, 2}, {129,   1,   2, 2}, {  1,   2, 2, 130} }, /* 32 */
            { {128, 2, 2, 130}, {0,   0,   2, 2}, {  0,   0,   1, 2}, {  0,   0, 1, 129} }, /* 33 */
            { {128, 0, 1, 129}, {0,   0,   1, 2}, {  0,   0,   2, 2}, {  0,   2, 2, 130} }, /* 34 */
            { {128, 1, 2,   0}, {0, 129,   2, 0}, {  0,   1, 130, 0}, {  0,   1, 2,   0} }, /* 35 */
            { {128, 0, 0,   0}, {1,   1, 129, 1}, {  2,   2, 130, 2}, {  0,   0, 0,   0} }, /* 36 */
            { {128, 1, 2,   0}, {1,   2,   0, 1}, {130,   0, 129, 2}, {  0,   1, 2,   0} }, /* 37 */
            { {128, 1, 2,   0}, {2,   0,   1, 2}, {129, 130,   0, 1}, {  0,   1, 2,   0} }, /* 38 */
            { {128, 0, 1,   1}, {2,   2,   0, 0}, {  1,   1, 130, 2}, {  0,   0, 1, 129} }, /* 39 */
            { {128, 0, 1,   1}, {1,   1, 130, 2}, {  2,   2,   0, 0}, {  0,   0, 1, 129} }, /* 40 */
            { {128, 1, 0, 129}, {0,   1,   0, 1}, {  2,   2,   2, 2}, {  2,   2, 2, 130} }, /* 41 */
            { {128, 0, 0,   0}, {0,   0,   0, 0}, {130,   1,   2, 1}, {  2,   1, 2, 129} }, /* 42 */
            { {128, 0, 2,   2}, {1, 129,   2, 2}, {  0,   0,   2, 2}, {  1,   1, 2, 130} }, /* 43 */
            { {128, 0, 2, 130}, {0,   0,   1, 1}, {  0,   0,   2, 2}, {  0,   0, 1, 129} }, /* 44 */
            { {128, 2, 2,   0}, {1,   2, 130, 1}, {  0,   2,   2, 0}, {  1,   2, 2, 129} }, /* 45 */
            { {128, 1, 0,   1}, {2,   2, 130, 2}, {  2,   2,   2, 2}, {  0,   1, 0, 129} }, /* 46 */
            { {128, 0, 0,   0}, {2,   1,   2, 1}, {130,   1,   2, 1}, {  2,   1, 2, 129} }, /* 47 */
            { {128, 1, 0, 129}, {0,   1,   0, 1}, {  0,   1,   0, 1}, {  2,   2, 2, 130} }, /* 48 */
            { {128, 2, 2, 130}, {0,   1,   1, 1}, {  0,   2,   2, 2}, {  0,   1, 1, 129} }, /* 49 */
            { {128, 0, 0,   2}, {1, 129,   1, 2}, {  0,   0,   0, 2}, {  1,   1, 1, 130} }, /* 50 */
            { {128, 0, 0,   0}, {2, 129,   1, 2}, {  2,   1,   1, 2}, {  2,   1, 1, 130} }, /* 51 */
            { {128, 2, 2,   2}, {0, 129,   1, 1}, {  0,   1,   1, 1}, {  0,   2, 2, 130} }, /* 52 */
            { {128, 0, 0,   2}, {1,   1,   1, 2}, {129,   1,   1, 2}, {  0,   0, 0, 130} }, /* 53 */
            { {128, 1, 1,   0}, {0, 129,   1, 0}, {  0,   1,   1, 0}, {  2,   2, 2, 130} }, /* 54 */
            { {128, 0, 0,   0}, {0,   0,   0, 0}, {  2,   1, 129, 2}, {  2,   1, 1, 130} }, /* 55 */
            { {128, 1, 1,   0}, {0, 129,   1, 0}, {  2,   2,   2, 2}, {  2,   2, 2, 130} }, /* 56 */
            { {128, 0, 2,   2}, {0,   0,   1, 1}, {  0,   0, 129, 1}, {  0,   0, 2, 130} }, /* 57 */
            { {128, 0, 2,   2}, {1,   1,   2, 2}, {129,   1,   2, 2}, {  0,   0, 2, 130} }, /* 58 */
            { {128, 0, 0,   0}, {0,   0,   0, 0}, {  0,   0,   0, 0}, {  2, 129, 1, 130} }, /* 59 */
            { {128, 0, 0, 130}, {0,   0,   0, 1}, {  0,   0,   0, 2}, {  0,   0, 0, 129} }, /* 60 */
            { {128, 2, 2,   2}, {1,   2,   2, 2}, {  0,   2,   2, 2}, {129,   2, 2, 130} }, /* 61 */
            { {128, 1, 0, 129}, {2,   2,   2, 2}, {  2,   2,   2, 2}, {  2,   2, 2, 130} }, /* 62 */
            { {128, 1, 1, 129}, {2,   0,   1, 1}, {130,   2,   0, 1}, {  2,   2, 2,   0} }  /* 63 */
        }
    };

    static int aWeight2[] = { 0, 21, 43, 64 };
    static int aWeight3[] = { 0, 9, 18, 27, 37, 46, 55, 64 };
    static int aWeight4[] = { 0, 4, 9, 13, 17, 21, 26, 30, 34, 38, 43, 47, 51, 55, 60, 64 };

    static unsigned char sModeHasPBits = 0b11001011;

    bcdec__bitstream_t bstream;
    int mode, partition, numPartitions, numEndpoints, i, j, k, rotation, partitionSet;
    int indexSelectionBit, indexBits, indexBits2, index, index2;
    int endpoints[6][4];
    char indices[4][4];
    int r, g, b, a;
    int* weights, * weights2;
    unsigned char* decompressed;

    decompressed = (unsigned char*)decompressedBlock;

    bstream.low = ((unsigned long long*)compressedBlock)[0];
    bstream.high = ((unsigned long long*)compressedBlock)[1];

    for (mode = 0; mode < 8 && (0 == bcdec__bitstream_read_bit(&bstream)); ++mode);

    /* unexpected mode, clear the block (transparent black) */
    if (mode >= 8) {
        for (i = 0; i < 4; ++i) {
            for (j = 0; j < 4; ++j) {
                decompressed[j * 4 + 0] = 0;
                decompressed[j * 4 + 1] = 0;
                decompressed[j * 4 + 2] = 0;
                decompressed[j * 4 + 3] = 0;
            }
            decompressed += destinationPitch;
        }

        return;
    }

    partition = 0;
    numPartitions = 1;
    rotation = 0;
    indexSelectionBit = 0;

    if (mode == 0 || mode == 1 || mode == 2 || mode == 3 || mode == 7) {
        numPartitions = (mode == 0 || mode == 2) ? 3 : 2;
        partition = bcdec__bitstream_read_bits(&bstream, (mode == 0) ? 4 : 6);
    }

    numEndpoints = numPartitions * 2;

    if (mode == 4 || mode == 5) {
        rotation = bcdec__bitstream_read_bits(&bstream, 2);

        if (mode == 4) {
            indexSelectionBit = bcdec__bitstream_read_bit(&bstream);
        }
    }

    /* Extract endpoints */
    /* RGB */
    for (i = 0; i < 3; ++i) {
        for (j = 0; j < numEndpoints; ++j) {
            endpoints[j][i] = bcdec__bitstream_read_bits(&bstream, actual_bits_count[0][mode]);
        }
    }
    /* Alpha (if any) */
    if (actual_bits_count[1][mode] > 0) {
        for (j = 0; j < numEndpoints; ++j) {
            endpoints[j][3] = bcdec__bitstream_read_bits(&bstream, actual_bits_count[1][mode]);
        }
    }

    /* Fully decode endpoints */
    /* First handle modes that have P-bits */
    if (mode == 0 || mode == 1 || mode == 3 || mode == 6 || mode == 7) {
        for (i = 0; i < numEndpoints; ++i) {
            /* component-wise left-shift */
            for (j = 0; j < 4; ++j) {
                endpoints[i][j] <<= 1;
            }
        }

        /* if P-bit is shared */
        if (mode == 1) {
            i = bcdec__bitstream_read_bit(&bstream);
            j = bcdec__bitstream_read_bit(&bstream);

            /* rgb component-wise insert pbits */
            for (k = 0; k < 3; ++k) {
                endpoints[0][k] |= i;
                endpoints[1][k] |= i;
                endpoints[2][k] |= j;
                endpoints[3][k] |= j;
            }
        } else if (sModeHasPBits & (1 << mode)) {
            /* unique P-bit per endpoint */
            for (i = 0; i < numEndpoints; ++i) {
                j = bcdec__bitstream_read_bit(&bstream);
                for (k = 0; k < 4; ++k) {
                    endpoints[i][k] |= j;
                }
            }
        }
    }

    for (i = 0; i < numEndpoints; ++i) {
        /* get color components precision including pbit */
        j = actual_bits_count[0][mode] + ((sModeHasPBits >> mode) & 1);

        for (k = 0; k < 3; ++k) {
            /* left shift endpoint components so that their MSB lies in bit 7 */
            endpoints[i][k] = endpoints[i][k] << (8 - j);
            /* Replicate each component's MSB into the LSBs revealed by the left-shift operation above */
            endpoints[i][k] = endpoints[i][k] | (endpoints[i][k] >> j);
        }

        /* get alpha component precision including pbit */
        j = actual_bits_count[1][mode] + ((sModeHasPBits >> mode) & 1);

        /* left shift endpoint components so that their MSB lies in bit 7 */
        endpoints[i][3] = endpoints[i][3] << (8 - j);
        /* Replicate each component's MSB into the LSBs revealed by the left-shift operation above */
        endpoints[i][3] = endpoints[i][3] | (endpoints[i][3] >> j);
    }

    /* If this mode does not explicitly define the alpha component */
    /* set alpha equal to 1.0 */
    if (!actual_bits_count[1][mode]) {
        for (j = 0; j < numEndpoints; ++j) {
            endpoints[j][3] = 0xFF;
        }
    }

    /* Determine weights tables */
    indexBits = (mode == 0 || mode == 1) ? 3 : ((mode == 6) ? 4 : 2);
    indexBits2 = (mode == 4) ? 3 : ((mode == 5) ? 2 : 0);
    weights = (indexBits == 2) ? aWeight2 : ((indexBits == 3) ? aWeight3 : aWeight4);
    weights2 = (indexBits2 == 2) ? aWeight2 : aWeight3;

    /* Quite inconvenient that indices aren't interleaved so we have to make 2 passes here */
    /* Pass #1: collecting color indices */
    for (i = 0; i < 4; ++i) {
        for (j = 0; j < 4; ++j) {
            partitionSet = (numPartitions == 1) ? ((i | j) ? 0 : 128) : partition_sets[numPartitions - 2][partition][i][j];

            indexBits = (mode == 0 || mode == 1) ? 3 : ((mode == 6) ? 4 : 2);
            /* fix-up index is specified with one less bit */
            /* The fix-up index for subset 0 is always index 0 */
            if (partitionSet & 0x80) {
                indexBits--;
            }

            indices[i][j] = bcdec__bitstream_read_bits(&bstream, indexBits);
        }
    }

    /* Pass #2: reading alpha indices (if any) and interpolating & rotating */
    for (i = 0; i < 4; ++i) {
        for (j = 0; j < 4; ++j) {
            partitionSet = (numPartitions == 1) ? ((i|j) ? 0 : 128) : partition_sets[numPartitions - 2][partition][i][j];
            partitionSet &= 0x03;

            index = indices[i][j];

            if (!indexBits2) {
                r = bcdec__interpolate(endpoints[partitionSet * 2][0], endpoints[partitionSet * 2 + 1][0], weights, index);
                g = bcdec__interpolate(endpoints[partitionSet * 2][1], endpoints[partitionSet * 2 + 1][1], weights, index);
                b = bcdec__interpolate(endpoints[partitionSet * 2][2], endpoints[partitionSet * 2 + 1][2], weights, index);
                a = bcdec__interpolate(endpoints[partitionSet * 2][3], endpoints[partitionSet * 2 + 1][3], weights, index);
            } else {
                index2 = bcdec__bitstream_read_bits(&bstream, (i|j) ? indexBits2 : (indexBits2 - 1));
                /* The index value for interpolating color comes from the secondary index bits for the texel
                   if the mode has an index selection bit and its value is one, and from the primary index bits otherwise.
                   The alpha index comes from the secondary index bits if the block has a secondary index and
                   the block either doesn’t have an index selection bit or that bit is zero, and from the primary index bits otherwise. */
                if (!indexSelectionBit) {
                    r = bcdec__interpolate(endpoints[partitionSet * 2][0], endpoints[partitionSet * 2 + 1][0],  weights,  index);
                    g = bcdec__interpolate(endpoints[partitionSet * 2][1], endpoints[partitionSet * 2 + 1][1],  weights,  index);
                    b = bcdec__interpolate(endpoints[partitionSet * 2][2], endpoints[partitionSet * 2 + 1][2],  weights,  index);
                    a = bcdec__interpolate(endpoints[partitionSet * 2][3], endpoints[partitionSet * 2 + 1][3], weights2, index2);
                } else {
                    r = bcdec__interpolate(endpoints[partitionSet * 2][0], endpoints[partitionSet * 2 + 1][0], weights2, index2);
                    g = bcdec__interpolate(endpoints[partitionSet * 2][1], endpoints[partitionSet * 2 + 1][1], weights2, index2);
                    b = bcdec__interpolate(endpoints[partitionSet * 2][2], endpoints[partitionSet * 2 + 1][2], weights2, index2);
                    a = bcdec__interpolate(endpoints[partitionSet * 2][3], endpoints[partitionSet * 2 + 1][3],  weights,  index);
                }
            }

            switch (rotation) {
                case 1: {   /* 01 – Block format is Scalar(R) Vector(AGB) - swap A and R */
                    bcdec__swap_values(&a, &r);
                } break;
                case 2: {   /* 10 – Block format is Scalar(G) Vector(RAB) - swap A and G */
                    bcdec__swap_values(&a, &g);
                } break;
                case 3: {   /* 11 - Block format is Scalar(B) Vector(RGA) - swap A and B */
                    bcdec__swap_values(&a, &b);
                } break;
            }

            decompressed[j * 4 + 0] = r;
            decompressed[j * 4 + 1] = g;
            decompressed[j * 4 + 2] = b;
            decompressed[j * 4 + 3] = a;
        }

        decompressed += destinationPitch;
    }
}

#endif /* BCDEC_IMPLEMENTATION */

/* LICENSE:

This software is available under 2 licenses -- choose whichever you prefer.

------------------------------------------------------------------------------
ALTERNATIVE A - MIT License

Copyright (c) 2022 Sergii Kudlai

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

------------------------------------------------------------------------------
ALTERNATIVE B - The Unlicense

This is free and unencumbered software released into the public domain.

Anyone is free to copy, modify, publish, use, compile, sell, or
distribute this software, either in source code form or as a compiled
binary, for any purpose, commercial or non-commercial, and by any
means.

In jurisdictions that recognize copyright laws, the author or authors
of this software dedicate any and all copyright interest in the
software to the public domain. We make this dedication for the benefit
of the public at large and to the detriment of our heirs and
successors. We intend this dedication to be an overt act of
relinquishment in perpetuity of all present and future rights to this
software under copyright law.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

For more information, please refer to <https://unlicense.org>

*/

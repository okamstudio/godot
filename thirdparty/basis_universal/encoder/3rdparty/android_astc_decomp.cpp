// File: android_astc_decomp.cpp

/*-------------------------------------------------------------------------
 * drawElements Quality Program Tester Core
 * ----------------------------------------
 *
 * Copyright 2016 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * rg: Removed external dependencies, minor fix to decompress() so it converts non-sRGB
 * output to 8-bits correctly. I've compared this decoder's output
 * vs. astc-codec with random inputs.
 * 
 *//*!
 * \file
 * \brief ASTC Utilities.
 *//*--------------------------------------------------------------------*/
#include "android_astc_decomp.h"
#include <assert.h>
#include <algorithm>
#include <fenv.h>
#include <math.h>

#define DE_LENGTH_OF_ARRAY(x) (sizeof(x)/sizeof(x[0]))
#define DE_UNREF(x) (void)x

typedef uint8_t deUint8;
typedef int8_t deInt8;
typedef uint32_t deUint32;
typedef int32_t deInt32;
typedef uint16_t deUint16;
typedef int16_t deInt16;
typedef int64_t deInt64;
typedef uint64_t deUint64;

#define DE_ASSERT assert

#ifdef _MSC_VER
#pragma warning (disable:4505) // unreferenced local function has been removed
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#endif

namespace basisu_astc
{
    template <typename S> inline S maximum(S a, S b) { return (a > b) ? a : b; }
    template <typename S> inline S maximum(S a, S b, S c) { return maximum(maximum(a, b), c); }
    template <typename S> inline S maximum(S a, S b, S c, S d) { return maximum(maximum(maximum(a, b), c), d); }

    static bool inBounds(int v, int l, int h)
    {
        return (v >= l) && (v < h);
    }

    static bool inRange(int v, int l, int h)
    {
        return (v >= l) && (v <= h);
    }

    template<typename T>
    static inline T max(T a, T b)
    {
        return (a > b) ? a : b;
    }

    template<typename T>
    static inline T min(T a, T b)
    {
        return (a < b) ? a : b;
    }

    template<typename T>
    static inline T clamp(T a, T l, T h)
    {
        if (a < l)
            return l;
        else if (a > h)
            return h;
        return a;
    }

    struct UVec4
    {
        uint32_t m_c[4];

        UVec4()
        {
            m_c[0] = 0;
            m_c[1] = 0;
            m_c[2] = 0;
            m_c[3] = 0;
        }

        UVec4(uint32_t x, uint32_t y, uint32_t z, uint32_t w)
        {
            m_c[0] = x;
            m_c[1] = y;
            m_c[2] = z;
            m_c[3] = w;
        }

        uint32_t x() const { return m_c[0]; }
        uint32_t y() const { return m_c[1]; }
        uint32_t z() const { return m_c[2]; }
        uint32_t w() const { return m_c[3]; }

        uint32_t& x() { return m_c[0]; }
        uint32_t& y() { return m_c[1]; }
        uint32_t& z() { return m_c[2]; }
        uint32_t& w() { return m_c[3]; }

        uint32_t operator[] (uint32_t idx) const { assert(idx < 4);  return m_c[idx]; }
        uint32_t& operator[] (uint32_t idx) { assert(idx < 4);  return m_c[idx]; }
    };

    struct IVec4
    {
        int32_t m_c[4];

        IVec4()
        {
            m_c[0] = 0;
            m_c[1] = 0;
            m_c[2] = 0;
            m_c[3] = 0;
        }

        IVec4(int32_t x, int32_t y, int32_t z, int32_t w)
        {
            m_c[0] = x;
            m_c[1] = y;
            m_c[2] = z;
            m_c[3] = w;
        }

        int32_t x() const { return m_c[0]; }
        int32_t y() const { return m_c[1]; }
        int32_t z() const { return m_c[2]; }
        int32_t w() const { return m_c[3]; }

        int32_t& x() { return m_c[0]; }
        int32_t& y() { return m_c[1]; }
        int32_t& z() { return m_c[2]; }
        int32_t& w() { return m_c[3]; }

        UVec4 asUint() const
        {
            return UVec4(maximum(0, m_c[0]), maximum(0, m_c[1]), maximum(0, m_c[2]), maximum(0, m_c[3]));
        }

        int32_t operator[] (uint32_t idx) const { assert(idx < 4);  return m_c[idx]; }
        int32_t& operator[] (uint32_t idx) { assert(idx < 4);  return m_c[idx]; }
    };

    struct IVec3
    {
        int32_t m_c[3];

        IVec3()
        {
            m_c[0] = 0;
            m_c[1] = 0;
            m_c[2] = 0;
        }

        IVec3(int32_t x, int32_t y, int32_t z)
        {
            m_c[0] = x;
            m_c[1] = y;
            m_c[2] = z;
        }

        int32_t x() const { return m_c[0]; }
        int32_t y() const { return m_c[1]; }
        int32_t z() const { return m_c[2]; }

        int32_t& x() { return m_c[0]; }
        int32_t& y() { return m_c[1]; }
        int32_t& z() { return m_c[2]; }

        int32_t operator[] (uint32_t idx) const { assert(idx < 3);  return m_c[idx]; }
        int32_t& operator[] (uint32_t idx) { assert(idx < 3);  return m_c[idx]; }
    };

    static uint32_t deDivRoundUp32(uint32_t a, uint32_t b)
    {
        return (a + b - 1) / b;
    }

    static bool deInBounds32(uint32_t v, uint32_t l, uint32_t h)
    {
        return (v >= l) && (v < h);
    }

namespace astc 
{

using std::vector;

namespace
{

// Common utilities
enum
{
    MAX_BLOCK_WIDTH     = 12,
    MAX_BLOCK_HEIGHT    = 12
};

inline deUint32 getBit (deUint32 src, int ndx)
{
    DE_ASSERT(basisu_astc::inBounds(ndx, 0, 32));
    return (src >> ndx) & 1;
}

inline deUint32 getBits (deUint32 src, int low, int high)
{
    const int numBits = (high-low) + 1;
    DE_ASSERT(basisu_astc::inRange(numBits, 1, 32));

    if (numBits < 32)
        return (deUint32)((src >> low) & ((1u<<numBits)-1));
    else
        return (deUint32)((src >> low) & 0xFFFFFFFFu);
}

inline bool isBitSet (deUint32 src, int ndx)
{
    return getBit(src, ndx) != 0;
}

inline deUint32 reverseBits (deUint32 src, int numBits)
{
    DE_ASSERT(basisu_astc::inRange(numBits, 0, 32));
    
    deUint32 result = 0;
    for (int i = 0; i < numBits; i++)
        result |= ((src >> i) & 1) << (numBits-1-i);

    return result;
}

inline deUint32 bitReplicationScale (deUint32 src, int numSrcBits, int numDstBits)
{
    DE_ASSERT(numSrcBits <= numDstBits);
    DE_ASSERT((src & ((1<<numSrcBits)-1)) == src);

    deUint32 dst = 0;
    for (int shift = numDstBits-numSrcBits; shift > -numSrcBits; shift -= numSrcBits)
        dst |= (shift >= 0) ? (src << shift) : (src >> -shift);

    return dst;
}

inline deInt32 signExtend (deInt32 src, int numSrcBits)
{
    DE_ASSERT(basisu_astc::inRange(numSrcBits, 2, 31));

    const bool negative = (src & (1 << (numSrcBits-1))) != 0;
    return src | (negative ? ~((1 << numSrcBits) - 1) : 0);
}

typedef uint16_t deFloat16;

inline bool isFloat16InfOrNan (deFloat16 v)
{
    return getBits(v, 10, 14) == 31;
}

float deFloat16To32(deFloat16 val16)
{
    deUint32 sign;
    deUint32 expotent;
    deUint32 mantissa;

    union
    {
        float       f;
        deUint32    u;
    } x;

    x.u = 0u;

    sign = ((deUint32)val16 >> 15u) & 0x00000001u;
    expotent = ((deUint32)val16 >> 10u) & 0x0000001fu;
    mantissa = (deUint32)val16 & 0x000003ffu;

    if (expotent == 0u)
    {
        if (mantissa == 0u)
        {
            /* +/- 0 */
            x.u = sign << 31u;
            return x.f;
        }
        else
        {
            /* Denormalized, normalize it. */

            while (!(mantissa & 0x00000400u))
            {
                mantissa <<= 1u;
                expotent -= 1u;
            }

            expotent += 1u;
            mantissa &= ~0x00000400u;
        }
    }
    else if (expotent == 31u)
    {
        if (mantissa == 0u)
        {
            /* +/- InF */
            x.u = (sign << 31u) | 0x7f800000u;
            return x.f;
        }
        else
        {
            /* +/- NaN */
            x.u = (sign << 31u) | 0x7f800000u | (mantissa << 13u);
            return x.f;
        }
    }

    expotent = expotent + (127u - 15u);
    mantissa = mantissa << 13u;

    x.u = (sign << 31u) | (expotent << 23u) | mantissa;
    return x.f;
}

enum ISEMode
{
    ISEMODE_TRIT = 0,
    ISEMODE_QUINT,
    ISEMODE_PLAIN_BIT,
    ISEMODE_LAST
};

struct ISEParams
{
    ISEMode     mode;
    int         numBits;
    ISEParams (ISEMode mode_, int numBits_) : mode(mode_), numBits(numBits_) {}
};

inline int computeNumRequiredBits (const ISEParams& iseParams, int numValues)
{
    switch (iseParams.mode)
    {
        case ISEMODE_TRIT:          return deDivRoundUp32(numValues*8, 5) + numValues*iseParams.numBits;
        case ISEMODE_QUINT:         return deDivRoundUp32(numValues*7, 3) + numValues*iseParams.numBits;
        case ISEMODE_PLAIN_BIT:     return numValues*iseParams.numBits;
        default:
            DE_ASSERT(false);
            return -1;
    }
}

ISEParams computeMaximumRangeISEParams (int numAvailableBits, int numValuesInSequence)
{
    int curBitsForTritMode      = 6;
    int curBitsForQuintMode     = 5;
    int curBitsForPlainBitMode  = 8;

    while (true)
    {
        DE_ASSERT(curBitsForTritMode > 0 || curBitsForQuintMode > 0 || curBitsForPlainBitMode > 0);
        const int tritRange         = (curBitsForTritMode > 0)        ? (3 << curBitsForTritMode) - 1         : -1;
        const int quintRange        = (curBitsForQuintMode > 0)       ? (5 << curBitsForQuintMode) - 1        : -1;
        const int plainBitRange     = (curBitsForPlainBitMode > 0)    ? (1 << curBitsForPlainBitMode) - 1     : -1;
        const int maxRange          = basisu_astc::max(basisu_astc::max(tritRange, quintRange), plainBitRange);

        if (maxRange == tritRange)
        {
            const ISEParams params(ISEMODE_TRIT, curBitsForTritMode);

            if (computeNumRequiredBits(params, numValuesInSequence) <= numAvailableBits)
                return ISEParams(ISEMODE_TRIT, curBitsForTritMode);

            curBitsForTritMode--;
        }
        else if (maxRange == quintRange)
        {
            const ISEParams params(ISEMODE_QUINT, curBitsForQuintMode);

            if (computeNumRequiredBits(params, numValuesInSequence) <= numAvailableBits)
                return ISEParams(ISEMODE_QUINT, curBitsForQuintMode);

            curBitsForQuintMode--;
        }
        else
        {
            const ISEParams params(ISEMODE_PLAIN_BIT, curBitsForPlainBitMode);
            DE_ASSERT(maxRange == plainBitRange);

            if (computeNumRequiredBits(params, numValuesInSequence) <= numAvailableBits)
                return ISEParams(ISEMODE_PLAIN_BIT, curBitsForPlainBitMode);

            curBitsForPlainBitMode--;
        }
    }
}

inline int computeNumColorEndpointValues (deUint32 endpointMode)
{
    DE_ASSERT(endpointMode < 16);
    return (endpointMode/4 + 1) * 2;
}

// Decompression utilities
enum DecompressResult
{
    DECOMPRESS_RESULT_VALID_BLOCK   = 0,    //!< Decompressed valid block
    DECOMPRESS_RESULT_ERROR,                //!< Encountered error while decompressing, error color written
    DECOMPRESS_RESULT_LAST
};

// A helper for getting bits from a 128-bit block.
class Block128
{
private:
    typedef deUint64 Word;

    enum
    {
        WORD_BYTES  = sizeof(Word),
        WORD_BITS   = 8*WORD_BYTES,
        NUM_WORDS   = 128 / WORD_BITS
    };
    //DE_STATIC_ASSERT(128 % WORD_BITS == 0);

public:
    Block128 (const deUint8* src)
    {
        for (int wordNdx = 0; wordNdx < NUM_WORDS; wordNdx++)
        {
            m_words[wordNdx] = 0;
            for (int byteNdx = 0; byteNdx < WORD_BYTES; byteNdx++)
                m_words[wordNdx] |= (Word)src[wordNdx*WORD_BYTES + byteNdx] << (8*byteNdx);
        }
    }

    deUint32 getBit (int ndx) const
    {
        DE_ASSERT(basisu_astc::inBounds(ndx, 0, 128));
        return (m_words[ndx / WORD_BITS] >> (ndx % WORD_BITS)) & 1;
    }

    deUint32 getBits (int low, int high) const
    {
        DE_ASSERT(basisu_astc::inBounds(low, 0, 128));
        DE_ASSERT(basisu_astc::inBounds(high, 0, 128));
        DE_ASSERT(basisu_astc::inRange(high-low+1, 0, 32));

        if (high-low+1 == 0)
            return 0;

        const int word0Ndx = low / WORD_BITS;
        const int word1Ndx = high / WORD_BITS;
        // \note "foo << bar << 1" done instead of "foo << (bar+1)" to avoid overflow, i.e. shift amount being too big.
        if (word0Ndx == word1Ndx)
            return (deUint32)((m_words[word0Ndx] & ((((Word)1 << high%WORD_BITS << 1) - 1))) >> ((Word)low % WORD_BITS));
        else
        {
            DE_ASSERT(word1Ndx == word0Ndx + 1);
            return (deUint32)(m_words[word0Ndx] >> (low%WORD_BITS)) |
                   (deUint32)((m_words[word1Ndx] & (((Word)1 << high%WORD_BITS << 1) - 1)) << (high-low - high%WORD_BITS));
        }
    }

    bool isBitSet (int ndx) const
    {
        DE_ASSERT(basisu_astc::inBounds(ndx, 0, 128));
        return getBit(ndx) != 0;
    }

private:
    Word m_words[NUM_WORDS];
};

// A helper for sequential access into a Block128.
class BitAccessStream
{
public:
    BitAccessStream (const Block128& src, int startNdxInSrc, int length, bool forward)
        : m_src             (src)
        , m_startNdxInSrc   (startNdxInSrc)
        , m_length          (length)
        , m_forward         (forward)
        , m_ndx             (0)
    {
    }

    // Get the next num bits. Bits at positions greater than or equal to m_length are zeros.
    deUint32 getNext (int num)
    {
        if (num == 0 || m_ndx >= m_length)
            return 0;
        const int end               = m_ndx + num;
        const int numBitsFromSrc    = basisu_astc::max(0, basisu_astc::min(m_length, end) - m_ndx);
        const int low               = m_ndx;
        const int high              = m_ndx + numBitsFromSrc - 1;

        m_ndx += num;
        
        return m_forward ?             m_src.getBits(m_startNdxInSrc + low,  m_startNdxInSrc + high)
                         : reverseBits(m_src.getBits(m_startNdxInSrc - high, m_startNdxInSrc - low), numBitsFromSrc);
    }

private:
    const Block128&     m_src;
    const int           m_startNdxInSrc;
    const int           m_length;
    const bool          m_forward;
    int                 m_ndx;
};

struct ISEDecodedResult
{
    deUint32 m;
    deUint32 tq; //!< Trit or quint value, depending on ISE mode.
    deUint32 v;
};

// Data from an ASTC block's "block mode" part (i.e. bits [0,10]).
struct ASTCBlockMode
{
    bool        isError;
    // \note Following fields only relevant if !isError.
    bool        isVoidExtent;
    // \note Following fields only relevant if !isVoidExtent.
    bool        isDualPlane;
    int         weightGridWidth;
    int         weightGridHeight;
    ISEParams   weightISEParams;

    ASTCBlockMode (void)
        : isError           (true)
        , isVoidExtent      (true)
        , isDualPlane       (true)
        , weightGridWidth   (-1)
        , weightGridHeight  (-1)
        , weightISEParams   (ISEMODE_LAST, -1)
    {
    }
};

inline int computeNumWeights (const ASTCBlockMode& mode)
{
    return mode.weightGridWidth * mode.weightGridHeight * (mode.isDualPlane ? 2 : 1);
}

struct ColorEndpointPair
{
    UVec4 e0;
    UVec4 e1;
};

struct TexelWeightPair
{
    deUint32 w[2];
};

ASTCBlockMode getASTCBlockMode (deUint32 blockModeData)
{
    ASTCBlockMode blockMode;
    blockMode.isError = true; // \note Set to false later, if not error.
    blockMode.isVoidExtent = getBits(blockModeData, 0, 8) == 0x1fc;
    if (!blockMode.isVoidExtent)
    {
        if ((getBits(blockModeData, 0, 1) == 0 && getBits(blockModeData, 6, 8) == 7) || getBits(blockModeData, 0, 3) == 0)
            return blockMode; // Invalid ("reserved").

        deUint32 r = (deUint32)-1; // \note Set in the following branches.

        if (getBits(blockModeData, 0, 1) == 0)
        {
            const deUint32 r0   = getBit(blockModeData, 4);
            const deUint32 r1   = getBit(blockModeData, 2);
            const deUint32 r2   = getBit(blockModeData, 3);
            const deUint32 i78  = getBits(blockModeData, 7, 8);
            
            r = (r2 << 2) | (r1 << 1) | (r0 << 0);

            if (i78 == 3)
            {
                const bool i5 = isBitSet(blockModeData, 5);
                blockMode.weightGridWidth   = i5 ? 10 : 6;
                blockMode.weightGridHeight  = i5 ? 6  : 10;
            }
            else
            {
                const deUint32 a = getBits(blockModeData, 5, 6);

                switch (i78)
                {
                    case 0:     blockMode.weightGridWidth = 12;     blockMode.weightGridHeight = a + 2;                                 break;
                    case 1:     blockMode.weightGridWidth = a + 2;  blockMode.weightGridHeight = 12;                                    break;
                    case 2:     blockMode.weightGridWidth = a + 6;  blockMode.weightGridHeight = getBits(blockModeData, 9, 10) + 6;     break;
                    default: DE_ASSERT(false);
                }
            }
        }
        else
        {
            const deUint32 r0   = getBit(blockModeData, 4);
            const deUint32 r1   = getBit(blockModeData, 0);
            const deUint32 r2   = getBit(blockModeData, 1);
            const deUint32 i23  = getBits(blockModeData, 2, 3);
            const deUint32 a    = getBits(blockModeData, 5, 6);

            r = (r2 << 2) | (r1 << 1) | (r0 << 0);
            if (i23 == 3)
            {
                const deUint32  b   = getBit(blockModeData, 7);
                const bool      i8  = isBitSet(blockModeData, 8);
                blockMode.weightGridWidth   = i8 ? b+2 : a+2;
                blockMode.weightGridHeight  = i8 ? a+2 : b+6;
            }
            else
            {
                const deUint32 b = getBits(blockModeData, 7, 8);
                switch (i23)
                {
                    case 0:     blockMode.weightGridWidth = b + 4;  blockMode.weightGridHeight = a + 2; break;
                    case 1:     blockMode.weightGridWidth = b + 8;  blockMode.weightGridHeight = a + 2; break;
                    case 2:     blockMode.weightGridWidth = a + 2;  blockMode.weightGridHeight = b + 8; break;
                    default: DE_ASSERT(false);
                }
            }
        }

        const bool  zeroDH      = getBits(blockModeData, 0, 1) == 0 && getBits(blockModeData, 7, 8) == 2;
        const bool  h           = zeroDH ? 0 : isBitSet(blockModeData, 9);
        blockMode.isDualPlane   = zeroDH ? 0 : isBitSet(blockModeData, 10);

        {
            ISEMode&    m   = blockMode.weightISEParams.mode;
            int&        b   = blockMode.weightISEParams.numBits;
            m = ISEMODE_PLAIN_BIT;
            b = 0;
            if (h)
            {
                switch (r)
                {
                    case 2:                         m = ISEMODE_QUINT;  b = 1;  break;
                    case 3:     m = ISEMODE_TRIT;                       b = 2;  break;
                    case 4:                                             b = 4;  break;
                    case 5:                         m = ISEMODE_QUINT;  b = 2;  break;
                    case 6:     m = ISEMODE_TRIT;                       b = 3;  break;
                    case 7:                                             b = 5;  break;
                    default:    DE_ASSERT(false);
                }
            }
            else
            {
                switch (r)
                {
                    case 2:                                             b = 1;  break;
                    case 3:     m = ISEMODE_TRIT;                               break;
                    case 4:                                             b = 2;  break;
                    case 5:                         m = ISEMODE_QUINT;          break;
                    case 6:     m = ISEMODE_TRIT;                       b = 1;  break;
                    case 7:                                             b = 3;  break;
                    default:    DE_ASSERT(false);
                }
            }
        }
    }

    blockMode.isError = false;
    return blockMode;
}

inline void setASTCErrorColorBlock (void* dst, int blockWidth, int blockHeight, bool isSRGB)
{
    if (isSRGB)
    {
        deUint8* const dstU = (deUint8*)dst;
        for (int i = 0; i < blockWidth*blockHeight; i++)
        {
            dstU[4*i + 0] = 0xff;
            dstU[4*i + 1] = 0;
            dstU[4*i + 2] = 0xff;
            dstU[4*i + 3] = 0xff;
        }
    }
    else
    {
        float* const dstF = (float*)dst;
        for (int i = 0; i < blockWidth*blockHeight; i++)
        {
            dstF[4*i + 0] = 1.0f;
            dstF[4*i + 1] = 0.0f;
            dstF[4*i + 2] = 1.0f;
            dstF[4*i + 3] = 1.0f;
        }
    }
}

DecompressResult decodeVoidExtentBlock (void* dst, const Block128& blockData, int blockWidth, int blockHeight, bool isSRGB, bool isLDRMode)
{
    const deUint32  minSExtent          = blockData.getBits(12, 24);
    const deUint32  maxSExtent          = blockData.getBits(25, 37);
    const deUint32  minTExtent          = blockData.getBits(38, 50);
    const deUint32  maxTExtent          = blockData.getBits(51, 63);
    const bool      allExtentsAllOnes   = (minSExtent == 0x1fff) && (maxSExtent == 0x1fff) && (minTExtent == 0x1fff) && (maxTExtent == 0x1fff);
    const bool      isHDRBlock          = blockData.isBitSet(9);
    
    if ((isLDRMode && isHDRBlock) || (!allExtentsAllOnes && (minSExtent >= maxSExtent || minTExtent >= maxTExtent)))
    {
        setASTCErrorColorBlock(dst, blockWidth, blockHeight, isSRGB);
        return DECOMPRESS_RESULT_ERROR;
    }
    
    const deUint32 rgba[4] =
    {
        blockData.getBits(64,  79),
        blockData.getBits(80,  95),
        blockData.getBits(96,  111),
        blockData.getBits(112, 127)
    };

    if (isSRGB)
    {
        deUint8* const dstU = (deUint8*)dst;
        for (int i = 0; i < blockWidth * blockHeight; i++)
        {
            for (int c = 0; c < 4; c++)
                dstU[i * 4 + c] = (deUint8)((rgba[c] & 0xff00) >> 8);
        }
    }
    else
    {
        float* const dstF = (float*)dst;

        if (isHDRBlock)
        {
            for (int c = 0; c < 4; c++)
            {
                if (isFloat16InfOrNan((deFloat16)rgba[c]))
                {
                    //throw InternalError("Infinity or NaN color component in HDR void extent block in ASTC texture (behavior undefined by ASTC specification)");
                    setASTCErrorColorBlock(dst, blockWidth, blockHeight, isSRGB);
                    return DECOMPRESS_RESULT_ERROR;
                }
            }

            for (int i = 0; i < blockWidth * blockHeight; i++)
            {
                for (int c = 0; c < 4; c++)
                    dstF[i * 4 + c] = deFloat16To32((deFloat16)rgba[c]);
            }
        }
        else
        {
            for (int i = 0; i < blockWidth * blockHeight; i++)
            {
                for (int c = 0; c < 4; c++)
                    dstF[i * 4 + c] = (rgba[c] == 65535) ? 1.0f : ((float)rgba[c] / 65536.0f);
            }
        }
    }

    return DECOMPRESS_RESULT_VALID_BLOCK;
}

void decodeColorEndpointModes (deUint32* endpointModesDst, const Block128& blockData, int numPartitions, int extraCemBitsStart)
{
    if (numPartitions == 1)
        endpointModesDst[0] = blockData.getBits(13, 16);
    else
    {
        const deUint32 highLevelSelector = blockData.getBits(23, 24);

        if (highLevelSelector == 0)
        {
            const deUint32 mode = blockData.getBits(25, 28);

            for (int i = 0; i < numPartitions; i++)
                endpointModesDst[i] = mode;
        }
        else
        {
            for (int partNdx = 0; partNdx < numPartitions; partNdx++)
            {
                const deUint32 cemClass     = highLevelSelector - (blockData.isBitSet(25 + partNdx) ? 0 : 1);
                const deUint32 lowBit0Ndx   = numPartitions + 2*partNdx;
                const deUint32 lowBit1Ndx   = numPartitions + 2*partNdx + 1;
                const deUint32 lowBit0      = blockData.getBit(lowBit0Ndx < 4 ? 25+lowBit0Ndx : extraCemBitsStart+lowBit0Ndx-4);
                const deUint32 lowBit1      = blockData.getBit(lowBit1Ndx < 4 ? 25+lowBit1Ndx : extraCemBitsStart+lowBit1Ndx-4);

                endpointModesDst[partNdx] = (cemClass << 2) | (lowBit1 << 1) | lowBit0;
            }
        }
    }
}

int computeNumColorEndpointValues (const deUint32* endpointModes, int numPartitions)
{
    int result = 0;

    for (int i = 0; i < numPartitions; i++)
        result += computeNumColorEndpointValues(endpointModes[i]);

    return result;
}

void decodeISETritBlock (ISEDecodedResult* dst, int numValues, BitAccessStream& data, int numBits)
{
    DE_ASSERT(basisu_astc::inRange(numValues, 1, 5));

    deUint32 m[5];
    m[0]            = data.getNext(numBits);
    deUint32 T01    = data.getNext(2);
    m[1]            = data.getNext(numBits);
    deUint32 T23    = data.getNext(2);
    m[2]            = data.getNext(numBits);
    deUint32 T4     = data.getNext(1);
    m[3]            = data.getNext(numBits);
    deUint32 T56    = data.getNext(2);
    m[4]            = data.getNext(numBits);
    deUint32 T7     = data.getNext(1);

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wimplicit-fallthrough="            
#endif  
    switch (numValues)
    {
        // \note Fall-throughs.
        case 1: T23     = 0;
        case 2: T4      = 0;
        case 3: T56     = 0;
        case 4: T7      = 0;
        case 5: break;
        default:
            DE_ASSERT(false);
    }
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif 

    const deUint32 T = (T7 << 7) | (T56 << 5) | (T4 << 4) | (T23 << 2) | (T01 << 0);

    static const deUint32 tritsFromT[256][5] =
    {
        { 0,0,0,0,0 }, { 1,0,0,0,0 }, { 2,0,0,0,0 }, { 0,0,2,0,0 }, { 0,1,0,0,0 }, { 1,1,0,0,0 }, { 2,1,0,0,0 }, { 1,0,2,0,0 }, { 0,2,0,0,0 }, { 1,2,0,0,0 }, { 2,2,0,0,0 }, { 2,0,2,0,0 }, { 0,2,2,0,0 }, { 1,2,2,0,0 }, { 2,2,2,0,0 }, { 2,0,2,0,0 },
        { 0,0,1,0,0 }, { 1,0,1,0,0 }, { 2,0,1,0,0 }, { 0,1,2,0,0 }, { 0,1,1,0,0 }, { 1,1,1,0,0 }, { 2,1,1,0,0 }, { 1,1,2,0,0 }, { 0,2,1,0,0 }, { 1,2,1,0,0 }, { 2,2,1,0,0 }, { 2,1,2,0,0 }, { 0,0,0,2,2 }, { 1,0,0,2,2 }, { 2,0,0,2,2 }, { 0,0,2,2,2 },
        { 0,0,0,1,0 }, { 1,0,0,1,0 }, { 2,0,0,1,0 }, { 0,0,2,1,0 }, { 0,1,0,1,0 }, { 1,1,0,1,0 }, { 2,1,0,1,0 }, { 1,0,2,1,0 }, { 0,2,0,1,0 }, { 1,2,0,1,0 }, { 2,2,0,1,0 }, { 2,0,2,1,0 }, { 0,2,2,1,0 }, { 1,2,2,1,0 }, { 2,2,2,1,0 }, { 2,0,2,1,0 },
        { 0,0,1,1,0 }, { 1,0,1,1,0 }, { 2,0,1,1,0 }, { 0,1,2,1,0 }, { 0,1,1,1,0 }, { 1,1,1,1,0 }, { 2,1,1,1,0 }, { 1,1,2,1,0 }, { 0,2,1,1,0 }, { 1,2,1,1,0 }, { 2,2,1,1,0 }, { 2,1,2,1,0 }, { 0,1,0,2,2 }, { 1,1,0,2,2 }, { 2,1,0,2,2 }, { 1,0,2,2,2 },
        { 0,0,0,2,0 }, { 1,0,0,2,0 }, { 2,0,0,2,0 }, { 0,0,2,2,0 }, { 0,1,0,2,0 }, { 1,1,0,2,0 }, { 2,1,0,2,0 }, { 1,0,2,2,0 }, { 0,2,0,2,0 }, { 1,2,0,2,0 }, { 2,2,0,2,0 }, { 2,0,2,2,0 }, { 0,2,2,2,0 }, { 1,2,2,2,0 }, { 2,2,2,2,0 }, { 2,0,2,2,0 },
        { 0,0,1,2,0 }, { 1,0,1,2,0 }, { 2,0,1,2,0 }, { 0,1,2,2,0 }, { 0,1,1,2,0 }, { 1,1,1,2,0 }, { 2,1,1,2,0 }, { 1,1,2,2,0 }, { 0,2,1,2,0 }, { 1,2,1,2,0 }, { 2,2,1,2,0 }, { 2,1,2,2,0 }, { 0,2,0,2,2 }, { 1,2,0,2,2 }, { 2,2,0,2,2 }, { 2,0,2,2,2 },
        { 0,0,0,0,2 }, { 1,0,0,0,2 }, { 2,0,0,0,2 }, { 0,0,2,0,2 }, { 0,1,0,0,2 }, { 1,1,0,0,2 }, { 2,1,0,0,2 }, { 1,0,2,0,2 }, { 0,2,0,0,2 }, { 1,2,0,0,2 }, { 2,2,0,0,2 }, { 2,0,2,0,2 }, { 0,2,2,0,2 }, { 1,2,2,0,2 }, { 2,2,2,0,2 }, { 2,0,2,0,2 },
        { 0,0,1,0,2 }, { 1,0,1,0,2 }, { 2,0,1,0,2 }, { 0,1,2,0,2 }, { 0,1,1,0,2 }, { 1,1,1,0,2 }, { 2,1,1,0,2 }, { 1,1,2,0,2 }, { 0,2,1,0,2 }, { 1,2,1,0,2 }, { 2,2,1,0,2 }, { 2,1,2,0,2 }, { 0,2,2,2,2 }, { 1,2,2,2,2 }, { 2,2,2,2,2 }, { 2,0,2,2,2 },
        { 0,0,0,0,1 }, { 1,0,0,0,1 }, { 2,0,0,0,1 }, { 0,0,2,0,1 }, { 0,1,0,0,1 }, { 1,1,0,0,1 }, { 2,1,0,0,1 }, { 1,0,2,0,1 }, { 0,2,0,0,1 }, { 1,2,0,0,1 }, { 2,2,0,0,1 }, { 2,0,2,0,1 }, { 0,2,2,0,1 }, { 1,2,2,0,1 }, { 2,2,2,0,1 }, { 2,0,2,0,1 },
        { 0,0,1,0,1 }, { 1,0,1,0,1 }, { 2,0,1,0,1 }, { 0,1,2,0,1 }, { 0,1,1,0,1 }, { 1,1,1,0,1 }, { 2,1,1,0,1 }, { 1,1,2,0,1 }, { 0,2,1,0,1 }, { 1,2,1,0,1 }, { 2,2,1,0,1 }, { 2,1,2,0,1 }, { 0,0,1,2,2 }, { 1,0,1,2,2 }, { 2,0,1,2,2 }, { 0,1,2,2,2 },
        { 0,0,0,1,1 }, { 1,0,0,1,1 }, { 2,0,0,1,1 }, { 0,0,2,1,1 }, { 0,1,0,1,1 }, { 1,1,0,1,1 }, { 2,1,0,1,1 }, { 1,0,2,1,1 }, { 0,2,0,1,1 }, { 1,2,0,1,1 }, { 2,2,0,1,1 }, { 2,0,2,1,1 }, { 0,2,2,1,1 }, { 1,2,2,1,1 }, { 2,2,2,1,1 }, { 2,0,2,1,1 },
        { 0,0,1,1,1 }, { 1,0,1,1,1 }, { 2,0,1,1,1 }, { 0,1,2,1,1 }, { 0,1,1,1,1 }, { 1,1,1,1,1 }, { 2,1,1,1,1 }, { 1,1,2,1,1 }, { 0,2,1,1,1 }, { 1,2,1,1,1 }, { 2,2,1,1,1 }, { 2,1,2,1,1 }, { 0,1,1,2,2 }, { 1,1,1,2,2 }, { 2,1,1,2,2 }, { 1,1,2,2,2 },
        { 0,0,0,2,1 }, { 1,0,0,2,1 }, { 2,0,0,2,1 }, { 0,0,2,2,1 }, { 0,1,0,2,1 }, { 1,1,0,2,1 }, { 2,1,0,2,1 }, { 1,0,2,2,1 }, { 0,2,0,2,1 }, { 1,2,0,2,1 }, { 2,2,0,2,1 }, { 2,0,2,2,1 }, { 0,2,2,2,1 }, { 1,2,2,2,1 }, { 2,2,2,2,1 }, { 2,0,2,2,1 },
        { 0,0,1,2,1 }, { 1,0,1,2,1 }, { 2,0,1,2,1 }, { 0,1,2,2,1 }, { 0,1,1,2,1 }, { 1,1,1,2,1 }, { 2,1,1,2,1 }, { 1,1,2,2,1 }, { 0,2,1,2,1 }, { 1,2,1,2,1 }, { 2,2,1,2,1 }, { 2,1,2,2,1 }, { 0,2,1,2,2 }, { 1,2,1,2,2 }, { 2,2,1,2,2 }, { 2,1,2,2,2 },
        { 0,0,0,1,2 }, { 1,0,0,1,2 }, { 2,0,0,1,2 }, { 0,0,2,1,2 }, { 0,1,0,1,2 }, { 1,1,0,1,2 }, { 2,1,0,1,2 }, { 1,0,2,1,2 }, { 0,2,0,1,2 }, { 1,2,0,1,2 }, { 2,2,0,1,2 }, { 2,0,2,1,2 }, { 0,2,2,1,2 }, { 1,2,2,1,2 }, { 2,2,2,1,2 }, { 2,0,2,1,2 },
        { 0,0,1,1,2 }, { 1,0,1,1,2 }, { 2,0,1,1,2 }, { 0,1,2,1,2 }, { 0,1,1,1,2 }, { 1,1,1,1,2 }, { 2,1,1,1,2 }, { 1,1,2,1,2 }, { 0,2,1,1,2 }, { 1,2,1,1,2 }, { 2,2,1,1,2 }, { 2,1,2,1,2 }, { 0,2,2,2,2 }, { 1,2,2,2,2 }, { 2,2,2,2,2 }, { 2,1,2,2,2 }
    };

    const deUint32 (& trits)[5] = tritsFromT[T];
    for (int i = 0; i < numValues; i++)
    {
        dst[i].m    = m[i];
        dst[i].tq   = trits[i];
        dst[i].v    = (trits[i] << numBits) + m[i];
    }
}

void decodeISEQuintBlock (ISEDecodedResult* dst, int numValues, BitAccessStream& data, int numBits)
{
    DE_ASSERT(basisu_astc::inRange(numValues, 1, 3));

    deUint32 m[3];
    m[0]            = data.getNext(numBits);
    deUint32 Q012   = data.getNext(3);
    m[1]            = data.getNext(numBits);
    deUint32 Q34    = data.getNext(2);
    m[2]            = data.getNext(numBits);
    deUint32 Q56    = data.getNext(2);

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wimplicit-fallthrough="            
#endif  
    switch (numValues)
    {
        // \note Fall-throughs.
        case 1: Q34     = 0;
        case 2: Q56     = 0;
        case 3: break;
        default:
            DE_ASSERT(false);
    }
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif 

    const deUint32 Q = (Q56 << 5) | (Q34 << 3) | (Q012 << 0);

    static const deUint32 quintsFromQ[256][3] =
    {
        { 0,0,0 }, { 1,0,0 }, { 2,0,0 }, { 3,0,0 }, { 4,0,0 }, { 0,4,0 }, { 4,4,0 }, { 4,4,4 }, { 0,1,0 }, { 1,1,0 }, { 2,1,0 }, { 3,1,0 }, { 4,1,0 }, { 1,4,0 }, { 4,4,1 }, { 4,4,4 },
        { 0,2,0 }, { 1,2,0 }, { 2,2,0 }, { 3,2,0 }, { 4,2,0 }, { 2,4,0 }, { 4,4,2 }, { 4,4,4 }, { 0,3,0 }, { 1,3,0 }, { 2,3,0 }, { 3,3,0 }, { 4,3,0 }, { 3,4,0 }, { 4,4,3 }, { 4,4,4 },
        { 0,0,1 }, { 1,0,1 }, { 2,0,1 }, { 3,0,1 }, { 4,0,1 }, { 0,4,1 }, { 4,0,4 }, { 0,4,4 }, { 0,1,1 }, { 1,1,1 }, { 2,1,1 }, { 3,1,1 }, { 4,1,1 }, { 1,4,1 }, { 4,1,4 }, { 1,4,4 },
        { 0,2,1 }, { 1,2,1 }, { 2,2,1 }, { 3,2,1 }, { 4,2,1 }, { 2,4,1 }, { 4,2,4 }, { 2,4,4 }, { 0,3,1 }, { 1,3,1 }, { 2,3,1 }, { 3,3,1 }, { 4,3,1 }, { 3,4,1 }, { 4,3,4 }, { 3,4,4 },
        { 0,0,2 }, { 1,0,2 }, { 2,0,2 }, { 3,0,2 }, { 4,0,2 }, { 0,4,2 }, { 2,0,4 }, { 3,0,4 }, { 0,1,2 }, { 1,1,2 }, { 2,1,2 }, { 3,1,2 }, { 4,1,2 }, { 1,4,2 }, { 2,1,4 }, { 3,1,4 },
        { 0,2,2 }, { 1,2,2 }, { 2,2,2 }, { 3,2,2 }, { 4,2,2 }, { 2,4,2 }, { 2,2,4 }, { 3,2,4 }, { 0,3,2 }, { 1,3,2 }, { 2,3,2 }, { 3,3,2 }, { 4,3,2 }, { 3,4,2 }, { 2,3,4 }, { 3,3,4 },
        { 0,0,3 }, { 1,0,3 }, { 2,0,3 }, { 3,0,3 }, { 4,0,3 }, { 0,4,3 }, { 0,0,4 }, { 1,0,4 }, { 0,1,3 }, { 1,1,3 }, { 2,1,3 }, { 3,1,3 }, { 4,1,3 }, { 1,4,3 }, { 0,1,4 }, { 1,1,4 },
        { 0,2,3 }, { 1,2,3 }, { 2,2,3 }, { 3,2,3 }, { 4,2,3 }, { 2,4,3 }, { 0,2,4 }, { 1,2,4 }, { 0,3,3 }, { 1,3,3 }, { 2,3,3 }, { 3,3,3 }, { 4,3,3 }, { 3,4,3 }, { 0,3,4 }, { 1,3,4 }
    };

    const deUint32 (& quints)[3] = quintsFromQ[Q];
    for (int i = 0; i < numValues; i++)
    {
        dst[i].m    = m[i];
        dst[i].tq   = quints[i];
        dst[i].v    = (quints[i] << numBits) + m[i];
    }
}

inline void decodeISEBitBlock (ISEDecodedResult* dst, BitAccessStream& data, int numBits)
{
    dst[0].m = data.getNext(numBits);
    dst[0].v = dst[0].m;
}

void decodeISE (ISEDecodedResult* dst, int numValues, BitAccessStream& data, const ISEParams& params)
{
    if (params.mode == ISEMODE_TRIT)
    {
        const int numBlocks = deDivRoundUp32(numValues, 5);
        for (int blockNdx = 0; blockNdx < numBlocks; blockNdx++)
        {
            const int numValuesInBlock = blockNdx == numBlocks-1 ? numValues - 5*(numBlocks-1) : 5;
            decodeISETritBlock(&dst[5*blockNdx], numValuesInBlock, data, params.numBits);
        }
    }
    else if (params.mode == ISEMODE_QUINT)
    {
        const int numBlocks = deDivRoundUp32(numValues, 3);
        for (int blockNdx = 0; blockNdx < numBlocks; blockNdx++)
        {
            const int numValuesInBlock = blockNdx == numBlocks-1 ? numValues - 3*(numBlocks-1) : 3;
            decodeISEQuintBlock(&dst[3*blockNdx], numValuesInBlock, data, params.numBits);
        }
    }
    else
    {
        DE_ASSERT(params.mode == ISEMODE_PLAIN_BIT);
        for (int i = 0; i < numValues; i++)
            decodeISEBitBlock(&dst[i], data, params.numBits);
    }
}

void unquantizeColorEndpoints (deUint32* dst, const ISEDecodedResult* iseResults, int numEndpoints, const ISEParams& iseParams)
{
    if ((iseParams.mode == ISEMODE_TRIT) || (iseParams.mode == ISEMODE_QUINT))
    {
        const int rangeCase             = iseParams.numBits*2 - (iseParams.mode == ISEMODE_TRIT ? 2 : 1);
        DE_ASSERT(basisu_astc::inRange(rangeCase, 0, 10));

        static const deUint32   Ca[11]  = { 204, 113, 93, 54, 44, 26, 22, 13, 11, 6, 5 };
        const deUint32          C       = Ca[rangeCase];

        for (int endpointNdx = 0; endpointNdx < numEndpoints; endpointNdx++)
        {
            const deUint32 a = getBit(iseResults[endpointNdx].m, 0);
            const deUint32 b = getBit(iseResults[endpointNdx].m, 1);
            const deUint32 c = getBit(iseResults[endpointNdx].m, 2);
            const deUint32 d = getBit(iseResults[endpointNdx].m, 3);
            const deUint32 e = getBit(iseResults[endpointNdx].m, 4);
            const deUint32 f = getBit(iseResults[endpointNdx].m, 5);
            const deUint32 A = (a == 0) ? 0 : (1<<9)-1;

            const deUint32 B = (rangeCase == 0)   ? 0
                             : (rangeCase == 1)   ? 0
                             : (rangeCase == 2)   ? ((b << 8) | (b << 4) | (b << 2) | (b << 1))
                             : (rangeCase == 3)   ? ((b << 8) | (b << 3) | (b << 2))
                             : (rangeCase == 4)   ? ((c << 8) | (b << 7) | (c << 3) | (b << 2) | (c << 1) | (b << 0))
                             : (rangeCase == 5)   ? ((c << 8) | (b << 7) | (c << 2) | (b << 1) | (c << 0))
                             : (rangeCase == 6)   ? ((d << 8) | (c << 7) | (b << 6) | (d << 2) | (c << 1) | (b << 0))
                             : (rangeCase == 7)   ? ((d << 8) | (c << 7) | (b << 6) | (d << 1) | (c << 0))
                             : (rangeCase == 8)   ? ((e << 8) | (d << 7) | (c << 6) | (b << 5) | (e << 1) | (d << 0))
                             : (rangeCase == 9)   ? ((e << 8) | (d << 7) | (c << 6) | (b << 5) | (e << 0))
                             : (rangeCase == 10)  ? ((f << 8) | (e << 7) | (d << 6) | (c << 5) | (b << 4) | (f << 0))
                             : (deUint32)-1;

            DE_ASSERT(B != (deUint32)-1);
            dst[endpointNdx] = (((iseResults[endpointNdx].tq*C + B) ^ A) >> 2) | (A & 0x80);
        }
    }
    else
    {
        DE_ASSERT(iseParams.mode == ISEMODE_PLAIN_BIT);
        for (int endpointNdx = 0; endpointNdx < numEndpoints; endpointNdx++)
            dst[endpointNdx] = bitReplicationScale(iseResults[endpointNdx].v, iseParams.numBits, 8);
    }
}

inline void bitTransferSigned (deInt32& a, deInt32& b)
{
    b >>= 1;
    b |= a & 0x80;
    a >>= 1;
    a &= 0x3f;
    if (isBitSet(a, 5))
        a -= 0x40;
}

inline UVec4 clampedRGBA (const IVec4& rgba)
{
    return UVec4(basisu_astc::clamp(rgba.x(), 0, 0xff),
        basisu_astc::clamp(rgba.y(), 0, 0xff),
        basisu_astc::clamp(rgba.z(), 0, 0xff),
        basisu_astc::clamp(rgba.w(), 0, 0xff));
}

inline IVec4 blueContract (int r, int g, int b, int a)
{
    return IVec4((r+b)>>1, (g+b)>>1, b, a);
}

inline bool isColorEndpointModeHDR (deUint32 mode)
{
    return (mode == 2)    ||
           (mode == 3)    ||
           (mode == 7)    ||
           (mode == 11)   ||
           (mode == 14)   ||
           (mode == 15);
}

void decodeHDREndpointMode7 (UVec4& e0, UVec4& e1, deUint32 v0, deUint32 v1, deUint32 v2, deUint32 v3)
{
    const deUint32 m10      = getBit(v1, 7) | (getBit(v2, 7) << 1);
    const deUint32 m23      = getBits(v0, 6, 7);

    const deUint32 majComp  = (m10 != 3)  ? m10
                            : (m23 != 3)  ? m23
                            :             0;
    
    const deUint32 mode     = (m10 != 3)  ? m23
                            : (m23 != 3)  ? 4
                            :             5;

    deInt32         red     = (deInt32)getBits(v0, 0, 5);
    deInt32         green   = (deInt32)getBits(v1, 0, 4);
    deInt32         blue    = (deInt32)getBits(v2, 0, 4);
    deInt32         scale   = (deInt32)getBits(v3, 0, 4);

    {
#define SHOR(DST_VAR, SHIFT, BIT_VAR) (DST_VAR) |= (BIT_VAR) << (SHIFT)
#define ASSIGN_X_BITS(V0,S0, V1,S1, V2,S2, V3,S3, V4,S4, V5,S5, V6,S6) do { SHOR(V0,S0,x0); SHOR(V1,S1,x1); SHOR(V2,S2,x2); SHOR(V3,S3,x3); SHOR(V4,S4,x4); SHOR(V5,S5,x5); SHOR(V6,S6,x6); } while (false)

        const deUint32  x0  = getBit(v1, 6);
        const deUint32  x1  = getBit(v1, 5);
        const deUint32  x2  = getBit(v2, 6);
        const deUint32  x3  = getBit(v2, 5);
        const deUint32  x4  = getBit(v3, 7);
        const deUint32  x5  = getBit(v3, 6);
        const deUint32  x6  = getBit(v3, 5);

        deInt32&        R   = red;
        deInt32&        G   = green;
        deInt32&        B   = blue;
        deInt32&        S   = scale;

        switch (mode)
        {
            case 0: ASSIGN_X_BITS(R,9,  R,8,  R,7,  R,10,  R,6,  S,6,   S,5); break;
            case 1: ASSIGN_X_BITS(R,8,  G,5,  R,7,  B,5,   R,6,  R,10,  R,9); break;
            case 2: ASSIGN_X_BITS(R,9,  R,8,  R,7,  R,6,   S,7,  S,6,   S,5); break;
            case 3: ASSIGN_X_BITS(R,8,  G,5,  R,7,  B,5,   R,6,  S,6,   S,5); break;
            case 4: ASSIGN_X_BITS(G,6,  G,5,  B,6,  B,5,   R,6,  R,7,   S,5); break;
            case 5: ASSIGN_X_BITS(G,6,  G,5,  B,6,  B,5,   R,6,  S,6,   S,5); break;
            default:
                DE_ASSERT(false);
        }
#undef ASSIGN_X_BITS
#undef SHOR
    }

    static const int shiftAmounts[] = { 1, 1, 2, 3, 4, 5 };
    DE_ASSERT(mode < DE_LENGTH_OF_ARRAY(shiftAmounts));

    red     <<= shiftAmounts[mode];
    green   <<= shiftAmounts[mode];
    blue    <<= shiftAmounts[mode];
    scale   <<= shiftAmounts[mode];

    if (mode != 5)
    {
        green   = red - green;
        blue    = red - blue;
    }

    if (majComp == 1)
        std::swap(red, green);
    else if (majComp == 2)
        std::swap(red, blue);

    e0 = UVec4(basisu_astc::clamp(red   - scale,    0, 0xfff),
        basisu_astc::clamp(green    - scale,    0, 0xfff),
        basisu_astc::clamp(blue - scale,    0, 0xfff),
               0x780);

    e1 = UVec4(basisu_astc::clamp(red,              0, 0xfff),
        basisu_astc::clamp(green,               0, 0xfff),
        basisu_astc::clamp(blue,                0, 0xfff),
               0x780);
}

void decodeHDREndpointMode11 (UVec4& e0, UVec4& e1, deUint32 v0, deUint32 v1, deUint32 v2, deUint32 v3, deUint32 v4, deUint32 v5)
{
    const deUint32 major = (getBit(v5, 7) << 1) | getBit(v4, 7);

    if (major == 3)
    {
        e0 = UVec4(v0<<4, v2<<4, getBits(v4,0,6)<<5, 0x780);
        e1 = UVec4(v1<<4, v3<<4, getBits(v5,0,6)<<5, 0x780);
    }
    else
    {
        const deUint32 mode = (getBit(v3, 7) << 2) | (getBit(v2, 7) << 1) | getBit(v1, 7);

        deInt32 a   = (deInt32)((getBit(v1, 6) << 8) | v0);
        deInt32 c   = (deInt32)(getBits(v1, 0, 5));
        deInt32 b0  = (deInt32)(getBits(v2, 0, 5));
        deInt32 b1  = (deInt32)(getBits(v3, 0, 5));
        deInt32 d0  = (deInt32)(getBits(v4, 0, 4));
        deInt32 d1  = (deInt32)(getBits(v5, 0, 4));

        {
#define SHOR(DST_VAR, SHIFT, BIT_VAR) (DST_VAR) |= (BIT_VAR) << (SHIFT)
#define ASSIGN_X_BITS(V0,S0, V1,S1, V2,S2, V3,S3, V4,S4, V5,S5) do { SHOR(V0,S0,x0); SHOR(V1,S1,x1); SHOR(V2,S2,x2); SHOR(V3,S3,x3); SHOR(V4,S4,x4); SHOR(V5,S5,x5); } while (false)
            const deUint32 x0 = getBit(v2, 6);
            const deUint32 x1 = getBit(v3, 6);
            const deUint32 x2 = getBit(v4, 6);
            const deUint32 x3 = getBit(v5, 6);
            const deUint32 x4 = getBit(v4, 5);
            const deUint32 x5 = getBit(v5, 5);

            switch (mode)
            {
                case 0: ASSIGN_X_BITS(b0,6,  b1,6,   d0,6,  d1,6,  d0,5,  d1,5); break;
                case 1: ASSIGN_X_BITS(b0,6,  b1,6,   b0,7,  b1,7,  d0,5,  d1,5); break;
                case 2: ASSIGN_X_BITS(a,9,   c,6,    d0,6,  d1,6,  d0,5,  d1,5); break;
                case 3: ASSIGN_X_BITS(b0,6,  b1,6,   a,9,   c,6,   d0,5,  d1,5); break;
                case 4: ASSIGN_X_BITS(b0,6,  b1,6,   b0,7,  b1,7,  a,9,   a,10); break;
                case 5: ASSIGN_X_BITS(a,9,   a,10,   c,7,   c,6,   d0,5,  d1,5); break;
                case 6: ASSIGN_X_BITS(b0,6,  b1,6,   a,11,  c,6,   a,9,   a,10); break;
                case 7: ASSIGN_X_BITS(a,9,   a,10,   a,11,  c,6,   d0,5,  d1,5); break;
                default:
                    DE_ASSERT(false);
            }
#undef ASSIGN_X_BITS
#undef SHOR
        }

        static const int numDBits[] = { 7, 6, 7, 6, 5, 6, 5, 6 };
        DE_ASSERT(mode < DE_LENGTH_OF_ARRAY(numDBits));
        d0 = signExtend(d0, numDBits[mode]);
        d1 = signExtend(d1, numDBits[mode]);
        
        const int shiftAmount = (mode >> 1) ^ 3;
        a   = (uint32_t)a  << shiftAmount;
        c   = (uint32_t)c  << shiftAmount;
        b0  = (uint32_t)b0 << shiftAmount;
        b1  = (uint32_t)b1 << shiftAmount;
        d0  = (uint32_t)d0 << shiftAmount;
        d1  = (uint32_t)d1 << shiftAmount;

        e0 = UVec4(basisu_astc::clamp(a-c, 0, 0xfff), basisu_astc::clamp(a-b0-c-d0, 0, 0xfff), basisu_astc::clamp(a-b1-c-d1, 0, 0xfff), 0x780);
        e1 = UVec4(basisu_astc::clamp(a, 0, 0xfff), basisu_astc::clamp(a-b0, 0, 0xfff), basisu_astc::clamp(a-b1, 0, 0xfff), 0x780);

        if (major == 1)
        {
            std::swap(e0.x(), e0.y());
            std::swap(e1.x(), e1.y());
        }
        else if (major == 2)
        {
            std::swap(e0.x(), e0.z());
            std::swap(e1.x(), e1.z());
        }
    }
}

void decodeHDREndpointMode15(UVec4& e0, UVec4& e1, deUint32 v0, deUint32 v1, deUint32 v2, deUint32 v3, deUint32 v4, deUint32 v5, deUint32 v6In, deUint32 v7In)
{
    decodeHDREndpointMode11(e0, e1, v0, v1, v2, v3, v4, v5);
    
    const deUint32  mode    = (getBit(v7In, 7) << 1) | getBit(v6In, 7);
    deInt32         v6      = (deInt32)getBits(v6In, 0, 6);
    deInt32         v7      = (deInt32)getBits(v7In, 0, 6);

    if (mode == 3)
    {
        e0.w() = v6 << 5;
        e1.w() = v7 << 5;
    }
    else
    {
        v6 |= (v7 << (mode+1)) & 0x780;
        v7 &= (0x3f >> mode);
        v7 ^= 0x20 >> mode;
        v7 -= 0x20 >> mode;
        v6 <<= 4-mode;
        v7 <<= 4-mode;
        v7 += v6;
        v7 = basisu_astc::clamp(v7, 0, 0xfff);
        e0.w() = v6;
        e1.w() = v7;
    }
}

void decodeColorEndpoints (ColorEndpointPair* dst, const deUint32* unquantizedEndpoints, const deUint32* endpointModes, int numPartitions)
{
    int unquantizedNdx = 0;

    for (int partitionNdx = 0; partitionNdx < numPartitions; partitionNdx++)
    {
        const deUint32      endpointMode    = endpointModes[partitionNdx];
        const deUint32*     v               = &unquantizedEndpoints[unquantizedNdx];

        UVec4&              e0              = dst[partitionNdx].e0;
        UVec4&              e1              = dst[partitionNdx].e1;
        unquantizedNdx += computeNumColorEndpointValues(endpointMode);

        switch (endpointMode)
        {
            case 0:
            {
                e0 = UVec4(v[0], v[0], v[0], 0xff);
                e1 = UVec4(v[1], v[1], v[1], 0xff);
                break;
            }
            case 1:
            {
                const deUint32 L0 = (v[0] >> 2) | (getBits(v[1], 6, 7) << 6);
                const deUint32 L1 = basisu_astc::min(0xffu, L0 + getBits(v[1], 0, 5));
                e0 = UVec4(L0, L0, L0, 0xff);
                e1 = UVec4(L1, L1, L1, 0xff);
                break;
            }
            case 2:
            {
                const deUint32 v1Gr     = v[1] >= v[0];
                const deUint32 y0       = v1Gr ? v[0]<<4 : (v[1]<<4) + 8;
                const deUint32 y1       = v1Gr ? v[1]<<4 : (v[0]<<4) - 8;
                e0 = UVec4(y0, y0, y0, 0x780);
                e1 = UVec4(y1, y1, y1, 0x780);
                break;
            }
            case 3:
            {
                const bool      m   = isBitSet(v[0], 7);
                const deUint32  y0  = m ? (getBits(v[1], 5, 7) << 9) | (getBits(v[0], 0, 6) << 2)
                                        : (getBits(v[1], 4, 7) << 8) | (getBits(v[0], 0, 6) << 1);
                const deUint32  d   = m ? getBits(v[1], 0, 4) << 2
                                        : getBits(v[1], 0, 3) << 1;
                const deUint32  y1  = basisu_astc::min(0xfffu, y0+d);
                e0 = UVec4(y0, y0, y0, 0x780);
                e1 = UVec4(y1, y1, y1, 0x780);
                break;
            }
            case 4:
            {
                e0 = UVec4(v[0], v[0], v[0], v[2]);
                e1 = UVec4(v[1], v[1], v[1], v[3]);
                break;
            }
            case 5:
            {
                deInt32 v0 = (deInt32)v[0];
                deInt32 v1 = (deInt32)v[1];
                deInt32 v2 = (deInt32)v[2];
                deInt32 v3 = (deInt32)v[3];
                bitTransferSigned(v1, v0);
                bitTransferSigned(v3, v2);
                e0 = clampedRGBA(IVec4(v0,      v0,     v0,     v2));
                e1 = clampedRGBA(IVec4(v0+v1,   v0+v1,  v0+v1,  v2+v3));
                break;
            }
            case 6:
                e0 = UVec4((v[0]*v[3]) >> 8,    (v[1]*v[3]) >> 8,   (v[2]*v[3]) >> 8,   0xff);
                e1 = UVec4(v[0],                v[1],               v[2],               0xff);
                break;
            case 7:
                decodeHDREndpointMode7(e0, e1, v[0], v[1], v[2], v[3]);
                break;
            case 8:
            {
                if (v[1]+v[3]+v[5] >= v[0]+v[2]+v[4])
                {
                    e0 = UVec4(v[0], v[2], v[4], 0xff);
                    e1 = UVec4(v[1], v[3], v[5], 0xff);
                }
                else
                {
                    e0 = blueContract(v[1], v[3], v[5], 0xff).asUint();
                    e1 = blueContract(v[0], v[2], v[4], 0xff).asUint();
                }
                break;
            }
            case 9:
            {
                deInt32 v0 = (deInt32)v[0];
                deInt32 v1 = (deInt32)v[1];
                deInt32 v2 = (deInt32)v[2];
                deInt32 v3 = (deInt32)v[3];
                deInt32 v4 = (deInt32)v[4];
                deInt32 v5 = (deInt32)v[5];
                bitTransferSigned(v1, v0);
                bitTransferSigned(v3, v2);
                bitTransferSigned(v5, v4);
                if (v1+v3+v5 >= 0)
                {
                    e0 = clampedRGBA(IVec4(v0,      v2,     v4,     0xff));
                    e1 = clampedRGBA(IVec4(v0+v1,   v2+v3,  v4+v5,  0xff));
                }
                else
                {
                    e0 = clampedRGBA(blueContract(v0+v1,    v2+v3,  v4+v5,  0xff));
                    e1 = clampedRGBA(blueContract(v0,       v2,     v4,     0xff));
                }
                break;
            }
            case 10:
            {
                e0 = UVec4((v[0]*v[3]) >> 8,    (v[1]*v[3]) >> 8,   (v[2]*v[3]) >> 8,   v[4]);
                e1 = UVec4(v[0],                v[1],               v[2],               v[5]);
                break;
            }
            case 11:
            {
                decodeHDREndpointMode11(e0, e1, v[0], v[1], v[2], v[3], v[4], v[5]);
                break;
            }
            case 12:
            {
                if (v[1] + v[3] + v[5] >= v[0] + v[2] + v[4])
                {
                    e0 = UVec4(v[0], v[2], v[4], v[6]);
                    e1 = UVec4(v[1], v[3], v[5], v[7]);
                }
                else
                {
                    e0 = clampedRGBA(blueContract(v[1], v[3], v[5], v[7]));
                    e1 = clampedRGBA(blueContract(v[0], v[2], v[4], v[6]));
                }
                break;
            }
            case 13:
            {
                deInt32 v0 = (deInt32)v[0];
                deInt32 v1 = (deInt32)v[1];
                deInt32 v2 = (deInt32)v[2];
                deInt32 v3 = (deInt32)v[3];
                deInt32 v4 = (deInt32)v[4];
                deInt32 v5 = (deInt32)v[5];
                deInt32 v6 = (deInt32)v[6];
                deInt32 v7 = (deInt32)v[7];
                bitTransferSigned(v1, v0);
                bitTransferSigned(v3, v2);
                bitTransferSigned(v5, v4);
                bitTransferSigned(v7, v6);
                if (v1+v3+v5 >= 0)
                {
                    e0 = clampedRGBA(IVec4(v0,      v2,     v4,     v6));
                    e1 = clampedRGBA(IVec4(v0+v1,   v2+v3,  v4+v5,  v6+v7));
                }
                else
                {
                    e0 = clampedRGBA(blueContract(v0+v1,    v2+v3,  v4+v5,  v6+v7));
                    e1 = clampedRGBA(blueContract(v0,       v2,     v4,     v6));
                }
                break;
            }
            case 14:
                decodeHDREndpointMode11(e0, e1, v[0], v[1], v[2], v[3], v[4], v[5]);
                e0.w() = v[6];
                e1.w() = v[7];
                break;
            case 15:
            {
                decodeHDREndpointMode15(e0, e1, v[0], v[1], v[2], v[3], v[4], v[5], v[6], v[7]);
                break;
            }
            default:
                DE_ASSERT(false);
        }
    }
}

void computeColorEndpoints (ColorEndpointPair* dst, const Block128& blockData, const deUint32* endpointModes, int numPartitions, int numColorEndpointValues, const ISEParams& iseParams, int numBitsAvailable)
{
    const int           colorEndpointDataStart = (numPartitions == 1) ? 17 : 29;
    ISEDecodedResult    colorEndpointData[18];
    
    {
        BitAccessStream dataStream(blockData, colorEndpointDataStart, numBitsAvailable, true);
        decodeISE(&colorEndpointData[0], numColorEndpointValues, dataStream, iseParams);
    }

    {
        deUint32 unquantizedEndpoints[18];
        unquantizeColorEndpoints(&unquantizedEndpoints[0], &colorEndpointData[0], numColorEndpointValues, iseParams);
        decodeColorEndpoints(dst, &unquantizedEndpoints[0], &endpointModes[0], numPartitions);
    }
}

void unquantizeWeights (deUint32 dst[64], const ISEDecodedResult* weightGrid, const ASTCBlockMode& blockMode)
{
    const int           numWeights  = computeNumWeights(blockMode);
    const ISEParams&    iseParams   = blockMode.weightISEParams;

    if ((iseParams.mode == ISEMODE_TRIT) || (iseParams.mode == ISEMODE_QUINT))
    {
        const int rangeCase = iseParams.numBits*2 + (iseParams.mode == ISEMODE_QUINT ? 1 : 0);

        if ((rangeCase == 0) || (rangeCase == 1))
        {
            static const deUint32 map0[3]   = { 0, 32, 63 };
            static const deUint32 map1[5]   = { 0, 16, 32, 47, 63 };
            const deUint32* const map = (rangeCase == 0) ? &map0[0] : &map1[0];

            for (int i = 0; i < numWeights; i++)
            {
                DE_ASSERT(weightGrid[i].v < (rangeCase == 0 ? 3u : 5u));
                dst[i] = map[weightGrid[i].v];
            }
        }
        else
        {
            DE_ASSERT(rangeCase <= 6);
            static const deUint32   Ca[5]   = { 50, 28, 23, 13, 11 };
            const deUint32          C       = Ca[rangeCase-2];

            for (int weightNdx = 0; weightNdx < numWeights; weightNdx++)
            {
                const deUint32 a = getBit(weightGrid[weightNdx].m, 0);
                const deUint32 b = getBit(weightGrid[weightNdx].m, 1);
                const deUint32 c = getBit(weightGrid[weightNdx].m, 2);
                
                const deUint32 A = (a == 0) ? 0 : (1<<7)-1;
                const deUint32 B = (rangeCase == 2) ? 0
                                 : (rangeCase == 3) ? 0
                                 : (rangeCase == 4) ? (b << 6) | (b << 2) | (b << 0)
                                 : (rangeCase == 5) ? (b << 6) | (b << 1)
                                 : (rangeCase == 6) ? (c << 6) | (b << 5) | (c << 1) |  (b << 0)
                                 : (deUint32)-1;

                dst[weightNdx] = (((weightGrid[weightNdx].tq*C + B) ^ A) >> 2) | (A & 0x20);
            }
        }
    }
    else
    {
        DE_ASSERT(iseParams.mode == ISEMODE_PLAIN_BIT);
        for (int weightNdx = 0; weightNdx < numWeights; weightNdx++)
            dst[weightNdx] = bitReplicationScale(weightGrid[weightNdx].v, iseParams.numBits, 6);
    }

    for (int weightNdx = 0; weightNdx < numWeights; weightNdx++)
        dst[weightNdx] += dst[weightNdx] > 32 ? 1 : 0;

    // Initialize nonexistent weights to poison values
    for (int weightNdx = numWeights; weightNdx < 64; weightNdx++)
        dst[weightNdx] = ~0u;
}

void interpolateWeights (TexelWeightPair* dst, const deUint32 (&unquantizedWeights) [64], int blockWidth, int blockHeight, const ASTCBlockMode& blockMode)
{
    const int       numWeightsPerTexel  = blockMode.isDualPlane ? 2 : 1;
    const deUint32  scaleX              = (1024 + blockWidth/2) / (blockWidth-1);
    const deUint32  scaleY              = (1024 + blockHeight/2) / (blockHeight-1);
    DE_ASSERT(blockMode.weightGridWidth*blockMode.weightGridHeight*numWeightsPerTexel <= (int)DE_LENGTH_OF_ARRAY(unquantizedWeights));

    for (int texelY = 0; texelY < blockHeight; texelY++)
    {
        for (int texelX = 0; texelX < blockWidth; texelX++)
        {
            const deUint32 gX   = (scaleX*texelX*(blockMode.weightGridWidth-1) + 32) >> 6;
            const deUint32 gY   = (scaleY*texelY*(blockMode.weightGridHeight-1) + 32) >> 6;
            const deUint32 jX   = gX >> 4;
            const deUint32 jY   = gY >> 4;
            const deUint32 fX   = gX & 0xf;
            const deUint32 fY   = gY & 0xf;
            const deUint32 w11  = (fX*fY + 8) >> 4;
            const deUint32 w10  = fY - w11;
            const deUint32 w01  = fX - w11;
            const deUint32 w00  = 16 - fX - fY + w11;
            const deUint32 i00  = jY*blockMode.weightGridWidth + jX;
            const deUint32 i01  = i00 + 1;
            const deUint32 i10  = i00 + blockMode.weightGridWidth;
            const deUint32 i11  = i00 + blockMode.weightGridWidth + 1;
            
            // These addresses can be out of bounds, but respective weights will be 0 then.
            DE_ASSERT(deInBounds32(i00, 0, blockMode.weightGridWidth*blockMode.weightGridHeight) || w00 == 0);
            DE_ASSERT(deInBounds32(i01, 0, blockMode.weightGridWidth*blockMode.weightGridHeight) || w01 == 0);
            DE_ASSERT(deInBounds32(i10, 0, blockMode.weightGridWidth*blockMode.weightGridHeight) || w10 == 0);
            DE_ASSERT(deInBounds32(i11, 0, blockMode.weightGridWidth*blockMode.weightGridHeight) || w11 == 0);

            for (int texelWeightNdx = 0; texelWeightNdx < numWeightsPerTexel; texelWeightNdx++)
            {
                // & 0x3f clamps address to bounds of unquantizedWeights
                const deUint32 p00  = unquantizedWeights[(i00 * numWeightsPerTexel + texelWeightNdx) & 0x3f];
                const deUint32 p01  = unquantizedWeights[(i01 * numWeightsPerTexel + texelWeightNdx) & 0x3f];
                const deUint32 p10  = unquantizedWeights[(i10 * numWeightsPerTexel + texelWeightNdx) & 0x3f];
                const deUint32 p11  = unquantizedWeights[(i11 * numWeightsPerTexel + texelWeightNdx) & 0x3f];
                                
                dst[texelY*blockWidth + texelX].w[texelWeightNdx] = (p00*w00 + p01*w01 + p10*w10 + p11*w11 + 8) >> 4;
            }
        }
    }
}

void computeTexelWeights (TexelWeightPair* dst, const Block128& blockData, int blockWidth, int blockHeight, const ASTCBlockMode& blockMode)
{
    ISEDecodedResult weightGrid[64];

    {
        BitAccessStream dataStream(blockData, 127, computeNumRequiredBits(blockMode.weightISEParams, computeNumWeights(blockMode)), false);
        decodeISE(&weightGrid[0], computeNumWeights(blockMode), dataStream, blockMode.weightISEParams);
    }

    {
        deUint32 unquantizedWeights[64];
        unquantizeWeights(&unquantizedWeights[0], &weightGrid[0], blockMode);

        interpolateWeights(dst, unquantizedWeights, blockWidth, blockHeight, blockMode);
    }
}

inline deUint32 hash52 (deUint32 v)
{
    deUint32 p = v;
    p ^= p >> 15;   p -= p << 17;   p += p << 7;    p += p << 4;
    p ^= p >>  5;   p += p << 16;   p ^= p >> 7;    p ^= p >> 3;
    p ^= p <<  6;   p ^= p >> 17;
    return p;
}

int computeTexelPartition (deUint32 seedIn, deUint32 xIn, deUint32 yIn, deUint32 zIn, int numPartitions, bool smallBlock)
{
    DE_ASSERT(zIn == 0);

    const deUint32  x       = smallBlock ? xIn << 1 : xIn;
    const deUint32  y       = smallBlock ? yIn << 1 : yIn;
    const deUint32  z       = smallBlock ? zIn << 1 : zIn;
    const deUint32  seed    = seedIn + 1024*(numPartitions-1);
    const deUint32  rnum    = hash52(seed);

    deUint8         seed1   = (deUint8)( rnum                           & 0xf);
    deUint8         seed2   = (deUint8)((rnum >>  4)                    & 0xf);
    deUint8         seed3   = (deUint8)((rnum >>  8)                    & 0xf);
    deUint8         seed4   = (deUint8)((rnum >> 12)                    & 0xf);
    deUint8         seed5   = (deUint8)((rnum >> 16)                    & 0xf);
    deUint8         seed6   = (deUint8)((rnum >> 20)                    & 0xf);
    deUint8         seed7   = (deUint8)((rnum >> 24)                    & 0xf);
    deUint8         seed8   = (deUint8)((rnum >> 28)                    & 0xf);
    deUint8         seed9   = (deUint8)((rnum >> 18)                    & 0xf);
    deUint8         seed10  = (deUint8)((rnum >> 22)                    & 0xf);
    deUint8         seed11  = (deUint8)((rnum >> 26)                    & 0xf);
    deUint8         seed12  = (deUint8)(((rnum >> 30) | (rnum << 2))    & 0xf);

    seed1  = (deUint8)(seed1  * seed1 );
    seed2  = (deUint8)(seed2  * seed2 );
    seed3  = (deUint8)(seed3  * seed3 );
    seed4  = (deUint8)(seed4  * seed4 );
    seed5  = (deUint8)(seed5  * seed5 );
    seed6  = (deUint8)(seed6  * seed6 );
    seed7  = (deUint8)(seed7  * seed7 );
    seed8  = (deUint8)(seed8  * seed8 );
    seed9  = (deUint8)(seed9  * seed9 );
    seed10 = (deUint8)(seed10 * seed10);
    seed11 = (deUint8)(seed11 * seed11);
    seed12 = (deUint8)(seed12 * seed12);

    const int shA = (seed & 2) != 0     ? 4     : 5;
    const int shB = numPartitions == 3  ? 6     : 5;
    const int sh1 = (seed & 1) != 0     ? shA   : shB;
    const int sh2 = (seed & 1) != 0     ? shB   : shA;
    const int sh3 = (seed & 0x10) != 0  ? sh1   : sh2;

    seed1  = (deUint8)(seed1  >> sh1);
    seed2  = (deUint8)(seed2  >> sh2);
    seed3  = (deUint8)(seed3  >> sh1);
    seed4  = (deUint8)(seed4  >> sh2);
    seed5  = (deUint8)(seed5  >> sh1);
    seed6  = (deUint8)(seed6  >> sh2);
    seed7  = (deUint8)(seed7  >> sh1);
    seed8  = (deUint8)(seed8  >> sh2);
    seed9  = (deUint8)(seed9  >> sh3);
    seed10 = (deUint8)(seed10 >> sh3);
    seed11 = (deUint8)(seed11 >> sh3);
    seed12 = (deUint8)(seed12 >> sh3);

    const int a =                         0x3f & (seed1*x + seed2*y + seed11*z + (rnum >> 14));
    const int b =                         0x3f & (seed3*x + seed4*y + seed12*z + (rnum >> 10));
    const int c = (numPartitions >= 3) ?  0x3f & (seed5*x + seed6*y + seed9*z  + (rnum >>  6))    : 0;
    const int d = (numPartitions >= 4) ?  0x3f & (seed7*x + seed8*y + seed10*z + (rnum >>  2))    : 0;

    return (a >= b && a >= c && a >= d) ? 0
         : (b >= c && b >= d)           ? 1
         : (c >= d)                     ? 2
         :                                3;
}

DecompressResult setTexelColors (void* dst, ColorEndpointPair* colorEndpoints, TexelWeightPair* texelWeights, int ccs, deUint32 partitionIndexSeed,
                                 int numPartitions, int blockWidth, int blockHeight, bool isSRGB, bool isLDRMode, const deUint32* colorEndpointModes)
{
    const bool          smallBlock  = blockWidth*blockHeight < 31;
    DecompressResult    result      = DECOMPRESS_RESULT_VALID_BLOCK;
    bool                isHDREndpoint[4];

    for (int i = 0; i < numPartitions; i++)
    {
        isHDREndpoint[i] = isColorEndpointModeHDR(colorEndpointModes[i]);
    }

    for (int texelY = 0; texelY < blockHeight; texelY++)
    {
        for (int texelX = 0; texelX < blockWidth; texelX++)
        {
            const int texelNdx = texelY * blockWidth + texelX;
            const int colorEndpointNdx = (numPartitions == 1) ? 0 : computeTexelPartition(partitionIndexSeed, texelX, texelY, 0, numPartitions, smallBlock);

            DE_ASSERT(colorEndpointNdx < numPartitions);
            const UVec4& e0 = colorEndpoints[colorEndpointNdx].e0;
            const UVec4& e1 = colorEndpoints[colorEndpointNdx].e1;
            const TexelWeightPair& weight = texelWeights[texelNdx];

            if (isLDRMode && isHDREndpoint[colorEndpointNdx])
            {
                if (isSRGB)
                {
                    ((deUint8*)dst)[texelNdx * 4 + 0] = 0xff;
                    ((deUint8*)dst)[texelNdx * 4 + 1] = 0;
                    ((deUint8*)dst)[texelNdx * 4 + 2] = 0xff;
                    ((deUint8*)dst)[texelNdx * 4 + 3] = 0xff;
                }
                else
                {
                    ((float*)dst)[texelNdx * 4 + 0] = 1.0f;
                    ((float*)dst)[texelNdx * 4 + 1] = 0;
                    ((float*)dst)[texelNdx * 4 + 2] = 1.0f;
                    ((float*)dst)[texelNdx * 4 + 3] = 1.0f;
                }
                result = DECOMPRESS_RESULT_ERROR;
            }
            else
            {
                for (int channelNdx = 0; channelNdx < 4; channelNdx++)
                {
                    if (!isHDREndpoint[colorEndpointNdx] || (channelNdx == 3 && colorEndpointModes[colorEndpointNdx] == 14)) // \note Alpha for mode 14 is treated the same as LDR.
                    {
                        const deUint32 c0 = (e0[channelNdx] << 8) | (isSRGB ? 0x80 : e0[channelNdx]);
                        const deUint32 c1 = (e1[channelNdx] << 8) | (isSRGB ? 0x80 : e1[channelNdx]);
                        const deUint32 w = weight.w[ccs == channelNdx ? 1 : 0];
                        const deUint32 c = (c0 * (64 - w) + c1 * w + 32) / 64;

                        if (isSRGB)
                            ((deUint8*)dst)[texelNdx * 4 + channelNdx] = (deUint8)((c & 0xff00) >> 8);
                        else
                            ((float*)dst)[texelNdx * 4 + channelNdx] = (c == 65535) ? 1.0f : (float)c / 65536.0f;
                    }
                    else
                    {
                        DE_ASSERT(!isSRGB);
                        //DE_STATIC_ASSERT((basisu_astc::meta::TypesSame<deFloat16, deUint16>::Value));

                        const deUint32      c0 = e0[channelNdx] << 4;
                        const deUint32      c1 = e1[channelNdx] << 4;
                        const deUint32      w = weight.w[(ccs == channelNdx) ? 1 : 0];
                        const deUint32      c = (c0 * (64 - w) + c1 * w + 32) / 64;
                        const deUint32      e = getBits(c, 11, 15);
                        const deUint32      m = getBits(c, 0, 10);
                        const deUint32      mt = (m < 512) ? (3 * m)
                            : (m >= 1536) ? (5 * m - 2048)
                            : (4 * m - 512);

                        const deFloat16     cf = (deFloat16)((e << 10) + (mt >> 3));

                        ((float*)dst)[texelNdx * 4 + channelNdx] = deFloat16To32(isFloat16InfOrNan(cf) ? 0x7bff : cf);
                    }
                
                } // channelNdx
            }
        } // texelX
    } // texelY

    return result;
}

DecompressResult decompressBlock (void* dst, const Block128& blockData, int blockWidth, int blockHeight, bool isSRGB, bool isLDR)
{
    DE_ASSERT(isLDR || !isSRGB);
    
    // Decode block mode.
    const ASTCBlockMode blockMode = getASTCBlockMode(blockData.getBits(0, 10));
    
    // Check for block mode errors.
    if (blockMode.isError)
    {
        setASTCErrorColorBlock(dst, blockWidth, blockHeight, isSRGB);
        return DECOMPRESS_RESULT_ERROR;
    }
    
    // Separate path for void-extent.
    if (blockMode.isVoidExtent)
        return decodeVoidExtentBlock(dst, blockData, blockWidth, blockHeight, isSRGB, isLDR);
    
    // Compute weight grid values.
    const int numWeights            = computeNumWeights(blockMode);
    const int numWeightDataBits     = computeNumRequiredBits(blockMode.weightISEParams, numWeights);
    const int numPartitions         = (int)blockData.getBits(11, 12) + 1;
    
    // Check for errors in weight grid, partition and dual-plane parameters.
    if ((numWeights > 64)                               ||
        (numWeightDataBits > 96)                        ||
        (numWeightDataBits < 24)                        ||
        (blockMode.weightGridWidth > blockWidth)        ||
        (blockMode.weightGridHeight > blockHeight)      ||
        ((numPartitions == 4) && blockMode.isDualPlane))
    {
        setASTCErrorColorBlock(dst, blockWidth, blockHeight, isSRGB);
        return DECOMPRESS_RESULT_ERROR;
    }
    
    // Compute number of bits available for color endpoint data.
    const bool  isSingleUniqueCem           = (numPartitions == 1) || (blockData.getBits(23, 24) == 0);

    const int   numConfigDataBits           = ((numPartitions == 1) ? 17 : isSingleUniqueCem ? 29 : 25 + 3*numPartitions) +
                                              (blockMode.isDualPlane ? 2 : 0);

    const int   numBitsForColorEndpoints    = 128 - numWeightDataBits - numConfigDataBits;

    const int   extraCemBitsStart           = 127 - numWeightDataBits - (isSingleUniqueCem      ? -1
                                                                        : (numPartitions == 4)  ? 7
                                                                        : (numPartitions == 3)  ? 4
                                                                        : (numPartitions == 2)  ? 1
                                                                        : 0);
    
    // Decode color endpoint modes.
    deUint32 colorEndpointModes[4];
    decodeColorEndpointModes(&colorEndpointModes[0], blockData, numPartitions, extraCemBitsStart);
    const int numColorEndpointValues = computeNumColorEndpointValues(colorEndpointModes, numPartitions);
    
    // Check for errors in color endpoint value count.
    if ((numColorEndpointValues > 18) || (numBitsForColorEndpoints < (int)deDivRoundUp32(13*numColorEndpointValues, 5)))
    {
        setASTCErrorColorBlock(dst, blockWidth, blockHeight, isSRGB);
        return DECOMPRESS_RESULT_ERROR;
    }
    
    // Compute color endpoints.
    ColorEndpointPair colorEndpoints[4];
    computeColorEndpoints(&colorEndpoints[0], blockData, &colorEndpointModes[0], numPartitions, numColorEndpointValues,
                          computeMaximumRangeISEParams(numBitsForColorEndpoints, numColorEndpointValues), numBitsForColorEndpoints);
    
    // Compute texel weights.
    TexelWeightPair texelWeights[MAX_BLOCK_WIDTH*MAX_BLOCK_HEIGHT];
    computeTexelWeights(&texelWeights[0], blockData, blockWidth, blockHeight, blockMode);
    
    // Set texel colors.
    const int       ccs                     = blockMode.isDualPlane ? (int)blockData.getBits(extraCemBitsStart-2, extraCemBitsStart-1) : -1;
    const deUint32  partitionIndexSeed      = (numPartitions > 1) ? blockData.getBits(13, 22) : (deUint32)-1;

    return setTexelColors(dst, &colorEndpoints[0], &texelWeights[0], ccs, partitionIndexSeed, numPartitions, blockWidth, blockHeight, isSRGB, isLDR, &colorEndpointModes[0]);
}

// Returns -1 on error, 0 if LDR, 1 if HDR
int isHDR(const Block128& blockData, int blockWidth, int blockHeight)
{
    // Decode block mode.
    const ASTCBlockMode blockMode = getASTCBlockMode(blockData.getBits(0, 10));

    // Check for block mode errors.
    if (blockMode.isError)
        return -1;

    // Separate path for void-extent.
    if (blockMode.isVoidExtent)
    {
        const bool isHDRBlock = blockData.isBitSet(9);
        return isHDRBlock ? 1 : 0;
    }

    // Compute weight grid values.
    const int numWeights = computeNumWeights(blockMode);
    const int numWeightDataBits = computeNumRequiredBits(blockMode.weightISEParams, numWeights);
    const int numPartitions = (int)blockData.getBits(11, 12) + 1;

    // Check for errors in weight grid, partition and dual-plane parameters.
    if ((numWeights > 64) ||
        (numWeightDataBits > 96) ||
        (numWeightDataBits < 24) ||
        (blockMode.weightGridWidth > blockWidth) ||
        (blockMode.weightGridHeight > blockHeight) ||
        ((numPartitions == 4) && blockMode.isDualPlane))
    {
        return -1;
    }

    // Compute number of bits available for color endpoint data.
    const bool  isSingleUniqueCem = (numPartitions == 1) || (blockData.getBits(23, 24) == 0);

    const int   extraCemBitsStart = 127 - numWeightDataBits - (isSingleUniqueCem ? -1
        : (numPartitions == 4) ? 7
        : (numPartitions == 3) ? 4
        : (numPartitions == 2) ? 1
        : 0);

    // Decode color endpoint modes.
    deUint32 colorEndpointModes[4];
    decodeColorEndpointModes(&colorEndpointModes[0], blockData, numPartitions, extraCemBitsStart);
    
    for (int i = 0; i < numPartitions; i++)
    {
        if (isColorEndpointModeHDR(colorEndpointModes[i]))
            return 1;
    }

    return 0;
}

typedef uint16_t half_float;

half_float float_to_half(float val, bool toward_zero)
{
    union { float f; int32_t i; uint32_t u; } fi = { val };
    const int flt_m = fi.i & 0x7FFFFF, flt_e = (fi.i >> 23) & 0xFF, flt_s = (fi.i >> 31) & 0x1;
    int s = flt_s, e = 0, m = 0;

    // inf/NaN
    if (flt_e == 0xff)
    {
        e = 31;
        if (flt_m != 0) // NaN
            m = 1;
    }
    // not zero or denormal
    else if (flt_e != 0)
    {
        int new_exp = flt_e - 127;
        if (new_exp > 15)
            e = 31;
        else if (new_exp < -14)
        {
            if (toward_zero)
                m = (int)truncf((1 << 24) * fabsf(fi.f));
            else
                m = lrintf((1 << 24) * fabsf(fi.f));
        }
        else
        {
            e = new_exp + 15;
            if (toward_zero)
                m = (int)truncf((float)flt_m * (1.0f / (float)(1 << 13)));
            else
                m = lrintf((float)flt_m * (1.0f / (float)(1 << 13)));
        }
    }

    assert((0 <= m) && (m <= 1024));
    if (m == 1024)
    {
        e++;
        m = 0;
    }

    assert((s >= 0) && (s <= 1));
    assert((e >= 0) && (e <= 31));
    assert((m >= 0) && (m <= 1023));

    half_float result = (half_float)((s << 15) | (e << 10) | m);
    return result;
}

float half_to_float(half_float hval)
{
    union { float f; uint32_t u; } x = { 0 };

    uint32_t s = ((uint32_t)hval >> 15) & 1;
    uint32_t e = ((uint32_t)hval >> 10) & 0x1F;
    uint32_t m = (uint32_t)hval & 0x3FF;

    if (!e)
    {
        if (!m)
        {
            // +- 0
            x.u = s << 31;
            return x.f;
        }
        else
        {
            // denormalized
            while (!(m & 0x00000400))
            {
                m <<= 1;
                --e;
            }

            ++e;
            m &= ~0x00000400;
        }
    }
    else if (e == 31)
    {
        if (m == 0)
        {
            // +/- INF
            x.u = (s << 31) | 0x7f800000;
            return x.f;
        }
        else
        {
            // +/- NaN
            x.u = (s << 31) | 0x7f800000 | (m << 13);
            return x.f;
        }
    }

    e = e + (127 - 15);
    m = m << 13;

    assert(s <= 1);
    assert(m <= 0x7FFFFF);
    assert(e <= 255);

    x.u = m | (e << 23) | (s << 31);
    return x.f;
}

} // anonymous

// See https://registry.khronos.org/DataFormat/specs/1.3/dataformat.1.3.inline.html#_hdr_endpoint_decoding
static void convert_to_half_prec(uint32_t n, float* pVals)
{
#if 0
    const int prev_dir = fesetround(FE_TOWARDZERO);

    for (uint32_t i = 0; i < n; i++)
        pVals[i] = half_to_float(float_to_half(pVals[i]));

    fesetround(prev_dir);

    for (uint32_t i = 0; i < n; i++)
    {
        assert(pVals[i] == half_to_float(float_to_half(pVals[i], true)));
    }
#else
    // This ensures the values are rounded towards zero as half floats.
    for (uint32_t i = 0; i < n; i++)
    {
        pVals[i] = half_to_float(float_to_half(pVals[i], true));
    }
#endif
}

bool decompress_ldr(uint8_t *pDst, const uint8_t * data, bool isSRGB, int blockWidth, int blockHeight)
{
    float linear[MAX_BLOCK_WIDTH * MAX_BLOCK_HEIGHT * 4];

    const Block128 blockData(data);
    
    // isSRGB is true, this writes uint8_t's. Otherwise it writes floats.
    if (decompressBlock(isSRGB ? (void*)pDst : (void*)&linear[0], blockData, blockWidth, blockHeight, isSRGB, true) != DECOMPRESS_RESULT_VALID_BLOCK)
    {
        return false;
    }

    if (!isSRGB)
    {
        // Convert the floats to 8-bits with rounding.
        int pix = 0;
        for (int i = 0; i < blockHeight; i++)
        {
            for (int j = 0; j < blockWidth; j++, pix++)
            {
                pDst[4 * pix + 0] = (uint8_t)(basisu_astc::clamp<int>((int)(linear[pix * 4 + 0] * 65536.0f + .5f), 0, 65535) >> 8);
                pDst[4 * pix + 1] = (uint8_t)(basisu_astc::clamp<int>((int)(linear[pix * 4 + 1] * 65536.0f + .5f), 0, 65535) >> 8);
                pDst[4 * pix + 2] = (uint8_t)(basisu_astc::clamp<int>((int)(linear[pix * 4 + 2] * 65536.0f + .5f), 0, 65535) >> 8);
                pDst[4 * pix + 3] = (uint8_t)(basisu_astc::clamp<int>((int)(linear[pix * 4 + 3] * 65536.0f + .5f), 0, 65535) >> 8);
            }
        }
    }

    return true;
}

bool decompress_hdr(float* pDstRGBA, const uint8_t* data, int blockWidth, int blockHeight)
{
    const Block128 blockData(data);

    if (decompressBlock(pDstRGBA, blockData, blockWidth, blockHeight, false, false) != DECOMPRESS_RESULT_VALID_BLOCK)
    {
        return false;
    }

    convert_to_half_prec(blockWidth * blockHeight * 4, pDstRGBA);
        
    return true;
}

bool is_hdr(const uint8_t* data, int blockWidth, int blockHeight, bool &is_hdr)
{
    is_hdr = false;

    const Block128 blockData(data);
    
    int status = isHDR(blockData, blockWidth, blockHeight);
    if (status < 0)
    {
        return false;
    }

    is_hdr = (status == 1);

    return true;
}

} // astc

} // basisu_astc

#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

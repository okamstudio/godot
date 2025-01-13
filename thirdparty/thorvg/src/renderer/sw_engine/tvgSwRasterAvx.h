/*
 * Copyright (c) 2021 - 2024 the ThorVG project. All rights reserved.

 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:

 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.

 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifdef THORVG_AVX_VECTOR_SUPPORT

#include <immintrin.h>

#define N_32BITS_IN_128REG 4
#define N_32BITS_IN_256REG 8

static inline __m128i ALPHA_BLEND(__m128i c, __m128i a)
{
    //1. set the masks for the A/G and R/B channels
    auto AG = _mm_set1_epi32(0xff00ff00);
    auto RB = _mm_set1_epi32(0x00ff00ff);

    //2. mask the alpha vector - originally quartet [a, a, a, a]
    auto aAG = _mm_and_si128(a, AG);
    auto aRB = _mm_and_si128(a, RB);

    //3. calculate the alpha blending of the 2nd and 4th channel
    //- mask the color vector
    //- multiply it by the masked alpha vector
    //- add the correction to compensate bit shifting used instead of dividing by 255
    //- shift bits - corresponding to division by 256
    auto even = _mm_and_si128(c, RB);
    even = _mm_mullo_epi16(even, aRB);
    even =_mm_add_epi16(even, RB);
    even = _mm_srli_epi16(even, 8);

    //4. calculate the alpha blending of the 1st and 3rd channel:
    //- mask the color vector
    //- multiply it by the corresponding masked alpha vector and store the high bits of the result
    //- add the correction to compensate division by 256 instead of by 255 (next step)
    //- remove the low 8 bits to mimic the division by 256
    auto odd = _mm_and_si128(c, AG);
    odd = _mm_mulhi_epu16(odd, aAG);
    odd = _mm_add_epi16(odd, RB);
    odd = _mm_and_si128(odd, AG);

    //5. the final result
    return _mm_or_si128(odd, even);
}


static void avxRasterGrayscale8(uint8_t* dst, uint8_t val, uint32_t offset, int32_t len) 
{
    dst += offset; 

    __m256i vecVal = _mm256_set1_epi8(val);

    int32_t i = 0;
    for (; i <= len - 32; i += 32) {
        _mm256_storeu_si256((__m256i*)(dst + i), vecVal);
    }

    for (; i < len; ++i) {
        dst[i] = val;
    }
}


static void avxRasterPixel32(uint32_t *dst, uint32_t val, uint32_t offset, int32_t len)
{
    //1. calculate how many iterations we need to cover the length
    uint32_t iterations = len / N_32BITS_IN_256REG;
    uint32_t avxFilled = iterations * N_32BITS_IN_256REG;

    //2. set the beginning of the array
    dst += offset;

    //3. fill the octets
    for (uint32_t i = 0; i < iterations; ++i, dst += N_32BITS_IN_256REG) {
        _mm256_storeu_si256((__m256i*)dst, _mm256_set1_epi32(val));
    }

    //4. fill leftovers (in the first step we have to set the pointer to the place where the avx job is done)
    int32_t leftovers = len - avxFilled;
    while (leftovers--) *dst++ = val;
}


static bool avxRasterTranslucentRect(SwSurface* surface, const SwBBox& region, uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    auto h = static_cast<uint32_t>(region.max.y - region.min.y);
    auto w = static_cast<uint32_t>(region.max.x - region.min.x);

    //32bits channels
    if (surface->channelSize == sizeof(uint32_t)) {
        auto color = surface->join(r, g, b, a);
        auto buffer = surface->buf32 + (region.min.y * surface->stride) + region.min.x;

        uint32_t ialpha = 255 - a;

        auto avxColor = _mm_set1_epi32(color);
        auto avxIalpha = _mm_set1_epi8(ialpha);

        for (uint32_t y = 0; y < h; ++y) {
            auto dst = &buffer[y * surface->stride];

            //1. fill the not aligned memory (for 128-bit registers a 16-bytes alignment is required)
            auto notAligned = ((uintptr_t)dst & 0xf) / 4;
            if (notAligned) {
                notAligned = (N_32BITS_IN_128REG - notAligned > w ? w : N_32BITS_IN_128REG - notAligned);
                for (uint32_t x = 0; x < notAligned; ++x, ++dst) {
                    *dst = color + ALPHA_BLEND(*dst, ialpha);
                }
            }

            //2. fill the aligned memory - N_32BITS_IN_128REG pixels processed at once
            uint32_t iterations = (w - notAligned) / N_32BITS_IN_128REG;
            uint32_t avxFilled = iterations * N_32BITS_IN_128REG;
            auto avxDst = (__m128i*)dst;
            for (uint32_t x = 0; x < iterations; ++x, ++avxDst) {
                *avxDst = _mm_add_epi32(avxColor, ALPHA_BLEND(*avxDst, avxIalpha));
            }

            //3. fill the remaining pixels
            int32_t leftovers = w - notAligned - avxFilled;
            dst += avxFilled;
            while (leftovers--) {
                *dst = color + ALPHA_BLEND(*dst, ialpha);
                dst++;
            }
        }
    //8bit grayscale
    } else if (surface->channelSize == sizeof(uint8_t)) {
        TVGLOG("SW_ENGINE", "Require AVX Optimization, Channel Size = %d", surface->channelSize);
        auto buffer = surface->buf8 + (region.min.y * surface->stride) + region.min.x;
        auto ialpha = ~a;
        for (uint32_t y = 0; y < h; ++y) {
            auto dst = &buffer[y * surface->stride];
            for (uint32_t x = 0; x < w; ++x, ++dst) {
                *dst = a + MULTIPLY(*dst, ialpha);
            }
        }
    }
    return true;
}


static bool avxRasterTranslucentRle(SwSurface* surface, const SwRle* rle, uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    auto span = rle->spans;

    //32bit channels
    if (surface->channelSize == sizeof(uint32_t)) {
        auto color = surface->join(r, g, b, a);
        uint32_t src;

        for (uint32_t i = 0; i < rle->size; ++i) {
            auto dst = &surface->buf32[span->y * surface->stride + span->x];

            if (span->coverage < 255) src = ALPHA_BLEND(color, span->coverage);
            else src = color;

        auto ialpha = IA(src);

            //1. fill the not aligned memory (for 128-bit registers a 16-bytes alignment is required)
            auto notAligned = ((uintptr_t)dst & 0xf) / 4;
            if (notAligned) {
                notAligned = (N_32BITS_IN_128REG - notAligned > span->len ? span->len : N_32BITS_IN_128REG - notAligned);
                for (uint32_t x = 0; x < notAligned; ++x, ++dst) {
                    *dst = src + ALPHA_BLEND(*dst, ialpha);
                }
            }

            //2. fill the aligned memory using avx - N_32BITS_IN_128REG pixels processed at once
            //In order to avoid unnecessary avx variables declarations a check is made whether there are any iterations at all
            uint32_t iterations = (span->len - notAligned) / N_32BITS_IN_128REG;
            uint32_t avxFilled = 0;
            if (iterations > 0) {
                auto avxSrc = _mm_set1_epi32(src);
                auto avxIalpha = _mm_set1_epi8(ialpha);

                avxFilled = iterations * N_32BITS_IN_128REG;
                auto avxDst = (__m128i*)dst;
                for (uint32_t x = 0; x < iterations; ++x, ++avxDst) {
                    *avxDst = _mm_add_epi32(avxSrc, ALPHA_BLEND(*avxDst, avxIalpha));
                }
            }

            //3. fill the remaining pixels
            int32_t leftovers = span->len - notAligned - avxFilled;
            dst += avxFilled;
            while (leftovers--) {
                *dst = src + ALPHA_BLEND(*dst, ialpha);
                dst++;
            }

            ++span;
        }
    //8bit grayscale
    } else if (surface->channelSize == sizeof(uint8_t)) {
        TVGLOG("SW_ENGINE", "Require AVX Optimization, Channel Size = %d", surface->channelSize);
        uint8_t src;
        for (uint32_t i = 0; i < rle->size; ++i, ++span) {
            auto dst = &surface->buf8[span->y * surface->stride + span->x];
            if (span->coverage < 255) src = MULTIPLY(span->coverage, a);
            else src = a;
            auto ialpha = ~a;
            for (uint32_t x = 0; x < span->len; ++x, ++dst) {
                *dst = src + MULTIPLY(*dst, ialpha);
            }
        }
    }
    return true;
}


#endif

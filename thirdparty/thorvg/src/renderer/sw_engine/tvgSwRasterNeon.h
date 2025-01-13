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

#ifdef THORVG_NEON_VECTOR_SUPPORT

#include <arm_neon.h>

//TODO : need to support windows ARM
 
#if defined(__ARM_64BIT_STATE) || defined(_M_ARM64)
#define TVG_AARCH64 1
#else
#define TVG_AARCH64 0
#endif


static inline uint8x8_t ALPHA_BLEND(uint8x8_t c, uint8x8_t a)
{
    uint16x8_t t = vmull_u8(c, a);
    return vshrn_n_u16(t, 8);
}


static void neonRasterGrayscale8(uint8_t* dst, uint8_t val, uint32_t offset, int32_t len)
{
    dst += offset;

    int32_t i = 0;
    const uint8x16_t valVec = vdupq_n_u8(val);
#if TVG_AARCH64
    uint8x16x4_t valQuad = {valVec, valVec, valVec, valVec};
    for (; i <= len - 16 * 4; i += 16 * 4) {
        vst1q_u8_x4(dst + i, valQuad);
    }
#else
    for (; i <= len - 16; i += 16) {
        vst1q_u8(dst + i, valVec);
    }
#endif
    for (; i < len; i++) {
        dst[i] = val;
    }
}


static void neonRasterPixel32(uint32_t *dst, uint32_t val, uint32_t offset, int32_t len)
{
    dst += offset;

    uint32x4_t vectorVal = vdupq_n_u32(val);

#if TVG_AARCH64
    uint32_t iterations = len / 16;
    uint32_t neonFilled = iterations * 16;
    uint32x4x4_t valQuad = {vectorVal, vectorVal, vectorVal, vectorVal};
    for (uint32_t i = 0; i < iterations; ++i) {
        vst4q_u32(dst, valQuad);
        dst += 16;
    }
#else
    uint32_t iterations = len / 4;
    uint32_t neonFilled = iterations * 4;
    for (uint32_t i = 0; i < iterations; ++i) {
        vst1q_u32(dst, vectorVal);
        dst += 4;
    }
#endif
    int32_t leftovers = len - neonFilled;
    while (leftovers--) *dst++ = val;
}


static bool neonRasterTranslucentRle(SwSurface* surface, const SwRle* rle, uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    auto span = rle->spans;

    //32bit channels
    if (surface->channelSize == sizeof(uint32_t)) {
        auto color = surface->join(r, g, b, a);
        uint32_t src;
        uint8x8_t *vDst = nullptr;
        uint16_t align;

        for (uint32_t i = 0; i < rle->size; ++i) {
            if (span->coverage < 255) src = ALPHA_BLEND(color, span->coverage);
            else src = color;

            auto dst = &surface->buf32[span->y * surface->stride + span->x];
            auto ialpha = IA(src);

            if ((((uintptr_t) dst) & 0x7) != 0) {
                //fill not aligned byte
                *dst = src + ALPHA_BLEND(*dst, ialpha);
                vDst = (uint8x8_t*)(dst + 1);
                align = 1;
            } else {
                vDst = (uint8x8_t*) dst;
                align = 0;
            }

            uint8x8_t vSrc = (uint8x8_t) vdup_n_u32(src);
            uint8x8_t vIalpha = vdup_n_u8((uint8_t) ialpha);

            for (uint32_t x = 0; x < (span->len - align) / 2; ++x)
                vDst[x] = vadd_u8(vSrc, ALPHA_BLEND(vDst[x], vIalpha));

            auto leftovers = (span->len - align) % 2;
            if (leftovers > 0) dst[span->len - 1] = src + ALPHA_BLEND(dst[span->len - 1], ialpha);

            ++span;
        }
    //8bit grayscale
    } else if (surface->channelSize == sizeof(uint8_t)) {
        TVGLOG("SW_ENGINE", "Require Neon Optimization, Channel Size = %d", surface->channelSize);
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


static bool neonRasterTranslucentRect(SwSurface* surface, const SwBBox& region, uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    auto h = static_cast<uint32_t>(region.max.y - region.min.y);
    auto w = static_cast<uint32_t>(region.max.x - region.min.x);

    //32bits channels
    if (surface->channelSize == sizeof(uint32_t)) {
        auto color = surface->join(r, g, b, a);
        auto buffer = surface->buf32 + (region.min.y * surface->stride) + region.min.x;
        auto ialpha = 255 - a;

        auto vColor = vdup_n_u32(color);
        auto vIalpha = vdup_n_u8((uint8_t) ialpha);

        uint8x8_t* vDst = nullptr;
        uint32_t align;

        for (uint32_t y = 0; y < h; ++y) {
            auto dst = &buffer[y * surface->stride];

            if ((((uintptr_t) dst) & 0x7) != 0) {
                //fill not aligned byte
                *dst = color + ALPHA_BLEND(*dst, ialpha);
                vDst = (uint8x8_t*) (dst + 1);
                align = 1;
            } else {
                vDst = (uint8x8_t*) dst;
                align = 0;
            }

            for (uint32_t x = 0; x <  (w - align) / 2; ++x)
                vDst[x] = vadd_u8((uint8x8_t)vColor, ALPHA_BLEND(vDst[x], vIalpha));

            auto leftovers = (w - align) % 2;
            if (leftovers > 0) dst[w - 1] = color + ALPHA_BLEND(dst[w - 1], ialpha);
        }
    //8bit grayscale
    } else if (surface->channelSize == sizeof(uint8_t)) {
        TVGLOG("SW_ENGINE", "Require Neon Optimization, Channel Size = %d", surface->channelSize);
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

#endif

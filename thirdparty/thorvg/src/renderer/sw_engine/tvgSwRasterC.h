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


template<typename PIXEL_T>
static void inline cRasterTranslucentPixels(PIXEL_T* dst, PIXEL_T* src, uint32_t len, uint32_t opacity)
{
    //TODO: 64bits faster?
    if (opacity == 255) {
        for (uint32_t x = 0; x < len; ++x, ++dst, ++src) {
            *dst = *src + ALPHA_BLEND(*dst, IA(*src));
        }
    } else {
        for (uint32_t x = 0; x < len; ++x, ++dst, ++src) {
            auto tmp = ALPHA_BLEND(*src, opacity);
            *dst = tmp + ALPHA_BLEND(*dst, IA(tmp));
        }
    }
}


template<typename PIXEL_T>
static void inline cRasterPixels(PIXEL_T* dst, PIXEL_T* src, uint32_t len, uint32_t opacity)
{
    //TODO: 64bits faster?
    if (opacity == 255) {
        for (uint32_t x = 0; x < len; ++x, ++dst, ++src) {
            *dst = *src;
        }
    } else {
        cRasterTranslucentPixels(dst, src, len, opacity);
    }
}


template<typename PIXEL_T>
static void inline cRasterPixels(PIXEL_T* dst, PIXEL_T val, uint32_t offset, int32_t len)
{
    dst += offset;

    //fix the misaligned memory
    auto alignOffset = (long long) dst % 8;
    if (alignOffset > 0) {
        if (sizeof(PIXEL_T) == 4) alignOffset /= 4;
        else if (sizeof(PIXEL_T) == 1) alignOffset = 8 - alignOffset;
        while (alignOffset > 0 && len > 0) {
            *dst++ = val;
            --len;
            --alignOffset;
        }
    }

    //64bits faster clear
    if ((sizeof(PIXEL_T) == 4)) {
        auto val64 = (uint64_t(val) << 32) | uint64_t(val);
        while (len > 1) {
            *reinterpret_cast<uint64_t*>(dst) = val64;
            len -= 2;
            dst += 2;
        }
    } else if (sizeof(PIXEL_T) == 1) {
        auto val32 = (uint32_t(val) << 24) | (uint32_t(val) << 16) | (uint32_t(val) << 8) | uint32_t(val);
        auto val64 = (uint64_t(val32) << 32) | val32;
        while (len > 7) {
            *reinterpret_cast<uint64_t*>(dst) = val64;
            len -= 8;
            dst += 8;
        }
    }

    //leftovers
    while (len--) *dst++ = val;
}


static bool inline cRasterTranslucentRle(SwSurface* surface, const SwRle* rle, uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    auto span = rle->spans;

    //32bit channels
    if (surface->channelSize == sizeof(uint32_t)) {
        auto color = surface->join(r, g, b, a);
        uint32_t src;
        for (uint32_t i = 0; i < rle->size; ++i, ++span) {
            auto dst = &surface->buf32[span->y * surface->stride + span->x];
            if (span->coverage < 255) src = ALPHA_BLEND(color, span->coverage);
            else src = color;
            auto ialpha = IA(src);
            for (uint32_t x = 0; x < span->len; ++x, ++dst) {
                *dst = src + ALPHA_BLEND(*dst, ialpha);
            }
        }
    //8bit grayscale
    } else if (surface->channelSize == sizeof(uint8_t)) {
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


static bool inline cRasterTranslucentRect(SwSurface* surface, const SwBBox& region, uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    auto h = static_cast<uint32_t>(region.max.y - region.min.y);
    auto w = static_cast<uint32_t>(region.max.x - region.min.x);

    //32bits channels
    if (surface->channelSize == sizeof(uint32_t)) {
        auto color = surface->join(r, g, b, a);
        auto buffer = surface->buf32 + (region.min.y * surface->stride) + region.min.x;
        auto ialpha = 255 - a;
        for (uint32_t y = 0; y < h; ++y) {
            auto dst = &buffer[y * surface->stride];
            for (uint32_t x = 0; x < w; ++x, ++dst) {
                *dst = color + ALPHA_BLEND(*dst, ialpha);
            }
        }
    //8bit grayscale
    } else if (surface->channelSize == sizeof(uint8_t)) {
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


static bool inline cRasterABGRtoARGB(RenderSurface* surface)
{
    TVGLOG("SW_ENGINE", "Convert ColorSpace ABGR - ARGB [Size: %d x %d]", surface->w, surface->h);

    //64bits faster converting
    if (surface->w % 2 == 0) {
        auto buffer = reinterpret_cast<uint64_t*>(surface->buf32);
        for (uint32_t y = 0; y < surface->h; ++y, buffer += surface->stride / 2) {
            auto dst = buffer;
            for (uint32_t x = 0; x < surface->w / 2; ++x, ++dst) {
                auto c = *dst;
                //flip Blue, Red channels
                *dst = (c & 0xff000000ff000000) + ((c & 0x00ff000000ff0000) >> 16) + (c & 0x0000ff000000ff00) + ((c & 0x000000ff000000ff) << 16);
            }
        }
    //default converting
    } else {
        auto buffer = surface->buf32;
        for (uint32_t y = 0; y < surface->h; ++y, buffer += surface->stride) {
            auto dst = buffer;
            for (uint32_t x = 0; x < surface->w; ++x, ++dst) {
                auto c = *dst;
                //flip Blue, Red channels
                *dst = (c & 0xff000000) + ((c & 0x00ff0000) >> 16) + (c & 0x0000ff00) + ((c & 0x000000ff) << 16);
            }
        }
    }
    return true;
}


static bool inline cRasterARGBtoABGR(RenderSurface* surface)
{
    //exactly same with ABGRtoARGB
    return cRasterABGRtoARGB(surface);
}

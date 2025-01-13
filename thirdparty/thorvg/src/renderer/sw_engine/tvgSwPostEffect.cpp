/*
 * Copyright (c) 2024 the ThorVG project. All rights reserved.

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

#include "tvgMath.h"
#include "tvgSwCommon.h"

/************************************************************************/
/* Gaussian Blur Implementation                                         */
/************************************************************************/

struct SwGaussianBlur
{
    static constexpr int MAX_LEVEL = 3;
    int level;
    int kernel[MAX_LEVEL];
};


static void _gaussianExtendRegion(RenderRegion& region, int extra, int8_t direction)
{
    //bbox region expansion for feathering
    if (direction != 2) {
        region.x = -extra;
        region.w = extra * 2;
    }
    if (direction != 1) {
        region.y = -extra;
        region.h = extra * 2;
    }
}


static int _gaussianEdgeWrap(int end, int idx)
{
    auto r = idx % end;
    return (r < 0) ? end + r : r;
}


static int _gaussianEdgeExtend(int end, int idx)
{
    if (idx < 0) return 0;
    else if (idx >= end) return end - 1;
    return idx;
}


static int _gaussianRemap(int end, int idx, int border)
{
    if (border == 1) return _gaussianEdgeWrap(end, idx);
    return _gaussianEdgeExtend(end, idx);
}


//TODO: SIMD OPTIMIZATION?
static void _gaussianFilter(uint8_t* dst, uint8_t* src, int32_t stride, int32_t w, int32_t h, const SwBBox& bbox, int32_t dimension, int border, bool flipped)
{
    if (flipped) {
        src += (bbox.min.x * stride + bbox.min.y) << 2;
        dst += (bbox.min.x * stride + bbox.min.y) << 2;
    } else {
        src += (bbox.min.y * stride + bbox.min.x) << 2;
        dst += (bbox.min.y * stride + bbox.min.x) << 2;
    }

    auto iarr = 1.0f / (dimension + dimension + 1);

    #pragma omp parallel for
    for (int y = 0; y < h; ++y) {
        auto p = y * stride;
        auto i = p * 4;                 //current index
        auto l = -(dimension + 1);      //left index
        auto r = dimension;             //right index
        int acc[4] = {0, 0, 0, 0};      //sliding accumulator

        //initial accumulation
        for (int x = l; x < r; ++x) {
            auto id = (_gaussianRemap(w, x, border) + p) * 4;
            acc[0] += src[id++];
            acc[1] += src[id++];
            acc[2] += src[id++];
            acc[3] += src[id];
        }
        //perform filtering
        for (int x = 0; x < w; ++x, ++r, ++l) {
            auto rid = (_gaussianRemap(w, r, border) + p) * 4;
            auto lid = (_gaussianRemap(w, l, border) + p) * 4;
            acc[0] += src[rid++] - src[lid++];
            acc[1] += src[rid++] - src[lid++];
            acc[2] += src[rid++] - src[lid++];
            acc[3] += src[rid] - src[lid];
            dst[i++] = static_cast<uint8_t>(acc[0] * iarr + 0.5f);
            dst[i++] = static_cast<uint8_t>(acc[1] * iarr + 0.5f);
            dst[i++] = static_cast<uint8_t>(acc[2] * iarr + 0.5f);
            dst[i++] = static_cast<uint8_t>(acc[3] * iarr + 0.5f);
        }
    }
}


static int _gaussianInit(SwGaussianBlur* data, float sigma, int quality)
{
    const auto MAX_LEVEL = SwGaussianBlur::MAX_LEVEL;

    if (tvg::zero(sigma)) return 0;

    data->level = int(SwGaussianBlur::MAX_LEVEL * ((quality - 1) * 0.01f)) + 1;

    //compute box kernel sizes
    auto wl = (int) sqrt((12 * sigma / MAX_LEVEL) + 1);
    if (wl % 2 == 0) --wl;
    auto wu = wl + 2;
    auto mi = (12 * sigma - MAX_LEVEL * wl * wl - 4 * MAX_LEVEL * wl - 3 * MAX_LEVEL) / (-4 * wl - 4);
    auto m = int(mi + 0.5f);
    auto extends = 0;

    for (int i = 0; i < data->level; i++) {
        data->kernel[i] = ((i < m ? wl : wu) - 1) / 2;
        extends += data->kernel[i];
    }

    return extends;
}


bool effectGaussianBlurPrepare(RenderEffectGaussianBlur* params)
{
    auto rd = (SwGaussianBlur*)malloc(sizeof(SwGaussianBlur));

    auto extends = _gaussianInit(rd, params->sigma * params->sigma, params->quality);

    //invalid
    if (extends == 0) {
        params->invalid = true;
        free(rd);
        return false;
    }

    _gaussianExtendRegion(params->extend, extends, params->direction);

    params->rd = rd;

    return true;
}


bool effectGaussianBlur(SwCompositor* cmp, SwSurface* surface, const RenderEffectGaussianBlur* params)
{
    if (cmp->image.channelSize != sizeof(uint32_t)) {
        TVGERR("SW_ENGINE", "Not supported grayscale Gaussian Blur!");
        return false;
    }

    auto& buffer = surface->compositor->image;
    auto data = static_cast<SwGaussianBlur*>(params->rd);
    auto& bbox = cmp->bbox;
    auto w = (bbox.max.x - bbox.min.x);
    auto h = (bbox.max.y - bbox.min.y);
    auto stride = cmp->image.stride;
    auto front = cmp->image.buf32;
    auto back = buffer.buf32;
    auto swapped = false;

    TVGLOG("SW_ENGINE", "GaussianFilter region(%ld, %ld, %ld, %ld) params(%f %d %d), level(%d)", bbox.min.x, bbox.min.y, bbox.max.x, bbox.max.y, params->sigma, params->direction, params->border, data->level);

    /* It is best to take advantage of the Gaussian blur’s separable property
       by dividing the process into two passes. horizontal and vertical.
       We can expect fewer calculations. */

    //horizontal
    if (params->direction != 2) {
        for (int i = 0; i < data->level; ++i) {
            _gaussianFilter(reinterpret_cast<uint8_t*>(back), reinterpret_cast<uint8_t*>(front), stride, w, h, bbox, data->kernel[i], params->border, false);
            std::swap(front, back);
            swapped = !swapped;
        }
    }

    //vertical. x/y flipping and horionztal access is pretty compatible with the memory architecture.
    if (params->direction != 1) {
        rasterXYFlip(front, back, stride, w, h, bbox, false);
        std::swap(front, back);

        for (int i = 0; i < data->level; ++i) {
            _gaussianFilter(reinterpret_cast<uint8_t*>(back), reinterpret_cast<uint8_t*>(front), stride, h, w, bbox, data->kernel[i], params->border, true);
            std::swap(front, back);
            swapped = !swapped;
        }

        rasterXYFlip(front, back, stride, h, w, bbox, true);
        std::swap(front, back);
    }

    if (swapped) std::swap(cmp->image.buf8, buffer.buf8);

    return true;
}

/************************************************************************/
/* Drop Shadow Implementation                                           */
/************************************************************************/

struct SwDropShadow : SwGaussianBlur
{
    SwPoint offset;
};


//TODO: SIMD OPTIMIZATION?
static void _dropShadowFilter(uint32_t* dst, uint32_t* src, int stride, int w, int h, const SwBBox& bbox, int32_t dimension, uint32_t color, bool flipped)
{
    if (flipped) {
        src += (bbox.min.x * stride + bbox.min.y);
        dst += (bbox.min.x * stride + bbox.min.y);
    } else {
        src += (bbox.min.y * stride + bbox.min.x);
        dst += (bbox.min.y * stride + bbox.min.x);
    }
    auto iarr = 1.0f / (dimension + dimension + 1);

    #pragma omp parallel for
    for (int y = 0; y < h; ++y) {
        auto p = y * stride;
        auto i = p;                     //current index
        auto l = -(dimension + 1);      //left index
        auto r = dimension;             //right index
        int acc = 0;                    //sliding accumulator

        //initial accumulation
        for (int x = l; x < r; ++x) {
            auto id = _gaussianEdgeExtend(w, x) + p;
            acc += A(src[id]);
        }
        //perform filtering
        for (int x = 0; x < w; ++x, ++r, ++l) {
            auto rid = _gaussianEdgeExtend(w, r) + p;
            auto lid = _gaussianEdgeExtend(w, l) + p;
            acc += A(src[rid]) - A(src[lid]);
            dst[i++] = ALPHA_BLEND(color, static_cast<uint8_t>(acc * iarr + 0.5f));
        }
    }
}


static void _dropShadowShift(uint32_t* dst, uint32_t* src, int stride, SwBBox& region, SwPoint& offset, uint8_t opacity, bool direct)
{
    src += (region.min.y * stride + region.min.x);
    dst += (region.min.y * stride + region.min.x);

    auto w = region.max.x - region.min.x;
    auto h = region.max.y - region.min.y;
    auto translucent = (direct || opacity < 255);

    //shift offset
    if (region.min.x + offset.x < 0) src -= offset.x;
    else dst += offset.x;

    if (region.min.y + offset.y < 0) src -= (offset.y * stride);
    else dst += (offset.y * stride);

    for (auto y = 0; y < h; ++y) {
        if (translucent) rasterTranslucentPixel32(dst, src, w, opacity);
        else rasterPixel32(dst, src, w, opacity);
        src += stride;
        dst += stride;
    }
}


static void _dropShadowExtendRegion(RenderRegion& region, int extra, SwPoint& offset)
{
    //bbox region expansion for feathering
    region.x = -extra;
    region.w = extra * 2;
    region.y = -extra;
    region.h = extra * 2;

    region.x = std::min(region.x + (int32_t)offset.x, region.x);
    region.y = std::min(region.y + (int32_t)offset.y, region.y);
    region.w += abs(offset.x);
    region.h += abs(offset.y);
}


bool effectDropShadowPrepare(RenderEffectDropShadow* params)
{
    auto rd = (SwDropShadow*)malloc(sizeof(SwDropShadow));

    //compute box kernel sizes
    auto extends = _gaussianInit(rd, params->sigma * params->sigma, params->quality);

    //invalid
    if (extends == 0 || params->color[3] == 0) {
        params->invalid = true;
        free(rd);
        return false;
    }

    //offset
    if (params->distance > 0.0f) {
        auto radian = tvg::deg2rad(90.0f - params->angle);
        rd->offset = {(SwCoord)(params->distance * cosf(radian)), (SwCoord)(-1.0f * params->distance * sinf(radian))};
    } else {
        rd->offset = {0, 0};
    }

    //bbox region expansion for feathering
    _dropShadowExtendRegion(params->extend, extends, rd->offset);

    params->rd = rd;

    return true;
}


//A quite same integration with effectGaussianBlur(). See it for detailed comments.
//surface[0]: the original image, to overlay it into the filtered image.
//surface[1]: temporary buffer for generating the filtered image.
bool effectDropShadow(SwCompositor* cmp, SwSurface* surface[2], const RenderEffectDropShadow* params, uint8_t opacity, bool direct)
{
    if (cmp->image.channelSize != sizeof(uint32_t)) {
        TVGERR("SW_ENGINE", "Not supported grayscale Drop Shadow!");
        return false;
    }

    //FIXME: if the body is partially visible due to clipping, the shadow also becomes partially visible.

    auto data = static_cast<SwDropShadow*>(params->rd);
    auto& bbox = cmp->bbox;
    auto w = (bbox.max.x - bbox.min.x);
    auto h = (bbox.max.y - bbox.min.y);

    //outside the screen
    if (abs(data->offset.x) >= w || abs(data->offset.y) >= h) return true;

    SwImage* buffer[] = {&surface[0]->compositor->image, &surface[1]->compositor->image};
    auto color = cmp->recoverSfc->join(params->color[0], params->color[1], params->color[2], 255);
    auto stride = cmp->image.stride;
    auto front = cmp->image.buf32;
    auto back = buffer[1]->buf32;
    opacity = MULTIPLY(params->color[3], opacity);

    TVGLOG("SW_ENGINE", "DropShadow region(%ld, %ld, %ld, %ld) params(%f %f %f), level(%d)", bbox.min.x, bbox.min.y, bbox.max.x, bbox.max.y, params->angle, params->distance, params->sigma, data->level);

    //saving the original image in order to overlay it into the filtered image.
    _dropShadowFilter(back, front, stride, w, h, bbox, data->kernel[0], color, false);
    std::swap(front, buffer[0]->buf32);
    std::swap(front, back);

    //horizontal
    for (int i = 1; i < data->level; ++i) {
        _dropShadowFilter(back, front, stride, w, h, bbox, data->kernel[i], color, false);
        std::swap(front, back);
    }

    //vertical
    rasterXYFlip(front, back, stride, w, h, bbox, false);
    std::swap(front, back);

    for (int i = 0; i < data->level; ++i) {
        _dropShadowFilter(back, front, stride, h, w, bbox, data->kernel[i], color, true);
        std::swap(front, back);
    }

    rasterXYFlip(front, back, stride, h, w, bbox, true);
    std::swap(cmp->image.buf32, back);

    //draw to the main surface directly
    if (direct) {
        _dropShadowShift(cmp->recoverSfc->buf32, cmp->image.buf32, stride, bbox, data->offset, opacity, direct);
        std::swap(cmp->image.buf32, buffer[0]->buf32);
        return true;
    }

    //draw to the intermediate surface
    rasterClear(surface[1], bbox.min.x, bbox.min.y, w, h);
    _dropShadowShift(buffer[1]->buf32, cmp->image.buf32, stride, bbox, data->offset, opacity, direct);
    std::swap(cmp->image.buf32, buffer[1]->buf32);

    //compositing shadow and body
    auto s = buffer[0]->buf32 + (bbox.min.y * buffer[0]->stride + bbox.min.x);
    auto d = cmp->image.buf32 + (bbox.min.y * cmp->image.stride + bbox.min.x);

    for (auto y = 0; y < h; ++y) {
        rasterTranslucentPixel32(d, s, w, 255);
        s += buffer[0]->stride;
        d += cmp->image.stride;
    }

    return true;
}

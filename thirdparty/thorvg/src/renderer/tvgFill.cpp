/*
 * Copyright (c) 2020 - 2024 the ThorVG project. All rights reserved.

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

#include "tvgFill.h"

/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/

Fill* RadialGradient::Impl::duplicate()
{
    auto ret = RadialGradient::gen();
    if (!ret) return nullptr;

    ret->pImpl->cx = cx;
    ret->pImpl->cy = cy;
    ret->pImpl->r = r;
    ret->pImpl->fx = fx;
    ret->pImpl->fy = fy;
    ret->pImpl->fr = fr;

    return ret.release();
}


Result RadialGradient::Impl::radial(float cx, float cy, float r, float fx, float fy, float fr)
{
    if (r < 0 || fr < 0) return Result::InvalidArguments;

    this->cx = cx;
    this->cy = cy;
    this->r = r;
    this->fx = fx;
    this->fy = fy;
    this->fr = fr;

    return Result::Success;
};


Fill* LinearGradient::Impl::duplicate()
{
    auto ret = LinearGradient::gen();
    if (!ret) return nullptr;

    ret->pImpl->x1 = x1;
    ret->pImpl->y1 = y1;
    ret->pImpl->x2 = x2;
    ret->pImpl->y2 = y2;

    return ret.release();
};


/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/

Fill::Fill():pImpl(new Impl())
{
}


Fill::~Fill()
{
    delete(pImpl);
}


Result Fill::colorStops(const ColorStop* colorStops, uint32_t cnt) noexcept
{
    if ((!colorStops && cnt > 0) || (colorStops && cnt == 0)) return Result::InvalidArguments;

    if (cnt == 0) {
        if (pImpl->colorStops) {
            free(pImpl->colorStops);
            pImpl->colorStops = nullptr;
            pImpl->cnt = 0;
        }
        return Result::Success;
    }

    if (pImpl->cnt != cnt) {
        pImpl->colorStops = static_cast<ColorStop*>(realloc(pImpl->colorStops, cnt * sizeof(ColorStop)));
    }

    pImpl->cnt = cnt;
    memcpy(pImpl->colorStops, colorStops, cnt * sizeof(ColorStop));

    return Result::Success;
}


uint32_t Fill::colorStops(const ColorStop** colorStops) const noexcept
{
    if (colorStops) *colorStops = pImpl->colorStops;

    return pImpl->cnt;
}


Result Fill::spread(FillSpread s) noexcept
{
    pImpl->spread = s;

    return Result::Success;
}


FillSpread Fill::spread() const noexcept
{
    return pImpl->spread;
}


Result Fill::transform(const Matrix& m) noexcept
{
    if (!pImpl->transform) {
        pImpl->transform = static_cast<Matrix*>(malloc(sizeof(Matrix)));
    }
    *pImpl->transform = m;
    return Result::Success;
}


Matrix Fill::transform() const noexcept
{
    if (pImpl->transform) return *pImpl->transform;
    return {1, 0, 0, 0, 1, 0, 0, 0, 1};
}


Fill* Fill::duplicate() const noexcept
{
    return pImpl->duplicate();
}


TVG_DEPRECATED uint32_t Fill::identifier() const noexcept
{
    return (uint32_t) type();
}


RadialGradient::RadialGradient():pImpl(new Impl())
{
    Fill::pImpl->method(new FillDup<RadialGradient::Impl>(pImpl));
}


RadialGradient::~RadialGradient()
{
    delete(pImpl);
}


Result RadialGradient::radial(float cx, float cy, float r) noexcept
{
    return pImpl->radial(cx, cy, r, cx, cy, 0.0f);
}


Result RadialGradient::radial(float* cx, float* cy, float* r) const noexcept
{
    if (cx) *cx = pImpl->cx;
    if (cy) *cy = pImpl->cy;
    if (r) *r = pImpl->r;

    return Result::Success;
}


unique_ptr<RadialGradient> RadialGradient::gen() noexcept
{
    return unique_ptr<RadialGradient>(new RadialGradient);
}


TVG_DEPRECATED uint32_t RadialGradient::identifier() noexcept
{
    return (uint32_t) Type::RadialGradient;
}


Type RadialGradient::type() const noexcept
{
    return Type::RadialGradient;
}


LinearGradient::LinearGradient():pImpl(new Impl())
{
    Fill::pImpl->method(new FillDup<LinearGradient::Impl>(pImpl));
}


LinearGradient::~LinearGradient()
{
    delete(pImpl);
}


Result LinearGradient::linear(float x1, float y1, float x2, float y2) noexcept
{
    pImpl->x1 = x1;
    pImpl->y1 = y1;
    pImpl->x2 = x2;
    pImpl->y2 = y2;

    return Result::Success;
}


Result LinearGradient::linear(float* x1, float* y1, float* x2, float* y2) const noexcept
{
    if (x1) *x1 = pImpl->x1;
    if (x2) *x2 = pImpl->x2;
    if (y1) *y1 = pImpl->y1;
    if (y2) *y2 = pImpl->y2;

    return Result::Success;
}


unique_ptr<LinearGradient> LinearGradient::gen() noexcept
{
    return unique_ptr<LinearGradient>(new LinearGradient);
}


TVG_DEPRECATED uint32_t LinearGradient::identifier() noexcept
{
    return (uint32_t) Type::LinearGradient;
}


Type LinearGradient::type() const noexcept
{
    return Type::LinearGradient;
}

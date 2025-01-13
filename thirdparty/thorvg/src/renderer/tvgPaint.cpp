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

#include "tvgMath.h"
#include "tvgPaint.h"
#include "tvgShape.h"
#include "tvgPicture.h"
#include "tvgScene.h"
#include "tvgText.h"

/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/

#define PAINT_METHOD(ret, METHOD) \
    switch (paint->type()) { \
        case Type::Shape: ret = P((Shape*)paint)->METHOD; break; \
        case Type::Scene: ret = P((Scene*)paint)->METHOD; break; \
        case Type::Picture: ret = P((Picture*)paint)->METHOD; break; \
        case Type::Text: ret = P((Text*)paint)->METHOD; break; \
        default: ret = {}; \
    }


static Result _clipRect(RenderMethod* renderer, const Point* pts, const Matrix& pm, const Matrix& rm, RenderRegion& before)
{
    //sorting
    Point tmp[4];
    Point min = {FLT_MAX, FLT_MAX};
    Point max = {0.0f, 0.0f};

    for (int i = 0; i < 4; ++i) {
        tmp[i] = pts[i];
        tmp[i] *= rm;
        tmp[i] *= pm;
        if (tmp[i].x < min.x) min.x = tmp[i].x;
        if (tmp[i].x > max.x) max.x = tmp[i].x;
        if (tmp[i].y < min.y) min.y = tmp[i].y;
        if (tmp[i].y > max.y) max.y = tmp[i].y;
    }

    float region[4] = {float(before.x), float(before.x + before.w), float(before.y), float(before.y + before.h)};

    //figure out if the clipper is a superset of the current viewport(before) region
    if (min.x <= region[0] && max.x >= region[1] && min.y <= region[2] && max.y >= region[3]) {
        //viewport region is same, nothing to do.
        return Result::Success;
    //figure out if the clipper is totally outside of the viewport
    } else if (max.x <= region[0] || min.x >= region[1] || max.y <= region[2] || min.y >= region[3]) {
        renderer->viewport({0, 0, 0, 0});
        return Result::Success;
    }
    return Result::InsufficientCondition;
}


static Result _compFastTrack(RenderMethod* renderer, Paint* cmpTarget, const Matrix& pm, RenderRegion& before)
{
    /* Access Shape class by Paint is bad... but it's ok still it's an internal usage. */
    auto shape = static_cast<Shape*>(cmpTarget);

    //Rectangle Candidates?
    const Point* pts;
    auto ptsCnt = shape->pathCoords(&pts);

    //nothing to clip
    if (ptsCnt == 0) return Result::InvalidArguments;
    if (ptsCnt != 4) return Result::InsufficientCondition;

    auto& rm = P(cmpTarget)->transform();

    //No rotation and no skewing, still can try out clipping the rect region.
    auto tryClip = false;

    if ((!rightAngle(pm) || skewed(pm))) tryClip = true;
    if ((!rightAngle(rm) || skewed(rm))) tryClip = true;

    if (tryClip) return _clipRect(renderer, pts, pm, rm, before);

    //Perpendicular Rectangle?
    auto pt1 = pts + 0;
    auto pt2 = pts + 1;
    auto pt3 = pts + 2;
    auto pt4 = pts + 3;

    if ((tvg::equal(pt1->x, pt2->x) && tvg::equal(pt2->y, pt3->y) && tvg::equal(pt3->x, pt4->x) && tvg::equal(pt1->y, pt4->y)) ||
        (tvg::equal(pt2->x, pt3->x) && tvg::equal(pt1->y, pt2->y) && tvg::equal(pt1->x, pt4->x) && tvg::equal(pt3->y, pt4->y))) {

        RenderRegion after;

        auto v1 = *pt1;
        auto v2 = *pt3;
        v1 *= rm;
        v2 *= rm;
        v1 *= pm;
        v2 *= pm;

        //sorting
        if (v1.x > v2.x) std::swap(v1.x, v2.x);
        if (v1.y > v2.y) std::swap(v1.y, v2.y);

        after.x = static_cast<int32_t>(v1.x);
        after.y = static_cast<int32_t>(v1.y);
        after.w = static_cast<int32_t>(ceil(v2.x - after.x));
        after.h = static_cast<int32_t>(ceil(v2.y - after.y));

        if (after.w < 0) after.w = 0;
        if (after.h < 0) after.h = 0;

        after.intersect(before);
        renderer->viewport(after);

        return Result::Success;
    }
    return Result::InsufficientCondition;
}


RenderRegion Paint::Impl::bounds(RenderMethod* renderer) const
{
    RenderRegion ret;
    PAINT_METHOD(ret, bounds(renderer));
    return ret;
}


Iterator* Paint::Impl::iterator()
{
    Iterator* ret;
    PAINT_METHOD(ret, iterator());
    return ret;
}


Paint* Paint::Impl::duplicate(Paint* ret)
{
    if (ret) ret->composite(nullptr, CompositeMethod::None);

    PAINT_METHOD(ret, duplicate(ret));

    //duplicate Transform
    ret->pImpl->tr = tr;
    ret->pImpl->renderFlag |= RenderUpdateFlag::Transform;

    ret->pImpl->opacity = opacity;

    if (compData) ret->pImpl->composite(ret, compData->target->duplicate(), compData->method);
    if (clipper) ret->pImpl->clip(clipper->duplicate());

    return ret;
}


bool Paint::Impl::rotate(float degree)
{
    if (tr.overriding) return false;
    if (tvg::equal(degree, tr.degree)) return true;
    tr.degree = degree;
    renderFlag |= RenderUpdateFlag::Transform;

    return true;
}


bool Paint::Impl::scale(float factor)
{
    if (tr.overriding) return false;
    if (tvg::equal(factor, tr.scale)) return true;
    tr.scale = factor;
    renderFlag |= RenderUpdateFlag::Transform;

    return true;
}


bool Paint::Impl::translate(float x, float y)
{
    if (tr.overriding) return false;
    if (tvg::equal(x, tr.m.e13) && tvg::equal(y, tr.m.e23)) return true;
    tr.m.e13 = x;
    tr.m.e23 = y;
    renderFlag |= RenderUpdateFlag::Transform;

    return true;
}


bool Paint::Impl::render(RenderMethod* renderer)
{
    if (opacity == 0) return true;

    RenderCompositor* cmp = nullptr;

    if (compData && !(compData->target->pImpl->ctxFlag & ContextFlag::FastTrack)) {
        RenderRegion region;
        PAINT_METHOD(region, bounds(renderer));

        if (MASK_REGION_MERGING(compData->method)) region.add(P(compData->target)->bounds(renderer));
        if (region.w == 0 || region.h == 0) return true;
        cmp = renderer->target(region, COMPOSITE_TO_COLORSPACE(renderer, compData->method));
        if (renderer->beginComposite(cmp, CompositeMethod::None, 255)) {
            compData->target->pImpl->render(renderer);
        }
    }

    if (cmp) renderer->beginComposite(cmp, compData->method, compData->target->pImpl->opacity);

    bool ret;
    PAINT_METHOD(ret, render(renderer));

    if (cmp) renderer->endComposite(cmp);

    return ret;
}


RenderData Paint::Impl::update(RenderMethod* renderer, const Matrix& pm, Array<RenderData>& clips, uint8_t opacity, RenderUpdateFlag pFlag, bool clipper)
{
    if (this->renderer != renderer) {
        if (this->renderer) TVGERR("RENDERER", "paint's renderer has been changed!");
        renderer->ref();
        this->renderer = renderer;
    }

    if (renderFlag & RenderUpdateFlag::Transform) tr.update();

    /* 1. Composition Pre Processing */
    RenderData trd = nullptr;                 //composite target render data
    RenderRegion viewport;
    Result compFastTrack = Result::InsufficientCondition;

    if (compData) {
        auto target = compData->target;
        auto method = compData->method;
        P(target)->ctxFlag &= ~ContextFlag::FastTrack;   //reset

        /* If the transformation has no rotational factors and the Alpha(InvAlpha)Masking involves a simple rectangle,
           we can optimize by using the viewport instead of the regular AlphaMasking sequence for improved performance. */
        if (target->type() == Type::Shape) {
            auto shape = static_cast<Shape*>(target);
            uint8_t a;
            shape->fillColor(nullptr, nullptr, nullptr, &a);
            //no gradient fill & no compositions of the composition target.
            if (!shape->fill() && !(PP(shape)->compData)) {
                if ((method == CompositeMethod::AlphaMask && a == 255 && PP(shape)->opacity == 255) || (method == CompositeMethod::InvAlphaMask && (a == 0 || PP(shape)->opacity == 0))) {
                    viewport = renderer->viewport();
                    if ((compFastTrack = _compFastTrack(renderer, target, pm, viewport)) == Result::Success) {
                        P(target)->ctxFlag |= ContextFlag::FastTrack;
                    }
                }
            }
        }
        if (compFastTrack == Result::InsufficientCondition) {
            trd = P(target)->update(renderer, pm, clips, 255, pFlag, false);
        }
    }

    /* 2. Clipping */
    if (this->clipper) {
        P(this->clipper)->ctxFlag &= ~ContextFlag::FastTrack;   //reset
        viewport = renderer->viewport();
        /* TODO: Intersect the clipper's clipper, if both are FastTrack.
           Update the subsequent clipper first and check its ctxFlag. */
        if (!P(this->clipper)->clipper && (compFastTrack = _compFastTrack(renderer, this->clipper, pm, viewport)) == Result::Success) {
            P(this->clipper)->ctxFlag |= ContextFlag::FastTrack;
        }
        if (compFastTrack == Result::InsufficientCondition) {
            trd = P(this->clipper)->update(renderer, pm, clips, 255, pFlag, true);
            clips.push(trd);
        }
    }

    /* 3. Main Update */
    auto newFlag = static_cast<RenderUpdateFlag>(pFlag | renderFlag);
    renderFlag = RenderUpdateFlag::None;
    opacity = MULTIPLY(opacity, this->opacity);

    RenderData rd = nullptr;

    tr.cm = pm * tr.m;
    PAINT_METHOD(rd, update(renderer, tr.cm, clips, opacity, newFlag, clipper));

    /* 4. Composition Post Processing */
    if (compFastTrack == Result::Success) renderer->viewport(viewport);
    else if (this->clipper) clips.pop();

    return rd;
}


bool Paint::Impl::bounds(float* x, float* y, float* w, float* h, bool transformed, bool stroking, bool origin)
{
    bool ret;
    const auto& m = this->transform(origin);

    //Case: No transformed, quick return!
    if (!transformed || identity(&m)) {
        PAINT_METHOD(ret, bounds(x, y, w, h, stroking));
        return ret;
    }

    //Case: Transformed
    auto tx = 0.0f;
    auto ty = 0.0f;
    auto tw = 0.0f;
    auto th = 0.0f;

    PAINT_METHOD(ret, bounds(&tx, &ty, &tw, &th, stroking));

    //Get vertices
    Point pt[4] = {{tx, ty}, {tx + tw, ty}, {tx + tw, ty + th}, {tx, ty + th}};

    //New bounding box
    auto x1 = FLT_MAX;
    auto y1 = FLT_MAX;
    auto x2 = -FLT_MAX;
    auto y2 = -FLT_MAX;

    //Compute the AABB after transformation
    for (int i = 0; i < 4; i++) {
        pt[i] *= m;

        if (pt[i].x < x1) x1 = pt[i].x;
        if (pt[i].x > x2) x2 = pt[i].x;
        if (pt[i].y < y1) y1 = pt[i].y;
        if (pt[i].y > y2) y2 = pt[i].y;
    }

    if (x) *x = x1;
    if (y) *y = y1;
    if (w) *w = x2 - x1;
    if (h) *h = y2 - y1;

    return ret;
}


void Paint::Impl::reset()
{
    if (clipper) {
        delete(clipper);
        clipper = nullptr;
    }

    if (compData) {
        if (P(compData->target)->unref() == 0) delete(compData->target);
        free(compData);
        compData = nullptr;
    }

    tvg::identity(&tr.m);
    tr.degree = 0.0f;
    tr.scale = 1.0f;
    tr.overriding = false;

    blendMethod = BlendMethod::Normal;
    renderFlag = RenderUpdateFlag::None;
    ctxFlag = ContextFlag::Invalid;
    opacity = 255;
    paint->id = 0;
}


/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/

Paint :: Paint() : pImpl(new Impl(this))
{
}


Paint :: ~Paint()
{
    delete(pImpl);
}


Result Paint::rotate(float degree) noexcept
{
    if (pImpl->rotate(degree)) return Result::Success;
    return Result::InsufficientCondition;
}


Result Paint::scale(float factor) noexcept
{
    if (pImpl->scale(factor)) return Result::Success;
    return Result::InsufficientCondition;
}


Result Paint::translate(float x, float y) noexcept
{
    if (pImpl->translate(x, y)) return Result::Success;
    return Result::InsufficientCondition;
}


Result Paint::transform(const Matrix& m) noexcept
{
    if (pImpl->transform(m)) return Result::Success;
    return Result::InsufficientCondition;
}


Matrix Paint::transform() noexcept
{
    return pImpl->transform();
}


TVG_DEPRECATED Result Paint::bounds(float* x, float* y, float* w, float* h) const noexcept
{
    return this->bounds(x, y, w, h, false);
}


Result Paint::bounds(float* x, float* y, float* w, float* h, bool transformed) const noexcept
{
    if (pImpl->bounds(x, y, w, h, transformed, true, transformed)) return Result::Success;
    return Result::InsufficientCondition;
}


Paint* Paint::duplicate() const noexcept
{
    return pImpl->duplicate();
}


Result Paint::clip(std::unique_ptr<Paint> clipper) noexcept
{
    auto p = clipper.release();

    if (p && p->type() != Type::Shape) {
        TVGERR("RENDERER", "Clipping only supports the Shape!");
        return Result::NonSupport;
    }
    pImpl->clip(p);
    return Result::Success;
}


Result Paint::composite(std::unique_ptr<Paint> target, CompositeMethod method) noexcept
{
    //TODO: remove. Keep this for the backward compatibility
    if (target && method == CompositeMethod::ClipPath) return clip(std::move(target));

    auto p = target.release();
    if (pImpl->composite(this, p, method)) return Result::Success;

    delete(p);
    return Result::InvalidArguments;
}


CompositeMethod Paint::composite(const Paint** target) const noexcept
{
    if (pImpl->compData) {
        if (target) *target = pImpl->compData->target;
        return pImpl->compData->method;
    } else {
        //TODO: remove. Keep this for the backward compatibility
        if (pImpl->clipper) {
            if (target) *target = pImpl->clipper;
            return CompositeMethod::ClipPath;
        }
        if (target) *target = nullptr;
        return CompositeMethod::None;
    }
}


Result Paint::opacity(uint8_t o) noexcept
{
    if (pImpl->opacity == o) return Result::Success;

    pImpl->opacity = o;
    pImpl->renderFlag |= RenderUpdateFlag::Color;

    return Result::Success;
}


uint8_t Paint::opacity() const noexcept
{
    return pImpl->opacity;
}


TVG_DEPRECATED uint32_t Paint::identifier() const noexcept
{
    return (uint32_t) type();
}


Result Paint::blend(BlendMethod method) noexcept
{
    //TODO: Remove later
    if (method == BlendMethod::Hue || method == BlendMethod::Saturation || method == BlendMethod::Color || method == BlendMethod::Luminosity || method == BlendMethod::HardMix) return Result::NonSupport;

    if (pImpl->blendMethod != method) {
        pImpl->blendMethod = method;
        pImpl->renderFlag |= RenderUpdateFlag::Blend;
    }

    return Result::Success;
}

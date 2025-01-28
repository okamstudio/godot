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

#ifdef THORVG_SW_OPENMP_SUPPORT
    #include <omp.h>
#endif
#include <algorithm>
#include "tvgMath.h"
#include "tvgSwCommon.h"
#include "tvgTaskScheduler.h"
#include "tvgSwRenderer.h"

/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/
static int32_t initEngineCnt = false;
static int32_t rendererCnt = 0;
static SwMpool* globalMpool = nullptr;
static uint32_t threadsCnt = 0;

struct SwTask : Task
{
    SwSurface* surface = nullptr;
    SwMpool* mpool = nullptr;
    SwBBox bbox;                          //Rendering Region
    Matrix transform;
    Array<RenderData> clips;
    RenderUpdateFlag flags = RenderUpdateFlag::None;
    uint8_t opacity;
    bool pushed = false;                  //Pushed into task list?
    bool disposed = false;                //Disposed task?

    RenderRegion bounds()
    {
        //Can we skip the synchronization?
        done();

        RenderRegion region;

        //Range over?
        region.x = bbox.min.x > 0 ? bbox.min.x : 0;
        region.y = bbox.min.y > 0 ? bbox.min.y : 0;
        region.w = bbox.max.x - region.x;
        region.h = bbox.max.y - region.y;
        if (region.w < 0) region.w = 0;
        if (region.h < 0) region.h = 0;

        return region;
    }

    virtual void dispose() = 0;
    virtual bool clip(SwRle* target) = 0;
    virtual ~SwTask() {}
};


struct SwShapeTask : SwTask
{
    SwShape shape;
    const RenderShape* rshape = nullptr;
    bool clipper = false;

    /* We assume that if the stroke width is greater than 2,
       the shape's outline beneath the stroke could be adequately covered by the stroke drawing.
       Therefore, antialiasing is disabled under this condition.
       Additionally, the stroke style should not be dashed. */
    bool antialiasing(float strokeWidth)
    {
        return strokeWidth < 2.0f || rshape->stroke->dashCnt > 0 || rshape->stroke->strokeFirst || rshape->strokeTrim() || rshape->stroke->color[3] < 255;;
    }

    float validStrokeWidth()
    {
        if (!rshape->stroke) return 0.0f;

        auto width = rshape->stroke->width;
        if (tvg::zero(width)) return 0.0f;

        if (!rshape->stroke->fill && (MULTIPLY(rshape->stroke->color[3], opacity) == 0)) return 0.0f;
        if (tvg::zero(rshape->stroke->trim.begin - rshape->stroke->trim.end)) return 0.0f;

        return (width * sqrt(transform.e11 * transform.e11 + transform.e12 * transform.e12));
    }

    bool clip(SwRle* target) override
    {
        if (shape.fastTrack) return rleClip(target, &bbox);
        else if (shape.rle) return rleClip(target, shape.rle);
        return false;
    }

    void run(unsigned tid) override
    {
        //Invisible
        if (opacity == 0 && !clipper) {
            bbox.reset();
            return;
        }

        auto strokeWidth = validStrokeWidth();
        SwBBox renderRegion{};
        auto visibleFill = false;

        //This checks also for the case, if the invisible shape turned to visible by alpha.
        auto prepareShape = false;
        if (!shapePrepared(&shape) && (flags & RenderUpdateFlag::Color)) prepareShape = true;

        //Shape
        if (flags & (RenderUpdateFlag::Path | RenderUpdateFlag::Transform) || prepareShape) {
            uint8_t alpha = 0;
            rshape->fillColor(nullptr, nullptr, nullptr, &alpha);
            alpha = MULTIPLY(alpha, opacity);
            visibleFill = (alpha > 0 || rshape->fill);
            shapeReset(&shape);
            if (visibleFill || clipper) {
                if (!shapePrepare(&shape, rshape, transform, bbox, renderRegion, mpool, tid, clips.count > 0 ? true : false)) {
                    visibleFill = false;
                    renderRegion.reset();
                }
            }
        }
        //Fill
        if (flags & (RenderUpdateFlag::Path |RenderUpdateFlag::Gradient | RenderUpdateFlag::Transform | RenderUpdateFlag::Color)) {
            if (visibleFill || clipper) {
                if (!shapeGenRle(&shape, rshape, antialiasing(strokeWidth))) goto err;
            }
            if (auto fill = rshape->fill) {
                auto ctable = (flags & RenderUpdateFlag::Gradient) ? true : false;
                if (ctable) shapeResetFill(&shape);
                if (!shapeGenFillColors(&shape, fill, transform, surface, opacity, ctable)) goto err;
            } else {
                shapeDelFill(&shape);
            }
        }
        //Stroke
        if (flags & (RenderUpdateFlag::Path | RenderUpdateFlag::Stroke | RenderUpdateFlag::Transform)) {
            if (strokeWidth > 0.0f) {
                shapeResetStroke(&shape, rshape, transform);

                if (!shapeGenStrokeRle(&shape, rshape, transform, bbox, renderRegion, mpool, tid)) goto err;
                if (auto fill = rshape->strokeFill()) {
                    auto ctable = (flags & RenderUpdateFlag::GradientStroke) ? true : false;
                    if (ctable) shapeResetStrokeFill(&shape);
                    if (!shapeGenStrokeFillColors(&shape, fill, transform, surface, opacity, ctable)) goto err;
                } else {
                    shapeDelStrokeFill(&shape);
                }
            } else {
                shapeDelStroke(&shape);
            }
        }

        //Clear current task memorypool here if the clippers would use the same memory pool
        shapeDelOutline(&shape, mpool, tid);

        //Clip Path
        for (auto clip = clips.begin(); clip < clips.end(); ++clip) {
            auto clipper = static_cast<SwTask*>(*clip);
            if (shape.rle && !clipper->clip(shape.rle)) goto err;                 //Clip shape rle
            if (shape.strokeRle && !clipper->clip(shape.strokeRle)) goto err;     //Clip stroke rle
        }

        bbox = renderRegion; //sync

        return;

    err:
        bbox.reset();
        shapeReset(&shape);
        shapeDelOutline(&shape, mpool, tid);
    }

    void dispose() override
    {
       shapeFree(&shape);
    }
};


struct SwImageTask : SwTask
{
    SwImage image;
    RenderSurface* source;                //Image source

    bool clip(SwRle* target) override
    {
        TVGERR("SW_ENGINE", "Image is used as ClipPath?");
        return true;
    }

    void run(unsigned tid) override
    {
        auto clipRegion = bbox;

        //Convert colorspace if it's not aligned.
        rasterConvertCS(source, surface->cs);
        rasterPremultiply(source);

        image.data = source->data;
        image.w = source->w;
        image.h = source->h;
        image.stride = source->stride;
        image.channelSize = source->channelSize;

        //Invisible shape turned to visible by alpha.
        if ((flags & (RenderUpdateFlag::Image | RenderUpdateFlag::Transform | RenderUpdateFlag::Color)) && (opacity > 0)) {
            imageReset(&image);
            if (!image.data || image.w == 0 || image.h == 0) goto end;

            if (!imagePrepare(&image, transform, clipRegion, bbox, mpool, tid)) goto end;

            if (clips.count > 0) {
                if (!imageGenRle(&image, bbox, false)) goto end;
                if (image.rle) {
                    //Clear current task memorypool here if the clippers would use the same memory pool
                    imageDelOutline(&image, mpool, tid);
                    for (auto clip = clips.begin(); clip < clips.end(); ++clip) {
                        auto clipper = static_cast<SwTask*>(*clip);
                        if (!clipper->clip(image.rle)) goto err;
                    }
                    return;
                }
            }
        }
        goto end;
    err:
        rleReset(image.rle);
    end:
        imageDelOutline(&image, mpool, tid);
    }

    void dispose() override
    {
       imageFree(&image);
    }
};


static void _termEngine()
{
    if (rendererCnt > 0) return;

    mpoolTerm(globalMpool);
    globalMpool = nullptr;
}


static void _renderFill(SwShapeTask* task, SwSurface* surface, uint8_t opacity)
{
    uint8_t r, g, b, a;
    if (auto fill = task->rshape->fill) {
        rasterGradientShape(surface, &task->shape, fill, opacity);
    } else {
        task->rshape->fillColor(&r, &g, &b, &a);
        a = MULTIPLY(opacity, a);
        if (a > 0) rasterShape(surface, &task->shape, r, g, b, a);
    }
}

static void _renderStroke(SwShapeTask* task, SwSurface* surface, uint8_t opacity)
{
    uint8_t r, g, b, a;
    if (auto strokeFill = task->rshape->strokeFill()) {
        rasterGradientStroke(surface, &task->shape, strokeFill, opacity);
    } else {
        if (task->rshape->strokeColor(&r, &g, &b, &a)) {
            a = MULTIPLY(opacity, a);
            if (a > 0) rasterStroke(surface, &task->shape, r, g, b, a);
        }
    }
}

/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/

SwRenderer::~SwRenderer()
{
    clearCompositors();

    delete(surface);

    if (!sharedMpool) mpoolTerm(mpool);

    --rendererCnt;

    if (rendererCnt == 0 && initEngineCnt == 0) _termEngine();
}


bool SwRenderer::clear()
{
    for (auto task = tasks.begin(); task < tasks.end(); ++task) {
        if ((*task)->disposed) {
            delete(*task);
        } else {
            (*task)->done();
            (*task)->pushed = false;
        }
    }
    tasks.clear();

    if (!sharedMpool) mpoolClear(mpool);

    if (surface) {
        vport.x = vport.y = 0;
        vport.w = surface->w;
        vport.h = surface->h;
    }

    return true;
}


bool SwRenderer::sync()
{
    return true;
}


RenderRegion SwRenderer::viewport()
{
    return vport;
}


bool SwRenderer::viewport(const RenderRegion& vp)
{
    vport = vp;
    return true;
}


bool SwRenderer::target(pixel_t* data, uint32_t stride, uint32_t w, uint32_t h, ColorSpace cs)
{
    if (!data || stride == 0 || w == 0 || h == 0 || w > stride) return false;

    clearCompositors();

    if (!surface) surface = new SwSurface;

    surface->data = data;
    surface->stride = stride;
    surface->w = w;
    surface->h = h;
    surface->cs = cs;
    surface->channelSize = CHANNEL_SIZE(cs);
    surface->premultiplied = true;

    return rasterCompositor(surface);
}


bool SwRenderer::preRender()
{
    return rasterClear(surface, 0, 0, surface->w, surface->h);
}


void SwRenderer::clearCompositors()
{
    //Free Composite Caches
    for (auto comp = compositors.begin(); comp < compositors.end(); ++comp) {
        free((*comp)->compositor->image.data);
        delete((*comp)->compositor);
        delete(*comp);
    }
    compositors.reset();
}


bool SwRenderer::postRender()
{
    //Unmultiply alpha if needed
    if (surface->cs == ColorSpace::ABGR8888S || surface->cs == ColorSpace::ARGB8888S) {
        rasterUnpremultiply(surface);
    }

    for (auto task = tasks.begin(); task < tasks.end(); ++task) {
        if ((*task)->disposed) delete(*task);
        else (*task)->pushed = false;
    }
    tasks.clear();

    return true;
}


bool SwRenderer::renderImage(RenderData data)
{
    auto task = static_cast<SwImageTask*>(data);
    task->done();

    if (task->opacity == 0) return true;

    return rasterImage(surface, &task->image, task->transform, task->bbox, task->opacity);
}


bool SwRenderer::renderShape(RenderData data)
{
    auto task = static_cast<SwShapeTask*>(data);
    if (!task) return false;

    task->done();

    if (task->opacity == 0) return true;

    //Main raster stage
    if (task->rshape->stroke && task->rshape->stroke->strokeFirst) {
        _renderStroke(task, surface, task->opacity);
        _renderFill(task, surface, task->opacity);
    } else {
        _renderFill(task, surface, task->opacity);
        _renderStroke(task, surface, task->opacity);
    }

    return true;
}


bool SwRenderer::blend(BlendMethod method)
{
    if (surface->blendMethod == method) return true;
    surface->blendMethod = method;

    switch (method) {
        case BlendMethod::Normal:
            surface->blender = nullptr;
            break;
        case BlendMethod::Multiply:
            surface->blender = opBlendMultiply;
            break;
        case BlendMethod::Screen:
            surface->blender = opBlendScreen;
            break;
        case BlendMethod::Overlay:
            surface->blender = opBlendOverlay;
            break;
        case BlendMethod::Darken:
            surface->blender = opBlendDarken;
            break;
        case BlendMethod::Lighten:
            surface->blender = opBlendLighten;
            break;
        case BlendMethod::ColorDodge:
            surface->blender = opBlendColorDodge;
            break;
        case BlendMethod::ColorBurn:
            surface->blender = opBlendColorBurn;
            break;
        case BlendMethod::HardLight:
            surface->blender = opBlendHardLight;
            break;
        case BlendMethod::SoftLight:
            surface->blender = opBlendSoftLight;
            break;
        case BlendMethod::Difference:
            surface->blender = opBlendDifference;
            break;
        case BlendMethod::Exclusion:
            surface->blender = opBlendExclusion;
            break;
        case BlendMethod::Add:
            surface->blender = opBlendAdd;
            break;
        default:
            TVGLOG("SW_ENGINE", "Non supported blending option = %d", (int) method);
            surface->blender = nullptr;
            break;
    }
    return false;
}


RenderRegion SwRenderer::region(RenderData data)
{
    return static_cast<SwTask*>(data)->bounds();
}


bool SwRenderer::beginComposite(RenderCompositor* cmp, CompositeMethod method, uint8_t opacity)
{
    if (!cmp) return false;
    auto p = static_cast<SwCompositor*>(cmp);

    p->method = method;
    p->opacity = opacity;

    //Current Context?
    if (p->method != CompositeMethod::None) {
        surface = p->recoverSfc;
        surface->compositor = p;
    }

    return true;
}


bool SwRenderer::mempool(bool shared)
{
    if (shared == sharedMpool) return true;

    if (shared) {
        if (!sharedMpool) {
            if (!mpoolTerm(mpool)) return false;
            mpool = globalMpool;
        }
    } else {
        if (sharedMpool) mpool = mpoolInit(threadsCnt);
    }

    sharedMpool = shared;

    if (mpool) return true;
    return false;
}


const RenderSurface* SwRenderer::mainSurface()
{
    return surface;
}


SwSurface* SwRenderer::request(int channelSize, bool square)
{
    SwSurface* cmp = nullptr;
    uint32_t w, h;

    if (square) {
        //Same Dimensional Size is demanded for the Post Processing Fast Flipping
        w = h = std::max(surface->w, surface->h);
    } else {
        w = surface->w;
        h = surface->h;
    }

    //Use cached data
    for (auto p = compositors.begin(); p < compositors.end(); ++p) {
        auto cur = *p;
        if (cur->compositor->valid && cur->compositor->image.channelSize == channelSize) {
            if (w == cur->w && h == cur->h) {
                cmp = *p;
                break;
            }
        }
    }

    //New Composition
    if (!cmp) {
        //Inherits attributes from main surface
        cmp = new SwSurface(surface);
        cmp->compositor = new SwCompositor;
        cmp->compositor->image.data = (pixel_t*)malloc(channelSize * w * h);
        cmp->w = cmp->compositor->image.w = w;
        cmp->h = cmp->compositor->image.h = h;
        cmp->compositor->image.stride = w;
        cmp->compositor->image.direct = true;
        cmp->compositor->valid = true;
        cmp->channelSize = cmp->compositor->image.channelSize = channelSize;

        compositors.push(cmp);
    }

    //Sync. This may have been modified by post-processing.
    cmp->data = cmp->compositor->image.data;

    return cmp;
}


RenderCompositor* SwRenderer::target(const RenderRegion& region, ColorSpace cs, CompositionFlag flags)
{
    auto x = region.x;
    auto y = region.y;
    auto w = region.w;
    auto h = region.h;
    auto sw = static_cast<int32_t>(surface->w);
    auto sh = static_cast<int32_t>(surface->h);

    //Out of boundary
    if (x >= sw || y >= sh || x + w < 0 || y + h < 0) return nullptr;

    auto cmp = request(CHANNEL_SIZE(cs), (flags & CompositionFlag::PostProcessing));

    //Boundary Check
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x + w > sw) w = (sw - x);
    if (y + h > sh) h = (sh - y);

    if (w == 0 || h == 0) return nullptr;

    cmp->compositor->recoverSfc = surface;
    cmp->compositor->recoverCmp = surface->compositor;
    cmp->compositor->valid = false;
    cmp->compositor->bbox.min.x = x;
    cmp->compositor->bbox.min.y = y;
    cmp->compositor->bbox.max.x = x + w;
    cmp->compositor->bbox.max.y = y + h;

    /* TODO: Currently, only blending might work.
       Blending and composition must be handled together. */
    auto color = (surface->blender && !surface->compositor) ? 0x00ffffff : 0x00000000;
    rasterClear(cmp, x, y, w, h, color);

    //Switch render target
    surface = cmp;

    return cmp->compositor;
}


bool SwRenderer::endComposite(RenderCompositor* cmp)
{
    if (!cmp) return false;

    auto p = static_cast<SwCompositor*>(cmp);

    //Recover Context
    surface = p->recoverSfc;
    surface->compositor = p->recoverCmp;

    //only invalid (currently used) surface can be composited
    if (p->valid) return true;
    p->valid = true;

    //Default is alpha blending
    if (p->method == CompositeMethod::None) {
        Matrix m = {1, 0, 0, 0, 1, 0, 0, 0, 1};
        return rasterImage(surface, &p->image, m, p->bbox, p->opacity);
    }

    return true;
}


bool SwRenderer::prepare(RenderEffect* effect)
{
    switch (effect->type) {
        case SceneEffect::GaussianBlur: return effectGaussianBlurPrepare(static_cast<RenderEffectGaussianBlur*>(effect));
        case SceneEffect::DropShadow: return effectDropShadowPrepare(static_cast<RenderEffectDropShadow*>(effect));
        case SceneEffect::Fill: return effectFillPrepare(static_cast<RenderEffectFill*>(effect));
        case SceneEffect::Tint: return effectTintPrepare(static_cast<RenderEffectTint*>(effect));
        case SceneEffect::Tritone: return effectTritonePrepare(static_cast<RenderEffectTritone*>(effect));
        default: return false;
    }
}


bool SwRenderer::effect(RenderCompositor* cmp, const RenderEffect* effect, bool direct)
{
    if (!effect->valid) return false;

    auto p = static_cast<SwCompositor*>(cmp);

    if (p->image.channelSize != sizeof(uint32_t)) {
        TVGERR("SW_ENGINE", "Not supported grayscale Gaussian Blur!");
        return false;
    }

    switch (effect->type) {
        case SceneEffect::GaussianBlur: {
            return effectGaussianBlur(p, request(surface->channelSize, true), static_cast<const RenderEffectGaussianBlur*>(effect));
        }
        case SceneEffect::DropShadow: {
            auto cmp1 = request(surface->channelSize, true);
            cmp1->compositor->valid = false;
            auto cmp2 = request(surface->channelSize, true);
            SwSurface* surfaces[] = {cmp1, cmp2};
            auto ret = effectDropShadow(p, surfaces, static_cast<const RenderEffectDropShadow*>(effect), direct);
            cmp1->compositor->valid = true;
            return ret;
        }
        case SceneEffect::Fill: {
            return effectFill(p, static_cast<const RenderEffectFill*>(effect), direct);
        }
        case SceneEffect::Tint: {
            return effectTint(p, static_cast<const RenderEffectTint*>(effect), direct);
        }
        case SceneEffect::Tritone: {
            return effectTritone(p, static_cast<const RenderEffectTritone*>(effect), direct);
        }
        default: return false;
    }
}


ColorSpace SwRenderer::colorSpace()
{
    if (surface) return surface->cs;
    else return ColorSpace::Unsupported;
}


void SwRenderer::dispose(RenderData data)
{
    auto task = static_cast<SwTask*>(data);
    if (!task) return;
    task->done();
    task->dispose();

    if (task->pushed) task->disposed = true;
    else delete(task);
}


void* SwRenderer::prepareCommon(SwTask* task, const Matrix& transform, const Array<RenderData>& clips, uint8_t opacity, RenderUpdateFlag flags)
{
    if (!surface) return task;
    if (flags == RenderUpdateFlag::None) return task;

    //TODO: Failed threading them. It would be better if it's possible.
    //See: https://github.com/thorvg/thorvg/issues/1409
    //Guarantee composition targets get ready.
    for (auto clip = clips.begin(); clip < clips.end(); ++clip) {
        static_cast<SwTask*>(*clip)->done();
    }

    task->clips = clips;
    task->transform = transform;
    
    //zero size?
    if (task->transform.e11 == 0.0f && task->transform.e12 == 0.0f) return task; //zero width
    if (task->transform.e21 == 0.0f && task->transform.e22 == 0.0f) return task; //zero height

    task->opacity = opacity;
    task->surface = surface;
    task->mpool = mpool;
    task->flags = flags;
    task->bbox.min.x = std::max(static_cast<SwCoord>(0), static_cast<SwCoord>(vport.x));
    task->bbox.min.y = std::max(static_cast<SwCoord>(0), static_cast<SwCoord>(vport.y));
    task->bbox.max.x = std::min(static_cast<SwCoord>(surface->w), static_cast<SwCoord>(vport.x + vport.w));
    task->bbox.max.y = std::min(static_cast<SwCoord>(surface->h), static_cast<SwCoord>(vport.y + vport.h));

    if (!task->pushed) {
        task->pushed = true;
        tasks.push(task);
    }

    TaskScheduler::request(task);

    return task;
}


RenderData SwRenderer::prepare(RenderSurface* surface, RenderData data, const Matrix& transform, Array<RenderData>& clips, uint8_t opacity, RenderUpdateFlag flags)
{
    //prepare task
    auto task = static_cast<SwImageTask*>(data);
    if (!task) task = new SwImageTask;
    else task->done();

    task->source = surface;

    return prepareCommon(task, transform, clips, opacity, flags);
}


RenderData SwRenderer::prepare(const RenderShape& rshape, RenderData data, const Matrix& transform, Array<RenderData>& clips, uint8_t opacity, RenderUpdateFlag flags, bool clipper)
{
    //prepare task
    auto task = static_cast<SwShapeTask*>(data);
    if (!task) task = new SwShapeTask;
    else task->done();

    task->rshape = &rshape;
    task->clipper = clipper;

    return prepareCommon(task, transform, clips, opacity, flags);
}


SwRenderer::SwRenderer():mpool(globalMpool)
{
}


bool SwRenderer::init(uint32_t threads)
{
    if ((initEngineCnt++) > 0) return true;

    threadsCnt = threads;

    //Share the memory pool among the renderer
    globalMpool = mpoolInit(threads);
    if (!globalMpool) {
        --initEngineCnt;
        return false;
    }

    return true;
}


int32_t SwRenderer::init()
{
#ifdef THORVG_SW_OPENMP_SUPPORT
    omp_set_num_threads(TaskScheduler::threads());
#endif

    return initEngineCnt;
}


bool SwRenderer::term()
{
    if ((--initEngineCnt) > 0) return true;

    initEngineCnt = 0;

   _termEngine();

    return true;
}

SwRenderer* SwRenderer::gen()
{
    ++rendererCnt;
    return new SwRenderer();
}

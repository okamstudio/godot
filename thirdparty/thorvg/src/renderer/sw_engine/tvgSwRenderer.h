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

#ifndef _TVG_SW_RENDERER_H_
#define _TVG_SW_RENDERER_H_

#include "tvgRender.h"

struct SwSurface;
struct SwTask;
struct SwCompositor;
struct SwMpool;

namespace tvg
{

class SwRenderer : public RenderMethod
{
public:
    RenderData prepare(const RenderShape& rshape, RenderData data, const Matrix& transform, Array<RenderData>& clips, uint8_t opacity, RenderUpdateFlag flags, bool clipper) override;
    RenderData prepare(RenderSurface* surface, RenderData data, const Matrix& transform, Array<RenderData>& clips, uint8_t opacity, RenderUpdateFlag flags) override;
    bool preRender() override;
    bool renderShape(RenderData data) override;
    bool renderImage(RenderData data) override;
    bool postRender() override;
    void dispose(RenderData data) override;
    RenderRegion region(RenderData data) override;
    RenderRegion viewport() override;
    bool viewport(const RenderRegion& vp) override;
    bool blend(BlendMethod method) override;
    ColorSpace colorSpace() override;
    const RenderSurface* mainSurface() override;

    bool clear() override;
    bool sync() override;
    bool target(pixel_t* data, uint32_t stride, uint32_t w, uint32_t h, ColorSpace cs);
    bool mempool(bool shared);

    RenderCompositor* target(const RenderRegion& region, ColorSpace cs, CompositionFlag flags) override;
    bool beginComposite(RenderCompositor* cmp, CompositeMethod method, uint8_t opacity) override;
    bool endComposite(RenderCompositor* cmp) override;
    void clearCompositors();

    bool prepare(RenderEffect* effect) override;
    bool effect(RenderCompositor* cmp, const RenderEffect* effect, bool direct) override;

    static SwRenderer* gen();
    static bool init(uint32_t threads);
    static int32_t init();
    static bool term();

private:
    SwSurface*           surface = nullptr;           //active surface
    Array<SwTask*>       tasks;                       //async task list
    Array<SwSurface*>    compositors;                 //render targets cache list
    SwMpool*             mpool;                       //private memory pool
    RenderRegion         vport;                       //viewport
    bool                 sharedMpool = true;          //memory-pool behavior policy

    SwRenderer();
    ~SwRenderer();

    SwSurface* request(int channelSize, bool square);
    RenderData prepareCommon(SwTask* task, const Matrix& transform, const Array<RenderData>& clips, uint8_t opacity, RenderUpdateFlag flags);
};

}

#endif /* _TVG_SW_RENDERER_H_ */

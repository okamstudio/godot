/*
 * Copyright (c) 2023 - 2024 the ThorVG project. All rights reserved.

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

#ifndef _TVG_LOTTIE_BUILDER_H_
#define _TVG_LOTTIE_BUILDER_H_

#include "tvgCommon.h"
#include "tvgInlist.h"
#include "tvgPaint.h"
#include "tvgShape.h"
#include "tvgLottieExpressions.h"
#include "tvgLottieModifier.h"

struct LottieComposition;

struct RenderRepeater
{
    int cnt;
    Matrix transform;
    float offset;
    Point position;
    Point anchor;
    Point scale;
    float rotation;
    uint8_t startOpacity;
    uint8_t endOpacity;
    bool interpOpacity;
    bool inorder;
};

struct RenderContext
{
    INLIST_ITEM(RenderContext);

    Shape* propagator = nullptr;  //for propagating the shape properties excluding paths
    Shape* merging = nullptr;  //merging shapes if possible (if shapes have same properties)
    LottieObject** begin = nullptr; //iteration entry point
    Array<RenderRepeater> repeaters;
    Matrix* transform = nullptr;
    LottieRoundnessModifier* roundness = nullptr;
    LottieOffsetModifier* offsetPath = nullptr;
    bool fragmenting = false;  //render context has been fragmented by filling
    bool reqFragment = false;  //requirement to fragment the render context

    RenderContext(Shape* propagator)
    {
        P(propagator)->reset();
        PP(propagator)->ref();
        this->propagator = propagator;
    }

    ~RenderContext()
    {
        PP(propagator)->unref();
        free(transform);
        delete(roundness);
        delete(offsetPath);
    }

    RenderContext(const RenderContext& rhs, Shape* propagator, bool mergeable = false)
    {
        if (mergeable) merging = rhs.merging;
        PP(propagator)->ref();
        this->propagator = propagator;
        this->repeaters = rhs.repeaters;
        if (rhs.roundness) this->roundness = new LottieRoundnessModifier(rhs.roundness->r);
        if (rhs.offsetPath) this->offsetPath = new LottieOffsetModifier(rhs.offsetPath->offset, rhs.offsetPath->miterLimit, rhs.offsetPath->join);
    }
};

struct LottieBuilder
{
    LottieBuilder()
    {
        exps = LottieExpressions::instance();
    }

    ~LottieBuilder()
    {
        LottieExpressions::retrieve(exps);
    }

    bool update(LottieComposition* comp, float progress);
    void build(LottieComposition* comp);

private:
    void updateEffect(LottieLayer* layer, float frameNo);
    void updateLayer(LottieComposition* comp, Scene* scene, LottieLayer* layer, float frameNo);
    bool updateMatte(LottieComposition* comp, float frameNo, Scene* scene, LottieLayer* layer);
    void updatePrecomp(LottieComposition* comp, LottieLayer* precomp, float frameNo);
    void updateSolid(LottieLayer* layer);
    void updateImage(LottieGroup* layer);
    void updateText(LottieLayer* layer, float frameNo);
    void updateMaskings(LottieLayer* layer, float frameNo);
    void updateTransform(LottieLayer* layer, float frameNo);
    void updateChildren(LottieGroup* parent, float frameNo, Inlist<RenderContext>& contexts);
    void updateGroup(LottieGroup* parent, LottieObject** child, float frameNo, Inlist<RenderContext>& pcontexts, RenderContext* ctx);
    void updateTransform(LottieGroup* parent, LottieObject** child, float frameNo, Inlist<RenderContext>& contexts, RenderContext* ctx);
    void updateSolidFill(LottieGroup* parent, LottieObject** child, float frameNo, Inlist<RenderContext>& contexts, RenderContext* ctx);
    void updateSolidStroke(LottieGroup* parent, LottieObject** child, float frameNo, Inlist<RenderContext>& contexts, RenderContext* ctx);
    void updateGradientFill(LottieGroup* parent, LottieObject** child, float frameNo, Inlist<RenderContext>& contexts, RenderContext* ctx);
    void updateGradientStroke(LottieGroup* parent, LottieObject** child, float frameNo, Inlist<RenderContext>& contexts, RenderContext* ctx);
    void updateRect(LottieGroup* parent, LottieObject** child, float frameNo, Inlist<RenderContext>& contexts, RenderContext* ctx);
    void updateEllipse(LottieGroup* parent, LottieObject** child, float frameNo, Inlist<RenderContext>& contexts, RenderContext* ctx);
    void updatePath(LottieGroup* parent, LottieObject** child, float frameNo, Inlist<RenderContext>& contexts, RenderContext* ctx);
    void updatePolystar(LottieGroup* parent, LottieObject** child, float frameNo, Inlist<RenderContext>& contexts, RenderContext* ctx);
    void updateTrimpath(LottieGroup* parent, LottieObject** child, float frameNo, Inlist<RenderContext>& contexts, RenderContext* ctx);
    void updateRepeater(LottieGroup* parent, LottieObject** child, float frameNo, Inlist<RenderContext>& contexts, RenderContext* ctx);
    void updateRoundedCorner(LottieGroup* parent, LottieObject** child, float frameNo, Inlist<RenderContext>& contexts, RenderContext* ctx);
    void updateOffsetPath(LottieGroup* parent, LottieObject** child, float frameNo, Inlist<RenderContext>& contexts, RenderContext* ctx);

    LottieExpressions* exps;
};

#endif //_TVG_LOTTIE_BUILDER_H

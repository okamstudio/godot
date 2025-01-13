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

#ifndef _TVG_PAINT_H_
#define _TVG_PAINT_H_

#include "tvgRender.h"
#include "tvgMath.h"

namespace tvg
{
    enum ContextFlag : uint8_t {Invalid = 0, FastTrack = 1};

    struct Iterator
    {
        virtual ~Iterator() {}
        virtual const Paint* next() = 0;
        virtual uint32_t count() = 0;
        virtual void begin() = 0;
    };

    struct Composite
    {
        Paint* target;
        Paint* source;
        CompositeMethod method;
    };

    struct Paint::Impl
    {
        Paint* paint = nullptr;
        Composite* compData = nullptr;
        Paint* clipper = nullptr;
        RenderMethod* renderer = nullptr;
        struct {
            Matrix m;                 //input matrix
            Matrix cm;                //multipled parents matrix
            float degree;             //rotation degree
            float scale;              //scale factor
            bool overriding;          //user transform?

            void update()
            {
                if (overriding) return;
                m.e11 = 1.0f;
                m.e12 = 0.0f;
                m.e21 = 0.0f;
                m.e22 = 1.0f;
                m.e31 = 0.0f;
                m.e32 = 0.0f;
                m.e33 = 1.0f;
                tvg::scale(&m, scale, scale);
                tvg::rotate(&m, degree);
            }
        } tr;
        BlendMethod blendMethod;
        uint8_t renderFlag;
        uint8_t ctxFlag;
        uint8_t opacity;
        uint8_t refCnt = 0;                              //reference count

        Impl(Paint* pnt) : paint(pnt)
        {
            reset();
        }

        ~Impl()
        {
            if (compData) {
                if (P(compData->target)->unref() == 0) delete(compData->target);
                free(compData);
            }
            if (clipper && P(clipper)->unref() == 0) delete(clipper);
            if (renderer && (renderer->unref() == 0)) delete(renderer);
        }

        uint8_t ref()
        {
            if (refCnt == 255) TVGERR("RENDERER", "Corrupted reference count!");
            return ++refCnt;
        }

        uint8_t unref()
        {
            if (refCnt == 0) TVGERR("RENDERER", "Corrupted reference count!");
            return --refCnt;
        }

        bool transform(const Matrix& m)
        {
            if (&tr.m != &m) tr.m = m;
            tr.overriding = true;
            renderFlag |= RenderUpdateFlag::Transform;

            return true;
        }

        Matrix& transform(bool origin = false)
        {
            //update transform
            if (renderFlag & RenderUpdateFlag::Transform) tr.update();
            if (origin) return tr.cm;
            return tr.m;
        }

        void clip(Paint* clp)
        {
            if (this->clipper) {
                P(this->clipper)->unref();
                if (this->clipper != clp && P(this->clipper)->refCnt == 0) {
                    delete(this->clipper);
                }
            }
            this->clipper = clp;
            if (!clp) return;

            P(clipper)->ref();
        }

        bool composite(Paint* source, Paint* target, CompositeMethod method)
        {
            //Invalid case
            if ((!target && method != CompositeMethod::None) || (target && method == CompositeMethod::None)) return false;

            if (compData) {
                P(compData->target)->unref();
                if ((compData->target != target) && P(compData->target)->refCnt == 0) {
                    delete(compData->target);
                }
                //Reset scenario
                if (!target && method == CompositeMethod::None) {
                    free(compData);
                    compData = nullptr;
                    return true;
                }
            } else {
                if (!target && method == CompositeMethod::None) return true;
                compData = static_cast<Composite*>(calloc(1, sizeof(Composite)));
            }
            P(target)->ref();
            compData->target = target;
            compData->source = source;
            compData->method = method;
            return true;
        }

        RenderRegion bounds(RenderMethod* renderer) const;
        Iterator* iterator();
        bool rotate(float degree);
        bool scale(float factor);
        bool translate(float x, float y);
        bool bounds(float* x, float* y, float* w, float* h, bool transformed, bool stroking, bool origin = false);
        RenderData update(RenderMethod* renderer, const Matrix& pm, Array<RenderData>& clips, uint8_t opacity, RenderUpdateFlag pFlag, bool clipper = false);
        bool render(RenderMethod* renderer);
        Paint* duplicate(Paint* ret = nullptr);
        void reset();
    };
}

#endif //_TVG_PAINT_H_

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

#ifndef _TVG_PICTURE_H_
#define _TVG_PICTURE_H_

#include <string>
#include "tvgPaint.h"
#include "tvgLoader.h"


struct PictureIterator : Iterator
{
    Paint* paint = nullptr;
    Paint* ptr = nullptr;

    PictureIterator(Paint* p) : paint(p) {}

    const Paint* next() override
    {
        if (!ptr) ptr = paint;
        else ptr = nullptr;
        return ptr;
    }

    uint32_t count() override
    {
        if (paint) return 1;
        else return 0;
    }

    void begin() override
    {
        ptr = nullptr;
    }
};


struct Picture::Impl
{
    ImageLoader* loader = nullptr;

    Paint* paint = nullptr;           //vector picture uses
    RenderSurface* surface = nullptr; //bitmap picture uses
    RenderData rd = nullptr;          //engine data
    float w = 0, h = 0;
    Picture* picture = nullptr;
    bool resizing = false;
    bool needComp = false;            //need composition

    bool needComposition(uint8_t opacity);
    bool render(RenderMethod* renderer);
    bool size(float w, float h);
    RenderRegion bounds(RenderMethod* renderer);
    Result load(ImageLoader* ploader);

    Impl(Picture* p) : picture(p)
    {
    }

    ~Impl()
    {
        LoaderMgr::retrieve(loader);
        if (surface) {
            if (auto renderer = PP(picture)->renderer) {
                renderer->dispose(rd);
            }
        }
        delete(paint);
    }

    RenderData update(RenderMethod* renderer, const Matrix& transform, Array<RenderData>& clips, uint8_t opacity, RenderUpdateFlag pFlag, TVG_UNUSED bool clipper)
    {
        auto flag = static_cast<RenderUpdateFlag>(pFlag | load());

        if (surface) {
            if (flag == RenderUpdateFlag::None) return rd;

            //Overriding Transformation by the desired image size
            auto sx = w / loader->w;
            auto sy = h / loader->h;
            auto scale = sx < sy ? sx : sy;
            auto m = transform * Matrix{scale, 0, 0, 0, scale, 0, 0, 0, 1};

            rd = renderer->prepare(surface, rd, m, clips, opacity, flag);
        } else if (paint) {
            if (resizing) {
                loader->resize(paint, w, h);
                resizing = false;
            }
            needComp = needComposition(opacity) ? true : false;
            rd = paint->pImpl->update(renderer, transform, clips, opacity, flag, false);
        }
        return rd;
    }

    bool bounds(float* x, float* y, float* w, float* h, bool stroking)
    {
        if (x) *x = 0;
        if (y) *y = 0;
        if (w) *w = this->w;
        if (h) *h = this->h;
        return true;
    }

    Result load(const string& path)
    {
        if (paint || surface) return Result::InsufficientCondition;

        bool invalid;  //Invalid Path
        auto loader = static_cast<ImageLoader*>(LoaderMgr::loader(path, &invalid));
        if (!loader) {
            if (invalid) return Result::InvalidArguments;
            return Result::NonSupport;
        }
        return load(loader);
    }

    Result load(const char* data, uint32_t size, const string& mimeType, bool copy)
    {
        if (paint || surface) return Result::InsufficientCondition;
        auto loader = static_cast<ImageLoader*>(LoaderMgr::loader(data, size, mimeType, copy));
        if (!loader) return Result::NonSupport;
        return load(loader);
    }

    Result load(uint32_t* data, uint32_t w, uint32_t h, bool copy)
    {
        if (paint || surface) return Result::InsufficientCondition;

        auto loader = static_cast<ImageLoader*>(LoaderMgr::loader(data, w, h, copy));
        if (!loader) return Result::FailedAllocation;

        return load(loader);
    }

    Paint* duplicate(Paint* ret)
    {
        if (ret) TVGERR("RENDERER", "TODO: duplicate()");

        load();

        auto picture = Picture::gen().release();
        auto dup = picture->pImpl;

        if (paint) dup->paint = paint->duplicate();

        if (loader) {
            dup->loader = loader;
            ++dup->loader->sharing;
            PP(picture)->renderFlag |= RenderUpdateFlag::Image;
        }

        dup->surface = surface;
        dup->w = w;
        dup->h = h;
        dup->resizing = resizing;

        return picture;
    }

    Iterator* iterator()
    {
        load();
        return new PictureIterator(paint);
    }

    uint32_t* data(uint32_t* w, uint32_t* h)
    {
        //Try it, If not loaded yet.
        load();

        if (loader) {
            if (w) *w = static_cast<uint32_t>(loader->w);
            if (h) *h = static_cast<uint32_t>(loader->h);
        } else {
            if (w) *w = 0;
            if (h) *h = 0;
        }
        if (surface) return surface->buf32;
        else return nullptr;
    }

    RenderUpdateFlag load();
};

#endif //_TVG_PICTURE_H_

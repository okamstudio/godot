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

#include "tvgPaint.h"
#include "tvgPicture.h"

/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/

RenderUpdateFlag Picture::Impl::load()
{
    if (loader) {
        if (paint) {
            loader->sync();
        } else {
            paint = loader->paint();
            if (paint) {
                if (w != loader->w || h != loader->h) {
                    if (!resizing) {
                        w = loader->w;
                        h = loader->h;
                    }
                    loader->resize(paint, w, h);
                    resizing = false;
                }
                return RenderUpdateFlag::None;
            }
        }
        if (!surface) {
            if ((surface = loader->bitmap())) {
                return RenderUpdateFlag::Image;
            }
        }
    }
    return RenderUpdateFlag::None;
}


bool Picture::Impl::needComposition(uint8_t opacity)
{
    //In this case, paint(scene) would try composition itself.
    if (opacity < 255) return false;

    //Composition test
    const Paint* target;
    auto method = picture->composite(&target);
    if (!target || method == tvg::CompositeMethod::ClipPath) return false;
    if (target->pImpl->opacity == 255 || target->pImpl->opacity == 0) return false;

    return true;
}


bool Picture::Impl::render(RenderMethod* renderer)
{
    bool ret = false;
    renderer->blend(PP(picture)->blendMethod);

    if (surface) return renderer->renderImage(rd);
    else if (paint) {
        RenderCompositor* cmp = nullptr;
        if (needComp) {
            cmp = renderer->target(bounds(renderer), renderer->colorSpace());
            renderer->beginComposite(cmp, CompositeMethod::None, 255);
        }
        ret = paint->pImpl->render(renderer);
        if (cmp) renderer->endComposite(cmp);
    }
    return ret;
}


bool Picture::Impl::size(float w, float h)
{
    this->w = w;
    this->h = h;
    resizing = true;
    return true;
}


RenderRegion Picture::Impl::bounds(RenderMethod* renderer)
{
    if (rd) return renderer->region(rd);
    if (paint) return paint->pImpl->bounds(renderer);
    return {0, 0, 0, 0};
}


Result Picture::Impl::load(ImageLoader* loader)
{
    //Same resource has been loaded.
    if (this->loader == loader) {
        this->loader->sharing--;  //make it sure the reference counting.
        return Result::Success;
    } else if (this->loader) {
        LoaderMgr::retrieve(this->loader);
    }

    this->loader = loader;

    if (!loader->read()) return Result::Unknown;

    this->w = loader->w;
    this->h = loader->h;    

    return Result::Success;
}



/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/

Picture::Picture() : pImpl(new Impl(this))
{
}


Picture::~Picture()
{
    delete(pImpl);
}


unique_ptr<Picture> Picture::gen() noexcept
{
    return unique_ptr<Picture>(new Picture);
}


TVG_DEPRECATED uint32_t Picture::identifier() noexcept
{
    return (uint32_t) Type::Picture;
}


Type Picture::type() const noexcept
{
    return Type::Picture;
}


Result Picture::load(const std::string& path) noexcept
{
    if (path.empty()) return Result::InvalidArguments;

    return pImpl->load(path);
}


Result Picture::load(const char* data, uint32_t size, const string& mimeType, bool copy) noexcept
{
    if (!data || size <= 0) return Result::InvalidArguments;

    return pImpl->load(data, size, mimeType, copy);
}


TVG_DEPRECATED Result Picture::load(const char* data, uint32_t size, bool copy) noexcept
{
    return load(data, size, "", copy);
}


Result Picture::load(uint32_t* data, uint32_t w, uint32_t h, bool copy) noexcept
{
    if (!data || w <= 0 || h <= 0) return Result::InvalidArguments;

    return pImpl->load(data, w, h, copy);
}


Result Picture::size(float w, float h) noexcept
{
    if (pImpl->size(w, h)) return Result::Success;
    return Result::InsufficientCondition;
}


Result Picture::size(float* w, float* h) const noexcept
{
    if (!pImpl->loader) return Result::InsufficientCondition;
    if (w) *w = pImpl->w;
    if (h) *h = pImpl->h;
    return Result::Success;
}


const Paint* Picture::paint(uint32_t id) noexcept
{
    struct Value
    {
        uint32_t id;
        const Paint* ret;
    } value = {id, nullptr};

    auto cb = [](const tvg::Paint* paint, void* data) -> bool
    {
        auto p = static_cast<Value*>(data);
        if (p->id == paint->id) {
            p->ret = paint;
            return false;
        }
        return true;
    };

    tvg::Accessor::gen()->set(this, cb, &value);
    return value.ret;
}

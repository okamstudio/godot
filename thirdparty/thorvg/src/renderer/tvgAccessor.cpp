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

#include "tvgIteratorAccessor.h"
#include "tvgCompressor.h"

/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/

static bool accessChildren(Iterator* it, function<bool(const Paint* paint, void* data)> func, void* data)
{
    while (auto child = it->next()) {
        //Access the child
        if (!func(child, data)) return false;

        //Access the children of the child
        if (auto it2 = IteratorAccessor::iterator(child)) {
            if (!accessChildren(it2, func, data)) {
                delete(it2);
                return false;
            }
            delete(it2);
        }
    }
    return true;
}


/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/

TVG_DEPRECATED unique_ptr<Picture> Accessor::set(unique_ptr<Picture> picture, function<bool(const Paint* paint)> func) noexcept
{
    auto backward = [](const tvg::Paint* paint, void* data) -> bool
    {
        auto func = reinterpret_cast<function<bool(const Paint* paint)>*>(data);
        if (!(*func)(paint)) return false;
        return true;
    };

    set(picture.get(), backward, reinterpret_cast<void*>(&func));
    return picture;
}


Result Accessor::set(Paint* paint, function<bool(const Paint* paint, void* data)> func, void* data) noexcept
{
    if (!paint || !func) return Result::InvalidArguments;

    //Use the Preorder Tree-Searc

    //Root
    if (!func(paint, data)) return Result::Success;

    //Children
    if (auto it = IteratorAccessor::iterator(paint)) {
        accessChildren(it, func, data);
        delete(it);
    }
    return Result::Success;
}


uint32_t Accessor::id(const char* name) noexcept
{
    return djb2Encode(name);
}


Accessor::~Accessor()
{

}


Accessor::Accessor() : pImpl(nullptr)
{

}


unique_ptr<Accessor> Accessor::gen() noexcept
{
    return unique_ptr<Accessor>(new Accessor);
}

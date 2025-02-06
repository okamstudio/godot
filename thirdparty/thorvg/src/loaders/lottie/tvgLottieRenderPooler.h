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

#ifndef _TVG_LOTTIE_RENDER_POOLER_H_
#define _TVG_LOTTIE_RENDER_POOLER_H_

#include "tvgCommon.h"
#include "tvgArray.h"
#include "tvgPaint.h"


template<typename T>
struct LottieRenderPooler
{
    Array<T*> pooler;

    ~LottieRenderPooler()
    {
        for (auto p = pooler.begin(); p < pooler.end(); ++p) {
            if (PP(*p)->unref() == 0) delete(*p);
        }
    }

    T* pooling(bool copy = false)
    {
        //return available one.
        for (auto p = pooler.begin(); p < pooler.end(); ++p) {
            if (PP(*p)->refCnt == 1) return *p;
        }

        //no empty, generate a new one.
        auto p = copy ? static_cast<T*>(pooler[0]->duplicate()) : T::gen().release();
        PP(p)->ref();
        pooler.push(p);
        return p;
    }
};

#endif //_TVG_LOTTIE_RENDER_POOLER_H_

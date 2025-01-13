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

#ifndef _TVG_WEBP_LOADER_H_
#define _TVG_WEBP_LOADER_H_

#include "tvgLoader.h"
#include "tvgTaskScheduler.h"

class WebpLoader : public ImageLoader, public Task
{
public:
    WebpLoader();
    ~WebpLoader();

    bool open(const string& path) override;
    bool open(const char* data, uint32_t size, bool copy) override;
    bool read() override;

    RenderSurface* bitmap() override;

private:
    void run(unsigned tid) override;

    unsigned char* data = nullptr;
    unsigned long size = 0;
    bool freeData = false;
};

#endif //_TVG_WEBP_LOADER_H_

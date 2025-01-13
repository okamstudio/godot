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

#ifndef _TVG_JPG_LOADER_H_
#define _TVG_JPG_LOADER_H_

#include "tvgLoader.h"
#include "tvgTaskScheduler.h"
#include "tvgJpgd.h"

class JpgLoader : public ImageLoader, public Task
{
private:
    jpeg_decoder* decoder = nullptr;
    char* data = nullptr;
    bool freeData = false;

    void clear();
    void run(unsigned tid) override;

public:
    JpgLoader();
    ~JpgLoader();

    bool open(const string& path) override;
    bool open(const char* data, uint32_t size, bool copy) override;
    bool read() override;
    bool close() override;

    RenderSurface* bitmap() override;
};

#endif //_TVG_JPG_LOADER_H_

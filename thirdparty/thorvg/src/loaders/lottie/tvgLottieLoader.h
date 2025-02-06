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

#ifndef _TVG_LOTTIE_LOADER_H_
#define _TVG_LOTTIE_LOADER_H_

#include "tvgCommon.h"
#include "tvgFrameModule.h"
#include "tvgTaskScheduler.h"

struct LottieComposition;
struct LottieBuilder;

class LottieLoader : public FrameModule, public Task
{
public:
    const char* content = nullptr;      //lottie file data
    uint32_t size = 0;                  //lottie data size
    float frameNo = 0.0f;               //current frame number
    float frameCnt = 0.0f;
    float frameDuration = 0.0f;
    float frameRate = 0.0f;

    LottieBuilder* builder;
    LottieComposition* comp = nullptr;

    Key key;
    char* dirName = nullptr;            //base resource directory
    bool copy = false;                  //"content" is owned by this loader
    bool overridden = false;             //overridden properties with slots
    bool rebuild = false;               //require building the lottie scene

    LottieLoader();
    ~LottieLoader();

    bool open(const string& path) override;
    bool open(const char* data, uint32_t size, bool copy) override;
    bool resize(Paint* paint, float w, float h) override;
    bool read() override;
    Paint* paint() override;
    bool override(const char* slot, bool byDefault = false);

    //Frame Controls
    bool frame(float no) override;
    float totalFrame() override;
    float curFrame() override;
    float duration() override;
    void sync() override;

    //Marker Supports
    uint32_t markersCnt();
    const char* markers(uint32_t index);
    bool segment(const char* marker, float& begin, float& end);

private:
    bool ready();
    bool header();
    void clear();
    float startFrame();
    void run(unsigned tid) override;
    void release();
};


#endif //_TVG_LOTTIELOADER_H_

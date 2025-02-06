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

#include "tvgLottieLoader.h"
#include "tvgLottieModel.h"
#include "tvgLottieParser.h"
#include "tvgLottieBuilder.h"
#include "tvgStr.h"

/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/

void LottieLoader::run(unsigned tid)
{
    //update frame
    if (comp) {
        builder->update(comp, frameNo);
    //initial loading
    } else {
        LottieParser parser(content, dirName);
        if (!parser.parse()) return;
        {
            ScopedLock lock(key);
            comp = parser.comp;
        }
        if (parser.slots) {
            override(parser.slots, true);
            parser.slots = nullptr;
        }
        builder->build(comp);

        release();
    }
    rebuild = false;
}


void LottieLoader::release()
{
    if (copy) {
        free((char*)content);
        content = nullptr;
    }
}


/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/

LottieLoader::LottieLoader() : FrameModule(FileType::Lottie), builder(new LottieBuilder)
{

}


LottieLoader::~LottieLoader()
{
    done();

    release();

    //TODO: correct position?
    delete(comp);
    delete(builder);

    free(dirName);
}


bool LottieLoader::header()
{
    //A single thread doesn't need to perform intensive tasks.
    if (TaskScheduler::threads() == 0) {
        LoadModule::read();
        run(0);
        if (comp) {
            w = static_cast<float>(comp->w);
            h = static_cast<float>(comp->h);
            frameDuration = comp->duration();
            frameCnt = comp->frameCnt();
            frameRate = comp->frameRate;
            return true;
        } else {
            return false;
        }
    }

    //Quickly validate the given Lottie file without parsing in order to get the animation info.
    auto startFrame = 0.0f;
    auto endFrame = 0.0f;
    uint32_t depth = 0;

    auto p = content;

    while (*p != '\0') {
        if (*p == '{') {
            ++depth;
            ++p;
            continue;
        }
        if (*p == '}') {
            --depth;
            ++p;
            continue;
        }
        if (depth != 1) {
            ++p;
            continue;
        }
        //version.
        if (!strncmp(p, "\"v\":", 4)) {
            p += 4;
            continue;
        }

        //framerate
        if (!strncmp(p, "\"fr\":", 5)) {
            p += 5;
            auto e = strstr(p, ",");
            if (!e) e = strstr(p, "}");
            frameRate = strToFloat(p, nullptr);
            p = e;
            continue;
        }

        //start frame
        if (!strncmp(p, "\"ip\":", 5)) {
            p += 5;
            auto e = strstr(p, ",");
            if (!e) e = strstr(p, "}");
            startFrame = strToFloat(p, nullptr);
            p = e;
            continue;
        }

        //end frame
        if (!strncmp(p, "\"op\":", 5)) {
            p += 5;
            auto e = strstr(p, ",");
            if (!e) e = strstr(p, "}");
            endFrame = strToFloat(p, nullptr);
            p = e;
            continue;
        }

        //width
        if (!strncmp(p, "\"w\":", 4)) {
            p += 4;
            auto e = strstr(p, ",");
            if (!e) e = strstr(p, "}");
            w = strToFloat(p, nullptr);
            p = e;
            continue;
        }
        //height
        if (!strncmp(p, "\"h\":", 4)) {
            p += 4;
            auto e = strstr(p, ",");
            if (!e) e = strstr(p, "}");
            h = strToFloat(p, nullptr);
            p = e;
            continue;
        }
        ++p;
    }

    if (frameRate < FLOAT_EPSILON) {
        TVGLOG("LOTTIE", "Not a Lottie file? Frame rate is 0!");
        return false;
    }

    frameCnt = (endFrame - startFrame);
    frameDuration = frameCnt / frameRate;

    TVGLOG("LOTTIE", "info: frame rate = %f, duration = %f size = %f x %f", frameRate, frameDuration, w, h);

    return true;
}


bool LottieLoader::open(const char* data, uint32_t size, bool copy)
{
    if (copy) {
        content = (char*)malloc(size + 1);
        if (!content) return false;
        memcpy((char*)content, data, size);
        const_cast<char*>(content)[size] = '\0';
    } else content = data;

    this->dirName = strdup(".");

    this->size = size;
    this->copy = copy;

    return header();
}


bool LottieLoader::open(const string& path)
{
    auto f = fopen(path.c_str(), "r");
    if (!f) return false;

    fseek(f, 0, SEEK_END);

    size = ftell(f);
    if (size == 0) {
        fclose(f);
        return false;
    }

    auto content = (char*)(malloc(sizeof(char) * size + 1));
    fseek(f, 0, SEEK_SET);
    auto ret = fread(content, sizeof(char), size, f);
    if (ret < size) {
        fclose(f);
        return false;
    }
    content[size] = '\0';

    fclose(f);

    this->dirName = strDirname(path.c_str());
    this->content = content;
    this->copy = true;

    return header();
}


bool LottieLoader::resize(Paint* paint, float w, float h)
{
    if (!paint) return false;

    auto sx = w / this->w;
    auto sy = h / this->h;
    Matrix m = {sx, 0, 0, 0, sy, 0, 0, 0, 1};
    paint->transform(m);

    //apply the scale to the base clipper
    const Paint* clipper;
    paint->composite(&clipper);
    if (clipper) const_cast<Paint*>(clipper)->transform(m);

    return true;
}


bool LottieLoader::read()
{
    //the loading has been already completed
    if (!LoadModule::read()) return true;

    if (!content || size == 0) return false;

    TaskScheduler::request(this);

    return true;
}


Paint* LottieLoader::paint()
{
    done();

    if (!comp) return nullptr;
    comp->initiated = true;
    return comp->root->scene;
}


bool LottieLoader::override(const char* slots, bool byDefault)
{
    if (!ready() || comp->slots.count == 0) return false;

    //override slots
    if (slots) {
        //Copy the input data because the JSON parser will encode the data immediately.
        auto temp = byDefault ? slots : strdup(slots);

        //parsing slot json
        LottieParser parser(temp, dirName);
        parser.comp = comp;

        auto idx = 0;
        auto succeed = false;
        while (auto sid = parser.sid(idx == 0)) {
            auto applied = false;
            for (auto s = comp->slots.begin(); s < comp->slots.end(); ++s) {
                if (strcmp((*s)->sid, sid)) continue;
                if (parser.apply(*s, byDefault)) succeed = applied = true;
                break;
            }
            if (!applied) parser.skip(sid);
            ++idx;
        }
        free((char*)temp);
        rebuild = succeed;
        overridden |= succeed;
        return rebuild;
    //reset slots
    } else if (overridden) {
        for (auto s = comp->slots.begin(); s < comp->slots.end(); ++s) {
            (*s)->reset();
        }
        overridden = false;
        rebuild = true;
    }
    return true;
}


bool LottieLoader::frame(float no)
{
    auto frameNo = no + startFrame();

    //This ensures that the target frame number is reached.
    frameNo *= 10000.0f;
    frameNo = nearbyintf(frameNo);
    frameNo *= 0.0001f;

    //Skip update if frame diff is too small.
    if (fabsf(this->frameNo - frameNo) <= 0.0009f) return false;

    this->done();

    this->frameNo = frameNo;

    TaskScheduler::request(this);

    return true;
}


float LottieLoader::startFrame()
{
    return frameCnt * segmentBegin;
}


float LottieLoader::totalFrame()
{
    return (segmentEnd - segmentBegin) * frameCnt;
}


float LottieLoader::curFrame()
{
    return frameNo - startFrame();
}


float LottieLoader::duration()
{
    if (segmentBegin == 0.0f && segmentEnd == 1.0f) return frameDuration;
    return frameCnt * (segmentEnd - segmentBegin) / frameRate;
}


void LottieLoader::sync()
{
    done();

    if (rebuild) run(0);
}


uint32_t LottieLoader::markersCnt()
{
    return ready() ? comp->markers.count : 0;
}


const char* LottieLoader::markers(uint32_t index)
{
    if (!ready() || index >= comp->markers.count) return nullptr;
    auto marker = comp->markers.begin() + index;
    return (*marker)->name;
}


bool LottieLoader::segment(const char* marker, float& begin, float& end)
{
    if (!ready() || comp->markers.count == 0) return false;

    for (auto m = comp->markers.begin(); m < comp->markers.end(); ++m) {
        if (!strcmp(marker, (*m)->name)) {
            begin = (*m)->time / frameCnt;
            end = ((*m)->time + (*m)->duration) / frameCnt;
            return true;
        }
    }
    return false;
}


bool LottieLoader::ready()
{
    {
        ScopedLock lock(key);
        if (comp) return true;
    }
    done();
    if (comp) return true;
    return false;
}

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

#ifndef _TVG_LOTTIE_PROPERTY_H_
#define _TVG_LOTTIE_PROPERTY_H_

#include <algorithm>
#include "tvgMath.h"
#include "tvgLottieCommon.h"
#include "tvgLottieInterpolator.h"
#include "tvgLottieExpressions.h"
#include "tvgLottieModifier.h"


struct LottieFont;
struct LottieLayer;
struct LottieObject;


template<typename T>
struct LottieScalarFrame
{
    T value;                    //keyframe value
    float no;                   //frame number
    LottieInterpolator* interpolator;
    bool hold = false;           //do not interpolate.

    T interpolate(LottieScalarFrame<T>* next, float frameNo)
    {
        auto t = (frameNo - no) / (next->no - no);
        if (interpolator) t = interpolator->progress(t);

        if (hold) {
            if (t < 1.0f) return value;
            else return next->value;
        }
        return lerp(value, next->value, t);
    }
};


template<typename T>
struct LottieVectorFrame
{
    T value;                    //keyframe value
    float no;                   //frame number
    LottieInterpolator* interpolator;
    T outTangent, inTangent;
    float length;
    bool hasTangent = false;
    bool hold = false;

    T interpolate(LottieVectorFrame* next, float frameNo)
    {
        auto t = (frameNo - no) / (next->no - no);
        if (interpolator) t = interpolator->progress(t);

        if (hold) {
            if (t < 1.0f) return value;
            else return next->value;
        }

        if (hasTangent) {
            Bezier bz = {value, value + outTangent, next->value + inTangent, next->value};
            return bz.at(bz.atApprox(t * length, length));
        } else {
            return lerp(value, next->value, t);
        }
    }

    float angle(LottieVectorFrame* next, float frameNo)
    {
        if (!hasTangent) {
            Point dp = next->value - value;
            return rad2deg(tvg::atan2(dp.y, dp.x));
        }

        auto t = (frameNo - no) / (next->no - no);
        if (interpolator) t = interpolator->progress(t);
        Bezier bz = {value, value + outTangent, next->value + inTangent, next->value};
        t = bz.atApprox(t * length, length);
        return bz.angle(t >= 1.0f ? 0.99f : (t <= 0.0f ? 0.01f : t));
    }

    void prepare(LottieVectorFrame* next)
    {
        Bezier bz = {value, value + outTangent, next->value + inTangent, next->value};
        length = bz.lengthApprox();
    }
};


//Property would have an either keyframes or single value.
struct LottieProperty
{
    enum class Type : uint8_t { Point = 0, Float, Opacity, Color, PathSet, ColorStop, Position, TextDoc, Image, Invalid };

    LottieExpression* exp = nullptr;
    Type type;
    uint8_t ix;  //property index

    //TODO: Apply common bodies?
    virtual ~LottieProperty() {}
    virtual uint32_t frameCnt() = 0;
    virtual uint32_t nearest(float time) = 0;
    virtual float frameNo(int32_t key) = 0;
};


struct LottieExpression
{
    enum LoopMode : uint8_t { None = 0, InCycle = 1, InPingPong, InOffset, InContinue, OutCycle, OutPingPong, OutOffset, OutContinue };

    char* code;
    LottieComposition* comp;
    LottieLayer* layer;
    LottieObject* object;
    LottieProperty* property;
    bool disabled = false;

    struct {
        uint32_t key = 0;      //the keyframe number repeating to
        float in = FLT_MAX;    //looping duration in frame number
        LoopMode mode = None;
    } loop;

    ~LottieExpression()
    {
        free(code);
    }
};


static void _copy(PathSet* pathset, Array<Point>& outPts, Matrix* transform)
{
    Array<Point> inPts;

    if (transform) {
        for (int i = 0; i < pathset->ptsCnt; ++i) {
            Point pt = pathset->pts[i];
            pt *= *transform;
            outPts.push(pt);
        }
    } else {
        inPts.data = pathset->pts;
        inPts.count = pathset->ptsCnt;
        outPts.push(inPts);
        inPts.data = nullptr;
    }
}


static void _copy(PathSet* pathset, Array<PathCommand>& outCmds)
{
    Array<PathCommand> inCmds;
    inCmds.data = pathset->cmds;
    inCmds.count = pathset->cmdsCnt;
    outCmds.push(inCmds);
    inCmds.data = nullptr;
}


template<typename T>
uint32_t _bsearch(T* frames, float frameNo)
{
    int32_t low = 0;
    int32_t high = int32_t(frames->count) - 1;

    while (low <= high) {
        auto mid = low + (high - low) / 2;
        auto frame = frames->data + mid;
        if (frameNo < frame->no) high = mid - 1;
        else low = mid + 1;
    }
    if (high < low) low = high;
    if (low < 0) low = 0;
    return low;
}


template<typename T>
uint32_t _nearest(T* frames, float frameNo)
{
    if (frames) {
        auto key = _bsearch(frames, frameNo);
        if (key == frames->count - 1) return key;
        return (fabsf(frames->data[key].no - frameNo) < fabsf(frames->data[key + 1].no - frameNo)) ? key : (key + 1);
    }
    return 0;
}


template<typename T>
float _frameNo(T* frames, int32_t key)
{
    if (!frames) return 0.0f;
    if (key < 0) key = 0;
    if (key >= (int32_t) frames->count) key = (int32_t)(frames->count - 1);
    return (*frames)[key].no;
}


template<typename T>
float _loop(T* frames, float frameNo, LottieExpression* exp)
{
    if (frameNo >= exp->loop.in || frameNo < frames->first().no || frameNo < frames->last().no) return frameNo;

    frameNo -= frames->first().no;

    switch (exp->loop.mode) {
        case LottieExpression::LoopMode::InCycle: {
            return fmodf(frameNo, frames->last().no - frames->first().no) + (*frames)[exp->loop.key].no;
        }
        case LottieExpression::LoopMode::InPingPong: {
            auto range = frames->last().no - (*frames)[exp->loop.key].no;
            auto forward = (static_cast<int>(frameNo / range) % 2) == 0 ? true : false;
            frameNo = fmodf(frameNo, range);
            return (forward ? frameNo : (range - frameNo)) + (*frames)[exp->loop.key].no;
        }
        case LottieExpression::LoopMode::OutCycle: {
            return fmodf(frameNo, (*frames)[frames->count - 1 - exp->loop.key].no - frames->first().no) + frames->first().no;
        }
        case LottieExpression::LoopMode::OutPingPong: {
            auto range = (*frames)[frames->count - 1 - exp->loop.key].no - frames->first().no;
            auto forward = (static_cast<int>(frameNo / range) % 2) == 0 ? true : false;
            frameNo = fmodf(frameNo, range);
            return (forward ? frameNo : (range - frameNo)) + frames->first().no;
        }
        default: break;
    }
    return frameNo;
}


template<typename T>
struct LottieGenericProperty : LottieProperty
{
    //Property has an either keyframes or single value.
    Array<LottieScalarFrame<T>>* frames = nullptr;
    T value;

    LottieGenericProperty(T v) : value(v) {}
    LottieGenericProperty() {}

    LottieGenericProperty(const LottieGenericProperty<T>& rhs)
    {
        copy(rhs);
        type = rhs.type;
    }

    ~LottieGenericProperty()
    {
        release();
    }

    void release()
    {
        delete(frames);
        frames = nullptr;
        if (exp) {
            delete(exp);
            exp = nullptr;
        }
    }

    uint32_t nearest(float frameNo) override
    {
        return _nearest(frames, frameNo);
    }

    uint32_t frameCnt() override
    {
        return frames ? frames->count : 1;
    }

    float frameNo(int32_t key) override
    {
        return _frameNo(frames, key);
    }

    LottieScalarFrame<T>& newFrame()
    {
        if (!frames) frames = new Array<LottieScalarFrame<T>>;
        if (frames->count + 1 >= frames->reserved) {
            auto old = frames->reserved;
            frames->grow(frames->count + 2);
            memset((void*)(frames->data + old), 0x00, sizeof(LottieScalarFrame<T>) * (frames->reserved - old));
        }
        ++frames->count;
        return frames->last();
    }

    LottieScalarFrame<T>& nextFrame()
    {
        return (*frames)[frames->count];
    }

    T operator()(float frameNo)
    {
        if (!frames) return value;
        if (frames->count == 1 || frameNo <= frames->first().no) return frames->first().value;
        if (frameNo >= frames->last().no) return frames->last().value;

        auto frame = frames->data + _bsearch(frames, frameNo);
        if (tvg::equal(frame->no, frameNo)) return frame->value;
        return frame->interpolate(frame + 1, frameNo);
    }

    T operator()(float frameNo, LottieExpressions* exps)
    {
        if (exps && exp) {
            T out{};
            if (exp->loop.mode != LottieExpression::LoopMode::None) frameNo = _loop(frames, frameNo, exp);
            if (exps->result<LottieGenericProperty<T>>(frameNo, out, exp)) return out;
        }
        return operator()(frameNo);
    }

    void copy(const LottieGenericProperty<T>& rhs, bool shallow = true)
    {
        if (rhs.frames) {
            if (shallow) {
                frames = rhs.frames;
                const_cast<LottieGenericProperty<T>&>(rhs).frames = nullptr;
            } else {
                frames = new Array<LottieScalarFrame<T>>;
                *frames = *rhs.frames;
            }
        } else value = rhs.value;
    }

    float angle(float frameNo) { return 0; }
    void prepare() {}
};


struct LottiePathSet : LottieProperty
{
    Array<LottieScalarFrame<PathSet>>* frames = nullptr;
    PathSet value;

    ~LottiePathSet()
    {
        release();
    }

    void release()
    {
        if (exp) {
            delete(exp);
            exp = nullptr;
        }

        free(value.cmds);
        free(value.pts);

        if (!frames) return;

        for (auto p = frames->begin(); p < frames->end(); ++p) {
            free((*p).value.cmds);
            free((*p).value.pts);
        }
        free(frames->data);
        free(frames);
    }

    uint32_t nearest(float frameNo) override
    {
        return _nearest(frames, frameNo);
    }

    uint32_t frameCnt() override
    {
        return frames ? frames->count : 1;
    }

    float frameNo(int32_t key) override
    {
        return _frameNo(frames, key);
    }

    LottieScalarFrame<PathSet>& newFrame()
    {
        if (!frames) {
            frames = static_cast<Array<LottieScalarFrame<PathSet>>*>(calloc(1, sizeof(Array<LottieScalarFrame<PathSet>>)));
        }
        if (frames->count + 1 >= frames->reserved) {
            auto old = frames->reserved;
            frames->grow(frames->count + 2);
            memset((void*)(frames->data + old), 0x00, sizeof(LottieScalarFrame<PathSet>) * (frames->reserved - old));
        }
        ++frames->count;
        return frames->last();
    }

    LottieScalarFrame<PathSet>& nextFrame()
    {
        return (*frames)[frames->count];
    }

    bool operator()(float frameNo, Array<PathCommand>& cmds, Array<Point>& pts, Matrix* transform, const LottieRoundnessModifier* roundness, const LottieOffsetModifier* offsetPath)
    {
        PathSet* path = nullptr;
        LottieScalarFrame<PathSet>* frame = nullptr;
        float t;
        bool interpolate = false;

        if (!frames) path = &value;
        else if (frames->count == 1 || frameNo <= frames->first().no) path = &frames->first().value;
        else if (frameNo >= frames->last().no) path = &frames->last().value;
        else {
            frame = frames->data + _bsearch(frames, frameNo);
            if (tvg::equal(frame->no, frameNo)) path = &frame->value;
            else if (frame->value.ptsCnt != (frame + 1)->value.ptsCnt) {
                path = &frame->value;
                TVGLOG("LOTTIE", "Different numbers of points in consecutive frames - interpolation omitted.");
            } else {
                t = (frameNo - frame->no) / ((frame + 1)->no - frame->no);
                if (frame->interpolator) t = frame->interpolator->progress(t);
                if (frame->hold) path = &(frame + ((t < 1.0f) ? 0 : 1))->value;
                else interpolate = true;
            }
        }

        if (!interpolate) {
            if (roundness) {
                if (offsetPath) {
                    Array<PathCommand> cmds1(path->cmdsCnt);
                    Array<Point> pts1(path->ptsCnt);
                    roundness->modifyPath(path->cmds, path->cmdsCnt, path->pts, path->ptsCnt, cmds1, pts1, transform);
                    return offsetPath->modifyPath(cmds1.data, cmds1.count, pts1.data, pts1.count, cmds, pts);
                }
                return roundness->modifyPath(path->cmds, path->cmdsCnt, path->pts, path->ptsCnt, cmds, pts, transform);
            }
            if (offsetPath) return offsetPath->modifyPath(path->cmds, path->cmdsCnt, path->pts, path->ptsCnt, cmds, pts);

            _copy(path, cmds);
            _copy(path, pts, transform);
            return true;
        }

        auto s = frame->value.pts;
        auto e = (frame + 1)->value.pts;

        if (!roundness && !offsetPath) {
            for (auto i = 0; i < frame->value.ptsCnt; ++i, ++s, ++e) {
                auto pt = lerp(*s, *e, t);
                if (transform) pt *= *transform;
                pts.push(pt);
            }
            _copy(&frame->value, cmds);
            return true;
        }

        auto interpPts = (Point*)malloc(frame->value.ptsCnt * sizeof(Point));
        auto p = interpPts;
        for (auto i = 0; i < frame->value.ptsCnt; ++i, ++s, ++e, ++p) {
            *p = lerp(*s, *e, t);
            if (transform) *p *= *transform;
        }

        if (roundness) {
            if (offsetPath) {
                Array<PathCommand> cmds1;
                Array<Point> pts1;
                roundness->modifyPath(frame->value.cmds, frame->value.cmdsCnt, interpPts, frame->value.ptsCnt, cmds1, pts1, nullptr);
                offsetPath->modifyPath(cmds1.data, cmds1.count, pts1.data, pts1.count, cmds, pts);
            } else roundness->modifyPath(frame->value.cmds, frame->value.cmdsCnt, interpPts, frame->value.ptsCnt, cmds, pts, nullptr);
        } else if (offsetPath) offsetPath->modifyPath(frame->value.cmds, frame->value.cmdsCnt, interpPts, frame->value.ptsCnt, cmds, pts);

        free(interpPts);

        return true;
    }


    bool operator()(float frameNo, Array<PathCommand>& cmds, Array<Point>& pts, Matrix* transform, const LottieRoundnessModifier* roundness, const LottieOffsetModifier* offsetPath, LottieExpressions* exps)
    {
        if (exps && exp) {
            if (exp->loop.mode != LottieExpression::LoopMode::None) frameNo = _loop(frames, frameNo, exp);
            if (exps->result<LottiePathSet>(frameNo, cmds, pts, transform, roundness, offsetPath, exp)) return true;
        }
        return operator()(frameNo, cmds, pts, transform, roundness, offsetPath);
    }

    void prepare() {}
};


struct LottieColorStop : LottieProperty
{
    Array<LottieScalarFrame<ColorStop>>* frames = nullptr;
    ColorStop value;
    uint16_t count = 0;     //colorstop count
    bool populated = false;

    LottieColorStop() {}

    LottieColorStop(const LottieColorStop& rhs)
    {
        copy(rhs);
        type = rhs.type;
    }

    ~LottieColorStop()
    {
        release();
    }

    void release()
    {
        if (exp) {
            delete(exp);
            exp = nullptr;
        }

        if (value.data) {
            free(value.data);
            value.data = nullptr;
        }

        if (!frames) return;

        for (auto p = frames->begin(); p < frames->end(); ++p) {
            free((*p).value.data);
        }
        free(frames->data);
        free(frames);
        frames = nullptr;
    }

    uint32_t nearest(float frameNo) override
    {
        return _nearest(frames, frameNo);
    }

    uint32_t frameCnt() override
    {
        return frames ? frames->count : 1;
    }

    float frameNo(int32_t key) override
    {
        return _frameNo(frames, key);
    }

    LottieScalarFrame<ColorStop>& newFrame()
    {
        if (!frames) {
            frames = static_cast<Array<LottieScalarFrame<ColorStop>>*>(calloc(1, sizeof(Array<LottieScalarFrame<ColorStop>>)));
        }
        if (frames->count + 1 >= frames->reserved) {
            auto old = frames->reserved;
            frames->grow(frames->count + 2);
            memset((void*)(frames->data + old), 0x00, sizeof(LottieScalarFrame<ColorStop>) * (frames->reserved - old));
        }
        ++frames->count;
        return frames->last();
    }

    LottieScalarFrame<ColorStop>& nextFrame()
    {
        return (*frames)[frames->count];
    }

    Result operator()(float frameNo, Fill* fill, LottieExpressions* exps)
    {
        if (exps && exp) {
            if (exp->loop.mode != LottieExpression::LoopMode::None) frameNo = _loop(frames, frameNo, exp);
            if (exps->result<LottieColorStop>(frameNo, fill, exp)) return Result::Success;
        }

        if (!frames) return fill->colorStops(value.data, count);

        if (frames->count == 1 || frameNo <= frames->first().no) {
            return fill->colorStops(frames->first().value.data, count);
        }

        if (frameNo >= frames->last().no) {
            return fill->colorStops(frames->last().value.data, count);
        }

        auto frame = frames->data + _bsearch(frames, frameNo);
        if (tvg::equal(frame->no, frameNo)) return fill->colorStops(frame->value.data, count);

        //interpolate
        auto t = (frameNo - frame->no) / ((frame + 1)->no - frame->no);
        if (frame->interpolator) t = frame->interpolator->progress(t);

        if (frame->hold) {
            if (t < 1.0f) fill->colorStops(frame->value.data, count);
            else fill->colorStops((frame + 1)->value.data, count);
        }

        auto s = frame->value.data;
        auto e = (frame + 1)->value.data;

        Array<Fill::ColorStop> result;

        for (auto i = 0; i < count; ++i, ++s, ++e) {
            auto offset = lerp(s->offset, e->offset, t);
            auto r = lerp(s->r, e->r, t);
            auto g = lerp(s->g, e->g, t);
            auto b = lerp(s->b, e->b, t);
            auto a = lerp(s->a, e->a, t);
            result.push({offset, r, g, b, a});
        }
        return fill->colorStops(result.data, count);
    }

    void copy(const LottieColorStop& rhs, bool shallow = true)
    {
        if (rhs.frames) {
            if (shallow) {
                frames = rhs.frames;
                const_cast<LottieColorStop&>(rhs).frames = nullptr;
            } else {
                frames = new Array<LottieScalarFrame<ColorStop>>;
                *frames = *rhs.frames;
            }
        } else {
            value = rhs.value;
            const_cast<LottieColorStop&>(rhs).value = ColorStop();
        }
        populated = rhs.populated;
        count = rhs.count;
    }

    void prepare() {}
};


struct LottiePosition : LottieProperty
{
    Array<LottieVectorFrame<Point>>* frames = nullptr;
    Point value;

    LottiePosition(Point v) : value(v)
    {
    }

    ~LottiePosition()
    {
        release();
    }

    void release()
    {
        delete(frames);
        frames = nullptr;

        if (exp) {
            delete(exp);
            exp = nullptr;
        }
    }

    uint32_t nearest(float frameNo) override
    {
        return _nearest(frames, frameNo);
    }

    uint32_t frameCnt() override
    {
        return frames ? frames->count : 1;
    }

    float frameNo(int32_t key) override
    {
        return _frameNo(frames, key);
    }

    LottieVectorFrame<Point>& newFrame()
    {
        if (!frames) frames = new Array<LottieVectorFrame<Point>>;
        if (frames->count + 1 >= frames->reserved) {
            auto old = frames->reserved;
            frames->grow(frames->count + 2);
            memset((void*)(frames->data + old), 0x00, sizeof(LottieVectorFrame<Point>) * (frames->reserved - old));
        }
        ++frames->count;
        return frames->last();
    }

    LottieVectorFrame<Point>& nextFrame()
    {
        return (*frames)[frames->count];
    }

    Point operator()(float frameNo)
    {
        if (!frames) return value;
        if (frames->count == 1 || frameNo <= frames->first().no) return frames->first().value;
        if (frameNo >= frames->last().no) return frames->last().value;

        auto frame = frames->data + _bsearch(frames, frameNo);
        if (tvg::equal(frame->no, frameNo)) return frame->value;
        return frame->interpolate(frame + 1, frameNo);
    }

    Point operator()(float frameNo, LottieExpressions* exps)
    {
        Point out{};
        if (exps && exp) {
            if (exp->loop.mode != LottieExpression::LoopMode::None) frameNo = _loop(frames, frameNo, exp);
            if (exps->result<LottiePosition>(frameNo, out, exp)) return out;
        }
        return operator()(frameNo);
    }

    float angle(float frameNo)
    {
        if (!frames || frames->count == 1) return 0;

        if (frameNo <= frames->first().no) return frames->first().angle(frames->data + 1, frames->first().no);
        if (frameNo >= frames->last().no) {
            auto frame = frames->data + frames->count - 2;
            return frame->angle(frame + 1, frames->last().no);
        }

        auto frame = frames->data + _bsearch(frames, frameNo);
        return frame->angle(frame + 1, frameNo);
    }

    void prepare()
    {
        if (!frames || frames->count < 2) return;
        for (auto frame = frames->begin() + 1; frame < frames->end(); ++frame) {
            (frame - 1)->prepare(frame);
        }
    }
};


struct LottieTextDoc : LottieProperty
{
    Array<LottieScalarFrame<TextDocument>>* frames = nullptr;
    TextDocument value;

    LottieTextDoc() {}

    LottieTextDoc(const LottieTextDoc& rhs)
    {
        copy(rhs);
        type = rhs.type;
    }

    ~LottieTextDoc()
    {
        release();
    }

    void release()
    {
        if (exp) {
            delete(exp);
            exp = nullptr;
        }

        if (value.text) {
            free(value.text);
            value.text = nullptr;
        }
        if (value.name) {
            free(value.name);
            value.name = nullptr;
        }

        if (!frames) return;

        for (auto p = frames->begin(); p < frames->end(); ++p) {
            free((*p).value.text);
            free((*p).value.name);
        }
        delete(frames);
        frames = nullptr;
    }

    uint32_t nearest(float frameNo) override
    {
        return _nearest(frames, frameNo);
    }

    uint32_t frameCnt() override
    {
        return frames ? frames->count : 1;
    }

    float frameNo(int32_t key) override
    {
        return _frameNo(frames, key);
    }

    LottieScalarFrame<TextDocument>& newFrame()
    {
        if (!frames) frames = new Array<LottieScalarFrame<TextDocument>>;
        if (frames->count + 1 >= frames->reserved) {
            auto old = frames->reserved;
            frames->grow(frames->count + 2);
            memset((void*)(frames->data + old), 0x00, sizeof(LottieScalarFrame<TextDocument>) * (frames->reserved - old));
        }
        ++frames->count;
        return frames->last();
    }

    LottieScalarFrame<TextDocument>& nextFrame()
    {
        return (*frames)[frames->count];
    }

    TextDocument& operator()(float frameNo)
    {
        if (!frames) return value;
        if (frames->count == 1 || frameNo <= frames->first().no) return frames->first().value;
        if (frameNo >= frames->last().no) return frames->last().value;

        auto frame = frames->data + _bsearch(frames, frameNo);
        return frame->value;
    }

    void copy(const LottieTextDoc& rhs, bool shallow = true)
    {
        if (rhs.frames) {
            if (shallow) {
                frames = rhs.frames;
                const_cast<LottieTextDoc&>(rhs).frames = nullptr;
            } else {
                frames = new Array<LottieScalarFrame<TextDocument>>;
                *frames = *rhs.frames;
            }
        } else {
            value = rhs.value;
            const_cast<LottieTextDoc&>(rhs).value.text = nullptr;
            const_cast<LottieTextDoc&>(rhs).value.name = nullptr;
        }
    }

    void prepare() {}
};


struct LottieBitmap : LottieProperty
{
    union {
        char* b64Data = nullptr;
        char* path;
    };
    char* mimeType = nullptr;
    uint32_t size = 0;
    float width = 0.0f;
    float height = 0.0f;

    LottieBitmap() {}

    LottieBitmap(const LottieBitmap& rhs)
    {
        copy(rhs);
        type = rhs.type;
    }

    ~LottieBitmap()
    {
        release();
    }

    void release()
    {
        free(b64Data);
        free(mimeType);

        b64Data = nullptr;
        mimeType = nullptr;
    }

    uint32_t frameCnt() override { return 0; }
    uint32_t nearest(float time) override { return 0; }
    float frameNo(int32_t key) override { return 0; }

    void copy(const LottieBitmap& rhs, bool shallow = true)
    {
        if (shallow) {
            b64Data = rhs.b64Data;
            mimeType = rhs.mimeType;
        } else {
            //TODO: optimize here by avoiding data copy
            TVGLOG("LOTTIE", "Shallow copy of the image data!");
            b64Data = strdup(rhs.b64Data);
            mimeType = strdup(rhs.mimeType);
        }
        size = rhs.size;
        width = rhs.width;
        height = rhs.height;

        const_cast<LottieBitmap&>(rhs).b64Data = nullptr;
        const_cast<LottieBitmap&>(rhs).mimeType = nullptr;
    }
};


using LottiePoint = LottieGenericProperty<Point>;
using LottieFloat = LottieGenericProperty<float>;
using LottieOpacity = LottieGenericProperty<uint8_t>;
using LottieColor = LottieGenericProperty<RGB24>;
using LottieSlider = LottieFloat;
using LottieAngle = LottieFloat;
using LottieCheckbox = LottieGenericProperty<int8_t>;

#endif //_TVG_LOTTIE_PROPERTY_H_

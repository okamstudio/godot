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

#ifndef _TVG_RENDER_H_
#define _TVG_RENDER_H_

#include <math.h>
#include <cstdarg>
#include "tvgCommon.h"
#include "tvgArray.h"
#include "tvgLock.h"

namespace tvg
{

using RenderData = void*;
using pixel_t = uint32_t;

enum RenderUpdateFlag : uint8_t {None = 0, Path = 1, Color = 2, Gradient = 4, Stroke = 8, Transform = 16, Image = 32, GradientStroke = 64, Blend = 128, All = 255};
enum CompositionFlag : uint8_t {Invalid = 0, Opacity = 1, Blending = 2, Masking = 4, PostProcessing = 8};  //Composition Purpose

//TODO: Move this in public header unifying with SwCanvas::Colorspace
enum ColorSpace : uint8_t
{
    ABGR8888 = 0,      //The channels are joined in the order: alpha, blue, green, red. Colors are alpha-premultiplied.
    ARGB8888,          //The channels are joined in the order: alpha, red, green, blue. Colors are alpha-premultiplied.
    ABGR8888S,         //The channels are joined in the order: alpha, blue, green, red. Colors are un-alpha-premultiplied.
    ARGB8888S,         //The channels are joined in the order: alpha, red, green, blue. Colors are un-alpha-premultiplied.
    Grayscale8,        //One single channel data.
    Unsupported        //TODO: Change to the default, At the moment, we put it in the last to align with SwCanvas::Colorspace.
};

struct RenderSurface
{
    union {
        pixel_t* data = nullptr;    //system based data pointer
        uint32_t* buf32;            //for explicit 32bits channels
        uint8_t*  buf8;             //for explicit 8bits grayscale
    };
    Key key;                        //a reserved lock for the thread safety
    uint32_t stride = 0;
    uint32_t w = 0, h = 0;
    ColorSpace cs = ColorSpace::Unsupported;
    uint8_t channelSize = 0;
    bool premultiplied = false;         //Alpha-premultiplied

    RenderSurface()
    {
    }

    RenderSurface(const RenderSurface* rhs)
    {
        data = rhs->data;
        stride = rhs->stride;
        w = rhs->w;
        h = rhs->h;
        cs = rhs->cs;
        channelSize = rhs->channelSize;
        premultiplied = rhs->premultiplied;
    }


};

struct RenderCompositor
{
    CompositeMethod method;
    uint8_t opacity;
};

struct RenderRegion
{
    int32_t x, y, w, h;

    void intersect(const RenderRegion& rhs);
    void add(const RenderRegion& rhs);

    bool operator==(const RenderRegion& rhs) const
    {
        if (x == rhs.x && y == rhs.y && w == rhs.w && h == rhs.h) return true;
        return false;
    }
};

struct RenderStroke
{
    float width = 0.0f;
    uint8_t color[4] = {0, 0, 0, 0};
    Fill *fill = nullptr;
    float* dashPattern = nullptr;
    uint32_t dashCnt = 0;
    float dashOffset = 0.0f;
    StrokeCap cap = StrokeCap::Square;
    StrokeJoin join = StrokeJoin::Bevel;
    float miterlimit = 4.0f;
    bool strokeFirst = false;

    struct {
        float begin = 0.0f;
        float end = 1.0f;
        bool simultaneous = true;
    } trim;

    void operator=(const RenderStroke& rhs)
    {
        width = rhs.width;

        memcpy(color, rhs.color, sizeof(color));

        delete(fill);
        if (rhs.fill) fill = rhs.fill->duplicate();
        else fill = nullptr;

        free(dashPattern);
        if (rhs.dashCnt > 0) {
            dashPattern = static_cast<float*>(malloc(sizeof(float) * rhs.dashCnt));
            memcpy(dashPattern, rhs.dashPattern, sizeof(float) * rhs.dashCnt);
        } else {
            dashPattern = nullptr;
        }
        dashCnt = rhs.dashCnt;
        dashOffset = rhs.dashOffset;
        miterlimit = rhs.miterlimit;
        cap = rhs.cap;
        join = rhs.join;
        strokeFirst = rhs.strokeFirst;
        trim = rhs.trim;
    }

    bool strokeTrim(float& begin, float& end) const
    {
        begin = trim.begin;
        end = trim.end;

        if (fabsf(end - begin) >= 1.0f) {
            begin = 0.0f;
            end = 1.0f;
            return false;
        }

        auto loop = true;

        if (begin > 1.0f && end > 1.0f) loop = false;
        if (begin < 0.0f && end < 0.0f) loop = false;
        if (begin >= 0.0f && begin <= 1.0f && end >= 0.0f  && end <= 1.0f) loop = false;

        if (begin > 1.0f) begin -= 1.0f;
        if (begin < 0.0f) begin += 1.0f;
        if (end > 1.0f) end -= 1.0f;
        if (end < 0.0f) end += 1.0f;

        if ((loop && begin < end) || (!loop && begin > end)) std::swap(begin, end);
        return true;
    }

    ~RenderStroke()
    {
        free(dashPattern);
        delete(fill);
    }
};

struct RenderShape
{
    struct
    {
        Array<PathCommand> cmds;
        Array<Point> pts;
    } path;

    Fill *fill = nullptr;
    RenderStroke *stroke = nullptr;
    uint8_t color[4] = {0, 0, 0, 0};    //r, g, b, a
    FillRule rule = FillRule::Winding;

    ~RenderShape()
    {
        delete(fill);
        delete(stroke);
    }

    void fillColor(uint8_t* r, uint8_t* g, uint8_t* b, uint8_t* a) const
    {
        if (r) *r = color[0];
        if (g) *g = color[1];
        if (b) *b = color[2];
        if (a) *a = color[3];
    }

    float strokeWidth() const
    {
        if (!stroke) return 0;
        return stroke->width;
    }

    bool strokeTrim() const
    {
        if (!stroke) return false;
        if (stroke->trim.begin == 0.0f && stroke->trim.end == 1.0f) return false;
        if (fabsf(stroke->trim.end - stroke->trim.begin) >= 1.0f) return false;
        return true;
    }

    bool strokeColor(uint8_t* r, uint8_t* g, uint8_t* b, uint8_t* a) const
    {
        if (!stroke) return false;

        if (r) *r = stroke->color[0];
        if (g) *g = stroke->color[1];
        if (b) *b = stroke->color[2];
        if (a) *a = stroke->color[3];

        return true;
    }

    const Fill* strokeFill() const
    {
        if (!stroke) return nullptr;
        return stroke->fill;
    }

    uint32_t strokeDash(const float** dashPattern, float* offset) const
    {
        if (!stroke) return 0;
        if (dashPattern) *dashPattern = stroke->dashPattern;
        if (offset) *offset = stroke->dashOffset;
        return stroke->dashCnt;
    }

    StrokeCap strokeCap() const
    {
        if (!stroke) return StrokeCap::Square;
        return stroke->cap;
    }

    StrokeJoin strokeJoin() const
    {
        if (!stroke) return StrokeJoin::Bevel;
        return stroke->join;
    }

    float strokeMiterlimit() const
    {
        if (!stroke) return 4.0f;
        return stroke->miterlimit;;
    }
};

struct RenderEffect
{
    RenderData rd = nullptr;
    RenderRegion extend = {0, 0, 0, 0};
    SceneEffect type;
    bool valid = false;

    virtual ~RenderEffect()
    {
        free(rd);
    }
};

struct RenderEffectGaussianBlur : RenderEffect
{
    float sigma;
    uint8_t direction; //0: both, 1: horizontal, 2: vertical
    uint8_t border;    //0: duplicate, 1: wrap
    uint8_t quality;   //0 ~ 100  (optional)

    static RenderEffectGaussianBlur* gen(va_list& args)
    {
        auto inst = new RenderEffectGaussianBlur;
        inst->sigma = std::max((float) va_arg(args, double), 0.0f);
        inst->direction = std::min(va_arg(args, int), 2);
        inst->border = std::min(va_arg(args, int), 1);
        inst->quality = std::min(va_arg(args, int), 100);
        inst->type = SceneEffect::GaussianBlur;
        return inst;
    }
};

struct RenderEffectDropShadow : RenderEffect
{
    uint8_t color[4];  //rgba
    float angle;
    float distance;
    float sigma;
    uint8_t quality;   //0 ~ 100  (optional)

    static RenderEffectDropShadow* gen(va_list& args)
    {
        auto inst = new RenderEffectDropShadow;
        inst->color[0] = va_arg(args, int);
        inst->color[1] = va_arg(args, int);
        inst->color[2] = va_arg(args, int);
        inst->color[3] = va_arg(args, int);
        inst->angle = (float) va_arg(args, double);
        inst->distance = (float) va_arg(args, double);
        inst->sigma = std::max((float) va_arg(args, double), 0.0f);
        inst->quality = std::min(va_arg(args, int), 100);
        inst->type = SceneEffect::DropShadow;
        return inst;
    }
};

struct RenderEffectFill : RenderEffect
{
    uint8_t color[4];  //rgba

    static RenderEffectFill* gen(va_list& args)
    {
        auto inst = new RenderEffectFill;
        inst->color[0] = va_arg(args, int);
        inst->color[1] = va_arg(args, int);
        inst->color[2] = va_arg(args, int);
        inst->color[3] = va_arg(args, int);
        inst->type = SceneEffect::Fill;
        return inst;
    }
};

struct RenderEffectTint : RenderEffect
{
    uint8_t black[3];  //rgb
    uint8_t white[3];  //rgb
    uint8_t intensity; //0 - 255

    static RenderEffectTint* gen(va_list& args)
    {
        auto inst = new RenderEffectTint;
        inst->black[0] = va_arg(args, int);
        inst->black[1] = va_arg(args, int);
        inst->black[2] = va_arg(args, int);
        inst->white[0] = va_arg(args, int);
        inst->white[1] = va_arg(args, int);
        inst->white[2] = va_arg(args, int);
        inst->intensity = (uint8_t)(va_arg(args, double) * 2.55f);
        inst->type = SceneEffect::Tint;
        return inst;
    }
};

struct RenderEffectTritone : RenderEffect
{
    uint8_t shadow[3];       //rgb
    uint8_t midtone[3];      //rgb
    uint8_t highlight[3];    //rgb

    static RenderEffectTritone* gen(va_list& args)
    {
        auto inst = new RenderEffectTritone;
        inst->shadow[0] = va_arg(args, int);
        inst->shadow[1] = va_arg(args, int);
        inst->shadow[2] = va_arg(args, int);
        inst->midtone[0] = va_arg(args, int);
        inst->midtone[1] = va_arg(args, int);
        inst->midtone[2] = va_arg(args, int);
        inst->highlight[0] = va_arg(args, int);
        inst->highlight[1] = va_arg(args, int);
        inst->highlight[2] = va_arg(args, int);
        inst->type = SceneEffect::Tritone;
        return inst;
    }
};

class RenderMethod
{
private:
    uint32_t refCnt = 0;        //reference count
    Key key;

public:
    uint32_t ref();
    uint32_t unref();

    virtual ~RenderMethod() {}
    virtual RenderData prepare(const RenderShape& rshape, RenderData data, const Matrix& transform, Array<RenderData>& clips, uint8_t opacity, RenderUpdateFlag flags, bool clipper) = 0;
    virtual RenderData prepare(RenderSurface* surface, RenderData data, const Matrix& transform, Array<RenderData>& clips, uint8_t opacity, RenderUpdateFlag flags) = 0;
    virtual bool preRender() = 0;
    virtual bool renderShape(RenderData data) = 0;
    virtual bool renderImage(RenderData data) = 0;
    virtual bool postRender() = 0;
    virtual void dispose(RenderData data) = 0;
    virtual RenderRegion region(RenderData data) = 0;
    virtual RenderRegion viewport() = 0;
    virtual bool viewport(const RenderRegion& vp) = 0;
    virtual bool blend(BlendMethod method) = 0;
    virtual ColorSpace colorSpace() = 0;
    virtual const RenderSurface* mainSurface() = 0;

    virtual bool clear() = 0;
    virtual bool sync() = 0;

    virtual RenderCompositor* target(const RenderRegion& region, ColorSpace cs, CompositionFlag flags) = 0;
    virtual bool beginComposite(RenderCompositor* cmp, CompositeMethod method, uint8_t opacity) = 0;
    virtual bool endComposite(RenderCompositor* cmp) = 0;

    virtual bool prepare(RenderEffect* effect) = 0;
    virtual bool effect(RenderCompositor* cmp, const RenderEffect* effect, bool direct) = 0;
};

static inline bool MASK_REGION_MERGING(CompositeMethod method)
{
    switch(method) {
        case CompositeMethod::AlphaMask:
        case CompositeMethod::InvAlphaMask:
        case CompositeMethod::LumaMask:
        case CompositeMethod::InvLumaMask:
        case CompositeMethod::SubtractMask:
        case CompositeMethod::IntersectMask:
            return false;
        //these might expand the rendering region
        case CompositeMethod::AddMask:
        case CompositeMethod::DifferenceMask:
        case CompositeMethod::LightenMask:
        case CompositeMethod::DarkenMask:
            return true;
        default:
            TVGERR("RENDERER", "Unsupported Composite Method! = %d", (int)method);
            return false;
    }
}

static inline uint8_t CHANNEL_SIZE(ColorSpace cs)
{
    switch(cs) {
        case ColorSpace::ABGR8888:
        case ColorSpace::ABGR8888S:
        case ColorSpace::ARGB8888:
        case ColorSpace::ARGB8888S:
            return sizeof(uint32_t);
        case ColorSpace::Grayscale8:
            return sizeof(uint8_t);
        case ColorSpace::Unsupported:
        default:
            TVGERR("RENDERER", "Unsupported Channel Size! = %d", (int)cs);
            return 0;
    }
}

static inline ColorSpace COMPOSITE_TO_COLORSPACE(RenderMethod* renderer, CompositeMethod method)
{
    switch(method) {
        case CompositeMethod::AlphaMask:
        case CompositeMethod::InvAlphaMask:
        case CompositeMethod::AddMask:
        case CompositeMethod::DifferenceMask:
        case CompositeMethod::SubtractMask:
        case CompositeMethod::IntersectMask:
        case CompositeMethod::LightenMask:
        case CompositeMethod::DarkenMask:
            return ColorSpace::Grayscale8;
        //TODO: Optimize Luma/InvLuma colorspace to Grayscale8
        case CompositeMethod::LumaMask:
        case CompositeMethod::InvLumaMask:
            return renderer->colorSpace();
        default:
            TVGERR("RENDERER", "Unsupported Composite Size! = %d", (int)method);
            return ColorSpace::Unsupported;
    }
}

static inline uint8_t MULTIPLY(uint8_t c, uint8_t a)
{
    return (((c) * (a) + 0xff) >> 8);
}

}

#endif //_TVG_RENDER_H_

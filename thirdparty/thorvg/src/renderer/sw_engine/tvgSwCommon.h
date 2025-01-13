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

#ifndef _TVG_SW_COMMON_H_
#define _TVG_SW_COMMON_H_

#include <algorithm>
#include "tvgCommon.h"
#include "tvgRender.h"

#define SW_CURVE_TYPE_POINT 0
#define SW_CURVE_TYPE_CUBIC 1
#define SW_ANGLE_PI (180L << 16)
#define SW_ANGLE_2PI (SW_ANGLE_PI << 1)
#define SW_ANGLE_PI2 (SW_ANGLE_PI >> 1)

using SwCoord = signed long;
using SwFixed = signed long long;


static inline float TO_FLOAT(SwCoord val)
{
    return static_cast<float>(val) / 64.0f;
}

struct SwPoint
{
    SwCoord x, y;

    SwPoint& operator+=(const SwPoint& rhs)
    {
        x += rhs.x;
        y += rhs.y;
        return *this;
    }

    SwPoint operator+(const SwPoint& rhs) const
    {
        return {x + rhs.x, y + rhs.y};
    }

    SwPoint operator-(const SwPoint& rhs) const
    {
        return {x - rhs.x, y - rhs.y};
    }

    bool operator==(const SwPoint& rhs) const
    {
        return (x == rhs.x && y == rhs.y);
    }

    bool operator!=(const SwPoint& rhs) const
    {
        return (x != rhs.x || y != rhs.y);
    }

    bool zero() const
    {
        if (x == 0 && y == 0) return true;
        else return false;
    }

    bool small() const
    {
        //2 is epsilon...
        if (abs(x) < 2 && abs(y) < 2) return true;
        else return false;
    }

    Point toPoint() const
    {
        return {TO_FLOAT(x),  TO_FLOAT(y)};
    }
};

struct SwSize
{
    SwCoord w, h;
};

struct SwOutline
{
    Array<SwPoint> pts;             //the outline's points
    Array<uint32_t> cntrs;          //the contour end points
    Array<uint8_t> types;           //curve type
    Array<bool> closed;             //opened or closed path?
    FillRule fillRule;
};

struct SwSpan
{
    uint16_t x, y;
    uint16_t len;
    uint8_t coverage;
};

struct SwRle
{
    SwSpan *spans;
    uint32_t alloc;
    uint32_t size;
};

struct SwBBox
{
    SwPoint min, max;

    void reset()
    {
        min.x = min.y = max.x = max.y = 0;
    }
};

struct SwFill
{
    struct SwLinear {
        float dx, dy;
        float offset;
    };

    struct SwRadial {
        float a11, a12, a13;
        float a21, a22, a23;
        float fx, fy, fr;
        float dx, dy, dr;
        float invA, a;
    };

    union {
        SwLinear linear;
        SwRadial radial;
    };

    uint32_t* ctable;
    FillSpread spread;

    bool solid = false; //solid color fill with the last color from colorStops
    bool translucent;
};

struct SwStrokeBorder
{
    uint32_t ptsCnt;
    uint32_t maxPts;
    SwPoint* pts;
    uint8_t* tags;
    int32_t start;     //index of current sub-path start point
    bool movable;      //true: for ends of lineto borders
};

struct SwStroke
{
    SwFixed angleIn;
    SwFixed angleOut;
    SwPoint center;
    SwFixed lineLength;
    SwFixed subPathAngle;
    SwPoint ptStartSubPath;
    SwFixed subPathLineLength;
    SwFixed width;
    SwFixed miterlimit;

    StrokeCap cap;
    StrokeJoin join;
    StrokeJoin joinSaved;
    SwFill* fill = nullptr;

    SwStrokeBorder borders[2];

    float sx, sy;

    bool firstPt;
    bool closedSubPath;
    bool handleWideStrokes;
};

struct SwDashStroke
{
    SwOutline* outline = nullptr;
    float curLen = 0;
    int32_t curIdx = 0;
    Point ptStart = {0, 0};
    Point ptCur = {0, 0};
    float* pattern = nullptr;
    uint32_t cnt = 0;
    bool curOpGap = false;
    bool move = true;
};

struct SwShape
{
    SwOutline*   outline = nullptr;
    SwStroke*    stroke = nullptr;
    SwFill*      fill = nullptr;
    SwRle*   rle = nullptr;
    SwRle*   strokeRle = nullptr;
    SwBBox       bbox;           //Keep it boundary without stroke region. Using for optimal filling.

    bool         fastTrack = false;   //Fast Track: axis-aligned rectangle without any clips?
};

struct SwImage
{
    SwOutline*   outline = nullptr;
    SwRle*   rle = nullptr;
    union {
        pixel_t*  data;      //system based data pointer
        uint32_t* buf32;     //for explicit 32bits channels
        uint8_t*  buf8;      //for explicit 8bits grayscale
    };
    uint32_t     w, h, stride;
    int32_t      ox = 0;         //offset x
    int32_t      oy = 0;         //offset y
    float        scale;
    uint8_t      channelSize;

    bool         direct = false;  //draw image directly (with offset)
    bool         scaled = false;  //draw scaled image
};

typedef uint8_t(*SwMask)(uint8_t s, uint8_t d, uint8_t a);                  //src, dst, alpha
typedef uint32_t(*SwBlender)(uint32_t s, uint32_t d, uint8_t a);            //src, dst, alpha
typedef uint32_t(*SwJoin)(uint8_t r, uint8_t g, uint8_t b, uint8_t a);      //color channel join
typedef uint8_t(*SwAlpha)(uint8_t*);                                        //blending alpha

struct SwCompositor;

struct SwSurface : RenderSurface
{
    SwJoin  join;
    SwAlpha alphas[4];                    //Alpha:2, InvAlpha:3, Luma:4, InvLuma:5
    SwBlender blender = nullptr;          //blender (optional)
    SwCompositor* compositor = nullptr;   //compositor (optional)
    BlendMethod blendMethod = BlendMethod::Normal;

    SwAlpha alpha(CompositeMethod method)
    {
        auto idx = (int)(method) - 2;       //0: None, 1: ClipPath
        return alphas[idx > 3 ? 0 : idx];   //CompositeMethod has only four Matting methods.
    }

    SwSurface()
    {
    }

    SwSurface(const SwSurface* rhs) : RenderSurface(rhs)
    {
        join = rhs->join;
        memcpy(alphas, rhs->alphas, sizeof(alphas));
        blender = rhs->blender;
        compositor = rhs->compositor;
        blendMethod = rhs->blendMethod;
     }
};

struct SwCompositor : RenderCompositor
{
    SwSurface* recoverSfc;                  //Recover surface when composition is started
    SwCompositor* recoverCmp;               //Recover compositor when composition is done
    SwImage image;
    SwBBox bbox;
    bool valid;
};

struct SwMpool
{
    SwOutline* outline;
    SwOutline* strokeOutline;
    SwOutline* dashOutline;
    unsigned allocSize;
};

static inline SwCoord TO_SWCOORD(float val)
{
    return SwCoord(val * 64.0f);
}

static inline uint32_t JOIN(uint8_t c0, uint8_t c1, uint8_t c2, uint8_t c3)
{
    return (c0 << 24 | c1 << 16 | c2 << 8 | c3);
}

static inline uint32_t ALPHA_BLEND(uint32_t c, uint32_t a)
{
    ++a;
    return (((((c >> 8) & 0x00ff00ff) * a) & 0xff00ff00) + ((((c & 0x00ff00ff) * a) >> 8) & 0x00ff00ff));
}

static inline uint32_t INTERPOLATE(uint32_t s, uint32_t d, uint8_t a)
{
    return (((((((s >> 8) & 0xff00ff) - ((d >> 8) & 0xff00ff)) * a) + (d & 0xff00ff00)) & 0xff00ff00) + ((((((s & 0xff00ff) - (d & 0xff00ff)) * a) >> 8) + (d & 0xff00ff)) & 0xff00ff));
}

static inline uint8_t INTERPOLATE8(uint8_t s, uint8_t d, uint8_t a)
{
    return (((s) * (a) + 0xff) >> 8) + (((d) * ~(a) + 0xff) >> 8);
}

static inline SwCoord HALF_STROKE(float width)
{
    return TO_SWCOORD(width * 0.5f);
}

static inline uint8_t A(uint32_t c)
{
    return ((c) >> 24);
}

static inline uint8_t IA(uint32_t c)
{
    return (~(c) >> 24);
}

static inline uint8_t C1(uint32_t c)
{
    return ((c) >> 16);
}

static inline uint8_t C2(uint32_t c)
{
    return ((c) >> 8);
}

static inline uint8_t C3(uint32_t c)
{
    return (c);
}

static inline uint32_t opBlendInterp(uint32_t s, uint32_t d, uint8_t a)
{
    return INTERPOLATE(s, d, a);
}

static inline uint32_t opBlendNormal(uint32_t s, uint32_t d, uint8_t a)
{
    auto t = ALPHA_BLEND(s, a);
    return t + ALPHA_BLEND(d, IA(t));
}

static inline uint32_t opBlendPreNormal(uint32_t s, uint32_t d, TVG_UNUSED uint8_t a)
{
    return s + ALPHA_BLEND(d, IA(s));
}

static inline uint32_t opBlendSrcOver(uint32_t s, TVG_UNUSED uint32_t d, TVG_UNUSED uint8_t a)
{
    return s;
}

//TODO: BlendMethod could remove the alpha parameter.
static inline uint32_t opBlendDifference(uint32_t s, uint32_t d, TVG_UNUSED uint8_t a)
{
    //if (s > d) => s - d
    //else => d - s
    auto c1 = (C1(s) > C1(d)) ? (C1(s) - C1(d)) : (C1(d) - C1(s));
    auto c2 = (C2(s) > C2(d)) ? (C2(s) - C2(d)) : (C2(d) - C2(s));
    auto c3 = (C3(s) > C3(d)) ? (C3(s) - C3(d)) : (C3(d) - C3(s));
    return JOIN(255, c1, c2, c3);
}

static inline uint32_t opBlendExclusion(uint32_t s, uint32_t d, TVG_UNUSED uint8_t a)
{
    //A + B - 2AB
    auto c1 = std::min(255, C1(s) + C1(d) - std::min(255, (C1(s) * C1(d)) << 1));
    auto c2 = std::min(255, C2(s) + C2(d) - std::min(255, (C2(s) * C2(d)) << 1));
    auto c3 = std::min(255, C3(s) + C3(d) - std::min(255, (C3(s) * C3(d)) << 1));
    return JOIN(255, c1, c2, c3);
}

static inline uint32_t opBlendAdd(uint32_t s, uint32_t d, TVG_UNUSED uint8_t a)
{
    // s + d
    auto c1 = std::min(C1(s) + C1(d), 255);
    auto c2 = std::min(C2(s) + C2(d), 255);
    auto c3 = std::min(C3(s) + C3(d), 255);
    return JOIN(255, c1, c2, c3);
}

static inline uint32_t opBlendScreen(uint32_t s, uint32_t d, TVG_UNUSED uint8_t a)
{
    // s + d - s * d
    auto c1 = C1(s) + C1(d) - MULTIPLY(C1(s), C1(d));
    auto c2 = C2(s) + C2(d) - MULTIPLY(C2(s), C2(d));
    auto c3 = C3(s) + C3(d) - MULTIPLY(C3(s), C3(d));
    return JOIN(255, c1, c2, c3);
}

static inline uint32_t opBlendMultiply(uint32_t s, uint32_t d, TVG_UNUSED uint8_t a)
{
    // s * d
    auto c1 = MULTIPLY(C1(s), C1(d));
    auto c2 = MULTIPLY(C2(s), C2(d));
    auto c3 = MULTIPLY(C3(s), C3(d));
    return JOIN(255, c1, c2, c3);
}

static inline uint32_t opBlendOverlay(uint32_t s, uint32_t d, TVG_UNUSED uint8_t a)
{
    // if (2 * d < da) => 2 * s * d,
    // else => 1 - 2 * (1 - s) * (1 - d)
    auto c1 = (C1(d) < 128) ? std::min(255, 2 * MULTIPLY(C1(s), C1(d))) : (255 - std::min(255, 2 * MULTIPLY(255 - C1(s), 255 - C1(d))));
    auto c2 = (C2(d) < 128) ? std::min(255, 2 * MULTIPLY(C2(s), C2(d))) : (255 - std::min(255, 2 * MULTIPLY(255 - C2(s), 255 - C2(d))));
    auto c3 = (C3(d) < 128) ? std::min(255, 2 * MULTIPLY(C3(s), C3(d))) : (255 - std::min(255, 2 * MULTIPLY(255 - C3(s), 255 - C3(d))));
    return JOIN(255, c1, c2, c3);
}

static inline uint32_t opBlendDarken(uint32_t s, uint32_t d, TVG_UNUSED uint8_t a)
{
    // min(s, d)
    auto c1 = std::min(C1(s), C1(d));
    auto c2 = std::min(C2(s), C2(d));
    auto c3 = std::min(C3(s), C3(d));
    return JOIN(255, c1, c2, c3);
}

static inline uint32_t opBlendLighten(uint32_t s, uint32_t d, TVG_UNUSED uint8_t a)
{
    // max(s, d)
    auto c1 = std::max(C1(s), C1(d));
    auto c2 = std::max(C2(s), C2(d));
    auto c3 = std::max(C3(s), C3(d));
    return JOIN(255, c1, c2, c3);
}

static inline uint32_t opBlendColorDodge(uint32_t s, uint32_t d, TVG_UNUSED uint8_t a)
{
    // d / (1 - s)
    auto is = 0xffffffff - s;
    auto c1 = (C1(is) > 0) ? (C1(d) / C1(is)) : C1(d);
    auto c2 = (C2(is) > 0) ? (C2(d) / C2(is)) : C2(d);
    auto c3 = (C3(is) > 0) ? (C3(d) / C3(is)) : C3(d);
    return JOIN(255, c1, c2, c3);
}

static inline uint32_t opBlendColorBurn(uint32_t s, uint32_t d, TVG_UNUSED uint8_t a)
{
    // 1 - (1 - d) / s
    auto id = 0xffffffff - d;
    auto c1 = 255 - ((C1(s) > 0) ? (C1(id) / C1(s)) : C1(id));
    auto c2 = 255 - ((C2(s) > 0) ? (C2(id) / C2(s)) : C2(id));
    auto c3 = 255 - ((C3(s) > 0) ? (C3(id) / C3(s)) : C3(id));
    return JOIN(255, c1, c2, c3);
}

static inline uint32_t opBlendHardLight(uint32_t s, uint32_t d, TVG_UNUSED uint8_t a)
{
    auto c1 = (C1(s) < 128) ? std::min(255, 2 * MULTIPLY(C1(s), C1(d))) : (255 - std::min(255, 2 * MULTIPLY(255 - C1(s), 255 - C1(d))));
    auto c2 = (C2(s) < 128) ? std::min(255, 2 * MULTIPLY(C2(s), C2(d))) : (255 - std::min(255, 2 * MULTIPLY(255 - C2(s), 255 - C2(d))));
    auto c3 = (C3(s) < 128) ? std::min(255, 2 * MULTIPLY(C3(s), C3(d))) : (255 - std::min(255, 2 * MULTIPLY(255 - C3(s), 255 - C3(d))));
    return JOIN(255, c1, c2, c3);
}

static inline uint32_t opBlendSoftLight(uint32_t s, uint32_t d, TVG_UNUSED uint8_t a)
{
    //(255 - 2 * s) * (d * d) + (2 * s * b)
    auto c1 = std::min(255, MULTIPLY(255 - std::min(255, 2 * C1(s)), MULTIPLY(C1(d), C1(d))) + 2 * MULTIPLY(C1(s), C1(d)));
    auto c2 = std::min(255, MULTIPLY(255 - std::min(255, 2 * C2(s)), MULTIPLY(C2(d), C2(d))) + 2 * MULTIPLY(C2(s), C2(d)));
    auto c3 = std::min(255, MULTIPLY(255 - std::min(255, 2 * C3(s)), MULTIPLY(C3(d), C3(d))) + 2 * MULTIPLY(C3(s), C3(d)));
    return JOIN(255, c1, c2, c3);
}


int64_t mathMultiply(int64_t a, int64_t b);
int64_t mathDivide(int64_t a, int64_t b);
int64_t mathMulDiv(int64_t a, int64_t b, int64_t c);
void mathRotate(SwPoint& pt, SwFixed angle);
SwFixed mathTan(SwFixed angle);
SwFixed mathAtan(const SwPoint& pt);
SwFixed mathCos(SwFixed angle);
SwFixed mathSin(SwFixed angle);
void mathSplitCubic(SwPoint* base);
void mathSplitLine(SwPoint* base);
SwFixed mathDiff(SwFixed angle1, SwFixed angle2);
SwFixed mathLength(const SwPoint& pt);
int mathCubicAngle(const SwPoint* base, SwFixed& angleIn, SwFixed& angleMid, SwFixed& angleOut);
SwFixed mathMean(SwFixed angle1, SwFixed angle2);
SwPoint mathTransform(const Point* to, const Matrix& transform);
bool mathUpdateOutlineBBox(const SwOutline* outline, const SwBBox& clipRegion, SwBBox& renderRegion, bool fastTrack);
bool mathClipBBox(const SwBBox& clipper, SwBBox& clippee);

void shapeReset(SwShape* shape);
bool shapePrepare(SwShape* shape, const RenderShape* rshape, const Matrix& transform, const SwBBox& clipRegion, SwBBox& renderRegion, SwMpool* mpool, unsigned tid, bool hasComposite);
bool shapePrepared(const SwShape* shape);
bool shapeGenRle(SwShape* shape, const RenderShape* rshape, bool antiAlias);
void shapeDelOutline(SwShape* shape, SwMpool* mpool, uint32_t tid);
void shapeResetStroke(SwShape* shape, const RenderShape* rshape, const Matrix& transform);
bool shapeGenStrokeRle(SwShape* shape, const RenderShape* rshape, const Matrix& transform, const SwBBox& clipRegion, SwBBox& renderRegion, SwMpool* mpool, unsigned tid);
void shapeFree(SwShape* shape);
void shapeDelStroke(SwShape* shape);
bool shapeGenFillColors(SwShape* shape, const Fill* fill, const Matrix& transform, SwSurface* surface, uint8_t opacity, bool ctable);
bool shapeGenStrokeFillColors(SwShape* shape, const Fill* fill, const Matrix& transform, SwSurface* surface, uint8_t opacity, bool ctable);
void shapeResetFill(SwShape* shape);
void shapeResetStrokeFill(SwShape* shape);
void shapeDelFill(SwShape* shape);
void shapeDelStrokeFill(SwShape* shape);

void strokeReset(SwStroke* stroke, const RenderShape* shape, const Matrix& transform);
bool strokeParseOutline(SwStroke* stroke, const SwOutline& outline);
SwOutline* strokeExportOutline(SwStroke* stroke, SwMpool* mpool, unsigned tid);
void strokeFree(SwStroke* stroke);

bool imagePrepare(SwImage* image, const Matrix& transform, const SwBBox& clipRegion, SwBBox& renderRegion, SwMpool* mpool, unsigned tid);
bool imageGenRle(SwImage* image, const SwBBox& renderRegion, bool antiAlias);
void imageDelOutline(SwImage* image, SwMpool* mpool, uint32_t tid);
void imageReset(SwImage* image);
void imageFree(SwImage* image);

bool fillGenColorTable(SwFill* fill, const Fill* fdata, const Matrix& transform, SwSurface* surface, uint8_t opacity, bool ctable);
const Fill::ColorStop* fillFetchSolid(const SwFill* fill, const Fill* fdata);
void fillReset(SwFill* fill);
void fillFree(SwFill* fill);

//OPTIMIZE_ME: Skip the function pointer access
void fillLinear(const SwFill* fill, uint8_t* dst, uint32_t y, uint32_t x, uint32_t len, SwMask maskOp, uint8_t opacity);                                   //composite masking ver.
void fillLinear(const SwFill* fill, uint8_t* dst, uint32_t y, uint32_t x, uint32_t len, uint8_t* cmp, SwMask maskOp, uint8_t opacity);                     //direct masking ver.
void fillLinear(const SwFill* fill, uint32_t* dst, uint32_t y, uint32_t x, uint32_t len, SwBlender op, uint8_t a);                                         //blending ver.
void fillLinear(const SwFill* fill, uint32_t* dst, uint32_t y, uint32_t x, uint32_t len, SwBlender op, SwBlender op2, uint8_t a);                          //blending + BlendingMethod(op2) ver.
void fillLinear(const SwFill* fill, uint32_t* dst, uint32_t y, uint32_t x, uint32_t len, uint8_t* cmp, SwAlpha alpha, uint8_t csize, uint8_t opacity);     //matting ver.

void fillRadial(const SwFill* fill, uint8_t* dst, uint32_t y, uint32_t x, uint32_t len, SwMask op, uint8_t a);                                             //composite masking ver.
void fillRadial(const SwFill* fill, uint8_t* dst, uint32_t y, uint32_t x, uint32_t len, uint8_t* cmp, SwMask op, uint8_t a) ;                              //direct masking ver.
void fillRadial(const SwFill* fill, uint32_t* dst, uint32_t y, uint32_t x, uint32_t len, SwBlender op, uint8_t a);                                         //blending ver.
void fillRadial(const SwFill* fill, uint32_t* dst, uint32_t y, uint32_t x, uint32_t len, SwBlender op, SwBlender op2, uint8_t a);                          //blending + BlendingMethod(op2) ver.
void fillRadial(const SwFill* fill, uint32_t* dst, uint32_t y, uint32_t x, uint32_t len, uint8_t* cmp, SwAlpha alpha, uint8_t csize, uint8_t opacity);     //matting ver.

SwRle* rleRender(SwRle* rle, const SwOutline* outline, const SwBBox& renderRegion, bool antiAlias);
SwRle* rleRender(const SwBBox* bbox);
void rleFree(SwRle* rle);
void rleReset(SwRle* rle);
void rleMerge(SwRle* rle, SwRle* clip1, SwRle* clip2);
void rleClip(SwRle* rle, const SwRle* clip);
void rleClip(SwRle* rle, const SwBBox* clip);

SwMpool* mpoolInit(uint32_t threads);
bool mpoolTerm(SwMpool* mpool);
bool mpoolClear(SwMpool* mpool);
SwOutline* mpoolReqOutline(SwMpool* mpool, unsigned idx);
void mpoolRetOutline(SwMpool* mpool, unsigned idx);
SwOutline* mpoolReqStrokeOutline(SwMpool* mpool, unsigned idx);
void mpoolRetStrokeOutline(SwMpool* mpool, unsigned idx);
SwOutline* mpoolReqDashOutline(SwMpool* mpool, unsigned idx);
void mpoolRetDashOutline(SwMpool* mpool, unsigned idx);

bool rasterCompositor(SwSurface* surface);
bool rasterGradientShape(SwSurface* surface, SwShape* shape, const Fill* fdata, uint8_t opacity);
bool rasterShape(SwSurface* surface, SwShape* shape, uint8_t r, uint8_t g, uint8_t b, uint8_t a);
bool rasterImage(SwSurface* surface, SwImage* image, const Matrix& transform, const SwBBox& bbox, uint8_t opacity);
bool rasterStroke(SwSurface* surface, SwShape* shape, uint8_t r, uint8_t g, uint8_t b, uint8_t a);
bool rasterGradientStroke(SwSurface* surface, SwShape* shape, const Fill* fdata, uint8_t opacity);
bool rasterClear(SwSurface* surface, uint32_t x, uint32_t y, uint32_t w, uint32_t h, pixel_t val = 0);
void rasterPixel32(uint32_t *dst, uint32_t val, uint32_t offset, int32_t len);
void rasterTranslucentPixel32(uint32_t* dst, uint32_t* src, uint32_t len, uint8_t opacity);
void rasterPixel32(uint32_t* dst, uint32_t* src, uint32_t len, uint8_t opacity);
void rasterGrayscale8(uint8_t *dst, uint8_t val, uint32_t offset, int32_t len);
void rasterXYFlip(uint32_t* src, uint32_t* dst, int32_t stride, int32_t w, int32_t h, const SwBBox& bbox, bool flipped);
void rasterUnpremultiply(RenderSurface* surface);
void rasterPremultiply(RenderSurface* surface);
bool rasterConvertCS(RenderSurface* surface, ColorSpace to);

bool effectGaussianBlur(SwCompositor* cmp, SwSurface* surface, const RenderEffectGaussianBlur* params);
bool effectGaussianBlurPrepare(RenderEffectGaussianBlur* effect);
bool effectDropShadow(SwCompositor* cmp, SwSurface* surfaces[2], const RenderEffectDropShadow* params, uint8_t opacity, bool direct);
bool effectDropShadowPrepare(RenderEffectDropShadow* effect);

#endif /* _TVG_SW_COMMON_H_ */

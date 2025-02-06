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

#ifndef _TVG_LOTTIE_MODEL_H_
#define _TVG_LOTTIE_MODEL_H_

#include <cstring>

#include "tvgCommon.h"
#include "tvgRender.h"
#include "tvgLottieProperty.h"
#include "tvgLottieRenderPooler.h"


struct LottieComposition;

struct LottieStroke
{
    struct DashAttr
    {
        //0: offset, 1: dash, 2: gap
        LottieFloat value[3] = {0.0f, 0.0f, 0.0f};
    };

    virtual ~LottieStroke()
    {
        delete(dashattr);
    }

    LottieFloat& dash(int no)
    {
        if (!dashattr) dashattr = new DashAttr;
        return dashattr->value[no];
    }

    float dashOffset(float frameNo, LottieExpressions* exps)
    {
        return dash(0)(frameNo, exps);
    }

    float dashGap(float frameNo, LottieExpressions* exps)
    {
        return dash(2)(frameNo, exps);
    }

    float dashSize(float frameNo, LottieExpressions* exps)
    {
        auto d = dash(1)(frameNo, exps);
        if (d == 0.0f) return 0.1f;
        else return d;
    }

    LottieFloat width = 0.0f;
    DashAttr* dashattr = nullptr;
    float miterLimit = 0;
    StrokeCap cap = StrokeCap::Round;
    StrokeJoin join = StrokeJoin::Round;
};


struct LottieEffect
{
    enum Type : uint8_t
    {
        DropShadow = 0,
        GaussianBlur = 1,
    };

    virtual ~LottieEffect() {}

    Type type;
    bool enable = false;
};


struct LottieDropShadow : LottieEffect
{
    LottieColor color;
    LottieFloat opacity = 0;
    LottieAngle angle = 0.0f;
    LottieSlider distance = 0.0f;
    LottieSlider blurness = 0.0f;

    LottieDropShadow()
    {
        type = DropShadow;
    }
};


struct LottieGaussianBlur : LottieEffect
{
    LottieSlider blurness = 0.0f;
    LottieCheckbox direction = 0;
    LottieCheckbox wrap = 0;

    LottieGaussianBlur()
    {
        type = GaussianBlur;
    }
};


struct LottieMask
{
    LottiePathSet pathset;
    LottieFloat expand = 0.0f;
    LottieOpacity opacity = 255;
    CompositeMethod method;
    bool inverse = false;
};


struct LottieObject
{
    enum Type : uint8_t
    {
        Composition = 0,
        Layer,
        Group,
        Transform,
        SolidFill,
        SolidStroke,
        GradientFill,
        GradientStroke,
        Rect,
        Ellipse,
        Path,
        Polystar,
        Image,
        Trimpath,
        Text,
        Repeater,
        RoundedCorner,
        OffsetPath
    };

    virtual ~LottieObject()
    {
    }

    virtual void override(LottieProperty* prop, bool shallow, bool byDefault)
    {
        TVGERR("LOTTIE", "Unsupported slot type");
    }

    virtual bool mergeable() { return false; }
    virtual LottieProperty* property(uint16_t ix) { return nullptr; }

    unsigned long id = 0;
    Type type;
    bool hidden = false;       //remove?
};


struct LottieGlyph
{
    Array<LottieObject*> children;   //glyph shapes.
    float width;
    char* code;
    char* family = nullptr;
    char* style = nullptr;
    uint16_t size;
    uint8_t len;

    void prepare()
    {
        len = strlen(code);
    }

    ~LottieGlyph()
    {
        for (auto p = children.begin(); p < children.end(); ++p) delete(*p);
        free(code);
    }
};


struct LottieTextRange
{
    enum Based : uint8_t { Chars = 1, CharsExcludingSpaces, Words, Lines };
    enum Shape : uint8_t { Square = 1, RampUp, RampDown, Triangle, Round, Smooth };
    enum Unit : uint8_t { Percent = 1, Index };

    ~LottieTextRange()
    {
        free(interpolator);
    }

    struct {
        LottieColor fillColor = RGB24{255, 255, 255};
        LottieColor strokeColor = RGB24{255, 255, 255};
        LottiePosition position = Point{0, 0};
        LottiePoint scale = Point{100, 100};
        LottieFloat letterSpacing = 0.0f;
        LottieFloat lineSpacing = 0.0f;
        LottieFloat strokeWidth = 0.0f;
        LottieFloat rotation = 0.0f;
        LottieOpacity fillOpacity = 255;
        LottieOpacity strokeOpacity = 255;
        LottieOpacity opacity = 255;
    } style;

    LottieFloat offset = 0.0f;
    LottieFloat maxEase = 0.0f;
    LottieFloat minEase = 0.0f;
    LottieFloat maxAmount = 0.0f;
    LottieFloat smoothness = 0.0f;
    LottieFloat start = 0.0f;
    LottieFloat end = FLT_MAX;
    LottieInterpolator* interpolator = nullptr;
    Based based = Chars;
    Shape shape = Square;
    Unit rangeUnit = Percent;
    uint8_t random = 0;
    bool expressible = false;

    float factor(float frameNo, float totalLen, float idx);
};


struct LottieFont
{
    enum Origin : uint8_t { Local = 0, CssURL, ScriptURL, FontURL, Embedded };

    ~LottieFont()
    {
        for (auto c = chars.begin(); c < chars.end(); ++c) delete(*c);
        free(style);
        free(family);
        free(name);
    }

    Array<LottieGlyph*> chars;
    char* name = nullptr;
    char* family = nullptr;
    char* style = nullptr;
    float ascent = 0.0f;
    Origin origin = Embedded;
};

struct LottieMarker
{
    char* name = nullptr;
    float time = 0.0f;
    float duration = 0.0f;
    
    ~LottieMarker()
    {
        free(name);
    }
};

struct LottieText : LottieObject, LottieRenderPooler<tvg::Shape>
{
    struct AlignOption
    {
        enum Group : uint8_t { Chars = 1, Word = 2, Line = 3, All = 4 };
        Group grouping = Chars;
        LottiePoint anchor{};
    } alignOption;

    LottieText()
    {
        LottieObject::type = LottieObject::Text;
    }

    void override(LottieProperty* prop, bool shallow, bool byDefault = false) override
    {
        if (byDefault) doc.release();
        doc.copy(*static_cast<LottieTextDoc*>(prop), shallow);
    }

    LottieProperty* property(uint16_t ix) override
    {
        if (doc.ix == ix) return &doc;
        return nullptr;
    }

    LottieTextDoc doc;
    LottieFont* font;
    Array<LottieTextRange*> ranges;

    ~LottieText()
    {
        for (auto r = ranges.begin(); r < ranges.end(); ++r) delete(*r);
    }
};


struct LottieTrimpath : LottieObject
{
    enum Type : uint8_t { Simultaneous = 1, Individual = 2 };

    LottieTrimpath()
    {
        LottieObject::type = LottieObject::Trimpath;
    }

    bool mergeable() override
    {
        if (!start.frames && start.value == 0.0f && !end.frames && end.value == 100.0f && !offset.frames && offset.value == 0.0f) return true;
        return false;
    }

    LottieProperty* property(uint16_t ix) override
    {
        if (start.ix == ix) return &start;
        if (end.ix == ix) return &end;
        if (offset.ix == ix) return &offset;
        return nullptr;
    }

    void segment(float frameNo, float& start, float& end, LottieExpressions* exps);

    LottieFloat start = 0.0f;
    LottieFloat end = 100.0f;
    LottieFloat offset = 0.0f;
    Type type = Simultaneous;
};


struct LottieShape : LottieObject, LottieRenderPooler<tvg::Shape>
{
    bool clockwise = true;   //clockwise or counter-clockwise

    virtual ~LottieShape() {}

    bool mergeable() override
    {
        return true;
    }

    LottieShape(LottieObject::Type type)
    {
        LottieObject::type = type;
    }
};


struct LottieRoundedCorner : LottieObject
{
    LottieRoundedCorner()
    {
        LottieObject::type = LottieObject::RoundedCorner;
    }

    LottieProperty* property(uint16_t ix) override
    {
        if (radius.ix == ix) return &radius;
        return nullptr;
    }

    LottieFloat radius = 0.0f;
};


struct LottiePath : LottieShape
{
    LottiePath() : LottieShape(LottieObject::Path) {}

    LottieProperty* property(uint16_t ix) override
    {
        if (pathset.ix == ix) return &pathset;
        return nullptr;
    }

    LottiePathSet pathset;
};


struct LottieRect : LottieShape
{
    LottieRect() : LottieShape(LottieObject::Rect) {}

    LottieProperty* property(uint16_t ix) override
    {
        if (position.ix == ix) return &position;
        if (size.ix == ix) return &size;
        if (radius.ix == ix) return &radius;
        return nullptr;
    }

    LottiePosition position = Point{0.0f, 0.0f};
    LottiePoint size = Point{0.0f, 0.0f};
    LottieFloat radius = 0.0f;       //rounded corner radius
};


struct LottiePolyStar : LottieShape
{
    enum Type : uint8_t {Star = 1, Polygon};

    LottiePolyStar() : LottieShape(LottieObject::Polystar) {}

    LottieProperty* property(uint16_t ix) override
    {
        if (position.ix == ix) return &position;
        if (innerRadius.ix == ix) return &innerRadius;
        if (outerRadius.ix == ix) return &outerRadius;
        if (innerRoundness.ix == ix) return &innerRoundness;
        if (outerRoundness.ix == ix) return &outerRoundness;
        if (rotation.ix == ix) return &rotation;
        if (ptsCnt.ix == ix) return &ptsCnt;
        return nullptr;
    }

    LottiePosition position = Point{0.0f, 0.0f};
    LottieFloat innerRadius = 0.0f;
    LottieFloat outerRadius = 0.0f;
    LottieFloat innerRoundness = 0.0f;
    LottieFloat outerRoundness = 0.0f;
    LottieFloat rotation = 0.0f;
    LottieFloat ptsCnt = 0.0f;
    Type type = Polygon;
};


struct LottieEllipse : LottieShape
{
    LottieEllipse() : LottieShape(LottieObject::Ellipse) {}

    LottieProperty* property(uint16_t ix) override
    {
        if (position.ix == ix) return &position;
        if (size.ix == ix) return &size;
        return nullptr;
    }

    LottiePosition position = Point{0.0f, 0.0f};
    LottiePoint size = Point{0.0f, 0.0f};
};


struct LottieTransform : LottieObject
{
    struct SeparateCoord
    {
        LottieFloat x = 0.0f;
        LottieFloat y = 0.0f;
    };

    struct RotationEx
    {
        LottieFloat x = 0.0f;
        LottieFloat y = 0.0f;
    };

    ~LottieTransform()
    {
        delete(coords);
        delete(rotationEx);
    }

    LottieTransform()
    {
        LottieObject::type = LottieObject::Transform;
    }

    bool mergeable() override
    {
        if (!opacity.frames && opacity.value == 255) return true;
        return false;
    }

    LottieProperty* property(uint16_t ix) override
    {
        if (position.ix == ix) return &position;
        if (rotation.ix == ix) return &rotation;
        if (scale.ix == ix) return &scale;
        if (anchor.ix == ix) return &anchor;
        if (opacity.ix == ix) return &opacity;
        if (skewAngle.ix == ix) return &skewAngle;
        if (skewAxis.ix == ix) return &skewAxis;
        if (coords) {
            if (coords->x.ix == ix) return &coords->x;
            if (coords->y.ix == ix) return &coords->y;
        }
        return nullptr;
    }

    LottiePosition position = Point{0.0f, 0.0f};
    LottieFloat rotation = 0.0f;           //z rotation
    LottiePoint scale = Point{100.0f, 100.0f};
    LottiePoint anchor = Point{0.0f, 0.0f};
    LottieOpacity opacity = 255;
    LottieFloat skewAngle = 0.0f;
    LottieFloat skewAxis = 0.0f;

    SeparateCoord* coords = nullptr;       //either a position or separate coordinates
    RotationEx* rotationEx = nullptr;      //extension for 3d rotation
};


struct LottieSolid : LottieObject 
{
    LottieColor color = RGB24{255, 255, 255};
    LottieOpacity opacity = 255;

    LottieProperty* property(uint16_t ix) override
    {
        if (color.ix == ix) return &color;
        if (opacity.ix == ix) return &opacity;
        return nullptr;
    }
};


struct LottieSolidStroke : LottieSolid, LottieStroke
{
    LottieSolidStroke()
    {
        LottieObject::type = LottieObject::SolidStroke;
    }

    LottieProperty* property(uint16_t ix) override
    {
        if (width.ix == ix) return &width;
        if (dashattr) {
            if (dashattr->value[0].ix == ix) return &dashattr->value[0];
            if (dashattr->value[1].ix == ix) return &dashattr->value[1];
            if (dashattr->value[2].ix == ix) return &dashattr->value[2];
        }
        return LottieSolid::property(ix);
    }

    void override(LottieProperty* prop, bool shallow, bool byDefault) override
    {
        if (byDefault) color.release();
        color.copy(*static_cast<LottieColor*>(prop), shallow);
    }
};


struct LottieSolidFill : LottieSolid
{
    LottieSolidFill()
    {
        LottieObject::type = LottieObject::SolidFill;
    }

    void override(LottieProperty* prop, bool shallow, bool byDefault) override
    {
        if (prop->type == LottieProperty::Type::Opacity) {
            if (byDefault) opacity.release();
            opacity.copy(*static_cast<LottieOpacity*>(prop), shallow);
        } else if (prop->type == LottieProperty::Type::Color) {
            if (byDefault) color.release();
            color.copy(*static_cast<LottieColor*>(prop), shallow);
        }
    }

    FillRule rule = FillRule::Winding;
};


struct LottieGradient : LottieObject
{
    bool prepare()
    {
        if (!colorStops.populated) {
            auto count = colorStops.count;  //colorstop count can be modified after population
            if (colorStops.frames) {
                for (auto v = colorStops.frames->begin(); v < colorStops.frames->end(); ++v) {
                    colorStops.count = populate(v->value, count);
                }
            } else {
                colorStops.count = populate(colorStops.value, count);
            }
            colorStops.populated = true;
        }
        if (start.frames || end.frames || height.frames || angle.frames || opacity.frames || colorStops.frames) return true;
        return false;
    }

    LottieProperty* property(uint16_t ix) override
    {
        if (start.ix == ix) return &start;
        if (end.ix == ix) return &end;
        if (height.ix == ix) return &height;
        if (angle.ix == ix) return &angle;
        if (opacity.ix == ix) return &opacity;
        if (colorStops.ix == ix) return &colorStops;
        return nullptr;
    }

    void override(LottieProperty* prop, bool shallow, bool byDefault = false) override
    {
        if (byDefault) colorStops.release();
        colorStops.copy(*static_cast<LottieColorStop*>(prop), shallow);
        prepare();
    }

    uint32_t populate(ColorStop& color, size_t count);
    Fill* fill(float frameNo, LottieExpressions* exps);

    LottiePoint start = Point{0.0f, 0.0f};
    LottiePoint end = Point{0.0f, 0.0f};
    LottieFloat height = 0.0f;
    LottieFloat angle = 0.0f;
    LottieOpacity opacity = 255;
    LottieColorStop colorStops;
    uint8_t id = 0;    //1: linear, 2: radial
};


struct LottieGradientFill : LottieGradient
{
    LottieGradientFill()
    {
        LottieObject::type = LottieObject::GradientFill;
    }

    FillRule rule = FillRule::Winding;
};


struct LottieGradientStroke : LottieGradient, LottieStroke
{
    LottieGradientStroke()
    {
        LottieObject::type = LottieObject::GradientStroke;
    }

    LottieProperty* property(uint16_t ix) override
    {
        if (width.ix == ix) return &width;
        if (dashattr) {
            if (dashattr->value[0].ix == ix) return &dashattr->value[0];
            if (dashattr->value[1].ix == ix) return &dashattr->value[1];
            if (dashattr->value[2].ix == ix) return &dashattr->value[2];
        }
        return LottieGradient::property(ix);
    }
};


struct LottieImage : LottieObject, LottieRenderPooler<tvg::Picture>
{
    LottieBitmap data;

    void override(LottieProperty* prop, bool shallow, bool byDefault = false) override
    {
        if (byDefault) data.release();
        data.copy(*static_cast<LottieBitmap*>(prop), shallow);
        update();
    }

    void prepare();
    void update();
};


struct LottieRepeater : LottieObject
{
    LottieRepeater()
    {
        LottieObject::type = LottieObject::Repeater;
    }

    LottieProperty* property(uint16_t ix) override
    {
        if (copies.ix == ix) return &copies;
        if (offset.ix == ix) return &offset;
        if (position.ix == ix) return &position;
        if (rotation.ix == ix) return &rotation;
        if (scale.ix == ix) return &scale;
        if (anchor.ix == ix) return &anchor;
        if (startOpacity.ix == ix) return &startOpacity;
        if (endOpacity.ix == ix) return &endOpacity;
        return nullptr;
    }

    LottieFloat copies = 0.0f;
    LottieFloat offset = 0.0f;

    //Transform
    LottiePosition position = Point{0.0f, 0.0f};
    LottieFloat rotation = 0.0f;
    LottiePoint scale = Point{100.0f, 100.0f};
    LottiePoint anchor = Point{0.0f, 0.0f};
    LottieOpacity startOpacity = 255;
    LottieOpacity endOpacity = 255;
    bool inorder = true;        //true: higher,  false: lower
};


struct LottieOffsetPath : LottieObject
{
    LottieOffsetPath()
    {
        LottieObject::type = LottieObject::OffsetPath;
    }

    LottieFloat offset = 0.0f;
    LottieFloat miterLimit = 4.0f;
    StrokeJoin join = StrokeJoin::Miter;
};


struct LottieGroup : LottieObject, LottieRenderPooler<tvg::Shape>
{
    LottieGroup();

    virtual ~LottieGroup()
    {
        for (auto p = children.begin(); p < children.end(); ++p) delete(*p);
    }

    void prepare(LottieObject::Type type = LottieObject::Group);
    bool mergeable() override { return allowMerge; }

    LottieObject* content(unsigned long id)
    {
        if (this->id == id) return this;

        //source has children, find recursively.
        for (auto c = children.begin(); c < children.end(); ++c) {
            auto child = *c;
            if (child->type == LottieObject::Type::Group || child->type == LottieObject::Type::Layer) {
                if (auto ret = static_cast<LottieGroup*>(child)->content(id)) return ret;
            } else if (child->id == id) return child;
        }
        return nullptr;
    }

    Scene* scene = nullptr;
    Array<LottieObject*> children;

    bool reqFragment : 1;   //requirement to fragment the render context
    bool buildDone : 1;     //completed in building the composition.
    bool trimpath : 1;      //this group has a trimpath.
    bool visible : 1;       //this group has visible contents.
    bool allowMerge : 1;    //if this group is consisted of simple (transformed) shapes.
};


struct LottieLayer : LottieGroup
{
    enum Type : uint8_t {Precomp = 0, Solid, Image, Null, Shape, Text};

    ~LottieLayer();

    uint8_t opacity(float frameNo)
    {
        //return zero if the visibility is false.
        if (type == Null) return 255;
        return transform->opacity(frameNo);
    }

    bool mergeable() override { return false; }

    void prepare(RGB24* color = nullptr);
    float remap(LottieComposition* comp, float frameNo, LottieExpressions* exp);

    char* name = nullptr;
    LottieLayer* parent = nullptr;
    LottieFloat timeRemap = 0.0f;
    LottieLayer* comp = nullptr;  //Precompositor, current layer is belonges.
    LottieTransform* transform = nullptr;
    Array<LottieMask*> masks;
    Array<LottieEffect*> effects;
    LottieLayer* matteTarget = nullptr;

    LottieRenderPooler<tvg::Shape> statical;  //static pooler for solid fill and clipper

    float timeStretch = 1.0f;
    float w = 0.0f, h = 0.0f;
    float inFrame = 0.0f;
    float outFrame = 0.0f;
    float startFrame = 0.0f;
    unsigned long rid = 0;      //pre-composition reference id.
    int16_t mid = -1;           //id of the matte layer.
    int16_t pidx = -1;          //index of the parent layer.
    int16_t idx = -1;           //index of the current layer.

    struct {
        float frameNo = -1.0f;
        Matrix matrix;
        uint8_t opacity;
    } cache;

    CompositeMethod matteType = CompositeMethod::None;
    BlendMethod blendMethod = BlendMethod::Normal;
    Type type = Null;
    bool autoOrient = false;
    bool matteSrc = false;

    LottieLayer* layerById(unsigned long id)
    {
        for (auto child = children.begin(); child < children.end(); ++child) {
            if ((*child)->type != LottieObject::Type::Layer) continue;
            auto layer = static_cast<LottieLayer*>(*child);
            if (layer->id == id) return layer;
        }
        return nullptr;
    }

    LottieLayer* layerByIdx(int16_t idx)
    {
        for (auto child = children.begin(); child < children.end(); ++child) {
            if ((*child)->type != LottieObject::Type::Layer) continue;
            auto layer = static_cast<LottieLayer*>(*child);
            if (layer->idx == idx) return layer;
        }
        return nullptr;
    }
};


struct LottieSlot
{
    struct Pair {
        LottieObject* obj;
        LottieProperty* prop;
    };

    void assign(LottieObject* target, bool byDefault);
    void reset();

    LottieSlot(char* sid, LottieObject* obj, LottieProperty::Type type) : sid(sid), type(type)
    {
        pairs.push({obj});
    }

    ~LottieSlot()
    {
        free(sid);
        if (!overridden) return;
        for (auto pair = pairs.begin(); pair < pairs.end(); ++pair) {
            delete(pair->prop);
        }
    }

    char* sid;
    Array<Pair> pairs;
    LottieProperty::Type type;
    bool overridden = false;
};


struct LottieComposition
{
    ~LottieComposition();

    float duration() const
    {
        return frameCnt() / frameRate;  // in second
    }

    float frameAtTime(float timeInSec) const
    {
        auto p = timeInSec / duration();
        if (p < 0.0f) p = 0.0f;
        return p * frameCnt();
    }

    float timeAtFrame(float frameNo)
    {
        return (frameNo - root->inFrame) / frameRate;
    }

    float frameCnt() const
    {
        return root->outFrame - root->inFrame;
    }

    LottieLayer* asset(unsigned long id)
    {
        for (auto asset = assets.begin(); asset < assets.end(); ++asset) {
            auto layer = static_cast<LottieLayer*>(*asset);
            if (layer->id == id) return layer;
        }
        return nullptr;
    }

    LottieLayer* root = nullptr;
    char* version = nullptr;
    char* name = nullptr;
    float w, h;
    float frameRate;
    Array<LottieObject*> assets;
    Array<LottieInterpolator*> interpolators;
    Array<LottieFont*> fonts;
    Array<LottieSlot*> slots;
    Array<LottieMarker*> markers;
    bool expressions = false;
    bool initiated = false;
};

#endif //_TVG_LOTTIE_MODEL_H_

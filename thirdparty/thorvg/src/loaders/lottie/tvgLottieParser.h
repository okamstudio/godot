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

#ifndef _TVG_LOTTIE_PARSER_H_
#define _TVG_LOTTIE_PARSER_H_

#include "tvgCommon.h"
#include "tvgLottieParserHandler.h"
#include "tvgLottieProperty.h"

struct LottieParser : LookaheadParserHandler
{
public:
    LottieParser(const char *str, const char* dirName) : LookaheadParserHandler(str)
    {
        this->dirName = dirName;
    }

    bool parse();
    bool apply(LottieSlot* slot, bool byDefault);
    const char* sid(bool first = false);
    void captureSlots(const char* key);
    template<LottieProperty::Type type = LottieProperty::Type::Invalid> void registerSlot(LottieObject* obj, const char* sid);

    LottieComposition* comp = nullptr;
    const char* dirName = nullptr;       //base resource directory
    char* slots = nullptr;

private:
    RGB24 getColor(const char *str);
    CompositeMethod getMatteType();
    FillRule getFillRule();
    StrokeCap getStrokeCap();
    StrokeJoin getStrokeJoin();
    CompositeMethod getMaskMethod(bool inversed);
    LottieInterpolator* getInterpolator(const char* key, Point& in, Point& out);
    LottieEffect* getEffect(int type);

    void getInterpolatorPoint(Point& pt);
    void getPathSet(LottiePathSet& path);
    void getLayerSize(float& val);
    bool getValue(TextDocument& doc);
    bool getValue(PathSet& path);
    bool getValue(Array<Point>& pts);
    bool getValue(ColorStop& color);
    bool getValue(float& val);
    bool getValue(uint8_t& val);
    bool getValue(int8_t& val);
    bool getValue(RGB24& color);
    bool getValue(Point& pt);

    template<typename T> bool parseTangent(const char *key, LottieVectorFrame<T>& value);
    template<typename T> bool parseTangent(const char *key, LottieScalarFrame<T>& value);
    template<typename T> void parseKeyFrame(T& prop);
    template<typename T> void parsePropertyInternal(T& prop);
    template<LottieProperty::Type type = LottieProperty::Type::Invalid, typename T> void parseProperty(T& prop, LottieObject* obj = nullptr);
    template<LottieProperty::Type type = LottieProperty::Type::Invalid, typename T> void parseSlotProperty(T& prop);

    LottieObject* parseObject();
    LottieObject* parseAsset();
    void parseImage(LottieImage* image, const char* data, const char* subPath, bool embedded, float width, float height);
    LottieLayer* parseLayer(LottieLayer* precomp);
    LottieObject* parseGroup();
    LottieRect* parseRect();
    LottieEllipse* parseEllipse();
    LottieSolidFill* parseSolidFill();
    LottieTransform* parseTransform(bool ddd = false);
    LottieSolidStroke* parseSolidStroke();
    LottieGradientStroke* parseGradientStroke();
    LottiePath* parsePath();
    LottiePolyStar* parsePolyStar();
    LottieRoundedCorner* parseRoundedCorner();
    LottieGradientFill* parseGradientFill();
    LottieLayer* parseLayers(LottieLayer* root);
    LottieMask* parseMask();
    LottieTrimpath* parseTrimpath();
    LottieRepeater* parseRepeater();
    LottieOffsetPath* parseOffsetPath();
    LottieFont* parseFont();
    LottieMarker* parseMarker();

    void parseGaussianBlur(LottieGaussianBlur* effect);
    void parseDropShadow(LottieDropShadow* effect);

    bool parseDirection(LottieShape* shape, const char* key);
    bool parseCommon(LottieObject* obj, const char* key);
    void parseObject(Array<LottieObject*>& parent);
    void parseShapes(Array<LottieObject*>& parent);
    void parseText(Array<LottieObject*>& parent);
    void parseMasks(LottieLayer* layer);
    void parseEffects(LottieLayer* layer);
    void parseTimeRemap(LottieLayer* layer);
    void parseStrokeDash(LottieStroke* stroke);
    void parseGradient(LottieGradient* gradient, const char* key);
    void parseColorStop(LottieGradient* gradient);
    void parseTextRange(LottieText* text);
    void parseTextAlignmentOption(LottieText* text);
    void parseAssets();
    void parseFonts();
    void parseChars(Array<LottieGlyph*>& glyphs);
    void parseMarkers();
    void parseEffect(LottieEffect* effect);
    void postProcess(Array<LottieGlyph*>& glyphs);

    //Current parsing context
    struct Context {
        LottieLayer* layer = nullptr;
        LottieObject* parent = nullptr;
    } context;
};

#endif //_TVG_LOTTIE_PARSER_H_

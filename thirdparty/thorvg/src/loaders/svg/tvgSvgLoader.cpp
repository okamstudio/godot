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

/*
 * Copyright notice for the EFL:

 * Copyright (C) EFL developers (see AUTHORS)

 * All rights reserved.

 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:

 *   1. Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *   2. Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.

 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
 * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <cstring>
#include <fstream>
#include <float.h>
#include "tvgLoader.h"
#include "tvgXmlParser.h"
#include "tvgSvgLoader.h"
#include "tvgSvgSceneBuilder.h"
#include "tvgStr.h"
#include "tvgSvgCssStyle.h"
#include "tvgMath.h"

/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/

/*
 * According to: https://www.w3.org/TR/SVG2/coords.html#Units
 * and: https://www.w3.org/TR/css-values-4/#absolute-lengths
 */
#define PX_PER_IN 96        //1 in = 96 px
#define PX_PER_PC 16        //1 pc = 1/6 in  -> PX_PER_IN/6
#define PX_PER_PT 1.333333f //1 pt = 1/72 in -> PX_PER_IN/72
#define PX_PER_MM 3.779528f //1 in = 25.4 mm -> PX_PER_IN/25.4
#define PX_PER_CM 37.79528f //1 in = 2.54 cm -> PX_PER_IN/2.54

typedef bool (*parseAttributes)(const char* buf, unsigned bufLength, simpleXMLAttributeCb func, const void* data);
typedef SvgNode* (*FactoryMethod)(SvgLoaderData* loader, SvgNode* parent, const char* buf, unsigned bufLength, parseAttributes func);
typedef SvgStyleGradient* (*GradientFactoryMethod)(SvgLoaderData* loader, const char* buf, unsigned bufLength);

static char* _skipSpace(const char* str, const char* end)
{
    while (((end && str < end) || (!end && *str != '\0')) && isspace(*str)) {
        ++str;
    }
    return (char*) str;
}


static char* _copyId(const char* str)
{
    if (!str) return nullptr;
    if (strlen(str) == 0) return nullptr;

    return strdup(str);
}


static const char* _skipComma(const char* content)
{
    content = _skipSpace(content, nullptr);
    if (*content == ',') return content + 1;
    return content;
}


static bool _parseNumber(const char** content, const char** end, float* number)
{
    const char* _end = end ? *end : nullptr;

    *number = strToFloat(*content, (char**)&_end);
    //If the start of string is not number
    if ((*content) == _end) {
        if (end) *end = _end;
        return false;
    }
    //Skip comma if any
    *content = _skipComma(_end);
    if (end) *end = _end;

    return true;
}


static constexpr struct
{
    AspectRatioAlign align;
    const char* tag;
} alignTags[] = {
    { AspectRatioAlign::XMinYMin, "xMinYMin" },
    { AspectRatioAlign::XMidYMin, "xMidYMin" },
    { AspectRatioAlign::XMaxYMin, "xMaxYMin" },
    { AspectRatioAlign::XMinYMid, "xMinYMid" },
    { AspectRatioAlign::XMidYMid, "xMidYMid" },
    { AspectRatioAlign::XMaxYMid, "xMaxYMid" },
    { AspectRatioAlign::XMinYMax, "xMinYMax" },
    { AspectRatioAlign::XMidYMax, "xMidYMax" },
    { AspectRatioAlign::XMaxYMax, "xMaxYMax" },
};


static void _parseAspectRatio(const char** content, AspectRatioAlign* align, AspectRatioMeetOrSlice* meetOrSlice)
{
    if (!strcmp(*content, "none")) {
        *align = AspectRatioAlign::None;
        return;
    }

    for (unsigned int i = 0; i < sizeof(alignTags) / sizeof(alignTags[0]); i++) {
        if (!strncmp(*content, alignTags[i].tag, 8)) {
            *align = alignTags[i].align;
            *content += 8;
            *content = _skipSpace(*content, nullptr);
            break;
        }
    }

    if (!strcmp(*content, "meet")) {
        *meetOrSlice = AspectRatioMeetOrSlice::Meet;
    } else if (!strcmp(*content, "slice")) {
        *meetOrSlice = AspectRatioMeetOrSlice::Slice;
    }
}


/**
 * According to https://www.w3.org/TR/SVG/coords.html#Units
 */
static float _toFloat(const SvgParser* svgParse, const char* str, SvgParserLengthType type)
{
    float parsedValue = strToFloat(str, nullptr);

    if (strstr(str, "cm")) parsedValue *= PX_PER_CM;
    else if (strstr(str, "mm")) parsedValue *= PX_PER_MM;
    else if (strstr(str, "pt")) parsedValue *= PX_PER_PT;
    else if (strstr(str, "pc")) parsedValue *= PX_PER_PC;
    else if (strstr(str, "in")) parsedValue *= PX_PER_IN;
    else if (strstr(str, "%")) {
        if (type == SvgParserLengthType::Vertical) parsedValue = (parsedValue / 100.0f) * svgParse->global.h;
        else if (type == SvgParserLengthType::Horizontal) parsedValue = (parsedValue / 100.0f) * svgParse->global.w;
        else if (type == SvgParserLengthType::Diagonal) parsedValue = (sqrtf(powf(svgParse->global.w, 2) + powf(svgParse->global.h, 2)) / sqrtf(2.0f)) * (parsedValue / 100.0f);
        else //if other than it's radius
        {
            float max = svgParse->global.w;
            if (max < svgParse->global.h)
                max = svgParse->global.h;
            parsedValue = (parsedValue / 100.0f) * max;
        }
    }
    //TODO: Implement 'em', 'ex' attributes

    return parsedValue;
}


static float _gradientToFloat(const SvgParser* svgParse, const char* str, bool& isPercentage)
{
    char* end = nullptr;

    float parsedValue = strToFloat(str, &end);
    isPercentage = false;

    if (strstr(str, "%")) {
        parsedValue = parsedValue / 100.0;
        isPercentage = true;
    }
    else if (strstr(str, "cm")) parsedValue *= PX_PER_CM;
    else if (strstr(str, "mm")) parsedValue *= PX_PER_MM;
    else if (strstr(str, "pt")) parsedValue *= PX_PER_PT;
    else if (strstr(str, "pc")) parsedValue *= PX_PER_PC;
    else if (strstr(str, "in")) parsedValue *= PX_PER_IN;
    //TODO: Implement 'em', 'ex' attributes

    return parsedValue;
}


static float _toOffset(const char* str)
{
    char* end = nullptr;
    auto strEnd = str + strlen(str);

    float parsedValue = strToFloat(str, &end);

    end = _skipSpace(end, nullptr);
    auto ptr = strstr(str, "%");

    if (ptr) {
        parsedValue = parsedValue / 100.0;
        if (end != ptr || (end + 1) != strEnd) return 0;
    } else if (end != strEnd) return 0;

    return parsedValue;
}


static int _toOpacity(const char* str)
{
    char* end = nullptr;
    float opacity = strToFloat(str, &end);

    if (end) {
        if (end[0] == '%' && end[1] == '\0') return lrint(opacity * 2.55f);
        else if (*end == '\0') return lrint(opacity * 255);
    }
    return 255;
}


static SvgMaskType _toMaskType(const char* str)
{
    if (!strcmp(str, "Alpha")) return SvgMaskType::Alpha;

    return SvgMaskType::Luminance;
}


//The default rendering order: fill, stroke, markers
//If any is omitted, will be rendered in its default order after the specified ones.
static bool _toPaintOrder(const char* str)
{
    uint8_t position = 1;
    uint8_t strokePosition = 0;
    uint8_t fillPosition = 0;

    while (*str != '\0') {
        str = _skipSpace(str, nullptr);
        if (!strncmp(str, "fill", 4)) {
            fillPosition = position++;
            str += 4;
        } else if (!strncmp(str, "stroke", 6)) {
            strokePosition = position++;
            str += 6;
        } else if (!strncmp(str, "markers", 7)) {
            str += 7;
        } else {
            return _toPaintOrder("fill stroke");
        }
    }

    if (fillPosition == 0) fillPosition = position++;
    if (strokePosition == 0) strokePosition = position++;

    return fillPosition < strokePosition;
}


#define _PARSE_TAG(Type, Name, Name1, Tags_Array, Default)                        \
    static Type _to##Name1(const char* str)                                       \
    {                                                                             \
        unsigned int i;                                                           \
                                                                                  \
        for (i = 0; i < sizeof(Tags_Array) / sizeof(Tags_Array[0]); i++) {        \
            if (!strcmp(str, Tags_Array[i].tag)) return Tags_Array[i].Name;       \
        }                                                                         \
        return Default;                                                           \
    }


/* parse the line cap used during stroking a path.
 * Value:    butt | round | square | inherit
 * Initial:    butt
 * https://www.w3.org/TR/SVG/painting.html
 */
static constexpr struct
{
    StrokeCap lineCap;
    const char* tag;
} lineCapTags[] = {
    { StrokeCap::Butt, "butt" },
    { StrokeCap::Round, "round" },
    { StrokeCap::Square, "square" }
};


_PARSE_TAG(StrokeCap, lineCap, LineCap, lineCapTags, StrokeCap::Butt)


/* parse the line join used during stroking a path.
 * Value:   miter | round | bevel | inherit
 * Initial:    miter
 * https://www.w3.org/TR/SVG/painting.html
 */
static constexpr struct
{
    StrokeJoin lineJoin;
    const char* tag;
} lineJoinTags[] = {
    { StrokeJoin::Miter, "miter" },
    { StrokeJoin::Round, "round" },
    { StrokeJoin::Bevel, "bevel" }
};


_PARSE_TAG(StrokeJoin, lineJoin, LineJoin, lineJoinTags, StrokeJoin::Miter)


/* parse the fill rule used during filling a path.
 * Value:   nonzero | evenodd | inherit
 * Initial:    nonzero
 * https://www.w3.org/TR/SVG/painting.html
 */
static constexpr struct
{
    FillRule fillRule;
    const char* tag;
} fillRuleTags[] = {
    { FillRule::EvenOdd, "evenodd" }
};


_PARSE_TAG(FillRule, fillRule, FillRule, fillRuleTags, FillRule::Winding)


/* parse the dash pattern used during stroking a path.
 * Value:   none | <dasharray> | inherit
 * Initial:    none
 * https://www.w3.org/TR/SVG/painting.html
 */
static void _parseDashArray(SvgLoaderData* loader, const char *str, SvgDash* dash)
{
    if (!strncmp(str, "none", 4)) return;

    char *end = nullptr;

    while (*str) {
        str = _skipComma(str);
        float parsedValue = strToFloat(str, &end);
        if (str == end) break;
        if (parsedValue <= 0.0f) break;
        if (*end == '%') {
            ++end;
            //Refers to the diagonal length of the viewport.
            //https://www.w3.org/TR/SVG2/coords.html#Units
            parsedValue = (sqrtf(powf(loader->svgParse->global.w, 2) + powf(loader->svgParse->global.h, 2)) / sqrtf(2.0f)) * (parsedValue / 100.0f);
        }
        (*dash).array.push(parsedValue);
        str = end;
    }
    //If dash array size is 1, it means that dash and gap size are the same.
    if ((*dash).array.count == 1) (*dash).array.push((*dash).array[0]);
}


static char* _idFromUrl(const char* url)
{
    auto open = strchr(url, '(');
    auto close = strchr(url, ')');
    if (!open || !close || open >= close) return nullptr;

    open = strchr(url, '#');
    if (!open || open >= close) return nullptr;

    ++open;
    --close;

    //trim the rest of the spaces if any
    while (open < close && *close == ' ') --close;

    //quick verification
    for (auto id = open; id < close; id++) {
        if (*id == ' ' || *id == '\'') return nullptr;
    }

    return strDuplicate(open, (close - open + 1));
}


static unsigned char _parseColor(const char* value, char** end)
{
    float r;

    r = strToFloat(value, end);
    *end = _skipSpace(*end, nullptr);
    if (**end == '%') {
        r = 255 * r / 100;
        (*end)++;
    }
    *end = _skipSpace(*end, nullptr);

    if (r < 0 || r > 255) {
        *end = nullptr;
        return 0;
    }

    return lrint(r);
}


static constexpr struct
{
    const char* name;
    unsigned int value;
} colors[] = {
    { "aliceblue", 0xfff0f8ff },
    { "antiquewhite", 0xfffaebd7 },
    { "aqua", 0xff00ffff },
    { "aquamarine", 0xff7fffd4 },
    { "azure", 0xfff0ffff },
    { "beige", 0xfff5f5dc },
    { "bisque", 0xffffe4c4 },
    { "black", 0xff000000 },
    { "blanchedalmond", 0xffffebcd },
    { "blue", 0xff0000ff },
    { "blueviolet", 0xff8a2be2 },
    { "brown", 0xffa52a2a },
    { "burlywood", 0xffdeb887 },
    { "cadetblue", 0xff5f9ea0 },
    { "chartreuse", 0xff7fff00 },
    { "chocolate", 0xffd2691e },
    { "coral", 0xffff7f50 },
    { "cornflowerblue", 0xff6495ed },
    { "cornsilk", 0xfffff8dc },
    { "crimson", 0xffdc143c },
    { "cyan", 0xff00ffff },
    { "darkblue", 0xff00008b },
    { "darkcyan", 0xff008b8b },
    { "darkgoldenrod", 0xffb8860b },
    { "darkgray", 0xffa9a9a9 },
    { "darkgrey", 0xffa9a9a9 },
    { "darkgreen", 0xff006400 },
    { "darkkhaki", 0xffbdb76b },
    { "darkmagenta", 0xff8b008b },
    { "darkolivegreen", 0xff556b2f },
    { "darkorange", 0xffff8c00 },
    { "darkorchid", 0xff9932cc },
    { "darkred", 0xff8b0000 },
    { "darksalmon", 0xffe9967a },
    { "darkseagreen", 0xff8fbc8f },
    { "darkslateblue", 0xff483d8b },
    { "darkslategray", 0xff2f4f4f },
    { "darkslategrey", 0xff2f4f4f },
    { "darkturquoise", 0xff00ced1 },
    { "darkviolet", 0xff9400d3 },
    { "deeppink", 0xffff1493 },
    { "deepskyblue", 0xff00bfff },
    { "dimgray", 0xff696969 },
    { "dimgrey", 0xff696969 },
    { "dodgerblue", 0xff1e90ff },
    { "firebrick", 0xffb22222 },
    { "floralwhite", 0xfffffaf0 },
    { "forestgreen", 0xff228b22 },
    { "fuchsia", 0xffff00ff },
    { "gainsboro", 0xffdcdcdc },
    { "ghostwhite", 0xfff8f8ff },
    { "gold", 0xffffd700 },
    { "goldenrod", 0xffdaa520 },
    { "gray", 0xff808080 },
    { "grey", 0xff808080 },
    { "green", 0xff008000 },
    { "greenyellow", 0xffadff2f },
    { "honeydew", 0xfff0fff0 },
    { "hotpink", 0xffff69b4 },
    { "indianred", 0xffcd5c5c },
    { "indigo", 0xff4b0082 },
    { "ivory", 0xfffffff0 },
    { "khaki", 0xfff0e68c },
    { "lavender", 0xffe6e6fa },
    { "lavenderblush", 0xfffff0f5 },
    { "lawngreen", 0xff7cfc00 },
    { "lemonchiffon", 0xfffffacd },
    { "lightblue", 0xffadd8e6 },
    { "lightcoral", 0xfff08080 },
    { "lightcyan", 0xffe0ffff },
    { "lightgoldenrodyellow", 0xfffafad2 },
    { "lightgray", 0xffd3d3d3 },
    { "lightgrey", 0xffd3d3d3 },
    { "lightgreen", 0xff90ee90 },
    { "lightpink", 0xffffb6c1 },
    { "lightsalmon", 0xffffa07a },
    { "lightseagreen", 0xff20b2aa },
    { "lightskyblue", 0xff87cefa },
    { "lightslategray", 0xff778899 },
    { "lightslategrey", 0xff778899 },
    { "lightsteelblue", 0xffb0c4de },
    { "lightyellow", 0xffffffe0 },
    { "lime", 0xff00ff00 },
    { "limegreen", 0xff32cd32 },
    { "linen", 0xfffaf0e6 },
    { "magenta", 0xffff00ff },
    { "maroon", 0xff800000 },
    { "mediumaquamarine", 0xff66cdaa },
    { "mediumblue", 0xff0000cd },
    { "mediumorchid", 0xffba55d3 },
    { "mediumpurple", 0xff9370d8 },
    { "mediumseagreen", 0xff3cb371 },
    { "mediumslateblue", 0xff7b68ee },
    { "mediumspringgreen", 0xff00fa9a },
    { "mediumturquoise", 0xff48d1cc },
    { "mediumvioletred", 0xffc71585 },
    { "midnightblue", 0xff191970 },
    { "mintcream", 0xfff5fffa },
    { "mistyrose", 0xffffe4e1 },
    { "moccasin", 0xffffe4b5 },
    { "navajowhite", 0xffffdead },
    { "navy", 0xff000080 },
    { "oldlace", 0xfffdf5e6 },
    { "olive", 0xff808000 },
    { "olivedrab", 0xff6b8e23 },
    { "orange", 0xffffa500 },
    { "orangered", 0xffff4500 },
    { "orchid", 0xffda70d6 },
    { "palegoldenrod", 0xffeee8aa },
    { "palegreen", 0xff98fb98 },
    { "paleturquoise", 0xffafeeee },
    { "palevioletred", 0xffd87093 },
    { "papayawhip", 0xffffefd5 },
    { "peachpuff", 0xffffdab9 },
    { "peru", 0xffcd853f },
    { "pink", 0xffffc0cb },
    { "plum", 0xffdda0dd },
    { "powderblue", 0xffb0e0e6 },
    { "purple", 0xff800080 },
    { "red", 0xffff0000 },
    { "rosybrown", 0xffbc8f8f },
    { "royalblue", 0xff4169e1 },
    { "saddlebrown", 0xff8b4513 },
    { "salmon", 0xfffa8072 },
    { "sandybrown", 0xfff4a460 },
    { "seagreen", 0xff2e8b57 },
    { "seashell", 0xfffff5ee },
    { "sienna", 0xffa0522d },
    { "silver", 0xffc0c0c0 },
    { "skyblue", 0xff87ceeb },
    { "slateblue", 0xff6a5acd },
    { "slategray", 0xff708090 },
    { "slategrey", 0xff708090 },
    { "snow", 0xfffffafa },
    { "springgreen", 0xff00ff7f },
    { "steelblue", 0xff4682b4 },
    { "tan", 0xffd2b48c },
    { "teal", 0xff008080 },
    { "thistle", 0xffd8bfd8 },
    { "tomato", 0xffff6347 },
    { "turquoise", 0xff40e0d0 },
    { "violet", 0xffee82ee },
    { "wheat", 0xfff5deb3 },
    { "white", 0xffffffff },
    { "whitesmoke", 0xfff5f5f5 },
    { "yellow", 0xffffff00 },
    { "yellowgreen", 0xff9acd32 }
};


static bool _hslToRgb(float hue, float saturation, float brightness, uint8_t* red, uint8_t* green, uint8_t* blue)
{
    if (!red || !green || !blue) return false;

    float sv, vsf, f, p, q, t, v;
    float _red = 0, _green = 0, _blue = 0;
    uint32_t i = 0;

    while (hue < 0) hue += 360.0f;
    hue = fmod(hue, 360.0f);
    saturation = saturation > 0 ? std::min(saturation, 1.0f) : 0.0f;
    brightness = brightness > 0 ? std::min(brightness, 1.0f) : 0.0f;

    if (tvg::zero(saturation))  _red = _green = _blue = brightness;
    else {
        if (tvg::equal(hue, 360.0)) hue = 0.0f;
        hue /= 60.0f;

        v = (brightness <= 0.5f) ? (brightness * (1.0f + saturation)) : (brightness + saturation - (brightness * saturation));
        p = brightness + brightness - v;

        if (!tvg::zero(v)) sv = (v - p) / v;
        else sv = 0;

        i = static_cast<uint8_t>(hue);
        f = hue - i;

        vsf = v * sv * f;

        t = p + vsf;
        q = v - vsf;

        switch (i) {
            case 0: {
                _red = v;
                _green = t;
                _blue = p;
                break;
            }
            case 1: {
                _red = q;
                _green = v;
                _blue = p;
                break;
            }
            case 2: {
                _red = p;
                _green = v;
                _blue = t;
                break;
            }
            case 3: {
                _red = p;
                _green = q;
                _blue = v;
                break;
            }
            case 4: {
                _red = t;
                _green = p;
                _blue = v;
                break;
            }
            case 5: {
                _red = v;
                _green = p;
                _blue = q;
                break;
            }
        }
    }

    *red = (uint8_t)nearbyint(_red * 255.0f);
    *green = (uint8_t)nearbyint(_green * 255.0f);
    *blue = (uint8_t)nearbyint(_blue * 255.0f);

    return true;
}


static bool _toColor(const char* str, uint8_t* r, uint8_t* g, uint8_t* b, char** ref)
{
    unsigned int len = strlen(str);
    char *red, *green, *blue;
    unsigned char tr, tg, tb;

    if (len == 4 && str[0] == '#') {
        //Case for "#456" should be interpreted as "#445566"
        if (isxdigit(str[1]) && isxdigit(str[2]) && isxdigit(str[3])) {
            char tmp[3] = { '\0', '\0', '\0' };
            tmp[0] = str[1];
            tmp[1] = str[1];
            *r = strtol(tmp, nullptr, 16);
            tmp[0] = str[2];
            tmp[1] = str[2];
            *g = strtol(tmp, nullptr, 16);
            tmp[0] = str[3];
            tmp[1] = str[3];
            *b = strtol(tmp, nullptr, 16);
        }
        return true;
    } else if (len == 7 && str[0] == '#') {
        if (isxdigit(str[1]) && isxdigit(str[2]) && isxdigit(str[3]) && isxdigit(str[4]) && isxdigit(str[5]) && isxdigit(str[6])) {
            char tmp[3] = { '\0', '\0', '\0' };
            tmp[0] = str[1];
            tmp[1] = str[2];
            *r = strtol(tmp, nullptr, 16);
            tmp[0] = str[3];
            tmp[1] = str[4];
            *g = strtol(tmp, nullptr, 16);
            tmp[0] = str[5];
            tmp[1] = str[6];
            *b = strtol(tmp, nullptr, 16);
        }
        return true;
    } else if (len >= 10 && (str[0] == 'r' || str[0] == 'R') && (str[1] == 'g' || str[1] == 'G') && (str[2] == 'b' || str[2] == 'B') && str[3] == '(' && str[len - 1] == ')') {
        tr = _parseColor(str + 4, &red);
        if (red && *red == ',') {
            tg = _parseColor(red + 1, &green);
            if (green && *green == ',') {
                tb = _parseColor(green + 1, &blue);
                if (blue && blue[0] == ')' && blue[1] == '\0') {
                    *r = tr;
                    *g = tg;
                    *b = tb;
                }
            }
        }
        return true;
    } else if (ref && len >= 3 && !strncmp(str, "url", 3)) {
        if (*ref) free(*ref);
        *ref = _idFromUrl((const char*)(str + 3));
        return true;
    } else if (len >= 10 && (str[0] == 'h' || str[0] == 'H') && (str[1] == 's' || str[1] == 'S') && (str[2] == 'l' || str[2] == 'L') && str[3] == '(' && str[len - 1] == ')') {
        float th, ts, tb;
        const char* content = _skipSpace(str + 4, nullptr);
        const char* hue = nullptr;
        if (_parseNumber(&content, &hue, &th) && hue) {
            const char* saturation = nullptr;
            hue = _skipSpace(hue, nullptr);
            hue = (char*)_skipComma(hue);
            hue = _skipSpace(hue, nullptr);
            if (_parseNumber(&hue, &saturation, &ts) && saturation && *saturation == '%') {
                const char* brightness = nullptr;
                ts /= 100.0f;
                saturation = _skipSpace(saturation + 1, nullptr);
                saturation = (char*)_skipComma(saturation);
                saturation = _skipSpace(saturation, nullptr);
                if (_parseNumber(&saturation, &brightness, &tb) && brightness && *brightness == '%') {
                    tb /= 100.0f;
                    brightness = _skipSpace(brightness + 1, nullptr);
                    if (brightness && brightness[0] == ')' && brightness[1] == '\0') {
                       return _hslToRgb(th, ts, tb, r, g, b);
                    }
                }
            }
        }
    } else {
        //Handle named color
        for (unsigned int i = 0; i < (sizeof(colors) / sizeof(colors[0])); i++) {
            if (!strcasecmp(colors[i].name, str)) {
                *r = (((uint8_t*)(&(colors[i].value)))[2]);
                *g = (((uint8_t*)(&(colors[i].value)))[1]);
                *b = (((uint8_t*)(&(colors[i].value)))[0]);
                return true;
            }
        }
    }
    return false;
}


static char* _parseNumbersArray(char* str, float* points, int* ptCount, int len)
{
    int count = 0;
    char* end = nullptr;

    str = _skipSpace(str, nullptr);
    while ((count < len) && (isdigit(*str) || *str == '-' || *str == '+' || *str == '.')) {
        points[count++] = strToFloat(str, &end);
        str = end;
        str = _skipSpace(str, nullptr);
        if (*str == ',') ++str;
        //Eat the rest of space
        str = _skipSpace(str, nullptr);
    }
    *ptCount = count;
    return str;
}


enum class MatrixState {
    Unknown,
    Matrix,
    Translate,
    Rotate,
    Scale,
    SkewX,
    SkewY
};


#define MATRIX_DEF(Name, Value)     \
    {                               \
#Name, sizeof(#Name), Value \
    }


static constexpr struct
{
    const char* tag;
    int sz;
    MatrixState state;
} matrixTags[] = {
    MATRIX_DEF(matrix, MatrixState::Matrix),
    MATRIX_DEF(translate, MatrixState::Translate),
    MATRIX_DEF(rotate, MatrixState::Rotate),
    MATRIX_DEF(scale, MatrixState::Scale),
    MATRIX_DEF(skewX, MatrixState::SkewX),
    MATRIX_DEF(skewY, MatrixState::SkewY)
};


/* parse transform attribute
 * https://www.w3.org/TR/SVG/coords.html#TransformAttribute
 */
static Matrix* _parseTransformationMatrix(const char* value)
{
    const int POINT_CNT = 8;

    auto matrix = (Matrix*)malloc(sizeof(Matrix));
    if (!matrix) return nullptr;
    *matrix = {1, 0, 0, 0, 1, 0, 0, 0, 1};

    float points[POINT_CNT];
    int ptCount = 0;
    char* str = (char*)value;
    char* end = str + strlen(str);

    while (str < end) {
        auto state = MatrixState::Unknown;

        if (isspace(*str) || (*str == ',')) {
            ++str;
            continue;
        }
        for (unsigned int i = 0; i < sizeof(matrixTags) / sizeof(matrixTags[0]); i++) {
            if (!strncmp(matrixTags[i].tag, str, matrixTags[i].sz - 1)) {
                state = matrixTags[i].state;
                str += (matrixTags[i].sz - 1);
                break;
            }
        }
        if (state == MatrixState::Unknown) goto error;

        str = _skipSpace(str, end);
        if (*str != '(') goto error;
        ++str;
        str = _parseNumbersArray(str, points, &ptCount, POINT_CNT);
        if (*str != ')') goto error;
        ++str;

        if (state == MatrixState::Matrix) {
            if (ptCount != 6) goto error;
            Matrix tmp = {points[0], points[2], points[4], points[1], points[3], points[5], 0, 0, 1};
            *matrix *= tmp;
        } else if (state == MatrixState::Translate) {
            if (ptCount == 1) {
                Matrix tmp = {1, 0, points[0], 0, 1, 0, 0, 0, 1};
                *matrix *= tmp;
            } else if (ptCount == 2) {
                Matrix tmp = {1, 0, points[0], 0, 1, points[1], 0, 0, 1};
                *matrix *= tmp;
            } else goto error;
        } else if (state == MatrixState::Rotate) {
            //Transform to signed.
            points[0] = fmodf(points[0], 360.0f);
            if (points[0] < 0) points[0] += 360.0f;
            auto c = cosf(deg2rad(points[0]));
            auto s = sinf(deg2rad(points[0]));
            if (ptCount == 1) {
                Matrix tmp = { c, -s, 0, s, c, 0, 0, 0, 1 };
                *matrix *= tmp;
            } else if (ptCount == 3) {
                Matrix tmp = { 1, 0, points[1], 0, 1, points[2], 0, 0, 1 };
                *matrix *= tmp;
                tmp = { c, -s, 0, s, c, 0, 0, 0, 1 };
                *matrix *= tmp;
                tmp = { 1, 0, -points[1], 0, 1, -points[2], 0, 0, 1 };
                *matrix *= tmp;
            } else {
                goto error;
            }
        } else if (state == MatrixState::Scale) {
            if (ptCount < 1 || ptCount > 2) goto error;
            auto sx = points[0];
            auto sy = sx;
            if (ptCount == 2) sy = points[1];
            Matrix tmp = { sx, 0, 0, 0, sy, 0, 0, 0, 1 };
            *matrix *= tmp;
        } else if (state == MatrixState::SkewX) {
            if (ptCount != 1) goto error;
            auto deg = tanf(deg2rad(points[0]));
            Matrix tmp = { 1, deg, 0, 0, 1, 0, 0, 0, 1 };
            *matrix *= tmp;
        } else if (state == MatrixState::SkewY) {
            if (ptCount != 1) goto error;
            auto deg = tanf(deg2rad(points[0]));
            Matrix tmp = { 1, 0, 0, deg, 1, 0, 0, 0, 1 };
            *matrix *= tmp;
        }
    }
    return matrix;
error:
    if (matrix) free(matrix);
    return nullptr;
}


#define LENGTH_DEF(Name, Value)     \
    {                               \
#Name, sizeof(#Name), Value \
    }


static void _postpone(Array<SvgNodeIdPair>& nodes, SvgNode *node, char* id)
{
    nodes.push({node, id});
}


/*
// TODO - remove?
static constexpr struct
{
    const char* tag;
    int sz;
    SvgLengthType type;
} lengthTags[] = {
    LENGTH_DEF(%, SvgLengthType::Percent),
    LENGTH_DEF(px, SvgLengthType::Px),
    LENGTH_DEF(pc, SvgLengthType::Pc),
    LENGTH_DEF(pt, SvgLengthType::Pt),
    LENGTH_DEF(mm, SvgLengthType::Mm),
    LENGTH_DEF(cm, SvgLengthType::Cm),
    LENGTH_DEF(in, SvgLengthType::In)
};

static float _parseLength(const char* str, SvgLengthType* type)
{
    float value;
    int sz = strlen(str);

    *type = SvgLengthType::Px;
    for (unsigned int i = 0; i < sizeof(lengthTags) / sizeof(lengthTags[0]); i++) {
        if (lengthTags[i].sz - 1 == sz && !strncmp(lengthTags[i].tag, str, sz)) *type = lengthTags[i].type;
    }
    value = svgUtilStrtof(str, nullptr);
    return value;
}
*/

static bool _parseStyleAttr(void* data, const char* key, const char* value);
static bool _parseStyleAttr(void* data, const char* key, const char* value, bool style);


static bool _attrParseSvgNode(void* data, const char* key, const char* value)
{
    SvgLoaderData* loader = (SvgLoaderData*)data;
    SvgNode* node = loader->svgParse->node;
    SvgDocNode* doc = &(node->node.doc);

    if (!strcmp(key, "width")) {
        doc->w = _toFloat(loader->svgParse, value, SvgParserLengthType::Horizontal);
        if (strstr(value, "%") && !(doc->viewFlag & SvgViewFlag::Viewbox)) {
            doc->viewFlag = (doc->viewFlag | SvgViewFlag::WidthInPercent);
        } else {
            doc->viewFlag = (doc->viewFlag | SvgViewFlag::Width);
        }
    } else if (!strcmp(key, "height")) {
        doc->h = _toFloat(loader->svgParse, value, SvgParserLengthType::Vertical);
        if (strstr(value, "%") && !(doc->viewFlag & SvgViewFlag::Viewbox)) {
            doc->viewFlag = (doc->viewFlag | SvgViewFlag::HeightInPercent);
        } else {
            doc->viewFlag = (doc->viewFlag | SvgViewFlag::Height);
        }
    } else if (!strcmp(key, "viewBox")) {
        if (_parseNumber(&value, nullptr, &doc->vx)) {
            if (_parseNumber(&value, nullptr, &doc->vy)) {
                if (_parseNumber(&value, nullptr, &doc->vw)) {
                    if (_parseNumber(&value, nullptr, &doc->vh)) {
                        doc->viewFlag = (doc->viewFlag | SvgViewFlag::Viewbox);
                        loader->svgParse->global.h = doc->vh;
                    }
                    loader->svgParse->global.w = doc->vw;
                }
                loader->svgParse->global.y = doc->vy;
            }
            loader->svgParse->global.x = doc->vx;
        }
        if ((doc->viewFlag & SvgViewFlag::Viewbox) && (doc->vw < 0.0f || doc->vh < 0.0f)) {
            doc->viewFlag = (SvgViewFlag)((uint32_t)doc->viewFlag & ~(uint32_t)SvgViewFlag::Viewbox);
            TVGLOG("SVG", "Negative values of the <viewBox> width and/or height - the attribute invalidated.");
        }
        if (!(doc->viewFlag & SvgViewFlag::Viewbox)) {
            loader->svgParse->global.x = loader->svgParse->global.y = 0.0f;
            loader->svgParse->global.w = loader->svgParse->global.h = 1.0f;
        }
    } else if (!strcmp(key, "preserveAspectRatio")) {
        _parseAspectRatio(&value, &doc->align, &doc->meetOrSlice);
    } else if (!strcmp(key, "style")) {
        return simpleXmlParseW3CAttribute(value, strlen(value), _parseStyleAttr, loader);
#ifdef THORVG_LOG_ENABLED
    } else if ((!strcmp(key, "x") || !strcmp(key, "y")) && fabsf(strToFloat(value, nullptr)) > FLOAT_EPSILON) {
        TVGLOG("SVG", "Unsupported attributes used [Elements type: Svg][Attribute: %s][Value: %s]", key, value);
#endif
    } else {
        return _parseStyleAttr(loader, key, value, false);
    }
    return true;
}


//https://www.w3.org/TR/SVGTiny12/painting.html#SpecifyingPaint
static void _handlePaintAttr(SvgPaint* paint, const char* value)
{
    if (!strcmp(value, "none")) {
        //No paint property
        paint->none = true;
        return;
    }
    if (!strcmp(value, "currentColor")) {
        paint->curColor = true;
        paint->none = false;
        return;
    }
    if (_toColor(value, &paint->color.r, &paint->color.g, &paint->color.b, &paint->url)) paint->none = false;
}


static void _handleColorAttr(TVG_UNUSED SvgLoaderData* loader, SvgNode* node, const char* value)
{
    SvgStyleProperty* style = node->style;
    if (_toColor(value, &style->color.r, &style->color.g, &style->color.b, nullptr)) {
        style->curColorSet = true;
    }
}


static void _handleFillAttr(TVG_UNUSED SvgLoaderData* loader, SvgNode* node, const char* value)
{
    SvgStyleProperty* style = node->style;
    style->fill.flags = (style->fill.flags | SvgFillFlags::Paint);
    _handlePaintAttr(&style->fill.paint, value);
}


static void _handleStrokeAttr(TVG_UNUSED SvgLoaderData* loader, SvgNode* node, const char* value)
{
    SvgStyleProperty* style = node->style;
    style->stroke.flags = (style->stroke.flags | SvgStrokeFlags::Paint);
    _handlePaintAttr(&style->stroke.paint, value);
}


static void _handleStrokeOpacityAttr(TVG_UNUSED SvgLoaderData* loader, SvgNode* node, const char* value)
{
    node->style->stroke.flags = (node->style->stroke.flags | SvgStrokeFlags::Opacity);
    node->style->stroke.opacity = _toOpacity(value);
}

static void _handleStrokeDashArrayAttr(SvgLoaderData* loader, SvgNode* node, const char* value)
{
    node->style->stroke.flags = (node->style->stroke.flags | SvgStrokeFlags::Dash);
    _parseDashArray(loader, value, &node->style->stroke.dash);
}

static void _handleStrokeDashOffsetAttr(SvgLoaderData* loader, SvgNode* node, const char* value)
{
    node->style->stroke.flags = (node->style->stroke.flags | SvgStrokeFlags::DashOffset);
    node->style->stroke.dash.offset = _toFloat(loader->svgParse, value, SvgParserLengthType::Horizontal);
}

static void _handleStrokeWidthAttr(SvgLoaderData* loader, SvgNode* node, const char* value)
{
    node->style->stroke.flags = (node->style->stroke.flags | SvgStrokeFlags::Width);
    node->style->stroke.width = _toFloat(loader->svgParse, value, SvgParserLengthType::Diagonal);
}


static void _handleStrokeLineCapAttr(TVG_UNUSED SvgLoaderData* loader, SvgNode* node, const char* value)
{
    node->style->stroke.flags = (node->style->stroke.flags | SvgStrokeFlags::Cap);
    node->style->stroke.cap = _toLineCap(value);
}


static void _handleStrokeLineJoinAttr(TVG_UNUSED SvgLoaderData* loader, SvgNode* node, const char* value)
{
    node->style->stroke.flags = (node->style->stroke.flags | SvgStrokeFlags::Join);
    node->style->stroke.join = _toLineJoin(value);
}

static void _handleStrokeMiterlimitAttr(SvgLoaderData* loader, SvgNode* node, const char* value)
{
    char* end = nullptr;
    const float miterlimit = strToFloat(value, &end);

    // https://www.w3.org/TR/SVG2/painting.html#LineJoin
    // - A negative value for stroke-miterlimit must be treated as an illegal value.
    if (miterlimit < 0.0f) {
        TVGERR("SVG", "A stroke-miterlimit change (%f <- %f) with a negative value is omitted.",
            node->style->stroke.miterlimit, miterlimit);
        return;
    }

    node->style->stroke.flags = (node->style->stroke.flags | SvgStrokeFlags::Miterlimit);
    node->style->stroke.miterlimit = miterlimit;
}

static void _handleFillRuleAttr(TVG_UNUSED SvgLoaderData* loader, SvgNode* node, const char* value)
{
    node->style->fill.flags = (node->style->fill.flags | SvgFillFlags::FillRule);
    node->style->fill.fillRule = _toFillRule(value);
}


static void _handleOpacityAttr(TVG_UNUSED SvgLoaderData* loader, SvgNode* node, const char* value)
{
    node->style->flags = (node->style->flags | SvgStyleFlags::Opacity);
    node->style->opacity = _toOpacity(value);
}


static void _handleFillOpacityAttr(TVG_UNUSED SvgLoaderData* loader, SvgNode* node, const char* value)
{
    node->style->fill.flags = (node->style->fill.flags | SvgFillFlags::Opacity);
    node->style->fill.opacity = _toOpacity(value);
}


static void _handleTransformAttr(TVG_UNUSED SvgLoaderData* loader, SvgNode* node, const char* value)
{
    node->transform = _parseTransformationMatrix(value);
}


static void _handleClipPathAttr(TVG_UNUSED SvgLoaderData* loader, SvgNode* node, const char* value)
{
    SvgStyleProperty* style = node->style;
    int len = strlen(value);
    if (len >= 3 && !strncmp(value, "url", 3)) {
        if (style->clipPath.url) free(style->clipPath.url);
        style->clipPath.url = _idFromUrl((const char*)(value + 3));
    }
}


static void _handleMaskAttr(TVG_UNUSED SvgLoaderData* loader, SvgNode* node, const char* value)
{
    SvgStyleProperty* style = node->style;
    int len = strlen(value);
    if (len >= 3 && !strncmp(value, "url", 3)) {
        if (style->mask.url) free(style->mask.url);
        style->mask.url = _idFromUrl((const char*)(value + 3));
    }
}


static void _handleMaskTypeAttr(TVG_UNUSED SvgLoaderData* loader, SvgNode* node, const char* value)
{
    node->node.mask.type = _toMaskType(value);
}


static void _handleDisplayAttr(TVG_UNUSED SvgLoaderData* loader, SvgNode* node, const char* value)
{
    //TODO : The display attribute can have various values as well as "none".
    //       The default is "inline" which means visible and "none" means invisible.
    //       Depending on the type of node, additional functionality may be required.
    //       refer to https://developer.mozilla.org/en-US/docs/Web/SVG/Attribute/display
    node->style->flags = (node->style->flags | SvgStyleFlags::Display);
    if (!strcmp(value, "none")) node->style->display = false;
    else node->style->display = true;
}


static void _handlePaintOrderAttr(TVG_UNUSED SvgLoaderData* loader, SvgNode* node, const char* value)
{
    node->style->flags = (node->style->flags | SvgStyleFlags::PaintOrder);
    node->style->paintOrder = _toPaintOrder(value);
}


static void _handleCssClassAttr(SvgLoaderData* loader, SvgNode* node, const char* value)
{
    auto cssClass = &node->style->cssClass;

    if (*cssClass && value) free(*cssClass);
    *cssClass = _copyId(value);

    bool cssClassFound = false;

    //css styling: tag.name has higher priority than .name
    if (auto cssNode = cssFindStyleNode(loader->cssStyle, *cssClass, node->type)) {
        cssClassFound = true;
        cssCopyStyleAttr(node, cssNode);
    }
    if (auto cssNode = cssFindStyleNode(loader->cssStyle, *cssClass)) {
        cssClassFound = true;
        cssCopyStyleAttr(node, cssNode);
    }

    if (!cssClassFound) _postpone(loader->nodesToStyle, node, *cssClass);
}


typedef void (*styleMethod)(SvgLoaderData* loader, SvgNode* node, const char* value);

#define STYLE_DEF(Name, Name1, Flag) { #Name, sizeof(#Name), _handle##Name1##Attr, Flag }


static constexpr struct
{
    const char* tag;
    int sz;
    styleMethod tagHandler;
    SvgStyleFlags flag;
} styleTags[] = {
    STYLE_DEF(color, Color, SvgStyleFlags::Color),
    STYLE_DEF(fill, Fill, SvgStyleFlags::Fill),
    STYLE_DEF(fill-rule, FillRule, SvgStyleFlags::FillRule),
    STYLE_DEF(fill-opacity, FillOpacity, SvgStyleFlags::FillOpacity),
    STYLE_DEF(opacity, Opacity, SvgStyleFlags::Opacity),
    STYLE_DEF(stroke, Stroke, SvgStyleFlags::Stroke),
    STYLE_DEF(stroke-width, StrokeWidth, SvgStyleFlags::StrokeWidth),
    STYLE_DEF(stroke-linejoin, StrokeLineJoin, SvgStyleFlags::StrokeLineJoin),
    STYLE_DEF(stroke-miterlimit, StrokeMiterlimit, SvgStyleFlags::StrokeMiterlimit),
    STYLE_DEF(stroke-linecap, StrokeLineCap, SvgStyleFlags::StrokeLineCap),
    STYLE_DEF(stroke-opacity, StrokeOpacity, SvgStyleFlags::StrokeOpacity),
    STYLE_DEF(stroke-dasharray, StrokeDashArray, SvgStyleFlags::StrokeDashArray),
    STYLE_DEF(stroke-dashoffset, StrokeDashOffset, SvgStyleFlags::StrokeDashOffset),
    STYLE_DEF(transform, Transform, SvgStyleFlags::Transform),
    STYLE_DEF(clip-path, ClipPath, SvgStyleFlags::ClipPath),
    STYLE_DEF(mask, Mask, SvgStyleFlags::Mask),
    STYLE_DEF(mask-type, MaskType, SvgStyleFlags::MaskType),
    STYLE_DEF(display, Display, SvgStyleFlags::Display),
    STYLE_DEF(paint-order, PaintOrder, SvgStyleFlags::PaintOrder)
};


static bool _parseStyleAttr(void* data, const char* key, const char* value, bool style)
{
    SvgLoaderData* loader = (SvgLoaderData*)data;
    SvgNode* node = loader->svgParse->node;
    int sz;
    if (!key || !value) return false;

    //Trim the white space
    key = _skipSpace(key, nullptr);
    value = _skipSpace(value, nullptr);

    sz = strlen(key);
    for (unsigned int i = 0; i < sizeof(styleTags) / sizeof(styleTags[0]); i++) {
        if (styleTags[i].sz - 1 == sz && !strncmp(styleTags[i].tag, key, sz)) {
            bool importance = false;
            if (auto ptr = strstr(value, "!important")) {
                size_t size = ptr - value;
                while (size > 0 && isspace(value[size - 1])) {
                    size--;
                }
                value = strDuplicate(value, size);
                importance = true;
            }
            if (style) {
                if (importance || !(node->style->flagsImportance & styleTags[i].flag)) {
                    styleTags[i].tagHandler(loader, node, value);
                    node->style->flags = (node->style->flags | styleTags[i].flag);
                }
            } else if (!(node->style->flags & styleTags[i].flag)) {
                styleTags[i].tagHandler(loader, node, value);
            }
            if (importance) {
                node->style->flagsImportance = (node->style->flags | styleTags[i].flag);
                free(const_cast<char*>(value));
            }
            return true;
        }
    }

    return false;
}


static bool _parseStyleAttr(void* data, const char* key, const char* value)
{
    return _parseStyleAttr(data, key, value, true);
}


/* parse g node
 * https://www.w3.org/TR/SVG/struct.html#Groups
 */
static bool _attrParseGNode(void* data, const char* key, const char* value)
{
    SvgLoaderData* loader = (SvgLoaderData*)data;
    SvgNode* node = loader->svgParse->node;

    if (!strcmp(key, "style")) {
        return simpleXmlParseW3CAttribute(value, strlen(value), _parseStyleAttr, loader);
    } else if (!strcmp(key, "transform")) {
        node->transform = _parseTransformationMatrix(value);
    } else if (!strcmp(key, "id")) {
        if (node->id && value) free(node->id);
        node->id = _copyId(value);
    } else if (!strcmp(key, "class")) {
        _handleCssClassAttr(loader, node, value);
    } else if (!strcmp(key, "clip-path")) {
        _handleClipPathAttr(loader, node, value);
    } else if (!strcmp(key, "mask")) {
        _handleMaskAttr(loader, node, value);
    } else {
        return _parseStyleAttr(loader, key, value, false);
    }
    return true;
}


/* parse clipPath node
 * https://www.w3.org/TR/SVG/struct.html#Groups
 */
static bool _attrParseClipPathNode(void* data, const char* key, const char* value)
{
    SvgLoaderData* loader = (SvgLoaderData*)data;
    SvgNode* node = loader->svgParse->node;
    SvgClipNode* clip = &(node->node.clip);

    if (!strcmp(key, "style")) {
        return simpleXmlParseW3CAttribute(value, strlen(value), _parseStyleAttr, loader);
    } else if (!strcmp(key, "transform")) {
        node->transform = _parseTransformationMatrix(value);
    } else if (!strcmp(key, "id")) {
        if (node->id && value) free(node->id);
        node->id = _copyId(value);
    } else if (!strcmp(key, "class")) {
        _handleCssClassAttr(loader, node, value);
    } else if (!strcmp(key, "clipPathUnits")) {
        if (!strcmp(value, "objectBoundingBox")) clip->userSpace = false;
    } else {
        return _parseStyleAttr(loader, key, value, false);
    }
    return true;
}


static bool _attrParseMaskNode(void* data, const char* key, const char* value)
{
    SvgLoaderData* loader = (SvgLoaderData*)data;
    SvgNode* node = loader->svgParse->node;
    SvgMaskNode* mask = &(node->node.mask);

    if (!strcmp(key, "style")) {
        return simpleXmlParseW3CAttribute(value, strlen(value), _parseStyleAttr, loader);
    } else if (!strcmp(key, "transform")) {
        node->transform = _parseTransformationMatrix(value);
    } else if (!strcmp(key, "id")) {
        if (node->id && value) free(node->id);
        node->id = _copyId(value);
    } else if (!strcmp(key, "class")) {
        _handleCssClassAttr(loader, node, value);
    } else if (!strcmp(key, "maskContentUnits")) {
        if (!strcmp(value, "objectBoundingBox")) mask->userSpace = false;
    } else if (!strcmp(key, "mask-type")) {
        mask->type = _toMaskType(value);
    } else {
        return _parseStyleAttr(loader, key, value, false);
    }
    return true;
}


static bool _attrParseCssStyleNode(void* data, const char* key, const char* value)
{
    SvgLoaderData* loader = (SvgLoaderData*)data;
    SvgNode* node = loader->svgParse->node;

    if (!strcmp(key, "id")) {
        if (node->id && value) free(node->id);
        node->id = _copyId(value);
    } else {
        return _parseStyleAttr(loader, key, value, false);
    }
    return true;
}


static bool _attrParseSymbolNode(void* data, const char* key, const char* value)
{
    SvgLoaderData* loader = (SvgLoaderData*)data;
    SvgNode* node = loader->svgParse->node;
    SvgSymbolNode* symbol = &(node->node.symbol);

    if (!strcmp(key, "viewBox")) {
        if (!_parseNumber(&value, nullptr, &symbol->vx) || !_parseNumber(&value, nullptr, &symbol->vy)) return false;
        if (!_parseNumber(&value, nullptr, &symbol->vw) || !_parseNumber(&value, nullptr, &symbol->vh)) return false;
        symbol->hasViewBox = true;
    } else if (!strcmp(key, "width")) {
        symbol->w = _toFloat(loader->svgParse, value, SvgParserLengthType::Horizontal);
        symbol->hasWidth = true;
    } else if (!strcmp(key, "height")) {
        symbol->h = _toFloat(loader->svgParse, value, SvgParserLengthType::Vertical);
        symbol->hasHeight = true;
    } else if (!strcmp(key, "preserveAspectRatio")) {
        _parseAspectRatio(&value, &symbol->align, &symbol->meetOrSlice);
    } else if (!strcmp(key, "overflow")) {
        if (!strcmp(value, "visible")) symbol->overflowVisible = true;
    } else {
        return _attrParseGNode(data, key, value);
    }

    return true;
}


static SvgNode* _createNode(SvgNode* parent, SvgNodeType type)
{
    SvgNode* node = (SvgNode*)calloc(1, sizeof(SvgNode));

    if (!node) return nullptr;

    //Default fill property
    node->style = (SvgStyleProperty*)calloc(1, sizeof(SvgStyleProperty));

    if (!node->style) {
        free(node);
        return nullptr;
    }

    //Update the default value of stroke and fill
    //https://www.w3.org/TR/SVGTiny12/painting.html#SpecifyingPaint
    node->style->fill.paint.none = false;
    //Default fill opacity is 1
    node->style->fill.opacity = 255;
    node->style->opacity = 255;
    //Default current color is not set
    node->style->fill.paint.curColor = false;
    node->style->curColorSet = false;
    //Default fill rule is nonzero
    node->style->fill.fillRule = FillRule::Winding;

    //Default stroke is none
    node->style->stroke.paint.none = true;
    //Default stroke opacity is 1
    node->style->stroke.opacity = 255;
    //Default stroke current color is not set
    node->style->stroke.paint.curColor = false;
    //Default stroke width is 1
    node->style->stroke.width = 1;
    //Default line cap is butt
    node->style->stroke.cap = StrokeCap::Butt;
    //Default line join is miter
    node->style->stroke.join = StrokeJoin::Miter;
    node->style->stroke.miterlimit = 4.0f;
    node->style->stroke.scale = 1.0;

    node->style->paintOrder = _toPaintOrder("fill stroke");

    //Default display is true("inline").
    node->style->display = true;

    node->parent = parent;
    node->type = type;

    if (parent) parent->child.push(node);
    return node;
}


static SvgNode* _createDefsNode(TVG_UNUSED SvgLoaderData* loader, TVG_UNUSED SvgNode* parent, const char* buf, unsigned bufLength, TVG_UNUSED parseAttributes func)
{
    if (loader->def && loader->doc->node.doc.defs) return loader->def;
    SvgNode* node = _createNode(nullptr, SvgNodeType::Defs);

    loader->def = node;
    loader->doc->node.doc.defs = node;
    return node;
}


static SvgNode* _createGNode(TVG_UNUSED SvgLoaderData* loader, SvgNode* parent, const char* buf, unsigned bufLength, parseAttributes func)
{
    loader->svgParse->node = _createNode(parent, SvgNodeType::G);
    if (!loader->svgParse->node) return nullptr;

    func(buf, bufLength, _attrParseGNode, loader);
    return loader->svgParse->node;
}


static SvgNode* _createSvgNode(SvgLoaderData* loader, SvgNode* parent, const char* buf, unsigned bufLength, parseAttributes func)
{
    loader->svgParse->node = _createNode(parent, SvgNodeType::Doc);
    if (!loader->svgParse->node) return nullptr;
    SvgDocNode* doc = &(loader->svgParse->node->node.doc);

    loader->svgParse->global.w = 1.0f;
    loader->svgParse->global.h = 1.0f;

    doc->align = AspectRatioAlign::XMidYMid;
    doc->meetOrSlice = AspectRatioMeetOrSlice::Meet;
    doc->viewFlag = SvgViewFlag::None;
    func(buf, bufLength, _attrParseSvgNode, loader);

    if (!(doc->viewFlag & SvgViewFlag::Viewbox)) {
        if (doc->viewFlag & SvgViewFlag::Width) {
            loader->svgParse->global.w = doc->w;
        }
        if (doc->viewFlag & SvgViewFlag::Height) {
            loader->svgParse->global.h = doc->h;
        }
    }
    return loader->svgParse->node;
}


static SvgNode* _createMaskNode(SvgLoaderData* loader, SvgNode* parent, TVG_UNUSED const char* buf, TVG_UNUSED unsigned bufLength, parseAttributes func)
{
    loader->svgParse->node = _createNode(parent, SvgNodeType::Mask);
    if (!loader->svgParse->node) return nullptr;

    loader->svgParse->node->node.mask.userSpace = true;
    loader->svgParse->node->node.mask.type = SvgMaskType::Luminance;

    func(buf, bufLength, _attrParseMaskNode, loader);

    return loader->svgParse->node;
}


static SvgNode* _createClipPathNode(SvgLoaderData* loader, SvgNode* parent, const char* buf, unsigned bufLength, parseAttributes func)
{
    loader->svgParse->node = _createNode(parent, SvgNodeType::ClipPath);
    if (!loader->svgParse->node) return nullptr;

    loader->svgParse->node->style->display = false;
    loader->svgParse->node->node.clip.userSpace = true;

    func(buf, bufLength, _attrParseClipPathNode, loader);

    return loader->svgParse->node;
}


static SvgNode* _createCssStyleNode(SvgLoaderData* loader, SvgNode* parent, const char* buf, unsigned bufLength, parseAttributes func)
{
    loader->svgParse->node = _createNode(parent, SvgNodeType::CssStyle);
    if (!loader->svgParse->node) return nullptr;

    func(buf, bufLength, _attrParseCssStyleNode, loader);

    return loader->svgParse->node;
}


static SvgNode* _createSymbolNode(SvgLoaderData* loader, SvgNode* parent, const char* buf, unsigned bufLength, parseAttributes func)
{
    loader->svgParse->node = _createNode(parent, SvgNodeType::Symbol);
    if (!loader->svgParse->node) return nullptr;

    loader->svgParse->node->node.symbol.align = AspectRatioAlign::XMidYMid;
    loader->svgParse->node->node.symbol.meetOrSlice = AspectRatioMeetOrSlice::Meet;
    loader->svgParse->node->node.symbol.overflowVisible = false;

    loader->svgParse->node->node.symbol.hasViewBox = false;
    loader->svgParse->node->node.symbol.hasWidth = false;
    loader->svgParse->node->node.symbol.hasHeight = false;
    loader->svgParse->node->node.symbol.vx = 0.0f;
    loader->svgParse->node->node.symbol.vy = 0.0f;

    func(buf, bufLength, _attrParseSymbolNode, loader);

    return loader->svgParse->node;
}


static bool _attrParsePathNode(void* data, const char* key, const char* value)
{
    SvgLoaderData* loader = (SvgLoaderData*)data;
    SvgNode* node = loader->svgParse->node;
    SvgPathNode* path = &(node->node.path);

    if (!strcmp(key, "d")) {
        if (path->path) free(path->path);
        //Temporary: need to copy
        path->path = _copyId(value);
    } else if (!strcmp(key, "style")) {
        return simpleXmlParseW3CAttribute(value, strlen(value), _parseStyleAttr, loader);
    } else if (!strcmp(key, "clip-path")) {
        _handleClipPathAttr(loader, node, value);
    } else if (!strcmp(key, "mask")) {
        _handleMaskAttr(loader, node, value);
    } else if (!strcmp(key, "id")) {
        if (node->id && value) free(node->id);
        node->id = _copyId(value);
    } else if (!strcmp(key, "class")) {
        _handleCssClassAttr(loader, node, value);
    } else {
        return _parseStyleAttr(loader, key, value, false);
    }
    return true;
}


static SvgNode* _createPathNode(SvgLoaderData* loader, SvgNode* parent, const char* buf, unsigned bufLength, parseAttributes func)
{
    loader->svgParse->node = _createNode(parent, SvgNodeType::Path);

    if (!loader->svgParse->node) return nullptr;

    func(buf, bufLength, _attrParsePathNode, loader);

    return loader->svgParse->node;
}


static constexpr struct
{
    const char* tag;
    SvgParserLengthType type;
    int sz;
    size_t offset;
} circleTags[] = {
    {"cx", SvgParserLengthType::Horizontal, sizeof("cx"), offsetof(SvgCircleNode, cx)},
    {"cy", SvgParserLengthType::Vertical, sizeof("cy"), offsetof(SvgCircleNode, cy)},
    {"r", SvgParserLengthType::Diagonal, sizeof("r"), offsetof(SvgCircleNode, r)}
};


/* parse the attributes for a circle element.
 * https://www.w3.org/TR/SVG/shapes.html#CircleElement
 */
static bool _attrParseCircleNode(void* data, const char* key, const char* value)
{
    SvgLoaderData* loader = (SvgLoaderData*)data;
    SvgNode* node = loader->svgParse->node;
    SvgCircleNode* circle = &(node->node.circle);
    unsigned char* array;
    int sz = strlen(key);

    array = (unsigned char*)circle;
    for (unsigned int i = 0; i < sizeof(circleTags) / sizeof(circleTags[0]); i++) {
        if (circleTags[i].sz - 1 == sz && !strncmp(circleTags[i].tag, key, sz)) {
            *((float*)(array + circleTags[i].offset)) = _toFloat(loader->svgParse, value, circleTags[i].type);
            return true;
        }
    }

    if (!strcmp(key, "style")) {
        return simpleXmlParseW3CAttribute(value, strlen(value), _parseStyleAttr, loader);
    } else if (!strcmp(key, "clip-path")) {
        _handleClipPathAttr(loader, node, value);
    } else if (!strcmp(key, "mask")) {
        _handleMaskAttr(loader, node, value);
    } else if (!strcmp(key, "id")) {
        if (node->id && value) free(node->id);
        node->id = _copyId(value);
    } else if (!strcmp(key, "class")) {
        _handleCssClassAttr(loader, node, value);
    } else {
        return _parseStyleAttr(loader, key, value, false);
    }
    return true;
}


static SvgNode* _createCircleNode(SvgLoaderData* loader, SvgNode* parent, const char* buf, unsigned bufLength, parseAttributes func)
{
    loader->svgParse->node = _createNode(parent, SvgNodeType::Circle);

    if (!loader->svgParse->node) return nullptr;

    func(buf, bufLength, _attrParseCircleNode, loader);
    return loader->svgParse->node;
}


static constexpr struct
{
    const char* tag;
    SvgParserLengthType type;
    int sz;
    size_t offset;
} ellipseTags[] = {
    {"cx", SvgParserLengthType::Horizontal, sizeof("cx"), offsetof(SvgEllipseNode, cx)},
    {"cy", SvgParserLengthType::Vertical, sizeof("cy"), offsetof(SvgEllipseNode, cy)},
    {"rx", SvgParserLengthType::Horizontal, sizeof("rx"), offsetof(SvgEllipseNode, rx)},
    {"ry", SvgParserLengthType::Vertical, sizeof("ry"), offsetof(SvgEllipseNode, ry)}
};


/* parse the attributes for an ellipse element.
 * https://www.w3.org/TR/SVG/shapes.html#EllipseElement
 */
static bool _attrParseEllipseNode(void* data, const char* key, const char* value)
{
    SvgLoaderData* loader = (SvgLoaderData*)data;
    SvgNode* node = loader->svgParse->node;
    SvgEllipseNode* ellipse = &(node->node.ellipse);
    unsigned char* array;
    int sz = strlen(key);

    array = (unsigned char*)ellipse;
    for (unsigned int i = 0; i < sizeof(ellipseTags) / sizeof(ellipseTags[0]); i++) {
        if (ellipseTags[i].sz - 1 == sz && !strncmp(ellipseTags[i].tag, key, sz)) {
            *((float*)(array + ellipseTags[i].offset)) = _toFloat(loader->svgParse, value, ellipseTags[i].type);
            return true;
        }
    }

    if (!strcmp(key, "id")) {
        if (node->id && value) free(node->id);
        node->id = _copyId(value);
    } else if (!strcmp(key, "class")) {
        _handleCssClassAttr(loader, node, value);
    } else if (!strcmp(key, "style")) {
        return simpleXmlParseW3CAttribute(value, strlen(value), _parseStyleAttr, loader);
    } else if (!strcmp(key, "clip-path")) {
        _handleClipPathAttr(loader, node, value);
    } else if (!strcmp(key, "mask")) {
        _handleMaskAttr(loader, node, value);
    } else {
        return _parseStyleAttr(loader, key, value, false);
    }
    return true;
}


static SvgNode* _createEllipseNode(SvgLoaderData* loader, SvgNode* parent, const char* buf, unsigned bufLength, parseAttributes func)
{
    loader->svgParse->node = _createNode(parent, SvgNodeType::Ellipse);

    if (!loader->svgParse->node) return nullptr;

    func(buf, bufLength, _attrParseEllipseNode, loader);
    return loader->svgParse->node;
}


static bool _attrParsePolygonPoints(const char* str, SvgPolygonNode* polygon)
{
    float num_x, num_y;
    while (_parseNumber(&str, nullptr, &num_x) && _parseNumber(&str, nullptr, &num_y)) {
        polygon->pts.push(num_x);
        polygon->pts.push(num_y);
    }
    return true;
}


/* parse the attributes for a polygon element.
 * https://www.w3.org/TR/SVG/shapes.html#PolylineElement
 */
static bool _attrParsePolygonNode(void* data, const char* key, const char* value)
{
    SvgLoaderData* loader = (SvgLoaderData*)data;
    SvgNode* node = loader->svgParse->node;
    SvgPolygonNode* polygon = nullptr;

    if (node->type == SvgNodeType::Polygon) polygon = &(node->node.polygon);
    else polygon = &(node->node.polyline);

    if (!strcmp(key, "points")) {
        return _attrParsePolygonPoints(value, polygon);
    } else if (!strcmp(key, "style")) {
        return simpleXmlParseW3CAttribute(value, strlen(value), _parseStyleAttr, loader);
    } else if (!strcmp(key, "clip-path")) {
        _handleClipPathAttr(loader, node, value);
    } else if (!strcmp(key, "mask")) {
        _handleMaskAttr(loader, node, value);
    } else if (!strcmp(key, "id")) {
        if (node->id && value) free(node->id);
        node->id = _copyId(value);
    } else if (!strcmp(key, "class")) {
        _handleCssClassAttr(loader, node, value);
    } else {
        return _parseStyleAttr(loader, key, value, false);
    }
    return true;
}


static SvgNode* _createPolygonNode(SvgLoaderData* loader, SvgNode* parent, const char* buf, unsigned bufLength, parseAttributes func)
{
    loader->svgParse->node = _createNode(parent, SvgNodeType::Polygon);

    if (!loader->svgParse->node) return nullptr;

    func(buf, bufLength, _attrParsePolygonNode, loader);
    return loader->svgParse->node;
}


static SvgNode* _createPolylineNode(SvgLoaderData* loader, SvgNode* parent, const char* buf, unsigned bufLength, parseAttributes func)
{
    loader->svgParse->node = _createNode(parent, SvgNodeType::Polyline);

    if (!loader->svgParse->node) return nullptr;

    func(buf, bufLength, _attrParsePolygonNode, loader);
    return loader->svgParse->node;
}

static constexpr struct
{
    const char* tag;
    SvgParserLengthType type;
    int sz;
    size_t offset;
} rectTags[] = {
    {"x", SvgParserLengthType::Horizontal, sizeof("x"), offsetof(SvgRectNode, x)},
    {"y", SvgParserLengthType::Vertical, sizeof("y"), offsetof(SvgRectNode, y)},
    {"width", SvgParserLengthType::Horizontal, sizeof("width"), offsetof(SvgRectNode, w)},
    {"height", SvgParserLengthType::Vertical, sizeof("height"), offsetof(SvgRectNode, h)},
    {"rx", SvgParserLengthType::Horizontal, sizeof("rx"), offsetof(SvgRectNode, rx)},
    {"ry", SvgParserLengthType::Vertical, sizeof("ry"), offsetof(SvgRectNode, ry)}
};


/* parse the attributes for a rect element.
 * https://www.w3.org/TR/SVG/shapes.html#RectElement
 */
static bool _attrParseRectNode(void* data, const char* key, const char* value)
{
    SvgLoaderData* loader = (SvgLoaderData*)data;
    SvgNode* node = loader->svgParse->node;
    SvgRectNode* rect = &(node->node.rect);
    unsigned char* array;
    bool ret = true;
    int sz = strlen(key);

    array = (unsigned char*)rect;
    for (unsigned int i = 0; i < sizeof(rectTags) / sizeof(rectTags[0]); i++) {
        if (rectTags[i].sz - 1 == sz && !strncmp(rectTags[i].tag, key, sz)) {
            *((float*)(array + rectTags[i].offset)) = _toFloat(loader->svgParse, value, rectTags[i].type);

            //Case if only rx or ry is declared
            if (!strncmp(rectTags[i].tag, "rx", sz)) rect->hasRx = true;
            if (!strncmp(rectTags[i].tag, "ry", sz)) rect->hasRy = true;

            if ((rect->rx >= FLOAT_EPSILON) && (rect->ry < FLOAT_EPSILON) && rect->hasRx && !rect->hasRy) rect->ry = rect->rx;
            if ((rect->ry >= FLOAT_EPSILON) && (rect->rx < FLOAT_EPSILON) && !rect->hasRx && rect->hasRy) rect->rx = rect->ry;
            return ret;
        }
    }

    if (!strcmp(key, "id")) {
        if (node->id && value) free(node->id);
        node->id = _copyId(value);
    } else if (!strcmp(key, "class")) {
        _handleCssClassAttr(loader, node, value);
    } else if (!strcmp(key, "style")) {
        ret = simpleXmlParseW3CAttribute(value, strlen(value), _parseStyleAttr, loader);
    } else if (!strcmp(key, "clip-path")) {
        _handleClipPathAttr(loader, node, value);
    } else if (!strcmp(key, "mask")) {
        _handleMaskAttr(loader, node, value);
    } else {
        ret = _parseStyleAttr(loader, key, value, false);
    }

    return ret;
}


static SvgNode* _createRectNode(SvgLoaderData* loader, SvgNode* parent, const char* buf, unsigned bufLength, parseAttributes func)
{
    loader->svgParse->node = _createNode(parent, SvgNodeType::Rect);

    if (!loader->svgParse->node) return nullptr;

    loader->svgParse->node->node.rect.hasRx = loader->svgParse->node->node.rect.hasRy = false;

    func(buf, bufLength, _attrParseRectNode, loader);
    return loader->svgParse->node;
}


static constexpr struct
{
    const char* tag;
    SvgParserLengthType type;
    int sz;
    size_t offset;
} lineTags[] = {
    {"x1", SvgParserLengthType::Horizontal, sizeof("x1"), offsetof(SvgLineNode, x1)},
    {"y1", SvgParserLengthType::Vertical, sizeof("y1"), offsetof(SvgLineNode, y1)},
    {"x2", SvgParserLengthType::Horizontal, sizeof("x2"), offsetof(SvgLineNode, x2)},
    {"y2", SvgParserLengthType::Vertical, sizeof("y2"), offsetof(SvgLineNode, y2)}
};


/* parse the attributes for a line element.
 * https://www.w3.org/TR/SVG/shapes.html#LineElement
 */
static bool _attrParseLineNode(void* data, const char* key, const char* value)
{
    SvgLoaderData* loader = (SvgLoaderData*)data;
    SvgNode* node = loader->svgParse->node;
    SvgLineNode* line = &(node->node.line);
    unsigned char* array;
    int sz = strlen(key);

    array = (unsigned char*)line;
    for (unsigned int i = 0; i < sizeof(lineTags) / sizeof(lineTags[0]); i++) {
        if (lineTags[i].sz - 1 == sz && !strncmp(lineTags[i].tag, key, sz)) {
            *((float*)(array + lineTags[i].offset)) = _toFloat(loader->svgParse, value, lineTags[i].type);
            return true;
        }
    }

    if (!strcmp(key, "id")) {
        if (node->id && value) free(node->id);
        node->id = _copyId(value);
    } else if (!strcmp(key, "class")) {
        _handleCssClassAttr(loader, node, value);
    } else if (!strcmp(key, "style")) {
        return simpleXmlParseW3CAttribute(value, strlen(value), _parseStyleAttr, loader);
    } else if (!strcmp(key, "clip-path")) {
        _handleClipPathAttr(loader, node, value);
    } else if (!strcmp(key, "mask")) {
        _handleMaskAttr(loader, node, value);
    } else {
        return _parseStyleAttr(loader, key, value, false);
    }
    return true;
}


static SvgNode* _createLineNode(SvgLoaderData* loader, SvgNode* parent, const char* buf, unsigned bufLength, parseAttributes func)
{
    loader->svgParse->node = _createNode(parent, SvgNodeType::Line);

    if (!loader->svgParse->node) return nullptr;

    func(buf, bufLength, _attrParseLineNode, loader);
    return loader->svgParse->node;
}


static char* _idFromHref(const char* href)
{
    href = _skipSpace(href, nullptr);
    if ((*href) == '#') href++;
    return strdup(href);
}


static constexpr struct
{
    const char* tag;
    SvgParserLengthType type;
    int sz;
    size_t offset;
} imageTags[] = {
    {"x", SvgParserLengthType::Horizontal, sizeof("x"), offsetof(SvgRectNode, x)},
    {"y", SvgParserLengthType::Vertical, sizeof("y"), offsetof(SvgRectNode, y)},
    {"width", SvgParserLengthType::Horizontal, sizeof("width"), offsetof(SvgRectNode, w)},
    {"height", SvgParserLengthType::Vertical, sizeof("height"), offsetof(SvgRectNode, h)},
};


/* parse the attributes for a image element.
 * https://www.w3.org/TR/SVG/embedded.html#ImageElement
 */
static bool _attrParseImageNode(void* data, const char* key, const char* value)
{
    SvgLoaderData* loader = (SvgLoaderData*)data;
    SvgNode* node = loader->svgParse->node;
    SvgImageNode* image = &(node->node.image);
    unsigned char* array;
    int sz = strlen(key);

    array = (unsigned char*)image;
    for (unsigned int i = 0; i < sizeof(imageTags) / sizeof(imageTags[0]); i++) {
        if (imageTags[i].sz - 1 == sz && !strncmp(imageTags[i].tag, key, sz)) {
            *((float*)(array + imageTags[i].offset)) = _toFloat(loader->svgParse, value, imageTags[i].type);
            return true;
        }
    }

    if (!strcmp(key, "href") || !strcmp(key, "xlink:href")) {
        if (image->href && value) free(image->href);
        image->href = _idFromHref(value);
    } else if (!strcmp(key, "id")) {
        if (node->id && value) free(node->id);
        node->id = _copyId(value);
    } else if (!strcmp(key, "class")) {
        _handleCssClassAttr(loader, node, value);
    } else if (!strcmp(key, "style")) {
        return simpleXmlParseW3CAttribute(value, strlen(value), _parseStyleAttr, loader);
    } else if (!strcmp(key, "clip-path")) {
        _handleClipPathAttr(loader, node, value);
    } else if (!strcmp(key, "mask")) {
        _handleMaskAttr(loader, node, value);
    } else if (!strcmp(key, "transform")) {
        node->transform = _parseTransformationMatrix(value);
    } else {
        return _parseStyleAttr(loader, key, value);
    }
    return true;
}


static SvgNode* _createImageNode(SvgLoaderData* loader, SvgNode* parent, const char* buf, unsigned bufLength, parseAttributes func)
{
    loader->svgParse->node = _createNode(parent, SvgNodeType::Image);

    if (!loader->svgParse->node) return nullptr;

    func(buf, bufLength, _attrParseImageNode, loader);
    return loader->svgParse->node;
}


static SvgNode* _getDefsNode(SvgNode* node)
{
    if (!node) return nullptr;

    while (node->parent != nullptr) {
        node = node->parent;
    }

    if (node->type == SvgNodeType::Doc) return node->node.doc.defs;
    if (node->type == SvgNodeType::Defs) return node;

    return nullptr;
}


static SvgNode* _findNodeById(SvgNode *node, const char* id)
{
    if (!node) return nullptr;

    SvgNode* result = nullptr;
    if (node->id && !strcmp(node->id, id)) return node;

    if (node->child.count > 0) {
        auto child = node->child.data;
        for (uint32_t i = 0; i < node->child.count; ++i, ++child) {
            result = _findNodeById(*child, id);
            if (result) break;
        }
    }
    return result;
}


static SvgNode* _findParentById(SvgNode* node, char* id, SvgNode* doc)
{
    SvgNode *parent = node->parent;
    while (parent != nullptr && parent != doc) {
        if (parent->id && !strcmp(parent->id, id)) {
            return parent;
        }
        parent = parent->parent;
    }
    return nullptr;
}


static constexpr struct
{
    const char* tag;
    SvgParserLengthType type;
    int sz;
    size_t offset;
} useTags[] = {
    {"x", SvgParserLengthType::Horizontal, sizeof("x"), offsetof(SvgUseNode, x)},
    {"y", SvgParserLengthType::Vertical, sizeof("y"), offsetof(SvgUseNode, y)},
    {"width", SvgParserLengthType::Horizontal, sizeof("width"), offsetof(SvgUseNode, w)},
    {"height", SvgParserLengthType::Vertical, sizeof("height"), offsetof(SvgUseNode, h)}
};


static void _cloneNode(SvgNode* from, SvgNode* parent, int depth);
static bool _attrParseUseNode(void* data, const char* key, const char* value)
{
    SvgLoaderData* loader = (SvgLoaderData*)data;
    SvgNode *defs, *nodeFrom, *node = loader->svgParse->node;
    char* id;

    SvgUseNode* use = &(node->node.use);
    int sz = strlen(key);
    unsigned char* array = (unsigned char*)use;
    for (unsigned int i = 0; i < sizeof(useTags) / sizeof(useTags[0]); i++) {
        if (useTags[i].sz - 1 == sz && !strncmp(useTags[i].tag, key, sz)) {
            *((float*)(array + useTags[i].offset)) = _toFloat(loader->svgParse, value, useTags[i].type);

            if (useTags[i].offset == offsetof(SvgUseNode, w)) use->isWidthSet = true;
            else if (useTags[i].offset == offsetof(SvgUseNode, h)) use->isHeightSet = true;

            return true;
        }
    }

    if (!strcmp(key, "href") || !strcmp(key, "xlink:href")) {
        id = _idFromHref(value);
        defs = _getDefsNode(node);
        nodeFrom = _findNodeById(defs, id);
        if (nodeFrom) {
            if (!_findParentById(node, id, loader->doc)) {
                _cloneNode(nodeFrom, node, 0);
                if (nodeFrom->type == SvgNodeType::Symbol) use->symbol = nodeFrom;
            } else {
                TVGLOG("SVG", "%s is ancestor element. This reference is invalid.", id);
            }
            free(id);
        } else {
            //some svg export software include <defs> element at the end of the file
            //if so the 'from' element won't be found now and we have to repeat finding
            //after the whole file is parsed
            _postpone(loader->cloneNodes, node, id);
        }
    } else {
        return _attrParseGNode(data, key, value);
    }
    return true;
}


static SvgNode* _createUseNode(SvgLoaderData* loader, SvgNode* parent, const char* buf, unsigned bufLength, parseAttributes func)
{
    loader->svgParse->node = _createNode(parent, SvgNodeType::Use);

    if (!loader->svgParse->node) return nullptr;

    loader->svgParse->node->node.use.isWidthSet = false;
    loader->svgParse->node->node.use.isHeightSet = false;

    func(buf, bufLength, _attrParseUseNode, loader);
    return loader->svgParse->node;
}


static constexpr struct
{
    const char* tag;
    SvgParserLengthType type;
    int sz;
    size_t offset;
} textTags[] = {
        {"x", SvgParserLengthType::Horizontal, sizeof("x"), offsetof(SvgTextNode, x)},
        {"y", SvgParserLengthType::Vertical, sizeof("y"), offsetof(SvgTextNode, y)},
        {"font-size", SvgParserLengthType::Vertical, sizeof("font-size"), offsetof(SvgTextNode, fontSize)}
};


static bool _attrParseTextNode(void* data, const char* key, const char* value)
{
    SvgLoaderData* loader = (SvgLoaderData*)data;
    SvgNode* node = loader->svgParse->node;
    SvgTextNode* text = &(node->node.text);

    unsigned char* array;
    int sz = strlen(key);

    array = (unsigned char*)text;
    for (unsigned int i = 0; i < sizeof(textTags) / sizeof(textTags[0]); i++) {
        if (textTags[i].sz - 1 == sz && !strncmp(textTags[i].tag, key, sz)) {
            *((float*)(array + textTags[i].offset)) = _toFloat(loader->svgParse, value, textTags[i].type);
            return true;
        }
    }

    if (!strcmp(key, "font-family")) {
        if (text->fontFamily && value) free(text->fontFamily);
        text->fontFamily = strdup(value);
    } else if (!strcmp(key, "style")) {
        return simpleXmlParseW3CAttribute(value, strlen(value), _parseStyleAttr, loader);
    } else if (!strcmp(key, "clip-path")) {
        _handleClipPathAttr(loader, node, value);
    } else if (!strcmp(key, "mask")) {
        _handleMaskAttr(loader, node, value);
    } else if (!strcmp(key, "id")) {
        if (node->id && value) free(node->id);
        node->id = _copyId(value);
    } else if (!strcmp(key, "class")) {
        _handleCssClassAttr(loader, node, value);
    } else {
        return _parseStyleAttr(loader, key, value, false);
    }
    return true;
}


static SvgNode* _createTextNode(SvgLoaderData* loader, SvgNode* parent, const char* buf, unsigned bufLength, parseAttributes func)
{
    loader->svgParse->node = _createNode(parent, SvgNodeType::Text);
    if (!loader->svgParse->node) return nullptr;

    //TODO: support the def font and size as used in a system?
    loader->svgParse->node->node.text.fontSize = 10.0f;
    loader->svgParse->node->node.text.fontFamily = nullptr;
    loader->svgParse->node->node.text.text = nullptr;

    func(buf, bufLength, _attrParseTextNode, loader);

    return loader->svgParse->node;
}


static constexpr struct
{
    const char* tag;
    int sz;
    FactoryMethod tagHandler;
} graphicsTags[] = {
    {"use", sizeof("use"), _createUseNode},
    {"circle", sizeof("circle"), _createCircleNode},
    {"ellipse", sizeof("ellipse"), _createEllipseNode},
    {"path", sizeof("path"), _createPathNode},
    {"polygon", sizeof("polygon"), _createPolygonNode},
    {"rect", sizeof("rect"), _createRectNode},
    {"polyline", sizeof("polyline"), _createPolylineNode},
    {"line", sizeof("line"), _createLineNode},
    {"image", sizeof("image"), _createImageNode},
    {"text", sizeof("text"), _createTextNode}
};


static constexpr struct
{
    const char* tag;
    int sz;
    FactoryMethod tagHandler;
} groupTags[] = {
    {"defs", sizeof("defs"), _createDefsNode},
    {"g", sizeof("g"), _createGNode},
    {"svg", sizeof("svg"), _createSvgNode},
    {"mask", sizeof("mask"), _createMaskNode},
    {"clipPath", sizeof("clipPath"), _createClipPathNode},
    {"style", sizeof("style"), _createCssStyleNode},
    {"symbol", sizeof("symbol"), _createSymbolNode}
};


#define FIND_FACTORY(Short_Name, Tags_Array)                                           \
    static FactoryMethod                                                               \
        _find##Short_Name##Factory(const char* name)                                   \
    {                                                                                  \
        unsigned int i;                                                                \
        int sz = strlen(name);                                                         \
                                                                                       \
        for (i = 0; i < sizeof(Tags_Array) / sizeof(Tags_Array[0]); i++) {             \
            if (Tags_Array[i].sz - 1 == sz && !strncmp(Tags_Array[i].tag, name, sz)) { \
                return Tags_Array[i].tagHandler;                                       \
            }                                                                          \
        }                                                                              \
        return nullptr;                                                                \
    }

FIND_FACTORY(Group, groupTags)
FIND_FACTORY(Graphics, graphicsTags)


FillSpread _parseSpreadValue(const char* value)
{
    auto spread = FillSpread::Pad;

    if (!strcmp(value, "reflect")) {
        spread = FillSpread::Reflect;
    } else if (!strcmp(value, "repeat")) {
        spread = FillSpread::Repeat;
    }

    return spread;
}


static void _handleRadialCxAttr(SvgLoaderData* loader, SvgRadialGradient* radial, const char* value)
{
    radial->cx = _gradientToFloat(loader->svgParse, value, radial->isCxPercentage);
    if (!loader->svgParse->gradient.parsedFx) {
        radial->fx = radial->cx;
        radial->isFxPercentage = radial->isCxPercentage;
    }
}


static void _handleRadialCyAttr(SvgLoaderData* loader, SvgRadialGradient* radial, const char* value)
{
    radial->cy = _gradientToFloat(loader->svgParse, value, radial->isCyPercentage);
    if (!loader->svgParse->gradient.parsedFy) {
        radial->fy = radial->cy;
        radial->isFyPercentage = radial->isCyPercentage;
    }
}


static void _handleRadialFxAttr(SvgLoaderData* loader, SvgRadialGradient* radial, const char* value)
{
    radial->fx = _gradientToFloat(loader->svgParse, value, radial->isFxPercentage);
    loader->svgParse->gradient.parsedFx = true;
}


static void _handleRadialFyAttr(SvgLoaderData* loader, SvgRadialGradient* radial, const char* value)
{
    radial->fy = _gradientToFloat(loader->svgParse, value, radial->isFyPercentage);
    loader->svgParse->gradient.parsedFy = true;
}


static void _handleRadialFrAttr(SvgLoaderData* loader, SvgRadialGradient* radial, const char* value)
{
    radial->fr = _gradientToFloat(loader->svgParse, value, radial->isFrPercentage);
}


static void _handleRadialRAttr(SvgLoaderData* loader, SvgRadialGradient* radial, const char* value)
{
    radial->r = _gradientToFloat(loader->svgParse, value, radial->isRPercentage);
}


static void _recalcRadialCxAttr(SvgLoaderData* loader, SvgRadialGradient* radial, bool userSpace)
{
    if (userSpace && !radial->isCxPercentage) radial->cx = radial->cx / loader->svgParse->global.w;
}


static void _recalcRadialCyAttr(SvgLoaderData* loader, SvgRadialGradient* radial, bool userSpace)
{
    if (userSpace && !radial->isCyPercentage) radial->cy = radial->cy / loader->svgParse->global.h;
}


static void _recalcRadialFxAttr(SvgLoaderData* loader, SvgRadialGradient* radial, bool userSpace)
{
    if (userSpace && !radial->isFxPercentage) radial->fx = radial->fx / loader->svgParse->global.w;
}


static void _recalcRadialFyAttr(SvgLoaderData* loader, SvgRadialGradient* radial, bool userSpace)
{
    if (userSpace && !radial->isFyPercentage) radial->fy = radial->fy / loader->svgParse->global.h;
}


static void _recalcRadialFrAttr(SvgLoaderData* loader, SvgRadialGradient* radial, bool userSpace)
{
    // scaling factor based on the Units paragraph from : https://www.w3.org/TR/2015/WD-SVG2-20150915/coords.html
    if (userSpace && !radial->isFrPercentage) radial->fr = radial->fr / (sqrtf(powf(loader->svgParse->global.h, 2) + powf(loader->svgParse->global.w, 2)) / sqrtf(2.0));
}


static void _recalcRadialRAttr(SvgLoaderData* loader, SvgRadialGradient* radial, bool userSpace)
{
    // scaling factor based on the Units paragraph from : https://www.w3.org/TR/2015/WD-SVG2-20150915/coords.html
    if (userSpace && !radial->isRPercentage) radial->r = radial->r / (sqrtf(powf(loader->svgParse->global.h, 2) + powf(loader->svgParse->global.w, 2)) / sqrtf(2.0));
}


static void _recalcInheritedRadialCxAttr(SvgLoaderData* loader, SvgRadialGradient* radial, bool userSpace)
{
    if (!radial->isCxPercentage) {
        if (userSpace) radial->cx /= loader->svgParse->global.w;
        else radial->cx *= loader->svgParse->global.w;
    }
}


static void _recalcInheritedRadialCyAttr(SvgLoaderData* loader, SvgRadialGradient* radial, bool userSpace)
{
    if (!radial->isCyPercentage) {
        if (userSpace) radial->cy /= loader->svgParse->global.h;
        else radial->cy *= loader->svgParse->global.h;
    }
}


static void _recalcInheritedRadialFxAttr(SvgLoaderData* loader, SvgRadialGradient* radial, bool userSpace)
{
    if (!radial->isFxPercentage) {
        if (userSpace) radial->fx /= loader->svgParse->global.w;
        else radial->fx *= loader->svgParse->global.w;
    }
}


static void _recalcInheritedRadialFyAttr(SvgLoaderData* loader, SvgRadialGradient* radial, bool userSpace)
{
    if (!radial->isFyPercentage) {
        if (userSpace) radial->fy /= loader->svgParse->global.h;
        else radial->fy *= loader->svgParse->global.h;
    }
}


static void _recalcInheritedRadialFrAttr(SvgLoaderData* loader, SvgRadialGradient* radial, bool userSpace)
{
    if (!radial->isFrPercentage) {
        if (userSpace) radial->fr /= sqrtf(powf(loader->svgParse->global.h, 2) + powf(loader->svgParse->global.w, 2)) / sqrtf(2.0);
        else radial->fr *= sqrtf(powf(loader->svgParse->global.h, 2) + powf(loader->svgParse->global.w, 2)) / sqrtf(2.0);
    }
}


static void _recalcInheritedRadialRAttr(SvgLoaderData* loader, SvgRadialGradient* radial, bool userSpace)
{
    if (!radial->isRPercentage) {
        if (userSpace) radial->r /= sqrtf(powf(loader->svgParse->global.h, 2) + powf(loader->svgParse->global.w, 2)) / sqrtf(2.0);
        else radial->r *= sqrtf(powf(loader->svgParse->global.h, 2) + powf(loader->svgParse->global.w, 2)) / sqrtf(2.0);
    }
}


static void _inheritRadialCxAttr(SvgStyleGradient* to, SvgStyleGradient* from)
{
    to->radial->cx = from->radial->cx;
    to->radial->isCxPercentage = from->radial->isCxPercentage;
    to->flags = (to->flags | SvgGradientFlags::Cx);
}


static void _inheritRadialCyAttr(SvgStyleGradient* to, SvgStyleGradient* from)
{
    to->radial->cy = from->radial->cy;
    to->radial->isCyPercentage = from->radial->isCyPercentage;
    to->flags = (to->flags | SvgGradientFlags::Cy);
}


static void _inheritRadialFxAttr(SvgStyleGradient* to, SvgStyleGradient* from)
{
    to->radial->fx = from->radial->fx;
    to->radial->isFxPercentage = from->radial->isFxPercentage;
    to->flags = (to->flags | SvgGradientFlags::Fx);
}


static void _inheritRadialFyAttr(SvgStyleGradient* to, SvgStyleGradient* from)
{
    to->radial->fy = from->radial->fy;
    to->radial->isFyPercentage = from->radial->isFyPercentage;
    to->flags = (to->flags | SvgGradientFlags::Fy);
}


static void _inheritRadialFrAttr(SvgStyleGradient* to, SvgStyleGradient* from)
{
    to->radial->fr = from->radial->fr;
    to->radial->isFrPercentage = from->radial->isFrPercentage;
    to->flags = (to->flags | SvgGradientFlags::Fr);
}


static void _inheritRadialRAttr(SvgStyleGradient* to, SvgStyleGradient* from)
{
    to->radial->r = from->radial->r;
    to->radial->isRPercentage = from->radial->isRPercentage;
    to->flags = (to->flags | SvgGradientFlags::R);
}


typedef void (*radialMethod)(SvgLoaderData* loader, SvgRadialGradient* radial, const char* value);
typedef void (*radialInheritMethod)(SvgStyleGradient* to, SvgStyleGradient* from);
typedef void (*radialMethodRecalc)(SvgLoaderData* loader, SvgRadialGradient* radial, bool userSpace);


#define RADIAL_DEF(Name, Name1, Flag)                                                    \
    {                                                                                    \
#Name, sizeof(#Name), _handleRadial##Name1##Attr, _inheritRadial##Name1##Attr, _recalcRadial##Name1##Attr, _recalcInheritedRadial##Name1##Attr, Flag \
    }


static constexpr struct
{
    const char* tag;
    int sz;
    radialMethod tagHandler;
    radialInheritMethod tagInheritHandler;
    radialMethodRecalc tagRecalc;
    radialMethodRecalc tagInheritedRecalc;
    SvgGradientFlags flag;
} radialTags[] = {
    RADIAL_DEF(cx, Cx, SvgGradientFlags::Cx),
    RADIAL_DEF(cy, Cy, SvgGradientFlags::Cy),
    RADIAL_DEF(fx, Fx, SvgGradientFlags::Fx),
    RADIAL_DEF(fy, Fy, SvgGradientFlags::Fy),
    RADIAL_DEF(r, R, SvgGradientFlags::R),
    RADIAL_DEF(fr, Fr, SvgGradientFlags::Fr)
};


static bool _attrParseRadialGradientNode(void* data, const char* key, const char* value)
{
    SvgLoaderData* loader = (SvgLoaderData*)data;
    SvgStyleGradient* grad = loader->svgParse->styleGrad;
    SvgRadialGradient* radial = grad->radial;
    int sz = strlen(key);

    for (unsigned int i = 0; i < sizeof(radialTags) / sizeof(radialTags[0]); i++) {
        if (radialTags[i].sz - 1 == sz && !strncmp(radialTags[i].tag, key, sz)) {
            radialTags[i].tagHandler(loader, radial, value);
            grad->flags = (grad->flags | radialTags[i].flag);
            return true;
        }
    }

    if (!strcmp(key, "id")) {
        if (grad->id && value) free(grad->id);
        grad->id = _copyId(value);
    } else if (!strcmp(key, "spreadMethod")) {
        grad->spread = _parseSpreadValue(value);
        grad->flags = (grad->flags | SvgGradientFlags::SpreadMethod);
    } else if (!strcmp(key, "href") || !strcmp(key, "xlink:href")) {
        if (grad->ref && value) free(grad->ref);
        grad->ref = _idFromHref(value);
    } else if (!strcmp(key, "gradientUnits")) {
        if (!strcmp(value, "userSpaceOnUse")) grad->userSpace = true;
        grad->flags = (grad->flags | SvgGradientFlags::GradientUnits);
    } else if (!strcmp(key, "gradientTransform")) {
        grad->transform = _parseTransformationMatrix(value);
    } else {
        return false;
    }

    return true;
}


static SvgStyleGradient* _createRadialGradient(SvgLoaderData* loader, const char* buf, unsigned bufLength)
{
    auto grad = (SvgStyleGradient*)(calloc(1, sizeof(SvgStyleGradient)));
    loader->svgParse->styleGrad = grad;

    grad->flags = SvgGradientFlags::None;
    grad->type = SvgGradientType::Radial;
    grad->userSpace = false;
    grad->radial = (SvgRadialGradient*)calloc(1, sizeof(SvgRadialGradient));
    if (!grad->radial) {
        grad->clear();
        free(grad);
        return nullptr;
    }
    /**
    * Default values of gradient transformed into global percentage
    */
    grad->radial->cx = 0.5f;
    grad->radial->cy = 0.5f;
    grad->radial->fx = 0.5f;
    grad->radial->fy = 0.5f;
    grad->radial->r = 0.5f;
    grad->radial->isCxPercentage = true;
    grad->radial->isCyPercentage = true;
    grad->radial->isFxPercentage = true;
    grad->radial->isFyPercentage = true;
    grad->radial->isRPercentage = true;
    grad->radial->isFrPercentage = true;

    loader->svgParse->gradient.parsedFx = false;
    loader->svgParse->gradient.parsedFy = false;
    simpleXmlParseAttributes(buf, bufLength,
        _attrParseRadialGradientNode, loader);

    for (unsigned int i = 0; i < sizeof(radialTags) / sizeof(radialTags[0]); i++) {
        radialTags[i].tagRecalc(loader, grad->radial, grad->userSpace);
    }

    return loader->svgParse->styleGrad;
}


static SvgColor* _findLatestColor(const SvgLoaderData* loader)
{
    auto parent = loader->stack.count > 0 ? loader->stack.last() : loader->doc;

    while (parent != nullptr) {
        if (parent->style->curColorSet) return &parent->style->color;
        parent = parent->parent;
    }
    return nullptr;
}


static bool _attrParseStopsStyle(void* data, const char* key, const char* value)
{
    SvgLoaderData* loader = (SvgLoaderData*)data;
    auto stop = &loader->svgParse->gradStop;

    if (!strcmp(key, "stop-opacity")) {
        stop->a = _toOpacity(value);
        loader->svgParse->flags = (loader->svgParse->flags | SvgStopStyleFlags::StopOpacity);
    } else if (!strcmp(key, "stop-color")) {
        if (!strcmp(value, "currentColor")) {
            if (auto latestColor = _findLatestColor(loader)) {
                stop->r = latestColor->r;
                stop->g = latestColor->g;
                stop->b = latestColor->b;
            }
        } else if (_toColor(value, &stop->r, &stop->g, &stop->b, nullptr)) {
            loader->svgParse->flags = (loader->svgParse->flags | SvgStopStyleFlags::StopColor);
        }
    } else {
        return false;
    }

    return true;
}


static bool _attrParseStops(void* data, const char* key, const char* value)
{
    SvgLoaderData* loader = (SvgLoaderData*)data;
    auto stop = &loader->svgParse->gradStop;

    if (!strcmp(key, "offset")) {
        stop->offset = _toOffset(value);
    } else if (!strcmp(key, "stop-opacity")) {
        if (!(loader->svgParse->flags & SvgStopStyleFlags::StopOpacity)) {
            stop->a = _toOpacity(value);
        }
    } else if (!strcmp(key, "stop-color")) {
        if (!strcmp(value, "currentColor")) {
            if (auto latestColor = _findLatestColor(loader)) {
                stop->r = latestColor->r;
                stop->g = latestColor->g;
                stop->b = latestColor->b;
            }
        } else if (!(loader->svgParse->flags & SvgStopStyleFlags::StopColor)) {
            _toColor(value, &stop->r, &stop->g, &stop->b, nullptr);
        }
    } else if (!strcmp(key, "style")) {
        simpleXmlParseW3CAttribute(value, strlen(value), _attrParseStopsStyle, data);
    } else {
        return false;
    }

    return true;
}


static void _handleLinearX1Attr(SvgLoaderData* loader, SvgLinearGradient* linear, const char* value)
{
    linear->x1 = _gradientToFloat(loader->svgParse, value, linear->isX1Percentage);
}


static void _handleLinearY1Attr(SvgLoaderData* loader, SvgLinearGradient* linear, const char* value)
{
    linear->y1 = _gradientToFloat(loader->svgParse, value, linear->isY1Percentage);
}


static void _handleLinearX2Attr(SvgLoaderData* loader, SvgLinearGradient* linear, const char* value)
{
    linear->x2 = _gradientToFloat(loader->svgParse, value, linear->isX2Percentage);
}


static void _handleLinearY2Attr(SvgLoaderData* loader, SvgLinearGradient* linear, const char* value)
{
    linear->y2 = _gradientToFloat(loader->svgParse, value, linear->isY2Percentage);
}


static void _recalcLinearX1Attr(SvgLoaderData* loader, SvgLinearGradient* linear, bool userSpace)
{
    if (userSpace && !linear->isX1Percentage) linear->x1 = linear->x1 / loader->svgParse->global.w;
}


static void _recalcLinearY1Attr(SvgLoaderData* loader, SvgLinearGradient* linear, bool userSpace)
{
    if (userSpace && !linear->isY1Percentage) linear->y1 = linear->y1 / loader->svgParse->global.h;
}


static void _recalcLinearX2Attr(SvgLoaderData* loader, SvgLinearGradient* linear, bool userSpace)
{
    if (userSpace && !linear->isX2Percentage) linear->x2 = linear->x2 / loader->svgParse->global.w;
}


static void _recalcLinearY2Attr(SvgLoaderData* loader, SvgLinearGradient* linear, bool userSpace)
{
    if (userSpace && !linear->isY2Percentage) linear->y2 = linear->y2 / loader->svgParse->global.h;
}


static void _recalcInheritedLinearX1Attr(SvgLoaderData* loader, SvgLinearGradient* linear, bool userSpace)
{
    if (!linear->isX1Percentage) {
        if (userSpace) linear->x1 /= loader->svgParse->global.w;
        else linear->x1 *= loader->svgParse->global.w;
    }
}


static void _recalcInheritedLinearX2Attr(SvgLoaderData* loader, SvgLinearGradient* linear, bool userSpace)
{
    if (!linear->isX2Percentage) {
        if (userSpace) linear->x2 /= loader->svgParse->global.w;
        else linear->x2 *= loader->svgParse->global.w;
    }
}


static void _recalcInheritedLinearY1Attr(SvgLoaderData* loader, SvgLinearGradient* linear, bool userSpace)
{
    if (!linear->isY1Percentage) {
        if (userSpace) linear->y1 /= loader->svgParse->global.h;
        else linear->y1 *= loader->svgParse->global.h;
    }
}


static void _recalcInheritedLinearY2Attr(SvgLoaderData* loader, SvgLinearGradient* linear, bool userSpace)
{
    if (!linear->isY2Percentage) {
        if (userSpace) linear->y2 /= loader->svgParse->global.h;
        else linear->y2 *= loader->svgParse->global.h;
    }
}


static void _inheritLinearX1Attr(SvgStyleGradient* to, SvgStyleGradient* from)
{
    to->linear->x1 = from->linear->x1;
    to->linear->isX1Percentage = from->linear->isX1Percentage;
    to->flags = (to->flags | SvgGradientFlags::X1);
}


static void _inheritLinearX2Attr(SvgStyleGradient* to, SvgStyleGradient* from)
{
    to->linear->x2 = from->linear->x2;
    to->linear->isX2Percentage = from->linear->isX2Percentage;
    to->flags = (to->flags | SvgGradientFlags::X2);
}


static void _inheritLinearY1Attr(SvgStyleGradient* to, SvgStyleGradient* from)
{
    to->linear->y1 = from->linear->y1;
    to->linear->isY1Percentage = from->linear->isY1Percentage;
    to->flags = (to->flags | SvgGradientFlags::Y1);
}


static void _inheritLinearY2Attr(SvgStyleGradient* to, SvgStyleGradient* from)
{
    to->linear->y2 = from->linear->y2;
    to->linear->isY2Percentage = from->linear->isY2Percentage;
    to->flags = (to->flags | SvgGradientFlags::Y2);
}


typedef void (*Linear_Method)(SvgLoaderData* loader, SvgLinearGradient* linear, const char* value);
typedef void (*Linear_Inherit_Method)(SvgStyleGradient* to, SvgStyleGradient* from);
typedef void (*Linear_Method_Recalc)(SvgLoaderData* loader, SvgLinearGradient* linear, bool userSpace);


#define LINEAR_DEF(Name, Name1, Flag)                                                    \
    {                                                                                    \
#Name, sizeof(#Name), _handleLinear##Name1##Attr, _inheritLinear##Name1##Attr, _recalcLinear##Name1##Attr, _recalcInheritedLinear##Name1##Attr, Flag \
    }


static constexpr struct
{
    const char* tag;
    int sz;
    Linear_Method tagHandler;
    Linear_Inherit_Method tagInheritHandler;
    Linear_Method_Recalc tagRecalc;
    Linear_Method_Recalc tagInheritedRecalc;
    SvgGradientFlags flag;
} linear_tags[] = {
    LINEAR_DEF(x1, X1, SvgGradientFlags::X1),
    LINEAR_DEF(y1, Y1, SvgGradientFlags::Y1),
    LINEAR_DEF(x2, X2, SvgGradientFlags::X2),
    LINEAR_DEF(y2, Y2, SvgGradientFlags::Y2)
};


static bool _attrParseLinearGradientNode(void* data, const char* key, const char* value)
{
    SvgLoaderData* loader = (SvgLoaderData*)data;
    SvgStyleGradient* grad = loader->svgParse->styleGrad;
    SvgLinearGradient* linear = grad->linear;
    int sz = strlen(key);

    for (unsigned int i = 0; i < sizeof(linear_tags) / sizeof(linear_tags[0]); i++) {
        if (linear_tags[i].sz - 1 == sz && !strncmp(linear_tags[i].tag, key, sz)) {
            linear_tags[i].tagHandler(loader, linear, value);
            grad->flags = (grad->flags | linear_tags[i].flag);
            return true;
        }
    }

    if (!strcmp(key, "id")) {
        if (grad->id && value) free(grad->id);
        grad->id = _copyId(value);
    } else if (!strcmp(key, "spreadMethod")) {
        grad->spread = _parseSpreadValue(value);
        grad->flags = (grad->flags | SvgGradientFlags::SpreadMethod);
    } else if (!strcmp(key, "href") || !strcmp(key, "xlink:href")) {
        if (grad->ref && value) free(grad->ref);
        grad->ref = _idFromHref(value);
    } else if (!strcmp(key, "gradientUnits")) {
        if (!strcmp(value, "userSpaceOnUse")) grad->userSpace = true;
        grad->flags = (grad->flags | SvgGradientFlags::GradientUnits);
    } else if (!strcmp(key, "gradientTransform")) {
        grad->transform = _parseTransformationMatrix(value);
    } else {
        return false;
    }

    return true;
}


static SvgStyleGradient* _createLinearGradient(SvgLoaderData* loader, const char* buf, unsigned bufLength)
{
    auto grad = (SvgStyleGradient*)(calloc(1, sizeof(SvgStyleGradient)));
    loader->svgParse->styleGrad = grad;

    grad->flags = SvgGradientFlags::None;
    grad->type = SvgGradientType::Linear;
    grad->userSpace = false;
    grad->linear = (SvgLinearGradient*)calloc(1, sizeof(SvgLinearGradient));
    if (!grad->linear) {
        grad->clear();
        free(grad);
        return nullptr;
    }
    /**
    * Default value of x2 is 100% - transformed to the global percentage
    */
    grad->linear->x2 = 1.0f;
    grad->linear->isX2Percentage = true;

    simpleXmlParseAttributes(buf, bufLength, _attrParseLinearGradientNode, loader);

    for (unsigned int i = 0; i < sizeof(linear_tags) / sizeof(linear_tags[0]); i++) {
        linear_tags[i].tagRecalc(loader, grad->linear, grad->userSpace);
    }

    return loader->svgParse->styleGrad;
}


#define GRADIENT_DEF(Name, Name1)            \
    {                                        \
#Name, sizeof(#Name), _create##Name1         \
    }


/**
 * In the case when the gradients lengths are given as numbers (not percentages)
 * in the current user coordinate system, they are recalculated into percentages
 * related to the canvas width and height.
 */
static constexpr struct
{
    const char* tag;
    int sz;
    GradientFactoryMethod tagHandler;
} gradientTags[] = {
    GRADIENT_DEF(linearGradient, LinearGradient),
    GRADIENT_DEF(radialGradient, RadialGradient)
};


static GradientFactoryMethod _findGradientFactory(const char* name)
{
    int sz = strlen(name);

    for (unsigned int i = 0; i < sizeof(gradientTags) / sizeof(gradientTags[0]); i++) {
        if (gradientTags[i].sz - 1 == sz && !strncmp(gradientTags[i].tag, name, sz)) {
            return gradientTags[i].tagHandler;
        }
    }
    return nullptr;
}


static void _cloneGradStops(Array<Fill::ColorStop>& dst, const Array<Fill::ColorStop>& src)
{
    for (uint32_t i = 0; i < src.count; ++i) {
        dst.push(src[i]);
    }
}


static void _inheritGradient(SvgLoaderData* loader, SvgStyleGradient* to, SvgStyleGradient* from)
{
    if (!to || !from) return;

    if (!(to->flags & SvgGradientFlags::SpreadMethod) && (from->flags & SvgGradientFlags::SpreadMethod)) {
        to->spread = from->spread;
        to->flags = (to->flags | SvgGradientFlags::SpreadMethod);
    }
    bool gradUnitSet = (to->flags & SvgGradientFlags::GradientUnits);
    if (!(to->flags & SvgGradientFlags::GradientUnits) && (from->flags & SvgGradientFlags::GradientUnits)) {
        to->userSpace = from->userSpace;
        to->flags = (to->flags | SvgGradientFlags::GradientUnits);
    }

    if (!to->transform && from->transform) {
        to->transform = (Matrix*)malloc(sizeof(Matrix));
        if (to->transform) memcpy(to->transform, from->transform, sizeof(Matrix));
    }

    if (to->type == SvgGradientType::Linear) {
        for (unsigned int i = 0; i < sizeof(linear_tags) / sizeof(linear_tags[0]); i++) {
            bool coordSet = to->flags & linear_tags[i].flag;
            if (!(to->flags & linear_tags[i].flag) && (from->flags & linear_tags[i].flag)) {
                linear_tags[i].tagInheritHandler(to, from);
            }

            //GradUnits not set directly, coord set
            if (!gradUnitSet && coordSet) {
                linear_tags[i].tagRecalc(loader, to->linear, to->userSpace);
            }
            //GradUnits set, coord not set directly
            if (to->userSpace == from->userSpace) continue;
            if (gradUnitSet && !coordSet) {
                linear_tags[i].tagInheritedRecalc(loader, to->linear, to->userSpace);
            }
        }
    } else if (to->type == SvgGradientType::Radial) {
        for (unsigned int i = 0; i < sizeof(radialTags) / sizeof(radialTags[0]); i++) {
            bool coordSet = (to->flags & radialTags[i].flag);
            if (!(to->flags & radialTags[i].flag) && (from->flags & radialTags[i].flag)) {
                radialTags[i].tagInheritHandler(to, from);
            }

            //GradUnits not set directly, coord set
            if (!gradUnitSet && coordSet) {
                radialTags[i].tagRecalc(loader, to->radial, to->userSpace);
                //If fx and fy are not set, set cx and cy.
                if (!strcmp(radialTags[i].tag, "cx") && !(to->flags & SvgGradientFlags::Fx)) to->radial->fx = to->radial->cx;
                if (!strcmp(radialTags[i].tag, "cy") && !(to->flags & SvgGradientFlags::Fy)) to->radial->fy = to->radial->cy;
            }
            //GradUnits set, coord not set directly
            if (to->userSpace == from->userSpace) continue;
            if (gradUnitSet && !coordSet) {
                //If fx and fx are not set, do not call recalc.
                if (!strcmp(radialTags[i].tag, "fx") && !(to->flags & SvgGradientFlags::Fx)) continue;
                if (!strcmp(radialTags[i].tag, "fy") && !(to->flags & SvgGradientFlags::Fy)) continue;
                radialTags[i].tagInheritedRecalc(loader, to->radial, to->userSpace);
            }
        }
    }

    if (to->stops.empty()) _cloneGradStops(to->stops, from->stops);
}


static SvgStyleGradient* _cloneGradient(SvgStyleGradient* from)
{
    if (!from) return nullptr;

    auto grad = (SvgStyleGradient*)(calloc(1, sizeof(SvgStyleGradient)));
    if (!grad) return nullptr;

    grad->type = from->type;
    grad->id = from->id ? _copyId(from->id) : nullptr;
    grad->ref = from->ref ? _copyId(from->ref) : nullptr;
    grad->spread = from->spread;
    grad->userSpace = from->userSpace;
    grad->flags = from->flags;

    if (from->transform) {
        grad->transform = (Matrix*)calloc(1, sizeof(Matrix));
        if (grad->transform) memcpy(grad->transform, from->transform, sizeof(Matrix));
    }

    if (grad->type == SvgGradientType::Linear) {
        grad->linear = (SvgLinearGradient*)calloc(1, sizeof(SvgLinearGradient));
        if (!grad->linear) goto error_grad_alloc;
        memcpy(grad->linear, from->linear, sizeof(SvgLinearGradient));
    } else if (grad->type == SvgGradientType::Radial) {
        grad->radial = (SvgRadialGradient*)calloc(1, sizeof(SvgRadialGradient));
        if (!grad->radial) goto error_grad_alloc;
        memcpy(grad->radial, from->radial, sizeof(SvgRadialGradient));
    }

    _cloneGradStops(grad->stops, from->stops);

    return grad;

    error_grad_alloc:
    if (grad) {
        grad->clear();
        free(grad);
    }
    return nullptr;
}


static void _styleInherit(SvgStyleProperty* child, const SvgStyleProperty* parent)
{
    if (parent == nullptr) return;
    //Inherit the property of parent if not present in child.
    if (!child->curColorSet) {
        child->color = parent->color;
        child->curColorSet = parent->curColorSet;
    }
    if (!(child->flags & SvgStyleFlags::PaintOrder)) {
        child->paintOrder = parent->paintOrder;
    }
    //Fill
    if (!(child->fill.flags & SvgFillFlags::Paint)) {
        child->fill.paint.color = parent->fill.paint.color;
        child->fill.paint.none = parent->fill.paint.none;
        child->fill.paint.curColor = parent->fill.paint.curColor;
        if (parent->fill.paint.url) {
            if (child->fill.paint.url) free(child->fill.paint.url);
            child->fill.paint.url = _copyId(parent->fill.paint.url);
        }
    }
    if (!(child->fill.flags & SvgFillFlags::Opacity)) {
        child->fill.opacity = parent->fill.opacity;
    }
    if (!(child->fill.flags & SvgFillFlags::FillRule)) {
        child->fill.fillRule = parent->fill.fillRule;
    }
    //Stroke
    if (!(child->stroke.flags & SvgStrokeFlags::Paint)) {
        child->stroke.paint.color = parent->stroke.paint.color;
        child->stroke.paint.none = parent->stroke.paint.none;
        child->stroke.paint.curColor = parent->stroke.paint.curColor;
        if (parent->stroke.paint.url) {
            if (child->stroke.paint.url) free(child->stroke.paint.url);
            child->stroke.paint.url = _copyId(parent->stroke.paint.url);
        }
    }
    if (!(child->stroke.flags & SvgStrokeFlags::Opacity)) {
        child->stroke.opacity = parent->stroke.opacity;
    }
    if (!(child->stroke.flags & SvgStrokeFlags::Width)) {
        child->stroke.width = parent->stroke.width;
    }
    if (!(child->stroke.flags & SvgStrokeFlags::Dash)) {
        if (parent->stroke.dash.array.count > 0) {
            child->stroke.dash.array.clear();
            child->stroke.dash.array.reserve(parent->stroke.dash.array.count);
            for (uint32_t i = 0; i < parent->stroke.dash.array.count; ++i) {
                child->stroke.dash.array.push(parent->stroke.dash.array[i]);
            }
        }
    }
    if (!(child->stroke.flags & SvgStrokeFlags::DashOffset)) {
        child->stroke.dash.offset = parent->stroke.dash.offset;
    }
    if (!(child->stroke.flags & SvgStrokeFlags::Cap)) {
        child->stroke.cap = parent->stroke.cap;
    }
    if (!(child->stroke.flags & SvgStrokeFlags::Join)) {
        child->stroke.join = parent->stroke.join;
    }
    if (!(child->stroke.flags & SvgStrokeFlags::Miterlimit)) {
        child->stroke.miterlimit = parent->stroke.miterlimit;
    }
}


static void _styleCopy(SvgStyleProperty* to, const SvgStyleProperty* from)
{
    if (from == nullptr) return;
    //Copy the properties of 'from' only if they were explicitly set (not the default ones).
    if (from->curColorSet) {
        to->color = from->color;
        to->curColorSet = true;
    }
    if (from->flags & SvgStyleFlags::Opacity) {
        to->opacity = from->opacity;
    }
    if (from->flags & SvgStyleFlags::PaintOrder) {
        to->paintOrder = from->paintOrder;
    }
    if (from->flags & SvgStyleFlags::Display) {
        to->display = from->display;
    }
    //Fill
    to->fill.flags = (to->fill.flags | from->fill.flags);
    if (from->fill.flags & SvgFillFlags::Paint) {
        to->fill.paint.color = from->fill.paint.color;
        to->fill.paint.none = from->fill.paint.none;
        to->fill.paint.curColor = from->fill.paint.curColor;
        if (from->fill.paint.url) {
            if (to->fill.paint.url) free(to->fill.paint.url);
            to->fill.paint.url = _copyId(from->fill.paint.url);
        }
    }
    if (from->fill.flags & SvgFillFlags::Opacity) {
        to->fill.opacity = from->fill.opacity;
    }
    if (from->fill.flags & SvgFillFlags::FillRule) {
        to->fill.fillRule = from->fill.fillRule;
    }
    //Stroke
    to->stroke.flags = (to->stroke.flags | from->stroke.flags);
    if (from->stroke.flags & SvgStrokeFlags::Paint) {
        to->stroke.paint.color = from->stroke.paint.color;
        to->stroke.paint.none = from->stroke.paint.none;
        to->stroke.paint.curColor = from->stroke.paint.curColor;
        if (from->stroke.paint.url) {
            if (to->stroke.paint.url) free(to->stroke.paint.url);
            to->stroke.paint.url = _copyId(from->stroke.paint.url);
        }
    }
    if (from->stroke.flags & SvgStrokeFlags::Opacity) {
        to->stroke.opacity = from->stroke.opacity;
    }
    if (from->stroke.flags & SvgStrokeFlags::Width) {
        to->stroke.width = from->stroke.width;
    }
    if (from->stroke.flags & SvgStrokeFlags::Dash) {
        if (from->stroke.dash.array.count > 0) {
            to->stroke.dash.array.clear();
            to->stroke.dash.array.reserve(from->stroke.dash.array.count);
            for (uint32_t i = 0; i < from->stroke.dash.array.count; ++i) {
                to->stroke.dash.array.push(from->stroke.dash.array[i]);
            }
        }
    }
    if (from->stroke.flags & SvgStrokeFlags::DashOffset) {
        to->stroke.dash.offset = from->stroke.dash.offset;
    }
    if (from->stroke.flags & SvgStrokeFlags::Cap) {
        to->stroke.cap = from->stroke.cap;
    }
    if (from->stroke.flags & SvgStrokeFlags::Join) {
        to->stroke.join = from->stroke.join;
    }
    if (from->stroke.flags & SvgStrokeFlags::Miterlimit) {
        to->stroke.miterlimit = from->stroke.miterlimit;
    }
}


static void _copyAttr(SvgNode* to, const SvgNode* from)
{
    //Copy matrix attribute
    if (from->transform) {
        to->transform = (Matrix*)malloc(sizeof(Matrix));
        if (to->transform) *to->transform = *from->transform;
    }
    //Copy style attribute
    _styleCopy(to->style, from->style);
    to->style->flags = (to->style->flags | from->style->flags);
    if (from->style->clipPath.url) {
        if (to->style->clipPath.url) free(to->style->clipPath.url);
        to->style->clipPath.url = strdup(from->style->clipPath.url);
    }
    if (from->style->mask.url) {
        if (to->style->mask.url) free(to->style->mask.url);
        to->style->mask.url = strdup(from->style->mask.url);
    }

    //Copy node attribute
    switch (from->type) {
        case SvgNodeType::Circle: {
            to->node.circle.cx = from->node.circle.cx;
            to->node.circle.cy = from->node.circle.cy;
            to->node.circle.r = from->node.circle.r;
            break;
        }
        case SvgNodeType::Ellipse: {
            to->node.ellipse.cx = from->node.ellipse.cx;
            to->node.ellipse.cy = from->node.ellipse.cy;
            to->node.ellipse.rx = from->node.ellipse.rx;
            to->node.ellipse.ry = from->node.ellipse.ry;
            break;
        }
        case SvgNodeType::Rect: {
            to->node.rect.x = from->node.rect.x;
            to->node.rect.y = from->node.rect.y;
            to->node.rect.w = from->node.rect.w;
            to->node.rect.h = from->node.rect.h;
            to->node.rect.rx = from->node.rect.rx;
            to->node.rect.ry = from->node.rect.ry;
            to->node.rect.hasRx = from->node.rect.hasRx;
            to->node.rect.hasRy = from->node.rect.hasRy;
            break;
        }
        case SvgNodeType::Line: {
            to->node.line.x1 = from->node.line.x1;
            to->node.line.y1 = from->node.line.y1;
            to->node.line.x2 = from->node.line.x2;
            to->node.line.y2 = from->node.line.y2;
            break;
        }
        case SvgNodeType::Path: {
            if (from->node.path.path) {
                if (to->node.path.path) free(to->node.path.path);
                to->node.path.path = strdup(from->node.path.path);
            }
            break;
        }
        case SvgNodeType::Polygon: {
            if ((to->node.polygon.pts.count = from->node.polygon.pts.count)) {
                to->node.polygon.pts = from->node.polygon.pts;
            }
            break;
        }
        case SvgNodeType::Polyline: {
            if ((to->node.polyline.pts.count = from->node.polyline.pts.count)) {
                to->node.polyline.pts = from->node.polyline.pts;
            }
            break;
        }
        case SvgNodeType::Image: {
            to->node.image.x = from->node.image.x;
            to->node.image.y = from->node.image.y;
            to->node.image.w = from->node.image.w;
            to->node.image.h = from->node.image.h;
            if (from->node.image.href) {
                if (to->node.image.href) free(to->node.image.href);
                to->node.image.href = strdup(from->node.image.href);
            }
            break;
        }
        case SvgNodeType::Use: {
            to->node.use.x = from->node.use.x;
            to->node.use.y = from->node.use.y;
            to->node.use.w = from->node.use.w;
            to->node.use.h = from->node.use.h;
            to->node.use.isWidthSet = from->node.use.isWidthSet;
            to->node.use.isHeightSet = from->node.use.isHeightSet;
            to->node.use.symbol = from->node.use.symbol;
            break;
        }
        case SvgNodeType::Text: {
            to->node.text.x = from->node.text.x;
            to->node.text.y = from->node.text.y;
            to->node.text.fontSize = from->node.text.fontSize;
            if (from->node.text.text) {
                if (to->node.text.text) free(to->node.text.text);
                to->node.text.text = strdup(from->node.text.text);
            }
            if (from->node.text.fontFamily) {
                if (to->node.text.fontFamily) free(to->node.text.fontFamily);
                to->node.text.fontFamily = strdup(from->node.text.fontFamily);
            }
            break;
        }
        default: {
            break;
        }
    }
}


static void _cloneNode(SvgNode* from, SvgNode* parent, int depth)
{
    /* Exception handling: Prevent invalid SVG data input.
       The size is the arbitrary value, we need an experimental size. */
    if (depth == 8192) {
        TVGERR("SVG", "Infinite recursive call - stopped after %d calls! Svg file may be incorrectly formatted.", depth);
        return;
    }

    SvgNode* newNode;
    if (!from || !parent || from == parent) return;

    newNode = _createNode(parent, from->type);
    if (!newNode) return;

    _styleInherit(newNode->style, parent->style);
    _copyAttr(newNode, from);

    auto child = from->child.data;
    for (uint32_t i = 0; i < from->child.count; ++i, ++child) {
        _cloneNode(*child, newNode, depth + 1);
    }
}


static void _clonePostponedNodes(Array<SvgNodeIdPair>* cloneNodes, SvgNode* doc)
{
    for (uint32_t i = 0; i < cloneNodes->count; ++i) {
        auto nodeIdPair = (*cloneNodes)[i];
        auto defs = _getDefsNode(nodeIdPair.node);
        auto nodeFrom = _findNodeById(defs, nodeIdPair.id);
        if (!nodeFrom) nodeFrom = _findNodeById(doc, nodeIdPair.id);
        if (!_findParentById(nodeIdPair.node, nodeIdPair.id, doc)) {
            _cloneNode(nodeFrom, nodeIdPair.node, 0);
            if (nodeFrom && nodeFrom->type == SvgNodeType::Symbol && nodeIdPair.node->type == SvgNodeType::Use) {
                nodeIdPair.node->node.use.symbol = nodeFrom;
            }
        } else {
            TVGLOG("SVG", "%s is ancestor element. This reference is invalid.", nodeIdPair.id);
        }
        free(nodeIdPair.id);
    }
}


static void _svgLoaderParserXmlClose(SvgLoaderData* loader, const char* content, unsigned int length)
{
    const char* itr = nullptr;
    int sz = length;
    char tagName[20] = "";

    content = _skipSpace(content, nullptr);
    itr = content;
    while ((itr != nullptr) && *itr != '>') itr++;

    if (itr) {
        sz = itr - content;
        while ((sz > 0) && (isspace(content[sz - 1]))) sz--;
        if ((unsigned int)sz >= sizeof(tagName)) sz = sizeof(tagName) - 1;
        strncpy(tagName, content, sz);
        tagName[sz] = '\0';
    }
    else return;

    for (unsigned int i = 0; i < sizeof(groupTags) / sizeof(groupTags[0]); i++) {
        if (!strncmp(tagName, groupTags[i].tag, sz)) {
            loader->stack.pop();
            break;
        }
    }

    for (unsigned int i = 0; i < sizeof(graphicsTags) / sizeof(graphicsTags[0]); i++) {
        if (!strncmp(tagName, graphicsTags[i].tag, sz)) {
            loader->currentGraphicsNode = nullptr;
            if (!strncmp(tagName, "text", 4)) loader->openedTag = OpenedTagType::Other;
            loader->stack.pop();
            break;
        }
    }

    loader->level--;
}


static void _svgLoaderParserXmlOpen(SvgLoaderData* loader, const char* content, unsigned int length, bool empty)
{
    const char* attrs = nullptr;
    int attrsLength = 0;
    int sz = length;
    char tagName[20] = "";
    FactoryMethod method;
    GradientFactoryMethod gradientMethod;
    SvgNode *node = nullptr, *parent = nullptr;
    loader->level++;
    attrs = simpleXmlFindAttributesTag(content, length);

    if (!attrs) {
        //Parse the empty tag
        attrs = content;
        while ((attrs != nullptr) && *attrs != '>') attrs++;
        if (empty) attrs--;
    }

    if (attrs) {
        //Find out the tag name starting from content till sz length
        sz = attrs - content;
        while ((sz > 0) && (isspace(content[sz - 1]))) sz--;
        if ((unsigned)sz >= sizeof(tagName)) return;
        strncpy(tagName, content, sz);
        tagName[sz] = '\0';
        attrsLength = length - sz;
    }

    if ((method = _findGroupFactory(tagName))) {
        //Group
        if (empty) return;
        if (!loader->doc) {
            if (strcmp(tagName, "svg")) return; //Not a valid svg document
            node = method(loader, nullptr, attrs, attrsLength, simpleXmlParseAttributes);
            loader->doc = node;
        } else {
            if (!strcmp(tagName, "svg")) return; //Already loaded <svg>(SvgNodeType::Doc) tag
            if (loader->stack.count > 0) parent = loader->stack.last();
            else parent = loader->doc;
            if (!strcmp(tagName, "style")) {
                // TODO: For now only the first style node is saved. After the css id selector
                // is introduced this if condition shouldn't be necessary any more
                if (!loader->cssStyle) {
                    node = method(loader, nullptr, attrs, attrsLength, simpleXmlParseAttributes);
                    loader->cssStyle = node;
                    loader->doc->node.doc.style = node;
                    loader->openedTag = OpenedTagType::Style;
                }
            } else {
                node = method(loader, parent, attrs, attrsLength, simpleXmlParseAttributes);
            }
        }

        if (!node) return;
        if (node->type != SvgNodeType::Defs || !empty) {
            loader->stack.push(node);
        }
    } else if ((method = _findGraphicsFactory(tagName))) {
        if (loader->stack.count > 0) parent = loader->stack.last();
        else parent = loader->doc;
        node = method(loader, parent, attrs, attrsLength, simpleXmlParseAttributes);
        if (node && !empty) {
            if (!strcmp(tagName, "text")) loader->openedTag = OpenedTagType::Text;
            auto defs = _createDefsNode(loader, nullptr, nullptr, 0, nullptr);
            loader->stack.push(defs);
            loader->currentGraphicsNode = node;
        }
    } else if ((gradientMethod = _findGradientFactory(tagName))) {
        SvgStyleGradient* gradient;
        gradient = gradientMethod(loader, attrs, attrsLength);
        //FIXME: The current parsing structure does not distinguish end tags.
        //       There is no way to know if the currently parsed gradient is in defs.
        //       If a gradient is declared outside of defs after defs is set, it is included in the gradients of defs.
        //       But finally, the loader has a gradient style list regardless of defs.
        //       This is only to support this when multiple gradients are declared, even if no defs are declared.
        //       refer to: https://developer.mozilla.org/en-US/docs/Web/SVG/Element/defs
        if (loader->def && loader->doc->node.doc.defs) {
            loader->def->node.defs.gradients.push(gradient);
        } else {
            loader->gradients.push(gradient);
        }
        loader->latestGradient = gradient;
    } else if (!strcmp(tagName, "stop")) {
        if (!loader->latestGradient) {
            TVGLOG("SVG", "Stop element is used outside of the Gradient element");
            return;
        }
        /* default value for opacity */
        loader->svgParse->gradStop = {0.0f, 0, 0, 0, 255};
        loader->svgParse->flags = SvgStopStyleFlags::StopDefault;
        simpleXmlParseAttributes(attrs, attrsLength, _attrParseStops, loader);
        loader->latestGradient->stops.push(loader->svgParse->gradStop);
    } else if (!isIgnoreUnsupportedLogElements(tagName)) {
        TVGLOG("SVG", "Unsupported elements used [Elements: %s]", tagName);
    }
}


static void _svgLoaderParserText(SvgLoaderData* loader, const char* content, unsigned int length)
{
    auto text = &loader->svgParse->node->node.text;
    text->text = strAppend(text->text, content, length);
}


static void _svgLoaderParserXmlCssStyle(SvgLoaderData* loader, const char* content, unsigned int length)
{
    char* tag;
    char* name;
    const char* attrs = nullptr;
    unsigned int attrsLength = 0;

    FactoryMethod method;
    GradientFactoryMethod gradientMethod;
    SvgNode *node = nullptr;

    while (auto next = simpleXmlParseCSSAttribute(content, length, &tag, &name, &attrs, &attrsLength)) {
        if ((method = _findGroupFactory(tag))) {
            if ((node = method(loader, loader->cssStyle, attrs, attrsLength, simpleXmlParseW3CAttribute))) node->id = _copyId(name);
        } else if ((method = _findGraphicsFactory(tag))) {
            if ((node = method(loader, loader->cssStyle, attrs, attrsLength, simpleXmlParseW3CAttribute))) node->id = _copyId(name);
        } else if ((gradientMethod = _findGradientFactory(tag))) {
            TVGLOG("SVG", "Unsupported elements used in the internal CSS style sheets [Elements: %s]", tag);
        } else if (!strcmp(tag, "stop")) {
            TVGLOG("SVG", "Unsupported elements used in the internal CSS style sheets [Elements: %s]", tag);
        } else if (!strcmp(tag, "all")) {
            if ((node = _createCssStyleNode(loader, loader->cssStyle, attrs, attrsLength, simpleXmlParseW3CAttribute))) node->id = _copyId(name);
        } else if (!isIgnoreUnsupportedLogElements(tag)) {
            TVGLOG("SVG", "Unsupported elements used in the internal CSS style sheets [Elements: %s]", tag);
        }

        length -= next - content;
        content = next;

        free(tag);
        free(name);
    }
    loader->openedTag = OpenedTagType::Other;
}


static bool _svgLoaderParser(void* data, SimpleXMLType type, const char* content, unsigned int length)
{
    SvgLoaderData* loader = (SvgLoaderData*)data;

    switch (type) {
        case SimpleXMLType::Open: {
            _svgLoaderParserXmlOpen(loader, content, length, false);
            break;
        }
        case SimpleXMLType::OpenEmpty: {
            _svgLoaderParserXmlOpen(loader, content, length, true);
            break;
        }
        case SimpleXMLType::Close: {
            _svgLoaderParserXmlClose(loader, content, length);
            break;
        }
        case SimpleXMLType::Data:
        case SimpleXMLType::CData: {
            if (loader->openedTag == OpenedTagType::Style) _svgLoaderParserXmlCssStyle(loader, content, length);
            else if (loader->openedTag == OpenedTagType::Text) _svgLoaderParserText(loader, content, length);
            break;
        }
        case SimpleXMLType::DoctypeChild: {
            break;
        }
        case SimpleXMLType::Ignored:
        case SimpleXMLType::Comment:
        case SimpleXMLType::Doctype: {
            break;
        }
        default: {
            break;
        }
    }

    return true;
}


static void _inefficientNodeCheck(TVG_UNUSED SvgNode* node)
{
#ifdef THORVG_LOG_ENABLED
    auto type = simpleXmlNodeTypeToString(node->type);

    if (!node->style->display && node->type != SvgNodeType::ClipPath) TVGLOG("SVG", "Inefficient elements used [Display is none][Node Type : %s]", type);
    if (node->style->opacity == 0) TVGLOG("SVG", "Inefficient elements used [Opacity is zero][Node Type : %s]", type);
    if (node->style->fill.opacity == 0 && node->style->stroke.opacity == 0) TVGLOG("SVG", "Inefficient elements used [Fill opacity and stroke opacity are zero][Node Type : %s]", type);

    switch (node->type) {
        case SvgNodeType::Path: {
            if (!node->node.path.path) TVGLOG("SVG", "Inefficient elements used [Empty path][Node Type : %s]", type);
            break;
        }
        case SvgNodeType::Ellipse: {
            if (node->node.ellipse.rx == 0 && node->node.ellipse.ry == 0) TVGLOG("SVG", "Inefficient elements used [Size is zero][Node Type : %s]", type);
            break;
        }
        case SvgNodeType::Polygon:
        case SvgNodeType::Polyline: {
            if (node->node.polygon.pts.count < 2) TVGLOG("SVG", "Inefficient elements used [Invalid Polygon][Node Type : %s]", type);
            break;
        }
        case SvgNodeType::Circle: {
            if (node->node.circle.r == 0) TVGLOG("SVG", "Inefficient elements used [Size is zero][Node Type : %s]", type);
            break;
        }
        case SvgNodeType::Rect: {
            if (node->node.rect.w == 0 && node->node.rect.h) TVGLOG("SVG", "Inefficient elements used [Size is zero][Node Type : %s]", type);
            break;
        }
        case SvgNodeType::Line: {
            if (node->node.line.x1 == node->node.line.x2 && node->node.line.y1 == node->node.line.y2) TVGLOG("SVG", "Inefficient elements used [Size is zero][Node Type : %s]", type);
            break;
        }
        default: break;
    }
#endif
}


static void _updateStyle(SvgNode* node, SvgStyleProperty* parentStyle)
{
    _styleInherit(node->style, parentStyle);
    _inefficientNodeCheck(node);

    auto child = node->child.data;
    for (uint32_t i = 0; i < node->child.count; ++i, ++child) {
        _updateStyle(*child, node->style);
    }
}


static SvgStyleGradient* _gradientDup(SvgLoaderData* loader, Array<SvgStyleGradient*>* gradients, const char* id)
{
    SvgStyleGradient* result = nullptr;

    auto gradList = gradients->data;

    for (uint32_t i = 0; i < gradients->count; ++i) {
        if ((*gradList)->id && !strcmp((*gradList)->id, id)) {
            result = _cloneGradient(*gradList);
            break;
        }
        ++gradList;
    }

    if (result && result->ref) {
        gradList = gradients->data;
        for (uint32_t i = 0; i < gradients->count; ++i) {
            if ((*gradList)->id && !strcmp((*gradList)->id, result->ref)) {
                _inheritGradient(loader, result, *gradList);
                break;
            }
            ++gradList;
        }
    }

    return result;
}


static void _updateGradient(SvgLoaderData* loader, SvgNode* node, Array<SvgStyleGradient*>* gradients)
{
    if (node->child.count > 0) {
        auto child = node->child.data;
        for (uint32_t i = 0; i < node->child.count; ++i, ++child) {
            _updateGradient(loader, *child, gradients);
        }
    } else {
        if (node->style->fill.paint.url) {
            auto newGrad = _gradientDup(loader, gradients, node->style->fill.paint.url);
            if (newGrad) {
                if (node->style->fill.paint.gradient) {
                    node->style->fill.paint.gradient->clear();
                    free(node->style->fill.paint.gradient);
                }
                node->style->fill.paint.gradient = newGrad;
            }
        }
        if (node->style->stroke.paint.url) {
            auto newGrad = _gradientDup(loader, gradients, node->style->stroke.paint.url);
            if (newGrad) {
                if (node->style->stroke.paint.gradient) {
                    node->style->stroke.paint.gradient->clear();
                    free(node->style->stroke.paint.gradient);
                }
                node->style->stroke.paint.gradient = newGrad;
            }
        }
    }
}


static void _updateComposite(SvgNode* node, SvgNode* root)
{
    if (node->style->clipPath.url && !node->style->clipPath.node) {
        SvgNode* findResult = _findNodeById(root, node->style->clipPath.url);
        if (findResult) node->style->clipPath.node = findResult;
    }
    if (node->style->mask.url && !node->style->mask.node) {
        SvgNode* findResult = _findNodeById(root, node->style->mask.url);
        if (findResult) node->style->mask.node = findResult;
    }
    if (node->child.count > 0) {
        auto child = node->child.data;
        for (uint32_t i = 0; i < node->child.count; ++i, ++child) {
            _updateComposite(*child, root);
        }
    }
}


static void _freeNodeStyle(SvgStyleProperty* style)
{
    if (!style) return;

    //style->clipPath.node and style->mask.node has only the addresses of node. Therefore, node is released from _freeNode.
    free(style->clipPath.url);
    free(style->mask.url);
    free(style->cssClass);

    if (style->fill.paint.gradient) {
        style->fill.paint.gradient->clear();
        free(style->fill.paint.gradient);
    }
    if (style->stroke.paint.gradient) {
        style->stroke.paint.gradient->clear();
        free(style->stroke.paint.gradient);
    }
    free(style->fill.paint.url);
    free(style->stroke.paint.url);
    style->stroke.dash.array.reset();
    free(style);
}


static void _freeNode(SvgNode* node)
{
    if (!node) return;

    auto child = node->child.data;
    for (uint32_t i = 0; i < node->child.count; ++i, ++child) {
        _freeNode(*child);
    }
    node->child.reset();

    free(node->id);
    free(node->transform);
    _freeNodeStyle(node->style);
    switch (node->type) {
         case SvgNodeType::Path: {
             free(node->node.path.path);
             break;
         }
         case SvgNodeType::Polygon: {
             free(node->node.polygon.pts.data);
             break;
         }
         case SvgNodeType::Polyline: {
             free(node->node.polyline.pts.data);
             break;
         }
         case SvgNodeType::Doc: {
             _freeNode(node->node.doc.defs);
             _freeNode(node->node.doc.style);
             break;
         }
         case SvgNodeType::Defs: {
             auto gradients = node->node.defs.gradients.data;
             for (size_t i = 0; i < node->node.defs.gradients.count; ++i) {
                 (*gradients)->clear();
                 free(*gradients);
                 ++gradients;
             }
             node->node.defs.gradients.reset();
             break;
         }
         case SvgNodeType::Image: {
             free(node->node.image.href);
             break;
         }
         case SvgNodeType::Text: {
             free(node->node.text.text);
             free(node->node.text.fontFamily);
             break;
         }
         default: {
             break;
         }
    }
    free(node);
}


static bool _svgLoaderParserForValidCheckXmlOpen(SvgLoaderData* loader, const char* content, unsigned int length)
{
    const char* attrs = nullptr;
    int sz = length;
    char tagName[20] = "";
    FactoryMethod method;
    SvgNode *node = nullptr;
    int attrsLength = 0;
    loader->level++;
    attrs = simpleXmlFindAttributesTag(content, length);

    if (!attrs) {
        //Parse the empty tag
        attrs = content;
        while ((attrs != nullptr) && *attrs != '>') attrs++;
    }

    if (attrs) {
        sz = attrs - content;
        while ((sz > 0) && (isspace(content[sz - 1]))) sz--;
        if ((unsigned)sz >= sizeof(tagName)) return false;
        strncpy(tagName, content, sz);
        tagName[sz] = '\0';
        attrsLength = length - sz;
    }

    if ((method = _findGroupFactory(tagName))) {
        if (!loader->doc) {
            if (strcmp(tagName, "svg")) return true; //Not a valid svg document
            node = method(loader, nullptr, attrs, attrsLength, simpleXmlParseAttributes);
            loader->doc = node;
            loader->stack.push(node);
            return false;
        }
    }
    return true;
}


static bool _svgLoaderParserForValidCheck(void* data, SimpleXMLType type, const char* content, unsigned int length)
{
    SvgLoaderData* loader = (SvgLoaderData*)data;
    bool res = true;;

    switch (type) {
        case SimpleXMLType::Open:
        case SimpleXMLType::OpenEmpty: {
            //If 'res' is false, it means <svg> tag is found.
            res = _svgLoaderParserForValidCheckXmlOpen(loader, content, length);
            break;
        }
        default: {
            break;
        }
    }

    return res;
}


void SvgLoader::clear(bool all)
{
    //flush out the intermediate data
    free(loaderData.svgParse);
    loaderData.svgParse = nullptr;

    for (auto gradient = loaderData.gradients.begin(); gradient < loaderData.gradients.end(); ++gradient) {
        (*gradient)->clear();
        free(*gradient);
    }
    loaderData.gradients.reset();

    _freeNode(loaderData.doc);
    loaderData.doc = nullptr;
    loaderData.stack.reset();

    if (!all) return;

    for (auto p = loaderData.images.begin(); p < loaderData.images.end(); ++p) {
        free(*p);
    }
    loaderData.images.reset();

    if (copy) free((char*)content);

    delete(root);
    root = nullptr;

    size = 0;
    content = nullptr;
    copy = false;
}


/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/

SvgLoader::SvgLoader() : ImageLoader(FileType::Svg)
{
}


SvgLoader::~SvgLoader()
{
    this->done();
    clear();
}


void SvgLoader::run(unsigned tid)
{
    //According to the SVG standard the value of the width/height of the viewbox set to 0 disables rendering
    if ((viewFlag & SvgViewFlag::Viewbox) && (fabsf(vw) <= FLOAT_EPSILON || fabsf(vh) <= FLOAT_EPSILON)) {
        TVGLOG("SVG", "The <viewBox> width and/or height set to 0 - rendering disabled.");
        root = Scene::gen().release();
        return;
    }

    if (!simpleXmlParse(content, size, true, _svgLoaderParser, &(loaderData))) return;

    if (loaderData.doc) {
        auto defs = loaderData.doc->node.doc.defs;

        if (loaderData.nodesToStyle.count > 0) cssApplyStyleToPostponeds(loaderData.nodesToStyle, loaderData.cssStyle);
        if (loaderData.cssStyle) cssUpdateStyle(loaderData.doc, loaderData.cssStyle);

        if (loaderData.cloneNodes.count > 0) _clonePostponedNodes(&loaderData.cloneNodes, loaderData.doc);

        _updateComposite(loaderData.doc, loaderData.doc);
        if (defs) _updateComposite(loaderData.doc, defs);

        _updateStyle(loaderData.doc, nullptr);
        if (defs) _updateStyle(defs, nullptr);

        if (loaderData.gradients.count > 0) _updateGradient(&loaderData, loaderData.doc, &loaderData.gradients);
        if (defs) _updateGradient(&loaderData, loaderData.doc, &defs->node.defs.gradients);
    }
    root = svgSceneBuild(loaderData, {vx, vy, vw, vh}, w, h, align, meetOrSlice, svgPath, viewFlag);

    //In case no viewbox and width/height data is provided the completion of loading
    //has to be forced, in order to establish this data based on the whole picture.
    if (!(viewFlag & SvgViewFlag::Viewbox)) {
        //Override viewbox & size again after svg loading.
        vx = loaderData.doc->node.doc.vx;
        vy = loaderData.doc->node.doc.vy;
        vw = loaderData.doc->node.doc.vw;
        vh = loaderData.doc->node.doc.vh;
        w = loaderData.doc->node.doc.w;
        h = loaderData.doc->node.doc.h;
    }

    clear(false);
}


bool SvgLoader::header()
{
    //For valid check, only <svg> tag is parsed first.
    //If the <svg> tag is found, the loaded file is valid and stores viewbox information.
    //After that, the remaining content data is parsed in order with async.
    loaderData.svgParse = (SvgParser*)malloc(sizeof(SvgParser));
    if (!loaderData.svgParse) return false;

    loaderData.svgParse->flags = SvgStopStyleFlags::StopDefault;
    viewFlag = SvgViewFlag::None;

    simpleXmlParse(content, size, true, _svgLoaderParserForValidCheck, &(loaderData));

    if (loaderData.doc && loaderData.doc->type == SvgNodeType::Doc) {
        viewFlag = loaderData.doc->node.doc.viewFlag;
        align = loaderData.doc->node.doc.align;
        meetOrSlice = loaderData.doc->node.doc.meetOrSlice;

        if (viewFlag & SvgViewFlag::Viewbox) {
            vx = loaderData.doc->node.doc.vx;
            vy = loaderData.doc->node.doc.vy;
            vw = loaderData.doc->node.doc.vw;
            vh = loaderData.doc->node.doc.vh;

            if (viewFlag & SvgViewFlag::Width) w = loaderData.doc->node.doc.w;
            else {
                w = loaderData.doc->node.doc.vw;
                if (viewFlag & SvgViewFlag::WidthInPercent) {
                    w *= loaderData.doc->node.doc.w;
                    viewFlag = (viewFlag ^ SvgViewFlag::WidthInPercent);
                }
                viewFlag = (viewFlag | SvgViewFlag::Width);
            }
            if (viewFlag & SvgViewFlag::Height) h = loaderData.doc->node.doc.h;
            else {
                h = loaderData.doc->node.doc.vh;
                if (viewFlag & SvgViewFlag::HeightInPercent) {
                    h *= loaderData.doc->node.doc.h;
                    viewFlag = (viewFlag ^ SvgViewFlag::HeightInPercent);
                }
                viewFlag = (viewFlag | SvgViewFlag::Height);
            }
        //In case no viewbox and width/height data is provided the completion of loading
        //has to be forced, in order to establish this data based on the whole picture.
        } else {
            //Before loading, set default viewbox & size if they are empty
            vx = vy = 0.0f;
            if (viewFlag & SvgViewFlag::Width) {
                vw = w = loaderData.doc->node.doc.w;
            } else {
                vw = 1.0f;
                if (viewFlag & SvgViewFlag::WidthInPercent) {
                    w = loaderData.doc->node.doc.w;
                } else w = 1.0f;
            }

            if (viewFlag & SvgViewFlag::Height) {
                vh = h = loaderData.doc->node.doc.h;
            } else {
                vh = 1.0f;
                if (viewFlag & SvgViewFlag::HeightInPercent) {
                    h = loaderData.doc->node.doc.h;
                } else h = 1.0f;
            }

            run(0);
        }

        return true;
    }

    TVGLOG("SVG", "No SVG File. There is no <svg/>");
    return false;
}


bool SvgLoader::open(const char* data, uint32_t size, bool copy)
{
    clear();

    if (copy) {
        content = (char*)malloc(size + 1);
        if (!content) return false;
        memcpy((char*)content, data, size);
        content[size] = '\0';
    } else content = (char*)data;

    this->size = size;
    this->copy = copy;

    return header();
}


bool SvgLoader::open(const string& path)
{
    clear();

    ifstream f;
    f.open(path);

    if (!f.is_open()) return false;

    svgPath = path;
    getline(f, filePath, '\0');
    f.close();

    if (filePath.empty()) return false;

    content = (char*)filePath.c_str();
    size = filePath.size();

    return header();
}


bool SvgLoader::resize(Paint* paint, float w, float h)
{
    if (!paint) return false;

    auto sx = w / this->w;
    auto sy = h / this->h;
    Matrix m = {sx, 0, 0, 0, sy, 0, 0, 0, 1};
    paint->transform(m);

    return true;
}


bool SvgLoader::read()
{
    if (!content || size == 0) return false;

    //the loading has been already completed in header()
    if (root || !LoadModule::read()) return true;

    TaskScheduler::request(this);

    return true;
}


bool SvgLoader::close()
{
    if (!LoadModule::close()) return false;
    this->done();
    clear();
    return true;
}


Paint* SvgLoader::paint()
{
    this->done();
    auto ret = root;
    root = nullptr;
    return ret;
}

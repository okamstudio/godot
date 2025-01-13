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

#include <cstring>
#include <ctype.h>
#include <string>

#ifdef _WIN32
    #include <malloc.h>
#elif defined(__linux__)
    #include <alloca.h>
#else
    #include <stdlib.h>
#endif

#include "tvgXmlParser.h"
#include "tvgStr.h"

/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/

bool _isIgnoreUnsupportedLogAttributes(TVG_UNUSED const char* tagAttribute, TVG_UNUSED const char* tagValue)
{
#ifdef THORVG_LOG_ENABLED
    const auto attributesNum = 6;
    const struct
    {
        const char* tag;
        bool tagWildcard; //If true, it is assumed that a wildcard is used after the tag. (ex: tagName*)
        const char* value;
    } attributes[] = {
        {"id", false, nullptr},
        {"data-name", false, nullptr},
        {"overflow", false, "visible"},
        {"version", false, nullptr},
        {"xmlns", true, nullptr},
        {"xml:space", false, nullptr},
    };

    for (unsigned int i = 0; i < attributesNum; ++i) {
        if (!strncmp(tagAttribute, attributes[i].tag, attributes[i].tagWildcard ? strlen(attributes[i].tag) : strlen(tagAttribute))) {
            if (attributes[i].value && tagValue) {
                if (!strncmp(tagValue, attributes[i].value, strlen(tagValue))) {
                    return true;
                } else continue;
            }
            return true;
        }
    }
    return false;
#endif
    return true;
}


static const char* _simpleXmlFindWhiteSpace(const char* itr, const char* itrEnd)
{
    for (; itr < itrEnd; itr++) {
        if (isspace((unsigned char)*itr)) break;
    }
    return itr;
}


static const char* _simpleXmlSkipWhiteSpace(const char* itr, const char* itrEnd)
{
    for (; itr < itrEnd; itr++) {
        if (!isspace((unsigned char)*itr)) break;
    }
    return itr;
}


static const char* _simpleXmlUnskipWhiteSpace(const char* itr, const char* itrStart)
{
    for (itr--; itr > itrStart; itr--) {
        if (!isspace((unsigned char)*itr)) break;
    }
    return itr + 1;
}


static const char* _simpleXmlSkipXmlEntities(const char* itr, const char* itrEnd)
{
    auto p = itr;
    while (itr < itrEnd && *itr == '&') {
        for (int i = 0; i < NUMBER_OF_XML_ENTITIES; ++i) {
            if (strncmp(itr, xmlEntity[i], xmlEntityLength[i]) == 0) {
                itr += xmlEntityLength[i];
                break;
            }
        }
        if (itr == p) break;
        p = itr;
    }
    return itr;
}


static const char* _simpleXmlUnskipXmlEntities(const char* itr, const char* itrStart)
{
    auto p = itr;
    while (itr > itrStart && *(itr - 1) == ';') {
        for (int i = 0; i < NUMBER_OF_XML_ENTITIES; ++i) {
            if (itr - xmlEntityLength[i] > itrStart &&
                strncmp(itr - xmlEntityLength[i], xmlEntity[i], xmlEntityLength[i]) == 0) {
                itr -= xmlEntityLength[i];
                break;
            }
        }
        if (itr == p) break;
        p = itr;
    }
    return itr;
}


static const char* _skipWhiteSpacesAndXmlEntities(const char* itr, const char* itrEnd)
{
    itr = _simpleXmlSkipWhiteSpace(itr, itrEnd);
    auto p = itr;
    while (true) {
        if (p != (itr = _simpleXmlSkipXmlEntities(itr, itrEnd))) p = itr;
        else break;
        if (p != (itr = _simpleXmlSkipWhiteSpace(itr, itrEnd))) p = itr;
        else break;
    }
    return itr;
}


static const char* _unskipWhiteSpacesAndXmlEntities(const char* itr, const char* itrStart)
{
    itr = _simpleXmlUnskipWhiteSpace(itr, itrStart);
    auto p = itr;
    while (true) {
        if (p != (itr = _simpleXmlUnskipXmlEntities(itr, itrStart))) p = itr;
        else break;
        if (p != (itr = _simpleXmlUnskipWhiteSpace(itr, itrStart))) p = itr;
        else break;
    }
    return itr;
}


static const char* _simpleXmlFindStartTag(const char* itr, const char* itrEnd)
{
    return (const char*)memchr(itr, '<', itrEnd - itr);
}


static const char* _simpleXmlFindEndTag(const char* itr, const char* itrEnd)
{
    bool insideQuote[2] = {false, false}; // 0: ", 1: '
    for (; itr < itrEnd; itr++) {
        if (*itr == '"' && !insideQuote[1]) insideQuote[0] = !insideQuote[0];
        if (*itr == '\'' && !insideQuote[0]) insideQuote[1] = !insideQuote[1];
        if (!insideQuote[0] && !insideQuote[1]) {
            if ((*itr == '>') || (*itr == '<'))
                return itr;
        }
    }
    return nullptr;
}


static const char* _simpleXmlFindEndCommentTag(const char* itr, const char* itrEnd)
{
    for (; itr < itrEnd; itr++) {
        if ((*itr == '-') && ((itr + 1 < itrEnd) && (*(itr + 1) == '-')) && ((itr + 2 < itrEnd) && (*(itr + 2) == '>'))) return itr + 2;
    }
    return nullptr;
}


static const char* _simpleXmlFindEndCdataTag(const char* itr, const char* itrEnd)
{
    for (; itr < itrEnd; itr++) {
        if ((*itr == ']') && ((itr + 1 < itrEnd) && (*(itr + 1) == ']')) && ((itr + 2 < itrEnd) && (*(itr + 2) == '>'))) return itr + 2;
    }
    return nullptr;
}


static const char* _simpleXmlFindDoctypeChildEndTag(const char* itr, const char* itrEnd)
{
    for (; itr < itrEnd; itr++) {
        if (*itr == '>') return itr;
    }
    return nullptr;
}


static SimpleXMLType _getXMLType(const char* itr, const char* itrEnd, size_t &toff)
{
    toff = 0;
    if (itr[1] == '/') {
        toff = 1;
        return SimpleXMLType::Close;
    } else if (itr[1] == '?') {
        toff = 1;
        return SimpleXMLType::Processing;
    } else if (itr[1] == '!') {
        if ((itr + sizeof("<!DOCTYPE>") - 1 < itrEnd) && (!memcmp(itr + 2, "DOCTYPE", sizeof("DOCTYPE") - 1)) && ((itr[2 + sizeof("DOCTYPE") - 1] == '>') || (isspace((unsigned char)itr[2 + sizeof("DOCTYPE") - 1])))) {
            toff = sizeof("!DOCTYPE") - 1;
            return SimpleXMLType::Doctype;
        } else if ((itr + sizeof("<![CDATA[]]>") - 1 < itrEnd) && (!memcmp(itr + 2, "[CDATA[", sizeof("[CDATA[") - 1))) {
            toff = sizeof("![CDATA[") - 1;
            return SimpleXMLType::CData;
        } else if ((itr + sizeof("<!---->") - 1 < itrEnd) && (!memcmp(itr + 2, "--", sizeof("--") - 1))) {
            toff = sizeof("!--") - 1;
            return SimpleXMLType::Comment;
        } else if (itr + sizeof("<!>") - 1 < itrEnd) {
            toff = sizeof("!") - 1;
            return SimpleXMLType::DoctypeChild;
        }
        return SimpleXMLType::Open;
    }
    return SimpleXMLType::Open;
}


/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/

const char* simpleXmlNodeTypeToString(TVG_UNUSED SvgNodeType type)
{
#ifdef THORVG_LOG_ENABLED
    static const char* TYPE_NAMES[] = {
        "Svg",
        "G",
        "Defs",
        "Animation",
        "Arc",
        "Circle",
        "Ellipse",
        "Image",
        "Line",
        "Path",
        "Polygon",
        "Polyline",
        "Rect",
        "Text",
        "TextArea",
        "Tspan",
        "Use",
        "Video",
        "ClipPath",
        "Mask",
        "Symbol",
        "Unknown",
    };
    return TYPE_NAMES[(int) type];
#endif
    return nullptr;
}


bool isIgnoreUnsupportedLogElements(TVG_UNUSED const char* tagName)
{
#ifdef THORVG_LOG_ENABLED
    const auto elementsNum = 1;
    const char* const elements[] = { "title" };

    for (unsigned int i = 0; i < elementsNum; ++i) {
        if (!strncmp(tagName, elements[i], strlen(tagName))) {
            return true;
        }
    }
    return false;
#else
    return true;
#endif
}


bool simpleXmlParseAttributes(const char* buf, unsigned bufLength, simpleXMLAttributeCb func, const void* data)
{
    const char *itr = buf, *itrEnd = buf + bufLength;
    char* tmpBuf = (char*)malloc(bufLength + 1);

    if (!buf || !func || !tmpBuf) goto error;

    while (itr < itrEnd) {
        const char* p = _skipWhiteSpacesAndXmlEntities(itr, itrEnd);
        const char *key, *keyEnd, *value, *valueEnd;
        char* tval;

        if (p == itrEnd) goto success;

        key = p;
        for (keyEnd = key; keyEnd < itrEnd; keyEnd++) {
            if ((*keyEnd == '=') || (isspace((unsigned char)*keyEnd))) break;
        }
        if (keyEnd == itrEnd) goto error;
        if (keyEnd == key) {  // There is no key. This case is invalid, but explores the following syntax.
            itr = keyEnd + 1;
            continue;
        }

        if (*keyEnd == '=') value = keyEnd + 1;
        else {
            value = (const char*)memchr(keyEnd, '=', itrEnd - keyEnd);
            if (!value) goto error;
            value++;
        }
        keyEnd = _simpleXmlUnskipXmlEntities(keyEnd, key);

        value = _skipWhiteSpacesAndXmlEntities(value, itrEnd);
        if (value == itrEnd) goto error;

        if ((*value == '"') || (*value == '\'')) {
            valueEnd = (const char*)memchr(value + 1, *value, itrEnd - value);
            if (!valueEnd) goto error;
            value++;
        } else {
            valueEnd = _simpleXmlFindWhiteSpace(value, itrEnd);
        }

        itr = valueEnd + 1;

        value = _skipWhiteSpacesAndXmlEntities(value, itrEnd);
        valueEnd = _unskipWhiteSpacesAndXmlEntities(valueEnd, value);

        memcpy(tmpBuf, key, keyEnd - key);
        tmpBuf[keyEnd - key] = '\0';

        tval = tmpBuf + (keyEnd - key) + 1;
        int i = 0;
        while (value < valueEnd) {
            value = _simpleXmlSkipXmlEntities(value, valueEnd);
            tval[i++] = *value;
            value++;
        }
        tval[i] = '\0';

        if (!func((void*)data, tmpBuf, tval)) {
            if (!_isIgnoreUnsupportedLogAttributes(tmpBuf, tval)) {
                TVGLOG("SVG", "Unsupported attributes used [Elements type: %s][Id : %s][Attribute: %s][Value: %s]", simpleXmlNodeTypeToString(((SvgLoaderData*)data)->svgParse->node->type), ((SvgLoaderData*)data)->svgParse->node->id ? ((SvgLoaderData*)data)->svgParse->node->id : "NO_ID", tmpBuf, tval ? tval : "NONE");
            }
        }
    }

success:
    free(tmpBuf);
    return true;

error:
    free(tmpBuf);
    return false;
}


bool simpleXmlParse(const char* buf, unsigned bufLength, bool strip, simpleXMLCb func, const void* data)
{
    const char *itr = buf, *itrEnd = buf + bufLength;

    if (!buf || !func) return false;

    while (itr < itrEnd) {
        if (itr[0] == '<') {
            //Invalid case
            if (itr + 1 >= itrEnd) return false;

            size_t toff = 0;
            SimpleXMLType type = _getXMLType(itr, itrEnd, toff);

            const char* p;
            if (type == SimpleXMLType::CData) p = _simpleXmlFindEndCdataTag(itr + 1 + toff, itrEnd);
            else if (type == SimpleXMLType::DoctypeChild) p = _simpleXmlFindDoctypeChildEndTag(itr + 1 + toff, itrEnd);
            else if (type == SimpleXMLType::Comment) p = _simpleXmlFindEndCommentTag(itr + 1 + toff, itrEnd);
            else p = _simpleXmlFindEndTag(itr + 1 + toff, itrEnd);

            if (p) {
                //Invalid case: '<' nested
                if (*p == '<' && type != SimpleXMLType::Doctype) return false;
                const char *start, *end;

                start = itr + 1 + toff;
                end = p;

                switch (type) {
                    case SimpleXMLType::Open: {
                        if (p[-1] == '/') {
                            type = SimpleXMLType::OpenEmpty;
                            end--;
                        }
                        break;
                    }
                    case SimpleXMLType::CData: {
                        if (!memcmp(p - 2, "]]", 2)) end -= 2;
                        break;
                    }
                    case SimpleXMLType::Processing: {
                        if (p[-1] == '?') end--;
                        break;
                    }
                    case SimpleXMLType::Comment: {
                        if (!memcmp(p - 2, "--", 2)) end -= 2;
                        break;
                    }
                    default: {
                        break;
                    }
                }

                if (strip && (type != SimpleXMLType::CData)) {
                    start = _skipWhiteSpacesAndXmlEntities(start, end);
                    end = _unskipWhiteSpacesAndXmlEntities(end, start);
                }

                if (!func((void*)data, type, start, (unsigned int)(end - start))) return false;

                itr = p + 1;
            } else {
                return false;
            }
        } else {
            const char *p, *end;

            if (strip) {
                p = itr;
                p = _skipWhiteSpacesAndXmlEntities(p, itrEnd);
                if (p) {
                    if (!func((void*)data, SimpleXMLType::Ignored, itr, (unsigned int)(p - itr))) return false;
                    itr = p;
                }
            }

            p = _simpleXmlFindStartTag(itr, itrEnd);
            if (!p) p = itrEnd;

            end = p;
            if (strip) end = _unskipWhiteSpacesAndXmlEntities(end, itr);

            if (itr != end && !func((void*)data, SimpleXMLType::Data, itr, (unsigned int)(end - itr))) return false;

            if (strip && (end < p) && !func((void*)data, SimpleXMLType::Ignored, end, (unsigned int)(p - end))) return false;

            itr = p;
        }
    }
    return true;
}


bool simpleXmlParseW3CAttribute(const char* buf, unsigned bufLength, simpleXMLAttributeCb func, const void* data)
{
    const char* end;
    char* key;
    char* val;
    char* next;

    if (!buf) return false;

    end = buf + bufLength;
    key = (char*)alloca(end - buf + 1);
    val = (char*)alloca(end - buf + 1);

    if (buf == end) return true;

    do {
        char* sep = (char*)strchr(buf, ':');
        next = (char*)strchr(buf, ';');
        if (sep >= end) {
            next = nullptr;
            sep = nullptr;
        }
        if (next >= end) next = nullptr;

        key[0] = '\0';
        val[0] = '\0';

        if (sep != nullptr && next == nullptr) {
            memcpy(key, buf, sep - buf);
            key[sep - buf] = '\0';

            memcpy(val, sep + 1, end - sep - 1);
            val[end - sep - 1] = '\0';
        } else if (sep != nullptr && sep < next) {
            memcpy(key, buf, sep - buf);
            key[sep - buf] = '\0';

            memcpy(val, sep + 1, next - sep - 1);
            val[next - sep - 1] = '\0';
        } else if (next) {
            memcpy(key, buf, next - buf);
            key[next - buf] = '\0';
        }

        if (key[0]) {
            key = const_cast<char*>(_simpleXmlSkipWhiteSpace(key, key + strlen(key)));
            key[_simpleXmlUnskipWhiteSpace(key + strlen(key) , key) - key] = '\0';
            val = const_cast<char*>(_simpleXmlSkipWhiteSpace(val, val + strlen(val)));
            val[_simpleXmlUnskipWhiteSpace(val + strlen(val) , val) - val] = '\0';

            if (!func((void*)data, key, val)) {
                if (!_isIgnoreUnsupportedLogAttributes(key, val)) {
                    TVGLOG("SVG", "Unsupported attributes used [Elements type: %s][Id : %s][Attribute: %s][Value: %s]", simpleXmlNodeTypeToString(((SvgLoaderData*)data)->svgParse->node->type), ((SvgLoaderData*)data)->svgParse->node->id ? ((SvgLoaderData*)data)->svgParse->node->id : "NO_ID", key, val ? val : "NONE");
                }
            }
        }

        if (!next) break;
        buf = next + 1;
    } while (true);

    return true;
}


/*
 * Supported formats:
 * tag {}, .name {}, tag.name{}
 */
const char* simpleXmlParseCSSAttribute(const char* buf, unsigned bufLength, char** tag, char** name, const char** attrs, unsigned* attrsLength)
{
    if (!buf) return nullptr;

    *tag = *name = nullptr;
    *attrsLength = 0;

    auto itr = _simpleXmlSkipWhiteSpace(buf, buf + bufLength);
    auto itrEnd = (const char*)memchr(buf, '{', bufLength);

    if (!itrEnd || itr == itrEnd) return nullptr;

    auto nextElement = (const char*)memchr(itrEnd, '}', bufLength - (itrEnd - buf));
    if (!nextElement) return nullptr;

    *attrs = itrEnd + 1;
    *attrsLength = nextElement - *attrs;

    const char *p;

    itrEnd = _simpleXmlUnskipWhiteSpace(itrEnd, itr);
    if (*(itrEnd - 1) == '.') return nullptr;

    for (p = itr; p < itrEnd; p++) {
        if (*p == '.') break;
    }

    if (p == itr) *tag = strdup("all");
    else *tag = strDuplicate(itr, p - itr);

    if (p == itrEnd) *name = nullptr;
    else *name = strDuplicate(p + 1, itrEnd - p - 1);

    return (nextElement ? nextElement + 1 : nullptr);
}


const char* simpleXmlFindAttributesTag(const char* buf, unsigned bufLength)
{
    const char *itr = buf, *itrEnd = buf + bufLength;

    for (; itr < itrEnd; itr++) {
        if (!isspace((unsigned char)*itr)) {
            //User skip tagname and already gave it the attributes.
            if (*itr == '=') return buf;
        } else {
            itr = _simpleXmlUnskipXmlEntities(itr, buf);
            if (itr == itrEnd) return nullptr;
            return itr;
        }
    }

    return nullptr;
}

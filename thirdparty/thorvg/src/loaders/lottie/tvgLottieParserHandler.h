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

/*
 * Copyright (c) 2020 Samsung Electronics Co., Ltd. All rights reserved.

 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:

 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.

 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef _TVG_LOTTIE_PARSER_HANDLER_H_
#define _TVG_LOTTIE_PARSER_HANDLER_H_

#include "rapidjson/document.h"
#include "tvgCommon.h"


using namespace rapidjson;


struct LookaheadParserHandler
{
    enum LookaheadParsingState {
        kInit = 0,
        kError,
        kHasNull,
        kHasBool,
        kHasNumber,
        kHasString,
        kHasKey,
        kEnteringObject,
        kExitingObject,
        kEnteringArray,
        kExitingArray
    };

    Value                   val;
    LookaheadParsingState   state = kInit;
    Reader                  reader;
    InsituStringStream      iss;

    LookaheadParserHandler(const char *str) : iss((char*)str)
    {
        reader.IterativeParseInit();
    }

    bool Null()
    {
        state = kHasNull;
        val.SetNull();
        return true;
    }

    bool Bool(bool b)
    {
        state = kHasBool;
        val.SetBool(b);
        return true;
    }

    bool Int(int i)
    {
        state = kHasNumber;
        val.SetInt(i);
        return true;
    }

    bool Uint(unsigned u)
    {
        state = kHasNumber;
        val.SetUint(u);
        return true;
    }

    bool Int64(int64_t i)
    {
        state = kHasNumber;
        val.SetInt64(i);
        return true;
    }

    bool Uint64(int64_t u)
    {
        state = kHasNumber;
        val.SetUint64(u);
        return true;
    }

    bool Double(double d)
    {
        state = kHasNumber;
        val.SetDouble(d);
        return true;
    }

    bool RawNumber(const char *, SizeType, TVG_UNUSED bool)
    { 
        return false;
    }

    bool String(const char *str, SizeType length, TVG_UNUSED bool)
    {
        state = kHasString;
        val.SetString(str, length);
        return true;
    }

    bool StartObject()
    {
        state = kEnteringObject;
        return true;
    }

    bool Key(const char *str, SizeType length, TVG_UNUSED bool)
    {
        state = kHasKey;
        val.SetString(str, length);
        return true;
    }

    bool EndObject(SizeType)
    {
        state = kExitingObject;
        return true;
    }

    bool StartArray()
    {
        state = kEnteringArray;
        return true;
    }

    bool EndArray(SizeType)
    {
        state = kExitingArray;
        return true;
    }

    void Error()
    {
        TVGERR("LOTTIE", "Parsing Error!");
        state = kError;
    }

    bool Invalid()
    {
        return state == kError;
    }

    bool enterObject();
    bool enterArray();
    bool nextArrayValue();
    int getInt();
    float getFloat();
    const char* getString();
    char* getStringCopy();
    bool getBool();
    void getNull();
    bool parseNext();
    const char* nextObjectKey();
    void skip(const char* key);
    void skipOut(int depth);
    int peekType();
    char* getPos();
};

#endif //_TVG_LOTTIE_PARSER_HANDLER_H_

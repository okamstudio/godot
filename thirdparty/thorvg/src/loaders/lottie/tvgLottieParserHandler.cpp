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

#include "tvgLottieParserHandler.h"


/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/

static const int PARSE_FLAGS = kParseDefaultFlags | kParseInsituFlag;


/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/


bool LookaheadParserHandler::enterArray()
{
    if (state != kEnteringArray) {
        Error();
        return false;
    }
    parseNext();
    return true;
}


bool LookaheadParserHandler::nextArrayValue()
{
    if (state == kExitingArray) {
        parseNext();
        return false;
    }
    //SPECIAL CASE: same as nextObjectKey()
    if (state == kExitingObject) return false;
    if (state == kError || state == kHasKey) {
        Error();
        return false;
    }
    return true;
}


int LookaheadParserHandler::getInt()
{
    if (state != kHasNumber) {
        Error();
        return 0;
    }
    auto result = val.GetInt();
    parseNext();
    return result;
}


float LookaheadParserHandler::getFloat()
{
    if (state != kHasNumber) {
        Error();
        return 0;
    }
    auto result = val.GetFloat();
    parseNext();
    return result;
}


const char* LookaheadParserHandler::getString()
{
    if (state != kHasString) {
        Error();
        return nullptr;
    }
    auto result = val.GetString();
    parseNext();
    return result;
}


char* LookaheadParserHandler::getStringCopy()
{
    auto str = getString();
    if (str) return strdup(str);
    return nullptr;
}


bool LookaheadParserHandler::getBool()
{
    if (state != kHasBool) {
        Error();
        return false;
    }
    auto result = val.GetBool();
    parseNext();
    return result;
}


void LookaheadParserHandler::getNull()
{
    if (state != kHasNull) {
        Error();
        return;
    }
    parseNext();
}


bool LookaheadParserHandler::parseNext()
{
    if (reader.HasParseError()) {
        Error();
        return false;
    }
    if (!reader.IterativeParseNext<PARSE_FLAGS>(iss, *this)) {
        Error();
        return false;
    }
    return true;
}


bool LookaheadParserHandler::enterObject()
{
    if (state != kEnteringObject) {
        Error();
        return false;
    }
    parseNext();
    return true;
}


int LookaheadParserHandler::peekType()
{
    if (state >= kHasNull && state <= kHasKey) return val.GetType();
    if (state == kEnteringArray) return kArrayType;
    if (state == kEnteringObject) return kObjectType;
    return -1;
}


void LookaheadParserHandler::skipOut(int depth)
{
    do {
        if (state == kEnteringArray || state == kEnteringObject) ++depth;
        else if (state == kExitingArray || state == kExitingObject) --depth;
        else if (state == kError) return;
        parseNext();
    } while (depth > 0);
}


const char* LookaheadParserHandler::nextObjectKey()
{
    if (state == kHasKey) {
        auto result = val.GetString();
        parseNext();
        return result;
    }

    /* SPECIAL CASE: The parser works with a predefined rule that it will be only
       while (nextObjectKey()) for each object but in case of our nested group
       object we can call multiple time nextObjectKey() while exiting the object
       so ignore those and don't put parser in the error state. */
    if (state == kExitingArray || state == kEnteringObject) return nullptr;

    if (state != kExitingObject) {
        Error();
        return nullptr;
    }

    parseNext();
    return nullptr;
}


void LookaheadParserHandler::skip(const char* key)
{
    //if (key) TVGLOG("LOTTIE", "Skipped parsing value = %s", key);

    if (peekType() == kArrayType) {
        enterArray();
        skipOut(1);
    } else if (peekType() == kObjectType) {
        enterObject();
        skipOut(1);
    } else {
        skipOut(0);
    }
}


char* LookaheadParserHandler::getPos()
{
    return iss.src_;
}

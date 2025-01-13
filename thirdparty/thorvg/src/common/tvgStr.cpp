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

#include "config.h"
#include <cmath>
#include <cstring>
#include <memory.h>
#include "tvgMath.h"
#include "tvgStr.h"


/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/

static inline bool _floatExact(float a, float b)
{
    return memcmp(&a, &b, sizeof(float)) == 0;
}


/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/

namespace tvg {

/*
 * https://docs.microsoft.com/en-us/cpp/c-runtime-library/reference/strtof-strtof-l-wcstof-wcstof-l?view=msvc-160
 *
 * src should be one of the following form :
 *
 * [whitespace] [sign] {digits [radix digits] | radix digits} [{e | E} [sign] digits]
 * [whitespace] [sign] {INF | INFINITY}
 * [whitespace] [sign] NAN [sequence]
 *
 * No hexadecimal form supported
 * no sequence supported after NAN
 */
float strToFloat(const char *nPtr, char **endPtr)
{
    if (endPtr) *endPtr = (char *) (nPtr);
    if (!nPtr) return 0.0f;

    auto a = nPtr;
    auto iter = nPtr;
    auto val = 0.0f;
    unsigned long long integerPart = 0;
    int minus = 1;

    //ignore leading whitespaces
    while (isspace(*iter)) iter++;

    //signed or not
    if (*iter == '-') {
        minus = -1;
        iter++;
    } else if (*iter == '+') {
        iter++;
    }

    if (tolower(*iter) == 'i') {
        if ((tolower(*(iter + 1)) == 'n') && (tolower(*(iter + 2)) == 'f')) iter += 3;
        else goto error;

        if (tolower(*(iter)) == 'i') {
            if ((tolower(*(iter + 1)) == 'n') && (tolower(*(iter + 2)) == 'i') && (tolower(*(iter + 3)) == 't') &&
                (tolower(*(iter + 4)) == 'y'))
                iter += 5;
            else goto error;
        }
        if (endPtr) *endPtr = (char *) (iter);
        return (minus == -1) ? -INFINITY : INFINITY;
    }

    if (tolower(*iter) == 'n') {
        if ((tolower(*(iter + 1)) == 'a') && (tolower(*(iter + 2)) == 'n')) iter += 3;
        else goto error;

        if (endPtr) *endPtr = (char *) (iter);
        return (minus == -1) ? -NAN : NAN;
    }

    //Optional: integer part before dot
    if (isdigit(*iter)) {
        for (; isdigit(*iter); iter++) {
            integerPart = integerPart * 10ULL + (unsigned long long) (*iter - '0');
        }
        a = iter;
    } else if (*iter != '.') {
        goto success;
    }

    val = static_cast<float>(integerPart);

    //Optional: decimal part after dot
    if (*iter == '.') {
        unsigned long long decimalPart = 0;
        unsigned long long pow10 = 1;
        int count = 0;

        iter++;

        if (isdigit(*iter)) {
            for (; isdigit(*iter); iter++, count++) {
                if (count < 19) {
                    decimalPart = decimalPart * 10ULL + +static_cast<unsigned long long>(*iter - '0');
                    pow10 *= 10ULL;
                }
            }
        } else if (isspace(*iter)) { //skip if there is a space after the dot.
            a = iter;
            goto success;
        }

        val += static_cast<float>(decimalPart) / static_cast<float>(pow10);
        a = iter;
    }

    //Optional: exponent
    if (*iter == 'e' || *iter == 'E') {
        ++iter;

        //Exception: svg may have 'em' unit for fonts. ex) 5em, 10.5em
        if ((*iter == 'm') || (*iter == 'M')) {
            //TODO: We don't support font em unit now, but has to multiply val * font size later...
            a = iter + 1;
            goto success;
        }

        //signed or not
        int minus_e = 1;

        if (*iter == '-') {
            minus_e = -1;
            ++iter;
        } else if (*iter == '+') {
            iter++;
        }

        unsigned int exponentPart = 0;

        if (isdigit(*iter)) {
            while (*iter == '0') iter++;
            for (; isdigit(*iter); iter++) {
                exponentPart = exponentPart * 10U + static_cast<unsigned int>(*iter - '0');
            }
        } else if (!isdigit(*(a - 1))) {
            a = nPtr;
            goto success;
        } else if (*iter == 0) {
            goto success;
        }

        //if ((_floatExact(val, 2.2250738585072011f)) && ((minus_e * static_cast<int>(exponentPart)) <= -308)) {
        if ((_floatExact(val, 1.175494351f)) && ((minus_e * static_cast<int>(exponentPart)) <= -38)) {
            //val *= 1.0e-308f;
            val *= 1.0e-38f;
            a = iter;
            goto success;
        }

        a = iter;
        auto scale = 1.0f;

        while (exponentPart >= 8U) {
            scale *= 1E8;
            exponentPart -= 8U;
        }
        while (exponentPart > 0U) {
            scale *= 10.0f;
            exponentPart--;
        }
        val = (minus_e == -1) ? (val / scale) : (val * scale);
    } else if ((iter > nPtr) && !isdigit(*(iter - 1))) {
        a = nPtr;
        goto success;
    }

success:
    if (endPtr) *endPtr = (char *)(a);
    if (!std::isfinite(val)) return 0.0f;

    return minus * val;

error:
    if (endPtr) *endPtr = (char *)(nPtr);
    return 0.0f;
}

char* strDuplicate(const char *str, size_t n)
{
    auto len = strlen(str);
    if (len < n) n = len;

    auto ret = (char *) malloc(n + 1);
    if (!ret) return nullptr;
    ret[n] = '\0';

    return (char *) memcpy(ret, str, n);
}

char* strAppend(char* lhs, const char* rhs, size_t n)
{
    if (!rhs) return lhs;
    if (!lhs) return strDuplicate(rhs, n);
    lhs = (char*)realloc(lhs, strlen(lhs) + n + 1);
    return strncat(lhs, rhs, n);
}

char* strDirname(const char* path)
{
    const char *ptr = strrchr(path, '/');
#ifdef _WIN32
    if (ptr) ptr = strrchr(ptr + 1, '\\');
#endif
    int len = int(ptr + 1 - path);  // +1 to include '/'
    return strDuplicate(path, len);
}

}

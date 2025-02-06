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

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <string.h>
#include "tvgCommon.h"
#include "tvgMath.h"
#include "tvgLottieInterpolator.h"


/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/

#define NEWTON_MIN_SLOPE 0.02f
#define NEWTON_ITERATIONS 4
#define SUBDIVISION_PRECISION 0.0000001f
#define SUBDIVISION_MAX_ITERATIONS 10


static inline float _constA(float aA1, float aA2) { return 1.0f - 3.0f * aA2 + 3.0f * aA1; }
static inline float _constB(float aA1, float aA2) { return 3.0f * aA2 - 6.0f * aA1; }
static inline float _constC(float aA1) { return 3.0f * aA1; }


static inline float _getSlope(float t, float aA1, float aA2)
{
    return 3.0f * _constA(aA1, aA2) * t * t + 2.0f * _constB(aA1, aA2) * t + _constC(aA1);
}


static inline float _calcBezier(float t, float aA1, float aA2)
{
    return ((_constA(aA1, aA2) * t + _constB(aA1, aA2)) * t + _constC(aA1)) * t;
}


float LottieInterpolator::getTForX(float aX)
{
    //Find interval where t lies
    auto intervalStart = 0.0f;
    auto currentSample = &samples[1];
    auto lastSample = &samples[SPLINE_TABLE_SIZE - 1];

    for (; currentSample != lastSample && *currentSample <= aX; ++currentSample) {
        intervalStart += SAMPLE_STEP_SIZE;
    }

    --currentSample;  // t now lies between *currentSample and *currentSample+1

    // Interpolate to provide an initial guess for t
    auto dist = (aX - *currentSample) / (*(currentSample + 1) - *currentSample);
    auto guessForT = intervalStart + dist * SAMPLE_STEP_SIZE;

    // Check the slope to see what strategy to use. If the slope is too small
    // Newton-Raphson iteration won't converge on a root so we use bisection
    // instead.
    auto initialSlope = _getSlope(guessForT, outTangent.x, inTangent.x);
    if (initialSlope >= NEWTON_MIN_SLOPE) return NewtonRaphsonIterate(aX, guessForT);
    else if (initialSlope == 0.0) return guessForT;
    else return binarySubdivide(aX, intervalStart, intervalStart + SAMPLE_STEP_SIZE);
}


float LottieInterpolator::binarySubdivide(float aX, float aA, float aB)
{
    float x, t;
    int i = 0;

    do {
        t = aA + (aB - aA) / 2.0f;
        x = _calcBezier(t, outTangent.x, inTangent.x) - aX;
        if (x > 0.0f) aB = t;
        else aA = t;
    } while (fabsf(x) > SUBDIVISION_PRECISION && ++i < SUBDIVISION_MAX_ITERATIONS);
    return t;
}


float LottieInterpolator::NewtonRaphsonIterate(float aX, float aGuessT)
{
    // Refine guess with Newton-Raphson iteration
    for (int i = 0; i < NEWTON_ITERATIONS; ++i) {
        // We're trying to find where f(t) = aX,
        // so we're actually looking for a root for: CalcBezier(t) - aX
        auto currentX = _calcBezier(aGuessT, outTangent.x, inTangent.x) - aX;
        auto currentSlope = _getSlope(aGuessT, outTangent.x, inTangent.x);
        if (currentSlope == 0.0f) return aGuessT;
        aGuessT -= currentX / currentSlope;
    }
    return aGuessT;
}


/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/

float LottieInterpolator::progress(float t)
{
    if (outTangent.x == outTangent.y && inTangent.x == inTangent.y) return t;
    return _calcBezier(getTForX(t), outTangent.y, inTangent.y);
}


void LottieInterpolator::set(const char* key, Point& inTangent, Point& outTangent)
{
    if (key) this->key = strdup(key);
    this->inTangent = inTangent;
    this->outTangent = outTangent;

    if (outTangent.x == outTangent.y && inTangent.x == inTangent.y) return;

    //calculates sample values
    for (int i = 0; i < SPLINE_TABLE_SIZE; ++i) {
        samples[i] = _calcBezier(float(i) * SAMPLE_STEP_SIZE, outTangent.x, inTangent.x);
    }
}

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

#ifndef _TVG_LOTTIE_INTERPOLATOR_H_
#define _TVG_LOTTIE_INTERPOLATOR_H_

#define SPLINE_TABLE_SIZE 11

struct LottieInterpolator
{
    char* key;
    Point outTangent, inTangent;

    float progress(float t);
    void set(const char* key, Point& inTangent, Point& outTangent);

private:
    static constexpr float SAMPLE_STEP_SIZE = 1.0f / float(SPLINE_TABLE_SIZE - 1);
    float samples[SPLINE_TABLE_SIZE];

    float getTForX(float aX);
    float binarySubdivide(float aX, float aA, float aB);
    float NewtonRaphsonIterate(float aX, float aGuessT);
};

#endif //_TVG_LOTTIE_INTERPOLATOR_H_

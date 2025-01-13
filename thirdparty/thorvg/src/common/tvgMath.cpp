/*
 * Copyright (c) 2021 - 2024 the ThorVG project. All rights reserved.

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

#include "tvgMath.h"

#define BEZIER_EPSILON 1e-2f


/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/

static float _lineLengthApprox(const Point& pt1, const Point& pt2)
{
    /* approximate sqrt(x*x + y*y) using alpha max plus beta min algorithm.
       With alpha = 1, beta = 3/8, giving results with the largest error less
       than 7% compared to the exact value. */
    Point diff = {pt2.x - pt1.x, pt2.y - pt1.y};
    if (diff.x < 0) diff.x = -diff.x;
    if (diff.y < 0) diff.y = -diff.y;
    return (diff.x > diff.y) ? (diff.x + diff.y * 0.375f) : (diff.y + diff.x * 0.375f);
}


static float _lineLength(const Point& pt1, const Point& pt2)
{
    Point diff = {pt2.x - pt1.x, pt2.y - pt1.y};
    return sqrtf(diff.x * diff.x + diff.y * diff.y);
}


template<typename LengthFunc>
float _bezLength(const Bezier& cur, LengthFunc lineLengthFunc)
{
    Bezier left, right;
    auto len = lineLengthFunc(cur.start, cur.ctrl1) + lineLengthFunc(cur.ctrl1, cur.ctrl2) + lineLengthFunc(cur.ctrl2, cur.end);
    auto chord = lineLengthFunc(cur.start, cur.end);

    if (fabsf(len - chord) > BEZIER_EPSILON) {
        cur.split(left, right);
        return _bezLength(left, lineLengthFunc) + _bezLength(right, lineLengthFunc);
    }
    return len;
}


template<typename LengthFunc>
float _bezAt(const Bezier& bz, float at, float length, LengthFunc lineLengthFunc)
{
    auto biggest = 1.0f;
    auto smallest = 0.0f;
    auto t = 0.5f;

    //just in case to prevent an infinite loop
    if (at <= 0) return 0.0f;
    if (at >= length) return 1.0f;

    while (true) {
        auto right = bz;
        Bezier left;
        right.split(t, left);
        length = _bezLength(left, lineLengthFunc);
        if (fabsf(length - at) < BEZIER_EPSILON || fabsf(smallest - biggest) < BEZIER_EPSILON) {
            break;
        }
        if (length < at) {
            smallest = t;
            t = (t + biggest) * 0.5f;
        } else {
            biggest = t;
            t = (smallest + t) * 0.5f;
        }
    }
    return t;
}


/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/

namespace tvg {

//https://en.wikipedia.org/wiki/Remez_algorithm
float atan2(float y, float x)
{
    if (y == 0.0f && x == 0.0f) return 0.0f;
    auto a = std::min(fabsf(x), fabsf(y)) / std::max(fabsf(x), fabsf(y));
    auto s = a * a;
    auto r = ((-0.0464964749f * s + 0.15931422f) * s - 0.327622764f) * s * a + a;
    if (fabsf(y) > fabsf(x)) r = 1.57079637f - r;
    if (x < 0) r = 3.14159274f - r;
    if (y < 0) return -r;
    return r;
}


bool inverse(const Matrix* m, Matrix* out)
{
    auto det = m->e11 * (m->e22 * m->e33 - m->e32 * m->e23) -
               m->e12 * (m->e21 * m->e33 - m->e23 * m->e31) +
               m->e13 * (m->e21 * m->e32 - m->e22 * m->e31);

    auto invDet = 1.0f / det;
    if (std::isinf(invDet)) return false;

    out->e11 = (m->e22 * m->e33 - m->e32 * m->e23) * invDet;
    out->e12 = (m->e13 * m->e32 - m->e12 * m->e33) * invDet;
    out->e13 = (m->e12 * m->e23 - m->e13 * m->e22) * invDet;
    out->e21 = (m->e23 * m->e31 - m->e21 * m->e33) * invDet;
    out->e22 = (m->e11 * m->e33 - m->e13 * m->e31) * invDet;
    out->e23 = (m->e21 * m->e13 - m->e11 * m->e23) * invDet;
    out->e31 = (m->e21 * m->e32 - m->e31 * m->e22) * invDet;
    out->e32 = (m->e31 * m->e12 - m->e11 * m->e32) * invDet;
    out->e33 = (m->e11 * m->e22 - m->e21 * m->e12) * invDet;

    return true;
}


bool identity(const Matrix* m)
{
    if (m->e11 != 1.0f || m->e12 != 0.0f || m->e13 != 0.0f ||
        m->e21 != 0.0f || m->e22 != 1.0f || m->e23 != 0.0f ||
        m->e31 != 0.0f || m->e32 != 0.0f || m->e33 != 1.0f) {
        return false;
    }
    return true;
}


void rotate(Matrix* m, float degree)
{
    if (degree == 0.0f) return;

    auto radian = degree / 180.0f * MATH_PI;
    auto cosVal = cosf(radian);
    auto sinVal = sinf(radian);

    m->e12 = m->e11 * -sinVal;
    m->e11 *= cosVal;
    m->e21 = m->e22 * sinVal;
    m->e22 *= cosVal;
}


Matrix operator*(const Matrix& lhs, const Matrix& rhs)
{
    Matrix m;

    m.e11 = lhs.e11 * rhs.e11 + lhs.e12 * rhs.e21 + lhs.e13 * rhs.e31;
    m.e12 = lhs.e11 * rhs.e12 + lhs.e12 * rhs.e22 + lhs.e13 * rhs.e32;
    m.e13 = lhs.e11 * rhs.e13 + lhs.e12 * rhs.e23 + lhs.e13 * rhs.e33;

    m.e21 = lhs.e21 * rhs.e11 + lhs.e22 * rhs.e21 + lhs.e23 * rhs.e31;
    m.e22 = lhs.e21 * rhs.e12 + lhs.e22 * rhs.e22 + lhs.e23 * rhs.e32;
    m.e23 = lhs.e21 * rhs.e13 + lhs.e22 * rhs.e23 + lhs.e23 * rhs.e33;

    m.e31 = lhs.e31 * rhs.e11 + lhs.e32 * rhs.e21 + lhs.e33 * rhs.e31;
    m.e32 = lhs.e31 * rhs.e12 + lhs.e32 * rhs.e22 + lhs.e33 * rhs.e32;
    m.e33 = lhs.e31 * rhs.e13 + lhs.e32 * rhs.e23 + lhs.e33 * rhs.e33;

    return m;
}


bool operator==(const Matrix& lhs, const Matrix& rhs)
{
    if (!tvg::equal(lhs.e11, rhs.e11) || !tvg::equal(lhs.e12, rhs.e12) || !tvg::equal(lhs.e13, rhs.e13) ||
        !tvg::equal(lhs.e21, rhs.e21) || !tvg::equal(lhs.e22, rhs.e22) || !tvg::equal(lhs.e23, rhs.e23) ||
        !tvg::equal(lhs.e31, rhs.e31) || !tvg::equal(lhs.e32, rhs.e32) || !tvg::equal(lhs.e33, rhs.e33)) {
       return false;
    }
    return true;
}


void operator*=(Point& pt, const Matrix& m)
{
    auto tx = pt.x * m.e11 + pt.y * m.e12 + m.e13;
    auto ty = pt.x * m.e21 + pt.y * m.e22 + m.e23;
    pt.x = tx;
    pt.y = ty;
}


Point operator*(const Point& pt, const Matrix& m)
{
    auto tx = pt.x * m.e11 + pt.y * m.e12 + m.e13;
    auto ty = pt.x * m.e21 + pt.y * m.e22 + m.e23;
    return {tx, ty};
}


Point normal(const Point& p1, const Point& p2)
{
    auto dir = p2 - p1;
    auto len = length(dir);
    if (tvg::zero(len)) return {};

    auto unitDir = dir / len;
    return {-unitDir.y, unitDir.x};
}


float Line::length() const
{
    return _lineLength(pt1, pt2);
}


void Line::split(float at, Line& left, Line& right) const
{
    auto len = length();
    auto dx = ((pt2.x - pt1.x) / len) * at;
    auto dy = ((pt2.y - pt1.y) / len) * at;
    left.pt1 = pt1;
    left.pt2.x = left.pt1.x + dx;
    left.pt2.y = left.pt1.y + dy;
    right.pt1 = left.pt2;
    right.pt2 = pt2;
}


void Bezier::split(Bezier& left, Bezier& right) const
{
    auto c = (ctrl1.x + ctrl2.x) * 0.5f;
    left.ctrl1.x = (start.x + ctrl1.x) * 0.5f;
    right.ctrl2.x = (ctrl2.x + end.x) * 0.5f;
    left.start.x = start.x;
    right.end.x = end.x;
    left.ctrl2.x = (left.ctrl1.x + c) * 0.5f;
    right.ctrl1.x = (right.ctrl2.x + c) * 0.5f;
    left.end.x = right.start.x = (left.ctrl2.x + right.ctrl1.x) * 0.5f;

    c = (ctrl1.y + ctrl2.y) * 0.5f;
    left.ctrl1.y = (start.y + ctrl1.y) * 0.5f;
    right.ctrl2.y = (ctrl2.y + end.y) * 0.5f;
    left.start.y = start.y;
    right.end.y = end.y;
    left.ctrl2.y = (left.ctrl1.y + c) * 0.5f;
    right.ctrl1.y = (right.ctrl2.y + c) * 0.5f;
    left.end.y = right.start.y = (left.ctrl2.y + right.ctrl1.y) * 0.5f;
}


void Bezier::split(float at, Bezier& left, Bezier& right) const
{
    right = *this;
    auto t = right.at(at, right.length());
    right.split(t, left);
}


float Bezier::length() const
{
    return _bezLength(*this, _lineLength);
}


float Bezier::lengthApprox() const
{
    return _bezLength(*this, _lineLengthApprox);
}


void Bezier::split(float t, Bezier& left)
{
    left.start = start;

    left.ctrl1.x = start.x + t * (ctrl1.x - start.x);
    left.ctrl1.y = start.y + t * (ctrl1.y - start.y);

    left.ctrl2.x = ctrl1.x + t * (ctrl2.x - ctrl1.x); //temporary holding spot
    left.ctrl2.y = ctrl1.y + t * (ctrl2.y - ctrl1.y); //temporary holding spot

    ctrl2.x = ctrl2.x + t * (end.x - ctrl2.x);
    ctrl2.y = ctrl2.y + t * (end.y - ctrl2.y);

    ctrl1.x = left.ctrl2.x + t * (ctrl2.x - left.ctrl2.x);
    ctrl1.y = left.ctrl2.y + t * (ctrl2.y - left.ctrl2.y);

    left.ctrl2.x = left.ctrl1.x + t * (left.ctrl2.x - left.ctrl1.x);
    left.ctrl2.y = left.ctrl1.y + t * (left.ctrl2.y - left.ctrl1.y);

    left.end.x = start.x = left.ctrl2.x + t * (ctrl1.x - left.ctrl2.x);
    left.end.y = start.y = left.ctrl2.y + t * (ctrl1.y - left.ctrl2.y);
}


float Bezier::at(float at, float length) const
{
    return _bezAt(*this, at, length, _lineLength);
}


float Bezier::atApprox(float at, float length) const
{
    return _bezAt(*this, at, length, _lineLengthApprox);
}


Point Bezier::at(float t) const
{
    Point cur;
    auto it = 1.0f - t;

    auto ax = start.x * it + ctrl1.x * t;
    auto bx = ctrl1.x * it + ctrl2.x * t;
    auto cx = ctrl2.x * it + end.x * t;
    ax = ax * it + bx * t;
    bx = bx * it + cx * t;
    cur.x = ax * it + bx * t;

    float ay = start.y * it + ctrl1.y * t;
    float by = ctrl1.y * it + ctrl2.y * t;
    float cy = ctrl2.y * it + end.y * t;
    ay = ay * it + by * t;
    by = by * it + cy * t;
    cur.y = ay * it + by * t;

    return cur;
}


float Bezier::angle(float t) const
{
    if (t < 0 || t > 1) return 0;

    //derivate
    // p'(t) = 3 * (-(1-2t+t^2) * p0 + (1 - 4 * t + 3 * t^2) * p1 + (2 * t - 3 *
    // t^2) * p2 + t^2 * p3)
    float mt = 1.0f - t;
    float d = t * t;
    float a = -mt * mt;
    float b = 1 - 4 * t + 3 * d;
    float c = 2 * t - 3 * d;

    Point pt ={a * start.x + b * ctrl1.x + c * ctrl2.x + d * end.x, a * start.y + b * ctrl1.y + c * ctrl2.y + d * end.y};
    pt.x *= 3;
    pt.y *= 3;

    return rad2deg(tvg::atan2(pt.y, pt.x));
}


uint8_t lerp(const uint8_t &start, const uint8_t &end, float t)
{
    auto result = static_cast<int>(start + (end - start) * t);
    tvg::clamp(result, 0, 255);
    return static_cast<uint8_t>(result);
}

}


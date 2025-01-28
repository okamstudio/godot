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

#include "tvgMath.h"
#include "tvgShape.h"

/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/


/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/

Shape :: Shape() : pImpl(new Impl(this))
{
}


Shape :: ~Shape()
{
    delete(pImpl);
}


unique_ptr<Shape> Shape::gen() noexcept
{
    return unique_ptr<Shape>(new Shape);
}


uint32_t Shape::identifier() noexcept
{
    return (uint32_t) Type::Shape;
}


Type Shape::type() const noexcept
{
    return Type::Shape;
}


Result Shape::reset() noexcept
{
    pImpl->rs.path.cmds.clear();
    pImpl->rs.path.pts.clear();

    pImpl->rFlag |= RenderUpdateFlag::Path;

    return Result::Success;
}


uint32_t Shape::pathCommands(const PathCommand** cmds) const noexcept
{
    if (cmds) *cmds = pImpl->rs.path.cmds.data;
    return pImpl->rs.path.cmds.count;
}


uint32_t Shape::pathCoords(const Point** pts) const noexcept
{
    if (pts) *pts = pImpl->rs.path.pts.data;
    return pImpl->rs.path.pts.count;
}


Result Shape::appendPath(const PathCommand *cmds, uint32_t cmdCnt, const Point* pts, uint32_t ptsCnt) noexcept
{
    if (cmdCnt == 0 || ptsCnt == 0 || !cmds || !pts) return Result::InvalidArguments;

    pImpl->grow(cmdCnt, ptsCnt);
    pImpl->append(cmds, cmdCnt, pts, ptsCnt);

    pImpl->rFlag |= RenderUpdateFlag::Path;

    return Result::Success;
}


Result Shape::moveTo(float x, float y) noexcept
{
    pImpl->moveTo(x, y);

    return Result::Success;
}


Result Shape::lineTo(float x, float y) noexcept
{
    pImpl->lineTo(x, y);

    pImpl->rFlag |= RenderUpdateFlag::Path;

    return Result::Success;
}


Result Shape::cubicTo(float cx1, float cy1, float cx2, float cy2, float x, float y) noexcept
{
    pImpl->cubicTo(cx1, cy1, cx2, cy2, x, y);

    pImpl->rFlag |= RenderUpdateFlag::Path;

    return Result::Success;
}


Result Shape::close() noexcept
{
    pImpl->close();

    pImpl->rFlag |= RenderUpdateFlag::Path;

    return Result::Success;
}


Result Shape::appendCircle(float cx, float cy, float rx, float ry) noexcept
{
    auto rxKappa = rx * PATH_KAPPA;
    auto ryKappa = ry * PATH_KAPPA;

    pImpl->grow(6, 13);
    pImpl->moveTo(cx + rx, cy);
    pImpl->cubicTo(cx + rx, cy + ryKappa, cx + rxKappa, cy + ry, cx, cy + ry);
    pImpl->cubicTo(cx - rxKappa, cy + ry, cx - rx, cy + ryKappa, cx - rx, cy);
    pImpl->cubicTo(cx - rx, cy - ryKappa, cx - rxKappa, cy - ry, cx, cy - ry);
    pImpl->cubicTo(cx + rxKappa, cy - ry, cx + rx, cy - ryKappa, cx + rx, cy);
    pImpl->close();

    pImpl->rFlag |= RenderUpdateFlag::Path;

    return Result::Success;
}


TVG_DEPRECATED Result Shape::appendArc(float cx, float cy, float radius, float startAngle, float sweep, bool pie) noexcept
{
    //just circle
    if (sweep >= 360.0f || sweep <= -360.0f) return appendCircle(cx, cy, radius, radius);

    const float arcPrecision = 1e-5f;
    startAngle = deg2rad(startAngle);
    sweep = deg2rad(sweep);

    auto nCurves = static_cast<int>(fabsf(sweep / MATH_PI2));
    if (fabsf(sweep / MATH_PI2) - nCurves > arcPrecision) ++nCurves;
    auto sweepSign = (sweep < 0 ? -1 : 1);
    auto fract = fmodf(sweep, MATH_PI2);
    fract = (fabsf(fract) < arcPrecision) ? MATH_PI2 * sweepSign : fract;

    //Start from here
    Point start = {radius * cosf(startAngle), radius * sinf(startAngle)};

    if (pie) {
        pImpl->moveTo(cx, cy);
        pImpl->lineTo(start.x + cx, start.y + cy);
    } else {
        pImpl->moveTo(start.x + cx, start.y + cy);
    }

    for (int i = 0; i < nCurves; ++i) {
        auto endAngle = startAngle + ((i != nCurves - 1) ? MATH_PI2 * sweepSign : fract);
        Point end = {radius * cosf(endAngle), radius * sinf(endAngle)};

        //variables needed to calculate bezier control points

        //get bezier control points using article:
        //(http://itc.ktu.lt/index.php/ITC/article/view/11812/6479)
        auto ax = start.x;
        auto ay = start.y;
        auto bx = end.x;
        auto by = end.y;
        auto q1 = ax * ax + ay * ay;
        auto q2 = ax * bx + ay * by + q1;
        auto k2 = (4.0f/3.0f) * ((sqrtf(2 * q1 * q2) - q2) / (ax * by - ay * bx));

        start = end; //Next start point is the current end point

        end.x += cx;
        end.y += cy;

        Point ctrl1 = {ax - k2 * ay + cx, ay + k2 * ax + cy};
        Point ctrl2 = {bx + k2 * by + cx, by - k2 * bx + cy};

        pImpl->cubicTo(ctrl1.x, ctrl1.y, ctrl2.x, ctrl2.y, end.x, end.y);

        startAngle = endAngle;
    }

    if (pie) pImpl->close();

    pImpl->rFlag |= RenderUpdateFlag::Path;

    return Result::Success;
}


Result Shape::appendRect(float x, float y, float w, float h, float rx, float ry) noexcept
{
    auto halfW = w * 0.5f;
    auto halfH = h * 0.5f;

    //clamping cornerRadius by minimum size
    if (rx > halfW) rx = halfW;
    if (ry > halfH) ry = halfH;

    //rectangle
    if (rx == 0 && ry == 0) {
        pImpl->grow(5, 4);
        pImpl->moveTo(x, y);
        pImpl->lineTo(x + w, y);
        pImpl->lineTo(x + w, y + h);
        pImpl->lineTo(x, y + h);
        pImpl->close();
    //rounded rectangle or circle
    } else {
        auto hrx = rx * PATH_KAPPA;
        auto hry = ry * PATH_KAPPA;
        pImpl->grow(10, 17);
        pImpl->moveTo(x + rx, y);
        pImpl->lineTo(x + w - rx, y);
        pImpl->cubicTo(x + w - rx + hrx, y, x + w, y + ry - hry, x + w, y + ry);
        pImpl->lineTo(x + w, y + h - ry);
        pImpl->cubicTo(x + w, y + h - ry + hry, x + w - rx + hrx, y + h, x + w - rx, y + h);
        pImpl->lineTo(x + rx, y + h);
        pImpl->cubicTo(x + rx - hrx, y + h, x, y + h - ry + hry, x, y + h - ry);
        pImpl->lineTo(x, y + ry);
        pImpl->cubicTo(x, y + ry - hry, x + rx - hrx, y, x + rx, y);
        pImpl->close();
    }

    pImpl->rFlag |= RenderUpdateFlag::Path;

    return Result::Success;
}


Result Shape::fill(uint8_t r, uint8_t g, uint8_t b, uint8_t a) noexcept
{
    if (pImpl->rs.fill) {
        delete(pImpl->rs.fill);
        pImpl->rs.fill = nullptr;
        pImpl->rFlag |= RenderUpdateFlag::Gradient;
    }

    if (r == pImpl->rs.color[0] && g == pImpl->rs.color[1] && b == pImpl->rs.color[2] && a == pImpl->rs.color[3]) return Result::Success;

    pImpl->rs.color[0] = r;
    pImpl->rs.color[1] = g;
    pImpl->rs.color[2] = b;
    pImpl->rs.color[3] = a;
    pImpl->rFlag |= RenderUpdateFlag::Color;

    return Result::Success;
}


Result Shape::fill(unique_ptr<Fill> f) noexcept
{
    auto p = f.release();
    if (!p) return Result::MemoryCorruption;

    if (pImpl->rs.fill && pImpl->rs.fill != p) delete(pImpl->rs.fill);
    pImpl->rs.fill = p;
    pImpl->rFlag |= RenderUpdateFlag::Gradient;

    return Result::Success;
}


Result Shape::fillColor(uint8_t* r, uint8_t* g, uint8_t* b, uint8_t* a) const noexcept
{
    pImpl->rs.fillColor(r, g, b, a);

    return Result::Success;
}


const Fill* Shape::fill() const noexcept
{
    return pImpl->rs.fill;
}


Result Shape::order(bool strokeFirst) noexcept
{
    pImpl->strokeFirst(strokeFirst);
    return Result::Success;
}


Result Shape::stroke(float width) noexcept
{
    pImpl->strokeWidth(width);
    return Result::Success;
}


float Shape::strokeWidth() const noexcept
{
    return pImpl->rs.strokeWidth();
}


Result Shape::stroke(uint8_t r, uint8_t g, uint8_t b, uint8_t a) noexcept
{
    pImpl->strokeColor(r, g, b, a);
    return Result::Success;
}


Result Shape::strokeColor(uint8_t* r, uint8_t* g, uint8_t* b, uint8_t* a) const noexcept
{
    if (!pImpl->rs.strokeColor(r, g, b, a)) return Result::InsufficientCondition;

    return Result::Success;
}


Result Shape::stroke(unique_ptr<Fill> f) noexcept
{
    return pImpl->strokeFill(std::move(f));
}


const Fill* Shape::strokeFill() const noexcept
{
    return pImpl->rs.strokeFill();
}


Result Shape::stroke(const float* dashPattern, uint32_t cnt) noexcept
{
    return pImpl->strokeDash(dashPattern, cnt, 0);
}


uint32_t Shape::strokeDash(const float** dashPattern) const noexcept
{
    return pImpl->rs.strokeDash(dashPattern, nullptr);
}


Result Shape::stroke(StrokeCap cap) noexcept
{
    pImpl->strokeCap(cap);
    return Result::Success;
}


Result Shape::stroke(StrokeJoin join) noexcept
{
    pImpl->strokeJoin(join);
    return Result::Success;
}


Result Shape::strokeMiterlimit(float miterlimit) noexcept
{
    // https://www.w3.org/TR/SVG2/painting.html#LineJoin
    // - A negative value for stroke-miterlimit must be treated as an illegal value.
    if (miterlimit < 0.0f) return Result::InvalidArguments;
    // TODO Find out a reasonable max value.
    pImpl->strokeMiterlimit(miterlimit);
    return Result::Success;
}


StrokeCap Shape::strokeCap() const noexcept
{
    return pImpl->rs.strokeCap();
}


StrokeJoin Shape::strokeJoin() const noexcept
{
    return pImpl->rs.strokeJoin();
}


float Shape::strokeMiterlimit() const noexcept
{
    return pImpl->rs.strokeMiterlimit();
}


Result Shape::strokeTrim(float begin, float end, bool simultaneous) noexcept
{
    pImpl->strokeTrim(begin, end, simultaneous);
    return Result::Success;
}


Result Shape::fill(FillRule r) noexcept
{
    pImpl->rs.rule = r;

    return Result::Success;
}


FillRule Shape::fillRule() const noexcept
{
    return pImpl->rs.rule;
}

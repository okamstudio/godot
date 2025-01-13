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

#include <string.h>
#include <math.h>
#include "tvgSwCommon.h"

/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/

static constexpr auto SW_STROKE_TAG_POINT = 1;
static constexpr auto SW_STROKE_TAG_CUBIC = 2;
static constexpr auto SW_STROKE_TAG_BEGIN = 4;
static constexpr auto SW_STROKE_TAG_END = 8;

static inline SwFixed SIDE_TO_ROTATE(const int32_t s)
{
    return (SW_ANGLE_PI2 - static_cast<SwFixed>(s) * SW_ANGLE_PI);
}


static inline void SCALE(const SwStroke& stroke, SwPoint& pt)
{
    pt.x = static_cast<SwCoord>(pt.x * stroke.sx);
    pt.y = static_cast<SwCoord>(pt.y * stroke.sy);
}


static void _growBorder(SwStrokeBorder* border, uint32_t newPts)
{
    auto maxOld = border->maxPts;
    auto maxNew = border->ptsCnt + newPts;

    if (maxNew <= maxOld) return;

    auto maxCur = maxOld;

    while (maxCur < maxNew)
        maxCur += (maxCur >> 1) + 16;
    //OPTIMIZE: use mempool!
    border->pts = static_cast<SwPoint*>(realloc(border->pts, maxCur * sizeof(SwPoint)));
    border->tags = static_cast<uint8_t*>(realloc(border->tags, maxCur * sizeof(uint8_t)));
    border->maxPts = maxCur;
}


static void _borderClose(SwStrokeBorder* border, bool reverse)
{
    auto start = border->start;
    auto count = border->ptsCnt;

    //Don't record empty paths!
    if (count <= start + 1U) {
        border->ptsCnt = start;
    } else {
        /* Copy the last point to the start of this sub-path,
           since it contains the adjusted starting coordinates */
        border->ptsCnt = --count;
        border->pts[start] = border->pts[count];

        if (reverse) {
            //reverse the points
            auto pt1 = border->pts + start + 1;
            auto pt2 = border->pts + count - 1;

            while (pt1 < pt2) {
                auto tmp = *pt1;
                *pt1 = *pt2;
                *pt2 = tmp;
                ++pt1;
                --pt2;
            }

            //reverse the tags
            auto tag1 = border->tags + start + 1;
            auto tag2 = border->tags + count - 1;

            while (tag1 < tag2) {
                auto tmp = *tag1;
                *tag1 = *tag2;
                *tag2 = tmp;
                ++tag1;
                --tag2;
            }
        }

        border->tags[start] |= SW_STROKE_TAG_BEGIN;
        border->tags[count - 1] |=  SW_STROKE_TAG_END;
    }

    border->start = -1;
    border->movable = false;
}


static void _borderCubicTo(SwStrokeBorder* border, const SwPoint& ctrl1, const SwPoint& ctrl2, const SwPoint& to)
{
    _growBorder(border, 3);

    auto pt = border->pts + border->ptsCnt;
    auto tag = border->tags + border->ptsCnt;

    pt[0] = ctrl1;
    pt[1] = ctrl2;
    pt[2] = to;

    tag[0] = SW_STROKE_TAG_CUBIC;
    tag[1] = SW_STROKE_TAG_CUBIC;
    tag[2] = SW_STROKE_TAG_POINT;

    border->ptsCnt += 3;
    border->movable = false;
}


static void _borderArcTo(SwStrokeBorder* border, const SwPoint& center, SwFixed radius, SwFixed angleStart, SwFixed angleDiff, SwStroke& stroke)
{
    constexpr SwFixed ARC_CUBIC_ANGLE = SW_ANGLE_PI / 2;
    SwPoint a = {static_cast<SwCoord>(radius), 0};
    mathRotate(a, angleStart);
    SCALE(stroke, a);
    a += center;

    auto total = angleDiff;
    auto angle = angleStart;
    auto rotate = (angleDiff >= 0) ? SW_ANGLE_PI2 : -SW_ANGLE_PI2;

    while (total != 0) {
        auto step = total;
        if (step > ARC_CUBIC_ANGLE) step = ARC_CUBIC_ANGLE;
        else if (step < -ARC_CUBIC_ANGLE) step = -ARC_CUBIC_ANGLE;

        auto next = angle + step;
        auto theta = step;
        if (theta < 0) theta = -theta;

        theta >>= 1;

        //compute end point
        SwPoint b = {static_cast<SwCoord>(radius), 0};
        mathRotate(b, next);
        SCALE(stroke, b);
        b += center;

        //compute first and second control points
        auto length = mathMulDiv(radius, mathSin(theta) * 4, (0x10000L + mathCos(theta)) * 3);

        SwPoint a2 = {static_cast<SwCoord>(length), 0};
        mathRotate(a2, angle + rotate);
        SCALE(stroke, a2);
        a2 += a;

        SwPoint b2 = {static_cast<SwCoord>(length), 0};
        mathRotate(b2, next - rotate);
        SCALE(stroke, b2);
        b2 += b;

        //add cubic arc
        _borderCubicTo(border, a2, b2, b);

        //process the rest of the arc?
        a = b;
        total -= step;
        angle = next;
    }
}


static void _borderLineTo(SwStrokeBorder* border, const SwPoint& to, bool movable)
{
    if (border->movable) {
        //move last point
        border->pts[border->ptsCnt - 1] = to;
    } else {
        //don't add zero-length line_to
        if (border->ptsCnt > 0 && (border->pts[border->ptsCnt - 1] - to).small()) return;

        _growBorder(border, 1);
        border->pts[border->ptsCnt] = to;
        border->tags[border->ptsCnt] = SW_STROKE_TAG_POINT;
        border->ptsCnt += 1;
    }

    border->movable = movable;
}


static void _borderMoveTo(SwStrokeBorder* border, SwPoint& to)
{
    //close current open path if any?
    if (border->start >= 0) _borderClose(border, false);

    border->start = border->ptsCnt;
    border->movable = false;

    _borderLineTo(border, to, false);
}


static void _arcTo(SwStroke& stroke, int32_t side)
{
    auto border = stroke.borders + side;
    auto rotate = SIDE_TO_ROTATE(side);
    auto total = mathDiff(stroke.angleIn, stroke.angleOut);
    if (total == SW_ANGLE_PI) total = -rotate * 2;

    _borderArcTo(border, stroke.center, stroke.width, stroke.angleIn + rotate, total, stroke);
    border->movable = false;
}


static void _outside(SwStroke& stroke, int32_t side, SwFixed lineLength)
{
    auto border = stroke.borders + side;

    if (stroke.join == StrokeJoin::Round) {
        _arcTo(stroke, side);
    } else {
        //this is a mitered (pointed) or beveled (truncated) corner
        auto rotate = SIDE_TO_ROTATE(side);
        auto bevel = stroke.join == StrokeJoin::Bevel;
        SwFixed phi = 0;
        SwFixed thcos = 0;

        if (!bevel) {
            auto theta = mathDiff(stroke.angleIn, stroke.angleOut);
            if (theta == SW_ANGLE_PI) {
                theta = rotate;
                phi = stroke.angleIn;
            } else {
                theta /= 2;
                phi = stroke.angleIn + theta + rotate;
            }

            thcos = mathCos(theta);
            auto sigma = mathMultiply(stroke.miterlimit, thcos);

            //is miter limit exceeded?
            if (sigma < 0x10000L) bevel = true;
        }

        //this is a bevel (broken angle)
        if (bevel) {
            SwPoint delta = {static_cast<SwCoord>(stroke.width), 0};
            mathRotate(delta, stroke.angleOut + rotate);
            SCALE(stroke, delta);
            delta += stroke.center;
            border->movable = false;
            _borderLineTo(border, delta, false);
        //this is a miter (intersection)
        } else {
            auto length = mathDivide(stroke.width, thcos);
            SwPoint delta = {static_cast<SwCoord>(length), 0};
            mathRotate(delta, phi);
            SCALE(stroke, delta);
            delta += stroke.center;
            _borderLineTo(border, delta, false);

            /* Now add and end point
               Only needed if not lineto (lineLength is zero for curves) */
            if (lineLength == 0) {
                delta = {static_cast<SwCoord>(stroke.width), 0};
                mathRotate(delta, stroke.angleOut + rotate);
                SCALE(stroke, delta);
                delta += stroke.center;
                _borderLineTo(border, delta, false);
            }
        }
    }
}


static void _inside(SwStroke& stroke, int32_t side, SwFixed lineLength)
{
    auto border = stroke.borders + side;
    auto theta = mathDiff(stroke.angleIn, stroke.angleOut) / 2;
    SwPoint delta;
    bool intersect = false;

    /* Only intersect borders if between two line_to's and both
       lines are long enough (line length is zero for curves). */
    if (border->movable && lineLength > 0) {
        //compute minimum required length of lines
        SwFixed minLength = abs(mathMultiply(stroke.width, mathTan(theta)));
        if (stroke.lineLength >= minLength && lineLength >= minLength) intersect = true;
    }

    auto rotate = SIDE_TO_ROTATE(side);

    if (!intersect) {
        delta = {static_cast<SwCoord>(stroke.width), 0};
        mathRotate(delta, stroke.angleOut + rotate);
        SCALE(stroke, delta);
        delta += stroke.center;
        border->movable = false;
    } else {
        //compute median angle
        auto phi = stroke.angleIn + theta;
        auto thcos = mathCos(theta);
        delta = {static_cast<SwCoord>(mathDivide(stroke.width, thcos)), 0};
        mathRotate(delta, phi + rotate);
        SCALE(stroke, delta);
        delta += stroke.center;
    }

    _borderLineTo(border, delta, false);
}


void _processCorner(SwStroke& stroke, SwFixed lineLength)
{
    auto turn = mathDiff(stroke.angleIn, stroke.angleOut);

    //no specific corner processing is required if the turn is 0
    if (turn == 0) return;

    //when we turn to the right, the inside side is 0
    int32_t inside = 0;

    //otherwise, the inside is 1
    if (turn < 0) inside = 1;

    //process the inside
    _inside(stroke, inside, lineLength);

    //process the outside
    _outside(stroke, 1 - inside, lineLength);
}


void _firstSubPath(SwStroke& stroke, SwFixed startAngle, SwFixed lineLength)
{
    SwPoint delta = {static_cast<SwCoord>(stroke.width), 0};
    mathRotate(delta, startAngle + SW_ANGLE_PI2);
    SCALE(stroke, delta);

    auto pt = stroke.center + delta;
    auto border = stroke.borders;
    _borderMoveTo(border, pt);

    pt = stroke.center - delta;
    ++border;
    _borderMoveTo(border, pt);

    /* Save angle, position and line length for last join
       lineLength is zero for curves */
    stroke.subPathAngle = startAngle;
    stroke.firstPt = false;
    stroke.subPathLineLength = lineLength;
}


static void _lineTo(SwStroke& stroke, const SwPoint& to)
{
    auto delta = to - stroke.center;

    //a zero-length lineto is a no-op; avoid creating a spurious corner
    if (delta.zero()) return;

    /* The lineLength is used to determine the intersection of strokes outlines.
       The scale needs to be reverted since the stroke width has not been scaled.
       An alternative option is to scale the width of the stroke properly by
       calculating the mixture of the sx/sy rating on the stroke direction. */
    delta.x = static_cast<SwCoord>(delta.x / stroke.sx);
    delta.y = static_cast<SwCoord>(delta.y / stroke.sy);
    auto lineLength = mathLength(delta);
    auto angle = mathAtan(delta);

    delta = {static_cast<SwCoord>(stroke.width), 0};
    mathRotate(delta, angle + SW_ANGLE_PI2);
    SCALE(stroke, delta);

    //process corner if necessary
    if (stroke.firstPt) {
        /* This is the first segment of a subpath. We need to add a point to each border
        at their respective starting point locations. */
        _firstSubPath(stroke, angle, lineLength);
    } else {
        //process the current corner
        stroke.angleOut = angle;
        _processCorner(stroke, lineLength);
    }

    //now add a line segment to both the inside and outside paths
    auto border = stroke.borders;
    auto side = 1;

    while (side >= 0) {
        auto pt = to + delta;

        //the ends of lineto borders are movable
        _borderLineTo(border, pt, true);

        delta.x = -delta.x;
        delta.y = -delta.y;

        --side;
        ++border;
    }

    stroke.angleIn = angle;
    stroke.center = to;
    stroke.lineLength = lineLength;
}


static void _cubicTo(SwStroke& stroke, const SwPoint& ctrl1, const SwPoint& ctrl2, const SwPoint& to)
{
    SwPoint bezStack[37];   //TODO: static?
    auto limit = bezStack + 32;
    auto arc = bezStack;
    auto firstArc = true;
    arc[0] = to;
    arc[1] = ctrl2;
    arc[2] = ctrl1;
    arc[3] = stroke.center;

    while (arc >= bezStack) {
        SwFixed angleIn, angleOut, angleMid;

        //initialize with current direction
        angleIn = angleOut = angleMid = stroke.angleIn;

        auto valid = mathCubicAngle(arc, angleIn, angleMid, angleOut);

        //valid size
        if (valid > 0 && arc < limit) {
            if (stroke.firstPt) stroke.angleIn = angleIn;
            mathSplitCubic(arc);
            arc += 3;
            continue;
        }

        //ignoreable size
        if (valid < 0 && arc == bezStack) {
            stroke.center = to;
            return;
        }

        //small size
        if (firstArc) {
            firstArc = false;
            //process corner if necessary
            if (stroke.firstPt) {
                _firstSubPath(stroke, angleIn, 0);
            } else {
                stroke.angleOut = angleIn;
                _processCorner(stroke, 0);
            }
        } else if (abs(mathDiff(stroke.angleIn, angleIn)) > (SW_ANGLE_PI / 8) / 4) {
            //if the deviation from one arc to the next is too great add a round corner
            stroke.center = arc[3];
            stroke.angleOut = angleIn;
            stroke.join = StrokeJoin::Round;

            _processCorner(stroke, 0);

            //reinstate line join style
            stroke.join = stroke.joinSaved;
        }

        //the arc's angle is small enough; we can add it directly to each border
        auto theta1 = mathDiff(angleIn, angleMid) / 2;
        auto theta2 = mathDiff(angleMid, angleOut) / 2;
        auto phi1 = mathMean(angleIn, angleMid);
        auto phi2 = mathMean(angleMid, angleOut);
        auto length1 = mathDivide(stroke.width, mathCos(theta1));
        auto length2 = mathDivide(stroke.width, mathCos(theta2));
        SwFixed alpha0 = 0;

        //compute direction of original arc
        if (stroke.handleWideStrokes) {
            alpha0 = mathAtan(arc[0] - arc[3]);
        }

        auto border = stroke.borders;
        int32_t side = 0;

        while (side < 2) {
            auto rotate = SIDE_TO_ROTATE(side);

            //compute control points
            SwPoint _ctrl1 = {static_cast<SwCoord>(length1), 0};
            mathRotate(_ctrl1, phi1 + rotate);
            SCALE(stroke, _ctrl1);
            _ctrl1 += arc[2];

            SwPoint _ctrl2 = {static_cast<SwCoord>(length2), 0};
            mathRotate(_ctrl2, phi2 + rotate);
            SCALE(stroke, _ctrl2);
            _ctrl2 += arc[1];

            //compute end point
            SwPoint _end = {static_cast<SwCoord>(stroke.width), 0};
            mathRotate(_end, angleOut + rotate);
            SCALE(stroke, _end);
            _end += arc[0];

            if (stroke.handleWideStrokes) {
                /* determine whether the border radius is greater than the radius of
                   curvature of the original arc */
                auto _start = border->pts[border->ptsCnt - 1];
                auto alpha1 = mathAtan(_end - _start);

                //is the direction of the border arc opposite to that of the original arc?
                if (abs(mathDiff(alpha0, alpha1)) > SW_ANGLE_PI / 2) {

                    //use the sine rule to find the intersection point
                    auto beta = mathAtan(arc[3] - _start);
                    auto gamma = mathAtan(arc[0] - _end);
                    auto bvec = _end - _start;
                    auto blen = mathLength(bvec);
                    auto sinA = abs(mathSin(alpha1 - gamma));
                    auto sinB = abs(mathSin(beta - gamma));
                    auto alen = mathMulDiv(blen, sinA, sinB);

                    SwPoint delta = {static_cast<SwCoord>(alen), 0};
                    mathRotate(delta, beta);
                    delta += _start;

                    //circumnavigate the negative sector backwards
                    border->movable = false;
                    _borderLineTo(border, delta, false);
                    _borderLineTo(border, _end, false);
                    _borderCubicTo(border, _ctrl2, _ctrl1, _start);

                    //and then move to the endpoint
                    _borderLineTo(border, _end, false);

                    ++side;
                    ++border;
                    continue;
                }
            }
            _borderCubicTo(border, _ctrl1, _ctrl2, _end);
            ++side;
            ++border;
        }
        arc -= 3;
        stroke.angleIn = angleOut;
    }
    stroke.center = to;
}


static void _addCap(SwStroke& stroke, SwFixed angle, int32_t side)
{
    if (stroke.cap == StrokeCap::Square) {
        auto rotate = SIDE_TO_ROTATE(side);
        auto border = stroke.borders + side;

        SwPoint delta = {static_cast<SwCoord>(stroke.width), 0};
        mathRotate(delta, angle);
        SCALE(stroke, delta);

        SwPoint delta2 = {static_cast<SwCoord>(stroke.width), 0};
        mathRotate(delta2, angle + rotate);
        SCALE(stroke, delta2);
        delta += stroke.center + delta2;

        _borderLineTo(border, delta, false);

        delta = {static_cast<SwCoord>(stroke.width), 0};
        mathRotate(delta, angle);
        SCALE(stroke, delta);

        delta2 = {static_cast<SwCoord>(stroke.width), 0};
        mathRotate(delta2, angle - rotate);
        SCALE(stroke, delta2);
        delta += delta2 + stroke.center;

        _borderLineTo(border, delta, false);

    } else if (stroke.cap == StrokeCap::Round) {

        stroke.angleIn = angle;
        stroke.angleOut = angle + SW_ANGLE_PI;
        _arcTo(stroke, side);
        return;

    } else {  //Butt
        auto rotate = SIDE_TO_ROTATE(side);
        auto border = stroke.borders + side;

        SwPoint delta = {static_cast<SwCoord>(stroke.width), 0};
        mathRotate(delta, angle + rotate);
        SCALE(stroke, delta);
        delta += stroke.center;

        _borderLineTo(border, delta, false);

        delta = {static_cast<SwCoord>(stroke.width), 0};
        mathRotate(delta, angle - rotate);
        SCALE(stroke, delta);
        delta += stroke.center;

        _borderLineTo(border, delta, false);
    }
}


static void _addReverseLeft(SwStroke& stroke, bool opened)
{
    auto right = stroke.borders + 0;
    auto left = stroke.borders + 1;
    auto newPts = left->ptsCnt - left->start;

    if (newPts <= 0) return;

    _growBorder(right, newPts);

    auto dstPt = right->pts + right->ptsCnt;
    auto dstTag = right->tags + right->ptsCnt;
    auto srcPt = left->pts + left->ptsCnt - 1;
    auto srcTag = left->tags + left->ptsCnt - 1;

    while (srcPt >= left->pts + left->start) {
        *dstPt = *srcPt;
        *dstTag = *srcTag;

        if (opened) {
             dstTag[0] &= ~(SW_STROKE_TAG_BEGIN | SW_STROKE_TAG_END);
        } else {
            //switch begin/end tags if necessary
            auto ttag = dstTag[0] & (SW_STROKE_TAG_BEGIN | SW_STROKE_TAG_END);
            if (ttag == SW_STROKE_TAG_BEGIN || ttag == SW_STROKE_TAG_END)
              dstTag[0] ^= (SW_STROKE_TAG_BEGIN | SW_STROKE_TAG_END);
        }
        --srcPt;
        --srcTag;
        ++dstPt;
        ++dstTag;
    }

    left->ptsCnt = left->start;
    right->ptsCnt += newPts;
    right->movable = false;
    left->movable = false;
}


static void _beginSubPath(SwStroke& stroke, const SwPoint& to, bool closed)
{
    /* We cannot process the first point because there is not enough
       information regarding its corner/cap. Later, it will be processed
       in the _endSubPath() */

    stroke.firstPt = true;
    stroke.center = to;
    stroke.closedSubPath = closed;

    /* Determine if we need to check whether the border radius is greater
       than the radius of curvature of a curve, to handle this case specially.
       This is only required if bevel joins or butt caps may be created because
       round & miter joins and round & square caps cover the negative sector
       created with wide strokes. */
    if ((stroke.join != StrokeJoin::Round) || (!stroke.closedSubPath && stroke.cap == StrokeCap::Butt))
        stroke.handleWideStrokes = true;
    else
        stroke.handleWideStrokes = false;

    stroke.ptStartSubPath = to;
    stroke.angleIn = 0;
}


static void _endSubPath(SwStroke& stroke)
{
    if (stroke.closedSubPath) {
        //close the path if needed
        if (stroke.center != stroke.ptStartSubPath)
            _lineTo(stroke, stroke.ptStartSubPath);

        //process the corner
        stroke.angleOut = stroke.subPathAngle;
        auto turn = mathDiff(stroke.angleIn, stroke.angleOut);

        //No specific corner processing is required if the turn is 0
        if (turn != 0) {
            //when we turn to the right, the inside is 0
            int32_t inside = 0;

            //otherwise, the inside is 1
            if (turn < 0) inside = 1;

            _inside(stroke, inside, stroke.subPathLineLength);        //inside
            _outside(stroke, 1 - inside, stroke.subPathLineLength);   //outside
        }

        _borderClose(stroke.borders + 0, false);
        _borderClose(stroke.borders + 1, true);
    } else {
        auto right = stroke.borders;

        /* all right, this is an opened path, we need to add a cap between
           right & left, add the reverse of left, then add a final cap
           between left & right */
        _addCap(stroke, stroke.angleIn, 0);

        //add reversed points from 'left' to 'right'
        _addReverseLeft(stroke, true);

        //now add the final cap
        stroke.center = stroke.ptStartSubPath;
        _addCap(stroke, stroke.subPathAngle + SW_ANGLE_PI, 0);

        /* now end the right subpath accordingly. The left one is rewind
           and doesn't need further processing */
        _borderClose(right, false);
    }
}


static void _getCounts(SwStrokeBorder* border, uint32_t& ptsCnt, uint32_t& cntrsCnt)
{
    auto count = border->ptsCnt;
    auto tags = border->tags;
    uint32_t _ptsCnt = 0;
    uint32_t _cntrsCnt = 0;
    bool inCntr = false;

    while (count > 0) {
        if (tags[0] & SW_STROKE_TAG_BEGIN) {
            if (inCntr) goto fail;
            inCntr = true;
        } else if (!inCntr) goto fail;

        if (tags[0] & SW_STROKE_TAG_END) {
            inCntr = false;
            ++_cntrsCnt;
        }
        --count;
        ++_ptsCnt;
        ++tags;
    }

    if (inCntr) goto fail;

    ptsCnt = _ptsCnt;
    cntrsCnt = _cntrsCnt;

    return;

fail:
    ptsCnt = 0;
    cntrsCnt = 0;
}


static void _exportBorderOutline(const SwStroke& stroke, SwOutline* outline, uint32_t side)
{
    auto border = stroke.borders + side;
    if (border->ptsCnt == 0) return;

    memcpy(outline->pts.data + outline->pts.count, border->pts, border->ptsCnt * sizeof(SwPoint));

    auto cnt = border->ptsCnt;
    auto src = border->tags;
    auto tags = outline->types.data + outline->types.count;
    auto idx = outline->pts.count;

    while (cnt > 0) {
        if (*src & SW_STROKE_TAG_POINT) *tags = SW_CURVE_TYPE_POINT;
        else if (*src & SW_STROKE_TAG_CUBIC) *tags = SW_CURVE_TYPE_CUBIC;
        else TVGERR("SW_ENGINE", "Invalid stroke tag was given! = %d", *src);
        if (*src & SW_STROKE_TAG_END) outline->cntrs.push(idx);
        ++src;
        ++tags;
        ++idx;
        --cnt;
    }
    outline->pts.count += border->ptsCnt;
    outline->types.count += border->ptsCnt;
}


/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/

void strokeFree(SwStroke* stroke)
{
    if (!stroke) return;

    //free borders
    if (stroke->borders[0].pts) free(stroke->borders[0].pts);
    if (stroke->borders[0].tags) free(stroke->borders[0].tags);
    if (stroke->borders[1].pts) free(stroke->borders[1].pts);
    if (stroke->borders[1].tags) free(stroke->borders[1].tags);

    fillFree(stroke->fill);
    stroke->fill = nullptr;

    free(stroke);
}


void strokeReset(SwStroke* stroke, const RenderShape* rshape, const Matrix& transform)
{
    stroke->sx = sqrtf(powf(transform.e11, 2.0f) + powf(transform.e21, 2.0f));
    stroke->sy = sqrtf(powf(transform.e12, 2.0f) + powf(transform.e22, 2.0f));
    stroke->width = HALF_STROKE(rshape->strokeWidth());
    stroke->cap = rshape->strokeCap();
    stroke->miterlimit = static_cast<SwFixed>(rshape->strokeMiterlimit() * 65536.0f);

    //Save line join: it can be temporarily changed when stroking curves...
    stroke->joinSaved = stroke->join = rshape->strokeJoin();

    stroke->borders[0].ptsCnt = 0;
    stroke->borders[0].start = -1;
    stroke->borders[1].ptsCnt = 0;
    stroke->borders[1].start = -1;
}


bool strokeParseOutline(SwStroke* stroke, const SwOutline& outline)
{
    uint32_t first = 0;
    uint32_t i = 0;

    for (auto cntr = outline.cntrs.begin(); cntr < outline.cntrs.end(); ++cntr, ++i) {
        auto last = *cntr;           //index of last point in contour
        auto limit = outline.pts.data + last;

        //Skip empty points
        if (last <= first) {
            first = last + 1;
            continue;
        }

        auto start = outline.pts[first];
        auto pt = outline.pts.data + first;
        auto types = outline.types.data + first;
        auto type = types[0];

        //A contour cannot start with a cubic control point
        if (type == SW_CURVE_TYPE_CUBIC) return false;
        ++types;

        auto closed =  outline.closed.data ? outline.closed.data[i]: false;

        _beginSubPath(*stroke, start, closed);

        while (pt < limit) {
            //emit a single line_to
            if (types[0] == SW_CURVE_TYPE_POINT) {
                ++pt;
                ++types;
                _lineTo(*stroke, *pt);
            //types cubic
            } else {
                pt += 3;
                types += 3;
                if (pt <= limit) _cubicTo(*stroke, pt[-2], pt[-1], pt[0]);
                else if (pt - 1 == limit) _cubicTo(*stroke, pt[-2], pt[-1], start);
                else goto close;
            }
        }
    close:
        if (!stroke->firstPt) _endSubPath(*stroke);
        first = last + 1;
    }
    return true;
}


SwOutline* strokeExportOutline(SwStroke* stroke, SwMpool* mpool, unsigned tid)
{
    uint32_t count1, count2, count3, count4;

    _getCounts(stroke->borders + 0, count1, count2);
    _getCounts(stroke->borders + 1, count3, count4);

    auto ptsCnt = count1 + count3;
    auto cntrsCnt = count2 + count4;

    auto outline = mpoolReqStrokeOutline(mpool, tid);
    outline->pts.reserve(ptsCnt);
    outline->types.reserve(ptsCnt);
    outline->cntrs.reserve(cntrsCnt);

    _exportBorderOutline(*stroke, outline, 0);  //left
    _exportBorderOutline(*stroke, outline, 1);  //right

    return outline;
}

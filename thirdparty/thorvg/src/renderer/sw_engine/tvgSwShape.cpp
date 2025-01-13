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

#include "tvgSwCommon.h"
#include "tvgMath.h"

/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/

static bool _outlineBegin(SwOutline& outline)
{
    //Make a contour if lineTo/curveTo without calling close or moveTo beforehand.
    if (outline.pts.empty()) return false;
    outline.cntrs.push(outline.pts.count - 1);
    outline.closed.push(false);
    outline.pts.push(outline.pts[outline.cntrs.last()]);
    outline.types.push(SW_CURVE_TYPE_POINT);
    return false;
}


static bool _outlineEnd(SwOutline& outline)
{
    if (outline.pts.empty()) return false;
    outline.cntrs.push(outline.pts.count - 1);
    outline.closed.push(false);
    return false;
}


static bool _outlineMoveTo(SwOutline& outline, const Point* to, const Matrix& transform, bool closed = false)
{
    //make it a contour, if the last contour is not closed yet.
    if (!closed) _outlineEnd(outline);

    outline.pts.push(mathTransform(to, transform));
    outline.types.push(SW_CURVE_TYPE_POINT);
    return false;
}


static void _outlineLineTo(SwOutline& outline, const Point* to, const Matrix& transform)
{
    outline.pts.push(mathTransform(to, transform));
    outline.types.push(SW_CURVE_TYPE_POINT);
}


static void _outlineCubicTo(SwOutline& outline, const Point* ctrl1, const Point* ctrl2, const Point* to, const Matrix& transform)
{
    outline.pts.push(mathTransform(ctrl1, transform));
    outline.types.push(SW_CURVE_TYPE_CUBIC);

    outline.pts.push(mathTransform(ctrl2, transform));
    outline.types.push(SW_CURVE_TYPE_CUBIC);    

    outline.pts.push(mathTransform(to, transform));
    outline.types.push(SW_CURVE_TYPE_POINT);
}


static bool _outlineClose(SwOutline& outline)
{
    uint32_t i;
    if (outline.cntrs.count > 0) i = outline.cntrs.last() + 1;
    else i = 0;

    //Make sure there is at least one point in the current path
    if (outline.pts.count == i) return false;

    //Close the path
    outline.pts.push(outline.pts[i]);
    outline.cntrs.push(outline.pts.count - 1);
    outline.types.push(SW_CURVE_TYPE_POINT);
    outline.closed.push(true);

    return true;
}


static void _dashLineTo(SwDashStroke& dash, const Point* to, const Matrix& transform)
{
    Line cur = {dash.ptCur, *to};
    auto len = cur.length();

    if (tvg::zero(len)) {
        _outlineMoveTo(*dash.outline, &dash.ptCur, transform);
    //draw the current line fully
    } else if (len <= dash.curLen) {
        dash.curLen -= len;
        if (!dash.curOpGap) {
            if (dash.move) {
                _outlineMoveTo(*dash.outline, &dash.ptCur, transform);
                dash.move = false;
            }
            _outlineLineTo(*dash.outline, to, transform);
        }
    //draw the current line partially
    } else {
        while (len - dash.curLen > 0.0001f) {
            Line left, right;
            if (dash.curLen > 0) {
                len -= dash.curLen;
                cur.split(dash.curLen, left, right);
                if (!dash.curOpGap) {
                    if (dash.move || dash.pattern[dash.curIdx] - dash.curLen < FLOAT_EPSILON) {
                        _outlineMoveTo(*dash.outline, &left.pt1, transform);
                        dash.move = false;
                    }
                    _outlineLineTo(*dash.outline, &left.pt2, transform);
                }
            } else {
                right = cur;
            }
            dash.curIdx = (dash.curIdx + 1) % dash.cnt;
            dash.curLen = dash.pattern[dash.curIdx];
            dash.curOpGap = !dash.curOpGap;
            cur = right;
            dash.ptCur = cur.pt1;
            dash.move = true;
        }
        //leftovers
        dash.curLen -= len;
        if (!dash.curOpGap) {
            if (dash.move) {
                _outlineMoveTo(*dash.outline, &cur.pt1, transform);
                dash.move = false;
            }
            _outlineLineTo(*dash.outline, &cur.pt2, transform);
        }
        if (dash.curLen < 1 && TO_SWCOORD(len) > 1) {
            //move to next dash
            dash.curIdx = (dash.curIdx + 1) % dash.cnt;
            dash.curLen = dash.pattern[dash.curIdx];
            dash.curOpGap = !dash.curOpGap;
        }
    }
    dash.ptCur = *to;
}


static void _dashCubicTo(SwDashStroke& dash, const Point* ctrl1, const Point* ctrl2, const Point* to, const Matrix& transform)
{
    Bezier cur = {dash.ptCur, *ctrl1, *ctrl2, *to};
    auto len = cur.length();

    //draw the current line fully
    if (tvg::zero(len)) {
        _outlineMoveTo(*dash.outline, &dash.ptCur, transform);
    } else if (len <= dash.curLen) {
        dash.curLen -= len;
        if (!dash.curOpGap) {
            if (dash.move) {
                _outlineMoveTo(*dash.outline, &dash.ptCur, transform);
                dash.move = false;
            }
            _outlineCubicTo(*dash.outline, ctrl1, ctrl2, to, transform);
        }
    //draw the current line partially
    } else {
        while ((len - dash.curLen) > 0.0001f) {
            Bezier left, right;
            if (dash.curLen > 0) {
                len -= dash.curLen;
                cur.split(dash.curLen, left, right);
                if (!dash.curOpGap) {
                    if (dash.move || dash.pattern[dash.curIdx] - dash.curLen < FLOAT_EPSILON) {
                        _outlineMoveTo(*dash.outline, &left.start, transform);
                        dash.move = false;
                    }
                    _outlineCubicTo(*dash.outline, &left.ctrl1, &left.ctrl2, &left.end, transform);
                }
            } else {
                right = cur;
            }
            dash.curIdx = (dash.curIdx + 1) % dash.cnt;
            dash.curLen = dash.pattern[dash.curIdx];
            dash.curOpGap = !dash.curOpGap;
            cur = right;
            dash.ptCur = right.start;
            dash.move = true;
        }
        //leftovers
        dash.curLen -= len;
        if (!dash.curOpGap) {
            if (dash.move) {
                _outlineMoveTo(*dash.outline, &cur.start, transform);
                dash.move = false;
            }
            _outlineCubicTo(*dash.outline, &cur.ctrl1, &cur.ctrl2, &cur.end, transform);
        }
        if (dash.curLen < 0.1f && TO_SWCOORD(len) > 1) {
            //move to next dash
            dash.curIdx = (dash.curIdx + 1) % dash.cnt;
            dash.curLen = dash.pattern[dash.curIdx];
            dash.curOpGap = !dash.curOpGap;
        }
    }
    dash.ptCur = *to;
}


static void _dashClose(SwDashStroke& dash, const Matrix& transform)
{
    _dashLineTo(dash, &dash.ptStart, transform);
}


static void _dashMoveTo(SwDashStroke& dash, const Point* pts)
{
    dash.ptCur = *pts;
    dash.ptStart = *pts;
    dash.move = true;
}


static void _dashMoveTo(SwDashStroke& dash, uint32_t offIdx, float offset, const Point* pts)
{
    dash.curIdx = offIdx % dash.cnt;
    dash.curLen = dash.pattern[dash.curIdx] - offset;
    dash.curOpGap = offIdx % 2;
    dash.ptStart = dash.ptCur = *pts;
    dash.move = true;
}


static void _trimPattern(SwDashStroke* dash, const RenderShape* rshape, float length, float trimBegin, float trimEnd)
{
    auto begin = length * trimBegin;
    auto end = length * trimEnd;

    //default
    if (end > begin) {
        if (begin > 0.0f) dash->cnt = 4;
        else dash->cnt = 2;
        //looping
    } else dash->cnt = 3;

    if (dash->cnt == 2) {
        dash->pattern[0] = end - begin;
        dash->pattern[1] = length - (end - begin);
    } else if (dash->cnt == 3) {
        dash->pattern[0] = end;
        dash->pattern[1] = (begin - end);
        dash->pattern[2] = length - begin;
    } else {
        dash->pattern[0] = 0;     //zero dash to start with a space.
        dash->pattern[1] = begin;
        dash->pattern[2] = end - begin;
        dash->pattern[3] = length - end;
    }
}


static float _outlineLength(const RenderShape* rshape, uint32_t shiftPts, uint32_t shiftCmds, bool subpath)
{
    const PathCommand* cmds = rshape->path.cmds.data + shiftCmds;
    auto cmdCnt = rshape->path.cmds.count - shiftCmds;
    const Point* pts = rshape->path.pts.data + shiftPts;
    auto ptsCnt = rshape->path.pts.count - shiftPts;

    //No actual shape data
    if (cmdCnt <= 0 || ptsCnt <= 0) return 0.0f;

    const Point* close = nullptr;
    auto len = 0.0f;

    //must begin with moveTo
    if (cmds[0] == PathCommand::MoveTo) {
        close = pts;
        cmds++;
        pts++;
        cmdCnt--;
    }

    while (cmdCnt-- > 0) {
        switch (*cmds) {
            case PathCommand::Close: {
                len += length(pts - 1, close);
                if (subpath) return len;
                break;
            }
            case PathCommand::MoveTo: {
                if (subpath) return len;
                close = pts;
                ++pts;
                break;
            }
            case PathCommand::LineTo: {
                len += length(pts - 1, pts);
                ++pts;
                break;
            }
            case PathCommand::CubicTo: {
                len += Bezier{*(pts - 1), *pts, *(pts + 1), *(pts + 2)}.length();
                pts += 3;
                break;
            }
        }
        ++cmds;
    }
    return len;
}


static SwOutline* _genDashOutline(const RenderShape* rshape, const Matrix& transform, bool trimmed, SwMpool* mpool, unsigned tid)
{
    const PathCommand* cmds = rshape->path.cmds.data;
    auto cmdCnt = rshape->path.cmds.count;
    const Point* pts = rshape->path.pts.data;
    auto ptsCnt = rshape->path.pts.count;

    //No actual shape data
    if (cmdCnt == 0 || ptsCnt == 0) return nullptr;

    auto startPts = pts;
    auto startCmds = cmds;

    SwDashStroke dash;
    auto offset = 0.0f;
    dash.cnt = rshape->strokeDash((const float**)&dash.pattern, &offset);
    auto simultaneous = rshape->stroke->trim.simultaneous;
    float trimBegin = 0.0f, trimEnd = 1.0f;
    if (trimmed) rshape->stroke->strokeTrim(trimBegin, trimEnd);

    if (dash.cnt == 0) {
        if (trimmed) dash.pattern = (float*)malloc(sizeof(float) * 4);
        else return nullptr;
    } else {
        //TODO: handle dash + trim - for now trimming ignoring is forced
        trimmed = false;
    }

    //offset
    auto patternLength = 0.0f;
    uint32_t offIdx = 0;
    if (!tvg::zero(offset)) {
        for (size_t i = 0; i < dash.cnt; ++i) patternLength += dash.pattern[i];
        bool isOdd = dash.cnt % 2;
        if (isOdd) patternLength *= 2;

        offset = fmodf(offset, patternLength);
        if (offset < 0) offset += patternLength;

        for (size_t i = 0; i < dash.cnt * (1 + (size_t)isOdd); ++i, ++offIdx) {
            auto curPattern = dash.pattern[i % dash.cnt];
            if (offset < curPattern) break;
            offset -= curPattern;
        }
    }

    dash.outline = mpoolReqDashOutline(mpool, tid);

    //must begin with moveTo
    if (cmds[0] == PathCommand::MoveTo) {
        if (trimmed) _trimPattern(&dash, rshape, _outlineLength(rshape, 0, 0, simultaneous), trimBegin, trimEnd);
        _dashMoveTo(dash, offIdx, offset, pts);
        cmds++;
        pts++;
    }

    while (--cmdCnt > 0) {
        switch (*cmds) {
            case PathCommand::Close: {
                _dashClose(dash, transform);
                break;
            }
            case PathCommand::MoveTo: {
                if (trimmed) {
                    if (simultaneous) {
                        _trimPattern(&dash, rshape, _outlineLength(rshape, pts - startPts, cmds - startCmds, true), trimBegin, trimEnd);
                        _dashMoveTo(dash, offIdx, offset, pts);
                    } else _dashMoveTo(dash, pts);
                } else _dashMoveTo(dash, offIdx, offset, pts);
                ++pts;
                break;
            }
            case PathCommand::LineTo: {
                _dashLineTo(dash, pts, transform);
                ++pts;
                break;
            }
            case PathCommand::CubicTo: {
                _dashCubicTo(dash, pts, pts + 1, pts + 2, transform);
                pts += 3;
                break;
            }
        }
        ++cmds;
    }

    _outlineEnd(*dash.outline);

    if (trimmed) free(dash.pattern);

    return dash.outline;
}


static bool _axisAlignedRect(const SwOutline* outline)
{
    //Fast Track: axis-aligned rectangle?
    if (outline->pts.count != 5) return false;
    if (outline->types[2] == SW_CURVE_TYPE_CUBIC) return false;

    auto pt1 = outline->pts.data + 0;
    auto pt2 = outline->pts.data + 1;
    auto pt3 = outline->pts.data + 2;
    auto pt4 = outline->pts.data + 3;

    auto a = SwPoint{pt1->x, pt3->y};
    auto b = SwPoint{pt3->x, pt1->y};

    if ((*pt2 == a && *pt4 == b) || (*pt2 == b && *pt4 == a)) return true;

    return false;
}


static bool _genOutline(SwShape* shape, const RenderShape* rshape, const Matrix& transform, SwMpool* mpool, unsigned tid, bool hasComposite)
{
    const PathCommand* cmds = rshape->path.cmds.data;
    auto cmdCnt = rshape->path.cmds.count;
    const Point* pts = rshape->path.pts.data;
    auto ptsCnt = rshape->path.pts.count;

    //No actual shape data
    if (cmdCnt == 0 || ptsCnt == 0) return false;

    shape->outline = mpoolReqOutline(mpool, tid);
    auto outline = shape->outline;
    auto closed = false;

    //Generate Outlines
    while (cmdCnt-- > 0) {
        switch (*cmds) {
            case PathCommand::Close: {
                if (!closed) closed = _outlineClose(*outline);
                break;
            }
            case PathCommand::MoveTo: {
                closed = _outlineMoveTo(*outline, pts, transform, closed);
                ++pts;
                break;
            }
            case PathCommand::LineTo: {
                if (closed) closed = _outlineBegin(*outline);
                _outlineLineTo(*outline, pts, transform);
                ++pts;
                break;
            }
            case PathCommand::CubicTo: {
                if (closed) closed = _outlineBegin(*outline);
                _outlineCubicTo(*outline, pts, pts + 1, pts + 2, transform);
                pts += 3;
                break;
            }
        }
        ++cmds;
    }

    if (!closed) _outlineEnd(*outline);

    outline->fillRule = rshape->rule;
    shape->outline = outline;

    shape->fastTrack = (!hasComposite && _axisAlignedRect(shape->outline));
    return true;
}


/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/

bool shapePrepare(SwShape* shape, const RenderShape* rshape, const Matrix& transform,  const SwBBox& clipRegion, SwBBox& renderRegion, SwMpool* mpool, unsigned tid, bool hasComposite)
{
    if (!_genOutline(shape, rshape, transform, mpool, tid, hasComposite)) return false;
    if (!mathUpdateOutlineBBox(shape->outline, clipRegion, renderRegion, shape->fastTrack)) return false;

    shape->bbox = renderRegion;

    //Check valid region
    if (renderRegion.max.x - renderRegion.min.x < 1 && renderRegion.max.y - renderRegion.min.y < 1) return false;

    //Check boundary
    if (renderRegion.min.x >= clipRegion.max.x || renderRegion.min.y >= clipRegion.max.y ||
        renderRegion.max.x <= clipRegion.min.x || renderRegion.max.y <= clipRegion.min.y) return false;

    return true;
}


bool shapePrepared(const SwShape* shape)
{
    return shape->rle ? true : false;
}


bool shapeGenRle(SwShape* shape, TVG_UNUSED const RenderShape* rshape, bool antiAlias)
{
    //FIXME: Should we draw it?
    //Case: Stroke Line
    //if (shape.outline->opened) return true;

    //Case A: Fast Track Rectangle Drawing
    if (shape->fastTrack) return true;

    //Case B: Normal Shape RLE Drawing
    if ((shape->rle = rleRender(shape->rle, shape->outline, shape->bbox, antiAlias))) return true;

    return false;
}


void shapeDelOutline(SwShape* shape, SwMpool* mpool, uint32_t tid)
{
    mpoolRetOutline(mpool, tid);
    shape->outline = nullptr;
}


void shapeReset(SwShape* shape)
{
    rleReset(shape->rle);
    rleReset(shape->strokeRle);
    shape->fastTrack = false;
    shape->bbox.reset();
}


void shapeFree(SwShape* shape)
{
    rleFree(shape->rle);
    shape->rle = nullptr;

    shapeDelFill(shape);

    if (shape->stroke) {
        rleFree(shape->strokeRle);
        shape->strokeRle = nullptr;
        strokeFree(shape->stroke);
        shape->stroke = nullptr;
    }
}


void shapeDelStroke(SwShape* shape)
{
    if (!shape->stroke) return;
    rleFree(shape->strokeRle);
    shape->strokeRle = nullptr;
    strokeFree(shape->stroke);
    shape->stroke = nullptr;
}


void shapeResetStroke(SwShape* shape, const RenderShape* rshape, const Matrix& transform)
{
    if (!shape->stroke) shape->stroke = static_cast<SwStroke*>(calloc(1, sizeof(SwStroke)));
    auto stroke = shape->stroke;
    if (!stroke) return;

    strokeReset(stroke, rshape, transform);
    rleReset(shape->strokeRle);
}


bool shapeGenStrokeRle(SwShape* shape, const RenderShape* rshape, const Matrix& transform, const SwBBox& clipRegion, SwBBox& renderRegion, SwMpool* mpool, unsigned tid)
{
    SwOutline* shapeOutline = nullptr;
    SwOutline* strokeOutline = nullptr;
    auto dashStroking = false;
    auto ret = true;

    //Dash style (+trimming)
    auto trimmed = rshape->strokeTrim();
    if (rshape->stroke->dashCnt > 0 || trimmed) {
        shapeOutline = _genDashOutline(rshape, transform, trimmed, mpool, tid);
        if (!shapeOutline) return false;
        dashStroking = true;
    //Normal style
    } else {
        if (!shape->outline) {
            if (!_genOutline(shape, rshape, transform, mpool, tid, false)) return false;
        }
        shapeOutline = shape->outline;
    }

    if (!strokeParseOutline(shape->stroke, *shapeOutline)) {
        ret = false;
        goto clear;
    }

    strokeOutline = strokeExportOutline(shape->stroke, mpool, tid);

    if (!mathUpdateOutlineBBox(strokeOutline, clipRegion, renderRegion, false)) {
        ret = false;
        goto clear;
    }

    shape->strokeRle = rleRender(shape->strokeRle, strokeOutline, renderRegion, true);

clear:
    if (dashStroking) mpoolRetDashOutline(mpool, tid);
    mpoolRetStrokeOutline(mpool, tid);

    return ret;
}


bool shapeGenFillColors(SwShape* shape, const Fill* fill, const Matrix& transform, SwSurface* surface, uint8_t opacity, bool ctable)
{
    return fillGenColorTable(shape->fill, fill, transform, surface, opacity, ctable);
}


bool shapeGenStrokeFillColors(SwShape* shape, const Fill* fill, const Matrix& transform, SwSurface* surface, uint8_t opacity, bool ctable)
{
    return fillGenColorTable(shape->stroke->fill, fill, transform, surface, opacity, ctable);
}


void shapeResetFill(SwShape* shape)
{
    if (!shape->fill) {
        shape->fill = static_cast<SwFill*>(calloc(1, sizeof(SwFill)));
        if (!shape->fill) return;
    }
    fillReset(shape->fill);
}


void shapeResetStrokeFill(SwShape* shape)
{
    if (!shape->stroke->fill) {
        shape->stroke->fill = static_cast<SwFill*>(calloc(1, sizeof(SwFill)));
        if (!shape->stroke->fill) return;
    }
    fillReset(shape->stroke->fill);
}


void shapeDelFill(SwShape* shape)
{
    if (!shape->fill) return;
    fillFree(shape->fill);
    shape->fill = nullptr;
}


void shapeDelStrokeFill(SwShape* shape)
{
    if (!shape->stroke->fill) return;
    fillFree(shape->stroke->fill);
    shape->stroke->fill = nullptr;
}

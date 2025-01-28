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

/*
 *                   The FreeType Project LICENSE
 *                   ----------------------------

 *                           2006-Jan-27

 *                   Copyright 1996-2002, 2006 by
 *         David Turner, Robert Wilhelm, and Werner Lemberg



 * Introduction
 * ============

 * The FreeType  Project is distributed in  several archive packages;
 * some of them may contain, in addition to the FreeType font engine,
 * various tools and  contributions which rely on, or  relate to, the
 * FreeType Project.

 * This  license applies  to all  files found  in such  packages, and
 * which do not  fall under their own explicit  license.  The license
 * affects  thus  the  FreeType   font  engine,  the  test  programs,
 * documentation and makefiles, at the very least.

 * This  license   was  inspired  by  the  BSD,   Artistic,  and  IJG
 * (Independent JPEG  Group) licenses, which  all encourage inclusion
 * and  use of  free  software in  commercial  and freeware  products
 * alike.  As a consequence, its main points are that:

 *   o We don't promise that this software works. However, we will be
 *     interested in any kind of bug reports. (`as is' distribution)

 *   o You can  use this software for whatever you  want, in parts or
 *      full form, without having to pay us. (`royalty-free' usage)

 *    o You may not pretend that  you wrote this software.  If you use
 *      it, or  only parts of it,  in a program,  you must acknowledge
 *     somewhere  in  your  documentation  that  you  have  used  the
 *     FreeType code. (`credits')

 * We  specifically  permit  and  encourage  the  inclusion  of  this
 * software, with  or without modifications,  in commercial products.
 * We  disclaim  all warranties  covering  The  FreeType Project  and
 * assume no liability related to The FreeType Project.


 *  Finally,  many  people  asked  us  for  a  preferred  form  for  a
 *  credit/disclaimer to use in compliance with this license.  We thus
 * encourage you to use the following text:

 *   """
 *    Portions of this software are copyright � <year> The FreeType
 *    Project (www.freetype.org).  All rights reserved.
 *   """

 *  Please replace <year> with the value from the FreeType version you
 *  actually use.

* Legal Terms
* ===========

* 0. Definitions
* --------------

*   Throughout this license,  the terms `package', `FreeType Project',
*   and  `FreeType  archive' refer  to  the  set  of files  originally
*   distributed  by the  authors  (David Turner,  Robert Wilhelm,  and
*   Werner Lemberg) as the `FreeType Project', be they named as alpha,
*   beta or final release.

*   `You' refers to  the licensee, or person using  the project, where
*   `using' is a generic term including compiling the project's source
*   code as  well as linking it  to form a  `program' or `executable'.
*   This  program is  referred to  as  `a program  using the  FreeType
*   engine'.

*   This  license applies  to all  files distributed  in  the original
*   FreeType  Project,   including  all  source   code,  binaries  and
*   documentation,  unless  otherwise  stated   in  the  file  in  its
*   original, unmodified form as  distributed in the original archive.
*   If you are  unsure whether or not a particular  file is covered by
*   this license, you must contact us to verify this.

*   The FreeType  Project is copyright (C) 1996-2000  by David Turner,
*   Robert Wilhelm, and Werner Lemberg.  All rights reserved except as
*   specified below.

* 1. No Warranty
* --------------

*   THE FREETYPE PROJECT  IS PROVIDED `AS IS' WITHOUT  WARRANTY OF ANY
*   KIND, EITHER  EXPRESS OR IMPLIED,  INCLUDING, BUT NOT  LIMITED TO,
*   WARRANTIES  OF  MERCHANTABILITY   AND  FITNESS  FOR  A  PARTICULAR
*   PURPOSE.  IN NO EVENT WILL ANY OF THE AUTHORS OR COPYRIGHT HOLDERS
*   BE LIABLE  FOR ANY DAMAGES CAUSED  BY THE USE OR  THE INABILITY TO
*   USE, OF THE FREETYPE PROJECT.

* 2. Redistribution
* -----------------

*   This  license  grants  a  worldwide, royalty-free,  perpetual  and
*   irrevocable right  and license to use,  execute, perform, compile,
*   display,  copy,   create  derivative  works   of,  distribute  and
*   sublicense the  FreeType Project (in  both source and  object code
*   forms)  and  derivative works  thereof  for  any  purpose; and  to
*   authorize others  to exercise  some or all  of the  rights granted
*   herein, subject to the following conditions:

*    o Redistribution of  source code  must retain this  license file
*      (`FTL.TXT') unaltered; any  additions, deletions or changes to
*      the original  files must be clearly  indicated in accompanying
*      documentation.   The  copyright   notices  of  the  unaltered,
*      original  files must  be  preserved in  all  copies of  source
*      files.

*    o Redistribution in binary form must provide a  disclaimer  that
*      states  that  the software is based in part of the work of the
*      FreeType Team,  in  the  distribution  documentation.  We also
*      encourage you to put an URL to the FreeType web page  in  your
*      documentation, though this isn't mandatory.

*  These conditions  apply to any  software derived from or  based on
*  the FreeType Project,  not just the unmodified files.   If you use
*  our work, you  must acknowledge us.  However, no  fee need be paid
*  to us.

* 3. Advertising
* --------------

*  Neither the  FreeType authors and  contributors nor you  shall use
*  the name of the  other for commercial, advertising, or promotional
*  purposes without specific prior written permission.

*  We suggest,  but do not require, that  you use one or  more of the
*  following phrases to refer  to this software in your documentation
*  or advertising  materials: `FreeType Project',  `FreeType Engine',
*  `FreeType library', or `FreeType Distribution'.

*  As  you have  not signed  this license,  you are  not  required to
*  accept  it.   However,  as  the FreeType  Project  is  copyrighted
*  material, only  this license, or  another one contracted  with the
*  authors, grants you  the right to use, distribute,  and modify it.
*  Therefore,  by  using,  distributing,  or modifying  the  FreeType
*  Project, you indicate that you understand and accept all the terms
*  of this license.

* 4. Contacts
* -----------

*  There are two mailing lists related to FreeType:

*    o freetype@nongnu.org

*      Discusses general use and applications of FreeType, as well as
*      future and  wanted additions to the  library and distribution.
*      If  you are looking  for support,  start in  this list  if you
*      haven't found anything to help you in the documentation.

*    o freetype-devel@nongnu.org

*      Discusses bugs,  as well  as engine internals,  design issues,
*      specific licenses, porting, etc.

*  Our home page can be found at

*    http://www.freetype.org
*/

#include <limits.h>
#include <memory.h>
#include "tvgSwCommon.h"

/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/

constexpr auto PIXEL_BITS = 8;   //must be at least 6 bits!
constexpr auto ONE_PIXEL = (1L << PIXEL_BITS);

using Area = long;

struct Band
{
    SwCoord min, max;
};

struct Cell
{
    SwCoord x;
    SwCoord cover;
    Area area;
    Cell *next;
};

struct RleWorker
{
    SwRle* rle;

    SwPoint cellPos;
    SwPoint cellMin;
    SwPoint cellMax;
    SwCoord cellXCnt;
    SwCoord cellYCnt;

    Area area;
    SwCoord cover;

    Cell* cells;
    ptrdiff_t maxCells;
    ptrdiff_t cellsCnt;

    SwPoint pos;

    SwPoint bezStack[32 * 3 + 1];
    SwPoint lineStack[32 + 1];
    int levStack[32];

    SwOutline* outline;

    int bandSize;
    int bandShoot;

    void* buffer;
    long bufferSize;

    Cell** yCells;
    SwCoord yCnt;

    bool invalid;
    bool antiAlias;
};


static inline SwPoint UPSCALE(const SwPoint& pt)
{
    return {SwCoord(((unsigned long) pt.x) << (PIXEL_BITS - 6)), SwCoord(((unsigned long) pt.y) << (PIXEL_BITS - 6))};
}


static inline SwPoint TRUNC(const SwPoint& pt)
{
    return  {pt.x >> PIXEL_BITS, pt.y >> PIXEL_BITS};
}


static inline SwCoord TRUNC(const SwCoord x)
{
    return  x >> PIXEL_BITS;
}


static inline SwPoint SUBPIXELS(const SwPoint& pt)
{
    return {SwCoord(((unsigned long) pt.x) << PIXEL_BITS), SwCoord(((unsigned long) pt.y) << PIXEL_BITS)};
}


static inline SwCoord SUBPIXELS(const SwCoord x)
{
    return SwCoord(((unsigned long) x) << PIXEL_BITS);
}

/*
 *  Approximate sqrt(x*x+y*y) using the `alpha max plus beta min'
 *  algorithm.  We use alpha = 1, beta = 3/8, giving us results with a
 *  largest error less than 7% compared to the exact value.
 */
static inline SwCoord HYPOT(SwPoint pt)
{
    if (pt.x < 0) pt.x = -pt.x;
    if (pt.y < 0) pt.y = -pt.y;
    return ((pt.x > pt.y) ? (pt.x + (3 * pt.y >> 3)) : (pt.y + (3 * pt.x >> 3)));
}


static void _horizLine(RleWorker& rw, SwCoord x, SwCoord y, SwCoord area, SwCoord aCount)
{
    x += rw.cellMin.x;
    y += rw.cellMin.y;

    //Clip Y range
    if (y < rw.cellMin.y || y >= rw.cellMax.y) return;

    /* compute the coverage line's coverage, depending on the outline fill rule */
    /* the coverage percentage is area/(PIXEL_BITS*PIXEL_BITS*2) */
    auto coverage = static_cast<int>(area >> (PIXEL_BITS * 2 + 1 - 8));    //range 0 - 255

    if (coverage < 0) coverage = -coverage;

    if (rw.outline->fillRule == FillRule::EvenOdd) {
        coverage &= 511;
        if (coverage > 255) coverage = 511 - coverage;
    } else {
        //normal non-zero winding rule
        if (coverage > 255) coverage = 255;
    }

    if (coverage == 0) return;

    //span has ushort coordinates. check limit overflow
    if (x >= SHRT_MAX) {
        TVGERR("SW_ENGINE", "X-coordinate overflow!");
        return;
    }
    if (y >= SHRT_MAX) {
        TVGERR("SW_ENGINE", "Y-coordinate overflow!");
        return;
    }

    auto rle = rw.rle;

    if (!rw.antiAlias) coverage = 255;

    //see whether we can add this span to the current list
    if (rle->size > 0) {
        auto span = rle->spans + rle->size - 1;
        if ((span->coverage == coverage) && (span->y == y) && (span->x + span->len == x)) {
            //Clip x range
            SwCoord xOver = 0;
            if (x + aCount >= rw.cellMax.x) xOver -= (x + aCount - rw.cellMax.x);
            if (x < rw.cellMin.x) xOver -= (rw.cellMin.x - x);

            //span->len += (aCount + xOver) - 1;
            span->len += (aCount + xOver);
            return;
        }
    }

    //span pool is full, grow it.
    if (rle->size >= rle->alloc) {
        auto newSize = (rle->size > 0) ? (rle->size * 2) : 256;
        if (rle->alloc < newSize) {
            rle->alloc = newSize;
            rle->spans = static_cast<SwSpan*>(realloc(rle->spans, rle->alloc * sizeof(SwSpan)));
        }
    }

    //Clip x range
    SwCoord xOver = 0;
    if (x + aCount >= rw.cellMax.x) xOver -= (x + aCount - rw.cellMax.x);
    if (x < rw.cellMin.x) {
        xOver -= (rw.cellMin.x - x);
        x = rw.cellMin.x;
    }

    //Nothing to draw
    if (aCount + xOver <= 0) return;

    //add a span to the current list
    auto span = rle->spans + rle->size;
    span->x = x;
    span->y = y;
    span->len = (aCount + xOver);
    span->coverage = coverage;
    rle->size++;
}


static void _sweep(RleWorker& rw)
{
    if (rw.cellsCnt == 0) return;

    for (int y = 0; y < rw.yCnt; ++y) {
        auto cover = 0;
        auto x = 0;
        auto cell = rw.yCells[y];

        while (cell) {
            if (cell->x > x && cover != 0) _horizLine(rw, x, y, cover * (ONE_PIXEL * 2), cell->x - x);
            cover += cell->cover;
            auto area = cover * (ONE_PIXEL * 2) - cell->area;
            if (area != 0 && cell->x >= 0) _horizLine(rw, cell->x, y, area, 1);
            x = cell->x + 1;
            cell = cell->next;
        }

        if (cover != 0) _horizLine(rw, x, y, cover * (ONE_PIXEL * 2), rw.cellXCnt - x);
    }
}


static Cell* _findCell(RleWorker& rw)
{
    auto x = rw.cellPos.x;
    if (x > rw.cellXCnt) x = rw.cellXCnt;

    auto pcell = &rw.yCells[rw.cellPos.y];

    while(true) {
        Cell* cell = *pcell;
        if (!cell || cell->x > x) break;
        if (cell->x == x) return cell;
        pcell = &cell->next;
    }

    if (rw.cellsCnt >= rw.maxCells) return nullptr;

    auto cell = rw.cells + rw.cellsCnt++;
    cell->x = x;
    cell->area = 0;
    cell->cover = 0;
    cell->next = *pcell;
    *pcell = cell;

    return cell;
}


static bool _recordCell(RleWorker& rw)
{
    if (rw.area | rw.cover) {
        auto cell = _findCell(rw);

        if (cell == nullptr) return false;

        cell->area += rw.area;
        cell->cover += rw.cover;
    }

    return true;
}


static bool _setCell(RleWorker& rw, SwPoint pos)
{
    /* Move the cell pointer to a new position.  We set the `invalid'      */
    /* flag to indicate that the cell isn't part of those we're interested */
    /* in during the render phase.  This means that:                       */
    /*                                                                     */
    /* . the new vertical position must be within min_ey..max_ey-1.        */
    /* . the new horizontal position must be strictly less than max_ex     */
    /*                                                                     */
    /* Note that if a cell is to the left of the clipping region, it is    */
    /* actually set to the (min_ex-1) horizontal position.                 */

    /* All cells that are on the left of the clipping region go to the
       min_ex - 1 horizontal position. */
    pos.x -= rw.cellMin.x;
    pos.y -= rw.cellMin.y;

    //exceptions
    if (pos.x < 0) pos.x = -1;
    else if (pos.x > rw.cellMax.x) pos.x = rw.cellMax.x;

    //Are we moving to a different cell?
    if (pos != rw.cellPos) {
        //Record the current one if it is valid
        if (!rw.invalid) {
            if (!_recordCell(rw)) return false;
        }
        rw.area = rw.cover = 0;
        rw.cellPos = pos;
    }
    rw.invalid = ((unsigned)pos.y >= (unsigned)rw.cellYCnt || pos.x >= rw.cellXCnt);

    return true;
}


static bool _startCell(RleWorker& rw, SwPoint pos)
{
    if (pos.x > rw.cellMax.x) pos.x = rw.cellMax.x;
    if (pos.x < rw.cellMin.x) pos.x = rw.cellMin.x;

    rw.area = 0;
    rw.cover = 0;
    rw.cellPos = pos - rw.cellMin;
    rw.invalid = false;

    return _setCell(rw, pos);
}


static bool _moveTo(RleWorker& rw, const SwPoint& to)
{
    //record current cell, if any */
    if (!rw.invalid) {
        if (!_recordCell(rw)) return false;
    }

    //start to a new position
    if (!_startCell(rw, TRUNC(to))) return false;

    rw.pos = to;

    return true;
}


static bool _lineTo(RleWorker& rw, const SwPoint& to)
{
#define SW_UDIV(a, b) \
    static_cast<SwCoord>(((unsigned long)(a) * (unsigned long)(b)) >> \
    (sizeof(long) * CHAR_BIT - PIXEL_BITS))

    auto e1 = TRUNC(rw.pos);
    auto e2 = TRUNC(to);

    //vertical clipping
    if ((e1.y >= rw.cellMax.y && e2.y >= rw.cellMax.y) || (e1.y < rw.cellMin.y && e2.y < rw.cellMin.y)) {
        rw.pos = to;
        return true;
    }

    auto line = rw.lineStack;
    line[0] = to;
    line[1] = rw.pos;

    while (true) {
        auto diff = line[0] - line[1];
        auto L = HYPOT(diff);

        if (L > SHRT_MAX) {
            mathSplitLine(line);
            ++line;
            continue;
        }
        e1 = TRUNC(line[1]);
        e2 = TRUNC(line[0]);

        auto f1 = line[1] - SUBPIXELS(e1);
        SwPoint f2;

        //inside one cell
        if (e1 == e2) {
            ;
        //any horizontal line
        } else if (diff.y == 0) {
            e1.x = e2.x;
            if (!_setCell(rw, e1)) return false;
        } else if (diff.x == 0) {
            //vertical line up
            if (diff.y > 0) {
                do {
                    f2.y = ONE_PIXEL;
                    rw.cover += (f2.y - f1.y);
                    rw.area += (f2.y - f1.y) * f1.x * 2;
                    f1.y = 0;
                    ++e1.y;
                    if (!_setCell(rw, e1)) return false;
                } while(e1.y != e2.y);
            //vertical line down
            } else {
                do {
                    f2.y = 0;
                    rw.cover += (f2.y - f1.y);
                    rw.area += (f2.y - f1.y) * f1.x * 2;
                    f1.y = ONE_PIXEL;
                    --e1.y;
                    if (!_setCell(rw, e1)) return false;
                } while(e1.y != e2.y);
            }
        //any other line
        } else {
            Area prod = diff.x * f1.y - diff.y * f1.x;

            /* These macros speed up repetitive divisions by replacing them
               with multiplications and right shifts. */
            auto dx_r = static_cast<long>(ULONG_MAX >> PIXEL_BITS) / (diff.x);
            auto dy_r = static_cast<long>(ULONG_MAX >> PIXEL_BITS) / (diff.y);

            /* The fundamental value `prod' determines which side and the  */
            /* exact coordinate where the line exits current cell.  It is  */
            /* also easily updated when moving from one cell to the next.  */
            do {
                auto px = diff.x * ONE_PIXEL;
                auto py = diff.y * ONE_PIXEL;

                //left
                if (prod <= 0 && prod - px > 0) {
                    f2 = {0, SW_UDIV(-prod, -dx_r)};
                    prod -= py;
                    rw.cover += (f2.y - f1.y);
                    rw.area += (f2.y - f1.y) * (f1.x + f2.x);
                    f1 = {ONE_PIXEL, f2.y};
                    --e1.x;
                //up
                } else if (prod - px <= 0 && prod - px + py > 0) {
                    prod -= px;
                    f2 = {SW_UDIV(-prod, dy_r), ONE_PIXEL};
                    rw.cover += (f2.y - f1.y);
                    rw.area += (f2.y - f1.y) * (f1.x + f2.x);
                    f1 = {f2.x, 0};
                    ++e1.y;
                //right
                } else if (prod - px + py <= 0 && prod + py >= 0) {
                    prod += py;
                    f2 = {ONE_PIXEL, SW_UDIV(prod, dx_r)};
                    rw.cover += (f2.y - f1.y);
                    rw.area += (f2.y - f1.y) * (f1.x + f2.x);
                    f1 = {0, f2.y};
                    ++e1.x;
                //down
                } else {
                    f2 = {SW_UDIV(prod, -dy_r), 0};
                    prod += px;
                    rw.cover += (f2.y - f1.y);
                    rw.area += (f2.y - f1.y) * (f1.x + f2.x);
                    f1 = {f2.x, ONE_PIXEL};
                    --e1.y;
                }

                if (!_setCell(rw, e1)) return false;

            } while(e1 != e2);
        }

        f2 = {line[0].x - SUBPIXELS(e2.x), line[0].y - SUBPIXELS(e2.y)};
        rw.cover += (f2.y - f1.y);
        rw.area += (f2.y - f1.y) * (f1.x + f2.x);
        rw.pos = line[0];

        if (line-- == rw.lineStack) return true;
    }
}


static bool _cubicTo(RleWorker& rw, const SwPoint& ctrl1, const SwPoint& ctrl2, const SwPoint& to)
{
    auto arc = rw.bezStack;
    arc[0] = to;
    arc[1] = ctrl2;
    arc[2] = ctrl1;
    arc[3] = rw.pos;

    //Short-cut the arc that crosses the current band
    auto min = arc[0].y;
    auto max = arc[0].y;

    SwCoord y;
    for (auto i = 1; i < 4; ++i) {
        y = arc[i].y;
        if (y < min) min = y;
        if (y > max) max = y;
    }

    if (TRUNC(min) >= rw.cellMax.y || TRUNC(max) < rw.cellMin.y) goto draw;

    /* Decide whether to split or draw. See `Rapid Termination          */
    /* Evaluation for Recursive Subdivision of Bezier Curves' by Thomas */
    /* F. Hain, at                                                      */
    /* http://www.cis.southalabama.edu/~hain/general/Publications/Bezier/Camera-ready%20CISST02%202.pdf */
    while (true) {
        {
            //diff is the P0 - P3 chord vector
            auto diff = arc[3] - arc[0];
            auto L = HYPOT(diff);

            //avoid possible arithmetic overflow below by splitting
            if (L > SHRT_MAX) goto split;

            //max deviation may be as much as (s/L) * 3/4 (if Hain's v = 1)
            auto sLimit = L * (ONE_PIXEL / 6);

            auto diff1 = arc[1] - arc[0];
            auto s = diff.y * diff1.x - diff.x * diff1.y;
            if (s < 0) s = -s;
            if (s > sLimit) goto split;

            //s is L * the perpendicular distance from P2 to the line P0 - P3
            auto diff2 = arc[2] - arc[0];
            s = diff.y * diff2.x - diff.x * diff2.y;
            if (s < 0) s = -s;
            if (s > sLimit) goto split;

            /* Split super curvy segments where the off points are so far
            from the chord that the angles P0-P1-P3 or P0-P2-P3 become
            acute as detected by appropriate dot products */
            if (diff1.x * (diff1.x - diff.x) + diff1.y * (diff1.y - diff.y) > 0 ||
                diff2.x * (diff2.x - diff.x) + diff2.y * (diff2.y - diff.y) > 0)
                goto split;

            //no reason to split
            goto draw;
        }
    split:
        mathSplitCubic(arc);
        arc += 3;
        continue;

    draw:
        if (!_lineTo(rw, arc[0])) return false;
        if (arc == rw.bezStack) return true;
        arc -= 3;
    }
}


static bool _decomposeOutline(RleWorker& rw)
{
    auto outline = rw.outline;
    auto first = 0;  //index of first point in contour

    for (auto cntr = outline->cntrs.begin(); cntr < outline->cntrs.end(); ++cntr) {
        auto last = *cntr;
        auto limit = outline->pts.data + last;
        auto start = UPSCALE(outline->pts[first]);
        auto pt = outline->pts.data + first;
        auto types = outline->types.data + first;
        ++types;

        if (!_moveTo(rw, UPSCALE(outline->pts[first]))) return false;

        while (pt < limit) {
            //emit a single line_to
            if (types[0] == SW_CURVE_TYPE_POINT) {
                ++pt;
                ++types;
                if (!_lineTo(rw, UPSCALE(*pt))) return false;
            //types cubic
            } else {
                pt += 3;
                types += 3;
                if (pt <= limit) {
                    if (!_cubicTo(rw, UPSCALE(pt[-2]), UPSCALE(pt[-1]), UPSCALE(pt[0]))) return false;
                }
                else if (pt - 1 == limit) {
                    if (!_cubicTo(rw, UPSCALE(pt[-2]), UPSCALE(pt[-1]), start)) return false;
                }
                else goto close;
            }
        }
    close:
        if (!_lineTo(rw, start)) return false;
       first = last + 1;
    }

    return true;
}


static int _genRle(RleWorker& rw)
{
    if (!_decomposeOutline(rw)) return -1;
    if (!rw.invalid) {
        if (!_recordCell(rw)) return -1;
    }
    return 0;
}


static SwSpan* _intersectSpansRegion(const SwRle *clip, const SwRle *target, SwSpan *outSpans, uint32_t outSpansCnt)
{
    auto out = outSpans;
    auto spans = target->spans;
    auto end = target->spans + target->size;
    auto clipSpans = clip->spans;
    auto clipEnd = clip->spans + clip->size;

    while (spans < end && clipSpans < clipEnd) {
        //align y-coordinates.
        if (clipSpans->y > spans->y) {
            ++spans;
            continue;
        }
        if (spans->y > clipSpans->y) {
            ++clipSpans;
            continue;
        }

        //Try clipping with all clip spans which have a same y-coordinate.
        auto temp = clipSpans;
        while(temp < clipEnd && outSpansCnt > 0 && temp->y == clipSpans->y) {
            auto sx1 = spans->x;
            auto sx2 = sx1 + spans->len;
            auto cx1 = temp->x;
            auto cx2 = cx1 + temp->len;

            //The span must be left(x1) to right(x2) direction. Not intersected.
            if (cx2 < sx1 || sx2 < cx1) {
                ++temp;
                continue;
            }

            //clip span region.
            auto x = sx1 > cx1 ? sx1 : cx1;
            auto len = (sx2 < cx2 ? sx2 : cx2) - x;
            if (len > 0) {
                out->x = x;
                out->y = temp->y;
                out->len = len;
                out->coverage = (uint8_t)(((spans->coverage * temp->coverage) + 0xff) >> 8);
                ++out;
                --outSpansCnt;
            }
            ++temp;
        }
        ++spans;
    }
    return out;
}


static SwSpan* _intersectSpansRect(const SwBBox *bbox, const SwRle *targetRle, SwSpan *outSpans, uint32_t outSpansCnt)
{
    auto out = outSpans;
    auto spans = targetRle->spans;
    auto end = targetRle->spans + targetRle->size;
    auto minx = static_cast<int16_t>(bbox->min.x);
    auto miny = static_cast<int16_t>(bbox->min.y);
    auto maxx = minx + static_cast<int16_t>(bbox->max.x - bbox->min.x) - 1;
    auto maxy = miny + static_cast<int16_t>(bbox->max.y - bbox->min.y) - 1;

    while (outSpansCnt > 0 && spans < end) {
        if (spans->y > maxy) {
            spans = end;
            break;
        }
        if (spans->y < miny || spans->x > maxx || spans->x + spans->len <= minx) {
            ++spans;
            continue;
        }
        if (spans->x < minx) {
            out->len = (spans->len - (minx - spans->x)) < (maxx - minx + 1) ? (spans->len - (minx - spans->x)) : (maxx - minx + 1);
            out->x = minx;
        }
        else {
            out->x = spans->x;
            out->len = spans->len < (maxx - spans->x + 1) ? spans->len : (maxx - spans->x + 1);
        }
        if (out->len > 0) {
            out->y = spans->y;
            out->coverage = spans->coverage;
            ++out;
            --outSpansCnt;
        }
        ++spans;
    }
    return out;
}


void _replaceClipSpan(SwRle *rle, SwSpan* clippedSpans, uint32_t size)
{
    free(rle->spans);
    rle->spans = clippedSpans;
    rle->size = rle->alloc = size;
}


/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/

SwRle* rleRender(SwRle* rle, const SwOutline* outline, const SwBBox& renderRegion, bool antiAlias)
{
    constexpr auto RENDER_POOL_SIZE = 16384L;
    constexpr auto BAND_SIZE = 40;

    //TODO: We can preserve several static workers in advance
    RleWorker rw;
    Cell buffer[RENDER_POOL_SIZE / sizeof(Cell)];

    //Init Cells
    rw.buffer = buffer;
    rw.bufferSize = sizeof(buffer);
    rw.yCells = reinterpret_cast<Cell**>(buffer);
    rw.cells = nullptr;
    rw.maxCells = 0;
    rw.cellsCnt = 0;
    rw.area = 0;
    rw.cover = 0;
    rw.invalid = true;
    rw.cellMin = renderRegion.min;
    rw.cellMax = renderRegion.max;
    rw.cellXCnt = rw.cellMax.x - rw.cellMin.x;
    rw.cellYCnt = rw.cellMax.y - rw.cellMin.y;
    rw.outline = const_cast<SwOutline*>(outline);
    rw.bandSize = rw.bufferSize / (sizeof(Cell) * 2);  //bandSize: 256
    rw.bandShoot = 0;
    rw.antiAlias = antiAlias;

    if (!rle) rw.rle = reinterpret_cast<SwRle*>(calloc(1, sizeof(SwRle)));
    else rw.rle = rle;

    //Generate RLE
    Band bands[BAND_SIZE];
    Band* band;

    /* set up vertical bands */
    auto bandCnt = static_cast<int>((rw.cellMax.y - rw.cellMin.y) / rw.bandSize);
    if (bandCnt == 0) bandCnt = 1;
    else if (bandCnt >= BAND_SIZE) bandCnt = (BAND_SIZE - 1);

    auto min = rw.cellMin.y;
    auto yMax = rw.cellMax.y;
    SwCoord max;
    int ret;

    for (int n = 0; n < bandCnt; ++n, min = max) {
        max = min + rw.bandSize;
        if (n == bandCnt -1 || max > yMax) max = yMax;

        bands[0].min = min;
        bands[0].max = max;
        band = bands;

        while (band >= bands) {
            rw.yCells = static_cast<Cell**>(rw.buffer);
            rw.yCnt = band->max - band->min;

            int cellStart = sizeof(Cell*) * (int)rw.yCnt;
            int cellMod = cellStart % sizeof(Cell);

            if (cellMod > 0) cellStart += sizeof(Cell) - cellMod;

            auto cellsMax = reinterpret_cast<Cell*>((char*)rw.buffer + rw.bufferSize);
            rw.cells = reinterpret_cast<Cell*>((char*)rw.buffer + cellStart);

            if (rw.cells >= cellsMax) goto reduce_bands;

            rw.maxCells = cellsMax - rw.cells;
            if (rw.maxCells < 2) goto reduce_bands;

            for (int y = 0; y < rw.yCnt; ++y)
                rw.yCells[y] = nullptr;

            rw.cellsCnt = 0;
            rw.invalid = true;
            rw.cellMin.y = band->min;
            rw.cellMax.y = band->max;
            rw.cellYCnt = band->max - band->min;

            ret = _genRle(rw);
            if (ret == 0) {
                _sweep(rw);
                --band;
                continue;
            } else if (ret == 1) {
                goto error;
            }

        reduce_bands:
            /* render pool overflow: we will reduce the render band by half */
            auto bottom = band->min;
            auto top = band->max;
            auto middle = bottom + ((top - bottom) >> 1);

            /* This is too complex for a single scanline; there must
               be some problems */
            if (middle == bottom) goto error;

            if (bottom - top >= rw.bandSize) ++rw.bandShoot;

            band[1].min = bottom;
            band[1].max = middle;
            band[0].min = middle;
            band[0].max = top;
            ++band;
        }
    }

    if (rw.bandShoot > 8 && rw.bandSize > 16)
        rw.bandSize = (rw.bandSize >> 1);

    return rw.rle;

error:
    free(rw.rle);
    return nullptr;
}


SwRle* rleRender(const SwBBox* bbox)
{
    auto width = static_cast<uint16_t>(bbox->max.x - bbox->min.x);
    auto height = static_cast<uint16_t>(bbox->max.y - bbox->min.y);

    auto rle = static_cast<SwRle*>(malloc(sizeof(SwRle)));
    rle->spans = static_cast<SwSpan*>(malloc(sizeof(SwSpan) * height));
    rle->size = height;
    rle->alloc = height;

    auto span = rle->spans;
    for (uint16_t i = 0; i < height; ++i, ++span) {
        span->x = bbox->min.x;
        span->y = bbox->min.y + i;
        span->len = width;
        span->coverage = 255;
    }

    return rle;
}


void rleReset(SwRle* rle)
{
    if (!rle) return;
    rle->size = 0;
}


void rleFree(SwRle* rle)
{
    if (!rle) return;
    if (rle->spans) free(rle->spans);
    free(rle);
}


bool rleClip(SwRle *rle, const SwRle *clip)
{
    if (rle->size == 0 || clip->size == 0) return false;
    auto spanCnt = rle->size > clip->size ? rle->size : clip->size;
    auto spans = static_cast<SwSpan*>(malloc(sizeof(SwSpan) * (spanCnt)));
    auto spansEnd = _intersectSpansRegion(clip, rle, spans, spanCnt);

    _replaceClipSpan(rle, spans, spansEnd - spans);

    TVGLOG("SW_ENGINE", "Using Path Clipping!");

    return true;
}


bool rleClip(SwRle *rle, const SwBBox* clip)
{
    if (rle->size == 0) return false;
    auto spans = static_cast<SwSpan*>(malloc(sizeof(SwSpan) * (rle->size)));
    auto spansEnd = _intersectSpansRect(clip, rle, spans, rle->size);

    _replaceClipSpan(rle, spans, spansEnd - spans);

    TVGLOG("SW_ENGINE", "Using Box Clipping!");

    return true;
}


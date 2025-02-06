/*
 * Copyright (c) 2024 the ThorVG project. All rights reserved.

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

#ifndef _TVG_LOTTIE_MODIFIER_H_
#define _TVG_LOTTIE_MODIFIER_H_

#include "tvgCommon.h"
#include "tvgArray.h"
#include "tvgMath.h"


struct LottieRoundnessModifier
{
    static constexpr float ROUNDNESS_EPSILON = 1.0f;
    float r;

    LottieRoundnessModifier(float r) : r(r) {};

    bool modifyPath(const PathCommand* inCmds, uint32_t inCmdsCnt, const Point* inPts, uint32_t inPtsCnt, Array<PathCommand>& outCmds, Array<Point>& outPts, Matrix* transform) const;
    bool modifyPolystar(const Array<PathCommand>& inCmds, const Array<Point>& inPts, Array<PathCommand>& outCmds, Array<Point>& outPts, float outerRoundness, bool hasRoundness) const;
    bool modifyRect(const Point& size, float& r) const;
};


struct LottieOffsetModifier
{
    float offset;
    float miterLimit;
    StrokeJoin join;

    LottieOffsetModifier(float offset, float miter = 4.0f, StrokeJoin join = StrokeJoin::Round) : offset(offset), miterLimit(miter), join(join) {};

    bool modifyPath(const PathCommand* inCmds, uint32_t inCmdsCnt, const Point* inPts, uint32_t inPtsCnt, Array<PathCommand>& outCmds, Array<Point>& outPts) const;
    bool modifyPolystar(const Array<PathCommand>& inCmds, const Array<Point>& inPts, Array<PathCommand>& outCmds, Array<Point>& outPts) const;
    bool modifyRect(const PathCommand* inCmds, uint32_t inCmdsCnt, const Point* inPts, uint32_t inPtsCnt, Array<PathCommand>& outCmds, Array<Point>& outPts) const;
    bool modifyEllipse(float& rx, float& ry) const;

private:
    struct State
    {
        Line line{};
        Line firstLine{};
        bool moveto = false;
        uint32_t movetoOutIndex = 0;
        uint32_t movetoInIndex = 0;
    };

    void line(const PathCommand* inCmds, uint32_t inCmdsCnt, const Point* inPts, uint32_t& currentPt, uint32_t currentCmd, State& state, bool degenerated, Array<PathCommand>& cmds, Array<Point>& pts, float offset) const;
    void corner(const Line& line, const Line& nextLine, uint32_t movetoIndex, bool nextClose, Array<PathCommand>& cmds, Array<Point>& pts) const;
};

#endif

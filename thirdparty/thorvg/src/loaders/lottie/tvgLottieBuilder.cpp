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

#include <cstring>
#include <algorithm>

#include "tvgCommon.h"
#include "tvgMath.h"
#include "tvgLottieModel.h"
#include "tvgLottieBuilder.h"
#include "tvgLottieExpressions.h"


/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/

static bool _buildComposition(LottieComposition* comp, LottieLayer* parent);
static bool _draw(LottieGroup* parent, LottieShape* shape, RenderContext* ctx);


static void _rotationXYZ(Matrix* m, float degreeX, float degreeY, float degreeZ)
{
    auto radianX = deg2rad(degreeX);
    auto radianY = deg2rad(degreeY);
    auto radianZ = deg2rad(degreeZ);

    auto cx = cosf(radianX), sx = sinf(radianX);
    auto cy = cosf(radianY), sy = sinf(radianY);;
    auto cz = cosf(radianZ), sz = sinf(radianZ);;
    m->e11 = cy * cz;
    m->e12 = -cy * sz;
    m->e21 = sx * sy * cz + cx * sz;
    m->e22 = -sx * sy * sz + cx * cz;
}


static void _rotationZ(Matrix* m, float degree)
{
    if (degree == 0.0f) return;
    auto radian = deg2rad(degree);
    m->e11 = cosf(radian);
    m->e12 = -sinf(radian);
    m->e21 = sinf(radian);
    m->e22 = cosf(radian);
}


static void _skew(Matrix* m, float angleDeg, float axisDeg)
{
    auto angle = -deg2rad(angleDeg);
    float tanVal = tanf(angle);

    axisDeg = fmod(axisDeg, 180.0f);
    if (fabsf(axisDeg) < 0.01f || fabsf(axisDeg - 180.0f) < 0.01f || fabsf(axisDeg + 180.0f) < 0.01f) {
        float cosVal = cosf(deg2rad(axisDeg));
        auto B = cosVal * cosVal * tanVal;
        m->e12 += B * m->e11;
        m->e22 += B * m->e21;
        return;
    } else if (fabsf(axisDeg - 90.0f) < 0.01f || fabsf(axisDeg + 90.0f) < 0.01f) {
        float sinVal = -sinf(deg2rad(axisDeg));
        auto C = sinVal * sinVal * tanVal;
        m->e11 -= C * m->e12;
        m->e21 -= C * m->e22;
        return;
    }

    auto axis = -deg2rad(axisDeg);
    float cosVal = cosf(axis);
    float sinVal = sinf(axis);
    auto A = sinVal * cosVal * tanVal;
    auto B = cosVal * cosVal * tanVal;
    auto C = sinVal * sinVal * tanVal;

    auto e11 = m->e11;
    auto e21 = m->e21;
    m->e11 = (1.0f - A) * e11 - C * m->e12;
    m->e12 = B * e11 + (1.0f + A) * m->e12;
    m->e21 = (1.0f - A) * e21 - C * m->e22;
    m->e22 = B * e21 + (1.0f + A) * m->e22;
}


static bool _updateTransform(LottieTransform* transform, float frameNo, bool autoOrient, Matrix& matrix, uint8_t& opacity, LottieExpressions* exps)
{
    identity(&matrix);

    if (!transform) {
        opacity = 255;
        return false;
    }

    if (transform->coords) {
        translate(&matrix, transform->coords->x(frameNo, exps), transform->coords->y(frameNo, exps));
    } else {
        auto position = transform->position(frameNo, exps);
        translate(&matrix, position.x, position.y);
    }

    auto angle = 0.0f;
    if (autoOrient) angle = transform->position.angle(frameNo);
    if (transform->rotationEx) _rotationXYZ(&matrix, transform->rotationEx->x(frameNo, exps), transform->rotationEx->y(frameNo, exps), transform->rotation(frameNo, exps) + angle);
    else _rotationZ(&matrix, transform->rotation(frameNo, exps) + angle);


    auto skewAngle = transform->skewAngle(frameNo, exps);
    if (skewAngle != 0.0f) {
        // For angles where tangent explodes, the shape degenerates into an infinitely thin line.
        // This is handled by zeroing out the matrix due to finite numerical precision.
        skewAngle = fmod(skewAngle, 180.0f);
        if (fabsf(skewAngle - 90.0f) < 0.01f || fabsf(skewAngle + 90.0f) < 0.01f) return false;
        _skew(&matrix, skewAngle, transform->skewAxis(frameNo, exps));
    }

    auto scale = transform->scale(frameNo, exps);
    scaleR(&matrix, scale.x * 0.01f, scale.y * 0.01f);

    //Lottie specific anchor transform.
    auto anchor = transform->anchor(frameNo, exps);
    translateR(&matrix, -anchor.x, -anchor.y);

    //invisible just in case.
    if (scale.x == 0.0f || scale.y == 0.0f) opacity = 0;
    else opacity = transform->opacity(frameNo, exps);

    return true;
}


void LottieBuilder::updateTransform(LottieLayer* layer, float frameNo)
{
    if (!layer || tvg::equal(layer->cache.frameNo, frameNo)) return;

    auto transform = layer->transform;
    auto parent = layer->parent;

    if (parent) updateTransform(parent, frameNo);

    auto& matrix = layer->cache.matrix;

    _updateTransform(transform, frameNo, layer->autoOrient, matrix, layer->cache.opacity, exps);

    if (parent) {
        if (!identity((const Matrix*) &parent->cache.matrix)) {
            if (identity((const Matrix*) &matrix)) layer->cache.matrix = parent->cache.matrix;
            else layer->cache.matrix = parent->cache.matrix * matrix;
        }
    }
    layer->cache.frameNo = frameNo;
}


void LottieBuilder::updateTransform(LottieGroup* parent, LottieObject** child, float frameNo, TVG_UNUSED Inlist<RenderContext>& contexts, RenderContext* ctx)
{
    auto transform = static_cast<LottieTransform*>(*child);
    if (!transform) return;

    uint8_t opacity;

    if (parent->mergeable()) {
        if (!ctx->transform) ctx->transform = (Matrix*)malloc(sizeof(Matrix));
        _updateTransform(transform, frameNo, false, *ctx->transform, opacity, exps);
        return;
    }

    ctx->merging = nullptr;

    Matrix matrix;
    if (!_updateTransform(transform, frameNo, false, matrix, opacity, exps)) return;

    ctx->propagator->transform(PP(ctx->propagator)->transform() * matrix);
    ctx->propagator->opacity(MULTIPLY(opacity, PP(ctx->propagator)->opacity));

    //FIXME: preserve the stroke width. too workaround, need a better design.
    if (P(ctx->propagator)->rs.strokeWidth() > 0.0f) {
        auto denominator = sqrtf(matrix.e11 * matrix.e11 + matrix.e12 * matrix.e12);
        if (denominator > 1.0f) ctx->propagator->stroke(ctx->propagator->strokeWidth() / denominator);
    }
}


void LottieBuilder::updateGroup(LottieGroup* parent, LottieObject** child, float frameNo, TVG_UNUSED Inlist<RenderContext>& pcontexts, RenderContext* ctx)
{
    auto group = static_cast<LottieGroup*>(*child);

    if (!group->visible) return;

    //Prepare render data
    group->scene = parent->scene;
    group->reqFragment |= ctx->reqFragment;

    //generate a merging shape to consolidate partial shapes into a single entity
    if (group->mergeable()) _draw(parent, nullptr, ctx);

    Inlist<RenderContext> contexts;
    auto propagator = group->mergeable() ? ctx->propagator : static_cast<Shape*>(PP(ctx->propagator)->duplicate(group->pooling()));
    contexts.back(new RenderContext(*ctx, propagator, group->mergeable()));

    updateChildren(group, frameNo, contexts);

    contexts.free();
}


static void _updateStroke(LottieStroke* stroke, float frameNo, RenderContext* ctx, LottieExpressions* exps)
{
    ctx->propagator->stroke(stroke->width(frameNo, exps));
    ctx->propagator->stroke(stroke->cap);
    ctx->propagator->stroke(stroke->join);
    ctx->propagator->strokeMiterlimit(stroke->miterLimit);

    if (stroke->dashattr) {
        float dashes[2];
        dashes[0] = stroke->dashSize(frameNo, exps);
        dashes[1] = dashes[0] + stroke->dashGap(frameNo, exps);
        P(ctx->propagator)->strokeDash(dashes, 2, stroke->dashOffset(frameNo, exps));
    } else {
        ctx->propagator->stroke(nullptr, 0);
    }
}


static bool _fragmented(LottieGroup* parent, LottieObject** child, Inlist<RenderContext>& contexts, RenderContext* ctx)
{
    if (!ctx->reqFragment) return false;
    if (ctx->fragmenting) return true;

    contexts.back(new RenderContext(*ctx, static_cast<Shape*>(PP(ctx->propagator)->duplicate(parent->pooling()))));
    auto fragment = contexts.tail;
    fragment->begin = child - 1;
    ctx->fragmenting = true;

    return false;
}


void LottieBuilder::updateSolidStroke(LottieGroup* parent, LottieObject** child, float frameNo, Inlist<RenderContext>& contexts, RenderContext* ctx)
{
    if (_fragmented(parent, child, contexts, ctx)) return;

    auto stroke = static_cast<LottieSolidStroke*>(*child);

    ctx->merging = nullptr;
    auto color = stroke->color(frameNo, exps);
    ctx->propagator->stroke(color.rgb[0], color.rgb[1], color.rgb[2], stroke->opacity(frameNo, exps));
    _updateStroke(static_cast<LottieStroke*>(stroke), frameNo, ctx, exps);
}


void LottieBuilder::updateGradientStroke(LottieGroup* parent, LottieObject** child, float frameNo, Inlist<RenderContext>& contexts, RenderContext* ctx)
{
    if (_fragmented(parent, child, contexts, ctx)) return;

    auto stroke = static_cast<LottieGradientStroke*>(*child);

    ctx->merging = nullptr;
    ctx->propagator->stroke(unique_ptr<Fill>(stroke->fill(frameNo, exps)));
    _updateStroke(static_cast<LottieStroke*>(stroke), frameNo, ctx, exps);
}


void LottieBuilder::updateSolidFill(LottieGroup* parent, LottieObject** child, float frameNo, Inlist<RenderContext>& contexts, RenderContext* ctx)
{
    if (_fragmented(parent, child, contexts, ctx)) return;

    auto fill = static_cast<LottieSolidFill*>(*child);

    ctx->merging = nullptr;
    auto color = fill->color(frameNo, exps);
    ctx->propagator->fill(color.rgb[0], color.rgb[1], color.rgb[2], fill->opacity(frameNo, exps));
    ctx->propagator->fill(fill->rule);

    if (ctx->propagator->strokeWidth() > 0) ctx->propagator->order(true);
}


void LottieBuilder::updateGradientFill(LottieGroup* parent, LottieObject** child, float frameNo, Inlist<RenderContext>& contexts, RenderContext* ctx)
{
    if (_fragmented(parent, child, contexts, ctx)) return;

    auto fill = static_cast<LottieGradientFill*>(*child);

    ctx->merging = nullptr;
    //TODO: reuse the fill instance?
    ctx->propagator->fill(unique_ptr<Fill>(fill->fill(frameNo, exps)));
    ctx->propagator->fill(fill->rule);

    if (ctx->propagator->strokeWidth() > 0) ctx->propagator->order(true);
}


static bool _draw(LottieGroup* parent, LottieShape* shape, RenderContext* ctx)
{
    if (ctx->merging) return false;

    if (shape) {
        ctx->merging = shape->pooling();
        PP(ctx->propagator)->duplicate(ctx->merging);
    } else {
        ctx->merging = static_cast<Shape*>(ctx->propagator->duplicate());
    }

    parent->scene->push(cast(ctx->merging));

    return true;
}


static void _repeat(LottieGroup* parent, Shape* path, RenderContext* ctx)
{
    Array<Shape*> propagators;
    propagators.push(ctx->propagator);
    Array<Shape*> shapes;

    for (auto repeater = ctx->repeaters.end() - 1; repeater >= ctx->repeaters.begin(); --repeater) {
        shapes.reserve(repeater->cnt);

        for (int i = 0; i < repeater->cnt; ++i) {
            auto multiplier = repeater->offset + static_cast<float>(i);

            for (auto propagator = propagators.begin(); propagator < propagators.end(); ++propagator) {
                auto shape = static_cast<Shape*>((*propagator)->duplicate());
                P(shape)->rs.path = P(path)->rs.path;

                auto opacity = repeater->interpOpacity ? lerp<uint8_t>(repeater->startOpacity, repeater->endOpacity, static_cast<float>(i + 1) / repeater->cnt) : repeater->startOpacity;
                shape->opacity(opacity);

                Matrix m;
                identity(&m);
                translate(&m, repeater->position.x * multiplier + repeater->anchor.x, repeater->position.y * multiplier + repeater->anchor.y);
                scale(&m, powf(repeater->scale.x * 0.01f, multiplier), powf(repeater->scale.y * 0.01f, multiplier));
                rotate(&m, repeater->rotation * multiplier);
                translateR(&m, -repeater->anchor.x, -repeater->anchor.y);
                m = repeater->transform * m;

                Matrix inv;
                inverse(&repeater->transform, &inv);
                shape->transform(m * (inv * PP(shape)->transform()));
                shapes.push(shape);
            }
        }

        propagators.clear();
        propagators.reserve(shapes.count);

        //push repeat shapes in order.
        if (repeater->inorder) {
            for (auto shape = shapes.begin(); shape < shapes.end(); ++shape) {
                parent->scene->push(cast(*shape));
                propagators.push(*shape);
            }
        } else if (!shapes.empty()) {
            for (auto shape = shapes.end() - 1; shape >= shapes.begin(); --shape) {
                parent->scene->push(cast(*shape));
                propagators.push(*shape);
            }
        }
        shapes.clear();
    }
}


static void _appendRect(Shape* shape, float x, float y, float w, float h, float r, const LottieOffsetModifier* offsetPath, Matrix* transform, bool clockwise)
{
    //sharp rect
    if (tvg::zero(r)) {
        PathCommand commands[] = {
            PathCommand::MoveTo, PathCommand::LineTo, PathCommand::LineTo,
            PathCommand::LineTo, PathCommand::Close
        };

        Point points[4];
        if (clockwise) {
            points[0] = {x + w, y};
            points[1] = {x + w, y + h};
            points[2] = {x, y + h};
            points[3] = {x, y};
        } else {
            points[0] = {x + w, y};
            points[1] = {x, y};
            points[2] = {x, y + h};
            points[3] = {x + w, y + h};
        }
        if (transform) {
            for (int i = 0; i < 4; i++) {
                points[i] *= *transform;
            }
        }

        if (offsetPath) offsetPath->modifyRect(commands, 5, points, 4, P(shape)->rs.path.cmds, P(shape)->rs.path.pts);
        else shape->appendPath(commands, 5, points, 4);
    //round rect
    } else {
        constexpr int cmdCnt = 10;
        PathCommand commands[cmdCnt];

        auto halfW = w * 0.5f;
        auto halfH = h * 0.5f;
        auto rx = r > halfW ? halfW : r;
        auto ry = r > halfH ? halfH : r;
        auto hrx = rx * PATH_KAPPA;
        auto hry = ry * PATH_KAPPA;

        constexpr int ptsCnt = 17;
        Point points[ptsCnt];
        if (clockwise) {
            commands[0] = PathCommand::MoveTo; commands[1] = PathCommand::LineTo; commands[2] = PathCommand::CubicTo;
            commands[3] = PathCommand::LineTo; commands[4] = PathCommand::CubicTo;commands[5] = PathCommand::LineTo;
            commands[6] = PathCommand::CubicTo; commands[7] = PathCommand::LineTo; commands[8] = PathCommand::CubicTo;
            commands[9] = PathCommand::Close;

            points[0] = {x + w, y + ry}; //moveTo
            points[1] = {x + w, y + h - ry}; //lineTo
            points[2] = {x + w, y + h - ry + hry}; points[3] = {x + w - rx + hrx, y + h}; points[4] = {x + w - rx, y + h}; //cubicTo
            points[5] = {x + rx, y + h}, //lineTo
            points[6] = {x + rx - hrx, y + h}; points[7] = {x, y + h - ry + hry}; points[8] = {x, y + h - ry}; //cubicTo
            points[9] = {x, y + ry}, //lineTo
            points[10] = {x, y + ry - hry}; points[11] = {x + rx - hrx, y}; points[12] = {x + rx, y}; //cubicTo
            points[13] = {x + w - rx, y}; //lineTo
            points[14] = {x + w - rx + hrx, y}; points[15] = {x + w, y + ry - hry}; points[16] = {x + w, y + ry}; //cubicTo
        } else {
            commands[0] = PathCommand::MoveTo; commands[1] = PathCommand::CubicTo; commands[2] = PathCommand::LineTo;
            commands[3] = PathCommand::CubicTo; commands[4] = PathCommand::LineTo; commands[5] = PathCommand::CubicTo;
            commands[6] = PathCommand::LineTo; commands[7] = PathCommand::CubicTo; commands[8] = PathCommand::LineTo;
            commands[9] = PathCommand::Close;

            points[0] = {x + w, y + ry}; //moveTo
            points[1] = {x + w, y + ry - hry}; points[2] = {x + w - rx + hrx, y}; points[3] = {x + w - rx, y}; //cubicTo
            points[4] = {x + rx, y}, //lineTo
            points[5] = {x + rx - hrx, y}; points[6] = {x, y + ry - hry}; points[7] = {x, y + ry}; //cubicTo
            points[8] = {x, y + h - ry}; //lineTo
            points[9] = {x, y + h - ry + hry}; points[10] = {x + rx - hrx, y + h}; points[11] = {x + rx, y + h}; //cubicTo
            points[12] = {x + w - rx, y + h}; //lineTo
            points[13] = {x + w - rx + hrx, y + h}; points[14] = {x + w, y + h - ry + hry}; points[15] = {x + w, y + h - ry}; //cubicTo
            points[16] = {x + w, y + ry}; //lineTo
        }
        if (transform) {
            for (int i = 0; i < ptsCnt; i++) {
                points[i] *= *transform;
            }
        }

        if (offsetPath) offsetPath->modifyRect(commands, cmdCnt, points, ptsCnt, P(shape)->rs.path.cmds, P(shape)->rs.path.pts);
        else shape->appendPath(commands, cmdCnt, points, ptsCnt);
    }
}


void LottieBuilder::updateRect(LottieGroup* parent, LottieObject** child, float frameNo, TVG_UNUSED Inlist<RenderContext>& contexts, RenderContext* ctx)
{
    auto rect = static_cast<LottieRect*>(*child);

    auto position = rect->position(frameNo, exps);
    auto size = rect->size(frameNo, exps);
    auto r = rect->radius(frameNo, exps);
    if (r == 0.0f)  {
        if (ctx->roundness) ctx->roundness->modifyRect(size, r);
    } else {
        r = std::min({r, size.x * 0.5f, size.y * 0.5f});
    }
    
    if (!ctx->repeaters.empty()) {
        auto shape = rect->pooling();
        shape->reset();
        _appendRect(shape, position.x - size.x * 0.5f, position.y - size.y * 0.5f, size.x, size.y, r, ctx->offsetPath, ctx->transform, rect->clockwise);
        _repeat(parent, shape, ctx);
    } else {
        _draw(parent, rect, ctx);
        _appendRect(ctx->merging, position.x - size.x * 0.5f, position.y - size.y * 0.5f, size.x, size.y, r, ctx->offsetPath, ctx->transform, rect->clockwise);
    }
}


static void _appendCircle(Shape* shape, float cx, float cy, float rx, float ry, const LottieOffsetModifier* offsetPath, Matrix* transform, bool clockwise)
{
    if (offsetPath) offsetPath->modifyEllipse(rx, ry);

    if (rx == 0.0f || ry == 0.0f) return;

    auto rxKappa = rx * PATH_KAPPA;
    auto ryKappa = ry * PATH_KAPPA;

    constexpr int cmdsCnt = 6;
    PathCommand commands[cmdsCnt] = {
        PathCommand::MoveTo, PathCommand::CubicTo, PathCommand::CubicTo,
        PathCommand::CubicTo, PathCommand::CubicTo, PathCommand::Close
    };

    constexpr int ptsCnt = 13;
    Point points[ptsCnt];

    if (clockwise) {
        points[0] = {cx, cy - ry}; //moveTo
        points[1] = {cx + rxKappa, cy - ry}; points[2] = {cx + rx, cy - ryKappa}; points[3] = {cx + rx, cy}; //cubicTo
        points[4] = {cx + rx, cy + ryKappa}; points[5] = {cx + rxKappa, cy + ry}; points[6] = {cx, cy + ry}; //cubicTo
        points[7] = {cx - rxKappa, cy + ry}; points[8] = {cx - rx, cy + ryKappa}; points[9] = {cx - rx, cy}; //cubicTo
        points[10] = {cx - rx, cy - ryKappa}; points[11] = {cx - rxKappa, cy - ry}; points[12] = {cx, cy - ry}; //cubicTo
    } else {
        points[0] = {cx, cy - ry}; //moveTo
        points[1] = {cx - rxKappa, cy - ry}; points[2] = {cx - rx, cy - ryKappa}; points[3] = {cx - rx, cy}; //cubicTo
        points[4] = {cx - rx, cy + ryKappa}; points[5] = {cx - rxKappa, cy + ry}; points[6] = {cx, cy + ry}; //cubicTo
        points[7] = {cx + rxKappa, cy + ry}; points[8] = {cx + rx, cy + ryKappa}; points[9] = {cx + rx, cy}; //cubicTo
        points[10] = {cx + rx, cy - ryKappa}; points[11] = {cx + rxKappa, cy - ry}; points[12] = {cx, cy - ry}; //cubicTo
    }

    if (transform) {
        for (int i = 0; i < ptsCnt; ++i) {
            points[i] *= *transform;
        }
    }
    
    shape->appendPath(commands, cmdsCnt, points, ptsCnt);
}


void LottieBuilder::updateEllipse(LottieGroup* parent, LottieObject** child, float frameNo, TVG_UNUSED Inlist<RenderContext>& contexts, RenderContext* ctx)
{
    auto ellipse = static_cast<LottieEllipse*>(*child);

    auto position = ellipse->position(frameNo, exps);
    auto size = ellipse->size(frameNo, exps);

    if (!ctx->repeaters.empty()) {
        auto shape = ellipse->pooling();
        shape->reset();
        _appendCircle(shape, position.x, position.y, size.x * 0.5f, size.y * 0.5f, ctx->offsetPath, ctx->transform, ellipse->clockwise);
        _repeat(parent, shape, ctx);
    } else {
        _draw(parent, ellipse, ctx);
        _appendCircle(ctx->merging, position.x, position.y, size.x * 0.5f, size.y * 0.5f, ctx->offsetPath, ctx->transform, ellipse->clockwise);
    }
}


void LottieBuilder::updatePath(LottieGroup* parent, LottieObject** child, float frameNo, TVG_UNUSED Inlist<RenderContext>& contexts, RenderContext* ctx)
{
    auto path = static_cast<LottiePath*>(*child);

    if (!ctx->repeaters.empty()) {
        auto shape = path->pooling();
        shape->reset();
        path->pathset(frameNo, P(shape)->rs.path.cmds, P(shape)->rs.path.pts, ctx->transform, ctx->roundness, ctx->offsetPath, exps);
        _repeat(parent, shape, ctx);
    } else {
        _draw(parent, path, ctx);
        if (path->pathset(frameNo, P(ctx->merging)->rs.path.cmds, P(ctx->merging)->rs.path.pts, ctx->transform, ctx->roundness, ctx->offsetPath, exps)) {
            P(ctx->merging)->update(RenderUpdateFlag::Path);
        }
    }
}


static void _updateStar(TVG_UNUSED LottieGroup* parent, LottiePolyStar* star, Matrix* transform, const LottieRoundnessModifier* roundness, const LottieOffsetModifier* offsetPath, float frameNo, Shape* merging, LottieExpressions* exps)
{
    static constexpr auto POLYSTAR_MAGIC_NUMBER = 0.47829f / 0.28f;

    auto ptsCnt = star->ptsCnt(frameNo, exps);
    auto innerRadius = star->innerRadius(frameNo, exps);
    auto outerRadius = star->outerRadius(frameNo, exps);
    auto innerRoundness = star->innerRoundness(frameNo, exps) * 0.01f;
    auto outerRoundness = star->outerRoundness(frameNo, exps) * 0.01f;

    auto angle = deg2rad(-90.0f);
    auto partialPointRadius = 0.0f;
    auto anglePerPoint = (2.0f * MATH_PI / ptsCnt);
    auto halfAnglePerPoint = anglePerPoint * 0.5f;
    auto partialPointAmount = ptsCnt - floorf(ptsCnt);
    auto longSegment = false;
    auto numPoints = size_t(ceilf(ptsCnt) * 2);
    auto direction = star->clockwise ? 1.0f : -1.0f;
    auto hasRoundness = false;
    bool roundedCorner = roundness && (tvg::zero(innerRoundness) || tvg::zero(outerRoundness));

    Shape* shape;
    if (roundedCorner || offsetPath) {
        shape = star->pooling();
        shape->reset();
    } else {
        shape = merging;
    }

    float x, y;

    if (!tvg::zero(partialPointAmount)) {
        angle += halfAnglePerPoint * (1.0f - partialPointAmount) * direction;
    }

    if (!tvg::zero(partialPointAmount)) {
        partialPointRadius = innerRadius + partialPointAmount * (outerRadius - innerRadius);
        x = partialPointRadius * cosf(angle);
        y = partialPointRadius * sinf(angle);
        angle += anglePerPoint * partialPointAmount * 0.5f * direction;
    } else {
        x = outerRadius * cosf(angle);
        y = outerRadius * sinf(angle);
        angle += halfAnglePerPoint * direction;
    }

    if (tvg::zero(innerRoundness) && tvg::zero(outerRoundness)) {
        P(shape)->rs.path.pts.reserve(numPoints + 2);
        P(shape)->rs.path.cmds.reserve(numPoints + 3);
    } else {
        P(shape)->rs.path.pts.reserve(numPoints * 3 + 2);
        P(shape)->rs.path.cmds.reserve(numPoints + 3);
        hasRoundness = true;
    }

    Point in = {x, y};
    if (transform) in *= *transform;
    shape->moveTo(in.x, in.y);

    for (size_t i = 0; i < numPoints; i++) {
        auto radius = longSegment ? outerRadius : innerRadius;
        auto dTheta = halfAnglePerPoint;
        if (!tvg::zero(partialPointRadius) && i == numPoints - 2) {
            dTheta = anglePerPoint * partialPointAmount * 0.5f;
        }
        if (!tvg::zero(partialPointRadius) && i == numPoints - 1) {
            radius = partialPointRadius;
        }
        auto previousX = x;
        auto previousY = y;
        x = radius * cosf(angle);
        y = radius * sinf(angle);

        if (hasRoundness) {
            auto cp1Theta = (tvg::atan2(previousY, previousX) - MATH_PI2 * direction);
            auto cp1Dx = cosf(cp1Theta);
            auto cp1Dy = sinf(cp1Theta);
            auto cp2Theta = (tvg::atan2(y, x) - MATH_PI2 * direction);
            auto cp2Dx = cosf(cp2Theta);
            auto cp2Dy = sinf(cp2Theta);

            auto cp1Roundness = longSegment ? innerRoundness : outerRoundness;
            auto cp2Roundness = longSegment ? outerRoundness : innerRoundness;
            auto cp1Radius = longSegment ? innerRadius : outerRadius;
            auto cp2Radius = longSegment ? outerRadius : innerRadius;

            auto cp1x = cp1Radius * cp1Roundness * POLYSTAR_MAGIC_NUMBER * cp1Dx / ptsCnt;
            auto cp1y = cp1Radius * cp1Roundness * POLYSTAR_MAGIC_NUMBER * cp1Dy / ptsCnt;
            auto cp2x = cp2Radius * cp2Roundness * POLYSTAR_MAGIC_NUMBER * cp2Dx / ptsCnt;
            auto cp2y = cp2Radius * cp2Roundness * POLYSTAR_MAGIC_NUMBER * cp2Dy / ptsCnt;

            if (!tvg::zero(partialPointAmount) && ((i == 0) || (i == numPoints - 1))) {
                cp1x *= partialPointAmount;
                cp1y *= partialPointAmount;
                cp2x *= partialPointAmount;
                cp2y *= partialPointAmount;
            }
            Point in2 = {previousX - cp1x, previousY - cp1y};
            Point in3 = {x + cp2x, y + cp2y};
            Point in4 = {x, y};
            if (transform) {
                in2 *= *transform;
                in3 *= *transform;
                in4 *= *transform;
            }
            shape->cubicTo(in2.x, in2.y, in3.x, in3.y, in4.x, in4.y);
        } else {
            Point in = {x, y};
            if (transform) in *= *transform;
            shape->lineTo(in.x, in.y);
        }
        angle += dTheta * direction;
        longSegment = !longSegment;
    }
    shape->close();

    if (roundedCorner) {
        if (offsetPath) {
            auto intermediate = Shape::gen();
            roundness->modifyPolystar(P(shape)->rs.path.cmds, P(shape)->rs.path.pts, P(intermediate)->rs.path.cmds, P(intermediate)->rs.path.pts, outerRoundness, hasRoundness);
            offsetPath->modifyPolystar(P(intermediate)->rs.path.cmds, P(intermediate)->rs.path.pts, P(merging)->rs.path.cmds, P(merging)->rs.path.pts);
        } else {
            roundness->modifyPolystar(P(shape)->rs.path.cmds, P(shape)->rs.path.pts, P(merging)->rs.path.cmds, P(merging)->rs.path.pts, outerRoundness, hasRoundness);
        }
    } else if (offsetPath) offsetPath->modifyPolystar(P(shape)->rs.path.cmds, P(shape)->rs.path.pts, P(merging)->rs.path.cmds, P(merging)->rs.path.pts);
}


static void _updatePolygon(LottieGroup* parent, LottiePolyStar* star, Matrix* transform, const LottieRoundnessModifier* roundness, const LottieOffsetModifier* offsetPath, float frameNo, Shape* merging, LottieExpressions* exps)
{
    static constexpr auto POLYGON_MAGIC_NUMBER = 0.25f;

    auto ptsCnt = size_t(floor(star->ptsCnt(frameNo, exps)));
    auto radius = star->outerRadius(frameNo, exps);
    auto outerRoundness = star->outerRoundness(frameNo, exps) * 0.01f;

    auto angle = deg2rad(-90.0f);
    auto anglePerPoint = 2.0f * MATH_PI / float(ptsCnt);
    auto direction = star->clockwise ? 1.0f : -1.0f;
    auto hasRoundness = !tvg::zero(outerRoundness);
    bool roundedCorner = roundness && !hasRoundness;
    auto x = radius * cosf(angle);
    auto y = radius * sinf(angle);

    angle += anglePerPoint * direction;

    Shape* shape;
    if (roundedCorner || offsetPath) {
        shape = star->pooling();
        shape->reset();
    } else {
        shape = merging;
        if (hasRoundness) {
            P(shape)->rs.path.pts.reserve(ptsCnt * 3 + 2);
            P(shape)->rs.path.cmds.reserve(ptsCnt + 3);
        } else {
            P(shape)->rs.path.pts.reserve(ptsCnt + 2);
            P(shape)->rs.path.cmds.reserve(ptsCnt + 3);
        }
    }

    Point in = {x, y};
    if (transform) in *= *transform;
    shape->moveTo(in.x, in.y);

    for (size_t i = 0; i < ptsCnt; i++) {
        auto previousX = x;
        auto previousY = y;
        x = (radius * cosf(angle));
        y = (radius * sinf(angle));

        if (hasRoundness) {
            auto cp1Theta = tvg::atan2(previousY, previousX) - MATH_PI2 * direction;
            auto cp1Dx = cosf(cp1Theta);
            auto cp1Dy = sinf(cp1Theta);
            auto cp2Theta = tvg::atan2(y, x) - MATH_PI2 * direction;
            auto cp2Dx = cosf(cp2Theta);
            auto cp2Dy = sinf(cp2Theta);

            auto cp1x = radius * outerRoundness * POLYGON_MAGIC_NUMBER * cp1Dx;
            auto cp1y = radius * outerRoundness * POLYGON_MAGIC_NUMBER * cp1Dy;
            auto cp2x = radius * outerRoundness * POLYGON_MAGIC_NUMBER * cp2Dx;
            auto cp2y = radius * outerRoundness * POLYGON_MAGIC_NUMBER * cp2Dy;

            Point in2 = {previousX - cp1x, previousY - cp1y};
            Point in3 = {x + cp2x, y + cp2y};
            Point in4 = {x, y};
            if (transform) {
                in2 *= *transform;
                in3 *= *transform;
                in4 *= *transform;
            }
            shape->cubicTo(in2.x, in2.y, in3.x, in3.y, in4.x, in4.y);
        } else {
            Point in = {x, y};
            if (transform) in *= *transform;
            shape->lineTo(in.x, in.y);
        }
        angle += anglePerPoint * direction;
    }
    shape->close();

    if (roundedCorner) {
        if (offsetPath) {
            auto intermediate = Shape::gen();
            roundness->modifyPolystar(P(shape)->rs.path.cmds, P(shape)->rs.path.pts, P(intermediate)->rs.path.cmds, P(intermediate)->rs.path.pts, 0.0f, false);
            offsetPath->modifyPolystar(P(intermediate)->rs.path.cmds, P(intermediate)->rs.path.pts, P(merging)->rs.path.cmds, P(merging)->rs.path.pts);
        } else {
            roundness->modifyPolystar(P(shape)->rs.path.cmds, P(shape)->rs.path.pts, P(merging)->rs.path.cmds, P(merging)->rs.path.pts, 0.0f, false);
        }
    } else if (offsetPath) offsetPath->modifyPolystar(P(shape)->rs.path.cmds, P(shape)->rs.path.pts, P(merging)->rs.path.cmds, P(merging)->rs.path.pts);
}


void LottieBuilder::updatePolystar(LottieGroup* parent, LottieObject** child, float frameNo, TVG_UNUSED Inlist<RenderContext>& contexts, RenderContext* ctx)
{
    auto star = static_cast<LottiePolyStar*>(*child);

    //Optimize: Can we skip the individual coords transform?
    Matrix matrix;
    identity(&matrix);
    auto position = star->position(frameNo, exps);
    translate(&matrix, position.x, position.y);
    rotate(&matrix, star->rotation(frameNo, exps));

    if (ctx->transform) matrix = *ctx->transform * matrix;

    auto identity = tvg::identity((const Matrix*)&matrix);

    if (!ctx->repeaters.empty()) {
        auto shape = star->pooling();
        shape->reset();
        if (star->type == LottiePolyStar::Star) _updateStar(parent, star, identity ? nullptr : &matrix, ctx->roundness, ctx->offsetPath, frameNo, shape, exps);
        else _updatePolygon(parent, star, identity  ? nullptr : &matrix, ctx->roundness, ctx->offsetPath, frameNo, shape, exps);
        _repeat(parent, shape, ctx);
    } else {
        _draw(parent, star, ctx);
        if (star->type == LottiePolyStar::Star) _updateStar(parent, star, identity ? nullptr : &matrix, ctx->roundness, ctx->offsetPath, frameNo, ctx->merging, exps);
        else _updatePolygon(parent, star, identity  ? nullptr : &matrix, ctx->roundness, ctx->offsetPath, frameNo, ctx->merging, exps);
        P(ctx->merging)->update(RenderUpdateFlag::Path);
    }
}


void LottieBuilder::updateRoundedCorner(TVG_UNUSED LottieGroup* parent, LottieObject** child, float frameNo, TVG_UNUSED Inlist<RenderContext>& contexts, RenderContext* ctx)
{
    auto roundedCorner = static_cast<LottieRoundedCorner*>(*child);
    auto r = roundedCorner->radius(frameNo, exps);
    if (r < LottieRoundnessModifier::ROUNDNESS_EPSILON) return;

    if (!ctx->roundness) ctx->roundness = new LottieRoundnessModifier(r);
    else if (ctx->roundness->r < r) ctx->roundness->r = r;
}


void LottieBuilder::updateOffsetPath(TVG_UNUSED LottieGroup* parent, LottieObject** child, float frameNo, TVG_UNUSED Inlist<RenderContext>& contexts, RenderContext* ctx)
{
    auto offsetPath = static_cast<LottieOffsetPath*>(*child);
    if (!ctx->offsetPath) ctx->offsetPath = new LottieOffsetModifier(offsetPath->offset(frameNo, exps), offsetPath->miterLimit(frameNo, exps), offsetPath->join);
}


void LottieBuilder::updateRepeater(TVG_UNUSED LottieGroup* parent, LottieObject** child, float frameNo, TVG_UNUSED Inlist<RenderContext>& contexts, RenderContext* ctx)
{
    auto repeater = static_cast<LottieRepeater*>(*child);

    RenderRepeater r;
    r.cnt = static_cast<int>(repeater->copies(frameNo, exps));
    r.transform = PP(ctx->propagator)->transform();
    r.offset = repeater->offset(frameNo, exps);
    r.position = repeater->position(frameNo, exps);
    r.anchor = repeater->anchor(frameNo, exps);
    r.scale = repeater->scale(frameNo, exps);
    r.rotation = repeater->rotation(frameNo, exps);
    r.startOpacity = repeater->startOpacity(frameNo, exps);
    r.endOpacity = repeater->endOpacity(frameNo, exps);
    r.inorder = repeater->inorder;
    r.interpOpacity = (r.startOpacity == r.endOpacity) ? false : true;
    ctx->repeaters.push(r);

    ctx->merging = nullptr;
}


void LottieBuilder::updateTrimpath(TVG_UNUSED LottieGroup* parent, LottieObject** child, float frameNo, TVG_UNUSED Inlist<RenderContext>& contexts, RenderContext* ctx)
{
    auto trimpath = static_cast<LottieTrimpath*>(*child);

    float begin, end;
    trimpath->segment(frameNo, begin, end, exps);

    if (P(ctx->propagator)->rs.stroke) {
        auto pbegin = P(ctx->propagator)->rs.stroke->trim.begin;
        auto pend = P(ctx->propagator)->rs.stroke->trim.end;
        auto length = fabsf(pend - pbegin);
        begin = (length * begin) + pbegin;
        end = (length * end) + pbegin;
    }

    P(ctx->propagator)->strokeTrim(begin, end, trimpath->type == LottieTrimpath::Type::Simultaneous);
    ctx->merging = nullptr;
}


void LottieBuilder::updateChildren(LottieGroup* parent, float frameNo, Inlist<RenderContext>& contexts)
{
    contexts.head->begin = parent->children.end() - 1;

    while (!contexts.empty()) {
        auto ctx = contexts.front();
        ctx->reqFragment = parent->reqFragment;
        for (auto child = ctx->begin; child >= parent->children.data; --child) {
            //Here switch-case statements are more performant than virtual methods.
            switch ((*child)->type) {
                case LottieObject::Group: {
                    updateGroup(parent, child, frameNo, contexts, ctx);
                    break;
                }
                case LottieObject::Transform: {
                    updateTransform(parent, child, frameNo, contexts, ctx);
                    break;
                }
                case LottieObject::SolidFill: {
                    updateSolidFill(parent, child, frameNo, contexts, ctx);
                    break;
                }
                case LottieObject::SolidStroke: {
                    updateSolidStroke(parent, child, frameNo, contexts, ctx);
                    break;
                }
                case LottieObject::GradientFill: {
                    updateGradientFill(parent, child, frameNo, contexts, ctx);
                    break;
                }
                case LottieObject::GradientStroke: {
                    updateGradientStroke(parent, child, frameNo, contexts, ctx);
                    break;
                }
                case LottieObject::Rect: {
                    updateRect(parent, child, frameNo, contexts, ctx);
                    break;
                }
                case LottieObject::Ellipse: {
                    updateEllipse(parent, child, frameNo, contexts, ctx);
                    break;
                }
                case LottieObject::Path: {
                    updatePath(parent, child, frameNo, contexts, ctx);
                    break;
                }
                case LottieObject::Polystar: {
                    updatePolystar(parent, child, frameNo, contexts, ctx);
                    break;
                }
                case LottieObject::Trimpath: {
                    updateTrimpath(parent, child, frameNo, contexts, ctx);
                    break;
                }
                case LottieObject::Repeater: {
                    updateRepeater(parent, child, frameNo, contexts, ctx);
                    break;
                }
                case LottieObject::RoundedCorner: {
                    updateRoundedCorner(parent, child, frameNo, contexts, ctx);
                    break;
                }
                case LottieObject::OffsetPath: {
                    updateOffsetPath(parent, child, frameNo, contexts, ctx);
                    break;
                }
                default: break;
            }
            if (ctx->propagator->opacity() == 0) break;
        }
        delete(ctx);
    }
}


void LottieBuilder::updatePrecomp(LottieComposition* comp, LottieLayer* precomp, float frameNo)
{
    if (precomp->children.empty()) return;

    frameNo = precomp->remap(comp, frameNo, exps);

    for (auto c = precomp->children.end() - 1; c >= precomp->children.begin(); --c) {
        auto child = static_cast<LottieLayer*>(*c);
        if (!child->matteSrc) updateLayer(comp, precomp->scene, child, frameNo);
    }

    //clip the layer viewport
    auto clipper = precomp->statical.pooling(true);
    clipper->transform(precomp->cache.matrix);
    precomp->scene->clip(cast(clipper));
}


void LottieBuilder::updateSolid(LottieLayer* layer)
{
    auto solidFill = layer->statical.pooling(true);
    solidFill->opacity(layer->cache.opacity);
    layer->scene->push(cast(solidFill));
}


void LottieBuilder::updateImage(LottieGroup* layer)
{
    auto image = static_cast<LottieImage*>(layer->children.first());
    layer->scene->push(tvg::cast(image->pooling(true)));
}


void LottieBuilder::updateText(LottieLayer* layer, float frameNo)
{
    auto text = static_cast<LottieText*>(layer->children.first());
    auto textGrouping = text->alignOption.grouping;
    auto& doc = text->doc(frameNo);
    auto p = doc.text;

    if (!p || !text->font) return;

    auto scale = doc.size;
    Point cursor = {0.0f, 0.0f};
    auto scene = Scene::gen();
    auto textGroup = Scene::gen();
    int line = 0;
    int space = 0;
    auto lineSpacing = 0.0f;
    auto totalLineSpacing = 0.0f;

    //text string
    int idx = 0;
    auto totalChars = strlen(p);
    while (true) {
        //TODO: remove nested scenes.
        //end of text, new line of the cursor position
        if (*p == 13 || *p == 3 || *p == '\0') {
            //text layout position
            auto ascent = text->font->ascent * scale;
            if (ascent > doc.bbox.size.y) ascent = doc.bbox.size.y;
            Point layout = {doc.bbox.pos.x, doc.bbox.pos.y + ascent - doc.shift};

            //adjust the layout
            if (doc.justify == 1) layout.x += doc.bbox.size.x - (cursor.x * scale);  //right aligned
            else if (doc.justify == 2) layout.x += (doc.bbox.size.x * 0.5f) - (cursor.x * 0.5f * scale);  //center aligned

            //new text group, single scene based on text-grouping
            scene->push(std::move(textGroup));
            textGroup = Scene::gen();
            textGroup->translate(cursor.x, cursor.y);

            scene->translate(layout.x, layout.y);
            scene->scale(scale);

            layer->scene->push(std::move(scene));

            if (*p == '\0') break;
            ++p;

            totalLineSpacing += lineSpacing;
            lineSpacing = 0.0f;

            //new text group, single scene for each line
            scene = Scene::gen();
            cursor.x = 0.0f;
            cursor.y = (++line * doc.height + totalLineSpacing) / scale;
            continue;
        }

        if (*p == ' ') {
            ++space;
            if (textGrouping == LottieText::AlignOption::Group::Word) {
                //new text group, single scene for each word
                scene->push(std::move(textGroup));
                textGroup = Scene::gen();
                textGroup->translate(cursor.x, cursor.y);
            }
        }

        //find the glyph
        bool found = false;
        for (auto g = text->font->chars.begin(); g < text->font->chars.end(); ++g) {
            auto glyph = *g;
            //draw matched glyphs
            if (!strncmp(glyph->code, p, glyph->len)) {
                if (textGrouping == LottieText::AlignOption::Group::Chars || textGrouping == LottieText::AlignOption::Group::All) {
                    //new text group, single scene for each characters
                    scene->push(std::move(textGroup));
                    textGroup = Scene::gen();
                    textGroup->translate(cursor.x, cursor.y);
                }

                auto textGroupMatrix = textGroup->transform();
                auto shape = text->pooling();
                shape->reset();
                for (auto g = glyph->children.begin(); g < glyph->children.end(); ++g) {
                    auto group = static_cast<LottieGroup*>(*g);
                    for (auto p = group->children.begin(); p < group->children.end(); ++p) {
                        if (static_cast<LottiePath*>(*p)->pathset(frameNo, P(shape)->rs.path.cmds, P(shape)->rs.path.pts, nullptr, nullptr, nullptr)) {
                            P(shape)->update(RenderUpdateFlag::Path);
                        }
                    }
                }
                shape->fill(doc.color.rgb[0], doc.color.rgb[1], doc.color.rgb[2]);
                shape->translate(cursor.x - textGroupMatrix.e13, cursor.y - textGroupMatrix.e23);
                shape->opacity(255);

                if (doc.stroke.render) {
                    shape->stroke(StrokeJoin::Round);
                    shape->stroke(doc.stroke.width / scale);
                    shape->stroke(doc.stroke.color.rgb[0], doc.stroke.color.rgb[1], doc.stroke.color.rgb[2]);
                }

                auto needGroup = false;
                //text range process
                if (!text->ranges.empty()) {
                    Point scaling = {1.0f, 1.0f};
                    auto rotation = 0.0f;
                    Point translation = {0.0f, 0.0f};
                    auto color = doc.color;
                    auto strokeColor = doc.stroke.color;
                    uint8_t opacity = 255;
                    uint8_t fillOpacity = 255;
                    uint8_t strokeOpacity = 255;

                    for (auto s = text->ranges.begin(); s < text->ranges.end(); ++s) {
                        auto basedIdx = idx;
                        if ((*s)->based == LottieTextRange::Based::CharsExcludingSpaces) basedIdx = idx - space;
                        else if ((*s)->based == LottieTextRange::Based::Words) basedIdx = line + space;
                        else if ((*s)->based == LottieTextRange::Based::Lines) basedIdx = line;

                        auto f = (*s)->factor(frameNo, float(totalChars), (float)basedIdx);
                        if (tvg::zero(f)) continue;
                        needGroup = true;

                        translation = translation + f * (*s)->style.position(frameNo);
                        auto temp = (*s)->style.scale(frameNo);
                        temp.x *= 0.01f;
                        temp.y *= 0.01f;
                        temp.x -= 1.0f;
                        temp.y -= 1.0f;
                        temp.x *= f;
                        temp.y *= f;
                        temp.x += 1.0f;
                        temp.y += 1.0f;
                        scaling.x *= temp.x;
                        scaling.y *= temp.y;
                        rotation += f * (*s)->style.rotation(frameNo);

                        opacity = (uint8_t)(opacity - f * (opacity - (*s)->style.opacity(frameNo)));
                        shape->opacity(opacity);

                        auto rangeColor = (*s)->style.fillColor(frameNo); //TODO: use flag to check whether it was really set
                        if (tvg::equal(f, 1.0f)) color = rangeColor;
                        else {
                            color.rgb[0] = lerp<uint8_t>(color.rgb[0], rangeColor.rgb[0], f);
                            color.rgb[1] = lerp<uint8_t>(color.rgb[1], rangeColor.rgb[1], f);
                            color.rgb[2] = lerp<uint8_t>(color.rgb[2], rangeColor.rgb[2], f);
                        }
                        fillOpacity = (uint8_t)(fillOpacity - f * (fillOpacity - (*s)->style.fillOpacity(frameNo)));
                        shape->fill(color.rgb[0], color.rgb[1], color.rgb[2], fillOpacity);

                        if (doc.stroke.render) {
                            shape->stroke(f * (*s)->style.strokeWidth(frameNo) / scale);
                            auto rangeColor = (*s)->style.strokeColor(frameNo); //TODO: use flag to check whether it was really set
                            if (tvg::equal(f, 1.0f)) strokeColor = rangeColor;
                            else {
                                strokeColor.rgb[0] = lerp<uint8_t>(strokeColor.rgb[0], rangeColor.rgb[0], f);
                                strokeColor.rgb[1] = lerp<uint8_t>(strokeColor.rgb[1], rangeColor.rgb[1], f);
                                strokeColor.rgb[2] = lerp<uint8_t>(strokeColor.rgb[2], rangeColor.rgb[2], f);
                            }
                            strokeOpacity = (uint8_t)(strokeOpacity - f * (strokeOpacity - (*s)->style.strokeOpacity(frameNo)));
                            shape->stroke(strokeColor.rgb[0], strokeColor.rgb[1], strokeColor.rgb[2], strokeOpacity);
                        }

                        cursor.x += f * (*s)->style.letterSpacing(frameNo);

                        auto spacing = f * (*s)->style.lineSpacing(frameNo);
                        if (spacing > lineSpacing) lineSpacing = spacing;
                    }

                    // TextGroup transformation is performed once
                    if (textGroup->paints().size() == 0 && needGroup) {
                        identity(&textGroupMatrix);
                        translate(&textGroupMatrix, cursor.x, cursor.y);

                        auto alignment = text->alignOption.anchor(frameNo);

                        // center pivoting
                        textGroupMatrix.e13 += alignment.x;
                        textGroupMatrix.e23 += alignment.y;

                        rotate(&textGroupMatrix, rotation);

                        auto pivotX = alignment.x * -1;
                        auto pivotY = alignment.y * -1;

                        //center pivoting
                        textGroupMatrix.e13 += (pivotX * textGroupMatrix.e11 + pivotX * textGroupMatrix.e12);
                        textGroupMatrix.e23 += (pivotY * textGroupMatrix.e21 + pivotY * textGroupMatrix.e22);

                        textGroup->transform(textGroupMatrix);
                    }

                    Matrix matrix;
                    identity(&matrix);
                    translate(&matrix, translation.x / scale + cursor.x - textGroupMatrix.e13, translation.y / scale + cursor.y - textGroupMatrix.e23);
                    tvg::scale(&matrix, scaling.x, scaling.y);
                    shape->transform(matrix);
                }

                if (needGroup) {
                    textGroup->push(cast(shape));
                } else {
                    // When text isn't selected, exclude the shape from the text group
                    auto matrix = shape->transform();
                    matrix.e13 = cursor.x;
                    matrix.e23 = cursor.y;
                    shape->transform(matrix);
                    scene->push(cast(shape));
                }

                p += glyph->len;
                idx += glyph->len;

                //advance the cursor position horizontally
                cursor.x += glyph->width + doc.tracking;

                found = true;
                break;
            }
        }

        if (!found) {
            ++p;
            ++idx;
        }
    }
}


void LottieBuilder::updateMaskings(LottieLayer* layer, float frameNo)
{
    if (layer->masks.count == 0) return;

    //Apply the base mask
    auto pMask = static_cast<LottieMask*>(layer->masks[0]);
    auto pMethod = pMask->method;
    auto opacity = pMask->opacity(frameNo);
    auto expand = pMask->expand(frameNo);

    auto pShape = layer->pooling();
    pShape->reset();
    pShape->fill(255, 255, 255, opacity);
    pShape->transform(layer->cache.matrix);

    //Apply Masking Expansion (Offset)
    if (expand == 0.0f) {
        pMask->pathset(frameNo, P(pShape)->rs.path.cmds, P(pShape)->rs.path.pts, nullptr, nullptr, nullptr, exps);
    } else {
        //TODO: Once path direction support is implemented, ensure that the direction is ignored here
        auto offset = LottieOffsetModifier(pMask->expand(frameNo));
        pMask->pathset(frameNo, P(pShape)->rs.path.cmds, P(pShape)->rs.path.pts, nullptr, nullptr, &offset, exps);
    }

    auto compMethod = (pMethod == CompositeMethod::SubtractMask || pMethod == CompositeMethod::InvAlphaMask) ? CompositeMethod::InvAlphaMask : CompositeMethod::AlphaMask;

    //Cheaper. Replace the masking with a clipper
    if (layer->masks.count == 1 && compMethod == CompositeMethod::AlphaMask && opacity == 255) {
        layer->scene->clip(tvg::cast(pShape));
        return;
    }

    //Introduce an intermediate scene for embracing the matte + masking
    if (layer->matteTarget) {
        auto scene = Scene::gen().release();
        scene->push(cast(layer->scene));
        layer->scene = scene;
    }

    layer->scene->composite(tvg::cast(pShape), compMethod);

    //Apply the subsquent masks
    for (auto m = layer->masks.begin() + 1; m < layer->masks.end(); ++m) {
        auto mask = static_cast<LottieMask*>(*m);
        auto method = mask->method;
        if (method == CompositeMethod::None) continue;

        //Append the mask shape
        if (pMethod == method && (method == CompositeMethod::SubtractMask || method == CompositeMethod::DifferenceMask)) {
            mask->pathset(frameNo, P(pShape)->rs.path.cmds, P(pShape)->rs.path.pts, nullptr, nullptr, nullptr, exps);
        //Chain composition
        } else {
            auto shape = layer->pooling();
            shape->reset();
            shape->fill(255, 255, 255, mask->opacity(frameNo));
            shape->transform(layer->cache.matrix);
            mask->pathset(frameNo, P(shape)->rs.path.cmds, P(shape)->rs.path.pts, nullptr, nullptr, nullptr, exps);
            pShape->composite(tvg::cast(shape), method);
            pShape = shape;
            pMethod = method;
        }
    }
}


bool LottieBuilder::updateMatte(LottieComposition* comp, float frameNo, Scene* scene, LottieLayer* layer)
{
    auto target = layer->matteTarget;
    if (!target) return true;

    updateLayer(comp, scene, target, frameNo);

    if (target->scene) {
        layer->scene->composite(cast(target->scene), layer->matteType);
    } else if (layer->matteType == CompositeMethod::AlphaMask || layer->matteType == CompositeMethod::LumaMask) {
        //matte target is not exist. alpha blending definitely bring an invisible result
        delete(layer->scene);
        layer->scene = nullptr;
        return false;
    }
    return true;
}


void LottieBuilder::updateEffect(LottieLayer* layer, float frameNo)
{
    constexpr int QUALITY = 25;
    constexpr float BLUR_TO_SIGMA = 0.3f;

    if (layer->effects.count == 0) return;

    for (auto ef = layer->effects.begin(); ef < layer->effects.end(); ++ef) {
        if (!(*ef)->enable) continue;
        switch ((*ef)->type) {
            case LottieEffect::DropShadow: {
                auto effect = static_cast<LottieDropShadow*>(*ef);
                auto color = effect->color(frameNo);
                layer->scene->push(SceneEffect::DropShadow, color.rgb[0], color.rgb[1], color.rgb[2], (int)effect->opacity(frameNo), effect->angle(frameNo), effect->distance(frameNo), effect->blurness(frameNo) * BLUR_TO_SIGMA, QUALITY);
                break;
            }
            case LottieEffect::GaussianBlur: {
                auto effect = static_cast<LottieGaussianBlur*>(*ef);
                layer->scene->push(SceneEffect::GaussianBlur, effect->blurness(frameNo) * BLUR_TO_SIGMA, effect->direction(frameNo) - 1, effect->wrap(frameNo), QUALITY);
                break;
            }
            default: break;
        }
    }
}


void LottieBuilder::updateLayer(LottieComposition* comp, Scene* scene, LottieLayer* layer, float frameNo)
{
    layer->scene = nullptr;

    //visibility
    if (frameNo < layer->inFrame || frameNo >= layer->outFrame) return;

    updateTransform(layer, frameNo);

    //full transparent scene. no need to perform
    if (layer->type != LottieLayer::Null && layer->cache.opacity == 0) return;

    //Prepare render data
    layer->scene = Scene::gen().release();
    layer->scene->id = layer->id;

    //ignore opacity when Null layer?
    if (layer->type != LottieLayer::Null) layer->scene->opacity(layer->cache.opacity);

    layer->scene->transform(layer->cache.matrix);

    if (!updateMatte(comp, frameNo, scene, layer)) return;

    switch (layer->type) {
        case LottieLayer::Precomp: {
            updatePrecomp(comp, layer, frameNo);
            break;
        }
        case LottieLayer::Solid: {
            updateSolid(layer);
            break;
        }
        case LottieLayer::Image: {
            updateImage(layer);
            break;
        }
        case LottieLayer::Text: {
            updateText(layer, frameNo);
            break;
        }
        default: {
            if (!layer->children.empty()) {
                Inlist<RenderContext> contexts;
                contexts.back(new RenderContext(layer->pooling()));
                updateChildren(layer, frameNo, contexts);
                contexts.free();
            }
            break;
        }
    }

    updateMaskings(layer, frameNo);

    layer->scene->blend(layer->blendMethod);

    updateEffect(layer, frameNo);

    //the given matte source was composited by the target earlier.
    if (!layer->matteSrc) scene->push(cast(layer->scene));
}


static void _buildReference(LottieComposition* comp, LottieLayer* layer)
{
    for (auto asset = comp->assets.begin(); asset < comp->assets.end(); ++asset) {
        if (layer->rid != (*asset)->id) continue;
        if (layer->type == LottieLayer::Precomp) {
            auto assetLayer = static_cast<LottieLayer*>(*asset);
            if (_buildComposition(comp, assetLayer)) {
                layer->children = assetLayer->children;
                layer->reqFragment = assetLayer->reqFragment;
            }
        } else if (layer->type == LottieLayer::Image) {
            layer->children.push(*asset);
        }
        break;
    }
}


static void _buildHierarchy(LottieGroup* parent, LottieLayer* child)
{
    if (child->pidx == -1) return;

    if (child->matteTarget && child->pidx == child->matteTarget->idx) {
        child->parent = child->matteTarget;
        return;
    }

    for (auto p = parent->children.begin(); p < parent->children.end(); ++p) {
        auto parent = static_cast<LottieLayer*>(*p);
        if (child == parent) continue;
        if (child->pidx == parent->idx) {
            child->parent = parent;
            break;
        }
        if (parent->matteTarget && parent->matteTarget->idx == child->pidx) {
            child->parent = parent->matteTarget;
            break;
        }
    }
}


static void _attachFont(LottieComposition* comp, LottieLayer* parent)
{
    //TODO: Consider to migrate this attachment to the frame update time.
    for (auto c = parent->children.begin(); c < parent->children.end(); ++c) {
        auto text = static_cast<LottieText*>(*c);
        auto& doc = text->doc(0);
        if (!doc.name) continue;
        auto len = strlen(doc.name);
        for (uint32_t i = 0; i < comp->fonts.count; ++i) {
            auto font = comp->fonts[i];
            auto len2 = strlen(font->name);
            if (len == len2 && !strcmp(font->name, doc.name)) {
                text->font = font;
                break;
            }
        }
    }
}


static bool _buildComposition(LottieComposition* comp, LottieLayer* parent)
{
    if (parent->children.count == 0) return false;
    if (parent->buildDone) return true;
    parent->buildDone = true;

    for (auto c = parent->children.begin(); c < parent->children.end(); ++c) {
        auto child = static_cast<LottieLayer*>(*c);

        //attach the precomp layer.
        if (child->rid) _buildReference(comp, child);

        if (child->matteType != CompositeMethod::None) {
            //no index of the matte layer is provided: the layer above is used as the matte source
            if (child->mid == -1) {
                if (c > parent->children.begin()) {
                    child->matteTarget = static_cast<LottieLayer*>(*(c - 1));
                }
            //matte layer is specified by an index.
            } else child->matteTarget = parent->layerByIdx(child->mid);
        }

        if (child->matteTarget) {
            //parenting
            _buildHierarchy(parent, child->matteTarget);
            //precomp referencing
            if (child->matteTarget->rid) _buildReference(comp, child->matteTarget);
        }
        _buildHierarchy(parent, child);

        //attach the necessary font data
        if (child->type == LottieLayer::Text) _attachFont(comp, child);
    }
    return true;
}


/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/

bool LottieBuilder::update(LottieComposition* comp, float frameNo)
{
    if (comp->root->children.empty()) return false;

    frameNo += comp->root->inFrame;
    if (frameNo <comp->root->inFrame) frameNo = comp->root->inFrame;
    if (frameNo >= comp->root->outFrame) frameNo = (comp->root->outFrame - 1);

    //update children layers
    auto root = comp->root;
    root->scene->clear();

    if (exps && comp->expressions) exps->update(comp->timeAtFrame(frameNo));

    for (auto child = root->children.end() - 1; child >= root->children.begin(); --child) {
        auto layer = static_cast<LottieLayer*>(*child);
        if (!layer->matteSrc) updateLayer(comp, root->scene, layer, frameNo);
    }

    return true;
}


void LottieBuilder::build(LottieComposition* comp)
{
    if (!comp) return;

    comp->root->scene = Scene::gen().release();

    _buildComposition(comp, comp->root);

    if (!update(comp, 0)) return;

    //viewport clip
    auto clip = Shape::gen();
    clip->appendRect(0, 0, comp->w, comp->h);
    comp->root->scene->clip(std::move(clip));
}

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

#include "tvgMath.h" /* to include math.h before cstring */
#include <cstring>
#include <string>
#include "tvgShape.h"
#include "tvgCompressor.h"
#include "tvgPaint.h"
#include "tvgFill.h"
#include "tvgStr.h"
#include "tvgSvgLoaderCommon.h"
#include "tvgSvgSceneBuilder.h"
#include "tvgSvgPath.h"
#include "tvgSvgUtil.h"

/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/

static bool _appendShape(SvgLoaderData& loaderData, SvgNode* node, Shape* shape, const Box& vBox, const string& svgPath);
static bool _appendClipShape(SvgLoaderData& loaderData, SvgNode* node, Shape* shape, const Box& vBox, const string& svgPath, const Matrix* transform);
static unique_ptr<Scene> _sceneBuildHelper(SvgLoaderData& loaderData, const SvgNode* node, const Box& vBox, const string& svgPath, bool mask, int depth, bool* isMaskWhite = nullptr);


static inline bool _isGroupType(SvgNodeType type)
{
    if (type == SvgNodeType::Doc || type == SvgNodeType::G || type == SvgNodeType::Use || type == SvgNodeType::ClipPath || type == SvgNodeType::Symbol) return true;
    return false;
}


//According to: https://www.w3.org/TR/SVG11/coords.html#ObjectBoundingBoxUnits (the last paragraph)
//a stroke width should be ignored for bounding box calculations
static Box _boundingBox(const Shape* shape)
{
    float x, y, w, h;
    shape->bounds(&x, &y, &w, &h, false);

    if (auto strokeW = shape->strokeWidth()) {
        x += 0.5f * strokeW;
        y += 0.5f * strokeW;
        w -= strokeW;
        h -= strokeW;
    }

    return {x, y, w, h};
}


static Box _boundingBox(const Text* text)
{
    float x, y, w, h;
    text->bounds(&x, &y, &w, &h, false);
    return {x, y, w, h};
}


static void _transformMultiply(const Matrix* mBBox, Matrix* gradTransf)
{
    gradTransf->e13 = gradTransf->e13 * mBBox->e11 + mBBox->e13;
    gradTransf->e12 *= mBBox->e11;
    gradTransf->e11 *= mBBox->e11;

    gradTransf->e23 = gradTransf->e23 * mBBox->e22 + mBBox->e23;
    gradTransf->e22 *= mBBox->e22;
    gradTransf->e21 *= mBBox->e22;
}


static unique_ptr<LinearGradient> _applyLinearGradientProperty(SvgStyleGradient* g, const Box& vBox, int opacity)
{
    Fill::ColorStop* stops;
    int stopCount = 0;
    auto fillGrad = LinearGradient::gen();

    bool isTransform = (g->transform ? true : false);
    Matrix finalTransform = {1, 0, 0, 0, 1, 0, 0, 0, 1};
    if (isTransform) finalTransform = *g->transform;

    if (g->userSpace) {
        g->linear->x1 = g->linear->x1 * vBox.w;
        g->linear->y1 = g->linear->y1 * vBox.h;
        g->linear->x2 = g->linear->x2 * vBox.w;
        g->linear->y2 = g->linear->y2 * vBox.h;
    } else {
        Matrix m = {vBox.w, 0, vBox.x, 0, vBox.h, vBox.y, 0, 0, 1};
        if (isTransform) _transformMultiply(&m, &finalTransform);
        else {
            finalTransform = m;
            isTransform = true;
        }
    }

    if (isTransform) fillGrad->transform(finalTransform);

    fillGrad->linear(g->linear->x1, g->linear->y1, g->linear->x2, g->linear->y2);
    fillGrad->spread(g->spread);

    //Update the stops
    stopCount = g->stops.count;
    if (stopCount > 0) {
        stops = (Fill::ColorStop*)calloc(stopCount, sizeof(Fill::ColorStop));
        if (!stops) return fillGrad;
        auto prevOffset = 0.0f;
        for (uint32_t i = 0; i < g->stops.count; ++i) {
            auto colorStop = &g->stops[i];
            //Use premultiplied color
            stops[i].r = colorStop->r;
            stops[i].g = colorStop->g;
            stops[i].b = colorStop->b;
            stops[i].a = static_cast<uint8_t>((colorStop->a * opacity) / 255);
            stops[i].offset = colorStop->offset;
            //check the offset corner cases - refer to: https://svgwg.org/svg2-draft/pservers.html#StopNotes
            if (colorStop->offset < prevOffset) stops[i].offset = prevOffset;
            else if (colorStop->offset > 1) stops[i].offset = 1;
            prevOffset = stops[i].offset;
        }
        fillGrad->colorStops(stops, stopCount);
        free(stops);
    }
    return fillGrad;
}


static unique_ptr<RadialGradient> _applyRadialGradientProperty(SvgStyleGradient* g, const Box& vBox, int opacity)
{
    Fill::ColorStop *stops;
    int stopCount = 0;
    auto fillGrad = RadialGradient::gen();

    bool isTransform = (g->transform ? true : false);
    Matrix finalTransform = {1, 0, 0, 0, 1, 0, 0, 0, 1};
    if (isTransform) finalTransform = *g->transform;

    if (g->userSpace) {
        //The radius scaling is done according to the Units section:
        //https://www.w3.org/TR/2015/WD-SVG2-20150915/coords.html
        g->radial->cx = g->radial->cx * vBox.w;
        g->radial->cy = g->radial->cy * vBox.h;
        g->radial->r = g->radial->r * sqrtf(powf(vBox.w, 2.0f) + powf(vBox.h, 2.0f)) / sqrtf(2.0f);
        g->radial->fx = g->radial->fx * vBox.w;
        g->radial->fy = g->radial->fy * vBox.h;
        g->radial->fr = g->radial->fr * sqrtf(powf(vBox.w, 2.0f) + powf(vBox.h, 2.0f)) / sqrtf(2.0f);
    } else {
        Matrix m = {vBox.w, 0, vBox.x, 0, vBox.h, vBox.y, 0, 0, 1};
        if (isTransform) _transformMultiply(&m, &finalTransform);
        else {
            finalTransform = m;
            isTransform = true;
        }
    }

    if (isTransform) fillGrad->transform(finalTransform);

    P(fillGrad)->radial(g->radial->cx, g->radial->cy, g->radial->r, g->radial->fx, g->radial->fy, g->radial->fr);
    fillGrad->spread(g->spread);

    //Update the stops
    stopCount = g->stops.count;
    if (stopCount > 0) {
        stops = (Fill::ColorStop*)calloc(stopCount, sizeof(Fill::ColorStop));
        if (!stops) return fillGrad;
        auto prevOffset = 0.0f;
        for (uint32_t i = 0; i < g->stops.count; ++i) {
            auto colorStop = &g->stops[i];
            //Use premultiplied color
            stops[i].r = colorStop->r;
            stops[i].g = colorStop->g;
            stops[i].b = colorStop->b;
            stops[i].a = static_cast<uint8_t>((colorStop->a * opacity) / 255);
            stops[i].offset = colorStop->offset;
            //check the offset corner cases - refer to: https://svgwg.org/svg2-draft/pservers.html#StopNotes
            if (colorStop->offset < prevOffset) stops[i].offset = prevOffset;
            else if (colorStop->offset > 1) stops[i].offset = 1;
            prevOffset = stops[i].offset;
        }
        fillGrad->colorStops(stops, stopCount);
        free(stops);
    }
    return fillGrad;
}


//The SVG standard allows only for 'use' nodes that point directly to a basic shape.
static bool _appendClipUseNode(SvgLoaderData& loaderData, SvgNode* node, Shape* shape, const Box& vBox, const string& svgPath)
{
    if (node->child.count != 1) return false;
    auto child = *(node->child.data);

    Matrix finalTransform = {1, 0, 0, 0, 1, 0, 0, 0, 1};
    if (node->transform) finalTransform = *node->transform;
    if (node->node.use.x != 0.0f || node->node.use.y != 0.0f) {
        Matrix m = {1, 0, node->node.use.x, 0, 1, node->node.use.y, 0, 0, 1};
        finalTransform *= m;
    }
    if (child->transform) finalTransform *= *child->transform;

    return _appendClipShape(loaderData, child, shape, vBox, svgPath, identity((const Matrix*)(&finalTransform)) ? nullptr : &finalTransform);
}


static bool _appendClipChild(SvgLoaderData& loaderData, SvgNode* node, Shape* shape, const Box& vBox, const string& svgPath, bool clip)
{
    if (node->type == SvgNodeType::Use) {
        return _appendClipUseNode(loaderData, node, shape, vBox, svgPath);
    }
    return _appendClipShape(loaderData, node, shape, vBox, svgPath, nullptr);
}


static Matrix _compositionTransform(Paint* paint, const SvgNode* node, const SvgNode* compNode, SvgNodeType type)
{
    Matrix m = {1, 0, 0, 0, 1, 0, 0, 0, 1};
    //The initial mask transformation ignored according to the SVG standard.
    if (node->transform && type != SvgNodeType::Mask) {
        m = *node->transform;
    }
    if (compNode->transform) {
        m *= *compNode->transform;
    }
    if (!compNode->node.clip.userSpace) {
        float x, y, w, h;
        P(paint)->bounds(&x, &y, &w, &h, false, false);
        Matrix mBBox = {w, 0, x, 0, h, y, 0, 0, 1};
        m *= mBBox;
    }
    return m;
}


static void _applyComposition(SvgLoaderData& loaderData, Paint* paint, const SvgNode* node, const Box& vBox, const string& svgPath)
{
    /* ClipPath */
    /* Do not drop in Circular Dependency for ClipPath.
       Composition can be applied recursively if its children nodes have composition target to this one. */
    if (node->style->clipPath.applying) {
        TVGLOG("SVG", "Multiple Composition Tried! Check out Circular dependency?");
    } else {
        auto compNode = node->style->clipPath.node;
        if (compNode && compNode->child.count > 0) {
            node->style->clipPath.applying = true;

            auto comp = Shape::gen();

            auto child = compNode->child.data;
            auto valid = false; //Composite only when valid shapes exist

            for (uint32_t i = 0; i < compNode->child.count; ++i, ++child) {
                if (_appendClipChild(loaderData, *child, comp.get(), vBox, svgPath, compNode->child.count > 1)) valid = true;
            }

            if (valid) {
                Matrix finalTransform = _compositionTransform(paint, node, compNode, SvgNodeType::ClipPath);
                comp->transform(finalTransform);
                paint->clip(std::move(comp));
            }

            node->style->clipPath.applying = false;
        }
    }

    /* Mask */
    /* Do not drop in Circular Dependency for Mask.
       Composition can be applied recursively if its children nodes have composition target to this one. */
    if (node->style->mask.applying) {
        TVGLOG("SVG", "Multiple Composition Tried! Check out Circular dependency?");
    } else {
        auto compNode = node->style->mask.node;
        if (compNode && compNode->child.count > 0) {
            node->style->mask.applying = true;

            bool isMaskWhite = true;
            if (auto comp = _sceneBuildHelper(loaderData, compNode, vBox, svgPath, true, 0, &isMaskWhite)) {
                if (!compNode->node.mask.userSpace) {
                    Matrix finalTransform = _compositionTransform(paint, node, compNode, SvgNodeType::Mask);
                    comp->transform(finalTransform);
                } else {
                    if (node->transform) comp->transform(*node->transform);
                }

                if (compNode->node.mask.type == SvgMaskType::Luminance && !isMaskWhite) {
                    paint->composite(std::move(comp), CompositeMethod::LumaMask);
                } else {
                    paint->composite(std::move(comp), CompositeMethod::AlphaMask);
                }
            }

            node->style->mask.applying = false;
        }
    }
}


static void _applyProperty(SvgLoaderData& loaderData, SvgNode* node, Shape* vg, const Box& vBox, const string& svgPath, bool clip)
{
    SvgStyleProperty* style = node->style;

    //Clip transformation is applied directly to the path in the _appendClipShape function
    if (node->transform && !clip) vg->transform(*node->transform);
    if (node->type == SvgNodeType::Doc || !node->style->display) return;

    //If fill property is nullptr then do nothing
    if (style->fill.paint.none) {
        //Do nothing
    } else if (style->fill.paint.gradient) {
        Box bBox = vBox;
        if (!style->fill.paint.gradient->userSpace) bBox = _boundingBox(vg);

        if (style->fill.paint.gradient->type == SvgGradientType::Linear) {
            auto linear = _applyLinearGradientProperty(style->fill.paint.gradient, bBox, style->fill.opacity);
            vg->fill(std::move(linear));
        } else if (style->fill.paint.gradient->type == SvgGradientType::Radial) {
            auto radial = _applyRadialGradientProperty(style->fill.paint.gradient, bBox, style->fill.opacity);
            vg->fill(std::move(radial));
        }
    } else if (style->fill.paint.url) {
        //TODO: Apply the color pointed by url
        TVGLOG("SVG", "The fill's url not supported.");
    } else if (style->fill.paint.curColor) {
        //Apply the current style color
        vg->fill(style->color.r, style->color.g, style->color.b, style->fill.opacity);
    } else {
        //Apply the fill color
        vg->fill(style->fill.paint.color.r, style->fill.paint.color.g, style->fill.paint.color.b, style->fill.opacity);
    }

    //Apply the fill rule
    vg->fill((tvg::FillRule)style->fill.fillRule);
    //Rendering order
    vg->order(!style->paintOrder);

    //Apply node opacity
    if (style->opacity < 255) vg->opacity(style->opacity);

    if (node->type == SvgNodeType::G || node->type == SvgNodeType::Use) return;

    //Apply the stroke style property
    vg->stroke(style->stroke.width);
    vg->stroke(style->stroke.cap);
    vg->stroke(style->stroke.join);
    vg->strokeMiterlimit(style->stroke.miterlimit);
    if (style->stroke.dash.array.count > 0) {
        P(vg)->strokeDash(style->stroke.dash.array.data, style->stroke.dash.array.count, style->stroke.dash.offset);
    }

    //If stroke property is nullptr then do nothing
    if (style->stroke.paint.none) {
        vg->stroke(0.0f);
    } else if (style->stroke.paint.gradient) {
        Box bBox = vBox;
        if (!style->stroke.paint.gradient->userSpace) bBox = _boundingBox(vg);

        if (style->stroke.paint.gradient->type == SvgGradientType::Linear) {
             auto linear = _applyLinearGradientProperty(style->stroke.paint.gradient, bBox, style->stroke.opacity);
             vg->stroke(std::move(linear));
        } else if (style->stroke.paint.gradient->type == SvgGradientType::Radial) {
             auto radial = _applyRadialGradientProperty(style->stroke.paint.gradient, bBox, style->stroke.opacity);
             vg->stroke(std::move(radial));
        }
    } else if (style->stroke.paint.url) {
        //TODO: Apply the color pointed by url
        TVGLOG("SVG", "The stroke's url not supported.");
    } else if (style->stroke.paint.curColor) {
        //Apply the current style color
        vg->stroke(style->color.r, style->color.g, style->color.b, style->stroke.opacity);
    } else {
        //Apply the stroke color
        vg->stroke(style->stroke.paint.color.r, style->stroke.paint.color.g, style->stroke.paint.color.b, style->stroke.opacity);
    }

    _applyComposition(loaderData, vg, node, vBox, svgPath);
}


static unique_ptr<Shape> _shapeBuildHelper(SvgLoaderData& loaderData, SvgNode* node, const Box& vBox, const string& svgPath)
{
    auto shape = Shape::gen();
    if (_appendShape(loaderData, node, shape.get(), vBox, svgPath)) return shape;
    else return nullptr;
}


static bool _recognizeShape(SvgNode* node, Shape* shape)
{
    switch (node->type) {
        case SvgNodeType::Path: {
            if (node->node.path.path) {
                if (!svgPathToShape(node->node.path.path, shape)) {
                    TVGERR("SVG", "Invalid path information.");
                    return false;
                }
            }
            break;
        }
        case SvgNodeType::Ellipse: {
            shape->appendCircle(node->node.ellipse.cx, node->node.ellipse.cy, node->node.ellipse.rx, node->node.ellipse.ry);
            break;
        }
        case SvgNodeType::Polygon: {
            if (node->node.polygon.pts.count < 2) break;
            auto pts = node->node.polygon.pts.begin();
            shape->moveTo(pts[0], pts[1]);
            for (pts += 2; pts < node->node.polygon.pts.end(); pts += 2) {
                shape->lineTo(pts[0], pts[1]);
            }
            shape->close();
            break;
        }
        case SvgNodeType::Polyline: {
            if (node->node.polyline.pts.count < 2) break;
            auto pts = node->node.polyline.pts.begin();
            shape->moveTo(pts[0], pts[1]);
            for (pts += 2; pts < node->node.polyline.pts.end(); pts += 2) {
                shape->lineTo(pts[0], pts[1]);
            }
            break;
        }
        case SvgNodeType::Circle: {
            shape->appendCircle(node->node.circle.cx, node->node.circle.cy, node->node.circle.r, node->node.circle.r);
            break;
        }
        case SvgNodeType::Rect: {
            shape->appendRect(node->node.rect.x, node->node.rect.y, node->node.rect.w, node->node.rect.h, node->node.rect.rx, node->node.rect.ry);
            break;
        }
        case SvgNodeType::Line: {
            shape->moveTo(node->node.line.x1, node->node.line.y1);
            shape->lineTo(node->node.line.x2, node->node.line.y2);
            break;
        }
        default: {
            return false;
        }
    }
    return true;
}


static bool _appendShape(SvgLoaderData& loaderData, SvgNode* node, Shape* shape, const Box& vBox, const string& svgPath)
{
    if (!_recognizeShape(node, shape)) return false;

    _applyProperty(loaderData, node, shape, vBox, svgPath, false);
    return true;
}


static bool _appendClipShape(SvgLoaderData& loaderData, SvgNode* node, Shape* shape, const Box& vBox, const string& svgPath, const Matrix* transform)
{
    //The 'transform' matrix has higher priority than the node->transform, since it already contains it
    auto m = transform ? transform : (node->transform ? node->transform : nullptr);

    uint32_t currentPtsCnt = 0;
    if (m) {
        const Point *tmp = nullptr;
        currentPtsCnt = shape->pathCoords(&tmp);
    }

    if (!_recognizeShape(node, shape)) return false;

    if (m) {
        const Point *pts = nullptr;
        auto ptsCnt = shape->pathCoords(&pts);

        auto p = const_cast<Point*>(pts) + currentPtsCnt;
        while (currentPtsCnt++ < ptsCnt) {
            *p *= *m;
            ++p;
        }
    }

    _applyProperty(loaderData, node, shape, vBox, svgPath, true);
    return true;
}


enum class imageMimeTypeEncoding
{
    base64 = 0x1,
    utf8 = 0x2
};

constexpr imageMimeTypeEncoding operator|(imageMimeTypeEncoding a, imageMimeTypeEncoding b) {
    return static_cast<imageMimeTypeEncoding>(static_cast<int>(a) | static_cast<int>(b));
}

constexpr bool operator&(imageMimeTypeEncoding a, imageMimeTypeEncoding b) {
    return (static_cast<int>(a) & static_cast<int>(b));
}


static constexpr struct
{
    const char* name;
    int sz;
    imageMimeTypeEncoding encoding;
} imageMimeTypes[] = {
    {"jpeg", sizeof("jpeg"), imageMimeTypeEncoding::base64},
    {"png", sizeof("png"), imageMimeTypeEncoding::base64},
    {"webp", sizeof("webp"), imageMimeTypeEncoding::base64},
    {"svg+xml", sizeof("svg+xml"), imageMimeTypeEncoding::base64 | imageMimeTypeEncoding::utf8},
};


static bool _isValidImageMimeTypeAndEncoding(const char** href, const char** mimetype, imageMimeTypeEncoding* encoding) {
    if (strncmp(*href, "image/", sizeof("image/") - 1)) return false; //not allowed mime type
    *href += sizeof("image/") - 1;

    //RFC2397 data:[<mediatype>][;base64],<data>
    //mediatype  := [ type "/" subtype ] *( ";" parameter )
    //parameter  := attribute "=" value
    for (unsigned int i = 0; i < sizeof(imageMimeTypes) / sizeof(imageMimeTypes[0]); i++) {
        if (!strncmp(*href, imageMimeTypes[i].name, imageMimeTypes[i].sz - 1)) {
            *href += imageMimeTypes[i].sz  - 1;
            *mimetype = imageMimeTypes[i].name;

            while (**href && **href != ',') {
                while (**href && **href != ';') ++(*href);
                if (!**href) return false;
                ++(*href);

                if (imageMimeTypes[i].encoding & imageMimeTypeEncoding::base64) {
                    if (!strncmp(*href, "base64,", sizeof("base64,") - 1)) {
                        *href += sizeof("base64,") - 1;
                        *encoding = imageMimeTypeEncoding::base64;
                        return true; //valid base64
                    }
                }
                if (imageMimeTypes[i].encoding & imageMimeTypeEncoding::utf8) {
                    if (!strncmp(*href, "utf8,", sizeof("utf8,") - 1)) {
                        *href += sizeof("utf8,") - 1;
                        *encoding = imageMimeTypeEncoding::utf8;
                        return true; //valid utf8
                    }
                }
            }
            //no encoding defined
            if (**href == ',' && (imageMimeTypes[i].encoding & imageMimeTypeEncoding::utf8)) {
                ++(*href);
                *encoding = imageMimeTypeEncoding::utf8;
                return true; //allow no encoding defined if utf8 expected
            }
            return false;
        }
    }
    return false;
}

#include "tvgTaskScheduler.h"

static unique_ptr<Picture> _imageBuildHelper(SvgLoaderData& loaderData, SvgNode* node, const Box& vBox, const string& svgPath)
{
    if (!node->node.image.href || !strlen(node->node.image.href)) return nullptr;
    auto picture = Picture::gen();

    TaskScheduler::async(false);    //force to load a picture on the same thread

    const char* href = node->node.image.href;
    if (!strncmp(href, "data:", sizeof("data:") - 1)) {
        href += sizeof("data:") - 1;
        const char* mimetype;
        imageMimeTypeEncoding encoding;
        if (!_isValidImageMimeTypeAndEncoding(&href, &mimetype, &encoding)) return nullptr; //not allowed mime type or encoding
        char *decoded = nullptr;
        if (encoding == imageMimeTypeEncoding::base64) {
            auto size = b64Decode(href, strlen(href), &decoded);
            if (picture->load(decoded, size, mimetype, false) != Result::Success) {
                free(decoded);
                TaskScheduler::async(true);
                return nullptr;
            }
        } else {
            auto size = svgUtilURLDecode(href, &decoded);
            if (picture->load(decoded, size, mimetype, false) != Result::Success) {
                free(decoded);
                TaskScheduler::async(true);
                return nullptr;
            }
        }
        loaderData.images.push(decoded);
    } else {
        if (!strncmp(href, "file://", sizeof("file://") - 1)) href += sizeof("file://") - 1;
        //TODO: protect against recursive svg image loading
        //Temporarily disable embedded svg:
        const char *dot = strrchr(href, '.');
        if (dot && !strcmp(dot, ".svg")) {
            TVGLOG("SVG", "Embedded svg file is disabled.");
            TaskScheduler::async(true);
            return nullptr;
        }
        string imagePath = href;
        if (strncmp(href, "/", 1)) {
            auto last = svgPath.find_last_of("/");
            imagePath = svgPath.substr(0, (last == string::npos ? 0 : last + 1)) + imagePath;
        }
        if (picture->load(imagePath) != Result::Success) {
            TaskScheduler::async(true);
            return nullptr;
        }
    }

    TaskScheduler::async(true);

    float w, h;
    Matrix m = {1, 0, 0, 0, 1, 0, 0, 0, 1};
    if (picture->size(&w, &h) == Result::Success && w  > 0 && h > 0) {
        auto sx = node->node.image.w / w;
        auto sy = node->node.image.h / h;
        m = {sx, 0, node->node.image.x, 0, sy, node->node.image.y, 0, 0, 1};
    }
    if (node->transform) m = *node->transform * m;
    picture->transform(m);

    _applyComposition(loaderData, picture.get(), node, vBox, svgPath);

    return picture;
}


static Matrix _calculateAspectRatioMatrix(AspectRatioAlign align, AspectRatioMeetOrSlice meetOrSlice, float width, float height, const Box& box)
{
    auto sx = width / box.w;
    auto sy = height / box.h;
    auto tvx = box.x * sx;
    auto tvy = box.y * sy;

    if (align == AspectRatioAlign::None)
        return {sx, 0, -tvx, 0, sy, -tvy, 0, 0, 1};

    //Scale
    if (meetOrSlice == AspectRatioMeetOrSlice::Meet) {
        if (sx < sy) sy = sx;
        else sx = sy;
    } else {
        if (sx < sy) sx = sy;
        else sy = sx;
    }

    //Align
    tvx = box.x * sx;
    tvy = box.y * sy;
    auto tvw = box.w * sx;
    auto tvh = box.h * sy;

    switch (align) {
        case AspectRatioAlign::XMinYMin: {
            break;
        }
        case AspectRatioAlign::XMidYMin: {
            tvx -= (width - tvw) * 0.5f;
            break;
        }
        case AspectRatioAlign::XMaxYMin: {
            tvx -= width - tvw;
            break;
        }
        case AspectRatioAlign::XMinYMid: {
            tvy -= (height - tvh) * 0.5f;
            break;
        }
        case AspectRatioAlign::XMidYMid: {
            tvx -= (width - tvw) * 0.5f;
            tvy -= (height - tvh) * 0.5f;
            break;
        }
        case AspectRatioAlign::XMaxYMid: {
            tvx -= width - tvw;
            tvy -= (height - tvh) * 0.5f;
            break;
        }
        case AspectRatioAlign::XMinYMax: {
            tvy -= height - tvh;
            break;
        }
        case AspectRatioAlign::XMidYMax: {
            tvx -= (width - tvw) * 0.5f;
            tvy -= height - tvh;
            break;
        }
        case AspectRatioAlign::XMaxYMax: {
            tvx -= width - tvw;
            tvy -= height - tvh;
            break;
        }
        default: {
            break;
        }
    }

    return {sx, 0, -tvx, 0, sy, -tvy, 0, 0, 1};
}


static unique_ptr<Scene> _useBuildHelper(SvgLoaderData& loaderData, const SvgNode* node, const Box& vBox, const string& svgPath, int depth, bool* isMaskWhite)
{
    auto scene = _sceneBuildHelper(loaderData, node, vBox, svgPath, false, depth + 1, isMaskWhite);

    // mUseTransform = mUseTransform * mTranslate
    Matrix mUseTransform = {1, 0, 0, 0, 1, 0, 0, 0, 1};
    if (node->transform) mUseTransform = *node->transform;
    if (node->node.use.x != 0.0f || node->node.use.y != 0.0f) {
        Matrix mTranslate = {1, 0, node->node.use.x, 0, 1, node->node.use.y, 0, 0, 1};
        mUseTransform *= mTranslate;
    }

    if (node->node.use.symbol) {
        auto symbol = node->node.use.symbol->node.symbol;

        auto width = (symbol.hasWidth ? symbol.w : vBox.w);
        if (node->node.use.isWidthSet) width = node->node.use.w;
        auto height = (symbol.hasHeight ? symbol.h : vBox.h);;
        if (node->node.use.isHeightSet) height = node->node.use.h;
        auto vw = (symbol.hasViewBox ? symbol.vw : width);
        auto vh = (symbol.hasViewBox ? symbol.vh : height);

        Matrix mViewBox = {1, 0, 0, 0, 1, 0, 0, 0, 1};
        if ((!tvg::equal(width, vw) || !tvg::equal(height, vh)) && vw > 0 && vh > 0) {
            Box box = {symbol.vx, symbol.vy, vw, vh};
            mViewBox = _calculateAspectRatioMatrix(symbol.align, symbol.meetOrSlice, width, height, box);
        } else if (!tvg::zero(symbol.vx) || !tvg::zero(symbol.vy)) {
            mViewBox = {1, 0, -symbol.vx, 0, 1, -symbol.vy, 0, 0, 1};
        }

        // mSceneTransform = mUseTransform * mSymbolTransform * mViewBox
        Matrix mSceneTransform = mViewBox;
        if (node->node.use.symbol->transform) {
            mSceneTransform = *node->node.use.symbol->transform * mViewBox;
        }
        mSceneTransform = mUseTransform * mSceneTransform;
        scene->transform(mSceneTransform);

        if (!node->node.use.symbol->node.symbol.overflowVisible) {
            auto viewBoxClip = Shape::gen();
            viewBoxClip->appendRect(0, 0, width, height, 0, 0);

            // mClipTransform = mUseTransform * mSymbolTransform
            Matrix mClipTransform = mUseTransform;
            if (node->node.use.symbol->transform) {
                mClipTransform = mUseTransform * *node->node.use.symbol->transform;
            }
            viewBoxClip->transform(mClipTransform);

            scene->clip(std::move(viewBoxClip));
        }
    } else {
        scene->transform(mUseTransform);
    }

    return scene;
}


static void _applyTextFill(SvgStyleProperty* style, Text* text, const Box& vBox)
{
    //If fill property is nullptr then do nothing
    if (style->fill.paint.none) {
        //Do nothing
    } else if (style->fill.paint.gradient) {
        Box bBox = vBox;
        if (!style->fill.paint.gradient->userSpace) bBox = _boundingBox(text);

        if (style->fill.paint.gradient->type == SvgGradientType::Linear) {
            auto linear = _applyLinearGradientProperty(style->fill.paint.gradient, bBox, style->fill.opacity);
            text->fill(std::move(linear));
        } else if (style->fill.paint.gradient->type == SvgGradientType::Radial) {
            auto radial = _applyRadialGradientProperty(style->fill.paint.gradient, bBox, style->fill.opacity);
            text->fill(std::move(radial));
        }
    } else if (style->fill.paint.url) {
        //TODO: Apply the color pointed by url
        TVGLOG("SVG", "The fill's url not supported.");
    } else if (style->fill.paint.curColor) {
        //Apply the current style color
        text->fill(style->color.r, style->color.g, style->color.b);
        text->opacity(style->fill.opacity);
    } else {
        //Apply the fill color
        text->fill(style->fill.paint.color.r, style->fill.paint.color.g, style->fill.paint.color.b);
        text->opacity(style->fill.opacity);
    }
}


static unique_ptr<Text> _textBuildHelper(SvgLoaderData& loaderData, const SvgNode* node, const Box& vBox, const string& svgPath)
{
    auto textNode = &node->node.text;
    if (!textNode->text) return nullptr;
    auto text = Text::gen();

    Matrix textTransform = {1, 0, 0, 0, 1, 0, 0, 0, 1};
    if (node->transform) textTransform = *node->transform;
    translateR(&textTransform, node->node.text.x, node->node.text.y - textNode->fontSize);
    text->transform(textTransform);

    //TODO: handle def values of font and size as used in a system?
    const float ptPerPx = 0.75f; //1 pt = 1/72; 1 in = 96 px; -> 72/96 = 0.75
    auto fontSizePt = textNode->fontSize * ptPerPx;
    if (textNode->fontFamily) text->font(textNode->fontFamily, fontSizePt);
    text->text(textNode->text);

    _applyTextFill(node->style, text.get(), vBox);
    _applyComposition(loaderData, text.get(), node, vBox, svgPath);

    return text;
}


static unique_ptr<Scene> _sceneBuildHelper(SvgLoaderData& loaderData, const SvgNode* node, const Box& vBox, const string& svgPath, bool mask, int depth, bool* isMaskWhite)
{
    /* Exception handling: Prevent invalid SVG data input.
       The size is the arbitrary value, we need an experimental size. */
    if (depth > 2192) {
        TVGERR("SVG", "Infinite recursive call - stopped after %d calls! Svg file may be incorrectly formatted.", depth);
        return nullptr;
    }

    if (_isGroupType(node->type) || mask) {
        auto scene = Scene::gen();
        // For a Symbol node, the viewBox transformation has to be applied first - see _useBuildHelper()
        if (!mask && node->transform && node->type != SvgNodeType::Symbol) scene->transform(*node->transform);

        if (node->style->display && node->style->opacity != 0) {
            auto child = node->child.data;
            for (uint32_t i = 0; i < node->child.count; ++i, ++child) {
                if (_isGroupType((*child)->type)) {
                    if ((*child)->type == SvgNodeType::Use)
                        scene->push(_useBuildHelper(loaderData, *child, vBox, svgPath, depth + 1, isMaskWhite));
                    else if (!((*child)->type == SvgNodeType::Symbol && node->type != SvgNodeType::Use))
                        scene->push(_sceneBuildHelper(loaderData, *child, vBox, svgPath, false, depth + 1, isMaskWhite));
                } else if ((*child)->type == SvgNodeType::Image) {
                    auto image = _imageBuildHelper(loaderData, *child, vBox, svgPath);
                    if (image) {
                        scene->push(std::move(image));
                        if (isMaskWhite) *isMaskWhite = false;
                    }
                } else if ((*child)->type == SvgNodeType::Text) {
                    auto text = _textBuildHelper(loaderData, *child, vBox, svgPath);
                    if (text) scene->push(std::move(text));
                } else if ((*child)->type != SvgNodeType::Mask) {
                    auto shape = _shapeBuildHelper(loaderData, *child, vBox, svgPath);
                    if (shape) {
                        if (isMaskWhite) {
                            uint8_t r, g, b;
                            shape->fillColor(&r, &g, &b);
                            if (shape->fill() || r < 255 || g < 255 || b < 255 || shape->strokeFill() ||
                                (shape->strokeColor(&r, &g, &b) == Result::Success && (r < 255 || g < 255 || b < 255))) {
                                *isMaskWhite = false;
                            }
                        }
                        scene->push(std::move(shape));
                    }
                }
            }
            _applyComposition(loaderData, scene.get(), node, vBox, svgPath);
            scene->opacity(node->style->opacity);
        }
        return scene;
    }
    return nullptr;
}


static void _updateInvalidViewSize(const Scene* scene, Box& vBox, float& w, float& h, SvgViewFlag viewFlag)
{
    bool validWidth = (viewFlag & SvgViewFlag::Width);
    bool validHeight = (viewFlag & SvgViewFlag::Height);

    float x, y;
    scene->bounds(&x, &y, &vBox.w, &vBox.h, false);
    if (!validWidth && !validHeight) {
        vBox.x = x;
        vBox.y = y;
    } else {
        if (validWidth) vBox.w = w;
        if (validHeight) vBox.h = h;
    }

    //the size would have 1x1 or percentage values.
    if (!validWidth) w *= vBox.w;
    if (!validHeight) h *= vBox.h;
}

/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/

Scene* svgSceneBuild(SvgLoaderData& loaderData, Box vBox, float w, float h, AspectRatioAlign align, AspectRatioMeetOrSlice meetOrSlice, const string& svgPath, SvgViewFlag viewFlag)
{
    //TODO: aspect ratio is valid only if viewBox was set

    if (!loaderData.doc || (loaderData.doc->type != SvgNodeType::Doc)) return nullptr;

    auto docNode = _sceneBuildHelper(loaderData, loaderData.doc, vBox, svgPath, false, 0);

    if (!(viewFlag & SvgViewFlag::Viewbox)) _updateInvalidViewSize(docNode.get(), vBox, w, h, viewFlag);

    if (!tvg::equal(w, vBox.w) || !tvg::equal(h, vBox.h)) {
        Matrix m = _calculateAspectRatioMatrix(align, meetOrSlice, w, h, vBox);
        docNode->transform(m);
    } else if (!tvg::zero(vBox.x) || !tvg::zero(vBox.y)) {
        docNode->translate(-vBox.x, -vBox.y);
    }

    auto viewBoxClip = Shape::gen();
    viewBoxClip->appendRect(0, 0, w, h);

    auto compositeLayer = Scene::gen();
    compositeLayer->clip(std::move(viewBoxClip));
    compositeLayer->push(std::move(docNode));

    auto root = Scene::gen();
    root->push(std::move(compositeLayer));

    loaderData.doc->node.doc.vx = vBox.x;
    loaderData.doc->node.doc.vy = vBox.y;
    loaderData.doc->node.doc.vw = vBox.w;
    loaderData.doc->node.doc.vh = vBox.h;
    loaderData.doc->node.doc.w = w;
    loaderData.doc->node.doc.h = h;

    return root.release();
}

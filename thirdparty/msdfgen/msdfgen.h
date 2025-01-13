
#pragma once

/*
 * MULTI-CHANNEL SIGNED DISTANCE FIELD GENERATOR
 * ---------------------------------------------
 * A utility by Viktor Chlumsky, (c) 2014 - 2024
 *
 * The technique used to generate multi-channel distance fields in this code
 * has been developed by Viktor Chlumsky in 2014 for his master's thesis,
 * "Shape Decomposition for Multi-Channel Distance Fields". It provides improved
 * quality of sharp corners in glyphs and other 2D shapes compared to monochrome
 * distance fields. To reconstruct an image of the shape, apply the median of three
 * operation on the triplet of sampled signed distance values.
 *
 */

#include "core/base.h"
#include "core/arithmetics.hpp"
#include "core/Vector2.hpp"
#include "core/Range.hpp"
#include "core/Projection.h"
#include "core/DistanceMapping.h"
#include "core/SDFTransformation.h"
#include "core/Scanline.h"
#include "core/Shape.h"
#include "core/BitmapRef.hpp"
#include "core/Bitmap.h"
#include "core/bitmap-interpolation.hpp"
#include "core/pixel-conversion.hpp"
#include "core/edge-coloring.h"
#include "core/generator-config.h"
#include "core/msdf-error-correction.h"
#include "core/render-sdf.h"
#include "core/rasterization.h"
#include "core/sdf-error-estimation.h"
#include "core/save-bmp.h"
#include "core/save-tiff.h"
#include "core/save-rgba.h"
#include "core/save-fl32.h"
#include "core/shape-description.h"
#include "core/export-svg.h"

namespace msdfgen {

/// Generates a conventional single-channel signed distance field.
void generateSDF(const BitmapRef<float, 1> &output, const Shape &shape, const SDFTransformation &transformation, const GeneratorConfig &config = GeneratorConfig());

/// Generates a single-channel signed perpendicular distance field.
void generatePSDF(const BitmapRef<float, 1> &output, const Shape &shape, const SDFTransformation &transformation, const GeneratorConfig &config = GeneratorConfig());

/// Generates a multi-channel signed distance field. Edge colors must be assigned first! (See edgeColoringSimple)
void generateMSDF(const BitmapRef<float, 3> &output, const Shape &shape, const SDFTransformation &transformation, const MSDFGeneratorConfig &config = MSDFGeneratorConfig());

/// Generates a multi-channel signed distance field with true distance in the alpha channel. Edge colors must be assigned first.
void generateMTSDF(const BitmapRef<float, 4> &output, const Shape &shape, const SDFTransformation &transformation, const MSDFGeneratorConfig &config = MSDFGeneratorConfig());

// Old version of the function API's kept for backwards compatibility
void generateSDF(const BitmapRef<float, 1> &output, const Shape &shape, const Projection &projection, Range range, const GeneratorConfig &config = GeneratorConfig());
void generatePSDF(const BitmapRef<float, 1> &output, const Shape &shape, const Projection &projection, Range range, const GeneratorConfig &config = GeneratorConfig());
void generatePseudoSDF(const BitmapRef<float, 1> &output, const Shape &shape, const Projection &projection, Range range, const GeneratorConfig &config = GeneratorConfig());
void generateMSDF(const BitmapRef<float, 3> &output, const Shape &shape, const Projection &projection, Range range, const MSDFGeneratorConfig &config = MSDFGeneratorConfig());
void generateMTSDF(const BitmapRef<float, 4> &output, const Shape &shape, const Projection &projection, Range range, const MSDFGeneratorConfig &config = MSDFGeneratorConfig());

void generateSDF(const BitmapRef<float, 1> &output, const Shape &shape, Range range, const Vector2 &scale, const Vector2 &translate, bool overlapSupport = true);
void generatePSDF(const BitmapRef<float, 1> &output, const Shape &shape, Range range, const Vector2 &scale, const Vector2 &translate, bool overlapSupport = true);
void generatePseudoSDF(const BitmapRef<float, 1> &output, const Shape &shape, Range range, const Vector2 &scale, const Vector2 &translate, bool overlapSupport = true);
void generateMSDF(const BitmapRef<float, 3> &output, const Shape &shape, Range range, const Vector2 &scale, const Vector2 &translate, const ErrorCorrectionConfig &errorCorrectionConfig = ErrorCorrectionConfig(), bool overlapSupport = true);
void generateMTSDF(const BitmapRef<float, 4> &output, const Shape &shape, Range range, const Vector2 &scale, const Vector2 &translate, const ErrorCorrectionConfig &errorCorrectionConfig = ErrorCorrectionConfig(), bool overlapSupport = true);

// Original simpler versions of the previous functions, which work well under normal circumstances, but cannot deal with overlapping contours.
void generateSDF_legacy(const BitmapRef<float, 1> &output, const Shape &shape, Range range, const Vector2 &scale, const Vector2 &translate);
void generatePSDF_legacy(const BitmapRef<float, 1> &output, const Shape &shape, Range range, const Vector2 &scale, const Vector2 &translate);
void generatePseudoSDF_legacy(const BitmapRef<float, 1> &output, const Shape &shape, Range range, const Vector2 &scale, const Vector2 &translate);
void generateMSDF_legacy(const BitmapRef<float, 3> &output, const Shape &shape, Range range, const Vector2 &scale, const Vector2 &translate, ErrorCorrectionConfig errorCorrectionConfig = ErrorCorrectionConfig());
void generateMTSDF_legacy(const BitmapRef<float, 4> &output, const Shape &shape, Range range, const Vector2 &scale, const Vector2 &translate, ErrorCorrectionConfig errorCorrectionConfig = ErrorCorrectionConfig());

}


#pragma once

#include "Vector2.hpp"
#include "Range.hpp"
#include "Projection.h"
#include "SDFTransformation.h"
#include "Shape.h"
#include "BitmapRef.hpp"
#include "generator-config.h"

namespace msdfgen {

/// Predicts potential artifacts caused by the interpolation of the MSDF and corrects them by converting nearby texels to single-channel.
void msdfErrorCorrection(const BitmapRef<float, 3> &sdf, const Shape &shape, const SDFTransformation &transformation, const MSDFGeneratorConfig &config = MSDFGeneratorConfig());
void msdfErrorCorrection(const BitmapRef<float, 4> &sdf, const Shape &shape, const SDFTransformation &transformation, const MSDFGeneratorConfig &config = MSDFGeneratorConfig());
void msdfErrorCorrection(const BitmapRef<float, 3> &sdf, const Shape &shape, const Projection &projection, Range range, const MSDFGeneratorConfig &config = MSDFGeneratorConfig());
void msdfErrorCorrection(const BitmapRef<float, 4> &sdf, const Shape &shape, const Projection &projection, Range range, const MSDFGeneratorConfig &config = MSDFGeneratorConfig());

/// Applies the simplified error correction to all discontiunous distances (INDISCRIMINATE mode). Does not need shape or translation.
void msdfFastDistanceErrorCorrection(const BitmapRef<float, 3> &sdf, const SDFTransformation &transformation, double minDeviationRatio = ErrorCorrectionConfig::defaultMinDeviationRatio);
void msdfFastDistanceErrorCorrection(const BitmapRef<float, 4> &sdf, const SDFTransformation &transformation, double minDeviationRatio = ErrorCorrectionConfig::defaultMinDeviationRatio);
void msdfFastDistanceErrorCorrection(const BitmapRef<float, 3> &sdf, const Projection &projection, Range range, double minDeviationRatio = ErrorCorrectionConfig::defaultMinDeviationRatio);
void msdfFastDistanceErrorCorrection(const BitmapRef<float, 4> &sdf, const Projection &projection, Range range, double minDeviationRatio = ErrorCorrectionConfig::defaultMinDeviationRatio);

/// Applies the simplified error correction to edges only (EDGE_ONLY mode). Does not need shape or translation.
void msdfFastEdgeErrorCorrection(const BitmapRef<float, 3> &sdf, const SDFTransformation &transformation, double minDeviationRatio = ErrorCorrectionConfig::defaultMinDeviationRatio);
void msdfFastEdgeErrorCorrection(const BitmapRef<float, 4> &sdf, const SDFTransformation &transformation, double minDeviationRatio = ErrorCorrectionConfig::defaultMinDeviationRatio);
void msdfFastEdgeErrorCorrection(const BitmapRef<float, 3> &sdf, const Projection &projection, Range range, double minDeviationRatio = ErrorCorrectionConfig::defaultMinDeviationRatio);
void msdfFastEdgeErrorCorrection(const BitmapRef<float, 4> &sdf, const Projection &projection, Range range, double minDeviationRatio = ErrorCorrectionConfig::defaultMinDeviationRatio);

/// The original version of the error correction algorithm.
void msdfErrorCorrection_legacy(const BitmapRef<float, 3> &output, const Vector2 &threshold);
void msdfErrorCorrection_legacy(const BitmapRef<float, 4> &output, const Vector2 &threshold);

}

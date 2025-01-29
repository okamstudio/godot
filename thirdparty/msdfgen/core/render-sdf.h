
#pragma once

#include "Vector2.hpp"
#include "Range.hpp"
#include "BitmapRef.hpp"

namespace msdfgen {

/// Reconstructs the shape's appearance into output from the distance field sdf.
void renderSDF(const BitmapRef<float, 1> &output, const BitmapConstRef<float, 1> &sdf, Range sdfPxRange = 0, float sdThreshold = .5f);
void renderSDF(const BitmapRef<float, 3> &output, const BitmapConstRef<float, 1> &sdf, Range sdfPxRange = 0, float sdThreshold = .5f);
void renderSDF(const BitmapRef<float, 1> &output, const BitmapConstRef<float, 3> &sdf, Range sdfPxRange = 0, float sdThreshold = .5f);
void renderSDF(const BitmapRef<float, 3> &output, const BitmapConstRef<float, 3> &sdf, Range sdfPxRange = 0, float sdThreshold = .5f);
void renderSDF(const BitmapRef<float, 1> &output, const BitmapConstRef<float, 4> &sdf, Range sdfPxRange = 0, float sdThreshold = .5f);
void renderSDF(const BitmapRef<float, 4> &output, const BitmapConstRef<float, 4> &sdf, Range sdfPxRange = 0, float sdThreshold = .5f);

/// Snaps the values of the floating-point bitmaps into one of the 256 values representable in a standard 8-bit bitmap.
void simulate8bit(const BitmapRef<float, 1> &bitmap);
void simulate8bit(const BitmapRef<float, 3> &bitmap);
void simulate8bit(const BitmapRef<float, 4> &bitmap);

}

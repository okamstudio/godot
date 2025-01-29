
#pragma once

#include "BitmapRef.hpp"

namespace msdfgen {

/// Saves the bitmap as a simple RGBA file, which can be decoded trivially.
bool saveRgba(const BitmapConstRef<byte, 1> &bitmap, const char *filename);
bool saveRgba(const BitmapConstRef<byte, 3> &bitmap, const char *filename);
bool saveRgba(const BitmapConstRef<byte, 4> &bitmap, const char *filename);
bool saveRgba(const BitmapConstRef<float, 1> &bitmap, const char *filename);
bool saveRgba(const BitmapConstRef<float, 3> &bitmap, const char *filename);
bool saveRgba(const BitmapConstRef<float, 4> &bitmap, const char *filename);

}


#include "render-sdf.h"

#include "arithmetics.hpp"
#include "DistanceMapping.h"
#include "pixel-conversion.hpp"
#include "bitmap-interpolation.hpp"

namespace msdfgen {

static float distVal(float dist, DistanceMapping mapping) {
    return (float) clamp(mapping(dist)+.5);
}

void renderSDF(const BitmapRef<float, 1> &output, const BitmapConstRef<float, 1> &sdf, Range sdfPxRange, float sdThreshold) {
    Vector2 scale((double) sdf.width/output.width, (double) sdf.height/output.height);
    if (sdfPxRange.lower == sdfPxRange.upper) {
        for (int y = 0; y < output.height; ++y) {
            for (int x = 0; x < output.width; ++x) {
                float sd;
                interpolate(&sd, sdf, scale*Point2(x+.5, y+.5));
                *output(x, y) = float(sd >= sdThreshold);
            }
        }
    } else {
        sdfPxRange *= (double) (output.width+output.height)/(sdf.width+sdf.height);
        DistanceMapping distanceMapping = DistanceMapping::inverse(sdfPxRange);
        float sdBias = .5f-sdThreshold;
        for (int y = 0; y < output.height; ++y) {
            for (int x = 0; x < output.width; ++x) {
                float sd;
                interpolate(&sd, sdf, scale*Point2(x+.5, y+.5));
                *output(x, y) = distVal(sd+sdBias, distanceMapping);
            }
        }
    }

}

void renderSDF(const BitmapRef<float, 3> &output, const BitmapConstRef<float, 1> &sdf, Range sdfPxRange, float sdThreshold) {
    Vector2 scale((double) sdf.width/output.width, (double) sdf.height/output.height);
    if (sdfPxRange.lower == sdfPxRange.upper) {
        for (int y = 0; y < output.height; ++y) {
            for (int x = 0; x < output.width; ++x) {
                float sd;
                interpolate(&sd, sdf, scale*Point2(x+.5, y+.5));
                float v = float(sd >= sdThreshold);
                output(x, y)[0] = v;
                output(x, y)[1] = v;
                output(x, y)[2] = v;
            }
        }
    } else {
        sdfPxRange *= (double) (output.width+output.height)/(sdf.width+sdf.height);
        DistanceMapping distanceMapping = DistanceMapping::inverse(sdfPxRange);
        float sdBias = .5f-sdThreshold;
        for (int y = 0; y < output.height; ++y) {
            for (int x = 0; x < output.width; ++x) {
                float sd;
                interpolate(&sd, sdf, scale*Point2(x+.5, y+.5));
                float v = distVal(sd+sdBias, distanceMapping);
                output(x, y)[0] = v;
                output(x, y)[1] = v;
                output(x, y)[2] = v;
            }
        }
    }
}

void renderSDF(const BitmapRef<float, 1> &output, const BitmapConstRef<float, 3> &sdf, Range sdfPxRange, float sdThreshold) {
    Vector2 scale((double) sdf.width/output.width, (double) sdf.height/output.height);
    if (sdfPxRange.lower == sdfPxRange.upper) {
        for (int y = 0; y < output.height; ++y) {
            for (int x = 0; x < output.width; ++x) {
                float sd[3];
                interpolate(sd, sdf, scale*Point2(x+.5, y+.5));
                *output(x, y) = float(median(sd[0], sd[1], sd[2]) >= sdThreshold);
            }
        }
    } else {
        sdfPxRange *= (double) (output.width+output.height)/(sdf.width+sdf.height);
        DistanceMapping distanceMapping = DistanceMapping::inverse(sdfPxRange);
        float sdBias = .5f-sdThreshold;
        for (int y = 0; y < output.height; ++y) {
            for (int x = 0; x < output.width; ++x) {
                float sd[3];
                interpolate(sd, sdf, scale*Point2(x+.5, y+.5));
                *output(x, y) = distVal(median(sd[0], sd[1], sd[2])+sdBias, distanceMapping);
            }
        }
    }
}

void renderSDF(const BitmapRef<float, 3> &output, const BitmapConstRef<float, 3> &sdf, Range sdfPxRange, float sdThreshold) {
    Vector2 scale((double) sdf.width/output.width, (double) sdf.height/output.height);
    if (sdfPxRange.lower == sdfPxRange.upper) {
        for (int y = 0; y < output.height; ++y) {
            for (int x = 0; x < output.width; ++x) {
                float sd[3];
                interpolate(sd, sdf, scale*Point2(x+.5, y+.5));
                output(x, y)[0] = float(sd[0] >= sdThreshold);
                output(x, y)[1] = float(sd[1] >= sdThreshold);
                output(x, y)[2] = float(sd[2] >= sdThreshold);
            }
        }
    } else {
        sdfPxRange *= (double) (output.width+output.height)/(sdf.width+sdf.height);
        DistanceMapping distanceMapping = DistanceMapping::inverse(sdfPxRange);
        float sdBias = .5f-sdThreshold;
        for (int y = 0; y < output.height; ++y) {
            for (int x = 0; x < output.width; ++x) {
                float sd[3];
                interpolate(sd, sdf, scale*Point2(x+.5, y+.5));
                output(x, y)[0] = distVal(sd[0]+sdBias, distanceMapping);
                output(x, y)[1] = distVal(sd[1]+sdBias, distanceMapping);
                output(x, y)[2] = distVal(sd[2]+sdBias, distanceMapping);
            }
        }
    }
}

void renderSDF(const BitmapRef<float, 1> &output, const BitmapConstRef<float, 4> &sdf, Range sdfPxRange, float sdThreshold) {
    Vector2 scale((double) sdf.width/output.width, (double) sdf.height/output.height);
    if (sdfPxRange.lower == sdfPxRange.upper) {
        for (int y = 0; y < output.height; ++y) {
            for (int x = 0; x < output.width; ++x) {
                float sd[4];
                interpolate(sd, sdf, scale*Point2(x+.5, y+.5));
                *output(x, y) = float(median(sd[0], sd[1], sd[2]) >= sdThreshold);
            }
        }
    } else {
        sdfPxRange *= (double) (output.width+output.height)/(sdf.width+sdf.height);
        DistanceMapping distanceMapping = DistanceMapping::inverse(sdfPxRange);
        float sdBias = .5f-sdThreshold;
        for (int y = 0; y < output.height; ++y) {
            for (int x = 0; x < output.width; ++x) {
                float sd[4];
                interpolate(sd, sdf, scale*Point2(x+.5, y+.5));
                *output(x, y) = distVal(median(sd[0], sd[1], sd[2])+sdBias, distanceMapping);
            }
        }
    }
}

void renderSDF(const BitmapRef<float, 4> &output, const BitmapConstRef<float, 4> &sdf, Range sdfPxRange, float sdThreshold) {
    Vector2 scale((double) sdf.width/output.width, (double) sdf.height/output.height);
    if (sdfPxRange.lower == sdfPxRange.upper) {
        for (int y = 0; y < output.height; ++y) {
            for (int x = 0; x < output.width; ++x) {
                float sd[4];
                interpolate(sd, sdf, scale*Point2(x+.5, y+.5));
                output(x, y)[0] = float(sd[0] >= sdThreshold);
                output(x, y)[1] = float(sd[1] >= sdThreshold);
                output(x, y)[2] = float(sd[2] >= sdThreshold);
                output(x, y)[3] = float(sd[3] >= sdThreshold);
            }
        }
    } else {
        sdfPxRange *= (double) (output.width+output.height)/(sdf.width+sdf.height);
        DistanceMapping distanceMapping = DistanceMapping::inverse(sdfPxRange);
        float sdBias = .5f-sdThreshold;
        for (int y = 0; y < output.height; ++y) {
            for (int x = 0; x < output.width; ++x) {
                float sd[4];
                interpolate(sd, sdf, scale*Point2(x+.5, y+.5));
                output(x, y)[0] = distVal(sd[0]+sdBias, distanceMapping);
                output(x, y)[1] = distVal(sd[1]+sdBias, distanceMapping);
                output(x, y)[2] = distVal(sd[2]+sdBias, distanceMapping);
                output(x, y)[3] = distVal(sd[3]+sdBias, distanceMapping);
            }
        }
    }
}

void simulate8bit(const BitmapRef<float, 1> &bitmap) {
    const float *end = bitmap.pixels+1*bitmap.width*bitmap.height;
    for (float *p = bitmap.pixels; p < end; ++p)
        *p = pixelByteToFloat(pixelFloatToByte(*p));
}

void simulate8bit(const BitmapRef<float, 3> &bitmap) {
    const float *end = bitmap.pixels+3*bitmap.width*bitmap.height;
    for (float *p = bitmap.pixels; p < end; ++p)
        *p = pixelByteToFloat(pixelFloatToByte(*p));
}

void simulate8bit(const BitmapRef<float, 4> &bitmap) {
    const float *end = bitmap.pixels+4*bitmap.width*bitmap.height;
    for (float *p = bitmap.pixels; p < end; ++p)
        *p = pixelByteToFloat(pixelFloatToByte(*p));
}

}

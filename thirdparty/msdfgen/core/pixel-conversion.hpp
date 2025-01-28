
#pragma once

#include "arithmetics.hpp"

namespace msdfgen {

inline byte pixelFloatToByte(float x) {
    return byte(~int(255.5f-255.f*clamp(x)));
}

inline float pixelByteToFloat(byte x) {
    return 1.f/255.f*float(x);
}

}

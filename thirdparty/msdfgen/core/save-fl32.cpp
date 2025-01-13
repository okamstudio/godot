
#include "save-fl32.h"

#include <cstdio>

namespace msdfgen {

// Requires byte reversal for floats on big-endian platform
#ifndef __BIG_ENDIAN__

template <int N>
bool saveFl32(const BitmapConstRef<float, N> &bitmap, const char *filename) {
    if (FILE *f = fopen(filename, "wb")) {
        byte header[16] = { byte('F'), byte('L'), byte('3'), byte('2') };
        header[4] = byte(bitmap.height);
        header[5] = byte(bitmap.height>>8);
        header[6] = byte(bitmap.height>>16);
        header[7] = byte(bitmap.height>>24);
        header[8] = byte(bitmap.width);
        header[9] = byte(bitmap.width>>8);
        header[10] = byte(bitmap.width>>16);
        header[11] = byte(bitmap.width>>24);
        header[12] = byte(N);
        fwrite(header, 1, 16, f);
        fwrite(bitmap.pixels, sizeof(float), N*bitmap.width*bitmap.height, f);
        fclose(f);
        return true;
    }
    return false;
}

template bool saveFl32(const BitmapConstRef<float, 1> &bitmap, const char *filename);
template bool saveFl32(const BitmapConstRef<float, 2> &bitmap, const char *filename);
template bool saveFl32(const BitmapConstRef<float, 3> &bitmap, const char *filename);
template bool saveFl32(const BitmapConstRef<float, 4> &bitmap, const char *filename);

#endif

}


#define _CRT_SECURE_NO_WARNINGS

#include "save-bmp.h"

#include <cstdio>

#ifdef MSDFGEN_USE_CPP11
    #include <cstdint>
#else
namespace msdfgen {
    typedef int int32_t;
    typedef unsigned uint32_t;
    typedef unsigned short uint16_t;
    typedef unsigned char uint8_t;
}
#endif

#include "pixel-conversion.hpp"

namespace msdfgen {

template <typename T>
static bool writeValue(FILE *file, T value) {
    #ifdef __BIG_ENDIAN__
        T reverse = 0;
        for (int i = 0; i < sizeof(T); ++i) {
            reverse <<= 8;
            reverse |= value&T(0xff);
            value >>= 8;
        }
        return fwrite(&reverse, sizeof(T), 1, file) == 1;
    #else
        return fwrite(&value, sizeof(T), 1, file) == 1;
    #endif
}

static bool writeBmpHeader(FILE *file, int width, int height, int &paddedWidth) {
    paddedWidth = (3*width+3)&~3;
    const uint32_t bitmapStart = 54;
    const uint32_t bitmapSize = paddedWidth*height;
    const uint32_t fileSize = bitmapStart+bitmapSize;

    writeValue<uint16_t>(file, 0x4d42u);
    writeValue<uint32_t>(file, fileSize);
    writeValue<uint16_t>(file, 0);
    writeValue<uint16_t>(file, 0);
    writeValue<uint32_t>(file, bitmapStart);

    writeValue<uint32_t>(file, 40);
    writeValue<int32_t>(file, width);
    writeValue<int32_t>(file, height);
    writeValue<uint16_t>(file, 1);
    writeValue<uint16_t>(file, 24);
    writeValue<uint32_t>(file, 0);
    writeValue<uint32_t>(file, bitmapSize);
    writeValue<uint32_t>(file, 2835);
    writeValue<uint32_t>(file, 2835);
    writeValue<uint32_t>(file, 0);
    writeValue<uint32_t>(file, 0);

    return true;
}

bool saveBmp(const BitmapConstRef<byte, 1> &bitmap, const char *filename) {
    FILE *file = fopen(filename, "wb");
    if (!file)
        return false;

    int paddedWidth;
    writeBmpHeader(file, bitmap.width, bitmap.height, paddedWidth);

    const uint8_t padding[4] = { };
    int padLength = paddedWidth-3*bitmap.width;
    for (int y = 0; y < bitmap.height; ++y) {
        for (int x = 0; x < bitmap.width; ++x) {
            uint8_t px = (uint8_t) *bitmap(x, y);
            fwrite(&px, sizeof(uint8_t), 1, file);
            fwrite(&px, sizeof(uint8_t), 1, file);
            fwrite(&px, sizeof(uint8_t), 1, file);
        }
        fwrite(padding, 1, padLength, file);
    }

    return !fclose(file);
}

bool saveBmp(const BitmapConstRef<byte, 3> &bitmap, const char *filename) {
    FILE *file = fopen(filename, "wb");
    if (!file)
        return false;

    int paddedWidth;
    writeBmpHeader(file, bitmap.width, bitmap.height, paddedWidth);

    const uint8_t padding[4] = { };
    int padLength = paddedWidth-3*bitmap.width;
    for (int y = 0; y < bitmap.height; ++y) {
        for (int x = 0; x < bitmap.width; ++x) {
            uint8_t bgr[3] = {
                (uint8_t) bitmap(x, y)[2],
                (uint8_t) bitmap(x, y)[1],
                (uint8_t) bitmap(x, y)[0]
            };
            fwrite(bgr, sizeof(uint8_t), 3, file);
        }
        fwrite(padding, 1, padLength, file);
    }

    return !fclose(file);
}

bool saveBmp(const BitmapConstRef<byte, 4> &bitmap, const char *filename) {
    // RGBA not supported by the BMP format
    return false;
}

bool saveBmp(const BitmapConstRef<float, 1> &bitmap, const char *filename) {
    FILE *file = fopen(filename, "wb");
    if (!file)
        return false;

    int paddedWidth;
    writeBmpHeader(file, bitmap.width, bitmap.height, paddedWidth);

    const uint8_t padding[4] = { };
    int padLength = paddedWidth-3*bitmap.width;
    for (int y = 0; y < bitmap.height; ++y) {
        for (int x = 0; x < bitmap.width; ++x) {
            uint8_t px = (uint8_t) pixelFloatToByte(*bitmap(x, y));
            fwrite(&px, sizeof(uint8_t), 1, file);
            fwrite(&px, sizeof(uint8_t), 1, file);
            fwrite(&px, sizeof(uint8_t), 1, file);
        }
        fwrite(padding, 1, padLength, file);
    }

    return !fclose(file);
}

bool saveBmp(const BitmapConstRef<float, 3> &bitmap, const char *filename) {
    FILE *file = fopen(filename, "wb");
    if (!file)
        return false;

    int paddedWidth;
    writeBmpHeader(file, bitmap.width, bitmap.height, paddedWidth);

    const uint8_t padding[4] = { };
    int padLength = paddedWidth-3*bitmap.width;
    for (int y = 0; y < bitmap.height; ++y) {
        for (int x = 0; x < bitmap.width; ++x) {
            uint8_t bgr[3] = {
                (uint8_t) pixelFloatToByte(bitmap(x, y)[2]),
                (uint8_t) pixelFloatToByte(bitmap(x, y)[1]),
                (uint8_t) pixelFloatToByte(bitmap(x, y)[0])
            };
            fwrite(bgr, sizeof(uint8_t), 3, file);
        }
        fwrite(padding, 1, padLength, file);
    }

    return !fclose(file);
}

bool saveBmp(const BitmapConstRef<float, 4> &bitmap, const char *filename) {
    // RGBA not supported by the BMP format
    return false;
}

}

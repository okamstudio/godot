
#include "save-rgba.h"

#include <cstdio>
#include "pixel-conversion.hpp"

namespace msdfgen {

class RgbaFileOutput {
    FILE *file;

public:
    RgbaFileOutput(const char *filename, unsigned width, unsigned height) {
        if ((file = fopen(filename, "wb"))) {
            byte header[12] = { byte('R'), byte('G'), byte('B'), byte('A') };
            header[4] = byte(width>>24);
            header[5] = byte(width>>16);
            header[6] = byte(width>>8);
            header[7] = byte(width);
            header[8] = byte(height>>24);
            header[9] = byte(height>>16);
            header[10] = byte(height>>8);
            header[11] = byte(height);
            fwrite(header, 1, 12, file);
        }
    }

    ~RgbaFileOutput() {
        if (file)
            fclose(file);
    }

    void writePixel(const byte rgba[4]) {
        fwrite(rgba, 1, 4, file);
    }

    operator FILE *() {
        return file;
    }

};

bool saveRgba(const BitmapConstRef<byte, 1> &bitmap, const char *filename) {
    RgbaFileOutput output(filename, bitmap.width, bitmap.height);
    if (output) {
        byte rgba[4] = { byte(0), byte(0), byte(0), byte(0xff) };
        for (int y = bitmap.height; y--;) {
            for (const byte *p = bitmap(0, y), *end = p+bitmap.width; p < end; ++p) {
                rgba[0] = rgba[1] = rgba[2] = *p;
                output.writePixel(rgba);
            }
        }
        return true;
    }
    return false;
}

bool saveRgba(const BitmapConstRef<byte, 3> &bitmap, const char *filename) {
    RgbaFileOutput output(filename, bitmap.width, bitmap.height);
    if (output) {
        byte rgba[4] = { byte(0), byte(0), byte(0), byte(0xff) };
        for (int y = bitmap.height; y--;) {
            for (const byte *p = bitmap(0, y), *end = p+3*bitmap.width; p < end; p += 3) {
                rgba[0] = p[0], rgba[1] = p[1], rgba[2] = p[2];
                output.writePixel(rgba);
            }
        }
        return true;
    }
    return false;
}

bool saveRgba(const BitmapConstRef<byte, 4> &bitmap, const char *filename) {
    RgbaFileOutput output(filename, bitmap.width, bitmap.height);
    if (output) {
        for (int y = bitmap.height; y--;)
            fwrite(bitmap(0, y), 1, 4*bitmap.width, output);
        return true;
    }
    return false;
}

bool saveRgba(const BitmapConstRef<float, 1> &bitmap, const char *filename) {
    RgbaFileOutput output(filename, bitmap.width, bitmap.height);
    if (output) {
        byte rgba[4] = { byte(0), byte(0), byte(0), byte(0xff) };
        for (int y = bitmap.height; y--;) {
            for (const float *p = bitmap(0, y), *end = p+bitmap.width; p < end; ++p) {
                rgba[0] = rgba[1] = rgba[2] = pixelFloatToByte(*p);
                output.writePixel(rgba);
            }
        }
        return true;
    }
    return false;
}

bool saveRgba(const BitmapConstRef<float, 3> &bitmap, const char *filename) {
    RgbaFileOutput output(filename, bitmap.width, bitmap.height);
    if (output) {
        byte rgba[4] = { byte(0), byte(0), byte(0), byte(0xff) };
        for (int y = bitmap.height; y--;) {
            for (const float *p = bitmap(0, y), *end = p+3*bitmap.width; p < end; p += 3) {
                rgba[0] = pixelFloatToByte(p[0]);
                rgba[1] = pixelFloatToByte(p[1]);
                rgba[2] = pixelFloatToByte(p[2]);
                output.writePixel(rgba);
            }
        }
        return true;
    }
    return false;
}

bool saveRgba(const BitmapConstRef<float, 4> &bitmap, const char *filename) {
    RgbaFileOutput output(filename, bitmap.width, bitmap.height);
    if (output) {
        byte rgba[4];
        for (int y = bitmap.height; y--;) {
            for (const float *p = bitmap(0, y), *end = p+4*bitmap.width; p < end; p += 4) {
                rgba[0] = pixelFloatToByte(p[0]);
                rgba[1] = pixelFloatToByte(p[1]);
                rgba[2] = pixelFloatToByte(p[2]);
                rgba[3] = pixelFloatToByte(p[3]);
                output.writePixel(rgba);
            }
        }
        return true;
    }
    return false;
}

}

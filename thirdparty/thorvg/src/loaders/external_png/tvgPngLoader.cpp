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

#include "tvgPngLoader.h"

/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/

void PngLoader::clear()
{
    png_image_free(image);
    free(image);
    image = nullptr;
}


/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/

PngLoader::PngLoader() : ImageLoader(FileType::Png)
{
    image = static_cast<png_imagep>(calloc(1, sizeof(png_image)));
    image->version = PNG_IMAGE_VERSION;
    image->opaque = NULL;
}

PngLoader::~PngLoader()
{
    clear();
    free((void*)surface.buf32);
}


bool PngLoader::open(const string& path)
{
    image->opaque = NULL;

    if (!png_image_begin_read_from_file(image, path.c_str())) return false;

    w = (float)image->width;
    h = (float)image->height;

    return true;
}


bool PngLoader::open(const char* data, uint32_t size, bool copy)
{
#ifdef THORVG_FILE_IO_SUPPORT
    image->opaque = NULL;

    if (!png_image_begin_read_from_memory(image, data, size)) return false;

    w = (float)image->width;
    h = (float)image->height;

    return true;
#else
    return false;
#endif
}


bool PngLoader::read()
{
    if (!LoadModule::read()) return true;

    if (w == 0 || h == 0) return false;

    if (cs == ColorSpace::ARGB8888 || cs == ColorSpace::ARGB8888S) {
        image->format = PNG_FORMAT_BGRA;
        surface.cs = ColorSpace::ARGB8888S;
    } else {
        image->format = PNG_FORMAT_RGBA;
        surface.cs = ColorSpace::ABGR8888S;
    }

    auto buffer = static_cast<png_bytep>(malloc(PNG_IMAGE_SIZE((*image))));
    if (!png_image_finish_read(image, NULL, buffer, 0, NULL)) {
        free(buffer);
        return false;
    }

    //setup the surface
    surface.buf32 = reinterpret_cast<uint32_t*>(buffer);
    surface.stride = (uint32_t)w;
    surface.w = (uint32_t)w;
    surface.h = (uint32_t)h;
    surface.channelSize = sizeof(uint32_t);
    //TODO: we can acquire a pre-multiplied image. See "png_structrp"
    surface.premultiplied = false;

    clear();

    return true;
}

/**************************************************************************/
/*  png_driver_common.cpp                                                 */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#include "png_driver_common.h"

#include "core/config/engine.h"

#include <png.h>
#include <string.h>

namespace PNGDriverCommon {

// Print any warnings.
// On error, set explain and return true.
// Call should be wrapped in ERR_FAIL_COND
static bool check_error(const png_image &image) {
	const png_uint_32 failed = PNG_IMAGE_FAILED(image);
	if (failed & PNG_IMAGE_ERROR) {
		return true;
	} else if (failed) {
#ifdef TOOLS_ENABLED
		// suppress this warning, to avoid log spam when opening assetlib
		const static char *const noisy = "iCCP: known incorrect sRGB profile";
		const Engine *const eng = Engine::get_singleton();
		if (eng && eng->is_editor_hint() && !strcmp(image.message, noisy)) {
			return false;
		}
#endif
		WARN_PRINT(image.message);
	}
	return false;
}

Error png_to_image(const uint8_t *p_source, size_t p_size, bool p_force_linear, Ref<Image> p_image) {
	png_image png_img;
	memset(&png_img, 0, sizeof(png_img));
	png_img.version = PNG_IMAGE_VERSION;

	// fetch image properties
	int success = png_image_begin_read_from_memory(&png_img, p_source, p_size);
	ERR_FAIL_COND_V_MSG(check_error(png_img), ERR_FILE_CORRUPT, png_img.message);
	ERR_FAIL_COND_V(!success, ERR_FILE_CORRUPT);

	// flags to be masked out of input format to give target format
	const png_uint_32 format_mask = ~(
			// convert component order to RGBA
			PNG_FORMAT_FLAG_BGR | PNG_FORMAT_FLAG_AFIRST
			// convert 16 bit components to 8 bit
			| PNG_FORMAT_FLAG_LINEAR
			// convert indexed image to direct color
			| PNG_FORMAT_FLAG_COLORMAP);

	png_img.format &= format_mask;

	Image::Format dest_format;
	switch (png_img.format) {
		case PNG_FORMAT_GRAY:
			dest_format = Image::FORMAT_L8;
			break;
		case PNG_FORMAT_GA:
			dest_format = Image::FORMAT_LA8;
			break;
		case PNG_FORMAT_RGB:
			dest_format = Image::FORMAT_RGB8;
			break;
		case PNG_FORMAT_RGBA:
			dest_format = Image::FORMAT_RGBA8;
			break;
		default:
			png_image_free(&png_img); // only required when we return before finish_read
			ERR_PRINT("Unsupported png format.");
			return ERR_UNAVAILABLE;
	}

	if (!p_force_linear) {
		// assume 16 bit pngs without sRGB or gAMA chunks are in sRGB format
		png_img.flags |= PNG_IMAGE_FLAG_16BIT_sRGB;
	}

	const png_uint_32 stride = PNG_IMAGE_ROW_STRIDE(png_img);
	Vector<uint8_t> buffer;
	Error err = buffer.resize(PNG_IMAGE_BUFFER_SIZE(png_img, stride));
	if (err) {
		png_image_free(&png_img); // only required when we return before finish_read
		return err;
	}
	uint8_t *writer = buffer.ptrw();

	// read image data to buffer and release libpng resources
	success = png_image_finish_read(&png_img, nullptr, writer, stride, nullptr);
	ERR_FAIL_COND_V_MSG(check_error(png_img), ERR_FILE_CORRUPT, png_img.message);
	ERR_FAIL_COND_V(!success, ERR_FILE_CORRUPT);

	//print_line("png width: "+itos(png_img.width)+" height: "+itos(png_img.height));
	p_image->set_data(png_img.width, png_img.height, false, dest_format, buffer);

	return OK;
}

Error image_to_png(const Ref<Image> &p_image, Vector<uint8_t> &p_buffer) {
	Ref<Image> source_image = p_image->duplicate();

	if (source_image->is_compressed()) {
		source_image->decompress();
	}

	ERR_FAIL_COND_V(source_image->is_compressed(), FAILED);

	png_image png_img;
	memset(&png_img, 0, sizeof(png_img));
	png_img.version = PNG_IMAGE_VERSION;
	png_img.width = source_image->get_width();
	png_img.height = source_image->get_height();

	switch (source_image->get_format()) {
		case Image::FORMAT_L8:
			png_img.format = PNG_FORMAT_GRAY;
			break;
		case Image::FORMAT_LA8:
			png_img.format = PNG_FORMAT_GA;
			break;
		case Image::FORMAT_RGB8:
			png_img.format = PNG_FORMAT_RGB;
			break;
		case Image::FORMAT_RGBA8:
			png_img.format = PNG_FORMAT_RGBA;
			break;
		default:
			if (source_image->detect_alpha()) {
				source_image->convert(Image::FORMAT_RGBA8);
				png_img.format = PNG_FORMAT_RGBA;
			} else {
				source_image->convert(Image::FORMAT_RGB8);
				png_img.format = PNG_FORMAT_RGB;
			}
	}

	const Vector<uint8_t> image_data = source_image->get_data();
	const uint8_t *reader = image_data.ptr();

	// we may be passed a buffer with existing content we're expected to append to
	const int buffer_offset = p_buffer.size();

	const size_t png_size_estimate = PNG_IMAGE_PNG_SIZE_MAX(png_img);

	// try with estimated size
	size_t compressed_size = png_size_estimate;
	int success = 0;
	{ // scope writer lifetime
		Error err = p_buffer.resize(buffer_offset + png_size_estimate);
		ERR_FAIL_COND_V(err, err);

		uint8_t *writer = p_buffer.ptrw();
		success = png_image_write_to_memory(&png_img, &writer[buffer_offset],
				&compressed_size, 0, reader, 0, nullptr);
		ERR_FAIL_COND_V_MSG(check_error(png_img), FAILED, png_img.message);
	}
	if (!success) {
		// buffer was big enough, must be some other error
		ERR_FAIL_COND_V(compressed_size <= png_size_estimate, FAILED);

		// write failed due to buffer size, resize and retry
		Error err = p_buffer.resize(buffer_offset + compressed_size);
		ERR_FAIL_COND_V(err, err);

		uint8_t *writer = p_buffer.ptrw();
		success = png_image_write_to_memory(&png_img, &writer[buffer_offset],
				&compressed_size, 0, reader, 0, nullptr);
		ERR_FAIL_COND_V_MSG(check_error(png_img), FAILED, png_img.message);
		ERR_FAIL_COND_V(!success, FAILED);
	}

	// trim buffer size to content
	Error err = p_buffer.resize(buffer_offset + compressed_size);
	ERR_FAIL_COND_V(err, err);

	return OK;
}
} // namespace PNGDriverCommon

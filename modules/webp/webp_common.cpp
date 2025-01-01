/**************************************************************************/
/*  webp_common.cpp                                                       */
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

#include "webp_common.h"

#include "core/config/project_settings.h"

#include <webp/decode.h>
#include <webp/encode.h>

#include <string.h>

namespace WebPCommon {
Vector<uint8_t> _webp_lossy_pack(const Ref<Image> &p_image, float p_quality) {
	ERR_FAIL_COND_V(p_image.is_null() || p_image->is_empty(), Vector<uint8_t>());

	return _webp_packer(p_image, CLAMP(p_quality * 100.0f, 0.0f, 100.0f), false);
}

Vector<uint8_t> _webp_lossless_pack(const Ref<Image> &p_image) {
	ERR_FAIL_COND_V(p_image.is_null() || p_image->is_empty(), Vector<uint8_t>());

	float compression_factor = GLOBAL_GET("rendering/textures/webp_compression/lossless_compression_factor");
	compression_factor = CLAMP(compression_factor, 0.0f, 100.0f);

	return _webp_packer(p_image, compression_factor, true);
}

Vector<uint8_t> _webp_packer(const Ref<Image> &p_image, float p_quality, bool p_lossless) {
	int compression_method = GLOBAL_GET("rendering/textures/webp_compression/compression_method");
	compression_method = CLAMP(compression_method, 0, 6);

	Ref<Image> img = p_image->duplicate();
	if (img->is_compressed()) {
		Error error = img->decompress();
		ERR_FAIL_COND_V_MSG(error != OK, Vector<uint8_t>(), "Couldn't decompress image.");
	}
	if (img->detect_alpha()) {
		img->convert(Image::FORMAT_RGBA8);
	} else {
		img->convert(Image::FORMAT_RGB8);
	}

	Size2 s(img->get_width(), img->get_height());
	Vector<uint8_t> data = img->get_data();
	const uint8_t *r = data.ptr();

	// we need to use the more complex API in order to access specific flags...

	WebPConfig config;
	WebPPicture pic;
	if (!WebPConfigInit(&config) || !WebPPictureInit(&pic)) {
		ERR_FAIL_V(Vector<uint8_t>());
	}

	WebPMemoryWriter wrt;
	if (p_lossless) {
		config.lossless = 1;
		config.exact = 1;
	}
	config.method = compression_method;
	config.quality = p_quality;
	config.use_sharp_yuv = 1;
	pic.use_argb = 1;
	pic.width = s.width;
	pic.height = s.height;
	pic.writer = WebPMemoryWrite;
	pic.custom_ptr = &wrt;
	WebPMemoryWriterInit(&wrt);

	bool success_import = false;
	if (img->get_format() == Image::FORMAT_RGB8) {
		success_import = WebPPictureImportRGB(&pic, r, 3 * s.width);
	} else {
		success_import = WebPPictureImportRGBA(&pic, r, 4 * s.width);
	}
	bool success_encode = false;
	if (success_import) {
		success_encode = WebPEncode(&config, &pic);
	}
	WebPPictureFree(&pic);

	if (!success_encode) {
		WebPMemoryWriterClear(&wrt);
		ERR_FAIL_V_MSG(Vector<uint8_t>(), "WebP packing failed.");
	}

	// copy from wrt
	Vector<uint8_t> dst;
	dst.resize(wrt.size);
	uint8_t *w = dst.ptrw();
	memcpy(w, wrt.mem, wrt.size);
	WebPMemoryWriterClear(&wrt);
	return dst;
}

Ref<Image> _webp_unpack(const Vector<uint8_t> &p_buffer) {
	int size = p_buffer.size();
	ERR_FAIL_COND_V(size <= 0, Ref<Image>());
	const uint8_t *r = p_buffer.ptr();

	// A WebP file uses a RIFF header, which starts with "RIFF____WEBP".
	ERR_FAIL_COND_V(r[0] != 'R' || r[1] != 'I' || r[2] != 'F' || r[3] != 'F' || r[8] != 'W' || r[9] != 'E' || r[10] != 'B' || r[11] != 'P', Ref<Image>());
	WebPBitstreamFeatures features;
	if (WebPGetFeatures(r, size, &features) != VP8_STATUS_OK) {
		ERR_FAIL_V_MSG(Ref<Image>(), "Error unpacking WebP image.");
	}

	Vector<uint8_t> dst_image;
	int datasize = features.width * features.height * (features.has_alpha ? 4 : 3);
	dst_image.resize(datasize);

	uint8_t *dst_w = dst_image.ptrw();

	bool errdec = false;
	if (features.has_alpha) {
		errdec = WebPDecodeRGBAInto(r, size, dst_w, datasize, 4 * features.width) == nullptr;
	} else {
		errdec = WebPDecodeRGBInto(r, size, dst_w, datasize, 3 * features.width) == nullptr;
	}

	ERR_FAIL_COND_V_MSG(errdec, Ref<Image>(), "Failed decoding WebP image.");

	Ref<Image> img = memnew(Image(features.width, features.height, false, features.has_alpha ? Image::FORMAT_RGBA8 : Image::FORMAT_RGB8, dst_image));
	return img;
}

Error webp_load_image_from_buffer(Image *p_image, const uint8_t *p_buffer, int p_buffer_len) {
	ERR_FAIL_NULL_V(p_image, ERR_INVALID_PARAMETER);

	WebPBitstreamFeatures features;
	if (WebPGetFeatures(p_buffer, p_buffer_len, &features) != VP8_STATUS_OK) {
		ERR_FAIL_V(ERR_FILE_CORRUPT);
	}

	Vector<uint8_t> dst_image;
	int datasize = features.width * features.height * (features.has_alpha ? 4 : 3);
	dst_image.resize(datasize);
	uint8_t *dst_w = dst_image.ptrw();

	bool errdec = false;
	if (features.has_alpha) {
		errdec = WebPDecodeRGBAInto(p_buffer, p_buffer_len, dst_w, datasize, 4 * features.width) == nullptr;
	} else {
		errdec = WebPDecodeRGBInto(p_buffer, p_buffer_len, dst_w, datasize, 3 * features.width) == nullptr;
	}

	ERR_FAIL_COND_V_MSG(errdec, ERR_FILE_CORRUPT, "Failed decoding WebP image.");

	p_image->set_data(features.width, features.height, false, features.has_alpha ? Image::FORMAT_RGBA8 : Image::FORMAT_RGB8, dst_image);

	return OK;
}
} // namespace WebPCommon

/**************************************************************************/
/*  image_loader_png.cpp                                                  */
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

#include "image_loader_png.h"

#include "drivers/png/png_driver_common.h"

#include <string.h>

Error ImageLoaderPNG::load_image(Ref<Image> p_image, Ref<FileAccess> f, BitField<ImageFormatLoader::LoaderFlags> p_flags, float p_scale) {
	const uint64_t buffer_size = f->get_length();
	Vector<uint8_t> file_buffer;
	Error err = file_buffer.resize(buffer_size);
	if (err) {
		return err;
	}
	{
		uint8_t *writer = file_buffer.ptrw();
		f->get_buffer(writer, buffer_size);
	}
	const uint8_t *reader = file_buffer.ptr();
	return PNGDriverCommon::png_to_image(reader, buffer_size, p_flags & FLAG_FORCE_LINEAR, p_image);
}

void ImageLoaderPNG::get_recognized_extensions(List<String> *p_extensions) const {
	p_extensions->push_back("png");
}

Ref<Image> ImageLoaderPNG::load_mem_png(const uint8_t *p_png, int p_size) {
	Ref<Image> img;
	img.instantiate();

	// the value of p_force_linear does not matter since it only applies to 16 bit
	Error err = PNGDriverCommon::png_to_image(p_png, p_size, false, img);
	ERR_FAIL_COND_V(err, Ref<Image>());

	return img;
}

Ref<Image> ImageLoaderPNG::unpack_mem_png(const uint8_t *p_png, int p_size) {
	ERR_FAIL_COND_V(p_size < 4, Ref<Image>());
	ERR_FAIL_COND_V(p_png[0] != 'P' || p_png[1] != 'N' || p_png[2] != 'G' || p_png[3] != ' ', Ref<Image>());
	return load_mem_png(&p_png[4], p_size - 4);
}

Ref<Image> ImageLoaderPNG::lossless_unpack_png(const Vector<uint8_t> &p_data) {
	return unpack_mem_png(p_data.ptr(), p_data.size());
}

Vector<uint8_t> ImageLoaderPNG::lossless_pack_png(const Ref<Image> &p_image) {
	Vector<uint8_t> out_buffer;

	// add Godot's own "PNG " prefix
	if (out_buffer.resize(4) != OK) {
		ERR_FAIL_V(Vector<uint8_t>());
	}

	// scope for writer lifetime
	{
		// must be closed before call to image_to_png
		uint8_t *writer = out_buffer.ptrw();
		memcpy(writer, "PNG ", 4);
	}

	Error err = PNGDriverCommon::image_to_png(p_image, out_buffer);
	if (err) {
		ERR_FAIL_V(Vector<uint8_t>());
	}

	return out_buffer;
}

ImageLoaderPNG::ImageLoaderPNG() {
	Image::_png_mem_loader_func = load_mem_png;
	Image::_png_mem_unpacker_func = unpack_mem_png;
	Image::png_unpacker = lossless_unpack_png;
	Image::png_packer = lossless_pack_png;
}

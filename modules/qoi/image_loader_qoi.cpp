/**************************************************************************/
/*  image_loader_qoi.cpp                                                  */
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

#include "image_loader_qoi.h"

#include "core/io/file_access_memory.h"
#include "core/os/os.h"
#include "core/string/print_string.h"

#define QOI_NO_STDIO
#include "thirdparty/misc/qoi.h"

#include <string.h>

Error qoi_load_image_from_buffer(Image *p_image, const uint8_t *p_buffer, int p_buffer_len) {
	qoi_desc desc;
	void *pixels = qoi_decode(p_buffer, p_buffer_len, &desc, 0);

	if (!pixels) {
		return ERR_FILE_CORRUPT;
	}

	Vector<uint8_t> data;
	Image::Format fmt;
	if (desc.channels == 1) {
		fmt = Image::FORMAT_L8;
		data.resize(desc.height * desc.width);
	} else if (desc.channels == 3) {
		fmt = Image::FORMAT_RGB8;
		data.resize(desc.height * desc.width * 3);
	} else {
		fmt = Image::FORMAT_RGBA8;
		data.resize(desc.height * desc.width * 4);
	}

	memcpy(data.ptrw(), pixels, data.size());
	memfree(pixels);

	p_image->set_data(desc.width, desc.height, false, fmt, data);

	return OK;
}

Error ImageLoaderQOI::load_image(Ref<Image> p_image, Ref<FileAccess> f, BitField<ImageFormatLoader::LoaderFlags> p_flags, float p_scale) {
	Vector<uint8_t> src_image;
	uint64_t src_image_len = f->get_length();
	ERR_FAIL_COND_V(src_image_len == 0, ERR_FILE_CORRUPT);
	src_image.resize(src_image_len);

	uint8_t *w = src_image.ptrw();

	f->get_buffer(&w[0], src_image_len);

	Error err = qoi_load_image_from_buffer(p_image.ptr(), w, src_image_len);

	return err;
}

void ImageLoaderQOI::get_recognized_extensions(List<String> *p_extensions) const {
	p_extensions->push_back("qoi");
}

static Ref<Image> _qoi_mem_loader_func(const uint8_t *p_bmp, int p_size) {
	Ref<FileAccessMemory> memfile;
	memfile.instantiate();
	Error open_memfile_error = memfile->open_custom(p_bmp, p_size);
	ERR_FAIL_COND_V_MSG(open_memfile_error, Ref<Image>(), "Could not create memfile for QOI image buffer.");

	Ref<Image> img;
	img.instantiate();
	Error load_error = ImageLoaderQOI().load_image(img, memfile, false, 1.0f);
	ERR_FAIL_COND_V_MSG(load_error, Ref<Image>(), "Failed to load QOI image.");
	return img;
}

ImageLoaderQOI::ImageLoaderQOI() {
	Image::_qoi_mem_loader_func = _qoi_mem_loader_func;
}

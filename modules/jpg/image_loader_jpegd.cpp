/**************************************************************************/
/*  image_loader_jpegd.cpp                                                */
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

#include "image_loader_jpegd.h"

#include <jpgd.h>
#include <jpge.h>

#include <string.h>

Error jpeg_load_image_from_buffer(Image *p_image, const uint8_t *p_buffer, int p_buffer_len) {
	jpgd::jpeg_decoder_mem_stream mem_stream(p_buffer, p_buffer_len);

	jpgd::jpeg_decoder decoder(&mem_stream);

	if (decoder.get_error_code() != jpgd::JPGD_SUCCESS) {
		return ERR_CANT_OPEN;
	}

	const int image_width = decoder.get_width();
	const int image_height = decoder.get_height();
	const int comps = decoder.get_num_components();
	if (comps != 1 && comps != 3) {
		return ERR_FILE_CORRUPT;
	}

	if (decoder.begin_decoding() != jpgd::JPGD_SUCCESS) {
		return ERR_FILE_CORRUPT;
	}

	const int dst_bpl = image_width * comps;

	Vector<uint8_t> data;

	data.resize(dst_bpl * image_height);

	uint8_t *dw = data.ptrw();

	jpgd::uint8 *pImage_data = (jpgd::uint8 *)dw;

	for (int y = 0; y < image_height; y++) {
		const jpgd::uint8 *pScan_line;
		jpgd::uint scan_line_len;
		if (decoder.decode((const void **)&pScan_line, &scan_line_len) != jpgd::JPGD_SUCCESS) {
			return ERR_FILE_CORRUPT;
		}

		jpgd::uint8 *pDst = pImage_data + y * dst_bpl;

		if (comps == 1) {
			memcpy(pDst, pScan_line, dst_bpl);
		} else {
			// For images with more than 1 channel pScan_line will always point to a buffer
			// containing 32-bit RGBA pixels. Alpha is always 255 and we ignore it.
			for (int x = 0; x < image_width; x++) {
				pDst[0] = pScan_line[x * 4 + 0];
				pDst[1] = pScan_line[x * 4 + 1];
				pDst[2] = pScan_line[x * 4 + 2];
				pDst += 3;
			}
		}
	}

	//all good

	Image::Format fmt;
	if (comps == 1) {
		fmt = Image::FORMAT_L8;
	} else {
		fmt = Image::FORMAT_RGB8;
	}

	p_image->set_data(image_width, image_height, false, fmt, data);

	return OK;
}

Error ImageLoaderJPG::load_image(Ref<Image> p_image, Ref<FileAccess> f, BitField<ImageFormatLoader::LoaderFlags> p_flags, float p_scale) {
	Vector<uint8_t> src_image;
	uint64_t src_image_len = f->get_length();
	ERR_FAIL_COND_V(src_image_len == 0, ERR_FILE_CORRUPT);
	src_image.resize(src_image_len);

	uint8_t *w = src_image.ptrw();

	f->get_buffer(&w[0], src_image_len);

	Error err = jpeg_load_image_from_buffer(p_image.ptr(), w, src_image_len);

	return err;
}

void ImageLoaderJPG::get_recognized_extensions(List<String> *p_extensions) const {
	p_extensions->push_back("jpg");
	p_extensions->push_back("jpeg");
}

static Ref<Image> _jpegd_mem_loader_func(const uint8_t *p_png, int p_size) {
	Ref<Image> img;
	img.instantiate();
	Error err = jpeg_load_image_from_buffer(img.ptr(), p_png, p_size);
	ERR_FAIL_COND_V(err, Ref<Image>());
	return img;
}

class ImageLoaderJPGOSFile : public jpge::output_stream {
public:
	Ref<FileAccess> f;

	virtual bool put_buf(const void *Pbuf, int len) {
		f->store_buffer((const uint8_t *)Pbuf, len);
		return true;
	}
};

class ImageLoaderJPGOSBuffer : public jpge::output_stream {
public:
	Vector<uint8_t> *buffer = nullptr;
	virtual bool put_buf(const void *Pbuf, int len) {
		uint32_t base = buffer->size();
		buffer->resize(base + len);
		memcpy(buffer->ptrw() + base, Pbuf, len);
		return true;
	}
};

static Error _jpgd_save_to_output_stream(jpge::output_stream *p_output_stream, const Ref<Image> &p_img, float p_quality) {
	ERR_FAIL_COND_V(p_img.is_null() || p_img->is_empty(), ERR_INVALID_PARAMETER);
	Ref<Image> image = p_img->duplicate();
	if (image->is_compressed()) {
		Error error = image->decompress();
		ERR_FAIL_COND_V_MSG(error != OK, error, "Couldn't decompress image.");
	}
	if (image->get_format() != Image::FORMAT_RGB8) {
		image = image->duplicate();
		image->convert(Image::FORMAT_RGB8);
	}

	jpge::params p;
	p.m_quality = CLAMP(p_quality * 100, 1, 100);

	jpge::jpeg_encoder enc;
	enc.init(p_output_stream, image->get_width(), image->get_height(), 3, p);

	const uint8_t *src_data = image->get_data().ptr();
	for (int i = 0; i < image->get_height(); i++) {
		if (!enc.process_scanline(&src_data[i * image->get_width() * 3])) {
			return FAILED;
		}
	}

	if (enc.process_scanline(nullptr)) {
		return OK;
	} else {
		return FAILED;
	}
}

static Vector<uint8_t> _jpgd_buffer_save_func(const Ref<Image> &p_img, float p_quality) {
	Vector<uint8_t> output;
	ImageLoaderJPGOSBuffer ob;
	ob.buffer = &output;
	if (_jpgd_save_to_output_stream(&ob, p_img, p_quality) != OK) {
		return Vector<uint8_t>();
	}
	return output;
}

static Error _jpgd_save_func(const String &p_path, const Ref<Image> &p_img, float p_quality) {
	Error err;
	Ref<FileAccess> file = FileAccess::open(p_path, FileAccess::WRITE, &err);
	ERR_FAIL_COND_V_MSG(err, err, vformat("Can't save JPG at path: '%s'.", p_path));
	ImageLoaderJPGOSFile ob;
	ob.f = file;
	return _jpgd_save_to_output_stream(&ob, p_img, p_quality);
}

ImageLoaderJPG::ImageLoaderJPG() {
	Image::_jpg_mem_loader_func = _jpegd_mem_loader_func;
	Image::save_jpg_func = _jpgd_save_func;
	Image::save_jpg_buffer_func = _jpgd_buffer_save_func;
}

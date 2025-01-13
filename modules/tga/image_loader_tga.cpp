/**************************************************************************/
/*  image_loader_tga.cpp                                                  */
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

#include "image_loader_tga.h"

#include "core/error/error_macros.h"
#include "core/io/file_access_memory.h"
#include "core/io/image.h"

Error ImageLoaderTGA::decode_tga_rle(const uint8_t *p_compressed_buffer, size_t p_pixel_size, uint8_t *p_uncompressed_buffer, size_t p_output_size, size_t p_input_size) {
	Error error;

	Vector<uint8_t> pixels;
	error = pixels.resize(p_pixel_size);
	if (error != OK) {
		return error;
	}

	uint8_t *pixels_w = pixels.ptrw();

	size_t compressed_pos = 0;
	size_t output_pos = 0;
	size_t c = 0;
	size_t count = 0;

	while (output_pos < p_output_size) {
		c = p_compressed_buffer[compressed_pos];
		compressed_pos += 1;
		count = (c & 0x7f) + 1;

		if (output_pos + count * p_pixel_size > p_output_size) {
			return ERR_PARSE_ERROR;
		}

		if (c & 0x80) {
			if (compressed_pos + p_pixel_size > p_input_size) {
				return ERR_PARSE_ERROR;
			}
			for (size_t i = 0; i < p_pixel_size; i++) {
				pixels_w[i] = p_compressed_buffer[compressed_pos];
				compressed_pos += 1;
			}
			for (size_t i = 0; i < count; i++) {
				for (size_t j = 0; j < p_pixel_size; j++) {
					p_uncompressed_buffer[output_pos + j] = pixels_w[j];
				}
				output_pos += p_pixel_size;
			}
		} else {
			if (compressed_pos + count * p_pixel_size > p_input_size) {
				return ERR_PARSE_ERROR;
			}
			count *= p_pixel_size;
			for (size_t i = 0; i < count; i++) {
				p_uncompressed_buffer[output_pos] = p_compressed_buffer[compressed_pos];
				compressed_pos += 1;
				output_pos += 1;
			}
		}
	}
	return OK;
}

Error ImageLoaderTGA::convert_to_image(Ref<Image> p_image, const uint8_t *p_buffer, const tga_header_s &p_header, const uint8_t *p_palette, const bool p_is_monochrome, size_t p_input_size) {
#define TGA_PUT_PIXEL(r, g, b, a)             \
	int image_data_ofs = ((y * width) + x);   \
	image_data_w[image_data_ofs * 4 + 0] = r; \
	image_data_w[image_data_ofs * 4 + 1] = g; \
	image_data_w[image_data_ofs * 4 + 2] = b; \
	image_data_w[image_data_ofs * 4 + 3] = a;

	uint32_t width = p_header.image_width;
	uint32_t height = p_header.image_height;
	tga_origin_e origin = static_cast<tga_origin_e>((p_header.image_descriptor & TGA_ORIGIN_MASK) >> TGA_ORIGIN_SHIFT);
	uint8_t alpha_bits = p_header.image_descriptor & TGA_IMAGE_DESCRIPTOR_ALPHA_MASK;
	uint32_t x_start;
	int32_t x_step;
	uint32_t x_end;
	uint32_t y_start;
	int32_t y_step;
	uint32_t y_end;

	if (origin == TGA_ORIGIN_TOP_LEFT || origin == TGA_ORIGIN_TOP_RIGHT) {
		y_start = 0;
		y_step = 1;
		y_end = height;
	} else {
		y_start = height - 1;
		y_step = -1;
		y_end = -1;
	}

	if (origin == TGA_ORIGIN_TOP_LEFT || origin == TGA_ORIGIN_BOTTOM_LEFT) {
		x_start = 0;
		x_step = 1;
		x_end = width;
	} else {
		x_start = width - 1;
		x_step = -1;
		x_end = -1;
	}

	Vector<uint8_t> image_data;
	image_data.resize(width * height * sizeof(uint32_t));
	uint8_t *image_data_w = image_data.ptrw();

	size_t i = 0;
	uint32_t x = x_start;
	uint32_t y = y_start;

	if (p_header.pixel_depth == 8) {
		if (p_is_monochrome) {
			while (y != y_end) {
				while (x != x_end) {
					if (i >= p_input_size) {
						return ERR_PARSE_ERROR;
					}
					uint8_t shade = p_buffer[i];

					TGA_PUT_PIXEL(shade, shade, shade, 0xff)

					x += x_step;
					i += 1;
				}
				x = x_start;
				y += y_step;
			}
		} else {
			while (y != y_end) {
				while (x != x_end) {
					if (i >= p_input_size) {
						return ERR_PARSE_ERROR;
					}
					uint8_t index = p_buffer[i];
					uint8_t r = 0x00;
					uint8_t g = 0x00;
					uint8_t b = 0x00;
					uint8_t a = 0xff;

					if (p_header.color_map_depth == 24) {
						// Due to low-high byte order, the color table must be
						// read in the same order as image data (little endian)
						r = (p_palette[(index * 3) + 2]);
						g = (p_palette[(index * 3) + 1]);
						b = (p_palette[(index * 3) + 0]);
					} else {
						return ERR_INVALID_DATA;
					}

					TGA_PUT_PIXEL(r, g, b, a)

					x += x_step;
					i += 1;
				}
				x = x_start;
				y += y_step;
			}
		}
	} else if (p_header.pixel_depth == 16) {
		while (y != y_end) {
			while (x != x_end) {
				if (i + 1 >= p_input_size) {
					return ERR_PARSE_ERROR;
				}

				// Always stored as RGBA5551
				uint8_t r = (p_buffer[i + 1] & 0x7c) << 1;
				uint8_t g = ((p_buffer[i + 1] & 0x03) << 6) | ((p_buffer[i + 0] & 0xe0) >> 2);
				uint8_t b = (p_buffer[i + 0] & 0x1f) << 3;
				uint8_t a = (p_buffer[i + 1] & 0x80) ? 0xff : 0;

				TGA_PUT_PIXEL(r, g, b, alpha_bits ? a : 0xff);

				x += x_step;
				i += 2;
			}
			x = x_start;
			y += y_step;
		}
	} else if (p_header.pixel_depth == 24) {
		while (y != y_end) {
			while (x != x_end) {
				if (i + 2 >= p_input_size) {
					return ERR_PARSE_ERROR;
				}

				uint8_t r = p_buffer[i + 2];
				uint8_t g = p_buffer[i + 1];
				uint8_t b = p_buffer[i + 0];

				TGA_PUT_PIXEL(r, g, b, 0xff)

				x += x_step;
				i += 3;
			}
			x = x_start;
			y += y_step;
		}
	} else if (p_header.pixel_depth == 32) {
		while (y != y_end) {
			while (x != x_end) {
				if (i + 3 >= p_input_size) {
					return ERR_PARSE_ERROR;
				}

				uint8_t a = p_buffer[i + 3];
				uint8_t r = p_buffer[i + 2];
				uint8_t g = p_buffer[i + 1];
				uint8_t b = p_buffer[i + 0];

				TGA_PUT_PIXEL(r, g, b, a)

				x += x_step;
				i += 4;
			}
			x = x_start;
			y += y_step;
		}
	}

	p_image->initialize_data(width, height, false, Image::FORMAT_RGBA8, image_data);

	return OK;
}

Error ImageLoaderTGA::load_image(Ref<Image> p_image, Ref<FileAccess> f, BitField<ImageFormatLoader::LoaderFlags> p_flags, float p_scale) {
	Vector<uint8_t> src_image;
	uint64_t src_image_len = f->get_length();
	ERR_FAIL_COND_V(src_image_len == 0, ERR_FILE_CORRUPT);
	ERR_FAIL_COND_V(src_image_len < (int64_t)sizeof(tga_header_s), ERR_FILE_CORRUPT);
	src_image.resize(src_image_len);

	Error err = OK;

	tga_header_s tga_header;
	tga_header.id_length = f->get_8();
	tga_header.color_map_type = f->get_8();
	tga_header.image_type = static_cast<tga_type_e>(f->get_8());

	tga_header.first_color_entry = f->get_16();
	tga_header.color_map_length = f->get_16();
	tga_header.color_map_depth = f->get_8();

	tga_header.x_origin = f->get_16();
	tga_header.y_origin = f->get_16();
	tga_header.image_width = f->get_16();
	tga_header.image_height = f->get_16();
	tga_header.pixel_depth = f->get_8();
	tga_header.image_descriptor = f->get_8();

	bool is_encoded = (tga_header.image_type == TGA_TYPE_RLE_INDEXED || tga_header.image_type == TGA_TYPE_RLE_RGB || tga_header.image_type == TGA_TYPE_RLE_MONOCHROME);
	bool has_color_map = (tga_header.image_type == TGA_TYPE_RLE_INDEXED || tga_header.image_type == TGA_TYPE_INDEXED);
	bool is_monochrome = (tga_header.image_type == TGA_TYPE_RLE_MONOCHROME || tga_header.image_type == TGA_TYPE_MONOCHROME);

	if (tga_header.image_type == TGA_TYPE_NO_DATA) {
		err = FAILED;
	}

	uint64_t color_map_size;
	if (has_color_map) {
		if (tga_header.color_map_length > 256 || (tga_header.color_map_depth != 24) || tga_header.color_map_type != 1) {
			err = FAILED;
		}
		color_map_size = tga_header.color_map_length * (tga_header.color_map_depth >> 3);
	} else {
		if (tga_header.color_map_type) {
			err = FAILED;
		}
		color_map_size = 0;
	}

	if ((src_image_len - f->get_position()) < (tga_header.id_length + color_map_size)) {
		err = FAILED; // TGA data appears to be truncated (fewer bytes than expected).
	}

	if (tga_header.image_width <= 0 || tga_header.image_height <= 0) {
		err = FAILED;
	}

	if (!(tga_header.pixel_depth == 8 || tga_header.pixel_depth == 16 || tga_header.pixel_depth == 24 || tga_header.pixel_depth == 32)) {
		err = FAILED;
	}

	if (err == OK) {
		f->seek(f->get_position() + tga_header.id_length);

		Vector<uint8_t> palette;

		if (has_color_map) {
			err = palette.resize(color_map_size);
			if (err == OK) {
				uint8_t *palette_w = palette.ptrw();
				f->get_buffer(&palette_w[0], color_map_size);
			} else {
				return OK;
			}
		}

		uint8_t *src_image_w = src_image.ptrw();
		f->get_buffer(&src_image_w[0], src_image_len - f->get_position());

		const uint8_t *src_image_r = src_image.ptr();

		const size_t pixel_size = tga_header.pixel_depth >> 3;
		size_t buffer_size = (tga_header.image_width * tga_header.image_height) * pixel_size;

		Vector<uint8_t> uncompressed_buffer;
		uncompressed_buffer.resize(buffer_size);
		uint8_t *uncompressed_buffer_w = uncompressed_buffer.ptrw();
		const uint8_t *uncompressed_buffer_r;

		const uint8_t *buffer = nullptr;

		if (is_encoded) {
			err = decode_tga_rle(src_image_r, pixel_size, uncompressed_buffer_w, buffer_size, src_image_len);

			if (err == OK) {
				uncompressed_buffer_r = uncompressed_buffer.ptr();
				buffer = uncompressed_buffer_r;
			}
		} else {
			buffer = src_image_r;
			buffer_size = src_image_len;
		};

		if (err == OK) {
			const uint8_t *palette_r = palette.ptr();
			err = convert_to_image(p_image, buffer, tga_header, palette_r, is_monochrome, buffer_size);
		}
	}

	return err;
}

void ImageLoaderTGA::get_recognized_extensions(List<String> *p_extensions) const {
	p_extensions->push_back("tga");
}

static Ref<Image> _tga_mem_loader_func(const uint8_t *p_tga, int p_size) {
	Ref<FileAccessMemory> memfile;
	memfile.instantiate();
	Error open_memfile_error = memfile->open_custom(p_tga, p_size);
	ERR_FAIL_COND_V_MSG(open_memfile_error, Ref<Image>(), "Could not create memfile for TGA image buffer.");

	Ref<Image> img;
	img.instantiate();
	Error load_error = ImageLoaderTGA().load_image(img, memfile, false, 1.0f);
	ERR_FAIL_COND_V_MSG(load_error, Ref<Image>(), "Failed to load TGA image.");
	return img;
}

ImageLoaderTGA::ImageLoaderTGA() {
	Image::_tga_mem_loader_func = _tga_mem_loader_func;
}

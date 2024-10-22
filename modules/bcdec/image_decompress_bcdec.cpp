/**************************************************************************/
/*  image_decompress_bcdec.cpp                                            */
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

#include "image_decompress_bcdec.h"

#include "core/os/os.h"
#include "core/string/print_string.h"

#define BCDEC_IMPLEMENTATION
#include "thirdparty/misc/bcdec.h"

inline void bcdec_bc6h_half_s(const void *compressedBlock, void *decompressedBlock, int destinationPitch) {
	bcdec_bc6h_half(compressedBlock, decompressedBlock, destinationPitch, true);
}

inline void bcdec_bc6h_half_u(const void *compressedBlock, void *decompressedBlock, int destinationPitch) {
	bcdec_bc6h_half(compressedBlock, decompressedBlock, destinationPitch, false);
}

static void decompress_image(BCdecFormat format, const void *src, void *dst, const uint64_t width, const uint64_t height) {
	const uint8_t *src_blocks = reinterpret_cast<const uint8_t *>(src);
	uint8_t *dec_blocks = reinterpret_cast<uint8_t *>(dst);
	uint64_t src_pos = 0, dst_pos = 0;

#define DECOMPRESS_LOOP(func, block_size, color_bytesize, color_components)            \
	for (uint64_t y = 0; y < height; y += 4) {                                         \
		for (uint64_t x = 0; x < width; x += 4) {                                      \
			func(&src_blocks[src_pos], &dec_blocks[dst_pos], width *color_components); \
			src_pos += block_size;                                                     \
			dst_pos += 4 * color_bytesize;                                             \
		}                                                                              \
		dst_pos += 3 * width * color_bytesize;                                         \
	}

	switch (format) {
		case BCdec_BC1: {
			DECOMPRESS_LOOP(bcdec_bc1, BCDEC_BC1_BLOCK_SIZE, 4, 4)
		} break;
		case BCdec_BC2: {
			DECOMPRESS_LOOP(bcdec_bc2, BCDEC_BC2_BLOCK_SIZE, 4, 4)
		} break;
		case BCdec_BC3: {
			DECOMPRESS_LOOP(bcdec_bc3, BCDEC_BC3_BLOCK_SIZE, 4, 4)
		} break;
		case BCdec_BC4: {
			DECOMPRESS_LOOP(bcdec_bc4, BCDEC_BC4_BLOCK_SIZE, 1, 1)
		} break;
		case BCdec_BC5: {
			DECOMPRESS_LOOP(bcdec_bc5, BCDEC_BC5_BLOCK_SIZE, 2, 2)
		} break;
		case BCdec_BC6U: {
			DECOMPRESS_LOOP(bcdec_bc6h_half_u, BCDEC_BC6H_BLOCK_SIZE, 6, 3)
		} break;
		case BCdec_BC6S: {
			DECOMPRESS_LOOP(bcdec_bc6h_half_s, BCDEC_BC6H_BLOCK_SIZE, 6, 3)
		} break;
		case BCdec_BC7: {
			DECOMPRESS_LOOP(bcdec_bc7, BCDEC_BC7_BLOCK_SIZE, 4, 4)
		} break;
	}

#undef DECOMPRESS_LOOP
}

void image_decompress_bcdec(Image *p_image) {
	uint64_t start_time = OS::get_singleton()->get_ticks_msec();

	int width = p_image->get_width();
	int height = p_image->get_height();

	// Compressed images' dimensions should be padded to the upper multiple of 4.
	// If they aren't, they need to be realigned (the actual data is correctly padded though).
	if (width % 4 != 0 || height % 4 != 0) {
		int new_width = width + (4 - (width % 4));
		int new_height = height + (4 - (height % 4));

		print_verbose(vformat("Compressed image's dimensions are not multiples of 4 (%dx%d), aligning to (%dx%d)", width, height, new_width, new_height));

		width = new_width;
		height = new_height;
	}

	Image::Format source_format = p_image->get_format();
	Image::Format target_format = Image::FORMAT_MAX;

	BCdecFormat bcdec_format = BCdec_BC1;

	switch (source_format) {
		case Image::FORMAT_DXT1:
			bcdec_format = BCdec_BC1;
			target_format = Image::FORMAT_RGBA8;
			break;

		case Image::FORMAT_DXT3:
			bcdec_format = BCdec_BC2;
			target_format = Image::FORMAT_RGBA8;
			break;

		case Image::FORMAT_DXT5:
		case Image::FORMAT_DXT5_RA_AS_RG:
			bcdec_format = BCdec_BC3;
			target_format = Image::FORMAT_RGBA8;
			break;

		case Image::FORMAT_RGTC_R:
			bcdec_format = BCdec_BC4;
			target_format = Image::FORMAT_R8;
			break;

		case Image::FORMAT_RGTC_RG:
			bcdec_format = BCdec_BC5;
			target_format = Image::FORMAT_RG8;
			break;

		case Image::FORMAT_BPTC_RGBFU:
			bcdec_format = BCdec_BC6U;
			target_format = Image::FORMAT_RGBH;
			break;

		case Image::FORMAT_BPTC_RGBF:
			bcdec_format = BCdec_BC6S;
			target_format = Image::FORMAT_RGBH;
			break;

		case Image::FORMAT_BPTC_RGBA:
			bcdec_format = BCdec_BC7;
			target_format = Image::FORMAT_RGBA8;
			break;

		default:
			ERR_FAIL_MSG("bcdec: Can't decompress unknown format: " + Image::get_format_name(source_format) + ".");
			break;
	}

	int mm_count = p_image->get_mipmap_count();
	int64_t target_size = Image::get_image_data_size(width, height, target_format, p_image->has_mipmaps());

	// Decompressed data.
	Vector<uint8_t> data;
	data.resize(target_size);
	uint8_t *wb = data.ptrw();

	// Source data.
	const uint8_t *rb = p_image->get_data().ptr();

	// Decompress mipmaps.
	for (int i = 0; i <= mm_count; i++) {
		int mipmap_w = 0, mipmap_h = 0;
		int64_t src_ofs = Image::get_image_mipmap_offset_and_dimensions(width, height, source_format, i, mipmap_w, mipmap_h);
		int64_t dst_ofs = Image::get_image_mipmap_offset(width, height, target_format, i);
		decompress_image(bcdec_format, rb + src_ofs, wb + dst_ofs, mipmap_w, mipmap_h);
	}

	p_image->set_data(width, height, p_image->has_mipmaps(), target_format, data);

	// Swap channels if the format is using a channel swizzle.
	if (source_format == Image::FORMAT_DXT5_RA_AS_RG) {
		p_image->convert_ra_rgba8_to_rg();
	}

	print_verbose(vformat("bcdec: Decompression of a %dx%d %s image with %d mipmaps took %d ms.",
			p_image->get_width(), p_image->get_height(), Image::get_format_name(source_format), p_image->get_mipmap_count(), OS::get_singleton()->get_ticks_msec() - start_time));
}

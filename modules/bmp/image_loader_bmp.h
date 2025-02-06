/**************************************************************************/
/*  image_loader_bmp.h                                                    */
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

#pragma once

#include "core/io/image_loader.h"

class ImageLoaderBMP : public ImageFormatLoader {
protected:
	static const unsigned BITMAP_SIGNATURE = 0x4d42;

	static const unsigned BITMAP_FILE_HEADER_SIZE = 14; // bmp_file_header_s
	static const unsigned BITMAP_INFO_HEADER_MIN_SIZE = 40; // bmp_info_header_s

	enum bmp_compression_s {
		BI_RGB = 0x00,
		BI_RLE8 = 0x01, // compressed
		BI_RLE4 = 0x02, // compressed
		BI_BITFIELDS = 0x03,
		BI_JPEG = 0x04,
		BI_PNG = 0x05,
		BI_ALPHABITFIELDS = 0x06,
		BI_CMYK = 0x0b,
		BI_CMYKRLE8 = 0x0c, // compressed
		BI_CMYKRLE4 = 0x0d // compressed
	};

	struct bmp_header_s {
		struct bmp_file_header_s {
			uint16_t bmp_signature = 0;
			uint32_t bmp_file_size = 0;
			uint32_t bmp_file_padding = 0;
			uint32_t bmp_file_offset = 0;
		} bmp_file_header;

		struct bmp_info_header_s {
			uint32_t bmp_header_size = 0;
			uint32_t bmp_width = 0;
			uint32_t bmp_height = 0;
			uint16_t bmp_planes = 0;
			uint16_t bmp_bit_count = 0;
			uint32_t bmp_compression = 0;
			uint32_t bmp_size_image = 0;
			uint32_t bmp_pixels_per_meter_x = 0;
			uint32_t bmp_pixels_per_meter_y = 0;
			uint32_t bmp_colors_used = 0;
			uint32_t bmp_important_colors = 0;
		} bmp_info_header;

		struct bmp_bitfield_s {
			uint16_t alpha_mask = 0x8000;
			uint16_t red_mask = 0x7C00;
			uint16_t green_mask = 0x03E0;
			uint16_t blue_mask = 0x001F;
			uint16_t alpha_mask_width = 1u;
			uint16_t red_mask_width = 5u;
			uint16_t green_mask_width = 5u;
			uint16_t blue_mask_width = 5u;
			uint8_t alpha_offset = 15u; // Used for bit shifting.
			uint8_t red_offset = 10u; // Used for bit shifting.
			uint8_t green_offset = 5u; // Used for bit shifting.
			//uint8_t blue_offset = 0u; // Always LSB aligned no shifting needed.
			//uint8_t alpha_max = 1u; // Always boolean or on, so no scaling needed.
			uint8_t red_max = 32u; // Used for color space scaling.
			uint8_t green_max = 32u; // Used for color space scaling.
			uint8_t blue_max = 32u; // Used for color space scaling.
		} bmp_bitfield;
	};

	static Error convert_to_image(Ref<Image> p_image,
			const uint8_t *p_buffer,
			const uint8_t *p_color_buffer,
			const uint32_t color_table_size,
			const bmp_header_s &p_header);

public:
	virtual Error load_image(Ref<Image> p_image, Ref<FileAccess> f, BitField<ImageFormatLoader::LoaderFlags> p_flags, float p_scale);
	virtual void get_recognized_extensions(List<String> *p_extensions) const;
	ImageLoaderBMP();
};

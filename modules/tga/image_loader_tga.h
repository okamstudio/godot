/**************************************************************************/
/*  image_loader_tga.h                                                    */
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

#define TGA_IMAGE_DESCRIPTOR_ALPHA_MASK 0xf

class ImageLoaderTGA : public ImageFormatLoader {
	enum tga_type_e {
		TGA_TYPE_NO_DATA = 0,
		TGA_TYPE_INDEXED = 1,
		TGA_TYPE_RGB = 2,
		TGA_TYPE_MONOCHROME = 3,
		TGA_TYPE_RLE_INDEXED = 9,
		TGA_TYPE_RLE_RGB = 10,
		TGA_TYPE_RLE_MONOCHROME = 11
	};

	enum tga_origin_e {
		TGA_ORIGIN_BOTTOM_LEFT = 0x00,
		TGA_ORIGIN_BOTTOM_RIGHT = 0x01,
		TGA_ORIGIN_TOP_LEFT = 0x02,
		TGA_ORIGIN_TOP_RIGHT = 0x03,
		TGA_ORIGIN_SHIFT = 0x04,
		TGA_ORIGIN_MASK = 0x30
	};

	struct tga_header_s {
		uint8_t id_length = 0;
		uint8_t color_map_type = 0;
		tga_type_e image_type;

		uint16_t first_color_entry = 0;
		uint16_t color_map_length = 0;
		uint8_t color_map_depth = 0;

		uint16_t x_origin = 0;
		uint16_t y_origin = 0;
		uint16_t image_width = 0;
		uint16_t image_height = 0;
		uint8_t pixel_depth = 0;
		uint8_t image_descriptor = 0;
	};
	static Error decode_tga_rle(const uint8_t *p_compressed_buffer, size_t p_pixel_size, uint8_t *p_uncompressed_buffer, size_t p_output_size, size_t p_input_size);
	static Error convert_to_image(Ref<Image> p_image, const uint8_t *p_buffer, const tga_header_s &p_header, const uint8_t *p_palette, const bool p_is_monochrome, size_t p_input_size);

public:
	virtual Error load_image(Ref<Image> p_image, Ref<FileAccess> f, BitField<ImageFormatLoader::LoaderFlags> p_flags, float p_scale);
	virtual void get_recognized_extensions(List<String> *p_extensions) const;
	ImageLoaderTGA();
};

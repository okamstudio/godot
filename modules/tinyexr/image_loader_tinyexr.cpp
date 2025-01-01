/**************************************************************************/
/*  image_loader_tinyexr.cpp                                              */
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

#include "image_loader_tinyexr.h"

#include <zlib.h> // Should come before including tinyexr.

#include "thirdparty/tinyexr/tinyexr.h"

Error ImageLoaderTinyEXR::load_image(Ref<Image> p_image, Ref<FileAccess> f, BitField<ImageFormatLoader::LoaderFlags> p_flags, float p_scale) {
	Vector<uint8_t> src_image;
	uint64_t src_image_len = f->get_length();
	ERR_FAIL_COND_V(src_image_len == 0, ERR_FILE_CORRUPT);
	src_image.resize(src_image_len);

	uint8_t *w = src_image.ptrw();

	f->get_buffer(&w[0], src_image_len);

	// Re-implementation of tinyexr's LoadEXRFromMemory using Godot types to store the Image data
	// and Godot's error codes.
	// When debugging after updating the thirdparty library, check that we're still in sync with
	// their API usage in LoadEXRFromMemory.

	EXRVersion exr_version;
	EXRImage exr_image;
	EXRHeader exr_header;
	const char *err = nullptr;

	InitEXRHeader(&exr_header);

	int ret = ParseEXRVersionFromMemory(&exr_version, w, src_image_len);
	if (ret != TINYEXR_SUCCESS) {
		return ERR_FILE_CORRUPT;
	}

	ret = ParseEXRHeaderFromMemory(&exr_header, &exr_version, w, src_image_len, &err);
	if (ret != TINYEXR_SUCCESS) {
		if (err) {
			ERR_PRINT(String(err));
			FreeEXRErrorMessage(err);
		}
		return ERR_FILE_CORRUPT;
	}

	// Read HALF channel as FLOAT. (GH-13490)
	bool use_float16 = false;
	for (int i = 0; i < exr_header.num_channels; i++) {
		if (exr_header.pixel_types[i] == TINYEXR_PIXELTYPE_HALF) {
			use_float16 = true;
			exr_header.requested_pixel_types[i] = TINYEXR_PIXELTYPE_FLOAT;
		}
	}

	InitEXRImage(&exr_image);
	ret = LoadEXRImageFromMemory(&exr_image, &exr_header, w, src_image_len, &err);
	if (ret != TINYEXR_SUCCESS) {
		if (err) {
			ERR_PRINT(String(err));
			FreeEXRErrorMessage(err);
		}
		return ERR_FILE_CORRUPT;
	}

	// RGBA
	int idxR = -1;
	int idxG = -1;
	int idxB = -1;
	int idxA = -1;
	for (int c = 0; c < exr_header.num_channels; c++) {
		if (strcmp(exr_header.channels[c].name, "R") == 0) {
			idxR = c;
		} else if (strcmp(exr_header.channels[c].name, "G") == 0) {
			idxG = c;
		} else if (strcmp(exr_header.channels[c].name, "B") == 0) {
			idxB = c;
		} else if (strcmp(exr_header.channels[c].name, "A") == 0) {
			idxA = c;
		} else if (strcmp(exr_header.channels[c].name, "Y") == 0) {
			idxR = c;
			idxG = c;
			idxB = c;
		}
	}

	// EXR image data loaded, now parse it into Godot-friendly image data

	Vector<uint8_t> imgdata;
	Image::Format format;
	int output_channels = 0;

	int channel_size = use_float16 ? 2 : 4;
	if (idxA != -1) {
		imgdata.resize(exr_image.width * exr_image.height * 4 * channel_size); //RGBA
		format = use_float16 ? Image::FORMAT_RGBAH : Image::FORMAT_RGBAF;
		output_channels = 4;
	} else if (idxB != -1) {
		ERR_FAIL_COND_V(idxG == -1, ERR_FILE_CORRUPT);
		ERR_FAIL_COND_V(idxR == -1, ERR_FILE_CORRUPT);
		imgdata.resize(exr_image.width * exr_image.height * 3 * channel_size); //RGB
		format = use_float16 ? Image::FORMAT_RGBH : Image::FORMAT_RGBF;
		output_channels = 3;
	} else if (idxG != -1) {
		ERR_FAIL_COND_V(idxR == -1, ERR_FILE_CORRUPT);
		imgdata.resize(exr_image.width * exr_image.height * 2 * channel_size); //RG
		format = use_float16 ? Image::FORMAT_RGH : Image::FORMAT_RGF;
		output_channels = 2;
	} else {
		ERR_FAIL_COND_V(idxR == -1, ERR_FILE_CORRUPT);
		imgdata.resize(exr_image.width * exr_image.height * 1 * channel_size); //R
		format = use_float16 ? Image::FORMAT_RH : Image::FORMAT_RF;
		output_channels = 1;
	}

	EXRTile single_image_tile;
	int num_tiles;
	int tile_width = 0;
	int tile_height = 0;

	const EXRTile *exr_tiles;

	if (!exr_header.tiled) {
		single_image_tile.images = exr_image.images;
		single_image_tile.width = exr_image.width;
		single_image_tile.height = exr_image.height;
		single_image_tile.level_x = exr_image.width;
		single_image_tile.level_y = exr_image.height;
		single_image_tile.offset_x = 0;
		single_image_tile.offset_y = 0;

		exr_tiles = &single_image_tile;
		num_tiles = 1;
		tile_width = exr_image.width;
		tile_height = exr_image.height;
	} else {
		tile_width = exr_header.tile_size_x;
		tile_height = exr_header.tile_size_y;
		num_tiles = exr_image.num_tiles;
		exr_tiles = exr_image.tiles;
	}

	//print_line("reading format: " + Image::get_format_name(format));
	{
		uint8_t *wd = imgdata.ptrw();
		uint16_t *iw16 = (uint16_t *)wd;
		float *iw32 = (float *)wd;

		// Assume `out_rgba` have enough memory allocated.
		for (int tile_index = 0; tile_index < num_tiles; tile_index++) {
			const EXRTile &tile = exr_tiles[tile_index];

			int tw = tile.width;
			int th = tile.height;

			const float *r_channel_start = reinterpret_cast<const float *>(tile.images[idxR]);
			const float *g_channel_start = nullptr;
			const float *b_channel_start = nullptr;
			const float *a_channel_start = nullptr;

			if (idxG != -1) {
				g_channel_start = reinterpret_cast<const float *>(tile.images[idxG]);
			}
			if (idxB != -1) {
				b_channel_start = reinterpret_cast<const float *>(tile.images[idxB]);
			}
			if (idxA != -1) {
				a_channel_start = reinterpret_cast<const float *>(tile.images[idxA]);
			}

			uint16_t *first_row_w16 = iw16 + (tile.offset_y * tile_height * exr_image.width + tile.offset_x * tile_width) * output_channels;
			float *first_row_w32 = iw32 + (tile.offset_y * tile_height * exr_image.width + tile.offset_x * tile_width) * output_channels;

			for (int y = 0; y < th; y++) {
				const float *r_channel = r_channel_start + y * tile_width;
				const float *g_channel = nullptr;
				const float *b_channel = nullptr;
				const float *a_channel = nullptr;
				if (g_channel_start) {
					g_channel = g_channel_start + y * tile_width;
				}
				if (b_channel_start) {
					b_channel = b_channel_start + y * tile_width;
				}
				if (a_channel_start) {
					a_channel = a_channel_start + y * tile_width;
				}

				if (use_float16) {
					uint16_t *row_w = first_row_w16 + (y * exr_image.width * output_channels);

					for (int x = 0; x < tw; x++) {
						Color color;
						color.r = *r_channel++;
						if (g_channel) {
							color.g = *g_channel++;
						}
						if (b_channel) {
							color.b = *b_channel++;
						}
						if (a_channel) {
							color.a = *a_channel++;
						}

						if (p_flags & FLAG_FORCE_LINEAR) {
							color = color.srgb_to_linear();
						}

						*row_w++ = Math::make_half_float(color.r);
						if (g_channel) {
							*row_w++ = Math::make_half_float(color.g);
						}
						if (b_channel) {
							*row_w++ = Math::make_half_float(color.b);
						}
						if (a_channel) {
							*row_w++ = Math::make_half_float(color.a);
						}
					}
				} else {
					float *row_w = first_row_w32 + (y * exr_image.width * output_channels);

					for (int x = 0; x < tw; x++) {
						Color color;
						color.r = *r_channel++;
						if (g_channel) {
							color.g = *g_channel++;
						}
						if (b_channel) {
							color.b = *b_channel++;
						}
						if (a_channel) {
							color.a = *a_channel++;
						}

						if (p_flags & FLAG_FORCE_LINEAR) {
							color = color.srgb_to_linear();
						}

						*row_w++ = color.r;
						if (g_channel) {
							*row_w++ = color.g;
						}
						if (b_channel) {
							*row_w++ = color.b;
						}
						if (a_channel) {
							*row_w++ = color.a;
						}
					}
				}
			}
		}
	}

	p_image->set_data(exr_image.width, exr_image.height, false, format, imgdata);

	FreeEXRHeader(&exr_header);
	FreeEXRImage(&exr_image);

	return OK;
}

void ImageLoaderTinyEXR::get_recognized_extensions(List<String> *p_extensions) const {
	p_extensions->push_back("exr");
}

ImageLoaderTinyEXR::ImageLoaderTinyEXR() {
}

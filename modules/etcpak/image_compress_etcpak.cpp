/**************************************************************************/
/*  image_compress_etcpak.cpp                                             */
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

#include "image_compress_etcpak.h"

#include "core/os/os.h"
#include "core/string/print_string.h"

#include <ProcessDxtc.hpp>
#include <ProcessRGB.hpp>

EtcpakType _determine_etc_type(Image::UsedChannels p_channels) {
	switch (p_channels) {
		case Image::USED_CHANNELS_L:
			return EtcpakType::ETCPAK_TYPE_ETC2;
		case Image::USED_CHANNELS_LA:
			return EtcpakType::ETCPAK_TYPE_ETC2_ALPHA;
		case Image::USED_CHANNELS_R:
			return EtcpakType::ETCPAK_TYPE_ETC2_R;
		case Image::USED_CHANNELS_RG:
			return EtcpakType::ETCPAK_TYPE_ETC2_RG;
		case Image::USED_CHANNELS_RGB:
			return EtcpakType::ETCPAK_TYPE_ETC2;
		case Image::USED_CHANNELS_RGBA:
			return EtcpakType::ETCPAK_TYPE_ETC2_ALPHA;

		default:
			return EtcpakType::ETCPAK_TYPE_ETC2_ALPHA;
	}
}

EtcpakType _determine_dxt_type(Image::UsedChannels p_channels) {
	switch (p_channels) {
		case Image::USED_CHANNELS_L:
			return EtcpakType::ETCPAK_TYPE_DXT1;
		case Image::USED_CHANNELS_LA:
			return EtcpakType::ETCPAK_TYPE_DXT5;
		case Image::USED_CHANNELS_R:
			return EtcpakType::ETCPAK_TYPE_RGTC_R;
		case Image::USED_CHANNELS_RG:
			return EtcpakType::ETCPAK_TYPE_RGTC_RG;
		case Image::USED_CHANNELS_RGB:
			return EtcpakType::ETCPAK_TYPE_DXT1;
		case Image::USED_CHANNELS_RGBA:
			return EtcpakType::ETCPAK_TYPE_DXT5;

		default:
			return EtcpakType::ETCPAK_TYPE_DXT5;
	}
}

void _compress_etc1(Image *r_img) {
	_compress_etcpak(EtcpakType::ETCPAK_TYPE_ETC1, r_img);
}

void _compress_etc2(Image *r_img, Image::UsedChannels p_channels) {
	_compress_etcpak(_determine_etc_type(p_channels), r_img);
}

void _compress_bc(Image *r_img, Image::UsedChannels p_channels) {
	_compress_etcpak(_determine_dxt_type(p_channels), r_img);
}

void _compress_etcpak(EtcpakType p_compress_type, Image *r_img) {
	uint64_t start_time = OS::get_singleton()->get_ticks_msec();

	// The image is already compressed, return.
	if (r_img->is_compressed()) {
		return;
	}

	// Convert to RGBA8 for compression.
	r_img->convert(Image::FORMAT_RGBA8);

	// Determine output format based on Etcpak type.
	Image::Format target_format = Image::FORMAT_RGBA8;

	switch (p_compress_type) {
		case EtcpakType::ETCPAK_TYPE_ETC1:
			target_format = Image::FORMAT_ETC;
			break;

		case EtcpakType::ETCPAK_TYPE_ETC2:
			target_format = Image::FORMAT_ETC2_RGB8;
			break;

		case EtcpakType::ETCPAK_TYPE_ETC2_ALPHA:
			target_format = Image::FORMAT_ETC2_RGBA8;
			break;

		case EtcpakType::ETCPAK_TYPE_ETC2_R:
			target_format = Image::FORMAT_ETC2_R11;
			break;

		case EtcpakType::ETCPAK_TYPE_ETC2_RG:
			target_format = Image::FORMAT_ETC2_RG11;
			break;

		case EtcpakType::ETCPAK_TYPE_ETC2_RA_AS_RG:
			target_format = Image::FORMAT_ETC2_RA_AS_RG;
			r_img->convert_rg_to_ra_rgba8();
			break;

		case EtcpakType::ETCPAK_TYPE_DXT1:
			target_format = Image::FORMAT_DXT1;
			break;

		case EtcpakType::ETCPAK_TYPE_DXT5:
			target_format = Image::FORMAT_DXT5;
			break;

		case EtcpakType::ETCPAK_TYPE_RGTC_R:
			target_format = Image::FORMAT_RGTC_R;
			break;

		case EtcpakType::ETCPAK_TYPE_RGTC_RG:
			target_format = Image::FORMAT_RGTC_RG;
			break;

		case EtcpakType::ETCPAK_TYPE_DXT5_RA_AS_RG:
			target_format = Image::FORMAT_DXT5_RA_AS_RG;
			r_img->convert_rg_to_ra_rgba8();
			break;

		default:
			ERR_FAIL_MSG("Invalid or unsupported etcpak compression format, not ETC or DXT.");
			break;
	}

	// It's badly documented but ETCPAK seems to expect BGRA8 for ETC formats.
	if (p_compress_type < EtcpakType::ETCPAK_TYPE_DXT1) {
		r_img->convert_rgba8_to_bgra8();
	}

	// Compress image data and (if required) mipmaps.
	const bool has_mipmaps = r_img->has_mipmaps();
	int width = r_img->get_width();
	int height = r_img->get_height();

	/*
	The first mipmap level of a compressed texture must be a multiple of 4. Quote from D3D11.3 spec:

	BC format surfaces are always multiples of full blocks, each block representing 4x4 pixels.
	For mipmaps, the top level map is required to be a multiple of 4 size in all dimensions.
	The sizes for the lower level maps are computed as they are for all mipmapped surfaces,
	and thus may not be a multiple of 4, for example a top level map of 20 results in a second level
	map size of 10. For these cases, there is a differing 'physical' size and a 'virtual' size.
	The virtual size is that computed for each mip level without adjustment, which is 10 for the example.
	The physical size is the virtual size rounded up to the next multiple of 4, which is 12 for the example,
	and this represents the actual memory size. The sampling hardware will apply texture address
	processing based on the virtual size (using, for example, border color if specified for accesses
	beyond 10), and thus for the example case will not access the 11th and 12th row of the resource.
	So for mipmap chains when an axis becomes < 4 in size, only texels 'a','b','e','f'
	are used for a 2x2 map, and texel 'a' is used for 1x1. Note that this is similar to, but distinct from,
	the surface pitch, which can encompass additional padding beyond the physical surface size.
	*/

	if (width % 4 != 0 || height % 4 != 0) {
		width = width <= 2 ? width : (width + 3) & ~3;
		height = height <= 2 ? height : (height + 3) & ~3;
	}

	// Multiple-of-4 should be guaranteed by above.
	// However, power-of-two 3d textures will create Nx2 and Nx1 mipmap levels,
	// which are individually compressed Image objects that violate the above rule.
	// Hence, we allow Nx1 and Nx2 images through without forcing to multiple-of-4.

	// Create the buffer for compressed image data.
	Vector<uint8_t> dest_data;
	dest_data.resize(Image::get_image_data_size(width, height, target_format, has_mipmaps));
	uint8_t *dest_write = dest_data.ptrw();

	const uint8_t *src_read = r_img->get_data().ptr();

	const int mip_count = has_mipmaps ? Image::get_image_required_mipmaps(width, height, target_format) : 0;
	Vector<uint32_t> padded_src;

	for (int i = 0; i < mip_count + 1; i++) {
		// Get write mip metrics for target image.
		int dest_mip_w, dest_mip_h;
		int64_t dest_mip_ofs = Image::get_image_mipmap_offset_and_dimensions(width, height, target_format, i, dest_mip_w, dest_mip_h);

		// Ensure that mip offset is a multiple of 8 (etcpak expects uint64_t pointer).
		ERR_FAIL_COND(dest_mip_ofs % 8 != 0);
		uint64_t *dest_mip_write = reinterpret_cast<uint64_t *>(dest_write + dest_mip_ofs);

		// Block size.
		dest_mip_w = (dest_mip_w + 3) & ~3;
		dest_mip_h = (dest_mip_h + 3) & ~3;
		const uint32_t blocks = dest_mip_w * dest_mip_h / 16;

		// Get mip data from source image for reading.
		int64_t src_mip_ofs, src_mip_size;
		int src_mip_w, src_mip_h;

		r_img->get_mipmap_offset_size_and_dimensions(i, src_mip_ofs, src_mip_size, src_mip_w, src_mip_h);

		const uint32_t *src_mip_read = reinterpret_cast<const uint32_t *>(src_read + src_mip_ofs);

		// Pad textures to nearest block by smearing.
		if (dest_mip_w != src_mip_w || dest_mip_h != src_mip_h) {
			// Reserve the buffer for padded image data.
			padded_src.resize(dest_mip_w * dest_mip_h);
			uint32_t *ptrw = padded_src.ptrw();

			int x = 0, y = 0;
			for (y = 0; y < src_mip_h; y++) {
				for (x = 0; x < src_mip_w; x++) {
					ptrw[dest_mip_w * y + x] = src_mip_read[src_mip_w * y + x];
				}

				// First, smear in x.
				for (; x < dest_mip_w; x++) {
					ptrw[dest_mip_w * y + x] = ptrw[dest_mip_w * y + x - 1];
				}
			}

			// Then, smear in y.
			for (; y < dest_mip_h; y++) {
				for (x = 0; x < dest_mip_w; x++) {
					ptrw[dest_mip_w * y + x] = ptrw[dest_mip_w * y + x - dest_mip_w];
				}
			}

			// Override the src_mip_read pointer to our temporary Vector.
			src_mip_read = padded_src.ptr();
		}

		switch (p_compress_type) {
			case EtcpakType::ETCPAK_TYPE_ETC1:
				CompressEtc1RgbDither(src_mip_read, dest_mip_write, blocks, dest_mip_w);
				break;

			case EtcpakType::ETCPAK_TYPE_ETC2:
				CompressEtc2Rgb(src_mip_read, dest_mip_write, blocks, dest_mip_w, true);
				break;

			case EtcpakType::ETCPAK_TYPE_ETC2_ALPHA:
			case EtcpakType::ETCPAK_TYPE_ETC2_RA_AS_RG:
				CompressEtc2Rgba(src_mip_read, dest_mip_write, blocks, dest_mip_w, true);
				break;

			case EtcpakType::ETCPAK_TYPE_ETC2_R:
				CompressEacR(src_mip_read, dest_mip_write, blocks, dest_mip_w);
				break;

			case EtcpakType::ETCPAK_TYPE_ETC2_RG:
				CompressEacRg(src_mip_read, dest_mip_write, blocks, dest_mip_w);
				break;

			case EtcpakType::ETCPAK_TYPE_DXT1:
				CompressBc1Dither(src_mip_read, dest_mip_write, blocks, dest_mip_w);
				break;

			case EtcpakType::ETCPAK_TYPE_DXT5:
			case EtcpakType::ETCPAK_TYPE_DXT5_RA_AS_RG:
				CompressBc3(src_mip_read, dest_mip_write, blocks, dest_mip_w);
				break;

			case EtcpakType::ETCPAK_TYPE_RGTC_R:
				CompressBc4(src_mip_read, dest_mip_write, blocks, dest_mip_w);
				break;

			case EtcpakType::ETCPAK_TYPE_RGTC_RG:
				CompressBc5(src_mip_read, dest_mip_write, blocks, dest_mip_w);
				break;

			default:
				ERR_FAIL_MSG("etcpak: Invalid or unsupported compression format.");
				break;
		}
	}

	// Replace original image with compressed one.
	r_img->set_data(width, height, has_mipmaps, target_format, dest_data);

	print_verbose(vformat("etcpak: Encoding took %d ms.", OS::get_singleton()->get_ticks_msec() - start_time));
}

/**************************************************************************/
/*  image_saver_dds.cpp                                                   */
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

#include "image_saver_dds.h"

#include "core/io/file_access.h"
#include "core/io/stream_peer.h"
#include "dds.h"

Error save_dds(const String &p_path, const Ref<Image> &p_img) {
	Vector<uint8_t> buffer = save_dds_buffer(p_img);

	Ref<FileAccess> file = FileAccess::open(p_path, FileAccess::WRITE);
	if (file.is_null()) {
		return ERR_CANT_CREATE;
	}

	file->store_buffer(buffer.ptr(), buffer.size());

	return OK;
}

enum DDSFormatType {
	DDFT_BITMASK,
	DDFT_FOURCC,
	DDFT_DXGI,
};

DDSFormatType _dds_format_get_type(DDSFormat p_format) {
	switch (p_format) {
		case DDS_DXT1:
		case DDS_DXT3:
		case DDS_DXT5:
		case DDS_ATI1:
		case DDS_ATI2:
		case DDS_R16F:
		case DDS_RG16F:
		case DDS_RGBA16F:
		case DDS_R32F:
		case DDS_RG32F:
		case DDS_RGBA32F:
			return DDFT_FOURCC;

		case DDS_BC6S:
		case DDS_BC6U:
		case DDS_BC7:
		case DDS_RGB9E5:
		case DDS_RGB32F:
			return DDFT_DXGI;

		default:
			return DDFT_BITMASK;
	}
}

DDSFormat _image_format_to_dds_format(Image::Format p_image_format) {
	switch (p_image_format) {
		case Image::FORMAT_RGBAF: {
			return DDS_RGBA32F;
		}
		case Image::FORMAT_RGBF: {
			return DDS_RGB32F;
		}
		case Image::FORMAT_RGBAH: {
			return DDS_RGBA16F;
		}
		case Image::FORMAT_RGF: {
			return DDS_RG32F;
		}
		case Image::FORMAT_RGBA8: {
			return DDS_RGBA8;
		}
		case Image::FORMAT_RGH: {
			return DDS_RG16F;
		}
		case Image::FORMAT_RF: {
			return DDS_R32F;
		}
		case Image::FORMAT_L8:
		case Image::FORMAT_R8: {
			return DDS_LUMINANCE;
		}
		case Image::FORMAT_RH: {
			return DDS_R16F;
		}
		case Image::FORMAT_LA8:
		case Image::FORMAT_RG8: {
			return DDS_LUMINANCE_ALPHA;
		}
		case Image::FORMAT_RGBE9995: {
			return DDS_RGB9E5;
		}
		case Image::FORMAT_DXT1: {
			return DDS_DXT1;
		}
		case Image::FORMAT_DXT3: {
			return DDS_DXT3;
		}
		case Image::FORMAT_DXT5: {
			return DDS_DXT5;
		}
		case Image::FORMAT_RGTC_R: {
			return DDS_ATI1;
		}
		case Image::FORMAT_RGTC_RG: {
			return DDS_ATI2;
		}
		case Image::FORMAT_RGB8: {
			return DDS_RGB8;
		}
		case Image::FORMAT_BPTC_RGBFU: {
			return DDS_BC6U;
		}
		case Image::FORMAT_BPTC_RGBF: {
			return DDS_BC6S;
		}
		case Image::FORMAT_BPTC_RGBA: {
			return DDS_BC7;
		}
		default: {
			return DDS_MAX;
		}
	}
}

uint32_t _image_format_to_fourcc_format(Image::Format p_format) {
	switch (p_format) {
		case Image::FORMAT_DXT1:
			return DDFCC_DXT1;
		case Image::FORMAT_DXT3:
			return DDFCC_DXT3;
		case Image::FORMAT_DXT5:
			return DDFCC_DXT5;
		case Image::FORMAT_RGTC_R:
			return DDFCC_ATI1;
		case Image::FORMAT_RGTC_RG:
			return DDFCC_ATI2;
		case Image::FORMAT_RF:
			return DDFCC_R32F;
		case Image::FORMAT_RGF:
			return DDFCC_RG32F;
		case Image::FORMAT_RGBAF:
			return DDFCC_RGBA32F;
		case Image::FORMAT_RH:
			return DDFCC_R16F;
		case Image::FORMAT_RGH:
			return DDFCC_RG16F;
		case Image::FORMAT_RGBAH:
			return DDFCC_RGBA16F;

		default:
			return 0;
	}
}

uint32_t _image_format_to_dxgi_format(Image::Format p_format) {
	switch (p_format) {
		case Image::FORMAT_DXT1:
			return DXGI_BC1_UNORM;
		case Image::FORMAT_DXT3:
			return DXGI_BC2_UNORM;
		case Image::FORMAT_DXT5:
			return DXGI_BC3_UNORM;
		case Image::FORMAT_RGTC_R:
			return DXGI_BC4_UNORM;
		case Image::FORMAT_RGTC_RG:
			return DXGI_BC5_UNORM;
		case Image::FORMAT_BPTC_RGBFU:
			return DXGI_BC6H_UF16;
		case Image::FORMAT_BPTC_RGBF:
			return DXGI_BC6H_SF16;
		case Image::FORMAT_BPTC_RGBA:
			return DXGI_BC7_UNORM;
		case Image::FORMAT_RF:
			return DXGI_R32_FLOAT;
		case Image::FORMAT_RGF:
			return DXGI_R32G32_FLOAT;
		case Image::FORMAT_RGBF:
			return DXGI_R32G32B32_FLOAT;
		case Image::FORMAT_RGBAF:
			return DXGI_R32G32B32A32_FLOAT;
		case Image::FORMAT_RH:
			return DXGI_R16_FLOAT;
		case Image::FORMAT_RGH:
			return DXGI_R16G16_FLOAT;
		case Image::FORMAT_RGBAH:
			return DXGI_R16G16B16A16_FLOAT;
		case Image::FORMAT_RGBE9995:
			return DXGI_R9G9B9E5;

		default:
			return 0;
	}
}

void _get_dds_pixel_bitmask(Image::Format p_format, uint32_t &r_bit_count, uint32_t &r_red_mask, uint32_t &r_green_mask, uint32_t &r_blue_mask, uint32_t &r_alpha_mask) {
	switch (p_format) {
		case Image::FORMAT_R8:
		case Image::FORMAT_L8: {
			r_bit_count = 8;
			r_red_mask = 0xff;
			r_green_mask = 0;
			r_blue_mask = 0;
			r_alpha_mask = 0;
		} break;
		case Image::FORMAT_RG8:
		case Image::FORMAT_LA8: {
			r_bit_count = 16;
			r_red_mask = 0xff;
			r_green_mask = 0;
			r_blue_mask = 0;
			r_alpha_mask = 0xff00;
		} break;
		case Image::FORMAT_RGB8: {
			// BGR8
			r_bit_count = 24;
			r_red_mask = 0xff0000;
			r_green_mask = 0xff00;
			r_blue_mask = 0xff;
			r_alpha_mask = 0;
		} break;
		case Image::FORMAT_RGBA8: {
			r_bit_count = 32;
			r_red_mask = 0xff;
			r_green_mask = 0xff00;
			r_blue_mask = 0xff0000;
			r_alpha_mask = 0xff000000;
		} break;
		case Image::FORMAT_RGBA4444: {
			// BGRA4444
			r_bit_count = 16;
			r_red_mask = 0xf00;
			r_green_mask = 0xf0;
			r_blue_mask = 0xf;
			r_alpha_mask = 0xf000;
		} break;
		case Image::FORMAT_RGB565: {
			// BGR565
			r_bit_count = 16;
			r_red_mask = 0xf800;
			r_green_mask = 0x7e0;
			r_blue_mask = 0x1f;
			r_alpha_mask = 0;
		} break;

		default: {
			r_bit_count = 0;
			r_red_mask = 0;
			r_green_mask = 0;
			r_blue_mask = 0;
			r_alpha_mask = 0;
		} break;
	}
}

Vector<uint8_t> save_dds_buffer(const Ref<Image> &p_img) {
	Ref<StreamPeerBuffer> stream_buffer;
	stream_buffer.instantiate();

	Ref<Image> image = p_img;

	stream_buffer->put_32(DDS_MAGIC);
	stream_buffer->put_32(DDS_HEADER_SIZE);

	uint32_t flags = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH | DDSD_PIXELFORMAT | DDSD_PITCH | DDSD_LINEARSIZE;

	if (image->has_mipmaps()) {
		flags |= DDSD_MIPMAPCOUNT;
	}

	stream_buffer->put_32(flags);

	uint32_t height = image->get_height();
	stream_buffer->put_32(height);

	uint32_t width = image->get_width();
	stream_buffer->put_32(width);

	DDSFormat dds_format = _image_format_to_dds_format(image->get_format());
	const DDSFormatInfo &info = dds_format_info[dds_format];

	uint32_t depth = 1; // Default depth for 2D textures

	uint32_t pitch;
	if (info.compressed) {
		pitch = ((MAX(info.divisor, width) + info.divisor - 1) / info.divisor) * ((MAX(info.divisor, height) + info.divisor - 1) / info.divisor) * info.block_size;
	} else {
		pitch = width * info.block_size;
	}

	stream_buffer->put_32(pitch);
	stream_buffer->put_32(depth);

	uint32_t mipmaps = image->has_mipmaps() ? (image->get_mipmap_count() + 1) : 1;
	stream_buffer->put_32(mipmaps);

	uint32_t reserved = 0;
	for (int i = 0; i < 11; i++) {
		stream_buffer->put_32(reserved);
	}

	stream_buffer->put_32(DDS_PIXELFORMAT_SIZE);

	uint32_t pf_flags = 0;

	if (info.compressed) {
		pf_flags = DDPF_FOURCC;
	} else {
		pf_flags = DDPF_RGB;

		if (image->get_format() == Image::FORMAT_LA8 || image->get_format() == Image::FORMAT_RG8 || image->get_format() == Image::FORMAT_RGBA8 || image->get_format() == Image::FORMAT_RGBA4444) {
			pf_flags |= DDPF_ALPHAPIXELS;
		}
	}

	stream_buffer->put_32(pf_flags);

	bool needs_pixeldata_swap = false;

	DDSFormatType format_type = _dds_format_get_type(dds_format);

	if (format_type == DDFT_BITMASK) {
		// Uncompressed bitmasked.
		stream_buffer->put_32(0); // FourCC

		uint32_t bit_count, r_mask, g_mask, b_mask, a_mask;
		_get_dds_pixel_bitmask(image->get_format(), bit_count, r_mask, g_mask, b_mask, a_mask);

		stream_buffer->put_32(bit_count);
		stream_buffer->put_32(r_mask);
		stream_buffer->put_32(g_mask);
		stream_buffer->put_32(b_mask);
		stream_buffer->put_32(a_mask);

		if (image->get_format() == Image::FORMAT_RGBA4444 || image->get_format() == Image::FORMAT_RGB565 || image->get_format() == Image::FORMAT_RGB8) {
			needs_pixeldata_swap = true;
		}
	} else if (format_type == DDFT_FOURCC) {
		// FourCC.
		uint32_t fourcc = _image_format_to_fourcc_format(image->get_format());
		stream_buffer->put_32(fourcc);

		stream_buffer->put_32(0); // Bit count
		stream_buffer->put_32(0); // R Bitmask
		stream_buffer->put_32(0); // G Bitmask
		stream_buffer->put_32(0); // B Bitmask
		stream_buffer->put_32(0); // A Bitmask
	} else {
		// DXGI format and DX10 header.
		stream_buffer->put_32(DDFCC_DX10);

		stream_buffer->put_32(0); // Bit count
		stream_buffer->put_32(0); // R Bitmask
		stream_buffer->put_32(0); // G Bitmask
		stream_buffer->put_32(0); // B Bitmask
		stream_buffer->put_32(0); // A Bitmask
	}

	uint32_t caps1 = info.compressed ? DDSD_LINEARSIZE : DDSD_PITCH;
	stream_buffer->put_32(caps1);

	stream_buffer->put_32(0); // Caps2
	stream_buffer->put_32(0); // Caps3
	stream_buffer->put_32(0); // Caps4
	stream_buffer->put_32(0); // Reserved 2

	if (format_type == DDFT_DXGI) {
		// DX10 header.
		uint32_t dxgi_format = _image_format_to_dxgi_format(image->get_format());
		stream_buffer->put_32(dxgi_format);
		stream_buffer->put_32(DX10D_2D);
		stream_buffer->put_32(0); // Misc flags 1
		stream_buffer->put_32(1); // Array size
		stream_buffer->put_32(0); // Misc flags 2
	}

	for (uint32_t mip_i = 0; mip_i < mipmaps; mip_i++) {
		uint32_t mip_width = MAX(1u, width >> mip_i);
		uint32_t mip_height = MAX(1u, height >> mip_i);

		uint32_t expected_size = 0;
		if (info.compressed) {
			uint32_t blocks_x = (mip_width + info.divisor - 1) / info.divisor;
			uint32_t blocks_y = (mip_height + info.divisor - 1) / info.divisor;
			expected_size = blocks_x * blocks_y * info.block_size;
		} else {
			expected_size = mip_width * mip_height * info.block_size;
		}

		if (needs_pixeldata_swap) {
			// The image's channels need to be swapped.
			Ref<Image> mip_image = image->get_image_from_mipmap(mip_i);
			Vector<uint8_t> data = mip_image->get_data();

			ERR_FAIL_COND_V_MSG(data.size() != expected_size, Vector<uint8_t>(),
					"Image data size mismatch for mipmap level " + itos(mip_i) +
							". Expected size: " + itos(expected_size) + ", actual size: " + itos(data.size()) + ".");

			if (mip_image->get_format() == Image::FORMAT_RGBA4444) {
				// RGBA4 to BGRA4
				const int64_t data_size = data.size();
				uint8_t *wb = data.ptrw();

				for (int64_t data_i = 0; data_i < data_size; data_i += 2) {
					uint8_t r = wb[data_i + 0] & 0x0F;
					uint8_t b = wb[data_i + 1] & 0x0F;

					wb[data_i + 0] = (wb[data_i + 0] & 0xF0) | b;
					wb[data_i + 1] = (wb[data_i + 1] & 0xF0) | r;
				}
			} else if (mip_image->get_format() == Image::FORMAT_RGB565) {
				// RGB565 to BGR565
				const int64_t data_size = data.size();
				uint8_t *wb = data.ptrw();

				for (int64_t data_i = 0; data_i < data_size; data_i += 2) {
					uint16_t px = wb[data_i] | (wb[data_i + 1] << 8);

					uint8_t r = (px >> 11) & 0x1F;
					uint8_t g = (px >> 5) & 0x3F;
					uint8_t b = px & 0x1F;

					uint16_t out_px = (b << 11) | (g << 5) | r;

					wb[data_i + 0] = out_px & 0xFF;
					wb[data_i + 1] = (out_px >> 8) & 0xFF;
				}
			} else if (mip_image->get_format() == Image::FORMAT_RGB8) {
				// RGB8 to BGR8
				const int64_t data_size = data.size();
				uint8_t *wb = data.ptrw();

				for (int64_t data_i = 0; data_i < data_size; data_i += 3) {
					SWAP(wb[data_i], wb[data_i + 2]);
				}
			}

			stream_buffer->put_data(data.ptr(), data.size());
		} else {
			int64_t ofs, size;

			image->get_mipmap_offset_and_size(mip_i, ofs, size);

			ERR_FAIL_COND_V_MSG(size != expected_size, Vector<uint8_t>(),
					"Image data size mismatch for mipmap level " + itos(mip_i) +
							". Expected size: " + itos(expected_size) + ", actual size: " + itos(size) + ".");

			stream_buffer->put_data(image->ptr() + ofs, size);
		}
	}

	return stream_buffer->get_data_array();
}

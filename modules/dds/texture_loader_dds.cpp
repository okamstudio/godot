/**************************************************************************/
/*  texture_loader_dds.cpp                                                */
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

#include "texture_loader_dds.h"

#include "core/io/file_access.h"
#include "core/io/file_access_memory.h"
#include "dds.h"
#include "scene/resources/image_texture.h"

DDSFormat _dxgi_to_dds_format(uint32_t p_dxgi_format) {
	switch (p_dxgi_format) {
		case DXGI_R32G32B32A32_FLOAT: {
			return DDS_RGBA32F;
		}
		case DXGI_R32G32B32_FLOAT: {
			return DDS_RGB32F;
		}
		case DXGI_R16G16B16A16_FLOAT: {
			return DDS_RGBA16F;
		}
		case DXGI_R32G32_FLOAT: {
			return DDS_RG32F;
		}
		case DXGI_R10G10B10A2_UNORM: {
			return DDS_RGB10A2;
		}
		case DXGI_R8G8B8A8_UNORM:
		case DXGI_R8G8B8A8_UNORM_SRGB: {
			return DDS_RGBA8;
		}
		case DXGI_R16G16_FLOAT: {
			return DDS_RG16F;
		}
		case DXGI_R32_FLOAT: {
			return DDS_R32F;
		}
		case DXGI_R8_UNORM:
		case DXGI_A8_UNORM: {
			return DDS_LUMINANCE;
		}
		case DXGI_R16_FLOAT: {
			return DDS_R16F;
		}
		case DXGI_R8G8_UNORM: {
			return DDS_LUMINANCE_ALPHA;
		}
		case DXGI_R9G9B9E5: {
			return DDS_RGB9E5;
		}
		case DXGI_BC1_UNORM:
		case DXGI_BC1_UNORM_SRGB: {
			return DDS_DXT1;
		}
		case DXGI_BC2_UNORM:
		case DXGI_BC2_UNORM_SRGB: {
			return DDS_DXT3;
		}
		case DXGI_BC3_UNORM:
		case DXGI_BC3_UNORM_SRGB: {
			return DDS_DXT5;
		}
		case DXGI_BC4_UNORM: {
			return DDS_ATI1;
		}
		case DXGI_BC5_UNORM: {
			return DDS_ATI2;
		}
		case DXGI_B5G6R5_UNORM: {
			return DDS_BGR565;
		}
		case DXGI_B5G5R5A1_UNORM: {
			return DDS_BGR5A1;
		}
		case DXGI_B8G8R8A8_UNORM: {
			return DDS_BGRA8;
		}
		case DXGI_BC6H_UF16: {
			return DDS_BC6U;
		}
		case DXGI_BC6H_SF16: {
			return DDS_BC6S;
		}
		case DXGI_BC7_UNORM:
		case DXGI_BC7_UNORM_SRGB: {
			return DDS_BC7;
		}
		case DXGI_B4G4R4A4_UNORM: {
			return DDS_BGRA4;
		}

		default: {
			return DDS_MAX;
		}
	}
}

static Ref<Image> _dds_load_layer(Ref<FileAccess> p_file, DDSFormat p_dds_format, uint32_t p_width, uint32_t p_height, uint32_t p_mipmaps, uint32_t p_pitch, uint32_t p_flags, Vector<uint8_t> &r_src_data) {
	const DDSFormatInfo &info = dds_format_info[p_dds_format];

	uint32_t w = p_width;
	uint32_t h = p_height;

	if (info.compressed) {
		// BC compressed.
		w += w % info.divisor;
		h += h % info.divisor;
		if (w != p_width) {
			WARN_PRINT(vformat("%s: DDS width '%d' is not divisible by %d. This is not allowed as per the DDS specification, attempting to load anyway.", p_file->get_path(), p_width, info.divisor));
		}
		if (h != p_height) {
			WARN_PRINT(vformat("%s: DDS height '%d' is not divisible by %d. This is not allowed as per the DDS specification, attempting to load anyway.", p_file->get_path(), p_height, info.divisor));
		}

		uint32_t size = MAX(info.divisor, w) / info.divisor * MAX(info.divisor, h) / info.divisor * info.block_size;

		if (p_flags & DDSD_LINEARSIZE) {
			ERR_FAIL_COND_V_MSG(size != p_pitch, Ref<Resource>(), "DDS header flags specify that a linear size of the top-level image is present, but the specified size does not match the expected value.");
		} else {
			ERR_FAIL_COND_V_MSG(p_pitch != 0, Ref<Resource>(), "DDS header flags specify that no linear size will given for the top-level image, but a non-zero linear size value is present in the header.");
		}

		for (uint32_t i = 1; i < p_mipmaps; i++) {
			w = MAX(1u, w >> 1);
			h = MAX(1u, h >> 1);

			uint32_t bsize = MAX(info.divisor, w) / info.divisor * MAX(info.divisor, h) / info.divisor * info.block_size;
			size += bsize;
		}

		r_src_data.resize(size);
		uint8_t *wb = r_src_data.ptrw();
		p_file->get_buffer(wb, size);

	} else {
		// Generic uncompressed.
		uint32_t size = p_width * p_height * info.block_size;

		for (uint32_t i = 1; i < p_mipmaps; i++) {
			w = (w + 1) >> 1;
			h = (h + 1) >> 1;
			size += w * h * info.block_size;
		}

		// Calculate the space these formats will take up after decoding.
		switch (p_dds_format) {
			case DDS_BGR565:
				size = size * 3 / 2;
				break;

			case DDS_BGR5A1:
			case DDS_BGRA4:
			case DDS_B2GR3A8:
			case DDS_LUMINANCE_ALPHA_4:
				size = size * 2;
				break;

			case DDS_B2GR3:
				size = size * 3;
				break;

			default:
				break;
		}

		r_src_data.resize(size);
		uint8_t *wb = r_src_data.ptrw();
		p_file->get_buffer(wb, size);

		switch (p_dds_format) {
			case DDS_BGR5A1: {
				// To RGBA8.
				int colcount = size / 4;

				for (int i = colcount - 1; i >= 0; i--) {
					int src_ofs = i * 2;
					int dst_ofs = i * 4;

					uint8_t a = wb[src_ofs + 1] & 0x80;
					uint8_t b = wb[src_ofs] & 0x1F;
					uint8_t g = (wb[src_ofs] >> 5) | ((wb[src_ofs + 1] & 0x3) << 3);
					uint8_t r = (wb[src_ofs + 1] >> 2) & 0x1F;

					wb[dst_ofs + 0] = r << 3;
					wb[dst_ofs + 1] = g << 3;
					wb[dst_ofs + 2] = b << 3;
					wb[dst_ofs + 3] = a ? 255 : 0;
				}

			} break;
			case DDS_BGR565: {
				// To RGB8.
				int colcount = size / 3;

				for (int i = colcount - 1; i >= 0; i--) {
					int src_ofs = i * 2;
					int dst_ofs = i * 3;

					uint8_t b = wb[src_ofs] & 0x1F;
					uint8_t g = (wb[src_ofs] >> 5) | ((wb[src_ofs + 1] & 0x7) << 3);
					uint8_t r = wb[src_ofs + 1] >> 3;

					wb[dst_ofs + 0] = r << 3;
					wb[dst_ofs + 1] = g << 2;
					wb[dst_ofs + 2] = b << 3;
				}

			} break;
			case DDS_BGRA4: {
				// To RGBA8.
				int colcount = size / 4;

				for (int i = colcount - 1; i >= 0; i--) {
					int src_ofs = i * 2;
					int dst_ofs = i * 4;

					uint8_t b = wb[src_ofs] & 0x0F;
					uint8_t g = wb[src_ofs] & 0xF0;
					uint8_t r = wb[src_ofs + 1] & 0x0F;
					uint8_t a = wb[src_ofs + 1] & 0xF0;

					wb[dst_ofs] = (r << 4) | r;
					wb[dst_ofs + 1] = g | (g >> 4);
					wb[dst_ofs + 2] = (b << 4) | b;
					wb[dst_ofs + 3] = a | (a >> 4);
				}

			} break;
			case DDS_B2GR3: {
				// To RGB8.
				int colcount = size / 3;

				for (int i = colcount - 1; i >= 0; i--) {
					int src_ofs = i;
					int dst_ofs = i * 3;

					uint8_t b = (wb[src_ofs] & 0x3) << 6;
					uint8_t g = (wb[src_ofs] & 0x1C) << 3;
					uint8_t r = (wb[src_ofs] & 0xE0);

					wb[dst_ofs] = r;
					wb[dst_ofs + 1] = g;
					wb[dst_ofs + 2] = b;
				}

			} break;
			case DDS_B2GR3A8: {
				// To RGBA8.
				int colcount = size / 4;

				for (int i = colcount - 1; i >= 0; i--) {
					int src_ofs = i * 2;
					int dst_ofs = i * 4;

					uint8_t b = (wb[src_ofs] & 0x3) << 6;
					uint8_t g = (wb[src_ofs] & 0x1C) << 3;
					uint8_t r = (wb[src_ofs] & 0xE0);
					uint8_t a = wb[src_ofs + 1];

					wb[dst_ofs] = r;
					wb[dst_ofs + 1] = g;
					wb[dst_ofs + 2] = b;
					wb[dst_ofs + 3] = a;
				}

			} break;
			case DDS_RGB10A2: {
				// To RGBA8.
				int colcount = size / 4;

				for (int i = 0; i < colcount; i++) {
					int ofs = i * 4;

					uint32_t w32 = uint32_t(wb[ofs + 0]) | (uint32_t(wb[ofs + 1]) << 8) | (uint32_t(wb[ofs + 2]) << 16) | (uint32_t(wb[ofs + 3]) << 24);

					// This method follows the 'standard' way of decoding 10-bit dds files,
					// which means the ones created with DirectXTex will be loaded incorrectly.
					uint8_t a = (w32 & 0xc0000000) >> 24;
					uint8_t r = (w32 & 0x3ff) >> 2;
					uint8_t g = (w32 & 0xffc00) >> 12;
					uint8_t b = (w32 & 0x3ff00000) >> 22;

					wb[ofs + 0] = r;
					wb[ofs + 1] = g;
					wb[ofs + 2] = b;
					wb[ofs + 3] = a == 0xc0 ? 255 : a; // 0xc0 should be opaque.
				}

			} break;
			case DDS_BGR10A2: {
				// To RGBA8.
				int colcount = size / 4;

				for (int i = 0; i < colcount; i++) {
					int ofs = i * 4;

					uint32_t w32 = uint32_t(wb[ofs + 0]) | (uint32_t(wb[ofs + 1]) << 8) | (uint32_t(wb[ofs + 2]) << 16) | (uint32_t(wb[ofs + 3]) << 24);

					// This method follows the 'standard' way of decoding 10-bit dds files,
					// which means the ones created with DirectXTex will be loaded incorrectly.
					uint8_t a = (w32 & 0xc0000000) >> 24;
					uint8_t r = (w32 & 0x3ff00000) >> 22;
					uint8_t g = (w32 & 0xffc00) >> 12;
					uint8_t b = (w32 & 0x3ff) >> 2;

					wb[ofs + 0] = r;
					wb[ofs + 1] = g;
					wb[ofs + 2] = b;
					wb[ofs + 3] = a == 0xc0 ? 255 : a; // 0xc0 should be opaque.
				}

			} break;

			// Channel-swapped.
			case DDS_BGRA8: {
				// To RGBA8.
				int colcount = size / 4;

				for (int i = 0; i < colcount; i++) {
					SWAP(wb[i * 4 + 0], wb[i * 4 + 2]);
				}

			} break;
			case DDS_BGR8: {
				// To RGB8.
				int colcount = size / 3;

				for (int i = 0; i < colcount; i++) {
					SWAP(wb[i * 3 + 0], wb[i * 3 + 2]);
				}

			} break;

			// Grayscale.
			case DDS_LUMINANCE_ALPHA_4: {
				// To LA8.
				int colcount = size / 2;

				for (int i = colcount - 1; i >= 0; i--) {
					int src_ofs = i;
					int dst_ofs = i * 2;

					uint8_t l = wb[src_ofs] & 0x0F;
					uint8_t a = wb[src_ofs] & 0xF0;

					wb[dst_ofs] = (l << 4) | l;
					wb[dst_ofs + 1] = a | (a >> 4);
				}

			} break;

			default: {
			}
		}
	}

	return memnew(Image(p_width, p_height, p_mipmaps > 1, info.format, r_src_data));
}

static Vector<Ref<Image>> _dds_load_images(Ref<FileAccess> f, DDSFormat dds_format, uint32_t width, uint32_t height, uint32_t mipmaps, uint32_t pitch, uint32_t flags, uint32_t layer_count) {
	Vector<uint8_t> src_data;
	Vector<Ref<Image>> images;
	images.resize(layer_count);

	for (uint32_t i = 0; i < layer_count; i++) {
		images.write[i] = _dds_load_layer(f, dds_format, width, height, mipmaps, pitch, flags, src_data);
	}

	return images;
}

static Ref<Resource> _dds_create_texture(Vector<Ref<Image>> images, uint32_t dds_type, uint32_t width, uint32_t height, uint32_t layer_count, uint32_t mipmaps, Error *r_error) {
	if ((dds_type & DDST_TYPE_MASK) == DDST_2D) {
		if (dds_type & DDST_ARRAY) {
			Ref<Texture2DArray> texture = memnew(Texture2DArray());
			texture->create_from_images(images);

			if (r_error) {
				*r_error = OK;
			}

			return texture;

		} else {
			if (r_error) {
				*r_error = OK;
			}

			return ImageTexture::create_from_image(images[0]);
		}

	} else if ((dds_type & DDST_TYPE_MASK) == DDST_CUBEMAP) {
		ERR_FAIL_COND_V(layer_count % 6 != 0, Ref<Resource>());

		if (dds_type & DDST_ARRAY) {
			Ref<CubemapArray> texture = memnew(CubemapArray());
			texture->create_from_images(images);

			if (r_error) {
				*r_error = OK;
			}

			return texture;

		} else {
			Ref<Cubemap> texture = memnew(Cubemap());
			texture->create_from_images(images);

			if (r_error) {
				*r_error = OK;
			}

			return texture;
		}

	} else if ((dds_type & DDST_TYPE_MASK) == DDST_3D) {
		Ref<ImageTexture3D> texture = memnew(ImageTexture3D());
		texture->create(images[0]->get_format(), width, height, layer_count, mipmaps > 1, images);

		if (r_error) {
			*r_error = OK;
		}

		return texture;
	}

	return Ref<Resource>();
}

static Ref<Resource> _dds_create_texture_from_images(Vector<Ref<Image>> images, DDSFormat dds_format, uint32_t width, uint32_t height, uint32_t mipmaps, uint32_t pitch, uint32_t flags, uint32_t layer_count, uint32_t dds_type, Error *r_error) {
	return _dds_create_texture(images, dds_type, width, height, layer_count, mipmaps, r_error);
}

static Vector<Ref<Image>> _dds_load_images_from_buffer(Ref<FileAccess> f, DDSFormat &dds_format, uint32_t &width, uint32_t &height, uint32_t &mipmaps, uint32_t &pitch, uint32_t &flags, uint32_t &layer_count, uint32_t &dds_type, const String &p_path = "") {
	ERR_FAIL_COND_V_MSG(f.is_null(), Vector<Ref<Image>>(), vformat("Empty DDS texture file."));
	ERR_FAIL_COND_V_MSG(!f->get_length(), Vector<Ref<Image>>(), vformat("Empty DDS texture file."));

	uint32_t magic = f->get_32();
	uint32_t hsize = f->get_32();
	flags = f->get_32();
	height = f->get_32();
	width = f->get_32();
	pitch = f->get_32();
	uint32_t depth = f->get_32();
	mipmaps = f->get_32();

	// Skip reserved.
	for (int i = 0; i < 11; i++) {
		f->get_32();
	}

	// Validate.
	// We don't check DDSD_CAPS or DDSD_PIXELFORMAT, as they're mandatory when writing,
	// but non-mandatory when reading (as some writers don't set them).
	if (magic != DDS_MAGIC || hsize != 124) {
		ERR_FAIL_V_MSG(Vector<Ref<Image>>(), vformat("Invalid or unsupported DDS texture file '%s'.", p_path));
	}

	/* uint32_t format_size = */ f->get_32();
	uint32_t format_flags = f->get_32();
	uint32_t format_fourcc = f->get_32();
	uint32_t format_rgb_bits = f->get_32();
	uint32_t format_red_mask = f->get_32();
	uint32_t format_green_mask = f->get_32();
	uint32_t format_blue_mask = f->get_32();
	uint32_t format_alpha_mask = f->get_32();

	/* uint32_t caps_1 = */ f->get_32();
	uint32_t caps_2 = f->get_32();
	/* uint32_t caps_3 = */ f->get_32();
	/* uint32_t caps_4 = */ f->get_32();

	// Skip reserved.
	f->get_32();

	if (f->get_position() < 128) {
		f->seek(128);
	}

	layer_count = 1;
	dds_type = DDST_2D;

	if (caps_2 & DDSC2_CUBEMAP) {
		dds_type = DDST_CUBEMAP;
		layer_count *= 6;

	} else if (caps_2 & DDSC2_VOLUME) {
		dds_type = DDST_3D;
		layer_count = depth;
	}

	dds_format = DDS_MAX;

	if (format_flags & DDPF_FOURCC) {
		// FourCC formats.
		switch (format_fourcc) {
			case DDFCC_DXT1: {
				dds_format = DDS_DXT1;
			} break;
			case DDFCC_DXT2:
			case DDFCC_DXT3: {
				dds_format = DDS_DXT3;
			} break;
			case DDFCC_DXT4:
			case DDFCC_DXT5: {
				dds_format = DDS_DXT5;
			} break;
			case DDFCC_ATI1:
			case DDFCC_BC4U: {
				dds_format = DDS_ATI1;
			} break;
			case DDFCC_ATI2:
			case DDFCC_BC5U:
			case DDFCC_A2XY: {
				dds_format = DDS_ATI2;
			} break;
			case DDFCC_R16F: {
				dds_format = DDS_R16F;
			} break;
			case DDFCC_RG16F: {
				dds_format = DDS_RG16F;
			} break;
			case DDFCC_RGBA16F: {
				dds_format = DDS_RGBA16F;
			} break;
			case DDFCC_R32F: {
				dds_format = DDS_R32F;
			} break;
			case DDFCC_RG32F: {
				dds_format = DDS_RG32F;
			} break;
			case DDFCC_RGBA32F: {
				dds_format = DDS_RGBA32F;
			} break;
			case DDFCC_DX10: {
				uint32_t dxgi_format = f->get_32();
				uint32_t dimension = f->get_32();
				/* uint32_t misc_flags_1 = */ f->get_32();
				uint32_t array_size = f->get_32();
				/* uint32_t misc_flags_2 = */ f->get_32();

				if (dimension == DX10D_3D) {
					dds_type = DDST_3D;
					layer_count = depth;
				}

				if (array_size > 1) {
					layer_count *= array_size;
					dds_type |= DDST_ARRAY;
				}

				dds_format = _dxgi_to_dds_format(dxgi_format);
			} break;

			default: {
				ERR_FAIL_V_MSG(Vector<Ref<Image>>(), vformat("Unrecognized or unsupported FourCC in DDS '%s'.", p_path));
			}
		}

	} else if (format_flags & DDPF_RGB) {
		// Channel-bitmasked formats.
		if (format_flags & DDPF_ALPHAPIXELS) {
			// With alpha.
			if (format_rgb_bits == 32 && format_red_mask == 0xff0000 && format_green_mask == 0xff00 && format_blue_mask == 0xff && format_alpha_mask == 0xff000000) {
				dds_format = DDS_BGRA8;
			} else if (format_rgb_bits == 32 && format_red_mask == 0xff && format_green_mask == 0xff00 && format_blue_mask == 0xff0000 && format_alpha_mask == 0xff000000) {
				dds_format = DDS_RGBA8;
			} else if (format_rgb_bits == 16 && format_red_mask == 0x00007c00 && format_green_mask == 0x000003e0 && format_blue_mask == 0x0000001f && format_alpha_mask == 0x00008000) {
				dds_format = DDS_BGR5A1;
			} else if (format_rgb_bits == 32 && format_red_mask == 0x3ff00000 && format_green_mask == 0xffc00 && format_blue_mask == 0x3ff && format_alpha_mask == 0xc0000000) {
				dds_format = DDS_BGR10A2;
			} else if (format_rgb_bits == 32 && format_red_mask == 0x3ff && format_green_mask == 0xffc00 && format_blue_mask == 0x3ff00000 && format_alpha_mask == 0xc0000000) {
				dds_format = DDS_RGB10A2;
			} else if (format_rgb_bits == 16 && format_red_mask == 0xf00 && format_green_mask == 0xf0 && format_blue_mask == 0xf && format_alpha_mask == 0xf000) {
				dds_format = DDS_BGRA4;
			} else if (format_rgb_bits == 16 && format_red_mask == 0xe0 && format_green_mask == 0x1c && format_blue_mask == 0x3 && format_alpha_mask == 0xff00) {
				dds_format = DDS_B2GR3A8;
			}

		} else {
			// Without alpha.
			if (format_rgb_bits == 24 && format_red_mask == 0xff0000 && format_green_mask == 0xff00 && format_blue_mask == 0xff) {
				dds_format = DDS_BGR8;
			} else if (format_rgb_bits == 24 && format_red_mask == 0xff && format_green_mask == 0xff00 && format_blue_mask == 0xff0000) {
				dds_format = DDS_RGB8;
			} else if (format_rgb_bits == 16 && format_red_mask == 0x0000f800 && format_green_mask == 0x000007e0 && format_blue_mask == 0x0000001f) {
				dds_format = DDS_BGR565;
			} else if (format_rgb_bits == 8 && format_red_mask == 0xe0 && format_green_mask == 0x1c && format_blue_mask == 0x3) {
				dds_format = DDS_B2GR3;
			}
		}

	} else {
		// Other formats.
		if (format_flags & DDPF_ALPHAONLY && format_rgb_bits == 8 && format_alpha_mask == 0xff) {
			// Alpha only.
			dds_format = DDS_LUMINANCE;
		}
	}

	// Depending on the writer, luminance formats may or may not have the DDPF_RGB or DDPF_LUMINANCE flags defined,
	// so we check for these formats after everything else failed.
	if (dds_format == DDS_MAX) {
		if (format_flags & DDPF_ALPHAPIXELS) {
			// With alpha.
			if (format_rgb_bits == 16 && format_red_mask == 0xff && format_alpha_mask == 0xff00) {
				dds_format = DDS_LUMINANCE_ALPHA;
			} else if (format_rgb_bits == 8 && format_red_mask == 0xf && format_alpha_mask == 0xf0) {
				dds_format = DDS_LUMINANCE_ALPHA_4;
			}

		} else {
			// Without alpha.
			if (format_rgb_bits == 8 && format_red_mask == 0xff) {
				dds_format = DDS_LUMINANCE;
			}
		}
	}

	// No format detected, error.
	if (dds_format == DDS_MAX) {
		ERR_FAIL_V_MSG(Vector<Ref<Image>>(), vformat("Unrecognized or unsupported color layout in DDS '%s'.", p_path));
	}

	if (!(flags & DDSD_MIPMAPCOUNT)) {
		mipmaps = 1;
	}

	return _dds_load_images(f, dds_format, width, height, mipmaps, pitch, flags, layer_count);
}

static Ref<Resource> _dds_load_from_buffer(Ref<FileAccess> f, Error *r_error, const String &p_path = "") {
	if (r_error) {
		*r_error = ERR_FILE_CORRUPT;
	}

	DDSFormat dds_format;
	uint32_t width, height, mipmaps, pitch, flags, layer_count, dds_type;

	Vector<Ref<Image>> images = _dds_load_images_from_buffer(f, dds_format, width, height, mipmaps, pitch, flags, layer_count, dds_type, p_path);
	return _dds_create_texture_from_images(images, dds_format, width, height, mipmaps, pitch, flags, layer_count, dds_type, r_error);
}

static Ref<Resource> _dds_load_from_file(const String &p_path, Error *r_error) {
	if (r_error) {
		*r_error = ERR_CANT_OPEN;
	}

	Error err;
	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::READ, &err);
	if (f.is_null()) {
		return Ref<Resource>();
	}

	return _dds_load_from_buffer(f, r_error, p_path);
}

Ref<Resource> ResourceFormatDDS::load(const String &p_path, const String &p_original_path, Error *r_error, bool p_use_sub_threads, float *r_progress, CacheMode p_cache_mode) {
	return _dds_load_from_file(p_path, r_error);
}

void ResourceFormatDDS::get_recognized_extensions(List<String> *p_extensions) const {
	p_extensions->push_back("dds");
}

bool ResourceFormatDDS::handles_type(const String &p_type) const {
	return ClassDB::is_parent_class(p_type, "Texture");
}

String ResourceFormatDDS::get_resource_type(const String &p_path) const {
	if (p_path.get_extension().to_lower() == "dds") {
		return "Texture";
	}
	return "";
}

Ref<Image> load_mem_dds(const uint8_t *p_dds, int p_size) {
	ERR_FAIL_NULL_V(p_dds, Ref<Image>());
	ERR_FAIL_COND_V(!p_size, Ref<Image>());
	Ref<FileAccessMemory> memfile;
	memfile.instantiate();
	Error open_memfile_error = memfile->open_custom(p_dds, p_size);
	ERR_FAIL_COND_V_MSG(open_memfile_error, Ref<Image>(), "Could not create memfile for DDS image buffer.");

	DDSFormat dds_format;
	uint32_t width, height, mipmaps, pitch, flags, layer_count, dds_type;

	Vector<Ref<Image>> images = _dds_load_images_from_buffer(memfile, dds_format, width, height, mipmaps, pitch, flags, layer_count, dds_type);
	ERR_FAIL_COND_V_MSG(images.is_empty(), Ref<Image>(), "Failed to load DDS image.");

	return images[0];
}

ResourceFormatDDS::ResourceFormatDDS() {
	Image::_dds_mem_loader_func = load_mem_dds;
}

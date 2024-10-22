// basisu_gpu_texture.h
// Copyright (C) 2019-2024 Binomial LLC. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#pragma once
#include "../transcoder/basisu.h"
#include "basisu_etc.h"

namespace basisu
{
	// GPU texture "image"
	class gpu_image
	{
	public:
		enum { cMaxBlockSize = 12 };

		gpu_image()
		{
			clear();
		}

		gpu_image(texture_format fmt, uint32_t width, uint32_t height)
		{
			init(fmt, width, height);
		}

		void clear()
		{
			m_fmt = texture_format::cInvalidTextureFormat;
			m_width = 0;
			m_height = 0;
			m_block_width = 0;
			m_block_height = 0;
			m_blocks_x = 0;
			m_blocks_y = 0;
			m_qwords_per_block = 0;
			m_blocks.clear();
		}

		inline texture_format get_format() const { return m_fmt; }
		inline bool is_hdr() const { return is_hdr_texture_format(m_fmt); }
		
		// Width/height in pixels
		inline uint32_t get_pixel_width() const { return m_width; }
		inline uint32_t get_pixel_height() const { return m_height; }
		
		// Width/height in blocks, row pitch is assumed to be m_blocks_x.
		inline uint32_t get_blocks_x() const { return m_blocks_x; }
		inline uint32_t get_blocks_y() const { return m_blocks_y; }

		// Size of each block in pixels
		inline uint32_t get_block_width() const { return m_block_width; }
		inline uint32_t get_block_height() const { return m_block_height; }

		inline uint32_t get_qwords_per_block() const { return m_qwords_per_block; }
		inline uint32_t get_total_blocks() const { return m_blocks_x * m_blocks_y; }
		inline uint32_t get_bytes_per_block() const { return get_qwords_per_block() * sizeof(uint64_t); }
		inline uint32_t get_row_pitch_in_bytes() const { return get_bytes_per_block() * get_blocks_x(); }

		inline const uint64_vec &get_blocks() const { return m_blocks; }
		
		inline const uint64_t *get_ptr() const { return &m_blocks[0]; }
		inline uint64_t *get_ptr() { return &m_blocks[0]; }

		inline uint32_t get_size_in_bytes() const { return get_total_blocks() * get_qwords_per_block() * sizeof(uint64_t); }

		inline const void *get_block_ptr(uint32_t block_x, uint32_t block_y, uint32_t element_index = 0) const
		{
			assert(block_x < m_blocks_x && block_y < m_blocks_y);
			return &m_blocks[(block_x + block_y * m_blocks_x) * m_qwords_per_block + element_index];
		}

		inline void *get_block_ptr(uint32_t block_x, uint32_t block_y, uint32_t element_index = 0)
		{
			assert(block_x < m_blocks_x && block_y < m_blocks_y && element_index < m_qwords_per_block);
			return &m_blocks[(block_x + block_y * m_blocks_x) * m_qwords_per_block + element_index];
		}

		void init(texture_format fmt, uint32_t width, uint32_t height)
		{
			m_fmt = fmt;
			m_width = width;
			m_height = height;
			m_block_width = basisu::get_block_width(m_fmt);
			m_block_height = basisu::get_block_height(m_fmt);
			m_blocks_x = (m_width + m_block_width - 1) / m_block_width;
			m_blocks_y = (m_height + m_block_height - 1) / m_block_height;
			m_qwords_per_block = basisu::get_qwords_per_block(m_fmt);

			m_blocks.resize(0);
			m_blocks.resize(m_blocks_x * m_blocks_y * m_qwords_per_block);
		}

		// Unpacks LDR textures only.
		bool unpack(image& img) const;

		// Unpacks HDR textures only.
		bool unpack_hdr(imagef& img) const;
		
		inline void override_dimensions(uint32_t w, uint32_t h)
		{
			m_width = w;
			m_height = h;
		}

	private:
		texture_format m_fmt;
		uint32_t m_width, m_height, m_blocks_x, m_blocks_y, m_block_width, m_block_height, m_qwords_per_block;
		uint64_vec m_blocks;
	};

	typedef basisu::vector<gpu_image> gpu_image_vec;

	// KTX1 file writing
	bool create_ktx_texture_file(uint8_vec &ktx_data, const basisu::vector<gpu_image_vec>& gpu_images, bool cubemap_flag);
	
	bool does_dds_support_format(texture_format fmt);
	bool write_dds_file(uint8_vec& dds_data, const basisu::vector<gpu_image_vec>& gpu_images, bool cubemap_flag, bool use_srgb_format);
	bool write_dds_file(const char* pFilename, const basisu::vector<gpu_image_vec>& gpu_images, bool cubemap_flag, bool use_srgb_format);

	// Currently reads 2D 32bpp RGBA, 16-bit HALF RGBA, or 32-bit FLOAT RGBA, with or without mipmaps. No tex arrays or cubemaps, yet.
	bool read_uncompressed_dds_file(const char* pFilename, basisu::vector<image>& ldr_mips, basisu::vector<imagef>& hdr_mips);

	// Supports DDS and KTX
	bool write_compressed_texture_file(const char *pFilename, const basisu::vector<gpu_image_vec>& g, bool cubemap_flag, bool use_srgb_format);
	bool write_compressed_texture_file(const char* pFilename, const gpu_image_vec& g, bool use_srgb_format);
	bool write_compressed_texture_file(const char *pFilename, const gpu_image &g, bool use_srgb_format);
	
	bool write_3dfx_out_file(const char* pFilename, const gpu_image& gi);

	// GPU texture block unpacking
	// For ETC1, use in basisu_etc.h: bool unpack_etc1(const etc_block& block, color_rgba *pDst, bool preserve_alpha)
	void unpack_etc2_eac(const void *pBlock_bits, color_rgba *pPixels);
	bool unpack_bc1(const void *pBlock_bits, color_rgba *pPixels, bool set_alpha);
	void unpack_bc4(const void *pBlock_bits, uint8_t *pPixels, uint32_t stride);
	bool unpack_bc3(const void *pBlock_bits, color_rgba *pPixels);
	void unpack_bc5(const void *pBlock_bits, color_rgba *pPixels);
	bool unpack_bc7_mode6(const void *pBlock_bits, color_rgba *pPixels);
	bool unpack_bc7(const void* pBlock_bits, color_rgba* pPixels); // full format
	bool unpack_bc6h(const void* pSrc_block, void* pDst_block, bool is_signed, uint32_t dest_pitch_in_halfs = 4 * 3); // full format, outputs HALF values, RGB texels only (not RGBA)
	void unpack_atc(const void* pBlock_bits, color_rgba* pPixels);
	// We only support CC_MIXED non-alpha blocks here because that's the only mode the transcoder uses at the moment.
	bool unpack_fxt1(const void* p, color_rgba* pPixels);
	// PVRTC2 is currently limited to only what our transcoder outputs (non-interpolated, hard_flag=1 modulation=0). In this mode, PVRTC2 looks much like BC1/ATC.
	bool unpack_pvrtc2(const void* p, color_rgba* pPixels);
	void unpack_etc2_eac_r(const void *p, color_rgba* pPixels, uint32_t c);
	void unpack_etc2_eac_rg(const void* p, color_rgba* pPixels);
	
	// unpack_block() is primarily intended to unpack texture data created by the transcoder.
	// For some texture formats (like ETC2 RGB, PVRTC2, FXT1) it's not yet a complete implementation.
	// Unpacks LDR texture formats only.
	bool unpack_block(texture_format fmt, const void *pBlock, color_rgba *pPixels);

	// Unpacks HDR texture formats only.
	bool unpack_block_hdr(texture_format fmt, const void* pBlock, vec4F* pPixels);
	
	bool write_astc_file(const char* pFilename, const void* pBlocks, uint32_t block_width, uint32_t block_height, uint32_t dim_x, uint32_t dim_y);
							
} // namespace basisu


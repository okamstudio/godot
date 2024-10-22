// basisu_basis_file.cpp
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
#include "basisu_basis_file.h"
#include "../transcoder/basisu_transcoder.h"

// The output file version. Keep in sync with BASISD_SUPPORTED_BASIS_VERSION.
#define BASIS_FILE_VERSION (0x13)

namespace basisu
{
	void basisu_file::create_header(const basisu_backend_output &encoder_output, basist::basis_texture_type tex_type, uint32_t userdata0, uint32_t userdata1, bool y_flipped, uint32_t us_per_frame)
	{
		m_header.m_header_size = sizeof(basist::basis_file_header);

		m_header.m_data_size = m_total_file_size - sizeof(basist::basis_file_header);

		m_header.m_total_slices = (uint32_t)encoder_output.m_slice_desc.size();
		
		m_header.m_total_images = 0;
		for (uint32_t i = 0; i < encoder_output.m_slice_desc.size(); i++)
			m_header.m_total_images = maximum<uint32_t>(m_header.m_total_images, encoder_output.m_slice_desc[i].m_source_file_index + 1);
		
		m_header.m_tex_format = (int)encoder_output.m_tex_format;
		m_header.m_flags = 0;
		
		if (encoder_output.m_etc1s)
		{
			assert(encoder_output.m_tex_format == basist::basis_tex_format::cETC1S);
			m_header.m_flags = m_header.m_flags | basist::cBASISHeaderFlagETC1S;
		}
		else
		{
			assert(encoder_output.m_tex_format != basist::basis_tex_format::cETC1S);
		}

		if (y_flipped)
			m_header.m_flags = m_header.m_flags | basist::cBASISHeaderFlagYFlipped;
		if (encoder_output.m_uses_global_codebooks)
			m_header.m_flags = m_header.m_flags | basist::cBASISHeaderFlagUsesGlobalCodebook;
		if (encoder_output.m_srgb)
			m_header.m_flags = m_header.m_flags | basist::cBASISHeaderFlagSRGB;
				
		for (uint32_t i = 0; i < encoder_output.m_slice_desc.size(); i++)
		{
			if (encoder_output.m_slice_desc[i].m_alpha)
			{
				m_header.m_flags = m_header.m_flags | basist::cBASISHeaderFlagHasAlphaSlices;
				break;
			}
		}

		m_header.m_tex_type = static_cast<uint8_t>(tex_type);
		m_header.m_us_per_frame = clamp<uint32_t>(us_per_frame, 0, basist::cBASISMaxUSPerFrame);

		m_header.m_userdata0 = userdata0;
		m_header.m_userdata1 = userdata1;

		m_header.m_total_endpoints = encoder_output.m_num_endpoints;
		if (!encoder_output.m_uses_global_codebooks)
		{
			m_header.m_endpoint_cb_file_ofs = m_endpoint_cb_file_ofs;
			m_header.m_endpoint_cb_file_size = (uint32_t)encoder_output.m_endpoint_palette.size();
		}
		else
		{
			assert(!m_endpoint_cb_file_ofs);
		}

		m_header.m_total_selectors = encoder_output.m_num_selectors;
		if (!encoder_output.m_uses_global_codebooks)
		{
			m_header.m_selector_cb_file_ofs = m_selector_cb_file_ofs;
			m_header.m_selector_cb_file_size = (uint32_t)encoder_output.m_selector_palette.size();
		}
		else
		{
			assert(!m_selector_cb_file_ofs);
		}

		m_header.m_tables_file_ofs = m_tables_file_ofs;
		m_header.m_tables_file_size = (uint32_t)encoder_output.m_slice_image_tables.size();

		m_header.m_slice_desc_file_ofs = m_slice_descs_file_ofs;
	}

	bool basisu_file::create_image_descs(const basisu_backend_output &encoder_output)
	{
		const basisu_backend_slice_desc_vec &slice_descs = encoder_output.m_slice_desc;

		m_images_descs.resize(slice_descs.size());

		uint64_t cur_slice_file_ofs = m_first_image_file_ofs;
		for (uint32_t i = 0; i < slice_descs.size(); i++)
		{
			clear_obj(m_images_descs[i]);

			m_images_descs[i].m_image_index = slice_descs[i].m_source_file_index;
			m_images_descs[i].m_level_index = slice_descs[i].m_mip_index;
			
			if (slice_descs[i].m_alpha)
				m_images_descs[i].m_flags = m_images_descs[i].m_flags | basist::cSliceDescFlagsHasAlpha;
			if (slice_descs[i].m_iframe)
				m_images_descs[i].m_flags = m_images_descs[i].m_flags | basist::cSliceDescFlagsFrameIsIFrame;

			m_images_descs[i].m_orig_width = slice_descs[i].m_orig_width;
			m_images_descs[i].m_orig_height = slice_descs[i].m_orig_height;
			m_images_descs[i].m_num_blocks_x = slice_descs[i].m_num_blocks_x;
			m_images_descs[i].m_num_blocks_y = slice_descs[i].m_num_blocks_y;
			m_images_descs[i].m_slice_data_crc16 = encoder_output.m_slice_image_crcs[i];

			if (encoder_output.m_slice_image_data[i].size() > UINT32_MAX)
			{
				error_printf("basisu_file::create_image_descs: Basis file too large\n");
				return false;
			}

			const uint32_t image_size = (uint32_t)encoder_output.m_slice_image_data[i].size();

			m_images_descs[i].m_file_ofs = (uint32_t)cur_slice_file_ofs;
			m_images_descs[i].m_file_size = image_size;

			cur_slice_file_ofs += image_size;
			if (cur_slice_file_ofs > UINT32_MAX)
			{
				error_printf("basisu_file::create_image_descs: Basis file too large\n");
				return false;
			}
		}

		assert(cur_slice_file_ofs == m_total_file_size);
		return true;
	}

	void basisu_file::create_comp_data(const basisu_backend_output &encoder_output)
	{
		const basisu_backend_slice_desc_vec &slice_descs = encoder_output.m_slice_desc;

		append_vector(m_comp_data, reinterpret_cast<const uint8_t *>(&m_header), sizeof(m_header));

		assert(m_comp_data.size() == m_slice_descs_file_ofs);
		append_vector(m_comp_data, reinterpret_cast<const uint8_t*>(&m_images_descs[0]), m_images_descs.size() * sizeof(m_images_descs[0]));

		if (!encoder_output.m_uses_global_codebooks)
		{
			if (encoder_output.m_endpoint_palette.size())
			{
				assert(m_comp_data.size() == m_endpoint_cb_file_ofs);
				append_vector(m_comp_data, reinterpret_cast<const uint8_t*>(&encoder_output.m_endpoint_palette[0]), encoder_output.m_endpoint_palette.size());
			}

			if (encoder_output.m_selector_palette.size())
			{
				assert(m_comp_data.size() == m_selector_cb_file_ofs);
				append_vector(m_comp_data, reinterpret_cast<const uint8_t*>(&encoder_output.m_selector_palette[0]), encoder_output.m_selector_palette.size());
			}
		}

		if (encoder_output.m_slice_image_tables.size())
		{
			assert(m_comp_data.size() == m_tables_file_ofs);
			append_vector(m_comp_data, reinterpret_cast<const uint8_t*>(&encoder_output.m_slice_image_tables[0]), encoder_output.m_slice_image_tables.size());
		}

		assert(m_comp_data.size() == m_first_image_file_ofs);
		for (uint32_t i = 0; i < slice_descs.size(); i++)
			append_vector(m_comp_data, &encoder_output.m_slice_image_data[i][0], encoder_output.m_slice_image_data[i].size());

		assert(m_comp_data.size() == m_total_file_size);
	}

	void basisu_file::fixup_crcs()
	{
		basist::basis_file_header *pHeader = reinterpret_cast<basist::basis_file_header *>(&m_comp_data[0]);

		pHeader->m_data_size = m_total_file_size - sizeof(basist::basis_file_header);
		pHeader->m_data_crc16 = basist::crc16(&m_comp_data[0] + sizeof(basist::basis_file_header), m_total_file_size - sizeof(basist::basis_file_header), 0);
				
		pHeader->m_header_crc16 = basist::crc16(&pHeader->m_data_size, sizeof(basist::basis_file_header) - BASISU_OFFSETOF(basist::basis_file_header, m_data_size), 0);

		pHeader->m_sig = basist::basis_file_header::cBASISSigValue;
		pHeader->m_ver = BASIS_FILE_VERSION;// basist::basis_file_header::cBASISFirstVersion;
	}

	bool basisu_file::init(const basisu_backend_output &encoder_output, basist::basis_texture_type tex_type, uint32_t userdata0, uint32_t userdata1, bool y_flipped, uint32_t us_per_frame)
	{
		clear();

		const basisu_backend_slice_desc_vec &slice_descs = encoder_output.m_slice_desc;

		// The Basis file uses 32-bit fields for lots of stuff, so make sure it's not too large.
		uint64_t check_size = 0;
		if (!encoder_output.m_uses_global_codebooks)
		{
			check_size = (uint64_t)sizeof(basist::basis_file_header) + (uint64_t)sizeof(basist::basis_slice_desc) * slice_descs.size() +
			(uint64_t)encoder_output.m_endpoint_palette.size() + (uint64_t)encoder_output.m_selector_palette.size() + (uint64_t)encoder_output.m_slice_image_tables.size();
		}
		else
		{
			check_size = (uint64_t)sizeof(basist::basis_file_header) + (uint64_t)sizeof(basist::basis_slice_desc) * slice_descs.size() +
				(uint64_t)encoder_output.m_slice_image_tables.size();
		}
		if (check_size >= 0xFFFF0000ULL)
		{
			error_printf("basisu_file::init: File is too large!\n");
			return false;
		}

		m_header_file_ofs = 0;
		m_slice_descs_file_ofs = sizeof(basist::basis_file_header);
		if (encoder_output.m_tex_format == basist::basis_tex_format::cETC1S)
		{
			if (encoder_output.m_uses_global_codebooks)
			{
				m_endpoint_cb_file_ofs = 0;
				m_selector_cb_file_ofs = 0;
				m_tables_file_ofs = m_slice_descs_file_ofs + sizeof(basist::basis_slice_desc) * (uint32_t)slice_descs.size();
			}
			else
			{
				m_endpoint_cb_file_ofs = m_slice_descs_file_ofs + sizeof(basist::basis_slice_desc) * (uint32_t)slice_descs.size();
				m_selector_cb_file_ofs = m_endpoint_cb_file_ofs + (uint32_t)encoder_output.m_endpoint_palette.size();
				m_tables_file_ofs = m_selector_cb_file_ofs + (uint32_t)encoder_output.m_selector_palette.size();
			}
			m_first_image_file_ofs = m_tables_file_ofs + (uint32_t)encoder_output.m_slice_image_tables.size();
		}
		else
		{
			m_endpoint_cb_file_ofs = 0;
			m_selector_cb_file_ofs = 0;
			m_tables_file_ofs = 0;
			m_first_image_file_ofs = m_slice_descs_file_ofs + sizeof(basist::basis_slice_desc) * (uint32_t)slice_descs.size();
		}
				
		uint64_t total_file_size = m_first_image_file_ofs;
		for (uint32_t i = 0; i < encoder_output.m_slice_image_data.size(); i++)
			total_file_size += encoder_output.m_slice_image_data[i].size();
		if (total_file_size >= 0xFFFF0000ULL)
		{
			error_printf("basisu_file::init: File is too large!\n");
			return false;
		}

		m_total_file_size = (uint32_t)total_file_size;

		create_header(encoder_output, tex_type, userdata0, userdata1, y_flipped, us_per_frame);

		if (!create_image_descs(encoder_output))
			return false;

		create_comp_data(encoder_output);

		fixup_crcs();

		return true;
	}

} // namespace basisu

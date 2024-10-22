// basisu_basis_file.h
// Copyright (C) 2024 Binomial LLC. All Rights Reserved.
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
#include "../transcoder/basisu_file_headers.h"
#include "basisu_backend.h"

namespace basisu
{
	class basisu_file
	{
		BASISU_NO_EQUALS_OR_COPY_CONSTRUCT(basisu_file);

	public:
		basisu_file()
		{
		}

		void clear()
		{
			m_comp_data.clear();

			clear_obj(m_header);
			m_images_descs.clear();

			m_header_file_ofs = 0;
			m_slice_descs_file_ofs = 0;
			m_endpoint_cb_file_ofs = 0;
			m_selector_cb_file_ofs = 0;
			m_tables_file_ofs = 0;
			m_first_image_file_ofs = 0;
			m_total_file_size = 0;
		}

		bool init(const basisu_backend_output& encoder_output, basist::basis_texture_type tex_type, uint32_t userdata0, uint32_t userdata1, bool y_flipped, uint32_t us_per_frame);

		const uint8_vec &get_compressed_data() const { return m_comp_data; }

	private:
		basist::basis_file_header m_header;
		basisu::vector<basist::basis_slice_desc> m_images_descs;

		uint8_vec m_comp_data;

		uint32_t m_header_file_ofs;
		uint32_t m_slice_descs_file_ofs;
		uint32_t m_endpoint_cb_file_ofs;
		uint32_t m_selector_cb_file_ofs;
		uint32_t m_tables_file_ofs;
		uint32_t m_first_image_file_ofs;
		uint32_t m_total_file_size;

		void create_header(const basisu_backend_output& encoder_output,  basist::basis_texture_type tex_type, uint32_t userdata0, uint32_t userdata1, bool y_flipped, uint32_t us_per_frame);
		bool create_image_descs(const basisu_backend_output& encoder_output);
		void create_comp_data(const basisu_backend_output& encoder_output);
		void fixup_crcs();
	};

} // namespace basisu

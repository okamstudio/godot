// basisu_opencl.h
// Copyright (C) 2019-2024 Binomial LLC. All Rights Reserved.
//
// Note: Undefine or set BASISU_SUPPORT_OPENCL to 0 to completely OpenCL support.
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
#include "basisu_enc.h"
#include "basisu_etc.h"

namespace basisu
{
	bool opencl_init(bool force_serialization);
	void opencl_deinit();
	bool opencl_is_available();

	struct opencl_context;

	// Each thread calling OpenCL should have its own opencl_context_ptr. This corresponds to a OpenCL command queue. (Confusingly, we only use a single OpenCL device "context".)
	typedef opencl_context* opencl_context_ptr;

	opencl_context_ptr opencl_create_context();
	void opencl_destroy_context(opencl_context_ptr context);

#pragma pack(push, 1)
	struct cl_pixel_block
	{
		color_rgba m_pixels[16]; // [y*4+x]
	};
#pragma pack(pop)

	// Must match BASISU_ETC1_CLUSTER_FIT_ORDER_TABLE_SIZE
	const uint32_t OPENCL_ENCODE_ETC1S_MAX_PERMS = 165;

	bool opencl_set_pixel_blocks(opencl_context_ptr pContext, uint32_t total_blocks, const cl_pixel_block* pPixel_blocks);

	bool opencl_encode_etc1s_blocks(opencl_context_ptr pContext, etc_block* pOutput_blocks, bool perceptual, uint32_t total_perms);

	// opencl_encode_etc1s_pixel_clusters

#pragma pack(push, 1)
	struct cl_pixel_cluster
	{
		uint64_t m_total_pixels;
		uint64_t m_first_pixel_index;
	};
#pragma pack(pop)

	bool opencl_encode_etc1s_pixel_clusters(
		opencl_context_ptr pContext,
		etc_block* pOutput_blocks, 
		uint32_t total_clusters,
		const cl_pixel_cluster *pClusters,
		uint64_t total_pixels,
		const color_rgba *pPixels,
		const uint32_t *pPixel_weights,
		bool perceptual, uint32_t total_perms);

	// opencl_refine_endpoint_clusterization

#pragma pack(push, 1)
	struct cl_block_info_struct
	{
		uint16_t m_first_cluster_ofs;
		uint16_t m_num_clusters;
		uint16_t m_cur_cluster_index;
		uint8_t m_cur_cluster_etc_inten;
	};

	struct cl_endpoint_cluster_struct
	{
		color_rgba m_unscaled_color;
		uint8_t m_etc_inten;
		uint16_t m_cluster_index;
	};
#pragma pack(pop)

	bool opencl_refine_endpoint_clusterization(
		opencl_context_ptr pContext,
		const cl_block_info_struct *pPixel_block_info,
		uint32_t total_clusters,
		const cl_endpoint_cluster_struct *pCluster_info,
		const uint32_t *pSorted_block_indices,
		uint32_t* pOutput_cluster_indices, 
		bool perceptual);

	// opencl_find_optimal_selector_clusters_for_each_block

#pragma pack(push, 1)
	struct fosc_selector_struct
	{
		uint32_t m_packed_selectors;	// 4x4 grid of 2-bit selectors
	};

	struct fosc_block_struct
	{
		color_rgba m_etc_color5_inten;  // unscaled 5-bit block color in RGB, alpha has block's intensity index
		uint32_t m_first_selector;		// offset into selector table
		uint32_t m_num_selectors;		// number of selectors to check
	};

	struct fosc_param_struct
	{
		uint32_t m_total_blocks;
		int m_perceptual;
	};
#pragma pack(pop)

	bool opencl_find_optimal_selector_clusters_for_each_block(
		opencl_context_ptr pContext,
		const fosc_block_struct* pInput_block_info,	// one per block
		uint32_t total_input_selectors,
		const fosc_selector_struct* pInput_selectors,
		const uint32_t* pSelector_cluster_indices,
		uint32_t* pOutput_selector_cluster_indices, // one per block
		bool perceptual);

#pragma pack(push, 1)
	struct ds_param_struct
	{
		uint32_t m_total_blocks;
		int m_perceptual;
	};
#pragma pack(pop)

	bool opencl_determine_selectors(
		opencl_context_ptr pContext,
		const color_rgba* pInput_etc_color5_and_inten,
		etc_block* pOutput_blocks,
		bool perceptual);

} // namespace basisu

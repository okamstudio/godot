// basisu_frontend.h
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
#include "basisu_enc.h"
#include "basisu_etc.h"
#include "basisu_gpu_texture.h"
#include "../transcoder/basisu_file_headers.h"
#include "../transcoder/basisu_transcoder.h"

namespace basisu
{
	struct opencl_context;
	typedef opencl_context* opencl_context_ptr;

	struct vec2U
	{
		uint32_t m_comps[2];

		vec2U() { }
		vec2U(uint32_t a, uint32_t b) { set(a, b); }

		void set(uint32_t a, uint32_t b) { m_comps[0] = a; m_comps[1] = b; }

		uint32_t operator[] (uint32_t i) const { assert(i < 2); return m_comps[i]; }
		uint32_t &operator[] (uint32_t i) { assert(i < 2); return m_comps[i]; }
	};

	const uint32_t BASISU_DEFAULT_COMPRESSION_LEVEL = 2;
	const uint32_t BASISU_MAX_COMPRESSION_LEVEL = 6;

	class basisu_frontend
	{
		BASISU_NO_EQUALS_OR_COPY_CONSTRUCT(basisu_frontend);

	public:

		basisu_frontend() :
			m_total_blocks(0),
			m_total_pixels(0),
			m_endpoint_refinement(false),
			m_use_hierarchical_endpoint_codebooks(false),
			m_use_hierarchical_selector_codebooks(false),
			m_num_endpoint_codebook_iterations(0),
			m_num_selector_codebook_iterations(0),
			m_opencl_failed(false)
		{
		}

		enum
		{
			cMaxEndpointClusters = 16128,
						
			cMaxSelectorClusters = 16128,
		};

		struct params
		{
			params() :
				m_num_source_blocks(0),
				m_pSource_blocks(NULL),
				m_max_endpoint_clusters(256),
				m_max_selector_clusters(256),
				m_compression_level(BASISU_DEFAULT_COMPRESSION_LEVEL),
				m_perceptual(true),
				m_debug_stats(false),
				m_debug_images(false),
				m_dump_endpoint_clusterization(true),
				m_validate(false),
				m_multithreaded(false),
				m_disable_hierarchical_endpoint_codebooks(false),
				m_tex_type(basist::cBASISTexType2D),
				m_pOpenCL_context(nullptr),
				m_pJob_pool(nullptr)
			{
			}

			uint32_t m_num_source_blocks;
			pixel_block *m_pSource_blocks;

			uint32_t m_max_endpoint_clusters;
			uint32_t m_max_selector_clusters;

			uint32_t m_compression_level;

			bool m_perceptual;
			bool m_debug_stats;
			bool m_debug_images;
			bool m_dump_endpoint_clusterization;
			bool m_validate;
			bool m_multithreaded;
			bool m_disable_hierarchical_endpoint_codebooks;
			
			basist::basis_texture_type m_tex_type;
			const basist::basisu_lowlevel_etc1s_transcoder *m_pGlobal_codebooks;
						
			opencl_context_ptr m_pOpenCL_context;
			
			job_pool *m_pJob_pool;
		};

		bool init(const params &p);

		bool compress();

		const params &get_params() const { return m_params; }

		const pixel_block &get_source_pixel_block(uint32_t i) const { return m_source_blocks[i]; }

		// RDO output blocks
		uint32_t get_total_output_blocks() const { return static_cast<uint32_t>(m_encoded_blocks.size()); }

		const etc_block &get_output_block(uint32_t block_index) const { return m_encoded_blocks[block_index]; }
		const etc_block_vec &get_output_blocks() const { return m_encoded_blocks; }

		// "Best" ETC1S blocks
		const etc_block &get_etc1s_block(uint32_t block_index) const { return m_etc1_blocks_etc1s[block_index]; }

		// Per-block flags
		bool get_diff_flag(uint32_t block_index) const { return m_encoded_blocks[block_index].get_diff_bit(); }

		// Endpoint clusters
		uint32_t get_total_endpoint_clusters() const { return static_cast<uint32_t>(m_endpoint_clusters.size()); }
		uint32_t get_subblock_endpoint_cluster_index(uint32_t block_index, uint32_t subblock_index) const { return m_block_endpoint_clusters_indices[block_index][subblock_index]; }

		const color_rgba &get_endpoint_cluster_unscaled_color(uint32_t cluster_index, bool individual_mode) const { return m_endpoint_cluster_etc_params[cluster_index].m_color_unscaled[individual_mode]; }
		uint32_t get_endpoint_cluster_inten_table(uint32_t cluster_index, bool individual_mode) const { return m_endpoint_cluster_etc_params[cluster_index].m_inten_table[individual_mode]; }

		bool get_endpoint_cluster_color_is_used(uint32_t cluster_index, bool individual_mode) const { return m_endpoint_cluster_etc_params[cluster_index].m_color_used[individual_mode]; }

		// Selector clusters
		uint32_t get_total_selector_clusters() const { return static_cast<uint32_t>(m_selector_cluster_block_indices.size()); }
		uint32_t get_block_selector_cluster_index(uint32_t block_index) const { return m_block_selector_cluster_index[block_index]; }
		const etc_block &get_selector_cluster_selector_bits(uint32_t cluster_index) const { return m_optimized_cluster_selectors[cluster_index]; }
				
		// Returns block indices using each selector cluster
		const uint_vec &get_selector_cluster_block_indices(uint32_t selector_cluster_index) const { return m_selector_cluster_block_indices[selector_cluster_index]; }

		void dump_debug_image(const char *pFilename, uint32_t first_block, uint32_t num_blocks_x, uint32_t num_blocks_y, bool output_blocks);
		
		void reoptimize_remapped_endpoints(const uint_vec &new_block_endpoints, int_vec &old_to_new_endpoint_cluster_indices, bool optimize_final_codebook, uint_vec *pBlock_selector_indices = nullptr);

		bool get_opencl_failed() const { return m_opencl_failed; }

	private:
		params m_params;
		uint32_t m_total_blocks;
		uint32_t m_total_pixels;

		bool m_endpoint_refinement;
		bool m_use_hierarchical_endpoint_codebooks;
		bool m_use_hierarchical_selector_codebooks;

		uint32_t m_num_endpoint_codebook_iterations;
		uint32_t m_num_selector_codebook_iterations;

		// Source pixels for each blocks
		pixel_block_vec m_source_blocks;

		// The quantized ETC1S texture.
		etc_block_vec m_encoded_blocks;
		
		// Quantized blocks after endpoint quant, but before selector quant
		etc_block_vec m_orig_encoded_blocks; 
				
		// Full quality ETC1S texture
		etc_block_vec m_etc1_blocks_etc1s;
				
		typedef vec<6, float> vec6F;
		
		// Endpoint clusterizer
		typedef tree_vector_quant<vec6F> vec6F_quantizer;
		vec6F_quantizer m_endpoint_clusterizer;

		// For each endpoint cluster: An array of which subblock indices (block_index*2+subblock) are located in that cluster.
		basisu::vector<uint_vec> m_endpoint_clusters;

		// Array of subblock indices for each parent endpoint cluster
		// Note: Initially, each endpoint cluster will only live in a single parent cluster, in a shallow tree. 
		// As the endpoint clusters are manipulated this constraint gets broken.
		basisu::vector<uint_vec> m_endpoint_parent_clusters;
		
		// Each block's parent endpoint cluster index
		uint8_vec m_block_parent_endpoint_cluster; 

		// Array of endpoint cluster indices for each parent endpoint cluster
		basisu::vector<uint_vec> m_endpoint_clusters_within_each_parent_cluster;
				
		struct endpoint_cluster_etc_params
		{
			endpoint_cluster_etc_params()
			{
				clear();
			}

			void clear()
			{
				clear_obj(m_color_unscaled);
				clear_obj(m_inten_table);
				clear_obj(m_color_error);
				m_subblocks.clear();

				clear_obj(m_color_used);
				m_valid = false;
			}

			// TODO: basisu doesn't use individual mode.
			color_rgba m_color_unscaled[2]; // [use_individual_mode]
			uint32_t m_inten_table[2];

			uint64_t m_color_error[2];

			uint_vec m_subblocks;

			bool m_color_used[2];

			bool m_valid;

			bool operator== (const endpoint_cluster_etc_params &other) const
			{
				for (uint32_t i = 0; i < 2; i++)
				{
					if (m_color_unscaled[i] != other.m_color_unscaled[i])
						return false;
				}

				if (m_inten_table[0] != other.m_inten_table[0])
					return false;
				if (m_inten_table[1] != other.m_inten_table[1])
					return false;

				return true;
			}

			bool operator< (const endpoint_cluster_etc_params &other) const
			{
				for (uint32_t i = 0; i < 2; i++)
				{
					if (m_color_unscaled[i] < other.m_color_unscaled[i])
						return true;
					else if (m_color_unscaled[i] != other.m_color_unscaled[i])
						return false;
				}

				if (m_inten_table[0] < other.m_inten_table[0])
					return true;
				else if (m_inten_table[0] == other.m_inten_table[0])
				{
					if (m_inten_table[1] < other.m_inten_table[1])
						return true;
				}

				return false;
			}
		};

		typedef basisu::vector<endpoint_cluster_etc_params> cluster_subblock_etc_params_vec;
		
		// Each endpoint cluster's ETC1S parameters 
		cluster_subblock_etc_params_vec m_endpoint_cluster_etc_params;

		// The endpoint cluster index used by each ETC1 subblock.
		basisu::vector<vec2U> m_block_endpoint_clusters_indices;
				
		// The block(s) within each selector cluster
		// Note: If you add anything here that uses selector cluster indicies, be sure to update optimize_selector_codebook()!
		basisu::vector<uint_vec> m_selector_cluster_block_indices;

		// The selector bits for each selector cluster.
		basisu::vector<etc_block> m_optimized_cluster_selectors;

		// The block(s) within each parent selector cluster.
		basisu::vector<uint_vec> m_selector_parent_cluster_block_indices;
		
		// Each block's parent selector cluster
		uint8_vec m_block_parent_selector_cluster;

		// Array of selector cluster indices for each parent selector cluster
		basisu::vector<uint_vec> m_selector_clusters_within_each_parent_cluster;
				
		// Each block's selector cluster index
		basisu::vector<uint32_t> m_block_selector_cluster_index;

		struct subblock_endpoint_quant_err
		{
			uint64_t m_total_err;
			uint32_t m_cluster_index;
			uint32_t m_cluster_subblock_index;
			uint32_t m_block_index;
			uint32_t m_subblock_index;

			bool operator< (const subblock_endpoint_quant_err &rhs) const
			{
				if (m_total_err < rhs.m_total_err)
					return true;
				else if (m_total_err == rhs.m_total_err)
				{
					if (m_block_index < rhs.m_block_index)
						return true;
					else if (m_block_index == rhs.m_block_index)
						return m_subblock_index < rhs.m_subblock_index;
				}
				return false;
			}
		};

		// The sorted subblock endpoint quant error for each endpoint cluster
		basisu::vector<subblock_endpoint_quant_err> m_subblock_endpoint_quant_err_vec;

		std::mutex m_lock;

		bool m_opencl_failed;

		//-----------------------------------------------------------------------------

		void init_etc1_images();
		bool init_global_codebooks();
		void init_endpoint_training_vectors();
		void dump_endpoint_clusterization_visualization(const char *pFilename, bool vis_endpoint_colors);
		void generate_endpoint_clusters();
		void compute_endpoint_subblock_error_vec();
		void introduce_new_endpoint_clusters();
		void generate_endpoint_codebook(uint32_t step);
		uint32_t refine_endpoint_clusterization();
		void eliminate_redundant_or_empty_endpoint_clusters();
		void generate_block_endpoint_clusters();
		void compute_endpoint_clusters_within_each_parent_cluster();
		void compute_selector_clusters_within_each_parent_cluster();
		void create_initial_packed_texture();
		void generate_selector_clusters();
		void create_optimized_selector_codebook(uint32_t iter);
		void find_optimal_selector_clusters_for_each_block();
		uint32_t refine_block_endpoints_given_selectors();
		void finalize();
		bool validate_endpoint_cluster_hierarchy(bool ensure_clusters_have_same_parents) const;
		bool validate_output() const;
		void introduce_special_selector_clusters();
		void optimize_selector_codebook();
		bool check_etc1s_constraints() const;
	};

} // namespace basisu

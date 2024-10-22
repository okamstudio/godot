// basisu_backend.cpp
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
//
// TODO: This code originally supported full ETC1 and ETC1S, so there's some legacy stuff in here.
//
#include "basisu_backend.h"

#if BASISU_SUPPORT_SSE
#define CPPSPMD_NAME(a) a##_sse41
#include "basisu_kernels_declares.h"
#endif

#define BASISU_FASTER_SELECTOR_REORDERING 0
#define BASISU_BACKEND_VERIFY(c) verify(c, __LINE__);

namespace basisu
{
	// TODO
	static inline void verify(bool condition, int line)
	{
		if (!condition)
		{
			fprintf(stderr, "ERROR: basisu_backend: verify() failed at line %i!\n", line);
			abort();
		}
	}

	basisu_backend::basisu_backend()
	{
		clear();
	}

	void basisu_backend::clear()
	{
		m_pFront_end = NULL;
		m_params.clear();
		m_output.clear();
	}

	void basisu_backend::init(basisu_frontend* pFront_end, basisu_backend_params& params, const basisu_backend_slice_desc_vec& slice_descs)
	{
		m_pFront_end = pFront_end;
		m_params = params;
		m_slices = slice_descs;
		
		debug_printf("basisu_backend::Init: Slices: %u, ETC1S: %u, EndpointRDOQualityThresh: %f, SelectorRDOQualityThresh: %f\n",
			m_slices.size(),
			params.m_etc1s,
			params.m_endpoint_rdo_quality_thresh,
			params.m_selector_rdo_quality_thresh);

		debug_printf("Frontend endpoints: %u selectors: %u\n", m_pFront_end->get_total_endpoint_clusters(), m_pFront_end->get_total_selector_clusters());

		for (uint32_t i = 0; i < m_slices.size(); i++)
		{
			debug_printf("Slice: %u, OrigWidth: %u, OrigHeight: %u, Width: %u, Height: %u, NumBlocksX: %u, NumBlocksY: %u, FirstBlockIndex: %u\n",
				i,
				m_slices[i].m_orig_width, m_slices[i].m_orig_height,
				m_slices[i].m_width, m_slices[i].m_height,
				m_slices[i].m_num_blocks_x, m_slices[i].m_num_blocks_y,
				m_slices[i].m_first_block_index);
		}
	}

	void basisu_backend::create_endpoint_palette()
	{
		const basisu_frontend& r = *m_pFront_end;

		m_output.m_num_endpoints = r.get_total_endpoint_clusters();

		m_endpoint_palette.resize(r.get_total_endpoint_clusters());
		for (uint32_t i = 0; i < r.get_total_endpoint_clusters(); i++)
		{
			etc1_endpoint_palette_entry& e = m_endpoint_palette[i];

			e.m_color5_valid = r.get_endpoint_cluster_color_is_used(i, false);
			e.m_color5 = r.get_endpoint_cluster_unscaled_color(i, false);
			e.m_inten5 = r.get_endpoint_cluster_inten_table(i, false);

			BASISU_BACKEND_VERIFY(e.m_color5_valid);
		}
	}

	void basisu_backend::create_selector_palette()
	{
		const basisu_frontend& r = *m_pFront_end;

		m_output.m_num_selectors = r.get_total_selector_clusters();

		m_selector_palette.resize(r.get_total_selector_clusters());

		for (uint32_t i = 0; i < r.get_total_selector_clusters(); i++)
		{
			etc1_selector_palette_entry& s = m_selector_palette[i];

			const etc_block& selector_bits = r.get_selector_cluster_selector_bits(i);

			for (uint32_t y = 0; y < 4; y++)
			{
				for (uint32_t x = 0; x < 4; x++)
				{
					s[y * 4 + x] = static_cast<uint8_t>(selector_bits.get_selector(x, y));
				}
			}
		}
	}

	static const struct
	{
		int8_t m_dx, m_dy;
	} g_endpoint_preds[] =
	{
		{ -1, 0 },
		{ 0, -1 },
		{ -1, -1 }
	};

	void basisu_backend::reoptimize_and_sort_endpoints_codebook(uint32_t total_block_endpoints_remapped, uint_vec& all_endpoint_indices)
	{
		basisu_frontend& r = *m_pFront_end;
		//const bool is_video = r.get_params().m_tex_type == basist::cBASISTexTypeVideoFrames;

		if (m_params.m_used_global_codebooks)
		{
			m_endpoint_remap_table_old_to_new.clear();
			m_endpoint_remap_table_old_to_new.resize(r.get_total_endpoint_clusters());
			for (uint32_t i = 0; i < r.get_total_endpoint_clusters(); i++)
				m_endpoint_remap_table_old_to_new[i] = i;
		}
		else
		{
			//if ((total_block_endpoints_remapped) && (m_params.m_compression_level > 0))
			if ((total_block_endpoints_remapped) && (m_params.m_compression_level > 1))
			{
				// We've changed the block endpoint indices, so we need to go and adjust the endpoint codebook (remove unused entries, optimize existing entries that have changed)
				uint_vec new_block_endpoints(get_total_blocks());

				for (uint32_t slice_index = 0; slice_index < m_slices.size(); slice_index++)
				{
					const uint32_t first_block_index = m_slices[slice_index].m_first_block_index;
					const uint32_t num_blocks_x = m_slices[slice_index].m_num_blocks_x;
					const uint32_t num_blocks_y = m_slices[slice_index].m_num_blocks_y;

					for (uint32_t block_y = 0; block_y < num_blocks_y; block_y++)
						for (uint32_t block_x = 0; block_x < num_blocks_x; block_x++)
							new_block_endpoints[first_block_index + block_x + block_y * num_blocks_x] = m_slice_encoder_blocks[slice_index](block_x, block_y).m_endpoint_index;
				}

				int_vec old_to_new_endpoint_indices;
				r.reoptimize_remapped_endpoints(new_block_endpoints, old_to_new_endpoint_indices, true);

				create_endpoint_palette();

				for (uint32_t slice_index = 0; slice_index < m_slices.size(); slice_index++)
				{
					//const uint32_t first_block_index = m_slices[slice_index].m_first_block_index;

					//const uint32_t width = m_slices[slice_index].m_width;
					//const uint32_t height = m_slices[slice_index].m_height;
					const uint32_t num_blocks_x = m_slices[slice_index].m_num_blocks_x;
					const uint32_t num_blocks_y = m_slices[slice_index].m_num_blocks_y;

					for (uint32_t block_y = 0; block_y < num_blocks_y; block_y++)
					{
						for (uint32_t block_x = 0; block_x < num_blocks_x; block_x++)
						{
							//const uint32_t block_index = first_block_index + block_x + block_y * num_blocks_x;

							encoder_block& m = m_slice_encoder_blocks[slice_index](block_x, block_y);

							m.m_endpoint_index = old_to_new_endpoint_indices[m.m_endpoint_index];
						} // block_x
					} // block_y
				} // slice_index

				for (uint32_t i = 0; i < all_endpoint_indices.size(); i++)
					all_endpoint_indices[i] = old_to_new_endpoint_indices[all_endpoint_indices[i]];

			} //if (total_block_endpoints_remapped)

			// Sort endpoint codebook
			palette_index_reorderer reorderer;
			reorderer.init((uint32_t)all_endpoint_indices.size(), &all_endpoint_indices[0], r.get_total_endpoint_clusters(), nullptr, nullptr, 0);
			m_endpoint_remap_table_old_to_new = reorderer.get_remap_table();
		}

		// For endpoints, old_to_new[] may not be bijective! 
		// Some "old" entries may be unused and don't get remapped into the "new" array.

		m_old_endpoint_was_used.clear();
		m_old_endpoint_was_used.resize(r.get_total_endpoint_clusters());
		uint32_t first_old_entry_index = UINT32_MAX;

		for (uint32_t slice_index = 0; slice_index < m_slices.size(); slice_index++)
		{
			const uint32_t num_blocks_x = m_slices[slice_index].m_num_blocks_x, num_blocks_y = m_slices[slice_index].m_num_blocks_y;
			for (uint32_t block_y = 0; block_y < num_blocks_y; block_y++)
			{
				for (uint32_t block_x = 0; block_x < num_blocks_x; block_x++)
				{
					encoder_block& m = m_slice_encoder_blocks[slice_index](block_x, block_y);
					const uint32_t old_endpoint_index = m.m_endpoint_index;

					m_old_endpoint_was_used[old_endpoint_index] = true;
					first_old_entry_index = basisu::minimum(first_old_entry_index, old_endpoint_index);
				} // block_x
			} // block_y
		} // slice_index

		debug_printf("basisu_backend::reoptimize_and_sort_endpoints_codebook: First old entry index: %u\n", first_old_entry_index);
						
		m_new_endpoint_was_used.clear();
		m_new_endpoint_was_used.resize(r.get_total_endpoint_clusters());

		m_endpoint_remap_table_new_to_old.clear();
		m_endpoint_remap_table_new_to_old.resize(r.get_total_endpoint_clusters());
		
		// Set unused entries in the new array to point to the first used entry in the old array.
		m_endpoint_remap_table_new_to_old.set_all(first_old_entry_index);

		for (uint32_t old_index = 0; old_index < m_endpoint_remap_table_old_to_new.size(); old_index++)
		{
			if (m_old_endpoint_was_used[old_index])
			{
				const uint32_t new_index = m_endpoint_remap_table_old_to_new[old_index];
				
				m_new_endpoint_was_used[new_index] = true;

				m_endpoint_remap_table_new_to_old[new_index] = old_index;
			}
		}
	}

	void basisu_backend::sort_selector_codebook()
	{
		basisu_frontend& r = *m_pFront_end;

		m_selector_remap_table_new_to_old.resize(r.get_total_selector_clusters());

		if ((m_params.m_compression_level == 0) || (m_params.m_used_global_codebooks))
		{
			for (uint32_t i = 0; i < r.get_total_selector_clusters(); i++)
				m_selector_remap_table_new_to_old[i] = i;
		}
		else
		{
			m_selector_remap_table_new_to_old[0] = 0;
			uint32_t prev_selector_index = 0;

			int_vec remaining_selectors;
			remaining_selectors.reserve(r.get_total_selector_clusters() - 1);
			for (uint32_t i = 1; i < r.get_total_selector_clusters(); i++)
				remaining_selectors.push_back(i);

			uint_vec selector_palette_bytes(m_selector_palette.size());
			for (uint32_t i = 0; i < m_selector_palette.size(); i++)
				selector_palette_bytes[i] = m_selector_palette[i].get_byte(0) | (m_selector_palette[i].get_byte(1) << 8) | (m_selector_palette[i].get_byte(2) << 16) | (m_selector_palette[i].get_byte(3) << 24);

			// This is the traveling salesman problem.
			for (uint32_t i = 1; i < r.get_total_selector_clusters(); i++)
			{
				uint32_t best_hamming_dist = 100;
				uint32_t best_index = 0;

#if BASISU_FASTER_SELECTOR_REORDERING
				const uint32_t step = (remaining_selectors.size() > 16) ? 16 : 1;
				for (uint32_t j = 0; j < remaining_selectors.size(); j += step)
#else
				for (uint32_t j = 0; j < remaining_selectors.size(); j++)
#endif
				{
					int selector_index = remaining_selectors[j];

					uint32_t k = selector_palette_bytes[prev_selector_index] ^ selector_palette_bytes[selector_index];
					uint32_t hamming_dist = g_hamming_dist[k & 0xFF] + g_hamming_dist[(k >> 8) & 0xFF] + g_hamming_dist[(k >> 16) & 0xFF] + g_hamming_dist[k >> 24];

					if (hamming_dist < best_hamming_dist)
					{
						best_hamming_dist = hamming_dist;
						best_index = j;
						if (best_hamming_dist <= 1)
							break;
					}
				}

				prev_selector_index = remaining_selectors[best_index];
				m_selector_remap_table_new_to_old[i] = prev_selector_index;

				remaining_selectors[best_index] = remaining_selectors.back();
				remaining_selectors.resize(remaining_selectors.size() - 1);
			}
		}

		m_selector_remap_table_old_to_new.resize(r.get_total_selector_clusters());
		for (uint32_t i = 0; i < m_selector_remap_table_new_to_old.size(); i++)
			m_selector_remap_table_old_to_new[m_selector_remap_table_new_to_old[i]] = i;
	}
	int basisu_backend::find_video_frame(int slice_index, int delta)
	{
		for (uint32_t s = 0; s < m_slices.size(); s++)
		{
			if ((int)m_slices[s].m_source_file_index != ((int)m_slices[slice_index].m_source_file_index + delta))
				continue;
			if (m_slices[s].m_mip_index != m_slices[slice_index].m_mip_index)
				continue;

			// Being super paranoid here.
			if (m_slices[s].m_num_blocks_x != (m_slices[slice_index].m_num_blocks_x))
				continue;
			if (m_slices[s].m_num_blocks_y != (m_slices[slice_index].m_num_blocks_y))
				continue;
			if (m_slices[s].m_alpha != (m_slices[slice_index].m_alpha))
				continue;
			return s;
		}

		return -1;
	}

	void basisu_backend::check_for_valid_cr_blocks()
	{
		basisu_frontend& r = *m_pFront_end;
		const bool is_video = r.get_params().m_tex_type == basist::cBASISTexTypeVideoFrames;

		if (!is_video)
			return;

		debug_printf("basisu_backend::check_for_valid_cr_blocks\n");

		uint32_t total_crs = 0;
		uint32_t total_invalid_crs = 0;

		for (uint32_t slice_index = 0; slice_index < m_slices.size(); slice_index++)
		{
			const bool is_iframe = m_slices[slice_index].m_iframe;
			//const uint32_t first_block_index = m_slices[slice_index].m_first_block_index;

			//const uint32_t width = m_slices[slice_index].m_width;
			//const uint32_t height = m_slices[slice_index].m_height;
			const uint32_t num_blocks_x = m_slices[slice_index].m_num_blocks_x;
			const uint32_t num_blocks_y = m_slices[slice_index].m_num_blocks_y;
			const int prev_frame_slice_index = find_video_frame(slice_index, -1);

			// If we don't have a previous frame, and we're not an i-frame, something is wrong.
			if ((prev_frame_slice_index < 0) && (!is_iframe))
			{
				BASISU_BACKEND_VERIFY(0);
			}

			if ((is_iframe) || (prev_frame_slice_index < 0))
			{
				// Ensure no blocks use CR's
				for (uint32_t block_y = 0; block_y < num_blocks_y; block_y++)
				{
					for (uint32_t block_x = 0; block_x < num_blocks_x; block_x++)
					{
						encoder_block& m = m_slice_encoder_blocks[slice_index](block_x, block_y);
						BASISU_BACKEND_VERIFY(m.m_endpoint_predictor != basist::CR_ENDPOINT_PRED_INDEX);
					}
				}
			}
			else
			{
				// For blocks that use CR's, make sure the endpoints/selectors haven't really changed.
				for (uint32_t block_y = 0; block_y < num_blocks_y; block_y++)
				{
					for (uint32_t block_x = 0; block_x < num_blocks_x; block_x++)
					{
						encoder_block& m = m_slice_encoder_blocks[slice_index](block_x, block_y);

						if (m.m_endpoint_predictor == basist::CR_ENDPOINT_PRED_INDEX)
						{
							total_crs++;

							encoder_block& prev_m = m_slice_encoder_blocks[prev_frame_slice_index](block_x, block_y);

							if ((m.m_endpoint_index != prev_m.m_endpoint_index) || (m.m_selector_index != prev_m.m_selector_index))
							{
								total_invalid_crs++;
							}
						}
					} // block_x
				} // block_y

			} // !slice_index

		} // slice_index

		debug_printf("Total CR's: %u, Total invalid CR's: %u\n", total_crs, total_invalid_crs);

		BASISU_BACKEND_VERIFY(total_invalid_crs == 0);
	}

	void basisu_backend::create_encoder_blocks()
	{
		debug_printf("basisu_backend::create_encoder_blocks\n");

		interval_timer tm;
		tm.start();

		basisu_frontend& r = *m_pFront_end;
		const bool is_video = r.get_params().m_tex_type == basist::cBASISTexTypeVideoFrames;

		m_slice_encoder_blocks.resize(m_slices.size());

		uint32_t total_endpoint_pred_missed = 0, total_endpoint_pred_hits = 0, total_block_endpoints_remapped = 0;

		uint_vec all_endpoint_indices;
		all_endpoint_indices.reserve(get_total_blocks());

		for (uint32_t slice_index = 0; slice_index < m_slices.size(); slice_index++)
		{
			const int prev_frame_slice_index = is_video ? find_video_frame(slice_index, -1) : -1;
			const bool is_iframe = m_slices[slice_index].m_iframe;
			const uint32_t first_block_index = m_slices[slice_index].m_first_block_index;

			//const uint32_t width = m_slices[slice_index].m_width;
			//const uint32_t height = m_slices[slice_index].m_height;
			const uint32_t num_blocks_x = m_slices[slice_index].m_num_blocks_x;
			const uint32_t num_blocks_y = m_slices[slice_index].m_num_blocks_y;

			m_slice_encoder_blocks[slice_index].resize(num_blocks_x, num_blocks_y);

			for (uint32_t block_y = 0; block_y < num_blocks_y; block_y++)
			{
				for (uint32_t block_x = 0; block_x < num_blocks_x; block_x++)
				{
					const uint32_t block_index = first_block_index + block_x + block_y * num_blocks_x;

					encoder_block& m = m_slice_encoder_blocks[slice_index](block_x, block_y);

					m.m_endpoint_index = r.get_subblock_endpoint_cluster_index(block_index, 0);
					BASISU_BACKEND_VERIFY(r.get_subblock_endpoint_cluster_index(block_index, 0) == r.get_subblock_endpoint_cluster_index(block_index, 1));

					m.m_selector_index = r.get_block_selector_cluster_index(block_index);

					m.m_endpoint_predictor = basist::NO_ENDPOINT_PRED_INDEX;

					const uint32_t block_endpoint = m.m_endpoint_index;

					uint32_t best_endpoint_pred = UINT32_MAX;

					for (uint32_t endpoint_pred = 0; endpoint_pred < basist::NUM_ENDPOINT_PREDS; endpoint_pred++)
					{
						if ((is_video) && (endpoint_pred == basist::CR_ENDPOINT_PRED_INDEX))
						{
							if ((prev_frame_slice_index != -1) && (!is_iframe))
							{
								const uint32_t cur_endpoint = m_slice_encoder_blocks[slice_index](block_x, block_y).m_endpoint_index;
								const uint32_t cur_selector = m_slice_encoder_blocks[slice_index](block_x, block_y).m_selector_index;
								const uint32_t prev_endpoint = m_slice_encoder_blocks[prev_frame_slice_index](block_x, block_y).m_endpoint_index;
								const uint32_t prev_selector = m_slice_encoder_blocks[prev_frame_slice_index](block_x, block_y).m_selector_index;
								if ((cur_endpoint == prev_endpoint) && (cur_selector == prev_selector))
								{
									best_endpoint_pred = basist::CR_ENDPOINT_PRED_INDEX;
									m_slice_encoder_blocks[prev_frame_slice_index](block_x, block_y).m_is_cr_target = true;
								}
							}
						}
						else
						{
							int pred_block_x = block_x + g_endpoint_preds[endpoint_pred].m_dx;
							if ((pred_block_x < 0) || (pred_block_x >= (int)num_blocks_x))
								continue;

							int pred_block_y = block_y + g_endpoint_preds[endpoint_pred].m_dy;
							if ((pred_block_y < 0) || (pred_block_y >= (int)num_blocks_y))
								continue;

							uint32_t pred_endpoint = m_slice_encoder_blocks[slice_index](pred_block_x, pred_block_y).m_endpoint_index;

							if (pred_endpoint == block_endpoint)
							{
								if (endpoint_pred < best_endpoint_pred)
								{
									best_endpoint_pred = endpoint_pred;
								}
							}
						}

					} // endpoint_pred

					if (best_endpoint_pred != UINT32_MAX)
					{
						m.m_endpoint_predictor = best_endpoint_pred;

						total_endpoint_pred_hits++;
					}
					else if (m_params.m_endpoint_rdo_quality_thresh > 0.0f)
					{
						const pixel_block& src_pixels = r.get_source_pixel_block(block_index);

						etc_block etc_blk(r.get_output_block(block_index));

						uint64_t cur_err = etc_blk.evaluate_etc1_error(src_pixels.get_ptr(), r.get_params().m_perceptual);

						if (cur_err)
						{
							const uint64_t thresh_err = (uint64_t)(cur_err * maximum(1.0f, m_params.m_endpoint_rdo_quality_thresh));

							etc_block trial_etc_block(etc_blk);

							uint64_t best_err = UINT64_MAX;
							uint32_t best_endpoint_index = 0;

							best_endpoint_pred = UINT32_MAX;

							for (uint32_t endpoint_pred = 0; endpoint_pred < basist::NUM_ENDPOINT_PREDS; endpoint_pred++)
							{
								if ((is_video) && (endpoint_pred == basist::CR_ENDPOINT_PRED_INDEX))
									continue;

								int pred_block_x = block_x + g_endpoint_preds[endpoint_pred].m_dx;
								if ((pred_block_x < 0) || (pred_block_x >= (int)num_blocks_x))
									continue;

								int pred_block_y = block_y + g_endpoint_preds[endpoint_pred].m_dy;
								if ((pred_block_y < 0) || (pred_block_y >= (int)num_blocks_y))
									continue;

								uint32_t pred_endpoint_index = m_slice_encoder_blocks[slice_index](pred_block_x, pred_block_y).m_endpoint_index;

								uint32_t pred_inten = r.get_endpoint_cluster_inten_table(pred_endpoint_index, false);
								color_rgba pred_color = r.get_endpoint_cluster_unscaled_color(pred_endpoint_index, false);

								trial_etc_block.set_block_color5(pred_color, pred_color);
								trial_etc_block.set_inten_table(0, pred_inten);
								trial_etc_block.set_inten_table(1, pred_inten);

								color_rgba trial_colors[16];
								unpack_etc1(trial_etc_block, trial_colors);

								uint64_t trial_err = 0;
								if (r.get_params().m_perceptual)
								{
									for (uint32_t p = 0; p < 16; p++)
									{
										trial_err += color_distance(true, src_pixels.get_ptr()[p], trial_colors[p], false);
										if (trial_err > thresh_err)
											break;
									}
								}
								else
								{
									for (uint32_t p = 0; p < 16; p++)
									{
										trial_err += color_distance(false, src_pixels.get_ptr()[p], trial_colors[p], false);
										if (trial_err > thresh_err)
											break;
									}
								}

								if (trial_err <= thresh_err)
								{
									if ((trial_err < best_err) || ((trial_err == best_err) && (endpoint_pred < best_endpoint_pred)))
									{
										best_endpoint_pred = endpoint_pred;
										best_err = trial_err;
										best_endpoint_index = pred_endpoint_index;
									}
								}
							} // endpoint_pred

							if (best_endpoint_pred != UINT32_MAX)
							{
								m.m_endpoint_index = best_endpoint_index;
								m.m_endpoint_predictor = best_endpoint_pred;

								total_endpoint_pred_hits++;
								total_block_endpoints_remapped++;
							}
							else
							{
								total_endpoint_pred_missed++;
							}
						}
					}
					else
					{
						total_endpoint_pred_missed++;
					}

					if (m.m_endpoint_predictor == basist::NO_ENDPOINT_PRED_INDEX)
					{
						all_endpoint_indices.push_back(m.m_endpoint_index);
					}

				} // block_x

			} // block_y

		} // slice

		debug_printf("total_endpoint_pred_missed: %u (%3.2f%%) total_endpoint_pred_hit: %u (%3.2f%%), total_block_endpoints_remapped: %u (%3.2f%%)\n",
			total_endpoint_pred_missed, total_endpoint_pred_missed * 100.0f / get_total_blocks(),
			total_endpoint_pred_hits, total_endpoint_pred_hits * 100.0f / get_total_blocks(),
			total_block_endpoints_remapped, total_block_endpoints_remapped * 100.0f / get_total_blocks());

		reoptimize_and_sort_endpoints_codebook(total_block_endpoints_remapped, all_endpoint_indices);

		sort_selector_codebook();
		check_for_valid_cr_blocks();
		
		debug_printf("Elapsed time: %3.3f secs\n", tm.get_elapsed_secs());
	}

	void basisu_backend::compute_slice_crcs()
	{
		for (uint32_t slice_index = 0; slice_index < m_slices.size(); slice_index++)
		{
			//const uint32_t first_block_index = m_slices[slice_index].m_first_block_index;
			const uint32_t width = m_slices[slice_index].m_width;
			const uint32_t height = m_slices[slice_index].m_height;
			const uint32_t num_blocks_x = m_slices[slice_index].m_num_blocks_x;
			const uint32_t num_blocks_y = m_slices[slice_index].m_num_blocks_y;

			gpu_image gi;
			gi.init(texture_format::cETC1, width, height);

			for (uint32_t block_y = 0; block_y < num_blocks_y; block_y++)
			{
				for (uint32_t block_x = 0; block_x < num_blocks_x; block_x++)
				{
					//const uint32_t block_index = first_block_index + block_x + block_y * num_blocks_x;

					encoder_block& m = m_slice_encoder_blocks[slice_index](block_x, block_y);

					{
						etc_block& output_block = *(etc_block*)gi.get_block_ptr(block_x, block_y);

						output_block.set_diff_bit(true);
						// Setting the flip bit to false to be compatible with the Khronos KDFS.
						//output_block.set_flip_bit(true);
						output_block.set_flip_bit(false);

						const uint32_t endpoint_index = m.m_endpoint_index;

						output_block.set_block_color5_etc1s(m_endpoint_palette[endpoint_index].m_color5);
						output_block.set_inten_tables_etc1s(m_endpoint_palette[endpoint_index].m_inten5);

						const uint32_t selector_idx = m.m_selector_index;

						const etc1_selector_palette_entry& selectors = m_selector_palette[selector_idx];
						for (uint32_t sy = 0; sy < 4; sy++)
							for (uint32_t sx = 0; sx < 4; sx++)
								output_block.set_selector(sx, sy, selectors(sx, sy));
					}

				} // block_x
			} // block_y

			m_output.m_slice_image_crcs[slice_index] = basist::crc16(gi.get_ptr(), gi.get_size_in_bytes(), 0);

			if (m_params.m_debug_images)
			{
				image gi_unpacked;
				gi.unpack(gi_unpacked);

				char buf[256];
#ifdef _WIN32				
				sprintf_s(buf, sizeof(buf), "basisu_backend_slice_%u.png", slice_index);
#else
				snprintf(buf, sizeof(buf), "basisu_backend_slice_%u.png", slice_index);
#endif				
				save_png(buf, gi_unpacked);
			}

		} // slice_index
	}

	//uint32_t g_color_delta_hist[255 * 3 + 1];
	//uint32_t g_color_delta_bad_hist[255 * 3 + 1];
		
	// TODO: Split this into multiple methods.
	bool basisu_backend::encode_image()
	{
		basisu_frontend& r = *m_pFront_end;
		const bool is_video = r.get_params().m_tex_type == basist::cBASISTexTypeVideoFrames;

		uint32_t total_used_selector_history_buf = 0;
		uint32_t total_selector_indices_remapped = 0;

		basist::approx_move_to_front selector_history_buf(basist::MAX_SELECTOR_HISTORY_BUF_SIZE);
		histogram selector_history_buf_histogram(basist::MAX_SELECTOR_HISTORY_BUF_SIZE);
		histogram selector_histogram(r.get_total_selector_clusters() + basist::MAX_SELECTOR_HISTORY_BUF_SIZE + 1);
		histogram selector_history_buf_rle_histogram(1 << basist::SELECTOR_HISTORY_BUF_RLE_COUNT_BITS);

		basisu::vector<uint_vec> selector_syms(m_slices.size());

		const uint32_t SELECTOR_HISTORY_BUF_FIRST_SYMBOL_INDEX = r.get_total_selector_clusters();
		const uint32_t SELECTOR_HISTORY_BUF_RLE_SYMBOL_INDEX = SELECTOR_HISTORY_BUF_FIRST_SYMBOL_INDEX + basist::MAX_SELECTOR_HISTORY_BUF_SIZE;

		m_output.m_slice_image_crcs.resize(m_slices.size());

		histogram delta_endpoint_histogram(r.get_total_endpoint_clusters());

		histogram endpoint_pred_histogram(basist::ENDPOINT_PRED_TOTAL_SYMBOLS);
		basisu::vector<uint_vec> endpoint_pred_syms(m_slices.size());

		uint32_t total_endpoint_indices_remapped = 0;

		uint_vec block_endpoint_indices, block_selector_indices;

		interval_timer tm;
		tm.start();

		const int COLOR_DELTA_THRESH = 8;
		const int SEL_DIFF_THRESHOLD = 11;
		
		for (uint32_t slice_index = 0; slice_index < m_slices.size(); slice_index++)
		{
			//const int prev_frame_slice_index = is_video ? find_video_frame(slice_index, -1) : -1;
			//const int next_frame_slice_index = is_video ? find_video_frame(slice_index, 1) : -1;
			const uint32_t first_block_index = m_slices[slice_index].m_first_block_index;
			//const uint32_t width = m_slices[slice_index].m_width;
			//const uint32_t height = m_slices[slice_index].m_height;
			const uint32_t num_blocks_x = m_slices[slice_index].m_num_blocks_x;
			const uint32_t num_blocks_y = m_slices[slice_index].m_num_blocks_y;

			selector_history_buf.reset();

			int selector_history_buf_rle_count = 0;

			int prev_endpoint_pred_sym_bits = -1, endpoint_pred_repeat_count = 0;

			uint32_t prev_endpoint_index = 0;

			vector2D<uint8_t> block_endpoints_are_referenced(num_blocks_x, num_blocks_y);

			for (uint32_t block_y = 0; block_y < num_blocks_y; block_y++)
			{
				for (uint32_t block_x = 0; block_x < num_blocks_x; block_x++)
				{
					//const uint32_t block_index = first_block_index + block_x + block_y * num_blocks_x;

					encoder_block& m = m_slice_encoder_blocks[slice_index](block_x, block_y);

					if (m.m_endpoint_predictor == 0)
						block_endpoints_are_referenced(block_x - 1, block_y) = true;
					else if (m.m_endpoint_predictor == 1)
						block_endpoints_are_referenced(block_x, block_y - 1) = true;
					else if (m.m_endpoint_predictor == 2)
					{
						if (!is_video)
							block_endpoints_are_referenced(block_x - 1, block_y - 1) = true;
					}
					if (is_video)
					{
						if (m.m_is_cr_target)
							block_endpoints_are_referenced(block_x, block_y) = true;
					}

				}  // block_x
			} // block_y
						
			for (uint32_t block_y = 0; block_y < num_blocks_y; block_y++)
			{
				for (uint32_t block_x = 0; block_x < num_blocks_x; block_x++)
				{
					const uint32_t block_index = first_block_index + block_x + block_y * num_blocks_x;

					encoder_block& m = m_slice_encoder_blocks[slice_index](block_x, block_y);

					if (((block_x & 1) == 0) && ((block_y & 1) == 0))
					{
						uint32_t endpoint_pred_cur_sym_bits = 0;

						for (uint32_t y = 0; y < 2; y++)
						{
							for (uint32_t x = 0; x < 2; x++)
							{
								const uint32_t bx = block_x + x;
								const uint32_t by = block_y + y;

								uint32_t pred = basist::NO_ENDPOINT_PRED_INDEX;
								if ((bx < num_blocks_x) && (by < num_blocks_y))
									pred = m_slice_encoder_blocks[slice_index](bx, by).m_endpoint_predictor;

								endpoint_pred_cur_sym_bits |= (pred << (x * 2 + y * 4));
							}
						}

						if ((int)endpoint_pred_cur_sym_bits == prev_endpoint_pred_sym_bits)
						{
							endpoint_pred_repeat_count++;
						}
						else
						{
							if (endpoint_pred_repeat_count > 0)
							{
								if (endpoint_pred_repeat_count > (int)basist::ENDPOINT_PRED_MIN_REPEAT_COUNT)
								{
									endpoint_pred_histogram.inc(basist::ENDPOINT_PRED_REPEAT_LAST_SYMBOL);
									endpoint_pred_syms[slice_index].push_back(basist::ENDPOINT_PRED_REPEAT_LAST_SYMBOL);

									endpoint_pred_syms[slice_index].push_back(endpoint_pred_repeat_count);
								}
								else
								{
									for (int j = 0; j < endpoint_pred_repeat_count; j++)
									{
										endpoint_pred_histogram.inc(prev_endpoint_pred_sym_bits);
										endpoint_pred_syms[slice_index].push_back(prev_endpoint_pred_sym_bits);
									}
								}

								endpoint_pred_repeat_count = 0;
							}

							endpoint_pred_histogram.inc(endpoint_pred_cur_sym_bits);
							endpoint_pred_syms[slice_index].push_back(endpoint_pred_cur_sym_bits);

							prev_endpoint_pred_sym_bits = endpoint_pred_cur_sym_bits;
						}
					}

					int new_endpoint_index = m_endpoint_remap_table_old_to_new[m.m_endpoint_index];

					if (m.m_endpoint_predictor == basist::NO_ENDPOINT_PRED_INDEX)
					{
						int endpoint_delta = new_endpoint_index - prev_endpoint_index;

						if ((m_params.m_endpoint_rdo_quality_thresh > 1.0f) && (iabs(endpoint_delta) > 1) && (!block_endpoints_are_referenced(block_x, block_y)))
						{
							const pixel_block& src_pixels = r.get_source_pixel_block(block_index);

							etc_block etc_blk(r.get_output_block(block_index));

							const uint64_t cur_err = etc_blk.evaluate_etc1_error(src_pixels.get_ptr(), r.get_params().m_perceptual);
							const uint32_t cur_inten5 = etc_blk.get_inten_table(0);

							const etc1_endpoint_palette_entry& cur_endpoints = m_endpoint_palette[m.m_endpoint_index];
														
							if (cur_err)
							{
								const float endpoint_remap_thresh = maximum(1.0f, m_params.m_endpoint_rdo_quality_thresh);
								const uint64_t thresh_err = (uint64_t)(cur_err * endpoint_remap_thresh);

								//const int MAX_ENDPOINT_SEARCH_DIST = (m_params.m_compression_level >= 2) ? 64 : 32;
								const int MAX_ENDPOINT_SEARCH_DIST = (m_params.m_compression_level >= 2) ? 64 : 16;

								if (!g_cpu_supports_sse41)
								{
									const uint64_t initial_best_trial_err = UINT64_MAX;
									uint64_t best_trial_err = initial_best_trial_err;
									int best_trial_idx = 0;

									etc_block trial_etc_blk(etc_blk);
																		
									const int search_dist = minimum<int>(iabs(endpoint_delta) - 1, MAX_ENDPOINT_SEARCH_DIST);
									for (int d = -search_dist; d < search_dist; d++)
									{
										int trial_idx = prev_endpoint_index + d;
										if (trial_idx < 0)
											trial_idx += (int)r.get_total_endpoint_clusters();
										else if (trial_idx >= (int)r.get_total_endpoint_clusters())
											trial_idx -= (int)r.get_total_endpoint_clusters();

										if (trial_idx == new_endpoint_index)
											continue;

										// Skip it if this new endpoint palette entry is actually never used.
										if (!m_new_endpoint_was_used[trial_idx])
											continue;

										const etc1_endpoint_palette_entry& p = m_endpoint_palette[m_endpoint_remap_table_new_to_old[trial_idx]];
																				
										if (m_params.m_compression_level <= 1)
										{
											if (p.m_inten5 > cur_inten5)
												continue;

											int delta_r = iabs(cur_endpoints.m_color5.r - p.m_color5.r);
											int delta_g = iabs(cur_endpoints.m_color5.g - p.m_color5.g);
											int delta_b = iabs(cur_endpoints.m_color5.b - p.m_color5.b);
											int color_delta = delta_r + delta_g + delta_b;
																						
											if (color_delta > COLOR_DELTA_THRESH)
												continue;
										}

										trial_etc_blk.set_block_color5_etc1s(p.m_color5);
										trial_etc_blk.set_inten_tables_etc1s(p.m_inten5);

										uint64_t trial_err = trial_etc_blk.evaluate_etc1_error(src_pixels.get_ptr(), r.get_params().m_perceptual);

										if ((trial_err < best_trial_err) && (trial_err <= thresh_err))
										{
											best_trial_err = trial_err;
											best_trial_idx = trial_idx;
										}
									}

									if (best_trial_err != initial_best_trial_err)
									{
										m.m_endpoint_index = m_endpoint_remap_table_new_to_old[best_trial_idx];

										new_endpoint_index = best_trial_idx;

										endpoint_delta = new_endpoint_index - prev_endpoint_index;

										total_endpoint_indices_remapped++;
									}
								}
								else
								{
#if BASISU_SUPPORT_SSE
									uint8_t block_selectors[16];
									for (uint32_t i = 0; i < 16; i++)
										block_selectors[i] = (uint8_t)etc_blk.get_selector(i & 3, i >> 2);

									const int64_t initial_best_trial_err = INT64_MAX;
									int64_t best_trial_err = initial_best_trial_err;
									int best_trial_idx = 0;
																																				
									const int search_dist = minimum<int>(iabs(endpoint_delta) - 1, MAX_ENDPOINT_SEARCH_DIST);
									for (int d = -search_dist; d < search_dist; d++)
									{
										int trial_idx = prev_endpoint_index + d;
										if (trial_idx < 0)
											trial_idx += (int)r.get_total_endpoint_clusters();
										else if (trial_idx >= (int)r.get_total_endpoint_clusters())
											trial_idx -= (int)r.get_total_endpoint_clusters();

										if (trial_idx == new_endpoint_index)
											continue;

										// Skip it if this new endpoint palette entry is actually never used.
										if (!m_new_endpoint_was_used[trial_idx])
											continue;

										const etc1_endpoint_palette_entry& p = m_endpoint_palette[m_endpoint_remap_table_new_to_old[trial_idx]];
																				
										if (m_params.m_compression_level <= 1)
										{
											if (p.m_inten5 > cur_inten5)
												continue;

											int delta_r = iabs(cur_endpoints.m_color5.r - p.m_color5.r);
											int delta_g = iabs(cur_endpoints.m_color5.g - p.m_color5.g);
											int delta_b = iabs(cur_endpoints.m_color5.b - p.m_color5.b);
											int color_delta = delta_r + delta_g + delta_b;
											
											if (color_delta > COLOR_DELTA_THRESH)
												continue;
										}

										color_rgba block_colors[4];
										etc_block::get_block_colors_etc1s(block_colors, p.m_color5, p.m_inten5);

										int64_t trial_err;
										if (r.get_params().m_perceptual)
										{
											perceptual_distance_rgb_4_N_sse41(&trial_err, block_selectors, block_colors, src_pixels.get_ptr(), 16, best_trial_err);
										}
										else
										{
											linear_distance_rgb_4_N_sse41(&trial_err, block_selectors, block_colors, src_pixels.get_ptr(), 16, best_trial_err);
										}

										//if (trial_err > thresh_err)
										//	g_color_delta_bad_hist[color_delta]++;

										if ((trial_err < best_trial_err) && (trial_err <= (int64_t)thresh_err))
										{
											best_trial_err = trial_err;
											best_trial_idx = trial_idx;
										}
									}

									if (best_trial_err != initial_best_trial_err)
									{
										m.m_endpoint_index = m_endpoint_remap_table_new_to_old[best_trial_idx];

										new_endpoint_index = best_trial_idx;

										endpoint_delta = new_endpoint_index - prev_endpoint_index;

										total_endpoint_indices_remapped++;
									}
#endif // BASISU_SUPPORT_SSE
								} // if (!g_cpu_supports_sse41)
															
							} // if (cur_err)

						} // if ((m_params.m_endpoint_rdo_quality_thresh > 1.0f) && (iabs(endpoint_delta) > 1) && (!block_endpoints_are_referenced(block_x, block_y)))

						if (endpoint_delta < 0)
							endpoint_delta += (int)r.get_total_endpoint_clusters();

						delta_endpoint_histogram.inc(endpoint_delta);

					} // if (m.m_endpoint_predictor == basist::NO_ENDPOINT_PRED_INDEX)

					block_endpoint_indices.push_back(m_endpoint_remap_table_new_to_old[new_endpoint_index]);

					prev_endpoint_index = new_endpoint_index;

					if ((!is_video) || (m.m_endpoint_predictor != basist::CR_ENDPOINT_PRED_INDEX))
					{
						int new_selector_index = m_selector_remap_table_old_to_new[m.m_selector_index];
												
						const float selector_remap_thresh = maximum(1.0f, m_params.m_selector_rdo_quality_thresh); //2.5f;

						int selector_history_buf_index = -1;

						// At low comp levels this hurts compression a tiny amount, but is significantly faster so it's a good tradeoff.
						if ((m.m_is_cr_target) || (m_params.m_compression_level <= 1))
						{
							for (uint32_t j = 0; j < selector_history_buf.size(); j++)
							{
								const int trial_idx = selector_history_buf[j];
								if (trial_idx == new_selector_index)
								{
									total_used_selector_history_buf++;
									selector_history_buf_index = j;
									selector_history_buf_histogram.inc(j);
									break;
								}
							}
						}

						// If the block is a CR target we can't override its selectors.
						if ((!m.m_is_cr_target) && (selector_history_buf_index == -1))
						{
							const pixel_block& src_pixels = r.get_source_pixel_block(block_index);

							etc_block etc_blk = r.get_output_block(block_index);

							// This is new code - the initial release just used the endpoints from the frontend, which isn't correct/accurate.
							const etc1_endpoint_palette_entry& q = m_endpoint_palette[m_endpoint_remap_table_new_to_old[new_endpoint_index]];
							etc_blk.set_block_color5_etc1s(q.m_color5);
							etc_blk.set_inten_tables_etc1s(q.m_inten5);

							color_rgba block_colors[4];
							etc_blk.get_block_colors(block_colors, 0);

							const uint8_t* pCur_selectors = &m_selector_palette[m.m_selector_index][0];

							uint64_t cur_err = 0;
							if (r.get_params().m_perceptual)
							{
								for (uint32_t p = 0; p < 16; p++)
									cur_err += color_distance(true, src_pixels.get_ptr()[p], block_colors[pCur_selectors[p]], false);
							}
							else
							{
								for (uint32_t p = 0; p < 16; p++)
									cur_err += color_distance(false, src_pixels.get_ptr()[p], block_colors[pCur_selectors[p]], false);
							}
							
							const uint64_t limit_err = (uint64_t)ceilf(cur_err * selector_remap_thresh);

							// Even if cur_err==limit_err, we still want to scan the history buffer because there may be equivalent entries that are cheaper to code.

							uint64_t best_trial_err = UINT64_MAX;
							int best_trial_idx = 0;
							uint32_t best_trial_history_buf_idx = 0;

							for (uint32_t j = 0; j < selector_history_buf.size(); j++)
							{
								const int trial_idx = selector_history_buf[j];

								const uint8_t* pSelectors = &m_selector_palette[m_selector_remap_table_new_to_old[trial_idx]][0];

								if (m_params.m_compression_level <= 1)
								{
									// Predict if evaluating the full color error would cause an early out, by summing the abs err of the selector indices.
									int sel_diff = 0;
									for (uint32_t p = 0; p < 16; p += 4)
									{
										sel_diff += iabs(pCur_selectors[p + 0] - pSelectors[p + 0]);
										sel_diff += iabs(pCur_selectors[p + 1] - pSelectors[p + 1]);
										sel_diff += iabs(pCur_selectors[p + 2] - pSelectors[p + 2]);
										sel_diff += iabs(pCur_selectors[p + 3] - pSelectors[p + 3]);
										if (sel_diff >= SEL_DIFF_THRESHOLD)
											break;
									}
									if (sel_diff >= SEL_DIFF_THRESHOLD)
										continue;
								}
									
								const uint64_t thresh_err = minimum(limit_err, best_trial_err);
								uint64_t trial_err = 0;

								// This tends to early out quickly, so SSE has a hard time competing.
								if (r.get_params().m_perceptual)
								{
									for (uint32_t p = 0; p < 16; p++)
									{
										uint32_t sel = pSelectors[p];
										trial_err += color_distance(true, src_pixels.get_ptr()[p], block_colors[sel], false);
										if (trial_err > thresh_err)
											break;
									}
								}
								else
								{
									for (uint32_t p = 0; p < 16; p++)
									{
										uint32_t sel = pSelectors[p];
										trial_err += color_distance(false, src_pixels.get_ptr()[p], block_colors[sel], false);
										if (trial_err > thresh_err)
											break;
									}
								}

								if ((trial_err < best_trial_err) && (trial_err <= thresh_err))
								{
									assert(trial_err <= limit_err);

									best_trial_err = trial_err;
									best_trial_idx = trial_idx;
									best_trial_history_buf_idx = j;
								}
							}

							if (best_trial_err != UINT64_MAX)
							{
								if (new_selector_index != best_trial_idx)
									total_selector_indices_remapped++;

								new_selector_index = best_trial_idx;

								total_used_selector_history_buf++;

								selector_history_buf_index = best_trial_history_buf_idx;

								selector_history_buf_histogram.inc(best_trial_history_buf_idx);
							}

						} // if (m_params.m_selector_rdo_quality_thresh > 0.0f)

						m.m_selector_index = m_selector_remap_table_new_to_old[new_selector_index];


						if ((selector_history_buf_rle_count) && (selector_history_buf_index != 0))
						{
							if (selector_history_buf_rle_count >= (int)basist::SELECTOR_HISTORY_BUF_RLE_COUNT_THRESH)
							{
								selector_syms[slice_index].push_back(SELECTOR_HISTORY_BUF_RLE_SYMBOL_INDEX);
								selector_syms[slice_index].push_back(selector_history_buf_rle_count);

								int run_sym = selector_history_buf_rle_count - basist::SELECTOR_HISTORY_BUF_RLE_COUNT_THRESH;
								if (run_sym >= ((int)basist::SELECTOR_HISTORY_BUF_RLE_COUNT_TOTAL - 1))
									selector_history_buf_rle_histogram.inc(basist::SELECTOR_HISTORY_BUF_RLE_COUNT_TOTAL - 1);
								else
									selector_history_buf_rle_histogram.inc(run_sym);

								selector_histogram.inc(SELECTOR_HISTORY_BUF_RLE_SYMBOL_INDEX);
							}
							else
							{
								for (int k = 0; k < selector_history_buf_rle_count; k++)
								{
									uint32_t sym_index = SELECTOR_HISTORY_BUF_FIRST_SYMBOL_INDEX + 0;

									selector_syms[slice_index].push_back(sym_index);

									selector_histogram.inc(sym_index);
								}
							}

							selector_history_buf_rle_count = 0;
						}

						if (selector_history_buf_index >= 0)
						{
							if (selector_history_buf_index == 0)
								selector_history_buf_rle_count++;
							else
							{
								uint32_t history_buf_sym = SELECTOR_HISTORY_BUF_FIRST_SYMBOL_INDEX + selector_history_buf_index;

								selector_syms[slice_index].push_back(history_buf_sym);

								selector_histogram.inc(history_buf_sym);
							}
						}
						else
						{
							selector_syms[slice_index].push_back(new_selector_index);

							selector_histogram.inc(new_selector_index);
						}

						m.m_selector_history_buf_index = selector_history_buf_index;

						if (selector_history_buf_index < 0)
							selector_history_buf.add(new_selector_index);
						else if (selector_history_buf.size())
							selector_history_buf.use(selector_history_buf_index);
					}
					block_selector_indices.push_back(m.m_selector_index);

				} // block_x

			} // block_y

			if (endpoint_pred_repeat_count > 0)
			{
				if (endpoint_pred_repeat_count > (int)basist::ENDPOINT_PRED_MIN_REPEAT_COUNT)
				{
					endpoint_pred_histogram.inc(basist::ENDPOINT_PRED_REPEAT_LAST_SYMBOL);
					endpoint_pred_syms[slice_index].push_back(basist::ENDPOINT_PRED_REPEAT_LAST_SYMBOL);

					endpoint_pred_syms[slice_index].push_back(endpoint_pred_repeat_count);
				}
				else
				{
					for (int j = 0; j < endpoint_pred_repeat_count; j++)
					{
						endpoint_pred_histogram.inc(prev_endpoint_pred_sym_bits);
						endpoint_pred_syms[slice_index].push_back(prev_endpoint_pred_sym_bits);
					}
				}

				endpoint_pred_repeat_count = 0;
			}

			if (selector_history_buf_rle_count)
			{
				if (selector_history_buf_rle_count >= (int)basist::SELECTOR_HISTORY_BUF_RLE_COUNT_THRESH)
				{
					selector_syms[slice_index].push_back(SELECTOR_HISTORY_BUF_RLE_SYMBOL_INDEX);
					selector_syms[slice_index].push_back(selector_history_buf_rle_count);

					int run_sym = selector_history_buf_rle_count - basist::SELECTOR_HISTORY_BUF_RLE_COUNT_THRESH;
					if (run_sym >= ((int)basist::SELECTOR_HISTORY_BUF_RLE_COUNT_TOTAL - 1))
						selector_history_buf_rle_histogram.inc(basist::SELECTOR_HISTORY_BUF_RLE_COUNT_TOTAL - 1);
					else
						selector_history_buf_rle_histogram.inc(run_sym);

					selector_histogram.inc(SELECTOR_HISTORY_BUF_RLE_SYMBOL_INDEX);
				}
				else
				{
					for (int i = 0; i < selector_history_buf_rle_count; i++)
					{
						uint32_t sym_index = SELECTOR_HISTORY_BUF_FIRST_SYMBOL_INDEX + 0;

						selector_syms[slice_index].push_back(sym_index);

						selector_histogram.inc(sym_index);
					}
				}

				selector_history_buf_rle_count = 0;
			}

		} // slice_index

		//for (int i = 0; i <= 255 * 3; i++)
		//{
		//	printf("%u, %u, %f\n", g_color_delta_bad_hist[i], g_color_delta_hist[i], g_color_delta_hist[i] ? g_color_delta_bad_hist[i] / (float)g_color_delta_hist[i] : 0);
		//}
				
		double total_prep_time = tm.get_elapsed_secs();
		debug_printf("basisu_backend::encode_image: Total prep time: %3.2f\n", total_prep_time);

		debug_printf("Endpoint pred RDO total endpoint indices remapped: %u %3.2f%%\n",
			total_endpoint_indices_remapped, total_endpoint_indices_remapped * 100.0f / get_total_blocks());

		debug_printf("Selector history RDO total selector indices remapped: %u %3.2f%%, Used history buf: %u %3.2f%%\n",
			total_selector_indices_remapped, total_selector_indices_remapped * 100.0f / get_total_blocks(),
			total_used_selector_history_buf, total_used_selector_history_buf * 100.0f / get_total_blocks());

		//if ((total_endpoint_indices_remapped) && (m_params.m_compression_level > 0))
		if ((total_endpoint_indices_remapped) && (m_params.m_compression_level > 1) && (!m_params.m_used_global_codebooks))
		{
			int_vec unused;
			r.reoptimize_remapped_endpoints(block_endpoint_indices, unused, false, &block_selector_indices);

			create_endpoint_palette();
		}

		check_for_valid_cr_blocks();
		compute_slice_crcs();

		double endpoint_pred_entropy = endpoint_pred_histogram.get_entropy() / endpoint_pred_histogram.get_total();
		double delta_endpoint_entropy = delta_endpoint_histogram.get_entropy() / delta_endpoint_histogram.get_total();
		double selector_entropy = selector_histogram.get_entropy() / selector_histogram.get_total();

		debug_printf("Histogram entropy: EndpointPred: %3.3f DeltaEndpoint: %3.3f DeltaSelector: %3.3f\n", endpoint_pred_entropy, delta_endpoint_entropy, selector_entropy);

		if (!endpoint_pred_histogram.get_total())
			endpoint_pred_histogram.inc(0);
		huffman_encoding_table endpoint_pred_model;
		if (!endpoint_pred_model.init(endpoint_pred_histogram, 16))
		{
			error_printf("endpoint_pred_model.init() failed!");
			return false;
		}

		if (!delta_endpoint_histogram.get_total())
			delta_endpoint_histogram.inc(0);
		huffman_encoding_table delta_endpoint_model;
		if (!delta_endpoint_model.init(delta_endpoint_histogram, 16))
		{
			error_printf("delta_endpoint_model.init() failed!");
			return false;
		}
		if (!selector_histogram.get_total())
			selector_histogram.inc(0);

		huffman_encoding_table selector_model;
		if (!selector_model.init(selector_histogram, 16))
		{
			error_printf("selector_model.init() failed!");
			return false;
		}

		if (!selector_history_buf_rle_histogram.get_total())
			selector_history_buf_rle_histogram.inc(0);

		huffman_encoding_table selector_history_buf_rle_model;
		if (!selector_history_buf_rle_model.init(selector_history_buf_rle_histogram, 16))
		{
			error_printf("selector_history_buf_rle_model.init() failed!");
			return false;
		}

		bitwise_coder coder;
		coder.init(1024 * 1024 * 4);

		uint32_t endpoint_pred_model_bits = coder.emit_huffman_table(endpoint_pred_model);
		uint32_t delta_endpoint_bits = coder.emit_huffman_table(delta_endpoint_model);
		uint32_t selector_model_bits = coder.emit_huffman_table(selector_model);
		uint32_t selector_history_buf_run_sym_bits = coder.emit_huffman_table(selector_history_buf_rle_model);

		coder.put_bits(basist::MAX_SELECTOR_HISTORY_BUF_SIZE, 13);

		debug_printf("Model sizes: EndpointPred: %u bits %u bytes (%3.3f bpp) DeltaEndpoint: %u bits %u bytes (%3.3f bpp) Selector: %u bits %u bytes (%3.3f bpp) SelectorHistBufRLE: %u bits %u bytes (%3.3f bpp)\n",
			endpoint_pred_model_bits, (endpoint_pred_model_bits + 7) / 8, endpoint_pred_model_bits / float(get_total_input_texels()),
			delta_endpoint_bits, (delta_endpoint_bits + 7) / 8, delta_endpoint_bits / float(get_total_input_texels()),
			selector_model_bits, (selector_model_bits + 7) / 8, selector_model_bits / float(get_total_input_texels()),
			selector_history_buf_run_sym_bits, (selector_history_buf_run_sym_bits + 7) / 8, selector_history_buf_run_sym_bits / float(get_total_input_texels()));

		coder.flush();

		m_output.m_slice_image_tables = coder.get_bytes();

		uint32_t total_endpoint_pred_bits = 0, total_delta_endpoint_bits = 0, total_selector_bits = 0;

		uint32_t total_image_bytes = 0;

		m_output.m_slice_image_data.resize(m_slices.size());

		for (uint32_t slice_index = 0; slice_index < m_slices.size(); slice_index++)
		{
			//const uint32_t width = m_slices[slice_index].m_width;
			//const uint32_t height = m_slices[slice_index].m_height;
			const uint32_t num_blocks_x = m_slices[slice_index].m_num_blocks_x;
			const uint32_t num_blocks_y = m_slices[slice_index].m_num_blocks_y;

			coder.init(1024 * 1024 * 4);

			uint32_t cur_selector_sym_ofs = 0;
			uint32_t selector_rle_count = 0;

			int endpoint_pred_repeat_count = 0;
			uint32_t cur_endpoint_pred_sym_ofs = 0;
//			uint32_t prev_endpoint_pred_sym = 0;
			uint32_t prev_endpoint_index = 0;

			for (uint32_t block_y = 0; block_y < num_blocks_y; block_y++)
			{
				for (uint32_t block_x = 0; block_x < num_blocks_x; block_x++)
				{
					const encoder_block& m = m_slice_encoder_blocks[slice_index](block_x, block_y);

					if (((block_x & 1) == 0) && ((block_y & 1) == 0))
					{
						if (endpoint_pred_repeat_count > 0)
						{
							endpoint_pred_repeat_count--;
						}
						else
						{
							uint32_t sym = endpoint_pred_syms[slice_index][cur_endpoint_pred_sym_ofs++];

							if (sym == basist::ENDPOINT_PRED_REPEAT_LAST_SYMBOL)
							{
								total_endpoint_pred_bits += coder.put_code(sym, endpoint_pred_model);

								endpoint_pred_repeat_count = endpoint_pred_syms[slice_index][cur_endpoint_pred_sym_ofs++];
								assert(endpoint_pred_repeat_count >= (int)basist::ENDPOINT_PRED_MIN_REPEAT_COUNT);

								total_endpoint_pred_bits += coder.put_vlc(endpoint_pred_repeat_count - basist::ENDPOINT_PRED_MIN_REPEAT_COUNT, basist::ENDPOINT_PRED_COUNT_VLC_BITS);

								endpoint_pred_repeat_count--;
							}
							else
							{
								total_endpoint_pred_bits += coder.put_code(sym, endpoint_pred_model);

								//prev_endpoint_pred_sym = sym;
							}
						}
					}

					const int new_endpoint_index = m_endpoint_remap_table_old_to_new[m.m_endpoint_index];

					if (m.m_endpoint_predictor == basist::NO_ENDPOINT_PRED_INDEX)
					{
						int endpoint_delta = new_endpoint_index - prev_endpoint_index;
						if (endpoint_delta < 0)
							endpoint_delta += (int)r.get_total_endpoint_clusters();

						total_delta_endpoint_bits += coder.put_code(endpoint_delta, delta_endpoint_model);
					}

					prev_endpoint_index = new_endpoint_index;

					if ((!is_video) || (m.m_endpoint_predictor != basist::CR_ENDPOINT_PRED_INDEX))
					{
						if (!selector_rle_count)
						{
							uint32_t selector_sym_index = selector_syms[slice_index][cur_selector_sym_ofs++];

							if (selector_sym_index == SELECTOR_HISTORY_BUF_RLE_SYMBOL_INDEX)
								selector_rle_count = selector_syms[slice_index][cur_selector_sym_ofs++];

							total_selector_bits += coder.put_code(selector_sym_index, selector_model);

							if (selector_sym_index == SELECTOR_HISTORY_BUF_RLE_SYMBOL_INDEX)
							{
								int run_sym = selector_rle_count - basist::SELECTOR_HISTORY_BUF_RLE_COUNT_THRESH;
								if (run_sym >= ((int)basist::SELECTOR_HISTORY_BUF_RLE_COUNT_TOTAL - 1))
								{
									total_selector_bits += coder.put_code(basist::SELECTOR_HISTORY_BUF_RLE_COUNT_TOTAL - 1, selector_history_buf_rle_model);

									uint32_t n = selector_rle_count - basist::SELECTOR_HISTORY_BUF_RLE_COUNT_THRESH;
									total_selector_bits += coder.put_vlc(n, 7);
								}
								else
									total_selector_bits += coder.put_code(run_sym, selector_history_buf_rle_model);
							}
						}

						if (selector_rle_count)
							selector_rle_count--;
					}

				} // block_x

			} // block_y

			BASISU_BACKEND_VERIFY(cur_endpoint_pred_sym_ofs == endpoint_pred_syms[slice_index].size());
			BASISU_BACKEND_VERIFY(cur_selector_sym_ofs == selector_syms[slice_index].size());

			coder.flush();

			m_output.m_slice_image_data[slice_index] = coder.get_bytes();

			total_image_bytes += (uint32_t)coder.get_bytes().size();

			debug_printf("Slice %u compressed size: %u bytes, %3.3f bits per slice texel\n", slice_index, m_output.m_slice_image_data[slice_index].size(), m_output.m_slice_image_data[slice_index].size() * 8.0f / (m_slices[slice_index].m_orig_width * m_slices[slice_index].m_orig_height));

		} // slice_index

		const double total_texels = static_cast<double>(get_total_input_texels());
		const double total_blocks = static_cast<double>(get_total_blocks());

		debug_printf("Total endpoint pred bits: %u bytes: %u bits/texel: %3.3f bits/block: %3.3f\n", total_endpoint_pred_bits, total_endpoint_pred_bits / 8, total_endpoint_pred_bits / total_texels, total_endpoint_pred_bits / total_blocks);
		debug_printf("Total delta endpoint bits: %u bytes: %u bits/texel: %3.3f bits/block: %3.3f\n", total_delta_endpoint_bits, total_delta_endpoint_bits / 8, total_delta_endpoint_bits / total_texels, total_delta_endpoint_bits / total_blocks);
		debug_printf("Total selector bits: %u bytes: %u bits/texel: %3.3f bits/block: %3.3f\n", total_selector_bits, total_selector_bits / 8, total_selector_bits / total_texels, total_selector_bits / total_blocks);

		debug_printf("Total table bytes: %u, %3.3f bits/texel\n", m_output.m_slice_image_tables.size(), m_output.m_slice_image_tables.size() * 8.0f / total_texels);
		debug_printf("Total image bytes: %u, %3.3f bits/texel\n", total_image_bytes, total_image_bytes * 8.0f / total_texels);

		return true;
	}

	bool basisu_backend::encode_endpoint_palette()
	{
		const basisu_frontend& r = *m_pFront_end;

		// The endpoint indices may have been changed by the backend's RDO step, so go and figure out which ones are actually used again.
		bool_vec old_endpoint_was_used(r.get_total_endpoint_clusters());
		uint32_t first_old_entry_index = UINT32_MAX;

		for (uint32_t slice_index = 0; slice_index < m_slices.size(); slice_index++)
		{
			const uint32_t num_blocks_x = m_slices[slice_index].m_num_blocks_x, num_blocks_y = m_slices[slice_index].m_num_blocks_y;
			for (uint32_t block_y = 0; block_y < num_blocks_y; block_y++)
			{
				for (uint32_t block_x = 0; block_x < num_blocks_x; block_x++)
				{
					encoder_block& m = m_slice_encoder_blocks[slice_index](block_x, block_y);
					const uint32_t old_endpoint_index = m.m_endpoint_index;

					old_endpoint_was_used[old_endpoint_index] = true;
					first_old_entry_index = basisu::minimum(first_old_entry_index, old_endpoint_index);
				} // block_x
			} // block_y
		} // slice_index

		debug_printf("basisu_backend::encode_endpoint_palette: first_old_entry_index: %u\n", first_old_entry_index);

		// Maps NEW to OLD endpoints
		uint_vec endpoint_remap_table_new_to_old(r.get_total_endpoint_clusters());
		endpoint_remap_table_new_to_old.set_all(first_old_entry_index);

		bool_vec new_endpoint_was_used(r.get_total_endpoint_clusters());

		for (uint32_t old_endpoint_index = 0; old_endpoint_index < m_endpoint_remap_table_old_to_new.size(); old_endpoint_index++)
		{
			if (old_endpoint_was_used[old_endpoint_index])
			{
				const uint32_t new_endpoint_index = m_endpoint_remap_table_old_to_new[old_endpoint_index];
				
				new_endpoint_was_used[new_endpoint_index] = true;

				endpoint_remap_table_new_to_old[new_endpoint_index] = old_endpoint_index;
			}
		}

		// TODO: Some new endpoint palette entries may actually be unused and aren't worth coding. Fix that.

		uint32_t total_unused_new_entries = 0;
		for (uint32_t i = 0; i < new_endpoint_was_used.size(); i++)
			if (!new_endpoint_was_used[i])
				total_unused_new_entries++;
		debug_printf("basisu_backend::encode_endpoint_palette: total_unused_new_entries: %u out of %u\n", total_unused_new_entries, new_endpoint_was_used.size());

		bool is_grayscale = true;
		for (uint32_t old_endpoint_index = 0; old_endpoint_index < (uint32_t)m_endpoint_palette.size(); old_endpoint_index++)
		{
			int r5 = m_endpoint_palette[old_endpoint_index].m_color5[0];
			int g5 = m_endpoint_palette[old_endpoint_index].m_color5[1];
			int b5 = m_endpoint_palette[old_endpoint_index].m_color5[2];
			if ((r5 != g5) || (r5 != b5))
			{
				is_grayscale = false;
				break;
			}
		}

		histogram color5_delta_hist0(32); // prev 0-9, delta is -9 to 31
		histogram color5_delta_hist1(32); // prev 10-21, delta is -21 to 21
		histogram color5_delta_hist2(32); // prev 22-31, delta is -31 to 9
		histogram inten_delta_hist(8);

		color_rgba prev_color5(16, 16, 16, 0);
		uint32_t prev_inten = 0;

		for (uint32_t new_endpoint_index = 0; new_endpoint_index < r.get_total_endpoint_clusters(); new_endpoint_index++)
		{
			const uint32_t old_endpoint_index = endpoint_remap_table_new_to_old[new_endpoint_index];

			int delta_inten = m_endpoint_palette[old_endpoint_index].m_inten5 - prev_inten;
			inten_delta_hist.inc(delta_inten & 7);
			prev_inten = m_endpoint_palette[old_endpoint_index].m_inten5;

			for (uint32_t i = 0; i < (is_grayscale ? 1U : 3U); i++)
			{
				const int delta = (m_endpoint_palette[old_endpoint_index].m_color5[i] - prev_color5[i]) & 31;

				if (prev_color5[i] <= basist::COLOR5_PAL0_PREV_HI)
					color5_delta_hist0.inc(delta);
				else if (prev_color5[i] <= basist::COLOR5_PAL1_PREV_HI)
					color5_delta_hist1.inc(delta);
				else
					color5_delta_hist2.inc(delta);

				prev_color5[i] = m_endpoint_palette[old_endpoint_index].m_color5[i];
			}
		}

		if (!color5_delta_hist0.get_total()) color5_delta_hist0.inc(0);
		if (!color5_delta_hist1.get_total()) color5_delta_hist1.inc(0);
		if (!color5_delta_hist2.get_total()) color5_delta_hist2.inc(0);

		huffman_encoding_table color5_delta_model0, color5_delta_model1, color5_delta_model2, inten_delta_model;
		if (!color5_delta_model0.init(color5_delta_hist0, 16))
		{
			error_printf("color5_delta_model.init() failed!");
			return false;
		}

		if (!color5_delta_model1.init(color5_delta_hist1, 16))
		{
			error_printf("color5_delta_model.init() failed!");
			return false;
		}

		if (!color5_delta_model2.init(color5_delta_hist2, 16))
		{
			error_printf("color5_delta_model.init() failed!");
			return false;
		}

		if (!inten_delta_model.init(inten_delta_hist, 16))
		{
			error_printf("inten3_model.init() failed!");
			return false;
		}

		bitwise_coder coder;

		coder.init(8192);

		coder.emit_huffman_table(color5_delta_model0);
		coder.emit_huffman_table(color5_delta_model1);
		coder.emit_huffman_table(color5_delta_model2);
		coder.emit_huffman_table(inten_delta_model);

		coder.put_bits(is_grayscale, 1);

		prev_color5.set(16, 16, 16, 0);
		prev_inten = 0;

		for (uint32_t new_endpoint_index = 0; new_endpoint_index < r.get_total_endpoint_clusters(); new_endpoint_index++)
		{
			const uint32_t old_endpoint_index = endpoint_remap_table_new_to_old[new_endpoint_index];

			int delta_inten = (m_endpoint_palette[old_endpoint_index].m_inten5 - prev_inten) & 7;
			coder.put_code(delta_inten, inten_delta_model);
			prev_inten = m_endpoint_palette[old_endpoint_index].m_inten5;

			for (uint32_t i = 0; i < (is_grayscale ? 1U : 3U); i++)
			{
				const int delta = (m_endpoint_palette[old_endpoint_index].m_color5[i] - prev_color5[i]) & 31;

				if (prev_color5[i] <= basist::COLOR5_PAL0_PREV_HI)
					coder.put_code(delta, color5_delta_model0);
				else if (prev_color5[i] <= basist::COLOR5_PAL1_PREV_HI)
					coder.put_code(delta, color5_delta_model1);
				else
					coder.put_code(delta, color5_delta_model2);

				prev_color5[i] = m_endpoint_palette[old_endpoint_index].m_color5[i];
			}

		} // q

		coder.flush();

		m_output.m_endpoint_palette = coder.get_bytes();

		debug_printf("Endpoint codebook size: %u bits %u bytes, Bits per entry: %3.1f, Avg bits/texel: %3.3f\n",
			8 * (int)m_output.m_endpoint_palette.size(), (int)m_output.m_endpoint_palette.size(), m_output.m_endpoint_palette.size() * 8.0f / r.get_total_endpoint_clusters(), m_output.m_endpoint_palette.size() * 8.0f / get_total_input_texels());

		return true;
	}

	bool basisu_backend::encode_selector_palette()
	{
		const basisu_frontend& r = *m_pFront_end;
		
		histogram delta_selector_pal_histogram(256);

		for (uint32_t q = 0; q < r.get_total_selector_clusters(); q++)
		{
			if (!q)
				continue;

			const etc1_selector_palette_entry& cur = m_selector_palette[m_selector_remap_table_new_to_old[q]];
			const etc1_selector_palette_entry predictor(m_selector_palette[m_selector_remap_table_new_to_old[q - 1]]);

			for (uint32_t j = 0; j < 4; j++)
				delta_selector_pal_histogram.inc(cur.get_byte(j) ^ predictor.get_byte(j));
		}

		if (!delta_selector_pal_histogram.get_total())
			delta_selector_pal_histogram.inc(0);

		huffman_encoding_table delta_selector_pal_model;
		if (!delta_selector_pal_model.init(delta_selector_pal_histogram, 16))
		{
			error_printf("delta_selector_pal_model.init() failed!");
			return false;
		}

		bitwise_coder coder;
		coder.init(1024 * 1024);

		coder.put_bits(0, 1); // use global codebook
		coder.put_bits(0, 1); // uses hybrid codebooks

		coder.put_bits(0, 1); // raw bytes

		coder.emit_huffman_table(delta_selector_pal_model);

		for (uint32_t q = 0; q < r.get_total_selector_clusters(); q++)
		{
			if (!q)
			{
				for (uint32_t j = 0; j < 4; j++)
					coder.put_bits(m_selector_palette[m_selector_remap_table_new_to_old[q]].get_byte(j), 8);
				continue;
			}

			const etc1_selector_palette_entry& cur = m_selector_palette[m_selector_remap_table_new_to_old[q]];
			const etc1_selector_palette_entry predictor(m_selector_palette[m_selector_remap_table_new_to_old[q - 1]]);

			for (uint32_t j = 0; j < 4; j++)
				coder.put_code(cur.get_byte(j) ^ predictor.get_byte(j), delta_selector_pal_model);
		}

		coder.flush();

		m_output.m_selector_palette = coder.get_bytes();

		if (m_output.m_selector_palette.size() >= r.get_total_selector_clusters() * 4)
		{
			coder.init(1024 * 1024);

			coder.put_bits(0, 1); // use global codebook
			coder.put_bits(0, 1); // uses hybrid codebooks

			coder.put_bits(1, 1); // raw bytes

			for (uint32_t q = 0; q < r.get_total_selector_clusters(); q++)
			{
				const uint32_t i = m_selector_remap_table_new_to_old[q];

				for (uint32_t j = 0; j < 4; j++)
					coder.put_bits(m_selector_palette[i].get_byte(j), 8);
			}

			coder.flush();

			m_output.m_selector_palette = coder.get_bytes();
		}

		debug_printf("Selector codebook bits: %u bytes: %u, Bits per entry: %3.1f, Avg bits/texel: %3.3f\n",
			(int)m_output.m_selector_palette.size() * 8, (int)m_output.m_selector_palette.size(),
			m_output.m_selector_palette.size() * 8.0f / r.get_total_selector_clusters(), m_output.m_selector_palette.size() * 8.0f / get_total_input_texels());

		return true;
	}

	uint32_t basisu_backend::encode()
	{
		//const bool is_video = m_pFront_end->get_params().m_tex_type == basist::cBASISTexTypeVideoFrames;
		m_output.m_slice_desc = m_slices;
		m_output.m_etc1s = m_params.m_etc1s;
		m_output.m_uses_global_codebooks = m_params.m_used_global_codebooks;
		m_output.m_srgb = m_pFront_end->get_params().m_perceptual;

		create_endpoint_palette();
		create_selector_palette();

		create_encoder_blocks();

		if (!encode_image())
			return 0;

		if (!encode_endpoint_palette())
			return 0;

		if (!encode_selector_palette())
			return 0;

		uint32_t total_compressed_bytes = (uint32_t)(m_output.m_slice_image_tables.size() + m_output.m_endpoint_palette.size() + m_output.m_selector_palette.size());
		for (uint32_t i = 0; i < m_output.m_slice_image_data.size(); i++)
			total_compressed_bytes += (uint32_t)m_output.m_slice_image_data[i].size();

		debug_printf("Wrote %u bytes, %3.3f bits/texel\n", total_compressed_bytes, total_compressed_bytes * 8.0f / get_total_input_texels());

		return total_compressed_bytes;
	}

} // namespace basisu

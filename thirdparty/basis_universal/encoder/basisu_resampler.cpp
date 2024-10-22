// basisu_resampler.cpp
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
#include "basisu_resampler.h"
#include "basisu_resampler_filters.h"

#define RESAMPLER_DEBUG 0

namespace basisu
{
	static inline int resampler_range_check(int v, int h)
	{
		BASISU_NOTE_UNUSED(h);
		assert((v >= 0) && (v < h));
		return v;
	}

	// Float to int cast with truncation.
	static inline int cast_to_int(Resample_Real i)
	{
		return (int)i;
	}

	// Ensure that the contributing source sample is within bounds. If not, reflect, clamp, or wrap.
	int Resampler::reflect(const int j, const int src_x, const Boundary_Op boundary_op)
	{
		int n;

		if (j < 0)
		{
			if (boundary_op == BOUNDARY_REFLECT)
			{
				n = -j;

				if (n >= src_x)
					n = src_x - 1;
			}
			else if (boundary_op == BOUNDARY_WRAP)
				n = posmod(j, src_x);
			else
				n = 0;
		}
		else if (j >= src_x)
		{
			if (boundary_op == BOUNDARY_REFLECT)
			{
				n = (src_x - j) + (src_x - 1);

				if (n < 0)
					n = 0;
			}
			else if (boundary_op == BOUNDARY_WRAP)
				n = posmod(j, src_x);
			else
				n = src_x - 1;
		}
		else
			n = j;

		return n;
	}

	// The make_clist() method generates, for all destination samples,
	// the list of all source samples with non-zero weighted contributions.
	Resampler::Contrib_List * Resampler::make_clist(
		int src_x, int dst_x, Boundary_Op boundary_op,
		Resample_Real(*Pfilter)(Resample_Real),
		Resample_Real filter_support,
		Resample_Real filter_scale,
		Resample_Real src_ofs)
	{
		struct Contrib_Bounds
		{
			// The center of the range in DISCRETE coordinates (pixel center = 0.0f).
			Resample_Real center;
			int left, right;
		};

		int i, j, k, n, left, right;
		Resample_Real total_weight;
		Resample_Real xscale, center, half_width, weight;
		Contrib_List* Pcontrib;
		Contrib* Pcpool;
		Contrib* Pcpool_next;
		Contrib_Bounds* Pcontrib_bounds;

		if ((Pcontrib = (Contrib_List*)calloc(dst_x, sizeof(Contrib_List))) == NULL)
			return NULL;

		Pcontrib_bounds = (Contrib_Bounds*)calloc(dst_x, sizeof(Contrib_Bounds));
		if (!Pcontrib_bounds)
		{
			free(Pcontrib);
			return (NULL);
		}

		const Resample_Real oo_filter_scale = 1.0f / filter_scale;

		const Resample_Real NUDGE = 0.5f;
		xscale = dst_x / (Resample_Real)src_x;

		if (xscale < 1.0f)
		{
			int total;
			(void)total;

			// Handle case when there are fewer destination samples than source samples (downsampling/minification).

			// stretched half width of filter
			half_width = (filter_support / xscale) * filter_scale;

			// Find the range of source sample(s) that will contribute to each destination sample.

			for (i = 0, n = 0; i < dst_x; i++)
			{
				// Convert from discrete to continuous coordinates, scale, then convert back to discrete.
				center = ((Resample_Real)i + NUDGE) / xscale;
				center -= NUDGE;
				center += src_ofs;

				left = cast_to_int((Resample_Real)floor(center - half_width));
				right = cast_to_int((Resample_Real)ceil(center + half_width));

				Pcontrib_bounds[i].center = center;
				Pcontrib_bounds[i].left = left;
				Pcontrib_bounds[i].right = right;

				n += (right - left + 1);
			}

			// Allocate memory for contributors. 

			if ((n == 0) || ((Pcpool = (Contrib*)calloc(n, sizeof(Contrib))) == NULL))
			{
				free(Pcontrib);
				free(Pcontrib_bounds);
				return NULL;
			}
			total = n;

			Pcpool_next = Pcpool;

			// Create the list of source samples which contribute to each destination sample.

			for (i = 0; i < dst_x; i++)
			{
				int max_k = -1;
				Resample_Real max_w = -1e+20f;

				center = Pcontrib_bounds[i].center;
				left = Pcontrib_bounds[i].left;
				right = Pcontrib_bounds[i].right;

				Pcontrib[i].n = 0;
				Pcontrib[i].p = Pcpool_next;
				Pcpool_next += (right - left + 1);
				assert((Pcpool_next - Pcpool) <= total);

				total_weight = 0;

				for (j = left; j <= right; j++)
					total_weight += (*Pfilter)((center - (Resample_Real)j) * xscale * oo_filter_scale);
				const Resample_Real norm = static_cast<Resample_Real>(1.0f / total_weight);

				total_weight = 0;

#if RESAMPLER_DEBUG
				printf("%i: ", i);
#endif

				for (j = left; j <= right; j++)
				{
					weight = (*Pfilter)((center - (Resample_Real)j) * xscale * oo_filter_scale) * norm;
					if (weight == 0.0f)
						continue;

					n = reflect(j, src_x, boundary_op);

#if RESAMPLER_DEBUG
					printf("%i(%f), ", n, weight);
#endif

					// Increment the number of source samples which contribute to the current destination sample.

					k = Pcontrib[i].n++;

					Pcontrib[i].p[k].pixel = (unsigned short)n; /* store src sample number */
					Pcontrib[i].p[k].weight = weight;           /* store src sample weight */

					total_weight += weight; /* total weight of all contributors */

					if (weight > max_w)
					{
						max_w = weight;
						max_k = k;
					}
				}

#if RESAMPLER_DEBUG
				printf("\n\n");
#endif

				//assert(Pcontrib[i].n);
				//assert(max_k != -1);
				if ((max_k == -1) || (Pcontrib[i].n == 0))
				{
					free(Pcpool);
					free(Pcontrib);
					free(Pcontrib_bounds);
					return NULL;
				}

				if (total_weight != 1.0f)
					Pcontrib[i].p[max_k].weight += 1.0f - total_weight;
			}
		}
		else
		{
			// Handle case when there are more destination samples than source samples (upsampling).

			half_width = filter_support * filter_scale;

			// Find the source sample(s) that contribute to each destination sample.

			for (i = 0, n = 0; i < dst_x; i++)
			{
				// Convert from discrete to continuous coordinates, scale, then convert back to discrete.
				center = ((Resample_Real)i + NUDGE) / xscale;
				center -= NUDGE;
				center += src_ofs;

				left = cast_to_int((Resample_Real)floor(center - half_width));
				right = cast_to_int((Resample_Real)ceil(center + half_width));

				Pcontrib_bounds[i].center = center;
				Pcontrib_bounds[i].left = left;
				Pcontrib_bounds[i].right = right;

				n += (right - left + 1);
			}

			/* Allocate memory for contributors. */

			int total = n;
			if ((total == 0) || ((Pcpool = (Contrib*)calloc(total, sizeof(Contrib))) == NULL))
			{
				free(Pcontrib);
				free(Pcontrib_bounds);
				return NULL;
			}

			Pcpool_next = Pcpool;

			// Create the list of source samples which contribute to each destination sample.

			for (i = 0; i < dst_x; i++)
			{
				int max_k = -1;
				Resample_Real max_w = -1e+20f;

				center = Pcontrib_bounds[i].center;
				left = Pcontrib_bounds[i].left;
				right = Pcontrib_bounds[i].right;

				Pcontrib[i].n = 0;
				Pcontrib[i].p = Pcpool_next;
				Pcpool_next += (right - left + 1);
				assert((Pcpool_next - Pcpool) <= total);

				total_weight = 0;
				for (j = left; j <= right; j++)
					total_weight += (*Pfilter)((center - (Resample_Real)j) * oo_filter_scale);

				const Resample_Real norm = static_cast<Resample_Real>(1.0f / total_weight);

				total_weight = 0;

#if RESAMPLER_DEBUG
				printf("%i: ", i);
#endif

				for (j = left; j <= right; j++)
				{
					weight = (*Pfilter)((center - (Resample_Real)j) * oo_filter_scale) * norm;
					if (weight == 0.0f)
						continue;

					n = reflect(j, src_x, boundary_op);

#if RESAMPLER_DEBUG
					printf("%i(%f), ", n, weight);
#endif

					// Increment the number of source samples which contribute to the current destination sample.

					k = Pcontrib[i].n++;

					Pcontrib[i].p[k].pixel = (unsigned short)n; /* store src sample number */
					Pcontrib[i].p[k].weight = weight;           /* store src sample weight */

					total_weight += weight; /* total weight of all contributors */

					if (weight > max_w)
					{
						max_w = weight;
						max_k = k;
					}
				}

#if RESAMPLER_DEBUG
				printf("\n\n");
#endif

				//assert(Pcontrib[i].n);
				//assert(max_k != -1);

				if ((max_k == -1) || (Pcontrib[i].n == 0))
				{
					free(Pcpool);
					free(Pcontrib);
					free(Pcontrib_bounds);
					return NULL;
				}

				if (total_weight != 1.0f)
					Pcontrib[i].p[max_k].weight += 1.0f - total_weight;
			}
		}

#if RESAMPLER_DEBUG
		printf("*******\n");
#endif

		free(Pcontrib_bounds);

		return Pcontrib;
	}

	void Resampler::resample_x(Sample * Pdst, const Sample * Psrc)
	{
		assert(Pdst);
		assert(Psrc);

		int i, j;
		Sample total;
		Contrib_List* Pclist = m_Pclist_x;
		Contrib* p;

		for (i = m_resample_dst_x; i > 0; i--, Pclist++)
		{
#if BASISU_RESAMPLER_DEBUG_OPS
			total_ops += Pclist->n;
#endif

			for (j = Pclist->n, p = Pclist->p, total = 0; j > 0; j--, p++)
				total += Psrc[p->pixel] * p->weight;

			*Pdst++ = total;
		}
	}

	void Resampler::scale_y_mov(Sample * Ptmp, const Sample * Psrc, Resample_Real weight, int dst_x)
	{
		int i;

#if BASISU_RESAMPLER_DEBUG_OPS
		total_ops += dst_x;
#endif

		// Not += because temp buf wasn't cleared.
		for (i = dst_x; i > 0; i--)
			* Ptmp++ = *Psrc++ * weight;
	}

	void Resampler::scale_y_add(Sample * Ptmp, const Sample * Psrc, Resample_Real weight, int dst_x)
	{
#if BASISU_RESAMPLER_DEBUG_OPS
		total_ops += dst_x;
#endif

		for (int i = dst_x; i > 0; i--)
			(*Ptmp++) += *Psrc++ * weight;
	}

	void Resampler::clamp(Sample * Pdst, int n)
	{
		while (n > 0)
		{
			Sample x = *Pdst;
			*Pdst++ = clamp_sample(x);
			n--;
		}
	}

	void Resampler::resample_y(Sample * Pdst)
	{
		int i, j;
		Sample* Psrc;
		Contrib_List* Pclist = &m_Pclist_y[m_cur_dst_y];

		Sample* Ptmp = m_delay_x_resample ? m_Ptmp_buf : Pdst;
		assert(Ptmp);

		/* Process each contributor. */

		for (i = 0; i < Pclist->n; i++)
		{
			// locate the contributor's location in the scan buffer -- the contributor must always be found!
			for (j = 0; j < MAX_SCAN_BUF_SIZE; j++)
				if (m_Pscan_buf->scan_buf_y[j] == Pclist->p[i].pixel)
					break;

			assert(j < MAX_SCAN_BUF_SIZE);

			Psrc = m_Pscan_buf->scan_buf_l[j];

			if (!i)
				scale_y_mov(Ptmp, Psrc, Pclist->p[i].weight, m_intermediate_x);
			else
				scale_y_add(Ptmp, Psrc, Pclist->p[i].weight, m_intermediate_x);

			/* If this source line doesn't contribute to any
			* more destination lines then mark the scanline buffer slot
			* which holds this source line as free.
			* (The max. number of slots used depends on the Y
			* axis sampling factor and the scaled filter width.)
			*/

			if (--m_Psrc_y_count[resampler_range_check(Pclist->p[i].pixel, m_resample_src_y)] == 0)
			{
				m_Psrc_y_flag[resampler_range_check(Pclist->p[i].pixel, m_resample_src_y)] = false;
				m_Pscan_buf->scan_buf_y[j] = -1;
			}
		}

		/* Now generate the destination line */

		if (m_delay_x_resample) // Was X resampling delayed until after Y resampling?
		{
			assert(Pdst != Ptmp);
			resample_x(Pdst, Ptmp);
		}
		else
		{
			assert(Pdst == Ptmp);
		}

		if (m_lo < m_hi)
			clamp(Pdst, m_resample_dst_x);
	}

	bool Resampler::put_line(const Sample * Psrc)
	{
		int i;

		if (m_cur_src_y >= m_resample_src_y)
			return false;

		/* Does this source line contribute
		* to any destination line? if not,
		* exit now.
		*/

		if (!m_Psrc_y_count[resampler_range_check(m_cur_src_y, m_resample_src_y)])
		{
			m_cur_src_y++;
			return true;
		}

		/* Find an empty slot in the scanline buffer. (FIXME: Perf. is terrible here with extreme scaling ratios.) */

		for (i = 0; i < MAX_SCAN_BUF_SIZE; i++)
			if (m_Pscan_buf->scan_buf_y[i] == -1)
				break;

		/* If the buffer is full, exit with an error. */

		if (i == MAX_SCAN_BUF_SIZE)
		{
			m_status = STATUS_SCAN_BUFFER_FULL;
			return false;
		}

		m_Psrc_y_flag[resampler_range_check(m_cur_src_y, m_resample_src_y)] = true;
		m_Pscan_buf->scan_buf_y[i] = m_cur_src_y;

		/* Does this slot have any memory allocated to it? */

		if (!m_Pscan_buf->scan_buf_l[i])
		{
			if ((m_Pscan_buf->scan_buf_l[i] = (Sample*)malloc(m_intermediate_x * sizeof(Sample))) == NULL)
			{
				m_status = STATUS_OUT_OF_MEMORY;
				return false;
			}
		}

		// Resampling on the X axis first?
		if (m_delay_x_resample)
		{
			assert(m_intermediate_x == m_resample_src_x);

			// Y-X resampling order
			memcpy(m_Pscan_buf->scan_buf_l[i], Psrc, m_intermediate_x * sizeof(Sample));
		}
		else
		{
			assert(m_intermediate_x == m_resample_dst_x);

			// X-Y resampling order
			resample_x(m_Pscan_buf->scan_buf_l[i], Psrc);
		}

		m_cur_src_y++;

		return true;
	}

	const Resampler::Sample* Resampler::get_line()
	{
		int i;

		/* If all the destination lines have been
		* generated, then always return NULL.
		*/

		if (m_cur_dst_y == m_resample_dst_y)
			return NULL;

		/* Check to see if all the required
		* contributors are present, if not,
		* return NULL.
		*/

		for (i = 0; i < m_Pclist_y[m_cur_dst_y].n; i++)
			if (!m_Psrc_y_flag[resampler_range_check(m_Pclist_y[m_cur_dst_y].p[i].pixel, m_resample_src_y)])
				return NULL;

		resample_y(m_Pdst_buf);

		m_cur_dst_y++;

		return m_Pdst_buf;
	}

	Resampler::~Resampler()
	{
		int i;

#if BASISU_RESAMPLER_DEBUG_OPS
		printf("actual ops: %i\n", total_ops);
#endif

		free(m_Pdst_buf);
		m_Pdst_buf = NULL;

		if (m_Ptmp_buf)
		{
			free(m_Ptmp_buf);
			m_Ptmp_buf = NULL;
		}

		/* Don't deallocate a contibutor list
		* if the user passed us one of their own.
	*/

		if ((m_Pclist_x) && (!m_clist_x_forced))
		{
			free(m_Pclist_x->p);
			free(m_Pclist_x);
			m_Pclist_x = NULL;
		}

		if ((m_Pclist_y) && (!m_clist_y_forced))
		{
			free(m_Pclist_y->p);
			free(m_Pclist_y);
			m_Pclist_y = NULL;
		}

		free(m_Psrc_y_count);
		m_Psrc_y_count = NULL;

		free(m_Psrc_y_flag);
		m_Psrc_y_flag = NULL;

		if (m_Pscan_buf)
		{
			for (i = 0; i < MAX_SCAN_BUF_SIZE; i++)
				free(m_Pscan_buf->scan_buf_l[i]);

			free(m_Pscan_buf);
			m_Pscan_buf = NULL;
		}
	}

	void Resampler::restart()
	{
		if (STATUS_OKAY != m_status)
			return;

		m_cur_src_y = m_cur_dst_y = 0;

		int i, j;
		for (i = 0; i < m_resample_src_y; i++)
		{
			m_Psrc_y_count[i] = 0;
			m_Psrc_y_flag[i] = false;
		}

		for (i = 0; i < m_resample_dst_y; i++)
		{
			for (j = 0; j < m_Pclist_y[i].n; j++)
				m_Psrc_y_count[resampler_range_check(m_Pclist_y[i].p[j].pixel, m_resample_src_y)]++;
		}

		for (i = 0; i < MAX_SCAN_BUF_SIZE; i++)
		{
			m_Pscan_buf->scan_buf_y[i] = -1;

			free(m_Pscan_buf->scan_buf_l[i]);
			m_Pscan_buf->scan_buf_l[i] = NULL;
		}
	}

	Resampler::Resampler(int src_x, int src_y,
		int dst_x, int dst_y,
		Boundary_Op boundary_op,
		Resample_Real sample_low, Resample_Real sample_high,
		const char* Pfilter_name,
		Contrib_List * Pclist_x,
		Contrib_List * Pclist_y,
		Resample_Real filter_x_scale,
		Resample_Real filter_y_scale,
		Resample_Real src_x_ofs,
		Resample_Real src_y_ofs)
	{
		int i, j;
		Resample_Real support, (*func)(Resample_Real);

		assert(src_x > 0);
		assert(src_y > 0);
		assert(dst_x > 0);
		assert(dst_y > 0);

#if BASISU_RESAMPLER_DEBUG_OPS
		total_ops = 0;
#endif

		m_lo = sample_low;
		m_hi = sample_high;

		m_delay_x_resample = false;
		m_intermediate_x = 0;
		m_Pdst_buf = NULL;
		m_Ptmp_buf = NULL;
		m_clist_x_forced = false;
		m_Pclist_x = NULL;
		m_clist_y_forced = false;
		m_Pclist_y = NULL;
		m_Psrc_y_count = NULL;
		m_Psrc_y_flag = NULL;
		m_Pscan_buf = NULL;
		m_status = STATUS_OKAY;

		m_resample_src_x = src_x;
		m_resample_src_y = src_y;
		m_resample_dst_x = dst_x;
		m_resample_dst_y = dst_y;

		m_boundary_op = boundary_op;

		if ((m_Pdst_buf = (Sample*)malloc(m_resample_dst_x * sizeof(Sample))) == NULL)
		{
			m_status = STATUS_OUT_OF_MEMORY;
			return;
		}

		// Find the specified filter.

		if (Pfilter_name == NULL)
			Pfilter_name = BASISU_RESAMPLER_DEFAULT_FILTER;

		for (i = 0; i < g_num_resample_filters; i++)
			if (strcmp(Pfilter_name, g_resample_filters[i].name) == 0)
				break;

		if (i == g_num_resample_filters)
		{
			m_status = STATUS_BAD_FILTER_NAME;
			return;
		}

		func = g_resample_filters[i].func;
		support = g_resample_filters[i].support;

		/* Create contributor lists, unless the user supplied custom lists. */

		if (!Pclist_x)
		{
			m_Pclist_x = make_clist(m_resample_src_x, m_resample_dst_x, m_boundary_op, func, support, filter_x_scale, src_x_ofs);
			if (!m_Pclist_x)
			{
				m_status = STATUS_OUT_OF_MEMORY;
				return;
			}
		}
		else
		{
			m_Pclist_x = Pclist_x;
			m_clist_x_forced = true;
		}

		if (!Pclist_y)
		{
			m_Pclist_y = make_clist(m_resample_src_y, m_resample_dst_y, m_boundary_op, func, support, filter_y_scale, src_y_ofs);
			if (!m_Pclist_y)
			{
				m_status = STATUS_OUT_OF_MEMORY;
				return;
			}
		}
		else
		{
			m_Pclist_y = Pclist_y;
			m_clist_y_forced = true;
		}

		if ((m_Psrc_y_count = (int*)calloc(m_resample_src_y, sizeof(int))) == NULL)
		{
			m_status = STATUS_OUT_OF_MEMORY;
			return;
		}

		if ((m_Psrc_y_flag = (unsigned char*)calloc(m_resample_src_y, sizeof(unsigned char))) == NULL)
		{
			m_status = STATUS_OUT_OF_MEMORY;
			return;
		}

		// Count how many times each source line contributes to a destination line.

		for (i = 0; i < m_resample_dst_y; i++)
			for (j = 0; j < m_Pclist_y[i].n; j++)
				m_Psrc_y_count[resampler_range_check(m_Pclist_y[i].p[j].pixel, m_resample_src_y)]++;

		if ((m_Pscan_buf = (Scan_Buf*)malloc(sizeof(Scan_Buf))) == NULL)
		{
			m_status = STATUS_OUT_OF_MEMORY;
			return;
		}

		for (i = 0; i < MAX_SCAN_BUF_SIZE; i++)
		{
			m_Pscan_buf->scan_buf_y[i] = -1;
			m_Pscan_buf->scan_buf_l[i] = NULL;
		}

		m_cur_src_y = m_cur_dst_y = 0;
		{
			// Determine which axis to resample first by comparing the number of multiplies required
			// for each possibility.
			int x_ops = count_ops(m_Pclist_x, m_resample_dst_x);
			int y_ops = count_ops(m_Pclist_y, m_resample_dst_y);

			// Hack 10/2000: Weight Y axis ops a little more than X axis ops.
			// (Y axis ops use more cache resources.)
			int xy_ops = x_ops * m_resample_src_y +
				(4 * y_ops * m_resample_dst_x) / 3;

			int yx_ops = (4 * y_ops * m_resample_src_x) / 3 +
				x_ops * m_resample_dst_y;

#if BASISU_RESAMPLER_DEBUG_OPS
			printf("src: %i %i\n", m_resample_src_x, m_resample_src_y);
			printf("dst: %i %i\n", m_resample_dst_x, m_resample_dst_y);
			printf("x_ops: %i\n", x_ops);
			printf("y_ops: %i\n", y_ops);
			printf("xy_ops: %i\n", xy_ops);
			printf("yx_ops: %i\n", yx_ops);
#endif

			// Now check which resample order is better. In case of a tie, choose the order
			// which buffers the least amount of data.
			if ((xy_ops > yx_ops) ||
				((xy_ops == yx_ops) && (m_resample_src_x < m_resample_dst_x)))
			{
				m_delay_x_resample = true;
				m_intermediate_x = m_resample_src_x;
			}
			else
			{
				m_delay_x_resample = false;
				m_intermediate_x = m_resample_dst_x;
			}
#if BASISU_RESAMPLER_DEBUG_OPS
			printf("delaying: %i\n", m_delay_x_resample);
#endif
		}

		if (m_delay_x_resample)
		{
			if ((m_Ptmp_buf = (Sample*)malloc(m_intermediate_x * sizeof(Sample))) == NULL)
			{
				m_status = STATUS_OUT_OF_MEMORY;
				return;
			}
		}
	}

	void Resampler::get_clists(Contrib_List * *ptr_clist_x, Contrib_List * *ptr_clist_y)
	{
		if (ptr_clist_x)
			* ptr_clist_x = m_Pclist_x;

		if (ptr_clist_y)
			* ptr_clist_y = m_Pclist_y;
	}

	int Resampler::get_filter_num()
	{
		return g_num_resample_filters;
	}

	const char* Resampler::get_filter_name(int filter_num)
	{
		if ((filter_num < 0) || (filter_num >= g_num_resample_filters))
			return NULL;
		else
			return g_resample_filters[filter_num].name;
	}
	
} // namespace basisu

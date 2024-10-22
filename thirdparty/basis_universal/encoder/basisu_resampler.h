// basisu_resampler.h
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

#define BASISU_RESAMPLER_DEBUG_OPS (0)
#define BASISU_RESAMPLER_DEFAULT_FILTER "lanczos4"
#define BASISU_RESAMPLER_MAX_DIMENSION (16384)

namespace basisu
{
	// float or double
	typedef float Resample_Real;

	class Resampler
	{
	public:
		typedef Resample_Real Sample;

		struct Contrib
		{
			Resample_Real weight;
			uint16_t pixel;
		};

		struct Contrib_List
		{
			uint16_t n;
			Contrib *p;
		};

		enum Boundary_Op
		{
			BOUNDARY_WRAP = 0,
			BOUNDARY_REFLECT = 1,
			BOUNDARY_CLAMP = 2
		};

		enum Status
		{
			STATUS_OKAY = 0,
			STATUS_OUT_OF_MEMORY = 1,
			STATUS_BAD_FILTER_NAME = 2,
			STATUS_SCAN_BUFFER_FULL = 3
		};

		// src_x/src_y - Input dimensions
		// dst_x/dst_y - Output dimensions
		// boundary_op - How to sample pixels near the image boundaries
		// sample_low/sample_high - Clamp output samples to specified range, or disable clamping if sample_low >= sample_high
		// Pclist_x/Pclist_y - Optional pointers to contributor lists from another instance of a Resampler
		// src_x_ofs/src_y_ofs - Offset input image by specified amount (fractional values okay)
		Resampler(
			int src_x, int src_y,
			int dst_x, int dst_y,
			Boundary_Op boundary_op = BOUNDARY_CLAMP,
			Resample_Real sample_low = 0.0f, Resample_Real sample_high = 0.0f,
			const char *Pfilter_name = BASISU_RESAMPLER_DEFAULT_FILTER,
			Contrib_List *Pclist_x = NULL,
			Contrib_List *Pclist_y = NULL,
			Resample_Real filter_x_scale = 1.0f,
			Resample_Real filter_y_scale = 1.0f,
			Resample_Real src_x_ofs = 0.0f,
			Resample_Real src_y_ofs = 0.0f);

		~Resampler();

		// Reinits resampler so it can handle another frame.
		void restart();

		// false on out of memory.
		bool put_line(const Sample *Psrc);

		// NULL if no scanlines are currently available (give the resampler more scanlines!)
		const Sample *get_line();

		Status status() const
		{
			return m_status;
		}

		// Returned contributor lists can be shared with another Resampler.
		void get_clists(Contrib_List **ptr_clist_x, Contrib_List **ptr_clist_y);
		Contrib_List *get_clist_x() const
		{
			return m_Pclist_x;
		}
		Contrib_List *get_clist_y() const
		{
			return m_Pclist_y;
		}

		// Filter accessors.
		static int get_filter_num();
		static const char *get_filter_name(int filter_num);

		static Contrib_List *make_clist(
			int src_x, int dst_x, Boundary_Op boundary_op,
			Resample_Real(*Pfilter)(Resample_Real),
			Resample_Real filter_support,
			Resample_Real filter_scale,
			Resample_Real src_ofs);

	private:
		Resampler();
		Resampler(const Resampler &o);
		Resampler &operator=(const Resampler &o);

#ifdef BASISU_RESAMPLER_DEBUG_OPS
		int total_ops;
#endif

		int m_intermediate_x;

		int m_resample_src_x;
		int m_resample_src_y;
		int m_resample_dst_x;
		int m_resample_dst_y;

		Boundary_Op m_boundary_op;

		Sample *m_Pdst_buf;
		Sample *m_Ptmp_buf;

		Contrib_List *m_Pclist_x;
		Contrib_List *m_Pclist_y;

		bool m_clist_x_forced;
		bool m_clist_y_forced;

		bool m_delay_x_resample;

		int *m_Psrc_y_count;
		uint8_t *m_Psrc_y_flag;

		// The maximum number of scanlines that can be buffered at one time.
		enum
		{
			MAX_SCAN_BUF_SIZE = BASISU_RESAMPLER_MAX_DIMENSION
		};

		struct Scan_Buf
		{
			int scan_buf_y[MAX_SCAN_BUF_SIZE];
			Sample *scan_buf_l[MAX_SCAN_BUF_SIZE];
		};

		Scan_Buf *m_Pscan_buf;

		int m_cur_src_y;
		int m_cur_dst_y;

		Status m_status;

		void resample_x(Sample *Pdst, const Sample *Psrc);
		void scale_y_mov(Sample *Ptmp, const Sample *Psrc, Resample_Real weight, int dst_x);
		void scale_y_add(Sample *Ptmp, const Sample *Psrc, Resample_Real weight, int dst_x);
		void clamp(Sample *Pdst, int n);
		void resample_y(Sample *Pdst);

		static int reflect(const int j, const int src_x, const Boundary_Op boundary_op);

		inline int count_ops(Contrib_List *Pclist, int k)
		{
			int i, t = 0;
			for (i = 0; i < k; i++)
				t += Pclist[i].n;
			return (t);
		}

		Resample_Real m_lo;
		Resample_Real m_hi;

		inline Resample_Real clamp_sample(Resample_Real f) const
		{
			if (f < m_lo)
				f = m_lo;
			else if (f > m_hi)
				f = m_hi;
			return f;
		}
	};

} // namespace basisu

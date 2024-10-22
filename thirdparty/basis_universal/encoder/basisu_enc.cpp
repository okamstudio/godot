// basisu_enc.cpp
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
#include "basisu_enc.h"
#include "basisu_resampler.h"
#include "basisu_resampler_filters.h"
#include "basisu_etc.h"
#include "../transcoder/basisu_transcoder.h"
#include "basisu_bc7enc.h"
#include "jpgd.h"
#include "pvpngreader.h"
#include "basisu_opencl.h"
#include "basisu_astc_hdr_enc.h"
#include <vector>

#ifndef TINYEXR_USE_ZFP
#define TINYEXR_USE_ZFP (1)
#endif
#include <tinyexr.h>

#ifndef MINIZ_HEADER_FILE_ONLY
#define MINIZ_HEADER_FILE_ONLY
#endif
#ifndef MINIZ_NO_ZLIB_COMPATIBLE_NAMES
#define MINIZ_NO_ZLIB_COMPATIBLE_NAMES
#endif
#include "basisu_miniz.h"

#if defined(_WIN32)
// For QueryPerformanceCounter/QueryPerformanceFrequency
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace basisu
{
	uint64_t interval_timer::g_init_ticks, interval_timer::g_freq;
	double interval_timer::g_timer_freq;
#if BASISU_SUPPORT_SSE
	bool g_cpu_supports_sse41;
#endif

	uint8_t g_hamming_dist[256] =
	{
		0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4,
		1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
		1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
		2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
		1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
		2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
		2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
		3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
		1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
		2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
		2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
		3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
		2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
		3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
		3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
		4, 5, 5, 6, 5, 6, 6, 7, 5, 6, 6, 7, 6, 7, 7, 8
	};

	// This is a Public Domain 8x8 font from here:
	// https://github.com/dhepper/font8x8/blob/master/font8x8_basic.h
	const uint8_t g_debug_font8x8_basic[127 - 32 + 1][8] = 
	{
	 { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},	// U+0020 ( )
	 { 0x18, 0x3C, 0x3C, 0x18, 0x18, 0x00, 0x18, 0x00},   // U+0021 (!)
	 { 0x36, 0x36, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},   // U+0022 (")
	 { 0x36, 0x36, 0x7F, 0x36, 0x7F, 0x36, 0x36, 0x00},   // U+0023 (#)
	 { 0x0C, 0x3E, 0x03, 0x1E, 0x30, 0x1F, 0x0C, 0x00},   // U+0024 ($)
	 { 0x00, 0x63, 0x33, 0x18, 0x0C, 0x66, 0x63, 0x00},   // U+0025 (%)
	 { 0x1C, 0x36, 0x1C, 0x6E, 0x3B, 0x33, 0x6E, 0x00},   // U+0026 (&)
	 { 0x06, 0x06, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00},   // U+0027 (')
	 { 0x18, 0x0C, 0x06, 0x06, 0x06, 0x0C, 0x18, 0x00},   // U+0028 (()
	 { 0x06, 0x0C, 0x18, 0x18, 0x18, 0x0C, 0x06, 0x00},   // U+0029 ())
	 { 0x00, 0x66, 0x3C, 0xFF, 0x3C, 0x66, 0x00, 0x00},   // U+002A (*)
	 { 0x00, 0x0C, 0x0C, 0x3F, 0x0C, 0x0C, 0x00, 0x00},   // U+002B (+)
	 { 0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C, 0x06},   // U+002C (,)
	 { 0x00, 0x00, 0x00, 0x3F, 0x00, 0x00, 0x00, 0x00},   // U+002D (-)
	 { 0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C, 0x00},   // U+002E (.)
	 { 0x60, 0x30, 0x18, 0x0C, 0x06, 0x03, 0x01, 0x00},   // U+002F (/)
	 { 0x3E, 0x63, 0x73, 0x7B, 0x6F, 0x67, 0x3E, 0x00},   // U+0030 (0)
	 { 0x0C, 0x0E, 0x0C, 0x0C, 0x0C, 0x0C, 0x3F, 0x00},   // U+0031 (1)
	 { 0x1E, 0x33, 0x30, 0x1C, 0x06, 0x33, 0x3F, 0x00},   // U+0032 (2)
	 { 0x1E, 0x33, 0x30, 0x1C, 0x30, 0x33, 0x1E, 0x00},   // U+0033 (3)
	 { 0x38, 0x3C, 0x36, 0x33, 0x7F, 0x30, 0x78, 0x00},   // U+0034 (4)
	 { 0x3F, 0x03, 0x1F, 0x30, 0x30, 0x33, 0x1E, 0x00},   // U+0035 (5)
	 { 0x1C, 0x06, 0x03, 0x1F, 0x33, 0x33, 0x1E, 0x00},   // U+0036 (6)
	 { 0x3F, 0x33, 0x30, 0x18, 0x0C, 0x0C, 0x0C, 0x00},   // U+0037 (7)
	 { 0x1E, 0x33, 0x33, 0x1E, 0x33, 0x33, 0x1E, 0x00},   // U+0038 (8)
	 { 0x1E, 0x33, 0x33, 0x3E, 0x30, 0x18, 0x0E, 0x00},   // U+0039 (9)
	 { 0x00, 0x0C, 0x0C, 0x00, 0x00, 0x0C, 0x0C, 0x00},   // U+003A (:)
	 { 0x00, 0x0C, 0x0C, 0x00, 0x00, 0x0C, 0x0C, 0x06},   // U+003B (;)
	 { 0x18, 0x0C, 0x06, 0x03, 0x06, 0x0C, 0x18, 0x00},   // U+003C (<)
	 { 0x00, 0x00, 0x3F, 0x00, 0x00, 0x3F, 0x00, 0x00},   // U+003D (=)
	 { 0x06, 0x0C, 0x18, 0x30, 0x18, 0x0C, 0x06, 0x00},   // U+003E (>)
	 { 0x1E, 0x33, 0x30, 0x18, 0x0C, 0x00, 0x0C, 0x00},   // U+003F (?)
	 { 0x3E, 0x63, 0x7B, 0x7B, 0x7B, 0x03, 0x1E, 0x00},   // U+0040 (@)
	 { 0x0C, 0x1E, 0x33, 0x33, 0x3F, 0x33, 0x33, 0x00},   // U+0041 (A)
	 { 0x3F, 0x66, 0x66, 0x3E, 0x66, 0x66, 0x3F, 0x00},   // U+0042 (B)
	 { 0x3C, 0x66, 0x03, 0x03, 0x03, 0x66, 0x3C, 0x00},   // U+0043 (C)
	 { 0x1F, 0x36, 0x66, 0x66, 0x66, 0x36, 0x1F, 0x00},   // U+0044 (D)
	 { 0x7F, 0x46, 0x16, 0x1E, 0x16, 0x46, 0x7F, 0x00},   // U+0045 (E)
	 { 0x7F, 0x46, 0x16, 0x1E, 0x16, 0x06, 0x0F, 0x00},   // U+0046 (F)
	 { 0x3C, 0x66, 0x03, 0x03, 0x73, 0x66, 0x7C, 0x00},   // U+0047 (G)
	 { 0x33, 0x33, 0x33, 0x3F, 0x33, 0x33, 0x33, 0x00},   // U+0048 (H)
	 { 0x1E, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x1E, 0x00},   // U+0049 (I)
	 { 0x78, 0x30, 0x30, 0x30, 0x33, 0x33, 0x1E, 0x00},   // U+004A (J)
	 { 0x67, 0x66, 0x36, 0x1E, 0x36, 0x66, 0x67, 0x00},   // U+004B (K)
	 { 0x0F, 0x06, 0x06, 0x06, 0x46, 0x66, 0x7F, 0x00},   // U+004C (L)
	 { 0x63, 0x77, 0x7F, 0x7F, 0x6B, 0x63, 0x63, 0x00},   // U+004D (M)
	 { 0x63, 0x67, 0x6F, 0x7B, 0x73, 0x63, 0x63, 0x00},   // U+004E (N)
	 { 0x1C, 0x36, 0x63, 0x63, 0x63, 0x36, 0x1C, 0x00},   // U+004F (O)
	 { 0x3F, 0x66, 0x66, 0x3E, 0x06, 0x06, 0x0F, 0x00},   // U+0050 (P)
	 { 0x1E, 0x33, 0x33, 0x33, 0x3B, 0x1E, 0x38, 0x00},   // U+0051 (Q)
	 { 0x3F, 0x66, 0x66, 0x3E, 0x36, 0x66, 0x67, 0x00},   // U+0052 (R)
	 { 0x1E, 0x33, 0x07, 0x0E, 0x38, 0x33, 0x1E, 0x00},   // U+0053 (S)
	 { 0x3F, 0x2D, 0x0C, 0x0C, 0x0C, 0x0C, 0x1E, 0x00},   // U+0054 (T)
	 { 0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x3F, 0x00},   // U+0055 (U)
	 { 0x33, 0x33, 0x33, 0x33, 0x33, 0x1E, 0x0C, 0x00},   // U+0056 (V)
	 { 0x63, 0x63, 0x63, 0x6B, 0x7F, 0x77, 0x63, 0x00},   // U+0057 (W)
	 { 0x63, 0x63, 0x36, 0x1C, 0x1C, 0x36, 0x63, 0x00},   // U+0058 (X)
	 { 0x33, 0x33, 0x33, 0x1E, 0x0C, 0x0C, 0x1E, 0x00},   // U+0059 (Y)
	 { 0x7F, 0x63, 0x31, 0x18, 0x4C, 0x66, 0x7F, 0x00},   // U+005A (Z)
	 { 0x1E, 0x06, 0x06, 0x06, 0x06, 0x06, 0x1E, 0x00},   // U+005B ([)
	 { 0x03, 0x06, 0x0C, 0x18, 0x30, 0x60, 0x40, 0x00},   // U+005C (\)
	 { 0x1E, 0x18, 0x18, 0x18, 0x18, 0x18, 0x1E, 0x00},   // U+005D (])
	 { 0x08, 0x1C, 0x36, 0x63, 0x00, 0x00, 0x00, 0x00},   // U+005E (^)
	 { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF},   // U+005F (_)
	 { 0x0C, 0x0C, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00},   // U+0060 (`)
	 { 0x00, 0x00, 0x1E, 0x30, 0x3E, 0x33, 0x6E, 0x00},   // U+0061 (a)
	 { 0x07, 0x06, 0x06, 0x3E, 0x66, 0x66, 0x3B, 0x00},   // U+0062 (b)
	 { 0x00, 0x00, 0x1E, 0x33, 0x03, 0x33, 0x1E, 0x00},   // U+0063 (c)
	 { 0x38, 0x30, 0x30, 0x3e, 0x33, 0x33, 0x6E, 0x00},   // U+0064 (d)
	 { 0x00, 0x00, 0x1E, 0x33, 0x3f, 0x03, 0x1E, 0x00},   // U+0065 (e)
	 { 0x1C, 0x36, 0x06, 0x0f, 0x06, 0x06, 0x0F, 0x00},   // U+0066 (f)
	 { 0x00, 0x00, 0x6E, 0x33, 0x33, 0x3E, 0x30, 0x1F},   // U+0067 (g)
	 { 0x07, 0x06, 0x36, 0x6E, 0x66, 0x66, 0x67, 0x00},   // U+0068 (h)
	 { 0x0C, 0x00, 0x0E, 0x0C, 0x0C, 0x0C, 0x1E, 0x00},   // U+0069 (i)
	 { 0x30, 0x00, 0x30, 0x30, 0x30, 0x33, 0x33, 0x1E},   // U+006A (j)
	 { 0x07, 0x06, 0x66, 0x36, 0x1E, 0x36, 0x67, 0x00},   // U+006B (k)
	 { 0x0E, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x1E, 0x00},   // U+006C (l)
	 { 0x00, 0x00, 0x33, 0x7F, 0x7F, 0x6B, 0x63, 0x00},   // U+006D (m)
	 { 0x00, 0x00, 0x1F, 0x33, 0x33, 0x33, 0x33, 0x00},   // U+006E (n)
	 { 0x00, 0x00, 0x1E, 0x33, 0x33, 0x33, 0x1E, 0x00},   // U+006F (o)
	 { 0x00, 0x00, 0x3B, 0x66, 0x66, 0x3E, 0x06, 0x0F},   // U+0070 (p)
	 { 0x00, 0x00, 0x6E, 0x33, 0x33, 0x3E, 0x30, 0x78},   // U+0071 (q)
	 { 0x00, 0x00, 0x3B, 0x6E, 0x66, 0x06, 0x0F, 0x00},   // U+0072 (r)
	 { 0x00, 0x00, 0x3E, 0x03, 0x1E, 0x30, 0x1F, 0x00},   // U+0073 (s)
	 { 0x08, 0x0C, 0x3E, 0x0C, 0x0C, 0x2C, 0x18, 0x00},   // U+0074 (t)
	 { 0x00, 0x00, 0x33, 0x33, 0x33, 0x33, 0x6E, 0x00},   // U+0075 (u)
	 { 0x00, 0x00, 0x33, 0x33, 0x33, 0x1E, 0x0C, 0x00},   // U+0076 (v)
	 { 0x00, 0x00, 0x63, 0x6B, 0x7F, 0x7F, 0x36, 0x00},   // U+0077 (w)
	 { 0x00, 0x00, 0x63, 0x36, 0x1C, 0x36, 0x63, 0x00},   // U+0078 (x)
	 { 0x00, 0x00, 0x33, 0x33, 0x33, 0x3E, 0x30, 0x1F},   // U+0079 (y)
	 { 0x00, 0x00, 0x3F, 0x19, 0x0C, 0x26, 0x3F, 0x00},   // U+007A (z)
	 { 0x38, 0x0C, 0x0C, 0x07, 0x0C, 0x0C, 0x38, 0x00},   // U+007B ({)
	 { 0x18, 0x18, 0x18, 0x00, 0x18, 0x18, 0x18, 0x00},   // U+007C (|)
	 { 0x07, 0x0C, 0x0C, 0x38, 0x0C, 0x0C, 0x07, 0x00},   // U+007D (})
	 { 0x6E, 0x3B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},   // U+007E (~)
	 { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}    // U+007F
	};

	bool g_library_initialized;
	std::mutex g_encoder_init_mutex;
		
	// Encoder library initialization (just call once at startup)
	bool basisu_encoder_init(bool use_opencl, bool opencl_force_serialization)
	{
		std::lock_guard<std::mutex> lock(g_encoder_init_mutex);

		if (g_library_initialized)
			return true;

		detect_sse41();

		basist::basisu_transcoder_init();
		pack_etc1_solid_color_init();
		//uastc_init();
		bc7enc_compress_block_init(); // must be after uastc_init()

		// Don't bother initializing the OpenCL module at all if it's been completely disabled.
		if (use_opencl)
		{
			opencl_init(opencl_force_serialization);
		}

		interval_timer::init(); // make sure interval_timer globals are initialized from main thread to avoid TSAN reports

		astc_hdr_enc_init();
		basist::bc6h_enc_init();

		g_library_initialized = true;
		return true;
	}

	void basisu_encoder_deinit()
	{
		opencl_deinit();

		g_library_initialized = false;
	}

	void error_vprintf(const char* pFmt, va_list args)
	{
		char buf[8192];

#ifdef _WIN32		
		vsprintf_s(buf, sizeof(buf), pFmt, args);
#else
		vsnprintf(buf, sizeof(buf), pFmt, args);
#endif

		fprintf(stderr, "ERROR: %s", buf);
	}

	void error_printf(const char *pFmt, ...)
	{
		va_list args;
		va_start(args, pFmt);
		error_vprintf(pFmt, args);
		va_end(args);
	}

#if defined(_WIN32)
	inline void query_counter(timer_ticks* pTicks)
	{
		QueryPerformanceCounter(reinterpret_cast<LARGE_INTEGER*>(pTicks));
	}
	inline void query_counter_frequency(timer_ticks* pTicks)
	{
		QueryPerformanceFrequency(reinterpret_cast<LARGE_INTEGER*>(pTicks));
	}
#elif defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__EMSCRIPTEN__)
#include <sys/time.h>
	inline void query_counter(timer_ticks* pTicks)
	{
		struct timeval cur_time;
		gettimeofday(&cur_time, NULL);
		*pTicks = static_cast<unsigned long long>(cur_time.tv_sec) * 1000000ULL + static_cast<unsigned long long>(cur_time.tv_usec);
	}
	inline void query_counter_frequency(timer_ticks* pTicks)
	{
		*pTicks = 1000000;
	}
#elif defined(__GNUC__)
#include <sys/timex.h>
	inline void query_counter(timer_ticks* pTicks)
	{
		struct timeval cur_time;
		gettimeofday(&cur_time, NULL);
		*pTicks = static_cast<unsigned long long>(cur_time.tv_sec) * 1000000ULL + static_cast<unsigned long long>(cur_time.tv_usec);
	}
	inline void query_counter_frequency(timer_ticks* pTicks)
	{
		*pTicks = 1000000;
	}
#else
#error TODO
#endif
				
	interval_timer::interval_timer() : m_start_time(0), m_stop_time(0), m_started(false), m_stopped(false)
	{
		if (!g_timer_freq)
			init();
	}

	void interval_timer::start()
	{
		query_counter(&m_start_time);
		m_started = true;
		m_stopped = false;
	}

	void interval_timer::stop()
	{
		assert(m_started);
		query_counter(&m_stop_time);
		m_stopped = true;
	}

	double interval_timer::get_elapsed_secs() const
	{
		assert(m_started);
		if (!m_started)
			return 0;

		timer_ticks stop_time = m_stop_time;
		if (!m_stopped)
			query_counter(&stop_time);

		timer_ticks delta = stop_time - m_start_time;
		return delta * g_timer_freq;
	}
		
	void interval_timer::init()
	{
		if (!g_timer_freq)
		{
			query_counter_frequency(&g_freq);
			g_timer_freq = 1.0f / g_freq;
			query_counter(&g_init_ticks);
		}
	}

	timer_ticks interval_timer::get_ticks()
	{
		if (!g_timer_freq)
			init();
		timer_ticks ticks;
		query_counter(&ticks);
		return ticks - g_init_ticks;
	}

	double interval_timer::ticks_to_secs(timer_ticks ticks)
	{
		if (!g_timer_freq)
			init();
		return ticks * g_timer_freq;
	}

	float linear_to_srgb(float l)
	{
		assert(l >= 0.0f && l <= 1.0f);
		if (l < .0031308f)
			return saturate(l * 12.92f);
		else
			return saturate(1.055f * powf(l, 1.0f / 2.4f) - .055f);
	}

	float srgb_to_linear(float s)
	{
		assert(s >= 0.0f && s <= 1.0f);
		if (s < .04045f)
			return saturate(s * (1.0f / 12.92f));
		else
			return saturate(powf((s + .055f) * (1.0f / 1.055f), 2.4f));
	}
		
	const uint32_t MAX_32BIT_ALLOC_SIZE = 250000000;
		
	bool load_tga(const char* pFilename, image& img)
	{
		int w = 0, h = 0, n_chans = 0;
		uint8_t* pImage_data = read_tga(pFilename, w, h, n_chans);
				
		if ((!pImage_data) || (!w) || (!h) || ((n_chans != 3) && (n_chans != 4)))
		{
			error_printf("Failed loading .TGA image \"%s\"!\n", pFilename);

			if (pImage_data)
				free(pImage_data);
						
			return false;
		}

		if (sizeof(void *) == sizeof(uint32_t))
		{
			if (((uint64_t)w * h * n_chans) > MAX_32BIT_ALLOC_SIZE)
			{
				error_printf("Image \"%s\" is too large (%ux%u) to process in a 32-bit build!\n", pFilename, w, h);

				if (pImage_data)
					free(pImage_data);

				return false;
			}
		}
		
		img.resize(w, h);

		const uint8_t *pSrc = pImage_data;
		for (int y = 0; y < h; y++)
		{
			color_rgba *pDst = &img(0, y);

			for (int x = 0; x < w; x++)
			{
				pDst->r = pSrc[0];
				pDst->g = pSrc[1];
				pDst->b = pSrc[2];
				pDst->a = (n_chans == 3) ? 255 : pSrc[3];

				pSrc += n_chans;
				++pDst;
			}
		}

		free(pImage_data);

		return true;
	}

	bool load_qoi(const char* pFilename, image& img)
	{
		return false;
	}

	bool load_png(const uint8_t *pBuf, size_t buf_size, image &img, const char *pFilename)
	{
		interval_timer tm;
		tm.start();
		
		if (!buf_size)
			return false;

		uint32_t width = 0, height = 0, num_chans = 0;
		void* pImage = pv_png::load_png(pBuf, buf_size, 4, width, height, num_chans);
		if (!pBuf)
		{
			error_printf("pv_png::load_png failed while loading image \"%s\"\n", pFilename);
			return false;
		}

		img.grant_ownership(reinterpret_cast<color_rgba*>(pImage), width, height);

		//debug_printf("Total load_png() time: %3.3f secs\n", tm.get_elapsed_secs());

		return true;
	}
		
	bool load_png(const char* pFilename, image& img)
	{
		uint8_vec buffer;
		if (!read_file_to_vec(pFilename, buffer))
		{
			error_printf("load_png: Failed reading file \"%s\"!\n", pFilename);
			return false;
		}

		return load_png(buffer.data(), buffer.size(), img, pFilename);
	}

	bool load_jpg(const char *pFilename, image& img)
	{
		int width = 0, height = 0, actual_comps = 0;
		uint8_t *pImage_data = jpgd::decompress_jpeg_image_from_file(pFilename, &width, &height, &actual_comps, 4, jpgd::jpeg_decoder::cFlagBoxChromaFiltering);
		if (!pImage_data)
			return false;
		
		img.init(pImage_data, width, height, 4);
		
		free(pImage_data);

		return true;
	}

	bool load_image(const char* pFilename, image& img)
	{
		std::string ext(string_get_extension(std::string(pFilename)));

		if (ext.length() == 0)
			return false;

		const char *pExt = ext.c_str();

		if (strcasecmp(pExt, "png") == 0)
			return load_png(pFilename, img);
		if (strcasecmp(pExt, "tga") == 0)
			return load_tga(pFilename, img);
		if (strcasecmp(pExt, "qoi") == 0)
			return load_qoi(pFilename, img);
		if ( (strcasecmp(pExt, "jpg") == 0) || (strcasecmp(pExt, "jfif") == 0) || (strcasecmp(pExt, "jpeg") == 0) )
			return load_jpg(pFilename, img);

		return false;
	}

	static void convert_ldr_to_hdr_image(imagef &img, const image &ldr_img, bool ldr_srgb_to_linear)
	{
		img.resize(ldr_img.get_width(), ldr_img.get_height());

		for (uint32_t y = 0; y < ldr_img.get_height(); y++)
		{
			for (uint32_t x = 0; x < ldr_img.get_width(); x++)
			{
				const color_rgba& c = ldr_img(x, y);

				vec4F& d = img(x, y);
				if (ldr_srgb_to_linear)
				{
					// TODO: Multiply by 100-200 nits?
					d[0] = srgb_to_linear(c[0] * (1.0f / 255.0f));
					d[1] = srgb_to_linear(c[1] * (1.0f / 255.0f));
					d[2] = srgb_to_linear(c[2] * (1.0f / 255.0f));
				}
				else
				{
					d[0] = c[0] * (1.0f / 255.0f);
					d[1] = c[1] * (1.0f / 255.0f);
					d[2] = c[2] * (1.0f / 255.0f);
				}
				d[3] = c[3] * (1.0f / 255.0f);
			}
		}
	}

	bool load_image_hdr(const void* pMem, size_t mem_size, imagef& img, uint32_t width, uint32_t height, hdr_image_type img_type, bool ldr_srgb_to_linear)
	{
		if ((!pMem) || (!mem_size))
		{
			assert(0);
			return false;
		}

		switch (img_type)
		{
		case hdr_image_type::cHITRGBAHalfFloat:
		{
			if (mem_size != width * height * sizeof(basist::half_float) * 4)
			{
				assert(0);
				return false;
			}

			if ((!width) || (!height))
			{
				assert(0);
				return false;
			}

			const basist::half_float* pSrc_image_h = static_cast<const basist::half_float *>(pMem);

			img.resize(width, height);
			for (uint32_t y = 0; y < height; y++)
			{
				for (uint32_t x = 0; x < width; x++)
				{
					const basist::half_float* pSrc_pixel = &pSrc_image_h[x * 4];

					vec4F& dst = img(x, y);
					dst[0] = basist::half_to_float(pSrc_pixel[0]);
					dst[1] = basist::half_to_float(pSrc_pixel[1]);
					dst[2] = basist::half_to_float(pSrc_pixel[2]);
					dst[3] = basist::half_to_float(pSrc_pixel[3]);
				}
			
				pSrc_image_h += (width * 4);
			}

			break;
		}
		case hdr_image_type::cHITRGBAFloat:
		{
			if (mem_size != width * height * sizeof(float) * 4)
			{
				assert(0);
				return false;
			}

			if ((!width) || (!height))
			{
				assert(0);
				return false;
			}

			img.resize(width, height);
			memcpy(img.get_ptr(), pMem, width * height * sizeof(float) * 4);

			break;
		}
		case hdr_image_type::cHITPNGImage:
		{
			image ldr_img;
			if (!load_png(static_cast<const uint8_t *>(pMem), mem_size, ldr_img))
				return false;

			convert_ldr_to_hdr_image(img, ldr_img, ldr_srgb_to_linear);
			break;
		}
		case hdr_image_type::cHITEXRImage:
		{
			if (!read_exr(pMem, mem_size, img))
				return false;

			break;
		}
		case hdr_image_type::cHITHDRImage:
		{
			uint8_vec buf(mem_size);
			memcpy(buf.get_ptr(), pMem, mem_size);

			rgbe_header_info hdr;
			if (!read_rgbe(buf, img, hdr))
				return false;

			break;
		}
		default:
			assert(0);
			return false;
		}

		return true;
	}
	
	bool load_image_hdr(const char* pFilename, imagef& img, bool ldr_srgb_to_linear)
	{
		std::string ext(string_get_extension(std::string(pFilename)));

		if (ext.length() == 0)
			return false;

		const char* pExt = ext.c_str();

		if (strcasecmp(pExt, "hdr") == 0)
		{
			rgbe_header_info rgbe_info;
			if (!read_rgbe(pFilename, img, rgbe_info))
				return false;
			return true;
		}
					
		if (strcasecmp(pExt, "exr") == 0)
		{
			int n_chans = 0;
			if (!read_exr(pFilename, img, n_chans))
				return false;
			return true;
		}

		// Try loading image as LDR, then optionally convert to linear light.
		{
			image ldr_img;
			if (!load_image(pFilename, ldr_img))
				return false;

			convert_ldr_to_hdr_image(img, ldr_img, ldr_srgb_to_linear);
		}

		return true;
	}
	
	bool save_png(const char* pFilename, const image &img, uint32_t image_save_flags, uint32_t grayscale_comp)
	{
		if (!img.get_total_pixels())
			return false;
				
		void* pPNG_data = nullptr;
		size_t PNG_data_size = 0;

		if (image_save_flags & cImageSaveGrayscale)
		{
			uint8_vec g_pixels(img.get_total_pixels());
			uint8_t* pDst = &g_pixels[0];

			for (uint32_t y = 0; y < img.get_height(); y++)
				for (uint32_t x = 0; x < img.get_width(); x++)
					*pDst++ = img(x, y)[grayscale_comp];

			pPNG_data = buminiz::tdefl_write_image_to_png_file_in_memory_ex(g_pixels.data(), img.get_width(), img.get_height(), 1, &PNG_data_size, 1, false);
		}
		else
		{
			bool has_alpha = false;
			
			if ((image_save_flags & cImageSaveIgnoreAlpha) == 0)
				has_alpha = img.has_alpha();

			if (!has_alpha)
			{
				uint8_vec rgb_pixels(img.get_total_pixels() * 3);
				uint8_t* pDst = &rgb_pixels[0];

				for (uint32_t y = 0; y < img.get_height(); y++)
				{
					const color_rgba* pSrc = &img(0, y);
					for (uint32_t x = 0; x < img.get_width(); x++)
					{
						pDst[0] = pSrc->r;
						pDst[1] = pSrc->g;
						pDst[2] = pSrc->b;
						
						pSrc++;
						pDst += 3;
					}
				}

				pPNG_data = buminiz::tdefl_write_image_to_png_file_in_memory_ex(rgb_pixels.data(), img.get_width(), img.get_height(), 3, &PNG_data_size, 1, false);
			}
			else
			{
				pPNG_data = buminiz::tdefl_write_image_to_png_file_in_memory_ex(img.get_ptr(), img.get_width(), img.get_height(), 4, &PNG_data_size, 1, false);
			}
		}

		if (!pPNG_data)
			return false;

		bool status = write_data_to_file(pFilename, pPNG_data, PNG_data_size);
		if (!status)
		{
			error_printf("save_png: Failed writing to filename \"%s\"!\n", pFilename);
		}

		free(pPNG_data);
						
		return status;
	}
		
	bool read_file_to_vec(const char* pFilename, uint8_vec& data)
	{
		FILE* pFile = nullptr;
#ifdef _WIN32
		fopen_s(&pFile, pFilename, "rb");
#else
		pFile = fopen(pFilename, "rb");
#endif
		if (!pFile)
			return false;
				
		fseek(pFile, 0, SEEK_END);
#ifdef _WIN32
		int64_t filesize = _ftelli64(pFile);
#else
		int64_t filesize = ftello(pFile);
#endif
		if (filesize < 0)
		{
			fclose(pFile);
			return false;
		}
		fseek(pFile, 0, SEEK_SET);

		if (sizeof(size_t) == sizeof(uint32_t))
		{
			if (filesize > 0x70000000)
			{
				// File might be too big to load safely in one alloc
				fclose(pFile);
				return false;
			}
		}

		if (!data.try_resize((size_t)filesize))
		{
			fclose(pFile);
			return false;
		}

		if (filesize)
		{
			if (fread(&data[0], 1, (size_t)filesize, pFile) != (size_t)filesize)
			{
				fclose(pFile);
				return false;
			}
		}

		fclose(pFile);
		return true;
	}

	bool read_file_to_data(const char* pFilename, void *pData, size_t len)
	{
		assert(pData && len);
		if ((!pData) || (!len))
			return false;

		FILE* pFile = nullptr;
#ifdef _WIN32
		fopen_s(&pFile, pFilename, "rb");
#else
		pFile = fopen(pFilename, "rb");
#endif
		if (!pFile)
			return false;

		fseek(pFile, 0, SEEK_END);
#ifdef _WIN32
		int64_t filesize = _ftelli64(pFile);
#else
		int64_t filesize = ftello(pFile);
#endif

		if ((filesize < 0) || ((size_t)filesize < len))
		{
			fclose(pFile);
			return false;
		}
		fseek(pFile, 0, SEEK_SET);
				
		if (fread(pData, 1, (size_t)len, pFile) != (size_t)len)
		{
			fclose(pFile);
			return false;
		}

		fclose(pFile);
		return true;
	}

	bool write_data_to_file(const char* pFilename, const void* pData, size_t len)
	{
		FILE* pFile = nullptr;
#ifdef _WIN32
		fopen_s(&pFile, pFilename, "wb");
#else
		pFile = fopen(pFilename, "wb");
#endif
		if (!pFile)
			return false;

		if (len)
		{
			if (fwrite(pData, 1, len, pFile) != len)
			{
				fclose(pFile);
				return false;
			}
		}

		return fclose(pFile) != EOF;
	}
		
	bool image_resample(const image &src, image &dst, bool srgb,
		const char *pFilter, float filter_scale, 
		bool wrapping,
		uint32_t first_comp, uint32_t num_comps)
	{
		assert((first_comp + num_comps) <= 4);

		const int cMaxComps = 4;
				
		const uint32_t src_w = src.get_width(), src_h = src.get_height();
		const uint32_t dst_w = dst.get_width(), dst_h = dst.get_height();
				
		if (maximum(src_w, src_h) > BASISU_RESAMPLER_MAX_DIMENSION)
		{
			printf("Image is too large!\n");
			return false;
		}

		if (!src_w || !src_h || !dst_w || !dst_h)
			return false;
				
		if ((num_comps < 1) || (num_comps > cMaxComps))
			return false;
				
		if ((minimum(dst_w, dst_h) < 1) || (maximum(dst_w, dst_h) > BASISU_RESAMPLER_MAX_DIMENSION))
		{
			printf("Image is too large!\n");
			return false;
		}

		if ((src_w == dst_w) && (src_h == dst_h))
		{
			dst = src;
			return true;
		}

		float srgb_to_linear_table[256];
		if (srgb)
		{
			for (int i = 0; i < 256; ++i)
				srgb_to_linear_table[i] = srgb_to_linear((float)i * (1.0f/255.0f));
		}

		const int LINEAR_TO_SRGB_TABLE_SIZE = 8192;
		uint8_t linear_to_srgb_table[LINEAR_TO_SRGB_TABLE_SIZE];

		if (srgb)
		{
			for (int i = 0; i < LINEAR_TO_SRGB_TABLE_SIZE; ++i)
				linear_to_srgb_table[i] = (uint8_t)clamp<int>((int)(255.0f * linear_to_srgb((float)i * (1.0f / (LINEAR_TO_SRGB_TABLE_SIZE - 1))) + .5f), 0, 255);
		}

		std::vector<float> samples[cMaxComps];
		Resampler *resamplers[cMaxComps];
		
		resamplers[0] = new Resampler(src_w, src_h, dst_w, dst_h,
			wrapping ? Resampler::BOUNDARY_WRAP : Resampler::BOUNDARY_CLAMP, 0.0f, 1.0f,
			pFilter, nullptr, nullptr, filter_scale, filter_scale, 0, 0);
		samples[0].resize(src_w);

		for (uint32_t i = 1; i < num_comps; ++i)
		{
			resamplers[i] = new Resampler(src_w, src_h, dst_w, dst_h,
				wrapping ? Resampler::BOUNDARY_WRAP : Resampler::BOUNDARY_CLAMP, 0.0f, 1.0f,
				pFilter, resamplers[0]->get_clist_x(), resamplers[0]->get_clist_y(), filter_scale, filter_scale, 0, 0);
			samples[i].resize(src_w);
		}

		uint32_t dst_y = 0;

		for (uint32_t src_y = 0; src_y < src_h; ++src_y)
		{
			const color_rgba *pSrc = &src(0, src_y);

			// Put source lines into resampler(s)
			for (uint32_t x = 0; x < src_w; ++x)
			{
				for (uint32_t c = 0; c < num_comps; ++c)
				{
					const uint32_t comp_index = first_comp + c;
					const uint32_t v = (*pSrc)[comp_index];

					if (!srgb || (comp_index == 3))
						samples[c][x] = v * (1.0f / 255.0f);
					else
						samples[c][x] = srgb_to_linear_table[v];
				}

				pSrc++;
			}

			for (uint32_t c = 0; c < num_comps; ++c)
			{
				if (!resamplers[c]->put_line(&samples[c][0]))
				{
					for (uint32_t i = 0; i < num_comps; i++)
						delete resamplers[i];
					return false;
				}
			}

			// Now retrieve any output lines
			for (;;)
			{
				uint32_t c;
				for (c = 0; c < num_comps; ++c)
				{
					const uint32_t comp_index = first_comp + c;

					const float *pOutput_samples = resamplers[c]->get_line();
					if (!pOutput_samples)
						break;

					const bool linear_flag = !srgb || (comp_index == 3);
					
					color_rgba *pDst = &dst(0, dst_y);

					for (uint32_t x = 0; x < dst_w; x++)
					{
						// TODO: Add dithering
						if (linear_flag)
						{
							int j = (int)(255.0f * pOutput_samples[x] + .5f);
							(*pDst)[comp_index] = (uint8_t)clamp<int>(j, 0, 255);
						}
						else
						{
							int j = (int)((LINEAR_TO_SRGB_TABLE_SIZE - 1) * pOutput_samples[x] + .5f);
							(*pDst)[comp_index] = linear_to_srgb_table[clamp<int>(j, 0, LINEAR_TO_SRGB_TABLE_SIZE - 1)];
						}

						pDst++;
					}
				}
				if (c < num_comps)
					break;

				++dst_y;
			}
		}

		for (uint32_t i = 0; i < num_comps; ++i)
			delete resamplers[i];

		return true;
	}

	bool image_resample(const imagef& src, imagef& dst, 
		const char* pFilter, float filter_scale,
		bool wrapping,
		uint32_t first_comp, uint32_t num_comps)
	{
		assert((first_comp + num_comps) <= 4);

		const int cMaxComps = 4;

		const uint32_t src_w = src.get_width(), src_h = src.get_height();
		const uint32_t dst_w = dst.get_width(), dst_h = dst.get_height();

		if (maximum(src_w, src_h) > BASISU_RESAMPLER_MAX_DIMENSION)
		{
			printf("Image is too large!\n");
			return false;
		}

		if (!src_w || !src_h || !dst_w || !dst_h)
			return false;

		if ((num_comps < 1) || (num_comps > cMaxComps))
			return false;

		if ((minimum(dst_w, dst_h) < 1) || (maximum(dst_w, dst_h) > BASISU_RESAMPLER_MAX_DIMENSION))
		{
			printf("Image is too large!\n");
			return false;
		}

		if ((src_w == dst_w) && (src_h == dst_h))
		{
			dst = src;
			return true;
		}

		std::vector<float> samples[cMaxComps];
		Resampler* resamplers[cMaxComps];

		resamplers[0] = new Resampler(src_w, src_h, dst_w, dst_h,
			wrapping ? Resampler::BOUNDARY_WRAP : Resampler::BOUNDARY_CLAMP, 1.0f, 0.0f, // no clamping
			pFilter, nullptr, nullptr, filter_scale, filter_scale, 0, 0);
		samples[0].resize(src_w);

		for (uint32_t i = 1; i < num_comps; ++i)
		{
			resamplers[i] = new Resampler(src_w, src_h, dst_w, dst_h,
				wrapping ? Resampler::BOUNDARY_WRAP : Resampler::BOUNDARY_CLAMP, 1.0f, 0.0f, // no clamping
				pFilter, resamplers[0]->get_clist_x(), resamplers[0]->get_clist_y(), filter_scale, filter_scale, 0, 0);
			samples[i].resize(src_w);
		}

		uint32_t dst_y = 0;

		for (uint32_t src_y = 0; src_y < src_h; ++src_y)
		{
			const vec4F* pSrc = &src(0, src_y);

			// Put source lines into resampler(s)
			for (uint32_t x = 0; x < src_w; ++x)
			{
				for (uint32_t c = 0; c < num_comps; ++c)
				{
					const uint32_t comp_index = first_comp + c;
					const float v = (*pSrc)[comp_index];

					samples[c][x] = v;
				}

				pSrc++;
			}

			for (uint32_t c = 0; c < num_comps; ++c)
			{
				if (!resamplers[c]->put_line(&samples[c][0]))
				{
					for (uint32_t i = 0; i < num_comps; i++)
						delete resamplers[i];
					return false;
				}
			}

			// Now retrieve any output lines
			for (;;)
			{
				uint32_t c;
				for (c = 0; c < num_comps; ++c)
				{
					const uint32_t comp_index = first_comp + c;

					const float* pOutput_samples = resamplers[c]->get_line();
					if (!pOutput_samples)
						break;
										
					vec4F* pDst = &dst(0, dst_y);

					for (uint32_t x = 0; x < dst_w; x++)
					{
						(*pDst)[comp_index] = pOutput_samples[x];
						pDst++;
					}
				}
				if (c < num_comps)
					break;

				++dst_y;
			}
		}

		for (uint32_t i = 0; i < num_comps; ++i)
			delete resamplers[i];

		return true;
	}

	void canonical_huffman_calculate_minimum_redundancy(sym_freq *A, int num_syms)
	{
		// See the paper "In-Place Calculation of Minimum Redundancy Codes" by Moffat and Katajainen
		if (!num_syms)
			return;

		if (1 == num_syms)
		{
			A[0].m_key = 1;
			return;
		}
		
		A[0].m_key += A[1].m_key;
		
		int s = 2, r = 0, next;
		for (next = 1; next < (num_syms - 1); ++next)
		{
			if ((s >= num_syms) || (A[r].m_key < A[s].m_key))
			{
				A[next].m_key = A[r].m_key;
				A[r].m_key = next;
				++r;
			}
			else
			{
				A[next].m_key = A[s].m_key;
				++s;
			}

			if ((s >= num_syms) || ((r < next) && A[r].m_key < A[s].m_key))
			{
				A[next].m_key = A[next].m_key + A[r].m_key;
				A[r].m_key = next;
				++r;
			}
			else
			{
				A[next].m_key = A[next].m_key + A[s].m_key;
				++s;
			}
		}
		A[num_syms - 2].m_key = 0;

		for (next = num_syms - 3; next >= 0; --next)
		{
			A[next].m_key = 1 + A[A[next].m_key].m_key;
		}

		int num_avail = 1, num_used = 0, depth = 0;
		r = num_syms - 2;
		next = num_syms - 1;
		while (num_avail > 0)
		{
			for ( ; (r >= 0) && ((int)A[r].m_key == depth); ++num_used, --r )
				;

			for ( ; num_avail > num_used; --next, --num_avail)
				A[next].m_key = depth;

			num_avail = 2 * num_used;
			num_used = 0;
			++depth;
		}
	}

	void canonical_huffman_enforce_max_code_size(int *pNum_codes, int code_list_len, int max_code_size)
	{
		int i;
		uint32_t total = 0;
		if (code_list_len <= 1)
			return;

		for (i = max_code_size + 1; i <= cHuffmanMaxSupportedInternalCodeSize; i++)
			pNum_codes[max_code_size] += pNum_codes[i];

		for (i = max_code_size; i > 0; i--)
			total += (((uint32_t)pNum_codes[i]) << (max_code_size - i));

		while (total != (1UL << max_code_size))
		{
			pNum_codes[max_code_size]--;
			for (i = max_code_size - 1; i > 0; i--)
			{
				if (pNum_codes[i])
				{
					pNum_codes[i]--;
					pNum_codes[i + 1] += 2;
					break;
				}
			}

			total--;
		}
	}

	sym_freq *canonical_huffman_radix_sort_syms(uint32_t num_syms, sym_freq *pSyms0, sym_freq *pSyms1)
	{
		uint32_t total_passes = 2, pass_shift, pass, i, hist[256 * 2];
		sym_freq *pCur_syms = pSyms0, *pNew_syms = pSyms1;

		clear_obj(hist);

		for (i = 0; i < num_syms; i++)
		{
			uint32_t freq = pSyms0[i].m_key;
			
			// We scale all input frequencies to 16-bits.
			assert(freq <= UINT16_MAX);

			hist[freq & 0xFF]++;
			hist[256 + ((freq >> 8) & 0xFF)]++;
		}

		while ((total_passes > 1) && (num_syms == hist[(total_passes - 1) * 256]))
			total_passes--;

		for (pass_shift = 0, pass = 0; pass < total_passes; pass++, pass_shift += 8)
		{
			const uint32_t *pHist = &hist[pass << 8];
			uint32_t offsets[256], cur_ofs = 0;
			for (i = 0; i < 256; i++)
			{
				offsets[i] = cur_ofs;
				cur_ofs += pHist[i];
			}

			for (i = 0; i < num_syms; i++)
				pNew_syms[offsets[(pCur_syms[i].m_key >> pass_shift) & 0xFF]++] = pCur_syms[i];

			sym_freq *t = pCur_syms;
			pCur_syms = pNew_syms;
			pNew_syms = t;
		}

		return pCur_syms;
	}

	bool huffman_encoding_table::init(uint32_t num_syms, const uint16_t *pFreq, uint32_t max_code_size)
	{
		if (max_code_size > cHuffmanMaxSupportedCodeSize)
			return false;
		if ((!num_syms) || (num_syms > cHuffmanMaxSyms))
			return false;

		uint32_t total_used_syms = 0;
		for (uint32_t i = 0; i < num_syms; i++)
			if (pFreq[i])
				total_used_syms++;

		if (!total_used_syms)
			return false;

		std::vector<sym_freq> sym_freq0(total_used_syms), sym_freq1(total_used_syms);
		for (uint32_t i = 0, j = 0; i < num_syms; i++)
		{
			if (pFreq[i])
			{
				sym_freq0[j].m_key = pFreq[i];
				sym_freq0[j++].m_sym_index = static_cast<uint16_t>(i);
			}
		}

		sym_freq *pSym_freq = canonical_huffman_radix_sort_syms(total_used_syms, &sym_freq0[0], &sym_freq1[0]);

		canonical_huffman_calculate_minimum_redundancy(pSym_freq, total_used_syms);

		int num_codes[cHuffmanMaxSupportedInternalCodeSize + 1];
		clear_obj(num_codes);

		for (uint32_t i = 0; i < total_used_syms; i++)
		{
			if (pSym_freq[i].m_key > cHuffmanMaxSupportedInternalCodeSize)
				return false;

			num_codes[pSym_freq[i].m_key]++;
		}

		canonical_huffman_enforce_max_code_size(num_codes, total_used_syms, max_code_size);

		m_code_sizes.resize(0);
		m_code_sizes.resize(num_syms);

		m_codes.resize(0);
		m_codes.resize(num_syms);

		for (uint32_t i = 1, j = total_used_syms; i <= max_code_size; i++)
			for (uint32_t l = num_codes[i]; l > 0; l--)
				m_code_sizes[pSym_freq[--j].m_sym_index] = static_cast<uint8_t>(i);

		uint32_t next_code[cHuffmanMaxSupportedInternalCodeSize + 1];

		next_code[1] = 0;
		for (uint32_t j = 0, i = 2; i <= max_code_size; i++)
			next_code[i] = j = ((j + num_codes[i - 1]) << 1);

		for (uint32_t i = 0; i < num_syms; i++)
		{
			uint32_t rev_code = 0, code, code_size;
			if ((code_size = m_code_sizes[i]) == 0)
				continue;
			if (code_size > cHuffmanMaxSupportedInternalCodeSize)
				return false;
			code = next_code[code_size]++;
			for (uint32_t l = code_size; l > 0; l--, code >>= 1)
				rev_code = (rev_code << 1) | (code & 1);
			m_codes[i] = static_cast<uint16_t>(rev_code);
		}

		return true;
	}

	bool huffman_encoding_table::init(uint32_t num_syms, const uint32_t *pSym_freq, uint32_t max_code_size)
	{
		if ((!num_syms) || (num_syms > cHuffmanMaxSyms))
			return false;

		uint16_vec sym_freq(num_syms);

		uint32_t max_freq = 0;
		for (uint32_t i = 0; i < num_syms; i++)
			max_freq = maximum(max_freq, pSym_freq[i]);

		if (max_freq < UINT16_MAX)
		{
			for (uint32_t i = 0; i < num_syms; i++)
				sym_freq[i] = static_cast<uint16_t>(pSym_freq[i]);
		}
		else
		{
			for (uint32_t i = 0; i < num_syms; i++)
			{
				if (pSym_freq[i])
				{
					uint32_t f = static_cast<uint32_t>((static_cast<uint64_t>(pSym_freq[i]) * 65534U + (max_freq >> 1)) / max_freq);
					sym_freq[i] = static_cast<uint16_t>(clamp<uint32_t>(f, 1, 65534));
				}
			}
		}

		return init(num_syms, &sym_freq[0], max_code_size);
	}

	void bitwise_coder::end_nonzero_run(uint16_vec &syms, uint32_t &run_size, uint32_t len)
	{
		if (run_size)
		{
			if (run_size < cHuffmanSmallRepeatSizeMin)
			{
				while (run_size--)
					syms.push_back(static_cast<uint16_t>(len));
			}
			else if (run_size <= cHuffmanSmallRepeatSizeMax)
			{
				syms.push_back(static_cast<uint16_t>(cHuffmanSmallRepeatCode | ((run_size - cHuffmanSmallRepeatSizeMin) << 6)));
			}
			else
			{
				assert((run_size >= cHuffmanBigRepeatSizeMin) && (run_size <= cHuffmanBigRepeatSizeMax));
				syms.push_back(static_cast<uint16_t>(cHuffmanBigRepeatCode | ((run_size - cHuffmanBigRepeatSizeMin) << 6)));
			}
		}

		run_size = 0;
	}

	void bitwise_coder::end_zero_run(uint16_vec &syms, uint32_t &run_size)
	{
		if (run_size)
		{
			if (run_size < cHuffmanSmallZeroRunSizeMin)
			{
				while (run_size--)
					syms.push_back(0);
			}
			else if (run_size <= cHuffmanSmallZeroRunSizeMax)
			{
				syms.push_back(static_cast<uint16_t>(cHuffmanSmallZeroRunCode | ((run_size - cHuffmanSmallZeroRunSizeMin) << 6)));
			}
			else
			{
				assert((run_size >= cHuffmanBigZeroRunSizeMin) && (run_size <= cHuffmanBigZeroRunSizeMax));
				syms.push_back(static_cast<uint16_t>(cHuffmanBigZeroRunCode | ((run_size - cHuffmanBigZeroRunSizeMin) << 6)));
			}
		}

		run_size = 0;
	}

	uint32_t bitwise_coder::emit_huffman_table(const huffman_encoding_table &tab)
	{
		const uint64_t start_bits = m_total_bits;

		const uint8_vec &code_sizes = tab.get_code_sizes();

		uint32_t total_used = tab.get_total_used_codes();
		put_bits(total_used, cHuffmanMaxSymsLog2);
			
		if (!total_used)
			return 0;

		uint16_vec syms;
		syms.reserve(total_used + 16);

		uint32_t prev_code_len = UINT_MAX, zero_run_size = 0, nonzero_run_size = 0;

		for (uint32_t i = 0; i <= total_used; ++i)
		{
			const uint32_t code_len = (i == total_used) ? 0xFF : code_sizes[i];
			assert((code_len == 0xFF) || (code_len <= 16));

			if (code_len)
			{
				end_zero_run(syms, zero_run_size);

				if (code_len != prev_code_len)
				{
					end_nonzero_run(syms, nonzero_run_size, prev_code_len);
					if (code_len != 0xFF)
						syms.push_back(static_cast<uint16_t>(code_len));
				}
				else if (++nonzero_run_size == cHuffmanBigRepeatSizeMax)
					end_nonzero_run(syms, nonzero_run_size, prev_code_len);
			}
			else
			{
				end_nonzero_run(syms, nonzero_run_size, prev_code_len);

				if (++zero_run_size == cHuffmanBigZeroRunSizeMax)
					end_zero_run(syms, zero_run_size);
			}

			prev_code_len = code_len;
		}

		histogram h(cHuffmanTotalCodelengthCodes);
		for (uint32_t i = 0; i < syms.size(); i++)
			h.inc(syms[i] & 63);

		huffman_encoding_table ct;
		if (!ct.init(h, 7))
			return 0;

		assert(cHuffmanTotalSortedCodelengthCodes == cHuffmanTotalCodelengthCodes);

		uint32_t total_codelength_codes;
		for (total_codelength_codes = cHuffmanTotalSortedCodelengthCodes; total_codelength_codes > 0; total_codelength_codes--)
			if (ct.get_code_sizes()[g_huffman_sorted_codelength_codes[total_codelength_codes - 1]])
				break;

		assert(total_codelength_codes);

		put_bits(total_codelength_codes, 5);
		for (uint32_t i = 0; i < total_codelength_codes; i++)
			put_bits(ct.get_code_sizes()[g_huffman_sorted_codelength_codes[i]], 3);

		for (uint32_t i = 0; i < syms.size(); ++i)
		{
			const uint32_t l = syms[i] & 63, e = syms[i] >> 6;

			put_code(l, ct);
				
			if (l == cHuffmanSmallZeroRunCode)
				put_bits(e, cHuffmanSmallZeroRunExtraBits);
			else if (l == cHuffmanBigZeroRunCode)
				put_bits(e, cHuffmanBigZeroRunExtraBits);
			else if (l == cHuffmanSmallRepeatCode)
				put_bits(e, cHuffmanSmallRepeatExtraBits);
			else if (l == cHuffmanBigRepeatCode)
				put_bits(e, cHuffmanBigRepeatExtraBits);
		}

		return (uint32_t)(m_total_bits - start_bits);
	}

	bool huffman_test(int rand_seed)
	{
		histogram h(19);

		// Feed in a fibonacci sequence to force large codesizes
		h[0] += 1; h[1] += 1; h[2] += 2; h[3] += 3;
		h[4] += 5; h[5] += 8; h[6] += 13; h[7] += 21;
		h[8] += 34; h[9] += 55; h[10] += 89; h[11] += 144;
		h[12] += 233; h[13] += 377; h[14] += 610; h[15] += 987;
		h[16] += 1597; h[17] += 2584; h[18] += 4181;

		huffman_encoding_table etab;
		etab.init(h, 16);
		
		{
			bitwise_coder c;
			c.init(1024);

			c.emit_huffman_table(etab);
			for (int i = 0; i < 19; i++)
				c.put_code(i, etab);

			c.flush();

			basist::bitwise_decoder d;
			d.init(&c.get_bytes()[0], static_cast<uint32_t>(c.get_bytes().size()));

			basist::huffman_decoding_table dtab;
			bool success = d.read_huffman_table(dtab);
			if (!success)
			{
				assert(0);
				printf("Failure 5\n");
				return false;
			}

			for (uint32_t i = 0; i < 19; i++)
			{
				uint32_t s = d.decode_huffman(dtab);
				if (s != i)
				{
					assert(0);
					printf("Failure 5\n");
					return false;
				}
			}
		}

		basisu::rand r;
		r.seed(rand_seed);

		for (int iter = 0; iter < 500000; iter++)
		{
			printf("%u\n", iter);

			uint32_t max_sym = r.irand(0, 8193);
			uint32_t num_codes = r.irand(1, 10000);
			uint_vec syms(num_codes);

			for (uint32_t i = 0; i < num_codes; i++)
			{
				if (r.bit())
					syms[i] = r.irand(0, max_sym);
				else
				{
					int s = (int)(r.gaussian((float)max_sym / 2, (float)maximum<int>(1, max_sym / 2)) + .5f);
					s = basisu::clamp<int>(s, 0, max_sym);

					syms[i] = s;
				}

			}

			histogram h1(max_sym + 1);
			for (uint32_t i = 0; i < num_codes; i++)
				h1[syms[i]]++;

			huffman_encoding_table etab2;
			if (!etab2.init(h1, 16))
			{
				assert(0);
				printf("Failed 0\n");
				return false;
			}

			bitwise_coder c;
			c.init(1024);

			c.emit_huffman_table(etab2);

			for (uint32_t i = 0; i < num_codes; i++)
				c.put_code(syms[i], etab2);

			c.flush();

			basist::bitwise_decoder d;
			d.init(&c.get_bytes()[0], (uint32_t)c.get_bytes().size());

			basist::huffman_decoding_table dtab;
			bool success = d.read_huffman_table(dtab);
			if (!success)
			{
				assert(0);
				printf("Failed 2\n");
				return false;
			}

			for (uint32_t i = 0; i < num_codes; i++)
			{
				uint32_t s = d.decode_huffman(dtab);
				if (s != syms[i])
				{
					assert(0);
					printf("Failed 4\n");
					return false;
				}
			}

		}
		return true;
	}

	void palette_index_reorderer::init(uint32_t num_indices, const uint32_t *pIndices, uint32_t num_syms, pEntry_dist_func pDist_func, void *pCtx, float dist_func_weight)
	{
		assert((num_syms > 0) && (num_indices > 0));
		assert((dist_func_weight >= 0.0f) && (dist_func_weight <= 1.0f));

		clear();

		m_remap_table.resize(num_syms);
		m_entries_picked.reserve(num_syms);
		m_total_count_to_picked.resize(num_syms);

		if (num_indices <= 1)
			return;

		prepare_hist(num_syms, num_indices, pIndices);
		find_initial(num_syms);

		while (m_entries_to_do.size())
		{
			// Find the best entry to move into the picked list.
			uint32_t best_entry;
			double best_count;
			find_next_entry(best_entry, best_count, pDist_func, pCtx, dist_func_weight);

			// We now have chosen an entry to place in the picked list, now determine which side it goes on.
			const uint32_t entry_to_move = m_entries_to_do[best_entry];
								
			float side = pick_side(num_syms, entry_to_move, pDist_func, pCtx, dist_func_weight);
								
			// Put entry_to_move either on the "left" or "right" side of the picked entries
			if (side <= 0)
				m_entries_picked.push_back(entry_to_move);
			else
				m_entries_picked.insert(m_entries_picked.begin(), entry_to_move);

			// Erase best_entry from the todo list
			m_entries_to_do.erase(m_entries_to_do.begin() + best_entry);

			// We've just moved best_entry to the picked list, so now we need to update m_total_count_to_picked[] to factor the additional count to best_entry
			for (uint32_t i = 0; i < m_entries_to_do.size(); i++)
				m_total_count_to_picked[m_entries_to_do[i]] += get_hist(m_entries_to_do[i], entry_to_move, num_syms);
		}

		for (uint32_t i = 0; i < num_syms; i++)
			m_remap_table[m_entries_picked[i]] = i;
	}

	void palette_index_reorderer::prepare_hist(uint32_t num_syms, uint32_t num_indices, const uint32_t *pIndices)
	{
		m_hist.resize(0);
		m_hist.resize(num_syms * num_syms);

		for (uint32_t i = 0; i < num_indices; i++)
		{
			const uint32_t idx = pIndices[i];
			inc_hist(idx, (i < (num_indices - 1)) ? pIndices[i + 1] : -1, num_syms);
			inc_hist(idx, (i > 0) ? pIndices[i - 1] : -1, num_syms);
		}
	}

	void palette_index_reorderer::find_initial(uint32_t num_syms)
	{
		uint32_t max_count = 0, max_index = 0;
		for (uint32_t i = 0; i < num_syms * num_syms; i++)
			if (m_hist[i] > max_count)
				max_count = m_hist[i], max_index = i;

		uint32_t a = max_index / num_syms, b = max_index % num_syms;

		const uint32_t ofs = m_entries_picked.size();

		m_entries_picked.push_back(a);
		m_entries_picked.push_back(b);

		for (uint32_t i = 0; i < num_syms; i++)
			if ((i != m_entries_picked[ofs + 1]) && (i != m_entries_picked[ofs]))
				m_entries_to_do.push_back(i);

		for (uint32_t i = 0; i < m_entries_to_do.size(); i++)
			for (uint32_t j = 0; j < m_entries_picked.size(); j++)
				m_total_count_to_picked[m_entries_to_do[i]] += get_hist(m_entries_to_do[i], m_entries_picked[j], num_syms);
	}

	void palette_index_reorderer::find_next_entry(uint32_t &best_entry, double &best_count, pEntry_dist_func pDist_func, void *pCtx, float dist_func_weight)
	{
		best_entry = 0;
		best_count = 0;

		for (uint32_t i = 0; i < m_entries_to_do.size(); i++)
		{
			const uint32_t u = m_entries_to_do[i];
			double total_count = m_total_count_to_picked[u];

			if (pDist_func)
			{
				float w = maximum<float>((*pDist_func)(u, m_entries_picked.front(), pCtx), (*pDist_func)(u, m_entries_picked.back(), pCtx));
				assert((w >= 0.0f) && (w <= 1.0f));
				total_count = (total_count + 1.0f) * lerp(1.0f - dist_func_weight, 1.0f + dist_func_weight, w);
			}

			if (total_count <= best_count)
				continue;

			best_entry = i;
			best_count = total_count;
		}
	}

	float palette_index_reorderer::pick_side(uint32_t num_syms, uint32_t entry_to_move, pEntry_dist_func pDist_func, void *pCtx, float dist_func_weight)
	{
		float which_side = 0;

		int l_count = 0, r_count = 0;
		for (uint32_t j = 0; j < m_entries_picked.size(); j++)
		{
			const int count = get_hist(entry_to_move, m_entries_picked[j], num_syms), r = ((int)m_entries_picked.size() + 1 - 2 * (j + 1));
			which_side += static_cast<float>(r * count);
			if (r >= 0)
				l_count += r * count;
			else
				r_count += -r * count;
		}

		if (pDist_func)
		{
			float w_left = lerp(1.0f - dist_func_weight, 1.0f + dist_func_weight, (*pDist_func)(entry_to_move, m_entries_picked.front(), pCtx));
			float w_right = lerp(1.0f - dist_func_weight, 1.0f + dist_func_weight, (*pDist_func)(entry_to_move, m_entries_picked.back(), pCtx));
			which_side = w_left * l_count - w_right * r_count;
		}
		return which_side;
	}
	
	void image_metrics::calc(const imagef& a, const imagef& b, uint32_t first_chan, uint32_t total_chans, bool avg_comp_error, bool log)
	{
		assert((first_chan < 4U) && (first_chan + total_chans <= 4U));

		const uint32_t width = basisu::minimum(a.get_width(), b.get_width());
		const uint32_t height = basisu::minimum(a.get_height(), b.get_height());

		double max_e = -1e+30f;
		double sum = 0.0f, sum_sqr = 0.0f;

		m_has_neg = false;
		m_any_abnormal = false;
		m_hf_mag_overflow = false;
				
		for (uint32_t y = 0; y < height; y++)
		{
			for (uint32_t x = 0; x < width; x++)
			{
				const vec4F& ca = a(x, y), &cb = b(x, y);
								
				if (total_chans)
				{
					for (uint32_t c = 0; c < total_chans; c++)
					{
						float fa = ca[first_chan + c], fb = cb[first_chan + c];

						if ((fabs(fa) > basist::MAX_HALF_FLOAT) || (fabs(fb) > basist::MAX_HALF_FLOAT))
							m_hf_mag_overflow = true;

						if ((fa < 0.0f) || (fb < 0.0f))
							m_has_neg = true;

						if (std::isinf(fa) || std::isinf(fb) || std::isnan(fa) || std::isnan(fb))
							m_any_abnormal = true;
												
						const double delta = fabs(fa - fb);
						max_e = basisu::maximum<double>(max_e, delta);

						if (log)
						{
							double log2_delta = log2f(basisu::maximum(0.0f, fa) + 1.0f) - log2f(basisu::maximum(0.0f, fb) + 1.0f);

							sum += fabs(log2_delta);
							sum_sqr += log2_delta * log2_delta;
						}
						else
						{
							sum += fabs(delta);
							sum_sqr += delta * delta;
						}
					}
				}
				else
				{
					for (uint32_t c = 0; c < 3; c++)
					{
						float fa = ca[c], fb = cb[c];

						if ((fabs(fa) > basist::MAX_HALF_FLOAT) || (fabs(fb) > basist::MAX_HALF_FLOAT))
							m_hf_mag_overflow = true;

						if ((fa < 0.0f) || (fb < 0.0f))
							m_has_neg = true;

						if (std::isinf(fa) || std::isinf(fb) || std::isnan(fa) || std::isnan(fb))
							m_any_abnormal = true;
					}

					double ca_l = get_luminance(ca), cb_l = get_luminance(cb);
					
					double delta = fabs(ca_l - cb_l);
					max_e = basisu::maximum(max_e, delta);
					
					if (log)
					{
						double log2_delta = log2(basisu::maximum<double>(0.0f, ca_l) + 1.0f) - log2(basisu::maximum<double>(0.0f, cb_l) + 1.0f);

						sum += fabs(log2_delta);
						sum_sqr += log2_delta * log2_delta;
					}
					else
					{
						sum += delta;
						sum_sqr += delta * delta;
					}
				}
			}
		}

		m_max = (double)(max_e);

		double total_values = (double)width * (double)height;
		if (avg_comp_error)
			total_values *= (double)clamp<uint32_t>(total_chans, 1, 4);

		m_mean = (float)(sum / total_values);
		m_mean_squared = (float)(sum_sqr / total_values);
		m_rms = (float)sqrt(sum_sqr / total_values);
		
		const double max_val = 1.0f;
		m_psnr = m_rms ? (float)clamp<double>(log10(max_val / m_rms) * 20.0f, 0.0f, 1000.0f) : 1000.0f;
	}

	void image_metrics::calc_half(const imagef& a, const imagef& b, uint32_t first_chan, uint32_t total_chans, bool avg_comp_error)
	{
		assert(total_chans);
		assert((first_chan < 4U) && (first_chan + total_chans <= 4U));

		const uint32_t width = basisu::minimum(a.get_width(), b.get_width());
		const uint32_t height = basisu::minimum(a.get_height(), b.get_height());

		m_has_neg = false;
		m_hf_mag_overflow = false;
		m_any_abnormal = false;

		uint_vec hist(65536);
		
		for (uint32_t y = 0; y < height; y++)
		{
			for (uint32_t x = 0; x < width; x++)
			{
				const vec4F& ca = a(x, y), &cb = b(x, y);

				for (uint32_t i = 0; i < 4; i++)
				{
					if ((ca[i] < 0.0f) || (cb[i] < 0.0f))
						m_has_neg = true;
					
					if ((fabs(ca[i]) > basist::MAX_HALF_FLOAT) || (fabs(cb[i]) > basist::MAX_HALF_FLOAT))
						m_hf_mag_overflow = true;

					if (std::isnan(ca[i]) || std::isnan(cb[i]) || std::isinf(ca[i]) || std::isinf(cb[i]))
						m_any_abnormal = true;
				}

				int cah[4] = { basist::float_to_half(ca[0]), basist::float_to_half(ca[1]), basist::float_to_half(ca[2]), basist::float_to_half(ca[3]) };
				int cbh[4] = { basist::float_to_half(cb[0]), basist::float_to_half(cb[1]), basist::float_to_half(cb[2]), basist::float_to_half(cb[3]) };

				for (uint32_t c = 0; c < total_chans; c++)
					hist[iabs(cah[first_chan + c] - cbh[first_chan + c]) & 65535]++;

			} // x
		} // y

		m_max = 0;
		double sum = 0.0f, sum2 = 0.0f;
		for (uint32_t i = 0; i < 65536; i++)
		{
			if (hist[i])
			{
				m_max = basisu::maximum<double>(m_max, (double)i);
				double v = (double)i * (double)hist[i];
				sum += v;
				sum2 += (double)i * v;
			}
		}

		double total_values = (double)width * (double)height;
		if (avg_comp_error)
			total_values *= (double)clamp<uint32_t>(total_chans, 1, 4);

		const float max_val = 65535.0f;
		m_mean = (float)clamp<double>(sum / total_values, 0.0f, max_val);
		m_mean_squared = (float)clamp<double>(sum2 / total_values, 0.0f, max_val * max_val);
		m_rms = (float)sqrt(m_mean_squared);
		m_psnr = m_rms ? (float)clamp<double>(log10(max_val / m_rms) * 20.0f, 0.0f, 1000.0f) : 1000.0f;
	}

	// Alt. variant, same as calc_half(), for validation.
	void image_metrics::calc_half2(const imagef& a, const imagef& b, uint32_t first_chan, uint32_t total_chans, bool avg_comp_error)
	{
		assert(total_chans);
		assert((first_chan < 4U) && (first_chan + total_chans <= 4U));

		const uint32_t width = basisu::minimum(a.get_width(), b.get_width());
		const uint32_t height = basisu::minimum(a.get_height(), b.get_height());

		m_has_neg = false;
		m_hf_mag_overflow = false;
		m_any_abnormal = false;
				
		double sum = 0.0f, sum2 = 0.0f;
		m_max = 0;

		for (uint32_t y = 0; y < height; y++)
		{
			for (uint32_t x = 0; x < width; x++)
			{
				const vec4F& ca = a(x, y), & cb = b(x, y);

				for (uint32_t i = 0; i < 4; i++)
				{
					if ((ca[i] < 0.0f) || (cb[i] < 0.0f))
						m_has_neg = true;

					if ((fabs(ca[i]) > basist::MAX_HALF_FLOAT) || (fabs(cb[i]) > basist::MAX_HALF_FLOAT))
						m_hf_mag_overflow = true;

					if (std::isnan(ca[i]) || std::isnan(cb[i]) || std::isinf(ca[i]) || std::isinf(cb[i]))
						m_any_abnormal = true;
				}

				int cah[4] = { basist::float_to_half(ca[0]), basist::float_to_half(ca[1]), basist::float_to_half(ca[2]), basist::float_to_half(ca[3]) };
				int cbh[4] = { basist::float_to_half(cb[0]), basist::float_to_half(cb[1]), basist::float_to_half(cb[2]), basist::float_to_half(cb[3]) };

				for (uint32_t c = 0; c < total_chans; c++)
				{
					int diff = iabs(cah[first_chan + c] - cbh[first_chan + c]);
					if (diff)
						m_max = std::max<double>(m_max, (double)diff);

					sum += diff;
					sum2 += squarei(cah[first_chan + c] - cbh[first_chan + c]);
				}

			} // x
		} // y
						
		double total_values = (double)width * (double)height;
		if (avg_comp_error)
			total_values *= (double)clamp<uint32_t>(total_chans, 1, 4);

		const float max_val = 65535.0f;
		m_mean = (float)clamp<double>(sum / total_values, 0.0f, max_val);
		m_mean_squared = (float)clamp<double>(sum2 / total_values, 0.0f, max_val * max_val);
		m_rms = (float)sqrt(m_mean_squared);
		m_psnr = m_rms ? (float)clamp<double>(log10(max_val / m_rms) * 20.0f, 0.0f, 1000.0f) : 1000.0f;
	}

	void image_metrics::calc(const image &a, const image &b, uint32_t first_chan, uint32_t total_chans, bool avg_comp_error, bool use_601_luma)
	{
		assert((first_chan < 4U) && (first_chan + total_chans <= 4U));

		const uint32_t width = basisu::minimum(a.get_width(), b.get_width());
		const uint32_t height = basisu::minimum(a.get_height(), b.get_height());

		double hist[256];
		clear_obj(hist);

		m_has_neg = false;
		m_any_abnormal = false;
		m_hf_mag_overflow = false;

		for (uint32_t y = 0; y < height; y++)
		{
			for (uint32_t x = 0; x < width; x++)
			{
				const color_rgba &ca = a(x, y), &cb = b(x, y);

				if (total_chans)
				{
					for (uint32_t c = 0; c < total_chans; c++)
						hist[iabs(ca[first_chan + c] - cb[first_chan + c])]++;
				}
				else
				{
					if (use_601_luma)
						hist[iabs(ca.get_601_luma() - cb.get_601_luma())]++;
					else
						hist[iabs(ca.get_709_luma() - cb.get_709_luma())]++;
				}
			}
		}

		m_max = 0;
		double sum = 0.0f, sum2 = 0.0f;
		for (uint32_t i = 0; i < 256; i++)
		{
			if (hist[i])
			{
				m_max = basisu::maximum<double>(m_max, (double)i);
				double v = i * hist[i];
				sum += v;
				sum2 += i * v;
			}
		}

		double total_values = (double)width * (double)height;
		if (avg_comp_error)
			total_values *= (double)clamp<uint32_t>(total_chans, 1, 4);

		m_mean = (float)clamp<double>(sum / total_values, 0.0f, 255.0);
		m_mean_squared = (float)clamp<double>(sum2 / total_values, 0.0f, 255.0f * 255.0f);
		m_rms = (float)sqrt(m_mean_squared);
		m_psnr = m_rms ? (float)clamp<double>(log10(255.0 / m_rms) * 20.0f, 0.0f, 100.0f) : 100.0f;
	}

	void fill_buffer_with_random_bytes(void *pBuf, size_t size, uint32_t seed)
	{
		rand r(seed);

		uint8_t *pDst = static_cast<uint8_t *>(pBuf);

		while (size >= sizeof(uint32_t))
		{
			*(uint32_t *)pDst = r.urand32();
			pDst += sizeof(uint32_t);
			size -= sizeof(uint32_t);
		}

		while (size)
		{
			*pDst++ = r.byte();
			size--;
		}
	}

	uint32_t hash_hsieh(const uint8_t *pBuf, size_t len)
	{
		if (!pBuf || !len) 
			return 0;

		uint32_t h = static_cast<uint32_t>(len);

		const uint32_t bytes_left = len & 3;
		len >>= 2;

		while (len--)
		{
			const uint16_t *pWords = reinterpret_cast<const uint16_t *>(pBuf);

			h += pWords[0];
			
			const uint32_t t = (pWords[1] << 11) ^ h;
			h = (h << 16) ^ t;
			
			pBuf += sizeof(uint32_t);
			
			h += h >> 11;
		}

		switch (bytes_left)
		{
		case 1: 
			h += *reinterpret_cast<const signed char*>(pBuf);
			h ^= h << 10;
			h += h >> 1;
			break;
		case 2: 
			h += *reinterpret_cast<const uint16_t *>(pBuf);
			h ^= h << 11;
			h += h >> 17;
			break;
		case 3:
			h += *reinterpret_cast<const uint16_t *>(pBuf);
			h ^= h << 16;
			h ^= (static_cast<signed char>(pBuf[sizeof(uint16_t)])) << 18;
			h += h >> 11;
			break;
		default:
			break;
		}
		
		h ^= h << 3;
		h += h >> 5;
		h ^= h << 4;
		h += h >> 17;
		h ^= h << 25;
		h += h >> 6;

		return h;
	}

	job_pool::job_pool(uint32_t num_threads) : 
		m_num_active_jobs(0),
		m_kill_flag(false)
	{
		assert(num_threads >= 1U);

		debug_printf("job_pool::job_pool: %u total threads\n", num_threads);

		if (num_threads > 1)
		{
			m_threads.resize(num_threads - 1);

			for (int i = 0; i < ((int)num_threads - 1); i++)
			   m_threads[i] = std::thread([this, i] { job_thread(i); });
		}
	}

	job_pool::~job_pool()
	{
		debug_printf("job_pool::~job_pool\n");
		
		// Notify all workers that they need to die right now.
		m_kill_flag = true;
		
		m_has_work.notify_all();

		// Wait for all workers to die.
		for (uint32_t i = 0; i < m_threads.size(); i++)
			m_threads[i].join();
	}
				
	void job_pool::add_job(const std::function<void()>& job)
	{
		std::unique_lock<std::mutex> lock(m_mutex);

		m_queue.emplace_back(job);

		const size_t queue_size = m_queue.size();

		lock.unlock();

		if (queue_size > 1)
			m_has_work.notify_one();
	}

	void job_pool::add_job(std::function<void()>&& job)
	{
		std::unique_lock<std::mutex> lock(m_mutex);

		m_queue.emplace_back(std::move(job));
						
		const size_t queue_size = m_queue.size();

		lock.unlock();

		if (queue_size > 1)
		{
			m_has_work.notify_one();
		}
	}

	void job_pool::wait_for_all()
	{
		std::unique_lock<std::mutex> lock(m_mutex);

		// Drain the job queue on the calling thread.
		while (!m_queue.empty())
		{
			std::function<void()> job(m_queue.back());
			m_queue.pop_back();

			lock.unlock();

			job();

			lock.lock();
		}

		// The queue is empty, now wait for all active jobs to finish up.
		m_no_more_jobs.wait(lock, [this]{ return !m_num_active_jobs; } );
	}

	void job_pool::job_thread(uint32_t index)
	{
		BASISU_NOTE_UNUSED(index);
		//debug_printf("job_pool::job_thread: starting %u\n", index);
		
		while (true)
		{
			std::unique_lock<std::mutex> lock(m_mutex);

			// Wait for any jobs to be issued.
			m_has_work.wait(lock, [this] { return m_kill_flag || m_queue.size(); } );

			// Check to see if we're supposed to exit.
			if (m_kill_flag)
				break;

			// Get the job and execute it.
			std::function<void()> job(m_queue.back());
			m_queue.pop_back();

			++m_num_active_jobs;

			lock.unlock();

			job();

			lock.lock();

			--m_num_active_jobs;

			// Now check if there are no more jobs remaining. 
			const bool all_done = m_queue.empty() && !m_num_active_jobs;
			
			lock.unlock();

			if (all_done)
				m_no_more_jobs.notify_all();
		}

		//debug_printf("job_pool::job_thread: exiting\n");
	}

	// .TGA image loading
	#pragma pack(push)
	#pragma pack(1)
	struct tga_header
	{
		uint8_t			m_id_len;
		uint8_t			m_cmap;
		uint8_t			m_type;
		packed_uint<2>	m_cmap_first;
		packed_uint<2> m_cmap_len;
		uint8_t			m_cmap_bpp;
		packed_uint<2> m_x_org;
		packed_uint<2> m_y_org;
		packed_uint<2> m_width;
		packed_uint<2> m_height;
		uint8_t			m_depth;
		uint8_t			m_desc;
	};
	#pragma pack(pop)

	const uint32_t MAX_TGA_IMAGE_SIZE = 16384;

	enum tga_image_type
	{
		cITPalettized = 1,
		cITRGB = 2,
		cITGrayscale = 3
	};

	uint8_t *read_tga(const uint8_t *pBuf, uint32_t buf_size, int &width, int &height, int &n_chans)
	{
		width = 0;
		height = 0;
		n_chans = 0;

		if (buf_size <= sizeof(tga_header))
			return nullptr;

		const tga_header &hdr = *reinterpret_cast<const tga_header *>(pBuf);

		if ((!hdr.m_width) || (!hdr.m_height) || (hdr.m_width > MAX_TGA_IMAGE_SIZE) || (hdr.m_height > MAX_TGA_IMAGE_SIZE))
			return nullptr;

		if (hdr.m_desc >> 6)
			return nullptr;

		// Simple validation
		if ((hdr.m_cmap != 0) && (hdr.m_cmap != 1))
			return nullptr;
		
		if (hdr.m_cmap)
		{
			if ((hdr.m_cmap_bpp == 0) || (hdr.m_cmap_bpp > 32))
				return nullptr;

			// Nobody implements CMapFirst correctly, so we're not supporting it. Never seen it used, either.
			if (hdr.m_cmap_first != 0)
				return nullptr;
		}

		const bool x_flipped = (hdr.m_desc & 0x10) != 0;
		const bool y_flipped = (hdr.m_desc & 0x20) == 0;

		bool rle_flag = false;
		int file_image_type = hdr.m_type;
		if (file_image_type > 8)
		{
			file_image_type -= 8;
			rle_flag = true;
		}

		const tga_image_type image_type = static_cast<tga_image_type>(file_image_type);

		switch (file_image_type)
		{
		case cITRGB:
			if (hdr.m_depth == 8)
				return nullptr;
			break;
		case cITPalettized:
			if ((hdr.m_depth != 8) || (hdr.m_cmap != 1) || (hdr.m_cmap_len == 0))
				return nullptr;
			break;
		case cITGrayscale:
			if ((hdr.m_cmap != 0) || (hdr.m_cmap_len != 0))
				return nullptr;
			if ((hdr.m_depth != 8) && (hdr.m_depth != 16))
				return nullptr;
			break;
		default:
			return nullptr;
		}

		uint32_t tga_bytes_per_pixel = 0;

		switch (hdr.m_depth)
		{
		case 32:
			tga_bytes_per_pixel = 4;
			n_chans = 4;
			break;
		case 24:
			tga_bytes_per_pixel = 3;
			n_chans = 3;
			break;
		case 16:
		case 15:
			tga_bytes_per_pixel = 2;
			// For compatibility with stb_image_write.h
			n_chans = ((file_image_type == cITGrayscale) && (hdr.m_depth == 16)) ? 4 : 3;
			break;
		case 8:
			tga_bytes_per_pixel = 1;
			// For palettized RGBA support, which both FreeImage and stb_image support.
			n_chans = ((file_image_type == cITPalettized) && (hdr.m_cmap_bpp == 32)) ? 4 : 3;
			break;
		default:
			return nullptr;
		}

		//const uint32_t bytes_per_line = hdr.m_width * tga_bytes_per_pixel;

		const uint8_t *pSrc = pBuf + sizeof(tga_header);
		uint32_t bytes_remaining = buf_size - sizeof(tga_header);

		if (hdr.m_id_len)
		{
			if (bytes_remaining < hdr.m_id_len)
				return nullptr;
			pSrc += hdr.m_id_len;
			bytes_remaining += hdr.m_id_len;
		}

		color_rgba pal[256];
		for (uint32_t i = 0; i < 256; i++)
			pal[i].set(0, 0, 0, 255);

		if ((hdr.m_cmap) && (hdr.m_cmap_len))
		{
			if (image_type == cITPalettized)
			{
				// Note I cannot find any files using 32bpp palettes in the wild (never seen any in ~30 years).
				if ( ((hdr.m_cmap_bpp != 32) && (hdr.m_cmap_bpp != 24) && (hdr.m_cmap_bpp != 15) && (hdr.m_cmap_bpp != 16)) || (hdr.m_cmap_len > 256) )
					return nullptr;

				if (hdr.m_cmap_bpp == 32)
				{
					const uint32_t pal_size = hdr.m_cmap_len * 4;
					if (bytes_remaining < pal_size)
						return nullptr;

					for (uint32_t i = 0; i < hdr.m_cmap_len; i++)
					{
						pal[i].r = pSrc[i * 4 + 2];
						pal[i].g = pSrc[i * 4 + 1];
						pal[i].b = pSrc[i * 4 + 0];
						pal[i].a = pSrc[i * 4 + 3];
					}

					bytes_remaining -= pal_size;
					pSrc += pal_size;
				}
				else if (hdr.m_cmap_bpp == 24)
				{
					const uint32_t pal_size = hdr.m_cmap_len * 3;
					if (bytes_remaining < pal_size)
						return nullptr;

					for (uint32_t i = 0; i < hdr.m_cmap_len; i++)
					{
						pal[i].r = pSrc[i * 3 + 2];
						pal[i].g = pSrc[i * 3 + 1];
						pal[i].b = pSrc[i * 3 + 0];
						pal[i].a = 255;
					}

					bytes_remaining -= pal_size;
					pSrc += pal_size;
				}
				else
				{
					const uint32_t pal_size = hdr.m_cmap_len * 2;
					if (bytes_remaining < pal_size)
						return nullptr;

					for (uint32_t i = 0; i < hdr.m_cmap_len; i++)
					{
						const uint32_t v = pSrc[i * 2 + 0] | (pSrc[i * 2 + 1] << 8);

						pal[i].r = (((v >> 10) & 31) * 255 + 15) / 31;
						pal[i].g = (((v >> 5) & 31) * 255 + 15) / 31;
						pal[i].b = ((v & 31) * 255 + 15) / 31;
						pal[i].a = 255;
					}

					bytes_remaining -= pal_size;
					pSrc += pal_size;
				}
			}
			else
			{
				const uint32_t bytes_to_skip = (hdr.m_cmap_bpp >> 3) * hdr.m_cmap_len;
				if (bytes_remaining < bytes_to_skip)
					return nullptr;
				pSrc += bytes_to_skip;
				bytes_remaining += bytes_to_skip;
			}
		}
		
		width = hdr.m_width;
		height = hdr.m_height;

		const uint32_t source_pitch = width * tga_bytes_per_pixel;
		const uint32_t dest_pitch = width * n_chans;
		
		uint8_t *pImage = (uint8_t *)malloc(dest_pitch * height);
		if (!pImage)
			return nullptr;

		std::vector<uint8_t> input_line_buf;
		if (rle_flag)
			input_line_buf.resize(source_pitch);

		int run_type = 0, run_remaining = 0;
		uint8_t run_pixel[4];
		memset(run_pixel, 0, sizeof(run_pixel));

		for (int y = 0; y < height; y++)
		{
			const uint8_t *pLine_data;

			if (rle_flag)
			{
				int pixels_remaining = width;
				uint8_t *pDst = &input_line_buf[0];

				do 
				{
					if (!run_remaining)
					{
						if (bytes_remaining < 1)
						{
							free(pImage);
							return nullptr;
						}

						int v = *pSrc++;
						bytes_remaining--;

						run_type = v & 0x80;
						run_remaining = (v & 0x7F) + 1;

						if (run_type)
						{
							if (bytes_remaining < tga_bytes_per_pixel)
							{
								free(pImage);
								return nullptr;
							}

							memcpy(run_pixel, pSrc, tga_bytes_per_pixel);
							pSrc += tga_bytes_per_pixel;
							bytes_remaining -= tga_bytes_per_pixel;
						}
					}

					const uint32_t n = basisu::minimum<uint32_t>(pixels_remaining, run_remaining);
					pixels_remaining -= n;
					run_remaining -= n;

					if (run_type)
					{
						for (uint32_t i = 0; i < n; i++)
							for (uint32_t j = 0; j < tga_bytes_per_pixel; j++)
								*pDst++ = run_pixel[j];
					}
					else
					{
						const uint32_t bytes_wanted = n * tga_bytes_per_pixel;

						if (bytes_remaining < bytes_wanted)
						{
							free(pImage);
							return nullptr;
						}

						memcpy(pDst, pSrc, bytes_wanted);
						pDst += bytes_wanted;

						pSrc += bytes_wanted;
						bytes_remaining -= bytes_wanted;
					}

				} while (pixels_remaining);

				assert((pDst - &input_line_buf[0]) == (int)(width * tga_bytes_per_pixel));

				pLine_data = &input_line_buf[0];
			}
			else
			{
				if (bytes_remaining < source_pitch)
				{
					free(pImage);
					return nullptr;
				}

				pLine_data = pSrc;
				bytes_remaining -= source_pitch;
				pSrc += source_pitch;
			}

			// Convert to 24bpp RGB or 32bpp RGBA.
			uint8_t *pDst = pImage + (y_flipped ? (height - 1 - y) : y) * dest_pitch + (x_flipped ? (width - 1) * n_chans : 0);
			const int dst_stride = x_flipped ? -((int)n_chans) : n_chans;

			switch (hdr.m_depth)
			{
			case 32:
				assert(tga_bytes_per_pixel == 4 && n_chans == 4);
				for (int i = 0; i < width; i++, pLine_data += 4, pDst += dst_stride)
				{
					pDst[0] = pLine_data[2];
					pDst[1] = pLine_data[1];
					pDst[2] = pLine_data[0];
					pDst[3] = pLine_data[3];
				}
				break;
			case 24:
				assert(tga_bytes_per_pixel == 3 && n_chans == 3);
				for (int i = 0; i < width; i++, pLine_data += 3, pDst += dst_stride)
				{
					pDst[0] = pLine_data[2];
					pDst[1] = pLine_data[1];
					pDst[2] = pLine_data[0];
				}
				break;
			case 16:
			case 15:
				if (image_type == cITRGB)
				{
					assert(tga_bytes_per_pixel == 2 && n_chans == 3);
					for (int i = 0; i < width; i++, pLine_data += 2, pDst += dst_stride)
					{
						const uint32_t v = pLine_data[0] | (pLine_data[1] << 8);
						pDst[0] = (((v >> 10) & 31) * 255 + 15) / 31;
						pDst[1] = (((v >> 5) & 31) * 255 + 15) / 31;
						pDst[2] = ((v & 31) * 255 + 15) / 31;
					}
				}
				else
				{
					assert(image_type == cITGrayscale && tga_bytes_per_pixel == 2 && n_chans == 4);
					for (int i = 0; i < width; i++, pLine_data += 2, pDst += dst_stride)
					{
						pDst[0] = pLine_data[0];
						pDst[1] = pLine_data[0];
						pDst[2] = pLine_data[0];
						pDst[3] = pLine_data[1];
					}
				}
				break;
			case 8:
				assert(tga_bytes_per_pixel == 1);
				if (image_type == cITPalettized)
				{
					if (hdr.m_cmap_bpp == 32)
					{
						assert(n_chans == 4);
						for (int i = 0; i < width; i++, pLine_data++, pDst += dst_stride)
						{
							const uint32_t c = *pLine_data;
							pDst[0] = pal[c].r;
							pDst[1] = pal[c].g;
							pDst[2] = pal[c].b;
							pDst[3] = pal[c].a;
						}
					}
					else
					{
						assert(n_chans == 3);
						for (int i = 0; i < width; i++, pLine_data++, pDst += dst_stride)
						{
							const uint32_t c = *pLine_data;
							pDst[0] = pal[c].r;
							pDst[1] = pal[c].g;
							pDst[2] = pal[c].b;
						}
					}
				}
				else
				{
					assert(n_chans == 3);
					for (int i = 0; i < width; i++, pLine_data++, pDst += dst_stride)
					{
						const uint8_t c = *pLine_data;
						pDst[0] = c;
						pDst[1] = c;
						pDst[2] = c;
					}
				}
				break;
			default:
				assert(0);
				break;
			}
		} // y

		return pImage;
	}

	uint8_t *read_tga(const char *pFilename, int &width, int &height, int &n_chans)
	{
		width = height = n_chans = 0;

		uint8_vec filedata;
		if (!read_file_to_vec(pFilename, filedata))
			return nullptr;

		if (!filedata.size() || (filedata.size() > UINT32_MAX))
			return nullptr;
		
		return read_tga(&filedata[0], (uint32_t)filedata.size(), width, height, n_chans);
	}

	static inline void hdr_convert(const color_rgba& rgbe, vec4F& c)
	{
		if (rgbe[3] != 0)
		{
			float scale = ldexp(1.0f, rgbe[3] - 128 - 8);
			c.set((float)rgbe[0] * scale, (float)rgbe[1] * scale, (float)rgbe[2] * scale, 1.0f);
		}
		else
		{
			c.set(0.0f, 0.0f, 0.0f, 1.0f);
		}
	}

	bool string_begins_with(const std::string& str, const char* pPhrase)
	{
		const size_t str_len = str.size();

		const size_t phrase_len = strlen(pPhrase);
		assert(phrase_len);

		if (str_len >= phrase_len)
		{
#ifdef _MSC_VER
			if (_strnicmp(pPhrase, str.c_str(), phrase_len) == 0)
#else
			if (strncasecmp(pPhrase, str.c_str(), phrase_len) == 0)
#endif
				return true;
		}

		return false;
	}

	// Radiance RGBE (.HDR) image reading.
	// This code tries to preserve the original logic in Radiance's ray/src/common/color.c code:
	// https://www.radiance-online.org/cgi-bin/viewcvs.cgi/ray/src/common/color.c?revision=2.26&view=markup&sortby=log
	// Also see: https://flipcode.com/archives/HDR_Image_Reader.shtml.
	// https://github.com/LuminanceHDR/LuminanceHDR/blob/master/src/Libpfs/io/rgbereader.cpp.
	// https://radsite.lbl.gov/radiance/refer/filefmts.pdf
	// Buggy readers:
	// stb_image.h: appears to be a clone of rgbe.c, but with goto's (doesn't support old format files, doesn't support mixture of RLE/non-RLE scanlines)
	// http://www.graphics.cornell.edu/~bjw/rgbe.html - rgbe.c/h
	// http://www.graphics.cornell.edu/online/formats/rgbe/ - rgbe.c/.h - buggy
	bool read_rgbe(const uint8_vec &filedata, imagef& img, rgbe_header_info& hdr_info)
	{
		hdr_info.clear();

		const uint32_t MAX_SUPPORTED_DIM = 65536;

		if (filedata.size() < 4)
			return false;

		// stb_image.h checks for the string "#?RADIANCE" or "#?RGBE" in the header.
		// The original Radiance header code doesn't care about the specific string.
		// opencv's reader only checks for "#?", so that's what we're going to do.
		if ((filedata[0] != '#') || (filedata[1] != '?'))
			return false;

		//uint32_t width = 0, height = 0;
		bool is_rgbe = false;
		size_t cur_ofs = 0;

		// Parse the lines until we encounter a blank line.
		std::string cur_line;
		for (; ; )
		{
			if (cur_ofs >= filedata.size())
				return false;

			const uint32_t HEADER_TOO_BIG_SIZE = 4096;
			if (cur_ofs >= HEADER_TOO_BIG_SIZE)
			{
				// Header seems too large - something is likely wrong. Return failure.
				return false;
			}

			uint8_t c = filedata[cur_ofs++];

			if (c == '\n')
			{
				if (!cur_line.size())
					break;

				if ((cur_line[0] == '#') && (!string_begins_with(cur_line, "#?")) && (!hdr_info.m_program.size()))
				{
					cur_line.erase(0, 1);
					while (cur_line.size() && (cur_line[0] == ' '))
						cur_line.erase(0, 1);

					hdr_info.m_program = cur_line;
				}
				else if (string_begins_with(cur_line, "EXPOSURE=") && (cur_line.size() > 9))
				{
					hdr_info.m_exposure = atof(cur_line.c_str() + 9);
					hdr_info.m_has_exposure = true;
				}
				else if (string_begins_with(cur_line, "GAMMA=") && (cur_line.size() > 6))
				{
					hdr_info.m_exposure = atof(cur_line.c_str() + 6);
					hdr_info.m_has_gamma = true;
				}
				else if (cur_line == "FORMAT=32-bit_rle_rgbe")
				{
					is_rgbe = true;
				}

				cur_line.resize(0);
			}
			else
				cur_line.push_back((char)c);
		}

		if (!is_rgbe)
			return false;

		// Assume and require the final line to have the image's dimensions. We're not supporting flipping.
		for (; ; )
		{
			if (cur_ofs >= filedata.size())
				return false;
			uint8_t c = filedata[cur_ofs++];
			if (c == '\n')
				break;
			cur_line.push_back((char)c);
		}

		int comp[2] = { 1, 0 }; // y, x (major, minor)
		int dir[2] = { -1, 1 }; // -1, 1, (major, minor), for y -1=up
		uint32_t major_dim = 0, minor_dim = 0;

		// Parse the dimension string, normally it'll be "-Y # +X #" (major, minor), rarely it differs
		for (uint32_t d = 0; d < 2; d++) // 0=major, 1=minor
		{
			const bool is_neg_x = (strncmp(&cur_line[0], "-X ", 3) == 0);
			const bool is_pos_x = (strncmp(&cur_line[0], "+X ", 3) == 0);
			const bool is_x = is_neg_x || is_pos_x;

			const bool is_neg_y = (strncmp(&cur_line[0], "-Y ", 3) == 0);
			const bool is_pos_y = (strncmp(&cur_line[0], "+Y ", 3) == 0);
			const bool is_y = is_neg_y || is_pos_y;

			if (cur_line.size() < 3)
				return false;
			
			if (!is_x && !is_y)
				return false;

			comp[d] = is_x ? 0 : 1;
			dir[d] = (is_neg_x || is_neg_y) ? -1 : 1;
			
			uint32_t& dim = d ? minor_dim : major_dim;

			cur_line.erase(0, 3);

			while (cur_line.size())
			{
				char c = cur_line[0];
				if (c != ' ')
					break;
				cur_line.erase(0, 1);
			}

			bool has_digits = false;
			while (cur_line.size())
			{
				char c = cur_line[0];
				cur_line.erase(0, 1);

				if (c == ' ')
					break;

				if ((c < '0') || (c > '9'))
					return false;

				const uint32_t prev_dim = dim;
				dim = dim * 10 + (c - '0');
				if (dim < prev_dim)
					return false;

				has_digits = true;
			}
			if (!has_digits)
				return false;

			if ((dim < 1) || (dim > MAX_SUPPORTED_DIM))
				return false;
		}
				
		// temp image: width=minor, height=major
		img.resize(minor_dim, major_dim);

		std::vector<color_rgba> temp_scanline(minor_dim);

		// Read the scanlines.
		for (uint32_t y = 0; y < major_dim; y++)
		{
			vec4F* pDst = &img(0, y);

			if ((filedata.size() - cur_ofs) < 4)
				return false;

			// Determine if the line uses the new or old format. See the logic in color.c.
			bool old_decrunch = false;
			if ((minor_dim < 8) || (minor_dim > 0x7FFF))
			{
				// Line is too short or long; must be old format.
				old_decrunch = true;
			}
			else if (filedata[cur_ofs] != 2)
			{
				// R is not 2, must be old format
				old_decrunch = true;
			}
			else
			{
				// c[0]/red is 2.Check GB and E for validity.				
				color_rgba c;
				memcpy(&c, &filedata[cur_ofs], 4);

				if ((c[1] != 2) || (c[2] & 0x80))
				{
					// G isn't 2, or the high bit of B is set which is impossible (image's > 0x7FFF pixels can't get here). Use old format.
					old_decrunch = true;
				}
				else
				{
					// Check B and E. If this isn't the minor_dim in network order, something is wrong. The pixel would also be denormalized, and invalid.
					uint32_t w = (c[2] << 8) | c[3];
					if (w != minor_dim)
						return false;

					cur_ofs += 4;
				}
			}

			if (old_decrunch)
			{
				uint32_t rshift = 0, x = 0;

				while (x < minor_dim)
				{
					if ((filedata.size() - cur_ofs) < 4)
						return false;

					color_rgba c;
					memcpy(&c, &filedata[cur_ofs], 4);
					cur_ofs += 4;

					if ((c[0] == 1) && (c[1] == 1) && (c[2] == 1))
					{
						// We'll allow RLE matches to cross scanlines, but not on the very first pixel.
						if ((!x) && (!y))
							return false;

						const uint32_t run_len = c[3] << rshift;
						const vec4F run_color(pDst[-1]);

						if ((x + run_len) > minor_dim)
							return false;

						for (uint32_t i = 0; i < run_len; i++)
							*pDst++ = run_color;

						rshift += 8;
						x += run_len;
					}
					else
					{
						rshift = 0;

						hdr_convert(c, *pDst);
						pDst++;
						x++;
					}
				}
				continue;
			}

			// New format
			for (uint32_t s = 0; s < 4; s++)
			{
				uint32_t x_ofs = 0;
				while (x_ofs < minor_dim)
				{
					uint32_t num_remaining = minor_dim - x_ofs;

					if (cur_ofs >= filedata.size())
						return false;

					uint8_t count = filedata[cur_ofs++];
					if (count > 128)
					{
						count -= 128;
						if (count > num_remaining)
							return false;

						if (cur_ofs >= filedata.size())
							return false;
						const uint8_t val = filedata[cur_ofs++];

						for (uint32_t i = 0; i < count; i++)
							temp_scanline[x_ofs + i][s] = val;

						x_ofs += count;
					}
					else
					{
						if ((!count) || (count > num_remaining))
							return false;

						for (uint32_t i = 0; i < count; i++)
						{
							if (cur_ofs >= filedata.size())
								return false;
							const uint8_t val = filedata[cur_ofs++];

							temp_scanline[x_ofs + i][s] = val;
						}

						x_ofs += count;
					}
				} // while (x_ofs < minor_dim)
			} // c

			// Convert all the RGBE pixels to float now
			for (uint32_t x = 0; x < minor_dim; x++, pDst++)
				hdr_convert(temp_scanline[x], *pDst);

			assert((pDst - &img(0, y)) == (int)minor_dim);

		} // y

		// at here:
		// img(width,height)=image pixels as read from file, x=minor axis, y=major axis
		// width=minor axis dimension
		// height=major axis dimension
		// in file, pixels are emitted in minor order, them major (so major=scanlines in the file)
		
		imagef final_img;
		if (comp[0] == 0) // if major axis is X
			final_img.resize(major_dim, minor_dim);
		else // major axis is Y, minor is X
			final_img.resize(minor_dim, major_dim);

		// TODO: optimize the identity case
		for (uint32_t major_iter = 0; major_iter < major_dim; major_iter++)
		{
			for (uint32_t minor_iter = 0; minor_iter < minor_dim; minor_iter++)
			{
				const vec4F& p = img(minor_iter, major_iter);

				uint32_t dst_x = 0, dst_y = 0;

				// is the minor dim output x?
				if (comp[1] == 0) 
				{
					// minor axis is x, major is y
					
					// is minor axis (which is output x) flipped?
					if (dir[1] < 0)
						dst_x = minor_dim - 1 - minor_iter;
					else
						dst_x = minor_iter;

					// is major axis (which is output y) flipped? -1=down in raster order, 1=up
					if (dir[0] < 0)
						dst_y = major_iter;
					else
						dst_y = major_dim - 1 - major_iter;
				}
				else
				{
					// minor axis is output y, major is output x

					// is minor axis (which is output y) flipped?
					if (dir[1] < 0)
						dst_y = minor_iter;
					else
						dst_y = minor_dim - 1 - minor_iter;

					// is major axis (which is output x) flipped?
					if (dir[0] < 0)
						dst_x = major_dim - 1 - major_iter;
					else
						dst_x = major_iter;
				}

				final_img(dst_x, dst_y) = p;
			}
		}

		final_img.swap(img);

		return true;
	}

	bool read_rgbe(const char* pFilename, imagef& img, rgbe_header_info& hdr_info)
	{
		uint8_vec filedata;
		if (!read_file_to_vec(pFilename, filedata))
			return false;
		return read_rgbe(filedata, img, hdr_info);
	}

	static uint8_vec& append_string(uint8_vec& buf, const char* pStr)
	{
		const size_t str_len = strlen(pStr);
		if (!str_len)
			return buf;

		const size_t ofs = buf.size();
		buf.resize(ofs + str_len);
		memcpy(&buf[ofs], pStr, str_len);

		return buf;
	}
	
	static uint8_vec& append_string(uint8_vec& buf, const std::string& str)
	{
		if (!str.size())
			return buf;
		return append_string(buf, str.c_str());
	}

	static inline void float2rgbe(color_rgba &rgbe, const vec4F &c)
	{
		const float red = c[0], green = c[1], blue = c[2];
		assert(red >= 0.0f && green >= 0.0f && blue >= 0.0f);

		const float max_v = basisu::maximumf(basisu::maximumf(red, green), blue);

		if (max_v < 1e-32f)
			rgbe.clear();
		else 
		{
			int e;
			const float scale = frexp(max_v, &e) * 256.0f / max_v;
			rgbe[0] = (uint8_t)(clamp<int>((int)(red * scale), 0, 255));
			rgbe[1] = (uint8_t)(clamp<int>((int)(green * scale), 0, 255));
			rgbe[2] = (uint8_t)(clamp<int>((int)(blue * scale), 0, 255));
			rgbe[3] = (uint8_t)(e + 128);
		}
	}

	const bool RGBE_FORCE_RAW = false;
	const bool RGBE_FORCE_OLD_CRUNCH = false; // note must readers (particularly stb_image.h's) don't properly support this, when they should
		
	bool write_rgbe(uint8_vec &file_data, imagef& img, rgbe_header_info& hdr_info)
	{
		if (!img.get_width() || !img.get_height())
			return false;

		const uint32_t width = img.get_width(), height = img.get_height();
		
		file_data.resize(0);
		file_data.reserve(1024 + img.get_width() * img.get_height() * 4);

		append_string(file_data, "#?RADIANCE\n");

		if (hdr_info.m_has_exposure)
			append_string(file_data, string_format("EXPOSURE=%g\n", hdr_info.m_exposure));

		if (hdr_info.m_has_gamma)
			append_string(file_data, string_format("GAMMA=%g\n", hdr_info.m_gamma));

		append_string(file_data, "FORMAT=32-bit_rle_rgbe\n\n");
		append_string(file_data, string_format("-Y %u +X %u\n", height, width));

		if (((width < 8) || (width > 0x7FFF)) || (RGBE_FORCE_RAW))
		{
			for (uint32_t y = 0; y < height; y++)
			{
				for (uint32_t x = 0; x < width; x++)
				{
					color_rgba rgbe;
					float2rgbe(rgbe, img(x, y));
					append_vector(file_data, (const uint8_t *)&rgbe, sizeof(rgbe));
				}
			}
		}
		else if (RGBE_FORCE_OLD_CRUNCH)
		{
			for (uint32_t y = 0; y < height; y++)
			{
				int prev_r = -1, prev_g = -1, prev_b = -1, prev_e = -1;
				uint32_t cur_run_len = 0;
				
				for (uint32_t x = 0; x < width; x++)
				{
					color_rgba rgbe;
					float2rgbe(rgbe, img(x, y));

					if ((rgbe[0] == prev_r) && (rgbe[1] == prev_g) && (rgbe[2] == prev_b) && (rgbe[3] == prev_e))
					{
						if (++cur_run_len == 255)
						{
							// this ensures rshift stays 0, it's lame but this path is only for testing readers
							color_rgba f(1, 1, 1, cur_run_len - 1);
							append_vector(file_data, (const uint8_t*)&f, sizeof(f));
							append_vector(file_data, (const uint8_t*)&rgbe, sizeof(rgbe)); 
							cur_run_len = 0;
						}
					}
					else
					{
						if (cur_run_len > 0)
						{
							color_rgba f(1, 1, 1, cur_run_len);
							append_vector(file_data, (const uint8_t*)&f, sizeof(f));
							
							cur_run_len = 0;
						}
						
						append_vector(file_data, (const uint8_t*)&rgbe, sizeof(rgbe));
																		
						prev_r = rgbe[0];
						prev_g = rgbe[1];
						prev_b = rgbe[2];
						prev_e = rgbe[3];
					}
				} // x

				if (cur_run_len > 0)
				{
					color_rgba f(1, 1, 1, cur_run_len);
					append_vector(file_data, (const uint8_t*)&f, sizeof(f));
				}
			} // y
		}
		else
		{
			uint8_vec temp[4];
			for (uint32_t c = 0; c < 4; c++)
				temp[c].resize(width);

			for (uint32_t y = 0; y < height; y++)
			{
				color_rgba rgbe(2, 2, width >> 8, width & 0xFF);
				append_vector(file_data, (const uint8_t*)&rgbe, sizeof(rgbe));
								
				for (uint32_t x = 0; x < width; x++)
				{
					float2rgbe(rgbe, img(x, y));

					for (uint32_t c = 0; c < 4; c++)
						temp[c][x] = rgbe[c];
				}

				for (uint32_t c = 0; c < 4; c++)
				{
					int raw_ofs = -1;
					
					uint32_t x = 0;
					while (x < width)
					{
						const uint32_t num_bytes_remaining = width - x;
						const uint32_t max_run_len = basisu::minimum<uint32_t>(num_bytes_remaining, 127);
						const uint8_t cur_byte = temp[c][x];

						uint32_t run_len = 1;
						while (run_len < max_run_len)
						{
							if (temp[c][x + run_len] != cur_byte)
								break;
							run_len++;
						}
												
						const uint32_t cost_to_keep_raw = ((raw_ofs != -1) ? 0 : 1) + run_len; // 0 or 1 bytes to start a raw run, then the repeated bytes issued as raw
						const uint32_t cost_to_take_run = 2 + 1; // 2 bytes to issue the RLE, then 1 bytes to start whatever follows it (raw or RLE)

						if ((run_len >= 3) && (cost_to_take_run < cost_to_keep_raw))
						{
							file_data.push_back((uint8_t)(128 + run_len));
							file_data.push_back(cur_byte);

							x += run_len;
							raw_ofs = -1;
						}
						else
						{
							if (raw_ofs < 0)
							{
								raw_ofs = (int)file_data.size();
								file_data.push_back(0);
							}

							if (++file_data[raw_ofs] == 128)
								raw_ofs = -1;

							file_data.push_back(cur_byte);
							
							x++;
						}
					} // x

				} // c
			} // y
		}

		return true;
	}

	bool write_rgbe(const char* pFilename, imagef& img, rgbe_header_info& hdr_info)
	{
		uint8_vec file_data;
		if (!write_rgbe(file_data, img, hdr_info))
			return false;
		return write_vec_to_file(pFilename, file_data);
	}
		
	bool read_exr(const char* pFilename, imagef& img, int& n_chans)
	{
		n_chans = 0;

		int width = 0, height = 0;
		float* out_rgba = nullptr;
		const char* err = nullptr;
		
		int status = LoadEXRWithLayer(&out_rgba, &width, &height, pFilename, nullptr, &err);
		n_chans = 4;
		if (status != 0)
		{
			error_printf("Failed loading .EXR image \"%s\"! (TinyEXR error: %s)\n", pFilename, err ? err : "?");
			FreeEXRErrorMessage(err);
			free(out_rgba);
			return false;
		}

		const uint32_t MAX_SUPPORTED_DIM = 65536;
		if ((width < 1) || (height < 1) || (width > (int)MAX_SUPPORTED_DIM) || (height > (int)MAX_SUPPORTED_DIM))
		{
			error_printf("Invalid dimensions of .EXR image \"%s\"!\n", pFilename);
			free(out_rgba);
			return false;
		}

		img.resize(width, height);
		
		if (n_chans == 1)
		{
			const float* pSrc = out_rgba;
			vec4F* pDst = img.get_ptr();

			for (int y = 0; y < height; y++)
			{
				for (int x = 0; x < width; x++)
				{
					(*pDst)[0] = pSrc[0];
					(*pDst)[1] = pSrc[1];
					(*pDst)[2] = pSrc[2];
					(*pDst)[3] = 1.0f;

					pSrc += 4;
					++pDst;
				}
			}
		}
		else
		{
			memcpy(img.get_ptr(), out_rgba, sizeof(float) * 4 * img.get_total_pixels());
		}

		free(out_rgba);
		return true;
	}

	bool read_exr(const void* pMem, size_t mem_size, imagef& img)
	{
		float* out_rgba = nullptr;
		int width = 0, height = 0;
		const char* pErr = nullptr;
		int res = LoadEXRFromMemory(&out_rgba, &width, &height, (const uint8_t*)pMem, mem_size, &pErr);
		if (res < 0)
		{
			error_printf("Failed loading .EXR image from memory! (TinyEXR error: %s)\n", pErr ? pErr : "?");
			FreeEXRErrorMessage(pErr);
			free(out_rgba);
			return false;
		}

		img.resize(width, height);
		memcpy(img.get_ptr(), out_rgba, width * height * sizeof(float) * 4);
		free(out_rgba);

		return true;
	}

	bool write_exr(const char* pFilename, imagef& img, uint32_t n_chans, uint32_t flags)
	{
		assert((n_chans == 1) || (n_chans == 3) || (n_chans == 4));

		const bool linear_hint = (flags & WRITE_EXR_LINEAR_HINT) != 0, 
			store_float = (flags & WRITE_EXR_STORE_FLOATS) != 0,
			no_compression = (flags & WRITE_EXR_NO_COMPRESSION) != 0;
								
		const uint32_t width = img.get_width(), height = img.get_height();
		assert(width && height);
		
		if (!width || !height)
			return false;
		
		float_vec layers[4];
		float* image_ptrs[4];
		for (uint32_t c = 0; c < n_chans; c++)
		{
			layers[c].resize(width * height);
			image_ptrs[c] = layers[c].get_ptr();
		}

		// ABGR
		int chan_order[4] = { 3, 2, 1, 0 };

		if (n_chans == 1)
		{
			// Y
			chan_order[0] = 0;
		}
		else if (n_chans == 3)
		{
			// BGR
			chan_order[0] = 2;
			chan_order[1] = 1;
			chan_order[2] = 0;
		}
		else if (n_chans != 4)
		{
			assert(0);
			return false;
		}
		
		for (uint32_t y = 0; y < height; y++)
		{
			for (uint32_t x = 0; x < width; x++)
			{
				const vec4F& p = img(x, y);

				for (uint32_t c = 0; c < n_chans; c++)
					layers[c][x + y * width] = p[chan_order[c]];
			} // x
		} // y

		EXRHeader header;
		InitEXRHeader(&header);

		EXRImage image;
		InitEXRImage(&image);

		image.num_channels = n_chans;
		image.images = (unsigned char**)image_ptrs;
		image.width = width;
		image.height = height;

		header.num_channels = n_chans;
		
		header.channels = (EXRChannelInfo*)calloc(header.num_channels, sizeof(EXRChannelInfo));

		// Must be (A)BGR order, since most of EXR viewers expect this channel order.
		for (uint32_t i = 0; i < n_chans; i++)
		{
			char c = 'Y';
			if (n_chans == 3)
				c = "BGR"[i];
			else if (n_chans == 4)
				c = "ABGR"[i];
						
			header.channels[i].name[0] = c;
			header.channels[i].name[1] = '\0';

			header.channels[i].p_linear = linear_hint;
		}
		
		header.pixel_types = (int*)calloc(header.num_channels, sizeof(int));
		header.requested_pixel_types = (int*)calloc(header.num_channels, sizeof(int));
		
		if (!no_compression)
			header.compression_type = TINYEXR_COMPRESSIONTYPE_ZIP;

		for (int i = 0; i < header.num_channels; i++) 
		{
			// pixel type of input image
			header.pixel_types[i] = TINYEXR_PIXELTYPE_FLOAT; 

			// pixel type of output image to be stored in .EXR
			header.requested_pixel_types[i] = store_float ? TINYEXR_PIXELTYPE_FLOAT : TINYEXR_PIXELTYPE_HALF; 
		}

		const char* pErr_msg = nullptr;

		int ret = SaveEXRImageToFile(&image, &header, pFilename, &pErr_msg);
		if (ret != TINYEXR_SUCCESS) 
		{
			error_printf("Save EXR err: %s\n", pErr_msg);
			FreeEXRErrorMessage(pErr_msg);
		}
				
		free(header.channels);
		free(header.pixel_types);
		free(header.requested_pixel_types);

		return (ret == TINYEXR_SUCCESS);
	}

	void image::debug_text(uint32_t x_ofs, uint32_t y_ofs, uint32_t scale_x, uint32_t scale_y, const color_rgba& fg, const color_rgba* pBG, bool alpha_only, const char* pFmt, ...)
	{
		char buf[2048];

		va_list args;
		va_start(args, pFmt);
#ifdef _WIN32		
		vsprintf_s(buf, sizeof(buf), pFmt, args);
#else
		vsnprintf(buf, sizeof(buf), pFmt, args);
#endif
		va_end(args);

		const char* p = buf;

		const uint32_t orig_x_ofs = x_ofs;

		while (*p)
		{
			uint8_t c = *p++;
			if ((c < 32) || (c > 127))
				c = '.';

			const uint8_t* pGlpyh = &g_debug_font8x8_basic[c - 32][0];

			for (uint32_t y = 0; y < 8; y++)
			{
				uint32_t row_bits = pGlpyh[y];
				for (uint32_t x = 0; x < 8; x++)
				{
					const uint32_t q = row_bits & (1 << x);
										
					const color_rgba* pColor = q ? &fg : pBG;
					if (!pColor)
						continue;

					if (alpha_only)
						fill_box_alpha(x_ofs + x * scale_x, y_ofs + y * scale_y, scale_x, scale_y, *pColor);
					else
						fill_box(x_ofs + x * scale_x, y_ofs + y * scale_y, scale_x, scale_y, *pColor);
				}
			}

			x_ofs += 8 * scale_x;
			if ((x_ofs + 8 * scale_x) > m_width)
			{
				x_ofs = orig_x_ofs;
				y_ofs += 8 * scale_y;
			}
		}
	}
	
	// Very basic global Reinhard tone mapping, output converted to sRGB with no dithering, alpha is carried through unchanged. 
	// Only used for debugging/development.
	void tonemap_image_reinhard(image &ldr_img, const imagef &hdr_img, float exposure)
	{
		uint32_t width = hdr_img.get_width(), height = hdr_img.get_height();

		ldr_img.resize(width, height);
				
		for (uint32_t y = 0; y < height; y++)
		{
			for (uint32_t x = 0; x < width; x++)
			{
				vec4F c(hdr_img(x, y));

				for (uint32_t t = 0; t < 3; t++)
				{
					if (c[t] <= 0.0f)
					{
						c[t] = 0.0f;
					}
					else
					{
						c[t] *= exposure;
						c[t] = c[t] / (1.0f + c[t]);
					}
				}

				c.clamp(0.0f, 1.0f);

				c[0] = linear_to_srgb(c[0]) * 255.0f;
				c[1] = linear_to_srgb(c[1]) * 255.0f;
				c[2] = linear_to_srgb(c[2]) * 255.0f;
				c[3] = c[3] * 255.0f;

				color_rgba& o = ldr_img(x, y);
				
				o[0] = (uint8_t)std::round(c[0]);
				o[1] = (uint8_t)std::round(c[1]);
				o[2] = (uint8_t)std::round(c[2]);
				o[3] = (uint8_t)std::round(c[3]);
			}
		}
	}

	bool tonemap_image_compressive(image& dst_img, const imagef& hdr_test_img)
	{
		const uint32_t width = hdr_test_img.get_width();
		const uint32_t height = hdr_test_img.get_height();

		uint16_vec orig_half_img(width * 3 * height);
		uint16_vec half_img(width * 3 * height);

		int max_shift = 32;

		for (uint32_t y = 0; y < height; y++)
		{
			for (uint32_t x = 0; x < width; x++)
			{
				const vec4F& p = hdr_test_img(x, y);

				for (uint32_t i = 0; i < 3; i++)
				{
					if (p[i] < 0.0f)
						return false;
					if (p[i] > basist::MAX_HALF_FLOAT)
						return false;

					uint32_t h = basist::float_to_half(p[i]);
					//uint32_t orig_h = h;

					orig_half_img[(x + y * width) * 3 + i] = (uint16_t)h;

					// Rotate sign bit into LSB
					//h = rot_left16((uint16_t)h, 1);
					//assert(rot_right16((uint16_t)h, 1) == orig_h);
					h <<= 1;

					half_img[(x + y * width) * 3 + i] = (uint16_t)h;

					// Determine # of leading zero bits, ignoring the sign bit
					if (h)
					{
						int lz = clz(h) - 16;
						assert(lz >= 0 && lz <= 16);

						assert((h << lz) <= 0xFFFF);

						max_shift = basisu::minimum<int>(max_shift, lz);
					}
				} // i
			} // x
		} // y

		//printf("tonemap_image_compressive: Max leading zeros: %i\n", max_shift);

		uint32_t high_hist[256];
		clear_obj(high_hist);

		for (uint32_t y = 0; y < height; y++)
		{
			for (uint32_t x = 0; x < width; x++)
			{
				for (uint32_t i = 0; i < 3; i++)
				{
					uint16_t& hf = half_img[(x + y * width) * 3 + i];

					assert(((uint32_t)hf << max_shift) <= 65535);

					hf <<= max_shift;

					uint32_t h = (uint8_t)(hf >> 8);
					high_hist[h]++;
				}
			} // x
		} // y

		uint32_t total_vals_used = 0;
		int remap_old_to_new[256];
		for (uint32_t i = 0; i < 256; i++)
			remap_old_to_new[i] = -1;

		for (uint32_t i = 0; i < 256; i++)
		{
			if (high_hist[i] != 0)
			{
				remap_old_to_new[i] = total_vals_used;
				total_vals_used++;
			}
		}

		assert(total_vals_used >= 1);

		//printf("tonemap_image_compressive: Total used high byte values: %u, unused: %u\n", total_vals_used, 256 - total_vals_used);

		bool val_used[256];
		clear_obj(val_used);

		int remap_new_to_old[256];
		for (uint32_t i = 0; i < 256; i++)
			remap_new_to_old[i] = -1;
		BASISU_NOTE_UNUSED(remap_new_to_old);

		int prev_c = -1;
		BASISU_NOTE_UNUSED(prev_c);
		for (uint32_t i = 0; i < 256; i++)
		{
			if (remap_old_to_new[i] >= 0)
			{
				int c;
				if (total_vals_used <= 1)
					c = remap_old_to_new[i];
				else
				{
					c = (remap_old_to_new[i] * 255 + ((total_vals_used - 1) / 2)) / (total_vals_used - 1);

					assert(c > prev_c);
				}

				assert(!val_used[c]);

				remap_new_to_old[c] = i;

				remap_old_to_new[i] = c;
				prev_c = c;

				//printf("%u ", c);

				val_used[c] = true;
			}
		} // i
		//printf("\n");

		dst_img.resize(width, height);

		for (uint32_t y = 0; y < height; y++)
		{
			for (uint32_t x = 0; x < width; x++)
			{
				for (uint32_t c = 0; c < 3; c++)
				{
					uint16_t& v16 = half_img[(x + y * width) * 3 + c];

					uint32_t hb = v16 >> 8;
					//uint32_t lb = v16 & 0xFF;

					assert(remap_old_to_new[hb] != -1);
					assert(remap_old_to_new[hb] <= 255);
					assert(remap_new_to_old[remap_old_to_new[hb]] == (int)hb);

					hb = remap_old_to_new[hb];

					//v16 = (uint16_t)((hb << 8) | lb);

					dst_img(x, y)[c] = (uint8_t)hb;
				}
			} // x
		} // y

		return true;
	}
					
} // namespace basisu

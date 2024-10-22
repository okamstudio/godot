// basisu_enc.h
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
#include "../transcoder/basisu_transcoder_internal.h"

#include <mutex>
#include <atomic>
#include <condition_variable>
#include <functional>
#include <thread>
#include <unordered_map>
#include <ostream>

#if !defined(_WIN32) || defined(__MINGW32__)
#include <libgen.h>
#endif

// This module is really just a huge grab bag of classes and helper functions needed by the encoder.

// If BASISU_USE_HIGH_PRECISION_COLOR_DISTANCE is 1, quality in perceptual mode will be slightly greater, but at a large increase in encoding CPU time.
#define BASISU_USE_HIGH_PRECISION_COLOR_DISTANCE (0)

#if BASISU_SUPPORT_SSE
// Declared in basisu_kernels_imp.h, but we can't include that here otherwise it would lead to circular type errors.
extern void update_covar_matrix_16x16_sse41(uint32_t num_vecs, const void* pWeighted_vecs, const void* pOrigin, const uint32_t *pVec_indices, void* pMatrix16x16);
#endif

namespace basisu
{
	extern uint8_t g_hamming_dist[256];
	extern const uint8_t g_debug_font8x8_basic[127 - 32 + 1][8];

	// true if basisu_encoder_init() has been called and returned.
	extern bool g_library_initialized;

	// Encoder library initialization.
	// This function MUST be called before encoding anything!
	// Returns false if library initialization fails.
	bool basisu_encoder_init(bool use_opencl = false, bool opencl_force_serialization = false);
	void basisu_encoder_deinit();

	// basisu_kernels_sse.cpp - will be a no-op and g_cpu_supports_sse41 will always be false unless compiled with BASISU_SUPPORT_SSE=1
	extern void detect_sse41();

#if BASISU_SUPPORT_SSE
	extern bool g_cpu_supports_sse41;
#else
	const bool g_cpu_supports_sse41 = false;
#endif

	void error_vprintf(const char* pFmt, va_list args);
	void error_printf(const char *pFmt, ...);
	
	// Helpers

	inline uint8_t clamp255(int32_t i)
	{
		return (uint8_t)((i & 0xFFFFFF00U) ? (~(i >> 31)) : i);
	}

	inline int left_shift32(int val, int shift)
	{
		assert((shift >= 0) && (shift < 32));
		return static_cast<int>(static_cast<uint32_t>(val) << shift);
	}

	inline uint32_t left_shift32(uint32_t val, int shift)
	{
		assert((shift >= 0) && (shift < 32));
		return val << shift;
	}

	inline int32_t clampi(int32_t value, int32_t low, int32_t high) 
	{ 
		if (value < low) 
			value = low; 
		else if (value > high) 
			value = high; 
		return value; 
	}

	inline uint8_t mul_8(uint32_t v, uint32_t a)
	{
		v = v * a + 128; 
		return (uint8_t)((v + (v >> 8)) >> 8);
	}

	inline uint64_t read_bits(const uint8_t* pBuf, uint32_t& bit_offset, uint32_t codesize)
	{
		assert(codesize <= 64);
		uint64_t bits = 0;
		uint32_t total_bits = 0;

		while (total_bits < codesize)
		{
			uint32_t byte_bit_offset = bit_offset & 7;
			uint32_t bits_to_read = minimum<int>(codesize - total_bits, 8 - byte_bit_offset);

			uint32_t byte_bits = pBuf[bit_offset >> 3] >> byte_bit_offset;
			byte_bits &= ((1 << bits_to_read) - 1);

			bits |= ((uint64_t)(byte_bits) << total_bits);

			total_bits += bits_to_read;
			bit_offset += bits_to_read;
		}

		return bits;
	}

	inline uint32_t read_bits32(const uint8_t* pBuf, uint32_t& bit_offset, uint32_t codesize)
	{
		assert(codesize <= 32);
		uint32_t bits = 0;
		uint32_t total_bits = 0;

		while (total_bits < codesize)
		{
			uint32_t byte_bit_offset = bit_offset & 7;
			uint32_t bits_to_read = minimum<int>(codesize - total_bits, 8 - byte_bit_offset);

			uint32_t byte_bits = pBuf[bit_offset >> 3] >> byte_bit_offset;
			byte_bits &= ((1 << bits_to_read) - 1);

			bits |= (byte_bits << total_bits);

			total_bits += bits_to_read;
			bit_offset += bits_to_read;
		}

		return bits;
	}
		
	// Open interval
	inline int bounds_check(int v, int l, int h) { (void)v; (void)l; (void)h; assert(v >= l && v < h); return v; }
	inline uint32_t bounds_check(uint32_t v, uint32_t l, uint32_t h) { (void)v; (void)l; (void)h; assert(v >= l && v < h); return v; }

	// Closed interval
	inline int bounds_check_incl(int v, int l, int h) { (void)v; (void)l; (void)h; assert(v >= l && v <= h); return v; }
	inline uint32_t bounds_check_incl(uint32_t v, uint32_t l, uint32_t h) { (void)v; (void)l; (void)h; assert(v >= l && v <= h); return v; }

	inline uint32_t clz(uint32_t x)
	{
		if (!x)
			return 32;

		uint32_t n = 0;
		while ((x & 0x80000000) == 0)
		{
			x <<= 1u;
			n++;
		}

		return n;
	}

	bool string_begins_with(const std::string& str, const char* pPhrase);
				
	// Hashing
	
	inline uint32_t bitmix32c(uint32_t v) 
	{
		v = (v + 0x7ed55d16) + (v << 12);
		v = (v ^ 0xc761c23c) ^ (v >> 19);
		v = (v + 0x165667b1) + (v << 5);
		v = (v + 0xd3a2646c) ^ (v << 9);
		v = (v + 0xfd7046c5) + (v << 3);
		v = (v ^ 0xb55a4f09) ^ (v >> 16);
		return v;
	}

	inline uint32_t bitmix32(uint32_t v) 
	{
		v -= (v << 6);
		v ^= (v >> 17);
		v -= (v << 9);
		v ^= (v << 4);
		v -= (v << 3);
		v ^= (v << 10);
		v ^= (v >> 15);
		return v;
	}

	inline uint32_t wang_hash(uint32_t seed)
	{
		 seed = (seed ^ 61) ^ (seed >> 16);
		 seed *= 9;
		 seed = seed ^ (seed >> 4);
		 seed *= 0x27d4eb2d;
		 seed = seed ^ (seed >> 15);
		 return seed;
	}

	uint32_t hash_hsieh(const uint8_t* pBuf, size_t len);

	template <typename Key>
	struct bit_hasher
	{
		std::size_t operator()(const Key& k) const
		{
			return hash_hsieh(reinterpret_cast<const uint8_t *>(&k), sizeof(k));
		}
	};

	class running_stat
	{
	public:
		running_stat() { clear(); }

		void clear()
		{
			m_n = 0;
			m_total = 0;
			m_old_m = 0;
			m_new_m = 0;
			m_old_s = 0;
			m_new_s = 0;
			m_min = 0;
			m_max = 0;
		}

		void push(double x)
		{
			m_n++;
			m_total += x;
			if (m_n == 1)
			{
				m_old_m = m_new_m = x;
				m_old_s = 0.0;
				m_min = x;
				m_max = x;
			}
			else
			{
				// See Knuth TAOCP vol 2, 3rd edition, page 232
				m_new_m = m_old_m + (x - m_old_m) / m_n;
				m_new_s = m_old_s + (x - m_old_m) * (x - m_new_m);
				m_old_m = m_new_m;
				m_old_s = m_new_s;
				m_min = basisu::minimum(x, m_min);
				m_max = basisu::maximum(x, m_max);
			}
		}

		uint32_t get_num() const
		{
			return m_n;
		}

		double get_total() const
		{
			return m_total;
		}

		double get_mean() const
		{
			return (m_n > 0) ? m_new_m : 0.0;
		}

		// Returns sample variance
		double get_variance() const
		{
			return ((m_n > 1) ? m_new_s / (m_n - 1) : 0.0);
		}

		double get_std_dev() const
		{
			return sqrt(get_variance());
		}

		double get_min() const
		{
			return m_min;
		}

		double get_max() const
		{
			return m_max;
		}

	private:
		uint32_t m_n;
		double m_total, m_old_m, m_new_m, m_old_s, m_new_s, m_min, m_max;
	};

	// Linear algebra

	template <uint32_t N, typename T>
	class vec
	{
	protected:
		T m_v[N];

	public:
		enum { num_elements = N };
		typedef T scalar_type;

		inline vec() { }
		inline vec(eZero) { set_zero();  }

		explicit inline vec(T val) { set(val); }
		inline vec(T v0, T v1) { set(v0, v1); }
		inline vec(T v0, T v1, T v2) { set(v0, v1, v2); }
		inline vec(T v0, T v1, T v2, T v3) { set(v0, v1, v2, v3); }
		inline vec(const vec &other) { for (uint32_t i = 0; i < N; i++) m_v[i] = other.m_v[i]; }
		template <uint32_t OtherN, typename OtherT> inline vec(const vec<OtherN, OtherT> &other) { set(other); }

		inline T operator[](uint32_t i) const { assert(i < N); return m_v[i]; }
		inline T &operator[](uint32_t i) { assert(i < N); return m_v[i]; }

		inline T getX() const { return m_v[0]; }
		inline T getY() const { static_assert(N >= 2, "N too small"); return m_v[1]; }
		inline T getZ() const { static_assert(N >= 3, "N too small"); return m_v[2]; }
		inline T getW() const { static_assert(N >= 4, "N too small"); return m_v[3]; }

		inline bool operator==(const vec &rhs) const { for (uint32_t i = 0; i < N; i++) if (m_v[i] != rhs.m_v[i]) return false;	return true; }
		inline bool operator<(const vec &rhs) const { for (uint32_t i = 0; i < N; i++) { if (m_v[i] < rhs.m_v[i]) return true; else if (m_v[i] != rhs.m_v[i]) return false; } return false; }

		inline void set_zero() { for (uint32_t i = 0; i < N; i++) m_v[i] = 0; }
		inline void clear() { set_zero(); }

		template <uint32_t OtherN, typename OtherT>
		inline vec &set(const vec<OtherN, OtherT> &other)
		{
			uint32_t i;
			if ((const void *)(&other) == (const void *)(this))
				return *this;
			const uint32_t m = minimum(OtherN, N);
			for (i = 0; i < m; i++)
				m_v[i] = static_cast<T>(other[i]);
			for (; i < N; i++)
				m_v[i] = 0;
			return *this;
		}

		inline vec &set_component(uint32_t index, T val) { assert(index < N); m_v[index] = val; return *this; }
		inline vec &set(T val) { for (uint32_t i = 0; i < N; i++) m_v[i] = val; return *this; }
		inline void clear_elements(uint32_t s, uint32_t e) { assert(e <= N); for (uint32_t i = s; i < e; i++) m_v[i] = 0; }

		inline vec &set(T v0, T v1)
		{
			m_v[0] = v0;
			if (N >= 2)
			{
				m_v[1] = v1;
				clear_elements(2, N);
			}
			return *this;
		}

		inline vec &set(T v0, T v1, T v2)
		{
			m_v[0] = v0;
			if (N >= 2)
			{
				m_v[1] = v1;
				if (N >= 3)
				{
					m_v[2] = v2;
					clear_elements(3, N);
				}
			}
			return *this;
		}

		inline vec &set(T v0, T v1, T v2, T v3)
		{
			m_v[0] = v0;
			if (N >= 2)
			{
				m_v[1] = v1;
				if (N >= 3)
				{
					m_v[2] = v2;

					if (N >= 4)
					{
						m_v[3] = v3;
						clear_elements(5, N);
					}
				}
			}
			return *this;
		}

		inline vec &operator=(const vec &rhs) { if (this != &rhs) for (uint32_t i = 0; i < N; i++) m_v[i] = rhs.m_v[i]; return *this; }
		template <uint32_t OtherN, typename OtherT> inline vec &operator=(const vec<OtherN, OtherT> &rhs) { set(rhs); return *this; }

		inline const T *get_ptr() const { return reinterpret_cast<const T *>(&m_v[0]); }
		inline T *get_ptr() { return reinterpret_cast<T *>(&m_v[0]); }
		
		inline vec operator- () const { vec res; for (uint32_t i = 0; i < N; i++) res.m_v[i] = -m_v[i]; return res; }
		inline vec operator+ () const { return *this; }
		inline vec &operator+= (const vec &other) { for (uint32_t i = 0; i < N; i++) m_v[i] += other.m_v[i]; return *this; }
		inline vec &operator-= (const vec &other) { for (uint32_t i = 0; i < N; i++) m_v[i] -= other.m_v[i]; return *this; }
		inline vec &operator/= (const vec &other) { for (uint32_t i = 0; i < N; i++) m_v[i] /= other.m_v[i]; return *this; }
		inline vec &operator*=(const vec &other) { for (uint32_t i = 0; i < N; i++) m_v[i] *= other.m_v[i]; return *this; }
		inline vec &operator/= (T s) { for (uint32_t i = 0; i < N; i++) m_v[i] /= s; return *this; }
		inline vec &operator*= (T s) { for (uint32_t i = 0; i < N; i++) m_v[i] *= s; return *this; }
		
		friend inline vec operator+(const vec &lhs, const vec &rhs) { vec res; for (uint32_t i = 0; i < N; i++) res.m_v[i] = lhs.m_v[i] + rhs.m_v[i]; return res; }
		friend inline vec operator-(const vec &lhs, const vec &rhs) { vec res; for (uint32_t i = 0; i < N; i++) res.m_v[i] = lhs.m_v[i] - rhs.m_v[i]; return res; }
		friend inline vec operator*(const vec &lhs, T val) { vec res; for (uint32_t i = 0; i < N; i++) res.m_v[i] = lhs.m_v[i] * val; return res; }
		friend inline vec operator*(T val, const vec &rhs) { vec res; for (uint32_t i = 0; i < N; i++) res.m_v[i] = val * rhs.m_v[i]; return res; }
		friend inline vec operator/(const vec &lhs, T val) { vec res; for (uint32_t i = 0; i < N; i++) res.m_v[i] = lhs.m_v[i] / val; return res; }
		friend inline vec operator/(const vec &lhs, const vec &rhs) { vec res; for (uint32_t i = 0; i < N; i++) res.m_v[i] = lhs.m_v[i] / rhs.m_v[i]; return res; }
		
		static inline T dot_product(const vec &lhs, const vec &rhs) { T res = lhs.m_v[0] * rhs.m_v[0]; for (uint32_t i = 1; i < N; i++) res += lhs.m_v[i] * rhs.m_v[i]; return res; }

		inline T dot(const vec &rhs) const { return dot_product(*this, rhs); }

		inline T norm() const { return dot_product(*this, *this); }
		inline T length() const { return sqrt(norm()); }

		inline T squared_distance(const vec &other) const { T d2 = 0; for (uint32_t i = 0; i < N; i++) { T d = m_v[i] - other.m_v[i]; d2 += d * d; } return d2; }
		inline double squared_distance_d(const vec& other) const { double d2 = 0; for (uint32_t i = 0; i < N; i++) { double d = (double)m_v[i] - (double)other.m_v[i]; d2 += d * d; } return d2; }

		inline T distance(const vec &other) const { return static_cast<T>(sqrt(squared_distance(other))); }
		inline double distance_d(const vec& other) const { return sqrt(squared_distance_d(other)); }

		inline vec &normalize_in_place() { T len = length(); if (len != 0.0f) *this *= (1.0f / len); return *this; }

		inline vec &clamp(T l, T h)
		{
			for (uint32_t i = 0; i < N; i++)
				m_v[i] = basisu::clamp(m_v[i], l, h);
			return *this;
		}

		static vec component_min(const vec& a, const vec& b)
		{
			vec res;
			for (uint32_t i = 0; i < N; i++)
				res[i] = minimum(a[i], b[i]);
			return res;
		}

		static vec component_max(const vec& a, const vec& b)
		{
			vec res;
			for (uint32_t i = 0; i < N; i++)
				res[i] = maximum(a[i], b[i]);
			return res;
		}
	};

	typedef vec<4, double> vec4D;
	typedef vec<3, double> vec3D;
	typedef vec<2, double> vec2D;
	typedef vec<1, double> vec1D;

	typedef vec<4, float> vec4F;
	typedef vec<3, float> vec3F;
	typedef vec<2, float> vec2F;
	typedef vec<1, float> vec1F;

	typedef vec<16, float> vec16F;
		
	template <uint32_t Rows, uint32_t Cols, typename T>
	class matrix
	{
	public:
		typedef vec<Rows, T> col_vec;
		typedef vec<Cols, T> row_vec;

		typedef T scalar_type;

		enum { rows = Rows, cols = Cols };

	protected:
		row_vec m_r[Rows];

	public:
		inline matrix() {}
		inline matrix(eZero) { set_zero();  }
		inline matrix(const matrix &other) { for (uint32_t i = 0; i < Rows; i++) m_r[i] = other.m_r[i];	}
		inline matrix &operator=(const matrix &rhs) { if (this != &rhs) for (uint32_t i = 0; i < Rows; i++) m_r[i] = rhs.m_r[i]; return *this; }

		inline T operator()(uint32_t r, uint32_t c) const { assert((r < Rows) && (c < Cols)); return m_r[r][c]; }
		inline T &operator()(uint32_t r, uint32_t c) { assert((r < Rows) && (c < Cols)); return m_r[r][c]; }

		inline const row_vec &operator[](uint32_t r) const { assert(r < Rows); return m_r[r]; }
		inline row_vec &operator[](uint32_t r) { assert(r < Rows); return m_r[r]; }

		inline matrix &set_zero()
		{
			for (uint32_t i = 0; i < Rows; i++)
				m_r[i].set_zero();
			return *this;
		}

		inline matrix &set_identity()
		{
			for (uint32_t i = 0; i < Rows; i++)
			{
				m_r[i].set_zero();
				if (i < Cols)
					m_r[i][i] = 1.0f;
			}
			return *this;
		}
	};

	template<uint32_t N, typename VectorType>
	inline VectorType compute_pca_from_covar(matrix<N, N, float> &cmatrix)
	{
		VectorType axis;
		if (N == 1)
			axis.set(1.0f);
		else
		{
			for (uint32_t i = 0; i < N; i++)
				axis[i] = lerp(.75f, 1.25f, i * (1.0f / maximum<int>(N - 1, 1)));
		}

		VectorType prev_axis(axis);

		// Power iterations
		for (uint32_t power_iter = 0; power_iter < 8; power_iter++)
		{
			VectorType trial_axis;
			double max_sum = 0;

			for (uint32_t i = 0; i < N; i++)
			{
				double sum = 0;
				for (uint32_t j = 0; j < N; j++)
					sum += cmatrix[i][j] * axis[j];

				trial_axis[i] = static_cast<float>(sum);

				max_sum = maximum(fabs(sum), max_sum);
			}

			if (max_sum != 0.0f)
				trial_axis *= static_cast<float>(1.0f / max_sum);

			VectorType delta_axis(prev_axis - trial_axis);

			prev_axis = axis;
			axis = trial_axis;

			if (delta_axis.norm() < .0024f)
				break;
		}

		return axis.normalize_in_place();
	}

	template<typename T> inline void indirect_sort(uint32_t num_indices, uint32_t* pIndices, const T* pKeys)
	{
		for (uint32_t i = 0; i < num_indices; i++)
			pIndices[i] = i;

		std::sort(
			pIndices,
			pIndices + num_indices,
			[pKeys](uint32_t a, uint32_t b) { return pKeys[a] < pKeys[b]; }
		);
	}

	// 1-4 byte direct Radix sort.
	template <typename T>
	T* radix_sort(uint32_t num_vals, T* pBuf0, T* pBuf1, uint32_t key_ofs, uint32_t key_size)
	{
		assert(key_ofs < sizeof(T));
		assert((key_size >= 1) && (key_size <= 4));

		uint32_t hist[256 * 4];

		memset(hist, 0, sizeof(hist[0]) * 256 * key_size);

#define BASISU_GET_KEY(p) (*(uint32_t *)((uint8_t *)(p) + key_ofs))

		if (key_size == 4)
		{
			T* p = pBuf0;
			T* q = pBuf0 + num_vals;
			for (; p != q; p++)
			{
				const uint32_t key = BASISU_GET_KEY(p);

				hist[key & 0xFF]++;
				hist[256 + ((key >> 8) & 0xFF)]++;
				hist[512 + ((key >> 16) & 0xFF)]++;
				hist[768 + ((key >> 24) & 0xFF)]++;
			}
		}
		else if (key_size == 3)
		{
			T* p = pBuf0;
			T* q = pBuf0 + num_vals;
			for (; p != q; p++)
			{
				const uint32_t key = BASISU_GET_KEY(p);

				hist[key & 0xFF]++;
				hist[256 + ((key >> 8) & 0xFF)]++;
				hist[512 + ((key >> 16) & 0xFF)]++;
			}
		}
		else if (key_size == 2)
		{
			T* p = pBuf0;
			T* q = pBuf0 + (num_vals >> 1) * 2;

			for (; p != q; p += 2)
			{
				const uint32_t key0 = BASISU_GET_KEY(p);
				const uint32_t key1 = BASISU_GET_KEY(p + 1);

				hist[key0 & 0xFF]++;
				hist[256 + ((key0 >> 8) & 0xFF)]++;

				hist[key1 & 0xFF]++;
				hist[256 + ((key1 >> 8) & 0xFF)]++;
			}

			if (num_vals & 1)
			{
				const uint32_t key = BASISU_GET_KEY(p);

				hist[key & 0xFF]++;
				hist[256 + ((key >> 8) & 0xFF)]++;
			}
		}
		else
		{
			assert(key_size == 1);
			if (key_size != 1)
				return NULL;

			T* p = pBuf0;
			T* q = pBuf0 + (num_vals >> 1) * 2;

			for (; p != q; p += 2)
			{
				const uint32_t key0 = BASISU_GET_KEY(p);
				const uint32_t key1 = BASISU_GET_KEY(p + 1);

				hist[key0 & 0xFF]++;
				hist[key1 & 0xFF]++;
			}

			if (num_vals & 1)
			{
				const uint32_t key = BASISU_GET_KEY(p);
				hist[key & 0xFF]++;
			}
		}

		T* pCur = pBuf0;
		T* pNew = pBuf1;

		for (uint32_t pass = 0; pass < key_size; pass++)
		{
			const uint32_t* pHist = &hist[pass << 8];

			uint32_t offsets[256];

			uint32_t cur_ofs = 0;
			for (uint32_t i = 0; i < 256; i += 2)
			{
				offsets[i] = cur_ofs;
				cur_ofs += pHist[i];

				offsets[i + 1] = cur_ofs;
				cur_ofs += pHist[i + 1];
			}

			const uint32_t pass_shift = pass << 3;

			T* p = pCur;
			T* q = pCur + (num_vals >> 1) * 2;

			for (; p != q; p += 2)
			{
				uint32_t c0 = (BASISU_GET_KEY(p) >> pass_shift) & 0xFF;
				uint32_t c1 = (BASISU_GET_KEY(p + 1) >> pass_shift) & 0xFF;

				if (c0 == c1)
				{
					uint32_t dst_offset0 = offsets[c0];

					offsets[c0] = dst_offset0 + 2;

					pNew[dst_offset0] = p[0];
					pNew[dst_offset0 + 1] = p[1];
				}
				else
				{
					uint32_t dst_offset0 = offsets[c0]++;
					uint32_t dst_offset1 = offsets[c1]++;

					pNew[dst_offset0] = p[0];
					pNew[dst_offset1] = p[1];
				}
			}

			if (num_vals & 1)
			{
				uint32_t c = (BASISU_GET_KEY(p) >> pass_shift) & 0xFF;

				uint32_t dst_offset = offsets[c];
				offsets[c] = dst_offset + 1;

				pNew[dst_offset] = *p;
			}

			T* t = pCur;
			pCur = pNew;
			pNew = t;
		}

		return pCur;
	}

#undef BASISU_GET_KEY
	
	// Very simple job pool with no dependencies.
	class job_pool
	{
		BASISU_NO_EQUALS_OR_COPY_CONSTRUCT(job_pool);

	public:
		// num_threads is the TOTAL number of job pool threads, including the calling thread! So 2=1 new thread, 3=2 new threads, etc.
		job_pool(uint32_t num_threads);
		~job_pool();
				
		void add_job(const std::function<void()>& job);
		void add_job(std::function<void()>&& job);

		void wait_for_all();

		size_t get_total_threads() const { return 1 + m_threads.size(); }
		
	private:
		std::vector<std::thread> m_threads;
		std::vector<std::function<void()> > m_queue;
		
		std::mutex m_mutex;
		std::condition_variable m_has_work;
		std::condition_variable m_no_more_jobs;
		
		uint32_t m_num_active_jobs;
		
		std::atomic<bool> m_kill_flag;

		void job_thread(uint32_t index);
	};

	// Simple 64-bit color class

	class color_rgba_i16
	{
	public:
		union
		{
			int16_t m_comps[4];

			struct
			{
				int16_t r;
				int16_t g;
				int16_t b;
				int16_t a;
			};
		};

		inline color_rgba_i16()
		{
			static_assert(sizeof(*this) == sizeof(int16_t)*4, "sizeof(*this) == sizeof(int16_t)*4");
		}

		inline color_rgba_i16(int sr, int sg, int sb, int sa)
		{
			set(sr, sg, sb, sa);
		}

		inline color_rgba_i16 &set(int sr, int sg, int sb, int sa)
		{
			m_comps[0] = (int16_t)clamp<int>(sr, INT16_MIN, INT16_MAX);
			m_comps[1] = (int16_t)clamp<int>(sg, INT16_MIN, INT16_MAX);
			m_comps[2] = (int16_t)clamp<int>(sb, INT16_MIN, INT16_MAX);
			m_comps[3] = (int16_t)clamp<int>(sa, INT16_MIN, INT16_MAX);
			return *this;
		}
	};
				
	class color_rgba
	{
	public:
		union
		{
			uint8_t m_comps[4];

			struct
			{
				uint8_t r;
				uint8_t g;
				uint8_t b;
				uint8_t a;
			};
		};

		inline color_rgba()
		{
			static_assert(sizeof(*this) == 4, "sizeof(*this) != 4");
			static_assert(sizeof(*this) == sizeof(basist::color32), "sizeof(*this) != sizeof(basist::color32)");
		}

		// Not too hot about this idea.
		inline color_rgba(const basist::color32& other) :
			r(other.r),
			g(other.g),
			b(other.b),
			a(other.a)
		{
		}

		color_rgba& operator= (const basist::color32& rhs)
		{
			r = rhs.r;
			g = rhs.g;
			b = rhs.b;
			a = rhs.a;
			return *this;
		}

		inline color_rgba(int y)
		{
			set(y);
		}

		inline color_rgba(int y, int na)
		{
			set(y, na);
		}

		inline color_rgba(int sr, int sg, int sb, int sa)
		{
			set(sr, sg, sb, sa);
		}

		inline color_rgba(eNoClamp, int sr, int sg, int sb, int sa)
		{
			set_noclamp_rgba((uint8_t)sr, (uint8_t)sg, (uint8_t)sb, (uint8_t)sa);
		}

		inline color_rgba& set_noclamp_y(int y)
		{
			m_comps[0] = (uint8_t)y;
			m_comps[1] = (uint8_t)y;
			m_comps[2] = (uint8_t)y;
			m_comps[3] = (uint8_t)255;
			return *this;
		}

		inline color_rgba &set_noclamp_rgba(int sr, int sg, int sb, int sa)
		{
			m_comps[0] = (uint8_t)sr;
			m_comps[1] = (uint8_t)sg;
			m_comps[2] = (uint8_t)sb;
			m_comps[3] = (uint8_t)sa;
			return *this;
		}

		inline color_rgba &set(int y)
		{
			m_comps[0] = static_cast<uint8_t>(clamp<int>(y, 0, 255));
			m_comps[1] = m_comps[0];
			m_comps[2] = m_comps[0];
			m_comps[3] = 255;
			return *this;
		}

		inline color_rgba &set(int y, int na)
		{
			m_comps[0] = static_cast<uint8_t>(clamp<int>(y, 0, 255));
			m_comps[1] = m_comps[0];
			m_comps[2] = m_comps[0];
			m_comps[3] = static_cast<uint8_t>(clamp<int>(na, 0, 255));
			return *this;
		}

		inline color_rgba &set(int sr, int sg, int sb, int sa)
		{
			m_comps[0] = static_cast<uint8_t>(clamp<int>(sr, 0, 255));
			m_comps[1] = static_cast<uint8_t>(clamp<int>(sg, 0, 255));
			m_comps[2] = static_cast<uint8_t>(clamp<int>(sb, 0, 255));
			m_comps[3] = static_cast<uint8_t>(clamp<int>(sa, 0, 255));
			return *this;
		}

		inline color_rgba &set_rgb(int sr, int sg, int sb)
		{
			m_comps[0] = static_cast<uint8_t>(clamp<int>(sr, 0, 255));
			m_comps[1] = static_cast<uint8_t>(clamp<int>(sg, 0, 255));
			m_comps[2] = static_cast<uint8_t>(clamp<int>(sb, 0, 255));
			return *this;
		}

		inline color_rgba &set_rgb(const color_rgba &other)
		{
			r = other.r;
			g = other.g;
			b = other.b;
			return *this;
		}

		inline const uint8_t &operator[] (uint32_t index) const { assert(index < 4); return m_comps[index]; }
		inline uint8_t &operator[] (uint32_t index) { assert(index < 4); return m_comps[index]; }
		
		inline void clear()
		{
			m_comps[0] = 0;
			m_comps[1] = 0;
			m_comps[2] = 0;
			m_comps[3] = 0;
		}

		inline bool operator== (const color_rgba &rhs) const
		{
			if (m_comps[0] != rhs.m_comps[0]) return false;
			if (m_comps[1] != rhs.m_comps[1]) return false;
			if (m_comps[2] != rhs.m_comps[2]) return false;
			if (m_comps[3] != rhs.m_comps[3]) return false;
			return true;
		}

		inline bool operator!= (const color_rgba &rhs) const
		{
			return !(*this == rhs);
		}

		inline bool operator<(const color_rgba &rhs) const
		{
			for (int i = 0; i < 4; i++)
			{
				if (m_comps[i] < rhs.m_comps[i])
					return true;
				else if (m_comps[i] != rhs.m_comps[i])
					return false;
			}
			return false;
		}

		inline int get_601_luma() const { return (19595U * m_comps[0] + 38470U * m_comps[1] + 7471U * m_comps[2] + 32768U) >> 16U; }
		inline int get_709_luma() const { return (13938U * m_comps[0] + 46869U * m_comps[1] + 4729U * m_comps[2] + 32768U) >> 16U; } 
		inline int get_luma(bool luma_601) const { return luma_601 ? get_601_luma() : get_709_luma(); }

		inline basist::color32 get_color32() const
		{
			return basist::color32(r, g, b, a);
		}

		static color_rgba comp_min(const color_rgba& a, const color_rgba& b) { return color_rgba(basisu::minimum(a[0], b[0]), basisu::minimum(a[1], b[1]), basisu::minimum(a[2], b[2]), basisu::minimum(a[3], b[3])); }
		static color_rgba comp_max(const color_rgba& a, const color_rgba& b) { return color_rgba(basisu::maximum(a[0], b[0]), basisu::maximum(a[1], b[1]), basisu::maximum(a[2], b[2]), basisu::maximum(a[3], b[3])); }
	};

	typedef basisu::vector<color_rgba> color_rgba_vec;

	const color_rgba g_black_color(0, 0, 0, 255);
	const color_rgba g_black_trans_color(0, 0, 0, 0);
	const color_rgba g_white_color(255, 255, 255, 255);

	inline int color_distance(int r0, int g0, int b0, int r1, int g1, int b1)
	{
		int dr = r0 - r1, dg = g0 - g1, db = b0 - b1;
		return dr * dr + dg * dg + db * db;
	}

	inline int color_distance(int r0, int g0, int b0, int a0, int r1, int g1, int b1, int a1)
	{
		int dr = r0 - r1, dg = g0 - g1, db = b0 - b1, da = a0 - a1;
		return dr * dr + dg * dg + db * db + da * da;
	}

	inline int color_distance(const color_rgba &c0, const color_rgba &c1, bool alpha)
	{
		if (alpha)
			return color_distance(c0.r, c0.g, c0.b, c0.a, c1.r, c1.g, c1.b, c1.a);
		else
			return color_distance(c0.r, c0.g, c0.b, c1.r, c1.g, c1.b);
	}
		
	// TODO: Allow user to control channel weightings.
	inline uint32_t color_distance(bool perceptual, const color_rgba &e1, const color_rgba &e2, bool alpha)
	{
		if (perceptual)
		{
#if BASISU_USE_HIGH_PRECISION_COLOR_DISTANCE
			const float l1 = e1.r * .2126f + e1.g * .715f + e1.b * .0722f;
			const float l2 = e2.r * .2126f + e2.g * .715f + e2.b * .0722f;

			const float cr1 = e1.r - l1;
			const float cr2 = e2.r - l2;

			const float cb1 = e1.b - l1;
			const float cb2 = e2.b - l2;

			const float dl = l1 - l2;
			const float dcr = cr1 - cr2;
			const float dcb = cb1 - cb2;

			uint32_t d = static_cast<uint32_t>(32.0f*4.0f*dl*dl + 32.0f*2.0f*(.5f / (1.0f - .2126f))*(.5f / (1.0f - .2126f))*dcr*dcr + 32.0f*.25f*(.5f / (1.0f - .0722f))*(.5f / (1.0f - .0722f))*dcb*dcb);
			
			if (alpha)
			{
				int da = static_cast<int>(e1.a) - static_cast<int>(e2.a);
				d += static_cast<uint32_t>(128.0f*da*da);
			}

			return d;
#elif 1
			int dr = e1.r - e2.r;
			int dg = e1.g - e2.g;
			int db = e1.b - e2.b;

#if 0
			int delta_l = dr * 27 + dg * 92 + db * 9;
			int delta_cr = dr * 128 - delta_l;
			int delta_cb = db * 128 - delta_l;
															
			uint32_t id = ((uint32_t)(delta_l * delta_l) >> 7U) +
				((((uint32_t)(delta_cr * delta_cr) >> 7U) * 26U) >> 7U) +
				((((uint32_t)(delta_cb * delta_cb) >> 7U) * 3U) >> 7U);
#else
			int64_t delta_l = dr * 27 + dg * 92 + db * 9;
			int64_t delta_cr = dr * 128 - delta_l;
			int64_t delta_cb = db * 128 - delta_l;

			uint32_t id = ((uint32_t)((delta_l * delta_l) >> 7U)) +
				((((uint32_t)((delta_cr * delta_cr) >> 7U)) * 26U) >> 7U) +
				((((uint32_t)((delta_cb * delta_cb) >> 7U)) * 3U) >> 7U);
#endif

			if (alpha)
			{
				int da = (e1.a - e2.a) << 7;
				// This shouldn't overflow if da is 255 or -255: 29.99 bits after squaring.
				id += ((uint32_t)(da * da) >> 7U);
			}

			return id;
#else
			int dr = e1.r - e2.r;
			int dg = e1.g - e2.g;
			int db = e1.b - e2.b;

			int64_t delta_l = dr * 27 + dg * 92 + db * 9;
			int64_t delta_cr = dr * 128 - delta_l;
			int64_t delta_cb = db * 128 - delta_l;

			int64_t id = ((delta_l * delta_l) * 128) +
				((delta_cr * delta_cr) * 26) +
				((delta_cb * delta_cb) * 3);

			if (alpha)
			{
				int64_t da = (e1.a - e2.a);
				id += (da * da) * 128;
			}

			int d = (id + 8192) >> 14;

			return d;
#endif
		}
		else
			return color_distance(e1, e2, alpha);
	}

	static inline uint32_t color_distance_la(const color_rgba& a, const color_rgba& b)
	{
		const int dl = a.r - b.r;
		const int da = a.a - b.a;
		return dl * dl + da * da;
	}

	// String helpers

	inline int string_find_right(const std::string& filename, char c)
	{
		size_t result = filename.find_last_of(c);
		return (result == std::string::npos) ? -1 : (int)result;
	}

	inline std::string string_get_extension(const std::string &filename)
	{
		int sep = -1;
#ifdef _WIN32
		sep = string_find_right(filename, '\\');
#endif
		if (sep < 0)
			sep = string_find_right(filename, '/');

		int dot = string_find_right(filename, '.');
		if (dot <= sep)
			return "";

		std::string result(filename);
		result.erase(0, dot + 1);

		return result;
	}

	inline bool string_remove_extension(std::string &filename)
	{
		int sep = -1;
#ifdef _WIN32
		sep = string_find_right(filename, '\\');
#endif
		if (sep < 0)
			sep = string_find_right(filename, '/');

		int dot = string_find_right(filename, '.');
		if ((dot < sep) || (dot < 0))
			return false;

		filename.resize(dot);

		return true;
	}

	inline std::string string_format(const char* pFmt, ...)
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

		return std::string(buf);
	}

	inline std::string string_tolower(const std::string& s)
	{
		std::string result(s);
		for (size_t i = 0; i < result.size(); i++)
		{
			result[i] = (char)tolower((uint8_t)(result[i]));
		}
		return result;
	}

	inline char *strcpy_safe(char *pDst, size_t dst_len, const char *pSrc)
	{
		assert(pDst && pSrc && dst_len);
		if (!dst_len)
			return pDst;

		const size_t src_len = strlen(pSrc);
		const size_t src_len_plus_terminator = src_len + 1;

		if (src_len_plus_terminator <= dst_len)
			memcpy(pDst, pSrc, src_len_plus_terminator);
		else
		{
			if (dst_len > 1)
				memcpy(pDst, pSrc, dst_len - 1);
			pDst[dst_len - 1] = '\0';
		}

		return pDst;
	}

	inline bool string_ends_with(const std::string& s, char c)
	{
		return (s.size() != 0) && (s.back() == c);
	}

	inline bool string_split_path(const char *p, std::string *pDrive, std::string *pDir, std::string *pFilename, std::string *pExt)
	{
#ifdef _MSC_VER
		char drive_buf[_MAX_DRIVE] = { 0 };
		char dir_buf[_MAX_DIR] = { 0 };
		char fname_buf[_MAX_FNAME] = { 0 };
		char ext_buf[_MAX_EXT] = { 0 };

		errno_t error = _splitpath_s(p, 
			pDrive ? drive_buf : NULL, pDrive ? _MAX_DRIVE : 0,
			pDir ? dir_buf : NULL, pDir ? _MAX_DIR : 0,
			pFilename ? fname_buf : NULL, pFilename ? _MAX_FNAME : 0,
			pExt ? ext_buf : NULL, pExt ? _MAX_EXT : 0);
		if (error != 0)
			return false;

		if (pDrive) *pDrive = drive_buf;
		if (pDir) *pDir = dir_buf;
		if (pFilename) *pFilename = fname_buf;
		if (pExt) *pExt = ext_buf;
		return true;
#else
		char dirtmp[1024], nametmp[1024];
		strcpy_safe(dirtmp, sizeof(dirtmp), p);
		strcpy_safe(nametmp, sizeof(nametmp), p);

		if (pDrive)
			pDrive->resize(0);

		const char *pDirName = dirname(dirtmp);
		const char* pBaseName = basename(nametmp);
		if ((!pDirName) || (!pBaseName))
			return false;

		if (pDir)
		{
			*pDir = pDirName;
			if ((pDir->size()) && (pDir->back() != '/'))
				*pDir += "/";
		}
				
		if (pFilename)
		{
			*pFilename = pBaseName;
			string_remove_extension(*pFilename);
		}

		if (pExt)
		{
			*pExt = pBaseName;
			*pExt = string_get_extension(*pExt);
			if (pExt->size())
				*pExt = "." + *pExt;
		}

		return true;
#endif
	}

	inline bool is_path_separator(char c)
	{
#ifdef _WIN32
		return (c == '/') || (c == '\\');
#else
		return (c == '/');
#endif
	}
		
	inline bool is_drive_separator(char c)
	{
#ifdef _WIN32
		return (c == ':');
#else
		(void)c;
		return false;
#endif
	}

	inline void string_combine_path(std::string &dst, const char *p, const char *q)
	{
		std::string temp(p);
		if (temp.size() && !is_path_separator(q[0]))
		{
			if (!is_path_separator(temp.back()))
				temp.append(1, BASISU_PATH_SEPERATOR_CHAR);
		}
		temp += q;
		dst.swap(temp);
	}

	inline void string_combine_path(std::string &dst, const char *p, const char *q, const char *r)
	{
		string_combine_path(dst, p, q);
		string_combine_path(dst, dst.c_str(), r);
	}
		
	inline void string_combine_path_and_extension(std::string &dst, const char *p, const char *q, const char *r, const char *pExt)
	{
		string_combine_path(dst, p, q, r);
		if ((!string_ends_with(dst, '.')) && (pExt[0]) && (pExt[0] != '.'))
			dst.append(1, '.');
		dst.append(pExt);
	}

	inline bool string_get_pathname(const char *p, std::string &path)
	{
		std::string temp_drive, temp_path;
		if (!string_split_path(p, &temp_drive, &temp_path, NULL, NULL))
			return false;
		string_combine_path(path, temp_drive.c_str(), temp_path.c_str());
		return true;
	}

	inline bool string_get_filename(const char *p, std::string &filename)
	{
		std::string temp_ext;
		if (!string_split_path(p, nullptr, nullptr, &filename, &temp_ext))
			return false;
		filename += temp_ext;
		return true;
	}

	class rand
	{
		std::mt19937 m_mt;

	public:
		rand() {	}

		rand(uint32_t s) { seed(s); }
		void seed(uint32_t s) { m_mt.seed(s); }

		// between [l,h]
		int irand(int l, int h) { std::uniform_int_distribution<int> d(l, h); return d(m_mt); }

		uint32_t urand32() { return static_cast<uint32_t>(irand(INT32_MIN, INT32_MAX)); }

		bool bit() { return irand(0, 1) == 1; }

		uint8_t byte() { return static_cast<uint8_t>(urand32()); }

		// between [l,h)
		float frand(float l, float h) { std::uniform_real_distribution<float> d(l, h); return d(m_mt); }

		float gaussian(float mean, float stddev) { std::normal_distribution<float> d(mean, stddev); return d(m_mt); }
	};

	class priority_queue
	{
	public:
		priority_queue() :
			m_size(0)
		{
		}

		void clear()
		{
			m_heap.clear();
			m_size = 0;
		}

		void init(uint32_t max_entries, uint32_t first_index, float first_priority)
		{
			m_heap.resize(max_entries + 1);
			m_heap[1].m_index = first_index;
			m_heap[1].m_priority = first_priority;
			m_size = 1;
		}

		inline uint32_t size() const { return m_size; }

		inline uint32_t get_top_index() const { return m_heap[1].m_index; }
		inline float get_top_priority() const { return m_heap[1].m_priority; }

		inline void delete_top()
		{
			assert(m_size > 0);
			m_heap[1] = m_heap[m_size];
			m_size--;
			if (m_size)
				down_heap(1);
		}

		inline void add_heap(uint32_t index, float priority)
		{
			m_size++;

			uint32_t k = m_size;

			if (m_size >= m_heap.size())
				m_heap.resize(m_size + 1);

			for (;;)
			{
				uint32_t parent_index = k >> 1;
				if ((!parent_index) || (m_heap[parent_index].m_priority > priority))
					break;
				m_heap[k] = m_heap[parent_index];
				k = parent_index;
			}

			m_heap[k].m_index = index;
			m_heap[k].m_priority = priority;
		}

	private:
		struct entry
		{
			uint32_t m_index;
			float m_priority;
		};

		basisu::vector<entry> m_heap;
		uint32_t m_size;

		// Push down entry at index
		inline void down_heap(uint32_t heap_index)
		{
			uint32_t orig_index = m_heap[heap_index].m_index;
			const float orig_priority = m_heap[heap_index].m_priority;

			uint32_t child_index;
			while ((child_index = (heap_index << 1)) <= m_size)
			{
				if ((child_index < m_size) && (m_heap[child_index].m_priority < m_heap[child_index + 1].m_priority)) ++child_index;
				if (orig_priority > m_heap[child_index].m_priority)
					break;
				m_heap[heap_index] = m_heap[child_index];
				heap_index = child_index;
			}

			m_heap[heap_index].m_index = orig_index;
			m_heap[heap_index].m_priority = orig_priority;
		}
	};

	// Tree structured vector quantization (TSVQ)

	template <typename TrainingVectorType>
	class tree_vector_quant
	{
	public:
		typedef TrainingVectorType training_vec_type;
		typedef std::pair<TrainingVectorType, uint64_t> training_vec_with_weight;
		typedef basisu::vector< training_vec_with_weight > array_of_weighted_training_vecs;

		tree_vector_quant() :
			m_next_codebook_index(0)
		{
		}

		void clear()
		{
			clear_vector(m_training_vecs);
			clear_vector(m_nodes);
			m_next_codebook_index = 0;
		}

		void add_training_vec(const TrainingVectorType &v, uint64_t weight) { m_training_vecs.push_back(std::make_pair(v, weight)); }

		size_t get_total_training_vecs() const { return m_training_vecs.size(); }
		const array_of_weighted_training_vecs &get_training_vecs() const	{ return m_training_vecs; }
			  array_of_weighted_training_vecs &get_training_vecs()			{ return m_training_vecs; }

		void retrieve(basisu::vector< basisu::vector<uint32_t> > &codebook) const
		{
			for (uint32_t i = 0; i < m_nodes.size(); i++)
			{
				const tsvq_node &n = m_nodes[i];
				if (!n.is_leaf())
					continue;

				codebook.resize(codebook.size() + 1);
				codebook.back() = n.m_training_vecs;
			}
		}

		void retrieve(basisu::vector<TrainingVectorType> &codebook) const
		{
			for (uint32_t i = 0; i < m_nodes.size(); i++)
			{
				const tsvq_node &n = m_nodes[i];
				if (!n.is_leaf())
					continue;

				codebook.resize(codebook.size() + 1);
				codebook.back() = n.m_origin;
			}
		}

		void retrieve(uint32_t max_clusters, basisu::vector<uint_vec> &codebook) const
		{
			uint_vec node_stack;
			node_stack.reserve(512);

			codebook.resize(0);
			codebook.reserve(max_clusters);
			         
			uint32_t node_index = 0;

			while (true)
			{
				const tsvq_node& cur = m_nodes[node_index];

				if (cur.is_leaf() || ((2 + cur.m_codebook_index) > (int)max_clusters))
				{
					codebook.resize(codebook.size() + 1);
					codebook.back() = cur.m_training_vecs;
										
					if (node_stack.empty())
						break;

					node_index = node_stack.back();
					node_stack.pop_back();
					continue;
				}
				            
				node_stack.push_back(cur.m_right_index);
					node_index = cur.m_left_index;
			}
		}

		bool generate(uint32_t max_size)
		{
			if (!m_training_vecs.size())
				return false;

			m_next_codebook_index = 0;

			clear_vector(m_nodes);
			m_nodes.reserve(max_size * 2 + 1);

			m_nodes.push_back(prepare_root());

			priority_queue var_heap;
			var_heap.init(max_size, 0, m_nodes[0].m_var);

			basisu::vector<uint32_t> l_children, r_children;

			// Now split the worst nodes
			l_children.reserve(m_training_vecs.size() + 1);
			r_children.reserve(m_training_vecs.size() + 1);

			uint32_t total_leaf_nodes = 1;

			//interval_timer tm;
			//tm.start();

			while ((var_heap.size()) && (total_leaf_nodes < max_size))
			{
				const uint32_t node_index = var_heap.get_top_index();
				const tsvq_node &node = m_nodes[node_index];

				assert(node.m_var == var_heap.get_top_priority());
				assert(node.is_leaf());

				var_heap.delete_top();
								
				if (node.m_training_vecs.size() > 1)
				{
					if (split_node(node_index, var_heap, l_children, r_children))
					{
						// This removes one leaf node (making an internal node) and replaces it with two new leaves, so +1 total.
						total_leaf_nodes += 1;
					}
				}
			}

			//debug_printf("tree_vector_quant::generate %u: %3.3f secs\n", TrainingVectorType::num_elements, tm.get_elapsed_secs());

			return true;
		}

	private:
		class tsvq_node
		{
		public:
			inline tsvq_node() : m_weight(0), m_origin(cZero), m_left_index(-1), m_right_index(-1), m_codebook_index(-1) { }

			// vecs is erased
			inline void set(const TrainingVectorType &org, uint64_t weight, float var, basisu::vector<uint32_t> &vecs) { m_origin = org; m_weight = weight; m_var = var; m_training_vecs.swap(vecs); }

			inline bool is_leaf() const { return m_left_index < 0; }

			float m_var;
			uint64_t m_weight;
			TrainingVectorType m_origin;
			int32_t m_left_index, m_right_index;
			basisu::vector<uint32_t> m_training_vecs;
			int m_codebook_index;
		};

		typedef basisu::vector<tsvq_node> tsvq_node_vec;
		tsvq_node_vec m_nodes;

		array_of_weighted_training_vecs m_training_vecs;

		uint32_t m_next_codebook_index;

		tsvq_node prepare_root() const
		{
			double ttsum = 0.0f;

			// Prepare root node containing all training vectors
			tsvq_node root;
			root.m_training_vecs.reserve(m_training_vecs.size());

			for (uint32_t i = 0; i < m_training_vecs.size(); i++)
			{
				const TrainingVectorType &v = m_training_vecs[i].first;
				const uint64_t weight = m_training_vecs[i].second;

				root.m_training_vecs.push_back(i);

				root.m_origin += (v * static_cast<float>(weight));
				root.m_weight += weight;

				ttsum += v.dot(v) * weight;
			}

			root.m_var = static_cast<float>(ttsum - (root.m_origin.dot(root.m_origin) / root.m_weight));

			root.m_origin *= (1.0f / root.m_weight);

			return root;
		}

		bool split_node(uint32_t node_index, priority_queue &var_heap, basisu::vector<uint32_t> &l_children, basisu::vector<uint32_t> &r_children)
		{
			TrainingVectorType l_child_org, r_child_org;
			uint64_t l_weight = 0, r_weight = 0;
			float l_var = 0.0f, r_var = 0.0f;

			// Compute initial left/right child origins
			if (!prep_split(m_nodes[node_index], l_child_org, r_child_org))
				return false;

			// Use k-means iterations to refine these children vectors
			if (!refine_split(m_nodes[node_index], l_child_org, l_weight, l_var, l_children, r_child_org, r_weight, r_var, r_children))
				return false;

			// Create children
			const uint32_t l_child_index = (uint32_t)m_nodes.size(), r_child_index = (uint32_t)m_nodes.size() + 1;

			m_nodes[node_index].m_left_index = l_child_index;
			m_nodes[node_index].m_right_index = r_child_index;
			
			m_nodes[node_index].m_codebook_index = m_next_codebook_index;
			m_next_codebook_index++;

			m_nodes.resize(m_nodes.size() + 2);

			tsvq_node &l_child = m_nodes[l_child_index], &r_child = m_nodes[r_child_index];

			l_child.set(l_child_org, l_weight, l_var, l_children);
			r_child.set(r_child_org, r_weight, r_var, r_children);

			if ((l_child.m_var <= 0.0f) && (l_child.m_training_vecs.size() > 1))
			{
				TrainingVectorType v(m_training_vecs[l_child.m_training_vecs[0]].first);
				
				for (uint32_t i = 1; i < l_child.m_training_vecs.size(); i++)
				{
					if (!(v == m_training_vecs[l_child.m_training_vecs[i]].first))
					{
						l_child.m_var = 1e-4f;
						break;
					}
				}
			}

			if ((r_child.m_var <= 0.0f) && (r_child.m_training_vecs.size() > 1))
			{
				TrainingVectorType v(m_training_vecs[r_child.m_training_vecs[0]].first);

				for (uint32_t i = 1; i < r_child.m_training_vecs.size(); i++)
				{
					if (!(v == m_training_vecs[r_child.m_training_vecs[i]].first))
					{
						r_child.m_var = 1e-4f;
						break;
					}
				}
			}

			if ((l_child.m_var > 0.0f) && (l_child.m_training_vecs.size() > 1))
				var_heap.add_heap(l_child_index, l_child.m_var);
						
			if ((r_child.m_var > 0.0f) && (r_child.m_training_vecs.size() > 1))
				var_heap.add_heap(r_child_index, r_child.m_var);
						
			return true;
		}

		TrainingVectorType compute_split_axis(const tsvq_node &node) const
		{
			const uint32_t N = TrainingVectorType::num_elements;

			matrix<N, N, float> cmatrix;

			if ((N != 16) || (!g_cpu_supports_sse41))
			{
				cmatrix.set_zero();

				// Compute covariance matrix from weighted input vectors
				for (uint32_t i = 0; i < node.m_training_vecs.size(); i++)
				{
					const TrainingVectorType v(m_training_vecs[node.m_training_vecs[i]].first - node.m_origin);
					const TrainingVectorType w(static_cast<float>(m_training_vecs[node.m_training_vecs[i]].second) * v);

					for (uint32_t x = 0; x < N; x++)
						for (uint32_t y = x; y < N; y++)
							cmatrix[x][y] = cmatrix[x][y] + v[x] * w[y];
				}
			}
			else
			{
#if BASISU_SUPPORT_SSE
				// Specialize the case with 16x16 matrices, which are quite expensive without SIMD.
				// This SSE function takes pointers to void types, so do some sanity checks.
				assert(sizeof(TrainingVectorType) == sizeof(float) * 16);
				assert(sizeof(training_vec_with_weight) == sizeof(std::pair<vec16F, uint64_t>));
				update_covar_matrix_16x16_sse41(node.m_training_vecs.size(), m_training_vecs.data(), &node.m_origin, node.m_training_vecs.data(), &cmatrix);
#endif
			}

			const float renorm_scale = 1.0f / node.m_weight;

			for (uint32_t x = 0; x < N; x++)
				for (uint32_t y = x; y < N; y++)
					cmatrix[x][y] *= renorm_scale;

			// Diagonal flip
			for (uint32_t x = 0; x < (N - 1); x++)
				for (uint32_t y = x + 1; y < N; y++)
					cmatrix[y][x] = cmatrix[x][y];

			return compute_pca_from_covar<N, TrainingVectorType>(cmatrix);
		}

		bool prep_split(const tsvq_node &node, TrainingVectorType &l_child_result, TrainingVectorType &r_child_result) const
		{
			//const uint32_t N = TrainingVectorType::num_elements;

			if (2 == node.m_training_vecs.size())
			{
				l_child_result = m_training_vecs[node.m_training_vecs[0]].first;
				r_child_result = m_training_vecs[node.m_training_vecs[1]].first;
				return true;
			}

			TrainingVectorType axis(compute_split_axis(node)), l_child(0.0f), r_child(0.0f);
			double l_weight = 0.0f, r_weight = 0.0f;

			// Compute initial left/right children
			for (uint32_t i = 0; i < node.m_training_vecs.size(); i++)
			{
				const float weight = (float)m_training_vecs[node.m_training_vecs[i]].second;

				const TrainingVectorType &v = m_training_vecs[node.m_training_vecs[i]].first;

				double t = (v - node.m_origin).dot(axis);
				if (t >= 0.0f)
				{
					r_child += v * weight;
					r_weight += weight;
				}
				else
				{
					l_child += v * weight;
					l_weight += weight;
				}
			}

			if ((l_weight > 0.0f) && (r_weight > 0.0f))
			{
				l_child_result = l_child * static_cast<float>(1.0f / l_weight);
				r_child_result = r_child * static_cast<float>(1.0f / r_weight);
			}
			else
			{
				TrainingVectorType l(1e+20f);
				TrainingVectorType h(-1e+20f);
				for (uint32_t i = 0; i < node.m_training_vecs.size(); i++)
				{
					const TrainingVectorType& v = m_training_vecs[node.m_training_vecs[i]].first;
					
					l = TrainingVectorType::component_min(l, v);
					h = TrainingVectorType::component_max(h, v);
				}

				TrainingVectorType r(h - l);

				float largest_axis_v = 0.0f;
				int largest_axis_index = -1;
				for (uint32_t i = 0; i < TrainingVectorType::num_elements; i++)
				{
					if (r[i] > largest_axis_v)
					{
						largest_axis_v = r[i];
						largest_axis_index = i;
					}
				}

				if (largest_axis_index < 0)
					return false;

				basisu::vector<float> keys(node.m_training_vecs.size());
				for (uint32_t i = 0; i < node.m_training_vecs.size(); i++)
					keys[i] = m_training_vecs[node.m_training_vecs[i]].first[largest_axis_index];

				uint_vec indices(node.m_training_vecs.size());
				indirect_sort((uint32_t)node.m_training_vecs.size(), &indices[0], &keys[0]);

				l_child.set_zero();
				l_weight = 0;

				r_child.set_zero();
				r_weight = 0;

				const uint32_t half_index = (uint32_t)node.m_training_vecs.size() / 2;
				for (uint32_t i = 0; i < node.m_training_vecs.size(); i++)
				{
					const float weight = (float)m_training_vecs[node.m_training_vecs[i]].second;

					const TrainingVectorType& v = m_training_vecs[node.m_training_vecs[i]].first;

					if (i < half_index)
					{
						l_child += v * weight;
						l_weight += weight;
					}
					else
					{
						r_child += v * weight;
						r_weight += weight;
					}
				}

				if ((l_weight > 0.0f) && (r_weight > 0.0f))
				{
					l_child_result = l_child * static_cast<float>(1.0f / l_weight);
					r_child_result = r_child * static_cast<float>(1.0f / r_weight);
				}
				else
				{
					l_child_result = l;
					r_child_result = h;
				}
			}

			return true;
		}

		bool refine_split(const tsvq_node &node,
			TrainingVectorType &l_child, uint64_t &l_weight, float &l_var, basisu::vector<uint32_t> &l_children,
			TrainingVectorType &r_child, uint64_t &r_weight, float &r_var, basisu::vector<uint32_t> &r_children) const
		{
			l_children.reserve(node.m_training_vecs.size());
			r_children.reserve(node.m_training_vecs.size());

			float prev_total_variance = 1e+10f;

			// Refine left/right children locations using k-means iterations
			const uint32_t cMaxIters = 6;
			for (uint32_t iter = 0; iter < cMaxIters; iter++)
			{
				l_children.resize(0); 
				r_children.resize(0); 

				TrainingVectorType new_l_child(cZero), new_r_child(cZero);

				double l_ttsum = 0.0f, r_ttsum = 0.0f;

				l_weight = 0;
				r_weight = 0;

				for (uint32_t i = 0; i < node.m_training_vecs.size(); i++)
				{
					const TrainingVectorType &v = m_training_vecs[node.m_training_vecs[i]].first;
					const uint64_t weight = m_training_vecs[node.m_training_vecs[i]].second;

					double left_dist2 = l_child.squared_distance_d(v), right_dist2 = r_child.squared_distance_d(v);

					if (left_dist2 >= right_dist2)
					{
						new_r_child += (v * static_cast<float>(weight));
						r_weight += weight;

						r_ttsum += weight * v.dot(v);
						r_children.push_back(node.m_training_vecs[i]);
					}
					else
					{
						new_l_child += (v * static_cast<float>(weight));
						l_weight += weight;

						l_ttsum += weight * v.dot(v);
						l_children.push_back(node.m_training_vecs[i]);
					}
				}

				// Node is unsplittable using the above algorithm - try something else to split it up.
				if ((!l_weight) || (!r_weight))
				{
					l_children.resize(0);
					new_l_child.set(0.0f);
					l_ttsum = 0.0f;
					l_weight = 0;

					r_children.resize(0);
					new_r_child.set(0.0f);
					r_ttsum = 0.0f;
					r_weight = 0;

					TrainingVectorType firstVec;
					for (uint32_t i = 0; i < node.m_training_vecs.size(); i++)
					{
						const TrainingVectorType& v = m_training_vecs[node.m_training_vecs[i]].first;
						const uint64_t weight = m_training_vecs[node.m_training_vecs[i]].second;
					
						if ((!i) || (v == firstVec))
						{
							firstVec = v;

							new_r_child += (v * static_cast<float>(weight));
							r_weight += weight;

							r_ttsum += weight * v.dot(v);
							r_children.push_back(node.m_training_vecs[i]);
						}
						else
						{
							new_l_child += (v * static_cast<float>(weight));
							l_weight += weight;

							l_ttsum += weight * v.dot(v);
							l_children.push_back(node.m_training_vecs[i]);
						}
					}

					if ((!l_weight) || (!r_weight))
						return false;
				}

				l_var = static_cast<float>(l_ttsum - (new_l_child.dot(new_l_child) / l_weight));
				r_var = static_cast<float>(r_ttsum - (new_r_child.dot(new_r_child) / r_weight));

				new_l_child *= (1.0f / l_weight);
				new_r_child *= (1.0f / r_weight);

				l_child = new_l_child;
				r_child = new_r_child;

				float total_var = l_var + r_var;
				const float cGiveupVariance = .00001f;
				if (total_var < cGiveupVariance)
					break;

				// Check to see if the variance has settled
				const float cVarianceDeltaThresh = .00125f;
				if (((prev_total_variance - total_var) / total_var) < cVarianceDeltaThresh)
					break;

				prev_total_variance = total_var;
			}

			return true;
		}
	};

	struct weighted_block_group
	{
		uint64_t m_total_weight;
		uint_vec m_indices;
	};

	template<typename Quantizer>
	bool generate_hierarchical_codebook_threaded_internal(Quantizer& q,
		uint32_t max_codebook_size, uint32_t max_parent_codebook_size,
		basisu::vector<uint_vec>& codebook,
		basisu::vector<uint_vec>& parent_codebook,
		uint32_t max_threads, bool limit_clusterizers, job_pool *pJob_pool)
	{
		codebook.resize(0);
		parent_codebook.resize(0);

		if ((max_threads <= 1) || (q.get_training_vecs().size() < 256) || (max_codebook_size < max_threads * 16))
		{
			if (!q.generate(max_codebook_size))
				return false;

			q.retrieve(codebook);

			if (max_parent_codebook_size)
				q.retrieve(max_parent_codebook_size, parent_codebook);

			return true;
		}

		const uint32_t cMaxThreads = 16;
		if (max_threads > cMaxThreads)
			max_threads = cMaxThreads;

		if (!q.generate(max_threads))
			return false;

		basisu::vector<uint_vec> initial_codebook;

		q.retrieve(initial_codebook);

		if (initial_codebook.size() < max_threads)
		{
			codebook = initial_codebook;

			if (max_parent_codebook_size)
				q.retrieve(max_parent_codebook_size, parent_codebook);

			return true;
		}

		Quantizer quantizers[cMaxThreads];
		
		bool success_flags[cMaxThreads];
		clear_obj(success_flags);

		basisu::vector<uint_vec> local_clusters[cMaxThreads];
		basisu::vector<uint_vec> local_parent_clusters[cMaxThreads];

		for (uint32_t thread_iter = 0; thread_iter < max_threads; thread_iter++)
		{
#ifndef __EMSCRIPTEN__
			pJob_pool->add_job( [thread_iter, &local_clusters, &local_parent_clusters, &success_flags, &quantizers, &initial_codebook, &q, &limit_clusterizers, &max_codebook_size, &max_threads, &max_parent_codebook_size] {
#endif

				Quantizer& lq = quantizers[thread_iter];
				uint_vec& cluster_indices = initial_codebook[thread_iter];

				uint_vec local_to_global(cluster_indices.size());

				for (uint32_t i = 0; i < cluster_indices.size(); i++)
				{
					const uint32_t global_training_vec_index = cluster_indices[i];
					local_to_global[i] = global_training_vec_index;

					lq.add_training_vec(q.get_training_vecs()[global_training_vec_index].first, q.get_training_vecs()[global_training_vec_index].second);
				}

				const uint32_t max_clusters = limit_clusterizers ? ((max_codebook_size + max_threads - 1) / max_threads) : (uint32_t)lq.get_total_training_vecs();

				success_flags[thread_iter] = lq.generate(max_clusters);

				if (success_flags[thread_iter])
				{
					lq.retrieve(local_clusters[thread_iter]);

					for (uint32_t i = 0; i < local_clusters[thread_iter].size(); i++)
					{
						for (uint32_t j = 0; j < local_clusters[thread_iter][i].size(); j++)
							local_clusters[thread_iter][i][j] = local_to_global[local_clusters[thread_iter][i][j]];
					}

					if (max_parent_codebook_size)
					{
						lq.retrieve((max_parent_codebook_size + max_threads - 1) / max_threads, local_parent_clusters[thread_iter]);

						for (uint32_t i = 0; i < local_parent_clusters[thread_iter].size(); i++)
						{
							for (uint32_t j = 0; j < local_parent_clusters[thread_iter][i].size(); j++)
								local_parent_clusters[thread_iter][i][j] = local_to_global[local_parent_clusters[thread_iter][i][j]];
						}
					}
				}

#ifndef __EMSCRIPTEN__
			} );
#endif

		} // thread_iter

#ifndef __EMSCRIPTEN__
		pJob_pool->wait_for_all();
#endif

		uint32_t total_clusters = 0, total_parent_clusters = 0;

		for (int thread_iter = 0; thread_iter < (int)max_threads; thread_iter++)
		{
			if (!success_flags[thread_iter])
				return false;
			total_clusters += (uint32_t)local_clusters[thread_iter].size();
			total_parent_clusters += (uint32_t)local_parent_clusters[thread_iter].size();
		}

		codebook.reserve(total_clusters);
		parent_codebook.reserve(total_parent_clusters);

		for (uint32_t thread_iter = 0; thread_iter < max_threads; thread_iter++)
		{
			for (uint32_t j = 0; j < local_clusters[thread_iter].size(); j++)
			{
				codebook.resize(codebook.size() + 1);
				codebook.back().swap(local_clusters[thread_iter][j]);
			}

			for (uint32_t j = 0; j < local_parent_clusters[thread_iter].size(); j++)
			{
				parent_codebook.resize(parent_codebook.size() + 1);
				parent_codebook.back().swap(local_parent_clusters[thread_iter][j]);
			}
		}

		return true;
	}

	template<typename Quantizer>
	bool generate_hierarchical_codebook_threaded(Quantizer& q,
		uint32_t max_codebook_size, uint32_t max_parent_codebook_size,
		basisu::vector<uint_vec>& codebook,
		basisu::vector<uint_vec>& parent_codebook,
		uint32_t max_threads, job_pool *pJob_pool,
		bool even_odd_input_pairs_equal)
	{
		typedef bit_hasher<typename Quantizer::training_vec_type> training_vec_bit_hasher;
		
		typedef std::unordered_map < typename Quantizer::training_vec_type, weighted_block_group, 
			training_vec_bit_hasher> group_hash;
		
		//interval_timer tm;
		//tm.start();

		group_hash unique_vecs;

		unique_vecs.reserve(20000);

		weighted_block_group g;
		
		if (even_odd_input_pairs_equal)
		{
			g.m_indices.resize(2);

			assert(q.get_training_vecs().size() >= 2 && (q.get_training_vecs().size() & 1) == 0);

			for (uint32_t i = 0; i < q.get_training_vecs().size(); i += 2)
			{
				assert(q.get_training_vecs()[i].first == q.get_training_vecs()[i + 1].first);

				g.m_total_weight = q.get_training_vecs()[i].second + q.get_training_vecs()[i + 1].second;
				g.m_indices[0] = i;
				g.m_indices[1] = i + 1;

				auto ins_res = unique_vecs.insert(std::make_pair(q.get_training_vecs()[i].first, g));

				if (!ins_res.second)
				{
					(ins_res.first)->second.m_total_weight += g.m_total_weight;
					(ins_res.first)->second.m_indices.push_back(i);
					(ins_res.first)->second.m_indices.push_back(i + 1);
				}
			}
		}
		else
		{
			g.m_indices.resize(1);

			for (uint32_t i = 0; i < q.get_training_vecs().size(); i++)
			{
				g.m_total_weight = q.get_training_vecs()[i].second;
				g.m_indices[0] = i;

				auto ins_res = unique_vecs.insert(std::make_pair(q.get_training_vecs()[i].first, g));

				if (!ins_res.second)
				{
					(ins_res.first)->second.m_total_weight += g.m_total_weight;
					(ins_res.first)->second.m_indices.push_back(i);
				}
			}
		}

		//debug_printf("generate_hierarchical_codebook_threaded: %u training vectors, %u unique training vectors, %3.3f secs\n", q.get_total_training_vecs(), (uint32_t)unique_vecs.size(), tm.get_elapsed_secs());
		debug_printf("generate_hierarchical_codebook_threaded: %u training vectors, %u unique training vectors\n", q.get_total_training_vecs(), (uint32_t)unique_vecs.size());

		Quantizer group_quant;
		typedef typename group_hash::const_iterator group_hash_const_iter;
		basisu::vector<group_hash_const_iter> unique_vec_iters;
		unique_vec_iters.reserve(unique_vecs.size());

		for (auto iter = unique_vecs.begin(); iter != unique_vecs.end(); ++iter)
		{
			group_quant.add_training_vec(iter->first, iter->second.m_total_weight);
			unique_vec_iters.push_back(iter);
		}

		bool limit_clusterizers = true;
		if (unique_vecs.size() <= max_codebook_size)
			limit_clusterizers = false;

		debug_printf("Limit clusterizers: %u\n", limit_clusterizers);

		basisu::vector<uint_vec> group_codebook, group_parent_codebook;
		bool status = generate_hierarchical_codebook_threaded_internal(group_quant,
			max_codebook_size, max_parent_codebook_size,
			group_codebook,
			group_parent_codebook,
			(unique_vecs.size() < 65536*4) ? 1 : max_threads, limit_clusterizers, pJob_pool);

		if (!status)
			return false;

		codebook.resize(0);
		for (uint32_t i = 0; i < group_codebook.size(); i++)
		{
			codebook.resize(codebook.size() + 1);

			for (uint32_t j = 0; j < group_codebook[i].size(); j++)
			{
				const uint32_t group_index = group_codebook[i][j];

				typename group_hash::const_iterator group_iter = unique_vec_iters[group_index];
				const uint_vec& training_vec_indices = group_iter->second.m_indices;
				
				append_vector(codebook.back(), training_vec_indices);
			}
		}

		parent_codebook.resize(0);
		for (uint32_t i = 0; i < group_parent_codebook.size(); i++)
		{
			parent_codebook.resize(parent_codebook.size() + 1);

			for (uint32_t j = 0; j < group_parent_codebook[i].size(); j++)
			{
				const uint32_t group_index = group_parent_codebook[i][j];

				typename group_hash::const_iterator group_iter = unique_vec_iters[group_index];
				const uint_vec& training_vec_indices = group_iter->second.m_indices;

				append_vector(parent_codebook.back(), training_vec_indices);
			}
		}

		return true;
	}

	// Canonical Huffman coding

	class histogram
	{
		basisu::vector<uint32_t> m_hist;

	public:
		histogram(uint32_t size = 0) { init(size); }

		void clear()
		{
			clear_vector(m_hist);
		}

		void init(uint32_t size)
		{
			m_hist.resize(0);
			m_hist.resize(size);
		}

		inline uint32_t size() const { return static_cast<uint32_t>(m_hist.size()); }

		inline const uint32_t &operator[] (uint32_t index) const
		{
			return m_hist[index];
		}

		inline uint32_t &operator[] (uint32_t index)
		{
			return m_hist[index];
		}

		inline void inc(uint32_t index)
		{
			m_hist[index]++;
		}

		uint64_t get_total() const
		{
			uint64_t total = 0;
			for (uint32_t i = 0; i < m_hist.size(); ++i)
				total += m_hist[i];
			return total;
		}

		double get_entropy() const
		{
			double total = static_cast<double>(get_total());
			if (total == 0.0f)
				return 0.0f;

			const double inv_total = 1.0f / total;
			const double neg_inv_log2 = -1.0f / log(2.0f);
			
			double e = 0.0f;
			for (uint32_t i = 0; i < m_hist.size(); i++)
				if (m_hist[i])
					e += log(m_hist[i] * inv_total) * neg_inv_log2 * static_cast<double>(m_hist[i]);

			return e;
		}
	};
		
	struct sym_freq
	{
		uint32_t m_key;
		uint16_t m_sym_index;
	};

	sym_freq *canonical_huffman_radix_sort_syms(uint32_t num_syms, sym_freq *pSyms0, sym_freq *pSyms1);
	void canonical_huffman_calculate_minimum_redundancy(sym_freq *A, int num_syms);
	void canonical_huffman_enforce_max_code_size(int *pNum_codes, int code_list_len, int max_code_size);
	
	class huffman_encoding_table
	{
	public:
		huffman_encoding_table()
		{
		}

		void clear()
		{
			clear_vector(m_codes);
			clear_vector(m_code_sizes);
		}

		bool init(const histogram &h, uint32_t max_code_size = cHuffmanMaxSupportedCodeSize)
		{
			return init(h.size(), &h[0], max_code_size);
		}

		bool init(uint32_t num_syms, const uint16_t *pFreq, uint32_t max_code_size);
		bool init(uint32_t num_syms, const uint32_t *pSym_freq, uint32_t max_code_size);
		
		inline const uint16_vec &get_codes() const { return m_codes; }
		inline const uint8_vec &get_code_sizes() const { return m_code_sizes; }

		uint32_t get_total_used_codes() const
		{
			for (int i = static_cast<int>(m_code_sizes.size()) - 1; i >= 0; i--)
				if (m_code_sizes[i])
					return i + 1;
			return 0;
		}

	private:
		uint16_vec m_codes;
		uint8_vec m_code_sizes;
	};

	class bitwise_coder
	{
	public:
		bitwise_coder() :
			m_bit_buffer(0),
			m_bit_buffer_size(0),
			m_total_bits(0)
		{
		}

		inline void clear()
		{
			clear_vector(m_bytes);
			m_bit_buffer = 0;
			m_bit_buffer_size = 0;
			m_total_bits = 0;
		}

		inline void restart()
		{
			m_bytes.resize(0);
			m_bit_buffer = 0;
			m_bit_buffer_size = 0;
			m_total_bits = 0;
		}

		inline const uint8_vec &get_bytes() const { return m_bytes; }

		inline uint64_t get_total_bits() const { return m_total_bits; }
		inline void clear_total_bits() { m_total_bits = 0; }

		inline void init(uint32_t reserve_size = 1024)
		{
			m_bytes.reserve(reserve_size);
			m_bytes.resize(0);

			m_bit_buffer = 0;
			m_bit_buffer_size = 0;
			m_total_bits = 0;
		}

		inline uint32_t flush()
		{
			if (m_bit_buffer_size)
			{
				m_total_bits += 8 - (m_bit_buffer_size & 7);
				append_byte(static_cast<uint8_t>(m_bit_buffer));

				m_bit_buffer = 0;
				m_bit_buffer_size = 0;
				
				return 8;
			}

			return 0;
		}

		inline uint32_t put_bits(uint32_t bits, uint32_t num_bits)
		{
			assert(num_bits <= 32);
			assert(bits < (1ULL << num_bits));

			if (!num_bits)
				return 0;

			m_total_bits += num_bits;

			uint64_t v = (static_cast<uint64_t>(bits) << m_bit_buffer_size) | m_bit_buffer;
			m_bit_buffer_size += num_bits;

			while (m_bit_buffer_size >= 8)
			{
				append_byte(static_cast<uint8_t>(v));
				v >>= 8;
				m_bit_buffer_size -= 8;
			}

			m_bit_buffer = static_cast<uint8_t>(v);
			return num_bits;
		}

		inline uint32_t put_code(uint32_t sym, const huffman_encoding_table &tab)
		{
			uint32_t code = tab.get_codes()[sym];
			uint32_t code_size = tab.get_code_sizes()[sym];
			assert(code_size >= 1);
			put_bits(code, code_size);
			return code_size;
		}

		inline uint32_t put_truncated_binary(uint32_t v, uint32_t n)
		{
			assert((n >= 2) && (v < n));

			uint32_t k = floor_log2i(n);
			uint32_t u = (1 << (k + 1)) - n;

			if (v < u)
				return put_bits(v, k);
			
			uint32_t x = v + u;
			assert((x >> 1) >= u);

			put_bits(x >> 1, k);
			put_bits(x & 1, 1);
			return k + 1;
		}

		inline uint32_t put_rice(uint32_t v, uint32_t m)
		{
			assert(m);
			
			const uint64_t start_bits = m_total_bits;

			uint32_t q = v >> m, r = v & ((1 << m) - 1);

			// rice coding sanity check
			assert(q <= 64);
			
			for (; q > 16; q -= 16)
				put_bits(0xFFFF, 16);

			put_bits((1 << q) - 1, q);
			put_bits(r << 1, m + 1);
			
			return (uint32_t)(m_total_bits - start_bits);
		}

		inline uint32_t put_vlc(uint32_t v, uint32_t chunk_bits)
		{
			assert(chunk_bits);

			const uint32_t chunk_size = 1 << chunk_bits;
			const uint32_t chunk_mask = chunk_size - 1;
					
			uint32_t total_bits = 0;

			for ( ; ; )
			{
				uint32_t next_v = v >> chunk_bits;
								
				total_bits += put_bits((v & chunk_mask) | (next_v ? chunk_size : 0), chunk_bits + 1);
				if (!next_v)
					break;

				v = next_v;
			}

			return total_bits;
		}

		uint32_t emit_huffman_table(const huffman_encoding_table &tab);
		
	private:
		uint8_vec m_bytes;
		uint32_t m_bit_buffer, m_bit_buffer_size;
		uint64_t m_total_bits;

		void append_byte(uint8_t c)
		{
			m_bytes.resize(m_bytes.size() + 1);
			m_bytes.back() = c;
		}

		static void end_nonzero_run(uint16_vec &syms, uint32_t &run_size, uint32_t len);
		static void end_zero_run(uint16_vec &syms, uint32_t &run_size);
	};

	class huff2D
	{
	public:
		huff2D() { }
		huff2D(uint32_t bits_per_sym, uint32_t total_syms_per_group) { init(bits_per_sym, total_syms_per_group); }

		inline const histogram &get_histogram() const { return m_histogram; }
		inline const huffman_encoding_table &get_encoding_table() const { return m_encoding_table; }

		inline void init(uint32_t bits_per_sym, uint32_t total_syms_per_group)
		{
			assert((bits_per_sym * total_syms_per_group) <= 16 && total_syms_per_group >= 1 && bits_per_sym >= 1);
						
			m_bits_per_sym = bits_per_sym;
			m_total_syms_per_group = total_syms_per_group;
			m_cur_sym_bits = 0;
			m_cur_num_syms = 0;
			m_decode_syms_remaining = 0;
			m_next_decoder_group_index = 0;

			m_histogram.init(1 << (bits_per_sym * total_syms_per_group));
		}

		inline void clear()
		{
			m_group_bits.clear();

			m_cur_sym_bits = 0;
			m_cur_num_syms = 0;
			m_decode_syms_remaining = 0;
			m_next_decoder_group_index = 0;
		}

		inline void emit(uint32_t sym)
		{
			m_cur_sym_bits |= (sym << (m_cur_num_syms * m_bits_per_sym));
			m_cur_num_syms++;

			if (m_cur_num_syms == m_total_syms_per_group)
				flush();
		}

		inline void flush()
		{
			if (m_cur_num_syms)
			{
				m_group_bits.push_back(m_cur_sym_bits);
				m_histogram.inc(m_cur_sym_bits);

				m_cur_sym_bits = 0;
				m_cur_num_syms = 0;
			}
		}

		inline bool start_encoding(uint32_t code_size_limit = 16)
		{
			flush();

			if (!m_encoding_table.init(m_histogram, code_size_limit))
				return false;

			m_decode_syms_remaining = 0;
			m_next_decoder_group_index = 0;

			return true;
		}
				
		inline uint32_t emit_next_sym(bitwise_coder &c)
		{
			uint32_t bits = 0;

			if (!m_decode_syms_remaining)
			{
				bits = c.put_code(m_group_bits[m_next_decoder_group_index++], m_encoding_table);
				m_decode_syms_remaining = m_total_syms_per_group;
			}

			m_decode_syms_remaining--;
			return bits;
		}

		inline void emit_flush()
		{
			m_decode_syms_remaining = 0;
		}

	private:
		uint_vec m_group_bits;
		huffman_encoding_table m_encoding_table;
		histogram m_histogram;
		uint32_t m_bits_per_sym, m_total_syms_per_group, m_cur_sym_bits, m_cur_num_syms, m_next_decoder_group_index, m_decode_syms_remaining;
	};

	bool huffman_test(int rand_seed);

	// VQ index reordering
	
	class palette_index_reorderer
	{
	public:
		palette_index_reorderer()
		{
		}

		void clear()
		{
			clear_vector(m_hist);
			clear_vector(m_total_count_to_picked);
			clear_vector(m_entries_picked);
			clear_vector(m_entries_to_do);
			clear_vector(m_remap_table);
		}

		// returns [0,1] distance of entry i to entry j
		typedef float(*pEntry_dist_func)(uint32_t i, uint32_t j, void *pCtx);

		void init(uint32_t num_indices, const uint32_t *pIndices, uint32_t num_syms, pEntry_dist_func pDist_func, void *pCtx, float dist_func_weight);
		
		// Table remaps old to new symbol indices
		inline const uint_vec &get_remap_table() const { return m_remap_table; }

	private:
		uint_vec m_hist, m_total_count_to_picked, m_entries_picked, m_entries_to_do, m_remap_table;

		inline uint32_t get_hist(int i, int j, int n) const { return (i > j) ? m_hist[j * n + i] : m_hist[i * n + j]; }
		inline void inc_hist(int i, int j, int n) { if ((i != j) && (i < j) && (i != -1) && (j != -1)) { assert(((uint32_t)i < (uint32_t)n) && ((uint32_t)j < (uint32_t)n)); m_hist[i * n + j]++; } }

		void prepare_hist(uint32_t num_syms, uint32_t num_indices, const uint32_t *pIndices);
		void find_initial(uint32_t num_syms);
		void find_next_entry(uint32_t &best_entry, double &best_count, pEntry_dist_func pDist_func, void *pCtx, float dist_func_weight);
		float pick_side(uint32_t num_syms, uint32_t entry_to_move, pEntry_dist_func pDist_func, void *pCtx, float dist_func_weight);
	};

	// Simple 32-bit 2D image class

	class image
	{
	public:
		image() : 
			m_width(0), m_height(0), m_pitch(0)
		{
		}

		image(uint32_t w, uint32_t h, uint32_t p = UINT32_MAX) : 
			m_width(0), m_height(0), m_pitch(0)
		{
			resize(w, h, p);
		}

		image(const uint8_t *pImage, uint32_t width, uint32_t height, uint32_t comps) :
			m_width(0), m_height(0), m_pitch(0)
		{
			init(pImage, width, height, comps);
		}

		image(const image &other) :
			m_width(0), m_height(0), m_pitch(0)
		{
			*this = other;
		}

		image &swap(image &other)
		{
			std::swap(m_width, other.m_width);
			std::swap(m_height, other.m_height);
			std::swap(m_pitch, other.m_pitch);
			m_pixels.swap(other.m_pixels);
			return *this;
		}

		image &operator= (const image &rhs)
		{
			if (this != &rhs)
			{
				m_width = rhs.m_width;
				m_height = rhs.m_height;
				m_pitch = rhs.m_pitch;
				m_pixels = rhs.m_pixels;
			}
			return *this;
		}

		image &clear()
		{
			m_width = 0; 
			m_height = 0;
			m_pitch = 0;
			clear_vector(m_pixels);
			return *this;
		}

		image &resize(uint32_t w, uint32_t h, uint32_t p = UINT32_MAX, const color_rgba& background = g_black_color)
		{
			return crop(w, h, p, background);
		}

		image &set_all(const color_rgba &c)
		{
			for (uint32_t i = 0; i < m_pixels.size(); i++)
				m_pixels[i] = c;
			return *this;
		}

		void init(const uint8_t *pImage, uint32_t width, uint32_t height, uint32_t comps)
		{
			assert(comps >= 1 && comps <= 4);
			
			resize(width, height);

			for (uint32_t y = 0; y < height; y++)
			{
				for (uint32_t x = 0; x < width; x++)
				{
					const uint8_t *pSrc = &pImage[(x + y * width) * comps];
					color_rgba &dst = (*this)(x, y);

					if (comps == 1)
					{
						dst.r = pSrc[0];
						dst.g = pSrc[0];
						dst.b = pSrc[0];
						dst.a = 255;
					}
					else if (comps == 2)
					{
						dst.r = pSrc[0];
						dst.g = pSrc[0];
						dst.b = pSrc[0];
						dst.a = pSrc[1];
					}
					else
					{
						dst.r = pSrc[0];
						dst.g = pSrc[1];
						dst.b = pSrc[2];
						if (comps == 4)
							dst.a = pSrc[3];
						else
							dst.a = 255;
					}
				}
			}
		}

		image &fill_box(uint32_t x, uint32_t y, uint32_t w, uint32_t h, const color_rgba &c)
		{
			for (uint32_t iy = 0; iy < h; iy++)
				for (uint32_t ix = 0; ix < w; ix++)
					set_clipped(x + ix, y + iy, c);
			return *this;
		}

		image& fill_box_alpha(uint32_t x, uint32_t y, uint32_t w, uint32_t h, const color_rgba& c)
		{
			for (uint32_t iy = 0; iy < h; iy++)
				for (uint32_t ix = 0; ix < w; ix++)
					set_clipped_alpha(x + ix, y + iy, c);
			return *this;
		}

		image &crop_dup_borders(uint32_t w, uint32_t h)
		{
			const uint32_t orig_w = m_width, orig_h = m_height;

			crop(w, h);

			if (orig_w && orig_h)
			{
				if (m_width > orig_w)
				{
					for (uint32_t x = orig_w; x < m_width; x++)
						for (uint32_t y = 0; y < m_height; y++)
							set_clipped(x, y, get_clamped(minimum(x, orig_w - 1U), minimum(y, orig_h - 1U)));
				}

				if (m_height > orig_h)
				{
					for (uint32_t y = orig_h; y < m_height; y++)
						for (uint32_t x = 0; x < m_width; x++)
							set_clipped(x, y, get_clamped(minimum(x, orig_w - 1U), minimum(y, orig_h - 1U)));
				}
			}
			return *this;
		}

		// pPixels MUST have been allocated using malloc() (basisu::vector will eventually use free() on the pointer).
		image& grant_ownership(color_rgba* pPixels, uint32_t w, uint32_t h, uint32_t p = UINT32_MAX)
		{
			if (p == UINT32_MAX)
				p = w;

			clear();
			
			if ((!p) || (!w) || (!h))
				return *this;

			m_pixels.grant_ownership(pPixels, p * h, p * h);

			m_width = w;
			m_height = h;
			m_pitch = p;

			return *this;
		}

		image &crop(uint32_t w, uint32_t h, uint32_t p = UINT32_MAX, const color_rgba &background = g_black_color, bool init_image = true)
		{
			if (p == UINT32_MAX)
				p = w;

			if ((w == m_width) && (m_height == h) && (m_pitch == p))
				return *this;

			if ((!w) || (!h) || (!p))
			{
				clear();
				return *this;
			}

			color_rgba_vec cur_state;
			cur_state.swap(m_pixels);

			m_pixels.resize(p * h);

			if (init_image)
			{
				if (m_width || m_height)
				{
					for (uint32_t y = 0; y < h; y++)
					{
						for (uint32_t x = 0; x < w; x++)
						{
							if ((x < m_width) && (y < m_height))
								m_pixels[x + y * p] = cur_state[x + y * m_pitch];
							else
								m_pixels[x + y * p] = background;
						}
					}
				}
				else
				{
					m_pixels.set_all(background);
				}
			}

			m_width = w;
			m_height = h;
			m_pitch = p;

			return *this;
		}

		inline const color_rgba &operator() (uint32_t x, uint32_t y) const { assert(x < m_width && y < m_height); return m_pixels[x + y * m_pitch]; }
		inline color_rgba &operator() (uint32_t x, uint32_t y) { assert(x < m_width && y < m_height); return m_pixels[x + y * m_pitch]; }

		inline const color_rgba &get_clamped(int x, int y) const { return (*this)(clamp<int>(x, 0, m_width - 1), clamp<int>(y, 0, m_height - 1)); }
		inline color_rgba &get_clamped(int x, int y) { return (*this)(clamp<int>(x, 0, m_width - 1), clamp<int>(y, 0, m_height - 1)); }

		inline const color_rgba &get_clamped_or_wrapped(int x, int y, bool wrap_u, bool wrap_v) const
		{
			x = wrap_u ? posmod(x, m_width) : clamp<int>(x, 0, m_width - 1);
			y = wrap_v ? posmod(y, m_height) : clamp<int>(y, 0, m_height - 1);
			return m_pixels[x + y * m_pitch];
		}

		inline color_rgba &get_clamped_or_wrapped(int x, int y, bool wrap_u, bool wrap_v)
		{
			x = wrap_u ? posmod(x, m_width) : clamp<int>(x, 0, m_width - 1);
			y = wrap_v ? posmod(y, m_height) : clamp<int>(y, 0, m_height - 1);
			return m_pixels[x + y * m_pitch];
		}
		
		inline image &set_clipped(int x, int y, const color_rgba &c) 
		{
			if ((static_cast<uint32_t>(x) < m_width) && (static_cast<uint32_t>(y) < m_height))
				(*this)(x, y) = c;
			return *this;
		}

		inline image& set_clipped_alpha(int x, int y, const color_rgba& c)
		{
			if ((static_cast<uint32_t>(x) < m_width) && (static_cast<uint32_t>(y) < m_height))
				(*this)(x, y).m_comps[3] = c.m_comps[3];
			return *this;
		}

		// Very straightforward blit with full clipping. Not fast, but it works.
		image &blit(const image &src, int src_x, int src_y, int src_w, int src_h, int dst_x, int dst_y)
		{
			for (int y = 0; y < src_h; y++)
			{
				const int sy = src_y + y;
				if (sy < 0)
					continue;
				else if (sy >= (int)src.get_height())
					break;

				for (int x = 0; x < src_w; x++)
				{
					const int sx = src_x + x;
					if (sx < 0)
						continue;
					else if (sx >= (int)src.get_height())
						break;

					set_clipped(dst_x + x, dst_y + y, src(sx, sy));
				}
			}

			return *this;
		}

		const image &extract_block_clamped(color_rgba *pDst, uint32_t src_x, uint32_t src_y, uint32_t w, uint32_t h) const
		{
			if (((src_x + w) > m_width) || ((src_y + h) > m_height))
			{
				// Slower clamping case
				for (uint32_t y = 0; y < h; y++)
					for (uint32_t x = 0; x < w; x++)
						*pDst++ = get_clamped(src_x + x, src_y + y);
			}
			else
			{
				const color_rgba* pSrc = &m_pixels[src_x + src_y * m_pitch];

				for (uint32_t y = 0; y < h; y++)
				{
					memcpy(pDst, pSrc, w * sizeof(color_rgba));
					pSrc += m_pitch;
					pDst += w;
				}
			}

			return *this;
		}

		image &set_block_clipped(const color_rgba *pSrc, uint32_t dst_x, uint32_t dst_y, uint32_t w, uint32_t h)
		{
			for (uint32_t y = 0; y < h; y++)
				for (uint32_t x = 0; x < w; x++)
					set_clipped(dst_x + x, dst_y + y, *pSrc++);
			return *this;
		}

		inline uint32_t get_width() const { return m_width; }
		inline uint32_t get_height() const { return m_height; }
		inline uint32_t get_pitch() const { return m_pitch; }
		inline uint32_t get_total_pixels() const { return m_width * m_height; }

		inline uint32_t get_block_width(uint32_t w) const { return (m_width + (w - 1)) / w; }
		inline uint32_t get_block_height(uint32_t h) const { return (m_height + (h - 1)) / h; }
		inline uint32_t get_total_blocks(uint32_t w, uint32_t h) const { return get_block_width(w) * get_block_height(h); }

		inline const color_rgba_vec &get_pixels() const { return m_pixels; }
		inline color_rgba_vec &get_pixels() { return m_pixels; }

		inline const color_rgba *get_ptr() const { return &m_pixels[0]; }
		inline color_rgba *get_ptr() { return &m_pixels[0]; }

		bool has_alpha(uint32_t channel = 3) const
		{
			for (uint32_t y = 0; y < m_height; ++y)
				for (uint32_t x = 0; x < m_width; ++x)
					if ((*this)(x, y)[channel] < 255)
						return true;

			return false;
		}

		image &set_alpha(uint8_t a)
		{
			for (uint32_t y = 0; y < m_height; ++y)
				for (uint32_t x = 0; x < m_width; ++x)
					(*this)(x, y).a = a;
			return *this;
		}

		image &flip_y()
		{
			for (uint32_t y = 0; y < m_height / 2; ++y)
				for (uint32_t x = 0; x < m_width; ++x)
					std::swap((*this)(x, y), (*this)(x, m_height - 1 - y));
			return *this;
		}

		// TODO: There are many ways to do this, not sure this is the best way.
		image &renormalize_normal_map()
		{
			for (uint32_t y = 0; y < m_height; y++)
			{
				for (uint32_t x = 0; x < m_width; x++)
				{
					color_rgba &c = (*this)(x, y);
					if ((c.r == 128) && (c.g == 128) && (c.b == 128))
						continue;

					vec3F v(c.r, c.g, c.b);
					v = (v * (2.0f / 255.0f)) - vec3F(1.0f);
					v.clamp(-1.0f, 1.0f);

					float length = v.length();
					const float cValidThresh = .077f;
					if (length < cValidThresh)
					{
						c.set(128, 128, 128, c.a);
					}
					else if (fabs(length - 1.0f) > cValidThresh)
					{
						if (length)
							v /= length;

						for (uint32_t i = 0; i < 3; i++)
							c[i] = static_cast<uint8_t>(clamp<float>(floor((v[i] + 1.0f) * 255.0f * .5f + .5f), 0.0f, 255.0f));

						if ((c.g == 128) && (c.r == 128))
						{
							if (c.b < 128)
								c.b = 0;
							else
								c.b = 255;
						}
					}
				}
			}
			return *this;
		}

		void debug_text(uint32_t x_ofs, uint32_t y_ofs, uint32_t x_scale, uint32_t y_scale, const color_rgba &fg, const color_rgba *pBG, bool alpha_only, const char* p, ...);
				
	private:
		uint32_t m_width, m_height, m_pitch;  // all in pixels
		color_rgba_vec m_pixels;
	};

	// Float images

	typedef basisu::vector<vec4F> vec4F_vec;

	class imagef
	{
	public:
		imagef() : 
			m_width(0), m_height(0), m_pitch(0)
		{
		}

		imagef(uint32_t w, uint32_t h, uint32_t p = UINT32_MAX) : 
			m_width(0), m_height(0), m_pitch(0)
		{
			resize(w, h, p);
		}

		imagef(const imagef &other) :
			m_width(0), m_height(0), m_pitch(0)
		{
			*this = other;
		}

		imagef &swap(imagef &other)
		{
			std::swap(m_width, other.m_width);
			std::swap(m_height, other.m_height);
			std::swap(m_pitch, other.m_pitch);
			m_pixels.swap(other.m_pixels);
			return *this;
		}

		imagef &operator= (const imagef &rhs)
		{
			if (this != &rhs)
			{
				m_width = rhs.m_width;
				m_height = rhs.m_height;
				m_pitch = rhs.m_pitch;
				m_pixels = rhs.m_pixels;
			}
			return *this;
		}

		imagef &clear()
		{
			m_width = 0; 
			m_height = 0;
			m_pitch = 0;
			clear_vector(m_pixels);
			return *this;
		}

		imagef &set(const image &src, const vec4F &scale = vec4F(1), const vec4F &bias = vec4F(0))
		{
			const uint32_t width = src.get_width();
			const uint32_t height = src.get_height();

			resize(width, height);

			for (int y = 0; y < (int)height; y++)
			{
				for (uint32_t x = 0; x < width; x++)
				{
					const color_rgba &src_pixel = src(x, y);
					(*this)(x, y).set((float)src_pixel.r * scale[0] + bias[0], (float)src_pixel.g * scale[1] + bias[1], (float)src_pixel.b * scale[2] + bias[2], (float)src_pixel.a * scale[3] + bias[3]);
				}
			}

			return *this;
		}

		imagef &resize(const imagef &other, uint32_t p = UINT32_MAX, const vec4F& background = vec4F(0,0,0,1))
		{
			return resize(other.get_width(), other.get_height(), p, background);
		}

		imagef &resize(uint32_t w, uint32_t h, uint32_t p = UINT32_MAX, const vec4F& background = vec4F(0,0,0,1))
		{
			return crop(w, h, p, background);
		}

		imagef &set_all(const vec4F &c)
		{
			for (uint32_t i = 0; i < m_pixels.size(); i++)
				m_pixels[i] = c;
			return *this;
		}

		imagef &fill_box(uint32_t x, uint32_t y, uint32_t w, uint32_t h, const vec4F &c)
		{
			for (uint32_t iy = 0; iy < h; iy++)
				for (uint32_t ix = 0; ix < w; ix++)
					set_clipped(x + ix, y + iy, c);
			return *this;
		}
				
		imagef &crop(uint32_t w, uint32_t h, uint32_t p = UINT32_MAX, const vec4F &background = vec4F(0,0,0,1))
		{
			if (p == UINT32_MAX)
				p = w;

			if ((w == m_width) && (m_height == h) && (m_pitch == p))
				return *this;

			if ((!w) || (!h) || (!p))
			{
				clear();
				return *this;
			}

			vec4F_vec cur_state;
			cur_state.swap(m_pixels);

			m_pixels.resize(p * h);
			
			for (uint32_t y = 0; y < h; y++)
			{
				for (uint32_t x = 0; x < w; x++)
				{
					if ((x < m_width) && (y < m_height))
						m_pixels[x + y * p] = cur_state[x + y * m_pitch];
					else
						m_pixels[x + y * p] = background;
				}
			}

			m_width = w;
			m_height = h;
			m_pitch = p;

			return *this;
		}

		imagef& crop_dup_borders(uint32_t w, uint32_t h)
		{
			const uint32_t orig_w = m_width, orig_h = m_height;

			crop(w, h);

			if (orig_w && orig_h)
			{
				if (m_width > orig_w)
				{
					for (uint32_t x = orig_w; x < m_width; x++)
						for (uint32_t y = 0; y < m_height; y++)
							set_clipped(x, y, get_clamped(minimum(x, orig_w - 1U), minimum(y, orig_h - 1U)));
				}

				if (m_height > orig_h)
				{
					for (uint32_t y = orig_h; y < m_height; y++)
						for (uint32_t x = 0; x < m_width; x++)
							set_clipped(x, y, get_clamped(minimum(x, orig_w - 1U), minimum(y, orig_h - 1U)));
				}
			}
			return *this;
		}

		inline const vec4F &operator() (uint32_t x, uint32_t y) const { assert(x < m_width && y < m_height); return m_pixels[x + y * m_pitch]; }
		inline vec4F &operator() (uint32_t x, uint32_t y) { assert(x < m_width && y < m_height); return m_pixels[x + y * m_pitch]; }

		inline const vec4F &get_clamped(int x, int y) const { return (*this)(clamp<int>(x, 0, m_width - 1), clamp<int>(y, 0, m_height - 1)); }
		inline vec4F &get_clamped(int x, int y) { return (*this)(clamp<int>(x, 0, m_width - 1), clamp<int>(y, 0, m_height - 1)); }

		inline const vec4F &get_clamped_or_wrapped(int x, int y, bool wrap_u, bool wrap_v) const
		{
			x = wrap_u ? posmod(x, m_width) : clamp<int>(x, 0, m_width - 1);
			y = wrap_v ? posmod(y, m_height) : clamp<int>(y, 0, m_height - 1);
			return m_pixels[x + y * m_pitch];
		}

		inline vec4F &get_clamped_or_wrapped(int x, int y, bool wrap_u, bool wrap_v)
		{
			x = wrap_u ? posmod(x, m_width) : clamp<int>(x, 0, m_width - 1);
			y = wrap_v ? posmod(y, m_height) : clamp<int>(y, 0, m_height - 1);
			return m_pixels[x + y * m_pitch];
		}
		
		inline imagef &set_clipped(int x, int y, const vec4F &c) 
		{
			if ((static_cast<uint32_t>(x) < m_width) && (static_cast<uint32_t>(y) < m_height))
				(*this)(x, y) = c;
			return *this;
		}

		// Very straightforward blit with full clipping. Not fast, but it works.
		imagef &blit(const imagef &src, int src_x, int src_y, int src_w, int src_h, int dst_x, int dst_y)
		{
			for (int y = 0; y < src_h; y++)
			{
				const int sy = src_y + y;
				if (sy < 0)
					continue;
				else if (sy >= (int)src.get_height())
					break;

				for (int x = 0; x < src_w; x++)
				{
					const int sx = src_x + x;
					if (sx < 0)
						continue;
					else if (sx >= (int)src.get_height())
						break;

					set_clipped(dst_x + x, dst_y + y, src(sx, sy));
				}
			}

			return *this;
		}

		const imagef &extract_block_clamped(vec4F *pDst, uint32_t src_x, uint32_t src_y, uint32_t w, uint32_t h) const
		{
			for (uint32_t y = 0; y < h; y++)
				for (uint32_t x = 0; x < w; x++)
					*pDst++ = get_clamped(src_x + x, src_y + y);
			return *this;
		}

		imagef &set_block_clipped(const vec4F *pSrc, uint32_t dst_x, uint32_t dst_y, uint32_t w, uint32_t h)
		{
			for (uint32_t y = 0; y < h; y++)
				for (uint32_t x = 0; x < w; x++)
					set_clipped(dst_x + x, dst_y + y, *pSrc++);
			return *this;
		}

		inline uint32_t get_width() const { return m_width; }
		inline uint32_t get_height() const { return m_height; }
		inline uint32_t get_pitch() const { return m_pitch; }
		inline uint32_t get_total_pixels() const { return m_width * m_height; }

		inline uint32_t get_block_width(uint32_t w) const { return (m_width + (w - 1)) / w; }
		inline uint32_t get_block_height(uint32_t h) const { return (m_height + (h - 1)) / h; }
		inline uint32_t get_total_blocks(uint32_t w, uint32_t h) const { return get_block_width(w) * get_block_height(h); }

		inline const vec4F_vec &get_pixels() const { return m_pixels; }
		inline vec4F_vec &get_pixels() { return m_pixels; }

		inline const vec4F *get_ptr() const { return &m_pixels[0]; }
		inline vec4F *get_ptr() { return &m_pixels[0]; }

		bool clean_astc_hdr_pixels(float highest_mag)
		{
			bool status = true;
			bool nan_msg = false;
			bool inf_msg = false;
			bool neg_zero_msg = false;
			bool neg_msg = false;
			bool clamp_msg = false;

			for (uint32_t iy = 0; iy < m_height; iy++)
			{
				for (uint32_t ix = 0; ix < m_width; ix++)
				{
					vec4F& c = (*this)(ix, iy);

					for (uint32_t s = 0; s < 4; s++)
					{
						float &p = c[s];
						union { float f; uint32_t u; } x; x.f = p;
						
						if ((std::isnan(p)) || (std::isinf(p)) || (x.u == 0x80000000))
						{
							if (std::isnan(p))
							{
								if (!nan_msg)
								{
									fprintf(stderr, "One or more pixels was NaN, setting to 0.\n");
									nan_msg = true;
								}
							}

							if (std::isinf(p))
							{
								if (!inf_msg)
								{
									fprintf(stderr, "One or more pixels was INF, setting to 0.\n");
									inf_msg = true;
								}
							}

							if (x.u == 0x80000000)
							{
								if (!neg_zero_msg)
								{
									fprintf(stderr, "One or more pixels was -0, setting them to 0.\n");
									neg_zero_msg = true;
								}
							}

							p = 0.0f;
							status = false;
						}
						else
						{
							//const float o = p;
							if (p < 0.0f)
							{
								p = 0.0f;

								if (!neg_msg)
								{
									fprintf(stderr, "One or more pixels was negative -- setting these pixel components to 0 because ASTC HDR doesn't support signed values.\n");
									neg_msg = true;
								}
								
								status = false;
							}

							if (p > highest_mag)
							{
								p = highest_mag;
								
								if (!clamp_msg)
								{
									fprintf(stderr, "One or more pixels had to be clamped to %f.\n", highest_mag);
									clamp_msg = true;
								}

								status = false;
							}
						}
					}
				}
			}

			return status;
		}

		imagef& flip_y()
		{
			for (uint32_t y = 0; y < m_height / 2; ++y)
				for (uint32_t x = 0; x < m_width; ++x)
					std::swap((*this)(x, y), (*this)(x, m_height - 1 - y));

			return *this;
		}
						
	private:
		uint32_t m_width, m_height, m_pitch;  // all in pixels
		vec4F_vec m_pixels;
	};

	// REC 709 coefficients
	const float REC_709_R = 0.212656f, REC_709_G = 0.715158f, REC_709_B = 0.072186f;

	inline float get_luminance(const vec4F &c)
	{
		return c[0] * REC_709_R + c[1] * REC_709_G + c[2] * REC_709_B;
	}

	float linear_to_srgb(float l);
	float srgb_to_linear(float s);

	// Image metrics
		
	class image_metrics
	{
	public:
		// TODO: Add ssim
		double m_max, m_mean, m_mean_squared, m_rms, m_psnr, m_ssim;
		bool m_has_neg, m_hf_mag_overflow, m_any_abnormal;

		image_metrics()
		{
			clear();
		}

		void clear()
		{
			m_max = 0;
			m_mean = 0;
			m_mean_squared = 0;
			m_rms = 0;
			m_psnr = 0;
			m_ssim = 0;
			m_has_neg = false;
			m_hf_mag_overflow = false;
			m_any_abnormal = false;
		}

		void print(const char *pPrefix = nullptr)	{ printf("%sMax: %3.3f Mean: %3.3f RMS: %3.3f PSNR: %2.3f dB\n", pPrefix ? pPrefix : "", m_max, m_mean, m_rms, m_psnr);	}
		void print_hp(const char* pPrefix = nullptr) { printf("%sMax: %3.6f Mean: %3.6f RMS: %3.6f PSNR: %2.6f dB, Any Neg: %u, Half float overflow: %u, Any NaN/Inf: %u\n", pPrefix ? pPrefix : "", m_max, m_mean, m_rms, m_psnr, m_has_neg, m_hf_mag_overflow, m_any_abnormal); }

		void calc(const imagef& a, const imagef& b, uint32_t first_chan = 0, uint32_t total_chans = 0, bool avg_comp_error = true, bool log = false);
		void calc_half(const imagef& a, const imagef& b, uint32_t first_chan, uint32_t total_chans, bool avg_comp_error);
		void calc_half2(const imagef& a, const imagef& b, uint32_t first_chan, uint32_t total_chans, bool avg_comp_error);
		void calc(const image &a, const image &b, uint32_t first_chan = 0, uint32_t total_chans = 0, bool avg_comp_error = true, bool use_601_luma = false);
	};

	// Image saving/loading/resampling

	bool load_png(const uint8_t* pBuf, size_t buf_size, image& img, const char* pFilename = nullptr);
	bool load_png(const char* pFilename, image& img);
	inline bool load_png(const std::string &filename, image &img) { return load_png(filename.c_str(), img); }

	bool load_tga(const char* pFilename, image& img);
	inline bool load_tga(const std::string &filename, image &img) { return load_tga(filename.c_str(), img); }

	bool load_qoi(const char* pFilename, image& img);

	bool load_jpg(const char *pFilename, image& img);
	inline bool load_jpg(const std::string &filename, image &img) { return load_jpg(filename.c_str(), img); }
	
	// Currently loads .PNG, .TGA, or .JPG
	bool load_image(const char* pFilename, image& img);
	inline bool load_image(const std::string &filename, image &img) { return load_image(filename.c_str(), img); }

	// Supports .HDR and most (but not all) .EXR's (see TinyEXR).
	bool load_image_hdr(const char* pFilename, imagef& img, bool ldr_srgb_to_linear = true);
	inline bool load_image_hdr(const std::string& filename, imagef& img, bool ldr_srgb_to_linear = true) { return load_image_hdr(filename.c_str(), img, ldr_srgb_to_linear); }

	enum class hdr_image_type
	{
		cHITRGBAHalfFloat = 0,
		cHITRGBAFloat = 1,
		cHITPNGImage = 2,
		cHITEXRImage = 3,
		cHITHDRImage = 4
	};

	bool load_image_hdr(const void* pMem, size_t mem_size, imagef& img, uint32_t width, uint32_t height, hdr_image_type img_type, bool ldr_srgb_to_linear);

	uint8_t *read_tga(const uint8_t *pBuf, uint32_t buf_size, int &width, int &height, int &n_chans);
	uint8_t *read_tga(const char *pFilename, int &width, int &height, int &n_chans);
		
	struct rgbe_header_info
	{
		std::string m_program;

		// Note no validation is done, either gamma or exposure may be 0.
		double m_gamma;
		bool m_has_gamma;

		double m_exposure; // watts/steradian/m^2.
		bool m_has_exposure;

		void clear() 
		{ 
			m_program.clear(); 
			m_gamma = 1.0f; 
			m_has_gamma = false; 
			m_exposure = 1.0f; 
			m_has_exposure = false; 
		}
	};

	bool read_rgbe(const uint8_vec& filedata, imagef& img, rgbe_header_info& hdr_info);
	bool read_rgbe(const char* pFilename, imagef& img, rgbe_header_info &hdr_info);

	bool write_rgbe(uint8_vec& file_data, imagef& img, rgbe_header_info& hdr_info);
	bool write_rgbe(const char* pFilename, imagef& img, rgbe_header_info& hdr_info);

	bool read_exr(const char* pFilename, imagef& img, int& n_chans);
	bool read_exr(const void* pMem, size_t mem_size, imagef& img);
	
	enum
	{
		WRITE_EXR_LINEAR_HINT = 1, // hint for lossy comp. methods: exr_perceptual_treatment_t, logarithmic or linear, defaults to logarithmic
		WRITE_EXR_STORE_FLOATS = 2, // use 32-bit floats, otherwise it uses half floats
		WRITE_EXR_NO_COMPRESSION = 4 // no compression, otherwise it uses ZIP compression (16 scanlines per block)
	};

	// Supports 1 (Y), 3 (RGB), or 4 (RGBA) channel images.
	bool write_exr(const char* pFilename, imagef& img, uint32_t n_chans, uint32_t flags);
			
	enum
	{
		cImageSaveGrayscale = 1,
		cImageSaveIgnoreAlpha = 2
	};

	bool save_png(const char* pFilename, const image& img, uint32_t image_save_flags = 0, uint32_t grayscale_comp = 0);
	inline bool save_png(const std::string &filename, const image &img, uint32_t image_save_flags = 0, uint32_t grayscale_comp = 0) { return save_png(filename.c_str(), img, image_save_flags, grayscale_comp); }
	
	bool read_file_to_vec(const char* pFilename, uint8_vec& data);
	bool read_file_to_data(const char* pFilename, void *pData, size_t len);	

	bool write_data_to_file(const char* pFilename, const void* pData, size_t len);
	
	inline bool write_vec_to_file(const char* pFilename, const uint8_vec& v) {	return v.size() ? write_data_to_file(pFilename, &v[0], v.size()) : write_data_to_file(pFilename, "", 0); }
		
	bool image_resample(const image &src, image &dst, bool srgb = false,
		const char *pFilter = "lanczos4", float filter_scale = 1.0f, 
		bool wrapping = false,
		uint32_t first_comp = 0, uint32_t num_comps = 4);

	bool image_resample(const imagef& src, imagef& dst, 
		const char* pFilter = "lanczos4", float filter_scale = 1.0f,
		bool wrapping = false,
		uint32_t first_comp = 0, uint32_t num_comps = 4);
		
	// Timing
			
	typedef uint64_t timer_ticks;

	class interval_timer
	{
	public:
		interval_timer();

		void start();
		void stop();

		double get_elapsed_secs() const;
		inline double get_elapsed_ms() const { return 1000.0f* get_elapsed_secs(); }
		
		static void init();
		static inline timer_ticks get_ticks_per_sec() { return g_freq; }
		static timer_ticks get_ticks();
		static double ticks_to_secs(timer_ticks ticks);
		static inline double ticks_to_ms(timer_ticks ticks) {	return ticks_to_secs(ticks) * 1000.0f; }

	private:
		static timer_ticks g_init_ticks, g_freq;
		static double g_timer_freq;

		timer_ticks m_start_time, m_stop_time;

		bool m_started, m_stopped;
	};

	inline double get_interval_timer() { return interval_timer::ticks_to_secs(interval_timer::get_ticks()); }

	// 2D array

	template<typename T>
	class vector2D
	{
		typedef basisu::vector<T> TVec;

		uint32_t m_width, m_height;
		TVec m_values;

	public:
		vector2D() :
			m_width(0),
			m_height(0)
		{
		}

		vector2D(uint32_t w, uint32_t h) :
			m_width(0),
			m_height(0)
		{
			resize(w, h);
		}

		vector2D(const vector2D &other)
		{
			*this = other;
		}

		vector2D &operator= (const vector2D &other)
		{
			if (this != &other)
			{
				m_width = other.m_width;
				m_height = other.m_height;
				m_values = other.m_values;
			}
			return *this;
		}

		inline bool operator== (const vector2D &rhs) const
		{
			return (m_width == rhs.m_width) && (m_height == rhs.m_height) && (m_values == rhs.m_values);
		}

		inline uint32_t size_in_bytes() const { return (uint32_t)m_values.size() * sizeof(m_values[0]); }

		inline const T &operator() (uint32_t x, uint32_t y) const { assert(x < m_width && y < m_height); return m_values[x + y * m_width]; }
		inline T &operator() (uint32_t x, uint32_t y) { assert(x < m_width && y < m_height); return m_values[x + y * m_width]; }

		inline const T &operator[] (uint32_t i) const { return m_values[i]; }
		inline T &operator[] (uint32_t i) { return m_values[i]; }
				
		inline const T &at_clamped(int x, int y) const { return (*this)(clamp<int>(x, 0, m_width - 1), clamp<int>(y, 0, m_height - 1)); }		
		inline T &at_clamped(int x, int y) { return (*this)(clamp<int>(x, 0, m_width - 1), clamp<int>(y, 0, m_height - 1)); }

		void clear()
		{
			m_width = 0;
			m_height = 0;
			m_values.clear();
		}

		void set_all(const T&val)
		{
			vector_set_all(m_values, val);
		}

		inline const T* get_ptr() const { return &m_values[0]; }
		inline T* get_ptr() { return &m_values[0]; }

		vector2D &resize(uint32_t new_width, uint32_t new_height)
		{
			if ((m_width == new_width) && (m_height == new_height))
				return *this;

			TVec oldVals(new_width * new_height);
			oldVals.swap(m_values);

			const uint32_t w = minimum(m_width, new_width);
			const uint32_t h = minimum(m_height, new_height);

			if ((w) && (h))
			{
				for (uint32_t y = 0; y < h; y++)
					for (uint32_t x = 0; x < w; x++)
						m_values[x + y * new_width] = oldVals[x + y * m_width];
			}

			m_width = new_width;
			m_height = new_height;

			return *this;
		}
	};

	inline FILE *fopen_safe(const char *pFilename, const char *pMode)
	{
#ifdef _WIN32
		FILE *pFile = nullptr;
		fopen_s(&pFile, pFilename, pMode);
		return pFile;
#else
		return fopen(pFilename, pMode);
#endif
	}

	void fill_buffer_with_random_bytes(void *pBuf, size_t size, uint32_t seed = 1);

	const uint32_t cPixelBlockWidth = 4;
	const uint32_t cPixelBlockHeight = 4;
	const uint32_t cPixelBlockTotalPixels = cPixelBlockWidth * cPixelBlockHeight;

	struct pixel_block
	{
		color_rgba m_pixels[cPixelBlockHeight][cPixelBlockWidth]; // [y][x]

		inline const color_rgba& operator() (uint32_t x, uint32_t y) const { assert((x < cPixelBlockWidth) && (y < cPixelBlockHeight)); return m_pixels[y][x]; }
		inline color_rgba& operator() (uint32_t x, uint32_t y) { assert((x < cPixelBlockWidth) && (y < cPixelBlockHeight)); return m_pixels[y][x]; }

		inline const color_rgba* get_ptr() const { return &m_pixels[0][0]; }
		inline color_rgba* get_ptr() { return &m_pixels[0][0]; }

		inline void clear() { clear_obj(*this); }

		inline bool operator== (const pixel_block& rhs) const
		{
			return memcmp(m_pixels, rhs.m_pixels, sizeof(m_pixels)) == 0;
		}
	};
	typedef basisu::vector<pixel_block> pixel_block_vec;

	struct pixel_block_hdr
	{
		vec4F m_pixels[cPixelBlockHeight][cPixelBlockWidth]; // [y][x]

		inline const vec4F& operator() (uint32_t x, uint32_t y) const { assert((x < cPixelBlockWidth) && (y < cPixelBlockHeight)); return m_pixels[y][x]; }
		inline vec4F& operator() (uint32_t x, uint32_t y) { assert((x < cPixelBlockWidth) && (y < cPixelBlockHeight)); return m_pixels[y][x]; }

		inline const vec4F* get_ptr() const { return &m_pixels[0][0]; }
		inline vec4F* get_ptr() { return &m_pixels[0][0]; }

		inline void clear() { clear_obj(*this); }

		inline bool operator== (const pixel_block& rhs) const
		{
			return memcmp(m_pixels, rhs.m_pixels, sizeof(m_pixels)) == 0;
		}
	};
	typedef basisu::vector<pixel_block_hdr> pixel_block_hdr_vec;

	void tonemap_image_reinhard(image& ldr_img, const imagef& hdr_img, float exposure);
	bool tonemap_image_compressive(image& dst_img, const imagef& hdr_test_img);
	
	// Intersection
	enum eClear { cClear = 0 };
	enum eInitExpand { cInitExpand = 0 };

	template<typename vector_type>
	class ray
	{
	public:
		typedef vector_type vector_t;
		typedef typename vector_type::scalar_type scalar_type;

		inline ray() { }
		inline ray(eClear) { clear(); }
		inline ray(const vector_type& origin, const vector_type& direction) : m_origin(origin), m_direction(direction) { }

		inline void clear()
		{
			m_origin.clear();
			m_direction.clear();
		}

		inline const vector_type& get_origin(void) const { return m_origin; }
		inline void set_origin(const vector_type& origin) { m_origin = origin; }

		inline const vector_type& get_direction(void) const { return m_direction; }
		inline void set_direction(const vector_type& direction) { m_direction = direction; }

		inline void set_endpoints(const vector_type& start, const vector_type& end)
		{
			m_origin = start;

			m_direction = end - start;
			m_direction.normalize_in_place();
		}

		inline vector_type eval(scalar_type t) const
		{
			return m_origin + m_direction * t;
		}

	private:
		vector_type m_origin;
		vector_type m_direction;
	};

	typedef ray<vec2F> ray2F;
	typedef ray<vec3F> ray3F;

	template<typename T>
	class vec_interval
	{
	public:
		enum { N = T::num_elements };
		typedef typename T::scalar_type scalar_type;

		inline vec_interval(const T& v) { m_bounds[0] = v; m_bounds[1] = v; }
		inline vec_interval(const T& low, const T& high) { m_bounds[0] = low; m_bounds[1] = high; }

		inline vec_interval() { }
		inline vec_interval(eClear) { clear(); }
		inline vec_interval(eInitExpand) { init_expand(); }

		inline void clear() { m_bounds[0].clear(); m_bounds[1].clear(); }

		inline void init_expand()
		{
			m_bounds[0].set(1e+30f, 1e+30f, 1e+30f);
			m_bounds[1].set(-1e+30f, -1e+30f, -1e+30f);
		}

		inline vec_interval expand(const T& p)
		{
			for (uint32_t c = 0; c < N; c++)
			{
				if (p[c] < m_bounds[0][c])
					m_bounds[0][c] = p[c];

				if (p[c] > m_bounds[1][c])
					m_bounds[1][c] = p[c];
			}

			return *this;
		}

		inline const T& operator[] (uint32_t i) const { assert(i < 2); return m_bounds[i]; }
		inline       T& operator[] (uint32_t i) { assert(i < 2); return m_bounds[i]; }

		const T& get_low() const { return m_bounds[0]; }
		T& get_low() { return m_bounds[0]; }

		const T& get_high() const { return m_bounds[1]; }
		T& get_high() { return m_bounds[1]; }

		scalar_type get_dim(uint32_t axis) const { return m_bounds[1][axis] - m_bounds[0][axis]; }

		bool contains(const T& p) const
		{
			const T& low = get_low(), high = get_high();

			for (uint32_t i = 0; i < N; i++)
			{
				if (p[i] < low[i])
					return false;

				if (p[i] > high[i])
					return false;
			}
			return true;
		}

	private:
		T m_bounds[2];
	};

	typedef vec_interval<vec1F> vec_interval1F;
	typedef vec_interval<vec2F> vec_interval2F;
	typedef vec_interval<vec3F> vec_interval3F;
	typedef vec_interval<vec4F> vec_interval4F;

	typedef vec_interval2F aabb2F;
	typedef vec_interval3F aabb3F;

	namespace intersection
	{
		enum result
		{
			cBackfacing = -1,
			cFailure = 0,
			cSuccess,
			cParallel,
			cInside,
		};

		// Returns cInside, cSuccess, or cFailure.
		// Algorithm: Graphics Gems 1
		template<typename vector_type, typename scalar_type, typename ray_type, typename aabb_type>
		result ray_aabb(vector_type& coord, scalar_type& t, const ray_type& ray, const aabb_type& box)
		{
			enum
			{
				cNumDim = vector_type::num_elements,
				cRight = 0,
				cLeft = 1,
				cMiddle = 2
			};

			bool inside = true;
			int quadrant[cNumDim];
			scalar_type candidate_plane[cNumDim];

			for (int i = 0; i < cNumDim; i++)
			{
				if (ray.get_origin()[i] < box[0][i])
				{
					quadrant[i] = cLeft;
					candidate_plane[i] = box[0][i];
					inside = false;
				}
				else if (ray.get_origin()[i] > box[1][i])
				{
					quadrant[i] = cRight;
					candidate_plane[i] = box[1][i];
					inside = false;
				}
				else
				{
					quadrant[i] = cMiddle;
				}
			}

			if (inside)
			{
				coord = ray.get_origin();
				t = 0.0f;
				return cInside;
			}

			scalar_type max_t[cNumDim];
			for (int i = 0; i < cNumDim; i++)
			{
				if ((quadrant[i] != cMiddle) && (ray.get_direction()[i] != 0.0f))
					max_t[i] = (candidate_plane[i] - ray.get_origin()[i]) / ray.get_direction()[i];
				else
					max_t[i] = -1.0f;
			}

			int which_plane = 0;
			for (int i = 1; i < cNumDim; i++)
				if (max_t[which_plane] < max_t[i])
					which_plane = i;

			if (max_t[which_plane] < 0.0f)
				return cFailure;

			for (int i = 0; i < cNumDim; i++)
			{
				if (i != which_plane)
				{
					coord[i] = ray.get_origin()[i] + max_t[which_plane] * ray.get_direction()[i];

					if ((coord[i] < box[0][i]) || (coord[i] > box[1][i]))
						return cFailure;
				}
				else
				{
					coord[i] = candidate_plane[i];
				}

				assert(coord[i] >= box[0][i] && coord[i] <= box[1][i]);
			}

			t = max_t[which_plane];
			return cSuccess;
		}

		template<typename vector_type, typename scalar_type, typename ray_type, typename aabb_type>
		result ray_aabb(bool& started_within, vector_type& coord, scalar_type& t, const ray_type& ray, const aabb_type& box)
		{
			if (!box.contains(ray.get_origin()))
			{
				started_within = false;
				return ray_aabb(coord, t, ray, box);
			}

			started_within = true;

			typename vector_type::T diag_dist = box.diagonal_length() * 1.5f;
			ray_type outside_ray(ray.eval(diag_dist), -ray.get_direction());

			result res(ray_aabb(coord, t, outside_ray, box));
			if (res != cSuccess)
				return res;

			t = basisu::maximum(0.0f, diag_dist - t);
			return cSuccess;
		}

	} // intersect

	// This float->half conversion matches how "F32TO16" works on Intel GPU's.
	// Input cannot be negative, Inf or Nan.
	inline basist::half_float float_to_half_non_neg_no_nan_inf(float val)
	{
		union { float f; int32_t i; uint32_t u; } fi = { val };
		const int flt_m = fi.i & 0x7FFFFF, flt_e = (fi.i >> 23) & 0xFF;
		int e = 0, m = 0;

		assert(((fi.i >> 31) == 0) && (flt_e != 0xFF));

		// not zero or denormal
		if (flt_e != 0)
		{
			int new_exp = flt_e - 127;
			if (new_exp > 15)
				e = 31;
			else if (new_exp < -14)
				m = lrintf((1 << 24) * fabsf(fi.f));
			else
			{
				e = new_exp + 15;
				m = lrintf(flt_m * (1.0f / ((float)(1 << 13))));
			}
		}

		assert((0 <= m) && (m <= 1024));
		if (m == 1024)
		{
			e++;
			m = 0;
		}

		assert((e >= 0) && (e <= 31));
		assert((m >= 0) && (m <= 1023));

		basist::half_float result = (basist::half_float)((e << 10) | m);
		return result;
	}

	// Supports positive and denormals only. No NaN or Inf.
	inline float fast_half_to_float_pos_not_inf_or_nan(basist::half_float h)
	{
		assert(!basist::half_is_signed(h) && !basist::is_half_inf_or_nan(h));

		union fu32
		{
			uint32_t u;
			float f;
		};

		static const fu32 K = { 0x77800000 };

		fu32 o;
		o.u = h << 13;
		o.f *= K.f;

		return o.f;
	}
				
} // namespace basisu



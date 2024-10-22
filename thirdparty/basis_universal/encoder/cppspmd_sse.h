// cppspmd_sse.h
// Copyright 2020-2022 Binomial LLC
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
// Notes for Basis Universal:
// All of the "cppspmd" code and headers are OPTIONAL to Basis Universal. if BASISU_SUPPORT_SSE is 0, it will never be included and does not impact compilation.
// The techniques used in this code were originally demonstrated for AVX2 by Nicolas Guillemot, Jefferson Amstutz in their "CppSPMD" project.
// This is new code for use in Basis Universal, although it uses the same general SPMD techniques in SSE 2/4.

#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <math.h>
#include <utility>
#include <algorithm>

#if CPPSPMD_SSE2
#include <xmmintrin.h>		// SSE
#include <emmintrin.h>		// SSE2
#else
#include <xmmintrin.h>		// SSE
#include <emmintrin.h>		// SSE2
#include <pmmintrin.h>		// SSE3
#include <tmmintrin.h>		// SSSE3
#include <smmintrin.h>		// SSE4.1
//#include <nmmintrin.h>		// SSE4.2
#endif

#undef CPPSPMD_SSE
#undef CPPSPMD_AVX1
#undef CPPSPMD_AVX2
#undef CPPSPMD_AVX
#undef CPPSPMD_FLOAT4
#undef CPPSPMD_INT16

#define CPPSPMD_SSE 1
#define CPPSPMD_AVX 0
#define CPPSPMD_AVX1 0
#define CPPSPMD_AVX2 0
#define CPPSPMD_FLOAT4 0
#define CPPSPMD_INT16 0

#ifdef _MSC_VER
	#ifndef CPPSPMD_DECL
	#define CPPSPMD_DECL(type, name) __declspec(align(16)) type name
	#endif

	#ifndef CPPSPMD_ALIGN
	#define CPPSPMD_ALIGN(v) __declspec(align(v))
	#endif

	#define _mm_undefined_si128 _mm_setzero_si128
	#define _mm_undefined_ps _mm_setzero_ps
#else
	#ifndef CPPSPMD_DECL
	#define CPPSPMD_DECL(type, name) type name __attribute__((aligned(32)))
	#endif

	#ifndef CPPSPMD_ALIGN
	#define CPPSPMD_ALIGN(v) __attribute__((aligned(v)))
	#endif
#endif

#ifndef CPPSPMD_FORCE_INLINE
#ifdef _DEBUG
#define CPPSPMD_FORCE_INLINE inline
#else
	#ifdef _MSC_VER
		#define CPPSPMD_FORCE_INLINE __forceinline
	#else
		#define CPPSPMD_FORCE_INLINE inline
	#endif
#endif
#endif

#undef CPPSPMD
#undef CPPSPMD_ARCH

#if CPPSPMD_SSE2
	#define CPPSPMD_SSE41 0
	#define CPPSPMD cppspmd_sse2
	#define CPPSPMD_ARCH _sse2
#else
	#define CPPSPMD_SSE41 1
	#define CPPSPMD cppspmd_sse41
	#define CPPSPMD_ARCH _sse41
#endif

#ifndef CPPSPMD_GLUER
	#define CPPSPMD_GLUER(a, b) a##b
#endif

#ifndef CPPSPMD_GLUER2
	#define CPPSPMD_GLUER2(a, b) CPPSPMD_GLUER(a, b)
#endif

#ifndef CPPSPMD_NAME
#define CPPSPMD_NAME(a) CPPSPMD_GLUER2(a, CPPSPMD_ARCH)
#endif

#undef VASSERT
#define VCOND(cond) ((exec_mask(vbool(cond)) & m_exec).get_movemask() == m_exec.get_movemask())
#define VASSERT(cond) assert( VCOND(cond) )

#define CPPSPMD_ALIGNMENT (16)

#define storeu_si32(p, a) (void)(*(int*)(p) = _mm_cvtsi128_si32((a)))

namespace CPPSPMD
{

const int PROGRAM_COUNT_SHIFT = 2;
const int PROGRAM_COUNT = 1 << PROGRAM_COUNT_SHIFT;

template <typename N> inline N* aligned_new() { void* p = _mm_malloc(sizeof(N), 64); new (p) N;	return static_cast<N*>(p); }
template <typename N> void aligned_delete(N* p) { if (p) { p->~N(); _mm_free(p); } }

CPPSPMD_DECL(const uint32_t, g_allones_128[4]) = { UINT32_MAX, UINT32_MAX, UINT32_MAX, UINT32_MAX };
CPPSPMD_DECL(const uint32_t, g_x_128[4]) = { UINT32_MAX, 0, 0, 0 };
CPPSPMD_DECL(const float, g_onef_128[4]) = { 1.0f, 1.0f, 1.0f, 1.0f };
CPPSPMD_DECL(const uint32_t, g_oneu_128[4]) = { 1, 1, 1, 1 };

CPPSPMD_DECL(const uint32_t, g_lane_masks_128[4][4]) = 
{ 
	{ UINT32_MAX, 0, 0, 0 },
	{ 0, UINT32_MAX, 0, 0 },
	{ 0, 0, UINT32_MAX, 0 },
	{ 0, 0, 0, UINT32_MAX },
};

#if CPPSPMD_SSE41
CPPSPMD_FORCE_INLINE __m128i _mm_blendv_epi32(__m128i a, __m128i b, __m128i c) { return _mm_castps_si128(_mm_blendv_ps(_mm_castsi128_ps(a), _mm_castsi128_ps(b), _mm_castsi128_ps(c))); }
#endif

CPPSPMD_FORCE_INLINE __m128i blendv_epi8(__m128i a, __m128i b, __m128i mask)
{
#if CPPSPMD_SSE2
	return _mm_castps_si128(_mm_or_ps(_mm_and_ps(_mm_castsi128_ps(mask), _mm_castsi128_ps(b)), _mm_andnot_ps(_mm_castsi128_ps(mask), _mm_castsi128_ps(a))));
#else
	return _mm_blendv_epi8(a, b, mask);
#endif
}

CPPSPMD_FORCE_INLINE __m128 blendv_mask_ps(__m128 a, __m128 b, __m128 mask)
{
#if CPPSPMD_SSE2
	// We know it's a mask, so we can just emulate the blend.
	return _mm_or_ps(_mm_and_ps(mask, b), _mm_andnot_ps(mask, a));
#else
	return _mm_blendv_ps(a, b, mask);
#endif
}

CPPSPMD_FORCE_INLINE __m128 blendv_ps(__m128 a, __m128 b, __m128 mask)
{
#if CPPSPMD_SSE2
	// Input is not a mask, but MSB bits - so emulate _mm_blendv_ps() by replicating bit 31.
	mask = _mm_castsi128_ps(_mm_srai_epi32(_mm_castps_si128(mask), 31));
	return _mm_or_ps(_mm_and_ps(mask, b), _mm_andnot_ps(mask, a));
#else
	return _mm_blendv_ps(a, b, mask);
#endif
}

CPPSPMD_FORCE_INLINE __m128i blendv_mask_epi32(__m128i a, __m128i b, __m128i mask)
{
	return _mm_castps_si128(blendv_mask_ps(_mm_castsi128_ps(a), _mm_castsi128_ps(b), _mm_castsi128_ps(mask)));
}

CPPSPMD_FORCE_INLINE __m128i blendv_epi32(__m128i a, __m128i b, __m128i mask)
{
	return _mm_castps_si128(blendv_ps(_mm_castsi128_ps(a), _mm_castsi128_ps(b), _mm_castsi128_ps(mask)));
}

#if CPPSPMD_SSE2
CPPSPMD_FORCE_INLINE int extract_x(const __m128i& vec) { return _mm_cvtsi128_si32(vec); }
CPPSPMD_FORCE_INLINE int extract_y(const __m128i& vec) { return _mm_cvtsi128_si32(_mm_shuffle_epi32(vec, 0x55)); }
CPPSPMD_FORCE_INLINE int extract_z(const __m128i& vec) { return _mm_cvtsi128_si32(_mm_shuffle_epi32(vec, 0xAA)); }
CPPSPMD_FORCE_INLINE int extract_w(const __m128i& vec) { return _mm_cvtsi128_si32(_mm_shuffle_epi32(vec, 0xFF)); }

// Returns float bits as int, to emulate _mm_extract_ps()
CPPSPMD_FORCE_INLINE int extract_ps_x(const __m128& vec) { float f = _mm_cvtss_f32(vec); return *(const int*)&f;  }
CPPSPMD_FORCE_INLINE int extract_ps_y(const __m128& vec) { float f = _mm_cvtss_f32(_mm_shuffle_ps(vec, vec, 0x55)); return *(const int*)&f; }
CPPSPMD_FORCE_INLINE int extract_ps_z(const __m128& vec) { float f = _mm_cvtss_f32(_mm_shuffle_ps(vec, vec, 0xAA)); return *(const int*)&f; }
CPPSPMD_FORCE_INLINE int extract_ps_w(const __m128& vec) { float f = _mm_cvtss_f32(_mm_shuffle_ps(vec, vec, 0xFF)); return *(const int*)&f; }

// Returns floats
CPPSPMD_FORCE_INLINE float extractf_ps_x(const __m128& vec) { return _mm_cvtss_f32(vec); }
CPPSPMD_FORCE_INLINE float extractf_ps_y(const __m128& vec) { return _mm_cvtss_f32(_mm_shuffle_ps(vec, vec, 0x55)); }
CPPSPMD_FORCE_INLINE float extractf_ps_z(const __m128& vec) { return _mm_cvtss_f32(_mm_shuffle_ps(vec, vec, 0xAA)); }
CPPSPMD_FORCE_INLINE float extractf_ps_w(const __m128& vec) { return _mm_cvtss_f32(_mm_shuffle_ps(vec, vec, 0xFF)); }
#else
CPPSPMD_FORCE_INLINE int extract_x(const __m128i& vec) { return _mm_extract_epi32(vec, 0); }
CPPSPMD_FORCE_INLINE int extract_y(const __m128i& vec) { return _mm_extract_epi32(vec, 1); }
CPPSPMD_FORCE_INLINE int extract_z(const __m128i& vec) { return _mm_extract_epi32(vec, 2); }
CPPSPMD_FORCE_INLINE int extract_w(const __m128i& vec) { return _mm_extract_epi32(vec, 3); }

// Returns float bits as int
CPPSPMD_FORCE_INLINE int extract_ps_x(const __m128& vec) { return _mm_extract_ps(vec, 0); }
CPPSPMD_FORCE_INLINE int extract_ps_y(const __m128& vec) { return _mm_extract_ps(vec, 1); }
CPPSPMD_FORCE_INLINE int extract_ps_z(const __m128& vec) { return _mm_extract_ps(vec, 2); }
CPPSPMD_FORCE_INLINE int extract_ps_w(const __m128& vec) { return _mm_extract_ps(vec, 3); }

// Returns floats
CPPSPMD_FORCE_INLINE float extractf_ps_x(const __m128& vec) { int v = extract_ps_x(vec); return *(const float*)&v; }
CPPSPMD_FORCE_INLINE float extractf_ps_y(const __m128& vec) { int v = extract_ps_y(vec); return *(const float*)&v; }
CPPSPMD_FORCE_INLINE float extractf_ps_z(const __m128& vec) { int v = extract_ps_z(vec); return *(const float*)&v; }
CPPSPMD_FORCE_INLINE float extractf_ps_w(const __m128& vec) { int v = extract_ps_w(vec); return *(const float*)&v; }
#endif

#if CPPSPMD_SSE2
CPPSPMD_FORCE_INLINE __m128i insert_x(const __m128i& vec, int v) { return _mm_insert_epi16(_mm_insert_epi16(vec, v, 0), (uint32_t)v >> 16U, 1); }
CPPSPMD_FORCE_INLINE __m128i insert_y(const __m128i& vec, int v) { return _mm_insert_epi16(_mm_insert_epi16(vec, v, 2), (uint32_t)v >> 16U, 3); }
CPPSPMD_FORCE_INLINE __m128i insert_z(const __m128i& vec, int v) { return _mm_insert_epi16(_mm_insert_epi16(vec, v, 4), (uint32_t)v >> 16U, 5); }
CPPSPMD_FORCE_INLINE __m128i insert_w(const __m128i& vec, int v) { return _mm_insert_epi16(_mm_insert_epi16(vec, v, 6), (uint32_t)v >> 16U, 7); }
#else
CPPSPMD_FORCE_INLINE __m128i insert_x(const __m128i& vec, int v) { return _mm_insert_epi32(vec, v, 0); }
CPPSPMD_FORCE_INLINE __m128i insert_y(const __m128i& vec, int v) { return _mm_insert_epi32(vec, v, 1); }
CPPSPMD_FORCE_INLINE __m128i insert_z(const __m128i& vec, int v) { return _mm_insert_epi32(vec, v, 2); }
CPPSPMD_FORCE_INLINE __m128i insert_w(const __m128i& vec, int v) { return _mm_insert_epi32(vec, v, 3); }
#endif

#if CPPSPMD_SSE2
inline __m128i shuffle_epi8(const __m128i& a, const __m128i& b)
{
	// Just emulate _mm_shuffle_epi8. This is very slow, but what else can we do?
	CPPSPMD_ALIGN(16) uint8_t av[16];
	_mm_store_si128((__m128i*)av, a);
		
	CPPSPMD_ALIGN(16) uint8_t bvi[16];
	_mm_store_ps((float*)bvi, _mm_and_ps(_mm_castsi128_ps(b), _mm_castsi128_ps(_mm_set1_epi32(0x0F0F0F0F))));

	CPPSPMD_ALIGN(16) uint8_t result[16];

	result[0] = av[bvi[0]];
	result[1] = av[bvi[1]];
	result[2] = av[bvi[2]];
	result[3] = av[bvi[3]];
	
	result[4] = av[bvi[4]];
	result[5] = av[bvi[5]];
	result[6] = av[bvi[6]];
	result[7] = av[bvi[7]];

	result[8] = av[bvi[8]];
	result[9] = av[bvi[9]];
	result[10] = av[bvi[10]];
	result[11] = av[bvi[11]];

	result[12] = av[bvi[12]];
	result[13] = av[bvi[13]];
	result[14] = av[bvi[14]];
	result[15] = av[bvi[15]];

	return _mm_andnot_si128(_mm_cmplt_epi8(b, _mm_setzero_si128()), _mm_load_si128((__m128i*)result));
}
#else
CPPSPMD_FORCE_INLINE __m128i shuffle_epi8(const __m128i& a, const __m128i& b) 
{ 
	return _mm_shuffle_epi8(a, b); 
}
#endif

#if CPPSPMD_SSE2
CPPSPMD_FORCE_INLINE __m128i min_epi32(__m128i a, __m128i b)
{
	return blendv_mask_epi32(b, a, _mm_cmplt_epi32(a, b));
}
CPPSPMD_FORCE_INLINE __m128i max_epi32(__m128i a, __m128i b)
{
	return blendv_mask_epi32(b, a, _mm_cmpgt_epi32(a, b));
}
CPPSPMD_FORCE_INLINE __m128i min_epu32(__m128i a, __m128i b)
{
	__m128i n = _mm_set1_epi32(0x80000000);
	__m128i ac = _mm_add_epi32(a, n);
	__m128i bc = _mm_add_epi32(b, n);
	return blendv_mask_epi32(b, a, _mm_cmplt_epi32(ac, bc));
}
CPPSPMD_FORCE_INLINE __m128i max_epu32(__m128i a, __m128i b)
{
	__m128i n = _mm_set1_epi32(0x80000000);
	__m128i ac = _mm_add_epi32(a, n);
	__m128i bc = _mm_add_epi32(b, n);
	return blendv_mask_epi32(b, a, _mm_cmpgt_epi32(ac, bc));
}
#else
CPPSPMD_FORCE_INLINE __m128i min_epi32(__m128i a, __m128i b)
{
	return _mm_min_epi32(a, b);
}
CPPSPMD_FORCE_INLINE __m128i max_epi32(__m128i a, __m128i b)
{
	return _mm_max_epi32(a, b);
}
CPPSPMD_FORCE_INLINE __m128i min_epu32(__m128i a, __m128i b)
{
	return _mm_min_epu32(a, b);
}
CPPSPMD_FORCE_INLINE __m128i max_epu32(__m128i a, __m128i b)
{
	return _mm_max_epu32(a, b);
}
#endif

#if CPPSPMD_SSE2
CPPSPMD_FORCE_INLINE __m128i abs_epi32(__m128i a)
{
	__m128i sign_mask = _mm_srai_epi32(a, 31);
	return _mm_sub_epi32(_mm_castps_si128(_mm_xor_ps(_mm_castsi128_ps(a), _mm_castsi128_ps(sign_mask))), sign_mask);
}
#else
CPPSPMD_FORCE_INLINE __m128i abs_epi32(__m128i a)
{
	return _mm_abs_epi32(a);
}
#endif

#if CPPSPMD_SSE2
CPPSPMD_FORCE_INLINE __m128i mullo_epi32(__m128i a, __m128i b)
{
	__m128i tmp1 = _mm_mul_epu32(a, b);
	__m128i tmp2 = _mm_mul_epu32(_mm_srli_si128(a, 4), _mm_srli_si128(b, 4));
	return _mm_unpacklo_epi32(_mm_shuffle_epi32(tmp1, _MM_SHUFFLE(0, 0, 2, 0)), _mm_shuffle_epi32(tmp2, _MM_SHUFFLE(0, 0, 2, 0)));
}
#else
CPPSPMD_FORCE_INLINE __m128i mullo_epi32(__m128i a, __m128i b)
{
	return _mm_mullo_epi32(a, b);
}
#endif

CPPSPMD_FORCE_INLINE __m128i mulhi_epu32(__m128i a, __m128i b)
{
	__m128i tmp1 = _mm_mul_epu32(a, b);
	__m128i tmp2 = _mm_mul_epu32(_mm_srli_si128(a, 4), _mm_srli_si128(b, 4));
	return _mm_unpacklo_epi32(_mm_shuffle_epi32(tmp1, _MM_SHUFFLE(0, 0, 3, 1)), _mm_shuffle_epi32(tmp2, _MM_SHUFFLE(0, 0, 3, 1)));
}

#if CPPSPMD_SSE2
inline __m128i load_rgba32(const void* p)
{
	__m128i xmm = _mm_cvtsi32_si128(*(const int*)p);
	xmm = _mm_unpacklo_epi8(xmm, _mm_setzero_si128());
	xmm = _mm_unpacklo_epi16(xmm, _mm_setzero_si128());
	return xmm;
}
#else
inline __m128i load_rgba32(const void* p)
{
	return _mm_cvtepu8_epi32(_mm_castps_si128(_mm_load_ss((const float*)p)));
}
#endif

inline void transpose4x4(__m128i& x, __m128i& y, __m128i& z, __m128i& w, const __m128i& r0, const __m128i& r1, const __m128i& r2, const __m128i& r3)
{
	__m128i t0 = _mm_unpacklo_epi32(r0, r1);
	__m128i t1 = _mm_unpacklo_epi32(r2, r3);
	__m128i t2 = _mm_unpackhi_epi32(r0, r1);
	__m128i t3 = _mm_unpackhi_epi32(r2, r3);
	x = _mm_unpacklo_epi64(t0, t1);
	y = _mm_unpackhi_epi64(t0, t1);
	z = _mm_unpacklo_epi64(t2, t3);
	w = _mm_unpackhi_epi64(t2, t3);
}

const uint32_t ALL_ON_MOVEMASK = 0xF;

struct spmd_kernel
{
	struct vint;
	struct lint;
	struct vbool;
	struct vfloat;

	typedef int int_t;
	typedef vint vint_t;
	typedef lint lint_t;
		
	// Exec mask
	struct exec_mask
	{
		__m128i m_mask;

		exec_mask() = default;

		CPPSPMD_FORCE_INLINE explicit exec_mask(const vbool& b);
		CPPSPMD_FORCE_INLINE explicit exec_mask(const __m128i& mask) : m_mask(mask) { }

		CPPSPMD_FORCE_INLINE void enable_lane(uint32_t lane) { m_mask = _mm_load_si128((const __m128i *)&g_lane_masks_128[lane][0]); }
				
		static CPPSPMD_FORCE_INLINE exec_mask all_on()	{ return exec_mask{ _mm_load_si128((const __m128i*)g_allones_128) };	}
		static CPPSPMD_FORCE_INLINE exec_mask all_off() { return exec_mask{ _mm_setzero_si128() }; }

		CPPSPMD_FORCE_INLINE uint32_t get_movemask() const { return _mm_movemask_ps(_mm_castsi128_ps(m_mask)); }
	};

	friend CPPSPMD_FORCE_INLINE bool all(const exec_mask& e);
	friend CPPSPMD_FORCE_INLINE bool any(const exec_mask& e);

	CPPSPMD_FORCE_INLINE bool spmd_all() const { return all(m_exec); }
	CPPSPMD_FORCE_INLINE bool spmd_any() const { return any(m_exec); }
	CPPSPMD_FORCE_INLINE bool spmd_none() { return !any(m_exec); }

	// true if cond is true for all active lanes - false if no active lanes
	CPPSPMD_FORCE_INLINE bool spmd_all(const vbool& e) { uint32_t m = m_exec.get_movemask(); return (m != 0) && ((exec_mask(e) & m_exec).get_movemask() == m); }
	// true if cond is true for any active lanes
	CPPSPMD_FORCE_INLINE bool spmd_any(const vbool& e) { return (exec_mask(e) & m_exec).get_movemask() != 0; }
	CPPSPMD_FORCE_INLINE bool spmd_none(const vbool& e) { return !spmd_any(e); }

	friend CPPSPMD_FORCE_INLINE exec_mask operator^ (const exec_mask& a, const exec_mask& b);
	friend CPPSPMD_FORCE_INLINE exec_mask operator& (const exec_mask& a, const exec_mask& b);
	friend CPPSPMD_FORCE_INLINE exec_mask operator| (const exec_mask& a, const exec_mask& b);
		
	exec_mask m_exec;
	exec_mask m_kernel_exec;
	exec_mask m_continue_mask;
#ifdef _DEBUG
	bool m_in_loop;
#endif
		
	CPPSPMD_FORCE_INLINE uint32_t get_movemask() const { return m_exec.get_movemask(); }
		
	void init(const exec_mask& kernel_exec);
	
	// Varying bool
		
	struct vbool
	{
		__m128i m_value;

		vbool() = default;

		CPPSPMD_FORCE_INLINE vbool(bool value) : m_value(_mm_set1_epi32(value ? UINT32_MAX : 0)) { }

		CPPSPMD_FORCE_INLINE explicit vbool(const __m128i& value) : m_value(value) { }

		CPPSPMD_FORCE_INLINE explicit operator vfloat() const;
		CPPSPMD_FORCE_INLINE explicit operator vint() const;
								
	private:
		//vbool& operator=(const vbool&);
	};

	friend vbool operator!(const vbool& v);
		
	CPPSPMD_FORCE_INLINE vbool& store(vbool& dst, const vbool& src)
	{
		dst.m_value = blendv_mask_epi32(dst.m_value, src.m_value, m_exec.m_mask);
		return dst;
	}
		
	CPPSPMD_FORCE_INLINE vbool& store_all(vbool& dst, const vbool& src)
	{
		dst.m_value = src.m_value;
		return dst;
	}
	
	// Varying float
	struct vfloat
	{
		__m128 m_value;

		vfloat() = default;

		CPPSPMD_FORCE_INLINE explicit vfloat(const __m128& v) : m_value(v) { }

		CPPSPMD_FORCE_INLINE vfloat(float value) : m_value(_mm_set1_ps(value)) { }

		CPPSPMD_FORCE_INLINE explicit vfloat(int value) : m_value(_mm_set1_ps((float)value)) { }

	private:
		//vfloat& operator=(const vfloat&);
	};

	CPPSPMD_FORCE_INLINE vfloat& store(vfloat& dst, const vfloat& src)
	{
		dst.m_value = blendv_mask_ps(dst.m_value, src.m_value, _mm_castsi128_ps(m_exec.m_mask));
		return dst;
	}

	CPPSPMD_FORCE_INLINE vfloat& store(vfloat&& dst, const vfloat& src)
	{
		dst.m_value = blendv_mask_ps(dst.m_value, src.m_value, _mm_castsi128_ps(m_exec.m_mask));
		return dst;
	}
	
	CPPSPMD_FORCE_INLINE vfloat& store_all(vfloat& dst, const vfloat& src)
	{
		dst.m_value = src.m_value;
		return dst;
	}

	CPPSPMD_FORCE_INLINE vfloat& store_all(vfloat&& dst, const vfloat& src)
	{
		dst.m_value = src.m_value;
		return dst;
	}

	// Linear ref to floats
	struct float_lref
	{
		float* m_pValue;

	private:
		//float_lref& operator=(const float_lref&);
	};

	CPPSPMD_FORCE_INLINE const float_lref& store(const float_lref& dst, const vfloat& src)
	{
		int mask = _mm_movemask_ps(_mm_castsi128_ps(m_exec.m_mask));
		if (mask == ALL_ON_MOVEMASK)
			_mm_storeu_ps(dst.m_pValue, src.m_value);
		else
			_mm_storeu_ps(dst.m_pValue, blendv_mask_ps(_mm_loadu_ps(dst.m_pValue), src.m_value, _mm_castsi128_ps(m_exec.m_mask)));
		return dst;
	}

	CPPSPMD_FORCE_INLINE const float_lref& store(const float_lref&& dst, const vfloat& src)
	{
		int mask = _mm_movemask_ps(_mm_castsi128_ps(m_exec.m_mask));
		if (mask == ALL_ON_MOVEMASK)
			_mm_storeu_ps(dst.m_pValue, src.m_value);
		else
			_mm_storeu_ps(dst.m_pValue, blendv_mask_ps(_mm_loadu_ps(dst.m_pValue), src.m_value, _mm_castsi128_ps(m_exec.m_mask)));
		return dst;
	}
	
	CPPSPMD_FORCE_INLINE const float_lref& store_all(const float_lref& dst, const vfloat& src)
	{
		_mm_storeu_ps(dst.m_pValue, src.m_value);
		return dst;
	}

	CPPSPMD_FORCE_INLINE const float_lref& store_all(const float_lref&& dst, const vfloat& src)
	{
		_mm_storeu_ps(dst.m_pValue, src.m_value);
		return dst;
	}

	CPPSPMD_FORCE_INLINE vfloat load(const float_lref& src)
	{
		return vfloat{ _mm_and_ps(_mm_loadu_ps(src.m_pValue), _mm_castsi128_ps(m_exec.m_mask)) };
	}
		
	// Varying ref to floats
	struct float_vref
	{
		__m128i m_vindex;
		float* m_pValue;
		
	private:
		//float_vref& operator=(const float_vref&);
	};

	// Varying ref to varying float
	struct vfloat_vref
	{
		__m128i m_vindex;
		vfloat* m_pValue;
		
	private:
		//vfloat_vref& operator=(const vfloat_vref&);
	};

	// Varying ref to varying int
	struct vint_vref
	{
		__m128i m_vindex;
		vint* m_pValue;
		
	private:
		//vint_vref& operator=(const vint_vref&);
	};

	CPPSPMD_FORCE_INLINE const float_vref& store(const float_vref& dst, const vfloat& src);
	CPPSPMD_FORCE_INLINE const float_vref& store(const float_vref&& dst, const vfloat& src);
		
	CPPSPMD_FORCE_INLINE const float_vref& store_all(const float_vref& dst, const vfloat& src);
	CPPSPMD_FORCE_INLINE const float_vref& store_all(const float_vref&& dst, const vfloat& src);

	CPPSPMD_FORCE_INLINE vfloat load(const float_vref& src)
	{
		CPPSPMD_ALIGN(16) int vindex[4];
		_mm_store_si128((__m128i *)vindex, src.m_vindex);

		CPPSPMD_ALIGN(16) float loaded[4];

		int mask = _mm_movemask_ps(_mm_castsi128_ps(m_exec.m_mask));
		for (int i = 0; i < 4; i++)
		{
			if (mask & (1 << i))
				loaded[i] = src.m_pValue[vindex[i]];
		}
		return vfloat{ _mm_and_ps(_mm_castsi128_ps(m_exec.m_mask), _mm_load_ps((const float*)loaded)) };
	}

	CPPSPMD_FORCE_INLINE vfloat load_all(const float_vref& src)
	{
		CPPSPMD_ALIGN(16) int vindex[4];
		_mm_store_si128((__m128i *)vindex, src.m_vindex);

		CPPSPMD_ALIGN(16) float loaded[4];

		for (int i = 0; i < 4; i++)
			loaded[i] = src.m_pValue[vindex[i]];
		return vfloat{ _mm_load_ps((const float*)loaded) };
	}

	// Linear ref to ints
	struct int_lref
	{
		int* m_pValue;

	private:
		//int_lref& operator=(const int_lref&);
	};
		
	CPPSPMD_FORCE_INLINE const int_lref& store(const int_lref& dst, const vint& src)
	{
		int mask = _mm_movemask_ps(_mm_castsi128_ps(m_exec.m_mask));
		if (mask == ALL_ON_MOVEMASK)
		{
			_mm_storeu_si128((__m128i *)dst.m_pValue, src.m_value);
		}
		else
		{
			CPPSPMD_ALIGN(16) int stored[4];
			_mm_store_si128((__m128i *)stored, src.m_value);

			for (int i = 0; i < 4; i++)
			{
				if (mask & (1 << i))
					dst.m_pValue[i] = stored[i];
			}
		}
		return dst;
	}

	CPPSPMD_FORCE_INLINE vint load(const int_lref& src)
	{
		__m128i v = _mm_loadu_si128((const __m128i*)src.m_pValue);

		v = _mm_castps_si128(_mm_and_ps(_mm_castsi128_ps(v), _mm_castsi128_ps(m_exec.m_mask)));

		return vint{ v };
	}

	// Linear ref to int16's
	struct int16_lref
	{
		int16_t* m_pValue;

	private:
		//int16_lref& operator=(const int16_lref&);
	};

	CPPSPMD_FORCE_INLINE const int16_lref& store(const int16_lref& dst, const vint& src)
	{
		CPPSPMD_ALIGN(16) int stored[4];
		_mm_store_si128((__m128i *)stored, src.m_value);

		int mask = _mm_movemask_ps(_mm_castsi128_ps(m_exec.m_mask));
		for (int i = 0; i < 4; i++)
		{
			if (mask & (1 << i))
				dst.m_pValue[i] = static_cast<int16_t>(stored[i]);
		}
		return dst;
	}

	CPPSPMD_FORCE_INLINE const int16_lref& store_all(const int16_lref& dst, const vint& src)
	{
		CPPSPMD_ALIGN(16) int stored[4];
		_mm_store_si128((__m128i *)stored, src.m_value);

		for (int i = 0; i < 4; i++)
			dst.m_pValue[i] = static_cast<int16_t>(stored[i]);
		return dst;
	}
		
	CPPSPMD_FORCE_INLINE vint load(const int16_lref& src)
	{
		CPPSPMD_ALIGN(16) int values[4];

		for (int i = 0; i < 4; i++)
			values[i] = static_cast<int16_t>(src.m_pValue[i]);

		__m128i t = _mm_load_si128( (const __m128i *)values );

		return vint{ _mm_castps_si128(_mm_and_ps(_mm_castsi128_ps( t ), _mm_castsi128_ps(m_exec.m_mask))) };
	}

	CPPSPMD_FORCE_INLINE vint load_all(const int16_lref& src)
	{
		CPPSPMD_ALIGN(16) int values[4];

		for (int i = 0; i < 4; i++)
			values[i] = static_cast<int16_t>(src.m_pValue[i]);

		__m128i t = _mm_load_si128( (const __m128i *)values );

		return vint{ t };
	}
		
	// Linear ref to constant ints
	struct cint_lref
	{
		const int* m_pValue;

	private:
		//cint_lref& operator=(const cint_lref&);
	};

	CPPSPMD_FORCE_INLINE vint load(const cint_lref& src)
	{
		__m128i v = _mm_loadu_si128((const __m128i *)src.m_pValue);
		v = _mm_castps_si128(_mm_and_ps(_mm_castsi128_ps(v), _mm_castsi128_ps(m_exec.m_mask)));
		return vint{ v };
	}

	CPPSPMD_FORCE_INLINE vint load_all(const cint_lref& src)
	{
		return vint{ _mm_loadu_si128((const __m128i *)src.m_pValue) };
	}
	
	// Varying ref to ints
	struct int_vref
	{
		__m128i m_vindex;
		int* m_pValue;

	private:
		//int_vref& operator=(const int_vref&);
	};

	// Varying ref to constant ints
	struct cint_vref
	{
		__m128i m_vindex;
		const int* m_pValue;

	private:
		//cint_vref& operator=(const cint_vref&);
	};

	// Varying int
	struct vint
	{
		__m128i m_value;

		vint() = default;

		CPPSPMD_FORCE_INLINE explicit vint(const __m128i& value) : m_value(value)	{ }

		CPPSPMD_FORCE_INLINE explicit vint(const lint &other) : m_value(other.m_value) { }

		CPPSPMD_FORCE_INLINE vint& operator=(const lint& other) { m_value = other.m_value; return *this; }

		CPPSPMD_FORCE_INLINE vint(int value) : m_value(_mm_set1_epi32(value)) { }

		CPPSPMD_FORCE_INLINE explicit vint(float value) : m_value(_mm_set1_epi32((int)value))	{ }

		CPPSPMD_FORCE_INLINE explicit vint(const vfloat& other) : m_value(_mm_cvttps_epi32(other.m_value)) { }

		CPPSPMD_FORCE_INLINE explicit operator vbool() const 
		{
			return vbool{ _mm_xor_si128( _mm_load_si128((const __m128i*)g_allones_128), _mm_cmpeq_epi32(m_value, _mm_setzero_si128())) };
		}

		CPPSPMD_FORCE_INLINE explicit operator vfloat() const
		{
			return vfloat{ _mm_cvtepi32_ps(m_value) };
		}

		CPPSPMD_FORCE_INLINE int_vref operator[](int* ptr) const
		{
			return int_vref{ m_value, ptr };
		}

		CPPSPMD_FORCE_INLINE cint_vref operator[](const int* ptr) const
		{
			return cint_vref{ m_value, ptr };
		}

		CPPSPMD_FORCE_INLINE float_vref operator[](float* ptr) const
		{
			return float_vref{ m_value, ptr };
		}

		CPPSPMD_FORCE_INLINE vfloat_vref operator[](vfloat* ptr) const
		{
			return vfloat_vref{ m_value, ptr };
		}

		CPPSPMD_FORCE_INLINE vint_vref operator[](vint* ptr) const
		{
			return vint_vref{ m_value, ptr };
		}

	private:
		//vint& operator=(const vint&);
	};

	// Load/store linear int
	CPPSPMD_FORCE_INLINE void storeu_linear(int *pDst, const vint& src)
	{
		int mask = _mm_movemask_ps(_mm_castsi128_ps(m_exec.m_mask));
		if (mask == ALL_ON_MOVEMASK)
			_mm_storeu_si128((__m128i *)pDst, src.m_value);
		else
		{
			if (mask & 1) pDst[0] = extract_x(src.m_value);
			if (mask & 2) pDst[1] = extract_y(src.m_value);
			if (mask & 4) pDst[2] = extract_z(src.m_value);
			if (mask & 8) pDst[3] = extract_w(src.m_value);
		}
	}

	CPPSPMD_FORCE_INLINE void storeu_linear_all(int *pDst, const vint& src)
	{
		_mm_storeu_si128((__m128i*)pDst, src.m_value);
	}

	CPPSPMD_FORCE_INLINE void store_linear_all(int *pDst, const vint& src)
	{
		_mm_store_si128((__m128i*)pDst, src.m_value);
	}
		
	CPPSPMD_FORCE_INLINE vint loadu_linear(const int *pSrc)
	{
		__m128i v = _mm_loadu_si128((const __m128i*)pSrc);

		v = _mm_castps_si128(_mm_and_ps(_mm_castsi128_ps(v), _mm_castsi128_ps(m_exec.m_mask)));

		return vint{ v };
	}

	CPPSPMD_FORCE_INLINE vint loadu_linear_all(const int *pSrc)
	{
		return vint{ _mm_loadu_si128((__m128i*)pSrc) };
	}

	CPPSPMD_FORCE_INLINE vint load_linear_all(const int *pSrc)
	{
		return vint{ _mm_load_si128((__m128i*)pSrc) };
	}

	// Load/store linear float
	CPPSPMD_FORCE_INLINE void storeu_linear(float *pDst, const vfloat& src)
	{
		int mask = _mm_movemask_ps(_mm_castsi128_ps(m_exec.m_mask));
		if (mask == ALL_ON_MOVEMASK)
			_mm_storeu_ps((float*)pDst, src.m_value);
		else
		{
			int *pDstI = (int *)pDst;
			if (mask & 1) pDstI[0] = extract_ps_x(src.m_value);
			if (mask & 2) pDstI[1] = extract_ps_y(src.m_value);
			if (mask & 4) pDstI[2] = extract_ps_z(src.m_value);
			if (mask & 8) pDstI[3] = extract_ps_w(src.m_value);
		}
	}

	CPPSPMD_FORCE_INLINE void storeu_linear_all(float *pDst, const vfloat& src)
	{
		_mm_storeu_ps((float*)pDst, src.m_value);
	}

	CPPSPMD_FORCE_INLINE void store_linear_all(float *pDst, const vfloat& src)
	{
		_mm_store_ps((float*)pDst, src.m_value);
	}
		
	CPPSPMD_FORCE_INLINE vfloat loadu_linear(const float *pSrc)
	{
		__m128 v = _mm_loadu_ps((const float*)pSrc);

		v = _mm_and_ps(v, _mm_castsi128_ps(m_exec.m_mask));

		return vfloat{ v };
	}

	CPPSPMD_FORCE_INLINE vfloat loadu_linear_all(const float *pSrc)
	{
		return vfloat{ _mm_loadu_ps((float*)pSrc) };
	}

	CPPSPMD_FORCE_INLINE vfloat load_linear_all(const float *pSrc)
	{
		return vfloat{ _mm_load_ps((float*)pSrc) };
	}
	
	CPPSPMD_FORCE_INLINE vint& store(vint& dst, const vint& src)
	{
		dst.m_value = blendv_mask_epi32(dst.m_value, src.m_value, m_exec.m_mask);
		return dst;
	}

	CPPSPMD_FORCE_INLINE const int_vref& store(const int_vref& dst, const vint& src)
	{
		CPPSPMD_ALIGN(16) int vindex[4];
		_mm_store_si128((__m128i*)vindex, dst.m_vindex);

		CPPSPMD_ALIGN(16) int stored[4];
		_mm_store_si128((__m128i*)stored, src.m_value);

		int mask = _mm_movemask_ps(_mm_castsi128_ps(m_exec.m_mask));
		for (int i = 0; i < 4; i++)
		{
			if (mask & (1 << i))
				dst.m_pValue[vindex[i]] = stored[i];
		}
		return dst;
	}
	
	CPPSPMD_FORCE_INLINE vint& store_all(vint& dst, const vint& src)
	{
		dst.m_value = src.m_value;
		return dst;
	}
				
	CPPSPMD_FORCE_INLINE const int_vref& store_all(const int_vref& dst, const vint& src)
	{
		CPPSPMD_ALIGN(16) int vindex[4];
		_mm_store_si128((__m128i*)vindex, dst.m_vindex);

		CPPSPMD_ALIGN(16) int stored[4];
		_mm_store_si128((__m128i*)stored, src.m_value);

		for (int i = 0; i < 4; i++)
			dst.m_pValue[vindex[i]] = stored[i];

		return dst;
	}

	CPPSPMD_FORCE_INLINE vint load(const int_vref& src)
	{
		CPPSPMD_ALIGN(16) int values[4];

		CPPSPMD_ALIGN(16) int indices[4];
		_mm_store_si128((__m128i *)indices, src.m_vindex);

		int mask = _mm_movemask_ps(_mm_castsi128_ps(m_exec.m_mask));
		for (int i = 0; i < 4; i++)
		{
			if (mask & (1 << i))
				values[i] = src.m_pValue[indices[i]];
		}

		return vint{ _mm_castps_si128(_mm_and_ps(_mm_castsi128_ps(m_exec.m_mask), _mm_load_ps((const float*)values))) };
	}
		
	CPPSPMD_FORCE_INLINE vint load_all(const int_vref& src)
	{
		CPPSPMD_ALIGN(16) int values[4];

		CPPSPMD_ALIGN(16) int indices[4];
		_mm_store_si128((__m128i *)indices, src.m_vindex);

		for (int i = 0; i < 4; i++)
			values[i] = src.m_pValue[indices[i]];

		return vint{ _mm_castps_si128( _mm_load_ps((const float*)values)) };
	}
		
	CPPSPMD_FORCE_INLINE vint load(const cint_vref& src)
	{
		CPPSPMD_ALIGN(16) int values[4];

		CPPSPMD_ALIGN(16) int indices[4];
		_mm_store_si128((__m128i *)indices, src.m_vindex);

		int mask = _mm_movemask_ps(_mm_castsi128_ps(m_exec.m_mask));
		for (int i = 0; i < 4; i++)
		{
			if (mask & (1 << i))
				values[i] = src.m_pValue[indices[i]];
		}

		return vint{ _mm_castps_si128(_mm_and_ps(_mm_castsi128_ps(m_exec.m_mask), _mm_load_ps((const float*)values))) };
	}
		
	CPPSPMD_FORCE_INLINE vint load_all(const cint_vref& src)
	{
		CPPSPMD_ALIGN(16) int values[4];

		CPPSPMD_ALIGN(16) int indices[4];
		_mm_store_si128((__m128i *)indices, src.m_vindex);

		for (int i = 0; i < 4; i++)
			values[i] = src.m_pValue[indices[i]];

		return vint{ _mm_castps_si128( _mm_load_ps((const float*)values)) };
	}

	CPPSPMD_FORCE_INLINE vint load_bytes_all(const cint_vref& src)
	{
		__m128i v0_l;

		const uint8_t* pSrc = (const uint8_t*)src.m_pValue;
		v0_l = insert_x(_mm_undefined_si128(), ((int*)(pSrc + extract_x(src.m_vindex)))[0]);
		v0_l = insert_y(v0_l, ((int*)(pSrc + extract_y(src.m_vindex)))[0]);
		v0_l = insert_z(v0_l, ((int*)(pSrc + extract_z(src.m_vindex)))[0]);
		v0_l = insert_w(v0_l, ((int*)(pSrc + extract_w(src.m_vindex)))[0]);

		return vint{ v0_l };
	}

	CPPSPMD_FORCE_INLINE vint load_words_all(const cint_vref& src)
	{
		__m128i v0_l;

		const uint8_t* pSrc = (const uint8_t*)src.m_pValue;
		v0_l = insert_x(_mm_undefined_si128(), ((int16_t*)(pSrc + 2 * extract_x(src.m_vindex)))[0]);
		v0_l = insert_y(v0_l, ((int16_t*)(pSrc + 2 * extract_y(src.m_vindex)))[0]);
		v0_l = insert_z(v0_l, ((int16_t*)(pSrc + 2 * extract_z(src.m_vindex)))[0]);
		v0_l = insert_w(v0_l, ((int16_t*)(pSrc + 2 * extract_w(src.m_vindex)))[0]);

		return vint{ v0_l };
	}

	CPPSPMD_FORCE_INLINE void store_strided(int *pDst, uint32_t stride, const vint &v)
	{
		int mask = _mm_movemask_ps(_mm_castsi128_ps(m_exec.m_mask));
		
		if (mask & 1) pDst[0] = extract_x(v.m_value);
		if (mask & 2) pDst[stride] = extract_y(v.m_value);
		if (mask & 4) pDst[stride*2] = extract_z(v.m_value);
		if (mask & 8) pDst[stride*3] = extract_w(v.m_value);
	}

	CPPSPMD_FORCE_INLINE void store_strided(float *pDstF, uint32_t stride, const vfloat &v)
	{
		int mask = _mm_movemask_ps(_mm_castsi128_ps(m_exec.m_mask));

		if (mask & 1) ((int *)pDstF)[0] = extract_ps_x(v.m_value);
		if (mask & 2) ((int *)pDstF)[stride] = extract_ps_y(v.m_value);
		if (mask & 4) ((int *)pDstF)[stride*2] = extract_ps_z(v.m_value);
		if (mask & 8) ((int *)pDstF)[stride*3] = extract_ps_w(v.m_value);
	}

	CPPSPMD_FORCE_INLINE void store_all_strided(int *pDst, uint32_t stride, const vint &v)
	{
		pDst[0] = extract_x(v.m_value);
		pDst[stride] = extract_y(v.m_value);
		pDst[stride*2] = extract_z(v.m_value);
		pDst[stride*3] = extract_w(v.m_value);
	}

	CPPSPMD_FORCE_INLINE void store_all_strided(float *pDstF, uint32_t stride, const vfloat &v)
	{
		((int *)pDstF)[0] = extract_ps_x(v.m_value);
		((int *)pDstF)[stride] = extract_ps_y(v.m_value);
		((int *)pDstF)[stride*2] = extract_ps_z(v.m_value);
		((int *)pDstF)[stride*3] = extract_ps_w(v.m_value);
	}

	CPPSPMD_FORCE_INLINE vint load_strided(const int *pSrc, uint32_t stride)
	{
		int mask = _mm_movemask_ps(_mm_castsi128_ps(m_exec.m_mask));
								
#if CPPSPMD_SSE2
		CPPSPMD_ALIGN(16) int vals[4] = { 0, 0, 0, 0 };
		if (mask & 1) vals[0] = pSrc[0];
		if (mask & 2) vals[1] = pSrc[stride];
		if (mask & 4) vals[2] = pSrc[stride * 2];
		if (mask & 8) vals[3] = pSrc[stride * 3];
		return vint{ _mm_load_si128((__m128i*)vals) };
#else
		const float* pSrcF = (const float*)pSrc;
		__m128 v = _mm_setzero_ps();
		if (mask & 1) v = _mm_load_ss(pSrcF);
		if (mask & 2) v = _mm_insert_ps(v, _mm_load_ss(pSrcF + stride), 0x10);
		if (mask & 4) v = _mm_insert_ps(v, _mm_load_ss(pSrcF + 2 * stride), 0x20);
		if (mask & 8) v = _mm_insert_ps(v, _mm_load_ss(pSrcF + 3 * stride), 0x30);
		return vint{ _mm_castps_si128(v) };
#endif
	}

	CPPSPMD_FORCE_INLINE vfloat load_strided(const float *pSrc, uint32_t stride)
	{
		int mask = _mm_movemask_ps(_mm_castsi128_ps(m_exec.m_mask));

#if CPPSPMD_SSE2
		CPPSPMD_ALIGN(16) float vals[4] = { 0, 0, 0, 0 };
		if (mask & 1) vals[0] = pSrc[0];
		if (mask & 2) vals[1] = pSrc[stride];
		if (mask & 4) vals[2] = pSrc[stride * 2];
		if (mask & 8) vals[3] = pSrc[stride * 3];
		return vfloat{ _mm_load_ps(vals) };
#else
		__m128 v = _mm_setzero_ps();
		if (mask & 1) v = _mm_load_ss(pSrc);
		if (mask & 2) v = _mm_insert_ps(v, _mm_load_ss(pSrc + stride), 0x10);
		if (mask & 4) v = _mm_insert_ps(v, _mm_load_ss(pSrc + 2 * stride), 0x20);
		if (mask & 8) v = _mm_insert_ps(v, _mm_load_ss(pSrc + 3 * stride), 0x30);
		return vfloat{ v };
#endif
	}

	CPPSPMD_FORCE_INLINE vint load_all_strided(const int *pSrc, uint32_t stride)
	{
#if CPPSPMD_SSE2
		CPPSPMD_ALIGN(16) int vals[4];
		vals[0] = pSrc[0];
		vals[1] = pSrc[stride];
		vals[2] = pSrc[stride * 2];
		vals[3] = pSrc[stride * 3];
		return vint{ _mm_load_si128((__m128i*)vals) };
#else		
		const float* pSrcF = (const float*)pSrc;
		__m128 v = _mm_load_ss(pSrcF);
		v = _mm_insert_ps(v, _mm_load_ss(pSrcF + stride), 0x10);
		v = _mm_insert_ps(v, _mm_load_ss(pSrcF + 2 * stride), 0x20);
		v = _mm_insert_ps(v, _mm_load_ss(pSrcF + 3 * stride), 0x30);
		return vint{ _mm_castps_si128(v) };
#endif
	}

	CPPSPMD_FORCE_INLINE vfloat load_all_strided(const float *pSrc, uint32_t stride)
	{
#if CPPSPMD_SSE2
		CPPSPMD_ALIGN(16) float vals[4];
		vals[0] = pSrc[0];
		vals[1] = pSrc[stride];
		vals[2] = pSrc[stride * 2];
		vals[3] = pSrc[stride * 3];
		return vfloat{ _mm_load_ps(vals) };
#else
		__m128 v = _mm_load_ss(pSrc);
		v = _mm_insert_ps(v, _mm_load_ss(pSrc + stride), 0x10);
		v = _mm_insert_ps(v, _mm_load_ss(pSrc + 2 * stride), 0x20);
		v = _mm_insert_ps(v, _mm_load_ss(pSrc + 3 * stride), 0x30);
		return vfloat{ v };
#endif
	}

	CPPSPMD_FORCE_INLINE const vfloat_vref& store(const vfloat_vref& dst, const vfloat& src)
	{
		// TODO: There's surely a better way
		int mask = _mm_movemask_ps(_mm_castsi128_ps(m_exec.m_mask));
		
		if (mask & 1) ((int *)(&dst.m_pValue[extract_x(dst.m_vindex)]))[0] = extract_x(_mm_castps_si128(src.m_value));
		if (mask & 2) ((int *)(&dst.m_pValue[extract_y(dst.m_vindex)]))[1] = extract_y(_mm_castps_si128(src.m_value));
		if (mask & 4) ((int *)(&dst.m_pValue[extract_z(dst.m_vindex)]))[2] = extract_z(_mm_castps_si128(src.m_value));
		if (mask & 8) ((int *)(&dst.m_pValue[extract_w(dst.m_vindex)]))[3] = extract_w(_mm_castps_si128(src.m_value));

		return dst;
	}

	CPPSPMD_FORCE_INLINE vfloat load(const vfloat_vref& src)
	{
		// TODO: There's surely a better way
		int mask = _mm_movemask_ps(_mm_castsi128_ps(m_exec.m_mask));

		__m128i k = _mm_setzero_si128();

		if (mask & 1) k = insert_x(k, ((int *)(&src.m_pValue[extract_x(src.m_vindex)]))[0]);
		if (mask & 2) k = insert_y(k, ((int *)(&src.m_pValue[extract_y(src.m_vindex)]))[1]);
		if (mask & 4) k = insert_z(k, ((int *)(&src.m_pValue[extract_z(src.m_vindex)]))[2]);
		if (mask & 8) k = insert_w(k, ((int *)(&src.m_pValue[extract_w(src.m_vindex)]))[3]);

		return vfloat{ _mm_castsi128_ps(k) };
	}

	CPPSPMD_FORCE_INLINE const vint_vref& store(const vint_vref& dst, const vint& src)
	{
		// TODO: There's surely a better way
		int mask = _mm_movemask_ps(_mm_castsi128_ps(m_exec.m_mask));
		
		if (mask & 1) ((int *)(&dst.m_pValue[extract_x(dst.m_vindex)]))[0] = extract_x(src.m_value);
		if (mask & 2) ((int *)(&dst.m_pValue[extract_y(dst.m_vindex)]))[1] = extract_y(src.m_value);
		if (mask & 4) ((int *)(&dst.m_pValue[extract_z(dst.m_vindex)]))[2] = extract_z(src.m_value);
		if (mask & 8) ((int *)(&dst.m_pValue[extract_w(dst.m_vindex)]))[3] = extract_w(src.m_value);

		return dst;
	}

	CPPSPMD_FORCE_INLINE vint load(const vint_vref& src)
	{
		// TODO: There's surely a better way
		int mask = _mm_movemask_ps(_mm_castsi128_ps(m_exec.m_mask));

		__m128i k = _mm_setzero_si128();

		if (mask & 1) k = insert_x(k, ((int *)(&src.m_pValue[extract_x(src.m_vindex)]))[0]);
		if (mask & 2) k = insert_y(k, ((int *)(&src.m_pValue[extract_y(src.m_vindex)]))[1]);
		if (mask & 4) k = insert_z(k, ((int *)(&src.m_pValue[extract_z(src.m_vindex)]))[2]);
		if (mask & 8) k = insert_w(k, ((int *)(&src.m_pValue[extract_w(src.m_vindex)]))[3]);

		return vint{ k };
	}

	CPPSPMD_FORCE_INLINE vint load_all(const vint_vref& src)
	{
		// TODO: There's surely a better way
		__m128i k = _mm_setzero_si128();

		k = insert_x(k, ((int*)(&src.m_pValue[extract_x(src.m_vindex)]))[0]);
		k = insert_y(k, ((int*)(&src.m_pValue[extract_y(src.m_vindex)]))[1]);
		k = insert_z(k, ((int*)(&src.m_pValue[extract_z(src.m_vindex)]))[2]);
		k = insert_w(k, ((int*)(&src.m_pValue[extract_w(src.m_vindex)]))[3]);

		return vint{ k };
	}
			
	// Linear integer
	struct lint
	{
		__m128i m_value;

		CPPSPMD_FORCE_INLINE explicit lint(__m128i value)
			: m_value(value)
		{ }

		CPPSPMD_FORCE_INLINE explicit operator vfloat() const
		{
			return vfloat{ _mm_cvtepi32_ps(m_value) };
		}

		CPPSPMD_FORCE_INLINE explicit operator vint() const
		{
			return vint{ m_value };
		}

		CPPSPMD_FORCE_INLINE int get_first_value() const 
		{
			return _mm_cvtsi128_si32(m_value);
		}

		CPPSPMD_FORCE_INLINE float_lref operator[](float* ptr) const
		{
			return float_lref{ ptr + get_first_value() };
		}

		CPPSPMD_FORCE_INLINE int_lref operator[](int* ptr) const
		{
			return int_lref{ ptr + get_first_value() };
		}

		CPPSPMD_FORCE_INLINE int16_lref operator[](int16_t* ptr) const
		{
			return int16_lref{ ptr + get_first_value() };
		}

		CPPSPMD_FORCE_INLINE cint_lref operator[](const int* ptr) const
		{
			return cint_lref{ ptr + get_first_value() };
		}

	private:
		//lint& operator=(const lint&);
	};

	CPPSPMD_FORCE_INLINE lint& store_all(lint& dst, const lint& src)
	{
		dst.m_value = src.m_value;
		return dst;
	}
	
	const lint program_index = lint{ _mm_set_epi32( 3, 2, 1, 0 ) };
	
	// SPMD condition helpers

	template<typename IfBody>
	CPPSPMD_FORCE_INLINE void spmd_if(const vbool& cond, const IfBody& ifBody);

	CPPSPMD_FORCE_INLINE void spmd_if_break(const vbool& cond);

	// No breaks, continues, etc. allowed
	template<typename IfBody>
	CPPSPMD_FORCE_INLINE void spmd_sif(const vbool& cond, const IfBody& ifBody);

	// No breaks, continues, etc. allowed
	template<typename IfBody, typename ElseBody>
	CPPSPMD_FORCE_INLINE void spmd_sifelse(const vbool& cond, const IfBody& ifBody, const ElseBody &elseBody);

	template<typename IfBody, typename ElseBody>
	CPPSPMD_FORCE_INLINE void spmd_ifelse(const vbool& cond, const IfBody& ifBody, const ElseBody& elseBody);

	template<typename WhileCondBody, typename WhileBody>
	CPPSPMD_FORCE_INLINE void spmd_while(const WhileCondBody& whileCondBody, const WhileBody& whileBody);

	template<typename ForInitBody, typename ForCondBody, typename ForIncrBody, typename ForBody>
	CPPSPMD_FORCE_INLINE void spmd_for(const ForInitBody& forInitBody, const ForCondBody& forCondBody, const ForIncrBody& forIncrBody, const ForBody& forBody);

	template<typename ForeachBody>
	CPPSPMD_FORCE_INLINE void spmd_foreach(int begin, int end, const ForeachBody& foreachBody);
		
#ifdef _DEBUG
	CPPSPMD_FORCE_INLINE void check_masks();
#else
	CPPSPMD_FORCE_INLINE void check_masks() { }
#endif

	CPPSPMD_FORCE_INLINE void spmd_break();
	CPPSPMD_FORCE_INLINE void spmd_continue();
	
	CPPSPMD_FORCE_INLINE void spmd_return();
	
	template<typename UnmaskedBody>
	CPPSPMD_FORCE_INLINE void spmd_unmasked(const UnmaskedBody& unmaskedBody);

	template<typename SPMDKernel, typename... Args>
	//CPPSPMD_FORCE_INLINE decltype(auto) spmd_call(Args&&... args);
	CPPSPMD_FORCE_INLINE void spmd_call(Args&&... args);

	CPPSPMD_FORCE_INLINE void swap(vint &a, vint &b) { vint temp = a; store(a, b); store(b, temp); }
	CPPSPMD_FORCE_INLINE void swap(vfloat &a, vfloat &b) { vfloat temp = a; store(a, b); store(b, temp); }
	CPPSPMD_FORCE_INLINE void swap(vbool &a, vbool &b) { vbool temp = a; store(a, b); store(b, temp); }

	CPPSPMD_FORCE_INLINE float reduce_add(vfloat v)	
	{ 
		__m128 k3210 = _mm_castsi128_ps(blendv_mask_epi32(_mm_setzero_si128(), _mm_castps_si128(v.m_value), m_exec.m_mask));
		__m128 temp = _mm_add_ps(_mm_shuffle_ps(k3210, k3210, _MM_SHUFFLE(0, 1, 2, 3)), k3210);
		return _mm_cvtss_f32(_mm_add_ss(_mm_movehl_ps(temp, temp), temp));
	}

	CPPSPMD_FORCE_INLINE int reduce_add(vint v)
	{
		__m128i k3210 = blendv_mask_epi32(_mm_setzero_si128(), v.m_value, m_exec.m_mask);
		__m128i temp = _mm_add_epi32(_mm_shuffle_epi32(k3210, _MM_SHUFFLE(0, 1, 2, 3)), k3210);
		return extract_x(_mm_add_epi32(_mm_castps_si128(_mm_movehl_ps(_mm_castsi128_ps(temp), _mm_castsi128_ps(temp))), temp));
	}

	#include "cppspmd_math_declares.h"

}; // struct spmd_kernel

using exec_mask = spmd_kernel::exec_mask;
using vint = spmd_kernel::vint;
using int_lref = spmd_kernel::int_lref;
using cint_vref = spmd_kernel::cint_vref;
using cint_lref = spmd_kernel::cint_lref;
using int_vref = spmd_kernel::int_vref;
using lint = spmd_kernel::lint;
using vbool = spmd_kernel::vbool;
using vfloat = spmd_kernel::vfloat;
using float_lref = spmd_kernel::float_lref;
using float_vref = spmd_kernel::float_vref;
using vfloat_vref = spmd_kernel::vfloat_vref;
using vint_vref = spmd_kernel::vint_vref;

CPPSPMD_FORCE_INLINE spmd_kernel::vbool::operator vfloat() const 
{ 
	return vfloat { _mm_and_ps( _mm_castsi128_ps(m_value), *(const __m128 *)g_onef_128 ) }; 
}
	
// Returns UINT32_MAX's for true, 0 for false. (Should it return 1's?)
CPPSPMD_FORCE_INLINE spmd_kernel::vbool::operator vint() const 
{ 
	return vint { m_value };
}

CPPSPMD_FORCE_INLINE vbool operator!(const vbool& v)
{
	return vbool{ _mm_castps_si128(_mm_xor_ps(_mm_load_ps((const float*)g_allones_128), _mm_castsi128_ps(v.m_value))) };
}

CPPSPMD_FORCE_INLINE exec_mask::exec_mask(const vbool& b) { m_mask = b.m_value; }

CPPSPMD_FORCE_INLINE exec_mask operator^(const exec_mask& a, const exec_mask& b) { return exec_mask{ _mm_xor_si128(a.m_mask, b.m_mask) }; }
CPPSPMD_FORCE_INLINE exec_mask operator&(const exec_mask& a, const exec_mask& b) {	return exec_mask{ _mm_and_si128(a.m_mask, b.m_mask) }; }
CPPSPMD_FORCE_INLINE exec_mask operator|(const exec_mask& a, const exec_mask& b) { return exec_mask{ _mm_or_si128(a.m_mask, b.m_mask) }; }

CPPSPMD_FORCE_INLINE bool all(const exec_mask& e) { return _mm_movemask_ps(_mm_castsi128_ps(e.m_mask)) == ALL_ON_MOVEMASK; }
CPPSPMD_FORCE_INLINE bool any(const exec_mask& e) { return _mm_movemask_ps(_mm_castsi128_ps(e.m_mask)) != 0; }

// Bad pattern - doesn't factor in the current exec mask. Prefer spmd_any() instead.
CPPSPMD_FORCE_INLINE bool all(const vbool& e) { return _mm_movemask_ps(_mm_castsi128_ps(e.m_value)) == ALL_ON_MOVEMASK; }
CPPSPMD_FORCE_INLINE bool any(const vbool& e) { return _mm_movemask_ps(_mm_castsi128_ps(e.m_value)) != 0; }

CPPSPMD_FORCE_INLINE exec_mask andnot(const exec_mask& a, const exec_mask& b) { return exec_mask{ _mm_andnot_si128(a.m_mask, b.m_mask) }; }
CPPSPMD_FORCE_INLINE vbool operator||(const vbool& a, const vbool& b) { return vbool{ _mm_or_si128(a.m_value, b.m_value) }; }
CPPSPMD_FORCE_INLINE vbool operator&&(const vbool& a, const vbool& b) { return vbool{ _mm_and_si128(a.m_value, b.m_value) }; }

CPPSPMD_FORCE_INLINE vfloat operator+(const vfloat& a, const vfloat& b) { return vfloat{ _mm_add_ps(a.m_value, b.m_value) }; }
CPPSPMD_FORCE_INLINE vfloat operator-(const vfloat& a, const vfloat& b) {	return vfloat{ _mm_sub_ps(a.m_value, b.m_value) }; }
CPPSPMD_FORCE_INLINE vfloat operator+(float a, const vfloat& b) { return vfloat(a) + b; }
CPPSPMD_FORCE_INLINE vfloat operator+(const vfloat& a, float b) { return a + vfloat(b); }
CPPSPMD_FORCE_INLINE vfloat operator-(const vfloat& a, const vint& b) { return a - vfloat(b); }
CPPSPMD_FORCE_INLINE vfloat operator-(const vint& a, const vfloat& b) { return vfloat(a) - b; }
CPPSPMD_FORCE_INLINE vfloat operator-(const vfloat& a, int b) { return a - vfloat(b); }
CPPSPMD_FORCE_INLINE vfloat operator-(int a, const vfloat& b) { return vfloat(a) - b; }
CPPSPMD_FORCE_INLINE vfloat operator-(const vfloat& a, float b) { return a - vfloat(b); }
CPPSPMD_FORCE_INLINE vfloat operator-(float a, const vfloat& b) { return vfloat(a) - b; }

CPPSPMD_FORCE_INLINE vfloat operator*(const vfloat& a, const vfloat& b) { return vfloat{ _mm_mul_ps(a.m_value, b.m_value) }; }
CPPSPMD_FORCE_INLINE vfloat operator*(const vfloat& a, float b) { return a * vfloat(b); }
CPPSPMD_FORCE_INLINE vfloat operator*(float a, const vfloat& b) { return vfloat(a) * b; }
CPPSPMD_FORCE_INLINE vfloat operator*(const vfloat& a, int b) { return a * vfloat(b); }
CPPSPMD_FORCE_INLINE vfloat operator*(int a, const vfloat& b) { return vfloat(a) * b; }

CPPSPMD_FORCE_INLINE vfloat operator/(const vfloat& a, const vfloat& b) {	return vfloat{ _mm_div_ps(a.m_value, b.m_value) }; }
CPPSPMD_FORCE_INLINE vfloat operator/(const vfloat& a, int b) { return a / vfloat(b); }
CPPSPMD_FORCE_INLINE vfloat operator/(int a, const vfloat& b) { return vfloat(a) / b; }
CPPSPMD_FORCE_INLINE vfloat operator/(const vfloat& a, float b) { return a / vfloat(b); }
CPPSPMD_FORCE_INLINE vfloat operator/(float a, const vfloat& b) { return vfloat(a) / b; }
CPPSPMD_FORCE_INLINE vfloat operator-(const vfloat& v) { return vfloat{ _mm_sub_ps(_mm_xor_ps(v.m_value, v.m_value), v.m_value) }; }

CPPSPMD_FORCE_INLINE vbool operator==(const vfloat& a, const vfloat& b) { return vbool{ _mm_castps_si128(_mm_cmpeq_ps(a.m_value, b.m_value)) }; }
CPPSPMD_FORCE_INLINE vbool operator==(const vfloat& a, float b) { return a == vfloat(b); }

CPPSPMD_FORCE_INLINE vbool operator!=(const vfloat& a, const vfloat& b) { return !vbool{ _mm_castps_si128(_mm_cmpeq_ps(a.m_value, b.m_value)) }; }
CPPSPMD_FORCE_INLINE vbool operator!=(const vfloat& a, float b) { return a != vfloat(b); }

CPPSPMD_FORCE_INLINE vbool operator<(const vfloat& a, const vfloat& b) { return vbool{ _mm_castps_si128(_mm_cmplt_ps(a.m_value, b.m_value)) }; }
CPPSPMD_FORCE_INLINE vbool operator<(const vfloat& a, float b) { return a < vfloat(b); }

CPPSPMD_FORCE_INLINE vbool operator>(const vfloat& a, const vfloat& b) { return vbool{ _mm_castps_si128(_mm_cmpgt_ps(a.m_value, b.m_value)) }; }
CPPSPMD_FORCE_INLINE vbool operator>(const vfloat& a, float b) { return a > vfloat(b); }

CPPSPMD_FORCE_INLINE vbool operator<=(const vfloat& a, const vfloat& b) { return vbool{ _mm_castps_si128(_mm_cmple_ps(a.m_value, b.m_value)) }; }
CPPSPMD_FORCE_INLINE vbool operator<=(const vfloat& a, float b) { return a <= vfloat(b); }

CPPSPMD_FORCE_INLINE vbool operator>=(const vfloat& a, const vfloat& b) { return vbool{ _mm_castps_si128(_mm_cmpge_ps(a.m_value, b.m_value)) }; }
CPPSPMD_FORCE_INLINE vbool operator>=(const vfloat& a, float b) { return a >= vfloat(b); }

CPPSPMD_FORCE_INLINE vfloat spmd_ternaryf(const vbool& cond, const vfloat& a, const vfloat& b) { return vfloat{ blendv_mask_ps(b.m_value, a.m_value, _mm_castsi128_ps(cond.m_value)) }; }
CPPSPMD_FORCE_INLINE vint spmd_ternaryi(const vbool& cond, const vint& a, const vint& b) { return vint{ blendv_mask_epi32(b.m_value, a.m_value, cond.m_value) }; }

CPPSPMD_FORCE_INLINE vfloat sqrt(const vfloat& v) { return vfloat{ _mm_sqrt_ps(v.m_value) }; }
CPPSPMD_FORCE_INLINE vfloat abs(const vfloat& v) { return vfloat{ _mm_andnot_ps(_mm_set1_ps(-0.0f), v.m_value) }; }
CPPSPMD_FORCE_INLINE vfloat max(const vfloat& a, const vfloat& b) { return vfloat{ _mm_max_ps(a.m_value, b.m_value) }; }
CPPSPMD_FORCE_INLINE vfloat min(const vfloat& a, const vfloat& b) {	return vfloat{ _mm_min_ps(a.m_value, b.m_value) }; }

#if CPPSPMD_SSE2
CPPSPMD_FORCE_INLINE vfloat round_truncate(const vfloat& a)
{
	__m128i abs_a = _mm_and_si128(_mm_castps_si128(a.m_value), _mm_set1_epi32(0x7FFFFFFFU) );
	__m128i has_fractional = _mm_cmplt_epi32(abs_a, _mm_castps_si128(_mm_set1_ps(8388608.0f)));
		
	__m128i ai = _mm_cvttps_epi32(a.m_value);
	
	__m128 af = _mm_cvtepi32_ps(ai);
	return vfloat{ blendv_mask_ps(a.m_value, af, _mm_castsi128_ps(has_fractional)) };
}

CPPSPMD_FORCE_INLINE vfloat floor(const vfloat& a)
{
	__m128i abs_a = _mm_and_si128(_mm_castps_si128(a.m_value), _mm_set1_epi32(0x7FFFFFFFU));
	__m128i has_fractional = _mm_cmplt_epi32(abs_a, _mm_castps_si128(_mm_set1_ps(8388608.0f)));

	__m128i ai = _mm_cvtps_epi32(a.m_value);
	__m128 af = _mm_cvtepi32_ps(ai);
	__m128 changed = _mm_cvtepi32_ps(_mm_castps_si128(_mm_cmpgt_ps(af, a.m_value)));

	af = _mm_add_ps(af, changed);

	return vfloat{ blendv_mask_ps(a.m_value, af, _mm_castsi128_ps(has_fractional)) };
}

CPPSPMD_FORCE_INLINE vfloat ceil(const vfloat& a)
{
	__m128i abs_a = _mm_and_si128(_mm_castps_si128(a.m_value), _mm_set1_epi32(0x7FFFFFFFU));
	__m128i has_fractional = _mm_cmplt_epi32(abs_a, _mm_castps_si128(_mm_set1_ps(8388608.0f)));
	
	__m128i ai = _mm_cvtps_epi32(a.m_value);
	__m128 af = _mm_cvtepi32_ps(ai);
	__m128 changed = _mm_cvtepi32_ps(_mm_castps_si128(_mm_cmplt_ps(af, a.m_value)));
	
	af = _mm_sub_ps(af, changed);

	return vfloat{ blendv_mask_ps(a.m_value, af, _mm_castsi128_ps(has_fractional)) };
}

// We need to disable unsafe math optimizations for the key operations used for rounding to nearest.
// I wish there was a better way.
#if defined(__GNUC__) && !defined(__INTEL_COMPILER) && !defined(__clang__)
inline __m128 add_sub(__m128 a, __m128 b) __attribute__((optimize("-fno-unsafe-math-optimizations")))
#elif defined(__clang__)
inline __m128 add_sub(__m128 a, __m128 b) __attribute__((optnone))
#elif defined (_MSC_VER)
#pragma float_control(push)
#pragma float_control(precise, on)
inline __m128 add_sub(__m128 a, __m128 b)
#else
inline __m128 add_sub(__m128 a, __m128 b)
#endif
{
	return _mm_sub_ps(_mm_add_ps(a, b), b);
}

#if defined (_MSC_VER)
#pragma float_control(pop)
#endif

CPPSPMD_FORCE_INLINE vfloat round_nearest(const vfloat& a)
{
	__m128i no_fract_fp_bits = _mm_castps_si128(_mm_set1_ps(8388608.0f));

	__m128i sign_a = _mm_and_si128(_mm_castps_si128(a.m_value), _mm_set1_epi32(0x80000000U));
	__m128 force_int = _mm_castsi128_ps(_mm_or_si128(no_fract_fp_bits, sign_a));
	
	// Can't use individual _mm_add_ps/_mm_sub_ps - this will be optimized out with /fp:fast by clang and probably other compilers.
	//__m128 temp1 = _mm_add_ps(a.m_value, force_int);
	//__m128 temp2 = _mm_sub_ps(temp1, force_int);
	__m128 temp2 = add_sub(a.m_value, force_int);
	
	__m128i abs_a = _mm_and_si128(_mm_castps_si128(a.m_value), _mm_set1_epi32(0x7FFFFFFFU));
	__m128i has_fractional = _mm_cmplt_epi32(abs_a, no_fract_fp_bits);
	return vfloat{ blendv_mask_ps(a.m_value, temp2, _mm_castsi128_ps(has_fractional)) };
}

#else
CPPSPMD_FORCE_INLINE vfloat floor(const vfloat& v) { return vfloat{ _mm_floor_ps(v.m_value) }; }
CPPSPMD_FORCE_INLINE vfloat ceil(const vfloat& a) { return vfloat{ _mm_ceil_ps(a.m_value) }; }
CPPSPMD_FORCE_INLINE vfloat round_nearest(const vfloat &a) { return vfloat{ _mm_round_ps(a.m_value, _MM_FROUND_TO_NEAREST_INT | _MM_FROUND_NO_EXC ) }; }
CPPSPMD_FORCE_INLINE vfloat round_truncate(const vfloat &a) { return vfloat{ _mm_round_ps(a.m_value, _MM_FROUND_TO_ZERO | _MM_FROUND_NO_EXC ) }; }
#endif

CPPSPMD_FORCE_INLINE vfloat frac(const vfloat& a) { return a - floor(a); }
CPPSPMD_FORCE_INLINE vfloat fmod(vfloat a, vfloat b) { vfloat c = frac(abs(a / b)) * abs(b); return spmd_ternaryf(a < 0, -c, c); }
CPPSPMD_FORCE_INLINE vfloat sign(const vfloat& a) { return spmd_ternaryf(a < 0.0f, 1.0f, 1.0f); }

CPPSPMD_FORCE_INLINE vint max(const vint& a, const vint& b) { return vint{ max_epi32(a.m_value, b.m_value) }; }
CPPSPMD_FORCE_INLINE vint min(const vint& a, const vint& b) {	return vint{ min_epi32(a.m_value, b.m_value) }; }

CPPSPMD_FORCE_INLINE vint maxu(const vint& a, const vint& b) { return vint{ max_epu32(a.m_value, b.m_value) }; }
CPPSPMD_FORCE_INLINE vint minu(const vint& a, const vint& b) { return vint{ min_epu32(a.m_value, b.m_value) }; }

CPPSPMD_FORCE_INLINE vint abs(const vint& v) { return vint{ abs_epi32(v.m_value) }; }

CPPSPMD_FORCE_INLINE vint byteswap(const vint& v) {	return vint{ shuffle_epi8(v.m_value, _mm_set_epi8(12, 13, 14, 15,  8,  9, 10, 11,  4,  5,  6,  7,  0,  1,  2,  3)) }; }

CPPSPMD_FORCE_INLINE vint cast_vfloat_to_vint(const vfloat& v) { return vint{ _mm_castps_si128(v.m_value) }; }
CPPSPMD_FORCE_INLINE vfloat cast_vint_to_vfloat(const vint& v) { return vfloat{ _mm_castsi128_ps(v.m_value) }; }

CPPSPMD_FORCE_INLINE vfloat clamp(const vfloat& v, const vfloat& a, const vfloat& b)
{
	return vfloat{ _mm_min_ps(b.m_value, _mm_max_ps(v.m_value, a.m_value) ) };
}

CPPSPMD_FORCE_INLINE vint clamp(const vint& v, const vint& a, const vint& b)
{
	return vint{ min_epi32(b.m_value, max_epi32(v.m_value, a.m_value) ) };
}

CPPSPMD_FORCE_INLINE vfloat vfma(const vfloat& a, const vfloat& b, const vfloat& c)
{
	return vfloat{ _mm_add_ps(_mm_mul_ps(a.m_value, b.m_value), c.m_value) };
}

CPPSPMD_FORCE_INLINE vfloat vfms(const vfloat& a, const vfloat& b, const vfloat& c)
{
	return vfloat{ _mm_sub_ps(_mm_mul_ps(a.m_value, b.m_value), c.m_value) };
}

CPPSPMD_FORCE_INLINE vfloat vfnma(const vfloat& a, const vfloat& b, const vfloat& c)
{
	return vfloat{ _mm_sub_ps(c.m_value, _mm_mul_ps(a.m_value, b.m_value)) };
}

CPPSPMD_FORCE_INLINE vfloat vfnms(const vfloat& a, const vfloat& b, const vfloat& c)
{
	return vfloat{ _mm_sub_ps(_mm_sub_ps(_mm_xor_ps(a.m_value, a.m_value), _mm_mul_ps(a.m_value, b.m_value)), c.m_value) };
}

CPPSPMD_FORCE_INLINE vfloat lerp(const vfloat &x, const vfloat &y, const vfloat &s) { return vfma(y - x, s, x); }

CPPSPMD_FORCE_INLINE lint operator+(int a, const lint& b) { return lint{ _mm_add_epi32(_mm_set1_epi32(a), b.m_value) }; }
CPPSPMD_FORCE_INLINE lint operator+(const lint& a, int b) { return lint{ _mm_add_epi32(a.m_value, _mm_set1_epi32(b)) }; }
CPPSPMD_FORCE_INLINE vfloat operator+(float a, const lint& b) { return vfloat(a) + vfloat(b); }
CPPSPMD_FORCE_INLINE vfloat operator+(const lint& a, float b) { return vfloat(a) + vfloat(b); }
CPPSPMD_FORCE_INLINE vfloat operator*(const lint& a, float b) { return vfloat(a) * vfloat(b); }
CPPSPMD_FORCE_INLINE vfloat operator*(float b, const lint& a) { return vfloat(a) * vfloat(b); }

CPPSPMD_FORCE_INLINE vint operator&(const vint& a, const vint& b) { return vint{ _mm_and_si128(a.m_value, b.m_value) }; }
CPPSPMD_FORCE_INLINE vint operator&(const vint& a, int b) { return a & vint(b); }
CPPSPMD_FORCE_INLINE vint andnot(const vint& a, const vint& b) { return vint{ _mm_andnot_si128(a.m_value, b.m_value) }; }
CPPSPMD_FORCE_INLINE vint operator|(const vint& a, const vint& b) { return vint{ _mm_or_si128(a.m_value, b.m_value) }; }
CPPSPMD_FORCE_INLINE vint operator|(const vint& a, int b) { return a | vint(b); }
CPPSPMD_FORCE_INLINE vint operator^(const vint& a, const vint& b) { return vint{ _mm_xor_si128(a.m_value, b.m_value) }; }
CPPSPMD_FORCE_INLINE vint operator^(const vint& a, int b) { return a ^ vint(b); }
CPPSPMD_FORCE_INLINE vbool operator==(const vint& a, const vint& b) { return vbool{ _mm_cmpeq_epi32(a.m_value, b.m_value) }; }
CPPSPMD_FORCE_INLINE vbool operator!=(const vint& a, const vint& b) { return !vbool{ _mm_cmpeq_epi32(a.m_value, b.m_value) }; }
CPPSPMD_FORCE_INLINE vbool operator<(const vint& a, const vint& b) { return vbool{ _mm_cmpgt_epi32(b.m_value, a.m_value) }; }
CPPSPMD_FORCE_INLINE vbool operator<=(const vint& a, const vint& b) { return !vbool{ _mm_cmpgt_epi32(a.m_value, b.m_value) }; }
CPPSPMD_FORCE_INLINE vbool operator>=(const vint& a, const vint& b) { return !vbool{ _mm_cmpgt_epi32(b.m_value, a.m_value) }; }
CPPSPMD_FORCE_INLINE vbool operator>(const vint& a, const vint& b) { return vbool{ _mm_cmpgt_epi32(a.m_value, b.m_value) }; }
CPPSPMD_FORCE_INLINE vint operator+(const vint& a, const vint& b) { return vint{ _mm_add_epi32(a.m_value, b.m_value) }; }
CPPSPMD_FORCE_INLINE vint operator-(const vint& a, const vint& b) { return vint{ _mm_sub_epi32(a.m_value, b.m_value) }; }
CPPSPMD_FORCE_INLINE vint operator+(const vint& a, int b) { return a + vint(b); }
CPPSPMD_FORCE_INLINE vint operator-(const vint& a, int b) { return a - vint(b); }
CPPSPMD_FORCE_INLINE vint operator+(int a, const vint& b) { return vint(a) + b; }
CPPSPMD_FORCE_INLINE vint operator-(int a, const vint& b) { return vint(a) - b; }
CPPSPMD_FORCE_INLINE vint operator*(const vint& a, const vint& b) { return vint{ mullo_epi32(a.m_value, b.m_value) }; }
CPPSPMD_FORCE_INLINE vint operator*(const vint& a, int b) { return a * vint(b); }
CPPSPMD_FORCE_INLINE vint operator*(int a, const vint& b) { return vint(a) * b; }

CPPSPMD_FORCE_INLINE vint mulhiu(const vint& a, const vint& b) { return vint{ mulhi_epu32(a.m_value, b.m_value) }; }

CPPSPMD_FORCE_INLINE vint operator-(const vint& v) { return vint{ _mm_sub_epi32(_mm_setzero_si128(), v.m_value) }; }

CPPSPMD_FORCE_INLINE vint operator~(const vint& a) { return vint{ -a - 1 }; }

// A few of these break the lane-based abstraction model. They are supported in SSE2, so it makes sense to support them and let the user figure it out.
CPPSPMD_FORCE_INLINE vint adds_epu8(const vint& a, const vint& b) {	return vint{ _mm_adds_epu8(a.m_value, b.m_value) }; }
CPPSPMD_FORCE_INLINE vint subs_epu8(const vint& a, const vint& b) { return vint{ _mm_subs_epu8(a.m_value, b.m_value) }; }
CPPSPMD_FORCE_INLINE vint avg_epu8(const vint & a, const vint & b) { return vint{ _mm_avg_epu8(a.m_value, b.m_value) }; }
CPPSPMD_FORCE_INLINE vint max_epu8(const vint& a, const vint& b) { return vint{ _mm_max_epu8(a.m_value, b.m_value) }; }
CPPSPMD_FORCE_INLINE vint min_epu8(const vint& a, const vint& b) { return vint{ _mm_min_epu8(a.m_value, b.m_value) }; }
CPPSPMD_FORCE_INLINE vint sad_epu8(const vint& a, const vint& b) { return vint{ _mm_sad_epu8(a.m_value, b.m_value) }; }

CPPSPMD_FORCE_INLINE vint add_epi8(const vint& a, const vint& b) { return vint{ _mm_add_epi8(a.m_value, b.m_value) }; }
CPPSPMD_FORCE_INLINE vint adds_epi8(const vint& a, const vint& b) { return vint{ _mm_adds_epi8(a.m_value, b.m_value) }; }
CPPSPMD_FORCE_INLINE vint sub_epi8(const vint& a, const vint& b) { return vint{ _mm_sub_epi8(a.m_value, b.m_value) }; }
CPPSPMD_FORCE_INLINE vint subs_epi8(const vint& a, const vint& b) { return vint{ _mm_subs_epi8(a.m_value, b.m_value) }; }
CPPSPMD_FORCE_INLINE vint cmpeq_epi8(const vint& a, const vint& b) { return vint{ _mm_cmpeq_epi8(a.m_value, b.m_value) }; }
CPPSPMD_FORCE_INLINE vint cmpgt_epi8(const vint& a, const vint& b) { return vint{ _mm_cmpgt_epi8(a.m_value, b.m_value) }; }
CPPSPMD_FORCE_INLINE vint cmplt_epi8(const vint& a, const vint& b) { return vint{ _mm_cmplt_epi8(a.m_value, b.m_value) }; }
CPPSPMD_FORCE_INLINE vint unpacklo_epi8(const vint& a, const vint& b) { return vint{ _mm_unpacklo_epi8(a.m_value, b.m_value) }; }
CPPSPMD_FORCE_INLINE vint unpackhi_epi8(const vint& a, const vint& b) { return vint{ _mm_unpackhi_epi8(a.m_value, b.m_value) }; }
CPPSPMD_FORCE_INLINE int movemask_epi8(const vint& a) { return _mm_movemask_epi8(a.m_value); }
CPPSPMD_FORCE_INLINE int movemask_epi32(const vint& a) { return _mm_movemask_ps(_mm_castsi128_ps(a.m_value)); }

CPPSPMD_FORCE_INLINE vint cmple_epu8(const vint& a, const vint& b) { return vint{ _mm_cmpeq_epi8(_mm_min_epu8(a.m_value, b.m_value), a.m_value) }; }
CPPSPMD_FORCE_INLINE vint cmpge_epu8(const vint& a, const vint& b) { return vint{ cmple_epu8(b, a) }; }
CPPSPMD_FORCE_INLINE vint cmpgt_epu8(const vint& a, const vint& b) { return vint{ _mm_andnot_si128(_mm_cmpeq_epi8(a.m_value, b.m_value), _mm_cmpeq_epi8(_mm_max_epu8(a.m_value, b.m_value), a.m_value)) }; }
CPPSPMD_FORCE_INLINE vint cmplt_epu8(const vint& a, const vint& b) { return vint{ cmpgt_epu8(b, a) }; }
CPPSPMD_FORCE_INLINE vint absdiff_epu8(const vint& a, const vint& b) { return vint{ _mm_or_si128(_mm_subs_epu8(a.m_value, b.m_value), _mm_subs_epu8(b.m_value, a.m_value)) }; }

CPPSPMD_FORCE_INLINE vint blendv_epi8(const vint& a, const vint& b, const vint &mask) { return vint{ blendv_epi8(a.m_value, b.m_value, _mm_cmplt_epi8(mask.m_value, _mm_setzero_si128())) }; }
CPPSPMD_FORCE_INLINE vint blendv_epi32(const vint& a, const vint& b, const vint &mask) { return vint{ blendv_epi32(a.m_value, b.m_value, mask.m_value) }; }

CPPSPMD_FORCE_INLINE vint add_epi16(const vint& a, const vint& b) { return vint{ _mm_add_epi16(a.m_value, b.m_value) }; }
CPPSPMD_FORCE_INLINE vint adds_epi16(const vint& a, const vint& b) { return vint{ _mm_adds_epi16(a.m_value, b.m_value) }; }
CPPSPMD_FORCE_INLINE vint adds_epu16(const vint& a, const vint& b) { return vint{ _mm_adds_epu16(a.m_value, b.m_value) }; }
CPPSPMD_FORCE_INLINE vint avg_epu16(const vint& a, const vint& b) { return vint{ _mm_avg_epu16(a.m_value, b.m_value) }; }
CPPSPMD_FORCE_INLINE vint sub_epi16(const vint& a, const vint& b) { return vint{ _mm_sub_epi16(a.m_value, b.m_value) }; }
CPPSPMD_FORCE_INLINE vint subs_epi16(const vint& a, const vint& b) { return vint{ _mm_subs_epi16(a.m_value, b.m_value) }; }
CPPSPMD_FORCE_INLINE vint subs_epu16(const vint& a, const vint& b) { return vint{ _mm_subs_epu16(a.m_value, b.m_value) }; }
CPPSPMD_FORCE_INLINE vint mullo_epi16(const vint& a, const vint& b) { return vint{ _mm_mullo_epi16(a.m_value, b.m_value) }; }
CPPSPMD_FORCE_INLINE vint mulhi_epi16(const vint& a, const vint& b) { return vint{ _mm_mulhi_epi16(a.m_value, b.m_value) }; }
CPPSPMD_FORCE_INLINE vint mulhi_epu16(const vint& a, const vint& b) { return vint{ _mm_mulhi_epu16(a.m_value, b.m_value) }; }
CPPSPMD_FORCE_INLINE vint min_epi16(const vint& a, const vint& b) { return vint{ _mm_min_epi16(a.m_value, b.m_value) }; }
CPPSPMD_FORCE_INLINE vint max_epi16(const vint& a, const vint& b) { return vint{ _mm_max_epi16(a.m_value, b.m_value) }; }
CPPSPMD_FORCE_INLINE vint madd_epi16(const vint& a, const vint& b) { return vint{ _mm_madd_epi16(a.m_value, b.m_value) }; }
CPPSPMD_FORCE_INLINE vint cmpeq_epi16(const vint& a, const vint& b) { return vint{ _mm_cmpeq_epi16(a.m_value, b.m_value) }; }
CPPSPMD_FORCE_INLINE vint cmpgt_epi16(const vint& a, const vint& b) { return vint{ _mm_cmpgt_epi16(a.m_value, b.m_value) }; }
CPPSPMD_FORCE_INLINE vint cmplt_epi16(const vint& a, const vint& b) { return vint{ _mm_cmplt_epi16(a.m_value, b.m_value) }; }
CPPSPMD_FORCE_INLINE vint packs_epi16(const vint& a, const vint& b) { return vint{ _mm_packs_epi16(a.m_value, b.m_value) }; }
CPPSPMD_FORCE_INLINE vint packus_epi16(const vint& a, const vint& b) { return vint{ _mm_packus_epi16(a.m_value, b.m_value) }; }

CPPSPMD_FORCE_INLINE vint uniform_shift_left_epi16(const vint& a, const vint& b) { return vint{ _mm_sll_epi16(a.m_value, b.m_value) }; }
CPPSPMD_FORCE_INLINE vint uniform_arith_shift_right_epi16(const vint& a, const vint& b) { return vint{ _mm_sra_epi16(a.m_value, b.m_value) }; }
CPPSPMD_FORCE_INLINE vint uniform_shift_right_epi16(const vint& a, const vint& b) { return vint{ _mm_srl_epi16(a.m_value, b.m_value) }; }

#define VINT_SHIFT_LEFT_EPI16(a, b) vint(_mm_slli_epi16((a).m_value, b))
#define VINT_SHIFT_RIGHT_EPI16(a, b) vint(_mm_srai_epi16((a).m_value, b))
#define VUINT_SHIFT_RIGHT_EPI16(a, b) vint(_mm_srli_epi16((a).m_value, b))

CPPSPMD_FORCE_INLINE vint undefined_vint() { return vint{ _mm_undefined_si128() }; }
CPPSPMD_FORCE_INLINE vfloat undefined_vfloat() { return vfloat{ _mm_undefined_ps() }; }

CPPSPMD_FORCE_INLINE vint zero_vint() { return vint{ _mm_setzero_si128() }; }
CPPSPMD_FORCE_INLINE vfloat zero_vfloat() { return vfloat{ _mm_setzero_ps() }; }

CPPSPMD_FORCE_INLINE vint vint_lane_set(int v0, int v1, int v2, int v3) { return vint{ _mm_set_epi32(v3, v2, v1, v0) }; }
CPPSPMD_FORCE_INLINE vfloat vfloat_lane_set(float v0, float v1, float v2, float v3) { return vfloat{ _mm_set_ps(v3, v2, v1, v0) }; }
CPPSPMD_FORCE_INLINE vint vint_lane_set_r(int v3, int v2, int v1, int v0) { return vint{ _mm_set_epi32(v3, v2, v1, v0) }; }
CPPSPMD_FORCE_INLINE vfloat vfloat_lane_set_r(float v3, float v2, float v1, float v0) { return vfloat{ _mm_set_ps(v3, v2, v1, v0) }; }
// control is an 8-bit immediate value containing 4 2-bit indices which shuffles the int32's in each 128-bit lane.
#define VINT_LANE_SHUFFLE_EPI32(a, control) vint(_mm_shuffle_epi32((a).m_value, control))
#define VFLOAT_LANE_SHUFFLE_PS(a, b, control) vfloat(_mm_shuffle_ps((a).m_value, (b).m_value, control))

// control is an 8-bit immediate value containing 4 2-bit indices which shuffles the int16's in either the high or low 64-bit lane.
#define VINT_LANE_SHUFFLELO_EPI16(a, control) vint(_mm_shufflelo_epi16((a).m_value, control))
#define VINT_LANE_SHUFFLEHI_EPI16(a, control) vint(_mm_shufflehi_epi16((a).m_value, control))

#define VINT_LANE_SHUFFLE_MASK(a, b, c, d) ((a) | ((b) << 2) | ((c) << 4) | ((d) << 6))
#define VINT_LANE_SHUFFLE_MASK_R(d, c, b, a) ((a) | ((b) << 2) | ((c) << 4) | ((d) << 6))

#define VINT_LANE_SHIFT_LEFT_BYTES(a, l) vint(_mm_slli_si128((a).m_value, l))
#define VINT_LANE_SHIFT_RIGHT_BYTES(a, l) vint(_mm_srli_si128((a).m_value, l))

// Unpack and interleave 8-bit integers from the low or high half of a and b
CPPSPMD_FORCE_INLINE vint vint_lane_unpacklo_epi8(const vint& a, const vint& b) { return vint(_mm_unpacklo_epi8(a.m_value, b.m_value)); }
CPPSPMD_FORCE_INLINE vint vint_lane_unpackhi_epi8(const vint& a, const vint& b) { return vint(_mm_unpackhi_epi8(a.m_value, b.m_value)); }

// Unpack and interleave 16-bit integers from the low or high half of a and b
CPPSPMD_FORCE_INLINE vint vint_lane_unpacklo_epi16(const vint& a, const vint& b) { return vint(_mm_unpacklo_epi16(a.m_value, b.m_value)); }
CPPSPMD_FORCE_INLINE vint vint_lane_unpackhi_epi16(const vint& a, const vint& b) { return vint(_mm_unpackhi_epi16(a.m_value, b.m_value)); }

// Unpack and interleave 32-bit integers from the low or high half of a and b
CPPSPMD_FORCE_INLINE vint vint_lane_unpacklo_epi32(const vint& a, const vint& b) { return vint(_mm_unpacklo_epi32(a.m_value, b.m_value)); }
CPPSPMD_FORCE_INLINE vint vint_lane_unpackhi_epi32(const vint& a, const vint& b) { return vint(_mm_unpackhi_epi32(a.m_value, b.m_value)); }

// Unpack and interleave 64-bit integers from the low or high half of a and b
CPPSPMD_FORCE_INLINE vint vint_lane_unpacklo_epi64(const vint& a, const vint& b) { return vint(_mm_unpacklo_epi64(a.m_value, b.m_value)); }
CPPSPMD_FORCE_INLINE vint vint_lane_unpackhi_epi64(const vint& a, const vint& b) { return vint(_mm_unpackhi_epi64(a.m_value, b.m_value)); }

CPPSPMD_FORCE_INLINE vint vint_set1_epi8(int8_t a) { return vint(_mm_set1_epi8(a)); }
CPPSPMD_FORCE_INLINE vint vint_set1_epi16(int16_t a) { return vint(_mm_set1_epi16(a)); }
CPPSPMD_FORCE_INLINE vint vint_set1_epi32(int32_t a) { return vint(_mm_set1_epi32(a)); }
CPPSPMD_FORCE_INLINE vint vint_set1_epi64(int64_t a) { return vint(_mm_set1_epi64x(a)); }

CPPSPMD_FORCE_INLINE vint mul_epu32(const vint &a, const vint& b) { return vint(_mm_mul_epu32(a.m_value, b.m_value)); }

CPPSPMD_FORCE_INLINE vint div_epi32(const vint &a, const vint& b)
{
	__m128d al = _mm_cvtepi32_pd(a.m_value);
	__m128d ah = _mm_cvtepi32_pd(_mm_unpackhi_epi64(a.m_value, a.m_value));

	__m128d bl = _mm_cvtepi32_pd(b.m_value);
	__m128d bh = _mm_cvtepi32_pd(_mm_unpackhi_epi64(b.m_value, b.m_value));

	__m128d rl = _mm_div_pd(al, bl);
	__m128d rh = _mm_div_pd(ah, bh);

	__m128i rli = _mm_cvttpd_epi32(rl);
	__m128i rhi = _mm_cvttpd_epi32(rh);

	return vint(_mm_unpacklo_epi64(rli, rhi));
}

CPPSPMD_FORCE_INLINE vint mod_epi32(const vint &a, const vint& b)
{
	vint aa = abs(a), ab = abs(b);
	vint q = div_epi32(aa, ab);
	vint r = aa - q * ab;
	return spmd_ternaryi(a < 0, -r, r);
}

CPPSPMD_FORCE_INLINE vint operator/ (const vint& a, const vint& b)
{
	return div_epi32(a, b);
}

CPPSPMD_FORCE_INLINE vint operator/ (const vint& a, int b)
{
	return div_epi32(a, vint(b));
}

CPPSPMD_FORCE_INLINE vint operator% (const vint& a, const vint& b)
{
	return mod_epi32(a, b);
}

CPPSPMD_FORCE_INLINE vint operator% (const vint& a, int b)
{
	return mod_epi32(a, vint(b));
}

CPPSPMD_FORCE_INLINE vint operator<< (const vint& a, const vint& b)
{
#if 0
	CPPSPMD_ALIGN(32) int result[4];
	result[0] = extract_x(a.m_value) << extract_x(b.m_value);
	result[1] = extract_y(a.m_value) << extract_y(b.m_value);
	result[2] = extract_z(a.m_value) << extract_z(b.m_value);
	result[3] = extract_w(a.m_value) << extract_w(b.m_value);

	return vint{ _mm_load_si128((__m128i*)result) };
#elif 0
	int x = extract_x(a.m_value) << extract_x(b.m_value);
	int y = extract_y(a.m_value) << extract_y(b.m_value);
	int z = extract_z(a.m_value) << extract_z(b.m_value);
	int w = extract_w(a.m_value) << extract_w(b.m_value);

	__m128i v = insert_x(_mm_undefined_si128(), x);
	v = insert_y(v, y);
	v = insert_z(v, z);
	return vint{ insert_w(v, w) };
#else
	// What this does: shift left each b lane by 23 bits (to move the shift amount into the FP exponent position), then epi32 add to the integer rep of 1.0f, then cast that to float, then convert that to int to get fast 2^x.
	return a * vint(cast_vint_to_vfloat(vint(_mm_slli_epi32(b.m_value, 23)) + cast_vfloat_to_vint(vfloat(1.0f))));
#endif
}

// uniform shift left
CPPSPMD_FORCE_INLINE vint operator<< (const vint& a, int b)
{
	__m128i bv = _mm_castps_si128(_mm_and_ps(_mm_castsi128_ps(_mm_set1_epi32(b)), _mm_castsi128_ps(_mm_load_si128((const __m128i *)g_x_128))));
	return vint{ _mm_sll_epi32(a.m_value, bv) };
}

// uniform arithmetic shift right
CPPSPMD_FORCE_INLINE vint operator>> (const vint& a, int b)
{
	__m128i bv = _mm_castps_si128(_mm_and_ps(_mm_castsi128_ps(_mm_set1_epi32(b)), _mm_castsi128_ps(_mm_load_si128((const __m128i *)g_x_128))));
	return vint{ _mm_sra_epi32(a.m_value, bv) };
}

// uniform shift right
CPPSPMD_FORCE_INLINE vint vuint_shift_right(const vint& a, int b)
{
	__m128i bv = _mm_castps_si128(_mm_and_ps(_mm_castsi128_ps(_mm_set1_epi32(b)), _mm_castsi128_ps(_mm_load_si128((const __m128i *)g_x_128))));
	return vint{ _mm_srl_epi32(a.m_value, bv) };
}

CPPSPMD_FORCE_INLINE vint vuint_shift_right(const vint& a, const vint& b)
{
#if 0
	CPPSPMD_ALIGN(32) int result[4];
	result[0] = ((uint32_t)extract_x(a.m_value)) >> extract_x(b.m_value);
	result[1] = ((uint32_t)extract_y(a.m_value)) >> extract_y(b.m_value);
	result[2] = ((uint32_t)extract_z(a.m_value)) >> extract_z(b.m_value);
	result[3] = ((uint32_t)extract_w(a.m_value)) >> extract_w(b.m_value);

	return vint{ _mm_load_si128((__m128i*)result) };
#elif 0
	uint32_t x = ((uint32_t)extract_x(a.m_value)) >> ((uint32_t)extract_x(b.m_value));
	uint32_t y = ((uint32_t)extract_y(a.m_value)) >> ((uint32_t)extract_y(b.m_value));
	uint32_t z = ((uint32_t)extract_z(a.m_value)) >> ((uint32_t)extract_z(b.m_value));
	uint32_t w = ((uint32_t)extract_w(a.m_value)) >> ((uint32_t)extract_w(b.m_value));

	__m128i v = insert_x(_mm_undefined_si128(), x);
	v = insert_y(v, y);
	v = insert_z(v, z);
	return vint{ insert_w(v, w) };
#else
	//vint inv_shift = 32 - b;
	//vfloat f = cast_vint_to_vfloat(vint(_mm_slli_epi32(inv_shift.m_value, 23)) + cast_vfloat_to_vint(vfloat(1.0f)));
	
	// Take float rep of 1.0f (0x3f800000), subtract (32<<23), subtract (shift<<23), cast to float.
	vfloat f = cast_vint_to_vfloat(vint(_mm_sub_epi32(_mm_set1_epi32(0x4f800000), _mm_slli_epi32(b.m_value, 23))));

	// Now convert scale factor to integer.
	vint r = vint(f);

	// mulhi_epu32 (using two _mm_mul_epu32), to emulate varying shift left.
	vint q(mulhi_epu32(a.m_value, r.m_value));

	// Handle shift amounts of 0.
	return spmd_ternaryi(b > 0, q, a);
#endif
}

CPPSPMD_FORCE_INLINE vint vuint_shift_right_not_zero(const vint& a, const vint& b)
{
	//vint inv_shift = 32 - b;
	//vfloat f = cast_vint_to_vfloat(vint(_mm_slli_epi32(inv_shift.m_value, 23)) + cast_vfloat_to_vint(vfloat(1.0f)));
	
	// Take float rep of 1.0f (0x3f800000), subtract (32<<23), subtract (shift<<23), cast to float.
	vfloat f = cast_vint_to_vfloat(vint(_mm_sub_epi32(_mm_set1_epi32(0x4f800000), _mm_slli_epi32(b.m_value, 23))));

	// Now convert scale factor to integer.
	vint r = vint(f);

	// mulhi_epu32 (using two _mm_mul_epu32), to emulate varying shift left.
	return vint(mulhi_epu32(a.m_value, r.m_value));
}

CPPSPMD_FORCE_INLINE vint operator>> (const vint& a, const vint& b)
{
#if 0
	CPPSPMD_ALIGN(32) int result[4];
	result[0] = extract_x(a.m_value) >> extract_x(b.m_value);
	result[1] = extract_y(a.m_value) >> extract_y(b.m_value);
	result[2] = extract_z(a.m_value) >> extract_z(b.m_value);
	result[3] = extract_w(a.m_value) >> extract_w(b.m_value);

	return vint{ _mm_load_si128((__m128i*)result) };
#elif 0
	int x = extract_x(a.m_value) >> extract_x(b.m_value);
	int y = extract_y(a.m_value) >> extract_y(b.m_value);
	int z = extract_z(a.m_value) >> extract_z(b.m_value);
	int w = extract_w(a.m_value) >> extract_w(b.m_value);

	__m128i v = insert_x(_mm_undefined_si128(), x);
	v = insert_y(v, y);
	v = insert_z(v, z);
	return vint{ insert_w(v, w) };
#else
	vint sign_mask(_mm_cmplt_epi32(a.m_value, _mm_setzero_si128()));
	vint a_shifted = vuint_shift_right(a ^ sign_mask, b) ^ sign_mask;
	return a_shifted;
#endif
}

#undef VINT_SHIFT_LEFT
#undef VINT_SHIFT_RIGHT
#undef VUINT_SHIFT_RIGHT

// Shift left/right by a uniform immediate constant
#define VINT_SHIFT_LEFT(a, b) vint(_mm_slli_epi32( (a).m_value, (b) ) )
#define VINT_SHIFT_RIGHT(a, b) vint( _mm_srai_epi32( (a).m_value, (b) ) ) 
#define VUINT_SHIFT_RIGHT(a, b) vint( _mm_srli_epi32( (a).m_value, (b) ) )
#define VINT_ROT(x, k) (VINT_SHIFT_LEFT((x), (k)) | VUINT_SHIFT_RIGHT((x), 32 - (k)))

CPPSPMD_FORCE_INLINE vbool operator==(const lint& a, const lint& b) { return vbool{ _mm_cmpeq_epi32(a.m_value, b.m_value) }; }
CPPSPMD_FORCE_INLINE vbool operator==(const lint& a, int b) { return vint(a) == vint(b); }
CPPSPMD_FORCE_INLINE vbool operator==(int a, const lint& b) { return vint(a) == vint(b); }
CPPSPMD_FORCE_INLINE vbool operator<(const lint& a, const lint& b) { return vbool{ _mm_cmpgt_epi32(b.m_value, a.m_value) }; }
CPPSPMD_FORCE_INLINE vbool operator>(const lint& a, const lint& b) { return vbool{ _mm_cmpgt_epi32(a.m_value, b.m_value) }; }
CPPSPMD_FORCE_INLINE vbool operator<=(const lint& a, const lint& b) { return !vbool{ _mm_cmpgt_epi32(a.m_value, b.m_value) }; }
CPPSPMD_FORCE_INLINE vbool operator>=(const lint& a, const lint& b) { return !vbool{ _mm_cmpgt_epi32(b.m_value, a.m_value) }; }

CPPSPMD_FORCE_INLINE float extract(const vfloat& v, int instance) { assert(instance < 4); CPPSPMD_ALIGN(16) float values[4]; _mm_store_ps(values, v.m_value); return values[instance]; }
CPPSPMD_FORCE_INLINE int extract(const vint& v, int instance) { assert(instance < 4); CPPSPMD_ALIGN(16) int values[4]; _mm_store_si128((__m128i*)values, v.m_value); return values[instance]; }
CPPSPMD_FORCE_INLINE int extract(const lint& v, int instance) { assert(instance < 4); CPPSPMD_ALIGN(16) int values[4]; _mm_store_si128((__m128i*)values, v.m_value); return values[instance]; }
CPPSPMD_FORCE_INLINE bool extract(const vbool& v, int instance) { assert(instance < 4); CPPSPMD_ALIGN(16) int values[4]; _mm_store_si128((__m128i*)values, v.m_value); return values[instance] != 0; }

#undef VINT_EXTRACT
#undef VBOOL_EXTRACT
#undef VFLOAT_EXTRACT

#if CPPSPMD_SSE2
// Pass in an immediate constant and the compiler will optimize these expressions.
#define VINT_EXTRACT(v, instance) ( ((instance) == 0) ? extract_x((v).m_value) : (((instance) == 1) ? extract_y((v).m_value) : (((instance) == 2) ? extract_z((v).m_value) : extract_w((v).m_value))) )
#define VBOOL_EXTRACT(v, instance) ( ((instance) == 0) ? extract_x((v).m_value) : (((instance) == 1) ? extract_y((v).m_value) : (((instance) == 2) ? extract_z((v).m_value) : extract_w((v).m_value))) )
#define VFLOAT_EXTRACT(v, instance) ( ((instance) == 0) ? extractf_ps_x((v).m_value) : (((instance) == 1) ? extractf_ps_y((v).m_value) : (((instance) == 2) ? extractf_ps_z((v).m_value) : extractf_ps_w((v).m_value))) )
#else
CPPSPMD_FORCE_INLINE float cast_int_bits_as_float(int v) { return *(const float*)&v; }

#define VINT_EXTRACT(v, instance) _mm_extract_epi32((v).m_value, instance)
#define VBOOL_EXTRACT(v, instance) _mm_extract_epi32((v).m_value, instance)
#define VFLOAT_EXTRACT(v, instance) cast_int_bits_as_float(_mm_extract_ps((v).m_value, instance))
#endif

CPPSPMD_FORCE_INLINE vfloat &insert(vfloat& v, int instance, float f)
{
	assert(instance < 4);
	CPPSPMD_ALIGN(16) float values[4];
	_mm_store_ps(values, v.m_value);
	values[instance] = f;
	v.m_value = _mm_load_ps(values);
	return v;
}

CPPSPMD_FORCE_INLINE vint &insert(vint& v, int instance, int i)
{
	assert(instance < 4);
	CPPSPMD_ALIGN(16) int values[4];
	_mm_store_si128((__m128i *)values, v.m_value);
	values[instance] = i;
	v.m_value = _mm_load_si128((__m128i *)values);
	return v;
}

CPPSPMD_FORCE_INLINE vint init_lookup4(const uint8_t pTab[16])
{
	__m128i l = _mm_loadu_si128((const __m128i*)pTab);
	return vint{ l };
}

CPPSPMD_FORCE_INLINE vint table_lookup4_8(const vint& a, const vint& table)
{
	return vint{ shuffle_epi8(table.m_value, a.m_value) };
}

CPPSPMD_FORCE_INLINE void init_lookup5(const uint8_t pTab[32], vint& table_0, vint& table_1)
{
	__m128i l = _mm_loadu_si128((const __m128i*)pTab);
	__m128i h = _mm_loadu_si128((const __m128i*)(pTab + 16));
	table_0.m_value = l;
	table_1.m_value = h;
}

CPPSPMD_FORCE_INLINE vint table_lookup5_8(const vint& a, const vint& table_0, const vint& table_1)
{
	__m128i l_0 = shuffle_epi8(table_0.m_value, a.m_value);
	__m128i h_0 = shuffle_epi8(table_1.m_value, a.m_value);

	__m128i m_0 = _mm_slli_epi32(a.m_value, 31 - 4);

	__m128 v_0 = blendv_ps(_mm_castsi128_ps(l_0), _mm_castsi128_ps(h_0), _mm_castsi128_ps(m_0));

	return vint{ _mm_castps_si128(v_0) };
}

CPPSPMD_FORCE_INLINE void init_lookup6(const uint8_t pTab[64], vint& table_0, vint& table_1, vint& table_2, vint& table_3)
{
	__m128i a = _mm_loadu_si128((const __m128i*)pTab);
	__m128i b = _mm_loadu_si128((const __m128i*)(pTab + 16));
	__m128i c = _mm_loadu_si128((const __m128i*)(pTab + 32));
	__m128i d = _mm_loadu_si128((const __m128i*)(pTab + 48));

	table_0.m_value = a;
	table_1.m_value = b;
	table_2.m_value = c;
	table_3.m_value = d;
}

CPPSPMD_FORCE_INLINE vint table_lookup6_8(const vint& a, const vint& table_0, const vint& table_1, const vint& table_2, const vint& table_3)
{
	__m128i m_0 = _mm_slli_epi32(a.m_value, 31 - 4);

	__m128 av_0;
	{
		__m128i al_0 = shuffle_epi8(table_0.m_value, a.m_value);
		__m128i ah_0 = shuffle_epi8(table_1.m_value, a.m_value);
		av_0 = blendv_ps(_mm_castsi128_ps(al_0), _mm_castsi128_ps(ah_0), _mm_castsi128_ps(m_0));
	}

	__m128 bv_0;
	{
		__m128i bl_0 = shuffle_epi8(table_2.m_value, a.m_value);
		__m128i bh_0 = shuffle_epi8(table_3.m_value, a.m_value);
		bv_0 = blendv_ps(_mm_castsi128_ps(bl_0), _mm_castsi128_ps(bh_0), _mm_castsi128_ps(m_0));
	}

	__m128i m2_0 = _mm_slli_epi32(a.m_value, 31 - 5);
	__m128 v2_0 = blendv_ps(av_0, bv_0, _mm_castsi128_ps(m2_0));

	return vint{ _mm_castps_si128(v2_0) };
}

#if 0
template<typename SPMDKernel, typename... Args>
CPPSPMD_FORCE_INLINE decltype(auto) spmd_call(Args&&... args)
{
	SPMDKernel kernel;
	kernel.init(exec_mask::all_on());
	return kernel._call(std::forward<Args>(args)...);
}
#else
template<typename SPMDKernel, typename... Args>
CPPSPMD_FORCE_INLINE void spmd_call(Args&&... args)
{
	SPMDKernel kernel;
	kernel.init(exec_mask::all_on());
	kernel._call(std::forward<Args>(args)...);
}
#endif

CPPSPMD_FORCE_INLINE void spmd_kernel::init(const spmd_kernel::exec_mask& kernel_exec)
{
	m_exec = kernel_exec;
	m_kernel_exec = kernel_exec;
	m_continue_mask = exec_mask::all_off();

#ifdef _DEBUG
	m_in_loop = false;
#endif
}

CPPSPMD_FORCE_INLINE const float_vref& spmd_kernel::store(const float_vref& dst, const vfloat& src)
{
	CPPSPMD_ALIGN(16) int vindex[4];
	_mm_store_si128((__m128i*)vindex, dst.m_vindex);

	CPPSPMD_ALIGN(16) float stored[4];
	_mm_store_ps(stored, src.m_value);

	int mask = _mm_movemask_ps(_mm_castsi128_ps(m_exec.m_mask));
	for (int i = 0; i < 4; i++)
	{
		if (mask & (1 << i))
			dst.m_pValue[vindex[i]] = stored[i];
	}
	return dst;
}

CPPSPMD_FORCE_INLINE const float_vref& spmd_kernel::store_all(const float_vref& dst, const vfloat& src)
{
	CPPSPMD_ALIGN(16) int vindex[4];
	_mm_store_si128((__m128i*)vindex, dst.m_vindex);

	CPPSPMD_ALIGN(16) float stored[4];
	_mm_store_ps(stored, src.m_value);

	for (int i = 0; i < 4; i++)
		dst.m_pValue[vindex[i]] = stored[i];
	return dst;
}

CPPSPMD_FORCE_INLINE const float_vref& spmd_kernel::store(const float_vref&& dst, const vfloat& src)
{
	CPPSPMD_ALIGN(16) int vindex[4];
	_mm_store_si128((__m128i*)vindex, dst.m_vindex);

	CPPSPMD_ALIGN(16) float stored[4];
	_mm_store_ps(stored, src.m_value);

	int mask = _mm_movemask_ps(_mm_castsi128_ps(m_exec.m_mask));
	for (int i = 0; i < 4; i++)
	{
		if (mask & (1 << i))
			dst.m_pValue[vindex[i]] = stored[i];
	}
	return dst;
}

CPPSPMD_FORCE_INLINE const float_vref& spmd_kernel::store_all(const float_vref&& dst, const vfloat& src)
{
	CPPSPMD_ALIGN(16) int vindex[4];
	_mm_store_si128((__m128i*)vindex, dst.m_vindex);

	CPPSPMD_ALIGN(16) float stored[4];
	_mm_store_ps(stored, src.m_value);

	for (int i = 0; i < 4; i++)
		dst.m_pValue[vindex[i]] = stored[i];
	return dst;
}

#include "cppspmd_flow.h"
#include "cppspmd_math.h"

} // namespace cppspmd_sse41


// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2022 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#ifdef JPH_USE_NEON

// Constructing NEON values
#ifdef JPH_COMPILER_MSVC
	#define JPH_NEON_INT32x4(v1, v2, v3, v4) { int64_t(v1) + (int64_t(v2) << 32), int64_t(v3) + (int64_t(v4) << 32) }
	#define JPH_NEON_UINT32x4(v1, v2, v3, v4) { uint64_t(v1) + (uint64_t(v2) << 32), uint64_t(v3) + (uint64_t(v4) << 32) }
	#define JPH_NEON_INT8x16(v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16) { int64_t(v1) + (int64_t(v2) << 8) + (int64_t(v3) << 16) + (int64_t(v4) << 24) + (int64_t(v5) << 32) + (int64_t(v6) << 40) + (int64_t(v7) << 48) + (int64_t(v8) << 56), int64_t(v9) + (int64_t(v10) << 8) + (int64_t(v11) << 16) + (int64_t(v12) << 24) + (int64_t(v13) << 32) + (int64_t(v14) << 40) + (int64_t(v15) << 48) + (int64_t(v16) << 56) }
	#define JPH_NEON_UINT8x16(v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16) { uint64_t(v1) + (uint64_t(v2) << 8) + (uint64_t(v3) << 16) + (uint64_t(v4) << 24) + (uint64_t(v5) << 32) + (uint64_t(v6) << 40) + (uint64_t(v7) << 48) + (uint64_t(v8) << 56), uint64_t(v9) + (uint64_t(v10) << 8) + (uint64_t(v11) << 16) + (uint64_t(v12) << 24) + (uint64_t(v13) << 32) + (uint64_t(v14) << 40) + (uint64_t(v15) << 48) + (uint64_t(v16) << 56) }
#else
	#define JPH_NEON_INT32x4(v1, v2, v3, v4) { v1, v2, v3, v4 }
	#define JPH_NEON_UINT32x4(v1, v2, v3, v4) { v1, v2, v3, v4 }
	#define JPH_NEON_INT8x16(v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16) { v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16 }
	#define JPH_NEON_UINT8x16(v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16) { v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16 }
#endif

// MSVC and GCC prior to version 12 don't define __builtin_shufflevector
#if defined(JPH_COMPILER_MSVC) || (defined(JPH_COMPILER_GCC) && __GNUC__ < 12)
	JPH_NAMESPACE_BEGIN

	// Generic shuffle vector template
	template <unsigned I1, unsigned I2, unsigned I3, unsigned I4>
	JPH_INLINE float32x4_t NeonShuffleFloat32x4(float32x4_t inV1, float32x4_t inV2)
	{
		float32x4_t ret;
		ret = vmovq_n_f32(vgetq_lane_f32(I1 >= 4? inV2 : inV1, I1 & 0b11));
		ret = vsetq_lane_f32(vgetq_lane_f32(I2 >= 4? inV2 : inV1, I2 & 0b11), ret, 1);
		ret = vsetq_lane_f32(vgetq_lane_f32(I3 >= 4? inV2 : inV1, I3 & 0b11), ret, 2);
		ret = vsetq_lane_f32(vgetq_lane_f32(I4 >= 4? inV2 : inV1, I4 & 0b11), ret, 3);
		return ret;
	}

	// Specializations
	template <>
	JPH_INLINE float32x4_t NeonShuffleFloat32x4<0, 1, 2, 2>(float32x4_t inV1, float32x4_t inV2)
	{
		return vcombine_f32(vget_low_f32(inV1), vdup_lane_f32(vget_high_f32(inV1), 0));
	}

	template <>
	JPH_INLINE float32x4_t NeonShuffleFloat32x4<0, 1, 3, 3>(float32x4_t inV1, float32x4_t inV2)
	{
		return vcombine_f32(vget_low_f32(inV1), vdup_lane_f32(vget_high_f32(inV1), 1));
	}

	template <>
	JPH_INLINE float32x4_t NeonShuffleFloat32x4<0, 1, 2, 3>(float32x4_t inV1, float32x4_t inV2)
	{
		return inV1;
	}

	template <>
	JPH_INLINE float32x4_t NeonShuffleFloat32x4<1, 0, 3, 2>(float32x4_t inV1, float32x4_t inV2)
	{
		return vcombine_f32(vrev64_f32(vget_low_f32(inV1)), vrev64_f32(vget_high_f32(inV1)));
	}

	template <>
	JPH_INLINE float32x4_t NeonShuffleFloat32x4<2, 2, 1, 0>(float32x4_t inV1, float32x4_t inV2)
	{
		return vcombine_f32(vdup_lane_f32(vget_high_f32(inV1), 0), vrev64_f32(vget_low_f32(inV1)));
	}

	template <>
	JPH_INLINE float32x4_t NeonShuffleFloat32x4<2, 3, 0, 1>(float32x4_t inV1, float32x4_t inV2)
	{
		return vcombine_f32(vget_high_f32(inV1), vget_low_f32(inV1));
	}

	// Used extensively by cross product
	template <>
	JPH_INLINE float32x4_t NeonShuffleFloat32x4<1, 2, 0, 0>(float32x4_t inV1, float32x4_t inV2)
	{
		static uint8x16_t table = JPH_NEON_UINT8x16(0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x00, 0x01, 0x02, 0x03, 0x00, 0x01, 0x02, 0x03);
		return vreinterpretq_f32_u8(vqtbl1q_u8(vreinterpretq_u8_f32(inV1), table));
	}

	// Shuffle a vector
	#define JPH_NEON_SHUFFLE_F32x4(vec1, vec2, index1, index2, index3, index4) NeonShuffleFloat32x4<index1, index2, index3, index4>(vec1, vec2)
	#define JPH_NEON_SHUFFLE_U32x4(vec1, vec2, index1, index2, index3, index4) vreinterpretq_u32_f32((NeonShuffleFloat32x4<index1, index2, index3, index4>(vreinterpretq_f32_u32(vec1), vreinterpretq_f32_u32(vec2))))

	JPH_NAMESPACE_END
#else
	// Shuffle a vector
	#define JPH_NEON_SHUFFLE_F32x4(vec1, vec2, index1, index2, index3, index4) __builtin_shufflevector(vec1, vec2, index1, index2, index3, index4)
	#define JPH_NEON_SHUFFLE_U32x4(vec1, vec2, index1, index2, index3, index4) __builtin_shufflevector(vec1, vec2, index1, index2, index3, index4)
#endif

#endif // JPH_USE_NEON

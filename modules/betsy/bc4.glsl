#[versions]

unsigned = "";
signed = "#define SNORM";

#[compute]
#version 450

#include "CrossPlatformSettings_piece_all.glsl"
#include "UavCrossPlatform_piece_all.glsl"

#VERSION_DEFINES

shared float2 g_minMaxValues[4u * 4u * 4u];
shared uint2 g_mask[4u * 4u];

layout(binding = 0) uniform sampler2D srcTex;
layout(binding = 1, rg32ui) uniform restrict writeonly uimage2D dstTexture;

layout(push_constant, std430) uniform Params {
	uint p_channelIdx;
	uint p_padding[3];
}
params;

layout(local_size_x = 4, //
		local_size_y = 4, //
		local_size_z = 4) in;

/// Each block is 16 pixels
/// Each thread works on 4 pixels
/// Therefore each block needs 4 threads, generating 8 masks
/// At the end these 8 masks get merged into 2 and results written to output
///
/// **Q: Why 4 pixels per thread? Why not 1 pixel per thread? Why not 2? Why not 16?**
///
/// A: It's a sweetspot.
///  - Very short threads cannot fill expensive GPUs with enough work (dispatch bound)
///  - Lots of threads means lots of synchronization (e.g. evaluating min/max, merging masks)
///    overhead, and also more LDS usage which reduces occupancy.
///  - Long threads (e.g. 1 thread per block) misses parallelism opportunities
void main() {
	float minVal, maxVal;
	float4 srcPixel;

	const uint blockThreadId = gl_LocalInvocationID.x;

	const uint2 pixelsToLoadBase = gl_GlobalInvocationID.yz << 2u;

	for (uint i = 0u; i < 4u; ++i) {
		const uint2 pixelsToLoad = pixelsToLoadBase + uint2(i, blockThreadId);

		const float4 value = OGRE_Load2D(srcTex, int2(pixelsToLoad), 0).xyzw;
		srcPixel[i] = params.p_channelIdx == 0 ? value.x : (params.p_channelIdx == 1 ? value.y : value.w);
		srcPixel[i] *= 255.0f;
	}

	minVal = min3(srcPixel.x, srcPixel.y, srcPixel.z);
	maxVal = max3(srcPixel.x, srcPixel.y, srcPixel.z);
	minVal = min(minVal, srcPixel.w);
	maxVal = max(maxVal, srcPixel.w);

	const uint minMaxIdxBase = (gl_LocalInvocationID.z << 4u) + (gl_LocalInvocationID.y << 2u);
	const uint maskIdxBase = (gl_LocalInvocationID.z << 2u) + gl_LocalInvocationID.y;

	g_minMaxValues[minMaxIdxBase + blockThreadId] = float2(minVal, maxVal);
	g_mask[maskIdxBase] = uint2(0u, 0u);

	memoryBarrierShared();
	barrier();

	// Have all 4 threads in the block grab the min/max value by comparing what all 4 threads uploaded
	for (uint i = 0u; i < 4u; ++i) {
		minVal = min(g_minMaxValues[minMaxIdxBase + i].x, minVal);
		maxVal = max(g_minMaxValues[minMaxIdxBase + i].y, maxVal);
	}

	// determine bias and emit color indices
	// given the choice of maxVal/minVal, these indices are optimal:
	// http://fgiesen.wordpress.com/2009/12/15/dxt5-alpha-block-index-determination/
	float dist = maxVal - minVal;
	float dist4 = dist * 4.0f;
	float dist2 = dist * 2.0f;
	float bias = (dist < 8) ? (dist - 1) : (trunc(dist * 0.5f) + 2);
	bias -= minVal * 7;

	uint mask0 = 0u, mask1 = 0u;

	for (uint i = 0u; i < 4u; ++i) {
		float a = srcPixel[i] * 7.0f + bias;

		int ind = 0;

		// select index. this is a "linear scale" lerp factor between 0 (val=min) and 7 (val=max).
		if (a >= dist4) {
			ind = 4;
			a -= dist4;
		}

		if (a >= dist2) {
			ind += 2;
			a -= dist2;
		}

		if (a >= dist)
			ind += 1;

		// turn linear scale into DXT index (0/1 are extremal pts)
		ind = -ind & 7;
		ind ^= (2 > ind) ? 1 : 0;

		// write index
		const uint bits = 16u + ((blockThreadId << 2u) + i) * 3u;
		if (bits < 32u) {
			mask0 |= uint(ind) << bits;
			if (bits + 3u > 32u) {
				mask1 |= uint(ind) >> (32u - bits);
			}
		} else {
			mask1 |= uint(ind) << (bits - 32u);
		}
	}

	if (mask0 != 0u)
		atomicOr(g_mask[maskIdxBase].x, mask0);
	if (mask1 != 0u)
		atomicOr(g_mask[maskIdxBase].y, mask1);

	memoryBarrierShared();
	barrier();

	if (blockThreadId == 0u) {
		// Save data
		uint2 outputBytes;

#ifdef SNORM
		outputBytes.x =
				packSnorm4x8(float4(maxVal * (1.0f / 255.0f) * 2.0f - 1.0f,
						minVal * (1.0f / 255.0f) * 2.0f - 1.0f, 0.0f, 0.0f));
#else
		outputBytes.x = packUnorm4x8(
				float4(maxVal * (1.0f / 255.0f), minVal * (1.0f / 255.0f), 0.0f, 0.0f));
#endif

		outputBytes.x |= g_mask[maskIdxBase].x;
		outputBytes.y = g_mask[maskIdxBase].y;

		uint2 dstUV = gl_GlobalInvocationID.yz;
		imageStore(dstTexture, int2(dstUV), uint4(outputBytes.xy, 0u, 0u));
	}
}

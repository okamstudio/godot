#[versions]

standard = "";
dithered = "#define BC1_DITHER";

#[compute]
#version 450

#include "CrossPlatformSettings_piece_all.glsl"
#include "UavCrossPlatform_piece_all.glsl"

#define FLT_MAX 340282346638528859811704183484516925440.0f

layout(binding = 0) uniform sampler2D srcTex;
layout(binding = 1, rg32ui) uniform restrict writeonly uimage2D dstTexture;

layout(std430, binding = 2) readonly restrict buffer globalBuffer {
	float2 c_oMatch5[256];
	float2 c_oMatch6[256];
};

layout(push_constant, std430) uniform Params {
	uint p_numRefinements;
	uint p_padding[3];
}
params;

layout(local_size_x = 8, //
		local_size_y = 8, //
		local_size_z = 1) in;

float3 rgb565to888(float rgb565) {
	float3 retVal;
	retVal.x = floor(rgb565 / 2048.0f);
	retVal.y = floor(mod(rgb565, 2048.0f) / 32.0f);
	retVal.z = floor(mod(rgb565, 32.0f));

	// This is the correct 565 to 888 conversion:
	//		rgb = floor( rgb * ( 255.0f / float3( 31.0f, 63.0f, 31.0f ) ) + 0.5f )
	//
	// However stb_dxt follows a different one:
	//		rb = floor( rb * ( 256 / 32 + 8 / 32 ) );
	//		g  = floor( g  * ( 256 / 64 + 4 / 64 ) );
	//
	// I'm not sure exactly why but it's possible this is how the S3TC specifies it should be decoded
	// It's quite possible this is the reason:
	//		http://www.ludicon.com/castano/blog/2009/03/gpu-dxt-decompression/
	//
	// Or maybe it's just because it's cheap to do with integer shifts.
	// Anyway, we follow stb_dxt's conversion just in case
	// (gives almost the same result, with 1 or -1 of difference for a very few values)
	//
	// Perhaps when we make 888 -> 565 -> 888 it doesn't matter
	// because they end up mapping to the original number

	return floor(retVal * float3(8.25f, 4.0625f, 8.25f));
}

float rgb888to565(float3 rgbValue) {
	rgbValue.rb = floor(rgbValue.rb * 31.0f / 255.0f + 0.5f);
	rgbValue.g = floor(rgbValue.g * 63.0f / 255.0f + 0.5f);

	return rgbValue.r * 2048.0f + rgbValue.g * 32.0f + rgbValue.b;
}

// linear interpolation at 1/3 point between a and b, using desired rounding type
float3 lerp13(float3 a, float3 b) {
#ifdef STB_DXT_USE_ROUNDING_BIAS
	// with rounding bias
	return a + floor((b - a) * (1.0f / 3.0f) + 0.5f);
#else
	// without rounding bias
	return floor((2.0f * a + b) / 3.0f);
#endif
}

/// Unpacks a block of 4 colors from two 16-bit endpoints
void EvalColors(out float3 colors[4], float c0, float c1) {
	colors[0] = rgb565to888(c0);
	colors[1] = rgb565to888(c1);
	colors[2] = lerp13(colors[0], colors[1]);
	colors[3] = lerp13(colors[1], colors[0]);
}

/** The color optimization function. (Clever code, part 1)
@param outMinEndp16 [out]
	Minimum endpoint, in RGB565
@param outMaxEndp16 [out]
	Maximum endpoint, in RGB565
*/
void OptimizeColorsBlock(const uint srcPixelsBlock[16], out float outMinEndp16, out float outMaxEndp16) {
	// determine color distribution
	float3 avgColor;
	float3 minColor;
	float3 maxColor;

	avgColor = minColor = maxColor = unpackUnorm4x8(srcPixelsBlock[0]).xyz;
	for (int i = 1; i < 16; ++i) {
		const float3 currColorUnorm = unpackUnorm4x8(srcPixelsBlock[i]).xyz;
		avgColor += currColorUnorm;
		minColor = min(minColor, currColorUnorm);
		maxColor = max(maxColor, currColorUnorm);
	}

	avgColor = round(avgColor * 255.0f / 16.0f);
	maxColor *= 255.0f;
	minColor *= 255.0f;

	// determine covariance matrix
	float cov[6];
	for (int i = 0; i < 6; ++i) {
		cov[i] = 0;
	}

	for (int i = 0; i < 16; ++i) {
		const float3 currColor = unpackUnorm4x8(srcPixelsBlock[i]).xyz * 255.0f;
		float3 rgbDiff = currColor - avgColor;

		cov[0] += rgbDiff.r * rgbDiff.r;
		cov[1] += rgbDiff.r * rgbDiff.g;
		cov[2] += rgbDiff.r * rgbDiff.b;
		cov[3] += rgbDiff.g * rgbDiff.g;
		cov[4] += rgbDiff.g * rgbDiff.b;
		cov[5] += rgbDiff.b * rgbDiff.b;
	}

	// convert covariance matrix to float, find principal axis via power iter
	for (int i = 0; i < 6; ++i) {
		cov[i] /= 255.0f;
	}

	float3 vF = maxColor - minColor;

	const int nIterPower = 4;
	for (int iter = 0; iter < nIterPower; ++iter) {
		const float r = vF.r * cov[0] + vF.g * cov[1] + vF.b * cov[2];
		const float g = vF.r * cov[1] + vF.g * cov[3] + vF.b * cov[4];
		const float b = vF.r * cov[2] + vF.g * cov[4] + vF.b * cov[5];

		vF.r = r;
		vF.g = g;
		vF.b = b;
	}

	float magn = max3(abs(vF.r), abs(vF.g), abs(vF.b));
	float3 v;

	if (magn < 4.0f) { // too small, default to luminance
		v.r = 299.0f; // JPEG YCbCr luma coefs, scaled by 1000.
		v.g = 587.0f;
		v.b = 114.0f;
	} else {
		v = trunc(vF * (512.0f / magn));
	}

	// Pick colors at extreme points
	float3 minEndpoint, maxEndpoint;
	float minDot = FLT_MAX;
	float maxDot = -FLT_MAX;
	for (int i = 0; i < 16; ++i) {
		const float3 currColor = unpackUnorm4x8(srcPixelsBlock[i]).xyz * 255.0f;
		const float dotValue = dot(currColor, v);

		if (dotValue < minDot) {
			minDot = dotValue;
			minEndpoint = currColor;
		}

		if (dotValue > maxDot) {
			maxDot = dotValue;
			maxEndpoint = currColor;
		}
	}

	outMinEndp16 = rgb888to565(minEndpoint);
	outMaxEndp16 = rgb888to565(maxEndpoint);
}

// The color matching function
uint MatchColorsBlock(const uint srcPixelsBlock[16], float3 color[4]) {
	uint mask = 0u;
	float3 dir = color[0] - color[1];
	float stops[4];

	for (int i = 0; i < 4; ++i) {
		stops[i] = dot(color[i], dir);
	}

	// think of the colors as arranged on a line; project point onto that line, then choose
	// next color out of available ones. we compute the crossover points for "best color in top
	// half"/"best in bottom half" and then the same inside that subinterval.
	//
	// relying on this 1d approximation isn't always optimal in terms of euclidean distance,
	// but it's very close and a lot faster.
	// http://cbloomrants.blogspot.com/2008/12/12-08-08-dxtc-summary.html

	float c0Point = trunc((stops[1] + stops[3]) * 0.5f);
	float halfPoint = trunc((stops[3] + stops[2]) * 0.5f);
	float c3Point = trunc((stops[2] + stops[0]) * 0.5f);

#ifndef BC1_DITHER
	// the version without dithering is straightforward
	for (uint i = 16u; i-- > 0u;) {
		const float3 currColor = unpackUnorm4x8(srcPixelsBlock[i]).xyz * 255.0f;

		const float dotValue = dot(currColor, dir);
		mask <<= 2u;

		if (dotValue < halfPoint) {
			mask |= ((dotValue < c0Point) ? 1u : 3u);
		} else {
			mask |= ((dotValue < c3Point) ? 2u : 0u);
		}
	}
#else
	// with floyd-steinberg dithering
	float4 ep1 = float4(0, 0, 0, 0);
	float4 ep2 = float4(0, 0, 0, 0);

	c0Point *= 16.0f;
	halfPoint *= 16.0f;
	c3Point *= 16.0f;

	for (uint y = 0u; y < 4u; ++y) {
		float ditherDot;
		uint lmask, step;

		float3 currColor;
		float dotValue;

		currColor = unpackUnorm4x8(srcPixelsBlock[y * 4 + 0]).xyz * 255.0f;
		dotValue = dot(currColor, dir);

		ditherDot = (dotValue * 16.0f) + (3 * ep2[1] + 5 * ep2[0]);
		if (ditherDot < halfPoint) {
			step = (ditherDot < c0Point) ? 1u : 3u;
		} else {
			step = (ditherDot < c3Point) ? 2u : 0u;
		}
		ep1[0] = dotValue - stops[step];
		lmask = step;

		currColor = unpackUnorm4x8(srcPixelsBlock[y * 4 + 1]).xyz * 255.0f;
		dotValue = dot(currColor, dir);

		ditherDot = (dotValue * 16.0f) + (7 * ep1[0] + 3 * ep2[2] + 5 * ep2[1] + ep2[0]);
		if (ditherDot < halfPoint) {
			step = (ditherDot < c0Point) ? 1u : 3u;
		} else {
			step = (ditherDot < c3Point) ? 2u : 0u;
		}
		ep1[1] = dotValue - stops[step];
		lmask |= step << 2u;

		currColor = unpackUnorm4x8(srcPixelsBlock[y * 4 + 2]).xyz * 255.0f;
		dotValue = dot(currColor, dir);

		ditherDot = (dotValue * 16.0f) + (7 * ep1[1] + 3 * ep2[3] + 5 * ep2[2] + ep2[1]);
		if (ditherDot < halfPoint) {
			step = (ditherDot < c0Point) ? 1u : 3u;
		} else {
			step = (ditherDot < c3Point) ? 2u : 0u;
		}
		ep1[2] = dotValue - stops[step];
		lmask |= step << 4u;

		currColor = unpackUnorm4x8(srcPixelsBlock[y * 4 + 2]).xyz * 255.0f;
		dotValue = dot(currColor, dir);

		ditherDot = (dotValue * 16.0f) + (7 * ep1[2] + 5 * ep2[3] + ep2[2]);
		if (ditherDot < halfPoint) {
			step = (ditherDot < c0Point) ? 1u : 3u;
		} else {
			step = (ditherDot < c3Point) ? 2u : 0u;
		}
		ep1[3] = dotValue - stops[step];
		lmask |= step << 6u;

		mask |= lmask << (y * 8u);
		{
			float4 tmp = ep1;
			ep1 = ep2;
			ep2 = tmp;
		} // swap
	}
#endif

	return mask;
}

// The refinement function. (Clever code, part 2)
// Tries to optimize colors to suit block contents better.
// (By solving a least squares system via normal equations+Cramer's rule)
bool RefineBlock(const uint srcPixelsBlock[16], uint mask, inout float inOutMinEndp16,
		inout float inOutMaxEndp16) {
	float newMin16, newMax16;
	const float oldMin = inOutMinEndp16;
	const float oldMax = inOutMaxEndp16;

	if ((mask ^ (mask << 2u)) < 4u) // all pixels have the same index?
	{
		// yes, linear system would be singular; solve using optimal
		// single-color match on average color
		float3 rgbVal = float3(8.0f / 255.0f, 8.0f / 255.0f, 8.0f / 255.0f);
		for (int i = 0; i < 16; ++i) {
			rgbVal += unpackUnorm4x8(srcPixelsBlock[i]).xyz;
		}

		rgbVal = floor(rgbVal * (255.0f / 16.0f));

		newMax16 = c_oMatch5[uint(rgbVal.r)][0] * 2048.0f + //
				c_oMatch6[uint(rgbVal.g)][0] * 32.0f + //
				c_oMatch5[uint(rgbVal.b)][0];
		newMin16 = c_oMatch5[uint(rgbVal.r)][1] * 2048.0f + //
				c_oMatch6[uint(rgbVal.g)][1] * 32.0f + //
				c_oMatch5[uint(rgbVal.b)][1];
	} else {
		const float w1Tab[4] = { 3, 0, 2, 1 };
		const float prods[4] = { 589824.0f, 2304.0f, 262402.0f, 66562.0f };
		// ^some magic to save a lot of multiplies in the accumulating loop...
		// (precomputed products of weights for least squares system, accumulated inside one 32-bit
		// register)

		float akku = 0.0f;
		uint cm = mask;
		float3 at1 = float3(0, 0, 0);
		float3 at2 = float3(0, 0, 0);
		for (int i = 0; i < 16; ++i, cm >>= 2u) {
			const float3 currColor = unpackUnorm4x8(srcPixelsBlock[i]).xyz * 255.0f;

			const uint step = cm & 3u;
			const float w1 = w1Tab[step];
			akku += prods[step];
			at1 += currColor * w1;
			at2 += currColor;
		}

		at2 = 3.0f * at2 - at1;

		// extract solutions and decide solvability
		const float xx = floor(akku / 65535.0f);
		const float yy = floor(mod(akku, 65535.0f) / 256.0f);
		const float xy = mod(akku, 256.0f);

		float2 f_rb_g;
		f_rb_g.x = 3.0f * 31.0f / 255.0f / (xx * yy - xy * xy);
		f_rb_g.y = f_rb_g.x * 63.0f / 31.0f;

		// solve.
		const float3 newMaxVal = clamp(floor((at1 * yy - at2 * xy) * f_rb_g.xyx + 0.5f),
				float3(0.0f, 0.0f, 0.0f), float3(31, 63, 31));
		newMax16 = newMaxVal.x * 2048.0f + newMaxVal.y * 32.0f + newMaxVal.z;

		const float3 newMinVal = clamp(floor((at2 * xx - at1 * xy) * f_rb_g.xyx + 0.5f),
				float3(0.0f, 0.0f, 0.0f), float3(31, 63, 31));
		newMin16 = newMinVal.x * 2048.0f + newMinVal.y * 32.0f + newMinVal.z;
	}

	inOutMinEndp16 = newMin16;
	inOutMaxEndp16 = newMax16;

	return oldMin != newMin16 || oldMax != newMax16;
}

#ifdef BC1_DITHER
/// Quantizes 'srcValue' which is originally in 888 (full range),
/// converting it to 565 and then back to 888 (quantized)
float3 quant(float3 srcValue) {
	srcValue = clamp(srcValue, 0.0f, 255.0f);
	// Convert 888 -> 565
	srcValue = floor(srcValue * float3(31.0f / 255.0f, 63.0f / 255.0f, 31.0f / 255.0f) + 0.5f);
	// Convert 565 -> 888 back
	srcValue = floor(srcValue * float3(8.25f, 4.0625f, 8.25f));

	return srcValue;
}

void DitherBlock(const uint srcPixBlck[16], out uint dthPixBlck[16]) {
	float3 ep1[4] = { float3(0, 0, 0), float3(0, 0, 0), float3(0, 0, 0), float3(0, 0, 0) };
	float3 ep2[4] = { float3(0, 0, 0), float3(0, 0, 0), float3(0, 0, 0), float3(0, 0, 0) };

	for (uint y = 0u; y < 16u; y += 4u) {
		float3 srcPixel, dithPixel;

		srcPixel = unpackUnorm4x8(srcPixBlck[y + 0u]).xyz * 255.0f;
		dithPixel = quant(srcPixel + trunc((3 * ep2[1] + 5 * ep2[0]) * (1.0f / 16.0f)));
		ep1[0] = srcPixel - dithPixel;
		dthPixBlck[y + 0u] = packUnorm4x8(float4(dithPixel * (1.0f / 255.0f), 1.0f));

		srcPixel = unpackUnorm4x8(srcPixBlck[y + 1u]).xyz * 255.0f;
		dithPixel = quant(
				srcPixel + trunc((7 * ep1[0] + 3 * ep2[2] + 5 * ep2[1] + ep2[0]) * (1.0f / 16.0f)));
		ep1[1] = srcPixel - dithPixel;
		dthPixBlck[y + 1u] = packUnorm4x8(float4(dithPixel * (1.0f / 255.0f), 1.0f));

		srcPixel = unpackUnorm4x8(srcPixBlck[y + 2u]).xyz * 255.0f;
		dithPixel = quant(
				srcPixel + trunc((7 * ep1[1] + 3 * ep2[3] + 5 * ep2[2] + ep2[1]) * (1.0f / 16.0f)));
		ep1[2] = srcPixel - dithPixel;
		dthPixBlck[y + 2u] = packUnorm4x8(float4(dithPixel * (1.0f / 255.0f), 1.0f));

		srcPixel = unpackUnorm4x8(srcPixBlck[y + 3u]).xyz * 255.0f;
		dithPixel = quant(srcPixel + trunc((7 * ep1[2] + 5 * ep2[3] + ep2[2]) * (1.0f / 16.0f)));
		ep1[3] = srcPixel - dithPixel;
		dthPixBlck[y + 3u] = packUnorm4x8(float4(dithPixel * (1.0f / 255.0f), 1.0f));

		// swap( ep1, ep2 )
		for (uint i = 0u; i < 4u; ++i) {
			float3 tmp = ep1[i];
			ep1[i] = ep2[i];
			ep2[i] = tmp;
		}
	}
}
#endif

void main() {
	uint srcPixelsBlock[16];

	bool bAllColorsEqual = true;

	// Load the whole 4x4 block
	const uint2 pixelsToLoadBase = gl_GlobalInvocationID.xy << 2u;
	for (uint i = 0u; i < 16u; ++i) {
		const uint2 pixelsToLoad = pixelsToLoadBase + uint2(i & 0x03u, i >> 2u);
		const float3 srcPixels0 = OGRE_Load2D(srcTex, int2(pixelsToLoad), 0).xyz;
		srcPixelsBlock[i] = packUnorm4x8(float4(srcPixels0, 1.0f));
		bAllColorsEqual = bAllColorsEqual && srcPixelsBlock[0] == srcPixelsBlock[i];
	}

	float maxEndp16, minEndp16;
	uint mask = 0u;

	if (bAllColorsEqual) {
		const uint3 rgbVal = uint3(unpackUnorm4x8(srcPixelsBlock[0]).xyz * 255.0f);
		mask = 0xAAAAAAAAu;
		maxEndp16 =
				c_oMatch5[rgbVal.r][0] * 2048.0f + c_oMatch6[rgbVal.g][0] * 32.0f + c_oMatch5[rgbVal.b][0];
		minEndp16 =
				c_oMatch5[rgbVal.r][1] * 2048.0f + c_oMatch6[rgbVal.g][1] * 32.0f + c_oMatch5[rgbVal.b][1];
	} else {
#ifdef BC1_DITHER
		uint ditherPixelsBlock[16];
		// first step: compute dithered version for PCA if desired
		DitherBlock(srcPixelsBlock, ditherPixelsBlock);
#else
#define ditherPixelsBlock srcPixelsBlock
#endif

		// second step: pca+map along principal axis
		OptimizeColorsBlock(ditherPixelsBlock, minEndp16, maxEndp16);
		if (minEndp16 != maxEndp16) {
			float3 colors[4];
			EvalColors(colors, maxEndp16, minEndp16); // Note min/max are inverted
			mask = MatchColorsBlock(srcPixelsBlock, colors);
		}

		// third step: refine (multiple times if requested)
		bool bStopRefinement = false;
		for (uint i = 0u; i < params.p_numRefinements && !bStopRefinement; ++i) {
			const uint lastMask = mask;

			if (RefineBlock(ditherPixelsBlock, mask, minEndp16, maxEndp16)) {
				if (minEndp16 != maxEndp16) {
					float3 colors[4];
					EvalColors(colors, maxEndp16, minEndp16); // Note min/max are inverted
					mask = MatchColorsBlock(srcPixelsBlock, colors);
				} else {
					mask = 0u;
					bStopRefinement = true;
				}
			}

			bStopRefinement = mask == lastMask || bStopRefinement;
		}
	}

	// write the color block
	if (maxEndp16 < minEndp16) {
		const float tmpValue = minEndp16;
		minEndp16 = maxEndp16;
		maxEndp16 = tmpValue;
		mask ^= 0x55555555u;
	}

	uint2 outputBytes;
	outputBytes.x = uint(maxEndp16) | (uint(minEndp16) << 16u);
	outputBytes.y = mask;

	uint2 dstUV = gl_GlobalInvocationID.xy;
	imageStore(dstTexture, int2(dstUV), uint4(outputBytes.xy, 0u, 0u));
}

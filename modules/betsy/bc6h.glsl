#[versions]

signed = "#define SIGNED";
unsigned = "#define QUALITY"; // The "Quality" preset causes artifacting on signed data, so for now it's exclusive to unsigned.

#[compute]
#version 450

#include "CrossPlatformSettings_piece_all.glsl"

#VERSION_DEFINES

float3 f32tof16(float3 value) {
	return float3(packHalf2x16(float2(value.x, 0.0)),
			packHalf2x16(float2(value.y, 0.0)),
			packHalf2x16(float2(value.z, 0.0)));
}

float3 f16tof32(uint3 value) {
	return float3(unpackHalf2x16(value.x).x,
			unpackHalf2x16(value.y).x,
			unpackHalf2x16(value.z).x);
}

float f32tof16(float value) {
	return packHalf2x16(float2(value.x, 0.0));
}

float f16tof32(uint value) {
	return unpackHalf2x16(value.x).x;
}

layout(binding = 0) uniform sampler2D srcTexture;
layout(binding = 1, rgba32ui) uniform restrict writeonly uimage2D dstTexture;

layout(push_constant, std430) uniform Params {
	float2 p_textureSizeRcp;
	uint padding0;
	uint padding1;
}
params;

const float HALF_MAX = 65504.0f;
const uint PATTERN_NUM = 32u;

#ifdef SIGNED
const float HALF_MIN = -65504.0f;
#else
const float HALF_MIN = 0.0f;
#endif

#ifdef SIGNED
// https://github.com/godotengine/godot/pull/96377#issuecomment-2323488254
// https://github.com/godotengine/godot/pull/96377#issuecomment-2323450950
bool isNegative(float a) {
	return a < 0.0f;
}

float CalcSignlessMSLE(float a, float b) {
	float err = log2((b + 1.0f) / (a + 1.0f));
	err = err * err;
	return err;
}

float CrossCalcMSLE(float a, float b) {
	float result = 0.0f;
	result += CalcSignlessMSLE(0.0f, abs(a));
	result += CalcSignlessMSLE(0.0f, abs(b));
	return result;
}

float CalcMSLE(float3 a, float3 b) {
	float result = 0.0f;
	if (isNegative(a.x) != isNegative(b.x)) {
		result += CrossCalcMSLE(a.x, b.x);
	} else {
		result += CalcSignlessMSLE(abs(a.x), abs(b.x));
	}
	if (isNegative(a.y) != isNegative(b.y)) {
		result += CrossCalcMSLE(a.y, b.y);
	} else {
		result += CalcSignlessMSLE(abs(a.y), abs(b.y));
	}
	if (isNegative(a.z) != isNegative(b.z)) {
		result += CrossCalcMSLE(a.z, b.z);
	} else {
		result += CalcSignlessMSLE(abs(a.z), abs(b.z));
	}

	return result;
}
#else
float CalcMSLE(float3 a, float3 b) {
	float3 err = log2((b + 1.0f) / (a + 1.0f));
	err = err * err;
	return err.x + err.y + err.z;
}
#endif

uint PatternFixupID(uint i) {
	uint ret = 15u;
	ret = ((3441033216u >> i) & 0x1u) != 0 ? 2u : ret;
	ret = ((845414400u >> i) & 0x1u) != 0 ? 8u : ret;
	return ret;
}

uint Pattern(uint p, uint i) {
	uint p2 = p / 2u;
	uint p3 = p - p2 * 2u;

	uint enc = 0u;
	enc = p2 == 0u ? 2290666700u : enc;
	enc = p2 == 1u ? 3972591342u : enc;
	enc = p2 == 2u ? 4276930688u : enc;
	enc = p2 == 3u ? 3967876808u : enc;
	enc = p2 == 4u ? 4293707776u : enc;
	enc = p2 == 5u ? 3892379264u : enc;
	enc = p2 == 6u ? 4278255592u : enc;
	enc = p2 == 7u ? 4026597360u : enc;
	enc = p2 == 8u ? 9369360u : enc;
	enc = p2 == 9u ? 147747072u : enc;
	enc = p2 == 10u ? 1930428556u : enc;
	enc = p2 == 11u ? 2362323200u : enc;
	enc = p2 == 12u ? 823134348u : enc;
	enc = p2 == 13u ? 913073766u : enc;
	enc = p2 == 14u ? 267393000u : enc;
	enc = p2 == 15u ? 966553998u : enc;

	enc = p3 != 0u ? enc >> 16u : enc;
	uint ret = (enc >> i) & 0x1u;
	return ret;
}

#ifndef SIGNED
//UF
float3 Quantize7(float3 x) {
	return (f32tof16(x) * 128.0f) / (0x7bff + 1.0f);
}

float3 Quantize9(float3 x) {
	return (f32tof16(x) * 512.0f) / (0x7bff + 1.0f);
}

float3 Quantize10(float3 x) {
	return (f32tof16(x) * 1024.0f) / (0x7bff + 1.0f);
}

float3 Unquantize7(float3 x) {
	return (x * 65536.0f + 0x8000) / 128.0f;
}

float3 Unquantize9(float3 x) {
	return (x * 65536.0f + 0x8000) / 512.0f;
}

float3 Unquantize10(float3 x) {
	return (x * 65536.0f + 0x8000) / 1024.0f;
}

float3 FinishUnquantize(float3 endpoint0Unq, float3 endpoint1Unq, float weight) {
	float3 comp = (endpoint0Unq * (64.0f - weight) + endpoint1Unq * weight + 32.0f) * (31.0f / 4096.0f);
	return f16tof32(uint3(comp));
}
#else
//SF

float3 cmpSign(float3 value) {
	float3 signVal;
	signVal.x = value.x >= 0.0f ? 1.0f : -1.0f;
	signVal.y = value.y >= 0.0f ? 1.0f : -1.0f;
	signVal.z = value.z >= 0.0f ? 1.0f : -1.0f;
	return signVal;
}

float3 Quantize7(float3 x) {
	float3 signVal = cmpSign(x);
	return signVal * (f32tof16(abs(x)) * 64.0f) / (0x7bff + 1.0f);
}

float3 Quantize9(float3 x) {
	float3 signVal = cmpSign(x);
	return signVal * (f32tof16(abs(x)) * 256.0f) / (0x7bff + 1.0f);
}

float3 Quantize10(float3 x) {
	float3 signVal = cmpSign(x);
	return signVal * (f32tof16(abs(x)) * 512.0f) / (0x7bff + 1.0f);
}

float3 Unquantize7(float3 x) {
	float3 signVal = sign(x);
	x = abs(x);
	float3 finalVal = signVal * (x * 32768.0f + 0x4000) / 64.0f;
	finalVal.x = x.x >= 64.0f ? 32767.0 : finalVal.x;
	finalVal.y = x.y >= 64.0f ? 32767.0 : finalVal.y;
	finalVal.z = x.z >= 64.0f ? 32767.0 : finalVal.z;
	return finalVal;
}

float3 Unquantize9(float3 x) {
	float3 signVal = sign(x);
	x = abs(x);
	float3 finalVal = signVal * (x * 32768.0f + 0x4000) / 256.0f;
	finalVal.x = x.x >= 256.0f ? 32767.0 : finalVal.x;
	finalVal.y = x.y >= 256.0f ? 32767.0 : finalVal.y;
	finalVal.z = x.z >= 256.0f ? 32767.0 : finalVal.z;
	return finalVal;
}

float3 Unquantize10(float3 x) {
	float3 signVal = sign(x);
	x = abs(x);
	float3 finalVal = signVal * (x * 32768.0f + 0x4000) / 512.0f;
	finalVal.x = x.x >= 512.0f ? 32767.0 : finalVal.x;
	finalVal.y = x.y >= 512.0f ? 32767.0 : finalVal.y;
	finalVal.z = x.z >= 512.0f ? 32767.0 : finalVal.z;
	return finalVal;
}

float3 FinishUnquantize(float3 endpoint0Unq, float3 endpoint1Unq, float weight) {
	float3 comp = (endpoint0Unq * (64.0f - weight) + endpoint1Unq * weight + 32.0f) * (31.0f / 2048.0f);
	return f16tof32(uint3(comp));
}
#endif

void Swap(inout float3 a, inout float3 b) {
	float3 tmp = a;
	a = b;
	b = tmp;
}

void Swap(inout float a, inout float b) {
	float tmp = a;
	a = b;
	b = tmp;
}

uint ComputeIndex3(float texelPos, float endPoint0Pos, float endPoint1Pos) {
	float r = (texelPos - endPoint0Pos) / (endPoint1Pos - endPoint0Pos);
	return uint(clamp(r * 6.98182f + 0.00909f + 0.5f, 0.0f, 7.0f));
}

uint ComputeIndex4(float texelPos, float endPoint0Pos, float endPoint1Pos) {
	float r = (texelPos - endPoint0Pos) / (endPoint1Pos - endPoint0Pos);
	return uint(clamp(r * 14.93333f + 0.03333f + 0.5f, 0.0f, 15.0f));
}

// This adds a bitflag to quantized values that signifies whether they are negative.
void SignExtend(inout float3 v1, uint mask, uint signFlag) {
	int3 v = int3(v1);
	v.x = (v.x & int(mask)) | (v.x < 0 ? int(signFlag) : 0);
	v.y = (v.y & int(mask)) | (v.y < 0 ? int(signFlag) : 0);
	v.z = (v.z & int(mask)) | (v.z < 0 ? int(signFlag) : 0);
	v1 = v;
}

// Refine endpoints by insetting bounding box in log2 RGB space
void InsetColorBBoxP1(float3 texels[16], inout float3 blockMin, inout float3 blockMax) {
	float3 refinedBlockMin = blockMax;
	float3 refinedBlockMax = blockMin;

	for (uint i = 0; i < 16; ++i) {
		refinedBlockMin = min(refinedBlockMin, texels[i] == blockMin ? refinedBlockMin : texels[i]);
		refinedBlockMax = max(refinedBlockMax, texels[i] == blockMax ? refinedBlockMax : texels[i]);
	}

	float3 logRefinedBlockMax = log2(refinedBlockMax + 1.0f);
	float3 logRefinedBlockMin = log2(refinedBlockMin + 1.0f);

	float3 logBlockMax = log2(blockMax + 1.0f);
	float3 logBlockMin = log2(blockMin + 1.0f);
	float3 logBlockMaxExt = (logBlockMax - logBlockMin) * (1.0f / 32.0f);

	logBlockMin += min(logRefinedBlockMin - logBlockMin, logBlockMaxExt);
	logBlockMax -= min(logBlockMax - logRefinedBlockMax, logBlockMaxExt);

	blockMin = exp2(logBlockMin) - 1.0f;
	blockMax = exp2(logBlockMax) - 1.0f;
}

// Least squares optimization to find best endpoints for the selected block indices
void OptimizeEndpointsP1(float3 texels[16], inout float3 blockMin, inout float3 blockMax, in float3 blockMinNonInset, in float3 blockMaxNonInset) {
	float3 blockDir = blockMax - blockMin;
	blockDir = blockDir / (blockDir.x + blockDir.y + blockDir.z);

	float endPoint0Pos = f32tof16(dot(blockMin, blockDir));
	float endPoint1Pos = f32tof16(dot(blockMax, blockDir));

	float3 alphaTexelSum = float3(0.0f);
	float3 betaTexelSum = float3(0.0f);
	float alphaBetaSum = 0.0f;
	float alphaSqSum = 0.0f;
	float betaSqSum = 0.0f;

	for (uint i = 0; i < 16; i++) {
		float texelPos = f32tof16(dot(texels[i], blockDir));
		uint texelIndex = ComputeIndex4(texelPos, endPoint0Pos, endPoint1Pos);

		float beta = clamp(texelIndex / 15.0f, 0.0, 1.0);
		float alpha = 1.0f - beta;

		float3 texelF16 = f32tof16(texels[i].xyz);
		alphaTexelSum += alpha * texelF16;
		betaTexelSum += beta * texelF16;

		alphaBetaSum += alpha * beta;

		alphaSqSum += alpha * alpha;
		betaSqSum += beta * beta;
	}

	float det = alphaSqSum * betaSqSum - alphaBetaSum * alphaBetaSum;

	if (abs(det) > 0.00001f) {
		float detRcp = 1.0 / det;
		blockMin = clamp(f16tof32(uint3(clamp(detRcp * (alphaTexelSum * betaSqSum - betaTexelSum * alphaBetaSum), HALF_MIN, HALF_MAX))), blockMinNonInset, blockMaxNonInset);
		blockMax = clamp(f16tof32(uint3(clamp(detRcp * (betaTexelSum * alphaSqSum - alphaTexelSum * alphaBetaSum), HALF_MIN, HALF_MAX))), blockMinNonInset, blockMaxNonInset);
	}
}

// Least squares optimization to find best endpoints for the selected block indices
void OptimizeEndpointsP2(float3 texels[16], uint pattern, uint patternSelector, inout float3 blockMin, inout float3 blockMax) {
	float3 blockDir = blockMax - blockMin;
	blockDir = blockDir / (blockDir.x + blockDir.y + blockDir.z);

	float endPoint0Pos = f32tof16(dot(blockMin, blockDir));
	float endPoint1Pos = f32tof16(dot(blockMax, blockDir));

	float3 alphaTexelSum = float3(0.0f);
	float3 betaTexelSum = float3(0.0f);
	float alphaBetaSum = 0.0f;
	float alphaSqSum = 0.0f;
	float betaSqSum = 0.0f;

	for (int i = 0; i < 16; i++) {
		uint paletteID = Pattern(pattern, i);
		if (paletteID == patternSelector) {
			float texelPos = f32tof16(dot(texels[i], blockDir));
			uint texelIndex = ComputeIndex3(texelPos, endPoint0Pos, endPoint1Pos);

			float beta = clamp(texelIndex / 7.0f, 0.0, 1.0);
			float alpha = 1.0f - beta;

			float3 texelF16 = f32tof16(texels[i].xyz);
			alphaTexelSum += alpha * texelF16;
			betaTexelSum += beta * texelF16;

			alphaBetaSum += alpha * beta;

			alphaSqSum += alpha * alpha;
			betaSqSum += beta * beta;
		}
	}

	float det = alphaSqSum * betaSqSum - alphaBetaSum * alphaBetaSum;

	if (abs(det) > 0.00001f) {
		float detRcp = 1.0 / det;
		blockMin = f16tof32(uint3(clamp(detRcp * (alphaTexelSum * betaSqSum - betaTexelSum * alphaBetaSum), HALF_MIN, HALF_MAX)));
		blockMax = f16tof32(uint3(clamp(detRcp * (betaTexelSum * alphaSqSum - alphaTexelSum * alphaBetaSum), HALF_MIN, HALF_MAX)));
	}
}

// Encodes a block with mode 11 (2x 10-bit endpoints).
void EncodeP1(inout uint4 block, inout float blockMSLE, float3 texels[16]) {
	// compute endpoints (min/max RGB bbox)
	float3 blockMin = texels[0];
	float3 blockMax = texels[0];
	for (uint i = 1u; i < 16u; ++i) {
		blockMin = min(blockMin, texels[i]);
		blockMax = max(blockMax, texels[i]);
	}

	float3 blockMinNonInset = blockMin;
	float3 blockMaxNonInset = blockMax;
	InsetColorBBoxP1(texels, blockMin, blockMax);

	OptimizeEndpointsP1(texels, blockMin, blockMax, blockMinNonInset, blockMaxNonInset);

	float3 blockDir = blockMax - blockMin;
	blockDir = blockDir / (blockDir.x + blockDir.y + blockDir.z);

	float3 endpoint0 = Quantize10(blockMin);
	float3 endpoint1 = Quantize10(blockMax);
	float endPoint0Pos = f32tof16(dot(blockMin, blockDir));
	float endPoint1Pos = f32tof16(dot(blockMax, blockDir));

#ifdef SIGNED
	int maxVal10 = 0x1FF;
	endpoint0 = clamp(endpoint0, -maxVal10, maxVal10);
	endpoint1 = clamp(endpoint1, -maxVal10, maxVal10);
#endif

	// check if endpoint swap is required
	float fixupTexelPos = f32tof16(dot(texels[0], blockDir));
	uint fixupIndex = ComputeIndex4(fixupTexelPos, endPoint0Pos, endPoint1Pos);
	if (fixupIndex > 7) {
		Swap(endPoint0Pos, endPoint1Pos);
		Swap(endpoint0, endpoint1);
	}

	// compute indices
	uint indices[16] = { 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u };
	for (uint i = 0u; i < 16u; ++i) {
		float texelPos = f32tof16(dot(texels[i], blockDir));
		indices[i] = ComputeIndex4(texelPos, endPoint0Pos, endPoint1Pos);
	}

	// compute compression error (MSLE)
	float3 endpoint0Unq = Unquantize10(endpoint0);
	float3 endpoint1Unq = Unquantize10(endpoint1);
	float msle = 0.0f;
	for (uint i = 0u; i < 16u; ++i) {
		float weight = floor((indices[i] * 64.0f) / 15.0f + 0.5f);
		float3 texelUnc = FinishUnquantize(endpoint0Unq, endpoint1Unq, weight);

		msle += CalcMSLE(texels[i], texelUnc);
	}

#ifdef SIGNED
	SignExtend(endpoint0, 0x1FF, 0x200);
	SignExtend(endpoint1, 0x1FF, 0x200);
#endif

	// encode block for mode 11
	blockMSLE = msle;
	block.x = 0x03;

	// endpoints
	block.x |= uint(endpoint0.x) << 5u;
	block.x |= uint(endpoint0.y) << 15u;
	block.x |= uint(endpoint0.z) << 25u;
	block.y |= uint(endpoint0.z) >> 7u;
	block.y |= uint(endpoint1.x) << 3u;
	block.y |= uint(endpoint1.y) << 13u;
	block.y |= uint(endpoint1.z) << 23u;
	block.z |= uint(endpoint1.z) >> 9u;

	// indices
	block.z |= indices[0] << 1u;
	block.z |= indices[1] << 4u;
	block.z |= indices[2] << 8u;
	block.z |= indices[3] << 12u;
	block.z |= indices[4] << 16u;
	block.z |= indices[5] << 20u;
	block.z |= indices[6] << 24u;
	block.z |= indices[7] << 28u;
	block.w |= indices[8] << 0u;
	block.w |= indices[9] << 4u;
	block.w |= indices[10] << 8u;
	block.w |= indices[11] << 12u;
	block.w |= indices[12] << 16u;
	block.w |= indices[13] << 20u;
	block.w |= indices[14] << 24u;
	block.w |= indices[15] << 28u;
}

float DistToLineSq(float3 PointOnLine, float3 LineDirection, float3 Point) {
	float3 w = Point - PointOnLine;
	float3 x = w - dot(w, LineDirection) * LineDirection;

	return dot(x, x);
}

// Gets the deviation from the source data of a particular pattern (smaller is better).
float EvaluateP2Pattern(uint pattern, float3 texels[16]) {
	float3 p0BlockMin = float3(HALF_MAX, HALF_MAX, HALF_MAX);
	float3 p0BlockMax = float3(HALF_MIN, HALF_MIN, HALF_MIN);
	float3 p1BlockMin = float3(HALF_MAX, HALF_MAX, HALF_MAX);
	float3 p1BlockMax = float3(HALF_MIN, HALF_MIN, HALF_MIN);

	for (uint i = 0; i < 16; ++i) {
		uint paletteID = Pattern(pattern, i);
		if (paletteID == 0) {
			p0BlockMin = min(p0BlockMin, texels[i]);
			p0BlockMax = max(p0BlockMax, texels[i]);
		} else {
			p1BlockMin = min(p1BlockMin, texels[i]);
			p1BlockMax = max(p1BlockMax, texels[i]);
		}
	}

	float3 p0BlockDir = normalize(p0BlockMax - p0BlockMin);
	float3 p1BlockDir = normalize(p1BlockMax - p1BlockMin);

	float sqDistanceFromLine = 0.0f;

	for (uint i = 0; i < 16; ++i) {
		uint paletteID = Pattern(pattern, i);
		if (paletteID == 0) {
			sqDistanceFromLine += DistToLineSq(p0BlockMin, p0BlockDir, texels[i]);
		} else {
			sqDistanceFromLine += DistToLineSq(p1BlockMin, p1BlockDir, texels[i]);
		}
	}

	return sqDistanceFromLine;
}

// Encodes a block with either mode 2 (7-bit base, 3x 6-bit delta), or mode 6 (9-bit base, 3x 5-bit delta). Both use pattern encoding.
void EncodeP2Pattern(inout uint4 block, inout float blockMSLE, uint pattern, float3 texels[16]) {
	float3 p0BlockMin = float3(HALF_MAX, HALF_MAX, HALF_MAX);
	float3 p0BlockMax = float3(HALF_MIN, HALF_MIN, HALF_MIN);
	float3 p1BlockMin = float3(HALF_MAX, HALF_MAX, HALF_MAX);
	float3 p1BlockMax = float3(HALF_MIN, HALF_MIN, HALF_MIN);

	for (uint i = 0u; i < 16u; ++i) {
		uint paletteID = Pattern(pattern, i);
		if (paletteID == 0) {
			p0BlockMin = min(p0BlockMin, texels[i]);
			p0BlockMax = max(p0BlockMax, texels[i]);
		} else {
			p1BlockMin = min(p1BlockMin, texels[i]);
			p1BlockMax = max(p1BlockMax, texels[i]);
		}
	}

	OptimizeEndpointsP2(texels, pattern, 0, p0BlockMin, p0BlockMax);
	OptimizeEndpointsP2(texels, pattern, 1, p1BlockMin, p1BlockMax);

	float3 p0BlockDir = p0BlockMax - p0BlockMin;
	float3 p1BlockDir = p1BlockMax - p1BlockMin;
	p0BlockDir = p0BlockDir / (p0BlockDir.x + p0BlockDir.y + p0BlockDir.z);
	p1BlockDir = p1BlockDir / (p1BlockDir.x + p1BlockDir.y + p1BlockDir.z);

	float p0Endpoint0Pos = f32tof16(dot(p0BlockMin, p0BlockDir));
	float p0Endpoint1Pos = f32tof16(dot(p0BlockMax, p0BlockDir));
	float p1Endpoint0Pos = f32tof16(dot(p1BlockMin, p1BlockDir));
	float p1Endpoint1Pos = f32tof16(dot(p1BlockMax, p1BlockDir));

	uint fixupID = PatternFixupID(pattern);
	float p0FixupTexelPos = f32tof16(dot(texels[0], p0BlockDir));
	float p1FixupTexelPos = f32tof16(dot(texels[fixupID], p1BlockDir));
	uint p0FixupIndex = ComputeIndex3(p0FixupTexelPos, p0Endpoint0Pos, p0Endpoint1Pos);
	uint p1FixupIndex = ComputeIndex3(p1FixupTexelPos, p1Endpoint0Pos, p1Endpoint1Pos);
	if (p0FixupIndex > 3u) {
		Swap(p0Endpoint0Pos, p0Endpoint1Pos);
		Swap(p0BlockMin, p0BlockMax);
	}
	if (p1FixupIndex > 3u) {
		Swap(p1Endpoint0Pos, p1Endpoint1Pos);
		Swap(p1BlockMin, p1BlockMax);
	}

	uint indices[16] = { 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u };
	for (uint i = 0u; i < 16u; ++i) {
		float p0TexelPos = f32tof16(dot(texels[i], p0BlockDir));
		float p1TexelPos = f32tof16(dot(texels[i], p1BlockDir));
		uint p0Index = ComputeIndex3(p0TexelPos, p0Endpoint0Pos, p0Endpoint1Pos);
		uint p1Index = ComputeIndex3(p1TexelPos, p1Endpoint0Pos, p1Endpoint1Pos);

		uint paletteID = Pattern(pattern, i);
		indices[i] = paletteID == 0u ? p0Index : p1Index;
	}

	float3 endpoint760 = floor(Quantize7(p0BlockMin));
	float3 endpoint761 = floor(Quantize7(p0BlockMax));
	float3 endpoint762 = floor(Quantize7(p1BlockMin));
	float3 endpoint763 = floor(Quantize7(p1BlockMax));

	float3 endpoint950 = floor(Quantize9(p0BlockMin));
	float3 endpoint951 = floor(Quantize9(p0BlockMax));
	float3 endpoint952 = floor(Quantize9(p1BlockMin));
	float3 endpoint953 = floor(Quantize9(p1BlockMax));

	endpoint761 = endpoint761 - endpoint760;
	endpoint762 = endpoint762 - endpoint760;
	endpoint763 = endpoint763 - endpoint760;

	endpoint951 = endpoint951 - endpoint950;
	endpoint952 = endpoint952 - endpoint950;
	endpoint953 = endpoint953 - endpoint950;

	int maxVal76 = 0x1F;
	endpoint761 = clamp(endpoint761, -maxVal76, maxVal76);
	endpoint762 = clamp(endpoint762, -maxVal76, maxVal76);
	endpoint763 = clamp(endpoint763, -maxVal76, maxVal76);

	int maxVal95 = 0xF;
	endpoint951 = clamp(endpoint951, -maxVal95, maxVal95);
	endpoint952 = clamp(endpoint952, -maxVal95, maxVal95);
	endpoint953 = clamp(endpoint953, -maxVal95, maxVal95);

#ifdef SIGNED
	int maxVal7 = 0x3F;
	int maxVal9 = 0xFF;
	endpoint760 = clamp(endpoint760, -maxVal7, maxVal7);
	endpoint950 = clamp(endpoint950, -maxVal9, maxVal9);
#endif

	float3 endpoint760Unq = Unquantize7(endpoint760);
	float3 endpoint761Unq = Unquantize7(endpoint760 + endpoint761);
	float3 endpoint762Unq = Unquantize7(endpoint760 + endpoint762);
	float3 endpoint763Unq = Unquantize7(endpoint760 + endpoint763);
	float3 endpoint950Unq = Unquantize9(endpoint950);
	float3 endpoint951Unq = Unquantize9(endpoint950 + endpoint951);
	float3 endpoint952Unq = Unquantize9(endpoint950 + endpoint952);
	float3 endpoint953Unq = Unquantize9(endpoint950 + endpoint953);

	float msle76 = 0.0f;
	float msle95 = 0.0f;
	for (uint i = 0u; i < 16u; ++i) {
		uint paletteID = Pattern(pattern, i);

		float3 tmp760Unq = paletteID == 0u ? endpoint760Unq : endpoint762Unq;
		float3 tmp761Unq = paletteID == 0u ? endpoint761Unq : endpoint763Unq;
		float3 tmp950Unq = paletteID == 0u ? endpoint950Unq : endpoint952Unq;
		float3 tmp951Unq = paletteID == 0u ? endpoint951Unq : endpoint953Unq;

		float weight = floor((indices[i] * 64.0f) / 7.0f + 0.5f);
		float3 texelUnc76 = FinishUnquantize(tmp760Unq, tmp761Unq, weight);
		float3 texelUnc95 = FinishUnquantize(tmp950Unq, tmp951Unq, weight);

		msle76 += CalcMSLE(texels[i], texelUnc76);
		msle95 += CalcMSLE(texels[i], texelUnc95);
	}

	SignExtend(endpoint761, 0x1F, 0x20);
	SignExtend(endpoint762, 0x1F, 0x20);
	SignExtend(endpoint763, 0x1F, 0x20);

	SignExtend(endpoint951, 0xF, 0x10);
	SignExtend(endpoint952, 0xF, 0x10);
	SignExtend(endpoint953, 0xF, 0x10);

#ifdef SIGNED
	SignExtend(endpoint760, 0x3F, 0x40);
	SignExtend(endpoint950, 0xFF, 0x100);
#endif

	// encode block
	float p2MSLE = min(msle76, msle95);
	if (p2MSLE < blockMSLE) {
		blockMSLE = p2MSLE;
		block = uint4(0u, 0u, 0u, 0u);

		if (p2MSLE == msle76) {
			// 7.6
			block.x = 0x1u;
			block.x |= (uint(endpoint762.y) & 0x20u) >> 3u;
			block.x |= (uint(endpoint763.y) & 0x10u) >> 1u;
			block.x |= (uint(endpoint763.y) & 0x20u) >> 1u;
			block.x |= uint(endpoint760.x) << 5u;
			block.x |= (uint(endpoint763.z) & 0x01u) << 12u;
			block.x |= (uint(endpoint763.z) & 0x02u) << 12u;
			block.x |= (uint(endpoint762.z) & 0x10u) << 10u;
			block.x |= uint(endpoint760.y) << 15u;
			block.x |= (uint(endpoint762.z) & 0x20u) << 17u;
			block.x |= (uint(endpoint763.z) & 0x04u) << 21u;
			block.x |= (uint(endpoint762.y) & 0x10u) << 20u;
			block.x |= uint(endpoint760.z) << 25u;
			block.y |= (uint(endpoint763.z) & 0x08u) >> 3u;
			block.y |= (uint(endpoint763.z) & 0x20u) >> 4u;
			block.y |= (uint(endpoint763.z) & 0x10u) >> 2u;
			block.y |= uint(endpoint761.x) << 3u;
			block.y |= (uint(endpoint762.y) & 0x0Fu) << 9u;
			block.y |= uint(endpoint761.y) << 13u;
			block.y |= (uint(endpoint763.y) & 0x0Fu) << 19u;
			block.y |= uint(endpoint761.z) << 23u;
			block.y |= (uint(endpoint762.z) & 0x07u) << 29u;
			block.z |= (uint(endpoint762.z) & 0x08u) >> 3u;
			block.z |= uint(endpoint762.x) << 1u;
			block.z |= uint(endpoint763.x) << 7u;
		} else {
			// 9.5
			block.x = 0xEu;
			block.x |= uint(endpoint950.x) << 5u;
			block.x |= (uint(endpoint952.z) & 0x10u) << 10u;
			block.x |= uint(endpoint950.y) << 15u;
			block.x |= (uint(endpoint952.y) & 0x10u) << 20u;
			block.x |= uint(endpoint950.z) << 25u;
			block.y |= uint(endpoint950.z) >> 7u;
			block.y |= (uint(endpoint953.z) & 0x10u) >> 2u;
			block.y |= uint(endpoint951.x) << 3u;
			block.y |= (uint(endpoint953.y) & 0x10u) << 4u;
			block.y |= (uint(endpoint952.y) & 0x0Fu) << 9u;
			block.y |= uint(endpoint951.y) << 13u;
			block.y |= (uint(endpoint953.z) & 0x01u) << 18u;
			block.y |= (uint(endpoint953.y) & 0x0Fu) << 19u;
			block.y |= uint(endpoint951.z) << 23u;
			block.y |= (uint(endpoint953.z) & 0x02u) << 27u;
			block.y |= uint(endpoint952.z) << 29u;
			block.z |= (uint(endpoint952.z) & 0x08u) >> 3u;
			block.z |= uint(endpoint952.x) << 1u;
			block.z |= (uint(endpoint953.z) & 0x04u) << 4u;
			block.z |= uint(endpoint953.x) << 7u;
			block.z |= (uint(endpoint953.z) & 0x08u) << 9u;
		}

		block.z |= pattern << 13u;
		uint blockFixupID = PatternFixupID(pattern);
		if (blockFixupID == 15u) {
			block.z |= indices[0] << 18u;
			block.z |= indices[1] << 20u;
			block.z |= indices[2] << 23u;
			block.z |= indices[3] << 26u;
			block.z |= indices[4] << 29u;
			block.w |= indices[5] << 0u;
			block.w |= indices[6] << 3u;
			block.w |= indices[7] << 6u;
			block.w |= indices[8] << 9u;
			block.w |= indices[9] << 12u;
			block.w |= indices[10] << 15u;
			block.w |= indices[11] << 18u;
			block.w |= indices[12] << 21u;
			block.w |= indices[13] << 24u;
			block.w |= indices[14] << 27u;
			block.w |= indices[15] << 30u;
		} else if (blockFixupID == 2u) {
			block.z |= indices[0] << 18u;
			block.z |= indices[1] << 20u;
			block.z |= indices[2] << 23u;
			block.z |= indices[3] << 25u;
			block.z |= indices[4] << 28u;
			block.z |= indices[5] << 31u;
			block.w |= indices[5] >> 1u;
			block.w |= indices[6] << 2u;
			block.w |= indices[7] << 5u;
			block.w |= indices[8] << 8u;
			block.w |= indices[9] << 11u;
			block.w |= indices[10] << 14u;
			block.w |= indices[11] << 17u;
			block.w |= indices[12] << 20u;
			block.w |= indices[13] << 23u;
			block.w |= indices[14] << 26u;
			block.w |= indices[15] << 29u;
		} else {
			block.z |= indices[0] << 18u;
			block.z |= indices[1] << 20u;
			block.z |= indices[2] << 23u;
			block.z |= indices[3] << 26u;
			block.z |= indices[4] << 29u;
			block.w |= indices[5] << 0u;
			block.w |= indices[6] << 3u;
			block.w |= indices[7] << 6u;
			block.w |= indices[8] << 9u;
			block.w |= indices[9] << 11u;
			block.w |= indices[10] << 14u;
			block.w |= indices[11] << 17u;
			block.w |= indices[12] << 20u;
			block.w |= indices[13] << 23u;
			block.w |= indices[14] << 26u;
			block.w |= indices[15] << 29u;
		}
	}
}

layout(local_size_x = 8,
		local_size_y = 8,
		local_size_z = 1) in;

void main() {
	// gather texels for current 4x4 block
	// 0 1 2 3
	// 4 5 6 7
	// 8 9 10 11
	// 12 13 14 15
	float2 uv = gl_GlobalInvocationID.xy * params.p_textureSizeRcp * 4.0f + params.p_textureSizeRcp;
	float2 block0UV = uv;
	float2 block1UV = uv + float2(2.0f * params.p_textureSizeRcp.x, 0.0f);
	float2 block2UV = uv + float2(0.0f, 2.0f * params.p_textureSizeRcp.y);
	float2 block3UV = uv + float2(2.0f * params.p_textureSizeRcp.x, 2.0f * params.p_textureSizeRcp.y);
	float4 block0X = OGRE_GatherRed(srcTexture, pointSampler, block0UV);
	float4 block1X = OGRE_GatherRed(srcTexture, pointSampler, block1UV);
	float4 block2X = OGRE_GatherRed(srcTexture, pointSampler, block2UV);
	float4 block3X = OGRE_GatherRed(srcTexture, pointSampler, block3UV);
	float4 block0Y = OGRE_GatherGreen(srcTexture, pointSampler, block0UV);
	float4 block1Y = OGRE_GatherGreen(srcTexture, pointSampler, block1UV);
	float4 block2Y = OGRE_GatherGreen(srcTexture, pointSampler, block2UV);
	float4 block3Y = OGRE_GatherGreen(srcTexture, pointSampler, block3UV);
	float4 block0Z = OGRE_GatherBlue(srcTexture, pointSampler, block0UV);
	float4 block1Z = OGRE_GatherBlue(srcTexture, pointSampler, block1UV);
	float4 block2Z = OGRE_GatherBlue(srcTexture, pointSampler, block2UV);
	float4 block3Z = OGRE_GatherBlue(srcTexture, pointSampler, block3UV);

	float3 texels[16];
	texels[0] = float3(block0X.w, block0Y.w, block0Z.w);
	texels[1] = float3(block0X.z, block0Y.z, block0Z.z);
	texels[2] = float3(block1X.w, block1Y.w, block1Z.w);
	texels[3] = float3(block1X.z, block1Y.z, block1Z.z);
	texels[4] = float3(block0X.x, block0Y.x, block0Z.x);
	texels[5] = float3(block0X.y, block0Y.y, block0Z.y);
	texels[6] = float3(block1X.x, block1Y.x, block1Z.x);
	texels[7] = float3(block1X.y, block1Y.y, block1Z.y);
	texels[8] = float3(block2X.w, block2Y.w, block2Z.w);
	texels[9] = float3(block2X.z, block2Y.z, block2Z.z);
	texels[10] = float3(block3X.w, block3Y.w, block3Z.w);
	texels[11] = float3(block3X.z, block3Y.z, block3Z.z);
	texels[12] = float3(block2X.x, block2Y.x, block2Z.x);
	texels[13] = float3(block2X.y, block2Y.y, block2Z.y);
	texels[14] = float3(block3X.x, block3Y.x, block3Z.x);
	texels[15] = float3(block3X.y, block3Y.y, block3Z.y);

	uint4 block = uint4(0u, 0u, 0u, 0u);
	float blockMSLE = 0.0f;

	EncodeP1(block, blockMSLE, texels);

#ifdef QUALITY
	float bestScore = EvaluateP2Pattern(0, texels);
	uint bestPattern = 0;

	for (uint i = 1u; i < PATTERN_NUM; ++i) {
		float score = EvaluateP2Pattern(i, texels);

		if (score < bestScore) {
			bestPattern = i;
			bestScore = score;
		}
	}

	EncodeP2Pattern(block, blockMSLE, bestPattern, texels);
#endif

	imageStore(dstTexture, int2(gl_GlobalInvocationID.xy), block);
}

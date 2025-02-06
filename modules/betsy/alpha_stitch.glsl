// RGB and Alpha components of ETC2 RGBA are computed separately.
// This compute shader merely stitches them together to form the final result
// It's also used by RG11 driver to stitch two R11 into one RG11

#[compute]
#version 450

#include "CrossPlatformSettings_piece_all.glsl"

layout(binding = 0) uniform texture2D srcRGB[32];
layout(binding = 1) uniform texture2D srcAlpha[32];
layout(binding = 2) uniform sampler SAMPLER_NEAREST_CLAMP;
layout(binding = 3, rgba32ui) uniform restrict writeonly uimage2D dstTextures[32];

layout(push_constant, std430) uniform Params {
	uint p_textureIndex;
	uint p_padding[3];
}
params;

layout(local_size_x = 8, //
		local_size_y = 8, //
		local_size_z = 1) in;

void main() {
	float2 rgbBlock = OGRE_Load2D(sampler2D(srcRGB[params.p_textureIndex], SAMPLER_NEAREST_CLAMP), int2(gl_GlobalInvocationID.xy), 0).xy;
	float2 alphaBlock = OGRE_Load2D(sampler2D(srcAlpha[params.p_textureIndex], SAMPLER_NEAREST_CLAMP), int2(gl_GlobalInvocationID.xy), 0).xy;

	imageStore(dstTextures[params.p_textureIndex], int2(gl_GlobalInvocationID.xy), floatBitsToUint(float4(rgbBlock.xy, alphaBlock.xy)));
}

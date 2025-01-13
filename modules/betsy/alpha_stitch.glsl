// RGB and Alpha components of ETC2 RGBA are computed separately.
// This compute shader merely stitches them together to form the final result
// It's also used by RG11 driver to stitch two R11 into one RG11

#[compute]
#version 450

#include "CrossPlatformSettings_piece_all.glsl"
#include "UavCrossPlatform_piece_all.glsl"

layout(local_size_x = 8, //
		local_size_y = 8, //
		local_size_z = 1) in;

layout(binding = 0) uniform usampler2D srcRGB;
layout(binding = 1) uniform usampler2D srcAlpha;
layout(binding = 2, rgba32ui) uniform restrict writeonly uimage2D dstTexture;

void main() {
	uint2 rgbBlock = OGRE_Load2D(srcRGB, int2(gl_GlobalInvocationID.xy), 0).xy;
	uint2 alphaBlock = OGRE_Load2D(srcAlpha, int2(gl_GlobalInvocationID.xy), 0).xy;

	imageStore(dstTexture, int2(gl_GlobalInvocationID.xy), uint4(rgbBlock.xy, alphaBlock.xy));
}

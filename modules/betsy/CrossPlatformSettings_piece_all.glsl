#define min3(a, b, c) min(a, min(b, c))
#define max3(a, b, c) max(a, max(b, c))

#define float2 vec2
#define float3 vec3
#define float4 vec4

#define int2 ivec2
#define int3 ivec3
#define int4 ivec4

#define uint2 uvec2
#define uint3 uvec3
#define uint4 uvec4

#define float2x2 mat2
#define float3x3 mat3
#define float4x4 mat4
#define ogre_float4x3 mat3x4

#define ushort uint
#define ushort3 uint3
#define ushort4 uint4

//Short used for read operations. It's an int in GLSL & HLSL. An ushort in Metal
#define rshort int
#define rshort2 int2
#define rint int
//Short used for write operations. It's an int in GLSL. An ushort in HLSL & Metal
#define wshort2 int2
#define wshort3 int3

#define toFloat3x3(x) mat3(x)
#define buildFloat3x3(row0, row1, row2) mat3(row0, row1, row2)

#define mul(x, y) ((x) * (y))
#define saturate(x) clamp((x), 0.0, 1.0)
#define lerp mix
#define rsqrt inversesqrt
#define INLINE
#define NO_INTERPOLATION_PREFIX flat
#define NO_INTERPOLATION_SUFFIX

#define PARAMS_ARG_DECL
#define PARAMS_ARG

#define reversebits bitfieldReverse

#define OGRE_Sample(tex, sampler, uv) texture(tex, uv)
#define OGRE_SampleLevel(tex, sampler, uv, lod) textureLod(tex, uv, lod)
#define OGRE_SampleArray2D(tex, sampler, uv, arrayIdx) texture(tex, vec3(uv, arrayIdx))
#define OGRE_SampleArray2DLevel(tex, sampler, uv, arrayIdx, lod) textureLod(tex, vec3(uv, arrayIdx), lod)
#define OGRE_SampleArrayCubeLevel(tex, sampler, uv, arrayIdx, lod) textureLod(tex, vec4(uv, arrayIdx), lod)
#define OGRE_SampleGrad(tex, sampler, uv, ddx, ddy) textureGrad(tex, uv, ddx, ddy)
#define OGRE_SampleArray2DGrad(tex, sampler, uv, arrayIdx, ddx, ddy) textureGrad(tex, vec3(uv, arrayIdx), ddx, ddy)
#define OGRE_ddx(val) dFdx(val)
#define OGRE_ddy(val) dFdy(val)
#define OGRE_Load2D(tex, iuv, lod) texelFetch(tex, iuv, lod)
#define OGRE_LoadArray2D(tex, iuv, arrayIdx, lod) texelFetch(tex, ivec3(iuv, arrayIdx), lod)
#define OGRE_Load2DMS(tex, iuv, subsample) texelFetch(tex, iuv, subsample)

#define OGRE_Load3D(tex, iuv, lod) texelFetch(tex, ivec3(iuv), lod)

#define OGRE_GatherRed(tex, sampler, uv) textureGather(tex, uv, 0)
#define OGRE_GatherGreen(tex, sampler, uv) textureGather(tex, uv, 1)
#define OGRE_GatherBlue(tex, sampler, uv) textureGather(tex, uv, 2)

#define bufferFetch1(buffer, idx) texelFetch(buffer, idx).x

#define OGRE_SAMPLER_ARG_DECL(samplerName)
#define OGRE_SAMPLER_ARG(samplerName)

#define OGRE_Texture3D_float4 sampler3D
#define OGRE_OUT_REF(declType, variableName) out declType variableName
#define OGRE_INOUT_REF(declType, variableName) inout declType variableName

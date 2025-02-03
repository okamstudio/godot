/* clang-format off */
[vertex]

#ifdef USE_GLES_OVER_GL
#define lowp
#define mediump
#define highp
#else
precision highp float;
precision highp int;
#endif

attribute highp vec4 vertex_attrib; // attrib:0
/* clang-format on */

#if defined(USE_CUBEMAP) || defined(USE_PANORAMA)
attribute vec3 cube_in; // attrib:4
#else
attribute vec2 uv_in; // attrib:4
#endif

attribute vec2 uv2_in; // attrib:5

#if defined(USE_CUBEMAP) || defined(USE_PANORAMA)
varying vec3 cube_interp;
#else
varying vec2 uv_interp;
#endif
varying vec2 uv2_interp;

// These definitions are here because the shader-wrapper builder does
// not understand `#elif defined()`
#ifdef USE_DISPLAY_TRANSFORM
#endif

#ifdef USE_COPY_SECTION
uniform highp vec4 copy_section;
#elif defined(USE_DISPLAY_TRANSFORM)
uniform highp mat4 display_transform;
#endif

void main() {
#if defined(USE_CUBEMAP) || defined(USE_PANORAMA)
	cube_interp = cube_in;
#elif defined(USE_ASYM_PANO)
	uv_interp = vertex_attrib.xy;
#else
	uv_interp = uv_in;
#endif

	uv2_interp = uv2_in;
	gl_Position = vertex_attrib;

#ifdef USE_COPY_SECTION
	uv_interp = copy_section.xy + uv_interp * copy_section.zw;
	gl_Position.xy = (copy_section.xy + (gl_Position.xy * 0.5 + 0.5) * copy_section.zw) * 2.0 - 1.0;
#elif defined(USE_DISPLAY_TRANSFORM)
	uv_interp = (display_transform * vec4(uv_in, 1.0, 1.0)).xy;
#endif
}

/* clang-format off */
[fragment]

#define M_PI 3.14159265359

// texture2DLodEXT and textureCubeLodEXT are fragment shader specific.
// Do not copy these defines in the vertex section.
#ifndef USE_GLES_OVER_GL
#ifdef GL_EXT_shader_texture_lod
#extension GL_EXT_shader_texture_lod : enable
#define texture2DLod(img, coord, lod) texture2DLodEXT(img, coord, lod)
#define textureCubeLod(img, coord, lod) textureCubeLodEXT(img, coord, lod)
#endif
#endif // !USE_GLES_OVER_GL

#ifdef GL_ARB_shader_texture_lod
#extension GL_ARB_shader_texture_lod : enable
#endif

#if !defined(GL_EXT_shader_texture_lod) && !defined(GL_ARB_shader_texture_lod)
#define texture2DLod(img, coord, lod) texture2D(img, coord, lod)
#define textureCubeLod(img, coord, lod) textureCube(img, coord, lod)
#endif

#ifdef USE_GLES_OVER_GL
#define lowp
#define mediump
#define highp
#else
#if defined(USE_HIGHP_PRECISION)
precision highp float;
precision highp int;
#else
precision mediump float;
precision mediump int;
#endif
#endif

#if defined(USE_CUBEMAP) || defined(USE_PANORAMA)
varying highp vec3 cube_interp;
#else
varying vec2 uv_interp;
#endif
/* clang-format on */

#ifdef USE_ASYM_PANO
uniform highp mat4 pano_transform;
uniform highp vec4 asym_proj;
#endif

#ifdef USE_CUBEMAP
uniform samplerCube source_cube; // texunit:0
#else
uniform sampler2D source; // texunit:0
#endif

#ifdef SEP_CBCR_TEXTURE
uniform sampler2D CbCr; //texunit:1
#endif

varying vec2 uv2_interp;

#ifdef USE_MULTIPLIER
uniform float multiplier;
#endif

#ifdef USE_CUSTOM_ALPHA
uniform float custom_alpha;
#endif

#if defined(USE_PANORAMA) || defined(USE_ASYM_PANO)
uniform highp mat4 sky_transform;

vec4 texturePanorama(sampler2D pano, vec3 normal) {
	highp vec2 st = vec2(
			atan(normal.x, normal.z),
			acos(normal.y));

	if (st.x < 0.0)
		st.x += M_PI * 2.0;

	st /= vec2(M_PI * 2.0, M_PI);

	return texture2DLod(pano, st, 0.0);
}

#endif

void main() {
#ifdef USE_PANORAMA

	highp vec3 cube_normal = normalize(cube_interp);
	cube_normal.z = -cube_normal.z;
	cube_normal = mat3(sky_transform) * cube_normal;
	cube_normal.z = -cube_normal.z;

	vec4 color = texturePanorama(source, cube_normal);

#elif defined(USE_ASYM_PANO)

	// When an asymmetrical projection matrix is used (applicable for stereoscopic rendering i.e. VR) we need to do this calculation per fragment to get a perspective correct result.
	// Asymmetrical projection means the center of projection is no longer in the center of the screen but shifted.
	// The Matrix[2][0] (= asym_proj.x) and Matrix[2][1] (= asym_proj.z) values are what provide the right shift in the image.

	vec3 cube_normal;
	cube_normal.z = -1.0;
	cube_normal.x = (cube_normal.z * (-uv_interp.x - asym_proj.x)) / asym_proj.y;
	cube_normal.y = (cube_normal.z * (-uv_interp.y - asym_proj.z)) / asym_proj.a;
	cube_normal = mat3(sky_transform) * mat3(pano_transform) * cube_normal;
	cube_normal.z = -cube_normal.z;

	vec4 color = texturePanorama(source, normalize(cube_normal.xyz));

#elif defined(USE_CUBEMAP)
	vec4 color = textureCube(source_cube, normalize(cube_interp));
#elif defined(SEP_CBCR_TEXTURE)
	vec4 color;
	color.r = texture2D(source, uv_interp).r;
	color.gb = texture2D(CbCr, uv_interp).rg - vec2(0.5, 0.5);
	color.a = 1.0;
#else
	vec4 color = texture2D(source, uv_interp);
#endif

#ifdef YCBCR_TO_RGB
	// YCbCr -> RGB conversion

	// Using BT.601, which is the standard for SDTV is provided as a reference
	color.rgb = mat3(
						vec3(1.00000, 1.00000, 1.00000),
						vec3(0.00000, -0.34413, 1.77200),
						vec3(1.40200, -0.71414, 0.00000)) *
			color.rgb;
#endif

#ifdef USE_NO_ALPHA
	color.a = 1.0;
#endif

#ifdef USE_CUSTOM_ALPHA
	color.a = custom_alpha;
#endif

#ifdef USE_MULTIPLIER
	color.rgb *= multiplier;
#endif

#ifdef OUTPUT_LINEAR
	// sRGB -> linear
	color.rgb = mix(pow((color.rgb + vec3(0.055)) * (1.0 / (1.0 + 0.055)), vec3(2.4)), color.rgb * (1.0 / 12.92), vec3(lessThan(color.rgb, vec3(0.04045))));
#endif

	gl_FragColor = color;
}

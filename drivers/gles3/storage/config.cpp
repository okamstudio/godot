/**************************************************************************/
/*  config.cpp                                                            */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#ifdef GLES3_ENABLED

#include "config.h"

#include "../rasterizer_gles3.h"
#include "texture_storage.h"

#ifdef WEB_ENABLED
#include <emscripten/html5_webgl.h>
#endif

using namespace GLES3;

#define _GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT 0x84FF

Config *Config::singleton = nullptr;

Config::Config() {
	singleton = this;

#ifdef WEB_ENABLED
	// Starting with Emscripten 3.1.51, glGetStringi(GL_EXTENSIONS, i) will only ever return
	// a fixed list of extensions, regardless of what additional extensions are enabled. This
	// isn't very useful for us in determining which extensions we can rely on here. So, instead
	// we use emscripten_webgl_get_supported_extensions() to get all supported extensions, which
	// is what Emscripten 3.1.50 and earlier do.
	{
		char *extension_array_string = emscripten_webgl_get_supported_extensions();
		PackedStringArray extension_array = String((const char *)extension_array_string).split(" ");
		extensions.reserve(extension_array.size() * 2);
		for (const String &s : extension_array) {
			extensions.insert(s);
			extensions.insert("GL_" + s);
		}
		free(extension_array_string);
	}
#else
	{
		GLint max_extensions = 0;
		glGetIntegerv(GL_NUM_EXTENSIONS, &max_extensions);
		for (int i = 0; i < max_extensions; i++) {
			const GLubyte *s = glGetStringi(GL_EXTENSIONS, i);
			if (!s) {
				break;
			}
			extensions.insert((const char *)s);
		}
	}
#endif

	bptc_supported = extensions.has("GL_ARB_texture_compression_bptc") || extensions.has("EXT_texture_compression_bptc");
	astc_supported = extensions.has("GL_KHR_texture_compression_astc") || extensions.has("GL_OES_texture_compression_astc") || extensions.has("GL_KHR_texture_compression_astc_ldr") || extensions.has("GL_KHR_texture_compression_astc_hdr");
	astc_hdr_supported = extensions.has("GL_KHR_texture_compression_astc_ldr");
	astc_layered_supported = extensions.has("GL_KHR_texture_compression_astc_sliced_3d");

	if (RasterizerGLES3::is_gles_over_gl()) {
		float_texture_supported = true;
		etc2_supported = false;
		s3tc_supported = true;
		rgtc_supported = true; //RGTC - core since OpenGL version 3.0
		srgb_framebuffer_supported = true;
	} else {
		float_texture_supported = extensions.has("GL_EXT_color_buffer_float");
		etc2_supported = true;
#if defined(ANDROID_ENABLED) || defined(IOS_ENABLED)
		// Some Android devices report support for S3TC but we don't expect that and don't export the textures.
		// This could be fixed but so few devices support it that it doesn't seem useful (and makes bigger APKs).
		// For good measure we do the same hack for iOS, just in case.
		s3tc_supported = false;
#else
		s3tc_supported = extensions.has("GL_EXT_texture_compression_dxt1") || extensions.has("GL_EXT_texture_compression_s3tc") || extensions.has("WEBGL_compressed_texture_s3tc");
#endif
		rgtc_supported = extensions.has("GL_EXT_texture_compression_rgtc") || extensions.has("GL_ARB_texture_compression_rgtc") || extensions.has("EXT_texture_compression_rgtc");
		srgb_framebuffer_supported = extensions.has("GL_EXT_sRGB_write_control");
	}

	glGetIntegerv(GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS, &max_vertex_texture_image_units);
	glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &max_texture_image_units);
	glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max_texture_size);
	glGetIntegerv(GL_MAX_VIEWPORT_DIMS, max_viewport_size);
	glGetInteger64v(GL_MAX_UNIFORM_BLOCK_SIZE, &max_uniform_buffer_size);

	// sanity clamp buffer size to 16K..1MB
	max_uniform_buffer_size = CLAMP(max_uniform_buffer_size, 16384, 1048576);

	support_anisotropic_filter = extensions.has("GL_EXT_texture_filter_anisotropic");
	if (support_anisotropic_filter) {
		glGetFloatv(_GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &anisotropic_level);
		anisotropic_level = MIN(float(1 << int(GLOBAL_GET("rendering/textures/default_filters/anisotropic_filtering_level"))), anisotropic_level);
	}

	glGetIntegerv(GL_MAX_SAMPLES, &msaa_max_samples);
#ifdef WEB_ENABLED
	msaa_supported = (msaa_max_samples > 0);
#else
	msaa_supported = true;
#endif
#ifndef IOS_ENABLED
#ifdef WEB_ENABLED
	msaa_multiview_supported = extensions.has("OCULUS_multiview");
	rt_msaa_multiview_supported = msaa_multiview_supported;
#else
	msaa_multiview_supported = extensions.has("GL_EXT_multiview_texture_multisample");
#endif

	multiview_supported = extensions.has("OCULUS_multiview") || extensions.has("GL_OVR_multiview2") || extensions.has("GL_OVR_multiview");
#endif

#ifdef ANDROID_ENABLED
	// These are GLES only
	rt_msaa_supported = extensions.has("GL_EXT_multisampled_render_to_texture");
	rt_msaa_multiview_supported = extensions.has("GL_OVR_multiview_multisampled_render_to_texture");
	external_texture_supported = extensions.has("GL_OES_EGL_image_external_essl3");

	if (multiview_supported) {
		eglFramebufferTextureMultiviewOVR = (PFNGLFRAMEBUFFERTEXTUREMULTIVIEWOVRPROC)eglGetProcAddress("glFramebufferTextureMultiviewOVR");
		if (eglFramebufferTextureMultiviewOVR == nullptr) {
			multiview_supported = false;
		}
	}

	if (msaa_multiview_supported) {
		eglTexStorage3DMultisample = (PFNGLTEXSTORAGE3DMULTISAMPLEPROC)eglGetProcAddress("glTexStorage3DMultisample");
		if (eglTexStorage3DMultisample == nullptr) {
			msaa_multiview_supported = false;
		}
	}

	if (rt_msaa_supported) {
		eglFramebufferTexture2DMultisampleEXT = (PFNGLFRAMEBUFFERTEXTURE2DMULTISAMPLEEXTPROC)eglGetProcAddress("glFramebufferTexture2DMultisampleEXT");
		if (eglFramebufferTexture2DMultisampleEXT == nullptr) {
			rt_msaa_supported = false;
		}
	}

	if (rt_msaa_multiview_supported) {
		eglFramebufferTextureMultisampleMultiviewOVR = (PFNGLFRAMEBUFFERTEXTUREMULTISAMPLEMULTIVIEWOVRPROC)eglGetProcAddress("glFramebufferTextureMultisampleMultiviewOVR");
		if (eglFramebufferTextureMultisampleMultiviewOVR == nullptr) {
			rt_msaa_multiview_supported = false;
		}
	}

	if (external_texture_supported) {
		eglEGLImageTargetTexture2DOES = (PFNEGLIMAGETARGETTEXTURE2DOESPROC)eglGetProcAddress("glEGLImageTargetTexture2DOES");
		if (eglEGLImageTargetTexture2DOES == nullptr) {
			external_texture_supported = false;
		}
	}
#endif

	force_vertex_shading = GLOBAL_GET("rendering/shading/overrides/force_vertex_shading");
	use_nearest_mip_filter = GLOBAL_GET("rendering/textures/default_filters/use_nearest_mipmap_filter");

	use_depth_prepass = bool(GLOBAL_GET("rendering/driver/depth_prepass/enable"));
	if (use_depth_prepass) {
		String vendors = GLOBAL_GET("rendering/driver/depth_prepass/disable_for_vendors");
		Vector<String> vendor_match = vendors.split(",");
		const String &renderer = String::utf8((const char *)glGetString(GL_RENDERER));
		for (int i = 0; i < vendor_match.size(); i++) {
			String v = vendor_match[i].strip_edges();
			if (v == String()) {
				continue;
			}

			if (renderer.containsn(v)) {
				use_depth_prepass = false;
			}
		}
	}

	max_renderable_elements = GLOBAL_GET("rendering/limits/opengl/max_renderable_elements");
	max_renderable_lights = GLOBAL_GET("rendering/limits/opengl/max_renderable_lights");
	max_lights_per_object = GLOBAL_GET("rendering/limits/opengl/max_lights_per_object");

	//Adreno 3xx Compatibility
	const String rendering_device_name = String::utf8((const char *)glGetString(GL_RENDERER));
	if (rendering_device_name.left(13) == "Adreno (TM) 3") {
		flip_xy_workaround = true;
		disable_particles_workaround = true;

		// ignore driver version 331+
		const String gl_version = String::utf8((const char *)glGetString(GL_VERSION));
		// Adreno 3xx examples (https://opengles.gpuinfo.org/listreports.php):
		// ===========================================================================
		// OpenGL ES 3.0 V@84.0 AU@ (CL@)
		// OpenGL ES 3.0 V@127.0 AU@ (GIT@I96aee987eb)
		// OpenGL ES 3.0 V@140.0 AU@ (GIT@Ifd751822f5)
		// OpenGL ES 3.0 V@251.0 AU@08.00.00.312.030 (GIT@Ie4790512f3)
		// OpenGL ES 3.0 V@269.0 AU@ (GIT@I109c45a694)
		// OpenGL ES 3.0 V@331.0 (GIT@35e467f, Ice9844a736) (Date:04/15/19)
		// OpenGL ES 3.0 V@415.0 (GIT@d39f783, I79de86aa2c, 1591296226) (Date:06/04/20)
		// OpenGL ES 3.0 V@0502.0 (GIT@09fef447e8, I1fe547a144, 1661493934) (Date:08/25/22)
		String driver_version = gl_version.get_slice("V@", 1).get_slice(" ", 0);
		if (driver_version.is_valid_float() && driver_version.to_float() >= 331.0) {
			flip_xy_workaround = false;

			//TODO: also 'GPUParticles'?
			//https://github.com/godotengine/godot/issues/92662#issuecomment-2161199477
			//disable_particles_workaround = false;
		}
	} else if (rendering_device_name == "PowerVR Rogue GE8320") {
		disable_transform_feedback_shader_cache = true;
	}
}

Config::~Config() {
	singleton = nullptr;
}

#endif // GLES3_ENABLED

/**************************************************************************/
/*  config.h                                                              */
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

#ifndef CONFIG_GLES3_H
#define CONFIG_GLES3_H

#ifdef GLES3_ENABLED

#include "core/config/project_settings.h"
#include "core/string/ustring.h"
#include "core/templates/hash_set.h"

#include "platform_gl.h"

#ifdef ANDROID_ENABLED
typedef void (*PFNGLFRAMEBUFFERTEXTUREMULTIVIEWOVRPROC)(GLenum, GLenum, GLuint, GLint, GLint, GLsizei);
typedef void (*PFNGLTEXSTORAGE3DMULTISAMPLEPROC)(GLenum, GLsizei, GLenum, GLsizei, GLsizei, GLsizei, GLboolean);
typedef void (*PFNGLFRAMEBUFFERTEXTURE2DMULTISAMPLEEXTPROC)(GLenum, GLenum, GLenum, GLuint, GLint, GLsizei);
typedef void (*PFNGLFRAMEBUFFERTEXTUREMULTISAMPLEMULTIVIEWOVRPROC)(GLenum, GLenum, GLuint, GLint, GLsizei, GLint, GLsizei);
typedef void (*PFNEGLIMAGETARGETTEXTURE2DOESPROC)(GLenum, void *);
#endif

namespace GLES3 {

class Config {
private:
	static Config *singleton;

public:
	bool use_nearest_mip_filter = false;
	bool use_depth_prepass = true;

	GLint max_vertex_texture_image_units = 0;
	GLint max_texture_image_units = 0;
	GLint max_texture_size = 0;
	GLint max_viewport_size[2] = { 0, 0 };
	GLint64 max_uniform_buffer_size = 0;

	int64_t max_renderable_elements = 0;
	int64_t max_renderable_lights = 0;
	int64_t max_lights_per_object = 0;

	bool generate_wireframes = false;

	HashSet<String> extensions;

	bool float_texture_supported = false;
	bool s3tc_supported = false;
	bool rgtc_supported = false;
	bool bptc_supported = false;
	bool etc2_supported = false;
	bool astc_supported = false;
	bool astc_hdr_supported = false;
	bool astc_layered_supported = false;
	bool srgb_framebuffer_supported = false;

	bool force_vertex_shading = false;

	bool support_anisotropic_filter = false;
	float anisotropic_level = 0.0f;

	GLint msaa_max_samples = 0;
	bool msaa_supported = false;
	bool msaa_multiview_supported = false;
	bool rt_msaa_supported = false;
	bool rt_msaa_multiview_supported = false;
	bool multiview_supported = false;
	bool external_texture_supported = false;

	// Adreno 3XX compatibility.
	bool disable_particles_workaround = false; // Set to 'true' to disable 'GPUParticles'.
	bool flip_xy_workaround = false;

	// PowerVR GE 8320 workaround.
	bool disable_transform_feedback_shader_cache = false;

	// ANGLE shader workaround.
	bool polyfill_half2float = true;

#ifdef ANDROID_ENABLED
	PFNGLFRAMEBUFFERTEXTUREMULTIVIEWOVRPROC eglFramebufferTextureMultiviewOVR = nullptr;
	PFNGLTEXSTORAGE3DMULTISAMPLEPROC eglTexStorage3DMultisample = nullptr;
	PFNGLFRAMEBUFFERTEXTURE2DMULTISAMPLEEXTPROC eglFramebufferTexture2DMultisampleEXT = nullptr;
	PFNGLFRAMEBUFFERTEXTUREMULTISAMPLEMULTIVIEWOVRPROC eglFramebufferTextureMultisampleMultiviewOVR = nullptr;
	PFNEGLIMAGETARGETTEXTURE2DOESPROC eglEGLImageTargetTexture2DOES = nullptr;
#endif

	static Config *get_singleton() { return singleton; }

	Config();
	~Config();
};

} // namespace GLES3

#endif // GLES3_ENABLED

#endif // CONFIG_GLES3_H

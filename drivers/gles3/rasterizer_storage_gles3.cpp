/**************************************************************************/
/*  rasterizer_storage_gles3.cpp                                          */
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

#include "rasterizer_storage_gles3.h"

#include "core/engine.h"
#include "core/os/os.h"
#include "core/project_settings.h"
#include "core/threaded_callable_queue.h"
#include "main/main.h"
#include "rasterizer_canvas_gles3.h"
#include "rasterizer_scene_gles3.h"
#include "servers/visual/visual_server_canvas.h"
#include "servers/visual/visual_server_globals.h"
#include "servers/visual_server.h"

#if defined(IPHONE_ENABLED) || defined(ANDROID_ENABLED)
#include <dlfcn.h>
#endif

#ifdef TOOLS_ENABLED
#include "editor/editor_settings.h"
#endif

/* TEXTURE API */

#define _EXT_COMPRESSED_RGB_PVRTC_4BPPV1_IMG 0x8C00
#define _EXT_COMPRESSED_RGB_PVRTC_2BPPV1_IMG 0x8C01
#define _EXT_COMPRESSED_RGBA_PVRTC_4BPPV1_IMG 0x8C02
#define _EXT_COMPRESSED_RGBA_PVRTC_2BPPV1_IMG 0x8C03

#define _EXT_COMPRESSED_SRGB_PVRTC_2BPPV1_EXT 0x8A54
#define _EXT_COMPRESSED_SRGB_PVRTC_4BPPV1_EXT 0x8A55
#define _EXT_COMPRESSED_SRGB_ALPHA_PVRTC_2BPPV1_EXT 0x8A56
#define _EXT_COMPRESSED_SRGB_ALPHA_PVRTC_4BPPV1_EXT 0x8A57

#define _EXT_COMPRESSED_RGBA_S3TC_DXT1_EXT 0x83F1
#define _EXT_COMPRESSED_RGBA_S3TC_DXT3_EXT 0x83F2
#define _EXT_COMPRESSED_RGBA_S3TC_DXT5_EXT 0x83F3

#define _EXT_COMPRESSED_LUMINANCE_LATC1_EXT 0x8C70
#define _EXT_COMPRESSED_SIGNED_LUMINANCE_LATC1_EXT 0x8C71
#define _EXT_COMPRESSED_LUMINANCE_ALPHA_LATC2_EXT 0x8C72
#define _EXT_COMPRESSED_SIGNED_LUMINANCE_ALPHA_LATC2_EXT 0x8C73

#define _EXT_COMPRESSED_RED_RGTC1_EXT 0x8DBB
#define _EXT_COMPRESSED_RED_RGTC1 0x8DBB
#define _EXT_COMPRESSED_SIGNED_RED_RGTC1 0x8DBC
#define _EXT_COMPRESSED_RG_RGTC2 0x8DBD
#define _EXT_COMPRESSED_SIGNED_RG_RGTC2 0x8DBE
#define _EXT_COMPRESSED_SIGNED_RED_RGTC1_EXT 0x8DBC
#define _EXT_COMPRESSED_RED_GREEN_RGTC2_EXT 0x8DBD
#define _EXT_COMPRESSED_SIGNED_RED_GREEN_RGTC2_EXT 0x8DBE
#define _EXT_ETC1_RGB8_OES 0x8D64

#define _EXT_SLUMINANCE_NV 0x8C46
#define _EXT_SLUMINANCE_ALPHA_NV 0x8C44
#define _EXT_SRGB8_NV 0x8C41
#define _EXT_SLUMINANCE8_NV 0x8C47
#define _EXT_SLUMINANCE8_ALPHA8_NV 0x8C45

#define _EXT_COMPRESSED_SRGB_S3TC_DXT1_NV 0x8C4C
#define _EXT_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_NV 0x8C4D
#define _EXT_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_NV 0x8C4E
#define _EXT_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_NV 0x8C4F

#define _EXT_ATC_RGB_AMD 0x8C92
#define _EXT_ATC_RGBA_EXPLICIT_ALPHA_AMD 0x8C93
#define _EXT_ATC_RGBA_INTERPOLATED_ALPHA_AMD 0x87EE

#define _EXT_TEXTURE_CUBE_MAP_SEAMLESS 0x884F

#define _GL_TEXTURE_MAX_ANISOTROPY_EXT 0x84FE
#define _GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT 0x84FF

#define _EXT_COMPRESSED_R11_EAC 0x9270
#define _EXT_COMPRESSED_SIGNED_R11_EAC 0x9271
#define _EXT_COMPRESSED_RG11_EAC 0x9272
#define _EXT_COMPRESSED_SIGNED_RG11_EAC 0x9273
#define _EXT_COMPRESSED_RGB8_ETC2 0x9274
#define _EXT_COMPRESSED_SRGB8_ETC2 0x9275
#define _EXT_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2 0x9276
#define _EXT_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2 0x9277
#define _EXT_COMPRESSED_RGBA8_ETC2_EAC 0x9278
#define _EXT_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC 0x9279

#define _EXT_COMPRESSED_RGBA_BPTC_UNORM 0x8E8C
#define _EXT_COMPRESSED_SRGB_ALPHA_BPTC_UNORM 0x8E8D
#define _EXT_COMPRESSED_RGB_BPTC_SIGNED_FLOAT 0x8E8E
#define _EXT_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT 0x8E8F

#define _GL_TEXTURE_EXTERNAL_OES 0x8D65

#ifndef GLES_OVER_GL
#define glClearDepth glClearDepthf
#endif

void glTexStorage2DCustom(GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height, GLenum format, GLenum type) {
#ifdef GLES_OVER_GL

	for (int i = 0; i < levels; i++) {
		glTexImage2D(target, i, internalformat, width, height, 0, format, type, nullptr);
		width = MAX(1, (width / 2));
		height = MAX(1, (height / 2));
	}

#else
	glTexStorage2D(target, levels, internalformat, width, height);
#endif
}

GLuint RasterizerStorageGLES3::system_fbo = 0;

void RasterizerStorageGLES3::GLWrapper::initialize(int p_max_texture_image_units) {
	texture_unit_table.create(p_max_texture_image_units);
}

void RasterizerStorageGLES3::GLWrapper::reset() {
	for (uint32_t i = 0; i < texture_units_bound.size(); i++) {
		::glActiveTexture(GL_TEXTURE0 + texture_units_bound[i]);
		glBindTexture(GL_TEXTURE_2D, 0);
	}

	texture_units_bound.clear();
	texture_unit_table.blank();
}

int32_t RasterizerStorageGLES3::safe_gl_get_integer(unsigned int p_gl_param_name, int32_t p_max_accepted) {
	int64_t temp;
	glGetInteger64v(p_gl_param_name, &temp);
	temp = MIN(temp, (int64_t)p_max_accepted);
	return temp;
}

Ref<Image> RasterizerStorageGLES3::_get_gl_image_and_format(const Ref<Image> &p_image, Image::Format p_format, uint32_t p_flags, Image::Format &r_real_format, GLenum &r_gl_format, GLenum &r_gl_internal_format, GLenum &r_gl_type, bool &r_compressed, bool &r_srgb, bool p_force_decompress) const {
	r_compressed = false;
	r_gl_format = 0;
	r_real_format = p_format;
	Ref<Image> image = p_image;
	r_srgb = false;

	bool need_decompress = false;

	switch (p_format) {
		case Image::FORMAT_L8: {
#ifdef GLES_OVER_GL
			r_gl_internal_format = GL_R8;
			r_gl_format = GL_RED;
			r_gl_type = GL_UNSIGNED_BYTE;
#else
			r_gl_internal_format = GL_LUMINANCE;
			r_gl_format = GL_LUMINANCE;
			r_gl_type = GL_UNSIGNED_BYTE;
#endif
		} break;
		case Image::FORMAT_LA8: {
#ifdef GLES_OVER_GL
			r_gl_internal_format = GL_RG8;
			r_gl_format = GL_RG;
			r_gl_type = GL_UNSIGNED_BYTE;
#else
			r_gl_internal_format = GL_LUMINANCE_ALPHA;
			r_gl_format = GL_LUMINANCE_ALPHA;
			r_gl_type = GL_UNSIGNED_BYTE;
#endif
		} break;
		case Image::FORMAT_R8: {
			r_gl_internal_format = GL_R8;
			r_gl_format = GL_RED;
			r_gl_type = GL_UNSIGNED_BYTE;

		} break;
		case Image::FORMAT_RG8: {
			r_gl_internal_format = GL_RG8;
			r_gl_format = GL_RG;
			r_gl_type = GL_UNSIGNED_BYTE;

		} break;
		case Image::FORMAT_RGB8: {
			r_gl_internal_format = (config.srgb_decode_supported || (p_flags & VS::TEXTURE_FLAG_CONVERT_TO_LINEAR)) ? GL_SRGB8 : GL_RGB8;
			r_gl_format = GL_RGB;
			r_gl_type = GL_UNSIGNED_BYTE;
			r_srgb = true;

		} break;
		case Image::FORMAT_RGBA8: {
			r_gl_format = GL_RGBA;
			r_gl_internal_format = (config.srgb_decode_supported || (p_flags & VS::TEXTURE_FLAG_CONVERT_TO_LINEAR)) ? GL_SRGB8_ALPHA8 : GL_RGBA8;
			r_gl_type = GL_UNSIGNED_BYTE;
			r_srgb = true;

		} break;
		case Image::FORMAT_RGBA4444: {
			r_gl_internal_format = GL_RGBA4;
			r_gl_format = GL_RGBA;
			r_gl_type = GL_UNSIGNED_SHORT_4_4_4_4;

		} break;
		case Image::FORMAT_RGBA5551: {
			r_gl_internal_format = GL_RGB5_A1;
			r_gl_format = GL_RGBA;
			r_gl_type = GL_UNSIGNED_SHORT_5_5_5_1;

		} break;
		case Image::FORMAT_RF: {
			r_gl_internal_format = GL_R32F;
			r_gl_format = GL_RED;
			r_gl_type = GL_FLOAT;

		} break;
		case Image::FORMAT_RGF: {
			r_gl_internal_format = GL_RG32F;
			r_gl_format = GL_RG;
			r_gl_type = GL_FLOAT;

		} break;
		case Image::FORMAT_RGBF: {
			r_gl_internal_format = GL_RGB32F;
			r_gl_format = GL_RGB;
			r_gl_type = GL_FLOAT;

		} break;
		case Image::FORMAT_RGBAF: {
			r_gl_internal_format = GL_RGBA32F;
			r_gl_format = GL_RGBA;
			r_gl_type = GL_FLOAT;

		} break;
		case Image::FORMAT_RH: {
			r_gl_internal_format = GL_R16F;
			r_gl_format = GL_RED;
			r_gl_type = GL_HALF_FLOAT;
		} break;
		case Image::FORMAT_RGH: {
			r_gl_internal_format = GL_RG16F;
			r_gl_format = GL_RG;
			r_gl_type = GL_HALF_FLOAT;

		} break;
		case Image::FORMAT_RGBH: {
			r_gl_internal_format = GL_RGB16F;
			r_gl_format = GL_RGB;
			r_gl_type = GL_HALF_FLOAT;

		} break;
		case Image::FORMAT_RGBAH: {
			r_gl_internal_format = GL_RGBA16F;
			r_gl_format = GL_RGBA;
			r_gl_type = GL_HALF_FLOAT;

		} break;
		case Image::FORMAT_RGBE9995: {
			r_gl_internal_format = GL_RGB9_E5;
			r_gl_format = GL_RGB;
			r_gl_type = GL_UNSIGNED_INT_5_9_9_9_REV;

		} break;
		case Image::FORMAT_DXT1: {
			if (config.s3tc_supported) {
				r_gl_internal_format = (config.srgb_decode_supported || (p_flags & VS::TEXTURE_FLAG_CONVERT_TO_LINEAR)) ? _EXT_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_NV : _EXT_COMPRESSED_RGBA_S3TC_DXT1_EXT;
				r_gl_format = GL_RGBA;
				r_gl_type = GL_UNSIGNED_BYTE;
				r_compressed = true;
				r_srgb = true;

			} else {
				need_decompress = true;
			}

		} break;
		case Image::FORMAT_DXT3: {
			if (config.s3tc_supported) {
				r_gl_internal_format = (config.srgb_decode_supported || (p_flags & VS::TEXTURE_FLAG_CONVERT_TO_LINEAR)) ? _EXT_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_NV : _EXT_COMPRESSED_RGBA_S3TC_DXT3_EXT;
				r_gl_format = GL_RGBA;
				r_gl_type = GL_UNSIGNED_BYTE;
				r_compressed = true;
				r_srgb = true;

			} else {
				need_decompress = true;
			}

		} break;
		case Image::FORMAT_DXT5: {
			if (config.s3tc_supported) {
				r_gl_internal_format = (config.srgb_decode_supported || (p_flags & VS::TEXTURE_FLAG_CONVERT_TO_LINEAR)) ? _EXT_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_NV : _EXT_COMPRESSED_RGBA_S3TC_DXT5_EXT;
				r_gl_format = GL_RGBA;
				r_gl_type = GL_UNSIGNED_BYTE;
				r_compressed = true;
				r_srgb = true;

			} else {
				need_decompress = true;
			}

		} break;
		case Image::FORMAT_RGTC_R: {
			if (config.rgtc_supported) {
				r_gl_internal_format = _EXT_COMPRESSED_RED_RGTC1_EXT;
				r_gl_format = GL_RGBA;
				r_gl_type = GL_UNSIGNED_BYTE;
				r_compressed = true;

			} else {
				need_decompress = true;
			}

		} break;
		case Image::FORMAT_RGTC_RG: {
			if (config.rgtc_supported) {
				r_gl_internal_format = _EXT_COMPRESSED_RED_GREEN_RGTC2_EXT;
				r_gl_format = GL_RGBA;
				r_gl_type = GL_UNSIGNED_BYTE;
				r_compressed = true;
			} else {
				need_decompress = true;
			}

		} break;
		case Image::FORMAT_BPTC_RGBA: {
			if (config.bptc_supported) {
				r_gl_internal_format = (config.srgb_decode_supported || (p_flags & VS::TEXTURE_FLAG_CONVERT_TO_LINEAR)) ? _EXT_COMPRESSED_SRGB_ALPHA_BPTC_UNORM : _EXT_COMPRESSED_RGBA_BPTC_UNORM;
				r_gl_format = GL_RGBA;
				r_gl_type = GL_UNSIGNED_BYTE;
				r_compressed = true;
				r_srgb = true;

			} else {
				need_decompress = true;
			}
		} break;
		case Image::FORMAT_BPTC_RGBF: {
			if (config.bptc_supported) {
				r_gl_internal_format = _EXT_COMPRESSED_RGB_BPTC_SIGNED_FLOAT;
				r_gl_format = GL_RGB;
				r_gl_type = GL_FLOAT;
				r_compressed = true;
			} else {
				need_decompress = true;
			}
		} break;
		case Image::FORMAT_BPTC_RGBFU: {
			if (config.bptc_supported) {
				r_gl_internal_format = _EXT_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT;
				r_gl_format = GL_RGB;
				r_gl_type = GL_FLOAT;
				r_compressed = true;
			} else {
				need_decompress = true;
			}
		} break;
		case Image::FORMAT_PVRTC2: {
			if (config.pvrtc_supported) {
				r_gl_internal_format = (config.srgb_decode_supported || (p_flags & VS::TEXTURE_FLAG_CONVERT_TO_LINEAR)) ? _EXT_COMPRESSED_SRGB_PVRTC_2BPPV1_EXT : _EXT_COMPRESSED_RGB_PVRTC_2BPPV1_IMG;
				r_gl_format = GL_RGBA;
				r_gl_type = GL_UNSIGNED_BYTE;
				r_compressed = true;
				r_srgb = true;

			} else {
				need_decompress = true;
			}
		} break;
		case Image::FORMAT_PVRTC2A: {
			if (config.pvrtc_supported) {
				r_gl_internal_format = (config.srgb_decode_supported || (p_flags & VS::TEXTURE_FLAG_CONVERT_TO_LINEAR)) ? _EXT_COMPRESSED_SRGB_ALPHA_PVRTC_2BPPV1_EXT : _EXT_COMPRESSED_RGBA_PVRTC_2BPPV1_IMG;
				r_gl_format = GL_RGBA;
				r_gl_type = GL_UNSIGNED_BYTE;
				r_compressed = true;
				r_srgb = true;

			} else {
				need_decompress = true;
			}

		} break;
		case Image::FORMAT_PVRTC4: {
			if (config.pvrtc_supported) {
				r_gl_internal_format = (config.srgb_decode_supported || (p_flags & VS::TEXTURE_FLAG_CONVERT_TO_LINEAR)) ? _EXT_COMPRESSED_SRGB_PVRTC_4BPPV1_EXT : _EXT_COMPRESSED_RGB_PVRTC_4BPPV1_IMG;
				r_gl_format = GL_RGBA;
				r_gl_type = GL_UNSIGNED_BYTE;
				r_compressed = true;
				r_srgb = true;

			} else {
				need_decompress = true;
			}

		} break;
		case Image::FORMAT_PVRTC4A: {
			if (config.pvrtc_supported) {
				r_gl_internal_format = (config.srgb_decode_supported || (p_flags & VS::TEXTURE_FLAG_CONVERT_TO_LINEAR)) ? _EXT_COMPRESSED_SRGB_ALPHA_PVRTC_4BPPV1_EXT : _EXT_COMPRESSED_RGBA_PVRTC_4BPPV1_IMG;
				r_gl_format = GL_RGBA;
				r_gl_type = GL_UNSIGNED_BYTE;
				r_compressed = true;
				r_srgb = true;

			} else {
				need_decompress = true;
			}

		} break;
		case Image::FORMAT_ETC: {
			if (config.etc_supported) {
				r_gl_internal_format = _EXT_ETC1_RGB8_OES;
				r_gl_format = GL_RGBA;
				r_gl_type = GL_UNSIGNED_BYTE;
				r_compressed = true;

			} else {
				need_decompress = true;
			}

		} break;
		case Image::FORMAT_ETC2_R11: {
			if (config.etc2_supported) {
				r_gl_internal_format = _EXT_COMPRESSED_R11_EAC;
				r_gl_format = GL_RED;
				r_gl_type = GL_UNSIGNED_BYTE;
				r_compressed = true;

			} else {
				need_decompress = true;
			}
		} break;
		case Image::FORMAT_ETC2_R11S: {
			if (config.etc2_supported) {
				r_gl_internal_format = _EXT_COMPRESSED_SIGNED_R11_EAC;
				r_gl_format = GL_RED;
				r_gl_type = GL_UNSIGNED_BYTE;
				r_compressed = true;

			} else {
				need_decompress = true;
			}
		} break;
		case Image::FORMAT_ETC2_RG11: {
			if (config.etc2_supported) {
				r_gl_internal_format = _EXT_COMPRESSED_RG11_EAC;
				r_gl_format = GL_RG;
				r_gl_type = GL_UNSIGNED_BYTE;
				r_compressed = true;

			} else {
				need_decompress = true;
			}
		} break;
		case Image::FORMAT_ETC2_RG11S: {
			if (config.etc2_supported) {
				r_gl_internal_format = _EXT_COMPRESSED_SIGNED_RG11_EAC;
				r_gl_format = GL_RG;
				r_gl_type = GL_UNSIGNED_BYTE;
				r_compressed = true;

			} else {
				need_decompress = true;
			}
		} break;
		case Image::FORMAT_ETC2_RGB8: {
			if (config.etc2_supported) {
				r_gl_internal_format = (config.srgb_decode_supported || (p_flags & VS::TEXTURE_FLAG_CONVERT_TO_LINEAR)) ? _EXT_COMPRESSED_SRGB8_ETC2 : _EXT_COMPRESSED_RGB8_ETC2;
				r_gl_format = GL_RGB;
				r_gl_type = GL_UNSIGNED_BYTE;
				r_compressed = true;
				r_srgb = true;

			} else {
				need_decompress = true;
			}
		} break;
		case Image::FORMAT_ETC2_RGBA8: {
			if (config.etc2_supported) {
				r_gl_internal_format = (config.srgb_decode_supported || (p_flags & VS::TEXTURE_FLAG_CONVERT_TO_LINEAR)) ? _EXT_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC : _EXT_COMPRESSED_RGBA8_ETC2_EAC;
				r_gl_format = GL_RGBA;
				r_gl_type = GL_UNSIGNED_BYTE;
				r_compressed = true;
				r_srgb = true;

			} else {
				need_decompress = true;
			}
		} break;
		case Image::FORMAT_ETC2_RGB8A1: {
			if (config.etc2_supported) {
				r_gl_internal_format = (config.srgb_decode_supported || (p_flags & VS::TEXTURE_FLAG_CONVERT_TO_LINEAR)) ? _EXT_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2 : _EXT_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2;
				r_gl_format = GL_RGBA;
				r_gl_type = GL_UNSIGNED_BYTE;
				r_compressed = true;
				r_srgb = true;

			} else {
				need_decompress = true;
			}
		} break;
		default: {
			ERR_FAIL_V(Ref<Image>());
		}
	}

	if (need_decompress || p_force_decompress) {
		if (!image.is_null()) {
			image = image->duplicate();
			image->decompress();
			ERR_FAIL_COND_V(image->is_compressed(), image);
			image->convert(Image::FORMAT_RGBA8);
		}

		r_gl_format = GL_RGBA;
		r_gl_internal_format = (config.srgb_decode_supported || (p_flags & VS::TEXTURE_FLAG_CONVERT_TO_LINEAR)) ? GL_SRGB8_ALPHA8 : GL_RGBA8;
		r_gl_type = GL_UNSIGNED_BYTE;
		r_compressed = false;
		r_real_format = Image::FORMAT_RGBA8;
		r_srgb = true;

		return image;
	}

	return image;
}

static const GLenum _cube_side_enum[6] = {

	GL_TEXTURE_CUBE_MAP_NEGATIVE_X,
	GL_TEXTURE_CUBE_MAP_POSITIVE_X,
	GL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
	GL_TEXTURE_CUBE_MAP_POSITIVE_Y,
	GL_TEXTURE_CUBE_MAP_NEGATIVE_Z,
	GL_TEXTURE_CUBE_MAP_POSITIVE_Z,

};

RID RasterizerStorageGLES3::texture_create() {
	Texture *texture = memnew(Texture);
	ERR_FAIL_COND_V(!texture, RID());
	glGenTextures(1, &texture->tex_id);
	texture->active = false;
	texture->total_data_size = 0;

	return texture_owner.make_rid(texture);
}

void RasterizerStorageGLES3::texture_allocate(RID p_texture, int p_width, int p_height, int p_depth_3d, Image::Format p_format, VisualServer::TextureType p_type, uint32_t p_flags) {
	GLenum format;
	GLenum internal_format;
	GLenum type;

	bool compressed;
	bool srgb;

	if (p_flags & VS::TEXTURE_FLAG_USED_FOR_STREAMING) {
		p_flags &= ~VS::TEXTURE_FLAG_MIPMAPS; // no mipies for video
	}

#ifndef GLES_OVER_GL
	switch (p_format) {
		case Image::FORMAT_RF:
		case Image::FORMAT_RGF:
		case Image::FORMAT_RGBF:
		case Image::FORMAT_RGBAF:
		case Image::FORMAT_RH:
		case Image::FORMAT_RGH:
		case Image::FORMAT_RGBH:
		case Image::FORMAT_RGBAH: {
			if (!config.texture_float_linear_supported) {
				// disable linear texture filtering when not supported for float format on some devices (issue #24295)
				p_flags &= ~VS::TEXTURE_FLAG_FILTER;
			}
		} break;
		default: {
		}
	}
#endif

	Texture *texture = texture_owner.get(p_texture);
	ERR_FAIL_COND(!texture);
	texture->width = p_width;
	texture->height = p_height;
	texture->depth = p_depth_3d;
	texture->format = p_format;
	texture->flags = p_flags;
	texture->stored_cube_sides = 0;

	texture->type = p_type;

	switch (p_type) {
		case VS::TEXTURE_TYPE_2D: {
			texture->target = GL_TEXTURE_2D;
			texture->images.resize(1);
		} break;
		case VS::TEXTURE_TYPE_EXTERNAL: {
#ifdef ANDROID_ENABLED
			texture->target = _GL_TEXTURE_EXTERNAL_OES;
#else
			texture->target = GL_TEXTURE_2D;
#endif
			texture->images.resize(0);
		} break;
		case VS::TEXTURE_TYPE_CUBEMAP: {
			texture->target = GL_TEXTURE_CUBE_MAP;
			texture->images.resize(6);
		} break;
		case VS::TEXTURE_TYPE_2D_ARRAY: {
			texture->target = GL_TEXTURE_2D_ARRAY;
			texture->images.resize(p_depth_3d);
		} break;
		case VS::TEXTURE_TYPE_3D: {
			texture->target = GL_TEXTURE_3D;
			texture->images.resize(p_depth_3d);
		} break;
	}

	if (p_type != VS::TEXTURE_TYPE_EXTERNAL) {
		texture->is_npot_repeat_mipmap = false;
#ifdef JAVASCRIPT_ENABLED
		// WebGL 2.0 on browsers does not seem to properly support compressed non power-of-two (NPOT)
		// textures with repeat/mipmaps, even though NPOT textures should be supported as per the spec.
		// Force decompressing them to work it around on WebGL 2.0 at a performance cost (GH-33058).
		int po2_width = next_power_of_2(p_width);
		int po2_height = next_power_of_2(p_height);
		bool is_po2 = p_width == po2_width && p_height == po2_height;

		if (!is_po2 && (p_flags & VS::TEXTURE_FLAG_REPEAT || p_flags & VS::TEXTURE_FLAG_MIPMAPS)) {
			texture->is_npot_repeat_mipmap = true;
		}
#endif // JAVASCRIPT_ENABLED

		Image::Format real_format;
		_get_gl_image_and_format(Ref<Image>(),
				texture->format,
				texture->flags,
				real_format,
				format,
				internal_format,
				type,
				compressed,
				srgb,
				texture->is_npot_repeat_mipmap);

		texture->alloc_width = texture->width;
		texture->alloc_height = texture->height;
		texture->alloc_depth = texture->depth;

		texture->gl_format_cache = format;
		texture->gl_type_cache = type;
		texture->gl_internal_format_cache = internal_format;
		texture->compressed = compressed;
		texture->srgb = srgb;
		texture->data_size = 0;
		texture->mipmaps = 1;
	}

	gl_wrapper.gl_active_texture(GL_TEXTURE0);
	glBindTexture(texture->target, texture->tex_id);

	if (p_type == VS::TEXTURE_TYPE_EXTERNAL) {
		glTexParameteri(texture->target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(texture->target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(texture->target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(texture->target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	} else if (p_type == VS::TEXTURE_TYPE_3D || p_type == VS::TEXTURE_TYPE_2D_ARRAY) {
		int width = p_width;
		int height = p_height;
		int depth = p_depth_3d;

		int mipmaps = 0;

		while (width > 0 || height > 0 || (p_type == VS::TEXTURE_TYPE_3D && depth > 0)) {
			width = MAX(1, width);
			height = MAX(1, height);
			depth = MAX(1, depth);

			glTexImage3D(texture->target, mipmaps, internal_format, width, height, depth, 0, format, type, nullptr);

			width /= 2;
			height /= 2;

			if (p_type == VS::TEXTURE_TYPE_3D) {
				depth /= 2;
			}

			mipmaps++;

			if (!(p_flags & VS::TEXTURE_FLAG_MIPMAPS)) {
				break;
			}
		}

		glTexParameteri(texture->target, GL_TEXTURE_BASE_LEVEL, 0);
		glTexParameteri(texture->target, GL_TEXTURE_MAX_LEVEL, mipmaps - 1);

	} else if (p_flags & VS::TEXTURE_FLAG_USED_FOR_STREAMING) {
		//prealloc if video
		glTexImage2D(texture->target, 0, internal_format, p_width, p_height, 0, format, type, nullptr);
	}

	texture->active = true;
}

void RasterizerStorageGLES3::texture_set_data(RID p_texture, const Ref<Image> &p_image, int p_layer) {
	Texture *texture = texture_owner.get(p_texture);

	ERR_FAIL_COND(!texture);
	ERR_FAIL_COND(!texture->active);
	ERR_FAIL_COND(texture->render_target);
	ERR_FAIL_COND(texture->format != p_image->get_format());
	ERR_FAIL_COND(p_image.is_null());
	ERR_FAIL_COND(texture->type == VS::TEXTURE_TYPE_EXTERNAL);

	GLenum type;
	GLenum format;
	GLenum internal_format;
	bool compressed;
	bool srgb;

	if (config.keep_original_textures && !(texture->flags & VS::TEXTURE_FLAG_USED_FOR_STREAMING)) {
		texture->images.write[p_layer] = p_image;
	}

	Image::Format real_format;
	Ref<Image> img = _get_gl_image_and_format(p_image, p_image->get_format(), texture->flags, real_format, format, internal_format, type, compressed, srgb, texture->is_npot_repeat_mipmap);

	if (config.shrink_textures_x2 && (p_image->has_mipmaps() || !p_image->is_compressed()) && !(texture->flags & VS::TEXTURE_FLAG_USED_FOR_STREAMING)) {
		texture->alloc_height = MAX(1, texture->alloc_height / 2);
		texture->alloc_width = MAX(1, texture->alloc_width / 2);

		if (texture->alloc_width == img->get_width() / 2 && texture->alloc_height == img->get_height() / 2) {
			img->shrink_x2();
		} else if (img->get_format() <= Image::FORMAT_RGBA8) {
			img->resize(texture->alloc_width, texture->alloc_height, Image::INTERPOLATE_BILINEAR);
		}
	};

	GLenum blit_target = GL_TEXTURE_2D;

	switch (texture->type) {
		case VS::TEXTURE_TYPE_2D:
		case VS::TEXTURE_TYPE_EXTERNAL: {
			blit_target = GL_TEXTURE_2D;
		} break;
		case VS::TEXTURE_TYPE_CUBEMAP: {
			ERR_FAIL_INDEX(p_layer, 6);
			blit_target = _cube_side_enum[p_layer];
		} break;
		case VS::TEXTURE_TYPE_2D_ARRAY: {
			blit_target = GL_TEXTURE_2D_ARRAY;
		} break;
		case VS::TEXTURE_TYPE_3D: {
			blit_target = GL_TEXTURE_3D;
		} break;
	}

	texture->data_size = img->get_data().size();
	PoolVector<uint8_t>::Read read = img->get_data().read();
	ERR_FAIL_COND(!read.ptr());

	gl_wrapper.gl_active_texture(GL_TEXTURE0);
	glBindTexture(texture->target, texture->tex_id);

	texture->ignore_mipmaps = compressed && !img->has_mipmaps();

	if ((texture->flags & VS::TEXTURE_FLAG_MIPMAPS) && !texture->ignore_mipmaps) {
		if (texture->flags & VS::TEXTURE_FLAG_FILTER) {
			glTexParameteri(texture->target, GL_TEXTURE_MIN_FILTER, config.use_fast_texture_filter ? GL_LINEAR_MIPMAP_NEAREST : GL_LINEAR_MIPMAP_LINEAR);
		} else {
			glTexParameteri(texture->target, GL_TEXTURE_MIN_FILTER, config.use_fast_texture_filter ? GL_NEAREST_MIPMAP_NEAREST : GL_NEAREST_MIPMAP_LINEAR);
		}
	} else {
		if (texture->flags & VS::TEXTURE_FLAG_FILTER) {
			glTexParameteri(texture->target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		} else {
			glTexParameteri(texture->target, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		}
	}

	if (config.srgb_decode_supported && srgb) {
		if (texture->flags & VS::TEXTURE_FLAG_CONVERT_TO_LINEAR) {
			glTexParameteri(texture->target, _TEXTURE_SRGB_DECODE_EXT, _DECODE_EXT);
			texture->using_srgb = true;
		} else {
			glTexParameteri(texture->target, _TEXTURE_SRGB_DECODE_EXT, _SKIP_DECODE_EXT);
			texture->using_srgb = false;
		}
	}

	if (texture->flags & VS::TEXTURE_FLAG_FILTER) {
		glTexParameteri(texture->target, GL_TEXTURE_MAG_FILTER, GL_LINEAR); // Linear Filtering

	} else {
		glTexParameteri(texture->target, GL_TEXTURE_MAG_FILTER, GL_NEAREST); // raw Filtering
	}

	if (((texture->flags & VS::TEXTURE_FLAG_REPEAT) || (texture->flags & VS::TEXTURE_FLAG_MIRRORED_REPEAT)) && texture->target != GL_TEXTURE_CUBE_MAP) {
		if (texture->flags & VS::TEXTURE_FLAG_MIRRORED_REPEAT) {
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_MIRRORED_REPEAT);
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_MIRRORED_REPEAT);
		} else {
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		}
	} else {
		//glTexParameterf( texture->target, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE );
		glTexParameterf(texture->target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameterf(texture->target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	}

//set swizle for older format compatibility
#ifdef GLES_OVER_GL
	switch (texture->format) {
		case Image::FORMAT_L8: {
			glTexParameteri(texture->target, GL_TEXTURE_SWIZZLE_R, GL_RED);
			glTexParameteri(texture->target, GL_TEXTURE_SWIZZLE_G, GL_RED);
			glTexParameteri(texture->target, GL_TEXTURE_SWIZZLE_B, GL_RED);
			glTexParameteri(texture->target, GL_TEXTURE_SWIZZLE_A, GL_ONE);

		} break;
		case Image::FORMAT_LA8: {
			glTexParameteri(texture->target, GL_TEXTURE_SWIZZLE_R, GL_RED);
			glTexParameteri(texture->target, GL_TEXTURE_SWIZZLE_G, GL_RED);
			glTexParameteri(texture->target, GL_TEXTURE_SWIZZLE_B, GL_RED);
			glTexParameteri(texture->target, GL_TEXTURE_SWIZZLE_A, GL_GREEN);
		} break;
		default: {
			glTexParameteri(texture->target, GL_TEXTURE_SWIZZLE_R, GL_RED);
			glTexParameteri(texture->target, GL_TEXTURE_SWIZZLE_G, GL_GREEN);
			glTexParameteri(texture->target, GL_TEXTURE_SWIZZLE_B, GL_BLUE);
			glTexParameteri(texture->target, GL_TEXTURE_SWIZZLE_A, GL_ALPHA);

		} break;
	}
#endif
	if (config.use_anisotropic_filter) {
		if (texture->flags & VS::TEXTURE_FLAG_ANISOTROPIC_FILTER) {
			glTexParameterf(texture->target, _GL_TEXTURE_MAX_ANISOTROPY_EXT, config.anisotropic_level);
		} else {
			glTexParameterf(texture->target, _GL_TEXTURE_MAX_ANISOTROPY_EXT, 1);
		}
	}

	int mipmaps = ((texture->flags & VS::TEXTURE_FLAG_MIPMAPS) && img->has_mipmaps()) ? img->get_mipmap_count() + 1 : 1;

	int w = img->get_width();
	int h = img->get_height();

	int tsize = 0;

	for (int i = 0; i < mipmaps; i++) {
		int size, ofs;
		img->get_mipmap_offset_and_size(i, ofs, size);

		if (texture->type == VS::TEXTURE_TYPE_2D || texture->type == VS::TEXTURE_TYPE_CUBEMAP) {
			if (texture->compressed) {
				glPixelStorei(GL_UNPACK_ALIGNMENT, 4);

				int bw = w;
				int bh = h;

				glCompressedTexImage2D(blit_target, i, internal_format, bw, bh, 0, size, &read[ofs]);

			} else {
				glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
				if (texture->flags & VS::TEXTURE_FLAG_USED_FOR_STREAMING) {
					glTexSubImage2D(blit_target, i, 0, 0, w, h, format, type, &read[ofs]);
				} else {
					glTexImage2D(blit_target, i, internal_format, w, h, 0, format, type, &read[ofs]);
				}
			}
		} else {
			if (texture->compressed) {
				glPixelStorei(GL_UNPACK_ALIGNMENT, 4);

				int bw = w;
				int bh = h;

				glCompressedTexSubImage3D(blit_target, i, 0, 0, p_layer, bw, bh, 1, internal_format, size, &read[ofs]);
			} else {
				glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

				glTexSubImage3D(blit_target, i, 0, 0, p_layer, w, h, 1, format, type, &read[ofs]);
			}
		}
		tsize += size;

		w = MAX(1, w >> 1);
		h = MAX(1, h >> 1);
	}

	// Handle array and 3D textures, as those set their data per layer.
	tsize *= MAX(texture->alloc_depth, 1);

	info.texture_mem -= texture->total_data_size;
	texture->total_data_size = tsize;
	info.texture_mem += texture->total_data_size;

	//printf("texture: %i x %i - size: %i - total: %i\n",texture->width,texture->height,tsize,_rinfo.texture_mem);

	texture->stored_cube_sides |= (1 << p_layer);

	if ((texture->type == VS::TEXTURE_TYPE_2D || texture->type == VS::TEXTURE_TYPE_CUBEMAP) && (texture->flags & VS::TEXTURE_FLAG_MIPMAPS) && mipmaps == 1 && !texture->ignore_mipmaps && (texture->type != VS::TEXTURE_TYPE_CUBEMAP || texture->stored_cube_sides == (1 << 6) - 1)) {
		//generate mipmaps if they were requested and the image does not contain them
		glGenerateMipmap(texture->target);
	} else if (mipmaps > 1) {
		glTexParameteri(texture->target, GL_TEXTURE_BASE_LEVEL, 0);
		glTexParameteri(texture->target, GL_TEXTURE_MAX_LEVEL, mipmaps - 1);
	} else {
		glTexParameteri(texture->target, GL_TEXTURE_BASE_LEVEL, 0);
		glTexParameteri(texture->target, GL_TEXTURE_MAX_LEVEL, 0);
	}

	texture->mipmaps = mipmaps;

	//texture_set_flags(p_texture,texture->flags);
}

// Uploads pixel data to a sub-region of a texture, for the specified mipmap.
// The texture pixels must have been allocated before, because most features seen in texture_set_data() make no sense in a partial update.
// TODO If we want this to be usable without pre-filling pixels with a full image, we have to call glTexImage2D() with null data.
void RasterizerStorageGLES3::texture_set_data_partial(RID p_texture, const Ref<Image> &p_image, int src_x, int src_y, int src_w, int src_h, int dst_x, int dst_y, int p_dst_mip, int p_layer) {
	Texture *texture = texture_owner.get(p_texture);

	ERR_FAIL_COND(!texture);
	ERR_FAIL_COND(!texture->active);
	ERR_FAIL_COND(texture->render_target);
	ERR_FAIL_COND(texture->format != p_image->get_format());
	ERR_FAIL_COND(p_image.is_null());
	ERR_FAIL_COND(src_w <= 0 || src_h <= 0);
	ERR_FAIL_COND(src_x < 0 || src_y < 0 || src_x + src_w > p_image->get_width() || src_y + src_h > p_image->get_height());
	ERR_FAIL_COND(dst_x < 0 || dst_y < 0 || dst_x + src_w > texture->alloc_width || dst_y + src_h > texture->alloc_height);
	ERR_FAIL_COND(p_dst_mip < 0 || p_dst_mip >= texture->mipmaps);
	ERR_FAIL_COND(texture->type == VS::TEXTURE_TYPE_EXTERNAL);

	GLenum type;
	GLenum format;
	GLenum internal_format;
	bool compressed;
	bool srgb;

	// Because OpenGL wants data as a dense array, we have to extract the sub-image if the source rect isn't the full image
	Ref<Image> p_sub_img = p_image;
	if (src_x > 0 || src_y > 0 || src_w != p_image->get_width() || src_h != p_image->get_height()) {
		p_sub_img = p_image->get_rect(Rect2(src_x, src_y, src_w, src_h));
	}

	Image::Format real_format;
	Ref<Image> img = _get_gl_image_and_format(p_sub_img, p_sub_img->get_format(), texture->flags, real_format, format, internal_format, type, compressed, srgb, texture->is_npot_repeat_mipmap);

	GLenum blit_target = GL_TEXTURE_2D;

	switch (texture->type) {
		case VS::TEXTURE_TYPE_2D:
		case VS::TEXTURE_TYPE_EXTERNAL: {
			blit_target = GL_TEXTURE_2D;
		} break;
		case VS::TEXTURE_TYPE_CUBEMAP: {
			ERR_FAIL_INDEX(p_layer, 6);
			blit_target = _cube_side_enum[p_layer];
		} break;
		case VS::TEXTURE_TYPE_2D_ARRAY: {
			blit_target = GL_TEXTURE_2D_ARRAY;
		} break;
		case VS::TEXTURE_TYPE_3D: {
			blit_target = GL_TEXTURE_3D;
		} break;
	}

	PoolVector<uint8_t>::Read read = img->get_data().read();

	gl_wrapper.gl_active_texture(GL_TEXTURE0);
	glBindTexture(texture->target, texture->tex_id);

	int src_data_size = img->get_data().size();
	int src_ofs = 0;

	if (texture->type == VS::TEXTURE_TYPE_2D || texture->type == VS::TEXTURE_TYPE_CUBEMAP) {
		if (texture->compressed) {
			glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
			glCompressedTexSubImage2D(blit_target, p_dst_mip, dst_x, dst_y, src_w, src_h, internal_format, src_data_size, &read[src_ofs]);

		} else {
			glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
			// `format` has to match the internal_format used when the texture was created
			glTexSubImage2D(blit_target, p_dst_mip, dst_x, dst_y, src_w, src_h, format, type, &read[src_ofs]);
		}
	} else {
		if (texture->compressed) {
			glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
			glCompressedTexSubImage3D(blit_target, p_dst_mip, dst_x, dst_y, p_layer, src_w, src_h, 1, format, src_data_size, &read[src_ofs]);
		} else {
			glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
			// `format` has to match the internal_format used when the texture was created
			glTexSubImage3D(blit_target, p_dst_mip, dst_x, dst_y, p_layer, src_w, src_h, 1, format, type, &read[src_ofs]);
		}
	}

	if (texture->flags & VS::TEXTURE_FLAG_FILTER) {
		glTexParameteri(texture->target, GL_TEXTURE_MAG_FILTER, GL_LINEAR); // Linear Filtering

	} else {
		glTexParameteri(texture->target, GL_TEXTURE_MAG_FILTER, GL_NEAREST); // raw Filtering
	}
}

Ref<Image> RasterizerStorageGLES3::texture_get_data(RID p_texture, int p_layer) const {
	Texture *texture = texture_owner.get(p_texture);

	ERR_FAIL_COND_V(!texture, Ref<Image>());
	ERR_FAIL_COND_V(!texture->active, Ref<Image>());
	ERR_FAIL_COND_V(texture->data_size == 0 && !texture->render_target, Ref<Image>());

	if (texture->type == VS::TEXTURE_TYPE_CUBEMAP && p_layer < 6 && !texture->images[p_layer].is_null()) {
		return texture->images[p_layer];
	}

	// 3D textures and 2D texture arrays need special treatment, as the glGetTexImage reads **the whole**
	// texture to host-memory. 3D textures and 2D texture arrays are potentially very big, so reading
	// everything just to throw everything but one layer away is A Bad Idea.
	//
	// Unfortunately, to solve this, the copy shader has to read the data out via a shader and store it
	// in a temporary framebuffer. The data from the framebuffer can then be read using glReadPixels.
	if (texture->type == VS::TEXTURE_TYPE_2D_ARRAY || texture->type == VS::TEXTURE_TYPE_3D) {
		// can't read a layer that doesn't exist
		ERR_FAIL_INDEX_V(p_layer, texture->alloc_depth, Ref<Image>());

		// get some information about the texture
		Image::Format real_format;
		GLenum gl_format;
		GLenum gl_internal_format;
		GLenum gl_type;

		bool compressed;
		bool srgb;

		_get_gl_image_and_format(
				Ref<Image>(),
				texture->format,
				texture->flags,
				real_format,
				gl_format,
				gl_internal_format,
				gl_type,
				compressed,
				srgb,
				texture->is_npot_repeat_mipmap);

		PoolVector<uint8_t> data;

		// TODO need to decide between RgbaUnorm and RgbaFloat32 for output
		int data_size = Image::get_image_data_size(texture->alloc_width, texture->alloc_height, Image::FORMAT_RGBA8, false);

		data.resize(data_size * 2); // add some more memory at the end, just in case for buggy drivers
		PoolVector<uint8_t>::Write wb = data.write();

		// generate temporary resources
		GLuint tmp_fbo;
		glGenFramebuffers(1, &tmp_fbo);

		GLuint tmp_color_attachment;
		glGenTextures(1, &tmp_color_attachment);

		// now bring the OpenGL context into the correct state
		{
			glBindFramebuffer(GL_FRAMEBUFFER, tmp_fbo);

			// back color attachment with memory, then set properties
			gl_wrapper.gl_active_texture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, tmp_color_attachment);
			// TODO support HDR properly
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, texture->alloc_width, texture->alloc_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

			// use the color texture as color attachment for this render pass
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tmp_color_attachment, 0);

			// more GL state, wheeeey
			glDepthMask(GL_FALSE);
			glDisable(GL_DEPTH_TEST);
			glDisable(GL_CULL_FACE);
			glDisable(GL_BLEND);
			glDepthFunc(GL_LEQUAL);
			glColorMask(1, 1, 1, 1);

			// use volume tex for reading
			gl_wrapper.gl_active_texture(GL_TEXTURE0);
			glBindTexture(texture->target, texture->tex_id);

			glViewport(0, 0, texture->alloc_width, texture->alloc_height);

			// set up copy shader for proper use
			shaders.copy.set_conditional(CopyShaderGLES3::LINEAR_TO_SRGB, !srgb);
			shaders.copy.set_conditional(CopyShaderGLES3::USE_TEXTURE3D, texture->type == VS::TEXTURE_TYPE_3D);
			shaders.copy.set_conditional(CopyShaderGLES3::USE_TEXTURE2DARRAY, texture->type == VS::TEXTURE_TYPE_2D_ARRAY);
			shaders.copy.bind();

			float layer;
			if (texture->type == VS::TEXTURE_TYPE_2D_ARRAY) {
				layer = (float)p_layer;
			} else {
				// calculate the normalized z coordinate for the layer
				layer = (float)p_layer / (float)texture->alloc_depth;
			}

			shaders.copy.set_uniform(CopyShaderGLES3::LAYER, layer);

			glBindVertexArray(resources.quadie_array);
		}

		// clear color attachment, then perform copy
		glClearColor(0.0, 0.0, 0.0, 0.0);
		glClear(GL_COLOR_BUFFER_BIT);

		glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

		// read the image into the host buffer
		glReadPixels(0, 0, texture->alloc_width, texture->alloc_height, GL_RGBA, GL_UNSIGNED_BYTE, &wb[0]);

		// remove temp resources and unset some GL state
		{
			shaders.copy.set_conditional(CopyShaderGLES3::USE_TEXTURE3D, false);
			shaders.copy.set_conditional(CopyShaderGLES3::USE_TEXTURE2DARRAY, false);
			shaders.copy.set_conditional(CopyShaderGLES3::LINEAR_TO_SRGB, false);

			glBindFramebuffer(GL_FRAMEBUFFER, 0);

			glDeleteTextures(1, &tmp_color_attachment);
			glDeleteFramebuffers(1, &tmp_fbo);
		}

		wb.release();

		data.resize(data_size);

		Image *img = memnew(Image(texture->alloc_width, texture->alloc_height, false, Image::FORMAT_RGBA8, data));
		if (!texture->compressed) {
			img->convert(real_format);
		}

		return Ref<Image>(img);
	}

#ifdef GLES_OVER_GL

	Image::Format real_format;
	GLenum gl_format;
	GLenum gl_internal_format;
	GLenum gl_type;
	bool compressed;
	bool srgb;
	_get_gl_image_and_format(Ref<Image>(), texture->format, texture->flags, real_format, gl_format, gl_internal_format, gl_type, compressed, srgb, false);

	PoolVector<uint8_t> data;

	int data_size = Image::get_image_data_size(texture->alloc_width, texture->alloc_height, real_format, texture->mipmaps > 1);

	data.resize(data_size * 2); //add some memory at the end, just in case for buggy drivers
	PoolVector<uint8_t>::Write wb = data.write();

	gl_wrapper.gl_active_texture(GL_TEXTURE0);

	glBindTexture(texture->target, texture->tex_id);

	glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);

	for (int i = 0; i < texture->mipmaps; i++) {
		int ofs = Image::get_image_mipmap_offset(texture->alloc_width, texture->alloc_height, real_format, i);

		if (texture->compressed) {
			glPixelStorei(GL_PACK_ALIGNMENT, 4);
			glGetCompressedTexImage(texture->target, i, &wb[ofs]);

		} else {
			glPixelStorei(GL_PACK_ALIGNMENT, 1);

			glGetTexImage(texture->target, i, texture->gl_format_cache, texture->gl_type_cache, &wb[ofs]);
		}
	}

	Image::Format img_format;

	//convert special case RGB10_A2 to RGBA8 because it's not a supported image format
	if (texture->gl_internal_format_cache == GL_RGB10_A2) {
		img_format = Image::FORMAT_RGBA8;

		uint32_t *ptr = (uint32_t *)wb.ptr();
		uint32_t num_pixels = data_size / 4;

		for (uint32_t ofs = 0; ofs < num_pixels; ofs++) {
			uint32_t px = ptr[ofs];
			uint32_t a = px >> 30 & 0xFF;

			ptr[ofs] = (px >> 2 & 0xFF) |
					(px >> 12 & 0xFF) << 8 |
					(px >> 22 & 0xFF) << 16 |
					(a | a << 2 | a << 4 | a << 6) << 24;
		}
	} else {
		img_format = real_format;
	}

	wb.release();

	data.resize(data_size);

	Image *img = memnew(Image(texture->alloc_width, texture->alloc_height, texture->mipmaps > 1, img_format, data));

	return Ref<Image>(img);
#else

	Image::Format real_format;
	GLenum gl_format;
	GLenum gl_internal_format;
	GLenum gl_type;
	bool compressed;
	bool srgb;
	_get_gl_image_and_format(Ref<Image>(), texture->format, texture->flags, real_format, gl_format, gl_internal_format, gl_type, compressed, srgb, texture->is_npot_repeat_mipmap);

	PoolVector<uint8_t> data;

	int data_size = Image::get_image_data_size(texture->alloc_width, texture->alloc_height, Image::FORMAT_RGBA8, false);

	data.resize(data_size * 2); //add some memory at the end, just in case for buggy drivers
	PoolVector<uint8_t>::Write wb = data.write();

	GLuint temp_framebuffer;
	glGenFramebuffers(1, &temp_framebuffer);

	GLuint temp_color_texture;
	glGenTextures(1, &temp_color_texture);

	glBindFramebuffer(GL_FRAMEBUFFER, temp_framebuffer);

	glBindTexture(GL_TEXTURE_2D, temp_color_texture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, texture->alloc_width, texture->alloc_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, temp_color_texture, 0);

	glDepthMask(GL_FALSE);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	glDisable(GL_BLEND);
	glDepthFunc(GL_LEQUAL);
	glColorMask(1, 1, 1, 1);
	gl_wrapper.gl_active_texture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, texture->tex_id);

	glViewport(0, 0, texture->alloc_width, texture->alloc_height);

	shaders.copy.set_conditional(CopyShaderGLES3::LINEAR_TO_SRGB, !srgb);
	shaders.copy.bind();

	glClearColor(0.0, 0.0, 0.0, 0.0);
	glClear(GL_COLOR_BUFFER_BIT);
	glBindVertexArray(resources.quadie_array);
	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
	glBindVertexArray(0);

	glReadPixels(0, 0, texture->alloc_width, texture->alloc_height, GL_RGBA, GL_UNSIGNED_BYTE, &wb[0]);

	shaders.copy.set_conditional(CopyShaderGLES3::LINEAR_TO_SRGB, false);

	glDeleteTextures(1, &temp_color_texture);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glDeleteFramebuffers(1, &temp_framebuffer);

	wb.release();

	data.resize(data_size);

	Image *img = memnew(Image(texture->alloc_width, texture->alloc_height, false, Image::FORMAT_RGBA8, data));
	if (!texture->compressed) {
		img->convert(real_format);
	}

	return Ref<Image>(img);
#endif
}

void RasterizerStorageGLES3::texture_set_flags(RID p_texture, uint32_t p_flags) {
	Texture *texture = texture_owner.get(p_texture);
	ERR_FAIL_COND(!texture);
	if (texture->render_target) {
		// only allow filter and repeat flags for render target (ie. viewport) textures
		p_flags &= (VS::TEXTURE_FLAG_FILTER | VS::TEXTURE_FLAG_REPEAT);
	}

	bool had_mipmaps = texture->flags & VS::TEXTURE_FLAG_MIPMAPS;

	texture->flags = p_flags;

	gl_wrapper.gl_active_texture(GL_TEXTURE0);
	glBindTexture(texture->target, texture->tex_id);

	if (((texture->flags & VS::TEXTURE_FLAG_REPEAT) || (texture->flags & VS::TEXTURE_FLAG_MIRRORED_REPEAT)) && texture->target != GL_TEXTURE_CUBE_MAP) {
		if (texture->flags & VS::TEXTURE_FLAG_MIRRORED_REPEAT) {
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_MIRRORED_REPEAT);
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_MIRRORED_REPEAT);
		} else {
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		}
	} else {
		//glTexParameterf( texture->target, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE );
		glTexParameterf(texture->target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameterf(texture->target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	}

	if (config.use_anisotropic_filter) {
		if (texture->flags & VS::TEXTURE_FLAG_ANISOTROPIC_FILTER) {
			glTexParameterf(texture->target, _GL_TEXTURE_MAX_ANISOTROPY_EXT, config.anisotropic_level);
		} else {
			glTexParameterf(texture->target, _GL_TEXTURE_MAX_ANISOTROPY_EXT, 1);
		}
	}

	if ((texture->flags & VS::TEXTURE_FLAG_MIPMAPS) && !texture->ignore_mipmaps) {
		if (!had_mipmaps && texture->mipmaps == 1) {
			glGenerateMipmap(texture->target);
		}
		if (texture->flags & VS::TEXTURE_FLAG_FILTER) {
			glTexParameteri(texture->target, GL_TEXTURE_MIN_FILTER, config.use_fast_texture_filter ? GL_LINEAR_MIPMAP_NEAREST : GL_LINEAR_MIPMAP_LINEAR);
		} else {
			glTexParameteri(texture->target, GL_TEXTURE_MIN_FILTER, config.use_fast_texture_filter ? GL_NEAREST_MIPMAP_NEAREST : GL_NEAREST_MIPMAP_LINEAR);
		}

	} else {
		if (texture->flags & VS::TEXTURE_FLAG_FILTER) {
			glTexParameteri(texture->target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		} else {
			glTexParameteri(texture->target, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		}
	}

	if (config.srgb_decode_supported && texture->srgb) {
		if (texture->flags & VS::TEXTURE_FLAG_CONVERT_TO_LINEAR) {
			glTexParameteri(texture->target, _TEXTURE_SRGB_DECODE_EXT, _DECODE_EXT);
			texture->using_srgb = true;
		} else {
			glTexParameteri(texture->target, _TEXTURE_SRGB_DECODE_EXT, _SKIP_DECODE_EXT);
			texture->using_srgb = false;
		}
	}

	if (texture->flags & VS::TEXTURE_FLAG_FILTER) {
		glTexParameteri(texture->target, GL_TEXTURE_MAG_FILTER, GL_LINEAR); // Linear Filtering

	} else {
		glTexParameteri(texture->target, GL_TEXTURE_MAG_FILTER, GL_NEAREST); // raw Filtering
	}
}
uint32_t RasterizerStorageGLES3::texture_get_flags(RID p_texture) const {
	Texture *texture = texture_owner.get(p_texture);

	ERR_FAIL_COND_V(!texture, 0);

	return texture->flags;
}
Image::Format RasterizerStorageGLES3::texture_get_format(RID p_texture) const {
	Texture *texture = texture_owner.get(p_texture);

	ERR_FAIL_COND_V(!texture, Image::FORMAT_L8);

	return texture->format;
}

VisualServer::TextureType RasterizerStorageGLES3::texture_get_type(RID p_texture) const {
	Texture *texture = texture_owner.get(p_texture);

	ERR_FAIL_COND_V(!texture, VS::TEXTURE_TYPE_2D);

	return texture->type;
}
uint32_t RasterizerStorageGLES3::texture_get_texid(RID p_texture) const {
	Texture *texture = texture_owner.get(p_texture);

	ERR_FAIL_COND_V(!texture, 0);

	return texture->tex_id;
}
void RasterizerStorageGLES3::texture_bind(RID p_texture, uint32_t p_texture_no) {
	Texture *texture = texture_owner.getornull(p_texture);

	ERR_FAIL_COND(!texture);

	gl_wrapper.gl_active_texture(GL_TEXTURE0 + p_texture_no);
	glBindTexture(texture->target, texture->tex_id);
}
uint32_t RasterizerStorageGLES3::texture_get_width(RID p_texture) const {
	Texture *texture = texture_owner.get(p_texture);

	ERR_FAIL_COND_V(!texture, 0);

	return texture->width;
}
uint32_t RasterizerStorageGLES3::texture_get_height(RID p_texture) const {
	Texture *texture = texture_owner.get(p_texture);

	ERR_FAIL_COND_V(!texture, 0);

	return texture->height;
}

uint32_t RasterizerStorageGLES3::texture_get_depth(RID p_texture) const {
	Texture *texture = texture_owner.get(p_texture);

	ERR_FAIL_COND_V(!texture, 0);

	return texture->depth;
}

void RasterizerStorageGLES3::texture_set_size_override(RID p_texture, int p_width, int p_height, int p_depth) {
	Texture *texture = texture_owner.get(p_texture);

	ERR_FAIL_COND(!texture);
	ERR_FAIL_COND(texture->render_target);

	ERR_FAIL_COND(p_width <= 0 || p_width > 16384);
	ERR_FAIL_COND(p_height <= 0 || p_height > 16384);
	//real texture size is in alloc width and height
	texture->width = p_width;
	texture->height = p_height;
}

void RasterizerStorageGLES3::texture_set_path(RID p_texture, const String &p_path) {
	Texture *texture = texture_owner.get(p_texture);
	ERR_FAIL_COND(!texture);

	texture->path = p_path;
}

String RasterizerStorageGLES3::texture_get_path(RID p_texture) const {
	Texture *texture = texture_owner.get(p_texture);
	ERR_FAIL_COND_V(!texture, String());
	return texture->path;
}
void RasterizerStorageGLES3::texture_debug_usage(List<VS::TextureInfo> *r_info) {
	List<RID> textures;
	texture_owner.get_owned_list(&textures);

	for (List<RID>::Element *E = textures.front(); E; E = E->next()) {
		Texture *t = texture_owner.get(E->get());
		if (!t) {
			continue;
		}
		VS::TextureInfo tinfo;
		tinfo.texture = E->get();
		tinfo.path = t->path;
		tinfo.format = t->format;
		tinfo.width = t->alloc_width;
		tinfo.height = t->alloc_height;
		tinfo.depth = t->alloc_depth;
		tinfo.bytes = t->total_data_size;
		r_info->push_back(tinfo);
	}
}

void RasterizerStorageGLES3::texture_set_shrink_all_x2_on_set_data(bool p_enable) {
	config.shrink_textures_x2 = p_enable;
}

void RasterizerStorageGLES3::textures_keep_original(bool p_enable) {
	config.keep_original_textures = p_enable;
}

void RasterizerStorageGLES3::texture_set_detect_3d_callback(RID p_texture, VisualServer::TextureDetectCallback p_callback, void *p_userdata) {
	Texture *texture = texture_owner.get(p_texture);
	ERR_FAIL_COND(!texture);

	texture->detect_3d = p_callback;
	texture->detect_3d_ud = p_userdata;
}

void RasterizerStorageGLES3::texture_set_detect_srgb_callback(RID p_texture, VisualServer::TextureDetectCallback p_callback, void *p_userdata) {
	Texture *texture = texture_owner.get(p_texture);
	ERR_FAIL_COND(!texture);

	texture->detect_srgb = p_callback;
	texture->detect_srgb_ud = p_userdata;
}

void RasterizerStorageGLES3::texture_set_detect_normal_callback(RID p_texture, VisualServer::TextureDetectCallback p_callback, void *p_userdata) {
	Texture *texture = texture_owner.get(p_texture);
	ERR_FAIL_COND(!texture);

	texture->detect_normal = p_callback;
	texture->detect_normal_ud = p_userdata;
}

RID RasterizerStorageGLES3::texture_create_radiance_cubemap(RID p_source, int p_resolution) const {
	Texture *texture = texture_owner.get(p_source);
	ERR_FAIL_COND_V(!texture, RID());
	ERR_FAIL_COND_V(texture->type != VS::TEXTURE_TYPE_CUBEMAP, RID());

	bool use_float = config.framebuffer_half_float_supported;

	if (p_resolution < 0) {
		p_resolution = texture->width;
	}

	glBindVertexArray(0);
	glDisable(GL_CULL_FACE);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_SCISSOR_TEST);
	glDisable(GL_BLEND);

	gl_wrapper.gl_active_texture(GL_TEXTURE0);
	glBindTexture(texture->target, texture->tex_id);

	if (config.srgb_decode_supported && texture->srgb && !texture->using_srgb) {
		glTexParameteri(texture->target, _TEXTURE_SRGB_DECODE_EXT, _DECODE_EXT);
		texture->using_srgb = true;
#ifdef TOOLS_ENABLED
		if (!(texture->flags & VS::TEXTURE_FLAG_CONVERT_TO_LINEAR)) {
			texture->flags |= VS::TEXTURE_FLAG_CONVERT_TO_LINEAR;
			//notify that texture must be set to linear beforehand, so it works in other platforms when exported
		}
#endif
	}

	gl_wrapper.gl_active_texture(GL_TEXTURE1);
	GLuint new_cubemap;
	glGenTextures(1, &new_cubemap);
	glBindTexture(GL_TEXTURE_CUBE_MAP, new_cubemap);

	GLuint tmp_fb;

	glGenFramebuffers(1, &tmp_fb);
	glBindFramebuffer(GL_FRAMEBUFFER, tmp_fb);

	int size = p_resolution;

	int lod = 0;

	shaders.cubemap_filter.bind();

	int mipmaps = 6;

	int mm_level = mipmaps;

	GLenum internal_format = use_float ? GL_RGBA16F : GL_RGB10_A2;
	GLenum format = GL_RGBA;
	GLenum type = use_float ? GL_HALF_FLOAT : GL_UNSIGNED_INT_2_10_10_10_REV;

	while (mm_level) {
		for (int i = 0; i < 6; i++) {
			glTexImage2D(_cube_side_enum[i], lod, internal_format, size, size, 0, format, type, nullptr);
		}

		lod++;
		mm_level--;

		if (size > 1) {
			size >>= 1;
		}
	}

	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_BASE_LEVEL, 0);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAX_LEVEL, lod - 1);

	lod = 0;
	mm_level = mipmaps;

	size = p_resolution;

	shaders.cubemap_filter.set_conditional(CubemapFilterShaderGLES3::USE_DUAL_PARABOLOID, false);

	while (mm_level) {
		for (int i = 0; i < 6; i++) {
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, _cube_side_enum[i], new_cubemap, lod);

			glViewport(0, 0, size, size);
			glBindVertexArray(resources.quadie_array);

			shaders.cubemap_filter.set_uniform(CubemapFilterShaderGLES3::FACE_ID, i);
			shaders.cubemap_filter.set_uniform(CubemapFilterShaderGLES3::ROUGHNESS, lod / float(mipmaps - 1));

			glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
			glBindVertexArray(0);
#ifdef DEBUG_ENABLED
			GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
			ERR_CONTINUE(status != GL_FRAMEBUFFER_COMPLETE);
#endif
		}

		if (size > 1) {
			size >>= 1;
		}
		lod++;
		mm_level--;
	}

	//restore ranges
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_BASE_LEVEL, 0);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAX_LEVEL, lod - 1);

	glTexParameterf(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameterf(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameterf(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameterf(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameterf(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

	glBindFramebuffer(GL_FRAMEBUFFER, RasterizerStorageGLES3::system_fbo);
	glDeleteFramebuffers(1, &tmp_fb);

	Texture *ctex = memnew(Texture);

	ctex->type = VS::TEXTURE_TYPE_CUBEMAP;
	ctex->flags = VS::TEXTURE_FLAG_MIPMAPS | VS::TEXTURE_FLAG_FILTER;
	ctex->width = p_resolution;
	ctex->height = p_resolution;
	ctex->alloc_width = p_resolution;
	ctex->alloc_height = p_resolution;
	ctex->format = use_float ? Image::FORMAT_RGBAH : Image::FORMAT_RGBA8;
	ctex->target = GL_TEXTURE_CUBE_MAP;
	ctex->gl_format_cache = format;
	ctex->gl_internal_format_cache = internal_format;
	ctex->gl_type_cache = type;
	ctex->data_size = 0;
	ctex->compressed = false;
	ctex->srgb = false;
	ctex->total_data_size = 0;
	ctex->ignore_mipmaps = false;
	ctex->mipmaps = mipmaps;
	ctex->active = true;
	ctex->tex_id = new_cubemap;
	ctex->stored_cube_sides = (1 << 6) - 1;
	ctex->render_target = nullptr;

	return texture_owner.make_rid(ctex);
}

Size2 RasterizerStorageGLES3::texture_size_with_proxy(RID p_texture) const {
	const Texture *texture = texture_owner.getornull(p_texture);
	ERR_FAIL_COND_V(!texture, Size2());
	if (texture->proxy) {
		return Size2(texture->proxy->width, texture->proxy->height);
	} else {
		return Size2(texture->width, texture->height);
	}
}

void RasterizerStorageGLES3::texture_set_proxy(RID p_texture, RID p_proxy) {
	Texture *texture = texture_owner.get(p_texture);
	ERR_FAIL_COND(!texture);

	if (texture->proxy) {
		texture->proxy->proxy_owners.erase(texture);
		texture->proxy = nullptr;
	}

	if (p_proxy.is_valid()) {
		Texture *proxy = texture_owner.get(p_proxy);
		ERR_FAIL_COND(!proxy);
		ERR_FAIL_COND(proxy == texture);
		proxy->proxy_owners.insert(texture);
		texture->proxy = proxy;
	}
}

void RasterizerStorageGLES3::texture_set_force_redraw_if_visible(RID p_texture, bool p_enable) {
	Texture *texture = texture_owner.get(p_texture);
	ERR_FAIL_COND(!texture);
	texture->redraw_if_visible = p_enable;
}

RID RasterizerStorageGLES3::sky_create() {
	Sky *sky = memnew(Sky);
	sky->radiance = 0;
	sky->irradiance = 0;
	return sky_owner.make_rid(sky);
}

void RasterizerStorageGLES3::sky_set_texture(RID p_sky, RID p_panorama, int p_radiance_size) {
	Sky *sky = sky_owner.getornull(p_sky);
	ERR_FAIL_COND(!sky);

	if (sky->panorama.is_valid()) {
		sky->panorama = RID();
		glDeleteTextures(1, &sky->radiance);
		glDeleteTextures(1, &sky->irradiance);
		sky->radiance = 0;
		sky->irradiance = 0;
	}

	sky->panorama = p_panorama;
	if (!sky->panorama.is_valid()) {
		return; //cleared
	}

	Texture *texture = texture_owner.getornull(sky->panorama);
	if (!texture) {
		sky->panorama = RID();
		ERR_FAIL_COND(!texture);
	}

	texture = texture->get_ptr(); //resolve for proxies

	glBindVertexArray(0);
	glDisable(GL_CULL_FACE);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_SCISSOR_TEST);
	glDisable(GL_BLEND);

	gl_wrapper.gl_active_texture(GL_TEXTURE0);
	glBindTexture(texture->target, texture->tex_id);
	glTexParameteri(texture->target, GL_TEXTURE_BASE_LEVEL, 0);
#ifdef GLES_OVER_GL
	glTexParameteri(texture->target, GL_TEXTURE_MAX_LEVEL, int(Math::floor(Math::log(float(texture->width)) / Math::log(2.0f))));
	glGenerateMipmap(texture->target);
#else
	glTexParameteri(texture->target, GL_TEXTURE_MAX_LEVEL, 0);
#endif
	// Need Mipmaps regardless of whether they are set in import by user
	glTexParameterf(texture->target, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameterf(texture->target, GL_TEXTURE_WRAP_T, GL_REPEAT);
#ifdef GLES_OVER_GL
	glTexParameterf(texture->target, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
#else
	glTexParameterf(texture->target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
#endif
	glTexParameterf(texture->target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	if (config.srgb_decode_supported && texture->srgb && !texture->using_srgb) {
		glTexParameteri(texture->target, _TEXTURE_SRGB_DECODE_EXT, _DECODE_EXT);
		texture->using_srgb = true;
#ifdef TOOLS_ENABLED
		if (!(texture->flags & VS::TEXTURE_FLAG_CONVERT_TO_LINEAR)) {
			texture->flags |= VS::TEXTURE_FLAG_CONVERT_TO_LINEAR;
			//notify that texture must be set to linear beforehand, so it works in other platforms when exported
		}
#endif
	}

	{
		//Irradiance map
		gl_wrapper.gl_active_texture(GL_TEXTURE1);
		glGenTextures(1, &sky->irradiance);
		glBindTexture(GL_TEXTURE_2D, sky->irradiance);

		GLuint tmp_fb;

		glGenFramebuffers(1, &tmp_fb);
		glBindFramebuffer(GL_FRAMEBUFFER, tmp_fb);

		int size = 32;

		bool use_float = config.framebuffer_half_float_supported;

		GLenum internal_format = use_float ? GL_RGBA16F : GL_RGB10_A2;
		GLenum format = GL_RGBA;
		GLenum type = use_float ? GL_HALF_FLOAT : GL_UNSIGNED_INT_2_10_10_10_REV;

		glTexImage2D(GL_TEXTURE_2D, 0, internal_format, size, size * 2, 0, format, type, nullptr);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
		glTexParameterf(texture->target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameterf(texture->target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, sky->irradiance, 0);

		int irradiance_size = GLOBAL_GET("rendering/quality/reflections/irradiance_max_size");
		int upscale_size = MIN(int(previous_power_of_2(irradiance_size)), p_radiance_size);

		GLuint tmp_fb2;
		GLuint tmp_tex;
		{
			//generate another one for rendering, as can't read and write from a single texarray it seems
			glGenFramebuffers(1, &tmp_fb2);
			glBindFramebuffer(GL_FRAMEBUFFER, tmp_fb2);
			glGenTextures(1, &tmp_tex);
			glBindTexture(GL_TEXTURE_2D, tmp_tex);
			glTexImage2D(GL_TEXTURE_2D, 0, internal_format, upscale_size, 2.0 * upscale_size, 0, format, type, nullptr);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tmp_tex, 0);
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
#ifdef DEBUG_ENABLED
			GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
			ERR_FAIL_COND(status != GL_FRAMEBUFFER_COMPLETE);
#endif
		}

		shaders.cubemap_filter.set_conditional(CubemapFilterShaderGLES3::USE_DUAL_PARABOLOID, true);
		shaders.cubemap_filter.set_conditional(CubemapFilterShaderGLES3::USE_SOURCE_PANORAMA, true);
		shaders.cubemap_filter.set_conditional(CubemapFilterShaderGLES3::COMPUTE_IRRADIANCE, true);
		shaders.cubemap_filter.bind();

		// Very large Panoramas require way too much effort to compute irradiance so use a mipmap
		// level that corresponds to a panorama of 1024x512
		shaders.cubemap_filter.set_uniform(CubemapFilterShaderGLES3::SOURCE_MIP_LEVEL, MAX(Math::floor(Math::log(float(texture->width)) / Math::log(2.0f)) - 10.0f, 0.0f));

		// Compute Irradiance for a large texture, specified by radiance size and then pull out a low mipmap corresponding to 32x32
		for (int i = 0; i < 2; i++) {
			glViewport(0, i * upscale_size, upscale_size, upscale_size);
			glBindVertexArray(resources.quadie_array);

			shaders.cubemap_filter.set_uniform(CubemapFilterShaderGLES3::Z_FLIP, i > 0);

			glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
			glBindVertexArray(0);
		}
		glGenerateMipmap(GL_TEXTURE_2D);
		gl_wrapper.gl_active_texture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, tmp_tex);
		glBindFramebuffer(GL_FRAMEBUFFER, tmp_fb);

		shaders.cubemap_filter.set_conditional(CubemapFilterShaderGLES3::USE_DUAL_PARABOLOID, false);
		shaders.cubemap_filter.set_conditional(CubemapFilterShaderGLES3::USE_SOURCE_PANORAMA, false);
		shaders.cubemap_filter.set_conditional(CubemapFilterShaderGLES3::COMPUTE_IRRADIANCE, false);

		shaders.copy.set_conditional(CopyShaderGLES3::USE_LOD, true);
		shaders.copy.bind();
		shaders.copy.set_uniform(CopyShaderGLES3::MIP_LEVEL, MAX(Math::floor(Math::log(float(upscale_size)) / Math::log(2.0f)) - 5.0f, 0.0f)); // Mip level that corresponds to a 32x32 texture

		glViewport(0, 0, size, size * 2.0);
		glBindVertexArray(resources.quadie_array);
		glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
		glBindVertexArray(0);

		shaders.copy.set_conditional(CopyShaderGLES3::USE_LOD, false);

		glBindFramebuffer(GL_FRAMEBUFFER, RasterizerStorageGLES3::system_fbo);
		gl_wrapper.gl_active_texture(GL_TEXTURE0);
		glBindTexture(texture->target, texture->tex_id);
		glDeleteFramebuffers(1, &tmp_fb);
		glDeleteFramebuffers(1, &tmp_fb2);
		glDeleteTextures(1, &tmp_tex);
	}

	// Now compute radiance

	gl_wrapper.gl_active_texture(GL_TEXTURE1);
	glGenTextures(1, &sky->radiance);

	if (config.use_texture_array_environment) {
		//texture3D
		glBindTexture(GL_TEXTURE_2D_ARRAY, sky->radiance);

		GLuint tmp_fb;

		glGenFramebuffers(1, &tmp_fb);
		glBindFramebuffer(GL_FRAMEBUFFER, tmp_fb);

		int size = p_radiance_size;

		int array_level = 6;

		bool use_float = config.framebuffer_half_float_supported;

		GLenum internal_format = use_float ? GL_RGBA16F : GL_RGB10_A2;
		GLenum format = GL_RGBA;
		GLenum type = use_float ? GL_HALF_FLOAT : GL_UNSIGNED_INT_2_10_10_10_REV;

		glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, internal_format, size, size * 2, array_level, 0, format, type, nullptr);

		glTexParameterf(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameterf(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		GLuint tmp_fb2;
		GLuint tmp_tex;
		{
			//generate another one for rendering, as can't read and write from a single texarray it seems
			glGenFramebuffers(1, &tmp_fb2);
			glBindFramebuffer(GL_FRAMEBUFFER, tmp_fb2);
			glGenTextures(1, &tmp_tex);
			glBindTexture(GL_TEXTURE_2D, tmp_tex);
			glTexImage2D(GL_TEXTURE_2D, 0, internal_format, size, size * 2, 0, format, type, nullptr);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tmp_tex, 0);
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
#ifdef DEBUG_ENABLED
			GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
			ERR_FAIL_COND(status != GL_FRAMEBUFFER_COMPLETE);
#endif
		}

		for (int j = 0; j < array_level; j++) {
			glBindFramebuffer(GL_FRAMEBUFFER, tmp_fb2);

#ifdef GLES_OVER_GL
			if (j < 3) {
#else
			if (j == 0) {
#endif

				shaders.cubemap_filter.set_conditional(CubemapFilterShaderGLES3::USE_DUAL_PARABOLOID, true);
				shaders.cubemap_filter.set_conditional(CubemapFilterShaderGLES3::USE_SOURCE_PANORAMA, true);
				shaders.cubemap_filter.set_conditional(CubemapFilterShaderGLES3::USE_SOURCE_DUAL_PARABOLOID_ARRAY, false);
				shaders.cubemap_filter.bind();
				gl_wrapper.gl_active_texture(GL_TEXTURE0);
				glBindTexture(texture->target, texture->tex_id);
				shaders.cubemap_filter.set_uniform(CubemapFilterShaderGLES3::SOURCE_RESOLUTION, float(texture->width / 4));
			} else {
				shaders.cubemap_filter.set_conditional(CubemapFilterShaderGLES3::USE_DUAL_PARABOLOID, true);
				shaders.cubemap_filter.set_conditional(CubemapFilterShaderGLES3::USE_SOURCE_PANORAMA, false);
				shaders.cubemap_filter.set_conditional(CubemapFilterShaderGLES3::USE_SOURCE_DUAL_PARABOLOID_ARRAY, true);
				shaders.cubemap_filter.bind();
				gl_wrapper.gl_active_texture(GL_TEXTURE0);
				glBindTexture(GL_TEXTURE_2D_ARRAY, sky->radiance);
				shaders.cubemap_filter.set_uniform(CubemapFilterShaderGLES3::SOURCE_ARRAY_INDEX, j - 1); //read from previous to ensure better blur
			}

			for (int i = 0; i < 2; i++) {
				glViewport(0, i * size, size, size);
				glBindVertexArray(resources.quadie_array);

				shaders.cubemap_filter.set_uniform(CubemapFilterShaderGLES3::Z_FLIP, i > 0);
				shaders.cubemap_filter.set_uniform(CubemapFilterShaderGLES3::ROUGHNESS, j / float(array_level - 1));

				glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
				glBindVertexArray(0);
			}

			glBindFramebuffer(GL_DRAW_FRAMEBUFFER, tmp_fb);
			glFramebufferTextureLayer(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, sky->radiance, 0, j);
			glBindFramebuffer(GL_READ_FRAMEBUFFER, tmp_fb2);
			glReadBuffer(GL_COLOR_ATTACHMENT0);
			glBlitFramebuffer(0, 0, size, size * 2, 0, 0, size, size * 2, GL_COLOR_BUFFER_BIT, GL_NEAREST);
			glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
			glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
		}

		shaders.cubemap_filter.set_conditional(CubemapFilterShaderGLES3::USE_SOURCE_PANORAMA, false);
		shaders.cubemap_filter.set_conditional(CubemapFilterShaderGLES3::USE_DUAL_PARABOLOID, false);
		shaders.cubemap_filter.set_conditional(CubemapFilterShaderGLES3::USE_SOURCE_DUAL_PARABOLOID_ARRAY, false);

		//restore ranges
		gl_wrapper.gl_active_texture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D_ARRAY, sky->radiance);

		glGenerateMipmap(GL_TEXTURE_2D_ARRAY);

		glTexParameterf(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glTexParameterf(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameterf(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameterf(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		//reset flags on Sky Texture that may have changed
		texture_set_flags(sky->panorama, texture->flags);

		glBindFramebuffer(GL_FRAMEBUFFER, RasterizerStorageGLES3::system_fbo);
		glDeleteFramebuffers(1, &tmp_fb);
		glDeleteFramebuffers(1, &tmp_fb2);
		glDeleteTextures(1, &tmp_tex);

	} else {
		//regular single texture with mipmaps
		glBindTexture(GL_TEXTURE_2D, sky->radiance);

		GLuint tmp_fb;

		glGenFramebuffers(1, &tmp_fb);
		glBindFramebuffer(GL_FRAMEBUFFER, tmp_fb);

		int size = p_radiance_size;

		int lod = 0;

		int mipmaps = 6;

		int mm_level = mipmaps;

		bool use_float = config.framebuffer_half_float_supported;

		GLenum internal_format = use_float ? GL_RGBA16F : GL_RGB10_A2;
		GLenum format = GL_RGBA;
		GLenum type = use_float ? GL_HALF_FLOAT : GL_UNSIGNED_INT_2_10_10_10_REV;

		glTexStorage2DCustom(GL_TEXTURE_2D, mipmaps, internal_format, size, size * 2.0, format, type);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, mipmaps - 1);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		GLuint tmp_fb2;
		GLuint tmp_tex;
		{
			// Need a temporary framebuffer for rendering so we can read from previous iterations
			glGenFramebuffers(1, &tmp_fb2);
			glBindFramebuffer(GL_FRAMEBUFFER, tmp_fb2);
			glGenTextures(1, &tmp_tex);
			glBindTexture(GL_TEXTURE_2D, tmp_tex);
			glTexImage2D(GL_TEXTURE_2D, 0, internal_format, size, size * 2, 0, format, type, nullptr);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tmp_tex, 0);
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
#ifdef DEBUG_ENABLED
			GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
			ERR_FAIL_COND(status != GL_FRAMEBUFFER_COMPLETE);
#endif
		}

		lod = 0;
		mm_level = mipmaps;

		size = p_radiance_size;

		while (mm_level) {
			glBindFramebuffer(GL_FRAMEBUFFER, tmp_fb);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, sky->radiance, lod);

#ifdef DEBUG_ENABLED
			GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
			ERR_CONTINUE(status != GL_FRAMEBUFFER_COMPLETE);
#endif
			glBindTexture(GL_TEXTURE_2D, tmp_tex);
			glTexImage2D(GL_TEXTURE_2D, 0, internal_format, size, size * 2, 0, format, type, nullptr);
			glBindFramebuffer(GL_FRAMEBUFFER, tmp_fb2);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tmp_tex, 0);
#ifdef GLES_OVER_GL
			if (lod < 3) {
#else
			if (lod == 0) {
#endif

				shaders.cubemap_filter.set_conditional(CubemapFilterShaderGLES3::USE_DUAL_PARABOLOID, true);
				shaders.cubemap_filter.set_conditional(CubemapFilterShaderGLES3::USE_SOURCE_PANORAMA, true);
				shaders.cubemap_filter.set_conditional(CubemapFilterShaderGLES3::USE_SOURCE_DUAL_PARABOLOID, false);
				shaders.cubemap_filter.bind();
				gl_wrapper.gl_active_texture(GL_TEXTURE0);
				glBindTexture(texture->target, texture->tex_id);
				shaders.cubemap_filter.set_uniform(CubemapFilterShaderGLES3::SOURCE_RESOLUTION, float(texture->width / 4));
			} else {
				shaders.cubemap_filter.set_conditional(CubemapFilterShaderGLES3::USE_DUAL_PARABOLOID, true);
				shaders.cubemap_filter.set_conditional(CubemapFilterShaderGLES3::USE_SOURCE_PANORAMA, false);
				shaders.cubemap_filter.set_conditional(CubemapFilterShaderGLES3::USE_SOURCE_DUAL_PARABOLOID, true);
				shaders.cubemap_filter.bind();
				gl_wrapper.gl_active_texture(GL_TEXTURE0);
				glBindTexture(GL_TEXTURE_2D, sky->radiance);
				shaders.cubemap_filter.set_uniform(CubemapFilterShaderGLES3::SOURCE_MIP_LEVEL, float(lod - 1)); //read from previous to ensure better blur
			}

			for (int i = 0; i < 2; i++) {
				glViewport(0, i * size, size, size);
				glBindVertexArray(resources.quadie_array);

				shaders.cubemap_filter.set_uniform(CubemapFilterShaderGLES3::Z_FLIP, i > 0);
				shaders.cubemap_filter.set_uniform(CubemapFilterShaderGLES3::ROUGHNESS, lod / float(mipmaps - 1));

				glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
				glBindVertexArray(0);
			}

			glBindFramebuffer(GL_DRAW_FRAMEBUFFER, tmp_fb);
			glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, sky->radiance, lod);
			glBindFramebuffer(GL_READ_FRAMEBUFFER, tmp_fb2);
			glReadBuffer(GL_COLOR_ATTACHMENT0);
			glBlitFramebuffer(0, 0, size, size * 2, 0, 0, size, size * 2, GL_COLOR_BUFFER_BIT, GL_NEAREST);
			glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
			glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);

			if (size > 1) {
				size >>= 1;
			}
			lod++;
			mm_level--;
		}
		shaders.cubemap_filter.set_conditional(CubemapFilterShaderGLES3::USE_DUAL_PARABOLOID, false);
		shaders.cubemap_filter.set_conditional(CubemapFilterShaderGLES3::USE_SOURCE_PANORAMA, false);
		shaders.cubemap_filter.set_conditional(CubemapFilterShaderGLES3::USE_SOURCE_DUAL_PARABOLOID, false);

		//restore ranges
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, lod - 1);

		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		//reset flags on Sky Texture that may have changed
		texture_set_flags(sky->panorama, texture->flags);

		glBindFramebuffer(GL_FRAMEBUFFER, RasterizerStorageGLES3::system_fbo);
		glDeleteFramebuffers(1, &tmp_fb);
		glDeleteFramebuffers(1, &tmp_fb2);
		glDeleteTextures(1, &tmp_tex);
	}
}

/* SHADER API */

RID RasterizerStorageGLES3::shader_create() {
	Shader *shader = memnew(Shader);
	shader->mode = VS::SHADER_SPATIAL;
	shader->shader = &scene->state.scene_shader;
	RID rid = shader_owner.make_rid(shader);
	_shader_make_dirty(shader);
	shader->self = rid;

	return rid;
}

void RasterizerStorageGLES3::_shader_make_dirty(Shader *p_shader) {
	if (p_shader->dirty_list.in_list()) {
		return;
	}

	_shader_dirty_list.add(&p_shader->dirty_list);
}

void RasterizerStorageGLES3::shader_set_code(RID p_shader, const String &p_code) {
	Shader *shader = shader_owner.get(p_shader);
	ERR_FAIL_COND(!shader);

	shader->code = p_code;

	String mode_string = ShaderLanguage::get_shader_type(p_code);
	VS::ShaderMode mode;

	if (mode_string == "canvas_item") {
		mode = VS::SHADER_CANVAS_ITEM;
	} else if (mode_string == "particles") {
		mode = VS::SHADER_PARTICLES;
	} else {
		mode = VS::SHADER_SPATIAL;
	}

	if (shader->custom_code_id && mode != shader->mode) {
		shader->shader->free_custom_shader(shader->custom_code_id);
		shader->custom_code_id = 0;
	}

	shader->mode = mode;

	ShaderGLES3 *shaders[VS::SHADER_MAX] = {
		&scene->state.scene_shader,
		&canvas->state.canvas_shader,
		&this->shaders.particles,

	};

	shader->shader = shaders[mode];

	if (shader->custom_code_id == 0) {
		shader->custom_code_id = shader->shader->create_custom_shader();
	}

	_shader_make_dirty(shader);
}
String RasterizerStorageGLES3::shader_get_code(RID p_shader) const {
	const Shader *shader = shader_owner.get(p_shader);
	ERR_FAIL_COND_V(!shader, String());

	return shader->code;
}

void RasterizerStorageGLES3::_update_shader(Shader *p_shader) const {
	_shader_dirty_list.remove(&p_shader->dirty_list);

	p_shader->valid = false;
	p_shader->ubo_size = 0;

	p_shader->uniforms.clear();

	if (p_shader->code == String()) {
		return; //just invalid, but no error
	}

	ShaderCompilerGLES3::GeneratedCode gen_code;
	ShaderCompilerGLES3::IdentifierActions *actions = nullptr;

	int async_mode = (int)ShaderGLES3::ASYNC_MODE_VISIBLE;

	switch (p_shader->mode) {
		case VS::SHADER_CANVAS_ITEM: {
			p_shader->canvas_item.light_mode = Shader::CanvasItem::LIGHT_MODE_NORMAL;
			p_shader->canvas_item.blend_mode = Shader::CanvasItem::BLEND_MODE_MIX;
			p_shader->canvas_item.uses_screen_texture = false;
			p_shader->canvas_item.uses_screen_uv = false;
			p_shader->canvas_item.uses_time = false;
			p_shader->canvas_item.uses_modulate = false;
			p_shader->canvas_item.uses_color = false;
			p_shader->canvas_item.uses_vertex = false;
			p_shader->canvas_item.batch_flags = 0;

			p_shader->canvas_item.uses_world_matrix = false;
			p_shader->canvas_item.uses_extra_matrix = false;
			p_shader->canvas_item.uses_projection_matrix = false;
			p_shader->canvas_item.uses_instance_custom = false;

			shaders.actions_canvas.render_mode_values["blend_add"] = Pair<int *, int>(&p_shader->canvas_item.blend_mode, Shader::CanvasItem::BLEND_MODE_ADD);
			shaders.actions_canvas.render_mode_values["blend_mix"] = Pair<int *, int>(&p_shader->canvas_item.blend_mode, Shader::CanvasItem::BLEND_MODE_MIX);
			shaders.actions_canvas.render_mode_values["blend_sub"] = Pair<int *, int>(&p_shader->canvas_item.blend_mode, Shader::CanvasItem::BLEND_MODE_SUB);
			shaders.actions_canvas.render_mode_values["blend_mul"] = Pair<int *, int>(&p_shader->canvas_item.blend_mode, Shader::CanvasItem::BLEND_MODE_MUL);
			shaders.actions_canvas.render_mode_values["blend_premul_alpha"] = Pair<int *, int>(&p_shader->canvas_item.blend_mode, Shader::CanvasItem::BLEND_MODE_PMALPHA);
			shaders.actions_canvas.render_mode_values["blend_disabled"] = Pair<int *, int>(&p_shader->canvas_item.blend_mode, Shader::CanvasItem::BLEND_MODE_DISABLED);

			shaders.actions_canvas.render_mode_values["unshaded"] = Pair<int *, int>(&p_shader->canvas_item.light_mode, Shader::CanvasItem::LIGHT_MODE_UNSHADED);
			shaders.actions_canvas.render_mode_values["light_only"] = Pair<int *, int>(&p_shader->canvas_item.light_mode, Shader::CanvasItem::LIGHT_MODE_LIGHT_ONLY);

			shaders.actions_canvas.usage_flag_pointers["SCREEN_UV"] = &p_shader->canvas_item.uses_screen_uv;
			shaders.actions_canvas.usage_flag_pointers["SCREEN_PIXEL_SIZE"] = &p_shader->canvas_item.uses_screen_uv;
			shaders.actions_canvas.usage_flag_pointers["SCREEN_TEXTURE"] = &p_shader->canvas_item.uses_screen_texture;
			shaders.actions_canvas.usage_flag_pointers["TIME"] = &p_shader->canvas_item.uses_time;

			shaders.actions_canvas.usage_flag_pointers["MODULATE"] = &p_shader->canvas_item.uses_modulate;
			shaders.actions_canvas.usage_flag_pointers["COLOR"] = &p_shader->canvas_item.uses_color;
			shaders.actions_canvas.usage_flag_pointers["VERTEX"] = &p_shader->canvas_item.uses_vertex;

			shaders.actions_canvas.usage_flag_pointers["WORLD_MATRIX"] = &p_shader->canvas_item.uses_world_matrix;
			shaders.actions_canvas.usage_flag_pointers["EXTRA_MATRIX"] = &p_shader->canvas_item.uses_extra_matrix;
			shaders.actions_canvas.usage_flag_pointers["PROJECTION_MATRIX"] = &p_shader->canvas_item.uses_projection_matrix;
			shaders.actions_canvas.usage_flag_pointers["INSTANCE_CUSTOM"] = &p_shader->canvas_item.uses_instance_custom;

			actions = &shaders.actions_canvas;
			actions->uniforms = &p_shader->uniforms;

		} break;

		case VS::SHADER_SPATIAL: {
			p_shader->spatial.blend_mode = Shader::Spatial::BLEND_MODE_MIX;
			p_shader->spatial.depth_draw_mode = Shader::Spatial::DEPTH_DRAW_OPAQUE;
			p_shader->spatial.cull_mode = Shader::Spatial::CULL_MODE_BACK;
			p_shader->spatial.uses_alpha = false;
			p_shader->spatial.uses_alpha_scissor = false;
			p_shader->spatial.uses_discard = false;
			p_shader->spatial.unshaded = false;
			p_shader->spatial.no_depth_test = false;
			p_shader->spatial.uses_sss = false;
			p_shader->spatial.uses_time = false;
			p_shader->spatial.uses_vertex_lighting = false;
			p_shader->spatial.uses_screen_texture = false;
			p_shader->spatial.uses_depth_texture = false;
			p_shader->spatial.uses_vertex = false;
			p_shader->spatial.uses_tangent = false;
			p_shader->spatial.uses_ensure_correct_normals = false;
			p_shader->spatial.writes_modelview_or_projection = false;
			p_shader->spatial.uses_world_coordinates = false;

			shaders.actions_scene.render_mode_values["blend_add"] = Pair<int *, int>(&p_shader->spatial.blend_mode, Shader::Spatial::BLEND_MODE_ADD);
			shaders.actions_scene.render_mode_values["blend_mix"] = Pair<int *, int>(&p_shader->spatial.blend_mode, Shader::Spatial::BLEND_MODE_MIX);
			shaders.actions_scene.render_mode_values["blend_sub"] = Pair<int *, int>(&p_shader->spatial.blend_mode, Shader::Spatial::BLEND_MODE_SUB);
			shaders.actions_scene.render_mode_values["blend_mul"] = Pair<int *, int>(&p_shader->spatial.blend_mode, Shader::Spatial::BLEND_MODE_MUL);

			shaders.actions_scene.render_mode_values["depth_draw_opaque"] = Pair<int *, int>(&p_shader->spatial.depth_draw_mode, Shader::Spatial::DEPTH_DRAW_OPAQUE);
			shaders.actions_scene.render_mode_values["depth_draw_always"] = Pair<int *, int>(&p_shader->spatial.depth_draw_mode, Shader::Spatial::DEPTH_DRAW_ALWAYS);
			shaders.actions_scene.render_mode_values["depth_draw_never"] = Pair<int *, int>(&p_shader->spatial.depth_draw_mode, Shader::Spatial::DEPTH_DRAW_NEVER);
			shaders.actions_scene.render_mode_values["depth_draw_alpha_prepass"] = Pair<int *, int>(&p_shader->spatial.depth_draw_mode, Shader::Spatial::DEPTH_DRAW_ALPHA_PREPASS);

			shaders.actions_scene.render_mode_values["cull_front"] = Pair<int *, int>(&p_shader->spatial.cull_mode, Shader::Spatial::CULL_MODE_FRONT);
			shaders.actions_scene.render_mode_values["cull_back"] = Pair<int *, int>(&p_shader->spatial.cull_mode, Shader::Spatial::CULL_MODE_BACK);
			shaders.actions_scene.render_mode_values["cull_disabled"] = Pair<int *, int>(&p_shader->spatial.cull_mode, Shader::Spatial::CULL_MODE_DISABLED);

			shaders.actions_scene.render_mode_values["async_visible"] = Pair<int *, int>(&async_mode, (int)ShaderGLES3::ASYNC_MODE_VISIBLE);
			shaders.actions_scene.render_mode_values["async_hidden"] = Pair<int *, int>(&async_mode, (int)ShaderGLES3::ASYNC_MODE_HIDDEN);

			shaders.actions_scene.render_mode_flags["unshaded"] = &p_shader->spatial.unshaded;
			shaders.actions_scene.render_mode_flags["depth_test_disable"] = &p_shader->spatial.no_depth_test;

			shaders.actions_scene.render_mode_flags["vertex_lighting"] = &p_shader->spatial.uses_vertex_lighting;

			shaders.actions_scene.render_mode_flags["world_vertex_coords"] = &p_shader->spatial.uses_world_coordinates;

			shaders.actions_scene.render_mode_flags["ensure_correct_normals"] = &p_shader->spatial.uses_ensure_correct_normals;

			shaders.actions_scene.usage_flag_pointers["ALPHA"] = &p_shader->spatial.uses_alpha;
			shaders.actions_scene.usage_flag_pointers["ALPHA_SCISSOR"] = &p_shader->spatial.uses_alpha_scissor;

			shaders.actions_scene.usage_flag_pointers["SSS_STRENGTH"] = &p_shader->spatial.uses_sss;
			shaders.actions_scene.usage_flag_pointers["DISCARD"] = &p_shader->spatial.uses_discard;
			shaders.actions_scene.usage_flag_pointers["SCREEN_TEXTURE"] = &p_shader->spatial.uses_screen_texture;
			shaders.actions_scene.usage_flag_pointers["DEPTH_TEXTURE"] = &p_shader->spatial.uses_depth_texture;
			shaders.actions_scene.usage_flag_pointers["TIME"] = &p_shader->spatial.uses_time;

			// Use of any of these BUILTINS indicate the need for transformed tangents.
			// This is needed to know when to transform tangents in software skinning.
			shaders.actions_scene.usage_flag_pointers["TANGENT"] = &p_shader->spatial.uses_tangent;
			shaders.actions_scene.usage_flag_pointers["NORMALMAP"] = &p_shader->spatial.uses_tangent;

			shaders.actions_scene.write_flag_pointers["MODELVIEW_MATRIX"] = &p_shader->spatial.writes_modelview_or_projection;
			shaders.actions_scene.write_flag_pointers["PROJECTION_MATRIX"] = &p_shader->spatial.writes_modelview_or_projection;
			shaders.actions_scene.write_flag_pointers["VERTEX"] = &p_shader->spatial.uses_vertex;

			actions = &shaders.actions_scene;
			actions->uniforms = &p_shader->uniforms;

		} break;
		case VS::SHADER_PARTICLES: {
			actions = &shaders.actions_particles;
			actions->uniforms = &p_shader->uniforms;
		} break;
		case VS::SHADER_MAX:
			break; // Can't happen, but silences warning
	}

	Error err = shaders.compiler.compile(p_shader->mode, p_shader->code, actions, p_shader->path, gen_code);
	if (err != OK) {
		return;
	}

	p_shader->ubo_size = gen_code.uniform_total_size;
	p_shader->ubo_offsets = gen_code.uniform_offsets;
	p_shader->texture_count = gen_code.texture_uniforms.size();
	p_shader->texture_hints = gen_code.texture_hints;
	p_shader->texture_types = gen_code.texture_types;

	p_shader->uses_vertex_time = gen_code.uses_vertex_time;
	p_shader->uses_fragment_time = gen_code.uses_fragment_time;

	// some logic for batching
	if (p_shader->mode == VS::SHADER_CANVAS_ITEM) {
		if (p_shader->canvas_item.uses_modulate | p_shader->canvas_item.uses_color) {
			p_shader->canvas_item.batch_flags |= RasterizerStorageCommon::PREVENT_COLOR_BAKING;
		}
		if (p_shader->canvas_item.uses_vertex) {
			p_shader->canvas_item.batch_flags |= RasterizerStorageCommon::PREVENT_VERTEX_BAKING;
		}
		if (p_shader->canvas_item.uses_world_matrix | p_shader->canvas_item.uses_extra_matrix | p_shader->canvas_item.uses_projection_matrix | p_shader->canvas_item.uses_instance_custom) {
			p_shader->canvas_item.batch_flags |= RasterizerStorageCommon::PREVENT_ITEM_JOINING;
		}
	}

	p_shader->shader->set_custom_shader_code(p_shader->custom_code_id, gen_code.vertex, gen_code.vertex_global, gen_code.fragment, gen_code.light, gen_code.fragment_global, gen_code.uniforms, gen_code.texture_uniforms, gen_code.defines, (ShaderGLES3::AsyncMode)async_mode);

	//all materials using this shader will have to be invalidated, unfortunately

	for (SelfList<Material> *E = p_shader->materials.first(); E; E = E->next()) {
		_material_make_dirty(E->self());
	}

	if (shaders.tracker) {
		shaders.tracker->add_shader(p_shader->mode, p_shader->code, actions, p_shader->shader->get_conditional_version());
	}

	p_shader->valid = true;
	p_shader->version++;
}

void RasterizerStorageGLES3::update_dirty_shaders() {
	while (_shader_dirty_list.first()) {
		_update_shader(_shader_dirty_list.first()->self());
	}
}

void RasterizerStorageGLES3::shader_get_param_list(RID p_shader, List<PropertyInfo> *p_param_list) const {
	Shader *shader = shader_owner.get(p_shader);
	ERR_FAIL_COND(!shader);

	if (shader->dirty_list.in_list()) {
		_update_shader(shader); // ok should be not anymore dirty
	}

	Map<int, StringName> order;

	for (Map<StringName, ShaderLanguage::ShaderNode::Uniform>::Element *E = shader->uniforms.front(); E; E = E->next()) {
		if (E->get().texture_order >= 0) {
			order[E->get().texture_order + 100000] = E->key();
		} else {
			order[E->get().order] = E->key();
		}
	}

	for (Map<int, StringName>::Element *E = order.front(); E; E = E->next()) {
		PropertyInfo pi;
		ShaderLanguage::ShaderNode::Uniform &u = shader->uniforms[E->get()];
		pi.name = E->get();
		switch (u.type) {
			case ShaderLanguage::TYPE_STRUCT:
				pi.type = Variant::ARRAY;
				break;
			case ShaderLanguage::TYPE_VOID:
				pi.type = Variant::NIL;
				break;
			case ShaderLanguage::TYPE_BOOL:
				pi.type = Variant::BOOL;
				break;
			case ShaderLanguage::TYPE_BVEC2:
				pi.type = Variant::INT;
				pi.hint = PROPERTY_HINT_FLAGS;
				pi.hint_string = "x,y";
				break;
			case ShaderLanguage::TYPE_BVEC3:
				pi.type = Variant::INT;
				pi.hint = PROPERTY_HINT_FLAGS;
				pi.hint_string = "x,y,z";
				break;
			case ShaderLanguage::TYPE_BVEC4:
				pi.type = Variant::INT;
				pi.hint = PROPERTY_HINT_FLAGS;
				pi.hint_string = "x,y,z,w";
				break;
			case ShaderLanguage::TYPE_UINT:
			case ShaderLanguage::TYPE_INT: {
				pi.type = Variant::INT;
				if (u.hint == ShaderLanguage::ShaderNode::Uniform::HINT_RANGE) {
					pi.hint = PROPERTY_HINT_RANGE;
					pi.hint_string = rtos(u.hint_range[0]) + "," + rtos(u.hint_range[1]) + "," + rtos(u.hint_range[2]);
				}

			} break;
			case ShaderLanguage::TYPE_IVEC2:
			case ShaderLanguage::TYPE_IVEC3:
			case ShaderLanguage::TYPE_IVEC4:
			case ShaderLanguage::TYPE_UVEC2:
			case ShaderLanguage::TYPE_UVEC3:
			case ShaderLanguage::TYPE_UVEC4: {
				pi.type = Variant::POOL_INT_ARRAY;
			} break;
			case ShaderLanguage::TYPE_FLOAT: {
				pi.type = Variant::REAL;
				if (u.hint == ShaderLanguage::ShaderNode::Uniform::HINT_RANGE) {
					pi.hint = PROPERTY_HINT_RANGE;
					pi.hint_string = rtos(u.hint_range[0]) + "," + rtos(u.hint_range[1]) + "," + rtos(u.hint_range[2]);
				}

			} break;
			case ShaderLanguage::TYPE_VEC2:
				pi.type = Variant::VECTOR2;
				break;
			case ShaderLanguage::TYPE_VEC3:
				pi.type = Variant::VECTOR3;
				break;
			case ShaderLanguage::TYPE_VEC4: {
				if (u.hint == ShaderLanguage::ShaderNode::Uniform::HINT_COLOR) {
					pi.type = Variant::COLOR;
				} else {
					pi.type = Variant::PLANE;
				}
			} break;
			case ShaderLanguage::TYPE_MAT2:
				pi.type = Variant::TRANSFORM2D;
				break;
			case ShaderLanguage::TYPE_MAT3:
				pi.type = Variant::BASIS;
				break;
			case ShaderLanguage::TYPE_MAT4:
				pi.type = Variant::TRANSFORM;
				break;
			case ShaderLanguage::TYPE_SAMPLER2D:
			case ShaderLanguage::TYPE_SAMPLEREXT:
			case ShaderLanguage::TYPE_ISAMPLER2D:
			case ShaderLanguage::TYPE_USAMPLER2D: {
				pi.type = Variant::OBJECT;
				pi.hint = PROPERTY_HINT_RESOURCE_TYPE;
				pi.hint_string = "Texture";
			} break;
			case ShaderLanguage::TYPE_SAMPLER2DARRAY:
			case ShaderLanguage::TYPE_ISAMPLER2DARRAY:
			case ShaderLanguage::TYPE_USAMPLER2DARRAY: {
				pi.type = Variant::OBJECT;
				pi.hint = PROPERTY_HINT_RESOURCE_TYPE;
				pi.hint_string = "TextureArray";
			} break;
			case ShaderLanguage::TYPE_SAMPLER3D:
			case ShaderLanguage::TYPE_ISAMPLER3D:
			case ShaderLanguage::TYPE_USAMPLER3D: {
				pi.type = Variant::OBJECT;
				pi.hint = PROPERTY_HINT_RESOURCE_TYPE;
				pi.hint_string = "Texture3D";
			} break;
			case ShaderLanguage::TYPE_SAMPLERCUBE: {
				pi.type = Variant::OBJECT;
				pi.hint = PROPERTY_HINT_RESOURCE_TYPE;
				pi.hint_string = "CubeMap";
			} break;
			default: {
			}
		};

		p_param_list->push_back(pi);
	}
}

void RasterizerStorageGLES3::shader_set_default_texture_param(RID p_shader, const StringName &p_name, RID p_texture) {
	Shader *shader = shader_owner.get(p_shader);
	ERR_FAIL_COND(!shader);
	ERR_FAIL_COND(p_texture.is_valid() && !texture_owner.owns(p_texture));

	if (p_texture.is_valid()) {
		shader->default_textures[p_name] = p_texture;
	} else {
		shader->default_textures.erase(p_name);
	}

	_shader_make_dirty(shader);
}
RID RasterizerStorageGLES3::shader_get_default_texture_param(RID p_shader, const StringName &p_name) const {
	const Shader *shader = shader_owner.get(p_shader);
	ERR_FAIL_COND_V(!shader, RID());

	const Map<StringName, RID>::Element *E = shader->default_textures.find(p_name);
	if (!E) {
		return RID();
	}
	return E->get();
}

void RasterizerStorageGLES3::shader_add_custom_define(RID p_shader, const String &p_define) {
	Shader *shader = shader_owner.get(p_shader);
	ERR_FAIL_COND(!shader);

	shader->shader->add_custom_define(p_define);

	_shader_make_dirty(shader);
}

void RasterizerStorageGLES3::shader_get_custom_defines(RID p_shader, Vector<String> *p_defines) const {
	Shader *shader = shader_owner.get(p_shader);
	ERR_FAIL_COND(!shader);

	shader->shader->get_custom_defines(p_defines);
}

void RasterizerStorageGLES3::shader_remove_custom_define(RID p_shader, const String &p_define) {
	Shader *shader = shader_owner.get(p_shader);
	ERR_FAIL_COND(!shader);

	shader->shader->remove_custom_define(p_define);

	_shader_make_dirty(shader);
}

void RasterizerStorageGLES3::set_shader_async_hidden_forbidden(bool p_forbidden) {
	ShaderGLES3::async_hidden_forbidden = p_forbidden;
}

bool RasterizerStorageGLES3::is_shader_async_hidden_forbidden() {
	return ShaderGLES3::async_hidden_forbidden;
}

Error RasterizerStorageGLES3::shader_preload_spatial(const String &p_file_path) {
	return shaders.preLoader->load_spatial(p_file_path);
}

Error RasterizerStorageGLES3::shader_preload_canvas(const String &p_file_path) {
	return shaders.preLoader->load_canvas(p_file_path);
}

Error RasterizerStorageGLES3::shader_preload_particle(const String &p_file_path) {
	return shaders.preLoader->load_particle(p_file_path);
}

void RasterizerStorageGLES3::shader_preload_start() {
	shaders.preLoader->start();
}

bool RasterizerStorageGLES3::shader_preload_is_running() const {
	return shaders.preLoader->is_running();
}

int RasterizerStorageGLES3::shader_preload_get_stage() const {
	return shaders.preLoader->get_stage();
}

int RasterizerStorageGLES3::shader_preload_get_stage_count() const {
	return shaders.preLoader->get_stage_count();
}

/* COMMON MATERIAL API */

void RasterizerStorageGLES3::_material_make_dirty(Material *p_material) const {
	if (p_material->dirty_list.in_list()) {
		return;
	}

	_material_dirty_list.add(&p_material->dirty_list);
}

RID RasterizerStorageGLES3::material_create() {
	Material *material = memnew(Material);

	return material_owner.make_rid(material);
}

void RasterizerStorageGLES3::material_set_shader(RID p_material, RID p_shader) {
	Material *material = material_owner.get(p_material);
	ERR_FAIL_COND(!material);

	Shader *shader = shader_owner.getornull(p_shader);

	if (material->shader) {
		//if shader, remove from previous shader material list
		material->shader->materials.remove(&material->list);
	}
	material->shader = shader;

	if (shader) {
		shader->materials.add(&material->list);
	}

	_material_make_dirty(material);
}

RID RasterizerStorageGLES3::material_get_shader(RID p_material) const {
	const Material *material = material_owner.get(p_material);
	ERR_FAIL_COND_V(!material, RID());

	if (material->shader) {
		return material->shader->self;
	}

	return RID();
}

void RasterizerStorageGLES3::material_set_param(RID p_material, const StringName &p_param, const Variant &p_value) {
	Material *material = material_owner.get(p_material);
	ERR_FAIL_COND(!material);

	if (p_value.get_type() == Variant::NIL) {
		material->params.erase(p_param);
	} else {
		material->params[p_param] = p_value;
	}

	_material_make_dirty(material);
}
Variant RasterizerStorageGLES3::material_get_param(RID p_material, const StringName &p_param) const {
	const Material *material = material_owner.get(p_material);
	ERR_FAIL_COND_V(!material, Variant());

	if (material->params.has(p_param)) {
		return material->params[p_param];
	}

	return material_get_param_default(p_material, p_param);
}

Variant RasterizerStorageGLES3::material_get_param_default(RID p_material, const StringName &p_param) const {
	const Material *material = material_owner.get(p_material);
	ERR_FAIL_COND_V(!material, Variant());

	if (material->shader) {
		if (material->shader->uniforms.has(p_param)) {
			ShaderLanguage::ShaderNode::Uniform uniform = material->shader->uniforms[p_param];
			Vector<ShaderLanguage::ConstantNode::Value> default_value = uniform.default_value;
			return ShaderLanguage::constant_value_to_variant(default_value, uniform.type, uniform.hint);
		}
	}
	return Variant();
}

void RasterizerStorageGLES3::material_set_line_width(RID p_material, float p_width) {
	Material *material = material_owner.get(p_material);
	ERR_FAIL_COND(!material);

	material->line_width = p_width;
}

void RasterizerStorageGLES3::material_set_next_pass(RID p_material, RID p_next_material) {
	Material *material = material_owner.get(p_material);
	ERR_FAIL_COND(!material);

	material->next_pass = p_next_material;
}

bool RasterizerStorageGLES3::material_is_animated(RID p_material) {
	Material *material = material_owner.get(p_material);
	ERR_FAIL_COND_V(!material, false);
	if (material->dirty_list.in_list()) {
		_update_material(material);
	}

	bool animated = material->is_animated_cache;
	if (!animated && material->next_pass.is_valid()) {
		animated = material_is_animated(material->next_pass);
	}
	return animated;
}
bool RasterizerStorageGLES3::material_casts_shadows(RID p_material) {
	Material *material = material_owner.get(p_material);
	ERR_FAIL_COND_V(!material, false);
	if (material->dirty_list.in_list()) {
		_update_material(material);
	}

	bool casts_shadows = material->can_cast_shadow_cache;

	if (!casts_shadows && material->next_pass.is_valid()) {
		casts_shadows = material_casts_shadows(material->next_pass);
	}

	return casts_shadows;
}

bool RasterizerStorageGLES3::material_uses_tangents(RID p_material) {
	Material *material = material_owner.get(p_material);
	ERR_FAIL_COND_V(!material, false);

	if (!material->shader) {
		return false;
	}

	if (material->shader->dirty_list.in_list()) {
		_update_shader(material->shader);
	}

	return material->shader->spatial.uses_tangent;
}

bool RasterizerStorageGLES3::material_uses_ensure_correct_normals(RID p_material) {
	Material *material = material_owner.get(p_material);
	ERR_FAIL_COND_V(!material, false);

	if (!material->shader) {
		return false;
	}

	if (material->shader->dirty_list.in_list()) {
		_update_shader(material->shader);
	}

	return material->shader->spatial.uses_ensure_correct_normals;
}

void RasterizerStorageGLES3::material_add_instance_owner(RID p_material, RasterizerScene::InstanceBase *p_instance) {
	Material *material = material_owner.get(p_material);
	ERR_FAIL_COND(!material);

	Map<RasterizerScene::InstanceBase *, int>::Element *E = material->instance_owners.find(p_instance);
	if (E) {
		E->get()++;
	} else {
		material->instance_owners[p_instance] = 1;
	}
}

void RasterizerStorageGLES3::material_remove_instance_owner(RID p_material, RasterizerScene::InstanceBase *p_instance) {
	Material *material = material_owner.get(p_material);
	ERR_FAIL_COND(!material);

	Map<RasterizerScene::InstanceBase *, int>::Element *E = material->instance_owners.find(p_instance);
	ERR_FAIL_COND(!E);
	E->get()--;

	if (E->get() == 0) {
		material->instance_owners.erase(E);
	}
}

void RasterizerStorageGLES3::material_set_render_priority(RID p_material, int priority) {
	ERR_FAIL_COND(priority < VS::MATERIAL_RENDER_PRIORITY_MIN);
	ERR_FAIL_COND(priority > VS::MATERIAL_RENDER_PRIORITY_MAX);

	Material *material = material_owner.get(p_material);
	ERR_FAIL_COND(!material);

	material->render_priority = priority;
}

_FORCE_INLINE_ static void _fill_std140_variant_ubo_value(ShaderLanguage::DataType type, const Variant &value, uint8_t *data, bool p_linear_color) {
	switch (type) {
		case ShaderLanguage::TYPE_BOOL: {
			bool v = value;

			GLuint *gui = (GLuint *)data;
			*gui = v ? GL_TRUE : GL_FALSE;
		} break;
		case ShaderLanguage::TYPE_BVEC2: {
			int v = value;
			GLuint *gui = (GLuint *)data;
			gui[0] = (v & 1) ? GL_TRUE : GL_FALSE;
			gui[1] = (v & 2) ? GL_TRUE : GL_FALSE;

		} break;
		case ShaderLanguage::TYPE_BVEC3: {
			int v = value;
			GLuint *gui = (GLuint *)data;
			gui[0] = (v & 1) ? GL_TRUE : GL_FALSE;
			gui[1] = (v & 2) ? GL_TRUE : GL_FALSE;
			gui[2] = (v & 4) ? GL_TRUE : GL_FALSE;

		} break;
		case ShaderLanguage::TYPE_BVEC4: {
			int v = value;
			GLuint *gui = (GLuint *)data;
			gui[0] = (v & 1) ? GL_TRUE : GL_FALSE;
			gui[1] = (v & 2) ? GL_TRUE : GL_FALSE;
			gui[2] = (v & 4) ? GL_TRUE : GL_FALSE;
			gui[3] = (v & 8) ? GL_TRUE : GL_FALSE;

		} break;
		case ShaderLanguage::TYPE_INT: {
			int v = value;
			GLint *gui = (GLint *)data;
			gui[0] = v;

		} break;
		case ShaderLanguage::TYPE_IVEC2: {
			PoolVector<int> iv = value;
			int s = iv.size();
			GLint *gui = (GLint *)data;

			PoolVector<int>::Read r = iv.read();

			for (int i = 0; i < 2; i++) {
				if (i < s) {
					gui[i] = r[i];
				} else {
					gui[i] = 0;
				}
			}

		} break;
		case ShaderLanguage::TYPE_IVEC3: {
			PoolVector<int> iv = value;
			int s = iv.size();
			GLint *gui = (GLint *)data;

			PoolVector<int>::Read r = iv.read();

			for (int i = 0; i < 3; i++) {
				if (i < s) {
					gui[i] = r[i];
				} else {
					gui[i] = 0;
				}
			}
		} break;
		case ShaderLanguage::TYPE_IVEC4: {
			PoolVector<int> iv = value;
			int s = iv.size();
			GLint *gui = (GLint *)data;

			PoolVector<int>::Read r = iv.read();

			for (int i = 0; i < 4; i++) {
				if (i < s) {
					gui[i] = r[i];
				} else {
					gui[i] = 0;
				}
			}
		} break;
		case ShaderLanguage::TYPE_UINT: {
			int v = value;
			GLuint *gui = (GLuint *)data;
			gui[0] = v;

		} break;
		case ShaderLanguage::TYPE_UVEC2: {
			PoolVector<int> iv = value;
			int s = iv.size();
			GLuint *gui = (GLuint *)data;

			PoolVector<int>::Read r = iv.read();

			for (int i = 0; i < 2; i++) {
				if (i < s) {
					gui[i] = r[i];
				} else {
					gui[i] = 0;
				}
			}
		} break;
		case ShaderLanguage::TYPE_UVEC3: {
			PoolVector<int> iv = value;
			int s = iv.size();
			GLuint *gui = (GLuint *)data;

			PoolVector<int>::Read r = iv.read();

			for (int i = 0; i < 3; i++) {
				if (i < s) {
					gui[i] = r[i];
				} else {
					gui[i] = 0;
				}
			}

		} break;
		case ShaderLanguage::TYPE_UVEC4: {
			PoolVector<int> iv = value;
			int s = iv.size();
			GLuint *gui = (GLuint *)data;

			PoolVector<int>::Read r = iv.read();

			for (int i = 0; i < 4; i++) {
				if (i < s) {
					gui[i] = r[i];
				} else {
					gui[i] = 0;
				}
			}
		} break;
		case ShaderLanguage::TYPE_FLOAT: {
			float v = value;
			GLfloat *gui = (GLfloat *)data;
			gui[0] = v;

		} break;
		case ShaderLanguage::TYPE_VEC2: {
			Vector2 v = value;
			GLfloat *gui = (GLfloat *)data;
			gui[0] = v.x;
			gui[1] = v.y;

		} break;
		case ShaderLanguage::TYPE_VEC3: {
			Vector3 v = value;
			GLfloat *gui = (GLfloat *)data;
			gui[0] = v.x;
			gui[1] = v.y;
			gui[2] = v.z;

		} break;
		case ShaderLanguage::TYPE_VEC4: {
			GLfloat *gui = (GLfloat *)data;

			if (value.get_type() == Variant::COLOR) {
				Color v = value;

				if (p_linear_color) {
					v = v.to_linear();
				}

				gui[0] = v.r;
				gui[1] = v.g;
				gui[2] = v.b;
				gui[3] = v.a;
			} else if (value.get_type() == Variant::RECT2) {
				Rect2 v = value;

				gui[0] = v.position.x;
				gui[1] = v.position.y;
				gui[2] = v.size.x;
				gui[3] = v.size.y;
			} else if (value.get_type() == Variant::QUAT) {
				Quat v = value;

				gui[0] = v.x;
				gui[1] = v.y;
				gui[2] = v.z;
				gui[3] = v.w;
			} else {
				Plane v = value;

				gui[0] = v.normal.x;
				gui[1] = v.normal.y;
				gui[2] = v.normal.z;
				gui[3] = v.d;
			}
		} break;
		case ShaderLanguage::TYPE_MAT2: {
			Transform2D v = value;
			GLfloat *gui = (GLfloat *)data;

			//in std140 members of mat2 are treated as vec4s
			gui[0] = v.elements[0][0];
			gui[1] = v.elements[0][1];
			gui[2] = 0;
			gui[3] = 0;
			gui[4] = v.elements[1][0];
			gui[5] = v.elements[1][1];
			gui[6] = 0;
			gui[7] = 0;
		} break;
		case ShaderLanguage::TYPE_MAT3: {
			Basis v = value;
			GLfloat *gui = (GLfloat *)data;

			gui[0] = v.elements[0][0];
			gui[1] = v.elements[1][0];
			gui[2] = v.elements[2][0];
			gui[3] = 0;
			gui[4] = v.elements[0][1];
			gui[5] = v.elements[1][1];
			gui[6] = v.elements[2][1];
			gui[7] = 0;
			gui[8] = v.elements[0][2];
			gui[9] = v.elements[1][2];
			gui[10] = v.elements[2][2];
			gui[11] = 0;
		} break;
		case ShaderLanguage::TYPE_MAT4: {
			Transform v = value;
			GLfloat *gui = (GLfloat *)data;

			gui[0] = v.basis.elements[0][0];
			gui[1] = v.basis.elements[1][0];
			gui[2] = v.basis.elements[2][0];
			gui[3] = 0;
			gui[4] = v.basis.elements[0][1];
			gui[5] = v.basis.elements[1][1];
			gui[6] = v.basis.elements[2][1];
			gui[7] = 0;
			gui[8] = v.basis.elements[0][2];
			gui[9] = v.basis.elements[1][2];
			gui[10] = v.basis.elements[2][2];
			gui[11] = 0;
			gui[12] = v.origin.x;
			gui[13] = v.origin.y;
			gui[14] = v.origin.z;
			gui[15] = 1;
		} break;
		default: {
		}
	}
}

_FORCE_INLINE_ static void _fill_std140_ubo_value(ShaderLanguage::DataType type, const Vector<ShaderLanguage::ConstantNode::Value> &value, uint8_t *data) {
	switch (type) {
		case ShaderLanguage::TYPE_BOOL: {
			GLuint *gui = (GLuint *)data;
			*gui = value[0].boolean ? GL_TRUE : GL_FALSE;
		} break;
		case ShaderLanguage::TYPE_BVEC2: {
			GLuint *gui = (GLuint *)data;
			gui[0] = value[0].boolean ? GL_TRUE : GL_FALSE;
			gui[1] = value[1].boolean ? GL_TRUE : GL_FALSE;

		} break;
		case ShaderLanguage::TYPE_BVEC3: {
			GLuint *gui = (GLuint *)data;
			gui[0] = value[0].boolean ? GL_TRUE : GL_FALSE;
			gui[1] = value[1].boolean ? GL_TRUE : GL_FALSE;
			gui[2] = value[2].boolean ? GL_TRUE : GL_FALSE;

		} break;
		case ShaderLanguage::TYPE_BVEC4: {
			GLuint *gui = (GLuint *)data;
			gui[0] = value[0].boolean ? GL_TRUE : GL_FALSE;
			gui[1] = value[1].boolean ? GL_TRUE : GL_FALSE;
			gui[2] = value[2].boolean ? GL_TRUE : GL_FALSE;
			gui[3] = value[3].boolean ? GL_TRUE : GL_FALSE;

		} break;
		case ShaderLanguage::TYPE_INT: {
			GLint *gui = (GLint *)data;
			gui[0] = value[0].sint;

		} break;
		case ShaderLanguage::TYPE_IVEC2: {
			GLint *gui = (GLint *)data;

			for (int i = 0; i < 2; i++) {
				gui[i] = value[i].sint;
			}

		} break;
		case ShaderLanguage::TYPE_IVEC3: {
			GLint *gui = (GLint *)data;

			for (int i = 0; i < 3; i++) {
				gui[i] = value[i].sint;
			}

		} break;
		case ShaderLanguage::TYPE_IVEC4: {
			GLint *gui = (GLint *)data;

			for (int i = 0; i < 4; i++) {
				gui[i] = value[i].sint;
			}

		} break;
		case ShaderLanguage::TYPE_UINT: {
			GLuint *gui = (GLuint *)data;
			gui[0] = value[0].uint;

		} break;
		case ShaderLanguage::TYPE_UVEC2: {
			GLint *gui = (GLint *)data;

			for (int i = 0; i < 2; i++) {
				gui[i] = value[i].uint;
			}
		} break;
		case ShaderLanguage::TYPE_UVEC3: {
			GLint *gui = (GLint *)data;

			for (int i = 0; i < 3; i++) {
				gui[i] = value[i].uint;
			}

		} break;
		case ShaderLanguage::TYPE_UVEC4: {
			GLint *gui = (GLint *)data;

			for (int i = 0; i < 4; i++) {
				gui[i] = value[i].uint;
			}
		} break;
		case ShaderLanguage::TYPE_FLOAT: {
			GLfloat *gui = (GLfloat *)data;
			gui[0] = value[0].real;

		} break;
		case ShaderLanguage::TYPE_VEC2: {
			GLfloat *gui = (GLfloat *)data;

			for (int i = 0; i < 2; i++) {
				gui[i] = value[i].real;
			}

		} break;
		case ShaderLanguage::TYPE_VEC3: {
			GLfloat *gui = (GLfloat *)data;

			for (int i = 0; i < 3; i++) {
				gui[i] = value[i].real;
			}

		} break;
		case ShaderLanguage::TYPE_VEC4: {
			GLfloat *gui = (GLfloat *)data;

			for (int i = 0; i < 4; i++) {
				gui[i] = value[i].real;
			}
		} break;
		case ShaderLanguage::TYPE_MAT2: {
			GLfloat *gui = (GLfloat *)data;

			//in std140 members of mat2 are treated as vec4s
			gui[0] = value[0].real;
			gui[1] = value[1].real;
			gui[2] = 0;
			gui[3] = 0;
			gui[4] = value[2].real;
			gui[5] = value[3].real;
			gui[6] = 0;
			gui[7] = 0;
		} break;
		case ShaderLanguage::TYPE_MAT3: {
			GLfloat *gui = (GLfloat *)data;

			gui[0] = value[0].real;
			gui[1] = value[1].real;
			gui[2] = value[2].real;
			gui[3] = 0;
			gui[4] = value[3].real;
			gui[5] = value[4].real;
			gui[6] = value[5].real;
			gui[7] = 0;
			gui[8] = value[6].real;
			gui[9] = value[7].real;
			gui[10] = value[8].real;
			gui[11] = 0;
		} break;
		case ShaderLanguage::TYPE_MAT4: {
			GLfloat *gui = (GLfloat *)data;

			for (int i = 0; i < 16; i++) {
				gui[i] = value[i].real;
			}
		} break;
		default: {
		}
	}
}

_FORCE_INLINE_ static void _fill_std140_ubo_empty(ShaderLanguage::DataType type, uint8_t *data) {
	switch (type) {
		case ShaderLanguage::TYPE_BOOL:
		case ShaderLanguage::TYPE_INT:
		case ShaderLanguage::TYPE_UINT:
		case ShaderLanguage::TYPE_FLOAT: {
			memset(data, 0, 4);
		} break;
		case ShaderLanguage::TYPE_BVEC2:
		case ShaderLanguage::TYPE_IVEC2:
		case ShaderLanguage::TYPE_UVEC2:
		case ShaderLanguage::TYPE_VEC2: {
			memset(data, 0, 8);
		} break;
		case ShaderLanguage::TYPE_BVEC3:
		case ShaderLanguage::TYPE_IVEC3:
		case ShaderLanguage::TYPE_UVEC3:
		case ShaderLanguage::TYPE_VEC3: {
			memset(data, 0, 12);
		} break;
		case ShaderLanguage::TYPE_BVEC4:
		case ShaderLanguage::TYPE_IVEC4:
		case ShaderLanguage::TYPE_UVEC4:
		case ShaderLanguage::TYPE_VEC4: {
			memset(data, 0, 16);
		} break;
		case ShaderLanguage::TYPE_MAT2: {
			memset(data, 0, 32);
		} break;
		case ShaderLanguage::TYPE_MAT3: {
			memset(data, 0, 48);
		} break;
		case ShaderLanguage::TYPE_MAT4: {
			memset(data, 0, 64);
		} break;

		default: {
		}
	}
}

void RasterizerStorageGLES3::_update_material(Material *material) {
	if (material->dirty_list.in_list()) {
		_material_dirty_list.remove(&material->dirty_list);
	}

	if (material->shader && material->shader->dirty_list.in_list()) {
		_update_shader(material->shader);
	}

	if (material->shader && !material->shader->valid) {
		return;
	}

	//update caches

	{
		bool can_cast_shadow = false;
		bool is_animated = false;

		if (material->shader && material->shader->mode == VS::SHADER_SPATIAL) {
			if (material->shader->spatial.blend_mode == Shader::Spatial::BLEND_MODE_MIX &&
					(!(material->shader->spatial.uses_alpha && !material->shader->spatial.uses_alpha_scissor) || material->shader->spatial.depth_draw_mode == Shader::Spatial::DEPTH_DRAW_ALPHA_PREPASS)) {
				can_cast_shadow = true;
			}

			if (material->shader->spatial.uses_discard && material->shader->uses_fragment_time) {
				is_animated = true;
			}

			if (material->shader->spatial.uses_vertex && material->shader->uses_vertex_time) {
				is_animated = true;
			}

			if (can_cast_shadow != material->can_cast_shadow_cache || is_animated != material->is_animated_cache) {
				material->can_cast_shadow_cache = can_cast_shadow;
				material->is_animated_cache = is_animated;

				for (Map<Geometry *, int>::Element *E = material->geometry_owners.front(); E; E = E->next()) {
					E->key()->material_changed_notify();
				}

				for (Map<RasterizerScene::InstanceBase *, int>::Element *E = material->instance_owners.front(); E; E = E->next()) {
					E->key()->base_changed(false, true);
				}
			}
		}
	}

	//clear ubo if it needs to be cleared
	if (material->ubo_size) {
		if (!material->shader || material->shader->ubo_size != material->ubo_size) {
			//by by ubo
			glDeleteBuffers(1, &material->ubo_id);
			material->ubo_id = 0;
			material->ubo_size = 0;
		}
	}

	//create ubo if it needs to be created
	if (material->ubo_size == 0 && material->shader && material->shader->ubo_size) {
		glGenBuffers(1, &material->ubo_id);
		glBindBuffer(GL_UNIFORM_BUFFER, material->ubo_id);
		glBufferData(GL_UNIFORM_BUFFER, material->shader->ubo_size, nullptr, GL_STATIC_DRAW);
		glBindBuffer(GL_UNIFORM_BUFFER, 0);
		material->ubo_size = material->shader->ubo_size;
	}

	//fill up the UBO if it needs to be filled
	if (material->shader && material->ubo_size) {
		uint8_t *local_ubo = (uint8_t *)alloca(material->ubo_size);

		for (Map<StringName, ShaderLanguage::ShaderNode::Uniform>::Element *E = material->shader->uniforms.front(); E; E = E->next()) {
			if (E->get().order < 0) {
				continue; // texture, does not go here
			}

			//regular uniform
			uint8_t *data = &local_ubo[material->shader->ubo_offsets[E->get().order]];

			Map<StringName, Variant>::Element *V = material->params.find(E->key());

			if (V) {
				//user provided
				_fill_std140_variant_ubo_value(E->get().type, V->get(), data, material->shader->mode == VS::SHADER_SPATIAL);

			} else if (E->get().default_value.size()) {
				//default value
				_fill_std140_ubo_value(E->get().type, E->get().default_value, data);
				//value=E->get().default_value;
			} else {
				//zero because it was not provided
				if (E->get().type == ShaderLanguage::TYPE_VEC4 && E->get().hint == ShaderLanguage::ShaderNode::Uniform::HINT_COLOR) {
					//colors must be set as black, with alpha as 1.0
					_fill_std140_variant_ubo_value(E->get().type, Color(0, 0, 0, 1), data, material->shader->mode == VS::SHADER_SPATIAL);
				} else {
					//else just zero it out
					_fill_std140_ubo_empty(E->get().type, data);
				}
			}
		}

		glBindBuffer(GL_UNIFORM_BUFFER, material->ubo_id);
		glBufferData(GL_UNIFORM_BUFFER, material->ubo_size, local_ubo, GL_STATIC_DRAW);
		glBindBuffer(GL_UNIFORM_BUFFER, 0);
	}

	//set up the texture array, for easy access when it needs to be drawn
	if (material->shader && material->shader->texture_count) {
		material->texture_is_3d.resize(material->shader->texture_count);
		material->textures.resize(material->shader->texture_count);

		for (Map<StringName, ShaderLanguage::ShaderNode::Uniform>::Element *E = material->shader->uniforms.front(); E; E = E->next()) {
			if (E->get().texture_order < 0) {
				continue; // not a texture, does not go here
			}

			RID texture;

			switch (E->get().type) {
				case ShaderLanguage::TYPE_SAMPLER3D:
				case ShaderLanguage::TYPE_SAMPLER2DARRAY: {
					material->texture_is_3d.write[E->get().texture_order] = true;
				} break;
				default: {
					material->texture_is_3d.write[E->get().texture_order] = false;
				} break;
			}

			Map<StringName, Variant>::Element *V = material->params.find(E->key());
			if (V) {
				texture = V->get();
			}

			if (!texture.is_valid()) {
				Map<StringName, RID>::Element *W = material->shader->default_textures.find(E->key());
				if (W) {
					texture = W->get();
				}
			}

			material->textures.write[E->get().texture_order] = texture;
		}

	} else {
		material->textures.clear();
		material->texture_is_3d.clear();
	}
}

void RasterizerStorageGLES3::_material_add_geometry(RID p_material, Geometry *p_geometry) {
	Material *material = material_owner.getornull(p_material);
	ERR_FAIL_COND(!material);

	Map<Geometry *, int>::Element *I = material->geometry_owners.find(p_geometry);

	if (I) {
		I->get()++;
	} else {
		material->geometry_owners[p_geometry] = 1;
	}
}

void RasterizerStorageGLES3::_material_remove_geometry(RID p_material, Geometry *p_geometry) {
	Material *material = material_owner.getornull(p_material);
	ERR_FAIL_COND(!material);

	Map<Geometry *, int>::Element *I = material->geometry_owners.find(p_geometry);
	ERR_FAIL_COND(!I);

	I->get()--;
	if (I->get() == 0) {
		material->geometry_owners.erase(I);
	}
}

void RasterizerStorageGLES3::update_dirty_materials() {
	while (_material_dirty_list.first()) {
		Material *material = _material_dirty_list.first()->self();

		_update_material(material);
	}
}

/* MESH API */

RID RasterizerStorageGLES3::mesh_create() {
	Mesh *mesh = memnew(Mesh);

	return mesh_owner.make_rid(mesh);
}

void RasterizerStorageGLES3::mesh_add_surface(RID p_mesh, uint32_t p_format, VS::PrimitiveType p_primitive, const PoolVector<uint8_t> &p_array, int p_vertex_count, const PoolVector<uint8_t> &p_index_array, int p_index_count, const AABB &p_aabb, const Vector<PoolVector<uint8_t>> &p_blend_shapes, const Vector<AABB> &p_bone_aabbs) {
	PoolVector<uint8_t> array = p_array;

	Mesh *mesh = mesh_owner.getornull(p_mesh);
	ERR_FAIL_COND(!mesh);

	ERR_FAIL_COND(!(p_format & VS::ARRAY_FORMAT_VERTEX));

	//must have index and bones, both.
	{
		uint32_t bones_weight = VS::ARRAY_FORMAT_BONES | VS::ARRAY_FORMAT_WEIGHTS;
		ERR_FAIL_COND_MSG((p_format & bones_weight) && (p_format & bones_weight) != bones_weight, "Array must have both bones and weights in format or none.");
	}

	//bool has_morph = p_blend_shapes.size();
	bool use_split_stream = GLOBAL_GET("rendering/misc/mesh_storage/split_stream") && !(p_format & VS::ARRAY_FLAG_USE_DYNAMIC_UPDATE);

	Surface::Attrib attribs[VS::ARRAY_MAX];

	int attributes_base_offset = 0;
	int attributes_stride = 0;
	int positions_stride = 0;

	for (int i = 0; i < VS::ARRAY_MAX; i++) {
		attribs[i].index = i;

		if (!(p_format & (1 << i))) {
			attribs[i].enabled = false;
			attribs[i].integer = false;
			continue;
		}

		attribs[i].enabled = true;
		attribs[i].offset = attributes_base_offset + attributes_stride;
		attribs[i].integer = false;

		switch (i) {
			case VS::ARRAY_VERTEX: {
				if (p_format & VS::ARRAY_FLAG_USE_2D_VERTICES) {
					attribs[i].size = 2;
				} else {
					attribs[i].size = (p_format & VS::ARRAY_COMPRESS_VERTEX) ? 4 : 3;
				}

				if (p_format & VS::ARRAY_COMPRESS_VERTEX) {
					attribs[i].type = GL_HALF_FLOAT;
					positions_stride += attribs[i].size * 2;
				} else {
					attribs[i].type = GL_FLOAT;
					positions_stride += attribs[i].size * 4;
				}

				attribs[i].normalized = GL_FALSE;

				if (use_split_stream) {
					attributes_base_offset = positions_stride * p_vertex_count;
				} else {
					attributes_base_offset = positions_stride;
				}

			} break;
			case VS::ARRAY_NORMAL: {
				if (p_format & VS::ARRAY_FLAG_USE_OCTAHEDRAL_COMPRESSION) {
					// Always pack normal and tangent into vec4
					// normal will be xy tangent will be zw
					// normal will always be oct32 (4 byte) encoded
					// UNLESS tangent exists and is also compressed
					// then it will be oct16 encoded along with tangent
					attribs[i].normalized = GL_TRUE;
					attribs[i].size = 2;
					attribs[i].type = GL_SHORT;
					attributes_stride += 4;
					// Storing normal/tangent in the tangent attrib makes it easier to ubershaderify the scene shader
					attribs[i].index = VS::ARRAY_TANGENT;
				} else {
					attribs[i].size = 3;

					if (p_format & VS::ARRAY_COMPRESS_NORMAL) {
						attribs[i].type = GL_BYTE;
						attributes_stride += 4; //pad extra byte
						attribs[i].normalized = GL_TRUE;
					} else {
						attribs[i].type = GL_FLOAT;
						attributes_stride += 12;
						attribs[i].normalized = GL_FALSE;
					}
				}

			} break;
			case VS::ARRAY_TANGENT: {
				if (p_format & VS::ARRAY_FLAG_USE_OCTAHEDRAL_COMPRESSION) {
					attribs[i].enabled = false;
					attribs[VS::ARRAY_NORMAL].size = 4;
					if (p_format & VS::ARRAY_COMPRESS_TANGENT && p_format & VS::ARRAY_COMPRESS_NORMAL) {
						// normal and tangent will each be oct16 (2 bytes each)
						// pack into single vec4<GL_BYTE> for memory bandwidth
						// savings while keeping 4 byte alignment
						attribs[VS::ARRAY_NORMAL].type = GL_BYTE;
					} else {
						// normal and tangent will each be oct32 (4 bytes each)
						attributes_stride += 4;
					}
				} else {
					attribs[i].size = 4;

					if (p_format & VS::ARRAY_COMPRESS_TANGENT) {
						attribs[i].type = GL_BYTE;
						attributes_stride += 4;
						attribs[i].normalized = GL_TRUE;
					} else {
						attribs[i].type = GL_FLOAT;
						attributes_stride += 16;
						attribs[i].normalized = GL_FALSE;
					}
				}

			} break;
			case VS::ARRAY_COLOR: {
				attribs[i].size = 4;

				if (p_format & VS::ARRAY_COMPRESS_COLOR) {
					attribs[i].type = GL_UNSIGNED_BYTE;
					attributes_stride += 4;
					attribs[i].normalized = GL_TRUE;
				} else {
					attribs[i].type = GL_FLOAT;
					attributes_stride += 16;
					attribs[i].normalized = GL_FALSE;
				}

			} break;
			case VS::ARRAY_TEX_UV: {
				attribs[i].size = 2;

				if (p_format & VS::ARRAY_COMPRESS_TEX_UV) {
					attribs[i].type = GL_HALF_FLOAT;
					attributes_stride += 4;
				} else {
					attribs[i].type = GL_FLOAT;
					attributes_stride += 8;
				}

				attribs[i].normalized = GL_FALSE;

			} break;
			case VS::ARRAY_TEX_UV2: {
				attribs[i].size = 2;

				if (p_format & VS::ARRAY_COMPRESS_TEX_UV2) {
					attribs[i].type = GL_HALF_FLOAT;
					attributes_stride += 4;
				} else {
					attribs[i].type = GL_FLOAT;
					attributes_stride += 8;
				}
				attribs[i].normalized = GL_FALSE;

			} break;
			case VS::ARRAY_BONES: {
				attribs[i].size = 4;

				if (p_format & VS::ARRAY_FLAG_USE_16_BIT_BONES) {
					attribs[i].type = GL_UNSIGNED_SHORT;
					attributes_stride += 8;
				} else {
					attribs[i].type = GL_UNSIGNED_BYTE;
					attributes_stride += 4;
				}

				attribs[i].normalized = GL_FALSE;
				attribs[i].integer = true;

			} break;
			case VS::ARRAY_WEIGHTS: {
				attribs[i].size = 4;

				if (p_format & VS::ARRAY_COMPRESS_WEIGHTS) {
					attribs[i].type = GL_UNSIGNED_SHORT;
					attributes_stride += 8;
					attribs[i].normalized = GL_TRUE;
				} else {
					attribs[i].type = GL_FLOAT;
					attributes_stride += 16;
					attribs[i].normalized = GL_FALSE;
				}

			} break;
			case VS::ARRAY_INDEX: {
				attribs[i].size = 1;

				if (p_vertex_count >= (1 << 16)) {
					attribs[i].type = GL_UNSIGNED_INT;
					attribs[i].stride = 4;
				} else {
					attribs[i].type = GL_UNSIGNED_SHORT;
					attribs[i].stride = 2;
				}

				attribs[i].normalized = GL_FALSE;

			} break;
		}
	}

	if (use_split_stream) {
		attribs[VS::ARRAY_VERTEX].stride = positions_stride;
		for (int i = 1; i < VS::ARRAY_MAX - 1; i++) {
			attribs[i].stride = attributes_stride;
		}
	} else {
		for (int i = 0; i < VS::ARRAY_MAX - 1; i++) {
			attribs[i].stride = positions_stride + attributes_stride;
		}
	}

	//validate sizes

	int stride = positions_stride + attributes_stride;
	int array_size = stride * p_vertex_count;
	int index_array_size = 0;
	if (array.size() != array_size && array.size() + p_vertex_count * 2 == array_size) {
		//old format, convert
		array = PoolVector<uint8_t>();

		array.resize(p_array.size() + p_vertex_count * 2);

		PoolVector<uint8_t>::Write w = array.write();
		PoolVector<uint8_t>::Read r = p_array.read();

		uint16_t *w16 = (uint16_t *)w.ptr();
		const uint16_t *r16 = (uint16_t *)r.ptr();

		uint16_t one = Math::make_half_float(1);

		for (int i = 0; i < p_vertex_count; i++) {
			*w16++ = *r16++;
			*w16++ = *r16++;
			*w16++ = *r16++;
			*w16++ = one;
			for (int j = 0; j < (stride / 2) - 4; j++) {
				*w16++ = *r16++;
			}
		}
	}

	ERR_FAIL_COND(array.size() != array_size);

	if (p_format & VS::ARRAY_FORMAT_INDEX) {
		index_array_size = attribs[VS::ARRAY_INDEX].stride * p_index_count;
	}

	ERR_FAIL_COND(p_index_array.size() != index_array_size);

	ERR_FAIL_COND(p_blend_shapes.size() != mesh->blend_shape_count);

	for (int i = 0; i < p_blend_shapes.size(); i++) {
		ERR_FAIL_COND(p_blend_shapes[i].size() != array_size);
	}

	//ok all valid, create stuff

	Surface *surface = memnew(Surface);

	surface->active = true;
	surface->array_len = p_vertex_count;
	surface->index_array_len = p_index_count;
	surface->array_byte_size = array.size();
	surface->index_array_byte_size = p_index_array.size();
	surface->primitive = p_primitive;
	surface->mesh = mesh;
	surface->format = p_format;
	surface->skeleton_bone_aabb = p_bone_aabbs;
	surface->skeleton_bone_used.resize(surface->skeleton_bone_aabb.size());
	surface->aabb = p_aabb;
	surface->max_bone = p_bone_aabbs.size();
	surface->total_data_size += surface->array_byte_size + surface->index_array_byte_size;

	for (int i = 0; i < surface->skeleton_bone_used.size(); i++) {
		if (surface->skeleton_bone_aabb[i].size.x < 0 || surface->skeleton_bone_aabb[i].size.y < 0 || surface->skeleton_bone_aabb[i].size.z < 0) {
			surface->skeleton_bone_used.write[i] = false;
		} else {
			surface->skeleton_bone_used.write[i] = true;
		}
	}

	for (int i = 0; i < VS::ARRAY_MAX; i++) {
		surface->attribs[i] = attribs[i];
	}

	{
		PoolVector<uint8_t>::Read vr = array.read();

		glGenBuffers(1, &surface->vertex_id);
		glBindBuffer(GL_ARRAY_BUFFER, surface->vertex_id);
		glBufferData(GL_ARRAY_BUFFER, array_size, vr.ptr(), (p_format & VS::ARRAY_FLAG_USE_DYNAMIC_UPDATE) ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW);
		glBindBuffer(GL_ARRAY_BUFFER, 0); //unbind

		if (p_format & VS::ARRAY_FORMAT_INDEX) {
			PoolVector<uint8_t>::Read ir = p_index_array.read();

			glGenBuffers(1, &surface->index_id);
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, surface->index_id);
			glBufferData(GL_ELEMENT_ARRAY_BUFFER, index_array_size, ir.ptr(), GL_STATIC_DRAW);
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0); //unbind
		}

		//generate arrays for faster state switching

		for (int ai = 0; ai < 2; ai++) {
			if (ai == 0) {
				//for normal draw
				glGenVertexArrays(1, &surface->array_id);
				glBindVertexArray(surface->array_id);
				glBindBuffer(GL_ARRAY_BUFFER, surface->vertex_id);
			} else if (ai == 1) {
				//for instancing draw (can be changed and no one cares)
				glGenVertexArrays(1, &surface->instancing_array_id);
				glBindVertexArray(surface->instancing_array_id);
				glBindBuffer(GL_ARRAY_BUFFER, surface->vertex_id);
			}

			for (int i = 0; i < VS::ARRAY_MAX - 1; i++) {
				if (!attribs[i].enabled) {
					continue;
				}

				if (attribs[i].integer) {
					glVertexAttribIPointer(attribs[i].index, attribs[i].size, attribs[i].type, attribs[i].stride, CAST_INT_TO_UCHAR_PTR(attribs[i].offset));
				} else {
					glVertexAttribPointer(attribs[i].index, attribs[i].size, attribs[i].type, attribs[i].normalized, attribs[i].stride, CAST_INT_TO_UCHAR_PTR(attribs[i].offset));
				}
				glEnableVertexAttribArray(attribs[i].index);
			}

			if (surface->index_id) {
				glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, surface->index_id);
			}

			glBindVertexArray(0);
			glBindBuffer(GL_ARRAY_BUFFER, 0); //unbind
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
		}

#ifdef DEBUG_ENABLED

		if (config.generate_wireframes && p_primitive == VS::PRIMITIVE_TRIANGLES) {
			//generate wireframes, this is used mostly by editor
			PoolVector<uint32_t> wf_indices;
			int index_count;

			if (p_format & VS::ARRAY_FORMAT_INDEX) {
				index_count = p_index_count * 2;
				wf_indices.resize(index_count);

				PoolVector<uint8_t>::Read ir = p_index_array.read();
				PoolVector<uint32_t>::Write wr = wf_indices.write();

				if (p_vertex_count < (1 << 16)) {
					//read 16 bit indices
					const uint16_t *src_idx = (const uint16_t *)ir.ptr();
					for (int i = 0; i + 5 < index_count; i += 6) {
						wr[i + 0] = src_idx[i / 2];
						wr[i + 1] = src_idx[i / 2 + 1];
						wr[i + 2] = src_idx[i / 2 + 1];
						wr[i + 3] = src_idx[i / 2 + 2];
						wr[i + 4] = src_idx[i / 2 + 2];
						wr[i + 5] = src_idx[i / 2];
					}

				} else {
					//read 16 bit indices
					const uint32_t *src_idx = (const uint32_t *)ir.ptr();
					for (int i = 0; i + 5 < index_count; i += 6) {
						wr[i + 0] = src_idx[i / 2];
						wr[i + 1] = src_idx[i / 2 + 1];
						wr[i + 2] = src_idx[i / 2 + 1];
						wr[i + 3] = src_idx[i / 2 + 2];
						wr[i + 4] = src_idx[i / 2 + 2];
						wr[i + 5] = src_idx[i / 2];
					}
				}

			} else {
				index_count = p_vertex_count * 2;
				wf_indices.resize(index_count);
				PoolVector<uint32_t>::Write wr = wf_indices.write();
				for (int i = 0; i + 5 < index_count; i += 6) {
					wr[i + 0] = i / 2;
					wr[i + 1] = i / 2 + 1;
					wr[i + 2] = i / 2 + 1;
					wr[i + 3] = i / 2 + 2;
					wr[i + 4] = i / 2 + 2;
					wr[i + 5] = i / 2;
				}
			}
			{
				PoolVector<uint32_t>::Read ir = wf_indices.read();

				glGenBuffers(1, &surface->index_wireframe_id);
				glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, surface->index_wireframe_id);
				glBufferData(GL_ELEMENT_ARRAY_BUFFER, index_count * sizeof(uint32_t), ir.ptr(), GL_STATIC_DRAW);
				glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0); //unbind

				surface->index_wireframe_len = index_count;
			}

			for (int ai = 0; ai < 2; ai++) {
				if (ai == 0) {
					//for normal draw
					glGenVertexArrays(1, &surface->array_wireframe_id);
					glBindVertexArray(surface->array_wireframe_id);
					glBindBuffer(GL_ARRAY_BUFFER, surface->vertex_id);
				} else if (ai == 1) {
					//for instancing draw (can be changed and no one cares)
					glGenVertexArrays(1, &surface->instancing_array_wireframe_id);
					glBindVertexArray(surface->instancing_array_wireframe_id);
					glBindBuffer(GL_ARRAY_BUFFER, surface->vertex_id);
				}

				for (int i = 0; i < VS::ARRAY_MAX - 1; i++) {
					if (!attribs[i].enabled) {
						continue;
					}

					if (attribs[i].integer) {
						glVertexAttribIPointer(attribs[i].index, attribs[i].size, attribs[i].type, attribs[i].stride, CAST_INT_TO_UCHAR_PTR(attribs[i].offset));
					} else {
						glVertexAttribPointer(attribs[i].index, attribs[i].size, attribs[i].type, attribs[i].normalized, attribs[i].stride, CAST_INT_TO_UCHAR_PTR(attribs[i].offset));
					}
					glEnableVertexAttribArray(attribs[i].index);
				}

				glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, surface->index_wireframe_id);

				glBindVertexArray(0);
				glBindBuffer(GL_ARRAY_BUFFER, 0); //unbind
				glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
			}
		}

#endif
	}

	{
		//blend shapes

		for (int i = 0; i < p_blend_shapes.size(); i++) {
			Surface::BlendShape mt;

			PoolVector<uint8_t>::Read vr = p_blend_shapes[i].read();

			surface->total_data_size += array_size;

			glGenBuffers(1, &mt.vertex_id);
			glBindBuffer(GL_ARRAY_BUFFER, mt.vertex_id);
			glBufferData(GL_ARRAY_BUFFER, array_size, vr.ptr(), GL_STATIC_DRAW);
			glBindBuffer(GL_ARRAY_BUFFER, 0); //unbind

			glGenVertexArrays(1, &mt.array_id);
			glBindVertexArray(mt.array_id);
			glBindBuffer(GL_ARRAY_BUFFER, mt.vertex_id);

			for (int j = 0; j < VS::ARRAY_MAX - 1; j++) {
				if (!attribs[j].enabled) {
					continue;
				}

				if (attribs[j].integer) {
					glVertexAttribIPointer(attribs[j].index, attribs[j].size, attribs[j].type, attribs[j].stride, CAST_INT_TO_UCHAR_PTR(attribs[j].offset));
				} else {
					glVertexAttribPointer(attribs[j].index, attribs[j].size, attribs[j].type, attribs[j].normalized, attribs[j].stride, CAST_INT_TO_UCHAR_PTR(attribs[j].offset));
				}
				glEnableVertexAttribArray(attribs[j].index);
			}

			glBindVertexArray(0);
			glBindBuffer(GL_ARRAY_BUFFER, 0); //unbind

			surface->blend_shapes.push_back(mt);
		}
	}

	mesh->surfaces.push_back(surface);
	mesh->instance_change_notify(true, true);

	info.vertex_mem += surface->total_data_size;
}

void RasterizerStorageGLES3::mesh_set_blend_shape_count(RID p_mesh, int p_amount) {
	Mesh *mesh = mesh_owner.getornull(p_mesh);
	ERR_FAIL_COND(!mesh);

	ERR_FAIL_COND(mesh->surfaces.size() != 0);
	ERR_FAIL_COND(p_amount < 0);

	mesh->blend_shape_count = p_amount;
	mesh->instance_change_notify(true, false);
}
int RasterizerStorageGLES3::mesh_get_blend_shape_count(RID p_mesh) const {
	const Mesh *mesh = mesh_owner.getornull(p_mesh);
	ERR_FAIL_COND_V(!mesh, 0);

	return mesh->blend_shape_count;
}

void RasterizerStorageGLES3::mesh_set_blend_shape_mode(RID p_mesh, VS::BlendShapeMode p_mode) {
	Mesh *mesh = mesh_owner.getornull(p_mesh);
	ERR_FAIL_COND(!mesh);

	mesh->blend_shape_mode = p_mode;
}
VS::BlendShapeMode RasterizerStorageGLES3::mesh_get_blend_shape_mode(RID p_mesh) const {
	const Mesh *mesh = mesh_owner.getornull(p_mesh);
	ERR_FAIL_COND_V(!mesh, VS::BLEND_SHAPE_MODE_NORMALIZED);

	return mesh->blend_shape_mode;
}

void RasterizerStorageGLES3::mesh_set_blend_shape_values(RID p_mesh, PoolVector<float> p_values) {
	Mesh *mesh = mesh_owner.getornull(p_mesh);
	ERR_FAIL_COND(!mesh);
	mesh->blend_shape_values = p_values;
}

PoolVector<float> RasterizerStorageGLES3::mesh_get_blend_shape_values(RID p_mesh) const {
	const Mesh *mesh = mesh_owner.getornull(p_mesh);
	ERR_FAIL_COND_V(!mesh, PoolVector<float>());
	return mesh->blend_shape_values;
}

void RasterizerStorageGLES3::mesh_surface_update_region(RID p_mesh, int p_surface, int p_offset, const PoolVector<uint8_t> &p_data) {
	Mesh *mesh = mesh_owner.getornull(p_mesh);
	ERR_FAIL_COND(!mesh);
	ERR_FAIL_INDEX(p_surface, mesh->surfaces.size());

	int total_size = p_data.size();
	ERR_FAIL_COND(p_offset + total_size > mesh->surfaces[p_surface]->array_byte_size);

	PoolVector<uint8_t>::Read r = p_data.read();

	glBindBuffer(GL_ARRAY_BUFFER, mesh->surfaces[p_surface]->vertex_id);
	glBufferSubData(GL_ARRAY_BUFFER, p_offset, total_size, r.ptr());
	glBindBuffer(GL_ARRAY_BUFFER, 0); //unbind
}

void RasterizerStorageGLES3::mesh_surface_set_material(RID p_mesh, int p_surface, RID p_material) {
	Mesh *mesh = mesh_owner.getornull(p_mesh);
	ERR_FAIL_COND(!mesh);
	ERR_FAIL_INDEX(p_surface, mesh->surfaces.size());

	if (mesh->surfaces[p_surface]->material == p_material) {
		return;
	}

	if (mesh->surfaces[p_surface]->material.is_valid()) {
		_material_remove_geometry(mesh->surfaces[p_surface]->material, mesh->surfaces[p_surface]);
	}

	mesh->surfaces[p_surface]->material = p_material;

	if (mesh->surfaces[p_surface]->material.is_valid()) {
		_material_add_geometry(mesh->surfaces[p_surface]->material, mesh->surfaces[p_surface]);
	}

	mesh->instance_change_notify(false, true);
}
RID RasterizerStorageGLES3::mesh_surface_get_material(RID p_mesh, int p_surface) const {
	const Mesh *mesh = mesh_owner.getornull(p_mesh);
	ERR_FAIL_COND_V(!mesh, RID());
	ERR_FAIL_INDEX_V(p_surface, mesh->surfaces.size(), RID());

	return mesh->surfaces[p_surface]->material;
}

int RasterizerStorageGLES3::mesh_surface_get_array_len(RID p_mesh, int p_surface) const {
	const Mesh *mesh = mesh_owner.getornull(p_mesh);
	ERR_FAIL_COND_V(!mesh, 0);
	ERR_FAIL_INDEX_V(p_surface, mesh->surfaces.size(), 0);

	return mesh->surfaces[p_surface]->array_len;
}
int RasterizerStorageGLES3::mesh_surface_get_array_index_len(RID p_mesh, int p_surface) const {
	const Mesh *mesh = mesh_owner.getornull(p_mesh);
	ERR_FAIL_COND_V(!mesh, 0);
	ERR_FAIL_INDEX_V(p_surface, mesh->surfaces.size(), 0);

	return mesh->surfaces[p_surface]->index_array_len;
}

PoolVector<uint8_t> RasterizerStorageGLES3::mesh_surface_get_array(RID p_mesh, int p_surface) const {
	const Mesh *mesh = mesh_owner.getornull(p_mesh);
	ERR_FAIL_COND_V(!mesh, PoolVector<uint8_t>());
	ERR_FAIL_INDEX_V(p_surface, mesh->surfaces.size(), PoolVector<uint8_t>());

	Surface *surface = mesh->surfaces[p_surface];

	PoolVector<uint8_t> ret;
	ret.resize(surface->array_byte_size);
	glBindBuffer(GL_ARRAY_BUFFER, surface->vertex_id);

#if defined(GLES_OVER_GL) || defined(__EMSCRIPTEN__)
	{
		PoolVector<uint8_t>::Write w = ret.write();
		glGetBufferSubData(GL_ARRAY_BUFFER, 0, surface->array_byte_size, w.ptr());
	}
#else
	void *data = glMapBufferRange(GL_ARRAY_BUFFER, 0, surface->array_byte_size, GL_MAP_READ_BIT);
	ERR_FAIL_NULL_V(data, PoolVector<uint8_t>());
	{
		PoolVector<uint8_t>::Write w = ret.write();
		memcpy(w.ptr(), data, surface->array_byte_size);
	}
	glUnmapBuffer(GL_ARRAY_BUFFER);
#endif

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	return ret;
}

PoolVector<uint8_t> RasterizerStorageGLES3::mesh_surface_get_index_array(RID p_mesh, int p_surface) const {
	const Mesh *mesh = mesh_owner.getornull(p_mesh);
	ERR_FAIL_COND_V(!mesh, PoolVector<uint8_t>());
	ERR_FAIL_INDEX_V(p_surface, mesh->surfaces.size(), PoolVector<uint8_t>());

	Surface *surface = mesh->surfaces[p_surface];

	PoolVector<uint8_t> ret;
	ret.resize(surface->index_array_byte_size);

	if (surface->index_array_byte_size > 0) {
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, surface->index_id);

#if defined(GLES_OVER_GL) || defined(__EMSCRIPTEN__)
		{
			PoolVector<uint8_t>::Write w = ret.write();
			glGetBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, surface->index_array_byte_size, w.ptr());
		}
#else
		void *data = glMapBufferRange(GL_ELEMENT_ARRAY_BUFFER, 0, surface->index_array_byte_size, GL_MAP_READ_BIT);
		ERR_FAIL_NULL_V(data, PoolVector<uint8_t>());
		{
			PoolVector<uint8_t>::Write w = ret.write();
			memcpy(w.ptr(), data, surface->index_array_byte_size);
		}
		glUnmapBuffer(GL_ELEMENT_ARRAY_BUFFER);
#endif

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	}

	return ret;
}

uint32_t RasterizerStorageGLES3::mesh_surface_get_format(RID p_mesh, int p_surface) const {
	const Mesh *mesh = mesh_owner.getornull(p_mesh);

	ERR_FAIL_COND_V(!mesh, 0);
	ERR_FAIL_INDEX_V(p_surface, mesh->surfaces.size(), 0);

	return mesh->surfaces[p_surface]->format;
}

VS::PrimitiveType RasterizerStorageGLES3::mesh_surface_get_primitive_type(RID p_mesh, int p_surface) const {
	const Mesh *mesh = mesh_owner.getornull(p_mesh);
	ERR_FAIL_COND_V(!mesh, VS::PRIMITIVE_MAX);
	ERR_FAIL_INDEX_V(p_surface, mesh->surfaces.size(), VS::PRIMITIVE_MAX);

	return mesh->surfaces[p_surface]->primitive;
}

AABB RasterizerStorageGLES3::mesh_surface_get_aabb(RID p_mesh, int p_surface) const {
	const Mesh *mesh = mesh_owner.getornull(p_mesh);
	ERR_FAIL_COND_V(!mesh, AABB());
	ERR_FAIL_INDEX_V(p_surface, mesh->surfaces.size(), AABB());

	return mesh->surfaces[p_surface]->aabb;
}
Vector<PoolVector<uint8_t>> RasterizerStorageGLES3::mesh_surface_get_blend_shapes(RID p_mesh, int p_surface) const {
	const Mesh *mesh = mesh_owner.getornull(p_mesh);
	ERR_FAIL_COND_V(!mesh, Vector<PoolVector<uint8_t>>());
	ERR_FAIL_INDEX_V(p_surface, mesh->surfaces.size(), Vector<PoolVector<uint8_t>>());

	Vector<PoolVector<uint8_t>> bsarr;

	for (int i = 0; i < mesh->surfaces[p_surface]->blend_shapes.size(); i++) {
		PoolVector<uint8_t> ret;
		ret.resize(mesh->surfaces[p_surface]->array_byte_size);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh->surfaces[p_surface]->blend_shapes[i].vertex_id);

#if defined(GLES_OVER_GL) || defined(__EMSCRIPTEN__)
		{
			PoolVector<uint8_t>::Write w = ret.write();
			glGetBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, mesh->surfaces[p_surface]->array_byte_size, w.ptr());
		}
#else
		void *data = glMapBufferRange(GL_ELEMENT_ARRAY_BUFFER, 0, mesh->surfaces[p_surface]->array_byte_size, GL_MAP_READ_BIT);
		ERR_FAIL_COND_V(!data, Vector<PoolVector<uint8_t>>());
		{
			PoolVector<uint8_t>::Write w = ret.write();
			memcpy(w.ptr(), data, mesh->surfaces[p_surface]->array_byte_size);
		}
		glUnmapBuffer(GL_ELEMENT_ARRAY_BUFFER);
#endif

		bsarr.push_back(ret);
	}

	return bsarr;
}

Vector<AABB> RasterizerStorageGLES3::mesh_surface_get_skeleton_aabb(RID p_mesh, int p_surface) const {
	const Mesh *mesh = mesh_owner.getornull(p_mesh);
	ERR_FAIL_COND_V(!mesh, Vector<AABB>());
	ERR_FAIL_INDEX_V(p_surface, mesh->surfaces.size(), Vector<AABB>());

	return mesh->surfaces[p_surface]->skeleton_bone_aabb;
}

void RasterizerStorageGLES3::mesh_remove_surface(RID p_mesh, int p_surface) {
	Mesh *mesh = mesh_owner.getornull(p_mesh);
	ERR_FAIL_COND(!mesh);
	ERR_FAIL_INDEX(p_surface, mesh->surfaces.size());

	Surface *surface = mesh->surfaces[p_surface];

	if (surface->material.is_valid()) {
		_material_remove_geometry(surface->material, mesh->surfaces[p_surface]);
	}

	glDeleteBuffers(1, &surface->vertex_id);
	if (surface->index_id) {
		glDeleteBuffers(1, &surface->index_id);
	}

	glDeleteVertexArrays(1, &surface->array_id);
	glDeleteVertexArrays(1, &surface->instancing_array_id);

	for (int i = 0; i < surface->blend_shapes.size(); i++) {
		glDeleteBuffers(1, &surface->blend_shapes[i].vertex_id);
		glDeleteVertexArrays(1, &surface->blend_shapes[i].array_id);
	}

	if (surface->index_wireframe_id) {
		glDeleteBuffers(1, &surface->index_wireframe_id);
		glDeleteVertexArrays(1, &surface->array_wireframe_id);
		glDeleteVertexArrays(1, &surface->instancing_array_wireframe_id);
	}

	info.vertex_mem -= surface->total_data_size;

	memdelete(surface);

	mesh->surfaces.remove(p_surface);

	mesh->instance_change_notify(true, true);
}

int RasterizerStorageGLES3::mesh_get_surface_count(RID p_mesh) const {
	const Mesh *mesh = mesh_owner.getornull(p_mesh);
	ERR_FAIL_COND_V(!mesh, 0);
	return mesh->surfaces.size();
}

void RasterizerStorageGLES3::mesh_set_custom_aabb(RID p_mesh, const AABB &p_aabb) {
	Mesh *mesh = mesh_owner.getornull(p_mesh);
	ERR_FAIL_COND(!mesh);

	mesh->custom_aabb = p_aabb;
	mesh->instance_change_notify(true, false);
}

AABB RasterizerStorageGLES3::mesh_get_custom_aabb(RID p_mesh) const {
	const Mesh *mesh = mesh_owner.getornull(p_mesh);
	ERR_FAIL_COND_V(!mesh, AABB());

	return mesh->custom_aabb;
}

AABB RasterizerStorageGLES3::mesh_get_aabb(RID p_mesh, RID p_skeleton) const {
	Mesh *mesh = mesh_owner.get(p_mesh);
	ERR_FAIL_COND_V(!mesh, AABB());

	if (mesh->custom_aabb != AABB()) {
		return mesh->custom_aabb;
	}

	Skeleton *sk = nullptr;
	if (p_skeleton.is_valid()) {
		sk = skeleton_owner.get(p_skeleton);
	}

	AABB aabb;

	if (sk && sk->size != 0) {
		for (int i = 0; i < mesh->surfaces.size(); i++) {
			AABB laabb;
			if ((mesh->surfaces[i]->format & VS::ARRAY_FORMAT_BONES) && mesh->surfaces[i]->skeleton_bone_aabb.size()) {
				int bs = mesh->surfaces[i]->skeleton_bone_aabb.size();
				const AABB *skbones = mesh->surfaces[i]->skeleton_bone_aabb.ptr();
				const bool *skused = mesh->surfaces[i]->skeleton_bone_used.ptr();

				int sbs = sk->size;
				ERR_CONTINUE(bs > sbs);
				const float *texture = sk->skel_texture.ptr();

				bool first = true;
				if (sk->use_2d) {
					for (int j = 0; j < bs; j++) {
						if (!skused[j]) {
							continue;
						}

						int base_ofs = ((j / 256) * 256) * 2 * 4 + (j % 256) * 4;

						Transform mtx;

						mtx.basis[0].x = texture[base_ofs + 0];
						mtx.basis[0].y = texture[base_ofs + 1];
						mtx.origin.x = texture[base_ofs + 3];
						base_ofs += 256 * 4;
						mtx.basis[1].x = texture[base_ofs + 0];
						mtx.basis[1].y = texture[base_ofs + 1];
						mtx.origin.y = texture[base_ofs + 3];

						AABB baabb = mtx.xform(skbones[j]);

						if (first) {
							laabb = baabb;
							first = false;
						} else {
							laabb.merge_with(baabb);
						}
					}
				} else {
					for (int j = 0; j < bs; j++) {
						if (!skused[j]) {
							continue;
						}

						int base_ofs = ((j / 256) * 256) * 3 * 4 + (j % 256) * 4;

						Transform mtx;

						mtx.basis[0].x = texture[base_ofs + 0];
						mtx.basis[0].y = texture[base_ofs + 1];
						mtx.basis[0].z = texture[base_ofs + 2];
						mtx.origin.x = texture[base_ofs + 3];
						base_ofs += 256 * 4;
						mtx.basis[1].x = texture[base_ofs + 0];
						mtx.basis[1].y = texture[base_ofs + 1];
						mtx.basis[1].z = texture[base_ofs + 2];
						mtx.origin.y = texture[base_ofs + 3];
						base_ofs += 256 * 4;
						mtx.basis[2].x = texture[base_ofs + 0];
						mtx.basis[2].y = texture[base_ofs + 1];
						mtx.basis[2].z = texture[base_ofs + 2];
						mtx.origin.z = texture[base_ofs + 3];

						AABB baabb = mtx.xform(skbones[j]);
						if (first) {
							laabb = baabb;
							first = false;
						} else {
							laabb.merge_with(baabb);
						}
					}
				}

			} else {
				laabb = mesh->surfaces[i]->aabb;
			}

			if (i == 0) {
				aabb = laabb;
			} else {
				aabb.merge_with(laabb);
			}
		}
	} else {
		for (int i = 0; i < mesh->surfaces.size(); i++) {
			if (i == 0) {
				aabb = mesh->surfaces[i]->aabb;
			} else {
				aabb.merge_with(mesh->surfaces[i]->aabb);
			}
		}
	}

	return aabb;
}
void RasterizerStorageGLES3::mesh_clear(RID p_mesh) {
	Mesh *mesh = mesh_owner.getornull(p_mesh);
	ERR_FAIL_COND(!mesh);

	while (mesh->surfaces.size()) {
		mesh_remove_surface(p_mesh, 0);
	}
}

void RasterizerStorageGLES3::mesh_render_blend_shapes(Surface *s, const float *p_weights) {
	glBindVertexArray(s->array_id);

	BlendShapeShaderGLES3::Conditionals cond[VS::ARRAY_MAX - 1] = {
		BlendShapeShaderGLES3::ENABLE_NORMAL, //will be ignored
		BlendShapeShaderGLES3::ENABLE_NORMAL,
		BlendShapeShaderGLES3::ENABLE_TANGENT,
		BlendShapeShaderGLES3::ENABLE_COLOR,
		BlendShapeShaderGLES3::ENABLE_UV,
		BlendShapeShaderGLES3::ENABLE_UV2,
		BlendShapeShaderGLES3::ENABLE_SKELETON,
		BlendShapeShaderGLES3::ENABLE_SKELETON,
	};

	int stride = 0;

	if (s->format & VS::ARRAY_FLAG_USE_2D_VERTICES) {
		stride = 2 * 4;
	} else {
		stride = 3 * 4;
	}

	static const int sizes[VS::ARRAY_MAX - 1] = {
		3 * 4,
		3 * 4,
		4 * 4,
		4 * 4,
		2 * 4,
		2 * 4,
		4 * 4,
		4 * 4
	};

	for (int i = 1; i < VS::ARRAY_MAX - 1; i++) {
		shaders.blend_shapes.set_conditional(cond[i], s->format & (1 << i)); //enable conditional for format
		if (s->format & (1 << i)) {
			stride += sizes[i];
		}
	}

	//copy all first
	float base_weight = 1.0;

	int mtc = s->blend_shapes.size();

	if (s->mesh->blend_shape_mode == VS::BLEND_SHAPE_MODE_NORMALIZED) {
		for (int i = 0; i < mtc; i++) {
			base_weight -= p_weights[i];
		}
	}

	shaders.blend_shapes.set_conditional(BlendShapeShaderGLES3::ENABLE_BLEND, false); //first pass does not blend
	shaders.blend_shapes.set_conditional(BlendShapeShaderGLES3::USE_2D_VERTEX, s->format & VS::ARRAY_FLAG_USE_2D_VERTICES); //use 2D vertices if needed
	shaders.blend_shapes.set_conditional(BlendShapeShaderGLES3::ENABLE_OCTAHEDRAL_COMPRESSION, s->format & VS::ARRAY_FLAG_USE_OCTAHEDRAL_COMPRESSION); //use octahedral normal compression

	shaders.blend_shapes.bind();

	shaders.blend_shapes.set_uniform(BlendShapeShaderGLES3::BLEND_AMOUNT, base_weight);
	glEnable(GL_RASTERIZER_DISCARD);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, resources.transform_feedback_buffers[0]);
	glBeginTransformFeedback(GL_POINTS);
	glDrawArrays(GL_POINTS, 0, s->array_len);
	glEndTransformFeedback();

	shaders.blend_shapes.set_conditional(BlendShapeShaderGLES3::ENABLE_BLEND, true); //first pass does not blend
	shaders.blend_shapes.bind();

	for (int ti = 0; ti < mtc; ti++) {
		float weight = p_weights[ti];

		if (Math::is_zero_approx(weight)) {
			//not bother with this one
			continue;
		}

		glBindVertexArray(s->blend_shapes[ti].array_id);
		glBindBuffer(GL_ARRAY_BUFFER, resources.transform_feedback_buffers[0]);
		glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, resources.transform_feedback_buffers[1]);

		shaders.blend_shapes.set_uniform(BlendShapeShaderGLES3::BLEND_AMOUNT, weight);

		int ofs = 0;
		for (int i = 0; i < VS::ARRAY_MAX - 1; i++) {
			if (s->format & (1 << i)) {
				glEnableVertexAttribArray(i + 8);
				switch (i) {
					case VS::ARRAY_VERTEX: {
						if (s->format & VS::ARRAY_FLAG_USE_2D_VERTICES) {
							glVertexAttribPointer(i + 8, 2, GL_FLOAT, GL_FALSE, stride, CAST_INT_TO_UCHAR_PTR(ofs));
							ofs += 2 * 4;
						} else {
							glVertexAttribPointer(i + 8, 3, GL_FLOAT, GL_FALSE, stride, CAST_INT_TO_UCHAR_PTR(ofs));
							ofs += 3 * 4;
						}
					} break;
					case VS::ARRAY_NORMAL: {
						glVertexAttribPointer(i + 8, 3, GL_FLOAT, GL_FALSE, stride, CAST_INT_TO_UCHAR_PTR(ofs));
						ofs += 3 * 4;
					} break;
					case VS::ARRAY_TANGENT: {
						glVertexAttribPointer(i + 8, 4, GL_FLOAT, GL_FALSE, stride, CAST_INT_TO_UCHAR_PTR(ofs));
						ofs += 4 * 4;

					} break;
					case VS::ARRAY_COLOR: {
						glVertexAttribPointer(i + 8, 4, GL_FLOAT, GL_FALSE, stride, CAST_INT_TO_UCHAR_PTR(ofs));
						ofs += 4 * 4;

					} break;
					case VS::ARRAY_TEX_UV: {
						glVertexAttribPointer(i + 8, 2, GL_FLOAT, GL_FALSE, stride, CAST_INT_TO_UCHAR_PTR(ofs));
						ofs += 2 * 4;

					} break;
					case VS::ARRAY_TEX_UV2: {
						glVertexAttribPointer(i + 8, 2, GL_FLOAT, GL_FALSE, stride, CAST_INT_TO_UCHAR_PTR(ofs));
						ofs += 2 * 4;

					} break;
					case VS::ARRAY_BONES: {
						glVertexAttribIPointer(i + 8, 4, GL_UNSIGNED_INT, stride, CAST_INT_TO_UCHAR_PTR(ofs));
						ofs += 4 * 4;

					} break;
					case VS::ARRAY_WEIGHTS: {
						glVertexAttribPointer(i + 8, 4, GL_FLOAT, GL_FALSE, stride, CAST_INT_TO_UCHAR_PTR(ofs));
						ofs += 4 * 4;

					} break;
				}

			} else {
				glDisableVertexAttribArray(i + 8);
			}
		}

		glBeginTransformFeedback(GL_POINTS);
		glDrawArrays(GL_POINTS, 0, s->array_len);
		glEndTransformFeedback();

		SWAP(resources.transform_feedback_buffers[0], resources.transform_feedback_buffers[1]);
	}

	glDisable(GL_RASTERIZER_DISCARD);
	glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, 0);

	glBindVertexArray(resources.transform_feedback_array);
	glBindBuffer(GL_ARRAY_BUFFER, resources.transform_feedback_buffers[0]);

	int ofs = 0;
	for (int i = 0; i < VS::ARRAY_MAX - 1; i++) {
		if (s->format & (1 << i)) {
			glEnableVertexAttribArray(i);
			switch (i) {
				case VS::ARRAY_VERTEX: {
					if (s->format & VS::ARRAY_FLAG_USE_2D_VERTICES) {
						glVertexAttribPointer(i, 2, GL_FLOAT, GL_FALSE, stride, CAST_INT_TO_UCHAR_PTR(ofs));
						ofs += 2 * 4;
					} else {
						glVertexAttribPointer(i, 3, GL_FLOAT, GL_FALSE, stride, CAST_INT_TO_UCHAR_PTR(ofs));
						ofs += 3 * 4;
					}
				} break;
				case VS::ARRAY_NORMAL: {
					glVertexAttribPointer(i, 3, GL_FLOAT, GL_FALSE, stride, CAST_INT_TO_UCHAR_PTR(ofs));
					ofs += 3 * 4;
				} break;
				case VS::ARRAY_TANGENT: {
					glVertexAttribPointer(i, 4, GL_FLOAT, GL_FALSE, stride, CAST_INT_TO_UCHAR_PTR(ofs));
					ofs += 4 * 4;

				} break;
				case VS::ARRAY_COLOR: {
					glVertexAttribPointer(i, 4, GL_FLOAT, GL_FALSE, stride, CAST_INT_TO_UCHAR_PTR(ofs));
					ofs += 4 * 4;

				} break;
				case VS::ARRAY_TEX_UV: {
					glVertexAttribPointer(i, 2, GL_FLOAT, GL_FALSE, stride, CAST_INT_TO_UCHAR_PTR(ofs));
					ofs += 2 * 4;

				} break;
				case VS::ARRAY_TEX_UV2: {
					glVertexAttribPointer(i, 2, GL_FLOAT, GL_FALSE, stride, CAST_INT_TO_UCHAR_PTR(ofs));
					ofs += 2 * 4;

				} break;
				case VS::ARRAY_BONES: {
					glVertexAttribIPointer(i, 4, GL_UNSIGNED_INT, stride, CAST_INT_TO_UCHAR_PTR(ofs));
					ofs += 4 * 4;

				} break;
				case VS::ARRAY_WEIGHTS: {
					glVertexAttribPointer(i, 4, GL_FLOAT, GL_FALSE, stride, CAST_INT_TO_UCHAR_PTR(ofs));
					ofs += 4 * 4;

				} break;
			}

		} else {
			glDisableVertexAttribArray(i);
		}
	}

	if (s->index_array_len) {
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, s->index_id);
	}
}

/* MULTIMESH API */

RID RasterizerStorageGLES3::_multimesh_create() {
	MultiMesh *multimesh = memnew(MultiMesh);
	return multimesh_owner.make_rid(multimesh);
}

void RasterizerStorageGLES3::_multimesh_allocate(RID p_multimesh, int p_instances, VS::MultimeshTransformFormat p_transform_format, VS::MultimeshColorFormat p_color_format, VS::MultimeshCustomDataFormat p_data_format) {
	MultiMesh *multimesh = multimesh_owner.getornull(p_multimesh);
	ERR_FAIL_COND(!multimesh);

	if (multimesh->size == p_instances && multimesh->transform_format == p_transform_format && multimesh->color_format == p_color_format && multimesh->custom_data_format == p_data_format) {
		return;
	}

	if (multimesh->buffer) {
		glDeleteBuffers(1, &multimesh->buffer);
		multimesh->data.resize(0);
		multimesh->buffer = 0;
	}

	multimesh->size = p_instances;
	multimesh->transform_format = p_transform_format;
	multimesh->color_format = p_color_format;
	multimesh->custom_data_format = p_data_format;

	if (multimesh->size) {
		if (multimesh->transform_format == VS::MULTIMESH_TRANSFORM_2D) {
			multimesh->xform_floats = 8;
		} else {
			multimesh->xform_floats = 12;
		}

		if (multimesh->color_format == VS::MULTIMESH_COLOR_8BIT) {
			multimesh->color_floats = 1;
		} else if (multimesh->color_format == VS::MULTIMESH_COLOR_FLOAT) {
			multimesh->color_floats = 4;
		} else {
			multimesh->color_floats = 0;
		}

		if (multimesh->custom_data_format == VS::MULTIMESH_CUSTOM_DATA_8BIT) {
			multimesh->custom_data_floats = 1;
		} else if (multimesh->custom_data_format == VS::MULTIMESH_CUSTOM_DATA_FLOAT) {
			multimesh->custom_data_floats = 4;
		} else {
			multimesh->custom_data_floats = 0;
		}

		int format_floats = multimesh->color_floats + multimesh->xform_floats + multimesh->custom_data_floats;

		multimesh->data.resize(format_floats * p_instances);

		float *dataptr = multimesh->data.ptrw();

		for (int i = 0; i < p_instances * format_floats; i += format_floats) {
			int color_from = 0;
			int custom_data_from = 0;

			if (multimesh->transform_format == VS::MULTIMESH_TRANSFORM_2D) {
				dataptr[i + 0] = 1.0;
				dataptr[i + 1] = 0.0;
				dataptr[i + 2] = 0.0;
				dataptr[i + 3] = 0.0;
				dataptr[i + 4] = 0.0;
				dataptr[i + 5] = 1.0;
				dataptr[i + 6] = 0.0;
				dataptr[i + 7] = 0.0;
				color_from = 8;
				custom_data_from = 8;
			} else {
				dataptr[i + 0] = 1.0;
				dataptr[i + 1] = 0.0;
				dataptr[i + 2] = 0.0;
				dataptr[i + 3] = 0.0;
				dataptr[i + 4] = 0.0;
				dataptr[i + 5] = 1.0;
				dataptr[i + 6] = 0.0;
				dataptr[i + 7] = 0.0;
				dataptr[i + 8] = 0.0;
				dataptr[i + 9] = 0.0;
				dataptr[i + 10] = 1.0;
				dataptr[i + 11] = 0.0;
				color_from = 12;
				custom_data_from = 12;
			}

			if (multimesh->color_format == VS::MULTIMESH_COLOR_NONE) {
				//none
			} else if (multimesh->color_format == VS::MULTIMESH_COLOR_8BIT) {
				union {
					uint32_t colu;
					float colf;
				} cu;

				cu.colu = 0xFFFFFFFF;
				dataptr[i + color_from + 0] = cu.colf;
				custom_data_from = color_from + 1;

			} else if (multimesh->color_format == VS::MULTIMESH_COLOR_FLOAT) {
				dataptr[i + color_from + 0] = 1.0;
				dataptr[i + color_from + 1] = 1.0;
				dataptr[i + color_from + 2] = 1.0;
				dataptr[i + color_from + 3] = 1.0;
				custom_data_from = color_from + 4;
			}

			if (multimesh->custom_data_format == VS::MULTIMESH_CUSTOM_DATA_NONE) {
				//none
			} else if (multimesh->custom_data_format == VS::MULTIMESH_CUSTOM_DATA_8BIT) {
				union {
					uint32_t colu;
					float colf;
				} cu;

				cu.colu = 0;
				dataptr[i + custom_data_from + 0] = cu.colf;

			} else if (multimesh->custom_data_format == VS::MULTIMESH_CUSTOM_DATA_FLOAT) {
				dataptr[i + custom_data_from + 0] = 0.0;
				dataptr[i + custom_data_from + 1] = 0.0;
				dataptr[i + custom_data_from + 2] = 0.0;
				dataptr[i + custom_data_from + 3] = 0.0;
			}
		}

		glGenBuffers(1, &multimesh->buffer);
		glBindBuffer(GL_ARRAY_BUFFER, multimesh->buffer);
		glBufferData(GL_ARRAY_BUFFER, multimesh->data.size() * sizeof(float), nullptr, GL_STATIC_DRAW);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
	}

	multimesh->dirty_data = true;
	multimesh->dirty_aabb = true;

	if (!multimesh->update_list.in_list()) {
		multimesh_update_list.add(&multimesh->update_list);
	}
}

int RasterizerStorageGLES3::_multimesh_get_instance_count(RID p_multimesh) const {
	MultiMesh *multimesh = multimesh_owner.getornull(p_multimesh);
	ERR_FAIL_COND_V(!multimesh, 0);

	return multimesh->size;
}

void RasterizerStorageGLES3::_multimesh_set_mesh(RID p_multimesh, RID p_mesh) {
	MultiMesh *multimesh = multimesh_owner.getornull(p_multimesh);
	ERR_FAIL_COND(!multimesh);

	if (multimesh->mesh.is_valid()) {
		Mesh *mesh = mesh_owner.getornull(multimesh->mesh);
		if (mesh) {
			mesh->multimeshes.remove(&multimesh->mesh_list);
		}
	}

	multimesh->mesh = p_mesh;

	if (multimesh->mesh.is_valid()) {
		Mesh *mesh = mesh_owner.getornull(multimesh->mesh);
		if (mesh) {
			mesh->multimeshes.add(&multimesh->mesh_list);
		}
	}

	multimesh->dirty_aabb = true;

	if (!multimesh->update_list.in_list()) {
		multimesh_update_list.add(&multimesh->update_list);
	}
}

void RasterizerStorageGLES3::_multimesh_instance_set_transform(RID p_multimesh, int p_index, const Transform &p_transform) {
	MultiMesh *multimesh = multimesh_owner.getornull(p_multimesh);
	ERR_FAIL_COND(!multimesh);
	ERR_FAIL_INDEX(p_index, multimesh->size);
	ERR_FAIL_COND(multimesh->transform_format == VS::MULTIMESH_TRANSFORM_2D);

	int stride = multimesh->color_floats + multimesh->xform_floats + multimesh->custom_data_floats;
	float *dataptr = &multimesh->data.write[stride * p_index];

	dataptr[0] = p_transform.basis.elements[0][0];
	dataptr[1] = p_transform.basis.elements[0][1];
	dataptr[2] = p_transform.basis.elements[0][2];
	dataptr[3] = p_transform.origin.x;
	dataptr[4] = p_transform.basis.elements[1][0];
	dataptr[5] = p_transform.basis.elements[1][1];
	dataptr[6] = p_transform.basis.elements[1][2];
	dataptr[7] = p_transform.origin.y;
	dataptr[8] = p_transform.basis.elements[2][0];
	dataptr[9] = p_transform.basis.elements[2][1];
	dataptr[10] = p_transform.basis.elements[2][2];
	dataptr[11] = p_transform.origin.z;

	multimesh->dirty_data = true;
	multimesh->dirty_aabb = true;

	if (!multimesh->update_list.in_list()) {
		multimesh_update_list.add(&multimesh->update_list);
	}
}

void RasterizerStorageGLES3::_multimesh_instance_set_transform_2d(RID p_multimesh, int p_index, const Transform2D &p_transform) {
	MultiMesh *multimesh = multimesh_owner.getornull(p_multimesh);
	ERR_FAIL_COND(!multimesh);
	ERR_FAIL_INDEX(p_index, multimesh->size);
	ERR_FAIL_COND(multimesh->transform_format == VS::MULTIMESH_TRANSFORM_3D);

	int stride = multimesh->color_floats + multimesh->xform_floats + multimesh->custom_data_floats;
	float *dataptr = &multimesh->data.write[stride * p_index];

	dataptr[0] = p_transform.elements[0][0];
	dataptr[1] = p_transform.elements[1][0];
	dataptr[2] = 0;
	dataptr[3] = p_transform.elements[2][0];
	dataptr[4] = p_transform.elements[0][1];
	dataptr[5] = p_transform.elements[1][1];
	dataptr[6] = 0;
	dataptr[7] = p_transform.elements[2][1];

	multimesh->dirty_data = true;
	multimesh->dirty_aabb = true;

	if (!multimesh->update_list.in_list()) {
		multimesh_update_list.add(&multimesh->update_list);
	}
}
void RasterizerStorageGLES3::_multimesh_instance_set_color(RID p_multimesh, int p_index, const Color &p_color) {
	MultiMesh *multimesh = multimesh_owner.getornull(p_multimesh);
	ERR_FAIL_COND(!multimesh);
	ERR_FAIL_INDEX(p_index, multimesh->size);
	ERR_FAIL_COND(multimesh->color_format == VS::MULTIMESH_COLOR_NONE);
	ERR_FAIL_INDEX(multimesh->color_format, VS::MULTIMESH_COLOR_MAX);

	int stride = multimesh->color_floats + multimesh->xform_floats + multimesh->custom_data_floats;
	float *dataptr = &multimesh->data.write[stride * p_index + multimesh->xform_floats];

	if (multimesh->color_format == VS::MULTIMESH_COLOR_8BIT) {
		uint8_t *data8 = (uint8_t *)dataptr;
		data8[0] = CLAMP(p_color.r * 255.0, 0, 255);
		data8[1] = CLAMP(p_color.g * 255.0, 0, 255);
		data8[2] = CLAMP(p_color.b * 255.0, 0, 255);
		data8[3] = CLAMP(p_color.a * 255.0, 0, 255);

	} else if (multimesh->color_format == VS::MULTIMESH_COLOR_FLOAT) {
		dataptr[0] = p_color.r;
		dataptr[1] = p_color.g;
		dataptr[2] = p_color.b;
		dataptr[3] = p_color.a;
	}

	multimesh->dirty_data = true;
	multimesh->dirty_aabb = true;

	if (!multimesh->update_list.in_list()) {
		multimesh_update_list.add(&multimesh->update_list);
	}
}

void RasterizerStorageGLES3::_multimesh_instance_set_custom_data(RID p_multimesh, int p_index, const Color &p_custom_data) {
	MultiMesh *multimesh = multimesh_owner.getornull(p_multimesh);
	ERR_FAIL_COND(!multimesh);
	ERR_FAIL_INDEX(p_index, multimesh->size);
	ERR_FAIL_COND(multimesh->custom_data_format == VS::MULTIMESH_CUSTOM_DATA_NONE);
	ERR_FAIL_INDEX(multimesh->custom_data_format, VS::MULTIMESH_CUSTOM_DATA_MAX);

	int stride = multimesh->color_floats + multimesh->xform_floats + multimesh->custom_data_floats;
	float *dataptr = &multimesh->data.write[stride * p_index + multimesh->xform_floats + multimesh->color_floats];

	if (multimesh->custom_data_format == VS::MULTIMESH_CUSTOM_DATA_8BIT) {
		uint8_t *data8 = (uint8_t *)dataptr;
		data8[0] = CLAMP(p_custom_data.r * 255.0, 0, 255);
		data8[1] = CLAMP(p_custom_data.g * 255.0, 0, 255);
		data8[2] = CLAMP(p_custom_data.b * 255.0, 0, 255);
		data8[3] = CLAMP(p_custom_data.a * 255.0, 0, 255);

	} else if (multimesh->custom_data_format == VS::MULTIMESH_CUSTOM_DATA_FLOAT) {
		dataptr[0] = p_custom_data.r;
		dataptr[1] = p_custom_data.g;
		dataptr[2] = p_custom_data.b;
		dataptr[3] = p_custom_data.a;
	}

	multimesh->dirty_data = true;
	multimesh->dirty_aabb = true;

	if (!multimesh->update_list.in_list()) {
		multimesh_update_list.add(&multimesh->update_list);
	}
}
RID RasterizerStorageGLES3::_multimesh_get_mesh(RID p_multimesh) const {
	MultiMesh *multimesh = multimesh_owner.getornull(p_multimesh);
	ERR_FAIL_COND_V(!multimesh, RID());

	return multimesh->mesh;
}

Transform RasterizerStorageGLES3::_multimesh_instance_get_transform(RID p_multimesh, int p_index) const {
	MultiMesh *multimesh = multimesh_owner.getornull(p_multimesh);
	ERR_FAIL_COND_V(!multimesh, Transform());
	ERR_FAIL_INDEX_V(p_index, multimesh->size, Transform());
	ERR_FAIL_COND_V(multimesh->transform_format == VS::MULTIMESH_TRANSFORM_2D, Transform());

	int stride = multimesh->color_floats + multimesh->xform_floats + multimesh->custom_data_floats;
	float *dataptr = &multimesh->data.write[stride * p_index];

	Transform xform;

	xform.basis.elements[0][0] = dataptr[0];
	xform.basis.elements[0][1] = dataptr[1];
	xform.basis.elements[0][2] = dataptr[2];
	xform.origin.x = dataptr[3];
	xform.basis.elements[1][0] = dataptr[4];
	xform.basis.elements[1][1] = dataptr[5];
	xform.basis.elements[1][2] = dataptr[6];
	xform.origin.y = dataptr[7];
	xform.basis.elements[2][0] = dataptr[8];
	xform.basis.elements[2][1] = dataptr[9];
	xform.basis.elements[2][2] = dataptr[10];
	xform.origin.z = dataptr[11];

	return xform;
}
Transform2D RasterizerStorageGLES3::_multimesh_instance_get_transform_2d(RID p_multimesh, int p_index) const {
	MultiMesh *multimesh = multimesh_owner.getornull(p_multimesh);
	ERR_FAIL_COND_V(!multimesh, Transform2D());
	ERR_FAIL_INDEX_V(p_index, multimesh->size, Transform2D());
	ERR_FAIL_COND_V(multimesh->transform_format == VS::MULTIMESH_TRANSFORM_3D, Transform2D());

	int stride = multimesh->color_floats + multimesh->xform_floats + multimesh->custom_data_floats;
	float *dataptr = &multimesh->data.write[stride * p_index];

	Transform2D xform;

	xform.elements[0][0] = dataptr[0];
	xform.elements[1][0] = dataptr[1];
	xform.elements[2][0] = dataptr[3];
	xform.elements[0][1] = dataptr[4];
	xform.elements[1][1] = dataptr[5];
	xform.elements[2][1] = dataptr[7];

	return xform;
}

Color RasterizerStorageGLES3::_multimesh_instance_get_color(RID p_multimesh, int p_index) const {
	MultiMesh *multimesh = multimesh_owner.getornull(p_multimesh);
	ERR_FAIL_COND_V(!multimesh, Color());
	ERR_FAIL_INDEX_V(p_index, multimesh->size, Color());
	ERR_FAIL_COND_V(multimesh->color_format == VS::MULTIMESH_COLOR_NONE, Color());
	ERR_FAIL_INDEX_V(multimesh->color_format, VS::MULTIMESH_COLOR_MAX, Color());

	int stride = multimesh->color_floats + multimesh->xform_floats + multimesh->custom_data_floats;
	float *dataptr = &multimesh->data.write[stride * p_index + multimesh->xform_floats];

	if (multimesh->color_format == VS::MULTIMESH_COLOR_8BIT) {
		union {
			uint32_t colu;
			float colf;
		} cu;

		cu.colf = dataptr[0];

		return Color::hex(BSWAP32(cu.colu));

	} else if (multimesh->color_format == VS::MULTIMESH_COLOR_FLOAT) {
		Color c;
		c.r = dataptr[0];
		c.g = dataptr[1];
		c.b = dataptr[2];
		c.a = dataptr[3];

		return c;
	}

	return Color();
}

Color RasterizerStorageGLES3::_multimesh_instance_get_custom_data(RID p_multimesh, int p_index) const {
	MultiMesh *multimesh = multimesh_owner.getornull(p_multimesh);
	ERR_FAIL_COND_V(!multimesh, Color());
	ERR_FAIL_INDEX_V(p_index, multimesh->size, Color());
	ERR_FAIL_COND_V(multimesh->custom_data_format == VS::MULTIMESH_CUSTOM_DATA_NONE, Color());
	ERR_FAIL_INDEX_V(multimesh->custom_data_format, VS::MULTIMESH_CUSTOM_DATA_MAX, Color());

	int stride = multimesh->color_floats + multimesh->xform_floats + multimesh->custom_data_floats;
	float *dataptr = &multimesh->data.write[stride * p_index + multimesh->xform_floats + multimesh->color_floats];

	if (multimesh->custom_data_format == VS::MULTIMESH_CUSTOM_DATA_8BIT) {
		union {
			uint32_t colu;
			float colf;
		} cu;

		cu.colf = dataptr[0];

		return Color::hex(BSWAP32(cu.colu));

	} else if (multimesh->custom_data_format == VS::MULTIMESH_CUSTOM_DATA_FLOAT) {
		Color c;
		c.r = dataptr[0];
		c.g = dataptr[1];
		c.b = dataptr[2];
		c.a = dataptr[3];

		return c;
	}

	return Color();
}

void RasterizerStorageGLES3::_multimesh_set_as_bulk_array(RID p_multimesh, const PoolVector<float> &p_array) {
	MultiMesh *multimesh = multimesh_owner.getornull(p_multimesh);
	ERR_FAIL_COND(!multimesh);
	ERR_FAIL_COND(!multimesh->data.ptr());

	int dsize = multimesh->data.size();

	ERR_FAIL_COND(dsize != p_array.size());

	PoolVector<float>::Read r = p_array.read();
	memcpy(multimesh->data.ptrw(), r.ptr(), dsize * sizeof(float));

	multimesh->dirty_data = true;
	multimesh->dirty_aabb = true;

	if (!multimesh->update_list.in_list()) {
		multimesh_update_list.add(&multimesh->update_list);
	}
}

void RasterizerStorageGLES3::_multimesh_set_visible_instances(RID p_multimesh, int p_visible) {
	MultiMesh *multimesh = multimesh_owner.getornull(p_multimesh);
	ERR_FAIL_COND(!multimesh);

	multimesh->visible_instances = p_visible;
}
int RasterizerStorageGLES3::_multimesh_get_visible_instances(RID p_multimesh) const {
	MultiMesh *multimesh = multimesh_owner.getornull(p_multimesh);
	ERR_FAIL_COND_V(!multimesh, -1);

	return multimesh->visible_instances;
}

AABB RasterizerStorageGLES3::_multimesh_get_aabb(RID p_multimesh) const {
	MultiMesh *multimesh = multimesh_owner.getornull(p_multimesh);
	ERR_FAIL_COND_V(!multimesh, AABB());

	const_cast<RasterizerStorageGLES3 *>(this)->update_dirty_multimeshes(); //update pending AABBs

	return multimesh->aabb;
}

RasterizerStorage::MMInterpolator *RasterizerStorageGLES3::_multimesh_get_interpolator(RID p_multimesh) const {
	MultiMesh *multimesh = multimesh_owner.getornull(p_multimesh);
	ERR_FAIL_COND_V_MSG(!multimesh, nullptr, "Multimesh not found: " + itos(p_multimesh.get_id()));

	return &multimesh->interpolator;
}

void RasterizerStorageGLES3::multimesh_attach_canvas_item(RID p_multimesh, RID p_canvas_item, bool p_attach) {
	MultiMesh *multimesh = multimesh_owner.getornull(p_multimesh);
	ERR_FAIL_NULL(multimesh);
	ERR_FAIL_COND(!p_canvas_item.is_valid());

	if (p_attach) {
		int64_t found = multimesh->linked_canvas_items.find(p_canvas_item);
		if (found == -1) {
			multimesh->linked_canvas_items.push_back(p_canvas_item);
		}
	} else {
		int64_t found = multimesh->linked_canvas_items.find(p_canvas_item);
		if (found != -1) {
			multimesh->linked_canvas_items.remove_unordered(found);
		}
	}
}

void RasterizerStorageGLES3::update_dirty_multimeshes() {
	while (multimesh_update_list.first()) {
		MultiMesh *multimesh = multimesh_update_list.first()->self();

		if (multimesh->size && multimesh->dirty_data) {
			glBindBuffer(GL_ARRAY_BUFFER, multimesh->buffer);
			uint32_t buffer_size = multimesh->data.size() * sizeof(float);

			// this could potentially have a project setting for API options as with 2d
			// if (config.should_orphan) {
			glBufferData(GL_ARRAY_BUFFER, buffer_size, multimesh->data.ptr(), GL_DYNAMIC_DRAW);
			//	} else {
			//	glBufferSubData(GL_ARRAY_BUFFER, 0, buffer_size, multimesh->data.ptr());
			//	}
			glBindBuffer(GL_ARRAY_BUFFER, 0);
		}

		if (multimesh->size && multimesh->dirty_aabb) {
			AABB mesh_aabb;

			if (multimesh->mesh.is_valid()) {
				mesh_aabb = mesh_get_aabb(multimesh->mesh, RID());
			} else {
				mesh_aabb.size += Vector3(0.001, 0.001, 0.001);
			}

			int stride = multimesh->color_floats + multimesh->xform_floats + multimesh->custom_data_floats;
			int count = multimesh->data.size();
			float *data = multimesh->data.ptrw();

			AABB aabb;

			if (multimesh->transform_format == VS::MULTIMESH_TRANSFORM_2D) {
				for (int i = 0; i < count; i += stride) {
					float *dataptr = &data[i];
					Transform xform;
					xform.basis[0][0] = dataptr[0];
					xform.basis[0][1] = dataptr[1];
					xform.origin[0] = dataptr[3];
					xform.basis[1][0] = dataptr[4];
					xform.basis[1][1] = dataptr[5];
					xform.origin[1] = dataptr[7];

					AABB laabb = xform.xform(mesh_aabb);
					if (i == 0) {
						aabb = laabb;
					} else {
						aabb.merge_with(laabb);
					}
				}
			} else {
				for (int i = 0; i < count; i += stride) {
					float *dataptr = &data[i];
					Transform xform;

					xform.basis.elements[0][0] = dataptr[0];
					xform.basis.elements[0][1] = dataptr[1];
					xform.basis.elements[0][2] = dataptr[2];
					xform.origin.x = dataptr[3];
					xform.basis.elements[1][0] = dataptr[4];
					xform.basis.elements[1][1] = dataptr[5];
					xform.basis.elements[1][2] = dataptr[6];
					xform.origin.y = dataptr[7];
					xform.basis.elements[2][0] = dataptr[8];
					xform.basis.elements[2][1] = dataptr[9];
					xform.basis.elements[2][2] = dataptr[10];
					xform.origin.z = dataptr[11];

					AABB laabb = xform.xform(mesh_aabb);
					if (i == 0) {
						aabb = laabb;
					} else {
						aabb.merge_with(laabb);
					}
				}
			}

			multimesh->aabb = aabb;

			// Inform any linked canvas items that bounds have changed
			// (for hierarchical culling).
			int num_linked = multimesh->linked_canvas_items.size();
			for (int n = 0; n < num_linked; n++) {
				const RID &rid = multimesh->linked_canvas_items[n];
				VSG::canvas->_canvas_item_invalidate_local_bound(rid);
			}
		}
		multimesh->dirty_aabb = false;
		multimesh->dirty_data = false;

		multimesh->instance_change_notify(true, false);

		multimesh_update_list.remove(multimesh_update_list.first());
	}
}

/* IMMEDIATE API */

RID RasterizerStorageGLES3::immediate_create() {
	Immediate *im = memnew(Immediate);
	return immediate_owner.make_rid(im);
}

void RasterizerStorageGLES3::immediate_begin(RID p_immediate, VS::PrimitiveType p_primitive, RID p_texture) {
	ERR_FAIL_INDEX(p_primitive, (int)VS::PRIMITIVE_MAX);
	Immediate *im = immediate_owner.get(p_immediate);
	ERR_FAIL_COND(!im);
	ERR_FAIL_COND(im->building);

	Immediate::Chunk ic;
	ic.texture = p_texture;
	ic.primitive = p_primitive;
	im->chunks.push_back(ic);
	im->mask = 0;
	im->building = true;
}
void RasterizerStorageGLES3::immediate_vertex(RID p_immediate, const Vector3 &p_vertex) {
	Immediate *im = immediate_owner.get(p_immediate);
	ERR_FAIL_COND(!im);
	ERR_FAIL_COND(!im->building);

	Immediate::Chunk *c = &im->chunks.back()->get();

	if (c->vertices.empty() && im->chunks.size() == 1) {
		im->aabb.position = p_vertex;
		im->aabb.size = Vector3();
	} else {
		im->aabb.expand_to(p_vertex);
	}

	if (im->mask & VS::ARRAY_FORMAT_NORMAL) {
		c->normals.push_back(chunk_normal);
	}
	if (im->mask & VS::ARRAY_FORMAT_TANGENT) {
		c->tangents.push_back(chunk_tangent);
	}
	if (im->mask & VS::ARRAY_FORMAT_COLOR) {
		c->colors.push_back(chunk_color);
	}
	if (im->mask & VS::ARRAY_FORMAT_TEX_UV) {
		c->uvs.push_back(chunk_uv);
	}
	if (im->mask & VS::ARRAY_FORMAT_TEX_UV2) {
		c->uvs2.push_back(chunk_uv2);
	}
	im->mask |= VS::ARRAY_FORMAT_VERTEX;
	c->vertices.push_back(p_vertex);
}

void RasterizerStorageGLES3::immediate_normal(RID p_immediate, const Vector3 &p_normal) {
	Immediate *im = immediate_owner.get(p_immediate);
	ERR_FAIL_COND(!im);
	ERR_FAIL_COND(!im->building);

	im->mask |= VS::ARRAY_FORMAT_NORMAL;
	chunk_normal = p_normal;
}
void RasterizerStorageGLES3::immediate_tangent(RID p_immediate, const Plane &p_tangent) {
	Immediate *im = immediate_owner.get(p_immediate);
	ERR_FAIL_COND(!im);
	ERR_FAIL_COND(!im->building);

	im->mask |= VS::ARRAY_FORMAT_TANGENT;
	chunk_tangent = p_tangent;
}
void RasterizerStorageGLES3::immediate_color(RID p_immediate, const Color &p_color) {
	Immediate *im = immediate_owner.get(p_immediate);
	ERR_FAIL_COND(!im);
	ERR_FAIL_COND(!im->building);

	im->mask |= VS::ARRAY_FORMAT_COLOR;
	chunk_color = p_color;
}
void RasterizerStorageGLES3::immediate_uv(RID p_immediate, const Vector2 &tex_uv) {
	Immediate *im = immediate_owner.get(p_immediate);
	ERR_FAIL_COND(!im);
	ERR_FAIL_COND(!im->building);

	im->mask |= VS::ARRAY_FORMAT_TEX_UV;
	chunk_uv = tex_uv;
}
void RasterizerStorageGLES3::immediate_uv2(RID p_immediate, const Vector2 &tex_uv) {
	Immediate *im = immediate_owner.get(p_immediate);
	ERR_FAIL_COND(!im);
	ERR_FAIL_COND(!im->building);

	im->mask |= VS::ARRAY_FORMAT_TEX_UV2;
	chunk_uv2 = tex_uv;
}

void RasterizerStorageGLES3::immediate_end(RID p_immediate) {
	Immediate *im = immediate_owner.get(p_immediate);
	ERR_FAIL_COND(!im);
	ERR_FAIL_COND(!im->building);

	im->building = false;

	im->instance_change_notify(true, false);
}
void RasterizerStorageGLES3::immediate_clear(RID p_immediate) {
	Immediate *im = immediate_owner.get(p_immediate);
	ERR_FAIL_COND(!im);
	ERR_FAIL_COND(im->building);

	im->chunks.clear();
	im->instance_change_notify(true, false);
}

AABB RasterizerStorageGLES3::immediate_get_aabb(RID p_immediate) const {
	Immediate *im = immediate_owner.get(p_immediate);
	ERR_FAIL_COND_V(!im, AABB());
	return im->aabb;
}

void RasterizerStorageGLES3::immediate_set_material(RID p_immediate, RID p_material) {
	Immediate *im = immediate_owner.get(p_immediate);
	ERR_FAIL_COND(!im);
	im->material = p_material;
	im->instance_change_notify(false, true);
}

RID RasterizerStorageGLES3::immediate_get_material(RID p_immediate) const {
	const Immediate *im = immediate_owner.get(p_immediate);
	ERR_FAIL_COND_V(!im, RID());
	return im->material;
}

/* SKELETON API */

RID RasterizerStorageGLES3::skeleton_create() {
	Skeleton *skeleton = memnew(Skeleton);

	glGenTextures(1, &skeleton->texture);

	return skeleton_owner.make_rid(skeleton);
}

void RasterizerStorageGLES3::skeleton_allocate(RID p_skeleton, int p_bones, bool p_2d_skeleton) {
	Skeleton *skeleton = skeleton_owner.getornull(p_skeleton);
	ERR_FAIL_COND(!skeleton);
	ERR_FAIL_COND(p_bones < 0);

	if (skeleton->size == p_bones && skeleton->use_2d == p_2d_skeleton) {
		return;
	}

	skeleton->size = p_bones;
	skeleton->use_2d = p_2d_skeleton;

	int height = p_bones / 256;
	if (p_bones % 256) {
		height++;
	}

	gl_wrapper.gl_active_texture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, skeleton->texture);

	if (skeleton->use_2d) {
		skeleton->skel_texture.resize(256 * height * 2 * 4);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, 256, height * 2, 0, GL_RGBA, GL_FLOAT, nullptr);
	} else {
		skeleton->skel_texture.resize(256 * height * 3 * 4);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, 256, height * 3, 0, GL_RGBA, GL_FLOAT, nullptr);
	}

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	if (!skeleton->update_list.in_list()) {
		skeleton_update_list.add(&skeleton->update_list);
	}
}
int RasterizerStorageGLES3::skeleton_get_bone_count(RID p_skeleton) const {
	Skeleton *skeleton = skeleton_owner.getornull(p_skeleton);
	ERR_FAIL_COND_V(!skeleton, 0);

	return skeleton->size;
}

void RasterizerStorageGLES3::skeleton_bone_set_transform(RID p_skeleton, int p_bone, const Transform &p_transform) {
	Skeleton *skeleton = skeleton_owner.getornull(p_skeleton);

	ERR_FAIL_COND(!skeleton);
	ERR_FAIL_INDEX(p_bone, skeleton->size);
	ERR_FAIL_COND(skeleton->use_2d);

	float *texture = skeleton->skel_texture.ptrw();

	int base_ofs = ((p_bone / 256) * 256) * 3 * 4 + (p_bone % 256) * 4;

	texture[base_ofs + 0] = p_transform.basis[0].x;
	texture[base_ofs + 1] = p_transform.basis[0].y;
	texture[base_ofs + 2] = p_transform.basis[0].z;
	texture[base_ofs + 3] = p_transform.origin.x;
	base_ofs += 256 * 4;
	texture[base_ofs + 0] = p_transform.basis[1].x;
	texture[base_ofs + 1] = p_transform.basis[1].y;
	texture[base_ofs + 2] = p_transform.basis[1].z;
	texture[base_ofs + 3] = p_transform.origin.y;
	base_ofs += 256 * 4;
	texture[base_ofs + 0] = p_transform.basis[2].x;
	texture[base_ofs + 1] = p_transform.basis[2].y;
	texture[base_ofs + 2] = p_transform.basis[2].z;
	texture[base_ofs + 3] = p_transform.origin.z;

	if (!skeleton->update_list.in_list()) {
		skeleton_update_list.add(&skeleton->update_list);
	}
}

Transform RasterizerStorageGLES3::skeleton_bone_get_transform(RID p_skeleton, int p_bone) const {
	Skeleton *skeleton = skeleton_owner.getornull(p_skeleton);

	ERR_FAIL_COND_V(!skeleton, Transform());
	ERR_FAIL_INDEX_V(p_bone, skeleton->size, Transform());
	ERR_FAIL_COND_V(skeleton->use_2d, Transform());

	const float *texture = skeleton->skel_texture.ptr();

	Transform ret;

	int base_ofs = ((p_bone / 256) * 256) * 3 * 4 + (p_bone % 256) * 4;

	ret.basis[0].x = texture[base_ofs + 0];
	ret.basis[0].y = texture[base_ofs + 1];
	ret.basis[0].z = texture[base_ofs + 2];
	ret.origin.x = texture[base_ofs + 3];
	base_ofs += 256 * 4;
	ret.basis[1].x = texture[base_ofs + 0];
	ret.basis[1].y = texture[base_ofs + 1];
	ret.basis[1].z = texture[base_ofs + 2];
	ret.origin.y = texture[base_ofs + 3];
	base_ofs += 256 * 4;
	ret.basis[2].x = texture[base_ofs + 0];
	ret.basis[2].y = texture[base_ofs + 1];
	ret.basis[2].z = texture[base_ofs + 2];
	ret.origin.z = texture[base_ofs + 3];

	return ret;
}
void RasterizerStorageGLES3::skeleton_bone_set_transform_2d(RID p_skeleton, int p_bone, const Transform2D &p_transform) {
	Skeleton *skeleton = skeleton_owner.getornull(p_skeleton);

	ERR_FAIL_COND(!skeleton);
	ERR_FAIL_INDEX(p_bone, skeleton->size);
	ERR_FAIL_COND(!skeleton->use_2d);

	float *texture = skeleton->skel_texture.ptrw();

	int base_ofs = ((p_bone / 256) * 256) * 2 * 4 + (p_bone % 256) * 4;

	texture[base_ofs + 0] = p_transform[0][0];
	texture[base_ofs + 1] = p_transform[1][0];
	texture[base_ofs + 2] = 0;
	texture[base_ofs + 3] = p_transform[2][0];
	base_ofs += 256 * 4;
	texture[base_ofs + 0] = p_transform[0][1];
	texture[base_ofs + 1] = p_transform[1][1];
	texture[base_ofs + 2] = 0;
	texture[base_ofs + 3] = p_transform[2][1];

	if (!skeleton->update_list.in_list()) {
		skeleton_update_list.add(&skeleton->update_list);
	}

	skeleton->revision++;
}
Transform2D RasterizerStorageGLES3::skeleton_bone_get_transform_2d(RID p_skeleton, int p_bone) const {
	Skeleton *skeleton = skeleton_owner.getornull(p_skeleton);

	ERR_FAIL_COND_V(!skeleton, Transform2D());
	ERR_FAIL_INDEX_V(p_bone, skeleton->size, Transform2D());
	ERR_FAIL_COND_V(!skeleton->use_2d, Transform2D());

	const float *texture = skeleton->skel_texture.ptr();

	Transform2D ret;

	int base_ofs = ((p_bone / 256) * 256) * 2 * 4 + (p_bone % 256) * 4;

	ret[0][0] = texture[base_ofs + 0];
	ret[1][0] = texture[base_ofs + 1];
	ret[2][0] = texture[base_ofs + 3];
	base_ofs += 256 * 4;
	ret[0][1] = texture[base_ofs + 0];
	ret[1][1] = texture[base_ofs + 1];
	ret[2][1] = texture[base_ofs + 3];

	return ret;
}

void RasterizerStorageGLES3::skeleton_set_base_transform_2d(RID p_skeleton, const Transform2D &p_base_transform) {
	Skeleton *skeleton = skeleton_owner.getornull(p_skeleton);

	ERR_FAIL_COND(!skeleton->use_2d);

	skeleton->base_transform_2d = p_base_transform;
}

void RasterizerStorageGLES3::skeleton_attach_canvas_item(RID p_skeleton, RID p_canvas_item, bool p_attach) {
	Skeleton *skeleton = skeleton_owner.getornull(p_skeleton);
	ERR_FAIL_NULL(skeleton);
	ERR_FAIL_COND(!p_canvas_item.is_valid());

	if (p_attach) {
#ifdef DEV_ENABLED
		// skeleton_attach_canvas_item() is not bound,
		// and checks in canvas_item_attach_skeleton() should prevent this,
		// but there isn't much harm in a DEV_ENABLED check here.
		int64_t found = skeleton->linked_canvas_items.find(p_canvas_item);
		ERR_FAIL_COND(found != -1);
#endif
		skeleton->linked_canvas_items.push_back(p_canvas_item);
	} else {
		int64_t found = skeleton->linked_canvas_items.find(p_canvas_item);
		ERR_FAIL_COND(found == -1);
		skeleton->linked_canvas_items.remove_unordered(found);
	}
}

uint32_t RasterizerStorageGLES3::skeleton_get_revision(RID p_skeleton) const {
	const Skeleton *skeleton = skeleton_owner.getornull(p_skeleton);
	ERR_FAIL_COND_V(!skeleton, 0);
	return skeleton->revision;
}

void RasterizerStorageGLES3::update_dirty_skeletons() {
	// 2D Skeletons always need to update the polygons so they
	// know the bounds have changed.
	// TODO : Could we have a separate list for 2D only?
	SelfList<Skeleton> *ele = skeleton_update_list.first();

	while (ele) {
		Skeleton *skeleton = ele->self();

		int num_linked = skeleton->linked_canvas_items.size();
		for (int n = 0; n < num_linked; n++) {
			const RID &rid = skeleton->linked_canvas_items[n];
			VSG::canvas->_canvas_item_invalidate_local_bound(rid);
		}

		ele = ele->next();
	}

	// TODO : Is this update necessary for 2D software skinning?
	gl_wrapper.gl_active_texture(GL_TEXTURE0);

	while (skeleton_update_list.first()) {
		Skeleton *skeleton = skeleton_update_list.first()->self();
		if (skeleton->size) {
			int height = skeleton->size / 256;
			if (skeleton->size % 256) {
				height++;
			}

			glBindTexture(GL_TEXTURE_2D, skeleton->texture);
			glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 256, height * (skeleton->use_2d ? 2 : 3), GL_RGBA, GL_FLOAT, skeleton->skel_texture.ptr());
		}

		for (Set<RasterizerScene::InstanceBase *>::Element *E = skeleton->instances.front(); E; E = E->next()) {
			E->get()->base_changed(true, false);
		}

		skeleton_update_list.remove(skeleton_update_list.first());
	}
}

/* Light API */

RID RasterizerStorageGLES3::light_create(VS::LightType p_type) {
	Light *light = memnew(Light);
	light->type = p_type;

	light->param[VS::LIGHT_PARAM_ENERGY] = 1.0;
	light->param[VS::LIGHT_PARAM_INDIRECT_ENERGY] = 1.0;
	light->param[VS::LIGHT_PARAM_SIZE] = 0.0;
	light->param[VS::LIGHT_PARAM_SPECULAR] = 0.5;
	light->param[VS::LIGHT_PARAM_RANGE] = 1.0;
	light->param[VS::LIGHT_PARAM_SPOT_ANGLE] = 45;
	light->param[VS::LIGHT_PARAM_CONTACT_SHADOW_SIZE] = 45;
	light->param[VS::LIGHT_PARAM_SHADOW_MAX_DISTANCE] = 0;
	light->param[VS::LIGHT_PARAM_SHADOW_SPLIT_1_OFFSET] = 0.1;
	light->param[VS::LIGHT_PARAM_SHADOW_SPLIT_2_OFFSET] = 0.3;
	light->param[VS::LIGHT_PARAM_SHADOW_SPLIT_3_OFFSET] = 0.6;
	light->param[VS::LIGHT_PARAM_SHADOW_NORMAL_BIAS] = 0.1;
	light->param[VS::LIGHT_PARAM_SHADOW_BIAS_SPLIT_SCALE] = 0.1;
	light->param[VS::LIGHT_PARAM_SHADOW_FADE_START] = 0.8;

	light->color = Color(1, 1, 1, 1);
	light->shadow = false;
	light->negative = false;
	light->cull_mask = 0xFFFFFFFF;
	light->directional_shadow_mode = VS::LIGHT_DIRECTIONAL_SHADOW_ORTHOGONAL;
	light->omni_shadow_mode = VS::LIGHT_OMNI_SHADOW_DUAL_PARABOLOID;
	light->omni_shadow_detail = VS::LIGHT_OMNI_SHADOW_DETAIL_VERTICAL;
	light->directional_blend_splits = false;
	light->directional_range_mode = VS::LIGHT_DIRECTIONAL_SHADOW_DEPTH_RANGE_STABLE;
	light->reverse_cull = false;
	light->bake_mode = VS::LIGHT_BAKE_INDIRECT;
	light->version = 0;

	return light_owner.make_rid(light);
}

void RasterizerStorageGLES3::light_set_color(RID p_light, const Color &p_color) {
	Light *light = light_owner.getornull(p_light);
	ERR_FAIL_COND(!light);

	light->color = p_color;
}
void RasterizerStorageGLES3::light_set_param(RID p_light, VS::LightParam p_param, float p_value) {
	Light *light = light_owner.getornull(p_light);
	ERR_FAIL_COND(!light);
	ERR_FAIL_INDEX(p_param, VS::LIGHT_PARAM_MAX);

	switch (p_param) {
		case VS::LIGHT_PARAM_RANGE:
		case VS::LIGHT_PARAM_SPOT_ANGLE:
		case VS::LIGHT_PARAM_SHADOW_MAX_DISTANCE:
		case VS::LIGHT_PARAM_SHADOW_SPLIT_1_OFFSET:
		case VS::LIGHT_PARAM_SHADOW_SPLIT_2_OFFSET:
		case VS::LIGHT_PARAM_SHADOW_SPLIT_3_OFFSET:
		case VS::LIGHT_PARAM_SHADOW_NORMAL_BIAS:
		case VS::LIGHT_PARAM_SHADOW_BIAS:
		case VS::LIGHT_PARAM_SHADOW_FADE_START: {
			light->version++;
			light->instance_change_notify(true, false);
		} break;
		default: {
		}
	}

	light->param[p_param] = p_value;
}
void RasterizerStorageGLES3::light_set_shadow(RID p_light, bool p_enabled) {
	Light *light = light_owner.getornull(p_light);
	ERR_FAIL_COND(!light);
	light->shadow = p_enabled;

	light->version++;
	light->instance_change_notify(true, false);
}

void RasterizerStorageGLES3::light_set_shadow_color(RID p_light, const Color &p_color) {
	Light *light = light_owner.getornull(p_light);
	ERR_FAIL_COND(!light);
	light->shadow_color = p_color;
}

void RasterizerStorageGLES3::light_set_projector(RID p_light, RID p_texture) {
	Light *light = light_owner.getornull(p_light);
	ERR_FAIL_COND(!light);

	light->projector = p_texture;
}

void RasterizerStorageGLES3::light_set_negative(RID p_light, bool p_enable) {
	Light *light = light_owner.getornull(p_light);
	ERR_FAIL_COND(!light);

	light->negative = p_enable;
}
void RasterizerStorageGLES3::light_set_cull_mask(RID p_light, uint32_t p_mask) {
	Light *light = light_owner.getornull(p_light);
	ERR_FAIL_COND(!light);

	light->cull_mask = p_mask;

	light->version++;
	light->instance_change_notify(true, false);
}

void RasterizerStorageGLES3::light_set_reverse_cull_face_mode(RID p_light, bool p_enabled) {
	Light *light = light_owner.getornull(p_light);
	ERR_FAIL_COND(!light);

	light->reverse_cull = p_enabled;

	light->version++;
	light->instance_change_notify(true, false);
}

void RasterizerStorageGLES3::light_set_use_gi(RID p_light, bool p_enabled) {
	WARN_DEPRECATED_MSG("'VisualServer.light_set_use_gi' is deprecated and will be removed in a future version. Use 'VisualServer.light_set_bake_mode' instead.");
	light_set_bake_mode(p_light, p_enabled ? VS::LightBakeMode::LIGHT_BAKE_INDIRECT : VS::LightBakeMode::LIGHT_BAKE_DISABLED);
}

void RasterizerStorageGLES3::light_set_bake_mode(RID p_light, VS::LightBakeMode p_bake_mode) {
	Light *light = light_owner.getornull(p_light);
	ERR_FAIL_COND(!light);

	light->bake_mode = p_bake_mode;

	light->version++;
	light->instance_change_notify(true, false);
}

void RasterizerStorageGLES3::light_omni_set_shadow_mode(RID p_light, VS::LightOmniShadowMode p_mode) {
	Light *light = light_owner.getornull(p_light);
	ERR_FAIL_COND(!light);

	light->omni_shadow_mode = p_mode;

	light->version++;
	light->instance_change_notify(true, false);
}

VS::LightOmniShadowMode RasterizerStorageGLES3::light_omni_get_shadow_mode(RID p_light) {
	const Light *light = light_owner.getornull(p_light);
	ERR_FAIL_COND_V(!light, VS::LIGHT_OMNI_SHADOW_CUBE);

	return light->omni_shadow_mode;
}

void RasterizerStorageGLES3::light_omni_set_shadow_detail(RID p_light, VS::LightOmniShadowDetail p_detail) {
	Light *light = light_owner.getornull(p_light);
	ERR_FAIL_COND(!light);

	light->omni_shadow_detail = p_detail;
	light->version++;
	light->instance_change_notify(true, false);
}

void RasterizerStorageGLES3::light_directional_set_shadow_mode(RID p_light, VS::LightDirectionalShadowMode p_mode) {
	Light *light = light_owner.getornull(p_light);
	ERR_FAIL_COND(!light);

	light->directional_shadow_mode = p_mode;
	light->version++;
	light->instance_change_notify(true, false);
}

void RasterizerStorageGLES3::light_directional_set_blend_splits(RID p_light, bool p_enable) {
	Light *light = light_owner.getornull(p_light);
	ERR_FAIL_COND(!light);

	light->directional_blend_splits = p_enable;
	light->version++;
	light->instance_change_notify(true, false);
}

bool RasterizerStorageGLES3::light_directional_get_blend_splits(RID p_light) const {
	const Light *light = light_owner.getornull(p_light);
	ERR_FAIL_COND_V(!light, false);

	return light->directional_blend_splits;
}

VS::LightDirectionalShadowMode RasterizerStorageGLES3::light_directional_get_shadow_mode(RID p_light) {
	const Light *light = light_owner.getornull(p_light);
	ERR_FAIL_COND_V(!light, VS::LIGHT_DIRECTIONAL_SHADOW_ORTHOGONAL);

	return light->directional_shadow_mode;
}

void RasterizerStorageGLES3::light_directional_set_shadow_depth_range_mode(RID p_light, VS::LightDirectionalShadowDepthRangeMode p_range_mode) {
	Light *light = light_owner.getornull(p_light);
	ERR_FAIL_COND(!light);

	light->directional_range_mode = p_range_mode;
}

VS::LightDirectionalShadowDepthRangeMode RasterizerStorageGLES3::light_directional_get_shadow_depth_range_mode(RID p_light) const {
	const Light *light = light_owner.getornull(p_light);
	ERR_FAIL_COND_V(!light, VS::LIGHT_DIRECTIONAL_SHADOW_DEPTH_RANGE_STABLE);

	return light->directional_range_mode;
}

VS::LightType RasterizerStorageGLES3::light_get_type(RID p_light) const {
	const Light *light = light_owner.getornull(p_light);
	ERR_FAIL_COND_V(!light, VS::LIGHT_DIRECTIONAL);

	return light->type;
}

float RasterizerStorageGLES3::light_get_param(RID p_light, VS::LightParam p_param) {
	const Light *light = light_owner.getornull(p_light);
	ERR_FAIL_COND_V(!light, VS::LIGHT_DIRECTIONAL);

	return light->param[p_param];
}

Color RasterizerStorageGLES3::light_get_color(RID p_light) {
	const Light *light = light_owner.getornull(p_light);
	ERR_FAIL_COND_V(!light, Color());

	return light->color;
}

bool RasterizerStorageGLES3::light_get_use_gi(RID p_light) {
	return light_get_bake_mode(p_light) != VS::LightBakeMode::LIGHT_BAKE_DISABLED;
}

VS::LightBakeMode RasterizerStorageGLES3::light_get_bake_mode(RID p_light) {
	Light *light = light_owner.getornull(p_light);
	ERR_FAIL_COND_V(!light, VS::LightBakeMode::LIGHT_BAKE_DISABLED);

	return light->bake_mode;
}

bool RasterizerStorageGLES3::light_has_shadow(RID p_light) const {
	const Light *light = light_owner.getornull(p_light);
	ERR_FAIL_COND_V(!light, VS::LIGHT_DIRECTIONAL);

	return light->shadow;
}

uint64_t RasterizerStorageGLES3::light_get_version(RID p_light) const {
	const Light *light = light_owner.getornull(p_light);
	ERR_FAIL_COND_V(!light, 0);

	return light->version;
}

AABB RasterizerStorageGLES3::light_get_aabb(RID p_light) const {
	const Light *light = light_owner.getornull(p_light);
	ERR_FAIL_COND_V(!light, AABB());

	switch (light->type) {
		case VS::LIGHT_SPOT: {
			float len = light->param[VS::LIGHT_PARAM_RANGE];
			float size = Math::tan(Math::deg2rad(light->param[VS::LIGHT_PARAM_SPOT_ANGLE])) * len;
			return AABB(Vector3(-size, -size, -len), Vector3(size * 2, size * 2, len));
		};
		case VS::LIGHT_OMNI: {
			float r = light->param[VS::LIGHT_PARAM_RANGE];
			return AABB(-Vector3(r, r, r), Vector3(r, r, r) * 2);
		};
		case VS::LIGHT_DIRECTIONAL: {
			return AABB();
		};
	}

	ERR_FAIL_V(AABB());
}

/* PROBE API */

RID RasterizerStorageGLES3::reflection_probe_create() {
	ReflectionProbe *reflection_probe = memnew(ReflectionProbe);

	reflection_probe->intensity = 1.0;
	reflection_probe->interior_ambient = Color();
	reflection_probe->interior_ambient_energy = 1.0;
	reflection_probe->interior_ambient_probe_contrib = 0.0;

	reflection_probe->max_distance = 0;
	reflection_probe->extents = Vector3(1, 1, 1);
	reflection_probe->origin_offset = Vector3(0, 0, 0);
	reflection_probe->interior = false;
	reflection_probe->box_projection = false;
	reflection_probe->enable_shadows = false;
	reflection_probe->cull_mask = (1 << 20) - 1;
	reflection_probe->update_mode = VS::REFLECTION_PROBE_UPDATE_ONCE;

	return reflection_probe_owner.make_rid(reflection_probe);
}

void RasterizerStorageGLES3::reflection_probe_set_update_mode(RID p_probe, VS::ReflectionProbeUpdateMode p_mode) {
	ReflectionProbe *reflection_probe = reflection_probe_owner.getornull(p_probe);
	ERR_FAIL_COND(!reflection_probe);

	reflection_probe->update_mode = p_mode;
	reflection_probe->instance_change_notify(true, false);
}

void RasterizerStorageGLES3::reflection_probe_set_intensity(RID p_probe, float p_intensity) {
	ReflectionProbe *reflection_probe = reflection_probe_owner.getornull(p_probe);
	ERR_FAIL_COND(!reflection_probe);

	reflection_probe->intensity = p_intensity;
}

void RasterizerStorageGLES3::reflection_probe_set_interior_ambient(RID p_probe, const Color &p_ambient) {
	ReflectionProbe *reflection_probe = reflection_probe_owner.getornull(p_probe);
	ERR_FAIL_COND(!reflection_probe);

	reflection_probe->interior_ambient = p_ambient;
}

void RasterizerStorageGLES3::reflection_probe_set_interior_ambient_energy(RID p_probe, float p_energy) {
	ReflectionProbe *reflection_probe = reflection_probe_owner.getornull(p_probe);
	ERR_FAIL_COND(!reflection_probe);

	reflection_probe->interior_ambient_energy = p_energy;
}

void RasterizerStorageGLES3::reflection_probe_set_interior_ambient_probe_contribution(RID p_probe, float p_contrib) {
	ReflectionProbe *reflection_probe = reflection_probe_owner.getornull(p_probe);
	ERR_FAIL_COND(!reflection_probe);

	reflection_probe->interior_ambient_probe_contrib = p_contrib;
}

void RasterizerStorageGLES3::reflection_probe_set_max_distance(RID p_probe, float p_distance) {
	ReflectionProbe *reflection_probe = reflection_probe_owner.getornull(p_probe);
	ERR_FAIL_COND(!reflection_probe);

	reflection_probe->max_distance = p_distance;
	reflection_probe->instance_change_notify(true, false);
}
void RasterizerStorageGLES3::reflection_probe_set_extents(RID p_probe, const Vector3 &p_extents) {
	ReflectionProbe *reflection_probe = reflection_probe_owner.getornull(p_probe);
	ERR_FAIL_COND(!reflection_probe);

	reflection_probe->extents = p_extents;
	reflection_probe->instance_change_notify(true, false);
}
void RasterizerStorageGLES3::reflection_probe_set_origin_offset(RID p_probe, const Vector3 &p_offset) {
	ReflectionProbe *reflection_probe = reflection_probe_owner.getornull(p_probe);
	ERR_FAIL_COND(!reflection_probe);

	reflection_probe->origin_offset = p_offset;
	reflection_probe->instance_change_notify(true, false);
}

void RasterizerStorageGLES3::reflection_probe_set_as_interior(RID p_probe, bool p_enable) {
	ReflectionProbe *reflection_probe = reflection_probe_owner.getornull(p_probe);
	ERR_FAIL_COND(!reflection_probe);

	reflection_probe->interior = p_enable;
	reflection_probe->instance_change_notify(true, false);
}
void RasterizerStorageGLES3::reflection_probe_set_enable_box_projection(RID p_probe, bool p_enable) {
	ReflectionProbe *reflection_probe = reflection_probe_owner.getornull(p_probe);
	ERR_FAIL_COND(!reflection_probe);

	reflection_probe->box_projection = p_enable;
}

void RasterizerStorageGLES3::reflection_probe_set_enable_shadows(RID p_probe, bool p_enable) {
	ReflectionProbe *reflection_probe = reflection_probe_owner.getornull(p_probe);
	ERR_FAIL_COND(!reflection_probe);

	reflection_probe->enable_shadows = p_enable;
	reflection_probe->instance_change_notify(true, false);
}
void RasterizerStorageGLES3::reflection_probe_set_cull_mask(RID p_probe, uint32_t p_layers) {
	ReflectionProbe *reflection_probe = reflection_probe_owner.getornull(p_probe);
	ERR_FAIL_COND(!reflection_probe);

	reflection_probe->cull_mask = p_layers;
	reflection_probe->instance_change_notify(true, false);
}

void RasterizerStorageGLES3::reflection_probe_set_resolution(RID p_probe, int p_resolution) {
}

AABB RasterizerStorageGLES3::reflection_probe_get_aabb(RID p_probe) const {
	const ReflectionProbe *reflection_probe = reflection_probe_owner.getornull(p_probe);
	ERR_FAIL_COND_V(!reflection_probe, AABB());

	AABB aabb;
	aabb.position = -reflection_probe->extents;
	aabb.size = reflection_probe->extents * 2.0;

	return aabb;
}
VS::ReflectionProbeUpdateMode RasterizerStorageGLES3::reflection_probe_get_update_mode(RID p_probe) const {
	const ReflectionProbe *reflection_probe = reflection_probe_owner.getornull(p_probe);
	ERR_FAIL_COND_V(!reflection_probe, VS::REFLECTION_PROBE_UPDATE_ALWAYS);

	return reflection_probe->update_mode;
}

uint32_t RasterizerStorageGLES3::reflection_probe_get_cull_mask(RID p_probe) const {
	const ReflectionProbe *reflection_probe = reflection_probe_owner.getornull(p_probe);
	ERR_FAIL_COND_V(!reflection_probe, 0);

	return reflection_probe->cull_mask;
}

Vector3 RasterizerStorageGLES3::reflection_probe_get_extents(RID p_probe) const {
	const ReflectionProbe *reflection_probe = reflection_probe_owner.getornull(p_probe);
	ERR_FAIL_COND_V(!reflection_probe, Vector3());

	return reflection_probe->extents;
}
Vector3 RasterizerStorageGLES3::reflection_probe_get_origin_offset(RID p_probe) const {
	const ReflectionProbe *reflection_probe = reflection_probe_owner.getornull(p_probe);
	ERR_FAIL_COND_V(!reflection_probe, Vector3());

	return reflection_probe->origin_offset;
}

bool RasterizerStorageGLES3::reflection_probe_renders_shadows(RID p_probe) const {
	const ReflectionProbe *reflection_probe = reflection_probe_owner.getornull(p_probe);
	ERR_FAIL_COND_V(!reflection_probe, false);

	return reflection_probe->enable_shadows;
}

float RasterizerStorageGLES3::reflection_probe_get_origin_max_distance(RID p_probe) const {
	const ReflectionProbe *reflection_probe = reflection_probe_owner.getornull(p_probe);
	ERR_FAIL_COND_V(!reflection_probe, 0);

	return reflection_probe->max_distance;
}

RID RasterizerStorageGLES3::gi_probe_create() {
	GIProbe *gip = memnew(GIProbe);

	gip->bounds = AABB(Vector3(), Vector3(1, 1, 1));
	gip->dynamic_range = 1.0;
	gip->energy = 1.0;
	gip->propagation = 1.0;
	gip->bias = 0.4;
	gip->normal_bias = 0.4;
	gip->interior = false;
	gip->compress = false;
	gip->version = 1;
	gip->cell_size = 1.0;

	return gi_probe_owner.make_rid(gip);
}

void RasterizerStorageGLES3::gi_probe_set_bounds(RID p_probe, const AABB &p_bounds) {
	GIProbe *gip = gi_probe_owner.getornull(p_probe);
	ERR_FAIL_COND(!gip);

	gip->bounds = p_bounds;
	gip->version++;
	gip->instance_change_notify(true, false);
}
AABB RasterizerStorageGLES3::gi_probe_get_bounds(RID p_probe) const {
	const GIProbe *gip = gi_probe_owner.getornull(p_probe);
	ERR_FAIL_COND_V(!gip, AABB());

	return gip->bounds;
}

void RasterizerStorageGLES3::gi_probe_set_cell_size(RID p_probe, float p_size) {
	GIProbe *gip = gi_probe_owner.getornull(p_probe);
	ERR_FAIL_COND(!gip);

	gip->cell_size = p_size;
	gip->version++;
	gip->instance_change_notify(true, false);
}

float RasterizerStorageGLES3::gi_probe_get_cell_size(RID p_probe) const {
	const GIProbe *gip = gi_probe_owner.getornull(p_probe);
	ERR_FAIL_COND_V(!gip, 0);

	return gip->cell_size;
}

void RasterizerStorageGLES3::gi_probe_set_to_cell_xform(RID p_probe, const Transform &p_xform) {
	GIProbe *gip = gi_probe_owner.getornull(p_probe);
	ERR_FAIL_COND(!gip);

	gip->to_cell = p_xform;
}

Transform RasterizerStorageGLES3::gi_probe_get_to_cell_xform(RID p_probe) const {
	const GIProbe *gip = gi_probe_owner.getornull(p_probe);
	ERR_FAIL_COND_V(!gip, Transform());

	return gip->to_cell;
}

void RasterizerStorageGLES3::gi_probe_set_dynamic_data(RID p_probe, const PoolVector<int> &p_data) {
	GIProbe *gip = gi_probe_owner.getornull(p_probe);
	ERR_FAIL_COND(!gip);

	gip->dynamic_data = p_data;
	gip->version++;
	gip->instance_change_notify(true, false);
}
PoolVector<int> RasterizerStorageGLES3::gi_probe_get_dynamic_data(RID p_probe) const {
	const GIProbe *gip = gi_probe_owner.getornull(p_probe);
	ERR_FAIL_COND_V(!gip, PoolVector<int>());

	return gip->dynamic_data;
}

void RasterizerStorageGLES3::gi_probe_set_dynamic_range(RID p_probe, int p_range) {
	GIProbe *gip = gi_probe_owner.getornull(p_probe);
	ERR_FAIL_COND(!gip);

	gip->dynamic_range = p_range;
}
int RasterizerStorageGLES3::gi_probe_get_dynamic_range(RID p_probe) const {
	const GIProbe *gip = gi_probe_owner.getornull(p_probe);
	ERR_FAIL_COND_V(!gip, 0);

	return gip->dynamic_range;
}

void RasterizerStorageGLES3::gi_probe_set_energy(RID p_probe, float p_range) {
	GIProbe *gip = gi_probe_owner.getornull(p_probe);
	ERR_FAIL_COND(!gip);

	gip->energy = p_range;
}

void RasterizerStorageGLES3::gi_probe_set_bias(RID p_probe, float p_range) {
	GIProbe *gip = gi_probe_owner.getornull(p_probe);
	ERR_FAIL_COND(!gip);

	gip->bias = p_range;
}

void RasterizerStorageGLES3::gi_probe_set_normal_bias(RID p_probe, float p_range) {
	GIProbe *gip = gi_probe_owner.getornull(p_probe);
	ERR_FAIL_COND(!gip);

	gip->normal_bias = p_range;
}

void RasterizerStorageGLES3::gi_probe_set_propagation(RID p_probe, float p_range) {
	GIProbe *gip = gi_probe_owner.getornull(p_probe);
	ERR_FAIL_COND(!gip);

	gip->propagation = p_range;
}

void RasterizerStorageGLES3::gi_probe_set_interior(RID p_probe, bool p_enable) {
	GIProbe *gip = gi_probe_owner.getornull(p_probe);
	ERR_FAIL_COND(!gip);

	gip->interior = p_enable;
}

bool RasterizerStorageGLES3::gi_probe_is_interior(RID p_probe) const {
	const GIProbe *gip = gi_probe_owner.getornull(p_probe);
	ERR_FAIL_COND_V(!gip, false);

	return gip->interior;
}

void RasterizerStorageGLES3::gi_probe_set_compress(RID p_probe, bool p_enable) {
	if (p_enable) {
		WARN_DEPRECATED_MSG("GIProbe's Compress property has been deprecated due to known bugs and will be removed in Godot 4.0.");
	}

	GIProbe *gip = gi_probe_owner.getornull(p_probe);
	ERR_FAIL_COND(!gip);

	gip->compress = p_enable;
}

bool RasterizerStorageGLES3::gi_probe_is_compressed(RID p_probe) const {
	const GIProbe *gip = gi_probe_owner.getornull(p_probe);
	ERR_FAIL_COND_V(!gip, false);

	return gip->compress;
}
float RasterizerStorageGLES3::gi_probe_get_energy(RID p_probe) const {
	const GIProbe *gip = gi_probe_owner.getornull(p_probe);
	ERR_FAIL_COND_V(!gip, 0);

	return gip->energy;
}

float RasterizerStorageGLES3::gi_probe_get_bias(RID p_probe) const {
	const GIProbe *gip = gi_probe_owner.getornull(p_probe);
	ERR_FAIL_COND_V(!gip, 0);

	return gip->bias;
}

float RasterizerStorageGLES3::gi_probe_get_normal_bias(RID p_probe) const {
	const GIProbe *gip = gi_probe_owner.getornull(p_probe);
	ERR_FAIL_COND_V(!gip, 0);

	return gip->normal_bias;
}

float RasterizerStorageGLES3::gi_probe_get_propagation(RID p_probe) const {
	const GIProbe *gip = gi_probe_owner.getornull(p_probe);
	ERR_FAIL_COND_V(!gip, 0);

	return gip->propagation;
}

uint32_t RasterizerStorageGLES3::gi_probe_get_version(RID p_probe) {
	const GIProbe *gip = gi_probe_owner.getornull(p_probe);
	ERR_FAIL_COND_V(!gip, 0);

	return gip->version;
}

RID RasterizerStorageGLES3::gi_probe_dynamic_data_create(int p_width, int p_height, int p_depth, GIProbeCompression p_compression) {
	GIProbeData *gipd = memnew(GIProbeData);

	gipd->width = p_width;
	gipd->height = p_height;
	gipd->depth = p_depth;
	gipd->compression = GI_PROBE_UNCOMPRESSED;

	gl_wrapper.gl_active_texture(GL_TEXTURE0);
	glGenTextures(1, &gipd->tex_id);
	glBindTexture(GL_TEXTURE_3D, gipd->tex_id);

	int level = 0;
	int min_size = 1;

	if (gipd->compression == GI_PROBE_S3TC) {
		min_size = 4;
	}

	while (true) {
		glTexImage3D(GL_TEXTURE_3D, level, GL_RGBA8, p_width, p_height, p_depth, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

		if (p_width <= min_size || p_height <= min_size || p_depth <= min_size) {
			break;
		}
		p_width >>= 1;
		p_height >>= 1;
		p_depth >>= 1;
		level++;
	}

	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_BASE_LEVEL, 0);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAX_LEVEL, level);

	gipd->levels = level + 1;

	return gi_probe_data_owner.make_rid(gipd);
}

void RasterizerStorageGLES3::gi_probe_dynamic_data_update(RID p_gi_probe_data, int p_depth_slice, int p_slice_count, int p_mipmap, const void *p_data) {
	GIProbeData *gipd = gi_probe_data_owner.getornull(p_gi_probe_data);
	ERR_FAIL_COND(!gipd);
	/*
	Vector<uint8_t> data;
	data.resize((gipd->width>>p_mipmap)*(gipd->height>>p_mipmap)*(gipd->depth>>p_mipmap)*4);

	for(int i=0;i<(gipd->width>>p_mipmap);i++) {
		for(int j=0;j<(gipd->height>>p_mipmap);j++) {
			for(int k=0;k<(gipd->depth>>p_mipmap);k++) {

				int ofs = (k*(gipd->height>>p_mipmap)*(gipd->width>>p_mipmap)) + j *(gipd->width>>p_mipmap) + i;
				ofs*=4;
				data[ofs+0]=i*0xFF/(gipd->width>>p_mipmap);
				data[ofs+1]=j*0xFF/(gipd->height>>p_mipmap);
				data[ofs+2]=k*0xFF/(gipd->depth>>p_mipmap);
				data[ofs+3]=0xFF;
			}
		}
	}
*/
	gl_wrapper.gl_active_texture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_3D, gipd->tex_id);
	glTexSubImage3D(GL_TEXTURE_3D, p_mipmap, 0, 0, p_depth_slice, gipd->width >> p_mipmap, gipd->height >> p_mipmap, p_slice_count, GL_RGBA, GL_UNSIGNED_BYTE, p_data);
	//glTexImage3D(GL_TEXTURE_3D,p_mipmap,GL_RGBA8,gipd->width>>p_mipmap,gipd->height>>p_mipmap,gipd->depth>>p_mipmap,0,GL_RGBA,GL_UNSIGNED_BYTE,p_data);
	//glTexImage3D(GL_TEXTURE_3D,p_mipmap,GL_RGBA8,gipd->width>>p_mipmap,gipd->height>>p_mipmap,gipd->depth>>p_mipmap,0,GL_RGBA,GL_UNSIGNED_BYTE,data.ptr());
}
/////////////////////////////

RID RasterizerStorageGLES3::lightmap_capture_create() {
	LightmapCapture *capture = memnew(LightmapCapture);
	return lightmap_capture_data_owner.make_rid(capture);
}

void RasterizerStorageGLES3::lightmap_capture_set_bounds(RID p_capture, const AABB &p_bounds) {
	LightmapCapture *capture = lightmap_capture_data_owner.getornull(p_capture);
	ERR_FAIL_COND(!capture);
	capture->bounds = p_bounds;
	capture->instance_change_notify(true, false);
}
AABB RasterizerStorageGLES3::lightmap_capture_get_bounds(RID p_capture) const {
	const LightmapCapture *capture = lightmap_capture_data_owner.getornull(p_capture);
	ERR_FAIL_COND_V(!capture, AABB());
	return capture->bounds;
}
void RasterizerStorageGLES3::lightmap_capture_set_octree(RID p_capture, const PoolVector<uint8_t> &p_octree) {
	LightmapCapture *capture = lightmap_capture_data_owner.getornull(p_capture);
	ERR_FAIL_COND(!capture);

	ERR_FAIL_COND(p_octree.size() == 0 || (p_octree.size() % sizeof(LightmapCaptureOctree)) != 0);

	capture->octree.resize(p_octree.size() / sizeof(LightmapCaptureOctree));
	if (p_octree.size()) {
		PoolVector<LightmapCaptureOctree>::Write w = capture->octree.write();
		PoolVector<uint8_t>::Read r = p_octree.read();
		memcpy(w.ptr(), r.ptr(), p_octree.size());
	}
	capture->instance_change_notify(true, false);
}
PoolVector<uint8_t> RasterizerStorageGLES3::lightmap_capture_get_octree(RID p_capture) const {
	const LightmapCapture *capture = lightmap_capture_data_owner.getornull(p_capture);
	ERR_FAIL_COND_V(!capture, PoolVector<uint8_t>());

	if (capture->octree.size() == 0) {
		return PoolVector<uint8_t>();
	}

	PoolVector<uint8_t> ret;
	ret.resize(capture->octree.size() * sizeof(LightmapCaptureOctree));
	{
		PoolVector<LightmapCaptureOctree>::Read r = capture->octree.read();
		PoolVector<uint8_t>::Write w = ret.write();
		memcpy(w.ptr(), r.ptr(), ret.size());
	}

	return ret;
}

void RasterizerStorageGLES3::lightmap_capture_set_octree_cell_transform(RID p_capture, const Transform &p_xform) {
	LightmapCapture *capture = lightmap_capture_data_owner.getornull(p_capture);
	ERR_FAIL_COND(!capture);
	capture->cell_xform = p_xform;
}

Transform RasterizerStorageGLES3::lightmap_capture_get_octree_cell_transform(RID p_capture) const {
	const LightmapCapture *capture = lightmap_capture_data_owner.getornull(p_capture);
	ERR_FAIL_COND_V(!capture, Transform());
	return capture->cell_xform;
}

void RasterizerStorageGLES3::lightmap_capture_set_octree_cell_subdiv(RID p_capture, int p_subdiv) {
	LightmapCapture *capture = lightmap_capture_data_owner.getornull(p_capture);
	ERR_FAIL_COND(!capture);
	capture->cell_subdiv = p_subdiv;
}

int RasterizerStorageGLES3::lightmap_capture_get_octree_cell_subdiv(RID p_capture) const {
	const LightmapCapture *capture = lightmap_capture_data_owner.getornull(p_capture);
	ERR_FAIL_COND_V(!capture, 0);
	return capture->cell_subdiv;
}

void RasterizerStorageGLES3::lightmap_capture_set_energy(RID p_capture, float p_energy) {
	LightmapCapture *capture = lightmap_capture_data_owner.getornull(p_capture);
	ERR_FAIL_COND(!capture);
	capture->energy = p_energy;

	if (!capture->update_list.in_list()) {
		capture_update_list.add(&capture->update_list);
	}
}

float RasterizerStorageGLES3::lightmap_capture_get_energy(RID p_capture) const {
	const LightmapCapture *capture = lightmap_capture_data_owner.getornull(p_capture);
	ERR_FAIL_COND_V(!capture, 0);
	return capture->energy;
}

void RasterizerStorageGLES3::lightmap_capture_set_interior(RID p_capture, bool p_interior) {
	LightmapCapture *capture = lightmap_capture_data_owner.getornull(p_capture);
	ERR_FAIL_COND(!capture);
	capture->interior = p_interior;
	if (!capture->update_list.in_list()) {
		capture_update_list.add(&capture->update_list);
	}
}

bool RasterizerStorageGLES3::lightmap_capture_is_interior(RID p_capture) const {
	const LightmapCapture *capture = lightmap_capture_data_owner.getornull(p_capture);
	ERR_FAIL_COND_V(!capture, false);
	return capture->interior;
}

const PoolVector<RasterizerStorage::LightmapCaptureOctree> *RasterizerStorageGLES3::lightmap_capture_get_octree_ptr(RID p_capture) const {
	const LightmapCapture *capture = lightmap_capture_data_owner.getornull(p_capture);
	ERR_FAIL_COND_V(!capture, nullptr);
	return &capture->octree;
}

void RasterizerStorageGLES3::update_dirty_captures() {
	while (capture_update_list.first()) {
		LightmapCapture *capture = capture_update_list.first()->self();
		capture->instance_change_notify(false, true);
		capture_update_list.remove(capture_update_list.first());
	}
}

///////

RID RasterizerStorageGLES3::particles_create() {
	Particles *particles = memnew(Particles);

	return particles_owner.make_rid(particles);
}

void RasterizerStorageGLES3::particles_set_emitting(RID p_particles, bool p_emitting) {
	Particles *particles = particles_owner.getornull(p_particles);
	ERR_FAIL_COND(!particles);

	particles->emitting = p_emitting;
}

bool RasterizerStorageGLES3::particles_get_emitting(RID p_particles) {
	Particles *particles = particles_owner.getornull(p_particles);
	ERR_FAIL_COND_V(!particles, false);

	return particles->emitting;
}

void RasterizerStorageGLES3::particles_set_amount(RID p_particles, int p_amount) {
	Particles *particles = particles_owner.getornull(p_particles);
	ERR_FAIL_COND(!particles);

	particles->amount = p_amount;

	int floats = p_amount * 24;
	float *data = memnew_arr(float, floats);

	for (int i = 0; i < floats; i++) {
		data[i] = 0;
	}

	for (int i = 0; i < 2; i++) {
		glBindVertexArray(particles->particle_vaos[i]);

		glBindBuffer(GL_ARRAY_BUFFER, particles->particle_buffers[i]);
		glBufferData(GL_ARRAY_BUFFER, floats * sizeof(float), data, GL_STATIC_DRAW);

		for (int j = 0; j < 6; j++) {
			glEnableVertexAttribArray(j);
			glVertexAttribPointer(j, 4, GL_FLOAT, GL_FALSE, sizeof(float) * 4 * 6, CAST_INT_TO_UCHAR_PTR(j * 16));
		}
	}

	if (particles->histories_enabled) {
		for (int i = 0; i < 2; i++) {
			glBindVertexArray(particles->particle_vao_histories[i]);

			glBindBuffer(GL_ARRAY_BUFFER, particles->particle_buffer_histories[i]);
			glBufferData(GL_ARRAY_BUFFER, floats * sizeof(float), data, GL_DYNAMIC_COPY);

			for (int j = 0; j < 6; j++) {
				glEnableVertexAttribArray(j);
				glVertexAttribPointer(j, 4, GL_FLOAT, GL_FALSE, sizeof(float) * 4 * 6, CAST_INT_TO_UCHAR_PTR(j * 16));
			}
			particles->particle_valid_histories[i] = false;
		}
	}

	glBindVertexArray(0);

	particles->prev_ticks = 0;
	particles->phase = 0;
	particles->prev_phase = 0;
	particles->clear = true;

	memdelete_arr(data);
}

void RasterizerStorageGLES3::particles_set_lifetime(RID p_particles, float p_lifetime) {
	Particles *particles = particles_owner.getornull(p_particles);
	ERR_FAIL_COND(!particles);
	particles->lifetime = p_lifetime;
}

void RasterizerStorageGLES3::particles_set_one_shot(RID p_particles, bool p_one_shot) {
	Particles *particles = particles_owner.getornull(p_particles);
	ERR_FAIL_COND(!particles);
	particles->one_shot = p_one_shot;
}

void RasterizerStorageGLES3::particles_set_pre_process_time(RID p_particles, float p_time) {
	Particles *particles = particles_owner.getornull(p_particles);
	ERR_FAIL_COND(!particles);
	particles->pre_process_time = p_time;
}
void RasterizerStorageGLES3::particles_set_explosiveness_ratio(RID p_particles, float p_ratio) {
	Particles *particles = particles_owner.getornull(p_particles);
	ERR_FAIL_COND(!particles);
	particles->explosiveness = p_ratio;
}
void RasterizerStorageGLES3::particles_set_randomness_ratio(RID p_particles, float p_ratio) {
	Particles *particles = particles_owner.getornull(p_particles);
	ERR_FAIL_COND(!particles);
	particles->randomness = p_ratio;
}

void RasterizerStorageGLES3::_particles_update_histories(Particles *particles) {
	bool needs_histories = particles->draw_order == VS::PARTICLES_DRAW_ORDER_VIEW_DEPTH;

	if (needs_histories == particles->histories_enabled) {
		return;
	}

	particles->histories_enabled = needs_histories;

	int floats = particles->amount * 24;

	if (!needs_histories) {
		glDeleteBuffers(2, particles->particle_buffer_histories);
		glDeleteVertexArrays(2, particles->particle_vao_histories);

	} else {
		glGenBuffers(2, particles->particle_buffer_histories);
		glGenVertexArrays(2, particles->particle_vao_histories);

		for (int i = 0; i < 2; i++) {
			glBindVertexArray(particles->particle_vao_histories[i]);

			glBindBuffer(GL_ARRAY_BUFFER, particles->particle_buffer_histories[i]);
			glBufferData(GL_ARRAY_BUFFER, floats * sizeof(float), nullptr, GL_DYNAMIC_COPY);

			for (int j = 0; j < 6; j++) {
				glEnableVertexAttribArray(j);
				glVertexAttribPointer(j, 4, GL_FLOAT, GL_FALSE, sizeof(float) * 4 * 6, CAST_INT_TO_UCHAR_PTR(j * 16));
			}

			particles->particle_valid_histories[i] = false;
		}
	}

	particles->clear = true;
}

void RasterizerStorageGLES3::particles_set_custom_aabb(RID p_particles, const AABB &p_aabb) {
	Particles *particles = particles_owner.getornull(p_particles);
	ERR_FAIL_COND(!particles);
	particles->custom_aabb = p_aabb;
	_particles_update_histories(particles);
	particles->instance_change_notify(true, false);
}

void RasterizerStorageGLES3::particles_set_speed_scale(RID p_particles, float p_scale) {
	Particles *particles = particles_owner.getornull(p_particles);
	ERR_FAIL_COND(!particles);

	particles->speed_scale = p_scale;
}
void RasterizerStorageGLES3::particles_set_use_local_coordinates(RID p_particles, bool p_enable) {
	Particles *particles = particles_owner.getornull(p_particles);
	ERR_FAIL_COND(!particles);

	particles->use_local_coords = p_enable;
}

void RasterizerStorageGLES3::particles_set_fixed_fps(RID p_particles, int p_fps) {
	Particles *particles = particles_owner.getornull(p_particles);
	ERR_FAIL_COND(!particles);

	particles->fixed_fps = p_fps;
}

void RasterizerStorageGLES3::particles_set_fractional_delta(RID p_particles, bool p_enable) {
	Particles *particles = particles_owner.getornull(p_particles);
	ERR_FAIL_COND(!particles);

	particles->fractional_delta = p_enable;
}

void RasterizerStorageGLES3::particles_set_process_material(RID p_particles, RID p_material) {
	Particles *particles = particles_owner.getornull(p_particles);
	ERR_FAIL_COND(!particles);

	particles->process_material = p_material;
}

void RasterizerStorageGLES3::particles_set_draw_order(RID p_particles, VS::ParticlesDrawOrder p_order) {
	Particles *particles = particles_owner.getornull(p_particles);
	ERR_FAIL_COND(!particles);

	particles->draw_order = p_order;
	_particles_update_histories(particles);
}

void RasterizerStorageGLES3::particles_set_draw_passes(RID p_particles, int p_passes) {
	Particles *particles = particles_owner.getornull(p_particles);
	ERR_FAIL_COND(!particles);

	particles->draw_passes.resize(p_passes);
}

void RasterizerStorageGLES3::particles_set_draw_pass_mesh(RID p_particles, int p_pass, RID p_mesh) {
	Particles *particles = particles_owner.getornull(p_particles);
	ERR_FAIL_COND(!particles);
	ERR_FAIL_INDEX(p_pass, particles->draw_passes.size());
	particles->draw_passes.write[p_pass] = p_mesh;
}

void RasterizerStorageGLES3::particles_restart(RID p_particles) {
	Particles *particles = particles_owner.getornull(p_particles);
	ERR_FAIL_COND(!particles);

	particles->restart_request = true;
}

void RasterizerStorageGLES3::particles_request_process(RID p_particles) {
	Particles *particles = particles_owner.getornull(p_particles);
	ERR_FAIL_COND(!particles);

	if (!particles->particle_element.in_list()) {
		particle_update_list.add(&particles->particle_element);
	}
}

AABB RasterizerStorageGLES3::particles_get_current_aabb(RID p_particles) {
	const Particles *particles = particles_owner.getornull(p_particles);
	ERR_FAIL_COND_V(!particles, AABB());

	const float *data;
	glBindBuffer(GL_ARRAY_BUFFER, particles->particle_buffers[0]);

#if defined(GLES_OVER_GL) || defined(__EMSCRIPTEN__)
	PoolVector<uint8_t> vector;
	vector.resize(particles->amount * 16 * 6);
	{
		PoolVector<uint8_t>::Write w = vector.write();
		glGetBufferSubData(GL_ARRAY_BUFFER, 0, particles->amount * 16 * 6, w.ptr());
	}
	PoolVector<uint8_t>::Read r = vector.read();
	data = reinterpret_cast<const float *>(r.ptr());
#else
	data = (float *)glMapBufferRange(GL_ARRAY_BUFFER, 0, particles->amount * 16 * 6, GL_MAP_READ_BIT);
#endif
	AABB aabb;

	Transform inv = particles->emission_transform.affine_inverse();

	for (int i = 0; i < particles->amount; i++) {
		int ofs = i * 24;
		Vector3 pos = Vector3(data[ofs + 15], data[ofs + 19], data[ofs + 23]);
		if (!particles->use_local_coords) {
			pos = inv.xform(pos);
		}
		if (i == 0) {
			aabb.position = pos;
		} else {
			aabb.expand_to(pos);
		}
	}

#if defined(GLES_OVER_GL) || defined(__EMSCRIPTEN__)
	r.release();
	vector = PoolVector<uint8_t>();
#else
	glUnmapBuffer(GL_ARRAY_BUFFER);
#endif

	glBindBuffer(GL_ARRAY_BUFFER, 0);

	float longest_axis = 0;
	for (int i = 0; i < particles->draw_passes.size(); i++) {
		if (particles->draw_passes[i].is_valid()) {
			AABB maabb = mesh_get_aabb(particles->draw_passes[i], RID());
			longest_axis = MAX(maabb.get_longest_axis_size(), longest_axis);
		}
	}

	aabb.grow_by(longest_axis);

	return aabb;
}

AABB RasterizerStorageGLES3::particles_get_aabb(RID p_particles) const {
	const Particles *particles = particles_owner.getornull(p_particles);
	ERR_FAIL_COND_V(!particles, AABB());

	return particles->custom_aabb;
}

void RasterizerStorageGLES3::particles_set_emission_transform(RID p_particles, const Transform &p_transform) {
	Particles *particles = particles_owner.getornull(p_particles);
	ERR_FAIL_COND(!particles);

	particles->emission_transform = p_transform;
}

int RasterizerStorageGLES3::particles_get_draw_passes(RID p_particles) const {
	const Particles *particles = particles_owner.getornull(p_particles);
	ERR_FAIL_COND_V(!particles, 0);

	return particles->draw_passes.size();
}

RID RasterizerStorageGLES3::particles_get_draw_pass_mesh(RID p_particles, int p_pass) const {
	const Particles *particles = particles_owner.getornull(p_particles);
	ERR_FAIL_COND_V(!particles, RID());
	ERR_FAIL_INDEX_V(p_pass, particles->draw_passes.size(), RID());

	return particles->draw_passes[p_pass];
}

void RasterizerStorageGLES3::_particles_process(Particles *p_particles, float p_delta) {
	float new_phase = Math::fmod((float)p_particles->phase + (p_delta / p_particles->lifetime) * p_particles->speed_scale, (float)1.0);

	if (p_particles->clear) {
		p_particles->cycle_number = 0;
		p_particles->random_seed = Math::rand();
	} else if (new_phase < p_particles->phase) {
		if (p_particles->one_shot) {
			p_particles->emitting = false;
			shaders.particles.set_uniform(ParticlesShaderGLES3::EMITTING, false);
		}
		p_particles->cycle_number++;
	}

	shaders.particles.set_uniform(ParticlesShaderGLES3::SYSTEM_PHASE, new_phase);
	shaders.particles.set_uniform(ParticlesShaderGLES3::PREV_SYSTEM_PHASE, p_particles->phase);
	p_particles->phase = new_phase;

	shaders.particles.set_uniform(ParticlesShaderGLES3::DELTA, p_delta * p_particles->speed_scale);
	shaders.particles.set_uniform(ParticlesShaderGLES3::CLEAR, p_particles->clear);
	glUniform1ui(shaders.particles.get_uniform_location(ParticlesShaderGLES3::RANDOM_SEED), p_particles->random_seed);

	if (p_particles->use_local_coords) {
		shaders.particles.set_uniform(ParticlesShaderGLES3::EMISSION_TRANSFORM, Transform());
	} else {
		shaders.particles.set_uniform(ParticlesShaderGLES3::EMISSION_TRANSFORM, p_particles->emission_transform);
	}

	glUniform1ui(shaders.particles.get_uniform(ParticlesShaderGLES3::CYCLE), p_particles->cycle_number);

	p_particles->clear = false;

	glBindVertexArray(p_particles->particle_vaos[0]);

	glBindBuffer(GL_ARRAY_BUFFER, 0); // ensure this is unbound per WebGL2 spec
	glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, p_particles->particle_buffers[1]);

	//		GLint size = 0;
	//		glGetBufferParameteriv(GL_ARRAY_BUFFER, GL_BUFFER_SIZE, &size);

	glBeginTransformFeedback(GL_POINTS);
	glDrawArrays(GL_POINTS, 0, p_particles->amount);
	glEndTransformFeedback();

	SWAP(p_particles->particle_buffers[0], p_particles->particle_buffers[1]);
	SWAP(p_particles->particle_vaos[0], p_particles->particle_vaos[1]);

	glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, 0);
	glBindVertexArray(0);
	/* //debug particles :D
	glBindBuffer(GL_ARRAY_BUFFER, p_particles->particle_buffers[0]);

	float *data = (float *)glMapBufferRange(GL_ARRAY_BUFFER, 0, p_particles->amount * 16 * 6, GL_MAP_READ_BIT);
	for (int i = 0; i < p_particles->amount; i++) {
		int ofs = i * 24;
		print_line(itos(i) + ":");
		print_line("\tColor: " + Color(data[ofs + 0], data[ofs + 1], data[ofs + 2], data[ofs + 3]));
		print_line("\tVelocity: " + Vector3(data[ofs + 4], data[ofs + 5], data[ofs + 6]));
		print_line("\tActive: " + itos(data[ofs + 7]));
		print_line("\tCustom: " + Color(data[ofs + 8], data[ofs + 9], data[ofs + 10], data[ofs + 11]));
		print_line("\tXF X: " + Color(data[ofs + 12], data[ofs + 13], data[ofs + 14], data[ofs + 15]));
		print_line("\tXF Y: " + Color(data[ofs + 16], data[ofs + 17], data[ofs + 18], data[ofs + 19]));
		print_line("\tXF Z: " + Color(data[ofs + 20], data[ofs + 21], data[ofs + 22], data[ofs + 23]));
	}

	glUnmapBuffer(GL_ARRAY_BUFFER);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	//*/
}

void RasterizerStorageGLES3::update_particles() {
	glEnable(GL_RASTERIZER_DISCARD);

	while (particle_update_list.first()) {
		//use transform feedback to process particles

		Particles *particles = particle_update_list.first()->self();

		if (particles->restart_request) {
			particles->prev_ticks = 0;
			particles->phase = 0;
			particles->prev_phase = 0;
			particles->clear = true;
			particles->particle_valid_histories[0] = false;
			particles->particle_valid_histories[1] = false;
			particles->restart_request = false;
		}

		if (particles->inactive && !particles->emitting) {
			particle_update_list.remove(particle_update_list.first());
			continue;
		}

		if (particles->emitting) {
			if (particles->inactive) {
				//restart system from scratch
				particles->prev_ticks = 0;
				particles->phase = 0;
				particles->prev_phase = 0;
				particles->clear = true;
				particles->particle_valid_histories[0] = false;
				particles->particle_valid_histories[1] = false;
			}
			particles->inactive = false;
			particles->inactive_time = 0;
		} else {
			particles->inactive_time += particles->speed_scale * frame.delta;
			if (particles->inactive_time > particles->lifetime * 1.2) {
				particles->inactive = true;
				particle_update_list.remove(particle_update_list.first());
				continue;
			}
		}

		Material *material = material_owner.getornull(particles->process_material);
		if (!material || !material->shader || material->shader->mode != VS::SHADER_PARTICLES) {
			shaders.particles.set_custom_shader(0);
		} else {
			shaders.particles.set_custom_shader(material->shader->custom_code_id);

			if (material->ubo_id) {
				glBindBufferBase(GL_UNIFORM_BUFFER, 0, material->ubo_id);
			}

			int tc = material->textures.size();
			RID *textures = material->textures.ptrw();
			ShaderLanguage::ShaderNode::Uniform::Hint *texture_hints = material->shader->texture_hints.ptrw();

			for (int i = 0; i < tc; i++) {
				gl_wrapper.gl_active_texture(GL_TEXTURE0 + i);

				GLenum target;
				GLuint tex;

				RasterizerStorageGLES3::Texture *t = texture_owner.getornull(textures[i]);

				if (!t) {
					//check hints
					target = GL_TEXTURE_2D;

					switch (texture_hints[i]) {
						case ShaderLanguage::ShaderNode::Uniform::HINT_BLACK_ALBEDO:
						case ShaderLanguage::ShaderNode::Uniform::HINT_BLACK: {
							tex = resources.black_tex;
						} break;
						case ShaderLanguage::ShaderNode::Uniform::HINT_TRANSPARENT: {
							tex = resources.transparent_tex;
						} break;
						case ShaderLanguage::ShaderNode::Uniform::HINT_ANISO: {
							tex = resources.aniso_tex;
						} break;
						case ShaderLanguage::ShaderNode::Uniform::HINT_NORMAL: {
							tex = resources.normal_tex;
						} break;
						default: {
							tex = resources.white_tex;
						} break;
					}
				} else {
					t = t->get_ptr(); //resolve for proxies
					target = t->target;
					tex = t->tex_id;
				}

				glBindTexture(target, tex);
			}
		}

		shaders.particles.set_conditional(ParticlesShaderGLES3::USE_FRACTIONAL_DELTA, particles->fractional_delta);

		shaders.particles.bind();

		shaders.particles.set_uniform(ParticlesShaderGLES3::TOTAL_PARTICLES, particles->amount);
		shaders.particles.set_uniform(ParticlesShaderGLES3::TIME, frame.time[0]);
		shaders.particles.set_uniform(ParticlesShaderGLES3::EXPLOSIVENESS, particles->explosiveness);
		shaders.particles.set_uniform(ParticlesShaderGLES3::LIFETIME, particles->lifetime);
		shaders.particles.set_uniform(ParticlesShaderGLES3::ATTRACTOR_COUNT, 0);
		shaders.particles.set_uniform(ParticlesShaderGLES3::EMITTING, particles->emitting);
		shaders.particles.set_uniform(ParticlesShaderGLES3::RANDOMNESS, particles->randomness);

		bool zero_time_scale = Engine::get_singleton()->get_time_scale() <= 0.0;

		if (particles->clear && particles->pre_process_time > 0.0) {
			float frame_time;
			if (particles->fixed_fps > 0) {
				frame_time = 1.0 / particles->fixed_fps;
			} else {
				frame_time = 1.0 / 30.0;
			}

			float todo = particles->pre_process_time;

			while (todo >= 0) {
				_particles_process(particles, frame_time);
				todo -= frame_time;
			}
		}

		if (particles->fixed_fps > 0) {
			float frame_time;
			float decr;
			if (zero_time_scale) {
				frame_time = 0.0;
				decr = 1.0 / particles->fixed_fps;
			} else {
				frame_time = 1.0 / particles->fixed_fps;
				decr = frame_time;
			}
			float delta = frame.delta;
			if (delta > 0.1) { //avoid recursive stalls if fps goes below 10
				delta = 0.1;
			} else if (delta <= 0.0) { //unlikely but..
				delta = 0.001;
			}
			float todo = particles->frame_remainder + delta;

			while (todo >= frame_time) {
				_particles_process(particles, frame_time);
				todo -= decr;
			}

			particles->frame_remainder = todo;

		} else {
			if (zero_time_scale) {
				_particles_process(particles, 0.0);
			} else {
				_particles_process(particles, frame.delta);
			}
		}

		particle_update_list.remove(particle_update_list.first());

		if (particles->histories_enabled) {
			SWAP(particles->particle_buffer_histories[0], particles->particle_buffer_histories[1]);
			SWAP(particles->particle_vao_histories[0], particles->particle_vao_histories[1]);
			SWAP(particles->particle_valid_histories[0], particles->particle_valid_histories[1]);

			//copy
			glBindBuffer(GL_COPY_READ_BUFFER, particles->particle_buffers[0]);
			glBindBuffer(GL_COPY_WRITE_BUFFER, particles->particle_buffer_histories[0]);
			glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, 0, 0, particles->amount * 24 * sizeof(float));

			particles->particle_valid_histories[0] = true;
		}

		particles->instance_change_notify(true, false); //make sure shadows are updated
	}

	glDisable(GL_RASTERIZER_DISCARD);
}

bool RasterizerStorageGLES3::particles_is_inactive(RID p_particles) const {
	const Particles *particles = particles_owner.getornull(p_particles);
	ERR_FAIL_COND_V(!particles, false);
	return !particles->emitting && particles->inactive;
}

////////

void RasterizerStorageGLES3::instance_add_skeleton(RID p_skeleton, RasterizerScene::InstanceBase *p_instance) {
	Skeleton *skeleton = skeleton_owner.getornull(p_skeleton);
	ERR_FAIL_COND(!skeleton);

	skeleton->instances.insert(p_instance);
}

void RasterizerStorageGLES3::instance_remove_skeleton(RID p_skeleton, RasterizerScene::InstanceBase *p_instance) {
	Skeleton *skeleton = skeleton_owner.getornull(p_skeleton);
	ERR_FAIL_COND(!skeleton);

	skeleton->instances.erase(p_instance);
}

void RasterizerStorageGLES3::instance_add_dependency(RID p_base, RasterizerScene::InstanceBase *p_instance) {
	Instantiable *inst = nullptr;
	switch (p_instance->base_type) {
		case VS::INSTANCE_MESH: {
			inst = mesh_owner.getornull(p_base);
			ERR_FAIL_COND(!inst);
		} break;
		case VS::INSTANCE_MULTIMESH: {
			inst = multimesh_owner.getornull(p_base);
			ERR_FAIL_COND(!inst);
		} break;
		case VS::INSTANCE_IMMEDIATE: {
			inst = immediate_owner.getornull(p_base);
			ERR_FAIL_COND(!inst);
		} break;
		case VS::INSTANCE_PARTICLES: {
			inst = particles_owner.getornull(p_base);
			ERR_FAIL_COND(!inst);
		} break;
		case VS::INSTANCE_REFLECTION_PROBE: {
			inst = reflection_probe_owner.getornull(p_base);
			ERR_FAIL_COND(!inst);
		} break;
		case VS::INSTANCE_LIGHT: {
			inst = light_owner.getornull(p_base);
			ERR_FAIL_COND(!inst);
		} break;
		case VS::INSTANCE_GI_PROBE: {
			inst = gi_probe_owner.getornull(p_base);
			ERR_FAIL_COND(!inst);
		} break;
		case VS::INSTANCE_LIGHTMAP_CAPTURE: {
			inst = lightmap_capture_data_owner.getornull(p_base);
			ERR_FAIL_COND(!inst);
		} break;
		default: {
			ERR_FAIL();
		}
	}

	inst->instance_list.add(&p_instance->dependency_item);
}

void RasterizerStorageGLES3::instance_remove_dependency(RID p_base, RasterizerScene::InstanceBase *p_instance) {
	Instantiable *inst = nullptr;

	switch (p_instance->base_type) {
		case VS::INSTANCE_MESH: {
			inst = mesh_owner.getornull(p_base);
			ERR_FAIL_COND(!inst);
		} break;
		case VS::INSTANCE_MULTIMESH: {
			inst = multimesh_owner.getornull(p_base);
			ERR_FAIL_COND(!inst);
		} break;
		case VS::INSTANCE_IMMEDIATE: {
			inst = immediate_owner.getornull(p_base);
			ERR_FAIL_COND(!inst);
		} break;
		case VS::INSTANCE_PARTICLES: {
			inst = particles_owner.getornull(p_base);
			ERR_FAIL_COND(!inst);
		} break;
		case VS::INSTANCE_REFLECTION_PROBE: {
			inst = reflection_probe_owner.getornull(p_base);
			ERR_FAIL_COND(!inst);
		} break;
		case VS::INSTANCE_LIGHT: {
			inst = light_owner.getornull(p_base);
			ERR_FAIL_COND(!inst);
		} break;
		case VS::INSTANCE_GI_PROBE: {
			inst = gi_probe_owner.getornull(p_base);
			ERR_FAIL_COND(!inst);
		} break;
		case VS::INSTANCE_LIGHTMAP_CAPTURE: {
			inst = lightmap_capture_data_owner.getornull(p_base);
			ERR_FAIL_COND(!inst);
		} break;
		default: {
			ERR_FAIL();
		}
	}

	inst->instance_list.remove(&p_instance->dependency_item);
}

/* RENDER TARGET */

void RasterizerStorageGLES3::_render_target_clear(RenderTarget *rt) {
	if (rt->fbo) {
		glDeleteFramebuffers(1, &rt->fbo);
		glDeleteTextures(1, &rt->color);
		rt->fbo = 0;
	}

	if (rt->buffers.active) {
		glDeleteFramebuffers(1, &rt->buffers.fbo);
		glDeleteRenderbuffers(1, &rt->buffers.depth);
		glDeleteRenderbuffers(1, &rt->buffers.diffuse);
		if (rt->buffers.effects_active) {
			glDeleteRenderbuffers(1, &rt->buffers.specular);
			glDeleteRenderbuffers(1, &rt->buffers.normal_rough);
			glDeleteRenderbuffers(1, &rt->buffers.sss);
			glDeleteFramebuffers(1, &rt->buffers.effect_fbo);
			glDeleteTextures(1, &rt->buffers.effect);
		}

		rt->buffers.effects_active = false;
		rt->buffers.active = false;
	}

	if (rt->depth) {
		glDeleteTextures(1, &rt->depth);
		rt->depth = 0;
	}

	if (rt->effects.ssao.blur_fbo[0]) {
		glDeleteFramebuffers(1, &rt->effects.ssao.blur_fbo[0]);
		glDeleteTextures(1, &rt->effects.ssao.blur_red[0]);
		glDeleteFramebuffers(1, &rt->effects.ssao.blur_fbo[1]);
		glDeleteTextures(1, &rt->effects.ssao.blur_red[1]);
		for (int i = 0; i < rt->effects.ssao.depth_mipmap_fbos.size(); i++) {
			glDeleteFramebuffers(1, &rt->effects.ssao.depth_mipmap_fbos[i]);
		}

		rt->effects.ssao.depth_mipmap_fbos.clear();

		glDeleteTextures(1, &rt->effects.ssao.linear_depth);

		rt->effects.ssao.blur_fbo[0] = 0;
		rt->effects.ssao.blur_fbo[1] = 0;
	}

	if (rt->exposure.fbo) {
		glDeleteFramebuffers(1, &rt->exposure.fbo);
		glDeleteTextures(1, &rt->exposure.color);
		rt->exposure.fbo = 0;
	}

	Texture *tex = texture_owner.get(rt->texture);
	tex->alloc_height = 0;
	tex->alloc_width = 0;
	tex->width = 0;
	tex->height = 0;
	tex->active = false;

	if (rt->external.fbo != 0) {
		// free this
		glDeleteFramebuffers(1, &rt->external.fbo);

		// reset our texture back to the original
		tex->tex_id = rt->color;

		rt->external.fbo = 0;
		rt->external.color = 0;
		rt->external.depth = 0;
	}

	for (int i = 0; i < 2; i++) {
		if (rt->effects.mip_maps[i].color) {
			for (int j = 0; j < rt->effects.mip_maps[i].sizes.size(); j++) {
				glDeleteFramebuffers(1, &rt->effects.mip_maps[i].sizes[j].fbo);
			}

			glDeleteTextures(1, &rt->effects.mip_maps[i].color);
			rt->effects.mip_maps[i].sizes.clear();
			rt->effects.mip_maps[i].levels = 0;
			rt->effects.mip_maps[i].color = 0;
		}
	}

	/*
	if (rt->effects.screen_space_depth) {
		glDeleteTextures(1,&rt->effects.screen_space_depth);
		rt->effects.screen_space_depth=0;

	}
*/
}

void RasterizerStorageGLES3::_render_target_allocate(RenderTarget *rt) {
	if (rt->width <= 0 || rt->height <= 0) {
		return;
	}

	GLuint color_internal_format;
	GLuint color_format;
	GLuint color_type;
	Image::Format image_format;

	const bool hdr = rt->flags[RENDER_TARGET_HDR] && config.framebuffer_half_float_supported;

	if (!hdr || rt->flags[RENDER_TARGET_NO_3D]) {
		if (rt->flags[RENDER_TARGET_NO_3D_EFFECTS] && !rt->flags[RENDER_TARGET_TRANSPARENT]) {
			//if this is not used, linear colorspace looks pretty bad
			//this is the default mode used for mobile
			color_internal_format = GL_RGB10_A2;
			color_format = GL_RGBA;
			color_type = GL_UNSIGNED_INT_2_10_10_10_REV;
			image_format = Image::FORMAT_RGBA8;
		} else {
			color_internal_format = GL_RGBA8;
			color_format = GL_RGBA;
			color_type = GL_UNSIGNED_BYTE;
			image_format = Image::FORMAT_RGBA8;
		}
	} else {
		// HDR enabled.
		if (rt->flags[RENDER_TARGET_USE_32_BPC_DEPTH]) {
			// 32 bpc. Can be useful for advanced shaders, but should not be used
			// for general-purpose rendering as it's slower.
			color_internal_format = GL_RGBA32F;
			color_format = GL_RGBA;
			color_type = GL_FLOAT;
			image_format = Image::FORMAT_RGBAF;
		} else {
			// 16 bpc. This is the default HDR mode.
			color_internal_format = GL_RGBA16F;
			color_format = GL_RGBA;
			color_type = GL_HALF_FLOAT;
			image_format = Image::FORMAT_RGBAH;
		}
	}

	{
		/* FRONT FBO */

		gl_wrapper.gl_active_texture(GL_TEXTURE0);

		glGenFramebuffers(1, &rt->fbo);
		glBindFramebuffer(GL_FRAMEBUFFER, rt->fbo);

		glGenTextures(1, &rt->depth);
		glBindTexture(GL_TEXTURE_2D, rt->depth);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, rt->width, rt->height, 0,
				GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, nullptr);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		if (rt->external.depth == 0) {
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
					GL_TEXTURE_2D, rt->depth, 0);
		} else {
			// Use our external depth texture instead.
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
					GL_TEXTURE_2D, rt->external.depth, 0);
		}

		glGenTextures(1, &rt->color);
		glBindTexture(GL_TEXTURE_2D, rt->color);

		glTexImage2D(GL_TEXTURE_2D, 0, color_internal_format, rt->width, rt->height, 0, color_format, color_type, nullptr);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, rt->color, 0);

		GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
		glBindFramebuffer(GL_FRAMEBUFFER, RasterizerStorageGLES3::system_fbo);

		if (status != GL_FRAMEBUFFER_COMPLETE) {
			printf("framebuffer fail, status: %x\n", status);
		}

		ERR_FAIL_COND(status != GL_FRAMEBUFFER_COMPLETE);

		Texture *tex = texture_owner.get(rt->texture);
		tex->format = image_format;
		tex->gl_format_cache = color_format;
		tex->gl_type_cache = color_type;
		tex->gl_internal_format_cache = color_internal_format;
		tex->tex_id = rt->color;
		tex->width = rt->width;
		tex->alloc_width = rt->width;
		tex->height = rt->height;
		tex->alloc_height = rt->height;
		tex->active = true;

		texture_set_flags(rt->texture, tex->flags);
	}

	/* BACK FBO */

	if (!rt->flags[RENDER_TARGET_NO_3D] && (!rt->flags[RENDER_TARGET_NO_3D_EFFECTS] || rt->msaa != VS::VIEWPORT_MSAA_DISABLED)) {
		rt->buffers.active = true;

		static const int msaa_value[] = { 0, 2, 4, 8, 16, 4, 16 }; // MSAA_EXT_nX is a GLES2 temporary hack ignored in GLES3 for now...
		int msaa = msaa_value[rt->msaa];

		int max_samples = 0;
		glGetIntegerv(GL_MAX_SAMPLES, &max_samples);
		if (msaa > max_samples) {
			WARN_PRINT("MSAA must be <= GL_MAX_SAMPLES, falling-back to GL_MAX_SAMPLES = " + itos(max_samples));
			msaa = max_samples;
		}

		//regular fbo
		glGenFramebuffers(1, &rt->buffers.fbo);
		glBindFramebuffer(GL_FRAMEBUFFER, rt->buffers.fbo);

		glGenRenderbuffers(1, &rt->buffers.depth);
		glBindRenderbuffer(GL_RENDERBUFFER, rt->buffers.depth);
		if (msaa == 0) {
			glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, rt->width, rt->height);
		} else {
			glRenderbufferStorageMultisample(GL_RENDERBUFFER, msaa, GL_DEPTH_COMPONENT24, rt->width, rt->height);
		}

		glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rt->buffers.depth);

		glGenRenderbuffers(1, &rt->buffers.diffuse);
		glBindRenderbuffer(GL_RENDERBUFFER, rt->buffers.diffuse);

		if (msaa == 0) {
			glRenderbufferStorage(GL_RENDERBUFFER, color_internal_format, rt->width, rt->height);
		} else {
			glRenderbufferStorageMultisample(GL_RENDERBUFFER, msaa, color_internal_format, rt->width, rt->height);
		}

		glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, rt->buffers.diffuse);

		if (!rt->flags[RENDER_TARGET_NO_3D_EFFECTS]) {
			rt->buffers.effects_active = true;
			glGenRenderbuffers(1, &rt->buffers.specular);
			glBindRenderbuffer(GL_RENDERBUFFER, rt->buffers.specular);

			if (msaa == 0) {
				glRenderbufferStorage(GL_RENDERBUFFER, color_internal_format, rt->width, rt->height);
			} else {
				glRenderbufferStorageMultisample(GL_RENDERBUFFER, msaa, color_internal_format, rt->width, rt->height);
			}

			glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_RENDERBUFFER, rt->buffers.specular);

			glGenRenderbuffers(1, &rt->buffers.normal_rough);
			glBindRenderbuffer(GL_RENDERBUFFER, rt->buffers.normal_rough);

			if (msaa == 0) {
				glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, rt->width, rt->height);
			} else {
				glRenderbufferStorageMultisample(GL_RENDERBUFFER, msaa, GL_RGBA8, rt->width, rt->height);
			}

			glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_RENDERBUFFER, rt->buffers.normal_rough);

			glGenRenderbuffers(1, &rt->buffers.sss);
			glBindRenderbuffer(GL_RENDERBUFFER, rt->buffers.sss);

			if (msaa == 0) {
				glRenderbufferStorage(GL_RENDERBUFFER, GL_R8, rt->width, rt->height);
			} else {
				glRenderbufferStorageMultisample(GL_RENDERBUFFER, msaa, GL_R8, rt->width, rt->height);
			}

			glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT3, GL_RENDERBUFFER, rt->buffers.sss);

			GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
			glBindFramebuffer(GL_FRAMEBUFFER, RasterizerStorageGLES3::system_fbo);

			if (status != GL_FRAMEBUFFER_COMPLETE) {
				printf("err status: %x\n", status);
				_render_target_clear(rt);
				ERR_FAIL_COND(status != GL_FRAMEBUFFER_COMPLETE);
			}

			glBindRenderbuffer(GL_RENDERBUFFER, 0);

			// effect resolver

			glGenFramebuffers(1, &rt->buffers.effect_fbo);
			glBindFramebuffer(GL_FRAMEBUFFER, rt->buffers.effect_fbo);

			glGenTextures(1, &rt->buffers.effect);
			glBindTexture(GL_TEXTURE_2D, rt->buffers.effect);
			glTexImage2D(GL_TEXTURE_2D, 0, color_internal_format, rt->width, rt->height, 0,
					color_format, color_type, nullptr);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
					GL_TEXTURE_2D, rt->buffers.effect, 0);

			status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
			glBindFramebuffer(GL_FRAMEBUFFER, RasterizerStorageGLES3::system_fbo);

			if (status != GL_FRAMEBUFFER_COMPLETE) {
				printf("err status: %x\n", status);
				_render_target_clear(rt);
				ERR_FAIL_COND(status != GL_FRAMEBUFFER_COMPLETE);
			}

			///////////////// ssao

			//AO strength textures
			for (int i = 0; i < 2; i++) {
				glGenFramebuffers(1, &rt->effects.ssao.blur_fbo[i]);
				glBindFramebuffer(GL_FRAMEBUFFER, rt->effects.ssao.blur_fbo[i]);
				glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
						GL_TEXTURE_2D, rt->depth, 0);

				glGenTextures(1, &rt->effects.ssao.blur_red[i]);
				glBindTexture(GL_TEXTURE_2D, rt->effects.ssao.blur_red[i]);

				glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, rt->width, rt->height, 0, GL_RED, GL_UNSIGNED_BYTE, nullptr);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
				glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
				glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

				glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, rt->effects.ssao.blur_red[i], 0);

				status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
				if (status != GL_FRAMEBUFFER_COMPLETE) {
					_render_target_clear(rt);
					ERR_FAIL_COND(status != GL_FRAMEBUFFER_COMPLETE);
				}
			}
			//5 mip levels for depth texture, but base is read separately

			glGenTextures(1, &rt->effects.ssao.linear_depth);
			glBindTexture(GL_TEXTURE_2D, rt->effects.ssao.linear_depth);

			int ssao_w = rt->width / 2;
			int ssao_h = rt->height / 2;

			for (int i = 0; i < 4; i++) { //5, but 4 mips, base is read directly to save bw

				glTexImage2D(GL_TEXTURE_2D, i, GL_R16UI, ssao_w, ssao_h, 0, GL_RED_INTEGER, GL_UNSIGNED_SHORT, nullptr);
				ssao_w >>= 1;
				ssao_h >>= 1;
			}

			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 3);

			for (int i = 0; i < 4; i++) { //5, but 4 mips, base is read directly to save bw

				GLuint fbo;
				glGenFramebuffers(1, &fbo);
				glBindFramebuffer(GL_FRAMEBUFFER, fbo);
				glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, rt->effects.ssao.linear_depth, i);
				rt->effects.ssao.depth_mipmap_fbos.push_back(fbo);
			}

			//////Exposure

			glGenFramebuffers(1, &rt->exposure.fbo);
			glBindFramebuffer(GL_FRAMEBUFFER, rt->exposure.fbo);

			glGenTextures(1, &rt->exposure.color);
			glBindTexture(GL_TEXTURE_2D, rt->exposure.color);
			if (config.framebuffer_float_supported) {
				glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, 1, 1, 0, GL_RED, GL_FLOAT, nullptr);
			} else if (config.framebuffer_half_float_supported) {
				glTexImage2D(GL_TEXTURE_2D, 0, GL_R16F, 1, 1, 0, GL_RED, GL_HALF_FLOAT, nullptr);
			} else {
				glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB10_A2, 1, 1, 0, GL_RGBA, GL_UNSIGNED_INT_2_10_10_10_REV, nullptr);
			}

			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, rt->exposure.color, 0);

			status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
			if (status != GL_FRAMEBUFFER_COMPLETE) {
				_render_target_clear(rt);
				ERR_FAIL_COND(status != GL_FRAMEBUFFER_COMPLETE);
			}
		} else {
			rt->buffers.effects_active = false;
		}
	} else {
		rt->buffers.active = false;
		rt->buffers.effects_active = true;
	}

	if (!rt->flags[RENDER_TARGET_NO_SAMPLING] && rt->width >= 2 && rt->height >= 2) {
		for (int i = 0; i < 2; i++) {
			ERR_FAIL_COND(rt->effects.mip_maps[i].sizes.size());
			int w = rt->width;
			int h = rt->height;

			if (i > 0) {
				w >>= 1;
				h >>= 1;
			}

			glGenTextures(1, &rt->effects.mip_maps[i].color);
			glBindTexture(GL_TEXTURE_2D, rt->effects.mip_maps[i].color);

			int level = 0;
			int fb_w = w;
			int fb_h = h;

			while (true) {
				RenderTarget::Effects::MipMaps::Size mm;
				mm.width = w;
				mm.height = h;
				rt->effects.mip_maps[i].sizes.push_back(mm);

				w >>= 1;
				h >>= 1;

				if (w < 2 || h < 2) {
					break;
				}

				level++;
			}

			glTexStorage2DCustom(GL_TEXTURE_2D, level + 1, color_internal_format, fb_w, fb_h, color_format, color_type);

			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, level);
			glDisable(GL_SCISSOR_TEST);
			glColorMask(1, 1, 1, 1);
			if (!rt->buffers.active) {
				glDepthMask(GL_TRUE);
			}

			for (int j = 0; j < rt->effects.mip_maps[i].sizes.size(); j++) {
				RenderTarget::Effects::MipMaps::Size &mm = rt->effects.mip_maps[i].sizes.write[j];

				glGenFramebuffers(1, &mm.fbo);
				glBindFramebuffer(GL_FRAMEBUFFER, mm.fbo);
				glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, rt->effects.mip_maps[i].color, j);
				bool used_depth = false;
				if (j == 0 && i == 0) { //use always
					glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, rt->depth, 0);
					used_depth = true;
				}

				GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
				if (status != GL_FRAMEBUFFER_COMPLETE) {
					_render_target_clear(rt);
					ERR_FAIL_COND(status != GL_FRAMEBUFFER_COMPLETE);
				}

				float zero[4] = { 1, 0, 1, 0 };
				glViewport(0, 0, rt->effects.mip_maps[i].sizes[j].width, rt->effects.mip_maps[i].sizes[j].height);
				glClearBufferfv(GL_COLOR, 0, zero);
				if (used_depth) {
					glClearDepth(1.0);
					glClear(GL_DEPTH_BUFFER_BIT);
				}
			}

			glBindFramebuffer(GL_FRAMEBUFFER, RasterizerStorageGLES3::system_fbo);
			rt->effects.mip_maps[i].levels = level;

			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
			//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		}
	}
}

RID RasterizerStorageGLES3::render_target_create() {
	RenderTarget *rt = memnew(RenderTarget);

	Texture *t = memnew(Texture);

	t->type = VS::TEXTURE_TYPE_2D;
	t->flags = 0;
	t->width = 0;
	t->height = 0;
	t->alloc_height = 0;
	t->alloc_width = 0;
	t->format = Image::FORMAT_R8;
	t->target = GL_TEXTURE_2D;
	t->gl_format_cache = 0;
	t->gl_internal_format_cache = 0;
	t->gl_type_cache = 0;
	t->data_size = 0;
	t->compressed = false;
	t->srgb = false;
	t->total_data_size = 0;
	t->ignore_mipmaps = false;
	t->mipmaps = 1;
	t->active = true;
	t->tex_id = 0;
	t->render_target = rt;

	rt->texture = texture_owner.make_rid(t);

	return render_target_owner.make_rid(rt);
}

void RasterizerStorageGLES3::render_target_set_position(RID p_render_target, int p_x, int p_y) {
	//only used in GLES2
}

void RasterizerStorageGLES3::render_target_set_size(RID p_render_target, int p_width, int p_height) {
	RenderTarget *rt = render_target_owner.getornull(p_render_target);
	ERR_FAIL_COND(!rt);

	if (rt->width == p_width && rt->height == p_height) {
		return;
	}

	_render_target_clear(rt);
	rt->width = p_width;
	rt->height = p_height;
	_render_target_allocate(rt);
}

RID RasterizerStorageGLES3::render_target_get_texture(RID p_render_target) const {
	RenderTarget *rt = render_target_owner.getornull(p_render_target);
	ERR_FAIL_COND_V(!rt, RID());

	return rt->texture;
}

uint32_t RasterizerStorageGLES3::render_target_get_depth_texture_id(RID p_render_target) const {
	RenderTarget *rt = render_target_owner.getornull(p_render_target);
	ERR_FAIL_COND_V(!rt, 0);

	if (rt->external.depth == 0) {
		return rt->depth;
	} else {
		return rt->external.depth;
	}
}

void RasterizerStorageGLES3::render_target_set_external_texture(RID p_render_target, unsigned int p_texture_id, unsigned int p_depth_id) {
	RenderTarget *rt = render_target_owner.getornull(p_render_target);
	ERR_FAIL_COND(!rt);

	if (p_texture_id == 0) {
		if (rt->external.fbo != 0) {
			// return to our original depth buffer
			if (rt->external.depth != 0 && rt->fbo != 0) {
				glBindFramebuffer(GL_FRAMEBUFFER, rt->fbo);
				glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, rt->depth, 0);
				glBindFramebuffer(GL_FRAMEBUFFER, RasterizerStorageGLES3::system_fbo);
			}

			// free this
			glDeleteFramebuffers(1, &rt->external.fbo);

			// reset our texture back to the original
			Texture *t = texture_owner.get(rt->texture);
			t->tex_id = rt->color;
			t->width = rt->width;
			t->alloc_width = rt->width;
			t->height = rt->height;
			t->alloc_height = rt->height;

			rt->external.fbo = 0;
			rt->external.color = 0;
			rt->external.depth = 0;
		}
	} else {
		if (rt->external.fbo == 0) {
			// create our fbo
			glGenFramebuffers(1, &rt->external.fbo);
		}

		// bind our frame buffer
		glBindFramebuffer(GL_FRAMEBUFFER, rt->external.fbo);

		rt->external.color = p_texture_id;

		// Set our texture to the new image, note that we expect formats to be the same (or compatible) so we don't change those
		Texture *t = texture_owner.get(rt->texture);
		t->tex_id = p_texture_id;
		t->width = rt->width;
		t->height = rt->height;
		t->alloc_height = rt->width;
		t->alloc_width = rt->height;

		// set our texture as the destination for our framebuffer
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, p_texture_id, 0);

		// check status
		GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
		if (status != GL_FRAMEBUFFER_COMPLETE) {
			printf("framebuffer fail, status: %x\n", status);
		}

		// Copy our depth texture id,
		// if it's 0 then we don't use it,
		// else we use it instead of our normal depth buffer
		rt->external.depth = p_depth_id;

		if (rt->external.depth != 0 && rt->fbo != 0) {
			// Use our external depth texture instead.
			glBindFramebuffer(GL_FRAMEBUFFER, rt->fbo);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, rt->external.depth, 0);

			// check status
			GLenum status2 = glCheckFramebufferStatus(GL_FRAMEBUFFER);
			if (status2 != GL_FRAMEBUFFER_COMPLETE) {
				printf("framebuffer fail, status: %x\n", status2);
			}
		}

		// and unbind
		glBindFramebuffer(GL_FRAMEBUFFER, RasterizerStorageGLES3::system_fbo);

		ERR_FAIL_COND(status != GL_FRAMEBUFFER_COMPLETE);
	}
}

void RasterizerStorageGLES3::render_target_set_flag(RID p_render_target, RenderTargetFlags p_flag, bool p_value) {
	RenderTarget *rt = render_target_owner.getornull(p_render_target);
	ERR_FAIL_COND(!rt);

	rt->flags[p_flag] = p_value;

	switch (p_flag) {
		case RENDER_TARGET_HDR:
		case RENDER_TARGET_USE_32_BPC_DEPTH:
		case RENDER_TARGET_NO_3D:
		case RENDER_TARGET_NO_SAMPLING:
		case RENDER_TARGET_NO_3D_EFFECTS: {
			//must reset for these formats
			_render_target_clear(rt);
			_render_target_allocate(rt);

		} break;
		default: {
		}
	}
}
bool RasterizerStorageGLES3::render_target_was_used(RID p_render_target) {
	RenderTarget *rt = render_target_owner.getornull(p_render_target);
	ERR_FAIL_COND_V(!rt, false);

	return rt->used_in_frame;
}

void RasterizerStorageGLES3::render_target_clear_used(RID p_render_target) {
	RenderTarget *rt = render_target_owner.getornull(p_render_target);
	ERR_FAIL_COND(!rt);

	rt->used_in_frame = false;
}

void RasterizerStorageGLES3::render_target_set_msaa(RID p_render_target, VS::ViewportMSAA p_msaa) {
	RenderTarget *rt = render_target_owner.getornull(p_render_target);
	ERR_FAIL_COND(!rt);

	if (rt->msaa == p_msaa) {
		return;
	}

	_render_target_clear(rt);
	rt->msaa = p_msaa;
	_render_target_allocate(rt);
}

void RasterizerStorageGLES3::render_target_set_use_fxaa(RID p_render_target, bool p_fxaa) {
	RenderTarget *rt = render_target_owner.getornull(p_render_target);
	ERR_FAIL_COND(!rt);

	rt->use_fxaa = p_fxaa;
}

void RasterizerStorageGLES3::render_target_set_use_debanding(RID p_render_target, bool p_debanding) {
	RenderTarget *rt = render_target_owner.getornull(p_render_target);
	ERR_FAIL_COND(!rt);

	rt->use_debanding = p_debanding;
}

void RasterizerStorageGLES3::render_target_set_sharpen_intensity(RID p_render_target, float p_intensity) {
	RenderTarget *rt = render_target_owner.getornull(p_render_target);
	ERR_FAIL_COND(!rt);

	rt->sharpen_intensity = p_intensity;
}

/* CANVAS SHADOW */

RID RasterizerStorageGLES3::canvas_light_shadow_buffer_create(int p_width) {
	CanvasLightShadow *cls = memnew(CanvasLightShadow);
	if (p_width > config.max_texture_size) {
		p_width = config.max_texture_size;
	}

	cls->size = p_width;
	cls->height = 16;

	gl_wrapper.gl_active_texture(GL_TEXTURE0);

	glGenFramebuffers(1, &cls->fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, cls->fbo);

	glGenRenderbuffers(1, &cls->depth);
	glBindRenderbuffer(GL_RENDERBUFFER, cls->depth);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, cls->size, cls->height);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, cls->depth);
	glBindRenderbuffer(GL_RENDERBUFFER, 0);

	glGenTextures(1, &cls->distance);
	glBindTexture(GL_TEXTURE_2D, cls->distance);
	if (config.use_rgba_2d_shadows) {
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, cls->size, cls->height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
	} else {
		glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, cls->size, cls->height, 0, GL_RED, GL_FLOAT, nullptr);
	}

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, cls->distance, 0);

	GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	//printf("errnum: %x\n",status);
	glBindFramebuffer(GL_FRAMEBUFFER, RasterizerStorageGLES3::system_fbo);

	if (status != GL_FRAMEBUFFER_COMPLETE) {
		memdelete(cls);
		ERR_FAIL_COND_V(status != GL_FRAMEBUFFER_COMPLETE, RID());
	}

	return canvas_light_shadow_owner.make_rid(cls);
}

/* LIGHT SHADOW MAPPING */

RID RasterizerStorageGLES3::canvas_light_occluder_create() {
	CanvasOccluder *co = memnew(CanvasOccluder);
	co->index_id = 0;
	co->vertex_id = 0;
	co->len = 0;
	glGenVertexArrays(1, &co->array_id);

	return canvas_occluder_owner.make_rid(co);
}

void RasterizerStorageGLES3::canvas_light_occluder_set_polylines(RID p_occluder, const PoolVector<Vector2> &p_lines) {
	CanvasOccluder *co = canvas_occluder_owner.get(p_occluder);
	ERR_FAIL_COND(!co);

	co->lines = p_lines;

	if (p_lines.size() != co->len) {
		if (co->index_id) {
			glDeleteBuffers(1, &co->index_id);
		}
		if (co->vertex_id) {
			glDeleteBuffers(1, &co->vertex_id);
		}

		co->index_id = 0;
		co->vertex_id = 0;
		co->len = 0;
	}

	if (p_lines.size()) {
		PoolVector<float> geometry;
		PoolVector<uint16_t> indices;
		int lc = p_lines.size();

		geometry.resize(lc * 6);
		indices.resize(lc * 3);

		PoolVector<float>::Write vw = geometry.write();
		PoolVector<uint16_t>::Write iw = indices.write();

		PoolVector<Vector2>::Read lr = p_lines.read();

		const int POLY_HEIGHT = 16384;

		for (int i = 0; i < lc / 2; i++) {
			vw[i * 12 + 0] = lr[i * 2 + 0].x;
			vw[i * 12 + 1] = lr[i * 2 + 0].y;
			vw[i * 12 + 2] = POLY_HEIGHT;

			vw[i * 12 + 3] = lr[i * 2 + 1].x;
			vw[i * 12 + 4] = lr[i * 2 + 1].y;
			vw[i * 12 + 5] = POLY_HEIGHT;

			vw[i * 12 + 6] = lr[i * 2 + 1].x;
			vw[i * 12 + 7] = lr[i * 2 + 1].y;
			vw[i * 12 + 8] = -POLY_HEIGHT;

			vw[i * 12 + 9] = lr[i * 2 + 0].x;
			vw[i * 12 + 10] = lr[i * 2 + 0].y;
			vw[i * 12 + 11] = -POLY_HEIGHT;

			iw[i * 6 + 0] = i * 4 + 0;
			iw[i * 6 + 1] = i * 4 + 1;
			iw[i * 6 + 2] = i * 4 + 2;

			iw[i * 6 + 3] = i * 4 + 2;
			iw[i * 6 + 4] = i * 4 + 3;
			iw[i * 6 + 5] = i * 4 + 0;
		}

		//if same buffer len is being set, just use BufferSubData to avoid a pipeline flush

		if (!co->vertex_id) {
			glGenBuffers(1, &co->vertex_id);
			glBindBuffer(GL_ARRAY_BUFFER, co->vertex_id);
			glBufferData(GL_ARRAY_BUFFER, lc * 6 * sizeof(real_t), vw.ptr(), GL_STATIC_DRAW);
		} else {
			glBindBuffer(GL_ARRAY_BUFFER, co->vertex_id);
			glBufferSubData(GL_ARRAY_BUFFER, 0, lc * 6 * sizeof(real_t), vw.ptr());
		}

		glBindBuffer(GL_ARRAY_BUFFER, 0); //unbind

		if (!co->index_id) {
			glGenBuffers(1, &co->index_id);
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, co->index_id);
			glBufferData(GL_ELEMENT_ARRAY_BUFFER, lc * 3 * sizeof(uint16_t), iw.ptr(), GL_DYNAMIC_DRAW);
		} else {
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, co->index_id);
			glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, lc * 3 * sizeof(uint16_t), iw.ptr());
		}

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0); //unbind

		co->len = lc;
		glBindVertexArray(co->array_id);
		glBindBuffer(GL_ARRAY_BUFFER, co->vertex_id);
		glEnableVertexAttribArray(VS::ARRAY_VERTEX);
		glVertexAttribPointer(VS::ARRAY_VERTEX, 3, GL_FLOAT, false, 0, nullptr);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, co->index_id);
		glBindVertexArray(0);
	}
}

VS::InstanceType RasterizerStorageGLES3::get_base_type(RID p_rid) const {
	if (mesh_owner.owns(p_rid)) {
		return VS::INSTANCE_MESH;
	}

	if (multimesh_owner.owns(p_rid)) {
		return VS::INSTANCE_MULTIMESH;
	}

	if (immediate_owner.owns(p_rid)) {
		return VS::INSTANCE_IMMEDIATE;
	}

	if (particles_owner.owns(p_rid)) {
		return VS::INSTANCE_PARTICLES;
	}

	if (light_owner.owns(p_rid)) {
		return VS::INSTANCE_LIGHT;
	}

	if (reflection_probe_owner.owns(p_rid)) {
		return VS::INSTANCE_REFLECTION_PROBE;
	}

	if (gi_probe_owner.owns(p_rid)) {
		return VS::INSTANCE_GI_PROBE;
	}

	if (lightmap_capture_data_owner.owns(p_rid)) {
		return VS::INSTANCE_LIGHTMAP_CAPTURE;
	}

	return VS::INSTANCE_NONE;
}

bool RasterizerStorageGLES3::free(RID p_rid) {
	if (render_target_owner.owns(p_rid)) {
		RenderTarget *rt = render_target_owner.getornull(p_rid);
		_render_target_clear(rt);
		Texture *t = texture_owner.get(rt->texture);
		texture_owner.free(rt->texture);
		memdelete(t);
		render_target_owner.free(p_rid);
		memdelete(rt);

	} else if (texture_owner.owns(p_rid)) {
		// delete the texture
		Texture *texture = texture_owner.get(p_rid);
		ERR_FAIL_COND_V(texture->render_target, true); //can't free the render target texture, dude
		info.texture_mem -= texture->total_data_size;
		texture_owner.free(p_rid);
		memdelete(texture);

	} else if (sky_owner.owns(p_rid)) {
		// delete the sky
		Sky *sky = sky_owner.get(p_rid);
		sky_set_texture(p_rid, RID(), 256);
		sky_owner.free(p_rid);
		memdelete(sky);

	} else if (shader_owner.owns(p_rid)) {
		// delete the texture
		Shader *shader = shader_owner.get(p_rid);

		if (shader->shader && shader->custom_code_id) {
			shader->shader->free_custom_shader(shader->custom_code_id);
		}

		if (shader->dirty_list.in_list()) {
			_shader_dirty_list.remove(&shader->dirty_list);
		}

		while (shader->materials.first()) {
			Material *mat = shader->materials.first()->self();

			mat->shader = nullptr;
			_material_make_dirty(mat);

			shader->materials.remove(shader->materials.first());
		}

		//material_shader.free_custom_shader(shader->custom_code_id);
		shader_owner.free(p_rid);
		memdelete(shader);

	} else if (material_owner.owns(p_rid)) {
		// delete the texture
		Material *material = material_owner.get(p_rid);

		if (material->shader) {
			material->shader->materials.remove(&material->list);
		}

		if (material->ubo_id) {
			glDeleteBuffers(1, &material->ubo_id);
		}

		//remove from owners
		for (Map<Geometry *, int>::Element *E = material->geometry_owners.front(); E; E = E->next()) {
			Geometry *g = E->key();
			g->material = RID();
		}
		for (Map<RasterizerScene::InstanceBase *, int>::Element *E = material->instance_owners.front(); E; E = E->next()) {
			RasterizerScene::InstanceBase *ins = E->key();

			if (ins->material_override == p_rid) {
				ins->material_override = RID();
			}

			if (ins->material_overlay == p_rid) {
				ins->material_overlay = RID();
			}

			for (int i = 0; i < ins->materials.size(); i++) {
				if (ins->materials[i] == p_rid) {
					ins->materials.write[i] = RID();
				}
			}
		}

		material_owner.free(p_rid);
		memdelete(material);

	} else if (skeleton_owner.owns(p_rid)) {
		// delete the texture
		Skeleton *skeleton = skeleton_owner.get(p_rid);
		if (skeleton->update_list.in_list()) {
			skeleton_update_list.remove(&skeleton->update_list);
		}

		for (Set<RasterizerScene::InstanceBase *>::Element *E = skeleton->instances.front(); E; E = E->next()) {
			E->get()->skeleton = RID();
		}

		skeleton_allocate(p_rid, 0, false);

		glDeleteTextures(1, &skeleton->texture);
		skeleton_owner.free(p_rid);
		memdelete(skeleton);

	} else if (mesh_owner.owns(p_rid)) {
		// delete the texture
		Mesh *mesh = mesh_owner.get(p_rid);
		mesh->instance_remove_deps();
		mesh_clear(p_rid);

		while (mesh->multimeshes.first()) {
			MultiMesh *multimesh = mesh->multimeshes.first()->self();
			multimesh->mesh = RID();
			multimesh->dirty_aabb = true;
			mesh->multimeshes.remove(mesh->multimeshes.first());

			if (!multimesh->update_list.in_list()) {
				multimesh_update_list.add(&multimesh->update_list);
			}
		}

		mesh_owner.free(p_rid);
		memdelete(mesh);

	} else if (multimesh_owner.owns(p_rid)) {
		// remove from interpolator
		_interpolation_data.notify_free_multimesh(p_rid);

		MultiMesh *multimesh = multimesh_owner.get(p_rid);

		// remove any references in linked canvas items
		int num_linked = multimesh->linked_canvas_items.size();
		for (int n = 0; n < num_linked; n++) {
			const RID &rid = multimesh->linked_canvas_items[n];
			VSG::canvas->_canvas_item_remove_references(rid, p_rid);
		}

		// delete the texture
		multimesh->instance_remove_deps();

		if (multimesh->mesh.is_valid()) {
			Mesh *mesh = mesh_owner.getornull(multimesh->mesh);
			if (mesh) {
				mesh->multimeshes.remove(&multimesh->mesh_list);
			}
		}

		multimesh_allocate(p_rid, 0, VS::MULTIMESH_TRANSFORM_2D, VS::MULTIMESH_COLOR_NONE); //frees multimesh
		update_dirty_multimeshes();

		multimesh_owner.free(p_rid);
		memdelete(multimesh);
	} else if (immediate_owner.owns(p_rid)) {
		Immediate *immediate = immediate_owner.get(p_rid);
		immediate->instance_remove_deps();

		immediate_owner.free(p_rid);
		memdelete(immediate);
	} else if (light_owner.owns(p_rid)) {
		// delete the texture
		Light *light = light_owner.get(p_rid);
		light->instance_remove_deps();

		light_owner.free(p_rid);
		memdelete(light);

	} else if (reflection_probe_owner.owns(p_rid)) {
		// delete the texture
		ReflectionProbe *reflection_probe = reflection_probe_owner.get(p_rid);
		reflection_probe->instance_remove_deps();

		reflection_probe_owner.free(p_rid);
		memdelete(reflection_probe);

	} else if (gi_probe_owner.owns(p_rid)) {
		// delete the texture
		GIProbe *gi_probe = gi_probe_owner.get(p_rid);
		gi_probe->instance_remove_deps();

		gi_probe_owner.free(p_rid);
		memdelete(gi_probe);
	} else if (gi_probe_data_owner.owns(p_rid)) {
		// delete the texture
		GIProbeData *gi_probe_data = gi_probe_data_owner.get(p_rid);

		glDeleteTextures(1, &gi_probe_data->tex_id);
		gi_probe_data_owner.free(p_rid);
		memdelete(gi_probe_data);
	} else if (lightmap_capture_data_owner.owns(p_rid)) {
		// delete the texture
		LightmapCapture *lightmap_capture = lightmap_capture_data_owner.get(p_rid);
		lightmap_capture->instance_remove_deps();

		lightmap_capture_data_owner.free(p_rid);
		memdelete(lightmap_capture);

	} else if (canvas_occluder_owner.owns(p_rid)) {
		CanvasOccluder *co = canvas_occluder_owner.get(p_rid);
		if (co->index_id) {
			glDeleteBuffers(1, &co->index_id);
		}
		if (co->vertex_id) {
			glDeleteBuffers(1, &co->vertex_id);
		}

		glDeleteVertexArrays(1, &co->array_id);

		canvas_occluder_owner.free(p_rid);
		memdelete(co);

	} else if (canvas_light_shadow_owner.owns(p_rid)) {
		CanvasLightShadow *cls = canvas_light_shadow_owner.get(p_rid);
		glDeleteFramebuffers(1, &cls->fbo);
		glDeleteRenderbuffers(1, &cls->depth);
		glDeleteTextures(1, &cls->distance);
		canvas_light_shadow_owner.free(p_rid);
		memdelete(cls);
	} else if (particles_owner.owns(p_rid)) {
		Particles *particles = particles_owner.get(p_rid);
		particles->instance_remove_deps();
		particles_owner.free(p_rid);
		memdelete(particles);
	} else {
		return false;
	}

	return true;
}

bool RasterizerStorageGLES3::has_os_feature(const String &p_feature) const {
	if (p_feature == "bptc") {
		return config.bptc_supported;
	}

	if (p_feature == "s3tc") {
		return config.s3tc_supported;
	}

	if (p_feature == "etc") {
		return config.etc_supported;
	}

	if (p_feature == "etc2") {
		return config.etc2_supported;
	}

	if (p_feature == "pvrtc") {
		return config.pvrtc_supported;
	}

	return false;
}

////////////////////////////////////////////

void RasterizerStorageGLES3::set_debug_generate_wireframes(bool p_generate) {
	config.generate_wireframes = p_generate;
}

void RasterizerStorageGLES3::render_info_begin_capture() {
	info.snap = info.render;
}

void RasterizerStorageGLES3::render_info_end_capture() {
	info.snap.object_count = info.render.object_count - info.snap.object_count;
	info.snap.draw_call_count = info.render.draw_call_count - info.snap.draw_call_count;
	info.snap.material_switch_count = info.render.material_switch_count - info.snap.material_switch_count;
	info.snap.surface_switch_count = info.render.surface_switch_count - info.snap.surface_switch_count;
	info.snap.shader_rebind_count = info.render.shader_rebind_count - info.snap.shader_rebind_count;
	info.snap.shader_compiles_started_count = info.render.shader_compiles_started_count - info.snap.shader_compiles_started_count;
	info.snap.shader_compiles_in_progress_count = info.render.shader_compiles_in_progress_count - info.snap.shader_compiles_in_progress_count;
	info.snap.vertices_count = info.render.vertices_count - info.snap.vertices_count;
	info.snap._2d_item_count = info.render._2d_item_count - info.snap._2d_item_count;
	info.snap._2d_draw_call_count = info.render._2d_draw_call_count - info.snap._2d_draw_call_count;
}

int RasterizerStorageGLES3::get_captured_render_info(VS::RenderInfo p_info) {
	switch (p_info) {
		case VS::INFO_OBJECTS_IN_FRAME: {
			return info.snap.object_count;
		} break;
		case VS::INFO_VERTICES_IN_FRAME: {
			return info.snap.vertices_count;
		} break;
		case VS::INFO_MATERIAL_CHANGES_IN_FRAME: {
			return info.snap.material_switch_count;
		} break;
		case VS::INFO_SHADER_CHANGES_IN_FRAME: {
			return info.snap.shader_rebind_count;
		} break;
		case VS::INFO_SHADER_COMPILES_IN_FRAME: {
			return info.snap.shader_compiles_in_progress_count;
		} break;
		case VS::INFO_SURFACE_CHANGES_IN_FRAME: {
			return info.snap.surface_switch_count;
		} break;
		case VS::INFO_DRAW_CALLS_IN_FRAME: {
			return info.snap.draw_call_count;
		} break;
		case VS::INFO_2D_ITEMS_IN_FRAME: {
			return info.snap._2d_item_count;
		} break;
		case VS::INFO_2D_DRAW_CALLS_IN_FRAME: {
			return info.snap._2d_draw_call_count;
		} break;
		default: {
			return get_render_info(p_info);
		}
	}
}

uint64_t RasterizerStorageGLES3::get_render_info(VS::RenderInfo p_info) {
	switch (p_info) {
		case VS::INFO_OBJECTS_IN_FRAME:
			return info.render_final.object_count;
		case VS::INFO_VERTICES_IN_FRAME:
			return info.render_final.vertices_count;
		case VS::INFO_MATERIAL_CHANGES_IN_FRAME:
			return info.render_final.material_switch_count;
		case VS::INFO_SHADER_CHANGES_IN_FRAME:
			return info.render_final.shader_rebind_count;
		case VS::INFO_SHADER_COMPILES_IN_FRAME:
			return info.render.shader_compiles_in_progress_count;
		case VS::INFO_SURFACE_CHANGES_IN_FRAME:
			return info.render_final.surface_switch_count;
		case VS::INFO_DRAW_CALLS_IN_FRAME:
			return info.render_final.draw_call_count;
		case VS::INFO_2D_ITEMS_IN_FRAME:
			return info.render_final._2d_item_count;
		case VS::INFO_2D_DRAW_CALLS_IN_FRAME:
			return info.render_final._2d_draw_call_count;
		case VS::INFO_USAGE_VIDEO_MEM_TOTAL:
			return 0; //no idea
		case VS::INFO_VIDEO_MEM_USED:
			return info.vertex_mem + info.texture_mem;
		case VS::INFO_TEXTURE_MEM_USED:
			return info.texture_mem;
		case VS::INFO_VERTEX_MEM_USED:
			return info.vertex_mem;
		default:
			return 0; //no idea either
	}
}

String RasterizerStorageGLES3::get_video_adapter_name() const {
	return (const char *)glGetString(GL_RENDERER);
}

String RasterizerStorageGLES3::get_video_adapter_vendor() const {
	return (const char *)glGetString(GL_VENDOR);
}

void RasterizerStorageGLES3::initialize() {
	RasterizerStorageGLES3::system_fbo = 0;

	//// extensions config
	///

	{
		int max_extensions = safe_gl_get_integer(GL_NUM_EXTENSIONS);
		for (int i = 0; i < max_extensions; i++) {
			const GLubyte *s = glGetStringi(GL_EXTENSIONS, i);
			if (!s) {
				break;
			}
			config.extensions.insert((const char *)s);
		}
	}

	config.shrink_textures_x2 = false;
	config.use_fast_texture_filter = int(ProjectSettings::get_singleton()->get("rendering/quality/filters/use_nearest_mipmap_filter"));

	// Cap max_texture_image_units as we don't need large numbers of units,
	// just in case an implementation provides a large number, as we want to keep
	// the table in gl_wrapper small.
	config.max_texture_image_units = safe_gl_get_integer(GL_MAX_TEXTURE_IMAGE_UNITS, Config::max_desired_texture_image_units);

	// Initialize GLWrapper early on, as required for any calls to glActiveTexture.
	gl_wrapper.initialize(config.max_texture_image_units);

	config.etc_supported = config.extensions.has("GL_OES_compressed_ETC1_RGB8_texture");
	config.latc_supported = config.extensions.has("GL_EXT_texture_compression_latc");
	config.bptc_supported = config.extensions.has("GL_ARB_texture_compression_bptc");
#ifdef GLES_OVER_GL
	config.etc2_supported = false;
	config.s3tc_supported = true;
	config.rgtc_supported = true; //RGTC - core since OpenGL version 3.0
	config.texture_float_linear_supported = true;
	config.framebuffer_float_supported = true;
	config.framebuffer_half_float_supported = true;
#else
	config.etc2_supported = true;
	config.s3tc_supported = config.extensions.has("GL_EXT_texture_compression_dxt1") || config.extensions.has("GL_EXT_texture_compression_s3tc") || config.extensions.has("WEBGL_compressed_texture_s3tc");
	config.rgtc_supported = config.extensions.has("GL_EXT_texture_compression_rgtc") || config.extensions.has("GL_ARB_texture_compression_rgtc") || config.extensions.has("EXT_texture_compression_rgtc");
	config.texture_float_linear_supported = config.extensions.has("GL_OES_texture_float_linear");
	config.framebuffer_float_supported = config.extensions.has("GL_EXT_color_buffer_float");
	config.framebuffer_half_float_supported = config.extensions.has("GL_EXT_color_buffer_half_float") || config.framebuffer_float_supported;

	// If the desktop build is using S3TC, and you export / run from the IDE for android, if the device supports
	// S3TC it will crash trying to load these textures, as they are not exported in the APK. This is a simple way
	// to prevent Android devices trying to load S3TC, by faking lack of hardware support.
#if defined(ANDROID_ENABLED) || defined(IPHONE_ENABLED)
	config.s3tc_supported = false;
#endif
#endif

	// not yet detected on GLES3 (is this mandated?)
	config.support_npot_repeat_mipmap = true;

	config.pvrtc_supported = config.extensions.has("GL_IMG_texture_compression_pvrtc");
	config.srgb_decode_supported = config.extensions.has("GL_EXT_texture_sRGB_decode");

	config.anisotropic_level = 1.0;
	config.use_anisotropic_filter = config.extensions.has("GL_EXT_texture_filter_anisotropic");
	if (config.use_anisotropic_filter) {
		glGetFloatv(_GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &config.anisotropic_level);
		config.anisotropic_level = MIN(int(ProjectSettings::get_singleton()->get("rendering/quality/filters/anisotropic_filter_level")), config.anisotropic_level);
	}

#ifdef GLES_OVER_GL
	config.program_binary_supported = GLAD_GL_ARB_get_program_binary;
	config.parallel_shader_compile_supported = GLAD_GL_ARB_parallel_shader_compile || GLAD_GL_KHR_parallel_shader_compile;
#else
#ifdef JAVASCRIPT_ENABLED
	config.program_binary_supported = false;
#else
	config.program_binary_supported = true;
#endif
	config.parallel_shader_compile_supported = config.extensions.has("GL_KHR_parallel_shader_compile") || config.extensions.has("GL_ARB_parallel_shader_compile");
#endif

	int compilation_mode = 0;
	if (!(Engine::get_singleton()->is_editor_hint() || Main::is_project_manager())) {
		compilation_mode = ProjectSettings::get_singleton()->get("rendering/gles3/shaders/shader_compilation_mode");
	}
	config.async_compilation_enabled = compilation_mode >= 1;
	config.shader_cache_enabled = compilation_mode == 2;

	if (config.async_compilation_enabled) {
		ShaderGLES3::max_simultaneous_compiles = MAX(1, (int)ProjectSettings::get_singleton()->get("rendering/gles3/shaders/max_simultaneous_compiles"));
#ifdef GLES_OVER_GL
		if (GLAD_GL_ARB_parallel_shader_compile) {
			glMaxShaderCompilerThreadsARB(ShaderGLES3::max_simultaneous_compiles);
		} else if (GLAD_GL_KHR_parallel_shader_compile) {
			glMaxShaderCompilerThreadsKHR(ShaderGLES3::max_simultaneous_compiles);
		}
#else
#if defined(IPHONE_ENABLED) || defined(ANDROID_ENABLED) // TODO: Consider more platforms?
		void *gles3_lib = nullptr;
		void (*MaxShaderCompilerThreads)(GLuint) = nullptr;
#if defined(IPHONE_ENABLED)
		gles3_lib = dlopen(nullptr, RTLD_LAZY);
#elif defined(ANDROID_ENABLED)
		gles3_lib = dlopen("libGLESv3.so", RTLD_LAZY);
#endif
		if (gles3_lib) {
			MaxShaderCompilerThreads = (void (*)(GLuint))dlsym(gles3_lib, "glMaxShaderCompilerThreadsARB");
			if (!MaxShaderCompilerThreads) {
				MaxShaderCompilerThreads = (void (*)(GLuint))dlsym(gles3_lib, "glMaxShaderCompilerThreadsKHR");
			}
		}
		if (MaxShaderCompilerThreads) {
			MaxShaderCompilerThreads(ShaderGLES3::max_simultaneous_compiles);
		} else {
#ifdef DEBUG_ENABLED
			print_line("Async. shader compilation: No MaxShaderCompilerThreads function found.");
#endif
		}
#endif
#endif
	} else {
		ShaderGLES3::max_simultaneous_compiles = 0;
	}
#ifdef DEBUG_ENABLED
	ShaderGLES3::log_active_async_compiles_count = (bool)ProjectSettings::get_singleton()->get("rendering/gles3/shaders/log_active_async_compiles_count");
#endif

	frame.clear_request = false;

	shaders.compile_queue = nullptr;
	shaders.cache = nullptr;
	shaders.tracker = nullptr;
	if (Main::shader_tracke_enabled()) {
		shaders.tracker = memnew(ShaderTrackerGLES3);
	}
	shaders.preLoader = memnew(ShaderPreLoader);
	shaders.cache_write_queue = nullptr;
	bool effectively_on = false;
	if (config.async_compilation_enabled) {
		if (config.parallel_shader_compile_supported) {
			print_line("Async. shader compilation: ON (full native support)");
			effectively_on = true;
		} else if (config.program_binary_supported && OS::get_singleton()->is_offscreen_gl_available()) {
			shaders.compile_queue = memnew(ThreadedCallableQueue<GLuint>());
			shaders.compile_queue->enqueue(0, []() { OS::get_singleton()->set_offscreen_gl_current(true); });
			print_line("Async. shader compilation: ON (via secondary context)");
			effectively_on = true;
		} else {
			print_line("Async. shader compilation: OFF (enabled for " + String(Engine::get_singleton()->is_editor_hint() ? "editor" : "project") + ", but not supported)");
		}
		if (effectively_on) {
			if (config.shader_cache_enabled) {
				if (config.program_binary_supported) {
					print_line("Shader cache: ON");
					shaders.cache = memnew(ShaderCacheGLES3);
					shaders.cache_write_queue = memnew(ThreadedCallableQueue<GLuint>());
				} else {
					print_line("Shader cache: OFF (enabled, but not supported)");
				}
			} else {
				print_line("Shader cache: OFF");
			}
		}
	} else {
		print_line("Async. shader compilation: OFF");
	}
	ShaderGLES3::compile_queue = shaders.compile_queue;
	ShaderGLES3::parallel_compile_supported = config.parallel_shader_compile_supported;
	ShaderGLES3::shader_cache = shaders.cache;
	ShaderGLES3::cache_write_queue = shaders.cache_write_queue;

	shaders.copy.init();

	{
		// Generate default textures.

		// Opaque white color.
		glGenTextures(1, &resources.white_tex);
		unsigned char whitetexdata[8 * 8 * 3];
		for (int i = 0; i < 8 * 8 * 3; i++) {
			whitetexdata[i] = 255;
		}
		gl_wrapper.gl_active_texture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, resources.white_tex);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 8, 8, 0, GL_RGB, GL_UNSIGNED_BYTE, whitetexdata);
		glGenerateMipmap(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, 0);

		// Opaque black color.
		glGenTextures(1, &resources.black_tex);
		unsigned char blacktexdata[8 * 8 * 3];
		for (int i = 0; i < 8 * 8 * 3; i++) {
			blacktexdata[i] = 0;
		}
		gl_wrapper.gl_active_texture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, resources.black_tex);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 8, 8, 0, GL_RGB, GL_UNSIGNED_BYTE, blacktexdata);
		glGenerateMipmap(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, 0);

		// Transparent black color.
		glGenTextures(1, &resources.transparent_tex);
		unsigned char transparenttexdata[8 * 8 * 4];
		for (int i = 0; i < 8 * 8 * 4; i++) {
			transparenttexdata[i] = 0;
		}
		gl_wrapper.gl_active_texture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, resources.transparent_tex);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 8, 8, 0, GL_RGBA, GL_UNSIGNED_BYTE, transparenttexdata);
		glGenerateMipmap(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, 0);

		// Opaque "flat" normal map color.
		glGenTextures(1, &resources.normal_tex);
		unsigned char normaltexdata[8 * 8 * 3];
		for (int i = 0; i < 8 * 8 * 3; i += 3) {
			normaltexdata[i + 0] = 128;
			normaltexdata[i + 1] = 128;
			normaltexdata[i + 2] = 255;
		}
		gl_wrapper.gl_active_texture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, resources.normal_tex);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 8, 8, 0, GL_RGB, GL_UNSIGNED_BYTE, normaltexdata);
		glGenerateMipmap(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, 0);

		// Opaque "flat" flowmap color.
		glGenTextures(1, &resources.aniso_tex);
		unsigned char anisotexdata[8 * 8 * 3];
		for (int i = 0; i < 8 * 8 * 3; i += 3) {
			anisotexdata[i + 0] = 255;
			anisotexdata[i + 1] = 128;
			anisotexdata[i + 2] = 0;
		}
		gl_wrapper.gl_active_texture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, resources.aniso_tex);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 8, 8, 0, GL_RGB, GL_UNSIGNED_BYTE, anisotexdata);
		glGenerateMipmap(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, 0);

		glGenTextures(1, &resources.depth_tex);
		unsigned char depthtexdata[8 * 8 * 2] = {};

		gl_wrapper.gl_active_texture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, resources.depth_tex);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT16, 8, 8, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT, depthtexdata);
		glBindTexture(GL_TEXTURE_2D, 0);

		// Opaque white color for 3D texture.
		glGenTextures(1, &resources.white_tex_3d);
		gl_wrapper.gl_active_texture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_3D, resources.white_tex_3d);
		glTexImage3D(GL_TEXTURE_3D, 0, GL_RGB, 2, 2, 2, 0, GL_RGB, GL_UNSIGNED_BYTE, whitetexdata);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_BASE_LEVEL, 0);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAX_LEVEL, 0);

		// Opaque white color for texture array.
		glGenTextures(1, &resources.white_tex_array);
		gl_wrapper.gl_active_texture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D_ARRAY, resources.white_tex_array);
		glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGB, 8, 8, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
		glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, 0, 8, 8, 1, GL_RGB, GL_UNSIGNED_BYTE, whitetexdata);
		glGenerateMipmap(GL_TEXTURE_2D_ARRAY);
		glBindTexture(GL_TEXTURE_2D, 0);
	}

	config.max_texture_size = safe_gl_get_integer(GL_MAX_TEXTURE_SIZE);
	config.max_cubemap_texture_size = safe_gl_get_integer(GL_MAX_CUBE_MAP_TEXTURE_SIZE);

	config.use_rgba_2d_shadows = !config.framebuffer_float_supported;

	//generic quadie for copying

	{
		//quad buffers

		glGenBuffers(1, &resources.quadie);
		glBindBuffer(GL_ARRAY_BUFFER, resources.quadie);
		{
			const float qv[16] = {
				-1,
				-1,
				0,
				0,
				-1,
				1,
				0,
				1,
				1,
				1,
				1,
				1,
				1,
				-1,
				1,
				0,
			};

			glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 16, qv, GL_STATIC_DRAW);
		}

		glBindBuffer(GL_ARRAY_BUFFER, 0); //unbind

		glGenVertexArrays(1, &resources.quadie_array);
		glBindVertexArray(resources.quadie_array);
		glBindBuffer(GL_ARRAY_BUFFER, resources.quadie);
		glVertexAttribPointer(VS::ARRAY_VERTEX, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 4, nullptr);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(VS::ARRAY_TEX_UV, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 4, CAST_INT_TO_UCHAR_PTR(8));
		glEnableVertexAttribArray(4);
		glBindVertexArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, 0); //unbind
	}

	//generic quadie for copying without touching sky

	{
		//transform feedback buffers
		uint32_t xf_feedback_size = GLOBAL_DEF_RST("rendering/limits/buffers/blend_shape_max_buffer_size_kb", 4096);
		ProjectSettings::get_singleton()->set_custom_property_info("rendering/limits/buffers/blend_shape_max_buffer_size_kb", PropertyInfo(Variant::INT, "rendering/limits/buffers/blend_shape_max_buffer_size_kb", PROPERTY_HINT_RANGE, "0,8192,1,or_greater"));

		for (int i = 0; i < 2; i++) {
			glGenBuffers(1, &resources.transform_feedback_buffers[i]);
			glBindBuffer(GL_ARRAY_BUFFER, resources.transform_feedback_buffers[i]);
			glBufferData(GL_ARRAY_BUFFER, xf_feedback_size * 1024, nullptr, GL_STREAM_DRAW);
		}

		shaders.blend_shapes.init();

		glGenVertexArrays(1, &resources.transform_feedback_array);
	}

	shaders.cubemap_filter.init();
	bool ggx_hq = GLOBAL_GET("rendering/quality/reflections/high_quality_ggx");
	shaders.cubemap_filter.set_conditional(CubemapFilterShaderGLES3::LOW_QUALITY, !ggx_hq);
	shaders.particles.init();
	if (config.async_compilation_enabled) {
		shaders.particles.init_async_compilation();
	}

#ifdef GLES_OVER_GL
	glEnable(_EXT_TEXTURE_CUBE_MAP_SEAMLESS);
#endif

	frame.count = 0;
	frame.delta = 0;
	frame.current_rt = nullptr;
	config.keep_original_textures = false;
	config.generate_wireframes = false;
	config.use_texture_array_environment = GLOBAL_GET("rendering/quality/reflections/texture_array_reflections");

	config.force_vertex_shading = GLOBAL_GET("rendering/quality/shading/force_vertex_shading");

	String renderer = (const char *)glGetString(GL_RENDERER);

	GLOBAL_DEF("rendering/quality/lightmapping/use_bicubic_sampling", true);
	GLOBAL_DEF("rendering/quality/lightmapping/use_bicubic_sampling.mobile", false);
	config.use_lightmap_filter_bicubic = GLOBAL_GET("rendering/quality/lightmapping/use_bicubic_sampling");

	config.use_physical_light_attenuation = GLOBAL_GET("rendering/quality/shading/use_physical_light_attenuation");

	config.use_depth_prepass = bool(GLOBAL_GET("rendering/quality/depth_prepass/enable"));
	if (config.use_depth_prepass) {
		String vendors = GLOBAL_GET("rendering/quality/depth_prepass/disable_for_vendors");
		Vector<String> vendor_match = vendors.split(",");
		for (int i = 0; i < vendor_match.size(); i++) {
			String v = vendor_match[i].strip_edges();
			if (v == String()) {
				continue;
			}

			if (renderer.findn(v) != -1) {
				config.use_depth_prepass = false;
			}
		}
	}

	int orphan_mode = GLOBAL_GET("rendering/2d/opengl/legacy_orphan_buffers");
	switch (orphan_mode) {
		default: {
			config.should_orphan = true;
		} break;
		case 1: {
			config.should_orphan = false;
		} break;
		case 2: {
			config.should_orphan = true;
		} break;
	}
}

void RasterizerStorageGLES3::finalize() {
	glDeleteTextures(1, &resources.white_tex);
	glDeleteTextures(1, &resources.black_tex);
	glDeleteTextures(1, &resources.transparent_tex);
	glDeleteTextures(1, &resources.normal_tex);
	glDeleteTextures(1, &resources.depth_tex);
}

void RasterizerStorageGLES3::update_dirty_resources() {
	update_dirty_multimeshes();
	update_dirty_skeletons();
	update_dirty_shaders();
	update_dirty_materials();
	update_particles();
	update_dirty_captures();
}

RasterizerStorageGLES3::RasterizerStorageGLES3() {
	config.should_orphan = true;
}

RasterizerStorageGLES3::~RasterizerStorageGLES3() {
	if (shaders.cache) {
		memdelete(shaders.cache);
	}
	if (shaders.tracker) {
		memdelete(shaders.tracker);
	}
	if (shaders.preLoader) {
		memdelete(shaders.preLoader);
	}
	if (shaders.cache_write_queue) {
		memdelete(shaders.cache_write_queue);
	}
	if (shaders.compile_queue) {
		shaders.compile_queue->enqueue(0, []() { OS::get_singleton()->set_offscreen_gl_current(false); });
		memdelete(shaders.compile_queue);
	}
}

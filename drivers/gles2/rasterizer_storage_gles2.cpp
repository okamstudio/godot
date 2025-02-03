/**************************************************************************/
/*  rasterizer_storage_gles2.cpp                                          */
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

#include "rasterizer_storage_gles2.h"

#include "core/math/transform.h"
#include "core/project_settings.h"
#include "rasterizer_canvas_gles2.h"
#include "rasterizer_scene_gles2.h"
#include "servers/visual/shader_language.h"
#include "servers/visual/visual_server_canvas.h"
#include "servers/visual/visual_server_globals.h"

GLuint RasterizerStorageGLES2::system_fbo = 0;

/* TEXTURE API */

#define _EXT_COMPRESSED_RGBA_S3TC_DXT1_EXT 0x83F1
#define _EXT_COMPRESSED_RGBA_S3TC_DXT3_EXT 0x83F2
#define _EXT_COMPRESSED_RGBA_S3TC_DXT5_EXT 0x83F3

#define _EXT_COMPRESSED_RED_RGTC1_EXT 0x8DBB
#define _EXT_COMPRESSED_RED_RGTC1 0x8DBB
#define _EXT_COMPRESSED_SIGNED_RED_RGTC1 0x8DBC
#define _EXT_COMPRESSED_RG_RGTC2 0x8DBD
#define _EXT_COMPRESSED_SIGNED_RG_RGTC2 0x8DBE
#define _EXT_COMPRESSED_SIGNED_RED_RGTC1_EXT 0x8DBC
#define _EXT_COMPRESSED_RED_GREEN_RGTC2_EXT 0x8DBD
#define _EXT_COMPRESSED_SIGNED_RED_GREEN_RGTC2_EXT 0x8DBE
#define _EXT_ETC1_RGB8_OES 0x8D64

#define _EXT_COMPRESSED_RGB_PVRTC_4BPPV1_IMG 0x8C00
#define _EXT_COMPRESSED_RGB_PVRTC_2BPPV1_IMG 0x8C01
#define _EXT_COMPRESSED_RGBA_PVRTC_4BPPV1_IMG 0x8C02
#define _EXT_COMPRESSED_RGBA_PVRTC_2BPPV1_IMG 0x8C03

#define _EXT_COMPRESSED_SRGB_PVRTC_2BPPV1_EXT 0x8A54
#define _EXT_COMPRESSED_SRGB_PVRTC_4BPPV1_EXT 0x8A55
#define _EXT_COMPRESSED_SRGB_ALPHA_PVRTC_2BPPV1_EXT 0x8A56
#define _EXT_COMPRESSED_SRGB_ALPHA_PVRTC_4BPPV1_EXT 0x8A57

#define _EXT_COMPRESSED_RGBA_BPTC_UNORM 0x8E8C
#define _EXT_COMPRESSED_SRGB_ALPHA_BPTC_UNORM 0x8E8D
#define _EXT_COMPRESSED_RGB_BPTC_SIGNED_FLOAT 0x8E8E
#define _EXT_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT 0x8E8F

#define _GL_TEXTURE_EXTERNAL_OES 0x8D65

#ifdef GLES_OVER_GL
#define _GL_HALF_FLOAT_OES 0x140B
#else
#define _GL_HALF_FLOAT_OES 0x8D61
#endif

#define _EXT_TEXTURE_CUBE_MAP_SEAMLESS 0x884F

#define _GL_TEXTURE_MAX_ANISOTROPY_EXT 0x84FE
#define _GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT 0x84FF

#define _RED_OES 0x1903

#define _DEPTH_COMPONENT24_OES 0x81A6

#ifndef GLES_OVER_GL
#define glClearDepth glClearDepthf

// enable extensions manually for android and ios
#ifndef UWP_ENABLED
#include <dlfcn.h> // needed to load extensions
#endif

#ifdef IPHONE_ENABLED

#include <OpenGLES/ES2/glext.h>
//void *glRenderbufferStorageMultisampleAPPLE;
//void *glResolveMultisampleFramebufferAPPLE;
#define glRenderbufferStorageMultisample glRenderbufferStorageMultisampleAPPLE
#elif defined(ANDROID_ENABLED)

#include <GLES2/gl2ext.h>
PFNGLRENDERBUFFERSTORAGEMULTISAMPLEEXTPROC glRenderbufferStorageMultisampleEXT;
PFNGLFRAMEBUFFERTEXTURE2DMULTISAMPLEEXTPROC glFramebufferTexture2DMultisampleEXT;
#define glRenderbufferStorageMultisample glRenderbufferStorageMultisampleEXT
#define glFramebufferTexture2DMultisample glFramebufferTexture2DMultisampleEXT

#elif defined(UWP_ENABLED)
#include <GLES2/gl2ext.h>
#define glRenderbufferStorageMultisample glRenderbufferStorageMultisampleANGLE
#define glFramebufferTexture2DMultisample glFramebufferTexture2DMultisampleANGLE
#endif

#define GL_TEXTURE_3D 0x806F
#define GL_MAX_SAMPLES 0x8D57
#endif //!GLES_OVER_GL

void RasterizerStorageGLES2::GLWrapper::initialize(int p_max_texture_image_units) {
	texture_unit_table.create(p_max_texture_image_units);
}

void RasterizerStorageGLES2::GLWrapper::reset() {
	for (uint32_t i = 0; i < texture_units_bound.size(); i++) {
		::glActiveTexture(GL_TEXTURE0 + texture_units_bound[i]);
		glBindTexture(GL_TEXTURE_2D, 0);
	}

	texture_units_bound.clear();
	texture_unit_table.blank();
}

int32_t RasterizerStorageGLES2::safe_gl_get_integer(unsigned int p_gl_param_name, int32_t p_max_accepted) {
	// There is no glGetInteger64v in the base GLES2 spec as far as I can see.
	// So we will just have a capped 32 bit version for GLES2.
	int32_t temp;
	glGetIntegerv(p_gl_param_name, &temp);
	temp = MIN(temp, p_max_accepted);
	return temp;
}

void RasterizerStorageGLES2::bind_quad_array() const {
	glBindBuffer(GL_ARRAY_BUFFER, resources.quadie);
	glVertexAttribPointer(VS::ARRAY_VERTEX, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 4, nullptr);
	glVertexAttribPointer(VS::ARRAY_TEX_UV, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 4, CAST_INT_TO_UCHAR_PTR(8));

	glEnableVertexAttribArray(VS::ARRAY_VERTEX);
	glEnableVertexAttribArray(VS::ARRAY_TEX_UV);
}

Ref<Image> RasterizerStorageGLES2::_get_gl_image_and_format(const Ref<Image> &p_image, Image::Format p_format, uint32_t p_flags, Image::Format &r_real_format, GLenum &r_gl_format, GLenum &r_gl_internal_format, GLenum &r_gl_type, bool &r_compressed, bool p_force_decompress) const {
	r_gl_format = 0;
	Ref<Image> image = p_image;
	r_compressed = false;
	r_real_format = p_format;

	bool need_decompress = false;

	switch (p_format) {
		case Image::FORMAT_L8: {
			r_gl_internal_format = GL_LUMINANCE;
			r_gl_format = GL_LUMINANCE;
			r_gl_type = GL_UNSIGNED_BYTE;
		} break;
		case Image::FORMAT_LA8: {
			r_gl_internal_format = GL_LUMINANCE_ALPHA;
			r_gl_format = GL_LUMINANCE_ALPHA;
			r_gl_type = GL_UNSIGNED_BYTE;
		} break;
		case Image::FORMAT_R8: {
			r_gl_internal_format = GL_ALPHA;
			r_gl_format = GL_ALPHA;
			r_gl_type = GL_UNSIGNED_BYTE;

		} break;
		case Image::FORMAT_RG8: {
			ERR_PRINT("RG texture not supported, converting to RGB8.");
			if (image.is_valid()) {
				image->convert(Image::FORMAT_RGB8);
			}
			r_real_format = Image::FORMAT_RGB8;
			r_gl_internal_format = GL_RGB;
			r_gl_format = GL_RGB;
			r_gl_type = GL_UNSIGNED_BYTE;

		} break;
		case Image::FORMAT_RGB8: {
			r_gl_internal_format = GL_RGB;
			r_gl_format = GL_RGB;
			r_gl_type = GL_UNSIGNED_BYTE;

		} break;
		case Image::FORMAT_RGBA8: {
			r_gl_format = GL_RGBA;
			r_gl_internal_format = GL_RGBA;
			r_gl_type = GL_UNSIGNED_BYTE;

		} break;
		case Image::FORMAT_RGBA4444: {
			r_gl_internal_format = GL_RGBA;
			r_gl_format = GL_RGBA;
			r_gl_type = GL_UNSIGNED_SHORT_4_4_4_4;

		} break;
		case Image::FORMAT_RGBA5551: {
			r_gl_internal_format = GL_RGB5_A1;
			r_gl_format = GL_RGBA;
			r_gl_type = GL_UNSIGNED_SHORT_5_5_5_1;

		} break;
		case Image::FORMAT_RF: {
			if (!config.float_texture_supported) {
				ERR_PRINT("R float texture not supported, converting to RGB8.");
				if (image.is_valid()) {
					image->convert(Image::FORMAT_RGB8);
				}
				r_real_format = Image::FORMAT_RGB8;
				r_gl_internal_format = GL_RGB;
				r_gl_format = GL_RGB;
				r_gl_type = GL_UNSIGNED_BYTE;
			} else {
				r_gl_internal_format = GL_ALPHA;
				r_gl_format = GL_ALPHA;
				r_gl_type = GL_FLOAT;
			}
		} break;
		case Image::FORMAT_RGF: {
			ERR_PRINT("RG float texture not supported, converting to RGB8.");
			if (image.is_valid()) {
				image->convert(Image::FORMAT_RGB8);
			}
			r_real_format = Image::FORMAT_RGB8;
			r_gl_internal_format = GL_RGB;
			r_gl_format = GL_RGB;
			r_gl_type = GL_UNSIGNED_BYTE;
		} break;
		case Image::FORMAT_RGBF: {
			if (!config.float_texture_supported) {
				ERR_PRINT("RGB float texture not supported, converting to RGB8.");
				if (image.is_valid()) {
					image->convert(Image::FORMAT_RGB8);
				}
				r_real_format = Image::FORMAT_RGB8;
				r_gl_internal_format = GL_RGB;
				r_gl_format = GL_RGB;
				r_gl_type = GL_UNSIGNED_BYTE;
			} else {
				r_gl_internal_format = GL_RGB;
				r_gl_format = GL_RGB;
				r_gl_type = GL_FLOAT;
			}
		} break;
		case Image::FORMAT_RGBAF: {
			if (!config.float_texture_supported) {
				ERR_PRINT("RGBA float texture not supported, converting to RGBA8.");
				if (image.is_valid()) {
					image->convert(Image::FORMAT_RGBA8);
				}
				r_real_format = Image::FORMAT_RGBA8;
				r_gl_internal_format = GL_RGBA;
				r_gl_format = GL_RGBA;
				r_gl_type = GL_UNSIGNED_BYTE;
			} else {
				r_gl_internal_format = GL_RGBA;
				r_gl_format = GL_RGBA;
				r_gl_type = GL_FLOAT;
			}
		} break;
		case Image::FORMAT_RH: {
			need_decompress = true;
		} break;
		case Image::FORMAT_RGH: {
			need_decompress = true;
		} break;
		case Image::FORMAT_RGBH: {
			need_decompress = true;
		} break;
		case Image::FORMAT_RGBAH: {
			need_decompress = true;
		} break;
		case Image::FORMAT_RGBE9995: {
			r_gl_internal_format = GL_RGB;
			r_gl_format = GL_RGB;
			r_gl_type = GL_UNSIGNED_BYTE;

			if (image.is_valid()) {
				image = image->rgbe_to_srgb();
			}

			return image;

		} break;
		case Image::FORMAT_DXT1: {
			if (config.s3tc_supported) {
				r_gl_internal_format = _EXT_COMPRESSED_RGBA_S3TC_DXT1_EXT;
				r_gl_format = GL_RGBA;
				r_gl_type = GL_UNSIGNED_BYTE;
				r_compressed = true;
			} else {
				need_decompress = true;
			}

		} break;
		case Image::FORMAT_DXT3: {
			if (config.s3tc_supported) {
				r_gl_internal_format = _EXT_COMPRESSED_RGBA_S3TC_DXT3_EXT;
				r_gl_format = GL_RGBA;
				r_gl_type = GL_UNSIGNED_BYTE;
				r_compressed = true;
			} else {
				need_decompress = true;
			}

		} break;
		case Image::FORMAT_DXT5: {
			if (config.s3tc_supported) {
				r_gl_internal_format = _EXT_COMPRESSED_RGBA_S3TC_DXT5_EXT;
				r_gl_format = GL_RGBA;
				r_gl_type = GL_UNSIGNED_BYTE;
				r_compressed = true;
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
				r_gl_internal_format = _EXT_COMPRESSED_RGBA_BPTC_UNORM;
				r_gl_format = GL_RGBA;
				r_gl_type = GL_UNSIGNED_BYTE;
				r_compressed = true;

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
				r_gl_internal_format = _EXT_COMPRESSED_RGB_PVRTC_2BPPV1_IMG;
				r_gl_format = GL_RGBA;
				r_gl_type = GL_UNSIGNED_BYTE;
				r_compressed = true;

			} else {
				need_decompress = true;
			}
		} break;
		case Image::FORMAT_PVRTC2A: {
			if (config.pvrtc_supported) {
				r_gl_internal_format = _EXT_COMPRESSED_RGBA_PVRTC_2BPPV1_IMG;
				r_gl_format = GL_RGBA;
				r_gl_type = GL_UNSIGNED_BYTE;
				r_compressed = true;

			} else {
				need_decompress = true;
			}

		} break;
		case Image::FORMAT_PVRTC4: {
			if (config.pvrtc_supported) {
				r_gl_internal_format = _EXT_COMPRESSED_RGB_PVRTC_4BPPV1_IMG;
				r_gl_format = GL_RGBA;
				r_gl_type = GL_UNSIGNED_BYTE;
				r_compressed = true;

			} else {
				need_decompress = true;
			}

		} break;
		case Image::FORMAT_PVRTC4A: {
			if (config.pvrtc_supported) {
				r_gl_internal_format = _EXT_COMPRESSED_RGBA_PVRTC_4BPPV1_IMG;
				r_gl_format = GL_RGBA;
				r_gl_type = GL_UNSIGNED_BYTE;
				r_compressed = true;

			} else {
				need_decompress = true;
			}

		} break;
		case Image::FORMAT_ETC: {
			if (config.etc1_supported) {
				r_gl_internal_format = _EXT_ETC1_RGB8_OES;
				r_gl_format = GL_RGBA;
				r_gl_type = GL_UNSIGNED_BYTE;
				r_compressed = true;
			} else {
				need_decompress = true;
			}
		} break;
		case Image::FORMAT_ETC2_R11: {
			need_decompress = true;
		} break;
		case Image::FORMAT_ETC2_R11S: {
			need_decompress = true;
		} break;
		case Image::FORMAT_ETC2_RG11: {
			need_decompress = true;
		} break;
		case Image::FORMAT_ETC2_RG11S: {
			need_decompress = true;
		} break;
		case Image::FORMAT_ETC2_RGB8: {
			need_decompress = true;
		} break;
		case Image::FORMAT_ETC2_RGBA8: {
			need_decompress = true;
		} break;
		case Image::FORMAT_ETC2_RGB8A1: {
			need_decompress = true;
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
			switch (image->get_format()) {
				case Image::FORMAT_RGB8: {
					r_gl_format = GL_RGB;
					r_gl_internal_format = GL_RGB;
					r_gl_type = GL_UNSIGNED_BYTE;
					r_real_format = Image::FORMAT_RGB8;
					r_compressed = false;
				} break;
				case Image::FORMAT_RGBA8: {
					r_gl_format = GL_RGBA;
					r_gl_internal_format = GL_RGBA;
					r_gl_type = GL_UNSIGNED_BYTE;
					r_real_format = Image::FORMAT_RGBA8;
					r_compressed = false;
				} break;
				default: {
					image->convert(Image::FORMAT_RGBA8);
					r_gl_format = GL_RGBA;
					r_gl_internal_format = GL_RGBA;
					r_gl_type = GL_UNSIGNED_BYTE;
					r_real_format = Image::FORMAT_RGBA8;
					r_compressed = false;

				} break;
			}
		}

		return image;
	}

	return p_image;
}

static const GLenum _cube_side_enum[6] = {

	GL_TEXTURE_CUBE_MAP_NEGATIVE_X,
	GL_TEXTURE_CUBE_MAP_POSITIVE_X,
	GL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
	GL_TEXTURE_CUBE_MAP_POSITIVE_Y,
	GL_TEXTURE_CUBE_MAP_NEGATIVE_Z,
	GL_TEXTURE_CUBE_MAP_POSITIVE_Z,

};

RID RasterizerStorageGLES2::texture_create() {
	Texture *texture = memnew(Texture);
	ERR_FAIL_COND_V(!texture, RID());
	glGenTextures(1, &texture->tex_id);
	texture->active = false;
	texture->total_data_size = 0;

	return texture_owner.make_rid(texture);
}

void RasterizerStorageGLES2::texture_allocate(RID p_texture, int p_width, int p_height, int p_depth_3d, Image::Format p_format, VisualServer::TextureType p_type, uint32_t p_flags) {
	GLenum format;
	GLenum internal_format;
	GLenum type;

	bool compressed = false;

	if (p_flags & VS::TEXTURE_FLAG_USED_FOR_STREAMING) {
		p_flags &= ~VS::TEXTURE_FLAG_MIPMAPS; // no mipies for video
	}

	Texture *texture = texture_owner.getornull(p_texture);
	ERR_FAIL_COND(!texture);
	texture->width = p_width;
	texture->height = p_height;
	texture->format = p_format;

	if (texture->width > config.max_texture_size || texture->height > config.max_texture_size) {
		WARN_PRINT("Cannot create texture larger than maximum hardware supported size of " + itos(config.max_texture_size) + ". Setting size to maximum.");
		texture->width = MIN(texture->width, config.max_texture_size);
		texture->height = MIN(texture->height, config.max_texture_size);
	}

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
		case VS::TEXTURE_TYPE_2D_ARRAY:
		case VS::TEXTURE_TYPE_3D: {
			texture->target = GL_TEXTURE_3D;
			ERR_PRINT("3D textures and Texture Arrays are not supported in GLES2. Please switch to the GLES3 backend.");
			return;
		} break;
		default: {
			ERR_PRINT("Unknown texture type!");
			return;
		}
	}

	if (p_type != VS::TEXTURE_TYPE_EXTERNAL) {
		texture->alloc_width = texture->width;
		texture->alloc_height = texture->height;
		texture->resize_to_po2 = false;
		if (!config.support_npot_repeat_mipmap) {
			int po2_width = next_power_of_2(p_width);
			int po2_height = next_power_of_2(p_height);

			bool is_po2 = p_width == po2_width && p_height == po2_height;

			if (!is_po2 && (p_flags & VS::TEXTURE_FLAG_REPEAT || p_flags & VS::TEXTURE_FLAG_MIPMAPS)) {
				if (p_flags & VS::TEXTURE_FLAG_USED_FOR_STREAMING) {
					//not supported
					ERR_PRINT("Streaming texture for non power of 2 or has mipmaps on this hardware: " + texture->path + "'. Mipmaps and repeat disabled.");
					texture->flags &= ~(VS::TEXTURE_FLAG_REPEAT | VS::TEXTURE_FLAG_MIPMAPS);
				} else {
					texture->alloc_height = po2_height;
					texture->alloc_width = po2_width;
					texture->resize_to_po2 = true;
				}
			}
		}

		Image::Format real_format;
		_get_gl_image_and_format(Ref<Image>(),
				texture->format,
				texture->flags,
				real_format,
				format,
				internal_format,
				type,
				compressed,
				texture->resize_to_po2);

		texture->gl_format_cache = format;
		texture->gl_type_cache = type;
		texture->gl_internal_format_cache = internal_format;
		texture->data_size = 0;
		texture->mipmaps = 1;

		texture->compressed = compressed;
	}

	gl_wrapper.gl_active_texture(GL_TEXTURE0);
	glBindTexture(texture->target, texture->tex_id);

	if (p_type == VS::TEXTURE_TYPE_EXTERNAL) {
		glTexParameteri(texture->target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(texture->target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(texture->target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(texture->target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	} else if (p_flags & VS::TEXTURE_FLAG_USED_FOR_STREAMING) {
		//prealloc if video
		glTexImage2D(texture->target, 0, internal_format, texture->alloc_width, texture->alloc_height, 0, format, type, nullptr);
	}

	texture->active = true;
}

void RasterizerStorageGLES2::texture_set_data(RID p_texture, const Ref<Image> &p_image, int p_layer) {
	Texture *texture = texture_owner.getornull(p_texture);

	ERR_FAIL_COND(!texture);
	if (texture->target == GL_TEXTURE_3D) {
		// Target is set to a 3D texture or array texture, exit early to avoid spamming errors
		return;
	}
	ERR_FAIL_COND(!texture->active);
	ERR_FAIL_COND(texture->render_target);
	ERR_FAIL_COND(texture->format != p_image->get_format());
	ERR_FAIL_COND(p_image.is_null());
	ERR_FAIL_COND(texture->type == VS::TEXTURE_TYPE_EXTERNAL);

	GLenum type;
	GLenum format;
	GLenum internal_format;
	bool compressed = false;

	if (config.keep_original_textures && !(texture->flags & VS::TEXTURE_FLAG_USED_FOR_STREAMING)) {
		texture->images.write[p_layer] = p_image;
	}

	Image::Format real_format;
	Ref<Image> img = _get_gl_image_and_format(p_image, p_image->get_format(), texture->flags, real_format, format, internal_format, type, compressed, texture->resize_to_po2);

	if (texture->resize_to_po2) {
		if (p_image->is_compressed()) {
			ERR_PRINT("Texture '" + texture->path + "' is required to be a power of 2 because it uses either mipmaps or repeat, so it was decompressed. This will hurt performance and memory usage.");
		}

		if (img == p_image) {
			img = img->duplicate();
		}
		img->resize_to_po2(false, texture->flags & VS::TEXTURE_FLAG_FILTER ? Image::INTERPOLATE_BILINEAR : Image::INTERPOLATE_NEAREST);
	}

	if (config.shrink_textures_x2 && (p_image->has_mipmaps() || !p_image->is_compressed()) && !(texture->flags & VS::TEXTURE_FLAG_USED_FOR_STREAMING)) {
		texture->alloc_height = MAX(1, texture->alloc_height / 2);
		texture->alloc_width = MAX(1, texture->alloc_width / 2);

		if (texture->alloc_width == img->get_width() / 2 && texture->alloc_height == img->get_height() / 2) {
			img->shrink_x2();
		} else if (img->get_format() <= Image::FORMAT_RGBA8) {
			img->resize(texture->alloc_width, texture->alloc_height, Image::INTERPOLATE_BILINEAR);
		}
	}

	GLenum blit_target = (texture->target == GL_TEXTURE_CUBE_MAP) ? _cube_side_enum[p_layer] : GL_TEXTURE_2D;

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

		if (compressed) {
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

		tsize += size;

		w = MAX(1, w >> 1);
		h = MAX(1, h >> 1);
	}

	info.texture_mem -= texture->total_data_size;
	texture->total_data_size = tsize;
	info.texture_mem += texture->total_data_size;

	// printf("texture: %i x %i - size: %i - total: %i\n", texture->width, texture->height, tsize, info.texture_mem);

	texture->stored_cube_sides |= (1 << p_layer);

	if ((texture->flags & VS::TEXTURE_FLAG_MIPMAPS) && mipmaps == 1 && !texture->ignore_mipmaps && (texture->type != VS::TEXTURE_TYPE_CUBEMAP || texture->stored_cube_sides == (1 << 6) - 1)) {
		//generate mipmaps if they were requested and the image does not contain them
		glGenerateMipmap(texture->target);
	}

	texture->mipmaps = mipmaps;
}

void RasterizerStorageGLES2::texture_set_data_partial(RID p_texture, const Ref<Image> &p_image, int src_x, int src_y, int src_w, int src_h, int dst_x, int dst_y, int p_dst_mip, int p_layer) {
	// TODO
	ERR_PRINT("Not implemented (ask Karroffel to do it :p)");
}

Ref<Image> RasterizerStorageGLES2::texture_get_data(RID p_texture, int p_layer) const {
	Texture *texture = texture_owner.getornull(p_texture);

	ERR_FAIL_COND_V(!texture, Ref<Image>());
	ERR_FAIL_COND_V(!texture->active, Ref<Image>());
	ERR_FAIL_COND_V(texture->data_size == 0 && !texture->render_target, Ref<Image>());

	if (texture->type == VS::TEXTURE_TYPE_CUBEMAP && p_layer < 6 && p_layer >= 0 && !texture->images[p_layer].is_null()) {
		return texture->images[p_layer];
	}

#ifdef GLES_OVER_GL

	Image::Format real_format;
	GLenum gl_format;
	GLenum gl_internal_format;
	GLenum gl_type;
	bool compressed;
	_get_gl_image_and_format(Ref<Image>(), texture->format, texture->flags, real_format, gl_format, gl_internal_format, gl_type, compressed, false);

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

	wb.release();

	data.resize(data_size);

	Image *img = memnew(Image(texture->alloc_width, texture->alloc_height, texture->mipmaps > 1, real_format, data));

	return Ref<Image>(img);
#else

	Image::Format real_format;
	GLenum gl_format;
	GLenum gl_internal_format;
	GLenum gl_type;
	bool compressed;
	_get_gl_image_and_format(Ref<Image>(), texture->format, texture->flags, real_format, gl_format, gl_internal_format, gl_type, compressed, texture->resize_to_po2);

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

	shaders.copy.bind();

	glClearColor(0.0, 0.0, 0.0, 0.0);
	glClear(GL_COLOR_BUFFER_BIT);
	bind_quad_array();
	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	glReadPixels(0, 0, texture->alloc_width, texture->alloc_height, GL_RGBA, GL_UNSIGNED_BYTE, &wb[0]);

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

void RasterizerStorageGLES2::texture_set_flags(RID p_texture, uint32_t p_flags) {
	Texture *texture = texture_owner.getornull(p_texture);
	ERR_FAIL_COND(!texture);

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

	if (texture->flags & VS::TEXTURE_FLAG_FILTER) {
		glTexParameteri(texture->target, GL_TEXTURE_MAG_FILTER, GL_LINEAR); // Linear Filtering

	} else {
		glTexParameteri(texture->target, GL_TEXTURE_MAG_FILTER, GL_NEAREST); // raw Filtering
	}
}

uint32_t RasterizerStorageGLES2::texture_get_flags(RID p_texture) const {
	Texture *texture = texture_owner.getornull(p_texture);

	ERR_FAIL_COND_V(!texture, 0);

	return texture->flags;
}

Image::Format RasterizerStorageGLES2::texture_get_format(RID p_texture) const {
	Texture *texture = texture_owner.getornull(p_texture);

	ERR_FAIL_COND_V(!texture, Image::FORMAT_L8);

	return texture->format;
}

VisualServer::TextureType RasterizerStorageGLES2::texture_get_type(RID p_texture) const {
	Texture *texture = texture_owner.getornull(p_texture);

	ERR_FAIL_COND_V(!texture, VS::TEXTURE_TYPE_2D);

	return texture->type;
}

uint32_t RasterizerStorageGLES2::texture_get_texid(RID p_texture) const {
	Texture *texture = texture_owner.getornull(p_texture);

	ERR_FAIL_COND_V(!texture, 0);

	return texture->tex_id;
}

void RasterizerStorageGLES2::texture_bind(RID p_texture, uint32_t p_texture_no) {
	Texture *texture = texture_owner.getornull(p_texture);

	ERR_FAIL_COND(!texture);

	gl_wrapper.gl_active_texture(GL_TEXTURE0 + p_texture_no);
	glBindTexture(texture->target, texture->tex_id);
}

uint32_t RasterizerStorageGLES2::texture_get_width(RID p_texture) const {
	Texture *texture = texture_owner.getornull(p_texture);

	ERR_FAIL_COND_V(!texture, 0);

	return texture->width;
}

uint32_t RasterizerStorageGLES2::texture_get_height(RID p_texture) const {
	Texture *texture = texture_owner.getornull(p_texture);

	ERR_FAIL_COND_V(!texture, 0);

	return texture->height;
}

uint32_t RasterizerStorageGLES2::texture_get_depth(RID p_texture) const {
	Texture *texture = texture_owner.getornull(p_texture);

	ERR_FAIL_COND_V(!texture, 0);

	return texture->depth;
}

void RasterizerStorageGLES2::texture_set_size_override(RID p_texture, int p_width, int p_height, int p_depth) {
	Texture *texture = texture_owner.getornull(p_texture);

	ERR_FAIL_COND(!texture);
	ERR_FAIL_COND(texture->render_target);

	ERR_FAIL_COND(p_width <= 0 || p_width > 16384);
	ERR_FAIL_COND(p_height <= 0 || p_height > 16384);
	//real texture size is in alloc width and height
	texture->width = p_width;
	texture->height = p_height;
}

void RasterizerStorageGLES2::texture_set_path(RID p_texture, const String &p_path) {
	Texture *texture = texture_owner.getornull(p_texture);
	ERR_FAIL_COND(!texture);

	texture->path = p_path;
}

String RasterizerStorageGLES2::texture_get_path(RID p_texture) const {
	Texture *texture = texture_owner.getornull(p_texture);
	ERR_FAIL_COND_V(!texture, "");

	return texture->path;
}

void RasterizerStorageGLES2::texture_debug_usage(List<VS::TextureInfo> *r_info) {
	List<RID> textures;
	texture_owner.get_owned_list(&textures);

	for (List<RID>::Element *E = textures.front(); E; E = E->next()) {
		Texture *t = texture_owner.getornull(E->get());
		if (!t) {
			continue;
		}
		VS::TextureInfo tinfo;
		tinfo.texture = E->get();
		tinfo.path = t->path;
		tinfo.format = t->format;
		tinfo.width = t->alloc_width;
		tinfo.height = t->alloc_height;
		tinfo.depth = 0;
		tinfo.bytes = t->total_data_size;
		r_info->push_back(tinfo);
	}
}

void RasterizerStorageGLES2::texture_set_shrink_all_x2_on_set_data(bool p_enable) {
	config.shrink_textures_x2 = p_enable;
}

void RasterizerStorageGLES2::textures_keep_original(bool p_enable) {
	config.keep_original_textures = p_enable;
}

Size2 RasterizerStorageGLES2::texture_size_with_proxy(RID p_texture) const {
	const Texture *texture = texture_owner.getornull(p_texture);
	ERR_FAIL_COND_V(!texture, Size2());
	if (texture->proxy) {
		return Size2(texture->proxy->width, texture->proxy->height);
	} else {
		return Size2(texture->width, texture->height);
	}
}

void RasterizerStorageGLES2::texture_set_proxy(RID p_texture, RID p_proxy) {
	Texture *texture = texture_owner.getornull(p_texture);
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

void RasterizerStorageGLES2::texture_set_force_redraw_if_visible(RID p_texture, bool p_enable) {
	Texture *texture = texture_owner.getornull(p_texture);
	ERR_FAIL_COND(!texture);

	texture->redraw_if_visible = p_enable;
}

void RasterizerStorageGLES2::texture_set_detect_3d_callback(RID p_texture, VisualServer::TextureDetectCallback p_callback, void *p_userdata) {
	Texture *texture = texture_owner.get(p_texture);
	ERR_FAIL_COND(!texture);

	texture->detect_3d = p_callback;
	texture->detect_3d_ud = p_userdata;
}

void RasterizerStorageGLES2::texture_set_detect_srgb_callback(RID p_texture, VisualServer::TextureDetectCallback p_callback, void *p_userdata) {
	Texture *texture = texture_owner.get(p_texture);
	ERR_FAIL_COND(!texture);

	texture->detect_srgb = p_callback;
	texture->detect_srgb_ud = p_userdata;
}

void RasterizerStorageGLES2::texture_set_detect_normal_callback(RID p_texture, VisualServer::TextureDetectCallback p_callback, void *p_userdata) {
	Texture *texture = texture_owner.get(p_texture);
	ERR_FAIL_COND(!texture);

	texture->detect_normal = p_callback;
	texture->detect_normal_ud = p_userdata;
}

RID RasterizerStorageGLES2::texture_create_radiance_cubemap(RID p_source, int p_resolution) const {
	return RID();
}

RID RasterizerStorageGLES2::sky_create() {
	Sky *sky = memnew(Sky);
	sky->radiance = 0;
	return sky_owner.make_rid(sky);
}

void RasterizerStorageGLES2::sky_set_texture(RID p_sky, RID p_panorama, int p_radiance_size) {
	Sky *sky = sky_owner.getornull(p_sky);
	ERR_FAIL_COND(!sky);

	if (sky->panorama.is_valid()) {
		sky->panorama = RID();
		glDeleteTextures(1, &sky->radiance);
		sky->radiance = 0;
	}

	sky->panorama = p_panorama;
	if (!sky->panorama.is_valid()) {
		return; // the panorama was cleared
	}

	Texture *texture = texture_owner.getornull(sky->panorama);
	if (!texture) {
		sky->panorama = RID();
		ERR_FAIL_COND(!texture);
	}

	texture = texture->get_ptr(); //resolve for proxies

	// glBindVertexArray(0) and more
	{
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
		glDisable(GL_CULL_FACE);
		glDisable(GL_DEPTH_TEST);
		glDisable(GL_SCISSOR_TEST);
		glDisable(GL_BLEND);

		for (int i = 0; i < VS::ARRAY_MAX - 1; i++) {
			glDisableVertexAttribArray(i);
		}
	}

	gl_wrapper.gl_active_texture(GL_TEXTURE0);
	glBindTexture(texture->target, texture->tex_id);

	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR); //need this for proper sampling

	gl_wrapper.gl_active_texture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, resources.radical_inverse_vdc_cache_tex);

	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	// New cubemap that will hold the mipmaps with different roughness values
	gl_wrapper.gl_active_texture(GL_TEXTURE2);
	glGenTextures(1, &sky->radiance);
	glBindTexture(GL_TEXTURE_CUBE_MAP, sky->radiance);

	int size = p_radiance_size / 2; //divide by two because its a cubemap (this is an approximation because GLES3 uses a dual paraboloid)

	GLenum internal_format = GL_RGB;
	GLenum format = GL_RGB;
	GLenum type = GL_UNSIGNED_BYTE;

	// Set the initial (empty) mipmaps
	// Mobile hardware (PowerVR specially) prefers this approach,
	// the previous approach with manual lod levels kills the game.
	for (int i = 0; i < 6; i++) {
		glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, internal_format, size, size, 0, format, type, nullptr);
	}

	glGenerateMipmap(GL_TEXTURE_CUBE_MAP);

	// No filters for now
	glTexParameterf(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameterf(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameterf(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameterf(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	// Framebuffer

	glBindFramebuffer(GL_FRAMEBUFFER, resources.mipmap_blur_fbo);

	int mipmaps = 6;
	int lod = 0;
	int mm_level = mipmaps;
	size = p_radiance_size / 2;
	shaders.cubemap_filter.set_conditional(CubemapFilterShaderGLES2::USE_SOURCE_PANORAMA, true);
	shaders.cubemap_filter.set_conditional(CubemapFilterShaderGLES2::USE_DIRECT_WRITE, true);
	shaders.cubemap_filter.bind();

	// third, render to the framebuffer using separate textures, then copy to mipmaps
	while (size >= 1) {
		//make framebuffer size the texture size, need to use a separate texture for compatibility
		gl_wrapper.gl_active_texture(GL_TEXTURE3);
		glBindTexture(GL_TEXTURE_2D, resources.mipmap_blur_color);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, size, size, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, resources.mipmap_blur_color, 0);

		if (lod == 1) {
			// We set USE_DIRECT_WRITE to false for LOD levels 1 and up, so the shader will properly
			// filter the roughness instead of just copying 1:1 from the source panorama.
			shaders.cubemap_filter.set_conditional(CubemapFilterShaderGLES2::USE_DIRECT_WRITE, false);
			shaders.cubemap_filter.bind();
		}
		glViewport(0, 0, size, size);
		bind_quad_array();

		gl_wrapper.gl_active_texture(GL_TEXTURE2); //back to panorama

		for (int i = 0; i < 6; i++) {
			shaders.cubemap_filter.set_uniform(CubemapFilterShaderGLES2::FACE_ID, i);

			float roughness = mm_level >= 0 ? lod / (float)(mipmaps - 1) : 1;
			roughness = MIN(1.0, roughness); //keep max at 1
			shaders.cubemap_filter.set_uniform(CubemapFilterShaderGLES2::ROUGHNESS, roughness);
			shaders.cubemap_filter.set_uniform(CubemapFilterShaderGLES2::Z_FLIP, false);

			glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

			glCopyTexSubImage2D(_cube_side_enum[i], lod, 0, 0, 0, 0, size, size);
		}

		size >>= 1;

		mm_level--;

		lod++;
	}

	shaders.cubemap_filter.set_conditional(CubemapFilterShaderGLES2::USE_SOURCE_PANORAMA, false);
	shaders.cubemap_filter.set_conditional(CubemapFilterShaderGLES2::USE_DIRECT_WRITE, false);

	// restore ranges
	gl_wrapper.gl_active_texture(GL_TEXTURE2); //back to panorama

	glTexParameterf(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameterf(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameterf(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameterf(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	glBindTexture(GL_TEXTURE_2D, 0);
	gl_wrapper.gl_active_texture(GL_TEXTURE3); //back to panorama
	glBindTexture(GL_TEXTURE_2D, 0);
	gl_wrapper.gl_active_texture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, 0);
	gl_wrapper.gl_active_texture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, 0);

	//reset flags on Sky Texture that may have changed
	texture_set_flags(sky->panorama, texture->flags);

	// Framebuffer did its job. thank mr framebuffer
	gl_wrapper.gl_active_texture(GL_TEXTURE0); //back to panorama
	glBindFramebuffer(GL_FRAMEBUFFER, RasterizerStorageGLES2::system_fbo);
}

/* SHADER API */

RID RasterizerStorageGLES2::shader_create() {
	Shader *shader = memnew(Shader);
	shader->mode = VS::SHADER_SPATIAL;
	shader->shader = &scene->state.scene_shader;
	RID rid = shader_owner.make_rid(shader);
	_shader_make_dirty(shader);
	shader->self = rid;

	return rid;
}

void RasterizerStorageGLES2::_shader_make_dirty(Shader *p_shader) {
	if (p_shader->dirty_list.in_list()) {
		return;
	}

	_shader_dirty_list.add(&p_shader->dirty_list);
}

void RasterizerStorageGLES2::shader_set_code(RID p_shader, const String &p_code) {
	Shader *shader = shader_owner.getornull(p_shader);
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

	// TODO handle all shader types
	if (mode == VS::SHADER_CANVAS_ITEM) {
		shader->shader = &canvas->state.canvas_shader;

	} else if (mode == VS::SHADER_SPATIAL) {
		shader->shader = &scene->state.scene_shader;
	} else {
		return;
	}

	if (shader->custom_code_id == 0) {
		shader->custom_code_id = shader->shader->create_custom_shader();
	}

	_shader_make_dirty(shader);
}

String RasterizerStorageGLES2::shader_get_code(RID p_shader) const {
	const Shader *shader = shader_owner.get(p_shader);
	ERR_FAIL_COND_V(!shader, "");

	return shader->code;
}

void RasterizerStorageGLES2::_update_shader(Shader *p_shader) const {
	_shader_dirty_list.remove(&p_shader->dirty_list);

	p_shader->valid = false;

	p_shader->uniforms.clear();

	if (p_shader->code == String()) {
		return; //just invalid, but no error
	}

	ShaderCompilerGLES2::GeneratedCode gen_code;
	ShaderCompilerGLES2::IdentifierActions *actions = nullptr;

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

			if (p_shader->spatial.uses_screen_texture && p_shader->spatial.uses_depth_texture) {
				ERR_PRINT_ONCE("Using both SCREEN_TEXTURE and DEPTH_TEXTURE is not supported in GLES2");
			}

			if (p_shader->spatial.uses_depth_texture && !config.support_depth_texture) {
				ERR_PRINT_ONCE("Using DEPTH_TEXTURE is not permitted on this hardware, operation will fail.");
			}
		} break;

		default: {
			return;
		} break;
	}

	Error err = shaders.compiler.compile(p_shader->mode, p_shader->code, actions, p_shader->path, gen_code);
	if (err != OK) {
		return;
	}

	p_shader->shader->set_custom_shader_code(p_shader->custom_code_id, gen_code.vertex, gen_code.vertex_global, gen_code.fragment, gen_code.light, gen_code.fragment_global, gen_code.uniforms, gen_code.texture_uniforms, gen_code.custom_defines);

	p_shader->texture_count = gen_code.texture_uniforms.size();
	p_shader->texture_hints = gen_code.texture_hints;

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

	p_shader->shader->set_custom_shader(p_shader->custom_code_id);
	p_shader->shader->bind();

	// cache uniform locations

	for (SelfList<Material> *E = p_shader->materials.first(); E; E = E->next()) {
		_material_make_dirty(E->self());
	}

	p_shader->valid = true;
	p_shader->version++;
}

void RasterizerStorageGLES2::update_dirty_shaders() {
	while (_shader_dirty_list.first()) {
		_update_shader(_shader_dirty_list.first()->self());
	}
}

void RasterizerStorageGLES2::shader_get_param_list(RID p_shader, List<PropertyInfo> *p_param_list) const {
	Shader *shader = shader_owner.get(p_shader);
	ERR_FAIL_COND(!shader);

	if (shader->dirty_list.in_list()) {
		_update_shader(shader);
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
			case ShaderLanguage::TYPE_STRUCT: {
				pi.type = Variant::ARRAY;
			} break;
			case ShaderLanguage::TYPE_VOID: {
				pi.type = Variant::NIL;
			} break;

			case ShaderLanguage::TYPE_BOOL: {
				pi.type = Variant::BOOL;
			} break;

			// bool vectors
			case ShaderLanguage::TYPE_BVEC2: {
				pi.type = Variant::INT;
				pi.hint = PROPERTY_HINT_FLAGS;
				pi.hint_string = "x,y";
			} break;
			case ShaderLanguage::TYPE_BVEC3: {
				pi.type = Variant::INT;
				pi.hint = PROPERTY_HINT_FLAGS;
				pi.hint_string = "x,y,z";
			} break;
			case ShaderLanguage::TYPE_BVEC4: {
				pi.type = Variant::INT;
				pi.hint = PROPERTY_HINT_FLAGS;
				pi.hint_string = "x,y,z,w";
			} break;

				// int stuff
			case ShaderLanguage::TYPE_UINT:
			case ShaderLanguage::TYPE_INT: {
				pi.type = Variant::INT;

				if (u.hint == ShaderLanguage::ShaderNode::Uniform::HINT_RANGE) {
					pi.hint = PROPERTY_HINT_RANGE;
					pi.hint_string = rtos(u.hint_range[0]) + "," + rtos(u.hint_range[1]) + "," + rtos(u.hint_range[2]);
				}
			} break;

			case ShaderLanguage::TYPE_IVEC2:
			case ShaderLanguage::TYPE_UVEC2:
			case ShaderLanguage::TYPE_IVEC3:
			case ShaderLanguage::TYPE_UVEC3:
			case ShaderLanguage::TYPE_IVEC4:
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

			case ShaderLanguage::TYPE_VEC2: {
				pi.type = Variant::VECTOR2;
			} break;
			case ShaderLanguage::TYPE_VEC3: {
				pi.type = Variant::VECTOR3;
			} break;

			case ShaderLanguage::TYPE_VEC4: {
				if (u.hint == ShaderLanguage::ShaderNode::Uniform::HINT_COLOR) {
					pi.type = Variant::COLOR;
				} else {
					pi.type = Variant::PLANE;
				}
			} break;

			case ShaderLanguage::TYPE_MAT2: {
				pi.type = Variant::TRANSFORM2D;
			} break;

			case ShaderLanguage::TYPE_MAT3: {
				pi.type = Variant::BASIS;
			} break;

			case ShaderLanguage::TYPE_MAT4: {
				pi.type = Variant::TRANSFORM;
			} break;

			case ShaderLanguage::TYPE_SAMPLER2D:
			case ShaderLanguage::TYPE_SAMPLEREXT:
			case ShaderLanguage::TYPE_ISAMPLER2D:
			case ShaderLanguage::TYPE_USAMPLER2D: {
				pi.type = Variant::OBJECT;
				pi.hint = PROPERTY_HINT_RESOURCE_TYPE;
				pi.hint_string = "Texture";
			} break;

			case ShaderLanguage::TYPE_SAMPLERCUBE: {
				pi.type = Variant::OBJECT;
				pi.hint = PROPERTY_HINT_RESOURCE_TYPE;
				pi.hint_string = "CubeMap";
			} break;

			case ShaderLanguage::TYPE_SAMPLER2DARRAY:
			case ShaderLanguage::TYPE_ISAMPLER2DARRAY:
			case ShaderLanguage::TYPE_USAMPLER2DARRAY:
			case ShaderLanguage::TYPE_SAMPLER3D:
			case ShaderLanguage::TYPE_ISAMPLER3D:
			case ShaderLanguage::TYPE_USAMPLER3D: {
				// Not implemented in GLES2
			} break;

			default: {
			}
		}

		p_param_list->push_back(pi);
	}
}

void RasterizerStorageGLES2::shader_set_default_texture_param(RID p_shader, const StringName &p_name, RID p_texture) {
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

RID RasterizerStorageGLES2::shader_get_default_texture_param(RID p_shader, const StringName &p_name) const {
	const Shader *shader = shader_owner.get(p_shader);
	ERR_FAIL_COND_V(!shader, RID());

	const Map<StringName, RID>::Element *E = shader->default_textures.find(p_name);

	if (!E) {
		return RID();
	}

	return E->get();
}

void RasterizerStorageGLES2::shader_add_custom_define(RID p_shader, const String &p_define) {
	Shader *shader = shader_owner.get(p_shader);
	ERR_FAIL_COND(!shader);

	shader->shader->add_custom_define(p_define);

	_shader_make_dirty(shader);
}

void RasterizerStorageGLES2::shader_get_custom_defines(RID p_shader, Vector<String> *p_defines) const {
	Shader *shader = shader_owner.get(p_shader);
	ERR_FAIL_COND(!shader);

	shader->shader->get_custom_defines(p_defines);
}

void RasterizerStorageGLES2::shader_remove_custom_define(RID p_shader, const String &p_define) {
	Shader *shader = shader_owner.get(p_shader);
	ERR_FAIL_COND(!shader);

	shader->shader->remove_custom_define(p_define);

	_shader_make_dirty(shader);
}

/* COMMON MATERIAL API */

void RasterizerStorageGLES2::_material_make_dirty(Material *p_material) const {
	if (p_material->dirty_list.in_list()) {
		return;
	}

	_material_dirty_list.add(&p_material->dirty_list);
}

RID RasterizerStorageGLES2::material_create() {
	Material *material = memnew(Material);

	return material_owner.make_rid(material);
}

void RasterizerStorageGLES2::material_set_shader(RID p_material, RID p_shader) {
	Material *material = material_owner.get(p_material);
	ERR_FAIL_COND(!material);

	Shader *shader = shader_owner.getornull(p_shader);

	if (material->shader) {
		// if a shader is present, remove the old shader
		material->shader->materials.remove(&material->list);
	}

	material->shader = shader;

	if (shader) {
		shader->materials.add(&material->list);
	}

	_material_make_dirty(material);
}

RID RasterizerStorageGLES2::material_get_shader(RID p_material) const {
	const Material *material = material_owner.get(p_material);
	ERR_FAIL_COND_V(!material, RID());

	if (material->shader) {
		return material->shader->self;
	}

	return RID();
}

void RasterizerStorageGLES2::material_set_param(RID p_material, const StringName &p_param, const Variant &p_value) {
	Material *material = material_owner.get(p_material);
	ERR_FAIL_COND(!material);

	if (p_value.get_type() == Variant::NIL) {
		material->params.erase(p_param);
	} else {
		material->params[p_param] = p_value;
	}

	_material_make_dirty(material);
}

Variant RasterizerStorageGLES2::material_get_param(RID p_material, const StringName &p_param) const {
	const Material *material = material_owner.get(p_material);
	ERR_FAIL_COND_V(!material, RID());

	if (material->params.has(p_param)) {
		return material->params[p_param];
	}

	return material_get_param_default(p_material, p_param);
}

Variant RasterizerStorageGLES2::material_get_param_default(RID p_material, const StringName &p_param) const {
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

void RasterizerStorageGLES2::material_set_line_width(RID p_material, float p_width) {
	Material *material = material_owner.getornull(p_material);
	ERR_FAIL_COND(!material);

	material->line_width = p_width;
}

void RasterizerStorageGLES2::material_set_next_pass(RID p_material, RID p_next_material) {
	Material *material = material_owner.get(p_material);
	ERR_FAIL_COND(!material);

	material->next_pass = p_next_material;
}

void RasterizerStorageGLES2::material_blit(RID p_material, RID p_source_tex, RID p_output_tex, const Rect2 &p_source_rect, const Rect2 &p_output_rect) {
	Material *material = material_owner.getornull(p_material);
	ERR_FAIL_COND(!material);

	Shader *shader_res = material->shader;
	ERR_FAIL_COND(!shader_res);

	Texture *source_tex = nullptr;
	if (p_source_tex.is_valid()) {
		source_tex = texture_owner.getornull(p_source_tex);
		ERR_FAIL_COND(!source_tex);
	}

	Texture *output_tex = texture_owner.getornull(p_output_tex);
	ERR_FAIL_COND(!output_tex);

	if (material->dirty_list.in_list()) {
		_update_material(material);
	}

	Size2i src_tex_size;

	if (source_tex) {
		src_tex_size.x = source_tex->alloc_width;
		src_tex_size.y = source_tex->alloc_height;
	} else {
		src_tex_size.x = output_tex->alloc_width;
		src_tex_size.y = output_tex->alloc_height;
	}

	Point2i src_pos(p_source_rect.position);
	Size2i src_size(p_source_rect.size);
	Point2i out_pos(p_output_rect.position);
	Size2i out_size(p_output_rect.size);

	if (src_size.x == 0)
		src_size.x = src_tex_size.x;
	if (src_size.y == 0)
		src_size.y = src_tex_size.y;

	if (out_size.x == 0)
		out_size.x = output_tex->alloc_width;
	if (out_size.y == 0)
		out_size.y = output_tex->alloc_height;

	ERR_FAIL_COND_MSG(src_pos.x < 0 || src_pos.x + src_size.x > src_tex_size.x, "Source rect is out of bounds");
	ERR_FAIL_COND_MSG(src_pos.y < 0 || src_pos.y + src_size.y > src_tex_size.y, "Source rect is out of bounds");
	ERR_FAIL_COND_MSG(out_pos.x < 0 || out_pos.x + out_size.x > output_tex->alloc_width, "Output rect is out of bounds");
	ERR_FAIL_COND_MSG(out_pos.y < 0 || out_pos.y + out_size.y > output_tex->alloc_height, "Output rect is out of bounds");

	Rect2i src_rect(src_pos, src_size);
	Rect2i out_rect(out_pos, out_size);

	switch (shader_res->mode) {
		case VS::SHADER_CANVAS_ITEM: {
			canvas->material_blit(material, source_tex, output_tex, src_rect, out_rect);
		} break;
		case VS::SHADER_SPATIAL: {
			scene->material_blit(material, source_tex, output_tex, src_rect, out_rect);
		} break;
		case VS::SHADER_PARTICLES:
			break;
		case VS::SHADER_MAX:
			break;
	}
}

bool RasterizerStorageGLES2::material_is_animated(RID p_material) {
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

bool RasterizerStorageGLES2::material_casts_shadows(RID p_material) {
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

bool RasterizerStorageGLES2::material_uses_tangents(RID p_material) {
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

bool RasterizerStorageGLES2::material_uses_ensure_correct_normals(RID p_material) {
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

void RasterizerStorageGLES2::material_add_instance_owner(RID p_material, RasterizerScene::InstanceBase *p_instance) {
	Material *material = material_owner.getornull(p_material);
	ERR_FAIL_COND(!material);

	Map<RasterizerScene::InstanceBase *, int>::Element *E = material->instance_owners.find(p_instance);
	if (E) {
		E->get()++;
	} else {
		material->instance_owners[p_instance] = 1;
	}
}

void RasterizerStorageGLES2::material_remove_instance_owner(RID p_material, RasterizerScene::InstanceBase *p_instance) {
	Material *material = material_owner.getornull(p_material);
	ERR_FAIL_COND(!material);

	Map<RasterizerScene::InstanceBase *, int>::Element *E = material->instance_owners.find(p_instance);
	ERR_FAIL_COND(!E);

	E->get()--;

	if (E->get() == 0) {
		material->instance_owners.erase(E);
	}
}

void RasterizerStorageGLES2::material_set_render_priority(RID p_material, int priority) {
	ERR_FAIL_COND(priority < VS::MATERIAL_RENDER_PRIORITY_MIN);
	ERR_FAIL_COND(priority > VS::MATERIAL_RENDER_PRIORITY_MAX);

	Material *material = material_owner.get(p_material);
	ERR_FAIL_COND(!material);

	material->render_priority = priority;
}

void RasterizerStorageGLES2::_update_material(Material *p_material) {
	if (p_material->dirty_list.in_list()) {
		_material_dirty_list.remove(&p_material->dirty_list);
	}

	if (p_material->shader && p_material->shader->dirty_list.in_list()) {
		_update_shader(p_material->shader);
	}

	if (p_material->shader && !p_material->shader->valid) {
		return;
	}

	{
		bool can_cast_shadow = false;
		bool is_animated = false;

		if (p_material->shader && p_material->shader->mode == VS::SHADER_SPATIAL) {
			if (p_material->shader->spatial.blend_mode == Shader::Spatial::BLEND_MODE_MIX &&
					(!(p_material->shader->spatial.uses_alpha && !p_material->shader->spatial.uses_alpha_scissor) || p_material->shader->spatial.depth_draw_mode == Shader::Spatial::DEPTH_DRAW_ALPHA_PREPASS)) {
				can_cast_shadow = true;
			}

			if (p_material->shader->spatial.uses_discard && p_material->shader->uses_fragment_time) {
				is_animated = true;
			}

			if (p_material->shader->spatial.uses_vertex && p_material->shader->uses_vertex_time) {
				is_animated = true;
			}

			if (can_cast_shadow != p_material->can_cast_shadow_cache || is_animated != p_material->is_animated_cache) {
				p_material->can_cast_shadow_cache = can_cast_shadow;
				p_material->is_animated_cache = is_animated;

				for (Map<Geometry *, int>::Element *E = p_material->geometry_owners.front(); E; E = E->next()) {
					E->key()->material_changed_notify();
				}

				for (Map<RasterizerScene::InstanceBase *, int>::Element *E = p_material->instance_owners.front(); E; E = E->next()) {
					E->key()->base_changed(false, true);
				}
			}
		}
	}

	// uniforms and other things will be set in the use_material method in ShaderGLES2

	if (p_material->shader && p_material->shader->texture_count > 0) {
		p_material->textures.resize(p_material->shader->texture_count);

		for (Map<StringName, ShaderLanguage::ShaderNode::Uniform>::Element *E = p_material->shader->uniforms.front(); E; E = E->next()) {
			if (E->get().texture_order < 0) {
				continue; // not a texture, does not go here
			}

			RID texture;

			Map<StringName, Variant>::Element *V = p_material->params.find(E->key());

			if (V) {
				texture = V->get();
			}

			if (!texture.is_valid()) {
				Map<StringName, RID>::Element *W = p_material->shader->default_textures.find(E->key());

				if (W) {
					texture = W->get();
				}
			}

			p_material->textures.write[E->get().texture_order] = Pair<StringName, RID>(E->key(), texture);
		}
	} else {
		p_material->textures.clear();
	}
}

void RasterizerStorageGLES2::_material_add_geometry(RID p_material, Geometry *p_geometry) {
	Material *material = material_owner.getornull(p_material);
	ERR_FAIL_COND(!material);

	Map<Geometry *, int>::Element *I = material->geometry_owners.find(p_geometry);

	if (I) {
		I->get()++;
	} else {
		material->geometry_owners[p_geometry] = 1;
	}
}

void RasterizerStorageGLES2::_material_remove_geometry(RID p_material, Geometry *p_geometry) {
	Material *material = material_owner.getornull(p_material);
	ERR_FAIL_COND(!material);

	Map<Geometry *, int>::Element *I = material->geometry_owners.find(p_geometry);
	ERR_FAIL_COND(!I);

	I->get()--;

	if (I->get() == 0) {
		material->geometry_owners.erase(I);
	}
}

void RasterizerStorageGLES2::update_dirty_materials() {
	while (_material_dirty_list.first()) {
		Material *material = _material_dirty_list.first()->self();
		_update_material(material);
	}
}

/* MESH API */

RID RasterizerStorageGLES2::mesh_create() {
	Mesh *mesh = memnew(Mesh);

	return mesh_owner.make_rid(mesh);
}

static PoolVector<uint8_t> _unpack_half_floats(const PoolVector<uint8_t> &array, uint32_t &format, int p_vertices) {
	uint32_t p_format = format;

	static int src_size[VS::ARRAY_MAX];
	static int dst_size[VS::ARRAY_MAX];
	static int to_convert[VS::ARRAY_MAX];

	int src_stride = 0;
	int dst_stride = 0;

	for (int i = 0; i < VS::ARRAY_MAX; i++) {
		to_convert[i] = 0;
		if (!(p_format & (1 << i))) {
			src_size[i] = 0;
			dst_size[i] = 0;
			continue;
		}

		switch (i) {
			case VS::ARRAY_VERTEX: {
				if (p_format & VS::ARRAY_COMPRESS_VERTEX) {
					if (p_format & VS::ARRAY_FLAG_USE_2D_VERTICES) {
						src_size[i] = 4;
						dst_size[i] = 8;
						to_convert[i] = 2;
					} else {
						src_size[i] = 8;
						dst_size[i] = 12;
						to_convert[i] = 3;
					}

					format &= ~VS::ARRAY_COMPRESS_VERTEX;
				} else {
					if (p_format & VS::ARRAY_FLAG_USE_2D_VERTICES) {
						src_size[i] = 8;
						dst_size[i] = 8;
					} else {
						src_size[i] = 12;
						dst_size[i] = 12;
					}
				}

			} break;
			case VS::ARRAY_NORMAL: {
				if (p_format & VS::ARRAY_FLAG_USE_OCTAHEDRAL_COMPRESSION) {
					src_size[i] = 4;
					dst_size[i] = 4;
				} else {
					if (p_format & VS::ARRAY_COMPRESS_NORMAL) {
						src_size[i] = 4;
						dst_size[i] = 4;
					} else {
						src_size[i] = 12;
						dst_size[i] = 12;
					}
				}

			} break;
			case VS::ARRAY_TANGENT: {
				if (p_format & VS::ARRAY_FLAG_USE_OCTAHEDRAL_COMPRESSION) {
					if (!(p_format & VS::ARRAY_COMPRESS_TANGENT && p_format & VS::ARRAY_COMPRESS_NORMAL)) {
						src_size[VS::ARRAY_NORMAL] = 8;
						dst_size[VS::ARRAY_NORMAL] = 8;

						// These must be incremented manually,
						// as we are modifying a previous attribute size.
						src_stride += 4;
						dst_stride += 4;
					}
					src_size[i] = 0;
					dst_size[i] = 0;
				} else {
					if (p_format & VS::ARRAY_COMPRESS_TANGENT) {
						src_size[i] = 4;
						dst_size[i] = 4;
					} else {
						src_size[i] = 16;
						dst_size[i] = 16;
					}
				}

			} break;
			case VS::ARRAY_COLOR: {
				if (p_format & VS::ARRAY_COMPRESS_COLOR) {
					src_size[i] = 4;
					dst_size[i] = 4;
				} else {
					src_size[i] = 16;
					dst_size[i] = 16;
				}

			} break;
			case VS::ARRAY_TEX_UV: {
				if (p_format & VS::ARRAY_COMPRESS_TEX_UV) {
					src_size[i] = 4;
					to_convert[i] = 2;
					format &= ~VS::ARRAY_COMPRESS_TEX_UV;
				} else {
					src_size[i] = 8;
				}

				dst_size[i] = 8;

			} break;
			case VS::ARRAY_TEX_UV2: {
				if (p_format & VS::ARRAY_COMPRESS_TEX_UV2) {
					src_size[i] = 4;
					to_convert[i] = 2;
					format &= ~VS::ARRAY_COMPRESS_TEX_UV2;
				} else {
					src_size[i] = 8;
				}

				dst_size[i] = 8;

			} break;
			case VS::ARRAY_BONES: {
				if (p_format & VS::ARRAY_FLAG_USE_16_BIT_BONES) {
					src_size[i] = 8;
					dst_size[i] = 8;
				} else {
					src_size[i] = 4;
					dst_size[i] = 4;
				}

			} break;
			case VS::ARRAY_WEIGHTS: {
				if (p_format & VS::ARRAY_COMPRESS_WEIGHTS) {
					src_size[i] = 8;
					dst_size[i] = 8;
				} else {
					src_size[i] = 16;
					dst_size[i] = 16;
				}

			} break;
			case VS::ARRAY_INDEX: {
				src_size[i] = 0;
				dst_size[i] = 0;

			} break;
		}

		src_stride += src_size[i];
		dst_stride += dst_size[i];
	}

	PoolVector<uint8_t> ret;
	ret.resize(p_vertices * dst_stride);

	PoolVector<uint8_t>::Read r = array.read();
	PoolVector<uint8_t>::Write w = ret.write();

	int src_offset = 0;
	int dst_offset = 0;

	for (int i = 0; i < VS::ARRAY_MAX; i++) {
		if (src_size[i] == 0) {
			continue; //no go
		}
		const uint8_t *rptr = r.ptr();
		uint8_t *wptr = w.ptr();
		if (to_convert[i]) { //converting

			for (int j = 0; j < p_vertices; j++) {
				const uint16_t *src = (const uint16_t *)&rptr[src_stride * j + src_offset];
				float *dst = (float *)&wptr[dst_stride * j + dst_offset];

				for (int k = 0; k < to_convert[i]; k++) {
					dst[k] = Math::half_to_float(src[k]);
				}
			}

		} else {
			//just copy
			for (int j = 0; j < p_vertices; j++) {
				for (int k = 0; k < src_size[i]; k++) {
					wptr[dst_stride * j + dst_offset + k] = rptr[src_stride * j + src_offset + k];
				}
			}
		}

		src_offset += src_size[i];
		dst_offset += dst_size[i];
	}

	r.release();
	w.release();

	return ret;
}

void RasterizerStorageGLES2::mesh_add_surface(RID p_mesh, uint32_t p_format, VS::PrimitiveType p_primitive, const PoolVector<uint8_t> &p_array, int p_vertex_count, const PoolVector<uint8_t> &p_index_array, int p_index_count, const AABB &p_aabb, const Vector<PoolVector<uint8_t>> &p_blend_shapes, const Vector<AABB> &p_bone_aabbs) {
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
	bool uses_half_float = false;

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
					attribs[i].type = _GL_HALF_FLOAT_OES;
					positions_stride += attribs[i].size * 2;
					uses_half_float = true;
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
					// normal will always be oct32 encoded
					// UNLESS tangent exists and is also compressed
					// then it will be oct16 encoded along with tangent
					attribs[i].normalized = GL_TRUE;
					attribs[i].size = 2;
					attribs[i].type = GL_SHORT;
					attributes_stride += 4;
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
					attribs[i].type = _GL_HALF_FLOAT_OES;
					attributes_stride += 4;
					uses_half_float = true;
				} else {
					attribs[i].type = GL_FLOAT;
					attributes_stride += 8;
				}

				attribs[i].normalized = GL_FALSE;

			} break;
			case VS::ARRAY_TEX_UV2: {
				attribs[i].size = 2;

				if (p_format & VS::ARRAY_COMPRESS_TEX_UV2) {
					attribs[i].type = _GL_HALF_FLOAT_OES;
					attributes_stride += 4;
					uses_half_float = true;
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
	PoolVector<uint8_t> array = p_array;

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

	if (!config.support_half_float_vertices && uses_half_float) {
		uint32_t new_format = p_format;
		PoolVector<uint8_t> unpacked_array = _unpack_half_floats(array, new_format, p_vertex_count);

		Vector<PoolVector<uint8_t>> unpacked_blend_shapes;
		for (int i = 0; i < p_blend_shapes.size(); i++) {
			uint32_t temp_format = p_format; // Just throw this away as it will be the same as new_format
			unpacked_blend_shapes.push_back(_unpack_half_floats(p_blend_shapes[i], temp_format, p_vertex_count));
		}

		mesh_add_surface(p_mesh, new_format, p_primitive, unpacked_array, p_vertex_count, p_index_array, p_index_count, p_aabb, unpacked_blend_shapes, p_bone_aabbs);
		return; //do not go any further, above function used unpacked stuff will be used instead.
	}

	if (p_format & VS::ARRAY_FORMAT_INDEX) {
		index_array_size = attribs[VS::ARRAY_INDEX].stride * p_index_count;
	}

	ERR_FAIL_COND(p_index_array.size() != index_array_size);

	ERR_FAIL_COND(p_blend_shapes.size() != mesh->blend_shape_count);

	for (int i = 0; i < p_blend_shapes.size(); i++) {
		ERR_FAIL_COND(p_blend_shapes[i].size() != array_size);
	}

	// all valid, create stuff

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
	surface->blend_shape_data = p_blend_shapes;

	surface->data = array;
	surface->index_data = p_index_array;
	surface->total_data_size += surface->array_byte_size + surface->index_array_byte_size;

	for (int i = 0; i < surface->skeleton_bone_used.size(); i++) {
		surface->skeleton_bone_used.write[i] = !(surface->skeleton_bone_aabb[i].size.x < 0 || surface->skeleton_bone_aabb[i].size.y < 0 || surface->skeleton_bone_aabb[i].size.z < 0);
	}

	for (int i = 0; i < VS::ARRAY_MAX; i++) {
		surface->attribs[i] = attribs[i];
	}

	// Okay, now the OpenGL stuff, wheeeeey \o/
	{
		PoolVector<uint8_t>::Read vr = array.read();

		glGenBuffers(1, &surface->vertex_id);
		glBindBuffer(GL_ARRAY_BUFFER, surface->vertex_id);
		glBufferData(GL_ARRAY_BUFFER, array_size, vr.ptr(), (p_format & VS::ARRAY_FLAG_USE_DYNAMIC_UPDATE) ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW);

		glBindBuffer(GL_ARRAY_BUFFER, 0);

		if (p_format & VS::ARRAY_FORMAT_INDEX) {
			PoolVector<uint8_t>::Read ir = p_index_array.read();

			glGenBuffers(1, &surface->index_id);
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, surface->index_id);
			glBufferData(GL_ELEMENT_ARRAY_BUFFER, index_array_size, ir.ptr(), GL_STATIC_DRAW);
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
		} else {
			surface->index_id = 0;
		}

		// TODO generate wireframes

		// Make one blend shape buffer per surface
		{
			surface->blend_shape_buffer_size = 0;
			glGenBuffers(1, &surface->blend_shape_buffer_id);
		}
	}

	mesh->surfaces.push_back(surface);
	mesh->instance_change_notify(true, true);

	info.vertex_mem += surface->total_data_size;
}

void RasterizerStorageGLES2::mesh_set_blend_shape_count(RID p_mesh, int p_amount) {
	Mesh *mesh = mesh_owner.getornull(p_mesh);
	ERR_FAIL_COND(!mesh);

	ERR_FAIL_COND(mesh->surfaces.size() != 0);
	ERR_FAIL_COND(p_amount < 0);

	mesh->blend_shape_count = p_amount;
	mesh->instance_change_notify(true, false);
	if (!mesh->update_list.in_list()) {
		blend_shapes_update_list.add(&mesh->update_list);
	}
}

int RasterizerStorageGLES2::mesh_get_blend_shape_count(RID p_mesh) const {
	const Mesh *mesh = mesh_owner.getornull(p_mesh);
	ERR_FAIL_COND_V(!mesh, 0);
	return mesh->blend_shape_count;
}

void RasterizerStorageGLES2::mesh_set_blend_shape_mode(RID p_mesh, VS::BlendShapeMode p_mode) {
	Mesh *mesh = mesh_owner.getornull(p_mesh);
	ERR_FAIL_COND(!mesh);

	mesh->blend_shape_mode = p_mode;
	if (!mesh->update_list.in_list()) {
		blend_shapes_update_list.add(&mesh->update_list);
	}
}

VS::BlendShapeMode RasterizerStorageGLES2::mesh_get_blend_shape_mode(RID p_mesh) const {
	const Mesh *mesh = mesh_owner.getornull(p_mesh);
	ERR_FAIL_COND_V(!mesh, VS::BLEND_SHAPE_MODE_NORMALIZED);

	return mesh->blend_shape_mode;
}

void RasterizerStorageGLES2::mesh_set_blend_shape_values(RID p_mesh, PoolVector<float> p_values) {
	Mesh *mesh = mesh_owner.getornull(p_mesh);
	ERR_FAIL_COND(!mesh);

	mesh->blend_shape_values = p_values;
	if (!mesh->update_list.in_list()) {
		blend_shapes_update_list.add(&mesh->update_list);
	}
}

PoolVector<float> RasterizerStorageGLES2::mesh_get_blend_shape_values(RID p_mesh) const {
	const Mesh *mesh = mesh_owner.getornull(p_mesh);
	ERR_FAIL_COND_V(!mesh, PoolVector<float>());

	return mesh->blend_shape_values;
}

void RasterizerStorageGLES2::mesh_surface_update_region(RID p_mesh, int p_surface, int p_offset, const PoolVector<uint8_t> &p_data) {
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

void RasterizerStorageGLES2::mesh_surface_set_material(RID p_mesh, int p_surface, RID p_material) {
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

RID RasterizerStorageGLES2::mesh_surface_get_material(RID p_mesh, int p_surface) const {
	const Mesh *mesh = mesh_owner.getornull(p_mesh);
	ERR_FAIL_COND_V(!mesh, RID());
	ERR_FAIL_INDEX_V(p_surface, mesh->surfaces.size(), RID());

	return mesh->surfaces[p_surface]->material;
}

int RasterizerStorageGLES2::mesh_surface_get_array_len(RID p_mesh, int p_surface) const {
	const Mesh *mesh = mesh_owner.getornull(p_mesh);
	ERR_FAIL_COND_V(!mesh, 0);
	ERR_FAIL_INDEX_V(p_surface, mesh->surfaces.size(), 0);

	return mesh->surfaces[p_surface]->array_len;
}

int RasterizerStorageGLES2::mesh_surface_get_array_index_len(RID p_mesh, int p_surface) const {
	const Mesh *mesh = mesh_owner.getornull(p_mesh);
	ERR_FAIL_COND_V(!mesh, 0);
	ERR_FAIL_INDEX_V(p_surface, mesh->surfaces.size(), 0);

	return mesh->surfaces[p_surface]->index_array_len;
}

PoolVector<uint8_t> RasterizerStorageGLES2::mesh_surface_get_array(RID p_mesh, int p_surface) const {
	const Mesh *mesh = mesh_owner.getornull(p_mesh);
	ERR_FAIL_COND_V(!mesh, PoolVector<uint8_t>());
	ERR_FAIL_INDEX_V(p_surface, mesh->surfaces.size(), PoolVector<uint8_t>());

	Surface *surface = mesh->surfaces[p_surface];

	return surface->data;
}

PoolVector<uint8_t> RasterizerStorageGLES2::mesh_surface_get_index_array(RID p_mesh, int p_surface) const {
	const Mesh *mesh = mesh_owner.getornull(p_mesh);
	ERR_FAIL_COND_V(!mesh, PoolVector<uint8_t>());
	ERR_FAIL_INDEX_V(p_surface, mesh->surfaces.size(), PoolVector<uint8_t>());

	Surface *surface = mesh->surfaces[p_surface];

	return surface->index_data;
}

uint32_t RasterizerStorageGLES2::mesh_surface_get_format(RID p_mesh, int p_surface) const {
	const Mesh *mesh = mesh_owner.getornull(p_mesh);

	ERR_FAIL_COND_V(!mesh, 0);
	ERR_FAIL_INDEX_V(p_surface, mesh->surfaces.size(), 0);

	return mesh->surfaces[p_surface]->format;
}

VS::PrimitiveType RasterizerStorageGLES2::mesh_surface_get_primitive_type(RID p_mesh, int p_surface) const {
	const Mesh *mesh = mesh_owner.getornull(p_mesh);
	ERR_FAIL_COND_V(!mesh, VS::PRIMITIVE_MAX);
	ERR_FAIL_INDEX_V(p_surface, mesh->surfaces.size(), VS::PRIMITIVE_MAX);

	return mesh->surfaces[p_surface]->primitive;
}

AABB RasterizerStorageGLES2::mesh_surface_get_aabb(RID p_mesh, int p_surface) const {
	const Mesh *mesh = mesh_owner.getornull(p_mesh);
	ERR_FAIL_COND_V(!mesh, AABB());
	ERR_FAIL_INDEX_V(p_surface, mesh->surfaces.size(), AABB());

	return mesh->surfaces[p_surface]->aabb;
}

Vector<PoolVector<uint8_t>> RasterizerStorageGLES2::mesh_surface_get_blend_shapes(RID p_mesh, int p_surface) const {
	const Mesh *mesh = mesh_owner.getornull(p_mesh);
	ERR_FAIL_COND_V(!mesh, Vector<PoolVector<uint8_t>>());
	ERR_FAIL_INDEX_V(p_surface, mesh->surfaces.size(), Vector<PoolVector<uint8_t>>());

	return mesh->surfaces[p_surface]->blend_shape_data;
}
Vector<AABB> RasterizerStorageGLES2::mesh_surface_get_skeleton_aabb(RID p_mesh, int p_surface) const {
	const Mesh *mesh = mesh_owner.getornull(p_mesh);
	ERR_FAIL_COND_V(!mesh, Vector<AABB>());
	ERR_FAIL_INDEX_V(p_surface, mesh->surfaces.size(), Vector<AABB>());

	return mesh->surfaces[p_surface]->skeleton_bone_aabb;
}

void RasterizerStorageGLES2::mesh_remove_surface(RID p_mesh, int p_surface) {
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

	glDeleteBuffers(1, &surface->blend_shape_buffer_id);

	info.vertex_mem -= surface->total_data_size;

	memdelete(surface);

	mesh->surfaces.remove(p_surface);

	mesh->instance_change_notify(true, true);
}

int RasterizerStorageGLES2::mesh_get_surface_count(RID p_mesh) const {
	const Mesh *mesh = mesh_owner.getornull(p_mesh);
	ERR_FAIL_COND_V(!mesh, 0);
	return mesh->surfaces.size();
}

void RasterizerStorageGLES2::mesh_set_custom_aabb(RID p_mesh, const AABB &p_aabb) {
	Mesh *mesh = mesh_owner.getornull(p_mesh);
	ERR_FAIL_COND(!mesh);

	mesh->custom_aabb = p_aabb;
	mesh->instance_change_notify(true, false);
}

AABB RasterizerStorageGLES2::mesh_get_custom_aabb(RID p_mesh) const {
	const Mesh *mesh = mesh_owner.getornull(p_mesh);
	ERR_FAIL_COND_V(!mesh, AABB());

	return mesh->custom_aabb;
}

AABB RasterizerStorageGLES2::mesh_get_aabb(RID p_mesh, RID p_skeleton) const {
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
				const float *texture = sk->bone_data.ptr();

				bool first = true;
				if (sk->use_2d) {
					for (int j = 0; j < bs; j++) {
						if (!skused[j]) {
							continue;
						}

						int base_ofs = j * 2 * 4;

						Transform mtx;

						mtx.basis[0].x = texture[base_ofs + 0];
						mtx.basis[0].y = texture[base_ofs + 1];
						mtx.origin.x = texture[base_ofs + 3];
						base_ofs += 4;
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

						int base_ofs = j * 3 * 4;

						Transform mtx;

						mtx.basis[0].x = texture[base_ofs + 0];
						mtx.basis[0].y = texture[base_ofs + 1];
						mtx.basis[0].z = texture[base_ofs + 2];
						mtx.origin.x = texture[base_ofs + 3];
						base_ofs += 4;
						mtx.basis[1].x = texture[base_ofs + 0];
						mtx.basis[1].y = texture[base_ofs + 1];
						mtx.basis[1].z = texture[base_ofs + 2];
						mtx.origin.y = texture[base_ofs + 3];
						base_ofs += 4;
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
void RasterizerStorageGLES2::mesh_clear(RID p_mesh) {
	Mesh *mesh = mesh_owner.getornull(p_mesh);
	ERR_FAIL_COND(!mesh);

	while (mesh->surfaces.size()) {
		mesh_remove_surface(p_mesh, 0);
	}
}

/* MULTIMESH API */

RID RasterizerStorageGLES2::_multimesh_create() {
	MultiMesh *multimesh = memnew(MultiMesh);
	return multimesh_owner.make_rid(multimesh);
}

void RasterizerStorageGLES2::_multimesh_allocate(RID p_multimesh, int p_instances, VS::MultimeshTransformFormat p_transform_format, VS::MultimeshColorFormat p_color_format, VS::MultimeshCustomDataFormat p_data) {
	MultiMesh *multimesh = multimesh_owner.getornull(p_multimesh);
	ERR_FAIL_COND(!multimesh);

	if (multimesh->size == p_instances && multimesh->transform_format == p_transform_format && multimesh->color_format == p_color_format && multimesh->custom_data_format == p_data) {
		return;
	}

	multimesh->size = p_instances;

	multimesh->color_format = p_color_format;
	multimesh->transform_format = p_transform_format;
	multimesh->custom_data_format = p_data;

	if (multimesh->size) {
		multimesh->data.resize(0);
	}

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

	for (int i = 0; i < p_instances * format_floats; i += format_floats) {
		int color_from = 0;
		int custom_data_from = 0;

		if (multimesh->transform_format == VS::MULTIMESH_TRANSFORM_2D) {
			multimesh->data.write[i + 0] = 1.0;
			multimesh->data.write[i + 1] = 0.0;
			multimesh->data.write[i + 2] = 0.0;
			multimesh->data.write[i + 3] = 0.0;
			multimesh->data.write[i + 4] = 0.0;
			multimesh->data.write[i + 5] = 1.0;
			multimesh->data.write[i + 6] = 0.0;
			multimesh->data.write[i + 7] = 0.0;
			color_from = 8;
			custom_data_from = 8;
		} else {
			multimesh->data.write[i + 0] = 1.0;
			multimesh->data.write[i + 1] = 0.0;
			multimesh->data.write[i + 2] = 0.0;
			multimesh->data.write[i + 3] = 0.0;
			multimesh->data.write[i + 4] = 0.0;
			multimesh->data.write[i + 5] = 1.0;
			multimesh->data.write[i + 6] = 0.0;
			multimesh->data.write[i + 7] = 0.0;
			multimesh->data.write[i + 8] = 0.0;
			multimesh->data.write[i + 9] = 0.0;
			multimesh->data.write[i + 10] = 1.0;
			multimesh->data.write[i + 11] = 0.0;
			color_from = 12;
			custom_data_from = 12;
		}

		if (multimesh->color_format == VS::MULTIMESH_COLOR_8BIT) {
			union {
				uint32_t colu;
				float colf;
			} cu;

			cu.colu = 0xFFFFFFFF;
			multimesh->data.write[i + color_from + 0] = cu.colf;
			custom_data_from = color_from + 1;
		} else if (multimesh->color_format == VS::MULTIMESH_COLOR_FLOAT) {
			multimesh->data.write[i + color_from + 0] = 1.0;
			multimesh->data.write[i + color_from + 1] = 1.0;
			multimesh->data.write[i + color_from + 2] = 1.0;
			multimesh->data.write[i + color_from + 3] = 1.0;
			custom_data_from = color_from + 4;
		}

		if (multimesh->custom_data_format == VS::MULTIMESH_CUSTOM_DATA_8BIT) {
			union {
				uint32_t colu;
				float colf;
			} cu;

			cu.colu = 0;
			multimesh->data.write[i + custom_data_from + 0] = cu.colf;
		} else if (multimesh->custom_data_format == VS::MULTIMESH_CUSTOM_DATA_FLOAT) {
			multimesh->data.write[i + custom_data_from + 0] = 0.0;
			multimesh->data.write[i + custom_data_from + 1] = 0.0;
			multimesh->data.write[i + custom_data_from + 2] = 0.0;
			multimesh->data.write[i + custom_data_from + 3] = 0.0;
		}
	}

	multimesh->dirty_aabb = true;
	multimesh->dirty_data = true;

	if (!multimesh->update_list.in_list()) {
		multimesh_update_list.add(&multimesh->update_list);
	}
}

int RasterizerStorageGLES2::_multimesh_get_instance_count(RID p_multimesh) const {
	MultiMesh *multimesh = multimesh_owner.getornull(p_multimesh);
	ERR_FAIL_COND_V(!multimesh, 0);

	return multimesh->size;
}

void RasterizerStorageGLES2::_multimesh_set_mesh(RID p_multimesh, RID p_mesh) {
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

void RasterizerStorageGLES2::_multimesh_instance_set_transform(RID p_multimesh, int p_index, const Transform &p_transform) {
	MultiMesh *multimesh = multimesh_owner.getornull(p_multimesh);
	ERR_FAIL_COND(!multimesh);
	ERR_FAIL_INDEX(p_index, multimesh->size);
	ERR_FAIL_COND(multimesh->transform_format == VS::MULTIMESH_TRANSFORM_2D);

	int stride = multimesh->color_floats + multimesh->custom_data_floats + multimesh->xform_floats;

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

void RasterizerStorageGLES2::_multimesh_instance_set_transform_2d(RID p_multimesh, int p_index, const Transform2D &p_transform) {
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

void RasterizerStorageGLES2::_multimesh_instance_set_color(RID p_multimesh, int p_index, const Color &p_color) {
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

void RasterizerStorageGLES2::_multimesh_instance_set_custom_data(RID p_multimesh, int p_index, const Color &p_custom_data) {
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

RID RasterizerStorageGLES2::_multimesh_get_mesh(RID p_multimesh) const {
	MultiMesh *multimesh = multimesh_owner.getornull(p_multimesh);
	ERR_FAIL_COND_V(!multimesh, RID());

	return multimesh->mesh;
}

Transform RasterizerStorageGLES2::_multimesh_instance_get_transform(RID p_multimesh, int p_index) const {
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

Transform2D RasterizerStorageGLES2::_multimesh_instance_get_transform_2d(RID p_multimesh, int p_index) const {
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

Color RasterizerStorageGLES2::_multimesh_instance_get_color(RID p_multimesh, int p_index) const {
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

Color RasterizerStorageGLES2::_multimesh_instance_get_custom_data(RID p_multimesh, int p_index) const {
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

void RasterizerStorageGLES2::_multimesh_set_as_bulk_array(RID p_multimesh, const PoolVector<float> &p_array) {
	MultiMesh *multimesh = multimesh_owner.getornull(p_multimesh);
	ERR_FAIL_COND(!multimesh);
	ERR_FAIL_COND(!multimesh->data.ptr());

	int dsize = multimesh->data.size();

	ERR_FAIL_COND(dsize != p_array.size());

	PoolVector<float>::Read r = p_array.read();
	ERR_FAIL_COND(!r.ptr());
	memcpy(multimesh->data.ptrw(), r.ptr(), dsize * sizeof(float));

	multimesh->dirty_data = true;
	multimesh->dirty_aabb = true;

	if (!multimesh->update_list.in_list()) {
		multimesh_update_list.add(&multimesh->update_list);
	}
}

void RasterizerStorageGLES2::_multimesh_set_visible_instances(RID p_multimesh, int p_visible) {
	MultiMesh *multimesh = multimesh_owner.getornull(p_multimesh);
	ERR_FAIL_COND(!multimesh);

	multimesh->visible_instances = p_visible;
}

int RasterizerStorageGLES2::_multimesh_get_visible_instances(RID p_multimesh) const {
	MultiMesh *multimesh = multimesh_owner.getornull(p_multimesh);
	ERR_FAIL_COND_V(!multimesh, -1);

	return multimesh->visible_instances;
}

AABB RasterizerStorageGLES2::_multimesh_get_aabb(RID p_multimesh) const {
	MultiMesh *multimesh = multimesh_owner.getornull(p_multimesh);
	ERR_FAIL_COND_V(!multimesh, AABB());

	const_cast<RasterizerStorageGLES2 *>(this)->update_dirty_multimeshes();

	return multimesh->aabb;
}

RasterizerStorage::MMInterpolator *RasterizerStorageGLES2::_multimesh_get_interpolator(RID p_multimesh) const {
	MultiMesh *multimesh = multimesh_owner.getornull(p_multimesh);
	ERR_FAIL_COND_V(!multimesh, nullptr);

	return &multimesh->interpolator;
}

void RasterizerStorageGLES2::multimesh_attach_canvas_item(RID p_multimesh, RID p_canvas_item, bool p_attach) {
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

void RasterizerStorageGLES2::update_dirty_multimeshes() {
	while (multimesh_update_list.first()) {
		MultiMesh *multimesh = multimesh_update_list.first()->self();

		if (multimesh->size && multimesh->dirty_aabb) {
			AABB mesh_aabb;

			if (multimesh->mesh.is_valid()) {
				mesh_aabb = mesh_get_aabb(multimesh->mesh, RID());
			}

			mesh_aabb.size += Vector3(0.001, 0.001, 0.001); //in case mesh is empty in one of the sides

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

RID RasterizerStorageGLES2::immediate_create() {
	Immediate *im = memnew(Immediate);
	return immediate_owner.make_rid(im);
}

void RasterizerStorageGLES2::immediate_begin(RID p_immediate, VS::PrimitiveType p_primitive, RID p_texture) {
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

void RasterizerStorageGLES2::immediate_vertex(RID p_immediate, const Vector3 &p_vertex) {
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
		c->uv2s.push_back(chunk_uv2);
	}
	im->mask |= VS::ARRAY_FORMAT_VERTEX;
	c->vertices.push_back(p_vertex);
}

void RasterizerStorageGLES2::immediate_normal(RID p_immediate, const Vector3 &p_normal) {
	Immediate *im = immediate_owner.get(p_immediate);
	ERR_FAIL_COND(!im);
	ERR_FAIL_COND(!im->building);

	im->mask |= VS::ARRAY_FORMAT_NORMAL;
	chunk_normal = p_normal;
}

void RasterizerStorageGLES2::immediate_tangent(RID p_immediate, const Plane &p_tangent) {
	Immediate *im = immediate_owner.get(p_immediate);
	ERR_FAIL_COND(!im);
	ERR_FAIL_COND(!im->building);

	im->mask |= VS::ARRAY_FORMAT_TANGENT;
	chunk_tangent = p_tangent;
}

void RasterizerStorageGLES2::immediate_color(RID p_immediate, const Color &p_color) {
	Immediate *im = immediate_owner.get(p_immediate);
	ERR_FAIL_COND(!im);
	ERR_FAIL_COND(!im->building);

	im->mask |= VS::ARRAY_FORMAT_COLOR;
	chunk_color = p_color;
}

void RasterizerStorageGLES2::immediate_uv(RID p_immediate, const Vector2 &tex_uv) {
	Immediate *im = immediate_owner.get(p_immediate);
	ERR_FAIL_COND(!im);
	ERR_FAIL_COND(!im->building);

	im->mask |= VS::ARRAY_FORMAT_TEX_UV;
	chunk_uv = tex_uv;
}

void RasterizerStorageGLES2::immediate_uv2(RID p_immediate, const Vector2 &tex_uv) {
	Immediate *im = immediate_owner.get(p_immediate);
	ERR_FAIL_COND(!im);
	ERR_FAIL_COND(!im->building);

	im->mask |= VS::ARRAY_FORMAT_TEX_UV2;
	chunk_uv2 = tex_uv;
}

void RasterizerStorageGLES2::immediate_end(RID p_immediate) {
	Immediate *im = immediate_owner.get(p_immediate);
	ERR_FAIL_COND(!im);
	ERR_FAIL_COND(!im->building);

	im->building = false;
	im->instance_change_notify(true, false);
}

void RasterizerStorageGLES2::immediate_clear(RID p_immediate) {
	Immediate *im = immediate_owner.get(p_immediate);
	ERR_FAIL_COND(!im);
	ERR_FAIL_COND(im->building);

	im->chunks.clear();
	im->instance_change_notify(true, false);
}

AABB RasterizerStorageGLES2::immediate_get_aabb(RID p_immediate) const {
	Immediate *im = immediate_owner.get(p_immediate);
	ERR_FAIL_COND_V(!im, AABB());
	return im->aabb;
}

void RasterizerStorageGLES2::immediate_set_material(RID p_immediate, RID p_material) {
	Immediate *im = immediate_owner.get(p_immediate);
	ERR_FAIL_COND(!im);

	im->material = p_material;
	im->instance_change_notify(false, true);
}

RID RasterizerStorageGLES2::immediate_get_material(RID p_immediate) const {
	const Immediate *im = immediate_owner.get(p_immediate);
	ERR_FAIL_COND_V(!im, RID());
	return im->material;
}

/* SKELETON API */

RID RasterizerStorageGLES2::skeleton_create() {
	Skeleton *skeleton = memnew(Skeleton);

	glGenTextures(1, &skeleton->tex_id);

	return skeleton_owner.make_rid(skeleton);
}

void RasterizerStorageGLES2::skeleton_allocate(RID p_skeleton, int p_bones, bool p_2d_skeleton) {
	Skeleton *skeleton = skeleton_owner.getornull(p_skeleton);
	ERR_FAIL_COND(!skeleton);
	ERR_FAIL_COND(p_bones < 0);

	if (skeleton->size == p_bones && skeleton->use_2d == p_2d_skeleton) {
		return;
	}

	skeleton->size = p_bones;
	skeleton->use_2d = p_2d_skeleton;

	if (!config.use_skeleton_software) {
		gl_wrapper.gl_active_texture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, skeleton->tex_id);

#ifdef GLES_OVER_GL
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, p_bones * (skeleton->use_2d ? 2 : 3), 1, 0, GL_RGBA, GL_FLOAT, nullptr);
#else
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, p_bones * (skeleton->use_2d ? 2 : 3), 1, 0, GL_RGBA, GL_FLOAT, NULL);
#endif

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		glBindTexture(GL_TEXTURE_2D, 0);
	}
	if (skeleton->use_2d) {
		skeleton->bone_data.resize(p_bones * 4 * 2);
	} else {
		skeleton->bone_data.resize(p_bones * 4 * 3);
	}
}

int RasterizerStorageGLES2::skeleton_get_bone_count(RID p_skeleton) const {
	Skeleton *skeleton = skeleton_owner.getornull(p_skeleton);
	ERR_FAIL_COND_V(!skeleton, 0);

	return skeleton->size;
}

void RasterizerStorageGLES2::skeleton_bone_set_transform(RID p_skeleton, int p_bone, const Transform &p_transform) {
	Skeleton *skeleton = skeleton_owner.getornull(p_skeleton);
	ERR_FAIL_COND(!skeleton);

	ERR_FAIL_INDEX(p_bone, skeleton->size);
	ERR_FAIL_COND(skeleton->use_2d);

	float *bone_data = skeleton->bone_data.ptrw();

	int base_offset = p_bone * 4 * 3;

	bone_data[base_offset + 0] = p_transform.basis[0].x;
	bone_data[base_offset + 1] = p_transform.basis[0].y;
	bone_data[base_offset + 2] = p_transform.basis[0].z;
	bone_data[base_offset + 3] = p_transform.origin.x;

	bone_data[base_offset + 4] = p_transform.basis[1].x;
	bone_data[base_offset + 5] = p_transform.basis[1].y;
	bone_data[base_offset + 6] = p_transform.basis[1].z;
	bone_data[base_offset + 7] = p_transform.origin.y;

	bone_data[base_offset + 8] = p_transform.basis[2].x;
	bone_data[base_offset + 9] = p_transform.basis[2].y;
	bone_data[base_offset + 10] = p_transform.basis[2].z;
	bone_data[base_offset + 11] = p_transform.origin.z;

	if (!skeleton->update_list.in_list()) {
		skeleton_update_list.add(&skeleton->update_list);
	}
}

Transform RasterizerStorageGLES2::skeleton_bone_get_transform(RID p_skeleton, int p_bone) const {
	Skeleton *skeleton = skeleton_owner.getornull(p_skeleton);
	ERR_FAIL_COND_V(!skeleton, Transform());

	ERR_FAIL_INDEX_V(p_bone, skeleton->size, Transform());
	ERR_FAIL_COND_V(skeleton->use_2d, Transform());

	const float *bone_data = skeleton->bone_data.ptr();

	Transform ret;

	int base_offset = p_bone * 4 * 3;

	ret.basis[0].x = bone_data[base_offset + 0];
	ret.basis[0].y = bone_data[base_offset + 1];
	ret.basis[0].z = bone_data[base_offset + 2];
	ret.origin.x = bone_data[base_offset + 3];

	ret.basis[1].x = bone_data[base_offset + 4];
	ret.basis[1].y = bone_data[base_offset + 5];
	ret.basis[1].z = bone_data[base_offset + 6];
	ret.origin.y = bone_data[base_offset + 7];

	ret.basis[2].x = bone_data[base_offset + 8];
	ret.basis[2].y = bone_data[base_offset + 9];
	ret.basis[2].z = bone_data[base_offset + 10];
	ret.origin.z = bone_data[base_offset + 11];

	return ret;
}
void RasterizerStorageGLES2::skeleton_bone_set_transform_2d(RID p_skeleton, int p_bone, const Transform2D &p_transform) {
	Skeleton *skeleton = skeleton_owner.getornull(p_skeleton);
	ERR_FAIL_COND(!skeleton);

	ERR_FAIL_INDEX(p_bone, skeleton->size);
	ERR_FAIL_COND(!skeleton->use_2d);

	float *bone_data = skeleton->bone_data.ptrw();

	int base_offset = p_bone * 4 * 2;

	bone_data[base_offset + 0] = p_transform[0][0];
	bone_data[base_offset + 1] = p_transform[1][0];
	bone_data[base_offset + 2] = 0;
	bone_data[base_offset + 3] = p_transform[2][0];
	bone_data[base_offset + 4] = p_transform[0][1];
	bone_data[base_offset + 5] = p_transform[1][1];
	bone_data[base_offset + 6] = 0;
	bone_data[base_offset + 7] = p_transform[2][1];

	if (!skeleton->update_list.in_list()) {
		skeleton_update_list.add(&skeleton->update_list);
	}
	skeleton->revision++;
}

Transform2D RasterizerStorageGLES2::skeleton_bone_get_transform_2d(RID p_skeleton, int p_bone) const {
	Skeleton *skeleton = skeleton_owner.getornull(p_skeleton);
	ERR_FAIL_COND_V(!skeleton, Transform2D());

	ERR_FAIL_INDEX_V(p_bone, skeleton->size, Transform2D());
	ERR_FAIL_COND_V(!skeleton->use_2d, Transform2D());

	const float *bone_data = skeleton->bone_data.ptr();

	Transform2D ret;

	int base_offset = p_bone * 4 * 2;

	ret[0][0] = bone_data[base_offset + 0];
	ret[1][0] = bone_data[base_offset + 1];
	ret[2][0] = bone_data[base_offset + 3];
	ret[0][1] = bone_data[base_offset + 4];
	ret[1][1] = bone_data[base_offset + 5];
	ret[2][1] = bone_data[base_offset + 7];

	return ret;
}

void RasterizerStorageGLES2::skeleton_set_base_transform_2d(RID p_skeleton, const Transform2D &p_base_transform) {
	Skeleton *skeleton = skeleton_owner.getornull(p_skeleton);
	ERR_FAIL_COND(!skeleton);

	skeleton->base_transform_2d = p_base_transform;
}

void RasterizerStorageGLES2::skeleton_attach_canvas_item(RID p_skeleton, RID p_canvas_item, bool p_attach) {
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

uint32_t RasterizerStorageGLES2::skeleton_get_revision(RID p_skeleton) const {
	const Skeleton *skeleton = skeleton_owner.getornull(p_skeleton);
	ERR_FAIL_COND_V(!skeleton, 0);
	return skeleton->revision;
}

void RasterizerStorageGLES2::update_dirty_blend_shapes() {
	while (blend_shapes_update_list.first()) {
		Mesh *mesh = blend_shapes_update_list.first()->self();
		for (int is = 0; is < mesh->surfaces.size(); is++) {
			RasterizerStorageGLES2::Surface *s = mesh->surfaces[is];
			if (!s->blend_shape_data.empty()) {
				PoolVector<float> &transform_buffer = resources.blend_shape_transform_cpu_buffer;
				size_t buffer_size = s->array_len * 8 * 4;
				if (resources.blend_shape_transform_cpu_buffer_size < buffer_size) {
					resources.blend_shape_transform_cpu_buffer_size = buffer_size;
					transform_buffer.resize(buffer_size);
				}

				PoolVector<uint8_t>::Read read = s->data.read();
				PoolVector<float>::Write write = transform_buffer.write();
				float base_weight = 1.0;

				if (s->mesh->blend_shape_mode == VS::BLEND_SHAPE_MODE_NORMALIZED) {
					for (int ti = 0; ti < mesh->blend_shape_values.size(); ti++) {
						base_weight -= mesh->blend_shape_values.get(ti);
					}
				}

				for (int i = 0; i < VS::ARRAY_MAX - 1; i++) {
					if (s->attribs[i].enabled) {
						// Read all attributes
						for (int j = 0; j < s->array_len; j++) {
							size_t offset = s->attribs[i].offset + (j * s->attribs[i].stride);
							const float *rd = (const float *)(read.ptr() + offset);

							size_t offset_write = i * 4 + (j * 8 * 4);
							float *wr = (float *)(write.ptr() + offset_write);

							// Set the base
							switch (i) {
								case VS::ARRAY_VERTEX: {
									if (s->format & VS::ARRAY_COMPRESS_VERTEX) {
										wr[0] = Math::halfptr_to_float(&((uint16_t *)rd)[0]) * base_weight;
										wr[1] = Math::halfptr_to_float(&((uint16_t *)rd)[1]) * base_weight;
										wr[2] = Math::halfptr_to_float(&((uint16_t *)rd)[2]) * base_weight;
										wr[3] = 1.0f;
									} else {
										float a[3] = { 0 };
										a[0] = wr[0] = rd[0] * base_weight;
										a[1] = wr[1] = rd[1] * base_weight;
										a[2] = wr[2] = rd[2] * base_weight;
										memcpy(&write[offset_write], a, sizeof(float) * s->attribs[i].size);
									}
								} break;
								case VS::ARRAY_NORMAL: {
									if (s->format & VS::ARRAY_FLAG_USE_OCTAHEDRAL_COMPRESSION) {
										if (s->format & VS::ARRAY_COMPRESS_NORMAL && s->format & VS::ARRAY_FORMAT_TANGENT && s->format & VS::ARRAY_COMPRESS_TANGENT) {
											Vector2 oct(((int8_t *)rd)[0] / 127.0, ((int8_t *)rd)[1] / 127.0);
											Vector3 vec = VS::oct_to_norm(oct);
											wr[0] = vec.x * base_weight;
											wr[1] = vec.y * base_weight;
											wr[2] = vec.z * base_weight;
										} else {
											Vector2 oct(((int16_t *)rd)[0] / 32767.0, ((int16_t *)rd)[1] / 32767.0);
											Vector3 vec = VS::oct_to_norm(oct);
											wr[0] = vec.x * base_weight;
											wr[1] = vec.y * base_weight;
											wr[2] = vec.z * base_weight;
										}
									} else {
										if (s->format & VS::ARRAY_COMPRESS_NORMAL) {
											wr[0] = (((int8_t *)rd)[0] / 127.0) * base_weight;
											wr[1] = (((int8_t *)rd)[1] / 127.0) * base_weight;
											wr[2] = (((int8_t *)rd)[2] / 127.0) * base_weight;
										} else {
											wr[0] = rd[0] * base_weight;
											wr[1] = rd[1] * base_weight;
											wr[2] = rd[2] * base_weight;
										}
									}
								} break;
								case VS::ARRAY_TANGENT: {
									if (s->format & VS::ARRAY_FLAG_USE_OCTAHEDRAL_COMPRESSION) {
										if (s->format & VS::ARRAY_COMPRESS_TANGENT && s->format & VS::ARRAY_FORMAT_NORMAL && s->format & VS::ARRAY_COMPRESS_NORMAL) {
											Vector2 oct(((int8_t *)rd)[0] / 127.0, ((int8_t *)rd)[1] / 127.0);
											float sign;
											Vector3 vec = VS::oct_to_tangent(oct, &sign);
											wr[0] = vec.x * base_weight;
											wr[1] = vec.y * base_weight;
											wr[2] = vec.z * base_weight;
											wr[3] = sign * base_weight;
										} else {
											Vector2 oct(((int16_t *)rd)[0] / 32767.0, ((int16_t *)rd)[1] / 32767.0);
											float sign;
											Vector3 vec = VS::oct_to_tangent(oct, &sign);
											wr[0] = vec.x * base_weight;
											wr[1] = vec.y * base_weight;
											wr[2] = vec.z * base_weight;
											wr[3] = sign * base_weight;
										}
									} else {
										if (s->format & VS::ARRAY_COMPRESS_TANGENT) {
											wr[0] = (((int8_t *)rd)[0] / 127.0) * base_weight;
											wr[1] = (((int8_t *)rd)[1] / 127.0) * base_weight;
											wr[2] = (((int8_t *)rd)[2] / 127.0) * base_weight;
											wr[3] = (((int8_t *)rd)[3] / 127.0) * base_weight;
										} else {
											wr[0] = rd[0] * base_weight;
											wr[1] = rd[1] * base_weight;
											wr[2] = rd[2] * base_weight;
											wr[3] = rd[3] * base_weight;
										}
									}
								} break;
								case VS::ARRAY_COLOR: {
									if (s->format & VS::ARRAY_COMPRESS_COLOR) {
										wr[0] = (((uint8_t *)rd)[0] / 255.0) * base_weight;
										wr[1] = (((uint8_t *)rd)[1] / 255.0) * base_weight;
										wr[2] = (((uint8_t *)rd)[2] / 255.0) * base_weight;
										wr[3] = (((uint8_t *)rd)[3] / 255.0) * base_weight;
									} else {
										wr[0] = rd[0] * base_weight;
										wr[1] = rd[1] * base_weight;
										wr[2] = rd[2] * base_weight;
										wr[3] = rd[3] * base_weight;
									}
								} break;
								case VS::ARRAY_TEX_UV: {
									if (s->format & VS::ARRAY_COMPRESS_TEX_UV) {
										wr[0] = Math::halfptr_to_float(&((uint16_t *)rd)[0]) * base_weight;
										wr[1] = Math::halfptr_to_float(&((uint16_t *)rd)[1]) * base_weight;
									} else {
										wr[0] = rd[0] * base_weight;
										wr[1] = rd[1] * base_weight;
									}
								} break;
								case VS::ARRAY_TEX_UV2: {
									if (s->format & VS::ARRAY_COMPRESS_TEX_UV2) {
										wr[0] = Math::halfptr_to_float(&((uint16_t *)rd)[0]) * base_weight;
										wr[1] = Math::halfptr_to_float(&((uint16_t *)rd)[1]) * base_weight;
									} else {
										wr[0] = rd[0] * base_weight;
										wr[1] = rd[1] * base_weight;
									}
								} break;
								case VS::ARRAY_WEIGHTS: {
									if (s->format & VS::ARRAY_COMPRESS_WEIGHTS) {
										wr[0] = (((uint16_t *)rd)[0] / 65535.0) * base_weight;
										wr[1] = (((uint16_t *)rd)[1] / 65535.0) * base_weight;
										wr[2] = (((uint16_t *)rd)[2] / 65535.0) * base_weight;
										wr[3] = (((uint16_t *)rd)[3] / 65535.0) * base_weight;
									} else {
										wr[0] = rd[0] * base_weight;
										wr[1] = rd[1] * base_weight;
										wr[2] = rd[2] * base_weight;
										wr[3] = rd[3] * base_weight;
									}
								} break;
							}

							// Add all blend shapes
							for (int ti = 0; ti < mesh->blend_shape_values.size(); ti++) {
								PoolVector<uint8_t>::Read blend = s->blend_shape_data[ti].read();
								const float *br = (const float *)(blend.ptr() + offset);

								float weight = mesh->blend_shape_values.get(ti);
								if (Math::is_zero_approx(weight)) {
									continue;
								}

								switch (i) {
									case VS::ARRAY_VERTEX: {
										if (s->format & VS::ARRAY_COMPRESS_VERTEX) {
											wr[0] += Math::halfptr_to_float(&((uint16_t *)br)[0]) * weight;
											wr[1] += Math::halfptr_to_float(&((uint16_t *)br)[1]) * weight;
											wr[2] += Math::halfptr_to_float(&((uint16_t *)br)[2]) * weight;
											wr[3] = 1.0f;
										} else {
											wr[0] += br[0] * weight;
											wr[1] += br[1] * weight;
											wr[2] += br[2] * weight;
										}
									} break;
									case VS::ARRAY_NORMAL: {
										if (s->format & VS::ARRAY_FLAG_USE_OCTAHEDRAL_COMPRESSION) {
											if (s->format & VS::ARRAY_COMPRESS_NORMAL && s->format & VS::ARRAY_FORMAT_TANGENT && s->format & VS::ARRAY_COMPRESS_TANGENT) {
												Vector2 oct(((int8_t *)br)[0] / 127.0, ((int8_t *)br)[1] / 127.0);
												Vector3 vec = VS::oct_to_norm(oct);
												wr[0] += vec.x * weight;
												wr[1] += vec.y * weight;
												wr[2] += vec.z * weight;
											} else {
												Vector2 oct(((int16_t *)br)[0] / 32767.0, ((int16_t *)br)[1] / 32767.0);
												Vector3 vec = VS::oct_to_norm(oct);
												wr[0] += vec.x * weight;
												wr[1] += vec.y * weight;
												wr[2] += vec.z * weight;
											}
										} else {
											if (s->format & VS::ARRAY_COMPRESS_NORMAL) {
												wr[0] += (float(((int8_t *)br)[0]) / 127.0) * weight;
												wr[1] += (float(((int8_t *)br)[1]) / 127.0) * weight;
												wr[2] += (float(((int8_t *)br)[2]) / 127.0) * weight;
											} else {
												wr[0] += br[0] * weight;
												wr[1] += br[1] * weight;
												wr[2] += br[2] * weight;
											}
										}
									} break;
									case VS::ARRAY_TANGENT: {
										if (s->format & VS::ARRAY_FLAG_USE_OCTAHEDRAL_COMPRESSION) {
											if (s->format & VS::ARRAY_COMPRESS_TANGENT && s->format & VS::ARRAY_FORMAT_NORMAL && s->format & VS::ARRAY_COMPRESS_NORMAL) {
												Vector2 oct(((int8_t *)br)[0] / 127.0, ((int8_t *)br)[1] / 127.0);
												float sign;
												Vector3 vec = VS::oct_to_tangent(oct, &sign);
												wr[0] += vec.x * weight;
												wr[1] += vec.y * weight;
												wr[2] += vec.z * weight;
												wr[3] = sign * weight;
											} else {
												Vector2 oct(((int16_t *)rd)[0] / 32767.0, ((int16_t *)rd)[1] / 32767.0);
												float sign;
												Vector3 vec = VS::oct_to_tangent(oct, &sign);
												wr[0] += vec.x * weight;
												wr[1] += vec.y * weight;
												wr[2] += vec.z * weight;
												wr[3] = sign * weight;
											}
										} else {
											if (s->format & VS::ARRAY_COMPRESS_TANGENT) {
												wr[0] += (float(((int8_t *)br)[0]) / 127.0) * weight;
												wr[1] += (float(((int8_t *)br)[1]) / 127.0) * weight;
												wr[2] += (float(((int8_t *)br)[2]) / 127.0) * weight;
												wr[3] = (float(((int8_t *)br)[3]) / 127.0);
											} else {
												wr[0] += br[0] * weight;
												wr[1] += br[1] * weight;
												wr[2] += br[2] * weight;
												wr[3] = br[3];
											}
										}
									} break;
									case VS::ARRAY_COLOR: {
										if (s->format & VS::ARRAY_COMPRESS_COLOR) {
											wr[0] += (((uint8_t *)br)[0] / 255.0) * weight;
											wr[1] += (((uint8_t *)br)[1] / 255.0) * weight;
											wr[2] += (((uint8_t *)br)[2] / 255.0) * weight;
											wr[3] += (((uint8_t *)br)[3] / 255.0) * weight;
										} else {
											wr[0] += br[0] * weight;
											wr[1] += br[1] * weight;
											wr[2] += br[2] * weight;
											wr[3] += br[3] * weight;
										}
									} break;
									case VS::ARRAY_TEX_UV: {
										if (s->format & VS::ARRAY_COMPRESS_TEX_UV) {
											wr[0] += Math::halfptr_to_float(&((uint16_t *)br)[0]) * weight;
											wr[1] += Math::halfptr_to_float(&((uint16_t *)br)[1]) * weight;
										} else {
											wr[0] += br[0] * weight;
											wr[1] += br[1] * weight;
										}
									} break;
									case VS::ARRAY_TEX_UV2: {
										if (s->format & VS::ARRAY_COMPRESS_TEX_UV2) {
											wr[0] += Math::halfptr_to_float(&((uint16_t *)br)[0]) * weight;
											wr[1] += Math::halfptr_to_float(&((uint16_t *)br)[1]) * weight;
										} else {
											wr[0] += br[0] * weight;
											wr[1] += br[1] * weight;
										}
									} break;
									case VS::ARRAY_WEIGHTS: {
										if (s->format & VS::ARRAY_COMPRESS_WEIGHTS) {
											wr[0] += (((uint16_t *)br)[0] / 65535.0) * weight;
											wr[1] += (((uint16_t *)br)[1] / 65535.0) * weight;
											wr[2] += (((uint16_t *)br)[2] / 65535.0) * weight;
											wr[3] += (((uint16_t *)br)[3] / 65535.0) * weight;
										} else {
											wr[0] += br[0] * weight;
											wr[1] += br[1] * weight;
											wr[2] += br[2] * weight;
											wr[3] += br[3] * weight;
										}
									} break;
								}
							}
						}
					}
				}

				// Store size and send changed blend shape render to GL
				glBindBuffer(GL_ARRAY_BUFFER, s->blend_shape_buffer_id);
				if (buffer_size > s->blend_shape_buffer_size) {
					s->blend_shape_buffer_size = buffer_size;
					glBufferData(GL_ARRAY_BUFFER, buffer_size * sizeof(float), transform_buffer.read().ptr(), GL_DYNAMIC_DRAW);
				} else {
					buffer_orphan_and_upload(s->blend_shape_buffer_size * sizeof(float), 0, buffer_size * sizeof(float), transform_buffer.read().ptr(), GL_ARRAY_BUFFER, true);
				}
				glBindBuffer(GL_ARRAY_BUFFER, 0);
			}
		}
		blend_shapes_update_list.remove(blend_shapes_update_list.first());
	}
}

void RasterizerStorageGLES2::_update_skeleton_transform_buffer(const PoolVector<float> &p_data, size_t p_size) {
	glBindBuffer(GL_ARRAY_BUFFER, resources.skeleton_transform_buffer);

	uint32_t buffer_size = p_size * sizeof(float);

	if (p_size > resources.skeleton_transform_buffer_size) {
		// new requested buffer is bigger, so resizing the GPU buffer

		resources.skeleton_transform_buffer_size = p_size;

		glBufferData(GL_ARRAY_BUFFER, buffer_size, p_data.read().ptr(), GL_DYNAMIC_DRAW);
	} else {
		// this may not be best, it could be better to use glBufferData in both cases.
		buffer_orphan_and_upload(resources.skeleton_transform_buffer_size * sizeof(float), 0, buffer_size, p_data.read().ptr(), GL_ARRAY_BUFFER, true);
	}

	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void RasterizerStorageGLES2::update_dirty_skeletons() {
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

	if (config.use_skeleton_software) {
		return;
	}

	gl_wrapper.gl_active_texture(GL_TEXTURE0);

	while (skeleton_update_list.first()) {
		Skeleton *skeleton = skeleton_update_list.first()->self();

		if (skeleton->size) {
			glBindTexture(GL_TEXTURE_2D, skeleton->tex_id);

			glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, skeleton->size * (skeleton->use_2d ? 2 : 3), 1, GL_RGBA, GL_FLOAT, skeleton->bone_data.ptr());
		}

		for (Set<RasterizerScene::InstanceBase *>::Element *E = skeleton->instances.front(); E; E = E->next()) {
			E->get()->base_changed(true, false);
		}

		skeleton_update_list.remove(skeleton_update_list.first());
	}
}

/* Light API */

RID RasterizerStorageGLES2::light_create(VS::LightType p_type) {
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

void RasterizerStorageGLES2::light_set_color(RID p_light, const Color &p_color) {
	Light *light = light_owner.getornull(p_light);
	ERR_FAIL_COND(!light);

	light->color = p_color;
}

void RasterizerStorageGLES2::light_set_param(RID p_light, VS::LightParam p_param, float p_value) {
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

void RasterizerStorageGLES2::light_set_shadow(RID p_light, bool p_enabled) {
	Light *light = light_owner.getornull(p_light);
	ERR_FAIL_COND(!light);

	light->shadow = p_enabled;

	light->version++;
	light->instance_change_notify(true, false);
}

void RasterizerStorageGLES2::light_set_shadow_color(RID p_light, const Color &p_color) {
	Light *light = light_owner.getornull(p_light);
	ERR_FAIL_COND(!light);

	light->shadow_color = p_color;
}

void RasterizerStorageGLES2::light_set_projector(RID p_light, RID p_texture) {
	Light *light = light_owner.getornull(p_light);
	ERR_FAIL_COND(!light);

	light->projector = p_texture;
}

void RasterizerStorageGLES2::light_set_negative(RID p_light, bool p_enable) {
	Light *light = light_owner.getornull(p_light);
	ERR_FAIL_COND(!light);

	light->negative = p_enable;
}

void RasterizerStorageGLES2::light_set_cull_mask(RID p_light, uint32_t p_mask) {
	Light *light = light_owner.getornull(p_light);
	ERR_FAIL_COND(!light);

	light->cull_mask = p_mask;

	light->version++;
	light->instance_change_notify(true, false);
}

void RasterizerStorageGLES2::light_set_reverse_cull_face_mode(RID p_light, bool p_enabled) {
	Light *light = light_owner.getornull(p_light);
	ERR_FAIL_COND(!light);

	light->reverse_cull = p_enabled;

	light->version++;
	light->instance_change_notify(true, false);
}

void RasterizerStorageGLES2::light_set_use_gi(RID p_light, bool p_enabled) {
	WARN_DEPRECATED_MSG("'VisualServer.light_set_use_gi' is deprecated and will be removed in a future version. Use 'VisualServer.light_set_bake_mode' instead.");
	light_set_bake_mode(p_light, p_enabled ? VS::LightBakeMode::LIGHT_BAKE_INDIRECT : VS::LightBakeMode::LIGHT_BAKE_DISABLED);
}

void RasterizerStorageGLES2::light_set_bake_mode(RID p_light, VS::LightBakeMode p_bake_mode) {
	Light *light = light_owner.getornull(p_light);
	ERR_FAIL_COND(!light);

	light->bake_mode = p_bake_mode;

	light->version++;
	light->instance_change_notify(true, false);
}

void RasterizerStorageGLES2::light_omni_set_shadow_mode(RID p_light, VS::LightOmniShadowMode p_mode) {
	Light *light = light_owner.getornull(p_light);
	ERR_FAIL_COND(!light);

	light->omni_shadow_mode = p_mode;

	light->version++;
	light->instance_change_notify(true, false);
}

VS::LightOmniShadowMode RasterizerStorageGLES2::light_omni_get_shadow_mode(RID p_light) {
	Light *light = light_owner.getornull(p_light);
	ERR_FAIL_COND_V(!light, VS::LIGHT_OMNI_SHADOW_CUBE);

	return light->omni_shadow_mode;
}

void RasterizerStorageGLES2::light_omni_set_shadow_detail(RID p_light, VS::LightOmniShadowDetail p_detail) {
	Light *light = light_owner.getornull(p_light);
	ERR_FAIL_COND(!light);

	light->omni_shadow_detail = p_detail;

	light->version++;
	light->instance_change_notify(true, false);
}

void RasterizerStorageGLES2::light_directional_set_shadow_mode(RID p_light, VS::LightDirectionalShadowMode p_mode) {
	Light *light = light_owner.getornull(p_light);
	ERR_FAIL_COND(!light);

	light->directional_shadow_mode = p_mode;

	light->version++;
	light->instance_change_notify(true, false);
}

void RasterizerStorageGLES2::light_directional_set_blend_splits(RID p_light, bool p_enable) {
	Light *light = light_owner.getornull(p_light);
	ERR_FAIL_COND(!light);

	light->directional_blend_splits = p_enable;

	light->version++;
	light->instance_change_notify(true, false);
}

bool RasterizerStorageGLES2::light_directional_get_blend_splits(RID p_light) const {
	Light *light = light_owner.getornull(p_light);
	ERR_FAIL_COND_V(!light, false);
	return light->directional_blend_splits;
}

VS::LightDirectionalShadowMode RasterizerStorageGLES2::light_directional_get_shadow_mode(RID p_light) {
	Light *light = light_owner.getornull(p_light);
	ERR_FAIL_COND_V(!light, VS::LIGHT_DIRECTIONAL_SHADOW_ORTHOGONAL);
	return light->directional_shadow_mode;
}

void RasterizerStorageGLES2::light_directional_set_shadow_depth_range_mode(RID p_light, VS::LightDirectionalShadowDepthRangeMode p_range_mode) {
	Light *light = light_owner.getornull(p_light);
	ERR_FAIL_COND(!light);

	light->directional_range_mode = p_range_mode;
}

VS::LightDirectionalShadowDepthRangeMode RasterizerStorageGLES2::light_directional_get_shadow_depth_range_mode(RID p_light) const {
	Light *light = light_owner.getornull(p_light);
	ERR_FAIL_COND_V(!light, VS::LIGHT_DIRECTIONAL_SHADOW_DEPTH_RANGE_STABLE);

	return light->directional_range_mode;
}

VS::LightType RasterizerStorageGLES2::light_get_type(RID p_light) const {
	Light *light = light_owner.getornull(p_light);
	ERR_FAIL_COND_V(!light, VS::LIGHT_DIRECTIONAL);

	return light->type;
}

float RasterizerStorageGLES2::light_get_param(RID p_light, VS::LightParam p_param) {
	Light *light = light_owner.getornull(p_light);
	ERR_FAIL_COND_V(!light, 0.0);
	ERR_FAIL_INDEX_V(p_param, VS::LIGHT_PARAM_MAX, 0.0);

	return light->param[p_param];
}

Color RasterizerStorageGLES2::light_get_color(RID p_light) {
	Light *light = light_owner.getornull(p_light);
	ERR_FAIL_COND_V(!light, Color());

	return light->color;
}

bool RasterizerStorageGLES2::light_get_use_gi(RID p_light) {
	return light_get_bake_mode(p_light) != VS::LightBakeMode::LIGHT_BAKE_DISABLED;
}

VS::LightBakeMode RasterizerStorageGLES2::light_get_bake_mode(RID p_light) {
	Light *light = light_owner.getornull(p_light);
	ERR_FAIL_COND_V(!light, VS::LightBakeMode::LIGHT_BAKE_DISABLED);

	return light->bake_mode;
}

bool RasterizerStorageGLES2::light_has_shadow(RID p_light) const {
	Light *light = light_owner.getornull(p_light);
	ERR_FAIL_COND_V(!light, false);

	return light->shadow;
}

uint64_t RasterizerStorageGLES2::light_get_version(RID p_light) const {
	Light *light = light_owner.getornull(p_light);
	ERR_FAIL_COND_V(!light, 0);

	return light->version;
}

AABB RasterizerStorageGLES2::light_get_aabb(RID p_light) const {
	Light *light = light_owner.getornull(p_light);
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

RID RasterizerStorageGLES2::reflection_probe_create() {
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
	reflection_probe->resolution = 128;

	return reflection_probe_owner.make_rid(reflection_probe);
}

void RasterizerStorageGLES2::reflection_probe_set_update_mode(RID p_probe, VS::ReflectionProbeUpdateMode p_mode) {
	ReflectionProbe *reflection_probe = reflection_probe_owner.getornull(p_probe);
	ERR_FAIL_COND(!reflection_probe);

	reflection_probe->update_mode = p_mode;
	reflection_probe->instance_change_notify(true, false);
}

void RasterizerStorageGLES2::reflection_probe_set_intensity(RID p_probe, float p_intensity) {
	ReflectionProbe *reflection_probe = reflection_probe_owner.getornull(p_probe);
	ERR_FAIL_COND(!reflection_probe);

	reflection_probe->intensity = p_intensity;
}

void RasterizerStorageGLES2::reflection_probe_set_interior_ambient(RID p_probe, const Color &p_ambient) {
	ReflectionProbe *reflection_probe = reflection_probe_owner.getornull(p_probe);
	ERR_FAIL_COND(!reflection_probe);

	reflection_probe->interior_ambient = p_ambient;
}

void RasterizerStorageGLES2::reflection_probe_set_interior_ambient_energy(RID p_probe, float p_energy) {
	ReflectionProbe *reflection_probe = reflection_probe_owner.getornull(p_probe);
	ERR_FAIL_COND(!reflection_probe);

	reflection_probe->interior_ambient_energy = p_energy;
}

void RasterizerStorageGLES2::reflection_probe_set_interior_ambient_probe_contribution(RID p_probe, float p_contrib) {
	ReflectionProbe *reflection_probe = reflection_probe_owner.getornull(p_probe);
	ERR_FAIL_COND(!reflection_probe);

	reflection_probe->interior_ambient_probe_contrib = p_contrib;
}

void RasterizerStorageGLES2::reflection_probe_set_max_distance(RID p_probe, float p_distance) {
	ReflectionProbe *reflection_probe = reflection_probe_owner.getornull(p_probe);
	ERR_FAIL_COND(!reflection_probe);

	reflection_probe->max_distance = p_distance;
	reflection_probe->instance_change_notify(true, false);
}
void RasterizerStorageGLES2::reflection_probe_set_extents(RID p_probe, const Vector3 &p_extents) {
	ReflectionProbe *reflection_probe = reflection_probe_owner.getornull(p_probe);
	ERR_FAIL_COND(!reflection_probe);

	reflection_probe->extents = p_extents;
	reflection_probe->instance_change_notify(true, false);
}
void RasterizerStorageGLES2::reflection_probe_set_origin_offset(RID p_probe, const Vector3 &p_offset) {
	ReflectionProbe *reflection_probe = reflection_probe_owner.getornull(p_probe);
	ERR_FAIL_COND(!reflection_probe);

	reflection_probe->origin_offset = p_offset;
	reflection_probe->instance_change_notify(true, false);
}

void RasterizerStorageGLES2::reflection_probe_set_as_interior(RID p_probe, bool p_enable) {
	ReflectionProbe *reflection_probe = reflection_probe_owner.getornull(p_probe);
	ERR_FAIL_COND(!reflection_probe);

	reflection_probe->interior = p_enable;
	reflection_probe->instance_change_notify(true, false);
}
void RasterizerStorageGLES2::reflection_probe_set_enable_box_projection(RID p_probe, bool p_enable) {
	ReflectionProbe *reflection_probe = reflection_probe_owner.getornull(p_probe);
	ERR_FAIL_COND(!reflection_probe);

	reflection_probe->box_projection = p_enable;
}

void RasterizerStorageGLES2::reflection_probe_set_enable_shadows(RID p_probe, bool p_enable) {
	ReflectionProbe *reflection_probe = reflection_probe_owner.getornull(p_probe);
	ERR_FAIL_COND(!reflection_probe);

	reflection_probe->enable_shadows = p_enable;
	reflection_probe->instance_change_notify(true, false);
}
void RasterizerStorageGLES2::reflection_probe_set_cull_mask(RID p_probe, uint32_t p_layers) {
	ReflectionProbe *reflection_probe = reflection_probe_owner.getornull(p_probe);
	ERR_FAIL_COND(!reflection_probe);

	reflection_probe->cull_mask = p_layers;
	reflection_probe->instance_change_notify(true, false);
}

void RasterizerStorageGLES2::reflection_probe_set_resolution(RID p_probe, int p_resolution) {
	ReflectionProbe *reflection_probe = reflection_probe_owner.getornull(p_probe);
	ERR_FAIL_COND(!reflection_probe);

	reflection_probe->resolution = p_resolution;
}

AABB RasterizerStorageGLES2::reflection_probe_get_aabb(RID p_probe) const {
	const ReflectionProbe *reflection_probe = reflection_probe_owner.getornull(p_probe);
	ERR_FAIL_COND_V(!reflection_probe, AABB());

	AABB aabb;
	aabb.position = -reflection_probe->extents;
	aabb.size = reflection_probe->extents * 2.0;

	return aabb;
}
VS::ReflectionProbeUpdateMode RasterizerStorageGLES2::reflection_probe_get_update_mode(RID p_probe) const {
	const ReflectionProbe *reflection_probe = reflection_probe_owner.getornull(p_probe);
	ERR_FAIL_COND_V(!reflection_probe, VS::REFLECTION_PROBE_UPDATE_ALWAYS);

	return reflection_probe->update_mode;
}

uint32_t RasterizerStorageGLES2::reflection_probe_get_cull_mask(RID p_probe) const {
	const ReflectionProbe *reflection_probe = reflection_probe_owner.getornull(p_probe);
	ERR_FAIL_COND_V(!reflection_probe, 0);

	return reflection_probe->cull_mask;
}

Vector3 RasterizerStorageGLES2::reflection_probe_get_extents(RID p_probe) const {
	const ReflectionProbe *reflection_probe = reflection_probe_owner.getornull(p_probe);
	ERR_FAIL_COND_V(!reflection_probe, Vector3());

	return reflection_probe->extents;
}
Vector3 RasterizerStorageGLES2::reflection_probe_get_origin_offset(RID p_probe) const {
	const ReflectionProbe *reflection_probe = reflection_probe_owner.getornull(p_probe);
	ERR_FAIL_COND_V(!reflection_probe, Vector3());

	return reflection_probe->origin_offset;
}

bool RasterizerStorageGLES2::reflection_probe_renders_shadows(RID p_probe) const {
	const ReflectionProbe *reflection_probe = reflection_probe_owner.getornull(p_probe);
	ERR_FAIL_COND_V(!reflection_probe, false);

	return reflection_probe->enable_shadows;
}

float RasterizerStorageGLES2::reflection_probe_get_origin_max_distance(RID p_probe) const {
	const ReflectionProbe *reflection_probe = reflection_probe_owner.getornull(p_probe);
	ERR_FAIL_COND_V(!reflection_probe, 0);

	return reflection_probe->max_distance;
}

int RasterizerStorageGLES2::reflection_probe_get_resolution(RID p_probe) const {
	const ReflectionProbe *reflection_probe = reflection_probe_owner.getornull(p_probe);
	ERR_FAIL_COND_V(!reflection_probe, 0);

	return reflection_probe->resolution;
}

RID RasterizerStorageGLES2::gi_probe_create() {
	return RID();
}

void RasterizerStorageGLES2::gi_probe_set_bounds(RID p_probe, const AABB &p_bounds) {
}

AABB RasterizerStorageGLES2::gi_probe_get_bounds(RID p_probe) const {
	return AABB();
}

void RasterizerStorageGLES2::gi_probe_set_cell_size(RID p_probe, float p_size) {
}

float RasterizerStorageGLES2::gi_probe_get_cell_size(RID p_probe) const {
	return 0.0;
}

void RasterizerStorageGLES2::gi_probe_set_to_cell_xform(RID p_probe, const Transform &p_xform) {
}

Transform RasterizerStorageGLES2::gi_probe_get_to_cell_xform(RID p_probe) const {
	return Transform();
}

void RasterizerStorageGLES2::gi_probe_set_dynamic_data(RID p_probe, const PoolVector<int> &p_data) {
}

PoolVector<int> RasterizerStorageGLES2::gi_probe_get_dynamic_data(RID p_probe) const {
	return PoolVector<int>();
}

void RasterizerStorageGLES2::gi_probe_set_dynamic_range(RID p_probe, int p_range) {
}

int RasterizerStorageGLES2::gi_probe_get_dynamic_range(RID p_probe) const {
	return 0;
}

void RasterizerStorageGLES2::gi_probe_set_energy(RID p_probe, float p_range) {
}

void RasterizerStorageGLES2::gi_probe_set_bias(RID p_probe, float p_range) {
}

void RasterizerStorageGLES2::gi_probe_set_normal_bias(RID p_probe, float p_range) {
}

void RasterizerStorageGLES2::gi_probe_set_propagation(RID p_probe, float p_range) {
}

void RasterizerStorageGLES2::gi_probe_set_interior(RID p_probe, bool p_enable) {
}

bool RasterizerStorageGLES2::gi_probe_is_interior(RID p_probe) const {
	return false;
}

void RasterizerStorageGLES2::gi_probe_set_compress(RID p_probe, bool p_enable) {
}

bool RasterizerStorageGLES2::gi_probe_is_compressed(RID p_probe) const {
	return false;
}
float RasterizerStorageGLES2::gi_probe_get_energy(RID p_probe) const {
	return 0;
}

float RasterizerStorageGLES2::gi_probe_get_bias(RID p_probe) const {
	return 0;
}

float RasterizerStorageGLES2::gi_probe_get_normal_bias(RID p_probe) const {
	return 0;
}

float RasterizerStorageGLES2::gi_probe_get_propagation(RID p_probe) const {
	return 0;
}

uint32_t RasterizerStorageGLES2::gi_probe_get_version(RID p_probe) {
	return 0;
}

RID RasterizerStorageGLES2::gi_probe_dynamic_data_create(int p_width, int p_height, int p_depth, GIProbeCompression p_compression) {
	return RID();
}

void RasterizerStorageGLES2::gi_probe_dynamic_data_update(RID p_gi_probe_data, int p_depth_slice, int p_slice_count, int p_mipmap, const void *p_data) {
}

///////

RID RasterizerStorageGLES2::lightmap_capture_create() {
	LightmapCapture *capture = memnew(LightmapCapture);
	return lightmap_capture_data_owner.make_rid(capture);
}

void RasterizerStorageGLES2::lightmap_capture_set_bounds(RID p_capture, const AABB &p_bounds) {
	LightmapCapture *capture = lightmap_capture_data_owner.getornull(p_capture);
	ERR_FAIL_COND(!capture);
	capture->bounds = p_bounds;
	capture->instance_change_notify(true, false);
}
AABB RasterizerStorageGLES2::lightmap_capture_get_bounds(RID p_capture) const {
	const LightmapCapture *capture = lightmap_capture_data_owner.getornull(p_capture);
	ERR_FAIL_COND_V(!capture, AABB());
	return capture->bounds;
}
void RasterizerStorageGLES2::lightmap_capture_set_octree(RID p_capture, const PoolVector<uint8_t> &p_octree) {
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
PoolVector<uint8_t> RasterizerStorageGLES2::lightmap_capture_get_octree(RID p_capture) const {
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

void RasterizerStorageGLES2::lightmap_capture_set_octree_cell_transform(RID p_capture, const Transform &p_xform) {
	LightmapCapture *capture = lightmap_capture_data_owner.getornull(p_capture);
	ERR_FAIL_COND(!capture);
	capture->cell_xform = p_xform;
}

Transform RasterizerStorageGLES2::lightmap_capture_get_octree_cell_transform(RID p_capture) const {
	const LightmapCapture *capture = lightmap_capture_data_owner.getornull(p_capture);
	ERR_FAIL_COND_V(!capture, Transform());
	return capture->cell_xform;
}

void RasterizerStorageGLES2::lightmap_capture_set_octree_cell_subdiv(RID p_capture, int p_subdiv) {
	LightmapCapture *capture = lightmap_capture_data_owner.getornull(p_capture);
	ERR_FAIL_COND(!capture);
	capture->cell_subdiv = p_subdiv;
}

int RasterizerStorageGLES2::lightmap_capture_get_octree_cell_subdiv(RID p_capture) const {
	const LightmapCapture *capture = lightmap_capture_data_owner.getornull(p_capture);
	ERR_FAIL_COND_V(!capture, 0);
	return capture->cell_subdiv;
}

void RasterizerStorageGLES2::lightmap_capture_set_energy(RID p_capture, float p_energy) {
	LightmapCapture *capture = lightmap_capture_data_owner.getornull(p_capture);
	ERR_FAIL_COND(!capture);
	capture->energy = p_energy;

	if (!capture->update_list.in_list()) {
		capture_update_list.add(&capture->update_list);
	}
}

float RasterizerStorageGLES2::lightmap_capture_get_energy(RID p_capture) const {
	const LightmapCapture *capture = lightmap_capture_data_owner.getornull(p_capture);
	ERR_FAIL_COND_V(!capture, 0);
	return capture->energy;
}

void RasterizerStorageGLES2::lightmap_capture_set_interior(RID p_capture, bool p_interior) {
	LightmapCapture *capture = lightmap_capture_data_owner.getornull(p_capture);
	ERR_FAIL_COND(!capture);
	capture->interior = p_interior;

	if (!capture->update_list.in_list()) {
		capture_update_list.add(&capture->update_list);
	}
}

bool RasterizerStorageGLES2::lightmap_capture_is_interior(RID p_capture) const {
	const LightmapCapture *capture = lightmap_capture_data_owner.getornull(p_capture);
	ERR_FAIL_COND_V(!capture, false);
	return capture->interior;
}

void RasterizerStorageGLES2::update_dirty_captures() {
	while (capture_update_list.first()) {
		LightmapCapture *capture = capture_update_list.first()->self();
		capture->instance_change_notify(false, true);
		capture_update_list.remove(capture_update_list.first());
	}
}

const PoolVector<RasterizerStorage::LightmapCaptureOctree> *RasterizerStorageGLES2::lightmap_capture_get_octree_ptr(RID p_capture) const {
	const LightmapCapture *capture = lightmap_capture_data_owner.getornull(p_capture);
	ERR_FAIL_COND_V(!capture, nullptr);
	return &capture->octree;
}

///////

RID RasterizerStorageGLES2::particles_create() {
	return RID();
}

void RasterizerStorageGLES2::particles_set_emitting(RID p_particles, bool p_emitting) {
}

bool RasterizerStorageGLES2::particles_get_emitting(RID p_particles) {
	return false;
}

void RasterizerStorageGLES2::particles_set_amount(RID p_particles, int p_amount) {
}

void RasterizerStorageGLES2::particles_set_lifetime(RID p_particles, float p_lifetime) {
}

void RasterizerStorageGLES2::particles_set_one_shot(RID p_particles, bool p_one_shot) {
}

void RasterizerStorageGLES2::particles_set_pre_process_time(RID p_particles, float p_time) {
}

void RasterizerStorageGLES2::particles_set_explosiveness_ratio(RID p_particles, float p_ratio) {
}

void RasterizerStorageGLES2::particles_set_randomness_ratio(RID p_particles, float p_ratio) {
}

void RasterizerStorageGLES2::particles_set_custom_aabb(RID p_particles, const AABB &p_aabb) {
}

void RasterizerStorageGLES2::particles_set_speed_scale(RID p_particles, float p_scale) {
}

void RasterizerStorageGLES2::particles_set_use_local_coordinates(RID p_particles, bool p_enable) {
}

void RasterizerStorageGLES2::particles_set_fixed_fps(RID p_particles, int p_fps) {
}

void RasterizerStorageGLES2::particles_set_fractional_delta(RID p_particles, bool p_enable) {
}

void RasterizerStorageGLES2::particles_set_process_material(RID p_particles, RID p_material) {
}

void RasterizerStorageGLES2::particles_set_draw_order(RID p_particles, VS::ParticlesDrawOrder p_order) {
}

void RasterizerStorageGLES2::particles_set_draw_passes(RID p_particles, int p_passes) {
}

void RasterizerStorageGLES2::particles_set_draw_pass_mesh(RID p_particles, int p_pass, RID p_mesh) {
}

void RasterizerStorageGLES2::particles_restart(RID p_particles) {
}

void RasterizerStorageGLES2::particles_request_process(RID p_particles) {
}

AABB RasterizerStorageGLES2::particles_get_current_aabb(RID p_particles) {
	return AABB();
}

AABB RasterizerStorageGLES2::particles_get_aabb(RID p_particles) const {
	return AABB();
}

void RasterizerStorageGLES2::particles_set_emission_transform(RID p_particles, const Transform &p_transform) {
}

int RasterizerStorageGLES2::particles_get_draw_passes(RID p_particles) const {
	return 0;
}

RID RasterizerStorageGLES2::particles_get_draw_pass_mesh(RID p_particles, int p_pass) const {
	return RID();
}

void RasterizerStorageGLES2::update_particles() {
}

bool RasterizerStorageGLES2::particles_is_inactive(RID p_particles) const {
	return true;
}

////////

void RasterizerStorageGLES2::instance_add_skeleton(RID p_skeleton, RasterizerScene::InstanceBase *p_instance) {
	Skeleton *skeleton = skeleton_owner.getornull(p_skeleton);
	ERR_FAIL_COND(!skeleton);

	skeleton->instances.insert(p_instance);
}

void RasterizerStorageGLES2::instance_remove_skeleton(RID p_skeleton, RasterizerScene::InstanceBase *p_instance) {
	Skeleton *skeleton = skeleton_owner.getornull(p_skeleton);
	ERR_FAIL_COND(!skeleton);

	skeleton->instances.erase(p_instance);
}

void RasterizerStorageGLES2::instance_add_dependency(RID p_base, RasterizerScene::InstanceBase *p_instance) {
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
		/*case VS::INSTANCE_PARTICLES: {
			inst = particles_owner.getornull(p_base);
			ERR_FAIL_COND(!inst);
		} break;*/
		case VS::INSTANCE_REFLECTION_PROBE: {
			inst = reflection_probe_owner.getornull(p_base);
			ERR_FAIL_COND(!inst);
		} break;
		case VS::INSTANCE_LIGHT: {
			inst = light_owner.getornull(p_base);
			ERR_FAIL_COND(!inst);
		} break;
		/*case VS::INSTANCE_GI_PROBE: {
			inst = gi_probe_owner.getornull(p_base);
			ERR_FAIL_COND(!inst);
		} break;*/
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

void RasterizerStorageGLES2::instance_remove_dependency(RID p_base, RasterizerScene::InstanceBase *p_instance) {
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
		/*case VS::INSTANCE_PARTICLES: {
			inst = particles_owner.getornull(p_base);
			ERR_FAIL_COND(!inst);
		} break;*/
		case VS::INSTANCE_REFLECTION_PROBE: {
			inst = reflection_probe_owner.getornull(p_base);
			ERR_FAIL_COND(!inst);
		} break;
		case VS::INSTANCE_LIGHT: {
			inst = light_owner.getornull(p_base);
			ERR_FAIL_COND(!inst);
		} break;
		/*case VS::INSTANCE_GI_PROBE: {
			inst = gi_probe_owner.getornull(p_base);
			ERR_FAIL_COND(!inst);
		} break; */
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

void RasterizerStorageGLES2::_render_target_allocate(RenderTarget *rt) {
	// do not allocate a render target with no size
	if (rt->width <= 0 || rt->height <= 0) {
		return;
	}

	// do not allocate a render target that is attached to the screen
	if (rt->flags[RENDER_TARGET_DIRECT_TO_SCREEN]) {
		rt->fbo = RasterizerStorageGLES2::system_fbo;
		return;
	}

	if (rt->width > config.max_viewport_dimensions[0] || rt->height > config.max_viewport_dimensions[1]) {
		WARN_PRINT("Cannot create render target larger than maximum hardware supported size of (" + itos(config.max_viewport_dimensions[0]) + ", " + itos(config.max_viewport_dimensions[1]) + "). Setting size to maximum.");
		rt->width = MIN(rt->width, config.max_viewport_dimensions[0]);
		rt->height = MIN(rt->height, config.max_viewport_dimensions[1]);
	}

	GLuint color_internal_format;
	GLuint color_format;
	GLuint color_type = GL_UNSIGNED_BYTE;
	Image::Format image_format;

	if (rt->flags[RasterizerStorage::RENDER_TARGET_TRANSPARENT]) {
#ifdef GLES_OVER_GL
		color_internal_format = GL_RGBA8;
#else
		color_internal_format = GL_RGBA;
#endif
		color_format = GL_RGBA;
		image_format = Image::FORMAT_RGBA8;
	} else {
#ifdef GLES_OVER_GL
		color_internal_format = GL_RGB8;
#else
		color_internal_format = GL_RGB;
#endif
		color_format = GL_RGB;
		image_format = Image::FORMAT_RGB8;
	}

	rt->used_dof_blur_near = false;
	rt->mip_maps_allocated = false;

	{
		/* Front FBO */

		Texture *texture = texture_owner.getornull(rt->texture);
		ERR_FAIL_COND(!texture);

		// framebuffer
		glGenFramebuffers(1, &rt->fbo);
		glBindFramebuffer(GL_FRAMEBUFFER, rt->fbo);

		// color
		glGenTextures(1, &rt->color);
		glBindTexture(GL_TEXTURE_2D, rt->color);

		glTexImage2D(GL_TEXTURE_2D, 0, color_internal_format, rt->width, rt->height, 0, color_format, color_type, nullptr);

		if (texture->flags & VS::TEXTURE_FLAG_FILTER) {
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		} else {
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		}

		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, rt->color, 0);

		// depth

		if (config.support_depth_texture) {
			glGenTextures(1, &rt->depth);
			glBindTexture(GL_TEXTURE_2D, rt->depth);
			glTexImage2D(GL_TEXTURE_2D, 0, config.depth_internalformat, rt->width, rt->height, 0, GL_DEPTH_COMPONENT, config.depth_type, nullptr);

			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, rt->depth, 0);
		} else {
			glGenRenderbuffers(1, &rt->depth);
			glBindRenderbuffer(GL_RENDERBUFFER, rt->depth);

			glRenderbufferStorage(GL_RENDERBUFFER, config.depth_buffer_internalformat, rt->width, rt->height);

			glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rt->depth);
		}

		GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);

		if (status != GL_FRAMEBUFFER_COMPLETE) {
			glDeleteFramebuffers(1, &rt->fbo);
			if (config.support_depth_texture) {
				glDeleteTextures(1, &rt->depth);
			} else {
				glDeleteRenderbuffers(1, &rt->depth);
			}

			glDeleteTextures(1, &rt->color);
			rt->fbo = 0;
			rt->width = 0;
			rt->height = 0;
			rt->color = 0;
			rt->depth = 0;
			texture->tex_id = 0;
			texture->active = false;
			WARN_PRINT("Could not create framebuffer!!");
			return;
		}

		texture->format = image_format;
		texture->gl_format_cache = color_format;
		texture->gl_type_cache = GL_UNSIGNED_BYTE;
		texture->gl_internal_format_cache = color_internal_format;
		texture->tex_id = rt->color;
		texture->width = rt->width;
		texture->alloc_width = rt->width;
		texture->height = rt->height;
		texture->alloc_height = rt->height;
		texture->active = true;

		texture_set_flags(rt->texture, texture->flags);
	}

	/* BACK FBO */
	/* For MSAA */

#ifndef JAVASCRIPT_ENABLED
	if (rt->msaa >= VS::VIEWPORT_MSAA_2X && rt->msaa <= VS::VIEWPORT_MSAA_16X && config.multisample_supported) {
		rt->multisample_active = true;

		static const int msaa_value[] = { 0, 2, 4, 8, 16 };
		int msaa = msaa_value[rt->msaa];

		int max_samples = 0;
		glGetIntegerv(GL_MAX_SAMPLES, &max_samples);
		if (msaa > max_samples) {
			WARN_PRINT("MSAA must be <= GL_MAX_SAMPLES, falling-back to GL_MAX_SAMPLES = " + itos(max_samples));
			msaa = max_samples;
		}

		//regular fbo
		glGenFramebuffers(1, &rt->multisample_fbo);
		glBindFramebuffer(GL_FRAMEBUFFER, rt->multisample_fbo);

		glGenRenderbuffers(1, &rt->multisample_depth);
		glBindRenderbuffer(GL_RENDERBUFFER, rt->multisample_depth);
		glRenderbufferStorageMultisample(GL_RENDERBUFFER, msaa, config.depth_buffer_internalformat, rt->width, rt->height);

		glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rt->multisample_depth);

#if defined(GLES_OVER_GL) || defined(IPHONE_ENABLED)

		glGenRenderbuffers(1, &rt->multisample_color);
		glBindRenderbuffer(GL_RENDERBUFFER, rt->multisample_color);
		glRenderbufferStorageMultisample(GL_RENDERBUFFER, msaa, color_internal_format, rt->width, rt->height);

		glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, rt->multisample_color);
#elif defined(ANDROID_ENABLED)
		// Render to a texture in android
		glGenTextures(1, &rt->multisample_color);
		glBindTexture(GL_TEXTURE_2D, rt->multisample_color);

		glTexImage2D(GL_TEXTURE_2D, 0, color_internal_format, rt->width, rt->height, 0, color_format, color_type, NULL);

		// multisample buffer is same size as front buffer, so just use nearest
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

		glFramebufferTexture2DMultisample(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, rt->multisample_color, 0, msaa);
#endif

		GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);

		if (status != GL_FRAMEBUFFER_COMPLETE) {
			// Delete allocated resources and default to no MSAA
			WARN_PRINT_ONCE("Cannot allocate back framebuffer for MSAA");
			printf("err status: %x\n", status);
			config.multisample_supported = false;
			rt->multisample_active = false;

			glDeleteFramebuffers(1, &rt->multisample_fbo);
			rt->multisample_fbo = 0;

			glDeleteRenderbuffers(1, &rt->multisample_depth);
			rt->multisample_depth = 0;
#ifdef ANDROID_ENABLED
			glDeleteTextures(1, &rt->multisample_color);
#else
			glDeleteRenderbuffers(1, &rt->multisample_color);
#endif
			rt->multisample_color = 0;
		}

		glBindRenderbuffer(GL_RENDERBUFFER, 0);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
#ifdef ANDROID_ENABLED
		glBindTexture(GL_TEXTURE_2D, 0);
#endif

	} else
#endif // JAVASCRIPT_ENABLED
	{
		rt->multisample_active = false;
	}

	glClearColor(0, 0, 0, 0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	// copy texscreen buffers
	if (!(rt->flags[RasterizerStorage::RENDER_TARGET_NO_SAMPLING])) {
		glGenTextures(1, &rt->copy_screen_effect.color);
		glBindTexture(GL_TEXTURE_2D, rt->copy_screen_effect.color);

		if (rt->flags[RasterizerStorage::RENDER_TARGET_TRANSPARENT]) {
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, rt->width, rt->height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
		} else {
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, rt->width, rt->height, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
		}

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		glGenFramebuffers(1, &rt->copy_screen_effect.fbo);
		glBindFramebuffer(GL_FRAMEBUFFER, rt->copy_screen_effect.fbo);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, rt->copy_screen_effect.color, 0);

		glClearColor(0, 0, 0, 0);
		glClear(GL_COLOR_BUFFER_BIT);

		GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
		if (status != GL_FRAMEBUFFER_COMPLETE) {
			_render_target_clear(rt);
			ERR_FAIL_COND(status != GL_FRAMEBUFFER_COMPLETE);
		}
	}

	// Allocate mipmap chains for post_process effects
	if (!rt->flags[RasterizerStorage::RENDER_TARGET_NO_3D] && rt->width >= 2 && rt->height >= 2) {
		for (int i = 0; i < 2; i++) {
			ERR_FAIL_COND(rt->mip_maps[i].sizes.size());
			int w = rt->width;
			int h = rt->height;

			if (i > 0) {
				w >>= 1;
				h >>= 1;
			}

			int level = 0;
			int fb_w = w;
			int fb_h = h;

			while (true) {
				RenderTarget::MipMaps::Size mm;
				mm.width = w;
				mm.height = h;
				rt->mip_maps[i].sizes.push_back(mm);

				w >>= 1;
				h >>= 1;

				if (w < 2 || h < 2) {
					break;
				}

				level++;
			}

			GLsizei width = fb_w;
			GLsizei height = fb_h;

			if (config.render_to_mipmap_supported) {
				glGenTextures(1, &rt->mip_maps[i].color);
				glBindTexture(GL_TEXTURE_2D, rt->mip_maps[i].color);

				for (int l = 0; l < level + 1; l++) {
					glTexImage2D(GL_TEXTURE_2D, l, color_internal_format, width, height, 0, color_format, color_type, nullptr);
					width = MAX(1, (width / 2));
					height = MAX(1, (height / 2));
				}
#ifdef GLES_OVER_GL
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, level);
#endif
			} else {
				// Can't render to specific levels of a mipmap in ES 2.0 or Webgl so create a texture for each level
				for (int l = 0; l < level + 1; l++) {
					glGenTextures(1, &rt->mip_maps[i].sizes.write[l].color);
					glBindTexture(GL_TEXTURE_2D, rt->mip_maps[i].sizes[l].color);
					glTexImage2D(GL_TEXTURE_2D, 0, color_internal_format, width, height, 0, color_format, color_type, nullptr);
					width = MAX(1, (width / 2));
					height = MAX(1, (height / 2));
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
					glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
					glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
				}
			}

			glDisable(GL_SCISSOR_TEST);
			glColorMask(1, 1, 1, 1);
			glDepthMask(GL_TRUE);

			for (int j = 0; j < rt->mip_maps[i].sizes.size(); j++) {
				RenderTarget::MipMaps::Size &mm = rt->mip_maps[i].sizes.write[j];

				glGenFramebuffers(1, &mm.fbo);
				glBindFramebuffer(GL_FRAMEBUFFER, mm.fbo);

				if (config.render_to_mipmap_supported) {
					glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, rt->mip_maps[i].color, j);
				} else {
					glBindTexture(GL_TEXTURE_2D, rt->mip_maps[i].sizes[j].color);
					glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, rt->mip_maps[i].sizes[j].color, 0);
				}

				bool used_depth = false;
				if (j == 0 && i == 0) { //use always
					if (config.support_depth_texture) {
						glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, rt->depth, 0);
					} else {
						glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rt->depth);
					}
					used_depth = true;
				}

				GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
				if (status != GL_FRAMEBUFFER_COMPLETE) {
					WARN_PRINT_ONCE("Cannot allocate mipmaps for 3D post processing effects");
					glBindFramebuffer(GL_FRAMEBUFFER, RasterizerStorageGLES2::system_fbo);
					return;
				}

				glClearColor(1.0, 0.0, 1.0, 0.0);
				glClear(GL_COLOR_BUFFER_BIT);
				if (used_depth) {
					glClearDepth(1.0);
					glClear(GL_DEPTH_BUFFER_BIT);
				}
			}

			rt->mip_maps[i].levels = level;

			if (config.render_to_mipmap_supported) {
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
				glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
				glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			}
		}
		rt->mip_maps_allocated = true;
	}

	glBindFramebuffer(GL_FRAMEBUFFER, RasterizerStorageGLES2::system_fbo);
}

void RasterizerStorageGLES2::_render_target_clear(RenderTarget *rt) {
	// there is nothing to clear when DIRECT_TO_SCREEN is used
	if (rt->flags[RENDER_TARGET_DIRECT_TO_SCREEN]) {
		return;
	}

	if (rt->fbo) {
		glDeleteFramebuffers(1, &rt->fbo);
		glDeleteTextures(1, &rt->color);
		rt->fbo = 0;
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

		if (rt->external.depth != 0 && rt->external.depth_owned) {
			glDeleteRenderbuffers(1, &rt->external.depth);
		}

		rt->external.fbo = 0;
		rt->external.color = 0;
		rt->external.depth = 0;
		rt->external.depth_owned = false;
	}

	if (rt->depth) {
		if (config.support_depth_texture) {
			glDeleteTextures(1, &rt->depth);
		} else {
			glDeleteRenderbuffers(1, &rt->depth);
		}

		rt->depth = 0;
	}

	if (rt->copy_screen_effect.color) {
		glDeleteFramebuffers(1, &rt->copy_screen_effect.fbo);
		rt->copy_screen_effect.fbo = 0;

		glDeleteTextures(1, &rt->copy_screen_effect.color);
		rt->copy_screen_effect.color = 0;
	}

	for (int i = 0; i < 2; i++) {
		if (rt->mip_maps[i].sizes.size()) {
			for (int j = 0; j < rt->mip_maps[i].sizes.size(); j++) {
				glDeleteFramebuffers(1, &rt->mip_maps[i].sizes[j].fbo);
				glDeleteTextures(1, &rt->mip_maps[i].sizes[j].color);
			}

			glDeleteTextures(1, &rt->mip_maps[i].color);
			rt->mip_maps[i].sizes.clear();
			rt->mip_maps[i].levels = 0;
			rt->mip_maps[i].color = 0;
		}
	}

	if (rt->multisample_active) {
		glDeleteFramebuffers(1, &rt->multisample_fbo);
		rt->multisample_fbo = 0;

		glDeleteRenderbuffers(1, &rt->multisample_depth);
		rt->multisample_depth = 0;
#ifdef ANDROID_ENABLED
		glDeleteTextures(1, &rt->multisample_color);
#else
		glDeleteRenderbuffers(1, &rt->multisample_color);
#endif
		rt->multisample_color = 0;
	}
}

RID RasterizerStorageGLES2::render_target_create() {
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
	t->total_data_size = 0;
	t->ignore_mipmaps = false;
	t->compressed = false;
	t->mipmaps = 1;
	t->active = true;
	t->tex_id = 0;
	t->render_target = rt;

	rt->texture = texture_owner.make_rid(t);

	return render_target_owner.make_rid(rt);
}

void RasterizerStorageGLES2::render_target_set_position(RID p_render_target, int p_x, int p_y) {
	RenderTarget *rt = render_target_owner.getornull(p_render_target);
	ERR_FAIL_COND(!rt);

	rt->x = p_x;
	rt->y = p_y;
}

void RasterizerStorageGLES2::render_target_set_size(RID p_render_target, int p_width, int p_height) {
	RenderTarget *rt = render_target_owner.getornull(p_render_target);
	ERR_FAIL_COND(!rt);

	if (p_width == rt->width && p_height == rt->height) {
		return;
	}

	_render_target_clear(rt);

	rt->width = p_width;
	rt->height = p_height;

	_render_target_allocate(rt);
}

RID RasterizerStorageGLES2::render_target_get_texture(RID p_render_target) const {
	RenderTarget *rt = render_target_owner.getornull(p_render_target);
	ERR_FAIL_COND_V(!rt, RID());

	return rt->texture;
}

uint32_t RasterizerStorageGLES2::render_target_get_depth_texture_id(RID p_render_target) const {
	RenderTarget *rt = render_target_owner.getornull(p_render_target);
	ERR_FAIL_COND_V(!rt, 0);

	if (rt->external.depth == 0) {
		return rt->depth;
	} else {
		return rt->external.depth;
	}
}

void RasterizerStorageGLES2::render_target_set_external_texture(RID p_render_target, unsigned int p_texture_id, unsigned int p_depth_id) {
	RenderTarget *rt = render_target_owner.getornull(p_render_target);
	ERR_FAIL_COND(!rt);

	if (p_texture_id == 0) {
		if (rt->external.fbo != 0) {
			// free this
			glDeleteFramebuffers(1, &rt->external.fbo);

			// and this
			if (rt->external.depth != 0 && rt->external.depth_owned) {
				glDeleteRenderbuffers(1, &rt->external.depth);
			}

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

		// Switch our texture on our frame buffer
#ifdef ANDROID_ENABLED
		if (rt->msaa >= VS::VIEWPORT_MSAA_EXT_2X && rt->msaa <= VS::VIEWPORT_MSAA_EXT_4X) {
			// This code only applies to the Oculus Go and Oculus Quest. Due to the the tiled nature
			// of the GPU we can do a single render pass by rendering directly into our texture chains
			// texture and apply MSAA as we render.

			// On any other hardware these two modes are ignored and we do not have any MSAA,
			// the normal MSAA modes need to be used to enable our two pass approach

			// If we created a depth buffer before and we're now passed one, we need to clear it out
			if (rt->external.depth != 0 && rt->external.depth_owned && p_depth_id != 0) {
				glDeleteRenderbuffers(1, &rt->external.depth);
				rt->external.depth_owned = false;
				rt->external.depth = 0;
			}

			if (!rt->external.depth_owned) {
				rt->external.depth = p_depth_id;
			}

			static const int msaa_value[] = { 2, 4 };
			int msaa = msaa_value[rt->msaa - VS::VIEWPORT_MSAA_EXT_2X];

			if (rt->external.depth == 0) {
				rt->external.depth_owned = true;

				// create a multisample depth buffer, we're not reusing Godots because Godot's didn't get created..
				glGenRenderbuffers(1, &rt->external.depth);
				glBindRenderbuffer(GL_RENDERBUFFER, rt->external.depth);
				glRenderbufferStorageMultisample(GL_RENDERBUFFER, msaa, config.depth_buffer_internalformat, rt->width, rt->height);
				glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rt->external.depth);
			} else if (!rt->external.depth_owned) {
				// we make an exception here, external plugin MUST make sure this is a proper multisample render buffer!
				glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rt->external.depth);
			}

			// and set our external texture as the texture...
			glFramebufferTexture2DMultisample(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, p_texture_id, 0, msaa);

		} else
#endif
		{
			// if MSAA as on before, clear our render buffer
			if (rt->external.depth != 0 && rt->external.depth_owned) {
				glDeleteRenderbuffers(1, &rt->external.depth);
			}
			rt->external.depth_owned = false;
			rt->external.depth = p_depth_id;

			// set our texture as the destination for our framebuffer
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, p_texture_id, 0);

			// seeing we're rendering into this directly, better also use our depth buffer, just use our existing one :)
			if (rt->external.depth != 0) {
				glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, rt->external.depth, 0);
			} else if (config.support_depth_texture) {
				glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, rt->depth, 0);
			} else {
				glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rt->depth);
			}
		}

		// check status and unbind
		GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
		glBindFramebuffer(GL_FRAMEBUFFER, RasterizerStorageGLES2::system_fbo);

		if (status != GL_FRAMEBUFFER_COMPLETE) {
			printf("framebuffer fail, status: %x\n", status);
		}

		ERR_FAIL_COND(status != GL_FRAMEBUFFER_COMPLETE);
	}
}

void RasterizerStorageGLES2::render_target_set_flag(RID p_render_target, RenderTargetFlags p_flag, bool p_value) {
	RenderTarget *rt = render_target_owner.getornull(p_render_target);
	ERR_FAIL_COND(!rt);

	// When setting DIRECT_TO_SCREEN, you need to clear before the value is set, but allocate after as
	// those functions change how they operate depending on the value of DIRECT_TO_SCREEN
	if (p_flag == RENDER_TARGET_DIRECT_TO_SCREEN && p_value != rt->flags[RENDER_TARGET_DIRECT_TO_SCREEN]) {
		_render_target_clear(rt);
		rt->flags[p_flag] = p_value;
		_render_target_allocate(rt);
	}

	rt->flags[p_flag] = p_value;

	switch (p_flag) {
		case RENDER_TARGET_TRANSPARENT:
		case RENDER_TARGET_HDR:
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

bool RasterizerStorageGLES2::render_target_was_used(RID p_render_target) {
	RenderTarget *rt = render_target_owner.getornull(p_render_target);
	ERR_FAIL_COND_V(!rt, false);

	return rt->used_in_frame;
}

void RasterizerStorageGLES2::render_target_clear_used(RID p_render_target) {
	RenderTarget *rt = render_target_owner.getornull(p_render_target);
	ERR_FAIL_COND(!rt);

	rt->used_in_frame = false;
}

void RasterizerStorageGLES2::render_target_set_msaa(RID p_render_target, VS::ViewportMSAA p_msaa) {
	RenderTarget *rt = render_target_owner.getornull(p_render_target);
	ERR_FAIL_COND(!rt);

	if (rt->msaa == p_msaa) {
		return;
	}

	if (!config.multisample_supported) {
		ERR_PRINT("MSAA not supported on this hardware.");
		return;
	}

	_render_target_clear(rt);
	rt->msaa = p_msaa;
	_render_target_allocate(rt);
}

void RasterizerStorageGLES2::render_target_set_use_fxaa(RID p_render_target, bool p_fxaa) {
	RenderTarget *rt = render_target_owner.getornull(p_render_target);
	ERR_FAIL_COND(!rt);

	rt->use_fxaa = p_fxaa;
}

void RasterizerStorageGLES2::render_target_set_use_debanding(RID p_render_target, bool p_debanding) {
	RenderTarget *rt = render_target_owner.getornull(p_render_target);
	ERR_FAIL_COND(!rt);

	if (p_debanding) {
		WARN_PRINT_ONCE("Debanding is not supported in the GLES2 backend. To use debanding, switch to the GLES3 backend and make sure HDR is enabled.");
	}

	rt->use_debanding = p_debanding;
}

void RasterizerStorageGLES2::render_target_set_sharpen_intensity(RID p_render_target, float p_intensity) {
	RenderTarget *rt = render_target_owner.getornull(p_render_target);
	ERR_FAIL_COND(!rt);

	if (p_intensity >= 0.001) {
		WARN_PRINT_ONCE("Sharpening is not supported in the GLES2 backend. To use sharpening, switch to the GLES3 backend.");
	}

	rt->sharpen_intensity = p_intensity;
}

/* CANVAS SHADOW */

RID RasterizerStorageGLES2::canvas_light_shadow_buffer_create(int p_width) {
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
	glRenderbufferStorage(GL_RENDERBUFFER, config.depth_buffer_internalformat, cls->size, cls->height);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, cls->depth);

	glGenTextures(1, &cls->distance);
	glBindTexture(GL_TEXTURE_2D, cls->distance);
	if (config.use_rgba_2d_shadows) {
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, cls->size, cls->height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
	} else {
#ifdef GLES_OVER_GL
		glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, cls->size, cls->height, 0, _RED_OES, GL_FLOAT, nullptr);
#else
		glTexImage2D(GL_TEXTURE_2D, 0, GL_FLOAT, cls->size, cls->height, 0, _RED_OES, GL_FLOAT, NULL);
#endif
	}

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, cls->distance, 0);

	GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	//printf("errnum: %x\n",status);
	glBindFramebuffer(GL_FRAMEBUFFER, RasterizerStorageGLES2::system_fbo);

	if (status != GL_FRAMEBUFFER_COMPLETE) {
		memdelete(cls);
		ERR_FAIL_COND_V(status != GL_FRAMEBUFFER_COMPLETE, RID());
	}

	return canvas_light_shadow_owner.make_rid(cls);
}

/* LIGHT SHADOW MAPPING */

RID RasterizerStorageGLES2::canvas_light_occluder_create() {
	CanvasOccluder *co = memnew(CanvasOccluder);
	co->index_id = 0;
	co->vertex_id = 0;
	co->len = 0;

	return canvas_occluder_owner.make_rid(co);
}

void RasterizerStorageGLES2::canvas_light_occluder_set_polylines(RID p_occluder, const PoolVector<Vector2> &p_lines) {
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
	}
}

VS::InstanceType RasterizerStorageGLES2::get_base_type(RID p_rid) const {
	if (mesh_owner.owns(p_rid)) {
		return VS::INSTANCE_MESH;
	} else if (light_owner.owns(p_rid)) {
		return VS::INSTANCE_LIGHT;
	} else if (multimesh_owner.owns(p_rid)) {
		return VS::INSTANCE_MULTIMESH;
	} else if (immediate_owner.owns(p_rid)) {
		return VS::INSTANCE_IMMEDIATE;
	} else if (reflection_probe_owner.owns(p_rid)) {
		return VS::INSTANCE_REFLECTION_PROBE;
	} else if (lightmap_capture_data_owner.owns(p_rid)) {
		return VS::INSTANCE_LIGHTMAP_CAPTURE;
	} else {
		return VS::INSTANCE_NONE;
	}
}

bool RasterizerStorageGLES2::free(RID p_rid) {
	if (render_target_owner.owns(p_rid)) {
		RenderTarget *rt = render_target_owner.getornull(p_rid);
		_render_target_clear(rt);

		Texture *t = texture_owner.get(rt->texture);
		texture_owner.free(rt->texture);
		memdelete(t);
		render_target_owner.free(p_rid);
		memdelete(rt);

		return true;
	} else if (texture_owner.owns(p_rid)) {
		Texture *t = texture_owner.get(p_rid);
		// can't free a render target texture
		ERR_FAIL_COND_V(t->render_target, true);

		info.texture_mem -= t->total_data_size;
		texture_owner.free(p_rid);
		memdelete(t);

		return true;
	} else if (sky_owner.owns(p_rid)) {
		Sky *sky = sky_owner.get(p_rid);
		sky_set_texture(p_rid, RID(), 256);
		sky_owner.free(p_rid);
		memdelete(sky);

		return true;
	} else if (shader_owner.owns(p_rid)) {
		Shader *shader = shader_owner.get(p_rid);

		if (shader->shader && shader->custom_code_id) {
			shader->shader->free_custom_shader(shader->custom_code_id);
		}

		if (shader->dirty_list.in_list()) {
			_shader_dirty_list.remove(&shader->dirty_list);
		}

		while (shader->materials.first()) {
			Material *m = shader->materials.first()->self();

			m->shader = nullptr;
			_material_make_dirty(m);

			shader->materials.remove(shader->materials.first());
		}

		shader_owner.free(p_rid);
		memdelete(shader);

		return true;
	} else if (material_owner.owns(p_rid)) {
		Material *m = material_owner.get(p_rid);

		if (m->shader) {
			m->shader->materials.remove(&m->list);
		}

		for (Map<Geometry *, int>::Element *E = m->geometry_owners.front(); E; E = E->next()) {
			Geometry *g = E->key();
			g->material = RID();
		}

		for (Map<RasterizerScene::InstanceBase *, int>::Element *E = m->instance_owners.front(); E; E = E->next()) {
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
		memdelete(m);

		return true;
	} else if (skeleton_owner.owns(p_rid)) {
		Skeleton *s = skeleton_owner.get(p_rid);

		if (s->update_list.in_list()) {
			skeleton_update_list.remove(&s->update_list);
		}

		for (Set<RasterizerScene::InstanceBase *>::Element *E = s->instances.front(); E; E = E->next()) {
			E->get()->skeleton = RID();
		}

		skeleton_allocate(p_rid, 0, false);

		if (s->tex_id) {
			glDeleteTextures(1, &s->tex_id);
		}

		skeleton_owner.free(p_rid);
		memdelete(s);

		return true;
	} else if (mesh_owner.owns(p_rid)) {
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

		return true;
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

		multimesh->instance_remove_deps();

		if (multimesh->mesh.is_valid()) {
			Mesh *mesh = mesh_owner.getornull(multimesh->mesh);
			if (mesh) {
				mesh->multimeshes.remove(&multimesh->mesh_list);
			}
		}

		multimesh_allocate(p_rid, 0, VS::MULTIMESH_TRANSFORM_3D, VS::MULTIMESH_COLOR_NONE);

		update_dirty_multimeshes();

		multimesh_owner.free(p_rid);
		memdelete(multimesh);

		return true;
	} else if (immediate_owner.owns(p_rid)) {
		Immediate *im = immediate_owner.get(p_rid);
		im->instance_remove_deps();

		immediate_owner.free(p_rid);
		memdelete(im);

		return true;
	} else if (light_owner.owns(p_rid)) {
		Light *light = light_owner.get(p_rid);
		light->instance_remove_deps();

		light_owner.free(p_rid);
		memdelete(light);

		return true;
	} else if (reflection_probe_owner.owns(p_rid)) {
		// delete the texture
		ReflectionProbe *reflection_probe = reflection_probe_owner.get(p_rid);
		reflection_probe->instance_remove_deps();

		reflection_probe_owner.free(p_rid);
		memdelete(reflection_probe);

		return true;
	} else if (lightmap_capture_data_owner.owns(p_rid)) {
		// delete the texture
		LightmapCapture *lightmap_capture = lightmap_capture_data_owner.get(p_rid);
		lightmap_capture->instance_remove_deps();

		lightmap_capture_data_owner.free(p_rid);
		memdelete(lightmap_capture);
		return true;

	} else if (canvas_occluder_owner.owns(p_rid)) {
		CanvasOccluder *co = canvas_occluder_owner.get(p_rid);
		if (co->index_id) {
			glDeleteBuffers(1, &co->index_id);
		}
		if (co->vertex_id) {
			glDeleteBuffers(1, &co->vertex_id);
		}

		canvas_occluder_owner.free(p_rid);
		memdelete(co);

		return true;

	} else if (canvas_light_shadow_owner.owns(p_rid)) {
		CanvasLightShadow *cls = canvas_light_shadow_owner.get(p_rid);
		glDeleteFramebuffers(1, &cls->fbo);
		glDeleteRenderbuffers(1, &cls->depth);
		glDeleteTextures(1, &cls->distance);
		canvas_light_shadow_owner.free(p_rid);
		memdelete(cls);

		return true;
	} else {
		return false;
	}
}

bool RasterizerStorageGLES2::has_os_feature(const String &p_feature) const {
	if (p_feature == "pvrtc") {
		return config.pvrtc_supported;
	}

	if (p_feature == "s3tc") {
		return config.s3tc_supported;
	}

	if (p_feature == "etc") {
		return config.etc1_supported;
	}

	if (p_feature == "skinning_fallback") {
		return config.use_skeleton_software;
	}

	return false;
}

////////////////////////////////////////////

void RasterizerStorageGLES2::set_debug_generate_wireframes(bool p_generate) {
}

void RasterizerStorageGLES2::render_info_begin_capture() {
	info.snap = info.render;
}

void RasterizerStorageGLES2::render_info_end_capture() {
	info.snap.object_count = info.render.object_count - info.snap.object_count;
	info.snap.draw_call_count = info.render.draw_call_count - info.snap.draw_call_count;
	info.snap.material_switch_count = info.render.material_switch_count - info.snap.material_switch_count;
	info.snap.surface_switch_count = info.render.surface_switch_count - info.snap.surface_switch_count;
	info.snap.shader_rebind_count = info.render.shader_rebind_count - info.snap.shader_rebind_count;
	info.snap.vertices_count = info.render.vertices_count - info.snap.vertices_count;
	info.snap._2d_item_count = info.render._2d_item_count - info.snap._2d_item_count;
	info.snap._2d_draw_call_count = info.render._2d_draw_call_count - info.snap._2d_draw_call_count;
}

int RasterizerStorageGLES2::get_captured_render_info(VS::RenderInfo p_info) {
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

uint64_t RasterizerStorageGLES2::get_render_info(VS::RenderInfo p_info) {
	switch (p_info) {
		case VS::INFO_OBJECTS_IN_FRAME:
			return info.render_final.object_count;
		case VS::INFO_VERTICES_IN_FRAME:
			return info.render_final.vertices_count;
		case VS::INFO_MATERIAL_CHANGES_IN_FRAME:
			return info.render_final.material_switch_count;
		case VS::INFO_SHADER_CHANGES_IN_FRAME:
			return info.render_final.shader_rebind_count;
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

String RasterizerStorageGLES2::get_video_adapter_name() const {
	return (const char *)glGetString(GL_RENDERER);
}

String RasterizerStorageGLES2::get_video_adapter_vendor() const {
	return (const char *)glGetString(GL_VENDOR);
}

void RasterizerStorageGLES2::initialize() {
	RasterizerStorageGLES2::system_fbo = 0;

	{
		const GLubyte *extension_string = glGetString(GL_EXTENSIONS);

		Vector<String> extensions = String((const char *)extension_string).split(" ");

		for (int i = 0; i < extensions.size(); i++) {
			config.extensions.insert(extensions[i]);
		}
	}

	config.keep_original_textures = false;
	config.shrink_textures_x2 = false;
	config.depth_internalformat = GL_DEPTH_COMPONENT;
	config.depth_type = GL_UNSIGNED_INT;

	// Initialize GLWrapper early on, as required for any calls to glActiveTexture.
	config.max_texture_image_units = safe_gl_get_integer(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, Config::max_desired_texture_image_units);
	gl_wrapper.initialize(config.max_texture_image_units);

#ifdef GLES_OVER_GL
	config.float_texture_supported = true;
	config.s3tc_supported = true;
	config.pvrtc_supported = false;
	config.etc1_supported = false;
	config.support_npot_repeat_mipmap = true;
	config.depth_buffer_internalformat = GL_DEPTH_COMPONENT24;
#else
	config.float_texture_supported = config.extensions.has("GL_ARB_texture_float") || config.extensions.has("GL_OES_texture_float");
	config.s3tc_supported = config.extensions.has("GL_EXT_texture_compression_s3tc") || config.extensions.has("WEBGL_compressed_texture_s3tc");
	config.etc1_supported = config.extensions.has("GL_OES_compressed_ETC1_RGB8_texture") || config.extensions.has("WEBGL_compressed_texture_etc1");
	config.pvrtc_supported = config.extensions.has("GL_IMG_texture_compression_pvrtc") || config.extensions.has("WEBGL_compressed_texture_pvrtc");
	config.support_npot_repeat_mipmap = config.extensions.has("GL_OES_texture_npot");

	// If the desktop build is using S3TC, and you export / run from the IDE for android, if the device supports
	// S3TC it will crash trying to load these textures, as they are not exported in the APK. This is a simple way
	// to prevent Android devices trying to load S3TC, by faking lack of hardware support.
#if defined(ANDROID_ENABLED) || defined(IPHONE_ENABLED)
	config.s3tc_supported = false;
#endif

#ifdef JAVASCRIPT_ENABLED
	// RenderBuffer internal format must be 16 bits in WebGL,
	// but depth_texture should default to 32 always
	// if the implementation doesn't support 32, it should just quietly use 16 instead
	// https://www.khronos.org/registry/webgl/extensions/WEBGL_depth_texture/
	config.depth_buffer_internalformat = GL_DEPTH_COMPONENT16;
	config.depth_type = GL_UNSIGNED_INT;
#else
	// on mobile check for 24 bit depth support for RenderBufferStorage
	if (config.extensions.has("GL_OES_depth24")) {
		config.depth_buffer_internalformat = _DEPTH_COMPONENT24_OES;
		config.depth_type = GL_UNSIGNED_INT;
	} else {
		config.depth_buffer_internalformat = GL_DEPTH_COMPONENT16;
		config.depth_type = GL_UNSIGNED_SHORT;
	}
#endif
#endif

#ifndef GLES_OVER_GL
	//Manually load extensions for android and ios

#ifdef IPHONE_ENABLED
	// appears that IPhone doesn't need to dlopen TODO: test this rigorously before removing
	//void *gles2_lib = dlopen(NULL, RTLD_LAZY);
	//glRenderbufferStorageMultisampleAPPLE = dlsym(gles2_lib, "glRenderbufferStorageMultisampleAPPLE");
	//glResolveMultisampleFramebufferAPPLE = dlsym(gles2_lib, "glResolveMultisampleFramebufferAPPLE");
#elif defined(ANDROID_ENABLED)

	void *gles2_lib = dlopen("libGLESv2.so", RTLD_LAZY);
	glRenderbufferStorageMultisampleEXT = (PFNGLRENDERBUFFERSTORAGEMULTISAMPLEEXTPROC)dlsym(gles2_lib, "glRenderbufferStorageMultisampleEXT");
	glFramebufferTexture2DMultisampleEXT = (PFNGLFRAMEBUFFERTEXTURE2DMULTISAMPLEEXTPROC)dlsym(gles2_lib, "glFramebufferTexture2DMultisampleEXT");
#endif
#endif

	// Check for multisample support
	config.multisample_supported = config.extensions.has("GL_EXT_framebuffer_multisample") || config.extensions.has("GL_EXT_multisampled_render_to_texture") || config.extensions.has("GL_APPLE_framebuffer_multisample");

#ifdef GLES_OVER_GL
	//TODO: causes huge problems with desktop video drivers. Making false for now, needs to be true to render SCREEN_TEXTURE mipmaps
	config.render_to_mipmap_supported = false;
#else
	//check if mipmaps can be used for SCREEN_TEXTURE and Glow on Mobile and web platforms
	config.render_to_mipmap_supported = config.extensions.has("GL_OES_fbo_render_mipmap") && config.extensions.has("GL_EXT_texture_lod");
#endif

#ifdef GLES_OVER_GL
	config.use_rgba_2d_shadows = false;
	config.support_depth_texture = true;
	config.use_rgba_3d_shadows = false;
	config.support_depth_cubemaps = true;
#else
	config.use_rgba_2d_shadows = !(config.float_texture_supported && config.extensions.has("GL_EXT_texture_rg"));
	config.support_depth_texture = config.extensions.has("GL_OES_depth_texture") || config.extensions.has("WEBGL_depth_texture");
	config.use_rgba_3d_shadows = !config.support_depth_texture;
	config.support_depth_cubemaps = config.extensions.has("GL_OES_depth_texture_cube_map");
#endif

#ifdef GLES_OVER_GL
	config.support_32_bits_indices = true;
#else
	config.support_32_bits_indices = config.extensions.has("GL_OES_element_index_uint");
#endif

#ifdef GLES_OVER_GL
	config.support_write_depth = true;
#elif defined(JAVASCRIPT_ENABLED)
	config.support_write_depth = false;
#else
	config.support_write_depth = config.extensions.has("GL_EXT_frag_depth");
#endif

	config.support_half_float_vertices = true;
//every platform should support this except web, iOS has issues with their support, so add option to disable
#ifdef JAVASCRIPT_ENABLED
	config.support_half_float_vertices = false;
#endif
	bool disable_half_float = GLOBAL_GET("rendering/gles2/compatibility/disable_half_float");
	if (disable_half_float) {
		config.support_half_float_vertices = false;
	}

	config.rgtc_supported = config.extensions.has("GL_EXT_texture_compression_rgtc") || config.extensions.has("GL_ARB_texture_compression_rgtc") || config.extensions.has("EXT_texture_compression_rgtc");
	config.bptc_supported = config.extensions.has("GL_ARB_texture_compression_bptc") || config.extensions.has("EXT_texture_compression_bptc");

	config.anisotropic_level = 1.0;
	config.use_anisotropic_filter = config.extensions.has("GL_EXT_texture_filter_anisotropic");
	if (config.use_anisotropic_filter) {
		glGetFloatv(_GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &config.anisotropic_level);
		config.anisotropic_level = MIN(int(ProjectSettings::get_singleton()->get("rendering/quality/filters/anisotropic_filter_level")), config.anisotropic_level);
	}

	//determine formats for depth textures (or renderbuffers)
	if (config.support_depth_texture) {
		// Will use texture for depth
		// have to manually see if we can create a valid framebuffer texture using UNSIGNED_INT,
		// as there is no extension to test for this.
		GLuint fbo;
		glGenFramebuffers(1, &fbo);
		glBindFramebuffer(GL_FRAMEBUFFER, fbo);
		GLuint depth;
		glGenTextures(1, &depth);
		glBindTexture(GL_TEXTURE_2D, depth);
		glTexImage2D(GL_TEXTURE_2D, 0, config.depth_internalformat, 32, 32, 0, GL_DEPTH_COMPONENT, config.depth_type, nullptr);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depth, 0);

		GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);

		glBindFramebuffer(GL_FRAMEBUFFER, system_fbo);
		glDeleteFramebuffers(1, &fbo);
		glBindTexture(GL_TEXTURE_2D, 0);
		glDeleteTextures(1, &depth);

		if (status != GL_FRAMEBUFFER_COMPLETE) {
			// If it fails, test to see if it supports a framebuffer texture using UNSIGNED_SHORT
			// This is needed because many OSX devices don't support either UNSIGNED_INT or UNSIGNED_SHORT
#ifdef GLES_OVER_GL
			config.depth_internalformat = GL_DEPTH_COMPONENT16;
#else
			// OES_depth_texture extension only specifies GL_DEPTH_COMPONENT.
			config.depth_internalformat = GL_DEPTH_COMPONENT;
#endif
			config.depth_type = GL_UNSIGNED_SHORT;

			glGenFramebuffers(1, &fbo);
			glBindFramebuffer(GL_FRAMEBUFFER, fbo);

			glGenTextures(1, &depth);
			glBindTexture(GL_TEXTURE_2D, depth);
			glTexImage2D(GL_TEXTURE_2D, 0, config.depth_internalformat, 32, 32, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT, nullptr);

			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depth, 0);

			status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
			if (status != GL_FRAMEBUFFER_COMPLETE) {
				//if it fails again depth textures aren't supported, use rgba shadows and renderbuffer for depth
				config.support_depth_texture = false;
				config.use_rgba_3d_shadows = true;
			}

			glBindFramebuffer(GL_FRAMEBUFFER, system_fbo);
			glDeleteFramebuffers(1, &fbo);
			glBindTexture(GL_TEXTURE_2D, 0);
			glDeleteTextures(1, &depth);
		}
	}

	//picky requirements for these
	config.support_shadow_cubemaps = config.support_depth_texture && config.support_write_depth && config.support_depth_cubemaps;
	if (!config.support_shadow_cubemaps) {
		print_verbose("OmniLight cubemap shadows are not supported by this GPU. Falling back to dual paraboloid shadows for all omni lights (faster but less precise).");
	}

	frame.count = 0;
	frame.delta = 0;
	frame.current_rt = nullptr;
	frame.clear_request = false;

	glGetIntegerv(GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS, &config.max_vertex_texture_image_units);
	glGetIntegerv(GL_MAX_TEXTURE_SIZE, &config.max_texture_size);
	glGetIntegerv(GL_MAX_CUBE_MAP_TEXTURE_SIZE, &config.max_cubemap_texture_size);
	glGetIntegerv(GL_MAX_VIEWPORT_DIMS, config.max_viewport_dimensions);

	// the use skeleton software path should be used if either float texture is not supported,
	// OR max_vertex_texture_image_units is zero
	config.use_skeleton_software = (config.float_texture_supported == false) || (config.max_vertex_texture_image_units == 0);

	shaders.copy.init();
	shaders.cubemap_filter.init();
	bool ggx_hq = GLOBAL_GET("rendering/quality/reflections/high_quality_ggx");
	shaders.cubemap_filter.set_conditional(CubemapFilterShaderGLES2::LOW_QUALITY, !ggx_hq);

	{
		// quad for copying stuff

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

		glBindBuffer(GL_ARRAY_BUFFER, 0);
	}

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
	}

	// skeleton buffer
	{
		resources.skeleton_transform_buffer_size = 0;
		glGenBuffers(1, &resources.skeleton_transform_buffer);
	}

	// blend buffer
	{
		resources.blend_shape_transform_cpu_buffer_size = 0;
	}

	// radical inverse vdc cache texture
	// used for cubemap filtering
	if (true /*||config.float_texture_supported*/) { //uint8 is similar and works everywhere
		glGenTextures(1, &resources.radical_inverse_vdc_cache_tex);

		gl_wrapper.gl_active_texture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, resources.radical_inverse_vdc_cache_tex);

		uint8_t radical_inverse[512];

		for (uint32_t i = 0; i < 512; i++) {
			uint32_t bits = i;

			bits = (bits << 16) | (bits >> 16);
			bits = ((bits & 0x55555555) << 1) | ((bits & 0xAAAAAAAA) >> 1);
			bits = ((bits & 0x33333333) << 2) | ((bits & 0xCCCCCCCC) >> 2);
			bits = ((bits & 0x0F0F0F0F) << 4) | ((bits & 0xF0F0F0F0) >> 4);
			bits = ((bits & 0x00FF00FF) << 8) | ((bits & 0xFF00FF00) >> 8);

			float value = float(bits) * 2.3283064365386963e-10;
			radical_inverse[i] = uint8_t(CLAMP(value * 255.0, 0, 255));
		}

		glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, 512, 1, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, radical_inverse);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST); //need this for proper sampling

		glBindTexture(GL_TEXTURE_2D, 0);
	}

	{
		glGenFramebuffers(1, &resources.mipmap_blur_fbo);
		glGenTextures(1, &resources.mipmap_blur_color);
	}

#ifdef GLES_OVER_GL
	//this needs to be enabled manually in OpenGL 2.1

	if (config.extensions.has("GL_ARB_seamless_cube_map")) {
		glEnable(_EXT_TEXTURE_CUBE_MAP_SEAMLESS);
	}
	glEnable(GL_POINT_SPRITE);
	glEnable(GL_VERTEX_PROGRAM_POINT_SIZE);
#endif

	config.force_vertex_shading = GLOBAL_GET("rendering/quality/shading/force_vertex_shading");
	config.use_fast_texture_filter = GLOBAL_GET("rendering/quality/filters/use_nearest_mipmap_filter");
	GLOBAL_DEF_RST("rendering/quality/lightmapping/use_bicubic_sampling", true);
	GLOBAL_DEF_RST("rendering/quality/lightmapping/use_bicubic_sampling.mobile", false);
	config.use_lightmap_filter_bicubic = GLOBAL_GET("rendering/quality/lightmapping/use_bicubic_sampling");

	config.use_physical_light_attenuation = GLOBAL_GET("rendering/quality/shading/use_physical_light_attenuation");

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

void RasterizerStorageGLES2::finalize() {
}

void RasterizerStorageGLES2::_copy_screen() {
	bind_quad_array();
	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
}

void RasterizerStorageGLES2::update_dirty_resources() {
	update_dirty_shaders();
	update_dirty_materials();
	update_dirty_blend_shapes();
	update_dirty_skeletons();
	update_dirty_multimeshes();
	update_dirty_captures();
}

RasterizerStorageGLES2::RasterizerStorageGLES2() {
	RasterizerStorageGLES2::system_fbo = 0;
	config.should_orphan = true;
}

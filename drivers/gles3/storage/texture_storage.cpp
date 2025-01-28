/**************************************************************************/
/*  texture_storage.cpp                                                   */
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

#include "texture_storage.h"

#include "../effects/copy_effects.h"
#include "../rasterizer_gles3.h"
#include "config.h"
#include "utilities.h"

#ifdef ANDROID_ENABLED
#define glFramebufferTextureMultiviewOVR GLES3::Config::get_singleton()->eglFramebufferTextureMultiviewOVR
#endif

using namespace GLES3;

TextureStorage *TextureStorage::singleton = nullptr;

TextureStorage *TextureStorage::get_singleton() {
	return singleton;
}

static const GLenum _cube_side_enum[6] = {
	GL_TEXTURE_CUBE_MAP_NEGATIVE_X,
	GL_TEXTURE_CUBE_MAP_POSITIVE_X,
	GL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
	GL_TEXTURE_CUBE_MAP_POSITIVE_Y,
	GL_TEXTURE_CUBE_MAP_NEGATIVE_Z,
	GL_TEXTURE_CUBE_MAP_POSITIVE_Z,
};

TextureStorage::TextureStorage() {
	singleton = this;

	{ //create default textures
		{ // White Textures

			Ref<Image> image = Image::create_empty(4, 4, true, Image::FORMAT_RGBA8);
			image->fill(Color(1, 1, 1, 1));
			image->generate_mipmaps();

			default_gl_textures[DEFAULT_GL_TEXTURE_WHITE] = texture_allocate();
			texture_2d_initialize(default_gl_textures[DEFAULT_GL_TEXTURE_WHITE], image);

			Vector<Ref<Image>> images;
			images.push_back(image);

			default_gl_textures[DEFAULT_GL_TEXTURE_2D_ARRAY_WHITE] = texture_allocate();
			texture_2d_layered_initialize(default_gl_textures[DEFAULT_GL_TEXTURE_2D_ARRAY_WHITE], images, RS::TEXTURE_LAYERED_2D_ARRAY);

			for (int i = 0; i < 5; i++) {
				images.push_back(image);
			}

			default_gl_textures[DEFAULT_GL_TEXTURE_CUBEMAP_WHITE] = texture_allocate();
			texture_2d_layered_initialize(default_gl_textures[DEFAULT_GL_TEXTURE_CUBEMAP_WHITE], images, RS::TEXTURE_LAYERED_CUBEMAP);
		}

		{
			Ref<Image> image = Image::create_empty(4, 4, false, Image::FORMAT_RGBA8);
			image->fill(Color(1, 1, 1, 1));

			Vector<Ref<Image>> images;
			for (int i = 0; i < 4; i++) {
				images.push_back(image);
			}
			default_gl_textures[DEFAULT_GL_TEXTURE_3D_WHITE] = texture_allocate();
			texture_3d_initialize(default_gl_textures[DEFAULT_GL_TEXTURE_3D_WHITE], image->get_format(), 4, 4, 4, false, images);
		}

		{ // black
			Ref<Image> image = Image::create_empty(4, 4, true, Image::FORMAT_RGBA8);
			image->fill(Color(0, 0, 0, 1));
			image->generate_mipmaps();

			default_gl_textures[DEFAULT_GL_TEXTURE_BLACK] = texture_allocate();
			texture_2d_initialize(default_gl_textures[DEFAULT_GL_TEXTURE_BLACK], image);

			Vector<Ref<Image>> images;
			for (int i = 0; i < 6; i++) {
				images.push_back(image);
			}
			default_gl_textures[DEFAULT_GL_TEXTURE_CUBEMAP_BLACK] = texture_allocate();
			texture_2d_layered_initialize(default_gl_textures[DEFAULT_GL_TEXTURE_CUBEMAP_BLACK], images, RS::TEXTURE_LAYERED_CUBEMAP);
		}

		{
			Ref<Image> image = Image::create_empty(4, 4, false, Image::FORMAT_RGBA8);
			image->fill(Color());

			Vector<Ref<Image>> images;
			for (int i = 0; i < 4; i++) {
				images.push_back(image);
			}
			default_gl_textures[DEFAULT_GL_TEXTURE_3D_BLACK] = texture_allocate();
			texture_3d_initialize(default_gl_textures[DEFAULT_GL_TEXTURE_3D_BLACK], image->get_format(), 4, 4, 4, false, images);
		}

		{ // transparent black
			Ref<Image> image = Image::create_empty(4, 4, true, Image::FORMAT_RGBA8);
			image->fill(Color(0, 0, 0, 0));
			image->generate_mipmaps();

			default_gl_textures[DEFAULT_GL_TEXTURE_TRANSPARENT] = texture_allocate();
			texture_2d_initialize(default_gl_textures[DEFAULT_GL_TEXTURE_TRANSPARENT], image);
		}

		{
			Ref<Image> image = Image::create_empty(4, 4, true, Image::FORMAT_RGBA8);
			image->fill(Color(0.5, 0.5, 1, 1));
			image->generate_mipmaps();

			default_gl_textures[DEFAULT_GL_TEXTURE_NORMAL] = texture_allocate();
			texture_2d_initialize(default_gl_textures[DEFAULT_GL_TEXTURE_NORMAL], image);
		}

		{
			Ref<Image> image = Image::create_empty(4, 4, true, Image::FORMAT_RGBA8);
			image->fill(Color(1.0, 0.5, 1, 1));
			image->generate_mipmaps();

			default_gl_textures[DEFAULT_GL_TEXTURE_ANISO] = texture_allocate();
			texture_2d_initialize(default_gl_textures[DEFAULT_GL_TEXTURE_ANISO], image);
		}

		{
			default_gl_textures[DEFAULT_GL_TEXTURE_EXT] = texture_allocate();
			texture_external_initialize(default_gl_textures[DEFAULT_GL_TEXTURE_EXT], 1, 1, 0);
		}

		{
			unsigned char pixel_data[4 * 4 * 4];
			for (int i = 0; i < 16; i++) {
				pixel_data[i * 4 + 0] = 0;
				pixel_data[i * 4 + 1] = 0;
				pixel_data[i * 4 + 2] = 0;
				pixel_data[i * 4 + 3] = 0;
			}

			default_gl_textures[DEFAULT_GL_TEXTURE_2D_UINT] = texture_allocate();
			Texture texture;
			texture.width = 4;
			texture.height = 4;
			texture.format = Image::FORMAT_RGBA8;
			texture.type = Texture::TYPE_2D;
			texture.target = GL_TEXTURE_2D;
			texture.active = true;
			glGenTextures(1, &texture.tex_id);
			texture_owner.initialize_rid(default_gl_textures[DEFAULT_GL_TEXTURE_2D_UINT], texture);

			glBindTexture(GL_TEXTURE_2D, texture.tex_id);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8UI, 4, 4, 0, GL_RGBA_INTEGER, GL_UNSIGNED_BYTE, pixel_data);
			GLES3::Utilities::get_singleton()->texture_allocated_data(texture.tex_id, 4 * 4 * 4, "Default uint texture");
			texture.gl_set_filter(RS::CANVAS_ITEM_TEXTURE_FILTER_NEAREST);
		}
		{
			uint16_t pixel_data[4 * 4];
			for (int i = 0; i < 16; i++) {
				pixel_data[i] = Math::make_half_float(1.0f);
			}

			default_gl_textures[DEFAULT_GL_TEXTURE_DEPTH] = texture_allocate();
			Texture texture;
			texture.width = 4;
			texture.height = 4;
			texture.format = Image::FORMAT_RGBA8;
			texture.type = Texture::TYPE_2D;
			texture.target = GL_TEXTURE_2D;
			texture.active = true;
			glGenTextures(1, &texture.tex_id);
			texture_owner.initialize_rid(default_gl_textures[DEFAULT_GL_TEXTURE_DEPTH], texture);

			glBindTexture(GL_TEXTURE_2D, texture.tex_id);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT16, 4, 4, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT, pixel_data);
			GLES3::Utilities::get_singleton()->texture_allocated_data(texture.tex_id, 4 * 4 * 2, "Default depth texture");
			texture.gl_set_filter(RS::CANVAS_ITEM_TEXTURE_FILTER_NEAREST);
		}
	}

	glBindTexture(GL_TEXTURE_2D, 0);

	{ // Atlas Texture initialize.
		uint8_t pixel_data[4 * 4 * 4];
		for (int i = 0; i < 16; i++) {
			pixel_data[i * 4 + 0] = 0;
			pixel_data[i * 4 + 1] = 0;
			pixel_data[i * 4 + 2] = 0;
			pixel_data[i * 4 + 3] = 255;
		}

		glGenTextures(1, &texture_atlas.texture);
		glBindTexture(GL_TEXTURE_2D, texture_atlas.texture);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 4, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixel_data);
		GLES3::Utilities::get_singleton()->texture_allocated_data(texture_atlas.texture, 4 * 4 * 4, "Texture atlas (Default)");
	}

	glBindTexture(GL_TEXTURE_2D, 0);

	{
		sdf_shader.shader.initialize();
		sdf_shader.shader_version = sdf_shader.shader.version_create();
	}

	// Initialize texture placeholder data for the `texture_*_placeholder_initialize()` methods.

	constexpr int placeholder_size = 4;
	texture_2d_placeholder = Image::create_empty(placeholder_size, placeholder_size, false, Image::FORMAT_RGBA8);
	// Draw a magenta/black checkerboard pattern.
	for (int i = 0; i < placeholder_size * placeholder_size; i++) {
		const int x = i % placeholder_size;
		const int y = i / placeholder_size;
		texture_2d_placeholder->set_pixel(x, y, (x + y) % 2 == 0 ? Color(1, 0, 1) : Color(0, 0, 0));
	}

	texture_2d_array_placeholder.push_back(texture_2d_placeholder);

	for (int i = 0; i < 6; i++) {
		cubemap_placeholder.push_back(texture_2d_placeholder);
	}

	Ref<Image> texture_2d_placeholder_rotated;
	texture_2d_placeholder_rotated.instantiate();
	texture_2d_placeholder_rotated->copy_from(texture_2d_placeholder);
	texture_2d_placeholder_rotated->rotate_90(CLOCKWISE);
	for (int i = 0; i < 4; i++) {
		// Alternate checkerboard pattern on odd layers (by using a copy that is rotated 90 degrees).
		texture_3d_placeholder.push_back(i % 2 == 0 ? texture_2d_placeholder : texture_2d_placeholder_rotated);
	}

#ifdef GL_API_ENABLED
	if (RasterizerGLES3::is_gles_over_gl()) {
		glEnable(GL_PROGRAM_POINT_SIZE);
	}
#endif // GL_API_ENABLED
}

TextureStorage::~TextureStorage() {
	singleton = nullptr;
	for (int i = 0; i < DEFAULT_GL_TEXTURE_MAX; i++) {
		texture_free(default_gl_textures[i]);
	}
	if (texture_atlas.texture != 0) {
		GLES3::Utilities::get_singleton()->texture_free_data(texture_atlas.texture);
	}
	texture_atlas.texture = 0;
	glDeleteFramebuffers(1, &texture_atlas.framebuffer);
	texture_atlas.framebuffer = 0;
	sdf_shader.shader.version_free(sdf_shader.shader_version);
}

/* Canvas Texture API */

RID TextureStorage::canvas_texture_allocate() {
	return canvas_texture_owner.allocate_rid();
}

void TextureStorage::canvas_texture_initialize(RID p_rid) {
	canvas_texture_owner.initialize_rid(p_rid);
}

void TextureStorage::canvas_texture_free(RID p_rid) {
	canvas_texture_owner.free(p_rid);
}

void TextureStorage::canvas_texture_set_channel(RID p_canvas_texture, RS::CanvasTextureChannel p_channel, RID p_texture) {
	CanvasTexture *ct = canvas_texture_owner.get_or_null(p_canvas_texture);
	ERR_FAIL_NULL(ct);

	switch (p_channel) {
		case RS::CANVAS_TEXTURE_CHANNEL_DIFFUSE: {
			ct->diffuse = p_texture;
		} break;
		case RS::CANVAS_TEXTURE_CHANNEL_NORMAL: {
			ct->normal_map = p_texture;
		} break;
		case RS::CANVAS_TEXTURE_CHANNEL_SPECULAR: {
			ct->specular = p_texture;
		} break;
	}
}

void TextureStorage::canvas_texture_set_shading_parameters(RID p_canvas_texture, const Color &p_specular_color, float p_shininess) {
	CanvasTexture *ct = canvas_texture_owner.get_or_null(p_canvas_texture);
	ERR_FAIL_NULL(ct);

	ct->specular_color.r = p_specular_color.r;
	ct->specular_color.g = p_specular_color.g;
	ct->specular_color.b = p_specular_color.b;
	ct->specular_color.a = p_shininess;
}

void TextureStorage::canvas_texture_set_texture_filter(RID p_canvas_texture, RS::CanvasItemTextureFilter p_filter) {
	CanvasTexture *ct = canvas_texture_owner.get_or_null(p_canvas_texture);
	ERR_FAIL_NULL(ct);

	ct->texture_filter = p_filter;
}

void TextureStorage::canvas_texture_set_texture_repeat(RID p_canvas_texture, RS::CanvasItemTextureRepeat p_repeat) {
	CanvasTexture *ct = canvas_texture_owner.get_or_null(p_canvas_texture);
	ERR_FAIL_NULL(ct);

	ct->texture_repeat = p_repeat;
}

/* Texture API */

static inline Error _get_gl_uncompressed_format(Image::Format p_format, GLenum &r_gl_format, GLenum &r_gl_internal_format, GLenum &r_gl_type) {
	switch (p_format) {
		case Image::FORMAT_L8: {
			if (RasterizerGLES3::is_gles_over_gl()) {
				r_gl_internal_format = GL_R8;
				r_gl_format = GL_RED;
				r_gl_type = GL_UNSIGNED_BYTE;
			} else {
				r_gl_internal_format = GL_LUMINANCE;
				r_gl_format = GL_LUMINANCE;
				r_gl_type = GL_UNSIGNED_BYTE;
			}
		} break;
		case Image::FORMAT_LA8: {
			if (RasterizerGLES3::is_gles_over_gl()) {
				r_gl_internal_format = GL_RG8;
				r_gl_format = GL_RG;
				r_gl_type = GL_UNSIGNED_BYTE;
			} else {
				r_gl_internal_format = GL_LUMINANCE_ALPHA;
				r_gl_format = GL_LUMINANCE_ALPHA;
				r_gl_type = GL_UNSIGNED_BYTE;
			}
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
			r_gl_internal_format = GL_RGB8;
			r_gl_format = GL_RGB;
			r_gl_type = GL_UNSIGNED_BYTE;
		} break;
		case Image::FORMAT_RGBA8: {
			r_gl_internal_format = GL_RGBA8;
			r_gl_format = GL_RGBA;
			r_gl_type = GL_UNSIGNED_BYTE;
		} break;
		case Image::FORMAT_RGBA4444: {
			r_gl_internal_format = GL_RGBA4;
			r_gl_format = GL_RGBA;
			r_gl_type = GL_UNSIGNED_SHORT_4_4_4_4;
		} break;
		case Image::FORMAT_RGB565: {
			r_gl_internal_format = GL_RGB565;
			r_gl_format = GL_RGB;
			r_gl_type = GL_UNSIGNED_SHORT_5_6_5;
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
		default: {
			return ERR_UNAVAILABLE;
		}
	}

	return OK;
}

Ref<Image> TextureStorage::_get_gl_image_and_format(const Ref<Image> &p_image, Image::Format p_format, Image::Format &r_real_format, GLenum &r_gl_format, GLenum &r_gl_internal_format, GLenum &r_gl_type, bool &r_compressed, bool p_force_decompress) const {
	Config *config = Config::get_singleton();
	r_gl_format = 0;
	Ref<Image> image = p_image;
	r_compressed = false;
	r_real_format = p_format;

	if (!Image::is_format_compressed(p_format)) {
		Error err = _get_gl_uncompressed_format(p_format, r_gl_format, r_gl_internal_format, r_gl_type);
		ERR_FAIL_COND_V_MSG(err != OK, Ref<Image>(), vformat("The image format %d is not supported by the Compatibility renderer.", p_format));
		return p_image;
	}

	// For compressed images, some formats may not be supported by the current device and will require decompression.
	bool need_decompress = false;
	bool decompress_ra_to_rg = false;

	switch (p_format) {
		case Image::FORMAT_DXT1: {
			if (config->s3tc_supported) {
				r_gl_internal_format = _EXT_COMPRESSED_RGBA_S3TC_DXT1_EXT;
				r_gl_format = GL_RGBA;
				r_gl_type = GL_UNSIGNED_BYTE;
				r_compressed = true;
			} else {
				need_decompress = true;
			}
		} break;
		case Image::FORMAT_DXT3: {
			if (config->s3tc_supported) {
				r_gl_internal_format = _EXT_COMPRESSED_RGBA_S3TC_DXT3_EXT;
				r_gl_format = GL_RGBA;
				r_gl_type = GL_UNSIGNED_BYTE;
				r_compressed = true;
			} else {
				need_decompress = true;
			}
		} break;
		case Image::FORMAT_DXT5: {
			if (config->s3tc_supported) {
				r_gl_internal_format = _EXT_COMPRESSED_RGBA_S3TC_DXT5_EXT;
				r_gl_format = GL_RGBA;
				r_gl_type = GL_UNSIGNED_BYTE;
				r_compressed = true;
			} else {
				need_decompress = true;
			}
		} break;
		case Image::FORMAT_RGTC_R: {
			if (config->rgtc_supported) {
				r_gl_internal_format = _EXT_COMPRESSED_RED_RGTC1_EXT;
				r_gl_format = GL_RGBA;
				r_gl_type = GL_UNSIGNED_BYTE;
				r_compressed = true;
			} else {
				need_decompress = true;
			}
		} break;
		case Image::FORMAT_RGTC_RG: {
			if (config->rgtc_supported) {
				r_gl_internal_format = _EXT_COMPRESSED_RED_GREEN_RGTC2_EXT;
				r_gl_format = GL_RGBA;
				r_gl_type = GL_UNSIGNED_BYTE;
				r_compressed = true;
			} else {
				need_decompress = true;
			}
		} break;
		case Image::FORMAT_BPTC_RGBA: {
			if (config->bptc_supported) {
				r_gl_internal_format = _EXT_COMPRESSED_RGBA_BPTC_UNORM;
				r_gl_format = GL_RGBA;
				r_gl_type = GL_UNSIGNED_BYTE;
				r_compressed = true;
			} else {
				need_decompress = true;
			}
		} break;
		case Image::FORMAT_BPTC_RGBF: {
			if (config->bptc_supported) {
				r_gl_internal_format = _EXT_COMPRESSED_RGB_BPTC_SIGNED_FLOAT;
				r_gl_format = GL_RGB;
				r_gl_type = GL_FLOAT;
				r_compressed = true;
			} else {
				need_decompress = true;
			}
		} break;
		case Image::FORMAT_BPTC_RGBFU: {
			if (config->bptc_supported) {
				r_gl_internal_format = _EXT_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT;
				r_gl_format = GL_RGB;
				r_gl_type = GL_FLOAT;
				r_compressed = true;
			} else {
				need_decompress = true;
			}
		} break;
		case Image::FORMAT_ETC2_R11: {
			if (config->etc2_supported) {
				r_gl_internal_format = _EXT_COMPRESSED_R11_EAC;
				r_gl_format = GL_RED;
				r_gl_type = GL_UNSIGNED_BYTE;
				r_compressed = true;
			} else {
				need_decompress = true;
			}
		} break;
		case Image::FORMAT_ETC2_R11S: {
			if (config->etc2_supported) {
				r_gl_internal_format = _EXT_COMPRESSED_SIGNED_R11_EAC;
				r_gl_format = GL_RED;
				r_gl_type = GL_UNSIGNED_BYTE;
				r_compressed = true;
			} else {
				need_decompress = true;
			}
		} break;
		case Image::FORMAT_ETC2_RG11: {
			if (config->etc2_supported) {
				r_gl_internal_format = _EXT_COMPRESSED_RG11_EAC;
				r_gl_format = GL_RG;
				r_gl_type = GL_UNSIGNED_BYTE;
				r_compressed = true;
			} else {
				need_decompress = true;
			}
		} break;
		case Image::FORMAT_ETC2_RG11S: {
			if (config->etc2_supported) {
				r_gl_internal_format = _EXT_COMPRESSED_SIGNED_RG11_EAC;
				r_gl_format = GL_RG;
				r_gl_type = GL_UNSIGNED_BYTE;
				r_compressed = true;
			} else {
				need_decompress = true;
			}
		} break;
		case Image::FORMAT_ETC:
		case Image::FORMAT_ETC2_RGB8: {
			if (config->etc2_supported) {
				r_gl_internal_format = _EXT_COMPRESSED_RGB8_ETC2;
				r_gl_format = GL_RGB;
				r_gl_type = GL_UNSIGNED_BYTE;
				r_compressed = true;
			} else {
				need_decompress = true;
			}
		} break;
		case Image::FORMAT_ETC2_RGBA8: {
			if (config->etc2_supported) {
				r_gl_internal_format = _EXT_COMPRESSED_RGBA8_ETC2_EAC;
				r_gl_format = GL_RGBA;
				r_gl_type = GL_UNSIGNED_BYTE;
				r_compressed = true;
			} else {
				need_decompress = true;
			}
		} break;
		case Image::FORMAT_ETC2_RGB8A1: {
			if (config->etc2_supported) {
				r_gl_internal_format = _EXT_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2;
				r_gl_format = GL_RGBA;
				r_gl_type = GL_UNSIGNED_BYTE;
				r_compressed = true;
			} else {
				need_decompress = true;
			}
		} break;
		case Image::FORMAT_ETC2_RA_AS_RG: {
#ifndef WEB_ENABLED
			if (config->etc2_supported) {
				r_gl_internal_format = _EXT_COMPRESSED_RGBA8_ETC2_EAC;
				r_gl_format = GL_RGBA;
				r_gl_type = GL_UNSIGNED_BYTE;
				r_compressed = true;
			} else
#endif
			{
				need_decompress = true;
			}
			decompress_ra_to_rg = true;
		} break;
		case Image::FORMAT_DXT5_RA_AS_RG: {
#ifndef WEB_ENABLED
			if (config->s3tc_supported) {
				r_gl_internal_format = _EXT_COMPRESSED_RGBA_S3TC_DXT5_EXT;
				r_gl_format = GL_RGBA;
				r_gl_type = GL_UNSIGNED_BYTE;
				r_compressed = true;
			} else
#endif
			{
				need_decompress = true;
			}
			decompress_ra_to_rg = true;
		} break;
		case Image::FORMAT_ASTC_4x4: {
			if (config->astc_supported) {
				r_gl_internal_format = _EXT_COMPRESSED_RGBA_ASTC_4x4_KHR;
				r_gl_format = GL_RGBA;
				r_gl_type = GL_UNSIGNED_BYTE;
				r_compressed = true;
			} else {
				need_decompress = true;
			}
		} break;
		case Image::FORMAT_ASTC_4x4_HDR: {
			if (config->astc_hdr_supported) {
				r_gl_internal_format = _EXT_COMPRESSED_RGBA_ASTC_4x4_KHR;
				r_gl_format = GL_RGBA;
				r_gl_type = GL_UNSIGNED_BYTE;
				r_compressed = true;
			} else {
				need_decompress = true;
			}
		} break;
		case Image::FORMAT_ASTC_8x8: {
			if (config->astc_supported) {
				r_gl_internal_format = _EXT_COMPRESSED_RGBA_ASTC_8x8_KHR;
				r_gl_format = GL_RGBA;
				r_gl_type = GL_UNSIGNED_BYTE;
				r_compressed = true;
			} else {
				need_decompress = true;
			}
		} break;
		case Image::FORMAT_ASTC_8x8_HDR: {
			if (config->astc_hdr_supported) {
				r_gl_internal_format = _EXT_COMPRESSED_RGBA_ASTC_8x8_KHR;
				r_gl_format = GL_RGBA;
				r_gl_type = GL_UNSIGNED_BYTE;
				r_compressed = true;
			} else {
				need_decompress = true;
			}
		} break;
		default: {
			ERR_FAIL_V_MSG(Ref<Image>(), vformat("The image format %d is not supported by the Compatibility renderer.", p_format));
		}
	}

	if (need_decompress || p_force_decompress) {
		if (image.is_valid()) {
			image = image->duplicate();
			image->decompress();
			ERR_FAIL_COND_V(image->is_compressed(), image);

			if (decompress_ra_to_rg) {
				image->convert_ra_rgba8_to_rg();
				image->convert(Image::FORMAT_RG8);
			}

			Error err = _get_gl_uncompressed_format(image->get_format(), r_gl_format, r_gl_internal_format, r_gl_type);
			ERR_FAIL_COND_V_MSG(err != OK, Ref<Image>(), vformat("The image format %d is not supported by the Compatibility renderer.", image->get_format()));

			r_real_format = image->get_format();
			r_compressed = false;
		}

		return image;
	}

	return p_image;
}

RID TextureStorage::texture_allocate() {
	return texture_owner.allocate_rid();
}

void TextureStorage::texture_free(RID p_texture) {
	Texture *t = texture_owner.get_or_null(p_texture);
	ERR_FAIL_NULL(t);
	ERR_FAIL_COND(t->is_render_target);

	if (t->canvas_texture) {
		memdelete(t->canvas_texture);
	}

	bool must_free_data = false;
	if (t->is_proxy) {
		if (t->proxy_to.is_valid()) {
			Texture *proxy_to = texture_owner.get_or_null(t->proxy_to);
			if (proxy_to) {
				proxy_to->proxies.erase(p_texture);
			}
		}
	} else {
		must_free_data = t->tex_id != 0 && !t->is_from_native_handle;
	}
	if (must_free_data) {
		GLES3::Utilities::get_singleton()->texture_free_data(t->tex_id);
		t->tex_id = 0;
	}

	texture_atlas_remove_texture(p_texture);

	for (int i = 0; i < t->proxies.size(); i++) {
		Texture *p = texture_owner.get_or_null(t->proxies[i]);
		ERR_CONTINUE(!p);
		p->proxy_to = RID();
		p->tex_id = 0;
	}

	texture_owner.free(p_texture);
}

void TextureStorage::texture_2d_initialize(RID p_texture, const Ref<Image> &p_image) {
	ERR_FAIL_COND(p_image.is_null());

	Texture texture;
	texture.width = p_image->get_width();
	texture.height = p_image->get_height();
	texture.alloc_width = texture.width;
	texture.alloc_height = texture.height;
	texture.mipmaps = p_image->get_mipmap_count() + 1;
	texture.format = p_image->get_format();
	texture.type = Texture::TYPE_2D;
	texture.target = GL_TEXTURE_2D;
	_get_gl_image_and_format(Ref<Image>(), texture.format, texture.real_format, texture.gl_format_cache, texture.gl_internal_format_cache, texture.gl_type_cache, texture.compressed, false);
	texture.total_data_size = p_image->get_image_data_size(texture.width, texture.height, texture.format, texture.mipmaps);
	texture.active = true;
	glGenTextures(1, &texture.tex_id);
	GLES3::Utilities::get_singleton()->texture_allocated_data(texture.tex_id, texture.total_data_size, "Texture 2D");
	texture_owner.initialize_rid(p_texture, texture);
	texture_set_data(p_texture, p_image);
}

void TextureStorage::texture_external_initialize(RID p_texture, int p_width, int p_height, uint64_t p_external_buffer) {
	Texture texture;
	texture.active = true;
	texture.alloc_width = texture.width = p_width;
	texture.alloc_height = texture.height = p_height;
	texture.real_format = texture.format = Image::FORMAT_RGB8;
	texture.type = Texture::TYPE_2D;

	if (GLES3::Config::get_singleton()->external_texture_supported) {
		texture.target = _GL_TEXTURE_EXTERNAL_OES;
	} else {
		texture.target = GL_TEXTURE_2D;
	}

	glGenTextures(1, &texture.tex_id);
	glBindTexture(texture.target, texture.tex_id);

#ifdef ANDROID_ENABLED
	if (texture.target == _GL_TEXTURE_EXTERNAL_OES) {
		if (p_external_buffer) {
			GLES3::Config::get_singleton()->eglEGLImageTargetTexture2DOES(_GL_TEXTURE_EXTERNAL_OES, reinterpret_cast<void *>(p_external_buffer));
		}
		texture.total_data_size = 0;
	} else
#endif
	{
		// If external textures aren't supported, allocate an empty 1x1 texture.
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
		texture.total_data_size = 3;
	}

	glTexParameteri(texture.target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(texture.target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(texture.target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(texture.target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	GLES3::Utilities::get_singleton()->texture_allocated_data(texture.tex_id, texture.total_data_size, "Texture External");
	texture_owner.initialize_rid(p_texture, texture);

	glBindTexture(texture.target, 0);
}

void TextureStorage::texture_2d_layered_initialize(RID p_texture, const Vector<Ref<Image>> &p_layers, RS::TextureLayeredType p_layered_type) {
	ERR_FAIL_COND(p_layers.is_empty());

	ERR_FAIL_COND(p_layered_type == RS::TEXTURE_LAYERED_CUBEMAP && p_layers.size() != 6);
	ERR_FAIL_COND_MSG(p_layered_type == RS::TEXTURE_LAYERED_CUBEMAP_ARRAY, "Cubemap Arrays are not supported in the Compatibility renderer.");

	const Ref<Image> &image = p_layers[0];
	{
		int valid_width = 0;
		int valid_height = 0;
		bool valid_mipmaps = false;
		Image::Format valid_format = Image::FORMAT_MAX;

		for (int i = 0; i < p_layers.size(); i++) {
			ERR_FAIL_COND(p_layers[i]->is_empty());

			if (i == 0) {
				valid_width = p_layers[i]->get_width();
				valid_height = p_layers[i]->get_height();
				valid_format = p_layers[i]->get_format();
				valid_mipmaps = p_layers[i]->has_mipmaps();
			} else {
				ERR_FAIL_COND(p_layers[i]->get_width() != valid_width);
				ERR_FAIL_COND(p_layers[i]->get_height() != valid_height);
				ERR_FAIL_COND(p_layers[i]->get_format() != valid_format);
				ERR_FAIL_COND(p_layers[i]->has_mipmaps() != valid_mipmaps);
			}
		}
	}

	Texture texture;
	texture.width = image->get_width();
	texture.height = image->get_height();
	texture.alloc_width = texture.width;
	texture.alloc_height = texture.height;
	texture.mipmaps = image->get_mipmap_count() + 1;
	texture.format = image->get_format();
	texture.type = Texture::TYPE_LAYERED;
	texture.layered_type = p_layered_type;
	texture.target = p_layered_type == RS::TEXTURE_LAYERED_CUBEMAP ? GL_TEXTURE_CUBE_MAP : GL_TEXTURE_2D_ARRAY;
	texture.layers = p_layers.size();
	_get_gl_image_and_format(Ref<Image>(), texture.format, texture.real_format, texture.gl_format_cache, texture.gl_internal_format_cache, texture.gl_type_cache, texture.compressed, false);
	texture.total_data_size = p_layers[0]->get_image_data_size(texture.width, texture.height, texture.format, texture.mipmaps) * texture.layers;
	texture.active = true;
	glGenTextures(1, &texture.tex_id);
	GLES3::Utilities::get_singleton()->texture_allocated_data(texture.tex_id, texture.total_data_size, "Texture Layered");
	texture_owner.initialize_rid(p_texture, texture);
	for (int i = 0; i < p_layers.size(); i++) {
		_texture_set_data(p_texture, p_layers[i], i, i == 0);
	}
}

void TextureStorage::texture_3d_initialize(RID p_texture, Image::Format p_format, int p_width, int p_height, int p_depth, bool p_mipmaps, const Vector<Ref<Image>> &p_data) {
	ERR_FAIL_COND(p_data.is_empty());

	Image::Image3DValidateError verr = Image::validate_3d_image(p_format, p_width, p_height, p_depth, p_mipmaps, p_data);
	ERR_FAIL_COND_MSG(verr != Image::VALIDATE_3D_OK, Image::get_3d_image_validation_error_text(verr));

	Ref<Image> image = p_data[0];
	int mipmap_count = 0;
	{
		Size2i prev_size;
		for (int i = 0; i < p_data.size(); i++) {
			Size2i img_size(p_data[i]->get_width(), p_data[i]->get_height());
			if (img_size != prev_size) {
				mipmap_count++;
			}
			prev_size = img_size;
		}
	}

	Texture texture;
	texture.width = p_width;
	texture.height = p_height;
	texture.depth = p_depth;
	texture.alloc_width = texture.width;
	texture.alloc_height = texture.height;
	texture.mipmaps = mipmap_count;
	texture.format = image->get_format();
	texture.type = Texture::TYPE_3D;
	texture.target = GL_TEXTURE_3D;
	_get_gl_image_and_format(Ref<Image>(), texture.format, texture.real_format, texture.gl_format_cache, texture.gl_internal_format_cache, texture.gl_type_cache, texture.compressed, false);
	texture.total_data_size = p_data[0]->get_image_data_size(texture.width, texture.height, texture.format, texture.mipmaps) * texture.depth;
	texture.active = true;
	glGenTextures(1, &texture.tex_id);
	GLES3::Utilities::get_singleton()->texture_allocated_data(texture.tex_id, texture.total_data_size, "Texture 3D");
	texture_owner.initialize_rid(p_texture, texture);
	_texture_set_3d_data(p_texture, p_data, true);
}

// Called internally when texture_proxy_create(p_base) is called.
// Note: p_base is the root and p_texture is the proxy.
void TextureStorage::texture_proxy_initialize(RID p_texture, RID p_base) {
	Texture *texture = texture_owner.get_or_null(p_base);
	ERR_FAIL_NULL(texture);
	Texture proxy_tex;
	proxy_tex.copy_from(*texture);
	proxy_tex.proxy_to = p_base;
	proxy_tex.is_render_target = false;
	proxy_tex.is_proxy = true;
	proxy_tex.proxies.clear();
	texture->proxies.push_back(p_texture);
	texture_owner.initialize_rid(p_texture, proxy_tex);
}

RID TextureStorage::texture_create_from_native_handle(RS::TextureType p_type, Image::Format p_format, uint64_t p_native_handle, int p_width, int p_height, int p_depth, int p_layers, RS::TextureLayeredType p_layered_type) {
	Texture texture;
	texture.active = true;
	texture.is_from_native_handle = true;

	switch (p_type) {
		case RS::TEXTURE_TYPE_2D: {
			texture.type = Texture::TYPE_2D;
			texture.target = GL_TEXTURE_2D;
		} break;
		case RS::TEXTURE_TYPE_3D: {
			texture.type = Texture::TYPE_3D;
			texture.target = GL_TEXTURE_3D;
		} break;
		case RS::TEXTURE_TYPE_LAYERED: {
			texture.type = Texture::TYPE_LAYERED;
			texture.target = GL_TEXTURE_2D_ARRAY;
		} break;
	}

	texture.real_format = texture.format = p_format;
	texture.tex_id = p_native_handle;
	texture.alloc_width = texture.width = p_width;
	texture.alloc_height = texture.height = p_height;
	texture.depth = p_depth;
	texture.layers = p_layers;
	texture.layered_type = p_layered_type;

	return texture_owner.make_rid(texture);
}

void TextureStorage::texture_2d_update(RID p_texture, const Ref<Image> &p_image, int p_layer) {
	texture_set_data(p_texture, p_image, p_layer);

	Texture *tex = texture_owner.get_or_null(p_texture);
	ERR_FAIL_NULL(tex);
	GLES3::Utilities::get_singleton()->texture_resize_data(tex->tex_id, tex->total_data_size);

#ifdef TOOLS_ENABLED
	tex->image_cache_2d.unref();
#endif
}

void TextureStorage::texture_3d_update(RID p_texture, const Vector<Ref<Image>> &p_data) {
	Texture *tex = texture_owner.get_or_null(p_texture);
	ERR_FAIL_NULL(tex);
	ERR_FAIL_COND(tex->type != Texture::TYPE_3D);

	Image::Image3DValidateError verr = Image::validate_3d_image(tex->format, tex->width, tex->height, tex->depth, tex->mipmaps > 1, p_data);
	ERR_FAIL_COND_MSG(verr != Image::VALIDATE_3D_OK, Image::get_3d_image_validation_error_text(verr));

	_texture_set_3d_data(p_texture, p_data, false);

	GLES3::Utilities::get_singleton()->texture_resize_data(tex->tex_id, tex->total_data_size);
}

void TextureStorage::texture_external_update(RID p_texture, int p_width, int p_height, uint64_t p_external_buffer) {
	Texture *tex = texture_owner.get_or_null(p_texture);
	ERR_FAIL_NULL(tex);

	tex->alloc_width = tex->width = p_width;
	tex->alloc_height = tex->height = p_height;

#ifdef ANDROID_ENABLED
	if (tex->target == _GL_TEXTURE_EXTERNAL_OES && p_external_buffer) {
		glBindTexture(_GL_TEXTURE_EXTERNAL_OES, tex->tex_id);
		GLES3::Config::get_singleton()->eglEGLImageTargetTexture2DOES(_GL_TEXTURE_EXTERNAL_OES, reinterpret_cast<void *>(p_external_buffer));
		glBindTexture(_GL_TEXTURE_EXTERNAL_OES, 0);
	}
#endif
}

void TextureStorage::texture_proxy_update(RID p_texture, RID p_proxy_to) {
	Texture *tex = texture_owner.get_or_null(p_texture);
	ERR_FAIL_NULL(tex);
	ERR_FAIL_COND(!tex->is_proxy);
	Texture *proxy_to = texture_owner.get_or_null(p_proxy_to);
	ERR_FAIL_NULL(proxy_to);
	ERR_FAIL_COND(proxy_to->is_proxy);

	if (tex->proxy_to.is_valid()) {
		Texture *prev_tex = texture_owner.get_or_null(tex->proxy_to);
		ERR_FAIL_NULL(prev_tex);
		prev_tex->proxies.erase(p_texture);
	}

	*tex = *proxy_to;

	tex->proxy_to = p_proxy_to;
	tex->is_render_target = false;
	tex->is_proxy = true;
	tex->proxies.clear();
	tex->canvas_texture = nullptr;
	tex->tex_id = 0;
	proxy_to->proxies.push_back(p_texture);
}

void TextureStorage::texture_2d_placeholder_initialize(RID p_texture) {
	texture_2d_initialize(p_texture, texture_2d_placeholder);
}

void TextureStorage::texture_2d_layered_placeholder_initialize(RID p_texture, RS::TextureLayeredType p_layered_type) {
	if (p_layered_type == RS::TEXTURE_LAYERED_2D_ARRAY) {
		texture_2d_layered_initialize(p_texture, texture_2d_array_placeholder, p_layered_type);
	} else {
		texture_2d_layered_initialize(p_texture, cubemap_placeholder, p_layered_type);
	}
}

void TextureStorage::texture_3d_placeholder_initialize(RID p_texture) {
	texture_3d_initialize(p_texture, Image::FORMAT_RGBA8, 4, 4, 4, false, texture_3d_placeholder);
}

Ref<Image> TextureStorage::texture_2d_get(RID p_texture) const {
	Texture *texture = texture_owner.get_or_null(p_texture);
	ERR_FAIL_NULL_V(texture, Ref<Image>());

#ifdef TOOLS_ENABLED
	if (texture->image_cache_2d.is_valid() && !texture->is_render_target) {
		return texture->image_cache_2d;
	}
#endif

	Ref<Image> image;
#ifdef GL_API_ENABLED
	if (RasterizerGLES3::is_gles_over_gl()) {
		// OpenGL 3.3 supports glGetTexImage which is faster and simpler than glReadPixels.
		// It also allows for reading compressed textures, mipmaps, and more formats.
		Vector<uint8_t> data;

		int64_t data_size = Image::get_image_data_size(texture->alloc_width, texture->alloc_height, texture->real_format, texture->mipmaps > 1);

		data.resize(data_size * 2); // Add some memory at the end, just in case for buggy drivers.
		uint8_t *w = data.ptrw();

		glActiveTexture(GL_TEXTURE0);

		glBindTexture(texture->target, texture->tex_id);

		glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);

		for (int i = 0; i < texture->mipmaps; i++) {
			int64_t ofs = Image::get_image_mipmap_offset(texture->alloc_width, texture->alloc_height, texture->real_format, i);

			if (texture->compressed) {
				glPixelStorei(GL_PACK_ALIGNMENT, 4);
				glGetCompressedTexImage(texture->target, i, &w[ofs]);
			} else {
				glPixelStorei(GL_PACK_ALIGNMENT, 1);
				glGetTexImage(texture->target, i, texture->gl_format_cache, texture->gl_type_cache, &w[ofs]);
			}
		}

		data.resize(data_size);

		ERR_FAIL_COND_V(data.is_empty(), Ref<Image>());
		image = Image::create_from_data(texture->alloc_width, texture->alloc_height, texture->mipmaps > 1, texture->real_format, data);
		if (image->is_empty()) {
			const String &path_str = texture->path.is_empty() ? "with no path" : vformat("with path '%s'", texture->path);
			ERR_FAIL_V_MSG(Ref<Image>(), vformat("Texture %s has no data.", path_str));
		}

		if (texture->format != texture->real_format && !Image::is_format_compressed(texture->real_format)) {
			image->convert(texture->format);
		}
	}
#endif // GL_API_ENABLED
#ifdef GLES_API_ENABLED
	if (!RasterizerGLES3::is_gles_over_gl()) {
		Vector<uint8_t> data;

		// On web and mobile we always read an RGBA8 image with no mipmaps.
		int64_t data_size = Image::get_image_data_size(texture->alloc_width, texture->alloc_height, Image::FORMAT_RGBA8, false);

		data.resize(data_size * 2); // Add some memory at the end, just in case for buggy drivers.
		uint8_t *w = data.ptrw();

		GLuint temp_framebuffer;
		glGenFramebuffers(1, &temp_framebuffer);

		GLuint temp_color_texture;
		glGenTextures(1, &temp_color_texture);

		glBindFramebuffer(GL_FRAMEBUFFER, temp_framebuffer);

		glBindTexture(GL_TEXTURE_2D, temp_color_texture);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, texture->alloc_width, texture->alloc_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, temp_color_texture, 0);

		glDepthMask(GL_FALSE);
		glDisable(GL_DEPTH_TEST);
		glDisable(GL_CULL_FACE);
		glDisable(GL_BLEND);
		glDepthFunc(GL_GEQUAL);
		glColorMask(1, 1, 1, 1);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, texture->tex_id);

		glViewport(0, 0, texture->alloc_width, texture->alloc_height);
		glClearColor(0.0, 0.0, 0.0, 0.0);
		glClear(GL_COLOR_BUFFER_BIT);

		CopyEffects::get_singleton()->copy_to_rect(Rect2i(0, 0, 1.0, 1.0));

		glReadPixels(0, 0, texture->alloc_width, texture->alloc_height, GL_RGBA, GL_UNSIGNED_BYTE, &w[0]);

		glBindFramebuffer(GL_FRAMEBUFFER, GLES3::TextureStorage::system_fbo);
		glDeleteTextures(1, &temp_color_texture);
		glDeleteFramebuffers(1, &temp_framebuffer);

		data.resize(data_size);

		ERR_FAIL_COND_V(data.is_empty(), Ref<Image>());
		image = Image::create_from_data(texture->alloc_width, texture->alloc_height, false, Image::FORMAT_RGBA8, data);
		if (image->is_empty()) {
			const String &path_str = texture->path.is_empty() ? "with no path" : vformat("with path '%s'", texture->path);
			ERR_FAIL_V_MSG(Ref<Image>(), vformat("Texture %s has no data.", path_str));
		}

		if (texture->format != Image::FORMAT_RGBA8 && !Image::is_format_compressed(texture->format)) {
			image->convert(texture->format);
		}

		if (texture->mipmaps > 1) {
			image->generate_mipmaps();
		}
	}
#endif // GLES_API_ENABLED

#ifdef TOOLS_ENABLED
	if (Engine::get_singleton()->is_editor_hint() && !texture->is_render_target) {
		texture->image_cache_2d = image;
	}
#endif

	return image;
}

Ref<Image> TextureStorage::texture_2d_layer_get(RID p_texture, int p_layer) const {
	Texture *texture = texture_owner.get_or_null(p_texture);
	ERR_FAIL_NULL_V(texture, Ref<Image>());

	Vector<uint8_t> data;

	int64_t data_size = Image::get_image_data_size(texture->alloc_width, texture->alloc_height, Image::FORMAT_RGBA8, false);

	data.resize(data_size * 2); //add some memory at the end, just in case for buggy drivers
	uint8_t *w = data.ptrw();

	GLuint temp_framebuffer;
	glGenFramebuffers(1, &temp_framebuffer);

	GLuint temp_color_texture;
	glGenTextures(1, &temp_color_texture);

	glBindFramebuffer(GL_FRAMEBUFFER, temp_framebuffer);

	glBindTexture(GL_TEXTURE_2D, temp_color_texture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, texture->alloc_width, texture->alloc_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, temp_color_texture, 0);

	glDepthMask(GL_FALSE);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	glDisable(GL_BLEND);
	glDepthFunc(GL_LEQUAL);
	glColorMask(1, 1, 1, 1);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D_ARRAY, texture->tex_id);

	glViewport(0, 0, texture->alloc_width, texture->alloc_height);
	glClearColor(0.0, 0.0, 0.0, 0.0);
	glClear(GL_COLOR_BUFFER_BIT);

	CopyEffects::get_singleton()->copy_to_rect_3d(Rect2i(0, 0, 1, 1), p_layer, Texture::TYPE_LAYERED);

	glReadPixels(0, 0, texture->alloc_width, texture->alloc_height, GL_RGBA, GL_UNSIGNED_BYTE, &w[0]);

	glBindFramebuffer(GL_FRAMEBUFFER, GLES3::TextureStorage::system_fbo);
	glDeleteTextures(1, &temp_color_texture);
	glDeleteFramebuffers(1, &temp_framebuffer);

	data.resize(data_size);

	ERR_FAIL_COND_V(data.is_empty(), Ref<Image>());
	Ref<Image> image = Image::create_from_data(texture->width, texture->height, false, Image::FORMAT_RGBA8, data);
	if (image->is_empty()) {
		const String &path_str = texture->path.is_empty() ? "with no path" : vformat("with path '%s'", texture->path);
		ERR_FAIL_V_MSG(Ref<Image>(), vformat("Texture %s has no data.", path_str));
	}

	if (texture->format != Image::FORMAT_RGBA8 && !Image::is_format_compressed(texture->format)) {
		image->convert(texture->format);
	}

	if (texture->mipmaps > 1) {
		image->generate_mipmaps();
	}

	return image;
}

Vector<Ref<Image>> TextureStorage::_texture_3d_read_framebuffer(GLES3::Texture *p_texture) const {
	ERR_FAIL_NULL_V(p_texture, Vector<Ref<Image>>());

	Vector<Ref<Image>> ret;
	Vector<uint8_t> data;

	int width = p_texture->width;
	int height = p_texture->height;
	int depth = p_texture->depth;

	for (int mipmap_level = 0; mipmap_level < p_texture->mipmaps; mipmap_level++) {
		int64_t data_size = Image::get_image_data_size(width, height, Image::FORMAT_RGBA8, false);
		glViewport(0, 0, width, height);
		glClearColor(0.0, 0.0, 0.0, 0.0);
		glClear(GL_COLOR_BUFFER_BIT);

		for (int layer = 0; layer < depth; layer++) {
			data.resize(data_size * 2); //add some memory at the end, just in case for buggy drivers
			uint8_t *w = data.ptrw();

			float layer_f = layer / float(depth);
			CopyEffects::get_singleton()->copy_to_rect_3d(Rect2i(0, 0, 1, 1), layer_f, Texture::TYPE_3D, mipmap_level);
			glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, &w[0]);

			data.resize(data_size);
			ERR_FAIL_COND_V(data.is_empty(), Vector<Ref<Image>>());

			Ref<Image> img = Image::create_from_data(width, height, false, Image::FORMAT_RGBA8, data);
			ERR_FAIL_COND_V(img->is_empty(), Vector<Ref<Image>>());

			if (p_texture->format != Image::FORMAT_RGBA8 && !Image::is_format_compressed(p_texture->format)) {
				img->convert(p_texture->format);
			}

			ret.push_back(img);
		}

		width = MAX(1, width >> 1);
		height = MAX(1, height >> 1);
		depth = MAX(1, depth >> 1);
	}

	return ret;
}

Vector<Ref<Image>> TextureStorage::texture_3d_get(RID p_texture) const {
	Texture *texture = texture_owner.get_or_null(p_texture);
	ERR_FAIL_NULL_V(texture, Vector<Ref<Image>>());
	ERR_FAIL_COND_V(texture->type != Texture::TYPE_3D, Vector<Ref<Image>>());

#ifdef TOOLS_ENABLED
	if (!texture->image_cache_3d.is_empty() && !texture->is_render_target) {
		return texture->image_cache_3d;
	}
#endif

	GLuint temp_framebuffer;
	glGenFramebuffers(1, &temp_framebuffer);

	GLuint temp_color_texture;
	glGenTextures(1, &temp_color_texture);

	glBindFramebuffer(GL_FRAMEBUFFER, temp_framebuffer);

	glBindTexture(GL_TEXTURE_2D, temp_color_texture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, texture->alloc_width, texture->alloc_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, temp_color_texture, 0);

	glDepthMask(GL_FALSE);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	glDisable(GL_BLEND);
	glDepthFunc(GL_LEQUAL);
	glColorMask(1, 1, 1, 1);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_3D, texture->tex_id);

	Vector<Ref<Image>> ret = _texture_3d_read_framebuffer(texture);

	glBindFramebuffer(GL_FRAMEBUFFER, GLES3::TextureStorage::system_fbo);
	glDeleteTextures(1, &temp_color_texture);
	glDeleteFramebuffers(1, &temp_framebuffer);

#ifdef TOOLS_ENABLED
	if (Engine::get_singleton()->is_editor_hint() && !texture->is_render_target) {
		texture->image_cache_3d = ret;
	}
#endif

	return ret;
}

void TextureStorage::texture_replace(RID p_texture, RID p_by_texture) {
	Texture *tex_to = texture_owner.get_or_null(p_texture);
	ERR_FAIL_NULL(tex_to);
	ERR_FAIL_COND(tex_to->is_proxy); //can't replace proxy
	Texture *tex_from = texture_owner.get_or_null(p_by_texture);
	ERR_FAIL_NULL(tex_from);
	ERR_FAIL_COND(tex_from->is_proxy); //can't replace proxy

	if (tex_to == tex_from) {
		return;
	}

	if (tex_to->canvas_texture) {
		memdelete(tex_to->canvas_texture);
		tex_to->canvas_texture = nullptr;
	}

	if (tex_to->tex_id) {
		GLES3::Utilities::get_singleton()->texture_free_data(tex_to->tex_id);
		tex_to->tex_id = 0;
	}

	Vector<RID> proxies_to_update = tex_to->proxies;
	Vector<RID> proxies_to_redirect = tex_from->proxies;

	*tex_to = *tex_from;

	tex_to->proxies = proxies_to_update; //restore proxies, so they can be updated

	if (tex_to->canvas_texture) {
		tex_to->canvas_texture->diffuse = p_texture; //update
	}

	for (int i = 0; i < proxies_to_update.size(); i++) {
		texture_proxy_update(proxies_to_update[i], p_texture);
	}
	for (int i = 0; i < proxies_to_redirect.size(); i++) {
		texture_proxy_update(proxies_to_redirect[i], p_texture);
	}
	//delete last, so proxies can be updated
	texture_owner.free(p_by_texture);

	texture_atlas_mark_dirty_on_texture(p_texture);
}

void TextureStorage::texture_set_size_override(RID p_texture, int p_width, int p_height) {
	Texture *texture = texture_owner.get_or_null(p_texture);

	ERR_FAIL_NULL(texture);
	ERR_FAIL_COND(texture->is_render_target);

	ERR_FAIL_COND(p_width <= 0 || p_width > 16384);
	ERR_FAIL_COND(p_height <= 0 || p_height > 16384);
	//real texture size is in alloc width and height
	texture->width = p_width;
	texture->height = p_height;
}

void TextureStorage::texture_set_path(RID p_texture, const String &p_path) {
	Texture *texture = texture_owner.get_or_null(p_texture);
	ERR_FAIL_NULL(texture);

	texture->path = p_path;
}

String TextureStorage::texture_get_path(RID p_texture) const {
	Texture *texture = texture_owner.get_or_null(p_texture);
	ERR_FAIL_NULL_V(texture, "");

	return texture->path;
}

void TextureStorage::texture_set_detect_3d_callback(RID p_texture, RS::TextureDetectCallback p_callback, void *p_userdata) {
	Texture *texture = texture_owner.get_or_null(p_texture);
	ERR_FAIL_NULL(texture);

	texture->detect_3d_callback = p_callback;
	texture->detect_3d_callback_ud = p_userdata;
}

void TextureStorage::texture_set_detect_srgb_callback(RID p_texture, RS::TextureDetectCallback p_callback, void *p_userdata) {
}

void TextureStorage::texture_set_detect_normal_callback(RID p_texture, RS::TextureDetectCallback p_callback, void *p_userdata) {
	Texture *texture = texture_owner.get_or_null(p_texture);
	ERR_FAIL_NULL(texture);

	texture->detect_normal_callback = p_callback;
	texture->detect_normal_callback_ud = p_userdata;
}

void TextureStorage::texture_set_detect_roughness_callback(RID p_texture, RS::TextureDetectRoughnessCallback p_callback, void *p_userdata) {
	Texture *texture = texture_owner.get_or_null(p_texture);
	ERR_FAIL_NULL(texture);

	texture->detect_roughness_callback = p_callback;
	texture->detect_roughness_callback_ud = p_userdata;
}

void TextureStorage::texture_debug_usage(List<RS::TextureInfo> *r_info) {
	List<RID> textures;
	texture_owner.get_owned_list(&textures);

	for (List<RID>::Element *E = textures.front(); E; E = E->next()) {
		Texture *t = texture_owner.get_or_null(E->get());
		if (!t) {
			continue;
		}
		RS::TextureInfo tinfo;
		tinfo.path = t->path;
		tinfo.format = t->format;
		tinfo.width = t->alloc_width;
		tinfo.height = t->alloc_height;
		tinfo.bytes = t->total_data_size;

		switch (t->type) {
			case Texture::TYPE_3D:
				tinfo.depth = t->depth;
				break;

			case Texture::TYPE_LAYERED:
				tinfo.depth = t->layers;
				break;

			default:
				tinfo.depth = 0;
				break;
		}

		r_info->push_back(tinfo);
	}
}

void TextureStorage::texture_set_force_redraw_if_visible(RID p_texture, bool p_enable) {
	Texture *texture = texture_owner.get_or_null(p_texture);
	ERR_FAIL_NULL(texture);

	texture->redraw_if_visible = p_enable;
}

Size2 TextureStorage::texture_size_with_proxy(RID p_texture) {
	const Texture *texture = texture_owner.get_or_null(p_texture);
	ERR_FAIL_NULL_V(texture, Size2());
	if (texture->is_proxy) {
		const Texture *proxy = texture_owner.get_or_null(texture->proxy_to);
		return Size2(proxy->width, proxy->height);
	} else {
		return Size2(texture->width, texture->height);
	}
}

void TextureStorage::texture_rd_initialize(RID p_texture, const RID &p_rd_texture, const RS::TextureLayeredType p_layer_type) {
}

RID TextureStorage::texture_get_rd_texture(RID p_texture, bool p_srgb) const {
	return RID();
}

uint64_t TextureStorage::texture_get_native_handle(RID p_texture, bool p_srgb) const {
	const Texture *texture = texture_owner.get_or_null(p_texture);
	ERR_FAIL_NULL_V(texture, 0);

	return texture->tex_id;
}

void TextureStorage::texture_set_data(RID p_texture, const Ref<Image> &p_image, int p_layer) {
	_texture_set_data(p_texture, p_image, p_layer, false);
}

void TextureStorage::_texture_set_data(RID p_texture, const Ref<Image> &p_image, int p_layer, bool p_initialize) {
	Texture *texture = texture_owner.get_or_null(p_texture);

	ERR_FAIL_NULL(texture);
	if (texture->target == GL_TEXTURE_3D) {
		// Target is set to a 3D texture or array texture, exit early to avoid spamming errors
		return;
	}
	ERR_FAIL_COND(!texture->active);
	ERR_FAIL_COND(texture->is_render_target);
	ERR_FAIL_COND(p_image.is_null());
	ERR_FAIL_COND(texture->format != p_image->get_format());

	ERR_FAIL_COND(!p_image->get_width());
	ERR_FAIL_COND(!p_image->get_height());

	GLenum type;
	GLenum format;
	GLenum internal_format;
	bool compressed = false;

	bool needs_decompress = texture->resize_to_po2;

	// Support for RGTC-compressed Texture Arrays isn't mandated by GLES3/WebGL.
	if (!RasterizerGLES3::is_gles_over_gl() && texture->target == GL_TEXTURE_2D_ARRAY) {
		if (p_image->get_format() == Image::FORMAT_RGTC_R || p_image->get_format() == Image::FORMAT_RGTC_RG) {
			needs_decompress = true;
		}
	}

	Image::Format real_format;
	Ref<Image> img = _get_gl_image_and_format(p_image, p_image->get_format(), real_format, format, internal_format, type, compressed, needs_decompress);
	ERR_FAIL_COND(img.is_null());
	if (texture->resize_to_po2) {
		if (p_image->is_compressed()) {
			ERR_PRINT("Texture '" + texture->path + "' is required to be a power of 2 because it uses either mipmaps or repeat, so it was decompressed. This will hurt performance and memory usage.");
		}

		if (img == p_image) {
			img = img->duplicate();
		}
		img->resize_to_po2(false);
	}

	GLenum blit_target = (texture->target == GL_TEXTURE_CUBE_MAP) ? _cube_side_enum[p_layer] : texture->target;

	Vector<uint8_t> read = img->get_data();

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(texture->target, texture->tex_id);
	_texture_set_swizzle(texture, real_format);

	int mipmaps = img->has_mipmaps() ? img->get_mipmap_count() + 1 : 1;

	// Set filtering and repeat state to default.
	if (mipmaps > 1) {
		texture->gl_set_filter(RS::CANVAS_ITEM_TEXTURE_FILTER_NEAREST_WITH_MIPMAPS);
	} else {
		texture->gl_set_filter(RS::CANVAS_ITEM_TEXTURE_FILTER_NEAREST);
	}

	texture->gl_set_repeat(RS::CANVAS_ITEM_TEXTURE_REPEAT_ENABLED);

	int w = img->get_width();
	int h = img->get_height();

	int tsize = 0;

	for (int i = 0; i < mipmaps; i++) {
		int64_t size, ofs;
		img->get_mipmap_offset_and_size(i, ofs, size);
		if (compressed) {
			glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
			if (texture->target == GL_TEXTURE_2D_ARRAY) {
				if (p_initialize) {
					glCompressedTexImage3D(GL_TEXTURE_2D_ARRAY, i, internal_format, w, h, texture->layers, 0, size * texture->layers, nullptr);
				}
				glCompressedTexSubImage3D(GL_TEXTURE_2D_ARRAY, i, 0, 0, p_layer, w, h, 1, internal_format, size, &read[ofs]);
			} else {
				glCompressedTexImage2D(blit_target, i, internal_format, w, h, 0, size, &read[ofs]);
			}
		} else {
			glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
			if (texture->target == GL_TEXTURE_2D_ARRAY) {
				if (p_initialize) {
					glTexImage3D(GL_TEXTURE_2D_ARRAY, i, internal_format, w, h, texture->layers, 0, format, type, nullptr);
				}
				glTexSubImage3D(GL_TEXTURE_2D_ARRAY, i, 0, 0, p_layer, w, h, 1, format, type, &read[ofs]);
			} else {
				glTexImage2D(blit_target, i, internal_format, w, h, 0, format, type, &read[ofs]);
			}
		}

		tsize += size;

		w = MAX(1, w >> 1);
		h = MAX(1, h >> 1);
	}

	if (texture->target == GL_TEXTURE_CUBE_MAP || texture->target == GL_TEXTURE_2D_ARRAY) {
		texture->total_data_size = tsize * texture->layers;
	} else {
		texture->total_data_size = tsize;
	}

	texture->stored_cube_sides |= (1 << p_layer);

	texture->mipmaps = mipmaps;
}

void TextureStorage::_texture_set_3d_data(RID p_texture, const Vector<Ref<Image>> &p_data, bool p_initialize) {
	Texture *texture = texture_owner.get_or_null(p_texture);

	ERR_FAIL_NULL(texture);
	ERR_FAIL_COND(!texture->active);
	ERR_FAIL_COND(texture->is_render_target);
	ERR_FAIL_COND(texture->target != GL_TEXTURE_3D);
	ERR_FAIL_COND(p_data.is_empty());

	GLenum type;
	GLenum format;
	GLenum internal_format;
	bool compressed = false;

	Image::Format real_format;
	Ref<Image> img = _get_gl_image_and_format(p_data[0], p_data[0]->get_format(), real_format, format, internal_format, type, compressed, texture->resize_to_po2);
	ERR_FAIL_COND(img.is_null());

	ERR_FAIL_COND_MSG(compressed, "Compressed 3D textures are not supported in the Compatibility renderer.");

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(texture->target, texture->tex_id);
	_texture_set_swizzle(texture, texture->real_format);

	// Set filtering and repeat state to default.
	if (texture->mipmaps > 1) {
		texture->gl_set_filter(RS::CANVAS_ITEM_TEXTURE_FILTER_NEAREST_WITH_MIPMAPS);
	} else {
		texture->gl_set_filter(RS::CANVAS_ITEM_TEXTURE_FILTER_NEAREST);
	}

	texture->gl_set_repeat(RS::CANVAS_ITEM_TEXTURE_REPEAT_ENABLED);

	Vector<Ref<Image>> images;
	images.resize(p_data.size());
	for (int i = 0; i < p_data.size(); i++) {
		Ref<Image> image = p_data[i];
		if (image->get_format() != texture->format) {
			image = image->duplicate();
			image->convert(texture->format);
		}
		images.write[i] = image;
	}

	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

	int all_data_size = 0;
	int mipmap_level = 0;
	int layer = 0;
	int depth = texture->depth;
	Size2i prev_size(images[0]->get_width(), images[0]->get_height());
	for (int i = 0; i < images.size(); i++) {
		Ref<Image> image = images[i];
		Size2i img_size(image->get_width(), image->get_height());

		if (img_size != prev_size) {
			mipmap_level++;
			depth = MAX(1, depth >> 1);
			layer = 0;
		}
		prev_size = img_size;
		all_data_size += image->get_data().size();

		if (layer == 0 && p_initialize) {
			glTexImage3D(GL_TEXTURE_3D, mipmap_level, internal_format, img_size.width, img_size.height, depth, 0, format, type, nullptr);
		}

		glTexSubImage3D(GL_TEXTURE_3D, mipmap_level, 0, 0, layer, img_size.width, img_size.height, 1, format, type, image->get_data().ptr());

		layer++;
	}

	texture->total_data_size = all_data_size;
	texture->mipmaps = mipmap_level + 1;

#ifdef TOOLS_ENABLED
	if (Engine::get_singleton()->is_editor_hint() && !texture->is_render_target) {
		texture->image_cache_3d = images;
	}
#endif
}

void TextureStorage::_texture_set_swizzle(GLES3::Texture *p_texture, Image::Format p_real_format) {
#ifndef WEB_ENABLED
	switch (p_texture->format) {
		case Image::FORMAT_L8: {
			if (RasterizerGLES3::is_gles_over_gl()) {
				glTexParameteri(p_texture->target, GL_TEXTURE_SWIZZLE_R, GL_RED);
				glTexParameteri(p_texture->target, GL_TEXTURE_SWIZZLE_G, GL_RED);
				glTexParameteri(p_texture->target, GL_TEXTURE_SWIZZLE_B, GL_RED);
				glTexParameteri(p_texture->target, GL_TEXTURE_SWIZZLE_A, GL_ONE);
			} else {
				glTexParameteri(p_texture->target, GL_TEXTURE_SWIZZLE_R, GL_RED);
				glTexParameteri(p_texture->target, GL_TEXTURE_SWIZZLE_G, GL_GREEN);
				glTexParameteri(p_texture->target, GL_TEXTURE_SWIZZLE_B, GL_BLUE);
				glTexParameteri(p_texture->target, GL_TEXTURE_SWIZZLE_A, GL_ALPHA);
			}
		} break;
		case Image::FORMAT_LA8: {
			if (RasterizerGLES3::is_gles_over_gl()) {
				glTexParameteri(p_texture->target, GL_TEXTURE_SWIZZLE_R, GL_RED);
				glTexParameteri(p_texture->target, GL_TEXTURE_SWIZZLE_G, GL_RED);
				glTexParameteri(p_texture->target, GL_TEXTURE_SWIZZLE_B, GL_RED);
				glTexParameteri(p_texture->target, GL_TEXTURE_SWIZZLE_A, GL_GREEN);
			} else {
				glTexParameteri(p_texture->target, GL_TEXTURE_SWIZZLE_R, GL_RED);
				glTexParameteri(p_texture->target, GL_TEXTURE_SWIZZLE_G, GL_GREEN);
				glTexParameteri(p_texture->target, GL_TEXTURE_SWIZZLE_B, GL_BLUE);
				glTexParameteri(p_texture->target, GL_TEXTURE_SWIZZLE_A, GL_ALPHA);
			}
		} break;
		case Image::FORMAT_ETC2_RA_AS_RG:
		case Image::FORMAT_DXT5_RA_AS_RG: {
			glTexParameteri(p_texture->target, GL_TEXTURE_SWIZZLE_R, GL_RED);
			if (p_texture->format == p_real_format) {
				// Swizzle RA from compressed texture into RG.
				glTexParameteri(p_texture->target, GL_TEXTURE_SWIZZLE_G, GL_ALPHA);
			} else {
				// Converted textures are already in RG, leave as-is.
				glTexParameteri(p_texture->target, GL_TEXTURE_SWIZZLE_G, GL_GREEN);
			}
			glTexParameteri(p_texture->target, GL_TEXTURE_SWIZZLE_B, GL_ZERO);
			glTexParameteri(p_texture->target, GL_TEXTURE_SWIZZLE_A, GL_ONE);
		} break;
		default: {
			glTexParameteri(p_texture->target, GL_TEXTURE_SWIZZLE_R, GL_RED);
			glTexParameteri(p_texture->target, GL_TEXTURE_SWIZZLE_G, GL_GREEN);
			glTexParameteri(p_texture->target, GL_TEXTURE_SWIZZLE_B, GL_BLUE);
			glTexParameteri(p_texture->target, GL_TEXTURE_SWIZZLE_A, GL_ALPHA);
		} break;
	}
#endif // WEB_ENABLED
}

Image::Format TextureStorage::texture_get_format(RID p_texture) const {
	Texture *texture = texture_owner.get_or_null(p_texture);

	ERR_FAIL_NULL_V(texture, Image::FORMAT_L8);

	return texture->format;
}

uint32_t TextureStorage::texture_get_texid(RID p_texture) const {
	Texture *texture = texture_owner.get_or_null(p_texture);

	ERR_FAIL_NULL_V(texture, 0);

	return texture->tex_id;
}

Vector3i TextureStorage::texture_get_size(RID p_texture) const {
	Texture *texture = texture_owner.get_or_null(p_texture);

	ERR_FAIL_NULL_V(texture, Vector3i(0, 0, 0));

	return Vector3i(texture->width, texture->height, texture->depth);
}

uint32_t TextureStorage::texture_get_width(RID p_texture) const {
	Texture *texture = texture_owner.get_or_null(p_texture);

	ERR_FAIL_NULL_V(texture, 0);

	return texture->width;
}

uint32_t TextureStorage::texture_get_height(RID p_texture) const {
	Texture *texture = texture_owner.get_or_null(p_texture);

	ERR_FAIL_NULL_V(texture, 0);

	return texture->height;
}

uint32_t TextureStorage::texture_get_depth(RID p_texture) const {
	Texture *texture = texture_owner.get_or_null(p_texture);

	ERR_FAIL_NULL_V(texture, 0);

	return texture->depth;
}

void TextureStorage::texture_bind(RID p_texture, uint32_t p_texture_no) {
	Texture *texture = texture_owner.get_or_null(p_texture);

	ERR_FAIL_NULL(texture);

	glActiveTexture(GL_TEXTURE0 + p_texture_no);
	glBindTexture(texture->target, texture->tex_id);
}

/* TEXTURE ATLAS API */

void TextureStorage::texture_add_to_texture_atlas(RID p_texture) {
	if (!texture_atlas.textures.has(p_texture)) {
		TextureAtlas::Texture t;
		t.users = 1;
		texture_atlas.textures[p_texture] = t;
		texture_atlas.dirty = true;
	} else {
		TextureAtlas::Texture *t = texture_atlas.textures.getptr(p_texture);
		t->users++;
	}
}

void TextureStorage::texture_remove_from_texture_atlas(RID p_texture) {
	TextureAtlas::Texture *t = texture_atlas.textures.getptr(p_texture);
	ERR_FAIL_NULL(t);
	t->users--;
	if (t->users == 0) {
		texture_atlas.textures.erase(p_texture);
		// Do not mark it dirty, there is no need to since it remains working.
	}
}

void TextureStorage::texture_atlas_mark_dirty_on_texture(RID p_texture) {
	if (texture_atlas.textures.has(p_texture)) {
		texture_atlas.dirty = true; // Mark it dirty since it was most likely modified.
	}
}

void TextureStorage::texture_atlas_remove_texture(RID p_texture) {
	if (texture_atlas.textures.has(p_texture)) {
		texture_atlas.textures.erase(p_texture);
		// There is not much a point of making it dirty, texture can be removed next time the atlas is updated.
	}
}

GLuint TextureStorage::texture_atlas_get_texture() const {
	return texture_atlas.texture;
}

void TextureStorage::update_texture_atlas() {
	CopyEffects *copy_effects = CopyEffects::get_singleton();
	ERR_FAIL_NULL(copy_effects);

	if (!texture_atlas.dirty) {
		return; //nothing to do
	}

	texture_atlas.dirty = false;

	if (texture_atlas.texture != 0) {
		GLES3::Utilities::get_singleton()->texture_free_data(texture_atlas.texture);
		texture_atlas.texture = 0;
		glDeleteFramebuffers(1, &texture_atlas.framebuffer);
		texture_atlas.framebuffer = 0;
	}

	const int border = 2;

	if (texture_atlas.textures.size()) {
		//generate atlas
		Vector<TextureAtlas::SortItem> itemsv;
		itemsv.resize(texture_atlas.textures.size());
		uint32_t base_size = 8;

		int idx = 0;

		for (const KeyValue<RID, TextureAtlas::Texture> &E : texture_atlas.textures) {
			TextureAtlas::SortItem &si = itemsv.write[idx];

			Texture *src_tex = get_texture(E.key);

			si.size.width = (src_tex->width / border) + 1;
			si.size.height = (src_tex->height / border) + 1;
			si.pixel_size = Size2i(src_tex->width, src_tex->height);

			if (base_size < (uint32_t)si.size.width) {
				base_size = nearest_power_of_2_templated(si.size.width);
			}

			si.texture = E.key;
			idx++;
		}

		//sort items by size
		itemsv.sort();

		//attempt to create atlas
		int item_count = itemsv.size();
		TextureAtlas::SortItem *items = itemsv.ptrw();

		int atlas_height = 0;

		while (true) {
			Vector<int> v_offsetsv;
			v_offsetsv.resize(base_size);

			int *v_offsets = v_offsetsv.ptrw();
			memset(v_offsets, 0, sizeof(int) * base_size);

			int max_height = 0;

			for (int i = 0; i < item_count; i++) {
				//best fit
				TextureAtlas::SortItem &si = items[i];
				int best_idx = -1;
				int best_height = 0x7FFFFFFF;
				for (uint32_t j = 0; j <= base_size - si.size.width; j++) {
					int height = 0;
					for (int k = 0; k < si.size.width; k++) {
						int h = v_offsets[k + j];
						if (h > height) {
							height = h;
							if (height > best_height) {
								break; //already bad
							}
						}
					}

					if (height < best_height) {
						best_height = height;
						best_idx = j;
					}
				}

				//update
				for (int k = 0; k < si.size.width; k++) {
					v_offsets[k + best_idx] = best_height + si.size.height;
				}

				si.pos.x = best_idx;
				si.pos.y = best_height;

				if (si.pos.y + si.size.height > max_height) {
					max_height = si.pos.y + si.size.height;
				}
			}

			if ((uint32_t)max_height <= base_size * 2) {
				atlas_height = max_height;
				break; //good ratio, break;
			}

			base_size *= 2;
		}

		texture_atlas.size.width = base_size * border;
		texture_atlas.size.height = nearest_power_of_2_templated(atlas_height * border);

		for (int i = 0; i < item_count; i++) {
			TextureAtlas::Texture *t = texture_atlas.textures.getptr(items[i].texture);
			t->uv_rect.position = items[i].pos * border + Vector2i(border / 2, border / 2);
			t->uv_rect.size = items[i].pixel_size;

			t->uv_rect.position /= Size2(texture_atlas.size);
			t->uv_rect.size /= Size2(texture_atlas.size);
		}
	} else {
		texture_atlas.size.width = 4;
		texture_atlas.size.height = 4;
	}

	{ // Atlas Texture initialize.
		// TODO validate texture atlas size with maximum texture size
		glGenTextures(1, &texture_atlas.texture);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, texture_atlas.texture);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, texture_atlas.size.width, texture_atlas.size.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
		GLES3::Utilities::get_singleton()->texture_allocated_data(texture_atlas.texture, texture_atlas.size.width * texture_atlas.size.height * 4, "Texture atlas");

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 1);

		glGenFramebuffers(1, &texture_atlas.framebuffer);
		glBindFramebuffer(GL_FRAMEBUFFER, texture_atlas.framebuffer);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture_atlas.texture, 0);

		GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);

		if (status != GL_FRAMEBUFFER_COMPLETE) {
			glDeleteFramebuffers(1, &texture_atlas.framebuffer);
			texture_atlas.framebuffer = 0;
			GLES3::Utilities::get_singleton()->texture_free_data(texture_atlas.texture);
			texture_atlas.texture = 0;
			WARN_PRINT("Could not create texture atlas, status: " + get_framebuffer_error(status));
			return;
		}
		glViewport(0, 0, texture_atlas.size.width, texture_atlas.size.height);
		glClearColor(0.0, 0.0, 0.0, 0.0);
		glClear(GL_COLOR_BUFFER_BIT);
		glBindTexture(GL_TEXTURE_2D, 0);
	}

	glDisable(GL_BLEND);

	if (texture_atlas.textures.size()) {
		for (const KeyValue<RID, TextureAtlas::Texture> &E : texture_atlas.textures) {
			TextureAtlas::Texture *t = texture_atlas.textures.getptr(E.key);
			Texture *src_tex = get_texture(E.key);
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, src_tex->tex_id);
			copy_effects->copy_to_rect(t->uv_rect);
		}
	}
	glBindFramebuffer(GL_FRAMEBUFFER, GLES3::TextureStorage::system_fbo);
}

/* DECAL API */

RID TextureStorage::decal_allocate() {
	return RID();
}

void TextureStorage::decal_initialize(RID p_rid) {
}

void TextureStorage::decal_set_size(RID p_decal, const Vector3 &p_size) {
}

void TextureStorage::decal_set_texture(RID p_decal, RS::DecalTexture p_type, RID p_texture) {
}

void TextureStorage::decal_set_emission_energy(RID p_decal, float p_energy) {
}

void TextureStorage::decal_set_albedo_mix(RID p_decal, float p_mix) {
}

void TextureStorage::decal_set_modulate(RID p_decal, const Color &p_modulate) {
}

void TextureStorage::decal_set_cull_mask(RID p_decal, uint32_t p_layers) {
}

void TextureStorage::decal_set_distance_fade(RID p_decal, bool p_enabled, float p_begin, float p_length) {
}

void TextureStorage::decal_set_fade(RID p_decal, float p_above, float p_below) {
}

void TextureStorage::decal_set_normal_fade(RID p_decal, float p_fade) {
}

AABB TextureStorage::decal_get_aabb(RID p_decal) const {
	return AABB();
}

/* RENDER TARGET API */

GLuint TextureStorage::system_fbo = 0;

void TextureStorage::_update_render_target(RenderTarget *rt) {
	// do not allocate a render target with no size
	if (rt->size.x <= 0 || rt->size.y <= 0) {
		return;
	}

	// do not allocate a render target that is attached to the screen
	if (rt->direct_to_screen) {
		rt->fbo = system_fbo;
		return;
	}

	Config *config = Config::get_singleton();

	if (rt->hdr) {
		rt->color_internal_format = GL_RGBA16F;
		rt->color_format = GL_RGBA;
		rt->color_type = GL_FLOAT;
		rt->color_format_size = 8;
		rt->image_format = Image::FORMAT_RGBAF;
	} else if (rt->is_transparent) {
		rt->color_internal_format = GL_RGBA8;
		rt->color_format = GL_RGBA;
		rt->color_type = GL_UNSIGNED_BYTE;
		rt->color_format_size = 4;
		rt->image_format = Image::FORMAT_RGBA8;
	} else {
		rt->color_internal_format = GL_RGB10_A2;
		rt->color_format = GL_RGBA;
		rt->color_type = GL_UNSIGNED_INT_2_10_10_10_REV;
		rt->color_format_size = 4;
		rt->image_format = Image::FORMAT_RGBA8;
	}

	glDisable(GL_SCISSOR_TEST);
	glColorMask(1, 1, 1, 1);
	glDepthMask(GL_FALSE);

	{
		Texture *texture;
		bool use_multiview = rt->view_count > 1 && config->multiview_supported;
		GLenum texture_target = use_multiview ? GL_TEXTURE_2D_ARRAY : GL_TEXTURE_2D;

		/* Front FBO */

		glGenFramebuffers(1, &rt->fbo);
		glBindFramebuffer(GL_FRAMEBUFFER, rt->fbo);

		// color
		if (rt->overridden.color.is_valid()) {
			texture = get_texture(rt->overridden.color);
			ERR_FAIL_NULL(texture);

			rt->color = texture->tex_id;
			rt->size = Size2i(texture->width, texture->height);
		} else {
			texture = get_texture(rt->texture);
			ERR_FAIL_NULL(texture);

			glGenTextures(1, &rt->color);
			glBindTexture(texture_target, rt->color);

			if (use_multiview) {
				glTexImage3D(texture_target, 0, rt->color_internal_format, rt->size.x, rt->size.y, rt->view_count, 0, rt->color_format, rt->color_type, nullptr);
			} else {
				glTexImage2D(texture_target, 0, rt->color_internal_format, rt->size.x, rt->size.y, 0, rt->color_format, rt->color_type, nullptr);
			}

			texture->gl_set_filter(RS::CANVAS_ITEM_TEXTURE_FILTER_NEAREST);
			texture->gl_set_repeat(RS::CANVAS_ITEM_TEXTURE_REPEAT_DISABLED);

			GLES3::Utilities::get_singleton()->texture_allocated_data(rt->color, rt->size.x * rt->size.y * rt->view_count * rt->color_format_size, "Render target color texture");
		}
#ifndef IOS_ENABLED
		if (use_multiview) {
			glFramebufferTextureMultiviewOVR(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, rt->color, 0, 0, rt->view_count);
		} else {
#else
		{
#endif
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, texture_target, rt->color, 0);
		}

		// depth
		if (rt->overridden.depth.is_valid()) {
			texture = get_texture(rt->overridden.depth);
			ERR_FAIL_NULL(texture);

			rt->depth = texture->tex_id;
		} else {
			glGenTextures(1, &rt->depth);
			glBindTexture(texture_target, rt->depth);

			if (use_multiview) {
				glTexImage3D(texture_target, 0, GL_DEPTH_COMPONENT24, rt->size.x, rt->size.y, rt->view_count, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, nullptr);
			} else {
				glTexImage2D(texture_target, 0, GL_DEPTH_COMPONENT24, rt->size.x, rt->size.y, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, nullptr);
			}

			glTexParameteri(texture_target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			glTexParameteri(texture_target, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glTexParameteri(texture_target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(texture_target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

			GLES3::Utilities::get_singleton()->texture_allocated_data(rt->depth, rt->size.x * rt->size.y * rt->view_count * 3, "Render target depth texture");
		}
#ifndef IOS_ENABLED
		if (use_multiview) {
			glFramebufferTextureMultiviewOVR(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, rt->depth, 0, 0, rt->view_count);
		} else {
#else
		{
#endif
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, texture_target, rt->depth, 0);
		}

		GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
		if (status != GL_FRAMEBUFFER_COMPLETE) {
			glDeleteFramebuffers(1, &rt->fbo);
			if (rt->overridden.color.is_null()) {
				GLES3::Utilities::get_singleton()->texture_free_data(rt->color);
			}
			if (rt->overridden.depth.is_null()) {
				GLES3::Utilities::get_singleton()->texture_free_data(rt->depth);
			}
			rt->fbo = 0;
			rt->size.x = 0;
			rt->size.y = 0;
			rt->color = 0;
			rt->depth = 0;
			if (rt->overridden.color.is_null()) {
				texture->tex_id = 0;
				texture->active = false;
			}
			WARN_PRINT("Could not create render target, status: " + get_framebuffer_error(status));
			return;
		}

		texture->is_render_target = true;
		texture->render_target = rt;
		if (rt->overridden.color.is_null()) {
			texture->format = rt->image_format;
			texture->real_format = rt->image_format;
			texture->target = texture_target;
			if (rt->view_count > 1 && config->multiview_supported) {
				texture->type = Texture::TYPE_LAYERED;
				texture->layers = rt->view_count;
			} else {
				texture->type = Texture::TYPE_2D;
				texture->layers = 1;
			}
			texture->gl_format_cache = rt->color_format;
			texture->gl_type_cache = !rt->hdr ? GL_UNSIGNED_BYTE : GL_FLOAT; // to set HDR format size to 8 and keep 4 for LDR format
			texture->gl_internal_format_cache = rt->color_internal_format;
			texture->tex_id = rt->color;
			texture->width = rt->size.x;
			texture->alloc_width = rt->size.x;
			texture->height = rt->size.y;
			texture->alloc_height = rt->size.y;
			texture->active = true;
		}
	}

	glClearColor(0, 0, 0, 0);
	glClear(GL_COLOR_BUFFER_BIT);
	glBindFramebuffer(GL_FRAMEBUFFER, system_fbo);
}

void TextureStorage::_create_render_target_backbuffer(RenderTarget *rt) {
	ERR_FAIL_COND_MSG(rt->backbuffer_fbo != 0, "Cannot allocate RenderTarget backbuffer: already initialized.");
	ERR_FAIL_COND(rt->direct_to_screen);
	// Allocate mipmap chains for full screen blur
	// Limit mipmaps so smallest is 32x32 to avoid unnecessary framebuffer switches
	int count = MAX(1, Image::get_image_required_mipmaps(rt->size.x, rt->size.y, Image::FORMAT_RGBA8) - 4);
	if (rt->size.x > 40 && rt->size.y > 40) {
		GLsizei width = rt->size.x;
		GLsizei height = rt->size.y;

		rt->mipmap_count = count;

		glGenTextures(1, &rt->backbuffer);
		glBindTexture(GL_TEXTURE_2D, rt->backbuffer);
		uint32_t texture_size_bytes = 0;

		for (int l = 0; l < count; l++) {
			texture_size_bytes += width * height * 4;
			glTexImage2D(GL_TEXTURE_2D, l, rt->color_internal_format, width, height, 0, rt->color_format, rt->color_type, nullptr);
			width = MAX(1, (width / 2));
			height = MAX(1, (height / 2));
		}

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, count - 1);

		glGenFramebuffers(1, &rt->backbuffer_fbo);
		glBindFramebuffer(GL_FRAMEBUFFER, rt->backbuffer_fbo);

		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, rt->backbuffer, 0);

		GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
		if (status != GL_FRAMEBUFFER_COMPLETE) {
			WARN_PRINT_ONCE("Cannot allocate mipmaps for canvas screen blur. Status: " + get_framebuffer_error(status));
			glBindFramebuffer(GL_FRAMEBUFFER, system_fbo);
			return;
		}
		GLES3::Utilities::get_singleton()->texture_allocated_data(rt->backbuffer, texture_size_bytes, "Render target backbuffer color texture");

		// Initialize all levels to clear black.
		for (int j = 0; j < count; j++) {
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, rt->backbuffer, j);
			glClearColor(0.0, 0.0, 0.0, 0.0);
			glClear(GL_COLOR_BUFFER_BIT);
		}

		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, rt->backbuffer, 0);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	}
}
void GLES3::TextureStorage::check_backbuffer(RenderTarget *rt, const bool uses_screen_texture, const bool uses_depth_texture) {
	if (rt->backbuffer != 0 && rt->backbuffer_depth != 0) {
		return;
	}

	Config *config = Config::get_singleton();
	bool use_multiview = rt->view_count > 1 && config->multiview_supported;
	GLenum texture_target = use_multiview ? GL_TEXTURE_2D_ARRAY : GL_TEXTURE_2D;
	if (rt->backbuffer_fbo == 0) {
		glGenFramebuffers(1, &rt->backbuffer_fbo);
	}
	glBindFramebuffer(GL_FRAMEBUFFER, rt->backbuffer_fbo);
	if (rt->backbuffer == 0 && uses_screen_texture) {
		glGenTextures(1, &rt->backbuffer);
		glBindTexture(texture_target, rt->backbuffer);
		if (use_multiview) {
			glTexImage3D(texture_target, 0, rt->color_internal_format, rt->size.x, rt->size.y, rt->view_count, 0, rt->color_format, rt->color_type, nullptr);
		} else {
			glTexImage2D(texture_target, 0, rt->color_internal_format, rt->size.x, rt->size.y, 0, rt->color_format, rt->color_type, nullptr);
		}
		GLES3::Utilities::get_singleton()->texture_allocated_data(rt->backbuffer, rt->size.x * rt->size.y * rt->view_count * rt->color_format_size, "Render target backbuffer color texture (3D)");
		glTexParameteri(texture_target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(texture_target, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(texture_target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(texture_target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
#ifndef IOS_ENABLED
		if (use_multiview) {
			glFramebufferTextureMultiviewOVR(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, rt->backbuffer, 0, 0, rt->view_count);
		} else {
#else
		{
#endif
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, rt->backbuffer, 0);
		}
	}
	if (rt->backbuffer_depth == 0 && uses_depth_texture) {
		glGenTextures(1, &rt->backbuffer_depth);
		glBindTexture(texture_target, rt->backbuffer_depth);
		if (use_multiview) {
			glTexImage3D(texture_target, 0, GL_DEPTH_COMPONENT24, rt->size.x, rt->size.y, rt->view_count, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, nullptr);
		} else {
			glTexImage2D(texture_target, 0, GL_DEPTH_COMPONENT24, rt->size.x, rt->size.y, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, nullptr);
		}
		GLES3::Utilities::get_singleton()->texture_allocated_data(rt->backbuffer_depth, rt->size.x * rt->size.y * rt->view_count * 3, "Render target backbuffer depth texture");

		glTexParameteri(texture_target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(texture_target, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(texture_target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(texture_target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
#ifndef IOS_ENABLED
		if (use_multiview) {
			glFramebufferTextureMultiviewOVR(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, rt->backbuffer_depth, 0, 0, rt->view_count);
		} else {
#else
		{
#endif
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, rt->backbuffer_depth, 0);
		}
	}
}
void TextureStorage::_clear_render_target(RenderTarget *rt) {
	// there is nothing else to clear when DIRECT_TO_SCREEN is used
	if (rt->direct_to_screen) {
		return;
	}

	// Dispose of the cached fbo's and the allocated textures
	for (KeyValue<uint32_t, RenderTarget::RTOverridden::FBOCacheEntry> &E : rt->overridden.fbo_cache) {
		glDeleteTextures(E.value.allocated_textures.size(), E.value.allocated_textures.ptr());
		// Don't delete the current FBO, we'll do that a couple lines down.
		if (E.value.fbo != rt->fbo) {
			glDeleteFramebuffers(1, &E.value.fbo);
		}
	}
	rt->overridden.fbo_cache.clear();

	if (rt->fbo) {
		glDeleteFramebuffers(1, &rt->fbo);
		rt->fbo = 0;
	}

	if (rt->overridden.color.is_null()) {
		if (rt->texture.is_valid()) {
			Texture *tex = get_texture(rt->texture);
			tex->alloc_height = 0;
			tex->alloc_width = 0;
			tex->width = 0;
			tex->height = 0;
			tex->active = false;
			tex->render_target = nullptr;
			tex->is_render_target = false;
			tex->gl_set_filter(RS::CANVAS_ITEM_TEXTURE_FILTER_MAX);
			tex->gl_set_repeat(RS::CANVAS_ITEM_TEXTURE_REPEAT_MAX);
		}
	} else {
		Texture *tex = get_texture(rt->overridden.color);
		tex->render_target = nullptr;
		tex->is_render_target = false;
	}

	if (rt->overridden.color.is_valid()) {
		rt->overridden.color = RID();
	} else if (rt->color) {
		GLES3::Utilities::get_singleton()->texture_free_data(rt->color);
		if (rt->texture.is_valid()) {
			Texture *tex = get_texture(rt->texture);
			tex->tex_id = 0;
		}
	}
	rt->color = 0;

	if (rt->overridden.depth.is_valid()) {
		rt->overridden.depth = RID();
	} else if (rt->depth) {
		GLES3::Utilities::get_singleton()->texture_free_data(rt->depth);
	}
	rt->depth = 0;

	rt->overridden.velocity = RID();
	rt->overridden.is_overridden = false;

	if (rt->backbuffer_fbo != 0) {
		glDeleteFramebuffers(1, &rt->backbuffer_fbo);
		rt->backbuffer_fbo = 0;
	}
	if (rt->backbuffer != 0) {
		GLES3::Utilities::get_singleton()->texture_free_data(rt->backbuffer);
		rt->backbuffer = 0;
	}
	if (rt->backbuffer_depth != 0) {
		GLES3::Utilities::get_singleton()->texture_free_data(rt->backbuffer_depth);
		rt->backbuffer_depth = 0;
	}
	_render_target_clear_sdf(rt);
}

RID TextureStorage::render_target_create() {
	RenderTarget render_target;
	render_target.used_in_frame = false;
	render_target.clear_requested = false;

	Texture t;
	t.active = true;
	t.render_target = &render_target;
	t.is_render_target = true;

	render_target.texture = texture_owner.make_rid(t);
	_update_render_target(&render_target);
	return render_target_owner.make_rid(render_target);
}

void TextureStorage::render_target_free(RID p_rid) {
	RenderTarget *rt = render_target_owner.get_or_null(p_rid);
	_clear_render_target(rt);

	Texture *t = get_texture(rt->texture);
	if (t) {
		t->is_render_target = false;
		if (rt->overridden.color.is_null()) {
			texture_free(rt->texture);
		}
		//memdelete(t);
	}
	render_target_owner.free(p_rid);
}

void TextureStorage::render_target_set_position(RID p_render_target, int p_x, int p_y) {
	RenderTarget *rt = render_target_owner.get_or_null(p_render_target);
	ERR_FAIL_NULL(rt);

	rt->position = Point2i(p_x, p_y);
}

Point2i TextureStorage::render_target_get_position(RID p_render_target) const {
	RenderTarget *rt = render_target_owner.get_or_null(p_render_target);
	ERR_FAIL_NULL_V(rt, Point2i());

	return rt->position;
}

void TextureStorage::render_target_set_size(RID p_render_target, int p_width, int p_height, uint32_t p_view_count) {
	RenderTarget *rt = render_target_owner.get_or_null(p_render_target);
	ERR_FAIL_NULL(rt);

	if (p_width == rt->size.x && p_height == rt->size.y && p_view_count == rt->view_count) {
		return;
	}
	if (rt->overridden.color.is_valid()) {
		return;
	}

	_clear_render_target(rt);

	rt->size = Size2i(p_width, p_height);
	rt->view_count = p_view_count;

	_update_render_target(rt);
}

// TODO: convert to Size2i internally
Size2i TextureStorage::render_target_get_size(RID p_render_target) const {
	RenderTarget *rt = render_target_owner.get_or_null(p_render_target);
	ERR_FAIL_NULL_V(rt, Size2i());

	return rt->size;
}

void TextureStorage::render_target_set_override(RID p_render_target, RID p_color_texture, RID p_depth_texture, RID p_velocity_texture, RID p_velocity_depth_texture) {
	RenderTarget *rt = render_target_owner.get_or_null(p_render_target);
	ERR_FAIL_NULL(rt);
	ERR_FAIL_COND(rt->direct_to_screen);

	rt->overridden.velocity = p_velocity_texture;

	if (rt->overridden.color == p_color_texture && rt->overridden.depth == p_depth_texture) {
		return;
	}

	if (p_color_texture.is_null() && p_depth_texture.is_null()) {
		_clear_render_target(rt);
		_update_render_target(rt);
		return;
	}

	if (!rt->overridden.is_overridden) {
		_clear_render_target(rt);
	}

	rt->overridden.color = p_color_texture;
	rt->overridden.depth = p_depth_texture;
	rt->overridden.is_overridden = true;

	uint32_t hash_key = hash_murmur3_one_64(p_color_texture.get_id());
	hash_key = hash_murmur3_one_64(p_depth_texture.get_id(), hash_key);
	hash_key = hash_fmix32(hash_key);

	RBMap<uint32_t, RenderTarget::RTOverridden::FBOCacheEntry>::Element *cache;
	if ((cache = rt->overridden.fbo_cache.find(hash_key)) != nullptr) {
		rt->fbo = cache->get().fbo;
		rt->color = cache->get().color;
		rt->depth = cache->get().depth;
		rt->size = cache->get().size;
		rt->texture = p_color_texture;
		return;
	}

	_update_render_target(rt);

	RenderTarget::RTOverridden::FBOCacheEntry new_entry;
	new_entry.fbo = rt->fbo;
	new_entry.color = rt->color;
	new_entry.depth = rt->depth;
	new_entry.size = rt->size;
	// Keep track of any textures we had to allocate because they weren't overridden.
	if (p_color_texture.is_null()) {
		new_entry.allocated_textures.push_back(rt->color);
	}
	if (p_depth_texture.is_null()) {
		new_entry.allocated_textures.push_back(rt->depth);
	}
	rt->overridden.fbo_cache.insert(hash_key, new_entry);
}

RID TextureStorage::render_target_get_override_color(RID p_render_target) const {
	RenderTarget *rt = render_target_owner.get_or_null(p_render_target);
	ERR_FAIL_NULL_V(rt, RID());

	return rt->overridden.color;
}

RID TextureStorage::render_target_get_override_depth(RID p_render_target) const {
	RenderTarget *rt = render_target_owner.get_or_null(p_render_target);
	ERR_FAIL_NULL_V(rt, RID());

	return rt->overridden.depth;
}

RID TextureStorage::render_target_get_override_velocity(RID p_render_target) const {
	RenderTarget *rt = render_target_owner.get_or_null(p_render_target);
	ERR_FAIL_NULL_V(rt, RID());

	return rt->overridden.velocity;
}

void TextureStorage::render_target_set_render_region(RID p_render_target, const Rect2i &p_render_region) {
	RenderTarget *rt = render_target_owner.get_or_null(p_render_target);
	ERR_FAIL_NULL(rt);

	rt->render_region = p_render_region;
}

Rect2i TextureStorage::render_target_get_render_region(RID p_render_target) const {
	RenderTarget *rt = render_target_owner.get_or_null(p_render_target);
	ERR_FAIL_NULL_V(rt, Rect2i());

	return rt->render_region;
}

RID TextureStorage::render_target_get_texture(RID p_render_target) {
	RenderTarget *rt = render_target_owner.get_or_null(p_render_target);
	ERR_FAIL_NULL_V(rt, RID());

	if (rt->overridden.color.is_valid()) {
		return rt->overridden.color;
	}

	return rt->texture;
}

void TextureStorage::render_target_set_transparent(RID p_render_target, bool p_transparent) {
	RenderTarget *rt = render_target_owner.get_or_null(p_render_target);
	ERR_FAIL_NULL(rt);

	rt->is_transparent = p_transparent;

	if (rt->overridden.color.is_null()) {
		_clear_render_target(rt);
		_update_render_target(rt);
	}
}

bool TextureStorage::render_target_get_transparent(RID p_render_target) const {
	RenderTarget *rt = render_target_owner.get_or_null(p_render_target);
	ERR_FAIL_NULL_V(rt, false);

	return rt->is_transparent;
}

void TextureStorage::render_target_set_direct_to_screen(RID p_render_target, bool p_direct_to_screen) {
	RenderTarget *rt = render_target_owner.get_or_null(p_render_target);
	ERR_FAIL_NULL(rt);

	if (p_direct_to_screen == rt->direct_to_screen) {
		return;
	}
	// When setting DIRECT_TO_SCREEN, you need to clear before the value is set, but allocate after as
	// those functions change how they operate depending on the value of DIRECT_TO_SCREEN
	_clear_render_target(rt);
	rt->direct_to_screen = p_direct_to_screen;
	if (rt->direct_to_screen) {
		rt->overridden.color = RID();
		rt->overridden.depth = RID();
		rt->overridden.velocity = RID();
	}
	_update_render_target(rt);
}

bool TextureStorage::render_target_get_direct_to_screen(RID p_render_target) const {
	RenderTarget *rt = render_target_owner.get_or_null(p_render_target);
	ERR_FAIL_NULL_V(rt, false);

	return rt->direct_to_screen;
}

bool TextureStorage::render_target_was_used(RID p_render_target) const {
	RenderTarget *rt = render_target_owner.get_or_null(p_render_target);
	ERR_FAIL_NULL_V(rt, false);

	return rt->used_in_frame;
}

void TextureStorage::render_target_clear_used(RID p_render_target) {
	RenderTarget *rt = render_target_owner.get_or_null(p_render_target);
	ERR_FAIL_NULL(rt);

	rt->used_in_frame = false;
}

void TextureStorage::render_target_set_msaa(RID p_render_target, RS::ViewportMSAA p_msaa) {
	RenderTarget *rt = render_target_owner.get_or_null(p_render_target);
	ERR_FAIL_NULL(rt);
	ERR_FAIL_COND(rt->direct_to_screen);
	if (p_msaa == rt->msaa) {
		return;
	}

	WARN_PRINT("2D MSAA is not yet supported for GLES3.");

	_clear_render_target(rt);
	rt->msaa = p_msaa;
	_update_render_target(rt);
}

RS::ViewportMSAA TextureStorage::render_target_get_msaa(RID p_render_target) const {
	RenderTarget *rt = render_target_owner.get_or_null(p_render_target);
	ERR_FAIL_NULL_V(rt, RS::VIEWPORT_MSAA_DISABLED);

	return rt->msaa;
}

void TextureStorage::render_target_set_use_hdr(RID p_render_target, bool p_use_hdr_2d) {
	RenderTarget *rt = render_target_owner.get_or_null(p_render_target);
	ERR_FAIL_NULL(rt);
	ERR_FAIL_COND(rt->direct_to_screen);
	if (p_use_hdr_2d == rt->hdr) {
		return;
	}

	_clear_render_target(rt);
	rt->hdr = p_use_hdr_2d;
	_update_render_target(rt);
}

bool TextureStorage::render_target_is_using_hdr(RID p_render_target) const {
	RenderTarget *rt = render_target_owner.get_or_null(p_render_target);
	ERR_FAIL_NULL_V(rt, false);

	return rt->hdr;
}

GLuint TextureStorage::render_target_get_color_internal_format(RID p_render_target) const {
	RenderTarget *rt = render_target_owner.get_or_null(p_render_target);
	ERR_FAIL_NULL_V(rt, GL_RGBA8);

	return rt->color_internal_format;
}

GLuint TextureStorage::render_target_get_color_format(RID p_render_target) const {
	RenderTarget *rt = render_target_owner.get_or_null(p_render_target);
	ERR_FAIL_NULL_V(rt, GL_RGBA);

	return rt->color_format;
}

GLuint TextureStorage::render_target_get_color_type(RID p_render_target) const {
	RenderTarget *rt = render_target_owner.get_or_null(p_render_target);
	ERR_FAIL_NULL_V(rt, GL_UNSIGNED_BYTE);

	return rt->color_type;
}

uint32_t TextureStorage::render_target_get_color_format_size(RID p_render_target) const {
	RenderTarget *rt = render_target_owner.get_or_null(p_render_target);
	ERR_FAIL_NULL_V(rt, 4);

	return rt->color_format_size;
}

void TextureStorage::render_target_request_clear(RID p_render_target, const Color &p_clear_color) {
	RenderTarget *rt = render_target_owner.get_or_null(p_render_target);
	ERR_FAIL_NULL(rt);
	rt->clear_requested = true;
	rt->clear_color = p_clear_color;
}

bool TextureStorage::render_target_is_clear_requested(RID p_render_target) {
	RenderTarget *rt = render_target_owner.get_or_null(p_render_target);
	ERR_FAIL_NULL_V(rt, false);
	return rt->clear_requested;
}
Color TextureStorage::render_target_get_clear_request_color(RID p_render_target) {
	RenderTarget *rt = render_target_owner.get_or_null(p_render_target);
	ERR_FAIL_NULL_V(rt, Color());
	return rt->clear_color;
}

void TextureStorage::render_target_disable_clear_request(RID p_render_target) {
	RenderTarget *rt = render_target_owner.get_or_null(p_render_target);
	ERR_FAIL_NULL(rt);
	rt->clear_requested = false;
}

void TextureStorage::render_target_do_clear_request(RID p_render_target) {
	RenderTarget *rt = render_target_owner.get_or_null(p_render_target);
	ERR_FAIL_NULL(rt);
	if (!rt->clear_requested) {
		return;
	}
	glBindFramebuffer(GL_FRAMEBUFFER, rt->fbo);

	glClearBufferfv(GL_COLOR, 0, rt->clear_color.components);
	rt->clear_requested = false;
	glBindFramebuffer(GL_FRAMEBUFFER, system_fbo);
}

GLuint TextureStorage::render_target_get_fbo(RID p_render_target) const {
	RenderTarget *rt = render_target_owner.get_or_null(p_render_target);
	ERR_FAIL_NULL_V(rt, 0);

	return rt->fbo;
}

GLuint TextureStorage::render_target_get_color(RID p_render_target) const {
	RenderTarget *rt = render_target_owner.get_or_null(p_render_target);
	ERR_FAIL_NULL_V(rt, 0);

	if (rt->overridden.color.is_valid()) {
		Texture *texture = get_texture(rt->overridden.color);
		ERR_FAIL_NULL_V(texture, 0);

		return texture->tex_id;
	} else {
		return rt->color;
	}
}

GLuint TextureStorage::render_target_get_depth(RID p_render_target) const {
	RenderTarget *rt = render_target_owner.get_or_null(p_render_target);
	ERR_FAIL_NULL_V(rt, 0);

	if (rt->overridden.depth.is_valid()) {
		Texture *texture = get_texture(rt->overridden.depth);
		ERR_FAIL_NULL_V(texture, 0);

		return texture->tex_id;
	} else {
		return rt->depth;
	}
}

void TextureStorage::render_target_set_reattach_textures(RID p_render_target, bool p_reattach_textures) const {
	RenderTarget *rt = render_target_owner.get_or_null(p_render_target);
	ERR_FAIL_NULL(rt);

	rt->reattach_textures = p_reattach_textures;
}

bool TextureStorage::render_target_is_reattach_textures(RID p_render_target) const {
	RenderTarget *rt = render_target_owner.get_or_null(p_render_target);
	ERR_FAIL_NULL_V(rt, false);

	return rt->reattach_textures;
}

void TextureStorage::render_target_set_sdf_size_and_scale(RID p_render_target, RS::ViewportSDFOversize p_size, RS::ViewportSDFScale p_scale) {
	RenderTarget *rt = render_target_owner.get_or_null(p_render_target);
	ERR_FAIL_NULL(rt);
	if (rt->sdf_oversize == p_size && rt->sdf_scale == p_scale) {
		return;
	}

	rt->sdf_oversize = p_size;
	rt->sdf_scale = p_scale;

	_render_target_clear_sdf(rt);
}

Rect2i TextureStorage::_render_target_get_sdf_rect(const RenderTarget *rt) const {
	Size2i margin;
	int scale;
	switch (rt->sdf_oversize) {
		case RS::VIEWPORT_SDF_OVERSIZE_100_PERCENT: {
			scale = 100;
		} break;
		case RS::VIEWPORT_SDF_OVERSIZE_120_PERCENT: {
			scale = 120;
		} break;
		case RS::VIEWPORT_SDF_OVERSIZE_150_PERCENT: {
			scale = 150;
		} break;
		case RS::VIEWPORT_SDF_OVERSIZE_200_PERCENT: {
			scale = 200;
		} break;
		default: {
			ERR_PRINT("Invalid viewport SDF oversize, defaulting to 100%.");
			scale = 100;
		} break;
	}

	margin = (rt->size * scale / 100) - rt->size;

	Rect2i r(Vector2i(), rt->size);
	r.position -= margin;
	r.size += margin * 2;

	return r;
}

Rect2i TextureStorage::render_target_get_sdf_rect(RID p_render_target) const {
	const RenderTarget *rt = render_target_owner.get_or_null(p_render_target);
	ERR_FAIL_NULL_V(rt, Rect2i());

	return _render_target_get_sdf_rect(rt);
}

void TextureStorage::render_target_mark_sdf_enabled(RID p_render_target, bool p_enabled) {
	RenderTarget *rt = render_target_owner.get_or_null(p_render_target);
	ERR_FAIL_NULL(rt);

	rt->sdf_enabled = p_enabled;
}

bool TextureStorage::render_target_is_sdf_enabled(RID p_render_target) const {
	const RenderTarget *rt = render_target_owner.get_or_null(p_render_target);
	ERR_FAIL_NULL_V(rt, false);

	return rt->sdf_enabled;
}

GLuint TextureStorage::render_target_get_sdf_texture(RID p_render_target) {
	RenderTarget *rt = render_target_owner.get_or_null(p_render_target);
	ERR_FAIL_NULL_V(rt, 0);
	if (rt->sdf_texture_read == 0) {
		Texture *texture = texture_owner.get_or_null(default_gl_textures[DEFAULT_GL_TEXTURE_BLACK]);
		return texture->tex_id;
	}

	return rt->sdf_texture_read;
}

void TextureStorage::_render_target_allocate_sdf(RenderTarget *rt) {
	ERR_FAIL_COND(rt->sdf_texture_write_fb != 0);

	Size2i size = _render_target_get_sdf_rect(rt).size;

	glGenTextures(1, &rt->sdf_texture_write);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, rt->sdf_texture_write);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, size.width, size.height, 0, GL_RED, GL_UNSIGNED_BYTE, nullptr);
	GLES3::Utilities::get_singleton()->texture_allocated_data(rt->sdf_texture_write, size.width * size.height, "SDF texture");
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 1);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	glGenFramebuffers(1, &rt->sdf_texture_write_fb);
	glBindFramebuffer(GL_FRAMEBUFFER, rt->sdf_texture_write_fb);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, rt->sdf_texture_write, 0);

	int scale;
	switch (rt->sdf_scale) {
		case RS::VIEWPORT_SDF_SCALE_100_PERCENT: {
			scale = 100;
		} break;
		case RS::VIEWPORT_SDF_SCALE_50_PERCENT: {
			scale = 50;
		} break;
		case RS::VIEWPORT_SDF_SCALE_25_PERCENT: {
			scale = 25;
		} break;
		default: {
			ERR_PRINT("Invalid viewport SDF scale, defaulting to 100%.");
			scale = 100;
		} break;
	}

	rt->process_size = size * scale / 100;
	rt->process_size = rt->process_size.maxi(1);

	glGenTextures(2, rt->sdf_texture_process);
	glBindTexture(GL_TEXTURE_2D, rt->sdf_texture_process[0]);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RG16I, rt->process_size.width, rt->process_size.height, 0, GL_RG_INTEGER, GL_SHORT, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 1);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	GLES3::Utilities::get_singleton()->texture_allocated_data(rt->sdf_texture_process[0], rt->process_size.width * rt->process_size.height * 4, "SDF process texture[0]");

	glBindTexture(GL_TEXTURE_2D, rt->sdf_texture_process[1]);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RG16I, rt->process_size.width, rt->process_size.height, 0, GL_RG_INTEGER, GL_SHORT, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 1);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	GLES3::Utilities::get_singleton()->texture_allocated_data(rt->sdf_texture_process[1], rt->process_size.width * rt->process_size.height * 4, "SDF process texture[1]");

	glGenTextures(1, &rt->sdf_texture_read);
	glBindTexture(GL_TEXTURE_2D, rt->sdf_texture_read);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, rt->process_size.width, rt->process_size.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 1);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	GLES3::Utilities::get_singleton()->texture_allocated_data(rt->sdf_texture_read, rt->process_size.width * rt->process_size.height * 4, "SDF texture (read)");
}

void TextureStorage::_render_target_clear_sdf(RenderTarget *rt) {
	if (rt->sdf_texture_write_fb != 0) {
		GLES3::Utilities::get_singleton()->texture_free_data(rt->sdf_texture_read);
		GLES3::Utilities::get_singleton()->texture_free_data(rt->sdf_texture_write);
		GLES3::Utilities::get_singleton()->texture_free_data(rt->sdf_texture_process[0]);
		GLES3::Utilities::get_singleton()->texture_free_data(rt->sdf_texture_process[1]);

		glDeleteFramebuffers(1, &rt->sdf_texture_write_fb);
		rt->sdf_texture_read = 0;
		rt->sdf_texture_write = 0;
		rt->sdf_texture_process[0] = 0;
		rt->sdf_texture_process[1] = 0;
		rt->sdf_texture_write_fb = 0;
	}
}

GLuint TextureStorage::render_target_get_sdf_framebuffer(RID p_render_target) {
	RenderTarget *rt = render_target_owner.get_or_null(p_render_target);
	ERR_FAIL_NULL_V(rt, 0);

	if (rt->sdf_texture_write_fb == 0) {
		_render_target_allocate_sdf(rt);
	}

	return rt->sdf_texture_write_fb;
}
void TextureStorage::render_target_sdf_process(RID p_render_target) {
	CopyEffects *copy_effects = CopyEffects::get_singleton();

	RenderTarget *rt = render_target_owner.get_or_null(p_render_target);
	ERR_FAIL_NULL(rt);
	ERR_FAIL_COND(rt->sdf_texture_write_fb == 0);

	Rect2i r = _render_target_get_sdf_rect(rt);

	Size2i size = r.size;
	int32_t shift = 0;

	bool shrink = false;

	switch (rt->sdf_scale) {
		case RS::VIEWPORT_SDF_SCALE_50_PERCENT: {
			size[0] >>= 1;
			size[1] >>= 1;
			shift = 1;
			shrink = true;
		} break;
		case RS::VIEWPORT_SDF_SCALE_25_PERCENT: {
			size[0] >>= 2;
			size[1] >>= 2;
			shift = 2;
			shrink = true;
		} break;
		default: {
		};
	}

	GLuint temp_fb;
	glGenFramebuffers(1, &temp_fb);
	glBindFramebuffer(GL_FRAMEBUFFER, temp_fb);

	// Load
	CanvasSdfShaderGLES3::ShaderVariant variant = shrink ? CanvasSdfShaderGLES3::MODE_LOAD_SHRINK : CanvasSdfShaderGLES3::MODE_LOAD;
	bool success = sdf_shader.shader.version_bind_shader(sdf_shader.shader_version, variant);
	if (!success) {
		return;
	}

	sdf_shader.shader.version_set_uniform(CanvasSdfShaderGLES3::BASE_SIZE, r.size, sdf_shader.shader_version, variant);
	sdf_shader.shader.version_set_uniform(CanvasSdfShaderGLES3::SIZE, size, sdf_shader.shader_version, variant);
	sdf_shader.shader.version_set_uniform(CanvasSdfShaderGLES3::STRIDE, 0, sdf_shader.shader_version, variant);
	sdf_shader.shader.version_set_uniform(CanvasSdfShaderGLES3::SHIFT, shift, sdf_shader.shader_version, variant);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, rt->sdf_texture_write);

	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, rt->sdf_texture_process[0], 0);
	glViewport(0, 0, size.width, size.height);
	glEnable(GL_SCISSOR_TEST);
	glScissor(0, 0, size.width, size.height);

	copy_effects->draw_screen_triangle();

	// Process

	int stride = nearest_power_of_2_templated(MAX(size.width, size.height) / 2);

	variant = CanvasSdfShaderGLES3::MODE_PROCESS;
	success = sdf_shader.shader.version_bind_shader(sdf_shader.shader_version, variant);
	if (!success) {
		return;
	}

	sdf_shader.shader.version_set_uniform(CanvasSdfShaderGLES3::BASE_SIZE, r.size, sdf_shader.shader_version, variant);
	sdf_shader.shader.version_set_uniform(CanvasSdfShaderGLES3::SIZE, size, sdf_shader.shader_version, variant);
	sdf_shader.shader.version_set_uniform(CanvasSdfShaderGLES3::STRIDE, stride, sdf_shader.shader_version, variant);
	sdf_shader.shader.version_set_uniform(CanvasSdfShaderGLES3::SHIFT, shift, sdf_shader.shader_version, variant);

	bool swap = false;

	//jumpflood
	while (stride > 0) {
		glBindTexture(GL_TEXTURE_2D, 0);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, rt->sdf_texture_process[swap ? 0 : 1], 0);
		glBindTexture(GL_TEXTURE_2D, rt->sdf_texture_process[swap ? 1 : 0]);

		sdf_shader.shader.version_set_uniform(CanvasSdfShaderGLES3::STRIDE, stride, sdf_shader.shader_version, variant);

		copy_effects->draw_screen_triangle();

		stride /= 2;
		swap = !swap;
	}

	// Store
	variant = shrink ? CanvasSdfShaderGLES3::MODE_STORE_SHRINK : CanvasSdfShaderGLES3::MODE_STORE;
	success = sdf_shader.shader.version_bind_shader(sdf_shader.shader_version, variant);
	if (!success) {
		return;
	}

	sdf_shader.shader.version_set_uniform(CanvasSdfShaderGLES3::BASE_SIZE, r.size, sdf_shader.shader_version, variant);
	sdf_shader.shader.version_set_uniform(CanvasSdfShaderGLES3::SIZE, size, sdf_shader.shader_version, variant);
	sdf_shader.shader.version_set_uniform(CanvasSdfShaderGLES3::STRIDE, stride, sdf_shader.shader_version, variant);
	sdf_shader.shader.version_set_uniform(CanvasSdfShaderGLES3::SHIFT, shift, sdf_shader.shader_version, variant);

	glBindTexture(GL_TEXTURE_2D, 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, rt->sdf_texture_read, 0);
	glBindTexture(GL_TEXTURE_2D, rt->sdf_texture_process[swap ? 1 : 0]);

	copy_effects->draw_screen_triangle();

	glBindTexture(GL_TEXTURE_2D, 0);
	glBindFramebuffer(GL_FRAMEBUFFER, system_fbo);
	glDeleteFramebuffers(1, &temp_fb);
	glDisable(GL_SCISSOR_TEST);
}

void TextureStorage::render_target_copy_to_back_buffer(RID p_render_target, const Rect2i &p_region, bool p_gen_mipmaps) {
	RenderTarget *rt = render_target_owner.get_or_null(p_render_target);
	ERR_FAIL_NULL(rt);
	ERR_FAIL_COND(rt->direct_to_screen);

	if (rt->backbuffer_fbo == 0) {
		_create_render_target_backbuffer(rt);
	}

	Rect2i region;
	if (p_region == Rect2i()) {
		region.size = rt->size;
	} else {
		region = Rect2i(Size2i(), rt->size).intersection(p_region);
		if (region.size == Size2i()) {
			return; //nothing to do
		}
	}

	glDisable(GL_BLEND);
	// Single texture copy for backbuffer.
	glBindFramebuffer(GL_FRAMEBUFFER, rt->backbuffer_fbo);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, rt->color);
	Rect2 normalized_region = region;
	normalized_region.position = normalized_region.position / Size2(rt->size);
	normalized_region.size = normalized_region.size / Size2(rt->size);
	GLES3::CopyEffects::get_singleton()->copy_to_and_from_rect(normalized_region);

	if (p_gen_mipmaps) {
		GLES3::CopyEffects::get_singleton()->gaussian_blur(rt->backbuffer, rt->mipmap_count, region, rt->size);
		glBindFramebuffer(GL_FRAMEBUFFER, rt->backbuffer_fbo);
	}

	glEnable(GL_BLEND); // 2D starts with blend enabled.
}

void TextureStorage::render_target_clear_back_buffer(RID p_render_target, const Rect2i &p_region, const Color &p_color) {
	RenderTarget *rt = render_target_owner.get_or_null(p_render_target);
	ERR_FAIL_NULL(rt);
	ERR_FAIL_COND(rt->direct_to_screen);

	if (rt->backbuffer_fbo == 0) {
		_create_render_target_backbuffer(rt);
	}

	Rect2i region;
	if (p_region == Rect2i()) {
		// Just do a full screen clear;
		glBindFramebuffer(GL_FRAMEBUFFER, rt->backbuffer_fbo);
		glClearColor(p_color.r, p_color.g, p_color.b, p_color.a);
		glClear(GL_COLOR_BUFFER_BIT);
	} else {
		region = Rect2i(Size2i(), rt->size).intersection(p_region);
		if (region.size == Size2i()) {
			return; //nothing to do
		}
		glBindFramebuffer(GL_FRAMEBUFFER, rt->backbuffer_fbo);
		GLES3::CopyEffects::get_singleton()->set_color(p_color, region);
	}
}

void TextureStorage::render_target_gen_back_buffer_mipmaps(RID p_render_target, const Rect2i &p_region) {
	RenderTarget *rt = render_target_owner.get_or_null(p_render_target);
	ERR_FAIL_NULL(rt);

	if (rt->backbuffer_fbo == 0) {
		_create_render_target_backbuffer(rt);
	}

	Rect2i region;
	if (p_region == Rect2i()) {
		region.size = rt->size;
	} else {
		region = Rect2i(Size2i(), rt->size).intersection(p_region);
		if (region.size == Size2i()) {
			return; //nothing to do
		}
	}
	glDisable(GL_BLEND);
	GLES3::CopyEffects::get_singleton()->gaussian_blur(rt->backbuffer, rt->mipmap_count, region, rt->size);
	glEnable(GL_BLEND); // 2D starts with blend enabled.

	glBindFramebuffer(GL_FRAMEBUFFER, rt->backbuffer_fbo);
}

#endif // GLES3_ENABLED

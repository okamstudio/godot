/**************************************************************************/
/*  texture_storage.h                                                     */
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

#ifndef TEXTURE_STORAGE_GLES3_H
#define TEXTURE_STORAGE_GLES3_H

#ifdef GLES3_ENABLED

#include "platform_gl.h"

#include "config.h"
#include "core/io/image.h"
#include "core/os/os.h"
#include "core/templates/rid_owner.h"
#include "servers/rendering/renderer_compositor.h"
#include "servers/rendering/storage/texture_storage.h"

#include "drivers/gles3/shaders/canvas_sdf.glsl.gen.h"

namespace GLES3 {

#define _GL_TEXTURE_MAX_ANISOTROPY_EXT 0x84FE
#define _GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT 0x84FF

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

#define _EXT_COMPRESSED_RGBA_BPTC_UNORM 0x8E8C
#define _EXT_COMPRESSED_SRGB_ALPHA_BPTC_UNORM 0x8E8D
#define _EXT_COMPRESSED_RGB_BPTC_SIGNED_FLOAT 0x8E8E
#define _EXT_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT 0x8E8F

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

#define _EXT_COMPRESSED_RGBA_ASTC_4x4_KHR 0x93B0
#define _EXT_COMPRESSED_RGBA_ASTC_5x4_KHR 0x93B1
#define _EXT_COMPRESSED_RGBA_ASTC_5x5_KHR 0x93B2
#define _EXT_COMPRESSED_RGBA_ASTC_6x5_KHR 0x93B3
#define _EXT_COMPRESSED_RGBA_ASTC_6x6_KHR 0x93B4
#define _EXT_COMPRESSED_RGBA_ASTC_8x5_KHR 0x93B5
#define _EXT_COMPRESSED_RGBA_ASTC_8x6_KHR 0x93B6
#define _EXT_COMPRESSED_RGBA_ASTC_8x8_KHR 0x93B7
#define _EXT_COMPRESSED_RGBA_ASTC_10x5_KHR 0x93B8
#define _EXT_COMPRESSED_RGBA_ASTC_10x6_KHR 0x93B9
#define _EXT_COMPRESSED_RGBA_ASTC_10x8_KHR 0x93BA
#define _EXT_COMPRESSED_RGBA_ASTC_10x10_KHR 0x93BB
#define _EXT_COMPRESSED_RGBA_ASTC_12x10_KHR 0x93BC
#define _EXT_COMPRESSED_RGBA_ASTC_12x12_KHR 0x93BD

#define _EXT_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR 0x93D0
#define _EXT_COMPRESSED_SRGB8_ALPHA8_ASTC_5x4_KHR 0x93D1
#define _EXT_COMPRESSED_SRGB8_ALPHA8_ASTC_5x5_KHR 0x93D2
#define _EXT_COMPRESSED_SRGB8_ALPHA8_ASTC_6x5_KHR 0x93D3
#define _EXT_COMPRESSED_SRGB8_ALPHA8_ASTC_6x6_KHR 0x93D4
#define _EXT_COMPRESSED_SRGB8_ALPHA8_ASTC_8x5_KHR 0x93D5
#define _EXT_COMPRESSED_SRGB8_ALPHA8_ASTC_8x6_KHR 0x93D6
#define _EXT_COMPRESSED_SRGB8_ALPHA8_ASTC_8x8_KHR 0x93D7
#define _EXT_COMPRESSED_SRGB8_ALPHA8_ASTC_10x5_KHR 0x93D8
#define _EXT_COMPRESSED_SRGB8_ALPHA8_ASTC_10x6_KHR 0x93D9
#define _EXT_COMPRESSED_SRGB8_ALPHA8_ASTC_10x8_KHR 0x93DA
#define _EXT_COMPRESSED_SRGB8_ALPHA8_ASTC_10x10_KHR 0x93DB
#define _EXT_COMPRESSED_SRGB8_ALPHA8_ASTC_12x10_KHR 0x93DC
#define _EXT_COMPRESSED_SRGB8_ALPHA8_ASTC_12x12_KHR 0x93DD

#define _GL_TEXTURE_EXTERNAL_OES 0x8D65

#define _EXT_TEXTURE_CUBE_MAP_SEAMLESS 0x884F

enum DefaultGLTexture {
	DEFAULT_GL_TEXTURE_WHITE,
	DEFAULT_GL_TEXTURE_BLACK,
	DEFAULT_GL_TEXTURE_TRANSPARENT,
	DEFAULT_GL_TEXTURE_NORMAL,
	DEFAULT_GL_TEXTURE_ANISO,
	DEFAULT_GL_TEXTURE_DEPTH,
	DEFAULT_GL_TEXTURE_CUBEMAP_BLACK,
	//DEFAULT_GL_TEXTURE_CUBEMAP_ARRAY_BLACK, // Cubemap Arrays not supported in GL 3.3 or GL ES 3.0
	DEFAULT_GL_TEXTURE_CUBEMAP_WHITE,
	DEFAULT_GL_TEXTURE_3D_WHITE,
	DEFAULT_GL_TEXTURE_3D_BLACK,
	DEFAULT_GL_TEXTURE_2D_ARRAY_WHITE,
	DEFAULT_GL_TEXTURE_2D_UINT,
	DEFAULT_GL_TEXTURE_EXT,
	DEFAULT_GL_TEXTURE_MAX
};

struct CanvasTexture {
	RID diffuse;
	RID normal_map;
	RID specular;
	Color specular_color = Color(1, 1, 1, 1);
	float shininess = 1.0;

	RS::CanvasItemTextureFilter texture_filter = RS::CANVAS_ITEM_TEXTURE_FILTER_DEFAULT;
	RS::CanvasItemTextureRepeat texture_repeat = RS::CANVAS_ITEM_TEXTURE_REPEAT_DEFAULT;
};

struct RenderTarget;

struct Texture {
	RID self;

	bool is_proxy = false;
	bool is_from_native_handle = false;
	bool is_render_target = false;

	RID proxy_to;
	Vector<RID> proxies;

	String path;
	int width = 0;
	int height = 0;
	int depth = 0;
	int mipmaps = 1;
	int layers = 1;
	int alloc_width = 0;
	int alloc_height = 0;
	Image::Format format = Image::FORMAT_R8;
	Image::Format real_format = Image::FORMAT_R8;

	enum Type {
		TYPE_2D,
		TYPE_LAYERED,
		TYPE_3D
	};

	Type type = TYPE_2D;
	RS::TextureLayeredType layered_type = RS::TEXTURE_LAYERED_2D_ARRAY;

	GLenum target = GL_TEXTURE_2D;
	GLenum gl_format_cache = 0;
	GLenum gl_internal_format_cache = 0;
	GLenum gl_type_cache = 0;

	int total_data_size = 0;

	bool compressed = false;

	bool resize_to_po2 = false;

	bool active = false;
	GLuint tex_id = 0;

	uint16_t stored_cube_sides = 0;

	RenderTarget *render_target = nullptr;

	Ref<Image> image_cache_2d;
	Vector<Ref<Image>> image_cache_3d;

	bool redraw_if_visible = false;

	RS::TextureDetectCallback detect_3d_callback = nullptr;
	void *detect_3d_callback_ud = nullptr;

	RS::TextureDetectCallback detect_normal_callback = nullptr;
	void *detect_normal_callback_ud = nullptr;

	RS::TextureDetectRoughnessCallback detect_roughness_callback = nullptr;
	void *detect_roughness_callback_ud = nullptr;

	CanvasTexture *canvas_texture = nullptr;

	void copy_from(const Texture &o) {
		proxy_to = o.proxy_to;
		is_proxy = o.is_proxy;
		is_from_native_handle = o.is_from_native_handle;
		width = o.width;
		height = o.height;
		alloc_width = o.alloc_width;
		alloc_height = o.alloc_height;
		format = o.format;
		type = o.type;
		layered_type = o.layered_type;
		target = o.target;
		total_data_size = o.total_data_size;
		compressed = o.compressed;
		mipmaps = o.mipmaps;
		resize_to_po2 = o.resize_to_po2;
		active = o.active;
		tex_id = o.tex_id;
		stored_cube_sides = o.stored_cube_sides;
		render_target = o.render_target;
		is_render_target = o.is_render_target;
		redraw_if_visible = o.redraw_if_visible;
		detect_3d_callback = o.detect_3d_callback;
		detect_3d_callback_ud = o.detect_3d_callback_ud;
		detect_normal_callback = o.detect_normal_callback;
		detect_normal_callback_ud = o.detect_normal_callback_ud;
		detect_roughness_callback = o.detect_roughness_callback;
		detect_roughness_callback_ud = o.detect_roughness_callback_ud;
	}

	// texture state
	void gl_set_filter(RS::CanvasItemTextureFilter p_filter) {
		if (p_filter == state_filter) {
			return;
		}
		Config *config = Config::get_singleton();
		state_filter = p_filter;
		GLenum pmin = GL_NEAREST;
		GLenum pmag = GL_NEAREST;
		GLint max_lod = 0;
		GLfloat anisotropy = 1.0f;
		switch (state_filter) {
			case RS::CANVAS_ITEM_TEXTURE_FILTER_NEAREST: {
				pmin = GL_NEAREST;
				pmag = GL_NEAREST;
				max_lod = 0;
			} break;
			case RS::CANVAS_ITEM_TEXTURE_FILTER_LINEAR: {
				pmin = GL_LINEAR;
				pmag = GL_LINEAR;
				max_lod = 0;
			} break;
			case RS::CANVAS_ITEM_TEXTURE_FILTER_NEAREST_WITH_MIPMAPS_ANISOTROPIC: {
				anisotropy = config->anisotropic_level;
			};
				[[fallthrough]];
			case RS::CANVAS_ITEM_TEXTURE_FILTER_NEAREST_WITH_MIPMAPS: {
				pmag = GL_NEAREST;
				if (mipmaps <= 1) {
					pmin = GL_NEAREST;
					max_lod = 0;
				} else if (config->use_nearest_mip_filter) {
					pmin = GL_NEAREST_MIPMAP_NEAREST;
					max_lod = 1000;
				} else {
					pmin = GL_NEAREST_MIPMAP_LINEAR;
					max_lod = 1000;
				}
			} break;
			case RS::CANVAS_ITEM_TEXTURE_FILTER_LINEAR_WITH_MIPMAPS_ANISOTROPIC: {
				anisotropy = config->anisotropic_level;
			};
				[[fallthrough]];
			case RS::CANVAS_ITEM_TEXTURE_FILTER_LINEAR_WITH_MIPMAPS: {
				pmag = GL_LINEAR;
				if (mipmaps <= 1) {
					pmin = GL_LINEAR;
					max_lod = 0;
				} else if (config->use_nearest_mip_filter) {
					pmin = GL_LINEAR_MIPMAP_NEAREST;
					max_lod = 1000;
				} else {
					pmin = GL_LINEAR_MIPMAP_LINEAR;
					max_lod = 1000;
				}
			} break;
			default: {
				return;
			} break;
		}
		glTexParameteri(target, GL_TEXTURE_MIN_FILTER, pmin);
		glTexParameteri(target, GL_TEXTURE_MAG_FILTER, pmag);
		glTexParameteri(target, GL_TEXTURE_BASE_LEVEL, 0);
		glTexParameteri(target, GL_TEXTURE_MAX_LEVEL, max_lod);
		if (config->support_anisotropic_filter) {
			glTexParameterf(target, _GL_TEXTURE_MAX_ANISOTROPY_EXT, anisotropy);
		}
	}
	void gl_set_repeat(RS::CanvasItemTextureRepeat p_repeat) {
		if (p_repeat == state_repeat) {
			return;
		}
		state_repeat = p_repeat;
		GLenum prep = GL_CLAMP_TO_EDGE;
		switch (state_repeat) {
			case RS::CANVAS_ITEM_TEXTURE_REPEAT_DISABLED: {
				prep = GL_CLAMP_TO_EDGE;
			} break;
			case RS::CANVAS_ITEM_TEXTURE_REPEAT_ENABLED: {
				prep = GL_REPEAT;
			} break;
			case RS::CANVAS_ITEM_TEXTURE_REPEAT_MIRROR: {
				prep = GL_MIRRORED_REPEAT;
			} break;
			default: {
				return;
			} break;
		}
		glTexParameteri(target, GL_TEXTURE_WRAP_T, prep);
		glTexParameteri(target, GL_TEXTURE_WRAP_R, prep);
		glTexParameteri(target, GL_TEXTURE_WRAP_S, prep);
	}

private:
	RS::CanvasItemTextureFilter state_filter = RS::CANVAS_ITEM_TEXTURE_FILTER_MAX;
	RS::CanvasItemTextureRepeat state_repeat = RS::CANVAS_ITEM_TEXTURE_REPEAT_MAX;
};

struct RenderTarget {
	Point2i position = Point2i(0, 0);
	Size2i size = Size2i(0, 0);
	uint32_t view_count = 1;
	int mipmap_count = 1;
	RID self;
	GLuint fbo = 0;
	GLuint color = 0;
	GLuint depth = 0;
	GLuint backbuffer_fbo = 0;
	GLuint backbuffer = 0;
	GLuint backbuffer_depth = 0;

	bool hdr = false; // For Compatibility this effects both 2D and 3D rendering!
	GLuint color_internal_format = GL_RGBA8;
	GLuint color_format = GL_RGBA;
	GLuint color_type = GL_UNSIGNED_BYTE;
	uint32_t color_format_size = 4;
	Image::Format image_format = Image::FORMAT_RGBA8;

	GLuint sdf_texture_write = 0;
	GLuint sdf_texture_write_fb = 0;
	GLuint sdf_texture_process[2] = { 0, 0 };
	GLuint sdf_texture_read = 0;
	RS::ViewportSDFOversize sdf_oversize = RS::VIEWPORT_SDF_OVERSIZE_120_PERCENT;
	RS::ViewportSDFScale sdf_scale = RS::VIEWPORT_SDF_SCALE_50_PERCENT;
	Size2i process_size;
	bool sdf_enabled = false;

	bool is_transparent = false;
	bool direct_to_screen = false;

	bool used_in_frame = false;
	RS::ViewportMSAA msaa = RS::VIEWPORT_MSAA_DISABLED;
	bool reattach_textures = false;

	struct RTOverridden {
		bool is_overridden = false;
		RID color;
		RID depth;
		RID velocity;

		struct FBOCacheEntry {
			GLuint fbo;
			GLuint color;
			GLuint depth;
			Size2i size;
			Vector<GLuint> allocated_textures;
		};
		RBMap<uint32_t, FBOCacheEntry> fbo_cache;
	} overridden;

	RID texture;

	Color clear_color = Color(1, 1, 1, 1);
	bool clear_requested = false;

	RenderTarget() {
	}
};

class TextureStorage : public RendererTextureStorage {
private:
	static TextureStorage *singleton;

	RID default_gl_textures[DEFAULT_GL_TEXTURE_MAX];

	/* Canvas Texture API */

	RID_Owner<CanvasTexture, true> canvas_texture_owner;

	/* Texture API */
	// Textures can be created from threads, so this RID_Owner is thread safe.
	mutable RID_Owner<Texture, true> texture_owner;

	Ref<Image> _get_gl_image_and_format(const Ref<Image> &p_image, Image::Format p_format, Image::Format &r_real_format, GLenum &r_gl_format, GLenum &r_gl_internal_format, GLenum &r_gl_type, bool &r_compressed, bool p_force_decompress) const;

	/* TEXTURE ATLAS API */

	struct TextureAtlas {
		struct Texture {
			int users;
			Rect2 uv_rect;
		};

		struct SortItem {
			RID texture;
			Size2i pixel_size;
			Size2i size;
			Point2i pos;

			bool operator<(const SortItem &p_item) const {
				//sort larger to smaller
				if (size.height == p_item.size.height) {
					return size.width > p_item.size.width;
				} else {
					return size.height > p_item.size.height;
				}
			}
		};

		HashMap<RID, Texture> textures;
		bool dirty = true;

		GLuint texture = 0;
		GLuint framebuffer = 0;
		Size2i size;
	} texture_atlas;

	/* Render Target API */

	mutable RID_Owner<RenderTarget> render_target_owner;

	void _clear_render_target(RenderTarget *rt);
	void _update_render_target(RenderTarget *rt);
	void _create_render_target_backbuffer(RenderTarget *rt);
	void _render_target_allocate_sdf(RenderTarget *rt);
	void _render_target_clear_sdf(RenderTarget *rt);
	Rect2i _render_target_get_sdf_rect(const RenderTarget *rt) const;

	void _texture_set_data(RID p_texture, const Ref<Image> &p_image, int p_layer, bool p_initialize);
	void _texture_set_3d_data(RID p_texture, const Vector<Ref<Image>> &p_data, bool p_initialize);
	void _texture_set_swizzle(Texture *p_texture, Image::Format p_real_format);
	Vector<Ref<Image>> _texture_3d_read_framebuffer(Texture *p_texture) const;

	struct RenderTargetSDF {
		CanvasSdfShaderGLES3 shader;
		RID shader_version;
	} sdf_shader;

public:
	static TextureStorage *get_singleton();

	TextureStorage();
	virtual ~TextureStorage();

	_FORCE_INLINE_ RID texture_gl_get_default(DefaultGLTexture p_texture) {
		return default_gl_textures[p_texture];
	}

	/* Canvas Texture API */

	CanvasTexture *get_canvas_texture(RID p_rid) { return canvas_texture_owner.get_or_null(p_rid); };
	bool owns_canvas_texture(RID p_rid) { return canvas_texture_owner.owns(p_rid); };

	virtual RID canvas_texture_allocate() override;
	virtual void canvas_texture_initialize(RID p_rid) override;
	virtual void canvas_texture_free(RID p_rid) override;

	virtual void canvas_texture_set_channel(RID p_canvas_texture, RS::CanvasTextureChannel p_channel, RID p_texture) override;
	virtual void canvas_texture_set_shading_parameters(RID p_canvas_texture, const Color &p_base_color, float p_shininess) override;

	virtual void canvas_texture_set_texture_filter(RID p_item, RS::CanvasItemTextureFilter p_filter) override;
	virtual void canvas_texture_set_texture_repeat(RID p_item, RS::CanvasItemTextureRepeat p_repeat) override;

	/* Texture API */

	Texture *get_texture(RID p_rid) const {
		Texture *texture = texture_owner.get_or_null(p_rid);
		if (texture && texture->is_proxy) {
			return texture_owner.get_or_null(texture->proxy_to);
		}
		return texture;
	};
	bool owns_texture(RID p_rid) { return texture_owner.owns(p_rid); };

	void texture_2d_initialize_from_texture(RID p_texture, Texture &p_tex) {
		texture_owner.initialize_rid(p_texture, p_tex);
	}

	virtual RID texture_allocate() override;
	virtual void texture_free(RID p_rid) override;

	virtual void texture_2d_initialize(RID p_texture, const Ref<Image> &p_image) override;
	virtual void texture_2d_layered_initialize(RID p_texture, const Vector<Ref<Image>> &p_layers, RS::TextureLayeredType p_layered_type) override;
	virtual void texture_3d_initialize(RID p_texture, Image::Format, int p_width, int p_height, int p_depth, bool p_mipmaps, const Vector<Ref<Image>> &p_data) override;
	virtual void texture_external_initialize(RID p_texture, int p_width, int p_height, uint64_t p_external_buffer) override;
	virtual void texture_proxy_initialize(RID p_texture, RID p_base) override; //all slices, then all the mipmaps, must be coherent

	virtual RID texture_create_from_native_handle(RS::TextureType p_type, Image::Format p_format, uint64_t p_native_handle, int p_width, int p_height, int p_depth, int p_layers = 1, RS::TextureLayeredType p_layered_type = RS::TEXTURE_LAYERED_2D_ARRAY) override;

	virtual void texture_2d_update(RID p_texture, const Ref<Image> &p_image, int p_layer = 0) override;
	virtual void texture_3d_update(RID p_texture, const Vector<Ref<Image>> &p_data) override;
	virtual void texture_external_update(RID p_texture, int p_width, int p_height, uint64_t p_external_buffer) override;
	virtual void texture_proxy_update(RID p_proxy, RID p_base) override;

	Ref<Image> texture_2d_placeholder;
	Vector<Ref<Image>> texture_2d_array_placeholder;
	Vector<Ref<Image>> cubemap_placeholder;
	Vector<Ref<Image>> texture_3d_placeholder;

	//these two APIs can be used together or in combination with the others.
	virtual void texture_2d_placeholder_initialize(RID p_texture) override;
	virtual void texture_2d_layered_placeholder_initialize(RID p_texture, RenderingServer::TextureLayeredType p_layered_type) override;
	virtual void texture_3d_placeholder_initialize(RID p_texture) override;

	virtual Ref<Image> texture_2d_get(RID p_texture) const override;
	virtual Ref<Image> texture_2d_layer_get(RID p_texture, int p_layer) const override;
	virtual Vector<Ref<Image>> texture_3d_get(RID p_texture) const override;

	virtual void texture_replace(RID p_texture, RID p_by_texture) override;
	virtual void texture_set_size_override(RID p_texture, int p_width, int p_height) override;

	virtual void texture_set_path(RID p_texture, const String &p_path) override;
	virtual String texture_get_path(RID p_texture) const override;

	virtual void texture_set_detect_3d_callback(RID p_texture, RS::TextureDetectCallback p_callback, void *p_userdata) override;
	void texture_set_detect_srgb_callback(RID p_texture, RS::TextureDetectCallback p_callback, void *p_userdata);
	virtual void texture_set_detect_normal_callback(RID p_texture, RS::TextureDetectCallback p_callback, void *p_userdata) override;
	virtual void texture_set_detect_roughness_callback(RID p_texture, RS::TextureDetectRoughnessCallback p_callback, void *p_userdata) override;

	virtual void texture_debug_usage(List<RS::TextureInfo> *r_info) override;

	virtual void texture_set_force_redraw_if_visible(RID p_texture, bool p_enable) override;

	virtual Size2 texture_size_with_proxy(RID p_proxy) override;

	virtual void texture_rd_initialize(RID p_texture, const RID &p_rd_texture, const RS::TextureLayeredType p_layer_type = RS::TEXTURE_LAYERED_2D_ARRAY) override;
	virtual RID texture_get_rd_texture(RID p_texture, bool p_srgb = false) const override;
	virtual uint64_t texture_get_native_handle(RID p_texture, bool p_srgb = false) const override;

	void texture_set_data(RID p_texture, const Ref<Image> &p_image, int p_layer = 0);
	virtual Image::Format texture_get_format(RID p_texture) const override;
	uint32_t texture_get_texid(RID p_texture) const;
	Vector3i texture_get_size(RID p_texture) const;
	uint32_t texture_get_width(RID p_texture) const;
	uint32_t texture_get_height(RID p_texture) const;
	uint32_t texture_get_depth(RID p_texture) const;
	void texture_bind(RID p_texture, uint32_t p_texture_no);

	/* TEXTURE ATLAS API */

	void update_texture_atlas();

	GLuint texture_atlas_get_texture() const;
	_FORCE_INLINE_ Rect2 texture_atlas_get_texture_rect(RID p_texture) {
		TextureAtlas::Texture *t = texture_atlas.textures.getptr(p_texture);
		if (!t) {
			return Rect2();
		}

		return t->uv_rect;
	}

	void texture_add_to_texture_atlas(RID p_texture);
	void texture_remove_from_texture_atlas(RID p_texture);
	void texture_atlas_mark_dirty_on_texture(RID p_texture);
	void texture_atlas_remove_texture(RID p_texture);

	/* DECAL API */

	virtual RID decal_allocate() override;
	virtual void decal_initialize(RID p_rid) override;
	virtual void decal_free(RID p_rid) override {}

	virtual void decal_set_size(RID p_decal, const Vector3 &p_size) override;
	virtual void decal_set_texture(RID p_decal, RS::DecalTexture p_type, RID p_texture) override;
	virtual void decal_set_emission_energy(RID p_decal, float p_energy) override;
	virtual void decal_set_albedo_mix(RID p_decal, float p_mix) override;
	virtual void decal_set_modulate(RID p_decal, const Color &p_modulate) override;
	virtual void decal_set_cull_mask(RID p_decal, uint32_t p_layers) override;
	virtual void decal_set_distance_fade(RID p_decal, bool p_enabled, float p_begin, float p_length) override;
	virtual void decal_set_fade(RID p_decal, float p_above, float p_below) override;
	virtual void decal_set_normal_fade(RID p_decal, float p_fade) override;

	virtual AABB decal_get_aabb(RID p_decal) const override;
	virtual uint32_t decal_get_cull_mask(RID p_decal) const override { return 0; }

	virtual void texture_add_to_decal_atlas(RID p_texture, bool p_panorama_to_dp = false) override {}
	virtual void texture_remove_from_decal_atlas(RID p_texture, bool p_panorama_to_dp = false) override {}

	/* DECAL INSTANCE */

	virtual RID decal_instance_create(RID p_decal) override { return RID(); }
	virtual void decal_instance_free(RID p_decal_instance) override {}
	virtual void decal_instance_set_transform(RID p_decal, const Transform3D &p_transform) override {}
	virtual void decal_instance_set_sorting_offset(RID p_decal_instance, float p_sorting_offset) override {}

	/* RENDER TARGET API */

	static GLuint system_fbo;

	RenderTarget *get_render_target(RID p_rid) { return render_target_owner.get_or_null(p_rid); };
	bool owns_render_target(RID p_rid) { return render_target_owner.owns(p_rid); };

	void check_backbuffer(RenderTarget *rt, const bool uses_screen_texture, const bool uses_depth_texture);

	virtual RID render_target_create() override;
	virtual void render_target_free(RID p_rid) override;

	virtual void render_target_set_position(RID p_render_target, int p_x, int p_y) override;
	virtual Point2i render_target_get_position(RID p_render_target) const override;
	virtual void render_target_set_size(RID p_render_target, int p_width, int p_height, uint32_t p_view_count) override;
	virtual Size2i render_target_get_size(RID p_render_target) const override;
	virtual void render_target_set_transparent(RID p_render_target, bool p_is_transparent) override;
	virtual bool render_target_get_transparent(RID p_render_target) const override;
	virtual void render_target_set_direct_to_screen(RID p_render_target, bool p_direct_to_screen) override;
	virtual bool render_target_get_direct_to_screen(RID p_render_target) const override;
	virtual bool render_target_was_used(RID p_render_target) const override;
	void render_target_clear_used(RID p_render_target);
	virtual void render_target_set_msaa(RID p_render_target, RS::ViewportMSAA p_msaa) override;
	virtual RS::ViewportMSAA render_target_get_msaa(RID p_render_target) const override;
	virtual void render_target_set_msaa_needs_resolve(RID p_render_target, bool p_needs_resolve) override {}
	virtual bool render_target_get_msaa_needs_resolve(RID p_render_target) const override { return false; }
	virtual void render_target_do_msaa_resolve(RID p_render_target) override {}
	virtual void render_target_set_use_hdr(RID p_render_target, bool p_use_hdr_2d) override;
	virtual bool render_target_is_using_hdr(RID p_render_target) const override;

	// new
	void render_target_set_as_unused(RID p_render_target) override {
		render_target_clear_used(p_render_target);
	}

	GLuint render_target_get_color_internal_format(RID p_render_target) const;
	GLuint render_target_get_color_format(RID p_render_target) const;
	GLuint render_target_get_color_type(RID p_render_target) const;
	uint32_t render_target_get_color_format_size(RID p_render_target) const;

	void render_target_request_clear(RID p_render_target, const Color &p_clear_color) override;
	bool render_target_is_clear_requested(RID p_render_target) override;
	Color render_target_get_clear_request_color(RID p_render_target) override;
	void render_target_disable_clear_request(RID p_render_target) override;
	void render_target_do_clear_request(RID p_render_target) override;

	GLuint render_target_get_fbo(RID p_render_target) const;
	GLuint render_target_get_color(RID p_render_target) const;
	GLuint render_target_get_depth(RID p_render_target) const;
	void render_target_set_reattach_textures(RID p_render_target, bool p_reattach_textures) const;
	bool render_target_is_reattach_textures(RID p_render_target) const;

	virtual void render_target_set_sdf_size_and_scale(RID p_render_target, RS::ViewportSDFOversize p_size, RS::ViewportSDFScale p_scale) override;
	virtual Rect2i render_target_get_sdf_rect(RID p_render_target) const override;
	GLuint render_target_get_sdf_texture(RID p_render_target);
	GLuint render_target_get_sdf_framebuffer(RID p_render_target);
	void render_target_sdf_process(RID p_render_target);
	virtual void render_target_mark_sdf_enabled(RID p_render_target, bool p_enabled) override;
	bool render_target_is_sdf_enabled(RID p_render_target) const;

	void render_target_copy_to_back_buffer(RID p_render_target, const Rect2i &p_region, bool p_gen_mipmaps);
	void render_target_clear_back_buffer(RID p_render_target, const Rect2i &p_region, const Color &p_color);
	void render_target_gen_back_buffer_mipmaps(RID p_render_target, const Rect2i &p_region);

	virtual void render_target_set_vrs_mode(RID p_render_target, RS::ViewportVRSMode p_mode) override {}
	virtual RS::ViewportVRSMode render_target_get_vrs_mode(RID p_render_target) const override { return RS::VIEWPORT_VRS_DISABLED; }
	virtual void render_target_set_vrs_update_mode(RID p_render_target, RS::ViewportVRSUpdateMode p_mode) override {}
	virtual RS::ViewportVRSUpdateMode render_target_get_vrs_update_mode(RID p_render_target) const override { return RS::VIEWPORT_VRS_UPDATE_DISABLED; }
	virtual void render_target_set_vrs_texture(RID p_render_target, RID p_texture) override {}
	virtual RID render_target_get_vrs_texture(RID p_render_target) const override { return RID(); }

	virtual void render_target_set_override(RID p_render_target, RID p_color_texture, RID p_depth_texture, RID p_velocity_texture) override;
	virtual RID render_target_get_override_color(RID p_render_target) const override;
	virtual RID render_target_get_override_depth(RID p_render_target) const override;
	virtual RID render_target_get_override_velocity(RID p_render_target) const override;

	virtual RID render_target_get_texture(RID p_render_target) override;

	void bind_framebuffer(GLuint framebuffer) {
		glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
	}

	void bind_framebuffer_system() {
		glBindFramebuffer(GL_FRAMEBUFFER, GLES3::TextureStorage::system_fbo);
	}

	String get_framebuffer_error(GLenum p_status);
};

inline String TextureStorage::get_framebuffer_error(GLenum p_status) {
#if defined(DEBUG_ENABLED) && defined(GL_API_ENABLED)
	if (p_status == GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT) {
		return "GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT";
	} else if (p_status == GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT) {
		return "GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT";
	} else if (p_status == GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER) {
		return "GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER";
	} else if (p_status == GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER) {
		return "GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER";
	}
#endif
	return itos(p_status);
}

} // namespace GLES3

#endif // GLES3_ENABLED

#endif // TEXTURE_STORAGE_GLES3_H

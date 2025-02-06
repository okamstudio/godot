/**************************************************************************/
/*  compressed_texture.h                                                  */
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

#pragma once

#include "core/io/resource_loader.h"
#include "scene/resources/texture.h"

class BitMap;

class CompressedTexture2D : public Texture2D {
	GDCLASS(CompressedTexture2D, Texture2D);

public:
	enum DataFormat {
		DATA_FORMAT_IMAGE,
		DATA_FORMAT_PNG,
		DATA_FORMAT_WEBP,
		DATA_FORMAT_BASIS_UNIVERSAL,
	};

	enum {
		FORMAT_VERSION = 1
	};

	enum FormatBits {
		FORMAT_BIT_STREAM = 1 << 22,
		FORMAT_BIT_HAS_MIPMAPS = 1 << 23,
		FORMAT_BIT_DETECT_3D = 1 << 24,
		//FORMAT_BIT_DETECT_SRGB = 1 << 25,
		FORMAT_BIT_DETECT_NORMAL = 1 << 26,
		FORMAT_BIT_DETECT_ROUGNESS = 1 << 27,
	};

private:
	String path_to_file;
	mutable RID texture;
	Image::Format format = Image::FORMAT_L8;
	int w = 0;
	int h = 0;
	mutable Ref<BitMap> alpha_cache;

	Error _load_data(const String &p_path, int &r_width, int &r_height, Ref<Image> &image, bool &r_request_3d, bool &r_request_normal, bool &r_request_roughness, int &mipmap_limit, int p_size_limit = 0);
	virtual void reload_from_file() override;

	static void _requested_3d(void *p_ud);
	static void _requested_roughness(void *p_ud, const String &p_normal_path, RS::TextureDetectRoughnessChannel p_roughness_channel);
	static void _requested_normal(void *p_ud);

protected:
	static void _bind_methods();
	void _validate_property(PropertyInfo &p_property) const;

public:
	static Ref<Image> load_image_from_file(Ref<FileAccess> p_file, int p_size_limit);

	typedef void (*TextureFormatRequestCallback)(const Ref<CompressedTexture2D> &);
	typedef void (*TextureFormatRoughnessRequestCallback)(const Ref<CompressedTexture2D> &, const String &p_normal_path, RS::TextureDetectRoughnessChannel p_roughness_channel);

	static TextureFormatRequestCallback request_3d_callback;
	static TextureFormatRoughnessRequestCallback request_roughness_callback;
	static TextureFormatRequestCallback request_normal_callback;

	Image::Format get_format() const;
	Error load(const String &p_path);
	String get_load_path() const;

	int get_width() const override;
	int get_height() const override;
	virtual RID get_rid() const override;

	virtual void set_path(const String &p_path, bool p_take_over) override;

	virtual void draw(RID p_canvas_item, const Point2 &p_pos, const Color &p_modulate = Color(1, 1, 1), bool p_transpose = false) const override;
	virtual void draw_rect(RID p_canvas_item, const Rect2 &p_rect, bool p_tile = false, const Color &p_modulate = Color(1, 1, 1), bool p_transpose = false) const override;
	virtual void draw_rect_region(RID p_canvas_item, const Rect2 &p_rect, const Rect2 &p_src_rect, const Color &p_modulate = Color(1, 1, 1), bool p_transpose = false, bool p_clip_uv = true) const override;

	virtual bool has_alpha() const override;
	bool is_pixel_opaque(int p_x, int p_y) const override;

	virtual Ref<Image> get_image() const override;

	CompressedTexture2D();
	~CompressedTexture2D();
};

class ResourceFormatLoaderCompressedTexture2D : public ResourceFormatLoader {
public:
	virtual Ref<Resource> load(const String &p_path, const String &p_original_path = "", Error *r_error = nullptr, bool p_use_sub_threads = false, float *r_progress = nullptr, CacheMode p_cache_mode = CACHE_MODE_REUSE) override;
	virtual void get_recognized_extensions(List<String> *p_extensions) const override;
	virtual bool handles_type(const String &p_type) const override;
	virtual String get_resource_type(const String &p_path) const override;
};

class CompressedTextureLayered : public TextureLayered {
	GDCLASS(CompressedTextureLayered, TextureLayered);

public:
	enum DataFormat {
		DATA_FORMAT_IMAGE,
		DATA_FORMAT_PNG,
		DATA_FORMAT_WEBP,
		DATA_FORMAT_BASIS_UNIVERSAL,
	};

	enum {
		FORMAT_VERSION = 1
	};

	enum FormatBits {
		FORMAT_BIT_STREAM = 1 << 22,
		FORMAT_BIT_HAS_MIPMAPS = 1 << 23,
	};

private:
	Error _load_data(const String &p_path, Vector<Ref<Image>> &images, int &mipmap_limit, int p_size_limit = 0);
	String path_to_file;
	mutable RID texture;
	Image::Format format = Image::FORMAT_L8;
	int w = 0;
	int h = 0;
	int layers = 0;
	bool mipmaps = false;
	LayeredType layered_type = LayeredType::LAYERED_TYPE_2D_ARRAY;

	virtual void reload_from_file() override;

protected:
	static void _bind_methods();
	void _validate_property(PropertyInfo &p_property) const;

public:
	Image::Format get_format() const override;
	Error load(const String &p_path);
	String get_load_path() const;
	virtual LayeredType get_layered_type() const override;

	int get_width() const override;
	int get_height() const override;
	int get_layers() const override;
	virtual bool has_mipmaps() const override;
	virtual RID get_rid() const override;

	virtual void set_path(const String &p_path, bool p_take_over) override;

	virtual Ref<Image> get_layer_data(int p_layer) const override;

	CompressedTextureLayered(LayeredType p_layered_type);
	~CompressedTextureLayered();
};

class ResourceFormatLoaderCompressedTextureLayered : public ResourceFormatLoader {
public:
	virtual Ref<Resource> load(const String &p_path, const String &p_original_path = "", Error *r_error = nullptr, bool p_use_sub_threads = false, float *r_progress = nullptr, CacheMode p_cache_mode = CACHE_MODE_REUSE) override;
	virtual void get_recognized_extensions(List<String> *p_extensions) const override;
	virtual bool handles_type(const String &p_type) const override;
	virtual String get_resource_type(const String &p_path) const override;
};

class CompressedTexture2DArray : public CompressedTextureLayered {
	GDCLASS(CompressedTexture2DArray, CompressedTextureLayered)
public:
	CompressedTexture2DArray() :
			CompressedTextureLayered(LAYERED_TYPE_2D_ARRAY) {}
};

class CompressedCubemap : public CompressedTextureLayered {
	GDCLASS(CompressedCubemap, CompressedTextureLayered);

public:
	CompressedCubemap() :
			CompressedTextureLayered(LAYERED_TYPE_CUBEMAP) {}
};

class CompressedCubemapArray : public CompressedTextureLayered {
	GDCLASS(CompressedCubemapArray, CompressedTextureLayered);

public:
	CompressedCubemapArray() :
			CompressedTextureLayered(LAYERED_TYPE_CUBEMAP_ARRAY) {}
};

class CompressedTexture3D : public Texture3D {
	GDCLASS(CompressedTexture3D, Texture3D);

public:
	enum DataFormat {
		DATA_FORMAT_IMAGE,
		DATA_FORMAT_PNG,
		DATA_FORMAT_WEBP,
		DATA_FORMAT_BASIS_UNIVERSAL,
	};

	enum {
		FORMAT_VERSION = 1
	};

	enum FormatBits {
		FORMAT_BIT_STREAM = 1 << 22,
		FORMAT_BIT_HAS_MIPMAPS = 1 << 23,
	};

private:
	Error _load_data(const String &p_path, Vector<Ref<Image>> &r_data, Image::Format &r_format, int &r_width, int &r_height, int &r_depth, bool &r_mipmaps);
	String path_to_file;
	mutable RID texture;
	Image::Format format = Image::FORMAT_L8;
	int w = 0;
	int h = 0;
	int d = 0;
	bool mipmaps = false;

	virtual void reload_from_file() override;

protected:
	static void _bind_methods();
	void _validate_property(PropertyInfo &p_property) const;

public:
	Image::Format get_format() const override;
	Error load(const String &p_path);
	String get_load_path() const;

	int get_width() const override;
	int get_height() const override;
	int get_depth() const override;
	virtual bool has_mipmaps() const override;
	virtual RID get_rid() const override;

	virtual void set_path(const String &p_path, bool p_take_over) override;

	virtual Vector<Ref<Image>> get_data() const override;

	CompressedTexture3D();
	~CompressedTexture3D();
};

class ResourceFormatLoaderCompressedTexture3D : public ResourceFormatLoader {
public:
	virtual Ref<Resource> load(const String &p_path, const String &p_original_path = "", Error *r_error = nullptr, bool p_use_sub_threads = false, float *r_progress = nullptr, CacheMode p_cache_mode = CACHE_MODE_REUSE) override;
	virtual void get_recognized_extensions(List<String> *p_extensions) const override;
	virtual bool handles_type(const String &p_type) const override;
	virtual String get_resource_type(const String &p_path) const override;
};

/**************************************************************************/
/*  image.h                                                               */
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

#ifndef IMAGE_H
#define IMAGE_H

#include "core/color.h"
#include "core/math/rect2.h"
#include "core/pool_vector.h"
#include "core/resource.h"

/**
 *	@author Juan Linietsky <reduzio@gmail.com>
 *
 * Image storage class. This is used to store an image in user memory, as well as
 * providing some basic methods for image manipulation.
 * Images can be loaded from a file, or registered into the Render object as textures.
 */

class Image;

typedef Error (*SavePNGFunc)(const String &p_path, const Ref<Image> &p_img);
typedef PoolVector<uint8_t> (*SavePNGBufferFunc)(const Ref<Image> &p_img);
typedef Error (*SaveJPGFunc)(const String &p_path, const Ref<Image> &p_img, float p_quality);
typedef PoolVector<uint8_t> (*SaveJPGBufferFunc)(const Ref<Image> &p_img, float p_quality);
typedef Ref<Image> (*ImageMemLoadFunc)(const uint8_t *p_png, int p_size);

typedef Error (*SaveEXRFunc)(const String &p_path, const Ref<Image> &p_img, bool p_grayscale);

class Image : public Resource {
	GDCLASS(Image, Resource);

public:
	static SavePNGFunc save_png_func;
	static SaveJPGFunc save_jpg_func;
	static SaveEXRFunc save_exr_func;
	static SavePNGBufferFunc save_png_buffer_func;
	static SaveJPGBufferFunc save_jpg_buffer_func;

	enum {
		MAX_WIDTH = 16384, // force a limit somehow
		MAX_HEIGHT = 16384 // force a limit somehow
	};

	enum Format {

		FORMAT_L8, //luminance
		FORMAT_LA8, //luminance-alpha
		FORMAT_R8,
		FORMAT_RG8,
		FORMAT_RGB8,
		FORMAT_RGBA8,
		FORMAT_RGBA4444,
		FORMAT_RGBA5551,
		FORMAT_RF, //float
		FORMAT_RGF,
		FORMAT_RGBF,
		FORMAT_RGBAF,
		FORMAT_RH, //half float
		FORMAT_RGH,
		FORMAT_RGBH,
		FORMAT_RGBAH,
		FORMAT_RGBE9995,
		FORMAT_DXT1, //s3tc bc1
		FORMAT_DXT3, //bc2
		FORMAT_DXT5, //bc3
		FORMAT_RGTC_R,
		FORMAT_RGTC_RG,
		FORMAT_BPTC_RGBA, //btpc bc7
		FORMAT_BPTC_RGBF, //float bc6h
		FORMAT_BPTC_RGBFU, //unsigned float bc6hu
		FORMAT_PVRTC2, //pvrtc
		FORMAT_PVRTC2A,
		FORMAT_PVRTC4,
		FORMAT_PVRTC4A,
		FORMAT_ETC, //etc1
		FORMAT_ETC2_R11, //etc2
		FORMAT_ETC2_R11S, //signed, NOT srgb.
		FORMAT_ETC2_RG11,
		FORMAT_ETC2_RG11S,
		FORMAT_ETC2_RGB8,
		FORMAT_ETC2_RGBA8,
		FORMAT_ETC2_RGB8A1,
		FORMAT_MAX
	};

	static const char *format_names[FORMAT_MAX];
	enum Interpolation {

		INTERPOLATE_NEAREST,
		INTERPOLATE_BILINEAR,
		INTERPOLATE_CUBIC,
		INTERPOLATE_TRILINEAR,
		INTERPOLATE_LANCZOS,
		/* INTERPOLATE_TRICUBIC, */
		/* INTERPOLATE GAUSS */
	};

	enum CompressSource {
		COMPRESS_SOURCE_GENERIC,
		COMPRESS_SOURCE_SRGB,
		COMPRESS_SOURCE_NORMAL,
		COMPRESS_SOURCE_LAYERED,
		COMPRESS_SOURCE_MAX,
	};

	//some functions provided by something else

	static ImageMemLoadFunc _png_mem_loader_func;
	static ImageMemLoadFunc _jpg_mem_loader_func;
	static ImageMemLoadFunc _webp_mem_loader_func;
	static ImageMemLoadFunc _tga_mem_loader_func;
	static ImageMemLoadFunc _bmp_mem_loader_func;

	static void (*_image_compress_bc_func)(Image *, float, CompressSource p_source);
	static void (*_image_compress_bptc_func)(Image *, float p_lossy_quality, CompressSource p_source);
	static void (*_image_compress_pvrtc2_func)(Image *);
	static void (*_image_compress_pvrtc4_func)(Image *);
	static void (*_image_compress_etc1_func)(Image *, float);
	static void (*_image_compress_etc2_func)(Image *, float, CompressSource p_source);

	static void (*_image_decompress_pvrtc)(Image *);
	static void (*_image_decompress_bc)(Image *);
	static void (*_image_decompress_bptc)(Image *);
	static void (*_image_decompress_etc1)(Image *);
	static void (*_image_decompress_etc2)(Image *);

	static PoolVector<uint8_t> (*webp_lossy_packer)(const Ref<Image> &p_image, float p_quality);
	static PoolVector<uint8_t> (*webp_lossless_packer)(const Ref<Image> &p_image);
	static Ref<Image> (*webp_unpacker)(const PoolVector<uint8_t> &p_buffer);
	static PoolVector<uint8_t> (*png_packer)(const Ref<Image> &p_image);
	static Ref<Image> (*png_unpacker)(const PoolVector<uint8_t> &p_buffer);

	PoolVector<uint8_t>::Write write_lock;

protected:
	static void _bind_methods();

private:
	void _create_empty(int p_width, int p_height, bool p_use_mipmaps, Format p_format) {
		create(p_width, p_height, p_use_mipmaps, p_format);
	}

	void _create_from_data(int p_width, int p_height, bool p_use_mipmaps, Format p_format, const PoolVector<uint8_t> &p_data) {
		create(p_width, p_height, p_use_mipmaps, p_format, p_data);
	}

	Format format;
	PoolVector<uint8_t> data;
	int width, height;
	bool mipmaps;

	void _copy_internals_from(const Image &p_image) {
		format = p_image.format;
		width = p_image.width;
		height = p_image.height;
		mipmaps = p_image.mipmaps;
		data = p_image.data;
	}

	_FORCE_INLINE_ void _get_mipmap_offset_and_size(int p_mipmap, int &r_offset, int &r_width, int &r_height) const; //get where the mipmap begins in data

	static int _get_dst_image_size(int p_width, int p_height, Format p_format, int &r_mipmaps, int p_mipmaps = -1);
	bool _can_modify(Format p_format) const;

	_FORCE_INLINE_ void _get_clipped_src_and_dest_rects(const Ref<Image> &p_src, const Rect2i &p_src_rect, const Point2i &p_dest, Rect2i &r_clipped_src_rect, Rect2i &r_clipped_dest_rect) const;

	_FORCE_INLINE_ void _put_pixelb(int p_x, int p_y, uint32_t p_pixel_size, uint8_t *p_data, const uint8_t *p_pixel);
	_FORCE_INLINE_ void _get_pixelb(int p_x, int p_y, uint32_t p_pixel_size, const uint8_t *p_data, uint8_t *p_pixel);

	_FORCE_INLINE_ void _repeat_pixel_over_subsequent_memory(uint8_t *p_pixel, int p_pixel_size, int p_count);

	void _set_data(const Dictionary &p_data);
	Dictionary _get_data() const;

	Error _load_from_buffer(const PoolVector<uint8_t> &p_array, ImageMemLoadFunc p_loader);

	static void average_4_uint8(uint8_t &p_out, const uint8_t &p_a, const uint8_t &p_b, const uint8_t &p_c, const uint8_t &p_d);
	static void average_4_float(float &p_out, const float &p_a, const float &p_b, const float &p_c, const float &p_d);
	static void average_4_half(uint16_t &p_out, const uint16_t &p_a, const uint16_t &p_b, const uint16_t &p_c, const uint16_t &p_d);
	static void average_4_rgbe9995(uint32_t &p_out, const uint32_t &p_a, const uint32_t &p_b, const uint32_t &p_c, const uint32_t &p_d);
	static void renormalize_uint8(uint8_t *p_rgb);
	static void renormalize_float(float *p_rgb);
	static void renormalize_half(uint16_t *p_rgb);
	static void renormalize_rgbe9995(uint32_t *p_rgb);

public:
	int get_width() const; ///< Get image width
	int get_height() const; ///< Get image height
	Vector2 get_size() const;
	bool has_mipmaps() const;
	int get_mipmap_count() const;

	/**
	 * Convert the image to another format, conversion only to raw byte format
	 */
	void convert(Format p_new_format);

	/**
	 * Get the current image format.
	 */
	Format get_format() const;

	int get_mipmap_offset(int p_mipmap) const; //get where the mipmap begins in data
	void get_mipmap_offset_and_size(int p_mipmap, int &r_ofs, int &r_size) const; //get where the mipmap begins in data
	void get_mipmap_offset_size_and_dimensions(int p_mipmap, int &r_ofs, int &r_size, int &w, int &h) const; //get where the mipmap begins in data

	/**
	 * Resize the image, using the preferred interpolation method.
	 */
	void resize_to_po2(bool p_square = false, Interpolation p_interpolation = INTERPOLATE_BILINEAR);
	void resize(int p_width, int p_height, Interpolation p_interpolation = INTERPOLATE_BILINEAR);
	void shrink_x2();
	void expand_x2_hq2x();
	bool is_size_po2() const;
	/**
	 * Crop the image to a specific size, if larger, then the image is filled by black
	 */
	void crop_from_point(int p_x, int p_y, int p_width, int p_height);
	void crop(int p_width, int p_height);

	void flip_x();
	void flip_y();

	/**
	 * Generate a mipmap to an image (creates an image 1/4 the size, with averaging of 4->1)
	 */
	Error generate_mipmaps(bool p_renormalize = false);

	void clear_mipmaps();
	void normalize(); //for normal maps

	/**
	 * Create a new image of a given size and format. Current image will be lost
	 */
	void create(int p_width, int p_height, bool p_use_mipmaps, Format p_format);
	void create(int p_width, int p_height, bool p_use_mipmaps, Format p_format, const PoolVector<uint8_t> &p_data);

	void create(const char **p_xpm);
	/**
	 * returns true when the image is empty (0,0) in size
	 */
	bool empty() const;

	PoolVector<uint8_t> get_data() const;

	Error load(const String &p_path);
	Error save_png(const String &p_path) const;
	Error save_jpg(const String &p_path, float quality) const;
	PoolVector<uint8_t> save_png_to_buffer() const;
	PoolVector<uint8_t> save_jpg_to_buffer(float quality) const;
	Error save_exr(const String &p_path, bool p_grayscale) const;

	/**
	 * create an empty image
	 */
	Image();
	/**
	 * create an empty image of a specific size and format
	 */
	Image(int p_width, int p_height, bool p_use_mipmaps, Format p_format);
	/**
	 * import an image of a specific size and format from a pointer
	 */
	Image(int p_width, int p_height, bool p_mipmaps, Format p_format, const PoolVector<uint8_t> &p_data);

	enum AlphaMode {
		ALPHA_NONE,
		ALPHA_BIT,
		ALPHA_BLEND
	};

	AlphaMode detect_alpha() const;
	bool is_invisible() const;

	static int get_format_pixel_size(Format p_format);
	static int get_format_pixel_rshift(Format p_format);
	static int get_format_block_size(Format p_format);
	static void get_format_min_pixel_size(Format p_format, int &r_w, int &r_h);

	static int get_image_data_size(int p_width, int p_height, Format p_format, bool p_mipmaps = false);
	static int get_image_required_mipmaps(int p_width, int p_height, Format p_format);
	static int get_image_mipmap_offset(int p_width, int p_height, Format p_format, int p_mipmap);

	enum CompressMode {
		COMPRESS_S3TC,
		COMPRESS_PVRTC2,
		COMPRESS_PVRTC4,
		COMPRESS_ETC,
		COMPRESS_ETC2,
		COMPRESS_BPTC,
		COMPRESS_MAX,
	};

	Error compress(CompressMode p_mode = COMPRESS_S3TC, CompressSource p_source = COMPRESS_SOURCE_GENERIC, float p_lossy_quality = 0.7);
	Error decompress();
	bool is_compressed() const;

	void fix_alpha_edges();
	void premultiply_alpha();
	void srgb_to_linear();
	void normalmap_to_xy();
	Ref<Image> rgbe_to_srgb();
	void bumpmap_to_normalmap(float bump_scale = 1.0);

	void blit_rect(const Ref<Image> &p_src, const Rect2 &p_src_rect, const Point2 &p_dest);
	void blit_rect_mask(const Ref<Image> &p_src, const Ref<Image> &p_mask, const Rect2 &p_src_rect, const Point2 &p_dest);
	void blend_rect(const Ref<Image> &p_src, const Rect2 &p_src_rect, const Point2 &p_dest);
	void blend_rect_mask(const Ref<Image> &p_src, const Ref<Image> &p_mask, const Rect2 &p_src_rect, const Point2 &p_dest);
	void fill(const Color &p_color);
	void fill_rect(const Rect2 &p_rect, const Color &p_color);

	Rect2 get_used_rect() const;
	Ref<Image> get_rect(const Rect2 &p_area) const;

	static void set_compress_bc_func(void (*p_compress_func)(Image *, float, CompressSource));
	static void set_compress_bptc_func(void (*p_compress_func)(Image *, float, CompressSource));
	static String get_format_name(Format p_format);

	Error load_png_from_buffer(const PoolVector<uint8_t> &p_array);
	Error load_jpg_from_buffer(const PoolVector<uint8_t> &p_array);
	Error load_webp_from_buffer(const PoolVector<uint8_t> &p_array);
	Error load_tga_from_buffer(const PoolVector<uint8_t> &p_array);
	Error load_bmp_from_buffer(const PoolVector<uint8_t> &p_array);

	Image(const uint8_t *p_mem_png_jpg, int p_len = -1);
	Image(const char **p_xpm);

	virtual Ref<Resource> duplicate(bool p_subresources = false) const;

	void lock();
	void unlock();

	//this is used for compression
	enum DetectChannels {
		DETECTED_L,
		DETECTED_LA,
		DETECTED_R,
		DETECTED_RG,
		DETECTED_RGB,
		DETECTED_RGBA,
	};

	DetectChannels get_detected_channels();
	void optimize_channels();

	Color get_pixelv(const Point2 &p_src) const;
	Color get_pixel(int p_x, int p_y) const;
	void set_pixelv(const Point2 &p_dst, const Color &p_color);
	void set_pixel(int p_x, int p_y, const Color &p_color);

	void copy_internals_from(const Ref<Image> &p_image) {
		ERR_FAIL_COND_MSG(p_image.is_null(), "It's not a reference to a valid Image object.");
		format = p_image->format;
		width = p_image->width;
		height = p_image->height;
		mipmaps = p_image->mipmaps;
		data = p_image->data;
	}

	~Image();
};

VARIANT_ENUM_CAST(Image::Format)
VARIANT_ENUM_CAST(Image::Interpolation)
VARIANT_ENUM_CAST(Image::CompressMode)
VARIANT_ENUM_CAST(Image::CompressSource)
VARIANT_ENUM_CAST(Image::AlphaMode)

#endif // IMAGE_H

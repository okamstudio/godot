/**************************************************************************/
/*  portable_compressed_texture.cpp                                       */
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

#include "portable_compressed_texture.h"

#include "core/config/project_settings.h"
#include "core/io/marshalls.h"
#include "scene/resources/bit_map.h"

void PortableCompressedTexture2D::_set_data(const Vector<uint8_t> &p_data) {
	if (p_data.size() == 0) {
		return; //nothing to do
	}

	const uint8_t *data = p_data.ptr();
	uint32_t data_size = p_data.size();
	ERR_FAIL_COND(data_size < 20);
	compression_mode = CompressionMode(decode_uint16(data));
	DataFormat data_format = DataFormat(decode_uint16(data + 2));
	format = Image::Format(decode_uint32(data + 4));
	uint32_t mipmap_count = decode_uint32(data + 8);
	size.width = decode_uint32(data + 12);
	size.height = decode_uint32(data + 16);
	mipmaps = mipmap_count > 1;

	data += 20;
	data_size -= 20;

	Ref<Image> image;

	switch (compression_mode) {
		case COMPRESSION_MODE_LOSSLESS:
		case COMPRESSION_MODE_LOSSY: {
			ImageMemLoadFunc loader_func;
			if (data_format == DATA_FORMAT_UNDEFINED) {
				loader_func = nullptr;
			} else if (data_format == DATA_FORMAT_PNG) {
				loader_func = Image::_png_mem_unpacker_func;
			} else if (data_format == DATA_FORMAT_WEBP) {
				loader_func = Image::_webp_mem_loader_func;
			} else {
				ERR_FAIL();
			}
			Vector<uint8_t> image_data;

			ERR_FAIL_COND(data_size < 4);
			for (uint32_t i = 0; i < mipmap_count; i++) {
				uint32_t mipsize = decode_uint32(data);
				data += 4;
				data_size -= 4;
				ERR_FAIL_COND(mipsize > data_size);
				Ref<Image> img = loader_func == nullptr
						? memnew(Image(data, data_size))
						: Ref<Image>(loader_func(data, data_size));
				ERR_FAIL_COND(img->is_empty());
				if (img->get_format() != format) { // May happen due to webp/png in the tiny mipmaps.
					img->convert(format);
				}
				image_data.append_array(img->get_data());

				data += mipsize;
				data_size -= mipsize;
			}

			image.instantiate(size.width, size.height, mipmaps, format, image_data);

		} break;
		case COMPRESSION_MODE_BASIS_UNIVERSAL: {
			ERR_FAIL_NULL(Image::basis_universal_unpacker_ptr);
			image = Image::basis_universal_unpacker_ptr(data, data_size);

		} break;
		case COMPRESSION_MODE_S3TC:
		case COMPRESSION_MODE_ETC2:
		case COMPRESSION_MODE_BPTC: {
			image.instantiate(size.width, size.height, mipmaps, format, p_data.slice(20));
		} break;
	}
	ERR_FAIL_COND(image.is_null());

	if (texture.is_null()) {
		texture = RenderingServer::get_singleton()->texture_2d_create(image);
	} else {
		RID new_texture = RenderingServer::get_singleton()->texture_2d_create(image);
		RenderingServer::get_singleton()->texture_replace(texture, new_texture);
	}

	image_stored = true;
	size_override = size;
	RenderingServer::get_singleton()->texture_set_size_override(texture, size_override.width, size_override.height);
	alpha_cache.unref();

	if (keep_all_compressed_buffers || keep_compressed_buffer) {
		compressed_buffer = p_data;
	} else {
		compressed_buffer.clear();
	}
}

PortableCompressedTexture2D::CompressionMode PortableCompressedTexture2D::get_compression_mode() const {
	return compression_mode;
}
Vector<uint8_t> PortableCompressedTexture2D::_get_data() const {
	return compressed_buffer;
}

void PortableCompressedTexture2D::create_from_image(const Ref<Image> &p_image, CompressionMode p_compression_mode, bool p_normal_map, float p_lossy_quality) {
	ERR_FAIL_COND(p_image.is_null() || p_image->is_empty());

	Vector<uint8_t> buffer;

	buffer.resize(20);
	encode_uint16(p_compression_mode, buffer.ptrw());
	encode_uint16(DATA_FORMAT_UNDEFINED, buffer.ptrw() + 2);
	encode_uint32(p_image->get_format(), buffer.ptrw() + 4);
	encode_uint32(p_image->get_mipmap_count() + 1, buffer.ptrw() + 8);
	encode_uint32(p_image->get_width(), buffer.ptrw() + 12);
	encode_uint32(p_image->get_height(), buffer.ptrw() + 16);

	switch (p_compression_mode) {
		case COMPRESSION_MODE_LOSSLESS:
		case COMPRESSION_MODE_LOSSY: {
			bool lossless_force_png = GLOBAL_GET("rendering/textures/lossless_compression/force_png") ||
					!Image::_webp_mem_loader_func; // WebP module disabled.
			bool use_webp = !lossless_force_png && p_image->get_width() <= 16383 && p_image->get_height() <= 16383; // WebP has a size limit.
			for (int i = 0; i < p_image->get_mipmap_count() + 1; i++) {
				Vector<uint8_t> data;
				if (p_compression_mode == COMPRESSION_MODE_LOSSY) {
					data = Image::webp_lossy_packer(i ? p_image->get_image_from_mipmap(i) : p_image, p_lossy_quality);
					encode_uint16(DATA_FORMAT_WEBP, buffer.ptrw() + 2);
				} else {
					if (use_webp) {
						data = Image::webp_lossless_packer(i ? p_image->get_image_from_mipmap(i) : p_image);
						encode_uint16(DATA_FORMAT_WEBP, buffer.ptrw() + 2);
					} else {
						data = Image::png_packer(i ? p_image->get_image_from_mipmap(i) : p_image);
						encode_uint16(DATA_FORMAT_PNG, buffer.ptrw() + 2);
					}
				}
				int data_len = data.size();
				buffer.resize(buffer.size() + 4);
				encode_uint32(data_len, buffer.ptrw() + buffer.size() - 4);
				buffer.append_array(data);
			}
		} break;
		case COMPRESSION_MODE_BASIS_UNIVERSAL: {
			encode_uint16(DATA_FORMAT_BASIS_UNIVERSAL, buffer.ptrw() + 2);
			Image::UsedChannels uc = p_image->detect_used_channels(p_normal_map ? Image::COMPRESS_SOURCE_NORMAL : Image::COMPRESS_SOURCE_GENERIC);
			Vector<uint8_t> budata = Image::basis_universal_packer(p_image, uc);
			buffer.append_array(budata);

		} break;
		case COMPRESSION_MODE_S3TC:
		case COMPRESSION_MODE_ETC2:
		case COMPRESSION_MODE_BPTC: {
			encode_uint16(DATA_FORMAT_IMAGE, buffer.ptrw() + 2);
			Ref<Image> copy = p_image->duplicate();
			switch (p_compression_mode) {
				case COMPRESSION_MODE_S3TC:
					copy->compress(Image::COMPRESS_S3TC);
					break;
				case COMPRESSION_MODE_ETC2:
					copy->compress(Image::COMPRESS_ETC2);
					break;
				case COMPRESSION_MODE_BPTC:
					copy->compress(Image::COMPRESS_BPTC);
					break;
				default: {
				};
			}

			buffer.append_array(copy->get_data());

		} break;
	}

	_set_data(buffer);
}

Image::Format PortableCompressedTexture2D::get_format() const {
	return format;
}

Ref<Image> PortableCompressedTexture2D::get_image() const {
	if (image_stored) {
		return RenderingServer::get_singleton()->texture_2d_get(texture);
	} else {
		return Ref<Image>();
	}
}

int PortableCompressedTexture2D::get_width() const {
	return size.width;
}

int PortableCompressedTexture2D::get_height() const {
	return size.height;
}

RID PortableCompressedTexture2D::get_rid() const {
	if (texture.is_null()) {
		// We are in trouble, create something temporary.
		texture = RenderingServer::get_singleton()->texture_2d_placeholder_create();
	}
	return texture;
}

bool PortableCompressedTexture2D::has_alpha() const {
	return (format == Image::FORMAT_LA8 || format == Image::FORMAT_RGBA8);
}

void PortableCompressedTexture2D::draw(RID p_canvas_item, const Point2 &p_pos, const Color &p_modulate, bool p_transpose) const {
	if (size.width == 0 || size.height == 0) {
		return;
	}
	RenderingServer::get_singleton()->canvas_item_add_texture_rect(p_canvas_item, Rect2(p_pos, size), texture, false, p_modulate, p_transpose);
}

void PortableCompressedTexture2D::draw_rect(RID p_canvas_item, const Rect2 &p_rect, bool p_tile, const Color &p_modulate, bool p_transpose) const {
	if (size.width == 0 || size.height == 0) {
		return;
	}
	RenderingServer::get_singleton()->canvas_item_add_texture_rect(p_canvas_item, p_rect, texture, p_tile, p_modulate, p_transpose);
}

void PortableCompressedTexture2D::draw_rect_region(RID p_canvas_item, const Rect2 &p_rect, const Rect2 &p_src_rect, const Color &p_modulate, bool p_transpose, bool p_clip_uv) const {
	if (size.width == 0 || size.height == 0) {
		return;
	}
	RenderingServer::get_singleton()->canvas_item_add_texture_rect_region(p_canvas_item, p_rect, texture, p_src_rect, p_modulate, p_transpose, p_clip_uv);
}

bool PortableCompressedTexture2D::is_pixel_opaque(int p_x, int p_y) const {
	if (alpha_cache.is_null()) {
		Ref<Image> img = get_image();
		if (img.is_valid()) {
			if (img->is_compressed()) { //must decompress, if compressed
				Ref<Image> decom = img->duplicate();
				decom->decompress();
				img = decom;
			}
			alpha_cache.instantiate();
			alpha_cache->create_from_image_alpha(img);
		}
	}

	if (alpha_cache.is_valid()) {
		int aw = int(alpha_cache->get_size().width);
		int ah = int(alpha_cache->get_size().height);
		if (aw == 0 || ah == 0) {
			return true;
		}

		int x = p_x * aw / size.width;
		int y = p_y * ah / size.height;

		x = CLAMP(x, 0, aw);
		y = CLAMP(y, 0, ah);

		return alpha_cache->get_bit(x, y);
	}

	return true;
}

void PortableCompressedTexture2D::set_size_override(const Size2 &p_size) {
	size_override = p_size;
	RenderingServer::get_singleton()->texture_set_size_override(texture, size_override.width, size_override.height);
}

Size2 PortableCompressedTexture2D::get_size_override() const {
	return size_override;
}

void PortableCompressedTexture2D::set_path(const String &p_path, bool p_take_over) {
	if (texture.is_valid()) {
		RenderingServer::get_singleton()->texture_set_path(texture, p_path);
	}

	Resource::set_path(p_path, p_take_over);
}

bool PortableCompressedTexture2D::keep_all_compressed_buffers = false;

void PortableCompressedTexture2D::set_keep_all_compressed_buffers(bool p_keep) {
	keep_all_compressed_buffers = p_keep;
}

bool PortableCompressedTexture2D::is_keeping_all_compressed_buffers() {
	return keep_all_compressed_buffers;
}

void PortableCompressedTexture2D::set_keep_compressed_buffer(bool p_keep) {
	keep_compressed_buffer = p_keep;
	if (!p_keep) {
		compressed_buffer.clear();
	}
}

bool PortableCompressedTexture2D::is_keeping_compressed_buffer() const {
	return keep_compressed_buffer;
}

void PortableCompressedTexture2D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("create_from_image", "image", "compression_mode", "normal_map", "lossy_quality"), &PortableCompressedTexture2D::create_from_image, DEFVAL(false), DEFVAL(0.8));
	ClassDB::bind_method(D_METHOD("get_format"), &PortableCompressedTexture2D::get_format);
	ClassDB::bind_method(D_METHOD("get_compression_mode"), &PortableCompressedTexture2D::get_compression_mode);

	ClassDB::bind_method(D_METHOD("set_size_override", "size"), &PortableCompressedTexture2D::set_size_override);
	ClassDB::bind_method(D_METHOD("get_size_override"), &PortableCompressedTexture2D::get_size_override);

	ClassDB::bind_method(D_METHOD("set_keep_compressed_buffer", "keep"), &PortableCompressedTexture2D::set_keep_compressed_buffer);
	ClassDB::bind_method(D_METHOD("is_keeping_compressed_buffer"), &PortableCompressedTexture2D::is_keeping_compressed_buffer);

	ClassDB::bind_method(D_METHOD("_set_data", "data"), &PortableCompressedTexture2D::_set_data);
	ClassDB::bind_method(D_METHOD("_get_data"), &PortableCompressedTexture2D::_get_data);

	ClassDB::bind_static_method("PortableCompressedTexture2D", D_METHOD("set_keep_all_compressed_buffers", "keep"), &PortableCompressedTexture2D::set_keep_all_compressed_buffers);
	ClassDB::bind_static_method("PortableCompressedTexture2D", D_METHOD("is_keeping_all_compressed_buffers"), &PortableCompressedTexture2D::is_keeping_all_compressed_buffers);

	ADD_PROPERTY(PropertyInfo(Variant::PACKED_BYTE_ARRAY, "_data", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NO_EDITOR | PROPERTY_USAGE_INTERNAL), "_set_data", "_get_data");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "size_override", PROPERTY_HINT_NONE, "suffix:px"), "set_size_override", "get_size_override");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "keep_compressed_buffer"), "set_keep_compressed_buffer", "is_keeping_compressed_buffer");

	BIND_ENUM_CONSTANT(COMPRESSION_MODE_LOSSLESS);
	BIND_ENUM_CONSTANT(COMPRESSION_MODE_LOSSY);
	BIND_ENUM_CONSTANT(COMPRESSION_MODE_BASIS_UNIVERSAL);
	BIND_ENUM_CONSTANT(COMPRESSION_MODE_S3TC);
	BIND_ENUM_CONSTANT(COMPRESSION_MODE_ETC2);
	BIND_ENUM_CONSTANT(COMPRESSION_MODE_BPTC);
}

PortableCompressedTexture2D::PortableCompressedTexture2D() {}

PortableCompressedTexture2D::~PortableCompressedTexture2D() {
	if (texture.is_valid()) {
		ERR_FAIL_NULL(RenderingServer::get_singleton());
		RenderingServer::get_singleton()->free(texture);
	}
}

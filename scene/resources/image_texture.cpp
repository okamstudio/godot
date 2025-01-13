/**************************************************************************/
/*  image_texture.cpp                                                     */
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

#include "image_texture.h"

#include "core/io/image_loader.h"
#include "scene/resources/bit_map.h"
#include "scene/resources/placeholder_textures.h"

void ImageTexture::reload_from_file() {
	String path = ResourceLoader::path_remap(get_path());
	if (!path.is_resource_file()) {
		return;
	}

	Ref<Image> img;
	img.instantiate();

	if (ImageLoader::load_image(path, img) == OK) {
		set_image(img);
	} else {
		Resource::reload_from_file();
		notify_property_list_changed();
		emit_changed();
	}
}

bool ImageTexture::_set(const StringName &p_name, const Variant &p_value) {
	if (p_name == "image") {
		set_image(p_value);
		return true;
	}
	return false;
}

bool ImageTexture::_get(const StringName &p_name, Variant &r_ret) const {
	if (p_name == "image") {
		r_ret = get_image();
		return true;
	}
	return false;
}

void ImageTexture::_get_property_list(List<PropertyInfo> *p_list) const {
	p_list->push_back(PropertyInfo(Variant::OBJECT, PNAME("image"), PROPERTY_HINT_RESOURCE_TYPE, "Image", PROPERTY_USAGE_STORAGE | PROPERTY_USAGE_RESOURCE_NOT_PERSISTENT));
}

Ref<ImageTexture> ImageTexture::create_from_image(const Ref<Image> &p_image) {
	ERR_FAIL_COND_V_MSG(p_image.is_null(), Ref<ImageTexture>(), "Invalid image: null");
	ERR_FAIL_COND_V_MSG(p_image->is_empty(), Ref<ImageTexture>(), "Invalid image: image is empty");

	Ref<ImageTexture> image_texture;
	image_texture.instantiate();
	image_texture->set_image(p_image);
	return image_texture;
}

void ImageTexture::set_image(const Ref<Image> &p_image) {
	ERR_FAIL_COND_MSG(p_image.is_null() || p_image->is_empty(), "Invalid image");
	w = p_image->get_width();
	h = p_image->get_height();
	format = p_image->get_format();
	mipmaps = p_image->has_mipmaps();

	if (texture.is_null()) {
		texture = RenderingServer::get_singleton()->texture_2d_create(p_image);
	} else {
		RID new_texture = RenderingServer::get_singleton()->texture_2d_create(p_image);
		RenderingServer::get_singleton()->texture_replace(texture, new_texture);
	}
	notify_property_list_changed();
	emit_changed();

	image_stored = true;
}

Image::Format ImageTexture::get_format() const {
	return format;
}

void ImageTexture::update(const Ref<Image> &p_image) {
	ERR_FAIL_COND_MSG(p_image.is_null(), "Invalid image");
	ERR_FAIL_COND_MSG(texture.is_null(), "Texture is not initialized.");
	ERR_FAIL_COND_MSG(p_image->get_width() != w || p_image->get_height() != h,
			"The new image dimensions must match the texture size.");
	ERR_FAIL_COND_MSG(p_image->get_format() != format,
			"The new image format must match the texture's image format.");
	ERR_FAIL_COND_MSG(mipmaps != p_image->has_mipmaps(),
			"The new image mipmaps configuration must match the texture's image mipmaps configuration");

	RS::get_singleton()->texture_2d_update(texture, p_image);

	notify_property_list_changed();
	emit_changed();

	alpha_cache.unref();
	image_stored = true;
}

Ref<Image> ImageTexture::get_image() const {
	if (image_stored) {
		return RenderingServer::get_singleton()->texture_2d_get(texture);
	} else {
		return Ref<Image>();
	}
}

int ImageTexture::get_width() const {
	return w;
}

int ImageTexture::get_height() const {
	return h;
}

RID ImageTexture::get_rid() const {
	if (texture.is_null()) {
		// We are in trouble, create something temporary.
		texture = RenderingServer::get_singleton()->texture_2d_placeholder_create();
	}
	return texture;
}

bool ImageTexture::has_alpha() const {
	return (format == Image::FORMAT_LA8 || format == Image::FORMAT_RGBA8);
}

void ImageTexture::draw(RID p_canvas_item, const Point2 &p_pos, const Color &p_modulate, bool p_transpose) const {
	if ((w | h) == 0) {
		return;
	}
	RenderingServer::get_singleton()->canvas_item_add_texture_rect(p_canvas_item, Rect2(p_pos, Size2(w, h)), texture, false, p_modulate, p_transpose);
}

void ImageTexture::draw_rect(RID p_canvas_item, const Rect2 &p_rect, bool p_tile, const Color &p_modulate, bool p_transpose) const {
	if ((w | h) == 0) {
		return;
	}
	RenderingServer::get_singleton()->canvas_item_add_texture_rect(p_canvas_item, p_rect, texture, p_tile, p_modulate, p_transpose);
}

void ImageTexture::draw_rect_region(RID p_canvas_item, const Rect2 &p_rect, const Rect2 &p_src_rect, const Color &p_modulate, bool p_transpose, bool p_clip_uv) const {
	if ((w | h) == 0) {
		return;
	}
	RenderingServer::get_singleton()->canvas_item_add_texture_rect_region(p_canvas_item, p_rect, texture, p_src_rect, p_modulate, p_transpose, p_clip_uv);
}

bool ImageTexture::is_pixel_opaque(int p_x, int p_y) const {
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

		int x = p_x * aw / w;
		int y = p_y * ah / h;

		x = CLAMP(x, 0, aw);
		y = CLAMP(y, 0, ah);

		return alpha_cache->get_bit(x, y);
	}

	return true;
}

void ImageTexture::set_size_override(const Size2i &p_size) {
	Size2i s = p_size;
	if (s.x != 0) {
		w = s.x;
	}
	if (s.y != 0) {
		h = s.y;
	}
	RenderingServer::get_singleton()->texture_set_size_override(texture, w, h);
}

void ImageTexture::set_path(const String &p_path, bool p_take_over) {
	if (texture.is_valid()) {
		RenderingServer::get_singleton()->texture_set_path(texture, p_path);
	}

	Resource::set_path(p_path, p_take_over);
}

void ImageTexture::_bind_methods() {
	ClassDB::bind_static_method("ImageTexture", D_METHOD("create_from_image", "image"), &ImageTexture::create_from_image);
	ClassDB::bind_method(D_METHOD("get_format"), &ImageTexture::get_format);

	ClassDB::bind_method(D_METHOD("set_image", "image"), &ImageTexture::set_image);
	ClassDB::bind_method(D_METHOD("update", "image"), &ImageTexture::update);
	ClassDB::bind_method(D_METHOD("set_size_override", "size"), &ImageTexture::set_size_override);
}

ImageTexture::ImageTexture() {}

ImageTexture::~ImageTexture() {
	if (texture.is_valid()) {
		ERR_FAIL_NULL(RenderingServer::get_singleton());
		RenderingServer::get_singleton()->free(texture);
	}
}

Image::Format ImageTextureLayered::get_format() const {
	return format;
}

int ImageTextureLayered::get_width() const {
	return width;
}

int ImageTextureLayered::get_height() const {
	return height;
}

int ImageTextureLayered::get_layers() const {
	return layers;
}

bool ImageTextureLayered::has_mipmaps() const {
	return mipmaps;
}

ImageTextureLayered::LayeredType ImageTextureLayered::get_layered_type() const {
	return layered_type;
}

Error ImageTextureLayered::_create_from_images(const TypedArray<Image> &p_images) {
	Vector<Ref<Image>> images;
	for (int i = 0; i < p_images.size(); i++) {
		Ref<Image> img = p_images[i];
		ERR_FAIL_COND_V(img.is_null(), ERR_INVALID_PARAMETER);
		images.push_back(img);
	}

	return create_from_images(images);
}

TypedArray<Image> ImageTextureLayered::_get_images() const {
	TypedArray<Image> images;
	for (int i = 0; i < layers; i++) {
		images.push_back(get_layer_data(i));
	}
	return images;
}

void ImageTextureLayered::_set_images(const TypedArray<Image> &p_images) {
	ERR_FAIL_COND(_create_from_images(p_images) != OK);
}

Error ImageTextureLayered::create_from_images(Vector<Ref<Image>> p_images) {
	int new_layers = p_images.size();
	ERR_FAIL_COND_V(new_layers == 0, ERR_INVALID_PARAMETER);
	if (layered_type == LAYERED_TYPE_CUBEMAP) {
		ERR_FAIL_COND_V_MSG(new_layers != 6, ERR_INVALID_PARAMETER,
				"Cubemaps require exactly 6 layers");
	} else if (layered_type == LAYERED_TYPE_CUBEMAP_ARRAY) {
		ERR_FAIL_COND_V_MSG((new_layers % 6) != 0, ERR_INVALID_PARAMETER,
				"Cubemap array layers must be a multiple of 6");
	}

	ERR_FAIL_COND_V(p_images[0].is_null() || p_images[0]->is_empty(), ERR_INVALID_PARAMETER);

	Image::Format new_format = p_images[0]->get_format();
	int new_width = p_images[0]->get_width();
	int new_height = p_images[0]->get_height();
	bool new_mipmaps = p_images[0]->has_mipmaps();

	for (int i = 1; i < p_images.size(); i++) {
		ERR_FAIL_COND_V_MSG(p_images[i]->get_format() != new_format, ERR_INVALID_PARAMETER,
				"All images must share the same format");
		ERR_FAIL_COND_V_MSG(p_images[i]->get_width() != new_width || p_images[i]->get_height() != new_height, ERR_INVALID_PARAMETER,
				"All images must share the same dimensions");
		ERR_FAIL_COND_V_MSG(p_images[i]->has_mipmaps() != new_mipmaps, ERR_INVALID_PARAMETER,
				"All images must share the usage of mipmaps");
	}

	if (texture.is_valid()) {
		RID new_texture = RS::get_singleton()->texture_2d_layered_create(p_images, RS::TextureLayeredType(layered_type));
		ERR_FAIL_COND_V(!new_texture.is_valid(), ERR_CANT_CREATE);
		RS::get_singleton()->texture_replace(texture, new_texture);
	} else {
		texture = RS::get_singleton()->texture_2d_layered_create(p_images, RS::TextureLayeredType(layered_type));
		ERR_FAIL_COND_V(!texture.is_valid(), ERR_CANT_CREATE);
	}

	format = new_format;
	width = new_width;
	height = new_height;
	layers = new_layers;
	mipmaps = new_mipmaps;
	return OK;
}

void ImageTextureLayered::update_layer(const Ref<Image> &p_image, int p_layer) {
	ERR_FAIL_COND_MSG(texture.is_null(), "Texture is not initialized.");
	ERR_FAIL_COND_MSG(p_image.is_null(), "Invalid image.");
	ERR_FAIL_COND_MSG(p_image->get_format() != format, "Image format must match texture's image format.");
	ERR_FAIL_COND_MSG(p_image->get_width() != width || p_image->get_height() != height, "Image size must match texture's image size.");
	ERR_FAIL_COND_MSG(p_image->has_mipmaps() != mipmaps, "Image mipmap configuration must match texture's image mipmap configuration.");
	ERR_FAIL_INDEX_MSG(p_layer, layers, "Layer index is out of bounds.");
	RS::get_singleton()->texture_2d_update(texture, p_image, p_layer);
}

Ref<Image> ImageTextureLayered::get_layer_data(int p_layer) const {
	ERR_FAIL_INDEX_V(p_layer, layers, Ref<Image>());
	return RS::get_singleton()->texture_2d_layer_get(texture, p_layer);
}

RID ImageTextureLayered::get_rid() const {
	if (texture.is_null()) {
		texture = RS::get_singleton()->texture_2d_layered_placeholder_create(RS::TextureLayeredType(layered_type));
	}
	return texture;
}

void ImageTextureLayered::set_path(const String &p_path, bool p_take_over) {
	if (texture.is_valid()) {
		RS::get_singleton()->texture_set_path(texture, p_path);
	}

	Resource::set_path(p_path, p_take_over);
}

void ImageTextureLayered::_bind_methods() {
	ClassDB::bind_method(D_METHOD("create_from_images", "images"), &ImageTextureLayered::_create_from_images);
	ClassDB::bind_method(D_METHOD("update_layer", "image", "layer"), &ImageTextureLayered::update_layer);

	ClassDB::bind_method(D_METHOD("_get_images"), &ImageTextureLayered::_get_images);
	ClassDB::bind_method(D_METHOD("_set_images", "images"), &ImageTextureLayered::_set_images);

	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "_images", PROPERTY_HINT_ARRAY_TYPE, "Image", PROPERTY_USAGE_INTERNAL | PROPERTY_USAGE_STORAGE | PROPERTY_USAGE_RESOURCE_NOT_PERSISTENT), "_set_images", "_get_images");
}

ImageTextureLayered::ImageTextureLayered(LayeredType p_layered_type) {
	layered_type = p_layered_type;
}

ImageTextureLayered::~ImageTextureLayered() {
	if (texture.is_valid()) {
		ERR_FAIL_NULL(RenderingServer::get_singleton());
		RS::get_singleton()->free(texture);
	}
}

Image::Format ImageTexture3D::get_format() const {
	return format;
}
int ImageTexture3D::get_width() const {
	return width;
}
int ImageTexture3D::get_height() const {
	return height;
}
int ImageTexture3D::get_depth() const {
	return depth;
}
bool ImageTexture3D::has_mipmaps() const {
	return mipmaps;
}

Error ImageTexture3D::_create(Image::Format p_format, int p_width, int p_height, int p_depth, bool p_mipmaps, const TypedArray<Image> &p_data) {
	Vector<Ref<Image>> images;
	images.resize(p_data.size());
	for (int i = 0; i < images.size(); i++) {
		images.write[i] = p_data[i];
	}
	return create(p_format, p_width, p_height, p_depth, p_mipmaps, images);
}

void ImageTexture3D::_update(const TypedArray<Image> &p_data) {
	Vector<Ref<Image>> images;
	images.resize(p_data.size());
	for (int i = 0; i < images.size(); i++) {
		images.write[i] = p_data[i];
	}
	return update(images);
}

Error ImageTexture3D::create(Image::Format p_format, int p_width, int p_height, int p_depth, bool p_mipmaps, const Vector<Ref<Image>> &p_data) {
	RID tex = RenderingServer::get_singleton()->texture_3d_create(p_format, p_width, p_height, p_depth, p_mipmaps, p_data);
	ERR_FAIL_COND_V(tex.is_null(), ERR_CANT_CREATE);

	if (texture.is_valid()) {
		RenderingServer::get_singleton()->texture_replace(texture, tex);
	} else {
		texture = tex;
	}

	format = p_format;
	width = p_width;
	height = p_height;
	depth = p_depth;
	mipmaps = p_mipmaps;

	return OK;
}

void ImageTexture3D::update(const Vector<Ref<Image>> &p_data) {
	ERR_FAIL_COND(!texture.is_valid());
	RenderingServer::get_singleton()->texture_3d_update(texture, p_data);
}

Vector<Ref<Image>> ImageTexture3D::get_data() const {
	ERR_FAIL_COND_V(!texture.is_valid(), Vector<Ref<Image>>());
	return RS::get_singleton()->texture_3d_get(texture);
}

RID ImageTexture3D::get_rid() const {
	if (!texture.is_valid()) {
		texture = RS::get_singleton()->texture_3d_placeholder_create();
	}
	return texture;
}
void ImageTexture3D::set_path(const String &p_path, bool p_take_over) {
	if (texture.is_valid()) {
		RenderingServer::get_singleton()->texture_set_path(texture, p_path);
	}

	Resource::set_path(p_path, p_take_over);
}

TypedArray<Image> ImageTexture3D::_get_images() const {
	TypedArray<Image> images;
	if (texture.is_valid()) {
		Vector<Ref<Image>> raw_images = get_data();
		ERR_FAIL_COND_V(raw_images.is_empty(), TypedArray<Image>());

		for (int i = 0; i < raw_images.size(); i++) {
			images.push_back(raw_images[i]);
		}
	}
	return images;
}

void ImageTexture3D::_set_images(const TypedArray<Image> &p_images) {
	int new_layers = p_images.size();
	ERR_FAIL_COND(new_layers == 0);
	Ref<Image> img_base = p_images[0];
	ERR_FAIL_COND(img_base.is_null());

	Image::Format new_format = img_base->get_format();
	int new_width = img_base->get_width();
	int new_height = img_base->get_height();
	int new_depth = 0;
	bool new_mipmaps = false;

	for (int i = 1; i < p_images.size(); i++) {
		Ref<Image> img = p_images[i];
		ERR_FAIL_COND(img.is_null());
		ERR_FAIL_COND_MSG(img->get_format() != new_format, "All images must share the same format.");

		if (img->get_width() != new_width || img->get_height() != new_height) {
			new_mipmaps = true;
			if (new_depth == 0) {
				new_depth = i;
			}
		}
	}

	if (new_depth == 0) {
		new_depth = p_images.size();
	}

	Error err = _create(new_format, new_width, new_height, new_depth, new_mipmaps, p_images);
	ERR_FAIL_COND(err != OK);
}

void ImageTexture3D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("create", "format", "width", "height", "depth", "use_mipmaps", "data"), &ImageTexture3D::_create);
	ClassDB::bind_method(D_METHOD("update", "data"), &ImageTexture3D::_update);
	ClassDB::bind_method(D_METHOD("_get_images"), &ImageTexture3D::_get_images);
	ClassDB::bind_method(D_METHOD("_set_images", "images"), &ImageTexture3D::_set_images);
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "_images", PROPERTY_HINT_ARRAY_TYPE, "Image", PROPERTY_USAGE_INTERNAL | PROPERTY_USAGE_STORAGE | PROPERTY_USAGE_RESOURCE_NOT_PERSISTENT), "_set_images", "_get_images");
}

ImageTexture3D::ImageTexture3D() {
}

ImageTexture3D::~ImageTexture3D() {
	if (texture.is_valid()) {
		ERR_FAIL_NULL(RenderingServer::get_singleton());
		RS::get_singleton()->free(texture);
	}
}

void Texture2DArray::_bind_methods() {
	ClassDB::bind_method(D_METHOD("create_placeholder"), &Texture2DArray::create_placeholder);
}

Ref<Resource> Texture2DArray::create_placeholder() const {
	Ref<PlaceholderTexture2DArray> placeholder;
	placeholder.instantiate();
	placeholder->set_size(Size2i(get_width(), get_height()));
	placeholder->set_layers(get_layers());
	return placeholder;
}

void Cubemap::_bind_methods() {
	ClassDB::bind_method(D_METHOD("create_placeholder"), &Cubemap::create_placeholder);
}

Ref<Resource> Cubemap::create_placeholder() const {
	Ref<PlaceholderCubemap> placeholder;
	placeholder.instantiate();
	placeholder->set_size(Size2i(get_width(), get_height()));
	placeholder->set_layers(get_layers());
	return placeholder;
}

void CubemapArray::_bind_methods() {
	ClassDB::bind_method(D_METHOD("create_placeholder"), &CubemapArray::create_placeholder);
}

Ref<Resource> CubemapArray::create_placeholder() const {
	Ref<PlaceholderCubemapArray> placeholder;
	placeholder.instantiate();
	placeholder->set_size(Size2i(get_width(), get_height()));
	placeholder->set_layers(get_layers());
	return placeholder;
}

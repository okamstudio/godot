/**************************************************************************/
/*  texture_loader_dds.cpp                                                */
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

#include "texture_loader_dds.h"

#include "core/io/file_access.h"
#include "image_loader_dds.h"
#include "scene/resources/image_texture.h"

Ref<Resource> ResourceFormatDDS::load(const String &p_path, const String &p_original_path, Error *r_error, bool p_use_sub_threads, float *r_progress, CacheMode p_cache_mode) {
	if (r_error) {
		*r_error = ERR_CANT_OPEN;
	}

	Error err;
	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::READ, &err);
	if (f.is_null()) {
		return Ref<Resource>();
	}

	Ref<FileAccess> fref(f);
	if (r_error) {
		*r_error = ERR_FILE_CORRUPT;
	}

	ERR_FAIL_COND_V_MSG(err != OK, Ref<Resource>(), vformat("Unable to open DDS texture file '%s'.", p_path));

	Vector<Ref<Image>> images;
	uint32_t dds_type = 0;
	err = ImageLoaderDDS::load_image_layers(images, f, &dds_type);

	ERR_FAIL_COND_V_MSG(err != OK, Ref<Resource>(), vformat("Unable to read layers in DDS texture file '%s'.", p_path));

	uint32_t layer_count = images.size();

	if ((dds_type & ImageLoaderDDS::DDST_TYPE_MASK) == ImageLoaderDDS::DDST_2D) {
		if (dds_type & ImageLoaderDDS::DDST_ARRAY) {
			Ref<Texture2DArray> texture = memnew(Texture2DArray());
			texture->create_from_images(images);

			if (r_error) {
				*r_error = OK;
			}

			return texture;

		} else {
			if (r_error) {
				*r_error = OK;
			}

			return ImageTexture::create_from_image(images[0]);
		}

	} else if ((dds_type & ImageLoaderDDS::DDST_TYPE_MASK) == ImageLoaderDDS::DDST_CUBEMAP) {
		ERR_FAIL_COND_V(layer_count % 6 != 0, Ref<Resource>());

		if (dds_type & ImageLoaderDDS::DDST_ARRAY) {
			Ref<CubemapArray> texture = memnew(CubemapArray());
			texture->create_from_images(images);

			if (r_error) {
				*r_error = OK;
			}

			return texture;

		} else {
			Ref<Cubemap> texture = memnew(Cubemap());
			texture->create_from_images(images);

			if (r_error) {
				*r_error = OK;
			}

			return texture;
		}

	} else if ((dds_type & ImageLoaderDDS::DDST_TYPE_MASK) == ImageLoaderDDS::DDST_3D) {
		Ref<ImageTexture3D> texture = memnew(ImageTexture3D());
		texture->create(images[0]->get_format(), images[0]->get_width(), images[0]->get_height(), layer_count, images[0]->has_mipmaps(), images);

		if (r_error) {
			*r_error = OK;
		}

		return texture;
	}

	return Ref<Resource>();
}

void ResourceFormatDDS::get_recognized_extensions(List<String> *p_extensions) const {
	p_extensions->push_back("dds");
}

bool ResourceFormatDDS::handles_type(const String &p_type) const {
	return ClassDB::is_parent_class(p_type, "Texture");
}

String ResourceFormatDDS::get_resource_type(const String &p_path) const {
	if (p_path.get_extension().to_lower() == "dds") {
		return "Texture";
	}
	return "";
}

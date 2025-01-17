/**************************************************************************/
/*  resource_saver_qoi.cpp                                                */
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

#include "resource_saver_qoi.h"

#include "core/io/file_access.h"
#include "core/io/image.h"
#include "scene/resources/image_texture.h"

#define QOI_NO_STDIO
#include "thirdparty/misc/qoi.h"

Vector<uint8_t> ResourceSaverQOI::save_image_to_buffer(const Ref<Image> &p_img) {
	Vector<uint8_t> buffer;
	qoi_desc desc;
	desc.width = p_img->get_width();
	desc.height = p_img->get_height();
	desc.colorspace = QOI_SRGB;

	switch (p_img->get_format()) {
		case Image::Format::FORMAT_L8:
			desc.channels = 1;
			break;
		case Image::Format::FORMAT_RGB8:
			desc.channels = 3;
			break;
		case Image::Format::FORMAT_RGBA8:
			desc.channels = 4;
			break;
		default:
			ERR_FAIL_V_MSG(Vector<uint8_t>(), "Can't convert image to QOI.");
	}

	int written;
	void *data = qoi_encode(p_img->ptr(), &desc, &written);
	ERR_FAIL_NULL_V_MSG(data, Vector<uint8_t>(), "Can't convert image to QOI.");
	buffer.resize(written);
	memcpy(buffer.ptrw(), data, written);
	memfree(data);

	return buffer;
}

Error ResourceSaverQOI::save_image(const String &p_path, const Ref<Image> &p_img) {
	Error err;

	Vector<uint8_t> buffer = save_image_to_buffer(p_img);
	ERR_FAIL_COND_V_MSG(buffer.size() != 0, ERR_CANT_CREATE, "Can't convert image to QOI.");

	Ref<FileAccess> file = FileAccess::open(p_path, FileAccess::WRITE, &err);
	ERR_FAIL_COND_V_MSG(err, err, vformat("Can't save QOI at path: '%s'.", p_path));

	file->store_buffer(buffer.ptr(), buffer.size());
	if (file->get_error() != OK && file->get_error() != ERR_FILE_EOF) {
		return ERR_CANT_CREATE;
	}

	return err;
}

Error ResourceSaverQOI::save(const Ref<Resource> &p_resource, const String &p_path, uint32_t p_flags) {
	Ref<ImageTexture> texture = p_resource;

	ERR_FAIL_COND_V_MSG(texture.is_null(), ERR_INVALID_PARAMETER, "Can't save invalid texture as QOI.");
	ERR_FAIL_COND_V_MSG(!texture->get_width(), ERR_INVALID_PARAMETER, "Can't save empty texture as QOI.");

	Ref<Image> img = texture->get_image();

	Error err = save_image(p_path, img);

	return err;
}

bool ResourceSaverQOI::recognize(const Ref<Resource> &p_resource) const {
	return p_resource.is_valid() && p_resource->is_class("ImageTexture");
}

void ResourceSaverQOI::get_recognized_extensions(const Ref<Resource> &p_resource, List<String> *p_extensions) const {
	if (Object::cast_to<ImageTexture>(*p_resource)) {
		p_extensions->push_back("qoi");
	}
}

ResourceSaverQOI::ResourceSaverQOI() {
	Image::save_qoi_func = &save_image;
	Image::save_qoi_buffer_func = &save_image_to_buffer;
}

/**************************************************************************/
/*  external_texture.cpp                                                  */
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

#include "external_texture.h"

void ExternalTexture::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_size", "size"), &ExternalTexture::set_size);
	ClassDB::bind_method(D_METHOD("get_external_texture_id"), &ExternalTexture::get_external_texture_id);
	ClassDB::bind_method(D_METHOD("set_external_buffer_id", "external_buffer_id"), &ExternalTexture::set_external_buffer_id);

	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "size"), "set_size", "get_size");
}

uint64_t ExternalTexture::get_external_texture_id() const {
	_ensure_created();
	return RenderingServer::get_singleton()->texture_get_native_handle(texture);
}

void ExternalTexture::set_size(const Size2 &p_size) {
	if (p_size.width > 0 && p_size.height > 0 && p_size != size) {
		size = p_size;
		_ensure_created();
		RenderingServer::get_singleton()->texture_external_update(texture, size.width, size.height, external_buffer);
		emit_changed();
	}
}

Size2 ExternalTexture::get_size() const {
	return size;
}

void ExternalTexture::set_external_buffer_id(uint64_t p_external_buffer) {
	if (p_external_buffer != external_buffer) {
		external_buffer = p_external_buffer;
		_ensure_created();
		RenderingServer::get_singleton()->texture_external_update(texture, size.width, size.height, external_buffer);
	}
}

int ExternalTexture::get_width() const {
	return size.width;
}

int ExternalTexture::get_height() const {
	return size.height;
}

bool ExternalTexture::has_alpha() const {
	return false;
}

RID ExternalTexture::get_rid() const {
	if (!texture.is_valid()) {
		texture = RenderingServer::get_singleton()->texture_2d_placeholder_create();
		using_placeholder = true;
	}
	return texture;
}

void ExternalTexture::_ensure_created() const {
	if (texture.is_valid() && !using_placeholder) {
		return;
	}

	RID new_texture = RenderingServer::get_singleton()->texture_external_create(size.width, size.height);
	if (using_placeholder) {
		DEV_ASSERT(texture.is_valid());
		RenderingServer::get_singleton()->texture_replace(texture, new_texture);
		using_placeholder = false;
	} else {
		texture = new_texture;
	}
}

ExternalTexture::ExternalTexture() {
}

ExternalTexture::~ExternalTexture() {
	if (texture.is_valid()) {
		ERR_FAIL_NULL(RenderingServer::get_singleton());
		RenderingServer::get_singleton()->free(texture);
	}
}

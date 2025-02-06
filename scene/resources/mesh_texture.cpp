/**************************************************************************/
/*  mesh_texture.cpp                                                      */
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

#include "mesh_texture.h"

#include "scene/resources/mesh.h"

int MeshTexture::get_width() const {
	return size.width;
}

int MeshTexture::get_height() const {
	return size.height;
}

RID MeshTexture::get_rid() const {
	return texture;
}

bool MeshTexture::has_alpha() const {
	return true;
}

bool MeshTexture::is_pixel_opaque(int p_x, int p_y) const {
	Ref<Image> img = get_image();
	if (img.is_null()) {
		return true;
	}
	return img->get_pixel(p_x, p_y).a != 0;
}

Ref<Image> MeshTexture::get_image() const {
	return RS::get_singleton()->texture_2d_get(texture);
}

void MeshTexture::set_path(const String &p_path, bool p_take_over) {
	if (texture.is_valid()) {
		RS::get_singleton()->texture_set_path(texture, p_path);
	}

	Resource::set_path(p_path, p_take_over);
}

void MeshTexture::set_mesh(const Ref<Mesh> &p_mesh) {
	if (mesh.is_valid()) {
		mesh->disconnect_changed(callable_mp(this, &MeshTexture::_queue_update));
	}
	mesh = p_mesh;
	if (mesh.is_valid()) {
		mesh->connect_changed(callable_mp(this, &MeshTexture::_queue_update));
	}
	_queue_update();
}

Ref<Mesh> MeshTexture::get_mesh() const {
	return mesh;
}

void MeshTexture::set_image_size(const Size2 &p_size) {
	ERR_FAIL_COND(p_size.width <= 0 || p_size.height <= 0);
	size = p_size;
	RS::get_singleton()->viewport_set_size(viewport, size.width, size.height);
	_queue_update();
}

Size2 MeshTexture::get_image_size() const {
	return size;
}

void MeshTexture::set_base_texture(const Ref<Texture2D> &p_texture) {
	if (base_texture.is_valid()) {
		base_texture->disconnect_changed(callable_mp(this, &MeshTexture::_queue_update));
	}
	base_texture = p_texture;
	if (base_texture.is_valid()) {
		base_texture->connect_changed(callable_mp(this, &MeshTexture::_queue_update));
	}
	_queue_update();
}

Ref<Texture2D> MeshTexture::get_base_texture() const {
	return base_texture;
}

void MeshTexture::set_scale(const Size2 &p_scale) {
	scale = p_scale;
	_queue_update();
}

Size2 MeshTexture::get_scale() const {
	return scale;
}

void MeshTexture::set_offset(const Size2 &p_offset) {
	offset = p_offset;
	_queue_update();
}

Size2 MeshTexture::get_offset() const {
	return offset;
}

void MeshTexture::set_background(const Color &p_color) {
	background = p_color;
	_queue_update();
}

Color MeshTexture::get_background() const {
	return background;
}

void MeshTexture::_queue_update() {
	if (update_pending) {
		return;
	}
	update_pending = true;
	callable_mp(this, &MeshTexture::_update_viewport_texture).call_deferred();
}

void MeshTexture::_update_viewport_texture() {
	RS::get_singleton()->canvas_item_clear(canvas_item);
	RS::get_singleton()->canvas_item_add_rect(canvas_item, Rect2(Vector2(), size), background);
	if (mesh.is_valid() && base_texture.is_valid()) {
		Transform2D xform;
		Vector3 mesh_size = mesh->get_aabb().get_size();
		float mesh_scale = MAX(mesh_size.x, mesh_size.y);
		xform.set_scale(size / mesh_scale * scale);
		xform.set_origin(size / 2 + offset);
		RS::get_singleton()->canvas_item_add_mesh(canvas_item, mesh->get_rid(), xform, Color(1, 1, 1), base_texture->get_rid());
	}
	RS::get_singleton()->viewport_set_active(viewport, true);
	RS::get_singleton()->draw(false);
	RS::get_singleton()->viewport_set_active(viewport, false);
	emit_changed();
	update_pending = false;
}

void MeshTexture::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_mesh", "mesh"), &MeshTexture::set_mesh);
	ClassDB::bind_method(D_METHOD("get_mesh"), &MeshTexture::get_mesh);
	ClassDB::bind_method(D_METHOD("set_image_size", "size"), &MeshTexture::set_image_size);
	ClassDB::bind_method(D_METHOD("get_image_size"), &MeshTexture::get_image_size);
	ClassDB::bind_method(D_METHOD("set_base_texture", "texture"), &MeshTexture::set_base_texture);
	ClassDB::bind_method(D_METHOD("get_base_texture"), &MeshTexture::get_base_texture);
	ClassDB::bind_method(D_METHOD("set_scale", "scale"), &MeshTexture::set_scale);
	ClassDB::bind_method(D_METHOD("get_scale"), &MeshTexture::get_scale);
	ClassDB::bind_method(D_METHOD("set_offset", "offset"), &MeshTexture::set_offset);
	ClassDB::bind_method(D_METHOD("get_offset"), &MeshTexture::get_offset);
	ClassDB::bind_method(D_METHOD("set_background", "enable"), &MeshTexture::set_background);
	ClassDB::bind_method(D_METHOD("get_background"), &MeshTexture::get_background);

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "mesh", PROPERTY_HINT_RESOURCE_TYPE, "Mesh"), "set_mesh", "get_mesh");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "base_texture", PROPERTY_HINT_RESOURCE_TYPE, "Texture2D"), "set_base_texture", "get_base_texture");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "image_size", PROPERTY_HINT_RANGE, "1,2048,1,or_greater,hide_slider,suffix:px"), "set_image_size", "get_image_size");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "scale", PROPERTY_HINT_RANGE, "-1,1,0.01,or_greater,or_less,hide_slider"), "set_scale", "get_scale");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "offset", PROPERTY_HINT_RANGE, "-512,512,0.1,or_greater,or_less,hide_slider,suffix:px"), "set_offset", "get_offset");
	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "background"), "set_background", "get_background");
}

MeshTexture::MeshTexture() {
	canvas_item = RS::get_singleton()->canvas_item_create();
	canvas = RS::get_singleton()->canvas_create();
	viewport = RS::get_singleton()->viewport_create();
	RS::get_singleton()->canvas_item_set_parent(canvas_item, canvas);
	RS::get_singleton()->viewport_set_disable_3d(viewport, true);
	RS::get_singleton()->viewport_attach_canvas(viewport, canvas);
	RS::get_singleton()->viewport_set_transparent_background(viewport, true);
	RS::get_singleton()->viewport_set_update_mode(viewport, RS::VIEWPORT_UPDATE_ALWAYS);
	RS::get_singleton()->viewport_set_size(viewport, size.width, size.height);

	RS::get_singleton()->canvas_item_add_rect(canvas_item, Rect2(Vector2(), size), background);
	RS::get_singleton()->viewport_set_active(viewport, true);
	RS::get_singleton()->draw(false);
	RS::get_singleton()->viewport_set_active(viewport, false);

	texture = RS::get_singleton()->viewport_get_texture(viewport);
}

MeshTexture::~MeshTexture() {
	RS::get_singleton()->free(viewport);
	RS::get_singleton()->free(canvas);
	RS::get_singleton()->free(canvas_item);
}

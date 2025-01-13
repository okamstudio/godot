/**************************************************************************/
/*  sprite_3d.cpp                                                         */
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

#include "sprite_3d.h"

#include "scene/resources/atlas_texture.h"

Color SpriteBase3D::_get_color_accum() {
	if (!color_dirty) {
		return color_accum;
	}

	if (parent_sprite) {
		color_accum = parent_sprite->_get_color_accum();
	} else {
		color_accum = Color(1, 1, 1, 1);
	}

	color_accum.r *= modulate.r;
	color_accum.g *= modulate.g;
	color_accum.b *= modulate.b;
	color_accum.a *= modulate.a;
	color_dirty = false;
	return color_accum;
}

void SpriteBase3D::_propagate_color_changed() {
	if (color_dirty) {
		return;
	}

	color_dirty = true;
	_queue_redraw();

	for (SpriteBase3D *&E : children) {
		E->_propagate_color_changed();
	}
}

void SpriteBase3D::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			if (!pending_update) {
				_im_update();
			}

			parent_sprite = Object::cast_to<SpriteBase3D>(get_parent());
			if (parent_sprite) {
				pI = parent_sprite->children.push_back(this);
			}
		} break;

		case NOTIFICATION_EXIT_TREE: {
			if (parent_sprite) {
				parent_sprite->children.erase(pI);
				pI = nullptr;
				parent_sprite = nullptr;
			}
		} break;
	}
}

void SpriteBase3D::draw_texture_rect(Ref<Texture2D> p_texture, Rect2 p_dst_rect, Rect2 p_src_rect) {
	ERR_FAIL_COND(p_texture.is_null());

	Rect2 final_rect;
	Rect2 final_src_rect;
	if (!p_texture->get_rect_region(p_dst_rect, p_src_rect, final_rect, final_src_rect)) {
		return;
	}

	if (final_rect.size.x == 0 || final_rect.size.y == 0) {
		return;
	}

	// 2D:                                                     3D plane (axes match exactly when `axis == Vector3::AXIS_Z`):
	//   -X+                                                     -X+
	//  -                                                       +
	//  Y  +--------+       +--------+       +--------+         Y  +--------+
	//  +  | +--+   |       |        |  (2)  |        |         -  | 0--1   |
	//     | |ab|   |  (1)  | +--+   |  (3)  | 3--2   |            | |ab|   |
	//     | |cd|   |  -->  | |ab|   |  -->  | |cd|   |    <==>    | |cd|   |
	//     | +--+   |       | |cd|   |       | |ab|   |            | 3--2   |
	//     |        |       | +--+   |       | 0--1   |            |        |
	//     +--------+       +--------+       +--------+            +--------+

	// (1) Y-wise shift `final_rect` within `p_dst_rect` so after inverting Y
	// axis distances between top/bottom borders will be preserved (so for
	// example AtlasTextures with vertical margins will look the same in 2D/3D).
	final_rect.position.y = (p_dst_rect.position.y + p_dst_rect.size.y) - ((final_rect.position.y + final_rect.size.y) - p_dst_rect.position.y);

	Color color = _get_color_accum();

	real_t px_size = get_pixel_size();

	// (2) Order vertices (0123) bottom-top in 2D / top-bottom in 3D.
	Vector2 vertices[4] = {
		(final_rect.position + Vector2(0, final_rect.size.y)) * px_size,
		(final_rect.position + final_rect.size) * px_size,
		(final_rect.position + Vector2(final_rect.size.x, 0)) * px_size,
		final_rect.position * px_size,
	};

	Vector2 src_tsize = p_texture->get_size();

	// Properly setup UVs for impostor textures (AtlasTexture).
	Ref<AtlasTexture> atlas_tex = p_texture;
	if (atlas_tex.is_valid()) {
		src_tsize[0] = atlas_tex->get_atlas()->get_width();
		src_tsize[1] = atlas_tex->get_atlas()->get_height();
	}

	// (3) Assign UVs (abcd) according to the vertices order (bottom-top in 2D / top-bottom in 3D).
	Vector2 uvs[4] = {
		final_src_rect.position / src_tsize,
		(final_src_rect.position + Vector2(final_src_rect.size.x, 0)) / src_tsize,
		(final_src_rect.position + final_src_rect.size) / src_tsize,
		(final_src_rect.position + Vector2(0, final_src_rect.size.y)) / src_tsize,
	};

	if (is_flipped_h()) {
		SWAP(uvs[0], uvs[1]);
		SWAP(uvs[2], uvs[3]);
	}

	if (is_flipped_v()) {
		SWAP(uvs[0], uvs[3]);
		SWAP(uvs[1], uvs[2]);
	}

	Vector3 normal;
	int ax = get_axis();
	normal[ax] = 1.0;

	Plane tangent;
	if (ax == Vector3::AXIS_X) {
		tangent = Plane(0, 0, -1, 1);
	} else {
		tangent = Plane(1, 0, 0, 1);
	}

	int x_axis = ((ax + 1) % 3);
	int y_axis = ((ax + 2) % 3);

	if (ax != Vector3::AXIS_Z) {
		SWAP(x_axis, y_axis);

		for (int i = 0; i < 4; i++) {
			//uvs[i] = Vector2(1.0,1.0)-uvs[i];
			//SWAP(vertices[i].x,vertices[i].y);
			if (ax == Vector3::AXIS_Y) {
				vertices[i].y = -vertices[i].y;
			} else if (ax == Vector3::AXIS_X) {
				vertices[i].x = -vertices[i].x;
			}
		}
	}

	AABB aabb_new;

	// Everything except position and UV is compressed.
	uint8_t *vertex_write_buffer = vertex_buffer.ptrw();
	uint8_t *attribute_write_buffer = attribute_buffer.ptrw();

	uint32_t v_normal;
	{
		Vector2 res = normal.octahedron_encode();
		uint32_t value = 0;
		value |= (uint16_t)CLAMP(res.x * 65535, 0, 65535);
		value |= (uint16_t)CLAMP(res.y * 65535, 0, 65535) << 16;

		v_normal = value;
	}
	uint32_t v_tangent;
	{
		Plane t = tangent;
		Vector2 res = t.normal.octahedron_tangent_encode(t.d);
		uint32_t value = 0;
		value |= (uint16_t)CLAMP(res.x * 65535, 0, 65535);
		value |= (uint16_t)CLAMP(res.y * 65535, 0, 65535) << 16;
		if (value == 4294901760) {
			// (1, 1) and (0, 1) decode to the same value, but (0, 1) messes with our compression detection.
			// So we sanitize here.
			value = 4294967295;
		}

		v_tangent = value;
	}

	uint8_t v_color[4] = {
		uint8_t(CLAMP(color.r * 255.0, 0.0, 255.0)),
		uint8_t(CLAMP(color.g * 255.0, 0.0, 255.0)),
		uint8_t(CLAMP(color.b * 255.0, 0.0, 255.0)),
		uint8_t(CLAMP(color.a * 255.0, 0.0, 255.0))
	};

	for (int i = 0; i < 4; i++) {
		Vector3 vtx;
		vtx[x_axis] = vertices[i][0];
		vtx[y_axis] = vertices[i][1];
		if (i == 0) {
			aabb_new.position = vtx;
			aabb_new.size = Vector3();
		} else {
			aabb_new.expand_to(vtx);
		}

		float v_uv[2] = { (float)uvs[i].x, (float)uvs[i].y };
		memcpy(&attribute_write_buffer[i * attrib_stride + mesh_surface_offsets[RS::ARRAY_TEX_UV]], v_uv, 8);

		float v_vertex[3] = { (float)vtx.x, (float)vtx.y, (float)vtx.z };

		memcpy(&vertex_write_buffer[i * vertex_stride + mesh_surface_offsets[RS::ARRAY_VERTEX]], &v_vertex, sizeof(float) * 3);
		memcpy(&vertex_write_buffer[i * normal_tangent_stride + mesh_surface_offsets[RS::ARRAY_NORMAL]], &v_normal, 4);
		memcpy(&vertex_write_buffer[i * normal_tangent_stride + mesh_surface_offsets[RS::ARRAY_TANGENT]], &v_tangent, 4);
		memcpy(&attribute_write_buffer[i * attrib_stride + mesh_surface_offsets[RS::ARRAY_COLOR]], v_color, 4);
	}

	RID mesh_new = get_mesh();
	RS::get_singleton()->mesh_surface_update_vertex_region(mesh_new, 0, 0, vertex_buffer);
	RS::get_singleton()->mesh_surface_update_attribute_region(mesh_new, 0, 0, attribute_buffer);

	RS::get_singleton()->mesh_set_custom_aabb(mesh_new, aabb_new);
	set_aabb(aabb_new);

	RS::get_singleton()->material_set_param(get_material(), "alpha_scissor_threshold", alpha_scissor_threshold);
	RS::get_singleton()->material_set_param(get_material(), "alpha_hash_scale", alpha_hash_scale);
	RS::get_singleton()->material_set_param(get_material(), "alpha_antialiasing_edge", alpha_antialiasing_edge);

	BaseMaterial3D::Transparency mat_transparency = BaseMaterial3D::Transparency::TRANSPARENCY_DISABLED;
	if (get_draw_flag(FLAG_TRANSPARENT)) {
		if (get_alpha_cut_mode() == ALPHA_CUT_DISCARD) {
			mat_transparency = BaseMaterial3D::Transparency::TRANSPARENCY_ALPHA_SCISSOR;
		} else if (get_alpha_cut_mode() == ALPHA_CUT_OPAQUE_PREPASS) {
			mat_transparency = BaseMaterial3D::Transparency::TRANSPARENCY_ALPHA_DEPTH_PRE_PASS;
		} else if (get_alpha_cut_mode() == ALPHA_CUT_HASH) {
			mat_transparency = BaseMaterial3D::Transparency::TRANSPARENCY_ALPHA_HASH;
		} else {
			mat_transparency = BaseMaterial3D::Transparency::TRANSPARENCY_ALPHA;
		}
	}

	RID shader_rid;
	StandardMaterial3D::get_material_for_2d(get_draw_flag(FLAG_SHADED), mat_transparency, get_draw_flag(FLAG_DOUBLE_SIDED), get_billboard_mode() == StandardMaterial3D::BILLBOARD_ENABLED, get_billboard_mode() == StandardMaterial3D::BILLBOARD_FIXED_Y, false, get_draw_flag(FLAG_DISABLE_DEPTH_TEST), get_draw_flag(FLAG_FIXED_SIZE), get_texture_filter(), alpha_antialiasing_mode, &shader_rid);

	if (last_shader != shader_rid) {
		RS::get_singleton()->material_set_shader(get_material(), shader_rid);
		last_shader = shader_rid;
	}
	if (last_texture != p_texture->get_rid()) {
		RS::get_singleton()->material_set_param(get_material(), "texture_albedo", p_texture->get_rid());
		last_texture = p_texture->get_rid();
	}
	if (get_alpha_cut_mode() == ALPHA_CUT_DISABLED) {
		RS::get_singleton()->material_set_render_priority(get_material(), get_render_priority());
		RS::get_singleton()->mesh_surface_set_material(mesh, 0, get_material());
	}
}

void SpriteBase3D::set_centered(bool p_center) {
	if (centered == p_center) {
		return;
	}

	centered = p_center;
	_queue_redraw();
}

bool SpriteBase3D::is_centered() const {
	return centered;
}

void SpriteBase3D::set_offset(const Point2 &p_offset) {
	if (offset == p_offset) {
		return;
	}

	offset = p_offset;
	_queue_redraw();
}

Point2 SpriteBase3D::get_offset() const {
	return offset;
}

void SpriteBase3D::set_flip_h(bool p_flip) {
	if (hflip == p_flip) {
		return;
	}

	hflip = p_flip;
	_queue_redraw();
}

bool SpriteBase3D::is_flipped_h() const {
	return hflip;
}

void SpriteBase3D::set_flip_v(bool p_flip) {
	if (vflip == p_flip) {
		return;
	}

	vflip = p_flip;
	_queue_redraw();
}

bool SpriteBase3D::is_flipped_v() const {
	return vflip;
}

void SpriteBase3D::set_modulate(const Color &p_color) {
	if (modulate == p_color) {
		return;
	}

	modulate = p_color;
	_propagate_color_changed();
	_queue_redraw();
}

Color SpriteBase3D::get_modulate() const {
	return modulate;
}

void SpriteBase3D::set_render_priority(int p_priority) {
	ERR_FAIL_COND(p_priority < RS::MATERIAL_RENDER_PRIORITY_MIN || p_priority > RS::MATERIAL_RENDER_PRIORITY_MAX);

	if (render_priority == p_priority) {
		return;
	}

	render_priority = p_priority;
	_queue_redraw();
}

int SpriteBase3D::get_render_priority() const {
	return render_priority;
}

void SpriteBase3D::set_pixel_size(real_t p_amount) {
	if (pixel_size == p_amount) {
		return;
	}

	pixel_size = p_amount;
	_queue_redraw();
}

real_t SpriteBase3D::get_pixel_size() const {
	return pixel_size;
}

void SpriteBase3D::set_axis(Vector3::Axis p_axis) {
	ERR_FAIL_INDEX(p_axis, 3);

	if (axis == p_axis) {
		return;
	}

	axis = p_axis;
	_queue_redraw();
}

Vector3::Axis SpriteBase3D::get_axis() const {
	return axis;
}

void SpriteBase3D::_im_update() {
	_draw();

	pending_update = false;

	//texture->draw_rect_region(ci,dst_rect,src_rect,modulate);
}

void SpriteBase3D::_queue_redraw() {
	// The 3D equivalent of CanvasItem.queue_redraw().
	if (pending_update) {
		return;
	}

	triangle_mesh.unref();
	update_gizmos();

	pending_update = true;
	callable_mp(this, &SpriteBase3D::_im_update).call_deferred();
}

AABB SpriteBase3D::get_aabb() const {
	return aabb;
}

Ref<TriangleMesh> SpriteBase3D::generate_triangle_mesh() const {
	if (triangle_mesh.is_valid()) {
		return triangle_mesh;
	}

	Vector<Vector3> faces;
	faces.resize(6);
	Vector3 *facesw = faces.ptrw();

	Rect2 final_rect = get_item_rect();

	if (final_rect.size.x == 0 || final_rect.size.y == 0) {
		return Ref<TriangleMesh>();
	}

	real_t px_size = get_pixel_size();

	Vector2 vertices[4] = {
		(final_rect.position + Vector2(0, final_rect.size.y)) * px_size,
		(final_rect.position + final_rect.size) * px_size,
		(final_rect.position + Vector2(final_rect.size.x, 0)) * px_size,
		final_rect.position * px_size,
	};

	int x_axis = ((axis + 1) % 3);
	int y_axis = ((axis + 2) % 3);

	if (axis != Vector3::AXIS_Z) {
		SWAP(x_axis, y_axis);

		for (int i = 0; i < 4; i++) {
			if (axis == Vector3::AXIS_Y) {
				vertices[i].y = -vertices[i].y;
			} else if (axis == Vector3::AXIS_X) {
				vertices[i].x = -vertices[i].x;
			}
		}
	}

	static const int indices[6] = {
		0, 1, 2,
		0, 2, 3
	};

	for (int j = 0; j < 6; j++) {
		int i = indices[j];
		Vector3 vtx;
		vtx[x_axis] = vertices[i][0];
		vtx[y_axis] = vertices[i][1];
		facesw[j] = vtx;
	}

	triangle_mesh.instantiate();
	triangle_mesh->create(faces);

	return triangle_mesh;
}

void SpriteBase3D::set_draw_flag(DrawFlags p_flag, bool p_enable) {
	ERR_FAIL_INDEX(p_flag, FLAG_MAX);

	if (flags[p_flag] == p_enable) {
		return;
	}

	flags[p_flag] = p_enable;
	_queue_redraw();
}

bool SpriteBase3D::get_draw_flag(DrawFlags p_flag) const {
	ERR_FAIL_INDEX_V(p_flag, FLAG_MAX, false);
	return flags[p_flag];
}

void SpriteBase3D::set_alpha_cut_mode(AlphaCutMode p_mode) {
	ERR_FAIL_INDEX(p_mode, ALPHA_CUT_MAX);

	if (alpha_cut == p_mode) {
		return;
	}

	alpha_cut = p_mode;
	_queue_redraw();
}

SpriteBase3D::AlphaCutMode SpriteBase3D::get_alpha_cut_mode() const {
	return alpha_cut;
}

void SpriteBase3D::set_alpha_hash_scale(float p_hash_scale) {
	if (alpha_hash_scale == p_hash_scale) {
		return;
	}

	alpha_hash_scale = p_hash_scale;
	_queue_redraw();
}

float SpriteBase3D::get_alpha_hash_scale() const {
	return alpha_hash_scale;
}

void SpriteBase3D::set_alpha_scissor_threshold(float p_threshold) {
	if (alpha_scissor_threshold == p_threshold) {
		return;
	}

	alpha_scissor_threshold = p_threshold;
	_queue_redraw();
}

float SpriteBase3D::get_alpha_scissor_threshold() const {
	return alpha_scissor_threshold;
}

void SpriteBase3D::set_alpha_antialiasing(BaseMaterial3D::AlphaAntiAliasing p_alpha_aa) {
	if (alpha_antialiasing_mode == p_alpha_aa) {
		return;
	}

	alpha_antialiasing_mode = p_alpha_aa;
	_queue_redraw();
}

BaseMaterial3D::AlphaAntiAliasing SpriteBase3D::get_alpha_antialiasing() const {
	return alpha_antialiasing_mode;
}

void SpriteBase3D::set_alpha_antialiasing_edge(float p_edge) {
	if (alpha_antialiasing_edge == p_edge) {
		return;
	}

	alpha_antialiasing_edge = p_edge;
	_queue_redraw();
}

float SpriteBase3D::get_alpha_antialiasing_edge() const {
	return alpha_antialiasing_edge;
}

void SpriteBase3D::set_billboard_mode(StandardMaterial3D::BillboardMode p_mode) {
	ERR_FAIL_INDEX(p_mode, 3); // Cannot use BILLBOARD_PARTICLES.

	if (billboard_mode == p_mode) {
		return;
	}

	billboard_mode = p_mode;
	_queue_redraw();
}

StandardMaterial3D::BillboardMode SpriteBase3D::get_billboard_mode() const {
	return billboard_mode;
}

void SpriteBase3D::set_texture_filter(StandardMaterial3D::TextureFilter p_filter) {
	if (texture_filter == p_filter) {
		return;
	}

	texture_filter = p_filter;
	_queue_redraw();
}

StandardMaterial3D::TextureFilter SpriteBase3D::get_texture_filter() const {
	return texture_filter;
}

void SpriteBase3D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_centered", "centered"), &SpriteBase3D::set_centered);
	ClassDB::bind_method(D_METHOD("is_centered"), &SpriteBase3D::is_centered);

	ClassDB::bind_method(D_METHOD("set_offset", "offset"), &SpriteBase3D::set_offset);
	ClassDB::bind_method(D_METHOD("get_offset"), &SpriteBase3D::get_offset);

	ClassDB::bind_method(D_METHOD("set_flip_h", "flip_h"), &SpriteBase3D::set_flip_h);
	ClassDB::bind_method(D_METHOD("is_flipped_h"), &SpriteBase3D::is_flipped_h);

	ClassDB::bind_method(D_METHOD("set_flip_v", "flip_v"), &SpriteBase3D::set_flip_v);
	ClassDB::bind_method(D_METHOD("is_flipped_v"), &SpriteBase3D::is_flipped_v);

	ClassDB::bind_method(D_METHOD("set_modulate", "modulate"), &SpriteBase3D::set_modulate);
	ClassDB::bind_method(D_METHOD("get_modulate"), &SpriteBase3D::get_modulate);

	ClassDB::bind_method(D_METHOD("set_render_priority", "priority"), &SpriteBase3D::set_render_priority);
	ClassDB::bind_method(D_METHOD("get_render_priority"), &SpriteBase3D::get_render_priority);

	ClassDB::bind_method(D_METHOD("set_pixel_size", "pixel_size"), &SpriteBase3D::set_pixel_size);
	ClassDB::bind_method(D_METHOD("get_pixel_size"), &SpriteBase3D::get_pixel_size);

	ClassDB::bind_method(D_METHOD("set_axis", "axis"), &SpriteBase3D::set_axis);
	ClassDB::bind_method(D_METHOD("get_axis"), &SpriteBase3D::get_axis);

	ClassDB::bind_method(D_METHOD("set_draw_flag", "flag", "enabled"), &SpriteBase3D::set_draw_flag);
	ClassDB::bind_method(D_METHOD("get_draw_flag", "flag"), &SpriteBase3D::get_draw_flag);

	ClassDB::bind_method(D_METHOD("set_alpha_cut_mode", "mode"), &SpriteBase3D::set_alpha_cut_mode);
	ClassDB::bind_method(D_METHOD("get_alpha_cut_mode"), &SpriteBase3D::get_alpha_cut_mode);

	ClassDB::bind_method(D_METHOD("set_alpha_scissor_threshold", "threshold"), &SpriteBase3D::set_alpha_scissor_threshold);
	ClassDB::bind_method(D_METHOD("get_alpha_scissor_threshold"), &SpriteBase3D::get_alpha_scissor_threshold);

	ClassDB::bind_method(D_METHOD("set_alpha_hash_scale", "threshold"), &SpriteBase3D::set_alpha_hash_scale);
	ClassDB::bind_method(D_METHOD("get_alpha_hash_scale"), &SpriteBase3D::get_alpha_hash_scale);

	ClassDB::bind_method(D_METHOD("set_alpha_antialiasing", "alpha_aa"), &SpriteBase3D::set_alpha_antialiasing);
	ClassDB::bind_method(D_METHOD("get_alpha_antialiasing"), &SpriteBase3D::get_alpha_antialiasing);

	ClassDB::bind_method(D_METHOD("set_alpha_antialiasing_edge", "edge"), &SpriteBase3D::set_alpha_antialiasing_edge);
	ClassDB::bind_method(D_METHOD("get_alpha_antialiasing_edge"), &SpriteBase3D::get_alpha_antialiasing_edge);

	ClassDB::bind_method(D_METHOD("set_billboard_mode", "mode"), &SpriteBase3D::set_billboard_mode);
	ClassDB::bind_method(D_METHOD("get_billboard_mode"), &SpriteBase3D::get_billboard_mode);

	ClassDB::bind_method(D_METHOD("set_texture_filter", "mode"), &SpriteBase3D::set_texture_filter);
	ClassDB::bind_method(D_METHOD("get_texture_filter"), &SpriteBase3D::get_texture_filter);

	ClassDB::bind_method(D_METHOD("get_item_rect"), &SpriteBase3D::get_item_rect);
	ClassDB::bind_method(D_METHOD("generate_triangle_mesh"), &SpriteBase3D::generate_triangle_mesh);

	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "centered"), "set_centered", "is_centered");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "offset", PROPERTY_HINT_NONE, "suffix:px"), "set_offset", "get_offset");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "flip_h"), "set_flip_h", "is_flipped_h");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "flip_v"), "set_flip_v", "is_flipped_v");
	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "modulate"), "set_modulate", "get_modulate");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "pixel_size", PROPERTY_HINT_RANGE, "0.0001,128,0.0001,suffix:m"), "set_pixel_size", "get_pixel_size");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "axis", PROPERTY_HINT_ENUM, "X-Axis,Y-Axis,Z-Axis"), "set_axis", "get_axis");
	ADD_GROUP("Flags", "");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "billboard", PROPERTY_HINT_ENUM, "Disabled,Enabled,Y-Billboard"), "set_billboard_mode", "get_billboard_mode");
	ADD_PROPERTYI(PropertyInfo(Variant::BOOL, "transparent"), "set_draw_flag", "get_draw_flag", FLAG_TRANSPARENT);
	ADD_PROPERTYI(PropertyInfo(Variant::BOOL, "shaded"), "set_draw_flag", "get_draw_flag", FLAG_SHADED);
	ADD_PROPERTYI(PropertyInfo(Variant::BOOL, "double_sided"), "set_draw_flag", "get_draw_flag", FLAG_DOUBLE_SIDED);
	ADD_PROPERTYI(PropertyInfo(Variant::BOOL, "no_depth_test"), "set_draw_flag", "get_draw_flag", FLAG_DISABLE_DEPTH_TEST);
	ADD_PROPERTYI(PropertyInfo(Variant::BOOL, "fixed_size"), "set_draw_flag", "get_draw_flag", FLAG_FIXED_SIZE);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "alpha_cut", PROPERTY_HINT_ENUM, "Disabled,Discard,Opaque Pre-Pass,Alpha Hash"), "set_alpha_cut_mode", "get_alpha_cut_mode");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "alpha_scissor_threshold", PROPERTY_HINT_RANGE, "0,1,0.001"), "set_alpha_scissor_threshold", "get_alpha_scissor_threshold");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "alpha_hash_scale", PROPERTY_HINT_RANGE, "0,2,0.01"), "set_alpha_hash_scale", "get_alpha_hash_scale");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "alpha_antialiasing_mode", PROPERTY_HINT_ENUM, "Disabled,Alpha Edge Blend,Alpha Edge Clip"), "set_alpha_antialiasing", "get_alpha_antialiasing");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "alpha_antialiasing_edge", PROPERTY_HINT_RANGE, "0,1,0.01"), "set_alpha_antialiasing_edge", "get_alpha_antialiasing_edge");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "texture_filter", PROPERTY_HINT_ENUM, "Nearest,Linear,Nearest Mipmap,Linear Mipmap,Nearest Mipmap Anisotropic,Linear Mipmap Anisotropic"), "set_texture_filter", "get_texture_filter");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "render_priority", PROPERTY_HINT_RANGE, itos(RS::MATERIAL_RENDER_PRIORITY_MIN) + "," + itos(RS::MATERIAL_RENDER_PRIORITY_MAX) + ",1"), "set_render_priority", "get_render_priority");

	BIND_ENUM_CONSTANT(FLAG_TRANSPARENT);
	BIND_ENUM_CONSTANT(FLAG_SHADED);
	BIND_ENUM_CONSTANT(FLAG_DOUBLE_SIDED);
	BIND_ENUM_CONSTANT(FLAG_DISABLE_DEPTH_TEST);
	BIND_ENUM_CONSTANT(FLAG_FIXED_SIZE);
	BIND_ENUM_CONSTANT(FLAG_MAX);

	BIND_ENUM_CONSTANT(ALPHA_CUT_DISABLED);
	BIND_ENUM_CONSTANT(ALPHA_CUT_DISCARD);
	BIND_ENUM_CONSTANT(ALPHA_CUT_OPAQUE_PREPASS);
	BIND_ENUM_CONSTANT(ALPHA_CUT_HASH);
}

SpriteBase3D::SpriteBase3D() {
	for (int i = 0; i < FLAG_MAX; i++) {
		flags[i] = i == FLAG_TRANSPARENT || i == FLAG_DOUBLE_SIDED;
	}

	material = RenderingServer::get_singleton()->material_create();
	// Set defaults for material, names need to match up those in StandardMaterial3D.
	RS::get_singleton()->material_set_param(material, "albedo", Color(1, 1, 1, 1));
	RS::get_singleton()->material_set_param(material, "specular", 0.5);
	RS::get_singleton()->material_set_param(material, "metallic", 0.0);
	RS::get_singleton()->material_set_param(material, "roughness", 1.0);
	RS::get_singleton()->material_set_param(material, "uv1_offset", Vector3(0, 0, 0));
	RS::get_singleton()->material_set_param(material, "uv1_scale", Vector3(1, 1, 1));
	RS::get_singleton()->material_set_param(material, "uv2_offset", Vector3(0, 0, 0));
	RS::get_singleton()->material_set_param(material, "uv2_scale", Vector3(1, 1, 1));

	mesh = RenderingServer::get_singleton()->mesh_create();

	PackedVector3Array mesh_vertices;
	PackedVector3Array mesh_normals;
	PackedFloat32Array mesh_tangents;
	PackedColorArray mesh_colors;
	PackedVector2Array mesh_uvs;
	PackedInt32Array indices;

	mesh_vertices.resize(4);
	mesh_normals.resize(4);
	mesh_tangents.resize(16);
	mesh_colors.resize(4);
	mesh_uvs.resize(4);

	// Create basic mesh and store format information.
	for (int i = 0; i < 4; i++) {
		mesh_normals.write[i] = Vector3(0.0, 0.0, 1.0);
		mesh_tangents.write[i * 4 + 0] = 1.0;
		mesh_tangents.write[i * 4 + 1] = 0.0;
		mesh_tangents.write[i * 4 + 2] = 0.0;
		mesh_tangents.write[i * 4 + 3] = 1.0;
		mesh_colors.write[i] = Color(1.0, 1.0, 1.0, 1.0);
		mesh_uvs.write[i] = Vector2(0.0, 0.0);
		mesh_vertices.write[i] = Vector3(0.0, 0.0, 0.0);
	}

	indices.resize(6);
	indices.write[0] = 0;
	indices.write[1] = 1;
	indices.write[2] = 2;
	indices.write[3] = 0;
	indices.write[4] = 2;
	indices.write[5] = 3;

	Array mesh_array;
	mesh_array.resize(RS::ARRAY_MAX);
	mesh_array[RS::ARRAY_VERTEX] = mesh_vertices;
	mesh_array[RS::ARRAY_NORMAL] = mesh_normals;
	mesh_array[RS::ARRAY_TANGENT] = mesh_tangents;
	mesh_array[RS::ARRAY_COLOR] = mesh_colors;
	mesh_array[RS::ARRAY_TEX_UV] = mesh_uvs;
	mesh_array[RS::ARRAY_INDEX] = indices;

	RS::SurfaceData sd;
	RS::get_singleton()->mesh_create_surface_data_from_arrays(&sd, RS::PRIMITIVE_TRIANGLES, mesh_array);

	mesh_surface_format = sd.format;
	vertex_buffer = sd.vertex_data;
	attribute_buffer = sd.attribute_data;

	sd.material = material;

	RS::get_singleton()->mesh_surface_make_offsets_from_format(sd.format, sd.vertex_count, sd.index_count, mesh_surface_offsets, vertex_stride, normal_tangent_stride, attrib_stride, skin_stride);
	RS::get_singleton()->mesh_add_surface(mesh, sd);
	set_base(mesh);
}

SpriteBase3D::~SpriteBase3D() {
	ERR_FAIL_NULL(RenderingServer::get_singleton());
	RenderingServer::get_singleton()->free(mesh);
	RenderingServer::get_singleton()->free(material);
}

///////////////////////////////////////////

void Sprite3D::_draw() {
	if (get_base() != get_mesh()) {
		set_base(get_mesh());
	}
	if (texture.is_null()) {
		set_base(RID());
		return;
	}
	Vector2 tsize = texture->get_size();
	if (tsize.x == 0 || tsize.y == 0) {
		return;
	}

	Rect2 base_rect;
	if (region) {
		base_rect = region_rect;
	} else {
		base_rect = Rect2(0, 0, texture->get_width(), texture->get_height());
	}

	Size2 frame_size = base_rect.size / Size2(hframes, vframes);
	Point2 frame_offset = Point2(frame % hframes, frame / hframes) * frame_size;

	Point2 dst_offset = get_offset();
	if (is_centered()) {
		dst_offset -= frame_size / 2.0f;
	}

	Rect2 src_rect(base_rect.position + frame_offset, frame_size);
	Rect2 dst_rect(dst_offset, frame_size);

	draw_texture_rect(texture, dst_rect, src_rect);
}

void Sprite3D::set_texture(const Ref<Texture2D> &p_texture) {
	if (p_texture == texture) {
		return;
	}
	if (texture.is_valid()) {
		texture->disconnect(CoreStringName(changed), callable_mp((SpriteBase3D *)this, &Sprite3D::_queue_redraw));
	}
	texture = p_texture;
	if (texture.is_valid()) {
		texture->connect(CoreStringName(changed), callable_mp((SpriteBase3D *)this, &Sprite3D::_queue_redraw));
	}

	_queue_redraw();
	emit_signal(SceneStringName(texture_changed));
}

Ref<Texture2D> Sprite3D::get_texture() const {
	return texture;
}

void Sprite3D::set_region_enabled(bool p_region) {
	if (p_region == region) {
		return;
	}

	region = p_region;
	_queue_redraw();
	notify_property_list_changed();
}

bool Sprite3D::is_region_enabled() const {
	return region;
}

void Sprite3D::set_region_rect(const Rect2 &p_region_rect) {
	if (region_rect == p_region_rect) {
		return;
	}

	region_rect = p_region_rect;
	if (region) {
		_queue_redraw();
	}
}

Rect2 Sprite3D::get_region_rect() const {
	return region_rect;
}

void Sprite3D::set_frame(int p_frame) {
	ERR_FAIL_INDEX(p_frame, int64_t(vframes) * hframes);

	if (frame == p_frame) {
		return;
	}

	frame = p_frame;
	_queue_redraw();
	emit_signal(SceneStringName(frame_changed));
}

int Sprite3D::get_frame() const {
	return frame;
}

void Sprite3D::set_frame_coords(const Vector2i &p_coord) {
	ERR_FAIL_INDEX(p_coord.x, hframes);
	ERR_FAIL_INDEX(p_coord.y, vframes);

	set_frame(p_coord.y * hframes + p_coord.x);
}

Vector2i Sprite3D::get_frame_coords() const {
	return Vector2i(frame % hframes, frame / hframes);
}

void Sprite3D::set_vframes(int p_amount) {
	ERR_FAIL_COND_MSG(p_amount < 1, "Amount of vframes cannot be smaller than 1.");

	if (vframes == p_amount) {
		return;
	}

	vframes = p_amount;
	if (frame >= vframes * hframes) {
		frame = 0;
	}
	_queue_redraw();
	notify_property_list_changed();
}

int Sprite3D::get_vframes() const {
	return vframes;
}

void Sprite3D::set_hframes(int p_amount) {
	ERR_FAIL_COND_MSG(p_amount < 1, "Amount of hframes cannot be smaller than 1.");

	if (hframes == p_amount) {
		return;
	}

	if (vframes > 1) {
		// Adjust the frame to fit new sheet dimensions.
		int original_column = frame % hframes;
		if (original_column >= p_amount) {
			// Frame's column was dropped, reset.
			frame = 0;
		} else {
			int original_row = frame / hframes;
			frame = original_row * p_amount + original_column;
		}
	}
	hframes = p_amount;
	if (frame >= vframes * hframes) {
		frame = 0;
	}
	_queue_redraw();
	notify_property_list_changed();
}

int Sprite3D::get_hframes() const {
	return hframes;
}

Rect2 Sprite3D::get_item_rect() const {
	if (texture.is_null()) {
		return Rect2(0, 0, 1, 1);
	}

	Size2 s;

	if (region) {
		s = region_rect.size;
	} else {
		s = texture->get_size();
		s = s / Point2(hframes, vframes);
	}

	Point2 ofs = get_offset();
	if (is_centered()) {
		ofs -= s / 2;
	}

	if (s == Size2(0, 0)) {
		s = Size2(1, 1);
	}

	return Rect2(ofs, s);
}

void Sprite3D::_validate_property(PropertyInfo &p_property) const {
	if (p_property.name == "frame") {
		p_property.hint = PROPERTY_HINT_RANGE;
		p_property.hint_string = "0," + itos(vframes * hframes - 1) + ",1";
		p_property.usage |= PROPERTY_USAGE_KEYING_INCREMENTS;
	}

	if (p_property.name == "frame_coords") {
		p_property.usage |= PROPERTY_USAGE_KEYING_INCREMENTS;
	}

	if (!region && (p_property.name == "region_rect")) {
		p_property.usage = PROPERTY_USAGE_NO_EDITOR;
	}
}

void Sprite3D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_texture", "texture"), &Sprite3D::set_texture);
	ClassDB::bind_method(D_METHOD("get_texture"), &Sprite3D::get_texture);

	ClassDB::bind_method(D_METHOD("set_region_enabled", "enabled"), &Sprite3D::set_region_enabled);
	ClassDB::bind_method(D_METHOD("is_region_enabled"), &Sprite3D::is_region_enabled);

	ClassDB::bind_method(D_METHOD("set_region_rect", "rect"), &Sprite3D::set_region_rect);
	ClassDB::bind_method(D_METHOD("get_region_rect"), &Sprite3D::get_region_rect);

	ClassDB::bind_method(D_METHOD("set_frame", "frame"), &Sprite3D::set_frame);
	ClassDB::bind_method(D_METHOD("get_frame"), &Sprite3D::get_frame);

	ClassDB::bind_method(D_METHOD("set_frame_coords", "coords"), &Sprite3D::set_frame_coords);
	ClassDB::bind_method(D_METHOD("get_frame_coords"), &Sprite3D::get_frame_coords);

	ClassDB::bind_method(D_METHOD("set_vframes", "vframes"), &Sprite3D::set_vframes);
	ClassDB::bind_method(D_METHOD("get_vframes"), &Sprite3D::get_vframes);

	ClassDB::bind_method(D_METHOD("set_hframes", "hframes"), &Sprite3D::set_hframes);
	ClassDB::bind_method(D_METHOD("get_hframes"), &Sprite3D::get_hframes);

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "texture", PROPERTY_HINT_RESOURCE_TYPE, "Texture2D"), "set_texture", "get_texture");
	ADD_GROUP("Animation", "");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "hframes", PROPERTY_HINT_RANGE, "1,16384,1"), "set_hframes", "get_hframes");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "vframes", PROPERTY_HINT_RANGE, "1,16384,1"), "set_vframes", "get_vframes");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "frame"), "set_frame", "get_frame");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2I, "frame_coords", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_EDITOR), "set_frame_coords", "get_frame_coords");
	ADD_GROUP("Region", "region_");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "region_enabled"), "set_region_enabled", "is_region_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::RECT2, "region_rect", PROPERTY_HINT_NONE, "suffix:px"), "set_region_rect", "get_region_rect");

	ADD_SIGNAL(MethodInfo("frame_changed"));
	ADD_SIGNAL(MethodInfo("texture_changed"));
}

Sprite3D::Sprite3D() {
}

////////////////////////////////////////

void AnimatedSprite3D::_draw() {
	if (get_base() != get_mesh()) {
		set_base(get_mesh());
	}

	if (frames.is_null() || !frames->has_animation(animation)) {
		return;
	}

	Ref<Texture2D> texture = frames->get_frame_texture(animation, frame);
	if (texture.is_null()) {
		set_base(RID());
		return;
	}
	Size2 tsize = texture->get_size();
	if (tsize.x == 0 || tsize.y == 0) {
		return;
	}

	Rect2 src_rect;
	src_rect.size = tsize;

	Point2 ofs = get_offset();
	if (is_centered()) {
		ofs -= tsize / 2;
	}

	Rect2 dst_rect(ofs, tsize);

	draw_texture_rect(texture, dst_rect, src_rect);
}

void AnimatedSprite3D::_validate_property(PropertyInfo &p_property) const {
	if (frames.is_null()) {
		return;
	}

	if (p_property.name == "animation") {
		List<StringName> names;
		frames->get_animation_list(&names);
		names.sort_custom<StringName::AlphCompare>();

		bool current_found = false;
		bool is_first_element = true;

		for (const StringName &E : names) {
			if (!is_first_element) {
				p_property.hint_string += ",";
			} else {
				is_first_element = false;
			}

			p_property.hint_string += String(E);
			if (animation == E) {
				current_found = true;
			}
		}

		if (!current_found) {
			if (p_property.hint_string.is_empty()) {
				p_property.hint_string = String(animation);
			} else {
				p_property.hint_string = String(animation) + "," + p_property.hint_string;
			}
		}
		return;
	}

	if (p_property.name == "frame") {
		if (playing) {
			p_property.usage = PROPERTY_USAGE_EDITOR | PROPERTY_USAGE_READ_ONLY;
			return;
		}

		p_property.hint = PROPERTY_HINT_RANGE;
		if (frames->has_animation(animation) && frames->get_frame_count(animation) > 0) {
			p_property.hint_string = "0," + itos(frames->get_frame_count(animation) - 1) + ",1";
		} else {
			// Avoid an error, `hint_string` is required for `PROPERTY_HINT_RANGE`.
			p_property.hint_string = "0,0,1";
		}
		p_property.usage |= PROPERTY_USAGE_KEYING_INCREMENTS;
	}
}

void AnimatedSprite3D::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY: {
			if (!Engine::get_singleton()->is_editor_hint() && frames.is_valid() && frames->has_animation(autoplay)) {
				play(autoplay);
			}
		} break;

		case NOTIFICATION_INTERNAL_PROCESS: {
			if (frames.is_null() || !frames->has_animation(animation)) {
				return;
			}

			double remaining = get_process_delta_time();
			int i = 0;
			while (remaining) {
				// Animation speed may be changed by animation_finished or frame_changed signals.
				double speed = frames->get_animation_speed(animation) * speed_scale * custom_speed_scale * frame_speed_scale;
				double abs_speed = Math::abs(speed);

				if (speed == 0) {
					return; // Do nothing.
				}

				// Frame count may be changed by animation_finished or frame_changed signals.
				int fc = frames->get_frame_count(animation);

				int last_frame = fc - 1;
				if (!signbit(speed)) {
					// Forwards.
					if (frame_progress >= 1.0) {
						if (frame >= last_frame) {
							if (frames->get_animation_loop(animation)) {
								frame = 0;
								emit_signal("animation_looped");
							} else {
								frame = last_frame;
								pause();
								emit_signal(SceneStringName(animation_finished));
								return;
							}
						} else {
							frame++;
						}
						_calc_frame_speed_scale();
						frame_progress = 0.0;
						_queue_redraw();
						emit_signal(SceneStringName(frame_changed));
					}
					double to_process = MIN((1.0 - frame_progress) / abs_speed, remaining);
					frame_progress += to_process * abs_speed;
					remaining -= to_process;
				} else {
					// Backwards.
					if (frame_progress <= 0) {
						if (frame <= 0) {
							if (frames->get_animation_loop(animation)) {
								frame = last_frame;
								emit_signal("animation_looped");
							} else {
								frame = 0;
								pause();
								emit_signal(SceneStringName(animation_finished));
								return;
							}
						} else {
							frame--;
						}
						_calc_frame_speed_scale();
						frame_progress = 1.0;
						_queue_redraw();
						emit_signal(SceneStringName(frame_changed));
					}
					double to_process = MIN(frame_progress / abs_speed, remaining);
					frame_progress -= to_process * abs_speed;
					remaining -= to_process;
				}

				i++;
				if (i > fc) {
					return; // Prevents freezing if to_process is each time much less than remaining.
				}
			}
		} break;
	}
}

void AnimatedSprite3D::set_sprite_frames(const Ref<SpriteFrames> &p_frames) {
	if (frames == p_frames) {
		return;
	}

	if (frames.is_valid()) {
		frames->disconnect(CoreStringName(changed), callable_mp(this, &AnimatedSprite3D::_res_changed));
	}
	stop();
	frames = p_frames;
	if (frames.is_valid()) {
		frames->connect(CoreStringName(changed), callable_mp(this, &AnimatedSprite3D::_res_changed));

		List<StringName> al;
		frames->get_animation_list(&al);
		if (al.size() == 0) {
			set_animation(StringName());
			autoplay = String();
		} else {
			if (!frames->has_animation(animation)) {
				set_animation(al.front()->get());
			}
			if (!frames->has_animation(autoplay)) {
				autoplay = String();
			}
		}
	}

	notify_property_list_changed();
	_queue_redraw();
	update_configuration_warnings();
	emit_signal("sprite_frames_changed");
}

Ref<SpriteFrames> AnimatedSprite3D::get_sprite_frames() const {
	return frames;
}

void AnimatedSprite3D::set_frame(int p_frame) {
	set_frame_and_progress(p_frame, signbit(get_playing_speed()) ? 1.0 : 0.0);
}

int AnimatedSprite3D::get_frame() const {
	return frame;
}

void AnimatedSprite3D::set_frame_progress(real_t p_progress) {
	frame_progress = p_progress;
}

real_t AnimatedSprite3D::get_frame_progress() const {
	return frame_progress;
}

void AnimatedSprite3D::set_frame_and_progress(int p_frame, real_t p_progress) {
	if (frames.is_null()) {
		return;
	}

	bool has_animation = frames->has_animation(animation);
	int end_frame = has_animation ? MAX(0, frames->get_frame_count(animation) - 1) : 0;
	bool is_changed = frame != p_frame;

	if (p_frame < 0) {
		frame = 0;
	} else if (has_animation && p_frame > end_frame) {
		frame = end_frame;
	} else {
		frame = p_frame;
	}

	_calc_frame_speed_scale();
	frame_progress = p_progress;

	if (!is_changed) {
		return; // No change, don't redraw.
	}
	_queue_redraw();
	emit_signal(SceneStringName(frame_changed));
}

void AnimatedSprite3D::set_speed_scale(float p_speed_scale) {
	speed_scale = p_speed_scale;
}

float AnimatedSprite3D::get_speed_scale() const {
	return speed_scale;
}

float AnimatedSprite3D::get_playing_speed() const {
	if (!playing) {
		return 0;
	}
	return speed_scale * custom_speed_scale;
}

Rect2 AnimatedSprite3D::get_item_rect() const {
	if (frames.is_null() || !frames->has_animation(animation)) {
		return Rect2(0, 0, 1, 1);
	}
	if (frame < 0 || frame >= frames->get_frame_count(animation)) {
		return Rect2(0, 0, 1, 1);
	}

	Ref<Texture2D> t;
	if (animation) {
		t = frames->get_frame_texture(animation, frame);
	}
	if (t.is_null()) {
		return Rect2(0, 0, 1, 1);
	}
	Size2 s = t->get_size();

	Point2 ofs = get_offset();
	if (is_centered()) {
		ofs -= s / 2;
	}

	if (s == Size2(0, 0)) {
		s = Size2(1, 1);
	}

	return Rect2(ofs, s);
}

void AnimatedSprite3D::_res_changed() {
	set_frame_and_progress(frame, frame_progress);
	_queue_redraw();
	notify_property_list_changed();
}

bool AnimatedSprite3D::is_playing() const {
	return playing;
}

void AnimatedSprite3D::set_autoplay(const String &p_name) {
	if (is_inside_tree() && !Engine::get_singleton()->is_editor_hint()) {
		WARN_PRINT("Setting autoplay after the node has been added to the scene has no effect.");
	}

	autoplay = p_name;
}

String AnimatedSprite3D::get_autoplay() const {
	return autoplay;
}

void AnimatedSprite3D::play(const StringName &p_name, float p_custom_scale, bool p_from_end) {
	StringName name = p_name;

	if (name == StringName()) {
		name = animation;
	}

	ERR_FAIL_COND_MSG(frames.is_null(), vformat("There is no animation with name '%s'.", name));
	ERR_FAIL_COND_MSG(!frames->get_animation_names().has(name), vformat("There is no animation with name '%s'.", name));

	if (frames->get_frame_count(name) == 0) {
		return;
	}

	playing = true;
	custom_speed_scale = p_custom_scale;

	if (name != animation) {
		animation = name;
		int end_frame = MAX(0, frames->get_frame_count(animation) - 1);

		if (p_from_end) {
			set_frame_and_progress(end_frame, 1.0);
		} else {
			set_frame_and_progress(0, 0.0);
		}
		emit_signal(SceneStringName(animation_changed));
	} else {
		int end_frame = MAX(0, frames->get_frame_count(animation) - 1);
		bool is_backward = signbit(speed_scale * custom_speed_scale);

		if (p_from_end && is_backward && frame == 0 && frame_progress <= 0.0) {
			set_frame_and_progress(end_frame, 1.0);
		} else if (!p_from_end && !is_backward && frame == end_frame && frame_progress >= 1.0) {
			set_frame_and_progress(0, 0.0);
		}
	}

	set_process_internal(true);
	notify_property_list_changed();
	_queue_redraw();
}

void AnimatedSprite3D::play_backwards(const StringName &p_name) {
	play(p_name, -1, true);
}

void AnimatedSprite3D::_stop_internal(bool p_reset) {
	playing = false;
	if (p_reset) {
		custom_speed_scale = 1.0;
		set_frame_and_progress(0, 0.0);
	}
	notify_property_list_changed();
	set_process_internal(false);
}

void AnimatedSprite3D::pause() {
	_stop_internal(false);
}

void AnimatedSprite3D::stop() {
	_stop_internal(true);
}

double AnimatedSprite3D::_get_frame_duration() {
	if (frames.is_valid() && frames->has_animation(animation)) {
		return frames->get_frame_duration(animation, frame);
	}
	return 1.0;
}

void AnimatedSprite3D::_calc_frame_speed_scale() {
	frame_speed_scale = 1.0 / _get_frame_duration();
}

void AnimatedSprite3D::set_animation(const StringName &p_name) {
	if (animation == p_name) {
		return;
	}

	animation = p_name;

	emit_signal(SceneStringName(animation_changed));

	if (frames.is_null()) {
		animation = StringName();
		stop();
		ERR_FAIL_MSG(vformat("There is no animation with name '%s'.", p_name));
	}

	int frame_count = frames->get_frame_count(animation);
	if (animation == StringName() || frame_count == 0) {
		stop();
		return;
	} else if (!frames->get_animation_names().has(animation)) {
		animation = StringName();
		stop();
		ERR_FAIL_MSG(vformat("There is no animation with name '%s'.", p_name));
	}

	if (signbit(get_playing_speed())) {
		set_frame_and_progress(frame_count - 1, 1.0);
	} else {
		set_frame_and_progress(0, 0.0);
	}

	notify_property_list_changed();
	_queue_redraw();
}

StringName AnimatedSprite3D::get_animation() const {
	return animation;
}

PackedStringArray AnimatedSprite3D::get_configuration_warnings() const {
	PackedStringArray warnings = SpriteBase3D::get_configuration_warnings();
	if (frames.is_null()) {
		warnings.push_back(RTR("A SpriteFrames resource must be created or set in the \"Sprite Frames\" property in order for AnimatedSprite3D to display frames."));
	}
	return warnings;
}

#ifdef TOOLS_ENABLED
void AnimatedSprite3D::get_argument_options(const StringName &p_function, int p_idx, List<String> *r_options) const {
	const String pf = p_function;
	if (p_idx == 0 && frames.is_valid()) {
		if (pf == "play" || pf == "play_backwards" || pf == "set_animation" || pf == "set_autoplay") {
			List<StringName> al;
			frames->get_animation_list(&al);
			for (const StringName &name : al) {
				r_options->push_back(String(name).quote());
			}
		}
	}
	SpriteBase3D::get_argument_options(p_function, p_idx, r_options);
}
#endif

#ifndef DISABLE_DEPRECATED
bool AnimatedSprite3D::_set(const StringName &p_name, const Variant &p_value) {
	if ((p_name == SNAME("frames"))) {
		set_sprite_frames(p_value);
		return true;
	}
	return false;
}
#endif
void AnimatedSprite3D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_sprite_frames", "sprite_frames"), &AnimatedSprite3D::set_sprite_frames);
	ClassDB::bind_method(D_METHOD("get_sprite_frames"), &AnimatedSprite3D::get_sprite_frames);

	ClassDB::bind_method(D_METHOD("set_animation", "name"), &AnimatedSprite3D::set_animation);
	ClassDB::bind_method(D_METHOD("get_animation"), &AnimatedSprite3D::get_animation);

	ClassDB::bind_method(D_METHOD("set_autoplay", "name"), &AnimatedSprite3D::set_autoplay);
	ClassDB::bind_method(D_METHOD("get_autoplay"), &AnimatedSprite3D::get_autoplay);

	ClassDB::bind_method(D_METHOD("is_playing"), &AnimatedSprite3D::is_playing);

	ClassDB::bind_method(D_METHOD("play", "name", "custom_speed", "from_end"), &AnimatedSprite3D::play, DEFVAL(StringName()), DEFVAL(1.0), DEFVAL(false));
	ClassDB::bind_method(D_METHOD("play_backwards", "name"), &AnimatedSprite3D::play_backwards, DEFVAL(StringName()));
	ClassDB::bind_method(D_METHOD("pause"), &AnimatedSprite3D::pause);
	ClassDB::bind_method(D_METHOD("stop"), &AnimatedSprite3D::stop);

	ClassDB::bind_method(D_METHOD("set_frame", "frame"), &AnimatedSprite3D::set_frame);
	ClassDB::bind_method(D_METHOD("get_frame"), &AnimatedSprite3D::get_frame);

	ClassDB::bind_method(D_METHOD("set_frame_progress", "progress"), &AnimatedSprite3D::set_frame_progress);
	ClassDB::bind_method(D_METHOD("get_frame_progress"), &AnimatedSprite3D::get_frame_progress);

	ClassDB::bind_method(D_METHOD("set_frame_and_progress", "frame", "progress"), &AnimatedSprite3D::set_frame_and_progress);

	ClassDB::bind_method(D_METHOD("set_speed_scale", "speed_scale"), &AnimatedSprite3D::set_speed_scale);
	ClassDB::bind_method(D_METHOD("get_speed_scale"), &AnimatedSprite3D::get_speed_scale);
	ClassDB::bind_method(D_METHOD("get_playing_speed"), &AnimatedSprite3D::get_playing_speed);

	ClassDB::bind_method(D_METHOD("_res_changed"), &AnimatedSprite3D::_res_changed);

	ADD_SIGNAL(MethodInfo("sprite_frames_changed"));
	ADD_SIGNAL(MethodInfo("animation_changed"));
	ADD_SIGNAL(MethodInfo("frame_changed"));
	ADD_SIGNAL(MethodInfo("animation_looped"));
	ADD_SIGNAL(MethodInfo("animation_finished"));

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "sprite_frames", PROPERTY_HINT_RESOURCE_TYPE, "SpriteFrames"), "set_sprite_frames", "get_sprite_frames");
	ADD_PROPERTY(PropertyInfo(Variant::STRING_NAME, "animation", PROPERTY_HINT_ENUM, ""), "set_animation", "get_animation");
	ADD_PROPERTY(PropertyInfo(Variant::STRING_NAME, "autoplay", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NO_EDITOR), "set_autoplay", "get_autoplay");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "frame"), "set_frame", "get_frame");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "frame_progress", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NO_EDITOR), "set_frame_progress", "get_frame_progress");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "speed_scale"), "set_speed_scale", "get_speed_scale");
}

AnimatedSprite3D::AnimatedSprite3D() {
}

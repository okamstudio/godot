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
#include "core/core_string_names.h"
#include "scene/scene_string_names.h"

#ifdef TOOLS_ENABLED
#include "editor/editor_settings.h"
#endif

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
	_queue_update();

	for (List<SpriteBase3D *>::Element *E = children.front(); E; E = E->next()) {
		E->get()->_propagate_color_changed();
	}
}

void SpriteBase3D::_notification(int p_what) {
	if (p_what == NOTIFICATION_ENTER_TREE) {
		if (!pending_update) {
			_im_update();
		}

		parent_sprite = Object::cast_to<SpriteBase3D>(get_parent());
		if (parent_sprite) {
			pI = parent_sprite->children.push_back(this);
		}
	}

	if (p_what == NOTIFICATION_EXIT_TREE) {
		if (parent_sprite) {
			parent_sprite->children.erase(pI);
			pI = nullptr;
			parent_sprite = nullptr;
		}
	}
}

void SpriteBase3D::draw_texture_rect(Ref<Texture> p_texture, Rect2 p_dst_rect, Rect2 p_src_rect) {
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
	color.a *= get_opacity();

	float pixel_size = get_pixel_size();

	// (2) Order vertices (0123) bottom-top in 2D / top-bottom in 3D.
	Vector2 vertices[4] = {
		(final_rect.position + Vector2(0, final_rect.size.y)) * pixel_size,
		(final_rect.position + final_rect.size) * pixel_size,
		(final_rect.position + Vector2(final_rect.size.x, 0)) * pixel_size,
		final_rect.position * pixel_size,
	};

	Vector2 src_tsize = p_texture->get_size();

	// Properly setup UVs for impostor textures (AtlasTexture).
	Ref<AtlasTexture> atlas_tex = p_texture;
	if (atlas_tex != nullptr) {
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
	int axis = get_axis();
	normal[axis] = 1.0;

	Plane tangent;
	if (axis == Vector3::AXIS_X) {
		tangent = Plane(0, 0, -1, -1);
	} else {
		tangent = Plane(1, 0, 0, -1);
	}

	int x_axis = ((axis + 1) % 3);
	int y_axis = ((axis + 2) % 3);

	if (axis != Vector3::AXIS_Z) {
		SWAP(x_axis, y_axis);

		for (int i = 0; i < 4; i++) {
			//uvs[i] = Vector2(1.0,1.0)-uvs[i];
			//SWAP(vertices[i].x,vertices[i].y);
			if (axis == Vector3::AXIS_Y) {
				vertices[i].y = -vertices[i].y;
			} else if (axis == Vector3::AXIS_X) {
				vertices[i].x = -vertices[i].x;
			}
		}
	}

	AABB aabb;

	// Everything except position, color, and UV is compressed
	PoolVector<uint8_t>::Write write_buffer = mesh_buffer.write();

	Vector2 normal_oct = VisualServer::get_singleton()->norm_to_oct(normal);
	int8_t v_normal[2] = {
		(int8_t)CLAMP(normal_oct.x * 127, -128, 127),
		(int8_t)CLAMP(normal_oct.y * 127, -128, 127),
	};

	Vector2 tangent_oct = VisualServer::get_singleton()->tangent_to_oct(tangent.normal, tangent.d, false);
	int8_t v_tangent[2] = {
		(int8_t)CLAMP(tangent_oct.x * 127, -128, 127),
		(int8_t)CLAMP(tangent_oct.y * 127, -128, 127),
	};

	for (int i = 0; i < 4; i++) {
		Vector3 vtx;
		vtx[x_axis] = vertices[i][0];
		vtx[y_axis] = vertices[i][1];
		if (i == 0) {
			aabb.position = vtx;
			aabb.size = Vector3();
		} else {
			aabb.expand_to(vtx);
		}

		float v_uv[2] = { uvs[i].x, uvs[i].y };
		memcpy(&write_buffer[i * mesh_stride[VS::ARRAY_TEX_UV] + mesh_surface_offsets[VS::ARRAY_TEX_UV]], v_uv, 8);

		float v_vertex[3] = { vtx.x, vtx.y, vtx.z };
		memcpy(&write_buffer[i * mesh_stride[VS::ARRAY_VERTEX] + mesh_surface_offsets[VS::ARRAY_VERTEX]], &v_vertex, sizeof(float) * 3);
		memcpy(&write_buffer[i * mesh_stride[VS::ARRAY_NORMAL] + mesh_surface_offsets[VS::ARRAY_NORMAL]], v_normal, 2);
		memcpy(&write_buffer[i * mesh_stride[VS::ARRAY_TANGENT] + mesh_surface_offsets[VS::ARRAY_TANGENT]], v_tangent, 2);
		memcpy(&write_buffer[i * mesh_stride[VS::ARRAY_COLOR] + mesh_surface_offsets[VS::ARRAY_COLOR]], color.components, 4 * 4);
	}

	write_buffer.release();

	RID mesh = get_mesh();
	VS::get_singleton()->mesh_surface_update_region(mesh, 0, 0, mesh_buffer);

	VS::get_singleton()->mesh_set_custom_aabb(mesh, aabb);
	set_aabb(aabb);

	RID mat = Material3D::get_material_rid_for_2d(get_draw_flag(FLAG_SHADED), get_draw_flag(FLAG_TRANSPARENT), get_draw_flag(FLAG_DOUBLE_SIDED), get_alpha_cut_mode() == ALPHA_CUT_DISCARD, get_alpha_cut_mode() == ALPHA_CUT_OPAQUE_PREPASS, get_billboard_mode() == Material3D::BILLBOARD_ENABLED, get_billboard_mode() == Material3D::BILLBOARD_FIXED_Y, get_draw_flag(FLAG_DISABLE_DEPTH_TEST), get_draw_flag(FLAG_FIXED_SIZE));
	VS::get_singleton()->material_set_shader(get_material(), VS::get_singleton()->material_get_shader(mat));
	VS::get_singleton()->material_set_param(get_material(), "texture_albedo", p_texture->get_rid());
	if (get_alpha_cut_mode() == ALPHA_CUT_DISABLED) {
		VS::get_singleton()->material_set_render_priority(get_material(), get_render_priority());
	}
	VS::get_singleton()->instance_set_surface_material(get_instance(), 0, get_material());
}

void SpriteBase3D::set_centered(bool p_center) {
	centered = p_center;
	_queue_update();
}

bool SpriteBase3D::is_centered() const {
	return centered;
}

void SpriteBase3D::set_offset(const Point2 &p_offset) {
	offset = p_offset;
	_queue_update();
}
Point2 SpriteBase3D::get_offset() const {
	return offset;
}

void SpriteBase3D::set_flip_h(bool p_flip) {
	hflip = p_flip;
	_queue_update();
}
bool SpriteBase3D::is_flipped_h() const {
	return hflip;
}

void SpriteBase3D::set_flip_v(bool p_flip) {
	vflip = p_flip;
	_queue_update();
}
bool SpriteBase3D::is_flipped_v() const {
	return vflip;
}

void SpriteBase3D::set_modulate(const Color &p_color) {
	modulate = p_color;
	_propagate_color_changed();
	_queue_update();
}

Color SpriteBase3D::get_modulate() const {
	return modulate;
}

void SpriteBase3D::set_render_priority(int p_priority) {
	ERR_FAIL_COND(p_priority < VS::MATERIAL_RENDER_PRIORITY_MIN || p_priority > VS::MATERIAL_RENDER_PRIORITY_MAX);
	render_priority = p_priority;
	_queue_update();
}

int SpriteBase3D::get_render_priority() const {
	return render_priority;
}

void SpriteBase3D::set_pixel_size(float p_amount) {
	pixel_size = p_amount;
	_queue_update();
}
float SpriteBase3D::get_pixel_size() const {
	return pixel_size;
}

void SpriteBase3D::set_opacity(float p_amount) {
	opacity = p_amount;
	_queue_update();
}
float SpriteBase3D::get_opacity() const {
	return opacity;
}

void SpriteBase3D::set_axis(Vector3::Axis p_axis) {
	ERR_FAIL_INDEX(p_axis, 3);
	axis = p_axis;
	_queue_update();
}
Vector3::Axis SpriteBase3D::get_axis() const {
	return axis;
}

void SpriteBase3D::_im_update() {
	_draw();

	pending_update = false;

	//texture->draw_rect_region(ci,dst_rect,src_rect,modulate);
}

void SpriteBase3D::_queue_update() {
	if (pending_update) {
		return;
	}

	triangle_mesh.unref();
	update_gizmo();

	pending_update = true;
	call_deferred(SceneStringNames::get_singleton()->_im_update);
}

AABB SpriteBase3D::get_aabb() const {
	return aabb;
}
PoolVector<Face3> SpriteBase3D::get_faces(uint32_t p_usage_flags) const {
	return PoolVector<Face3>();
}

Ref<TriangleMesh> SpriteBase3D::generate_triangle_mesh() const {
	if (triangle_mesh.is_valid()) {
		return triangle_mesh;
	}

	PoolVector<Vector3> faces;
	faces.resize(6);
	PoolVector<Vector3>::Write facesw = faces.write();

	Rect2 final_rect = get_item_rect();

	if (final_rect.size.x == 0 || final_rect.size.y == 0) {
		return Ref<TriangleMesh>();
	}

	float pixel_size = get_pixel_size();

	Vector2 vertices[4] = {

		(final_rect.position + Vector2(0, final_rect.size.y)) * pixel_size,
		(final_rect.position + final_rect.size) * pixel_size,
		(final_rect.position + Vector2(final_rect.size.x, 0)) * pixel_size,
		final_rect.position * pixel_size,

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

	facesw.release();

	triangle_mesh = Ref<TriangleMesh>(memnew(TriangleMesh));
	triangle_mesh->create(faces);

	return triangle_mesh;
}

void SpriteBase3D::set_draw_flag(DrawFlags p_flag, bool p_enable) {
	ERR_FAIL_INDEX(p_flag, FLAG_MAX);
	flags[p_flag] = p_enable;
	_queue_update();
}

bool SpriteBase3D::get_draw_flag(DrawFlags p_flag) const {
	ERR_FAIL_INDEX_V(p_flag, FLAG_MAX, false);
	return flags[p_flag];
}

void SpriteBase3D::set_alpha_cut_mode(AlphaCutMode p_mode) {
	ERR_FAIL_INDEX(p_mode, 3);
	alpha_cut = p_mode;
	_queue_update();
}

SpriteBase3D::AlphaCutMode SpriteBase3D::get_alpha_cut_mode() const {
	return alpha_cut;
}

void SpriteBase3D::set_billboard_mode(Material3D::BillboardMode p_mode) {
	ERR_FAIL_INDEX(p_mode, 3);
	billboard_mode = p_mode;
	_queue_update();
}

Material3D::BillboardMode SpriteBase3D::get_billboard_mode() const {
	return billboard_mode;
}

Rect2 SpriteBase3D::get_item_rect() const {
	return Rect2();
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

	ClassDB::bind_method(D_METHOD("set_opacity", "opacity"), &SpriteBase3D::set_opacity);
	ClassDB::bind_method(D_METHOD("get_opacity"), &SpriteBase3D::get_opacity);

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

	ClassDB::bind_method(D_METHOD("set_billboard_mode", "mode"), &SpriteBase3D::set_billboard_mode);
	ClassDB::bind_method(D_METHOD("get_billboard_mode"), &SpriteBase3D::get_billboard_mode);

	ClassDB::bind_method(D_METHOD("get_item_rect"), &SpriteBase3D::get_item_rect);
	ClassDB::bind_method(D_METHOD("generate_triangle_mesh"), &SpriteBase3D::generate_triangle_mesh);

	ClassDB::bind_method(D_METHOD("_queue_update"), &SpriteBase3D::_queue_update);
	ClassDB::bind_method(D_METHOD("_im_update"), &SpriteBase3D::_im_update);

	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "centered"), "set_centered", "is_centered");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "offset"), "set_offset", "get_offset");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "flip_h"), "set_flip_h", "is_flipped_h");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "flip_v"), "set_flip_v", "is_flipped_v");
	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "modulate"), "set_modulate", "get_modulate");
	ADD_PROPERTY(PropertyInfo(Variant::REAL, "opacity", PROPERTY_HINT_RANGE, "0,1,0.01"), "set_opacity", "get_opacity");
	ADD_PROPERTY(PropertyInfo(Variant::REAL, "pixel_size", PROPERTY_HINT_RANGE, "0.0001,128,0.0001"), "set_pixel_size", "get_pixel_size");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "axis", PROPERTY_HINT_ENUM, "X-Axis,Y-Axis,Z-Axis"), "set_axis", "get_axis");
	ADD_GROUP("Flags", "");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "billboard", PROPERTY_HINT_ENUM, "Disabled,Enabled,Y-Billboard"), "set_billboard_mode", "get_billboard_mode");
	ADD_PROPERTYI(PropertyInfo(Variant::BOOL, "transparent"), "set_draw_flag", "get_draw_flag", FLAG_TRANSPARENT);
	ADD_PROPERTYI(PropertyInfo(Variant::BOOL, "shaded"), "set_draw_flag", "get_draw_flag", FLAG_SHADED);
	ADD_PROPERTYI(PropertyInfo(Variant::BOOL, "double_sided"), "set_draw_flag", "get_draw_flag", FLAG_DOUBLE_SIDED);
	ADD_PROPERTYI(PropertyInfo(Variant::BOOL, "no_depth_test"), "set_draw_flag", "get_draw_flag", FLAG_DISABLE_DEPTH_TEST);
	ADD_PROPERTYI(PropertyInfo(Variant::BOOL, "fixed_size"), "set_draw_flag", "get_draw_flag", FLAG_FIXED_SIZE);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "alpha_cut", PROPERTY_HINT_ENUM, "Disabled,Discard,Opaque Pre-Pass"), "set_alpha_cut_mode", "get_alpha_cut_mode");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "render_priority", PROPERTY_HINT_RANGE, itos(VS::MATERIAL_RENDER_PRIORITY_MIN) + "," + itos(VS::MATERIAL_RENDER_PRIORITY_MAX) + ",1"), "set_render_priority", "get_render_priority");

	BIND_ENUM_CONSTANT(FLAG_TRANSPARENT);
	BIND_ENUM_CONSTANT(FLAG_SHADED);
	BIND_ENUM_CONSTANT(FLAG_DOUBLE_SIDED);
	BIND_ENUM_CONSTANT(FLAG_DISABLE_DEPTH_TEST);
	BIND_ENUM_CONSTANT(FLAG_FIXED_SIZE);
	BIND_ENUM_CONSTANT(FLAG_MAX);

	BIND_ENUM_CONSTANT(ALPHA_CUT_DISABLED);
	BIND_ENUM_CONSTANT(ALPHA_CUT_DISCARD);
	BIND_ENUM_CONSTANT(ALPHA_CUT_OPAQUE_PREPASS);
}

SpriteBase3D::SpriteBase3D() {
	color_dirty = true;
	centered = true;
	hflip = false;
	vflip = false;
	parent_sprite = nullptr;
	pI = nullptr;

	for (int i = 0; i < FLAG_MAX; i++) {
		flags[i] = i == FLAG_TRANSPARENT || i == FLAG_DOUBLE_SIDED;
	}

	alpha_cut = ALPHA_CUT_DISABLED;
	billboard_mode = Material3D::BILLBOARD_DISABLED;
	axis = Vector3::AXIS_Z;
	pixel_size = 0.01;
	modulate = Color(1, 1, 1, 1);
	pending_update = false;
	opacity = 1.0;

	material = RID_PRIME(VisualServer::get_singleton()->material_create());
	// Set defaults for material, names need to match up those in SpatialMaterial
	VS::get_singleton()->material_set_param(material, "albedo", Color(1, 1, 1, 1));
	VS::get_singleton()->material_set_param(material, "specular", 0.5);
	VS::get_singleton()->material_set_param(material, "metallic", 0.0);
	VS::get_singleton()->material_set_param(material, "roughness", 1.0);
	VS::get_singleton()->material_set_param(material, "uv1_offset", Vector3(0, 0, 0));
	VS::get_singleton()->material_set_param(material, "uv1_scale", Vector3(1, 1, 1));
	VS::get_singleton()->material_set_param(material, "uv2_offset", Vector3(0, 0, 0));
	VS::get_singleton()->material_set_param(material, "uv2_scale", Vector3(1, 1, 1));
	VS::get_singleton()->material_set_param(material, "alpha_scissor_threshold", 0.98);

	mesh = RID_PRIME(VisualServer::get_singleton()->mesh_create());

	PoolVector3Array mesh_vertices;
	PoolVector3Array mesh_normals;
	PoolRealArray mesh_tangents;
	PoolColorArray mesh_colors;
	PoolVector2Array mesh_uvs;

	mesh_vertices.resize(4);
	mesh_normals.resize(4);
	mesh_tangents.resize(16);
	mesh_colors.resize(4);
	mesh_uvs.resize(4);

	// create basic mesh and store format information
	for (int i = 0; i < 4; i++) {
		mesh_normals.write()[i] = Vector3(0.0, 0.0, 1.0);
		mesh_tangents.write()[i * 4 + 0] = 0.0;
		mesh_tangents.write()[i * 4 + 1] = 0.0;
		mesh_tangents.write()[i * 4 + 2] = 1.0;
		mesh_tangents.write()[i * 4 + 3] = 1.0;
		mesh_colors.write()[i] = Color(1.0, 1.0, 1.0, 1.0);
		mesh_uvs.write()[i] = Vector2(0.0, 0.0);
		mesh_vertices.write()[i] = Vector3(0.0, 0.0, 0.0);
	}

	Array mesh_array;
	mesh_array.resize(VS::ARRAY_MAX);
	mesh_array[VS::ARRAY_VERTEX] = mesh_vertices;
	mesh_array[VS::ARRAY_NORMAL] = mesh_normals;
	mesh_array[VS::ARRAY_TANGENT] = mesh_tangents;
	mesh_array[VS::ARRAY_COLOR] = mesh_colors;
	mesh_array[VS::ARRAY_TEX_UV] = mesh_uvs;

	uint32_t compress_format = (VS::ARRAY_COMPRESS_DEFAULT & ~VS::ARRAY_COMPRESS_TEX_UV) & ~VS::ARRAY_COMPRESS_COLOR;
	compress_format |= VS::ARRAY_FLAG_USE_DYNAMIC_UPDATE;
	VS::get_singleton()->mesh_add_surface_from_arrays(mesh, VS::PRIMITIVE_TRIANGLE_FAN, mesh_array, Array(), compress_format);
	const int surface_vertex_len = VS::get_singleton()->mesh_surface_get_array_len(mesh, 0);
	const int surface_index_len = VS::get_singleton()->mesh_surface_get_array_index_len(mesh, 0);

	mesh_surface_format = VS::get_singleton()->mesh_surface_get_format(mesh, 0);
	mesh_buffer = VS::get_singleton()->mesh_surface_get_array(mesh, 0);
	VS::get_singleton()->mesh_surface_make_offsets_from_format(mesh_surface_format, surface_vertex_len, surface_index_len, mesh_surface_offsets, mesh_stride);
	set_base(mesh);
}

SpriteBase3D::~SpriteBase3D() {
	VisualServer::get_singleton()->free(mesh);
	VisualServer::get_singleton()->free(material);
}

///////////////////////////////////////////

void Sprite3D::_draw() {
	if (get_base() != get_mesh()) {
		set_base(get_mesh());
	}
	if (!texture.is_valid()) {
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
	Point2 frame_offset = Point2(frame % hframes, frame / hframes);
	frame_offset *= frame_size;

	Point2 dst_offset = get_offset();
	if (is_centered()) {
		dst_offset -= frame_size / 2;
	}

	Rect2 src_rect(base_rect.position + frame_offset, frame_size);
	Rect2 dst_rect(dst_offset, frame_size);

	draw_texture_rect(texture, dst_rect, src_rect);
}

void Sprite3D::set_texture(const Ref<Texture> &p_texture) {
	if (p_texture == texture) {
		return;
	}
	if (texture.is_valid()) {
		texture->disconnect(CoreStringNames::get_singleton()->changed, this, SceneStringNames::get_singleton()->_queue_update);
	}
	texture = p_texture;
	if (texture.is_valid()) {
		texture->set_flags(texture->get_flags()); //remove repeat from texture, it looks bad in sprites
		texture->connect(CoreStringNames::get_singleton()->changed, this, SceneStringNames::get_singleton()->_queue_update);
	}
	_queue_update();
}

Ref<Texture> Sprite3D::get_texture() const {
	return texture;
}

void Sprite3D::set_region(bool p_region) {
	if (p_region == region) {
		return;
	}

	region = p_region;
	_queue_update();
}

bool Sprite3D::is_region() const {
	return region;
}

void Sprite3D::set_region_rect(const Rect2 &p_region_rect) {
	bool changed = region_rect != p_region_rect;
	region_rect = p_region_rect;
	if (region && changed) {
		_queue_update();
	}
}

Rect2 Sprite3D::get_region_rect() const {
	return region_rect;
}

void Sprite3D::set_frame(int p_frame) {
	ERR_FAIL_INDEX(p_frame, int64_t(vframes) * hframes);

	frame = p_frame;

	_queue_update();

	_change_notify("frame");
	_change_notify("frame_coords");
	emit_signal(SceneStringNames::get_singleton()->frame_changed);
}

int Sprite3D::get_frame() const {
	return frame;
}

void Sprite3D::set_frame_coords(const Vector2 &p_coord) {
	ERR_FAIL_INDEX(int(p_coord.x), hframes);
	ERR_FAIL_INDEX(int(p_coord.y), vframes);

	set_frame(int(p_coord.y) * hframes + int(p_coord.x));
}

Vector2 Sprite3D::get_frame_coords() const {
	return Vector2(frame % hframes, frame / hframes);
}

void Sprite3D::set_vframes(int p_amount) {
	ERR_FAIL_COND(p_amount < 1);
	vframes = p_amount;
	_queue_update();
	_change_notify();
}
int Sprite3D::get_vframes() const {
	return vframes;
}

void Sprite3D::set_hframes(int p_amount) {
	ERR_FAIL_COND(p_amount < 1);
	hframes = p_amount;
	_queue_update();
	_change_notify();
}
int Sprite3D::get_hframes() const {
	return hframes;
}

Rect2 Sprite3D::get_item_rect() const {
	if (texture.is_null()) {
		return Rect2(0, 0, 1, 1);
	}
	/*
	if (texture.is_null())
		return CanvasItem::get_item_rect();
	*/

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

void Sprite3D::_validate_property(PropertyInfo &property) const {
	if (property.name == "frame") {
		property.hint = PROPERTY_HINT_RANGE;
		property.hint_string = "0," + itos(vframes * hframes - 1) + ",1";
		property.usage |= PROPERTY_USAGE_KEYING_INCREMENTS;
	}

	if (property.name == "frame_coords") {
		property.usage |= PROPERTY_USAGE_KEYING_INCREMENTS;
	}
}

void Sprite3D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_texture", "texture"), &Sprite3D::set_texture);
	ClassDB::bind_method(D_METHOD("get_texture"), &Sprite3D::get_texture);

	ClassDB::bind_method(D_METHOD("set_region", "enabled"), &Sprite3D::set_region);
	ClassDB::bind_method(D_METHOD("is_region"), &Sprite3D::is_region);

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

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "texture", PROPERTY_HINT_RESOURCE_TYPE, "Texture"), "set_texture", "get_texture");
	ADD_GROUP("Animation", "");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "hframes", PROPERTY_HINT_RANGE, "1,16384,1"), "set_hframes", "get_hframes");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "vframes", PROPERTY_HINT_RANGE, "1,16384,1"), "set_vframes", "get_vframes");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "frame"), "set_frame", "get_frame");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "frame_coords", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_EDITOR), "set_frame_coords", "get_frame_coords");
	ADD_GROUP("Region", "region_");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "region_enabled"), "set_region", "is_region");
	ADD_PROPERTY(PropertyInfo(Variant::RECT2, "region_rect"), "set_region_rect", "get_region_rect");

	ADD_SIGNAL(MethodInfo("frame_changed"));
}

Sprite3D::Sprite3D() {
	region = false;
	frame = 0;
	vframes = 1;
	hframes = 1;
}

////////////////////////////////////////

void AnimatedSprite3D::_draw() {
	if (get_base() != get_mesh()) {
		set_base(get_mesh());
	}

	if (frames.is_null()) {
		return;
	}

	if (frame < 0) {
		return;
	}

	if (!frames->has_animation(animation)) {
		return;
	}

	Ref<Texture> texture = frames->get_frame(animation, frame);
	if (!texture.is_valid()) {
		set_base(RID());
		return; //no texuture no life
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

void AnimatedSprite3D::_validate_property(PropertyInfo &property) const {
	if (!frames.is_valid()) {
		return;
	}
	if (property.name == "animation") {
		property.hint = PROPERTY_HINT_ENUM;
		List<StringName> names;
		frames->get_animation_list(&names);
		names.sort_custom<StringName::AlphCompare>();

		bool current_found = false;

		for (List<StringName>::Element *E = names.front(); E; E = E->next()) {
			if (E->prev()) {
				property.hint_string += ",";
			}

			property.hint_string += String(E->get());
			if (animation == E->get()) {
				current_found = true;
			}
		}

		if (!current_found) {
			if (property.hint_string == String()) {
				property.hint_string = String(animation);
			} else {
				property.hint_string = String(animation) + "," + property.hint_string;
			}
		}
	}

	if (property.name == "frame") {
		if (playing) {
			property.usage = PROPERTY_USAGE_EDITOR;
			return;
		}

		property.hint = PROPERTY_HINT_RANGE;
		if (frames->has_animation(animation) && frames->get_frame_count(animation) > 1) {
			property.hint_string = "0," + itos(frames->get_frame_count(animation) - 1) + ",1";
		}
		property.usage |= PROPERTY_USAGE_KEYING_INCREMENTS;
	}
}

void AnimatedSprite3D::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_INTERNAL_PROCESS: {
			if (frames.is_null()) {
				return;
			}
			if (!frames->has_animation(animation)) {
				return;
			}
			if (frame < 0) {
				return;
			}

			float remaining = get_process_delta_time();

			while (remaining) {
				float speed = frames->get_animation_speed(animation);
				if (speed == 0) {
					return; // Do nothing.
				}

				if (timeout <= 0) {
					timeout = 1.0 / speed;

					int fc = frames->get_frame_count(animation);
					if (frame >= fc - 1) {
						if (frames->get_animation_loop(animation)) {
							frame = 0;
						} else {
							frame = fc - 1;
						}
						emit_signal(SceneStringNames::get_singleton()->animation_finished);
					} else {
						frame++;
					}

					_queue_update();
					_change_notify("frame");
					emit_signal(SceneStringNames::get_singleton()->frame_changed);
				}

				float to_process = MIN(timeout, remaining);
				remaining -= to_process;
				timeout -= to_process;
			}
		} break;
	}
}

void AnimatedSprite3D::set_sprite_frames(const Ref<SpriteFrames> &p_frames) {
	if (frames.is_valid()) {
		frames->disconnect("changed", this, "_res_changed");
	}
	frames = p_frames;
	if (frames.is_valid()) {
		frames->connect("changed", this, "_res_changed");
	}

	if (!frames.is_valid()) {
		frame = 0;
	} else {
		set_frame(frame);
	}

	_change_notify();
	_reset_timeout();
	_queue_update();
	update_configuration_warning();
}

Ref<SpriteFrames> AnimatedSprite3D::get_sprite_frames() const {
	return frames;
}

void AnimatedSprite3D::set_frame(int p_frame) {
	if (!frames.is_valid()) {
		return;
	}

	if (frames->has_animation(animation)) {
		int limit = frames->get_frame_count(animation);
		if (p_frame >= limit) {
			p_frame = limit - 1;
		}
	}

	if (p_frame < 0) {
		p_frame = 0;
	}

	if (frame == p_frame) {
		return;
	}

	frame = p_frame;
	_reset_timeout();
	_queue_update();
	_change_notify("frame");
	emit_signal(SceneStringNames::get_singleton()->frame_changed);
}
int AnimatedSprite3D::get_frame() const {
	return frame;
}

Rect2 AnimatedSprite3D::get_item_rect() const {
	if (!frames.is_valid() || !frames->has_animation(animation) || frame < 0 || frame >= frames->get_frame_count(animation)) {
		return Rect2(0, 0, 1, 1);
	}

	Ref<Texture> t;
	if (animation) {
		t = frames->get_frame(animation, frame);
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
	set_frame(frame);
	_change_notify("frame");
	_change_notify("animation");
	_queue_update();
}

void AnimatedSprite3D::_set_playing(bool p_playing) {
	if (playing == p_playing) {
		return;
	}
	playing = p_playing;
	_reset_timeout();
	set_process_internal(playing);
	property_list_changed_notify();
}

bool AnimatedSprite3D::_is_playing() const {
	return playing;
}

void AnimatedSprite3D::play(const StringName &p_animation) {
	if (p_animation) {
		set_animation(p_animation);
	}
	_set_playing(true);
}

void AnimatedSprite3D::stop() {
	_set_playing(false);
}

bool AnimatedSprite3D::is_playing() const {
	return playing;
}

void AnimatedSprite3D::_reset_timeout() {
	if (!playing) {
		return;
	}

	if (frames.is_valid() && frames->has_animation(animation)) {
		float speed = frames->get_animation_speed(animation);
		if (speed > 0) {
			timeout = 1.0 / speed;
		} else {
			timeout = 0;
		}
	} else {
		timeout = 0;
	}
}

void AnimatedSprite3D::set_animation(const StringName &p_animation) {
	if (animation == p_animation) {
		return;
	}

	animation = p_animation;
	_reset_timeout();
	set_frame(0);
	_change_notify();
	_queue_update();
}
StringName AnimatedSprite3D::get_animation() const {
	return animation;
}

String AnimatedSprite3D::get_configuration_warning() const {
	String warning = SpriteBase3D::get_configuration_warning();
	if (frames.is_null()) {
		if (warning != String()) {
			warning += "\n\n";
		}
		warning += TTR("A SpriteFrames resource must be created or set in the \"Frames\" property in order for AnimatedSprite3D to display frames.");
	}

	return warning;
}

void AnimatedSprite3D::get_argument_options(const StringName &p_function, int p_idx, List<String> *r_options) const {
#ifdef TOOLS_ENABLED
	const String quote_style = EDITOR_GET("text_editor/completion/use_single_quotes") ? "'" : "\"";
#else
	const String quote_style = "\"";
#endif

	if (p_idx == 0 && p_function == "play" && frames.is_valid()) {
		List<StringName> al;
		frames->get_animation_list(&al);
		for (List<StringName>::Element *E = al.front(); E; E = E->next()) {
			r_options->push_back(quote_style + String(E->get()) + quote_style);
		}
	}
	Node::get_argument_options(p_function, p_idx, r_options);
}

void AnimatedSprite3D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_sprite_frames", "sprite_frames"), &AnimatedSprite3D::set_sprite_frames);
	ClassDB::bind_method(D_METHOD("get_sprite_frames"), &AnimatedSprite3D::get_sprite_frames);

	ClassDB::bind_method(D_METHOD("set_animation", "animation"), &AnimatedSprite3D::set_animation);
	ClassDB::bind_method(D_METHOD("get_animation"), &AnimatedSprite3D::get_animation);

	ClassDB::bind_method(D_METHOD("_set_playing", "playing"), &AnimatedSprite3D::_set_playing);
	ClassDB::bind_method(D_METHOD("_is_playing"), &AnimatedSprite3D::_is_playing);

	ClassDB::bind_method(D_METHOD("play", "anim"), &AnimatedSprite3D::play, DEFVAL(StringName()));
	ClassDB::bind_method(D_METHOD("stop"), &AnimatedSprite3D::stop);
	ClassDB::bind_method(D_METHOD("is_playing"), &AnimatedSprite3D::is_playing);

	ClassDB::bind_method(D_METHOD("set_frame", "frame"), &AnimatedSprite3D::set_frame);
	ClassDB::bind_method(D_METHOD("get_frame"), &AnimatedSprite3D::get_frame);

	ClassDB::bind_method(D_METHOD("_res_changed"), &AnimatedSprite3D::_res_changed);

	ADD_SIGNAL(MethodInfo("frame_changed"));
	ADD_SIGNAL(MethodInfo("animation_finished"));

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "frames", PROPERTY_HINT_RESOURCE_TYPE, "SpriteFrames"), "set_sprite_frames", "get_sprite_frames");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "animation"), "set_animation", "get_animation");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "frame"), "set_frame", "get_frame");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "playing"), "_set_playing", "_is_playing");
}

AnimatedSprite3D::AnimatedSprite3D() {
	frame = 0;
	playing = false;
	animation = "default";
	timeout = 0;
}

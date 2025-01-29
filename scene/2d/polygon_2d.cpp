/**************************************************************************/
/*  polygon_2d.cpp                                                        */
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

#include "polygon_2d.h"

#include "core/math/geometry_2d.h"
#include "scene/resources/2d/navigation_mesh_source_geometry_data_2d.h"
#include "scene/resources/2d/navigation_polygon.h"
#include "servers/navigation_server_2d.h"
#include "skeleton_2d.h"

Callable Polygon2D::_navmesh_source_geometry_parsing_callback;
RID Polygon2D::_navmesh_source_geometry_parser;

#ifdef TOOLS_ENABLED
Dictionary Polygon2D::_edit_get_state() const {
	Dictionary state = Node2D::_edit_get_state();
	state["offset"] = offset;
	return state;
}

void Polygon2D::_edit_set_state(const Dictionary &p_state) {
	Node2D::_edit_set_state(p_state);
	set_offset(p_state["offset"]);
}

void Polygon2D::_edit_set_pivot(const Point2 &p_pivot) {
	set_position(get_transform().xform(p_pivot));
	set_offset(get_offset() - p_pivot);
}

Point2 Polygon2D::_edit_get_pivot() const {
	return Vector2();
}

bool Polygon2D::_edit_use_pivot() const {
	return true;
}
#endif // TOOLS_ENABLED

#ifdef DEBUG_ENABLED
Rect2 Polygon2D::_edit_get_rect() const {
	if (rect_cache_dirty) {
		int l = polygon.size();
		const Vector2 *r = polygon.ptr();
		item_rect = Rect2();
		for (int i = 0; i < l; i++) {
			Vector2 pos = r[i] + offset;
			if (i == 0) {
				item_rect.position = pos;
			} else {
				item_rect.expand_to(pos);
			}
		}
		rect_cache_dirty = false;
	}

	return item_rect;
}

bool Polygon2D::_edit_use_rect() const {
	return polygon.size() > 0;
}

bool Polygon2D::_edit_is_selected_on_click(const Point2 &p_point, double p_tolerance) const {
	Vector<Vector2> polygon2d = Variant(polygon);
	if (internal_vertices > 0) {
		polygon2d.resize(polygon2d.size() - internal_vertices);
	}
	return Geometry2D::is_point_in_polygon(p_point - get_offset(), polygon2d);
}
#endif // DEBUG_ENABLED

void Polygon2D::_validate_property(PropertyInfo &p_property) const {
	if (!invert && p_property.name == "invert_border") {
		p_property.usage = PROPERTY_USAGE_NO_EDITOR;
	}
}

void Polygon2D::_skeleton_bone_setup_changed() {
	queue_redraw();
}

void Polygon2D::_notification(int p_what) {
	if (p_what == NOTIFICATION_TRANSFORM_CHANGED && !Engine::get_singleton()->is_editor_hint()) {
		return; // Mesh recreation for NOTIFICATION_TRANSFORM_CHANGED is only needed in editor.
	}

	switch (p_what) {
		case NOTIFICATION_TRANSFORM_CHANGED:
		case NOTIFICATION_DRAW: {
			if (polygon.size() < 3) {
				return;
			}

			Skeleton2D *skeleton_node = nullptr;
			if (has_node(skeleton)) {
				skeleton_node = Object::cast_to<Skeleton2D>(get_node(skeleton));
			}

			ObjectID new_skeleton_id;

			if (skeleton_node && !invert && bone_weights.size()) {
				RS::get_singleton()->canvas_item_attach_skeleton(get_canvas_item(), skeleton_node->get_skeleton());
				new_skeleton_id = skeleton_node->get_instance_id();
			} else {
				RS::get_singleton()->canvas_item_attach_skeleton(get_canvas_item(), RID());
			}

			if (new_skeleton_id != current_skeleton_id) {
				Object *old_skeleton = ObjectDB::get_instance(current_skeleton_id);
				if (old_skeleton) {
					old_skeleton->disconnect("bone_setup_changed", callable_mp(this, &Polygon2D::_skeleton_bone_setup_changed));
				}

				if (skeleton_node) {
					skeleton_node->connect("bone_setup_changed", callable_mp(this, &Polygon2D::_skeleton_bone_setup_changed));
				}

				current_skeleton_id = new_skeleton_id;
			}

			Vector<Vector2> points;
			Vector<Vector2> uvs;
			Vector<int> bones;
			Vector<float> weights;

			int len = polygon.size();
			if ((invert || polygons.size() == 0) && internal_vertices > 0) {
				//if no polygons are around, internal vertices must not be drawn, else let them be
				len -= internal_vertices;
			}

			if (len <= 0) {
				return;
			}
			points.resize(len);

			{
				const Vector2 *polyr = polygon.ptr();
				for (int i = 0; i < len; i++) {
					points.write[i] = polyr[i] + offset;
				}
			}

			if (invert) {
				Rect2 bounds;
				int highest_idx = -1;
				real_t highest_y = -1e20;
				real_t sum = 0.0;

				for (int i = 0; i < len; i++) {
					if (i == 0) {
						bounds.position = points[i];
					} else {
						bounds.expand_to(points[i]);
					}
					if (points[i].y > highest_y) {
						highest_idx = i;
						highest_y = points[i].y;
					}
					int ni = (i + 1) % len;
					sum += (points[ni].x - points[i].x) * (points[ni].y + points[i].y);
				}

				bounds = bounds.grow(invert_border);

				Vector2 ep[7] = {
					Vector2(points[highest_idx].x, points[highest_idx].y + invert_border),
					Vector2(bounds.position + bounds.size),
					Vector2(bounds.position + Vector2(bounds.size.x, 0)),
					Vector2(bounds.position),
					Vector2(bounds.position + Vector2(0, bounds.size.y)),
					Vector2(points[highest_idx].x - CMP_EPSILON, points[highest_idx].y + invert_border),
					Vector2(points[highest_idx].x - CMP_EPSILON, points[highest_idx].y),
				};

				if (sum > 0) {
					SWAP(ep[1], ep[4]);
					SWAP(ep[2], ep[3]);
					SWAP(ep[5], ep[0]);
					SWAP(ep[6], points.write[highest_idx]);
				}

				points.resize(points.size() + 7);
				for (int i = points.size() - 1; i >= highest_idx + 7; i--) {
					points.write[i] = points[i - 7];
				}

				for (int i = 0; i < 7; i++) {
					points.write[highest_idx + i + 1] = ep[i];
				}

				len = points.size();
			}

			if (texture.is_valid()) {
				Transform2D texmat(tex_rot, tex_ofs);
				texmat.scale(tex_scale);
				Size2 tex_size = texture->get_size();

				uvs.resize(len);

				if (points.size() == uv.size()) {
					const Vector2 *uvr = uv.ptr();

					for (int i = 0; i < len; i++) {
						uvs.write[i] = texmat.xform(uvr[i]) / tex_size;
					}

				} else {
					for (int i = 0; i < len; i++) {
						uvs.write[i] = texmat.xform(points[i]) / tex_size;
					}
				}
			}

			if (skeleton_node && !invert && bone_weights.size()) {
				//a skeleton is set! fill indices and weights
				int vc = len;
				bones.resize(vc * 4);
				weights.resize(vc * 4);

				int *bonesw = bones.ptrw();
				float *weightsw = weights.ptrw();

				for (int i = 0; i < vc * 4; i++) {
					bonesw[i] = 0;
					weightsw[i] = 0;
				}

				for (int i = 0; i < bone_weights.size(); i++) {
					if (bone_weights[i].weights.size() != points.size()) {
						continue; //different number of vertices, sorry not using.
					}
					if (!skeleton_node->has_node(bone_weights[i].path)) {
						continue; //node does not exist
					}
					Bone2D *bone = Object::cast_to<Bone2D>(skeleton_node->get_node(bone_weights[i].path));
					if (!bone) {
						continue;
					}

					int bone_index = bone->get_index_in_skeleton();
					const float *r = bone_weights[i].weights.ptr();
					for (int j = 0; j < vc; j++) {
						if (r[j] == 0.0) {
							continue; //weight is unpainted, skip
						}
						//find an index with a weight
						for (int k = 0; k < 4; k++) {
							if (weightsw[j * 4 + k] < r[j]) {
								//this is less than this weight, insert weight!
								for (int l = 3; l > k; l--) {
									weightsw[j * 4 + l] = weightsw[j * 4 + l - 1];
									bonesw[j * 4 + l] = bonesw[j * 4 + l - 1];
								}
								weightsw[j * 4 + k] = r[j];
								bonesw[j * 4 + k] = bone_index;
								break;
							}
						}
					}
				}

				//normalize the weights
				for (int i = 0; i < vc; i++) {
					real_t tw = 0.0;
					for (int j = 0; j < 4; j++) {
						tw += weightsw[i * 4 + j];
					}
					if (tw == 0) {
						continue; //unpainted, do nothing
					}

					//normalize
					for (int j = 0; j < 4; j++) {
						weightsw[i * 4 + j] /= tw;
					}
				}
			}

			Vector<Color> colors;
			colors.resize(len);

			if (vertex_colors.size() == points.size()) {
				const Color *color_r = vertex_colors.ptr();
				for (int i = 0; i < len; i++) {
					colors.write[i] = color_r[i];
				}
			} else {
				for (int i = 0; i < len; i++) {
					colors.write[i] = color;
				}
			}

			Vector<int> index_array;

			if (invert || polygons.size() == 0) {
				index_array = Geometry2D::triangulate_polygon(points);
			} else {
				//draw individual polygons
				for (int i = 0; i < polygons.size(); i++) {
					Vector<int> src_indices = polygons[i];
					int ic = src_indices.size();
					if (ic < 3) {
						continue;
					}
					const int *r = src_indices.ptr();

					Vector<Vector2> tmp_points;
					tmp_points.resize(ic);

					for (int j = 0; j < ic; j++) {
						int idx = r[j];
						ERR_CONTINUE(idx < 0 || idx >= points.size());
						tmp_points.write[j] = points[r[j]];
					}
					Vector<int> indices = Geometry2D::triangulate_polygon(tmp_points);
					int ic2 = indices.size();
					const int *r2 = indices.ptr();

					int bic = index_array.size();
					index_array.resize(bic + ic2);
					int *w2 = index_array.ptrw();

					for (int j = 0; j < ic2; j++) {
						w2[j + bic] = r[r2[j]];
					}
				}
			}

			RS::get_singleton()->mesh_clear(mesh);

			if (index_array.size()) {
				Array arr;
				arr.resize(RS::ARRAY_MAX);
				arr[RS::ARRAY_VERTEX] = points;
				if (uvs.size() == points.size()) {
					arr[RS::ARRAY_TEX_UV] = uvs;
				}
				if (colors.size() == points.size()) {
					arr[RS::ARRAY_COLOR] = colors;
				}

				if (bones.size() == points.size() * 4) {
					arr[RS::ARRAY_BONES] = bones;
					arr[RS::ARRAY_WEIGHTS] = weights;
				}

				arr[RS::ARRAY_INDEX] = index_array;

				RS::SurfaceData sd;

				if (skeleton_node) {
					// Compute transform between mesh and skeleton for runtime AABB compute.
					const Transform2D mesh_transform = get_global_transform();
					const Transform2D skeleton_transform = skeleton_node->get_global_transform();
					const Transform2D mesh_to_sk2d = skeleton_transform.affine_inverse() * mesh_transform;

					// Convert 2d transform to 3d.
					sd.mesh_to_skeleton_xform.basis.rows[0][0] = mesh_to_sk2d.columns[0][0];
					sd.mesh_to_skeleton_xform.basis.rows[1][0] = mesh_to_sk2d.columns[0][1];
					sd.mesh_to_skeleton_xform.origin.x = mesh_to_sk2d.get_origin().x;

					sd.mesh_to_skeleton_xform.basis.rows[0][1] = mesh_to_sk2d.columns[1][0];
					sd.mesh_to_skeleton_xform.basis.rows[1][1] = mesh_to_sk2d.columns[1][1];
					sd.mesh_to_skeleton_xform.origin.y = mesh_to_sk2d.get_origin().y;
				}

				Error err = RS::get_singleton()->mesh_create_surface_data_from_arrays(&sd, RS::PRIMITIVE_TRIANGLES, arr, Array(), Dictionary(), RS::ARRAY_FLAG_USE_2D_VERTICES);
				if (err != OK) {
					return;
				}

				RS::get_singleton()->mesh_add_surface(mesh, sd);
				RS::get_singleton()->canvas_item_add_mesh(get_canvas_item(), mesh, Transform2D(), Color(1, 1, 1), texture.is_valid() ? texture->get_rid() : RID());
			}

		} break;
	}
}

void Polygon2D::set_polygon(const Vector<Vector2> &p_polygon) {
	polygon = p_polygon;
	rect_cache_dirty = true;
	queue_redraw();
}

Vector<Vector2> Polygon2D::get_polygon() const {
	return polygon;
}

void Polygon2D::set_internal_vertex_count(int p_count) {
	internal_vertices = p_count;
}

int Polygon2D::get_internal_vertex_count() const {
	return internal_vertices;
}

void Polygon2D::set_uv(const Vector<Vector2> &p_uv) {
	uv = p_uv;
	queue_redraw();
}

Vector<Vector2> Polygon2D::get_uv() const {
	return uv;
}

void Polygon2D::set_polygons(const Array &p_polygons) {
	polygons = p_polygons;
	queue_redraw();
}

Array Polygon2D::get_polygons() const {
	return polygons;
}

void Polygon2D::set_color(const Color &p_color) {
	color = p_color;
	queue_redraw();
}

Color Polygon2D::get_color() const {
	return color;
}

void Polygon2D::set_vertex_colors(const Vector<Color> &p_colors) {
	vertex_colors = p_colors;
	queue_redraw();
}

Vector<Color> Polygon2D::get_vertex_colors() const {
	return vertex_colors;
}

void Polygon2D::set_texture(const Ref<Texture2D> &p_texture) {
	texture = p_texture;
	queue_redraw();
}

Ref<Texture2D> Polygon2D::get_texture() const {
	return texture;
}

void Polygon2D::set_texture_offset(const Vector2 &p_offset) {
	tex_ofs = p_offset;
	queue_redraw();
}

Vector2 Polygon2D::get_texture_offset() const {
	return tex_ofs;
}

void Polygon2D::set_texture_rotation(real_t p_rot) {
	tex_rot = p_rot;
	queue_redraw();
}

real_t Polygon2D::get_texture_rotation() const {
	return tex_rot;
}

void Polygon2D::set_texture_scale(const Size2 &p_scale) {
	tex_scale = p_scale;
	queue_redraw();
}

Size2 Polygon2D::get_texture_scale() const {
	return tex_scale;
}

void Polygon2D::set_invert(bool p_invert) {
	invert = p_invert;
	queue_redraw();
	notify_property_list_changed();
}

bool Polygon2D::get_invert() const {
	return invert;
}

void Polygon2D::set_antialiased(bool p_antialiased) {
	antialiased = p_antialiased;
	queue_redraw();
}

bool Polygon2D::get_antialiased() const {
	return antialiased;
}

void Polygon2D::set_invert_border(real_t p_invert_border) {
	invert_border = p_invert_border;
	queue_redraw();
}

real_t Polygon2D::get_invert_border() const {
	return invert_border;
}

void Polygon2D::set_offset(const Vector2 &p_offset) {
	offset = p_offset;
	rect_cache_dirty = true;
	queue_redraw();
}

Vector2 Polygon2D::get_offset() const {
	return offset;
}

void Polygon2D::add_bone(const NodePath &p_path, const Vector<float> &p_weights) {
	Bone bone;
	bone.path = p_path;
	bone.weights = p_weights;
	bone_weights.push_back(bone);
}

int Polygon2D::get_bone_count() const {
	return bone_weights.size();
}

NodePath Polygon2D::get_bone_path(int p_index) const {
	ERR_FAIL_INDEX_V(p_index, bone_weights.size(), NodePath());
	return bone_weights[p_index].path;
}

Vector<float> Polygon2D::get_bone_weights(int p_index) const {
	ERR_FAIL_INDEX_V(p_index, bone_weights.size(), Vector<float>());
	return bone_weights[p_index].weights;
}

void Polygon2D::erase_bone(int p_idx) {
	ERR_FAIL_INDEX(p_idx, bone_weights.size());
	bone_weights.remove_at(p_idx);
}

void Polygon2D::clear_bones() {
	bone_weights.clear();
}

void Polygon2D::set_bone_weights(int p_index, const Vector<float> &p_weights) {
	ERR_FAIL_INDEX(p_index, bone_weights.size());
	bone_weights.write[p_index].weights = p_weights;
	queue_redraw();
}

void Polygon2D::set_bone_path(int p_index, const NodePath &p_path) {
	ERR_FAIL_INDEX(p_index, bone_weights.size());
	bone_weights.write[p_index].path = p_path;
	queue_redraw();
}

Array Polygon2D::_get_bones() const {
	Array bones;
	for (int i = 0; i < get_bone_count(); i++) {
		// Convert path property to String to avoid errors due to invalid node path in editor,
		// because it's relative to the Skeleton2D node and not Polygon2D.
		bones.push_back(String(get_bone_path(i)));
		bones.push_back(get_bone_weights(i));
	}
	return bones;
}

void Polygon2D::_set_bones(const Array &p_bones) {
	ERR_FAIL_COND(p_bones.size() & 1);
	clear_bones();
	for (int i = 0; i < p_bones.size(); i += 2) {
		// Convert back from String to NodePath.
		add_bone(NodePath(p_bones[i]), p_bones[i + 1]);
	}
}

void Polygon2D::set_skeleton(const NodePath &p_skeleton) {
	if (skeleton == p_skeleton) {
		return;
	}
	skeleton = p_skeleton;
	queue_redraw();
}

NodePath Polygon2D::get_skeleton() const {
	return skeleton;
}

void Polygon2D::navmesh_parse_init() {
	ERR_FAIL_NULL(NavigationServer2D::get_singleton());
	if (!_navmesh_source_geometry_parser.is_valid()) {
		_navmesh_source_geometry_parsing_callback = callable_mp_static(&Polygon2D::navmesh_parse_source_geometry);
		_navmesh_source_geometry_parser = NavigationServer2D::get_singleton()->source_geometry_parser_create();
		NavigationServer2D::get_singleton()->source_geometry_parser_set_callback(_navmesh_source_geometry_parser, _navmesh_source_geometry_parsing_callback);
	}
}

void Polygon2D::navmesh_parse_source_geometry(const Ref<NavigationPolygon> &p_navigation_mesh, Ref<NavigationMeshSourceGeometryData2D> p_source_geometry_data, Node *p_node) {
	Polygon2D *polygon_2d = Object::cast_to<Polygon2D>(p_node);

	if (polygon_2d == nullptr) {
		return;
	}

	NavigationPolygon::ParsedGeometryType parsed_geometry_type = p_navigation_mesh->get_parsed_geometry_type();

	if (parsed_geometry_type == NavigationPolygon::PARSED_GEOMETRY_MESH_INSTANCES || parsed_geometry_type == NavigationPolygon::PARSED_GEOMETRY_BOTH) {
		const Transform2D polygon_2d_xform = p_source_geometry_data->root_node_transform * polygon_2d->get_global_transform();

		Vector<Vector2> shape_outline = polygon_2d->get_polygon();
		for (int i = 0; i < shape_outline.size(); i++) {
			shape_outline.write[i] = polygon_2d_xform.xform(shape_outline[i]);
		}

		p_source_geometry_data->add_obstruction_outline(shape_outline);
	}
}

void Polygon2D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_polygon", "polygon"), &Polygon2D::set_polygon);
	ClassDB::bind_method(D_METHOD("get_polygon"), &Polygon2D::get_polygon);

	ClassDB::bind_method(D_METHOD("set_uv", "uv"), &Polygon2D::set_uv);
	ClassDB::bind_method(D_METHOD("get_uv"), &Polygon2D::get_uv);

	ClassDB::bind_method(D_METHOD("set_color", "color"), &Polygon2D::set_color);
	ClassDB::bind_method(D_METHOD("get_color"), &Polygon2D::get_color);

	ClassDB::bind_method(D_METHOD("set_polygons", "polygons"), &Polygon2D::set_polygons);
	ClassDB::bind_method(D_METHOD("get_polygons"), &Polygon2D::get_polygons);

	ClassDB::bind_method(D_METHOD("set_vertex_colors", "vertex_colors"), &Polygon2D::set_vertex_colors);
	ClassDB::bind_method(D_METHOD("get_vertex_colors"), &Polygon2D::get_vertex_colors);

	ClassDB::bind_method(D_METHOD("set_texture", "texture"), &Polygon2D::set_texture);
	ClassDB::bind_method(D_METHOD("get_texture"), &Polygon2D::get_texture);

	ClassDB::bind_method(D_METHOD("set_texture_offset", "texture_offset"), &Polygon2D::set_texture_offset);
	ClassDB::bind_method(D_METHOD("get_texture_offset"), &Polygon2D::get_texture_offset);

	ClassDB::bind_method(D_METHOD("set_texture_rotation", "texture_rotation"), &Polygon2D::set_texture_rotation);
	ClassDB::bind_method(D_METHOD("get_texture_rotation"), &Polygon2D::get_texture_rotation);

	ClassDB::bind_method(D_METHOD("set_texture_scale", "texture_scale"), &Polygon2D::set_texture_scale);
	ClassDB::bind_method(D_METHOD("get_texture_scale"), &Polygon2D::get_texture_scale);

	ClassDB::bind_method(D_METHOD("set_invert_enabled", "invert"), &Polygon2D::set_invert);
	ClassDB::bind_method(D_METHOD("get_invert_enabled"), &Polygon2D::get_invert);

	ClassDB::bind_method(D_METHOD("set_antialiased", "antialiased"), &Polygon2D::set_antialiased);
	ClassDB::bind_method(D_METHOD("get_antialiased"), &Polygon2D::get_antialiased);

	ClassDB::bind_method(D_METHOD("set_invert_border", "invert_border"), &Polygon2D::set_invert_border);
	ClassDB::bind_method(D_METHOD("get_invert_border"), &Polygon2D::get_invert_border);

	ClassDB::bind_method(D_METHOD("set_offset", "offset"), &Polygon2D::set_offset);
	ClassDB::bind_method(D_METHOD("get_offset"), &Polygon2D::get_offset);

	ClassDB::bind_method(D_METHOD("add_bone", "path", "weights"), &Polygon2D::add_bone);
	ClassDB::bind_method(D_METHOD("get_bone_count"), &Polygon2D::get_bone_count);
	ClassDB::bind_method(D_METHOD("get_bone_path", "index"), &Polygon2D::get_bone_path);
	ClassDB::bind_method(D_METHOD("get_bone_weights", "index"), &Polygon2D::get_bone_weights);
	ClassDB::bind_method(D_METHOD("erase_bone", "index"), &Polygon2D::erase_bone);
	ClassDB::bind_method(D_METHOD("clear_bones"), &Polygon2D::clear_bones);
	ClassDB::bind_method(D_METHOD("set_bone_path", "index", "path"), &Polygon2D::set_bone_path);
	ClassDB::bind_method(D_METHOD("set_bone_weights", "index", "weights"), &Polygon2D::set_bone_weights);

	ClassDB::bind_method(D_METHOD("set_skeleton", "skeleton"), &Polygon2D::set_skeleton);
	ClassDB::bind_method(D_METHOD("get_skeleton"), &Polygon2D::get_skeleton);

	ClassDB::bind_method(D_METHOD("set_internal_vertex_count", "internal_vertex_count"), &Polygon2D::set_internal_vertex_count);
	ClassDB::bind_method(D_METHOD("get_internal_vertex_count"), &Polygon2D::get_internal_vertex_count);

	ClassDB::bind_method(D_METHOD("_set_bones", "bones"), &Polygon2D::_set_bones);
	ClassDB::bind_method(D_METHOD("_get_bones"), &Polygon2D::_get_bones);

	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "color"), "set_color", "get_color");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "offset"), "set_offset", "get_offset");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "antialiased"), "set_antialiased", "get_antialiased");

	ADD_GROUP("Texture", "texture_");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "texture", PROPERTY_HINT_RESOURCE_TYPE, "Texture2D"), "set_texture", "get_texture");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "texture_offset", PROPERTY_HINT_NONE, "suffix:px"), "set_texture_offset", "get_texture_offset");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "texture_scale", PROPERTY_HINT_LINK), "set_texture_scale", "get_texture_scale");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "texture_rotation", PROPERTY_HINT_RANGE, "-360,360,0.1,or_less,or_greater,radians_as_degrees"), "set_texture_rotation", "get_texture_rotation");

	ADD_GROUP("Skeleton", "");
	ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "skeleton", PROPERTY_HINT_NODE_PATH_VALID_TYPES, "Skeleton2D"), "set_skeleton", "get_skeleton");

	ADD_GROUP("Invert", "invert_");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "invert_enabled"), "set_invert_enabled", "get_invert_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "invert_border", PROPERTY_HINT_RANGE, "0.1,16384,0.1,suffix:px"), "set_invert_border", "get_invert_border");

	ADD_GROUP("Data", "");
	ADD_PROPERTY(PropertyInfo(Variant::PACKED_VECTOR2_ARRAY, "polygon"), "set_polygon", "get_polygon");
	ADD_PROPERTY(PropertyInfo(Variant::PACKED_VECTOR2_ARRAY, "uv"), "set_uv", "get_uv");
	ADD_PROPERTY(PropertyInfo(Variant::PACKED_COLOR_ARRAY, "vertex_colors"), "set_vertex_colors", "get_vertex_colors");
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "polygons"), "set_polygons", "get_polygons");
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "bones", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NO_EDITOR | PROPERTY_USAGE_INTERNAL), "_set_bones", "_get_bones");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "internal_vertex_count", PROPERTY_HINT_RANGE, "0,1000"), "set_internal_vertex_count", "get_internal_vertex_count");
}

Polygon2D::Polygon2D() {
	mesh = RS::get_singleton()->mesh_create();
}

Polygon2D::~Polygon2D() {
	// This will free the internally-allocated mesh instance, if allocated.
	ERR_FAIL_NULL(RenderingServer::get_singleton());
	RS::get_singleton()->canvas_item_attach_skeleton(get_canvas_item(), RID());
	RS::get_singleton()->free(mesh);
}

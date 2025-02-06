/**************************************************************************/
/*  csg_shape_2d.cpp                                                      */
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

#include "csg_shape_2d.h"

#include "core/math/geometry_2d.h"
#include "scene/2d/light_occluder_2d.h"
#include "scene/resources/2d/concave_polygon_shape_2d.h"
#include "scene/resources/2d/convex_polygon_shape_2d.h"
#include "scene/resources/2d/navigation_polygon.h"
#include "scene/resources/world_2d.h"
#include "servers/physics_server_2d.h"

#include "thirdparty/misc/polypartition.h"

void CSGShape2D::set_use_vertex_color(bool p_enable) {
	if (use_vertex_color == p_enable) {
		return;
	}

	use_vertex_color = p_enable;

	_make_dirty();

	notify_property_list_changed();
}

bool CSGShape2D::is_using_vertex_color() const {
	return use_vertex_color;
}

void CSGShape2D::set_vertex_color(const Color &p_color) {
	if (vertex_color == p_color) {
		return;
	}

	vertex_color = p_color;

	_make_dirty();
}

Color CSGShape2D::get_vertex_color() const {
	return vertex_color;
}

void CSGShape2D::set_use_collision(bool p_enable) {
	if (use_collision == p_enable) {
		return;
	}

	use_collision = p_enable;

	if (!is_inside_tree() || !is_root_shape()) {
		return;
	}

	if (use_collision) {
		_create_collision();
	} else {
		_free_collision();
	}

	notify_property_list_changed();
}

bool CSGShape2D::is_using_collision() const {
	return use_collision;
}

void CSGShape2D::set_collision_shape_type(CSGShape2D::CollisionShapeType p_type) {
	ERR_FAIL_INDEX(p_type, COLLISION_SHAPE_TYPE_MAX);
	collision_shape_type = p_type;
	_update_collision_shapes();
	queue_redraw();
}

CSGShape2D::CollisionShapeType CSGShape2D::get_collision_shape_type() const {
	return collision_shape_type;
}

void CSGShape2D::_create_collision() {
	if (!is_inside_tree() || !is_root_shape()) {
		return;
	}

	_update_collision_shapes();

	if (root_collision_instance.is_null()) {
		root_collision_instance = PhysicsServer2D::get_singleton()->body_create();
	}

	PhysicsServer2D::get_singleton()->body_set_mode(root_collision_instance, PhysicsServer2D::BODY_MODE_STATIC);
	PhysicsServer2D::get_singleton()->body_set_state(root_collision_instance, PhysicsServer2D::BODY_STATE_TRANSFORM, get_global_transform());

	PhysicsServer2D::get_singleton()->body_clear_shapes(root_collision_instance);
	for (Ref<ConvexPolygonShape2D> collision_shape : root_collision_shapes_convex) {
		PhysicsServer2D::get_singleton()->body_add_shape(root_collision_instance, collision_shape->get_rid());
	}
	for (Ref<ConcavePolygonShape2D> collision_shape : root_collision_shapes_concave) {
		PhysicsServer2D::get_singleton()->body_add_shape(root_collision_instance, collision_shape->get_rid());
	}

	PhysicsServer2D::get_singleton()->body_set_space(root_collision_instance, get_world_2d()->get_space());
	PhysicsServer2D::get_singleton()->body_attach_object_instance_id(root_collision_instance, get_instance_id());

	set_collision_layer(collision_layer);
	set_collision_mask(collision_mask);
	set_collision_priority(collision_priority);

	_make_dirty();
}

void CSGShape2D::_free_collision() {
	if (root_collision_instance.is_valid()) {
		PhysicsServer2D::get_singleton()->free(root_collision_instance);
	}
	root_collision_instance = RID();
	root_collision_shapes_convex.clear();
	root_collision_shapes_concave.clear();
}

void CSGShape2D::set_collision_layer(uint32_t p_layer) {
	collision_layer = p_layer;
	if (root_collision_instance.is_valid()) {
		PhysicsServer2D::get_singleton()->body_set_collision_layer(root_collision_instance, p_layer);
	}
}

uint32_t CSGShape2D::get_collision_layer() const {
	return collision_layer;
}

void CSGShape2D::set_collision_mask(uint32_t p_mask) {
	collision_mask = p_mask;
	if (root_collision_instance.is_valid()) {
		PhysicsServer2D::get_singleton()->body_set_collision_mask(root_collision_instance, p_mask);
	}
}

uint32_t CSGShape2D::get_collision_mask() const {
	return collision_mask;
}

void CSGShape2D::set_collision_layer_value(int p_layer_number, bool p_value) {
	ERR_FAIL_COND_MSG(p_layer_number < 1, "Collision layer number must be between 1 and 32 inclusive.");
	ERR_FAIL_COND_MSG(p_layer_number > 32, "Collision layer number must be between 1 and 32 inclusive.");
	uint32_t layer = get_collision_layer();
	if (p_value) {
		layer |= 1 << (p_layer_number - 1);
	} else {
		layer &= ~(1 << (p_layer_number - 1));
	}
	set_collision_layer(layer);
}

bool CSGShape2D::get_collision_layer_value(int p_layer_number) const {
	ERR_FAIL_COND_V_MSG(p_layer_number < 1, false, "Collision layer number must be between 1 and 32 inclusive.");
	ERR_FAIL_COND_V_MSG(p_layer_number > 32, false, "Collision layer number must be between 1 and 32 inclusive.");
	return get_collision_layer() & (1 << (p_layer_number - 1));
}

void CSGShape2D::set_collision_mask_value(int p_layer_number, bool p_value) {
	ERR_FAIL_COND_MSG(p_layer_number < 1, "Collision layer number must be between 1 and 32 inclusive.");
	ERR_FAIL_COND_MSG(p_layer_number > 32, "Collision layer number must be between 1 and 32 inclusive.");
	uint32_t mask = get_collision_mask();
	if (p_value) {
		mask |= 1 << (p_layer_number - 1);
	} else {
		mask &= ~(1 << (p_layer_number - 1));
	}
	set_collision_mask(mask);
}

bool CSGShape2D::get_collision_mask_value(int p_layer_number) const {
	ERR_FAIL_COND_V_MSG(p_layer_number < 1, false, "Collision layer number must be between 1 and 32 inclusive.");
	ERR_FAIL_COND_V_MSG(p_layer_number > 32, false, "Collision layer number must be between 1 and 32 inclusive.");
	return get_collision_mask() & (1 << (p_layer_number - 1));
}

void CSGShape2D::set_collision_priority(real_t p_priority) {
	collision_priority = p_priority;
	if (root_collision_instance.is_valid()) {
		PhysicsServer2D::get_singleton()->body_set_collision_priority(root_collision_instance, p_priority);
	}
}

real_t CSGShape2D::get_collision_priority() const {
	return collision_priority;
}

bool CSGShape2D::is_root_shape() const {
	return !parent_shape;
}

void CSGShape2D::_make_dirty(bool p_parent_removing) {
	if ((p_parent_removing || is_root_shape()) && !dirty) {
		callable_mp(this, &CSGShape2D::_update_shape).call_deferred(); // Must be deferred; otherwise, is_root_shape() will use the previous parent.
	}

	if (!is_root_shape()) {
		callable_mp(this, &CSGShape2D::_update_shape).call_deferred();
		parent_shape->_make_dirty();
	} else if (!dirty) {
		callable_mp(this, &CSGShape2D::_update_shape).call_deferred();
	}

	dirty = true;
}

bool CSGShape2D::is_point_in_outlines(const Point2 &p_point) const {
	for (uint32_t i = 0; i < mesh_outlines.size(); i++) {
		const int outline_size = mesh_outlines[i].size();
		if (outline_size < 3) {
			continue;
		}
		if (Geometry2D::is_point_in_polygon(p_point, Variant(mesh_outlines[i]))) {
			return true;
		}
	}
	return false;
}

void CSGShape2D::_recursive_process_polytree_items(List<TPPLPoly> &p_tppl_in_polygon, const Clipper2Lib::PolyPathD *p_polypath_item, LocalVector<Vector<Vector2>> &p_mesh_outlines) {
	TPPLPoly tp;
	int polypath_size = p_polypath_item->Polygon().size();
	tp.Init(polypath_size);

	Vector<Vector2> outline;
	outline.resize(polypath_size);

	int j = 0;
	for (const Clipper2Lib::PointD &polypath_point : p_polypath_item->Polygon()) {
		Vector2 vertex = Vector2(static_cast<real_t>(polypath_point.x), static_cast<real_t>(polypath_point.y));
		tp[j] = vertex;
		outline.write[j] = vertex;
		++j;
	}

	if (p_polypath_item->IsHole()) {
		tp.SetOrientation(TPPL_ORIENTATION_CW);
		tp.SetHole(true);
		if (!Geometry2D::is_polygon_clockwise(outline)) {
			outline.reverse();
		}
	} else {
		tp.SetOrientation(TPPL_ORIENTATION_CCW);
		if (Geometry2D::is_polygon_clockwise(outline)) {
			outline.reverse();
		}
	}
	p_tppl_in_polygon.push_back(tp);
	p_mesh_outlines.push_back(outline);

	for (size_t i = 0; i < p_polypath_item->Count(); i++) {
		const Clipper2Lib::PolyPathD *polypath_item = p_polypath_item->Child(i);
		_recursive_process_polytree_items(p_tppl_in_polygon, polypath_item, p_mesh_outlines);
	}
}

CSGBrush2D *CSGShape2D::_get_brush() {
	if (!dirty) {
		return brush;
	}
	if (brush) {
		memdelete(brush);
	}
	brush = nullptr;
	CSGBrush2D *n = _build_brush();

	Clipper2Lib::PathsD polygon_paths = n->poly_paths;

	for (int i = 0; i < get_child_count(); i++) {
		CSGShape2D *child = Object::cast_to<CSGShape2D>(get_child(i));
		if (!child || !child->is_visible()) {
			continue;
		}
		CSGBrush2D *child_brush = child->_get_brush();
		if (!child_brush) {
			continue;
		}
		CSGBrush2D transformed_brush;
		transformed_brush.copy_from(*child_brush, child->get_transform());

		Clipper2Lib::PathsD child_polygon_paths = transformed_brush.poly_paths;

		switch (child->get_operation()) {
			case CSGShape2D::Operation::OPERATION_UNION:
				polygon_paths = Clipper2Lib::Union(polygon_paths, child_polygon_paths, Clipper2Lib::FillRule::NonZero);
				break;
			case CSGShape2D::Operation::OPERATION_INTERSECTION:
				polygon_paths = Clipper2Lib::Intersect(polygon_paths, child_polygon_paths, Clipper2Lib::FillRule::NonZero);
				break;
			case CSGShape2D::Operation::OPERATION_SUBTRACTION:
				polygon_paths = Clipper2Lib::Difference(polygon_paths, child_polygon_paths, Clipper2Lib::FillRule::NonZero);
				break;
			default: {
				ERR_PRINT("CSGShape2D::Operation failed. Unrecognized operation.");
				return brush;
			}
		}
	}

	n->poly_paths = polygon_paths;

	Rect2 rect;
	if (n && !n->outlines.is_empty()) {
		rect.position = n->outlines[0].vertices[0];
		for (const CSGBrush2D::Outline &outline : n->outlines) {
			for (uint32_t i = 0; i < outline.vertices.size(); i++) {
				rect.expand_to(outline.vertices[i]);
			}
		}
	}
	node_rect = rect;

	brush = n;
	dirty = false;
	return brush;
}

void CSGShape2D::force_shape_update() {
	shape_update_forced = true;
	_update_shape();
	shape_update_forced = false;
}

void CSGShape2D::_update_shape() {
	if (Engine::get_singleton()->is_editor_hint()) {
		if (brush_mesh.is_null()) {
			brush_mesh.instantiate();
		}
		brush_mesh->clear_surfaces();

		{
			Array array;
			array.resize(Mesh::ARRAY_MAX);

			for (const Vector<Vector2> &brush_outline : brush_outlines) {
				Vector<Vector2> brush_mesh_vertices;
				brush_mesh_vertices.resize(brush_outline.size() * 2);
				Vector2 *brush_mesh_vertices_ptrw = brush_mesh_vertices.ptrw();

				for (uint32_t i = 0; i < brush_outline.size(); i++) {
					const Vector2 &vertex1 = brush_outline[i];
					const Vector2 &vertex2 = brush_outline[(i + 1) % brush_outline.size()];

					brush_mesh_vertices_ptrw[i * 2] = vertex1;
					brush_mesh_vertices_ptrw[i * 2 + 1] = vertex2;
				}

				array[Mesh::ARRAY_VERTEX] = brush_mesh_vertices;

				brush_mesh->add_surface_from_arrays(Mesh::PRIMITIVE_LINES, array, Array(), Dictionary(), RS::ARRAY_FLAG_USE_2D_VERTICES);
			}
		}
	}

	if (!is_root_shape()) {
		return;
	}

	mesh_vertices.clear();
	mesh_triangles.clear();
	mesh_convex_polygons.clear();
	mesh_outlines.clear();

	root_mesh.unref();
	root_edge_line_mesh.unref();
	root_outline_mesh.unref();

	CSGBrush2D *n = _get_brush();
	ERR_FAIL_NULL_MSG(n, "Cannot get CSGBrush2D.");

	root_mesh.instantiate();
	root_edge_line_mesh.instantiate();
	root_outline_mesh.instantiate();

	Clipper2Lib::PolyTreeD polytree;
	Clipper2Lib::ClipperD clipper_D;
	clipper_D.PreserveCollinear(false);

	clipper_D.AddSubject(n->poly_paths);
	clipper_D.Execute(Clipper2Lib::ClipType::Union, Clipper2Lib::FillRule::NonZero, polytree);

	List<TPPLPoly> tppl_in_polygon, tppl_out_polygon;

	for (size_t i = 0; i < polytree.Count(); i++) {
		const Clipper2Lib::PolyPathD *polypath_item = polytree[i];
		_recursive_process_polytree_items(tppl_in_polygon, polypath_item, mesh_outlines);
	}

	TPPLPartition tpart;

	if (tpart.ConvexPartition_HM(&tppl_in_polygon, &tppl_out_polygon) == 0) {
		ERR_PRINT("CSGShape2D outines to convex polygons conversion failed. Check input geometry for common shape outline and precision errors like self-overlapping shapes used in intersection operations.");
	}

	points_map.clear();

	for (List<TPPLPoly>::Element *I = tppl_out_polygon.front(); I; I = I->next()) {
		TPPLPoly &tp = I->get();

		Vector<int> new_polygon;

		for (int64_t i = 0; i < tp.GetNumPoints(); i++) {
			HashMap<Vector2, int>::Iterator E = points_map.find(tp[i]);
			if (!E) {
				E = points_map.insert(tp[i], mesh_vertices.size());
				mesh_vertices.push_back(tp[i]);
			}
			new_polygon.push_back(E->value);

			int new_polygon_size = new_polygon.size();
			if (new_polygon_size >= 3) {
				mesh_triangles.push_back(new_polygon[0]);
				mesh_triangles.push_back(new_polygon[new_polygon_size - 2]);
				mesh_triangles.push_back(new_polygon[new_polygon_size - 1]);
			}
		}

		mesh_convex_polygons.push_back(new_polygon);
	}

	if (!mesh_vertices.is_empty() && !mesh_triangles.is_empty()) {
		{
			Array array;
			array.resize(Mesh::ARRAY_MAX);

			Vector<Vector2> mesh_uvs;
			mesh_uvs.resize(mesh_vertices.size());

			Rect2 mesh_rect;
			mesh_rect.position = mesh_vertices[0];
			for (const Vector2 &vertex : mesh_vertices) {
				mesh_rect.expand_to(vertex);
			}

			real_t min_x = mesh_rect.position.x;
			real_t max_x = mesh_rect.position.x + mesh_rect.size.x;
			real_t min_y = mesh_rect.position.y;
			real_t max_y = mesh_rect.position.y + mesh_rect.size.y;

			Vector2 *mesh_uvs_ptrw = mesh_uvs.ptrw();
			int uv_index = 0;

			for (const Vector2 &vertex : mesh_vertices) {
				real_t uv_x = Math::remap(real_t(vertex.x), min_x, max_x, real_t(0.0), real_t(1.0));
				real_t uv_y = Math::remap(real_t(vertex.y), min_y, max_y, real_t(0.0), real_t(1.0));
				mesh_uvs_ptrw[uv_index++] = Vector2(uv_x, uv_y);
			}

			array[Mesh::ARRAY_VERTEX] = mesh_vertices;
			array[Mesh::ARRAY_INDEX] = mesh_triangles;
			array[Mesh::ARRAY_TEX_UV] = mesh_uvs;
			if (use_vertex_color) {
				Vector<Color> mesh_vertex_colors;
				mesh_vertex_colors.resize(mesh_vertices.size());
				mesh_vertex_colors.fill(vertex_color);
				array[Mesh::ARRAY_COLOR] = mesh_vertex_colors;
			}

			root_mesh->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, array, Array(), Dictionary(), RS::ARRAY_FLAG_USE_2D_VERTICES);
		}

		{
			Array array;
			array.resize(Mesh::ARRAY_MAX);

			for (uint32_t k = 0; k < mesh_convex_polygons.size(); k++) {
				Vector<Vector2> mesh_line_vertices;

				uint32_t polygon_vertex_count = mesh_convex_polygons[k].size();
				for (uint32_t i = 0; i < polygon_vertex_count; i++) {
					mesh_line_vertices.push_back(mesh_vertices[mesh_convex_polygons[k][i]]);
					mesh_line_vertices.push_back(mesh_vertices[mesh_convex_polygons[k][(i + 1) % polygon_vertex_count]]);
				}

				array[Mesh::ARRAY_VERTEX] = mesh_line_vertices;

				root_edge_line_mesh->add_surface_from_arrays(Mesh::PRIMITIVE_LINES, array, Array(), Dictionary(), RS::ARRAY_FLAG_USE_2D_VERTICES);
			}
		}
	}

	{
		Array array;
		array.resize(Mesh::ARRAY_MAX);

		for (const CSGBrush2D::Outline &outline : n->outlines) {
			Vector<Vector2> mesh_line_vertices;
			mesh_line_vertices.resize(outline.vertices.size() * 2);
			Vector2 *mesh_line_vertices_ptrw = mesh_line_vertices.ptrw();

			for (uint32_t i = 0; i < outline.vertices.size(); i++) {
				const Vector2 &vertex1 = outline.vertices[i];
				const Vector2 &vertex2 = outline.vertices[(i + 1) % outline.vertices.size()];

				mesh_line_vertices_ptrw[i * 2] = vertex1;
				mesh_line_vertices_ptrw[i * 2 + 1] = vertex2;
			}

			array[Mesh::ARRAY_VERTEX] = mesh_line_vertices;

			root_outline_mesh->add_surface_from_arrays(Mesh::PRIMITIVE_LINES, array, Array(), Dictionary(), RS::ARRAY_FLAG_USE_2D_VERTICES);
		}
	}

	queue_redraw();

	_update_collision_shapes();
}

void CSGShape2D::_update_collision_shapes() {
	if (!use_collision || !is_root_shape()) {
		return;
	}

	CSGBrush2D *n = _get_brush();
	if (!n) {
		return;
	}

	root_collision_shapes_convex.clear();
	root_collision_shapes_concave.clear();

	switch (collision_shape_type) {
		case CSGShape2D::CollisionShapeType::COLLISION_SHAPE_TYPE_CONVEX_POLYGONS:
			root_collision_shapes_convex.resize(mesh_convex_polygons.size());

			for (uint32_t i = 0; i < mesh_convex_polygons.size(); i++) {
				const Vector<int> &polygon_indices = mesh_convex_polygons[i];

				Vector<Vector2> convex_polygon_points;
				convex_polygon_points.resize(polygon_indices.size());

				Vector2 *convex_polygon_points_ptrw = convex_polygon_points.ptrw();

				for (uint32_t j = 0; j < polygon_indices.size(); j++) {
					convex_polygon_points_ptrw[j] = mesh_vertices[polygon_indices[j]];
				}

				Ref<ConvexPolygonShape2D> convex_shape;
				convex_shape.instantiate();
				convex_shape->set_points(convex_polygon_points);

				root_collision_shapes_convex[i] = convex_shape;
			}
			break;

		case CSGShape2D::CollisionShapeType::COLLISION_SHAPE_TYPE_CONCAVE_SEGMENTS:
			root_collision_shapes_concave.resize(mesh_outlines.size());

			for (uint32_t i = 0; i < mesh_outlines.size(); i++) {
				const Vector<Vector2> &outline = mesh_outlines[i];

				Vector<Vector2> concave_segments;
				concave_segments.resize(outline.size() * 2);

				Vector2 *concave_segments_ptrw = concave_segments.ptrw();
				const Vector2 *outline_ptr = outline.ptr();

				for (uint32_t j = 0; j < outline.size(); j++) {
					concave_segments_ptrw[j * 2] = outline_ptr[j];
					concave_segments_ptrw[j * 2 + 1] = outline_ptr[(j + 1) % outline.size()];
				}

				Ref<ConcavePolygonShape2D> concave_shape;
				concave_shape.instantiate();
				concave_shape->set_segments(concave_segments);

				root_collision_shapes_concave[i] = concave_shape;
			}
			break;

		default: {
			ERR_PRINT("CollisionShapeType failed. Unrecognized collision_shape_type.");
			break;
		}
	}

	if (_is_debug_collision_shape_visible()) {
		_update_debug_collision_shape();
	}

	if (root_collision_instance.is_valid()) {
		PhysicsServer2D::get_singleton()->body_clear_shapes(root_collision_instance);
		for (Ref<ConvexPolygonShape2D> collision_shape : root_collision_shapes_convex) {
			PhysicsServer2D::get_singleton()->body_add_shape(root_collision_instance, collision_shape->get_rid());
		}
		for (Ref<ConcavePolygonShape2D> collision_shape : root_collision_shapes_concave) {
			PhysicsServer2D::get_singleton()->body_add_shape(root_collision_instance, collision_shape->get_rid());
		}
	}
}

Ref<ArrayMesh> CSGShape2D::bake_static_mesh() const {
	Ref<ArrayMesh> baked_mesh;
	if (is_root_shape() && root_mesh.is_valid()) {
		baked_mesh = root_mesh;
	}
	return baked_mesh;
}

Array CSGShape2D::bake_collision_shapes() const {
	Array baked_collision_shapes;

	if (!is_root_shape()) {
		return baked_collision_shapes;
	}

	switch (collision_shape_type) {
		case CSGShape2D::CollisionShapeType::COLLISION_SHAPE_TYPE_CONVEX_POLYGONS:
			if (!root_collision_shapes_convex.is_empty()) {
				baked_collision_shapes.resize(root_collision_shapes_convex.size());

				for (uint32_t i = 0; i < root_collision_shapes_convex.size(); i++) {
					Ref<ConvexPolygonShape2D> convex_collision_shape;
					convex_collision_shape.instantiate();
					convex_collision_shape->set_points(root_collision_shapes_convex[i]->get_points());
					baked_collision_shapes[i] = convex_collision_shape;
				}
			}

			break;
		case CSGShape2D::CollisionShapeType::COLLISION_SHAPE_TYPE_CONCAVE_SEGMENTS:
			if (!root_collision_shapes_concave.is_empty()) {
				baked_collision_shapes.resize(root_collision_shapes_concave.size());

				for (uint32_t i = 0; i < root_collision_shapes_concave.size(); i++) {
					Ref<ConcavePolygonShape2D> convex_collision_shape;
					convex_collision_shape.instantiate();
					convex_collision_shape->set_segments(root_collision_shapes_concave[i]->get_segments());
					baked_collision_shapes[i] = convex_collision_shape;
				}
			}
			break;
		default: {
			ERR_PRINT("CollisionShapeType failed. Unrecognized collision_shape_type.");
			break;
		}
	}

	return baked_collision_shapes;
}

Array CSGShape2D::bake_light_occluders() const {
	Array baked_light_occluders;

	if (!is_root_shape()) {
		return baked_light_occluders;
	}

	for (uint32_t i = 0; i < mesh_outlines.size(); i++) {
		if (mesh_outlines[i].size() < 3) {
			continue;
		}
		Vector<Vector2> occluder_polygon = mesh_outlines[i];
		occluder_polygon.push_back(occluder_polygon[0]);

		Ref<OccluderPolygon2D> occluder_polygon_2d;
		occluder_polygon_2d.instantiate();
		occluder_polygon_2d->set_closed(false);
		occluder_polygon_2d->set_polygon(occluder_polygon);

		baked_light_occluders.push_back(occluder_polygon_2d);
	}

	return baked_light_occluders;
}

Ref<NavigationPolygon> CSGShape2D::bake_navigation_mesh() const {
	Ref<NavigationPolygon> navigation_mesh;

	if (!is_root_shape()) {
		return navigation_mesh;
	}

	navigation_mesh.instantiate();

	Vector<Vector<int>> nav_mesh_polygons;
	nav_mesh_polygons.resize(mesh_convex_polygons.size());

	for (uint32_t i = 0; i < mesh_convex_polygons.size(); i++) {
		nav_mesh_polygons.write[i] = mesh_convex_polygons[i];
	}

	navigation_mesh->set_data(mesh_vertices, nav_mesh_polygons);

	return navigation_mesh;
}

bool CSGShape2D::_is_debug_collision_shape_visible() {
	return !Engine::get_singleton()->is_editor_hint() && is_inside_tree() && get_tree()->is_debugging_collisions_hint();
}

void CSGShape2D::_update_debug_collision_shape() {
	if (!use_collision || !is_root_shape() || !root_collision_shapes_convex.is_empty() || !root_collision_shapes_concave.is_empty() || !_is_debug_collision_shape_visible()) {
		return;
	}

	ERR_FAIL_NULL(RenderingServer::get_singleton());

	if (root_collision_debug_instance.is_null()) {
		root_collision_debug_instance = RS::get_singleton()->instance_create();
	}
}

void CSGShape2D::_clear_debug_collision_shape() {
	if (root_collision_debug_instance.is_valid()) {
		RS::get_singleton()->free(root_collision_debug_instance);
		root_collision_debug_instance = RID();
	}
}

void CSGShape2D::_on_transform_changed() {
	if (root_collision_debug_instance.is_valid() && !debug_shape_old_transform.is_equal_approx(get_global_transform())) {
		debug_shape_old_transform = get_global_transform();
	}
}

Rect2 CSGShape2D::get_rect() const {
	return node_rect;
}

void CSGShape2D::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_PARENTED: {
			Node *parentn = get_parent();
			if (parentn) {
				parent_shape = Object::cast_to<CSGShape2D>(parentn);
				if (parent_shape) {
					RenderingServer::get_singleton()->canvas_item_clear(get_canvas_item());
					root_mesh.unref();
					root_edge_line_mesh.unref();
					root_outline_mesh.unref();
				}
			}
			if (!brush || parent_shape) {
				// Update this node if uninitialized, or both this node and its new parent if it gets added to another CSG shape
				_make_dirty();
			}
			last_visible = is_visible();
		} break;

		case NOTIFICATION_UNPARENTED: {
			if (!is_root_shape()) {
				// Update this node and its previous parent only if it's currently being removed from another CSG shape
				_make_dirty(true); // Must be forced since is_root_shape() uses the previous parent
			}
			parent_shape = nullptr;
		} break;

		case NOTIFICATION_CHILD_ORDER_CHANGED: {
			_make_dirty();
		} break;

		case NOTIFICATION_VISIBILITY_CHANGED: {
			if (!is_root_shape() && last_visible != is_visible()) {
				// Update this node's parent only if its own visibility has changed, not the visibility of parent nodes
				parent_shape->_make_dirty();
			}
			last_visible = is_visible();
		} break;

		case NOTIFICATION_LOCAL_TRANSFORM_CHANGED: {
			if (!is_root_shape()) {
				// Update this node's parent only if its own transformation has changed, not the transformation of parent nodes
				parent_shape->_make_dirty();
			}
		} break;

		case NOTIFICATION_ENTER_TREE: {
			if (use_collision && is_root_shape()) {
				_create_collision();
				debug_shape_old_transform = get_global_transform();
				_make_dirty();
			}
		} break;

		case NOTIFICATION_EXIT_TREE: {
			if (use_collision && is_root_shape() && root_collision_instance.is_valid()) {
				_free_collision();
				_clear_debug_collision_shape();
			}
		} break;

		case NOTIFICATION_TRANSFORM_CHANGED: {
			if (use_collision && is_root_shape() && root_collision_instance.is_valid()) {
				PhysicsServer2D::get_singleton()->body_set_state(root_collision_instance, PhysicsServer2D::BODY_STATE_TRANSFORM, get_global_transform());
			}
			_on_transform_changed();
		} break;

		case NOTIFICATION_DRAW: {
			draw_shape();
		} break;
	}
}

void CSGShape2D::draw_shape() {
	RenderingServer::get_singleton()->canvas_item_clear(get_canvas_item());

	CSGBrush2D *n = _get_brush();
	if (n && Engine::get_singleton()->is_editor_hint() && debug_show_brush && brush_mesh.is_valid()) {
		Color brush_mesh_outline_color = Color(1.0, 1.0, 1.0, 1.0);
		switch (get_operation()) {
			case CSGShape2D::Operation::OPERATION_UNION:
				brush_mesh_outline_color = Color(0.0, 1.0, 0.0, 1.0);
				break;
			case CSGShape2D::Operation::OPERATION_INTERSECTION:
				brush_mesh_outline_color = Color(1.0, 0.5, 0.0, 1.0);
				break;
			case CSGShape2D::Operation::OPERATION_SUBTRACTION:
				brush_mesh_outline_color = Color(1.0, 0.0, 0.0, 1.0);
				break;
			default: {
				brush_mesh_outline_color = Color(1.0, 1.0, 1.0, 1.0);
				break;
			}
		}
		RenderingServer::get_singleton()->canvas_item_add_mesh(get_canvas_item(), brush_mesh->get_rid(), Transform2D(), brush_mesh_outline_color);
	}

	if (!is_root_shape()) {
		return;
	}

	ERR_FAIL_NULL_MSG(n, "Cannot get CSGBrush2D.");

	Ref<Texture2D> p_texture;
	RID texture_rid = p_texture.is_valid() ? p_texture->get_rid() : RID();

	Color mesh_face_color = Color(1.0, 1.0, 1.0, 1.0);
	if (!use_vertex_color && Engine::get_singleton()->is_editor_hint()) {
		mesh_face_color = Color(0.373, 0.698, 1.0, 0.5);
	}
	RenderingServer::get_singleton()->canvas_item_add_mesh(get_canvas_item(), root_mesh->get_rid(), Transform2D(), mesh_face_color, texture_rid);
	if (Engine::get_singleton()->is_editor_hint()) {
		Color mesh_edge_color = Color(0.0, 0.45, 0.65, 1.0);
		RenderingServer::get_singleton()->canvas_item_add_mesh(get_canvas_item(), root_edge_line_mesh->get_rid(), Transform2D(), mesh_edge_color);
	}
}

void CSGShape2D::set_operation(Operation p_operation) {
	operation = p_operation;
	_make_dirty();
	queue_redraw();
}

CSGShape2D::Operation CSGShape2D::get_operation() const {
	return operation;
}

void CSGShape2D::set_debug_show_brush(bool p_enable) {
	if (debug_show_brush == p_enable) {
		return;
	}
	debug_show_brush = p_enable;
	queue_redraw();
}

bool CSGShape2D::get_debug_show_brush() const {
	return debug_show_brush;
}

void CSGShape2D::_validate_property(PropertyInfo &p_property) const {
	bool is_collision_prefixed = p_property.name.begins_with("collision_");

	if (is_root_shape()) {
		if (is_collision_prefixed && !bool(get("use_collision"))) {
			p_property.usage = PROPERTY_USAGE_NO_EDITOR;
			return;
		}
		if (p_property.name == "vertex_color" && !bool(get("use_vertex_color"))) {
			p_property.usage = PROPERTY_USAGE_NO_EDITOR;
			return;
		}
	}

	if (!is_root_shape()) {
		if (is_collision_prefixed || p_property.name.begins_with("use_collision")) {
			p_property.usage = PROPERTY_USAGE_NO_EDITOR;
			return;
		}

		if (p_property.name == "use_vertex_color") {
			p_property.usage = PROPERTY_USAGE_NO_EDITOR;
			return;
		}
		if (p_property.name == "vertex_color") {
			p_property.usage = PROPERTY_USAGE_NO_EDITOR;
			return;
		}
	}
}

void CSGShape2D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_debug_show_brush", "p_enable"), &CSGShape2D::set_debug_show_brush);
	ClassDB::bind_method(D_METHOD("get_debug_show_brush"), &CSGShape2D::get_debug_show_brush);

	ClassDB::bind_method(D_METHOD("force_shape_update"), &CSGShape2D::force_shape_update);
	ClassDB::bind_method(D_METHOD("is_root_shape"), &CSGShape2D::is_root_shape);

	ClassDB::bind_method(D_METHOD("set_operation", "operation"), &CSGShape2D::set_operation);
	ClassDB::bind_method(D_METHOD("get_operation"), &CSGShape2D::get_operation);

	ClassDB::bind_method(D_METHOD("set_use_vertex_color", "enable"), &CSGShape2D::set_use_vertex_color);
	ClassDB::bind_method(D_METHOD("is_using_vertex_color"), &CSGShape2D::is_using_vertex_color);

	ClassDB::bind_method(D_METHOD("set_vertex_color", "color"), &CSGShape2D::set_vertex_color);
	ClassDB::bind_method(D_METHOD("get_vertex_color"), &CSGShape2D::get_vertex_color);

	ClassDB::bind_method(D_METHOD("set_use_collision", "enable"), &CSGShape2D::set_use_collision);
	ClassDB::bind_method(D_METHOD("is_using_collision"), &CSGShape2D::is_using_collision);

	ClassDB::bind_method(D_METHOD("set_collision_shape_type", "type"), &CSGShape2D::set_collision_shape_type);
	ClassDB::bind_method(D_METHOD("get_collision_shape_type"), &CSGShape2D::get_collision_shape_type);

	ClassDB::bind_method(D_METHOD("set_collision_layer", "layer"), &CSGShape2D::set_collision_layer);
	ClassDB::bind_method(D_METHOD("get_collision_layer"), &CSGShape2D::get_collision_layer);

	ClassDB::bind_method(D_METHOD("set_collision_mask", "mask"), &CSGShape2D::set_collision_mask);
	ClassDB::bind_method(D_METHOD("get_collision_mask"), &CSGShape2D::get_collision_mask);

	ClassDB::bind_method(D_METHOD("set_collision_mask_value", "layer_number", "value"), &CSGShape2D::set_collision_mask_value);
	ClassDB::bind_method(D_METHOD("get_collision_mask_value", "layer_number"), &CSGShape2D::get_collision_mask_value);

	ClassDB::bind_method(D_METHOD("set_collision_layer_value", "layer_number", "value"), &CSGShape2D::set_collision_layer_value);
	ClassDB::bind_method(D_METHOD("get_collision_layer_value", "layer_number"), &CSGShape2D::get_collision_layer_value);

	ClassDB::bind_method(D_METHOD("set_collision_priority", "priority"), &CSGShape2D::set_collision_priority);
	ClassDB::bind_method(D_METHOD("get_collision_priority"), &CSGShape2D::get_collision_priority);

	ClassDB::bind_method(D_METHOD("bake_static_mesh"), &CSGShape2D::bake_static_mesh);
	ClassDB::bind_method(D_METHOD("bake_collision_shapes"), &CSGShape2D::bake_collision_shapes);
	ClassDB::bind_method(D_METHOD("bake_light_occluders"), &CSGShape2D::bake_light_occluders);
	ClassDB::bind_method(D_METHOD("bake_navigation_mesh"), &CSGShape2D::bake_navigation_mesh);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "operation", PROPERTY_HINT_ENUM, "Union,Intersection,Subtraction"), "set_operation", "get_operation");

	ADD_GROUP("", "debug_");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "debug_show_brush"), "set_debug_show_brush", "get_debug_show_brush");

	ADD_GROUP("Mesh", "mesh_");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "use_vertex_color"), "set_use_vertex_color", "is_using_vertex_color");
	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "vertex_color"), "set_vertex_color", "get_vertex_color");
	ADD_GROUP("Collision", "collision_");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "use_collision"), "set_use_collision", "is_using_collision");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "collision_shape_type", PROPERTY_HINT_ENUM, "Concave Segments Shape, Convex Polygons Shape"), "set_collision_shape_type", "get_collision_shape_type");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "collision_layer", PROPERTY_HINT_LAYERS_3D_PHYSICS), "set_collision_layer", "get_collision_layer");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "collision_mask", PROPERTY_HINT_LAYERS_3D_PHYSICS), "set_collision_mask", "get_collision_mask");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "collision_priority"), "set_collision_priority", "get_collision_priority");

	BIND_ENUM_CONSTANT(OPERATION_UNION);
	BIND_ENUM_CONSTANT(OPERATION_INTERSECTION);
	BIND_ENUM_CONSTANT(OPERATION_SUBTRACTION);

	BIND_ENUM_CONSTANT(COLLISION_SHAPE_TYPE_CONCAVE_SEGMENTS);
	BIND_ENUM_CONSTANT(COLLISION_SHAPE_TYPE_CONVEX_POLYGONS);
	BIND_ENUM_CONSTANT(COLLISION_SHAPE_TYPE_MAX);
}

CSGShape2D::CSGShape2D() {
	set_notify_local_transform(true);
}

CSGShape2D::~CSGShape2D() {
	if (brush) {
		memdelete(brush);
		brush = nullptr;
	}
}

//////////////////////////////////

#ifdef DEBUG_ENABLED
Rect2 CSGCombiner2D::_edit_get_rect() const {
	if (rect_cache_dirty || true) {
		item_rect = Rect2();
		bool first = true;

		for (const Vector<Vector2> &outline : mesh_outlines) {
			const int outline_size = outline.size();
			if (outline_size < 3) {
				continue;
			}
			for (int i = 0; i < outline_size; i++) {
				if (first) {
					item_rect = Rect2(outline[i], Vector2(0, 0));
					first = false;
				} else {
					item_rect.expand_to(outline[i]);
				}
			}
		}

		rect_cache_dirty = false;
	}
	return item_rect;
}

bool CSGCombiner2D::_edit_use_rect() const {
	return true;
}

bool CSGCombiner2D::_edit_is_selected_on_click(const Point2 &p_point, double p_tolerance) const {
	return is_point_in_outlines(p_point);
}
#endif // DEBUG_ENABLED

CSGBrush2D *CSGCombiner2D::_build_brush() {
	rect_cache_dirty = true;
	return memnew(CSGBrush2D); //does not build anything
}

/////////////////////

void CSGPrimitive2D::_bind_methods() {
}

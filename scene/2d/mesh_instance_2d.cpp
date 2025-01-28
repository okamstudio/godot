/**************************************************************************/
/*  mesh_instance_2d.cpp                                                  */
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

#include "mesh_instance_2d.h"

#include "scene/resources/2d/navigation_mesh_source_geometry_data_2d.h"
#include "scene/resources/2d/navigation_polygon.h"
#include "scene/scene_string_names.h"
#include "servers/navigation_server_2d.h"

#include "thirdparty/clipper2/include/clipper2/clipper.h"
#include "thirdparty/misc/polypartition.h"

Callable MeshInstance2D::_navmesh_source_geometry_parsing_callback;
RID MeshInstance2D::_navmesh_source_geometry_parser;

void MeshInstance2D::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_DRAW: {
			if (mesh.is_valid()) {
				draw_mesh(mesh, texture);
			}
		} break;
	}
}

void MeshInstance2D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_mesh", "mesh"), &MeshInstance2D::set_mesh);
	ClassDB::bind_method(D_METHOD("get_mesh"), &MeshInstance2D::get_mesh);

	ClassDB::bind_method(D_METHOD("set_texture", "texture"), &MeshInstance2D::set_texture);
	ClassDB::bind_method(D_METHOD("get_texture"), &MeshInstance2D::get_texture);

	ADD_SIGNAL(MethodInfo("texture_changed"));

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "mesh", PROPERTY_HINT_RESOURCE_TYPE, "Mesh"), "set_mesh", "get_mesh");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "texture", PROPERTY_HINT_RESOURCE_TYPE, "Texture2D"), "set_texture", "get_texture");
}

void MeshInstance2D::set_mesh(const Ref<Mesh> &p_mesh) {
	if (mesh == p_mesh) {
		return;
	}

	if (mesh.is_valid()) {
		mesh->disconnect_changed(callable_mp((CanvasItem *)this, &CanvasItem::queue_redraw));
	}

	mesh = p_mesh;

	if (mesh.is_valid()) {
		// If mesh is a PrimitiveMesh, calling get_rid on it can trigger a changed callback
		// so do this before connecting to the change signal.
		mesh->get_rid();
		mesh->connect_changed(callable_mp((CanvasItem *)this, &CanvasItem::queue_redraw));
	}

	queue_redraw();
}

Ref<Mesh> MeshInstance2D::get_mesh() const {
	return mesh;
}

void MeshInstance2D::set_texture(const Ref<Texture2D> &p_texture) {
	if (p_texture == texture) {
		return;
	}
	texture = p_texture;
	queue_redraw();
	emit_signal(SceneStringName(texture_changed));
}

Ref<Texture2D> MeshInstance2D::get_texture() const {
	return texture;
}

#ifdef DEBUG_ENABLED
Rect2 MeshInstance2D::_edit_get_rect() const {
	if (mesh.is_valid()) {
		AABB aabb = mesh->get_aabb();
		return Rect2(aabb.position.x, aabb.position.y, aabb.size.x, aabb.size.y);
	}

	return Node2D::_edit_get_rect();
}

bool MeshInstance2D::_edit_use_rect() const {
	return mesh.is_valid();
}
#endif // DEBUG_ENABLED

void MeshInstance2D::navmesh_parse_init() {
	ERR_FAIL_NULL(NavigationServer2D::get_singleton());
	if (!_navmesh_source_geometry_parser.is_valid()) {
		_navmesh_source_geometry_parsing_callback = callable_mp_static(&MeshInstance2D::navmesh_parse_source_geometry);
		_navmesh_source_geometry_parser = NavigationServer2D::get_singleton()->source_geometry_parser_create();
		NavigationServer2D::get_singleton()->source_geometry_parser_set_callback(_navmesh_source_geometry_parser, _navmesh_source_geometry_parsing_callback);
	}
}

void MeshInstance2D::navmesh_parse_source_geometry(const Ref<NavigationPolygon> &p_navigation_mesh, Ref<NavigationMeshSourceGeometryData2D> p_source_geometry_data, Node *p_node) {
	MeshInstance2D *mesh_instance = Object::cast_to<MeshInstance2D>(p_node);

	if (mesh_instance == nullptr) {
		return;
	}

	NavigationPolygon::ParsedGeometryType parsed_geometry_type = p_navigation_mesh->get_parsed_geometry_type();

	if (!(parsed_geometry_type == NavigationPolygon::PARSED_GEOMETRY_MESH_INSTANCES || parsed_geometry_type == NavigationPolygon::PARSED_GEOMETRY_BOTH)) {
		return;
	}

	Ref<Mesh> mesh = mesh_instance->get_mesh();
	if (mesh.is_null()) {
		return;
	}

	const Transform2D mesh_instance_xform = p_source_geometry_data->root_node_transform * mesh_instance->get_global_transform();

	using namespace Clipper2Lib;

	PathsD subject_paths, dummy_clip_paths;

	for (int i = 0; i < mesh->get_surface_count(); i++) {
		if (mesh->surface_get_primitive_type(i) != Mesh::PRIMITIVE_TRIANGLES) {
			continue;
		}

		if (!(mesh->surface_get_format(i) & Mesh::ARRAY_FLAG_USE_2D_VERTICES)) {
			continue;
		}

		PathD subject_path;

		int index_count = 0;
		if (mesh->surface_get_format(i) & Mesh::ARRAY_FORMAT_INDEX) {
			index_count = mesh->surface_get_array_index_len(i);
		} else {
			index_count = mesh->surface_get_array_len(i);
		}

		ERR_CONTINUE((index_count == 0 || (index_count % 3) != 0));

		Array a = mesh->surface_get_arrays(i);

		Vector<Vector2> mesh_vertices = a[Mesh::ARRAY_VERTEX];

		if (mesh->surface_get_format(i) & Mesh::ARRAY_FORMAT_INDEX) {
			Vector<int> mesh_indices = a[Mesh::ARRAY_INDEX];
			for (int vertex_index : mesh_indices) {
				const Vector2 &vertex = mesh_vertices[vertex_index];
				const PointD &point = PointD(vertex.x, vertex.y);
				subject_path.push_back(point);
			}
		} else {
			for (const Vector2 &vertex : mesh_vertices) {
				const PointD &point = PointD(vertex.x, vertex.y);
				subject_path.push_back(point);
			}
		}
		subject_paths.push_back(subject_path);
	}

	PathsD path_solution;

	path_solution = Union(subject_paths, dummy_clip_paths, FillRule::NonZero);

	//path_solution = RamerDouglasPeucker(path_solution, 0.025);

	Vector<Vector<Vector2>> polypaths;

	for (const PathD &scaled_path : path_solution) {
		Vector<Vector2> shape_outline;
		for (const PointD &scaled_point : scaled_path) {
			shape_outline.push_back(Point2(static_cast<real_t>(scaled_point.x), static_cast<real_t>(scaled_point.y)));
		}

		for (int i = 0; i < shape_outline.size(); i++) {
			shape_outline.write[i] = mesh_instance_xform.xform(shape_outline[i]);
		}

		p_source_geometry_data->add_obstruction_outline(shape_outline);
	}
}

MeshInstance2D::MeshInstance2D() {
}

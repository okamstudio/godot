/**************************************************************************/
/*  multimesh_instance_3d.cpp                                             */
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

#include "multimesh_instance_3d.h"

#include "scene/resources/3d/navigation_mesh_source_geometry_data_3d.h"
#include "scene/resources/navigation_mesh.h"
#include "servers/navigation_server_3d.h"

Callable MultiMeshInstance3D::_navmesh_source_geometry_parsing_callback;
RID MultiMeshInstance3D::_navmesh_source_geometry_parser;

void MultiMeshInstance3D::_refresh_interpolated() {
	if (is_inside_tree() && multimesh.is_valid()) {
		bool interpolated = is_physics_interpolated_and_enabled();
		multimesh->set_physics_interpolated(interpolated);
	}
}

void MultiMeshInstance3D::_physics_interpolated_changed() {
	VisualInstance3D::_physics_interpolated_changed();
	_refresh_interpolated();
}

void MultiMeshInstance3D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_multimesh", "multimesh"), &MultiMeshInstance3D::set_multimesh);
	ClassDB::bind_method(D_METHOD("get_multimesh"), &MultiMeshInstance3D::get_multimesh);
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "multimesh", PROPERTY_HINT_RESOURCE_TYPE, "MultiMesh"), "set_multimesh", "get_multimesh");
}

void MultiMeshInstance3D::_notification(int p_what) {
	if (p_what == NOTIFICATION_ENTER_TREE) {
		_refresh_interpolated();
	}
}

void MultiMeshInstance3D::set_multimesh(const Ref<MultiMesh> &p_multimesh) {
	multimesh = p_multimesh;
	if (multimesh.is_valid()) {
		set_base(multimesh->get_rid());
		_refresh_interpolated();
	} else {
		set_base(RID());
	}
}

Ref<MultiMesh> MultiMeshInstance3D::get_multimesh() const {
	return multimesh;
}

Array MultiMeshInstance3D::get_meshes() const {
	if (multimesh.is_null() || multimesh->get_mesh().is_null() || multimesh->get_transform_format() != MultiMesh::TransformFormat::TRANSFORM_3D) {
		return Array();
	}

	int count = multimesh->get_visible_instance_count();
	if (count == -1) {
		count = multimesh->get_instance_count();
	}

	Ref<Mesh> mesh = multimesh->get_mesh();

	Array results;
	for (int i = 0; i < count; i++) {
		results.push_back(multimesh->get_instance_transform(i));
		results.push_back(mesh);
	}
	return results;
}

AABB MultiMeshInstance3D::get_aabb() const {
	if (multimesh.is_null()) {
		return AABB();
	} else {
		return multimesh->get_aabb();
	}
}

void MultiMeshInstance3D::navmesh_parse_init() {
	ERR_FAIL_NULL(NavigationServer3D::get_singleton());
	if (!_navmesh_source_geometry_parser.is_valid()) {
		_navmesh_source_geometry_parsing_callback = callable_mp_static(&MultiMeshInstance3D::navmesh_parse_source_geometry);
		_navmesh_source_geometry_parser = NavigationServer3D::get_singleton()->source_geometry_parser_create();
		NavigationServer3D::get_singleton()->source_geometry_parser_set_callback(_navmesh_source_geometry_parser, _navmesh_source_geometry_parsing_callback);
	}
}

void MultiMeshInstance3D::navmesh_parse_source_geometry(const Ref<NavigationMesh> &p_navigation_mesh, Ref<NavigationMeshSourceGeometryData3D> p_source_geometry_data, Node *p_node) {
	MultiMeshInstance3D *multimesh_instance = Object::cast_to<MultiMeshInstance3D>(p_node);

	if (multimesh_instance == nullptr) {
		return;
	}

	NavigationMesh::ParsedGeometryType parsed_geometry_type = p_navigation_mesh->get_parsed_geometry_type();

	if (parsed_geometry_type == NavigationMesh::PARSED_GEOMETRY_MESH_INSTANCES || parsed_geometry_type == NavigationMesh::PARSED_GEOMETRY_BOTH) {
		Ref<MultiMesh> multimesh = multimesh_instance->get_multimesh();
		if (multimesh.is_valid()) {
			Ref<Mesh> mesh = multimesh->get_mesh();
			if (mesh.is_valid()) {
				int n = multimesh->get_visible_instance_count();
				if (n == -1) {
					n = multimesh->get_instance_count();
				}
				for (int i = 0; i < n; i++) {
					p_source_geometry_data->add_mesh(mesh, multimesh_instance->get_global_transform() * multimesh->get_instance_transform(i));
				}
			}
		}
	}
}

MultiMeshInstance3D::MultiMeshInstance3D() {
}

MultiMeshInstance3D::~MultiMeshInstance3D() {
}

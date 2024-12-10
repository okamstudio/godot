/**************************************************************************/
/*  csg_mesh_2d.cpp                                                       */
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

#include "csg_mesh_2d.h"

#include "core/math/geometry_2d.h"
#include "scene/resources/world_2d.h"
#include "servers/physics_server_2d.h"

#include "thirdparty/misc/polypartition.h"

#ifdef DEBUG_ENABLED
Rect2 CSGMesh2D::_edit_get_rect() const {
	Rect2 item_rect = Rect2(-10, -10, 20, 20);
	if (mesh.is_valid()) {
		AABB aabb = mesh->get_aabb();
		item_rect = Rect2(aabb.position.x, aabb.position.y, aabb.size.x, aabb.size.y);
	}
	item_rect.position -= item_rect.size * 0.3;
	item_rect.size += item_rect.size * 0.6;
	return item_rect;
}

bool CSGMesh2D::_edit_use_rect() const {
	return true;
}

bool CSGMesh2D::_edit_is_selected_on_click(const Point2 &p_point, double p_tolerance) const {
	return is_point_in_outlines(p_point);
}
#endif // DEBUG_ENABLED

CSGBrush2D *CSGMesh2D::_build_brush() {
	if (!mesh.is_valid()) {
		return memnew(CSGBrush2D);
	}

	CSGBrush2D *new_brush = memnew(CSGBrush2D);

	Clipper2Lib::PolyTreeD polytree;
	Clipper2Lib::ClipperD clipper_D;
	clipper_D.PreserveCollinear(false);

	Clipper2Lib::PathsD poly_paths;

	for (int i = 0; i < mesh->get_surface_count(); i++) {
		if (mesh->surface_get_primitive_type(i) != Mesh::PRIMITIVE_TRIANGLES) {
			continue;
		}
		if (!(mesh->surface_get_format(i) & Mesh::ARRAY_FLAG_USE_2D_VERTICES)) {
			continue;
		}

		Array arrays = mesh->surface_get_arrays(i);

		if (arrays.size() == 0) {
			_make_dirty();
			ERR_FAIL_COND_V(arrays.is_empty(), memnew(CSGBrush2D));
		}

		Vector<Vector2> avertices = arrays[Mesh::ARRAY_VERTEX];
		if (avertices.size() == 0) {
			continue;
		}

		const Vector2 *vr = avertices.ptr();

		Vector<Vector2> vertices;

		Vector<int> aindices = arrays[Mesh::ARRAY_INDEX];
		if (aindices.size() > 0) {
			for (int j = 0; j < aindices.size(); j++) {
				vertices.push_back(vr[aindices[j]]);
			}
			vertices.push_back(vr[aindices[0]]);
		} else {
			for (int j = 0; j < avertices.size(); j++) {
				vertices.push_back(vr[j]);
			}
			vertices.push_back(avertices[0]);
		}

		for (int j = 0; j < vertices.size() / 3; j++) {
			Clipper2Lib::PathD poly_path;

			Vector2 vertex1 = vertices[j * 3 + 0];
			Vector2 vertex2 = vertices[j * 3 + 2];
			Vector2 vertex3 = vertices[j * 3 + 1];

			poly_path.push_back(Clipper2Lib::PointD(vertex1.x, vertex1.y));
			poly_path.push_back(Clipper2Lib::PointD(vertex2.x, vertex2.y));
			poly_path.push_back(Clipper2Lib::PointD(vertex3.x, vertex3.y));

			poly_paths.push_back(poly_path);
		}
	}

	clipper_D.AddSubject(poly_paths);
	clipper_D.Execute(Clipper2Lib::ClipType::Union, Clipper2Lib::FillRule::NonZero, polytree);

	List<TPPLPoly> tppl_in_polygon, tppl_out_polygon;

	LocalVector<Vector<Vector2>> visual_mesh_outlines;

	for (size_t i = 0; i < polytree.Count(); i++) {
		const Clipper2Lib::PolyPathD *polypath_item = polytree[i];
		CSGShape2D::_recursive_process_polytree_items(tppl_in_polygon, polypath_item, visual_mesh_outlines);
	}

	LocalVector<CSGBrush2D::Outline> &outlines = new_brush->outlines;
	outlines.resize(visual_mesh_outlines.size());

	brush_outlines.resize(outlines.size());

	if (visual_mesh_outlines.size() == 0) {
		memdelete(new_brush);
		return memnew(CSGBrush2D);
	}

	for (uint32_t i = 0; i < visual_mesh_outlines.size(); i++) {
		CSGBrush2D::Outline &outline = outlines[i];
		const Vector<Vector2> &visual_mesh_outline = visual_mesh_outlines[i];

		LocalVector<Vector2> &vertices = outline.vertices;
		vertices.resize(visual_mesh_outline.size());

		for (int j = 0; j < visual_mesh_outline.size(); j++) {
			vertices[j] = visual_mesh_outline[j];
		}
		brush_outlines[i] = vertices;
	}

	new_brush->build_from_outlines(outlines);

	return new_brush;
}

void CSGMesh2D::_mesh_changed() {
	_make_dirty();
}

void CSGMesh2D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_mesh", "mesh"), &CSGMesh2D::set_mesh);
	ClassDB::bind_method(D_METHOD("get_mesh"), &CSGMesh2D::get_mesh);

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "mesh", PROPERTY_HINT_RESOURCE_TYPE, "Mesh"), "set_mesh", "get_mesh");
}

void CSGMesh2D::set_mesh(const Ref<Mesh> &p_mesh) {
	if (mesh == p_mesh) {
		return;
	}
	if (mesh.is_valid()) {
		mesh->disconnect_changed(callable_mp(this, &CSGMesh2D::_mesh_changed));
	}

	mesh = p_mesh;

	if (mesh.is_valid()) {
		mesh->connect_changed(callable_mp(this, &CSGMesh2D::_mesh_changed));
	}

	_mesh_changed();
}

Ref<Mesh> CSGMesh2D::get_mesh() {
	return mesh;
}

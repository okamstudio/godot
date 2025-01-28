/**************************************************************************/
/*  collision_object_3d_gizmo_plugin.cpp                                  */
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

#include "collision_object_3d_gizmo_plugin.h"

#include "scene/3d/physics/collision_object_3d.h"
#include "scene/3d/physics/collision_polygon_3d.h"
#include "scene/3d/physics/collision_shape_3d.h"
#include "scene/resources/surface_tool.h"

CollisionObject3DGizmoPlugin::CollisionObject3DGizmoPlugin() {
	const Color gizmo_color = SceneTree::get_singleton()->get_debug_collisions_color();
	create_material("shape_material", gizmo_color);
	const float gizmo_value = gizmo_color.get_v();
	const Color gizmo_color_disabled = Color(gizmo_value, gizmo_value, gizmo_value, 0.65);
	create_material("shape_material_disabled", gizmo_color_disabled);
}

bool CollisionObject3DGizmoPlugin::has_gizmo(Node3D *p_spatial) {
	return Object::cast_to<CollisionObject3D>(p_spatial) != nullptr;
}

String CollisionObject3DGizmoPlugin::get_gizmo_name() const {
	return "CollisionObject3D";
}

int CollisionObject3DGizmoPlugin::get_priority() const {
	return -2;
}

void CollisionObject3DGizmoPlugin::redraw(EditorNode3DGizmo *p_gizmo) {
	CollisionObject3D *co = Object::cast_to<CollisionObject3D>(p_gizmo->get_node_3d());

	p_gizmo->clear();

	List<uint32_t> owner_ids;
	co->get_shape_owners(&owner_ids);
	for (uint32_t &owner_id : owner_ids) {
		Transform3D xform = co->shape_owner_get_transform(owner_id);
		Object *owner = co->shape_owner_get_owner(owner_id);
		// Exclude CollisionShape3D and CollisionPolygon3D as they have their gizmo.
		if (!Object::cast_to<CollisionShape3D>(owner) && !Object::cast_to<CollisionPolygon3D>(owner)) {
			Ref<Material> material = get_material(!co->is_shape_owner_disabled(owner_id) ? "shape_material" : "shape_material_disabled", p_gizmo);
			for (int shape_id = 0; shape_id < co->shape_owner_get_shape_count(owner_id); shape_id++) {
				Ref<Shape3D> s = co->shape_owner_get_shape(owner_id, shape_id);
				if (s.is_null()) {
					continue;
				}
				SurfaceTool st;
				st.append_from(s->get_debug_mesh(), 0, xform);

				p_gizmo->add_mesh(st.commit(), material);
				p_gizmo->add_collision_segments(s->get_debug_mesh_lines());
			}
		}
	}
}

/**************************************************************************/
/*  grid_map.cpp                                                          */
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

#include "grid_map.h"

#include "core/io/marshalls.h"
#include "core/math/convex_hull.h"
#include "scene/resources/3d/box_shape_3d.h"
#include "scene/resources/3d/capsule_shape_3d.h"
#include "scene/resources/3d/concave_polygon_shape_3d.h"
#include "scene/resources/3d/convex_polygon_shape_3d.h"
#include "scene/resources/3d/cylinder_shape_3d.h"
#include "scene/resources/3d/height_map_shape_3d.h"
#include "scene/resources/3d/mesh_library.h"
#include "scene/resources/3d/navigation_mesh_source_geometry_data_3d.h"
#include "scene/resources/3d/primitive_meshes.h"
#include "scene/resources/3d/shape_3d.h"
#include "scene/resources/3d/sphere_shape_3d.h"
#include "scene/resources/physics_material.h"
#include "scene/resources/surface_tool.h"
#include "servers/navigation_server_3d.h"
#include "servers/rendering_server.h"

Callable GridMap::_navmesh_source_geometry_parsing_callback;
RID GridMap::_navmesh_source_geometry_parser;

bool GridMap::_set(const StringName &p_name, const Variant &p_value) {
	String name = p_name;

	if (name == "data") {
		Dictionary d = p_value;

		if (d.has("cells")) {
			Vector<int> cells = d["cells"];
			int amount = cells.size();
			const int *r = cells.ptr();
			ERR_FAIL_COND_V(amount % 3, false); // not even
			cell_map.clear();
			for (int i = 0; i < amount / 3; i++) {
				IndexKey ik;
				ik.key = decode_uint64((const uint8_t *)&r[i * 3]);
				Cell cell;
				cell.cell = decode_uint32((const uint8_t *)&r[i * 3 + 2]);
				cell_map[ik] = cell;
			}
		}

		_recreate_octant_data();

	} else if (name == "baked_meshes") {
		clear_baked_meshes();

		Array meshes = p_value;

		for (int i = 0; i < meshes.size(); i++) {
			BakedMesh bm;
			bm.mesh = meshes[i];
			ERR_CONTINUE(bm.mesh.is_null());
			bm.instance = RS::get_singleton()->instance_create();
			RS::get_singleton()->instance_set_base(bm.instance, bm.mesh->get_rid());
			RS::get_singleton()->instance_attach_object_instance_id(bm.instance, get_instance_id());
			if (is_inside_tree()) {
				RS::get_singleton()->instance_set_scenario(bm.instance, get_world_3d()->get_scenario());
				RS::get_singleton()->instance_set_transform(bm.instance, get_global_transform());
			}
			baked_meshes.push_back(bm);
		}

		_recreate_octant_data();

	} else {
		return false;
	}

	return true;
}

bool GridMap::_get(const StringName &p_name, Variant &r_ret) const {
	String name = p_name;

	if (name == "data") {
		Dictionary d;

		Vector<int> cells;
		cells.resize(cell_map.size() * 3);
		{
			int *w = cells.ptrw();
			int i = 0;
			for (const KeyValue<IndexKey, Cell> &E : cell_map) {
				encode_uint64(E.key.key, (uint8_t *)&w[i * 3]);
				encode_uint32(E.value.cell, (uint8_t *)&w[i * 3 + 2]);
				i++;
			}
		}

		d["cells"] = cells;

		r_ret = d;
	} else if (name == "baked_meshes") {
		Array ret;
		ret.resize(baked_meshes.size());
		for (int i = 0; i < baked_meshes.size(); i++) {
			ret[i] = baked_meshes[i].mesh;
		}
		r_ret = ret;

	} else {
		return false;
	}

	return true;
}

void GridMap::_get_property_list(List<PropertyInfo> *p_list) const {
	if (baked_meshes.size()) {
		p_list->push_back(PropertyInfo(Variant::ARRAY, "baked_meshes", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_STORAGE));
	}

	p_list->push_back(PropertyInfo(Variant::DICTIONARY, "data", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_STORAGE));
}

void GridMap::set_collision_layer(uint32_t p_layer) {
	collision_layer = p_layer;
	_update_physics_bodies_collision_properties();
}

uint32_t GridMap::get_collision_layer() const {
	return collision_layer;
}

void GridMap::set_collision_mask(uint32_t p_mask) {
	collision_mask = p_mask;
	_update_physics_bodies_collision_properties();
}

uint32_t GridMap::get_collision_mask() const {
	return collision_mask;
}

void GridMap::set_collision_layer_value(int p_layer_number, bool p_value) {
	ERR_FAIL_COND_MSG(p_layer_number < 1, "Collision layer number must be between 1 and 32 inclusive.");
	ERR_FAIL_COND_MSG(p_layer_number > 32, "Collision layer number must be between 1 and 32 inclusive.");
	uint32_t collision_layer_new = get_collision_layer();
	if (p_value) {
		collision_layer_new |= 1 << (p_layer_number - 1);
	} else {
		collision_layer_new &= ~(1 << (p_layer_number - 1));
	}
	set_collision_layer(collision_layer_new);
}

bool GridMap::get_collision_layer_value(int p_layer_number) const {
	ERR_FAIL_COND_V_MSG(p_layer_number < 1, false, "Collision layer number must be between 1 and 32 inclusive.");
	ERR_FAIL_COND_V_MSG(p_layer_number > 32, false, "Collision layer number must be between 1 and 32 inclusive.");
	return get_collision_layer() & (1 << (p_layer_number - 1));
}

void GridMap::set_collision_mask_value(int p_layer_number, bool p_value) {
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

void GridMap::set_collision_priority(real_t p_priority) {
	collision_priority = p_priority;
	_update_physics_bodies_collision_properties();
}

real_t GridMap::get_collision_priority() const {
	return collision_priority;
}

void GridMap::set_physics_material(Ref<PhysicsMaterial> p_material) {
	physics_material = p_material;
	_update_physics_bodies_characteristics();
}

Ref<PhysicsMaterial> GridMap::get_physics_material() const {
	return physics_material;
}

bool GridMap::get_collision_mask_value(int p_layer_number) const {
	ERR_FAIL_COND_V_MSG(p_layer_number < 1, false, "Collision layer number must be between 1 and 32 inclusive.");
	ERR_FAIL_COND_V_MSG(p_layer_number > 32, false, "Collision layer number must be between 1 and 32 inclusive.");
	return get_collision_mask() & (1 << (p_layer_number - 1));
}

Array GridMap::get_collision_shapes() const {
	Array shapes;
	for (const KeyValue<OctantKey, Octant *> &E : octant_map) {
		Octant *g = E.value;
		RID body = g->static_body;
		Transform3D body_xform = PhysicsServer3D::get_singleton()->body_get_state(body, PhysicsServer3D::BODY_STATE_TRANSFORM);
		int nshapes = PhysicsServer3D::get_singleton()->body_get_shape_count(body);
		for (int i = 0; i < nshapes; i++) {
			RID shape = PhysicsServer3D::get_singleton()->body_get_shape(body, i);
			Transform3D xform = PhysicsServer3D::get_singleton()->body_get_shape_transform(body, i);
			shapes.push_back(body_xform * xform);
			shapes.push_back(shape);
		}
	}

	return shapes;
}

void GridMap::set_bake_navigation(bool p_bake_navigation) {
	bake_navigation = p_bake_navigation;
	_recreate_octant_data();
}

bool GridMap::is_baking_navigation() {
	return bake_navigation;
}

void GridMap::set_navigation_map(RID p_navigation_map) {
	map_override = p_navigation_map;
	for (const KeyValue<OctantKey, Octant *> &E : octant_map) {
		Octant &g = *octant_map[E.key];
		for (KeyValue<IndexKey, Octant::NavigationCell> &F : g.navigation_cell_ids) {
			if (F.value.region.is_valid()) {
				NavigationServer3D::get_singleton()->region_set_map(F.value.region, map_override);
			}
		}
	}
}

RID GridMap::get_navigation_map() const {
	if (map_override.is_valid()) {
		return map_override;
	} else if (is_inside_tree()) {
		return get_world_3d()->get_navigation_map();
	}
	return RID();
}

void GridMap::set_mesh_library(const Ref<MeshLibrary> &p_mesh_library) {
	if (mesh_library.is_valid()) {
		mesh_library->disconnect_changed(callable_mp(this, &GridMap::_recreate_octant_data));
	}
	mesh_library = p_mesh_library;
	if (mesh_library.is_valid()) {
		mesh_library->connect_changed(callable_mp(this, &GridMap::_recreate_octant_data));
	}

	_recreate_octant_data();
	emit_signal(CoreStringName(changed));
}

Ref<MeshLibrary> GridMap::get_mesh_library() const {
	return mesh_library;
}

void GridMap::set_cell_size(const Vector3 &p_size) {
	ERR_FAIL_COND(p_size.x < 0.001 || p_size.y < 0.001 || p_size.z < 0.001);
	cell_size = p_size;
	_recreate_octant_data();
	emit_signal(SNAME("cell_size_changed"), cell_size);
}

Vector3 GridMap::get_cell_size() const {
	return cell_size;
}

void GridMap::set_octant_size(int p_size) {
	ERR_FAIL_COND(p_size == 0);
	octant_size = p_size;
	_recreate_octant_data();
}

int GridMap::get_octant_size() const {
	return octant_size;
}

void GridMap::set_center_x(bool p_enable) {
	center_x = p_enable;
	_recreate_octant_data();
}

bool GridMap::get_center_x() const {
	return center_x;
}

void GridMap::set_center_y(bool p_enable) {
	center_y = p_enable;
	_recreate_octant_data();
}

bool GridMap::get_center_y() const {
	return center_y;
}

void GridMap::set_center_z(bool p_enable) {
	center_z = p_enable;
	_recreate_octant_data();
}

bool GridMap::get_center_z() const {
	return center_z;
}

void GridMap::set_cell_item(const Vector3i &p_position, int p_item, int p_rot) {
	if (baked_meshes.size() && !recreating_octants) {
		//if you set a cell item, baked meshes go good bye
		clear_baked_meshes();
		_recreate_octant_data();
	}

	ERR_FAIL_INDEX(ABS(p_position.x), 1 << 20);
	ERR_FAIL_INDEX(ABS(p_position.y), 1 << 20);
	ERR_FAIL_INDEX(ABS(p_position.z), 1 << 20);

	IndexKey key;
	key.x = p_position.x;
	key.y = p_position.y;
	key.z = p_position.z;

	OctantKey ok;
	ok.x = p_position.x / octant_size;
	ok.y = p_position.y / octant_size;
	ok.z = p_position.z / octant_size;

	if (p_item < 0) {
		//erase
		if (cell_map.has(key)) {
			OctantKey octantkey = ok;

			ERR_FAIL_COND(!octant_map.has(octantkey));
			Octant &g = *octant_map[octantkey];
			g.cells.erase(key);
			g.dirty = true;
			cell_map.erase(key);
			_queue_octants_dirty();
		}
		return;
	}

	OctantKey octantkey = ok;

	if (!octant_map.has(octantkey)) {
		//create octant because it does not exist
		Octant *g = memnew(Octant);
		g->dirty = true;
		g->static_body = PhysicsServer3D::get_singleton()->body_create();
		PhysicsServer3D::get_singleton()->body_set_mode(g->static_body, PhysicsServer3D::BODY_MODE_STATIC);
		PhysicsServer3D::get_singleton()->body_attach_object_instance_id(g->static_body, get_instance_id());
		PhysicsServer3D::get_singleton()->body_set_collision_layer(g->static_body, collision_layer);
		PhysicsServer3D::get_singleton()->body_set_collision_mask(g->static_body, collision_mask);
		PhysicsServer3D::get_singleton()->body_set_collision_priority(g->static_body, collision_priority);
		if (physics_material.is_valid()) {
			PhysicsServer3D::get_singleton()->body_set_param(g->static_body, PhysicsServer3D::BODY_PARAM_FRICTION, physics_material->computed_friction());
			PhysicsServer3D::get_singleton()->body_set_param(g->static_body, PhysicsServer3D::BODY_PARAM_BOUNCE, physics_material->computed_bounce());
		}
		SceneTree *st = SceneTree::get_singleton();

		if (st && st->is_debugging_collisions_hint()) {
			g->collision_debug = RenderingServer::get_singleton()->mesh_create();
			g->collision_debug_instance = RenderingServer::get_singleton()->instance_create();
			RenderingServer::get_singleton()->instance_set_base(g->collision_debug_instance, g->collision_debug);
		}

		octant_map[octantkey] = g;

		if (is_inside_world()) {
			_octant_enter_world(octantkey);
			_octant_transform(octantkey);
		}
	}

	Octant &g = *octant_map[octantkey];
	g.cells.insert(key);
	g.dirty = true;
	_queue_octants_dirty();

	Cell c;
	c.item = p_item;
	c.rot = p_rot;

	cell_map[key] = c;
}

int GridMap::get_cell_item(const Vector3i &p_position) const {
	ERR_FAIL_INDEX_V(ABS(p_position.x), 1 << 20, INVALID_CELL_ITEM);
	ERR_FAIL_INDEX_V(ABS(p_position.y), 1 << 20, INVALID_CELL_ITEM);
	ERR_FAIL_INDEX_V(ABS(p_position.z), 1 << 20, INVALID_CELL_ITEM);

	IndexKey key;
	key.x = p_position.x;
	key.y = p_position.y;
	key.z = p_position.z;

	if (!cell_map.has(key)) {
		return INVALID_CELL_ITEM;
	}
	return cell_map[key].item;
}

int GridMap::get_cell_item_orientation(const Vector3i &p_position) const {
	ERR_FAIL_INDEX_V(ABS(p_position.x), 1 << 20, -1);
	ERR_FAIL_INDEX_V(ABS(p_position.y), 1 << 20, -1);
	ERR_FAIL_INDEX_V(ABS(p_position.z), 1 << 20, -1);

	IndexKey key;
	key.x = p_position.x;
	key.y = p_position.y;
	key.z = p_position.z;

	if (!cell_map.has(key)) {
		return -1;
	}
	return cell_map[key].rot;
}

static const Basis _ortho_bases[24] = {
	Basis(1, 0, 0, 0, 1, 0, 0, 0, 1),
	Basis(0, -1, 0, 1, 0, 0, 0, 0, 1),
	Basis(-1, 0, 0, 0, -1, 0, 0, 0, 1),
	Basis(0, 1, 0, -1, 0, 0, 0, 0, 1),
	Basis(1, 0, 0, 0, 0, -1, 0, 1, 0),
	Basis(0, 0, 1, 1, 0, 0, 0, 1, 0),
	Basis(-1, 0, 0, 0, 0, 1, 0, 1, 0),
	Basis(0, 0, -1, -1, 0, 0, 0, 1, 0),
	Basis(1, 0, 0, 0, -1, 0, 0, 0, -1),
	Basis(0, 1, 0, 1, 0, 0, 0, 0, -1),
	Basis(-1, 0, 0, 0, 1, 0, 0, 0, -1),
	Basis(0, -1, 0, -1, 0, 0, 0, 0, -1),
	Basis(1, 0, 0, 0, 0, 1, 0, -1, 0),
	Basis(0, 0, -1, 1, 0, 0, 0, -1, 0),
	Basis(-1, 0, 0, 0, 0, -1, 0, -1, 0),
	Basis(0, 0, 1, -1, 0, 0, 0, -1, 0),
	Basis(0, 0, 1, 0, 1, 0, -1, 0, 0),
	Basis(0, -1, 0, 0, 0, 1, -1, 0, 0),
	Basis(0, 0, -1, 0, -1, 0, -1, 0, 0),
	Basis(0, 1, 0, 0, 0, -1, -1, 0, 0),
	Basis(0, 0, 1, 0, -1, 0, 1, 0, 0),
	Basis(0, 1, 0, 0, 0, 1, 1, 0, 0),
	Basis(0, 0, -1, 0, 1, 0, 1, 0, 0),
	Basis(0, -1, 0, 0, 0, -1, 1, 0, 0)
};

Basis GridMap::get_cell_item_basis(const Vector3i &p_position) const {
	int orientation = get_cell_item_orientation(p_position);

	if (orientation == -1) {
		return Basis();
	}

	return get_basis_with_orthogonal_index(orientation);
}

Basis GridMap::get_basis_with_orthogonal_index(int p_index) const {
	ERR_FAIL_INDEX_V(p_index, 24, Basis());

	return _ortho_bases[p_index];
}

int GridMap::get_orthogonal_index_from_basis(const Basis &p_basis) const {
	Basis orth = p_basis;
	for (int i = 0; i < 3; i++) {
		for (int j = 0; j < 3; j++) {
			real_t v = orth[i][j];
			if (v > 0.5) {
				v = 1.0;
			} else if (v < -0.5) {
				v = -1.0;
			} else {
				v = 0;
			}

			orth[i][j] = v;
		}
	}

	for (int i = 0; i < 24; i++) {
		if (_ortho_bases[i] == orth) {
			return i;
		}
	}

	return 0;
}

Vector3i GridMap::local_to_map(const Vector3 &p_world_position) const {
	Vector3 map_position = (p_world_position / cell_size).floor();
	return Vector3i(map_position);
}

Vector3 GridMap::map_to_local(const Vector3i &p_map_position) const {
	Vector3 offset = _get_offset();
	Vector3 local_position(
			p_map_position.x * cell_size.x + offset.x,
			p_map_position.y * cell_size.y + offset.y,
			p_map_position.z * cell_size.z + offset.z);
	return local_position;
}

void GridMap::_octant_transform(const OctantKey &p_key) {
	ERR_FAIL_COND(!octant_map.has(p_key));
	Octant &g = *octant_map[p_key];
	PhysicsServer3D::get_singleton()->body_set_state(g.static_body, PhysicsServer3D::BODY_STATE_TRANSFORM, get_global_transform());

	if (g.collision_debug_instance.is_valid()) {
		RS::get_singleton()->instance_set_transform(g.collision_debug_instance, get_global_transform());
	}

	// update transform for NavigationServer regions and navigation debugmesh instances
	for (const KeyValue<IndexKey, Octant::NavigationCell> &E : g.navigation_cell_ids) {
		if (bake_navigation) {
			if (E.value.region.is_valid()) {
				NavigationServer3D::get_singleton()->region_set_transform(E.value.region, get_global_transform() * E.value.xform);
			}
			if (E.value.navigation_mesh_debug_instance.is_valid()) {
				RS::get_singleton()->instance_set_transform(E.value.navigation_mesh_debug_instance, get_global_transform() * E.value.xform);
			}
		}
	}

	for (int i = 0; i < g.multimesh_instances.size(); i++) {
		RS::get_singleton()->instance_set_transform(g.multimesh_instances[i].instance, get_global_transform());
	}
}

bool GridMap::_octant_update(const OctantKey &p_key) {
	ERR_FAIL_COND_V(!octant_map.has(p_key), false);
	Octant &g = *octant_map[p_key];
	if (!g.dirty) {
		return false;
	}

	//erase body shapes
	PhysicsServer3D::get_singleton()->body_clear_shapes(g.static_body);

	//erase body shapes debug
	if (g.collision_debug.is_valid()) {
		RS::get_singleton()->mesh_clear(g.collision_debug);
	}

	//erase navigation
	for (KeyValue<IndexKey, Octant::NavigationCell> &E : g.navigation_cell_ids) {
		if (E.value.region.is_valid()) {
			NavigationServer3D::get_singleton()->free(E.value.region);
			E.value.region = RID();
		}
		if (E.value.navigation_mesh_debug_instance.is_valid()) {
			RS::get_singleton()->free(E.value.navigation_mesh_debug_instance);
			E.value.navigation_mesh_debug_instance = RID();
		}
	}
	g.navigation_cell_ids.clear();

	//erase multimeshes

	for (int i = 0; i < g.multimesh_instances.size(); i++) {
		RS::get_singleton()->free(g.multimesh_instances[i].instance);
		RS::get_singleton()->free(g.multimesh_instances[i].multimesh);
	}
	g.multimesh_instances.clear();

	if (g.cells.size() == 0) {
		//octant no longer needed
		_octant_clean_up(p_key);
		return true;
	}

	Vector<Vector3> col_debug;

	/*
	 * foreach item in this octant,
	 * set item's multimesh's instance count to number of cells which have this item
	 * and set said multimesh bounding box to one containing all cells which have this item
	 */

	HashMap<int, List<Pair<Transform3D, IndexKey>>> multimesh_items;

	for (const IndexKey &E : g.cells) {
		ERR_CONTINUE(!cell_map.has(E));
		const Cell &c = cell_map[E];

		if (mesh_library.is_null() || !mesh_library->has_item(c.item)) {
			continue;
		}

		Vector3 cellpos = Vector3(E.x, E.y, E.z);
		Vector3 ofs = _get_offset();

		Transform3D xform;

		xform.basis = _ortho_bases[c.rot];
		xform.set_origin(cellpos * cell_size + ofs);
		xform.basis.scale(Vector3(cell_scale, cell_scale, cell_scale));
		if (baked_meshes.size() == 0) {
			if (mesh_library->get_item_mesh(c.item).is_valid()) {
				if (!multimesh_items.has(c.item)) {
					multimesh_items[c.item] = List<Pair<Transform3D, IndexKey>>();
				}

				Pair<Transform3D, IndexKey> p;
				p.first = xform * mesh_library->get_item_mesh_transform(c.item);
				p.second = E;
				multimesh_items[c.item].push_back(p);
			}
		}

		Vector<MeshLibrary::ShapeData> shapes = mesh_library->get_item_shapes(c.item);
		// add the item's shape at given xform to octant's static_body
		for (int i = 0; i < shapes.size(); i++) {
			// add the item's shape
			if (shapes[i].shape.is_null()) {
				continue;
			}
			PhysicsServer3D::get_singleton()->body_add_shape(g.static_body, shapes[i].shape->get_rid(), xform * shapes[i].local_transform);
			if (g.collision_debug.is_valid()) {
				shapes.write[i].shape->add_vertices_to_array(col_debug, xform * shapes[i].local_transform);
			}
		}

		// add the item's navigation_mesh at given xform to GridMap's Navigation ancestor
		Ref<NavigationMesh> navigation_mesh = mesh_library->get_item_navigation_mesh(c.item);
		if (navigation_mesh.is_valid()) {
			Octant::NavigationCell nm;
			nm.xform = xform * mesh_library->get_item_navigation_mesh_transform(c.item);
			nm.navigation_layers = mesh_library->get_item_navigation_layers(c.item);

			if (bake_navigation) {
				RID region = NavigationServer3D::get_singleton()->region_create();
				NavigationServer3D::get_singleton()->region_set_owner_id(region, get_instance_id());
				NavigationServer3D::get_singleton()->region_set_navigation_layers(region, nm.navigation_layers);
				NavigationServer3D::get_singleton()->region_set_navigation_mesh(region, navigation_mesh);
				NavigationServer3D::get_singleton()->region_set_transform(region, get_global_transform() * nm.xform);
				if (is_inside_tree()) {
					if (map_override.is_valid()) {
						NavigationServer3D::get_singleton()->region_set_map(region, map_override);
					} else {
						NavigationServer3D::get_singleton()->region_set_map(region, get_world_3d()->get_navigation_map());
					}
				}
				nm.region = region;

#ifdef DEBUG_ENABLED
				// add navigation debugmesh visual instances if debug is enabled
				SceneTree *st = SceneTree::get_singleton();
				if (st && st->is_debugging_navigation_hint()) {
					if (!nm.navigation_mesh_debug_instance.is_valid()) {
						RID navigation_mesh_debug_rid = navigation_mesh->get_debug_mesh()->get_rid();
						nm.navigation_mesh_debug_instance = RS::get_singleton()->instance_create();
						RS::get_singleton()->instance_set_base(nm.navigation_mesh_debug_instance, navigation_mesh_debug_rid);
					}
					if (is_inside_tree()) {
						RS::get_singleton()->instance_set_scenario(nm.navigation_mesh_debug_instance, get_world_3d()->get_scenario());
						RS::get_singleton()->instance_set_transform(nm.navigation_mesh_debug_instance, get_global_transform() * nm.xform);
					}
				}
#endif // DEBUG_ENABLED
			}
			g.navigation_cell_ids[E] = nm;
		}
	}

#ifdef DEBUG_ENABLED
	if (bake_navigation) {
		_update_octant_navigation_debug_edge_connections_mesh(p_key);
	}
#endif // DEBUG_ENABLED

	//update multimeshes, only if not baked
	if (baked_meshes.size() == 0) {
		for (const KeyValue<int, List<Pair<Transform3D, IndexKey>>> &E : multimesh_items) {
			Octant::MultimeshInstance mmi;

			RID mm = RS::get_singleton()->multimesh_create();
			RS::get_singleton()->multimesh_allocate_data(mm, E.value.size(), RS::MULTIMESH_TRANSFORM_3D);
			RS::get_singleton()->multimesh_set_mesh(mm, mesh_library->get_item_mesh(E.key)->get_rid());

			int idx = 0;
			for (const Pair<Transform3D, IndexKey> &F : E.value) {
				RS::get_singleton()->multimesh_instance_set_transform(mm, idx, F.first);
#ifdef TOOLS_ENABLED

				Octant::MultimeshInstance::Item it;
				it.index = idx;
				it.transform = F.first;
				it.key = F.second;
				mmi.items.push_back(it);
#endif

				idx++;
			}

			RID instance = RS::get_singleton()->instance_create();
			RS::get_singleton()->instance_set_base(instance, mm);

			if (is_inside_tree()) {
				RS::get_singleton()->instance_set_scenario(instance, get_world_3d()->get_scenario());
				RS::get_singleton()->instance_set_transform(instance, get_global_transform());
			}

			RS::ShadowCastingSetting cast_shadows = (RS::ShadowCastingSetting)mesh_library->get_item_mesh_cast_shadow(E.key);
			RS::get_singleton()->instance_geometry_set_cast_shadows_setting(instance, cast_shadows);

			mmi.multimesh = mm;
			mmi.instance = instance;

			g.multimesh_instances.push_back(mmi);
		}
	}

	if (col_debug.size()) {
		Array arr;
		arr.resize(RS::ARRAY_MAX);
		arr[RS::ARRAY_VERTEX] = col_debug;

		RS::get_singleton()->mesh_add_surface_from_arrays(g.collision_debug, RS::PRIMITIVE_LINES, arr);
		SceneTree *st = SceneTree::get_singleton();
		if (st) {
			RS::get_singleton()->mesh_surface_set_material(g.collision_debug, 0, st->get_debug_collision_material()->get_rid());
		}
	}

	g.dirty = false;

	return false;
}

void GridMap::_update_physics_bodies_collision_properties() {
	for (const KeyValue<OctantKey, Octant *> &E : octant_map) {
		PhysicsServer3D::get_singleton()->body_set_collision_layer(E.value->static_body, collision_layer);
		PhysicsServer3D::get_singleton()->body_set_collision_mask(E.value->static_body, collision_mask);
		PhysicsServer3D::get_singleton()->body_set_collision_priority(E.value->static_body, collision_priority);
	}
}

void GridMap::_update_physics_bodies_characteristics() {
	real_t friction = 1.0;
	real_t bounce = 0.0;
	if (physics_material.is_valid()) {
		friction = physics_material->computed_friction();
		bounce = physics_material->computed_bounce();
	}
	for (const KeyValue<OctantKey, Octant *> &E : octant_map) {
		PhysicsServer3D::get_singleton()->body_set_param(E.value->static_body, PhysicsServer3D::BODY_PARAM_FRICTION, friction);
		PhysicsServer3D::get_singleton()->body_set_param(E.value->static_body, PhysicsServer3D::BODY_PARAM_BOUNCE, bounce);
	}
}

void GridMap::_octant_enter_world(const OctantKey &p_key) {
	ERR_FAIL_COND(!octant_map.has(p_key));
	Octant &g = *octant_map[p_key];
	PhysicsServer3D::get_singleton()->body_set_state(g.static_body, PhysicsServer3D::BODY_STATE_TRANSFORM, get_global_transform());
	PhysicsServer3D::get_singleton()->body_set_space(g.static_body, get_world_3d()->get_space());

	if (g.collision_debug_instance.is_valid()) {
		RS::get_singleton()->instance_set_scenario(g.collision_debug_instance, get_world_3d()->get_scenario());
		RS::get_singleton()->instance_set_transform(g.collision_debug_instance, get_global_transform());
	}

	for (int i = 0; i < g.multimesh_instances.size(); i++) {
		RS::get_singleton()->instance_set_scenario(g.multimesh_instances[i].instance, get_world_3d()->get_scenario());
		RS::get_singleton()->instance_set_transform(g.multimesh_instances[i].instance, get_global_transform());
	}

	if (bake_navigation && mesh_library.is_valid()) {
		for (KeyValue<IndexKey, Octant::NavigationCell> &F : g.navigation_cell_ids) {
			if (cell_map.has(F.key) && F.value.region.is_valid() == false) {
				Ref<NavigationMesh> navigation_mesh = mesh_library->get_item_navigation_mesh(cell_map[F.key].item);
				if (navigation_mesh.is_valid()) {
					RID region = NavigationServer3D::get_singleton()->region_create();
					NavigationServer3D::get_singleton()->region_set_owner_id(region, get_instance_id());
					NavigationServer3D::get_singleton()->region_set_navigation_layers(region, F.value.navigation_layers);
					NavigationServer3D::get_singleton()->region_set_navigation_mesh(region, navigation_mesh);
					NavigationServer3D::get_singleton()->region_set_transform(region, get_global_transform() * F.value.xform);
					if (map_override.is_valid()) {
						NavigationServer3D::get_singleton()->region_set_map(region, map_override);
					} else {
						NavigationServer3D::get_singleton()->region_set_map(region, get_world_3d()->get_navigation_map());
					}

					F.value.region = region;
				}
			}
		}

#ifdef DEBUG_ENABLED
		if (bake_navigation) {
			if (!g.navigation_debug_edge_connections_instance.is_valid()) {
				g.navigation_debug_edge_connections_instance = RenderingServer::get_singleton()->instance_create();
			}
			if (g.navigation_debug_edge_connections_mesh.is_null()) {
				g.navigation_debug_edge_connections_mesh.instantiate();
			}

			_update_octant_navigation_debug_edge_connections_mesh(p_key);
		}
#endif // DEBUG_ENABLED
	}
}

void GridMap::_octant_exit_world(const OctantKey &p_key) {
	ERR_FAIL_NULL(RenderingServer::get_singleton());
	ERR_FAIL_NULL(PhysicsServer3D::get_singleton());
	ERR_FAIL_NULL(NavigationServer3D::get_singleton());

	ERR_FAIL_COND(!octant_map.has(p_key));
	Octant &g = *octant_map[p_key];
	PhysicsServer3D::get_singleton()->body_set_state(g.static_body, PhysicsServer3D::BODY_STATE_TRANSFORM, get_global_transform());
	PhysicsServer3D::get_singleton()->body_set_space(g.static_body, RID());

	if (g.collision_debug_instance.is_valid()) {
		RS::get_singleton()->instance_set_scenario(g.collision_debug_instance, RID());
	}

	for (int i = 0; i < g.multimesh_instances.size(); i++) {
		RS::get_singleton()->instance_set_scenario(g.multimesh_instances[i].instance, RID());
	}

	for (KeyValue<IndexKey, Octant::NavigationCell> &F : g.navigation_cell_ids) {
		if (F.value.region.is_valid()) {
			NavigationServer3D::get_singleton()->free(F.value.region);
			F.value.region = RID();
		}
		if (F.value.navigation_mesh_debug_instance.is_valid()) {
			RS::get_singleton()->free(F.value.navigation_mesh_debug_instance);
			F.value.navigation_mesh_debug_instance = RID();
		}
	}

#ifdef DEBUG_ENABLED
	if (bake_navigation) {
		if (g.navigation_debug_edge_connections_instance.is_valid()) {
			RenderingServer::get_singleton()->free(g.navigation_debug_edge_connections_instance);
			g.navigation_debug_edge_connections_instance = RID();
		}
		if (g.navigation_debug_edge_connections_mesh.is_valid()) {
			g.navigation_debug_edge_connections_mesh.unref();
		}
	}
#endif // DEBUG_ENABLED
}

void GridMap::_octant_clean_up(const OctantKey &p_key) {
	ERR_FAIL_NULL(RenderingServer::get_singleton());
	ERR_FAIL_NULL(PhysicsServer3D::get_singleton());
	ERR_FAIL_NULL(NavigationServer3D::get_singleton());

	ERR_FAIL_COND(!octant_map.has(p_key));
	Octant &g = *octant_map[p_key];

	if (g.collision_debug.is_valid()) {
		RS::get_singleton()->free(g.collision_debug);
	}
	if (g.collision_debug_instance.is_valid()) {
		RS::get_singleton()->free(g.collision_debug_instance);
	}

	PhysicsServer3D::get_singleton()->free(g.static_body);

	// Erase navigation
	for (const KeyValue<IndexKey, Octant::NavigationCell> &E : g.navigation_cell_ids) {
		if (E.value.region.is_valid()) {
			NavigationServer3D::get_singleton()->free(E.value.region);
		}
		if (E.value.navigation_mesh_debug_instance.is_valid()) {
			RS::get_singleton()->free(E.value.navigation_mesh_debug_instance);
		}
	}
	g.navigation_cell_ids.clear();

#ifdef DEBUG_ENABLED
	if (bake_navigation) {
		if (g.navigation_debug_edge_connections_instance.is_valid()) {
			RenderingServer::get_singleton()->free(g.navigation_debug_edge_connections_instance);
			g.navigation_debug_edge_connections_instance = RID();
		}
		if (g.navigation_debug_edge_connections_mesh.is_valid()) {
			g.navigation_debug_edge_connections_mesh.unref();
		}
	}
#endif // DEBUG_ENABLED

	//erase multimeshes

	for (int i = 0; i < g.multimesh_instances.size(); i++) {
		RS::get_singleton()->free(g.multimesh_instances[i].instance);
		RS::get_singleton()->free(g.multimesh_instances[i].multimesh);
	}
	g.multimesh_instances.clear();
}

void GridMap::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_WORLD: {
			last_transform = get_global_transform();

			for (const KeyValue<OctantKey, Octant *> &E : octant_map) {
				_octant_enter_world(E.key);
			}

			for (int i = 0; i < baked_meshes.size(); i++) {
				RS::get_singleton()->instance_set_scenario(baked_meshes[i].instance, get_world_3d()->get_scenario());
				RS::get_singleton()->instance_set_transform(baked_meshes[i].instance, get_global_transform());
			}
		} break;

		case NOTIFICATION_ENTER_TREE: {
#ifdef DEBUG_ENABLED
			if (bake_navigation && NavigationServer3D::get_singleton()->get_debug_navigation_enabled()) {
				_update_navigation_debug_edge_connections();
			}
#endif // DEBUG_ENABLED
			_update_visibility();
		} break;

		case NOTIFICATION_TRANSFORM_CHANGED: {
			Transform3D new_xform = get_global_transform();
			if (new_xform == last_transform) {
				break;
			}
			//update run
			for (const KeyValue<OctantKey, Octant *> &E : octant_map) {
				_octant_transform(E.key);
			}

			last_transform = new_xform;

			for (int i = 0; i < baked_meshes.size(); i++) {
				RS::get_singleton()->instance_set_transform(baked_meshes[i].instance, get_global_transform());
			}
		} break;

		case NOTIFICATION_EXIT_WORLD: {
			for (const KeyValue<OctantKey, Octant *> &E : octant_map) {
				_octant_exit_world(E.key);
			}

			//_queue_octants_dirty(MAP_DIRTY_INSTANCES|MAP_DIRTY_TRANSFORMS);
			//_update_octants_callback();
			//_update_area_instances();
			for (int i = 0; i < baked_meshes.size(); i++) {
				RS::get_singleton()->instance_set_scenario(baked_meshes[i].instance, RID());
			}
		} break;

		case NOTIFICATION_VISIBILITY_CHANGED: {
			_update_visibility();
		} break;
	}
}

void GridMap::_update_visibility() {
	if (!is_inside_tree()) {
		return;
	}

	for (KeyValue<OctantKey, Octant *> &e : octant_map) {
		Octant *octant = e.value;
		for (int i = 0; i < octant->multimesh_instances.size(); i++) {
			const Octant::MultimeshInstance &mi = octant->multimesh_instances[i];
			RS::get_singleton()->instance_set_visible(mi.instance, is_visible_in_tree());
		}
	}

	for (int i = 0; i < baked_meshes.size(); i++) {
		RS::get_singleton()->instance_set_visible(baked_meshes[i].instance, is_visible_in_tree());
	}
}

void GridMap::_queue_octants_dirty() {
	if (awaiting_update) {
		return;
	}

	callable_mp(this, &GridMap::_update_octants_callback).call_deferred();
	awaiting_update = true;
}

void GridMap::_recreate_octant_data() {
	recreating_octants = true;
	HashMap<IndexKey, Cell, IndexKey> cell_copy = cell_map;
	_clear_internal();
	for (const KeyValue<IndexKey, Cell> &E : cell_copy) {
		set_cell_item(Vector3i(E.key), E.value.item, E.value.rot);
	}
	recreating_octants = false;
}

void GridMap::_clear_internal() {
	for (const KeyValue<OctantKey, Octant *> &E : octant_map) {
		if (is_inside_world()) {
			_octant_exit_world(E.key);
		}

		_octant_clean_up(E.key);
		memdelete(E.value);
	}

	octant_map.clear();
	cell_map.clear();
}

void GridMap::clear() {
	_clear_internal();
	clear_baked_meshes();
}

#ifndef DISABLE_DEPRECATED
void GridMap::resource_changed(const Ref<Resource> &p_res) {
}
#endif

void GridMap::_update_octants_callback() {
	if (!awaiting_update) {
		return;
	}

	List<OctantKey> to_delete;
	for (const KeyValue<OctantKey, Octant *> &E : octant_map) {
		if (_octant_update(E.key)) {
			to_delete.push_back(E.key);
		}
	}

	while (to_delete.front()) {
		memdelete(octant_map[to_delete.front()->get()]);
		octant_map.erase(to_delete.front()->get());
		to_delete.pop_front();
	}

	_update_visibility();
	awaiting_update = false;
}

void GridMap::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_collision_layer", "layer"), &GridMap::set_collision_layer);
	ClassDB::bind_method(D_METHOD("get_collision_layer"), &GridMap::get_collision_layer);

	ClassDB::bind_method(D_METHOD("set_collision_mask", "mask"), &GridMap::set_collision_mask);
	ClassDB::bind_method(D_METHOD("get_collision_mask"), &GridMap::get_collision_mask);

	ClassDB::bind_method(D_METHOD("set_collision_mask_value", "layer_number", "value"), &GridMap::set_collision_mask_value);
	ClassDB::bind_method(D_METHOD("get_collision_mask_value", "layer_number"), &GridMap::get_collision_mask_value);

	ClassDB::bind_method(D_METHOD("set_collision_layer_value", "layer_number", "value"), &GridMap::set_collision_layer_value);
	ClassDB::bind_method(D_METHOD("get_collision_layer_value", "layer_number"), &GridMap::get_collision_layer_value);

	ClassDB::bind_method(D_METHOD("set_collision_priority", "priority"), &GridMap::set_collision_priority);
	ClassDB::bind_method(D_METHOD("get_collision_priority"), &GridMap::get_collision_priority);

	ClassDB::bind_method(D_METHOD("set_physics_material", "material"), &GridMap::set_physics_material);
	ClassDB::bind_method(D_METHOD("get_physics_material"), &GridMap::get_physics_material);

	ClassDB::bind_method(D_METHOD("set_bake_navigation", "bake_navigation"), &GridMap::set_bake_navigation);
	ClassDB::bind_method(D_METHOD("is_baking_navigation"), &GridMap::is_baking_navigation);

	ClassDB::bind_method(D_METHOD("set_navigation_map", "navigation_map"), &GridMap::set_navigation_map);
	ClassDB::bind_method(D_METHOD("get_navigation_map"), &GridMap::get_navigation_map);

	ClassDB::bind_method(D_METHOD("set_mesh_library", "mesh_library"), &GridMap::set_mesh_library);
	ClassDB::bind_method(D_METHOD("get_mesh_library"), &GridMap::get_mesh_library);

	ClassDB::bind_method(D_METHOD("set_cell_size", "size"), &GridMap::set_cell_size);
	ClassDB::bind_method(D_METHOD("get_cell_size"), &GridMap::get_cell_size);

	ClassDB::bind_method(D_METHOD("set_cell_scale", "scale"), &GridMap::set_cell_scale);
	ClassDB::bind_method(D_METHOD("get_cell_scale"), &GridMap::get_cell_scale);

	ClassDB::bind_method(D_METHOD("set_octant_size", "size"), &GridMap::set_octant_size);
	ClassDB::bind_method(D_METHOD("get_octant_size"), &GridMap::get_octant_size);

	ClassDB::bind_method(D_METHOD("set_cell_item", "position", "item", "orientation"), &GridMap::set_cell_item, DEFVAL(0));
	ClassDB::bind_method(D_METHOD("get_cell_item", "position"), &GridMap::get_cell_item);
	ClassDB::bind_method(D_METHOD("get_cell_item_orientation", "position"), &GridMap::get_cell_item_orientation);
	ClassDB::bind_method(D_METHOD("get_cell_item_basis", "position"), &GridMap::get_cell_item_basis);
	ClassDB::bind_method(D_METHOD("get_basis_with_orthogonal_index", "index"), &GridMap::get_basis_with_orthogonal_index);
	ClassDB::bind_method(D_METHOD("get_orthogonal_index_from_basis", "basis"), &GridMap::get_orthogonal_index_from_basis);

	ClassDB::bind_method(D_METHOD("local_to_map", "local_position"), &GridMap::local_to_map);
	ClassDB::bind_method(D_METHOD("map_to_local", "map_position"), &GridMap::map_to_local);

#ifndef DISABLE_DEPRECATED
	ClassDB::bind_method(D_METHOD("resource_changed", "resource"), &GridMap::resource_changed);
#endif

	ClassDB::bind_method(D_METHOD("set_center_x", "enable"), &GridMap::set_center_x);
	ClassDB::bind_method(D_METHOD("get_center_x"), &GridMap::get_center_x);
	ClassDB::bind_method(D_METHOD("set_center_y", "enable"), &GridMap::set_center_y);
	ClassDB::bind_method(D_METHOD("get_center_y"), &GridMap::get_center_y);
	ClassDB::bind_method(D_METHOD("set_center_z", "enable"), &GridMap::set_center_z);
	ClassDB::bind_method(D_METHOD("get_center_z"), &GridMap::get_center_z);

	ClassDB::bind_method(D_METHOD("clear"), &GridMap::clear);

	ClassDB::bind_method(D_METHOD("get_used_cells"), &GridMap::get_used_cells);
	ClassDB::bind_method(D_METHOD("get_used_cells_by_item", "item"), &GridMap::get_used_cells_by_item);

	ClassDB::bind_method(D_METHOD("get_meshes"), &GridMap::get_meshes);
	ClassDB::bind_method(D_METHOD("get_bake_meshes"), &GridMap::get_bake_meshes);
	ClassDB::bind_method(D_METHOD("get_bake_mesh_instance", "idx"), &GridMap::get_bake_mesh_instance);

	ClassDB::bind_method(D_METHOD("clear_baked_meshes"), &GridMap::clear_baked_meshes);
	ClassDB::bind_method(D_METHOD("make_baked_meshes", "gen_lightmap_uv", "lightmap_uv_texel_size"), &GridMap::make_baked_meshes, DEFVAL(false), DEFVAL(0.1));

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "mesh_library", PROPERTY_HINT_RESOURCE_TYPE, "MeshLibrary"), "set_mesh_library", "get_mesh_library");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "physics_material", PROPERTY_HINT_RESOURCE_TYPE, "PhysicsMaterial"), "set_physics_material", "get_physics_material");
	ADD_GROUP("Cell", "cell_");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "cell_size", PROPERTY_HINT_NONE, "suffix:m"), "set_cell_size", "get_cell_size");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "cell_octant_size", PROPERTY_HINT_RANGE, "1,1024,1"), "set_octant_size", "get_octant_size");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "cell_center_x"), "set_center_x", "get_center_x");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "cell_center_y"), "set_center_y", "get_center_y");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "cell_center_z"), "set_center_z", "get_center_z");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "cell_scale"), "set_cell_scale", "get_cell_scale");
	ADD_GROUP("Collision", "collision_");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "collision_layer", PROPERTY_HINT_LAYERS_3D_PHYSICS), "set_collision_layer", "get_collision_layer");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "collision_mask", PROPERTY_HINT_LAYERS_3D_PHYSICS), "set_collision_mask", "get_collision_mask");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "collision_priority"), "set_collision_priority", "get_collision_priority");
	ADD_GROUP("Navigation", "");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "bake_navigation"), "set_bake_navigation", "is_baking_navigation");

	BIND_CONSTANT(INVALID_CELL_ITEM);

	ADD_SIGNAL(MethodInfo("cell_size_changed", PropertyInfo(Variant::VECTOR3, "cell_size")));
	ADD_SIGNAL(MethodInfo(CoreStringName(changed)));
}

void GridMap::set_cell_scale(float p_scale) {
	cell_scale = p_scale;
	_recreate_octant_data();
}

float GridMap::get_cell_scale() const {
	return cell_scale;
}

TypedArray<Vector3i> GridMap::get_used_cells() const {
	TypedArray<Vector3i> a;
	a.resize(cell_map.size());
	int i = 0;
	for (const KeyValue<IndexKey, Cell> &E : cell_map) {
		Vector3i p(E.key.x, E.key.y, E.key.z);
		a[i++] = p;
	}

	return a;
}

TypedArray<Vector3i> GridMap::get_used_cells_by_item(int p_item) const {
	TypedArray<Vector3i> a;
	for (const KeyValue<IndexKey, Cell> &E : cell_map) {
		if ((int)E.value.item == p_item) {
			Vector3i p(E.key.x, E.key.y, E.key.z);
			a.push_back(p);
		}
	}

	return a;
}

Array GridMap::get_meshes() const {
	if (mesh_library.is_null()) {
		return Array();
	}

	Vector3 ofs = _get_offset();
	Array meshes;

	for (const KeyValue<IndexKey, Cell> &E : cell_map) {
		int id = E.value.item;
		if (!mesh_library->has_item(id)) {
			continue;
		}
		Ref<Mesh> mesh = mesh_library->get_item_mesh(id);
		if (mesh.is_null()) {
			continue;
		}

		IndexKey ik = E.key;

		Vector3 cellpos = Vector3(ik.x, ik.y, ik.z);

		Transform3D xform;

		xform.basis = _ortho_bases[E.value.rot];

		xform.set_origin(cellpos * cell_size + ofs);
		xform.basis.scale(Vector3(cell_scale, cell_scale, cell_scale));

		meshes.push_back(xform * mesh_library->get_item_mesh_transform(id));
		meshes.push_back(mesh);
	}

	return meshes;
}

Vector3 GridMap::_get_offset() const {
	return Vector3(
			cell_size.x * 0.5 * int(center_x),
			cell_size.y * 0.5 * int(center_y),
			cell_size.z * 0.5 * int(center_z));
}

void GridMap::clear_baked_meshes() {
	ERR_FAIL_NULL(RenderingServer::get_singleton());
	for (int i = 0; i < baked_meshes.size(); i++) {
		RS::get_singleton()->free(baked_meshes[i].instance);
	}
	baked_meshes.clear();

	_recreate_octant_data();
}

void GridMap::make_baked_meshes(bool p_gen_lightmap_uv, float p_lightmap_uv_texel_size) {
	if (mesh_library.is_null()) {
		return;
	}

	//generate
	HashMap<OctantKey, HashMap<Ref<Material>, Ref<SurfaceTool>>, OctantKey> surface_map;

	for (KeyValue<IndexKey, Cell> &E : cell_map) {
		IndexKey key = E.key;

		int item = E.value.item;
		if (!mesh_library->has_item(item)) {
			continue;
		}

		Ref<Mesh> mesh = mesh_library->get_item_mesh(item);
		if (mesh.is_null()) {
			continue;
		}

		Vector3 cellpos = Vector3(key.x, key.y, key.z);
		Vector3 ofs = _get_offset();

		Transform3D xform;

		xform.basis = _ortho_bases[E.value.rot];
		xform.set_origin(cellpos * cell_size + ofs);
		xform.basis.scale(Vector3(cell_scale, cell_scale, cell_scale));

		OctantKey ok;
		ok.x = key.x / octant_size;
		ok.y = key.y / octant_size;
		ok.z = key.z / octant_size;

		if (!surface_map.has(ok)) {
			surface_map[ok] = HashMap<Ref<Material>, Ref<SurfaceTool>>();
		}

		HashMap<Ref<Material>, Ref<SurfaceTool>> &mat_map = surface_map[ok];

		for (int i = 0; i < mesh->get_surface_count(); i++) {
			if (mesh->surface_get_primitive_type(i) != Mesh::PRIMITIVE_TRIANGLES) {
				continue;
			}

			Ref<Material> surf_mat = mesh->surface_get_material(i);
			if (!mat_map.has(surf_mat)) {
				Ref<SurfaceTool> st;
				st.instantiate();
				st->begin(Mesh::PRIMITIVE_TRIANGLES);
				st->set_material(surf_mat);
				mat_map[surf_mat] = st;
			}

			mat_map[surf_mat]->append_from(mesh, i, xform);
		}
	}

	for (KeyValue<OctantKey, HashMap<Ref<Material>, Ref<SurfaceTool>>> &E : surface_map) {
		Ref<ArrayMesh> mesh;
		mesh.instantiate();
		for (KeyValue<Ref<Material>, Ref<SurfaceTool>> &F : E.value) {
			F.value->commit(mesh);
		}

		BakedMesh bm;
		bm.mesh = mesh;
		bm.instance = RS::get_singleton()->instance_create();
		RS::get_singleton()->instance_set_base(bm.instance, bm.mesh->get_rid());
		RS::get_singleton()->instance_attach_object_instance_id(bm.instance, get_instance_id());
		if (is_inside_tree()) {
			RS::get_singleton()->instance_set_scenario(bm.instance, get_world_3d()->get_scenario());
			RS::get_singleton()->instance_set_transform(bm.instance, get_global_transform());
		}

		if (p_gen_lightmap_uv) {
			mesh->lightmap_unwrap(get_global_transform(), p_lightmap_uv_texel_size);
		}
		baked_meshes.push_back(bm);
	}

	_recreate_octant_data();
}

Array GridMap::get_bake_meshes() {
	if (!baked_meshes.size()) {
		make_baked_meshes(true);
	}

	Array arr;
	for (int i = 0; i < baked_meshes.size(); i++) {
		arr.push_back(baked_meshes[i].mesh);
		arr.push_back(Transform3D());
	}

	return arr;
}

RID GridMap::get_bake_mesh_instance(int p_idx) {
	ERR_FAIL_INDEX_V(p_idx, baked_meshes.size(), RID());
	return baked_meshes[p_idx].instance;
}

GridMap::GridMap() {
	set_notify_transform(true);
#ifdef DEBUG_ENABLED
	NavigationServer3D::get_singleton()->connect("map_changed", callable_mp(this, &GridMap::_navigation_map_changed));
	NavigationServer3D::get_singleton()->connect("navigation_debug_changed", callable_mp(this, &GridMap::_update_navigation_debug_edge_connections));
#endif // DEBUG_ENABLED
}

void GridMap::navmesh_parse_init() {
	ERR_FAIL_NULL(NavigationServer3D::get_singleton());
	if (!_navmesh_source_geometry_parser.is_valid()) {
		_navmesh_source_geometry_parsing_callback = callable_mp_static(&GridMap::navmesh_parse_source_geometry);
		_navmesh_source_geometry_parser = NavigationServer3D::get_singleton()->source_geometry_parser_create();
		NavigationServer3D::get_singleton()->source_geometry_parser_set_callback(_navmesh_source_geometry_parser, _navmesh_source_geometry_parsing_callback);
	}
}

void GridMap::navmesh_parse_source_geometry(const Ref<NavigationMesh> &p_navigation_mesh, Ref<NavigationMeshSourceGeometryData3D> p_source_geometry_data, Node *p_node) {
	GridMap *gridmap = Object::cast_to<GridMap>(p_node);

	if (gridmap == nullptr) {
		return;
	}

	NavigationMesh::ParsedGeometryType parsed_geometry_type = p_navigation_mesh->get_parsed_geometry_type();
	uint32_t parsed_collision_mask = p_navigation_mesh->get_collision_mask();

	if (parsed_geometry_type == NavigationMesh::PARSED_GEOMETRY_MESH_INSTANCES || parsed_geometry_type == NavigationMesh::PARSED_GEOMETRY_BOTH) {
		Array meshes = gridmap->get_meshes();
		Transform3D xform = gridmap->get_global_transform();
		for (int i = 0; i < meshes.size(); i += 2) {
			Ref<Mesh> mesh = meshes[i + 1];
			if (mesh.is_valid()) {
				p_source_geometry_data->add_mesh(mesh, xform * (Transform3D)meshes[i]);
			}
		}
	}

	else if ((parsed_geometry_type == NavigationMesh::PARSED_GEOMETRY_STATIC_COLLIDERS || parsed_geometry_type == NavigationMesh::PARSED_GEOMETRY_BOTH) && (gridmap->get_collision_layer() & parsed_collision_mask)) {
		Array shapes = gridmap->get_collision_shapes();
		for (int i = 0; i < shapes.size(); i += 2) {
			RID shape = shapes[i + 1];
			PhysicsServer3D::ShapeType type = PhysicsServer3D::get_singleton()->shape_get_type(shape);
			Variant data = PhysicsServer3D::get_singleton()->shape_get_data(shape);

			switch (type) {
				case PhysicsServer3D::SHAPE_SPHERE: {
					real_t radius = data;
					Array arr;
					arr.resize(RS::ARRAY_MAX);
					SphereMesh::create_mesh_array(arr, radius, radius * 2.0);
					p_source_geometry_data->add_mesh_array(arr, shapes[i]);
				} break;
				case PhysicsServer3D::SHAPE_BOX: {
					Vector3 extents = data;
					Array arr;
					arr.resize(RS::ARRAY_MAX);
					BoxMesh::create_mesh_array(arr, extents * 2.0);
					p_source_geometry_data->add_mesh_array(arr, shapes[i]);
				} break;
				case PhysicsServer3D::SHAPE_CAPSULE: {
					Dictionary dict = data;
					real_t radius = dict["radius"];
					real_t height = dict["height"];
					Array arr;
					arr.resize(RS::ARRAY_MAX);
					CapsuleMesh::create_mesh_array(arr, radius, height);
					p_source_geometry_data->add_mesh_array(arr, shapes[i]);
				} break;
				case PhysicsServer3D::SHAPE_CYLINDER: {
					Dictionary dict = data;
					real_t radius = dict["radius"];
					real_t height = dict["height"];
					Array arr;
					arr.resize(RS::ARRAY_MAX);
					CylinderMesh::create_mesh_array(arr, radius, radius, height);
					p_source_geometry_data->add_mesh_array(arr, shapes[i]);
				} break;
				case PhysicsServer3D::SHAPE_CONVEX_POLYGON: {
					PackedVector3Array vertices = data;
					Geometry3D::MeshData md;

					Error err = ConvexHullComputer::convex_hull(vertices, md);

					if (err == OK) {
						PackedVector3Array faces;

						for (const Geometry3D::MeshData::Face &face : md.faces) {
							for (uint32_t k = 2; k < face.indices.size(); ++k) {
								faces.push_back(md.vertices[face.indices[0]]);
								faces.push_back(md.vertices[face.indices[k - 1]]);
								faces.push_back(md.vertices[face.indices[k]]);
							}
						}

						p_source_geometry_data->add_faces(faces, shapes[i]);
					}
				} break;
				case PhysicsServer3D::SHAPE_CONCAVE_POLYGON: {
					Dictionary dict = data;
					PackedVector3Array faces = Variant(dict["faces"]);
					p_source_geometry_data->add_faces(faces, shapes[i]);
				} break;
				case PhysicsServer3D::SHAPE_HEIGHTMAP: {
					Dictionary dict = data;
					///< dict( int:"width", int:"depth",float:"cell_size", float_array:"heights"
					int heightmap_depth = dict["depth"];
					int heightmap_width = dict["width"];

					if (heightmap_depth >= 2 && heightmap_width >= 2) {
						const Vector<real_t> &map_data = dict["heights"];

						Vector2 heightmap_gridsize(heightmap_width - 1, heightmap_depth - 1);
						Vector3 start = Vector3(heightmap_gridsize.x, 0, heightmap_gridsize.y) * -0.5;

						Vector<Vector3> vertex_array;
						vertex_array.resize((heightmap_depth - 1) * (heightmap_width - 1) * 6);
						Vector3 *vertex_array_ptrw = vertex_array.ptrw();
						const real_t *map_data_ptr = map_data.ptr();
						int vertex_index = 0;

						for (int d = 0; d < heightmap_depth - 1; d++) {
							for (int w = 0; w < heightmap_width - 1; w++) {
								vertex_array_ptrw[vertex_index] = start + Vector3(w, map_data_ptr[(heightmap_width * d) + w], d);
								vertex_array_ptrw[vertex_index + 1] = start + Vector3(w + 1, map_data_ptr[(heightmap_width * d) + w + 1], d);
								vertex_array_ptrw[vertex_index + 2] = start + Vector3(w, map_data_ptr[(heightmap_width * d) + heightmap_width + w], d + 1);
								vertex_array_ptrw[vertex_index + 3] = start + Vector3(w + 1, map_data_ptr[(heightmap_width * d) + w + 1], d);
								vertex_array_ptrw[vertex_index + 4] = start + Vector3(w + 1, map_data_ptr[(heightmap_width * d) + heightmap_width + w + 1], d + 1);
								vertex_array_ptrw[vertex_index + 5] = start + Vector3(w, map_data_ptr[(heightmap_width * d) + heightmap_width + w], d + 1);
								vertex_index += 6;
							}
						}
						if (vertex_array.size() > 0) {
							p_source_geometry_data->add_faces(vertex_array, shapes[i]);
						}
					}
				} break;
				default: {
					WARN_PRINT("Unsupported collision shape type.");
				} break;
			}
		}
	}
}

#ifdef DEBUG_ENABLED
void GridMap::_update_navigation_debug_edge_connections() {
	if (bake_navigation) {
		for (const KeyValue<OctantKey, Octant *> &E : octant_map) {
			_update_octant_navigation_debug_edge_connections_mesh(E.key);
		}
	}
}

void GridMap::_navigation_map_changed(RID p_map) {
	if (bake_navigation && is_inside_tree() && p_map == get_world_3d()->get_navigation_map()) {
		_update_navigation_debug_edge_connections();
	}
}
#endif // DEBUG_ENABLED

GridMap::~GridMap() {
	clear();
#ifdef DEBUG_ENABLED
	NavigationServer3D::get_singleton()->disconnect("map_changed", callable_mp(this, &GridMap::_navigation_map_changed));
	NavigationServer3D::get_singleton()->disconnect("navigation_debug_changed", callable_mp(this, &GridMap::_update_navigation_debug_edge_connections));
#endif // DEBUG_ENABLED
}

#ifdef DEBUG_ENABLED
void GridMap::_update_octant_navigation_debug_edge_connections_mesh(const OctantKey &p_key) {
	ERR_FAIL_COND(!octant_map.has(p_key));
	Octant &g = *octant_map[p_key];

	if (!NavigationServer3D::get_singleton()->get_debug_navigation_enabled()) {
		if (g.navigation_debug_edge_connections_instance.is_valid()) {
			RS::get_singleton()->instance_set_visible(g.navigation_debug_edge_connections_instance, false);
		}
		return;
	}

	if (!is_inside_tree()) {
		return;
	}

	if (!bake_navigation) {
		if (g.navigation_debug_edge_connections_instance.is_valid()) {
			RS::get_singleton()->instance_set_visible(g.navigation_debug_edge_connections_instance, false);
		}
		return;
	}

	if (!g.navigation_debug_edge_connections_instance.is_valid()) {
		g.navigation_debug_edge_connections_instance = RenderingServer::get_singleton()->instance_create();
	}

	if (g.navigation_debug_edge_connections_mesh.is_null()) {
		g.navigation_debug_edge_connections_mesh.instantiate();
	}

	g.navigation_debug_edge_connections_mesh->clear_surfaces();

	float edge_connection_margin = NavigationServer3D::get_singleton()->map_get_edge_connection_margin(get_world_3d()->get_navigation_map());
	float half_edge_connection_margin = edge_connection_margin * 0.5;

	Vector<Vector3> vertex_array;

	for (KeyValue<IndexKey, Octant::NavigationCell> &F : g.navigation_cell_ids) {
		if (cell_map.has(F.key) && F.value.region.is_valid()) {
			int connections_count = NavigationServer3D::get_singleton()->region_get_connections_count(F.value.region);
			if (connections_count == 0) {
				continue;
			}

			for (int i = 0; i < connections_count; i++) {
				Vector3 connection_pathway_start = NavigationServer3D::get_singleton()->region_get_connection_pathway_start(F.value.region, i);
				Vector3 connection_pathway_end = NavigationServer3D::get_singleton()->region_get_connection_pathway_end(F.value.region, i);

				Vector3 direction_start_end = connection_pathway_start.direction_to(connection_pathway_end);
				Vector3 direction_end_start = connection_pathway_end.direction_to(connection_pathway_start);

				Vector3 start_right_dir = direction_start_end.cross(Vector3(0, 1, 0));
				Vector3 start_left_dir = -start_right_dir;

				Vector3 end_right_dir = direction_end_start.cross(Vector3(0, 1, 0));
				Vector3 end_left_dir = -end_right_dir;

				Vector3 left_start_pos = connection_pathway_start + (start_left_dir * half_edge_connection_margin);
				Vector3 right_start_pos = connection_pathway_start + (start_right_dir * half_edge_connection_margin);
				Vector3 left_end_pos = connection_pathway_end + (end_right_dir * half_edge_connection_margin);
				Vector3 right_end_pos = connection_pathway_end + (end_left_dir * half_edge_connection_margin);

				vertex_array.push_back(right_end_pos);
				vertex_array.push_back(left_start_pos);
				vertex_array.push_back(right_start_pos);

				vertex_array.push_back(left_end_pos);
				vertex_array.push_back(right_end_pos);
				vertex_array.push_back(right_start_pos);
			}
		}
	}

	if (vertex_array.size() == 0) {
		return;
	}

	Ref<StandardMaterial3D> edge_connections_material = NavigationServer3D::get_singleton()->get_debug_navigation_edge_connections_material();

	Array mesh_array;
	mesh_array.resize(Mesh::ARRAY_MAX);
	mesh_array[Mesh::ARRAY_VERTEX] = vertex_array;

	g.navigation_debug_edge_connections_mesh->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, mesh_array);
	g.navigation_debug_edge_connections_mesh->surface_set_material(0, edge_connections_material);

	RS::get_singleton()->instance_set_base(g.navigation_debug_edge_connections_instance, g.navigation_debug_edge_connections_mesh->get_rid());
	RS::get_singleton()->instance_set_visible(g.navigation_debug_edge_connections_instance, is_visible_in_tree());
	if (is_inside_tree()) {
		RS::get_singleton()->instance_set_scenario(g.navigation_debug_edge_connections_instance, get_world_3d()->get_scenario());
	}

	bool enable_edge_connections = NavigationServer3D::get_singleton()->get_debug_navigation_enable_edge_connections();
	if (!enable_edge_connections) {
		RS::get_singleton()->instance_set_visible(g.navigation_debug_edge_connections_instance, false);
	}
}
#endif // DEBUG_ENABLED

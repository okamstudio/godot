/**************************************************************************/
/*  grid_map.h                                                            */
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

#ifndef GRID_MAP_H
#define GRID_MAP_H

#include "scene/3d/node_3d.h"
#include "scene/resources/3d/mesh_library.h"
#include "scene/resources/multimesh.h"

//heh heh, godotsphir!! this shares no code and the design is completely different with previous projects i've done..
//should scale better with hardware that supports instancing

class NavigationMesh;
class NavigationMeshSourceGeometryData3D;
class PhysicsMaterial;

class GridMap : public Node3D {
	GDCLASS(GridMap, Node3D);

	enum {
		MAP_DIRTY_TRANSFORMS = 1,
		MAP_DIRTY_INSTANCES = 2,
	};

	union IndexKey {
		struct {
			int16_t x;
			int16_t y;
			int16_t z;
		};
		uint64_t key = 0;

		static uint32_t hash(const IndexKey &p_key) {
			return hash_one_uint64(p_key.key);
		}
		_FORCE_INLINE_ bool operator<(const IndexKey &p_key) const {
			return key < p_key.key;
		}
		_FORCE_INLINE_ bool operator==(const IndexKey &p_key) const {
			return key == p_key.key;
		}

		_FORCE_INLINE_ operator Vector3i() const {
			return Vector3i(x, y, z);
		}

		IndexKey(Vector3i p_vector) {
			x = (int16_t)p_vector.x;
			y = (int16_t)p_vector.y;
			z = (int16_t)p_vector.z;
		}
		IndexKey() {}
	};

	/**
	 * @brief A Cell is a single cell in the cube map space; it is defined by its coordinates and the populating Item, identified by int id.
	 */
	union Cell {
		struct {
			unsigned int item : 16;
			unsigned int rot : 5;
			unsigned int layer : 8;
		};
		uint32_t cell = 0;
	};

	/**
	 * @brief An Octant is a prism containing Cells, and possibly belonging to an Area.
	 * A GridMap can have multiple Octants.
	 */
	struct Octant {
		struct NavigationCell {
			RID region;
			Transform3D xform;
			RID navigation_mesh_debug_instance;
			uint32_t navigation_layers = 1;
		};

		struct MultimeshInstance {
			RID instance;
			RID multimesh;
			struct Item {
				int index = 0;
				Transform3D transform;
				IndexKey key;
			};

			Vector<Item> items; //tools only, for changing visibility
		};

		Vector<MultimeshInstance> multimesh_instances;
		HashSet<IndexKey> cells;
		RID collision_debug;
		RID collision_debug_instance;
#ifdef DEBUG_ENABLED
		RID navigation_debug_edge_connections_instance;
		Ref<ArrayMesh> navigation_debug_edge_connections_mesh;
#endif // DEBUG_ENABLED

		bool dirty = false;
		RID static_body;
		HashMap<IndexKey, NavigationCell> navigation_cell_ids;
	};

	union OctantKey {
		struct {
			int16_t x;
			int16_t y;
			int16_t z;
			int16_t empty;
		};

		uint64_t key = 0;

		static uint32_t hash(const OctantKey &p_key) {
			return hash_one_uint64(p_key.key);
		}
		_FORCE_INLINE_ bool operator==(const OctantKey &p_key) const {
			return key == p_key.key;
		}

		//OctantKey(const IndexKey& p_k, int p_item) { indexkey=p_k.key; item=p_item; }
		OctantKey() {}
	};

	uint32_t collision_layer = 1;
	uint32_t collision_mask = 1;
	real_t collision_priority = 1.0;
	Ref<PhysicsMaterial> physics_material;
	bool bake_navigation = false;
	RID map_override;

	Transform3D last_transform;

	bool _in_tree = false;
	Vector3 cell_size = Vector3(2, 2, 2);
	int octant_size = 8;
	bool center_x = true;
	bool center_y = true;
	bool center_z = true;
	float cell_scale = 1.0;

	bool recreating_octants = false;

	Ref<MeshLibrary> mesh_library;

	HashMap<OctantKey, Octant *, OctantKey> octant_map;
	HashMap<IndexKey, Cell, IndexKey> cell_map;

	void _recreate_octant_data();

	struct BakeLight {
		RS::LightType type = RS::LightType::LIGHT_DIRECTIONAL;
		Vector3 pos;
		Vector3 dir;
		float param[RS::LIGHT_PARAM_MAX] = {};
	};

	_FORCE_INLINE_ Vector3 _octant_get_offset(const OctantKey &p_key) const {
		return Vector3(p_key.x, p_key.y, p_key.z) * cell_size * octant_size;
	}

	void _update_physics_bodies_collision_properties();
	void _update_physics_bodies_characteristics();
	void _octant_enter_world(const OctantKey &p_key);
	void _octant_exit_world(const OctantKey &p_key);
	bool _octant_update(const OctantKey &p_key);
	void _octant_clean_up(const OctantKey &p_key);
	void _octant_transform(const OctantKey &p_key);
#ifdef DEBUG_ENABLED
	void _update_octant_navigation_debug_edge_connections_mesh(const OctantKey &p_key);
	void _navigation_map_changed(RID p_map);
	void _update_navigation_debug_edge_connections();
#endif // DEBUG_ENABLED
	bool awaiting_update = false;

	void _queue_octants_dirty();
	void _update_octants_callback();

#ifndef DISABLE_DEPRECATED
	void resource_changed(const Ref<Resource> &p_res);
#endif

	void _clear_internal();

	Vector3 _get_offset() const;

	struct BakedMesh {
		Ref<Mesh> mesh;
		RID instance;
	};

	Vector<BakedMesh> baked_meshes;

protected:
	bool _set(const StringName &p_name, const Variant &p_value);
	bool _get(const StringName &p_name, Variant &r_ret) const;
	void _get_property_list(List<PropertyInfo> *p_list) const;

	void _notification(int p_what);
	void _update_visibility();
	static void _bind_methods();

public:
	enum {
		INVALID_CELL_ITEM = -1
	};

	void set_collision_layer(uint32_t p_layer);
	uint32_t get_collision_layer() const;

	void set_collision_mask(uint32_t p_mask);
	uint32_t get_collision_mask() const;

	void set_collision_layer_value(int p_layer_number, bool p_value);
	bool get_collision_layer_value(int p_layer_number) const;

	void set_collision_mask_value(int p_layer_number, bool p_value);
	bool get_collision_mask_value(int p_layer_number) const;

	void set_collision_priority(real_t p_priority);
	real_t get_collision_priority() const;

	void set_physics_material(Ref<PhysicsMaterial> p_material);
	Ref<PhysicsMaterial> get_physics_material() const;

	Array get_collision_shapes() const;

	void set_bake_navigation(bool p_bake_navigation);
	bool is_baking_navigation();

	void set_navigation_map(RID p_navigation_map);
	RID get_navigation_map() const;

	void set_mesh_library(const Ref<MeshLibrary> &p_mesh_library);
	Ref<MeshLibrary> get_mesh_library() const;

	void set_cell_size(const Vector3 &p_size);
	Vector3 get_cell_size() const;

	void set_octant_size(int p_size);
	int get_octant_size() const;

	void set_center_x(bool p_enable);
	bool get_center_x() const;
	void set_center_y(bool p_enable);
	bool get_center_y() const;
	void set_center_z(bool p_enable);
	bool get_center_z() const;

	void set_cell_item(const Vector3i &p_position, int p_item, int p_rot = 0);
	int get_cell_item(const Vector3i &p_position) const;
	int get_cell_item_orientation(const Vector3i &p_position) const;
	Basis get_cell_item_basis(const Vector3i &p_position) const;
	Basis get_basis_with_orthogonal_index(int p_index) const;
	int get_orthogonal_index_from_basis(const Basis &p_basis) const;

	Vector3i local_to_map(const Vector3 &p_local_position) const;
	Vector3 map_to_local(const Vector3i &p_map_position) const;

	void set_cell_scale(float p_scale);
	float get_cell_scale() const;

	TypedArray<Vector3i> get_used_cells() const;
	TypedArray<Vector3i> get_used_cells_by_item(int p_item) const;

	Array get_meshes() const;

	void clear_baked_meshes();
	void make_baked_meshes(bool p_gen_lightmap_uv = false, float p_lightmap_uv_texel_size = 0.1);

	void clear();

	Array get_bake_meshes();
	RID get_bake_mesh_instance(int p_idx);

private:
	static Callable _navmesh_source_geometry_parsing_callback;
	static RID _navmesh_source_geometry_parser;

public:
	static void navmesh_parse_init();
	static void navmesh_parse_source_geometry(const Ref<NavigationMesh> &p_navigation_mesh, Ref<NavigationMeshSourceGeometryData3D> p_source_geometry_data, Node *p_node);

	GridMap();
	~GridMap();
};

#endif // GRID_MAP_H

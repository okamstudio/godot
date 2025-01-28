/**************************************************************************/
/*  tile_map.h                                                            */
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

#ifndef TILE_MAP_H
#define TILE_MAP_H

#include "scene/2d/tile_map_layer.h"
#include "scene/property_list_helper.h"
#include "scene/resources/2d/tile_set.h"

class Control;
class NavigationMeshSourceGeometryData2D;
class TileMapLayer;
class TerrainConstraint;

enum TileMapDataFormat {
	TILE_MAP_DATA_FORMAT_1 = 0,
	TILE_MAP_DATA_FORMAT_2,
	TILE_MAP_DATA_FORMAT_3,
	TILE_MAP_DATA_FORMAT_MAX,
};

class TileMap : public Node2D {
	GDCLASS(TileMap, Node2D)

public:
	// Kept for compatibility, but should use TileMapLayer::VisibilityMode instead.
	enum VisibilityMode {
		VISIBILITY_MODE_DEFAULT,
		VISIBILITY_MODE_FORCE_SHOW,
		VISIBILITY_MODE_FORCE_HIDE,
	};

private:
	friend class TileSetPlugin;

	// A compatibility enum to specify how is the data if formatted.
	mutable TileMapDataFormat format = TileMapDataFormat::TILE_MAP_DATA_FORMAT_3;

	// Properties.
	Ref<TileSet> tile_set;
	int rendering_quadrant_size = 16;
	bool collision_animatable = false;
	VisibilityMode collision_visibility_mode = VISIBILITY_MODE_DEFAULT;
	VisibilityMode navigation_visibility_mode = VISIBILITY_MODE_DEFAULT;

	// Layers.
	LocalVector<TileMapLayer *> layers;

	static inline PropertyListHelper base_property_helper;
	PropertyListHelper property_helper;

	// Transforms for collision_animatable.
	Transform2D last_valid_transform;
	Transform2D new_transform;

	void _tile_set_changed();

	void _emit_changed();

	// Kept for compatibility with TileMap. With TileMapLayers as individual nodes, the format is stored directly in the array.
	void _set_tile_map_data_using_compatibility_format(int p_layer, TileMapDataFormat p_format, const Vector<int> &p_data);
	Vector<int> _get_tile_map_data_using_compatibility_format(int p_layer) const;
	void _set_layer_tile_data(int p_layer, const PackedInt32Array &p_data);

protected:
	bool _set(const StringName &p_name, const Variant &p_value);
	bool _get(const StringName &p_name, Variant &r_ret) const;
	void _get_property_list(List<PropertyInfo> *p_list) const;
	bool _property_can_revert(const StringName &p_name) const { return property_helper.property_can_revert(p_name); }
	bool _property_get_revert(const StringName &p_name, Variant &r_property) const { return property_helper.property_get_revert(p_name, r_property); }

	void _notification(int p_what);
	static void _bind_methods();

#ifndef DISABLE_DEPRECATED
	Rect2i _get_used_rect_bind_compat_78328();
	void _set_quadrant_size_compat_81070(int p_quadrant_size);
	int _get_quadrant_size_compat_81070() const;
	VisibilityMode _get_collision_visibility_mode_bind_compat_87115();
	VisibilityMode _get_navigation_visibility_mode_bind_compat_87115();

	static void _bind_compatibility_methods();
#endif // DISABLE_DEPRECATED

public:
#ifdef DEBUG_ENABLED
	virtual Rect2 _edit_get_rect() const override;
#endif // DEBUG_ENABLED

#ifndef DISABLE_DEPRECATED
	void force_update(int p_layer);
#endif // DISABLE_DEPRECATED

	void set_rendering_quadrant_size(int p_size);
	int get_rendering_quadrant_size() const;

	// Accessors.
	void set_tileset(const Ref<TileSet> &p_tileset);
	Ref<TileSet> get_tileset() const;

	// Layers management.
	int get_layers_count() const;
	void add_layer(int p_to_pos);
	void move_layer(int p_layer, int p_to_pos);
	void remove_layer(int p_layer);

	void set_layer_name(int p_layer, String p_name);
	String get_layer_name(int p_layer) const;
	void set_layer_enabled(int p_layer, bool p_visible);
	bool is_layer_enabled(int p_layer) const;
	void set_layer_modulate(int p_layer, Color p_modulate);
	Color get_layer_modulate(int p_layer) const;
	void set_layer_y_sort_enabled(int p_layer, bool p_enabled);
	bool is_layer_y_sort_enabled(int p_layer) const;
	void set_layer_y_sort_origin(int p_layer, int p_y_sort_origin);
	int get_layer_y_sort_origin(int p_layer) const;
	void set_layer_z_index(int p_layer, int p_z_index);
	int get_layer_z_index(int p_layer) const;
	void set_layer_navigation_enabled(int p_layer, bool p_enabled);
	bool is_layer_navigation_enabled(int p_layer) const;
	void set_layer_navigation_map(int p_layer, RID p_map);
	RID get_layer_navigation_map(int p_layer) const;

	void set_collision_animatable(bool p_collision_animatable);
	bool is_collision_animatable() const;

	// Debug visibility modes.
	void set_collision_visibility_mode(VisibilityMode p_show_collision);
	VisibilityMode get_collision_visibility_mode() const;

	void set_navigation_visibility_mode(VisibilityMode p_show_navigation);
	VisibilityMode get_navigation_visibility_mode() const;

	// Cells accessors.
	void set_cell(int p_layer, const Vector2i &p_coords, int p_source_id = TileSet::INVALID_SOURCE, const Vector2i p_atlas_coords = TileSetSource::INVALID_ATLAS_COORDS, int p_alternative_tile = 0);
	void erase_cell(int p_layer, const Vector2i &p_coords);
	int get_cell_source_id(int p_layer, const Vector2i &p_coords, bool p_use_proxies = false) const;
	Vector2i get_cell_atlas_coords(int p_layer, const Vector2i &p_coords, bool p_use_proxies = false) const;
	int get_cell_alternative_tile(int p_layer, const Vector2i &p_coords, bool p_use_proxies = false) const;
	// Helper method to make accessing the data easier.
	TileData *get_cell_tile_data(int p_layer, const Vector2i &p_coords, bool p_use_proxies = false) const;

	bool is_cell_flipped_h(int p_layer, const Vector2i &p_coords, bool p_use_proxies = false) const;
	bool is_cell_flipped_v(int p_layer, const Vector2i &p_coords, bool p_use_proxies = false) const;
	bool is_cell_transposed(int p_layer, const Vector2i &p_coords, bool p_use_proxies = false) const;

	// Patterns.
	Ref<TileMapPattern> get_pattern(int p_layer, TypedArray<Vector2i> p_coords_array);
	Vector2i map_pattern(const Vector2i &p_position_in_tilemap, const Vector2i &p_coords_in_pattern, Ref<TileMapPattern> p_pattern);
	void set_pattern(int p_layer, const Vector2i &p_position, const Ref<TileMapPattern> p_pattern);

	// Terrains (Not exposed).
	HashMap<Vector2i, TileSet::TerrainsPattern> terrain_fill_constraints(int p_layer, const Vector<Vector2i> &p_to_replace, int p_terrain_set, const RBSet<TerrainConstraint> &p_constraints);
	HashMap<Vector2i, TileSet::TerrainsPattern> terrain_fill_connect(int p_layer, const Vector<Vector2i> &p_coords_array, int p_terrain_set, int p_terrain, bool p_ignore_empty_terrains = true);
	HashMap<Vector2i, TileSet::TerrainsPattern> terrain_fill_path(int p_layer, const Vector<Vector2i> &p_coords_array, int p_terrain_set, int p_terrain, bool p_ignore_empty_terrains = true);
	HashMap<Vector2i, TileSet::TerrainsPattern> terrain_fill_pattern(int p_layer, const Vector<Vector2i> &p_coords_array, int p_terrain_set, TileSet::TerrainsPattern p_terrains_pattern, bool p_ignore_empty_terrains = true);

	// Terrains (exposed).
	void set_cells_terrain_connect(int p_layer, TypedArray<Vector2i> p_cells, int p_terrain_set, int p_terrain, bool p_ignore_empty_terrains = true);
	void set_cells_terrain_path(int p_layer, TypedArray<Vector2i> p_path, int p_terrain_set, int p_terrain, bool p_ignore_empty_terrains = true);

	// Not exposed to users.
	TileMapCell get_cell(int p_layer, const Vector2i &p_coords, bool p_use_proxies = false) const;
	int get_effective_quadrant_size(int p_layer) const;

	virtual void set_y_sort_enabled(bool p_enable) override;

	Vector2 map_to_local(const Vector2i &p_pos) const;
	Vector2i local_to_map(const Vector2 &p_pos) const;
	bool is_existing_neighbor(TileSet::CellNeighbor p_cell_neighbor) const;
	Vector2i get_neighbor_cell(const Vector2i &p_coords, TileSet::CellNeighbor p_cell_neighbor) const;

	TypedArray<Vector2i> get_used_cells(int p_layer) const;
	TypedArray<Vector2i> get_used_cells_by_id(int p_layer, int p_source_id = TileSet::INVALID_SOURCE, const Vector2i p_atlas_coords = TileSetSource::INVALID_ATLAS_COORDS, int p_alternative_tile = TileSetSource::INVALID_TILE_ALTERNATIVE) const;
	Rect2i get_used_rect() const;

	// Override some methods of the CanvasItem class to pass the changes to the quadrants CanvasItems.
	virtual void set_light_mask(int p_light_mask) override;
	virtual void set_self_modulate(const Color &p_self_modulate) override;
	virtual void set_texture_filter(CanvasItem::TextureFilter p_texture_filter) override;
	virtual void set_texture_repeat(CanvasItem::TextureRepeat p_texture_repeat) override;

	// For finding tiles from collision.
	Vector2i get_coords_for_body_rid(RID p_physics_body);
	// For getting their layers as well.
	int get_layer_for_body_rid(RID p_physics_body);

	// Fixing and clearing methods.
	void fix_invalid_tiles();

#ifdef TOOLS_ENABLED
	// Moving layers outside of TileMap.
	TileMapLayer *duplicate_layer_from_internal(int p_layer);
#endif // TOOLS_ENABLED

	// Clears tiles from a given layer.
	void clear_layer(int p_layer);
	void clear();

	// Force a TileMap update.
	void update_internals();
	void notify_runtime_tile_data_update(int p_layer = -1);

	// Helpers?
	TypedArray<Vector2i> get_surrounding_cells(const Vector2i &p_coords);

	// Virtual function to modify the TileData at runtime.
	GDVIRTUAL2R(bool, _use_tile_data_runtime_update, int, Vector2i);
	GDVIRTUAL3(_tile_data_runtime_update, int, Vector2i, TileData *);

	// Configuration warnings.
	PackedStringArray get_configuration_warnings() const override;

private:
	static Callable _navmesh_source_geometry_parsing_callback;
	static RID _navmesh_source_geometry_parser;

public:
	static void navmesh_parse_init();
	static void navmesh_parse_source_geometry(const Ref<NavigationPolygon> &p_navigation_mesh, Ref<NavigationMeshSourceGeometryData2D> p_source_geometry_data, Node *p_node);

	TileMap();
};

VARIANT_ENUM_CAST(TileMap::VisibilityMode);

#endif // TILE_MAP_H

/**************************************************************************/
/*  tile_set.h                                                            */
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

#ifndef TILE_SET_H
#define TILE_SET_H

#include "core/io/resource.h"
#include "core/object/object.h"
#include "core/templates/local_vector.h"
#include "core/templates/rb_set.h"
#include "scene/2d/light_occluder_2d.h"
#include "scene/main/canvas_item.h"
#include "scene/resources/2d/convex_polygon_shape_2d.h"
#include "scene/resources/2d/navigation_polygon.h"
#include "scene/resources/image_texture.h"
#include "scene/resources/packed_scene.h"
#include "scene/resources/physics_material.h"

#ifndef DISABLE_DEPRECATED
#include "scene/resources/shader.h"
#endif

class TileMap;
class TileSetSource;
class TileSetAtlasSource;
class TileData;

// Forward-declare the plugins.
class TileSetPlugin;
class TileSetPluginAtlasRendering;
class TileSetPluginAtlasPhysics;
class TileSetPluginAtlasNavigation;

union TileMapCell {
	struct {
		int16_t source_id;
		int16_t coord_x;
		int16_t coord_y;
		int16_t alternative_tile;
	};

	uint64_t _u64t;

	static uint32_t hash(const TileMapCell &p_hash) {
		return hash_one_uint64(p_hash._u64t);
	}

	TileMapCell(int p_source_id = -1, Vector2i p_atlas_coords = Vector2i(-1, -1), int p_alternative_tile = -1) { // default are INVALID_SOURCE, INVALID_ATLAS_COORDS, INVALID_TILE_ALTERNATIVE
		source_id = p_source_id;
		set_atlas_coords(p_atlas_coords);
		alternative_tile = p_alternative_tile;
	}

	Vector2i get_atlas_coords() const {
		return Vector2i(coord_x, coord_y);
	}

	void set_atlas_coords(const Vector2i &r_coords) {
		coord_x = r_coords.x;
		coord_y = r_coords.y;
	}

	bool operator<(const TileMapCell &p_other) const {
		if (source_id == p_other.source_id) {
			if (coord_x == p_other.coord_x) {
				if (coord_y == p_other.coord_y) {
					return alternative_tile < p_other.alternative_tile;
				} else {
					return coord_y < p_other.coord_y;
				}
			} else {
				return coord_x < p_other.coord_x;
			}
		} else {
			return source_id < p_other.source_id;
		}
	}

	bool operator!=(const TileMapCell &p_other) const {
		return !(source_id == p_other.source_id && coord_x == p_other.coord_x && coord_y == p_other.coord_y && alternative_tile == p_other.alternative_tile);
	}
	bool operator==(const TileMapCell &p_other) const {
		return source_id == p_other.source_id && coord_x == p_other.coord_x && coord_y == p_other.coord_y && alternative_tile == p_other.alternative_tile;
	}
};

class TileMapPattern : public Resource {
	GDCLASS(TileMapPattern, Resource);

	Size2i size;
	HashMap<Vector2i, TileMapCell> pattern;

	void _set_tile_data(const Vector<int> &p_data);
	Vector<int> _get_tile_data() const;

protected:
	bool _set(const StringName &p_name, const Variant &p_value);
	bool _get(const StringName &p_name, Variant &r_ret) const;
	void _get_property_list(List<PropertyInfo> *p_list) const;

	static void _bind_methods();

public:
	void set_cell(const Vector2i &p_coords, int p_source_id, const Vector2i p_atlas_coords, int p_alternative_tile = 0);
	bool has_cell(const Vector2i &p_coords) const;
	void remove_cell(const Vector2i &p_coords, bool p_update_size = true);
	int get_cell_source_id(const Vector2i &p_coords) const;
	Vector2i get_cell_atlas_coords(const Vector2i &p_coords) const;
	int get_cell_alternative_tile(const Vector2i &p_coords) const;

	const HashMap<Vector2i, TileMapCell> &get_pattern() const { return pattern; }
	TypedArray<Vector2i> get_used_cells() const;

	Size2i get_size() const;
	void set_size(const Size2i &p_size);
	bool is_empty() const;

	void clear();
};

class TileSet : public Resource {
	GDCLASS(TileSet, Resource);

#ifndef DISABLE_DEPRECATED
private:
	struct CompatibilityShapeData {
		Vector2i autotile_coords;
		bool one_way;
		float one_way_margin;
		Ref<Shape2D> shape;
		Transform2D transform;
	};

	struct CompatibilityTileData {
		String name;
		Ref<Texture2D> texture;
		Vector2 tex_offset;
		Ref<Material> material;
		Rect2 region;
		int tile_mode = 0;
		Color modulate = Color(1, 1, 1);

		// Atlas or autotiles data
		int autotile_bitmask_mode = 0;
		Vector2 autotile_icon_coordinate;
		Size2i autotile_tile_size = Size2i(16, 16);

		int autotile_spacing = 0;
		HashMap<Vector2i, int> autotile_bitmask_flags;
		HashMap<Vector2i, Ref<OccluderPolygon2D>> autotile_occluder_map;
		HashMap<Vector2i, Ref<NavigationPolygon>> autotile_navpoly_map;
		HashMap<Vector2i, int> autotile_priority_map;
		HashMap<Vector2i, int> autotile_z_index_map;

		Vector<CompatibilityShapeData> shapes;
		Ref<OccluderPolygon2D> occluder;
		Vector2 occluder_offset;
		Ref<NavigationPolygon> navigation;
		Vector2 navigation_offset;
		int z_index = 0;
	};

	enum CompatibilityTileMode {
		COMPATIBILITY_TILE_MODE_SINGLE_TILE = 0,
		COMPATIBILITY_TILE_MODE_AUTO_TILE,
		COMPATIBILITY_TILE_MODE_ATLAS_TILE,
	};

	HashMap<int, CompatibilityTileData *> compatibility_data;
	HashMap<int, int> compatibility_tilemap_mapping_tile_modes;
	HashMap<int, RBMap<Array, Array>> compatibility_tilemap_mapping;
	HashMap<Vector2i, int> compatibility_size_count;

	void _compatibility_conversion();

public:
	// Format of output array [source_id, atlas_coords, alternative]
	Array compatibility_tilemap_map(int p_tile_id, Vector2i p_coords, bool p_flip_h, bool p_flip_v, bool p_transpose);
#endif // DISABLE_DEPRECATED

public:
	static const int INVALID_SOURCE; // -1;

	enum CellNeighbor {
		CELL_NEIGHBOR_RIGHT_SIDE = 0,
		CELL_NEIGHBOR_RIGHT_CORNER,
		CELL_NEIGHBOR_BOTTOM_RIGHT_SIDE,
		CELL_NEIGHBOR_BOTTOM_RIGHT_CORNER,
		CELL_NEIGHBOR_BOTTOM_SIDE,
		CELL_NEIGHBOR_BOTTOM_CORNER,
		CELL_NEIGHBOR_BOTTOM_LEFT_SIDE,
		CELL_NEIGHBOR_BOTTOM_LEFT_CORNER,
		CELL_NEIGHBOR_LEFT_SIDE,
		CELL_NEIGHBOR_LEFT_CORNER,
		CELL_NEIGHBOR_TOP_LEFT_SIDE,
		CELL_NEIGHBOR_TOP_LEFT_CORNER,
		CELL_NEIGHBOR_TOP_SIDE,
		CELL_NEIGHBOR_TOP_CORNER,
		CELL_NEIGHBOR_TOP_RIGHT_SIDE,
		CELL_NEIGHBOR_TOP_RIGHT_CORNER,
		CELL_NEIGHBOR_MAX,
	};

	static const char *CELL_NEIGHBOR_ENUM_TO_TEXT[];

	enum TerrainMode {
		TERRAIN_MODE_MATCH_CORNERS_AND_SIDES = 0,
		TERRAIN_MODE_MATCH_CORNERS,
		TERRAIN_MODE_MATCH_SIDES,
	};

	enum TileShape {
		TILE_SHAPE_SQUARE,
		TILE_SHAPE_ISOMETRIC,
		TILE_SHAPE_HALF_OFFSET_SQUARE,
		TILE_SHAPE_HEXAGON,
	};

	enum TileLayout {
		TILE_LAYOUT_STACKED,
		TILE_LAYOUT_STACKED_OFFSET,
		TILE_LAYOUT_STAIRS_RIGHT,
		TILE_LAYOUT_STAIRS_DOWN,
		TILE_LAYOUT_DIAMOND_RIGHT,
		TILE_LAYOUT_DIAMOND_DOWN,
	};

	enum TileOffsetAxis {
		TILE_OFFSET_AXIS_HORIZONTAL,
		TILE_OFFSET_AXIS_VERTICAL,
	};

	struct PackedSceneSource {
		Ref<PackedScene> scene;
		Vector2 offset;
	};

	class TerrainsPattern {
		bool valid = false;
		int terrain = -1;
		int bits[TileSet::CELL_NEIGHBOR_MAX] = {};
		bool is_valid_bit[TileSet::CELL_NEIGHBOR_MAX] = {};

		int not_empty_terrains_count = 0;

	public:
		bool is_valid() const;
		bool is_erase_pattern() const;

		bool operator<(const TerrainsPattern &p_terrains_pattern) const;
		bool operator==(const TerrainsPattern &p_terrains_pattern) const;
		bool operator!=(const TerrainsPattern &p_terrains_pattern) const {
			return !operator==(p_terrains_pattern);
		}

		void set_terrain(int p_terrain);
		int get_terrain() const;

		void set_terrain_peering_bit(TileSet::CellNeighbor p_peering_bit, int p_terrain);
		int get_terrain_peering_bit(TileSet::CellNeighbor p_peering_bit) const;

		void from_array(Array p_terrains);
		Array as_array() const;

		TerrainsPattern(const TileSet *p_tile_set, int p_terrain_set);
		TerrainsPattern() {}
	};

protected:
	bool _set(const StringName &p_name, const Variant &p_value);
	bool _get(const StringName &p_name, Variant &r_ret) const;
	void _get_property_list(List<PropertyInfo> *p_list) const;
	void _validate_property(PropertyInfo &p_property) const;

#ifdef TOOLS_ENABLED
	virtual uint32_t hash_edited_version_for_preview() const override { return 0; } // Not using preview, so disable it for performance.
#endif

private:
	// --- TileSet data ---
	// Basic shape and layout.
	TileShape tile_shape = TILE_SHAPE_SQUARE;
	TileLayout tile_layout = TILE_LAYOUT_STACKED;
	TileOffsetAxis tile_offset_axis = TILE_OFFSET_AXIS_HORIZONTAL;
	Size2i tile_size = Size2i(16, 16); //Size2(64, 64);

	// Rendering.
	bool uv_clipping = false;
	struct OcclusionLayer {
		uint32_t light_mask = 1;
		bool sdf_collision = false;
	};
	Vector<OcclusionLayer> occlusion_layers;

	Ref<ArrayMesh> tile_lines_mesh;
	Ref<ArrayMesh> tile_filled_mesh;
	mutable bool tile_meshes_dirty = true;

	// Physics
	struct PhysicsLayer {
		uint32_t collision_layer = 1;
		uint32_t collision_mask = 1;
		real_t collision_priority = 1.0;
		Ref<PhysicsMaterial> physics_material;
	};
	Vector<PhysicsLayer> physics_layers;

	// Terrains
	struct Terrain {
		String name;
		Color color;
	};
	struct TerrainSet {
		TerrainMode mode = TERRAIN_MODE_MATCH_CORNERS_AND_SIDES;
		Vector<Terrain> terrains;
	};
	Vector<TerrainSet> terrain_sets;

	HashMap<TerrainMode, Ref<ArrayMesh>> terrain_meshes;
	HashMap<TerrainMode, HashMap<CellNeighbor, Ref<ArrayMesh>>> terrain_peering_bits_meshes;
	bool terrain_bits_meshes_dirty = true;

	LocalVector<RBMap<TileSet::TerrainsPattern, RBSet<TileMapCell>>> per_terrain_pattern_tiles; // Cached data.
	bool terrains_cache_dirty = true;
	void _update_terrains_cache();

	// Navigation
	struct NavigationLayer {
		uint32_t layers = 1;
	};
	Vector<NavigationLayer> navigation_layers;

	// CustomData
	struct CustomDataLayer {
		String name;
		Variant::Type type = Variant::NIL;
	};
	Vector<CustomDataLayer> custom_data_layers;
	HashMap<String, int> custom_data_layers_by_name;

	// Per Atlas source data.
	HashMap<int, Ref<TileSetSource>> sources;
	Vector<int> source_ids;
	int next_source_id = 0;
	// ---------------------

	LocalVector<Ref<TileMapPattern>> patterns;

	void _compute_next_source_id();
	void _source_changed();

	// Tile proxies
	RBMap<int, int> source_level_proxies;
	RBMap<Array, Array> coords_level_proxies;
	RBMap<Array, Array> alternative_level_proxies;

	// Helpers
	Vector<Point2> _get_square_terrain_polygon(Vector2i p_size);
	Vector<Point2> _get_square_corner_or_side_terrain_peering_bit_polygon(Vector2i p_size, TileSet::CellNeighbor p_bit);
	Vector<Point2> _get_square_corner_terrain_peering_bit_polygon(Vector2i p_size, TileSet::CellNeighbor p_bit);
	Vector<Point2> _get_square_side_terrain_peering_bit_polygon(Vector2i p_size, TileSet::CellNeighbor p_bit);

	Vector<Point2> _get_isometric_terrain_polygon(Vector2i p_size);
	Vector<Point2> _get_isometric_corner_or_side_terrain_peering_bit_polygon(Vector2i p_size, TileSet::CellNeighbor p_bit);
	Vector<Point2> _get_isometric_corner_terrain_peering_bit_polygon(Vector2i p_size, TileSet::CellNeighbor p_bit);
	Vector<Point2> _get_isometric_side_terrain_peering_bit_polygon(Vector2i p_size, TileSet::CellNeighbor p_bit);

	Vector<Point2> _get_half_offset_terrain_polygon(Vector2i p_size, float p_overlap, TileSet::TileOffsetAxis p_offset_axis);
	Vector<Point2> _get_half_offset_corner_or_side_terrain_peering_bit_polygon(Vector2i p_size, float p_overlap, TileSet::TileOffsetAxis p_offset_axis, TileSet::CellNeighbor p_bit);
	Vector<Point2> _get_half_offset_corner_terrain_peering_bit_polygon(Vector2i p_size, float p_overlap, TileSet::TileOffsetAxis p_offset_axis, TileSet::CellNeighbor p_bit);
	Vector<Point2> _get_half_offset_side_terrain_peering_bit_polygon(Vector2i p_size, float p_overlap, TileSet::TileOffsetAxis p_offset_axis, TileSet::CellNeighbor p_bit);

protected:
	static void _bind_methods();

public:
	// --- Plugins ---
	Vector<TileSetPlugin *> get_tile_set_atlas_plugins() const;

	// --- Accessors for TileSet data ---

	// -- Shape and layout --
	void set_tile_shape(TileShape p_shape);
	TileShape get_tile_shape() const;
	void set_tile_layout(TileLayout p_layout);
	TileLayout get_tile_layout() const;
	void set_tile_offset_axis(TileOffsetAxis p_alignment);
	TileOffsetAxis get_tile_offset_axis() const;
	void set_tile_size(Size2i p_size);
	Size2i get_tile_size() const;

	// -- Sources management --
	int get_next_source_id() const;
	int get_source_count() const;
	int get_source_id(int p_index) const;
	int add_source(Ref<TileSetSource> p_tile_set_source, int p_source_id_override = -1);
	void set_source_id(int p_source_id, int p_new_id);
	void remove_source(int p_source_id);
	void remove_source_ptr(TileSetSource *p_tile_set_source); // Not exposed
	bool has_source(int p_source_id) const;
	Ref<TileSetSource> get_source(int p_source_id) const;

	// Rendering
	void set_uv_clipping(bool p_uv_clipping);
	bool is_uv_clipping() const;

	int get_occlusion_layers_count() const;
	void add_occlusion_layer(int p_index = -1);
	void move_occlusion_layer(int p_from_index, int p_to_pos);
	void remove_occlusion_layer(int p_index);
	void set_occlusion_layer_light_mask(int p_layer_index, int p_light_mask);
	int get_occlusion_layer_light_mask(int p_layer_index) const;
	void set_occlusion_layer_sdf_collision(int p_layer_index, bool p_sdf_collision);
	bool get_occlusion_layer_sdf_collision(int p_layer_index) const;

	// Physics
	int get_physics_layers_count() const;
	void add_physics_layer(int p_index = -1);
	void move_physics_layer(int p_from_index, int p_to_pos);
	void remove_physics_layer(int p_index);
	void set_physics_layer_collision_layer(int p_layer_index, uint32_t p_layer);
	uint32_t get_physics_layer_collision_layer(int p_layer_index) const;
	void set_physics_layer_collision_mask(int p_layer_index, uint32_t p_mask);
	uint32_t get_physics_layer_collision_mask(int p_layer_index) const;
	void set_physics_layer_collision_priority(int p_layer_index, real_t p_priority);
	real_t get_physics_layer_collision_priority(int p_layer_index) const;
	void set_physics_layer_physics_material(int p_layer_index, Ref<PhysicsMaterial> p_physics_material);
	Ref<PhysicsMaterial> get_physics_layer_physics_material(int p_layer_index) const;

	// Terrain sets
	int get_terrain_sets_count() const;
	void add_terrain_set(int p_index = -1);
	void move_terrain_set(int p_from_index, int p_to_pos);
	void remove_terrain_set(int p_index);
	void set_terrain_set_mode(int p_terrain_set, TerrainMode p_terrain_mode);
	TerrainMode get_terrain_set_mode(int p_terrain_set) const;

	// Terrains
	int get_terrains_count(int p_terrain_set) const;
	void add_terrain(int p_terrain_set, int p_index = -1);
	void move_terrain(int p_terrain_set, int p_from_index, int p_to_pos);
	void remove_terrain(int p_terrain_set, int p_index);
	void set_terrain_name(int p_terrain_set, int p_terrain_index, String p_name);
	String get_terrain_name(int p_terrain_set, int p_terrain_index) const;
	void set_terrain_color(int p_terrain_set, int p_terrain_index, Color p_color);
	Color get_terrain_color(int p_terrain_set, int p_terrain_index) const;
	bool is_valid_terrain_peering_bit_for_mode(TileSet::TerrainMode p_terrain_mode, TileSet::CellNeighbor p_peering_bit) const;
	bool is_valid_terrain_peering_bit(int p_terrain_set, TileSet::CellNeighbor p_peering_bit) const;

	// Navigation
	int get_navigation_layers_count() const;
	void add_navigation_layer(int p_index = -1);
	void move_navigation_layer(int p_from_index, int p_to_pos);
	void remove_navigation_layer(int p_index);
	void set_navigation_layer_layers(int p_layer_index, uint32_t p_layers);
	uint32_t get_navigation_layer_layers(int p_layer_index) const;
	void set_navigation_layer_layer_value(int p_layer_index, int p_layer_number, bool p_value);
	bool get_navigation_layer_layer_value(int p_layer_index, int p_layer_number) const;

	// Custom data
	int get_custom_data_layers_count() const;
	void add_custom_data_layer(int p_index = -1);
	void move_custom_data_layer(int p_from_index, int p_to_pos);
	void remove_custom_data_layer(int p_index);
	int get_custom_data_layer_by_name(String p_value) const;
	void set_custom_data_layer_name(int p_layer_id, String p_value);
	bool has_custom_data_layer_by_name(const String &p_value) const;
	String get_custom_data_layer_name(int p_layer_id) const;
	void set_custom_data_layer_type(int p_layer_id, Variant::Type p_value);
	Variant::Type get_custom_data_layer_type(int p_layer_id) const;

	// Tiles proxies.
	void set_source_level_tile_proxy(int p_source_from, int p_source_to);
	int get_source_level_tile_proxy(int p_source_from);
	bool has_source_level_tile_proxy(int p_source_from);
	void remove_source_level_tile_proxy(int p_source_from);

	void set_coords_level_tile_proxy(int p_source_from, Vector2i p_coords_from, int p_source_to, Vector2i p_coords_to);
	Array get_coords_level_tile_proxy(int p_source_from, Vector2i p_coords_from);
	bool has_coords_level_tile_proxy(int p_source_from, Vector2i p_coords_from);
	void remove_coords_level_tile_proxy(int p_source_from, Vector2i p_coords_from);

	void set_alternative_level_tile_proxy(int p_source_from, Vector2i p_coords_from, int p_alternative_from, int p_source_to, Vector2i p_coords_to, int p_alternative_to);
	Array get_alternative_level_tile_proxy(int p_source_from, Vector2i p_coords_from, int p_alternative_from);
	bool has_alternative_level_tile_proxy(int p_source_from, Vector2i p_coords_from, int p_alternative_from);
	void remove_alternative_level_tile_proxy(int p_source_from, Vector2i p_coords_from, int p_alternative_from);

	Array get_source_level_tile_proxies() const;
	Array get_coords_level_tile_proxies() const;
	Array get_alternative_level_tile_proxies() const;

	Array map_tile_proxy(int p_source_from, Vector2i p_coords_from, int p_alternative_from) const;

	void cleanup_invalid_tile_proxies();
	void clear_tile_proxies();

	// Patterns.
	int add_pattern(Ref<TileMapPattern> p_pattern, int p_index = -1);
	Ref<TileMapPattern> get_pattern(int p_index);
	void remove_pattern(int p_index);
	int get_patterns_count();

	// Terrains.
	RBSet<TerrainsPattern> get_terrains_pattern_set(int p_terrain_set);
	RBSet<TileMapCell> get_tiles_for_terrains_pattern(int p_terrain_set, TerrainsPattern p_terrain_tile_pattern);
	TileMapCell get_random_tile_from_terrains_pattern(int p_terrain_set, TerrainsPattern p_terrain_tile_pattern);

	// Helpers
	Vector<Vector2> get_tile_shape_polygon() const;
	void draw_tile_shape(CanvasItem *p_canvas_item, Transform2D p_transform, Color p_color, bool p_filled = false, Ref<Texture2D> p_texture = Ref<Texture2D>()) const;

	// Used by TileMap/TileMapLayer
	Vector2 map_to_local(const Vector2i &p_pos) const;
	Vector2i local_to_map(const Vector2 &p_pos) const;
	bool is_existing_neighbor(TileSet::CellNeighbor p_cell_neighbor) const;
	Vector2i get_neighbor_cell(const Vector2i &p_coords, TileSet::CellNeighbor p_cell_neighbor) const;
	TypedArray<Vector2i> get_surrounding_cells(const Vector2i &p_coords) const;
	Vector2i map_pattern(const Vector2i &p_position_in_tilemap, const Vector2i &p_coords_in_pattern, Ref<TileMapPattern> p_pattern) const;
	void draw_cells_outline(CanvasItem *p_canvas_item, const RBSet<Vector2i> &p_cells, Color p_color, Transform2D p_transform = Transform2D()) const;

	Vector<Point2> get_terrain_polygon(int p_terrain_set);
	Vector<Point2> get_terrain_peering_bit_polygon(int p_terrain_set, TileSet::CellNeighbor p_bit);
	void draw_terrains(CanvasItem *p_canvas_item, Transform2D p_transform, const TileData *p_tile_data);
	Vector<Vector<Ref<Texture2D>>> generate_terrains_icons(Size2i p_size);

	// Resource management
	virtual void reset_state() override;

	// Helpers.
	static Vector2i transform_coords_layout(const Vector2i &p_coords, TileSet::TileOffsetAxis p_offset_axis, TileSet::TileLayout p_from_layout, TileSet::TileLayout p_to_layout);

	TileSet();
	~TileSet();
};

class TileSetSource : public Resource {
	GDCLASS(TileSetSource, Resource);

protected:
	const TileSet *tile_set = nullptr;

	static void _bind_methods();

public:
	static const Vector2i INVALID_ATLAS_COORDS; // Vector2i(-1, -1);
	static const int INVALID_TILE_ALTERNATIVE; // -1;

	// Not exposed.
	virtual void set_tile_set(const TileSet *p_tile_set);
	TileSet *get_tile_set() const;
	virtual void notify_tile_data_properties_should_change() {}
	virtual void add_occlusion_layer(int p_index) {}
	virtual void move_occlusion_layer(int p_from_index, int p_to_pos) {}
	virtual void remove_occlusion_layer(int p_index) {}
	virtual void add_physics_layer(int p_index) {}
	virtual void move_physics_layer(int p_from_index, int p_to_pos) {}
	virtual void remove_physics_layer(int p_index) {}
	virtual void add_terrain_set(int p_index) {}
	virtual void move_terrain_set(int p_from_index, int p_to_pos) {}
	virtual void remove_terrain_set(int p_index) {}
	virtual void add_terrain(int p_terrain_set, int p_index) {}
	virtual void move_terrain(int p_terrain_set, int p_from_index, int p_to_pos) {}
	virtual void remove_terrain(int p_terrain_set, int p_index) {}
	virtual void add_navigation_layer(int p_index) {}
	virtual void move_navigation_layer(int p_from_index, int p_to_pos) {}
	virtual void remove_navigation_layer(int p_index) {}
	virtual void add_custom_data_layer(int p_index) {}
	virtual void move_custom_data_layer(int p_from_index, int p_to_pos) {}
	virtual void remove_custom_data_layer(int p_index) {}
	virtual void reset_state() override;

	// Tiles.
	virtual int get_tiles_count() const = 0;
	virtual Vector2i get_tile_id(int tile_index) const = 0;
	virtual bool has_tile(Vector2i p_atlas_coords) const = 0;

	// Alternative tiles.
	virtual int get_alternative_tiles_count(const Vector2i p_atlas_coords) const = 0;
	virtual int get_alternative_tile_id(const Vector2i p_atlas_coords, int p_index) const = 0;
	virtual bool has_alternative_tile(const Vector2i p_atlas_coords, int p_alternative_tile) const = 0;
};

class TileSetAtlasSource : public TileSetSource {
	GDCLASS(TileSetAtlasSource, TileSetSource);

public:
	enum TileAnimationMode {
		TILE_ANIMATION_MODE_DEFAULT,
		TILE_ANIMATION_MODE_RANDOM_START_TIMES,
		TILE_ANIMATION_MODE_MAX,
	};

	enum TransformBits {
		TRANSFORM_FLIP_H = 1 << 12,
		TRANSFORM_FLIP_V = 1 << 13,
		TRANSFORM_TRANSPOSE = 1 << 14,
	};

	static const int16_t UNTRANSFORM_MASK = ~(TileSetAtlasSource::TRANSFORM_FLIP_H + TileSetAtlasSource::TRANSFORM_FLIP_V + TileSetAtlasSource::TRANSFORM_TRANSPOSE);

private:
	struct TileAlternativesData {
		Vector2i size_in_atlas = Vector2i(1, 1);
		Vector2i texture_offset;

		// Animation
		int animation_columns = 0;
		Vector2i animation_separation;
		real_t animation_speed = 1.0;
		TileSetAtlasSource::TileAnimationMode animation_mode = TILE_ANIMATION_MODE_DEFAULT;
		LocalVector<real_t> animation_frames_durations;

		// Alternatives
		HashMap<int, TileData *> alternatives;
		Vector<int> alternatives_ids;
		int next_alternative_id = 1;
	};

	bool initializing = true;

	Ref<Texture2D> texture;
	Vector2i margins;
	Vector2i separation;
	Size2i texture_region_size = Size2i(16, 16);

	HashMap<Vector2i, TileAlternativesData> tiles;
	Vector<Vector2i> tiles_ids;
	HashMap<Vector2i, Vector2i> _coords_mapping_cache; // Maps any coordinate to the including tile

	TileData *_get_atlas_tile_data(Vector2i p_atlas_coords, int p_alternative_tile);
	const TileData *_get_atlas_tile_data(Vector2i p_atlas_coords, int p_alternative_tile) const;

	void _compute_next_alternative_id(const Vector2i p_atlas_coords);

	void _clear_coords_mapping_cache(Vector2i p_atlas_coords);
	void _create_coords_mapping_cache(Vector2i p_atlas_coords);

	bool use_texture_padding = true;
	Ref<CanvasTexture> padded_texture;
	bool padded_texture_needs_update = false;
	void _queue_update_padded_texture();
	Ref<ImageTexture> _create_padded_image_texture(const Ref<Texture2D> &p_source);
	void _update_padded_texture();
	void _try_emit_changed();

protected:
	bool _set(const StringName &p_name, const Variant &p_value);
	bool _get(const StringName &p_name, Variant &r_ret) const;
	void _get_property_list(List<PropertyInfo> *p_list) const;

	void _notification(int p_notification);
	static void _bind_methods();

public:
	// Not exposed.
	virtual void set_tile_set(const TileSet *p_tile_set) override;
	const TileSet *get_tile_set() const;
	virtual void notify_tile_data_properties_should_change() override;
	virtual void add_occlusion_layer(int p_index) override;
	virtual void move_occlusion_layer(int p_from_index, int p_to_pos) override;
	virtual void remove_occlusion_layer(int p_index) override;
	virtual void add_physics_layer(int p_index) override;
	virtual void move_physics_layer(int p_from_index, int p_to_pos) override;
	virtual void remove_physics_layer(int p_index) override;
	virtual void add_terrain_set(int p_index) override;
	virtual void move_terrain_set(int p_from_index, int p_to_pos) override;
	virtual void remove_terrain_set(int p_index) override;
	virtual void add_terrain(int p_terrain_set, int p_index) override;
	virtual void move_terrain(int p_terrain_set, int p_from_index, int p_to_pos) override;
	virtual void remove_terrain(int p_terrain_set, int p_index) override;
	virtual void add_navigation_layer(int p_index) override;
	virtual void move_navigation_layer(int p_from_index, int p_to_pos) override;
	virtual void remove_navigation_layer(int p_index) override;
	virtual void add_custom_data_layer(int p_index) override;
	virtual void move_custom_data_layer(int p_from_index, int p_to_pos) override;
	virtual void remove_custom_data_layer(int p_index) override;
	virtual void reset_state() override;

	// Base properties.
	void set_texture(Ref<Texture2D> p_texture);
	Ref<Texture2D> get_texture() const;
	void set_margins(Vector2i p_margins);
	Vector2i get_margins() const;
	void set_separation(Vector2i p_separation);
	Vector2i get_separation() const;
	void set_texture_region_size(Vector2i p_tile_size);
	Vector2i get_texture_region_size() const;

	// Padding.
	void set_use_texture_padding(bool p_use_padding);
	bool get_use_texture_padding() const;

	// Base tiles.
	void create_tile(const Vector2i p_atlas_coords, const Vector2i p_size = Vector2i(1, 1));
	void remove_tile(Vector2i p_atlas_coords);
	virtual bool has_tile(Vector2i p_atlas_coords) const override;
	void move_tile_in_atlas(Vector2i p_atlas_coords, Vector2i p_new_atlas_coords = INVALID_ATLAS_COORDS, Vector2i p_new_size = Vector2i(-1, -1));
	Vector2i get_tile_size_in_atlas(Vector2i p_atlas_coords) const;

	virtual int get_tiles_count() const override;
	virtual Vector2i get_tile_id(int p_index) const override;

	bool has_room_for_tile(Vector2i p_atlas_coords, Vector2i p_size, int p_animation_columns, Vector2i p_animation_separation, int p_frames_count, Vector2i p_ignored_tile = INVALID_ATLAS_COORDS) const;
	PackedVector2Array get_tiles_to_be_removed_on_change(Ref<Texture2D> p_texture, Vector2i p_margins, Vector2i p_separation, Vector2i p_texture_region_size);
	Vector2i get_tile_at_coords(Vector2i p_atlas_coords) const;

	bool has_tiles_outside_texture() const;
	Vector<Vector2i> get_tiles_outside_texture() const;
	void clear_tiles_outside_texture();

	// Animation.
	void set_tile_animation_columns(const Vector2i p_atlas_coords, int p_frame_columns);
	int get_tile_animation_columns(const Vector2i p_atlas_coords) const;
	void set_tile_animation_separation(const Vector2i p_atlas_coords, const Vector2i p_separation);
	Vector2i get_tile_animation_separation(const Vector2i p_atlas_coords) const;
	void set_tile_animation_speed(const Vector2i p_atlas_coords, real_t p_speed);
	real_t get_tile_animation_speed(const Vector2i p_atlas_coords) const;
	void set_tile_animation_mode(const Vector2i p_atlas_coords, const TileSetAtlasSource::TileAnimationMode p_mode);
	TileSetAtlasSource::TileAnimationMode get_tile_animation_mode(const Vector2i p_atlas_coords) const;
	void set_tile_animation_frames_count(const Vector2i p_atlas_coords, int p_frames_count);
	int get_tile_animation_frames_count(const Vector2i p_atlas_coords) const;
	void set_tile_animation_frame_duration(const Vector2i p_atlas_coords, int p_frame_index, real_t p_duration);
	real_t get_tile_animation_frame_duration(const Vector2i p_atlas_coords, int p_frame_index) const;
	real_t get_tile_animation_total_duration(const Vector2i p_atlas_coords) const;

	// Alternative tiles.
	int create_alternative_tile(const Vector2i p_atlas_coords, int p_alternative_id_override = -1);
	void remove_alternative_tile(const Vector2i p_atlas_coords, int p_alternative_tile);
	void set_alternative_tile_id(const Vector2i p_atlas_coords, int p_alternative_tile, int p_new_id);
	virtual bool has_alternative_tile(const Vector2i p_atlas_coords, int p_alternative_tile) const override;
	int get_next_alternative_tile_id(const Vector2i p_atlas_coords) const;

	virtual int get_alternative_tiles_count(const Vector2i p_atlas_coords) const override;
	virtual int get_alternative_tile_id(const Vector2i p_atlas_coords, int p_index) const override;

	// Get data associated to a tile.
	TileData *get_tile_data(const Vector2i p_atlas_coords, int p_alternative_tile) const;

	// Helpers.
	Vector2i get_atlas_grid_size() const;
	Rect2i get_tile_texture_region(Vector2i p_atlas_coords, int p_frame = 0) const;
	bool is_position_in_tile_texture_region(const Vector2i p_atlas_coords, int p_alternative_tile, Vector2 p_position) const;
	bool is_rect_in_tile_texture_region(const Vector2i p_atlas_coords, int p_alternative_tile, Rect2 p_rect) const;

	static int alternative_no_transform(int p_alternative_id);

	// Getters for texture and tile region (padded or not)
	Ref<Texture2D> get_runtime_texture() const;
	Rect2i get_runtime_tile_texture_region(Vector2i p_atlas_coords, int p_frame = 0) const;

	~TileSetAtlasSource();
};

class TileSetScenesCollectionSource : public TileSetSource {
	GDCLASS(TileSetScenesCollectionSource, TileSetSource);

private:
	struct SceneData {
		Ref<PackedScene> scene;
		bool display_placeholder = false;
	};
	Vector<int> scenes_ids;
	HashMap<int, SceneData> scenes;
	int next_scene_id = 1;

	bool initializing = true;

	void _compute_next_alternative_id();
	void _try_emit_changed();

protected:
	bool _set(const StringName &p_name, const Variant &p_value);
	bool _get(const StringName &p_name, Variant &r_ret) const;
	void _get_property_list(List<PropertyInfo> *p_list) const;

	void _notification(int p_notification);
	static void _bind_methods();

public:
	// Tiles.
	int get_tiles_count() const override;
	Vector2i get_tile_id(int p_tile_index) const override;
	bool has_tile(Vector2i p_atlas_coords) const override;

	// Alternative tiles.
	int get_alternative_tiles_count(const Vector2i p_atlas_coords) const override;
	int get_alternative_tile_id(const Vector2i p_atlas_coords, int p_index) const override;
	bool has_alternative_tile(const Vector2i p_atlas_coords, int p_alternative_tile) const override;

	// Scenes accessors. Lot are similar to "Alternative tiles".
	int get_scene_tiles_count() { return get_alternative_tiles_count(Vector2i()); }
	int get_scene_tile_id(int p_index) { return get_alternative_tile_id(Vector2i(), p_index); }
	bool has_scene_tile_id(int p_id) { return has_alternative_tile(Vector2i(), p_id); }
	int create_scene_tile(Ref<PackedScene> p_packed_scene = Ref<PackedScene>(), int p_id_override = -1);
	void set_scene_tile_id(int p_id, int p_new_id);
	void set_scene_tile_scene(int p_id, Ref<PackedScene> p_packed_scene);
	Ref<PackedScene> get_scene_tile_scene(int p_id) const;
	void set_scene_tile_display_placeholder(int p_id, bool p_packed_scene);
	bool get_scene_tile_display_placeholder(int p_id) const;
	void remove_scene_tile(int p_id);
	int get_next_scene_tile_id() const;
};

class TileData : public Object {
	GDCLASS(TileData, Object);

private:
	const TileSet *tile_set = nullptr;
	bool allow_transform = true;

	// Rendering
	bool flip_h = false;
	bool flip_v = false;
	bool transpose = false;
	Vector2i texture_origin;
	Ref<Material> material;
	Color modulate = Color(1.0, 1.0, 1.0, 1.0);
	int z_index = 0;
	int y_sort_origin = 0;
	struct OcclusionLayerTileData {
		struct PolygonOccluderTileData {
			Ref<OccluderPolygon2D> occluder_polygon;
			mutable HashMap<int, Ref<OccluderPolygon2D>> transformed_polygon_occluders;
		};
		Vector<PolygonOccluderTileData> polygons;
	};
	Vector<OcclusionLayerTileData> occluders;

	// Physics
	struct PhysicsLayerTileData {
		struct PolygonShapeTileData {
			LocalVector<Vector2> polygon;
			LocalVector<Ref<ConvexPolygonShape2D>> shapes;
			mutable HashMap<int, LocalVector<Ref<ConvexPolygonShape2D>>> transformed_shapes;
			bool one_way = false;
			float one_way_margin = 1.0;
		};

		Vector2 linear_velocity;
		double angular_velocity = 0.0;
		Vector<PolygonShapeTileData> polygons;
	};
	Vector<PhysicsLayerTileData> physics;
	// TODO add support for areas.

	// Terrain
	int terrain_set = -1;
	int terrain = -1;
	int terrain_peering_bits[16] = { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 };

	// Navigation
	struct NavigationLayerTileData {
		Ref<NavigationPolygon> navigation_polygon;
		mutable HashMap<int, Ref<NavigationPolygon>> transformed_navigation_polygon;
	};
	Vector<NavigationLayerTileData> navigation;

	// Misc
	double probability = 1.0;

	// Custom data
	Vector<Variant> custom_data;

protected:
	bool _set(const StringName &p_name, const Variant &p_value);
	bool _get(const StringName &p_name, Variant &r_ret) const;
	void _get_property_list(List<PropertyInfo> *p_list) const;
	static void _bind_methods();

#ifndef DISABLE_DEPRECATED
	Ref<NavigationPolygon> _get_navigation_polygon_bind_compat_84660(int p_layer_id) const;
	Ref<OccluderPolygon2D> _get_occluder_bind_compat_84660(int p_layer_id) const;

	static void _bind_compatibility_methods();
#endif

public:
	// Not exposed.
	void set_tile_set(const TileSet *p_tile_set);
	void notify_tile_data_properties_should_change();
	void add_occlusion_layer(int p_index);
	void move_occlusion_layer(int p_from_index, int p_to_pos);
	void remove_occlusion_layer(int p_index);
	void add_physics_layer(int p_index);
	void move_physics_layer(int p_from_index, int p_to_pos);
	void remove_physics_layer(int p_index);
	void add_terrain_set(int p_index);
	void move_terrain_set(int p_from_index, int p_to_pos);
	void remove_terrain_set(int p_index);
	void add_terrain(int p_terrain_set, int p_index);
	void move_terrain(int p_terrain_set, int p_from_index, int p_to_pos);
	void remove_terrain(int p_terrain_set, int p_index);
	void add_navigation_layer(int p_index);
	void move_navigation_layer(int p_from_index, int p_to_pos);
	void remove_navigation_layer(int p_index);
	void add_custom_data_layer(int p_index);
	void move_custom_data_layer(int p_from_index, int p_to_pos);
	void remove_custom_data_layer(int p_index);
	void set_allow_transform(bool p_allow_transform);
	bool is_allowing_transform() const;

	// To duplicate a TileData object, needed for runtiume update.
	TileData *duplicate();

	// Rendering
	void set_flip_h(bool p_flip_h);
	bool get_flip_h() const;
	void set_flip_v(bool p_flip_v);
	bool get_flip_v() const;
	void set_transpose(bool p_transpose);
	bool get_transpose() const;

	void set_texture_origin(Vector2i p_texture_origin);
	Vector2i get_texture_origin() const;
	void set_material(Ref<Material> p_material);
	Ref<Material> get_material() const;
	void set_modulate(Color p_modulate);
	Color get_modulate() const;
	void set_z_index(int p_z_index);
	int get_z_index() const;
	void set_y_sort_origin(int p_y_sort_origin);
	int get_y_sort_origin() const;

#ifndef DISABLE_DEPRECATED
	void set_occluder(int p_layer_id, Ref<OccluderPolygon2D> p_occluder_polygon);
	Ref<OccluderPolygon2D> get_occluder(int p_layer_id, bool p_flip_h = false, bool p_flip_v = false, bool p_transpose = false) const;
#endif // DISABLE_DEPRECATED

	void set_occluder_polygons_count(int p_layer_id, int p_polygons_count);
	int get_occluder_polygons_count(int p_layer_id) const;
	void add_occluder_polygon(int p_layer_id);
	void remove_occluder_polygon(int p_layer_id, int p_polygon_index);
	void set_occluder_polygon(int p_layer_id, int p_polygon_index, const Ref<OccluderPolygon2D> &p_occluder_polygon);
	Ref<OccluderPolygon2D> get_occluder_polygon(int p_layer_id, int p_polygon_index, bool p_flip_h = false, bool p_flip_v = false, bool p_transpose = false) const;

	// Physics
	void set_constant_linear_velocity(int p_layer_id, const Vector2 &p_velocity);
	Vector2 get_constant_linear_velocity(int p_layer_id) const;
	void set_constant_angular_velocity(int p_layer_id, real_t p_velocity);
	real_t get_constant_angular_velocity(int p_layer_id) const;
	void set_collision_polygons_count(int p_layer_id, int p_shapes_count);
	int get_collision_polygons_count(int p_layer_id) const;
	void add_collision_polygon(int p_layer_id);
	void remove_collision_polygon(int p_layer_id, int p_polygon_index);
	void set_collision_polygon_points(int p_layer_id, int p_polygon_index, Vector<Vector2> p_polygon);
	Vector<Vector2> get_collision_polygon_points(int p_layer_id, int p_polygon_index) const;
	void set_collision_polygon_one_way(int p_layer_id, int p_polygon_index, bool p_one_way);
	bool is_collision_polygon_one_way(int p_layer_id, int p_polygon_index) const;
	void set_collision_polygon_one_way_margin(int p_layer_id, int p_polygon_index, float p_one_way_margin);
	float get_collision_polygon_one_way_margin(int p_layer_id, int p_polygon_index) const;
	int get_collision_polygon_shapes_count(int p_layer_id, int p_polygon_index) const;
	Ref<ConvexPolygonShape2D> get_collision_polygon_shape(int p_layer_id, int p_polygon_index, int shape_index, bool p_flip_h = false, bool p_flip_v = false, bool p_transpose = false) const;

	// Terrain
	void set_terrain_set(int p_terrain_id);
	int get_terrain_set() const;
	void set_terrain(int p_terrain_id);
	int get_terrain() const;
	void set_terrain_peering_bit(TileSet::CellNeighbor p_peering_bit, int p_terrain_id);
	int get_terrain_peering_bit(TileSet::CellNeighbor p_peering_bit) const;
	bool is_valid_terrain_peering_bit(TileSet::CellNeighbor p_peering_bit) const;

	TileSet::TerrainsPattern get_terrains_pattern() const; // Not exposed.

	// Navigation
	void set_navigation_polygon(int p_layer_id, Ref<NavigationPolygon> p_navigation_polygon);
	Ref<NavigationPolygon> get_navigation_polygon(int p_layer_id, bool p_flip_h = false, bool p_flip_v = false, bool p_transpose = false) const;

	// Misc
	void set_probability(float p_probability);
	float get_probability() const;

	// Custom data.
	void set_custom_data(String p_layer_name, Variant p_value);
	Variant get_custom_data(String p_layer_name) const;
	bool has_custom_data(const String &p_layer_name) const;
	void set_custom_data_by_layer_id(int p_layer_id, Variant p_value);
	Variant get_custom_data_by_layer_id(int p_layer_id) const;

	// Polygons.
	static PackedVector2Array get_transformed_vertices(const PackedVector2Array &p_vertices, bool p_flip_h, bool p_flip_v, bool p_transpose);
};

VARIANT_ENUM_CAST(TileSet::CellNeighbor);
VARIANT_ENUM_CAST(TileSet::TerrainMode);
VARIANT_ENUM_CAST(TileSet::TileShape);
VARIANT_ENUM_CAST(TileSet::TileLayout);
VARIANT_ENUM_CAST(TileSet::TileOffsetAxis);

VARIANT_ENUM_CAST(TileSetAtlasSource::TileAnimationMode);

#endif // TILE_SET_H

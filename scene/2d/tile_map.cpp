/**************************************************************************/
/*  tile_map.cpp                                                          */
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

#include "tile_map.h"
#include "tile_map.compat.inc"

#include "core/io/marshalls.h"

#define TILEMAP_CALL_FOR_LAYER(layer, function, ...) \
	if (layer < 0) {                                 \
		layer = layers.size() + layer;               \
	};                                               \
	ERR_FAIL_INDEX(layer, (int)layers.size());       \
	layers[layer]->function(__VA_ARGS__);

#define TILEMAP_CALL_FOR_LAYER_V(layer, err_value, function, ...) \
	if (layer < 0) {                                              \
		layer = layers.size() + layer;                            \
	};                                                            \
	ERR_FAIL_INDEX_V(layer, (int)layers.size(), err_value);       \
	return layers[layer]->function(__VA_ARGS__);

void TileMap::_tile_set_changed() {
	update_configuration_warnings();
}

void TileMap::_emit_changed() {
	emit_signal(CoreStringName(changed));
}

void TileMap::_set_tile_map_data_using_compatibility_format(int p_layer, TileMapDataFormat p_format, const Vector<int> &p_data) {
	ERR_FAIL_INDEX(p_layer, (int)layers.size());
	ERR_FAIL_COND(p_format >= TileMapDataFormat::TILE_MAP_DATA_FORMAT_MAX);
#ifndef DISABLE_DEPRECATED
	ERR_FAIL_COND_MSG(p_format != (TileMapDataFormat)(TILE_MAP_DATA_FORMAT_MAX - 1), "Old TileMap data format detected despite DISABLE_DEPRECATED being set compilation time.");
#endif // DISABLE_DEPRECATED

	// Set data for a given tile from raw data.
	int c = p_data.size();
	const int *r = p_data.ptr();

	int offset = (p_format >= TileMapDataFormat::TILE_MAP_DATA_FORMAT_2) ? 3 : 2;
	ERR_FAIL_COND_MSG(c % offset != 0, vformat("Corrupted tile data. Got size: %d. Expected modulo: %d", c, offset));

	layers[p_layer]->clear();

	for (int i = 0; i < c; i += offset) {
		const uint8_t *ptr = (const uint8_t *)&r[i];
		uint8_t local[12];
		const int buffer_size = (p_format >= TILE_MAP_DATA_FORMAT_2) ? 12 : 8;
		for (int j = 0; j < buffer_size; j++) {
			local[j] = ptr[j];
		}

#ifdef BIG_ENDIAN_ENABLED
		SWAP(local[0], local[3]);
		SWAP(local[1], local[2]);
		SWAP(local[4], local[7]);
		SWAP(local[5], local[6]);
		//TODO: ask someone to check this...
		if (FORMAT >= FORMAT_2) {
			SWAP(local[8], local[11]);
			SWAP(local[9], local[10]);
		}
#endif // BIG_ENDIAN_ENABLED
	   // Extracts position in TileMap.
		int16_t x = decode_uint16(&local[0]);
		int16_t y = decode_uint16(&local[2]);

		if (p_format == TileMapDataFormat::TILE_MAP_DATA_FORMAT_3) {
			uint16_t source_id = decode_uint16(&local[4]);
			uint16_t atlas_coords_x = decode_uint16(&local[6]);
			uint16_t atlas_coords_y = decode_uint16(&local[8]);
			uint16_t alternative_tile = decode_uint16(&local[10]);
			layers[p_layer]->set_cell(Vector2i(x, y), source_id, Vector2i(atlas_coords_x, atlas_coords_y), alternative_tile);
		} else {
#ifndef DISABLE_DEPRECATED
			// Previous decated format.
			uint32_t v = decode_uint32(&local[4]);
			// Extract the transform flags that used to be in the tilemap.
			bool flip_h = v & (1UL << 29);
			bool flip_v = v & (1UL << 30);
			bool transpose = v & (1UL << 31);
			v &= (1UL << 29) - 1;

			// Extract autotile/atlas coords.
			int16_t coord_x = 0;
			int16_t coord_y = 0;
			if (p_format == TileMapDataFormat::TILE_MAP_DATA_FORMAT_2) {
				coord_x = decode_uint16(&local[8]);
				coord_y = decode_uint16(&local[10]);
			}

			if (tile_set.is_valid()) {
				Array a = tile_set->compatibility_tilemap_map(v, Vector2i(coord_x, coord_y), flip_h, flip_v, transpose);
				if (a.size() == 3) {
					layers[p_layer]->set_cell(Vector2i(x, y), a[0], a[1], a[2]);
				} else {
					ERR_PRINT(vformat("No valid tile in Tileset for: tile:%s coords:%s flip_h:%s flip_v:%s transpose:%s", v, Vector2i(coord_x, coord_y), flip_h, flip_v, transpose));
				}
			} else {
				int compatibility_alternative_tile = ((int)flip_h) + ((int)flip_v << 1) + ((int)transpose << 2);
				layers[p_layer]->set_cell(Vector2i(x, y), v, Vector2i(coord_x, coord_y), compatibility_alternative_tile);
			}
#endif // DISABLE_DEPRECATED
		}
	}
}

Vector<int> TileMap::_get_tile_map_data_using_compatibility_format(int p_layer) const {
	ERR_FAIL_INDEX_V(p_layer, (int)layers.size(), Vector<int>());

	// Export tile data to raw format.
	const HashMap<Vector2i, CellData> tile_map_layer_data = layers[p_layer]->get_tile_map_layer_data();
	Vector<int> tile_data;
	tile_data.resize(tile_map_layer_data.size() * 3);
	int *w = tile_data.ptrw();

	// Save in highest format.

	int idx = 0;
	for (const KeyValue<Vector2i, CellData> &E : tile_map_layer_data) {
		uint8_t *ptr = (uint8_t *)&w[idx];
		encode_uint16((int16_t)(E.key.x), &ptr[0]);
		encode_uint16((int16_t)(E.key.y), &ptr[2]);
		encode_uint16(E.value.cell.source_id, &ptr[4]);
		encode_uint16(E.value.cell.coord_x, &ptr[6]);
		encode_uint16(E.value.cell.coord_y, &ptr[8]);
		encode_uint16(E.value.cell.alternative_tile, &ptr[10]);
		idx += 3;
	}

	return tile_data;
}

void TileMap::_set_layer_tile_data(int p_layer, const PackedInt32Array &p_data) {
	_set_tile_map_data_using_compatibility_format(p_layer, format, p_data);
}

void TileMap::_notification(int p_what) {
	switch (p_what) {
		case TileMap::NOTIFICATION_INTERNAL_PHYSICS_PROCESS: {
			// This is only executed when collision_animatable is enabled.

			bool in_editor = false;
#ifdef TOOLS_ENABLED
			in_editor = Engine::get_singleton()->is_editor_hint();
#endif // TOOLS_ENABLED
			if (is_inside_tree() && collision_animatable && !in_editor) {
				// Update transform on the physics tick when in animatable mode.
				last_valid_transform = new_transform;
				set_notify_local_transform(false);
				set_global_transform(new_transform);
				set_notify_local_transform(true);
			}
		} break;

		case TileMap::NOTIFICATION_LOCAL_TRANSFORM_CHANGED: {
			// This is only executed when collision_animatable is enabled.

			bool in_editor = false;
#ifdef TOOLS_ENABLED
			in_editor = Engine::get_singleton()->is_editor_hint();
#endif // TOOLS_ENABLED

			if (is_inside_tree() && collision_animatable && !in_editor) {
				// Store last valid transform.
				new_transform = get_global_transform();

				// ... but then revert changes.
				set_notify_local_transform(false);
				set_global_transform(last_valid_transform);
				set_notify_local_transform(true);
			}
		} break;
	}
}

#ifndef DISABLE_DEPRECATED
// Deprecated methods.
void TileMap::force_update(int p_layer) {
	notify_runtime_tile_data_update(p_layer);
	update_internals();
}
#endif // DISABLE_DEPRECATED

void TileMap::set_rendering_quadrant_size(int p_size) {
	ERR_FAIL_COND_MSG(p_size < 1, "TileMapQuadrant size cannot be smaller than 1.");

	rendering_quadrant_size = p_size;
	for (TileMapLayer *layer : layers) {
		layer->set_rendering_quadrant_size(p_size);
	}
	_emit_changed();
}

int TileMap::get_rendering_quadrant_size() const {
	return rendering_quadrant_size;
}

void TileMap::set_tileset(const Ref<TileSet> &p_tileset) {
	if (p_tileset == tile_set) {
		return;
	}

	// Set the tileset, registering to its changes.
	if (tile_set.is_valid()) {
		tile_set->disconnect_changed(callable_mp(this, &TileMap::_tile_set_changed));
	}

	tile_set = p_tileset;

	if (tile_set.is_valid()) {
		tile_set->connect_changed(callable_mp(this, &TileMap::_tile_set_changed));
	}

	for (int i = 0; i < get_child_count(); i++) {
		TileMapLayer *layer = Object::cast_to<TileMapLayer>(get_child(i));
		if (layer) {
			layer->set_tile_set(tile_set);
		}
	}
}

Ref<TileSet> TileMap::get_tileset() const {
	return tile_set;
}

int TileMap::get_layers_count() const {
	return layers.size();
}

void TileMap::add_layer(int p_to_pos) {
	if (p_to_pos < 0) {
		p_to_pos = layers.size() + p_to_pos + 1;
	}

	ERR_FAIL_INDEX(p_to_pos, (int)layers.size() + 1);

	// Must clear before adding the layer.
	TileMapLayer *new_layer = memnew(TileMapLayer);
	layers.insert(p_to_pos, new_layer);
	add_child(new_layer, false, INTERNAL_MODE_FRONT);
	new_layer->set_name(vformat("Layer%d", p_to_pos));
	new_layer->set_tile_set(tile_set);
	move_child(new_layer, p_to_pos);
	for (uint32_t i = 0; i < layers.size(); i++) {
		layers[i]->set_as_tile_map_internal_node(i);
	}
	new_layer->connect(CoreStringName(changed), callable_mp(this, &TileMap::_emit_changed));

	notify_property_list_changed();

	_emit_changed();

	update_configuration_warnings();
}

void TileMap::move_layer(int p_layer, int p_to_pos) {
	ERR_FAIL_INDEX(p_layer, (int)layers.size());
	ERR_FAIL_INDEX(p_to_pos, (int)layers.size() + 1);

	// Clear before shuffling layers.
	TileMapLayer *layer = layers[p_layer];
	layers.insert(p_to_pos, layer);
	layers.remove_at(p_to_pos < p_layer ? p_layer + 1 : p_layer);
	for (uint32_t i = 0; i < layers.size(); i++) {
		move_child(layers[i], i);
		layers[i]->set_as_tile_map_internal_node(i);
	}
	notify_property_list_changed();

	_emit_changed();

	update_configuration_warnings();
}

void TileMap::remove_layer(int p_layer) {
	ERR_FAIL_INDEX(p_layer, (int)layers.size());

	// Clear before removing the layer.
	TileMapLayer *removed = layers[p_layer];
	layers.remove_at(p_layer);
	remove_child(removed);
	removed->queue_free();

	for (uint32_t i = 0; i < layers.size(); i++) {
		layers[i]->set_as_tile_map_internal_node(i);
	}
	notify_property_list_changed();

	_emit_changed();

	update_configuration_warnings();
}

void TileMap::set_layer_name(int p_layer, String p_name) {
	TILEMAP_CALL_FOR_LAYER(p_layer, set_name, p_name);
}

String TileMap::get_layer_name(int p_layer) const {
	TILEMAP_CALL_FOR_LAYER_V(p_layer, "", get_name);
}

void TileMap::set_layer_enabled(int p_layer, bool p_enabled) {
	TILEMAP_CALL_FOR_LAYER(p_layer, set_enabled, p_enabled);
}

bool TileMap::is_layer_enabled(int p_layer) const {
	TILEMAP_CALL_FOR_LAYER_V(p_layer, false, is_enabled);
}

void TileMap::set_layer_modulate(int p_layer, Color p_modulate) {
	TILEMAP_CALL_FOR_LAYER(p_layer, set_modulate, p_modulate);
}

Color TileMap::get_layer_modulate(int p_layer) const {
	TILEMAP_CALL_FOR_LAYER_V(p_layer, Color(), get_modulate);
}

void TileMap::set_layer_y_sort_enabled(int p_layer, bool p_y_sort_enabled) {
	TILEMAP_CALL_FOR_LAYER(p_layer, set_y_sort_enabled, p_y_sort_enabled);
	update_configuration_warnings();
}

bool TileMap::is_layer_y_sort_enabled(int p_layer) const {
	TILEMAP_CALL_FOR_LAYER_V(p_layer, false, is_y_sort_enabled);
}

void TileMap::set_layer_y_sort_origin(int p_layer, int p_y_sort_origin) {
	TILEMAP_CALL_FOR_LAYER(p_layer, set_y_sort_origin, p_y_sort_origin);
	update_configuration_warnings();
}

int TileMap::get_layer_y_sort_origin(int p_layer) const {
	TILEMAP_CALL_FOR_LAYER_V(p_layer, 0, get_y_sort_origin);
}

void TileMap::set_layer_z_index(int p_layer, int p_z_index) {
	TILEMAP_CALL_FOR_LAYER(p_layer, set_z_index, p_z_index);
}

int TileMap::get_layer_z_index(int p_layer) const {
	TILEMAP_CALL_FOR_LAYER_V(p_layer, 0, get_z_index);
}

void TileMap::set_layer_navigation_enabled(int p_layer, bool p_enabled) {
	TILEMAP_CALL_FOR_LAYER(p_layer, set_navigation_enabled, p_enabled);
}

bool TileMap::is_layer_navigation_enabled(int p_layer) const {
	TILEMAP_CALL_FOR_LAYER_V(p_layer, false, is_navigation_enabled);
}

void TileMap::set_layer_navigation_map(int p_layer, RID p_map) {
	TILEMAP_CALL_FOR_LAYER(p_layer, set_navigation_map, p_map);
}

RID TileMap::get_layer_navigation_map(int p_layer) const {
	TILEMAP_CALL_FOR_LAYER_V(p_layer, RID(), get_navigation_map);
}

void TileMap::set_collision_animatable(bool p_collision_animatable) {
	if (collision_animatable == p_collision_animatable) {
		return;
	}
	collision_animatable = p_collision_animatable;
	set_notify_local_transform(p_collision_animatable);
	set_physics_process_internal(p_collision_animatable);
	for (TileMapLayer *layer : layers) {
		layer->set_use_kinematic_bodies(layer);
	}
}

bool TileMap::is_collision_animatable() const {
	return collision_animatable;
}

void TileMap::set_collision_visibility_mode(TileMap::VisibilityMode p_show_collision) {
	if (collision_visibility_mode == p_show_collision) {
		return;
	}
	collision_visibility_mode = p_show_collision;
	for (TileMapLayer *layer : layers) {
		layer->set_collision_visibility_mode(TileMapLayer::DebugVisibilityMode(p_show_collision));
	}
	_emit_changed();
}

TileMap::VisibilityMode TileMap::get_collision_visibility_mode() const {
	return collision_visibility_mode;
}

void TileMap::set_navigation_visibility_mode(TileMap::VisibilityMode p_show_navigation) {
	if (navigation_visibility_mode == p_show_navigation) {
		return;
	}
	navigation_visibility_mode = p_show_navigation;
	for (TileMapLayer *layer : layers) {
		layer->set_navigation_visibility_mode(TileMapLayer::DebugVisibilityMode(p_show_navigation));
	}
	_emit_changed();
}

TileMap::VisibilityMode TileMap::get_navigation_visibility_mode() const {
	return navigation_visibility_mode;
}

void TileMap::set_y_sort_enabled(bool p_enable) {
	if (is_y_sort_enabled() == p_enable) {
		return;
	}
	Node2D::set_y_sort_enabled(p_enable);
	_emit_changed();
	update_configuration_warnings();
}

void TileMap::set_cell(int p_layer, const Vector2i &p_coords, int p_source_id, const Vector2i p_atlas_coords, int p_alternative_tile) {
	TILEMAP_CALL_FOR_LAYER(p_layer, set_cell, p_coords, p_source_id, p_atlas_coords, p_alternative_tile);
}

void TileMap::erase_cell(int p_layer, const Vector2i &p_coords) {
	TILEMAP_CALL_FOR_LAYER(p_layer, set_cell, p_coords, TileSet::INVALID_SOURCE, TileSetSource::INVALID_ATLAS_COORDS, TileSetSource::INVALID_TILE_ALTERNATIVE);
}

int TileMap::get_cell_source_id(int p_layer, const Vector2i &p_coords, bool p_use_proxies) const {
	if (p_use_proxies && tile_set.is_valid()) {
		if (p_layer < 0) {
			p_layer = layers.size() + p_layer;
		}
		ERR_FAIL_INDEX_V(p_layer, (int)layers.size(), TileSet::INVALID_SOURCE);

		int source_id = layers[p_layer]->get_cell_source_id(p_coords);
		Vector2i atlas_coords = layers[p_layer]->get_cell_atlas_coords(p_coords);
		int alternative_id = layers[p_layer]->get_cell_alternative_tile(p_coords);

		Array arr = tile_set->map_tile_proxy(source_id, atlas_coords, alternative_id);
		ERR_FAIL_COND_V(arr.size() != 3, TileSet::INVALID_SOURCE);
		return arr[0];
	} else {
		TILEMAP_CALL_FOR_LAYER_V(p_layer, TileSet::INVALID_SOURCE, get_cell_source_id, p_coords);
	}
}

Vector2i TileMap::get_cell_atlas_coords(int p_layer, const Vector2i &p_coords, bool p_use_proxies) const {
	if (p_use_proxies && tile_set.is_valid()) {
		if (p_layer < 0) {
			p_layer = layers.size() + p_layer;
		}
		ERR_FAIL_INDEX_V(p_layer, (int)layers.size(), TileSetAtlasSource::INVALID_ATLAS_COORDS);

		int source_id = layers[p_layer]->get_cell_source_id(p_coords);
		Vector2i atlas_coords = layers[p_layer]->get_cell_atlas_coords(p_coords);
		int alternative_id = layers[p_layer]->get_cell_alternative_tile(p_coords);

		Array arr = tile_set->map_tile_proxy(source_id, atlas_coords, alternative_id);
		ERR_FAIL_COND_V(arr.size() != 3, TileSetSource::INVALID_ATLAS_COORDS);
		return arr[1];
	} else {
		TILEMAP_CALL_FOR_LAYER_V(p_layer, TileSetSource::INVALID_ATLAS_COORDS, get_cell_atlas_coords, p_coords);
	}
}

int TileMap::get_cell_alternative_tile(int p_layer, const Vector2i &p_coords, bool p_use_proxies) const {
	if (p_use_proxies && tile_set.is_valid()) {
		if (p_layer < 0) {
			p_layer = layers.size() + p_layer;
		}
		ERR_FAIL_INDEX_V(p_layer, (int)layers.size(), TileSetSource::INVALID_TILE_ALTERNATIVE);

		int source_id = layers[p_layer]->get_cell_source_id(p_coords);
		Vector2i atlas_coords = layers[p_layer]->get_cell_atlas_coords(p_coords);
		int alternative_id = layers[p_layer]->get_cell_alternative_tile(p_coords);

		Array arr = tile_set->map_tile_proxy(source_id, atlas_coords, alternative_id);
		ERR_FAIL_COND_V(arr.size() != 3, TileSetSource::INVALID_TILE_ALTERNATIVE);
		return arr[2];
	} else {
		TILEMAP_CALL_FOR_LAYER_V(p_layer, TileSetSource::INVALID_TILE_ALTERNATIVE, get_cell_alternative_tile, p_coords);
	}
}

TileData *TileMap::get_cell_tile_data(int p_layer, const Vector2i &p_coords, bool p_use_proxies) const {
	if (p_use_proxies && tile_set.is_valid()) {
		if (p_layer < 0) {
			p_layer = layers.size() + p_layer;
		}
		ERR_FAIL_INDEX_V(p_layer, (int)layers.size(), nullptr);

		int source_id = layers[p_layer]->get_cell_source_id(p_coords);
		Vector2i atlas_coords = layers[p_layer]->get_cell_atlas_coords(p_coords);
		int alternative_id = layers[p_layer]->get_cell_alternative_tile(p_coords);

		Array arr = tile_set->map_tile_proxy(source_id, atlas_coords, alternative_id);
		ERR_FAIL_COND_V(arr.size() != 3, nullptr);

		Ref<TileSetAtlasSource> atlas_source = tile_set->get_source(arr[0]);
		if (atlas_source.is_valid()) {
			return atlas_source->get_tile_data(arr[1], arr[2]);
		} else {
			return nullptr;
		}
	} else {
		TILEMAP_CALL_FOR_LAYER_V(p_layer, nullptr, get_cell_tile_data, p_coords);
	}
}

bool TileMap::is_cell_flipped_h(int p_layer, const Vector2i &p_coords, bool p_use_proxies) const {
	return get_cell_alternative_tile(p_layer, p_coords, p_use_proxies) & TileSetAtlasSource::TRANSFORM_FLIP_H;
}

bool TileMap::is_cell_flipped_v(int p_layer, const Vector2i &p_coords, bool p_use_proxies) const {
	return get_cell_alternative_tile(p_layer, p_coords, p_use_proxies) & TileSetAtlasSource::TRANSFORM_FLIP_V;
}

bool TileMap::is_cell_transposed(int p_layer, const Vector2i &p_coords, bool p_use_proxies) const {
	return get_cell_alternative_tile(p_layer, p_coords, p_use_proxies) & TileSetAtlasSource::TRANSFORM_TRANSPOSE;
}

Ref<TileMapPattern> TileMap::get_pattern(int p_layer, TypedArray<Vector2i> p_coords_array) {
	TILEMAP_CALL_FOR_LAYER_V(p_layer, Ref<TileMapPattern>(), get_pattern, p_coords_array);
}

Vector2i TileMap::map_pattern(const Vector2i &p_position_in_tilemap, const Vector2i &p_coords_in_pattern, Ref<TileMapPattern> p_pattern) {
	ERR_FAIL_COND_V(tile_set.is_null(), Vector2i());
	return tile_set->map_pattern(p_position_in_tilemap, p_coords_in_pattern, p_pattern);
}

void TileMap::set_pattern(int p_layer, const Vector2i &p_position, const Ref<TileMapPattern> p_pattern) {
	TILEMAP_CALL_FOR_LAYER(p_layer, set_pattern, p_position, p_pattern);
}

HashMap<Vector2i, TileSet::TerrainsPattern> TileMap::terrain_fill_constraints(int p_layer, const Vector<Vector2i> &p_to_replace, int p_terrain_set, const RBSet<TerrainConstraint> &p_constraints) {
	HashMap<Vector2i, TileSet::TerrainsPattern> err_value;
	TILEMAP_CALL_FOR_LAYER_V(p_layer, err_value, terrain_fill_constraints, p_to_replace, p_terrain_set, p_constraints);
}

HashMap<Vector2i, TileSet::TerrainsPattern> TileMap::terrain_fill_connect(int p_layer, const Vector<Vector2i> &p_coords_array, int p_terrain_set, int p_terrain, bool p_ignore_empty_terrains) {
	HashMap<Vector2i, TileSet::TerrainsPattern> err_value;
	TILEMAP_CALL_FOR_LAYER_V(p_layer, err_value, terrain_fill_connect, p_coords_array, p_terrain_set, p_terrain, p_ignore_empty_terrains);
}

HashMap<Vector2i, TileSet::TerrainsPattern> TileMap::terrain_fill_path(int p_layer, const Vector<Vector2i> &p_coords_array, int p_terrain_set, int p_terrain, bool p_ignore_empty_terrains) {
	HashMap<Vector2i, TileSet::TerrainsPattern> err_value;
	TILEMAP_CALL_FOR_LAYER_V(p_layer, err_value, terrain_fill_path, p_coords_array, p_terrain_set, p_terrain, p_ignore_empty_terrains);
}

HashMap<Vector2i, TileSet::TerrainsPattern> TileMap::terrain_fill_pattern(int p_layer, const Vector<Vector2i> &p_coords_array, int p_terrain_set, TileSet::TerrainsPattern p_terrains_pattern, bool p_ignore_empty_terrains) {
	HashMap<Vector2i, TileSet::TerrainsPattern> err_value;
	TILEMAP_CALL_FOR_LAYER_V(p_layer, err_value, terrain_fill_pattern, p_coords_array, p_terrain_set, p_terrains_pattern, p_ignore_empty_terrains);
}

void TileMap::set_cells_terrain_connect(int p_layer, TypedArray<Vector2i> p_cells, int p_terrain_set, int p_terrain, bool p_ignore_empty_terrains) {
	TILEMAP_CALL_FOR_LAYER(p_layer, set_cells_terrain_connect, p_cells, p_terrain_set, p_terrain, p_ignore_empty_terrains);
}

void TileMap::set_cells_terrain_path(int p_layer, TypedArray<Vector2i> p_path, int p_terrain_set, int p_terrain, bool p_ignore_empty_terrains) {
	TILEMAP_CALL_FOR_LAYER(p_layer, set_cells_terrain_path, p_path, p_terrain_set, p_terrain, p_ignore_empty_terrains);
}

TileMapCell TileMap::get_cell(int p_layer, const Vector2i &p_coords, bool p_use_proxies) const {
	if (p_use_proxies) {
		WARN_DEPRECATED_MSG("use_proxies is deprecated.");
	}
	TILEMAP_CALL_FOR_LAYER_V(p_layer, TileMapCell(), get_cell, p_coords);
}

Vector2i TileMap::get_coords_for_body_rid(RID p_physics_body) {
	for (const TileMapLayer *layer : layers) {
		if (layer->has_body_rid(p_physics_body)) {
			return layer->get_coords_for_body_rid(p_physics_body);
		}
	}
	ERR_FAIL_V_MSG(Vector2i(), vformat("No tiles for the given body RID %d.", p_physics_body.get_id()));
}

int TileMap::get_layer_for_body_rid(RID p_physics_body) {
	for (uint32_t i = 0; i < layers.size(); i++) {
		if (layers[i]->has_body_rid(p_physics_body)) {
			return i;
		}
	}
	ERR_FAIL_V_MSG(-1, vformat("No tiles for the given body RID %d.", p_physics_body.get_id()));
}

void TileMap::fix_invalid_tiles() {
	for (TileMapLayer *layer : layers) {
		layer->fix_invalid_tiles();
	}
}

#ifdef TOOLS_ENABLED
TileMapLayer *TileMap::duplicate_layer_from_internal(int p_layer) {
	ERR_FAIL_INDEX_V(p_layer, (int)layers.size(), nullptr);
	return Object::cast_to<TileMapLayer>(layers[p_layer]->duplicate(DUPLICATE_USE_INSTANTIATION | DUPLICATE_FROM_EDITOR));
}
#endif // TOOLS_ENABLED

void TileMap::clear_layer(int p_layer) {
	TILEMAP_CALL_FOR_LAYER(p_layer, clear)
}

void TileMap::clear() {
	for (TileMapLayer *layer : layers) {
		layer->clear();
	}
}

void TileMap::update_internals() {
	for (TileMapLayer *layer : layers) {
		layer->update_internals();
	}
}

void TileMap::notify_runtime_tile_data_update(int p_layer) {
	if (p_layer >= 0) {
		TILEMAP_CALL_FOR_LAYER(p_layer, notify_runtime_tile_data_update);
	} else {
		for (TileMapLayer *layer : layers) {
			layer->notify_runtime_tile_data_update();
		}
	}
}

#ifdef DEBUG_ENABLED
Rect2 TileMap::_edit_get_rect() const {
	// Return the visible rect of the tilemap.
	if (layers.is_empty()) {
		return Rect2();
	}

	bool any_changed = false;
	bool changed = false;
	Rect2 rect = layers[0]->get_rect(changed);
	any_changed |= changed;
	for (uint32_t i = 1; i < layers.size(); i++) {
		rect = rect.merge(layers[i]->get_rect(changed));
		any_changed |= changed;
	}
	const_cast<TileMap *>(this)->item_rect_changed(any_changed);
	return rect;
}
#endif // DEBUG_ENABLED

bool TileMap::_set(const StringName &p_name, const Variant &p_value) {
	int index;
	const String sname = p_name;

	Vector<String> components = String(p_name).split("/", true, 2);
	if (sname == "format") {
		if (p_value.get_type() == Variant::INT) {
			format = (TileMapDataFormat)(p_value.operator int64_t()); // Set format used for loading.
			return true;
		}
	}
#ifndef DISABLE_DEPRECATED
	else if (sname == "cell_quadrant_size") {
		set_rendering_quadrant_size(p_value);
		return true;
	}
#endif // DISABLE_DEPRECATED
	else if (property_helper.is_property_valid(sname, &index)) {
		if (index >= (int)layers.size()) {
			while (index >= (int)layers.size()) {
				TileMapLayer *new_layer = memnew(TileMapLayer);
				add_child(new_layer, false, INTERNAL_MODE_FRONT);
				new_layer->set_as_tile_map_internal_node(index);
				new_layer->set_name(vformat("Layer%d", index));
				new_layer->set_tile_set(tile_set);
				new_layer->connect(CoreStringName(changed), callable_mp(this, &TileMap::_emit_changed));
				layers.push_back(new_layer);
			}

			notify_property_list_changed();
			_emit_changed();
			update_configuration_warnings();
		}

		if (property_helper.property_set_value(sname, p_value)) {
			if (components[1] == "tile_data") {
				_emit_changed();
			}
			return true;
		}
	}
	return false;
}

bool TileMap::_get(const StringName &p_name, Variant &r_ret) const {
	const String sname = p_name;

	Vector<String> components = String(p_name).split("/", true, 2);
	if (p_name == "format") {
		r_ret = TileMapDataFormat::TILE_MAP_DATA_FORMAT_MAX - 1; // When saving, always save highest format.
		return true;
	}
#ifndef DISABLE_DEPRECATED
	else if (sname == "cell_quadrant_size") { // Kept for compatibility reasons.
		r_ret = get_rendering_quadrant_size();
		return true;
	}
#endif // DISABLE_DEPRECATED
	else {
		return property_helper.property_get_value(sname, r_ret);
	}
}

void TileMap::_get_property_list(List<PropertyInfo> *p_list) const {
	p_list->push_back(PropertyInfo(Variant::INT, "format", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NO_EDITOR | PROPERTY_USAGE_INTERNAL));
	property_helper.get_property_list(p_list);
}

Vector2 TileMap::map_to_local(const Vector2i &p_pos) const {
	ERR_FAIL_COND_V(tile_set.is_null(), Vector2());
	return tile_set->map_to_local(p_pos);
}

Vector2i TileMap::local_to_map(const Vector2 &p_pos) const {
	ERR_FAIL_COND_V(tile_set.is_null(), Vector2i());
	return tile_set->local_to_map(p_pos);
}

bool TileMap::is_existing_neighbor(TileSet::CellNeighbor p_cell_neighbor) const {
	ERR_FAIL_COND_V(tile_set.is_null(), false);
	return tile_set->is_existing_neighbor(p_cell_neighbor);
}

Vector2i TileMap::get_neighbor_cell(const Vector2i &p_coords, TileSet::CellNeighbor p_cell_neighbor) const {
	ERR_FAIL_COND_V(tile_set.is_null(), Vector2i());
	return tile_set->get_neighbor_cell(p_coords, p_cell_neighbor);
}

TypedArray<Vector2i> TileMap::get_used_cells(int p_layer) const {
	TILEMAP_CALL_FOR_LAYER_V(p_layer, TypedArray<Vector2i>(), get_used_cells);
}

TypedArray<Vector2i> TileMap::get_used_cells_by_id(int p_layer, int p_source_id, const Vector2i p_atlas_coords, int p_alternative_tile) const {
	TILEMAP_CALL_FOR_LAYER_V(p_layer, TypedArray<Vector2i>(), get_used_cells_by_id, p_source_id, p_atlas_coords, p_alternative_tile);
}

Rect2i TileMap::get_used_rect() const {
	// Return the visible rect of the tilemap.
	bool first = true;
	Rect2i rect = Rect2i();
	for (const TileMapLayer *layer : layers) {
		Rect2i layer_rect = layer->get_used_rect();
		if (layer_rect == Rect2i()) {
			continue;
		}
		if (first) {
			rect = layer_rect;
			first = false;
		} else {
			rect = rect.merge(layer_rect);
		}
	}
	return rect;
}

// --- Override some methods of the CanvasItem class to pass the changes to the quadrants CanvasItems ---

void TileMap::set_light_mask(int p_light_mask) {
	// Set light mask for occlusion and applies it to all layers too.
	CanvasItem::set_light_mask(p_light_mask);
	for (TileMapLayer *layer : layers) {
		layer->set_light_mask(p_light_mask);
	}
}

void TileMap::set_self_modulate(const Color &p_self_modulate) {
	// Set self_modulation and applies it to all layers too.
	CanvasItem::set_self_modulate(p_self_modulate);
	for (TileMapLayer *layer : layers) {
		layer->set_self_modulate(p_self_modulate);
	}
}

void TileMap::set_texture_filter(TextureFilter p_texture_filter) {
	// Set a default texture filter and applies it to all layers too.
	CanvasItem::set_texture_filter(p_texture_filter);
	for (TileMapLayer *layer : layers) {
		layer->set_texture_filter(p_texture_filter);
	}
}

void TileMap::set_texture_repeat(CanvasItem::TextureRepeat p_texture_repeat) {
	// Set a default texture repeat and applies it to all layers too.
	CanvasItem::set_texture_repeat(p_texture_repeat);
	for (TileMapLayer *layer : layers) {
		layer->set_texture_repeat(p_texture_repeat);
	}
}

TypedArray<Vector2i> TileMap::get_surrounding_cells(const Vector2i &p_coords) {
	if (tile_set.is_null()) {
		return TypedArray<Vector2i>();
	}

	return tile_set->get_surrounding_cells(p_coords);
}

PackedStringArray TileMap::get_configuration_warnings() const {
	PackedStringArray warnings = Node2D::get_configuration_warnings();

	warnings.push_back(RTR("The TileMap node is deprecated as it is superseded by the use of multiple TileMapLayer nodes.\nTo convert a TileMap to a set of TileMapLayer nodes, open the TileMap bottom panel with this node selected, click the toolbox icon in the top-right corner and choose \"Extract TileMap layers as individual TileMapLayer nodes\"."));

	// Retrieve the set of Z index values with a Y-sorted layer.
	RBSet<int> y_sorted_z_index;
	for (const TileMapLayer *layer : layers) {
		if (layer->is_y_sort_enabled()) {
			y_sorted_z_index.insert(layer->get_z_index());
		}
	}

	// Check if we have a non-sorted layer in a Z-index with a Y-sorted layer.
	for (const TileMapLayer *layer : layers) {
		if (!layer->is_y_sort_enabled() && y_sorted_z_index.has(layer->get_z_index())) {
			warnings.push_back(RTR("A Y-sorted layer has the same Z-index value as a not Y-sorted layer.\nThis may lead to unwanted behaviors, as a layer that is not Y-sorted will be Y-sorted as a whole with tiles from Y-sorted layers."));
			break;
		}
	}

	if (!is_y_sort_enabled()) {
		// Check if Y-sort is enabled on a layer but not on the node.
		for (const TileMapLayer *layer : layers) {
			if (layer->is_y_sort_enabled()) {
				warnings.push_back(RTR("A TileMap layer is set as Y-sorted, but Y-sort is not enabled on the TileMap node itself."));
				break;
			}
		}
	} else {
		// Check if Y-sort is enabled on the node, but not on any of the layers.
		bool need_warning = true;
		for (const TileMapLayer *layer : layers) {
			if (layer->is_y_sort_enabled()) {
				need_warning = false;
				break;
			}
		}
		if (need_warning) {
			warnings.push_back(RTR("The TileMap node is set as Y-sorted, but Y-sort is not enabled on any of the TileMap's layers.\nThis may lead to unwanted behaviors, as a layer that is not Y-sorted will be Y-sorted as a whole."));
		}
	}

	// Check if we are in isometric mode without Y-sort enabled.
	if (tile_set.is_valid() && tile_set->get_tile_shape() == TileSet::TILE_SHAPE_ISOMETRIC) {
		bool warn = !is_y_sort_enabled();
		if (!warn) {
			for (const TileMapLayer *layer : layers) {
				if (!layer->is_y_sort_enabled()) {
					warn = true;
					break;
				}
			}
		}

		if (warn) {
			warnings.push_back(RTR("Isometric TileSet will likely not look as intended without Y-sort enabled for the TileMap and all of its layers."));
		}
	}

	return warnings;
}

void TileMap::_bind_methods() {
#ifndef DISABLE_DEPRECATED
	ClassDB::bind_method(D_METHOD("set_navigation_map", "layer", "map"), &TileMap::set_layer_navigation_map);
	ClassDB::bind_method(D_METHOD("get_navigation_map", "layer"), &TileMap::get_layer_navigation_map);
	ClassDB::bind_method(D_METHOD("force_update", "layer"), &TileMap::force_update, DEFVAL(-1));
#endif // DISABLE_DEPRECATED

	ClassDB::bind_method(D_METHOD("set_tileset", "tileset"), &TileMap::set_tileset);
	ClassDB::bind_method(D_METHOD("get_tileset"), &TileMap::get_tileset);

	ClassDB::bind_method(D_METHOD("set_rendering_quadrant_size", "size"), &TileMap::set_rendering_quadrant_size);
	ClassDB::bind_method(D_METHOD("get_rendering_quadrant_size"), &TileMap::get_rendering_quadrant_size);

	ClassDB::bind_method(D_METHOD("get_layers_count"), &TileMap::get_layers_count);
	ClassDB::bind_method(D_METHOD("add_layer", "to_position"), &TileMap::add_layer);
	ClassDB::bind_method(D_METHOD("move_layer", "layer", "to_position"), &TileMap::move_layer);
	ClassDB::bind_method(D_METHOD("remove_layer", "layer"), &TileMap::remove_layer);
	ClassDB::bind_method(D_METHOD("set_layer_name", "layer", "name"), &TileMap::set_layer_name);
	ClassDB::bind_method(D_METHOD("get_layer_name", "layer"), &TileMap::get_layer_name);
	ClassDB::bind_method(D_METHOD("set_layer_enabled", "layer", "enabled"), &TileMap::set_layer_enabled);
	ClassDB::bind_method(D_METHOD("is_layer_enabled", "layer"), &TileMap::is_layer_enabled);
	ClassDB::bind_method(D_METHOD("set_layer_modulate", "layer", "modulate"), &TileMap::set_layer_modulate);
	ClassDB::bind_method(D_METHOD("get_layer_modulate", "layer"), &TileMap::get_layer_modulate);
	ClassDB::bind_method(D_METHOD("set_layer_y_sort_enabled", "layer", "y_sort_enabled"), &TileMap::set_layer_y_sort_enabled);
	ClassDB::bind_method(D_METHOD("is_layer_y_sort_enabled", "layer"), &TileMap::is_layer_y_sort_enabled);
	ClassDB::bind_method(D_METHOD("set_layer_y_sort_origin", "layer", "y_sort_origin"), &TileMap::set_layer_y_sort_origin);
	ClassDB::bind_method(D_METHOD("get_layer_y_sort_origin", "layer"), &TileMap::get_layer_y_sort_origin);
	ClassDB::bind_method(D_METHOD("set_layer_z_index", "layer", "z_index"), &TileMap::set_layer_z_index);
	ClassDB::bind_method(D_METHOD("get_layer_z_index", "layer"), &TileMap::get_layer_z_index);
	ClassDB::bind_method(D_METHOD("set_layer_navigation_enabled", "layer", "enabled"), &TileMap::set_layer_navigation_enabled);
	ClassDB::bind_method(D_METHOD("is_layer_navigation_enabled", "layer"), &TileMap::is_layer_navigation_enabled);
	ClassDB::bind_method(D_METHOD("set_layer_navigation_map", "layer", "map"), &TileMap::set_layer_navigation_map);
	ClassDB::bind_method(D_METHOD("get_layer_navigation_map", "layer"), &TileMap::get_layer_navigation_map);

	ClassDB::bind_method(D_METHOD("set_collision_animatable", "enabled"), &TileMap::set_collision_animatable);
	ClassDB::bind_method(D_METHOD("is_collision_animatable"), &TileMap::is_collision_animatable);
	ClassDB::bind_method(D_METHOD("set_collision_visibility_mode", "collision_visibility_mode"), &TileMap::set_collision_visibility_mode);
	ClassDB::bind_method(D_METHOD("get_collision_visibility_mode"), &TileMap::get_collision_visibility_mode);

	ClassDB::bind_method(D_METHOD("set_navigation_visibility_mode", "navigation_visibility_mode"), &TileMap::set_navigation_visibility_mode);
	ClassDB::bind_method(D_METHOD("get_navigation_visibility_mode"), &TileMap::get_navigation_visibility_mode);

	ClassDB::bind_method(D_METHOD("set_cell", "layer", "coords", "source_id", "atlas_coords", "alternative_tile"), &TileMap::set_cell, DEFVAL(TileSet::INVALID_SOURCE), DEFVAL(TileSetSource::INVALID_ATLAS_COORDS), DEFVAL(0));
	ClassDB::bind_method(D_METHOD("erase_cell", "layer", "coords"), &TileMap::erase_cell);
	ClassDB::bind_method(D_METHOD("get_cell_source_id", "layer", "coords", "use_proxies"), &TileMap::get_cell_source_id, DEFVAL(false));
	ClassDB::bind_method(D_METHOD("get_cell_atlas_coords", "layer", "coords", "use_proxies"), &TileMap::get_cell_atlas_coords, DEFVAL(false));
	ClassDB::bind_method(D_METHOD("get_cell_alternative_tile", "layer", "coords", "use_proxies"), &TileMap::get_cell_alternative_tile, DEFVAL(false));
	ClassDB::bind_method(D_METHOD("get_cell_tile_data", "layer", "coords", "use_proxies"), &TileMap::get_cell_tile_data, DEFVAL(false));

	ClassDB::bind_method(D_METHOD("is_cell_flipped_h", "layer", "coords", "use_proxies"), &TileMap::is_cell_flipped_h, DEFVAL(false));
	ClassDB::bind_method(D_METHOD("is_cell_flipped_v", "layer", "coords", "use_proxies"), &TileMap::is_cell_flipped_v, DEFVAL(false));
	ClassDB::bind_method(D_METHOD("is_cell_transposed", "layer", "coords", "use_proxies"), &TileMap::is_cell_transposed, DEFVAL(false));

	ClassDB::bind_method(D_METHOD("get_coords_for_body_rid", "body"), &TileMap::get_coords_for_body_rid);
	ClassDB::bind_method(D_METHOD("get_layer_for_body_rid", "body"), &TileMap::get_layer_for_body_rid);

	ClassDB::bind_method(D_METHOD("get_pattern", "layer", "coords_array"), &TileMap::get_pattern);
	ClassDB::bind_method(D_METHOD("map_pattern", "position_in_tilemap", "coords_in_pattern", "pattern"), &TileMap::map_pattern);
	ClassDB::bind_method(D_METHOD("set_pattern", "layer", "position", "pattern"), &TileMap::set_pattern);

	ClassDB::bind_method(D_METHOD("set_cells_terrain_connect", "layer", "cells", "terrain_set", "terrain", "ignore_empty_terrains"), &TileMap::set_cells_terrain_connect, DEFVAL(true));
	ClassDB::bind_method(D_METHOD("set_cells_terrain_path", "layer", "path", "terrain_set", "terrain", "ignore_empty_terrains"), &TileMap::set_cells_terrain_path, DEFVAL(true));

	ClassDB::bind_method(D_METHOD("fix_invalid_tiles"), &TileMap::fix_invalid_tiles);
	ClassDB::bind_method(D_METHOD("clear_layer", "layer"), &TileMap::clear_layer);
	ClassDB::bind_method(D_METHOD("clear"), &TileMap::clear);

	ClassDB::bind_method(D_METHOD("update_internals"), &TileMap::update_internals);
	ClassDB::bind_method(D_METHOD("notify_runtime_tile_data_update", "layer"), &TileMap::notify_runtime_tile_data_update, DEFVAL(-1));

	ClassDB::bind_method(D_METHOD("get_surrounding_cells", "coords"), &TileMap::get_surrounding_cells);

	ClassDB::bind_method(D_METHOD("get_used_cells", "layer"), &TileMap::get_used_cells);
	ClassDB::bind_method(D_METHOD("get_used_cells_by_id", "layer", "source_id", "atlas_coords", "alternative_tile"), &TileMap::get_used_cells_by_id, DEFVAL(TileSet::INVALID_SOURCE), DEFVAL(TileSetSource::INVALID_ATLAS_COORDS), DEFVAL(TileSetSource::INVALID_TILE_ALTERNATIVE));
	ClassDB::bind_method(D_METHOD("get_used_rect"), &TileMap::get_used_rect);

	ClassDB::bind_method(D_METHOD("map_to_local", "map_position"), &TileMap::map_to_local);
	ClassDB::bind_method(D_METHOD("local_to_map", "local_position"), &TileMap::local_to_map);

	ClassDB::bind_method(D_METHOD("get_neighbor_cell", "coords", "neighbor"), &TileMap::get_neighbor_cell);

	GDVIRTUAL_BIND(_use_tile_data_runtime_update, "layer", "coords");
	GDVIRTUAL_BIND(_tile_data_runtime_update, "layer", "coords", "tile_data");

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "tile_set", PROPERTY_HINT_RESOURCE_TYPE, "TileSet"), "set_tileset", "get_tileset");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "rendering_quadrant_size", PROPERTY_HINT_RANGE, "1,128,1"), "set_rendering_quadrant_size", "get_rendering_quadrant_size");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "collision_animatable"), "set_collision_animatable", "is_collision_animatable");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "collision_visibility_mode", PROPERTY_HINT_ENUM, "Default,Force Show,Force Hide"), "set_collision_visibility_mode", "get_collision_visibility_mode");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "navigation_visibility_mode", PROPERTY_HINT_ENUM, "Default,Force Show,Force Hide"), "set_navigation_visibility_mode", "get_navigation_visibility_mode");

	ADD_ARRAY("layers", "layer_");

	ADD_PROPERTY_DEFAULT("format", TileMapDataFormat::TILE_MAP_DATA_FORMAT_1);

	ADD_SIGNAL(MethodInfo(CoreStringName(changed)));

	BIND_ENUM_CONSTANT(VISIBILITY_MODE_DEFAULT);
	BIND_ENUM_CONSTANT(VISIBILITY_MODE_FORCE_HIDE);
	BIND_ENUM_CONSTANT(VISIBILITY_MODE_FORCE_SHOW);
}

TileMap::TileMap() {
	TileMapLayer *new_layer = memnew(TileMapLayer);
	add_child(new_layer, false, INTERNAL_MODE_FRONT);
	new_layer->set_as_tile_map_internal_node(0);
	new_layer->set_name("Layer0");
	new_layer->set_tile_set(tile_set);
	new_layer->connect(CoreStringName(changed), callable_mp(this, &TileMap::_emit_changed));
	layers.push_back(new_layer);

	if (!base_property_helper.is_initialized()) {
		// Initialize static PropertyListHelper if it wasn't yet. This has to be done here,
		// because creating TileMapLayer in a static context is not always safe.
		TileMapLayer *defaults = memnew(TileMapLayer);

		base_property_helper.set_prefix("layer_");
		base_property_helper.set_array_length_getter(&TileMap::get_layers_count);
		base_property_helper.register_property(PropertyInfo(Variant::STRING, "name"), defaults->get_name(), &TileMap::set_layer_name, &TileMap::get_layer_name);
		base_property_helper.register_property(PropertyInfo(Variant::BOOL, "enabled"), defaults->is_enabled(), &TileMap::set_layer_enabled, &TileMap::is_layer_enabled);
		base_property_helper.register_property(PropertyInfo(Variant::COLOR, "modulate"), defaults->get_modulate(), &TileMap::set_layer_modulate, &TileMap::get_layer_modulate);
		base_property_helper.register_property(PropertyInfo(Variant::BOOL, "y_sort_enabled"), defaults->is_y_sort_enabled(), &TileMap::set_layer_y_sort_enabled, &TileMap::is_layer_y_sort_enabled);
		base_property_helper.register_property(PropertyInfo(Variant::INT, "y_sort_origin", PROPERTY_HINT_NONE, "suffix:px"), defaults->get_y_sort_origin(), &TileMap::set_layer_y_sort_origin, &TileMap::get_layer_y_sort_origin);
		base_property_helper.register_property(PropertyInfo(Variant::INT, "z_index"), defaults->get_z_index(), &TileMap::set_layer_z_index, &TileMap::get_layer_z_index);
		base_property_helper.register_property(PropertyInfo(Variant::BOOL, "navigation_enabled"), defaults->is_navigation_enabled(), &TileMap::set_layer_navigation_enabled, &TileMap::is_layer_navigation_enabled);
		base_property_helper.register_property(PropertyInfo(Variant::PACKED_INT32_ARRAY, "tile_data", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NO_EDITOR), Vector<int>(), &TileMap::_set_layer_tile_data, &TileMap::_get_tile_map_data_using_compatibility_format);
		PropertyListHelper::register_base_helper(&base_property_helper);

		memdelete(defaults);
	}

	property_helper.setup_for_instance(base_property_helper, this);
}

#undef TILEMAP_CALL_FOR_LAYER
#undef TILEMAP_CALL_FOR_LAYER_V

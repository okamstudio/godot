/**************************************************************************/
/*  tile_map_layer.cpp                                                    */
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

#include "tile_map_layer.h"

#include "core/io/marshalls.h"
#include "scene/2d/tile_map.h"
#include "scene/gui/control.h"
#include "scene/resources/world_2d.h"
#include "servers/navigation_server_2d.h"

#ifdef DEBUG_ENABLED
/////////////////////////////// Debug //////////////////////////////////////////
constexpr int TILE_MAP_DEBUG_QUADRANT_SIZE = 16;

Vector2i TileMapLayer::_coords_to_debug_quadrant_coords(const Vector2i &p_coords) const {
	return Vector2i(
			p_coords.x > 0 ? p_coords.x / TILE_MAP_DEBUG_QUADRANT_SIZE : (p_coords.x - (TILE_MAP_DEBUG_QUADRANT_SIZE - 1)) / TILE_MAP_DEBUG_QUADRANT_SIZE,
			p_coords.y > 0 ? p_coords.y / TILE_MAP_DEBUG_QUADRANT_SIZE : (p_coords.y - (TILE_MAP_DEBUG_QUADRANT_SIZE - 1)) / TILE_MAP_DEBUG_QUADRANT_SIZE);
}

void TileMapLayer::_debug_update(bool p_force_cleanup) {
	RenderingServer *rs = RenderingServer::get_singleton();

	// Check if we should cleanup everything.
	bool forced_cleanup = p_force_cleanup || !enabled || tile_set.is_null() || !is_visible_in_tree();

	if (forced_cleanup) {
		for (KeyValue<Vector2i, Ref<DebugQuadrant>> &kv : debug_quadrant_map) {
			// Free the quadrant.
			Ref<DebugQuadrant> &debug_quadrant = kv.value;
			if (debug_quadrant->canvas_item.is_valid()) {
				rs->free(debug_quadrant->canvas_item);
			}
		}
		debug_quadrant_map.clear();
		_debug_was_cleaned_up = true;
		return;
	}

	// Check if anything is dirty, in such a case, redraw debug.
	bool anything_changed = false;
	for (int i = 0; i < DIRTY_FLAGS_MAX; i++) {
		if (dirty.flags[i]) {
			anything_changed = true;
			break;
		}
	}

	// List all debug quadrants to update, creating new ones if needed.
	SelfList<DebugQuadrant>::List dirty_debug_quadrant_list;

	if (_debug_was_cleaned_up || anything_changed) {
		// Update all cells.
		for (KeyValue<Vector2i, CellData> &kv : tile_map_layer_data) {
			CellData &cell_data = kv.value;
			_debug_quadrants_update_cell(cell_data, dirty_debug_quadrant_list);
		}
	} else {
		// Update dirty cells.
		for (SelfList<CellData> *cell_data_list_element = dirty.cell_list.first(); cell_data_list_element; cell_data_list_element = cell_data_list_element->next()) {
			CellData &cell_data = *cell_data_list_element->self();
			_debug_quadrants_update_cell(cell_data, dirty_debug_quadrant_list);
		}
	}

	// Update those quadrants.
	bool needs_set_not_interpolated = is_inside_tree() && get_tree()->is_physics_interpolation_enabled() && !is_physics_interpolated();
	for (SelfList<DebugQuadrant> *quadrant_list_element = dirty_debug_quadrant_list.first(); quadrant_list_element;) {
		SelfList<DebugQuadrant> *next_quadrant_list_element = quadrant_list_element->next(); // "Hack" to clear the list while iterating.

		DebugQuadrant &debug_quadrant = *quadrant_list_element->self();

		// Check if the quadrant has a tile.
		bool has_a_tile = false;
		RID &ci = debug_quadrant.canvas_item;
		for (SelfList<CellData> *cell_data_list_element = debug_quadrant.cells.first(); cell_data_list_element; cell_data_list_element = cell_data_list_element->next()) {
			CellData &cell_data = *cell_data_list_element->self();
			if (cell_data.cell.source_id != TileSet::INVALID_SOURCE) {
				has_a_tile = true;
				break;
			}
		}

		if (has_a_tile) {
			// Update the quadrant.
			if (ci.is_valid()) {
				rs->canvas_item_clear(ci);
			} else {
				ci = rs->canvas_item_create();
				if (needs_set_not_interpolated) {
					rs->canvas_item_set_interpolated(ci, false);
				}
				rs->canvas_item_set_z_index(ci, RS::CANVAS_ITEM_Z_MAX - 1);
				rs->canvas_item_set_parent(ci, get_canvas_item());
			}

			const Vector2 quadrant_pos = tile_set->map_to_local(debug_quadrant.quadrant_coords * TILE_MAP_DEBUG_QUADRANT_SIZE);
			Transform2D xform(0, quadrant_pos);
			rs->canvas_item_set_transform(ci, xform);

			for (SelfList<CellData> *cell_data_list_element = debug_quadrant.cells.first(); cell_data_list_element; cell_data_list_element = cell_data_list_element->next()) {
				CellData &cell_data = *cell_data_list_element->self();
				if (cell_data.cell.source_id != TileSet::INVALID_SOURCE) {
					_rendering_draw_cell_debug(ci, quadrant_pos, cell_data);
					_physics_draw_cell_debug(ci, quadrant_pos, cell_data);
					_navigation_draw_cell_debug(ci, quadrant_pos, cell_data);
					_scenes_draw_cell_debug(ci, quadrant_pos, cell_data);
				}
			}
		} else {
			// Free the quadrant.
			if (ci.is_valid()) {
				rs->free(ci);
			}
			quadrant_list_element->remove_from_list();
			debug_quadrant_map.erase(debug_quadrant.quadrant_coords);
		}

		quadrant_list_element = next_quadrant_list_element;
	}

	dirty_debug_quadrant_list.clear();

	_debug_was_cleaned_up = false;
}

void TileMapLayer::_debug_quadrants_update_cell(CellData &r_cell_data, SelfList<DebugQuadrant>::List &r_dirty_debug_quadrant_list) {
	Vector2i quadrant_coords = _coords_to_debug_quadrant_coords(r_cell_data.coords);

	if (!debug_quadrant_map.has(quadrant_coords)) {
		// Create a new quadrant and add it to the quadrant map.
		Ref<DebugQuadrant> new_quadrant;
		new_quadrant.instantiate();
		new_quadrant->quadrant_coords = quadrant_coords;
		debug_quadrant_map[quadrant_coords] = new_quadrant;
	}

	// Add the cell to its quadrant, if it is not already in there.
	Ref<DebugQuadrant> &debug_quadrant = debug_quadrant_map[quadrant_coords];
	if (!r_cell_data.debug_quadrant_list_element.in_list()) {
		debug_quadrant->cells.add(&r_cell_data.debug_quadrant_list_element);
	}

	// Mark the quadrant as dirty.
	if (!debug_quadrant->dirty_quadrant_list_element.in_list()) {
		r_dirty_debug_quadrant_list.add(&debug_quadrant->dirty_quadrant_list_element);
	}
}
#endif // DEBUG_ENABLED

/////////////////////////////// Rendering //////////////////////////////////////
void TileMapLayer::_rendering_update(bool p_force_cleanup) {
	RenderingServer *rs = RenderingServer::get_singleton();

	// Check if we should cleanup everything.
	bool forced_cleanup = p_force_cleanup || !enabled || tile_set.is_null() || !is_visible_in_tree();

	// ----------- Layer level processing -----------
	if (!forced_cleanup) {
		// Modulate the layer.
		Color layer_modulate = get_modulate();
#ifdef TOOLS_ENABLED
		if (highlight_mode == HIGHLIGHT_MODE_BELOW) {
			layer_modulate = layer_modulate.darkened(0.5);
		} else if (highlight_mode == HIGHLIGHT_MODE_ABOVE) {
			layer_modulate = layer_modulate.darkened(0.5);
			layer_modulate.a *= 0.3;
		}
#endif // TOOLS_ENABLED
		rs->canvas_item_set_modulate(get_canvas_item(), layer_modulate);
	}

	// ----------- Quadrants processing -----------

	// List all rendering quadrants to update, creating new ones if needed.
	SelfList<RenderingQuadrant>::List dirty_rendering_quadrant_list;

	// Check if anything changed that might change the quadrant shape.
	// If so, recreate everything.
	bool quadrant_shape_changed = dirty.flags[DIRTY_FLAGS_LAYER_Y_SORT_ENABLED] || dirty.flags[DIRTY_FLAGS_TILE_SET] ||
			(is_y_sort_enabled() && (dirty.flags[DIRTY_FLAGS_LAYER_Y_SORT_ORIGIN] || dirty.flags[DIRTY_FLAGS_LAYER_X_DRAW_ORDER_REVERSED] || dirty.flags[DIRTY_FLAGS_LAYER_LOCAL_TRANSFORM])) ||
			(!is_y_sort_enabled() && dirty.flags[DIRTY_FLAGS_LAYER_RENDERING_QUADRANT_SIZE]);

	// Free all quadrants.
	if (forced_cleanup || quadrant_shape_changed) {
		for (const KeyValue<Vector2i, Ref<RenderingQuadrant>> &kv : rendering_quadrant_map) {
			for (const RID &ci : kv.value->canvas_items) {
				if (ci.is_valid()) {
					rs->free(ci);
				}
			}
			kv.value->cells.clear();
		}
		rendering_quadrant_map.clear();
		_rendering_was_cleaned_up = true;
	}

	if (!forced_cleanup) {
		// List all quadrants to update, recreating them if needed.
		if (dirty.flags[DIRTY_FLAGS_TILE_SET] || dirty.flags[DIRTY_FLAGS_LAYER_IN_TREE] || _rendering_was_cleaned_up) {
			// Update all cells.
			for (KeyValue<Vector2i, CellData> &kv : tile_map_layer_data) {
				CellData &cell_data = kv.value;
				_rendering_quadrants_update_cell(cell_data, dirty_rendering_quadrant_list);
			}
		} else {
			// Update dirty cells.
			for (SelfList<CellData> *cell_data_list_element = dirty.cell_list.first(); cell_data_list_element; cell_data_list_element = cell_data_list_element->next()) {
				CellData &cell_data = *cell_data_list_element->self();
				_rendering_quadrants_update_cell(cell_data, dirty_rendering_quadrant_list);
			}
		}

		// Update all dirty quadrants.
		bool needs_set_not_interpolated = is_inside_tree() && get_tree()->is_physics_interpolation_enabled() && !is_physics_interpolated();
		for (SelfList<RenderingQuadrant> *quadrant_list_element = dirty_rendering_quadrant_list.first(); quadrant_list_element;) {
			SelfList<RenderingQuadrant> *next_quadrant_list_element = quadrant_list_element->next(); // "Hack" to clear the list while iterating.

			const Ref<RenderingQuadrant> &rendering_quadrant = quadrant_list_element->self();

			// Check if the quadrant has a tile.
			bool has_a_tile = false;
			for (SelfList<CellData> *cell_data_list_element = rendering_quadrant->cells.first(); cell_data_list_element; cell_data_list_element = cell_data_list_element->next()) {
				CellData &cell_data = *cell_data_list_element->self();
				if (cell_data.cell.source_id != TileSet::INVALID_SOURCE) {
					has_a_tile = true;
					break;
				}
			}

			if (has_a_tile) {
				// Process the quadrant.

				// First, clear the quadrant's canvas items.
				for (RID &ci : rendering_quadrant->canvas_items) {
					rs->free(ci);
				}
				rendering_quadrant->canvas_items.clear();

				// Sort the quadrant cells.
				if (is_y_sort_enabled() && x_draw_order_reversed) {
					rendering_quadrant->cells.sort_custom<CellDataYSortedXReversedComparator>();
				} else {
					rendering_quadrant->cells.sort();
				}

				// Those allow to group cell per material or z-index.
				Ref<Material> prev_material;
				int prev_z_index = 0;
				RID prev_ci;

				for (SelfList<CellData> *cell_data_quadrant_list_element = rendering_quadrant->cells.first(); cell_data_quadrant_list_element; cell_data_quadrant_list_element = cell_data_quadrant_list_element->next()) {
					CellData &cell_data = *cell_data_quadrant_list_element->self();

					TileSetAtlasSource *atlas_source = Object::cast_to<TileSetAtlasSource>(*tile_set->get_source(cell_data.cell.source_id));

					// Get the tile data.
					const TileData *tile_data;
					if (cell_data.runtime_tile_data_cache) {
						tile_data = cell_data.runtime_tile_data_cache;
					} else {
						tile_data = atlas_source->get_tile_data(cell_data.cell.get_atlas_coords(), cell_data.cell.alternative_tile);
					}

					Ref<Material> mat = tile_data->get_material();
					int tile_z_index = tile_data->get_z_index();

					// Quandrant pos.

					// --- CanvasItems ---
					RID ci;

					// Check if the material or the z_index changed.
					if (prev_ci == RID() || prev_material != mat || prev_z_index != tile_z_index) {
						// If so, create a new CanvasItem.
						ci = rs->canvas_item_create();
						if (needs_set_not_interpolated) {
							rs->canvas_item_set_interpolated(ci, false);
						}
						if (mat.is_valid()) {
							rs->canvas_item_set_material(ci, mat->get_rid());
						}
						rs->canvas_item_set_parent(ci, get_canvas_item());
						rs->canvas_item_set_use_parent_material(ci, mat.is_null());

						Transform2D xform(0, rendering_quadrant->canvas_items_position);
						rs->canvas_item_set_transform(ci, xform);

						rs->canvas_item_set_light_mask(ci, get_light_mask());
						rs->canvas_item_set_z_as_relative_to_parent(ci, true);
						rs->canvas_item_set_z_index(ci, tile_z_index);

						rs->canvas_item_set_default_texture_filter(ci, RS::CanvasItemTextureFilter(get_texture_filter_in_tree()));
						rs->canvas_item_set_default_texture_repeat(ci, RS::CanvasItemTextureRepeat(get_texture_repeat_in_tree()));

						rendering_quadrant->canvas_items.push_back(ci);

						prev_ci = ci;
						prev_material = mat;
						prev_z_index = tile_z_index;

					} else {
						// Keep the same canvas_item to draw on.
						ci = prev_ci;
					}

					const Vector2 local_tile_pos = tile_set->map_to_local(cell_data.coords);

					// Random animation offset.
					real_t random_animation_offset = 0.0;
					if (atlas_source->get_tile_animation_mode(cell_data.cell.get_atlas_coords()) != TileSetAtlasSource::TILE_ANIMATION_MODE_DEFAULT) {
						Array to_hash;
						to_hash.push_back(local_tile_pos);
						to_hash.push_back(get_instance_id()); // Use instance id as a random hash
						random_animation_offset = RandomPCG(to_hash.hash()).randf();
					}

					// Drawing the tile in the canvas item.
					draw_tile(ci, local_tile_pos - rendering_quadrant->canvas_items_position, tile_set, cell_data.cell.source_id, cell_data.cell.get_atlas_coords(), cell_data.cell.alternative_tile, -1, get_self_modulate(), tile_data, random_animation_offset);
				}

				// Reset physics interpolation for any recreated canvas items.
				if (is_physics_interpolated_and_enabled() && is_visible_in_tree()) {
					for (const RID &ci : rendering_quadrant->canvas_items) {
						rs->canvas_item_reset_physics_interpolation(ci);
					}
				}

			} else {
				// Free the quadrant.
				for (const RID &ci : rendering_quadrant->canvas_items) {
					if (ci.is_valid()) {
						rs->free(ci);
					}
				}
				rendering_quadrant->cells.clear();
				rendering_quadrant_map.erase(rendering_quadrant->quadrant_coords);
			}

			quadrant_list_element = next_quadrant_list_element;
		}

		dirty_rendering_quadrant_list.clear();

		// Reset the drawing indices.
		{
			int index = -(int64_t)0x80000000; // Always must be drawn below children.

			// Sort the quadrants coords per local coordinates.
			RBMap<Vector2, Ref<RenderingQuadrant>, RenderingQuadrant::CoordsWorldComparator> local_to_map;
			for (KeyValue<Vector2i, Ref<RenderingQuadrant>> &kv : rendering_quadrant_map) {
				Ref<RenderingQuadrant> &rendering_quadrant = kv.value;
				local_to_map[tile_set->map_to_local(rendering_quadrant->quadrant_coords)] = rendering_quadrant;
			}

			// Sort the quadrants.
			for (const KeyValue<Vector2, Ref<RenderingQuadrant>> &E : local_to_map) {
				for (const RID &ci : E.value->canvas_items) {
					RS::get_singleton()->canvas_item_set_draw_index(ci, index++);
				}
			}
		}

		// Updates on rendering changes.
		if (dirty.flags[DIRTY_FLAGS_LAYER_LIGHT_MASK] ||
				dirty.flags[DIRTY_FLAGS_LAYER_TEXTURE_FILTER] ||
				dirty.flags[DIRTY_FLAGS_LAYER_TEXTURE_REPEAT] ||
				dirty.flags[DIRTY_FLAGS_LAYER_SELF_MODULATE]) {
			for (KeyValue<Vector2i, Ref<RenderingQuadrant>> &kv : rendering_quadrant_map) {
				Ref<RenderingQuadrant> &rendering_quadrant = kv.value;
				for (const RID &ci : rendering_quadrant->canvas_items) {
					rs->canvas_item_set_light_mask(ci, get_light_mask());
					rs->canvas_item_set_default_texture_filter(ci, RS::CanvasItemTextureFilter(get_texture_filter_in_tree()));
					rs->canvas_item_set_default_texture_repeat(ci, RS::CanvasItemTextureRepeat(get_texture_repeat_in_tree()));
					rs->canvas_item_set_self_modulate(ci, get_self_modulate());
				}
			}
		}
	}

	// ----------- Occluders processing -----------
	if (forced_cleanup || !occlusion_enabled) {
		// Clean everything.
		for (KeyValue<Vector2i, CellData> &kv : tile_map_layer_data) {
			_rendering_occluders_clear_cell(kv.value);
		}
	} else {
		if (_rendering_was_cleaned_up || dirty.flags[DIRTY_FLAGS_TILE_SET]) {
			// Update all cells.
			for (KeyValue<Vector2i, CellData> &kv : tile_map_layer_data) {
				_rendering_occluders_update_cell(kv.value);
			}
		} else {
			// Update dirty cells.
			for (SelfList<CellData> *cell_data_list_element = dirty.cell_list.first(); cell_data_list_element; cell_data_list_element = cell_data_list_element->next()) {
				CellData &cell_data = *cell_data_list_element->self();
				_rendering_occluders_update_cell(cell_data);
			}
		}
	}

	// -----------
	// Mark the rendering state as up to date.
	_rendering_was_cleaned_up = forced_cleanup || !occlusion_enabled;
}

void TileMapLayer::_rendering_notification(int p_what) {
	RenderingServer *rs = RenderingServer::get_singleton();
	if (p_what == NOTIFICATION_TRANSFORM_CHANGED || p_what == NOTIFICATION_ENTER_CANVAS || p_what == NOTIFICATION_VISIBILITY_CHANGED) {
		if (tile_set.is_valid()) {
			Transform2D tilemap_xform = get_global_transform();
			for (KeyValue<Vector2i, CellData> &kv : tile_map_layer_data) {
				const CellData &cell_data = kv.value;
				for (const LocalVector<RID> &polygons : cell_data.occluders) {
					for (const RID &rid : polygons) {
						if (rid.is_null()) {
							continue;
						}
						Transform2D xform(0, tile_set->map_to_local(kv.key));
						rs->canvas_light_occluder_attach_to_canvas(rid, get_canvas());
						rs->canvas_light_occluder_set_transform(rid, tilemap_xform * xform);
					}
				}
			}
		}
	} else if (p_what == NOTIFICATION_RESET_PHYSICS_INTERPOLATION) {
		if (is_physics_interpolated_and_enabled() && is_visible_in_tree()) {
			for (const KeyValue<Vector2i, Ref<RenderingQuadrant>> &kv : rendering_quadrant_map) {
				for (const RID &ci : kv.value->canvas_items) {
					if (ci.is_valid()) {
						rs->canvas_item_reset_physics_interpolation(ci);
					}
				}
			}
		}
	}
}

void TileMapLayer::_rendering_quadrants_update_cell(CellData &r_cell_data, SelfList<RenderingQuadrant>::List &r_dirty_rendering_quadrant_list) {
	// Check if the cell is valid and retrieve its y_sort_origin.
	bool is_valid = false;
	int tile_y_sort_origin = 0;
	TileSetSource *source;
	if (tile_set->has_source(r_cell_data.cell.source_id)) {
		source = *tile_set->get_source(r_cell_data.cell.source_id);
		TileSetAtlasSource *atlas_source = Object::cast_to<TileSetAtlasSource>(source);
		if (atlas_source && atlas_source->has_tile(r_cell_data.cell.get_atlas_coords()) && atlas_source->has_alternative_tile(r_cell_data.cell.get_atlas_coords(), r_cell_data.cell.alternative_tile)) {
			is_valid = true;
			const TileData *tile_data;
			if (r_cell_data.runtime_tile_data_cache) {
				tile_data = r_cell_data.runtime_tile_data_cache;
			} else {
				tile_data = atlas_source->get_tile_data(r_cell_data.cell.get_atlas_coords(), r_cell_data.cell.alternative_tile);
			}
			tile_y_sort_origin = tile_data->get_y_sort_origin();
		}
	}

	if (is_valid) {
		// Get the quadrant coords.
		Vector2 canvas_items_position;
		Vector2i quadrant_coords;
		if (is_y_sort_enabled()) {
			canvas_items_position = Vector2(0, tile_set->map_to_local(r_cell_data.coords).y + tile_y_sort_origin + y_sort_origin);
			quadrant_coords = canvas_items_position * 100;
		} else {
			const Vector2i &coords = r_cell_data.coords;

			// Rounding down, instead of simply rounding towards zero (truncating).
			quadrant_coords = Vector2i(
					coords.x > 0 ? coords.x / rendering_quadrant_size : (coords.x - (rendering_quadrant_size - 1)) / rendering_quadrant_size,
					coords.y > 0 ? coords.y / rendering_quadrant_size : (coords.y - (rendering_quadrant_size - 1)) / rendering_quadrant_size);
			canvas_items_position = tile_set->map_to_local(rendering_quadrant_size * quadrant_coords);
		}

		Ref<RenderingQuadrant> rendering_quadrant;
		if (rendering_quadrant_map.has(quadrant_coords)) {
			// Reuse existing rendering quadrant.
			rendering_quadrant = rendering_quadrant_map[quadrant_coords];
		} else {
			// Create a new rendering quadrant.
			rendering_quadrant.instantiate();
			rendering_quadrant->quadrant_coords = quadrant_coords;
			rendering_quadrant->canvas_items_position = canvas_items_position;
			rendering_quadrant_map[quadrant_coords] = rendering_quadrant;
		}

		// Mark the old quadrant as dirty (if it exists).
		if (r_cell_data.rendering_quadrant.is_valid()) {
			if (!r_cell_data.rendering_quadrant->dirty_quadrant_list_element.in_list()) {
				r_dirty_rendering_quadrant_list.add(&r_cell_data.rendering_quadrant->dirty_quadrant_list_element);
			}
		}

		// Remove the cell from that quadrant.
		if (r_cell_data.rendering_quadrant_list_element.in_list()) {
			r_cell_data.rendering_quadrant_list_element.remove_from_list();
		}

		// Add the cell to its new quadrant.
		r_cell_data.rendering_quadrant = rendering_quadrant;
		r_cell_data.rendering_quadrant->cells.add(&r_cell_data.rendering_quadrant_list_element);

		// Add the new quadrant to the dirty quadrant list.
		if (!rendering_quadrant->dirty_quadrant_list_element.in_list()) {
			r_dirty_rendering_quadrant_list.add(&rendering_quadrant->dirty_quadrant_list_element);
		}
	} else {
		Ref<RenderingQuadrant> rendering_quadrant = r_cell_data.rendering_quadrant;

		// Remove the cell from its quadrant.
		r_cell_data.rendering_quadrant = Ref<RenderingQuadrant>();
		if (r_cell_data.rendering_quadrant_list_element.in_list()) {
			rendering_quadrant->cells.remove(&r_cell_data.rendering_quadrant_list_element);
		}

		if (rendering_quadrant.is_valid()) {
			// Add the quadrant to the dirty quadrant list.
			if (!rendering_quadrant->dirty_quadrant_list_element.in_list()) {
				r_dirty_rendering_quadrant_list.add(&rendering_quadrant->dirty_quadrant_list_element);
			}
		}
	}
}

void TileMapLayer::_rendering_occluders_clear_cell(CellData &r_cell_data) {
	RenderingServer *rs = RenderingServer::get_singleton();

	// Free the occluders.
	for (const LocalVector<RID> &polygons : r_cell_data.occluders) {
		for (const RID &rid : polygons) {
			rs->free(rid);
		}
	}
	r_cell_data.occluders.clear();
}

void TileMapLayer::_rendering_occluders_update_cell(CellData &r_cell_data) {
	RenderingServer *rs = RenderingServer::get_singleton();

	// Free unused occluders then resize the occluder array.
	for (uint32_t i = tile_set->get_occlusion_layers_count(); i < r_cell_data.occluders.size(); i++) {
		for (const RID &occluder_id : r_cell_data.occluders[i]) {
			if (occluder_id.is_valid()) {
				rs->free(occluder_id);
			}
		}
	}
	r_cell_data.occluders.resize(tile_set->get_occlusion_layers_count());

	TileSetSource *source;
	if (tile_set->has_source(r_cell_data.cell.source_id)) {
		source = *tile_set->get_source(r_cell_data.cell.source_id);

		if (source->has_tile(r_cell_data.cell.get_atlas_coords()) && source->has_alternative_tile(r_cell_data.cell.get_atlas_coords(), r_cell_data.cell.alternative_tile)) {
			TileSetAtlasSource *atlas_source = Object::cast_to<TileSetAtlasSource>(source);
			if (atlas_source) {
				// Get the tile data.
				const TileData *tile_data;
				if (r_cell_data.runtime_tile_data_cache) {
					tile_data = r_cell_data.runtime_tile_data_cache;
				} else {
					tile_data = atlas_source->get_tile_data(r_cell_data.cell.get_atlas_coords(), r_cell_data.cell.alternative_tile);
				}

				// Transform flags.
				bool flip_h = (r_cell_data.cell.alternative_tile & TileSetAtlasSource::TRANSFORM_FLIP_H);
				bool flip_v = (r_cell_data.cell.alternative_tile & TileSetAtlasSource::TRANSFORM_FLIP_V);
				bool transpose = (r_cell_data.cell.alternative_tile & TileSetAtlasSource::TRANSFORM_TRANSPOSE);

				// Create, update or clear occluders.
				bool needs_set_not_interpolated = is_inside_tree() && get_tree()->is_physics_interpolation_enabled() && !is_physics_interpolated();
				for (uint32_t occlusion_layer_index = 0; occlusion_layer_index < r_cell_data.occluders.size(); occlusion_layer_index++) {
					LocalVector<RID> &occluders = r_cell_data.occluders[occlusion_layer_index];

					// Free unused occluders then resize the occluders array.
					for (uint32_t i = tile_data->get_occluder_polygons_count(occlusion_layer_index); i < r_cell_data.occluders[occlusion_layer_index].size(); i++) {
						RID occluder_id = occluders[i];
						if (occluder_id.is_valid()) {
							rs->free(occluder_id);
						}
					}
					occluders.resize(tile_data->get_occluder_polygons_count(occlusion_layer_index));

					for (uint32_t occlusion_polygon_index = 0; occlusion_polygon_index < occluders.size(); occlusion_polygon_index++) {
						RID &occluder = occluders[occlusion_polygon_index];
						Ref<OccluderPolygon2D> occluder_polygon = tile_data->get_occluder_polygon(occlusion_layer_index, occlusion_polygon_index);
						if (occluder_polygon.is_valid()) {
							// Create or update occluder.

							Transform2D xform;
							xform.set_origin(tile_set->map_to_local(r_cell_data.coords));
							if (!occluder.is_valid()) {
								occluder = rs->canvas_light_occluder_create();
								if (needs_set_not_interpolated) {
									rs->canvas_light_occluder_set_interpolated(occluder, false);
								}
							}
							rs->canvas_light_occluder_set_transform(occluder, get_global_transform() * xform);
							rs->canvas_light_occluder_set_polygon(occluder, tile_data->get_occluder_polygon(occlusion_layer_index, occlusion_polygon_index, flip_h, flip_v, transpose)->get_rid());
							rs->canvas_light_occluder_attach_to_canvas(occluder, get_canvas());
							rs->canvas_light_occluder_set_light_mask(occluder, tile_set->get_occlusion_layer_light_mask(occlusion_layer_index));
							rs->canvas_light_occluder_set_as_sdf_collision(occluder, tile_set->get_occlusion_layer_sdf_collision(occlusion_layer_index));
						} else {
							// Clear occluder.
							if (occluder.is_valid()) {
								rs->free(occluder);
								occluder = RID();
							}
						}
					}
				}

				return;
			}
		}
	}

	// If we did not return earlier, clear the cell.
	_rendering_occluders_clear_cell(r_cell_data);
}

#ifdef DEBUG_ENABLED
void TileMapLayer::_rendering_draw_cell_debug(const RID &p_canvas_item, const Vector2 &p_quadrant_pos, const CellData &r_cell_data) {
	ERR_FAIL_COND(tile_set.is_null());

	if (!Engine::get_singleton()->is_editor_hint()) {
		return;
	}

	// Draw a placeholder for tiles needing one.
	RenderingServer *rs = RenderingServer::get_singleton();
	const TileMapCell &c = r_cell_data.cell;

	TileSetSource *source;
	if (tile_set->has_source(c.source_id)) {
		source = *tile_set->get_source(c.source_id);

		if (source->has_tile(c.get_atlas_coords()) && source->has_alternative_tile(c.get_atlas_coords(), c.alternative_tile)) {
			TileSetAtlasSource *atlas_source = Object::cast_to<TileSetAtlasSource>(source);
			if (atlas_source) {
				Vector2i grid_size = atlas_source->get_atlas_grid_size();
				if (atlas_source->get_runtime_texture().is_null() || c.get_atlas_coords().x >= grid_size.x || c.get_atlas_coords().y >= grid_size.y) {
					// Generate a random color from the hashed values of the tiles.
					Array to_hash;
					to_hash.push_back(c.source_id);
					to_hash.push_back(c.get_atlas_coords());
					to_hash.push_back(c.alternative_tile);
					uint32_t hash = RandomPCG(to_hash.hash()).rand();

					Color color;
					color = color.from_hsv(
							(float)((hash >> 24) & 0xFF) / 256.0,
							Math::lerp(0.5, 1.0, (float)((hash >> 16) & 0xFF) / 256.0),
							Math::lerp(0.5, 1.0, (float)((hash >> 8) & 0xFF) / 256.0),
							0.8);

					// Draw a placeholder tile.
					Transform2D cell_to_quadrant;
					cell_to_quadrant.set_origin(tile_set->map_to_local(r_cell_data.coords) - p_quadrant_pos);
					rs->canvas_item_add_set_transform(p_canvas_item, cell_to_quadrant);
					rs->canvas_item_add_circle(p_canvas_item, Vector2(), MIN(tile_set->get_tile_size().x, tile_set->get_tile_size().y) / 4.0, color);
				}
			}
		}
	}
}
#endif // DEBUG_ENABLED

/////////////////////////////// Physics //////////////////////////////////////

void TileMapLayer::_physics_update(bool p_force_cleanup) {
	// Check if we should cleanup everything.
	bool forced_cleanup = p_force_cleanup || !enabled || !collision_enabled || !is_inside_tree() || tile_set.is_null();
	if (forced_cleanup) {
		// Clean everything.
		for (KeyValue<Vector2i, CellData> &kv : tile_map_layer_data) {
			_physics_clear_cell(kv.value);
		}
	} else {
		if (_physics_was_cleaned_up || dirty.flags[DIRTY_FLAGS_TILE_SET] || dirty.flags[DIRTY_FLAGS_LAYER_USE_KINEMATIC_BODIES] || dirty.flags[DIRTY_FLAGS_LAYER_IN_TREE]) {
			// Update all cells.
			for (KeyValue<Vector2i, CellData> &kv : tile_map_layer_data) {
				_physics_update_cell(kv.value);
			}
		} else {
			// Update dirty cells.
			for (SelfList<CellData> *cell_data_list_element = dirty.cell_list.first(); cell_data_list_element; cell_data_list_element = cell_data_list_element->next()) {
				CellData &cell_data = *cell_data_list_element->self();
				_physics_update_cell(cell_data);
			}
		}
	}

	// -----------
	// Mark the physics state as up to date.
	_physics_was_cleaned_up = forced_cleanup;
}

void TileMapLayer::_physics_notification(int p_what) {
	Transform2D gl_transform = get_global_transform();
	PhysicsServer2D *ps = PhysicsServer2D::get_singleton();

	switch (p_what) {
		case NOTIFICATION_TRANSFORM_CHANGED:
			// Move the collisison shapes along with the TileMap.
			if (is_inside_tree() && tile_set.is_valid()) {
				for (KeyValue<Vector2i, CellData> &kv : tile_map_layer_data) {
					const CellData &cell_data = kv.value;

					for (RID body : cell_data.bodies) {
						if (body.is_valid()) {
							Transform2D xform(0, tile_set->map_to_local(kv.key));
							xform = gl_transform * xform;
							ps->body_set_state(body, PhysicsServer2D::BODY_STATE_TRANSFORM, xform);
						}
					}
				}
			}
			break;
		case NOTIFICATION_ENTER_TREE:
			// Changes in the tree may cause the space to change (e.g. when reparenting to a SubViewport).
			if (is_inside_tree()) {
				RID space = get_world_2d()->get_space();

				for (KeyValue<Vector2i, CellData> &kv : tile_map_layer_data) {
					const CellData &cell_data = kv.value;

					for (RID body : cell_data.bodies) {
						if (body.is_valid()) {
							ps->body_set_space(body, space);
						}
					}
				}
			}
	}
}

void TileMapLayer::_physics_clear_cell(CellData &r_cell_data) {
	PhysicsServer2D *ps = PhysicsServer2D::get_singleton();

	// Clear bodies.
	for (RID body : r_cell_data.bodies) {
		if (body.is_valid()) {
			bodies_coords.erase(body);
			ps->free(body);
		}
	}
	r_cell_data.bodies.clear();
}

void TileMapLayer::_physics_update_cell(CellData &r_cell_data) {
	Transform2D gl_transform = get_global_transform();
	RID space = get_world_2d()->get_space();
	PhysicsServer2D *ps = PhysicsServer2D::get_singleton();

	// Recreate bodies and shapes.
	TileMapCell &c = r_cell_data.cell;

	TileSetSource *source;
	if (tile_set->has_source(c.source_id)) {
		source = *tile_set->get_source(c.source_id);

		if (source->has_tile(c.get_atlas_coords()) && source->has_alternative_tile(c.get_atlas_coords(), c.alternative_tile)) {
			TileSetAtlasSource *atlas_source = Object::cast_to<TileSetAtlasSource>(source);
			if (atlas_source) {
				const TileData *tile_data;
				if (r_cell_data.runtime_tile_data_cache) {
					tile_data = r_cell_data.runtime_tile_data_cache;
				} else {
					tile_data = atlas_source->get_tile_data(c.get_atlas_coords(), c.alternative_tile);
				}

				// Transform flags.
				bool flip_h = (c.alternative_tile & TileSetAtlasSource::TRANSFORM_FLIP_H);
				bool flip_v = (c.alternative_tile & TileSetAtlasSource::TRANSFORM_FLIP_V);
				bool transpose = (c.alternative_tile & TileSetAtlasSource::TRANSFORM_TRANSPOSE);

				// Free unused bodies then resize the bodies array.
				for (uint32_t i = tile_set->get_physics_layers_count(); i < r_cell_data.bodies.size(); i++) {
					RID &body = r_cell_data.bodies[i];
					if (body.is_valid()) {
						bodies_coords.erase(body);
						ps->free(body);
						body = RID();
					}
				}
				r_cell_data.bodies.resize(tile_set->get_physics_layers_count());

				for (uint32_t tile_set_physics_layer = 0; tile_set_physics_layer < (uint32_t)tile_set->get_physics_layers_count(); tile_set_physics_layer++) {
					Ref<PhysicsMaterial> physics_material = tile_set->get_physics_layer_physics_material(tile_set_physics_layer);
					uint32_t physics_layer = tile_set->get_physics_layer_collision_layer(tile_set_physics_layer);
					uint32_t physics_mask = tile_set->get_physics_layer_collision_mask(tile_set_physics_layer);
					real_t physics_priority = tile_set->get_physics_layer_collision_priority(tile_set_physics_layer);

					RID body = r_cell_data.bodies[tile_set_physics_layer];
					if (tile_data->get_collision_polygons_count(tile_set_physics_layer) == 0) {
						// No body needed, free it if it exists.
						if (body.is_valid()) {
							bodies_coords.erase(body);
							ps->free(body);
						}
						body = RID();
					} else {
						// Create or update the body.
						if (!body.is_valid()) {
							body = ps->body_create();
						}
						bodies_coords[body] = r_cell_data.coords;
						ps->body_set_mode(body, use_kinematic_bodies ? PhysicsServer2D::BODY_MODE_KINEMATIC : PhysicsServer2D::BODY_MODE_STATIC);
						ps->body_set_space(body, space);

						Transform2D xform;
						xform.set_origin(tile_set->map_to_local(r_cell_data.coords));
						xform = gl_transform * xform;
						ps->body_set_state(body, PhysicsServer2D::BODY_STATE_TRANSFORM, xform);

						ps->body_attach_object_instance_id(body, tile_map_node ? tile_map_node->get_instance_id() : get_instance_id());
						ps->body_set_collision_layer(body, physics_layer);
						ps->body_set_collision_mask(body, physics_mask);
						ps->body_set_collision_priority(body, physics_priority);
						ps->body_set_pickable(body, false);
						ps->body_set_state(body, PhysicsServer2D::BODY_STATE_LINEAR_VELOCITY, tile_data->get_constant_linear_velocity(tile_set_physics_layer));
						ps->body_set_state(body, PhysicsServer2D::BODY_STATE_ANGULAR_VELOCITY, tile_data->get_constant_angular_velocity(tile_set_physics_layer));

						if (physics_material.is_null()) {
							ps->body_set_param(body, PhysicsServer2D::BODY_PARAM_BOUNCE, 0);
							ps->body_set_param(body, PhysicsServer2D::BODY_PARAM_FRICTION, 1);
						} else {
							ps->body_set_param(body, PhysicsServer2D::BODY_PARAM_BOUNCE, physics_material->computed_bounce());
							ps->body_set_param(body, PhysicsServer2D::BODY_PARAM_FRICTION, physics_material->computed_friction());
						}

						// Clear body's shape if needed.
						ps->body_clear_shapes(body);

						// Add the shapes to the body.
						int body_shape_index = 0;
						for (int polygon_index = 0; polygon_index < tile_data->get_collision_polygons_count(tile_set_physics_layer); polygon_index++) {
							// Iterate over the polygons.
							bool one_way_collision = tile_data->is_collision_polygon_one_way(tile_set_physics_layer, polygon_index);
							float one_way_collision_margin = tile_data->get_collision_polygon_one_way_margin(tile_set_physics_layer, polygon_index);
							int shapes_count = tile_data->get_collision_polygon_shapes_count(tile_set_physics_layer, polygon_index);
							for (int shape_index = 0; shape_index < shapes_count; shape_index++) {
								// Add decomposed convex shapes.
								Ref<ConvexPolygonShape2D> shape = tile_data->get_collision_polygon_shape(tile_set_physics_layer, polygon_index, shape_index, flip_h, flip_v, transpose);
								ps->body_add_shape(body, shape->get_rid());
								ps->body_set_shape_as_one_way_collision(body, body_shape_index, one_way_collision, one_way_collision_margin);

								body_shape_index++;
							}
						}
					}

					// Set the body again.
					r_cell_data.bodies[tile_set_physics_layer] = body;
				}

				return;
			}
		}
	}

	// If we did not return earlier, clear the cell.
	_physics_clear_cell(r_cell_data);
}

#ifdef DEBUG_ENABLED
void TileMapLayer::_physics_draw_cell_debug(const RID &p_canvas_item, const Vector2 &p_quadrant_pos, const CellData &r_cell_data) {
	// Draw the debug collision shapes.
	ERR_FAIL_COND(tile_set.is_null());

	if (!get_tree()) {
		return;
	}

	bool show_collision = false;
	switch (collision_visibility_mode) {
		case TileMapLayer::DEBUG_VISIBILITY_MODE_DEFAULT:
			show_collision = !Engine::get_singleton()->is_editor_hint() && get_tree()->is_debugging_collisions_hint();
			break;
		case TileMapLayer::DEBUG_VISIBILITY_MODE_FORCE_HIDE:
			show_collision = false;
			break;
		case TileMapLayer::DEBUG_VISIBILITY_MODE_FORCE_SHOW:
			show_collision = true;
			break;
	}
	if (!show_collision) {
		return;
	}

	RenderingServer *rs = RenderingServer::get_singleton();
	PhysicsServer2D *ps = PhysicsServer2D::get_singleton();

	Color debug_collision_color = get_tree()->get_debug_collisions_color();
	Vector<Color> color;
	color.push_back(debug_collision_color);

	Transform2D quadrant_to_local(0, p_quadrant_pos);
	Transform2D global_to_quadrant = (get_global_transform() * quadrant_to_local).affine_inverse();

	for (RID body : r_cell_data.bodies) {
		if (body.is_valid()) {
			Transform2D body_to_quadrant = global_to_quadrant * Transform2D(ps->body_get_state(body, PhysicsServer2D::BODY_STATE_TRANSFORM));
			rs->canvas_item_add_set_transform(p_canvas_item, body_to_quadrant);
			for (int shape_index = 0; shape_index < ps->body_get_shape_count(body); shape_index++) {
				const RID &shape = ps->body_get_shape(body, shape_index);
				const PhysicsServer2D::ShapeType &type = ps->shape_get_type(shape);
				if (type == PhysicsServer2D::SHAPE_CONVEX_POLYGON) {
					rs->canvas_item_add_polygon(p_canvas_item, ps->shape_get_data(shape), color);
				} else {
					WARN_PRINT("Wrong shape type for a tile, should be SHAPE_CONVEX_POLYGON.");
				}
			}
			rs->canvas_item_add_set_transform(p_canvas_item, Transform2D());
		}
	}
}
#endif // DEBUG_ENABLED

/////////////////////////////// Navigation //////////////////////////////////////

void TileMapLayer::_navigation_update(bool p_force_cleanup) {
	ERR_FAIL_NULL(NavigationServer2D::get_singleton());
	NavigationServer2D *ns = NavigationServer2D::get_singleton();

	// Check if we should cleanup everything.
	bool forced_cleanup = p_force_cleanup || !enabled || !navigation_enabled || !is_inside_tree() || tile_set.is_null();

	// ----------- Layer level processing -----------
	// All this processing is kept for compatibility with the TileMap node.
	// Otherwise, layers shall use the World2D navigation map or define a custom one with set_navigation_map(...).
	if (tile_map_node) {
		if (forced_cleanup) {
			if (navigation_map_override.is_valid()) {
				ns->free(navigation_map_override);
				navigation_map_override = RID();
			}
		} else {
			// Update navigation maps.
			if (!navigation_map_override.is_valid()) {
				if (layer_index_in_tile_map_node > 0) {
					// Create a dedicated map for each layer.
					RID new_layer_map = ns->map_create();
					// Set the default NavigationPolygon cell_size on the new map as a mismatch causes an error.
					ns->map_set_cell_size(new_layer_map, NavigationDefaults2D::navmesh_cell_size);
					ns->map_set_active(new_layer_map, true);
					navigation_map_override = new_layer_map;
				}
			}
		}
	}

	// ----------- Navigation regions processing -----------
	if (forced_cleanup) {
		// Clean everything.
		for (KeyValue<Vector2i, CellData> &kv : tile_map_layer_data) {
			_navigation_clear_cell(kv.value);
		}
	} else {
		if (_navigation_was_cleaned_up || dirty.flags[DIRTY_FLAGS_TILE_SET] || dirty.flags[DIRTY_FLAGS_LAYER_IN_TREE] || dirty.flags[DIRTY_FLAGS_LAYER_NAVIGATION_MAP]) {
			// Update all cells.
			for (KeyValue<Vector2i, CellData> &kv : tile_map_layer_data) {
				_navigation_update_cell(kv.value);
			}
		} else {
			// Update dirty cells.
			for (SelfList<CellData> *cell_data_list_element = dirty.cell_list.first(); cell_data_list_element; cell_data_list_element = cell_data_list_element->next()) {
				CellData &cell_data = *cell_data_list_element->self();
				_navigation_update_cell(cell_data);
			}
		}
	}

	// -----------
	// Mark the navigation state as up to date.
	_navigation_was_cleaned_up = forced_cleanup;
}

void TileMapLayer::_navigation_notification(int p_what) {
	if (p_what == NOTIFICATION_TRANSFORM_CHANGED) {
		if (tile_set.is_valid()) {
			Transform2D tilemap_xform = get_global_transform();
			for (KeyValue<Vector2i, CellData> &kv : tile_map_layer_data) {
				const CellData &cell_data = kv.value;
				// Update navigation regions transform.
				for (const RID &region : cell_data.navigation_regions) {
					if (!region.is_valid()) {
						continue;
					}
					Transform2D tile_transform;
					tile_transform.set_origin(tile_set->map_to_local(kv.key));
					NavigationServer2D::get_singleton()->region_set_transform(region, tilemap_xform * tile_transform);
				}
			}
		}
	}
}

void TileMapLayer::_navigation_clear_cell(CellData &r_cell_data) {
	NavigationServer2D *ns = NavigationServer2D::get_singleton();
	// Clear navigation shapes.
	for (uint32_t i = 0; i < r_cell_data.navigation_regions.size(); i++) {
		const RID &region = r_cell_data.navigation_regions[i];
		if (region.is_valid()) {
			ns->region_set_map(region, RID());
			ns->free(region);
		}
	}
	r_cell_data.navigation_regions.clear();
}

void TileMapLayer::_navigation_update_cell(CellData &r_cell_data) {
	NavigationServer2D *ns = NavigationServer2D::get_singleton();
	Transform2D gl_xform = get_global_transform();
	RID navigation_map = navigation_map_override.is_valid() ? navigation_map_override : get_world_2d()->get_navigation_map();
	ERR_FAIL_COND(navigation_map.is_null());

	// Get the navigation polygons and create regions.
	TileMapCell &c = r_cell_data.cell;

	TileSetSource *source;
	if (tile_set->has_source(c.source_id)) {
		source = *tile_set->get_source(c.source_id);

		if (source->has_tile(c.get_atlas_coords()) && source->has_alternative_tile(c.get_atlas_coords(), c.alternative_tile)) {
			TileSetAtlasSource *atlas_source = Object::cast_to<TileSetAtlasSource>(source);
			if (atlas_source) {
				const TileData *tile_data;
				if (r_cell_data.runtime_tile_data_cache) {
					tile_data = r_cell_data.runtime_tile_data_cache;
				} else {
					tile_data = atlas_source->get_tile_data(c.get_atlas_coords(), c.alternative_tile);
				}

				// Transform flags.
				bool flip_h = (c.alternative_tile & TileSetAtlasSource::TRANSFORM_FLIP_H);
				bool flip_v = (c.alternative_tile & TileSetAtlasSource::TRANSFORM_FLIP_V);
				bool transpose = (c.alternative_tile & TileSetAtlasSource::TRANSFORM_TRANSPOSE);

				// Free unused regions then resize the regions array.
				for (uint32_t i = tile_set->get_navigation_layers_count(); i < r_cell_data.navigation_regions.size(); i++) {
					RID &region = r_cell_data.navigation_regions[i];
					if (region.is_valid()) {
						ns->region_set_map(region, RID());
						ns->free(region);
						region = RID();
					}
				}
				r_cell_data.navigation_regions.resize(tile_set->get_navigation_layers_count());

				// Create, update or clear regions.
				for (uint32_t navigation_layer_index = 0; navigation_layer_index < r_cell_data.navigation_regions.size(); navigation_layer_index++) {
					Ref<NavigationPolygon> navigation_polygon = tile_data->get_navigation_polygon(navigation_layer_index, flip_h, flip_v, transpose);

					RID &region = r_cell_data.navigation_regions[navigation_layer_index];

					if (navigation_polygon.is_valid() && (navigation_polygon->get_polygon_count() > 0 || navigation_polygon->get_outline_count() > 0)) {
						// Create or update regions.
						Transform2D tile_transform;
						tile_transform.set_origin(tile_set->map_to_local(r_cell_data.coords));
						if (!region.is_valid()) {
							region = ns->region_create();
						}
						ns->region_set_owner_id(region, tile_map_node ? tile_map_node->get_instance_id() : get_instance_id());
						ns->region_set_map(region, navigation_map);
						ns->region_set_transform(region, gl_xform * tile_transform);
						ns->region_set_navigation_layers(region, tile_set->get_navigation_layer_layers(navigation_layer_index));
						ns->region_set_navigation_polygon(region, navigation_polygon);
					} else {
						// Clear region.
						if (region.is_valid()) {
							ns->region_set_map(region, RID());
							ns->free(region);
							region = RID();
						}
					}
				}

				return;
			}
		}
	}

	// If we did not return earlier, clear the cell.
	_navigation_clear_cell(r_cell_data);
}

#ifdef DEBUG_ENABLED
void TileMapLayer::_navigation_draw_cell_debug(const RID &p_canvas_item, const Vector2 &p_quadrant_pos, const CellData &r_cell_data) {
	// Draw the debug collision shapes.
	bool show_navigation = false;
	switch (navigation_visibility_mode) {
		case TileMapLayer::DEBUG_VISIBILITY_MODE_DEFAULT:
			show_navigation = !Engine::get_singleton()->is_editor_hint() && get_tree()->is_debugging_navigation_hint();
			break;
		case TileMapLayer::DEBUG_VISIBILITY_MODE_FORCE_HIDE:
			show_navigation = false;
			break;
		case TileMapLayer::DEBUG_VISIBILITY_MODE_FORCE_SHOW:
			show_navigation = true;
			break;
	}
	if (!show_navigation) {
		return;
	}

	// Check if the navigation is used.
	if (r_cell_data.navigation_regions.is_empty()) {
		return;
	}

	RenderingServer *rs = RenderingServer::get_singleton();
	const NavigationServer2D *ns2d = NavigationServer2D::get_singleton();

	bool enabled_geometry_face_random_color = ns2d->get_debug_navigation_enable_geometry_face_random_color();
	bool enabled_edge_lines = ns2d->get_debug_navigation_enable_edge_lines();

	Color debug_face_color = ns2d->get_debug_navigation_geometry_face_color();
	Color debug_edge_color = ns2d->get_debug_navigation_geometry_edge_color();

	RandomPCG rand;

	const TileMapCell &c = r_cell_data.cell;

	TileSetSource *source;
	if (tile_set->has_source(c.source_id)) {
		source = *tile_set->get_source(c.source_id);

		if (source->has_tile(c.get_atlas_coords()) && source->has_alternative_tile(c.get_atlas_coords(), c.alternative_tile)) {
			TileSetAtlasSource *atlas_source = Object::cast_to<TileSetAtlasSource>(source);
			if (atlas_source) {
				const TileData *tile_data;
				if (r_cell_data.runtime_tile_data_cache) {
					tile_data = r_cell_data.runtime_tile_data_cache;
				} else {
					tile_data = atlas_source->get_tile_data(c.get_atlas_coords(), c.alternative_tile);
				}

				Transform2D cell_to_quadrant;
				cell_to_quadrant.set_origin(tile_set->map_to_local(r_cell_data.coords) - p_quadrant_pos);
				rs->canvas_item_add_set_transform(p_canvas_item, cell_to_quadrant);

				for (int layer_index = 0; layer_index < tile_set->get_navigation_layers_count(); layer_index++) {
					bool flip_h = (c.alternative_tile & TileSetAtlasSource::TRANSFORM_FLIP_H);
					bool flip_v = (c.alternative_tile & TileSetAtlasSource::TRANSFORM_FLIP_V);
					bool transpose = (c.alternative_tile & TileSetAtlasSource::TRANSFORM_TRANSPOSE);
					Ref<NavigationPolygon> navigation_polygon = tile_data->get_navigation_polygon(layer_index, flip_h, flip_v, transpose);
					if (navigation_polygon.is_valid()) {
						Vector<Vector2> navigation_polygon_vertices = navigation_polygon->get_vertices();
						if (navigation_polygon_vertices.size() < 3) {
							continue;
						}

						for (int i = 0; i < navigation_polygon->get_polygon_count(); i++) {
							// An array of vertices for this polygon.
							Vector<int> polygon = navigation_polygon->get_polygon(i);
							Vector<Vector2> debug_polygon_vertices;
							debug_polygon_vertices.resize(polygon.size());
							for (int j = 0; j < polygon.size(); j++) {
								ERR_FAIL_INDEX(polygon[j], navigation_polygon_vertices.size());
								debug_polygon_vertices.write[j] = navigation_polygon_vertices[polygon[j]];
							}

							// Generate the polygon color, slightly randomly modified from the settings one.
							Color random_variation_color = debug_face_color;
							if (enabled_geometry_face_random_color) {
								random_variation_color.set_hsv(
										debug_face_color.get_h() + rand.random(-1.0, 1.0) * 0.1,
										debug_face_color.get_s(),
										debug_face_color.get_v() + rand.random(-1.0, 1.0) * 0.2);
							}
							random_variation_color.a = debug_face_color.a;

							Vector<Color> debug_face_colors;
							debug_face_colors.push_back(random_variation_color);
							rs->canvas_item_add_polygon(p_canvas_item, debug_polygon_vertices, debug_face_colors);

							if (enabled_edge_lines) {
								Vector<Color> debug_edge_colors;
								debug_edge_colors.push_back(debug_edge_color);
								debug_polygon_vertices.push_back(debug_polygon_vertices[0]); // Add first again for closing polyline.
								rs->canvas_item_add_polyline(p_canvas_item, debug_polygon_vertices, debug_edge_colors);
							}
						}
					}
				}
			}
		}
	}
}
#endif // DEBUG_ENABLED

/////////////////////////////// Scenes //////////////////////////////////////

void TileMapLayer::_scenes_update(bool p_force_cleanup) {
	// Check if we should cleanup everything.
	bool forced_cleanup = p_force_cleanup || !enabled || !is_inside_tree() || tile_set.is_null();

	if (forced_cleanup) {
		// Clean everything.
		for (KeyValue<Vector2i, CellData> &kv : tile_map_layer_data) {
			_scenes_clear_cell(kv.value);
		}
	} else {
		if (_scenes_was_cleaned_up || dirty.flags[DIRTY_FLAGS_TILE_SET] || dirty.flags[DIRTY_FLAGS_LAYER_IN_TREE]) {
			// Update all cells.
			for (KeyValue<Vector2i, CellData> &kv : tile_map_layer_data) {
				_scenes_update_cell(kv.value);
			}
		} else {
			// Update dirty cells.
			for (SelfList<CellData> *cell_data_list_element = dirty.cell_list.first(); cell_data_list_element; cell_data_list_element = cell_data_list_element->next()) {
				CellData &cell_data = *cell_data_list_element->self();
				_scenes_update_cell(cell_data);
			}
		}
	}

	// -----------
	// Mark the scenes state as up to date.
	_scenes_was_cleaned_up = forced_cleanup;
}

void TileMapLayer::_scenes_clear_cell(CellData &r_cell_data) {
	// Cleanup existing scene.
	Node *node = nullptr;
	if (tile_map_node) {
		// Compatibility with TileMap.
		node = tile_map_node->get_node_or_null(r_cell_data.scene);
	} else {
		node = get_node_or_null(r_cell_data.scene);
	}
	if (node) {
		node->queue_free();
	}
	r_cell_data.scene = "";
}

void TileMapLayer::_scenes_update_cell(CellData &r_cell_data) {
	// Clear the scene in any case.
	_scenes_clear_cell(r_cell_data);

	// Create the scene.
	const TileMapCell &c = r_cell_data.cell;

	TileSetSource *source;
	if (tile_set->has_source(c.source_id)) {
		source = *tile_set->get_source(c.source_id);

		if (source->has_tile(c.get_atlas_coords()) && source->has_alternative_tile(c.get_atlas_coords(), c.alternative_tile)) {
			TileSetScenesCollectionSource *scenes_collection_source = Object::cast_to<TileSetScenesCollectionSource>(source);
			if (scenes_collection_source) {
				Ref<PackedScene> packed_scene = scenes_collection_source->get_scene_tile_scene(c.alternative_tile);
				if (packed_scene.is_valid()) {
					Node *scene = packed_scene->instantiate();
					Control *scene_as_control = Object::cast_to<Control>(scene);
					Node2D *scene_as_node2d = Object::cast_to<Node2D>(scene);
					if (scene_as_control) {
						scene_as_control->set_position(tile_set->map_to_local(r_cell_data.coords) + scene_as_control->get_position());
					} else if (scene_as_node2d) {
						Transform2D xform;
						xform.set_origin(tile_set->map_to_local(r_cell_data.coords));
						scene_as_node2d->set_transform(xform * scene_as_node2d->get_transform());
					}
					if (tile_map_node) {
						// Compatibility with TileMap.
						tile_map_node->add_child(scene);
					} else {
						add_child(scene);
					}
					r_cell_data.scene = scene->get_name();
				}
			}
		}
	}
}

#ifdef DEBUG_ENABLED
void TileMapLayer::_scenes_draw_cell_debug(const RID &p_canvas_item, const Vector2 &p_quadrant_pos, const CellData &r_cell_data) {
	ERR_FAIL_COND(tile_set.is_null());

	if (!Engine::get_singleton()->is_editor_hint()) {
		return;
	}

	// Draw a placeholder for scenes needing one.
	RenderingServer *rs = RenderingServer::get_singleton();

	const TileMapCell &c = r_cell_data.cell;

	TileSetSource *source;
	if (tile_set->has_source(c.source_id)) {
		source = *tile_set->get_source(c.source_id);

		if (!source->has_tile(c.get_atlas_coords()) || !source->has_alternative_tile(c.get_atlas_coords(), c.alternative_tile)) {
			return;
		}

		TileSetScenesCollectionSource *scenes_collection_source = Object::cast_to<TileSetScenesCollectionSource>(source);
		if (scenes_collection_source) {
			if (scenes_collection_source->get_scene_tile_scene(c.alternative_tile).is_null() || scenes_collection_source->get_scene_tile_display_placeholder(c.alternative_tile)) {
				// Generate a random color from the hashed values of the tiles.
				Array to_hash;
				to_hash.push_back(c.source_id);
				to_hash.push_back(c.alternative_tile);
				uint32_t hash = RandomPCG(to_hash.hash()).rand();

				Color color;
				color = color.from_hsv(
						(float)((hash >> 24) & 0xFF) / 256.0,
						Math::lerp(0.5, 1.0, (float)((hash >> 16) & 0xFF) / 256.0),
						Math::lerp(0.5, 1.0, (float)((hash >> 8) & 0xFF) / 256.0),
						0.8);

				// Draw a placeholder tile.
				Transform2D cell_to_quadrant;
				cell_to_quadrant.set_origin(tile_set->map_to_local(r_cell_data.coords) - p_quadrant_pos);
				rs->canvas_item_add_set_transform(p_canvas_item, cell_to_quadrant);
				rs->canvas_item_add_circle(p_canvas_item, Vector2(), MIN(tile_set->get_tile_size().x, tile_set->get_tile_size().y) / 4.0, color);
			}
		}
	}
}
#endif // DEBUG_ENABLED

/////////////////////////////////////////////////////////////////////

void TileMapLayer::_build_runtime_update_tile_data(bool p_force_cleanup) {
	// Check if we should cleanup everything.
	bool forced_cleanup = p_force_cleanup || !enabled || tile_set.is_null() || !is_visible_in_tree();
	if (!forced_cleanup) {
		bool valid_runtime_update = GDVIRTUAL_IS_OVERRIDDEN(_use_tile_data_runtime_update) && GDVIRTUAL_IS_OVERRIDDEN(_tile_data_runtime_update);
		bool valid_runtime_update_for_tilemap = tile_map_node && tile_map_node->GDVIRTUAL_IS_OVERRIDDEN(_use_tile_data_runtime_update) && tile_map_node->GDVIRTUAL_IS_OVERRIDDEN(_tile_data_runtime_update); // For keeping compatibility.
		if (valid_runtime_update || valid_runtime_update_for_tilemap) {
			bool use_tilemap_for_runtime = valid_runtime_update_for_tilemap && !valid_runtime_update;
			if (_runtime_update_tile_data_was_cleaned_up || dirty.flags[DIRTY_FLAGS_TILE_SET]) {
				_runtime_update_needs_all_cells_cleaned_up = true;
				for (KeyValue<Vector2i, CellData> &E : tile_map_layer_data) {
					_build_runtime_update_tile_data_for_cell(E.value, use_tilemap_for_runtime);
				}
			} else if (dirty.flags[DIRTY_FLAGS_LAYER_RUNTIME_UPDATE]) {
				for (KeyValue<Vector2i, CellData> &E : tile_map_layer_data) {
					_build_runtime_update_tile_data_for_cell(E.value, use_tilemap_for_runtime, true);
				}
			} else {
				for (SelfList<CellData> *cell_data_list_element = dirty.cell_list.first(); cell_data_list_element; cell_data_list_element = cell_data_list_element->next()) {
					CellData &cell_data = *cell_data_list_element->self();
					_build_runtime_update_tile_data_for_cell(cell_data, use_tilemap_for_runtime);
				}
			}
		}
	}

	// -----------
	// Mark the navigation state as up to date.
	_runtime_update_tile_data_was_cleaned_up = forced_cleanup;
}

void TileMapLayer::_build_runtime_update_tile_data_for_cell(CellData &r_cell_data, bool p_use_tilemap_for_runtime, bool p_auto_add_to_dirty_list) {
	TileMapCell &c = r_cell_data.cell;
	TileSetSource *source;
	if (tile_set->has_source(c.source_id)) {
		source = *tile_set->get_source(c.source_id);

		if (source->has_tile(c.get_atlas_coords()) && source->has_alternative_tile(c.get_atlas_coords(), c.alternative_tile)) {
			TileSetAtlasSource *atlas_source = Object::cast_to<TileSetAtlasSource>(source);
			if (atlas_source) {
				bool ret = false;

				if (p_use_tilemap_for_runtime) {
					// Compatibility with TileMap.
					if (tile_map_node->GDVIRTUAL_CALL(_use_tile_data_runtime_update, layer_index_in_tile_map_node, r_cell_data.coords, ret) && ret) {
						TileData *tile_data = atlas_source->get_tile_data(c.get_atlas_coords(), c.alternative_tile);

						// Create the runtime TileData.
						TileData *tile_data_runtime_use = tile_data->duplicate();
						tile_data_runtime_use->set_allow_transform(true);
						r_cell_data.runtime_tile_data_cache = tile_data_runtime_use;

						tile_map_node->GDVIRTUAL_CALL(_tile_data_runtime_update, layer_index_in_tile_map_node, r_cell_data.coords, tile_data_runtime_use);

						if (p_auto_add_to_dirty_list) {
							dirty.cell_list.add(&r_cell_data.dirty_list_element);
						}
					}
				} else {
					if (GDVIRTUAL_CALL(_use_tile_data_runtime_update, r_cell_data.coords, ret) && ret) {
						TileData *tile_data = atlas_source->get_tile_data(c.get_atlas_coords(), c.alternative_tile);

						// Create the runtime TileData.
						TileData *tile_data_runtime_use = tile_data->duplicate();
						tile_data_runtime_use->set_allow_transform(true);
						r_cell_data.runtime_tile_data_cache = tile_data_runtime_use;

						GDVIRTUAL_CALL(_tile_data_runtime_update, r_cell_data.coords, tile_data_runtime_use);

						if (p_auto_add_to_dirty_list) {
							dirty.cell_list.add(&r_cell_data.dirty_list_element);
						}
					}
				}
			}
		}
	}
}

void TileMapLayer::_clear_runtime_update_tile_data() {
	if (_runtime_update_needs_all_cells_cleaned_up) {
		for (KeyValue<Vector2i, CellData> &E : tile_map_layer_data) {
			_clear_runtime_update_tile_data_for_cell(E.value);
		}
		_runtime_update_needs_all_cells_cleaned_up = false;
	} else {
		for (SelfList<CellData> *cell_data_list_element = dirty.cell_list.first(); cell_data_list_element; cell_data_list_element = cell_data_list_element->next()) {
			CellData &r_cell_data = *cell_data_list_element->self();
			_clear_runtime_update_tile_data_for_cell(r_cell_data);
		}
	}
}

void TileMapLayer::_clear_runtime_update_tile_data_for_cell(CellData &r_cell_data) {
	// Clear the runtime tile data.
	if (r_cell_data.runtime_tile_data_cache) {
		memdelete(r_cell_data.runtime_tile_data_cache);
		r_cell_data.runtime_tile_data_cache = nullptr;
	}
}

void TileMapLayer::_update_cells_callback(bool p_force_cleanup) {
	if (!GDVIRTUAL_IS_OVERRIDDEN(_update_cells)) {
		return;
	}

	// Check if we should cleanup everything.
	bool forced_cleanup = p_force_cleanup || !enabled || tile_set.is_null() || !is_visible_in_tree();

	// List all the dirty cell's positions to notify script of cell updates.
	TypedArray<Vector2i> dirty_cell_positions;
	for (SelfList<CellData> *cell_data_list_element = dirty.cell_list.first(); cell_data_list_element; cell_data_list_element = cell_data_list_element->next()) {
		CellData &cell_data = *cell_data_list_element->self();
		dirty_cell_positions.push_back(cell_data.coords);
	}

	GDVIRTUAL_CALL(_update_cells, dirty_cell_positions, forced_cleanup);
}

TileSet::TerrainsPattern TileMapLayer::_get_best_terrain_pattern_for_constraints(int p_terrain_set, const Vector2i &p_position, const RBSet<TerrainConstraint> &p_constraints, TileSet::TerrainsPattern p_current_pattern) const {
	if (tile_set.is_null()) {
		return TileSet::TerrainsPattern();
	}
	// Returns all tiles compatible with the given constraints.
	RBMap<TileSet::TerrainsPattern, int> terrain_pattern_score;
	RBSet<TileSet::TerrainsPattern> pattern_set = tile_set->get_terrains_pattern_set(p_terrain_set);
	ERR_FAIL_COND_V(pattern_set.is_empty(), TileSet::TerrainsPattern());
	for (TileSet::TerrainsPattern &terrain_pattern : pattern_set) {
		int score = 0;

		// Check the center bit constraint.
		TerrainConstraint terrain_constraint = TerrainConstraint(tile_set, p_position, terrain_pattern.get_terrain());
		const RBSet<TerrainConstraint>::Element *in_set_constraint_element = p_constraints.find(terrain_constraint);
		if (in_set_constraint_element) {
			if (in_set_constraint_element->get().get_terrain() != terrain_constraint.get_terrain()) {
				score += in_set_constraint_element->get().get_priority();
			}
		} else if (p_current_pattern.get_terrain() != terrain_pattern.get_terrain()) {
			continue; // Ignore a pattern that cannot keep bits without constraints unmodified.
		}

		// Check the surrounding bits
		bool invalid_pattern = false;
		for (int i = 0; i < TileSet::CELL_NEIGHBOR_MAX; i++) {
			TileSet::CellNeighbor bit = TileSet::CellNeighbor(i);
			if (tile_set->is_valid_terrain_peering_bit(p_terrain_set, bit)) {
				// Check if the bit is compatible with the constraints.
				TerrainConstraint terrain_bit_constraint = TerrainConstraint(tile_set, p_position, bit, terrain_pattern.get_terrain_peering_bit(bit));
				in_set_constraint_element = p_constraints.find(terrain_bit_constraint);
				if (in_set_constraint_element) {
					if (in_set_constraint_element->get().get_terrain() != terrain_bit_constraint.get_terrain()) {
						score += in_set_constraint_element->get().get_priority();
					}
				} else if (p_current_pattern.get_terrain_peering_bit(bit) != terrain_pattern.get_terrain_peering_bit(bit)) {
					invalid_pattern = true; // Ignore a pattern that cannot keep bits without constraints unmodified.
					break;
				}
			}
		}
		if (invalid_pattern) {
			continue;
		}

		terrain_pattern_score[terrain_pattern] = score;
	}

	// Compute the minimum score.
	TileSet::TerrainsPattern min_score_pattern = p_current_pattern;
	int min_score = INT32_MAX;
	for (KeyValue<TileSet::TerrainsPattern, int> E : terrain_pattern_score) {
		if (E.value < min_score) {
			min_score_pattern = E.key;
			min_score = E.value;
		}
	}

	return min_score_pattern;
}

RBSet<TerrainConstraint> TileMapLayer::_get_terrain_constraints_from_added_pattern(const Vector2i &p_position, int p_terrain_set, TileSet::TerrainsPattern p_terrains_pattern) const {
	if (tile_set.is_null()) {
		return RBSet<TerrainConstraint>();
	}

	// Compute the constraints needed from the surrounding tiles.
	RBSet<TerrainConstraint> output;
	output.insert(TerrainConstraint(tile_set, p_position, p_terrains_pattern.get_terrain()));

	for (uint32_t i = 0; i < TileSet::CELL_NEIGHBOR_MAX; i++) {
		TileSet::CellNeighbor side = TileSet::CellNeighbor(i);
		if (tile_set->is_valid_terrain_peering_bit(p_terrain_set, side)) {
			TerrainConstraint c = TerrainConstraint(tile_set, p_position, side, p_terrains_pattern.get_terrain_peering_bit(side));
			output.insert(c);
		}
	}

	return output;
}

RBSet<TerrainConstraint> TileMapLayer::_get_terrain_constraints_from_painted_cells_list(const RBSet<Vector2i> &p_painted, int p_terrain_set, bool p_ignore_empty_terrains) const {
	if (tile_set.is_null()) {
		return RBSet<TerrainConstraint>();
	}

	ERR_FAIL_INDEX_V(p_terrain_set, tile_set->get_terrain_sets_count(), RBSet<TerrainConstraint>());

	// Build a set of dummy constraints to get the constrained points.
	RBSet<TerrainConstraint> dummy_constraints;
	for (const Vector2i &E : p_painted) {
		for (int i = 0; i < TileSet::CELL_NEIGHBOR_MAX; i++) { // Iterates over neighbor bits.
			TileSet::CellNeighbor bit = TileSet::CellNeighbor(i);
			if (tile_set->is_valid_terrain_peering_bit(p_terrain_set, bit)) {
				dummy_constraints.insert(TerrainConstraint(tile_set, E, bit, -1));
			}
		}
	}

	// For each constrained point, we get all overlapping tiles, and select the most adequate terrain for it.
	RBSet<TerrainConstraint> constraints;
	for (const TerrainConstraint &E_constraint : dummy_constraints) {
		HashMap<int, int> terrain_count;

		// Count the number of occurrences per terrain.
		HashMap<Vector2i, TileSet::CellNeighbor> overlapping_terrain_bits = E_constraint.get_overlapping_coords_and_peering_bits();
		for (const KeyValue<Vector2i, TileSet::CellNeighbor> &E_overlapping : overlapping_terrain_bits) {
			TileData *neighbor_tile_data = nullptr;
			TileMapCell neighbor_cell = get_cell(E_overlapping.key);
			if (neighbor_cell.source_id != TileSet::INVALID_SOURCE) {
				Ref<TileSetSource> source = tile_set->get_source(neighbor_cell.source_id);
				Ref<TileSetAtlasSource> atlas_source = source;
				if (atlas_source.is_valid()) {
					TileData *tile_data = atlas_source->get_tile_data(neighbor_cell.get_atlas_coords(), neighbor_cell.alternative_tile);
					if (tile_data && tile_data->get_terrain_set() == p_terrain_set) {
						neighbor_tile_data = tile_data;
					}
				}
			}

			int terrain = neighbor_tile_data ? neighbor_tile_data->get_terrain_peering_bit(TileSet::CellNeighbor(E_overlapping.value)) : -1;
			if (!p_ignore_empty_terrains || terrain >= 0) {
				if (!terrain_count.has(terrain)) {
					terrain_count[terrain] = 0;
				}
				terrain_count[terrain] += 1;
			}
		}

		// Get the terrain with the max number of occurrences.
		int max = 0;
		int max_terrain = -1;
		for (const KeyValue<int, int> &E_terrain_count : terrain_count) {
			if (E_terrain_count.value > max) {
				max = E_terrain_count.value;
				max_terrain = E_terrain_count.key;
			}
		}

		// Set the adequate terrain.
		if (max > 0) {
			TerrainConstraint c = E_constraint;
			c.set_terrain(max_terrain);
			constraints.insert(c);
		}
	}

	// Add the centers as constraints.
	for (Vector2i E_coords : p_painted) {
		TileData *tile_data = nullptr;
		TileMapCell cell = get_cell(E_coords);
		if (cell.source_id != TileSet::INVALID_SOURCE) {
			Ref<TileSetSource> source = tile_set->get_source(cell.source_id);
			Ref<TileSetAtlasSource> atlas_source = source;
			if (atlas_source.is_valid()) {
				tile_data = atlas_source->get_tile_data(cell.get_atlas_coords(), cell.alternative_tile);
			}
		}

		int terrain = (tile_data && tile_data->get_terrain_set() == p_terrain_set) ? tile_data->get_terrain() : -1;
		if (!p_ignore_empty_terrains || terrain >= 0) {
			constraints.insert(TerrainConstraint(tile_set, E_coords, terrain));
		}
	}

	return constraints;
}

void TileMapLayer::_tile_set_changed() {
	dirty.flags[DIRTY_FLAGS_TILE_SET] = true;
	_queue_internal_update();
	emit_signal(CoreStringName(changed));
}

void TileMapLayer::_renamed() {
	emit_signal(CoreStringName(changed));
}

void TileMapLayer::_update_notify_local_transform() {
	bool notify = is_using_kinematic_bodies() || is_y_sort_enabled();
	if (!notify) {
		if (is_y_sort_enabled()) {
			notify = true;
		}
	}
	set_notify_local_transform(notify);
}

void TileMapLayer::_queue_internal_update() {
	if (pending_update) {
		return;
	}
	// Don't update when outside the tree, it doesn't do anything useful, and causes threading problems.
	if (is_inside_tree()) {
		pending_update = true;
		callable_mp(this, &TileMapLayer::_deferred_internal_update).call_deferred();
	}
}

void TileMapLayer::_deferred_internal_update() {
	// Other updates.
	if (!pending_update) {
		return;
	}

	// Update dirty quadrants on layers.
	_internal_update(false);
}

void TileMapLayer::_internal_update(bool p_force_cleanup) {
	// Find TileData that need a runtime modification.
	// This may add cells to the dirty list if a runtime modification has been notified.
	_build_runtime_update_tile_data(p_force_cleanup);

	// Callback for implementing custom subsystems.
	// This may add to the dirty list if some cells are changed inside _update_cells.
	_update_cells_callback(p_force_cleanup);

	// Update all subsystems.
	_rendering_update(p_force_cleanup);
	_physics_update(p_force_cleanup);
	_navigation_update(p_force_cleanup);
	_scenes_update(p_force_cleanup);
#ifdef DEBUG_ENABLED
	_debug_update(p_force_cleanup);
#endif // DEBUG_ENABLED

	_clear_runtime_update_tile_data();

	// Clear the "what is dirty" flags.
	for (int i = 0; i < DIRTY_FLAGS_MAX; i++) {
		dirty.flags[i] = false;
	}

	// List the cells to delete definitely.
	Vector<Vector2i> to_delete;
	for (SelfList<CellData> *cell_data_list_element = dirty.cell_list.first(); cell_data_list_element; cell_data_list_element = cell_data_list_element->next()) {
		CellData &cell_data = *cell_data_list_element->self();
		// Select the cell from tile_map if it is invalid.
		if (cell_data.cell.source_id == TileSet::INVALID_SOURCE) {
			to_delete.push_back(cell_data.coords);
		}
	}

	// Remove cells that are empty after the cleanup.
	for (const Vector2i &coords : to_delete) {
		tile_map_layer_data.erase(coords);
	}

	// Clear the dirty cells list.
	dirty.cell_list.clear();

	pending_update = false;
}

void TileMapLayer::_physics_interpolated_changed() {
	RenderingServer *rs = RenderingServer::get_singleton();

	bool interpolated = is_physics_interpolated();
	bool needs_reset = interpolated && is_visible_in_tree();

	for (const KeyValue<Vector2i, Ref<RenderingQuadrant>> &kv : rendering_quadrant_map) {
		for (const RID &ci : kv.value->canvas_items) {
			if (ci.is_valid()) {
				rs->canvas_item_set_interpolated(ci, interpolated);
				if (needs_reset) {
					rs->canvas_item_reset_physics_interpolation(ci);
				}
			}
		}
	}

	for (const KeyValue<Vector2i, CellData> &E : tile_map_layer_data) {
		for (const LocalVector<RID> &polygons : E.value.occluders) {
			for (const RID &occluder_id : polygons) {
				if (occluder_id.is_valid()) {
					rs->canvas_light_occluder_set_interpolated(occluder_id, interpolated);
					if (needs_reset) {
						rs->canvas_light_occluder_reset_physics_interpolation(occluder_id);
					}
				}
			}
		}
	}
}

void TileMapLayer::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_POSTINITIALIZE: {
			connect(SNAME("renamed"), callable_mp(this, &TileMapLayer::_renamed));
			break;
		}
		case NOTIFICATION_ENTER_TREE: {
			_update_notify_local_transform();
			dirty.flags[DIRTY_FLAGS_LAYER_IN_TREE] = true;
			_queue_internal_update();
		} break;

		case NOTIFICATION_EXIT_TREE: {
			dirty.flags[DIRTY_FLAGS_LAYER_IN_TREE] = true;
			// Update immediately on exiting, and force cleanup.
			_internal_update(true);
		} break;

		case NOTIFICATION_ENTER_CANVAS: {
			dirty.flags[DIRTY_FLAGS_LAYER_IN_CANVAS] = true;
			_queue_internal_update();
		} break;

		case NOTIFICATION_EXIT_CANVAS: {
			dirty.flags[DIRTY_FLAGS_LAYER_IN_CANVAS] = true;
			// Update immediately on exiting, and force cleanup.
			_internal_update(true);
		} break;

		case NOTIFICATION_VISIBILITY_CHANGED: {
			dirty.flags[DIRTY_FLAGS_LAYER_VISIBILITY] = true;
			_queue_internal_update();
		} break;
	}

	_rendering_notification(p_what);
	_physics_notification(p_what);
	_navigation_notification(p_what);
}

void TileMapLayer::_bind_methods() {
	// --- Cells manipulation ---
	// Generic cells manipulations and access.
	ClassDB::bind_method(D_METHOD("set_cell", "coords", "source_id", "atlas_coords", "alternative_tile"), &TileMapLayer::set_cell, DEFVAL(TileSet::INVALID_SOURCE), DEFVAL(TileSetSource::INVALID_ATLAS_COORDS), DEFVAL(0));
	ClassDB::bind_method(D_METHOD("erase_cell", "coords"), &TileMapLayer::erase_cell);
	ClassDB::bind_method(D_METHOD("fix_invalid_tiles"), &TileMapLayer::fix_invalid_tiles);
	ClassDB::bind_method(D_METHOD("clear"), &TileMapLayer::clear);

	ClassDB::bind_method(D_METHOD("get_cell_source_id", "coords"), &TileMapLayer::get_cell_source_id);
	ClassDB::bind_method(D_METHOD("get_cell_atlas_coords", "coords"), &TileMapLayer::get_cell_atlas_coords);
	ClassDB::bind_method(D_METHOD("get_cell_alternative_tile", "coords"), &TileMapLayer::get_cell_alternative_tile);
	ClassDB::bind_method(D_METHOD("get_cell_tile_data", "coords"), &TileMapLayer::get_cell_tile_data);

	ClassDB::bind_method(D_METHOD("is_cell_flipped_h", "coords"), &TileMapLayer::is_cell_flipped_h);
	ClassDB::bind_method(D_METHOD("is_cell_flipped_v", "coords"), &TileMapLayer::is_cell_flipped_v);
	ClassDB::bind_method(D_METHOD("is_cell_transposed", "coords"), &TileMapLayer::is_cell_transposed);

	ClassDB::bind_method(D_METHOD("get_used_cells"), &TileMapLayer::get_used_cells);
	ClassDB::bind_method(D_METHOD("get_used_cells_by_id", "source_id", "atlas_coords", "alternative_tile"), &TileMapLayer::get_used_cells_by_id, DEFVAL(TileSet::INVALID_SOURCE), DEFVAL(TileSetSource::INVALID_ATLAS_COORDS), DEFVAL(TileSetSource::INVALID_TILE_ALTERNATIVE));
	ClassDB::bind_method(D_METHOD("get_used_rect"), &TileMapLayer::get_used_rect);

	// Patterns.
	ClassDB::bind_method(D_METHOD("get_pattern", "coords_array"), &TileMapLayer::get_pattern);
	ClassDB::bind_method(D_METHOD("set_pattern", "position", "pattern"), &TileMapLayer::set_pattern);

	// Terrains.
	ClassDB::bind_method(D_METHOD("set_cells_terrain_connect", "cells", "terrain_set", "terrain", "ignore_empty_terrains"), &TileMapLayer::set_cells_terrain_connect, DEFVAL(true));
	ClassDB::bind_method(D_METHOD("set_cells_terrain_path", "path", "terrain_set", "terrain", "ignore_empty_terrains"), &TileMapLayer::set_cells_terrain_path, DEFVAL(true));

	// --- Physics helpers ---
	ClassDB::bind_method(D_METHOD("has_body_rid", "body"), &TileMapLayer::has_body_rid);
	ClassDB::bind_method(D_METHOD("get_coords_for_body_rid", "body"), &TileMapLayer::get_coords_for_body_rid);

	// --- Runtime ---
	ClassDB::bind_method(D_METHOD("update_internals"), &TileMapLayer::update_internals);
	ClassDB::bind_method(D_METHOD("notify_runtime_tile_data_update"), &TileMapLayer::notify_runtime_tile_data_update, DEFVAL(-1));

	// --- Shortcuts to methods defined in TileSet ---
	ClassDB::bind_method(D_METHOD("map_pattern", "position_in_tilemap", "coords_in_pattern", "pattern"), &TileMapLayer::map_pattern);
	ClassDB::bind_method(D_METHOD("get_surrounding_cells", "coords"), &TileMapLayer::get_surrounding_cells);
	ClassDB::bind_method(D_METHOD("get_neighbor_cell", "coords", "neighbor"), &TileMapLayer::get_neighbor_cell);
	ClassDB::bind_method(D_METHOD("map_to_local", "map_position"), &TileMapLayer::map_to_local);
	ClassDB::bind_method(D_METHOD("local_to_map", "local_position"), &TileMapLayer::local_to_map);

	// --- Accessors ---
	ClassDB::bind_method(D_METHOD("set_tile_map_data_from_array", "tile_map_layer_data"), &TileMapLayer::set_tile_map_data_from_array);
	ClassDB::bind_method(D_METHOD("get_tile_map_data_as_array"), &TileMapLayer::get_tile_map_data_as_array);

	ClassDB::bind_method(D_METHOD("set_enabled", "enabled"), &TileMapLayer::set_enabled);
	ClassDB::bind_method(D_METHOD("is_enabled"), &TileMapLayer::is_enabled);

	ClassDB::bind_method(D_METHOD("set_tile_set", "tile_set"), &TileMapLayer::set_tile_set);
	ClassDB::bind_method(D_METHOD("get_tile_set"), &TileMapLayer::get_tile_set);

	ClassDB::bind_method(D_METHOD("set_y_sort_origin", "y_sort_origin"), &TileMapLayer::set_y_sort_origin);
	ClassDB::bind_method(D_METHOD("get_y_sort_origin"), &TileMapLayer::get_y_sort_origin);
	ClassDB::bind_method(D_METHOD("set_x_draw_order_reversed", "x_draw_order_reversed"), &TileMapLayer::set_x_draw_order_reversed);
	ClassDB::bind_method(D_METHOD("is_x_draw_order_reversed"), &TileMapLayer::is_x_draw_order_reversed);
	ClassDB::bind_method(D_METHOD("set_rendering_quadrant_size", "size"), &TileMapLayer::set_rendering_quadrant_size);
	ClassDB::bind_method(D_METHOD("get_rendering_quadrant_size"), &TileMapLayer::get_rendering_quadrant_size);

	ClassDB::bind_method(D_METHOD("set_collision_enabled", "enabled"), &TileMapLayer::set_collision_enabled);
	ClassDB::bind_method(D_METHOD("is_collision_enabled"), &TileMapLayer::is_collision_enabled);
	ClassDB::bind_method(D_METHOD("set_use_kinematic_bodies", "use_kinematic_bodies"), &TileMapLayer::set_use_kinematic_bodies);
	ClassDB::bind_method(D_METHOD("is_using_kinematic_bodies"), &TileMapLayer::is_using_kinematic_bodies);
	ClassDB::bind_method(D_METHOD("set_collision_visibility_mode", "visibility_mode"), &TileMapLayer::set_collision_visibility_mode);
	ClassDB::bind_method(D_METHOD("get_collision_visibility_mode"), &TileMapLayer::get_collision_visibility_mode);

	ClassDB::bind_method(D_METHOD("set_occlusion_enabled", "enabled"), &TileMapLayer::set_occlusion_enabled);
	ClassDB::bind_method(D_METHOD("is_occlusion_enabled"), &TileMapLayer::is_occlusion_enabled);

	ClassDB::bind_method(D_METHOD("set_navigation_enabled", "enabled"), &TileMapLayer::set_navigation_enabled);
	ClassDB::bind_method(D_METHOD("is_navigation_enabled"), &TileMapLayer::is_navigation_enabled);
	ClassDB::bind_method(D_METHOD("set_navigation_map", "map"), &TileMapLayer::set_navigation_map);
	ClassDB::bind_method(D_METHOD("get_navigation_map"), &TileMapLayer::get_navigation_map);
	ClassDB::bind_method(D_METHOD("set_navigation_visibility_mode", "show_navigation"), &TileMapLayer::set_navigation_visibility_mode);
	ClassDB::bind_method(D_METHOD("get_navigation_visibility_mode"), &TileMapLayer::get_navigation_visibility_mode);

	GDVIRTUAL_BIND(_use_tile_data_runtime_update, "coords");
	GDVIRTUAL_BIND(_tile_data_runtime_update, "coords", "tile_data");
	GDVIRTUAL_BIND(_update_cells, "coords", "forced_cleanup");

	ADD_PROPERTY(PropertyInfo(Variant::PACKED_BYTE_ARRAY, "tile_map_data", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NO_EDITOR), "set_tile_map_data_from_array", "get_tile_map_data_as_array");

	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "enabled"), "set_enabled", "is_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "tile_set", PROPERTY_HINT_RESOURCE_TYPE, "TileSet"), "set_tile_set", "get_tile_set");
	ADD_GROUP("Rendering", "");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "occlusion_enabled"), "set_occlusion_enabled", "is_occlusion_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "y_sort_origin"), "set_y_sort_origin", "get_y_sort_origin");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "x_draw_order_reversed"), "set_x_draw_order_reversed", "is_x_draw_order_reversed");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "rendering_quadrant_size"), "set_rendering_quadrant_size", "get_rendering_quadrant_size");
	ADD_GROUP("Physics", "");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "collision_enabled"), "set_collision_enabled", "is_collision_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "use_kinematic_bodies"), "set_use_kinematic_bodies", "is_using_kinematic_bodies");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "collision_visibility_mode", PROPERTY_HINT_ENUM, "Default,Force Show,Force Hide"), "set_collision_visibility_mode", "get_collision_visibility_mode");
	ADD_GROUP("Navigation", "");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "navigation_enabled"), "set_navigation_enabled", "is_navigation_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "navigation_visibility_mode", PROPERTY_HINT_ENUM, "Default,Force Show,Force Hide"), "set_navigation_visibility_mode", "get_navigation_visibility_mode");

	ADD_SIGNAL(MethodInfo(CoreStringName(changed)));

	ADD_PROPERTY_DEFAULT("tile_map_data_format", TileMapDataFormat::TILE_MAP_DATA_FORMAT_1);

	BIND_ENUM_CONSTANT(DEBUG_VISIBILITY_MODE_DEFAULT);
	BIND_ENUM_CONSTANT(DEBUG_VISIBILITY_MODE_FORCE_HIDE);
	BIND_ENUM_CONSTANT(DEBUG_VISIBILITY_MODE_FORCE_SHOW);
}

void TileMapLayer::_validate_property(PropertyInfo &p_property) const {
	if (is_y_sort_enabled()) {
		if (p_property.name == "rendering_quadrant_size") {
			p_property.usage |= PROPERTY_USAGE_READ_ONLY;
		}
	} else {
		if (p_property.name == "x_draw_order_reversed") {
			p_property.usage |= PROPERTY_USAGE_READ_ONLY;
		}
	}
}

void TileMapLayer::_update_self_texture_filter(RS::CanvasItemTextureFilter p_texture_filter) {
	// Set a default texture filter for the whole tilemap.
	CanvasItem::_update_self_texture_filter(p_texture_filter);
	dirty.flags[DIRTY_FLAGS_LAYER_TEXTURE_FILTER] = true;
	_queue_internal_update();
	emit_signal(CoreStringName(changed));
}

void TileMapLayer::_update_self_texture_repeat(RS::CanvasItemTextureRepeat p_texture_repeat) {
	// Set a default texture repeat for the whole tilemap.
	CanvasItem::_update_self_texture_repeat(p_texture_repeat);
	dirty.flags[DIRTY_FLAGS_LAYER_TEXTURE_REPEAT] = true;
	_queue_internal_update();
	emit_signal(CoreStringName(changed));
}

#ifdef TOOLS_ENABLED
bool TileMapLayer::_edit_is_selected_on_click(const Point2 &p_point, double p_tolerance) const {
	return tile_set.is_valid() && get_cell_source_id(local_to_map(p_point)) != TileSet::INVALID_SOURCE;
}
#endif

void TileMapLayer::set_as_tile_map_internal_node(int p_index) {
	// Compatibility with TileMap.
	ERR_FAIL_NULL(get_parent());
	tile_map_node = Object::cast_to<TileMap>(get_parent());
	set_use_parent_material(true);
	force_parent_owned();
	if (layer_index_in_tile_map_node != p_index) {
		layer_index_in_tile_map_node = p_index;
		dirty.flags[DIRTY_FLAGS_LAYER_INDEX_IN_TILE_MAP_NODE] = true;
		_queue_internal_update();
	}
}

Rect2 TileMapLayer::get_rect(bool &r_changed) const {
	if (tile_set.is_null()) {
		r_changed = rect_cache != Rect2();
		return Rect2();
	}

	// Compute the displayed area of the tilemap.
	r_changed = false;
#ifdef DEBUG_ENABLED

	if (rect_cache_dirty) {
		Rect2 r_total;
		bool first = true;
		for (const KeyValue<Vector2i, CellData> &E : tile_map_layer_data) {
			Rect2 r;
			r.position = tile_set->map_to_local(E.key);
			r.size = Size2();
			if (first) {
				r_total = r;
				first = false;
			} else {
				r_total = r_total.merge(r);
			}
		}

		r_changed = rect_cache != r_total;

		rect_cache = r_total;
		rect_cache_dirty = false;
	}
#endif
	return rect_cache;
}

HashMap<Vector2i, TileSet::TerrainsPattern> TileMapLayer::terrain_fill_constraints(const Vector<Vector2i> &p_to_replace, int p_terrain_set, const RBSet<TerrainConstraint> &p_constraints) const {
	if (tile_set.is_null()) {
		return HashMap<Vector2i, TileSet::TerrainsPattern>();
	}

	// Copy the constraints set.
	RBSet<TerrainConstraint> constraints = p_constraints;

	// Output map.
	HashMap<Vector2i, TileSet::TerrainsPattern> output;

	// Add all positions to a set.
	for (int i = 0; i < p_to_replace.size(); i++) {
		const Vector2i &coords = p_to_replace[i];

		// Select the best pattern for the given constraints.
		TileSet::TerrainsPattern current_pattern = TileSet::TerrainsPattern(*tile_set, p_terrain_set);
		TileMapCell cell = get_cell(coords);
		if (cell.source_id != TileSet::INVALID_SOURCE) {
			TileSetSource *source = *tile_set->get_source(cell.source_id);
			TileSetAtlasSource *atlas_source = Object::cast_to<TileSetAtlasSource>(source);
			if (atlas_source) {
				// Get tile data.
				TileData *tile_data = atlas_source->get_tile_data(cell.get_atlas_coords(), cell.alternative_tile);
				if (tile_data && tile_data->get_terrain_set() == p_terrain_set) {
					current_pattern = tile_data->get_terrains_pattern();
				}
			}
		}
		TileSet::TerrainsPattern pattern = _get_best_terrain_pattern_for_constraints(p_terrain_set, coords, constraints, current_pattern);

		// Update the constraint set with the new ones.
		RBSet<TerrainConstraint> new_constraints = _get_terrain_constraints_from_added_pattern(coords, p_terrain_set, pattern);
		for (const TerrainConstraint &E_constraint : new_constraints) {
			if (constraints.has(E_constraint)) {
				constraints.erase(E_constraint);
			}
			TerrainConstraint c = E_constraint;
			c.set_priority(5);
			constraints.insert(c);
		}

		output[coords] = pattern;
	}
	return output;
}

HashMap<Vector2i, TileSet::TerrainsPattern> TileMapLayer::terrain_fill_connect(const Vector<Vector2i> &p_coords_array, int p_terrain_set, int p_terrain, bool p_ignore_empty_terrains) const {
	HashMap<Vector2i, TileSet::TerrainsPattern> output;
	ERR_FAIL_COND_V(tile_set.is_null(), output);
	ERR_FAIL_INDEX_V(p_terrain_set, tile_set->get_terrain_sets_count(), output);

	// Build list and set of tiles that can be modified (painted and their surroundings).
	Vector<Vector2i> can_modify_list;
	RBSet<Vector2i> can_modify_set;
	RBSet<Vector2i> painted_set;
	for (int i = p_coords_array.size() - 1; i >= 0; i--) {
		const Vector2i &coords = p_coords_array[i];
		can_modify_list.push_back(coords);
		can_modify_set.insert(coords);
		painted_set.insert(coords);
	}
	for (Vector2i coords : p_coords_array) {
		// Find the adequate neighbor.
		for (int j = 0; j < TileSet::CELL_NEIGHBOR_MAX; j++) {
			TileSet::CellNeighbor bit = TileSet::CellNeighbor(j);
			if (tile_set->is_existing_neighbor(bit)) {
				Vector2i neighbor = tile_set->get_neighbor_cell(coords, bit);
				if (!can_modify_set.has(neighbor)) {
					can_modify_list.push_back(neighbor);
					can_modify_set.insert(neighbor);
				}
			}
		}
	}

	// Build a set, out of the possibly modified tiles, of the one with a center bit that is set (or will be) to the painted terrain.
	RBSet<Vector2i> cells_with_terrain_center_bit;
	for (Vector2i coords : can_modify_set) {
		bool connect = false;
		if (painted_set.has(coords)) {
			connect = true;
		} else {
			// Get the center bit of the cell.
			TileData *tile_data = nullptr;
			TileMapCell cell = get_cell(coords);
			if (cell.source_id != TileSet::INVALID_SOURCE) {
				Ref<TileSetSource> source = tile_set->get_source(cell.source_id);
				Ref<TileSetAtlasSource> atlas_source = source;
				if (atlas_source.is_valid()) {
					tile_data = atlas_source->get_tile_data(cell.get_atlas_coords(), cell.alternative_tile);
				}
			}

			if (tile_data && tile_data->get_terrain_set() == p_terrain_set && tile_data->get_terrain() == p_terrain) {
				connect = true;
			}
		}
		if (connect) {
			cells_with_terrain_center_bit.insert(coords);
		}
	}

	RBSet<TerrainConstraint> constraints;

	// Add new constraints from the path drawn.
	for (Vector2i coords : p_coords_array) {
		// Constraints on the center bit.
		TerrainConstraint c = TerrainConstraint(tile_set, coords, p_terrain);
		c.set_priority(10);
		constraints.insert(c);

		// Constraints on the connecting bits.
		for (int j = 0; j < TileSet::CELL_NEIGHBOR_MAX; j++) {
			TileSet::CellNeighbor bit = TileSet::CellNeighbor(j);
			if (tile_set->is_valid_terrain_peering_bit(p_terrain_set, bit)) {
				c = TerrainConstraint(tile_set, coords, bit, p_terrain);
				c.set_priority(10);
				if ((int(bit) % 2) == 0) {
					// Side peering bits: add the constraint if the center is of the same terrain.
					Vector2i neighbor = tile_set->get_neighbor_cell(coords, bit);
					if (cells_with_terrain_center_bit.has(neighbor)) {
						constraints.insert(c);
					}
				} else {
					// Corner peering bits: add the constraint if all tiles on the constraint has the same center bit.
					HashMap<Vector2i, TileSet::CellNeighbor> overlapping_terrain_bits = c.get_overlapping_coords_and_peering_bits();
					bool valid = true;
					for (KeyValue<Vector2i, TileSet::CellNeighbor> kv : overlapping_terrain_bits) {
						if (!cells_with_terrain_center_bit.has(kv.key)) {
							valid = false;
							break;
						}
					}
					if (valid) {
						constraints.insert(c);
					}
				}
			}
		}
	}

	// Fills in the constraint list from existing tiles.
	for (TerrainConstraint c : _get_terrain_constraints_from_painted_cells_list(painted_set, p_terrain_set, p_ignore_empty_terrains)) {
		constraints.insert(c);
	}

	// Fill the terrains.
	output = terrain_fill_constraints(can_modify_list, p_terrain_set, constraints);
	return output;
}

HashMap<Vector2i, TileSet::TerrainsPattern> TileMapLayer::terrain_fill_path(const Vector<Vector2i> &p_coords_array, int p_terrain_set, int p_terrain, bool p_ignore_empty_terrains) const {
	HashMap<Vector2i, TileSet::TerrainsPattern> output;
	ERR_FAIL_COND_V(tile_set.is_null(), output);
	ERR_FAIL_INDEX_V(p_terrain_set, tile_set->get_terrain_sets_count(), output);

	// Make sure the path is correct and build the peering bit list while doing it.
	Vector<TileSet::CellNeighbor> neighbor_list;
	for (int i = 0; i < p_coords_array.size() - 1; i++) {
		// Find the adequate neighbor.
		TileSet::CellNeighbor found_bit = TileSet::CELL_NEIGHBOR_MAX;
		for (int j = 0; j < TileSet::CELL_NEIGHBOR_MAX; j++) {
			TileSet::CellNeighbor bit = TileSet::CellNeighbor(j);
			if (tile_set->is_existing_neighbor(bit)) {
				if (tile_set->get_neighbor_cell(p_coords_array[i], bit) == p_coords_array[i + 1]) {
					found_bit = bit;
					break;
				}
			}
		}
		ERR_FAIL_COND_V_MSG(found_bit == TileSet::CELL_NEIGHBOR_MAX, output, vformat("Invalid terrain path, %s is not a neighboring tile of %s", p_coords_array[i + 1], p_coords_array[i]));
		neighbor_list.push_back(found_bit);
	}

	// Build list and set of tiles that can be modified (painted and their surroundings).
	Vector<Vector2i> can_modify_list;
	RBSet<Vector2i> can_modify_set;
	RBSet<Vector2i> painted_set;
	for (int i = p_coords_array.size() - 1; i >= 0; i--) {
		const Vector2i &coords = p_coords_array[i];
		can_modify_list.push_back(coords);
		can_modify_set.insert(coords);
		painted_set.insert(coords);
	}
	for (Vector2i coords : p_coords_array) {
		// Find the adequate neighbor.
		for (int j = 0; j < TileSet::CELL_NEIGHBOR_MAX; j++) {
			TileSet::CellNeighbor bit = TileSet::CellNeighbor(j);
			if (tile_set->is_valid_terrain_peering_bit(p_terrain_set, bit)) {
				Vector2i neighbor = tile_set->get_neighbor_cell(coords, bit);
				if (!can_modify_set.has(neighbor)) {
					can_modify_list.push_back(neighbor);
					can_modify_set.insert(neighbor);
				}
			}
		}
	}

	RBSet<TerrainConstraint> constraints;

	// Add new constraints from the path drawn.
	for (Vector2i coords : p_coords_array) {
		// Constraints on the center bit.
		TerrainConstraint c = TerrainConstraint(tile_set, coords, p_terrain);
		c.set_priority(10);
		constraints.insert(c);
	}
	for (int i = 0; i < p_coords_array.size() - 1; i++) {
		// Constraints on the peering bits.
		TerrainConstraint c = TerrainConstraint(tile_set, p_coords_array[i], neighbor_list[i], p_terrain);
		c.set_priority(10);
		constraints.insert(c);
	}

	// Fills in the constraint list from existing tiles.
	for (TerrainConstraint c : _get_terrain_constraints_from_painted_cells_list(painted_set, p_terrain_set, p_ignore_empty_terrains)) {
		constraints.insert(c);
	}

	// Fill the terrains.
	output = terrain_fill_constraints(can_modify_list, p_terrain_set, constraints);
	return output;
}

HashMap<Vector2i, TileSet::TerrainsPattern> TileMapLayer::terrain_fill_pattern(const Vector<Vector2i> &p_coords_array, int p_terrain_set, TileSet::TerrainsPattern p_terrains_pattern, bool p_ignore_empty_terrains) const {
	HashMap<Vector2i, TileSet::TerrainsPattern> output;
	ERR_FAIL_COND_V(tile_set.is_null(), output);
	ERR_FAIL_INDEX_V(p_terrain_set, tile_set->get_terrain_sets_count(), output);

	// Build list and set of tiles that can be modified (painted and their surroundings).
	Vector<Vector2i> can_modify_list;
	RBSet<Vector2i> can_modify_set;
	RBSet<Vector2i> painted_set;
	for (int i = p_coords_array.size() - 1; i >= 0; i--) {
		const Vector2i &coords = p_coords_array[i];
		can_modify_list.push_back(coords);
		can_modify_set.insert(coords);
		painted_set.insert(coords);
	}
	for (Vector2i coords : p_coords_array) {
		// Find the adequate neighbor.
		for (int j = 0; j < TileSet::CELL_NEIGHBOR_MAX; j++) {
			TileSet::CellNeighbor bit = TileSet::CellNeighbor(j);
			if (tile_set->is_valid_terrain_peering_bit(p_terrain_set, bit)) {
				Vector2i neighbor = tile_set->get_neighbor_cell(coords, bit);
				if (!can_modify_set.has(neighbor)) {
					can_modify_list.push_back(neighbor);
					can_modify_set.insert(neighbor);
				}
			}
		}
	}

	// Add constraint by the new ones.
	RBSet<TerrainConstraint> constraints;

	// Add new constraints from the path drawn.
	for (Vector2i coords : p_coords_array) {
		// Constraints on the center bit.
		RBSet<TerrainConstraint> added_constraints = _get_terrain_constraints_from_added_pattern(coords, p_terrain_set, p_terrains_pattern);
		for (TerrainConstraint c : added_constraints) {
			c.set_priority(10);
			constraints.insert(c);
		}
	}

	// Fills in the constraint list from modified tiles border.
	for (TerrainConstraint c : _get_terrain_constraints_from_painted_cells_list(painted_set, p_terrain_set, p_ignore_empty_terrains)) {
		constraints.insert(c);
	}

	// Fill the terrains.
	output = terrain_fill_constraints(can_modify_list, p_terrain_set, constraints);
	return output;
}

TileMapCell TileMapLayer::get_cell(const Vector2i &p_coords) const {
	if (!tile_map_layer_data.has(p_coords)) {
		return TileMapCell();
	} else {
		return tile_map_layer_data.find(p_coords)->value.cell;
	}
}

void TileMapLayer::draw_tile(RID p_canvas_item, const Vector2 &p_position, const Ref<TileSet> p_tile_set, int p_atlas_source_id, const Vector2i &p_atlas_coords, int p_alternative_tile, int p_frame, Color p_modulation, const TileData *p_tile_data_override, real_t p_normalized_animation_offset) {
	ERR_FAIL_COND(p_tile_set.is_null());
	ERR_FAIL_COND(!p_tile_set->has_source(p_atlas_source_id));
	ERR_FAIL_COND(!p_tile_set->get_source(p_atlas_source_id)->has_tile(p_atlas_coords));
	ERR_FAIL_COND(!p_tile_set->get_source(p_atlas_source_id)->has_alternative_tile(p_atlas_coords, p_alternative_tile));
	TileSetSource *source = *p_tile_set->get_source(p_atlas_source_id);
	TileSetAtlasSource *atlas_source = Object::cast_to<TileSetAtlasSource>(source);
	if (atlas_source) {
		// Check for the frame.
		if (p_frame >= 0) {
			ERR_FAIL_INDEX(p_frame, atlas_source->get_tile_animation_frames_count(p_atlas_coords));
		}

		// Get the texture.
		Ref<Texture2D> tex = atlas_source->get_runtime_texture();
		if (tex.is_null()) {
			return;
		}

		// Check if we are in the texture, return otherwise.
		Vector2i grid_size = atlas_source->get_atlas_grid_size();
		if (p_atlas_coords.x >= grid_size.x || p_atlas_coords.y >= grid_size.y) {
			return;
		}

		// Get tile data.
		const TileData *tile_data = p_tile_data_override ? p_tile_data_override : atlas_source->get_tile_data(p_atlas_coords, p_alternative_tile);

		// Get the tile modulation.
		Color modulate = tile_data->get_modulate() * p_modulation;

		// Compute the offset.
		Vector2 tile_offset = tile_data->get_texture_origin();

		// Get destination rect.
		Rect2 dest_rect;
		dest_rect.size = atlas_source->get_runtime_tile_texture_region(p_atlas_coords).size;
		dest_rect.size.x += FP_ADJUST;
		dest_rect.size.y += FP_ADJUST;

		bool transpose = tile_data->get_transpose() ^ bool(p_alternative_tile & TileSetAtlasSource::TRANSFORM_TRANSPOSE);
		if (transpose) {
			dest_rect.position = (p_position - Vector2(dest_rect.size.y, dest_rect.size.x) / 2 - tile_offset);
		} else {
			dest_rect.position = (p_position - dest_rect.size / 2 - tile_offset);
		}

		if (tile_data->get_flip_h() ^ bool(p_alternative_tile & TileSetAtlasSource::TRANSFORM_FLIP_H)) {
			dest_rect.size.x = -dest_rect.size.x;
		}

		if (tile_data->get_flip_v() ^ bool(p_alternative_tile & TileSetAtlasSource::TRANSFORM_FLIP_V)) {
			dest_rect.size.y = -dest_rect.size.y;
		}

		// Draw the tile.
		if (p_frame >= 0) {
			Rect2i source_rect = atlas_source->get_runtime_tile_texture_region(p_atlas_coords, p_frame);
			tex->draw_rect_region(p_canvas_item, dest_rect, source_rect, modulate, transpose, p_tile_set->is_uv_clipping());
		} else if (atlas_source->get_tile_animation_frames_count(p_atlas_coords) == 1) {
			Rect2i source_rect = atlas_source->get_runtime_tile_texture_region(p_atlas_coords, 0);
			tex->draw_rect_region(p_canvas_item, dest_rect, source_rect, modulate, transpose, p_tile_set->is_uv_clipping());
		} else {
			real_t speed = atlas_source->get_tile_animation_speed(p_atlas_coords);
			real_t animation_duration = atlas_source->get_tile_animation_total_duration(p_atlas_coords) / speed;
			real_t animation_offset = p_normalized_animation_offset * animation_duration;
			// Accumulate durations unaffected by the speed to avoid accumulating floating point division errors.
			// Aka do `sum(duration[i]) / speed` instead of `sum(duration[i] / speed)`.
			real_t time_unscaled = 0.0;
			for (int frame = 0; frame < atlas_source->get_tile_animation_frames_count(p_atlas_coords); frame++) {
				real_t frame_duration_unscaled = atlas_source->get_tile_animation_frame_duration(p_atlas_coords, frame);
				real_t slice_start = time_unscaled / speed;
				real_t slice_end = (time_unscaled + frame_duration_unscaled) / speed;
				RenderingServer::get_singleton()->canvas_item_add_animation_slice(p_canvas_item, animation_duration, slice_start, slice_end, animation_offset);

				Rect2i source_rect = atlas_source->get_runtime_tile_texture_region(p_atlas_coords, frame);
				tex->draw_rect_region(p_canvas_item, dest_rect, source_rect, modulate, transpose, p_tile_set->is_uv_clipping());

				time_unscaled += frame_duration_unscaled;
			}
			RenderingServer::get_singleton()->canvas_item_add_animation_slice(p_canvas_item, 1.0, 0.0, 1.0, 0.0);
		}
	}
}

void TileMapLayer::set_cell(const Vector2i &p_coords, int p_source_id, const Vector2i &p_atlas_coords, int p_alternative_tile) {
	// Set the current cell tile (using integer position).
	Vector2i pk(p_coords);
	HashMap<Vector2i, CellData>::Iterator E = tile_map_layer_data.find(pk);

	int source_id = p_source_id;
	Vector2i atlas_coords = p_atlas_coords;
	int alternative_tile = p_alternative_tile;

	if ((source_id == TileSet::INVALID_SOURCE || atlas_coords == TileSetSource::INVALID_ATLAS_COORDS || alternative_tile == TileSetSource::INVALID_TILE_ALTERNATIVE) &&
			(source_id != TileSet::INVALID_SOURCE || atlas_coords != TileSetSource::INVALID_ATLAS_COORDS || alternative_tile != TileSetSource::INVALID_TILE_ALTERNATIVE)) {
		source_id = TileSet::INVALID_SOURCE;
		atlas_coords = TileSetSource::INVALID_ATLAS_COORDS;
		alternative_tile = TileSetSource::INVALID_TILE_ALTERNATIVE;
	}

	if (!E) {
		if (source_id == TileSet::INVALID_SOURCE) {
			return; // Nothing to do, the tile is already empty.
		}

		// Insert a new cell in the tile map.
		CellData new_cell_data;
		new_cell_data.coords = pk;
		E = tile_map_layer_data.insert(pk, new_cell_data);
	} else {
		if (E->value.cell.source_id == source_id && E->value.cell.get_atlas_coords() == atlas_coords && E->value.cell.alternative_tile == alternative_tile) {
			return; // Nothing changed.
		}
	}

	TileMapCell &c = E->value.cell;
	c.source_id = source_id;
	c.set_atlas_coords(atlas_coords);
	c.alternative_tile = alternative_tile;

	// Make the given cell dirty.
	if (!E->value.dirty_list_element.in_list()) {
		dirty.cell_list.add(&(E->value.dirty_list_element));
	}
	_queue_internal_update();

	used_rect_cache_dirty = true;
}

void TileMapLayer::erase_cell(const Vector2i &p_coords) {
	set_cell(p_coords, TileSet::INVALID_SOURCE, TileSetSource::INVALID_ATLAS_COORDS, TileSetSource::INVALID_TILE_ALTERNATIVE);
}

void TileMapLayer::fix_invalid_tiles() {
	ERR_FAIL_COND_MSG(tile_set.is_null(), "Cannot call fix_invalid_tiles() on a TileMapLayer without a valid TileSet.");

	RBSet<Vector2i> coords;
	for (const KeyValue<Vector2i, CellData> &E : tile_map_layer_data) {
		TileSetSource *source = *tile_set->get_source(E.value.cell.source_id);
		if (!source || !source->has_tile(E.value.cell.get_atlas_coords()) || !source->has_alternative_tile(E.value.cell.get_atlas_coords(), E.value.cell.alternative_tile)) {
			coords.insert(E.key);
		}
	}
	for (const Vector2i &E : coords) {
		set_cell(E, TileSet::INVALID_SOURCE, TileSetSource::INVALID_ATLAS_COORDS, TileSetSource::INVALID_TILE_ALTERNATIVE);
	}
}

void TileMapLayer::clear() {
	// Remove all tiles.
	for (KeyValue<Vector2i, CellData> &kv : tile_map_layer_data) {
		erase_cell(kv.key);
	}
	used_rect_cache_dirty = true;
}

int TileMapLayer::get_cell_source_id(const Vector2i &p_coords) const {
	// Get a cell source id from position.
	HashMap<Vector2i, CellData>::ConstIterator E = tile_map_layer_data.find(p_coords);

	if (!E) {
		return TileSet::INVALID_SOURCE;
	}

	return E->value.cell.source_id;
}

Vector2i TileMapLayer::get_cell_atlas_coords(const Vector2i &p_coords) const {
	// Get a cell source id from position.
	HashMap<Vector2i, CellData>::ConstIterator E = tile_map_layer_data.find(p_coords);

	if (!E) {
		return TileSetSource::INVALID_ATLAS_COORDS;
	}

	return E->value.cell.get_atlas_coords();
}

int TileMapLayer::get_cell_alternative_tile(const Vector2i &p_coords) const {
	// Get a cell source id from position.
	HashMap<Vector2i, CellData>::ConstIterator E = tile_map_layer_data.find(p_coords);

	if (!E) {
		return TileSetSource::INVALID_TILE_ALTERNATIVE;
	}

	return E->value.cell.alternative_tile;
}

TileData *TileMapLayer::get_cell_tile_data(const Vector2i &p_coords) const {
	int source_id = get_cell_source_id(p_coords);
	if (source_id == TileSet::INVALID_SOURCE) {
		return nullptr;
	}

	Ref<TileSetAtlasSource> source = tile_set->get_source(source_id);
	if (source.is_valid()) {
		return source->get_tile_data(get_cell_atlas_coords(p_coords), get_cell_alternative_tile(p_coords));
	}

	return nullptr;
}

TypedArray<Vector2i> TileMapLayer::get_used_cells() const {
	// Returns the cells used in the tilemap.
	TypedArray<Vector2i> a;
	for (const KeyValue<Vector2i, CellData> &E : tile_map_layer_data) {
		const TileMapCell &c = E.value.cell;
		if (c.source_id == TileSet::INVALID_SOURCE) {
			continue;
		}
		a.push_back(E.key);
	}

	return a;
}

TypedArray<Vector2i> TileMapLayer::get_used_cells_by_id(int p_source_id, const Vector2i &p_atlas_coords, int p_alternative_tile) const {
	// Returns the cells used in the tilemap.
	TypedArray<Vector2i> a;
	for (const KeyValue<Vector2i, CellData> &E : tile_map_layer_data) {
		const TileMapCell &c = E.value.cell;
		if (c.source_id == TileSet::INVALID_SOURCE) {
			continue;
		}
		if ((p_source_id == TileSet::INVALID_SOURCE || p_source_id == c.source_id) &&
				(p_atlas_coords == TileSetSource::INVALID_ATLAS_COORDS || p_atlas_coords == c.get_atlas_coords()) &&
				(p_alternative_tile == TileSetSource::INVALID_TILE_ALTERNATIVE || p_alternative_tile == c.alternative_tile)) {
			a.push_back(E.key);
		}
	}

	return a;
}

Rect2i TileMapLayer::get_used_rect() const {
	// Return the rect of the currently used area.
	if (used_rect_cache_dirty) {
		used_rect_cache = Rect2i();

		bool first = true;
		for (const KeyValue<Vector2i, CellData> &E : tile_map_layer_data) {
			const TileMapCell &c = E.value.cell;
			if (c.source_id == TileSet::INVALID_SOURCE) {
				continue;
			}
			if (first) {
				used_rect_cache = Rect2i(E.key, Size2i());
				first = false;
			} else {
				used_rect_cache.expand_to(E.key);
			}
		}
		if (!first) {
			// Only if we have at least one cell.
			// The cache expands to top-left coordinate, so we add one full tile.
			used_rect_cache.size += Vector2i(1, 1);
		}
		used_rect_cache_dirty = false;
	}

	return used_rect_cache;
}

bool TileMapLayer::is_cell_flipped_h(const Vector2i &p_coords) const {
	return get_cell_alternative_tile(p_coords) & TileSetAtlasSource::TRANSFORM_FLIP_H;
}

bool TileMapLayer::is_cell_flipped_v(const Vector2i &p_coords) const {
	return get_cell_alternative_tile(p_coords) & TileSetAtlasSource::TRANSFORM_FLIP_V;
}

bool TileMapLayer::is_cell_transposed(const Vector2i &p_coords) const {
	return get_cell_alternative_tile(p_coords) & TileSetAtlasSource::TRANSFORM_TRANSPOSE;
}

Ref<TileMapPattern> TileMapLayer::get_pattern(TypedArray<Vector2i> p_coords_array) {
	ERR_FAIL_COND_V(tile_set.is_null(), nullptr);

	Ref<TileMapPattern> output;
	output.instantiate();
	if (p_coords_array.is_empty()) {
		return output;
	}

	Vector2i min = Vector2i(p_coords_array[0]);
	for (int i = 1; i < p_coords_array.size(); i++) {
		min = min.min(p_coords_array[i]);
	}

	Vector<Vector2i> coords_in_pattern_array;
	coords_in_pattern_array.resize(p_coords_array.size());
	Vector2i ensure_positive_offset;
	for (int i = 0; i < p_coords_array.size(); i++) {
		Vector2i coords = p_coords_array[i];
		Vector2i coords_in_pattern = coords - min;
		if (tile_set->get_tile_shape() != TileSet::TILE_SHAPE_SQUARE) {
			if (tile_set->get_tile_layout() == TileSet::TILE_LAYOUT_STACKED) {
				if (tile_set->get_tile_offset_axis() == TileSet::TILE_OFFSET_AXIS_HORIZONTAL && bool(min.y % 2) && bool(coords_in_pattern.y % 2)) {
					coords_in_pattern.x -= 1;
					if (coords_in_pattern.x < 0) {
						ensure_positive_offset.x = 1;
					}
				} else if (tile_set->get_tile_offset_axis() == TileSet::TILE_OFFSET_AXIS_VERTICAL && bool(min.x % 2) && bool(coords_in_pattern.x % 2)) {
					coords_in_pattern.y -= 1;
					if (coords_in_pattern.y < 0) {
						ensure_positive_offset.y = 1;
					}
				}
			} else if (tile_set->get_tile_layout() == TileSet::TILE_LAYOUT_STACKED_OFFSET) {
				if (tile_set->get_tile_offset_axis() == TileSet::TILE_OFFSET_AXIS_HORIZONTAL && bool(min.y % 2) && bool(coords_in_pattern.y % 2)) {
					coords_in_pattern.x += 1;
				} else if (tile_set->get_tile_offset_axis() == TileSet::TILE_OFFSET_AXIS_VERTICAL && bool(min.x % 2) && bool(coords_in_pattern.x % 2)) {
					coords_in_pattern.y += 1;
				}
			}
		}
		coords_in_pattern_array.write[i] = coords_in_pattern;
	}

	for (int i = 0; i < coords_in_pattern_array.size(); i++) {
		Vector2i coords = p_coords_array[i];
		Vector2i coords_in_pattern = coords_in_pattern_array[i];
		output->set_cell(coords_in_pattern + ensure_positive_offset, get_cell_source_id(coords), get_cell_atlas_coords(coords), get_cell_alternative_tile(coords));
	}

	return output;
}

void TileMapLayer::set_pattern(const Vector2i &p_position, const Ref<TileMapPattern> p_pattern) {
	ERR_FAIL_COND(tile_set.is_null());
	ERR_FAIL_COND(p_pattern.is_null());

	TypedArray<Vector2i> used_cells = p_pattern->get_used_cells();
	for (int i = 0; i < used_cells.size(); i++) {
		Vector2i coords = tile_set->map_pattern(p_position, used_cells[i], p_pattern);
		set_cell(coords, p_pattern->get_cell_source_id(used_cells[i]), p_pattern->get_cell_atlas_coords(used_cells[i]), p_pattern->get_cell_alternative_tile(used_cells[i]));
	}
}

void TileMapLayer::set_cells_terrain_connect(TypedArray<Vector2i> p_cells, int p_terrain_set, int p_terrain, bool p_ignore_empty_terrains) {
	ERR_FAIL_COND(tile_set.is_null());
	ERR_FAIL_INDEX(p_terrain_set, tile_set->get_terrain_sets_count());

	Vector<Vector2i> cells_vector;
	HashSet<Vector2i> painted_set;
	for (int i = 0; i < p_cells.size(); i++) {
		cells_vector.push_back(p_cells[i]);
		painted_set.insert(p_cells[i]);
	}
	HashMap<Vector2i, TileSet::TerrainsPattern> terrain_fill_output = terrain_fill_connect(cells_vector, p_terrain_set, p_terrain, p_ignore_empty_terrains);
	for (const KeyValue<Vector2i, TileSet::TerrainsPattern> &kv : terrain_fill_output) {
		if (painted_set.has(kv.key)) {
			// Paint a random tile with the correct terrain for the painted path.
			TileMapCell c = tile_set->get_random_tile_from_terrains_pattern(p_terrain_set, kv.value);
			set_cell(kv.key, c.source_id, c.get_atlas_coords(), c.alternative_tile);
		} else {
			// Avoids updating the painted path from the output if the new pattern is the same as before.
			TileSet::TerrainsPattern in_map_terrain_pattern = TileSet::TerrainsPattern(*tile_set, p_terrain_set);
			TileMapCell cell = get_cell(kv.key);
			if (cell.source_id != TileSet::INVALID_SOURCE) {
				TileSetSource *source = *tile_set->get_source(cell.source_id);
				TileSetAtlasSource *atlas_source = Object::cast_to<TileSetAtlasSource>(source);
				if (atlas_source) {
					// Get tile data.
					TileData *tile_data = atlas_source->get_tile_data(cell.get_atlas_coords(), cell.alternative_tile);
					if (tile_data && tile_data->get_terrain_set() == p_terrain_set) {
						in_map_terrain_pattern = tile_data->get_terrains_pattern();
					}
				}
			}
			if (in_map_terrain_pattern != kv.value) {
				TileMapCell c = tile_set->get_random_tile_from_terrains_pattern(p_terrain_set, kv.value);
				set_cell(kv.key, c.source_id, c.get_atlas_coords(), c.alternative_tile);
			}
		}
	}
}

void TileMapLayer::set_cells_terrain_path(TypedArray<Vector2i> p_path, int p_terrain_set, int p_terrain, bool p_ignore_empty_terrains) {
	ERR_FAIL_COND(tile_set.is_null());
	ERR_FAIL_INDEX(p_terrain_set, tile_set->get_terrain_sets_count());

	Vector<Vector2i> vector_path;
	HashSet<Vector2i> painted_set;
	for (int i = 0; i < p_path.size(); i++) {
		vector_path.push_back(p_path[i]);
		painted_set.insert(p_path[i]);
	}

	HashMap<Vector2i, TileSet::TerrainsPattern> terrain_fill_output = terrain_fill_path(vector_path, p_terrain_set, p_terrain, p_ignore_empty_terrains);
	for (const KeyValue<Vector2i, TileSet::TerrainsPattern> &kv : terrain_fill_output) {
		if (painted_set.has(kv.key)) {
			// Paint a random tile with the correct terrain for the painted path.
			TileMapCell c = tile_set->get_random_tile_from_terrains_pattern(p_terrain_set, kv.value);
			set_cell(kv.key, c.source_id, c.get_atlas_coords(), c.alternative_tile);
		} else {
			// Avoids updating the painted path from the output if the new pattern is the same as before.
			TileSet::TerrainsPattern in_map_terrain_pattern = TileSet::TerrainsPattern(*tile_set, p_terrain_set);
			TileMapCell cell = get_cell(kv.key);
			if (cell.source_id != TileSet::INVALID_SOURCE) {
				TileSetSource *source = *tile_set->get_source(cell.source_id);
				TileSetAtlasSource *atlas_source = Object::cast_to<TileSetAtlasSource>(source);
				if (atlas_source) {
					// Get tile data.
					TileData *tile_data = atlas_source->get_tile_data(cell.get_atlas_coords(), cell.alternative_tile);
					if (tile_data && tile_data->get_terrain_set() == p_terrain_set) {
						in_map_terrain_pattern = tile_data->get_terrains_pattern();
					}
				}
			}
			if (in_map_terrain_pattern != kv.value) {
				TileMapCell c = tile_set->get_random_tile_from_terrains_pattern(p_terrain_set, kv.value);
				set_cell(kv.key, c.source_id, c.get_atlas_coords(), c.alternative_tile);
			}
		}
	}
}

bool TileMapLayer::has_body_rid(RID p_physics_body) const {
	return bodies_coords.has(p_physics_body);
}

Vector2i TileMapLayer::get_coords_for_body_rid(RID p_physics_body) const {
	const Vector2i *found = bodies_coords.getptr(p_physics_body);
	ERR_FAIL_NULL_V(found, Vector2i());
	return *found;
}

void TileMapLayer::update_internals() {
	_internal_update(false);
}

void TileMapLayer::notify_runtime_tile_data_update() {
	dirty.flags[TileMapLayer::DIRTY_FLAGS_LAYER_RUNTIME_UPDATE] = true;
	_queue_internal_update();
	emit_signal(CoreStringName(changed));
}

Vector2i TileMapLayer::map_pattern(const Vector2i &p_position_in_tilemap, const Vector2i &p_coords_in_pattern, Ref<TileMapPattern> p_pattern) {
	ERR_FAIL_COND_V(tile_set.is_null(), Vector2i());
	return tile_set->map_pattern(p_position_in_tilemap, p_coords_in_pattern, p_pattern);
}

TypedArray<Vector2i> TileMapLayer::get_surrounding_cells(const Vector2i &p_coords) {
	ERR_FAIL_COND_V(tile_set.is_null(), TypedArray<Vector2i>());
	return tile_set->get_surrounding_cells(p_coords);
}

Vector2i TileMapLayer::get_neighbor_cell(const Vector2i &p_coords, TileSet::CellNeighbor p_cell_neighbor) const {
	ERR_FAIL_COND_V(tile_set.is_null(), Vector2i());
	return tile_set->get_neighbor_cell(p_coords, p_cell_neighbor);
}

Vector2 TileMapLayer::map_to_local(const Vector2i &p_pos) const {
	ERR_FAIL_COND_V(tile_set.is_null(), Vector2());
	return tile_set->map_to_local(p_pos);
}

Vector2i TileMapLayer::local_to_map(const Vector2 &p_pos) const {
	ERR_FAIL_COND_V(tile_set.is_null(), Vector2i());
	return tile_set->local_to_map(p_pos);
}

void TileMapLayer::set_enabled(bool p_enabled) {
	if (enabled == p_enabled) {
		return;
	}
	enabled = p_enabled;
	dirty.flags[DIRTY_FLAGS_LAYER_ENABLED] = true;
	_queue_internal_update();
	emit_signal(CoreStringName(changed));
}

bool TileMapLayer::is_enabled() const {
	return enabled;
}

void TileMapLayer::set_tile_set(const Ref<TileSet> &p_tile_set) {
	if (p_tile_set == tile_set) {
		return;
	}

	dirty.flags[DIRTY_FLAGS_TILE_SET] = true;
	_queue_internal_update();

	// Set the TileSet, registering to its changes.
	if (tile_set.is_valid()) {
		tile_set->disconnect_changed(callable_mp(this, &TileMapLayer::_tile_set_changed));
	}

	tile_set = p_tile_set;

	if (tile_set.is_valid()) {
		tile_set->connect_changed(callable_mp(this, &TileMapLayer::_tile_set_changed));
	}

	emit_signal(CoreStringName(changed));

	// Trigger updates for TileSet's read-only status.
	notify_property_list_changed();
}

Ref<TileSet> TileMapLayer::get_tile_set() const {
	return tile_set;
}

void TileMapLayer::set_highlight_mode(HighlightMode p_highlight_mode) {
	if (p_highlight_mode == highlight_mode) {
		return;
	}
	highlight_mode = p_highlight_mode;
	_queue_internal_update();
}

TileMapLayer::HighlightMode TileMapLayer::get_highlight_mode() const {
	return highlight_mode;
}

void TileMapLayer::set_tile_map_data_from_array(const Vector<uint8_t> &p_data) {
	if (p_data.is_empty()) {
		clear();
		return;
	}

	const int cell_data_struct_size = 12;

	int size = p_data.size();
	const uint8_t *ptr = p_data.ptr();

	// Index in the array.
	int index = 0;

	// First extract the data version.
	ERR_FAIL_COND_MSG(size < 2, "Corrupted tile map data: not enough bytes.");
	uint16_t format = decode_uint16(&ptr[index]);
	index += 2;
	ERR_FAIL_COND_MSG(format >= TileMapLayerDataFormat::TILE_MAP_LAYER_DATA_FORMAT_MAX, vformat("Unsupported tile map data format: %s. Expected format ID lower or equal to: %s", format, TileMapLayerDataFormat::TILE_MAP_LAYER_DATA_FORMAT_MAX - 1));

	// Clear the TileMap.
	clear();

	while (index < size) {
		ERR_FAIL_COND_MSG(index + cell_data_struct_size > size, vformat("Corrupted tile map data: tiles might be missing."));

		// Get a pointer at the start of the cell data.
		const uint8_t *cell_data_ptr = &ptr[index];

		// Extracts position in TileMap.
		int16_t x = decode_uint16(&cell_data_ptr[0]);
		int16_t y = decode_uint16(&cell_data_ptr[2]);

		// Extracts the tile identifiers.
		uint16_t source_id = decode_uint16(&cell_data_ptr[4]);
		uint16_t atlas_coords_x = decode_uint16(&cell_data_ptr[6]);
		uint16_t atlas_coords_y = decode_uint16(&cell_data_ptr[8]);
		uint16_t alternative_tile = decode_uint16(&cell_data_ptr[10]);

		set_cell(Vector2i(x, y), source_id, Vector2i(atlas_coords_x, atlas_coords_y), alternative_tile);
		index += cell_data_struct_size;
	}
}

Vector<uint8_t> TileMapLayer::get_tile_map_data_as_array() const {
	const int cell_data_struct_size = 12;

	Vector<uint8_t> tile_map_data_array;
	if (tile_map_layer_data.is_empty()) {
		return tile_map_data_array;
	}

	tile_map_data_array.resize(2 + tile_map_layer_data.size() * cell_data_struct_size);
	uint8_t *ptr = tile_map_data_array.ptrw();

	// Index in the array.
	int index = 0;

	// Save the version.
	encode_uint16(TileMapLayerDataFormat::TILE_MAP_LAYER_DATA_FORMAT_MAX - 1, &ptr[index]);
	index += 2;

	// Save in highest format.
	for (const KeyValue<Vector2i, CellData> &E : tile_map_layer_data) {
		// Get a pointer at the start of the cell data.
		uint8_t *cell_data_ptr = (uint8_t *)&ptr[index];

		// Store position in TileMap.
		encode_uint16((int16_t)(E.key.x), &cell_data_ptr[0]);
		encode_uint16((int16_t)(E.key.y), &cell_data_ptr[2]);

		// Store the tile identifiers.
		encode_uint16(E.value.cell.source_id, &cell_data_ptr[4]);
		encode_uint16(E.value.cell.coord_x, &cell_data_ptr[6]);
		encode_uint16(E.value.cell.coord_y, &cell_data_ptr[8]);
		encode_uint16(E.value.cell.alternative_tile, &cell_data_ptr[10]);

		index += cell_data_struct_size;
	}

	return tile_map_data_array;
}

void TileMapLayer::set_self_modulate(const Color &p_self_modulate) {
	if (get_self_modulate() == p_self_modulate) {
		return;
	}
	CanvasItem::set_self_modulate(p_self_modulate);
	dirty.flags[DIRTY_FLAGS_LAYER_SELF_MODULATE] = true;
	_queue_internal_update();
	emit_signal(CoreStringName(changed));
}

void TileMapLayer::set_y_sort_enabled(bool p_y_sort_enabled) {
	if (is_y_sort_enabled() == p_y_sort_enabled) {
		return;
	}
	CanvasItem::set_y_sort_enabled(p_y_sort_enabled);
	dirty.flags[DIRTY_FLAGS_LAYER_Y_SORT_ENABLED] = true;
	_queue_internal_update();
	emit_signal(CoreStringName(changed));

	notify_property_list_changed();
	_update_notify_local_transform();
}

void TileMapLayer::set_y_sort_origin(int p_y_sort_origin) {
	if (y_sort_origin == p_y_sort_origin) {
		return;
	}
	y_sort_origin = p_y_sort_origin;
	dirty.flags[DIRTY_FLAGS_LAYER_Y_SORT_ORIGIN] = true;
	_queue_internal_update();
	emit_signal(CoreStringName(changed));
}

int TileMapLayer::get_y_sort_origin() const {
	return y_sort_origin;
}

void TileMapLayer::set_x_draw_order_reversed(bool p_x_draw_order_reversed) {
	if (x_draw_order_reversed == p_x_draw_order_reversed) {
		return;
	}
	x_draw_order_reversed = p_x_draw_order_reversed;
	dirty.flags[DIRTY_FLAGS_LAYER_X_DRAW_ORDER_REVERSED] = true;
	_queue_internal_update();
	emit_signal(CoreStringName(changed));
}

bool TileMapLayer::is_x_draw_order_reversed() const {
	return x_draw_order_reversed;
}

void TileMapLayer::set_z_index(int p_z_index) {
	if (get_z_index() == p_z_index) {
		return;
	}
	CanvasItem::set_z_index(p_z_index);
	dirty.flags[DIRTY_FLAGS_LAYER_Z_INDEX] = true;
	_queue_internal_update();
	emit_signal(CoreStringName(changed));
}

void TileMapLayer::set_light_mask(int p_light_mask) {
	if (get_light_mask() == p_light_mask) {
		return;
	}
	CanvasItem::set_light_mask(p_light_mask);
	dirty.flags[DIRTY_FLAGS_LAYER_LIGHT_MASK] = true;
	_queue_internal_update();
	emit_signal(CoreStringName(changed));
}

void TileMapLayer::set_rendering_quadrant_size(int p_size) {
	if (rendering_quadrant_size == p_size) {
		return;
	}
	ERR_FAIL_COND_MSG(p_size < 1, "TileMapQuadrant size cannot be smaller than 1.");
	rendering_quadrant_size = p_size;
	dirty.flags[DIRTY_FLAGS_LAYER_RENDERING_QUADRANT_SIZE] = true;
	_queue_internal_update();
	emit_signal(CoreStringName(changed));
}

int TileMapLayer::get_rendering_quadrant_size() const {
	return rendering_quadrant_size;
}

void TileMapLayer::set_collision_enabled(bool p_enabled) {
	if (collision_enabled == p_enabled) {
		return;
	}
	collision_enabled = p_enabled;
	dirty.flags[DIRTY_FLAGS_LAYER_COLLISION_ENABLED] = true;
	_queue_internal_update();
	emit_signal(CoreStringName(changed));
}

bool TileMapLayer::is_collision_enabled() const {
	return collision_enabled;
}

void TileMapLayer::set_use_kinematic_bodies(bool p_use_kinematic_bodies) {
	if (use_kinematic_bodies == p_use_kinematic_bodies) {
		return;
	}
	use_kinematic_bodies = p_use_kinematic_bodies;
	dirty.flags[DIRTY_FLAGS_LAYER_USE_KINEMATIC_BODIES] = p_use_kinematic_bodies;
	_queue_internal_update();
	emit_signal(CoreStringName(changed));
}

bool TileMapLayer::is_using_kinematic_bodies() const {
	return use_kinematic_bodies;
}

void TileMapLayer::set_collision_visibility_mode(TileMapLayer::DebugVisibilityMode p_show_collision) {
	if (collision_visibility_mode == p_show_collision) {
		return;
	}
	collision_visibility_mode = p_show_collision;
	dirty.flags[DIRTY_FLAGS_LAYER_COLLISION_VISIBILITY_MODE] = true;
	_queue_internal_update();
	emit_signal(CoreStringName(changed));
}

TileMapLayer::DebugVisibilityMode TileMapLayer::get_collision_visibility_mode() const {
	return collision_visibility_mode;
}

void TileMapLayer::set_occlusion_enabled(bool p_enabled) {
	if (occlusion_enabled == p_enabled) {
		return;
	}
	occlusion_enabled = p_enabled;
	dirty.flags[DIRTY_FLAGS_LAYER_OCCLUSION_ENABLED] = true;
	_queue_internal_update();
	emit_signal(CoreStringName(changed));
}

bool TileMapLayer::is_occlusion_enabled() const {
	return occlusion_enabled;
}

void TileMapLayer::set_navigation_enabled(bool p_enabled) {
	if (navigation_enabled == p_enabled) {
		return;
	}
	navigation_enabled = p_enabled;
	dirty.flags[DIRTY_FLAGS_LAYER_NAVIGATION_ENABLED] = true;
	_queue_internal_update();
	emit_signal(CoreStringName(changed));
}

bool TileMapLayer::is_navigation_enabled() const {
	return navigation_enabled;
}

void TileMapLayer::set_navigation_map(RID p_map) {
	if (navigation_map_override == p_map) {
		return;
	}
	navigation_map_override = p_map;
	dirty.flags[DIRTY_FLAGS_LAYER_NAVIGATION_MAP] = true;
	_queue_internal_update();
	emit_signal(CoreStringName(changed));
}

RID TileMapLayer::get_navigation_map() const {
	if (navigation_map_override.is_valid()) {
		return navigation_map_override;
	} else if (is_inside_tree()) {
		return get_world_2d()->get_navigation_map();
	}
	return RID();
}

void TileMapLayer::set_navigation_visibility_mode(TileMapLayer::DebugVisibilityMode p_show_navigation) {
	if (navigation_visibility_mode == p_show_navigation) {
		return;
	}
	navigation_visibility_mode = p_show_navigation;
	dirty.flags[DIRTY_FLAGS_LAYER_NAVIGATION_VISIBILITY_MODE] = true;
	_queue_internal_update();
	emit_signal(CoreStringName(changed));
}

TileMapLayer::DebugVisibilityMode TileMapLayer::get_navigation_visibility_mode() const {
	return navigation_visibility_mode;
}

TileMapLayer::TileMapLayer() {
	set_notify_transform(true);
}

TileMapLayer::~TileMapLayer() {
	clear();
	_internal_update(true);
}

HashMap<Vector2i, TileSet::CellNeighbor> TerrainConstraint::get_overlapping_coords_and_peering_bits() const {
	HashMap<Vector2i, TileSet::CellNeighbor> output;

	ERR_FAIL_COND_V(is_center_bit(), output);
	ERR_FAIL_COND_V(tile_set.is_null(), output);

	TileSet::TileShape shape = tile_set->get_tile_shape();
	if (shape == TileSet::TILE_SHAPE_SQUARE) {
		switch (bit) {
			case 1:
				output[base_cell_coords] = TileSet::CELL_NEIGHBOR_RIGHT_SIDE;
				output[tile_set->get_neighbor_cell(base_cell_coords, TileSet::CELL_NEIGHBOR_RIGHT_SIDE)] = TileSet::CELL_NEIGHBOR_LEFT_SIDE;
				break;
			case 2:
				output[base_cell_coords] = TileSet::CELL_NEIGHBOR_BOTTOM_RIGHT_CORNER;
				output[tile_set->get_neighbor_cell(base_cell_coords, TileSet::CELL_NEIGHBOR_RIGHT_SIDE)] = TileSet::CELL_NEIGHBOR_BOTTOM_LEFT_CORNER;
				output[tile_set->get_neighbor_cell(base_cell_coords, TileSet::CELL_NEIGHBOR_BOTTOM_RIGHT_CORNER)] = TileSet::CELL_NEIGHBOR_TOP_LEFT_CORNER;
				output[tile_set->get_neighbor_cell(base_cell_coords, TileSet::CELL_NEIGHBOR_BOTTOM_SIDE)] = TileSet::CELL_NEIGHBOR_TOP_RIGHT_CORNER;
				break;
			case 3:
				output[base_cell_coords] = TileSet::CELL_NEIGHBOR_BOTTOM_SIDE;
				output[tile_set->get_neighbor_cell(base_cell_coords, TileSet::CELL_NEIGHBOR_BOTTOM_SIDE)] = TileSet::CELL_NEIGHBOR_TOP_SIDE;
				break;
			default:
				ERR_FAIL_V(output);
		}
	} else if (shape == TileSet::TILE_SHAPE_ISOMETRIC) {
		switch (bit) {
			case 1:
				output[base_cell_coords] = TileSet::CELL_NEIGHBOR_BOTTOM_RIGHT_SIDE;
				output[tile_set->get_neighbor_cell(base_cell_coords, TileSet::CELL_NEIGHBOR_BOTTOM_RIGHT_SIDE)] = TileSet::CELL_NEIGHBOR_TOP_LEFT_SIDE;
				break;
			case 2:
				output[base_cell_coords] = TileSet::CELL_NEIGHBOR_BOTTOM_CORNER;
				output[tile_set->get_neighbor_cell(base_cell_coords, TileSet::CELL_NEIGHBOR_BOTTOM_RIGHT_SIDE)] = TileSet::CELL_NEIGHBOR_LEFT_CORNER;
				output[tile_set->get_neighbor_cell(base_cell_coords, TileSet::CELL_NEIGHBOR_BOTTOM_CORNER)] = TileSet::CELL_NEIGHBOR_TOP_CORNER;
				output[tile_set->get_neighbor_cell(base_cell_coords, TileSet::CELL_NEIGHBOR_BOTTOM_LEFT_SIDE)] = TileSet::CELL_NEIGHBOR_RIGHT_CORNER;
				break;
			case 3:
				output[base_cell_coords] = TileSet::CELL_NEIGHBOR_BOTTOM_LEFT_SIDE;
				output[tile_set->get_neighbor_cell(base_cell_coords, TileSet::CELL_NEIGHBOR_BOTTOM_LEFT_SIDE)] = TileSet::CELL_NEIGHBOR_TOP_RIGHT_SIDE;
				break;
			default:
				ERR_FAIL_V(output);
		}
	} else {
		// Half offset shapes.
		TileSet::TileOffsetAxis offset_axis = tile_set->get_tile_offset_axis();
		if (offset_axis == TileSet::TILE_OFFSET_AXIS_HORIZONTAL) {
			switch (bit) {
				case 1:
					output[base_cell_coords] = TileSet::CELL_NEIGHBOR_RIGHT_SIDE;
					output[tile_set->get_neighbor_cell(base_cell_coords, TileSet::CELL_NEIGHBOR_RIGHT_SIDE)] = TileSet::CELL_NEIGHBOR_LEFT_SIDE;
					break;
				case 2:
					output[base_cell_coords] = TileSet::CELL_NEIGHBOR_BOTTOM_RIGHT_CORNER;
					output[tile_set->get_neighbor_cell(base_cell_coords, TileSet::CELL_NEIGHBOR_RIGHT_SIDE)] = TileSet::CELL_NEIGHBOR_BOTTOM_LEFT_CORNER;
					output[tile_set->get_neighbor_cell(base_cell_coords, TileSet::CELL_NEIGHBOR_BOTTOM_RIGHT_SIDE)] = TileSet::CELL_NEIGHBOR_TOP_CORNER;
					break;
				case 3:
					output[base_cell_coords] = TileSet::CELL_NEIGHBOR_BOTTOM_RIGHT_SIDE;
					output[tile_set->get_neighbor_cell(base_cell_coords, TileSet::CELL_NEIGHBOR_BOTTOM_RIGHT_SIDE)] = TileSet::CELL_NEIGHBOR_TOP_LEFT_SIDE;
					break;
				case 4:
					output[base_cell_coords] = TileSet::CELL_NEIGHBOR_BOTTOM_CORNER;
					output[tile_set->get_neighbor_cell(base_cell_coords, TileSet::CELL_NEIGHBOR_BOTTOM_RIGHT_SIDE)] = TileSet::CELL_NEIGHBOR_TOP_LEFT_CORNER;
					output[tile_set->get_neighbor_cell(base_cell_coords, TileSet::CELL_NEIGHBOR_BOTTOM_LEFT_SIDE)] = TileSet::CELL_NEIGHBOR_TOP_RIGHT_CORNER;
					break;
				case 5:
					output[base_cell_coords] = TileSet::CELL_NEIGHBOR_BOTTOM_LEFT_SIDE;
					output[tile_set->get_neighbor_cell(base_cell_coords, TileSet::CELL_NEIGHBOR_BOTTOM_LEFT_SIDE)] = TileSet::CELL_NEIGHBOR_TOP_RIGHT_SIDE;
					break;
				default:
					ERR_FAIL_V(output);
			}
		} else {
			switch (bit) {
				case 1:
					output[base_cell_coords] = TileSet::CELL_NEIGHBOR_RIGHT_CORNER;
					output[tile_set->get_neighbor_cell(base_cell_coords, TileSet::CELL_NEIGHBOR_TOP_RIGHT_SIDE)] = TileSet::CELL_NEIGHBOR_BOTTOM_LEFT_CORNER;
					output[tile_set->get_neighbor_cell(base_cell_coords, TileSet::CELL_NEIGHBOR_BOTTOM_RIGHT_SIDE)] = TileSet::CELL_NEIGHBOR_TOP_LEFT_CORNER;
					break;
				case 2:
					output[base_cell_coords] = TileSet::CELL_NEIGHBOR_BOTTOM_RIGHT_SIDE;
					output[tile_set->get_neighbor_cell(base_cell_coords, TileSet::CELL_NEIGHBOR_BOTTOM_RIGHT_SIDE)] = TileSet::CELL_NEIGHBOR_TOP_LEFT_SIDE;
					break;
				case 3:
					output[base_cell_coords] = TileSet::CELL_NEIGHBOR_BOTTOM_RIGHT_CORNER;
					output[tile_set->get_neighbor_cell(base_cell_coords, TileSet::CELL_NEIGHBOR_BOTTOM_RIGHT_SIDE)] = TileSet::CELL_NEIGHBOR_LEFT_CORNER;
					output[tile_set->get_neighbor_cell(base_cell_coords, TileSet::CELL_NEIGHBOR_BOTTOM_SIDE)] = TileSet::CELL_NEIGHBOR_TOP_LEFT_CORNER;
					break;
				case 4:
					output[base_cell_coords] = TileSet::CELL_NEIGHBOR_BOTTOM_SIDE;
					output[tile_set->get_neighbor_cell(base_cell_coords, TileSet::CELL_NEIGHBOR_BOTTOM_SIDE)] = TileSet::CELL_NEIGHBOR_TOP_SIDE;
					break;
				case 5:
					output[base_cell_coords] = TileSet::CELL_NEIGHBOR_BOTTOM_LEFT_SIDE;
					output[tile_set->get_neighbor_cell(base_cell_coords, TileSet::CELL_NEIGHBOR_BOTTOM_LEFT_SIDE)] = TileSet::CELL_NEIGHBOR_TOP_RIGHT_SIDE;
					break;
				default:
					ERR_FAIL_V(output);
			}
		}
	}
	return output;
}

TerrainConstraint::TerrainConstraint(Ref<TileSet> p_tile_set, const Vector2i &p_position, int p_terrain) {
	ERR_FAIL_COND(p_tile_set.is_null());
	tile_set = p_tile_set;
	bit = 0;
	base_cell_coords = p_position;
	terrain = p_terrain;
}

TerrainConstraint::TerrainConstraint(Ref<TileSet> p_tile_set, const Vector2i &p_position, const TileSet::CellNeighbor &p_bit, int p_terrain) {
	// The way we build the constraint make it easy to detect conflicting constraints.
	ERR_FAIL_COND(p_tile_set.is_null());
	tile_set = p_tile_set;

	TileSet::TileShape shape = tile_set->get_tile_shape();
	if (shape == TileSet::TILE_SHAPE_SQUARE) {
		switch (p_bit) {
			case TileSet::CELL_NEIGHBOR_RIGHT_SIDE:
				bit = 1;
				base_cell_coords = p_position;
				break;
			case TileSet::CELL_NEIGHBOR_BOTTOM_RIGHT_CORNER:
				bit = 2;
				base_cell_coords = p_position;
				break;
			case TileSet::CELL_NEIGHBOR_BOTTOM_SIDE:
				bit = 3;
				base_cell_coords = p_position;
				break;
			case TileSet::CELL_NEIGHBOR_BOTTOM_LEFT_CORNER:
				bit = 2;
				base_cell_coords = tile_set->get_neighbor_cell(p_position, TileSet::CELL_NEIGHBOR_LEFT_SIDE);
				break;
			case TileSet::CELL_NEIGHBOR_LEFT_SIDE:
				bit = 1;
				base_cell_coords = tile_set->get_neighbor_cell(p_position, TileSet::CELL_NEIGHBOR_LEFT_SIDE);
				break;
			case TileSet::CELL_NEIGHBOR_TOP_LEFT_CORNER:
				bit = 2;
				base_cell_coords = tile_set->get_neighbor_cell(p_position, TileSet::CELL_NEIGHBOR_TOP_LEFT_CORNER);
				break;
			case TileSet::CELL_NEIGHBOR_TOP_SIDE:
				bit = 3;
				base_cell_coords = tile_set->get_neighbor_cell(p_position, TileSet::CELL_NEIGHBOR_TOP_SIDE);
				break;
			case TileSet::CELL_NEIGHBOR_TOP_RIGHT_CORNER:
				bit = 2;
				base_cell_coords = tile_set->get_neighbor_cell(p_position, TileSet::CELL_NEIGHBOR_TOP_SIDE);
				break;
			default:
				ERR_FAIL();
				break;
		}
	} else if (shape == TileSet::TILE_SHAPE_ISOMETRIC) {
		switch (p_bit) {
			case TileSet::CELL_NEIGHBOR_RIGHT_CORNER:
				bit = 2;
				base_cell_coords = tile_set->get_neighbor_cell(p_position, TileSet::CELL_NEIGHBOR_TOP_RIGHT_SIDE);
				break;
			case TileSet::CELL_NEIGHBOR_BOTTOM_RIGHT_SIDE:
				bit = 1;
				base_cell_coords = p_position;
				break;
			case TileSet::CELL_NEIGHBOR_BOTTOM_CORNER:
				bit = 2;
				base_cell_coords = p_position;
				break;
			case TileSet::CELL_NEIGHBOR_BOTTOM_LEFT_SIDE:
				bit = 3;
				base_cell_coords = p_position;
				break;
			case TileSet::CELL_NEIGHBOR_LEFT_CORNER:
				bit = 2;
				base_cell_coords = tile_set->get_neighbor_cell(p_position, TileSet::CELL_NEIGHBOR_TOP_LEFT_SIDE);
				break;
			case TileSet::CELL_NEIGHBOR_TOP_LEFT_SIDE:
				bit = 1;
				base_cell_coords = tile_set->get_neighbor_cell(p_position, TileSet::CELL_NEIGHBOR_TOP_LEFT_SIDE);
				break;
			case TileSet::CELL_NEIGHBOR_TOP_CORNER:
				bit = 2;
				base_cell_coords = tile_set->get_neighbor_cell(p_position, TileSet::CELL_NEIGHBOR_TOP_CORNER);
				break;
			case TileSet::CELL_NEIGHBOR_TOP_RIGHT_SIDE:
				bit = 3;
				base_cell_coords = tile_set->get_neighbor_cell(p_position, TileSet::CELL_NEIGHBOR_TOP_RIGHT_SIDE);
				break;
			default:
				ERR_FAIL();
				break;
		}
	} else {
		// Half-offset shapes.
		TileSet::TileOffsetAxis offset_axis = tile_set->get_tile_offset_axis();
		if (offset_axis == TileSet::TILE_OFFSET_AXIS_HORIZONTAL) {
			switch (p_bit) {
				case TileSet::CELL_NEIGHBOR_RIGHT_SIDE:
					bit = 1;
					base_cell_coords = p_position;
					break;
				case TileSet::CELL_NEIGHBOR_BOTTOM_RIGHT_CORNER:
					bit = 2;
					base_cell_coords = p_position;
					break;
				case TileSet::CELL_NEIGHBOR_BOTTOM_RIGHT_SIDE:
					bit = 3;
					base_cell_coords = p_position;
					break;
				case TileSet::CELL_NEIGHBOR_BOTTOM_CORNER:
					bit = 4;
					base_cell_coords = p_position;
					break;
				case TileSet::CELL_NEIGHBOR_BOTTOM_LEFT_SIDE:
					bit = 5;
					base_cell_coords = p_position;
					break;
				case TileSet::CELL_NEIGHBOR_BOTTOM_LEFT_CORNER:
					bit = 2;
					base_cell_coords = tile_set->get_neighbor_cell(p_position, TileSet::CELL_NEIGHBOR_LEFT_SIDE);
					break;
				case TileSet::CELL_NEIGHBOR_LEFT_SIDE:
					bit = 1;
					base_cell_coords = tile_set->get_neighbor_cell(p_position, TileSet::CELL_NEIGHBOR_LEFT_SIDE);
					break;
				case TileSet::CELL_NEIGHBOR_TOP_LEFT_CORNER:
					bit = 4;
					base_cell_coords = tile_set->get_neighbor_cell(p_position, TileSet::CELL_NEIGHBOR_TOP_LEFT_SIDE);
					break;
				case TileSet::CELL_NEIGHBOR_TOP_LEFT_SIDE:
					bit = 3;
					base_cell_coords = tile_set->get_neighbor_cell(p_position, TileSet::CELL_NEIGHBOR_TOP_LEFT_SIDE);
					break;
				case TileSet::CELL_NEIGHBOR_TOP_CORNER:
					bit = 2;
					base_cell_coords = tile_set->get_neighbor_cell(p_position, TileSet::CELL_NEIGHBOR_TOP_LEFT_SIDE);
					break;
				case TileSet::CELL_NEIGHBOR_TOP_RIGHT_SIDE:
					bit = 5;
					base_cell_coords = tile_set->get_neighbor_cell(p_position, TileSet::CELL_NEIGHBOR_TOP_RIGHT_SIDE);
					break;
				case TileSet::CELL_NEIGHBOR_TOP_RIGHT_CORNER:
					bit = 4;
					base_cell_coords = tile_set->get_neighbor_cell(p_position, TileSet::CELL_NEIGHBOR_TOP_RIGHT_SIDE);
					break;
				default:
					ERR_FAIL();
					break;
			}
		} else {
			switch (p_bit) {
				case TileSet::CELL_NEIGHBOR_RIGHT_CORNER:
					bit = 1;
					base_cell_coords = p_position;
					break;
				case TileSet::CELL_NEIGHBOR_BOTTOM_RIGHT_SIDE:
					bit = 2;
					base_cell_coords = p_position;
					break;
				case TileSet::CELL_NEIGHBOR_BOTTOM_RIGHT_CORNER:
					bit = 3;
					base_cell_coords = p_position;
					break;
				case TileSet::CELL_NEIGHBOR_BOTTOM_SIDE:
					bit = 4;
					base_cell_coords = p_position;
					break;
				case TileSet::CELL_NEIGHBOR_BOTTOM_LEFT_CORNER:
					bit = 1;
					base_cell_coords = tile_set->get_neighbor_cell(p_position, TileSet::CELL_NEIGHBOR_BOTTOM_LEFT_SIDE);
					break;
				case TileSet::CELL_NEIGHBOR_BOTTOM_LEFT_SIDE:
					bit = 5;
					base_cell_coords = p_position;
					break;
				case TileSet::CELL_NEIGHBOR_LEFT_CORNER:
					bit = 3;
					base_cell_coords = tile_set->get_neighbor_cell(p_position, TileSet::CELL_NEIGHBOR_TOP_LEFT_SIDE);
					break;
				case TileSet::CELL_NEIGHBOR_TOP_LEFT_SIDE:
					bit = 2;
					base_cell_coords = tile_set->get_neighbor_cell(p_position, TileSet::CELL_NEIGHBOR_TOP_LEFT_SIDE);
					break;
				case TileSet::CELL_NEIGHBOR_TOP_LEFT_CORNER:
					bit = 1;
					base_cell_coords = tile_set->get_neighbor_cell(p_position, TileSet::CELL_NEIGHBOR_TOP_LEFT_SIDE);
					break;
				case TileSet::CELL_NEIGHBOR_TOP_SIDE:
					bit = 4;
					base_cell_coords = tile_set->get_neighbor_cell(p_position, TileSet::CELL_NEIGHBOR_TOP_SIDE);
					break;
				case TileSet::CELL_NEIGHBOR_TOP_RIGHT_CORNER:
					bit = 3;
					base_cell_coords = tile_set->get_neighbor_cell(p_position, TileSet::CELL_NEIGHBOR_TOP_SIDE);
					break;
				case TileSet::CELL_NEIGHBOR_TOP_RIGHT_SIDE:
					bit = 5;
					base_cell_coords = tile_set->get_neighbor_cell(p_position, TileSet::CELL_NEIGHBOR_TOP_RIGHT_SIDE);
					break;
				default:
					ERR_FAIL();
					break;
			}
		}
	}
	terrain = p_terrain;
}

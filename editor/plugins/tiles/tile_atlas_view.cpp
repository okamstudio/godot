/**************************************************************************/
/*  tile_atlas_view.cpp                                                   */
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

#include "tile_atlas_view.h"

#include "editor/editor_settings.h"
#include "editor/themes/editor_scale.h"
#include "scene/2d/tile_map.h"
#include "scene/gui/box_container.h"
#include "scene/gui/label.h"
#include "scene/gui/panel.h"
#include "scene/gui/view_panner.h"

void TileAtlasView::gui_input(const Ref<InputEvent> &p_event) {
	if (panner->gui_input(p_event, get_global_rect())) {
		accept_event();
	}
}

void TileAtlasView::_pan_callback(Vector2 p_scroll_vec, Ref<InputEvent> p_event) {
	panning += p_scroll_vec;
	_update_zoom_and_panning(true);
	emit_signal(SNAME("transform_changed"), zoom_widget->get_zoom(), panning);
}

void TileAtlasView::_zoom_callback(float p_zoom_factor, Vector2 p_origin, Ref<InputEvent> p_event) {
	zoom_widget->set_zoom(zoom_widget->get_zoom() * p_zoom_factor);
	_update_zoom_and_panning(true);
	emit_signal(SNAME("transform_changed"), zoom_widget->get_zoom(), panning);
}

Size2i TileAtlasView::_compute_base_tiles_control_size() {
	if (tile_set_atlas_source.is_null()) {
		return Size2i();
	}
	// Update the texture.
	Vector2i size;
	Ref<Texture2D> texture = tile_set_atlas_source->get_texture();
	if (texture.is_valid()) {
		size = texture->get_size();
	}
	return size;
}

Size2i TileAtlasView::_compute_alternative_tiles_control_size() {
	if (tile_set_atlas_source.is_null()) {
		return Size2i();
	}
	Vector2i size;
	for (int i = 0; i < tile_set_atlas_source->get_tiles_count(); i++) {
		Vector2i tile_id = tile_set_atlas_source->get_tile_id(i);
		int alternatives_count = tile_set_atlas_source->get_alternative_tiles_count(tile_id);
		Vector2i line_size;
		Size2i texture_region_size = tile_set_atlas_source->get_tile_texture_region(tile_id).size;
		for (int j = 1; j < alternatives_count; j++) {
			int alternative_id = tile_set_atlas_source->get_alternative_tile_id(tile_id, j);
			bool transposed = tile_set_atlas_source->get_tile_data(tile_id, alternative_id)->get_transpose();
			line_size.x += transposed ? texture_region_size.y : texture_region_size.x;
			line_size.y = MAX(line_size.y, transposed ? texture_region_size.x : texture_region_size.y);
		}
		size.x = MAX(size.x, line_size.x);
		size.y += line_size.y;
	}

	return size;
}

void TileAtlasView::_update_zoom_and_panning(bool p_zoom_on_mouse_pos) {
	if (tile_set_atlas_source.is_null()) {
		return;
	}
	float zoom = zoom_widget->get_zoom();

	// Compute the minimum sizes.
	Size2i base_tiles_control_size = _compute_base_tiles_control_size();
	base_tiles_root_control->set_custom_minimum_size(Vector2(base_tiles_control_size) * zoom);

	Size2i alternative_tiles_control_size = _compute_alternative_tiles_control_size();
	alternative_tiles_root_control->set_custom_minimum_size(Vector2(alternative_tiles_control_size) * zoom);

	// Set the texture for the base tiles.
	Ref<Texture2D> texture = tile_set_atlas_source->get_texture();

	// Set the scales.
	if (base_tiles_control_size.x > 0 && base_tiles_control_size.y > 0) {
		base_tiles_drawing_root->set_scale(Vector2(zoom, zoom));
	} else {
		base_tiles_drawing_root->set_scale(Vector2(1, 1));
	}
	if (alternative_tiles_control_size.x > 0 && alternative_tiles_control_size.y > 0) {
		alternative_tiles_drawing_root->set_scale(Vector2(zoom, zoom));
	} else {
		alternative_tiles_drawing_root->set_scale(Vector2(1, 1));
	}

	// Update the margin container's margins.
	const char *constants[] = { "margin_left", "margin_top", "margin_right", "margin_bottom" };
	for (int i = 0; i < 4; i++) {
		margin_container->add_theme_constant_override(constants[i], margin_container_paddings[i] * zoom);
	}

	// Update the backgrounds.
	background_left->set_size(base_tiles_root_control->get_custom_minimum_size());
	background_right->set_size(alternative_tiles_root_control->get_custom_minimum_size());

	// Zoom on the position.
	if (p_zoom_on_mouse_pos) {
		// Offset the panning relative to the center of panel.
		Vector2 relative_mpos = get_local_mouse_position() - get_size() / 2;
		panning = (panning - relative_mpos) * zoom / previous_zoom + relative_mpos;
	} else {
		// Center of panel.
		panning = panning * zoom / previous_zoom;
	}
	button_center_view->set_disabled(panning.is_zero_approx());

	previous_zoom = zoom;

	center_container->set_begin(panning - center_container->get_minimum_size() / 2);
	center_container->set_size(center_container->get_minimum_size());
}

void TileAtlasView::_zoom_widget_changed() {
	_update_zoom_and_panning();
	emit_signal(SNAME("transform_changed"), zoom_widget->get_zoom(), panning);
}

void TileAtlasView::_center_view() {
	panning = Vector2();
	button_center_view->set_disabled(true);
	_update_zoom_and_panning();
	emit_signal(SNAME("transform_changed"), zoom_widget->get_zoom(), panning);
}

void TileAtlasView::_base_tiles_root_control_gui_input(const Ref<InputEvent> &p_event) {
	if (tile_set_atlas_source.is_null()) {
		return;
	}
	base_tiles_root_control->set_tooltip_text("");

	Ref<InputEventMouseMotion> mm = p_event;
	if (mm.is_valid()) {
		Transform2D xform = base_tiles_drawing_root->get_transform().affine_inverse();
		Vector2i coords = get_atlas_tile_coords_at_pos(xform.xform(mm->get_position()));
		if (coords != TileSetSource::INVALID_ATLAS_COORDS) {
			coords = tile_set_atlas_source->get_tile_at_coords(coords);
			if (coords != TileSetSource::INVALID_ATLAS_COORDS) {
				base_tiles_root_control->set_tooltip_text(vformat(TTR("Source: %d\nAtlas coordinates: %s\nAlternative: 0"), source_id, coords));
			}
		}
	}
}

void TileAtlasView::_draw_base_tiles() {
	if (tile_set.is_null() || tile_set_atlas_source.is_null()) {
		return;
	}
	Ref<Texture2D> texture = tile_set_atlas_source->get_texture();
	if (texture.is_valid()) {
		Vector2i margins = tile_set_atlas_source->get_margins();
		Vector2i separation = tile_set_atlas_source->get_separation();
		Vector2i texture_region_size = tile_set_atlas_source->get_texture_region_size();
		Size2i grid_size = tile_set_atlas_source->get_atlas_grid_size();

		// Draw the texture where there is no tile.
		for (int x = 0; x < grid_size.x; x++) {
			for (int y = 0; y < grid_size.y; y++) {
				Vector2i coords = Vector2i(x, y);
				if (tile_set_atlas_source->get_tile_at_coords(coords) == TileSetSource::INVALID_ATLAS_COORDS) {
					Rect2i rect = Rect2i((texture_region_size + separation) * coords + margins, texture_region_size + separation);
					rect = rect.intersection(Rect2i(Vector2(), texture->get_size()));
					if (rect.size.x > 0 && rect.size.y > 0) {
						base_tiles_draw->draw_texture_rect_region(texture, rect, rect);
					}
				}
			}
		}

		// Draw dark overlay after for performance reasons.
		for (int x = 0; x < grid_size.x; x++) {
			for (int y = 0; y < grid_size.y; y++) {
				Vector2i coords = Vector2i(x, y);
				if (tile_set_atlas_source->get_tile_at_coords(coords) == TileSetSource::INVALID_ATLAS_COORDS) {
					Rect2i rect = Rect2i((texture_region_size + separation) * coords + margins, texture_region_size + separation);
					rect = rect.intersection(Rect2i(Vector2(), texture->get_size()));
					if (rect.size.x > 0 && rect.size.y > 0) {
						base_tiles_draw->draw_rect(rect, Color(0.0, 0.0, 0.0, 0.5));
					}
				}
			}
		}

		// Draw the texture around the grid.
		Rect2i rect;

		// Top.
		rect.position = Vector2i();
		rect.set_end(Vector2i(texture->get_size().x, margins.y));
		base_tiles_draw->draw_texture_rect_region(texture, rect, rect);
		base_tiles_draw->draw_rect(rect, Color(0.0, 0.0, 0.0, 0.5));

		// Bottom
		int bottom_border = margins.y + (grid_size.y * (texture_region_size.y + separation.y));
		if (bottom_border < texture->get_size().y) {
			rect.position = Vector2i(0, bottom_border);
			rect.set_end(texture->get_size());
			base_tiles_draw->draw_texture_rect_region(texture, rect, rect);
			base_tiles_draw->draw_rect(rect, Color(0.0, 0.0, 0.0, 0.5));
		}

		// Left
		rect.position = Vector2i(0, margins.y);
		rect.set_end(Vector2i(margins.x, margins.y + (grid_size.y * (texture_region_size.y + separation.y))));
		base_tiles_draw->draw_texture_rect_region(texture, rect, rect);
		base_tiles_draw->draw_rect(rect, Color(0.0, 0.0, 0.0, 0.5));

		// Right.
		int right_border = margins.x + (grid_size.x * (texture_region_size.x + separation.x));
		if (right_border < texture->get_size().x) {
			rect.position = Vector2i(right_border, margins.y);
			rect.set_end(Vector2i(texture->get_size().x, margins.y + (grid_size.y * (texture_region_size.y + separation.y))));
			base_tiles_draw->draw_texture_rect_region(texture, rect, rect);
			base_tiles_draw->draw_rect(rect, Color(0.0, 0.0, 0.0, 0.5));
		}

		// Draw actual tiles, using their properties (modulation, etc...)
		for (int i = 0; i < tile_set_atlas_source->get_tiles_count(); i++) {
			Vector2i atlas_coords = tile_set_atlas_source->get_tile_id(i);

			// Different materials need to be drawn with different CanvasItems.
			RID ci_rid = _get_canvas_item_to_draw(tile_set_atlas_source->get_tile_data(atlas_coords, 0), base_tiles_draw, material_tiles_draw);

			for (int frame = 0; frame < tile_set_atlas_source->get_tile_animation_frames_count(atlas_coords); frame++) {
				// Update the y to max value.
				Rect2i base_frame_rect = tile_set_atlas_source->get_tile_texture_region(atlas_coords, frame);
				Vector2 offset_pos = Rect2(base_frame_rect).get_center() + Vector2(tile_set_atlas_source->get_tile_data(atlas_coords, 0)->get_texture_origin());

				// Draw the tile.
				TileMapLayer::draw_tile(ci_rid, offset_pos, tile_set, source_id, atlas_coords, 0, frame);
			}
		}

		// Draw Dark overlay on separation in its own pass.
		if (separation.x > 0 || separation.y > 0) {
			for (int i = 0; i < tile_set_atlas_source->get_tiles_count(); i++) {
				Vector2i atlas_coords = tile_set_atlas_source->get_tile_id(i);

				for (int frame = 0; frame < tile_set_atlas_source->get_tile_animation_frames_count(atlas_coords); frame++) {
					// Update the y to max value.
					Rect2i base_frame_rect = tile_set_atlas_source->get_tile_texture_region(atlas_coords, frame);

					if (separation.x > 0) {
						Rect2i right_sep_rect = Rect2i(base_frame_rect.get_position() + Vector2i(base_frame_rect.size.x, 0), Vector2i(separation.x, base_frame_rect.size.y));
						right_sep_rect = right_sep_rect.intersection(Rect2i(Vector2(), texture->get_size()));
						if (right_sep_rect.size.x > 0 && right_sep_rect.size.y > 0) {
							//base_tiles_draw->draw_texture_rect_region(texture, right_sep_rect, right_sep_rect);
							base_tiles_draw->draw_rect(right_sep_rect, Color(0.0, 0.0, 0.0, 0.5));
						}
					}

					if (separation.y > 0) {
						Rect2i bottom_sep_rect = Rect2i(base_frame_rect.get_position() + Vector2i(0, base_frame_rect.size.y), Vector2i(base_frame_rect.size.x + separation.x, separation.y));
						bottom_sep_rect = bottom_sep_rect.intersection(Rect2i(Vector2(), texture->get_size()));
						if (bottom_sep_rect.size.x > 0 && bottom_sep_rect.size.y > 0) {
							//base_tiles_draw->draw_texture_rect_region(texture, bottom_sep_rect, bottom_sep_rect);
							base_tiles_draw->draw_rect(bottom_sep_rect, Color(0.0, 0.0, 0.0, 0.5));
						}
					}
				}
			}
		}
	}
}

RID TileAtlasView::_get_canvas_item_to_draw(const TileData *p_for_data, const CanvasItem *p_base_item, HashMap<Ref<Material>, RID> &p_material_map) {
	Ref<Material> mat = p_for_data->get_material();
	if (mat.is_null()) {
		return p_base_item->get_canvas_item();
	} else if (p_material_map.has(mat)) {
		return p_material_map[mat];
	} else {
		RID ci_rid = RS::get_singleton()->canvas_item_create();
		RS::get_singleton()->canvas_item_set_parent(ci_rid, p_base_item->get_canvas_item());
		RS::get_singleton()->canvas_item_set_material(ci_rid, mat->get_rid());
		RS::get_singleton()->canvas_item_set_default_texture_filter(ci_rid, RS::CanvasItemTextureFilter(p_base_item->get_texture_filter_in_tree()));
		p_material_map[mat] = ci_rid;
		return ci_rid;
	}
}

void TileAtlasView::_clear_material_canvas_items() {
	for (KeyValue<Ref<Material>, RID> kv : material_tiles_draw) {
		RS::get_singleton()->free(kv.value);
	}
	material_tiles_draw.clear();

	for (KeyValue<Ref<Material>, RID> kv : material_alternatives_draw) {
		RS::get_singleton()->free(kv.value);
	}
	material_alternatives_draw.clear();
}

void TileAtlasView::_draw_base_tiles_texture_grid() {
	if (tile_set_atlas_source.is_null()) {
		return;
	}
	Ref<Texture2D> texture = tile_set_atlas_source->get_texture();
	if (texture.is_valid()) {
		Vector2i margins = tile_set_atlas_source->get_margins();
		Vector2i separation = tile_set_atlas_source->get_separation();
		Vector2i texture_region_size = tile_set_atlas_source->get_texture_region_size();

		Size2i grid_size = tile_set_atlas_source->get_atlas_grid_size();

		// Draw each tile texture region.
		for (int x = 0; x < grid_size.x; x++) {
			for (int y = 0; y < grid_size.y; y++) {
				Vector2i origin = margins + (Vector2i(x, y) * (texture_region_size + separation));
				Vector2i base_tile_coords = tile_set_atlas_source->get_tile_at_coords(Vector2i(x, y));
				if (base_tile_coords != TileSetSource::INVALID_ATLAS_COORDS) {
					if (base_tile_coords == Vector2i(x, y)) {
						// Draw existing tile.
						Vector2i size_in_atlas = tile_set_atlas_source->get_tile_size_in_atlas(base_tile_coords);
						Vector2 region_size = texture_region_size * size_in_atlas + separation * (size_in_atlas - Vector2i(1, 1));
						base_tiles_texture_grid->draw_rect(Rect2i(origin, region_size), Color(1.0, 1.0, 1.0, 0.8), false);
					}
				} else {
					// Draw the grid.
					base_tiles_texture_grid->draw_rect(Rect2i(origin, texture_region_size), Color(0.7, 0.7, 0.7, 0.1), false);
				}
			}
		}
	}
}

void TileAtlasView::_draw_base_tiles_shape_grid() {
	if (tile_set.is_null() || tile_set_atlas_source.is_null()) {
		return;
	}
	// Draw the shapes.
	Color grid_color = EDITOR_GET("editors/tiles_editor/grid_color");
	Vector2i tile_shape_size = tile_set->get_tile_size();
	for (int i = 0; i < tile_set_atlas_source->get_tiles_count(); i++) {
		Vector2i tile_id = tile_set_atlas_source->get_tile_id(i);
		Vector2 in_tile_base_offset = tile_set_atlas_source->get_tile_data(tile_id, 0)->get_texture_origin();
		if (tile_set_atlas_source->is_rect_in_tile_texture_region(tile_id, 0, Rect2(Vector2(-tile_shape_size) / 2, tile_shape_size))) {
			for (int frame = 0; frame < tile_set_atlas_source->get_tile_animation_frames_count(tile_id); frame++) {
				Color color = grid_color;
				if (frame > 0) {
					color.a *= 0.3;
				}
				Rect2i texture_region = tile_set_atlas_source->get_tile_texture_region(tile_id, frame);
				Transform2D tile_xform;
				tile_xform.set_origin(Rect2(texture_region).get_center() + in_tile_base_offset);
				tile_xform.set_scale(tile_shape_size);
				tile_set->draw_tile_shape(base_tiles_shape_grid, tile_xform, color);
			}
		}
	}
}

void TileAtlasView::_alternative_tiles_root_control_gui_input(const Ref<InputEvent> &p_event) {
	alternative_tiles_root_control->set_tooltip_text("");

	Ref<InputEventMouseMotion> mm = p_event;
	if (mm.is_valid()) {
		Transform2D xform = alternative_tiles_drawing_root->get_transform().affine_inverse();
		Vector3i coords3 = get_alternative_tile_at_pos(xform.xform(mm->get_position()));
		Vector2i coords = Vector2i(coords3.x, coords3.y);
		int alternative_id = coords3.z;
		if (coords != TileSetSource::INVALID_ATLAS_COORDS && alternative_id != TileSetSource::INVALID_TILE_ALTERNATIVE) {
			alternative_tiles_root_control->set_tooltip_text(vformat(TTR("Source: %d\nAtlas coordinates: %s\nAlternative: %d"), source_id, coords, alternative_id));
		}
	}
}

void TileAtlasView::_draw_alternatives() {
	if (tile_set.is_null() || tile_set_atlas_source.is_null()) {
		return;
	}
	// Draw the alternative tiles.
	Ref<Texture2D> texture = tile_set_atlas_source->get_texture();
	if (texture.is_valid()) {
		Vector2 current_pos;
		for (int i = 0; i < tile_set_atlas_source->get_tiles_count(); i++) {
			Vector2i atlas_coords = tile_set_atlas_source->get_tile_id(i);
			current_pos.x = 0;
			int y_increment = 0;
			Size2i texture_region_size = tile_set_atlas_source->get_tile_texture_region(atlas_coords).size;
			int alternatives_count = tile_set_atlas_source->get_alternative_tiles_count(atlas_coords);
			for (int j = 1; j < alternatives_count; j++) {
				int alternative_id = tile_set_atlas_source->get_alternative_tile_id(atlas_coords, j);
				TileData *tile_data = tile_set_atlas_source->get_tile_data(atlas_coords, alternative_id);
				bool transposed = tile_data->get_transpose();

				// Different materials need to be drawn with different CanvasItems.
				RID ci_rid = _get_canvas_item_to_draw(tile_data, alternatives_draw, material_alternatives_draw);

				// Update the y to max value.
				Vector2i offset_pos;
				if (transposed) {
					offset_pos = (current_pos + Vector2(texture_region_size.y, texture_region_size.x) / 2 + tile_data->get_texture_origin());
					y_increment = MAX(y_increment, texture_region_size.x);
				} else {
					offset_pos = (current_pos + texture_region_size / 2 + tile_data->get_texture_origin());
					y_increment = MAX(y_increment, texture_region_size.y);
				}

				// Draw the tile.
				TileMapLayer::draw_tile(ci_rid, offset_pos, tile_set, source_id, atlas_coords, alternative_id);

				// Increment the x position.
				current_pos.x += transposed ? texture_region_size.y : texture_region_size.x;
			}
			if (alternatives_count > 1) {
				current_pos.y += y_increment;
			}
		}
	}
}

void TileAtlasView::_draw_background_left() {
	background_left->draw_texture_rect(theme_cache.checkerboard, Rect2(Vector2(), background_left->get_size()), true);
}

void TileAtlasView::_draw_background_right() {
	background_right->draw_texture_rect(theme_cache.checkerboard, Rect2(Vector2(), background_right->get_size()), true);
}

void TileAtlasView::set_atlas_source(TileSet *p_tile_set, TileSetAtlasSource *p_tile_set_atlas_source, int p_source_id) {
	tile_set = Ref<TileSet>(p_tile_set);
	tile_set_atlas_source = Ref<TileSetAtlasSource>(p_tile_set_atlas_source);

	_clear_material_canvas_items();

	if (tile_set.is_null()) {
		return;
	}

	ERR_FAIL_COND(p_source_id < 0);
	ERR_FAIL_COND(p_tile_set->get_source(p_source_id) != p_tile_set_atlas_source);

	source_id = p_source_id;

	// Show or hide the view.
	bool valid = tile_set_atlas_source->get_texture().is_valid();
	hbox->set_visible(valid);
	missing_source_label->set_visible(!valid);

	// Update the rect cache.
	_update_alternative_tiles_rect_cache();

	// Update everything.
	_update_zoom_and_panning();

	base_tiles_drawing_root->set_size(_compute_base_tiles_control_size());
	alternative_tiles_drawing_root->set_size(_compute_alternative_tiles_control_size());

	// Update.
	base_tiles_draw->queue_redraw();
	base_tiles_texture_grid->queue_redraw();
	base_tiles_shape_grid->queue_redraw();
	alternatives_draw->queue_redraw();
	background_left->queue_redraw();
	background_right->queue_redraw();
}

float TileAtlasView::get_zoom() const {
	return zoom_widget->get_zoom();
}

void TileAtlasView::set_transform(float p_zoom, Vector2i p_panning) {
	zoom_widget->set_zoom(p_zoom);
	panning = p_panning;
	_update_zoom_and_panning();
}

void TileAtlasView::set_padding(Side p_side, int p_padding) {
	ERR_FAIL_COND(p_padding < 0);
	margin_container_paddings[p_side] = p_padding;
}

Vector2i TileAtlasView::get_atlas_tile_coords_at_pos(const Vector2 p_pos, bool p_clamp) const {
	if (tile_set_atlas_source.is_null()) {
		return Vector2i();
	}

	Ref<Texture2D> texture = tile_set_atlas_source->get_texture();
	if (texture.is_null()) {
		return TileSetSource::INVALID_ATLAS_COORDS;
	}

	Vector2i margins = tile_set_atlas_source->get_margins();
	Vector2i separation = tile_set_atlas_source->get_separation();
	Vector2i texture_region_size = tile_set_atlas_source->get_texture_region_size();

	// Compute index in atlas.
	Vector2 pos = p_pos - margins;
	Vector2i ret = (pos / (texture_region_size + separation)).floor();

	// Clamp.
	if (p_clamp) {
		Vector2i size = tile_set_atlas_source->get_atlas_grid_size();
		ret = ret.clamp(Vector2i(), size - Vector2i(1, 1));
	}

	return ret;
}

void TileAtlasView::_update_alternative_tiles_rect_cache() {
	if (tile_set_atlas_source.is_null()) {
		return;
	}

	alternative_tiles_rect_cache.clear();

	Rect2i current;
	for (int i = 0; i < tile_set_atlas_source->get_tiles_count(); i++) {
		Vector2i tile_id = tile_set_atlas_source->get_tile_id(i);
		int alternatives_count = tile_set_atlas_source->get_alternative_tiles_count(tile_id);
		Size2i texture_region_size = tile_set_atlas_source->get_tile_texture_region(tile_id).size;
		int line_height = 0;
		for (int j = 1; j < alternatives_count; j++) {
			int alternative_id = tile_set_atlas_source->get_alternative_tile_id(tile_id, j);
			TileData *tile_data = tile_set_atlas_source->get_tile_data(tile_id, alternative_id);
			bool transposed = tile_data->get_transpose();
			current.size = transposed ? Vector2i(texture_region_size.y, texture_region_size.x) : texture_region_size;

			// Update the rect.
			if (!alternative_tiles_rect_cache.has(tile_id)) {
				alternative_tiles_rect_cache[tile_id] = HashMap<int, Rect2i>();
			}
			alternative_tiles_rect_cache[tile_id][alternative_id] = current;

			current.position.x += transposed ? texture_region_size.y : texture_region_size.x;
			line_height = MAX(line_height, transposed ? texture_region_size.x : texture_region_size.y);
		}

		current.position.x = 0;
		current.position.y += line_height;
	}
}

Vector3i TileAtlasView::get_alternative_tile_at_pos(const Vector2 p_pos) const {
	for (const KeyValue<Vector2, HashMap<int, Rect2i>> &E_coords : alternative_tiles_rect_cache) {
		for (const KeyValue<int, Rect2i> &E_alternative : E_coords.value) {
			if (E_alternative.value.has_point(p_pos)) {
				return Vector3i(E_coords.key.x, E_coords.key.y, E_alternative.key);
			}
		}
	}

	return Vector3i(TileSetSource::INVALID_ATLAS_COORDS.x, TileSetSource::INVALID_ATLAS_COORDS.y, TileSetSource::INVALID_TILE_ALTERNATIVE);
}

Rect2i TileAtlasView::get_alternative_tile_rect(const Vector2i p_coords, int p_alternative_tile) {
	ERR_FAIL_COND_V_MSG(!alternative_tiles_rect_cache.has(p_coords), Rect2i(), vformat("No cached rect for tile coords:%s", p_coords));
	ERR_FAIL_COND_V_MSG(!alternative_tiles_rect_cache[p_coords].has(p_alternative_tile), Rect2i(), vformat("No cached rect for tile coords:%s alternative_id:%d", p_coords, p_alternative_tile));

	return alternative_tiles_rect_cache[p_coords][p_alternative_tile];
}

void TileAtlasView::queue_redraw() {
	base_tiles_draw->queue_redraw();
	base_tiles_texture_grid->queue_redraw();
	base_tiles_shape_grid->queue_redraw();
	alternatives_draw->queue_redraw();
	background_left->queue_redraw();
	background_right->queue_redraw();
}

void TileAtlasView::_update_theme_item_cache() {
	Control::_update_theme_item_cache();

	theme_cache.center_view_icon = get_editor_theme_icon(SNAME("CenterView"));
	theme_cache.checkerboard = get_editor_theme_icon(SNAME("Checkerboard"));
}

void TileAtlasView::_notification(int p_what) {
	switch (p_what) {
		case EditorSettings::NOTIFICATION_EDITOR_SETTINGS_CHANGED: {
			if (!EditorSettings::get_singleton()->check_changed_settings_in_group("editors/panning")) {
				break;
			}
			[[fallthrough]];
		}
		case NOTIFICATION_ENTER_TREE: {
			panner->setup((ViewPanner::ControlScheme)EDITOR_GET("editors/panning/sub_editors_panning_scheme").operator int(), ED_GET_SHORTCUT("canvas_item_editor/pan_view"), bool(EDITOR_GET("editors/panning/simple_panning")));
			panner->setup_warped_panning(get_viewport(), EDITOR_GET("editors/panning/warped_mouse_panning"));
		} break;

		case NOTIFICATION_THEME_CHANGED: {
			button_center_view->set_button_icon(theme_cache.center_view_icon);
		} break;
	}
}

void TileAtlasView::_bind_methods() {
	ADD_SIGNAL(MethodInfo("transform_changed", PropertyInfo(Variant::FLOAT, "zoom"), PropertyInfo(Variant::VECTOR2, "scroll")));
}

TileAtlasView::TileAtlasView() {
	set_texture_filter(CanvasItem::TEXTURE_FILTER_NEAREST);

	Panel *panel = memnew(Panel);
	panel->set_clip_contents(true);
	panel->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
	panel->set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT);
	panel->set_h_size_flags(SIZE_EXPAND_FILL);
	panel->set_v_size_flags(SIZE_EXPAND_FILL);
	add_child(panel);

	zoom_widget = memnew(EditorZoomWidget);
	add_child(zoom_widget);
	zoom_widget->set_anchors_and_offsets_preset(Control::PRESET_TOP_LEFT, Control::PRESET_MODE_MINSIZE, 2 * EDSCALE);
	zoom_widget->connect("zoom_changed", callable_mp(this, &TileAtlasView::_zoom_widget_changed).unbind(1));
	zoom_widget->set_shortcut_context(this);

	button_center_view = memnew(Button);
	button_center_view->set_anchors_and_offsets_preset(Control::PRESET_TOP_RIGHT, Control::PRESET_MODE_MINSIZE, 5);
	button_center_view->set_grow_direction_preset(Control::PRESET_TOP_RIGHT);
	button_center_view->connect(SceneStringName(pressed), callable_mp(this, &TileAtlasView::_center_view));
	button_center_view->set_flat(true);
	button_center_view->set_disabled(true);
	button_center_view->set_tooltip_text(TTR("Center View"));
	add_child(button_center_view);

	panner.instantiate();
	panner->set_callbacks(callable_mp(this, &TileAtlasView::_pan_callback), callable_mp(this, &TileAtlasView::_zoom_callback));
	panner->set_enable_rmb(true);

	center_container = memnew(CenterContainer);
	center_container->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
	center_container->set_anchors_preset(Control::PRESET_CENTER);
	center_container->connect(SceneStringName(gui_input), callable_mp(this, &TileAtlasView::gui_input));
	center_container->connect(SceneStringName(focus_exited), callable_mp(panner.ptr(), &ViewPanner::release_pan_key));
	center_container->set_focus_mode(FOCUS_CLICK);
	panel->add_child(center_container);

	missing_source_label = memnew(Label);
	missing_source_label->set_text(TTR("The selected atlas source has no valid texture. Assign a texture in the TileSet bottom tab."));
	center_container->add_child(missing_source_label);

	margin_container = memnew(MarginContainer);
	margin_container->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
	center_container->add_child(margin_container);

	hbox = memnew(HBoxContainer);
	hbox->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
	hbox->add_theme_constant_override("separation", 10);
	hbox->hide();
	margin_container->add_child(hbox);

	VBoxContainer *left_vbox = memnew(VBoxContainer);
	left_vbox->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
	hbox->add_child(left_vbox);

	VBoxContainer *right_vbox = memnew(VBoxContainer);
	right_vbox->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
	hbox->add_child(right_vbox);

	// Base tiles.
	Label *base_tile_label = memnew(Label);
	base_tile_label->set_mouse_filter(Control::MOUSE_FILTER_PASS);
	base_tile_label->set_text(TTR("Base Tiles"));
	base_tile_label->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
	left_vbox->add_child(base_tile_label);

	base_tiles_root_control = memnew(Control);
	base_tiles_root_control->set_mouse_filter(Control::MOUSE_FILTER_PASS);
	base_tiles_root_control->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	base_tiles_root_control->connect(SceneStringName(gui_input), callable_mp(this, &TileAtlasView::_base_tiles_root_control_gui_input));
	left_vbox->add_child(base_tiles_root_control);

	background_left = memnew(Control);
	background_left->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
	background_left->set_anchors_and_offsets_preset(Control::PRESET_TOP_LEFT);
	background_left->set_texture_repeat(TextureRepeat::TEXTURE_REPEAT_ENABLED);
	background_left->connect(SceneStringName(draw), callable_mp(this, &TileAtlasView::_draw_background_left));
	base_tiles_root_control->add_child(background_left);

	base_tiles_drawing_root = memnew(Control);
	base_tiles_drawing_root->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
	base_tiles_drawing_root->set_texture_filter(TEXTURE_FILTER_NEAREST);
	base_tiles_root_control->add_child(base_tiles_drawing_root);

	base_tiles_draw = memnew(Control);
	base_tiles_draw->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
	base_tiles_draw->set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT);
	base_tiles_draw->connect(SceneStringName(draw), callable_mp(this, &TileAtlasView::_draw_base_tiles));
	base_tiles_drawing_root->add_child(base_tiles_draw);

	base_tiles_texture_grid = memnew(Control);
	base_tiles_texture_grid->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
	base_tiles_texture_grid->set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT);
	base_tiles_texture_grid->connect(SceneStringName(draw), callable_mp(this, &TileAtlasView::_draw_base_tiles_texture_grid));
	base_tiles_drawing_root->add_child(base_tiles_texture_grid);

	base_tiles_shape_grid = memnew(Control);
	base_tiles_shape_grid->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
	base_tiles_shape_grid->set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT);
	base_tiles_shape_grid->connect(SceneStringName(draw), callable_mp(this, &TileAtlasView::_draw_base_tiles_shape_grid));
	base_tiles_drawing_root->add_child(base_tiles_shape_grid);

	// Alternative tiles.
	Label *alternative_tiles_label = memnew(Label);
	alternative_tiles_label->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
	alternative_tiles_label->set_text(TTR("Alternative Tiles"));
	alternative_tiles_label->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
	right_vbox->add_child(alternative_tiles_label);

	alternative_tiles_root_control = memnew(Control);
	alternative_tiles_root_control->set_mouse_filter(Control::MOUSE_FILTER_PASS);
	alternative_tiles_root_control->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	alternative_tiles_root_control->connect(SceneStringName(gui_input), callable_mp(this, &TileAtlasView::_alternative_tiles_root_control_gui_input));
	right_vbox->add_child(alternative_tiles_root_control);

	background_right = memnew(Control);
	background_right->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
	background_right->set_anchors_and_offsets_preset(Control::PRESET_TOP_LEFT);
	background_right->set_texture_repeat(TextureRepeat::TEXTURE_REPEAT_ENABLED);
	background_right->connect(SceneStringName(draw), callable_mp(this, &TileAtlasView::_draw_background_right));
	alternative_tiles_root_control->add_child(background_right);

	alternative_tiles_drawing_root = memnew(Control);
	alternative_tiles_drawing_root->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
	alternative_tiles_drawing_root->set_texture_filter(TEXTURE_FILTER_NEAREST);
	alternative_tiles_root_control->add_child(alternative_tiles_drawing_root);

	alternatives_draw = memnew(Control);
	alternatives_draw->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
	alternatives_draw->set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT);
	alternatives_draw->connect(SceneStringName(draw), callable_mp(this, &TileAtlasView::_draw_alternatives));
	alternative_tiles_drawing_root->add_child(alternatives_draw);
}

TileAtlasView::~TileAtlasView() {
	_clear_material_canvas_items();
}

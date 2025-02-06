/**************************************************************************/
/*  tile_atlas_view.h                                                     */
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

#pragma once

#include "editor/gui/editor_zoom_widget.h"
#include "scene/gui/box_container.h"
#include "scene/gui/button.h"
#include "scene/gui/center_container.h"
#include "scene/gui/label.h"
#include "scene/gui/margin_container.h"
#include "scene/resources/2d/tile_set.h"

class ViewPanner;

class TileAtlasView : public Control {
	GDCLASS(TileAtlasView, Control);

private:
	Ref<TileSet> tile_set;
	Ref<TileSetAtlasSource> tile_set_atlas_source;
	int source_id = TileSet::INVALID_SOURCE;

	enum DragType {
		DRAG_TYPE_NONE,
		DRAG_TYPE_PAN,
	};
	DragType drag_type = DRAG_TYPE_NONE;
	float previous_zoom = 1.0;
	EditorZoomWidget *zoom_widget = nullptr;
	Button *button_center_view = nullptr;
	CenterContainer *center_container = nullptr;
	Vector2 panning;
	void _update_zoom_and_panning(bool p_zoom_on_mouse_pos = false);
	void _zoom_widget_changed();
	void _center_view();
	virtual void gui_input(const Ref<InputEvent> &p_event) override;

	Ref<ViewPanner> panner;
	void _pan_callback(Vector2 p_scroll_vec, Ref<InputEvent> p_event);
	void _zoom_callback(float p_zoom_factor, Vector2 p_origin, Ref<InputEvent> p_event);

	HashMap<Vector2, HashMap<int, Rect2i>> alternative_tiles_rect_cache;
	void _update_alternative_tiles_rect_cache();

	MarginContainer *margin_container = nullptr;
	int margin_container_paddings[4] = { 0, 0, 0, 0 };
	HBoxContainer *hbox = nullptr;
	Label *missing_source_label = nullptr;

	// Background
	Control *background_left = nullptr;
	void _draw_background_left();
	Control *background_right = nullptr;
	void _draw_background_right();

	// Left side.
	Control *base_tiles_root_control = nullptr;
	void _base_tiles_root_control_gui_input(const Ref<InputEvent> &p_event);

	Control *base_tiles_drawing_root = nullptr;

	Control *base_tiles_draw = nullptr;
	HashMap<Ref<Material>, RID> material_tiles_draw;
	HashMap<Ref<Material>, RID> material_alternatives_draw;
	void _draw_base_tiles();
	RID _get_canvas_item_to_draw(const TileData *p_for_data, const CanvasItem *p_base_item, HashMap<Ref<Material>, RID> &p_material_map);
	void _clear_material_canvas_items();

	Control *base_tiles_texture_grid = nullptr;
	void _draw_base_tiles_texture_grid();

	Control *base_tiles_shape_grid = nullptr;
	void _draw_base_tiles_shape_grid();

	Size2i _compute_base_tiles_control_size();

	// Right side.
	Control *alternative_tiles_root_control = nullptr;
	void _alternative_tiles_root_control_gui_input(const Ref<InputEvent> &p_event);

	Control *alternative_tiles_drawing_root = nullptr;

	Control *alternatives_draw = nullptr;
	void _draw_alternatives();

	Size2i _compute_alternative_tiles_control_size();

	struct ThemeCache {
		Ref<Texture2D> center_view_icon;
		Ref<Texture2D> checkerboard;
	} theme_cache;

protected:
	virtual void _update_theme_item_cache() override;

	void _notification(int p_what);
	static void _bind_methods();

public:
	// Global.
	void set_atlas_source(TileSet *p_tile_set, TileSetAtlasSource *p_tile_set_atlas_source, int p_source_id);

	float get_zoom() const;
	void set_transform(float p_zoom, Vector2i p_panning);

	void set_padding(Side p_side, int p_padding);

	// Left side.
	void set_texture_grid_visible(bool p_visible) { base_tiles_texture_grid->set_visible(p_visible); }
	void set_tile_shape_grid_visible(bool p_visible) { base_tiles_shape_grid->set_visible(p_visible); }

	Vector2i get_atlas_tile_coords_at_pos(const Vector2 p_pos, bool p_clamp = false) const;

	void add_control_over_atlas_tiles(Control *p_control, bool scaled = true) {
		if (scaled) {
			base_tiles_drawing_root->add_child(p_control);
		} else {
			base_tiles_root_control->add_child(p_control);
		}
		p_control->set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT);
		p_control->set_mouse_filter(Control::MOUSE_FILTER_PASS);
	}

	// Right side.
	Vector3i get_alternative_tile_at_pos(const Vector2 p_pos) const;
	Rect2i get_alternative_tile_rect(const Vector2i p_coords, int p_alternative_tile);

	void add_control_over_alternative_tiles(Control *p_control, bool scaled = true) {
		if (scaled) {
			alternative_tiles_drawing_root->add_child(p_control);
		} else {
			alternative_tiles_root_control->add_child(p_control);
		}
		p_control->set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT);
		p_control->set_mouse_filter(Control::MOUSE_FILTER_PASS);
	}

	// Redraw everything.
	void queue_redraw();

	TileAtlasView();
	~TileAtlasView();
};

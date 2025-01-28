/**************************************************************************/
/*  tile_data_editors.h                                                   */
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

#ifndef TILE_DATA_EDITORS_H
#define TILE_DATA_EDITORS_H

#include "tile_atlas_view.h"

#include "editor/editor_properties.h"
#include "scene/2d/tile_map.h"
#include "scene/gui/box_container.h"

class Label;
class MenuButton;
class SpinBox;
class EditorUndoRedoManager;

class TileDataEditor : public VBoxContainer {
	GDCLASS(TileDataEditor, VBoxContainer);

private:
	bool _tile_set_changed_update_needed = false;
	void _tile_set_changed_plan_update();
	void _tile_set_changed_deferred_update();

protected:
	Ref<TileSet> tile_set;
	TileData *_get_tile_data(TileMapCell p_cell);
	virtual void _tile_set_changed() {}

	static void _bind_methods();

public:
	void set_tile_set(Ref<TileSet> p_tile_set);

	// Input to handle painting.
	virtual Control *get_toolbar() { return nullptr; }
	virtual void forward_draw_over_atlas(TileAtlasView *p_tile_atlas_view, TileSetAtlasSource *p_tile_atlas_source, CanvasItem *p_canvas_item, Transform2D p_transform) {}
	virtual void forward_draw_over_alternatives(TileAtlasView *p_tile_atlas_view, TileSetAtlasSource *p_tile_atlas_source, CanvasItem *p_canvas_item, Transform2D p_transform) {}
	virtual void forward_painting_atlas_gui_input(TileAtlasView *p_tile_atlas_view, TileSetAtlasSource *p_tile_atlas_source, const Ref<InputEvent> &p_event) {}
	virtual void forward_painting_alternatives_gui_input(TileAtlasView *p_tile_atlas_view, TileSetAtlasSource *p_tile_atlas_source, const Ref<InputEvent> &p_event) {}

	// Used to draw the tile data property value over a tile.
	virtual void draw_over_tile(CanvasItem *p_canvas_item, Transform2D p_transform, TileMapCell p_cell, bool p_selected = false) {}
};

class DummyObject : public Object {
	GDCLASS(DummyObject, Object)
private:
	HashMap<String, Variant> properties;

protected:
	bool _set(const StringName &p_name, const Variant &p_value);
	bool _get(const StringName &p_name, Variant &r_ret) const;

public:
	bool has_dummy_property(const StringName &p_name);
	void add_dummy_property(const StringName &p_name);
	void remove_dummy_property(const StringName &p_name);
	void clear_dummy_properties();
};

class GenericTilePolygonEditor : public VBoxContainer {
	GDCLASS(GenericTilePolygonEditor, VBoxContainer);

private:
	Ref<TileSet> tile_set;
	LocalVector<Vector<Point2>> polygons;
	bool multiple_polygon_mode = false;

	bool use_undo_redo = true;

	// UI
	int hovered_polygon_index = -1;
	int hovered_point_index = -1;
	int hovered_segment_index = -1;
	Vector2 hovered_segment_point;

	enum DragType {
		DRAG_TYPE_NONE,
		DRAG_TYPE_DRAG_POINT,
		DRAG_TYPE_CREATE_POINT,
		DRAG_TYPE_PAN,
	};
	DragType drag_type = DRAG_TYPE_NONE;
	int drag_polygon_index = 0;
	int drag_point_index = 0;
	Vector2 drag_last_pos;
	PackedVector2Array drag_old_polygon;

	HBoxContainer *toolbar = nullptr;
	Ref<ButtonGroup> tools_button_group;
	Button *button_expand = nullptr;
	Button *button_create = nullptr;
	Button *button_edit = nullptr;
	Button *button_delete = nullptr;
	MenuButton *button_advanced_menu = nullptr;

	enum Snap {
		SNAP_NONE,
		SNAP_HALF_PIXEL,
		SNAP_GRID,
	};
	int current_snap_option = SNAP_HALF_PIXEL;
	MenuButton *button_pixel_snap = nullptr;
	SpinBox *snap_subdivision = nullptr;

	Vector<Point2> in_creation_polygon;

	Panel *panel = nullptr;
	Control *base_control = nullptr;
	EditorZoomWidget *editor_zoom_widget = nullptr;
	Button *button_center_view = nullptr;
	Vector2 panning;
	bool initializing = true;

	Ref<TileSetAtlasSource> background_atlas_source;
	Vector2i background_atlas_coords;
	int background_alternative_id;

	Color polygon_color = Color(1.0, 0.0, 0.0);

	enum AdvancedMenuOption {
		RESET_TO_DEFAULT_TILE,
		CLEAR_TILE,
		ROTATE_RIGHT,
		ROTATE_LEFT,
		FLIP_HORIZONTALLY,
		FLIP_VERTICALLY,
	};

	void _base_control_draw();
	void _zoom_changed();
	void _advanced_menu_item_pressed(int p_item_pressed);
	void _center_view();
	void _base_control_gui_input(Ref<InputEvent> p_event);
	void _set_snap_option(int p_index);
	void _store_snap_options();
	void _toggle_expand(bool p_expand);

	void _snap_to_tile_shape(Point2 &r_point, float &r_current_snapped_dist, float p_snap_dist);
	void _snap_point(Point2 &r_point);
	void _grab_polygon_point(Vector2 p_pos, const Transform2D &p_polygon_xform, int &r_polygon_index, int &r_point_index);
	void _grab_polygon_segment_point(Vector2 p_pos, const Transform2D &p_polygon_xform, int &r_polygon_index, int &r_segment_index, Vector2 &r_point);

protected:
	void _notification(int p_what);
	static void _bind_methods();

public:
	void set_use_undo_redo(bool p_use_undo_redo);

	void set_tile_set(Ref<TileSet> p_tile_set);
	void set_background_tile(const TileSetAtlasSource *p_atlas_source, const Vector2 &p_atlas_coords, int p_alternative_id);

	int get_polygon_count();
	int add_polygon(const Vector<Point2> &p_polygon, int p_index = -1);
	void remove_polygon(int p_index);
	void clear_polygons();
	void set_polygon(int p_polygon_index, const Vector<Point2> &p_polygon);
	Vector<Point2> get_polygon(int p_polygon_index);

	void set_polygons_color(Color p_color);
	void set_multiple_polygon_mode(bool p_multiple_polygon_mode);

	GenericTilePolygonEditor();
};

class TileDataDefaultEditor : public TileDataEditor {
	GDCLASS(TileDataDefaultEditor, TileDataEditor);

private:
	// Toolbar
	HBoxContainer *toolbar = memnew(HBoxContainer);
	Button *picker_button = nullptr;

	// UI
	Ref<Texture2D> tile_bool_checked;
	Ref<Texture2D> tile_bool_unchecked;
	Label *label = nullptr;

	EditorProperty *property_editor = nullptr;

	// Painting state.
	enum DragType {
		DRAG_TYPE_NONE = 0,
		DRAG_TYPE_PAINT,
		DRAG_TYPE_PAINT_RECT,
	};
	DragType drag_type = DRAG_TYPE_NONE;
	Vector2 drag_start_pos;
	Vector2 drag_last_pos;
	HashMap<TileMapCell, Variant, TileMapCell> drag_modified;
	Variant drag_painted_value;

	void _property_value_changed(const StringName &p_property, const Variant &p_value, const StringName &p_field);

protected:
	DummyObject *dummy_object = memnew(DummyObject);

	StringName type;
	String property;
	Variant::Type property_type;
	void _notification(int p_what);

	virtual Variant _get_painted_value();
	virtual void _set_painted_value(TileSetAtlasSource *p_tile_set_atlas_source, Vector2 p_coords, int p_alternative_tile);
	virtual void _set_value(TileSetAtlasSource *p_tile_set_atlas_source, Vector2 p_coords, int p_alternative_tile, const Variant &p_value);
	virtual Variant _get_value(TileSetAtlasSource *p_tile_set_atlas_source, Vector2 p_coords, int p_alternative_tile);
	virtual void _setup_undo_redo_action(TileSetAtlasSource *p_tile_set_atlas_source, const HashMap<TileMapCell, Variant, TileMapCell> &p_previous_values, const Variant &p_new_value);

public:
	virtual Control *get_toolbar() override { return toolbar; }
	virtual void forward_draw_over_atlas(TileAtlasView *p_tile_atlas_view, TileSetAtlasSource *p_tile_atlas_source, CanvasItem *p_canvas_item, Transform2D p_transform) override;
	virtual void forward_draw_over_alternatives(TileAtlasView *p_tile_atlas_view, TileSetAtlasSource *p_tile_atlas_source, CanvasItem *p_canvas_item, Transform2D p_transform) override;
	virtual void forward_painting_atlas_gui_input(TileAtlasView *p_tile_atlas_view, TileSetAtlasSource *p_tile_atlas_source, const Ref<InputEvent> &p_event) override;
	virtual void forward_painting_alternatives_gui_input(TileAtlasView *p_tile_atlas_view, TileSetAtlasSource *p_tile_atlas_source, const Ref<InputEvent> &p_event) override;
	virtual void draw_over_tile(CanvasItem *p_canvas_item, Transform2D p_transform, TileMapCell p_cell, bool p_selected = false) override;

	void setup_property_editor(Variant::Type p_type, const String &p_property, const String &p_label = "", const Variant &p_default_value = Variant());
	Variant::Type get_property_type();

	TileDataDefaultEditor();
	~TileDataDefaultEditor();
};

class TileDataTextureOriginEditor : public TileDataDefaultEditor {
	GDCLASS(TileDataTextureOriginEditor, TileDataDefaultEditor);

public:
	virtual void draw_over_tile(CanvasItem *p_canvas_item, Transform2D p_transform, TileMapCell p_cell, bool p_selected = false) override;
};

class TileDataPositionEditor : public TileDataDefaultEditor {
	GDCLASS(TileDataPositionEditor, TileDataDefaultEditor);

public:
	virtual void draw_over_tile(CanvasItem *p_canvas_item, Transform2D p_transform, TileMapCell p_cell, bool p_selected = false) override;
};

class TileDataYSortEditor : public TileDataDefaultEditor {
	GDCLASS(TileDataYSortEditor, TileDataDefaultEditor);

public:
	virtual void draw_over_tile(CanvasItem *p_canvas_item, Transform2D p_transform, TileMapCell p_cell, bool p_selected = false) override;
};

class TileDataOcclusionShapeEditor : public TileDataDefaultEditor {
	GDCLASS(TileDataOcclusionShapeEditor, TileDataDefaultEditor);

private:
	int occlusion_layer = -1;

	// UI
	GenericTilePolygonEditor *polygon_editor = nullptr;

	void _polygon_changed(const PackedVector2Array &p_polygon);

	virtual Variant _get_painted_value() override;
	virtual void _set_painted_value(TileSetAtlasSource *p_tile_set_atlas_source, Vector2 p_coords, int p_alternative_tile) override;
	virtual void _set_value(TileSetAtlasSource *p_tile_set_atlas_source, Vector2 p_coords, int p_alternative_tile, const Variant &p_value) override;
	virtual Variant _get_value(TileSetAtlasSource *p_tile_set_atlas_source, Vector2 p_coords, int p_alternative_tile) override;
	virtual void _setup_undo_redo_action(TileSetAtlasSource *p_tile_set_atlas_source, const HashMap<TileMapCell, Variant, TileMapCell> &p_previous_values, const Variant &p_new_value) override;

protected:
	virtual void _tile_set_changed() override;

	void _notification(int p_what);

public:
	virtual void draw_over_tile(CanvasItem *p_canvas_item, Transform2D p_transform, TileMapCell p_cell, bool p_selected = false) override;

	void set_occlusion_layer(int p_occlusion_layer) { occlusion_layer = p_occlusion_layer; }

	TileDataOcclusionShapeEditor();
};

class TileDataCollisionEditor : public TileDataDefaultEditor {
	GDCLASS(TileDataCollisionEditor, TileDataDefaultEditor);

	int physics_layer = -1;

	// UI
	GenericTilePolygonEditor *polygon_editor = nullptr;
	DummyObject *dummy_object = memnew(DummyObject);
	HashMap<StringName, EditorProperty *> property_editors;

	void _property_value_changed(const StringName &p_property, const Variant &p_value, const StringName &p_field);
	void _property_selected(const StringName &p_path, int p_focusable);
	void _polygons_changed();

	virtual Variant _get_painted_value() override;
	virtual void _set_painted_value(TileSetAtlasSource *p_tile_set_atlas_source, Vector2 p_coords, int p_alternative_tile) override;
	virtual void _set_value(TileSetAtlasSource *p_tile_set_atlas_source, Vector2 p_coords, int p_alternative_tile, const Variant &p_value) override;
	virtual Variant _get_value(TileSetAtlasSource *p_tile_set_atlas_source, Vector2 p_coords, int p_alternative_tile) override;
	virtual void _setup_undo_redo_action(TileSetAtlasSource *p_tile_set_atlas_source, const HashMap<TileMapCell, Variant, TileMapCell> &p_previous_values, const Variant &p_new_value) override;

protected:
	virtual void _tile_set_changed() override;

	void _notification(int p_what);

public:
	virtual void draw_over_tile(CanvasItem *p_canvas_item, Transform2D p_transform, TileMapCell p_cell, bool p_selected = false) override;

	void set_physics_layer(int p_physics_layer) { physics_layer = p_physics_layer; }

	TileDataCollisionEditor();
	~TileDataCollisionEditor();
};

class TileDataTerrainsEditor : public TileDataEditor {
	GDCLASS(TileDataTerrainsEditor, TileDataEditor);

private:
	// Toolbar
	HBoxContainer *toolbar = memnew(HBoxContainer);
	Button *picker_button = nullptr;

	// Painting state.
	enum DragType {
		DRAG_TYPE_NONE = 0,
		DRAG_TYPE_PAINT_TERRAIN_SET,
		DRAG_TYPE_PAINT_TERRAIN_SET_RECT,
		DRAG_TYPE_PAINT_TERRAIN_BITS,
		DRAG_TYPE_PAINT_TERRAIN_BITS_RECT,
	};
	DragType drag_type = DRAG_TYPE_NONE;
	Vector2 drag_start_pos;
	Vector2 drag_last_pos;
	HashMap<TileMapCell, Variant, TileMapCell> drag_modified;
	Variant drag_painted_value;

	// UI
	Label *label = nullptr;
	DummyObject *dummy_object = memnew(DummyObject);
	EditorPropertyEnum *terrain_set_property_editor = nullptr;
	EditorPropertyEnum *terrain_property_editor = nullptr;

	void _property_value_changed(const StringName &p_property, const Variant &p_value, const StringName &p_field);

	void _update_terrain_selector();

protected:
	virtual void _tile_set_changed() override;

	void _notification(int p_what);

public:
	virtual Control *get_toolbar() override { return toolbar; }
	virtual void forward_draw_over_atlas(TileAtlasView *p_tile_atlas_view, TileSetAtlasSource *p_tile_atlas_source, CanvasItem *p_canvas_item, Transform2D p_transform) override;
	virtual void forward_draw_over_alternatives(TileAtlasView *p_tile_atlas_view, TileSetAtlasSource *p_tile_atlas_source, CanvasItem *p_canvas_item, Transform2D p_transform) override;
	virtual void forward_painting_atlas_gui_input(TileAtlasView *p_tile_atlas_view, TileSetAtlasSource *p_tile_atlas_source, const Ref<InputEvent> &p_event) override;
	virtual void forward_painting_alternatives_gui_input(TileAtlasView *p_tile_atlas_view, TileSetAtlasSource *p_tile_atlas_source, const Ref<InputEvent> &p_event) override;
	virtual void draw_over_tile(CanvasItem *p_canvas_item, Transform2D p_transform, TileMapCell p_cell, bool p_selected = false) override;

	TileDataTerrainsEditor();
	~TileDataTerrainsEditor();
};

class TileDataNavigationEditor : public TileDataDefaultEditor {
	GDCLASS(TileDataNavigationEditor, TileDataDefaultEditor);

private:
	int navigation_layer = -1;
	PackedVector2Array navigation_polygon;

	// UI
	GenericTilePolygonEditor *polygon_editor = nullptr;

	void _polygon_changed(const PackedVector2Array &p_polygon);

	virtual Variant _get_painted_value() override;
	virtual void _set_painted_value(TileSetAtlasSource *p_tile_set_atlas_source, Vector2 p_coords, int p_alternative_tile) override;
	virtual void _set_value(TileSetAtlasSource *p_tile_set_atlas_source, Vector2 p_coords, int p_alternative_tile, const Variant &p_value) override;
	virtual Variant _get_value(TileSetAtlasSource *p_tile_set_atlas_source, Vector2 p_coords, int p_alternative_tile) override;
	virtual void _setup_undo_redo_action(TileSetAtlasSource *p_tile_set_atlas_source, const HashMap<TileMapCell, Variant, TileMapCell> &p_previous_values, const Variant &p_new_value) override;

protected:
	virtual void _tile_set_changed() override;

	void _notification(int p_what);

public:
	virtual void draw_over_tile(CanvasItem *p_canvas_item, Transform2D p_transform, TileMapCell p_cell, bool p_selected = false) override;

	void set_navigation_layer(int p_navigation_layer) { navigation_layer = p_navigation_layer; }

	TileDataNavigationEditor();
};

#endif // TILE_DATA_EDITORS_H

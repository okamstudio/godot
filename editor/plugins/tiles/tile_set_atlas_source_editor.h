/**************************************************************************/
/*  tile_set_atlas_source_editor.h                                        */
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

#ifndef TILE_SET_ATLAS_SOURCE_EDITOR_H
#define TILE_SET_ATLAS_SOURCE_EDITOR_H

#include "tile_atlas_view.h"
#include "tile_data_editors.h"

#include "scene/gui/split_container.h"
#include "scene/resources/2d/tile_set.h"

class Popup;
class TileSet;
class Tree;
class VSeparator;

class TileSetAtlasSourceEditor : public HSplitContainer {
	GDCLASS(TileSetAtlasSourceEditor, HSplitContainer);

public:
	// A class to store which tiles are selected.
	struct TileSelection {
		Vector2i tile = TileSetSource::INVALID_ATLAS_COORDS;
		int alternative = TileSetSource::INVALID_TILE_ALTERNATIVE;

		bool operator<(const TileSelection &p_other) const {
			if (tile == p_other.tile) {
				return alternative < p_other.alternative;
			} else {
				return tile < p_other.tile;
			}
		}
	};

	// -- Proxy object for an atlas source, needed by the inspector --
	class TileSetAtlasSourceProxyObject : public Object {
		GDCLASS(TileSetAtlasSourceProxyObject, Object);

	private:
		Ref<TileSet> tile_set;
		Ref<TileSetAtlasSource> tile_set_atlas_source;
		int source_id = TileSet::INVALID_SOURCE;

	protected:
		bool _set(const StringName &p_name, const Variant &p_value);
		bool _get(const StringName &p_name, Variant &r_ret) const;
		void _get_property_list(List<PropertyInfo> *p_list) const;
		static void _bind_methods();

	public:
		void set_id(int p_id);
		int get_id() const;

		void edit(Ref<TileSet> p_tile_set, Ref<TileSetAtlasSource> p_tile_set_atlas_source, int p_source_id);
		Ref<TileSetAtlasSource> get_edited() { return tile_set_atlas_source; }
	};

	// -- Proxy object for a tile, needed by the inspector --
	class AtlasTileProxyObject : public Object {
		GDCLASS(AtlasTileProxyObject, Object);

	private:
		TileSetAtlasSourceEditor *tiles_set_atlas_source_editor = nullptr;

		Ref<TileSetAtlasSource> tile_set_atlas_source;
		RBSet<TileSelection> tiles;

	protected:
		bool _set(const StringName &p_name, const Variant &p_value);
		bool _get(const StringName &p_name, Variant &r_ret) const;
		void _get_property_list(List<PropertyInfo> *p_list) const;

		static void _bind_methods();

	public:
		Ref<TileSetAtlasSource> get_edited_tile_set_atlas_source() const { return tile_set_atlas_source; }
		RBSet<TileSelection> get_edited_tiles() const { return tiles; }

		// Update the proxied object.
		void edit(Ref<TileSetAtlasSource> p_tile_set_atlas_source, const RBSet<TileSelection> &p_tiles = RBSet<TileSelection>());

		AtlasTileProxyObject(TileSetAtlasSourceEditor *p_tiles_set_atlas_source_editor) {
			tiles_set_atlas_source_editor = p_tiles_set_atlas_source_editor;
		}
	};

	class TileAtlasControl : public Control {
		TileSetAtlasSourceEditor *editor = nullptr;

	public:
		virtual CursorShape get_cursor_shape(const Point2 &p_pos) const override;
		TileAtlasControl(TileSetAtlasSourceEditor *p_editor) { editor = p_editor; }
	};
	friend class TileAtlasControl;

private:
	bool read_only = false;

	Ref<TileSet> tile_set;
	TileSetAtlasSource *tile_set_atlas_source = nullptr;
	int tile_set_atlas_source_id = TileSet::INVALID_SOURCE;
	Ref<Texture2D> atlas_source_texture;

	bool tile_set_changed_needs_update = false;

	// -- Properties painting --
	ScrollContainer *tile_data_editors_scroll = nullptr;
	VBoxContainer *tile_data_painting_editor_container = nullptr;
	Label *tile_data_editors_label = nullptr;
	Button *tile_data_editor_dropdown_button = nullptr;
	Popup *tile_data_editors_popup = nullptr;
	Tree *tile_data_editors_tree = nullptr;
	void _tile_data_editor_dropdown_button_draw();
	void _tile_data_editor_dropdown_button_pressed();

	// -- Tile data editors --
	String current_property;
	Control *current_tile_data_editor_toolbar = nullptr;
	HashMap<String, TileDataEditor *> tile_data_editors;
	TileDataEditor *current_tile_data_editor = nullptr;
	void _tile_data_editors_tree_selected();

	// -- Inspector --
	AtlasTileProxyObject *tile_proxy_object = nullptr;
	EditorInspector *tile_inspector = nullptr;
	Label *tile_inspector_no_tile_selected_label = nullptr;
	String selected_property;
	void _inspector_property_selected(const String &p_property);

	TileSetAtlasSourceProxyObject *atlas_source_proxy_object = nullptr;
	EditorInspector *atlas_source_inspector = nullptr;

	// -- Atlas view --
	TileAtlasView *tile_atlas_view = nullptr;
	VBoxContainer *tile_create_help = nullptr;

	// Dragging
	enum DragType {
		DRAG_TYPE_NONE = 0,
		DRAG_TYPE_CREATE_TILES,
		DRAG_TYPE_CREATE_TILES_USING_RECT,
		DRAG_TYPE_CREATE_BIG_TILE,
		DRAG_TYPE_REMOVE_TILES,
		DRAG_TYPE_REMOVE_TILES_USING_RECT,

		DRAG_TYPE_MOVE_TILE,

		DRAG_TYPE_RECT_SELECT,

		DRAG_TYPE_MAY_POPUP_MENU,

		// WARNING: Keep in this order.
		DRAG_TYPE_RESIZE_TOP_LEFT,
		DRAG_TYPE_RESIZE_TOP,
		DRAG_TYPE_RESIZE_TOP_RIGHT,
		DRAG_TYPE_RESIZE_RIGHT,
		DRAG_TYPE_RESIZE_BOTTOM_RIGHT,
		DRAG_TYPE_RESIZE_BOTTOM,
		DRAG_TYPE_RESIZE_BOTTOM_LEFT,
		DRAG_TYPE_RESIZE_LEFT,
	};
	DragType drag_type = DRAG_TYPE_NONE;
	Vector2i drag_start_mouse_pos;
	Vector2i drag_last_mouse_pos;
	Vector2i drag_current_tile;

	Rect2i drag_start_tile_shape;
	RBSet<Vector2i> drag_modified_tiles;
	void _end_dragging();

	HashMap<Vector2i, List<const PropertyInfo *>> _group_properties_per_tiles(const List<PropertyInfo> &r_list, const TileSetAtlasSource *p_atlas);

	// Popup functions.
	enum MenuOptions {
		TILE_CREATE,
		TILE_CREATE_ALTERNATIVE,
		TILE_DELETE,

		ADVANCED_AUTO_CREATE_TILES,
		ADVANCED_AUTO_REMOVE_TILES,
		ADVANCED_CLEANUP_TILES,
	};
	Vector2i menu_option_coords;
	int menu_option_alternative = TileSetSource::INVALID_TILE_ALTERNATIVE;
	void _menu_option(int p_option);

	// Tool buttons.
	Ref<ButtonGroup> tools_button_group;
	Button *tool_setup_atlas_source_button = nullptr;
	Button *tool_select_button = nullptr;
	Button *tool_paint_button = nullptr;
	Label *tool_tile_id_label = nullptr;

	// Tool settings.
	HBoxContainer *tool_settings = nullptr;
	HBoxContainer *tool_settings_tile_data_toolbar_container = nullptr;
	Button *tools_settings_erase_button = nullptr;
	MenuButton *tool_advanced_menu_button = nullptr;
	TextureRect *outside_tiles_warning = nullptr;

	// Selection.
	RBSet<TileSelection> selection;

	void _set_selection_from_array(const Array &p_selection);
	Array _get_selection_as_array();

	// A control on the tile atlas to draw and handle input events.
	Vector2i hovered_base_tile_coords = TileSetSource::INVALID_ATLAS_COORDS;

	PopupMenu *base_tile_popup_menu = nullptr;
	PopupMenu *empty_base_tile_popup_menu = nullptr;
	Ref<Texture2D> resize_handle;
	Ref<Texture2D> resize_handle_disabled;
	Control *tile_atlas_control = nullptr;
	Control *tile_atlas_control_unscaled = nullptr;
	void _tile_atlas_control_draw();
	void _tile_atlas_control_unscaled_draw();
	void _tile_atlas_control_mouse_exited();
	void _tile_atlas_control_gui_input(const Ref<InputEvent> &p_event);
	void _tile_atlas_view_transform_changed();

	// A control over the alternative tiles.
	Vector3i hovered_alternative_tile_coords = Vector3i(TileSetSource::INVALID_ATLAS_COORDS.x, TileSetSource::INVALID_ATLAS_COORDS.y, TileSetSource::INVALID_TILE_ALTERNATIVE);

	PopupMenu *alternative_tile_popup_menu = nullptr;
	Control *alternative_tiles_control = nullptr;
	Control *alternative_tiles_control_unscaled = nullptr;
	void _tile_alternatives_control_draw();
	void _tile_alternatives_control_unscaled_draw();
	void _tile_alternatives_control_mouse_exited();
	void _tile_alternatives_control_gui_input(const Ref<InputEvent> &p_event);

	// -- Update functions --
	void _update_tile_id_label();
	void _update_source_inspector();
	void _update_fix_selected_and_hovered_tiles();
	void _update_atlas_source_inspector();
	void _update_tile_inspector();
	void _update_tile_data_editors();
	void _update_current_tile_data_editor();
	void _update_manage_tile_properties_button();
	void _update_atlas_view();
	void _update_toolbar();
	void _update_buttons();

	// -- Misc --
	void _auto_create_tiles();
	void _auto_remove_tiles();
	void _cancel_auto_create_tiles();
	AcceptDialog *confirm_auto_create_tiles = nullptr;
	Vector<Ref<TileSetAtlasSource>> atlases_to_auto_create_tiles;
	Vector2i _get_drag_offset_tile_coords(const Vector2i &p_offset) const;

	void _update_source_texture();
	void _check_outside_tiles();
	void _cleanup_outside_tiles();

	void _tile_set_changed();
	void _tile_proxy_object_changed(const String &p_what);
	void _atlas_source_proxy_object_changed(const String &p_what);

	void _undo_redo_inspector_callback(Object *p_undo_redo, Object *p_edited, const String &p_property, const Variant &p_new_value);

protected:
	void _notification(int p_what);
	static void _bind_methods();

	// -- input events --
	virtual void shortcut_input(const Ref<InputEvent> &p_event) override;

public:
	void edit(Ref<TileSet> p_tile_set, TileSetAtlasSource *p_tile_set_source, int p_source_id);
	void init_new_atlases(const Vector<Ref<TileSetAtlasSource>> &p_atlases);

	TileSetAtlasSourceEditor();
	~TileSetAtlasSourceEditor();
};

class EditorPropertyTilePolygon : public EditorProperty {
	GDCLASS(EditorPropertyTilePolygon, EditorProperty);

	StringName count_property;
	String element_pattern;
	String base_type;

	void _add_focusable_children(Node *p_node);

	GenericTilePolygonEditor *generic_tile_polygon_editor = nullptr;
	void _polygons_changed();

public:
	virtual void update_property() override;
	void setup_single_mode(const StringName &p_property, const String &p_base_type);
	void setup_multiple_mode(const StringName &p_property, const StringName &p_count_property, const String &p_element_pattern, const String &p_base_type);
	EditorPropertyTilePolygon();
};

class EditorInspectorPluginTileData : public EditorInspectorPlugin {
	GDCLASS(EditorInspectorPluginTileData, EditorInspectorPlugin);

	void _occlusion_polygon_set_callback();
	void _polygons_changed(Object *p_generic_tile_polygon_editor, Object *p_object, const String &p_path);

public:
	virtual bool can_handle(Object *p_object) override;
	virtual bool parse_property(Object *p_object, const Variant::Type p_type, const String &p_path, const PropertyHint p_hint, const String &p_hint_text, const BitField<PropertyUsageFlags> p_usage, const bool p_wide = false) override;
};

#endif // TILE_SET_ATLAS_SOURCE_EDITOR_H

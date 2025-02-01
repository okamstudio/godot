/**************************************************************************/
/*  grid_map_editor_plugin.h                                              */
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

#ifdef TOOLS_ENABLED

#include "../grid_map.h"

#include "editor/plugins/editor_plugin.h"
#include "scene/gui/box_container.h"
#include "scene/gui/item_list.h"
#include "scene/gui/slider.h"
#include "scene/gui/spin_box.h"

class ConfirmationDialog;
class MenuButton;
class Node3DEditorPlugin;
class ButtonGroup;
class EditorZoomWidget;
class BaseButton;

class GridMapEditor : public VBoxContainer {
	GDCLASS(GridMapEditor, VBoxContainer);

	enum {
		GRID_CURSOR_SIZE = 50
	};

	enum InputAction {
		INPUT_NONE,
		INPUT_TRANSFORM,
		INPUT_PAINT,
		INPUT_ERASE,
		INPUT_PICK,
		INPUT_SELECT,
		INPUT_PASTE,
	};

	enum DisplayMode {
		DISPLAY_THUMBNAIL,
		DISPLAY_LIST
	};

	InputAction input_action = INPUT_NONE;
	Panel *panel = nullptr;
	MenuButton *options = nullptr;
	SpinBox *floor = nullptr;
	double accumulated_floor_delta = 0.0;

	HBoxContainer *toolbar = nullptr;
	List<BaseButton *> viewport_shortcut_buttons;
	Ref<ButtonGroup> mode_buttons_group;
	// mode
	Button *transform_mode_button = nullptr;
	Button *select_mode_button = nullptr;
	Button *erase_mode_button = nullptr;
	Button *paint_mode_button = nullptr;
	Button *pick_mode_button = nullptr;
	// action
	Button *fill_action_button = nullptr;
	Button *move_action_button = nullptr;
	Button *duplicate_action_button = nullptr;
	Button *delete_action_button = nullptr;
	// rotation
	Button *rotate_x_button = nullptr;
	Button *rotate_y_button = nullptr;
	Button *rotate_z_button = nullptr;

	EditorZoomWidget *zoom_widget = nullptr;
	Button *mode_thumbnail = nullptr;
	Button *mode_list = nullptr;
	LineEdit *search_box = nullptr;
	HSlider *size_slider = nullptr;
	ConfirmationDialog *settings_dialog = nullptr;
	VBoxContainer *settings_vbc = nullptr;
	SpinBox *settings_pick_distance = nullptr;
	Label *spin_box_label = nullptr;

	struct SetItem {
		Vector3i position;
		int new_value = 0;
		int new_orientation = 0;
		int old_value = 0;
		int old_orientation = 0;
	};

	List<SetItem> set_items;

	GridMap *node = nullptr;
	Ref<MeshLibrary> mesh_library = nullptr;

	Transform3D grid_xform;
	Transform3D edit_grid_xform;
	Vector3::Axis edit_axis;
	int edit_floor[3];
	Vector3 grid_ofs;

	RID grid[3];
	RID grid_instance[3];
	RID cursor_mesh;
	RID cursor_instance;
	RID selection_mesh;
	RID selection_instance;
	RID selection_level_mesh[3];
	RID selection_level_instance[3];
	RID paste_mesh;
	RID paste_instance;

	struct ClipboardItem {
		int cell_item = 0;
		Vector3 grid_offset;
		int orientation = 0;
		RID instance;
	};

	List<ClipboardItem> clipboard_items;

	Color default_color;
	Color erase_color;
	Color pick_color;
	Ref<StandardMaterial3D> indicator_mat;
	Ref<StandardMaterial3D> cursor_inner_mat;
	Ref<StandardMaterial3D> cursor_outer_mat;
	Ref<StandardMaterial3D> inner_mat;
	Ref<StandardMaterial3D> outer_mat;
	Ref<StandardMaterial3D> selection_floor_mat;

	bool updating = false;

	struct Selection {
		Vector3 click;
		Vector3 current;
		Vector3 begin;
		Vector3 end;
		bool active = false;
	} selection;
	Selection last_selection;

	struct PasteIndicator {
		Vector3 click;
		Vector3 current;
		Vector3 begin;
		Vector3 end;
		int orientation = 0;
	};
	PasteIndicator paste_indicator;

	bool cursor_visible = false;
	Transform3D cursor_transform;

	Vector3 cursor_origin;

	int display_mode = DISPLAY_THUMBNAIL;
	int selected_palette = -1;
	int cursor_rot = 0;

	enum Menu {
		MENU_OPTION_NEXT_LEVEL,
		MENU_OPTION_PREV_LEVEL,
		MENU_OPTION_LOCK_VIEW,
		MENU_OPTION_X_AXIS,
		MENU_OPTION_Y_AXIS,
		MENU_OPTION_Z_AXIS,
		MENU_OPTION_CURSOR_ROTATE_Y,
		MENU_OPTION_CURSOR_ROTATE_X,
		MENU_OPTION_CURSOR_ROTATE_Z,
		MENU_OPTION_CURSOR_BACK_ROTATE_Y,
		MENU_OPTION_CURSOR_BACK_ROTATE_X,
		MENU_OPTION_CURSOR_BACK_ROTATE_Z,
		MENU_OPTION_CURSOR_CLEAR_ROTATION,
		MENU_OPTION_PASTE_SELECTS,
		MENU_OPTION_SELECTION_DUPLICATE,
		MENU_OPTION_SELECTION_CUT,
		MENU_OPTION_SELECTION_CLEAR,
		MENU_OPTION_SELECTION_FILL,
		MENU_OPTION_GRIDMAP_SETTINGS

	};

	Node3DEditorPlugin *spatial_editor = nullptr;

	struct AreaDisplay {
		RID mesh;
		RID instance;
	};

	ItemList *mesh_library_palette = nullptr;
	Label *info_message = nullptr;

	void update_grid(); // Change which and where the grid is displayed
	void _draw_grids(const Vector3 &cell_size);
	void _configure();
	void _menu_option(int);
	void update_palette();
	void _update_mesh_library();
	void _set_display_mode(int p_mode);
	void _item_selected_cbk(int idx);
	void _update_cursor_transform();
	void _update_cursor_instance();
	void _on_tool_mode_changed();
	void _update_theme();

	void _text_changed(const String &p_text);
	void _sbox_input(const Ref<InputEvent> &p_event);
	void _mesh_library_palette_input(const Ref<InputEvent> &p_ie);

	void _icon_size_changed(float p_value);

	void _clear_clipboard_data();
	void _set_clipboard_data();
	void _update_paste_indicator();
	void _do_paste();
	void _show_viewports_transform_gizmo(bool p_value);
	void _update_selection_transform();
	void _validate_selection();
	void _set_selection(bool p_active, const Vector3 &p_begin = Vector3(), const Vector3 &p_end = Vector3());
	AABB _get_selection() const;
	bool _has_selection() const;
	Array _get_selected_cells() const;

	void _floor_changed(float p_value);
	void _floor_mouse_exited();

	void _delete_selection();
	void _fill_selection();

	bool do_input_action(Camera3D *p_camera, const Point2 &p_point, bool p_click);

	friend class GridMapEditorPlugin;

protected:
	void _notification(int p_what);
	static void _bind_methods();

public:
	EditorPlugin::AfterGUIInput forward_spatial_input_event(Camera3D *p_camera, const Ref<InputEvent> &p_event);

	void edit(GridMap *p_gridmap);
	GridMapEditor();
	~GridMapEditor();
};

class GridMapEditorPlugin : public EditorPlugin {
	GDCLASS(GridMapEditorPlugin, EditorPlugin);

	GridMapEditor *grid_map_editor = nullptr;
	Button *panel_button = nullptr;

protected:
	void _notification(int p_what);
	static void _bind_methods();

public:
	virtual EditorPlugin::AfterGUIInput forward_3d_gui_input(Camera3D *p_camera, const Ref<InputEvent> &p_event) override { return grid_map_editor->forward_spatial_input_event(p_camera, p_event); }
	virtual String get_plugin_name() const override { return "GridMap"; }
	bool has_main_screen() const override { return false; }
	virtual void edit(Object *p_object) override;
	virtual bool handles(Object *p_object) const override;
	virtual void make_visible(bool p_visible) override;

	GridMap *get_current_grid_map() const;
	void set_selection(const Vector3i &p_begin, const Vector3i &p_end);
	void clear_selection();
	AABB get_selection() const;
	bool has_selection() const;
	Array get_selected_cells() const;
	void set_selected_palette_item(int p_item) const;
	int get_selected_palette_item() const;

	GridMapEditorPlugin();
	~GridMapEditorPlugin();
};

#endif // TOOLS_ENABLED

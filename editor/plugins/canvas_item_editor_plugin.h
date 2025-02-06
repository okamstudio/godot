/**************************************************************************/
/*  canvas_item_editor_plugin.h                                           */
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

#include "editor/plugins/editor_plugin.h"
#include "scene/gui/box_container.h"

class AcceptDialog;
class Button;
class ButtonGroup;
class CanvasItemEditorViewport;
class ConfirmationDialog;
class EditorData;
class EditorSelection;
class EditorZoomWidget;
class HScrollBar;
class HSplitContainer;
class MenuButton;
class PanelContainer;
class StyleBoxTexture;
class ViewPanner;
class VScrollBar;
class VSeparator;
class VSplitContainer;

class CanvasItemEditorSelectedItem : public Object {
	GDCLASS(CanvasItemEditorSelectedItem, Object);

public:
	Transform2D prev_xform;
	Rect2 prev_rect;
	Vector2 prev_pivot;
	real_t prev_anchors[4] = { (real_t)0.0 };

	Transform2D pre_drag_xform;
	Rect2 pre_drag_rect;

	List<real_t> pre_drag_bones_length;
	List<Dictionary> pre_drag_bones_undo_state;

	Dictionary undo_state;

	CanvasItemEditorSelectedItem() {}
};

class CanvasItemEditor : public VBoxContainer {
	GDCLASS(CanvasItemEditor, VBoxContainer);

public:
	enum Tool {
		TOOL_SELECT,
		TOOL_LIST_SELECT,
		TOOL_MOVE,
		TOOL_SCALE,
		TOOL_ROTATE,
		TOOL_EDIT_PIVOT,
		TOOL_PAN,
		TOOL_RULER,
		TOOL_MAX
	};

	enum AddNodeOption {
		ADD_NODE,
		ADD_INSTANCE,
		ADD_PASTE,
		ADD_MOVE,
	};

private:
	enum SnapTarget {
		SNAP_TARGET_NONE = 0,
		SNAP_TARGET_PARENT,
		SNAP_TARGET_SELF_ANCHORS,
		SNAP_TARGET_SELF,
		SNAP_TARGET_OTHER_NODE,
		SNAP_TARGET_GUIDE,
		SNAP_TARGET_GRID,
		SNAP_TARGET_PIXEL
	};

	enum MenuOption {
		SNAP_USE,
		SNAP_USE_NODE_PARENT,
		SNAP_USE_NODE_ANCHORS,
		SNAP_USE_NODE_SIDES,
		SNAP_USE_NODE_CENTER,
		SNAP_USE_OTHER_NODES,
		SNAP_USE_GRID,
		SNAP_USE_GUIDES,
		SNAP_USE_ROTATION,
		SNAP_USE_SCALE,
		SNAP_RELATIVE,
		SNAP_CONFIGURE,
		SNAP_USE_PIXEL,
		SHOW_HELPERS,
		SHOW_RULERS,
		SHOW_GUIDES,
		SHOW_ORIGIN,
		SHOW_VIEWPORT,
		SHOW_POSITION_GIZMOS,
		SHOW_LOCK_GIZMOS,
		SHOW_GROUP_GIZMOS,
		SHOW_TRANSFORMATION_GIZMOS,
		LOCK_SELECTED,
		UNLOCK_SELECTED,
		GROUP_SELECTED,
		UNGROUP_SELECTED,
		ANIM_INSERT_KEY,
		ANIM_INSERT_KEY_EXISTING,
		ANIM_INSERT_POS,
		ANIM_INSERT_ROT,
		ANIM_INSERT_SCALE,
		ANIM_COPY_POSE,
		ANIM_PASTE_POSE,
		ANIM_CLEAR_POSE,
		CLEAR_GUIDES,
		VIEW_CENTER_TO_SELECTION,
		VIEW_FRAME_TO_SELECTION,
		PREVIEW_CANVAS_SCALE,
		SKELETON_MAKE_BONES,
		SKELETON_SHOW_BONES
	};

	enum DragType {
		DRAG_NONE,
		DRAG_BOX_SELECTION,
		DRAG_LEFT,
		DRAG_TOP_LEFT,
		DRAG_TOP,
		DRAG_TOP_RIGHT,
		DRAG_RIGHT,
		DRAG_BOTTOM_RIGHT,
		DRAG_BOTTOM,
		DRAG_BOTTOM_LEFT,
		DRAG_ANCHOR_TOP_LEFT,
		DRAG_ANCHOR_TOP_RIGHT,
		DRAG_ANCHOR_BOTTOM_RIGHT,
		DRAG_ANCHOR_BOTTOM_LEFT,
		DRAG_ANCHOR_ALL,
		DRAG_QUEUED,
		DRAG_MOVE,
		DRAG_MOVE_X,
		DRAG_MOVE_Y,
		DRAG_SCALE_X,
		DRAG_SCALE_Y,
		DRAG_SCALE_BOTH,
		DRAG_ROTATE,
		DRAG_PIVOT,
		DRAG_TEMP_PIVOT,
		DRAG_V_GUIDE,
		DRAG_H_GUIDE,
		DRAG_DOUBLE_GUIDE,
		DRAG_KEY_MOVE
	};

	enum GridVisibility {
		GRID_VISIBILITY_SHOW,
		GRID_VISIBILITY_SHOW_WHEN_SNAPPING,
		GRID_VISIBILITY_HIDE,
	};

	const String locked_transform_warning = TTRC("All selected CanvasItems are either invisible or locked in some way and can't be transformed.");

	bool selection_menu_additive_selection = false;

	Tool tool = TOOL_SELECT;
	Control *viewport = nullptr;
	Control *viewport_scrollable = nullptr;

	HScrollBar *h_scroll = nullptr;
	VScrollBar *v_scroll = nullptr;

	// Used for secondary menu items which are displayed depending on the currently selected node
	// (such as MeshInstance's "Mesh" menu).
	PanelContainer *context_toolbar_panel = nullptr;
	HBoxContainer *context_toolbar_hbox = nullptr;
	HashMap<Control *, VSeparator *> context_toolbar_separators;

	void _update_context_toolbar();

	Transform2D transform;
	GridVisibility grid_visibility = GRID_VISIBILITY_SHOW_WHEN_SNAPPING;
	bool show_rulers = true;
	bool show_guides = true;
	bool show_origin = true;
	bool show_viewport = true;
	bool show_helpers = false;
	bool show_position_gizmos = true;
	bool show_lock_gizmos = true;
	bool show_group_gizmos = true;
	bool show_transformation_gizmos = true;

	real_t zoom = 1.0;
	Point2 view_offset;
	Point2 previous_update_view_offset;

	bool selected_from_canvas = false;

	// Defaults are defined in clear().
	Point2 grid_offset;
	Point2 grid_step;
	Vector2i primary_grid_step;
	int grid_step_multiplier = 0;

	real_t snap_rotation_step = 0.0;
	real_t snap_rotation_offset = 0.0;
	real_t snap_scale_step = 0.0;
	bool smart_snap_active = false;
	bool grid_snap_active = false;

	bool snap_node_parent = true;
	bool snap_node_anchors = true;
	bool snap_node_sides = true;
	bool snap_node_center = true;
	bool snap_other_nodes = true;
	bool snap_guides = true;
	bool snap_rotation = false;
	bool snap_scale = false;
	bool snap_relative = false;
	// Enable pixel snapping even if pixel snap rendering is disabled in the Project Settings.
	// This results in crisper visuals by preventing 2D nodes from being placed at subpixel coordinates.
	bool snap_pixel = true;

	bool key_pos = true;
	bool key_rot = true;
	bool key_scale = false;

	bool pan_pressed = false;
	Vector2 temp_pivot = Vector2(INFINITY, INFINITY);

	bool ruler_tool_active = false;
	Point2 ruler_tool_origin;
	Point2 node_create_position;

	MenuOption last_option;

	struct _SelectResult {
		CanvasItem *item = nullptr;
		real_t z_index = 0;
		bool has_z = true;
		_FORCE_INLINE_ bool operator<(const _SelectResult &p_rr) const {
			return has_z && p_rr.has_z ? p_rr.z_index < z_index : p_rr.has_z;
		}
	};
	Vector<_SelectResult> selection_results;
	Vector<_SelectResult> selection_results_menu;

	struct _HoverResult {
		Point2 position;
		Ref<Texture2D> icon;
		String name;
	};
	Vector<_HoverResult> hovering_results;

	struct BoneList {
		Transform2D xform;
		real_t length = 0;
		uint64_t last_pass = 0;
	};

	uint64_t bone_last_frame = 0;

	struct BoneKey {
		ObjectID from;
		ObjectID to;
		_FORCE_INLINE_ bool operator<(const BoneKey &p_key) const {
			if (from == p_key.from) {
				return to < p_key.to;
			} else {
				return from < p_key.from;
			}
		}
	};

	HashMap<BoneKey, BoneList> bone_list;
	MenuButton *skeleton_menu = nullptr;

	struct PoseClipboard {
		Vector2 pos;
		Vector2 scale;
		real_t rot = 0;
		ObjectID id;
	};
	List<PoseClipboard> pose_clipboard;

	Button *select_button = nullptr;

	Button *move_button = nullptr;
	Button *scale_button = nullptr;
	Button *rotate_button = nullptr;

	Button *list_select_button = nullptr;
	Button *pivot_button = nullptr;
	Button *pan_button = nullptr;

	Button *ruler_button = nullptr;

	Button *smart_snap_button = nullptr;
	Button *grid_snap_button = nullptr;
	MenuButton *snap_config_menu = nullptr;
	PopupMenu *smartsnap_config_popup = nullptr;

	Button *lock_button = nullptr;
	Button *unlock_button = nullptr;

	Button *group_button = nullptr;
	Button *ungroup_button = nullptr;

	MenuButton *view_menu = nullptr;
	PopupMenu *grid_menu = nullptr;
	PopupMenu *theme_menu = nullptr;
	PopupMenu *gizmos_menu = nullptr;
	HBoxContainer *animation_hb = nullptr;
	MenuButton *animation_menu = nullptr;

	Button *key_loc_button = nullptr;
	Button *key_rot_button = nullptr;
	Button *key_scale_button = nullptr;
	Button *key_insert_button = nullptr;
	Button *key_auto_insert_button = nullptr;

	PopupMenu *selection_menu = nullptr;
	PopupMenu *add_node_menu = nullptr;

	Control *top_ruler = nullptr;
	Control *left_ruler = nullptr;

	Point2 drag_start_origin;
	DragType drag_type = DRAG_NONE;
	Point2 drag_from;
	Point2 drag_to;
	Point2 drag_rotation_center;
	List<CanvasItem *> drag_selection;
	int dragged_guide_index = -1;
	Point2 dragged_guide_pos;
	bool is_hovering_h_guide = false;
	bool is_hovering_v_guide = false;

	bool updating_value_dialog = false;
	Transform2D original_transform;

	Point2 box_selecting_to;

	Ref<StyleBoxTexture> select_sb;
	Ref<Texture2D> select_handle;
	Ref<Texture2D> anchor_handle;

	Ref<Shortcut> drag_pivot_shortcut;
	Ref<Shortcut> set_pivot_shortcut;
	Ref<Shortcut> multiply_grid_step_shortcut;
	Ref<Shortcut> divide_grid_step_shortcut;

	Ref<ViewPanner> panner;
	void _pan_callback(Vector2 p_scroll_vec, Ref<InputEvent> p_event);
	void _zoom_callback(float p_zoom_factor, Vector2 p_origin, Ref<InputEvent> p_event);

	bool _is_node_locked(const Node *p_node) const;
	bool _is_node_movable(const Node *p_node, bool p_popup_warning = false);
	void _find_canvas_items_at_pos(const Point2 &p_pos, Node *p_node, Vector<_SelectResult> &r_items, const Transform2D &p_parent_xform = Transform2D(), const Transform2D &p_canvas_xform = Transform2D());
	void _get_canvas_items_at_pos(const Point2 &p_pos, Vector<_SelectResult> &r_items, bool p_allow_locked = false);

	void _find_canvas_items_in_rect(const Rect2 &p_rect, Node *p_node, List<CanvasItem *> *r_items, const Transform2D &p_parent_xform = Transform2D(), const Transform2D &p_canvas_xform = Transform2D());
	bool _select_click_on_item(CanvasItem *item, Point2 p_click_pos, bool p_append);

	ConfirmationDialog *snap_dialog = nullptr;

	CanvasItem *ref_item = nullptr;

	void _save_canvas_item_state(const List<CanvasItem *> &p_canvas_items, bool save_bones = false);
	void _restore_canvas_item_state(const List<CanvasItem *> &p_canvas_items, bool restore_bones = false);
	void _commit_canvas_item_state(const List<CanvasItem *> &p_canvas_items, const String &action_name, bool commit_bones = false);

	Vector2 _anchor_to_position(const Control *p_control, Vector2 anchor);
	Vector2 _position_to_anchor(const Control *p_control, Vector2 position);

	void _prepare_view_menu();
	void _popup_callback(int p_op);
	bool updating_scroll = false;
	void _update_scroll(real_t);
	void _update_scrollbars();
	void _snap_changed();
	void _selection_result_pressed(int);
	void _selection_menu_hide();
	void _add_node_pressed(int p_result);
	void _adjust_new_node_position(Node *p_node);
	void _reset_create_position();
	void _update_editor_settings();
	bool _is_grid_visible() const;
	void _prepare_grid_menu();
	void _on_grid_menu_id_pressed(int p_id);

public:
	enum ThemePreviewMode {
		THEME_PREVIEW_PROJECT,
		THEME_PREVIEW_EDITOR,
		THEME_PREVIEW_DEFAULT,

		THEME_PREVIEW_MAX // The number of options for enumerating.
	};

private:
	ThemePreviewMode theme_preview = THEME_PREVIEW_PROJECT;
	void _switch_theme_preview(int p_mode);

	List<CanvasItem *> _get_edited_canvas_items(bool p_retrieve_locked = false, bool p_remove_canvas_item_if_parent_in_selection = true, bool *r_has_locked_items = nullptr) const;
	Rect2 _get_encompassing_rect_from_list(const List<CanvasItem *> &p_list);
	void _expand_encompassing_rect_using_children(Rect2 &r_rect, const Node *p_node, bool &r_first, const Transform2D &p_parent_xform = Transform2D(), const Transform2D &p_canvas_xform = Transform2D(), bool include_locked_nodes = true);
	Rect2 _get_encompassing_rect(const Node *p_node);

	Object *_get_editor_data(Object *p_what);

	void _insert_animation_keys(bool p_location, bool p_rotation, bool p_scale, bool p_on_existing);

	void _keying_changed();

	virtual void shortcut_input(const Ref<InputEvent> &p_ev) override;

	void _draw_text_at_position(Point2 p_position, const String &p_string, Side p_side);
	void _draw_margin_at_position(int p_value, Point2 p_position, Side p_side);
	void _draw_percentage_at_position(real_t p_value, Point2 p_position, Side p_side);
	void _draw_straight_line(Point2 p_from, Point2 p_to, Color p_color);

	void _draw_smart_snapping();
	void _draw_rulers();
	void _draw_guides();
	void _draw_focus();
	void _draw_grid();
	void _draw_ruler_tool();
	void _draw_control_anchors(Control *control);
	void _draw_control_helpers(Control *control);
	void _draw_selection();
	void _draw_axis();
	void _draw_invisible_nodes_positions(Node *p_node, const Transform2D &p_parent_xform = Transform2D(), const Transform2D &p_canvas_xform = Transform2D());
	void _draw_locks_and_groups(Node *p_node, const Transform2D &p_parent_xform = Transform2D(), const Transform2D &p_canvas_xform = Transform2D());
	void _draw_hover();
	void _draw_message();

	void _draw_viewport();

	bool _gui_input_anchors(const Ref<InputEvent> &p_event);
	bool _gui_input_move(const Ref<InputEvent> &p_event);
	bool _gui_input_open_scene_on_double_click(const Ref<InputEvent> &p_event);
	bool _gui_input_scale(const Ref<InputEvent> &p_event);
	bool _gui_input_pivot(const Ref<InputEvent> &p_event);
	bool _gui_input_resize(const Ref<InputEvent> &p_event);
	bool _gui_input_rotate(const Ref<InputEvent> &p_event);
	bool _gui_input_select(const Ref<InputEvent> &p_event);
	bool _gui_input_ruler_tool(const Ref<InputEvent> &p_event);
	bool _gui_input_zoom_or_pan(const Ref<InputEvent> &p_event, bool p_already_accepted);
	bool _gui_input_rulers_and_guides(const Ref<InputEvent> &p_event);
	bool _gui_input_hover(const Ref<InputEvent> &p_event);

	void _gui_input_viewport(const Ref<InputEvent> &p_event);
	void _update_cursor();
	void _update_lock_and_group_button();

	void _selection_changed();
	void _focus_selection(int p_op);
	void _reset_drag();

	void _project_settings_changed();

	SnapTarget snap_target[2];
	Transform2D snap_transform;
	void _snap_if_closer_float(
			const real_t p_value,
			real_t &r_current_snap, SnapTarget &r_current_snap_target,
			const real_t p_target_value, const SnapTarget p_snap_target,
			const real_t p_radius = 10.0);
	void _snap_if_closer_point(
			Point2 p_value,
			Point2 &r_current_snap, SnapTarget (&r_current_snap_target)[2],
			Point2 p_target_value, const SnapTarget p_snap_target,
			const real_t rotation = 0.0,
			const real_t p_radius = 10.0);
	void _snap_other_nodes(
			const Point2 p_value,
			const Transform2D p_transform_to_snap,
			Point2 &r_current_snap, SnapTarget (&r_current_snap_target)[2],
			const SnapTarget p_snap_target, List<const CanvasItem *> p_exceptions,
			const Node *p_current);

	VBoxContainer *controls_vb = nullptr;
	Button *button_center_view = nullptr;
	EditorZoomWidget *zoom_widget = nullptr;
	void _update_zoom(real_t p_zoom);
	void _shortcut_zoom_set(real_t p_zoom);
	void _zoom_on_position(real_t p_zoom, Point2 p_position = Point2());
	void _button_toggle_smart_snap(bool p_status);
	void _button_toggle_grid_snap(bool p_status);
	void _button_tool_select(int p_index);

	HSplitContainer *left_panel_split = nullptr;
	HSplitContainer *right_panel_split = nullptr;
	VSplitContainer *bottom_split = nullptr;

	void _set_owner_for_node_and_children(Node *p_node, Node *p_owner);

	friend class CanvasItemEditorPlugin;

protected:
	void _notification(int p_what);

	static void _bind_methods();

	static CanvasItemEditor *singleton;

public:
	enum SnapMode {
		SNAP_GRID = 1 << 0,
		SNAP_GUIDES = 1 << 1,
		SNAP_PIXEL = 1 << 2,
		SNAP_NODE_PARENT = 1 << 3,
		SNAP_NODE_ANCHORS = 1 << 4,
		SNAP_NODE_SIDES = 1 << 5,
		SNAP_NODE_CENTER = 1 << 6,
		SNAP_OTHER_NODES = 1 << 7,

		SNAP_DEFAULT = SNAP_GRID | SNAP_GUIDES | SNAP_PIXEL,
	};

	String message;

	Point2 snap_point(Point2 p_target, unsigned int p_modes = SNAP_DEFAULT, unsigned int p_forced_modes = 0, const CanvasItem *p_self_canvas_item = nullptr, const List<CanvasItem *> &p_other_nodes_exceptions = List<CanvasItem *>());
	real_t snap_angle(real_t p_target, real_t p_start = 0) const;

	Transform2D get_canvas_transform() const { return transform; }

	static CanvasItemEditor *get_singleton() { return singleton; }
	Dictionary get_state() const;
	void set_state(const Dictionary &p_state);
	void clear();

	void add_control_to_menu_panel(Control *p_control);
	void remove_control_from_menu_panel(Control *p_control);

	void add_control_to_left_panel(Control *p_control);
	void remove_control_from_left_panel(Control *p_control);

	void add_control_to_right_panel(Control *p_control);
	void remove_control_from_right_panel(Control *p_control);

	VSplitContainer *get_bottom_split();

	Control *get_viewport_control() { return viewport; }

	Control *get_controls_container() { return controls_vb; }

	void update_viewport();

	Tool get_current_tool() { return tool; }
	void set_current_tool(Tool p_tool);

	void edit(CanvasItem *p_canvas_item);

	void focus_selection();
	void center_at(const Point2 &p_pos);

	virtual CursorShape get_cursor_shape(const Point2 &p_pos) const override;

	ThemePreviewMode get_theme_preview() const { return theme_preview; }

	EditorSelection *editor_selection = nullptr;

	CanvasItemEditor();
};

class CanvasItemEditorPlugin : public EditorPlugin {
	GDCLASS(CanvasItemEditorPlugin, EditorPlugin);

	CanvasItemEditor *canvas_item_editor = nullptr;

protected:
	void _notification(int p_what);

public:
	virtual String get_plugin_name() const override { return "2D"; }
	bool has_main_screen() const override { return true; }
	virtual void edit(Object *p_object) override;
	virtual bool handles(Object *p_object) const override;
	virtual void make_visible(bool p_visible) override;
	virtual Dictionary get_state() const override;
	virtual void set_state(const Dictionary &p_state) override;
	virtual void clear() override;

	CanvasItemEditor *get_canvas_item_editor() { return canvas_item_editor; }

	CanvasItemEditorPlugin();
	~CanvasItemEditorPlugin();
};

class CanvasItemEditorViewport : public Control {
	GDCLASS(CanvasItemEditorViewport, Control);

	// The type of node that will be created when dropping texture into the viewport.
	String default_texture_node_type;
	// Node types that are available to select from when dropping texture into viewport.
	Vector<String> texture_node_types;

	Vector<String> selected_files;
	Node *target_node = nullptr;
	Point2 drop_pos;

	CanvasItemEditor *canvas_item_editor = nullptr;
	Control *preview_node = nullptr;
	AcceptDialog *accept = nullptr;
	AcceptDialog *texture_node_type_selector = nullptr;
	Label *label = nullptr;
	Label *label_desc = nullptr;
	Ref<ButtonGroup> button_group;

	void _on_mouse_exit();
	void _on_select_texture_node_type(Object *selected);
	void _on_change_type_confirmed();
	void _on_change_type_closed();

	void _create_preview(const Vector<String> &files) const;
	void _remove_preview();

	bool _cyclical_dependency_exists(const String &p_target_scene_path, Node *p_desired_node) const;
	bool _is_any_texture_selected() const;
	void _create_texture_node(Node *p_parent, Node *p_child, const String &p_path, const Point2 &p_point);
	void _create_audio_node(Node *p_parent, const String &p_path, const Point2 &p_point);
	bool _create_instance(Node *p_parent, const String &p_path, const Point2 &p_point);
	void _perform_drop_data();
	void _show_texture_node_type_selector();
	void _update_theme();

protected:
	void _notification(int p_what);

public:
	virtual bool can_drop_data(const Point2 &p_point, const Variant &p_data) const override;
	virtual void drop_data(const Point2 &p_point, const Variant &p_data) override;

	CanvasItemEditorViewport(CanvasItemEditor *p_canvas_item_editor);
	~CanvasItemEditorViewport();
};

/**************************************************************************/
/*  path_2d_editor_plugin.h                                               */
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

#ifndef PATH_2D_EDITOR_PLUGIN_H
#define PATH_2D_EDITOR_PLUGIN_H

#include "editor/plugins/editor_plugin.h"
#include "scene/2d/path_2d.h"
#include "scene/gui/box_container.h"

class CanvasItemEditor;
class ConfirmationDialog;
class MenuButton;

class Path2DEditor : public HBoxContainer {
	GDCLASS(Path2DEditor, HBoxContainer);

	friend class Path2DEditorPlugin;

	CanvasItemEditor *canvas_item_editor = nullptr;
	Panel *panel = nullptr;
	Path2D *node = nullptr;

	enum Mode {
		MODE_CREATE,
		MODE_EDIT,
		MODE_EDIT_CURVE,
		MODE_DELETE,
		MODE_CLOSE,
		MODE_CLEAR_POINTS,
	};

	Mode mode = MODE_EDIT;
	HBoxContainer *toolbar = nullptr;
	Button *curve_clear_points = nullptr;
	Button *curve_close = nullptr;
	Button *curve_create = nullptr;
	Button *curve_del = nullptr;
	Button *curve_edit = nullptr;
	Button *curve_edit_curve = nullptr;
	MenuButton *handle_menu = nullptr;

	Button *create_curve_button = nullptr;
	ConfirmationDialog *clear_points_dialog = nullptr;

	bool mirror_handle_angle = true;
	bool mirror_handle_length = true;
	bool on_edge = false;

	enum HandleOption {
		HANDLE_OPTION_ANGLE,
		HANDLE_OPTION_LENGTH,
	};

	enum Action {
		ACTION_NONE,
		ACTION_MOVING_POINT,
		ACTION_MOVING_NEW_POINT,
		ACTION_MOVING_NEW_POINT_FROM_SPLIT,
		ACTION_MOVING_IN,
		ACTION_MOVING_OUT,
	};

	Action action = ACTION_NONE;
	int action_point = 0;
	Point2 moving_from;
	Point2 moving_screen_from;
	float orig_in_length = 0.0f;
	float orig_out_length = 0.0f;
	Vector2 edge_point;
	Vector2 original_mouse_pos;

	void _mode_selected(int p_mode);
	void _handle_option_pressed(int p_option);
	void _cancel_current_action();

	void _node_visibility_changed();
	void _update_toolbar();

	void _create_curve();
	void _confirm_clear_points();
	void _clear_curve_points(Path2D *p_path2d);
	void _restore_curve_points(Path2D *p_path2d, const PackedVector2Array &p_points);

protected:
	void _notification(int p_what);
	void _node_removed(Node *p_node);
	static void _bind_methods();

public:
	bool forward_gui_input(const Ref<InputEvent> &p_event);
	void forward_canvas_draw_over_viewport(Control *p_overlay);
	void edit(Node *p_path2d);
	Path2DEditor();
};

class Path2DEditorPlugin : public EditorPlugin {
	GDCLASS(Path2DEditorPlugin, EditorPlugin);

	Path2DEditor *path2d_editor = nullptr;

public:
	virtual bool forward_canvas_gui_input(const Ref<InputEvent> &p_event) override { return path2d_editor->forward_gui_input(p_event); }
	virtual void forward_canvas_draw_over_viewport(Control *p_overlay) override { path2d_editor->forward_canvas_draw_over_viewport(p_overlay); }

	virtual String get_plugin_name() const override { return "Path2D"; }
	bool has_main_screen() const override { return false; }
	virtual void edit(Object *p_object) override;
	virtual bool handles(Object *p_object) const override;
	virtual void make_visible(bool p_visible) override;

	Path2DEditorPlugin();
	~Path2DEditorPlugin();
};

#endif // PATH_2D_EDITOR_PLUGIN_H

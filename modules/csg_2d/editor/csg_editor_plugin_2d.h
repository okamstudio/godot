/**************************************************************************/
/*  csg_editor_plugin_2d.h                                                */
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

#ifndef CSG_EDITOR_PLUGIN_2D_H
#define CSG_EDITOR_PLUGIN_2D_H

#ifdef TOOLS_ENABLED

#include "../csg_shape_2d.h"

#include "editor/plugins/abstract_polygon_2d_editor.h"
#include "editor/plugins/editor_plugin.h"
#include "scene/gui/control.h"

class AcceptDialog;
class CanvasItemEditor;
class CSGPolygon2D;
class MenuButton;

class CSGPolygon2DEditor : public AbstractPolygon2DEditor {
	GDCLASS(CSGPolygon2DEditor, AbstractPolygon2DEditor);

	CSGPolygon2D *node = nullptr;

protected:
	virtual Node2D *_get_node() const override;
	virtual void _set_node(Node *p_polygon) override;

	virtual Variant _get_polygon(int p_idx) const override;
	virtual void _set_polygon(int p_idx, const Variant &p_polygon) const override;

	virtual void _action_add_polygon(const Variant &p_polygon) override;
	virtual void _action_remove_polygon(int p_idx) override;
	virtual void _action_set_polygon(int p_idx, const Variant &p_previous, const Variant &p_polygon) override;

public:
	CSGPolygon2DEditor();
};

class CSGShape2DEditor : public Control {
	GDCLASS(CSGShape2DEditor, Control);

	enum CSGShapeType {
		NONE,
		CAPSULE_SHAPE,
		CIRCLE_SHAPE,
		MESH_SHAPE,
		POLYGON_SHAPE,
		RECTANGLE_SHAPE,
	};

	enum Menu {
		MENU_OPTION_BAKE_MESH_INSTANCE,
		MENU_OPTION_BAKE_COLLISION_SHAPE,
		MENU_OPTION_BAKE_POLYGON_2D,
		MENU_OPTION_BAKE_LIGHT_OCCLUDER_2D,
		MENU_OPTION_BAKE_NAVIGATION_REGION_2D,
	};

	const Point2 RECT_HANDLES[8] = {
		Point2(1, 0),
		Point2(1, 1),
		Point2(0, 1),
		Point2(-1, 1),
		Point2(-1, 0),
		Point2(-1, -1),
		Point2(0, -1),
		Point2(1, -1),
	};

	CSGPolygon2DEditor *polygon_editor = nullptr;
	CanvasItemEditor *canvas_item_editor = nullptr;
	CSGShape2D *node = nullptr;
	HBoxContainer *toolbar = nullptr;
	MenuButton *options = nullptr;
	AcceptDialog *err_dialog = nullptr;

	LocalVector<Point2> handles;

	CSGShapeType shape_type = CSGShapeType::NONE;
	int edit_handle = -1;
	bool pressed = false;
	real_t grab_threshold = 8;
	Variant original;
	Transform2D original_transform;
	Vector2 original_point;
	Point2 last_point;
	Vector2 original_mouse_pos;

	Variant get_handle_value(int idx) const;
	void set_handle(int idx, Point2 &p_point);
	void commit_handle(int idx, Variant &p_org);

	void _menu_option(int p_option);

	void _create_baked_mesh_instance();
	void _create_baked_collision_shape();
	void _create_baked_polygon_2d();
	void _create_baked_light_occluder_2d();
	void _create_baked_navigation_region_2d();

protected:
	void _node_removed(Node *p_node);
	void _notification(int p_what);

public:
	bool forward_canvas_gui_input(const Ref<InputEvent> &p_event);
	void forward_canvas_draw_over_viewport(Control *p_overlay);
	void edit(CSGShape2D *p_csg_shape);
	CSGShape2DEditor();
};

class EditorPluginCSG2D : public EditorPlugin {
	GDCLASS(EditorPluginCSG2D, EditorPlugin);

	CSGShape2DEditor *csg_shape_editor = nullptr;

public:
	virtual bool forward_canvas_gui_input(const Ref<InputEvent> &p_event) override { return csg_shape_editor->forward_canvas_gui_input(p_event); }
	virtual void forward_canvas_draw_over_viewport(Control *p_overlay) override { csg_shape_editor->forward_canvas_draw_over_viewport(p_overlay); }

	virtual String get_plugin_name() const override { return "CSGShape2D"; }
	bool has_main_screen() const override { return false; }
	virtual void edit(Object *p_object) override;
	virtual bool handles(Object *p_object) const override;
	virtual void make_visible(bool p_visible) override;

	EditorPluginCSG2D();
};

#endif // TOOLS_ENABLED

#endif // CSG_EDITOR_PLUGIN_2D_H

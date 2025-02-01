/**************************************************************************/
/*  collision_shape_2d_editor_plugin.h                                    */
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
#include "scene/2d/physics/collision_shape_2d.h"

class CanvasItemEditor;

class CollisionShape2DEditor : public Control {
	GDCLASS(CollisionShape2DEditor, Control);

	enum ShapeType {
		CAPSULE_SHAPE,
		CIRCLE_SHAPE,
		CONCAVE_POLYGON_SHAPE,
		CONVEX_POLYGON_SHAPE,
		WORLD_BOUNDARY_SHAPE,
		SEPARATION_RAY_SHAPE,
		RECTANGLE_SHAPE,
		SEGMENT_SHAPE
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

	CanvasItemEditor *canvas_item_editor = nullptr;
	CollisionShape2D *node = nullptr;

	Vector<Point2> handles;

	int shape_type = -1;
	int edit_handle = -1;
	bool pressed = false;
	real_t grab_threshold = 8;
	Variant original;
	Transform2D original_transform;
	Vector2 original_point;
	Point2 last_point;
	Vector2 original_mouse_pos;

	Ref<Shape2D> current_shape;

	Variant get_handle_value(int idx) const;
	void set_handle(int idx, Point2 &p_point);
	void commit_handle(int idx, Variant &p_org);

	void _shape_changed();

protected:
	void _notification(int p_what);
	void _node_removed(Node *p_node);

public:
	bool forward_canvas_gui_input(const Ref<InputEvent> &p_event);
	void forward_canvas_draw_over_viewport(Control *p_overlay);
	void edit(Node *p_node);

	CollisionShape2DEditor();
};

class CollisionShape2DEditorPlugin : public EditorPlugin {
	GDCLASS(CollisionShape2DEditorPlugin, EditorPlugin);

	CollisionShape2DEditor *collision_shape_2d_editor = nullptr;

public:
	virtual bool forward_canvas_gui_input(const Ref<InputEvent> &p_event) override { return collision_shape_2d_editor->forward_canvas_gui_input(p_event); }
	virtual void forward_canvas_draw_over_viewport(Control *p_overlay) override { collision_shape_2d_editor->forward_canvas_draw_over_viewport(p_overlay); }

	virtual String get_plugin_name() const override { return "CollisionShape2D"; }
	bool has_main_screen() const override { return false; }
	virtual void edit(Object *p_obj) override;
	virtual bool handles(Object *p_obj) const override;
	virtual void make_visible(bool visible) override;

	CollisionShape2DEditorPlugin();
	~CollisionShape2DEditorPlugin();
};

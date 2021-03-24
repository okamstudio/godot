/**************************************************************************/
/*  cast_2d_editor_plugin.h                                               */
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

#ifndef CAST_2D_EDITOR_PLUGIN_H
#define CAST_2D_EDITOR_PLUGIN_H

#include "editor/plugins/editor_plugin.h"
#include "scene/2d/node_2d.h"

class CanvasItemEditor;

class Cast2DEditor : public Control {
	GDCLASS(Cast2DEditor, Control);

	CanvasItemEditor *canvas_item_editor = nullptr;
	Node2D *node = nullptr;

	bool pressed = false;
	Point2 original_target_position;
	Vector2 original_mouse_pos;

protected:
	void _notification(int p_what);
	void _node_removed(Node *p_node);

public:
	bool forward_canvas_gui_input(const Ref<InputEvent> &p_event);
	void forward_canvas_draw_over_viewport(Control *p_overlay);
	void edit(Node2D *p_node);
};

class Cast2DEditorPlugin : public EditorPlugin {
	GDCLASS(Cast2DEditorPlugin, EditorPlugin);

	Cast2DEditor *cast_2d_editor = nullptr;

public:
	virtual bool forward_canvas_gui_input(const Ref<InputEvent> &p_event) override { return cast_2d_editor->forward_canvas_gui_input(p_event); }
	virtual void forward_canvas_draw_over_viewport(Control *p_overlay) override { cast_2d_editor->forward_canvas_draw_over_viewport(p_overlay); }

	virtual String get_plugin_name() const override { return "Cast2D"; }
	bool has_main_screen() const override { return false; }
	virtual void edit(Object *p_object) override;
	virtual bool handles(Object *p_object) const override;
	virtual void make_visible(bool visible) override;

	Cast2DEditorPlugin();
};

#endif // CAST_2D_EDITOR_PLUGIN_H

/**************************************************************************/
/*  cast_2d_editor_plugin.cpp                                             */
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

#include "cast_2d_editor_plugin.h"

#include "canvas_item_editor_plugin.h"
#include "editor/editor_node.h"
#include "editor/editor_undo_redo_manager.h"
#include "scene/2d/physics/ray_cast_2d.h"
#include "scene/2d/physics/shape_cast_2d.h"
#include "scene/main/viewport.h"

void Cast2DEditor::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			get_tree()->connect("node_removed", callable_mp(this, &Cast2DEditor::_node_removed));
		} break;

		case NOTIFICATION_EXIT_TREE: {
			get_tree()->disconnect("node_removed", callable_mp(this, &Cast2DEditor::_node_removed));
		} break;
	}
}

void Cast2DEditor::_node_removed(Node *p_node) {
	if (p_node == node) {
		node = nullptr;
	}
}

bool Cast2DEditor::forward_canvas_gui_input(const Ref<InputEvent> &p_event) {
	if (!node || !node->is_visible_in_tree()) {
		return false;
	}

	Viewport *vp = node->get_viewport();
	if (vp && !vp->is_visible_subviewport()) {
		return false;
	}

	Transform2D xform = canvas_item_editor->get_canvas_transform() * node->get_screen_transform();

	Ref<InputEventMouseButton> mb = p_event;
	if (mb.is_valid() && mb->get_button_index() == MouseButton::LEFT) {
		Vector2 target_position = node->get("target_position");

		Vector2 gpoint = mb->get_position();

		if (mb->is_pressed()) {
			if (xform.xform(target_position).distance_to(gpoint) < 8) {
				pressed = true;
				original_target_position = target_position;
				original_mouse_pos = gpoint;

				return true;
			} else {
				pressed = false;

				return false;
			}
		} else if (pressed) {
			if (original_mouse_pos != gpoint) {
				EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
				undo_redo->create_action(TTR("Set Target Position"));
				undo_redo->add_do_property(node, "target_position", target_position);
				undo_redo->add_do_method(canvas_item_editor, "update_viewport");
				undo_redo->add_undo_property(node, "target_position", original_target_position);
				undo_redo->add_undo_method(canvas_item_editor, "update_viewport");
				undo_redo->commit_action();
			}

			pressed = false;
			return true;
		}
	}

	Ref<InputEventMouseMotion> mm = p_event;
	if (mm.is_valid() && pressed) {
		Vector2 point = canvas_item_editor->snap_point(canvas_item_editor->get_canvas_transform().affine_inverse().xform(mm->get_position()));
		point = node->get_screen_transform().affine_inverse().xform(point);

		node->set("target_position", point);
		canvas_item_editor->update_viewport();

		return true;
	}

	return false;
}

void Cast2DEditor::forward_canvas_draw_over_viewport(Control *p_overlay) {
	if (!node || !node->is_visible_in_tree()) {
		return;
	}

	Viewport *vp = node->get_viewport();
	if (vp && !vp->is_visible_subviewport()) {
		return;
	}

	Transform2D gt = canvas_item_editor->get_canvas_transform() * node->get_screen_transform();

	const Ref<Texture2D> handle = get_editor_theme_icon(SNAME("EditorHandle"));
	p_overlay->draw_texture(handle, gt.xform((Vector2)node->get("target_position")) - handle->get_size() / 2);
}

void Cast2DEditor::edit(Node2D *p_node) {
	if (!canvas_item_editor) {
		canvas_item_editor = CanvasItemEditor::get_singleton();
	}

	if (Object::cast_to<RayCast2D>(p_node) || Object::cast_to<ShapeCast2D>(p_node)) {
		node = p_node;
	} else {
		node = nullptr;
	}

	canvas_item_editor->update_viewport();
}

///////////////////////

void Cast2DEditorPlugin::edit(Object *p_object) {
	cast_2d_editor->edit(Object::cast_to<Node2D>(p_object));
}

bool Cast2DEditorPlugin::handles(Object *p_object) const {
	return Object::cast_to<RayCast2D>(p_object) != nullptr || Object::cast_to<ShapeCast2D>(p_object) != nullptr;
}

void Cast2DEditorPlugin::make_visible(bool p_visible) {
	if (!p_visible) {
		edit(nullptr);
	}
}

Cast2DEditorPlugin::Cast2DEditorPlugin() {
	cast_2d_editor = memnew(Cast2DEditor);
	EditorNode::get_singleton()->get_gui_base()->add_child(cast_2d_editor);
}

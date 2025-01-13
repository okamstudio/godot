/**************************************************************************/
/*  navigation_link_2d_editor_plugin.cpp                                  */
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

#include "navigation_link_2d_editor_plugin.h"

#include "canvas_item_editor_plugin.h"
#include "editor/editor_node.h"
#include "editor/editor_settings.h"
#include "editor/editor_undo_redo_manager.h"
#include "scene/main/viewport.h"
#include "servers/navigation_server_3d.h"

void NavigationLink2DEditor::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			get_tree()->connect("node_removed", callable_mp(this, &NavigationLink2DEditor::_node_removed));
		} break;

		case NOTIFICATION_EXIT_TREE: {
			get_tree()->disconnect("node_removed", callable_mp(this, &NavigationLink2DEditor::_node_removed));
		} break;
	}
}

void NavigationLink2DEditor::_node_removed(Node *p_node) {
	if (p_node == node) {
		node = nullptr;
	}
}

bool NavigationLink2DEditor::forward_canvas_gui_input(const Ref<InputEvent> &p_event) {
	if (!node || !node->is_visible_in_tree()) {
		return false;
	}

	Viewport *vp = node->get_viewport();
	if (vp && !vp->is_visible_subviewport()) {
		return false;
	}

	real_t grab_threshold = EDITOR_GET("editors/polygon_editor/point_grab_radius");
	Transform2D xform = canvas_item_editor->get_canvas_transform() * node->get_screen_transform();

	Ref<InputEventMouseButton> mb = p_event;
	if (mb.is_valid() && mb->get_button_index() == MouseButton::LEFT) {
		if (mb->is_pressed()) {
			// Start position
			if (xform.xform(node->get_start_position()).distance_to(mb->get_position()) < grab_threshold) {
				start_grabbed = true;
				original_start_position = node->get_start_position();

				return true;
			} else {
				start_grabbed = false;
			}

			// End position
			if (xform.xform(node->get_end_position()).distance_to(mb->get_position()) < grab_threshold) {
				end_grabbed = true;
				original_end_position = node->get_end_position();

				return true;
			} else {
				end_grabbed = false;
			}
		} else {
			EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
			if (start_grabbed) {
				undo_redo->create_action(TTR("Set start_position"));
				undo_redo->add_do_method(node, "set_start_position", node->get_start_position());
				undo_redo->add_do_method(canvas_item_editor, "update_viewport");
				undo_redo->add_undo_method(node, "set_start_position", original_start_position);
				undo_redo->add_undo_method(canvas_item_editor, "update_viewport");
				undo_redo->commit_action();

				start_grabbed = false;

				return true;
			}

			if (end_grabbed) {
				undo_redo->create_action(TTR("Set end_position"));
				undo_redo->add_do_method(node, "set_end_position", node->get_end_position());
				undo_redo->add_do_method(canvas_item_editor, "update_viewport");
				undo_redo->add_undo_method(node, "set_end_position", original_end_position);
				undo_redo->add_undo_method(canvas_item_editor, "update_viewport");
				undo_redo->commit_action();

				end_grabbed = false;

				return true;
			}
		}
	}

	Ref<InputEventMouseMotion> mm = p_event;
	if (mm.is_valid()) {
		Vector2 point = canvas_item_editor->snap_point(canvas_item_editor->get_canvas_transform().affine_inverse().xform(mm->get_position()));
		point = node->get_screen_transform().affine_inverse().xform(point);

		if (start_grabbed) {
			node->set_start_position(point);
			canvas_item_editor->update_viewport();

			return true;
		}

		if (end_grabbed) {
			node->set_end_position(point);
			canvas_item_editor->update_viewport();

			return true;
		}
	}

	return false;
}

void NavigationLink2DEditor::forward_canvas_draw_over_viewport(Control *p_overlay) {
	if (!node || !node->is_visible_in_tree()) {
		return;
	}

	Viewport *vp = node->get_viewport();
	if (vp && !vp->is_visible_subviewport()) {
		return;
	}

	Transform2D gt = canvas_item_editor->get_canvas_transform() * node->get_screen_transform();
	Vector2 global_start_position = gt.xform(node->get_start_position());
	Vector2 global_end_position = gt.xform(node->get_end_position());

	// Only drawing the handles here, since the debug rendering will fill in the rest.
	const Ref<Texture2D> handle = get_editor_theme_icon(SNAME("EditorHandle"));
	p_overlay->draw_texture(handle, global_start_position - handle->get_size() / 2);
	p_overlay->draw_texture(handle, global_end_position - handle->get_size() / 2);
}

void NavigationLink2DEditor::edit(NavigationLink2D *p_node) {
	if (!canvas_item_editor) {
		canvas_item_editor = CanvasItemEditor::get_singleton();
	}

	if (p_node) {
		node = p_node;
	} else {
		node = nullptr;
	}

	canvas_item_editor->update_viewport();
}

///////////////////////

void NavigationLink2DEditorPlugin::edit(Object *p_object) {
	editor->edit(Object::cast_to<NavigationLink2D>(p_object));
}

bool NavigationLink2DEditorPlugin::handles(Object *p_object) const {
	return Object::cast_to<NavigationLink2D>(p_object) != nullptr;
}

void NavigationLink2DEditorPlugin::make_visible(bool p_visible) {
	if (!p_visible) {
		edit(nullptr);
	}
}

NavigationLink2DEditorPlugin::NavigationLink2DEditorPlugin() {
	editor = memnew(NavigationLink2DEditor);
	EditorNode::get_singleton()->get_gui_base()->add_child(editor);
}

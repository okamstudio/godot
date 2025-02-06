/**************************************************************************/
/*  area_2d_editor_plugin.cpp                                             */
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

#include "area_2d_editor_plugin.h"

#include "editor/editor_interface.h"
#include "editor/editor_settings.h"
#include "editor/editor_undo_redo_manager.h"
#include "editor/inspector_dock.h"
#include "editor/plugins/canvas_item_editor_plugin.h"
#include "scene/2d/area_2d.h"

bool Area2DEditorPlugin::forward_canvas_gui_input(const Ref<InputEvent> &p_event) {
	if (!node || !node->is_visible_in_tree() || !node->is_gravity_a_point() || node->get_gravity_space_override_mode() == Area2D::SpaceOverride::SPACE_OVERRIDE_DISABLED) {
		return false;
	}

	Ref<InputEventMouseButton> mb = p_event;
	if (mb.is_valid()) {
		if (action == Action::ACTION_MOVING_POINT && mb->get_button_index() == MouseButton::RIGHT) {
			return true;
		}

		Transform2D xform = canvas_item_editor->get_canvas_transform() * node->get_global_transform();

		Vector2 mpos = mb->get_position();
		Vector2 point = xform.xform(node->get_gravity_point_center());
		Vector2 mposxd = xform.affine_inverse().xform(mpos);

		real_t grab_threshold = EDITOR_GET("editors/polygon_editor/point_grab_radius");
		grab_threshold = grab_threshold * grab_threshold;

		// Check for gravity point movement start.
		if (action == ACTION_NONE && mb->is_pressed() && mb->get_button_index() == MouseButton::LEFT) {
			real_t dist_to_p = mpos.distance_squared_to(point);
			if (dist_to_p < grab_threshold) {
				action = ACTION_MOVING_POINT;
				moving_from = node->get_gravity_point_center();
				moving_mouse_from = mposxd;

				canvas_item_editor->update_viewport();
				return true;
			}
		}

		// Check for gravity point movement completion.
		if (action == ACTION_MOVING_POINT && !mb->is_pressed() && mb->get_button_index() == MouseButton::LEFT) {
			EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
			Vector2 new_pos = moving_from + mposxd - moving_mouse_from;

			undo_redo->create_action(TTR("Move Gravity Point Center"));
			undo_redo->add_do_method(node, "set_gravity_point_center", new_pos);
			undo_redo->add_undo_method(node, "set_gravity_point_center", moving_from);
			undo_redo->add_do_method(canvas_item_editor, "update_viewport");
			undo_redo->add_undo_method(canvas_item_editor, "update_viewport");
			undo_redo->commit_action();

			action = ACTION_NONE;
			return true;
		}
	}

	// Handle movement.
	Ref<InputEventMouseMotion> mm = p_event;
	if (mm.is_valid() && action == ACTION_MOVING_POINT) {
		Transform2D xform = canvas_item_editor->get_canvas_transform() * node->get_global_transform();
		Vector2 new_pos = moving_from + xform.affine_inverse().xform(mm->get_position()) - moving_mouse_from;

		node->set_gravity_point_center(new_pos);

		canvas_item_editor->update_viewport();
		return true;
	}

	return false;
}

void Area2DEditorPlugin::forward_canvas_draw_over_viewport(Control *p_overlay) {
	if (!node || !node->is_visible_in_tree() || !node->is_gravity_a_point() || node->get_gravity_space_override_mode() == Area2D::SpaceOverride::SPACE_OVERRIDE_DISABLED) {
		return;
	}

	Transform2D xform = canvas_item_editor->get_canvas_transform() * node->get_global_transform();
	const Ref<Texture2D> gravity_point_handle = canvas_item_editor->get_editor_theme_icon(SNAME("EditorHandle"));
	const Size2 handle_size = gravity_point_handle->get_size();
	Vector2 point = xform.xform(node->get_gravity_point_center());

	if (node->get_gravity_point_unit_distance() > 0) {
		float gdist = xform.get_scale().x * node->get_gravity_point_unit_distance();
		p_overlay->draw_circle(point, gdist, Color(0.92f, 1.0f, 0.6f, 0.3f));
	} else {
		p_overlay->draw_circle(point, 20.0f, Color(0.5f, 0.5f, 0.5f, 0.3f));
	}

	p_overlay->draw_texture_rect(gravity_point_handle, Rect2(point - handle_size * 0.5, handle_size));
}

void Area2DEditorPlugin::_property_edited(const StringName &p_prop) {
	if (((String)p_prop).begins_with("gravity_")) {
		canvas_item_editor->update_viewport();
	}
}

void Area2DEditorPlugin::edit(Object *p_object) {
	if (!canvas_item_editor) {
		canvas_item_editor = CanvasItemEditor::get_singleton();
	}
	action = Action::ACTION_NONE;

	node = Object::cast_to<Area2D>(p_object);
	EditorInspector *insp = get_editor_interface()->get_inspector();
	const Callable property_edited_callable = callable_mp(this, &Area2DEditorPlugin::_property_edited);

	if (node) {
		if (!insp->is_connected("property_edited", property_edited_callable)) {
			insp->connect("property_edited", property_edited_callable);
		}
		canvas_item_editor->update_viewport();
	} else {
		if (insp->is_connected("property_edited", property_edited_callable)) {
			insp->disconnect("property_edited", property_edited_callable);
		}
		canvas_item_editor->update_viewport();
	}
}

void Area2DEditorPlugin::make_visible(bool p_visible) {
	if (!p_visible) {
		edit(nullptr);
	}
}

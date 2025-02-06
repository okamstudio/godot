/**************************************************************************/
/*  camera_2d_editor_plugin.cpp                                           */
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

#include "camera_2d_editor_plugin.h"

#include "canvas_item_editor_plugin.h"
#include "core/config/project_settings.h"
#include "editor/editor_node.h"
#include "editor/themes/editor_scale.h"
#include "scene/2d/camera_2d.h"
#include "scene/gui/label.h"
#include "scene/gui/menu_button.h"

void Camera2DEditor::edit(Camera2D *p_camera) {
	if (p_camera == selected_camera) {
		return;
	}
	selected_camera = p_camera;
}

void Camera2DEditor::_menu_option(int p_option) {
	switch (p_option) {
		case MENU_SNAP_LIMITS_TO_VIEWPORT: {
			selected_camera->set_limit(SIDE_LEFT, 0);
			selected_camera->set_limit(SIDE_TOP, 0);
			selected_camera->set_limit(SIDE_RIGHT, GLOBAL_GET("display/window/size/viewport_width"));
			selected_camera->set_limit(SIDE_BOTTOM, GLOBAL_GET("display/window/size/viewport_height"));
		} break;
	}
}

void Camera2DEditor::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_THEME_CHANGED: {
			options->set_button_icon(get_editor_theme_icon(SNAME("Camera2D")));
		} break;
	}
}

Camera2DEditor::Camera2DEditor() {
	options = memnew(MenuButton);

	CanvasItemEditor::get_singleton()->add_control_to_menu_panel(options);

	options->set_text(TTRC("Camera2D"));

	options->get_popup()->add_item(TTRC("Snap the Limits to the Viewport"), MENU_SNAP_LIMITS_TO_VIEWPORT);
	options->set_switch_on_hover(true);

	options->get_popup()->connect(SceneStringName(id_pressed), callable_mp(this, &Camera2DEditor::_menu_option));

#ifdef TOOLS_ENABLED
	add_user_signal(MethodInfo("_editor_theme_changed"));
#endif
}

void Camera2DEditorPlugin::_update_approach_text_visibility() {
	if (camera_2d_editor->selected_camera == nullptr) {
		return;
	}
	approach_to_move_rect->set_visible(camera_2d_editor->selected_camera->is_limit_enabled());
}

void Camera2DEditorPlugin::_editor_theme_changed() {
	approach_to_move_rect->remove_theme_color_override(SceneStringName(font_color));
	approach_to_move_rect->add_theme_color_override(SceneStringName(font_color), Color(0.6f, 0.6f, 0.6f, 1));
	approach_to_move_rect->add_theme_color_override("font_shadow_color", Color(0.2f, 0.2f, 0.2f, 1));
	approach_to_move_rect->add_theme_constant_override("shadow_outline_size", 1 * EDSCALE);
	approach_to_move_rect->add_theme_constant_override("line_spacing", 0);
}

void Camera2DEditorPlugin::edit(Object *p_object) {
	Callable update_text = callable_mp(this, &Camera2DEditorPlugin::_update_approach_text_visibility);
	StringName update_signal = SNAME("_camera_limit_enabled_updated");
	Camera2D *prev_cam = camera_2d_editor->selected_camera;
	if (prev_cam != nullptr && prev_cam->is_connected(update_signal, update_text)) {
		prev_cam->disconnect(update_signal, update_text);
	}
	Camera2D *cam = Object::cast_to<Camera2D>(p_object);
	if (cam != nullptr) {
		camera_2d_editor->edit(cam);
		_update_approach_text_visibility();

		if (!cam->is_connected(update_signal, update_text)) {
			cam->connect(update_signal, update_text);
		}
	}
}

bool Camera2DEditorPlugin::handles(Object *p_object) const {
	return p_object->is_class("Camera2D");
}

void Camera2DEditorPlugin::make_visible(bool p_visible) {
	if (p_visible) {
		camera_2d_editor->options->show();
		approach_to_move_rect->show();
	} else {
		camera_2d_editor->options->hide();
		approach_to_move_rect->hide();
	}
}

Camera2DEditorPlugin::Camera2DEditorPlugin() {
	camera_2d_editor = memnew(Camera2DEditor);
	EditorNode::get_singleton()->get_gui_base()->add_child(camera_2d_editor);
#ifdef TOOLS_ENABLED
	camera_2d_editor->connect(SNAME("_editor_theme_changed"), callable_mp(this, &Camera2DEditorPlugin::_editor_theme_changed));
#endif

	approach_to_move_rect = memnew(Label);
	approach_to_move_rect->set_text(TTRC("In Move Mode: \nHold Ctrl + left mouse button to move the limit rectangle.\nHold left mouse button to move the camera only."));
	approach_to_move_rect->hide();
	_editor_theme_changed();
	CanvasItemEditor::get_singleton()->get_controls_container()->add_child(approach_to_move_rect);

	make_visible(false);
}

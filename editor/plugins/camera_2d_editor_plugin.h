/**************************************************************************/
/*  camera_2d_editor_plugin.h                                             */
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

#ifndef CAMERA_2D_EDITOR_PLUGIN_H
#define CAMERA_2D_EDITOR_PLUGIN_H

#include "editor/plugins/editor_plugin.h"

class Camera2D;
class Label;
class MenuButton;

class Camera2DEditor : public Control {
	GDCLASS(Camera2DEditor, Control);

	enum Menu {
		MENU_SNAP_LIMITS_TO_VIEWPORT,
	};

	Camera2D *selected_camera = nullptr;

	friend class Camera2DEditorPlugin;
	MenuButton *options = nullptr;

	void _menu_option(int p_option);

protected:
	void _notification(int p_what);

public:
	void edit(Camera2D *p_camera);
	Camera2DEditor();
};

class Camera2DEditorPlugin : public EditorPlugin {
	GDCLASS(Camera2DEditorPlugin, EditorPlugin);

	Camera2DEditor *camera_2d_editor = nullptr;

	Label *approach_to_move_rect = nullptr;

	void _editor_theme_changed();
	void _update_approach_text_visibility();

public:
	virtual void edit(Object *p_object) override;
	virtual bool handles(Object *p_object) const override;
	virtual void make_visible(bool p_visible) override;

	Camera2DEditorPlugin();
};

#endif // CAMERA_2D_EDITOR_PLUGIN_H

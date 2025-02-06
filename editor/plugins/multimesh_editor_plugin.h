/**************************************************************************/
/*  multimesh_editor_plugin.h                                             */
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
#include "scene/3d/multimesh_instance_3d.h"
#include "scene/gui/slider.h"
#include "scene/gui/spin_box.h"

class AcceptDialog;
class ConfirmationDialog;
class MenuButton;
class OptionButton;
class SceneTreeDialog;

class MultiMeshEditor : public Control {
	GDCLASS(MultiMeshEditor, Control);

	friend class MultiMeshEditorPlugin;

	AcceptDialog *err_dialog = nullptr;
	MenuButton *options = nullptr;
	MultiMeshInstance3D *_last_pp_node = nullptr;
	bool browsing_source = false;

	Panel *panel = nullptr;
	MultiMeshInstance3D *node = nullptr;

	LineEdit *surface_source = nullptr;
	LineEdit *mesh_source = nullptr;

	SceneTreeDialog *std = nullptr;

	ConfirmationDialog *populate_dialog = nullptr;
	OptionButton *populate_axis = nullptr;
	HSlider *populate_rotate_random = nullptr;
	HSlider *populate_tilt_random = nullptr;
	SpinBox *populate_scale_random = nullptr;
	SpinBox *populate_scale = nullptr;
	SpinBox *populate_amount = nullptr;

	enum Menu {
		MENU_OPTION_POPULATE
	};

	void _browsed(const NodePath &p_path);
	void _menu_option(int);
	void _populate();
	void _browse(bool p_source);

protected:
	void _node_removed(Node *p_node);

public:
	void edit(MultiMeshInstance3D *p_multimesh);
	MultiMeshEditor();
};

class MultiMeshEditorPlugin : public EditorPlugin {
	GDCLASS(MultiMeshEditorPlugin, EditorPlugin);

	MultiMeshEditor *multimesh_editor = nullptr;

public:
	virtual String get_plugin_name() const override { return "MultiMesh"; }
	bool has_main_screen() const override { return false; }
	virtual void edit(Object *p_object) override;
	virtual bool handles(Object *p_object) const override;
	virtual void make_visible(bool p_visible) override;

	MultiMeshEditorPlugin();
	~MultiMeshEditorPlugin();
};

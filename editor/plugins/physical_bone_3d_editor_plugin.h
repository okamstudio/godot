/**************************************************************************/
/*  physical_bone_3d_editor_plugin.h                                      */
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
#include "scene/gui/box_container.h"
#include "scene/gui/button.h"

class PhysicalBone3D;

class PhysicalBone3DEditor : public Object {
	GDCLASS(PhysicalBone3DEditor, Object);

	HBoxContainer *spatial_editor_hb = nullptr;
	Button *button_transform_joint = nullptr;

	PhysicalBone3D *selected = nullptr;

private:
	void _on_toggle_button_transform_joint(bool p_is_pressed);
	void _set_move_joint();

public:
	PhysicalBone3DEditor();
	~PhysicalBone3DEditor() {}

	void set_selected(PhysicalBone3D *p_pb);

	void hide();
	void show();
};

class PhysicalBone3DEditorPlugin : public EditorPlugin {
	GDCLASS(PhysicalBone3DEditorPlugin, EditorPlugin);

	PhysicalBone3D *selected = nullptr;
	PhysicalBone3DEditor physical_bone_editor;

public:
	virtual String get_plugin_name() const override { return "PhysicalBone3D"; }
	virtual bool handles(Object *p_object) const override { return p_object->is_class("PhysicalBone3D"); }
	virtual void make_visible(bool p_visible) override;
	virtual void edit(Object *p_node) override;

	PhysicalBone3DEditorPlugin();
};

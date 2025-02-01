/**************************************************************************/
/*  mesh_editor_plugin.h                                                  */
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

#include "editor/editor_inspector.h"
#include "editor/plugins/editor_plugin.h"
#include "scene/3d/camera_3d.h"
#include "scene/3d/light_3d.h"
#include "scene/3d/mesh_instance_3d.h"
#include "scene/gui/subviewport_container.h"
#include "scene/resources/camera_attributes.h"

class SubViewport;
class Button;

class MeshEditor : public SubViewportContainer {
	GDCLASS(MeshEditor, SubViewportContainer);

	float rot_x;
	float rot_y;

	SubViewport *viewport = nullptr;
	MeshInstance3D *mesh_instance = nullptr;
	Node3D *rotation = nullptr;
	DirectionalLight3D *light1 = nullptr;
	DirectionalLight3D *light2 = nullptr;
	Camera3D *camera = nullptr;
	Ref<CameraAttributesPractical> camera_attributes;

	Ref<Mesh> mesh;

	Button *light_1_switch = nullptr;
	Button *light_2_switch = nullptr;

	struct ThemeCache {
		Ref<Texture2D> light_1_icon;
		Ref<Texture2D> light_2_icon;
	} theme_cache;

	void _on_light_1_switch_pressed();
	void _on_light_2_switch_pressed();
	void _update_rotation();

protected:
	virtual void _update_theme_item_cache() override;
	void _notification(int p_what);
	void gui_input(const Ref<InputEvent> &p_event) override;

public:
	void edit(Ref<Mesh> p_mesh);
	MeshEditor();
};

class EditorInspectorPluginMesh : public EditorInspectorPlugin {
	GDCLASS(EditorInspectorPluginMesh, EditorInspectorPlugin);

public:
	virtual bool can_handle(Object *p_object) override;
	virtual void parse_begin(Object *p_object) override;
};

class MeshEditorPlugin : public EditorPlugin {
	GDCLASS(MeshEditorPlugin, EditorPlugin);

public:
	virtual String get_plugin_name() const override { return "Mesh"; }

	MeshEditorPlugin();
};

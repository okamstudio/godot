/**************************************************************************/
/*  polygon_3d_editor_plugin.h                                            */
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
#include "scene/3d/mesh_instance_3d.h"
#include "scene/3d/physics/collision_polygon_3d.h"
#include "scene/gui/box_container.h"
#include "scene/resources/immediate_mesh.h"

class CanvasItemEditor;
class MenuButton;

class Polygon3DEditor : public HBoxContainer {
	GDCLASS(Polygon3DEditor, HBoxContainer);

	enum Mode {
		MODE_CREATE,
		MODE_EDIT,

	};

	Mode mode;

	Button *button_create = nullptr;
	Button *button_edit = nullptr;

	Ref<StandardMaterial3D> line_material;
	Ref<StandardMaterial3D> handle_material;

	Panel *panel = nullptr;
	Node3D *node = nullptr;
	Ref<Resource> node_resource;
	Ref<ImmediateMesh> imesh;
	MeshInstance3D *imgeom = nullptr;
	MeshInstance3D *pointsm = nullptr;
	Ref<ArrayMesh> m;

	MenuButton *options = nullptr;

	int edited_point = 0;
	Vector2 edited_point_pos;
	PackedVector2Array pre_move_edit;
	PackedVector2Array wip;
	bool wip_active;
	bool snap_ignore;

	float prev_depth = 0.0f;

	void _wip_close();
	void _polygon_draw();
	void _menu_option(int p_option);

	float _get_depth();
	PackedVector2Array _get_polygon();
	void _set_polygon(const PackedVector2Array &p_poly);

protected:
	void _notification(int p_what);
	void _node_removed(Node *p_node);
	static void _bind_methods();

public:
	virtual EditorPlugin::AfterGUIInput forward_3d_gui_input(Camera3D *p_camera, const Ref<InputEvent> &p_event);
	void edit(Node *p_node);
	Polygon3DEditor();
	~Polygon3DEditor();
};

class Polygon3DEditorPlugin : public EditorPlugin {
	GDCLASS(Polygon3DEditorPlugin, EditorPlugin);

	Polygon3DEditor *polygon_editor = nullptr;

public:
	virtual EditorPlugin::AfterGUIInput forward_3d_gui_input(Camera3D *p_camera, const Ref<InputEvent> &p_event) override { return polygon_editor->forward_3d_gui_input(p_camera, p_event); }

	virtual String get_plugin_name() const override { return "Polygon3DEditor"; }
	bool has_main_screen() const override { return false; }
	virtual void edit(Object *p_object) override;
	virtual bool handles(Object *p_object) const override;
	virtual void make_visible(bool p_visible) override;

	Polygon3DEditorPlugin();
	~Polygon3DEditorPlugin();
};

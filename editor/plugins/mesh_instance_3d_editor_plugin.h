/**************************************************************************/
/*  mesh_instance_3d_editor_plugin.h                                      */
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
#include "scene/gui/option_button.h"

class AcceptDialog;
class AspectRatioContainer;
class ConfirmationDialog;
class MenuButton;
class SpinBox;

class MeshInstance3DEditor : public Control {
	GDCLASS(MeshInstance3DEditor, Control);

	enum Menu {
		MENU_OPTION_CREATE_COLLISION_SHAPE,
		MENU_OPTION_CREATE_NAVMESH,
		MENU_OPTION_CREATE_OUTLINE_MESH,
		MENU_OPTION_CREATE_DEBUG_TANGENTS,
		MENU_OPTION_CREATE_UV2,
		MENU_OPTION_DEBUG_UV1,
		MENU_OPTION_DEBUG_UV2,
	};

	enum ShapePlacement {
		SHAPE_PLACEMENT_SIBLING,
		SHAPE_PLACEMENT_STATIC_BODY_CHILD,
	};

	enum ShapeType {
		SHAPE_TYPE_TRIMESH,
		SHAPE_TYPE_SINGLE_CONVEX,
		SHAPE_TYPE_SIMPLIFIED_CONVEX,
		SHAPE_TYPE_MULTIPLE_CONVEX,
	};

	MeshInstance3D *node = nullptr;

	MenuButton *options = nullptr;

	ConfirmationDialog *outline_dialog = nullptr;
	SpinBox *outline_size = nullptr;

	ConfirmationDialog *shape_dialog = nullptr;
	OptionButton *shape_type = nullptr;
	OptionButton *shape_placement = nullptr;

	AcceptDialog *err_dialog = nullptr;

	AcceptDialog *debug_uv_dialog = nullptr;
	AspectRatioContainer *debug_uv_arc = nullptr;
	Control *debug_uv = nullptr;
	Vector<Vector2> uv_lines;

	ConfirmationDialog *navigation_mesh_dialog = nullptr;

	void _create_collision_shape();
	Vector<Ref<Shape3D>> create_shape_from_mesh(Ref<Mesh> p_mesh, int p_option, bool p_verbose);
	void _menu_option(int p_option);
	void _create_outline_mesh();
	void _create_navigation_mesh();

	void _create_uv_lines(int p_layer);
	friend class MeshInstance3DEditorPlugin;

	void _debug_uv_draw();

protected:
	void _node_removed(Node *p_node);

	void _notification(int p_what);

public:
	void edit(MeshInstance3D *p_mesh);
	MeshInstance3DEditor();
};

class MeshInstance3DEditorPlugin : public EditorPlugin {
	GDCLASS(MeshInstance3DEditorPlugin, EditorPlugin);

	MeshInstance3DEditor *mesh_editor = nullptr;

public:
	virtual String get_plugin_name() const override { return "MeshInstance3D"; }
	bool has_main_screen() const override { return false; }
	virtual void edit(Object *p_object) override;
	virtual bool handles(Object *p_object) const override;
	virtual void make_visible(bool p_visible) override;

	MeshInstance3DEditorPlugin();
	~MeshInstance3DEditorPlugin();
};

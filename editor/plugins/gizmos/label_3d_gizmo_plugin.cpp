/**************************************************************************/
/*  label_3d_gizmo_plugin.cpp                                             */
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

#include "label_3d_gizmo_plugin.h"

#include "scene/3d/label_3d.h"

Label3DGizmoPlugin::Label3DGizmoPlugin() {
}

bool Label3DGizmoPlugin::has_gizmo(Node3D *p_spatial) {
	return Object::cast_to<Label3D>(p_spatial) != nullptr;
}

String Label3DGizmoPlugin::get_gizmo_name() const {
	return "Label3D";
}

int Label3DGizmoPlugin::get_priority() const {
	return -1;
}

bool Label3DGizmoPlugin::can_be_hidden() const {
	return false;
}

void Label3DGizmoPlugin::redraw(EditorNode3DGizmo *p_gizmo) {
	Label3D *label = Object::cast_to<Label3D>(p_gizmo->get_node_3d());

	p_gizmo->clear();

	Ref<TriangleMesh> tm = label->generate_triangle_mesh();
	if (tm.is_valid()) {
		p_gizmo->add_collision_triangles(tm);
	}
}

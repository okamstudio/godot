/**************************************************************************/
/*  lightmap_probe_gizmo_plugin.cpp                                       */
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

#include "lightmap_probe_gizmo_plugin.h"

#include "editor/editor_node.h"
#include "editor/editor_settings.h"
#include "editor/editor_string_names.h"
#include "scene/3d/lightmap_probe.h"

LightmapProbeGizmoPlugin::LightmapProbeGizmoPlugin() {
	create_icon_material("lightmap_probe_icon", EditorNode::get_singleton()->get_editor_theme()->get_icon(SNAME("GizmoLightmapProbe"), EditorStringName(EditorIcons)));

	Color gizmo_color = EDITOR_GET("editors/3d_gizmos/gizmo_colors/lightprobe_lines");

	gizmo_color.a = 0.3;
	create_material("lightprobe_lines", gizmo_color);
}

bool LightmapProbeGizmoPlugin::has_gizmo(Node3D *p_spatial) {
	return Object::cast_to<LightmapProbe>(p_spatial) != nullptr;
}

String LightmapProbeGizmoPlugin::get_gizmo_name() const {
	return "LightmapProbe";
}

int LightmapProbeGizmoPlugin::get_priority() const {
	return -1;
}

void LightmapProbeGizmoPlugin::redraw(EditorNode3DGizmo *p_gizmo) {
	Ref<Material> material_lines = get_material("lightprobe_lines", p_gizmo);

	p_gizmo->clear();

	Vector<Vector3> lines;

	int stack_count = 8;
	int sector_count = 16;

	float sector_step = (Math_PI * 2.0) / sector_count;
	float stack_step = Math_PI / stack_count;

	Vector<Vector3> vertices;
	float radius = 0.2;

	for (int i = 0; i <= stack_count; ++i) {
		float stack_angle = Math_PI / 2 - i * stack_step; // starting from pi/2 to -pi/2
		float xy = radius * Math::cos(stack_angle); // r * cos(u)
		float z = radius * Math::sin(stack_angle); // r * sin(u)

		// add (sector_count+1) vertices per stack
		// the first and last vertices have same position and normal, but different tex coords
		for (int j = 0; j <= sector_count; ++j) {
			float sector_angle = j * sector_step; // starting from 0 to 2pi

			// vertex position (x, y, z)
			float x = xy * Math::cos(sector_angle); // r * cos(u) * cos(v)
			float y = xy * Math::sin(sector_angle); // r * cos(u) * sin(v)

			Vector3 n = Vector3(x, z, y);
			vertices.push_back(n);
		}
	}

	for (int i = 0; i < stack_count; ++i) {
		int k1 = i * (sector_count + 1); // beginning of current stack
		int k2 = k1 + sector_count + 1; // beginning of next stack

		for (int j = 0; j < sector_count; ++j, ++k1, ++k2) {
			// 2 triangles per sector excluding first and last stacks
			// k1 => k2 => k1+1
			if (i != 0) {
				lines.push_back(vertices[k1]);
				lines.push_back(vertices[k2]);
				lines.push_back(vertices[k1]);
				lines.push_back(vertices[k1 + 1]);
			}

			if (i != (stack_count - 1)) {
				lines.push_back(vertices[k1 + 1]);
				lines.push_back(vertices[k2]);
				lines.push_back(vertices[k2]);
				lines.push_back(vertices[k2 + 1]);
			}
		}
	}

	const Ref<Material> icon = get_material("lightmap_probe_icon", p_gizmo);

	p_gizmo->add_lines(lines, material_lines);
	p_gizmo->add_unscaled_billboard(icon, 0.05);
}

/**************************************************************************/
/*  root_motion_view.cpp                                                  */
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

#ifndef _3D_DISABLED

#include "root_motion_view.h"

#include "scene/animation/animation_tree.h"
#include "scene/resources/material.h"

void RootMotionView::set_animation_mixer(const NodePath &p_path) {
	path = p_path;
	first = true;
}

NodePath RootMotionView::get_animation_mixer() const {
	return path;
}

void RootMotionView::set_color(const Color &p_color) {
	color = p_color;
	first = true;
}

Color RootMotionView::get_color() const {
	return color;
}

void RootMotionView::set_cell_size(float p_size) {
	cell_size = p_size;
	first = true;
}

float RootMotionView::get_cell_size() const {
	return cell_size;
}

void RootMotionView::set_radius(float p_radius) {
	radius = p_radius;
	first = true;
}

float RootMotionView::get_radius() const {
	return radius;
}

void RootMotionView::set_zero_y(bool p_zero_y) {
	zero_y = p_zero_y;
}

bool RootMotionView::get_zero_y() const {
	return zero_y;
}

void RootMotionView::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			immediate_material = StandardMaterial3D::get_material_for_2d(false, BaseMaterial3D::TRANSPARENCY_ALPHA, false);

			first = true;
		} break;

		case NOTIFICATION_INTERNAL_PROCESS:
		case NOTIFICATION_INTERNAL_PHYSICS_PROCESS: {
			Transform3D transform;
			Basis diff;

			if (has_node(path)) {
				Node *node = get_node(path);
				AnimationMixer *mixer = Object::cast_to<AnimationMixer>(node);
				if (mixer && mixer->is_active() && mixer->get_root_motion_track() != NodePath()) {
					if (is_processing_internal() && mixer->get_callback_mode_process() == AnimationMixer::ANIMATION_CALLBACK_MODE_PROCESS_PHYSICS) {
						set_process_internal(false);
						set_physics_process_internal(true);
					}

					if (is_physics_processing_internal() && mixer->get_callback_mode_process() == AnimationMixer::ANIMATION_CALLBACK_MODE_PROCESS_IDLE) {
						set_process_internal(true);
						set_physics_process_internal(false);
					}

					transform.origin = mixer->get_root_motion_position();
					transform.basis = mixer->get_root_motion_rotation(); // Scale is meaningless.
					diff = mixer->is_root_motion_local() ? Quaternion() : mixer->get_root_motion_rotation_accumulator();
				}
			}
			if (!first && transform == Transform3D()) {
				return;
			}

			first = false;

			accumulated.basis *= transform.basis;
			transform.origin = (diff.inverse() * accumulated.basis).xform(transform.origin);
			accumulated.origin += transform.origin;

			accumulated.origin.x = Math::fposmod(accumulated.origin.x, cell_size);
			if (zero_y) {
				accumulated.origin.y = 0;
			}
			accumulated.origin.z = Math::fposmod(accumulated.origin.z, cell_size);

			immediate->clear_surfaces();

			int cells_in_radius = int((radius / cell_size) + 1.0);

			immediate->surface_begin(Mesh::PRIMITIVE_LINES, immediate_material);

			for (int i = -cells_in_radius; i < cells_in_radius; i++) {
				for (int j = -cells_in_radius; j < cells_in_radius; j++) {
					Vector3 from(i * cell_size, 0, j * cell_size);
					Vector3 from_i((i + 1) * cell_size, 0, j * cell_size);
					Vector3 from_j(i * cell_size, 0, (j + 1) * cell_size);
					from = accumulated.xform_inv(from);
					from_i = accumulated.xform_inv(from_i);
					from_j = accumulated.xform_inv(from_j);

					Color c = color, c_i = color, c_j = color;
					c.a *= MAX(0, 1.0 - from.length() / radius);
					c_i.a *= MAX(0, 1.0 - from_i.length() / radius);
					c_j.a *= MAX(0, 1.0 - from_j.length() / radius);

					immediate->surface_set_color(c);
					immediate->surface_add_vertex(from);

					immediate->surface_set_color(c_i);
					immediate->surface_add_vertex(from_i);

					immediate->surface_set_color(c);
					immediate->surface_add_vertex(from);

					immediate->surface_set_color(c_j);
					immediate->surface_add_vertex(from_j);
				}
			}

			immediate->surface_end();
		} break;
	}
}

AABB RootMotionView::get_aabb() const {
	return AABB(Vector3(-radius, 0, -radius), Vector3(radius * 2, 0.001, radius * 2));
}

void RootMotionView::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_animation_path", "path"), &RootMotionView::set_animation_mixer);
	ClassDB::bind_method(D_METHOD("get_animation_path"), &RootMotionView::get_animation_mixer);

	ClassDB::bind_method(D_METHOD("set_color", "color"), &RootMotionView::set_color);
	ClassDB::bind_method(D_METHOD("get_color"), &RootMotionView::get_color);

	ClassDB::bind_method(D_METHOD("set_cell_size", "size"), &RootMotionView::set_cell_size);
	ClassDB::bind_method(D_METHOD("get_cell_size"), &RootMotionView::get_cell_size);

	ClassDB::bind_method(D_METHOD("set_radius", "size"), &RootMotionView::set_radius);
	ClassDB::bind_method(D_METHOD("get_radius"), &RootMotionView::get_radius);

	ClassDB::bind_method(D_METHOD("set_zero_y", "enable"), &RootMotionView::set_zero_y);
	ClassDB::bind_method(D_METHOD("get_zero_y"), &RootMotionView::get_zero_y);

	ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "animation_path", PROPERTY_HINT_NODE_PATH_VALID_TYPES, "AnimationMixer"), "set_animation_path", "get_animation_path");
	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "color"), "set_color", "get_color");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "cell_size", PROPERTY_HINT_RANGE, "0.1,16,0.01,or_greater,suffix:m"), "set_cell_size", "get_cell_size");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "radius", PROPERTY_HINT_RANGE, "0.1,16,0.01,or_greater,suffix:m"), "set_radius", "get_radius");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "zero_y"), "set_zero_y", "get_zero_y");
}

RootMotionView::RootMotionView() {
	if (Engine::get_singleton()->is_editor_hint()) {
		set_process_internal(true);
	}
	immediate.instantiate();
	set_base(immediate->get_rid());
}

RootMotionView::~RootMotionView() {
	set_base(RID());
}

#endif // _3D_DISABLED

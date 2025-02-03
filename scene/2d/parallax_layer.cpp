/**************************************************************************/
/*  parallax_layer.cpp                                                    */
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

#include "parallax_layer.h"

#include "core/engine.h"
#include "parallax_background.h"

void ParallaxLayer::set_motion_scale(const Size2 &p_scale) {
	motion_scale = p_scale;

	ParallaxBackground *pb = Object::cast_to<ParallaxBackground>(get_parent());
	if (pb && is_inside_tree()) {
		Vector2 ofs = pb->get_final_offset();
		float scale = pb->get_scroll_scale();
		set_base_offset_and_scale(ofs, scale);
	}
}

Size2 ParallaxLayer::get_motion_scale() const {
	return motion_scale;
}

void ParallaxLayer::set_motion_offset(const Size2 &p_offset) {
	motion_offset = p_offset;

	ParallaxBackground *pb = Object::cast_to<ParallaxBackground>(get_parent());
	if (pb && is_inside_tree()) {
		Vector2 ofs = pb->get_final_offset();
		float scale = pb->get_scroll_scale();
		set_base_offset_and_scale(ofs, scale);
	}
}

Size2 ParallaxLayer::get_motion_offset() const {
	return motion_offset;
}

void ParallaxLayer::_update_mirroring() {
	if (!is_inside_tree()) {
		return;
	}

	ParallaxBackground *pb = Object::cast_to<ParallaxBackground>(get_parent());
	if (pb) {
		RID c = pb->get_canvas();
		RID ci = get_canvas_item();
		Point2 mirrorScale = mirroring * get_scale();
		VisualServer::get_singleton()->canvas_set_item_mirroring(c, ci, mirrorScale);
	}
}

void ParallaxLayer::set_mirroring(const Size2 &p_mirroring) {
	mirroring = p_mirroring;
	if (mirroring.x < 0) {
		mirroring.x = 0;
	}
	if (mirroring.y < 0) {
		mirroring.y = 0;
	}

	_update_mirroring();
}

Size2 ParallaxLayer::get_mirroring() const {
	return mirroring;
}

void ParallaxLayer::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			orig_offset = get_position();
			orig_scale = get_scale();
			_update_mirroring();
		} break;
		case NOTIFICATION_EXIT_TREE: {
			set_position(orig_offset);
			set_scale(orig_scale);
		} break;
	}
}

void ParallaxLayer::set_base_offset_and_scale(const Point2 &p_offset, float p_scale) {
	if (!is_inside_tree()) {
		return;
	}
	if (Engine::get_singleton()->is_editor_hint()) {
		return;
	}

	Point2 new_ofs = p_offset * motion_scale + motion_offset * p_scale + orig_offset * p_scale;

	if (mirroring.x) {
		double den = mirroring.x * p_scale;
		new_ofs.x -= den * ceil(new_ofs.x / den);
	}

	if (mirroring.y) {
		double den = mirroring.y * p_scale;
		new_ofs.y -= den * ceil(new_ofs.y / den);
	}

	set_position(new_ofs);
	set_scale(Vector2(1, 1) * p_scale * orig_scale);

	_update_mirroring();
}

String ParallaxLayer::get_configuration_warning() const {
	String warning = Node2D::get_configuration_warning();
	if (!Object::cast_to<ParallaxBackground>(get_parent())) {
		if (warning != String()) {
			warning += "\n\n";
		}
		warning += TTR("ParallaxLayer node only works when set as child of a ParallaxBackground node.");
	}

	return warning;
}

void ParallaxLayer::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_motion_scale", "scale"), &ParallaxLayer::set_motion_scale);
	ClassDB::bind_method(D_METHOD("get_motion_scale"), &ParallaxLayer::get_motion_scale);
	ClassDB::bind_method(D_METHOD("set_motion_offset", "offset"), &ParallaxLayer::set_motion_offset);
	ClassDB::bind_method(D_METHOD("get_motion_offset"), &ParallaxLayer::get_motion_offset);
	ClassDB::bind_method(D_METHOD("set_mirroring", "mirror"), &ParallaxLayer::set_mirroring);
	ClassDB::bind_method(D_METHOD("get_mirroring"), &ParallaxLayer::get_mirroring);

	ADD_GROUP("Motion", "motion_");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "motion_scale", PROPERTY_HINT_LINK), "set_motion_scale", "get_motion_scale");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "motion_offset"), "set_motion_offset", "get_motion_offset");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "motion_mirroring"), "set_mirroring", "get_mirroring");
}

ParallaxLayer::ParallaxLayer() {
	motion_scale = Size2(1, 1);

	// ParallaxLayer is always updated every frame so there is no need to interpolate.
	set_physics_interpolation_mode(Node::PHYSICS_INTERPOLATION_MODE_OFF);
}

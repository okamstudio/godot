/**************************************************************************/
/*  separation_ray_shape_3d.cpp                                           */
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

#include "separation_ray_shape_3d.h"

#include "scene/resources/mesh.h"
#include "servers/physics_server_3d.h"

Vector<Vector3> SeparationRayShape3D::get_debug_mesh_lines() const {
	Vector<Vector3> points = {
		Vector3(),
		Vector3(0, 0, get_length())
	};

	return points;
}

Ref<ArrayMesh> SeparationRayShape3D::get_debug_arraymesh_faces(const Color &p_modulate) const {
	return memnew(ArrayMesh);
}

real_t SeparationRayShape3D::get_enclosing_radius() const {
	return length;
}

void SeparationRayShape3D::_update_shape() {
	Dictionary d;
	d["length"] = length;
	d["slide_on_slope"] = slide_on_slope;
	PhysicsServer3D::get_singleton()->shape_set_data(get_shape(), d);
	Shape3D::_update_shape();
}

void SeparationRayShape3D::set_length(float p_length) {
	length = p_length;
	_update_shape();
	emit_changed();
}

float SeparationRayShape3D::get_length() const {
	return length;
}

void SeparationRayShape3D::set_slide_on_slope(bool p_active) {
	slide_on_slope = p_active;
	_update_shape();
	emit_changed();
}

bool SeparationRayShape3D::get_slide_on_slope() const {
	return slide_on_slope;
}

void SeparationRayShape3D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_length", "length"), &SeparationRayShape3D::set_length);
	ClassDB::bind_method(D_METHOD("get_length"), &SeparationRayShape3D::get_length);

	ClassDB::bind_method(D_METHOD("set_slide_on_slope", "active"), &SeparationRayShape3D::set_slide_on_slope);
	ClassDB::bind_method(D_METHOD("get_slide_on_slope"), &SeparationRayShape3D::get_slide_on_slope);

	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "length", PROPERTY_HINT_RANGE, "0.001,100,0.001,or_greater,suffix:m"), "set_length", "get_length");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "slide_on_slope"), "set_slide_on_slope", "get_slide_on_slope");
}

SeparationRayShape3D::SeparationRayShape3D() :
		Shape3D(PhysicsServer3D::get_singleton()->shape_create(PhysicsServer3D::SHAPE_SEPARATION_RAY)) {
	/* Code copied from setters to prevent the use of uninitialized variables */
	_update_shape();
	emit_changed();
}

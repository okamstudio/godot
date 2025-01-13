/**************************************************************************/
/*  openxr_composition_layer_equirect.cpp                                 */
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

#include "openxr_composition_layer_equirect.h"

#include "../openxr_interface.h"

#include "scene/resources/mesh.h"

OpenXRCompositionLayerEquirect::OpenXRCompositionLayerEquirect() :
		OpenXRCompositionLayer((XrCompositionLayerBaseHeader *)&composition_layer) {
	XRServer::get_singleton()->connect("reference_frame_changed", callable_mp(this, &OpenXRCompositionLayerEquirect::update_transform));
}

OpenXRCompositionLayerEquirect::~OpenXRCompositionLayerEquirect() {
}

void OpenXRCompositionLayerEquirect::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_radius", "radius"), &OpenXRCompositionLayerEquirect::set_radius);
	ClassDB::bind_method(D_METHOD("get_radius"), &OpenXRCompositionLayerEquirect::get_radius);

	ClassDB::bind_method(D_METHOD("set_central_horizontal_angle", "angle"), &OpenXRCompositionLayerEquirect::set_central_horizontal_angle);
	ClassDB::bind_method(D_METHOD("get_central_horizontal_angle"), &OpenXRCompositionLayerEquirect::get_central_horizontal_angle);

	ClassDB::bind_method(D_METHOD("set_upper_vertical_angle", "angle"), &OpenXRCompositionLayerEquirect::set_upper_vertical_angle);
	ClassDB::bind_method(D_METHOD("get_upper_vertical_angle"), &OpenXRCompositionLayerEquirect::get_upper_vertical_angle);

	ClassDB::bind_method(D_METHOD("set_lower_vertical_angle", "angle"), &OpenXRCompositionLayerEquirect::set_lower_vertical_angle);
	ClassDB::bind_method(D_METHOD("get_lower_vertical_angle"), &OpenXRCompositionLayerEquirect::get_lower_vertical_angle);

	ClassDB::bind_method(D_METHOD("set_fallback_segments", "segments"), &OpenXRCompositionLayerEquirect::set_fallback_segments);
	ClassDB::bind_method(D_METHOD("get_fallback_segments"), &OpenXRCompositionLayerEquirect::get_fallback_segments);

	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "radius", PROPERTY_HINT_NONE, ""), "set_radius", "get_radius");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "central_horizontal_angle", PROPERTY_HINT_RANGE, "0,360,0.1,or_less,or_greater,radians_as_degrees"), "set_central_horizontal_angle", "get_central_horizontal_angle");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "upper_vertical_angle", PROPERTY_HINT_RANGE, "0,90,0.1,or_less,or_greater,radians_as_degrees"), "set_upper_vertical_angle", "get_upper_vertical_angle");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "lower_vertical_angle", PROPERTY_HINT_RANGE, "0,90,0.1,or_less,or_greater,radians_as_degrees"), "set_lower_vertical_angle", "get_lower_vertical_angle");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "fallback_segments", PROPERTY_HINT_NONE, ""), "set_fallback_segments", "get_fallback_segments");
}

Ref<Mesh> OpenXRCompositionLayerEquirect::_create_fallback_mesh() {
	Ref<ArrayMesh> mesh;
	mesh.instantiate();

	Array arrays;
	arrays.resize(ArrayMesh::ARRAY_MAX);

	Vector<Vector3> vertices;
	Vector<Vector3> normals;
	Vector<Vector2> uvs;
	Vector<int> indices;

	float step_horizontal = central_horizontal_angle / fallback_segments;
	float step_vertical = (upper_vertical_angle + lower_vertical_angle) / fallback_segments;

	float start_horizontal_angle = Math_PI - (central_horizontal_angle / 2.0);

	for (uint32_t i = 0; i < fallback_segments + 1; i++) {
		for (uint32_t j = 0; j < fallback_segments + 1; j++) {
			float horizontal_angle = start_horizontal_angle + (step_horizontal * i);
			float vertical_angle = -lower_vertical_angle + (step_vertical * j);

			Vector3 vertex(
					radius * Math::cos(vertical_angle) * Math::sin(horizontal_angle),
					radius * Math::sin(vertical_angle),
					radius * Math::cos(vertical_angle) * Math::cos(horizontal_angle));

			vertices.push_back(vertex);
			normals.push_back(vertex.normalized());
			uvs.push_back(Vector2(1.0 - ((float)i / fallback_segments), 1.0 - (float(j) / fallback_segments)));
		}
	}

	for (uint32_t i = 0; i < fallback_segments; i++) {
		for (uint32_t j = 0; j < fallback_segments; j++) {
			uint32_t index = i * (fallback_segments + 1) + j;
			indices.push_back(index);
			indices.push_back(index + fallback_segments + 1);
			indices.push_back(index + fallback_segments + 2);

			indices.push_back(index);
			indices.push_back(index + fallback_segments + 2);
			indices.push_back(index + 1);
		}
	}

	arrays[ArrayMesh::ARRAY_VERTEX] = vertices;
	arrays[ArrayMesh::ARRAY_NORMAL] = normals;
	arrays[ArrayMesh::ARRAY_TEX_UV] = uvs;
	arrays[ArrayMesh::ARRAY_INDEX] = indices;

	mesh->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, arrays);
	return mesh;
}

void OpenXRCompositionLayerEquirect::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_LOCAL_TRANSFORM_CHANGED: {
			update_transform();
		} break;
	}
}

void OpenXRCompositionLayerEquirect::update_transform() {
	composition_layer.pose = get_openxr_pose();
}

void OpenXRCompositionLayerEquirect::set_radius(float p_radius) {
	ERR_FAIL_COND(p_radius <= 0);
	radius = p_radius;
	composition_layer.radius = radius;
	update_fallback_mesh();
}

float OpenXRCompositionLayerEquirect::get_radius() const {
	return radius;
}

void OpenXRCompositionLayerEquirect::set_central_horizontal_angle(float p_angle) {
	ERR_FAIL_COND(p_angle <= 0);
	central_horizontal_angle = p_angle;
	composition_layer.centralHorizontalAngle = central_horizontal_angle;
	update_fallback_mesh();
}

float OpenXRCompositionLayerEquirect::get_central_horizontal_angle() const {
	return central_horizontal_angle;
}

void OpenXRCompositionLayerEquirect::set_upper_vertical_angle(float p_angle) {
	ERR_FAIL_COND(p_angle <= 0 || p_angle > (Math_PI / 2.0));
	upper_vertical_angle = p_angle;
	composition_layer.upperVerticalAngle = p_angle;
	update_fallback_mesh();
}

float OpenXRCompositionLayerEquirect::get_upper_vertical_angle() const {
	return upper_vertical_angle;
}

void OpenXRCompositionLayerEquirect::set_lower_vertical_angle(float p_angle) {
	ERR_FAIL_COND(p_angle <= 0 || p_angle > (Math_PI / 2.0));
	lower_vertical_angle = p_angle;
	composition_layer.lowerVerticalAngle = -p_angle;
	update_fallback_mesh();
}

float OpenXRCompositionLayerEquirect::get_lower_vertical_angle() const {
	return lower_vertical_angle;
}

void OpenXRCompositionLayerEquirect::set_fallback_segments(uint32_t p_fallback_segments) {
	ERR_FAIL_COND(p_fallback_segments == 0);
	fallback_segments = p_fallback_segments;
	update_fallback_mesh();
}

uint32_t OpenXRCompositionLayerEquirect::get_fallback_segments() const {
	return fallback_segments;
}

Vector2 OpenXRCompositionLayerEquirect::intersects_ray(const Vector3 &p_origin, const Vector3 &p_direction) const {
	Transform3D equirect_transform = get_global_transform();

	Vector3 offset = p_origin - equirect_transform.origin;
	float a = p_direction.dot(p_direction);
	float b = 2.0 * offset.dot(p_direction);
	float c = offset.dot(offset) - (radius * radius);

	float discriminant = b * b - 4.0 * a * c;
	if (discriminant < 0.0) {
		return Vector2(-1.0, -1.0);
	}

	float t0 = (-b - Math::sqrt(discriminant)) / (2.0 * a);
	float t1 = (-b + Math::sqrt(discriminant)) / (2.0 * a);
	float t = MAX(t0, t1);

	if (t < 0.0) {
		return Vector2(-1.0, -1.0);
	}
	Vector3 intersection = p_origin + p_direction * t;

	Basis correction = equirect_transform.basis.inverse();
	correction.rotate(Vector3(0.0, 1.0, 0.0), -Math_PI / 2.0);
	Vector3 relative_point = correction.xform(intersection - equirect_transform.origin);

	float horizontal_intersection_angle = Math::atan2(relative_point.z, relative_point.x);
	if (Math::abs(horizontal_intersection_angle) > central_horizontal_angle / 2.0) {
		return Vector2(-1.0, -1.0);
	}

	float vertical_intersection_angle = Math::acos(relative_point.y / radius) - (Math_PI / 2.0);
	if (vertical_intersection_angle < 0) {
		if (Math::abs(vertical_intersection_angle) > upper_vertical_angle) {
			return Vector2(-1.0, -1.0);
		}
	} else if (vertical_intersection_angle > lower_vertical_angle) {
		return Vector2(-1.0, -1.0);
	}

	// Re-center the intersection angle if the vertical angle is uneven between upper and lower.
	if (upper_vertical_angle != lower_vertical_angle) {
		vertical_intersection_angle -= (-upper_vertical_angle + lower_vertical_angle) / 2.0;
	}

	float u = 0.5 + (horizontal_intersection_angle / central_horizontal_angle);
	float v = 0.5 + (vertical_intersection_angle / (upper_vertical_angle + lower_vertical_angle));

	return Vector2(u, v);
}

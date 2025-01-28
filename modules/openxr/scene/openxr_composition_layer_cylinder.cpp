/**************************************************************************/
/*  openxr_composition_layer_cylinder.cpp                                 */
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

#include "openxr_composition_layer_cylinder.h"

#include "../openxr_interface.h"

#include "scene/resources/mesh.h"

OpenXRCompositionLayerCylinder::OpenXRCompositionLayerCylinder() :
		OpenXRCompositionLayer((XrCompositionLayerBaseHeader *)&composition_layer) {
	XRServer::get_singleton()->connect("reference_frame_changed", callable_mp(this, &OpenXRCompositionLayerCylinder::update_transform));
}

OpenXRCompositionLayerCylinder::~OpenXRCompositionLayerCylinder() {
}

void OpenXRCompositionLayerCylinder::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_radius", "radius"), &OpenXRCompositionLayerCylinder::set_radius);
	ClassDB::bind_method(D_METHOD("get_radius"), &OpenXRCompositionLayerCylinder::get_radius);

	ClassDB::bind_method(D_METHOD("set_aspect_ratio", "aspect_ratio"), &OpenXRCompositionLayerCylinder::set_aspect_ratio);
	ClassDB::bind_method(D_METHOD("get_aspect_ratio"), &OpenXRCompositionLayerCylinder::get_aspect_ratio);

	ClassDB::bind_method(D_METHOD("set_central_angle", "angle"), &OpenXRCompositionLayerCylinder::set_central_angle);
	ClassDB::bind_method(D_METHOD("get_central_angle"), &OpenXRCompositionLayerCylinder::get_central_angle);

	ClassDB::bind_method(D_METHOD("set_fallback_segments", "segments"), &OpenXRCompositionLayerCylinder::set_fallback_segments);
	ClassDB::bind_method(D_METHOD("get_fallback_segments"), &OpenXRCompositionLayerCylinder::get_fallback_segments);

	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "radius", PROPERTY_HINT_NONE, ""), "set_radius", "get_radius");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "aspect_ratio", PROPERTY_HINT_RANGE, "0,100"), "set_aspect_ratio", "get_aspect_ratio");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "central_angle", PROPERTY_HINT_RANGE, "0,360,0.1,or_less,or_greater,radians_as_degrees"), "set_central_angle", "get_central_angle");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "fallback_segments", PROPERTY_HINT_NONE, ""), "set_fallback_segments", "get_fallback_segments");
}

Ref<Mesh> OpenXRCompositionLayerCylinder::_create_fallback_mesh() {
	Ref<ArrayMesh> mesh;
	mesh.instantiate();

	float arc_length = radius * central_angle;
	float half_height = ((1.0 / aspect_ratio) * arc_length) / 2.0;

	Array arrays;
	arrays.resize(ArrayMesh::ARRAY_MAX);

	Vector<Vector3> vertices;
	Vector<Vector3> normals;
	Vector<Vector2> uvs;
	Vector<int> indices;

	float delta_angle = central_angle / fallback_segments;
	float start_angle = (-Math_PI / 2.0) - (central_angle / 2.0);

	for (uint32_t i = 0; i < fallback_segments + 1; i++) {
		float current_angle = start_angle + (delta_angle * i);
		float x = radius * Math::cos(current_angle);
		float z = radius * Math::sin(current_angle);
		Vector3 normal(Math::cos(current_angle), 0, Math::sin(current_angle));

		vertices.push_back(Vector3(x, -half_height, z));
		normals.push_back(normal);
		uvs.push_back(Vector2((float)i / fallback_segments, 1));

		vertices.push_back(Vector3(x, half_height, z));
		normals.push_back(normal);
		uvs.push_back(Vector2((float)i / fallback_segments, 0));
	}

	for (uint32_t i = 0; i < fallback_segments; i++) {
		uint32_t index = i * 2;
		indices.push_back(index);
		indices.push_back(index + 1);
		indices.push_back(index + 3);
		indices.push_back(index);
		indices.push_back(index + 3);
		indices.push_back(index + 2);
	}

	arrays[ArrayMesh::ARRAY_VERTEX] = vertices;
	arrays[ArrayMesh::ARRAY_NORMAL] = normals;
	arrays[ArrayMesh::ARRAY_TEX_UV] = uvs;
	arrays[ArrayMesh::ARRAY_INDEX] = indices;

	mesh->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, arrays);
	return mesh;
}

void OpenXRCompositionLayerCylinder::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_LOCAL_TRANSFORM_CHANGED: {
			update_transform();
		} break;
	}
}

void OpenXRCompositionLayerCylinder::update_transform() {
	composition_layer.pose = get_openxr_pose();
}

void OpenXRCompositionLayerCylinder::set_radius(float p_radius) {
	ERR_FAIL_COND(p_radius <= 0);
	radius = p_radius;
	composition_layer.radius = radius;
	update_fallback_mesh();
}

float OpenXRCompositionLayerCylinder::get_radius() const {
	return radius;
}

void OpenXRCompositionLayerCylinder::set_aspect_ratio(float p_aspect_ratio) {
	ERR_FAIL_COND(p_aspect_ratio <= 0);
	aspect_ratio = p_aspect_ratio;
	composition_layer.aspectRatio = aspect_ratio;
	update_fallback_mesh();
}

float OpenXRCompositionLayerCylinder::get_aspect_ratio() const {
	return aspect_ratio;
}

void OpenXRCompositionLayerCylinder::set_central_angle(float p_central_angle) {
	ERR_FAIL_COND(p_central_angle <= 0);
	central_angle = p_central_angle;
	composition_layer.centralAngle = central_angle;
	update_fallback_mesh();
}

float OpenXRCompositionLayerCylinder::get_central_angle() const {
	return central_angle;
}

void OpenXRCompositionLayerCylinder::set_fallback_segments(uint32_t p_fallback_segments) {
	ERR_FAIL_COND(p_fallback_segments == 0);
	fallback_segments = p_fallback_segments;
	update_fallback_mesh();
}

uint32_t OpenXRCompositionLayerCylinder::get_fallback_segments() const {
	return fallback_segments;
}

Vector2 OpenXRCompositionLayerCylinder::intersects_ray(const Vector3 &p_origin, const Vector3 &p_direction) const {
	Transform3D cylinder_transform = get_global_transform();
	Vector3 cylinder_axis = cylinder_transform.basis.get_column(1);

	Vector3 offset = p_origin - cylinder_transform.origin;
	float a = p_direction.dot(p_direction - cylinder_axis * p_direction.dot(cylinder_axis));
	float b = 2.0 * (p_direction.dot(offset - cylinder_axis * offset.dot(cylinder_axis)));
	float c = offset.dot(offset - cylinder_axis * offset.dot(cylinder_axis)) - (radius * radius);

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

	Basis correction = cylinder_transform.basis.inverse();
	correction.rotate(Vector3(0.0, 1.0, 0.0), -Math_PI / 2.0);
	Vector3 relative_point = correction.xform(intersection - cylinder_transform.origin);

	Vector2 projected_point = Vector2(relative_point.x, relative_point.z);
	float intersection_angle = Math::atan2(projected_point.y, projected_point.x);
	if (Math::abs(intersection_angle) > central_angle / 2.0) {
		return Vector2(-1.0, -1.0);
	}

	float arc_length = radius * central_angle;
	float height = aspect_ratio * arc_length;
	if (Math::abs(relative_point.y) > height / 2.0) {
		return Vector2(-1.0, -1.0);
	}

	float u = 0.5 + (intersection_angle / central_angle);
	float v = 1.0 - (0.5 + (relative_point.y / height));

	return Vector2(u, v);
}

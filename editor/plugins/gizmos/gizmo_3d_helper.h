/**************************************************************************/
/*  gizmo_3d_helper.h                                                     */
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

#ifndef GIZMO_3D_HELPER_H
#define GIZMO_3D_HELPER_H

#include "core/object/ref_counted.h"

class Camera3D;

class Gizmo3DHelper : public RefCounted {
	GDCLASS(Gizmo3DHelper, RefCounted);

	int current_handle_id;
	Variant initial_value;
	Transform3D initial_transform;

private:
	void cylinder_or_capsule_set_handle(const Vector3 p_segment[2], int p_id, real_t &r_height, real_t &r_radius, Vector3 &r_cylinder_position, bool p_is_capsule);

public:
	/**
	 * Initializes a new action involving a handle.
	 *
	 * Depending on the type of gizmo that will be used, different formats for the `p_initial_value` are required:
	 * Box: The size of the box as `Vector3`
	 * Cylinder or Capsule: A `Vector2` of the form `Vector2(radius, height)`
	 */
	void initialize_handle_action(const Variant &p_initial_value, const Transform3D &p_initial_transform);
	void get_segment(Camera3D *p_camera, const Point2 &p_point, Vector3 *r_segment);

	// Box

	Vector<Vector3> box_get_handles(const Vector3 &p_box_size);
	String box_get_handle_name(int p_id) const;
	void box_set_handle(const Vector3 p_segment[2], int p_id, Vector3 &r_box_size, Vector3 &r_box_position);
	void box_commit_handle(const String &p_action_name, bool p_cancel, Object *p_position_object, Object *p_size_object = nullptr, const StringName &p_position_property = "global_position", const StringName &p_size_property = "size");

	// Cylinder

	Vector<Vector3> cylinder_get_handles(real_t p_height, real_t p_radius);
	String cylinder_get_handle_name(int p_id) const;
	_FORCE_INLINE_ void cylinder_set_handle(const Vector3 p_segment[2], int p_id, real_t &r_height, real_t &r_radius, Vector3 &r_cylinder_position) {
		cylinder_or_capsule_set_handle(p_segment, p_id, r_height, r_radius, r_cylinder_position, false);
	}
	void cylinder_commit_handle(int p_id, const String &p_radius_action_name, const String &p_height_action_name, bool p_cancel, Object *p_position_object, Object *p_height_object = nullptr, Object *p_radius_object = nullptr, const StringName &p_position_property = "global_position", const StringName &p_height_property = "height", const StringName &p_radius_property = "radius");

	// Capsule

	_FORCE_INLINE_ Vector<Vector3> capsule_get_handles(real_t p_height, real_t p_radius) { return cylinder_get_handles(p_height, p_radius); }
	_FORCE_INLINE_ String capsule_get_handle_name(int p_id) { return cylinder_get_handle_name(p_id); }
	_FORCE_INLINE_ void capsule_set_handle(const Vector3 p_segment[2], int p_id, real_t &r_height, real_t &r_radius, Vector3 &r_capsule_position) {
		cylinder_or_capsule_set_handle(p_segment, p_id, r_height, r_radius, r_capsule_position, true);
	}
	_FORCE_INLINE_ void capsule_commit_handle(int p_id, const String &p_radius_action_name, const String &p_height_action_name, bool p_cancel, Object *p_position_object, Object *p_height_object = nullptr, Object *p_radius_object = nullptr, const StringName &p_position_property = "global_position", const StringName &p_height_property = "height", const StringName &p_radius_property = "radius") {
		cylinder_commit_handle(p_id, p_radius_action_name, p_height_action_name, p_cancel, p_position_object, p_height_object, p_radius_object, p_position_property, p_height_property, p_radius_property);
	}
};

#endif // GIZMO_3D_HELPER_H

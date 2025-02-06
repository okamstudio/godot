/**************************************************************************/
/*  path_3d.h                                                             */
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

#include "scene/3d/node_3d.h"
#include "scene/resources/curve.h"

class Path3D : public Node3D {
	GDCLASS(Path3D, Node3D);

private:
	Ref<Curve3D> curve;
	RID debug_instance;
	Ref<ArrayMesh> debug_mesh;

	Callable update_callback; // Used only by CSG currently.

	void _update_debug_mesh();
	void _curve_changed();

protected:
	void _notification(int p_what);

	static void _bind_methods();

public:
	void set_update_callback(Callable p_callback);

	void set_curve(const Ref<Curve3D> &p_curve);
	Ref<Curve3D> get_curve() const;

	Path3D();
	~Path3D();
};

class PathFollow3D : public Node3D {
	GDCLASS(PathFollow3D, Node3D);

public:
	enum RotationMode {
		ROTATION_NONE,
		ROTATION_Y,
		ROTATION_XY,
		ROTATION_XYZ,
		ROTATION_ORIENTED
	};

private:
	Path3D *path = nullptr;
	real_t progress = 0.0;
	real_t h_offset = 0.0;
	real_t v_offset = 0.0;
	bool cubic = true;
	bool loop = true;
	bool tilt_enabled = true;
	bool transform_dirty = true;
	bool use_model_front = false;
	RotationMode rotation_mode = ROTATION_XYZ;

protected:
	void _validate_property(PropertyInfo &p_property) const;

	void _notification(int p_what);

	static void _bind_methods();

public:
	void set_progress(real_t p_progress);
	real_t get_progress() const;

	void set_h_offset(real_t p_h_offset);
	real_t get_h_offset() const;

	void set_v_offset(real_t p_v_offset);
	real_t get_v_offset() const;

	void set_progress_ratio(real_t p_ratio);
	real_t get_progress_ratio() const;

	void set_loop(bool p_loop);
	bool has_loop() const;

	void set_tilt_enabled(bool p_enabled);
	bool is_tilt_enabled() const;

	void set_rotation_mode(RotationMode p_rotation_mode);
	RotationMode get_rotation_mode() const;

	void set_use_model_front(bool p_use_model_front);
	bool is_using_model_front() const;

	void set_cubic_interpolation_enabled(bool p_enabled);
	bool is_cubic_interpolation_enabled() const;

	PackedStringArray get_configuration_warnings() const override;

	void update_transform();

	static Transform3D correct_posture(Transform3D p_transform, PathFollow3D::RotationMode p_rotation_mode);

	PathFollow3D() {}
};

VARIANT_ENUM_CAST(PathFollow3D::RotationMode);

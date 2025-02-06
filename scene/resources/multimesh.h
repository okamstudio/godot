/**************************************************************************/
/*  multimesh.h                                                           */
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

#include "scene/resources/mesh.h"
#include "servers/rendering_server.h"

class MultiMesh : public Resource {
	GDCLASS(MultiMesh, Resource);
	RES_BASE_EXTENSION("multimesh");

public:
	enum TransformFormat {
		TRANSFORM_2D = RS::MULTIMESH_TRANSFORM_2D,
		TRANSFORM_3D = RS::MULTIMESH_TRANSFORM_3D
	};

	enum PhysicsInterpolationQuality {
		INTERP_QUALITY_FAST,
		INTERP_QUALITY_HIGH,
	};

private:
	Ref<Mesh> mesh;
	RID multimesh;
	TransformFormat transform_format = TRANSFORM_2D;
	AABB custom_aabb;
	bool use_colors = false;
	bool use_custom_data = false;
	int instance_count = 0;
	int visible_instance_count = -1;
	PhysicsInterpolationQuality _physics_interpolation_quality = INTERP_QUALITY_FAST;

protected:
	static void _bind_methods();

#ifndef DISABLE_DEPRECATED
	// Kept for compatibility from 3.x to 4.0.
	void _set_transform_array(const Vector<Vector3> &p_array);
	Vector<Vector3> _get_transform_array() const;

	void _set_transform_2d_array(const Vector<Vector2> &p_array);
	Vector<Vector2> _get_transform_2d_array() const;

	void _set_color_array(const Vector<Color> &p_array);
	Vector<Color> _get_color_array() const;

	void _set_custom_data_array(const Vector<Color> &p_array);
	Vector<Color> _get_custom_data_array() const;
#endif
	void set_buffer(const Vector<float> &p_buffer);
	Vector<float> get_buffer() const;

	void set_buffer_interpolated(const Vector<float> &p_buffer_curr, const Vector<float> &p_buffer_prev);

public:
	void set_mesh(const Ref<Mesh> &p_mesh);
	Ref<Mesh> get_mesh() const;

	void set_use_colors(bool p_enable);
	bool is_using_colors() const;

	void set_use_custom_data(bool p_enable);
	bool is_using_custom_data() const;

	void set_transform_format(TransformFormat p_transform_format);
	TransformFormat get_transform_format() const;

	void set_instance_count(int p_count);
	int get_instance_count() const;

	void set_visible_instance_count(int p_count);
	int get_visible_instance_count() const;

	void set_physics_interpolation_quality(PhysicsInterpolationQuality p_quality);
	PhysicsInterpolationQuality get_physics_interpolation_quality() const { return _physics_interpolation_quality; }

	void set_instance_transform(int p_instance, const Transform3D &p_transform);
	void set_instance_transform_2d(int p_instance, const Transform2D &p_transform);
	Transform3D get_instance_transform(int p_instance) const;
	Transform2D get_instance_transform_2d(int p_instance) const;

	void set_instance_color(int p_instance, const Color &p_color);
	Color get_instance_color(int p_instance) const;

	void set_instance_custom_data(int p_instance, const Color &p_custom_data);
	Color get_instance_custom_data(int p_instance) const;

	void reset_instance_physics_interpolation(int p_instance);

	void set_physics_interpolated(bool p_interpolated);

	void set_custom_aabb(const AABB &p_custom);
	AABB get_custom_aabb() const;

	virtual AABB get_aabb() const;

	virtual RID get_rid() const override;

	MultiMesh();
	~MultiMesh();
};

VARIANT_ENUM_CAST(MultiMesh::TransformFormat);
VARIANT_ENUM_CAST(MultiMesh::PhysicsInterpolationQuality);

/**************************************************************************/
/*  primitive_meshes_2d.h                                                 */
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

#ifndef PRIMITIVE_MESHES_2D_H
#define PRIMITIVE_MESHES_2D_H

#include "scene/resources/mesh.h"

// Use the same antialiasing feather size as StyleBoxFlat's default.
// This value is empirically determined to provide good antialiasing quality
// while not making lines appear too soft.
const static float FEATHER_SIZE = 1.25f;

// @TODO Probably should change a few integers to unsigned integers...

/**
	Base class for all the classes in this file, handles a number of code functions that are shared among all meshes.
	This class is set apart that it assumes a single surface is always generated for our mesh.
*/

class PrimitiveMesh2D : public Mesh {
	GDCLASS(PrimitiveMesh2D, Mesh);

private:
	RID mesh;
	mutable AABB aabb;
	AABB custom_aabb;

	mutable int array_len = 0;
	mutable int index_array_len = 0;

	// Make sure we do an update after we've finished constructing our object.
	mutable bool pending_request = true;
	void _update() const;

protected:
	// Assume primitive triangles as the type, correct for all but one and it will change this :)
	Mesh::PrimitiveType primitive_type = Mesh::PRIMITIVE_TRIANGLES;

	bool antialiased = false;

	static void _bind_methods();
	void _validate_property(PropertyInfo &p_property) const;

	virtual void _create_mesh_array(Array &p_arr) const {}
	GDVIRTUAL0RC(Array, _create_mesh_array)

	void _on_settings_changed();

public:
	virtual int get_surface_count() const override;
	virtual int surface_get_array_len(int p_idx) const override;
	virtual int surface_get_array_index_len(int p_idx) const override;
	virtual Array surface_get_arrays(int p_surface) const override;
	virtual TypedArray<Array> surface_get_blend_shape_arrays(int p_surface) const override;
	virtual Dictionary surface_get_lods(int p_surface) const override;
	virtual BitField<ArrayFormat> surface_get_format(int p_idx) const override;
	virtual Mesh::PrimitiveType surface_get_primitive_type(int p_idx) const override;
	virtual void surface_set_material(int p_idx, const Ref<Material> &p_material) override;
	virtual Ref<Material> surface_get_material(int p_idx) const override;
	virtual int get_blend_shape_count() const override;
	virtual StringName get_blend_shape_name(int p_index) const override;
	virtual void set_blend_shape_name(int p_index, const StringName &p_name) override;
	virtual AABB get_aabb() const override;
	virtual RID get_rid() const override;

	Array get_mesh_arrays() const;

	bool is_antialiased() const;
	void set_antialiased(bool p_antialiased);

	void set_custom_aabb(const AABB &p_custom);
	AABB get_custom_aabb() const;

	void request_update();

	PrimitiveMesh2D();
	~PrimitiveMesh2D();
};

/**
	A rectangle
*/
class RectangleMesh2D : public PrimitiveMesh2D {
	GDCLASS(RectangleMesh2D, PrimitiveMesh2D);

private:
	Vector2 size = Vector2(20, 20);
	int subdivide_w = 0;
	int subdivide_h = 0;

protected:
	virtual void _create_mesh_array(Array &p_arr) const override;
	static void _bind_methods();

public:
	void set_size(const Vector2 &p_size);
	Vector2 get_size() const;

	void set_subdivide_width(const int p_divisions);
	int get_subdivide_width() const;

	void set_subdivide_height(const int p_divisions);
	int get_subdivide_height() const;

	RectangleMesh2D();
};

/**
	A circle
*/
class CircleMesh2D : public PrimitiveMesh2D {
	GDCLASS(CircleMesh2D, PrimitiveMesh2D);

private:
	float radius = 10;
	int radial_segments = 64;

protected:
	virtual void _create_mesh_array(Array &p_arr) const override;
	static void _bind_methods();

public:
	void set_radius(const float p_radius);
	float get_radius() const;

	void set_radial_segments(const int p_segments);
	int get_radial_segments() const;

	CircleMesh2D();
};

/**
	A capsule
*/
class CapsuleMesh2D : public PrimitiveMesh2D {
	GDCLASS(CapsuleMesh2D, PrimitiveMesh2D);

private:
	float radius = 10;
	float height = 30;
	int radial_segments = 32;

protected:
	virtual void _create_mesh_array(Array &p_arr) const override;
	static void _bind_methods();

public:
	void set_radius(const float p_radius);
	float get_radius() const;

	void set_height(const float p_height);
	float get_height() const;

	void set_radial_segments(const int p_segments);
	int get_radial_segments() const;

	CapsuleMesh2D();
};

#endif // PRIMITIVE_MESHES_2D_H

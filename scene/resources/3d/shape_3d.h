/**************************************************************************/
/*  shape_3d.h                                                            */
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

#include "core/io/resource.h"

class ArrayMesh;
class Material;

class Shape3D : public Resource {
	GDCLASS(Shape3D, Resource);
	OBJ_SAVE_TYPE(Shape3D);
	RES_BASE_EXTENSION("shape");
	RID shape;
	real_t custom_bias = 0.0;
	real_t margin = 0.04;

	Ref<ArrayMesh> debug_mesh_cache;
	Ref<Material> collision_material;

	// Not wrapped in `#ifdef DEBUG_ENABLED` as it is used for rendering.
	Color debug_color = Color(0.0, 0.0, 0.0, 0.0);
	bool debug_fill = true;
#ifdef DEBUG_ENABLED
	bool debug_properties_edited = false;
#endif // DEBUG_ENABLED

protected:
	static void _bind_methods();

	_FORCE_INLINE_ RID get_shape() const { return shape; }
	Shape3D(RID p_shape);

	Ref<Material> get_debug_collision_material();

	virtual void _update_shape();

public:
	virtual RID get_rid() const override { return shape; }

	Ref<ArrayMesh> get_debug_mesh();
	virtual Vector<Vector3> get_debug_mesh_lines() const = 0; // { return Vector<Vector3>(); }
	virtual Ref<ArrayMesh> get_debug_arraymesh_faces(const Color &p_modulate) const = 0;
	/// Returns the radius of a sphere that fully enclose this shape
	virtual real_t get_enclosing_radius() const = 0;

	void add_vertices_to_array(Vector<Vector3> &array, const Transform3D &p_xform);

	void set_custom_solver_bias(real_t p_bias);
	real_t get_custom_solver_bias() const;

	real_t get_margin() const;
	void set_margin(real_t p_margin);

	void set_debug_color(const Color &p_color);
	Color get_debug_color() const;

	void set_debug_fill(bool p_fill);
	bool get_debug_fill() const;

#ifdef DEBUG_ENABLED
	_FORCE_INLINE_ bool are_debug_properties_edited() const { return debug_properties_edited; }
#endif // DEBUG_ENABLED

	Shape3D();
	~Shape3D();
};

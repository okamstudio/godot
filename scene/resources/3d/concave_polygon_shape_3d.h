/**************************************************************************/
/*  concave_polygon_shape_3d.h                                            */
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

#include "scene/resources/3d/shape_3d.h"

class ArrayMesh;

class ConcavePolygonShape3D : public Shape3D {
	GDCLASS(ConcavePolygonShape3D, Shape3D);

	Vector<Vector3> faces;
	bool backface_collision = false;

	struct DrawEdge {
		Vector3 a;
		Vector3 b;
		static uint32_t hash(const DrawEdge &p_edge) {
			uint32_t h = hash_murmur3_one_32(HashMapHasherDefault::hash(p_edge.a));
			return hash_murmur3_one_32(HashMapHasherDefault::hash(p_edge.b), h);
		}
		bool operator==(const DrawEdge &p_edge) const {
			return (a == p_edge.a && b == p_edge.b);
		}

		DrawEdge(const Vector3 &p_a = Vector3(), const Vector3 &p_b = Vector3()) {
			a = p_a;
			b = p_b;
			if (a < b) {
				SWAP(a, b);
			}
		}
	};

protected:
	static void _bind_methods();

	virtual void _update_shape() override;

public:
	void set_faces(const Vector<Vector3> &p_faces);
	Vector<Vector3> get_faces() const;

	void set_backface_collision_enabled(bool p_enabled);
	bool is_backface_collision_enabled() const;

	virtual Vector<Vector3> get_debug_mesh_lines() const override;
	virtual Ref<ArrayMesh> get_debug_arraymesh_faces(const Color &p_modulate) const override;
	virtual real_t get_enclosing_radius() const override;

	ConcavePolygonShape3D();
};

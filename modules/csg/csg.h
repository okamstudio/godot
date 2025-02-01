/**************************************************************************/
/*  csg.h                                                                 */
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

#include "core/math/aabb.h"
#include "core/math/transform_3d.h"
#include "core/math/vector2.h"
#include "core/math/vector3.h"
#include "core/object/ref_counted.h"
#include "core/templates/vector.h"
#include "scene/resources/material.h"

struct CSGBrush {
	struct Face {
		Vector3 vertices[3];
		Vector2 uvs[3];
		AABB aabb;
		bool smooth = false;
		bool invert = false;
		int material = 0;
	};

	Vector<Face> faces;
	Vector<Ref<Material>> materials;

	inline void _regen_face_aabbs() {
		for (int i = 0; i < faces.size(); i++) {
			faces.write[i].aabb = AABB();
			faces.write[i].aabb.position = faces[i].vertices[0];
			faces.write[i].aabb.expand_to(faces[i].vertices[1]);
			faces.write[i].aabb.expand_to(faces[i].vertices[2]);
		}
	}

	// Create a brush from faces.
	void build_from_faces(const Vector<Vector3> &p_vertices, const Vector<Vector2> &p_uvs, const Vector<bool> &p_smooth, const Vector<Ref<Material>> &p_materials, const Vector<bool> &p_invert_faces);
	void copy_from(const CSGBrush &p_brush, const Transform3D &p_xform);
};

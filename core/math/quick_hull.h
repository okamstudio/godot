/**************************************************************************/
/*  quick_hull.h                                                          */
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

#include "core/math/geometry_3d.h"
#include "core/templates/list.h"

class QuickHull {
public:
	struct Edge {
		union {
			uint32_t vertices[2];
			uint64_t id = 0;
		};

		static uint32_t hash(const Edge &p_edge) {
			return hash_one_uint64(p_edge.id);
		}

		bool operator<(const Edge &p_edge) const {
			return id < p_edge.id;
		}
		bool operator==(const Edge &p_edge) const {
			return id == p_edge.id;
		}

		Edge(int p_vtx_a = 0, int p_vtx_b = 0) {
			if (p_vtx_a > p_vtx_b) {
				SWAP(p_vtx_a, p_vtx_b);
			}

			vertices[0] = p_vtx_a;
			vertices[1] = p_vtx_b;
		}
	};

	struct Face {
		Plane plane;
		uint32_t vertices[3] = { 0 };
		Vector<int> points_over;

		bool operator<(const Face &p_face) const {
			return points_over.size() < p_face.points_over.size();
		}
	};

private:
	struct FaceConnect {
		List<Face>::Element *left = nullptr;
		List<Face>::Element *right = nullptr;
		FaceConnect() {}
	};
	struct RetFaceConnect {
		List<Geometry3D::MeshData::Face>::Element *left = nullptr;
		List<Geometry3D::MeshData::Face>::Element *right = nullptr;
		RetFaceConnect() {}
	};

public:
	static uint32_t debug_stop_after;
	static Error build(const Vector<Vector3> &p_points, Geometry3D::MeshData &r_mesh);
};

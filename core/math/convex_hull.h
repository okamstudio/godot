/**************************************************************************/
/*  convex_hull.h                                                         */
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

/*
Copyright (c) 2011 Ole Kniemeyer, MAXON, www.maxon.net
This software is provided 'as-is', without any express or implied warranty.
In no event will the authors be held liable for any damages arising from the use of this software.
Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it freely,
subject to the following restrictions:
1. The origin of this software must not be misrepresented; you must not claim that you wrote the original software. If you use this software in a product, an acknowledgment in the product documentation would be appreciated but is not required.
2. Altered source versions must be plainly marked as such, and must not be misrepresented as being the original software.
3. This notice may not be removed or altered from any source distribution.
*/

#include "core/math/geometry_3d.h"
#include "core/math/vector3.h"
#include "core/templates/local_vector.h"
#include "core/templates/vector.h"

/// Convex hull implementation based on Preparata and Hong
/// See https://code.google.com/archive/p/bullet/issues/275
/// Ole Kniemeyer, MAXON Computer GmbH
class ConvexHullComputer {
public:
	class Edge {
	private:
		int32_t next = 0;
		int32_t reverse = 0;
		int32_t target_vertex = 0;

		friend class ConvexHullComputer;

	public:
		int32_t get_next_relative() const {
			return next;
		}

		int32_t get_source_vertex() const {
			return (this + reverse)->target_vertex;
		}

		int32_t get_target_vertex() const {
			return target_vertex;
		}

		const Edge *get_next_edge_of_vertex() const // clockwise list of all edges of a vertex
		{
			return this + next;
		}

		const Edge *get_next_edge_of_face() const // counter-clockwise list of all edges of a face
		{
			return (this + reverse)->get_next_edge_of_vertex();
		}

		const Edge *get_reverse_edge() const {
			return this + reverse;
		}
	};

	// Vertices of the output hull
	LocalVector<Vector3> vertices;

	// Edges of the output hull
	LocalVector<Edge> edges;

	// Faces of the convex hull. Each entry is an index into the "edges" array pointing to an edge of the face. Faces are planar n-gons
	LocalVector<int32_t> faces;

	/*
		Compute convex hull of "count" vertices stored in "coords".
		If "shrink" is positive, the convex hull is shrunken by that amount (each face is moved by "shrink" length units
		towards the center along its normal).
		If "shrinkClamp" is positive, "shrink" is clamped to not exceed "shrinkClamp * innerRadius", where "innerRadius"
		is the minimum distance of a face to the center of the convex hull.
		The returned value is the amount by which the hull has been shrunken. If it is negative, the amount was so large
		that the resulting convex hull is empty.
		The output convex hull can be found in the member variables "vertices", "edges", "faces".
		*/
	real_t compute(const Vector3 *p_coords, int32_t p_count, real_t p_shrink, real_t p_shrink_clamp);

	static Error convex_hull(const Vector<Vector3> &p_points, Geometry3D::MeshData &r_mesh);
};

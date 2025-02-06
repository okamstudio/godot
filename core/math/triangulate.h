/**************************************************************************/
/*  triangulate.h                                                         */
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

#include "core/math/vector2.h"
#include "core/templates/vector.h"

/*
https://www.flipcode.com/archives/Efficient_Polygon_Triangulation.shtml
*/

class Triangulate {
public:
	// triangulate a contour/polygon, places results in STL vector
	// as series of triangles.
	static bool triangulate(const Vector<Vector2> &contour, Vector<int> &result);

	// compute area of a contour/polygon
	static real_t get_area(const Vector<Vector2> &contour);

	// decide if point Px/Py is inside triangle defined by
	// (Ax,Ay) (Bx,By) (Cx,Cy)
	static bool is_inside_triangle(real_t Ax, real_t Ay,
			real_t Bx, real_t By,
			real_t Cx, real_t Cy,
			real_t Px, real_t Py,
			bool include_edges);

private:
	static bool snip(const Vector<Vector2> &p_contour, int u, int v, int w, int n, const Vector<int> &V, bool relaxed);
};

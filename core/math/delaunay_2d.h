/**************************************************************************/
/*  delaunay_2d.h                                                         */
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

#include "core/math/rect2.h"
#include "core/templates/vector.h"

class Delaunay2D {
public:
	struct Triangle {
		int points[3];
		Vector2 circum_center;
		real_t circum_radius_squared;
		Triangle() {}
		Triangle(int p_a, int p_b, int p_c) {
			points[0] = p_a;
			points[1] = p_b;
			points[2] = p_c;
		}
	};

	struct Edge {
		int points[2];
		bool bad = false;
		Edge() {}
		Edge(int p_a, int p_b) {
			// Store indices in a sorted manner to avoid having to check both orientations later.
			if (p_a > p_b) {
				points[0] = p_b;
				points[1] = p_a;
			} else {
				points[0] = p_a;
				points[1] = p_b;
			}
		}
	};

	static Triangle create_triangle(const Vector<Vector2> &p_vertices, int p_a, int p_b, int p_c) {
		Triangle triangle = Triangle(p_a, p_b, p_c);

		// Get the values of the circumcircle and store them inside the triangle object.
		Vector2 a = p_vertices[p_b] - p_vertices[p_a];
		Vector2 b = p_vertices[p_c] - p_vertices[p_a];

		Vector2 O = (b * a.length_squared() - a * b.length_squared()).orthogonal() / (a.cross(b) * 2.0f);

		triangle.circum_radius_squared = O.length_squared();
		triangle.circum_center = O + p_vertices[p_a];

		return triangle;
	}

	static Vector<Triangle> triangulate(const Vector<Vector2> &p_points) {
		Vector<Vector2> points = p_points;
		Vector<Triangle> triangles;

		int point_count = p_points.size();
		if (point_count <= 2) {
			return triangles;
		}

		// Get a bounding rectangle.
		Rect2 rect = Rect2(p_points[0], Size2());
		for (int i = 1; i < point_count; i++) {
			rect.expand_to(p_points[i]);
		}

		real_t delta_max = MAX(rect.size.width, rect.size.height);
		Vector2 center = rect.get_center();

		// Construct a bounding triangle around the rectangle.
		points.push_back(Vector2(center.x - delta_max * 16, center.y - delta_max));
		points.push_back(Vector2(center.x, center.y + delta_max * 16));
		points.push_back(Vector2(center.x + delta_max * 16, center.y - delta_max));

		Triangle bounding_triangle = create_triangle(points, point_count + 0, point_count + 1, point_count + 2);
		triangles.push_back(bounding_triangle);

		for (int i = 0; i < point_count; i++) {
			Vector<Edge> polygon;

			// Save the edges of the triangles whose circumcircles contain the i-th vertex. Delete the triangles themselves.
			for (int j = triangles.size() - 1; j >= 0; j--) {
				if (points[i].distance_squared_to(triangles[j].circum_center) < triangles[j].circum_radius_squared) {
					polygon.push_back(Edge(triangles[j].points[0], triangles[j].points[1]));
					polygon.push_back(Edge(triangles[j].points[1], triangles[j].points[2]));
					polygon.push_back(Edge(triangles[j].points[2], triangles[j].points[0]));

					triangles.remove_at(j);
				}
			}

			// Create a triangle for every unique edge.
			for (int j = 0; j < polygon.size(); j++) {
				if (polygon[j].bad) {
					continue;
				}

				for (int k = j + 1; k < polygon.size(); k++) {
					// Compare the edges.
					if (polygon[k].points[0] == polygon[j].points[0] && polygon[k].points[1] == polygon[j].points[1]) {
						polygon.write[j].bad = true;
						polygon.write[k].bad = true;

						break; // Since no more than two triangles can share an edge, no more than two edges can share vertices.
					}
				}

				// Create triangles out of good edges.
				if (!polygon[j].bad) {
					triangles.push_back(create_triangle(points, polygon[j].points[0], polygon[j].points[1], i));
				}
			}
		}

		// Filter out the triangles containing vertices of the bounding triangle.
		int preserved_count = 0;
		Triangle *triangles_ptrw = triangles.ptrw();
		for (int i = 0; i < triangles.size(); i++) {
			if (!(triangles[i].points[0] >= point_count || triangles[i].points[1] >= point_count || triangles[i].points[2] >= point_count)) {
				triangles_ptrw[preserved_count] = triangles[i];
				preserved_count++;
			}
		}
		triangles.resize(preserved_count);

		return triangles;
	}
};

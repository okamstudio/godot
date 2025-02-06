/**************************************************************************/
/*  quick_hull.cpp                                                        */
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

#include "quick_hull.h"

#include "core/templates/hash_map.h"
#include "core/templates/hash_set.h"

uint32_t QuickHull::debug_stop_after = 0xFFFFFFFF;

Error QuickHull::build(const Vector<Vector3> &p_points, Geometry3D::MeshData &r_mesh) {
	/* CREATE AABB VOLUME */

	AABB aabb;
	for (int i = 0; i < p_points.size(); i++) {
		if (i == 0) {
			aabb.position = p_points[i];
		} else {
			aabb.expand_to(p_points[i]);
		}
	}

	if (aabb.size == Vector3()) {
		return ERR_CANT_CREATE;
	}

	Vector<bool> valid_points;
	valid_points.resize(p_points.size());
	HashSet<Vector3> valid_cache;

	for (int i = 0; i < p_points.size(); i++) {
		Vector3 sp = p_points[i].snappedf(0.0001);
		if (valid_cache.has(sp)) {
			valid_points.write[i] = false;
		} else {
			valid_points.write[i] = true;
			valid_cache.insert(sp);
		}
	}

	/* CREATE INITIAL SIMPLEX */

	int longest_axis = aabb.get_longest_axis_index();

	//first two vertices are the most distant
	int simplex[4] = { 0 };

	{
		real_t max = 0, min = 0;

		for (int i = 0; i < p_points.size(); i++) {
			if (!valid_points[i]) {
				continue;
			}
			real_t d = p_points[i][longest_axis];
			if (i == 0 || d < min) {
				simplex[0] = i;
				min = d;
			}

			if (i == 0 || d > max) {
				simplex[1] = i;
				max = d;
			}
		}
	}

	//third vertex is one most further away from the line

	{
		real_t maxd = 0;
		Vector3 rel12 = p_points[simplex[0]] - p_points[simplex[1]];

		for (int i = 0; i < p_points.size(); i++) {
			if (!valid_points[i]) {
				continue;
			}

			Vector3 n = rel12.cross(p_points[simplex[0]] - p_points[i]).cross(rel12).normalized();
			real_t d = Math::abs(n.dot(p_points[simplex[0]]) - n.dot(p_points[i]));

			if (i == 0 || d > maxd) {
				maxd = d;
				simplex[2] = i;
			}
		}
	}

	//fourth vertex is the one most further away from the plane

	{
		real_t maxd = 0;
		Plane p(p_points[simplex[0]], p_points[simplex[1]], p_points[simplex[2]]);

		for (int i = 0; i < p_points.size(); i++) {
			if (!valid_points[i]) {
				continue;
			}

			real_t d = Math::abs(p.distance_to(p_points[i]));

			if (i == 0 || d > maxd) {
				maxd = d;
				simplex[3] = i;
			}
		}
	}

	//compute center of simplex, this is a point always warranted to be inside
	Vector3 center;

	for (int i = 0; i < 4; i++) {
		center += p_points[simplex[i]];
	}

	center /= 4.0;

	//add faces

	List<Face> faces;

	for (int i = 0; i < 4; i++) {
		static const int face_order[4][3] = {
			{ 0, 1, 2 },
			{ 0, 1, 3 },
			{ 0, 2, 3 },
			{ 1, 2, 3 }
		};

		Face f;
		for (int j = 0; j < 3; j++) {
			f.vertices[j] = simplex[face_order[i][j]];
		}

		Plane p(p_points[f.vertices[0]], p_points[f.vertices[1]], p_points[f.vertices[2]]);

		if (p.is_point_over(center)) {
			//flip face to clockwise if facing inwards
			SWAP(f.vertices[0], f.vertices[1]);
			p = -p;
		}

		f.plane = p;

		faces.push_back(f);
	}

	real_t over_tolerance = 3 * UNIT_EPSILON * (aabb.size.x + aabb.size.y + aabb.size.z);

	/* COMPUTE AVAILABLE VERTICES */

	for (int i = 0; i < p_points.size(); i++) {
		if (i == simplex[0]) {
			continue;
		}
		if (i == simplex[1]) {
			continue;
		}
		if (i == simplex[2]) {
			continue;
		}
		if (i == simplex[3]) {
			continue;
		}
		if (!valid_points[i]) {
			continue;
		}

		for (Face &E : faces) {
			if (E.plane.distance_to(p_points[i]) > over_tolerance) {
				E.points_over.push_back(i);
				break;
			}
		}
	}

	faces.sort(); // sort them, so the ones with points are in the back

	/* BUILD HULL */

	//poop face (while still remain)
	//find further away point
	//find lit faces
	//determine horizon edges
	//build new faces with horizon edges, them assign points side from all lit faces
	//remove lit faces

	uint32_t debug_stop = debug_stop_after;

	while (debug_stop > 0 && faces.back()->get().points_over.size()) {
		debug_stop--;
		Face &f = faces.back()->get();

		//find vertex most outside
		int next = -1;
		real_t next_d = 0;

		for (int i = 0; i < f.points_over.size(); i++) {
			real_t d = f.plane.distance_to(p_points[f.points_over[i]]);

			if (d > next_d) {
				next_d = d;
				next = i;
			}
		}

		ERR_FAIL_COND_V(next == -1, ERR_BUG);

		Vector3 v = p_points[f.points_over[next]];

		//find lit faces and lit edges
		List<List<Face>::Element *> lit_faces; //lit face is a death sentence

		HashMap<Edge, FaceConnect, Edge> lit_edges; //create this on the flight, should not be that bad for performance and simplifies code a lot

		for (List<Face>::Element *E = faces.front(); E; E = E->next()) {
			if (E->get().plane.distance_to(v) > 0) {
				lit_faces.push_back(E);

				for (int i = 0; i < 3; i++) {
					uint32_t a = E->get().vertices[i];
					uint32_t b = E->get().vertices[(i + 1) % 3];
					Edge e(a, b);

					HashMap<Edge, FaceConnect, Edge>::Iterator F = lit_edges.find(e);
					if (!F) {
						F = lit_edges.insert(e, FaceConnect());
					}
					if (e.vertices[0] == a) {
						//left
						F->value.left = E;
					} else {
						F->value.right = E;
					}
				}
			}
		}

		//create new faces from horizon edges
		List<List<Face>::Element *> new_faces; //new faces

		for (KeyValue<Edge, FaceConnect> &E : lit_edges) {
			FaceConnect &fc = E.value;
			if (fc.left && fc.right) {
				continue; //edge is uninteresting, not on horizon
			}

			//create new face!

			Face face;
			face.vertices[0] = f.points_over[next];
			face.vertices[1] = E.key.vertices[0];
			face.vertices[2] = E.key.vertices[1];

			Plane p(p_points[face.vertices[0]], p_points[face.vertices[1]], p_points[face.vertices[2]]);

			if (p.is_point_over(center)) {
				//flip face to clockwise if facing inwards
				SWAP(face.vertices[0], face.vertices[1]);
				p = -p;
			}

			face.plane = p;
			new_faces.push_back(faces.push_back(face));
		}

		//distribute points into new faces

		for (List<Face>::Element *&F : lit_faces) {
			Face &lf = F->get();

			for (int i = 0; i < lf.points_over.size(); i++) {
				if (lf.points_over[i] == f.points_over[next]) { //do not add current one
					continue;
				}

				Vector3 p = p_points[lf.points_over[i]];
				for (List<Face>::Element *&E : new_faces) {
					Face &f2 = E->get();
					if (f2.plane.distance_to(p) > over_tolerance) {
						f2.points_over.push_back(lf.points_over[i]);
						break;
					}
				}
			}
		}

		//erase lit faces

		while (lit_faces.size()) {
			faces.erase(lit_faces.front()->get());
			lit_faces.pop_front();
		}

		//put faces that contain no points on the front

		for (List<Face>::Element *&E : new_faces) {
			Face &f2 = E->get();
			if (f2.points_over.size() == 0) {
				faces.move_to_front(E);
			}
		}

		//whew, done with iteration, go next
	}

	/* CREATE MESHDATA */

	//make a map of edges again
	HashMap<Edge, RetFaceConnect, Edge> ret_edges;
	List<Geometry3D::MeshData::Face> ret_faces;

	for (const Face &E : faces) {
		Geometry3D::MeshData::Face f;
		f.plane = E.plane;

		for (int i = 0; i < 3; i++) {
			f.indices.push_back(E.vertices[i]);
		}

		List<Geometry3D::MeshData::Face>::Element *F = ret_faces.push_back(f);

		for (int i = 0; i < 3; i++) {
			uint32_t a = E.vertices[i];
			uint32_t b = E.vertices[(i + 1) % 3];
			Edge e(a, b);

			HashMap<Edge, RetFaceConnect, Edge>::Iterator G = ret_edges.find(e);
			if (!G) {
				G = ret_edges.insert(e, RetFaceConnect());
			}
			if (e.vertices[0] == a) {
				//left
				G->value.left = F;
			} else {
				G->value.right = F;
			}
		}
	}

	//fill faces

	for (List<Geometry3D::MeshData::Face>::Element *E = ret_faces.front(); E; E = E->next()) {
		Geometry3D::MeshData::Face &f = E->get();

		for (uint32_t i = 0; i < f.indices.size(); i++) {
			int a = E->get().indices[i];
			int b = E->get().indices[(i + 1) % f.indices.size()];
			Edge e(a, b);

			HashMap<Edge, RetFaceConnect, Edge>::Iterator F = ret_edges.find(e);

			ERR_CONTINUE(!F);
			List<Geometry3D::MeshData::Face>::Element *O = F->value.left == E ? F->value.right : F->value.left;
			ERR_CONTINUE(O == E);
			ERR_CONTINUE(O == nullptr);

			if (O->get().plane.is_equal_approx(f.plane)) {
				//merge and delete edge and contiguous face, while repointing edges (uuugh!)
				int o_index_size = O->get().indices.size();

				for (int j = 0; j < o_index_size; j++) {
					//search a
					if (O->get().indices[j] == a) {
						//append the rest
						for (int k = 0; k < o_index_size; k++) {
							int idx = O->get().indices[(k + j) % o_index_size];
							int idxn = O->get().indices[(k + j + 1) % o_index_size];
							if (idx == b && idxn == a) { //already have b!
								break;
							}
							if (idx != a) {
								f.indices.insert(i + 1, idx);
								i++;
							}
							Edge e2(idx, idxn);

							HashMap<Edge, RetFaceConnect, Edge>::Iterator F2 = ret_edges.find(e2);
							ERR_CONTINUE(!F2);
							//change faceconnect, point to this face instead
							if (F2->value.left == O) {
								F2->value.left = E;
							} else if (F2->value.right == O) {
								F2->value.right = E;
							}
						}

						break;
					}
				}

				// remove all edge connections to this face
				for (KeyValue<Edge, RetFaceConnect> &G : ret_edges) {
					if (G.value.left == O) {
						G.value.left = nullptr;
					}

					if (G.value.right == O) {
						G.value.right = nullptr;
					}
				}

				ret_edges.remove(F); //remove the edge
				ret_faces.erase(O); //remove the face
			}
		}
	}

	//fill mesh
	r_mesh.faces.clear();
	r_mesh.faces.resize(ret_faces.size());

	HashMap<List<Geometry3D::MeshData::Face>::Element *, int> face_indices;

	int idx = 0;
	for (List<Geometry3D::MeshData::Face>::Element *E = ret_faces.front(); E; E = E->next()) {
		face_indices[E] = idx;
		r_mesh.faces[idx++] = E->get();
	}
	r_mesh.edges.resize(ret_edges.size());
	idx = 0;
	for (const KeyValue<Edge, RetFaceConnect> &E : ret_edges) {
		Geometry3D::MeshData::Edge e;
		e.vertex_a = E.key.vertices[0];
		e.vertex_b = E.key.vertices[1];
		ERR_CONTINUE(!face_indices.has(E.value.left));
		ERR_CONTINUE(!face_indices.has(E.value.right));
		e.face_a = face_indices[E.value.left];
		e.face_b = face_indices[E.value.right];
		r_mesh.edges[idx++] = e;
	}

	r_mesh.vertices = p_points;

	return OK;
}

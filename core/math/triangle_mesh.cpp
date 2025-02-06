/**************************************************************************/
/*  triangle_mesh.cpp                                                     */
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

#include "triangle_mesh.h"

#include "core/templates/sort_array.h"

int TriangleMesh::_create_bvh(BVH *p_bvh, BVH **p_bb, int p_from, int p_size, int p_depth, int &r_max_depth, int &r_max_alloc) {
	if (p_depth > r_max_depth) {
		r_max_depth = p_depth;
	}

	if (p_size == 1) {
		return p_bb[p_from] - p_bvh;
	} else if (p_size == 0) {
		return -1;
	}

	AABB aabb;
	aabb = p_bb[p_from]->aabb;
	for (int i = 1; i < p_size; i++) {
		aabb.merge_with(p_bb[p_from + i]->aabb);
	}

	int li = aabb.get_longest_axis_index();

	switch (li) {
		case Vector3::AXIS_X: {
			SortArray<BVH *, BVHCmpX> sort_x;
			sort_x.nth_element(0, p_size, p_size / 2, &p_bb[p_from]);
			//sort_x.sort(&p_bb[p_from],p_size);
		} break;
		case Vector3::AXIS_Y: {
			SortArray<BVH *, BVHCmpY> sort_y;
			sort_y.nth_element(0, p_size, p_size / 2, &p_bb[p_from]);
			//sort_y.sort(&p_bb[p_from],p_size);
		} break;
		case Vector3::AXIS_Z: {
			SortArray<BVH *, BVHCmpZ> sort_z;
			sort_z.nth_element(0, p_size, p_size / 2, &p_bb[p_from]);
			//sort_z.sort(&p_bb[p_from],p_size);

		} break;
	}

	int left = _create_bvh(p_bvh, p_bb, p_from, p_size / 2, p_depth + 1, r_max_depth, r_max_alloc);
	int right = _create_bvh(p_bvh, p_bb, p_from + p_size / 2, p_size - p_size / 2, p_depth + 1, r_max_depth, r_max_alloc);

	int index = r_max_alloc++;
	BVH *_new = &p_bvh[index];
	_new->aabb = aabb;
	_new->center = aabb.get_center();
	_new->face_index = -1;
	_new->left = left;
	_new->right = right;

	return index;
}

void TriangleMesh::get_indices(Vector<int> *r_triangles_indices) const {
	if (!valid) {
		return;
	}

	const int triangles_num = triangles.size();

	// Parse vertices indices
	const Triangle *triangles_read = triangles.ptr();

	r_triangles_indices->resize(triangles_num * 3);
	int *r_indices_write = r_triangles_indices->ptrw();

	for (int i = 0; i < triangles_num; ++i) {
		r_indices_write[3 * i + 0] = triangles_read[i].indices[0];
		r_indices_write[3 * i + 1] = triangles_read[i].indices[1];
		r_indices_write[3 * i + 2] = triangles_read[i].indices[2];
	}
}

void TriangleMesh::create(const Vector<Vector3> &p_faces, const Vector<int32_t> &p_surface_indices) {
	valid = false;

	ERR_FAIL_COND(p_surface_indices.size() && p_surface_indices.size() != p_faces.size());

	int fc = p_faces.size();
	ERR_FAIL_COND(!fc || ((fc % 3) != 0));
	fc /= 3;
	triangles.resize(fc);

	bvh.resize(fc * 3); //will never be larger than this (todo make better)
	BVH *bw = bvh.ptrw();

	{
		//create faces and indices and base bvh
		//except for the Set for repeated triangles, everything
		//goes in-place.

		const Vector3 *r = p_faces.ptr();
		const int32_t *si = p_surface_indices.ptr();
		Triangle *w = triangles.ptrw();
		HashMap<Vector3, int> db;

		for (int i = 0; i < fc; i++) {
			Triangle &f = w[i];
			const Vector3 *v = &r[i * 3];

			for (int j = 0; j < 3; j++) {
				int vidx = -1;
				Vector3 vs = v[j].snappedf(0.0001);
				HashMap<Vector3, int>::Iterator E = db.find(vs);
				if (E) {
					vidx = E->value;
				} else {
					vidx = db.size();
					db[vs] = vidx;
				}

				f.indices[j] = vidx;
				if (j == 0) {
					bw[i].aabb.position = vs;
				} else {
					bw[i].aabb.expand_to(vs);
				}
			}

			f.normal = Face3(r[i * 3 + 0], r[i * 3 + 1], r[i * 3 + 2]).get_plane().get_normal();
			f.surface_index = si ? si[i] : 0;

			bw[i].left = -1;
			bw[i].right = -1;
			bw[i].face_index = i;
			bw[i].center = bw[i].aabb.get_center();
		}

		vertices.resize(db.size());
		Vector3 *vw = vertices.ptrw();
		for (const KeyValue<Vector3, int> &E : db) {
			vw[E.value] = E.key;
		}
	}

	Vector<BVH *> bwptrs;
	bwptrs.resize(fc);
	BVH **bwp = bwptrs.ptrw();
	for (int i = 0; i < fc; i++) {
		bwp[i] = &bw[i];
	}

	max_depth = 0;
	int max_alloc = fc;
	_create_bvh(bw, bwp, 0, fc, 1, max_depth, max_alloc);

	bvh.resize(max_alloc); //resize back

	valid = true;
}

bool TriangleMesh::intersect_segment(const Vector3 &p_begin, const Vector3 &p_end, Vector3 &r_point, Vector3 &r_normal, int32_t *r_surf_index) const {
	Vector3 segment = p_end - p_begin;
	real_t distance = segment.length();
	if (unlikely(distance == 0)) { // p_from and p_to are strictly identical.
		return false;
	}
	return intersect_ray(p_begin, segment / distance, r_point, r_normal, r_surf_index, distance);
}

bool TriangleMesh::intersect_ray(const Vector3 &p_begin, const Vector3 &p_dir, Vector3 &r_point, Vector3 &r_normal, int32_t *r_surf_index, real_t p_dist) const {
	uint32_t *stack = (uint32_t *)alloca(sizeof(int) * max_depth);

	enum {
		TEST_AABB_BIT = 0,
		VISIT_LEFT_BIT = 1,
		VISIT_RIGHT_BIT = 2,
		VISIT_DONE_BIT = 3,
		VISITED_BIT_SHIFT = 29,
		NODE_IDX_MASK = (1 << VISITED_BIT_SHIFT) - 1,
		VISITED_BIT_MASK = ~NODE_IDX_MASK,

	};

	Vector3 n = p_dir;
	real_t d = INFINITY;
	bool inters = false;

	int level = 0;

	const Triangle *triangleptr = triangles.ptr();
	const Vector3 *vertexptr = vertices.ptr();
	const BVH *bvhptr = bvh.ptr();

	int pos = bvh.size() - 1;

	stack[0] = pos;
	while (true) {
		uint32_t node = stack[level] & NODE_IDX_MASK;
		const BVH &b = bvhptr[node];
		bool done = false;

		switch (stack[level] >> VISITED_BIT_SHIFT) {
			case TEST_AABB_BIT: {
				bool inside = false;
				if (!b.aabb.find_intersects_ray(p_begin, p_dir, inside, nullptr, nullptr, p_dist)) {
					stack[level] = (VISIT_DONE_BIT << VISITED_BIT_SHIFT) | node;
				} else {
					if (b.face_index >= 0) {
						const Triangle &s = triangleptr[b.face_index];
						Face3 f3(vertexptr[s.indices[0]], vertexptr[s.indices[1]], vertexptr[s.indices[2]]);

						Vector3 res;

						if (f3.intersects_ray(p_begin, p_dir, &res, p_dist)) {
							real_t nd = n.dot(res);
							if (nd < d) {
								d = nd;
								r_point = res;
								r_normal = f3.get_plane().get_normal();
								if (r_surf_index) {
									*r_surf_index = s.surface_index;
								}
								inters = true;
							}
						}

						stack[level] = (VISIT_DONE_BIT << VISITED_BIT_SHIFT) | node;

					} else {
						stack[level] = (VISIT_LEFT_BIT << VISITED_BIT_SHIFT) | node;
					}
				}
				continue;
			}
			case VISIT_LEFT_BIT: {
				stack[level] = (VISIT_RIGHT_BIT << VISITED_BIT_SHIFT) | node;
				level++;
				stack[level] = b.left | TEST_AABB_BIT;
				continue;
			}
			case VISIT_RIGHT_BIT: {
				stack[level] = (VISIT_DONE_BIT << VISITED_BIT_SHIFT) | node;
				level++;
				stack[level] = b.right | TEST_AABB_BIT;
				continue;
			}
			case VISIT_DONE_BIT: {
				if (level == 0) {
					done = true;
					break;
				} else {
					level--;
				}
				continue;
			}
		}

		if (done) {
			break;
		}
	}

	if (inters) {
		if (n.dot(r_normal) > 0) {
			r_normal = -r_normal;
		}
	}

	return inters;
}

bool TriangleMesh::inside_convex_shape(const Plane *p_planes, int p_plane_count, const Vector3 *p_points, int p_point_count, Vector3 p_scale) const {
	uint32_t *stack = (uint32_t *)alloca(sizeof(int) * max_depth);

	enum {
		TEST_AABB_BIT = 0,
		VISIT_LEFT_BIT = 1,
		VISIT_RIGHT_BIT = 2,
		VISIT_DONE_BIT = 3,
		VISITED_BIT_SHIFT = 29,
		NODE_IDX_MASK = (1 << VISITED_BIT_SHIFT) - 1,
		VISITED_BIT_MASK = ~NODE_IDX_MASK,

	};

	int level = 0;

	const Triangle *triangleptr = triangles.ptr();
	const Vector3 *vertexptr = vertices.ptr();
	const BVH *bvhptr = bvh.ptr();

	Transform3D scale(Basis().scaled(p_scale));

	int pos = bvh.size() - 1;

	stack[0] = pos;
	while (true) {
		uint32_t node = stack[level] & NODE_IDX_MASK;
		const BVH &b = bvhptr[node];
		bool done = false;

		switch (stack[level] >> VISITED_BIT_SHIFT) {
			case TEST_AABB_BIT: {
				bool intersects = scale.xform(b.aabb).intersects_convex_shape(p_planes, p_plane_count, p_points, p_point_count);
				if (!intersects) {
					return false;
				}

				bool inside = scale.xform(b.aabb).inside_convex_shape(p_planes, p_plane_count);
				if (inside) {
					stack[level] = (VISIT_DONE_BIT << VISITED_BIT_SHIFT) | node;

				} else {
					if (b.face_index >= 0) {
						const Triangle &s = triangleptr[b.face_index];
						for (int j = 0; j < 3; ++j) {
							Vector3 point = scale.xform(vertexptr[s.indices[j]]);
							for (int i = 0; i < p_plane_count; i++) {
								const Plane &p = p_planes[i];
								if (p.is_point_over(point)) {
									return false;
								}
							}
						}

						stack[level] = (VISIT_DONE_BIT << VISITED_BIT_SHIFT) | node;

					} else {
						stack[level] = (VISIT_LEFT_BIT << VISITED_BIT_SHIFT) | node;
					}
				}
				continue;
			}
			case VISIT_LEFT_BIT: {
				stack[level] = (VISIT_RIGHT_BIT << VISITED_BIT_SHIFT) | node;
				level++;
				stack[level] = b.left | TEST_AABB_BIT;
				continue;
			}
			case VISIT_RIGHT_BIT: {
				stack[level] = (VISIT_DONE_BIT << VISITED_BIT_SHIFT) | node;
				level++;
				stack[level] = b.right | TEST_AABB_BIT;
				continue;
			}
			case VISIT_DONE_BIT: {
				if (level == 0) {
					done = true;
					break;
				} else {
					level--;
				}
				continue;
			}
		}

		if (done) {
			break;
		}
	}

	return true;
}

bool TriangleMesh::is_valid() const {
	return valid;
}

Vector<Face3> TriangleMesh::get_faces() const {
	if (!valid) {
		return Vector<Face3>();
	}

	Vector<Face3> faces;
	int ts = triangles.size();
	faces.resize(triangles.size());

	Face3 *w = faces.ptrw();
	const Triangle *r = triangles.ptr();
	const Vector3 *rv = vertices.ptr();

	for (int i = 0; i < ts; i++) {
		for (int j = 0; j < 3; j++) {
			w[i].vertex[j] = rv[r[i].indices[j]];
		}
	}

	return faces;
}

TriangleMesh::TriangleMesh() {
	valid = false;
	max_depth = 0;
}

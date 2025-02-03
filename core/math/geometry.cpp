/**************************************************************************/
/*  geometry.cpp                                                          */
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

#include "geometry.h"

#include "core/local_vector.h"
#include "core/print_string.h"

#include "thirdparty/misc/clipper.hpp"
#include "thirdparty/misc/triangulator.h"
#define STB_RECT_PACK_IMPLEMENTATION
#include "thirdparty/stb_rect_pack/stb_rect_pack.h"

#define SCALE_FACTOR 100000.0 // Based on CMP_EPSILON.

void Geometry::get_closest_points_between_segments(const Vector3 &p_p0, const Vector3 &p_p1, const Vector3 &p_q0, const Vector3 &p_q1, Vector3 &r_ps, Vector3 &r_qt) {
	// Based on David Eberly's Computation of Distance Between Line Segments algorithm.

	Vector3 p = p_p1 - p_p0;
	Vector3 q = p_q1 - p_q0;
	Vector3 r = p_p0 - p_q0;

	real_t a = p.dot(p);
	real_t b = p.dot(q);
	real_t c = q.dot(q);
	real_t d = p.dot(r);
	real_t e = q.dot(r);

	real_t s = 0.0f;
	real_t t = 0.0f;

	real_t det = a * c - b * b;
	if (det > CMP_EPSILON) {
		// Non-parallel segments
		real_t bte = b * e;
		real_t ctd = c * d;

		if (bte <= ctd) {
			// s <= 0.0f
			if (e <= 0.0f) {
				// t <= 0.0f
				s = (-d >= a ? 1 : (-d > 0.0f ? -d / a : 0.0f));
				t = 0.0f;
			} else if (e < c) {
				// 0.0f < t < 1
				s = 0.0f;
				t = e / c;
			} else {
				// t >= 1
				s = (b - d >= a ? 1 : (b - d > 0.0f ? (b - d) / a : 0.0f));
				t = 1;
			}
		} else {
			// s > 0.0f
			s = bte - ctd;
			if (s >= det) {
				// s >= 1
				if (b + e <= 0.0f) {
					// t <= 0.0f
					s = (-d <= 0.0f ? 0.0f : (-d < a ? -d / a : 1));
					t = 0.0f;
				} else if (b + e < c) {
					// 0.0f < t < 1
					s = 1;
					t = (b + e) / c;
				} else {
					// t >= 1
					s = (b - d <= 0.0f ? 0.0f : (b - d < a ? (b - d) / a : 1));
					t = 1;
				}
			} else {
				// 0.0f < s < 1
				real_t ate = a * e;
				real_t btd = b * d;

				if (ate <= btd) {
					// t <= 0.0f
					s = (-d <= 0.0f ? 0.0f : (-d >= a ? 1 : -d / a));
					t = 0.0f;
				} else {
					// t > 0.0f
					t = ate - btd;
					if (t >= det) {
						// t >= 1
						s = (b - d <= 0.0f ? 0.0f : (b - d >= a ? 1 : (b - d) / a));
						t = 1;
					} else {
						// 0.0f < t < 1
						s /= det;
						t /= det;
					}
				}
			}
		}
	} else {
		// Parallel segments
		if (e <= 0.0f) {
			s = (-d <= 0.0f ? 0.0f : (-d >= a ? 1 : -d / a));
			t = 0.0f;
		} else if (e >= c) {
			s = (b - d <= 0.0f ? 0.0f : (b - d >= a ? 1 : (b - d) / a));
			t = 1;
		} else {
			s = 0.0f;
			t = e / c;
		}
	}

	r_ps = (1 - s) * p_p0 + s * p_p1;
	r_qt = (1 - t) * p_q0 + t * p_q1;
}

real_t Geometry::get_closest_distance_between_segments(const Vector3 &p_p0, const Vector3 &p_p1, const Vector3 &p_q0, const Vector3 &p_q1) {
	Vector3 ps;
	Vector3 qt;
	get_closest_points_between_segments(p_p0, p_p1, p_q0, p_q1, ps, qt);
	Vector3 st = qt - ps;
	return st.length();
}

void Geometry::OccluderMeshData::clear() {
	faces.clear();
	vertices.clear();
}

void Geometry::MeshData::clear() {
	faces.clear();
	edges.clear();
	vertices.clear();
}

void Geometry::MeshData::optimize_vertices() {
	Map<int, int> vtx_remap;

	for (int i = 0; i < faces.size(); i++) {
		for (int j = 0; j < faces[i].indices.size(); j++) {
			int idx = faces[i].indices[j];
			if (!vtx_remap.has(idx)) {
				int ni = vtx_remap.size();
				vtx_remap[idx] = ni;
			}

			faces.write[i].indices.write[j] = vtx_remap[idx];
		}
	}

	for (int i = 0; i < edges.size(); i++) {
		int a = edges[i].a;
		int b = edges[i].b;

		if (!vtx_remap.has(a)) {
			int ni = vtx_remap.size();
			vtx_remap[a] = ni;
		}
		if (!vtx_remap.has(b)) {
			int ni = vtx_remap.size();
			vtx_remap[b] = ni;
		}

		edges.write[i].a = vtx_remap[a];
		edges.write[i].b = vtx_remap[b];
	}

	Vector<Vector3> new_vertices;
	new_vertices.resize(vtx_remap.size());

	for (int i = 0; i < vertices.size(); i++) {
		if (vtx_remap.has(i)) {
			new_vertices.write[vtx_remap[i]] = vertices[i];
		}
	}
	vertices = new_vertices;
}

struct _FaceClassify {
	struct _Link {
		int face;
		int edge;
		void clear() {
			face = -1;
			edge = -1;
		}
		_Link() {
			face = -1;
			edge = -1;
		}
	};
	bool valid;
	int group;
	_Link links[3];
	Face3 face;
	_FaceClassify() {
		group = -1;
		valid = false;
	};
};

static bool _connect_faces(_FaceClassify *p_faces, int len, int p_group) {
	// Connect faces, error will occur if an edge is shared between more than 2 faces.
	// Clear connections.

	bool error = false;

	for (int i = 0; i < len; i++) {
		for (int j = 0; j < 3; j++) {
			p_faces[i].links[j].clear();
		}
	}

	for (int i = 0; i < len; i++) {
		if (p_faces[i].group != p_group) {
			continue;
		}
		for (int j = i + 1; j < len; j++) {
			if (p_faces[j].group != p_group) {
				continue;
			}

			for (int k = 0; k < 3; k++) {
				Vector3 vi1 = p_faces[i].face.vertex[k];
				Vector3 vi2 = p_faces[i].face.vertex[(k + 1) % 3];

				for (int l = 0; l < 3; l++) {
					Vector3 vj2 = p_faces[j].face.vertex[l];
					Vector3 vj1 = p_faces[j].face.vertex[(l + 1) % 3];

					if (vi1.distance_to(vj1) < 0.00001f &&
							vi2.distance_to(vj2) < 0.00001f) {
						if (p_faces[i].links[k].face != -1) {
							ERR_PRINT("already linked\n");
							error = true;
							break;
						}
						if (p_faces[j].links[l].face != -1) {
							ERR_PRINT("already linked\n");
							error = true;
							break;
						}

						p_faces[i].links[k].face = j;
						p_faces[i].links[k].edge = l;
						p_faces[j].links[l].face = i;
						p_faces[j].links[l].edge = k;
					}
				}
				if (error) {
					break;
				}
			}
			if (error) {
				break;
			}
		}
		if (error) {
			break;
		}
	}

	for (int i = 0; i < len; i++) {
		p_faces[i].valid = true;
		for (int j = 0; j < 3; j++) {
			if (p_faces[i].links[j].face == -1) {
				p_faces[i].valid = false;
			}
		}
	}
	return error;
}

static bool _group_face(_FaceClassify *p_faces, int len, int p_index, int p_group) {
	if (p_faces[p_index].group >= 0) {
		return false;
	}

	p_faces[p_index].group = p_group;

	for (int i = 0; i < 3; i++) {
		ERR_FAIL_INDEX_V(p_faces[p_index].links[i].face, len, true);
		_group_face(p_faces, len, p_faces[p_index].links[i].face, p_group);
	}

	return true;
}

PoolVector<PoolVector<Face3>> Geometry::separate_objects(PoolVector<Face3> p_array) {
	PoolVector<PoolVector<Face3>> objects;

	int len = p_array.size();

	PoolVector<Face3>::Read r = p_array.read();

	const Face3 *arrayptr = r.ptr();

	PoolVector<_FaceClassify> fc;

	fc.resize(len);

	PoolVector<_FaceClassify>::Write fcw = fc.write();

	_FaceClassify *_fcptr = fcw.ptr();

	for (int i = 0; i < len; i++) {
		_fcptr[i].face = arrayptr[i];
	}

	bool error = _connect_faces(_fcptr, len, -1);

	ERR_FAIL_COND_V_MSG(error, PoolVector<PoolVector<Face3>>(), "Invalid geometry.");

	// Group connected faces in separate objects.

	int group = 0;
	for (int i = 0; i < len; i++) {
		if (!_fcptr[i].valid) {
			continue;
		}
		if (_group_face(_fcptr, len, i, group)) {
			group++;
		}
	}

	// Group connected faces in separate objects.

	for (int i = 0; i < len; i++) {
		_fcptr[i].face = arrayptr[i];
	}

	if (group >= 0) {
		objects.resize(group);
		PoolVector<PoolVector<Face3>>::Write obw = objects.write();
		PoolVector<Face3> *group_faces = obw.ptr();

		for (int i = 0; i < len; i++) {
			if (!_fcptr[i].valid) {
				continue;
			}
			if (_fcptr[i].group >= 0 && _fcptr[i].group < group) {
				group_faces[_fcptr[i].group].push_back(_fcptr[i].face);
			}
		}
	}

	return objects;
}

/*** GEOMETRY WRAPPER ***/

enum _CellFlags {

	_CELL_SOLID = 1,
	_CELL_EXTERIOR = 2,
	_CELL_STEP_MASK = 0x1C,
	_CELL_STEP_NONE = 0 << 2,
	_CELL_STEP_Y_POS = 1 << 2,
	_CELL_STEP_Y_NEG = 2 << 2,
	_CELL_STEP_X_POS = 3 << 2,
	_CELL_STEP_X_NEG = 4 << 2,
	_CELL_STEP_Z_POS = 5 << 2,
	_CELL_STEP_Z_NEG = 6 << 2,
	_CELL_STEP_DONE = 7 << 2,
	_CELL_PREV_MASK = 0xE0,
	_CELL_PREV_NONE = 0 << 5,
	_CELL_PREV_Y_POS = 1 << 5,
	_CELL_PREV_Y_NEG = 2 << 5,
	_CELL_PREV_X_POS = 3 << 5,
	_CELL_PREV_X_NEG = 4 << 5,
	_CELL_PREV_Z_POS = 5 << 5,
	_CELL_PREV_Z_NEG = 6 << 5,
	_CELL_PREV_FIRST = 7 << 5,

};

static inline void _plot_face(uint8_t ***p_cell_status, int x, int y, int z, int len_x, int len_y, int len_z, const Vector3 &voxelsize, const Face3 &p_face) {
	AABB aabb(Vector3(x, y, z), Vector3(len_x, len_y, len_z));
	aabb.position = aabb.position * voxelsize;
	aabb.size = aabb.size * voxelsize;

	if (!p_face.intersects_aabb(aabb)) {
		return;
	}

	if (len_x == 1 && len_y == 1 && len_z == 1) {
		p_cell_status[x][y][z] = _CELL_SOLID;
		return;
	}

	int div_x = len_x > 1 ? 2 : 1;
	int div_y = len_y > 1 ? 2 : 1;
	int div_z = len_z > 1 ? 2 : 1;

#define _SPLIT(m_i, m_div, m_v, m_len_v, m_new_v, m_new_len_v) \
	if (m_div == 1) {                                          \
		m_new_v = m_v;                                         \
		m_new_len_v = 1;                                       \
	} else if (m_i == 0) {                                     \
		m_new_v = m_v;                                         \
		m_new_len_v = m_len_v / 2;                             \
	} else {                                                   \
		m_new_v = m_v + m_len_v / 2;                           \
		m_new_len_v = m_len_v - m_len_v / 2;                   \
	}

	int new_x;
	int new_len_x;
	int new_y;
	int new_len_y;
	int new_z;
	int new_len_z;

	for (int i = 0; i < div_x; i++) {
		_SPLIT(i, div_x, x, len_x, new_x, new_len_x);

		for (int j = 0; j < div_y; j++) {
			_SPLIT(j, div_y, y, len_y, new_y, new_len_y);

			for (int k = 0; k < div_z; k++) {
				_SPLIT(k, div_z, z, len_z, new_z, new_len_z);

				_plot_face(p_cell_status, new_x, new_y, new_z, new_len_x, new_len_y, new_len_z, voxelsize, p_face);
			}
		}
	}
}

static inline void _mark_outside(uint8_t ***p_cell_status, int x, int y, int z, int len_x, int len_y, int len_z) {
	if (p_cell_status[x][y][z] & 3) {
		return; // Nothing to do, already used and/or visited.
	}

	p_cell_status[x][y][z] = _CELL_PREV_FIRST;

	while (true) {
		uint8_t &c = p_cell_status[x][y][z];

		if ((c & _CELL_STEP_MASK) == _CELL_STEP_NONE) {
			// Haven't been in here, mark as outside.
			p_cell_status[x][y][z] |= _CELL_EXTERIOR;
		}

		if ((c & _CELL_STEP_MASK) != _CELL_STEP_DONE) {
			// If not done, increase step.
			c += 1 << 2;
		}

		if ((c & _CELL_STEP_MASK) == _CELL_STEP_DONE) {
			// Go back.
			switch (c & _CELL_PREV_MASK) {
				case _CELL_PREV_FIRST: {
					return;
				} break;
				case _CELL_PREV_Y_POS: {
					y++;
					ERR_FAIL_COND(y >= len_y);
				} break;
				case _CELL_PREV_Y_NEG: {
					y--;
					ERR_FAIL_COND(y < 0);
				} break;
				case _CELL_PREV_X_POS: {
					x++;
					ERR_FAIL_COND(x >= len_x);
				} break;
				case _CELL_PREV_X_NEG: {
					x--;
					ERR_FAIL_COND(x < 0);
				} break;
				case _CELL_PREV_Z_POS: {
					z++;
					ERR_FAIL_COND(z >= len_z);
				} break;
				case _CELL_PREV_Z_NEG: {
					z--;
					ERR_FAIL_COND(z < 0);
				} break;
				default: {
					ERR_FAIL();
				}
			}
			continue;
		}

		int next_x = x, next_y = y, next_z = z;
		uint8_t prev = 0;

		switch (c & _CELL_STEP_MASK) {
			case _CELL_STEP_Y_POS: {
				next_y++;
				prev = _CELL_PREV_Y_NEG;
			} break;
			case _CELL_STEP_Y_NEG: {
				next_y--;
				prev = _CELL_PREV_Y_POS;
			} break;
			case _CELL_STEP_X_POS: {
				next_x++;
				prev = _CELL_PREV_X_NEG;
			} break;
			case _CELL_STEP_X_NEG: {
				next_x--;
				prev = _CELL_PREV_X_POS;
			} break;
			case _CELL_STEP_Z_POS: {
				next_z++;
				prev = _CELL_PREV_Z_NEG;
			} break;
			case _CELL_STEP_Z_NEG: {
				next_z--;
				prev = _CELL_PREV_Z_POS;
			} break;
			default:
				ERR_FAIL();
		}

		if (next_x < 0 || next_x >= len_x) {
			continue;
		}
		if (next_y < 0 || next_y >= len_y) {
			continue;
		}
		if (next_z < 0 || next_z >= len_z) {
			continue;
		}

		if (p_cell_status[next_x][next_y][next_z] & 3) {
			continue;
		}

		x = next_x;
		y = next_y;
		z = next_z;
		p_cell_status[x][y][z] |= prev;
	}
}

static inline void _build_faces(uint8_t ***p_cell_status, int x, int y, int z, int len_x, int len_y, int len_z, PoolVector<Face3> &p_faces) {
	ERR_FAIL_INDEX(x, len_x);
	ERR_FAIL_INDEX(y, len_y);
	ERR_FAIL_INDEX(z, len_z);

	if (p_cell_status[x][y][z] & _CELL_EXTERIOR) {
		return;
	}

#define vert(m_idx) Vector3(((m_idx)&4) >> 2, ((m_idx)&2) >> 1, (m_idx)&1)

	static const uint8_t indices[6][4] = {
		{ 7, 6, 4, 5 },
		{ 7, 3, 2, 6 },
		{ 7, 5, 1, 3 },
		{ 0, 2, 3, 1 },
		{ 0, 1, 5, 4 },
		{ 0, 4, 6, 2 },

	};

	for (int i = 0; i < 6; i++) {
		Vector3 face_points[4];
		int disp_x = x + ((i % 3) == 0 ? ((i < 3) ? 1 : -1) : 0);
		int disp_y = y + (((i - 1) % 3) == 0 ? ((i < 3) ? 1 : -1) : 0);
		int disp_z = z + (((i - 2) % 3) == 0 ? ((i < 3) ? 1 : -1) : 0);

		bool plot = false;

		if (disp_x < 0 || disp_x >= len_x) {
			plot = true;
		}
		if (disp_y < 0 || disp_y >= len_y) {
			plot = true;
		}
		if (disp_z < 0 || disp_z >= len_z) {
			plot = true;
		}

		if (!plot && (p_cell_status[disp_x][disp_y][disp_z] & _CELL_EXTERIOR)) {
			plot = true;
		}

		if (!plot) {
			continue;
		}

		for (int j = 0; j < 4; j++) {
			face_points[j] = vert(indices[i][j]) + Vector3(x, y, z);
		}

		p_faces.push_back(
				Face3(
						face_points[0],
						face_points[1],
						face_points[2]));

		p_faces.push_back(
				Face3(
						face_points[2],
						face_points[3],
						face_points[0]));
	}
}

PoolVector<Face3> Geometry::wrap_geometry(PoolVector<Face3> p_array, real_t *p_error) {
#define _MIN_SIZE 1.0f
#define _MAX_LENGTH 20

	int face_count = p_array.size();
	PoolVector<Face3>::Read facesr = p_array.read();
	const Face3 *faces = facesr.ptr();

	AABB global_aabb;

	for (int i = 0; i < face_count; i++) {
		if (i == 0) {
			global_aabb = faces[i].get_aabb();
		} else {
			global_aabb.merge_with(faces[i].get_aabb());
		}
	}

	global_aabb.grow_by(0.01f); // Avoid numerical error.

	// Determine amount of cells in grid axis.
	int div_x, div_y, div_z;

	if (global_aabb.size.x / _MIN_SIZE < _MAX_LENGTH) {
		div_x = (int)(global_aabb.size.x / _MIN_SIZE) + 1;
	} else {
		div_x = _MAX_LENGTH;
	}

	if (global_aabb.size.y / _MIN_SIZE < _MAX_LENGTH) {
		div_y = (int)(global_aabb.size.y / _MIN_SIZE) + 1;
	} else {
		div_y = _MAX_LENGTH;
	}

	if (global_aabb.size.z / _MIN_SIZE < _MAX_LENGTH) {
		div_z = (int)(global_aabb.size.z / _MIN_SIZE) + 1;
	} else {
		div_z = _MAX_LENGTH;
	}

	Vector3 voxelsize = global_aabb.size;
	voxelsize.x /= div_x;
	voxelsize.y /= div_y;
	voxelsize.z /= div_z;

	// Create and initialize cells to zero.

	uint8_t ***cell_status = memnew_arr(uint8_t **, div_x);
	for (int i = 0; i < div_x; i++) {
		cell_status[i] = memnew_arr(uint8_t *, div_y);

		for (int j = 0; j < div_y; j++) {
			cell_status[i][j] = memnew_arr(uint8_t, div_z);

			for (int k = 0; k < div_z; k++) {
				cell_status[i][j][k] = 0;
			}
		}
	}

	// Plot faces into cells.

	for (int i = 0; i < face_count; i++) {
		Face3 f = faces[i];
		for (int j = 0; j < 3; j++) {
			f.vertex[j] -= global_aabb.position;
		}
		_plot_face(cell_status, 0, 0, 0, div_x, div_y, div_z, voxelsize, f);
	}

	// Determine which cells connect to the outside by traversing the outside and recursively flood-fill marking.

	for (int i = 0; i < div_x; i++) {
		for (int j = 0; j < div_y; j++) {
			_mark_outside(cell_status, i, j, 0, div_x, div_y, div_z);
			_mark_outside(cell_status, i, j, div_z - 1, div_x, div_y, div_z);
		}
	}

	for (int i = 0; i < div_z; i++) {
		for (int j = 0; j < div_y; j++) {
			_mark_outside(cell_status, 0, j, i, div_x, div_y, div_z);
			_mark_outside(cell_status, div_x - 1, j, i, div_x, div_y, div_z);
		}
	}

	for (int i = 0; i < div_x; i++) {
		for (int j = 0; j < div_z; j++) {
			_mark_outside(cell_status, i, 0, j, div_x, div_y, div_z);
			_mark_outside(cell_status, i, div_y - 1, j, div_x, div_y, div_z);
		}
	}

	// Build faces for the inside-outside cell divisors.

	PoolVector<Face3> wrapped_faces;

	for (int i = 0; i < div_x; i++) {
		for (int j = 0; j < div_y; j++) {
			for (int k = 0; k < div_z; k++) {
				_build_faces(cell_status, i, j, k, div_x, div_y, div_z, wrapped_faces);
			}
		}
	}

	// Transform face vertices to global coords.

	int wrapped_faces_count = wrapped_faces.size();
	PoolVector<Face3>::Write wrapped_facesw = wrapped_faces.write();
	Face3 *wrapped_faces_ptr = wrapped_facesw.ptr();

	for (int i = 0; i < wrapped_faces_count; i++) {
		for (int j = 0; j < 3; j++) {
			Vector3 &v = wrapped_faces_ptr[i].vertex[j];
			v = v * voxelsize;
			v += global_aabb.position;
		}
	}

	// clean up grid

	for (int i = 0; i < div_x; i++) {
		for (int j = 0; j < div_y; j++) {
			memdelete_arr(cell_status[i][j]);
		}

		memdelete_arr(cell_status[i]);
	}

	memdelete_arr(cell_status);
	if (p_error) {
		*p_error = voxelsize.length();
	}

	return wrapped_faces;
}

Vector<Vector<Vector2>> Geometry::decompose_polygon_in_convex(Vector<Point2> polygon) {
	Vector<Vector<Vector2>> decomp;
	List<TriangulatorPoly> in_poly, out_poly;

	TriangulatorPoly inp;
	inp.Init(polygon.size());
	for (int i = 0; i < polygon.size(); i++) {
		inp.GetPoint(i) = polygon[i];
	}
	inp.SetOrientation(TRIANGULATOR_CCW);
	in_poly.push_back(inp);
	TriangulatorPartition tpart;
	if (tpart.ConvexPartition_HM(&in_poly, &out_poly) == 0) { // Failed.
		ERR_PRINT("Convex decomposing failed!");
		return decomp;
	}

	decomp.resize(out_poly.size());
	int idx = 0;
	for (List<TriangulatorPoly>::Element *I = out_poly.front(); I; I = I->next()) {
		TriangulatorPoly &tp = I->get();

		decomp.write[idx].resize(tp.GetNumPoints());

		for (int64_t i = 0; i < tp.GetNumPoints(); i++) {
			decomp.write[idx].write[i] = tp.GetPoint(i);
		}

		idx++;
	}

	return decomp;
}

Geometry::MeshData Geometry::build_convex_mesh(const PoolVector<Plane> &p_planes) {
	MeshData mesh;

#define SUBPLANE_SIZE 1024.0

	real_t subplane_size = 1024.0; // Should compute this from the actual plane.
	for (int i = 0; i < p_planes.size(); i++) {
		Plane p = p_planes[i];

		Vector3 ref = Vector3(0.0, 1.0, 0.0);

		if (ABS(p.normal.dot(ref)) > 0.95f) {
			ref = Vector3(0.0, 0.0, 1.0); // Change axis.
		}

		Vector3 right = p.normal.cross(ref).normalized();
		Vector3 up = p.normal.cross(right).normalized();

		Vector<Vector3> vertices;

		Vector3 center = p.get_any_point();
		// make a quad clockwise
		vertices.push_back(center - up * subplane_size + right * subplane_size);
		vertices.push_back(center - up * subplane_size - right * subplane_size);
		vertices.push_back(center + up * subplane_size - right * subplane_size);
		vertices.push_back(center + up * subplane_size + right * subplane_size);

		for (int j = 0; j < p_planes.size(); j++) {
			if (j == i) {
				continue;
			}

			Vector<Vector3> new_vertices;
			Plane clip = p_planes[j];

			if (clip.normal.dot(p.normal) > 0.95f) {
				continue;
			}

			if (vertices.size() < 3) {
				break;
			}

			for (int k = 0; k < vertices.size(); k++) {
				int k_n = (k + 1) % vertices.size();

				Vector3 edge0_A = vertices[k];
				Vector3 edge1_A = vertices[k_n];

				real_t dist0 = clip.distance_to(edge0_A);
				real_t dist1 = clip.distance_to(edge1_A);

				if (dist0 <= 0) { // Behind plane.

					new_vertices.push_back(vertices[k]);
				}

				// Check for different sides and non coplanar.
				if ((dist0 * dist1) < 0) {
					// Calculate intersection.
					Vector3 rel = edge1_A - edge0_A;

					real_t den = clip.normal.dot(rel);
					if (Math::is_zero_approx(den)) {
						continue; // Point too short.
					}

					real_t dist = -(clip.normal.dot(edge0_A) - clip.d) / den;
					Vector3 inters = edge0_A + rel * dist;
					new_vertices.push_back(inters);
				}
			}

			vertices = new_vertices;
		}

		if (vertices.size() < 3) {
			continue;
		}

		// Result is a clockwise face.

		MeshData::Face face;

		// Add face indices.
		for (int j = 0; j < vertices.size(); j++) {
			int idx = -1;
			for (int k = 0; k < mesh.vertices.size(); k++) {
				if (mesh.vertices[k].distance_to(vertices[j]) < 0.001f) {
					idx = k;
					break;
				}
			}

			if (idx == -1) {
				idx = mesh.vertices.size();
				mesh.vertices.push_back(vertices[j]);
			}

			face.indices.push_back(idx);
		}
		face.plane = p;
		mesh.faces.push_back(face);

		// Add edge.

		for (int j = 0; j < face.indices.size(); j++) {
			int a = face.indices[j];
			int b = face.indices[(j + 1) % face.indices.size()];

			bool found = false;
			for (int k = 0; k < mesh.edges.size(); k++) {
				if (mesh.edges[k].a == a && mesh.edges[k].b == b) {
					found = true;
					break;
				}
				if (mesh.edges[k].b == a && mesh.edges[k].a == b) {
					found = true;
					break;
				}
			}

			if (found) {
				continue;
			}
			MeshData::Edge edge;
			edge.a = a;
			edge.b = b;
			mesh.edges.push_back(edge);
		}
	}

	return mesh;
}

PoolVector<Plane> Geometry::build_box_planes(const Vector3 &p_extents) {
	PoolVector<Plane> planes;

	planes.push_back(Plane(Vector3(1, 0, 0), p_extents.x));
	planes.push_back(Plane(Vector3(-1, 0, 0), p_extents.x));
	planes.push_back(Plane(Vector3(0, 1, 0), p_extents.y));
	planes.push_back(Plane(Vector3(0, -1, 0), p_extents.y));
	planes.push_back(Plane(Vector3(0, 0, 1), p_extents.z));
	planes.push_back(Plane(Vector3(0, 0, -1), p_extents.z));

	return planes;
}

PoolVector<Plane> Geometry::build_cylinder_planes(real_t p_radius, real_t p_height, int p_sides, Vector3::Axis p_axis) {
	ERR_FAIL_INDEX_V(p_axis, 3, PoolVector<Plane>());

	PoolVector<Plane> planes;

	for (int i = 0; i < p_sides; i++) {
		Vector3 normal;
		normal[(p_axis + 1) % 3] = Math::cos(i * (real_t)(2.0 * Math_PI) / p_sides);
		normal[(p_axis + 2) % 3] = Math::sin(i * (real_t)(2.0 * Math_PI) / p_sides);

		planes.push_back(Plane(normal, p_radius));
	}

	Vector3 axis;
	axis[p_axis] = 1.0;

	planes.push_back(Plane(axis, p_height * 0.5f));
	planes.push_back(Plane(-axis, p_height * 0.5f));

	return planes;
}

PoolVector<Plane> Geometry::build_sphere_planes(real_t p_radius, int p_lats, int p_lons, Vector3::Axis p_axis) {
	ERR_FAIL_INDEX_V(p_axis, 3, PoolVector<Plane>());

	PoolVector<Plane> planes;

	Vector3 axis;
	axis[p_axis] = 1;

	Vector3 axis_neg;
	axis_neg[(p_axis + 1) % 3] = 1;
	axis_neg[(p_axis + 2) % 3] = 1;
	axis_neg[p_axis] = -1;

	for (int i = 0; i < p_lons; i++) {
		Vector3 normal;
		normal[(p_axis + 1) % 3] = Math::cos(i * (real_t)(2.0 * Math_PI) / p_lons);
		normal[(p_axis + 2) % 3] = Math::sin(i * (real_t)(2.0 * Math_PI) / p_lons);

		planes.push_back(Plane(normal, p_radius));

		for (int j = 1; j <= p_lats; j++) {
			// FIXME: This is stupid.
			Vector3 angle = normal.linear_interpolate(axis, j / (real_t)p_lats).normalized();
			Vector3 pos = angle * p_radius;
			planes.push_back(Plane(pos, angle));
			planes.push_back(Plane(pos * axis_neg, angle * axis_neg));
		}
	}

	return planes;
}

PoolVector<Plane> Geometry::build_capsule_planes(real_t p_radius, real_t p_height, int p_sides, int p_lats, Vector3::Axis p_axis) {
	ERR_FAIL_INDEX_V(p_axis, 3, PoolVector<Plane>());

	PoolVector<Plane> planes;

	Vector3 axis;
	axis[p_axis] = 1;

	Vector3 axis_neg;
	axis_neg[(p_axis + 1) % 3] = 1;
	axis_neg[(p_axis + 2) % 3] = 1;
	axis_neg[p_axis] = -1;

	for (int i = 0; i < p_sides; i++) {
		Vector3 normal;
		normal[(p_axis + 1) % 3] = Math::cos(i * (real_t)(2.0 * Math_PI) / p_sides);
		normal[(p_axis + 2) % 3] = Math::sin(i * (real_t)(2.0 * Math_PI) / p_sides);

		planes.push_back(Plane(normal, p_radius));

		for (int j = 1; j <= p_lats; j++) {
			Vector3 angle = normal.linear_interpolate(axis, j / (real_t)p_lats).normalized();
			Vector3 pos = axis * p_height * 0.5f + angle * p_radius;
			planes.push_back(Plane(pos, angle));
			planes.push_back(Plane(pos * axis_neg, angle * axis_neg));
		}
	}

	return planes;
}

struct _AtlasWorkRect {
	Size2i s;
	Point2i p;
	int idx;
	_FORCE_INLINE_ bool operator<(const _AtlasWorkRect &p_r) const { return s.width > p_r.s.width; }
};

struct _AtlasWorkRectResult {
	Vector<_AtlasWorkRect> result;
	int max_w;
	int max_h;
};

void Geometry::make_atlas(const Vector<Size2i> &p_rects, Vector<Point2i> &r_result, Size2i &r_size) {
	// Super simple, almost brute force scanline stacking fitter.
	// It's pretty basic for now, but it tries to make sure that the aspect ratio of the
	// resulting atlas is somehow square. This is necessary because video cards have limits.
	// On texture size (usually 2048 or 4096), so the more square a texture, the more chances.
	// It will work in every hardware.
	// For example, it will prioritize a 1024x1024 atlas (works everywhere) instead of a
	// 256x8192 atlas (won't work anywhere).

	ERR_FAIL_COND(p_rects.size() == 0);
	for (int i = 0; i < p_rects.size(); i++) {
		ERR_FAIL_COND(p_rects[i].width <= 0);
		ERR_FAIL_COND(p_rects[i].height <= 0);
	}

	Vector<_AtlasWorkRect> wrects;
	wrects.resize(p_rects.size());
	for (int i = 0; i < p_rects.size(); i++) {
		wrects.write[i].s = p_rects[i];
		wrects.write[i].idx = i;
	}
	wrects.sort();
	int widest = wrects[0].s.width;

	Vector<_AtlasWorkRectResult> results;

	for (int i = 0; i <= 12; i++) {
		int w = 1 << i;
		int max_h = 0;
		int max_w = 0;
		if (w < widest) {
			continue;
		}

		Vector<int> hmax;
		hmax.resize(w);
		for (int j = 0; j < w; j++) {
			hmax.write[j] = 0;
		}

		// Place them.
		int ofs = 0;
		int limit_h = 0;
		for (int j = 0; j < wrects.size(); j++) {
			if (ofs + wrects[j].s.width > w) {
				ofs = 0;
			}

			int from_y = 0;
			for (int k = 0; k < wrects[j].s.width; k++) {
				if (hmax[ofs + k] > from_y) {
					from_y = hmax[ofs + k];
				}
			}

			wrects.write[j].p.x = ofs;
			wrects.write[j].p.y = from_y;
			int end_h = from_y + wrects[j].s.height;
			int end_w = ofs + wrects[j].s.width;
			if (ofs == 0) {
				limit_h = end_h;
			}

			for (int k = 0; k < wrects[j].s.width; k++) {
				hmax.write[ofs + k] = end_h;
			}

			if (end_h > max_h) {
				max_h = end_h;
			}

			if (end_w > max_w) {
				max_w = end_w;
			}

			if (ofs == 0 || end_h > limit_h) { // While h limit not reached, keep stacking.
				ofs += wrects[j].s.width;
			}
		}

		_AtlasWorkRectResult result;
		result.result = wrects;
		result.max_h = max_h;
		result.max_w = max_w;
		results.push_back(result);
	}

	// Find the result with the best aspect ratio.

	int best = -1;
	real_t best_aspect = 1e20;

	for (int i = 0; i < results.size(); i++) {
		real_t h = next_power_of_2(results[i].max_h);
		real_t w = next_power_of_2(results[i].max_w);
		real_t aspect = h > w ? h / w : w / h;
		if (aspect < best_aspect) {
			best = i;
			best_aspect = aspect;
		}
	}

	r_result.resize(p_rects.size());

	for (int i = 0; i < p_rects.size(); i++) {
		r_result.write[results[best].result[i].idx] = results[best].result[i].p;
	}

	r_size = Size2(results[best].max_w, results[best].max_h);
}

Vector<Vector<Point2>> Geometry::_polypaths_do_operation(PolyBooleanOperation p_op, const Vector<Point2> &p_polypath_a, const Vector<Point2> &p_polypath_b, bool is_a_open) {
	using namespace ClipperLib;

	ClipType op = ctUnion;

	switch (p_op) {
		case OPERATION_UNION:
			op = ctUnion;
			break;
		case OPERATION_DIFFERENCE:
			op = ctDifference;
			break;
		case OPERATION_INTERSECTION:
			op = ctIntersection;
			break;
		case OPERATION_XOR:
			op = ctXor;
			break;
	}
	Path path_a, path_b;

	// Need to scale points (Clipper's requirement for robust computation).
	for (int i = 0; i != p_polypath_a.size(); ++i) {
		path_a << IntPoint(p_polypath_a[i].x * (real_t)SCALE_FACTOR, p_polypath_a[i].y * (real_t)SCALE_FACTOR);
	}
	for (int i = 0; i != p_polypath_b.size(); ++i) {
		path_b << IntPoint(p_polypath_b[i].x * (real_t)SCALE_FACTOR, p_polypath_b[i].y * (real_t)SCALE_FACTOR);
	}
	Clipper clp;
	clp.AddPath(path_a, ptSubject, !is_a_open); // Forward compatible with Clipper 10.0.0.
	clp.AddPath(path_b, ptClip, true); // Polylines cannot be set as clip.

	Paths paths;

	if (is_a_open) {
		PolyTree tree; // Needed to populate polylines.
		clp.Execute(op, tree);
		OpenPathsFromPolyTree(tree, paths);
	} else {
		clp.Execute(op, paths); // Works on closed polygons only.
	}
	// Have to scale points down now.
	Vector<Vector<Point2>> polypaths;

	for (Paths::size_type i = 0; i < paths.size(); ++i) {
		Vector<Vector2> polypath;

		const Path &scaled_path = paths[i];

		for (Paths::size_type j = 0; j < scaled_path.size(); ++j) {
			polypath.push_back(Point2(
					static_cast<real_t>(scaled_path[j].X) / (real_t)SCALE_FACTOR,
					static_cast<real_t>(scaled_path[j].Y) / (real_t)SCALE_FACTOR));
		}
		polypaths.push_back(polypath);
	}
	return polypaths;
}

Vector<Vector<Point2>> Geometry::_polypath_offset(const Vector<Point2> &p_polypath, real_t p_delta, PolyJoinType p_join_type, PolyEndType p_end_type) {
	using namespace ClipperLib;

	JoinType jt = jtSquare;

	switch (p_join_type) {
		case JOIN_SQUARE:
			jt = jtSquare;
			break;
		case JOIN_ROUND:
			jt = jtRound;
			break;
		case JOIN_MITER:
			jt = jtMiter;
			break;
	}

	EndType et = etClosedPolygon;

	switch (p_end_type) {
		case END_POLYGON:
			et = etClosedPolygon;
			break;
		case END_JOINED:
			et = etClosedLine;
			break;
		case END_BUTT:
			et = etOpenButt;
			break;
		case END_SQUARE:
			et = etOpenSquare;
			break;
		case END_ROUND:
			et = etOpenRound;
			break;
	}
	ClipperOffset co(2.0f, 0.25f * (real_t)SCALE_FACTOR); // Defaults from ClipperOffset.
	Path path;

	// Need to scale points (Clipper's requirement for robust computation).
	for (int i = 0; i != p_polypath.size(); ++i) {
		path << IntPoint(p_polypath[i].x * (real_t)SCALE_FACTOR, p_polypath[i].y * (real_t)SCALE_FACTOR);
	}
	co.AddPath(path, jt, et);

	Paths paths;
	co.Execute(paths, p_delta * (real_t)SCALE_FACTOR); // Inflate/deflate.

	// Have to scale points down now.
	Vector<Vector<Point2>> polypaths;

	for (Paths::size_type i = 0; i < paths.size(); ++i) {
		Vector<Vector2> polypath;

		const Path &scaled_path = paths[i];

		for (Paths::size_type j = 0; j < scaled_path.size(); ++j) {
			polypath.push_back(Point2(
					static_cast<real_t>(scaled_path[j].X) / (real_t)SCALE_FACTOR,
					static_cast<real_t>(scaled_path[j].Y) / (real_t)SCALE_FACTOR));
		}
		polypaths.push_back(polypath);
	}
	return polypaths;
}

Vector<Vector<Point2>> Geometry::_polypath_minkowski(const Vector<Point2> &p_polypath_pattern, const Vector<Point2> &p_polypath_path, bool p_is_sum, bool p_is_closed) {
	using namespace ClipperLib;

	Path path_pattern;
	Path path_path;
	path_pattern.reserve(p_polypath_pattern.size());
	path_path.reserve(p_polypath_path.size());

	// Need to scale points (Clipper's requirement for robust computation).
	for (int i = 0; i < p_polypath_pattern.size(); ++i) {
		path_pattern << IntPoint(p_polypath_pattern[i].x * (real_t)SCALE_FACTOR, p_polypath_pattern[i].y * (real_t)SCALE_FACTOR);
	}
	for (int i = 0; i < p_polypath_path.size(); ++i) {
		path_path << IntPoint(p_polypath_path[i].x * (real_t)SCALE_FACTOR, p_polypath_path[i].y * (real_t)SCALE_FACTOR);
	}

	Paths paths;

	if (p_is_sum) {
		MinkowskiSum(path_pattern, path_path, paths, p_is_closed);
	} else {
		MinkowskiDiff(path_pattern, path_path, paths);
	}

	// Have to scale points down now.
	Vector<Vector<Point2>> polypaths;
	polypaths.resize(paths.size());

	for (Paths::size_type i = 0; i < paths.size(); ++i) {
		const Path &scaled_path = paths[i];

		Vector<Vector2> polypath;
		polypath.resize(scaled_path.size());

		for (Paths::size_type j = 0; j < scaled_path.size(); ++j) {
			polypath.set(j, Point2(static_cast<real_t>(scaled_path[j].X) / (real_t)SCALE_FACTOR, static_cast<real_t>(scaled_path[j].Y) / (real_t)SCALE_FACTOR));
		}
		polypaths.set(i, polypath);
	}
	return polypaths;
}

real_t Geometry::calculate_convex_hull_volume(const Geometry::MeshData &p_md) {
	if (!p_md.vertices.size()) {
		return 0;
	}

	// find center
	Vector3 center;
	for (int n = 0; n < p_md.vertices.size(); n++) {
		center += p_md.vertices[n];
	}
	center /= p_md.vertices.size();

	Face3 fa;

	real_t volume = 0.0;

	// volume of each cone is 1/3 * height * area of face
	for (int f = 0; f < p_md.faces.size(); f++) {
		const Geometry::MeshData::Face &face = p_md.faces[f];

		real_t height = 0.0;
		real_t face_area = 0.0;

		for (int c = 0; c < face.indices.size() - 2; c++) {
			fa.vertex[0] = p_md.vertices[face.indices[0]];
			fa.vertex[1] = p_md.vertices[face.indices[c + 1]];
			fa.vertex[2] = p_md.vertices[face.indices[c + 2]];

			if (!c) {
				// calculate height
				Plane plane(fa.vertex[0], fa.vertex[1], fa.vertex[2]);
				height = -plane.distance_to(center);
			}

			face_area += Math::sqrt(fa.get_twice_area_squared());
		}
		volume += face_area * height;
	}

	volume *= (real_t)((1.0 / 3.0) * 0.5);
	return volume;
}

// note this function is slow, because it builds meshes etc. Not ideal to use in realtime.
// Planes must face OUTWARD from the center of the convex hull, by convention.
bool Geometry::convex_hull_intersects_convex_hull(const Plane *p_planes_a, int p_plane_count_a, const Plane *p_planes_b, int p_plane_count_b) {
	if (!p_plane_count_a || !p_plane_count_b) {
		return false;
	}

	// OR alternative approach, we can call compute_convex_mesh_points()
	// with both sets of planes, to get an intersection. Not sure which method is
	// faster... this may be faster with more complex hulls.

	// the usual silliness to get from one vector format to another...
	PoolVector<Plane> planes_a;
	PoolVector<Plane> planes_b;

	{
		planes_a.resize(p_plane_count_a);
		PoolVector<Plane>::Write w = planes_a.write();
		memcpy(w.ptr(), p_planes_a, p_plane_count_a * sizeof(Plane));
	}
	{
		planes_b.resize(p_plane_count_b);
		PoolVector<Plane>::Write w = planes_b.write();
		memcpy(w.ptr(), p_planes_b, p_plane_count_b * sizeof(Plane));
	}

	Geometry::MeshData md_A = build_convex_mesh(planes_a);
	Geometry::MeshData md_B = build_convex_mesh(planes_b);

	// hull can't be built
	if (!md_A.vertices.size() || !md_B.vertices.size()) {
		return false;
	}

	// first check the points against the planes
	for (int p = 0; p < p_plane_count_a; p++) {
		const Plane &plane = p_planes_a[p];

		for (int n = 0; n < md_B.vertices.size(); n++) {
			if (!plane.is_point_over(md_B.vertices[n])) {
				return true;
			}
		}
	}

	for (int p = 0; p < p_plane_count_b; p++) {
		const Plane &plane = p_planes_b[p];

		for (int n = 0; n < md_A.vertices.size(); n++) {
			if (!plane.is_point_over(md_A.vertices[n])) {
				return true;
			}
		}
	}

	// now check edges
	for (int n = 0; n < md_A.edges.size(); n++) {
		const Vector3 &pt_a = md_A.vertices[md_A.edges[n].a];
		const Vector3 &pt_b = md_A.vertices[md_A.edges[n].b];

		if (segment_intersects_convex(pt_a, pt_b, p_planes_b, p_plane_count_b, nullptr, nullptr)) {
			return true;
		}
	}

	for (int n = 0; n < md_B.edges.size(); n++) {
		const Vector3 &pt_a = md_B.vertices[md_B.edges[n].a];
		const Vector3 &pt_b = md_B.vertices[md_B.edges[n].b];

		if (segment_intersects_convex(pt_a, pt_b, p_planes_a, p_plane_count_a, nullptr, nullptr)) {
			return true;
		}
	}

	return false;
}

Vector<Vector3> Geometry::compute_convex_mesh_points(const Plane *p_planes, int p_plane_count, real_t p_epsilon) {
	Vector<Vector3> points;

	// Iterate through every unique combination of any three planes.
	for (int i = p_plane_count - 1; i >= 0; i--) {
		for (int j = i - 1; j >= 0; j--) {
			for (int k = j - 1; k >= 0; k--) {
				// Find the point where these planes all cross over (if they
				// do at all).
				Vector3 convex_shape_point;
				if (p_planes[i].intersect_3(p_planes[j], p_planes[k], &convex_shape_point)) {
					// See if any *other* plane excludes this point because it's
					// on the wrong side.
					bool excluded = false;
					for (int n = 0; n < p_plane_count; n++) {
						if (n != i && n != j && n != k) {
							real_t dist = p_planes[n].distance_to(convex_shape_point);
							if (dist > p_epsilon) {
								excluded = true;
								break;
							}
						}
					}

					// Only add the point if it passed all tests.
					if (!excluded) {
						points.push_back(convex_shape_point);
					}
				}
			}
		}
	}

	return points;
}

Vector<Geometry::PackRectsResult> Geometry::partial_pack_rects(const Vector<Vector2i> &p_sizes, const Size2i &p_atlas_size) {
	Vector<stbrp_node> nodes;
	nodes.resize(p_atlas_size.width);
	memset(nodes.ptrw(), 0, sizeof(stbrp_node) * nodes.size());

	stbrp_context context;
	stbrp_init_target(&context, p_atlas_size.width, p_atlas_size.height, nodes.ptrw(), p_atlas_size.width);

	Vector<stbrp_rect> rects;
	rects.resize(p_sizes.size());

	for (int i = 0; i < p_sizes.size(); i++) {
		rects.write[i].id = i;
		rects.write[i].w = p_sizes[i].width;
		rects.write[i].h = p_sizes[i].height;
		rects.write[i].x = 0;
		rects.write[i].y = 0;
		rects.write[i].was_packed = 0;
	}

	stbrp_pack_rects(&context, rects.ptrw(), rects.size());

	Vector<PackRectsResult> ret;
	ret.resize(p_sizes.size());

	for (int i = 0; i < p_sizes.size(); i++) {
		ret.write[rects[i].id] = { rects[i].x, rects[i].y, static_cast<bool>(rects[i].was_packed) };
	}

	return ret;
}

// Expects polygon as a triangle fan
real_t Geometry::find_polygon_area(const Vector3 *p_verts, int p_num_verts) {
	if (!p_verts || (p_num_verts < 3)) {
		return 0.0;
	}

	Face3 f;
	f.vertex[0] = p_verts[0];
	f.vertex[1] = p_verts[1];
	f.vertex[2] = p_verts[1];

	real_t area = 0.0;

	for (int n = 2; n < p_num_verts; n++) {
		f.vertex[1] = f.vertex[2];
		f.vertex[2] = p_verts[n];
		area += Math::sqrt(f.get_twice_area_squared());
	}

	return area * 0.5f;
}

// adapted from:
// https://stackoverflow.com/questions/6989100/sort-points-in-clockwise-order
void Geometry::sort_polygon_winding(Vector<Vector2> &r_verts, bool p_clockwise) {
	// sort winding order of a (primarily convex) polygon.
	// It can handle some concave polygons, but not
	// where a vertex 'goes back on' a previous vertex ..
	// i.e. it will change the shape in some concave cases.
	struct ElementComparator {
		Vector2 center;
		bool operator()(const Vector2 &a, const Vector2 &b) const {
			if (a.x - center.x >= 0 && b.x - center.x < 0) {
				return true;
			}
			if (a.x - center.x < 0 && b.x - center.x >= 0) {
				return false;
			}
			if (a.x - center.x == 0 && b.x - center.x == 0) {
				if (a.y - center.y >= 0 || b.y - center.y >= 0) {
					return a.y > b.y;
				}
				return b.y > a.y;
			}

			// compute the cross product of vectors (center -> a) x (center -> b)
			real_t det = (a.x - center.x) * (b.y - center.y) - (b.x - center.x) * (a.y - center.y);
			if (det < 0) {
				return true;
			}
			if (det > 0) {
				return false;
			}

			// points a and b are on the same line from the center
			// check which point is closer to the center
			real_t d1 = (a.x - center.x) * (a.x - center.x) + (a.y - center.y) * (a.y - center.y);
			real_t d2 = (b.x - center.x) * (b.x - center.x) + (b.y - center.y) * (b.y - center.y);
			return d1 > d2;
		}
	};

	int npoints = r_verts.size();
	if (!npoints) {
		return;
	}

	// first calculate center
	Vector2 center;
	for (int n = 0; n < npoints; n++) {
		center += r_verts[n];
	}
	center /= npoints;

	SortArray<Vector2, ElementComparator> sorter;
	sorter.compare.center = center;
	sorter.sort(r_verts.ptrw(), r_verts.size());

	// if not clockwise, reverse order
	if (!p_clockwise) {
		r_verts.invert();
	}
}

bool Geometry::verify_indices(const int *p_indices, int p_num_indices, int p_num_vertices) {
	ERR_FAIL_NULL_V(p_indices, false);
	ERR_FAIL_COND_V(p_num_indices < 0, false);
	ERR_FAIL_COND_V(p_num_vertices < 0, false);

	for (int n = 0; n < p_num_indices; n++) {
		if ((unsigned int)p_indices[n] >= (unsigned int)p_num_vertices) {
			return false;
		}
	}

	return true;
}

/**************************************************************************/
/*  godot_soft_body_3d.cpp                                                */
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

#include "godot_soft_body_3d.h"

#include "godot_space_3d.h"

#include "core/math/geometry_3d.h"
#include "servers/rendering_server.h"

// Based on Bullet soft body.

/*
Bullet Continuous Collision Detection and Physics Library
Copyright (c) 2003-2006 Erwin Coumans  http://continuousphysics.com/Bullet/

This software is provided 'as-is', without any express or implied warranty.
In no event will the authors be held liable for any damages arising from the use of this software.
Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it freely,
subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not claim that you wrote the original software. If you use this software in a product, an acknowledgment in the product documentation would be appreciated but is not required.
2. Altered source versions must be plainly marked as such, and must not be misrepresented as being the original software.
3. This notice may not be removed or altered from any source distribution.
*/
///btSoftBody implementation by Nathanael Presson

GodotSoftBody3D::GodotSoftBody3D() :
		GodotCollisionObject3D(TYPE_SOFT_BODY),
		active_list(this) {
	_set_static(false);
}

void GodotSoftBody3D::_shapes_changed() {
}

void GodotSoftBody3D::set_state(PhysicsServer3D::BodyState p_state, const Variant &p_variant) {
	switch (p_state) {
		case PhysicsServer3D::BODY_STATE_TRANSFORM: {
			_set_transform(p_variant);
			_set_inv_transform(get_transform().inverse());

			apply_nodes_transform(get_transform());

		} break;
		case PhysicsServer3D::BODY_STATE_LINEAR_VELOCITY: {
			// Not supported.
			ERR_FAIL_MSG("Linear velocity is not supported for Soft bodies.");
		} break;
		case PhysicsServer3D::BODY_STATE_ANGULAR_VELOCITY: {
			ERR_FAIL_MSG("Angular velocity is not supported for Soft bodies.");
		} break;
		case PhysicsServer3D::BODY_STATE_SLEEPING: {
			ERR_FAIL_MSG("Sleeping state is not supported for Soft bodies.");
		} break;
		case PhysicsServer3D::BODY_STATE_CAN_SLEEP: {
			ERR_FAIL_MSG("Sleeping state is not supported for Soft bodies.");
		} break;
	}
}

Variant GodotSoftBody3D::get_state(PhysicsServer3D::BodyState p_state) const {
	switch (p_state) {
		case PhysicsServer3D::BODY_STATE_TRANSFORM: {
			return get_transform();
		} break;
		case PhysicsServer3D::BODY_STATE_LINEAR_VELOCITY: {
			ERR_FAIL_V_MSG(Vector3(), "Linear velocity is not supported for Soft bodies.");
		} break;
		case PhysicsServer3D::BODY_STATE_ANGULAR_VELOCITY: {
			ERR_FAIL_V_MSG(Vector3(), "Angular velocity is not supported for Soft bodies.");
		} break;
		case PhysicsServer3D::BODY_STATE_SLEEPING: {
			ERR_FAIL_V_MSG(false, "Sleeping state is not supported for Soft bodies.");
		} break;
		case PhysicsServer3D::BODY_STATE_CAN_SLEEP: {
			ERR_FAIL_V_MSG(false, "Sleeping state is not supported for Soft bodies.");
		} break;
	}

	return Variant();
}

void GodotSoftBody3D::set_space(GodotSpace3D *p_space) {
	if (get_space()) {
		get_space()->soft_body_remove_from_active_list(&active_list);

		deinitialize_shape();
	}

	_set_space(p_space);

	if (get_space()) {
		get_space()->soft_body_add_to_active_list(&active_list);

		if (bounds != AABB()) {
			initialize_shape(true);
		}
	}
}

void GodotSoftBody3D::set_mesh(RID p_mesh) {
	destroy();

	soft_mesh = p_mesh;

	if (soft_mesh.is_null()) {
		return;
	}

	Array arrays = RenderingServer::get_singleton()->mesh_surface_get_arrays(soft_mesh, 0);
	ERR_FAIL_COND(arrays.is_empty());

	const Vector<int> &indices = arrays[RenderingServer::ARRAY_INDEX];
	const Vector<Vector3> &vertices = arrays[RenderingServer::ARRAY_VERTEX];
	ERR_FAIL_COND_MSG(indices.is_empty(), "Soft body's mesh needs to have indices");
	ERR_FAIL_COND_MSG(vertices.is_empty(), "Soft body's mesh needs to have vertices");

	bool success = create_from_trimesh(indices, vertices);
	if (!success) {
		destroy();
	}
}

void GodotSoftBody3D::update_rendering_server(PhysicsServer3DRenderingServerHandler *p_rendering_server_handler) {
	if (soft_mesh.is_null()) {
		return;
	}

	const uint32_t vertex_count = map_visual_to_physics.size();
	for (uint32_t i = 0; i < vertex_count; ++i) {
		const uint32_t node_index = map_visual_to_physics[i];
		const Node &node = nodes[node_index];

		p_rendering_server_handler->set_vertex(i, node.x);
		p_rendering_server_handler->set_normal(i, node.n);
	}

	p_rendering_server_handler->set_aabb(bounds);
}

void GodotSoftBody3D::update_normals_and_centroids() {
	for (Node &node : nodes) {
		node.n = Vector3();
	}

	for (Face &face : faces) {
		const Vector3 n = vec3_cross(face.n[0]->x - face.n[2]->x, face.n[0]->x - face.n[1]->x);
		face.n[0]->n += n;
		face.n[1]->n += n;
		face.n[2]->n += n;
		face.normal = n;
		face.normal.normalize();
		face.centroid = 0.33333333333 * (face.n[0]->x + face.n[1]->x + face.n[2]->x);
	}

	for (Node &node : nodes) {
		real_t len = node.n.length();
		if (len > CMP_EPSILON) {
			node.n /= len;
		}
	}
}

void GodotSoftBody3D::update_bounds() {
	AABB prev_bounds = bounds;
	prev_bounds.grow_by(collision_margin);

	bounds = AABB();

	const uint32_t nodes_count = nodes.size();
	if (nodes_count == 0) {
		deinitialize_shape();
		return;
	}

	bool first = true;
	bool moved = false;
	for (uint32_t node_index = 0; node_index < nodes_count; ++node_index) {
		const Node &node = nodes[node_index];
		if (!prev_bounds.has_point(node.x)) {
			moved = true;
		}
		if (first) {
			bounds.position = node.x;
			first = false;
		} else {
			bounds.expand_to(node.x);
		}
	}

	if (get_space()) {
		initialize_shape(moved);
	}
}

void GodotSoftBody3D::update_constants() {
	reset_link_rest_lengths();
	update_link_constants();
	update_area();
}

void GodotSoftBody3D::update_area() {
	int i, ni;

	// Face area.
	for (Face &face : faces) {
		const Vector3 &x0 = face.n[0]->x;
		const Vector3 &x1 = face.n[1]->x;
		const Vector3 &x2 = face.n[2]->x;

		const Vector3 a = x1 - x0;
		const Vector3 b = x2 - x0;
		const Vector3 cr = vec3_cross(a, b);
		face.ra = cr.length() * 0.5;
	}

	// Node area.
	LocalVector<int> counts;
	if (nodes.size() > 0) {
		counts.resize(nodes.size());
		memset(counts.ptr(), 0, counts.size() * sizeof(int));
	}

	for (Node &node : nodes) {
		node.area = 0.0;
	}

	for (const Face &face : faces) {
		for (int j = 0; j < 3; ++j) {
			const int index = (int)(face.n[j] - &nodes[0]);
			counts[index]++;
			face.n[j]->area += Math::abs(face.ra);
		}
	}

	for (i = 0, ni = nodes.size(); i < ni; ++i) {
		if (counts[i] > 0) {
			nodes[i].area /= (real_t)counts[i];
		} else {
			nodes[i].area = 0.0;
		}
	}
}

void GodotSoftBody3D::reset_link_rest_lengths() {
	for (Link &link : links) {
		link.rl = (link.n[0]->x - link.n[1]->x).length();
		link.c1 = link.rl * link.rl;
	}
}

void GodotSoftBody3D::update_link_constants() {
	real_t inv_linear_stiffness = 1.0 / linear_stiffness;
	for (Link &link : links) {
		link.c0 = (link.n[0]->im + link.n[1]->im) * inv_linear_stiffness;
	}
}

void GodotSoftBody3D::apply_nodes_transform(const Transform3D &p_transform) {
	if (soft_mesh.is_null()) {
		return;
	}

	uint32_t node_count = nodes.size();
	Vector3 leaf_size = Vector3(collision_margin, collision_margin, collision_margin) * 2.0;
	for (uint32_t node_index = 0; node_index < node_count; ++node_index) {
		Node &node = nodes[node_index];

		node.x = p_transform.xform(node.x);
		node.q = node.x;
		node.v = Vector3();
		node.bv = Vector3();

		AABB node_aabb(node.x, leaf_size);
		node_tree.update(node.leaf, node_aabb);
	}

	face_tree.clear();

	update_normals_and_centroids();
	update_bounds();
	update_constants();
}

Vector3 GodotSoftBody3D::get_vertex_position(int p_index) const {
	ERR_FAIL_COND_V(p_index < 0, Vector3());

	if (soft_mesh.is_null()) {
		return Vector3();
	}

	ERR_FAIL_COND_V(p_index >= (int)map_visual_to_physics.size(), Vector3());
	uint32_t node_index = map_visual_to_physics[p_index];

	ERR_FAIL_COND_V(node_index >= nodes.size(), Vector3());
	return nodes[node_index].x;
}

void GodotSoftBody3D::set_vertex_position(int p_index, const Vector3 &p_position) {
	ERR_FAIL_COND(p_index < 0);

	if (soft_mesh.is_null()) {
		return;
	}

	ERR_FAIL_COND(p_index >= (int)map_visual_to_physics.size());
	uint32_t node_index = map_visual_to_physics[p_index];

	ERR_FAIL_COND(node_index >= nodes.size());
	Node &node = nodes[node_index];
	node.q = node.x;
	node.x = p_position;
}

void GodotSoftBody3D::pin_vertex(int p_index) {
	ERR_FAIL_COND(p_index < 0);

	if (is_vertex_pinned(p_index)) {
		return;
	}

	pinned_vertices.push_back(p_index);

	if (!soft_mesh.is_null()) {
		ERR_FAIL_COND(p_index >= (int)map_visual_to_physics.size());
		uint32_t node_index = map_visual_to_physics[p_index];

		ERR_FAIL_COND(node_index >= nodes.size());
		Node &node = nodes[node_index];
		node.im = 0.0;
	}
}

void GodotSoftBody3D::unpin_vertex(int p_index) {
	ERR_FAIL_COND(p_index < 0);

	uint32_t pinned_count = pinned_vertices.size();
	for (uint32_t i = 0; i < pinned_count; ++i) {
		if (p_index == pinned_vertices[i]) {
			pinned_vertices.remove_at(i);

			if (!soft_mesh.is_null()) {
				ERR_FAIL_COND(p_index >= (int)map_visual_to_physics.size());
				uint32_t node_index = map_visual_to_physics[p_index];

				ERR_FAIL_COND(node_index >= nodes.size());
				real_t inv_node_mass = nodes.size() * inv_total_mass;

				Node &node = nodes[node_index];
				node.im = inv_node_mass;
			}

			return;
		}
	}
}

void GodotSoftBody3D::unpin_all_vertices() {
	if (!soft_mesh.is_null()) {
		real_t inv_node_mass = nodes.size() * inv_total_mass;
		uint32_t pinned_count = pinned_vertices.size();
		for (uint32_t i = 0; i < pinned_count; ++i) {
			int pinned_vertex = pinned_vertices[i];

			ERR_CONTINUE(pinned_vertex >= (int)map_visual_to_physics.size());
			uint32_t node_index = map_visual_to_physics[pinned_vertex];

			ERR_CONTINUE(node_index >= nodes.size());
			Node &node = nodes[node_index];
			node.im = inv_node_mass;
		}
	}

	pinned_vertices.clear();
}

bool GodotSoftBody3D::is_vertex_pinned(int p_index) const {
	ERR_FAIL_COND_V(p_index < 0, false);

	uint32_t pinned_count = pinned_vertices.size();
	for (uint32_t i = 0; i < pinned_count; ++i) {
		if (p_index == pinned_vertices[i]) {
			return true;
		}
	}

	return false;
}

uint32_t GodotSoftBody3D::get_node_count() const {
	return nodes.size();
}

real_t GodotSoftBody3D::get_node_inv_mass(uint32_t p_node_index) const {
	ERR_FAIL_UNSIGNED_INDEX_V(p_node_index, nodes.size(), 0.0);
	return nodes[p_node_index].im;
}

Vector3 GodotSoftBody3D::get_node_position(uint32_t p_node_index) const {
	ERR_FAIL_UNSIGNED_INDEX_V(p_node_index, nodes.size(), Vector3());
	return nodes[p_node_index].x;
}

Vector3 GodotSoftBody3D::get_node_velocity(uint32_t p_node_index) const {
	ERR_FAIL_UNSIGNED_INDEX_V(p_node_index, nodes.size(), Vector3());
	return nodes[p_node_index].v;
}

Vector3 GodotSoftBody3D::get_node_biased_velocity(uint32_t p_node_index) const {
	ERR_FAIL_UNSIGNED_INDEX_V(p_node_index, nodes.size(), Vector3());
	return nodes[p_node_index].bv;
}

void GodotSoftBody3D::apply_node_impulse(uint32_t p_node_index, const Vector3 &p_impulse) {
	ERR_FAIL_UNSIGNED_INDEX(p_node_index, nodes.size());
	Node &node = nodes[p_node_index];
	node.v += p_impulse * node.im;
}

void GodotSoftBody3D::apply_node_bias_impulse(uint32_t p_node_index, const Vector3 &p_impulse) {
	ERR_FAIL_UNSIGNED_INDEX(p_node_index, nodes.size());
	Node &node = nodes[p_node_index];
	node.bv += p_impulse * node.im;
}

uint32_t GodotSoftBody3D::get_face_count() const {
	return faces.size();
}

void GodotSoftBody3D::get_face_points(uint32_t p_face_index, Vector3 &r_point_1, Vector3 &r_point_2, Vector3 &r_point_3) const {
	ERR_FAIL_UNSIGNED_INDEX(p_face_index, faces.size());
	const Face &face = faces[p_face_index];
	r_point_1 = face.n[0]->x;
	r_point_2 = face.n[1]->x;
	r_point_3 = face.n[2]->x;
}

Vector3 GodotSoftBody3D::get_face_normal(uint32_t p_face_index) const {
	ERR_FAIL_UNSIGNED_INDEX_V(p_face_index, faces.size(), Vector3());
	return faces[p_face_index].normal;
}

bool GodotSoftBody3D::create_from_trimesh(const Vector<int> &p_indices, const Vector<Vector3> &p_vertices) {
	ERR_FAIL_COND_V(p_indices.is_empty(), false);
	ERR_FAIL_COND_V(p_vertices.is_empty(), false);

	uint32_t node_count = 0;
	LocalVector<Vector3> vertices;
	const int visual_vertex_count(p_vertices.size());

	LocalVector<int> triangles;
	const uint32_t triangle_count(p_indices.size() / 3);
	triangles.resize(triangle_count * 3);

	// Merge all overlapping vertices and create a map of physical vertices to visual vertices.
	{
		// Process vertices.
		{
			uint32_t vertex_count = 0;
			HashMap<Vector3, uint32_t> unique_vertices;

			vertices.resize(visual_vertex_count);
			map_visual_to_physics.resize(visual_vertex_count);

			for (int visual_vertex_index = 0; visual_vertex_index < visual_vertex_count; ++visual_vertex_index) {
				const Vector3 &vertex = p_vertices[visual_vertex_index];

				HashMap<Vector3, uint32_t>::Iterator e = unique_vertices.find(vertex);
				uint32_t vertex_id;
				if (e) {
					// Already existing.
					vertex_id = e->value;
				} else {
					// Create new one.
					vertex_id = vertex_count++;
					unique_vertices[vertex] = vertex_id;
					vertices[vertex_id] = vertex;
				}

				map_visual_to_physics[visual_vertex_index] = vertex_id;
			}

			vertices.resize(vertex_count);
		}

		// Process triangles.
		{
			for (uint32_t triangle_index = 0; triangle_index < triangle_count; ++triangle_index) {
				for (int i = 0; i < 3; ++i) {
					int visual_index = 3 * triangle_index + i;
					int physics_index = map_visual_to_physics[p_indices[visual_index]];
					triangles[visual_index] = physics_index;
					node_count = MAX((int)node_count, physics_index);
				}
			}
		}
	}

	++node_count;

	// Create nodes from vertices.
	nodes.resize(node_count);
	real_t inv_node_mass = node_count * inv_total_mass;
	Vector3 leaf_size = Vector3(collision_margin, collision_margin, collision_margin) * 2.0;
	for (uint32_t i = 0; i < node_count; ++i) {
		Node &node = nodes[i];
		node.s = vertices[i];
		node.x = node.s;
		node.q = node.s;
		node.im = inv_node_mass;

		AABB node_aabb(node.x, leaf_size);
		node.leaf = node_tree.insert(node_aabb, &node);

		node.index = i;
	}

	// Create links and faces from triangles.
	LocalVector<bool> chks;
	chks.resize(node_count * node_count);
	memset(chks.ptr(), 0, chks.size() * sizeof(bool));

	for (uint32_t i = 0; i < triangle_count * 3; i += 3) {
		const int idx[] = { triangles[i], triangles[i + 1], triangles[i + 2] };

		for (int j = 2, k = 0; k < 3; j = k++) {
			int chk = idx[k] * node_count + idx[j];
			if (!chks[chk]) {
				chks[chk] = true;
				int inv_chk = idx[j] * node_count + idx[k];
				chks[inv_chk] = true;

				append_link(idx[j], idx[k]);
			}
		}

		append_face(idx[0], idx[1], idx[2]);
	}

	// Set pinned nodes.
	uint32_t pinned_count = pinned_vertices.size();
	for (uint32_t i = 0; i < pinned_count; ++i) {
		int pinned_vertex = pinned_vertices[i];

		ERR_CONTINUE(pinned_vertex >= visual_vertex_count);
		uint32_t node_index = map_visual_to_physics[pinned_vertex];

		ERR_CONTINUE(node_index >= node_count);
		Node &node = nodes[node_index];
		node.im = 0.0;
	}

	generate_bending_constraints(2);
	reoptimize_link_order();

	update_constants();
	update_normals_and_centroids();
	update_bounds();

	return true;
}

void GodotSoftBody3D::generate_bending_constraints(int p_distance) {
	uint32_t i, j;

	if (p_distance > 1) {
		// Build graph.
		const uint32_t n = nodes.size();
		const unsigned inf = (~(unsigned)0) >> 1;
		const uint32_t adj_size = n * n;
		unsigned *adj = memnew_arr(unsigned, adj_size);

#define IDX(_x_, _y_) ((_y_) * n + (_x_))
		for (j = 0; j < n; ++j) {
			for (i = 0; i < n; ++i) {
				int idx_ij = j * n + i;
				int idx_ji = i * n + j;
				if (i != j) {
					adj[idx_ij] = adj[idx_ji] = inf;
				} else {
					adj[idx_ij] = adj[idx_ji] = 0;
				}
			}
		}
		for (Link &link : links) {
			const int ia = (int)(link.n[0] - &nodes[0]);
			const int ib = (int)(link.n[1] - &nodes[0]);
			int idx = ib * n + ia;
			int idx_inv = ia * n + ib;
			adj[idx] = 1;
			adj[idx_inv] = 1;
		}

		// Special optimized case for distance == 2.
		if (p_distance == 2) {
			LocalVector<LocalVector<int>> node_links;

			// Build node links.
			node_links.resize(nodes.size());

			for (Link &link : links) {
				const int ia = (int)(link.n[0] - &nodes[0]);
				const int ib = (int)(link.n[1] - &nodes[0]);
				if (!node_links[ia].has(ib)) {
					node_links[ia].push_back(ib);
				}

				if (!node_links[ib].has(ia)) {
					node_links[ib].push_back(ia);
				}
			}
			for (uint32_t ii = 0; ii < node_links.size(); ii++) {
				for (uint32_t jj = 0; jj < node_links[ii].size(); jj++) {
					int k = node_links[ii][jj];
					for (const int &l : node_links[k]) {
						if ((int)ii != l) {
							int idx_ik = k * n + ii;
							int idx_kj = l * n + k;
							const unsigned sum = adj[idx_ik] + adj[idx_kj];
							ERR_FAIL_COND(sum != 2);
							int idx_ij = l * n + ii;
							if (adj[idx_ij] > sum) {
								int idx_ji = l * n + ii;
								adj[idx_ij] = adj[idx_ji] = sum;
							}
						}
					}
				}
			}
		} else {
			// Generic Floyd's algorithm.
			for (uint32_t k = 0; k < n; ++k) {
				for (j = 0; j < n; ++j) {
					for (i = j + 1; i < n; ++i) {
						int idx_ik = k * n + i;
						int idx_kj = j * n + k;
						const unsigned sum = adj[idx_ik] + adj[idx_kj];
						int idx_ij = j * n + i;
						if (adj[idx_ij] > sum) {
							int idx_ji = j * n + i;
							adj[idx_ij] = adj[idx_ji] = sum;
						}
					}
				}
			}
		}

		// Build links.
		for (j = 0; j < n; ++j) {
			for (i = j + 1; i < n; ++i) {
				int idx_ij = j * n + i;
				if (adj[idx_ij] == (unsigned)p_distance) {
					append_link(i, j);
				}
			}
		}
		memdelete_arr(adj);
	}
}

//===================================================================
//
//
// This function takes in a list of interdependent Links and tries
// to maximize the distance between calculation
// of dependent links. This increases the amount of parallelism that can
// be exploited by out-of-order instruction processors with large but
// (inevitably) finite instruction windows.
//
//===================================================================

// A small structure to track lists of dependent link calculations.
class LinkDeps {
public:
	// A link calculation that is dependent on this one.
	// Positive values = "input A" while negative values = "input B".
	int value;
	// Next dependence in the list.
	LinkDeps *next;
};
typedef LinkDeps *LinkDepsPtr;

void GodotSoftBody3D::reoptimize_link_order() {
	const int reop_not_dependent = -1;
	const int reop_node_complete = -2;

	uint32_t link_count = links.size();
	uint32_t node_count = nodes.size();

	if (link_count < 1 || node_count < 2) {
		return;
	}

	uint32_t i;
	Link *lr;
	int ar, br;
	Node *node0 = &(nodes[0]);
	Node *node1 = &(nodes[1]);
	LinkDepsPtr link_dep;
	int ready_list_head, ready_list_tail, link_num, link_dep_frees, dep_link;

	// Allocate temporary buffers.
	int *node_written_at = memnew_arr(int, node_count + 1); // What link calculation produced this node's current values?
	int *link_dep_A = memnew_arr(int, link_count); // Link calculation input is dependent upon prior calculation #N
	int *link_dep_B = memnew_arr(int, link_count);
	int *ready_list = memnew_arr(int, link_count); // List of ready-to-process link calculations (# of links, maximum)
	LinkDeps *link_dep_free_list = memnew_arr(LinkDeps, 2 * link_count); // Dependent-on-me list elements (2x# of links, maximum)
	LinkDepsPtr *link_dep_list_starts = memnew_arr(LinkDepsPtr, link_count); // Start nodes of dependent-on-me lists, one for each link

	// Copy the original, unsorted links to a side buffer.
	Link *link_buffer = memnew_arr(Link, link_count);
	memcpy(link_buffer, &(links[0]), sizeof(Link) * link_count);

	// Clear out the node setup and ready list.
	for (i = 0; i < node_count + 1; i++) {
		node_written_at[i] = reop_not_dependent;
	}
	for (i = 0; i < link_count; i++) {
		link_dep_list_starts[i] = nullptr;
	}
	ready_list_head = ready_list_tail = link_dep_frees = 0;

	// Initial link analysis to set up data structures.
	for (i = 0; i < link_count; i++) {
		// Note which prior link calculations we are dependent upon & build up dependence lists.
		lr = &(links[i]);
		ar = (lr->n[0] - node0) / (node1 - node0);
		br = (lr->n[1] - node0) / (node1 - node0);
		if (node_written_at[ar] > reop_not_dependent) {
			link_dep_A[i] = node_written_at[ar];
			link_dep = &link_dep_free_list[link_dep_frees++];
			link_dep->value = i;
			link_dep->next = link_dep_list_starts[node_written_at[ar]];
			link_dep_list_starts[node_written_at[ar]] = link_dep;
		} else {
			link_dep_A[i] = reop_not_dependent;
		}
		if (node_written_at[br] > reop_not_dependent) {
			link_dep_B[i] = node_written_at[br];
			link_dep = &link_dep_free_list[link_dep_frees++];
			link_dep->value = -(int)(i + 1);
			link_dep->next = link_dep_list_starts[node_written_at[br]];
			link_dep_list_starts[node_written_at[br]] = link_dep;
		} else {
			link_dep_B[i] = reop_not_dependent;
		}

		// Add this link to the initial ready list, if it is not dependent on any other links.
		if ((link_dep_A[i] == reop_not_dependent) && (link_dep_B[i] == reop_not_dependent)) {
			ready_list[ready_list_tail++] = i;
			link_dep_A[i] = link_dep_B[i] = reop_node_complete; // Probably not needed now.
		}

		// Update the nodes to mark which ones are calculated by this link.
		node_written_at[ar] = node_written_at[br] = i;
	}

	// Process the ready list and create the sorted list of links:
	// -- By treating the ready list as a queue, we maximize the distance between any
	//    inter-dependent node calculations.
	// -- All other (non-related) nodes in the ready list will automatically be inserted
	//    in between each set of inter-dependent link calculations by this loop.
	i = 0;
	while (ready_list_head != ready_list_tail) {
		// Use ready list to select the next link to process.
		link_num = ready_list[ready_list_head++];
		// Copy the next-to-calculate link back into the original link array.
		links[i++] = link_buffer[link_num];

		// Free up any link inputs that are dependent on this one.
		link_dep = link_dep_list_starts[link_num];
		while (link_dep) {
			dep_link = link_dep->value;
			if (dep_link >= 0) {
				link_dep_A[dep_link] = reop_not_dependent;
			} else {
				dep_link = -dep_link - 1;
				link_dep_B[dep_link] = reop_not_dependent;
			}
			// Add this dependent link calculation to the ready list if *both* inputs are clear.
			if ((link_dep_A[dep_link] == reop_not_dependent) && (link_dep_B[dep_link] == reop_not_dependent)) {
				ready_list[ready_list_tail++] = dep_link;
				link_dep_A[dep_link] = link_dep_B[dep_link] = reop_node_complete; // Probably not needed now.
			}
			link_dep = link_dep->next;
		}
	}

	// Delete the temporary buffers.
	memdelete_arr(node_written_at);
	memdelete_arr(link_dep_A);
	memdelete_arr(link_dep_B);
	memdelete_arr(ready_list);
	memdelete_arr(link_dep_free_list);
	memdelete_arr(link_dep_list_starts);
	memdelete_arr(link_buffer);
}

void GodotSoftBody3D::append_link(uint32_t p_node1, uint32_t p_node2) {
	if (p_node1 == p_node2) {
		return;
	}

	Node *node1 = &nodes[p_node1];
	Node *node2 = &nodes[p_node2];

	Link link;
	link.n[0] = node1;
	link.n[1] = node2;
	link.rl = (node1->x - node2->x).length();

	links.push_back(link);
}

void GodotSoftBody3D::append_face(uint32_t p_node1, uint32_t p_node2, uint32_t p_node3) {
	if (p_node1 == p_node2) {
		return;
	}
	if (p_node1 == p_node3) {
		return;
	}
	if (p_node2 == p_node3) {
		return;
	}

	Node *node1 = &nodes[p_node1];
	Node *node2 = &nodes[p_node2];
	Node *node3 = &nodes[p_node3];

	Face face;
	face.n[0] = node1;
	face.n[1] = node2;
	face.n[2] = node3;

	face.index = faces.size();

	faces.push_back(face);
}

void GodotSoftBody3D::set_iteration_count(int p_val) {
	iteration_count = p_val;
}

void GodotSoftBody3D::set_total_mass(real_t p_val) {
	ERR_FAIL_COND(p_val < 0.0);

	inv_total_mass = 1.0 / p_val;
	real_t mass_factor = total_mass * inv_total_mass;
	total_mass = p_val;

	uint32_t node_count = nodes.size();
	for (uint32_t node_index = 0; node_index < node_count; ++node_index) {
		Node &node = nodes[node_index];
		node.im *= mass_factor;
	}

	update_constants();
}

void GodotSoftBody3D::set_collision_margin(real_t p_val) {
	collision_margin = p_val;
}

void GodotSoftBody3D::set_linear_stiffness(real_t p_val) {
	linear_stiffness = p_val;
}

void GodotSoftBody3D::set_pressure_coefficient(real_t p_val) {
	pressure_coefficient = p_val;
}

void GodotSoftBody3D::set_damping_coefficient(real_t p_val) {
	damping_coefficient = p_val;
}

void GodotSoftBody3D::set_drag_coefficient(real_t p_val) {
	drag_coefficient = p_val;
}

void GodotSoftBody3D::add_velocity(const Vector3 &p_velocity) {
	for (Node &node : nodes) {
		if (node.im > 0) {
			node.v += p_velocity;
		}
	}
}

void GodotSoftBody3D::apply_forces(const LocalVector<GodotArea3D *> &p_wind_areas) {
	if (nodes.is_empty()) {
		return;
	}

	int32_t j;

	real_t volume = 0.0;
	const Vector3 &org = nodes[0].x;

	// Iterate over faces (try not to iterate elsewhere if possible).
	for (const Face &face : faces) {
		Vector3 wind_force(0, 0, 0);

		// Compute volume.
		volume += vec3_dot(face.n[0]->x - org, vec3_cross(face.n[1]->x - org, face.n[2]->x - org));

		// Compute nodal forces from area winds.
		if (!p_wind_areas.is_empty()) {
			for (const GodotArea3D *area : p_wind_areas) {
				wind_force += _compute_area_windforce(area, &face);
			}

			for (j = 0; j < 3; j++) {
				Node *current_node = face.n[j];
				current_node->f += wind_force;
			}
		}
	}
	volume /= 6.0;

	// Apply nodal pressure forces.
	if (pressure_coefficient > CMP_EPSILON) {
		real_t ivolumetp = 1.0 / Math::abs(volume) * pressure_coefficient;
		for (Node &node : nodes) {
			if (node.im > 0) {
				node.f += node.n * (node.area * ivolumetp);
			}
		}
	}
}

Vector3 GodotSoftBody3D::_compute_area_windforce(const GodotArea3D *p_area, const Face *p_face) {
	real_t wfm = p_area->get_wind_force_magnitude();
	real_t waf = p_area->get_wind_attenuation_factor();
	const Vector3 &wd = p_area->get_wind_direction();
	const Vector3 &ws = p_area->get_wind_source();
	real_t projection_on_tri_normal = vec3_dot(p_face->normal, wd);
	real_t projection_toward_centroid = vec3_dot(p_face->centroid - ws, wd);
	real_t attenuation_over_distance = pow(projection_toward_centroid, -waf);
	real_t nodal_force_magnitude = wfm * 0.33333333333 * p_face->ra * projection_on_tri_normal * attenuation_over_distance;
	return nodal_force_magnitude * p_face->normal;
}

void GodotSoftBody3D::predict_motion(real_t p_delta) {
	const real_t inv_delta = 1.0 / p_delta;

	ERR_FAIL_NULL(get_space());

	bool gravity_done = false;
	Vector3 gravity;

	LocalVector<GodotArea3D *> wind_areas;

	int ac = areas.size();
	if (ac) {
		areas.sort();
		const AreaCMP *aa = &areas[0];
		for (int i = ac - 1; i >= 0; i--) {
			if (!gravity_done) {
				PhysicsServer3D::AreaSpaceOverrideMode area_gravity_mode = (PhysicsServer3D::AreaSpaceOverrideMode)(int)aa[i].area->get_param(PhysicsServer3D::AREA_PARAM_GRAVITY_OVERRIDE_MODE);
				if (area_gravity_mode != PhysicsServer3D::AREA_SPACE_OVERRIDE_DISABLED) {
					Vector3 area_gravity;
					aa[i].area->compute_gravity(get_transform().get_origin(), area_gravity);
					switch (area_gravity_mode) {
						case PhysicsServer3D::AREA_SPACE_OVERRIDE_COMBINE:
						case PhysicsServer3D::AREA_SPACE_OVERRIDE_COMBINE_REPLACE: {
							gravity += area_gravity;
							gravity_done = area_gravity_mode == PhysicsServer3D::AREA_SPACE_OVERRIDE_COMBINE_REPLACE;
						} break;
						case PhysicsServer3D::AREA_SPACE_OVERRIDE_REPLACE:
						case PhysicsServer3D::AREA_SPACE_OVERRIDE_REPLACE_COMBINE: {
							gravity = area_gravity;
							gravity_done = area_gravity_mode == PhysicsServer3D::AREA_SPACE_OVERRIDE_REPLACE;
						} break;
						default: {
						}
					}
				}
			}

			if (aa[i].area->get_wind_force_magnitude() > CMP_EPSILON) {
				wind_areas.push_back(aa[i].area);
			}
		}
	}

	// Add default gravity and damping from space area.
	if (!gravity_done) {
		GodotArea3D *default_area = get_space()->get_default_area();
		ERR_FAIL_NULL(default_area);

		Vector3 default_gravity;
		default_area->compute_gravity(get_transform().get_origin(), default_gravity);
		gravity += default_gravity;
	}

	// Apply forces.
	add_velocity(gravity * p_delta);
	if (pressure_coefficient > CMP_EPSILON || !wind_areas.is_empty()) {
		apply_forces(wind_areas);
	}

	// Avoid soft body from 'exploding' so use some upper threshold of maximum motion
	// that a node can travel per frame.
	const real_t max_displacement = 1000.0;
	real_t clamp_delta_v = max_displacement * inv_delta;

	// Integrate.
	for (Node &node : nodes) {
		node.q = node.x;
		Vector3 delta_v = node.f * node.im * p_delta;
		for (int c = 0; c < 3; c++) {
			delta_v[c] = CLAMP(delta_v[c], -clamp_delta_v, clamp_delta_v);
		}
		node.v += delta_v;
		node.x += node.v * p_delta;
		node.f = Vector3();
	}

	// Bounds and tree update.
	update_bounds();

	// Node tree update.
	for (const Node &node : nodes) {
		AABB node_aabb(node.x, Vector3());
		node_aabb.expand_to(node.x + node.v * p_delta);
		node_aabb.grow_by(collision_margin);

		node_tree.update(node.leaf, node_aabb);
	}

	// Face tree update.
	if (!face_tree.is_empty()) {
		update_face_tree(p_delta);
	}

	// Optimize node tree.
	node_tree.optimize_incremental(1);
	face_tree.optimize_incremental(1);
}

void GodotSoftBody3D::solve_constraints(real_t p_delta) {
	const real_t inv_delta = 1.0 / p_delta;

	for (Link &link : links) {
		link.c3 = link.n[1]->q - link.n[0]->q;
		link.c2 = 1 / (link.c3.length_squared() * link.c0);
	}

	// Solve velocities.
	for (Node &node : nodes) {
		node.x = node.q + node.v * p_delta;
	}

	// Solve positions.
	for (int isolve = 0; isolve < iteration_count; ++isolve) {
		const real_t ti = isolve / (real_t)iteration_count;
		solve_links(1.0, ti);
	}
	const real_t vc = (1.0 - damping_coefficient) * inv_delta;
	for (Node &node : nodes) {
		node.x += node.bv * p_delta;
		node.bv = Vector3();

		node.v = (node.x - node.q) * vc;

		node.q = node.x;
	}

	update_normals_and_centroids();
}

void GodotSoftBody3D::solve_links(real_t kst, real_t ti) {
	for (Link &link : links) {
		if (link.c0 > 0) {
			Node &node_a = *link.n[0];
			Node &node_b = *link.n[1];
			const Vector3 del = node_b.x - node_a.x;
			const real_t len = del.length_squared();
			if (link.c1 + len > CMP_EPSILON) {
				const real_t k = ((link.c1 - len) / (link.c0 * (link.c1 + len))) * kst;
				node_a.x -= del * (k * node_a.im);
				node_b.x += del * (k * node_b.im);
			}
		}
	}
}

struct AABBQueryResult {
	const GodotSoftBody3D *soft_body = nullptr;
	void *userdata = nullptr;
	GodotSoftBody3D::QueryResultCallback result_callback = nullptr;

	_FORCE_INLINE_ bool operator()(void *p_data) {
		return result_callback(soft_body->get_node_index(p_data), userdata);
	}
};

void GodotSoftBody3D::query_aabb(const AABB &p_aabb, GodotSoftBody3D::QueryResultCallback p_result_callback, void *p_userdata) {
	AABBQueryResult query_result;
	query_result.soft_body = this;
	query_result.result_callback = p_result_callback;
	query_result.userdata = p_userdata;

	node_tree.aabb_query(p_aabb, query_result);
}

struct RayQueryResult {
	const GodotSoftBody3D *soft_body = nullptr;
	void *userdata = nullptr;
	GodotSoftBody3D::QueryResultCallback result_callback = nullptr;

	_FORCE_INLINE_ bool operator()(void *p_data) {
		return result_callback(soft_body->get_face_index(p_data), userdata);
	}
};

void GodotSoftBody3D::query_ray(const Vector3 &p_from, const Vector3 &p_to, GodotSoftBody3D::QueryResultCallback p_result_callback, void *p_userdata) {
	if (face_tree.is_empty()) {
		initialize_face_tree();
	}

	RayQueryResult query_result;
	query_result.soft_body = this;
	query_result.result_callback = p_result_callback;
	query_result.userdata = p_userdata;

	face_tree.ray_query(p_from, p_to, query_result);
}

void GodotSoftBody3D::initialize_face_tree() {
	face_tree.clear();
	for (Face &face : faces) {
		AABB face_aabb;

		face_aabb.position = face.n[0]->x;
		face_aabb.expand_to(face.n[1]->x);
		face_aabb.expand_to(face.n[2]->x);

		face_aabb.grow_by(collision_margin);

		face.leaf = face_tree.insert(face_aabb, &face);
	}
}

void GodotSoftBody3D::update_face_tree(real_t p_delta) {
	for (const Face &face : faces) {
		AABB face_aabb;

		const Node *node0 = face.n[0];
		face_aabb.position = node0->x;
		face_aabb.expand_to(node0->x + node0->v * p_delta);

		const Node *node1 = face.n[1];
		face_aabb.expand_to(node1->x);
		face_aabb.expand_to(node1->x + node1->v * p_delta);

		const Node *node2 = face.n[2];
		face_aabb.expand_to(node2->x);
		face_aabb.expand_to(node2->x + node2->v * p_delta);

		face_aabb.grow_by(collision_margin);

		face_tree.update(face.leaf, face_aabb);
	}
}

void GodotSoftBody3D::initialize_shape(bool p_force_move) {
	if (get_shape_count() == 0) {
		GodotSoftBodyShape3D *soft_body_shape = memnew(GodotSoftBodyShape3D(this));
		add_shape(soft_body_shape);
	} else if (p_force_move) {
		GodotSoftBodyShape3D *soft_body_shape = static_cast<GodotSoftBodyShape3D *>(get_shape(0));
		soft_body_shape->update_bounds();
	}
}

void GodotSoftBody3D::deinitialize_shape() {
	if (get_shape_count() > 0) {
		GodotShape3D *shape = get_shape(0);
		remove_shape(shape);
		memdelete(shape);
	}
}

void GodotSoftBody3D::destroy() {
	soft_mesh = RID();

	map_visual_to_physics.clear();

	node_tree.clear();
	face_tree.clear();

	nodes.clear();
	links.clear();
	faces.clear();

	bounds = AABB();
	deinitialize_shape();
}

void GodotSoftBodyShape3D::update_bounds() {
	ERR_FAIL_NULL(soft_body);

	AABB collision_aabb = soft_body->get_bounds();
	collision_aabb.grow_by(soft_body->get_collision_margin());
	configure(collision_aabb);
}

GodotSoftBodyShape3D::GodotSoftBodyShape3D(GodotSoftBody3D *p_soft_body) {
	soft_body = p_soft_body;
	update_bounds();
}

struct _SoftBodyIntersectSegmentInfo {
	const GodotSoftBody3D *soft_body = nullptr;
	Vector3 from;
	Vector3 dir;
	Vector3 hit_position;
	uint32_t hit_face_index = -1;
	real_t hit_dist_sq = INFINITY;

	static bool process_hit(uint32_t p_face_index, void *p_userdata) {
		_SoftBodyIntersectSegmentInfo &query_info = *(static_cast<_SoftBodyIntersectSegmentInfo *>(p_userdata));

		Vector3 points[3];
		query_info.soft_body->get_face_points(p_face_index, points[0], points[1], points[2]);

		Vector3 result;
		if (Geometry3D::ray_intersects_triangle(query_info.from, query_info.dir, points[0], points[1], points[2], &result)) {
			real_t dist_sq = query_info.from.distance_squared_to(result);
			if (dist_sq < query_info.hit_dist_sq) {
				query_info.hit_dist_sq = dist_sq;
				query_info.hit_position = result;
				query_info.hit_face_index = p_face_index;
			}
		}

		// Continue with the query.
		return false;
	}
};

bool GodotSoftBodyShape3D::intersect_segment(const Vector3 &p_begin, const Vector3 &p_end, Vector3 &r_result, Vector3 &r_normal, int &r_face_index, bool p_hit_back_faces) const {
	_SoftBodyIntersectSegmentInfo query_info;
	query_info.soft_body = soft_body;
	query_info.from = p_begin;
	query_info.dir = (p_end - p_begin).normalized();

	soft_body->query_ray(p_begin, p_end, _SoftBodyIntersectSegmentInfo::process_hit, &query_info);

	if (query_info.hit_dist_sq != INFINITY) {
		r_result = query_info.hit_position;
		r_normal = soft_body->get_face_normal(query_info.hit_face_index);
		return true;
	}

	return false;
}

bool GodotSoftBodyShape3D::intersect_point(const Vector3 &p_point) const {
	return false;
}

Vector3 GodotSoftBodyShape3D::get_closest_point_to(const Vector3 &p_point) const {
	ERR_FAIL_V_MSG(Vector3(), "Get closest point is not supported for soft bodies.");
}

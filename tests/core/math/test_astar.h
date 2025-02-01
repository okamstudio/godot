/**************************************************************************/
/*  test_astar.h                                                          */
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

#include "core/math/a_star.h"

#include "tests/test_macros.h"

namespace TestAStar {

class ABCX : public AStar3D {
public:
	enum {
		A,
		B,
		C,
		X,
	};

	ABCX() {
		add_point(A, Vector3(0, 0, 0));
		add_point(B, Vector3(1, 0, 0));
		add_point(C, Vector3(0, 1, 0));
		add_point(X, Vector3(0, 0, 1));
		connect_points(A, B);
		connect_points(A, C);
		connect_points(B, C);
		connect_points(X, A);
	}

	// Disable heuristic completely.
	real_t _compute_cost(int64_t p_from, int64_t p_to) {
		if (p_from == A && p_to == C) {
			return 1000;
		}
		return 100;
	}
};

TEST_CASE("[AStar3D] ABC path") {
	ABCX abcx;
	Vector<int64_t> path = abcx.get_id_path(ABCX::A, ABCX::C);
	REQUIRE(path.size() == 3);
	CHECK(path[0] == ABCX::A);
	CHECK(path[1] == ABCX::B);
	CHECK(path[2] == ABCX::C);
}

TEST_CASE("[AStar3D] ABCX path") {
	ABCX abcx;
	Vector<int64_t> path = abcx.get_id_path(ABCX::X, ABCX::C);
	REQUIRE(path.size() == 4);
	CHECK(path[0] == ABCX::X);
	CHECK(path[1] == ABCX::A);
	CHECK(path[2] == ABCX::B);
	CHECK(path[3] == ABCX::C);
}

TEST_CASE("[AStar3D] Add/Remove") {
	AStar3D a;

	// Manual tests.
	a.add_point(1, Vector3(0, 0, 0));
	a.add_point(2, Vector3(0, 1, 0));
	a.add_point(3, Vector3(1, 1, 0));
	a.add_point(4, Vector3(2, 0, 0));
	a.connect_points(1, 2, true);
	a.connect_points(1, 3, true);
	a.connect_points(1, 4, false);

	CHECK(a.are_points_connected(2, 1));
	CHECK(a.are_points_connected(4, 1));
	CHECK(a.are_points_connected(2, 1, false));
	CHECK_FALSE(a.are_points_connected(4, 1, false));

	a.disconnect_points(1, 2, true);
	CHECK(a.get_point_connections(1).size() == 2); // 3, 4
	CHECK(a.get_point_connections(2).size() == 0);

	a.disconnect_points(4, 1, false);
	CHECK(a.get_point_connections(1).size() == 2); // 3, 4
	CHECK(a.get_point_connections(4).size() == 0);

	a.disconnect_points(4, 1, true);
	CHECK(a.get_point_connections(1).size() == 1); // 3
	CHECK(a.get_point_connections(4).size() == 0);

	a.connect_points(2, 3, false);
	CHECK(a.get_point_connections(2).size() == 1); // 3
	CHECK(a.get_point_connections(3).size() == 1); // 1

	a.connect_points(2, 3, true);
	CHECK(a.get_point_connections(2).size() == 1); // 3
	CHECK(a.get_point_connections(3).size() == 2); // 1, 2

	a.disconnect_points(2, 3, false);
	CHECK(a.get_point_connections(2).size() == 0);
	CHECK(a.get_point_connections(3).size() == 2); // 1, 2

	a.connect_points(4, 3, true);
	CHECK(a.get_point_connections(3).size() == 3); // 1, 2, 4
	CHECK(a.get_point_connections(4).size() == 1); // 3

	a.disconnect_points(3, 4, false);
	CHECK(a.get_point_connections(3).size() == 2); // 1, 2
	CHECK(a.get_point_connections(4).size() == 1); // 3

	a.remove_point(3);
	CHECK(a.get_point_connections(1).size() == 0);
	CHECK(a.get_point_connections(2).size() == 0);
	CHECK(a.get_point_connections(4).size() == 0);

	a.add_point(0, Vector3(0, -1, 0));
	a.add_point(3, Vector3(2, 1, 0));
	// 0: (0, -1)
	// 1: (0, 0)
	// 2: (0, 1)
	// 3: (2, 1)
	// 4: (2, 0)

	// Tests for get_closest_position_in_segment.
	a.connect_points(2, 3);
	CHECK(a.get_closest_position_in_segment(Vector3(0.5, 0.5, 0)) == Vector3(0.5, 1, 0));

	a.connect_points(3, 4);
	a.connect_points(0, 3);
	a.connect_points(1, 4);
	a.disconnect_points(1, 4, false);
	a.disconnect_points(4, 3, false);
	a.disconnect_points(3, 4, false);
	// Remaining edges: <2, 3>, <0, 3>, <1, 4> (directed).
	CHECK(a.get_closest_position_in_segment(Vector3(2, 0.5, 0)) == Vector3(1.75, 0.75, 0));
	CHECK(a.get_closest_position_in_segment(Vector3(-1, 0.2, 0)) == Vector3(0, 0, 0));
	CHECK(a.get_closest_position_in_segment(Vector3(3, 2, 0)) == Vector3(2, 1, 0));

	Math::seed(0);

	// Random tests for connectivity checks
	for (int i = 0; i < 20000; i++) {
		int u = Math::rand() % 5;
		int v = Math::rand() % 4;
		if (u == v) {
			v = 4;
		}
		if (Math::rand() % 2 == 1) {
			// Add a (possibly existing) directed edge and confirm connectivity.
			a.connect_points(u, v, false);
			CHECK(a.are_points_connected(u, v, false));
		} else {
			// Remove a (possibly nonexistent) directed edge and confirm disconnectivity.
			a.disconnect_points(u, v, false);
			CHECK_FALSE(a.are_points_connected(u, v, false));
		}
	}

	// Random tests for point removal.
	for (int i = 0; i < 20000; i++) {
		a.clear();
		for (int j = 0; j < 5; j++) {
			a.add_point(j, Vector3(0, 0, 0));
		}

		// Add or remove random edges.
		for (int j = 0; j < 10; j++) {
			int u = Math::rand() % 5;
			int v = Math::rand() % 4;
			if (u == v) {
				v = 4;
			}
			if (Math::rand() % 2 == 1) {
				a.connect_points(u, v, false);
			} else {
				a.disconnect_points(u, v, false);
			}
		}

		// Remove point 0.
		a.remove_point(0);
		// White box: this will check all edges remaining in the segments set.
		for (int j = 1; j < 5; j++) {
			CHECK_FALSE(a.are_points_connected(0, j, true));
		}
	}
	// It's been great work, cheers. \(^ ^)/
}

TEST_CASE("[Stress][AStar3D] Find paths") {
	// Random stress tests with Floyd-Warshall.
	const int N = 30;
	Math::seed(0);

	for (int test = 0; test < 1000; test++) {
		AStar3D a;
		Vector3 p[N];
		bool adj[N][N] = { { false } };

		// Assign initial coordinates.
		for (int u = 0; u < N; u++) {
			p[u].x = Math::rand() % 100;
			p[u].y = Math::rand() % 100;
			p[u].z = Math::rand() % 100;
			a.add_point(u, p[u]);
		}
		// Generate a random sequence of operations.
		for (int i = 0; i < 1000; i++) {
			// Pick two different vertices.
			int u, v;
			u = Math::rand() % N;
			v = Math::rand() % (N - 1);
			if (u == v) {
				v = N - 1;
			}
			// Pick a random operation.
			int op = Math::rand();
			switch (op % 9) {
				case 0:
				case 1:
				case 2:
				case 3:
				case 4:
				case 5:
					// Add edge (u, v); possibly bidirectional.
					a.connect_points(u, v, op % 2);
					adj[u][v] = true;
					if (op % 2) {
						adj[v][u] = true;
					}
					break;
				case 6:
				case 7:
					// Remove edge (u, v); possibly bidirectional.
					a.disconnect_points(u, v, op % 2);
					adj[u][v] = false;
					if (op % 2) {
						adj[v][u] = false;
					}
					break;
				case 8:
					// Remove point u and add it back; clears adjacent edges and changes coordinates.
					a.remove_point(u);
					p[u].x = Math::rand() % 100;
					p[u].y = Math::rand() % 100;
					p[u].z = Math::rand() % 100;
					a.add_point(u, p[u]);
					for (v = 0; v < N; v++) {
						adj[u][v] = adj[v][u] = false;
					}
					break;
			}
		}
		// Floyd-Warshall.
		float d[N][N];
		for (int u = 0; u < N; u++) {
			for (int v = 0; v < N; v++) {
				d[u][v] = (u == v || adj[u][v]) ? p[u].distance_to(p[v]) : INFINITY;
			}
		}
		for (int w = 0; w < N; w++) {
			for (int u = 0; u < N; u++) {
				for (int v = 0; v < N; v++) {
					if (d[u][v] > d[u][w] + d[w][v]) {
						d[u][v] = d[u][w] + d[w][v];
					}
				}
			}
		}
		// Display statistics.
		int count = 0;
		for (int u = 0; u < N; u++) {
			for (int v = 0; v < N; v++) {
				if (adj[u][v]) {
					count++;
				}
			}
		}
		print_verbose(vformat("Test #%4d: %3d edges, ", test + 1, count));
		count = 0;
		for (int u = 0; u < N; u++) {
			for (int v = 0; v < N; v++) {
				if (!Math::is_inf(d[u][v])) {
					count++;
				}
			}
		}
		print_verbose(vformat("%3d/%d pairs of reachable points\n", count - N, N * (N - 1)));

		// Check A*'s output.
		bool match = true;
		for (int u = 0; u < N; u++) {
			for (int v = 0; v < N; v++) {
				if (u != v) {
					Vector<int64_t> route = a.get_id_path(u, v);
					if (!Math::is_inf(d[u][v])) {
						// Reachable.
						if (route.size() == 0) {
							print_verbose(vformat("From %d to %d: A* did not find a path\n", u, v));
							match = false;
							goto exit;
						}
						float astar_dist = 0;
						for (int i = 1; i < route.size(); i++) {
							if (!adj[route[i - 1]][route[i]]) {
								print_verbose(vformat("From %d to %d: edge (%d, %d) does not exist\n",
										u, v, route[i - 1], route[i]));
								match = false;
								goto exit;
							}
							astar_dist += p[route[i - 1]].distance_to(p[route[i]]);
						}
						if (!Math::is_equal_approx(astar_dist, d[u][v])) {
							print_verbose(vformat("From %d to %d: Floyd-Warshall gives %.6f, A* gives %.6f\n",
									u, v, d[u][v], astar_dist));
							match = false;
							goto exit;
						}
					} else {
						// Unreachable.
						if (route.size() > 0) {
							print_verbose(vformat("From %d to %d: A* somehow found a nonexistent path\n", u, v));
							match = false;
							goto exit;
						}
					}
				}
			}
		}
	exit:
		CHECK_MESSAGE(match, "Found all paths.");
	}
}
} // namespace TestAStar

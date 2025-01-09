/**************************************************************************/
/*  test_navigation_server_2d.h                                           */
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

#ifndef TEST_NAVIGATION_SERVER_2D_H
#define TEST_NAVIGATION_SERVER_2D_H

#include "modules/navigation_2d/nav_utils_2d.h"
#include "servers/navigation_server_2d.h"

#include "scene/2d/polygon_2d.h"

#include "tests/test_macros.h"

namespace TestNavigationServer2D {

// TODO: Find a more generic way to create `Callable` mocks.
class CallableMock : public Object {
	GDCLASS(CallableMock, Object);

public:
	void function1(Variant arg0) {
		function1_calls++;
		function1_latest_arg0 = arg0;
	}

	unsigned function1_calls{ 0 };
	Variant function1_latest_arg0;
};

static inline Array build_array() {
	return Array();
}
template <typename... Targs>
static inline Array build_array(Variant item, Targs... Fargs) {
	Array a = build_array(Fargs...);
	a.push_front(item);
	return a;
}

struct GreaterThan {
	bool operator()(int p_a, int p_b) const { return p_a > p_b; }
};

struct CompareArrayValues {
	const int *array;

	CompareArrayValues(const int *p_array) :
			array(p_array) {}

	bool operator()(uint32_t p_index_a, uint32_t p_index_b) const {
		return array[p_index_a] < array[p_index_b];
	}
};

struct RegisterHeapIndexes {
	uint32_t *indexes;

	RegisterHeapIndexes(uint32_t *p_indexes) :
			indexes(p_indexes) {}

	void operator()(uint32_t p_vector_index, uint32_t p_heap_index) {
		indexes[p_vector_index] = p_heap_index;
	}
};

TEST_SUITE("[Navigation2D]") {
	TEST_CASE("[NavigationServer2D] Server should be empty when initialized") {
		NavigationServer2D *navigation_server = NavigationServer2D::get_singleton();
		CHECK_EQ(navigation_server->get_maps().size(), 0);

		SUBCASE("'ProcessInfo' should report all counters empty as well") {
			CHECK_EQ(navigation_server->get_process_info(NavigationServer2D::INFO_ACTIVE_MAPS), 0);
			CHECK_EQ(navigation_server->get_process_info(NavigationServer2D::INFO_REGION_COUNT), 0);
			CHECK_EQ(navigation_server->get_process_info(NavigationServer2D::INFO_AGENT_COUNT), 0);
			CHECK_EQ(navigation_server->get_process_info(NavigationServer2D::INFO_LINK_COUNT), 0);
			CHECK_EQ(navigation_server->get_process_info(NavigationServer2D::INFO_POLYGON_COUNT), 0);
			CHECK_EQ(navigation_server->get_process_info(NavigationServer2D::INFO_EDGE_COUNT), 0);
			CHECK_EQ(navigation_server->get_process_info(NavigationServer2D::INFO_EDGE_MERGE_COUNT), 0);
			CHECK_EQ(navigation_server->get_process_info(NavigationServer2D::INFO_EDGE_CONNECTION_COUNT), 0);
			CHECK_EQ(navigation_server->get_process_info(NavigationServer2D::INFO_EDGE_FREE_COUNT), 0);
		}
	}

	TEST_CASE("[NavigationServer2D] Server should manage agent properly") {
		NavigationServer2D *navigation_server = NavigationServer2D::get_singleton();

		RID agent = navigation_server->agent_create();
		CHECK(agent.is_valid());

		SUBCASE("'ProcessInfo' should not report dangling agent") {
			CHECK_EQ(navigation_server->get_process_info(NavigationServer2D::INFO_AGENT_COUNT), 0);
		}

		SUBCASE("Setters/getters should work") {
			bool initial_avoidance_enabled = navigation_server->agent_get_avoidance_enabled(agent);
			navigation_server->agent_set_avoidance_enabled(agent, !initial_avoidance_enabled);
			navigation_server->process(0.0); // Give server some cycles to commit.

			CHECK_EQ(navigation_server->agent_get_avoidance_enabled(agent), !initial_avoidance_enabled);
			// TODO: Add remaining setters/getters once the missing getters are added.
		}

		SUBCASE("'ProcessInfo' should report agent with active map") {
			RID map = navigation_server->map_create();
			CHECK(map.is_valid());
			navigation_server->map_set_active(map, true);
			navigation_server->agent_set_map(agent, map);
			navigation_server->process(0.0); // Give server some cycles to commit.
			CHECK_EQ(navigation_server->get_process_info(NavigationServer2D::INFO_AGENT_COUNT), 1);
			navigation_server->agent_set_map(agent, RID());
			navigation_server->free(map);
			navigation_server->process(0.0); // Give server some cycles to commit.
			CHECK_EQ(navigation_server->get_process_info(NavigationServer2D::INFO_AGENT_COUNT), 0);
		}

		navigation_server->free(agent);
	}

	TEST_CASE("[NavigationServer2D] Server should manage map properly") {
		NavigationServer2D *navigation_server = NavigationServer2D::get_singleton();

		RID map;
		CHECK_FALSE(map.is_valid());

		SUBCASE("Queries against invalid map should return empty or invalid values") {
			ERR_PRINT_OFF;
			CHECK_EQ(navigation_server->map_get_closest_point(map, Vector2(7, 7)), Vector2());
			CHECK_FALSE(navigation_server->map_get_closest_point_owner(map, Vector2(7, 7)).is_valid());
			CHECK_EQ(navigation_server->map_get_path(map, Vector2(7, 7), Vector2(8, 8), true).size(), 0);
			CHECK_EQ(navigation_server->map_get_path(map, Vector2(7, 7), Vector2(8, 8), false).size(), 0);

			Ref<NavigationPathQueryParameters2D> query_parameters;
			query_parameters.instantiate();
			query_parameters->set_map(map);
			query_parameters->set_start_position(Vector2(7, 7));
			query_parameters->set_target_position(Vector2(8, 8));
			Ref<NavigationPathQueryResult2D> query_result;
			query_result.instantiate();
			navigation_server->query_path(query_parameters, query_result);
			CHECK_EQ(query_result->get_path().size(), 0);
			CHECK_EQ(query_result->get_path_types().size(), 0);
			CHECK_EQ(query_result->get_path_rids().size(), 0);
			CHECK_EQ(query_result->get_path_owner_ids().size(), 0);
			ERR_PRINT_ON;
		}

		map = navigation_server->map_create();
		CHECK(map.is_valid());
		CHECK_EQ(navigation_server->get_maps().size(), 1);

		SUBCASE("'ProcessInfo' should not report inactive map") {
			CHECK_EQ(navigation_server->get_process_info(NavigationServer2D::INFO_ACTIVE_MAPS), 0);
		}

		SUBCASE("Setters/getters should work") {
			navigation_server->map_set_cell_size(map, 0.55);
			navigation_server->map_set_edge_connection_margin(map, 0.66);
			navigation_server->map_set_link_connection_radius(map, 0.77);
			bool initial_use_edge_connections = navigation_server->map_get_use_edge_connections(map);
			navigation_server->map_set_use_edge_connections(map, !initial_use_edge_connections);
			navigation_server->process(0.0); // Give server some cycles to commit.

			CHECK_EQ(navigation_server->map_get_cell_size(map), doctest::Approx(0.55));
			CHECK_EQ(navigation_server->map_get_edge_connection_margin(map), doctest::Approx(0.66));
			CHECK_EQ(navigation_server->map_get_link_connection_radius(map), doctest::Approx(0.77));
			CHECK_EQ(navigation_server->map_get_use_edge_connections(map), !initial_use_edge_connections);
		}

		SUBCASE("'ProcessInfo' should report map iff active") {
			navigation_server->map_set_active(map, true);
			navigation_server->process(0.0); // Give server some cycles to commit.
			CHECK(navigation_server->map_is_active(map));
			CHECK_EQ(navigation_server->get_process_info(NavigationServer2D::INFO_ACTIVE_MAPS), 1);
			navigation_server->map_set_active(map, false);
			navigation_server->process(0.0); // Give server some cycles to commit.
			CHECK_EQ(navigation_server->get_process_info(NavigationServer2D::INFO_ACTIVE_MAPS), 0);
		}

		SUBCASE("Number of agents should be reported properly") {
			RID agent = navigation_server->agent_create();
			CHECK(agent.is_valid());
			navigation_server->agent_set_map(agent, map);
			navigation_server->process(0.0); // Give server some cycles to commit.
			CHECK_EQ(navigation_server->map_get_agents(map).size(), 1);
			navigation_server->free(agent);
			navigation_server->process(0.0); // Give server some cycles to commit.
			CHECK_EQ(navigation_server->map_get_agents(map).size(), 0);
		}

		SUBCASE("Number of links should be reported properly") {
			RID link = navigation_server->link_create();
			CHECK(link.is_valid());
			navigation_server->link_set_map(link, map);
			navigation_server->process(0.0); // Give server some cycles to commit.
			CHECK_EQ(navigation_server->map_get_links(map).size(), 1);
			navigation_server->free(link);
			navigation_server->process(0.0); // Give server some cycles to commit.
			CHECK_EQ(navigation_server->map_get_links(map).size(), 0);
		}

		SUBCASE("Number of obstacles should be reported properly") {
			RID obstacle = navigation_server->obstacle_create();
			CHECK(obstacle.is_valid());
			navigation_server->obstacle_set_map(obstacle, map);
			navigation_server->process(0.0); // Give server some cycles to commit.
			CHECK_EQ(navigation_server->map_get_obstacles(map).size(), 1);
			navigation_server->free(obstacle);
			navigation_server->process(0.0); // Give server some cycles to commit.
			CHECK_EQ(navigation_server->map_get_obstacles(map).size(), 0);
		}

		SUBCASE("Number of regions should be reported properly") {
			RID region = navigation_server->region_create();
			CHECK(region.is_valid());
			navigation_server->region_set_map(region, map);
			navigation_server->process(0.0); // Give server some cycles to commit.
			CHECK_EQ(navigation_server->map_get_regions(map).size(), 1);
			navigation_server->free(region);
			navigation_server->process(0.0); // Give server some cycles to commit.
			CHECK_EQ(navigation_server->map_get_regions(map).size(), 0);
		}

		SUBCASE("Queries against empty map should return empty or invalid values") {
			navigation_server->map_set_active(map, true);
			navigation_server->process(0.0); // Give server some cycles to commit.

			ERR_PRINT_OFF;
			CHECK_EQ(navigation_server->map_get_closest_point(map, Vector2(7, 7)), Vector2());
			CHECK_FALSE(navigation_server->map_get_closest_point_owner(map, Vector2(7, 7)).is_valid());
			CHECK_EQ(navigation_server->map_get_path(map, Vector2(7, 7), Vector2(8, 8), true).size(), 0);
			CHECK_EQ(navigation_server->map_get_path(map, Vector2(7, 7), Vector2(8, 8), false).size(), 0);

			Ref<NavigationPathQueryParameters2D> query_parameters;
			query_parameters.instantiate();
			query_parameters->set_map(map);
			query_parameters->set_start_position(Vector2(7, 7));
			query_parameters->set_target_position(Vector2(8, 8));
			Ref<NavigationPathQueryResult2D> query_result;
			query_result.instantiate();
			navigation_server->query_path(query_parameters, query_result);
			CHECK_EQ(query_result->get_path().size(), 0);
			CHECK_EQ(query_result->get_path_types().size(), 0);
			CHECK_EQ(query_result->get_path_rids().size(), 0);
			CHECK_EQ(query_result->get_path_owner_ids().size(), 0);
			ERR_PRINT_ON;

			navigation_server->map_set_active(map, false);
			navigation_server->process(0.0); // Give server some cycles to commit.
		}

		navigation_server->free(map);
		navigation_server->process(0.0); // Give server some cycles to actually remove map.
		CHECK_EQ(navigation_server->get_maps().size(), 0);
	}

	TEST_CASE("[NavigationServer2D] Server should manage link properly") {
		NavigationServer2D *navigation_server = NavigationServer2D::get_singleton();

		RID link = navigation_server->link_create();
		CHECK(link.is_valid());

		SUBCASE("'ProcessInfo' should not report dangling link") {
			CHECK_EQ(navigation_server->get_process_info(NavigationServer2D::INFO_LINK_COUNT), 0);
		}

		SUBCASE("Setters/getters should work") {
			bool initial_bidirectional = navigation_server->link_is_bidirectional(link);
			navigation_server->link_set_bidirectional(link, !initial_bidirectional);
			navigation_server->link_set_end_position(link, Vector2(7, 7));
			navigation_server->link_set_enter_cost(link, 0.55);
			navigation_server->link_set_navigation_layers(link, 6);
			navigation_server->link_set_owner_id(link, ObjectID((int64_t)7));
			navigation_server->link_set_start_position(link, Vector2(8, 8));
			navigation_server->link_set_travel_cost(link, 0.66);
			navigation_server->process(0.0); // Give server some cycles to commit.

			CHECK_EQ(navigation_server->link_is_bidirectional(link), !initial_bidirectional);
			CHECK_EQ(navigation_server->link_get_end_position(link), Vector2(7, 7));
			CHECK_EQ(navigation_server->link_get_enter_cost(link), doctest::Approx(0.55));
			CHECK_EQ(navigation_server->link_get_navigation_layers(link), 6);
			CHECK_EQ(navigation_server->link_get_owner_id(link), ObjectID((int64_t)7));
			CHECK_EQ(navigation_server->link_get_start_position(link), Vector2(8, 8));
			CHECK_EQ(navigation_server->link_get_travel_cost(link), doctest::Approx(0.66));
		}

		SUBCASE("'ProcessInfo' should report link with active map") {
			RID map = navigation_server->map_create();
			CHECK(map.is_valid());
			navigation_server->map_set_active(map, true);
			navigation_server->link_set_map(link, map);
			navigation_server->process(0.0); // Give server some cycles to commit.
			CHECK_EQ(navigation_server->get_process_info(NavigationServer2D::INFO_LINK_COUNT), 1);
			navigation_server->link_set_map(link, RID());
			navigation_server->free(map);
			navigation_server->process(0.0); // Give server some cycles to commit.
			CHECK_EQ(navigation_server->get_process_info(NavigationServer2D::INFO_LINK_COUNT), 0);
		}

		navigation_server->free(link);
	}

	TEST_CASE("[NavigationServer2D] Server should manage obstacles properly") {
		NavigationServer2D *navigation_server = NavigationServer2D::get_singleton();

		RID obstacle = navigation_server->obstacle_create();
		CHECK(obstacle.is_valid());

		// TODO: Add tests for setters/getters once getters are added.

		navigation_server->free(obstacle);
	}

	TEST_CASE("[NavigationServer2D] Server should manage regions properly") {
		NavigationServer2D *navigation_server = NavigationServer2D::get_singleton();

		RID region = navigation_server->region_create();
		CHECK(region.is_valid());

		SUBCASE("'ProcessInfo' should not report dangling region") {
			CHECK_EQ(navigation_server->get_process_info(NavigationServer2D::INFO_REGION_COUNT), 0);
		}

		SUBCASE("Setters/getters should work") {
			bool initial_use_edge_connections = navigation_server->region_get_use_edge_connections(region);
			navigation_server->region_set_enter_cost(region, 0.55);
			navigation_server->region_set_navigation_layers(region, 5);
			navigation_server->region_set_owner_id(region, ObjectID((int64_t)7));
			navigation_server->region_set_travel_cost(region, 0.66);
			navigation_server->region_set_use_edge_connections(region, !initial_use_edge_connections);
			navigation_server->process(0.0); // Give server some cycles to commit.

			CHECK_EQ(navigation_server->region_get_enter_cost(region), doctest::Approx(0.55));
			CHECK_EQ(navigation_server->region_get_navigation_layers(region), 5);
			CHECK_EQ(navigation_server->region_get_owner_id(region), ObjectID((int64_t)7));
			CHECK_EQ(navigation_server->region_get_travel_cost(region), doctest::Approx(0.66));
			CHECK_EQ(navigation_server->region_get_use_edge_connections(region), !initial_use_edge_connections);
		}

		SUBCASE("'ProcessInfo' should report region with active map") {
			RID map = navigation_server->map_create();
			CHECK(map.is_valid());
			navigation_server->map_set_active(map, true);
			navigation_server->region_set_map(region, map);
			navigation_server->process(0.0); // Give server some cycles to commit.
			CHECK_EQ(navigation_server->get_process_info(NavigationServer2D::INFO_REGION_COUNT), 1);
			navigation_server->region_set_map(region, RID());
			navigation_server->free(map);
			navigation_server->process(0.0); // Give server some cycles to commit.
			CHECK_EQ(navigation_server->get_process_info(NavigationServer2D::INFO_REGION_COUNT), 0);
		}

		SUBCASE("Queries against empty region should return empty or invalid values") {
			ERR_PRINT_OFF;
			CHECK_EQ(navigation_server->region_get_connections_count(region), 0);
			CHECK_EQ(navigation_server->region_get_connection_pathway_end(region, 55), Vector2());
			CHECK_EQ(navigation_server->region_get_connection_pathway_start(region, 55), Vector2());
			ERR_PRINT_ON;
		}

		navigation_server->free(region);
	}

	// This test case does not check precise values on purpose - to not be too sensitivte.
	TEST_CASE("[NavigationServer2D] Server should move agent properly") {
		NavigationServer2D *navigation_server = NavigationServer2D::get_singleton();

		RID map = navigation_server->map_create();
		RID agent = navigation_server->agent_create();

		navigation_server->map_set_active(map, true);
		navigation_server->agent_set_map(agent, map);
		navigation_server->agent_set_avoidance_enabled(agent, true);
		navigation_server->agent_set_velocity(agent, Vector2(1, 1));
		CallableMock agent_avoidance_callback_mock;
		navigation_server->agent_set_avoidance_callback(agent, callable_mp(&agent_avoidance_callback_mock, &CallableMock::function1));
		CHECK_EQ(agent_avoidance_callback_mock.function1_calls, 0);
		navigation_server->process(0.0); // Give server some cycles to commit.
		CHECK_EQ(agent_avoidance_callback_mock.function1_calls, 1);
		CHECK_NE(agent_avoidance_callback_mock.function1_latest_arg0, Vector3(0, 0, 0));

		navigation_server->free(agent);
		navigation_server->free(map);
	}

	// This test case does not check precise values on purpose - to not be too sensitivte.
	TEST_CASE("[NavigationServer2D] Server should make agents avoid each other when avoidance enabled") {
		NavigationServer2D *navigation_server = NavigationServer2D::get_singleton();

		RID map = navigation_server->map_create();
		RID agent_1 = navigation_server->agent_create();
		RID agent_2 = navigation_server->agent_create();

		navigation_server->map_set_active(map, true);

		navigation_server->agent_set_map(agent_1, map);
		navigation_server->agent_set_avoidance_enabled(agent_1, true);
		navigation_server->agent_set_position(agent_1, Vector2(0, 0));
		navigation_server->agent_set_radius(agent_1, 1);
		navigation_server->agent_set_velocity(agent_1, Vector2(1, 0));
		CallableMock agent_1_avoidance_callback_mock;
		navigation_server->agent_set_avoidance_callback(agent_1, callable_mp(&agent_1_avoidance_callback_mock, &CallableMock::function1));

		navigation_server->agent_set_map(agent_2, map);
		navigation_server->agent_set_avoidance_enabled(agent_2, true);
		navigation_server->agent_set_position(agent_2, Vector2(2.5, 0.5));
		navigation_server->agent_set_radius(agent_2, 1);
		navigation_server->agent_set_velocity(agent_2, Vector2(-1, 0));
		CallableMock agent_2_avoidance_callback_mock;
		navigation_server->agent_set_avoidance_callback(agent_2, callable_mp(&agent_2_avoidance_callback_mock, &CallableMock::function1));

		CHECK_EQ(agent_1_avoidance_callback_mock.function1_calls, 0);
		CHECK_EQ(agent_2_avoidance_callback_mock.function1_calls, 0);
		navigation_server->process(0.0); // Give server some cycles to commit.
		CHECK_EQ(agent_1_avoidance_callback_mock.function1_calls, 1);
		CHECK_EQ(agent_2_avoidance_callback_mock.function1_calls, 1);
		Vector3 agent_1_safe_velocity = agent_1_avoidance_callback_mock.function1_latest_arg0;
		Vector3 agent_2_safe_velocity = agent_2_avoidance_callback_mock.function1_latest_arg0;
		CHECK_MESSAGE(agent_1_safe_velocity.x > 0, "agent 1 should move a bit along desired velocity (+X)");
		CHECK_MESSAGE(agent_2_safe_velocity.x < 0, "agent 2 should move a bit along desired velocity (-X)");
		CHECK_MESSAGE(agent_1_safe_velocity.z < 0, "agent 1 should move a bit to the side so that it avoids agent 2");
		CHECK_MESSAGE(agent_2_safe_velocity.z > 0, "agent 2 should move a bit to the side so that it avoids agent 1");

		navigation_server->free(agent_2);
		navigation_server->free(agent_1);
		navigation_server->free(map);
	}

	TEST_CASE("[NavigationServer2D] Server should make agents avoid dynamic obstacles when avoidance enabled") {
		NavigationServer2D *navigation_server = NavigationServer2D::get_singleton();

		RID map = navigation_server->map_create();
		RID agent_1 = navigation_server->agent_create();
		RID obstacle_1 = navigation_server->obstacle_create();

		navigation_server->map_set_active(map, true);

		navigation_server->agent_set_map(agent_1, map);
		navigation_server->agent_set_avoidance_enabled(agent_1, true);
		navigation_server->agent_set_position(agent_1, Vector2(0, 0));
		navigation_server->agent_set_radius(agent_1, 1);
		navigation_server->agent_set_velocity(agent_1, Vector2(1, 0));
		CallableMock agent_1_avoidance_callback_mock;
		navigation_server->agent_set_avoidance_callback(agent_1, callable_mp(&agent_1_avoidance_callback_mock, &CallableMock::function1));

		navigation_server->obstacle_set_map(obstacle_1, map);
		navigation_server->obstacle_set_avoidance_enabled(obstacle_1, true);
		navigation_server->obstacle_set_position(obstacle_1, Vector2(2.5, 0.5));
		navigation_server->obstacle_set_radius(obstacle_1, 1);

		CHECK_EQ(agent_1_avoidance_callback_mock.function1_calls, 0);
		navigation_server->process(0.0); // Give server some cycles to commit.
		CHECK_EQ(agent_1_avoidance_callback_mock.function1_calls, 1);
		Vector3 agent_1_safe_velocity = agent_1_avoidance_callback_mock.function1_latest_arg0;
		CHECK_MESSAGE(agent_1_safe_velocity.x > 0, "Agent 1 should move a bit along desired velocity (+X).");
		CHECK_MESSAGE(agent_1_safe_velocity.z < 0, "Agent 1 should move a bit to the side so that it avoids obstacle.");

		navigation_server->free(obstacle_1);
		navigation_server->free(agent_1);
		navigation_server->free(map);
		navigation_server->process(0.0); // Give server some cycles to commit.
	}

	TEST_CASE("[NavigationServer2D] Server should make agents avoid static obstacles when avoidance enabled") {
		NavigationServer2D *navigation_server = NavigationServer2D::get_singleton();

		RID map = navigation_server->map_create();
		RID agent_1 = navigation_server->agent_create();
		RID agent_2 = navigation_server->agent_create();
		RID obstacle_1 = navigation_server->obstacle_create();

		navigation_server->map_set_active(map, true);

		navigation_server->agent_set_map(agent_1, map);
		navigation_server->agent_set_avoidance_enabled(agent_1, true);
		navigation_server->agent_set_radius(agent_1, 1.6); // Have hit the obstacle already.
		navigation_server->agent_set_velocity(agent_1, Vector2(1, 0));
		CallableMock agent_1_avoidance_callback_mock;
		navigation_server->agent_set_avoidance_callback(agent_1, callable_mp(&agent_1_avoidance_callback_mock, &CallableMock::function1));

		navigation_server->agent_set_map(agent_2, map);
		navigation_server->agent_set_avoidance_enabled(agent_2, true);
		navigation_server->agent_set_radius(agent_2, 1.4); // Haven't hit the obstacle yet.
		navigation_server->agent_set_velocity(agent_2, Vector2(1, 0));
		CallableMock agent_2_avoidance_callback_mock;
		navigation_server->agent_set_avoidance_callback(agent_2, callable_mp(&agent_2_avoidance_callback_mock, &CallableMock::function1));

		navigation_server->obstacle_set_map(obstacle_1, map);
		navigation_server->obstacle_set_avoidance_enabled(obstacle_1, true);
		PackedVector2Array obstacle_1_vertices;

		SUBCASE("Static obstacles should work on ground level") {
			navigation_server->agent_set_position(agent_1, Vector2(0, 0));
			navigation_server->agent_set_position(agent_2, Vector2(0, 5));
			obstacle_1_vertices.push_back(Vector2(1.5, 0.5));
			obstacle_1_vertices.push_back(Vector2(1.5, 4.5));
		}

		navigation_server->obstacle_set_vertices(obstacle_1, obstacle_1_vertices);

		CHECK_EQ(agent_1_avoidance_callback_mock.function1_calls, 0);
		CHECK_EQ(agent_2_avoidance_callback_mock.function1_calls, 0);
		navigation_server->process(0.0); // Give server some cycles to commit.
		CHECK_EQ(agent_1_avoidance_callback_mock.function1_calls, 1);
		CHECK_EQ(agent_2_avoidance_callback_mock.function1_calls, 1);
		Vector3 agent_1_safe_velocity = agent_1_avoidance_callback_mock.function1_latest_arg0;
		Vector3 agent_2_safe_velocity = agent_2_avoidance_callback_mock.function1_latest_arg0;
		CHECK_MESSAGE(agent_1_safe_velocity.x > 0, "Agent 1 should move a bit along desired velocity (+X).");
		CHECK_MESSAGE(agent_1_safe_velocity.z < 0, "Agent 1 should move a bit to the side so that it avoids obstacle.");
		CHECK_MESSAGE(agent_2_safe_velocity.x > 0, "Agent 2 should move a bit along desired velocity (+X).");
		CHECK_MESSAGE(agent_2_safe_velocity.z == 0, "Agent 2 should not move to the side.");

		navigation_server->free(obstacle_1);
		navigation_server->free(agent_2);
		navigation_server->free(agent_1);
		navigation_server->free(map);
		navigation_server->process(0.0); // Give server some cycles to commit.
	}

	TEST_CASE("[NavigationServer2D][SceneTree] Server should be able to parse geometry") {
		NavigationServer2D *navigation_server = NavigationServer2D::get_singleton();

		// Prepare scene tree with simple mesh to serve as an input geometry.
		Node2D *node_2d = memnew(Node2D);
		SceneTree::get_singleton()->get_root()->add_child(node_2d);
		Polygon2D *polygon = memnew(Polygon2D);
		polygon->set_polygon(PackedVector2Array({ Vector2(200.0, 200.0), Vector2(400.0, 200.0), Vector2(400.0, 400.0), Vector2(200.0, 400.0) }));
		node_2d->add_child(polygon);

		// TODO: Use MeshInstance2D as well?

		Ref<NavigationPolygon> navigation_polygon;
		navigation_polygon.instantiate();
		Ref<NavigationMeshSourceGeometryData2D> source_geometry;
		source_geometry.instantiate();
		CHECK_EQ(source_geometry->get_traversable_outlines().size(), 0);
		CHECK_EQ(source_geometry->get_obstruction_outlines().size(), 0);

		navigation_server->parse_source_geometry_data(navigation_polygon, source_geometry, polygon);
		CHECK_EQ(source_geometry->get_traversable_outlines().size(), 0);
		REQUIRE_EQ(source_geometry->get_obstruction_outlines().size(), 1);
		CHECK_EQ(((PackedVector2Array)source_geometry->get_obstruction_outlines()[0]).size(), 4);

		SUBCASE("By default, parsing should remove any data that was parsed before") {
			navigation_server->parse_source_geometry_data(navigation_polygon, source_geometry, polygon);
			CHECK_EQ(source_geometry->get_traversable_outlines().size(), 0);
			REQUIRE_EQ(source_geometry->get_obstruction_outlines().size(), 1);
			CHECK_EQ(((PackedVector2Array)source_geometry->get_obstruction_outlines()[0]).size(), 4);
		}

		SUBCASE("Parsed geometry should be extendible with other geometry") {
			source_geometry->merge(source_geometry); // Merging with itself.
			CHECK_EQ(source_geometry->get_traversable_outlines().size(), 0);
			REQUIRE_EQ(source_geometry->get_obstruction_outlines().size(), 2);
			const PackedVector2Array obstruction_outline_1 = source_geometry->get_obstruction_outlines()[0];
			const PackedVector2Array obstruction_outline_2 = source_geometry->get_obstruction_outlines()[1];
			REQUIRE_EQ(obstruction_outline_1.size(), 4);
			REQUIRE_EQ(obstruction_outline_2.size(), 4);
			CHECK_EQ(obstruction_outline_1[0], obstruction_outline_2[0]);
			CHECK_EQ(obstruction_outline_1[1], obstruction_outline_2[1]);
			CHECK_EQ(obstruction_outline_1[2], obstruction_outline_2[2]);
			CHECK_EQ(obstruction_outline_1[3], obstruction_outline_2[3]);
		}

		memdelete(polygon);
		memdelete(node_2d);
	}

	// This test case uses only public APIs on purpose - other test cases use simplified baking.
	TEST_CASE("[NavigationServer2D][SceneTree] Server should be able to bake map correctly") {
		NavigationServer2D *navigation_server = NavigationServer2D::get_singleton();

		// Prepare scene tree with simple mesh to serve as an input geometry.
		Node2D *node_2d = memnew(Node2D);
		SceneTree::get_singleton()->get_root()->add_child(node_2d);
		Polygon2D *polygon = memnew(Polygon2D);
		polygon->set_polygon(PackedVector2Array({ Vector2(-200.0, -200.0), Vector2(200.0, -200.0), Vector2(200.0, 200.0), Vector2(-200.0, 200.0) }));
		node_2d->add_child(polygon);

		// TODO: Use MeshInstance2D as well?

		// Prepare anything necessary to bake navigation polygon.
		RID map = navigation_server->map_create();
		RID region = navigation_server->region_create();
		Ref<NavigationPolygon> navigation_polygon;
		navigation_polygon.instantiate();
		navigation_polygon->add_outline(PackedVector2Array({ Vector2(-1000.0, -1000.0), Vector2(1000.0, -1000.0), Vector2(1000.0, 1000.0), Vector2(-1000.0, 1000.0) }));
		navigation_server->map_set_active(map, true);
		navigation_server->map_set_use_async_iterations(map, false);
		navigation_server->region_set_map(region, map);
		navigation_server->region_set_navigation_polygon(region, navigation_polygon);
		navigation_server->process(0.0); // Give server some cycles to commit.

		CHECK_EQ(navigation_polygon->get_polygon_count(), 0);
		CHECK_EQ(navigation_polygon->get_vertices().size(), 0);
		CHECK_EQ(navigation_polygon->get_outline_count(), 1);

		Ref<NavigationMeshSourceGeometryData2D> source_geometry;
		source_geometry.instantiate();
		navigation_server->parse_source_geometry_data(navigation_polygon, source_geometry, node_2d);
		navigation_server->bake_from_source_geometry_data(navigation_polygon, source_geometry, Callable());
		// FIXME: The above line should trigger the update (line below) under the hood.
		navigation_server->region_set_navigation_polygon(region, navigation_polygon); // Force update.
		CHECK_EQ(navigation_polygon->get_polygon_count(), 4);
		CHECK_EQ(navigation_polygon->get_vertices().size(), 8);
		CHECK_EQ(navigation_polygon->get_outline_count(), 1);

		SUBCASE("Map should emit signal and take newly baked navigation mesh into account") {
			SIGNAL_WATCH(navigation_server, "map_changed");
			SIGNAL_CHECK_FALSE("map_changed");
			navigation_server->process(0.0); // Give server some cycles to commit.
			SIGNAL_CHECK("map_changed", build_array(build_array(map)));
			SIGNAL_UNWATCH(navigation_server, "map_changed");
			CHECK_NE(navigation_server->map_get_closest_point(map, Vector2(0, 0)), Vector2(0, 0));
		}

		navigation_server->free(region);
		navigation_server->free(map);
		navigation_server->process(0.0); // Give server some cycles to commit.

		memdelete(polygon);
		memdelete(node_2d);
	}

	// This test case does not check precise values on purpose - to not be too sensitivte.
	TEST_CASE("[NavigationServer2D] Server should respond to queries against valid map properly") {
		NavigationServer2D *navigation_server = NavigationServer2D::get_singleton();
		Ref<NavigationPolygon> navigation_polygon;
		navigation_polygon.instantiate();
		Ref<NavigationMeshSourceGeometryData2D> source_geometry;
		source_geometry.instantiate();

		navigation_polygon->add_outline(PackedVector2Array({ Vector2(-1000.0, -1000.0), Vector2(1000.0, -1000.0), Vector2(1000.0, 1000.0), Vector2(-1000.0, 1000.0) }));

		// TODO: Other input?
		source_geometry->add_obstruction_outline(PackedVector2Array({ Vector2(-200.0, -200.0), Vector2(200.0, -200.0), Vector2(200.0, 200.0), Vector2(-200.0, 200.0) }));

		navigation_server->bake_from_source_geometry_data(navigation_polygon, source_geometry, Callable());
		CHECK_NE(navigation_polygon->get_polygon_count(), 0);
		CHECK_NE(navigation_polygon->get_vertices().size(), 0);
		CHECK_NE(navigation_polygon->get_outline_count(), 0);

		RID map = navigation_server->map_create();
		RID region = navigation_server->region_create();
		navigation_server->map_set_active(map, true);
		navigation_server->map_set_use_async_iterations(map, false);
		navigation_server->region_set_map(region, map);
		navigation_server->region_set_navigation_polygon(region, navigation_polygon);
		navigation_server->process(0.0); // Give server some cycles to commit.

		SUBCASE("Simple queries should return non-default values") {
			CHECK_NE(navigation_server->map_get_closest_point(map, Vector2(0.0, 0.0)), Vector2(0, 0));
			CHECK(navigation_server->map_get_closest_point_owner(map, Vector2(0.0, 0.0)).is_valid());
			CHECK_NE(navigation_server->map_get_path(map, Vector2(0, 0), Vector2(10, 10), true).size(), 0);
			CHECK_NE(navigation_server->map_get_path(map, Vector2(0, 0), Vector2(10, 10), false).size(), 0);
		}

		SUBCASE("Elaborate query with 'CORRIDORFUNNEL' post-processing should yield non-empty result") {
			Ref<NavigationPathQueryParameters2D> query_parameters;
			query_parameters.instantiate();
			query_parameters->set_map(map);
			query_parameters->set_start_position(Vector2(0, 0));
			query_parameters->set_target_position(Vector2(10, 10));
			query_parameters->set_path_postprocessing(NavigationPathQueryParameters2D::PATH_POSTPROCESSING_CORRIDORFUNNEL);
			Ref<NavigationPathQueryResult2D> query_result;
			query_result.instantiate();
			navigation_server->query_path(query_parameters, query_result);
			CHECK_NE(query_result->get_path().size(), 0);
			CHECK_NE(query_result->get_path_types().size(), 0);
			CHECK_NE(query_result->get_path_rids().size(), 0);
			CHECK_NE(query_result->get_path_owner_ids().size(), 0);
		}

		SUBCASE("Elaborate query with 'EDGECENTERED' post-processing should yield non-empty result") {
			Ref<NavigationPathQueryParameters2D> query_parameters;
			query_parameters.instantiate();
			query_parameters->set_map(map);
			query_parameters->set_start_position(Vector2(10, 10));
			query_parameters->set_target_position(Vector2(0, 0));
			query_parameters->set_path_postprocessing(NavigationPathQueryParameters2D::PATH_POSTPROCESSING_EDGECENTERED);
			Ref<NavigationPathQueryResult2D> query_result;
			query_result.instantiate();
			navigation_server->query_path(query_parameters, query_result);
			CHECK_NE(query_result->get_path().size(), 0);
			CHECK_NE(query_result->get_path_types().size(), 0);
			CHECK_NE(query_result->get_path_rids().size(), 0);
			CHECK_NE(query_result->get_path_owner_ids().size(), 0);
		}

		SUBCASE("Elaborate query with non-matching navigation layer mask should yield empty result") {
			Ref<NavigationPathQueryParameters2D> query_parameters;
			query_parameters.instantiate();
			query_parameters->set_map(map);
			query_parameters->set_start_position(Vector2(10, 10));
			query_parameters->set_target_position(Vector2(0, 0));
			query_parameters->set_navigation_layers(2);
			Ref<NavigationPathQueryResult2D> query_result;
			query_result.instantiate();
			navigation_server->query_path(query_parameters, query_result);
			CHECK_EQ(query_result->get_path().size(), 0);
			CHECK_EQ(query_result->get_path_types().size(), 0);
			CHECK_EQ(query_result->get_path_rids().size(), 0);
			CHECK_EQ(query_result->get_path_owner_ids().size(), 0);
		}

		SUBCASE("Elaborate query without metadata flags should yield path only") {
			Ref<NavigationPathQueryParameters2D> query_parameters;
			query_parameters.instantiate();
			query_parameters->set_map(map);
			query_parameters->set_start_position(Vector2(10, 10));
			query_parameters->set_target_position(Vector2(0, 0));
			query_parameters->set_metadata_flags(0);
			Ref<NavigationPathQueryResult2D> query_result;
			query_result.instantiate();
			navigation_server->query_path(query_parameters, query_result);
			CHECK_NE(query_result->get_path().size(), 0);
			CHECK_EQ(query_result->get_path_types().size(), 0);
			CHECK_EQ(query_result->get_path_rids().size(), 0);
			CHECK_EQ(query_result->get_path_owner_ids().size(), 0);
		}

		navigation_server->free(region);
		navigation_server->free(map);
		navigation_server->process(0.0); // Give server some cycles to commit.
	}

	TEST_CASE("[NavigationServer2D] Server should simplify path properly") {
		real_t simplify_epsilon = 0.2;
		Vector<Vector2> source_path;
		source_path.resize(7);
		source_path.write[0] = Vector2(0.0, 0.0);
		source_path.write[1] = Vector2(0.0, 1.0); // This point needs to go.
		source_path.write[2] = Vector2(0.0, 2.0); // This point needs to go.
		source_path.write[3] = Vector2(0.0, 2.0);
		source_path.write[4] = Vector2(2.0, 3.0);
		source_path.write[5] = Vector2(2.5, 4.0); // This point needs to go.
		source_path.write[6] = Vector2(3.0, 5.0);
		Vector<Vector2> simplified_path = NavigationServer2D::get_singleton()->simplify_path(source_path, simplify_epsilon);
		CHECK_EQ(simplified_path.size(), 4);
	}
}
} //namespace TestNavigationServer2D

#endif // TEST_NAVIGATION_SERVER_2D_H

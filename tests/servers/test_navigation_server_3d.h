/**************************************************************************/
/*  test_navigation_server_3d.h                                           */
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

#include "scene/3d/mesh_instance_3d.h"
#include "scene/resources/3d/primitive_meshes.h"
#include "servers/navigation_server_3d.h"

#include "modules/navigation/nav_utils.h"

namespace TestNavigationServer3D {

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

TEST_SUITE("[Navigation]") {
	TEST_CASE("[NavigationServer3D] Server should be empty when initialized") {
		NavigationServer3D *navigation_server = NavigationServer3D::get_singleton();
		CHECK_EQ(navigation_server->get_maps().size(), 0);

		SUBCASE("'ProcessInfo' should report all counters empty as well") {
			CHECK_EQ(navigation_server->get_process_info(NavigationServer3D::INFO_ACTIVE_MAPS), 0);
			CHECK_EQ(navigation_server->get_process_info(NavigationServer3D::INFO_REGION_COUNT), 0);
			CHECK_EQ(navigation_server->get_process_info(NavigationServer3D::INFO_AGENT_COUNT), 0);
			CHECK_EQ(navigation_server->get_process_info(NavigationServer3D::INFO_LINK_COUNT), 0);
			CHECK_EQ(navigation_server->get_process_info(NavigationServer3D::INFO_POLYGON_COUNT), 0);
			CHECK_EQ(navigation_server->get_process_info(NavigationServer3D::INFO_EDGE_COUNT), 0);
			CHECK_EQ(navigation_server->get_process_info(NavigationServer3D::INFO_EDGE_MERGE_COUNT), 0);
			CHECK_EQ(navigation_server->get_process_info(NavigationServer3D::INFO_EDGE_CONNECTION_COUNT), 0);
			CHECK_EQ(navigation_server->get_process_info(NavigationServer3D::INFO_EDGE_FREE_COUNT), 0);
		}
	}

	TEST_CASE("[NavigationServer3D] Server should manage agent properly") {
		NavigationServer3D *navigation_server = NavigationServer3D::get_singleton();

		RID agent = navigation_server->agent_create();
		CHECK(agent.is_valid());

		SUBCASE("'ProcessInfo' should not report dangling agent") {
			CHECK_EQ(navigation_server->get_process_info(NavigationServer3D::INFO_AGENT_COUNT), 0);
		}

		SUBCASE("Setters/getters should work") {
			bool initial_use_3d_avoidance = navigation_server->agent_get_use_3d_avoidance(agent);
			navigation_server->agent_set_use_3d_avoidance(agent, !initial_use_3d_avoidance);
			navigation_server->process(0.0); // Give server some cycles to commit.

			CHECK_EQ(navigation_server->agent_get_use_3d_avoidance(agent), !initial_use_3d_avoidance);
			// TODO: Add remaining setters/getters once the missing getters are added.
		}

		SUBCASE("'ProcessInfo' should report agent with active map") {
			RID map = navigation_server->map_create();
			CHECK(map.is_valid());
			navigation_server->map_set_active(map, true);
			navigation_server->agent_set_map(agent, map);
			navigation_server->process(0.0); // Give server some cycles to commit.
			CHECK_EQ(navigation_server->get_process_info(NavigationServer3D::INFO_AGENT_COUNT), 1);
			navigation_server->agent_set_map(agent, RID());
			navigation_server->free(map);
			navigation_server->process(0.0); // Give server some cycles to commit.
			CHECK_EQ(navigation_server->get_process_info(NavigationServer3D::INFO_AGENT_COUNT), 0);
		}

		navigation_server->free(agent);
	}

	TEST_CASE("[NavigationServer3D] Server should manage map properly") {
		NavigationServer3D *navigation_server = NavigationServer3D::get_singleton();

		RID map;
		CHECK_FALSE(map.is_valid());

		SUBCASE("Queries against invalid map should return empty or invalid values") {
			ERR_PRINT_OFF;
			CHECK_EQ(navigation_server->map_get_closest_point(map, Vector3(7, 7, 7)), Vector3());
			CHECK_EQ(navigation_server->map_get_closest_point_normal(map, Vector3(7, 7, 7)), Vector3());
			CHECK_FALSE(navigation_server->map_get_closest_point_owner(map, Vector3(7, 7, 7)).is_valid());
			CHECK_EQ(navigation_server->map_get_closest_point_to_segment(map, Vector3(7, 7, 7), Vector3(8, 8, 8), true), Vector3());
			CHECK_EQ(navigation_server->map_get_closest_point_to_segment(map, Vector3(7, 7, 7), Vector3(8, 8, 8), false), Vector3());
			CHECK_EQ(navigation_server->map_get_path(map, Vector3(7, 7, 7), Vector3(8, 8, 8), true).size(), 0);
			CHECK_EQ(navigation_server->map_get_path(map, Vector3(7, 7, 7), Vector3(8, 8, 8), false).size(), 0);

			Ref<NavigationPathQueryParameters3D> query_parameters = memnew(NavigationPathQueryParameters3D);
			query_parameters->set_map(map);
			query_parameters->set_start_position(Vector3(7, 7, 7));
			query_parameters->set_target_position(Vector3(8, 8, 8));
			Ref<NavigationPathQueryResult3D> query_result = memnew(NavigationPathQueryResult3D);
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
			CHECK_EQ(navigation_server->get_process_info(NavigationServer3D::INFO_ACTIVE_MAPS), 0);
		}

		SUBCASE("Setters/getters should work") {
			navigation_server->map_set_cell_size(map, 0.55);
			navigation_server->map_set_edge_connection_margin(map, 0.66);
			navigation_server->map_set_link_connection_radius(map, 0.77);
			navigation_server->map_set_up(map, Vector3(1, 0, 0));
			bool initial_use_edge_connections = navigation_server->map_get_use_edge_connections(map);
			navigation_server->map_set_use_edge_connections(map, !initial_use_edge_connections);
			navigation_server->process(0.0); // Give server some cycles to commit.

			CHECK_EQ(navigation_server->map_get_cell_size(map), doctest::Approx(0.55));
			CHECK_EQ(navigation_server->map_get_edge_connection_margin(map), doctest::Approx(0.66));
			CHECK_EQ(navigation_server->map_get_link_connection_radius(map), doctest::Approx(0.77));
			CHECK_EQ(navigation_server->map_get_up(map), Vector3(1, 0, 0));
			CHECK_EQ(navigation_server->map_get_use_edge_connections(map), !initial_use_edge_connections);
		}

		SUBCASE("'ProcessInfo' should report map iff active") {
			navigation_server->map_set_active(map, true);
			navigation_server->process(0.0); // Give server some cycles to commit.
			CHECK(navigation_server->map_is_active(map));
			CHECK_EQ(navigation_server->get_process_info(NavigationServer3D::INFO_ACTIVE_MAPS), 1);
			navigation_server->map_set_active(map, false);
			navigation_server->process(0.0); // Give server some cycles to commit.
			CHECK_EQ(navigation_server->get_process_info(NavigationServer3D::INFO_ACTIVE_MAPS), 0);
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
			CHECK_EQ(navigation_server->map_get_closest_point(map, Vector3(7, 7, 7)), Vector3());
			CHECK_EQ(navigation_server->map_get_closest_point_normal(map, Vector3(7, 7, 7)), Vector3());
			CHECK_FALSE(navigation_server->map_get_closest_point_owner(map, Vector3(7, 7, 7)).is_valid());
			CHECK_EQ(navigation_server->map_get_closest_point_to_segment(map, Vector3(7, 7, 7), Vector3(8, 8, 8), true), Vector3());
			CHECK_EQ(navigation_server->map_get_closest_point_to_segment(map, Vector3(7, 7, 7), Vector3(8, 8, 8), false), Vector3());
			CHECK_EQ(navigation_server->map_get_path(map, Vector3(7, 7, 7), Vector3(8, 8, 8), true).size(), 0);
			CHECK_EQ(navigation_server->map_get_path(map, Vector3(7, 7, 7), Vector3(8, 8, 8), false).size(), 0);

			Ref<NavigationPathQueryParameters3D> query_parameters = memnew(NavigationPathQueryParameters3D);
			query_parameters->set_map(map);
			query_parameters->set_start_position(Vector3(7, 7, 7));
			query_parameters->set_target_position(Vector3(8, 8, 8));
			Ref<NavigationPathQueryResult3D> query_result = memnew(NavigationPathQueryResult3D);
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

	TEST_CASE("[NavigationServer3D] Server should manage link properly") {
		NavigationServer3D *navigation_server = NavigationServer3D::get_singleton();

		RID link = navigation_server->link_create();
		CHECK(link.is_valid());

		SUBCASE("'ProcessInfo' should not report dangling link") {
			CHECK_EQ(navigation_server->get_process_info(NavigationServer3D::INFO_LINK_COUNT), 0);
		}

		SUBCASE("Setters/getters should work") {
			bool initial_bidirectional = navigation_server->link_is_bidirectional(link);
			navigation_server->link_set_bidirectional(link, !initial_bidirectional);
			navigation_server->link_set_end_position(link, Vector3(7, 7, 7));
			navigation_server->link_set_enter_cost(link, 0.55);
			navigation_server->link_set_navigation_layers(link, 6);
			navigation_server->link_set_owner_id(link, ObjectID((int64_t)7));
			navigation_server->link_set_start_position(link, Vector3(8, 8, 8));
			navigation_server->link_set_travel_cost(link, 0.66);
			navigation_server->process(0.0); // Give server some cycles to commit.

			CHECK_EQ(navigation_server->link_is_bidirectional(link), !initial_bidirectional);
			CHECK_EQ(navigation_server->link_get_end_position(link), Vector3(7, 7, 7));
			CHECK_EQ(navigation_server->link_get_enter_cost(link), doctest::Approx(0.55));
			CHECK_EQ(navigation_server->link_get_navigation_layers(link), 6);
			CHECK_EQ(navigation_server->link_get_owner_id(link), ObjectID((int64_t)7));
			CHECK_EQ(navigation_server->link_get_start_position(link), Vector3(8, 8, 8));
			CHECK_EQ(navigation_server->link_get_travel_cost(link), doctest::Approx(0.66));
		}

		SUBCASE("'ProcessInfo' should report link with active map") {
			RID map = navigation_server->map_create();
			CHECK(map.is_valid());
			navigation_server->map_set_active(map, true);
			navigation_server->link_set_map(link, map);
			navigation_server->process(0.0); // Give server some cycles to commit.
			CHECK_EQ(navigation_server->get_process_info(NavigationServer3D::INFO_LINK_COUNT), 1);
			navigation_server->link_set_map(link, RID());
			navigation_server->free(map);
			navigation_server->process(0.0); // Give server some cycles to commit.
			CHECK_EQ(navigation_server->get_process_info(NavigationServer3D::INFO_LINK_COUNT), 0);
		}

		navigation_server->free(link);
	}

	TEST_CASE("[NavigationServer3D] Server should manage obstacles properly") {
		NavigationServer3D *navigation_server = NavigationServer3D::get_singleton();

		RID obstacle = navigation_server->obstacle_create();
		CHECK(obstacle.is_valid());

		// TODO: Add tests for setters/getters once getters are added.

		navigation_server->free(obstacle);
	}

	TEST_CASE("[NavigationServer3D] Server should manage regions properly") {
		NavigationServer3D *navigation_server = NavigationServer3D::get_singleton();

		RID region = navigation_server->region_create();
		CHECK(region.is_valid());

		SUBCASE("'ProcessInfo' should not report dangling region") {
			CHECK_EQ(navigation_server->get_process_info(NavigationServer3D::INFO_REGION_COUNT), 0);
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
			CHECK_EQ(navigation_server->get_process_info(NavigationServer3D::INFO_REGION_COUNT), 1);
			navigation_server->region_set_map(region, RID());
			navigation_server->free(map);
			navigation_server->process(0.0); // Give server some cycles to commit.
			CHECK_EQ(navigation_server->get_process_info(NavigationServer3D::INFO_REGION_COUNT), 0);
		}

		SUBCASE("Queries against empty region should return empty or invalid values") {
			ERR_PRINT_OFF;
			CHECK_EQ(navigation_server->region_get_connections_count(region), 0);
			CHECK_EQ(navigation_server->region_get_connection_pathway_end(region, 55), Vector3());
			CHECK_EQ(navigation_server->region_get_connection_pathway_start(region, 55), Vector3());
			ERR_PRINT_ON;
		}

		navigation_server->free(region);
	}

	// This test case does not check precise values on purpose - to not be too sensitivte.
	TEST_CASE("[NavigationServer3D] Server should move agent properly") {
		NavigationServer3D *navigation_server = NavigationServer3D::get_singleton();

		RID map = navigation_server->map_create();
		RID agent = navigation_server->agent_create();

		navigation_server->map_set_active(map, true);
		navigation_server->agent_set_map(agent, map);
		navigation_server->agent_set_avoidance_enabled(agent, true);
		navigation_server->agent_set_velocity(agent, Vector3(1, 0, 1));
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
	TEST_CASE("[NavigationServer3D] Server should make agents avoid each other when avoidance enabled") {
		NavigationServer3D *navigation_server = NavigationServer3D::get_singleton();

		RID map = navigation_server->map_create();
		RID agent_1 = navigation_server->agent_create();
		RID agent_2 = navigation_server->agent_create();

		navigation_server->map_set_active(map, true);

		navigation_server->agent_set_map(agent_1, map);
		navigation_server->agent_set_avoidance_enabled(agent_1, true);
		navigation_server->agent_set_position(agent_1, Vector3(0, 0, 0));
		navigation_server->agent_set_radius(agent_1, 1);
		navigation_server->agent_set_velocity(agent_1, Vector3(1, 0, 0));
		CallableMock agent_1_avoidance_callback_mock;
		navigation_server->agent_set_avoidance_callback(agent_1, callable_mp(&agent_1_avoidance_callback_mock, &CallableMock::function1));

		navigation_server->agent_set_map(agent_2, map);
		navigation_server->agent_set_avoidance_enabled(agent_2, true);
		navigation_server->agent_set_position(agent_2, Vector3(2.5, 0, 0.5));
		navigation_server->agent_set_radius(agent_2, 1);
		navigation_server->agent_set_velocity(agent_2, Vector3(-1, 0, 0));
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

	TEST_CASE("[NavigationServer3D] Server should make agents avoid dynamic obstacles when avoidance enabled") {
		NavigationServer3D *navigation_server = NavigationServer3D::get_singleton();

		RID map = navigation_server->map_create();
		RID agent_1 = navigation_server->agent_create();
		RID obstacle_1 = navigation_server->obstacle_create();

		navigation_server->map_set_active(map, true);

		navigation_server->agent_set_map(agent_1, map);
		navigation_server->agent_set_avoidance_enabled(agent_1, true);
		navigation_server->agent_set_position(agent_1, Vector3(0, 0, 0));
		navigation_server->agent_set_radius(agent_1, 1);
		navigation_server->agent_set_velocity(agent_1, Vector3(1, 0, 0));
		CallableMock agent_1_avoidance_callback_mock;
		navigation_server->agent_set_avoidance_callback(agent_1, callable_mp(&agent_1_avoidance_callback_mock, &CallableMock::function1));

		navigation_server->obstacle_set_map(obstacle_1, map);
		navigation_server->obstacle_set_avoidance_enabled(obstacle_1, true);
		navigation_server->obstacle_set_position(obstacle_1, Vector3(2.5, 0, 0.5));
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

	TEST_CASE("[NavigationServer3D] Server should make agents avoid static obstacles when avoidance enabled") {
		NavigationServer3D *navigation_server = NavigationServer3D::get_singleton();

		RID map = navigation_server->map_create();
		RID agent_1 = navigation_server->agent_create();
		RID agent_2 = navigation_server->agent_create();
		RID obstacle_1 = navigation_server->obstacle_create();

		navigation_server->map_set_active(map, true);

		navigation_server->agent_set_map(agent_1, map);
		navigation_server->agent_set_avoidance_enabled(agent_1, true);
		navigation_server->agent_set_radius(agent_1, 1.6); // Have hit the obstacle already.
		navigation_server->agent_set_velocity(agent_1, Vector3(1, 0, 0));
		CallableMock agent_1_avoidance_callback_mock;
		navigation_server->agent_set_avoidance_callback(agent_1, callable_mp(&agent_1_avoidance_callback_mock, &CallableMock::function1));

		navigation_server->agent_set_map(agent_2, map);
		navigation_server->agent_set_avoidance_enabled(agent_2, true);
		navigation_server->agent_set_radius(agent_2, 1.4); // Haven't hit the obstacle yet.
		navigation_server->agent_set_velocity(agent_2, Vector3(1, 0, 0));
		CallableMock agent_2_avoidance_callback_mock;
		navigation_server->agent_set_avoidance_callback(agent_2, callable_mp(&agent_2_avoidance_callback_mock, &CallableMock::function1));

		navigation_server->obstacle_set_map(obstacle_1, map);
		navigation_server->obstacle_set_avoidance_enabled(obstacle_1, true);
		PackedVector3Array obstacle_1_vertices;

		SUBCASE("Static obstacles should work on ground level") {
			navigation_server->agent_set_position(agent_1, Vector3(0, 0, 0));
			navigation_server->agent_set_position(agent_2, Vector3(0, 0, 5));
			obstacle_1_vertices.push_back(Vector3(1.5, 0, 0.5));
			obstacle_1_vertices.push_back(Vector3(1.5, 0, 4.5));
		}

		SUBCASE("Static obstacles should work when elevated") {
			navigation_server->agent_set_position(agent_1, Vector3(0, 5, 0));
			navigation_server->agent_set_position(agent_2, Vector3(0, 5, 5));
			obstacle_1_vertices.push_back(Vector3(1.5, 0, 0.5));
			obstacle_1_vertices.push_back(Vector3(1.5, 0, 4.5));
			navigation_server->obstacle_set_position(obstacle_1, Vector3(0, 5, 0));
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

#ifndef DISABLE_DEPRECATED
	// This test case uses only public APIs on purpose - other test cases use simplified baking.
	// FIXME: Remove once deprecated `region_bake_navigation_mesh()` is removed.
	TEST_CASE("[NavigationServer3D][SceneTree][DEPRECATED] Server should be able to bake map correctly") {
		NavigationServer3D *navigation_server = NavigationServer3D::get_singleton();

		// Prepare scene tree with simple mesh to serve as an input geometry.
		Node3D *node_3d = memnew(Node3D);
		SceneTree::get_singleton()->get_root()->add_child(node_3d);
		Ref<PlaneMesh> plane_mesh = memnew(PlaneMesh);
		plane_mesh->set_size(Size2(10.0, 10.0));
		MeshInstance3D *mesh_instance = memnew(MeshInstance3D);
		mesh_instance->set_mesh(plane_mesh);
		node_3d->add_child(mesh_instance);

		// Prepare anything necessary to bake navigation mesh.
		RID map = navigation_server->map_create();
		RID region = navigation_server->region_create();
		Ref<NavigationMesh> navigation_mesh = memnew(NavigationMesh);
		navigation_server->map_set_use_async_iterations(map, false);
		navigation_server->map_set_active(map, true);
		navigation_server->region_set_map(region, map);
		navigation_server->region_set_navigation_mesh(region, navigation_mesh);
		navigation_server->process(0.0); // Give server some cycles to commit.

		CHECK_EQ(navigation_mesh->get_polygon_count(), 0);
		CHECK_EQ(navigation_mesh->get_vertices().size(), 0);

		ERR_PRINT_OFF;
		navigation_server->region_bake_navigation_mesh(navigation_mesh, node_3d);
		ERR_PRINT_ON;
		// FIXME: The above line should trigger the update (line below) under the hood.
		navigation_server->region_set_navigation_mesh(region, navigation_mesh); // Force update.
		CHECK_EQ(navigation_mesh->get_polygon_count(), 2);
		CHECK_EQ(navigation_mesh->get_vertices().size(), 4);

		SUBCASE("Map should emit signal and take newly baked navigation mesh into account") {
			SIGNAL_WATCH(navigation_server, "map_changed");
			SIGNAL_CHECK_FALSE("map_changed");
			navigation_server->process(0.0); // Give server some cycles to commit.
			SIGNAL_CHECK("map_changed", build_array(build_array(map)));
			SIGNAL_UNWATCH(navigation_server, "map_changed");
			CHECK_NE(navigation_server->map_get_closest_point(map, Vector3(0, 0, 0)), Vector3(0, 0, 0));
		}

		navigation_server->free(region);
		navigation_server->free(map);
		navigation_server->process(0.0); // Give server some cycles to commit.
		memdelete(mesh_instance);
		memdelete(node_3d);
	}
#endif // DISABLE_DEPRECATED

	TEST_CASE("[NavigationServer3D][SceneTree] Server should be able to parse geometry") {
		NavigationServer3D *navigation_server = NavigationServer3D::get_singleton();

		// Prepare scene tree with simple mesh to serve as an input geometry.
		Node3D *node_3d = memnew(Node3D);
		SceneTree::get_singleton()->get_root()->add_child(node_3d);
		Ref<PlaneMesh> plane_mesh = memnew(PlaneMesh);
		plane_mesh->set_size(Size2(10.0, 10.0));
		MeshInstance3D *mesh_instance = memnew(MeshInstance3D);
		mesh_instance->set_mesh(plane_mesh);
		node_3d->add_child(mesh_instance);

		Ref<NavigationMesh> navigation_mesh = memnew(NavigationMesh);
		Ref<NavigationMeshSourceGeometryData3D> source_geometry = memnew(NavigationMeshSourceGeometryData3D);
		CHECK_EQ(source_geometry->get_vertices().size(), 0);
		CHECK_EQ(source_geometry->get_indices().size(), 0);

		navigation_server->parse_source_geometry_data(navigation_mesh, source_geometry, mesh_instance);
		CHECK_EQ(source_geometry->get_vertices().size(), 12);
		CHECK_EQ(source_geometry->get_indices().size(), 6);

		SUBCASE("By default, parsing should remove any data that was parsed before") {
			navigation_server->parse_source_geometry_data(navigation_mesh, source_geometry, mesh_instance);
			CHECK_EQ(source_geometry->get_vertices().size(), 12);
			CHECK_EQ(source_geometry->get_indices().size(), 6);
		}

		SUBCASE("Parsed geometry should be extendable with other geometry") {
			source_geometry->merge(source_geometry); // Merging with itself.
			const Vector<float> vertices = source_geometry->get_vertices();
			const Vector<int> indices = source_geometry->get_indices();
			REQUIRE_EQ(vertices.size(), 24);
			REQUIRE_EQ(indices.size(), 12);
			// Check if first newly added vertex is the same as first vertex.
			CHECK_EQ(vertices[0], vertices[12]);
			CHECK_EQ(vertices[1], vertices[13]);
			CHECK_EQ(vertices[2], vertices[14]);
			// Check if first newly added index is the same as first index.
			CHECK_EQ(indices[0] + 4, indices[6]);
		}

		memdelete(mesh_instance);
		memdelete(node_3d);
	}

	// This test case uses only public APIs on purpose - other test cases use simplified baking.
	TEST_CASE("[NavigationServer3D][SceneTree] Server should be able to bake map correctly") {
		NavigationServer3D *navigation_server = NavigationServer3D::get_singleton();

		// Prepare scene tree with simple mesh to serve as an input geometry.
		Node3D *node_3d = memnew(Node3D);
		SceneTree::get_singleton()->get_root()->add_child(node_3d);
		Ref<PlaneMesh> plane_mesh = memnew(PlaneMesh);
		plane_mesh->set_size(Size2(10.0, 10.0));
		MeshInstance3D *mesh_instance = memnew(MeshInstance3D);
		mesh_instance->set_mesh(plane_mesh);
		node_3d->add_child(mesh_instance);

		// Prepare anything necessary to bake navigation mesh.
		RID map = navigation_server->map_create();
		RID region = navigation_server->region_create();
		Ref<NavigationMesh> navigation_mesh = memnew(NavigationMesh);
		navigation_server->map_set_use_async_iterations(map, false);
		navigation_server->map_set_active(map, true);
		navigation_server->region_set_map(region, map);
		navigation_server->region_set_navigation_mesh(region, navigation_mesh);
		navigation_server->process(0.0); // Give server some cycles to commit.

		CHECK_EQ(navigation_mesh->get_polygon_count(), 0);
		CHECK_EQ(navigation_mesh->get_vertices().size(), 0);

		Ref<NavigationMeshSourceGeometryData3D> source_geometry = memnew(NavigationMeshSourceGeometryData3D);
		navigation_server->parse_source_geometry_data(navigation_mesh, source_geometry, node_3d);
		navigation_server->bake_from_source_geometry_data(navigation_mesh, source_geometry, Callable());
		// FIXME: The above line should trigger the update (line below) under the hood.
		navigation_server->region_set_navigation_mesh(region, navigation_mesh); // Force update.
		CHECK_EQ(navigation_mesh->get_polygon_count(), 2);
		CHECK_EQ(navigation_mesh->get_vertices().size(), 4);

		SUBCASE("Map should emit signal and take newly baked navigation mesh into account") {
			SIGNAL_WATCH(navigation_server, "map_changed");
			SIGNAL_CHECK_FALSE("map_changed");
			navigation_server->process(0.0); // Give server some cycles to commit.
			SIGNAL_CHECK("map_changed", build_array(build_array(map)));
			SIGNAL_UNWATCH(navigation_server, "map_changed");
			CHECK_NE(navigation_server->map_get_closest_point(map, Vector3(0, 0, 0)), Vector3(0, 0, 0));
		}

		navigation_server->free(region);
		navigation_server->free(map);
		navigation_server->process(0.0); // Give server some cycles to commit.
		memdelete(mesh_instance);
		memdelete(node_3d);
	}

	// This test case does not check precise values on purpose - to not be too sensitivte.
	TEST_CASE("[NavigationServer3D] Server should respond to queries against valid map properly") {
		NavigationServer3D *navigation_server = NavigationServer3D::get_singleton();
		Ref<NavigationMesh> navigation_mesh = memnew(NavigationMesh);
		Ref<NavigationMeshSourceGeometryData3D> source_geometry = memnew(NavigationMeshSourceGeometryData3D);

		Array arr;
		arr.resize(RS::ARRAY_MAX);
		BoxMesh::create_mesh_array(arr, Vector3(10.0, 0.001, 10.0));
		source_geometry->add_mesh_array(arr, Transform3D());
		navigation_server->bake_from_source_geometry_data(navigation_mesh, source_geometry, Callable());
		CHECK_NE(navigation_mesh->get_polygon_count(), 0);
		CHECK_NE(navigation_mesh->get_vertices().size(), 0);

		RID map = navigation_server->map_create();
		RID region = navigation_server->region_create();
		navigation_server->map_set_active(map, true);
		navigation_server->map_set_use_async_iterations(map, false);
		navigation_server->region_set_map(region, map);
		navigation_server->region_set_navigation_mesh(region, navigation_mesh);
		navigation_server->process(0.0); // Give server some cycles to commit.

		SUBCASE("Simple queries should return non-default values") {
			CHECK_NE(navigation_server->map_get_closest_point(map, Vector3(0, 0, 0)), Vector3(0, 0, 0));
			CHECK_NE(navigation_server->map_get_closest_point_normal(map, Vector3(0, 0, 0)), Vector3());
			CHECK(navigation_server->map_get_closest_point_owner(map, Vector3(0, 0, 0)).is_valid());
			CHECK_NE(navigation_server->map_get_closest_point_to_segment(map, Vector3(0, 0, 0), Vector3(1, 1, 1), false), Vector3());
			CHECK_NE(navigation_server->map_get_closest_point_to_segment(map, Vector3(0, 0, 0), Vector3(1, 1, 1), true), Vector3());
			CHECK_NE(navigation_server->map_get_path(map, Vector3(0, 0, 0), Vector3(10, 0, 10), true).size(), 0);
			CHECK_NE(navigation_server->map_get_path(map, Vector3(0, 0, 0), Vector3(10, 0, 10), false).size(), 0);
		}

		SUBCASE("'map_get_closest_point_to_segment' with 'use_collision' should return default if segment doesn't intersect map") {
			CHECK_EQ(navigation_server->map_get_closest_point_to_segment(map, Vector3(1, 2, 1), Vector3(1, 1, 1), true), Vector3());
		}

		SUBCASE("Elaborate query with 'CORRIDORFUNNEL' post-processing should yield non-empty result") {
			Ref<NavigationPathQueryParameters3D> query_parameters = memnew(NavigationPathQueryParameters3D);
			query_parameters->set_map(map);
			query_parameters->set_start_position(Vector3(0, 0, 0));
			query_parameters->set_target_position(Vector3(10, 0, 10));
			query_parameters->set_path_postprocessing(NavigationPathQueryParameters3D::PATH_POSTPROCESSING_CORRIDORFUNNEL);
			Ref<NavigationPathQueryResult3D> query_result = memnew(NavigationPathQueryResult3D);
			navigation_server->query_path(query_parameters, query_result);
			CHECK_NE(query_result->get_path().size(), 0);
			CHECK_NE(query_result->get_path_types().size(), 0);
			CHECK_NE(query_result->get_path_rids().size(), 0);
			CHECK_NE(query_result->get_path_owner_ids().size(), 0);
		}

		SUBCASE("Elaborate query with 'EDGECENTERED' post-processing should yield non-empty result") {
			Ref<NavigationPathQueryParameters3D> query_parameters = memnew(NavigationPathQueryParameters3D);
			query_parameters->set_map(map);
			query_parameters->set_start_position(Vector3(10, 0, 10));
			query_parameters->set_target_position(Vector3(0, 0, 0));
			query_parameters->set_path_postprocessing(NavigationPathQueryParameters3D::PATH_POSTPROCESSING_EDGECENTERED);
			Ref<NavigationPathQueryResult3D> query_result = memnew(NavigationPathQueryResult3D);
			navigation_server->query_path(query_parameters, query_result);
			CHECK_NE(query_result->get_path().size(), 0);
			CHECK_NE(query_result->get_path_types().size(), 0);
			CHECK_NE(query_result->get_path_rids().size(), 0);
			CHECK_NE(query_result->get_path_owner_ids().size(), 0);
		}

		SUBCASE("Elaborate query with non-matching navigation layer mask should yield empty result") {
			Ref<NavigationPathQueryParameters3D> query_parameters = memnew(NavigationPathQueryParameters3D);
			query_parameters->set_map(map);
			query_parameters->set_start_position(Vector3(10, 0, 10));
			query_parameters->set_target_position(Vector3(0, 0, 0));
			query_parameters->set_navigation_layers(2);
			Ref<NavigationPathQueryResult3D> query_result = memnew(NavigationPathQueryResult3D);
			navigation_server->query_path(query_parameters, query_result);
			CHECK_EQ(query_result->get_path().size(), 0);
			CHECK_EQ(query_result->get_path_types().size(), 0);
			CHECK_EQ(query_result->get_path_rids().size(), 0);
			CHECK_EQ(query_result->get_path_owner_ids().size(), 0);
		}

		SUBCASE("Elaborate query without metadata flags should yield path only") {
			Ref<NavigationPathQueryParameters3D> query_parameters = memnew(NavigationPathQueryParameters3D);
			query_parameters->set_map(map);
			query_parameters->set_start_position(Vector3(10, 0, 10));
			query_parameters->set_target_position(Vector3(0, 0, 0));
			query_parameters->set_metadata_flags(0);
			Ref<NavigationPathQueryResult3D> query_result = memnew(NavigationPathQueryResult3D);
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

	// FIXME: The race condition mentioned below is actually a problem and fails on CI (GH-90613).
	/*
	TEST_CASE("[NavigationServer3D] Server should be able to bake asynchronously") {
		NavigationServer3D *navigation_server = NavigationServer3D::get_singleton();
		Ref<NavigationMesh> navigation_mesh = memnew(NavigationMesh);
		Ref<NavigationMeshSourceGeometryData3D> source_geometry = memnew(NavigationMeshSourceGeometryData3D);

		Array arr;
		arr.resize(RS::ARRAY_MAX);
		BoxMesh::create_mesh_array(arr, Vector3(10.0, 0.001, 10.0));
		source_geometry->add_mesh_array(arr, Transform3D());

		// Race condition is present below, but baking should take many orders of magnitude
		// longer than basic checks on the main thread, so it's fine.
		navigation_server->bake_from_source_geometry_data_async(navigation_mesh, source_geometry, Callable());
		CHECK(navigation_server->is_baking_navigation_mesh(navigation_mesh));
		CHECK_EQ(navigation_mesh->get_polygon_count(), 0);
		CHECK_EQ(navigation_mesh->get_vertices().size(), 0);
	}
	*/

	TEST_CASE("[NavigationServer3D] Server should simplify path properly") {
		real_t simplify_epsilon = 0.2;
		Vector<Vector3> source_path;
		source_path.resize(7);
		source_path.write[0] = Vector3(0.0, 0.0, 0.0);
		source_path.write[1] = Vector3(0.0, 0.0, 1.0); // This point needs to go.
		source_path.write[2] = Vector3(0.0, 0.0, 2.0); // This point needs to go.
		source_path.write[3] = Vector3(0.0, 0.0, 2.0);
		source_path.write[4] = Vector3(2.0, 1.0, 3.0);
		source_path.write[5] = Vector3(2.0, 1.5, 4.0); // This point needs to go.
		source_path.write[6] = Vector3(2.0, 2.0, 5.0);
		Vector<Vector3> simplified_path = NavigationServer3D::get_singleton()->simplify_path(source_path, simplify_epsilon);
		CHECK_EQ(simplified_path.size(), 4);
	}

	TEST_CASE("[Heap] size") {
		gd::Heap<int> heap;

		CHECK(heap.size() == 0);

		heap.push(0);
		CHECK(heap.size() == 1);

		heap.push(1);
		CHECK(heap.size() == 2);

		heap.pop();
		CHECK(heap.size() == 1);

		heap.pop();
		CHECK(heap.size() == 0);
	}

	TEST_CASE("[Heap] is_empty") {
		gd::Heap<int> heap;

		CHECK(heap.is_empty() == true);

		heap.push(0);
		CHECK(heap.is_empty() == false);

		heap.pop();
		CHECK(heap.is_empty() == true);
	}

	TEST_CASE("[Heap] push/pop") {
		SUBCASE("Default comparator") {
			gd::Heap<int> heap;

			heap.push(2);
			heap.push(7);
			heap.push(5);
			heap.push(3);
			heap.push(4);

			CHECK(heap.pop() == 7);
			CHECK(heap.pop() == 5);
			CHECK(heap.pop() == 4);
			CHECK(heap.pop() == 3);
			CHECK(heap.pop() == 2);
		}

		SUBCASE("Custom comparator") {
			GreaterThan greaterThan;
			gd::Heap<int, GreaterThan> heap(greaterThan);

			heap.push(2);
			heap.push(7);
			heap.push(5);
			heap.push(3);
			heap.push(4);

			CHECK(heap.pop() == 2);
			CHECK(heap.pop() == 3);
			CHECK(heap.pop() == 4);
			CHECK(heap.pop() == 5);
			CHECK(heap.pop() == 7);
		}

		SUBCASE("Intermediate pops") {
			gd::Heap<int> heap;

			heap.push(0);
			heap.push(3);
			heap.pop();
			heap.push(1);
			heap.push(2);

			CHECK(heap.pop() == 2);
			CHECK(heap.pop() == 1);
			CHECK(heap.pop() == 0);
		}
	}

	TEST_CASE("[Heap] shift") {
		int values[] = { 5, 3, 6, 7, 1 };
		uint32_t heap_indexes[] = { 0, 0, 0, 0, 0 };
		CompareArrayValues comparator(values);
		RegisterHeapIndexes indexer(heap_indexes);
		gd::Heap<uint32_t, CompareArrayValues, RegisterHeapIndexes> heap(comparator, indexer);

		heap.push(0);
		heap.push(1);
		heap.push(2);
		heap.push(3);
		heap.push(4);

		// Shift down: 6 -> 2
		values[2] = 2;
		heap.shift(heap_indexes[2]);

		// Shift up: 5 -> 8
		values[0] = 8;
		heap.shift(heap_indexes[0]);

		CHECK(heap.pop() == 0);
		CHECK(heap.pop() == 3);
		CHECK(heap.pop() == 1);
		CHECK(heap.pop() == 2);
		CHECK(heap.pop() == 4);

		CHECK(heap_indexes[0] == UINT32_MAX);
		CHECK(heap_indexes[1] == UINT32_MAX);
		CHECK(heap_indexes[2] == UINT32_MAX);
		CHECK(heap_indexes[3] == UINT32_MAX);
		CHECK(heap_indexes[4] == UINT32_MAX);
	}

	TEST_CASE("[Heap] clear") {
		uint32_t heap_indexes[] = { 0, 0, 0, 0 };
		RegisterHeapIndexes indexer(heap_indexes);
		gd::Heap<uint32_t, Comparator<uint32_t>, RegisterHeapIndexes> heap(indexer);

		heap.push(0);
		heap.push(2);
		heap.push(1);
		heap.push(3);

		heap.clear();

		CHECK(heap.size() == 0);

		CHECK(heap_indexes[0] == UINT32_MAX);
		CHECK(heap_indexes[1] == UINT32_MAX);
		CHECK(heap_indexes[2] == UINT32_MAX);
		CHECK(heap_indexes[3] == UINT32_MAX);
	}
}
} //namespace TestNavigationServer3D

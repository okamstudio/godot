/**************************************************************************/
/*  nav_mesh_queries_2d.h                                                 */
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

#ifndef NAV_MESH_QUERIES_2D_H
#define NAV_MESH_QUERIES_2D_H

#include "../nav_utils_2d.h"

#include "servers/navigation/navigation_path_query_parameters_2d.h"
#include "servers/navigation/navigation_path_query_result_2d.h"
#include "servers/navigation/navigation_utilities.h"

using namespace NavigationUtilities;

class NavMap2D;
struct NavMapIteration2D;

class NavMeshQueries2D {
public:
	struct PathQuerySlot {
		LocalVector<nav_2d::NavigationPoly> path_corridor;
		Heap<nav_2d::NavigationPoly *, nav_2d::NavPolyTravelCostGreaterThan, nav_2d::NavPolyHeapIndexer> traversable_polys;
		bool in_use = false;
		uint32_t slot_index = 0;
	};

	struct NavMeshPathQueryTask2D {
		enum TaskStatus {
			QUERY_STARTED,
			QUERY_FINISHED,
			QUERY_FAILED,
			CALLBACK_DISPATCHED,
			CALLBACK_FAILED,
		};

		// Parameters.
		Vector2 start_position;
		Vector2 target_position;
		uint32_t navigation_layers;
		BitField<PathMetadataFlags> metadata_flags = PathMetadataFlags::PATH_INCLUDE_ALL;
		PathfindingAlgorithm pathfinding_algorithm = PathfindingAlgorithm::PATHFINDING_ALGORITHM_ASTAR;
		PathPostProcessing path_postprocessing = PathPostProcessing::PATH_POSTPROCESSING_CORRIDORFUNNEL;
		bool simplify_path = false;
		real_t simplify_epsilon = 0.0;

		// Path building.
		Vector2 begin_position;
		Vector2 end_position;
		const nav_2d::Polygon *begin_polygon = nullptr;
		const nav_2d::Polygon *end_polygon = nullptr;
		uint32_t least_cost_id = 0;

		// Map.
		NavMap2D *map = nullptr;
		PathQuerySlot *path_query_slot = nullptr;

		// Path points.
		LocalVector<Vector2> path_points;
		LocalVector<int32_t> path_meta_point_types;
		LocalVector<RID> path_meta_point_rids;
		LocalVector<int64_t> path_meta_point_owners;

		Ref<NavigationPathQueryParameters2D> query_parameters;
		Ref<NavigationPathQueryResult2D> query_result;
		Callable callback;
		NavMeshPathQueryTask2D::TaskStatus status = NavMeshPathQueryTask2D::TaskStatus::QUERY_STARTED;

		void path_clear() {
			path_points.clear();
			path_meta_point_types.clear();
			path_meta_point_rids.clear();
			path_meta_point_owners.clear();
		}

		void path_reverse() {
			path_points.invert();
			path_meta_point_types.invert();
			path_meta_point_rids.invert();
			path_meta_point_owners.invert();
		}
	};

	static bool emit_callback(const Callable &p_callback);

	static Vector2 polygons_get_random_point(const LocalVector<nav_2d::Polygon> &p_polygons, uint32_t p_navigation_layers, bool p_uniformly);

	static Vector2 polygons_get_closest_point(const LocalVector<nav_2d::Polygon> &p_polygons, const Vector2 &p_point);
	static nav_2d::ClosestPointQueryResult polygons_get_closest_point_info(const LocalVector<nav_2d::Polygon> &p_polygons, const Vector2 &p_point);
	static RID polygons_get_closest_point_owner(const LocalVector<nav_2d::Polygon> &p_polygons, const Vector2 &p_point);

	static Vector2 map_iteration_get_closest_point(const NavMapIteration2D &p_map_iteration, const Vector2 &p_point);
	static RID map_iteration_get_closest_point_owner(const NavMapIteration2D &p_map_iteration, const Vector2 &p_point);
	static nav_2d::ClosestPointQueryResult map_iteration_get_closest_point_info(const NavMapIteration2D &p_map_iteration, const Vector2 &p_point);
	static Vector2 map_iteration_get_random_point(const NavMapIteration2D &p_map_iteration, uint32_t p_navigation_layers, bool p_uniformly);

	static void map_query_path(NavMap2D *p_map, const Ref<NavigationPathQueryParameters2D> &p_query_parameters, Ref<NavigationPathQueryResult2D> p_query_result, const Callable &p_callback);

	static void query_task_map_iteration_get_path(NavMeshPathQueryTask2D &p_query_task, const NavMapIteration2D &p_map_iteration);
	static void _query_task_push_back_point_with_metadata(NavMeshPathQueryTask2D &p_query_task, const Vector2 &p_point, const nav_2d::Polygon *p_point_polygon);
	static void _query_task_find_start_end_positions(NavMeshPathQueryTask2D &p_query_task, const NavMapIteration2D &p_map_iteration);
	static void _query_task_build_path_corridor(NavMeshPathQueryTask2D &p_query_task);
	static void _query_task_post_process_corridorfunnel(NavMeshPathQueryTask2D &p_query_task);
	static void _query_task_post_process_edgecentered(NavMeshPathQueryTask2D &p_query_task);
	static void _query_task_post_process_nopostprocessing(NavMeshPathQueryTask2D &p_query_task);
	static void _query_task_clip_path(NavMeshPathQueryTask2D &p_query_task, const nav_2d::NavigationPoly *p_from_poly, const Vector2 &p_to_point, const nav_2d::NavigationPoly *p_to_poly);
	static void _query_task_simplified_path_points(NavMeshPathQueryTask2D &p_query_task);

	static void simplify_path_segment(int p_start_inx, int p_end_inx, const LocalVector<Vector2> &p_points, real_t p_epsilon, LocalVector<uint32_t> &r_simplified_path_indices);
	static LocalVector<uint32_t> get_simplified_path_indices(const LocalVector<Vector2> &p_path, real_t p_epsilon);
};

#endif // NAV_MESH_QUERIES_2D_H

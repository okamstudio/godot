/**************************************************************************/
/*  nav_mesh_queries_3d.h                                                 */
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

#ifndef NAV_MESH_QUERIES_3D_H
#define NAV_MESH_QUERIES_3D_H

#ifndef _3D_DISABLED

#include "../nav_utils.h"

#include "servers/navigation/navigation_path_query_parameters_3d.h"
#include "servers/navigation/navigation_path_query_result_3d.h"
#include "servers/navigation/navigation_utilities.h"

using namespace NavigationUtilities;

class NavMap;

class NavMeshQueries3D {
public:
	struct PathQuerySlot {
		LocalVector<gd::NavigationPoly> path_corridor;
		gd::Heap<gd::NavigationPoly *, gd::NavPolyTravelCostGreaterThan, gd::NavPolyHeapIndexer> traversable_polys;
		bool in_use = false;
		uint32_t slot_index = 0;
	};

	struct NavMeshPathQueryTask3D {
		enum TaskStatus {
			QUERY_STARTED,
			QUERY_FINISHED,
			QUERY_FAILED,
			CALLBACK_DISPATCHED,
			CALLBACK_FAILED,
		};

		// Parameters.
		Vector3 start_position;
		Vector3 target_position;
		uint32_t navigation_layers;
		BitField<PathMetadataFlags> metadata_flags = PathMetadataFlags::PATH_INCLUDE_ALL;
		PathfindingAlgorithm pathfinding_algorithm = PathfindingAlgorithm::PATHFINDING_ALGORITHM_ASTAR;
		PathPostProcessing path_postprocessing = PathPostProcessing::PATH_POSTPROCESSING_CORRIDORFUNNEL;
		bool simplify_path = false;
		real_t simplify_epsilon = 0.0;

		// Path building.
		Vector3 begin_position;
		Vector3 end_position;
		uint32_t least_cost_id = 0;
		Vector3 map_up;
		NavMap *map = nullptr;
		PathQuerySlot *path_query_slot = nullptr;

		// Path points.
		LocalVector<Vector3> path_points;
		LocalVector<int32_t> path_meta_point_types;
		LocalVector<RID> path_meta_point_rids;
		LocalVector<int64_t> path_meta_point_owners;

		Ref<NavigationPathQueryParameters3D> query_parameters;
		Ref<NavigationPathQueryResult3D> query_result;
		Callable callback;
		NavMeshPathQueryTask3D::TaskStatus status = NavMeshPathQueryTask3D::TaskStatus::QUERY_STARTED;
	};

	static bool emit_callback(const Callable &p_callback);

	static Vector3 polygons_get_random_point(const LocalVector<gd::Polygon> &p_polygons, uint32_t p_navigation_layers, bool p_uniformly);

	static Vector3 polygons_get_closest_point_to_segment(const LocalVector<gd::Polygon> &p_polygons, const Vector3 &p_from, const Vector3 &p_to, const bool p_use_collision);
	static Vector3 polygons_get_closest_point(const LocalVector<gd::Polygon> &p_polygons, const Vector3 &p_point);
	static Vector3 polygons_get_closest_point_normal(const LocalVector<gd::Polygon> &p_polygons, const Vector3 &p_point);
	static gd::ClosestPointQueryResult polygons_get_closest_point_info(const LocalVector<gd::Polygon> &p_polygons, const Vector3 &p_point);
	static RID polygons_get_closest_point_owner(const LocalVector<gd::Polygon> &p_polygons, const Vector3 &p_point);

	static void map_query_path(NavMap *map, const Ref<NavigationPathQueryParameters3D> &p_query_parameters, Ref<NavigationPathQueryResult3D> p_query_result, const Callable &p_callback);

	static void query_task_polygons_get_path(NavMeshPathQueryTask3D &p_query_task, const LocalVector<gd::Polygon> &p_polygons, const Vector3 &p_map_up, uint32_t p_link_polygons_size);

	static void _query_task_create_same_polygon_two_point_path(NavMeshPathQueryTask3D &p_query_task, const gd::Polygon *begin_poly, Vector3 begin_point, const gd::Polygon *end_poly, Vector3 end_point);
	static void _query_task_push_back_point_with_metadata(NavMeshPathQueryTask3D &p_query_task, Vector3 p_point, const gd::Polygon *p_point_polygon);
	static void _query_task_find_start_end_positions(NavMeshPathQueryTask3D &p_query_task, const LocalVector<gd::Polygon> &p_polygons, const gd::Polygon **r_begin_poly, Vector3 &r_begin_point, const gd::Polygon **r_end_poly, Vector3 &r_end_point);
	static void _query_task_build_path_corridor(NavMeshPathQueryTask3D &p_query_task, const LocalVector<gd::Polygon> &p_polygons, const Vector3 &p_map_up, uint32_t p_link_polygons_size, const gd::Polygon *begin_poly, Vector3 begin_point, const gd::Polygon *end_polygon, Vector3 end_point);
	static void _path_corridor_post_process_corridorfunnel(NavMeshPathQueryTask3D &p_query_task, int p_least_cost_id, const gd::Polygon *p_begin_poly, Vector3 p_begin_point, const gd::Polygon *p_end_polygon, Vector3 p_end_point, const Vector3 &p_map_up);
	static void _path_corridor_post_process_edgecentered(NavMeshPathQueryTask3D &p_query_task, int p_least_cost_id, const gd::Polygon *p_begin_poly, Vector3 p_begin_point, const gd::Polygon *p_end_polygon, Vector3 p_end_point);
	static void _path_corridor_post_process_nopostprocessing(NavMeshPathQueryTask3D &p_query_task, int p_least_cost_id, const gd::Polygon *p_begin_poly, Vector3 p_begin_point, const gd::Polygon *p_end_polygon, Vector3 p_end_point);

	static void clip_path(NavMeshPathQueryTask3D &p_query_task, const LocalVector<gd::NavigationPoly> &p_navigation_polys, const gd::NavigationPoly *from_poly, const Vector3 &p_to_point, const gd::NavigationPoly *p_to_poly, const Vector3 &p_map_up);
	static void _query_task_simplified_path_points(NavMeshPathQueryTask3D &p_query_task);

	static void simplify_path_segment(int p_start_inx, int p_end_inx, const LocalVector<Vector3> &p_points, real_t p_epsilon, LocalVector<bool> &r_valid_points);
	static LocalVector<uint32_t> get_simplified_path_indices(const LocalVector<Vector3> &p_path, real_t p_epsilon);
};

#endif // _3D_DISABLED

#endif // NAV_MESH_QUERIES_3D_H

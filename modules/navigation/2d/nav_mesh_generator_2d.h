/**************************************************************************/
/*  nav_mesh_generator_2d.h                                               */
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

#ifndef NAV_MESH_GENERATOR_2D_H
#define NAV_MESH_GENERATOR_2D_H

#ifdef CLIPPER2_ENABLED

#include "core/object/class_db.h"
#include "core/object/worker_thread_pool.h"
#include "core/templates/rid_owner.h"
#include "servers/navigation_server_2d.h"

class Node;
class NavigationPolygon;
class NavigationMeshSourceGeometryData2D;

class NavMeshGenerator2D : public Object {
	static NavMeshGenerator2D *singleton;

	static Mutex baking_navmesh_mutex;
	static Mutex generator_task_mutex;

	static RWLock generator_parsers_rwlock;
	static LocalVector<NavMeshGeometryParser2D *> generator_parsers;

	static bool use_threads;
	static bool baking_use_multiple_threads;
	static bool baking_use_high_priority_threads;

	struct NavMeshGeneratorTask2D {
		enum TaskStatus {
			BAKING_STARTED,
			BAKING_FINISHED,
			BAKING_FAILED,
			CALLBACK_DISPATCHED,
			CALLBACK_FAILED,
		};

		Ref<NavigationPolygon> navigation_mesh;
		Ref<NavigationMeshSourceGeometryData2D> source_geometry_data;
		Callable callback;
		WorkerThreadPool::TaskID thread_task_id = WorkerThreadPool::INVALID_TASK_ID;
		NavMeshGeneratorTask2D::TaskStatus status = NavMeshGeneratorTask2D::TaskStatus::BAKING_STARTED;
	};

	static HashMap<WorkerThreadPool::TaskID, NavMeshGeneratorTask2D *> generator_tasks;

	static void generator_thread_bake(void *p_arg);

	static HashSet<Ref<NavigationPolygon>> baking_navmeshes;

	static void generator_parse_geometry_node(Ref<NavigationPolygon> p_navigation_mesh, Ref<NavigationMeshSourceGeometryData2D> p_source_geometry_data, Node *p_node, bool p_recurse_children);
	static void generator_parse_source_geometry_data(Ref<NavigationPolygon> p_navigation_mesh, Ref<NavigationMeshSourceGeometryData2D> p_source_geometry_data, Node *p_root_node);
	static void generator_bake_from_source_geometry_data(Ref<NavigationPolygon> p_navigation_mesh, Ref<NavigationMeshSourceGeometryData2D> p_source_geometry_data);

	static bool generator_emit_callback(const Callable &p_callback);

public:
	static NavMeshGenerator2D *get_singleton();

	static void sync();
	static void cleanup();
	static void finish();

	static void set_generator_parsers(LocalVector<NavMeshGeometryParser2D *> p_parsers);

	static void parse_source_geometry_data(Ref<NavigationPolygon> p_navigation_mesh, Ref<NavigationMeshSourceGeometryData2D> p_source_geometry_data, Node *p_root_node, const Callable &p_callback = Callable());
	static void bake_from_source_geometry_data(Ref<NavigationPolygon> p_navigation_mesh, Ref<NavigationMeshSourceGeometryData2D> p_source_geometry_data, const Callable &p_callback = Callable());
	static void bake_from_source_geometry_data_async(Ref<NavigationPolygon> p_navigation_mesh, Ref<NavigationMeshSourceGeometryData2D> p_source_geometry_data, const Callable &p_callback = Callable());
	static bool is_baking(Ref<NavigationPolygon> p_navigation_polygon);

	NavMeshGenerator2D();
	~NavMeshGenerator2D();
};

#endif // CLIPPER2_ENABLED

#endif // NAV_MESH_GENERATOR_2D_H

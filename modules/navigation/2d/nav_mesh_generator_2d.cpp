/**************************************************************************/
/*  nav_mesh_generator_2d.cpp                                             */
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

#ifdef CLIPPER2_ENABLED

#include "nav_mesh_generator_2d.h"

#include "core/config/project_settings.h"
#include "scene/resources/2d/navigation_mesh_source_geometry_data_2d.h"
#include "scene/resources/2d/navigation_polygon.h"

#include "thirdparty/clipper2/include/clipper2/clipper.h"
#include "thirdparty/misc/polypartition.h"

NavMeshGenerator2D *NavMeshGenerator2D::singleton = nullptr;
Mutex NavMeshGenerator2D::baking_navmesh_mutex;
Mutex NavMeshGenerator2D::generator_task_mutex;
RWLock NavMeshGenerator2D::generator_parsers_rwlock;
bool NavMeshGenerator2D::use_threads = true;
bool NavMeshGenerator2D::baking_use_multiple_threads = true;
bool NavMeshGenerator2D::baking_use_high_priority_threads = true;
HashSet<Ref<NavigationPolygon>> NavMeshGenerator2D::baking_navmeshes;
HashMap<WorkerThreadPool::TaskID, NavMeshGenerator2D::NavMeshGeneratorTask2D *> NavMeshGenerator2D::generator_tasks;
LocalVector<NavMeshGeometryParser2D *> NavMeshGenerator2D::generator_parsers;

NavMeshGenerator2D *NavMeshGenerator2D::get_singleton() {
	return singleton;
}

NavMeshGenerator2D::NavMeshGenerator2D() {
	ERR_FAIL_COND(singleton != nullptr);
	singleton = this;

	baking_use_multiple_threads = GLOBAL_GET("navigation/baking/thread_model/baking_use_multiple_threads");
	baking_use_high_priority_threads = GLOBAL_GET("navigation/baking/thread_model/baking_use_high_priority_threads");

	// Using threads might cause problems on certain exports or with the Editor on certain devices.
	// This is the main switch to turn threaded navmesh baking off should the need arise.
	use_threads = baking_use_multiple_threads;
}

NavMeshGenerator2D::~NavMeshGenerator2D() {
	cleanup();
}

void NavMeshGenerator2D::sync() {
	if (generator_tasks.size() == 0) {
		return;
	}

	MutexLock baking_navmesh_lock(baking_navmesh_mutex);
	{
		MutexLock generator_task_lock(generator_task_mutex);

		LocalVector<WorkerThreadPool::TaskID> finished_task_ids;

		for (KeyValue<WorkerThreadPool::TaskID, NavMeshGeneratorTask2D *> &E : generator_tasks) {
			if (WorkerThreadPool::get_singleton()->is_task_completed(E.key)) {
				WorkerThreadPool::get_singleton()->wait_for_task_completion(E.key);
				finished_task_ids.push_back(E.key);

				NavMeshGeneratorTask2D *generator_task = E.value;
				DEV_ASSERT(generator_task->status == NavMeshGeneratorTask2D::TaskStatus::BAKING_FINISHED);

				baking_navmeshes.erase(generator_task->navigation_mesh);
				if (generator_task->callback.is_valid()) {
					generator_emit_callback(generator_task->callback);
				}
				memdelete(generator_task);
			}
		}

		for (WorkerThreadPool::TaskID finished_task_id : finished_task_ids) {
			generator_tasks.erase(finished_task_id);
		}
	}
}

void NavMeshGenerator2D::cleanup() {
	MutexLock baking_navmesh_lock(baking_navmesh_mutex);
	{
		MutexLock generator_task_lock(generator_task_mutex);

		baking_navmeshes.clear();

		for (KeyValue<WorkerThreadPool::TaskID, NavMeshGeneratorTask2D *> &E : generator_tasks) {
			WorkerThreadPool::get_singleton()->wait_for_task_completion(E.key);
			NavMeshGeneratorTask2D *generator_task = E.value;
			memdelete(generator_task);
		}
		generator_tasks.clear();

		generator_parsers_rwlock.write_lock();
		generator_parsers.clear();
		generator_parsers_rwlock.write_unlock();
	}
}

void NavMeshGenerator2D::finish() {
	cleanup();
}

void NavMeshGenerator2D::parse_source_geometry_data(Ref<NavigationPolygon> p_navigation_mesh, Ref<NavigationMeshSourceGeometryData2D> p_source_geometry_data, Node *p_root_node, const Callable &p_callback) {
	ERR_FAIL_COND(!Thread::is_main_thread());
	ERR_FAIL_COND(p_navigation_mesh.is_null());
	ERR_FAIL_NULL(p_root_node);
	ERR_FAIL_COND(!p_root_node->is_inside_tree());
	ERR_FAIL_COND(p_source_geometry_data.is_null());

	generator_parse_source_geometry_data(p_navigation_mesh, p_source_geometry_data, p_root_node);

	if (p_callback.is_valid()) {
		generator_emit_callback(p_callback);
	}
}

void NavMeshGenerator2D::bake_from_source_geometry_data(Ref<NavigationPolygon> p_navigation_mesh, Ref<NavigationMeshSourceGeometryData2D> p_source_geometry_data, const Callable &p_callback) {
	ERR_FAIL_COND(p_navigation_mesh.is_null());
	ERR_FAIL_COND(p_source_geometry_data.is_null());

	if (p_navigation_mesh->get_outline_count() == 0 && !p_source_geometry_data->has_data()) {
		p_navigation_mesh->clear();
		if (p_callback.is_valid()) {
			generator_emit_callback(p_callback);
		}
		return;
	}

	if (is_baking(p_navigation_mesh)) {
		ERR_FAIL_MSG("NavigationPolygon is already baking. Wait for current bake to finish.");
	}
	baking_navmesh_mutex.lock();
	baking_navmeshes.insert(p_navigation_mesh);
	baking_navmesh_mutex.unlock();

	generator_bake_from_source_geometry_data(p_navigation_mesh, p_source_geometry_data);

	baking_navmesh_mutex.lock();
	baking_navmeshes.erase(p_navigation_mesh);
	baking_navmesh_mutex.unlock();

	if (p_callback.is_valid()) {
		generator_emit_callback(p_callback);
	}
}

void NavMeshGenerator2D::bake_from_source_geometry_data_async(Ref<NavigationPolygon> p_navigation_mesh, Ref<NavigationMeshSourceGeometryData2D> p_source_geometry_data, const Callable &p_callback) {
	ERR_FAIL_COND(p_navigation_mesh.is_null());
	ERR_FAIL_COND(p_source_geometry_data.is_null());

	if (p_navigation_mesh->get_outline_count() == 0 && !p_source_geometry_data->has_data()) {
		p_navigation_mesh->clear();
		if (p_callback.is_valid()) {
			generator_emit_callback(p_callback);
		}
		return;
	}

	if (!use_threads) {
		bake_from_source_geometry_data(p_navigation_mesh, p_source_geometry_data, p_callback);
		return;
	}

	if (is_baking(p_navigation_mesh)) {
		ERR_FAIL_MSG("NavigationPolygon is already baking. Wait for current bake to finish.");
	}
	baking_navmesh_mutex.lock();
	baking_navmeshes.insert(p_navigation_mesh);
	baking_navmesh_mutex.unlock();

	MutexLock generator_task_lock(generator_task_mutex);
	NavMeshGeneratorTask2D *generator_task = memnew(NavMeshGeneratorTask2D);
	generator_task->navigation_mesh = p_navigation_mesh;
	generator_task->source_geometry_data = p_source_geometry_data;
	generator_task->callback = p_callback;
	generator_task->status = NavMeshGeneratorTask2D::TaskStatus::BAKING_STARTED;
	generator_task->thread_task_id = WorkerThreadPool::get_singleton()->add_native_task(&NavMeshGenerator2D::generator_thread_bake, generator_task, NavMeshGenerator2D::baking_use_high_priority_threads, "NavMeshGeneratorBake2D");
	generator_tasks.insert(generator_task->thread_task_id, generator_task);
}

bool NavMeshGenerator2D::is_baking(Ref<NavigationPolygon> p_navigation_polygon) {
	MutexLock baking_navmesh_lock(baking_navmesh_mutex);
	return baking_navmeshes.has(p_navigation_polygon);
}

void NavMeshGenerator2D::generator_thread_bake(void *p_arg) {
	NavMeshGeneratorTask2D *generator_task = static_cast<NavMeshGeneratorTask2D *>(p_arg);

	generator_bake_from_source_geometry_data(generator_task->navigation_mesh, generator_task->source_geometry_data);

	generator_task->status = NavMeshGeneratorTask2D::TaskStatus::BAKING_FINISHED;
}

void NavMeshGenerator2D::generator_parse_geometry_node(Ref<NavigationPolygon> p_navigation_mesh, Ref<NavigationMeshSourceGeometryData2D> p_source_geometry_data, Node *p_node, bool p_recurse_children) {
	generator_parsers_rwlock.read_lock();
	for (const NavMeshGeometryParser2D *parser : generator_parsers) {
		if (!parser->callback.is_valid()) {
			continue;
		}
		parser->callback.call(p_navigation_mesh, p_source_geometry_data, p_node);
	}
	generator_parsers_rwlock.read_unlock();

	if (p_recurse_children) {
		for (int i = 0; i < p_node->get_child_count(); i++) {
			generator_parse_geometry_node(p_navigation_mesh, p_source_geometry_data, p_node->get_child(i), p_recurse_children);
		}
	}
}

void NavMeshGenerator2D::set_generator_parsers(LocalVector<NavMeshGeometryParser2D *> p_parsers) {
	RWLockWrite write_lock(generator_parsers_rwlock);
	generator_parsers = p_parsers;
}

void NavMeshGenerator2D::generator_parse_source_geometry_data(Ref<NavigationPolygon> p_navigation_mesh, Ref<NavigationMeshSourceGeometryData2D> p_source_geometry_data, Node *p_root_node) {
	List<Node *> parse_nodes;

	if (p_navigation_mesh->get_source_geometry_mode() == NavigationPolygon::SOURCE_GEOMETRY_ROOT_NODE_CHILDREN) {
		parse_nodes.push_back(p_root_node);
	} else {
		p_root_node->get_tree()->get_nodes_in_group(p_navigation_mesh->get_source_geometry_group_name(), &parse_nodes);
	}

	Transform2D root_node_transform = Transform2D();
	if (Object::cast_to<Node2D>(p_root_node)) {
		root_node_transform = Object::cast_to<Node2D>(p_root_node)->get_global_transform().affine_inverse();
	}

	p_source_geometry_data->clear();
	p_source_geometry_data->root_node_transform = root_node_transform;

	bool recurse_children = p_navigation_mesh->get_source_geometry_mode() != NavigationPolygon::SOURCE_GEOMETRY_GROUPS_EXPLICIT;

	for (Node *E : parse_nodes) {
		generator_parse_geometry_node(p_navigation_mesh, p_source_geometry_data, E, recurse_children);
	}
}

static void generator_recursive_process_polytree_items(List<TPPLPoly> &p_tppl_in_polygon, const Clipper2Lib::PolyPathD *p_polypath_item) {
	using namespace Clipper2Lib;

	TPPLPoly tp;
	int size = p_polypath_item->Polygon().size();
	tp.Init(size);

	int j = 0;
	for (const PointD &polypath_point : p_polypath_item->Polygon()) {
		tp[j] = Vector2(static_cast<real_t>(polypath_point.x), static_cast<real_t>(polypath_point.y));
		++j;
	}

	if (p_polypath_item->IsHole()) {
		tp.SetOrientation(TPPL_ORIENTATION_CW);
		tp.SetHole(true);
	} else {
		tp.SetOrientation(TPPL_ORIENTATION_CCW);
	}
	p_tppl_in_polygon.push_back(tp);

	for (size_t i = 0; i < p_polypath_item->Count(); i++) {
		const PolyPathD *polypath_item = p_polypath_item->Child(i);
		generator_recursive_process_polytree_items(p_tppl_in_polygon, polypath_item);
	}
}

bool NavMeshGenerator2D::generator_emit_callback(const Callable &p_callback) {
	ERR_FAIL_COND_V(!p_callback.is_valid(), false);

	Callable::CallError ce;
	Variant result;
	p_callback.callp(nullptr, 0, result, ce);

	return ce.error == Callable::CallError::CALL_OK;
}

void NavMeshGenerator2D::generator_bake_from_source_geometry_data(Ref<NavigationPolygon> p_navigation_mesh, Ref<NavigationMeshSourceGeometryData2D> p_source_geometry_data) {
	if (p_navigation_mesh.is_null() || p_source_geometry_data.is_null()) {
		return;
	}

	using namespace Clipper2Lib;
	PathsD traversable_polygon_paths;
	PathsD obstruction_polygon_paths;
	bool empty_projected_obstructions = true;
	{
		RWLockRead read_lock(p_source_geometry_data->geometry_rwlock);

		const Vector<Vector<Vector2>> &traversable_outlines = p_source_geometry_data->traversable_outlines;
		int outline_count = p_navigation_mesh->get_outline_count();

		if (outline_count == 0 && (!p_source_geometry_data->has_data() || (traversable_outlines.is_empty()))) {
			return;
		}

		const Vector<Vector<Vector2>> &obstruction_outlines = p_source_geometry_data->obstruction_outlines;
		const Vector<NavigationMeshSourceGeometryData2D::ProjectedObstruction> &projected_obstructions = p_source_geometry_data->_projected_obstructions;

		traversable_polygon_paths.reserve(outline_count + traversable_outlines.size());
		obstruction_polygon_paths.reserve(obstruction_outlines.size());

		for (int i = 0; i < outline_count; i++) {
			const Vector<Vector2> &traversable_outline = p_navigation_mesh->get_outline(i);
			PathD subject_path;
			subject_path.reserve(traversable_outline.size());
			for (const Vector2 &traversable_point : traversable_outline) {
				subject_path.emplace_back(traversable_point.x, traversable_point.y);
			}
			traversable_polygon_paths.push_back(std::move(subject_path));
		}

		for (const Vector<Vector2> &traversable_outline : traversable_outlines) {
			PathD subject_path;
			subject_path.reserve(traversable_outline.size());
			for (const Vector2 &traversable_point : traversable_outline) {
				subject_path.emplace_back(traversable_point.x, traversable_point.y);
			}
			traversable_polygon_paths.push_back(std::move(subject_path));
		}

		empty_projected_obstructions = projected_obstructions.is_empty();
		if (!empty_projected_obstructions) {
			for (const NavigationMeshSourceGeometryData2D::ProjectedObstruction &projected_obstruction : projected_obstructions) {
				if (projected_obstruction.carve) {
					continue;
				}
				if (projected_obstruction.vertices.is_empty() || projected_obstruction.vertices.size() % 2 != 0) {
					continue;
				}

				PathD clip_path;
				clip_path.reserve(projected_obstruction.vertices.size() / 2);
				for (int i = 0; i < projected_obstruction.vertices.size() / 2; i++) {
					clip_path.emplace_back(projected_obstruction.vertices[i * 2], projected_obstruction.vertices[i * 2 + 1]);
				}
				if (!IsPositive(clip_path)) {
					std::reverse(clip_path.begin(), clip_path.end());
				}
				obstruction_polygon_paths.push_back(std::move(clip_path));
			}
		}

		for (const Vector<Vector2> &obstruction_outline : obstruction_outlines) {
			PathD clip_path;
			clip_path.reserve(obstruction_outline.size());
			for (const Vector2 &obstruction_point : obstruction_outline) {
				clip_path.emplace_back(obstruction_point.x, obstruction_point.y);
			}
			obstruction_polygon_paths.push_back(std::move(clip_path));
		}
	}

	Rect2 baking_rect = p_navigation_mesh->get_baking_rect();
	if (baking_rect.has_area()) {
		Vector2 baking_rect_offset = p_navigation_mesh->get_baking_rect_offset();

		const int rect_begin_x = baking_rect.position[0] + baking_rect_offset.x;
		const int rect_begin_y = baking_rect.position[1] + baking_rect_offset.y;
		const int rect_end_x = baking_rect.position[0] + baking_rect.size[0] + baking_rect_offset.x;
		const int rect_end_y = baking_rect.position[1] + baking_rect.size[1] + baking_rect_offset.y;

		RectD clipper_rect = RectD(rect_begin_x, rect_begin_y, rect_end_x, rect_end_y);

		traversable_polygon_paths = RectClip(clipper_rect, traversable_polygon_paths);
		obstruction_polygon_paths = RectClip(clipper_rect, obstruction_polygon_paths);
	}

	// first merge all traversable polygons according to user specified fill rule
	PathsD dummy_clip_path;
	traversable_polygon_paths = Union(traversable_polygon_paths, dummy_clip_path, FillRule::NonZero);
	// merge all obstruction polygons, don't allow holes for what is considered "solid" 2D geometry
	obstruction_polygon_paths = Union(obstruction_polygon_paths, dummy_clip_path, FillRule::NonZero);

	PathsD path_solution = Difference(traversable_polygon_paths, obstruction_polygon_paths, FillRule::NonZero);

	real_t agent_radius_offset = p_navigation_mesh->get_agent_radius();
	if (agent_radius_offset > 0.0) {
		path_solution = InflatePaths(path_solution, -agent_radius_offset, JoinType::Miter, EndType::Polygon);
	}

	// Apply obstructions that are not affected by agent radius, the ones with carve enabled.
	if (!empty_projected_obstructions) {
		RWLockRead read_lock(p_source_geometry_data->geometry_rwlock);
		const Vector<NavigationMeshSourceGeometryData2D::ProjectedObstruction> &projected_obstructions = p_source_geometry_data->_projected_obstructions;
		obstruction_polygon_paths.resize(0);
		for (const NavigationMeshSourceGeometryData2D::ProjectedObstruction &projected_obstruction : projected_obstructions) {
			if (!projected_obstruction.carve) {
				continue;
			}
			if (projected_obstruction.vertices.is_empty() || projected_obstruction.vertices.size() % 2 != 0) {
				continue;
			}

			PathD clip_path;
			clip_path.reserve(projected_obstruction.vertices.size() / 2);
			for (int i = 0; i < projected_obstruction.vertices.size() / 2; i++) {
				clip_path.emplace_back(projected_obstruction.vertices[i * 2], projected_obstruction.vertices[i * 2 + 1]);
			}
			if (!IsPositive(clip_path)) {
				std::reverse(clip_path.begin(), clip_path.end());
			}
			obstruction_polygon_paths.push_back(std::move(clip_path));
		}
		if (obstruction_polygon_paths.size() > 0) {
			path_solution = Difference(path_solution, obstruction_polygon_paths, FillRule::NonZero);
		}
	}

	//path_solution = RamerDouglasPeucker(path_solution, 0.025); //

	real_t border_size = p_navigation_mesh->get_border_size();
	if (baking_rect.has_area() && border_size > 0.0) {
		Vector2 baking_rect_offset = p_navigation_mesh->get_baking_rect_offset();

		const int rect_begin_x = baking_rect.position[0] + baking_rect_offset.x + border_size;
		const int rect_begin_y = baking_rect.position[1] + baking_rect_offset.y + border_size;
		const int rect_end_x = baking_rect.position[0] + baking_rect.size[0] + baking_rect_offset.x - border_size;
		const int rect_end_y = baking_rect.position[1] + baking_rect.size[1] + baking_rect_offset.y - border_size;

		RectD clipper_rect = RectD(rect_begin_x, rect_begin_y, rect_end_x, rect_end_y);

		path_solution = RectClip(clipper_rect, path_solution);
	}

	if (path_solution.size() == 0) {
		p_navigation_mesh->clear();
		return;
	}

	ClipType clipper_cliptype = ClipType::Union;

	List<TPPLPoly> tppl_in_polygon, tppl_out_polygon;

	PolyTreeD polytree;
	ClipperD clipper_D;

	clipper_D.AddSubject(path_solution);
	clipper_D.Execute(clipper_cliptype, FillRule::NonZero, polytree);

	for (size_t i = 0; i < polytree.Count(); i++) {
		const PolyPathD *polypath_item = polytree[i];
		generator_recursive_process_polytree_items(tppl_in_polygon, polypath_item);
	}

	TPPLPartition tpart;

	NavigationPolygon::SamplePartitionType sample_partition_type = p_navigation_mesh->get_sample_partition_type();

	switch (sample_partition_type) {
		case NavigationPolygon::SamplePartitionType::SAMPLE_PARTITION_CONVEX_PARTITION:
			if (tpart.ConvexPartition_HM(&tppl_in_polygon, &tppl_out_polygon) == 0) {
				ERR_PRINT("NavigationPolygon polygon convex partition failed. Unable to create a valid navigation mesh polygon layout from provided source geometry.");
				p_navigation_mesh->set_vertices(Vector<Vector2>());
				p_navigation_mesh->clear_polygons();
				return;
			}
			break;
		case NavigationPolygon::SamplePartitionType::SAMPLE_PARTITION_TRIANGULATE:
			if (tpart.Triangulate_EC(&tppl_in_polygon, &tppl_out_polygon) == 0) {
				ERR_PRINT("NavigationPolygon polygon triangulation failed. Unable to create a valid navigation mesh polygon layout from provided source geometry.");
				p_navigation_mesh->set_vertices(Vector<Vector2>());
				p_navigation_mesh->clear_polygons();
				return;
			}
			break;
		default: {
			ERR_PRINT("NavigationPolygon polygon partitioning failed. Unrecognized partition type.");
			p_navigation_mesh->set_vertices(Vector<Vector2>());
			p_navigation_mesh->clear_polygons();
			return;
		}
	}

	Vector<Vector2> new_vertices;
	Vector<Vector<int>> new_polygons;

	HashMap<Vector2, int> points;
	for (List<TPPLPoly>::Element *I = tppl_out_polygon.front(); I; I = I->next()) {
		TPPLPoly &tp = I->get();

		Vector<int> new_polygon;

		for (int64_t i = 0; i < tp.GetNumPoints(); i++) {
			HashMap<Vector2, int>::Iterator E = points.find(tp[i]);
			if (!E) {
				E = points.insert(tp[i], new_vertices.size());
				new_vertices.push_back(tp[i]);
			}
			new_polygon.push_back(E->value);
		}

		new_polygons.push_back(new_polygon);
	}

	p_navigation_mesh->set_data(new_vertices, new_polygons);
}

#endif // CLIPPER2_ENABLED

/**************************************************************************/
/*  nav_mesh_generator_3d.cpp                                             */
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

#ifndef _3D_DISABLED

#include "nav_mesh_generator_3d.h"

#include "core/config/project_settings.h"
#include "core/math/convex_hull.h"
#include "core/os/thread.h"
#include "scene/3d/mesh_instance_3d.h"
#include "scene/3d/multimesh_instance_3d.h"
#include "scene/3d/navigation_obstacle_3d.h"
#include "scene/3d/physics/static_body_3d.h"
#include "scene/resources/3d/box_shape_3d.h"
#include "scene/resources/3d/capsule_shape_3d.h"
#include "scene/resources/3d/concave_polygon_shape_3d.h"
#include "scene/resources/3d/convex_polygon_shape_3d.h"
#include "scene/resources/3d/cylinder_shape_3d.h"
#include "scene/resources/3d/height_map_shape_3d.h"
#include "scene/resources/3d/navigation_mesh_source_geometry_data_3d.h"
#include "scene/resources/3d/primitive_meshes.h"
#include "scene/resources/3d/shape_3d.h"
#include "scene/resources/3d/sphere_shape_3d.h"
#include "scene/resources/navigation_mesh.h"

#include "modules/modules_enabled.gen.h" // For csg, gridmap.

#ifdef MODULE_CSG_ENABLED
#include "modules/csg/csg_shape.h"
#endif
#ifdef MODULE_GRIDMAP_ENABLED
#include "modules/gridmap/grid_map.h"
#endif

#include <Recast.h>

NavMeshGenerator3D *NavMeshGenerator3D::singleton = nullptr;
Mutex NavMeshGenerator3D::baking_navmesh_mutex;
Mutex NavMeshGenerator3D::generator_task_mutex;
RWLock NavMeshGenerator3D::generator_rid_rwlock;
bool NavMeshGenerator3D::use_threads = true;
bool NavMeshGenerator3D::baking_use_multiple_threads = true;
bool NavMeshGenerator3D::baking_use_high_priority_threads = true;
HashSet<Ref<NavigationMesh>> NavMeshGenerator3D::baking_navmeshes;
HashMap<WorkerThreadPool::TaskID, NavMeshGenerator3D::NavMeshGeneratorTask3D *> NavMeshGenerator3D::generator_tasks;
RID_Owner<NavMeshGenerator3D::NavMeshGeometryParser3D> NavMeshGenerator3D::generator_parser_owner;
LocalVector<NavMeshGenerator3D::NavMeshGeometryParser3D *> NavMeshGenerator3D::generator_parsers;

NavMeshGenerator3D *NavMeshGenerator3D::get_singleton() {
	return singleton;
}

NavMeshGenerator3D::NavMeshGenerator3D() {
	ERR_FAIL_COND(singleton != nullptr);
	singleton = this;

	baking_use_multiple_threads = GLOBAL_GET("navigation/baking/thread_model/baking_use_multiple_threads");
	baking_use_high_priority_threads = GLOBAL_GET("navigation/baking/thread_model/baking_use_high_priority_threads");

	// Using threads might cause problems on certain exports or with the Editor on certain devices.
	// This is the main switch to turn threaded navmesh baking off should the need arise.
	use_threads = baking_use_multiple_threads;
}

NavMeshGenerator3D::~NavMeshGenerator3D() {
	cleanup();
}

void NavMeshGenerator3D::sync() {
	if (generator_tasks.size() == 0) {
		return;
	}

	MutexLock baking_navmesh_lock(baking_navmesh_mutex);
	{
		MutexLock generator_task_lock(generator_task_mutex);

		LocalVector<WorkerThreadPool::TaskID> finished_task_ids;

		for (KeyValue<WorkerThreadPool::TaskID, NavMeshGeneratorTask3D *> &E : generator_tasks) {
			if (WorkerThreadPool::get_singleton()->is_task_completed(E.key)) {
				WorkerThreadPool::get_singleton()->wait_for_task_completion(E.key);
				finished_task_ids.push_back(E.key);

				NavMeshGeneratorTask3D *generator_task = E.value;
				DEV_ASSERT(generator_task->status == NavMeshGeneratorTask3D::TaskStatus::BAKING_FINISHED);

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

void NavMeshGenerator3D::cleanup() {
	MutexLock baking_navmesh_lock(baking_navmesh_mutex);
	{
		MutexLock generator_task_lock(generator_task_mutex);

		baking_navmeshes.clear();

		for (KeyValue<WorkerThreadPool::TaskID, NavMeshGeneratorTask3D *> &E : generator_tasks) {
			WorkerThreadPool::get_singleton()->wait_for_task_completion(E.key);
			NavMeshGeneratorTask3D *generator_task = E.value;
			memdelete(generator_task);
		}
		generator_tasks.clear();

		generator_rid_rwlock.write_lock();
		for (NavMeshGeometryParser3D *parser : generator_parsers) {
			generator_parser_owner.free(parser->self);
		}
		generator_parsers.clear();
		generator_rid_rwlock.write_unlock();
	}
}

void NavMeshGenerator3D::finish() {
	cleanup();
}

void NavMeshGenerator3D::parse_source_geometry_data(Ref<NavigationMesh> p_navigation_mesh, Ref<NavigationMeshSourceGeometryData3D> p_source_geometry_data, Node *p_root_node, const Callable &p_callback) {
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

void NavMeshGenerator3D::bake_from_source_geometry_data(Ref<NavigationMesh> p_navigation_mesh, Ref<NavigationMeshSourceGeometryData3D> p_source_geometry_data, const Callable &p_callback) {
	ERR_FAIL_COND(p_navigation_mesh.is_null());
	ERR_FAIL_COND(p_source_geometry_data.is_null());

	if (!p_source_geometry_data->has_data()) {
		p_navigation_mesh->clear();
		if (p_callback.is_valid()) {
			generator_emit_callback(p_callback);
		}
		return;
	}

	if (is_baking(p_navigation_mesh)) {
		ERR_FAIL_MSG("NavigationMesh is already baking. Wait for current bake to finish.");
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

void NavMeshGenerator3D::bake_from_source_geometry_data_async(Ref<NavigationMesh> p_navigation_mesh, Ref<NavigationMeshSourceGeometryData3D> p_source_geometry_data, const Callable &p_callback) {
	ERR_FAIL_COND(p_navigation_mesh.is_null());
	ERR_FAIL_COND(p_source_geometry_data.is_null());

	if (!p_source_geometry_data->has_data()) {
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
		ERR_FAIL_MSG("NavigationMesh is already baking. Wait for current bake to finish.");
		return;
	}
	baking_navmesh_mutex.lock();
	baking_navmeshes.insert(p_navigation_mesh);
	baking_navmesh_mutex.unlock();

	MutexLock generator_task_lock(generator_task_mutex);
	NavMeshGeneratorTask3D *generator_task = memnew(NavMeshGeneratorTask3D);
	generator_task->navigation_mesh = p_navigation_mesh;
	generator_task->source_geometry_data = p_source_geometry_data;
	generator_task->callback = p_callback;
	generator_task->status = NavMeshGeneratorTask3D::TaskStatus::BAKING_STARTED;
	generator_task->thread_task_id = WorkerThreadPool::get_singleton()->add_native_task(&NavMeshGenerator3D::generator_thread_bake, generator_task, NavMeshGenerator3D::baking_use_high_priority_threads, SNAME("NavMeshGeneratorBake3D"));
	generator_tasks.insert(generator_task->thread_task_id, generator_task);
}

bool NavMeshGenerator3D::is_baking(Ref<NavigationMesh> p_navigation_mesh) {
	MutexLock baking_navmesh_lock(baking_navmesh_mutex);
	return baking_navmeshes.has(p_navigation_mesh);
}

void NavMeshGenerator3D::generator_thread_bake(void *p_arg) {
	NavMeshGeneratorTask3D *generator_task = static_cast<NavMeshGeneratorTask3D *>(p_arg);

	generator_bake_from_source_geometry_data(generator_task->navigation_mesh, generator_task->source_geometry_data);

	generator_task->status = NavMeshGeneratorTask3D::TaskStatus::BAKING_FINISHED;
}

void NavMeshGenerator3D::generator_parse_geometry_node(const Ref<NavigationMesh> &p_navigation_mesh, Ref<NavigationMeshSourceGeometryData3D> p_source_geometry_data, Node *p_node, bool p_recurse_children) {
	generator_parse_meshinstance3d_node(p_navigation_mesh, p_source_geometry_data, p_node);
	generator_parse_multimeshinstance3d_node(p_navigation_mesh, p_source_geometry_data, p_node);
	generator_parse_staticbody3d_node(p_navigation_mesh, p_source_geometry_data, p_node);
#ifdef MODULE_CSG_ENABLED
	generator_parse_csgshape3d_node(p_navigation_mesh, p_source_geometry_data, p_node);
#endif
#ifdef MODULE_GRIDMAP_ENABLED
	generator_parse_gridmap_node(p_navigation_mesh, p_source_geometry_data, p_node);
#endif
	generator_parse_navigationobstacle_node(p_navigation_mesh, p_source_geometry_data, p_node);

	generator_rid_rwlock.read_lock();
	for (const NavMeshGeometryParser3D *parser : generator_parsers) {
		if (!parser->callback.is_valid()) {
			continue;
		}
		parser->callback.call(p_navigation_mesh, p_source_geometry_data, p_node);
	}
	generator_rid_rwlock.read_unlock();

	if (p_recurse_children) {
		for (int i = 0; i < p_node->get_child_count(); i++) {
			generator_parse_geometry_node(p_navigation_mesh, p_source_geometry_data, p_node->get_child(i), p_recurse_children);
		}
	}
}

void NavMeshGenerator3D::generator_parse_meshinstance3d_node(const Ref<NavigationMesh> &p_navigation_mesh, Ref<NavigationMeshSourceGeometryData3D> p_source_geometry_data, Node *p_node) {
	MeshInstance3D *mesh_instance = Object::cast_to<MeshInstance3D>(p_node);

	if (mesh_instance) {
		NavigationMesh::ParsedGeometryType parsed_geometry_type = p_navigation_mesh->get_parsed_geometry_type();

		if (parsed_geometry_type == NavigationMesh::PARSED_GEOMETRY_MESH_INSTANCES || parsed_geometry_type == NavigationMesh::PARSED_GEOMETRY_BOTH) {
			Ref<Mesh> mesh = mesh_instance->get_mesh();
			if (mesh.is_valid()) {
				p_source_geometry_data->add_mesh(mesh, mesh_instance->get_global_transform());
			}
		}
	}
}

void NavMeshGenerator3D::generator_parse_multimeshinstance3d_node(const Ref<NavigationMesh> &p_navigation_mesh, Ref<NavigationMeshSourceGeometryData3D> p_source_geometry_data, Node *p_node) {
	MultiMeshInstance3D *multimesh_instance = Object::cast_to<MultiMeshInstance3D>(p_node);

	if (multimesh_instance) {
		NavigationMesh::ParsedGeometryType parsed_geometry_type = p_navigation_mesh->get_parsed_geometry_type();

		if (parsed_geometry_type == NavigationMesh::PARSED_GEOMETRY_MESH_INSTANCES || parsed_geometry_type == NavigationMesh::PARSED_GEOMETRY_BOTH) {
			Ref<MultiMesh> multimesh = multimesh_instance->get_multimesh();
			if (multimesh.is_valid()) {
				Ref<Mesh> mesh = multimesh->get_mesh();
				if (mesh.is_valid()) {
					int n = multimesh->get_visible_instance_count();
					if (n == -1) {
						n = multimesh->get_instance_count();
					}
					for (int i = 0; i < n; i++) {
						p_source_geometry_data->add_mesh(mesh, multimesh_instance->get_global_transform() * multimesh->get_instance_transform(i));
					}
				}
			}
		}
	}
}

void NavMeshGenerator3D::generator_parse_staticbody3d_node(const Ref<NavigationMesh> &p_navigation_mesh, Ref<NavigationMeshSourceGeometryData3D> p_source_geometry_data, Node *p_node) {
	StaticBody3D *static_body = Object::cast_to<StaticBody3D>(p_node);

	if (static_body) {
		NavigationMesh::ParsedGeometryType parsed_geometry_type = p_navigation_mesh->get_parsed_geometry_type();
		uint32_t parsed_collision_mask = p_navigation_mesh->get_collision_mask();

		if ((parsed_geometry_type == NavigationMesh::PARSED_GEOMETRY_STATIC_COLLIDERS || parsed_geometry_type == NavigationMesh::PARSED_GEOMETRY_BOTH) && (static_body->get_collision_layer() & parsed_collision_mask)) {
			List<uint32_t> shape_owners;
			static_body->get_shape_owners(&shape_owners);
			for (uint32_t shape_owner : shape_owners) {
				if (static_body->is_shape_owner_disabled(shape_owner)) {
					continue;
				}
				const int shape_count = static_body->shape_owner_get_shape_count(shape_owner);
				for (int shape_index = 0; shape_index < shape_count; shape_index++) {
					Ref<Shape3D> s = static_body->shape_owner_get_shape(shape_owner, shape_index);
					if (s.is_null()) {
						continue;
					}

					const Transform3D transform = static_body->get_global_transform() * static_body->shape_owner_get_transform(shape_owner);

					BoxShape3D *box = Object::cast_to<BoxShape3D>(*s);
					if (box) {
						Array arr;
						arr.resize(RS::ARRAY_MAX);
						BoxMesh::create_mesh_array(arr, box->get_size());
						p_source_geometry_data->add_mesh_array(arr, transform);
					}

					CapsuleShape3D *capsule = Object::cast_to<CapsuleShape3D>(*s);
					if (capsule) {
						Array arr;
						arr.resize(RS::ARRAY_MAX);
						CapsuleMesh::create_mesh_array(arr, capsule->get_radius(), capsule->get_height());
						p_source_geometry_data->add_mesh_array(arr, transform);
					}

					CylinderShape3D *cylinder = Object::cast_to<CylinderShape3D>(*s);
					if (cylinder) {
						Array arr;
						arr.resize(RS::ARRAY_MAX);
						CylinderMesh::create_mesh_array(arr, cylinder->get_radius(), cylinder->get_radius(), cylinder->get_height());
						p_source_geometry_data->add_mesh_array(arr, transform);
					}

					SphereShape3D *sphere = Object::cast_to<SphereShape3D>(*s);
					if (sphere) {
						Array arr;
						arr.resize(RS::ARRAY_MAX);
						SphereMesh::create_mesh_array(arr, sphere->get_radius(), sphere->get_radius() * 2.0);
						p_source_geometry_data->add_mesh_array(arr, transform);
					}

					ConcavePolygonShape3D *concave_polygon = Object::cast_to<ConcavePolygonShape3D>(*s);
					if (concave_polygon) {
						p_source_geometry_data->add_faces(concave_polygon->get_faces(), transform);
					}

					ConvexPolygonShape3D *convex_polygon = Object::cast_to<ConvexPolygonShape3D>(*s);
					if (convex_polygon) {
						Vector<Vector3> varr = Variant(convex_polygon->get_points());
						Geometry3D::MeshData md;

						Error err = ConvexHullComputer::convex_hull(varr, md);

						if (err == OK) {
							PackedVector3Array faces;

							for (const Geometry3D::MeshData::Face &face : md.faces) {
								for (uint32_t k = 2; k < face.indices.size(); ++k) {
									faces.push_back(md.vertices[face.indices[0]]);
									faces.push_back(md.vertices[face.indices[k - 1]]);
									faces.push_back(md.vertices[face.indices[k]]);
								}
							}

							p_source_geometry_data->add_faces(faces, transform);
						}
					}

					HeightMapShape3D *heightmap_shape = Object::cast_to<HeightMapShape3D>(*s);
					if (heightmap_shape) {
						int heightmap_depth = heightmap_shape->get_map_depth();
						int heightmap_width = heightmap_shape->get_map_width();

						if (heightmap_depth >= 2 && heightmap_width >= 2) {
							const Vector<real_t> &map_data = heightmap_shape->get_map_data();

							Vector2 heightmap_gridsize(heightmap_width - 1, heightmap_depth - 1);
							Vector3 start = Vector3(heightmap_gridsize.x, 0, heightmap_gridsize.y) * -0.5;

							Vector<Vector3> vertex_array;
							vertex_array.resize((heightmap_depth - 1) * (heightmap_width - 1) * 6);
							Vector3 *vertex_array_ptrw = vertex_array.ptrw();
							const real_t *map_data_ptr = map_data.ptr();
							int vertex_index = 0;

							for (int d = 0; d < heightmap_depth - 1; d++) {
								for (int w = 0; w < heightmap_width - 1; w++) {
									vertex_array_ptrw[vertex_index] = start + Vector3(w, map_data_ptr[(heightmap_width * d) + w], d);
									vertex_array_ptrw[vertex_index + 1] = start + Vector3(w + 1, map_data_ptr[(heightmap_width * d) + w + 1], d);
									vertex_array_ptrw[vertex_index + 2] = start + Vector3(w, map_data_ptr[(heightmap_width * d) + heightmap_width + w], d + 1);
									vertex_array_ptrw[vertex_index + 3] = start + Vector3(w + 1, map_data_ptr[(heightmap_width * d) + w + 1], d);
									vertex_array_ptrw[vertex_index + 4] = start + Vector3(w + 1, map_data_ptr[(heightmap_width * d) + heightmap_width + w + 1], d + 1);
									vertex_array_ptrw[vertex_index + 5] = start + Vector3(w, map_data_ptr[(heightmap_width * d) + heightmap_width + w], d + 1);
									vertex_index += 6;
								}
							}
							if (vertex_array.size() > 0) {
								p_source_geometry_data->add_faces(vertex_array, transform);
							}
						}
					}
				}
			}
		}
	}
}

#ifdef MODULE_CSG_ENABLED
void NavMeshGenerator3D::generator_parse_csgshape3d_node(const Ref<NavigationMesh> &p_navigation_mesh, Ref<NavigationMeshSourceGeometryData3D> p_source_geometry_data, Node *p_node) {
	CSGShape3D *csgshape3d = Object::cast_to<CSGShape3D>(p_node);

	if (csgshape3d) {
		NavigationMesh::ParsedGeometryType parsed_geometry_type = p_navigation_mesh->get_parsed_geometry_type();
		uint32_t parsed_collision_mask = p_navigation_mesh->get_collision_mask();

		if (parsed_geometry_type == NavigationMesh::PARSED_GEOMETRY_MESH_INSTANCES || (parsed_geometry_type == NavigationMesh::PARSED_GEOMETRY_STATIC_COLLIDERS && csgshape3d->is_using_collision() && (csgshape3d->get_collision_layer() & parsed_collision_mask)) || parsed_geometry_type == NavigationMesh::PARSED_GEOMETRY_BOTH) {
			CSGShape3D *csg_shape = Object::cast_to<CSGShape3D>(p_node);
			Array meshes = csg_shape->get_meshes();
			if (!meshes.is_empty()) {
				Ref<Mesh> mesh = meshes[1];
				if (mesh.is_valid()) {
					p_source_geometry_data->add_mesh(mesh, csg_shape->get_global_transform());
				}
			}
		}
	}
}
#endif // MODULE_CSG_ENABLED

#ifdef MODULE_GRIDMAP_ENABLED
void NavMeshGenerator3D::generator_parse_gridmap_node(const Ref<NavigationMesh> &p_navigation_mesh, Ref<NavigationMeshSourceGeometryData3D> p_source_geometry_data, Node *p_node) {
	GridMap *gridmap = Object::cast_to<GridMap>(p_node);

	if (gridmap) {
		NavigationMesh::ParsedGeometryType parsed_geometry_type = p_navigation_mesh->get_parsed_geometry_type();
		uint32_t parsed_collision_mask = p_navigation_mesh->get_collision_mask();

		if (parsed_geometry_type == NavigationMesh::PARSED_GEOMETRY_MESH_INSTANCES || parsed_geometry_type == NavigationMesh::PARSED_GEOMETRY_BOTH) {
			Array meshes = gridmap->get_meshes();
			Transform3D xform = gridmap->get_global_transform();
			for (int i = 0; i < meshes.size(); i += 2) {
				Ref<Mesh> mesh = meshes[i + 1];
				if (mesh.is_valid()) {
					p_source_geometry_data->add_mesh(mesh, xform * (Transform3D)meshes[i]);
				}
			}
		}

		else if ((parsed_geometry_type == NavigationMesh::PARSED_GEOMETRY_STATIC_COLLIDERS || parsed_geometry_type == NavigationMesh::PARSED_GEOMETRY_BOTH) && (gridmap->get_collision_layer() & parsed_collision_mask)) {
			Array shapes = gridmap->get_collision_shapes();
			for (int i = 0; i < shapes.size(); i += 2) {
				RID shape = shapes[i + 1];
				PhysicsServer3D::ShapeType type = PhysicsServer3D::get_singleton()->shape_get_type(shape);
				Variant data = PhysicsServer3D::get_singleton()->shape_get_data(shape);

				switch (type) {
					case PhysicsServer3D::SHAPE_SPHERE: {
						real_t radius = data;
						Array arr;
						arr.resize(RS::ARRAY_MAX);
						SphereMesh::create_mesh_array(arr, radius, radius * 2.0);
						p_source_geometry_data->add_mesh_array(arr, shapes[i]);
					} break;
					case PhysicsServer3D::SHAPE_BOX: {
						Vector3 extents = data;
						Array arr;
						arr.resize(RS::ARRAY_MAX);
						BoxMesh::create_mesh_array(arr, extents * 2.0);
						p_source_geometry_data->add_mesh_array(arr, shapes[i]);
					} break;
					case PhysicsServer3D::SHAPE_CAPSULE: {
						Dictionary dict = data;
						real_t radius = dict["radius"];
						real_t height = dict["height"];
						Array arr;
						arr.resize(RS::ARRAY_MAX);
						CapsuleMesh::create_mesh_array(arr, radius, height);
						p_source_geometry_data->add_mesh_array(arr, shapes[i]);
					} break;
					case PhysicsServer3D::SHAPE_CYLINDER: {
						Dictionary dict = data;
						real_t radius = dict["radius"];
						real_t height = dict["height"];
						Array arr;
						arr.resize(RS::ARRAY_MAX);
						CylinderMesh::create_mesh_array(arr, radius, radius, height);
						p_source_geometry_data->add_mesh_array(arr, shapes[i]);
					} break;
					case PhysicsServer3D::SHAPE_CONVEX_POLYGON: {
						PackedVector3Array vertices = data;
						Geometry3D::MeshData md;

						Error err = ConvexHullComputer::convex_hull(vertices, md);

						if (err == OK) {
							PackedVector3Array faces;

							for (const Geometry3D::MeshData::Face &face : md.faces) {
								for (uint32_t k = 2; k < face.indices.size(); ++k) {
									faces.push_back(md.vertices[face.indices[0]]);
									faces.push_back(md.vertices[face.indices[k - 1]]);
									faces.push_back(md.vertices[face.indices[k]]);
								}
							}

							p_source_geometry_data->add_faces(faces, shapes[i]);
						}
					} break;
					case PhysicsServer3D::SHAPE_CONCAVE_POLYGON: {
						Dictionary dict = data;
						PackedVector3Array faces = Variant(dict["faces"]);
						p_source_geometry_data->add_faces(faces, shapes[i]);
					} break;
					case PhysicsServer3D::SHAPE_HEIGHTMAP: {
						Dictionary dict = data;
						///< dict( int:"width", int:"depth",float:"cell_size", float_array:"heights"
						int heightmap_depth = dict["depth"];
						int heightmap_width = dict["width"];

						if (heightmap_depth >= 2 && heightmap_width >= 2) {
							const Vector<real_t> &map_data = dict["heights"];

							Vector2 heightmap_gridsize(heightmap_width - 1, heightmap_depth - 1);
							Vector3 start = Vector3(heightmap_gridsize.x, 0, heightmap_gridsize.y) * -0.5;

							Vector<Vector3> vertex_array;
							vertex_array.resize((heightmap_depth - 1) * (heightmap_width - 1) * 6);
							Vector3 *vertex_array_ptrw = vertex_array.ptrw();
							const real_t *map_data_ptr = map_data.ptr();
							int vertex_index = 0;

							for (int d = 0; d < heightmap_depth - 1; d++) {
								for (int w = 0; w < heightmap_width - 1; w++) {
									vertex_array_ptrw[vertex_index] = start + Vector3(w, map_data_ptr[(heightmap_width * d) + w], d);
									vertex_array_ptrw[vertex_index + 1] = start + Vector3(w + 1, map_data_ptr[(heightmap_width * d) + w + 1], d);
									vertex_array_ptrw[vertex_index + 2] = start + Vector3(w, map_data_ptr[(heightmap_width * d) + heightmap_width + w], d + 1);
									vertex_array_ptrw[vertex_index + 3] = start + Vector3(w + 1, map_data_ptr[(heightmap_width * d) + w + 1], d);
									vertex_array_ptrw[vertex_index + 4] = start + Vector3(w + 1, map_data_ptr[(heightmap_width * d) + heightmap_width + w + 1], d + 1);
									vertex_array_ptrw[vertex_index + 5] = start + Vector3(w, map_data_ptr[(heightmap_width * d) + heightmap_width + w], d + 1);
									vertex_index += 6;
								}
							}
							if (vertex_array.size() > 0) {
								p_source_geometry_data->add_faces(vertex_array, shapes[i]);
							}
						}
					} break;
					default: {
						WARN_PRINT("Unsupported collision shape type.");
					} break;
				}
			}
		}
	}
}
#endif // MODULE_GRIDMAP_ENABLED

void NavMeshGenerator3D::generator_parse_navigationobstacle_node(const Ref<NavigationMesh> &p_navigation_mesh, Ref<NavigationMeshSourceGeometryData3D> p_source_geometry_data, Node *p_node) {
	NavigationObstacle3D *obstacle = Object::cast_to<NavigationObstacle3D>(p_node);
	if (obstacle == nullptr) {
		return;
	}

	if (!obstacle->get_affect_navigation_mesh()) {
		return;
	}

	const float elevation = obstacle->get_global_position().y + p_source_geometry_data->root_node_transform.origin.y;
	// Prevent non-positive scaling.
	const Vector3 safe_scale = obstacle->get_global_basis().get_scale().abs().maxf(0.001);
	const float obstacle_radius = obstacle->get_radius();

	if (obstacle_radius > 0.0) {
		// Radius defined obstacle should be uniformly scaled from obstacle basis max scale axis.
		const float scaling_max_value = safe_scale[safe_scale.max_axis_index()];
		const Vector3 uniform_max_scale = Vector3(scaling_max_value, scaling_max_value, scaling_max_value);
		const Transform3D obstacle_circle_transform = p_source_geometry_data->root_node_transform * Transform3D(Basis().scaled(uniform_max_scale), obstacle->get_global_position());

		Vector<Vector3> obstruction_circle_vertices;

		// The point of this is that the moving obstacle can make a simple hole in the navigation mesh and affect the pathfinding.
		// Without, navigation paths can go directly through the middle of the obstacle and conflict with the avoidance to get agents stuck.
		// No place for excessive "round" detail here. Every additional edge adds a high cost for something that needs to be quick, not pretty.
		static const int circle_points = 12;

		obstruction_circle_vertices.resize(circle_points);
		Vector3 *circle_vertices_ptrw = obstruction_circle_vertices.ptrw();
		const real_t circle_point_step = Math_TAU / circle_points;

		for (int i = 0; i < circle_points; i++) {
			const float angle = i * circle_point_step;
			circle_vertices_ptrw[i] = obstacle_circle_transform.xform(Vector3(Math::cos(angle) * obstacle_radius, 0.0, Math::sin(angle) * obstacle_radius));
		}

		p_source_geometry_data->add_projected_obstruction(obstruction_circle_vertices, elevation - obstacle_radius, scaling_max_value * obstacle_radius, obstacle->get_carve_navigation_mesh());
	}

	// Obstacles are projected to the xz-plane, so only rotation around the y-axis can be taken into account.
	const Transform3D node_xform = p_source_geometry_data->root_node_transform * Transform3D(Basis().scaled(safe_scale).rotated(Vector3(0.0, 1.0, 0.0), obstacle->get_global_rotation().y), obstacle->get_global_position());

	const Vector<Vector3> &obstacle_vertices = obstacle->get_vertices();

	if (obstacle_vertices.is_empty()) {
		return;
	}

	Vector<Vector3> obstruction_shape_vertices;
	obstruction_shape_vertices.resize(obstacle_vertices.size());

	const Vector3 *obstacle_vertices_ptr = obstacle_vertices.ptr();
	Vector3 *obstruction_shape_vertices_ptrw = obstruction_shape_vertices.ptrw();

	for (int i = 0; i < obstacle_vertices.size(); i++) {
		obstruction_shape_vertices_ptrw[i] = node_xform.xform(obstacle_vertices_ptr[i]);
		obstruction_shape_vertices_ptrw[i].y = 0.0;
	}
	p_source_geometry_data->add_projected_obstruction(obstruction_shape_vertices, elevation, safe_scale.y * obstacle->get_height(), obstacle->get_carve_navigation_mesh());
}

void NavMeshGenerator3D::generator_parse_source_geometry_data(const Ref<NavigationMesh> &p_navigation_mesh, Ref<NavigationMeshSourceGeometryData3D> p_source_geometry_data, Node *p_root_node) {
	List<Node *> parse_nodes;

	if (p_navigation_mesh->get_source_geometry_mode() == NavigationMesh::SOURCE_GEOMETRY_ROOT_NODE_CHILDREN) {
		parse_nodes.push_back(p_root_node);
	} else {
		p_root_node->get_tree()->get_nodes_in_group(p_navigation_mesh->get_source_group_name(), &parse_nodes);
	}

	Transform3D root_node_transform = Transform3D();
	if (Object::cast_to<Node3D>(p_root_node)) {
		root_node_transform = Object::cast_to<Node3D>(p_root_node)->get_global_transform().affine_inverse();
	}

	p_source_geometry_data->clear();
	p_source_geometry_data->root_node_transform = root_node_transform;

	bool recurse_children = p_navigation_mesh->get_source_geometry_mode() != NavigationMesh::SOURCE_GEOMETRY_GROUPS_EXPLICIT;

	for (Node *parse_node : parse_nodes) {
		generator_parse_geometry_node(p_navigation_mesh, p_source_geometry_data, parse_node, recurse_children);
	}
}

void NavMeshGenerator3D::generator_bake_from_source_geometry_data(Ref<NavigationMesh> p_navigation_mesh, const Ref<NavigationMeshSourceGeometryData3D> &p_source_geometry_data) {
	if (p_navigation_mesh.is_null() || p_source_geometry_data.is_null()) {
		return;
	}

	Vector<float> source_geometry_vertices;
	Vector<int> source_geometry_indices;
	Vector<NavigationMeshSourceGeometryData3D::ProjectedObstruction> projected_obstructions;

	p_source_geometry_data->get_data(
			source_geometry_vertices,
			source_geometry_indices,
			projected_obstructions);

	if (source_geometry_vertices.size() < 3 || source_geometry_indices.size() < 3) {
		return;
	}

	rcHeightfield *hf = nullptr;
	rcCompactHeightfield *chf = nullptr;
	rcContourSet *cset = nullptr;
	rcPolyMesh *poly_mesh = nullptr;
	rcPolyMeshDetail *detail_mesh = nullptr;
	rcContext ctx;

	// added to keep track of steps, no functionality right now
	String bake_state = "";

	bake_state = "Setting up Configuration..."; // step #1

	const float *verts = source_geometry_vertices.ptr();
	const int nverts = source_geometry_vertices.size() / 3;
	const int *tris = source_geometry_indices.ptr();
	const int ntris = source_geometry_indices.size() / 3;

	float bmin[3], bmax[3];
	rcCalcBounds(verts, nverts, bmin, bmax);

	rcConfig cfg;
	memset(&cfg, 0, sizeof(cfg));

	cfg.cs = p_navigation_mesh->get_cell_size();
	cfg.ch = p_navigation_mesh->get_cell_height();
	if (p_navigation_mesh->get_border_size() > 0.0) {
		cfg.borderSize = (int)Math::ceil(p_navigation_mesh->get_border_size() / cfg.cs);
	}
	cfg.walkableSlopeAngle = p_navigation_mesh->get_agent_max_slope();
	cfg.walkableHeight = (int)Math::ceil(p_navigation_mesh->get_agent_height() / cfg.ch);
	cfg.walkableClimb = (int)Math::floor(p_navigation_mesh->get_agent_max_climb() / cfg.ch);
	cfg.walkableRadius = (int)Math::ceil(p_navigation_mesh->get_agent_radius() / cfg.cs);
	cfg.maxEdgeLen = (int)(p_navigation_mesh->get_edge_max_length() / p_navigation_mesh->get_cell_size());
	cfg.maxSimplificationError = p_navigation_mesh->get_edge_max_error();
	cfg.minRegionArea = (int)(p_navigation_mesh->get_region_min_size() * p_navigation_mesh->get_region_min_size());
	cfg.mergeRegionArea = (int)(p_navigation_mesh->get_region_merge_size() * p_navigation_mesh->get_region_merge_size());
	cfg.maxVertsPerPoly = (int)p_navigation_mesh->get_vertices_per_polygon();
	cfg.detailSampleDist = MAX(p_navigation_mesh->get_cell_size() * p_navigation_mesh->get_detail_sample_distance(), 0.1f);
	cfg.detailSampleMaxError = p_navigation_mesh->get_cell_height() * p_navigation_mesh->get_detail_sample_max_error();

	if (p_navigation_mesh->get_border_size() > 0.0 && Math::fmod(p_navigation_mesh->get_border_size(), p_navigation_mesh->get_cell_size()) != 0.0) {
		WARN_PRINT("Property border_size is ceiled to cell_size voxel units and loses precision.");
	}
	if (!Math::is_equal_approx((float)cfg.walkableHeight * cfg.ch, p_navigation_mesh->get_agent_height())) {
		WARN_PRINT("Property agent_height is ceiled to cell_height voxel units and loses precision.");
	}
	if (!Math::is_equal_approx((float)cfg.walkableClimb * cfg.ch, p_navigation_mesh->get_agent_max_climb())) {
		WARN_PRINT("Property agent_max_climb is floored to cell_height voxel units and loses precision.");
	}
	if (!Math::is_equal_approx((float)cfg.walkableRadius * cfg.cs, p_navigation_mesh->get_agent_radius())) {
		WARN_PRINT("Property agent_radius is ceiled to cell_size voxel units and loses precision.");
	}
	if (!Math::is_equal_approx((float)cfg.maxEdgeLen * cfg.cs, p_navigation_mesh->get_edge_max_length())) {
		WARN_PRINT("Property edge_max_length is rounded to cell_size voxel units and loses precision.");
	}
	if (!Math::is_equal_approx((float)cfg.minRegionArea, p_navigation_mesh->get_region_min_size() * p_navigation_mesh->get_region_min_size())) {
		WARN_PRINT("Property region_min_size is converted to int and loses precision.");
	}
	if (!Math::is_equal_approx((float)cfg.mergeRegionArea, p_navigation_mesh->get_region_merge_size() * p_navigation_mesh->get_region_merge_size())) {
		WARN_PRINT("Property region_merge_size is converted to int and loses precision.");
	}
	if (!Math::is_equal_approx((float)cfg.maxVertsPerPoly, p_navigation_mesh->get_vertices_per_polygon())) {
		WARN_PRINT("Property vertices_per_polygon is converted to int and loses precision.");
	}
	if (p_navigation_mesh->get_cell_size() * p_navigation_mesh->get_detail_sample_distance() < 0.1f) {
		WARN_PRINT("Property detail_sample_distance is clamped to 0.1 world units as the resulting value from multiplying with cell_size is too low.");
	}

	cfg.bmin[0] = bmin[0];
	cfg.bmin[1] = bmin[1];
	cfg.bmin[2] = bmin[2];
	cfg.bmax[0] = bmax[0];
	cfg.bmax[1] = bmax[1];
	cfg.bmax[2] = bmax[2];

	AABB baking_aabb = p_navigation_mesh->get_filter_baking_aabb();
	if (baking_aabb.has_volume()) {
		Vector3 baking_aabb_offset = p_navigation_mesh->get_filter_baking_aabb_offset();
		cfg.bmin[0] = baking_aabb.position[0] + baking_aabb_offset.x;
		cfg.bmin[1] = baking_aabb.position[1] + baking_aabb_offset.y;
		cfg.bmin[2] = baking_aabb.position[2] + baking_aabb_offset.z;
		cfg.bmax[0] = cfg.bmin[0] + baking_aabb.size[0];
		cfg.bmax[1] = cfg.bmin[1] + baking_aabb.size[1];
		cfg.bmax[2] = cfg.bmin[2] + baking_aabb.size[2];
	}

	bake_state = "Calculating grid size..."; // step #2
	rcCalcGridSize(cfg.bmin, cfg.bmax, cfg.cs, &cfg.width, &cfg.height);

	// ~30000000 seems to be around sweetspot where Editor baking breaks
	if ((cfg.width * cfg.height) > 30000000 && GLOBAL_GET("navigation/baking/use_crash_prevention_checks")) {
		ERR_FAIL_MSG("Baking interrupted."
					 "\nNavigationMesh baking process would likely crash the engine."
					 "\nSource geometry is suspiciously big for the current Cell Size and Cell Height in the NavMesh Resource bake settings."
					 "\nIf baking does not crash the engine or fail, the resulting NavigationMesh will create serious pathfinding performance issues."
					 "\nIt is advised to increase Cell Size and/or Cell Height in the NavMesh Resource bake settings or reduce the size / scale of the source geometry."
					 "\nIf you would like to try baking anyway, disable the 'navigation/baking/use_crash_prevention_checks' project setting.");
		return;
	}

	bake_state = "Creating heightfield..."; // step #3
	hf = rcAllocHeightfield();

	ERR_FAIL_NULL(hf);
	ERR_FAIL_COND(!rcCreateHeightfield(&ctx, *hf, cfg.width, cfg.height, cfg.bmin, cfg.bmax, cfg.cs, cfg.ch));

	bake_state = "Marking walkable triangles..."; // step #4
	{
		Vector<unsigned char> tri_areas;
		tri_areas.resize(ntris);

		ERR_FAIL_COND(tri_areas.is_empty());

		memset(tri_areas.ptrw(), 0, ntris * sizeof(unsigned char));
		rcMarkWalkableTriangles(&ctx, cfg.walkableSlopeAngle, verts, nverts, tris, ntris, tri_areas.ptrw());

		ERR_FAIL_COND(!rcRasterizeTriangles(&ctx, verts, nverts, tris, tri_areas.ptr(), ntris, *hf, cfg.walkableClimb));
	}

	if (p_navigation_mesh->get_filter_low_hanging_obstacles()) {
		rcFilterLowHangingWalkableObstacles(&ctx, cfg.walkableClimb, *hf);
	}
	if (p_navigation_mesh->get_filter_ledge_spans()) {
		rcFilterLedgeSpans(&ctx, cfg.walkableHeight, cfg.walkableClimb, *hf);
	}
	if (p_navigation_mesh->get_filter_walkable_low_height_spans()) {
		rcFilterWalkableLowHeightSpans(&ctx, cfg.walkableHeight, *hf);
	}

	bake_state = "Constructing compact heightfield..."; // step #5

	chf = rcAllocCompactHeightfield();

	ERR_FAIL_NULL(chf);
	ERR_FAIL_COND(!rcBuildCompactHeightfield(&ctx, cfg.walkableHeight, cfg.walkableClimb, *hf, *chf));

	rcFreeHeightField(hf);
	hf = nullptr;

	// Add obstacles to the source geometry. Those will be affected by e.g. agent_radius.
	if (!projected_obstructions.is_empty()) {
		for (const NavigationMeshSourceGeometryData3D::ProjectedObstruction &projected_obstruction : projected_obstructions) {
			if (projected_obstruction.carve) {
				continue;
			}
			if (projected_obstruction.vertices.is_empty() || projected_obstruction.vertices.size() % 3 != 0) {
				continue;
			}

			const float *projected_obstruction_verts = projected_obstruction.vertices.ptr();
			const int projected_obstruction_nverts = projected_obstruction.vertices.size() / 3;

			rcMarkConvexPolyArea(&ctx, projected_obstruction_verts, projected_obstruction_nverts, projected_obstruction.elevation, projected_obstruction.elevation + projected_obstruction.height, RC_NULL_AREA, *chf);
		}
	}

	bake_state = "Eroding walkable area..."; // step #6

	ERR_FAIL_COND(!rcErodeWalkableArea(&ctx, cfg.walkableRadius, *chf));

	// Carve obstacles to the eroded geometry. Those will NOT be affected by e.g. agent_radius because that step is already done.
	if (!projected_obstructions.is_empty()) {
		for (const NavigationMeshSourceGeometryData3D::ProjectedObstruction &projected_obstruction : projected_obstructions) {
			if (!projected_obstruction.carve) {
				continue;
			}
			if (projected_obstruction.vertices.is_empty() || projected_obstruction.vertices.size() % 3 != 0) {
				continue;
			}

			const float *projected_obstruction_verts = projected_obstruction.vertices.ptr();
			const int projected_obstruction_nverts = projected_obstruction.vertices.size() / 3;

			rcMarkConvexPolyArea(&ctx, projected_obstruction_verts, projected_obstruction_nverts, projected_obstruction.elevation, projected_obstruction.elevation + projected_obstruction.height, RC_NULL_AREA, *chf);
		}
	}

	bake_state = "Partitioning..."; // step #7

	if (p_navigation_mesh->get_sample_partition_type() == NavigationMesh::SAMPLE_PARTITION_WATERSHED) {
		ERR_FAIL_COND(!rcBuildDistanceField(&ctx, *chf));
		ERR_FAIL_COND(!rcBuildRegions(&ctx, *chf, cfg.borderSize, cfg.minRegionArea, cfg.mergeRegionArea));
	} else if (p_navigation_mesh->get_sample_partition_type() == NavigationMesh::SAMPLE_PARTITION_MONOTONE) {
		ERR_FAIL_COND(!rcBuildRegionsMonotone(&ctx, *chf, cfg.borderSize, cfg.minRegionArea, cfg.mergeRegionArea));
	} else {
		ERR_FAIL_COND(!rcBuildLayerRegions(&ctx, *chf, cfg.borderSize, cfg.minRegionArea));
	}

	bake_state = "Creating contours..."; // step #8

	cset = rcAllocContourSet();

	ERR_FAIL_NULL(cset);
	ERR_FAIL_COND(!rcBuildContours(&ctx, *chf, cfg.maxSimplificationError, cfg.maxEdgeLen, *cset));

	bake_state = "Creating polymesh..."; // step #9

	poly_mesh = rcAllocPolyMesh();
	ERR_FAIL_NULL(poly_mesh);
	ERR_FAIL_COND(!rcBuildPolyMesh(&ctx, *cset, cfg.maxVertsPerPoly, *poly_mesh));

	detail_mesh = rcAllocPolyMeshDetail();
	ERR_FAIL_NULL(detail_mesh);
	ERR_FAIL_COND(!rcBuildPolyMeshDetail(&ctx, *poly_mesh, *chf, cfg.detailSampleDist, cfg.detailSampleMaxError, *detail_mesh));

	rcFreeCompactHeightfield(chf);
	chf = nullptr;
	rcFreeContourSet(cset);
	cset = nullptr;

	bake_state = "Converting to native navigation mesh..."; // step #10

	Vector<Vector3> nav_vertices;
	Vector<Vector<int>> nav_polygons;

	HashMap<Vector3, int> recast_vertex_to_native_index;
	LocalVector<int> recast_index_to_native_index;
	recast_index_to_native_index.resize(detail_mesh->nverts);

	for (int i = 0; i < detail_mesh->nverts; i++) {
		const float *v = &detail_mesh->verts[i * 3];
		const Vector3 vertex = Vector3(v[0], v[1], v[2]);
		int *existing_index_ptr = recast_vertex_to_native_index.getptr(vertex);
		if (!existing_index_ptr) {
			int new_index = recast_vertex_to_native_index.size();
			recast_index_to_native_index[i] = new_index;
			recast_vertex_to_native_index[vertex] = new_index;
			nav_vertices.push_back(vertex);
		} else {
			recast_index_to_native_index[i] = *existing_index_ptr;
		}
	}

	for (int i = 0; i < detail_mesh->nmeshes; i++) {
		const unsigned int *detail_mesh_m = &detail_mesh->meshes[i * 4];
		const unsigned int detail_mesh_bverts = detail_mesh_m[0];
		const unsigned int detail_mesh_m_btris = detail_mesh_m[2];
		const unsigned int detail_mesh_ntris = detail_mesh_m[3];
		const unsigned char *detail_mesh_tris = &detail_mesh->tris[detail_mesh_m_btris * 4];
		for (unsigned int j = 0; j < detail_mesh_ntris; j++) {
			Vector<int> nav_indices;
			nav_indices.resize(3);
			// Polygon order in recast is opposite than godot's
			int index1 = ((int)(detail_mesh_bverts + detail_mesh_tris[j * 4 + 0]));
			int index2 = ((int)(detail_mesh_bverts + detail_mesh_tris[j * 4 + 2]));
			int index3 = ((int)(detail_mesh_bverts + detail_mesh_tris[j * 4 + 1]));

			nav_indices.write[0] = recast_index_to_native_index[index1];
			nav_indices.write[1] = recast_index_to_native_index[index2];
			nav_indices.write[2] = recast_index_to_native_index[index3];

			nav_polygons.push_back(nav_indices);
		}
	}

	p_navigation_mesh->set_data(nav_vertices, nav_polygons);

	bake_state = "Cleanup..."; // step #11

	rcFreePolyMesh(poly_mesh);
	poly_mesh = nullptr;
	rcFreePolyMeshDetail(detail_mesh);
	detail_mesh = nullptr;

	bake_state = "Baking finished."; // step #12
}

bool NavMeshGenerator3D::generator_emit_callback(const Callable &p_callback) {
	ERR_FAIL_COND_V(!p_callback.is_valid(), false);

	Callable::CallError ce;
	Variant result;
	p_callback.callp(nullptr, 0, result, ce);

	return ce.error == Callable::CallError::CALL_OK;
}

RID NavMeshGenerator3D::source_geometry_parser_create() {
	RWLockWrite write_lock(generator_rid_rwlock);

	RID rid = generator_parser_owner.make_rid();

	NavMeshGeometryParser3D *parser = generator_parser_owner.get_or_null(rid);
	parser->self = rid;

	generator_parsers.push_back(parser);

	return rid;
}

void NavMeshGenerator3D::source_geometry_parser_set_callback(RID p_parser, const Callable &p_callback) {
	RWLockWrite write_lock(generator_rid_rwlock);

	NavMeshGeometryParser3D *parser = generator_parser_owner.get_or_null(p_parser);
	ERR_FAIL_NULL(parser);

	parser->callback = p_callback;
}

bool NavMeshGenerator3D::owns(RID p_object) {
	RWLockRead read_lock(generator_rid_rwlock);
	return generator_parser_owner.owns(p_object);
}

void NavMeshGenerator3D::free(RID p_object) {
	RWLockWrite write_lock(generator_rid_rwlock);

	if (generator_parser_owner.owns(p_object)) {
		NavMeshGeometryParser3D *parser = generator_parser_owner.get_or_null(p_object);

		generator_parsers.erase(parser);

		generator_parser_owner.free(p_object);
	} else {
		ERR_PRINT("Attempted to free a NavMeshGenerator3D RID that did not exist (or was already freed).");
	}
}

#endif // _3D_DISABLED

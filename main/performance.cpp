/**************************************************************************/
/*  performance.cpp                                                       */
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

#include "performance.h"

#include "core/os/os.h"
#include "core/variant/typed_array.h"
#include "scene/main/node.h"
#include "scene/main/scene_tree.h"
#include "servers/audio_server.h"
#include "servers/navigation_server_3d.h"
#include "servers/rendering_server.h"

// 2D
#include "servers/physics_server_2d.h"

#ifndef _3D_DISABLED
#include "servers/physics_server_3d.h"
#endif // _3D_DISABLED

Performance *Performance::singleton = nullptr;

void Performance::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_monitor", "monitor"), &Performance::get_monitor);
	ClassDB::bind_method(D_METHOD("add_custom_monitor", "id", "callable", "arguments"), &Performance::add_custom_monitor, DEFVAL(Array()));
	ClassDB::bind_method(D_METHOD("remove_custom_monitor", "id"), &Performance::remove_custom_monitor);
	ClassDB::bind_method(D_METHOD("has_custom_monitor", "id"), &Performance::has_custom_monitor);
	ClassDB::bind_method(D_METHOD("get_custom_monitor", "id"), &Performance::get_custom_monitor);
	ClassDB::bind_method(D_METHOD("get_monitor_modification_time"), &Performance::get_monitor_modification_time);
	ClassDB::bind_method(D_METHOD("get_custom_monitor_names"), &Performance::get_custom_monitor_names);

	BIND_ENUM_CONSTANT(TIME_FPS);
	BIND_ENUM_CONSTANT(TIME_PROCESS);
	BIND_ENUM_CONSTANT(TIME_PHYSICS_PROCESS);
	BIND_ENUM_CONSTANT(TIME_NAVIGATION_PROCESS);
	BIND_ENUM_CONSTANT(MEMORY_STATIC);
	BIND_ENUM_CONSTANT(MEMORY_STATIC_MAX);
	BIND_ENUM_CONSTANT(MEMORY_MESSAGE_BUFFER_MAX);
	BIND_ENUM_CONSTANT(OBJECT_COUNT);
	BIND_ENUM_CONSTANT(OBJECT_RESOURCE_COUNT);
	BIND_ENUM_CONSTANT(OBJECT_NODE_COUNT);
	BIND_ENUM_CONSTANT(OBJECT_ORPHAN_NODE_COUNT);
	BIND_ENUM_CONSTANT(RENDER_TOTAL_OBJECTS_IN_FRAME);
	BIND_ENUM_CONSTANT(RENDER_TOTAL_PRIMITIVES_IN_FRAME);
	BIND_ENUM_CONSTANT(RENDER_TOTAL_DRAW_CALLS_IN_FRAME);
	BIND_ENUM_CONSTANT(RENDER_VIDEO_MEM_USED);
	BIND_ENUM_CONSTANT(RENDER_TEXTURE_MEM_USED);
	BIND_ENUM_CONSTANT(RENDER_BUFFER_MEM_USED);
	BIND_ENUM_CONSTANT(PHYSICS_2D_ACTIVE_OBJECTS);
	BIND_ENUM_CONSTANT(PHYSICS_2D_COLLISION_PAIRS);
	BIND_ENUM_CONSTANT(PHYSICS_2D_ISLAND_COUNT);
#ifndef _3D_DISABLED
	BIND_ENUM_CONSTANT(PHYSICS_3D_ACTIVE_OBJECTS);
	BIND_ENUM_CONSTANT(PHYSICS_3D_COLLISION_PAIRS);
	BIND_ENUM_CONSTANT(PHYSICS_3D_ISLAND_COUNT);
#endif // _3D_DISABLED
	BIND_ENUM_CONSTANT(AUDIO_OUTPUT_LATENCY);
	BIND_ENUM_CONSTANT(NAVIGATION_ACTIVE_MAPS);
	BIND_ENUM_CONSTANT(NAVIGATION_REGION_COUNT);
	BIND_ENUM_CONSTANT(NAVIGATION_AGENT_COUNT);
	BIND_ENUM_CONSTANT(NAVIGATION_LINK_COUNT);
	BIND_ENUM_CONSTANT(NAVIGATION_POLYGON_COUNT);
	BIND_ENUM_CONSTANT(NAVIGATION_EDGE_COUNT);
	BIND_ENUM_CONSTANT(NAVIGATION_EDGE_MERGE_COUNT);
	BIND_ENUM_CONSTANT(NAVIGATION_EDGE_CONNECTION_COUNT);
	BIND_ENUM_CONSTANT(NAVIGATION_EDGE_FREE_COUNT);
	BIND_ENUM_CONSTANT(NAVIGATION_OBSTACLE_COUNT);
	BIND_ENUM_CONSTANT(PIPELINE_COMPILATIONS_CANVAS);
	BIND_ENUM_CONSTANT(PIPELINE_COMPILATIONS_MESH);
	BIND_ENUM_CONSTANT(PIPELINE_COMPILATIONS_SURFACE);
	BIND_ENUM_CONSTANT(PIPELINE_COMPILATIONS_DRAW);
	BIND_ENUM_CONSTANT(PIPELINE_COMPILATIONS_SPECIALIZATION);
	BIND_ENUM_CONSTANT(MONITOR_MAX);
}

int Performance::_get_node_count() const {
	MainLoop *ml = OS::get_singleton()->get_main_loop();
	SceneTree *sml = Object::cast_to<SceneTree>(ml);
	if (!sml) {
		return 0;
	}
	return sml->get_node_count();
}

String Performance::get_monitor_name(Monitor p_monitor) const {
	ERR_FAIL_INDEX_V(p_monitor, MONITOR_MAX, String());
	static const char *names[MONITOR_MAX] = {
		PNAME("time/fps"),
		PNAME("time/process"),
		PNAME("time/physics_process"),
		PNAME("time/navigation_process"),
		PNAME("memory/static"),
		PNAME("memory/static_max"),
		PNAME("memory/msg_buf_max"),
		PNAME("object/objects"),
		PNAME("object/resources"),
		PNAME("object/nodes"),
		PNAME("object/orphan_nodes"),
		PNAME("raster/total_objects_drawn"),
		PNAME("raster/total_primitives_drawn"),
		PNAME("raster/total_draw_calls"),
		PNAME("video/video_mem"),
		PNAME("video/texture_mem"),
		PNAME("video/buffer_mem"),
		PNAME("physics_2d/active_objects"),
		PNAME("physics_2d/collision_pairs"),
		PNAME("physics_2d/islands"),
		PNAME("physics_3d/active_objects"),
		PNAME("physics_3d/collision_pairs"),
		PNAME("physics_3d/islands"),
		PNAME("audio/driver/output_latency"),
		PNAME("navigation/active_maps"),
		PNAME("navigation/regions"),
		PNAME("navigation/agents"),
		PNAME("navigation/links"),
		PNAME("navigation/polygons"),
		PNAME("navigation/edges"),
		PNAME("navigation/edges_merged"),
		PNAME("navigation/edges_connected"),
		PNAME("navigation/edges_free"),
		PNAME("navigation/obstacles"),
		PNAME("pipeline/compilations_canvas"),
		PNAME("pipeline/compilations_mesh"),
		PNAME("pipeline/compilations_surface"),
		PNAME("pipeline/compilations_draw"),
		PNAME("pipeline/compilations_specialization"),
	};

	return names[p_monitor];
}

double Performance::get_monitor(Monitor p_monitor) const {
	switch (p_monitor) {
		case TIME_FPS:
			return Engine::get_singleton()->get_frames_per_second();
		case TIME_PROCESS:
			return _process_time;
		case TIME_PHYSICS_PROCESS:
			return _physics_process_time;
		case TIME_NAVIGATION_PROCESS:
			return _navigation_process_time;
		case MEMORY_STATIC:
			return Memory::get_mem_usage();
		case MEMORY_STATIC_MAX:
			return Memory::get_mem_max_usage();
		case MEMORY_MESSAGE_BUFFER_MAX:
			return MessageQueue::get_singleton()->get_max_buffer_usage();
		case OBJECT_COUNT:
			return ObjectDB::get_object_count();
		case OBJECT_RESOURCE_COUNT:
			return ResourceCache::get_cached_resource_count();
		case OBJECT_NODE_COUNT:
			return _get_node_count();
		case OBJECT_ORPHAN_NODE_COUNT:
			return Node::orphan_node_count;
		case RENDER_TOTAL_OBJECTS_IN_FRAME:
			return RS::get_singleton()->get_rendering_info(RS::RENDERING_INFO_TOTAL_OBJECTS_IN_FRAME);
		case RENDER_TOTAL_PRIMITIVES_IN_FRAME:
			return RS::get_singleton()->get_rendering_info(RS::RENDERING_INFO_TOTAL_PRIMITIVES_IN_FRAME);
		case RENDER_TOTAL_DRAW_CALLS_IN_FRAME:
			return RS::get_singleton()->get_rendering_info(RS::RENDERING_INFO_TOTAL_DRAW_CALLS_IN_FRAME);
		case RENDER_VIDEO_MEM_USED:
			return RS::get_singleton()->get_rendering_info(RS::RENDERING_INFO_VIDEO_MEM_USED);
		case RENDER_TEXTURE_MEM_USED:
			return RS::get_singleton()->get_rendering_info(RS::RENDERING_INFO_TEXTURE_MEM_USED);
		case RENDER_BUFFER_MEM_USED:
			return RS::get_singleton()->get_rendering_info(RS::RENDERING_INFO_BUFFER_MEM_USED);
		case PIPELINE_COMPILATIONS_CANVAS:
			return RS::get_singleton()->get_rendering_info(RS::RENDERING_INFO_PIPELINE_COMPILATIONS_CANVAS);
		case PIPELINE_COMPILATIONS_MESH:
			return RS::get_singleton()->get_rendering_info(RS::RENDERING_INFO_PIPELINE_COMPILATIONS_MESH);
		case PIPELINE_COMPILATIONS_SURFACE:
			return RS::get_singleton()->get_rendering_info(RS::RENDERING_INFO_PIPELINE_COMPILATIONS_SURFACE);
		case PIPELINE_COMPILATIONS_DRAW:
			return RS::get_singleton()->get_rendering_info(RS::RENDERING_INFO_PIPELINE_COMPILATIONS_DRAW);
		case PIPELINE_COMPILATIONS_SPECIALIZATION:
			return RS::get_singleton()->get_rendering_info(RS::RENDERING_INFO_PIPELINE_COMPILATIONS_SPECIALIZATION);
		case PHYSICS_2D_ACTIVE_OBJECTS:
			return PhysicsServer2D::get_singleton()->get_process_info(PhysicsServer2D::INFO_ACTIVE_OBJECTS);
		case PHYSICS_2D_COLLISION_PAIRS:
			return PhysicsServer2D::get_singleton()->get_process_info(PhysicsServer2D::INFO_COLLISION_PAIRS);
		case PHYSICS_2D_ISLAND_COUNT:
			return PhysicsServer2D::get_singleton()->get_process_info(PhysicsServer2D::INFO_ISLAND_COUNT);
#ifdef _3D_DISABLED
		case PHYSICS_3D_ACTIVE_OBJECTS:
			return 0;
		case PHYSICS_3D_COLLISION_PAIRS:
			return 0;
		case PHYSICS_3D_ISLAND_COUNT:
			return 0;
#else
		case PHYSICS_3D_ACTIVE_OBJECTS:
			return PhysicsServer3D::get_singleton()->get_process_info(PhysicsServer3D::INFO_ACTIVE_OBJECTS);
		case PHYSICS_3D_COLLISION_PAIRS:
			return PhysicsServer3D::get_singleton()->get_process_info(PhysicsServer3D::INFO_COLLISION_PAIRS);
		case PHYSICS_3D_ISLAND_COUNT:
			return PhysicsServer3D::get_singleton()->get_process_info(PhysicsServer3D::INFO_ISLAND_COUNT);
#endif // _3D_DISABLED

		case AUDIO_OUTPUT_LATENCY:
			return AudioServer::get_singleton()->get_output_latency();
		case NAVIGATION_ACTIVE_MAPS:
			return NavigationServer3D::get_singleton()->get_process_info(NavigationServer3D::INFO_ACTIVE_MAPS);
		case NAVIGATION_REGION_COUNT:
			return NavigationServer3D::get_singleton()->get_process_info(NavigationServer3D::INFO_REGION_COUNT);
		case NAVIGATION_AGENT_COUNT:
			return NavigationServer3D::get_singleton()->get_process_info(NavigationServer3D::INFO_AGENT_COUNT);
		case NAVIGATION_LINK_COUNT:
			return NavigationServer3D::get_singleton()->get_process_info(NavigationServer3D::INFO_LINK_COUNT);
		case NAVIGATION_POLYGON_COUNT:
			return NavigationServer3D::get_singleton()->get_process_info(NavigationServer3D::INFO_POLYGON_COUNT);
		case NAVIGATION_EDGE_COUNT:
			return NavigationServer3D::get_singleton()->get_process_info(NavigationServer3D::INFO_EDGE_COUNT);
		case NAVIGATION_EDGE_MERGE_COUNT:
			return NavigationServer3D::get_singleton()->get_process_info(NavigationServer3D::INFO_EDGE_MERGE_COUNT);
		case NAVIGATION_EDGE_CONNECTION_COUNT:
			return NavigationServer3D::get_singleton()->get_process_info(NavigationServer3D::INFO_EDGE_CONNECTION_COUNT);
		case NAVIGATION_EDGE_FREE_COUNT:
			return NavigationServer3D::get_singleton()->get_process_info(NavigationServer3D::INFO_EDGE_FREE_COUNT);
		case NAVIGATION_OBSTACLE_COUNT:
			return NavigationServer3D::get_singleton()->get_process_info(NavigationServer3D::INFO_OBSTACLE_COUNT);

		default: {
		}
	}

	return 0;
}

Performance::MonitorType Performance::get_monitor_type(Monitor p_monitor) const {
	ERR_FAIL_INDEX_V(p_monitor, MONITOR_MAX, MONITOR_TYPE_QUANTITY);
	// ugly
	static const MonitorType types[MONITOR_MAX] = {
		MONITOR_TYPE_QUANTITY,
		MONITOR_TYPE_TIME,
		MONITOR_TYPE_TIME,
		MONITOR_TYPE_TIME,
		MONITOR_TYPE_MEMORY,
		MONITOR_TYPE_MEMORY,
		MONITOR_TYPE_MEMORY,
		MONITOR_TYPE_QUANTITY,
		MONITOR_TYPE_QUANTITY,
		MONITOR_TYPE_QUANTITY,
		MONITOR_TYPE_QUANTITY,
		MONITOR_TYPE_QUANTITY,
		MONITOR_TYPE_QUANTITY,
		MONITOR_TYPE_QUANTITY,
		MONITOR_TYPE_MEMORY,
		MONITOR_TYPE_MEMORY,
		MONITOR_TYPE_MEMORY,
		MONITOR_TYPE_QUANTITY,
		MONITOR_TYPE_QUANTITY,
		MONITOR_TYPE_QUANTITY,
		MONITOR_TYPE_QUANTITY,
		MONITOR_TYPE_QUANTITY,
		MONITOR_TYPE_QUANTITY,
		MONITOR_TYPE_TIME,
		MONITOR_TYPE_QUANTITY,
		MONITOR_TYPE_QUANTITY,
		MONITOR_TYPE_QUANTITY,
		MONITOR_TYPE_QUANTITY,
		MONITOR_TYPE_QUANTITY,
		MONITOR_TYPE_QUANTITY,
		MONITOR_TYPE_QUANTITY,
		MONITOR_TYPE_QUANTITY,
		MONITOR_TYPE_QUANTITY,
		MONITOR_TYPE_QUANTITY,

	};

	return types[p_monitor];
}

void Performance::set_process_time(double p_pt) {
	_process_time = p_pt;
}

void Performance::set_physics_process_time(double p_pt) {
	_physics_process_time = p_pt;
}

void Performance::set_navigation_process_time(double p_pt) {
	_navigation_process_time = p_pt;
}

void Performance::add_custom_monitor(const StringName &p_id, const Callable &p_callable, const Vector<Variant> &p_args) {
	ERR_FAIL_COND_MSG(has_custom_monitor(p_id), "Custom monitor with id '" + String(p_id) + "' already exists.");
	_monitor_map.insert(p_id, MonitorCall(p_callable, p_args));
	_monitor_modification_time = OS::get_singleton()->get_ticks_usec();
}

void Performance::remove_custom_monitor(const StringName &p_id) {
	ERR_FAIL_COND_MSG(!has_custom_monitor(p_id), "Custom monitor with id '" + String(p_id) + "' doesn't exists.");
	_monitor_map.erase(p_id);
	_monitor_modification_time = OS::get_singleton()->get_ticks_usec();
}

bool Performance::has_custom_monitor(const StringName &p_id) {
	return _monitor_map.has(p_id);
}

Variant Performance::get_custom_monitor(const StringName &p_id) {
	ERR_FAIL_COND_V_MSG(!has_custom_monitor(p_id), Variant(), "Custom monitor with id '" + String(p_id) + "' doesn't exists.");
	bool error;
	String error_message;
	Variant return_value = _monitor_map[p_id].call(error, error_message);
	ERR_FAIL_COND_V_MSG(error, return_value, "Error calling from custom monitor '" + String(p_id) + "' to callable: " + error_message);
	return return_value;
}

TypedArray<StringName> Performance::get_custom_monitor_names() {
	if (!_monitor_map.size()) {
		return TypedArray<StringName>();
	}
	TypedArray<StringName> return_array;
	return_array.resize(_monitor_map.size());
	int index = 0;
	for (KeyValue<StringName, MonitorCall> i : _monitor_map) {
		return_array.set(index, i.key);
		index++;
	}
	return return_array;
}

uint64_t Performance::get_monitor_modification_time() {
	return _monitor_modification_time;
}

Performance::Performance() {
	_process_time = 0;
	_physics_process_time = 0;
	_navigation_process_time = 0;
	_monitor_modification_time = 0;
	singleton = this;
}

Performance::MonitorCall::MonitorCall(Callable p_callable, Vector<Variant> p_arguments) {
	_callable = p_callable;
	_arguments = p_arguments;
}

Performance::MonitorCall::MonitorCall() {
}

Variant Performance::MonitorCall::call(bool &r_error, String &r_error_message) {
	Vector<const Variant *> arguments_mem;
	arguments_mem.resize(_arguments.size());
	for (int i = 0; i < _arguments.size(); i++) {
		arguments_mem.write[i] = &_arguments[i];
	}
	const Variant **args = (const Variant **)arguments_mem.ptr();
	int argc = _arguments.size();
	Variant return_value;
	Callable::CallError error;
	_callable.callp(args, argc, return_value, error);
	r_error = (error.error != Callable::CallError::CALL_OK);
	if (r_error) {
		r_error_message = Variant::get_callable_error_text(_callable, args, argc, error);
	}
	return return_value;
}

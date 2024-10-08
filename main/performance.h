/**************************************************************************/
/*  performance.h                                                         */
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

#ifndef PERFORMANCE_H
#define PERFORMANCE_H

#include "core/object/class_db.h"
#include "core/templates/hash_map.h"

#define PERF_WARN_OFFLINE_FUNCTION
#define PERF_WARN_PROCESS_SYNC

template <typename T>
class TypedArray;

class Performance : public Object {
	GDCLASS(Performance, Object);

	static Performance *singleton;
	static void _bind_methods();

	int _get_node_count() const;

	double _process_time;
	double _physics_process_time;
	double _navigation_process_time;

	class MonitorCall {
		Callable _callable;
		Vector<Variant> _arguments;

	public:
		MonitorCall(Callable p_callable, Vector<Variant> p_arguments);
		MonitorCall();
		Variant call(bool &r_error, String &r_error_message);
	};

	HashMap<StringName, MonitorCall> _monitor_map;
	uint64_t _monitor_modification_time;

public:
	enum Monitor {
		TIME_FPS,
		TIME_PROCESS,
		TIME_PHYSICS_PROCESS,
		TIME_NAVIGATION_PROCESS,
		MEMORY_STATIC,
		MEMORY_STATIC_MAX,
		MEMORY_MESSAGE_BUFFER_MAX,
		OBJECT_COUNT,
		OBJECT_RESOURCE_COUNT,
		OBJECT_NODE_COUNT,
		OBJECT_ORPHAN_NODE_COUNT,
		RENDER_TOTAL_OBJECTS_IN_FRAME,
		RENDER_TOTAL_PRIMITIVES_IN_FRAME,
		RENDER_TOTAL_DRAW_CALLS_IN_FRAME,
		RENDER_VIDEO_MEM_USED,
		RENDER_TEXTURE_MEM_USED,
		RENDER_BUFFER_MEM_USED,
		PHYSICS_2D_ACTIVE_OBJECTS,
		PHYSICS_2D_COLLISION_PAIRS,
		PHYSICS_2D_ISLAND_COUNT,
		PHYSICS_3D_ACTIVE_OBJECTS,
		PHYSICS_3D_COLLISION_PAIRS,
		PHYSICS_3D_ISLAND_COUNT,
		AUDIO_OUTPUT_LATENCY,
		NAVIGATION_ACTIVE_MAPS,
		NAVIGATION_REGION_COUNT,
		NAVIGATION_AGENT_COUNT,
		NAVIGATION_LINK_COUNT,
		NAVIGATION_POLYGON_COUNT,
		NAVIGATION_EDGE_COUNT,
		NAVIGATION_EDGE_MERGE_COUNT,
		NAVIGATION_EDGE_CONNECTION_COUNT,
		NAVIGATION_EDGE_FREE_COUNT,
		NAVIGATION_OBSTACLE_COUNT,
		PIPELINE_COMPILATIONS_CANVAS,
		PIPELINE_COMPILATIONS_MESH,
		PIPELINE_COMPILATIONS_SURFACE,
		PIPELINE_COMPILATIONS_DRAW,
		PIPELINE_COMPILATIONS_SPECIALIZATION,
		MONITOR_MAX
	};

	enum MonitorType {
		MONITOR_TYPE_QUANTITY,
		MONITOR_TYPE_MEMORY,
		MONITOR_TYPE_TIME
	};

	double get_monitor(Monitor p_monitor) const;
	String get_monitor_name(Monitor p_monitor) const;

	MonitorType get_monitor_type(Monitor p_monitor) const;

	void set_process_time(double p_pt);
	void set_physics_process_time(double p_pt);
	void set_navigation_process_time(double p_pt);

	void add_custom_monitor(const StringName &p_id, const Callable &p_callable, const Vector<Variant> &p_args);
	void remove_custom_monitor(const StringName &p_id);
	bool has_custom_monitor(const StringName &p_id);
	Variant get_custom_monitor(const StringName &p_id);
	TypedArray<StringName> get_custom_monitor_names();

	uint64_t get_monitor_modification_time();

	static Performance *get_singleton() { return singleton; }

	Performance();
};

VARIANT_ENUM_CAST(Performance::Monitor);

#endif // PERFORMANCE_H

/**************************************************************************/
/*  servers_debugger.h                                                    */
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

#include "servers/rendering_server.h"

class ServersDebugger {
public:
	// Memory usage
	struct ResourceInfo {
		String path;
		String format;
		String type;
		RID id;
		int vram = 0;
		bool operator<(const ResourceInfo &p_img) const { return vram == p_img.vram ? id < p_img.id : vram > p_img.vram; }
	};

	struct ResourceUsage {
		List<ResourceInfo> infos;

		Array serialize();
		bool deserialize(const Array &p_arr);
	};

	// Script Profiler
	struct ScriptFunctionSignature {
		StringName name;
		int id = -1;

		Array serialize();
		bool deserialize(const Array &p_arr);
	};

	struct ScriptFunctionInfo {
		StringName name;
		int sig_id = -1;
		int call_count = 0;
		double self_time = 0;
		double total_time = 0;
		double internal_time = 0;
	};

	// Servers profiler
	struct ServerFunctionInfo {
		StringName name;
		double time = 0;
	};

	struct ServerInfo {
		StringName name;
		List<ServerFunctionInfo> functions;
	};

	struct ServersProfilerFrame {
		int frame_number = 0;
		double frame_time = 0;
		double process_time = 0;
		double physics_time = 0;
		double physics_frame_time = 0;
		double script_time = 0;
		List<ServerInfo> servers;
		Vector<ScriptFunctionInfo> script_functions;

		Array serialize();
		bool deserialize(const Array &p_arr);
	};

	// Visual Profiler
	struct VisualProfilerFrame {
		uint64_t frame_number = 0;
		Vector<RS::FrameProfileArea> areas;

		Array serialize();
		bool deserialize(const Array &p_arr);
	};

private:
	class ScriptsProfiler;
	class ServersProfiler;
	class VisualProfiler;

	double last_draw_time = 0.0;
	Ref<ServersProfiler> servers_profiler;
	Ref<VisualProfiler> visual_profiler;

	static ServersDebugger *singleton;

	static Error _capture(void *p_user, const String &p_cmd, const Array &p_data, bool &r_captured);

	void _send_resource_usage();

	ServersDebugger();

public:
	static void initialize();
	static void deinitialize();

	~ServersDebugger();
};

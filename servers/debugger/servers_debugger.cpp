/**************************************************************************/
/*  servers_debugger.cpp                                                  */
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

#include "servers_debugger.h"

#include "core/config/project_settings.h"
#include "core/debugger/engine_debugger.h"
#include "core/debugger/engine_profiler.h"
#include "core/object/script_language.h"
#include "servers/display_server.h"

#define CHECK_SIZE(arr, expected, what) ERR_FAIL_COND_V_MSG((uint32_t)arr.size() < (uint32_t)(expected), false, String("Malformed ") + what + " message from script debugger, message too short. Expected size: " + itos(expected) + ", actual size: " + itos(arr.size()))
#define CHECK_END(arr, expected, what) ERR_FAIL_COND_V_MSG((uint32_t)arr.size() > (uint32_t)expected, false, String("Malformed ") + what + " message from script debugger, message too long. Expected size: " + itos(expected) + ", actual size: " + itos(arr.size()))

Array ServersDebugger::ResourceUsage::serialize() {
	infos.sort();

	Array arr;
	arr.push_back(infos.size() * 4);
	for (const ResourceInfo &E : infos) {
		arr.push_back(E.path);
		arr.push_back(E.format);
		arr.push_back(E.type);
		arr.push_back(E.vram);
	}
	return arr;
}

bool ServersDebugger::ResourceUsage::deserialize(const Array &p_arr) {
	CHECK_SIZE(p_arr, 1, "ResourceUsage");
	uint32_t size = p_arr[0];
	ERR_FAIL_COND_V(size % 4, false);
	CHECK_SIZE(p_arr, 1 + size, "ResourceUsage");
	uint32_t idx = 1;
	while (idx < 1 + size) {
		ResourceInfo info;
		info.path = p_arr[idx];
		info.format = p_arr[idx + 1];
		info.type = p_arr[idx + 2];
		info.vram = p_arr[idx + 3];
		infos.push_back(info);
		idx += 4;
	}
	CHECK_END(p_arr, idx, "ResourceUsage");
	return true;
}

Array ServersDebugger::ScriptFunctionSignature::serialize() {
	Array arr;
	arr.push_back(name);
	arr.push_back(id);
	return arr;
}

bool ServersDebugger::ScriptFunctionSignature::deserialize(const Array &p_arr) {
	CHECK_SIZE(p_arr, 2, "ScriptFunctionSignature");
	name = p_arr[0];
	id = p_arr[1];
	CHECK_END(p_arr, 2, "ScriptFunctionSignature");
	return true;
}

Array ServersDebugger::ServersProfilerFrame::serialize() {
	Array arr;
	arr.push_back(frame_number);
	arr.push_back(frame_time);
	arr.push_back(process_time);
	arr.push_back(physics_time);
	arr.push_back(physics_frame_time);
	arr.push_back(script_time);

	arr.push_back(servers.size());
	for (const ServerInfo &s : servers) {
		arr.push_back(s.name);
		arr.push_back(s.functions.size() * 2);
		for (const ServerFunctionInfo &f : s.functions) {
			arr.push_back(f.name);
			arr.push_back(f.time);
		}
	}

	arr.push_back(script_functions.size() * 5);
	for (int i = 0; i < script_functions.size(); i++) {
		arr.push_back(script_functions[i].sig_id);
		arr.push_back(script_functions[i].call_count);
		arr.push_back(script_functions[i].self_time);
		arr.push_back(script_functions[i].total_time);
		arr.push_back(script_functions[i].internal_time);
	}
	return arr;
}

bool ServersDebugger::ServersProfilerFrame::deserialize(const Array &p_arr) {
	CHECK_SIZE(p_arr, 7, "ServersProfilerFrame");
	frame_number = p_arr[0];
	frame_time = p_arr[1];
	process_time = p_arr[2];
	physics_time = p_arr[3];
	physics_frame_time = p_arr[4];
	script_time = p_arr[5];
	int servers_size = p_arr[6];
	int idx = 7;
	while (servers_size) {
		CHECK_SIZE(p_arr, idx + 2, "ServersProfilerFrame");
		servers_size--;
		ServerInfo si;
		si.name = p_arr[idx];
		int sub_data_size = p_arr[idx + 1];
		idx += 2;
		CHECK_SIZE(p_arr, idx + sub_data_size, "ServersProfilerFrame");
		for (int j = 0; j < sub_data_size / 2; j++) {
			ServerFunctionInfo sf;
			sf.name = p_arr[idx];
			sf.time = p_arr[idx + 1];
			idx += 2;
			si.functions.push_back(sf);
		}
		servers.push_back(si);
	}
	CHECK_SIZE(p_arr, idx + 1, "ServersProfilerFrame");
	int func_size = p_arr[idx];
	idx += 1;
	CHECK_SIZE(p_arr, idx + func_size, "ServersProfilerFrame");
	for (int i = 0; i < func_size / 5; i++) {
		ScriptFunctionInfo fi;
		fi.sig_id = p_arr[idx];
		fi.call_count = p_arr[idx + 1];
		fi.self_time = p_arr[idx + 2];
		fi.total_time = p_arr[idx + 3];
		fi.internal_time = p_arr[idx + 4];
		script_functions.push_back(fi);
		idx += 5;
	}
	CHECK_END(p_arr, idx, "ServersProfilerFrame");
	return true;
}

Array ServersDebugger::VisualProfilerFrame::serialize() {
	Array arr;
	arr.push_back(frame_number);
	arr.push_back(areas.size() * 3);
	for (int i = 0; i < areas.size(); i++) {
		arr.push_back(areas[i].name);
		arr.push_back(areas[i].cpu_msec);
		arr.push_back(areas[i].gpu_msec);
	}
	return arr;
}

bool ServersDebugger::VisualProfilerFrame::deserialize(const Array &p_arr) {
	CHECK_SIZE(p_arr, 2, "VisualProfilerFrame");
	frame_number = p_arr[0];
	int size = p_arr[1];
	CHECK_SIZE(p_arr, size, "VisualProfilerFrame");
	int idx = 2;
	areas.resize(size / 3);
	RS::FrameProfileArea *w = areas.ptrw();
	for (int i = 0; i < size / 3; i++) {
		w[i].name = p_arr[idx];
		w[i].cpu_msec = p_arr[idx + 1];
		w[i].gpu_msec = p_arr[idx + 2];
		idx += 3;
	}
	CHECK_END(p_arr, idx, "VisualProfilerFrame");
	return true;
}
class ServersDebugger::ScriptsProfiler : public EngineProfiler {
	typedef ServersDebugger::ScriptFunctionSignature FunctionSignature;
	typedef ServersDebugger::ScriptFunctionInfo FunctionInfo;
	struct ProfileInfoSort {
		bool operator()(ScriptLanguage::ProfilingInfo *A, ScriptLanguage::ProfilingInfo *B) const {
			return A->total_time > B->total_time;
		}
	};
	Vector<ScriptLanguage::ProfilingInfo> info;
	Vector<ScriptLanguage::ProfilingInfo *> ptrs;
	HashMap<StringName, int> sig_map;
	int max_frame_functions = 16;

public:
	void toggle(bool p_enable, const Array &p_opts) {
		if (p_enable) {
			sig_map.clear();
			for (int i = 0; i < ScriptServer::get_language_count(); i++) {
				ScriptServer::get_language(i)->profiling_start();
				if (p_opts.size() == 2 && p_opts[1].get_type() == Variant::BOOL) {
					ScriptServer::get_language(i)->profiling_set_save_native_calls(p_opts[1]);
				}
			}
			if (p_opts.size() > 0 && p_opts[0].get_type() == Variant::INT) {
				max_frame_functions = MAX(0, int(p_opts[0]));
			}
		} else {
			for (int i = 0; i < ScriptServer::get_language_count(); i++) {
				ScriptServer::get_language(i)->profiling_stop();
			}
		}
	}

	void write_frame_data(Vector<FunctionInfo> &r_funcs, uint64_t &r_total, bool p_accumulated) {
		int ofs = 0;
		for (int i = 0; i < ScriptServer::get_language_count(); i++) {
			if (p_accumulated) {
				ofs += ScriptServer::get_language(i)->profiling_get_accumulated_data(&info.write[ofs], info.size() - ofs);
			} else {
				ofs += ScriptServer::get_language(i)->profiling_get_frame_data(&info.write[ofs], info.size() - ofs);
			}
		}

		for (int i = 0; i < ofs; i++) {
			ptrs.write[i] = &info.write[i];
		}

		SortArray<ScriptLanguage::ProfilingInfo *, ProfileInfoSort> sa;
		sa.sort(ptrs.ptrw(), ofs);

		int to_send = MIN(ofs, max_frame_functions);

		// Check signatures first, and compute total time.
		r_total = 0;
		for (int i = 0; i < to_send; i++) {
			if (!sig_map.has(ptrs[i]->signature)) {
				int idx = sig_map.size();
				FunctionSignature sig;
				sig.name = ptrs[i]->signature;
				sig.id = idx;
				EngineDebugger::get_singleton()->send_message("servers:function_signature", sig.serialize());
				sig_map[ptrs[i]->signature] = idx;
			}
			r_total += ptrs[i]->self_time;
		}

		// Send frame, script time, functions information then
		r_funcs.resize(to_send);

		FunctionInfo *w = r_funcs.ptrw();
		for (int i = 0; i < to_send; i++) {
			if (sig_map.has(ptrs[i]->signature)) {
				w[i].sig_id = sig_map[ptrs[i]->signature];
			}
			w[i].call_count = ptrs[i]->call_count;
			w[i].total_time = ptrs[i]->total_time / 1000000.0;
			w[i].self_time = ptrs[i]->self_time / 1000000.0;
			w[i].internal_time = ptrs[i]->internal_time / 1000000.0;
		}
	}

	ScriptsProfiler() {
		info.resize(GLOBAL_GET("debug/settings/profiler/max_functions"));
		ptrs.resize(info.size());
	}
};

class ServersDebugger::ServersProfiler : public EngineProfiler {
	bool skip_profile_frame = false;
	typedef ServersDebugger::ServerInfo ServerInfo;
	typedef ServersDebugger::ServerFunctionInfo ServerFunctionInfo;

	HashMap<StringName, ServerInfo> server_data;
	ScriptsProfiler scripts_profiler;

	double frame_time = 0;
	double process_time = 0;
	double physics_time = 0;
	double physics_frame_time = 0;

	void _send_frame_data(bool p_final) {
		ServersDebugger::ServersProfilerFrame frame;
		frame.frame_number = Engine::get_singleton()->get_process_frames();
		frame.frame_time = frame_time;
		frame.process_time = process_time;
		frame.physics_time = physics_time;
		frame.physics_frame_time = physics_frame_time;
		HashMap<StringName, ServerInfo>::Iterator E = server_data.begin();
		while (E) {
			if (!p_final) {
				frame.servers.push_back(E->value);
			}
			E->value.functions.clear();
			++E;
		}
		uint64_t time = 0;
		scripts_profiler.write_frame_data(frame.script_functions, time, p_final);
		frame.script_time = USEC_TO_SEC(time);
		if (skip_profile_frame) {
			skip_profile_frame = false;
			return;
		}
		if (p_final) {
			EngineDebugger::get_singleton()->send_message("servers:profile_total", frame.serialize());
		} else {
			EngineDebugger::get_singleton()->send_message("servers:profile_frame", frame.serialize());
		}
	}

public:
	void toggle(bool p_enable, const Array &p_opts) {
		skip_profile_frame = false;
		if (p_enable) {
			server_data.clear(); // Clear old profiling data.
		} else {
			_send_frame_data(true); // Send final frame.
		}
		scripts_profiler.toggle(p_enable, p_opts);
	}

	void add(const Array &p_data) {
		String name = p_data[0];
		if (!server_data.has(name)) {
			ServerInfo info;
			info.name = name;
			server_data[name] = info;
		}
		ServerInfo &srv = server_data[name];

		for (int idx = 1; idx < p_data.size() - 1; idx += 2) {
			ServerFunctionInfo fi;
			fi.name = p_data[idx];
			fi.time = p_data[idx + 1];
			srv.functions.push_back(fi);
		}
	}

	void tick(double p_frame_time, double p_process_time, double p_physics_time, double p_physics_frame_time) {
		frame_time = p_frame_time;
		process_time = p_process_time;
		physics_time = p_physics_time;
		physics_frame_time = p_physics_frame_time;
		_send_frame_data(false);
	}

	void skip_frame() {
		skip_profile_frame = true;
	}
};

class ServersDebugger::VisualProfiler : public EngineProfiler {
	typedef ServersDebugger::ServerInfo ServerInfo;
	typedef ServersDebugger::ServerFunctionInfo ServerFunctionInfo;

	HashMap<StringName, ServerInfo> server_data;

public:
	void toggle(bool p_enable, const Array &p_opts) {
		RS::get_singleton()->set_frame_profiling_enabled(p_enable);

		// Send hardware information from the remote device so that it's accurate for remote debugging.
		Array hardware_info;
		hardware_info.push_back(OS::get_singleton()->get_processor_name());
		hardware_info.push_back(RenderingServer::get_singleton()->get_video_adapter_name());
		EngineDebugger::get_singleton()->send_message("visual:hardware_info", hardware_info);
	}

	void add(const Array &p_data) {}

	void tick(double p_frame_time, double p_process_time, double p_physics_time, double p_physics_frame_time) {
		Vector<RS::FrameProfileArea> profile_areas = RS::get_singleton()->get_frame_profile();
		ServersDebugger::VisualProfilerFrame frame;
		if (!profile_areas.size()) {
			return;
		}

		frame.frame_number = RS::get_singleton()->get_frame_profile_frame();
		frame.areas.append_array(profile_areas);
		EngineDebugger::get_singleton()->send_message("visual:profile_frame", frame.serialize());
	}
};

ServersDebugger *ServersDebugger::singleton = nullptr;

void ServersDebugger::initialize() {
	if (EngineDebugger::is_active()) {
		memnew(ServersDebugger);
	}
}

void ServersDebugger::deinitialize() {
	if (singleton) {
		memdelete(singleton);
	}
}

Error ServersDebugger::_capture(void *p_user, const String &p_cmd, const Array &p_data, bool &r_captured) {
	ERR_FAIL_NULL_V(singleton, ERR_BUG);
	r_captured = true;
	if (p_cmd == "memory") {
		singleton->_send_resource_usage();
	} else if (p_cmd == "draw") { // Forced redraw.
		// For camera override to stay live when the game is paused from the editor.
		double delta = 0.0;
		if (singleton->last_draw_time) {
			delta = (OS::get_singleton()->get_ticks_usec() - singleton->last_draw_time) / 1000000.0;
		}
		singleton->last_draw_time = OS::get_singleton()->get_ticks_usec();
		RenderingServer::get_singleton()->sync();
		if (RenderingServer::get_singleton()->has_changed()) {
			RenderingServer::get_singleton()->draw(true, delta);
		}
		EngineDebugger::get_singleton()->send_message("servers:drawn", Array());
	} else if (p_cmd == "foreground") {
		singleton->last_draw_time = 0.0;
		DisplayServer::get_singleton()->window_move_to_foreground();
		singleton->servers_profiler->skip_frame();
	} else {
		r_captured = false;
	}
	return OK;
}

void ServersDebugger::_send_resource_usage() {
	ServersDebugger::ResourceUsage usage;

	List<RS::TextureInfo> tinfo;
	RS::get_singleton()->texture_debug_usage(&tinfo);

	for (const RS::TextureInfo &E : tinfo) {
		ServersDebugger::ResourceInfo info;
		info.path = E.path;
		info.vram = E.bytes;
		info.id = E.texture;
		info.type = "Texture";
		if (E.depth == 0) {
			info.format = itos(E.width) + "x" + itos(E.height) + " " + Image::get_format_name(E.format);
		} else {
			info.format = itos(E.width) + "x" + itos(E.height) + "x" + itos(E.depth) + " " + Image::get_format_name(E.format);
		}
		usage.infos.push_back(info);
	}

	EngineDebugger::get_singleton()->send_message("servers:memory_usage", usage.serialize());
}

ServersDebugger::ServersDebugger() {
	singleton = this;

	// Generic servers profiler (audio/physics/...)
	servers_profiler.instantiate();
	servers_profiler->bind("servers");

	// Visual Profiler (cpu/gpu times)
	visual_profiler.instantiate();
	visual_profiler->bind("visual");

	EngineDebugger::Capture servers_cap(nullptr, &_capture);
	EngineDebugger::register_message_capture("servers", servers_cap);
}

ServersDebugger::~ServersDebugger() {
	EngineDebugger::unregister_message_capture("servers");
	singleton = nullptr;
}

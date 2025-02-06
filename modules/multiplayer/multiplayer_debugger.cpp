/**************************************************************************/
/*  multiplayer_debugger.cpp                                              */
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

#include "multiplayer_debugger.h"

#include "multiplayer_synchronizer.h"
#include "scene_replication_config.h"

#include "core/debugger/engine_debugger.h"
#include "scene/main/node.h"

List<Ref<EngineProfiler>> multiplayer_profilers;

void MultiplayerDebugger::initialize() {
	Ref<BandwidthProfiler> bandwidth;
	bandwidth.instantiate();
	bandwidth->bind("multiplayer:bandwidth");
	multiplayer_profilers.push_back(bandwidth);

	Ref<RPCProfiler> rpc_profiler;
	rpc_profiler.instantiate();
	rpc_profiler->bind("multiplayer:rpc");
	multiplayer_profilers.push_back(rpc_profiler);

	Ref<ReplicationProfiler> replication_profiler;
	replication_profiler.instantiate();
	replication_profiler->bind("multiplayer:replication");
	multiplayer_profilers.push_back(replication_profiler);

	EngineDebugger::register_message_capture("multiplayer", EngineDebugger::Capture(nullptr, &_capture));
}

void MultiplayerDebugger::deinitialize() {
	multiplayer_profilers.clear();
}

Error MultiplayerDebugger::_capture(void *p_user, const String &p_msg, const Array &p_args, bool &r_captured) {
	if (p_msg == "cache") {
		Array out;
		for (int i = 0; i < p_args.size(); i++) {
			ObjectID id = p_args[i].operator ObjectID();
			Object *obj = ObjectDB::get_instance(id);
			ERR_CONTINUE(!obj);
			if (Object::cast_to<SceneReplicationConfig>(obj)) {
				out.push_back(id);
				out.push_back(obj->get_class());
				out.push_back(((SceneReplicationConfig *)obj)->get_path());
			} else if (Object::cast_to<Node>(obj)) {
				out.push_back(id);
				out.push_back(obj->get_class());
				out.push_back(String(((Node *)obj)->get_path()));
			} else {
				ERR_FAIL_V(FAILED);
			}
		}
		EngineDebugger::get_singleton()->send_message("multiplayer:cache", out);
		return OK;
	}
	ERR_FAIL_V(FAILED);
}

// BandwidthProfiler

int MultiplayerDebugger::BandwidthProfiler::bandwidth_usage(const Vector<BandwidthFrame> &p_buffer, int p_pointer) {
	ERR_FAIL_COND_V(p_buffer.is_empty(), 0);
	int total_bandwidth = 0;

	uint64_t timestamp = OS::get_singleton()->get_ticks_msec();
	uint64_t final_timestamp = timestamp - 1000;

	int i = (p_pointer + p_buffer.size() - 1) % p_buffer.size();

	while (i != p_pointer && p_buffer[i].packet_size > 0) {
		if (p_buffer[i].timestamp < final_timestamp) {
			return total_bandwidth;
		}
		total_bandwidth += p_buffer[i].packet_size;
		i = (i + p_buffer.size() - 1) % p_buffer.size();
	}

	ERR_FAIL_COND_V_MSG(i == p_pointer, total_bandwidth, "Reached the end of the bandwidth profiler buffer, values might be inaccurate.");
	return total_bandwidth;
}

void MultiplayerDebugger::BandwidthProfiler::toggle(bool p_enable, const Array &p_opts) {
	if (!p_enable) {
		bandwidth_in.clear();
		bandwidth_out.clear();
	} else {
		bandwidth_in_ptr = 0;
		bandwidth_in.resize(16384); // ~128kB
		for (int i = 0; i < bandwidth_in.size(); ++i) {
			bandwidth_in.write[i].packet_size = -1;
		}
		bandwidth_out_ptr = 0;
		bandwidth_out.resize(16384); // ~128kB
		for (int i = 0; i < bandwidth_out.size(); ++i) {
			bandwidth_out.write[i].packet_size = -1;
		}
	}
}

void MultiplayerDebugger::BandwidthProfiler::add(const Array &p_data) {
	ERR_FAIL_COND(p_data.size() < 3);
	const String inout = p_data[0];
	int time = p_data[1];
	int size = p_data[2];
	if (inout == "in") {
		bandwidth_in.write[bandwidth_in_ptr].timestamp = time;
		bandwidth_in.write[bandwidth_in_ptr].packet_size = size;
		bandwidth_in_ptr = (bandwidth_in_ptr + 1) % bandwidth_in.size();
	} else if (inout == "out") {
		bandwidth_out.write[bandwidth_out_ptr].timestamp = time;
		bandwidth_out.write[bandwidth_out_ptr].packet_size = size;
		bandwidth_out_ptr = (bandwidth_out_ptr + 1) % bandwidth_out.size();
	}
}

void MultiplayerDebugger::BandwidthProfiler::tick(double p_frame_time, double p_process_time, double p_physics_time, double p_physics_frame_time) {
	uint64_t pt = OS::get_singleton()->get_ticks_msec();
	if (pt - last_bandwidth_time > 200) {
		last_bandwidth_time = pt;
		int incoming_bandwidth = bandwidth_usage(bandwidth_in, bandwidth_in_ptr);
		int outgoing_bandwidth = bandwidth_usage(bandwidth_out, bandwidth_out_ptr);

		Array arr;
		arr.push_back(incoming_bandwidth);
		arr.push_back(outgoing_bandwidth);
		EngineDebugger::get_singleton()->send_message("multiplayer:bandwidth", arr);
	}
}

// RPCProfiler

Array MultiplayerDebugger::RPCFrame::serialize() {
	Array arr;
	arr.push_back(infos.size() * 6);
	for (int i = 0; i < infos.size(); ++i) {
		arr.push_back(uint64_t(infos[i].node));
		arr.push_back(infos[i].node_path);
		arr.push_back(infos[i].incoming_rpc);
		arr.push_back(infos[i].incoming_size);
		arr.push_back(infos[i].outgoing_rpc);
		arr.push_back(infos[i].outgoing_size);
	}
	return arr;
}

bool MultiplayerDebugger::RPCFrame::deserialize(const Array &p_arr) {
	ERR_FAIL_COND_V(p_arr.is_empty(), false);
	uint32_t size = p_arr[0];
	ERR_FAIL_COND_V(size % 6, false);
	ERR_FAIL_COND_V((uint32_t)p_arr.size() != size + 1, false);
	infos.resize(size / 6);
	int idx = 1;
	for (uint32_t i = 0; i < size / 6; i++) {
		infos.write[i].node = uint64_t(p_arr[idx]);
		infos.write[i].node_path = p_arr[idx + 1];
		infos.write[i].incoming_rpc = p_arr[idx + 2];
		infos.write[i].incoming_size = p_arr[idx + 3];
		infos.write[i].outgoing_rpc = p_arr[idx + 4];
		infos.write[i].outgoing_size = p_arr[idx + 5];
		idx += 6;
	}
	return true;
}

void MultiplayerDebugger::RPCProfiler::init_node(const ObjectID p_node) {
	if (rpc_node_data.has(p_node)) {
		return;
	}
	rpc_node_data.insert(p_node, RPCNodeInfo());
	rpc_node_data[p_node].node = p_node;
	rpc_node_data[p_node].node_path = p_node.get_object<Node>()->get_path();
}

void MultiplayerDebugger::RPCProfiler::toggle(bool p_enable, const Array &p_opts) {
	rpc_node_data.clear();
}

void MultiplayerDebugger::RPCProfiler::add(const Array &p_data) {
	ERR_FAIL_COND(p_data.size() != 3);
	const String what = p_data[0];
	const ObjectID id = p_data[1];
	const int size = p_data[2];
	init_node(id);
	RPCNodeInfo &info = rpc_node_data[id];
	if (what == "rpc_in") {
		info.incoming_rpc++;
		info.incoming_size += size;
	} else if (what == "rpc_out") {
		info.outgoing_rpc++;
		info.outgoing_size += size;
	}
}

void MultiplayerDebugger::RPCProfiler::tick(double p_frame_time, double p_process_time, double p_physics_time, double p_physics_frame_time) {
	uint64_t pt = OS::get_singleton()->get_ticks_msec();
	if (pt - last_profile_time > 100) {
		last_profile_time = pt;
		RPCFrame frame;
		for (const KeyValue<ObjectID, RPCNodeInfo> &E : rpc_node_data) {
			frame.infos.push_back(E.value);
		}
		rpc_node_data.clear();
		EngineDebugger::get_singleton()->send_message("multiplayer:rpc", frame.serialize());
	}
}

// ReplicationProfiler

MultiplayerDebugger::SyncInfo::SyncInfo(MultiplayerSynchronizer *p_sync) {
	ERR_FAIL_NULL(p_sync);
	synchronizer = p_sync->get_instance_id();
	if (p_sync->get_replication_config_ptr()) {
		config = p_sync->get_replication_config_ptr()->get_instance_id();
	}
	if (p_sync->get_root_node()) {
		root_node = p_sync->get_root_node()->get_instance_id();
	}
}

void MultiplayerDebugger::SyncInfo::write_to_array(Array &r_arr) const {
	r_arr.push_back(synchronizer);
	r_arr.push_back(config);
	r_arr.push_back(root_node);
	r_arr.push_back(incoming_syncs);
	r_arr.push_back(incoming_size);
	r_arr.push_back(outgoing_syncs);
	r_arr.push_back(outgoing_size);
}

bool MultiplayerDebugger::SyncInfo::read_from_array(const Array &p_arr, int p_offset) {
	ERR_FAIL_COND_V(p_arr.size() - p_offset < 7, false);
	synchronizer = int64_t(p_arr[p_offset]);
	config = int64_t(p_arr[p_offset + 1]);
	root_node = int64_t(p_arr[p_offset + 2]);
	incoming_syncs = p_arr[p_offset + 3];
	incoming_size = p_arr[p_offset + 4];
	outgoing_syncs = p_arr[p_offset + 5];
	outgoing_size = p_arr[p_offset + 6];
	return true;
}

Array MultiplayerDebugger::ReplicationFrame::serialize() {
	Array arr;
	arr.push_back(infos.size() * 7);
	for (const KeyValue<ObjectID, SyncInfo> &E : infos) {
		E.value.write_to_array(arr);
	}
	return arr;
}

bool MultiplayerDebugger::ReplicationFrame::deserialize(const Array &p_arr) {
	ERR_FAIL_COND_V(p_arr.is_empty(), false);
	uint32_t size = p_arr[0];
	ERR_FAIL_COND_V(size % 7, false);
	ERR_FAIL_COND_V((uint32_t)p_arr.size() != size + 1, false);
	int idx = 1;
	for (uint32_t i = 0; i < size / 7; i++) {
		SyncInfo info;
		if (!info.read_from_array(p_arr, idx)) {
			return false;
		}
		infos[info.synchronizer] = info;
		idx += 7;
	}
	return true;
}

void MultiplayerDebugger::ReplicationProfiler::toggle(bool p_enable, const Array &p_opts) {
	sync_data.clear();
}

void MultiplayerDebugger::ReplicationProfiler::add(const Array &p_data) {
	ERR_FAIL_COND(p_data.size() != 3);
	const String what = p_data[0];
	const ObjectID id = p_data[1];
	const uint64_t size = p_data[2];
	MultiplayerSynchronizer *sync = id.get_object<MultiplayerSynchronizer>();
	ERR_FAIL_NULL(sync);
	if (!sync_data.has(id)) {
		sync_data[id] = SyncInfo(sync);
	}
	SyncInfo &info = sync_data[id];
	if (what == "sync_in") {
		info.incoming_syncs++;
		info.incoming_size += size;
	} else if (what == "sync_out") {
		info.outgoing_syncs++;
		info.outgoing_size += size;
	}
}

void MultiplayerDebugger::ReplicationProfiler::tick(double p_frame_time, double p_process_time, double p_physics_time, double p_physics_frame_time) {
	uint64_t pt = OS::get_singleton()->get_ticks_msec();
	if (pt - last_profile_time > 100) {
		last_profile_time = pt;
		ReplicationFrame frame;
		for (const KeyValue<ObjectID, SyncInfo> &E : sync_data) {
			frame.infos[E.key] = E.value;
		}
		sync_data.clear();
		EngineDebugger::get_singleton()->send_message("multiplayer:syncs", frame.serialize());
	}
}

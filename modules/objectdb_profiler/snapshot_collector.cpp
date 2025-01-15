/**************************************************************************/
/*  snapshot_collector.cpp                                                */
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

#include "snapshot_collector.h"

#include "core/core_bind.h"
#include "core/debugger/engine_debugger.h"
#include "core/os/time.h"
#include "core/version.h"
#include "scene/main/window.h"

HashMap<int, Vector<uint8_t>> SnapshotCollector::pending_snapshots;

void SnapshotCollector::initialize() {
	pending_snapshots.clear();
	EngineDebugger::register_message_capture("snapshot", EngineDebugger::Capture(nullptr, SnapshotCollector::parse_message));
}

void SnapshotCollector::deinitialize() {
	EngineDebugger::unregister_message_capture("snapshot");
	pending_snapshots.clear();
}

void SnapshotCollector::snapshot_objects(Array *p_arr, Dictionary &p_snapshot_context) {
	print_verbose("Starting to snapshot");
	p_arr->clear();

	// Gather all ObjectIDs first. The ObjectDB will be locked in debug_objects, so we can't serialize until it exits.
	List<ObjectID> debugger_object_ids;
	ObjectDB::debug_objects([](Object *p_obj, void *p_user_data) {
		List<ObjectID> *debugger_object_ids_ptr = (List<ObjectID> *)p_user_data;
		debugger_object_ids_ptr->push_back(p_obj->get_instance_id());
	},
			(void *)&debugger_object_ids);

	// Get SnapshotDataTransportObject from ObjectID list now that DB is unlocked.
	List<SnapshotDataTransportObject> debugger_objects;
	for (ObjectID oid : debugger_object_ids) {
		Object *obj = ObjectDB::get_instance(oid);
		// This is the same way objects in the remote scene tree are seialized,
		// but here we add a few extra properties via the extra_debug_data dictionary.
		SnapshotDataTransportObject debug_data(obj);

		// If we're RefCounted, send over our RefCount too. Could add code here to add a few other interesting properties.
		if (ClassDB::is_parent_class(obj->get_class_name(), RefCounted::get_class_static())) {
			RefCounted *ref = (RefCounted *)obj;
			debug_data.extra_debug_data["ref_count"] = ref->get_reference_count();
		}

		if (ClassDB::is_parent_class(obj->get_class_name(), Node::get_class_static())) {
			Node *node = (Node *)obj;
			debug_data.extra_debug_data["node_name"] = node->get_name();
			if (node->get_parent() != nullptr) {
				debug_data.extra_debug_data["node_parent"] = node->get_parent()->get_instance_id();
			}

			debug_data.extra_debug_data["node_is_scene_root"] = SceneTree::get_singleton()->get_root() == node;

			Array children;
			for (int i = 0; i < node->get_child_count(); i++) {
				children.push_back(node->get_child(i)->get_instance_id());
			}
			debug_data.extra_debug_data["node_children"] = children;
		}

		debugger_objects.push_back(debug_data);
	}

	// Add a header to the snapshot with general data about the state of the game, not tied to any particular object.
	p_snapshot_context["mem_available"] = Memory::get_mem_available();
	p_snapshot_context["mem_usage"] = Memory::get_mem_usage();
	p_snapshot_context["mem_max_usage"] = Memory::get_mem_max_usage();
	p_snapshot_context["timestamp"] = Time::get_singleton()->get_unix_time_from_system();
	p_snapshot_context["game_version"] = get_godot_version_string();
	p_arr->push_back(p_snapshot_context);
	for (SnapshotDataTransportObject &debug_data : debugger_objects) {
		debug_data.serialize(*p_arr);
		p_arr->push_back(debug_data.extra_debug_data);
	}

	print_verbose("snapshot size: " + String::num_uint64(p_arr->size()));
}

Error SnapshotCollector::parse_message(void *p_user, const String &p_msg, const Array &p_args, bool &r_captured) {
	r_captured = true;
	if (p_msg == "request_prepare_snapshot") {
		int request_id = (int)p_args.get(0);
		Dictionary snapshot_context;
		snapshot_context["editor_version"] = (String)p_args.get(1);
		Array objects;
		SnapshotCollector::snapshot_objects(&objects, snapshot_context);
		// Debugger networking has a limit on both how many objects can be queued to send and how
		// many bytes can be queued to send. Serializing to a string means we never hit the object
		// limit, and only have to deal with the byte limit.
		// Compress the snapshot in the game client to make sending the snapshot from game to editor a little faster.
		core_bind::Marshalls *m = core_bind::Marshalls::get_singleton();
		Vector<uint8_t> objs_buffer = m->base64_to_raw(m->variant_to_base64(objects));
		Vector<uint8_t> objs_buffer_compressed;
		objs_buffer_compressed.resize(objs_buffer.size());
		int new_size = Compression::compress(objs_buffer_compressed.ptrw(), objs_buffer.ptrw(), objs_buffer.size(), Compression::MODE_DEFLATE);
		objs_buffer_compressed.resize(new_size);
		pending_snapshots[request_id] = objs_buffer_compressed;

		// Tell the editor how long the snapshot is.
		Array resp;
		resp.push_back(request_id);
		resp.push_back(pending_snapshots[request_id].size());
		EngineDebugger::get_singleton()->send_message("snapshot:snapshot_prepared", resp);

	} else if (p_msg == "request_snapshot_chunk") {
		int request_id = (int)p_args.get(0);
		int begin = (int)p_args.get(1);
		int end = (int)p_args.get(2);

		Array resp;
		resp.push_back(request_id);
		resp.push_back(pending_snapshots[request_id].slice(begin, end));
		EngineDebugger::get_singleton()->send_message("snapshot:snapshot_chunk", resp);

		// If we sent the last part of the string, delete it locally.
		if (end >= pending_snapshots[request_id].size()) {
			pending_snapshots.erase(request_id);
		}
	} else {
		r_captured = false;
	}
	return OK;
}

String SnapshotCollector::get_godot_version_string() {
	String hash = String(VERSION_HASH);
	if (hash.length() != 0) {
		hash = " " + vformat("[%s]", hash.left(9));
	}
	return "v" VERSION_FULL_BUILD + hash;
}

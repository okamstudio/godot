/**************************************************************************/
/*  snapshot_collector.h                                                  */
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

#ifndef SNAPSHOT_COLLECTOR_H
#define SNAPSHOT_COLLECTOR_H

#include "scene/debugger/scene_debugger.h"

struct SnapshotDataTransportObject : public SceneDebuggerObject {
	SnapshotDataTransportObject() :
			SceneDebuggerObject() {}
	SnapshotDataTransportObject(Object *p_obj) :
			SceneDebuggerObject(p_obj) {}

	Dictionary extra_debug_data;
};

class SnapshotCollector {
public:
	static HashMap<int, Vector<uint8_t>> pending_snapshots;
	static void snapshot_objects(Array *p_arr, Dictionary &p_snapshot_context);
	static Error parse_message(void *p_user, const String &p_msg, const Array &p_args, bool &r_captured);
	static void initialize();
	static void deinitialize();
	static String get_godot_version_string();
};

#endif // SNAPSHOT_COLLECTOR_H

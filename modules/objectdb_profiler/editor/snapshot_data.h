/**************************************************************************/
/*  snapshot_data.h                                                       */
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

#ifndef SNAPSHOT_DATA_H
#define SNAPSHOT_DATA_H

#include "editor/debugger/editor_debugger_inspector.h"

class GameStateSnapshot;
class GameStateSnapshotRef;

class SnapshotDataObject : public EditorDebuggerRemoteObject {
	GDCLASS(SnapshotDataObject, EditorDebuggerRemoteObject);

	HashSet<ObjectID> _unique_references(const HashMap<String, ObjectID> &p_refs);
	String _get_script_name(Ref<Script> p_script);

public:
	GameStateSnapshot *snapshot = nullptr;
	Dictionary extra_debug_data;
	HashMap<String, ObjectID> outbound_references;
	HashMap<String, ObjectID> inbound_references;

	HashSet<ObjectID> get_unique_outbound_refernces();
	HashSet<ObjectID> get_unique_inbound_references();

	SnapshotDataObject(SceneDebuggerObject &p_obj, GameStateSnapshot *p_snapshot) :
			EditorDebuggerRemoteObject(p_obj), snapshot(p_snapshot) {}

	String get_name();
	String get_node_path();
	bool is_refcounted();
	bool is_node();
	bool is_class(const String &p_base_class);
};

class GameStateSnapshot : public Object {
	GDCLASS(GameStateSnapshot, Object);

	void _get_outbound_references(Variant &p_var, HashMap<String, ObjectID> &r_ret_val, const String &p_current_path = "");
	void _get_rc_cycles(SnapshotDataObject *p_obj, SnapshotDataObject *p_source_obj, HashSet<SnapshotDataObject *> p_traversed_objs, List<String> &r_ret_val, const String &p_current_path = "");

public:
	String name;
	HashMap<ObjectID, SnapshotDataObject *> objects;
	Dictionary snapshot_context;

	// Ideally, this would extend EditorDebuggerRemoteObject and be refcounted, but we can't have it both ways.
	// So, instead we have this static 'constructor' that returns a RefCounted wrapper around a GameStateSnapshot.
	static Ref<GameStateSnapshotRef> create_ref(const String &p_snapshot_name, const Vector<uint8_t> &p_snapshot_buffer);
	~GameStateSnapshot();

	void recompute_references();
};

// Thin RefCounted wrapper around a GameStateSnapshot.
class GameStateSnapshotRef : public RefCounted {
	GDCLASS(GameStateSnapshotRef, RefCounted);

	GameStateSnapshot *gamestate_snapshot = nullptr;

public:
	GameStateSnapshotRef(GameStateSnapshot *p_gss) :
			gamestate_snapshot(p_gss) {}

	bool unreference();
	GameStateSnapshot *get_snapshot();
};

#endif // SNAPSHOT_DATA_H

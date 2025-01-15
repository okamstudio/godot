/**************************************************************************/
/*  objectdb_profiler_panel.h                                             */
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

#ifndef OBJECTDB_PROFILER_PANEL_H
#define OBJECTDB_PROFILER_PANEL_H

#include "core/io/dir_access.h"
#include "core/templates/lru.h"
#include "data_viewers/snapshot_view.h"
#include "snapshot_data.h"

class TabContainer;
class Tree;

const int SNAPSHOT_CACHE_MAX_SIZE = 10;

// UI loaded by the debugger.
class ObjectDBProfilerPanel : public Control {
	GDCLASS(ObjectDBProfilerPanel, Control);

protected:
	enum OdbProfilerMenuOptions {
		ODB_MENU_RENAME,
		ODB_MENU_SHOW_IN_FOLDER,
		ODB_MENU_DELETE,
	};

	struct PartialSnapshot {
		int total_size;
		Vector<uint8_t> data;
	};

	int next_request_id = 0;
	bool awaiting_debug_break = false;
	bool requested_break_for_snapshot = false;

	Tree *snapshot_list = nullptr;
	Button *take_snapshot = nullptr;
	TabContainer *view_tabs = nullptr;
	PopupMenu *rmb_menu = nullptr;
	OptionButton *diff_button = nullptr;
	HashMap<int, String> diff_options;
	HashMap<int, PartialSnapshot> partial_snapshots;

	List<SnapshotView *> views;
	Ref<GameStateSnapshotRef> current_snapshot;
	Ref<GameStateSnapshotRef> diff_snapshot;
	LRUCache<String, Ref<GameStateSnapshotRef>> snapshot_cache;

	void _request_object_snapshot();
	void _begin_object_snapshot();
	void _on_debug_breaked(bool p_reallydid, bool p_can_debug, const String &p_reason, bool p_has_stackdump);
	void _show_selected_snapshot();
	Ref<DirAccess> _get_and_create_snapshot_storage_dir();
	TreeItem *_add_snapshot_button(const String &p_snapshot_file_name, const String &p_full_file_path);
	void _snapshot_rmb(const Vector2 &p_pos, MouseButton p_button);
	void _rmb_menu_pressed(int p_tool, bool p_confirm_override);
	void _apply_diff(int p_item_idx);
	void _update_diff_items();
	void _update_enabled_diff_items();
	void _edit_snapshot_name();
	void _view_tab_changed(int p_tab_idx);
	String _to_mb(int p_x);

public:
	ObjectDBProfilerPanel();

	void receive_snapshot(int p_request_id);
	void show_snapshot(const String &p_snapshot_file_name, const String &p_snapshot_diff_file_name);
	void clear_snapshot();
	Ref<GameStateSnapshotRef> get_snapshot(const String &p_snapshot_file_name);
	void set_enabled(bool p_enabled);
	void add_view(SnapshotView *p_to_add);

	bool handle_debug_message(const String &p_message, const Array &p_data, int p_index);
};

#endif // OBJECTDB_PROFILER_PANEL_H

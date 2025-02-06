/**************************************************************************/
/*  tiles_editor_plugin.h                                                 */
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

#include "editor/plugins/editor_plugin.h"

#include "tile_map_layer_editor.h"
#include "tile_set_editor.h"

class TilesEditorUtils : public Object {
	GDCLASS(TilesEditorUtils, Object);

	static TilesEditorUtils *singleton;

public:
	enum SourceSortOption {
		SOURCE_SORT_ID = 0,
		SOURCE_SORT_ID_REVERSE,
		SOURCE_SORT_NAME,
		SOURCE_SORT_NAME_REVERSE,
		SOURCE_SORT_MAX
	};

private:
	// For synchronization.
	int atlas_sources_lists_current = 0;
	float atlas_view_zoom = 1.0;
	Vector2 atlas_view_scroll;

	// Source sorting.
	int source_sort = SOURCE_SORT_ID;

	struct SourceNameComparator {
		static Ref<TileSet> tile_set;
		bool operator()(const int &p_a, const int &p_b) const;
	};

	// Patterns preview generation.
	struct QueueItem {
		Ref<TileSet> tile_set;
		Ref<TileMapPattern> pattern;
		Callable callback;
	};
	List<QueueItem> pattern_preview_queue;
	Mutex pattern_preview_mutex;
	Semaphore pattern_preview_sem;
	Thread pattern_preview_thread;
	SafeFlag pattern_thread_exit;
	SafeFlag pattern_thread_exited;
	Semaphore pattern_preview_done;
	void _preview_frame_started();
	void _pattern_preview_done();
	static void _thread_func(void *ud);
	void _thread();

public:
	_FORCE_INLINE_ static TilesEditorUtils *get_singleton() { return singleton; }

	// Pattern preview API.
	void queue_pattern_preview(Ref<TileSet> p_tile_set, Ref<TileMapPattern> p_pattern, Callable p_callback);

	// To synchronize the atlas sources lists.
	void set_sources_lists_current(int p_current);
	void synchronize_sources_list(Object *p_current_list, Object *p_current_sort_button);

	void set_atlas_view_transform(float p_zoom, Vector2 p_scroll);
	void synchronize_atlas_view(Object *p_current);

	// Sorting.
	void set_sorting_option(int p_option);
	List<int> get_sorted_sources(const Ref<TileSet> p_tile_set) const;

	// Misc.
	void display_tile_set_editor_panel();

	static void draw_selection_rect(CanvasItem *p_ci, const Rect2 &p_rect, const Color &p_color = Color(1.0, 1.0, 1.0));

	TilesEditorUtils();
	~TilesEditorUtils();
};

class TileMapEditorPlugin : public EditorPlugin {
	GDCLASS(TileMapEditorPlugin, EditorPlugin);

	TileMapLayerEditor *editor = nullptr;
	Button *button = nullptr;
	ObjectID tile_map_layer_id;
	ObjectID tile_map_group_id; // Allow keeping the layer selector up to date.

	bool tile_map_changed_needs_update = false;
	ObjectID tile_set_id; // The TileSet associated with the TileMap.

	void _tile_map_layer_changed();
	void _tile_map_layer_removed();
	void _update_tile_map();
	void _select_layer(const StringName &p_name);

	void _edit_tile_map_layer(TileMapLayer *p_tile_map_layer, bool p_show_layer_selector);
	void _edit_tile_map(TileMap *p_tile_map);

protected:
	void _notification(int p_notification);

public:
	virtual void edit(Object *p_object) override;
	virtual bool handles(Object *p_object) const override;
	virtual void make_visible(bool p_visible) override;

	virtual bool forward_canvas_gui_input(const Ref<InputEvent> &p_event) override;
	virtual void forward_canvas_draw_over_viewport(Control *p_overlay) override;

	void hide_editor();
	bool is_editor_visible() const;

	TileMapEditorPlugin();
	~TileMapEditorPlugin();
};

class TileSetEditorPlugin : public EditorPlugin {
	GDCLASS(TileSetEditorPlugin, EditorPlugin);

	TileSetEditor *editor = nullptr;
	Button *button = nullptr;

	ObjectID edited_tileset;

public:
	virtual void edit(Object *p_object) override;
	virtual bool handles(Object *p_object) const override;
	virtual void make_visible(bool p_visible) override;

	ObjectID get_edited_tileset() const;

	TileSetEditorPlugin();
	~TileSetEditorPlugin();
};

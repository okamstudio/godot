/**************************************************************************/
/*  json_view.h                                                           */
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

#ifndef JSON_VIEW_H
#define JSON_VIEW_H

#include "../snapshot_data.h"
#include "editor/editor_json_visualizer.h"
#include "snapshot_view.h"

class SnapshotJsonView : public SnapshotView {
	GDCLASS(SnapshotJsonView, SnapshotView);

protected:
	static void _serialization_worker(void *p_ud);
	void _update_text(GameStateSnapshot *p_data_ptr, GameStateSnapshot *p_diff_ptr, const String &p_data_str, const String &p_diff_data_str);

	static void _bind_methods();

	EditorJsonVisualizer *json_content = nullptr;
	EditorJsonVisualizer *diff_json_content = nullptr;

	Control *loading_panel = nullptr;

	void _load_theme_settings();
	static String _snapshot_to_json(GameStateSnapshot *p_snapshot);

public:
	SnapshotJsonView();
	virtual void show_snapshot(GameStateSnapshot *p_data, GameStateSnapshot *p_diff_data) override;
};

#endif // JSON_VIEW_H

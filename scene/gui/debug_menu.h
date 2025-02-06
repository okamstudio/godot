/**************************************************************************/
/*  debug_menu.h                                                          */
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

#ifndef DEBUG_MENU_H
#define DEBUG_MENU_H

#include "box_container.h"
#include "button.h"
#include "grid_container.h"
#include "label.h"
#include "margin_container.h"
#include "panel_container.h"
#include "rich_text_label.h"
#include "scene/main/canvas_layer.h"

class DebugMenu : public CanvasLayer {
	GDCLASS(DebugMenu, CanvasLayer);

public:
	enum GraphType {
		GRAPH_TYPE_FPS,
		GRAPH_TYPE_FRAMETIME_TOTAL,
		GRAPH_TYPE_FRAMETIME_CPU,
		GRAPH_TYPE_FRAMETIME_GPU,
	};

private:
	// Each graph's size in pixels.
	static constexpr int GRAPH_SIZE_X = 150;
	static constexpr int GRAPH_SIZE_Y = 25;

	// Minimum Y value on the graph and color gradients.
	// This value is also used for frametimes as `1.0 / GRAPH_MIN_FPS`.
	static constexpr int GRAPH_MIN_FPS = 10;
	static constexpr float GRAPH_MIN_FRAMETIME = 1.0 / GRAPH_MIN_FPS;

	// Maximum Y value on the graph and color gradients.
	// This value is also used for frametimes as `1.0 / GRAPH_MAX_FPS`.
	static constexpr int GRAPH_MAX_FPS = 160;
	static constexpr float GRAPH_MAX_FRAMETIME = 1.0 / GRAPH_MAX_FPS;

	// The number of frames to keep in history for graph drawing and best/worst calculations.
	// This also affects the average frametime measurements, since this is calculated over the entire history.
	static constexpr int HISTORY_NUM_FRAMES = 150;

	// Average frametime over the last measurement period, in milliseconds per frame.
	float frametime_total_avg = 1.0f;

	// Average CPU frametime over the last measurement period, in milliseconds per frame.
	float frametime_cpu_avg = 1.0f;

	// Average GPU frametime over the last measurement period, in milliseconds per frame.
	float frametime_gpu_avg = 1.0f;

	// The value of `Time::get_singleton()->get_ticks_usec()` during the last process frame.
	// Used to calculate the "Total" frametime.
	float last_tick = 0.0f;

	// Only used for graphs.
	PackedFloat32Array fps_history;

	// Used for graphs and best/worst calculations.
	PackedFloat32Array frame_history_total;
	PackedFloat32Array frame_history_cpu;
	PackedFloat32Array frame_history_gpu;

	Label *frames_per_second;
	Label *frametime;
	Label *frame_number;

	GridContainer *frame_time_history;
	Control *spacer_header;
	Label *avg_header;
	Label *best_header;
	Label *worst_header;
	Label *last_header;
	Label *total_header;
	Label *total_avg;
	Label *total_best;
	Label *total_worst;
	Label *total_last;
	Label *cpu_header;
	Label *cpu_avg;
	Label *cpu_best;
	Label *cpu_worst;
	Label *cpu_last;
	Label *gpu_header;
	Label *gpu_avg;
	Label *gpu_best;
	Label *gpu_worst;
	Label *gpu_last;

	HBoxContainer *fps_graph;
	Label *fps_graph_title;
	Panel *fps_graph_panel;

	HBoxContainer *total_graph;
	Label *total_graph_title;
	Panel *total_graph_panel;

	HBoxContainer *cpu_graph;
	Label *cpu_graph_title;
	Panel *cpu_graph_panel;

	HBoxContainer *gpu_graph;
	Label *gpu_graph_title;
	Panel *gpu_graph_panel;

	Label *information;
	Label *settings;

	// The display mode is always overridden to `DEBUG_MENU_DISPLAY_MODE_COMPACT` on initialization.
	SceneTree::DebugMenuDisplayMode display_mode = SceneTree::DEBUG_MENU_DISPLAY_MODE_HIDDEN;

	// The root viewport RID.
	RID viewport_rid;

	// Whether viewport render time measurements were enabled before the debug menu was created.
	bool previous_measure_render_time = false;

	Ref<Gradient> frame_time_gradient;

	void _update_information_label();
	void _update_settings_label();

	void _graph_draw(GraphType p_type);

protected:
	void _notification(int p_what);

public:
	void set_display_mode(SceneTree::DebugMenuDisplayMode p_mode);
	SceneTree::DebugMenuDisplayMode get_display_mode() const;

	DebugMenu();
};

VARIANT_ENUM_CAST(DebugMenu::GraphType);

#endif // DEBUG_MENU_H

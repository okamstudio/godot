/**************************************************************************/
/*  editor_performance_profiler.h                                         */
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

#include "core/templates/hash_map.h"
#include "main/performance.h"
#include "scene/gui/control.h"
#include "scene/gui/label.h"
#include "scene/gui/split_container.h"
#include "scene/gui/tree.h"

class EditorPerformanceProfiler : public HSplitContainer {
	GDCLASS(EditorPerformanceProfiler, HSplitContainer);

private:
	class Monitor {
	public:
		String name;
		String base;
		List<float> history;
		float max = 0.0f;
		TreeItem *item = nullptr;
		Performance::MonitorType type = Performance::MONITOR_TYPE_QUANTITY;
		int frame_index = 0;

		Monitor();
		Monitor(const String &p_name, const String &p_base, int p_frame_index, Performance::MonitorType p_type, TreeItem *p_item);
		void update_value(float p_value);
		void reset();
	};

	HashMap<StringName, Monitor> monitors;

	HashMap<StringName, TreeItem *> base_map;
	Tree *monitor_tree = nullptr;
	Control *monitor_draw = nullptr;
	Label *info_message = nullptr;
	StringName marker_key;
	int marker_frame = 0;
	const int MARGIN = 4;
	const int POINT_SEPARATION = 5;
	const int MARKER_MARGIN = 2;

	static String _create_label(float p_value, Performance::MonitorType p_type);
	void _monitor_select();
	void _monitor_draw();
	void _build_monitor_tree();
	TreeItem *_get_monitor_base(const StringName &p_base_name);
	TreeItem *_create_monitor_item(const StringName &p_monitor_name, TreeItem *p_base);
	void _marker_input(const Ref<InputEvent> &p_event);

protected:
	void _notification(int p_what);

public:
	void reset();
	void update_monitors(const Vector<StringName> &p_names);
	void add_profile_frame(const Vector<float> &p_values);
	List<float> *get_monitor_data(const StringName &p_name);
	EditorPerformanceProfiler();
};

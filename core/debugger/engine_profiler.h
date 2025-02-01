/**************************************************************************/
/*  engine_profiler.h                                                     */
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

#include "core/object/gdvirtual.gen.inc"
#include "core/object/ref_counted.h"

class EngineProfiler : public RefCounted {
	GDCLASS(EngineProfiler, RefCounted);

private:
	String registration;

protected:
	static void _bind_methods();

public:
	virtual void toggle(bool p_enable, const Array &p_opts);
	virtual void add(const Array &p_data);
	virtual void tick(double p_frame_time, double p_process_time, double p_physics_time, double p_physics_frame_time);

	Error bind(const String &p_name);
	Error unbind();
	bool is_bound() const { return registration.length() > 0; }

	GDVIRTUAL2(_toggle, bool, Array);
	GDVIRTUAL1(_add_frame, Array);
	GDVIRTUAL4(_tick, double, double, double, double);

	EngineProfiler() {}
	virtual ~EngineProfiler();
};

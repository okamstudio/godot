/**************************************************************************/
/*  range.h                                                               */
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

#include "scene/gui/control.h"

class Range : public Control {
	GDCLASS(Range, Control);

	struct Shared {
		double val = 0.0;
		double min = 0.0;
		double max = 100.0;
		double step = 1.0;
		double page = 0.0;
		bool exp_ratio = false;
		bool allow_greater = false;
		bool allow_lesser = false;
		HashSet<Range *> owners;
		void emit_value_changed();
		void emit_changed(const char *p_what = "");
		void redraw_owners();
	};

	Shared *shared = nullptr;

	void _ref_shared(Shared *p_shared);
	void _unref_shared();

	void _share(Node *p_range);

	void _value_changed_notify();
	void _changed_notify(const char *p_what = "");
	void _set_value_no_signal(double p_val);

protected:
	virtual void _value_changed(double p_value);
	void _notify_shared_value_changed() { shared->emit_value_changed(); }

	static void _bind_methods();

	bool _rounded_values = false;

	GDVIRTUAL1(_value_changed, double)

public:
	void set_value(double p_val);
	void set_value_no_signal(double p_val);
	void set_min(double p_min);
	void set_max(double p_max);
	void set_step(double p_step);
	void set_page(double p_page);
	void set_as_ratio(double p_value);

	double get_value() const;
	double get_min() const;
	double get_max() const;
	double get_step() const;
	double get_page() const;
	double get_as_ratio() const;

	void set_use_rounded_values(bool p_enable);
	bool is_using_rounded_values() const;

	void set_exp_ratio(bool p_enable);
	bool is_ratio_exp() const;

	void set_allow_greater(bool p_allow);
	bool is_greater_allowed() const;

	void set_allow_lesser(bool p_allow);
	bool is_lesser_allowed() const;

	void share(Range *p_range);
	void unshare();

	PackedStringArray get_configuration_warnings() const override;

	Range();
	~Range();
};

/**************************************************************************/
/*  range.cpp                                                             */
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

#include "range.h"

PackedStringArray Range::get_configuration_warnings() const {
	PackedStringArray warnings = Control::get_configuration_warnings();

	if (shared->exp_ratio && shared->min <= 0) {
		warnings.push_back(RTR("If \"Exp Edit\" is enabled, \"Min Value\" must be greater than 0."));
	}

	return warnings;
}

void Range::_value_changed(double p_value) {
	GDVIRTUAL_CALL(_value_changed, p_value);
}
void Range::_value_changed_notify() {
	_value_changed(shared->val);
	emit_signal(SceneStringName(value_changed), shared->val);
	queue_redraw();
}

void Range::Shared::emit_value_changed() {
	for (Range *E : owners) {
		Range *r = E;
		if (!r->is_inside_tree()) {
			continue;
		}
		r->_value_changed_notify();
	}
}

void Range::_changed_notify(const char *p_what) {
	emit_signal(CoreStringName(changed));
	queue_redraw();
}

void Range::Shared::emit_changed(const char *p_what) {
	for (Range *E : owners) {
		Range *r = E;
		if (!r->is_inside_tree()) {
			continue;
		}
		r->_changed_notify(p_what);
	}
}

void Range::Shared::redraw_owners() {
	for (Range *E : owners) {
		Range *r = E;
		if (!r->is_inside_tree()) {
			continue;
		}
		r->queue_redraw();
	}
}

void Range::set_value(double p_val) {
	double prev_val = shared->val;
	_set_value_no_signal(p_val);

	if (shared->val != prev_val) {
		shared->emit_value_changed();
	}
}

void Range::_set_value_no_signal(double p_val) {
	if (!Math::is_finite(p_val)) {
		return;
	}

	if (shared->step > 0 && !shared->skip_step_rounding) {
		p_val = Math::round((p_val - shared->min) / shared->step) * shared->step + shared->min;
	}

	if (_rounded_values) {
		p_val = Math::round(p_val);
	}

	if (!shared->allow_greater && p_val > shared->max - shared->page) {
		p_val = shared->max - shared->page;
	}

	if (!shared->allow_lesser && p_val < shared->min) {
		p_val = shared->min;
	}

	if (shared->val == p_val) {
		return;
	}

	shared->val = p_val;
}

void Range::set_value_no_signal(double p_val) {
	double prev_val = shared->val;
	_set_value_no_signal(p_val);

	if (shared->val != prev_val) {
		shared->redraw_owners();
	}
}

void Range::set_min(double p_min) {
	if (shared->min == p_min) {
		return;
	}

	shared->min = p_min;
	shared->max = MAX(shared->max, shared->min);
	shared->page = CLAMP(shared->page, 0, shared->max - shared->min);
	set_value(shared->val);

	shared->emit_changed("min");

	update_configuration_warnings();
}

void Range::set_max(double p_max) {
	double max_validated = MAX(p_max, shared->min);
	if (shared->max == max_validated) {
		return;
	}

	shared->max = max_validated;
	shared->page = CLAMP(shared->page, 0, shared->max - shared->min);
	set_value(shared->val);

	shared->emit_changed("max");
}

void Range::set_step(double p_step) {
	if (shared->step == p_step) {
		return;
	}

	shared->step = p_step;
	shared->emit_changed("step");
}

void Range::set_disallow_step_rounding(bool disallow) {
	shared->skip_step_rounding = disallow;
}

void Range::set_page(double p_page) {
	double page_validated = CLAMP(p_page, 0, shared->max - shared->min);
	if (shared->page == page_validated) {
		return;
	}

	shared->page = page_validated;
	set_value(shared->val);

	shared->emit_changed("page");
}

double Range::get_value() const {
	return shared->val;
}

double Range::get_min() const {
	return shared->min;
}

double Range::get_max() const {
	return shared->max;
}

double Range::get_step() const {
	return shared->step;
}

double Range::get_page() const {
	return shared->page;
}

void Range::set_as_ratio(double p_value) {
	double v;

	if (shared->exp_ratio && get_min() >= 0) {
		double exp_min = get_min() == 0 ? 0.0 : Math::log(get_min()) / Math::log((double)2);
		double exp_max = Math::log(get_max()) / Math::log((double)2);
		v = Math::pow(2, exp_min + (exp_max - exp_min) * p_value);
	} else {
		double percent = (get_max() - get_min()) * p_value;
		if (get_step() > 0) {
			double steps = round(percent / get_step());
			v = steps * get_step() + get_min();
		} else {
			v = percent + get_min();
		}
	}
	v = CLAMP(v, get_min(), get_max());
	set_value(v);
}

double Range::get_as_ratio() const {
	if (Math::is_equal_approx(get_max(), get_min())) {
		// Avoid division by zero.
		return 1.0;
	}

	if (shared->exp_ratio && get_min() >= 0) {
		double exp_min = get_min() == 0 ? 0.0 : Math::log(get_min()) / Math::log((double)2);
		double exp_max = Math::log(get_max()) / Math::log((double)2);
		float value = CLAMP(get_value(), shared->min, shared->max);
		double v = Math::log(value) / Math::log((double)2);

		return CLAMP((v - exp_min) / (exp_max - exp_min), 0, 1);
	} else {
		float value = CLAMP(get_value(), shared->min, shared->max);
		return CLAMP((value - get_min()) / (get_max() - get_min()), 0, 1);
	}
}

void Range::_share(Node *p_range) {
	Range *r = Object::cast_to<Range>(p_range);
	ERR_FAIL_NULL(r);
	share(r);
}

void Range::share(Range *p_range) {
	ERR_FAIL_NULL(p_range);

	p_range->_ref_shared(shared);
	p_range->_changed_notify();
	p_range->_value_changed_notify();
}

void Range::unshare() {
	Shared *nshared = memnew(Shared);
	nshared->min = shared->min;
	nshared->max = shared->max;
	nshared->val = shared->val;
	nshared->step = shared->step;
	nshared->skip_step_rounding = shared->skip_step_rounding;
	nshared->page = shared->page;
	nshared->exp_ratio = shared->exp_ratio;
	nshared->allow_greater = shared->allow_greater;
	nshared->allow_lesser = shared->allow_lesser;
	_unref_shared();
	_ref_shared(nshared);
}

void Range::_ref_shared(Shared *p_shared) {
	if (shared && p_shared == shared) {
		return;
	}

	_unref_shared();
	shared = p_shared;
	shared->owners.insert(this);
}

void Range::_unref_shared() {
	if (shared) {
		shared->owners.erase(this);
		if (shared->owners.size() == 0) {
			memdelete(shared);
			shared = nullptr;
		}
	}
}

void Range::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_value"), &Range::get_value);
	ClassDB::bind_method(D_METHOD("get_min"), &Range::get_min);
	ClassDB::bind_method(D_METHOD("get_max"), &Range::get_max);
	ClassDB::bind_method(D_METHOD("get_step"), &Range::get_step);
	ClassDB::bind_method(D_METHOD("get_page"), &Range::get_page);
	ClassDB::bind_method(D_METHOD("get_as_ratio"), &Range::get_as_ratio);
	ClassDB::bind_method(D_METHOD("set_value", "value"), &Range::set_value);
	ClassDB::bind_method(D_METHOD("set_value_no_signal", "value"), &Range::set_value_no_signal);
	ClassDB::bind_method(D_METHOD("set_min", "minimum"), &Range::set_min);
	ClassDB::bind_method(D_METHOD("set_max", "maximum"), &Range::set_max);
	ClassDB::bind_method(D_METHOD("set_step", "step"), &Range::set_step);
	ClassDB::bind_method(D_METHOD("set_page", "pagesize"), &Range::set_page);
	ClassDB::bind_method(D_METHOD("set_as_ratio", "value"), &Range::set_as_ratio);
	ClassDB::bind_method(D_METHOD("set_use_rounded_values", "enabled"), &Range::set_use_rounded_values);
	ClassDB::bind_method(D_METHOD("is_using_rounded_values"), &Range::is_using_rounded_values);
	ClassDB::bind_method(D_METHOD("set_exp_ratio", "enabled"), &Range::set_exp_ratio);
	ClassDB::bind_method(D_METHOD("is_ratio_exp"), &Range::is_ratio_exp);
	ClassDB::bind_method(D_METHOD("set_allow_greater", "allow"), &Range::set_allow_greater);
	ClassDB::bind_method(D_METHOD("is_greater_allowed"), &Range::is_greater_allowed);
	ClassDB::bind_method(D_METHOD("set_allow_lesser", "allow"), &Range::set_allow_lesser);
	ClassDB::bind_method(D_METHOD("is_lesser_allowed"), &Range::is_lesser_allowed);

	ClassDB::bind_method(D_METHOD("share", "with"), &Range::_share);
	ClassDB::bind_method(D_METHOD("unshare"), &Range::unshare);

	ADD_SIGNAL(MethodInfo("value_changed", PropertyInfo(Variant::FLOAT, "value")));
	ADD_SIGNAL(MethodInfo("changed"));

	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "min_value"), "set_min", "get_min");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "max_value"), "set_max", "get_max");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "step"), "set_step", "get_step");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "page"), "set_page", "get_page");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "value"), "set_value", "get_value");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "ratio", PROPERTY_HINT_RANGE, "0,1,0.01", PROPERTY_USAGE_NONE), "set_as_ratio", "get_as_ratio");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "exp_edit"), "set_exp_ratio", "is_ratio_exp");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "rounded"), "set_use_rounded_values", "is_using_rounded_values");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "allow_greater"), "set_allow_greater", "is_greater_allowed");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "allow_lesser"), "set_allow_lesser", "is_lesser_allowed");

	GDVIRTUAL_BIND(_value_changed, "new_value");

	ADD_LINKED_PROPERTY("min_value", "value");
	ADD_LINKED_PROPERTY("min_value", "max_value");
	ADD_LINKED_PROPERTY("min_value", "page");
	ADD_LINKED_PROPERTY("max_value", "value");
	ADD_LINKED_PROPERTY("max_value", "page");
}

void Range::set_use_rounded_values(bool p_enable) {
	_rounded_values = p_enable;
}

bool Range::is_using_rounded_values() const {
	return _rounded_values;
}

void Range::set_exp_ratio(bool p_enable) {
	if (shared->exp_ratio == p_enable) {
		return;
	}

	shared->exp_ratio = p_enable;

	update_configuration_warnings();
}

bool Range::is_ratio_exp() const {
	return shared->exp_ratio;
}

void Range::set_allow_greater(bool p_allow) {
	shared->allow_greater = p_allow;
}

bool Range::is_greater_allowed() const {
	return shared->allow_greater;
}

void Range::set_allow_lesser(bool p_allow) {
	shared->allow_lesser = p_allow;
}

bool Range::is_lesser_allowed() const {
	return shared->allow_lesser;
}

Range::Range() {
	shared = memnew(Shared);
	shared->owners.insert(this);
}

Range::~Range() {
	_unref_shared();
}

/**************************************************************************/
/*  tween.cpp                                                             */
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

#include "tween.h"

#include "scene/animation/easing_equations.h"
#include "scene/main/node.h"
#include "scene/resources/animation.h"

#define CHECK_VALID()                                                                                      \
	ERR_FAIL_COND_V_MSG(!valid, nullptr, "Tween invalid. Either finished or created outside scene tree."); \
	ERR_FAIL_COND_V_MSG(started, nullptr, "Can't append to a Tween that has started. Use stop() first.");

Tween::interpolater Tween::interpolaters[Tween::TRANS_MAX][Tween::EASE_MAX] = {
	{ &linear::in, &linear::in, &linear::in, &linear::in }, // Linear is the same for each easing.
	{ &sine::in, &sine::out, &sine::in_out, &sine::out_in },
	{ &quint::in, &quint::out, &quint::in_out, &quint::out_in },
	{ &quart::in, &quart::out, &quart::in_out, &quart::out_in },
	{ &quad::in, &quad::out, &quad::in_out, &quad::out_in },
	{ &expo::in, &expo::out, &expo::in_out, &expo::out_in },
	{ &elastic::in, &elastic::out, &elastic::in_out, &elastic::out_in },
	{ &cubic::in, &cubic::out, &cubic::in_out, &cubic::out_in },
	{ &circ::in, &circ::out, &circ::in_out, &circ::out_in },
	{ &bounce::in, &bounce::out, &bounce::in_out, &bounce::out_in },
	{ &back::in, &back::out, &back::in_out, &back::out_in },
	{ &spring::in, &spring::out, &spring::in_out, &spring::out_in },
};

void Tweener::set_tween(const Ref<Tween> &p_tween) {
	tween_id = p_tween->get_instance_id();
}

void Tweener::start() {
	elapsed_time = 0;
	finished = false;
}

Ref<Tween> Tweener::_get_tween() {
	return Ref<Tween>(ObjectDB::get_instance(tween_id));
}

void Tweener::_finish() {
	finished = true;
	emit_signal(SceneStringName(finished));
}

void Tweener::_bind_methods() {
	ADD_SIGNAL(MethodInfo("finished"));
}

bool Tween::_validate_type_match(const Variant &p_from, Variant &r_to) {
	if (p_from.get_type() != r_to.get_type()) {
		// Cast r_to between double and int to avoid minor annoyances.
		if (p_from.get_type() == Variant::FLOAT && r_to.get_type() == Variant::INT) {
			r_to = double(r_to);
		} else if (p_from.get_type() == Variant::INT && r_to.get_type() == Variant::FLOAT) {
			r_to = int(r_to);
		} else {
			ERR_FAIL_V_MSG(false, "Type mismatch between initial and final value: " + Variant::get_type_name(p_from.get_type()) + " and " + Variant::get_type_name(r_to.get_type()));
		}
	}
	return true;
}

void Tween::_start_tweeners() {
	if (tweeners.is_empty()) {
		dead = true;
		ERR_FAIL_MSG("Tween without commands, aborting.");
	}

	for (Ref<Tweener> &tweener : tweeners.write[current_step]) {
		tweener->start();
	}
}

void Tween::_stop_internal(bool p_reset) {
	running = false;
	if (p_reset) {
		started = false;
		dead = false;
		total_time = 0;
	}
}

Ref<PropertyTweener> Tween::tween_property(const Object *p_target, const NodePath &p_property, Variant p_to, double p_duration) {
	ERR_FAIL_NULL_V(p_target, nullptr);
	CHECK_VALID();

	Vector<StringName> property_subnames = p_property.get_as_property_path().get_subnames();
#ifdef DEBUG_ENABLED
	bool prop_valid;
	const Variant &prop_value = p_target->get_indexed(property_subnames, &prop_valid);
	ERR_FAIL_COND_V_MSG(!prop_valid, nullptr, vformat("The tweened property \"%s\" does not exist in object \"%s\".", p_property, p_target));
#else
	const Variant &prop_value = p_target->get_indexed(property_subnames);
#endif

	if (!_validate_type_match(prop_value, p_to)) {
		return nullptr;
	}

	Ref<PropertyTweener> tweener;
	tweener.instantiate(p_target, property_subnames, p_to, p_duration);
	append(tweener);
	return tweener;
}

Ref<IntervalTweener> Tween::tween_interval(double p_time) {
	CHECK_VALID();

	Ref<IntervalTweener> tweener;
	tweener.instantiate(p_time);
	append(tweener);
	return tweener;
}

Ref<CallbackTweener> Tween::tween_callback(const Callable &p_callback) {
	CHECK_VALID();

	Ref<CallbackTweener> tweener;
	tweener.instantiate(p_callback);
	append(tweener);
	return tweener;
}

Ref<MethodTweener> Tween::tween_method(const Callable &p_callback, const Variant p_from, Variant p_to, double p_duration) {
	CHECK_VALID();

	if (!_validate_type_match(p_from, p_to)) {
		return nullptr;
	}

	Ref<MethodTweener> tweener;
	tweener.instantiate(p_callback, p_from, p_to, p_duration);
	append(tweener);
	return tweener;
}

Ref<SubtweenTweener> Tween::tween_subtween(const Ref<Tween> &p_subtween) {
	CHECK_VALID();

	// Ensure that the subtween being added is not null.
	ERR_FAIL_COND_V(p_subtween.is_null(), nullptr);

	Ref<SubtweenTweener> tweener;
	tweener.instantiate(p_subtween);

	// Remove the tween from its parent tree, if it has one.
	// If the user created this tween without a parent tree attached,
	// then this step isn't necessary.
	if (tweener->subtween->parent_tree != nullptr) {
		tweener->subtween->parent_tree->remove_tween(tweener->subtween);
	}
	append(tweener);
	return tweener;
}

void Tween::append(Ref<Tweener> p_tweener) {
	p_tweener->set_tween(this);

	if (parallel_enabled) {
		current_step = MAX(current_step, 0);
	} else {
		current_step++;
	}
	parallel_enabled = default_parallel;

	tweeners.resize(current_step + 1);
	tweeners.write[current_step].push_back(p_tweener);
}

void Tween::stop() {
	_stop_internal(true);
}

void Tween::pause() {
	_stop_internal(false);
}

void Tween::play() {
	ERR_FAIL_COND_MSG(!valid, "Tween invalid. Either finished or created outside scene tree.");
	ERR_FAIL_COND_MSG(dead, "Can't play finished Tween, use stop() first to reset its state.");
	running = true;
}

void Tween::kill() {
	running = false; // For the sake of is_running().
	dead = true;
}

bool Tween::is_running() {
	return running;
}

bool Tween::is_valid() {
	return valid;
}

void Tween::clear() {
	valid = false;
	tweeners.clear();
}

Ref<Tween> Tween::bind_node(const Node *p_node) {
	ERR_FAIL_NULL_V(p_node, this);

	bound_node = p_node->get_instance_id();
	is_bound = true;
	return this;
}

Ref<Tween> Tween::set_process_mode(TweenProcessMode p_mode) {
	process_mode = p_mode;
	return this;
}

Tween::TweenProcessMode Tween::get_process_mode() const {
	return process_mode;
}

Ref<Tween> Tween::set_pause_mode(TweenPauseMode p_mode) {
	pause_mode = p_mode;
	return this;
}

Tween::TweenPauseMode Tween::get_pause_mode() const {
	return pause_mode;
}

Ref<Tween> Tween::set_ignore_time_scale(bool p_ignore) {
	ignore_time_scale = p_ignore;
	return this;
}

bool Tween::is_ignoring_time_scale() const {
	return ignore_time_scale;
}

Ref<Tween> Tween::set_parallel(bool p_parallel) {
	default_parallel = p_parallel;
	parallel_enabled = p_parallel;
	return this;
}

Ref<Tween> Tween::set_loops(int p_loops) {
	loops = p_loops;
	return this;
}

int Tween::get_loops_left() const {
	if (loops <= 0) {
		return -1; // Infinite loop.
	} else {
		return loops - loops_done;
	}
}

Ref<Tween> Tween::set_speed_scale(float p_speed) {
	speed_scale = p_speed;
	return this;
}

Ref<Tween> Tween::set_trans(TransitionType p_trans) {
	default_transition = p_trans;
	return this;
}

Tween::TransitionType Tween::get_trans() const {
	return default_transition;
}

Ref<Tween> Tween::set_ease(EaseType p_ease) {
	default_ease = p_ease;
	return this;
}

Tween::EaseType Tween::get_ease() const {
	return default_ease;
}

Ref<Tween> Tween::parallel() {
	parallel_enabled = true;
	return this;
}

Ref<Tween> Tween::chain() {
	parallel_enabled = false;
	return this;
}

bool Tween::custom_step(double p_delta) {
	bool r = running;
	running = true;
	bool ret = step(p_delta);
	running = running && r; // Running might turn false when Tween finished.
	return ret;
}

bool Tween::step(double p_delta) {
	if (dead) {
		return false;
	}

	if (is_bound) {
		Node *node = get_bound_node();
		if (node) {
			if (!node->is_inside_tree()) {
				return true;
			}
		} else {
			return false;
		}
	}

	if (!running) {
		return true;
	}

	if (!started) {
		if (tweeners.is_empty()) {
			String tween_id;
			Node *node = get_bound_node();
			if (node) {
				tween_id = vformat("Tween (bound to %s)", node->is_inside_tree() ? (String)node->get_path() : (String)node->get_name());
			} else {
				tween_id = to_string();
			}
			ERR_FAIL_V_MSG(false, tween_id + ": started with no Tweeners.");
		}
		current_step = 0;
		loops_done = 0;
		total_time = 0;
		_start_tweeners();
		started = true;
	}

	double rem_delta = p_delta * speed_scale;
	bool step_active = false;
	total_time += rem_delta;

#ifdef DEBUG_ENABLED
	double initial_delta = rem_delta;
	bool potential_infinite = false;
#endif

	while (rem_delta > 0 && running) {
		double step_delta = rem_delta;
		step_active = false;

		for (Ref<Tweener> &tweener : tweeners.write[current_step]) {
			// Modified inside Tweener.step().
			double temp_delta = rem_delta;
			// Turns to true if any Tweener returns true (i.e. is still not finished).
			step_active = tweener->step(temp_delta) || step_active;
			step_delta = MIN(temp_delta, step_delta);
		}

		rem_delta = step_delta;

		if (!step_active) {
			emit_signal(SNAME("step_finished"), current_step);
			current_step++;

			if (current_step == tweeners.size()) {
				loops_done++;
				if (loops_done == loops) {
					running = false;
					dead = true;
					emit_signal(SceneStringName(finished));
					break;
				} else {
					emit_signal(SNAME("loop_finished"), loops_done);
					current_step = 0;
					_start_tweeners();
#ifdef DEBUG_ENABLED
					if (loops <= 0 && Math::is_equal_approx(rem_delta, initial_delta)) {
						if (!potential_infinite) {
							potential_infinite = true;
						} else {
							// Looped twice without using any time, this is 100% certain infinite loop.
							ERR_FAIL_V_MSG(false, "Infinite loop detected. Check set_loops() description for more info.");
						}
					}
#endif
				}
			} else {
				_start_tweeners();
			}
		}
	}

	return true;
}

bool Tween::can_process(bool p_tree_paused) const {
	if (is_bound && pause_mode == TWEEN_PAUSE_BOUND) {
		Node *node = get_bound_node();
		if (node) {
			return node->is_inside_tree() && node->can_process();
		}
	}

	return !p_tree_paused || pause_mode == TWEEN_PAUSE_PROCESS;
}

Node *Tween::get_bound_node() const {
	if (is_bound) {
		return Object::cast_to<Node>(ObjectDB::get_instance(bound_node));
	} else {
		return nullptr;
	}
}

double Tween::get_total_time() const {
	return total_time;
}

real_t Tween::run_equation(TransitionType p_trans_type, EaseType p_ease_type, real_t p_time, real_t p_initial, real_t p_delta, real_t p_duration) {
	if (p_duration == 0) {
		// Special case to avoid dividing by 0 in equations.
		return p_initial + p_delta;
	}

	interpolater func = interpolaters[p_trans_type][p_ease_type];
	return func(p_time, p_initial, p_delta, p_duration);
}

Variant Tween::interpolate_variant(const Variant &p_initial_val, const Variant &p_delta_val, double p_time, double p_duration, TransitionType p_trans, EaseType p_ease) {
	ERR_FAIL_INDEX_V(p_trans, TransitionType::TRANS_MAX, Variant());
	ERR_FAIL_INDEX_V(p_ease, EaseType::EASE_MAX, Variant());

	Variant ret = Animation::add_variant(p_initial_val, p_delta_val);
	ret = Animation::interpolate_variant(p_initial_val, ret, run_equation(p_trans, p_ease, p_time, 0.0, 1.0, p_duration), p_initial_val.is_string());
	return ret;
}

String Tween::to_string() {
	String ret = Object::to_string();
	Node *node = get_bound_node();
	if (node) {
		ret += vformat(" (bound to %s)", node->get_name());
	}
	return ret;
}

void Tween::_bind_methods() {
	ClassDB::bind_method(D_METHOD("tween_property", "object", "property", "final_val", "duration"), &Tween::tween_property);
	ClassDB::bind_method(D_METHOD("tween_interval", "time"), &Tween::tween_interval);
	ClassDB::bind_method(D_METHOD("tween_callback", "callback"), &Tween::tween_callback);
	ClassDB::bind_method(D_METHOD("tween_method", "method", "from", "to", "duration"), &Tween::tween_method);
	ClassDB::bind_method(D_METHOD("tween_subtween", "subtween"), &Tween::tween_subtween);

	ClassDB::bind_method(D_METHOD("custom_step", "delta"), &Tween::custom_step);
	ClassDB::bind_method(D_METHOD("stop"), &Tween::stop);
	ClassDB::bind_method(D_METHOD("pause"), &Tween::pause);
	ClassDB::bind_method(D_METHOD("play"), &Tween::play);
	ClassDB::bind_method(D_METHOD("kill"), &Tween::kill);
	ClassDB::bind_method(D_METHOD("get_total_elapsed_time"), &Tween::get_total_time);

	ClassDB::bind_method(D_METHOD("is_running"), &Tween::is_running);
	ClassDB::bind_method(D_METHOD("is_valid"), &Tween::is_valid);
	ClassDB::bind_method(D_METHOD("bind_node", "node"), &Tween::bind_node);
	ClassDB::bind_method(D_METHOD("set_process_mode", "mode"), &Tween::set_process_mode);
	ClassDB::bind_method(D_METHOD("set_pause_mode", "mode"), &Tween::set_pause_mode);
	ClassDB::bind_method(D_METHOD("set_ignore_time_scale", "ignore"), &Tween::set_ignore_time_scale, DEFVAL(true));

	ClassDB::bind_method(D_METHOD("set_parallel", "parallel"), &Tween::set_parallel, DEFVAL(true));
	ClassDB::bind_method(D_METHOD("set_loops", "loops"), &Tween::set_loops, DEFVAL(0));
	ClassDB::bind_method(D_METHOD("get_loops_left"), &Tween::get_loops_left);
	ClassDB::bind_method(D_METHOD("set_speed_scale", "speed"), &Tween::set_speed_scale);
	ClassDB::bind_method(D_METHOD("set_trans", "trans"), &Tween::set_trans);
	ClassDB::bind_method(D_METHOD("set_ease", "ease"), &Tween::set_ease);

	ClassDB::bind_method(D_METHOD("parallel"), &Tween::parallel);
	ClassDB::bind_method(D_METHOD("chain"), &Tween::chain);

	ClassDB::bind_static_method("Tween", D_METHOD("interpolate_value", "initial_value", "delta_value", "elapsed_time", "duration", "trans_type", "ease_type"), &Tween::interpolate_variant);

	ADD_SIGNAL(MethodInfo("step_finished", PropertyInfo(Variant::INT, "idx")));
	ADD_SIGNAL(MethodInfo("loop_finished", PropertyInfo(Variant::INT, "loop_count")));
	ADD_SIGNAL(MethodInfo("finished"));

	BIND_ENUM_CONSTANT(TWEEN_PROCESS_PHYSICS);
	BIND_ENUM_CONSTANT(TWEEN_PROCESS_IDLE);

	BIND_ENUM_CONSTANT(TWEEN_PAUSE_BOUND);
	BIND_ENUM_CONSTANT(TWEEN_PAUSE_STOP);
	BIND_ENUM_CONSTANT(TWEEN_PAUSE_PROCESS);

	BIND_ENUM_CONSTANT(TRANS_LINEAR);
	BIND_ENUM_CONSTANT(TRANS_SINE);
	BIND_ENUM_CONSTANT(TRANS_QUINT);
	BIND_ENUM_CONSTANT(TRANS_QUART);
	BIND_ENUM_CONSTANT(TRANS_QUAD);
	BIND_ENUM_CONSTANT(TRANS_EXPO);
	BIND_ENUM_CONSTANT(TRANS_ELASTIC);
	BIND_ENUM_CONSTANT(TRANS_CUBIC);
	BIND_ENUM_CONSTANT(TRANS_CIRC);
	BIND_ENUM_CONSTANT(TRANS_BOUNCE);
	BIND_ENUM_CONSTANT(TRANS_BACK);
	BIND_ENUM_CONSTANT(TRANS_SPRING);

	BIND_ENUM_CONSTANT(EASE_IN);
	BIND_ENUM_CONSTANT(EASE_OUT);
	BIND_ENUM_CONSTANT(EASE_IN_OUT);
	BIND_ENUM_CONSTANT(EASE_OUT_IN);
}

Tween::Tween() {
	ERR_FAIL_MSG("Tween can't be created directly. Use create_tween() method.");
}

Tween::Tween(SceneTree *p_parent_tree) {
	parent_tree = p_parent_tree;
	valid = true;
}

Ref<PropertyTweener> PropertyTweener::from(const Variant &p_value) {
	Ref<Tween> tween = _get_tween();
	ERR_FAIL_COND_V(tween.is_null(), nullptr);

	Variant from_value = p_value;
	if (!tween->_validate_type_match(final_val, from_value)) {
		return nullptr;
	}

	initial_val = from_value;
	do_continue = false;
	return this;
}

Ref<PropertyTweener> PropertyTweener::from_current() {
	do_continue = false;
	return this;
}

Ref<PropertyTweener> PropertyTweener::as_relative() {
	relative = true;
	return this;
}

Ref<PropertyTweener> PropertyTweener::set_trans(Tween::TransitionType p_trans) {
	trans_type = p_trans;
	return this;
}

Ref<PropertyTweener> PropertyTweener::set_ease(Tween::EaseType p_ease) {
	ease_type = p_ease;
	return this;
}

Ref<PropertyTweener> PropertyTweener::set_custom_interpolator(const Callable &p_method) {
	custom_method = p_method;
	return this;
}

Ref<PropertyTweener> PropertyTweener::set_delay(double p_delay) {
	delay = p_delay;
	return this;
}

void PropertyTweener::start() {
	Tweener::start();

	Object *target_instance = ObjectDB::get_instance(target);
	if (!target_instance) {
		WARN_PRINT("Target object freed before starting, aborting Tweener.");
		return;
	}

	if (do_continue) {
		if (Math::is_zero_approx(delay)) {
			initial_val = target_instance->get_indexed(property);
		} else {
			do_continue_delayed = true;
		}
	}

	if (relative) {
		final_val = Animation::add_variant(initial_val, base_final_val);
	}

	delta_val = Animation::subtract_variant(final_val, initial_val);
}

bool PropertyTweener::step(double &r_delta) {
	if (finished) {
		// This is needed in case there's a parallel Tweener with longer duration.
		return false;
	}

	Object *target_instance = ObjectDB::get_instance(target);
	if (!target_instance) {
		_finish();
		return false;
	}
	elapsed_time += r_delta;

	if (elapsed_time < delay) {
		r_delta = 0;
		return true;
	} else if (do_continue_delayed && !Math::is_zero_approx(delay)) {
		initial_val = target_instance->get_indexed(property);
		delta_val = Animation::subtract_variant(final_val, initial_val);
		do_continue_delayed = false;
	}

	Ref<Tween> tween = _get_tween();

	double time = MIN(elapsed_time - delay, duration);
	if (time < duration) {
		if (custom_method.is_valid()) {
			const Variant t = tween->interpolate_variant(0.0, 1.0, time, duration, trans_type, ease_type);
			const Variant *argptr = &t;

			Variant result;
			Callable::CallError ce;
			custom_method.callp(&argptr, 1, result, ce);
			if (ce.error != Callable::CallError::CALL_OK) {
				ERR_FAIL_V_MSG(false, "Error calling custom method from PropertyTweener: " + Variant::get_callable_error_text(custom_method, &argptr, 1, ce) + ".");
			} else if (result.get_type() != Variant::FLOAT) {
				ERR_FAIL_V_MSG(false, vformat("Wrong return type in PropertyTweener custom method. Expected float, got %s.", Variant::get_type_name(result.get_type())));
			}

			target_instance->set_indexed(property, Animation::interpolate_variant(initial_val, final_val, result));
		} else {
			target_instance->set_indexed(property, tween->interpolate_variant(initial_val, delta_val, time, duration, trans_type, ease_type));
		}
		r_delta = 0;
		return true;
	} else {
		target_instance->set_indexed(property, final_val);
		r_delta = elapsed_time - delay - duration;
		_finish();
		return false;
	}
}

void PropertyTweener::set_tween(const Ref<Tween> &p_tween) {
	Tweener::set_tween(p_tween);
	if (trans_type == Tween::TRANS_MAX) {
		trans_type = p_tween->get_trans();
	}
	if (ease_type == Tween::EASE_MAX) {
		ease_type = p_tween->get_ease();
	}
}

void PropertyTweener::_bind_methods() {
	ClassDB::bind_method(D_METHOD("from", "value"), &PropertyTweener::from);
	ClassDB::bind_method(D_METHOD("from_current"), &PropertyTweener::from_current);
	ClassDB::bind_method(D_METHOD("as_relative"), &PropertyTweener::as_relative);
	ClassDB::bind_method(D_METHOD("set_trans", "trans"), &PropertyTweener::set_trans);
	ClassDB::bind_method(D_METHOD("set_ease", "ease"), &PropertyTweener::set_ease);
	ClassDB::bind_method(D_METHOD("set_custom_interpolator", "interpolator_method"), &PropertyTweener::set_custom_interpolator);
	ClassDB::bind_method(D_METHOD("set_delay", "delay"), &PropertyTweener::set_delay);
}

PropertyTweener::PropertyTweener(const Object *p_target, const Vector<StringName> &p_property, const Variant &p_to, double p_duration) {
	target = p_target->get_instance_id();
	property = p_property;
	initial_val = p_target->get_indexed(property);
	base_final_val = p_to;
	final_val = base_final_val;
	duration = p_duration;

	if (p_target->is_ref_counted()) {
		ref_copy = p_target;
	}
}

PropertyTweener::PropertyTweener() {
	ERR_FAIL_MSG("PropertyTweener can't be created directly. Use the tween_property() method in Tween.");
}

bool IntervalTweener::step(double &r_delta) {
	if (finished) {
		return false;
	}

	elapsed_time += r_delta;

	if (elapsed_time < duration) {
		r_delta = 0;
		return true;
	} else {
		r_delta = elapsed_time - duration;
		_finish();
		return false;
	}
}

IntervalTweener::IntervalTweener(double p_time) {
	duration = p_time;
}

IntervalTweener::IntervalTweener() {
	ERR_FAIL_MSG("IntervalTweener can't be created directly. Use the tween_interval() method in Tween.");
}

Ref<CallbackTweener> CallbackTweener::set_delay(double p_delay) {
	delay = p_delay;
	return this;
}

bool CallbackTweener::step(double &r_delta) {
	if (finished) {
		return false;
	}

	if (!callback.is_valid()) {
		_finish();
		return false;
	}

	elapsed_time += r_delta;
	if (elapsed_time >= delay) {
		Variant result;
		Callable::CallError ce;
		callback.callp(nullptr, 0, result, ce);
		if (ce.error != Callable::CallError::CALL_OK) {
			ERR_FAIL_V_MSG(false, "Error calling method from CallbackTweener: " + Variant::get_callable_error_text(callback, nullptr, 0, ce) + ".");
		}

		r_delta = elapsed_time - delay;
		_finish();
		return false;
	}

	r_delta = 0;
	return true;
}

void CallbackTweener::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_delay", "delay"), &CallbackTweener::set_delay);
}

CallbackTweener::CallbackTweener(const Callable &p_callback) {
	callback = p_callback;

	Object *callback_instance = p_callback.get_object();
	if (callback_instance && callback_instance->is_ref_counted()) {
		ref_copy = callback_instance;
	}
}

CallbackTweener::CallbackTweener() {
	ERR_FAIL_MSG("CallbackTweener can't be created directly. Use the tween_callback() method in Tween.");
}

Ref<MethodTweener> MethodTweener::set_delay(double p_delay) {
	delay = p_delay;
	return this;
}

Ref<MethodTweener> MethodTweener::set_trans(Tween::TransitionType p_trans) {
	trans_type = p_trans;
	return this;
}

Ref<MethodTweener> MethodTweener::set_ease(Tween::EaseType p_ease) {
	ease_type = p_ease;
	return this;
}

bool MethodTweener::step(double &r_delta) {
	if (finished) {
		return false;
	}

	if (!callback.is_valid()) {
		_finish();
		return false;
	}

	elapsed_time += r_delta;

	if (elapsed_time < delay) {
		r_delta = 0;
		return true;
	}

	Ref<Tween> tween = _get_tween();

	Variant current_val;
	double time = MIN(elapsed_time - delay, duration);
	if (time < duration) {
		current_val = tween->interpolate_variant(initial_val, delta_val, time, duration, trans_type, ease_type);
	} else {
		current_val = final_val;
	}
	const Variant **argptr = (const Variant **)alloca(sizeof(Variant *));
	argptr[0] = &current_val;

	Variant result;
	Callable::CallError ce;
	callback.callp(argptr, 1, result, ce);
	if (ce.error != Callable::CallError::CALL_OK) {
		ERR_FAIL_V_MSG(false, "Error calling method from MethodTweener: " + Variant::get_callable_error_text(callback, argptr, 1, ce) + ".");
	}

	if (time < duration) {
		r_delta = 0;
		return true;
	} else {
		r_delta = elapsed_time - delay - duration;
		_finish();
		return false;
	}
}

void MethodTweener::set_tween(const Ref<Tween> &p_tween) {
	Tweener::set_tween(p_tween);
	if (trans_type == Tween::TRANS_MAX) {
		trans_type = p_tween->get_trans();
	}
	if (ease_type == Tween::EASE_MAX) {
		ease_type = p_tween->get_ease();
	}
}

void MethodTweener::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_delay", "delay"), &MethodTweener::set_delay);
	ClassDB::bind_method(D_METHOD("set_trans", "trans"), &MethodTweener::set_trans);
	ClassDB::bind_method(D_METHOD("set_ease", "ease"), &MethodTweener::set_ease);
}

MethodTweener::MethodTweener(const Callable &p_callback, const Variant &p_from, const Variant &p_to, double p_duration) {
	callback = p_callback;
	initial_val = p_from;
	delta_val = Animation::subtract_variant(p_to, p_from);
	final_val = p_to;
	duration = p_duration;

	Object *callback_instance = p_callback.get_object();
	if (callback_instance && callback_instance->is_ref_counted()) {
		ref_copy = callback_instance;
	}
}

MethodTweener::MethodTweener() {
	ERR_FAIL_MSG("MethodTweener can't be created directly. Use the tween_method() method in Tween.");
}

void SubtweenTweener::start() {
	Tweener::start();

	// Reset the subtween.
	subtween->stop();
	subtween->play();
}

bool SubtweenTweener::step(double &r_delta) {
	if (finished) {
		return false;
	}

	elapsed_time += r_delta;

	if (elapsed_time < delay) {
		r_delta = 0;
		return true;
	}

	if (!subtween->step(r_delta)) {
		r_delta = elapsed_time - delay - subtween->get_total_time();
		_finish();
		return false;
	}

	r_delta = 0;
	return true;
}

Ref<SubtweenTweener> SubtweenTweener::set_delay(double p_delay) {
	delay = p_delay;
	return this;
}

void SubtweenTweener::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_delay", "delay"), &SubtweenTweener::set_delay);
}

SubtweenTweener::SubtweenTweener(const Ref<Tween> &p_subtween) {
	subtween = p_subtween;
}

SubtweenTweener::SubtweenTweener() {
	ERR_FAIL_MSG("SubtweenTweener can't be created directly. Use the tween_subtween() method in Tween.");
}

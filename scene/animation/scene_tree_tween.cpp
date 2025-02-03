/**************************************************************************/
/*  scene_tree_tween.cpp                                                  */
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

#include "scene_tree_tween.h"

#include "core/method_bind_ext.gen.inc"
#include "scene/animation/tween.h"
#include "scene/main/node.h"
#include "scene/scene_string_names.h"

void Tweener::set_tween(Ref<SceneTreeTween> p_tween) {
	tween = p_tween;
}

void Tweener::clear_tween() {
	tween.unref();
}

void Tweener::_bind_methods() {
	ADD_SIGNAL(MethodInfo("finished"));
}

void SceneTreeTween::start_tweeners() {
	if (tweeners.empty()) {
		dead = true;
		ERR_FAIL_MSG("SceneTreeTween without commands, aborting");
	}

	List<Ref<Tweener>> &step = tweeners.write[current_step];
	for (int i = 0; i < step.size(); i++) {
		Ref<Tweener> &tweener = step[i];
		tweener->start();
	}
}

Ref<PropertyTweener> SceneTreeTween::tween_property(Object *p_target, NodePath p_property, Variant p_to, float p_duration) {
	ERR_FAIL_NULL_V(p_target, nullptr);
	ERR_FAIL_COND_V_MSG(!valid, nullptr, "SceneTreeTween invalid. Either finished or created outside scene tree.");
	ERR_FAIL_COND_V_MSG(started, nullptr, "Can't append to a SceneTreeTween that has started. Use stop() first.");

	Variant::Type property_type = p_target->get_indexed(p_property.get_as_property_path().get_subnames()).get_type();
	if (property_type != p_to.get_type()) {
		// Cast p_to between floats and ints to avoid minor annoyances.
		if (property_type == Variant::REAL && p_to.get_type() == Variant::INT) {
			p_to = float(p_to);
		} else if (property_type == Variant::INT && p_to.get_type() == Variant::REAL) {
			p_to = int(p_to);
		} else {
			ERR_FAIL_V_MSG(Ref<PropertyTweener>(), "Type mismatch between property and final value: " + Variant::get_type_name(property_type) + " and " + Variant::get_type_name(p_to.get_type()));
		}
	}

	Ref<PropertyTweener> tweener = memnew(PropertyTweener(p_target, p_property, p_to, p_duration));
	append(tweener);
	return tweener;
}

Ref<IntervalTweener> SceneTreeTween::tween_interval(float p_time) {
	ERR_FAIL_COND_V_MSG(!valid, nullptr, "SceneTreeTween invalid. Either finished or created outside scene tree.");
	ERR_FAIL_COND_V_MSG(started, nullptr, "Can't append to a SceneTreeTween that has started. Use stop() first.");

	Ref<IntervalTweener> tweener = memnew(IntervalTweener(p_time));
	append(tweener);
	return tweener;
}

Ref<CallbackTweener> SceneTreeTween::tween_callback(Object *p_target, StringName p_method, const Vector<Variant> &p_binds) {
	ERR_FAIL_NULL_V(p_target, nullptr);
	ERR_FAIL_COND_V_MSG(!valid, nullptr, "SceneTreeTween invalid. Either finished or created outside scene tree.");
	ERR_FAIL_COND_V_MSG(started, nullptr, "Can't append to a SceneTreeTween that has started. Use stop() first.");

	Ref<CallbackTweener> tweener = memnew(CallbackTweener(p_target, p_method, p_binds));
	append(tweener);
	return tweener;
}

Ref<MethodTweener> SceneTreeTween::tween_method(Object *p_target, StringName p_method, Variant p_from, Variant p_to, float p_duration, const Vector<Variant> &p_binds) {
	ERR_FAIL_NULL_V(p_target, nullptr);
	ERR_FAIL_COND_V_MSG(!valid, nullptr, "SceneTreeTween invalid. Either finished or created outside scene tree.");
	ERR_FAIL_COND_V_MSG(started, nullptr, "Can't append to a SceneTreeTween that has started. Use stop() first.");

	Ref<MethodTweener> tweener = memnew(MethodTweener(p_target, p_method, p_from, p_to, p_duration, p_binds));
	append(tweener);
	return tweener;
}

Ref<YieldTweener> SceneTreeTween::tween_yield(Object *p_target, StringName p_signal) {
	ERR_FAIL_NULL_V(p_target, nullptr);
	ERR_FAIL_COND_V_MSG(!valid, nullptr, "SceneTreeTween invalid. Either finished or created outside scene tree.");
	ERR_FAIL_COND_V_MSG(started, nullptr, "Can't append to a SceneTreeTween that has started. Use stop() first.");

	Ref<YieldTweener> tweener = memnew(YieldTweener(p_target, p_signal));
	append(tweener);
	return tweener;
}

void SceneTreeTween::append(Ref<Tweener> p_tweener) {
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

void SceneTreeTween::stop() {
	started = false;
	running = false;
	dead = false;
	total_time = 0;
}

void SceneTreeTween::pause() {
	running = false;
}

void SceneTreeTween::play() {
	ERR_FAIL_COND_MSG(!valid, "SceneTreeTween invalid. Either finished or created outside scene tree.");
	ERR_FAIL_COND_MSG(dead, "Can't play finished SceneTreeTween, use stop() first to reset its state.");
	running = true;
}

void SceneTreeTween::kill() {
	running = false; // For the sake of is_running().
	dead = true;
}

bool SceneTreeTween::is_running() const {
	return running;
}

bool SceneTreeTween::is_valid() const {
	return valid;
}

void SceneTreeTween::clear() {
	valid = false;

	for (int i = 0; i < tweeners.size(); i++) {
		List<Ref<Tweener>> &step = tweeners.write[i];
		for (int j = 0; j < step.size(); j++) {
			Ref<Tweener> &tweener = step[j];
			tweener->clear_tween();
		}
	}
	tweeners.clear();
}

Ref<SceneTreeTween> SceneTreeTween::bind_node(Node *p_node) {
	ERR_FAIL_NULL_V(p_node, this);

	bound_node = p_node->get_instance_id();
	is_bound = true;
	return this;
}

Ref<SceneTreeTween> SceneTreeTween::set_process_mode(Tween::TweenProcessMode p_mode) {
	process_mode = p_mode;
	return this;
}

Ref<SceneTreeTween> SceneTreeTween::set_pause_mode(TweenPauseMode p_mode) {
	pause_mode = p_mode;
	return this;
}

Ref<SceneTreeTween> SceneTreeTween::set_parallel(bool p_parallel) {
	default_parallel = p_parallel;
	parallel_enabled = p_parallel;
	return this;
}

Ref<SceneTreeTween> SceneTreeTween::set_loops(int p_loops) {
	loops = p_loops;
	return this;
}

Ref<SceneTreeTween> SceneTreeTween::set_speed_scale(float p_speed) {
	speed_scale = p_speed;
	return this;
}

Ref<SceneTreeTween> SceneTreeTween::set_trans(Tween::TransitionType p_trans) {
	default_transition = p_trans;
	return this;
}

Ref<SceneTreeTween> SceneTreeTween::set_ease(Tween::EaseType p_ease) {
	default_ease = p_ease;
	return this;
}

Tween::TweenProcessMode SceneTreeTween::get_process_mode() const {
	return process_mode;
}

SceneTreeTween::TweenPauseMode SceneTreeTween::get_pause_mode() const {
	return pause_mode;
}

Tween::TransitionType SceneTreeTween::get_trans() const {
	return default_transition;
}

Tween::EaseType SceneTreeTween::get_ease() const {
	return default_ease;
}

Ref<SceneTreeTween> SceneTreeTween::parallel() {
	parallel_enabled = true;
	return this;
}

Ref<SceneTreeTween> SceneTreeTween::chain() {
	parallel_enabled = false;
	return this;
}

bool SceneTreeTween::custom_step(float p_delta) {
	bool r = running;
	running = true;
	bool ret = step(p_delta);
	running = running && r; // Running might turn false when SceneTreeTween finished;
	return ret;
}

bool SceneTreeTween::step(float p_delta) {
	if (dead) {
		return false;
	}

	if (!running) {
		return true;
	}

	if (is_bound) {
		Node *bound_node = get_bound_node();
		if (bound_node) {
			if (!bound_node->is_inside_tree()) {
				return true;
			}
		} else {
			return false;
		}
	}

	if (!started) {
		ERR_FAIL_COND_V_MSG(tweeners.empty(), false, "SceneTreeTween started, but has no Tweeners.");
		current_step = 0;
		loops_done = 0;
		total_time = 0;
		start_tweeners();
		started = true;
	}

	float rem_delta = p_delta * speed_scale;
	bool step_active = false;
	total_time += rem_delta;

#ifdef DEBUG_ENABLED
	float initial_delta = rem_delta;
	bool potential_infinite = false;
#endif

	while (rem_delta > 0 && running) {
		float step_delta = rem_delta;
		step_active = false;

		List<Ref<Tweener>> &step = tweeners.write[current_step];
		for (int i = 0; i < step.size(); i++) {
			Ref<Tweener> &tweener = step[i];

			// Modified inside Tweener.step().
			float temp_delta = rem_delta;
			// Turns to true if any Tweener returns true (i.e. is still not finished).
			step_active = tweener->step(temp_delta) || step_active;
			step_delta = MIN(temp_delta, step_delta);
		}

		rem_delta = step_delta;

		if (!step_active) {
			emit_signal(SceneStringNames::get_singleton()->step_finished, current_step);
			current_step++;

			if (current_step == tweeners.size()) {
				loops_done++;
				if (loops_done == loops) {
					running = false;
					dead = true;
					emit_signal(SceneStringNames::get_singleton()->finished);
					break;
				} else {
					emit_signal(SceneStringNames::get_singleton()->loop_finished, loops_done);
					current_step = 0;
					start_tweeners();
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
				start_tweeners();
			}
		}
	}

	return true;
}

bool SceneTreeTween::can_process(bool p_tree_paused) const {
	if (is_bound && pause_mode == TWEEN_PAUSE_BOUND) {
		Node *bound_node = get_bound_node();
		if (bound_node) {
			return bound_node->is_inside_tree() && bound_node->can_process();
		}
	}

	return !p_tree_paused || pause_mode == TWEEN_PAUSE_PROCESS;
}

Node *SceneTreeTween::get_bound_node() const {
	if (is_bound) {
		return Object::cast_to<Node>(ObjectDB::get_instance(bound_node));
	} else {
		return nullptr;
	}
}

float SceneTreeTween::get_total_time() const {
	return total_time;
}

Variant SceneTreeTween::interpolate_variant(Variant p_initial_val, Variant p_delta_val, float p_time, float p_duration, Tween::TransitionType p_trans, Tween::EaseType p_ease) const {
	ERR_FAIL_INDEX_V(p_trans, Tween::TRANS_COUNT, Variant());
	ERR_FAIL_INDEX_V(p_ease, Tween::EASE_COUNT, Variant());

// Helper macro to run equation on sub-elements of the value (e.g. x and y of Vector2).
#define APPLY_EQUATION(element) \
	r.element = Tween::run_equation(p_trans, p_ease, p_time, i.element, d.element, p_duration);

	switch (p_initial_val.get_type()) {
		case Variant::BOOL: {
			return (Tween::run_equation(p_trans, p_ease, p_time, p_initial_val, p_delta_val, p_duration)) >= 0.5;
		}

		case Variant::INT: {
			return (int)Tween::run_equation(p_trans, p_ease, p_time, (int)p_initial_val, (int)p_delta_val, p_duration);
		}

		case Variant::REAL: {
			return Tween::run_equation(p_trans, p_ease, p_time, (real_t)p_initial_val, (real_t)p_delta_val, p_duration);
		}

		case Variant::VECTOR2: {
			Vector2 i = p_initial_val;
			Vector2 d = p_delta_val;
			Vector2 r;

			APPLY_EQUATION(x);
			APPLY_EQUATION(y);
			return r;
		}

		case Variant::RECT2: {
			Rect2 i = p_initial_val;
			Rect2 d = p_delta_val;
			Rect2 r;

			APPLY_EQUATION(position.x);
			APPLY_EQUATION(position.y);
			APPLY_EQUATION(size.x);
			APPLY_EQUATION(size.y);
			return r;
		}

		case Variant::VECTOR3: {
			Vector3 i = p_initial_val;
			Vector3 d = p_delta_val;
			Vector3 r;

			APPLY_EQUATION(x);
			APPLY_EQUATION(y);
			APPLY_EQUATION(z);
			return r;
		}

		case Variant::TRANSFORM2D: {
			Transform2D i = p_initial_val;
			Transform2D d = p_delta_val;
			Transform2D r;

			APPLY_EQUATION(elements[0][0]);
			APPLY_EQUATION(elements[0][1]);
			APPLY_EQUATION(elements[1][0]);
			APPLY_EQUATION(elements[1][1]);
			APPLY_EQUATION(elements[2][0]);
			APPLY_EQUATION(elements[2][1]);
			return r;
		}

		case Variant::QUAT: {
			Quat i = p_initial_val;
			Quat d = p_delta_val;
			Quat r;

			APPLY_EQUATION(x);
			APPLY_EQUATION(y);
			APPLY_EQUATION(z);
			APPLY_EQUATION(w);
			return r;
		}

		case Variant::AABB: {
			AABB i = p_initial_val;
			AABB d = p_delta_val;
			AABB r;

			APPLY_EQUATION(position.x);
			APPLY_EQUATION(position.y);
			APPLY_EQUATION(position.z);
			APPLY_EQUATION(size.x);
			APPLY_EQUATION(size.y);
			APPLY_EQUATION(size.z);
			return r;
		}

		case Variant::BASIS: {
			Basis i = p_initial_val;
			Basis d = p_delta_val;
			Basis r;

			APPLY_EQUATION(elements[0][0]);
			APPLY_EQUATION(elements[0][1]);
			APPLY_EQUATION(elements[0][2]);
			APPLY_EQUATION(elements[1][0]);
			APPLY_EQUATION(elements[1][1]);
			APPLY_EQUATION(elements[1][2]);
			APPLY_EQUATION(elements[2][0]);
			APPLY_EQUATION(elements[2][1]);
			APPLY_EQUATION(elements[2][2]);
			return r;
		}

		case Variant::TRANSFORM: {
			Transform i = p_initial_val;
			Transform d = p_delta_val;
			Transform r;

			APPLY_EQUATION(basis.elements[0][0]);
			APPLY_EQUATION(basis.elements[0][1]);
			APPLY_EQUATION(basis.elements[0][2]);
			APPLY_EQUATION(basis.elements[1][0]);
			APPLY_EQUATION(basis.elements[1][1]);
			APPLY_EQUATION(basis.elements[1][2]);
			APPLY_EQUATION(basis.elements[2][0]);
			APPLY_EQUATION(basis.elements[2][1]);
			APPLY_EQUATION(basis.elements[2][2]);
			APPLY_EQUATION(origin.x);
			APPLY_EQUATION(origin.y);
			APPLY_EQUATION(origin.z);
			return r;
		}

		case Variant::COLOR: {
			Color i = p_initial_val;
			Color d = p_delta_val;
			Color r;

			APPLY_EQUATION(r);
			APPLY_EQUATION(g);
			APPLY_EQUATION(b);
			APPLY_EQUATION(a);
			return r;
		}

		default: {
			return p_initial_val;
		}
	};
#undef APPLY_EQUATION
}

Variant SceneTreeTween::calculate_delta_value(Variant p_intial_val, Variant p_final_val) {
	ERR_FAIL_COND_V_MSG(p_intial_val.get_type() != p_final_val.get_type(), p_intial_val, "Type mismatch between initial and final value: " + Variant::get_type_name(p_intial_val.get_type()) + " and " + Variant::get_type_name(p_final_val.get_type()));

	switch (p_intial_val.get_type()) {
		case Variant::BOOL: {
			return (int)p_final_val - (int)p_intial_val;
		}

		case Variant::RECT2: {
			Rect2 i = p_intial_val;
			Rect2 f = p_final_val;
			return Rect2(f.position - i.position, f.size - i.size);
		}

		case Variant::TRANSFORM2D: {
			Transform2D i = p_intial_val;
			Transform2D f = p_final_val;
			return Transform2D(f.elements[0][0] - i.elements[0][0],
					f.elements[0][1] - i.elements[0][1],
					f.elements[1][0] - i.elements[1][0],
					f.elements[1][1] - i.elements[1][1],
					f.elements[2][0] - i.elements[2][0],
					f.elements[2][1] - i.elements[2][1]);
		}

		case Variant::AABB: {
			AABB i = p_intial_val;
			AABB f = p_final_val;
			return AABB(f.position - i.position, f.size - i.size);
		}

		case Variant::BASIS: {
			Basis i = p_intial_val;
			Basis f = p_final_val;
			return Basis(f.elements[0][0] - i.elements[0][0],
					f.elements[0][1] - i.elements[0][1],
					f.elements[0][2] - i.elements[0][2],
					f.elements[1][0] - i.elements[1][0],
					f.elements[1][1] - i.elements[1][1],
					f.elements[1][2] - i.elements[1][2],
					f.elements[2][0] - i.elements[2][0],
					f.elements[2][1] - i.elements[2][1],
					f.elements[2][2] - i.elements[2][2]);
		}

		case Variant::TRANSFORM: {
			Transform i = p_intial_val;
			Transform f = p_final_val;
			return Transform(f.basis.elements[0][0] - i.basis.elements[0][0],
					f.basis.elements[0][1] - i.basis.elements[0][1],
					f.basis.elements[0][2] - i.basis.elements[0][2],
					f.basis.elements[1][0] - i.basis.elements[1][0],
					f.basis.elements[1][1] - i.basis.elements[1][1],
					f.basis.elements[1][2] - i.basis.elements[1][2],
					f.basis.elements[2][0] - i.basis.elements[2][0],
					f.basis.elements[2][1] - i.basis.elements[2][1],
					f.basis.elements[2][2] - i.basis.elements[2][2],
					f.origin.x - i.origin.x,
					f.origin.y - i.origin.y,
					f.origin.z - i.origin.z);
		}

		default: {
			return Variant::evaluate(Variant::OP_SUBTRACT, p_final_val, p_intial_val);
		}
	};
}

void SceneTreeTween::_bind_methods() {
	ClassDB::bind_method(D_METHOD("tween_property", "object", "property", "final_val", "duration"), &SceneTreeTween::tween_property);
	ClassDB::bind_method(D_METHOD("tween_interval", "time"), &SceneTreeTween::tween_interval);
	ClassDB::bind_method(D_METHOD("tween_callback", "object", "method", "binds"), &SceneTreeTween::tween_callback, DEFVAL(Array()));
	ClassDB::bind_method(D_METHOD("tween_method", "object", "method", "from", "to", "duration", "binds"), &SceneTreeTween::tween_method, DEFVAL(Array()));
	ClassDB::bind_method(D_METHOD("tween_yield", "object", "signal"), &SceneTreeTween::tween_yield);

	ClassDB::bind_method(D_METHOD("custom_step", "delta"), &SceneTreeTween::custom_step);
	ClassDB::bind_method(D_METHOD("stop"), &SceneTreeTween::stop);
	ClassDB::bind_method(D_METHOD("pause"), &SceneTreeTween::pause);
	ClassDB::bind_method(D_METHOD("play"), &SceneTreeTween::play);
	ClassDB::bind_method(D_METHOD("kill"), &SceneTreeTween::kill);
	ClassDB::bind_method(D_METHOD("get_total_elapsed_time"), &SceneTreeTween::get_total_time);

	ClassDB::bind_method(D_METHOD("is_running"), &SceneTreeTween::is_running);
	ClassDB::bind_method(D_METHOD("is_valid"), &SceneTreeTween::is_valid);
	ClassDB::bind_method(D_METHOD("bind_node", "node"), &SceneTreeTween::bind_node);
	ClassDB::bind_method(D_METHOD("set_process_mode", "mode"), &SceneTreeTween::set_process_mode);
	ClassDB::bind_method(D_METHOD("set_pause_mode", "mode"), &SceneTreeTween::set_pause_mode);

	ClassDB::bind_method(D_METHOD("set_parallel", "parallel"), &SceneTreeTween::set_parallel, DEFVAL(true));
	ClassDB::bind_method(D_METHOD("set_loops", "loops"), &SceneTreeTween::set_loops, DEFVAL(0));
	ClassDB::bind_method(D_METHOD("set_speed_scale", "speed"), &SceneTreeTween::set_speed_scale);
	ClassDB::bind_method(D_METHOD("set_trans", "trans"), &SceneTreeTween::set_trans);
	ClassDB::bind_method(D_METHOD("set_ease", "ease"), &SceneTreeTween::set_ease);

	ClassDB::bind_method(D_METHOD("parallel"), &SceneTreeTween::parallel);
	ClassDB::bind_method(D_METHOD("chain"), &SceneTreeTween::chain);

	ClassDB::bind_method(D_METHOD("interpolate_value", "initial_value", "delta_value", "elapsed_time", "duration", "trans_type", "ease_type"), &SceneTreeTween::interpolate_variant);

	ADD_SIGNAL(MethodInfo("step_finished", PropertyInfo(Variant::INT, "idx")));
	ADD_SIGNAL(MethodInfo("loop_finished", PropertyInfo(Variant::INT, "loop_count")));
	ADD_SIGNAL(MethodInfo("finished"));

	BIND_ENUM_CONSTANT(TWEEN_PAUSE_BOUND);
	BIND_ENUM_CONSTANT(TWEEN_PAUSE_STOP);
	BIND_ENUM_CONSTANT(TWEEN_PAUSE_PROCESS);
}

SceneTreeTween::SceneTreeTween(bool p_valid) {
	valid = p_valid;
}

Ref<PropertyTweener> PropertyTweener::from(Variant p_value) {
	initial_val = p_value;
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

Ref<PropertyTweener> PropertyTweener::set_delay(float p_delay) {
	delay = p_delay;
	return this;
}

void PropertyTweener::start() {
	elapsed_time = 0;
	finished = false;

	Object *target_instance = ObjectDB::get_instance(target);
	if (!target_instance) {
		WARN_PRINT("Target object freed before starting, aborting Tweener.");
		return;
	}

	if (do_continue) {
		initial_val = target_instance->get_indexed(property);
	}

	if (relative) {
		final_val = Variant::evaluate(Variant::Operator::OP_ADD, initial_val, base_final_val);
	}

	delta_val = tween->calculate_delta_value(initial_val, final_val);
}

bool PropertyTweener::step(float &r_delta) {
	if (finished) {
		// This is needed in case there's a parallel Tweener with longer duration.
		return false;
	}

	Object *target_instance = ObjectDB::get_instance(target);
	if (!target_instance) {
		return false;
	}
	elapsed_time += r_delta;

	if (elapsed_time < delay) {
		r_delta = 0;
		return true;
	}

	float time = MIN(elapsed_time - delay, duration);
	if (time < duration) {
		target_instance->set_indexed(property, tween->interpolate_variant(initial_val, delta_val, time, duration, trans_type, ease_type));
		r_delta = 0;
		return true;
	} else {
		target_instance->set_indexed(property, final_val);
		finished = true;
		r_delta = elapsed_time - delay - duration;
		emit_signal(SceneStringNames::get_singleton()->finished);
		return false;
	}
}

void PropertyTweener::set_tween(Ref<SceneTreeTween> p_tween) {
	tween = p_tween;
	if (trans_type == Tween::TRANS_COUNT) {
		trans_type = tween->get_trans();
	}
	if (ease_type == Tween::EASE_COUNT) {
		ease_type = tween->get_ease();
	}
}

void PropertyTweener::_bind_methods() {
	ClassDB::bind_method(D_METHOD("from", "value"), &PropertyTweener::from);
	ClassDB::bind_method(D_METHOD("from_current"), &PropertyTweener::from_current);
	ClassDB::bind_method(D_METHOD("as_relative"), &PropertyTweener::as_relative);
	ClassDB::bind_method(D_METHOD("set_trans", "trans"), &PropertyTweener::set_trans);
	ClassDB::bind_method(D_METHOD("set_ease", "ease"), &PropertyTweener::set_ease);
	ClassDB::bind_method(D_METHOD("set_delay", "delay"), &PropertyTweener::set_delay);
}

PropertyTweener::PropertyTweener(Object *p_target, NodePath p_property, Variant p_to, float p_duration) {
	target = p_target->get_instance_id();
	property = p_property.get_as_property_path().get_subnames();
	initial_val = p_target->get_indexed(property);
	base_final_val = p_to;
	final_val = base_final_val;
	duration = p_duration;
}

PropertyTweener::PropertyTweener() {
	ERR_FAIL_MSG("Can't create empty PropertyTweener. Use get_tree().tween_property() or tween_property() instead.");
}

void IntervalTweener::start() {
	elapsed_time = 0;
	finished = false;
}

bool IntervalTweener::step(float &r_delta) {
	if (finished) {
		return false;
	}

	elapsed_time += r_delta;

	if (elapsed_time < duration) {
		r_delta = 0;
		return true;
	} else {
		finished = true;
		r_delta = elapsed_time - duration;
		emit_signal(SceneStringNames::get_singleton()->finished);
		return false;
	}
}

IntervalTweener::IntervalTweener(float p_time) {
	duration = p_time;
}

IntervalTweener::IntervalTweener() {
	ERR_FAIL_MSG("Can't create empty IntervalTweener. Use get_tree().tween_property() or tween_property() instead.");
}

Ref<CallbackTweener> CallbackTweener::set_delay(float p_delay) {
	delay = p_delay;
	return this;
}

void CallbackTweener::start() {
	elapsed_time = 0;
	finished = false;
}

bool CallbackTweener::step(float &r_delta) {
	if (finished) {
		return false;
	}

	Object *target_instance = ObjectDB::get_instance(target);
	if (!target_instance) {
		return false;
	}

	elapsed_time += r_delta;
	if (elapsed_time >= delay) {
		Vector<const Variant *> bind_mem;

		if (binds.size()) {
			bind_mem.resize(binds.size());

			for (int i = 0; i < binds.size(); i++) {
				bind_mem.write[i] = &binds[i];
			}
		}

		const Variant **args = (const Variant **)bind_mem.ptr();
		int argc = bind_mem.size();

		Variant::CallError ce;
		target_instance->call(method, args, argc, ce);
		if (ce.error != Variant::CallError::CALL_OK) {
			ERR_FAIL_V_MSG(false, "Error calling method from CallbackTweener: " + Variant::get_call_error_text(target_instance, method, args, argc, ce));
		}

		finished = true;
		r_delta = elapsed_time - delay;
		emit_signal(SceneStringNames::get_singleton()->finished);
		return false;
	}

	r_delta = 0;
	return true;
}

void CallbackTweener::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_delay", "delay"), &CallbackTweener::set_delay);
}

CallbackTweener::CallbackTweener(Object *p_target, StringName p_method, const Vector<Variant> &p_binds) {
	target = p_target->get_instance_id();
	method = p_method;
	binds = p_binds;
}

CallbackTweener::CallbackTweener() {
	ERR_FAIL_MSG("Can't create empty CallbackTweener. Use get_tree().tween_callback() instead.");
}

Ref<MethodTweener> MethodTweener::set_delay(float p_delay) {
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

void MethodTweener::start() {
	elapsed_time = 0;
	finished = false;
}

bool MethodTweener::step(float &r_delta) {
	if (finished) {
		return false;
	}

	Object *target_instance = ObjectDB::get_instance(target);
	if (!target_instance) {
		return false;
	}

	elapsed_time += r_delta;

	if (elapsed_time < delay) {
		r_delta = 0;
		return true;
	}

	Variant current_val;
	float time = MIN(elapsed_time - delay, duration);
	if (time < duration) {
		current_val = tween->interpolate_variant(initial_val, delta_val, time, duration, trans_type, ease_type);
	} else {
		current_val = final_val;
	}

	Vector<const Variant *> bind_mem;

	if (binds.empty()) {
		bind_mem.push_back(&current_val);
	} else {
		bind_mem.resize(1 + binds.size());

		bind_mem.write[0] = &current_val;
		for (int i = 0; i < binds.size(); i++) {
			bind_mem.write[1 + i] = &binds[i];
		}
	}

	const Variant **args = (const Variant **)bind_mem.ptr();
	int argc = bind_mem.size();

	Variant::CallError ce;
	target_instance->call(method, args, argc, ce);
	if (ce.error != Variant::CallError::CALL_OK) {
		ERR_FAIL_V_MSG(false, "Error calling method from MethodTweener: " + Variant::get_call_error_text(target_instance, method, args, argc, ce));
	}

	if (time < duration) {
		r_delta = 0;
		return true;
	} else {
		finished = true;
		r_delta = elapsed_time - delay - duration;
		emit_signal(SceneStringNames::get_singleton()->finished);
		return false;
	}
}

void MethodTweener::set_tween(Ref<SceneTreeTween> p_tween) {
	tween = p_tween;
	if (trans_type == Tween::TRANS_COUNT) {
		trans_type = tween->get_trans();
	}
	if (ease_type == Tween::EASE_COUNT) {
		ease_type = tween->get_ease();
	}
}

void MethodTweener::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_delay", "delay"), &MethodTweener::set_delay);
	ClassDB::bind_method(D_METHOD("set_trans", "trans"), &MethodTweener::set_trans);
	ClassDB::bind_method(D_METHOD("set_ease", "ease"), &MethodTweener::set_ease);
}

MethodTweener::MethodTweener(Object *p_target, StringName p_method, Variant p_from, Variant p_to, float p_duration, const Vector<Variant> &p_binds) {
	target = p_target->get_instance_id();
	method = p_method;
	binds = p_binds;
	initial_val = p_from;
	delta_val = tween->calculate_delta_value(p_from, p_to);
	final_val = p_to;
	duration = p_duration;
}

MethodTweener::MethodTweener() {
	ERR_FAIL_MSG("Can't create empty MethodTweener. Use get_tree().tween_method() instead.");
}

Ref<YieldTweener> YieldTweener::set_timeout(float p_timeout) {
	timeout = p_timeout;
	return this;
}

void YieldTweener::start() {
	Object *target_instance = ObjectDB::get_instance(target);
	if (!target_instance) {
		return;
	}

	target_instance->connect(signal, this, "_signal_received");
}

bool YieldTweener::step(float &r_delta) {
	if (received) {
		return false;
	}

	if (timeout >= 0) {
		timeout -= r_delta;
		if (timeout <= 0) {
			return false;
		}
	}

	Object *target_instance = ObjectDB::get_instance(target);
	if (!target_instance) {
		return false;
	}

	r_delta = 0; // "Consume" all remaining time to prevent infinite loops.
	return true;
}

YieldTweener::YieldTweener(Object *p_target, StringName p_signal) {
	target = p_target->get_instance_id();
	signal = p_signal;
}

YieldTweener::YieldTweener() {
	ERR_FAIL_MSG("Can't create empty YieldTweener. Use get_tree().create_tween().tween_yield() instead.");
}

Variant YieldTweener::_signal_received(const Variant **p_args, int p_argcount, Variant::CallError &r_error) {
	received = true;
	return Variant();
}

void YieldTweener::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_timeout", "timeout"), &YieldTweener::set_timeout);
	ClassDB::bind_vararg_method(METHOD_FLAGS_DEFAULT, "_signal_received", &YieldTweener::_signal_received, MethodInfo("_signal_received"));
}

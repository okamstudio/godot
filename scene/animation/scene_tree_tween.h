/**************************************************************************/
/*  scene_tree_tween.h                                                    */
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

#ifndef SCENE_TREE_TWEEN_H
#define SCENE_TREE_TWEEN_H

#include "core/reference.h"
#include "scene/animation/tween.h"

class SceneTreeTween;

class Tweener : public Reference {
	GDCLASS(Tweener, Reference);

public:
	virtual void set_tween(Ref<SceneTreeTween> p_tween);
	virtual void start() = 0;
	virtual bool step(float &r_delta) = 0;
	void clear_tween();

protected:
	static void _bind_methods();
	Ref<SceneTreeTween> tween;
	float elapsed_time = 0;
	bool finished = false;
};

class PropertyTweener;
class IntervalTweener;
class CallbackTweener;
class MethodTweener;
class YieldTweener;

class SceneTreeTween : public Reference {
	GDCLASS(SceneTreeTween, Reference);

public:
	enum TweenPauseMode {
		TWEEN_PAUSE_BOUND,
		TWEEN_PAUSE_STOP,
		TWEEN_PAUSE_PROCESS,
	};

private:
	Tween::TweenProcessMode process_mode = Tween::TWEEN_PROCESS_IDLE;
	TweenPauseMode pause_mode = TweenPauseMode::TWEEN_PAUSE_BOUND;
	Tween::TransitionType default_transition = Tween::TRANS_LINEAR;
	Tween::EaseType default_ease = Tween::EASE_IN_OUT;
	ObjectID bound_node;

	Vector<List<Ref<Tweener>>> tweeners;
	float total_time = 0;
	int current_step = -1;
	int loops = 1;
	int loops_done = 0;
	float speed_scale = 1;

	bool is_bound = false;
	bool started = false;
	bool running = true;
	bool dead = false;
	bool valid = false;
	bool default_parallel = false;
	bool parallel_enabled = false;
#ifdef DEBUG_ENABLED
	bool is_infinite = false;
#endif

	void start_tweeners();

protected:
	static void _bind_methods();

public:
	Ref<PropertyTweener> tween_property(Object *p_target, NodePath p_property, Variant p_to, float p_duration);
	Ref<IntervalTweener> tween_interval(float p_time);
	Ref<CallbackTweener> tween_callback(Object *p_target, StringName p_method, const Vector<Variant> &p_binds = Vector<Variant>());
	Ref<MethodTweener> tween_method(Object *p_target, StringName p_method, Variant p_from, Variant p_to, float p_duration, const Vector<Variant> &p_binds = Vector<Variant>());
	Ref<YieldTweener> tween_yield(Object *p_target, StringName p_signal);
	void append(Ref<Tweener> p_tweener);

	bool custom_step(float p_delta);
	void stop();
	void pause();
	void play();
	void kill();

	bool is_running() const;
	bool is_valid() const;
	void clear();

	Tween::TweenProcessMode get_process_mode() const;
	TweenPauseMode get_pause_mode() const;
	Tween::TransitionType get_trans() const;
	Tween::EaseType get_ease() const;

	Ref<SceneTreeTween> bind_node(Node *p_node);
	Ref<SceneTreeTween> set_process_mode(Tween::TweenProcessMode p_mode);
	Ref<SceneTreeTween> set_pause_mode(TweenPauseMode p_mode);
	Ref<SceneTreeTween> set_parallel(bool p_parallel);
	Ref<SceneTreeTween> set_loops(int p_loops);
	Ref<SceneTreeTween> set_speed_scale(float p_speed);
	Ref<SceneTreeTween> set_trans(Tween::TransitionType p_trans);
	Ref<SceneTreeTween> set_ease(Tween::EaseType p_ease);

	Ref<SceneTreeTween> parallel();
	Ref<SceneTreeTween> chain();

	Variant interpolate_variant(Variant p_initial_val, Variant p_delta_val, float p_time, float p_duration, Tween::TransitionType p_trans, Tween::EaseType p_ease) const;
	Variant calculate_delta_value(Variant p_intial_val, Variant p_final_val);

	bool step(float p_delta);
	bool can_process(bool p_tree_paused) const;
	Node *get_bound_node() const;
	float get_total_time() const;

	SceneTreeTween() = default;
	SceneTreeTween(bool p_valid);
};

VARIANT_ENUM_CAST(SceneTreeTween::TweenPauseMode);

class PropertyTweener : public Tweener {
	GDCLASS(PropertyTweener, Tweener);

public:
	Ref<PropertyTweener> from(Variant p_value);
	Ref<PropertyTweener> from_current();
	Ref<PropertyTweener> as_relative();
	Ref<PropertyTweener> set_trans(Tween::TransitionType p_trans);
	Ref<PropertyTweener> set_ease(Tween::EaseType p_ease);
	Ref<PropertyTweener> set_delay(float p_delay);

	virtual void set_tween(Ref<SceneTreeTween> p_tween);
	virtual void start();
	virtual bool step(float &r_delta);

	PropertyTweener(Object *p_target, NodePath p_property, Variant p_to, float p_duration);
	PropertyTweener();

protected:
	static void _bind_methods();

private:
	ObjectID target;
	Vector<StringName> property;
	Variant initial_val;
	Variant base_final_val;
	Variant final_val;
	Variant delta_val;

	float duration = 0;
	Tween::TransitionType trans_type = Tween::TRANS_COUNT; // This is set inside set_tween();
	Tween::EaseType ease_type = Tween::EASE_COUNT;

	float delay = 0;
	bool do_continue = true;
	bool relative = false;
};

class IntervalTweener : public Tweener {
	GDCLASS(IntervalTweener, Tweener);

public:
	virtual void start();
	virtual bool step(float &r_delta);

	IntervalTweener(float p_time);
	IntervalTweener();

private:
	float duration = 0;
};

class CallbackTweener : public Tweener {
	GDCLASS(CallbackTweener, Tweener);

public:
	Ref<CallbackTweener> set_delay(float p_delay);

	virtual void start();
	virtual bool step(float &r_delta);

	CallbackTweener(Object *p_target, StringName p_method, const Vector<Variant> &p_binds);
	CallbackTweener();

protected:
	static void _bind_methods();

private:
	ObjectID target;
	StringName method;
	Vector<Variant> binds;
	int args = 0;
	float delay = 0;
};

class MethodTweener : public Tweener {
	GDCLASS(MethodTweener, Tweener);

public:
	Ref<MethodTweener> set_trans(Tween::TransitionType p_trans);
	Ref<MethodTweener> set_ease(Tween::EaseType p_ease);
	Ref<MethodTweener> set_delay(float p_delay);

	virtual void set_tween(Ref<SceneTreeTween> p_tween);
	virtual void start();
	virtual bool step(float &r_delta);

	MethodTweener(Object *p_target, StringName p_method, Variant p_from, Variant p_to, float p_duration, const Vector<Variant> &p_binds);
	MethodTweener();

protected:
	static void _bind_methods();

private:
	float duration = 0;
	float delay = 0;
	Tween::TransitionType trans_type = Tween::TRANS_COUNT;
	Tween::EaseType ease_type = Tween::EASE_COUNT;

	Ref<SceneTreeTween> tween;
	Variant initial_val;
	Variant delta_val;
	Variant final_val;
	ObjectID target;
	StringName method;
	Vector<Variant> binds;
};

class YieldTweener : public Tweener {
	GDCLASS(YieldTweener, Tweener);

public:
	Ref<YieldTweener> set_timeout(float p_timeout);

	virtual void start();
	virtual bool step(float &r_delta);

	Variant _signal_received(const Variant **p_args, int p_argcount, Variant::CallError &r_error);

	YieldTweener(Object *p_target, StringName p_signal);
	YieldTweener();

protected:
	static void _bind_methods();

private:
	bool received = false;
	float timeout = -1;

	ObjectID target;
	StringName signal;
};

#endif // SCENE_TREE_TWEEN_H

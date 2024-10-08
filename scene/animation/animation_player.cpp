/**************************************************************************/
/*  animation_player.cpp                                                  */
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

#include "animation_player.h"
#include "animation_player.compat.inc"

#include "core/config/engine.h"

bool AnimationPlayer::_set(const StringName &p_name, const Variant &p_value) {
	String name = p_name;
	if (name.begins_with("playback/play")) { // For backward compatibility.
		set_current_animation(p_value);
	} else if (name.begins_with("next/")) {
		String which = name.get_slicec('/', 1);
		animation_set_next(which, p_value);
	} else if (p_name == SceneStringName(blend_times)) {
		Array array = p_value;
		int len = array.size();
		ERR_FAIL_COND_V(len % 3, false);

		for (int i = 0; i < len / 3; i++) {
			StringName from = array[i * 3 + 0];
			StringName to = array[i * 3 + 1];
			float time = array[i * 3 + 2];
			set_blend_time(from, to, time);
		}
#ifndef DISABLE_DEPRECATED
	} else if (p_name == "method_call_mode") {
		set_callback_mode_method(static_cast<AnimationCallbackModeMethod>((int)p_value));
	} else if (p_name == "playback_process_mode") {
		set_callback_mode_process(static_cast<AnimationCallbackModeProcess>((int)p_value));
	} else if (p_name == "playback_active") {
		set_active(p_value);
#endif // DISABLE_DEPRECATED
	} else {
		return false;
	}
	return true;
}

bool AnimationPlayer::_get(const StringName &p_name, Variant &r_ret) const {
	String name = p_name;

	if (name == "playback/play") { // For backward compatibility.

		r_ret = get_current_animation();

	} else if (name.begins_with("next/")) {
		String which = name.get_slicec('/', 1);
		r_ret = animation_get_next(which);

	} else if (p_name == SceneStringName(blend_times)) {
		Vector<BlendKey> keys;
		for (const KeyValue<BlendKey, double> &E : blend_times) {
			keys.ordered_insert(E.key);
		}

		Array array;
		for (int i = 0; i < keys.size(); i++) {
			array.push_back(keys[i].from);
			array.push_back(keys[i].to);
			array.push_back(blend_times.get(keys[i]));
		}

		r_ret = array;
#ifndef DISABLE_DEPRECATED
	} else if (name == "method_call_mode") {
		r_ret = get_callback_mode_method();
	} else if (name == "playback_process_mode") {
		r_ret = get_callback_mode_process();
	} else if (name == "playback_active") {
		r_ret = is_active();
#endif // DISABLE_DEPRECATED
	} else {
		return false;
	}

	return true;
}

void AnimationPlayer::_validate_property(PropertyInfo &p_property) const {
	AnimationMixer::_validate_property(p_property);

	if (p_property.name == "current_animation") {
		List<String> names;

		for (const KeyValue<StringName, AnimationData> &E : animation_set) {
			names.push_back(E.key);
		}
		names.push_front("[stop]");
		String hint;
		for (List<String>::Element *E = names.front(); E; E = E->next()) {
			if (E != names.front()) {
				hint += ",";
			}
			hint += E->get();
		}

		p_property.hint_string = hint;
	} else if (!auto_capture && p_property.name.begins_with("playback_auto_capture_")) {
		p_property.usage = PROPERTY_USAGE_NONE;
	}
}

void AnimationPlayer::_get_property_list(List<PropertyInfo> *p_list) const {
	List<PropertyInfo> anim_names;

	for (const KeyValue<StringName, AnimationData> &E : animation_set) {
		HashMap<StringName, StringName>::ConstIterator F = animation_next_set.find(E.key);
		if (F && F->value != StringName()) {
			anim_names.push_back(PropertyInfo(Variant::STRING, "next/" + String(E.key), PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NO_EDITOR | PROPERTY_USAGE_INTERNAL));
		}
	}

	for (const PropertyInfo &E : anim_names) {
		p_list->push_back(E);
	}

	p_list->push_back(PropertyInfo(Variant::ARRAY, "blend_times", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NO_EDITOR | PROPERTY_USAGE_INTERNAL));
}

void AnimationPlayer::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY: {
			if (!Engine::get_singleton()->is_editor_hint() && animation_set.has(autoplay)) {
				set_active(active);
				play(autoplay);
				_check_immediately_after_start();
			}
		} break;
	}
}

void AnimationPlayer::_process_playback_data(PlaybackData &cd, double p_delta, float p_blend, bool p_seeked, bool p_internal_seeked, bool p_started, bool p_is_current) {
	double speed = speed_scale * cd.speed_scale;
	bool backwards = signbit(speed); // Negative zero means playing backwards too.
	double delta = p_started ? 0 : p_delta * speed;
	double next_pos = cd.pos + delta;

	double start = get_section_start_time();
	double end = get_section_end_time();

	Animation::LoopedFlag looped_flag = Animation::LOOPED_FLAG_NONE;

	switch (cd.from->animation->get_loop_mode()) {
		case Animation::LOOP_NONE: {
			if (Animation::is_less_approx(next_pos, start)) {
				next_pos = start;
			} else if (Animation::is_greater_approx(next_pos, end)) {
				next_pos = end;
			}
			delta = next_pos - cd.pos; // Fix delta (after determination of backwards because negative zero is lost here).
		} break;

		case Animation::LOOP_LINEAR: {
			if (Animation::is_less_approx(next_pos, start) && Animation::is_greater_or_equal_approx(cd.pos, start)) {
				looped_flag = Animation::LOOPED_FLAG_START;
			}
			if (Animation::is_greater_approx(next_pos, end) && Animation::is_less_or_equal_approx(cd.pos, end)) {
				looped_flag = Animation::LOOPED_FLAG_END;
			}
			next_pos = Math::fposmod(next_pos - start, end - start) + start;
		} break;

		case Animation::LOOP_PINGPONG: {
			if (Animation::is_less_approx(next_pos, start) && Animation::is_greater_or_equal_approx(cd.pos, start)) {
				cd.speed_scale *= -1.0;
				looped_flag = Animation::LOOPED_FLAG_START;
			}
			if (Animation::is_greater_approx(next_pos, end) && Animation::is_less_or_equal_approx(cd.pos, end)) {
				cd.speed_scale *= -1.0;
				looped_flag = Animation::LOOPED_FLAG_END;
			}
			next_pos = Math::pingpong(next_pos - start, end - start) + start;
		} break;

		default:
			break;
	}

	double prev_pos = cd.pos; // The animation may be changed during process, so it is safer that the state is changed before process.

	// End detection.
	if (p_is_current) {
		if (cd.from->animation->get_loop_mode() == Animation::LOOP_NONE) {
			if (!backwards && Animation::is_less_or_equal_approx(prev_pos, end) && Math::is_equal_approx(next_pos, end)) {
				// Playback finished.
				next_pos = end; // Snap to the edge.
				end_reached = true;
				end_notify = Animation::is_less_approx(prev_pos, end); // Notify only if not already at the end.
				p_blend = 1.0;
			}
			if (backwards && Animation::is_greater_or_equal_approx(prev_pos, start) && Math::is_equal_approx(next_pos, start)) {
				// Playback finished.
				next_pos = start; // Snap to the edge.
				end_reached = true;
				end_notify = Animation::is_greater_approx(prev_pos, start); // Notify only if not already at the beginning.
				p_blend = 1.0;
			}
		}
	}

	cd.pos = next_pos;

	PlaybackInfo pi;
	if (p_started) {
		pi.time = prev_pos;
		pi.delta = 0;
		pi.start = start;
		pi.end = end;
		pi.seeked = true;
	} else {
		pi.time = next_pos;
		pi.delta = delta;
		pi.start = start;
		pi.end = end;
		pi.seeked = p_seeked;
	}
	if (Math::is_zero_approx(pi.delta) && backwards) {
		pi.delta = -0.0; // Sign is needed to handle converted Continuous track from Discrete track correctly.
	}
	// Immediately after playback, discrete keys should be retrieved with EXACT mode since behind keys must be ignored at that time.
	pi.is_external_seeking = !p_internal_seeked && !p_started;
	pi.looped_flag = looped_flag;
	pi.weight = p_blend;
	make_animation_instance(cd.from->name, pi);
}

float AnimationPlayer::get_current_blend_amount() {
	Playback &c = playback;
	float blend = 1.0;
	for (List<Blend>::Element *E = c.blend.front(); E; E = E->next()) {
		Blend &b = E->get();
		blend = blend - b.blend_left;
	}
	return MAX(0, blend);
}

void AnimationPlayer::_blend_playback_data(double p_delta, bool p_started) {
	Playback &c = playback;

	bool seeked = c.seeked; // The animation may be changed during process, so it is safer that the state is changed before process.
	bool internal_seeked = c.internal_seeked;

	if (!Math::is_zero_approx(p_delta)) {
		c.seeked = false;
		c.internal_seeked = false;
	}

	// Second, process current animation to check if the animation end reached.
	_process_playback_data(c.current, p_delta, get_current_blend_amount(), seeked, internal_seeked, p_started, true);

	// Finally, if not end the animation, do blending.
	if (end_reached) {
		playback.blend.clear();
		return;
	}
	List<List<Blend>::Element *> to_erase;
	for (List<Blend>::Element *E = c.blend.front(); E; E = E->next()) {
		Blend &b = E->get();
		b.blend_left = MAX(0, b.blend_left - Math::absf(speed_scale * p_delta) / b.blend_time);
		if (Animation::is_less_or_equal_approx(b.blend_left, 0)) {
			to_erase.push_back(E);
			b.blend_left = CMP_EPSILON; // May want to play last frame.
		}
		// Note: There may be issues if an animation event triggers an animation change while this blend is active,
		// so it is best to use "deferred" calls instead of "immediate" for animation events that can trigger new animations.
		_process_playback_data(b.data, p_delta, b.blend_left, false, false, false);
	}
	for (List<Blend>::Element *&E : to_erase) {
		c.blend.erase(E);
	}
}

bool AnimationPlayer::_blend_pre_process(double p_delta, int p_track_count, const HashMap<NodePath, int> &p_track_map) {
	if (!playback.current.from) {
		_set_process(false);
		return false;
	}

	tmp_from = playback.current.from->animation->get_instance_id();
	end_reached = false;
	end_notify = false;

	bool started = playback.started; // The animation may be changed during process, so it is safer that the state is changed before process.
	if (playback.started) {
		playback.started = false;
	}

	AnimationData *prev_from = playback.current.from;
	_blend_playback_data(p_delta, started);

	if (prev_from != playback.current.from) {
		return false; // Animation has been changed in the process (may be caused by method track), abort process.
	}

	return true;
}

void AnimationPlayer::_blend_capture(double p_delta) {
	blend_capture(p_delta * Math::abs(speed_scale));
}

void AnimationPlayer::_blend_post_process() {
	if (end_reached) {
		// If the method track changes current animation, the animation is not finished.
		if (tmp_from == playback.current.from->animation->get_instance_id()) {
			if (playback_queue.size()) {
				String old = playback.assigned;
				play(playback_queue.front()->get());
				String new_name = playback.assigned;
				playback_queue.pop_front();
				if (end_notify) {
					emit_signal(SceneStringName(animation_changed), old, new_name);
				}
			} else {
				_clear_caches();
				playing = false;
				_set_process(false);
				if (end_notify) {
					emit_signal(SceneStringName(animation_finished), playback.assigned);
					if (movie_quit_on_finish && OS::get_singleton()->has_feature("movie")) {
						print_line(vformat("Movie Maker mode is enabled. Quitting on animation finish as requested by: %s", get_path()));
						get_tree()->quit();
					}
				}
			}
		}
		end_reached = false;
		end_notify = false;
	}
	tmp_from = ObjectID();
}

void AnimationPlayer::queue(const StringName &p_name) {
	if (!is_playing()) {
		play(p_name);
	} else {
		playback_queue.push_back(p_name);
	}
}

Vector<String> AnimationPlayer::get_queue() {
	Vector<String> ret;
	for (const StringName &E : playback_queue) {
		ret.push_back(E);
	}

	return ret;
}

void AnimationPlayer::clear_queue() {
	playback_queue.clear();
}

void AnimationPlayer::play_backwards(const StringName &p_name, double p_custom_blend) {
	play(p_name, p_custom_blend, -1, true);
}

void AnimationPlayer::play_section_with_markers_backwards(const StringName &p_name, const StringName &p_start_marker, const StringName &p_end_marker, double p_custom_blend) {
	play_section_with_markers(p_name, p_start_marker, p_end_marker, p_custom_blend, -1, true);
}

void AnimationPlayer::play_section_backwards(const StringName &p_name, double p_start_time, double p_end_time, double p_custom_blend) {
	play_section(p_name, p_start_time, p_end_time, -1, true);
}

void AnimationPlayer::play(const StringName &p_name, double p_custom_blend, float p_custom_scale, bool p_from_end) {
	if (auto_capture) {
		play_with_capture(p_name, auto_capture_duration, p_custom_blend, p_custom_scale, p_from_end, auto_capture_transition_type, auto_capture_ease_type);
	} else {
		_play(p_name, p_custom_blend, p_custom_scale, p_from_end);
	}
}

void AnimationPlayer::_play(const StringName &p_name, double p_custom_blend, float p_custom_scale, bool p_from_end) {
	play_section_with_markers(p_name, StringName(), StringName(), p_custom_blend, p_custom_scale, p_from_end);
}

void AnimationPlayer::play_section_with_markers(const StringName &p_name, const StringName &p_start_marker, const StringName &p_end_marker, double p_custom_blend, float p_custom_scale, bool p_from_end) {
	StringName name = p_name;

	if (name == StringName()) {
		name = playback.assigned;
	}

	ERR_FAIL_COND_MSG(!animation_set.has(name), vformat("Animation not found: %s.", name));

	Ref<Animation> animation = animation_set[name].animation;

	ERR_FAIL_COND_MSG(p_start_marker == p_end_marker && p_start_marker, vformat("Start marker and end marker cannot be the same marker: %s.", p_start_marker));
	ERR_FAIL_COND_MSG(p_start_marker && !animation->has_marker(p_start_marker), vformat("Marker %s not found in animation: %s.", p_start_marker, name));
	ERR_FAIL_COND_MSG(p_end_marker && !animation->has_marker(p_end_marker), vformat("Marker %s not found in animation: %s.", p_end_marker, name));

	double start_time = p_start_marker ? animation->get_marker_time(p_start_marker) : -1;
	double end_time = p_end_marker ? animation->get_marker_time(p_end_marker) : -1;

	ERR_FAIL_COND_MSG(p_start_marker && p_end_marker && Animation::is_greater_approx(start_time, end_time), vformat("End marker %s is placed earlier than start marker %s in animation: %s.", p_end_marker, p_start_marker, name));

	if (p_start_marker && Animation::is_less_approx(start_time, 0)) {
		WARN_PRINT_ED(vformat("Negative time start marker: %s is invalid in the section, so the start of the animation: %s is used instead.", p_start_marker, playback.current.from->animation->get_name()));
	}
	if (p_end_marker && Animation::is_less_approx(end_time, 0)) {
		WARN_PRINT_ED(vformat("Negative time end marker: %s is invalid in the section, so the end of the animation: %s is used instead.", p_end_marker, playback.current.from->animation->get_name()));
	}

	play_section(name, start_time, end_time, p_custom_blend, p_custom_scale, p_from_end);
}

void AnimationPlayer::play_section(const StringName &p_name, double p_start_time, double p_end_time, double p_custom_blend, float p_custom_scale, bool p_from_end) {
	StringName name = p_name;

	if (name == StringName()) {
		name = playback.assigned;
	}

	ERR_FAIL_COND_MSG(!animation_set.has(name), vformat("Animation not found: %s.", name));
	ERR_FAIL_COND_MSG(p_start_time >= 0 && p_end_time >= 0 && Math::is_equal_approx(p_start_time, p_end_time), "Start time and end time must not equal to each other.");
	ERR_FAIL_COND_MSG(p_start_time >= 0 && p_end_time >= 0 && Animation::is_greater_approx(p_start_time, p_end_time), vformat("Start time %f is greater than end time %f.", p_start_time, p_end_time));

	Playback &c = playback;

	if (c.current.from) {
		double blend_time = 0.0;
		// Find if it can blend.
		BlendKey bk;
		bk.from = c.current.from->name;
		bk.to = name;

		if (Animation::is_greater_or_equal_approx(p_custom_blend, 0)) {
			blend_time = p_custom_blend;
		} else if (blend_times.has(bk)) {
			blend_time = blend_times[bk];
		} else {
			bk.from = "*";
			if (blend_times.has(bk)) {
				blend_time = blend_times[bk];
			} else {
				bk.from = c.current.from->name;
				bk.to = "*";

				if (blend_times.has(bk)) {
					blend_time = blend_times[bk];
				}
			}
		}

		if (Animation::is_less_approx(p_custom_blend, 0) && Math::is_zero_approx(blend_time) && default_blend_time) {
			blend_time = default_blend_time;
		}
		if (Animation::is_greater_approx(blend_time, 0)) {
			Blend b;
			b.data = c.current;
			b.blend_left = get_current_blend_amount();
			b.blend_time = blend_time;
			c.blend.push_back(b);
		} else {
			c.blend.clear();
		}
	}

	if (get_current_animation() != p_name) {
		_clear_playing_caches();
	}

	c.current.from = &animation_set[name];
	c.current.speed_scale = p_custom_scale;
	c.current.start_time = p_start_time;
	c.current.end_time = p_end_time;

	double start = get_section_start_time();
	double end = get_section_end_time();

	if (!end_reached) {
		playback_queue.clear();
	}

	if (c.assigned != name) { // Reset.
		c.current.pos = p_from_end ? end : start;
		c.assigned = name;
		emit_signal(SNAME("current_animation_changed"), c.assigned);
	} else {
		if (p_from_end && Math::is_equal_approx(c.current.pos, start)) {
			// Animation reset but played backwards, set position to the end.
			seek_internal(end, true, true, true);
		} else if (!p_from_end && Math::is_equal_approx(c.current.pos, end)) {
			// Animation resumed but already ended, set position to the beginning.
			seek_internal(start, true, true, true);
		} else if (playing) {
			return;
		}
	}

	c.seeked = false;
	c.started = true;

	_set_process(true); // Always process when starting an animation.
	playing = true;

	emit_signal(SceneStringName(animation_started), c.assigned);

	if (is_inside_tree() && Engine::get_singleton()->is_editor_hint()) {
		return; // No next in this case.
	}

	StringName next = animation_get_next(p_name);
	if (next != StringName() && animation_set.has(next)) {
		queue(next);
	}
}

void AnimationPlayer::_capture(const StringName &p_name, bool p_from_end, double p_duration, Tween::TransitionType p_trans_type, Tween::EaseType p_ease_type) {
	StringName name = p_name;
	if (name == StringName()) {
		name = playback.assigned;
	}

	Ref<Animation> anim = get_animation(name);
	if (anim.is_null() || !anim->is_capture_included()) {
		return;
	}
	if (signbit(p_duration)) {
		double max_dur = 0;
		double current_pos = playback.current.pos;
		if (playback.assigned != name) {
			current_pos = p_from_end ? anim->get_length() : 0;
		}
		for (int i = 0; i < anim->get_track_count(); i++) {
			if (anim->track_get_type(i) != Animation::TYPE_VALUE) {
				continue;
			}
			if (anim->value_track_get_update_mode(i) != Animation::UPDATE_CAPTURE) {
				continue;
			}
			if (anim->track_get_key_count(i) == 0) {
				continue;
			}
			max_dur = MAX(max_dur, p_from_end ? current_pos - anim->track_get_key_time(i, anim->track_get_key_count(i) - 1) : anim->track_get_key_time(i, 0) - current_pos);
		}
		p_duration = max_dur;
	}
	if (Math::is_zero_approx(p_duration)) {
		return;
	}
	capture(name, p_duration, p_trans_type, p_ease_type);
}

void AnimationPlayer::play_with_capture(const StringName &p_name, double p_duration, double p_custom_blend, float p_custom_scale, bool p_from_end, Tween::TransitionType p_trans_type, Tween::EaseType p_ease_type) {
	_capture(p_name, p_from_end, p_duration, p_trans_type, p_ease_type);
	_play(p_name, p_custom_blend, p_custom_scale, p_from_end);
}

bool AnimationPlayer::is_playing() const {
	return playing;
}

void AnimationPlayer::set_current_animation(const String &p_animation) {
	if (p_animation == "[stop]" || p_animation.is_empty()) {
		stop();
	} else if (!is_playing()) {
		play(p_animation);
	} else if (playback.assigned != p_animation) {
		float speed = playback.current.speed_scale;
		play(p_animation, -1.0, speed, signbit(speed));
	} else {
		// Same animation, do not replay from start.
	}
}

String AnimationPlayer::get_current_animation() const {
	return (is_playing() ? playback.assigned : "");
}

void AnimationPlayer::set_assigned_animation(const String &p_animation) {
	if (is_playing()) {
		float speed = playback.current.speed_scale;
		play(p_animation, -1.0, speed, signbit(speed));
	} else {
		ERR_FAIL_COND_MSG(!animation_set.has(p_animation), vformat("Animation not found: %s.", p_animation));
		playback.current.pos = 0;
		playback.current.from = &animation_set[p_animation];
		playback.current.start_time = -1;
		playback.current.end_time = -1;
		playback.assigned = p_animation;
		emit_signal(SNAME("current_animation_changed"), playback.assigned);
	}
}

String AnimationPlayer::get_assigned_animation() const {
	return playback.assigned;
}

void AnimationPlayer::pause() {
	_stop_internal(false, false);
}

void AnimationPlayer::stop(bool p_keep_state) {
	_stop_internal(true, p_keep_state);
}

void AnimationPlayer::set_speed_scale(float p_speed) {
	speed_scale = p_speed;
}

float AnimationPlayer::get_speed_scale() const {
	return speed_scale;
}

float AnimationPlayer::get_playing_speed() const {
	if (!playing) {
		return 0;
	}
	return speed_scale * playback.current.speed_scale;
}

void AnimationPlayer::seek_internal(double p_time, bool p_update, bool p_update_only, bool p_is_internal_seek) {
	if (!active) {
		return;
	}

	bool is_backward = Animation::is_less_approx(p_time, playback.current.pos);

	_check_immediately_after_start();

	playback.current.pos = p_time;
	if (!playback.current.from) {
		if (playback.assigned) {
			ERR_FAIL_COND_MSG(!animation_set.has(playback.assigned), vformat("Animation not found: %s.", playback.assigned));
			playback.current.from = &animation_set[playback.assigned];
		}
		if (!playback.current.from) {
			return; // There is no animation.
		}
	}

	double start = get_section_start_time();
	double end = get_section_end_time();

	// Clamp the seek position.
	p_time = CLAMP(p_time, start, end);

	playback.seeked = true;
	playback.internal_seeked = p_is_internal_seek;

	if (p_update) {
		_process_animation(is_backward ? -0.0 : 0.0, p_update_only);
		playback.seeked = false; // If animation was proceeded here, no more seek in internal process.
	}
}

void AnimationPlayer::seek(double p_time, bool p_update, bool p_update_only) {
	seek_internal(p_time, p_update, p_update_only);
}

void AnimationPlayer::advance(double p_time) {
	_check_immediately_after_start();
	AnimationMixer::advance(p_time);
}

void AnimationPlayer::_check_immediately_after_start() {
	if (playback.started) {
		_process_animation(0); // Force process current key for Discrete/Method/Audio/AnimationPlayback. Then, started flag is cleared.
	}
}

bool AnimationPlayer::is_valid() const {
	return (playback.current.from);
}

double AnimationPlayer::get_current_animation_position() const {
	ERR_FAIL_NULL_V_MSG(playback.current.from, 0, "AnimationPlayer has no current animation.");
	return playback.current.pos;
}

double AnimationPlayer::get_current_animation_length() const {
	ERR_FAIL_NULL_V_MSG(playback.current.from, 0, "AnimationPlayer has no current animation.");
	return playback.current.from->animation->get_length();
}

void AnimationPlayer::set_section_with_markers(const StringName &p_start_marker, const StringName &p_end_marker) {
	ERR_FAIL_NULL_MSG(playback.current.from, "AnimationPlayer has no current animation.");
	ERR_FAIL_COND_MSG(p_start_marker == p_end_marker && p_start_marker, vformat("Start marker and end marker cannot be the same marker: %s.", p_start_marker));
	ERR_FAIL_COND_MSG(p_start_marker && !playback.current.from->animation->has_marker(p_start_marker), vformat("Marker %s not found in animation: %s.", p_start_marker, playback.current.from->animation->get_name()));
	ERR_FAIL_COND_MSG(p_end_marker && !playback.current.from->animation->has_marker(p_end_marker), vformat("Marker %s not found in animation: %s.", p_end_marker, playback.current.from->animation->get_name()));
	double start_time = p_start_marker ? playback.current.from->animation->get_marker_time(p_start_marker) : -1;
	double end_time = p_end_marker ? playback.current.from->animation->get_marker_time(p_end_marker) : -1;
	if (p_start_marker && Animation::is_less_approx(start_time, 0)) {
		WARN_PRINT_ONCE_ED(vformat("Marker %s time must be positive in animation: %s.", p_start_marker, playback.current.from->animation->get_name()));
	}
	if (p_end_marker && Animation::is_less_approx(end_time, 0)) {
		WARN_PRINT_ONCE_ED(vformat("Marker %s time must be positive in animation: %s.", p_end_marker, playback.current.from->animation->get_name()));
	}
	set_section(start_time, end_time);
}

void AnimationPlayer::set_section(double p_start_time, double p_end_time) {
	ERR_FAIL_NULL_MSG(playback.current.from, "AnimationPlayer has no current animation.");
	ERR_FAIL_COND_MSG(Animation::is_greater_or_equal_approx(p_start_time, 0) && Animation::is_greater_or_equal_approx(p_end_time, 0) && Animation::is_greater_or_equal_approx(p_start_time, p_end_time), vformat("Start time %f is greater than end time %f.", p_start_time, p_end_time));
	playback.current.start_time = p_start_time;
	playback.current.end_time = p_end_time;
	playback.current.pos = CLAMP(playback.current.pos, get_section_start_time(), get_section_end_time());
}

void AnimationPlayer::reset_section() {
	playback.current.start_time = -1;
	playback.current.end_time = -1;
}

double AnimationPlayer::get_section_start_time() const {
	ERR_FAIL_NULL_V_MSG(playback.current.from, playback.current.start_time, "AnimationPlayer has no current animation.");
	if (Animation::is_less_approx(playback.current.start_time, 0) || playback.current.start_time > playback.current.from->animation->get_length()) {
		return 0;
	}
	return playback.current.start_time;
}

double AnimationPlayer::get_section_end_time() const {
	ERR_FAIL_NULL_V_MSG(playback.current.from, playback.current.end_time, "AnimationPlayer has no current animation.");
	if (Animation::is_less_approx(playback.current.end_time, 0) || playback.current.end_time > playback.current.from->animation->get_length()) {
		return playback.current.from->animation->get_length();
	}
	return playback.current.end_time;
}

bool AnimationPlayer::has_section() const {
	return Animation::is_greater_or_equal_approx(playback.current.start_time, 0) || Animation::is_greater_or_equal_approx(playback.current.end_time, 0);
}

void AnimationPlayer::set_autoplay(const String &p_name) {
	if (is_inside_tree() && !Engine::get_singleton()->is_editor_hint()) {
		WARN_PRINT("Setting autoplay after the node has been added to the scene has no effect.");
	}

	autoplay = p_name;
}

String AnimationPlayer::get_autoplay() const {
	return autoplay;
}

void AnimationPlayer::set_movie_quit_on_finish_enabled(bool p_enabled) {
	movie_quit_on_finish = p_enabled;
}

bool AnimationPlayer::is_movie_quit_on_finish_enabled() const {
	return movie_quit_on_finish;
}

void AnimationPlayer::_stop_internal(bool p_reset, bool p_keep_state) {
	_clear_caches();
	Playback &c = playback;
	// c.blend.clear();
	double start = c.current.from ? get_section_start_time() : 0;
	if (p_reset) {
		c.blend.clear();
		if (p_keep_state) {
			c.current.pos = start;
		} else {
			is_stopping = true;
			seek_internal(start, true, true, true);
			is_stopping = false;
		}
		c.current.from = nullptr;
		c.current.speed_scale = 1;
		emit_signal(SNAME("current_animation_changed"), "");
	}
	_set_process(false);
	playback_queue.clear();
	playing = false;
}

void AnimationPlayer::animation_set_next(const StringName &p_animation, const StringName &p_next) {
	ERR_FAIL_COND_MSG(!animation_set.has(p_animation), vformat("Animation not found: %s.", p_animation));
	animation_next_set[p_animation] = p_next;
}

StringName AnimationPlayer::animation_get_next(const StringName &p_animation) const {
	if (!animation_next_set.has(p_animation)) {
		return StringName();
	}
	return animation_next_set[p_animation];
}

void AnimationPlayer::set_default_blend_time(double p_default) {
	default_blend_time = p_default;
}

double AnimationPlayer::get_default_blend_time() const {
	return default_blend_time;
}

void AnimationPlayer::set_blend_time(const StringName &p_animation1, const StringName &p_animation2, double p_time) {
	ERR_FAIL_COND_MSG(!animation_set.has(p_animation1), vformat("Animation not found: %s.", p_animation1));
	ERR_FAIL_COND_MSG(!animation_set.has(p_animation2), vformat("Animation not found: %s.", p_animation2));
	ERR_FAIL_COND_MSG(p_time < 0, "Blend time cannot be smaller than 0.");

	BlendKey bk;
	bk.from = p_animation1;
	bk.to = p_animation2;
	if (Math::is_zero_approx(p_time)) {
		blend_times.erase(bk);
	} else {
		blend_times[bk] = p_time;
	}
}

double AnimationPlayer::get_blend_time(const StringName &p_animation1, const StringName &p_animation2) const {
	BlendKey bk;
	bk.from = p_animation1;
	bk.to = p_animation2;

	if (blend_times.has(bk)) {
		return blend_times[bk];
	} else {
		return 0;
	}
}

void AnimationPlayer::set_auto_capture(bool p_auto_capture) {
	auto_capture = p_auto_capture;
	notify_property_list_changed();
}

bool AnimationPlayer::is_auto_capture() const {
	return auto_capture;
}

void AnimationPlayer::set_auto_capture_duration(double p_auto_capture_duration) {
	auto_capture_duration = p_auto_capture_duration;
}

double AnimationPlayer::get_auto_capture_duration() const {
	return auto_capture_duration;
}

void AnimationPlayer::set_auto_capture_transition_type(Tween::TransitionType p_auto_capture_transition_type) {
	auto_capture_transition_type = p_auto_capture_transition_type;
}

Tween::TransitionType AnimationPlayer::get_auto_capture_transition_type() const {
	return auto_capture_transition_type;
}

void AnimationPlayer::set_auto_capture_ease_type(Tween::EaseType p_auto_capture_ease_type) {
	auto_capture_ease_type = p_auto_capture_ease_type;
}

Tween::EaseType AnimationPlayer::get_auto_capture_ease_type() const {
	return auto_capture_ease_type;
}

void AnimationPlayer::_animation_removed(const StringName &p_name, const StringName &p_library) {
	AnimationMixer::_animation_removed(p_name, p_library);

	StringName name = p_library == StringName() ? p_name : StringName(String(p_library) + "/" + String(p_name));

	if (!animation_set.has(name)) {
		return; // No need to update because not the one from the library being used.
	}

	_animation_set_cache_update();

	// Erase blends if needed
	List<BlendKey> to_erase;
	for (const KeyValue<BlendKey, double> &E : blend_times) {
		BlendKey bk = E.key;
		if (bk.from == name || bk.to == name) {
			to_erase.push_back(bk);
		}
	}

	while (to_erase.size()) {
		blend_times.erase(to_erase.front()->get());
		to_erase.pop_front();
	}
}

void AnimationPlayer::_rename_animation(const StringName &p_from_name, const StringName &p_to_name) {
	AnimationMixer::_rename_animation(p_from_name, p_to_name);

	// Rename autoplay or blends if needed.
	List<BlendKey> to_erase;
	HashMap<BlendKey, double, BlendKey> to_insert;
	for (const KeyValue<BlendKey, double> &E : blend_times) {
		BlendKey bk = E.key;
		BlendKey new_bk = bk;
		bool erase = false;
		if (bk.from == p_from_name) {
			new_bk.from = p_to_name;
			erase = true;
		}
		if (bk.to == p_from_name) {
			new_bk.to = p_to_name;
			erase = true;
		}

		if (erase) {
			to_erase.push_back(bk);
			to_insert[new_bk] = E.value;
		}
	}

	while (to_erase.size()) {
		blend_times.erase(to_erase.front()->get());
		to_erase.pop_front();
	}

	while (to_insert.size()) {
		blend_times[to_insert.begin()->key] = to_insert.begin()->value;
		to_insert.remove(to_insert.begin());
	}

	if (autoplay == p_from_name) {
		autoplay = p_to_name;
	}
}

void AnimationPlayer::_bind_methods() {
	ClassDB::bind_method(D_METHOD("animation_set_next", "animation_from", "animation_to"), &AnimationPlayer::animation_set_next);
	ClassDB::bind_method(D_METHOD("animation_get_next", "animation_from"), &AnimationPlayer::animation_get_next);

	ClassDB::bind_method(D_METHOD("set_blend_time", "animation_from", "animation_to", "sec"), &AnimationPlayer::set_blend_time);
	ClassDB::bind_method(D_METHOD("get_blend_time", "animation_from", "animation_to"), &AnimationPlayer::get_blend_time);

	ClassDB::bind_method(D_METHOD("set_default_blend_time", "sec"), &AnimationPlayer::set_default_blend_time);
	ClassDB::bind_method(D_METHOD("get_default_blend_time"), &AnimationPlayer::get_default_blend_time);

	ClassDB::bind_method(D_METHOD("set_auto_capture", "auto_capture"), &AnimationPlayer::set_auto_capture);
	ClassDB::bind_method(D_METHOD("is_auto_capture"), &AnimationPlayer::is_auto_capture);
	ClassDB::bind_method(D_METHOD("set_auto_capture_duration", "auto_capture_duration"), &AnimationPlayer::set_auto_capture_duration);
	ClassDB::bind_method(D_METHOD("get_auto_capture_duration"), &AnimationPlayer::get_auto_capture_duration);
	ClassDB::bind_method(D_METHOD("set_auto_capture_transition_type", "auto_capture_transition_type"), &AnimationPlayer::set_auto_capture_transition_type);
	ClassDB::bind_method(D_METHOD("get_auto_capture_transition_type"), &AnimationPlayer::get_auto_capture_transition_type);
	ClassDB::bind_method(D_METHOD("set_auto_capture_ease_type", "auto_capture_ease_type"), &AnimationPlayer::set_auto_capture_ease_type);
	ClassDB::bind_method(D_METHOD("get_auto_capture_ease_type"), &AnimationPlayer::get_auto_capture_ease_type);

	ClassDB::bind_method(D_METHOD("play", "name", "custom_blend", "custom_speed", "from_end"), &AnimationPlayer::play, DEFVAL(StringName()), DEFVAL(-1), DEFVAL(1.0), DEFVAL(false));
	ClassDB::bind_method(D_METHOD("play_section_with_markers", "name", "start_marker", "end_marker", "custom_blend", "custom_speed", "from_end"), &AnimationPlayer::play_section_with_markers, DEFVAL(StringName()), DEFVAL(StringName()), DEFVAL(StringName()), DEFVAL(-1), DEFVAL(1.0), DEFVAL(false));
	ClassDB::bind_method(D_METHOD("play_section", "name", "start_time", "end_time", "custom_blend", "custom_speed", "from_end"), &AnimationPlayer::play_section, DEFVAL(StringName()), DEFVAL(-1), DEFVAL(-1), DEFVAL(-1), DEFVAL(1.0), DEFVAL(false));
	ClassDB::bind_method(D_METHOD("play_backwards", "name", "custom_blend"), &AnimationPlayer::play_backwards, DEFVAL(StringName()), DEFVAL(-1));
	ClassDB::bind_method(D_METHOD("play_section_with_markers_backwards", "name", "start_marker", "end_marker", "custom_blend"), &AnimationPlayer::play_section_with_markers_backwards, DEFVAL(StringName()), DEFVAL(StringName()), DEFVAL(StringName()), DEFVAL(-1));
	ClassDB::bind_method(D_METHOD("play_section_backwards", "name", "start_time", "end_time", "custom_blend"), &AnimationPlayer::play_section_backwards, DEFVAL(StringName()), DEFVAL(-1), DEFVAL(-1), DEFVAL(-1));
	ClassDB::bind_method(D_METHOD("play_with_capture", "name", "duration", "custom_blend", "custom_speed", "from_end", "trans_type", "ease_type"), &AnimationPlayer::play_with_capture, DEFVAL(StringName()), DEFVAL(-1.0), DEFVAL(-1), DEFVAL(1.0), DEFVAL(false), DEFVAL(Tween::TRANS_LINEAR), DEFVAL(Tween::EASE_IN));
	ClassDB::bind_method(D_METHOD("pause"), &AnimationPlayer::pause);
	ClassDB::bind_method(D_METHOD("stop", "keep_state"), &AnimationPlayer::stop, DEFVAL(false));
	ClassDB::bind_method(D_METHOD("is_playing"), &AnimationPlayer::is_playing);

	ClassDB::bind_method(D_METHOD("set_current_animation", "animation"), &AnimationPlayer::set_current_animation);
	ClassDB::bind_method(D_METHOD("get_current_animation"), &AnimationPlayer::get_current_animation);
	ClassDB::bind_method(D_METHOD("set_assigned_animation", "animation"), &AnimationPlayer::set_assigned_animation);
	ClassDB::bind_method(D_METHOD("get_assigned_animation"), &AnimationPlayer::get_assigned_animation);
	ClassDB::bind_method(D_METHOD("queue", "name"), &AnimationPlayer::queue);
	ClassDB::bind_method(D_METHOD("get_queue"), &AnimationPlayer::get_queue);
	ClassDB::bind_method(D_METHOD("clear_queue"), &AnimationPlayer::clear_queue);

	ClassDB::bind_method(D_METHOD("set_speed_scale", "speed"), &AnimationPlayer::set_speed_scale);
	ClassDB::bind_method(D_METHOD("get_speed_scale"), &AnimationPlayer::get_speed_scale);
	ClassDB::bind_method(D_METHOD("get_playing_speed"), &AnimationPlayer::get_playing_speed);

	ClassDB::bind_method(D_METHOD("set_autoplay", "name"), &AnimationPlayer::set_autoplay);
	ClassDB::bind_method(D_METHOD("get_autoplay"), &AnimationPlayer::get_autoplay);

	ClassDB::bind_method(D_METHOD("find_animation", "animation"), &AnimationPlayer::find_animation);
	ClassDB::bind_method(D_METHOD("find_animation_library", "animation"), &AnimationPlayer::find_animation_library);

	ClassDB::bind_method(D_METHOD("set_movie_quit_on_finish_enabled", "enabled"), &AnimationPlayer::set_movie_quit_on_finish_enabled);
	ClassDB::bind_method(D_METHOD("is_movie_quit_on_finish_enabled"), &AnimationPlayer::is_movie_quit_on_finish_enabled);

	ClassDB::bind_method(D_METHOD("get_current_animation_position"), &AnimationPlayer::get_current_animation_position);
	ClassDB::bind_method(D_METHOD("get_current_animation_length"), &AnimationPlayer::get_current_animation_length);

	ClassDB::bind_method(D_METHOD("set_section_with_markers", "start_marker", "end_marker"), &AnimationPlayer::set_section_with_markers, DEFVAL(StringName()), DEFVAL(StringName()));
	ClassDB::bind_method(D_METHOD("set_section", "start_time", "end_time"), &AnimationPlayer::set_section, DEFVAL(-1), DEFVAL(-1));
	ClassDB::bind_method(D_METHOD("reset_section"), &AnimationPlayer::reset_section);

	ClassDB::bind_method(D_METHOD("get_section_start_time"), &AnimationPlayer::get_section_start_time);
	ClassDB::bind_method(D_METHOD("get_section_end_time"), &AnimationPlayer::get_section_end_time);
	ClassDB::bind_method(D_METHOD("has_section"), &AnimationPlayer::has_section);

	ClassDB::bind_method(D_METHOD("seek", "seconds", "update", "update_only"), &AnimationPlayer::seek, DEFVAL(false), DEFVAL(false));

	ADD_PROPERTY(PropertyInfo(Variant::STRING_NAME, "current_animation", PROPERTY_HINT_ENUM, "", PROPERTY_USAGE_EDITOR), "set_current_animation", "get_current_animation");
	ADD_PROPERTY(PropertyInfo(Variant::STRING_NAME, "assigned_animation", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "set_assigned_animation", "get_assigned_animation");
	ADD_PROPERTY(PropertyInfo(Variant::STRING_NAME, "autoplay", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NO_EDITOR), "set_autoplay", "get_autoplay");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "current_animation_length", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "get_current_animation_length");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "current_animation_position", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "get_current_animation_position");

	ADD_GROUP("Playback Options", "playback_");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "playback_auto_capture"), "set_auto_capture", "is_auto_capture");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "playback_auto_capture_duration", PROPERTY_HINT_NONE, "suffix:s"), "set_auto_capture_duration", "get_auto_capture_duration");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "playback_auto_capture_transition_type", PROPERTY_HINT_ENUM, "Linear,Sine,Quint,Quart,Quad,Expo,Elastic,Cubic,Circ,Bounce,Back,Spring"), "set_auto_capture_transition_type", "get_auto_capture_transition_type");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "playback_auto_capture_ease_type", PROPERTY_HINT_ENUM, "In,Out,InOut,OutIn"), "set_auto_capture_ease_type", "get_auto_capture_ease_type");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "playback_default_blend_time", PROPERTY_HINT_RANGE, "0,4096,0.01,suffix:s"), "set_default_blend_time", "get_default_blend_time");

	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "speed_scale", PROPERTY_HINT_RANGE, "-4,4,0.001,or_less,or_greater"), "set_speed_scale", "get_speed_scale");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "movie_quit_on_finish"), "set_movie_quit_on_finish_enabled", "is_movie_quit_on_finish_enabled");

	ADD_SIGNAL(MethodInfo(SNAME("current_animation_changed"), PropertyInfo(Variant::STRING, "name")));
	ADD_SIGNAL(MethodInfo(SNAME("animation_changed"), PropertyInfo(Variant::STRING_NAME, "old_name"), PropertyInfo(Variant::STRING_NAME, "new_name")));
}

AnimationPlayer::AnimationPlayer() {
}

AnimationPlayer::~AnimationPlayer() {
}

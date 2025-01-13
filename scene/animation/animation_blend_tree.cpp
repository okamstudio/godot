/**************************************************************************/
/*  animation_blend_tree.cpp                                              */
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

#include "animation_blend_tree.h"

#include "scene/resources/animation.h"

void AnimationNodeAnimation::set_animation(const StringName &p_name) {
	animation = p_name;
}

StringName AnimationNodeAnimation::get_animation() const {
	return animation;
}

Vector<String> (*AnimationNodeAnimation::get_editable_animation_list)() = nullptr;

void AnimationNodeAnimation::get_parameter_list(List<PropertyInfo> *r_list) const {
	AnimationNode::get_parameter_list(r_list);
}

AnimationNode::NodeTimeInfo AnimationNodeAnimation::get_node_time_info() const {
	NodeTimeInfo nti;
	if (!process_state->tree->has_animation(animation)) {
		return nti;
	}

	if (use_custom_timeline) {
		nti.length = timeline_length;
		nti.loop_mode = loop_mode;
	} else {
		Ref<Animation> anim = process_state->tree->get_animation(animation);
		nti.length = (double)anim->get_length();
		nti.loop_mode = anim->get_loop_mode();
	}
	nti.position = get_parameter(current_position);

	return nti;
}

void AnimationNodeAnimation::_validate_property(PropertyInfo &p_property) const {
	if (p_property.name == "animation" && get_editable_animation_list) {
		Vector<String> names = get_editable_animation_list();
		String anims;
		for (int i = 0; i < names.size(); i++) {
			if (i > 0) {
				anims += ",";
			}
			anims += String(names[i]);
		}
		if (!anims.is_empty()) {
			p_property.hint = PROPERTY_HINT_ENUM;
			p_property.hint_string = anims;
		}
	}

	if (!use_custom_timeline) {
		if (p_property.name == "timeline_length" || p_property.name == "start_offset" || p_property.name == "loop_mode" || p_property.name == "stretch_time_scale") {
			p_property.usage = PROPERTY_USAGE_NONE;
		}
	}
}

AnimationNode::NodeTimeInfo AnimationNodeAnimation::process(const AnimationMixer::PlaybackInfo p_playback_info, bool p_test_only) {
	process_state->is_testing = p_test_only;

	AnimationMixer::PlaybackInfo pi = p_playback_info;
	if (p_playback_info.seeked) {
		if (p_playback_info.is_external_seeking) {
			pi.delta = get_node_time_info().position - p_playback_info.time;
		}
	} else {
		pi.time = get_node_time_info().position + (backward ? -p_playback_info.delta : p_playback_info.delta);
	}

	NodeTimeInfo nti = _process(pi, p_test_only);

	if (!p_test_only) {
		set_node_time_info(nti);
	}

	return nti;
}

AnimationNode::NodeTimeInfo AnimationNodeAnimation::_process(const AnimationMixer::PlaybackInfo p_playback_info, bool p_test_only) {
	if (!process_state->tree->has_animation(animation)) {
		AnimationNodeBlendTree *tree = Object::cast_to<AnimationNodeBlendTree>(node_state.parent);
		if (tree) {
			String node_name = tree->get_node_name(Ref<AnimationNodeAnimation>(this));
			make_invalid(vformat(RTR("On BlendTree node '%s', animation not found: '%s'"), node_name, animation));

		} else {
			make_invalid(vformat(RTR("Animation not found: '%s'"), animation));
		}

		return NodeTimeInfo();
	}

	Ref<Animation> anim = process_state->tree->get_animation(animation);
	double anim_size = (double)anim->get_length();

	NodeTimeInfo cur_nti = get_node_time_info();
	double cur_len = cur_nti.length;
	double cur_time = p_playback_info.time;
	double cur_delta = p_playback_info.delta;

	Animation::LoopMode cur_loop_mode = cur_nti.loop_mode;
	double prev_time = cur_nti.position;

	Animation::LoopedFlag looped_flag = Animation::LOOPED_FLAG_NONE;
	bool node_backward = play_mode == PLAY_MODE_BACKWARD;

	bool p_seek = p_playback_info.seeked;
	bool p_is_external_seeking = p_playback_info.is_external_seeking;

	// 1. Progress for AnimationNode.
	bool will_end = Animation::is_greater_or_equal_approx(cur_time + cur_delta, cur_len);
	bool is_started = p_seek && !p_is_external_seeking && Math::is_zero_approx(cur_time);

	// 1. Progress for AnimationNode.
	if (is_started && advance_on_start) {
		cur_time = cur_delta;
	}
	if (cur_loop_mode != Animation::LOOP_NONE) {
		if (cur_loop_mode == Animation::LOOP_LINEAR) {
			if (!Math::is_zero_approx(cur_len)) {
				cur_time = Math::fposmod(cur_time, cur_len);
			}
			backward = false;
		} else {
			if (!Math::is_zero_approx(cur_len)) {
				if (Animation::is_greater_or_equal_approx(prev_time, 0) && Animation::is_less_approx(cur_time, 0)) {
					backward = !backward;
				} else if (Animation::is_less_or_equal_approx(prev_time, cur_len) && Animation::is_greater_approx(cur_time, cur_len)) {
					backward = !backward;
				}
				cur_time = Math::pingpong(cur_time, cur_len);
			}
		}
	} else {
		if (Animation::is_less_approx(cur_time, 0)) {
			cur_delta += cur_time;
			cur_time = 0;
		} else if (Animation::is_greater_approx(cur_time, cur_len)) {
			cur_delta += cur_time - cur_len;
			cur_time = cur_len;
		}
		backward = false;
		// If ended, don't progress AnimationNode. So set delta to 0.
		if (!Math::is_zero_approx(cur_delta)) {
			if (play_mode == PLAY_MODE_FORWARD) {
				if (Animation::is_greater_or_equal_approx(prev_time, cur_len)) {
					cur_delta = 0;
				}
			} else {
				if (Animation::is_less_or_equal_approx(prev_time, 0)) {
					cur_delta = 0;
				}
			}
		}
	}

	// 2. For return, store "AnimationNode" time info here, not "Animation" time info as below.
	NodeTimeInfo nti;
	nti.length = cur_len;
	nti.position = cur_time;
	nti.delta = cur_delta;
	nti.loop_mode = cur_loop_mode;
	nti.will_end = will_end;

	// 3. Progress for Animation.
	double prev_playback_time = prev_time + start_offset;
	double cur_playback_time = cur_time + start_offset;
	if (stretch_time_scale) {
		double mlt = anim_size / cur_len;
		prev_playback_time *= mlt;
		cur_playback_time *= mlt;
		cur_delta *= mlt;
	}
	if (cur_loop_mode == Animation::LOOP_LINEAR) {
		if (!Math::is_zero_approx(anim_size)) {
			prev_playback_time = Math::fposmod(prev_playback_time, anim_size);
			cur_playback_time = Math::fposmod(cur_playback_time, anim_size);
			if (Animation::is_greater_or_equal_approx(prev_playback_time, 0) && Animation::is_less_approx(cur_playback_time, 0)) {
				looped_flag = node_backward ? Animation::LOOPED_FLAG_END : Animation::LOOPED_FLAG_START;
			}
			if (Animation::is_less_or_equal_approx(prev_playback_time, anim_size) && Animation::is_greater_approx(cur_playback_time, anim_size)) {
				looped_flag = node_backward ? Animation::LOOPED_FLAG_START : Animation::LOOPED_FLAG_END;
			}
		}
	} else if (cur_loop_mode == Animation::LOOP_PINGPONG) {
		if (!Math::is_zero_approx(anim_size)) {
			if (Animation::is_greater_or_equal_approx(Math::fposmod(cur_playback_time, anim_size * 2.0), anim_size)) {
				cur_delta = -cur_delta; // Needed for retrieving discrete keys correctly.
			}
			prev_playback_time = Math::pingpong(prev_playback_time, anim_size);
			cur_playback_time = Math::pingpong(cur_playback_time, anim_size);
			if (Animation::is_greater_or_equal_approx(prev_playback_time, 0) && Animation::is_less_approx(cur_playback_time, 0)) {
				looped_flag = node_backward ? Animation::LOOPED_FLAG_END : Animation::LOOPED_FLAG_START;
			}
			if (Animation::is_less_or_equal_approx(prev_playback_time, anim_size) && Animation::is_greater_approx(cur_playback_time, anim_size)) {
				looped_flag = node_backward ? Animation::LOOPED_FLAG_START : Animation::LOOPED_FLAG_END;
			}
		}
	} else {
		if (Animation::is_less_approx(cur_playback_time, 0)) {
			cur_playback_time = 0;
		} else if (Animation::is_greater_approx(cur_playback_time, anim_size)) {
			cur_playback_time = anim_size;
		}

		// Emit start & finish signal. Internally, the detections are the same for backward.
		// We should use call_deferred since the track keys are still being processed.
		if (process_state->tree && !p_test_only) {
			// AnimationTree uses seek to 0 "internally" to process the first key of the animation, which is used as the start detection.
			if (is_started) {
				process_state->tree->call_deferred(SNAME("emit_signal"), SceneStringName(animation_started), animation);
			}
			// Finished.
			if (Animation::is_less_approx(prev_playback_time, anim_size) && Animation::is_greater_or_equal_approx(cur_playback_time, anim_size)) {
				cur_playback_time = anim_size;
				process_state->tree->call_deferred(SNAME("emit_signal"), SceneStringName(animation_finished), animation);
			}
		}
	}

	if (!p_test_only) {
		AnimationMixer::PlaybackInfo pi = p_playback_info;
		pi.start = 0.0;
		pi.end = cur_len;
		if (play_mode == PLAY_MODE_FORWARD) {
			pi.time = cur_playback_time;
			pi.delta = cur_delta;
		} else {
			pi.time = anim_size - cur_playback_time;
			pi.delta = -cur_delta;
		}
		pi.weight = 1.0;
		pi.looped_flag = looped_flag;
		blend_animation(animation, pi);
	}

	return nti;
}

String AnimationNodeAnimation::get_caption() const {
	return "Animation";
}

void AnimationNodeAnimation::set_play_mode(PlayMode p_play_mode) {
	play_mode = p_play_mode;
}

AnimationNodeAnimation::PlayMode AnimationNodeAnimation::get_play_mode() const {
	return play_mode;
}

void AnimationNodeAnimation::set_backward(bool p_backward) {
	backward = p_backward;
}

bool AnimationNodeAnimation::is_backward() const {
	return backward;
}

void AnimationNodeAnimation::set_advance_on_start(bool p_advance_on_start) {
	advance_on_start = p_advance_on_start;
}

bool AnimationNodeAnimation::is_advance_on_start() const {
	return advance_on_start;
}

void AnimationNodeAnimation::set_use_custom_timeline(bool p_use_custom_timeline) {
	use_custom_timeline = p_use_custom_timeline;
	notify_property_list_changed();
}

bool AnimationNodeAnimation::is_using_custom_timeline() const {
	return use_custom_timeline;
}

void AnimationNodeAnimation::set_timeline_length(double p_length) {
	timeline_length = p_length;
}

double AnimationNodeAnimation::get_timeline_length() const {
	return timeline_length;
}

void AnimationNodeAnimation::set_stretch_time_scale(bool p_strech_time_scale) {
	stretch_time_scale = p_strech_time_scale;
	notify_property_list_changed();
}

bool AnimationNodeAnimation::is_stretching_time_scale() const {
	return stretch_time_scale;
}

void AnimationNodeAnimation::set_start_offset(double p_offset) {
	start_offset = p_offset;
}

double AnimationNodeAnimation::get_start_offset() const {
	return start_offset;
}

void AnimationNodeAnimation::set_loop_mode(Animation::LoopMode p_loop_mode) {
	loop_mode = p_loop_mode;
}

Animation::LoopMode AnimationNodeAnimation::get_loop_mode() const {
	return loop_mode;
}

void AnimationNodeAnimation::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_animation", "name"), &AnimationNodeAnimation::set_animation);
	ClassDB::bind_method(D_METHOD("get_animation"), &AnimationNodeAnimation::get_animation);

	ClassDB::bind_method(D_METHOD("set_play_mode", "mode"), &AnimationNodeAnimation::set_play_mode);
	ClassDB::bind_method(D_METHOD("get_play_mode"), &AnimationNodeAnimation::get_play_mode);

	ClassDB::bind_method(D_METHOD("set_advance_on_start", "advance_on_start"), &AnimationNodeAnimation::set_advance_on_start);
	ClassDB::bind_method(D_METHOD("is_advance_on_start"), &AnimationNodeAnimation::is_advance_on_start);

	ClassDB::bind_method(D_METHOD("set_use_custom_timeline", "use_custom_timeline"), &AnimationNodeAnimation::set_use_custom_timeline);
	ClassDB::bind_method(D_METHOD("is_using_custom_timeline"), &AnimationNodeAnimation::is_using_custom_timeline);

	ClassDB::bind_method(D_METHOD("set_timeline_length", "timeline_length"), &AnimationNodeAnimation::set_timeline_length);
	ClassDB::bind_method(D_METHOD("get_timeline_length"), &AnimationNodeAnimation::get_timeline_length);

	ClassDB::bind_method(D_METHOD("set_stretch_time_scale", "stretch_time_scale"), &AnimationNodeAnimation::set_stretch_time_scale);
	ClassDB::bind_method(D_METHOD("is_stretching_time_scale"), &AnimationNodeAnimation::is_stretching_time_scale);

	ClassDB::bind_method(D_METHOD("set_start_offset", "start_offset"), &AnimationNodeAnimation::set_start_offset);
	ClassDB::bind_method(D_METHOD("get_start_offset"), &AnimationNodeAnimation::get_start_offset);

	ClassDB::bind_method(D_METHOD("set_loop_mode", "loop_mode"), &AnimationNodeAnimation::set_loop_mode);
	ClassDB::bind_method(D_METHOD("get_loop_mode"), &AnimationNodeAnimation::get_loop_mode);

	ADD_PROPERTY(PropertyInfo(Variant::STRING_NAME, "animation"), "set_animation", "get_animation");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "play_mode", PROPERTY_HINT_ENUM, "Forward,Backward"), "set_play_mode", "get_play_mode");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "advance_on_start"), "set_advance_on_start", "is_advance_on_start");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "use_custom_timeline"), "set_use_custom_timeline", "is_using_custom_timeline");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "timeline_length", PROPERTY_HINT_RANGE, "0.001,60,0.001,or_greater,or_less,hide_slider,suffix:s"), "set_timeline_length", "get_timeline_length");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "stretch_time_scale"), "set_stretch_time_scale", "is_stretching_time_scale");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "start_offset", PROPERTY_HINT_RANGE, "-60,60,0.001,or_greater,or_less,hide_slider,suffix:s"), "set_start_offset", "get_start_offset");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "loop_mode", PROPERTY_HINT_ENUM, "None,Linear,Ping-Pong"), "set_loop_mode", "get_loop_mode");

	BIND_ENUM_CONSTANT(PLAY_MODE_FORWARD);
	BIND_ENUM_CONSTANT(PLAY_MODE_BACKWARD);
}

AnimationNodeAnimation::AnimationNodeAnimation() {
}

////////////////////////////////////////////////////////

void AnimationNodeSync::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_use_sync", "enable"), &AnimationNodeSync::set_use_sync);
	ClassDB::bind_method(D_METHOD("is_using_sync"), &AnimationNodeSync::is_using_sync);

	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "sync"), "set_use_sync", "is_using_sync");
}

void AnimationNodeSync::set_use_sync(bool p_sync) {
	sync = p_sync;
}

bool AnimationNodeSync::is_using_sync() const {
	return sync;
}

AnimationNodeSync::AnimationNodeSync() {
}

////////////////////////////////////////////////////////
void AnimationNodeOneShot::get_parameter_list(List<PropertyInfo> *r_list) const {
	AnimationNode::get_parameter_list(r_list);
	r_list->push_back(PropertyInfo(Variant::BOOL, active, PROPERTY_HINT_NONE, "", PROPERTY_USAGE_EDITOR | PROPERTY_USAGE_STORAGE | PROPERTY_USAGE_READ_ONLY));
	r_list->push_back(PropertyInfo(Variant::BOOL, internal_active, PROPERTY_HINT_NONE, "", PROPERTY_USAGE_STORAGE | PROPERTY_USAGE_READ_ONLY));
	r_list->push_back(PropertyInfo(Variant::INT, request, PROPERTY_HINT_ENUM, ",Fire,Abort,Fade Out"));
	r_list->push_back(PropertyInfo(Variant::FLOAT, fade_in_remaining, PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE));
	r_list->push_back(PropertyInfo(Variant::FLOAT, fade_out_remaining, PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE));
	r_list->push_back(PropertyInfo(Variant::FLOAT, time_to_restart, PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE));
}

Variant AnimationNodeOneShot::get_parameter_default_value(const StringName &p_parameter) const {
	Variant ret = AnimationNode::get_parameter_default_value(p_parameter);
	if (ret != Variant()) {
		return ret;
	}

	if (p_parameter == request) {
		return ONE_SHOT_REQUEST_NONE;
	} else if (p_parameter == active || p_parameter == internal_active) {
		return false;
	} else if (p_parameter == time_to_restart) {
		return -1;
	} else {
		return 0.0;
	}
}

bool AnimationNodeOneShot::is_parameter_read_only(const StringName &p_parameter) const {
	if (AnimationNode::is_parameter_read_only(p_parameter)) {
		return true;
	}

	if (p_parameter == active || p_parameter == internal_active) {
		return true;
	}
	return false;
}

void AnimationNodeOneShot::set_fade_in_time(double p_time) {
	fade_in = p_time;
}

double AnimationNodeOneShot::get_fade_in_time() const {
	return fade_in;
}

void AnimationNodeOneShot::set_fade_out_time(double p_time) {
	fade_out = p_time;
}

double AnimationNodeOneShot::get_fade_out_time() const {
	return fade_out;
}

void AnimationNodeOneShot::set_fade_in_curve(const Ref<Curve> &p_curve) {
	fade_in_curve = p_curve;
}

Ref<Curve> AnimationNodeOneShot::get_fade_in_curve() const {
	return fade_in_curve;
}

void AnimationNodeOneShot::set_fade_out_curve(const Ref<Curve> &p_curve) {
	fade_out_curve = p_curve;
}

Ref<Curve> AnimationNodeOneShot::get_fade_out_curve() const {
	return fade_out_curve;
}

void AnimationNodeOneShot::set_auto_restart_enabled(bool p_enabled) {
	auto_restart = p_enabled;
}

void AnimationNodeOneShot::set_auto_restart_delay(double p_time) {
	auto_restart_delay = p_time;
}

void AnimationNodeOneShot::set_auto_restart_random_delay(double p_time) {
	auto_restart_random_delay = p_time;
}

bool AnimationNodeOneShot::is_auto_restart_enabled() const {
	return auto_restart;
}

double AnimationNodeOneShot::get_auto_restart_delay() const {
	return auto_restart_delay;
}

double AnimationNodeOneShot::get_auto_restart_random_delay() const {
	return auto_restart_random_delay;
}

void AnimationNodeOneShot::set_mix_mode(MixMode p_mix) {
	mix = p_mix;
}

AnimationNodeOneShot::MixMode AnimationNodeOneShot::get_mix_mode() const {
	return mix;
}

void AnimationNodeOneShot::set_break_loop_at_end(bool p_enable) {
	break_loop_at_end = p_enable;
}

bool AnimationNodeOneShot::is_loop_broken_at_end() const {
	return break_loop_at_end;
}

String AnimationNodeOneShot::get_caption() const {
	return "OneShot";
}

bool AnimationNodeOneShot::has_filter() const {
	return true;
}

AnimationNode::NodeTimeInfo AnimationNodeOneShot::_process(const AnimationMixer::PlaybackInfo p_playback_info, bool p_test_only) {
	OneShotRequest cur_request = static_cast<OneShotRequest>((int)get_parameter(request));
	bool cur_active = get_parameter(active);
	bool cur_internal_active = get_parameter(internal_active);
	NodeTimeInfo cur_nti = get_node_time_info();
	double cur_time_to_restart = get_parameter(time_to_restart);
	double cur_fade_in_remaining = get_parameter(fade_in_remaining);
	double cur_fade_out_remaining = get_parameter(fade_out_remaining);

	set_parameter(request, ONE_SHOT_REQUEST_NONE);

	bool is_shooting = true;
	bool clear_remaining_fade = false;
	bool is_fading_out = cur_active == true && cur_internal_active == false;

	double p_time = p_playback_info.time;
	double p_delta = p_playback_info.delta;
	double abs_delta = Math::abs(p_delta);
	bool p_seek = p_playback_info.seeked;
	bool p_is_external_seeking = p_playback_info.is_external_seeking;

	if (Math::is_zero_approx(p_time) && p_seek && !p_is_external_seeking) {
		clear_remaining_fade = true; // Reset occurs.
	}

	bool do_start = cur_request == ONE_SHOT_REQUEST_FIRE;
	if (cur_request == ONE_SHOT_REQUEST_ABORT) {
		set_parameter(internal_active, false);
		set_parameter(active, false);
		set_parameter(time_to_restart, -1);
		is_shooting = false;
	} else if (cur_request == ONE_SHOT_REQUEST_FADE_OUT && !is_fading_out) { // If fading, keep current fade.
		if (cur_active) {
			// Request fading.
			is_fading_out = true;
			cur_fade_out_remaining = fade_out;
			cur_fade_in_remaining = 0;
		} else {
			// Shot is ended, do nothing.
			is_shooting = false;
		}
		set_parameter(internal_active, false);
		set_parameter(time_to_restart, -1);
	} else if (!do_start && !cur_active) {
		if (Animation::is_greater_or_equal_approx(cur_time_to_restart, 0) && !p_seek) {
			cur_time_to_restart -= abs_delta;
			if (Animation::is_less_approx(cur_time_to_restart, 0)) {
				do_start = true; // Restart.
			}
			set_parameter(time_to_restart, cur_time_to_restart);
		}
		if (!do_start) {
			is_shooting = false;
		}
	}

	bool os_seek = p_seek;

	if (clear_remaining_fade) {
		os_seek = false;
		cur_fade_out_remaining = 0;
		set_parameter(fade_out_remaining, 0);
		if (is_fading_out) {
			is_fading_out = false;
			set_parameter(internal_active, false);
			set_parameter(active, false);
		}
	}

	if (!is_shooting) {
		AnimationMixer::PlaybackInfo pi = p_playback_info;
		pi.weight = 1.0;
		return blend_input(0, pi, FILTER_IGNORE, sync, p_test_only);
	}

	if (do_start) {
		os_seek = true;
		if (!cur_internal_active) {
			cur_fade_in_remaining = fade_in; // If already active, don't fade-in again.
		}
		cur_internal_active = true;
		set_parameter(request, ONE_SHOT_REQUEST_NONE);
		set_parameter(internal_active, true);
		set_parameter(active, true);
	}

	real_t blend = 1.0;
	bool use_blend = sync;

	if (Animation::is_greater_approx(cur_fade_in_remaining, 0)) {
		if (Animation::is_greater_approx(fade_in, 0)) {
			use_blend = true;
			blend = (fade_in - cur_fade_in_remaining) / fade_in;
			if (fade_in_curve.is_valid()) {
				blend = fade_in_curve->sample(blend);
			}
		} else {
			blend = 0; // Should not happen.
		}
	}

	if (is_fading_out) {
		use_blend = true;
		if (Animation::is_greater_approx(fade_out, 0)) {
			blend = cur_fade_out_remaining / fade_out;
			if (fade_out_curve.is_valid()) {
				blend = 1.0 - fade_out_curve->sample(1.0 - blend);
			}
		} else {
			blend = 0;
		}
	}

	AnimationMixer::PlaybackInfo pi = p_playback_info;
	NodeTimeInfo main_nti;
	if (mix == MIX_MODE_ADD) {
		pi.weight = 1.0;
		main_nti = blend_input(0, pi, FILTER_IGNORE, sync, p_test_only);
	} else {
		pi.seeked &= use_blend;
		pi.weight = 1.0 - blend;
		main_nti = blend_input(0, pi, FILTER_BLEND, sync, p_test_only); // Unlike below, processing this edge is a corner case.
	}

	pi = p_playback_info;
	if (do_start) {
		pi.time = 0;
	} else if (os_seek) {
		pi.time = cur_nti.position;
	}
	pi.seeked = os_seek;
	pi.weight = Math::is_zero_approx(blend) ? CMP_EPSILON : blend;

	NodeTimeInfo os_nti = blend_input(1, pi, FILTER_PASS, true, p_test_only); // Blend values must be more than CMP_EPSILON to process discrete keys in edge.

	if (Animation::is_less_or_equal_approx(cur_fade_in_remaining, 0) && !do_start && !is_fading_out && Animation::is_less_or_equal_approx(os_nti.get_remain(break_loop_at_end), fade_out)) {
		is_fading_out = true;
		cur_fade_out_remaining = os_nti.get_remain(break_loop_at_end);
		cur_fade_in_remaining = 0;
		set_parameter(internal_active, false);
	}

	if (!p_seek) {
		if (Animation::is_less_or_equal_approx(os_nti.get_remain(break_loop_at_end), 0) || (is_fading_out && Animation::is_less_or_equal_approx(cur_fade_out_remaining, 0))) {
			set_parameter(internal_active, false);
			set_parameter(active, false);
			if (auto_restart) {
				double restart_sec = auto_restart_delay + Math::randd() * auto_restart_random_delay;
				set_parameter(time_to_restart, restart_sec);
			}
		}
		double d = Math::abs(os_nti.delta);
		if (!do_start) {
			cur_fade_in_remaining = MAX(0, cur_fade_in_remaining - d); // Don't consider seeked delta by restart.
		}
		cur_fade_out_remaining = MAX(0, cur_fade_out_remaining - d);
	}

	set_parameter(fade_in_remaining, cur_fade_in_remaining);
	set_parameter(fade_out_remaining, cur_fade_out_remaining);

	return cur_internal_active ? os_nti : main_nti;
}

void AnimationNodeOneShot::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_fadein_time", "time"), &AnimationNodeOneShot::set_fade_in_time);
	ClassDB::bind_method(D_METHOD("get_fadein_time"), &AnimationNodeOneShot::get_fade_in_time);

	ClassDB::bind_method(D_METHOD("set_fadein_curve", "curve"), &AnimationNodeOneShot::set_fade_in_curve);
	ClassDB::bind_method(D_METHOD("get_fadein_curve"), &AnimationNodeOneShot::get_fade_in_curve);

	ClassDB::bind_method(D_METHOD("set_fadeout_time", "time"), &AnimationNodeOneShot::set_fade_out_time);
	ClassDB::bind_method(D_METHOD("get_fadeout_time"), &AnimationNodeOneShot::get_fade_out_time);

	ClassDB::bind_method(D_METHOD("set_fadeout_curve", "curve"), &AnimationNodeOneShot::set_fade_out_curve);
	ClassDB::bind_method(D_METHOD("get_fadeout_curve"), &AnimationNodeOneShot::get_fade_out_curve);

	ClassDB::bind_method(D_METHOD("set_break_loop_at_end", "enable"), &AnimationNodeOneShot::set_break_loop_at_end);
	ClassDB::bind_method(D_METHOD("is_loop_broken_at_end"), &AnimationNodeOneShot::is_loop_broken_at_end);

	ClassDB::bind_method(D_METHOD("set_autorestart", "active"), &AnimationNodeOneShot::set_auto_restart_enabled);
	ClassDB::bind_method(D_METHOD("has_autorestart"), &AnimationNodeOneShot::is_auto_restart_enabled);

	ClassDB::bind_method(D_METHOD("set_autorestart_delay", "time"), &AnimationNodeOneShot::set_auto_restart_delay);
	ClassDB::bind_method(D_METHOD("get_autorestart_delay"), &AnimationNodeOneShot::get_auto_restart_delay);

	ClassDB::bind_method(D_METHOD("set_autorestart_random_delay", "time"), &AnimationNodeOneShot::set_auto_restart_random_delay);
	ClassDB::bind_method(D_METHOD("get_autorestart_random_delay"), &AnimationNodeOneShot::get_auto_restart_random_delay);

	ClassDB::bind_method(D_METHOD("set_mix_mode", "mode"), &AnimationNodeOneShot::set_mix_mode);
	ClassDB::bind_method(D_METHOD("get_mix_mode"), &AnimationNodeOneShot::get_mix_mode);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "mix_mode", PROPERTY_HINT_ENUM, "Blend,Add"), "set_mix_mode", "get_mix_mode");

	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "fadein_time", PROPERTY_HINT_RANGE, "0,60,0.01,or_greater,suffix:s"), "set_fadein_time", "get_fadein_time");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "fadein_curve", PROPERTY_HINT_RESOURCE_TYPE, "Curve"), "set_fadein_curve", "get_fadein_curve");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "fadeout_time", PROPERTY_HINT_RANGE, "0,60,0.01,or_greater,suffix:s"), "set_fadeout_time", "get_fadeout_time");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "fadeout_curve", PROPERTY_HINT_RESOURCE_TYPE, "Curve"), "set_fadeout_curve", "get_fadeout_curve");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "break_loop_at_end"), "set_break_loop_at_end", "is_loop_broken_at_end");

	ADD_GROUP("Auto Restart", "autorestart_");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "autorestart"), "set_autorestart", "has_autorestart");

	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "autorestart_delay", PROPERTY_HINT_RANGE, "0,60,0.01,or_greater,suffix:s"), "set_autorestart_delay", "get_autorestart_delay");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "autorestart_random_delay", PROPERTY_HINT_RANGE, "0,60,0.01,or_greater,suffix:s"), "set_autorestart_random_delay", "get_autorestart_random_delay");

	BIND_ENUM_CONSTANT(ONE_SHOT_REQUEST_NONE);
	BIND_ENUM_CONSTANT(ONE_SHOT_REQUEST_FIRE);
	BIND_ENUM_CONSTANT(ONE_SHOT_REQUEST_ABORT);
	BIND_ENUM_CONSTANT(ONE_SHOT_REQUEST_FADE_OUT);

	BIND_ENUM_CONSTANT(MIX_MODE_BLEND);
	BIND_ENUM_CONSTANT(MIX_MODE_ADD);
}

AnimationNodeOneShot::AnimationNodeOneShot() {
	add_input("in");
	add_input("shot");
}

////////////////////////////////////////////////

void AnimationNodeAdd2::get_parameter_list(List<PropertyInfo> *r_list) const {
	AnimationNode::get_parameter_list(r_list);
	r_list->push_back(PropertyInfo(Variant::FLOAT, add_amount, PROPERTY_HINT_RANGE, "0,1,0.01,or_less,or_greater"));
}

Variant AnimationNodeAdd2::get_parameter_default_value(const StringName &p_parameter) const {
	Variant ret = AnimationNode::get_parameter_default_value(p_parameter);
	if (ret != Variant()) {
		return ret;
	}

	return 0;
}

String AnimationNodeAdd2::get_caption() const {
	return "Add2";
}

bool AnimationNodeAdd2::has_filter() const {
	return true;
}

AnimationNode::NodeTimeInfo AnimationNodeAdd2::_process(const AnimationMixer::PlaybackInfo p_playback_info, bool p_test_only) {
	double amount = get_parameter(add_amount);

	AnimationMixer::PlaybackInfo pi = p_playback_info;
	pi.weight = 1.0;
	NodeTimeInfo nti = blend_input(0, pi, FILTER_IGNORE, sync, p_test_only);
	pi.weight = amount;
	blend_input(1, pi, FILTER_PASS, sync, p_test_only);

	return nti;
}

AnimationNodeAdd2::AnimationNodeAdd2() {
	add_input("in");
	add_input("add");
}

////////////////////////////////////////////////

void AnimationNodeAdd3::get_parameter_list(List<PropertyInfo> *r_list) const {
	AnimationNode::get_parameter_list(r_list);
	r_list->push_back(PropertyInfo(Variant::FLOAT, add_amount, PROPERTY_HINT_RANGE, "-1,1,0.01,or_less,or_greater"));
}

Variant AnimationNodeAdd3::get_parameter_default_value(const StringName &p_parameter) const {
	Variant ret = AnimationNode::get_parameter_default_value(p_parameter);
	if (ret != Variant()) {
		return ret;
	}

	return 0;
}

String AnimationNodeAdd3::get_caption() const {
	return "Add3";
}

bool AnimationNodeAdd3::has_filter() const {
	return true;
}

AnimationNode::NodeTimeInfo AnimationNodeAdd3::_process(const AnimationMixer::PlaybackInfo p_playback_info, bool p_test_only) {
	double amount = get_parameter(add_amount);

	AnimationMixer::PlaybackInfo pi = p_playback_info;
	pi.weight = MAX(0, -amount);
	blend_input(0, pi, FILTER_PASS, sync, p_test_only);
	pi.weight = 1.0;
	NodeTimeInfo nti = blend_input(1, pi, FILTER_IGNORE, sync, p_test_only);
	pi.weight = MAX(0, amount);
	blend_input(2, pi, FILTER_PASS, sync, p_test_only);

	return nti;
}

AnimationNodeAdd3::AnimationNodeAdd3() {
	add_input("-add");
	add_input("in");
	add_input("+add");
}

/////////////////////////////////////////////

void AnimationNodeBlend2::get_parameter_list(List<PropertyInfo> *r_list) const {
	AnimationNode::get_parameter_list(r_list);
	r_list->push_back(PropertyInfo(Variant::FLOAT, blend_amount, PROPERTY_HINT_RANGE, "0,1,0.01,or_less,or_greater"));
}

Variant AnimationNodeBlend2::get_parameter_default_value(const StringName &p_parameter) const {
	Variant ret = AnimationNode::get_parameter_default_value(p_parameter);
	if (ret != Variant()) {
		return ret;
	}

	return 0; // For blend amount.
}

String AnimationNodeBlend2::get_caption() const {
	return "Blend2";
}

AnimationNode::NodeTimeInfo AnimationNodeBlend2::_process(const AnimationMixer::PlaybackInfo p_playback_info, bool p_test_only) {
	double amount = get_parameter(blend_amount);

	AnimationMixer::PlaybackInfo pi = p_playback_info;
	pi.weight = 1.0 - amount;
	NodeTimeInfo nti0 = blend_input(0, pi, FILTER_BLEND, sync, p_test_only);
	pi.weight = amount;
	NodeTimeInfo nti1 = blend_input(1, pi, FILTER_PASS, sync, p_test_only);

	return amount > 0.5 ? nti1 : nti0; // Hacky but good enough.
}

bool AnimationNodeBlend2::has_filter() const {
	return true;
}

AnimationNodeBlend2::AnimationNodeBlend2() {
	add_input("in");
	add_input("blend");
}

//////////////////////////////////////

void AnimationNodeBlend3::get_parameter_list(List<PropertyInfo> *r_list) const {
	AnimationNode::get_parameter_list(r_list);
	r_list->push_back(PropertyInfo(Variant::FLOAT, blend_amount, PROPERTY_HINT_RANGE, "-1,1,0.01,or_less,or_greater"));
}

Variant AnimationNodeBlend3::get_parameter_default_value(const StringName &p_parameter) const {
	Variant ret = AnimationNode::get_parameter_default_value(p_parameter);
	if (ret != Variant()) {
		return ret;
	}

	return 0; // For blend amount.
}

String AnimationNodeBlend3::get_caption() const {
	return "Blend3";
}

AnimationNode::NodeTimeInfo AnimationNodeBlend3::_process(const AnimationMixer::PlaybackInfo p_playback_info, bool p_test_only) {
	double amount = get_parameter(blend_amount);

	AnimationMixer::PlaybackInfo pi = p_playback_info;
	pi.weight = MAX(0, -amount);
	NodeTimeInfo nti0 = blend_input(0, pi, FILTER_IGNORE, sync, p_test_only);
	pi.weight = 1.0 - ABS(amount);
	NodeTimeInfo nti1 = blend_input(1, pi, FILTER_IGNORE, sync, p_test_only);
	pi.weight = MAX(0, amount);
	NodeTimeInfo nti2 = blend_input(2, pi, FILTER_IGNORE, sync, p_test_only);

	return amount > 0.5 ? nti2 : (amount < -0.5 ? nti0 : nti1); // Hacky but good enough.
}

AnimationNodeBlend3::AnimationNodeBlend3() {
	add_input("-blend");
	add_input("in");
	add_input("+blend");
}

////////////////////////////////////////////////

void AnimationNodeSub2::get_parameter_list(List<PropertyInfo> *r_list) const {
	AnimationNode::get_parameter_list(r_list);
	r_list->push_back(PropertyInfo(Variant::FLOAT, sub_amount, PROPERTY_HINT_RANGE, "0,1,0.01,or_less,or_greater"));
}

Variant AnimationNodeSub2::get_parameter_default_value(const StringName &p_parameter) const {
	Variant ret = AnimationNode::get_parameter_default_value(p_parameter);
	if (ret != Variant()) {
		return ret;
	}

	return 0;
}

String AnimationNodeSub2::get_caption() const {
	return "Sub2";
}

bool AnimationNodeSub2::has_filter() const {
	return true;
}

AnimationNode::NodeTimeInfo AnimationNodeSub2::_process(const AnimationMixer::PlaybackInfo p_playback_info, bool p_test_only) {
	double amount = get_parameter(sub_amount);

	AnimationMixer::PlaybackInfo pi = p_playback_info;
	// Out = Sub.Transform3D^(-1) * In.Transform3D
	pi.weight = -amount;
	blend_input(1, pi, FILTER_PASS, sync, p_test_only);
	pi.weight = 1.0;

	return blend_input(0, pi, FILTER_IGNORE, sync, p_test_only);
}

AnimationNodeSub2::AnimationNodeSub2() {
	add_input("in");
	add_input("sub");
}

/////////////////////////////////

void AnimationNodeTimeScale::get_parameter_list(List<PropertyInfo> *r_list) const {
	AnimationNode::get_parameter_list(r_list);
	r_list->push_back(PropertyInfo(Variant::FLOAT, scale, PROPERTY_HINT_RANGE, "-32,32,0.01,or_less,or_greater"));
}

Variant AnimationNodeTimeScale::get_parameter_default_value(const StringName &p_parameter) const {
	Variant ret = AnimationNode::get_parameter_default_value(p_parameter);
	if (ret != Variant()) {
		return ret;
	}

	return 1.0; // Initial timescale.
}

String AnimationNodeTimeScale::get_caption() const {
	return "TimeScale";
}

AnimationNode::NodeTimeInfo AnimationNodeTimeScale::_process(const AnimationMixer::PlaybackInfo p_playback_info, bool p_test_only) {
	double cur_scale = get_parameter(scale);

	AnimationMixer::PlaybackInfo pi = p_playback_info;
	pi.weight = 1.0;
	if (!pi.seeked) {
		pi.delta *= cur_scale;
	}

	return blend_input(0, pi, FILTER_IGNORE, true, p_test_only);
}

AnimationNodeTimeScale::AnimationNodeTimeScale() {
	add_input("in");
}

////////////////////////////////////

void AnimationNodeTimeSeek::get_parameter_list(List<PropertyInfo> *r_list) const {
	AnimationNode::get_parameter_list(r_list);
	r_list->push_back(PropertyInfo(Variant::FLOAT, seek_pos_request, PROPERTY_HINT_RANGE, "-1,3600,0.01,or_greater")); // It will be reset to -1 after seeking the position immediately.
}

Variant AnimationNodeTimeSeek::get_parameter_default_value(const StringName &p_parameter) const {
	Variant ret = AnimationNode::get_parameter_default_value(p_parameter);
	if (ret != Variant()) {
		return ret;
	}

	return -1.0; // Initial seek request.
}

String AnimationNodeTimeSeek::get_caption() const {
	return "TimeSeek";
}

void AnimationNodeTimeSeek::set_explicit_elapse(bool p_enable) {
	explicit_elapse = p_enable;
}

bool AnimationNodeTimeSeek::is_explicit_elapse() const {
	return explicit_elapse;
}

AnimationNode::NodeTimeInfo AnimationNodeTimeSeek::_process(const AnimationMixer::PlaybackInfo p_playback_info, bool p_test_only) {
	double cur_seek_pos = get_parameter(seek_pos_request);

	AnimationMixer::PlaybackInfo pi = p_playback_info;
	pi.weight = 1.0;
	if (Animation::is_greater_or_equal_approx(cur_seek_pos, 0)) {
		pi.time = cur_seek_pos;
		pi.seeked = true;
		pi.is_external_seeking = explicit_elapse;
		set_parameter(seek_pos_request, -1.0); // Reset.
	}

	return blend_input(0, pi, FILTER_IGNORE, true, p_test_only);
}

AnimationNodeTimeSeek::AnimationNodeTimeSeek() {
	add_input("in");
}

void AnimationNodeTimeSeek::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_explicit_elapse", "enable"), &AnimationNodeTimeSeek::set_explicit_elapse);
	ClassDB::bind_method(D_METHOD("is_explicit_elapse"), &AnimationNodeTimeSeek::is_explicit_elapse);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "explicit_elapse"), "set_explicit_elapse", "is_explicit_elapse");
}

/////////////////////////////////////////////////

bool AnimationNodeTransition::_set(const StringName &p_path, const Variant &p_value) {
	String path = p_path;

	if (!path.begins_with("input_")) {
		return false;
	}

	int which = path.get_slicec('/', 0).get_slicec('_', 1).to_int();
	String what = path.get_slicec('/', 1);

	if (which == get_input_count() && what == "name") {
		if (add_input(p_value)) {
			return true;
		}
		return false;
	}

	ERR_FAIL_INDEX_V(which, get_input_count(), false);

	if (what == "name") {
		set_input_name(which, p_value);
	} else if (what == "auto_advance") {
		set_input_as_auto_advance(which, p_value);
	} else if (what == "break_loop_at_end") {
		set_input_break_loop_at_end(which, p_value);
	} else if (what == "reset") {
		set_input_reset(which, p_value);
	} else {
		return false;
	}

	return true;
}

bool AnimationNodeTransition::_get(const StringName &p_path, Variant &r_ret) const {
	String path = p_path;

	if (!path.begins_with("input_")) {
		return false;
	}

	int which = path.get_slicec('/', 0).get_slicec('_', 1).to_int();
	String what = path.get_slicec('/', 1);

	ERR_FAIL_INDEX_V(which, get_input_count(), false);

	if (what == "name") {
		r_ret = get_input_name(which);
	} else if (what == "auto_advance") {
		r_ret = is_input_set_as_auto_advance(which);
	} else if (what == "break_loop_at_end") {
		r_ret = is_input_loop_broken_at_end(which);
	} else if (what == "reset") {
		r_ret = is_input_reset(which);
	} else {
		return false;
	}

	return true;
}

void AnimationNodeTransition::get_parameter_list(List<PropertyInfo> *r_list) const {
	AnimationNode::get_parameter_list(r_list);
	String anims;
	for (int i = 0; i < get_input_count(); i++) {
		if (i > 0) {
			anims += ",";
		}
		anims += inputs[i].name;
	}

	r_list->push_back(PropertyInfo(Variant::STRING, current_state, PROPERTY_HINT_NONE, "", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_READ_ONLY)); // For interface.
	r_list->push_back(PropertyInfo(Variant::STRING, transition_request, PROPERTY_HINT_ENUM, anims)); // For transition request. It will be cleared after setting the value immediately.
	r_list->push_back(PropertyInfo(Variant::INT, current_index, PROPERTY_HINT_NONE, "", PROPERTY_USAGE_STORAGE | PROPERTY_USAGE_READ_ONLY)); // To avoid finding the index every frame, use this internally.
	r_list->push_back(PropertyInfo(Variant::INT, prev_index, PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE));
	r_list->push_back(PropertyInfo(Variant::FLOAT, prev_xfading, PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE));
}

Variant AnimationNodeTransition::get_parameter_default_value(const StringName &p_parameter) const {
	Variant ret = AnimationNode::get_parameter_default_value(p_parameter);
	if (ret != Variant()) {
		return ret;
	}

	if (p_parameter == prev_xfading) {
		return 0.0;
	} else if (p_parameter == prev_index || p_parameter == current_index) {
		return -1;
	} else {
		return String();
	}
}

bool AnimationNodeTransition::is_parameter_read_only(const StringName &p_parameter) const {
	if (AnimationNode::is_parameter_read_only(p_parameter)) {
		return true;
	}

	if (p_parameter == current_state || p_parameter == current_index) {
		return true;
	}
	return false;
}

String AnimationNodeTransition::get_caption() const {
	return "Transition";
}

void AnimationNodeTransition::set_input_count(int p_inputs) {
	for (int i = get_input_count(); i < p_inputs; i++) {
		add_input("state_" + itos(i));
	}
	while (get_input_count() > p_inputs) {
		remove_input(get_input_count() - 1);
	}

	pending_update = true;

	emit_signal(SNAME("tree_changed")); // For updating connect activity map.
	notify_property_list_changed();
}

bool AnimationNodeTransition::add_input(const String &p_name) {
	if (AnimationNode::add_input(p_name)) {
		input_data.push_back(InputData());
		return true;
	}
	return false;
}

void AnimationNodeTransition::remove_input(int p_index) {
	input_data.remove_at(p_index);
	AnimationNode::remove_input(p_index);
}

bool AnimationNodeTransition::set_input_name(int p_input, const String &p_name) {
	pending_update = true;
	if (!AnimationNode::set_input_name(p_input, p_name)) {
		return false;
	}
	emit_signal(SNAME("tree_changed")); // For updating enum options.
	return true;
}

void AnimationNodeTransition::set_input_as_auto_advance(int p_input, bool p_enable) {
	ERR_FAIL_INDEX(p_input, get_input_count());
	input_data.write[p_input].auto_advance = p_enable;
}

bool AnimationNodeTransition::is_input_set_as_auto_advance(int p_input) const {
	ERR_FAIL_INDEX_V(p_input, get_input_count(), false);
	return input_data[p_input].auto_advance;
}

void AnimationNodeTransition::set_input_break_loop_at_end(int p_input, bool p_enable) {
	ERR_FAIL_INDEX(p_input, get_input_count());
	input_data.write[p_input].break_loop_at_end = p_enable;
}

bool AnimationNodeTransition::is_input_loop_broken_at_end(int p_input) const {
	ERR_FAIL_INDEX_V(p_input, get_input_count(), false);
	return input_data[p_input].break_loop_at_end;
}

void AnimationNodeTransition::set_input_reset(int p_input, bool p_enable) {
	ERR_FAIL_INDEX(p_input, get_input_count());
	input_data.write[p_input].reset = p_enable;
}

bool AnimationNodeTransition::is_input_reset(int p_input) const {
	ERR_FAIL_INDEX_V(p_input, get_input_count(), true);
	return input_data[p_input].reset;
}

void AnimationNodeTransition::set_xfade_time(double p_fade) {
	xfade_time = p_fade;
}

double AnimationNodeTransition::get_xfade_time() const {
	return xfade_time;
}

void AnimationNodeTransition::set_xfade_curve(const Ref<Curve> &p_curve) {
	xfade_curve = p_curve;
}

Ref<Curve> AnimationNodeTransition::get_xfade_curve() const {
	return xfade_curve;
}

void AnimationNodeTransition::set_allow_transition_to_self(bool p_enable) {
	allow_transition_to_self = p_enable;
}

bool AnimationNodeTransition::is_allow_transition_to_self() const {
	return allow_transition_to_self;
}

AnimationNode::NodeTimeInfo AnimationNodeTransition::_process(const AnimationMixer::PlaybackInfo p_playback_info, bool p_test_only) {
	String cur_transition_request = get_parameter(transition_request);
	int cur_current_index = get_parameter(current_index);
	int cur_prev_index = get_parameter(prev_index);

	NodeTimeInfo cur_nti = get_node_time_info();
	double cur_prev_xfading = get_parameter(prev_xfading);

	bool switched = false;
	bool restart = false;
	bool clear_remaining_fade = false;

	if (pending_update) {
		if (cur_current_index < 0 || cur_current_index >= get_input_count()) {
			set_parameter(prev_index, -1);
			if (get_input_count() > 0) {
				set_parameter(current_index, 0);
				set_parameter(current_state, get_input_name(0));
			} else {
				set_parameter(current_index, -1);
				set_parameter(current_state, StringName());
			}
		} else {
			set_parameter(current_state, get_input_name(cur_current_index));
		}
		pending_update = false;
	}

	double p_time = p_playback_info.time;
	bool p_seek = p_playback_info.seeked;
	bool p_is_external_seeking = p_playback_info.is_external_seeking;

	if (Math::is_zero_approx(p_time) && p_seek && !p_is_external_seeking) {
		clear_remaining_fade = true; // Reset occurs.
	}

	if (!cur_transition_request.is_empty()) {
		int new_idx = find_input(cur_transition_request);
		if (new_idx >= 0) {
			if (cur_current_index == new_idx) {
				if (allow_transition_to_self) {
					// Transition to same state.
					restart = input_data[cur_current_index].reset;
					clear_remaining_fade = true;
				}
			} else {
				switched = true;
				cur_prev_index = cur_current_index;
				set_parameter(prev_index, cur_current_index);
				cur_current_index = new_idx;
				set_parameter(current_index, cur_current_index);
				set_parameter(current_state, cur_transition_request);
			}
		} else {
			ERR_PRINT("No such input: '" + cur_transition_request + "'");
		}
		cur_transition_request = String();
		set_parameter(transition_request, cur_transition_request);
	}

	if (clear_remaining_fade) {
		cur_prev_xfading = 0;
		set_parameter(prev_xfading, 0);
		cur_prev_index = -1;
		set_parameter(prev_index, -1);
	}

	AnimationMixer::PlaybackInfo pi = p_playback_info;

	// Special case for restart.
	if (restart) {
		pi.time = 0;
		pi.seeked = true;
		pi.weight = 1.0;
		return blend_input(cur_current_index, pi, FILTER_IGNORE, true, p_test_only);
	}

	if (switched) {
		cur_prev_xfading = xfade_time;
	}

	if (cur_current_index < 0 || cur_current_index >= get_input_count() || cur_prev_index >= get_input_count()) {
		return NodeTimeInfo();
	}

	if (sync) {
		pi.weight = 0;
		for (int i = 0; i < get_input_count(); i++) {
			if (i != cur_current_index && i != cur_prev_index) {
				blend_input(i, pi, FILTER_IGNORE, true, p_test_only);
			}
		}
	}

	if (cur_prev_index < 0) { // Process current animation, check for transition.
		pi.weight = 1.0;
		cur_nti = blend_input(cur_current_index, pi, FILTER_IGNORE, true, p_test_only);
		if (input_data[cur_current_index].auto_advance && Animation::is_less_or_equal_approx(cur_nti.get_remain(input_data[cur_current_index].break_loop_at_end), xfade_time)) {
			set_parameter(transition_request, get_input_name((cur_current_index + 1) % get_input_count()));
		}
	} else { // Cross-fading from prev to current.

		real_t blend = 0.0;
		real_t blend_inv = 1.0;
		bool use_blend = sync;
		if (xfade_time > 0) {
			use_blend = true;
			blend = cur_prev_xfading / xfade_time;
			if (xfade_curve.is_valid()) {
				blend = xfade_curve->sample(blend);
			}
			blend_inv = 1.0 - blend;
			blend = Math::is_zero_approx(blend) ? CMP_EPSILON : blend;
			blend_inv = Math::is_zero_approx(blend_inv) ? CMP_EPSILON : blend_inv;
		}

		// Blend values must be more than CMP_EPSILON to process discrete keys in edge.
		pi.weight = blend_inv;
		if (input_data[cur_current_index].reset && !p_seek && switched) { // Just switched, seek to start of current.
			pi.time = 0;
			pi.seeked = true;
		}
		cur_nti = blend_input(cur_current_index, pi, FILTER_IGNORE, true, p_test_only);

		pi = p_playback_info;
		pi.seeked &= use_blend;
		pi.weight = blend;
		blend_input(cur_prev_index, pi, FILTER_IGNORE, true, p_test_only);
		if (!p_seek) {
			if (Animation::is_less_or_equal_approx(cur_prev_xfading, 0)) {
				set_parameter(prev_index, -1);
			}
			cur_prev_xfading -= Math::abs(p_playback_info.delta);
		}
	}

	set_parameter(prev_xfading, cur_prev_xfading);

	return cur_nti;
}

void AnimationNodeTransition::_get_property_list(List<PropertyInfo> *p_list) const {
	for (int i = 0; i < get_input_count(); i++) {
		p_list->push_back(PropertyInfo(Variant::STRING, "input_" + itos(i) + "/name", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_INTERNAL));
		p_list->push_back(PropertyInfo(Variant::BOOL, "input_" + itos(i) + "/auto_advance", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_INTERNAL));
		p_list->push_back(PropertyInfo(Variant::BOOL, "input_" + itos(i) + "/break_loop_at_end", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_INTERNAL));
		p_list->push_back(PropertyInfo(Variant::BOOL, "input_" + itos(i) + "/reset", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_INTERNAL));
	}
}

void AnimationNodeTransition::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_input_count", "input_count"), &AnimationNodeTransition::set_input_count);

	ClassDB::bind_method(D_METHOD("set_input_as_auto_advance", "input", "enable"), &AnimationNodeTransition::set_input_as_auto_advance);
	ClassDB::bind_method(D_METHOD("is_input_set_as_auto_advance", "input"), &AnimationNodeTransition::is_input_set_as_auto_advance);

	ClassDB::bind_method(D_METHOD("set_input_break_loop_at_end", "input", "enable"), &AnimationNodeTransition::set_input_break_loop_at_end);
	ClassDB::bind_method(D_METHOD("is_input_loop_broken_at_end", "input"), &AnimationNodeTransition::is_input_loop_broken_at_end);

	ClassDB::bind_method(D_METHOD("set_input_reset", "input", "enable"), &AnimationNodeTransition::set_input_reset);
	ClassDB::bind_method(D_METHOD("is_input_reset", "input"), &AnimationNodeTransition::is_input_reset);

	ClassDB::bind_method(D_METHOD("set_xfade_time", "time"), &AnimationNodeTransition::set_xfade_time);
	ClassDB::bind_method(D_METHOD("get_xfade_time"), &AnimationNodeTransition::get_xfade_time);

	ClassDB::bind_method(D_METHOD("set_xfade_curve", "curve"), &AnimationNodeTransition::set_xfade_curve);
	ClassDB::bind_method(D_METHOD("get_xfade_curve"), &AnimationNodeTransition::get_xfade_curve);

	ClassDB::bind_method(D_METHOD("set_allow_transition_to_self", "enable"), &AnimationNodeTransition::set_allow_transition_to_self);
	ClassDB::bind_method(D_METHOD("is_allow_transition_to_self"), &AnimationNodeTransition::is_allow_transition_to_self);

	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "xfade_time", PROPERTY_HINT_RANGE, "0,120,0.01,suffix:s"), "set_xfade_time", "get_xfade_time");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "xfade_curve", PROPERTY_HINT_RESOURCE_TYPE, "Curve"), "set_xfade_curve", "get_xfade_curve");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "allow_transition_to_self"), "set_allow_transition_to_self", "is_allow_transition_to_self");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "input_count", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_EDITOR | PROPERTY_USAGE_ARRAY | PROPERTY_USAGE_UPDATE_ALL_IF_MODIFIED, "Inputs,input_"), "set_input_count", "get_input_count");
}

AnimationNodeTransition::AnimationNodeTransition() {
}

/////////////////////

String AnimationNodeOutput::get_caption() const {
	return "Output";
}

AnimationNode::NodeTimeInfo AnimationNodeOutput::_process(const AnimationMixer::PlaybackInfo p_playback_info, bool p_test_only) {
	AnimationMixer::PlaybackInfo pi = p_playback_info;
	pi.weight = 1.0;
	return blend_input(0, pi, FILTER_IGNORE, true, p_test_only);
}

AnimationNodeOutput::AnimationNodeOutput() {
	add_input("output");
}

///////////////////////////////////////////////////////
void AnimationNodeBlendTree::add_node(const StringName &p_name, Ref<AnimationNode> p_node, const Vector2 &p_position) {
	ERR_FAIL_COND(nodes.has(p_name));
	ERR_FAIL_COND(p_node.is_null());
	ERR_FAIL_COND(p_name == SceneStringName(output));
	ERR_FAIL_COND(String(p_name).contains_char('/'));

	Node n;
	n.node = p_node;
	n.position = p_position;
	n.connections.resize(n.node->get_input_count());
	nodes[p_name] = n;

	emit_changed();
	emit_signal(SNAME("tree_changed"));

	p_node->connect(SNAME("tree_changed"), callable_mp(this, &AnimationNodeBlendTree::_tree_changed), CONNECT_REFERENCE_COUNTED);
	p_node->connect(SNAME("animation_node_renamed"), callable_mp(this, &AnimationNodeBlendTree::_animation_node_renamed), CONNECT_REFERENCE_COUNTED);
	p_node->connect(SNAME("animation_node_removed"), callable_mp(this, &AnimationNodeBlendTree::_animation_node_removed), CONNECT_REFERENCE_COUNTED);
	p_node->connect_changed(callable_mp(this, &AnimationNodeBlendTree::_node_changed).bind(p_name), CONNECT_REFERENCE_COUNTED);
}

Ref<AnimationNode> AnimationNodeBlendTree::get_node(const StringName &p_name) const {
	ERR_FAIL_COND_V(!nodes.has(p_name), Ref<AnimationNode>());

	return nodes[p_name].node;
}

StringName AnimationNodeBlendTree::get_node_name(const Ref<AnimationNode> &p_node) const {
	for (const KeyValue<StringName, Node> &E : nodes) {
		if (E.value.node == p_node) {
			return E.key;
		}
	}

	ERR_FAIL_V(StringName());
}

void AnimationNodeBlendTree::set_node_position(const StringName &p_node, const Vector2 &p_position) {
	ERR_FAIL_COND(!nodes.has(p_node));
	nodes[p_node].position = p_position;
}

Vector2 AnimationNodeBlendTree::get_node_position(const StringName &p_node) const {
	ERR_FAIL_COND_V(!nodes.has(p_node), Vector2());
	return nodes[p_node].position;
}

void AnimationNodeBlendTree::get_child_nodes(List<ChildNode> *r_child_nodes) {
	Vector<StringName> ns;

	for (const KeyValue<StringName, Node> &E : nodes) {
		ns.push_back(E.key);
	}

	for (int i = 0; i < ns.size(); i++) {
		ChildNode cn;
		cn.name = ns[i];
		cn.node = nodes[cn.name].node;
		r_child_nodes->push_back(cn);
	}
}

bool AnimationNodeBlendTree::has_node(const StringName &p_name) const {
	return nodes.has(p_name);
}

Vector<StringName> AnimationNodeBlendTree::get_node_connection_array(const StringName &p_name) const {
	ERR_FAIL_COND_V(!nodes.has(p_name), Vector<StringName>());
	return nodes[p_name].connections;
}

void AnimationNodeBlendTree::remove_node(const StringName &p_name) {
	ERR_FAIL_COND(!nodes.has(p_name));
	ERR_FAIL_COND(p_name == SceneStringName(output)); //can't delete output

	{
		Ref<AnimationNode> node = nodes[p_name].node;
		node->disconnect(SNAME("tree_changed"), callable_mp(this, &AnimationNodeBlendTree::_tree_changed));
		node->disconnect(SNAME("animation_node_renamed"), callable_mp(this, &AnimationNodeBlendTree::_animation_node_renamed));
		node->disconnect(SNAME("animation_node_removed"), callable_mp(this, &AnimationNodeBlendTree::_animation_node_removed));
		node->disconnect_changed(callable_mp(this, &AnimationNodeBlendTree::_node_changed));
	}

	nodes.erase(p_name);

	// Erase connections to name.
	for (KeyValue<StringName, Node> &E : nodes) {
		for (int i = 0; i < E.value.connections.size(); i++) {
			if (E.value.connections[i] == p_name) {
				E.value.connections.write[i] = StringName();
			}
		}
	}

	emit_signal(SNAME("animation_node_removed"), get_instance_id(), p_name);
	emit_changed();
	emit_signal(SNAME("tree_changed"));
}

void AnimationNodeBlendTree::rename_node(const StringName &p_name, const StringName &p_new_name) {
	ERR_FAIL_COND(!nodes.has(p_name));
	ERR_FAIL_COND(nodes.has(p_new_name));
	ERR_FAIL_COND(p_name == SceneStringName(output));
	ERR_FAIL_COND(p_new_name == SceneStringName(output));

	nodes[p_name].node->disconnect_changed(callable_mp(this, &AnimationNodeBlendTree::_node_changed));

	nodes[p_new_name] = nodes[p_name];
	nodes.erase(p_name);

	// Rename connections.
	for (KeyValue<StringName, Node> &E : nodes) {
		for (int i = 0; i < E.value.connections.size(); i++) {
			if (E.value.connections[i] == p_name) {
				E.value.connections.write[i] = p_new_name;
			}
		}
	}
	// Connection must be done with new name.
	nodes[p_new_name].node->connect_changed(callable_mp(this, &AnimationNodeBlendTree::_node_changed).bind(p_new_name), CONNECT_REFERENCE_COUNTED);

	emit_signal(SNAME("animation_node_renamed"), get_instance_id(), p_name, p_new_name);
	emit_signal(SNAME("tree_changed"));
}

void AnimationNodeBlendTree::connect_node(const StringName &p_input_node, int p_input_index, const StringName &p_output_node) {
	ERR_FAIL_COND(!nodes.has(p_output_node));
	ERR_FAIL_COND(!nodes.has(p_input_node));
	ERR_FAIL_COND(p_output_node == SceneStringName(output));
	ERR_FAIL_COND(p_input_node == p_output_node);

	Ref<AnimationNode> input = nodes[p_input_node].node;
	ERR_FAIL_INDEX(p_input_index, nodes[p_input_node].connections.size());

	for (KeyValue<StringName, Node> &E : nodes) {
		for (int i = 0; i < E.value.connections.size(); i++) {
			StringName output = E.value.connections[i];
			ERR_FAIL_COND(output == p_output_node);
		}
	}

	nodes[p_input_node].connections.write[p_input_index] = p_output_node;

	emit_changed();
}

void AnimationNodeBlendTree::disconnect_node(const StringName &p_node, int p_input_index) {
	ERR_FAIL_COND(!nodes.has(p_node));

	Ref<AnimationNode> input = nodes[p_node].node;
	ERR_FAIL_INDEX(p_input_index, nodes[p_node].connections.size());

	nodes[p_node].connections.write[p_input_index] = StringName();
}

AnimationNodeBlendTree::ConnectionError AnimationNodeBlendTree::can_connect_node(const StringName &p_input_node, int p_input_index, const StringName &p_output_node) const {
	if (!nodes.has(p_output_node) || p_output_node == SceneStringName(output)) {
		return CONNECTION_ERROR_NO_OUTPUT;
	}

	if (!nodes.has(p_input_node)) {
		return CONNECTION_ERROR_NO_INPUT;
	}

	if (p_input_node == p_output_node) {
		return CONNECTION_ERROR_SAME_NODE;
	}

	Ref<AnimationNode> input = nodes[p_input_node].node;

	if (p_input_index < 0 || p_input_index >= nodes[p_input_node].connections.size()) {
		return CONNECTION_ERROR_NO_INPUT_INDEX;
	}

	if (nodes[p_input_node].connections[p_input_index] != StringName()) {
		return CONNECTION_ERROR_CONNECTION_EXISTS;
	}

	for (const KeyValue<StringName, Node> &E : nodes) {
		for (int i = 0; i < E.value.connections.size(); i++) {
			const StringName output = E.value.connections[i];
			if (output == p_output_node) {
				return CONNECTION_ERROR_CONNECTION_EXISTS;
			}
		}
	}
	return CONNECTION_OK;
}

void AnimationNodeBlendTree::get_node_connections(List<NodeConnection> *r_connections) const {
	for (const KeyValue<StringName, Node> &E : nodes) {
		for (int i = 0; i < E.value.connections.size(); i++) {
			const StringName output = E.value.connections[i];
			if (output != StringName()) {
				NodeConnection nc;
				nc.input_node = E.key;
				nc.input_index = i;
				nc.output_node = output;
				r_connections->push_back(nc);
			}
		}
	}
}

String AnimationNodeBlendTree::get_caption() const {
	return "BlendTree";
}

AnimationNode::NodeTimeInfo AnimationNodeBlendTree::_process(const AnimationMixer::PlaybackInfo p_playback_info, bool p_test_only) {
	Ref<AnimationNodeOutput> output = nodes[SceneStringName(output)].node;
	node_state.connections = nodes[SceneStringName(output)].connections;
	ERR_FAIL_COND_V(output.is_null(), NodeTimeInfo());

	AnimationMixer::PlaybackInfo pi = p_playback_info;
	pi.weight = 1.0;

	return _blend_node(output, "output", this, pi, FILTER_IGNORE, true, p_test_only, nullptr);
}

void AnimationNodeBlendTree::get_node_list(List<StringName> *r_list) {
	for (const KeyValue<StringName, Node> &E : nodes) {
		r_list->push_back(E.key);
	}
}

void AnimationNodeBlendTree::set_graph_offset(const Vector2 &p_graph_offset) {
	graph_offset = p_graph_offset;
}

Vector2 AnimationNodeBlendTree::get_graph_offset() const {
	return graph_offset;
}

Ref<AnimationNode> AnimationNodeBlendTree::get_child_by_name(const StringName &p_name) const {
	return get_node(p_name);
}

bool AnimationNodeBlendTree::_set(const StringName &p_name, const Variant &p_value) {
	String prop_name = p_name;
	if (prop_name.begins_with("nodes/")) {
		String node_name = prop_name.get_slicec('/', 1);
		String what = prop_name.get_slicec('/', 2);

		if (what == "node") {
			Ref<AnimationNode> anode = p_value;
			if (anode.is_valid()) {
				add_node(node_name, p_value);
			}
			return true;
		}

		if (what == "position") {
			if (nodes.has(node_name)) {
				nodes[node_name].position = p_value;
			}
			return true;
		}
	} else if (prop_name == "node_connections") {
		Array conns = p_value;
		ERR_FAIL_COND_V(conns.size() % 3 != 0, false);

		for (int i = 0; i < conns.size(); i += 3) {
			connect_node(conns[i], conns[i + 1], conns[i + 2]);
		}
		return true;
	}

	return false;
}

bool AnimationNodeBlendTree::_get(const StringName &p_name, Variant &r_ret) const {
	String prop_name = p_name;
	if (prop_name.begins_with("nodes/")) {
		String node_name = prop_name.get_slicec('/', 1);
		String what = prop_name.get_slicec('/', 2);

		if (what == "node") {
			if (nodes.has(node_name)) {
				r_ret = nodes[node_name].node;
				return true;
			}
		}

		if (what == "position") {
			if (nodes.has(node_name)) {
				r_ret = nodes[node_name].position;
				return true;
			}
		}
	} else if (prop_name == "node_connections") {
		List<NodeConnection> nc;
		get_node_connections(&nc);
		Array conns;
		conns.resize(nc.size() * 3);

		int idx = 0;
		for (const NodeConnection &E : nc) {
			conns[idx * 3 + 0] = E.input_node;
			conns[idx * 3 + 1] = E.input_index;
			conns[idx * 3 + 2] = E.output_node;
			idx++;
		}

		r_ret = conns;
		return true;
	}

	return false;
}

void AnimationNodeBlendTree::_get_property_list(List<PropertyInfo> *p_list) const {
	List<StringName> names;
	for (const KeyValue<StringName, Node> &E : nodes) {
		names.push_back(E.key);
	}

	for (const StringName &E : names) {
		String prop_name = E;
		if (prop_name != "output") {
			p_list->push_back(PropertyInfo(Variant::OBJECT, "nodes/" + prop_name + "/node", PROPERTY_HINT_RESOURCE_TYPE, "AnimationNode", PROPERTY_USAGE_NO_EDITOR));
		}
		p_list->push_back(PropertyInfo(Variant::VECTOR2, "nodes/" + prop_name + "/position", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NO_EDITOR));
	}

	p_list->push_back(PropertyInfo(Variant::ARRAY, "node_connections", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NO_EDITOR));
}

void AnimationNodeBlendTree::_tree_changed() {
	AnimationRootNode::_tree_changed();
}

void AnimationNodeBlendTree::_animation_node_renamed(const ObjectID &p_oid, const String &p_old_name, const String &p_new_name) {
	AnimationRootNode::_animation_node_renamed(p_oid, p_old_name, p_new_name);
}

void AnimationNodeBlendTree::_animation_node_removed(const ObjectID &p_oid, const StringName &p_node) {
	AnimationRootNode::_animation_node_removed(p_oid, p_node);
}

void AnimationNodeBlendTree::reset_state() {
	graph_offset = Vector2();
	nodes.clear();
	_initialize_node_tree();
	emit_changed();
	emit_signal(SNAME("tree_changed"));
}

void AnimationNodeBlendTree::_node_changed(const StringName &p_node) {
	ERR_FAIL_COND(!nodes.has(p_node));
	nodes[p_node].connections.resize(nodes[p_node].node->get_input_count());
	emit_signal(SNAME("node_changed"), p_node);
}

#ifdef TOOLS_ENABLED
void AnimationNodeBlendTree::get_argument_options(const StringName &p_function, int p_idx, List<String> *r_options) const {
	const String pf = p_function;
	bool add_node_options = false;
	if (p_idx == 0) {
		add_node_options = (pf == "get_node" || pf == "has_node" || pf == "rename_node" || pf == "remove_node" || pf == "set_node_position" || pf == "get_node_position" || pf == "connect_node" || pf == "disconnect_node");
	} else if (p_idx == 2) {
		add_node_options = (pf == "connect_node" || pf == "disconnect_node");
	}
	if (add_node_options) {
		for (const KeyValue<StringName, Node> &E : nodes) {
			r_options->push_back(String(E.key).quote());
		}
	}
	AnimationRootNode::get_argument_options(p_function, p_idx, r_options);
}
#endif

void AnimationNodeBlendTree::_bind_methods() {
	ClassDB::bind_method(D_METHOD("add_node", "name", "node", "position"), &AnimationNodeBlendTree::add_node, DEFVAL(Vector2()));
	ClassDB::bind_method(D_METHOD("get_node", "name"), &AnimationNodeBlendTree::get_node);
	ClassDB::bind_method(D_METHOD("remove_node", "name"), &AnimationNodeBlendTree::remove_node);
	ClassDB::bind_method(D_METHOD("rename_node", "name", "new_name"), &AnimationNodeBlendTree::rename_node);
	ClassDB::bind_method(D_METHOD("has_node", "name"), &AnimationNodeBlendTree::has_node);
	ClassDB::bind_method(D_METHOD("connect_node", "input_node", "input_index", "output_node"), &AnimationNodeBlendTree::connect_node);
	ClassDB::bind_method(D_METHOD("disconnect_node", "input_node", "input_index"), &AnimationNodeBlendTree::disconnect_node);

	ClassDB::bind_method(D_METHOD("set_node_position", "name", "position"), &AnimationNodeBlendTree::set_node_position);
	ClassDB::bind_method(D_METHOD("get_node_position", "name"), &AnimationNodeBlendTree::get_node_position);

	ClassDB::bind_method(D_METHOD("set_graph_offset", "offset"), &AnimationNodeBlendTree::set_graph_offset);
	ClassDB::bind_method(D_METHOD("get_graph_offset"), &AnimationNodeBlendTree::get_graph_offset);

	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "graph_offset", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NO_EDITOR), "set_graph_offset", "get_graph_offset");

	BIND_CONSTANT(CONNECTION_OK);
	BIND_CONSTANT(CONNECTION_ERROR_NO_INPUT);
	BIND_CONSTANT(CONNECTION_ERROR_NO_INPUT_INDEX);
	BIND_CONSTANT(CONNECTION_ERROR_NO_OUTPUT);
	BIND_CONSTANT(CONNECTION_ERROR_SAME_NODE);
	BIND_CONSTANT(CONNECTION_ERROR_CONNECTION_EXISTS);

	ADD_SIGNAL(MethodInfo(SNAME("node_changed"), PropertyInfo(Variant::STRING_NAME, "node_name")));
}

void AnimationNodeBlendTree::_initialize_node_tree() {
	Ref<AnimationNodeOutput> output;
	output.instantiate();
	Node n;
	n.node = output;
	n.position = Vector2(300, 150);
	n.connections.resize(1);
	nodes["output"] = n;
}

AnimationNodeBlendTree::AnimationNodeBlendTree() {
	_initialize_node_tree();
}

AnimationNodeBlendTree::~AnimationNodeBlendTree() {
}

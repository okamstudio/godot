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

#include "scene/scene_string_names.h"

void AnimationNodeAnimation::set_animation(const StringName &p_name) {
	animation = p_name;
	_change_notify("animation");
}

StringName AnimationNodeAnimation::get_animation() const {
	return animation;
}

Vector<String> (*AnimationNodeAnimation::get_editable_animation_list)() = nullptr;

void AnimationNodeAnimation::get_parameter_list(List<PropertyInfo> *r_list) const {
	r_list->push_back(PropertyInfo(Variant::REAL, time, PROPERTY_HINT_NONE, "", 0));
}
void AnimationNodeAnimation::_validate_property(PropertyInfo &property) const {
	if (property.name == "animation" && get_editable_animation_list) {
		Vector<String> names = get_editable_animation_list();
		String anims;
		for (int i = 0; i < names.size(); i++) {
			if (i > 0) {
				anims += ",";
			}
			anims += String(names[i]);
		}
		if (anims != String()) {
			property.hint = PROPERTY_HINT_ENUM;
			property.hint_string = anims;
		}
	}
}

float AnimationNodeAnimation::process(float p_time, bool p_seek) {
	AnimationPlayer *ap = state->player;
	ERR_FAIL_COND_V(!ap, 0);

	const float current_time = get_parameter(this->time);

	if (!ap->has_animation(animation)) {
		AnimationNodeBlendTree *tree = Object::cast_to<AnimationNodeBlendTree>(parent);
		if (tree) {
			String name = tree->get_node_name(Ref<AnimationNodeAnimation>(this));
			make_invalid(vformat(RTR("On BlendTree node '%s', animation not found: '%s'"), name, animation));

		} else {
			make_invalid(vformat(RTR("Animation not found: '%s'"), animation));
		}

		return 0;
	}

	Ref<Animation> anim = ap->get_animation(animation);

	float time = current_time;
	float step;

	if (p_seek) {
		time = p_time;
		step = 0;
	} else {
		time = MAX(0, time + p_time);
		step = p_time;
	}

	float anim_size = anim->get_length();

	if (anim->has_loop()) {
		if (anim_size) {
			time = Math::fposmod(time, anim_size);
		}

	} else if (time > anim_size) {
		time = anim_size;
		step = anim_size - current_time;
	}

	blend_animation(animation, time, step, p_seek, 1.0);

	set_parameter(this->time, time);

	return anim_size - time;
}

String AnimationNodeAnimation::get_caption() const {
	return "Animation";
}

void AnimationNodeAnimation::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_animation", "name"), &AnimationNodeAnimation::set_animation);
	ClassDB::bind_method(D_METHOD("get_animation"), &AnimationNodeAnimation::get_animation);

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "animation"), "set_animation", "get_animation");
}

AnimationNodeAnimation::AnimationNodeAnimation() {
	last_version = 0;
	skip = false;
	time = "time";
}

////////////////////////////////////////////////////////

void AnimationNodeOneShot::get_parameter_list(List<PropertyInfo> *r_list) const {
	r_list->push_back(PropertyInfo(Variant::BOOL, active));
	r_list->push_back(PropertyInfo(Variant::BOOL, prev_active, PROPERTY_HINT_NONE, "", 0));
	r_list->push_back(PropertyInfo(Variant::REAL, time, PROPERTY_HINT_NONE, "", 0));
	r_list->push_back(PropertyInfo(Variant::REAL, remaining, PROPERTY_HINT_NONE, "", 0));
	r_list->push_back(PropertyInfo(Variant::REAL, time_to_restart, PROPERTY_HINT_NONE, "", 0));
}

Variant AnimationNodeOneShot::get_parameter_default_value(const StringName &p_parameter) const {
	if (p_parameter == active || p_parameter == prev_active) {
		return false;
	} else if (p_parameter == time_to_restart) {
		return -1;
	} else {
		return 0.0;
	}
}

void AnimationNodeOneShot::set_fadein_time(float p_time) {
	fade_in = p_time;
}

void AnimationNodeOneShot::set_fadeout_time(float p_time) {
	fade_out = p_time;
}

float AnimationNodeOneShot::get_fadein_time() const {
	return fade_in;
}
float AnimationNodeOneShot::get_fadeout_time() const {
	return fade_out;
}

void AnimationNodeOneShot::set_autorestart(bool p_active) {
	autorestart = p_active;
}
void AnimationNodeOneShot::set_autorestart_delay(float p_time) {
	autorestart_delay = p_time;
}
void AnimationNodeOneShot::set_autorestart_random_delay(float p_time) {
	autorestart_random_delay = p_time;
}

bool AnimationNodeOneShot::has_autorestart() const {
	return autorestart;
}
float AnimationNodeOneShot::get_autorestart_delay() const {
	return autorestart_delay;
}
float AnimationNodeOneShot::get_autorestart_random_delay() const {
	return autorestart_random_delay;
}

void AnimationNodeOneShot::set_mix_mode(MixMode p_mix) {
	mix = p_mix;
}
AnimationNodeOneShot::MixMode AnimationNodeOneShot::get_mix_mode() const {
	return mix;
}

String AnimationNodeOneShot::get_caption() const {
	return "OneShot";
}

bool AnimationNodeOneShot::has_filter() const {
	return true;
}

float AnimationNodeOneShot::process(float p_time, bool p_seek) {
	bool active = get_parameter(this->active);
	bool prev_active = get_parameter(this->prev_active);
	float time = get_parameter(this->time);
	float remaining = get_parameter(this->remaining);
	float time_to_restart = get_parameter(this->time_to_restart);

	if (!active) {
		//make it as if this node doesn't exist, pass input 0 by.
		if (prev_active) {
			set_parameter(this->prev_active, false);
		}
		if (time_to_restart >= 0.0 && !p_seek) {
			time_to_restart -= p_time;
			if (time_to_restart < 0) {
				//restart
				set_parameter(this->active, true);
				active = true;
			}
			set_parameter(this->time_to_restart, time_to_restart);
		}

		if (!active) {
			return blend_input(0, p_time, p_seek, 1.0, FILTER_IGNORE, !sync);
		}
	}

	bool os_seek = p_seek;

	if (p_seek) {
		time = p_time;
	}
	bool do_start = !prev_active;

	if (do_start) {
		time = 0;
		os_seek = true;
		set_parameter(this->prev_active, true);
	}

	float blend;

	if (time < fade_in) {
		if (fade_in > 0) {
			blend = time / fade_in;
		} else {
			blend = 0; //wtf
		}

	} else if (!do_start && remaining < fade_out) {
		if (fade_out) {
			blend = (remaining / fade_out);
		} else {
			blend = 1.0;
		}
	} else {
		blend = 1.0;
	}

	float main_rem;
	if (mix == MIX_MODE_ADD) {
		main_rem = blend_input(0, p_time, p_seek, 1.0, FILTER_IGNORE, !sync);
	} else {
		main_rem = blend_input(0, p_time, p_seek, 1.0 - blend, FILTER_BLEND, !sync);
	}

	float os_rem = blend_input(1, os_seek ? time : p_time, os_seek, blend, FILTER_PASS, false);

	if (do_start) {
		remaining = os_rem;
	}

	if (!p_seek) {
		time += p_time;
		remaining = os_rem;
		if (remaining <= 0) {
			set_parameter(this->active, false);
			set_parameter(this->prev_active, false);
			if (autorestart) {
				float restart_sec = autorestart_delay + Math::randf() * autorestart_random_delay;
				set_parameter(this->time_to_restart, restart_sec);
			}
		}
	}

	set_parameter(this->time, time);
	set_parameter(this->remaining, remaining);

	return MAX(main_rem, remaining);
}
void AnimationNodeOneShot::set_use_sync(bool p_sync) {
	sync = p_sync;
}

bool AnimationNodeOneShot::is_using_sync() const {
	return sync;
}

void AnimationNodeOneShot::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_fadein_time", "time"), &AnimationNodeOneShot::set_fadein_time);
	ClassDB::bind_method(D_METHOD("get_fadein_time"), &AnimationNodeOneShot::get_fadein_time);

	ClassDB::bind_method(D_METHOD("set_fadeout_time", "time"), &AnimationNodeOneShot::set_fadeout_time);
	ClassDB::bind_method(D_METHOD("get_fadeout_time"), &AnimationNodeOneShot::get_fadeout_time);

	ClassDB::bind_method(D_METHOD("set_autorestart", "enable"), &AnimationNodeOneShot::set_autorestart);
	ClassDB::bind_method(D_METHOD("has_autorestart"), &AnimationNodeOneShot::has_autorestart);

	ClassDB::bind_method(D_METHOD("set_autorestart_delay", "enable"), &AnimationNodeOneShot::set_autorestart_delay);
	ClassDB::bind_method(D_METHOD("get_autorestart_delay"), &AnimationNodeOneShot::get_autorestart_delay);

	ClassDB::bind_method(D_METHOD("set_autorestart_random_delay", "enable"), &AnimationNodeOneShot::set_autorestart_random_delay);
	ClassDB::bind_method(D_METHOD("get_autorestart_random_delay"), &AnimationNodeOneShot::get_autorestart_random_delay);

	ClassDB::bind_method(D_METHOD("set_mix_mode", "mode"), &AnimationNodeOneShot::set_mix_mode);
	ClassDB::bind_method(D_METHOD("get_mix_mode"), &AnimationNodeOneShot::get_mix_mode);

	ClassDB::bind_method(D_METHOD("set_use_sync", "enable"), &AnimationNodeOneShot::set_use_sync);
	ClassDB::bind_method(D_METHOD("is_using_sync"), &AnimationNodeOneShot::is_using_sync);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "mix_mode", PROPERTY_HINT_ENUM, "Blend,Add"), "set_mix_mode", "get_mix_mode");

	ADD_PROPERTY(PropertyInfo(Variant::REAL, "fadein_time", PROPERTY_HINT_RANGE, "0,60,0.01,or_greater"), "set_fadein_time", "get_fadein_time");
	ADD_PROPERTY(PropertyInfo(Variant::REAL, "fadeout_time", PROPERTY_HINT_RANGE, "0,60,0.01,or_greater"), "set_fadeout_time", "get_fadeout_time");

	ADD_GROUP("Auto Restart", "autorestart_");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "autorestart"), "set_autorestart", "has_autorestart");

	ADD_PROPERTY(PropertyInfo(Variant::REAL, "autorestart_delay", PROPERTY_HINT_RANGE, "0,60,0.01,or_greater"), "set_autorestart_delay", "get_autorestart_delay");
	ADD_PROPERTY(PropertyInfo(Variant::REAL, "autorestart_random_delay", PROPERTY_HINT_RANGE, "0,60,0.01,or_greater"), "set_autorestart_random_delay", "get_autorestart_random_delay");

	ADD_GROUP("", "");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "sync"), "set_use_sync", "is_using_sync");

	BIND_ENUM_CONSTANT(MIX_MODE_BLEND);
	BIND_ENUM_CONSTANT(MIX_MODE_ADD);
}

AnimationNodeOneShot::AnimationNodeOneShot() {
	add_input("in");
	add_input("shot");

	fade_in = 0.1;
	fade_out = 0.1;
	autorestart = false;
	autorestart_delay = 1;
	autorestart_random_delay = 0;

	mix = MIX_MODE_BLEND;
	sync = false;

	active = PNAME("active");
	prev_active = "prev_active";
	time = "time";
	remaining = "remaining";
	time_to_restart = "time_to_restart";
}

////////////////////////////////////////////////

void AnimationNodeAdd2::get_parameter_list(List<PropertyInfo> *r_list) const {
	r_list->push_back(PropertyInfo(Variant::REAL, add_amount, PROPERTY_HINT_RANGE, "0,1,0.01"));
}
Variant AnimationNodeAdd2::get_parameter_default_value(const StringName &p_parameter) const {
	return 0;
}

String AnimationNodeAdd2::get_caption() const {
	return "Add2";
}
void AnimationNodeAdd2::set_use_sync(bool p_sync) {
	sync = p_sync;
}

bool AnimationNodeAdd2::is_using_sync() const {
	return sync;
}

void AnimationNodeAdd2::set_add_directly(bool p_add_directly) {
	add_directly = p_add_directly;
}

bool AnimationNodeAdd2::get_add_directly() const {
	return add_directly;
}

bool AnimationNodeAdd2::has_filter() const {
	return true;
}

float AnimationNodeAdd2::process(float p_time, bool p_seek) {
	float amount = get_parameter(add_amount);
	float rem0 = blend_input(0, p_time, p_seek, 1.0, FILTER_IGNORE, !sync);
	blend_input(1, p_time, p_seek, amount, FILTER_PASS, !sync, add_directly);

	return rem0;
}

void AnimationNodeAdd2::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_use_sync", "enable"), &AnimationNodeAdd2::set_use_sync);
	ClassDB::bind_method(D_METHOD("is_using_sync"), &AnimationNodeAdd2::is_using_sync);

	ClassDB::bind_method(D_METHOD("set_add_directly", "enable"), &AnimationNodeAdd2::set_add_directly);
	ClassDB::bind_method(D_METHOD("get_add_directly"), &AnimationNodeAdd2::get_add_directly);

	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "sync"), "set_use_sync", "is_using_sync");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "add_directly"), "set_add_directly", "get_add_directly");
}

AnimationNodeAdd2::AnimationNodeAdd2() {
	add_amount = PNAME("add_amount");
	add_input("in");
	add_input("add");
	sync = false;
	add_directly = true;
}

////////////////////////////////////////////////

void AnimationNodeAdd3::get_parameter_list(List<PropertyInfo> *r_list) const {
	r_list->push_back(PropertyInfo(Variant::REAL, add_amount, PROPERTY_HINT_RANGE, "-1,1,0.01"));
}
Variant AnimationNodeAdd3::get_parameter_default_value(const StringName &p_parameter) const {
	return 0;
}

String AnimationNodeAdd3::get_caption() const {
	return "Add3";
}
void AnimationNodeAdd3::set_use_sync(bool p_sync) {
	sync = p_sync;
}

bool AnimationNodeAdd3::is_using_sync() const {
	return sync;
}

void AnimationNodeAdd3::set_add_directly(bool p_add_directly) {
	add_directly = p_add_directly;
}

bool AnimationNodeAdd3::get_add_directly() const {
	return add_directly;
}

bool AnimationNodeAdd3::has_filter() const {
	return true;
}

float AnimationNodeAdd3::process(float p_time, bool p_seek) {
	float amount = get_parameter(add_amount);
	float rem0 = blend_input(1, p_time, p_seek, 1.0, FILTER_IGNORE, !sync);
	if (amount < 0) {
		blend_input(0, p_time, p_seek, -amount, FILTER_PASS, !sync, add_directly);
		blend_input(2, p_time, p_seek, 0, FILTER_PASS, !sync);
	} else {
		blend_input(2, p_time, p_seek, amount, FILTER_PASS, !sync, add_directly);
		blend_input(0, p_time, p_seek, 0, FILTER_PASS, !sync);
	}

	return rem0;
}

void AnimationNodeAdd3::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_use_sync", "enable"), &AnimationNodeAdd3::set_use_sync);
	ClassDB::bind_method(D_METHOD("is_using_sync"), &AnimationNodeAdd3::is_using_sync);

	ClassDB::bind_method(D_METHOD("set_add_directly", "enable"), &AnimationNodeAdd3::set_add_directly);
	ClassDB::bind_method(D_METHOD("get_add_directly"), &AnimationNodeAdd3::get_add_directly);

	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "sync"), "set_use_sync", "is_using_sync");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "add_directly"), "set_add_directly", "get_add_directly");
}

AnimationNodeAdd3::AnimationNodeAdd3() {
	add_amount = PNAME("add_amount");
	add_input("-add");
	add_input("in");
	add_input("+add");
	sync = false;
	add_directly = true;
}
/////////////////////////////////////////////

void AnimationNodeBlend2::get_parameter_list(List<PropertyInfo> *r_list) const {
	r_list->push_back(PropertyInfo(Variant::REAL, blend_amount, PROPERTY_HINT_RANGE, "0,1,0.01"));
}
Variant AnimationNodeBlend2::get_parameter_default_value(const StringName &p_parameter) const {
	return 0; //for blend amount
}

String AnimationNodeBlend2::get_caption() const {
	return "Blend2";
}

float AnimationNodeBlend2::process(float p_time, bool p_seek) {
	float amount = get_parameter(blend_amount);

	float rem0 = blend_input(0, p_time, p_seek, 1.0 - amount, FILTER_BLEND, !sync);
	float rem1 = blend_input(1, p_time, p_seek, amount, FILTER_PASS, !sync);

	return amount > 0.5 ? rem1 : rem0; //hacky but good enough
}

void AnimationNodeBlend2::set_use_sync(bool p_sync) {
	sync = p_sync;
}

bool AnimationNodeBlend2::is_using_sync() const {
	return sync;
}

bool AnimationNodeBlend2::has_filter() const {
	return true;
}
void AnimationNodeBlend2::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_use_sync", "enable"), &AnimationNodeBlend2::set_use_sync);
	ClassDB::bind_method(D_METHOD("is_using_sync"), &AnimationNodeBlend2::is_using_sync);

	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "sync"), "set_use_sync", "is_using_sync");
}
AnimationNodeBlend2::AnimationNodeBlend2() {
	blend_amount = PNAME("blend_amount");
	add_input("in");
	add_input("blend");
	sync = false;
}

//////////////////////////////////////

void AnimationNodeBlend3::get_parameter_list(List<PropertyInfo> *r_list) const {
	r_list->push_back(PropertyInfo(Variant::REAL, blend_amount, PROPERTY_HINT_RANGE, "-1,1,0.01"));
}
Variant AnimationNodeBlend3::get_parameter_default_value(const StringName &p_parameter) const {
	return 0; //for blend amount
}

String AnimationNodeBlend3::get_caption() const {
	return "Blend3";
}

void AnimationNodeBlend3::set_use_sync(bool p_sync) {
	sync = p_sync;
}

bool AnimationNodeBlend3::is_using_sync() const {
	return sync;
}

float AnimationNodeBlend3::process(float p_time, bool p_seek) {
	float amount = get_parameter(blend_amount);
	float rem0 = blend_input(0, p_time, p_seek, MAX(0, -amount), FILTER_IGNORE, !sync);
	float rem1 = blend_input(1, p_time, p_seek, 1.0 - ABS(amount), FILTER_IGNORE, !sync);
	float rem2 = blend_input(2, p_time, p_seek, MAX(0, amount), FILTER_IGNORE, !sync);

	return amount > 0.5 ? rem2 : (amount < -0.5 ? rem0 : rem1); //hacky but good enough
}

void AnimationNodeBlend3::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_use_sync", "enable"), &AnimationNodeBlend3::set_use_sync);
	ClassDB::bind_method(D_METHOD("is_using_sync"), &AnimationNodeBlend3::is_using_sync);

	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "sync"), "set_use_sync", "is_using_sync");
}
AnimationNodeBlend3::AnimationNodeBlend3() {
	blend_amount = PNAME("blend_amount");
	add_input("-blend");
	add_input("in");
	add_input("+blend");
	sync = false;
}

/////////////////////////////////

void AnimationNodeTimeScale::get_parameter_list(List<PropertyInfo> *r_list) const {
	r_list->push_back(PropertyInfo(Variant::REAL, scale, PROPERTY_HINT_RANGE, "0,32,0.01,or_greater"));
}
Variant AnimationNodeTimeScale::get_parameter_default_value(const StringName &p_parameter) const {
	return 1.0; //initial timescale
}

String AnimationNodeTimeScale::get_caption() const {
	return "TimeScale";
}

float AnimationNodeTimeScale::process(float p_time, bool p_seek) {
	float scale = get_parameter(this->scale);
	if (p_seek) {
		return blend_input(0, p_time, true, 1.0, FILTER_IGNORE, false);
	} else {
		return blend_input(0, p_time * scale, false, 1.0, FILTER_IGNORE, false);
	}
}

void AnimationNodeTimeScale::_bind_methods() {
}
AnimationNodeTimeScale::AnimationNodeTimeScale() {
	scale = PNAME("scale");
	add_input("in");
}

////////////////////////////////////

void AnimationNodeTimeSeek::get_parameter_list(List<PropertyInfo> *r_list) const {
	r_list->push_back(PropertyInfo(Variant::REAL, seek_pos, PROPERTY_HINT_RANGE, "-1,3600,0.01,or_greater"));
}
Variant AnimationNodeTimeSeek::get_parameter_default_value(const StringName &p_parameter) const {
	return 1.0; //initial timescale
}

String AnimationNodeTimeSeek::get_caption() const {
	return "Seek";
}

float AnimationNodeTimeSeek::process(float p_time, bool p_seek) {
	float seek_pos = get_parameter(this->seek_pos);
	if (p_seek) {
		return blend_input(0, p_time, true, 1.0, FILTER_IGNORE, false);
	} else if (seek_pos >= 0) {
		float ret = blend_input(0, seek_pos, true, 1.0, FILTER_IGNORE, false);
		set_parameter(this->seek_pos, -1.0); //reset
		_change_notify("seek_pos");
		return ret;
	} else {
		return blend_input(0, p_time, false, 1.0, FILTER_IGNORE, false);
	}
}

void AnimationNodeTimeSeek::_bind_methods() {
}

AnimationNodeTimeSeek::AnimationNodeTimeSeek() {
	add_input("in");
	seek_pos = PNAME("seek_position");
}

/////////////////////////////////////////////////

void AnimationNodeTransition::get_parameter_list(List<PropertyInfo> *r_list) const {
	String anims;
	for (int i = 0; i < enabled_inputs; i++) {
		if (i > 0) {
			anims += ",";
		}
		anims += inputs[i].name;
	}

	r_list->push_back(PropertyInfo(Variant::INT, current, PROPERTY_HINT_ENUM, anims));
	r_list->push_back(PropertyInfo(Variant::INT, prev_current, PROPERTY_HINT_NONE, "", 0));
	r_list->push_back(PropertyInfo(Variant::INT, prev, PROPERTY_HINT_NONE, "", 0));
	r_list->push_back(PropertyInfo(Variant::REAL, time, PROPERTY_HINT_NONE, "", 0));
	r_list->push_back(PropertyInfo(Variant::REAL, prev_xfading, PROPERTY_HINT_NONE, "", 0));
}
Variant AnimationNodeTransition::get_parameter_default_value(const StringName &p_parameter) const {
	if (p_parameter == time || p_parameter == prev_xfading) {
		return 0.0;
	} else if (p_parameter == prev || p_parameter == prev_current) {
		return -1;
	} else {
		return 0;
	}
}

String AnimationNodeTransition::get_caption() const {
	return "Transition";
}

void AnimationNodeTransition::_update_inputs() {
	while (get_input_count() < enabled_inputs) {
		add_input(inputs[get_input_count()].name);
	}

	while (get_input_count() > enabled_inputs) {
		remove_input(get_input_count() - 1);
	}
}

void AnimationNodeTransition::set_enabled_inputs(int p_inputs) {
	ERR_FAIL_INDEX(p_inputs, MAX_INPUTS);
	enabled_inputs = p_inputs;
	_update_inputs();
}

int AnimationNodeTransition::get_enabled_inputs() {
	return enabled_inputs;
}

void AnimationNodeTransition::set_input_as_auto_advance(int p_input, bool p_enable) {
	ERR_FAIL_INDEX(p_input, MAX_INPUTS);
	inputs[p_input].auto_advance = p_enable;
}

bool AnimationNodeTransition::is_input_set_as_auto_advance(int p_input) const {
	ERR_FAIL_INDEX_V(p_input, MAX_INPUTS, false);
	return inputs[p_input].auto_advance;
}

void AnimationNodeTransition::set_input_caption(int p_input, const String &p_name) {
	ERR_FAIL_INDEX(p_input, MAX_INPUTS);
	inputs[p_input].name = p_name;
	set_input_name(p_input, p_name);
}

String AnimationNodeTransition::get_input_caption(int p_input) const {
	ERR_FAIL_INDEX_V(p_input, MAX_INPUTS, String());
	return inputs[p_input].name;
}

void AnimationNodeTransition::set_cross_fade_time(float p_fade) {
	xfade = p_fade;
}

float AnimationNodeTransition::get_cross_fade_time() const {
	return xfade;
}

float AnimationNodeTransition::process(float p_time, bool p_seek) {
	int current = get_parameter(this->current);
	int prev = get_parameter(this->prev);
	int prev_current = get_parameter(this->prev_current);

	float time = get_parameter(this->time);
	float prev_xfading = get_parameter(this->prev_xfading);

	bool switched = current != prev_current;

	if (switched) {
		set_parameter(this->prev_current, current);
		set_parameter(this->prev, prev_current);

		prev = prev_current;
		prev_xfading = xfade;
		time = 0;
		switched = true;
	}

	if (current < 0 || current >= enabled_inputs || prev >= enabled_inputs) {
		return 0;
	}

	float rem = 0;

	if (prev < 0) { // process current animation, check for transition

		rem = blend_input(current, p_time, p_seek, 1.0, FILTER_IGNORE, false);

		if (p_seek) {
			time = p_time;
		} else {
			time += p_time;
		}

		if (inputs[current].auto_advance && rem <= xfade) {
			set_parameter(this->current, (current + 1) % enabled_inputs);
		}

	} else { // cross-fading from prev to current

		float blend = xfade == 0 ? 0 : (prev_xfading / xfade);

		if (!p_seek && switched) { //just switched, seek to start of current

			rem = blend_input(current, 0, true, 1.0 - blend, FILTER_IGNORE, false);
		} else {
			rem = blend_input(current, p_time, p_seek, 1.0 - blend, FILTER_IGNORE, false);
		}

		if (p_seek) { // don't seek prev animation
			blend_input(prev, 0, false, blend, FILTER_IGNORE, false);
			time = p_time;
		} else {
			blend_input(prev, p_time, false, blend, FILTER_IGNORE, false);
			time += p_time;
			prev_xfading -= p_time;
			if (prev_xfading < 0) {
				set_parameter(this->prev, -1);
			}
		}
	}

	set_parameter(this->time, time);
	set_parameter(this->prev_xfading, prev_xfading);

	return rem;
}

void AnimationNodeTransition::_validate_property(PropertyInfo &property) const {
	if (property.name.begins_with("input_")) {
		String n = property.name.get_slicec('/', 0).get_slicec('_', 1);
		if (n != "count") {
			int idx = n.to_int();
			if (idx >= enabled_inputs) {
				property.usage = 0;
			}
		}
	}

	AnimationNode::_validate_property(property);
}

void AnimationNodeTransition::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_enabled_inputs", "amount"), &AnimationNodeTransition::set_enabled_inputs);
	ClassDB::bind_method(D_METHOD("get_enabled_inputs"), &AnimationNodeTransition::get_enabled_inputs);

	ClassDB::bind_method(D_METHOD("set_input_as_auto_advance", "input", "enable"), &AnimationNodeTransition::set_input_as_auto_advance);
	ClassDB::bind_method(D_METHOD("is_input_set_as_auto_advance", "input"), &AnimationNodeTransition::is_input_set_as_auto_advance);

	ClassDB::bind_method(D_METHOD("set_input_caption", "input", "caption"), &AnimationNodeTransition::set_input_caption);
	ClassDB::bind_method(D_METHOD("get_input_caption", "input"), &AnimationNodeTransition::get_input_caption);

	ClassDB::bind_method(D_METHOD("set_cross_fade_time", "time"), &AnimationNodeTransition::set_cross_fade_time);
	ClassDB::bind_method(D_METHOD("get_cross_fade_time"), &AnimationNodeTransition::get_cross_fade_time);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "input_count", PROPERTY_HINT_RANGE, "0,64,1", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_UPDATE_ALL_IF_MODIFIED), "set_enabled_inputs", "get_enabled_inputs");
	ADD_PROPERTY(PropertyInfo(Variant::REAL, "xfade_time", PROPERTY_HINT_RANGE, "0,120,0.01"), "set_cross_fade_time", "get_cross_fade_time");

	for (int i = 0; i < MAX_INPUTS; i++) {
		ADD_PROPERTYI(PropertyInfo(Variant::STRING, "input_" + itos(i) + "/name", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_INTERNAL), "set_input_caption", "get_input_caption", i);
		ADD_PROPERTYI(PropertyInfo(Variant::BOOL, "input_" + itos(i) + "/auto_advance", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_INTERNAL), "set_input_as_auto_advance", "is_input_set_as_auto_advance", i);
	}
}

AnimationNodeTransition::AnimationNodeTransition() {
	prev_xfading = "prev_xfading";
	prev = "prev";
	time = "time";
	current = PNAME("current");
	prev_current = "prev_current";
	xfade = 0.0;

	enabled_inputs = 0;
	for (int i = 0; i < MAX_INPUTS; i++) {
		inputs[i].auto_advance = false;
		inputs[i].name = "state " + itos(i);
	}
}

/////////////////////

String AnimationNodeOutput::get_caption() const {
	return "Output";
}

float AnimationNodeOutput::process(float p_time, bool p_seek) {
	return blend_input(0, p_time, p_seek, 1.0);
}

AnimationNodeOutput::AnimationNodeOutput() {
	add_input("output");
}

///////////////////////////////////////////////////////
void AnimationNodeBlendTree::add_node(const StringName &p_name, Ref<AnimationNode> p_node, const Vector2 &p_position) {
	ERR_FAIL_COND(nodes.has(p_name));
	ERR_FAIL_COND(p_node.is_null());
	ERR_FAIL_COND(p_name == SceneStringNames::get_singleton()->output);
	ERR_FAIL_COND(String(p_name).find("/") != -1);

	Node n;
	n.node = p_node;
	n.position = p_position;
	n.connections.resize(n.node->get_input_count());
	nodes[p_name] = n;

	emit_changed();
	emit_signal("tree_changed");

	p_node->connect("tree_changed", this, "_tree_changed", varray(), CONNECT_REFERENCE_COUNTED);
	p_node->connect("changed", this, "_node_changed", varray(p_name), CONNECT_REFERENCE_COUNTED);
}

Ref<AnimationNode> AnimationNodeBlendTree::get_node(const StringName &p_name) const {
	ERR_FAIL_COND_V(!nodes.has(p_name), Ref<AnimationNode>());

	return nodes[p_name].node;
}

StringName AnimationNodeBlendTree::get_node_name(const Ref<AnimationNode> &p_node) const {
	for (NodeMap::Element *E = nodes.front(); E; E = E->next()) {
		if (E->get().node == p_node) {
			return E->key();
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

	for (NodeMap::Element *E = nodes.front(); E; E = E->next()) {
		ns.push_back(E->key());
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
	ERR_FAIL_COND(p_name == SceneStringNames::get_singleton()->output); //can't delete output

	{
		Ref<AnimationNode> node = nodes[p_name].node;
		node->disconnect("tree_changed", this, "_tree_changed");
		node->disconnect("changed", this, "_node_changed");
	}

	nodes.erase(p_name);

	//erase connections to name
	for (NodeMap::Element *E = nodes.front(); E; E = E->next()) {
		for (int i = 0; i < E->get().connections.size(); i++) {
			if (E->get().connections[i] == p_name) {
				E->get().connections.write[i] = StringName();
			}
		}
	}

	emit_changed();
	emit_signal("tree_changed");
}

void AnimationNodeBlendTree::rename_node(const StringName &p_name, const StringName &p_new_name) {
	ERR_FAIL_COND(!nodes.has(p_name));
	ERR_FAIL_COND(nodes.has(p_new_name));
	ERR_FAIL_COND(p_name == SceneStringNames::get_singleton()->output);
	ERR_FAIL_COND(p_new_name == SceneStringNames::get_singleton()->output);

	nodes[p_name].node->disconnect("changed", this, "_node_changed");

	nodes[p_new_name] = nodes[p_name];
	nodes.erase(p_name);

	//rename connections
	for (NodeMap::Element *E = nodes.front(); E; E = E->next()) {
		for (int i = 0; i < E->get().connections.size(); i++) {
			if (E->get().connections[i] == p_name) {
				E->get().connections.write[i] = p_new_name;
			}
		}
	}
	//connection must be done with new name
	nodes[p_new_name].node->connect("changed", this, "_node_changed", varray(p_new_name), CONNECT_REFERENCE_COUNTED);

	emit_signal("tree_changed");
}

void AnimationNodeBlendTree::connect_node(const StringName &p_input_node, int p_input_index, const StringName &p_output_node) {
	ERR_FAIL_COND(!nodes.has(p_output_node));
	ERR_FAIL_COND(!nodes.has(p_input_node));
	ERR_FAIL_COND(p_output_node == SceneStringNames::get_singleton()->output);
	ERR_FAIL_COND(p_input_node == p_output_node);

	Ref<AnimationNode> input = nodes[p_input_node].node;
	ERR_FAIL_INDEX(p_input_index, nodes[p_input_node].connections.size());

	for (NodeMap::Element *E = nodes.front(); E; E = E->next()) {
		for (int i = 0; i < E->get().connections.size(); i++) {
			StringName output = E->get().connections[i];
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
	if (!nodes.has(p_output_node) || p_output_node == SceneStringNames::get_singleton()->output) {
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

	for (NodeMap::Element *E = nodes.front(); E; E = E->next()) {
		for (int i = 0; i < E->get().connections.size(); i++) {
			StringName output = E->get().connections[i];
			if (output == p_output_node) {
				return CONNECTION_ERROR_CONNECTION_EXISTS;
			}
		}
	}
	return CONNECTION_OK;
}

void AnimationNodeBlendTree::get_node_connections(List<NodeConnection> *r_connections) const {
	for (NodeMap::Element *E = nodes.front(); E; E = E->next()) {
		for (int i = 0; i < E->get().connections.size(); i++) {
			StringName output = E->get().connections[i];
			if (output != StringName()) {
				NodeConnection nc;
				nc.input_node = E->key();
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

float AnimationNodeBlendTree::process(float p_time, bool p_seek) {
	Ref<AnimationNodeOutput> output = nodes[SceneStringNames::get_singleton()->output].node;
	return _blend_node("output", nodes[SceneStringNames::get_singleton()->output].connections, this, output, p_time, p_seek, 1.0);
}

void AnimationNodeBlendTree::get_node_list(List<StringName> *r_list) {
	for (NodeMap::Element *E = nodes.front(); E; E = E->next()) {
		r_list->push_back(E->key());
	}
}

void AnimationNodeBlendTree::set_graph_offset(const Vector2 &p_graph_offset) {
	graph_offset = p_graph_offset;
}

Vector2 AnimationNodeBlendTree::get_graph_offset() const {
	return graph_offset;
}

Ref<AnimationNode> AnimationNodeBlendTree::get_child_by_name(const StringName &p_name) {
	return get_node(p_name);
}

bool AnimationNodeBlendTree::_set(const StringName &p_name, const Variant &p_value) {
	String name = p_name;
	if (name.begins_with("nodes/")) {
		String node_name = name.get_slicec('/', 1);
		String what = name.get_slicec('/', 2);

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
	} else if (name == "node_connections") {
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
	String name = p_name;
	if (name.begins_with("nodes/")) {
		String node_name = name.get_slicec('/', 1);
		String what = name.get_slicec('/', 2);

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
	} else if (name == "node_connections") {
		List<NodeConnection> nc;
		get_node_connections(&nc);
		Array conns;
		conns.resize(nc.size() * 3);

		int idx = 0;
		for (List<NodeConnection>::Element *E = nc.front(); E; E = E->next()) {
			conns[idx * 3 + 0] = E->get().input_node;
			conns[idx * 3 + 1] = E->get().input_index;
			conns[idx * 3 + 2] = E->get().output_node;
			idx++;
		}

		r_ret = conns;
		return true;
	}

	return false;
}
void AnimationNodeBlendTree::_get_property_list(List<PropertyInfo> *p_list) const {
	List<StringName> names;
	for (NodeMap::Element *E = nodes.front(); E; E = E->next()) {
		names.push_back(E->key());
	}

	for (List<StringName>::Element *E = names.front(); E; E = E->next()) {
		String name = E->get();
		if (name != "output") {
			p_list->push_back(PropertyInfo(Variant::OBJECT, "nodes/" + name + "/node", PROPERTY_HINT_RESOURCE_TYPE, "AnimationNode", PROPERTY_USAGE_NOEDITOR));
		}
		p_list->push_back(PropertyInfo(Variant::VECTOR2, "nodes/" + name + "/position", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NOEDITOR));
	}

	p_list->push_back(PropertyInfo(Variant::ARRAY, "node_connections", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NOEDITOR));
}

void AnimationNodeBlendTree::_tree_changed() {
	emit_signal("tree_changed");
}

void AnimationNodeBlendTree::_node_changed(const StringName &p_node) {
	ERR_FAIL_COND(!nodes.has(p_node));
	nodes[p_node].connections.resize(nodes[p_node].node->get_input_count());
}

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

	ClassDB::bind_method(D_METHOD("_tree_changed"), &AnimationNodeBlendTree::_tree_changed);
	ClassDB::bind_method(D_METHOD("_node_changed", "node"), &AnimationNodeBlendTree::_node_changed);

	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "graph_offset", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NOEDITOR), "set_graph_offset", "get_graph_offset");

	BIND_CONSTANT(CONNECTION_OK);
	BIND_CONSTANT(CONNECTION_ERROR_NO_INPUT);
	BIND_CONSTANT(CONNECTION_ERROR_NO_INPUT_INDEX);
	BIND_CONSTANT(CONNECTION_ERROR_NO_OUTPUT);
	BIND_CONSTANT(CONNECTION_ERROR_SAME_NODE);
	BIND_CONSTANT(CONNECTION_ERROR_CONNECTION_EXISTS);
}

AnimationNodeBlendTree::AnimationNodeBlendTree() {
	Ref<AnimationNodeOutput> output;
	output.instance();
	Node n;
	n.node = output;
	n.position = Vector2(300, 150);
	n.connections.resize(1);
	nodes["output"] = n;
}

AnimationNodeBlendTree::~AnimationNodeBlendTree() {
}

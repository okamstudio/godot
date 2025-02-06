/**************************************************************************/
/*  audio_effect_reverb.cpp                                               */
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

#include "audio_effect_reverb.h"
#include "servers/audio_server.h"
void AudioEffectReverbInstance::process(const AudioFrame *p_src_frames, AudioFrame *p_dst_frames, int p_frame_count) {
	for (int i = 0; i < 2; i++) {
		Reverb &r = reverb[i];

		r.set_predelay(base->predelay);
		r.set_predelay_feedback(base->predelay_fb);
		r.set_highpass(base->hpf);
		r.set_room_size(base->room_size);
		r.set_reflection(base->reflectivity);
		r.set_extra_spread(base->spread);
		r.set_wet(base->wet);
		r.set_dry(base->dry);
	}

	int todo = p_frame_count;
	int offset = 0;

	while (todo) {
		int to_mix = MIN(todo, Reverb::INPUT_BUFFER_MAX_SIZE);

		for (int j = 0; j < to_mix; j++) {
			tmp_src[j] = p_src_frames[offset + j].left;
		}

		reverb[0].process(tmp_src, tmp_dst, to_mix);

		for (int j = 0; j < to_mix; j++) {
			p_dst_frames[offset + j].left = tmp_dst[j];
			tmp_src[j] = p_src_frames[offset + j].right;
		}

		reverb[1].process(tmp_src, tmp_dst, to_mix);

		for (int j = 0; j < to_mix; j++) {
			p_dst_frames[offset + j].right = tmp_dst[j];
		}

		offset += to_mix;
		todo -= to_mix;
	}
}

AudioEffectReverbInstance::AudioEffectReverbInstance() {
	reverb[0].set_mix_rate(AudioServer::get_singleton()->get_mix_rate());
	reverb[0].set_extra_spread_base(0);
	reverb[1].set_mix_rate(AudioServer::get_singleton()->get_mix_rate());
	reverb[1].set_extra_spread_base(0.000521); //for stereo effect
}

Ref<AudioEffectInstance> AudioEffectReverb::instantiate() {
	Ref<AudioEffectReverbInstance> ins;
	ins.instantiate();
	ins->base = Ref<AudioEffectReverb>(this);
	return ins;
}

void AudioEffectReverb::set_predelay_msec(float p_msec) {
	predelay = p_msec;
}

void AudioEffectReverb::set_predelay_feedback(float p_feedback) {
	predelay_fb = CLAMP(p_feedback, 0, 0.98);
}

void AudioEffectReverb::set_room_size(float p_size) {
	room_size = p_size;
}

void AudioEffectReverb::set_reflectivity(float p_reflectivity) {
	reflectivity = p_reflectivity;
}

#ifndef DISABLE_DEPRECATED
void AudioEffectReverb::set_damping(float p_damping) {
	WARN_DEPRECATED_MSG("Use set_reflectivity instead.");
	set_reflectivity(p_damping);
}
#endif

void AudioEffectReverb::set_spread(float p_spread) {
	spread = p_spread;
}

void AudioEffectReverb::set_dry(float p_dry) {
	dry = p_dry;
}

void AudioEffectReverb::set_wet(float p_wet) {
	wet = p_wet;
}

void AudioEffectReverb::set_hpf(float p_hpf) {
	hpf = p_hpf;
}

float AudioEffectReverb::get_predelay_msec() const {
	return predelay;
}

float AudioEffectReverb::get_predelay_feedback() const {
	return predelay_fb;
}

float AudioEffectReverb::get_room_size() const {
	return room_size;
}

float AudioEffectReverb::get_reflectivity() const {
	return reflectivity;
}

#ifndef DISABLE_DEPRECATED
float AudioEffectReverb::get_damping() const {
	WARN_DEPRECATED_MSG("Use get_reflectivity instead.");
	return get_reflectivity();
}
#endif

float AudioEffectReverb::get_spread() const {
	return spread;
}

float AudioEffectReverb::get_dry() const {
	return dry;
}

float AudioEffectReverb::get_wet() const {
	return wet;
}

float AudioEffectReverb::get_hpf() const {
	return hpf;
}

void AudioEffectReverb::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_predelay_msec", "msec"), &AudioEffectReverb::set_predelay_msec);
	ClassDB::bind_method(D_METHOD("get_predelay_msec"), &AudioEffectReverb::get_predelay_msec);

	ClassDB::bind_method(D_METHOD("set_predelay_feedback", "feedback"), &AudioEffectReverb::set_predelay_feedback);
	ClassDB::bind_method(D_METHOD("get_predelay_feedback"), &AudioEffectReverb::get_predelay_feedback);

	ClassDB::bind_method(D_METHOD("set_room_size", "size"), &AudioEffectReverb::set_room_size);
	ClassDB::bind_method(D_METHOD("get_room_size"), &AudioEffectReverb::get_room_size);

	ClassDB::bind_method(D_METHOD("set_reflectivity", "amount"), &AudioEffectReverb::set_reflectivity);
	ClassDB::bind_method(D_METHOD("get_reflectivity"), &AudioEffectReverb::get_reflectivity);

#ifndef DISABLE_DEPRECATED
	ClassDB::bind_method(D_METHOD("set_damping", "amount"), &AudioEffectReverb::set_damping);
	ClassDB::bind_method(D_METHOD("get_damping"), &AudioEffectReverb::get_damping);
#endif

	ClassDB::bind_method(D_METHOD("set_spread", "amount"), &AudioEffectReverb::set_spread);
	ClassDB::bind_method(D_METHOD("get_spread"), &AudioEffectReverb::get_spread);

	ClassDB::bind_method(D_METHOD("set_dry", "amount"), &AudioEffectReverb::set_dry);
	ClassDB::bind_method(D_METHOD("get_dry"), &AudioEffectReverb::get_dry);

	ClassDB::bind_method(D_METHOD("set_wet", "amount"), &AudioEffectReverb::set_wet);
	ClassDB::bind_method(D_METHOD("get_wet"), &AudioEffectReverb::get_wet);

	ClassDB::bind_method(D_METHOD("set_hpf", "amount"), &AudioEffectReverb::set_hpf);
	ClassDB::bind_method(D_METHOD("get_hpf"), &AudioEffectReverb::get_hpf);

	ADD_GROUP("Predelay", "predelay_");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "predelay_msec", PROPERTY_HINT_RANGE, "20,500,1,suffix:ms"), "set_predelay_msec", "get_predelay_msec");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "predelay_feedback", PROPERTY_HINT_RANGE, "0,0.98,0.01"), "set_predelay_feedback", "get_predelay_feedback");
	ADD_GROUP("", "");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "room_size", PROPERTY_HINT_RANGE, "0,1,0.01"), "set_room_size", "get_room_size");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "reflectivity", PROPERTY_HINT_RANGE, "0,1,0.01"), "set_reflectivity", "get_reflectivity");
#ifndef DISABLE_DEPRECATED
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "damping", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NO_EDITOR), "set_damping", "get_damping");
#endif
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "spread", PROPERTY_HINT_RANGE, "0,1,0.01"), "set_spread", "get_spread");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "hipass", PROPERTY_HINT_RANGE, "0,1,0.01"), "set_hpf", "get_hpf");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "dry", PROPERTY_HINT_RANGE, "0,1,0.01"), "set_dry", "get_dry");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "wet", PROPERTY_HINT_RANGE, "0,1,0.01"), "set_wet", "get_wet");
}

AudioEffectReverb::AudioEffectReverb() {
	predelay = 150;
	predelay_fb = 0.4;
	hpf = 0;
	room_size = 0.8;
	reflectivity = 0.5;
	spread = 1.0;
	dry = 1.0;
	wet = 0.5;
}

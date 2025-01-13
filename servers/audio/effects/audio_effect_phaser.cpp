/**************************************************************************/
/*  audio_effect_phaser.cpp                                               */
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

#include "audio_effect_phaser.h"
#include "servers/audio_server.h"

void AudioEffectPhaserInstance::process(const AudioFrame *p_src_frames, AudioFrame *p_dst_frames, int p_frame_count) {
	float sampling_rate = AudioServer::get_singleton()->get_mix_rate();

	float dmin = base->range_min / (sampling_rate / 2.0);
	float dmax = base->range_max / (sampling_rate / 2.0);

	float increment = Math_TAU * (base->rate / sampling_rate);

	for (int i = 0; i < p_frame_count; i++) {
		phase += increment;

		while (phase >= Math_TAU) {
			phase -= Math_TAU;
		}

		float d = dmin + (dmax - dmin) * ((sin(phase) + 1.f) / 2.f);

		//update filter coeffs
		for (int j = 0; j < 6; j++) {
			allpass[0][j].delay(d);
			allpass[1][j].delay(d);
		}

		//calculate output
		float y = allpass[0][0].update(
				allpass[0][1].update(
						allpass[0][2].update(
								allpass[0][3].update(
										allpass[0][4].update(
												allpass[0][5].update(p_src_frames[i].left + h.left * base->feedback))))));
		h.left = y;

		p_dst_frames[i].left = p_src_frames[i].left + y * base->depth;

		y = allpass[1][0].update(
				allpass[1][1].update(
						allpass[1][2].update(
								allpass[1][3].update(
										allpass[1][4].update(
												allpass[1][5].update(p_src_frames[i].right + h.right * base->feedback))))));
		h.right = y;

		p_dst_frames[i].right = p_src_frames[i].right + y * base->depth;
	}
}

Ref<AudioEffectInstance> AudioEffectPhaser::instantiate() {
	Ref<AudioEffectPhaserInstance> ins;
	ins.instantiate();
	ins->base = Ref<AudioEffectPhaser>(this);
	ins->phase = 0;
	ins->h = AudioFrame(0, 0);

	return ins;
}

void AudioEffectPhaser::set_range_min_hz(float p_hz) {
	range_min = p_hz;
}

float AudioEffectPhaser::get_range_min_hz() const {
	return range_min;
}

void AudioEffectPhaser::set_range_max_hz(float p_hz) {
	range_max = p_hz;
}

float AudioEffectPhaser::get_range_max_hz() const {
	return range_max;
}

void AudioEffectPhaser::set_rate_hz(float p_hz) {
	rate = p_hz;
}

float AudioEffectPhaser::get_rate_hz() const {
	return rate;
}

void AudioEffectPhaser::set_feedback(float p_fbk) {
	feedback = p_fbk;
}

float AudioEffectPhaser::get_feedback() const {
	return feedback;
}

void AudioEffectPhaser::set_depth(float p_depth) {
	depth = p_depth;
}

float AudioEffectPhaser::get_depth() const {
	return depth;
}

void AudioEffectPhaser::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_range_min_hz", "hz"), &AudioEffectPhaser::set_range_min_hz);
	ClassDB::bind_method(D_METHOD("get_range_min_hz"), &AudioEffectPhaser::get_range_min_hz);

	ClassDB::bind_method(D_METHOD("set_range_max_hz", "hz"), &AudioEffectPhaser::set_range_max_hz);
	ClassDB::bind_method(D_METHOD("get_range_max_hz"), &AudioEffectPhaser::get_range_max_hz);

	ClassDB::bind_method(D_METHOD("set_rate_hz", "hz"), &AudioEffectPhaser::set_rate_hz);
	ClassDB::bind_method(D_METHOD("get_rate_hz"), &AudioEffectPhaser::get_rate_hz);

	ClassDB::bind_method(D_METHOD("set_feedback", "fbk"), &AudioEffectPhaser::set_feedback);
	ClassDB::bind_method(D_METHOD("get_feedback"), &AudioEffectPhaser::get_feedback);

	ClassDB::bind_method(D_METHOD("set_depth", "depth"), &AudioEffectPhaser::set_depth);
	ClassDB::bind_method(D_METHOD("get_depth"), &AudioEffectPhaser::get_depth);

	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "range_min_hz", PROPERTY_HINT_RANGE, "10,10000,suffix:Hz"), "set_range_min_hz", "get_range_min_hz");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "range_max_hz", PROPERTY_HINT_RANGE, "10,10000,suffix:Hz"), "set_range_max_hz", "get_range_max_hz");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "rate_hz", PROPERTY_HINT_RANGE, "0.01,20,suffix:Hz"), "set_rate_hz", "get_rate_hz");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "feedback", PROPERTY_HINT_RANGE, "0.1,0.9,0.1"), "set_feedback", "get_feedback");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "depth", PROPERTY_HINT_RANGE, "0.1,4,0.1"), "set_depth", "get_depth");
}

AudioEffectPhaser::AudioEffectPhaser() {
	range_min = 440;
	range_max = 1600;
	rate = 0.5;
	feedback = 0.7;
	depth = 1;
}

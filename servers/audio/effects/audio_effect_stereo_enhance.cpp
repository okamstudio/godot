/**************************************************************************/
/*  audio_effect_stereo_enhance.cpp                                       */
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

#include "audio_effect_stereo_enhance.h"

#include "servers/audio_server.h"

void AudioEffectStereoEnhanceInstance::process(const AudioFrame *p_src_frames, AudioFrame *p_dst_frames, int p_frame_count) {
	float intensity = base->pan_pullout;
	bool surround_mode = base->surround > 0;
	float surround_amount = base->surround;
	unsigned int delay_frames = (base->time_pullout / 1000.0) * AudioServer::get_singleton()->get_mix_rate();

	for (int i = 0; i < p_frame_count; i++) {
		float left = p_src_frames[i].left;
		float right = p_src_frames[i].right;

		float center = (left + right) / 2.0f;

		left = (center + (left - center) * intensity);
		right = (center + (right - center) * intensity);

		if (surround_mode) {
			float val = (left + right) / 2.0;

			delay_ringbuff[ringbuff_pos & ringbuff_mask] = val;

			float out = delay_ringbuff[(ringbuff_pos - delay_frames) & ringbuff_mask] * surround_amount;

			left += out;
			right += -out;
		} else {
			float val = right;

			delay_ringbuff[ringbuff_pos & ringbuff_mask] = val;

			// The right channel is delayed.
			right = delay_ringbuff[(ringbuff_pos - delay_frames) & ringbuff_mask];
		}

		p_dst_frames[i].left = left;
		p_dst_frames[i].right = right;
		ringbuff_pos++;
	}
}

AudioEffectStereoEnhanceInstance::~AudioEffectStereoEnhanceInstance() {
	memdelete_arr(delay_ringbuff);
}

Ref<AudioEffectInstance> AudioEffectStereoEnhance::instantiate() {
	Ref<AudioEffectStereoEnhanceInstance> ins;
	ins.instantiate();

	ins->base = Ref<AudioEffectStereoEnhance>(this);

	float ring_buffer_max_size = AudioEffectStereoEnhanceInstance::MAX_DELAY_MS + 2;
	ring_buffer_max_size /= 1000.0; //convert to seconds
	ring_buffer_max_size *= AudioServer::get_singleton()->get_mix_rate();

	int ringbuff_size = (int)ring_buffer_max_size;

	int bits = 0;

	while (ringbuff_size > 0) {
		bits++;
		ringbuff_size /= 2;
	}

	ringbuff_size = 1 << bits;
	ins->ringbuff_mask = ringbuff_size - 1;
	ins->ringbuff_pos = 0;

	ins->delay_ringbuff = memnew_arr(float, ringbuff_size);

	return ins;
}

void AudioEffectStereoEnhance::set_pan_pullout(float p_amount) {
	pan_pullout = p_amount;
}

float AudioEffectStereoEnhance::get_pan_pullout() const {
	return pan_pullout;
}

void AudioEffectStereoEnhance::set_time_pullout(float p_amount) {
	time_pullout = p_amount;
}

float AudioEffectStereoEnhance::get_time_pullout() const {
	return time_pullout;
}

void AudioEffectStereoEnhance::set_surround(float p_amount) {
	surround = p_amount;
}

float AudioEffectStereoEnhance::get_surround() const {
	return surround;
}

void AudioEffectStereoEnhance::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_pan_pullout", "amount"), &AudioEffectStereoEnhance::set_pan_pullout);
	ClassDB::bind_method(D_METHOD("get_pan_pullout"), &AudioEffectStereoEnhance::get_pan_pullout);

	ClassDB::bind_method(D_METHOD("set_time_pullout", "amount"), &AudioEffectStereoEnhance::set_time_pullout);
	ClassDB::bind_method(D_METHOD("get_time_pullout"), &AudioEffectStereoEnhance::get_time_pullout);

	ClassDB::bind_method(D_METHOD("set_surround", "amount"), &AudioEffectStereoEnhance::set_surround);
	ClassDB::bind_method(D_METHOD("get_surround"), &AudioEffectStereoEnhance::get_surround);

	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "pan_pullout", PROPERTY_HINT_RANGE, "0,4,0.01"), "set_pan_pullout", "get_pan_pullout");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "time_pullout_ms", PROPERTY_HINT_RANGE, "0,50,0.01,suffix:ms"), "set_time_pullout", "get_time_pullout");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "surround", PROPERTY_HINT_RANGE, "0,1,0.01"), "set_surround", "get_surround");
}

AudioEffectStereoEnhance::AudioEffectStereoEnhance() {}

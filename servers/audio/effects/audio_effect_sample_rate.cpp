/*************************************************************************/
/*  audio_effect_sample_rate.cpp                                         */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                      https://godotengine.org                          */
/*************************************************************************/
/* Copyright (c) 2007-2022 Juan Linietsky, Ariel Manzur.                 */
/* Copyright (c) 2014-2022 Godot Engine contributors (cf. AUTHORS.md).   */
/*                                                                       */
/* Permission is hereby granted, free of charge, to any person obtaining */
/* a copy of this software and associated documentation files (the       */
/* "Software"), to deal in the Software without restriction, including   */
/* without limitation the rights to use, copy, modify, merge, publish,   */
/* distribute, sublicense, and/or sell copies of the Software, and to    */
/* permit persons to whom the Software is furnished to do so, subject to */
/* the following conditions:                                             */
/*                                                                       */
/* The above copyright notice and this permission notice shall be        */
/* included in all copies or substantial portions of the Software.       */
/*                                                                       */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,       */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF    */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.*/
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY  */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,  */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE     */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                */
/*************************************************************************/

#include "audio_effect_sample_rate.h"
#include "core/math/math_funcs.h"
#include "servers/audio_server.h"

void AudioEffectSampleRateInstance::process(const AudioFrame *p_src_frames, AudioFrame *p_dst_frames, int p_frame_count) {
	float frames_until_next_sample = AudioServer::get_singleton()->get_mix_rate() / base->rate;

	for (int i = 0; i < p_frame_count; i++) {
		if (processed_frames >= frames_until_next_sample) {
			last_sampled_frame = p_src_frames[i]; // sample a new frame
			processed_frames = Math::fmod(processed_frames - frames_until_next_sample, 1.0f);
		}

		// output dry/wet signal based on the `mix` control
		p_dst_frames[i] = last_sampled_frame * base->mix + p_src_frames[i] * (1.0f - base->mix);

		processed_frames++;
	}
}

Ref<AudioEffectInstance> AudioEffectSampleRate::instantiate() {
	Ref<AudioEffectSampleRateInstance> ins;
	ins.instantiate();
	ins->base = Ref<AudioEffectSampleRate>(this);
	return ins;
}

void AudioEffectSampleRate::set_rate(float p_rate) {
	rate = MAX(p_rate, 1.0f);
}
float AudioEffectSampleRate::get_rate() const {
	return rate;
}

void AudioEffectSampleRate::set_mix(float p_mix) {
	mix = CLAMP(p_mix, 0.0f, 1.0f);
}
float AudioEffectSampleRate::get_mix() const {
	return mix;
}

void AudioEffectSampleRate::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_rate", "rate"), &AudioEffectSampleRate::set_rate);
	ClassDB::bind_method(D_METHOD("get_rate"), &AudioEffectSampleRate::get_rate);

	ClassDB::bind_method(D_METHOD("set_mix", "mix"), &AudioEffectSampleRate::set_mix);
	ClassDB::bind_method(D_METHOD("get_mix"), &AudioEffectSampleRate::get_mix);

	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "rate", PROPERTY_HINT_RANGE, "1.0,22050.0,1.0,or_greater,hide_slider,suffix:Hz"), "set_rate", "get_rate");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "mix", PROPERTY_HINT_RANGE, "0.0,1.0,0.01"), "set_mix", "get_mix");
}

AudioEffectSampleRate::AudioEffectSampleRate() {
	rate = 11025.0f;
	mix = 1.0f;
}

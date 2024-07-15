/**************************************************************************/
/*  audio_effect_distortion.cpp                                           */
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

#include "audio_effect_distortion.h"
#include "core/math/math_funcs.h"
#include "servers/audio_server.h"

void AudioEffectDistortionInstance::process(const AudioFrame *p_src_frames, AudioFrame *p_dst_frames, int p_frame_count) {
	const float *src = (const float *)p_src_frames;
	float *dst = (float *)p_dst_frames;

	//float lpf_c=expf(-Math_TAU*keep_hf_hz.get()/(mix_rate*(float)OVERSAMPLE));
	float lpf_c = Math::exp(-Math_TAU * base->keep_hf_hz / (AudioServer::get_singleton()->get_mix_rate()));
	float lpf_ic = 1.0 - lpf_c;

	float drive_f = base->drive;
	float pregain_f = Math::db_to_linear(base->pre_gain);
	float postgain_f = Math::db_to_linear(base->post_gain);

	float atan_mult = Math::pow(10, drive_f * drive_f * 3.0) - 1.0 + 0.001;
	float atan_div = 1.0 / (Math::atan(atan_mult) * (1.0 + drive_f * 8));

	float lofi_mult = Math::pow(2.0, 2.0 + (1.0 - drive_f) * 14); //goes from 16 to 2 bits

	for (int i = 0; i < p_frame_count * 2; i++) {
		float out = undenormalize(src[i] * lpf_ic + lpf_c * h[i & 1]);
		h[i & 1] = out;
		float a = out;
		float ha = src[i] - out; //high freqs
		a *= pregain_f;

		switch (base->mode) {
			case AudioEffectDistortion::MODE_CLIP: {
				float a_sign = a < 0 ? -1.0f : 1.0f;
				a = Math::pow(Math::abs(a), 1.0001f - drive_f) * a_sign;
				if (a > 1.0) {
					a = 1.0;
				} else if (a < (-1.0)) {
					a = -1.0;
				}

			} break;
			case AudioEffectDistortion::MODE_ATAN: {
				a = Math::atan(a * atan_mult) * atan_div;

			} break;
			case AudioEffectDistortion::MODE_LOFI: {
				a = Math::floor(a * lofi_mult + 0.5) / lofi_mult;

			} break;
			case AudioEffectDistortion::MODE_OVERDRIVE: {
				const double x = a * 0.686306;
				const double z = 1 + Math::exp(Math::sqrt(Math::abs(x)) * -0.75);
				a = (Math::exp(x) - Math::exp(-x * z)) / (Math::exp(x) + Math::exp(-x));
			} break;
			case AudioEffectDistortion::MODE_WAVESHAPE: {
				float x = a;
				float k = 2 * drive_f / (1.00001 - drive_f);

				a = (1.0 + k) * x / (1.0 + k * Math::abs(x));

			} break;
		}

		dst[i] = a * postgain_f + ha;
	}
}

Ref<AudioEffectInstance> AudioEffectDistortion::instantiate() {
	Ref<AudioEffectDistortionInstance> ins;
	ins.instantiate();
	ins->base = Ref<AudioEffectDistortion>(this);
	ins->h[0] = 0;
	ins->h[1] = 0;

	return ins;
}

void AudioEffectDistortion::set_mode(Mode p_mode) {
	mode = p_mode;
}

AudioEffectDistortion::Mode AudioEffectDistortion::get_mode() const {
	return mode;
}

void AudioEffectDistortion::set_pre_gain(float p_pre_gain) {
	pre_gain = p_pre_gain;
}

float AudioEffectDistortion::get_pre_gain() const {
	return pre_gain;
}

void AudioEffectDistortion::set_keep_hf_hz(float p_keep_hf_hz) {
	keep_hf_hz = p_keep_hf_hz;
}

float AudioEffectDistortion::get_keep_hf_hz() const {
	return keep_hf_hz;
}

void AudioEffectDistortion::set_drive(float p_drive) {
	drive = p_drive;
}

float AudioEffectDistortion::get_drive() const {
	return drive;
}

void AudioEffectDistortion::set_post_gain(float p_post_gain) {
	post_gain = p_post_gain;
}

float AudioEffectDistortion::get_post_gain() const {
	return post_gain;
}

void AudioEffectDistortion::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_mode", "mode"), &AudioEffectDistortion::set_mode);
	ClassDB::bind_method(D_METHOD("get_mode"), &AudioEffectDistortion::get_mode);

	ClassDB::bind_method(D_METHOD("set_pre_gain", "pre_gain"), &AudioEffectDistortion::set_pre_gain);
	ClassDB::bind_method(D_METHOD("get_pre_gain"), &AudioEffectDistortion::get_pre_gain);

	ClassDB::bind_method(D_METHOD("set_keep_hf_hz", "keep_hf_hz"), &AudioEffectDistortion::set_keep_hf_hz);
	ClassDB::bind_method(D_METHOD("get_keep_hf_hz"), &AudioEffectDistortion::get_keep_hf_hz);

	ClassDB::bind_method(D_METHOD("set_drive", "drive"), &AudioEffectDistortion::set_drive);
	ClassDB::bind_method(D_METHOD("get_drive"), &AudioEffectDistortion::get_drive);

	ClassDB::bind_method(D_METHOD("set_post_gain", "post_gain"), &AudioEffectDistortion::set_post_gain);
	ClassDB::bind_method(D_METHOD("get_post_gain"), &AudioEffectDistortion::get_post_gain);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "mode", PROPERTY_HINT_ENUM, "Clip,ATan,LoFi,Overdrive,Wave Shape"), "set_mode", "get_mode");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "pre_gain", PROPERTY_HINT_RANGE, "-60,60,0.01,suffix:dB"), "set_pre_gain", "get_pre_gain");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "keep_hf_hz", PROPERTY_HINT_RANGE, "1,20500,1,suffix:Hz"), "set_keep_hf_hz", "get_keep_hf_hz");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "drive", PROPERTY_HINT_RANGE, "0,1,0.01"), "set_drive", "get_drive");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "post_gain", PROPERTY_HINT_RANGE, "-80,24,0.01,suffix:dB"), "set_post_gain", "get_post_gain");

	BIND_ENUM_CONSTANT(MODE_CLIP);
	BIND_ENUM_CONSTANT(MODE_ATAN);
	BIND_ENUM_CONSTANT(MODE_LOFI);
	BIND_ENUM_CONSTANT(MODE_OVERDRIVE);
	BIND_ENUM_CONSTANT(MODE_WAVESHAPE);
}

AudioEffectDistortion::AudioEffectDistortion() {
	mode = MODE_CLIP;
	pre_gain = 0;
	post_gain = 0;
	keep_hf_hz = 16000;
	drive = 0;
}

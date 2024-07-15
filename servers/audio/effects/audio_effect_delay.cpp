/**************************************************************************/
/*  audio_effect_delay.cpp                                                */
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

#include "audio_effect_delay.h"

#include "core/math/math_funcs.h"
#include "servers/audio_server.h"

void AudioEffectDelayInstance::process(const AudioFrame *p_src_frames, AudioFrame *p_dst_frames, int p_frame_count) {
	int todo = p_frame_count;

	while (todo) {
		int to_mix = MIN(todo, 256); //can't mix too much

		_process_chunk(p_src_frames, p_dst_frames, to_mix);

		p_src_frames += to_mix;
		p_dst_frames += to_mix;

		todo -= to_mix;
	}
}

void AudioEffectDelayInstance::_process_chunk(const AudioFrame *p_src_frames, AudioFrame *p_dst_frames, int p_frame_count) {
	float main_level_f = base->dry;

	float mix_rate = AudioServer::get_singleton()->get_mix_rate();

	float tap_1_level_f = base->tap_1_active ? Math::db_to_linear(base->tap_1_level) : 0.0;
	int tap_1_delay_frames = int((base->tap_1_delay_ms / 1000.0) * mix_rate);

	float tap_2_level_f = base->tap_2_active ? Math::db_to_linear(base->tap_2_level) : 0.0;
	int tap_2_delay_frames = int((base->tap_2_delay_ms / 1000.0) * mix_rate);

	float feedback_level_f = base->feedback_active ? Math::db_to_linear(base->feedback_level) : 0.0;
	unsigned int feedback_delay_frames = int((base->feedback_delay_ms / 1000.0) * mix_rate);

	AudioFrame tap1_vol = AudioFrame(tap_1_level_f, tap_1_level_f);

	tap1_vol.left *= CLAMP(1.0 - base->tap_1_pan, 0, 1);
	tap1_vol.right *= CLAMP(1.0 + base->tap_1_pan, 0, 1);

	AudioFrame tap2_vol = AudioFrame(tap_2_level_f, tap_2_level_f);

	tap2_vol.left *= CLAMP(1.0 - base->tap_2_pan, 0, 1);
	tap2_vol.right *= CLAMP(1.0 + base->tap_2_pan, 0, 1);

	// feedback lowpass here
	float lpf_c = Math::exp(-Math_TAU * base->feedback_lowpass / mix_rate); // 0 .. 10khz
	float lpf_ic = 1.0 - lpf_c;

	const AudioFrame *src = p_src_frames;
	AudioFrame *dst = p_dst_frames;
	AudioFrame *rb_buf = ring_buffer.ptrw();
	AudioFrame *fb_buf = feedback_buffer.ptrw();

	for (int i = 0; i < p_frame_count; i++) {
		rb_buf[ring_buffer_pos & ring_buffer_mask] = src[i];

		AudioFrame main_val = src[i] * main_level_f;
		AudioFrame tap_1_val = rb_buf[(ring_buffer_pos - tap_1_delay_frames) & ring_buffer_mask] * tap1_vol;
		AudioFrame tap_2_val = rb_buf[(ring_buffer_pos - tap_2_delay_frames) & ring_buffer_mask] * tap2_vol;

		AudioFrame out = main_val + tap_1_val + tap_2_val;

		out += fb_buf[feedback_buffer_pos];

		//apply lowpass and feedback gain
		AudioFrame fb_in = out * feedback_level_f * lpf_ic + h * lpf_c;
		fb_in.undenormalize(); //avoid denormals

		h = fb_in;
		fb_buf[feedback_buffer_pos] = fb_in;

		dst[i] = out;

		ring_buffer_pos++;

		if ((++feedback_buffer_pos) >= feedback_delay_frames) {
			feedback_buffer_pos = 0;
		}
	}
}

Ref<AudioEffectInstance> AudioEffectDelay::instantiate() {
	Ref<AudioEffectDelayInstance> ins;
	ins.instantiate();
	ins->base = Ref<AudioEffectDelay>(this);

	float ring_buffer_max_size = MAX_DELAY_MS + 100; //add 100ms of extra room, just in case
	ring_buffer_max_size /= 1000.0; //convert to seconds
	ring_buffer_max_size *= AudioServer::get_singleton()->get_mix_rate();

	int ringbuff_size = ring_buffer_max_size;

	int bits = 0;

	while (ringbuff_size > 0) {
		bits++;
		ringbuff_size /= 2;
	}

	ringbuff_size = 1 << bits;
	ins->ring_buffer_mask = ringbuff_size - 1;
	ins->ring_buffer_pos = 0;

	ins->ring_buffer.resize(ringbuff_size);
	ins->feedback_buffer.resize(ringbuff_size);

	ins->feedback_buffer_pos = 0;

	ins->h = AudioFrame(0, 0);

	return ins;
}

void AudioEffectDelay::set_dry(float p_dry) {
	dry = p_dry;
}

float AudioEffectDelay::get_dry() {
	return dry;
}

void AudioEffectDelay::set_tap1_active(bool p_active) {
	tap_1_active = p_active;
}

bool AudioEffectDelay::is_tap1_active() const {
	return tap_1_active;
}

void AudioEffectDelay::set_tap1_delay_ms(float p_delay_ms) {
	tap_1_delay_ms = p_delay_ms;
}

float AudioEffectDelay::get_tap1_delay_ms() const {
	return tap_1_delay_ms;
}

void AudioEffectDelay::set_tap1_level_db(float p_level_db) {
	tap_1_level = p_level_db;
}

float AudioEffectDelay::get_tap1_level_db() const {
	return tap_1_level;
}

void AudioEffectDelay::set_tap1_pan(float p_pan) {
	tap_1_pan = p_pan;
}

float AudioEffectDelay::get_tap1_pan() const {
	return tap_1_pan;
}

void AudioEffectDelay::set_tap2_active(bool p_active) {
	tap_2_active = p_active;
}

bool AudioEffectDelay::is_tap2_active() const {
	return tap_2_active;
}

void AudioEffectDelay::set_tap2_delay_ms(float p_delay_ms) {
	tap_2_delay_ms = p_delay_ms;
}

float AudioEffectDelay::get_tap2_delay_ms() const {
	return tap_2_delay_ms;
}

void AudioEffectDelay::set_tap2_level_db(float p_level_db) {
	tap_2_level = p_level_db;
}

float AudioEffectDelay::get_tap2_level_db() const {
	return tap_2_level;
}

void AudioEffectDelay::set_tap2_pan(float p_pan) {
	tap_2_pan = p_pan;
}

float AudioEffectDelay::get_tap2_pan() const {
	return tap_2_pan;
}

void AudioEffectDelay::set_feedback_active(bool p_active) {
	feedback_active = p_active;
}

bool AudioEffectDelay::is_feedback_active() const {
	return feedback_active;
}

void AudioEffectDelay::set_feedback_delay_ms(float p_delay_ms) {
	feedback_delay_ms = p_delay_ms;
}

float AudioEffectDelay::get_feedback_delay_ms() const {
	return feedback_delay_ms;
}

void AudioEffectDelay::set_feedback_level_db(float p_level_db) {
	feedback_level = p_level_db;
}

float AudioEffectDelay::get_feedback_level_db() const {
	return feedback_level;
}

void AudioEffectDelay::set_feedback_lowpass(float p_lowpass) {
	feedback_lowpass = p_lowpass;
}

float AudioEffectDelay::get_feedback_lowpass() const {
	return feedback_lowpass;
}

void AudioEffectDelay::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_dry", "amount"), &AudioEffectDelay::set_dry);
	ClassDB::bind_method(D_METHOD("get_dry"), &AudioEffectDelay::get_dry);

	ClassDB::bind_method(D_METHOD("set_tap1_active", "amount"), &AudioEffectDelay::set_tap1_active);
	ClassDB::bind_method(D_METHOD("is_tap1_active"), &AudioEffectDelay::is_tap1_active);

	ClassDB::bind_method(D_METHOD("set_tap1_delay_ms", "amount"), &AudioEffectDelay::set_tap1_delay_ms);
	ClassDB::bind_method(D_METHOD("get_tap1_delay_ms"), &AudioEffectDelay::get_tap1_delay_ms);

	ClassDB::bind_method(D_METHOD("set_tap1_level_db", "amount"), &AudioEffectDelay::set_tap1_level_db);
	ClassDB::bind_method(D_METHOD("get_tap1_level_db"), &AudioEffectDelay::get_tap1_level_db);

	ClassDB::bind_method(D_METHOD("set_tap1_pan", "amount"), &AudioEffectDelay::set_tap1_pan);
	ClassDB::bind_method(D_METHOD("get_tap1_pan"), &AudioEffectDelay::get_tap1_pan);

	ClassDB::bind_method(D_METHOD("set_tap2_active", "amount"), &AudioEffectDelay::set_tap2_active);
	ClassDB::bind_method(D_METHOD("is_tap2_active"), &AudioEffectDelay::is_tap2_active);

	ClassDB::bind_method(D_METHOD("set_tap2_delay_ms", "amount"), &AudioEffectDelay::set_tap2_delay_ms);
	ClassDB::bind_method(D_METHOD("get_tap2_delay_ms"), &AudioEffectDelay::get_tap2_delay_ms);

	ClassDB::bind_method(D_METHOD("set_tap2_level_db", "amount"), &AudioEffectDelay::set_tap2_level_db);
	ClassDB::bind_method(D_METHOD("get_tap2_level_db"), &AudioEffectDelay::get_tap2_level_db);

	ClassDB::bind_method(D_METHOD("set_tap2_pan", "amount"), &AudioEffectDelay::set_tap2_pan);
	ClassDB::bind_method(D_METHOD("get_tap2_pan"), &AudioEffectDelay::get_tap2_pan);

	ClassDB::bind_method(D_METHOD("set_feedback_active", "amount"), &AudioEffectDelay::set_feedback_active);
	ClassDB::bind_method(D_METHOD("is_feedback_active"), &AudioEffectDelay::is_feedback_active);

	ClassDB::bind_method(D_METHOD("set_feedback_delay_ms", "amount"), &AudioEffectDelay::set_feedback_delay_ms);
	ClassDB::bind_method(D_METHOD("get_feedback_delay_ms"), &AudioEffectDelay::get_feedback_delay_ms);

	ClassDB::bind_method(D_METHOD("set_feedback_level_db", "amount"), &AudioEffectDelay::set_feedback_level_db);
	ClassDB::bind_method(D_METHOD("get_feedback_level_db"), &AudioEffectDelay::get_feedback_level_db);

	ClassDB::bind_method(D_METHOD("set_feedback_lowpass", "amount"), &AudioEffectDelay::set_feedback_lowpass);
	ClassDB::bind_method(D_METHOD("get_feedback_lowpass"), &AudioEffectDelay::get_feedback_lowpass);

	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "dry", PROPERTY_HINT_RANGE, "0,1,0.01"), "set_dry", "get_dry");

	ADD_GROUP("Tap 1", "tap1_");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "tap1_active"), "set_tap1_active", "is_tap1_active");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "tap1_delay_ms", PROPERTY_HINT_RANGE, "0,1500,1,suffix:ms"), "set_tap1_delay_ms", "get_tap1_delay_ms");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "tap1_level_db", PROPERTY_HINT_RANGE, "-60,0,0.01,suffix:dB"), "set_tap1_level_db", "get_tap1_level_db");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "tap1_pan", PROPERTY_HINT_RANGE, "-1,1,0.01"), "set_tap1_pan", "get_tap1_pan");

	ADD_GROUP("Tap 2", "tap2_");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "tap2_active"), "set_tap2_active", "is_tap2_active");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "tap2_delay_ms", PROPERTY_HINT_RANGE, "0,1500,1,suffix:ms"), "set_tap2_delay_ms", "get_tap2_delay_ms");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "tap2_level_db", PROPERTY_HINT_RANGE, "-60,0,0.01,suffix:dB"), "set_tap2_level_db", "get_tap2_level_db");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "tap2_pan", PROPERTY_HINT_RANGE, "-1,1,0.01"), "set_tap2_pan", "get_tap2_pan");

	ADD_GROUP("Feedback", "feedback_");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "feedback_active"), "set_feedback_active", "is_feedback_active");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "feedback_delay_ms", PROPERTY_HINT_RANGE, "0,1500,1,suffix:ms"), "set_feedback_delay_ms", "get_feedback_delay_ms");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "feedback_level_db", PROPERTY_HINT_RANGE, "-60,0,0.01,suffix:dB"), "set_feedback_level_db", "get_feedback_level_db");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "feedback_lowpass", PROPERTY_HINT_RANGE, "1,16000,1"), "set_feedback_lowpass", "get_feedback_lowpass");
}

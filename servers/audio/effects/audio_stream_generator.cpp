/**************************************************************************/
/*  audio_stream_generator.cpp                                            */
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

#include "audio_stream_generator.h"

void AudioStreamGenerator::set_mix_rate(float p_mix_rate) {
	mix_rate = p_mix_rate;
}

float AudioStreamGenerator::get_mix_rate() const {
	return mix_rate;
}

void AudioStreamGenerator::set_buffer_length(float p_seconds) {
	buffer_len = p_seconds;
}

float AudioStreamGenerator::get_buffer_length() const {
	return buffer_len;
}

Ref<AudioStreamPlayback> AudioStreamGenerator::instantiate_playback() {
	Ref<AudioStreamGeneratorPlayback> playback;
	playback.instantiate();
	playback->generator = this;
	int target_buffer_size = mix_rate * buffer_len;
	playback->buffer.resize(nearest_shift(target_buffer_size));
	playback->buffer.clear();
	return playback;
}

String AudioStreamGenerator::get_stream_name() const {
	return "UserFeed";
}

double AudioStreamGenerator::get_length() const {
	return 0;
}

bool AudioStreamGenerator::is_monophonic() const {
	return true;
}

void AudioStreamGenerator::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_mix_rate", "hz"), &AudioStreamGenerator::set_mix_rate);
	ClassDB::bind_method(D_METHOD("get_mix_rate"), &AudioStreamGenerator::get_mix_rate);

	ClassDB::bind_method(D_METHOD("set_buffer_length", "seconds"), &AudioStreamGenerator::set_buffer_length);
	ClassDB::bind_method(D_METHOD("get_buffer_length"), &AudioStreamGenerator::get_buffer_length);

	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "mix_rate", PROPERTY_HINT_RANGE, "20,192000,1,suffix:Hz"), "set_mix_rate", "get_mix_rate");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "buffer_length", PROPERTY_HINT_RANGE, "0.01,10,0.01,suffix:s"), "set_buffer_length", "get_buffer_length");
}

AudioStreamGenerator::AudioStreamGenerator() {
	mix_rate = 44100;
	buffer_len = 0.5;
}

////////////////

bool AudioStreamGeneratorPlayback::push_frame(const Vector2 &p_frame) {
	if (buffer.space_left() < 1) {
		return false;
	}

	AudioFrame f = p_frame;

	buffer.write(&f, 1);
	return true;
}

bool AudioStreamGeneratorPlayback::can_push_buffer(int p_frames) const {
	return buffer.space_left() >= p_frames;
}

bool AudioStreamGeneratorPlayback::push_buffer(const PackedVector2Array &p_frames) {
	int to_write = p_frames.size();
	if (buffer.space_left() < to_write) {
		return false;
	}

	const Vector2 *r = p_frames.ptr();
	if constexpr (sizeof(real_t) == 4) {
		//write directly
		buffer.write((const AudioFrame *)r, to_write);
	} else {
		//convert from double
		AudioFrame buf[2048];
		int ofs = 0;
		while (to_write) {
			int w = MIN(to_write, 2048);
			for (int i = 0; i < w; i++) {
				buf[i] = r[i + ofs];
			}
			buffer.write(buf, w);
			ofs += w;
			to_write -= w;
		}
	}
	return true;
}

int AudioStreamGeneratorPlayback::get_frames_available() const {
	return buffer.space_left();
}

int AudioStreamGeneratorPlayback::get_frames_buffered() const {
	return buffer.data_left();
}

int AudioStreamGeneratorPlayback::get_frames_buffer_length() const {
	return buffer.size();
}

int AudioStreamGeneratorPlayback::get_skips() const {
	return skips;
}

void AudioStreamGeneratorPlayback::clear_buffer() {
	ERR_FAIL_COND(active);
	buffer.clear();
	mixed = 0;
}

int AudioStreamGeneratorPlayback::_mix_internal(AudioFrame *p_buffer, int p_frames) {
	if (!active) {
		return 0;
	}

	int read_amount = buffer.data_left();
	if (p_frames < read_amount) {
		read_amount = p_frames;
	}

	buffer.read(p_buffer, read_amount);

	if (read_amount < p_frames) {
		// Fill with zeros as fallback in case of buffer underrun.
		for (int i = read_amount; i < p_frames; i++) {
			p_buffer[i] = AudioFrame(0, 0);
		}
		skips++;
	}

	mixed += p_frames / generator->get_mix_rate();
	return p_frames;
}

float AudioStreamGeneratorPlayback::get_stream_sampling_rate() {
	return generator->get_mix_rate();
}

void AudioStreamGeneratorPlayback::start(double p_from_pos) {
	if (mixed == 0.0) {
		begin_resample();
	}
	skips = 0;
	active = true;
	mixed = 0.0;
}

void AudioStreamGeneratorPlayback::stop() {
	active = false;
}

bool AudioStreamGeneratorPlayback::is_playing() const {
	return active;
}

int AudioStreamGeneratorPlayback::get_loop_count() const {
	return 0;
}

double AudioStreamGeneratorPlayback::get_playback_position() const {
	return mixed;
}

void AudioStreamGeneratorPlayback::seek(double p_time) {
	//no seek possible
}

void AudioStreamGeneratorPlayback::tag_used_streams() {
	generator->tag_used(0);
}

void AudioStreamGeneratorPlayback::_bind_methods() {
	ClassDB::bind_method(D_METHOD("push_frame", "frame"), &AudioStreamGeneratorPlayback::push_frame);
	ClassDB::bind_method(D_METHOD("can_push_buffer", "amount"), &AudioStreamGeneratorPlayback::can_push_buffer);
	ClassDB::bind_method(D_METHOD("push_buffer", "frames"), &AudioStreamGeneratorPlayback::push_buffer);
	ClassDB::bind_method(D_METHOD("get_frames_available"), &AudioStreamGeneratorPlayback::get_frames_available);
	ClassDB::bind_method(D_METHOD("get_frames_buffered"), &AudioStreamGeneratorPlayback::get_frames_buffered);
	ClassDB::bind_method(D_METHOD("get_frames_buffer_length"), &AudioStreamGeneratorPlayback::get_frames_buffer_length);
	ClassDB::bind_method(D_METHOD("get_skips"), &AudioStreamGeneratorPlayback::get_skips);
	ClassDB::bind_method(D_METHOD("clear_buffer"), &AudioStreamGeneratorPlayback::clear_buffer);
}

AudioStreamGeneratorPlayback::AudioStreamGeneratorPlayback() {
	generator = nullptr;
	skips = 0;
	active = false;
	mixed = 0;
}

/**************************************************************************/
/*  audio_stream_mp3.cpp                                                  */
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

#define MINIMP3_FLOAT_OUTPUT
#define MINIMP3_IMPLEMENTATION
#define MINIMP3_NO_STDIO

#include "audio_stream_mp3.h"

int AudioStreamPlaybackMP3::_mix_internal(AudioFrame *p_buffer, int p_frames) {
	if (!active) {
		return 0;
	}

	int todo = p_frames;

	int frames_mixed_this_step = p_frames;

	int beat_length_frames = -1;
	bool use_loop = looping_override ? looping : mp3_stream->loop;

	bool beat_loop = use_loop && mp3_stream->get_bpm() > 0 && mp3_stream->get_beat_count() > 0;
	if (beat_loop) {
		beat_length_frames = mp3_stream->get_beat_count() * mp3_stream->sample_rate * 60 / mp3_stream->get_bpm();
	}

	while (todo && active) {
		mp3dec_frame_info_t frame_info;
		mp3d_sample_t *buf_frame = nullptr;

		int samples_mixed = mp3dec_ex_read_frame(&mp3d, &buf_frame, &frame_info, mp3_stream->channels);

		if (samples_mixed) {
			p_buffer[p_frames - todo] = AudioFrame(buf_frame[0], buf_frame[samples_mixed - 1]);
			if (loop_fade_remaining < FADE_SIZE) {
				p_buffer[p_frames - todo] += loop_fade[loop_fade_remaining] * (float(FADE_SIZE - loop_fade_remaining) / float(FADE_SIZE));
				loop_fade_remaining++;
			}
			--todo;
			++frames_mixed;

			if (beat_loop && (int)frames_mixed >= beat_length_frames) {
				for (int i = 0; i < FADE_SIZE; i++) {
					samples_mixed = mp3dec_ex_read_frame(&mp3d, &buf_frame, &frame_info, mp3_stream->channels);
					loop_fade[i] = AudioFrame(buf_frame[0], buf_frame[samples_mixed - 1]);
					if (!samples_mixed) {
						break;
					}
				}
				loop_fade_remaining = 0;
				seek(mp3_stream->loop_offset);
				loops++;
			}
		}

		else {
			//EOF
			if (use_loop) {
				seek(mp3_stream->loop_offset);
				loops++;
			} else {
				frames_mixed_this_step = p_frames - todo;
				//fill remainder with silence
				for (int i = p_frames - todo; i < p_frames; i++) {
					p_buffer[i] = AudioFrame(0, 0);
				}
				active = false;
				todo = 0;
			}
		}
	}
	return frames_mixed_this_step;
}

float AudioStreamPlaybackMP3::get_stream_sampling_rate() {
	return mp3_stream->sample_rate;
}

void AudioStreamPlaybackMP3::start(double p_from_pos) {
	active = true;
	seek(p_from_pos);
	loops = 0;
	begin_resample();
}

void AudioStreamPlaybackMP3::stop() {
	active = false;
}

bool AudioStreamPlaybackMP3::is_playing() const {
	return active;
}

int AudioStreamPlaybackMP3::get_loop_count() const {
	return loops;
}

double AudioStreamPlaybackMP3::get_playback_position() const {
	return double(frames_mixed) / mp3_stream->sample_rate;
}

void AudioStreamPlaybackMP3::seek(double p_time) {
	if (!active) {
		return;
	}

	if (p_time >= mp3_stream->get_length()) {
		p_time = 0;
	}

	frames_mixed = uint32_t(mp3_stream->sample_rate * p_time);
	mp3dec_ex_seek(&mp3d, (uint64_t)frames_mixed * mp3_stream->channels);
}

void AudioStreamPlaybackMP3::tag_used_streams() {
	mp3_stream->tag_used(get_playback_position());
}

void AudioStreamPlaybackMP3::set_is_sample(bool p_is_sample) {
	_is_sample = p_is_sample;
}

bool AudioStreamPlaybackMP3::get_is_sample() const {
	return _is_sample;
}

Ref<AudioSamplePlayback> AudioStreamPlaybackMP3::get_sample_playback() const {
	return sample_playback;
}

void AudioStreamPlaybackMP3::set_sample_playback(const Ref<AudioSamplePlayback> &p_playback) {
	sample_playback = p_playback;
	if (sample_playback.is_valid()) {
		sample_playback->stream_playback = Ref<AudioStreamPlayback>(this);
	}
}

void AudioStreamPlaybackMP3::set_parameter(const StringName &p_name, const Variant &p_value) {
	if (p_name == SNAME("looping")) {
		if (p_value == Variant()) {
			looping_override = false;
			looping = false;
		} else {
			looping_override = true;
			looping = p_value;
		}
	}
}

Variant AudioStreamPlaybackMP3::get_parameter(const StringName &p_name) const {
	if (looping_override && p_name == SNAME("looping")) {
		return looping;
	}
	return Variant();
}

AudioStreamPlaybackMP3::~AudioStreamPlaybackMP3() {
	mp3dec_ex_close(&mp3d);
}

Ref<AudioStreamPlayback> AudioStreamMP3::instantiate_playback() {
	Ref<AudioStreamPlaybackMP3> mp3s;

	ERR_FAIL_COND_V_MSG(data.is_empty(), mp3s,
			"This AudioStreamMP3 does not have an audio file assigned "
			"to it. AudioStreamMP3 should not be created from the "
			"inspector or with `.new()`. Instead, load an audio file.");

	mp3s.instantiate();
	mp3s->mp3_stream = Ref<AudioStreamMP3>(this);

	int errorcode = mp3dec_ex_open_buf(&mp3s->mp3d, data.ptr(), data_len, MP3D_SEEK_TO_SAMPLE);

	mp3s->frames_mixed = 0;
	mp3s->active = false;
	mp3s->loops = 0;

	if (errorcode) {
		ERR_FAIL_COND_V(errorcode, Ref<AudioStreamPlaybackMP3>());
	}

	return mp3s;
}

String AudioStreamMP3::get_stream_name() const {
	return ""; //return stream_name;
}

void AudioStreamMP3::clear_data() {
	data.clear();
}

void AudioStreamMP3::set_data(const Vector<uint8_t> &p_data) {
	int src_data_len = p_data.size();

	mp3dec_ex_t *mp3d = memnew(mp3dec_ex_t);
	int err = mp3dec_ex_open_buf(mp3d, p_data.ptr(), src_data_len, MP3D_SEEK_TO_SAMPLE);
	if (err || mp3d->info.hz == 0) {
		memdelete(mp3d);
		ERR_FAIL_MSG("Failed to decode mp3 file. Make sure it is a valid mp3 audio file.");
	}

	channels = mp3d->info.channels;
	sample_rate = mp3d->info.hz;
	length = float(mp3d->samples) / (sample_rate * float(channels));

	mp3dec_ex_close(mp3d);
	memdelete(mp3d);

	data = p_data;
	data_len = src_data_len;
}

Vector<uint8_t> AudioStreamMP3::get_data() const {
	return data;
}

void AudioStreamMP3::set_loop(bool p_enable) {
	loop = p_enable;
}

bool AudioStreamMP3::has_loop() const {
	return loop;
}

void AudioStreamMP3::set_loop_offset(double p_seconds) {
	loop_offset = p_seconds;
}

double AudioStreamMP3::get_loop_offset() const {
	return loop_offset;
}

double AudioStreamMP3::get_length() const {
	return length;
}

bool AudioStreamMP3::is_monophonic() const {
	return false;
}

void AudioStreamMP3::get_parameter_list(List<Parameter> *r_parameters) {
	r_parameters->push_back(Parameter(PropertyInfo(Variant::BOOL, "looping", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_CHECKABLE), Variant()));
}

void AudioStreamMP3::set_bpm(double p_bpm) {
	ERR_FAIL_COND(p_bpm < 0);
	bpm = p_bpm;
	emit_changed();
}

double AudioStreamMP3::get_bpm() const {
	return bpm;
}

void AudioStreamMP3::set_beat_count(int p_beat_count) {
	ERR_FAIL_COND(p_beat_count < 0);
	beat_count = p_beat_count;
	emit_changed();
}

int AudioStreamMP3::get_beat_count() const {
	return beat_count;
}

void AudioStreamMP3::set_bar_beats(int p_bar_beats) {
	ERR_FAIL_COND(p_bar_beats < 0);
	bar_beats = p_bar_beats;
	emit_changed();
}

int AudioStreamMP3::get_bar_beats() const {
	return bar_beats;
}

Ref<AudioSample> AudioStreamMP3::generate_sample() const {
	Ref<AudioSample> sample;
	sample.instantiate();
	sample->stream = this;
	sample->loop_mode = loop
			? AudioSample::LoopMode::LOOP_FORWARD
			: AudioSample::LoopMode::LOOP_DISABLED;
	sample->loop_begin = loop_offset;
	sample->loop_end = 0;
	return sample;
}

void AudioStreamMP3::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_data", "data"), &AudioStreamMP3::set_data);
	ClassDB::bind_method(D_METHOD("get_data"), &AudioStreamMP3::get_data);

	ClassDB::bind_method(D_METHOD("set_loop", "enable"), &AudioStreamMP3::set_loop);
	ClassDB::bind_method(D_METHOD("has_loop"), &AudioStreamMP3::has_loop);

	ClassDB::bind_method(D_METHOD("set_loop_offset", "seconds"), &AudioStreamMP3::set_loop_offset);
	ClassDB::bind_method(D_METHOD("get_loop_offset"), &AudioStreamMP3::get_loop_offset);

	ClassDB::bind_method(D_METHOD("set_bpm", "bpm"), &AudioStreamMP3::set_bpm);
	ClassDB::bind_method(D_METHOD("get_bpm"), &AudioStreamMP3::get_bpm);

	ClassDB::bind_method(D_METHOD("set_beat_count", "count"), &AudioStreamMP3::set_beat_count);
	ClassDB::bind_method(D_METHOD("get_beat_count"), &AudioStreamMP3::get_beat_count);

	ClassDB::bind_method(D_METHOD("set_bar_beats", "count"), &AudioStreamMP3::set_bar_beats);
	ClassDB::bind_method(D_METHOD("get_bar_beats"), &AudioStreamMP3::get_bar_beats);

	ADD_PROPERTY(PropertyInfo(Variant::PACKED_BYTE_ARRAY, "data", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NO_EDITOR), "set_data", "get_data");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "bpm", PROPERTY_HINT_RANGE, "0,400,0.01,or_greater"), "set_bpm", "get_bpm");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "beat_count", PROPERTY_HINT_RANGE, "0,512,1,or_greater"), "set_beat_count", "get_beat_count");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "bar_beats", PROPERTY_HINT_RANGE, "2,32,1,or_greater"), "set_bar_beats", "get_bar_beats");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "loop"), "set_loop", "has_loop");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "loop_offset"), "set_loop_offset", "get_loop_offset");
}

AudioStreamMP3::AudioStreamMP3() {
}

AudioStreamMP3::~AudioStreamMP3() {
	clear_data();
}

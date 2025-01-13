/**************************************************************************/
/*  audio_stream_polyphonic.cpp                                           */
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

#include "audio_stream_polyphonic.h"
#include "audio_stream_polyphonic.compat.inc"

#include "scene/main/scene_tree.h"
#include "servers/audio_server.h"

constexpr uint64_t ID_MASK = 0xFFFFFFFF;
constexpr uint64_t INDEX_SHIFT = 32;

Ref<AudioStreamPlayback> AudioStreamPolyphonic::instantiate_playback() {
	Ref<AudioStreamPlaybackPolyphonic> playback;
	playback.instantiate();
	playback->streams.resize(polyphony);
	return playback;
}

String AudioStreamPolyphonic::get_stream_name() const {
	return "AudioStreamPolyphonic";
}

bool AudioStreamPolyphonic::is_monophonic() const {
	return true; // This avoids stream players to instantiate more than one of these.
}

void AudioStreamPolyphonic::set_polyphony(int p_voices) {
	ERR_FAIL_COND(p_voices < 0 || p_voices > 128);
	polyphony = p_voices;
}
int AudioStreamPolyphonic::get_polyphony() const {
	return polyphony;
}

void AudioStreamPolyphonic::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_polyphony", "voices"), &AudioStreamPolyphonic::set_polyphony);
	ClassDB::bind_method(D_METHOD("get_polyphony"), &AudioStreamPolyphonic::get_polyphony);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "polyphony", PROPERTY_HINT_RANGE, "1,128,1"), "set_polyphony", "get_polyphony");
}

AudioStreamPolyphonic::AudioStreamPolyphonic() {
}

////////////////////////

void AudioStreamPlaybackPolyphonic::start(double p_from_pos) {
	if (active) {
		stop();
	}

	active = true;
}

void AudioStreamPlaybackPolyphonic::stop() {
	if (!active) {
		return;
	}

	bool locked = false;
	for (Stream &s : streams) {
		if (s.active.is_set()) {
			// Need locking because something may still be mixing.
			locked = true;
			AudioServer::get_singleton()->lock();
		}
		s.active.clear();
		s.finish_request.clear();
		s.stream_playback.unref();
		s.stream.unref();
	}
	if (locked) {
		AudioServer::get_singleton()->unlock();
	}

	active = false;
}

bool AudioStreamPlaybackPolyphonic::is_playing() const {
	return active;
}

int AudioStreamPlaybackPolyphonic::get_loop_count() const {
	return 0;
}

double AudioStreamPlaybackPolyphonic::get_playback_position() const {
	return 0;
}
void AudioStreamPlaybackPolyphonic::seek(double p_time) {
	// Ignored.
}

void AudioStreamPlaybackPolyphonic::tag_used_streams() {
	for (Stream &s : streams) {
		if (s.active.is_set()) {
			s.stream_playback->tag_used_streams();
		}
	}
}

int AudioStreamPlaybackPolyphonic::mix(AudioFrame *p_buffer, float p_rate_scale, int p_frames) {
	if (!active) {
		return 0;
	}

	// Pre-clear buffer.
	for (int i = 0; i < p_frames; i++) {
		p_buffer[i] = AudioFrame(0, 0);
	}

	for (Stream &s : streams) {
		if (!s.active.is_set()) {
			continue;
		}

		if (s.stream_playback->get_is_sample()) {
			if (s.finish_request.is_set()) {
				s.active.clear();
				AudioServer::get_singleton()->stop_sample_playback(s.stream_playback->get_sample_playback());
			}
			continue;
		}

		float volume_db = s.volume_db; // Copy because it can be overridden at any time.
		float next_volume = Math::db_to_linear(volume_db);
		s.prev_volume_db = volume_db;

		if (s.finish_request.is_set()) {
			if (s.pending_play.is_set()) {
				// Did not get the chance to play, was finalized too soon.
				s.active.clear();
				continue;
			}
			next_volume = 0;
		}

		if (s.pending_play.is_set()) {
			s.stream_playback->start(s.play_offset);
			s.pending_play.clear();
		}
		float prev_volume = Math::db_to_linear(s.prev_volume_db);

		float volume_inc = (next_volume - prev_volume) / float(p_frames);

		int todo = p_frames;
		int offset = 0;
		float volume = prev_volume;

		bool stream_done = false;

		while (todo) {
			int to_mix = MIN(todo, int(INTERNAL_BUFFER_LEN));
			int mixed = s.stream_playback->mix(internal_buffer, s.pitch_scale, to_mix);

			for (int i = 0; i < to_mix; i++) {
				p_buffer[offset + i] += internal_buffer[i] * volume;
				volume += volume_inc;
			}

			if (mixed < to_mix) {
				// Stream is done.
				s.active.clear();
				stream_done = true;
				break;
			}

			todo -= to_mix;
			offset += to_mix;
		}

		if (stream_done) {
			continue;
		}

		if (s.finish_request.is_set()) {
			s.active.clear();
		}
	}

	return p_frames;
}

AudioStreamPlaybackPolyphonic::ID AudioStreamPlaybackPolyphonic::play_stream(const Ref<AudioStream> &p_stream, float p_from_offset, float p_volume_db, float p_pitch_scale, AudioServer::PlaybackType p_playback_type, const StringName &p_bus) {
	ERR_FAIL_COND_V(p_stream.is_null(), INVALID_ID);

	AudioServer::PlaybackType playback_type = p_playback_type == AudioServer::PlaybackType::PLAYBACK_TYPE_DEFAULT
			? AudioServer::get_singleton()->get_default_playback_type()
			: p_playback_type;

	for (uint32_t i = 0; i < streams.size(); i++) {
		if (!streams[i].active.is_set()) {
			// Can use this stream, as it's not active.
			streams[i].stream = p_stream;
			streams[i].stream_playback = streams[i].stream->instantiate_playback();
			streams[i].play_offset = p_from_offset;
			streams[i].volume_db = p_volume_db;
			streams[i].prev_volume_db = p_volume_db;
			streams[i].pitch_scale = p_pitch_scale;
			streams[i].id = id_counter++;
			streams[i].finish_request.clear();
			streams[i].pending_play.set();
			streams[i].active.set();

			// Sample playback.
			if (playback_type == AudioServer::PlaybackType::PLAYBACK_TYPE_SAMPLE && p_stream->can_be_sampled()) {
				streams[i].stream_playback->set_is_sample(true);
				if (!AudioServer::get_singleton()->is_stream_registered_as_sample(p_stream)) {
					AudioServer::get_singleton()->register_stream_as_sample(p_stream);
				}
				float linear_volume = Math::db_to_linear(p_volume_db);
				Ref<AudioSamplePlayback> sp;
				sp.instantiate();
				sp->stream = streams[i].stream;
				sp->offset = p_from_offset;
				sp->volume_vector.resize(4);
				sp->volume_vector.write[0] = AudioFrame(linear_volume, linear_volume);
				sp->volume_vector.write[1] = AudioFrame(linear_volume, /* LFE= */ 1.0f);
				sp->volume_vector.write[2] = AudioFrame(linear_volume, linear_volume);
				sp->volume_vector.write[3] = AudioFrame(linear_volume, linear_volume);
				sp->bus = p_bus;

				if (streams[i].stream_playback->get_sample_playback().is_valid()) {
					AudioServer::get_singleton()->stop_playback_stream(sp);
				}

				streams[i].stream_playback->set_sample_playback(sp);
				AudioServer::get_singleton()->start_sample_playback(sp);
			}

			return (ID(i) << INDEX_SHIFT) | ID(streams[i].id);
		}
	}

	return INVALID_ID;
}

AudioStreamPlaybackPolyphonic::Stream *AudioStreamPlaybackPolyphonic::_find_stream(int64_t p_id) {
	uint32_t index = static_cast<uint64_t>(p_id) >> INDEX_SHIFT;
	if (index >= streams.size()) {
		return nullptr;
	}
	if (!streams[index].active.is_set()) {
		return nullptr; // Not active, no longer exists.
	}
	int64_t id = static_cast<uint64_t>(p_id) & ID_MASK;
	if (streams[index].id != id) {
		return nullptr;
	}
	return &streams[index];
}

const AudioStreamPlaybackPolyphonic::Stream *AudioStreamPlaybackPolyphonic::_find_stream(int64_t p_id) const {
	uint32_t index = static_cast<uint64_t>(p_id) >> INDEX_SHIFT;
	if (index >= streams.size()) {
		return nullptr;
	}
	if (!streams[index].active.is_set()) {
		return nullptr; // Not active, no longer exists.
	}
	int64_t id = static_cast<uint64_t>(p_id) & ID_MASK;
	if (streams[index].id != id) {
		return nullptr;
	}
	return &streams[index];
}

void AudioStreamPlaybackPolyphonic::set_stream_volume(ID p_stream_id, float p_volume_db) {
	Stream *s = _find_stream(p_stream_id);
	if (!s) {
		return;
	}
	s->volume_db = p_volume_db;
}

void AudioStreamPlaybackPolyphonic::set_stream_pitch_scale(ID p_stream_id, float p_pitch_scale) {
	Stream *s = _find_stream(p_stream_id);
	if (!s) {
		return;
	}
	s->pitch_scale = p_pitch_scale;
}

bool AudioStreamPlaybackPolyphonic::is_stream_playing(ID p_stream_id) const {
	return _find_stream(p_stream_id) != nullptr;
}

void AudioStreamPlaybackPolyphonic::stop_stream(ID p_stream_id) {
	Stream *s = _find_stream(p_stream_id);
	if (!s) {
		return;
	}
	s->finish_request.set();
}

void AudioStreamPlaybackPolyphonic::set_is_sample(bool p_is_sample) {
	_is_sample = p_is_sample;
}

bool AudioStreamPlaybackPolyphonic::get_is_sample() const {
	return _is_sample;
}

Ref<AudioSamplePlayback> AudioStreamPlaybackPolyphonic::get_sample_playback() const {
	return sample_playback;
}

void AudioStreamPlaybackPolyphonic::set_sample_playback(const Ref<AudioSamplePlayback> &p_playback) {
	sample_playback = p_playback;
	if (sample_playback.is_valid()) {
		sample_playback->stream_playback = Ref<AudioStreamPlayback>(this);
	}
}

void AudioStreamPlaybackPolyphonic::_bind_methods() {
	ClassDB::bind_method(D_METHOD("play_stream", "stream", "from_offset", "volume_db", "pitch_scale", "playback_type", "bus"), &AudioStreamPlaybackPolyphonic::play_stream, DEFVAL(0), DEFVAL(0), DEFVAL(1.0), DEFVAL(0), DEFVAL(SceneStringName(Master)));
	ClassDB::bind_method(D_METHOD("set_stream_volume", "stream", "volume_db"), &AudioStreamPlaybackPolyphonic::set_stream_volume);
	ClassDB::bind_method(D_METHOD("set_stream_pitch_scale", "stream", "pitch_scale"), &AudioStreamPlaybackPolyphonic::set_stream_pitch_scale);
	ClassDB::bind_method(D_METHOD("is_stream_playing", "stream"), &AudioStreamPlaybackPolyphonic::is_stream_playing);
	ClassDB::bind_method(D_METHOD("stop_stream", "stream"), &AudioStreamPlaybackPolyphonic::stop_stream);

	BIND_CONSTANT(INVALID_ID);
}

AudioStreamPlaybackPolyphonic::AudioStreamPlaybackPolyphonic() {
}

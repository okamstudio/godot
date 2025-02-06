/**************************************************************************/
/*  audio_stream_synchronized.cpp                                         */
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

#include "audio_stream_synchronized.h"

#include "core/math/math_funcs.h"

AudioStreamSynchronized::AudioStreamSynchronized() {
}

AudioStreamSynchronized::~AudioStreamSynchronized() {
	if (playback_sync.is_valid()) {
		playback_sync.unref();
	}
}

Ref<AudioStreamPlayback> AudioStreamSynchronized::instantiate_playback() {
	if (!playback_sync.is_valid()) {
		playback_sync.instantiate();
		playback_sync->stream = Ref<AudioStreamSynchronized>(this);
		playback_sync->_update_playback_instances();
	}
	
	return playback_sync;
}

String AudioStreamSynchronized::get_stream_name() const {
	return "Synchronized";
}

void AudioStreamSynchronized::set_sync_stream(int p_stream_index, Ref<AudioStream> p_stream) {
	ERR_FAIL_COND(p_stream == this);
	ERR_FAIL_INDEX(p_stream_index, MAX_STREAMS);

	AudioServer::get_singleton()->lock();
	audio_streams[p_stream_index].unref();
	audio_streams[p_stream_index] = p_stream;

	if (playback_sync.is_valid()) {
		playback_sync->playbacks[p_stream_index].unref();

		if (audio_streams[p_stream_index].is_valid()) {
			double playing_pos = playback_sync->is_playing() ? playback_sync->get_playback_position() : -1;
			playback_sync->playbacks[p_stream_index] = audio_streams[p_stream_index]->instantiate_playback();

			if (playing_pos >= 0) {
				playback_sync->playbacks[p_stream_index]->start(playing_pos);
			}
		}
	}

	AudioServer::get_singleton()->unlock();
}

Ref<AudioStream> AudioStreamSynchronized::get_sync_stream(int p_stream_index) const {
	ERR_FAIL_INDEX_V(p_stream_index, MAX_STREAMS, Ref<AudioStream>());

	return audio_streams[p_stream_index];
}

void AudioStreamSynchronized::set_sync_stream_volume(int p_stream_index, float p_db) {
	ERR_FAIL_INDEX(p_stream_index, MAX_STREAMS);
	audio_stream_volume_db[p_stream_index] = p_db;
}

float AudioStreamSynchronized::get_sync_stream_volume(int p_stream_index) const {
	ERR_FAIL_INDEX_V(p_stream_index, MAX_STREAMS, 0);
	return audio_stream_volume_db[p_stream_index];
}

double AudioStreamSynchronized::get_bpm() const {
	for (int i = 0; i < stream_count; i++) {
		if (audio_streams[i].is_valid()) {
			double bpm = audio_streams[i]->get_bpm();
			if (bpm != 0.0) {
				return bpm;
			}
		}
	}
	return 0.0;
}

int AudioStreamSynchronized::get_beat_count() const {
	int max_beats = 0;
	for (int i = 0; i < stream_count; i++) {
		if (audio_streams[i].is_valid()) {
			max_beats = MAX(max_beats, audio_streams[i]->get_beat_count());
		}
	}
	return max_beats;
}

int AudioStreamSynchronized::get_bar_beats() const {
	for (int i = 0; i < stream_count; i++) {
		if (audio_streams[i].is_valid()) {
			int bar_beats = audio_streams[i]->get_bar_beats();
			if (bar_beats != 0) {
				return bar_beats;
			}
		}
	}
	return 0;
}

bool AudioStreamSynchronized::has_loop() const {
	for (int i = 0; i < stream_count; i++) {
		if (audio_streams[i].is_valid()) {
			if (audio_streams[i]->has_loop()) {
				return true;
			}
		}
	}
	return false;
}

double AudioStreamSynchronized::get_length() const {
	double max_length = 0.0;
	for (int i = 0; i < stream_count; i++) {
		if (audio_streams[i].is_valid()) {
			max_length = MAX(max_length, audio_streams[i]->get_length());
		}
	}
	return max_length;
}

void AudioStreamSynchronized::set_stream_count(int p_count) {
	ERR_FAIL_COND(p_count < 0 || p_count > MAX_STREAMS);
	AudioServer::get_singleton()->lock();
	stream_count = p_count;
	AudioServer::get_singleton()->unlock();
	notify_property_list_changed();
}

int AudioStreamSynchronized::get_stream_count() const {
	return stream_count;
}

void AudioStreamSynchronized::_validate_property(PropertyInfo &property) const {
	String prop = property.name;
	if (prop != "stream_count" && prop.begins_with("stream_")) {
		int stream = prop.get_slicec('/', 0).get_slicec('_', 1).to_int();
		if (stream >= stream_count) {
			property.usage = PROPERTY_USAGE_INTERNAL;
		}
	}
}

void AudioStreamSynchronized::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_stream_count", "stream_count"), &AudioStreamSynchronized::set_stream_count);
	ClassDB::bind_method(D_METHOD("get_stream_count"), &AudioStreamSynchronized::get_stream_count);

	ClassDB::bind_method(D_METHOD("set_sync_stream", "stream_index", "audio_stream"), &AudioStreamSynchronized::set_sync_stream);
	ClassDB::bind_method(D_METHOD("get_sync_stream", "stream_index"), &AudioStreamSynchronized::get_sync_stream);
	ClassDB::bind_method(D_METHOD("set_sync_stream_volume", "stream_index", "volume_db"), &AudioStreamSynchronized::set_sync_stream_volume);
	ClassDB::bind_method(D_METHOD("get_sync_stream_volume", "stream_index"), &AudioStreamSynchronized::get_sync_stream_volume);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "stream_count", PROPERTY_HINT_RANGE, "0," + itos(MAX_STREAMS), PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_ARRAY, "Streams,stream_,unfoldable,page_size=999,add_button_text=" + String(RTR("Add Stream"))), "set_stream_count", "get_stream_count");

	for (int i = 0; i < MAX_STREAMS; i++) {
		ADD_PROPERTYI(PropertyInfo(Variant::OBJECT, "stream_" + itos(i) + "/stream", PROPERTY_HINT_RESOURCE_TYPE, "AudioStream", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_INTERNAL), "set_sync_stream", "get_sync_stream", i);
		ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "stream_" + itos(i) + "/volume", PROPERTY_HINT_RANGE, "-60,12,0.01,suffix:db", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_INTERNAL), "set_sync_stream_volume", "get_sync_stream_volume", i);
	}

	BIND_CONSTANT(MAX_STREAMS);
}

//////////////////////
//////////////////////

AudioStreamPlaybackSynchronized::AudioStreamPlaybackSynchronized() {
}

void AudioStreamPlaybackSynchronized::stop() {
	active = false;
	for (int i = 0; i < stream->stream_count; i++) {
		if (playbacks[i].is_valid()) {
			playbacks[i]->stop();
		}
	}
}

void AudioStreamPlaybackSynchronized::start(double p_from_pos) {
	_seek_or_start(p_from_pos, true);
}

void AudioStreamPlaybackSynchronized::seek(double p_time) {
	_seek_or_start(p_time, false);
}

void AudioStreamPlaybackSynchronized::_seek_or_start(double p_from_pos, bool is_start) {
	if (is_start && active) {
		stop();
	}

	bool is_pos_within_range = p_from_pos >= 0 && p_from_pos < stream->get_length();

	for (int i = 0; i < stream->stream_count; i++) {
		if (playbacks[i].is_valid() && stream->audio_streams[i].is_valid()) {
			double audio_stream_length = stream->audio_streams[i]->get_length();
			double playback_pos = p_from_pos;

			if (is_pos_within_range && audio_stream_length < playback_pos) {
				if (!stream->audio_streams[i]->has_loop()) {
					continue;
				}

				while (audio_stream_length < playback_pos) {
					playback_pos -= audio_stream_length;
				}
			}

			if (is_start) {
				playbacks[i]->start(playback_pos);
			} else {
				playbacks[i]->seek(playback_pos);
			}

			active = true;
		}
	}
}

int AudioStreamPlaybackSynchronized::mix(AudioFrame *p_buffer, float p_rate_scale, int p_frames) {
	if (!active) {
		return 0;
	}

	int todo = p_frames;

	bool any_active = false;
	while (todo) {
		int to_mix = MIN(todo, MIX_BUFFER_SIZE);

		bool first = true;
		for (int i = 0; i < stream->stream_count; i++) {
			if (playbacks[i].is_valid() && playbacks[i]->is_playing()) {
				float volume = Math::db_to_linear(stream->audio_stream_volume_db[i]);
				if (first) {
					playbacks[i]->mix(p_buffer, p_rate_scale, to_mix);
					for (int j = 0; j < to_mix; j++) {
						p_buffer[j] *= volume;
					}
					first = false;
					any_active = true;
				} else {
					playbacks[i]->mix(mix_buffer, p_rate_scale, to_mix);
					for (int j = 0; j < to_mix; j++) {
						p_buffer[j] += mix_buffer[j] * volume;
					}
				}
			}
		}

		if (first) {
			// Nothing mixed, put zeroes.
			for (int j = 0; j < to_mix; j++) {
				p_buffer[j] = AudioFrame(0, 0);
			}
		}

		p_buffer += to_mix;
		todo -= to_mix;
	}

	if (!any_active) {
		active = false;
	}
	return p_frames;
}

void AudioStreamPlaybackSynchronized::tag_used_streams() {
	if (active) {
		for (int i = 0; i < stream->stream_count; i++) {
			if (playbacks[i].is_valid() && playbacks[i]->is_playing()) {
				stream->audio_streams[i]->tag_used(playbacks[i]->get_playback_position());
			}
		}
		stream->tag_used(0);
	}
}

int AudioStreamPlaybackSynchronized::get_loop_count() const {
	int min_loops = 0;
	bool min_loops_found = false;
	for (int i = 0; i < stream->stream_count; i++) {
		if (playbacks[i].is_valid() && playbacks[i]->is_playing()) {
			int loops = playbacks[i]->get_loop_count();
			if (!min_loops_found || loops < min_loops) {
				min_loops = loops;
				min_loops_found = true;
			}
		}
	}
	return min_loops;
}

double AudioStreamPlaybackSynchronized::get_playback_position() const {
	float max_pos = 0;
	bool pos_found = false;
	for (int i = 0; i < stream->stream_count; i++) {
		if (playbacks[i].is_valid() && playbacks[i]->is_playing()) {
			float pos = playbacks[i]->get_playback_position();
			if (!pos_found || pos > max_pos) {
				max_pos = pos;
				pos_found = true;
			}
		}
	}
	return max_pos;
}

bool AudioStreamPlaybackSynchronized::is_playing() const {
	return active;
}

void AudioStreamPlaybackSynchronized::_update_playback_instances() {
	AudioServer::get_singleton()->lock();
	for (int i = 0; i < stream->stream_count; i++) {
		playbacks[i].unref();

		if (stream->audio_streams[i].is_valid()) {
			playbacks[i] = stream->audio_streams[i]->instantiate_playback();
		}
	}
	AudioServer::get_singleton()->unlock();
}

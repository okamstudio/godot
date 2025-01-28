/**************************************************************************/
/*  audio_stream_playlist.cpp                                             */
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

#include "audio_stream_playlist.h"

#include "core/math/math_funcs.h"

Ref<AudioStreamPlayback> AudioStreamPlaylist::instantiate_playback() {
	Ref<AudioStreamPlaybackPlaylist> playback_playlist;
	playback_playlist.instantiate();
	playback_playlist->playlist = Ref<AudioStreamPlaylist>(this);
	playback_playlist->_update_playback_instances();
	playbacks.insert(playback_playlist.operator->());
	return playback_playlist;
}

String AudioStreamPlaylist::get_stream_name() const {
	return "Playlist";
}

void AudioStreamPlaylist::set_list_stream(int p_stream_index, Ref<AudioStream> p_stream) {
	ERR_FAIL_COND(p_stream == this);
	ERR_FAIL_INDEX(p_stream_index, MAX_STREAMS);

	AudioServer::get_singleton()->lock();
	audio_streams[p_stream_index] = p_stream;
	for (AudioStreamPlaybackPlaylist *E : playbacks) {
		E->_update_playback_instances();
	}
	AudioServer::get_singleton()->unlock();
}

Ref<AudioStream> AudioStreamPlaylist::get_list_stream(int p_stream_index) const {
	ERR_FAIL_INDEX_V(p_stream_index, MAX_STREAMS, Ref<AudioStream>());

	return audio_streams[p_stream_index];
}

double AudioStreamPlaylist::get_bpm() const {
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

double AudioStreamPlaylist::get_length() const {
	double total_length = 0.0;
	for (int i = 0; i < stream_count; i++) {
		if (audio_streams[i].is_valid()) {
			double bpm = audio_streams[i]->get_bpm();
			int beat_count = audio_streams[i]->get_beat_count();
			if (bpm > 0.0 && beat_count > 0) {
				total_length += beat_count * 60.0 / bpm;
			} else {
				total_length += audio_streams[i]->get_length();
			}
		}
	}
	return total_length;
}

void AudioStreamPlaylist::set_stream_count(int p_count) {
	ERR_FAIL_COND(p_count < 0 || p_count > MAX_STREAMS);
	AudioServer::get_singleton()->lock();
	stream_count = p_count;
	AudioServer::get_singleton()->unlock();
	notify_property_list_changed();
}

int AudioStreamPlaylist::get_stream_count() const {
	return stream_count;
}

void AudioStreamPlaylist::set_fade_time(float p_time) {
	fade_time = p_time;
}

float AudioStreamPlaylist::get_fade_time() const {
	return fade_time;
}

void AudioStreamPlaylist::set_shuffle(bool p_shuffle) {
	shuffle = p_shuffle;
}

bool AudioStreamPlaylist::get_shuffle() const {
	return shuffle;
}

void AudioStreamPlaylist::set_loop(bool p_loop) {
	loop = p_loop;
}

bool AudioStreamPlaylist::has_loop() const {
	return loop;
}

void AudioStreamPlaylist::_validate_property(PropertyInfo &r_property) const {
	String prop = r_property.name;
	if (prop != "stream_count" && prop.begins_with("stream_")) {
		int stream = prop.get_slicec('/', 0).get_slicec('_', 1).to_int();
		if (stream >= stream_count) {
			r_property.usage = PROPERTY_USAGE_INTERNAL;
		}
	}
}

void AudioStreamPlaylist::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_stream_count", "stream_count"), &AudioStreamPlaylist::set_stream_count);
	ClassDB::bind_method(D_METHOD("get_stream_count"), &AudioStreamPlaylist::get_stream_count);

	ClassDB::bind_method(D_METHOD("get_bpm"), &AudioStreamPlaylist::get_bpm);

	ClassDB::bind_method(D_METHOD("set_list_stream", "stream_index", "audio_stream"), &AudioStreamPlaylist::set_list_stream);
	ClassDB::bind_method(D_METHOD("get_list_stream", "stream_index"), &AudioStreamPlaylist::get_list_stream);

	ClassDB::bind_method(D_METHOD("set_shuffle", "shuffle"), &AudioStreamPlaylist::set_shuffle);
	ClassDB::bind_method(D_METHOD("get_shuffle"), &AudioStreamPlaylist::get_shuffle);

	ClassDB::bind_method(D_METHOD("set_fade_time", "dec"), &AudioStreamPlaylist::set_fade_time);
	ClassDB::bind_method(D_METHOD("get_fade_time"), &AudioStreamPlaylist::get_fade_time);

	ClassDB::bind_method(D_METHOD("set_loop", "loop"), &AudioStreamPlaylist::set_loop);
	ClassDB::bind_method(D_METHOD("has_loop"), &AudioStreamPlaylist::has_loop);

	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "shuffle"), "set_shuffle", "get_shuffle");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "loop"), "set_loop", "has_loop");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "fade_time", PROPERTY_HINT_RANGE, "0,1,0.01,suffix:s"), "set_fade_time", "get_fade_time");

	ADD_PROPERTY(PropertyInfo(Variant::INT, "stream_count", PROPERTY_HINT_RANGE, "0," + itos(MAX_STREAMS), PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_ARRAY, "Streams,stream_,unfoldable,page_size=999,add_button_text=" + String(RTR("Add Stream"))), "set_stream_count", "get_stream_count");

	for (int i = 0; i < MAX_STREAMS; i++) {
		ADD_PROPERTYI(PropertyInfo(Variant::OBJECT, "stream_" + itos(i), PROPERTY_HINT_RESOURCE_TYPE, "AudioStream", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_INTERNAL), "set_list_stream", "get_list_stream", i);
	}

	BIND_CONSTANT(MAX_STREAMS);
}

//////////////////////
//////////////////////

AudioStreamPlaybackPlaylist::~AudioStreamPlaybackPlaylist() {
	if (playlist.is_valid()) {
		playlist->playbacks.erase(this);
	}
}

void AudioStreamPlaybackPlaylist::stop() {
	active = false;
	for (int i = 0; i < playlist->stream_count; i++) {
		if (playback[i].is_valid()) {
			playback[i]->stop();
		}
	}
}

void AudioStreamPlaybackPlaylist::_update_order() {
	for (int i = 0; i < playlist->stream_count; i++) {
		play_order[i] = i;
	}

	if (playlist->shuffle) {
		for (int i = 0; i < playlist->stream_count; i++) {
			int swap_with = Math::rand() % uint32_t(playlist->stream_count);
			SWAP(play_order[i], play_order[swap_with]);
		}
	}
}

void AudioStreamPlaybackPlaylist::start(double p_from_pos) {
	if (active) {
		stop();
	}

	p_from_pos = MAX(0, p_from_pos);

	float pl_length = playlist->get_length();
	if (p_from_pos >= pl_length) {
		if (!playlist->loop) {
			return; // No loop, end.
		}
		p_from_pos = Math::fmod((float)p_from_pos, (float)pl_length);
	}

	_update_order();

	play_index = -1;

	double play_ofs = p_from_pos;
	for (int i = 0; i < playlist->stream_count; i++) {
		int idx = play_order[i];
		if (playlist->audio_streams[idx].is_valid()) {
			double bpm = playlist->audio_streams[idx]->get_bpm();
			int beat_count = playlist->audio_streams[idx]->get_beat_count();
			double length;
			if (bpm > 0.0 && beat_count > 0) {
				length = beat_count * 60.0 / bpm;
			} else {
				length = playlist->audio_streams[idx]->get_length();
			}
			if (play_ofs < length) {
				play_index = i;
				stream_todo = length - play_ofs;
				break;
			} else {
				play_ofs -= length;
			}
		}
	}

	if (play_index == -1) {
		return;
	}

	playback[play_order[play_index]]->start(play_ofs);
	fade_index = -1;
	loop_count = 0;

	active = true;
}

void AudioStreamPlaybackPlaylist::seek(double p_time) {
	stop();
	start(p_time);
}

int AudioStreamPlaybackPlaylist::mix(AudioFrame *p_buffer, float p_rate_scale, int p_frames) {
	if (!active) {
		return 0;
	}

	double time_dec = (1.0 / AudioServer::get_singleton()->get_mix_rate());
	double fade_dec = (1.0 / playlist->fade_time) / AudioServer::get_singleton()->get_mix_rate();

	int todo = p_frames;

	while (todo) {
		int to_mix = MIN(todo, MIX_BUFFER_SIZE);

		playback[play_order[play_index]]->mix(mix_buffer, 1.0, to_mix);
		if (fade_index != -1) {
			playback[fade_index]->mix(fade_buffer, 1.0, to_mix);
		}

		offset += time_dec * to_mix;

		for (int i = 0; i < to_mix; i++) {
			*p_buffer = mix_buffer[i];
			stream_todo -= time_dec;
			if (stream_todo < 0) {
				//find next stream.
				int prev = play_order[play_index];

				for (int j = 0; j < playlist->stream_count; j++) {
					play_index++;
					if (play_index >= playlist->stream_count) {
						// No loop, exit.
						if (!playlist->loop) {
							for (int k = i; k < todo - i; k++) {
								p_buffer[k] = AudioFrame(0, 0);
							}
							todo = to_mix;
							active = false;
							break;
						}

						_update_order();
						play_index = 0;
						loop_count++;
						offset = time_dec * (to_mix - i);
					}
					if (playback[play_order[play_index]].is_valid()) {
						break;
					}
				}

				if (!active) {
					break;
				}

				if (playback[play_order[play_index]].is_null()) {
					todo = to_mix; // Weird error.
					active = false;
					break;
				}

				bool restart = true;
				if (prev == play_order[play_index]) {
					// Went back to the same one, continue loop (if it loops) or restart if it does not.
					if (playlist->audio_streams[prev]->has_loop()) {
						restart = false;
					}
					fade_index = -1;
				} else {
					// Move current mixed data to fade buffer.
					for (int j = i; j < to_mix; j++) {
						fade_buffer[j] = mix_buffer[j];
					}

					fade_index = prev;
					fade_volume = 1.0;
				}

				int idx = play_order[play_index];

				if (restart) {
					playback[idx]->start(0); // No loop, just cold-restart.
					playback[idx]->mix(mix_buffer + i, 1.0, to_mix - i); // Fill rest of mix buffer
				}

				// Update fade todo.
				double bpm = playlist->audio_streams[idx]->get_bpm();
				int beat_count = playlist->audio_streams[idx]->get_beat_count();

				if (bpm > 0.0 && beat_count > 0) {
					stream_todo = beat_count * 60.0 / bpm;
				} else {
					stream_todo = playlist->audio_streams[idx]->get_length();
				}
			}

			if (fade_index != -1) {
				*p_buffer += fade_buffer[i] * fade_volume;
				fade_volume -= fade_dec;
				if (fade_volume <= 0.0) {
					playback[fade_index]->stop();
					fade_index = -1;
				}
			}

			p_buffer++;
		}

		todo -= to_mix;
	}

	return p_frames;
}

void AudioStreamPlaybackPlaylist::tag_used_streams() {
	if (active) {
		playlist->audio_streams[play_order[play_index]]->tag_used(playback[play_order[play_index]]->get_playback_position());
	}

	playlist->tag_used(0);
}

int AudioStreamPlaybackPlaylist::get_loop_count() const {
	return loop_count;
}

double AudioStreamPlaybackPlaylist::get_playback_position() const {
	return offset;
}

bool AudioStreamPlaybackPlaylist::is_playing() const {
	return active;
}

void AudioStreamPlaybackPlaylist::_update_playback_instances() {
	stop();

	for (int i = 0; i < playlist->stream_count; i++) {
		if (playlist->audio_streams[i].is_valid()) {
			playback[i] = playlist->audio_streams[i]->instantiate_playback();
		} else {
			playback[i].unref();
		}
	}
}

/**************************************************************************/
/*  video_stream_player.cpp                                               */
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

#include "video_stream_player.h"

#include "core/os/os.h"
#include "servers/audio_server.h"

int VideoStreamPlayer::sp_get_channel_count() const {
	if (playback.is_null()) {
		return 0;
	}

	return playback->get_channels();
}

bool VideoStreamPlayer::mix(AudioFrame *p_buffer, int p_frames) {
	// Check the amount resampler can really handle.
	// If it cannot, wait "wait_resampler_phase_limit" times.
	// This mechanism contributes to smoother pause/unpause operation.
	if (p_frames <= resampler.get_num_of_ready_frames() ||
			wait_resampler_limit <= wait_resampler) {
		wait_resampler = 0;
		return resampler.mix(p_buffer, p_frames);
	}
	wait_resampler++;
	return false;
}

// Called from main thread (e.g. VideoStreamPlaybackTheora::update).
int VideoStreamPlayer::_audio_mix_callback(void *p_udata, const float *p_data, int p_frames) {
	ERR_FAIL_NULL_V(p_udata, 0);
	ERR_FAIL_NULL_V(p_data, 0);

	VideoStreamPlayer *vp = static_cast<VideoStreamPlayer *>(p_udata);

	int todo = MIN(vp->resampler.get_writer_space(), p_frames);

	float *wb = vp->resampler.get_write_buffer();
	int c = vp->resampler.get_channel_count();

	for (int i = 0; i < todo * c; i++) {
		wb[i] = p_data[i];
	}
	vp->resampler.write(todo);

	return todo;
}

void VideoStreamPlayer::_mix_audios(void *p_self) {
	ERR_FAIL_NULL(p_self);
	static_cast<VideoStreamPlayer *>(p_self)->_mix_audio();
}

// Called from audio thread
void VideoStreamPlayer::_mix_audio() {
	if (stream.is_null()) {
		return;
	}
	if (playback.is_null() || !playback->is_playing() || playback->is_paused()) {
		return;
	}

	AudioFrame *buffer = mix_buffer.ptrw();
	int buffer_size = mix_buffer.size();

	// Resample
	if (!mix(buffer, buffer_size)) {
		return;
	}

	AudioFrame vol = AudioFrame(volume, volume);

	int cc = AudioServer::get_singleton()->get_channel_count();

	if (cc == 1) {
		AudioFrame *target = AudioServer::get_singleton()->thread_get_channel_mix_buffer(bus_index, 0);
		ERR_FAIL_NULL(target);

		for (int j = 0; j < buffer_size; j++) {
			target[j] += buffer[j] * vol;
		}

	} else {
		AudioFrame *targets[4];

		for (int k = 0; k < cc; k++) {
			targets[k] = AudioServer::get_singleton()->thread_get_channel_mix_buffer(bus_index, k);
			ERR_FAIL_NULL(targets[k]);
		}

		for (int j = 0; j < buffer_size; j++) {
			AudioFrame frame = buffer[j] * vol;
			for (int k = 0; k < cc; k++) {
				targets[k][j] += frame;
			}
		}
	}
}

void VideoStreamPlayer::_notification(int p_notification) {
	switch (p_notification) {
		case NOTIFICATION_ENTER_TREE: {
			AudioServer::get_singleton()->add_mix_callback(_mix_audios, this);

			if (stream.is_valid() && autoplay && !Engine::get_singleton()->is_editor_hint()) {
				play();
			}
		} break;

		case NOTIFICATION_EXIT_TREE: {
			AudioServer::get_singleton()->remove_mix_callback(_mix_audios, this);
		} break;

		case NOTIFICATION_INTERNAL_PROCESS: {
			bus_index = AudioServer::get_singleton()->thread_find_bus_index(bus);

			if (stream.is_null() || paused || playback.is_null() || !playback->is_playing()) {
				return;
			}

			double audio_time = USEC_TO_SEC(OS::get_singleton()->get_ticks_usec());

			double delta = last_audio_time == 0 ? 0 : audio_time - last_audio_time;
			last_audio_time = audio_time;

			if (delta == 0) {
				return;
			}

			playback->update(delta); // playback->is_playing() returns false in the last video frame

			if (!playback->is_playing()) {
				if (loop) {
					play();
					return;
				}
				emit_signal(SceneStringName(finished));
			}
		} break;

		case NOTIFICATION_DRAW: {
			if (texture.is_null()) {
				return;
			}
			if (texture->get_width() == 0) {
				return;
			}

			Size2 s = expand ? get_size() : texture->get_size();
			draw_texture_rect(texture, Rect2(Point2(), s), false);
		} break;

		case NOTIFICATION_SUSPENDED:
		case NOTIFICATION_PAUSED: {
			if (is_playing() && !is_paused()) {
				paused_from_tree = true;
				if (playback.is_valid()) {
					playback->set_paused(true);
					set_process_internal(false);
				}
				last_audio_time = 0;
			}
		} break;

		case NOTIFICATION_UNSUSPENDED: {
			if (get_tree()->is_paused()) {
				break;
			}
			[[fallthrough]];
		}

		case NOTIFICATION_UNPAUSED: {
			if (paused_from_tree) {
				paused_from_tree = false;
				if (playback.is_valid()) {
					playback->set_paused(false);
					set_process_internal(true);
				}
				last_audio_time = 0;
			}
		} break;
	}
}

Size2 VideoStreamPlayer::get_minimum_size() const {
	if (!expand && texture.is_valid()) {
		return texture->get_size();
	} else {
		return Size2();
	}
}

void VideoStreamPlayer::set_expand(bool p_expand) {
	if (expand == p_expand) {
		return;
	}

	expand = p_expand;
	queue_redraw();
	update_minimum_size();
}

bool VideoStreamPlayer::has_expand() const {
	return expand;
}

void VideoStreamPlayer::set_loop(bool p_loop) {
	loop = p_loop;
}

bool VideoStreamPlayer::has_loop() const {
	return loop;
}

void VideoStreamPlayer::set_stream(const Ref<VideoStream> &p_stream) {
	stop();

	// Make sure to handle stream changes seamlessly, e.g. when done via
	// translation remapping.
	if (stream.is_valid()) {
		stream->disconnect_changed(callable_mp(this, &VideoStreamPlayer::set_stream));
	}

	AudioServer::get_singleton()->lock();
	mix_buffer.resize(AudioServer::get_singleton()->thread_get_mix_buffer_size());
	stream = p_stream;
	if (stream.is_valid()) {
		stream->set_audio_track(audio_track);
		playback = stream->instantiate_playback();
	} else {
		playback = Ref<VideoStreamPlayback>();
	}
	AudioServer::get_singleton()->unlock();

	if (stream.is_valid()) {
		stream->connect_changed(callable_mp(this, &VideoStreamPlayer::set_stream).bind(stream));
	}

	if (playback.is_valid()) {
		playback->set_paused(paused);
		texture = playback->get_texture();

		const int channels = playback->get_channels();

		AudioServer::get_singleton()->lock();
		if (channels > 0) {
			resampler.setup(channels, playback->get_mix_rate(), AudioServer::get_singleton()->get_mix_rate(), buffering_ms, 0);
		} else {
			resampler.clear();
		}
		AudioServer::get_singleton()->unlock();

		if (channels > 0) {
			playback->set_mix_callback(_audio_mix_callback, this);
		}

	} else {
		texture.unref();
		AudioServer::get_singleton()->lock();
		resampler.clear();
		AudioServer::get_singleton()->unlock();
	}

	queue_redraw();

	if (!expand) {
		update_minimum_size();
	}
}

Ref<VideoStream> VideoStreamPlayer::get_stream() const {
	return stream;
}

void VideoStreamPlayer::play() {
	ERR_FAIL_COND(!is_inside_tree());
	if (playback.is_null()) {
		return;
	}
	playback->stop();
	playback->play();
	set_process_internal(true);
	last_audio_time = 0;

	// We update the playback to render the first frame immediately.
	playback->update(0);

	if (!can_process()) {
		_notification(NOTIFICATION_PAUSED);
	}
}

void VideoStreamPlayer::stop() {
	if (!is_inside_tree()) {
		return;
	}
	if (playback.is_null()) {
		return;
	}

	playback->stop();
	resampler.flush();
	set_process_internal(false);
	last_audio_time = 0;
}

bool VideoStreamPlayer::is_playing() const {
	if (playback.is_null()) {
		return false;
	}

	return playback->is_playing();
}

void VideoStreamPlayer::set_paused(bool p_paused) {
	if (paused == p_paused) {
		return;
	}

	paused = p_paused;
	if (!p_paused && !can_process()) {
		paused_from_tree = true;
		return;
	} else if (p_paused && paused_from_tree) {
		paused_from_tree = false;
		return;
	}

	if (playback.is_valid()) {
		playback->set_paused(p_paused);
		set_process_internal(!p_paused);
	}
	last_audio_time = 0;
}

bool VideoStreamPlayer::is_paused() const {
	return paused;
}

void VideoStreamPlayer::set_buffering_msec(int p_msec) {
	buffering_ms = p_msec;
}

int VideoStreamPlayer::get_buffering_msec() const {
	return buffering_ms;
}

void VideoStreamPlayer::set_audio_track(int p_track) {
	audio_track = p_track;
	if (stream.is_valid()) {
		stream->set_audio_track(audio_track);
	}
	if (playback.is_valid()) {
		playback->set_audio_track(audio_track);
	}
}

int VideoStreamPlayer::get_audio_track() const {
	return audio_track;
}

void VideoStreamPlayer::set_volume(float p_vol) {
	volume = p_vol;
}

float VideoStreamPlayer::get_volume() const {
	return volume;
}

void VideoStreamPlayer::set_volume_db(float p_db) {
	if (p_db < -79) {
		set_volume(0);
	} else {
		set_volume(Math::db_to_linear(p_db));
	}
}

float VideoStreamPlayer::get_volume_db() const {
	if (volume == 0) {
		return -80;
	} else {
		return Math::linear_to_db(volume);
	}
}

String VideoStreamPlayer::get_stream_name() const {
	if (stream.is_null()) {
		return "<No Stream>";
	}
	return stream->get_name();
}

double VideoStreamPlayer::get_stream_length() const {
	if (playback.is_null()) {
		return 0;
	}
	return playback->get_length();
}

double VideoStreamPlayer::get_stream_position() const {
	if (playback.is_null()) {
		return 0;
	}
	return playback->get_playback_position();
}

void VideoStreamPlayer::set_stream_position(double p_position) {
	if (playback.is_valid()) {
		playback->seek(p_position);
	}
}

Ref<Texture2D> VideoStreamPlayer::get_video_texture() const {
	if (playback.is_valid()) {
		return playback->get_texture();
	}

	return Ref<Texture2D>();
}

void VideoStreamPlayer::set_autoplay(bool p_enable) {
	autoplay = p_enable;
}

bool VideoStreamPlayer::has_autoplay() const {
	return autoplay;
}

void VideoStreamPlayer::set_bus(const StringName &p_bus) {
	// If audio is active, must lock this.
	AudioServer::get_singleton()->lock();
	bus = p_bus;
	AudioServer::get_singleton()->unlock();
}

StringName VideoStreamPlayer::get_bus() const {
	for (int i = 0; i < AudioServer::get_singleton()->get_bus_count(); i++) {
		if (AudioServer::get_singleton()->get_bus_name(i) == bus) {
			return bus;
		}
	}
	return SceneStringName(Master);
}

void VideoStreamPlayer::_validate_property(PropertyInfo &p_property) const {
	if (p_property.name == "bus") {
		String options;
		for (int i = 0; i < AudioServer::get_singleton()->get_bus_count(); i++) {
			if (i > 0) {
				options += ",";
			}
			String name = AudioServer::get_singleton()->get_bus_name(i);
			options += name;
		}

		p_property.hint_string = options;
	}
}

void VideoStreamPlayer::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_stream", "stream"), &VideoStreamPlayer::set_stream);
	ClassDB::bind_method(D_METHOD("get_stream"), &VideoStreamPlayer::get_stream);

	ClassDB::bind_method(D_METHOD("play"), &VideoStreamPlayer::play);
	ClassDB::bind_method(D_METHOD("stop"), &VideoStreamPlayer::stop);

	ClassDB::bind_method(D_METHOD("is_playing"), &VideoStreamPlayer::is_playing);

	ClassDB::bind_method(D_METHOD("set_paused", "paused"), &VideoStreamPlayer::set_paused);
	ClassDB::bind_method(D_METHOD("is_paused"), &VideoStreamPlayer::is_paused);

	ClassDB::bind_method(D_METHOD("set_loop", "loop"), &VideoStreamPlayer::set_loop);
	ClassDB::bind_method(D_METHOD("has_loop"), &VideoStreamPlayer::has_loop);

	ClassDB::bind_method(D_METHOD("set_volume", "volume"), &VideoStreamPlayer::set_volume);
	ClassDB::bind_method(D_METHOD("get_volume"), &VideoStreamPlayer::get_volume);

	ClassDB::bind_method(D_METHOD("set_volume_db", "db"), &VideoStreamPlayer::set_volume_db);
	ClassDB::bind_method(D_METHOD("get_volume_db"), &VideoStreamPlayer::get_volume_db);

	ClassDB::bind_method(D_METHOD("set_audio_track", "track"), &VideoStreamPlayer::set_audio_track);
	ClassDB::bind_method(D_METHOD("get_audio_track"), &VideoStreamPlayer::get_audio_track);

	ClassDB::bind_method(D_METHOD("get_stream_name"), &VideoStreamPlayer::get_stream_name);
	ClassDB::bind_method(D_METHOD("get_stream_length"), &VideoStreamPlayer::get_stream_length);

	ClassDB::bind_method(D_METHOD("set_stream_position", "position"), &VideoStreamPlayer::set_stream_position);
	ClassDB::bind_method(D_METHOD("get_stream_position"), &VideoStreamPlayer::get_stream_position);

	ClassDB::bind_method(D_METHOD("set_autoplay", "enabled"), &VideoStreamPlayer::set_autoplay);
	ClassDB::bind_method(D_METHOD("has_autoplay"), &VideoStreamPlayer::has_autoplay);

	ClassDB::bind_method(D_METHOD("set_expand", "enable"), &VideoStreamPlayer::set_expand);
	ClassDB::bind_method(D_METHOD("has_expand"), &VideoStreamPlayer::has_expand);

	ClassDB::bind_method(D_METHOD("set_buffering_msec", "msec"), &VideoStreamPlayer::set_buffering_msec);
	ClassDB::bind_method(D_METHOD("get_buffering_msec"), &VideoStreamPlayer::get_buffering_msec);

	ClassDB::bind_method(D_METHOD("set_bus", "bus"), &VideoStreamPlayer::set_bus);
	ClassDB::bind_method(D_METHOD("get_bus"), &VideoStreamPlayer::get_bus);

	ClassDB::bind_method(D_METHOD("get_video_texture"), &VideoStreamPlayer::get_video_texture);

	ADD_SIGNAL(MethodInfo("finished"));

	ADD_PROPERTY(PropertyInfo(Variant::INT, "audio_track", PROPERTY_HINT_RANGE, "0,128,1"), "set_audio_track", "get_audio_track");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "stream", PROPERTY_HINT_RESOURCE_TYPE, "VideoStream"), "set_stream", "get_stream");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "volume_db", PROPERTY_HINT_RANGE, "-80,24,0.01,suffix:dB"), "set_volume_db", "get_volume_db");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "volume", PROPERTY_HINT_RANGE, "0,15,0.01,exp", PROPERTY_USAGE_NONE), "set_volume", "get_volume");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "autoplay"), "set_autoplay", "has_autoplay");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "paused"), "set_paused", "is_paused");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "expand"), "set_expand", "has_expand");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "loop"), "set_loop", "has_loop");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "buffering_msec", PROPERTY_HINT_RANGE, "10,1000,suffix:ms"), "set_buffering_msec", "get_buffering_msec");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "stream_position", PROPERTY_HINT_RANGE, "0,1280000,0.1", PROPERTY_USAGE_NONE), "set_stream_position", "get_stream_position");

	ADD_PROPERTY(PropertyInfo(Variant::STRING_NAME, "bus", PROPERTY_HINT_ENUM, ""), "set_bus", "get_bus");
}

VideoStreamPlayer::VideoStreamPlayer() {}

VideoStreamPlayer::~VideoStreamPlayer() {
	resampler.clear(); // Not necessary here, but make in consistent with other "stream_player" classes.
}

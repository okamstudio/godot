/**************************************************************************/
/*  audio_stream_polyphonic.h                                             */
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

#pragma once

#include "core/templates/local_vector.h"
#include "scene/scene_string_names.h"
#include "servers/audio/audio_stream.h"
#include "servers/audio_server.h"

class AudioStreamPolyphonic : public AudioStream {
	GDCLASS(AudioStreamPolyphonic, AudioStream)
	int polyphony = 32;

	AudioServer::PlaybackType playback_type;

	static void _bind_methods();

public:
	virtual Ref<AudioStreamPlayback> instantiate_playback() override;
	virtual String get_stream_name() const override;
	virtual bool is_monophonic() const override;

	void set_polyphony(int p_voices);
	int get_polyphony() const;

	virtual bool is_meta_stream() const override { return true; }

	AudioStreamPolyphonic();
};

class AudioStreamPlaybackPolyphonic : public AudioStreamPlayback {
	GDCLASS(AudioStreamPlaybackPolyphonic, AudioStreamPlayback)

	constexpr static uint32_t INTERNAL_BUFFER_LEN = 128;

	struct Stream {
		SafeFlag active;
		SafeFlag pending_play;
		SafeFlag finish_request;
		float play_offset = 0;
		float pitch_scale = 1.0;
		Ref<AudioStream> stream;
		Ref<AudioStreamPlayback> stream_playback;
		float prev_volume_db = 0;
		float volume_db = 0;
		uint32_t id = 0;

		Stream() :
				active(false), pending_play(false), finish_request(false) {}
	};

	LocalVector<Stream> streams;
	AudioFrame internal_buffer[INTERNAL_BUFFER_LEN];

	bool active = false;
	uint32_t id_counter = 1;

	bool _is_sample = false;
	Ref<AudioSamplePlayback> sample_playback;

	_FORCE_INLINE_ Stream *_find_stream(int64_t p_id);
	_FORCE_INLINE_ const Stream *_find_stream(int64_t p_id) const;

	friend class AudioStreamPolyphonic;

protected:
	static void _bind_methods();

public:
	typedef int64_t ID;
	enum {
		INVALID_ID = -1
	};

	virtual void start(double p_from_pos = 0.0) override;
	virtual void stop() override;
	virtual bool is_playing() const override;

	virtual int get_loop_count() const override; //times it looped

	virtual double get_playback_position() const override;
	virtual void seek(double p_time) override;

	virtual void tag_used_streams() override;

	virtual int mix(AudioFrame *p_buffer, float p_rate_scale, int p_frames) override;

	ID play_stream(const Ref<AudioStream> &p_stream, float p_from_offset = 0, float p_volume_db = 0, float p_pitch_scale = 1.0, AudioServer::PlaybackType p_playback_type = AudioServer::PlaybackType::PLAYBACK_TYPE_DEFAULT, const StringName &p_bus = SceneStringName(Master));
	void set_stream_volume(ID p_stream_id, float p_volume_db);
	void set_stream_pitch_scale(ID p_stream_id, float p_pitch_scale);
	bool is_stream_playing(ID p_stream_id) const;
	void stop_stream(ID p_stream_id);

	virtual void set_is_sample(bool p_is_sample) override;
	virtual bool get_is_sample() const override;
	virtual Ref<AudioSamplePlayback> get_sample_playback() const override;
	virtual void set_sample_playback(const Ref<AudioSamplePlayback> &p_playback) override;

private:
#ifndef DISABLE_DEPRECATED
	ID _play_stream_bind_compat_91382(const Ref<AudioStream> &p_stream, float p_from_offset = 0, float p_volume_db = 0, float p_pitch_scale = 1.0);
	static void _bind_compatibility_methods();
#endif // DISABLE_DEPRECATED

public:
	AudioStreamPlaybackPolyphonic();
};

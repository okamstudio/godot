/**************************************************************************/
/*  audio_stream_player_internal.h                                        */
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

#include "core/object/ref_counted.h"
#include "core/templates/safe_refcount.h"
#include "servers/audio_server.h"

class AudioStream;
class AudioStreamPlayback;
class AudioSamplePlayback;
class Node;

class AudioStreamPlayerInternal : public Object {
	GDCLASS(AudioStreamPlayerInternal, Object);

private:
	struct ParameterData {
		StringName path;
		Variant value;
	};

	static inline const String PARAM_PREFIX = "parameters/";

	Node *node = nullptr;
	Callable play_callable;
	Callable stop_callable;
	bool physical = false;
	AudioServer::PlaybackType playback_type = AudioServer::PlaybackType::PLAYBACK_TYPE_DEFAULT;

	HashMap<StringName, ParameterData> playback_parameters;

	void _set_process(bool p_enabled);
	void _update_stream_parameters();

	_FORCE_INLINE_ bool _is_sample() {
		return (AudioServer::get_singleton()->get_default_playback_type() == AudioServer::PlaybackType::PLAYBACK_TYPE_SAMPLE && get_playback_type() == AudioServer::PlaybackType::PLAYBACK_TYPE_DEFAULT) || get_playback_type() == AudioServer::PlaybackType::PLAYBACK_TYPE_SAMPLE;
	}

public:
	Vector<Ref<AudioStreamPlayback>> stream_playbacks;
	Ref<AudioStream> stream;

	SafeFlag active;

	float pitch_scale = 1.0;
	float volume_db = 0.0;
	bool autoplay = false;
	StringName bus;
	int max_polyphony = 1;

	void process();
	void ensure_playback_limit();

	void notification(int p_what);
	void validate_property(PropertyInfo &p_property) const;
	bool set(const StringName &p_name, const Variant &p_value);
	bool get(const StringName &p_name, Variant &r_ret) const;
	void get_property_list(List<PropertyInfo> *p_list) const;

	void set_stream(Ref<AudioStream> p_stream);
	void set_pitch_scale(float p_pitch_scale);
	void set_max_polyphony(int p_max_polyphony);

	StringName get_bus() const;

	Ref<AudioStreamPlayback> play_basic();
	void seek(float p_seconds);
	void stop_basic();
	bool is_playing() const;
	float get_playback_position();

	void set_playing(bool p_enable);
	bool is_active() const;

	void set_stream_paused(bool p_pause);
	bool get_stream_paused() const;

	bool has_stream_playback();
	Ref<AudioStreamPlayback> get_stream_playback();

	void set_playback_type(AudioServer::PlaybackType p_playback_type);
	AudioServer::PlaybackType get_playback_type() const;

	AudioStreamPlayerInternal(Node *p_node, const Callable &p_play_callable, const Callable &p_stop_callable, bool p_physical);
};

/**************************************************************************/
/*  audio_stream_player.cpp                                               */
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

#include "audio_stream_player.h"
#include "audio_stream_player.compat.inc"

#include "scene/audio/audio_stream_player_internal.h"
#include "servers/audio/audio_stream.h"

void AudioStreamPlayer::_notification(int p_what) {
	internal->notification(p_what);
}

void AudioStreamPlayer::set_stream(Ref<AudioStream> p_stream) {
	internal->set_stream(p_stream);
}

bool AudioStreamPlayer::_set(const StringName &p_name, const Variant &p_value) {
	return internal->set(p_name, p_value);
}

bool AudioStreamPlayer::_get(const StringName &p_name, Variant &r_ret) const {
	return internal->get(p_name, r_ret);
}

void AudioStreamPlayer::_get_property_list(List<PropertyInfo> *p_list) const {
	internal->get_property_list(p_list);
}

Ref<AudioStream> AudioStreamPlayer::get_stream() const {
	return internal->stream;
}

void AudioStreamPlayer::set_volume_db(float p_volume) {
	ERR_FAIL_COND_MSG(Math::is_nan(p_volume), "Volume can't be set to NaN.");
	internal->volume_db = p_volume;

	Vector<AudioFrame> volume_vector = _get_volume_vector();
	for (Ref<AudioStreamPlayback> &playback : internal->stream_playbacks) {
		AudioServer::get_singleton()->set_playback_all_bus_volumes_linear(playback, volume_vector);
	}
}

float AudioStreamPlayer::get_volume_db() const {
	return internal->volume_db;
}

void AudioStreamPlayer::set_pitch_scale(float p_pitch_scale) {
	internal->set_pitch_scale(p_pitch_scale);
}

float AudioStreamPlayer::get_pitch_scale() const {
	return internal->pitch_scale;
}

void AudioStreamPlayer::set_max_polyphony(int p_max_polyphony) {
	internal->set_max_polyphony(p_max_polyphony);
}

int AudioStreamPlayer::get_max_polyphony() const {
	return internal->max_polyphony;
}

void AudioStreamPlayer::play(float p_from_pos) {
	Ref<AudioStreamPlayback> stream_playback = internal->play_basic();
	if (stream_playback.is_null()) {
		return;
	}
	AudioServer::get_singleton()->start_playback_stream(stream_playback, internal->bus, _get_volume_vector(), p_from_pos, internal->pitch_scale);
	internal->ensure_playback_limit();

	// Sample handling.
	if (stream_playback->get_is_sample() && stream_playback->get_sample_playback().is_valid()) {
		Ref<AudioSamplePlayback> sample_playback = stream_playback->get_sample_playback();
		sample_playback->offset = p_from_pos;
		sample_playback->volume_vector = _get_volume_vector();
		sample_playback->bus = get_bus();

		AudioServer::get_singleton()->start_sample_playback(sample_playback);
	}
}

void AudioStreamPlayer::seek(float p_seconds) {
	internal->seek(p_seconds);
}

void AudioStreamPlayer::stop() {
	internal->stop_basic();
}

bool AudioStreamPlayer::is_playing() const {
	return internal->is_playing();
}

float AudioStreamPlayer::get_playback_position() {
	return internal->get_playback_position();
}

void AudioStreamPlayer::set_bus(const StringName &p_bus) {
	internal->bus = p_bus;
	for (const Ref<AudioStreamPlayback> &playback : internal->stream_playbacks) {
		AudioServer::get_singleton()->set_playback_bus_exclusive(playback, p_bus, _get_volume_vector());
	}
}

StringName AudioStreamPlayer::get_bus() const {
	return internal->get_bus();
}

void AudioStreamPlayer::set_autoplay(bool p_enable) {
	internal->autoplay = p_enable;
}

bool AudioStreamPlayer::is_autoplay_enabled() const {
	return internal->autoplay;
}

void AudioStreamPlayer::set_mix_target(MixTarget p_target) {
	mix_target = p_target;
}

AudioStreamPlayer::MixTarget AudioStreamPlayer::get_mix_target() const {
	return mix_target;
}

void AudioStreamPlayer::_set_playing(bool p_enable) {
	internal->set_playing(p_enable);
}

void AudioStreamPlayer::set_stream_paused(bool p_pause) {
	internal->set_stream_paused(p_pause);
}

bool AudioStreamPlayer::get_stream_paused() const {
	return internal->get_stream_paused();
}

Vector<AudioFrame> AudioStreamPlayer::_get_volume_vector() {
	Vector<AudioFrame> volume_vector;
	// We need at most four stereo pairs (for 7.1 systems).
	volume_vector.resize(4);

	// Initialize the volume vector to zero.
	for (AudioFrame &channel_volume_db : volume_vector) {
		channel_volume_db = AudioFrame(0, 0);
	}

	float volume_linear = Math::db_to_linear(internal->volume_db);

	// Set the volume vector up according to the speaker mode and mix target.
	// TODO do we need to scale the volume down when we output to more channels?
	if (AudioServer::get_singleton()->get_speaker_mode() == AudioServer::SPEAKER_MODE_STEREO) {
		volume_vector.write[0] = AudioFrame(volume_linear, volume_linear);
	} else {
		switch (mix_target) {
			case MIX_TARGET_STEREO: {
				volume_vector.write[0] = AudioFrame(volume_linear, volume_linear);
			} break;
			case MIX_TARGET_SURROUND: {
				// TODO Make sure this is right.
				volume_vector.write[0] = AudioFrame(volume_linear, volume_linear);
				volume_vector.write[1] = AudioFrame(volume_linear, /* LFE= */ 1.0f);
				volume_vector.write[2] = AudioFrame(volume_linear, volume_linear);
				volume_vector.write[3] = AudioFrame(volume_linear, volume_linear);
			} break;
			case MIX_TARGET_CENTER: {
				// TODO Make sure this is right.
				volume_vector.write[1] = AudioFrame(volume_linear, /* LFE= */ 1.0f);
			} break;
		}
	}
	return volume_vector;
}

void AudioStreamPlayer::_validate_property(PropertyInfo &p_property) const {
	internal->validate_property(p_property);
}

bool AudioStreamPlayer::has_stream_playback() {
	return internal->has_stream_playback();
}

Ref<AudioStreamPlayback> AudioStreamPlayer::get_stream_playback() {
	return internal->get_stream_playback();
}

AudioServer::PlaybackType AudioStreamPlayer::get_playback_type() const {
	return internal->get_playback_type();
}

void AudioStreamPlayer::set_playback_type(AudioServer::PlaybackType p_playback_type) {
	internal->set_playback_type(p_playback_type);
}

void AudioStreamPlayer::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_stream", "stream"), &AudioStreamPlayer::set_stream);
	ClassDB::bind_method(D_METHOD("get_stream"), &AudioStreamPlayer::get_stream);

	ClassDB::bind_method(D_METHOD("set_volume_db", "volume_db"), &AudioStreamPlayer::set_volume_db);
	ClassDB::bind_method(D_METHOD("get_volume_db"), &AudioStreamPlayer::get_volume_db);

	ClassDB::bind_method(D_METHOD("set_pitch_scale", "pitch_scale"), &AudioStreamPlayer::set_pitch_scale);
	ClassDB::bind_method(D_METHOD("get_pitch_scale"), &AudioStreamPlayer::get_pitch_scale);

	ClassDB::bind_method(D_METHOD("play", "from_position"), &AudioStreamPlayer::play, DEFVAL(0.0));
	ClassDB::bind_method(D_METHOD("seek", "to_position"), &AudioStreamPlayer::seek);
	ClassDB::bind_method(D_METHOD("stop"), &AudioStreamPlayer::stop);

	ClassDB::bind_method(D_METHOD("is_playing"), &AudioStreamPlayer::is_playing);
	ClassDB::bind_method(D_METHOD("get_playback_position"), &AudioStreamPlayer::get_playback_position);

	ClassDB::bind_method(D_METHOD("set_bus", "bus"), &AudioStreamPlayer::set_bus);
	ClassDB::bind_method(D_METHOD("get_bus"), &AudioStreamPlayer::get_bus);

	ClassDB::bind_method(D_METHOD("set_autoplay", "enable"), &AudioStreamPlayer::set_autoplay);
	ClassDB::bind_method(D_METHOD("is_autoplay_enabled"), &AudioStreamPlayer::is_autoplay_enabled);

	ClassDB::bind_method(D_METHOD("set_mix_target", "mix_target"), &AudioStreamPlayer::set_mix_target);
	ClassDB::bind_method(D_METHOD("get_mix_target"), &AudioStreamPlayer::get_mix_target);

	ClassDB::bind_method(D_METHOD("set_playing", "enable"), &AudioStreamPlayer::_set_playing);

	ClassDB::bind_method(D_METHOD("set_stream_paused", "pause"), &AudioStreamPlayer::set_stream_paused);
	ClassDB::bind_method(D_METHOD("get_stream_paused"), &AudioStreamPlayer::get_stream_paused);

	ClassDB::bind_method(D_METHOD("set_max_polyphony", "max_polyphony"), &AudioStreamPlayer::set_max_polyphony);
	ClassDB::bind_method(D_METHOD("get_max_polyphony"), &AudioStreamPlayer::get_max_polyphony);

	ClassDB::bind_method(D_METHOD("has_stream_playback"), &AudioStreamPlayer::has_stream_playback);
	ClassDB::bind_method(D_METHOD("get_stream_playback"), &AudioStreamPlayer::get_stream_playback);

	ClassDB::bind_method(D_METHOD("set_playback_type", "playback_type"), &AudioStreamPlayer::set_playback_type);
	ClassDB::bind_method(D_METHOD("get_playback_type"), &AudioStreamPlayer::get_playback_type);

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "stream", PROPERTY_HINT_RESOURCE_TYPE, "AudioStream"), "set_stream", "get_stream");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "volume_db", PROPERTY_HINT_RANGE, "-80,24,suffix:dB"), "set_volume_db", "get_volume_db");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "pitch_scale", PROPERTY_HINT_RANGE, "0.01,4,0.01,or_greater"), "set_pitch_scale", "get_pitch_scale");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "playing", PROPERTY_HINT_ONESHOT, "", PROPERTY_USAGE_EDITOR), "set_playing", "is_playing");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "autoplay"), "set_autoplay", "is_autoplay_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "stream_paused", PROPERTY_HINT_NONE, ""), "set_stream_paused", "get_stream_paused");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "mix_target", PROPERTY_HINT_ENUM, "Stereo,Surround,Center"), "set_mix_target", "get_mix_target");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "max_polyphony", PROPERTY_HINT_NONE, ""), "set_max_polyphony", "get_max_polyphony");
	ADD_PROPERTY(PropertyInfo(Variant::STRING_NAME, "bus", PROPERTY_HINT_ENUM, ""), "set_bus", "get_bus");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "playback_type", PROPERTY_HINT_ENUM, "Default,Stream,Sample"), "set_playback_type", "get_playback_type");

	ADD_SIGNAL(MethodInfo("finished"));

	BIND_ENUM_CONSTANT(MIX_TARGET_STEREO);
	BIND_ENUM_CONSTANT(MIX_TARGET_SURROUND);
	BIND_ENUM_CONSTANT(MIX_TARGET_CENTER);
}

AudioStreamPlayer::AudioStreamPlayer() {
	internal = memnew(AudioStreamPlayerInternal(this, callable_mp(this, &AudioStreamPlayer::play), callable_mp(this, &AudioStreamPlayer::stop), false));
}

AudioStreamPlayer::~AudioStreamPlayer() {
	memdelete(internal);
}

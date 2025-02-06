/**************************************************************************/
/*  audio_stream_player_internal.cpp                                      */
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

#include "audio_stream_player_internal.h"

#include "scene/main/node.h"
#include "servers/audio/audio_stream.h"

void AudioStreamPlayerInternal::_set_process(bool p_enabled) {
	if (physical) {
		node->set_physics_process_internal(p_enabled);
	} else {
		node->set_process_internal(p_enabled);
	}
}

void AudioStreamPlayerInternal::_update_stream_parameters() {
	if (stream.is_null()) {
		return;
	}

	List<AudioStream::Parameter> parameters;
	stream->get_parameter_list(&parameters);
	for (const AudioStream::Parameter &K : parameters) {
		const PropertyInfo &pi = K.property;
		StringName key = PARAM_PREFIX + pi.name;
		if (!playback_parameters.has(key)) {
			ParameterData pd;
			pd.path = pi.name;
			pd.value = K.default_value;
			playback_parameters.insert(key, pd);
		}
	}
}

void AudioStreamPlayerInternal::process() {
	LocalVector<Ref<AudioStreamPlayback>> playbacks_to_remove;
	for (Ref<AudioStreamPlayback> &playback : stream_playbacks) {
		if (playback.is_valid() && !AudioServer::get_singleton()->is_playback_active(playback) && !AudioServer::get_singleton()->is_playback_paused(playback)) {
			playbacks_to_remove.push_back(playback);
		}
	}
	// Now go through and remove playbacks that have finished. Removing elements from a LocalVector in a range based for is asking for trouble.
	for (Ref<AudioStreamPlayback> &playback : playbacks_to_remove) {
		stream_playbacks.erase(playback);
	}
	if (!playbacks_to_remove.is_empty() && stream_playbacks.is_empty()) {
		// This node is no longer actively playing audio.
		active.clear();
		_set_process(false);
	}
	if (!playbacks_to_remove.is_empty()) {
		node->emit_signal(SceneStringName(finished));
	}
}

void AudioStreamPlayerInternal::ensure_playback_limit() {
	while (stream_playbacks.size() > (uint)MAX(max_polyphony, 0)) {
		AudioServer::get_singleton()->stop_playback_stream(stream_playbacks[0]);
		stream_playbacks.remove_at(0);
	}
}

void AudioStreamPlayerInternal::notification(int p_what) {
	switch (p_what) {
		case Node::NOTIFICATION_ENTER_TREE: {
			if (autoplay && !Engine::get_singleton()->is_editor_hint()) {
				play_callable.call(0.0);
			}
			set_stream_paused(!node->can_process());
		} break;

		case Node::NOTIFICATION_EXIT_TREE: {
			set_stream_paused(true);
		} break;

		case Node::NOTIFICATION_INTERNAL_PROCESS: {
			process();
		} break;

		case Node::NOTIFICATION_PREDELETE: {
			for (Ref<AudioStreamPlayback> &playback : stream_playbacks) {
				AudioServer::get_singleton()->stop_playback_stream(playback);
			}
			stream_playbacks.clear();
		} break;

		case Node::NOTIFICATION_SUSPENDED:
		case Node::NOTIFICATION_PAUSED: {
			if (!node->can_process()) {
				// Node can't process so we start fading out to silence
				set_stream_paused(true);
			}
		} break;

		case Node::NOTIFICATION_UNSUSPENDED: {
			if (node->get_tree()->is_paused()) {
				break;
			}
			[[fallthrough]];
		}

		case Node::NOTIFICATION_UNPAUSED: {
			set_stream_paused(false);
		} break;
	}
}

Ref<AudioStreamPlayback> AudioStreamPlayerInternal::play_basic() {
	Ref<AudioStreamPlayback> stream_playback;
	if (stream.is_null()) {
		return stream_playback;
	}
	ERR_FAIL_COND_V_MSG(!node->is_inside_tree(), stream_playback, "Playback can only happen when a node is inside the scene tree");
	if (stream->is_monophonic() && is_playing()) {
		stop_callable.call();
	}
	stream_playback = stream->instantiate_playback();
	ERR_FAIL_COND_V_MSG(stream_playback.is_null(), stream_playback, "Failed to instantiate playback.");

	for (const KeyValue<StringName, ParameterData> &K : playback_parameters) {
		stream_playback->set_parameter(K.value.path, K.value.value);
	}

	// Sample handling.
	if (_is_sample()) {
		if (stream->can_be_sampled()) {
			stream_playback->set_is_sample(true);
			if (stream_playback->get_is_sample() && stream_playback->get_sample_playback().is_null()) {
				if (!AudioServer::get_singleton()->is_stream_registered_as_sample(stream)) {
					AudioServer::get_singleton()->register_stream_as_sample(stream);
				}
				Ref<AudioSamplePlayback> sample_playback;
				sample_playback.instantiate();
				sample_playback->stream = stream;
				sample_playback->pitch_scale = pitch_scale;
				stream_playback->set_sample_playback(sample_playback);
			}
		} else if (!stream->is_meta_stream()) {
			WARN_PRINT(vformat(R"(%s is trying to play a sample from a stream that cannot be sampled.)", node->get_path()));
		}
	}

	stream_playbacks.push_back(stream_playback);
	active.set();
	_set_process(true);
	return stream_playback;
}

void AudioStreamPlayerInternal::set_stream_paused(bool p_pause) {
	// TODO this does not have perfect recall, fix that maybe? If there are zero playbacks registered with the AudioServer, this bool isn't persisted.
	for (Ref<AudioStreamPlayback> &playback : stream_playbacks) {
		AudioServer::get_singleton()->set_playback_paused(playback, p_pause);
		if (_is_sample() && playback->get_sample_playback().is_valid()) {
			AudioServer::get_singleton()->set_sample_playback_pause(playback->get_sample_playback(), p_pause);
		}
	}
}

bool AudioStreamPlayerInternal::get_stream_paused() const {
	// There's currently no way to pause some playback streams but not others. Check the first and don't bother looking at the rest.
	if (!stream_playbacks.is_empty()) {
		return AudioServer::get_singleton()->is_playback_paused(stream_playbacks[0]);
	}
	return false;
}

void AudioStreamPlayerInternal::validate_property(PropertyInfo &p_property) const {
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

bool AudioStreamPlayerInternal::set(const StringName &p_name, const Variant &p_value) {
	ParameterData *pd = playback_parameters.getptr(p_name);
	if (!pd) {
		return false;
	}
	pd->value = p_value;
	for (Ref<AudioStreamPlayback> &playback : stream_playbacks) {
		playback->set_parameter(pd->path, pd->value);
	}
	return true;
}

bool AudioStreamPlayerInternal::get(const StringName &p_name, Variant &r_ret) const {
	const ParameterData *pd = playback_parameters.getptr(p_name);
	if (!pd) {
		return false;
	}
	r_ret = pd->value;
	return true;
}

void AudioStreamPlayerInternal::get_property_list(List<PropertyInfo> *p_list) const {
	if (stream.is_null()) {
		return;
	}
	List<AudioStream::Parameter> parameters;
	stream->get_parameter_list(&parameters);
	for (const AudioStream::Parameter &K : parameters) {
		PropertyInfo pi = K.property;
		pi.name = PARAM_PREFIX + pi.name;

		const ParameterData *pd = playback_parameters.getptr(pi.name);
		if (pd && pd->value == K.default_value) {
			pi.usage &= ~PROPERTY_USAGE_STORAGE;
		}

		p_list->push_back(pi);
	}
}

void AudioStreamPlayerInternal::set_stream(Ref<AudioStream> p_stream) {
	if (stream.is_valid()) {
		stream->disconnect(SNAME("parameter_list_changed"), callable_mp(this, &AudioStreamPlayerInternal::_update_stream_parameters));
	}
	stop_callable.call();
	stream = p_stream;
	_update_stream_parameters();
	if (stream.is_valid()) {
		stream->connect(SNAME("parameter_list_changed"), callable_mp(this, &AudioStreamPlayerInternal::_update_stream_parameters));
	}
	node->notify_property_list_changed();
}

void AudioStreamPlayerInternal::seek(float p_seconds) {
	if (is_playing()) {
		stop_callable.call();
		play_callable.call(p_seconds);
	}
}

void AudioStreamPlayerInternal::stop_basic() {
	for (Ref<AudioStreamPlayback> &playback : stream_playbacks) {
		AudioServer::get_singleton()->stop_playback_stream(playback);
	}
	stream_playbacks.clear();

	active.clear();
	_set_process(false);
}

bool AudioStreamPlayerInternal::is_playing() const {
	for (const Ref<AudioStreamPlayback> &playback : stream_playbacks) {
		if (AudioServer::get_singleton()->is_playback_active(playback)) {
			return true;
		}
	}
	return false;
}

float AudioStreamPlayerInternal::get_playback_position() {
	// Return the playback position of the most recently started playback stream.
	if (!stream_playbacks.is_empty()) {
		return AudioServer::get_singleton()->get_playback_position(stream_playbacks[stream_playbacks.size() - 1]);
	}
	return 0;
}

void AudioStreamPlayerInternal::set_playing(bool p_enable) {
	if (p_enable) {
		play_callable.call(0.0);
	} else {
		stop_callable.call();
	}
}

bool AudioStreamPlayerInternal::is_active() const {
	return active.is_set();
}

void AudioStreamPlayerInternal::set_pitch_scale(float p_pitch_scale) {
	ERR_FAIL_COND(p_pitch_scale <= 0.0);
	pitch_scale = p_pitch_scale;

	for (Ref<AudioStreamPlayback> &playback : stream_playbacks) {
		AudioServer::get_singleton()->set_playback_pitch_scale(playback, pitch_scale);
	}
}

void AudioStreamPlayerInternal::set_max_polyphony(int p_max_polyphony) {
	if (p_max_polyphony > 0) {
		max_polyphony = p_max_polyphony;
	}
}

bool AudioStreamPlayerInternal::has_stream_playback() {
	return !stream_playbacks.is_empty();
}

Ref<AudioStreamPlayback> AudioStreamPlayerInternal::get_stream_playback() {
	ERR_FAIL_COND_V_MSG(stream_playbacks.is_empty(), Ref<AudioStreamPlayback>(), "Player is inactive. Call play() before requesting get_stream_playback().");
	return stream_playbacks[stream_playbacks.size() - 1];
}

void AudioStreamPlayerInternal::set_playback_type(AudioServer::PlaybackType p_playback_type) {
	playback_type = p_playback_type;
}

AudioServer::PlaybackType AudioStreamPlayerInternal::get_playback_type() const {
	return playback_type;
}

StringName AudioStreamPlayerInternal::get_bus() const {
	const String bus_name = bus;
	for (int i = 0; i < AudioServer::get_singleton()->get_bus_count(); i++) {
		if (AudioServer::get_singleton()->get_bus_name(i) == bus_name) {
			return bus;
		}
	}
	return SceneStringName(Master);
}

AudioStreamPlayerInternal::AudioStreamPlayerInternal(Node *p_node, const Callable &p_play_callable, const Callable &p_stop_callable, bool p_physical) {
	node = p_node;
	play_callable = p_play_callable;
	stop_callable = p_stop_callable;
	physical = p_physical;
	bus = SceneStringName(Master);

	AudioServer::get_singleton()->connect("bus_layout_changed", callable_mp((Object *)node, &Object::notify_property_list_changed));
	AudioServer::get_singleton()->connect("bus_renamed", callable_mp((Object *)node, &Object::notify_property_list_changed).unbind(3));
}

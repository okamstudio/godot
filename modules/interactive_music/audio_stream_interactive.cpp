/**************************************************************************/
/*  audio_stream_interactive.cpp                                          */
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

#include "audio_stream_interactive.h"

#include "core/math/math_funcs.h"

AudioStreamInteractive::AudioStreamInteractive() {
}

Ref<AudioStreamPlayback> AudioStreamInteractive::instantiate_playback() {
	Ref<AudioStreamPlaybackInteractive> playback_transitioner;
	playback_transitioner.instantiate();
	playback_transitioner->stream = Ref<AudioStreamInteractive>(this);
	return playback_transitioner;
}

String AudioStreamInteractive::get_stream_name() const {
	return "Transitioner";
}

void AudioStreamInteractive::set_clip_count(int p_count) {
	ERR_FAIL_COND(p_count < 0 || p_count > MAX_CLIPS);

	AudioServer::get_singleton()->lock();

	if (p_count < clip_count) {
		// Removing should stop players.
		version++;
	}

#ifdef TOOLS_ENABLED
	stream_name_cache = "";
	if (p_count < clip_count) {
		for (int i = 0; i < clip_count; i++) {
			if (clips[i].auto_advance_next_clip >= p_count) {
				clips[i].auto_advance_next_clip = 0;
				clips[i].auto_advance = AUTO_ADVANCE_DISABLED;
			}
		}

		for (KeyValue<TransitionKey, Transition> &K : transition_map) {
			if (K.value.filler_clip >= p_count) {
				K.value.use_filler_clip = false;
				K.value.filler_clip = 0;
			}
		}
		if (initial_clip >= p_count) {
			initial_clip = 0;
		}
	}
#endif
	clip_count = p_count;
	AudioServer::get_singleton()->unlock();

	notify_property_list_changed();
	emit_signal(SNAME("parameter_list_changed"));
}

void AudioStreamInteractive::set_initial_clip(int p_clip) {
	ERR_FAIL_INDEX(p_clip, clip_count);
	initial_clip = p_clip;
}

int AudioStreamInteractive::get_initial_clip() const {
	return initial_clip;
}

int AudioStreamInteractive::get_clip_count() const {
	return clip_count;
}

void AudioStreamInteractive::set_clip_name(int p_clip, const StringName &p_name) {
	ERR_FAIL_INDEX(p_clip, MAX_CLIPS);
	clips[p_clip].name = p_name;
}

StringName AudioStreamInteractive::get_clip_name(int p_clip) const {
	ERR_FAIL_COND_V(p_clip < -1 || p_clip >= MAX_CLIPS, StringName());
	if (p_clip == CLIP_ANY) {
		return RTR("All Clips");
	}
	return clips[p_clip].name;
}

void AudioStreamInteractive::set_clip_stream(int p_clip, const Ref<AudioStream> &p_stream) {
	ERR_FAIL_INDEX(p_clip, MAX_CLIPS);
	AudioServer::get_singleton()->lock();
	if (clips[p_clip].stream.is_valid()) {
		version++;
	}
	clips[p_clip].stream = p_stream;
	AudioServer::get_singleton()->unlock();
#ifdef TOOLS_ENABLED
	if (Engine::get_singleton()->is_editor_hint()) {
		if (clips[p_clip].name == StringName() && p_stream.is_valid()) {
			String n;
			if (!clips[p_clip].stream->get_name().is_empty()) {
				n = clips[p_clip].stream->get_name().replace(",", " ");
			} else if (clips[p_clip].stream->get_path().is_resource_file()) {
				n = clips[p_clip].stream->get_path().get_file().get_basename().replace(",", " ");
				n = n.capitalize();
			}

			if (n != "") {
				clips[p_clip].name = n;
			}
		}
	}
#endif

#ifdef TOOLS_ENABLED
	stream_name_cache = "";
	notify_property_list_changed(); // Hints change if stream changes.
	emit_signal(SNAME("parameter_list_changed"));
#endif
}

Ref<AudioStream> AudioStreamInteractive::get_clip_stream(int p_clip) const {
	ERR_FAIL_INDEX_V(p_clip, MAX_CLIPS, Ref<AudioStream>());
	return clips[p_clip].stream;
}

void AudioStreamInteractive::set_clip_auto_advance(int p_clip, AutoAdvanceMode p_mode) {
	ERR_FAIL_INDEX(p_clip, MAX_CLIPS);
	ERR_FAIL_INDEX(p_mode, 3);
	clips[p_clip].auto_advance = p_mode;
	notify_property_list_changed();
}

AudioStreamInteractive::AutoAdvanceMode AudioStreamInteractive::get_clip_auto_advance(int p_clip) const {
	ERR_FAIL_INDEX_V(p_clip, MAX_CLIPS, AUTO_ADVANCE_DISABLED);
	return clips[p_clip].auto_advance;
}

void AudioStreamInteractive::set_clip_auto_advance_next_clip(int p_clip, int p_index) {
	ERR_FAIL_INDEX(p_clip, MAX_CLIPS);
	clips[p_clip].auto_advance_next_clip = p_index;
}

int AudioStreamInteractive::get_clip_auto_advance_next_clip(int p_clip) const {
	ERR_FAIL_INDEX_V(p_clip, MAX_CLIPS, -1);
	return clips[p_clip].auto_advance_next_clip;
}

// TRANSITIONS

void AudioStreamInteractive::_set_transitions(const Dictionary &p_transitions) {
	List<Variant> keys;
	p_transitions.get_key_list(&keys);
	for (const Variant &K : keys) {
		Vector2i k = K;
		Dictionary data = p_transitions[K];
		ERR_CONTINUE(!data.has("from_time"));
		ERR_CONTINUE(!data.has("to_time"));
		ERR_CONTINUE(!data.has("fade_mode"));
		ERR_CONTINUE(!data.has("fade_beats"));
		bool use_filler_clip = false;
		int filler_clip = 0;
		if (data.has("use_filler_clip") && data.has("filler_clip")) {
			use_filler_clip = data["use_filler_clip"];
			filler_clip = data["filler_clip"];
		}
		bool hold_previous = data.has("hold_previous") ? bool(data["hold_previous"]) : false;

		add_transition(k.x, k.y, TransitionFromTime(int(data["from_time"])), TransitionToTime(int(data["to_time"])), FadeMode(int(data["fade_mode"])), data["fade_beats"], use_filler_clip, filler_clip, hold_previous);
	}
}

Dictionary AudioStreamInteractive::_get_transitions() const {
	Vector<Vector2i> keys;

	for (const KeyValue<TransitionKey, Transition> &K : transition_map) {
		keys.push_back(Vector2i(K.key.from_clip, K.key.to_clip));
	}
	keys.sort();
	Dictionary ret;
	for (int i = 0; i < keys.size(); i++) {
		const Transition &tr = transition_map[TransitionKey(keys[i].x, keys[i].y)];
		Dictionary data;
		data["from_time"] = tr.from_time;
		data["to_time"] = tr.to_time;
		data["fade_mode"] = tr.fade_mode;
		data["fade_beats"] = tr.fade_beats;
		if (tr.use_filler_clip) {
			data["use_filler_clip"] = true;
			data["filler_clip"] = tr.filler_clip;
		}
		if (tr.hold_previous) {
			data["hold_previous"] = true;
		}

		ret[keys[i]] = data;
	}
	return ret;
}

bool AudioStreamInteractive::has_transition(int p_from_clip, int p_to_clip) const {
	TransitionKey tk(p_from_clip, p_to_clip);
	return transition_map.has(tk);
}

void AudioStreamInteractive::erase_transition(int p_from_clip, int p_to_clip) {
	TransitionKey tk(p_from_clip, p_to_clip);
	ERR_FAIL_COND(!transition_map.has(tk));
	AudioDriver::get_singleton()->lock();
	transition_map.erase(tk);
	AudioDriver::get_singleton()->unlock();
}

PackedInt32Array AudioStreamInteractive::get_transition_list() const {
	PackedInt32Array ret;

	for (const KeyValue<TransitionKey, Transition> &K : transition_map) {
		ret.push_back(K.key.from_clip);
		ret.push_back(K.key.to_clip);
	}
	return ret;
}

void AudioStreamInteractive::add_transition(int p_from_clip, int p_to_clip, TransitionFromTime p_from_time, TransitionToTime p_to_time, FadeMode p_fade_mode, float p_fade_beats, bool p_use_filler_flip, int p_filler_clip, bool p_hold_previous) {
	ERR_FAIL_COND(p_from_clip < CLIP_ANY || p_from_clip >= clip_count);
	ERR_FAIL_COND(p_to_clip < CLIP_ANY || p_to_clip >= clip_count);
	ERR_FAIL_UNSIGNED_INDEX(p_from_time, TRANSITION_FROM_TIME_MAX);
	ERR_FAIL_UNSIGNED_INDEX(p_to_time, TRANSITION_TO_TIME_MAX);
	ERR_FAIL_UNSIGNED_INDEX(p_fade_mode, FADE_MAX);

	Transition tr;
	tr.from_time = p_from_time;
	tr.to_time = p_to_time;
	tr.fade_mode = p_fade_mode;
	tr.fade_beats = p_fade_beats;
	tr.use_filler_clip = p_use_filler_flip;
	tr.filler_clip = p_filler_clip;
	tr.hold_previous = p_hold_previous;

	TransitionKey tk(p_from_clip, p_to_clip);

	AudioDriver::get_singleton()->lock();
	transition_map[tk] = tr;
	AudioDriver::get_singleton()->unlock();
}

AudioStreamInteractive::TransitionFromTime AudioStreamInteractive::get_transition_from_time(int p_from_clip, int p_to_clip) const {
	TransitionKey tk(p_from_clip, p_to_clip);
	ERR_FAIL_COND_V(!transition_map.has(tk), TRANSITION_FROM_TIME_END);
	return transition_map[tk].from_time;
}

AudioStreamInteractive::TransitionToTime AudioStreamInteractive::get_transition_to_time(int p_from_clip, int p_to_clip) const {
	TransitionKey tk(p_from_clip, p_to_clip);
	ERR_FAIL_COND_V(!transition_map.has(tk), TRANSITION_TO_TIME_START);
	return transition_map[tk].to_time;
}

AudioStreamInteractive::FadeMode AudioStreamInteractive::get_transition_fade_mode(int p_from_clip, int p_to_clip) const {
	TransitionKey tk(p_from_clip, p_to_clip);
	ERR_FAIL_COND_V(!transition_map.has(tk), FADE_DISABLED);
	return transition_map[tk].fade_mode;
}

float AudioStreamInteractive::get_transition_fade_beats(int p_from_clip, int p_to_clip) const {
	TransitionKey tk(p_from_clip, p_to_clip);
	ERR_FAIL_COND_V(!transition_map.has(tk), -1);
	return transition_map[tk].fade_beats;
}

bool AudioStreamInteractive::is_transition_using_filler_clip(int p_from_clip, int p_to_clip) const {
	TransitionKey tk(p_from_clip, p_to_clip);
	ERR_FAIL_COND_V(!transition_map.has(tk), false);
	return transition_map[tk].use_filler_clip;
}

int AudioStreamInteractive::get_transition_filler_clip(int p_from_clip, int p_to_clip) const {
	TransitionKey tk(p_from_clip, p_to_clip);
	ERR_FAIL_COND_V(!transition_map.has(tk), -1);
	return transition_map[tk].filler_clip;
}

bool AudioStreamInteractive::is_transition_holding_previous(int p_from_clip, int p_to_clip) const {
	TransitionKey tk(p_from_clip, p_to_clip);
	ERR_FAIL_COND_V(!transition_map.has(tk), false);
	return transition_map[tk].hold_previous;
}

#ifdef TOOLS_ENABLED

PackedStringArray AudioStreamInteractive::_get_linked_undo_properties(const String &p_property, const Variant &p_new_value) const {
	PackedStringArray ret;

	if (p_property.begins_with("clip_") && p_property.ends_with("/stream")) {
		int clip = p_property.get_slicec('_', 1).to_int();
		if (clip < clip_count) {
			ret.push_back("clip_" + itos(clip) + "/name");
		}
	}

	if (p_property == "clip_count") {
		int new_clip_count = p_new_value;

		if (new_clip_count < clip_count) {
			for (int i = 0; i < clip_count; i++) {
				if (clips[i].auto_advance_next_clip >= new_clip_count) {
					ret.push_back("clip_" + itos(i) + "/auto_advance");
					ret.push_back("clip_" + itos(i) + "/next_clip");
				}
			}

			ret.push_back("_transitions");
			if (initial_clip >= new_clip_count) {
				ret.push_back("initial_clip");
			}
		}
	}
	return ret;
}

template <class T>
static void _test_and_swap(T &p_elem, uint32_t p_a, uint32_t p_b) {
	if ((uint32_t)p_elem == p_a) {
		p_elem = p_b;
	} else if (uint32_t(p_elem) == p_b) {
		p_elem = p_a;
	}
}

void AudioStreamInteractive::_inspector_array_swap_clip(uint32_t p_item_a, uint32_t p_item_b) {
	ERR_FAIL_UNSIGNED_INDEX(p_item_a, (uint32_t)clip_count);
	ERR_FAIL_UNSIGNED_INDEX(p_item_b, (uint32_t)clip_count);

	for (int i = 0; i < clip_count; i++) {
		_test_and_swap(clips[i].auto_advance_next_clip, p_item_a, p_item_b);
	}

	Vector<TransitionKey> to_remove;
	HashMap<TransitionKey, Transition, TransitionKeyHasher> to_add;

	for (KeyValue<TransitionKey, Transition> &K : transition_map) {
		if (K.key.from_clip == p_item_a || K.key.from_clip == p_item_b || K.key.to_clip == p_item_a || K.key.to_clip == p_item_b) {
			to_remove.push_back(K.key);
			TransitionKey new_key = K.key;
			_test_and_swap(new_key.from_clip, p_item_a, p_item_b);
			_test_and_swap(new_key.to_clip, p_item_a, p_item_b);
			to_add[new_key] = K.value;
		}
	}

	for (int i = 0; i < to_remove.size(); i++) {
		transition_map.erase(to_remove[i]);
	}

	for (KeyValue<TransitionKey, Transition> &K : to_add) {
		transition_map.insert(K.key, K.value);
	}

	SWAP(clips[p_item_a], clips[p_item_b]);

	stream_name_cache = "";

	notify_property_list_changed();
	emit_signal(SNAME("parameter_list_changed"));
}

String AudioStreamInteractive::_get_streams_hint() const {
	if (!stream_name_cache.is_empty()) {
		return stream_name_cache;
	}

	for (int i = 0; i < clip_count; i++) {
		if (i > 0) {
			stream_name_cache += ",";
		}
		String n = String(clips[i].name).replace(",", " ");

		if (n == "" && clips[i].stream.is_valid()) {
			if (!clips[i].stream->get_name().is_empty()) {
				n = clips[i].stream->get_name().replace(",", " ");
			} else if (clips[i].stream->get_path().is_resource_file()) {
				n = clips[i].stream->get_path().get_file().replace(",", " ");
			}
		}

		if (n == "") {
			n = "Clip " + itos(i);
		}

		stream_name_cache += n;
	}

	return stream_name_cache;
}

#endif
void AudioStreamInteractive::_validate_property(PropertyInfo &r_property) const {
	String prop = r_property.name;

#ifdef TOOLS_ENABLED
	if (prop == "switch_to") {
		r_property.hint_string = _get_streams_hint();
		return;
	}
#endif

	if (prop == "initial_clip") {
#ifdef TOOLS_ENABLED
		r_property.hint_string = _get_streams_hint();
#endif
	} else if (prop.begins_with("clip_") && prop != "clip_count") {
		int clip = prop.get_slicec('_', 1).to_int();
		if (clip >= clip_count) {
			r_property.usage = PROPERTY_USAGE_INTERNAL;
		} else if (prop == "clip_" + itos(clip) + "/next_clip") {
			if (clips[clip].auto_advance != AUTO_ADVANCE_ENABLED) {
				r_property.usage = 0;
			} else {
#ifdef TOOLS_ENABLED
				r_property.hint_string = _get_streams_hint();
#endif
			}
		}
	}
}

void AudioStreamInteractive::get_parameter_list(List<Parameter> *r_parameters) {
	String clip_names;
	for (int i = 0; i < clip_count; i++) {
		clip_names += ",";
		clip_names += clips[i].name;
	}
	r_parameters->push_back(Parameter(PropertyInfo(Variant::STRING, "switch_to_clip", PROPERTY_HINT_ENUM, clip_names, PROPERTY_USAGE_EDITOR), ""));
}

void AudioStreamInteractive::_bind_methods() {
#ifdef TOOLS_ENABLED
	ClassDB::bind_method(D_METHOD("_get_linked_undo_properties", "for_property", "for_value"), &AudioStreamInteractive::_get_linked_undo_properties);
	ClassDB::bind_method(D_METHOD("_inspector_array_swap_clip", "a", "b"), &AudioStreamInteractive::_inspector_array_swap_clip);
#endif

	// CLIPS

	ClassDB::bind_method(D_METHOD("set_clip_count", "clip_count"), &AudioStreamInteractive::set_clip_count);
	ClassDB::bind_method(D_METHOD("get_clip_count"), &AudioStreamInteractive::get_clip_count);

	ClassDB::bind_method(D_METHOD("set_initial_clip", "clip_index"), &AudioStreamInteractive::set_initial_clip);
	ClassDB::bind_method(D_METHOD("get_initial_clip"), &AudioStreamInteractive::get_initial_clip);

	ClassDB::bind_method(D_METHOD("set_clip_name", "clip_index", "name"), &AudioStreamInteractive::set_clip_name);
	ClassDB::bind_method(D_METHOD("get_clip_name", "clip_index"), &AudioStreamInteractive::get_clip_name);

	ClassDB::bind_method(D_METHOD("set_clip_stream", "clip_index", "stream"), &AudioStreamInteractive::set_clip_stream);
	ClassDB::bind_method(D_METHOD("get_clip_stream", "clip_index"), &AudioStreamInteractive::get_clip_stream);

	ClassDB::bind_method(D_METHOD("set_clip_auto_advance", "clip_index", "mode"), &AudioStreamInteractive::set_clip_auto_advance);
	ClassDB::bind_method(D_METHOD("get_clip_auto_advance", "clip_index"), &AudioStreamInteractive::get_clip_auto_advance);

	ClassDB::bind_method(D_METHOD("set_clip_auto_advance_next_clip", "clip_index", "auto_advance_next_clip"), &AudioStreamInteractive::set_clip_auto_advance_next_clip);
	ClassDB::bind_method(D_METHOD("get_clip_auto_advance_next_clip", "clip_index"), &AudioStreamInteractive::get_clip_auto_advance_next_clip);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "clip_count", PROPERTY_HINT_RANGE, "1," + itos(MAX_CLIPS), PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_ARRAY, "Clips,clip_,page_size=999,unfoldable,numbered,swap_method=_inspector_array_swap_clip,add_button_text=" + String(RTR("Add Clip"))), "set_clip_count", "get_clip_count");
	for (int i = 0; i < MAX_CLIPS; i++) {
		ADD_PROPERTYI(PropertyInfo(Variant::STRING_NAME, "clip_" + itos(i) + "/name", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_INTERNAL), "set_clip_name", "get_clip_name", i);
		ADD_PROPERTYI(PropertyInfo(Variant::OBJECT, "clip_" + itos(i) + "/stream", PROPERTY_HINT_RESOURCE_TYPE, "AudioStream", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_INTERNAL), "set_clip_stream", "get_clip_stream", i);
		ADD_PROPERTYI(PropertyInfo(Variant::INT, "clip_" + itos(i) + "/auto_advance", PROPERTY_HINT_ENUM, "Disabled,Enabled,ReturnToHold", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_INTERNAL), "set_clip_auto_advance", "get_clip_auto_advance", i);
		ADD_PROPERTYI(PropertyInfo(Variant::INT, "clip_" + itos(i) + "/next_clip", PROPERTY_HINT_ENUM, "", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_INTERNAL), "set_clip_auto_advance_next_clip", "get_clip_auto_advance_next_clip", i);
	}

	// Needs to be registered after `clip_*` properties, as it depends on them.
	ADD_PROPERTY(PropertyInfo(Variant::INT, "initial_clip", PROPERTY_HINT_ENUM, "", PROPERTY_USAGE_DEFAULT), "set_initial_clip", "get_initial_clip");

	// TRANSITIONS

	ClassDB::bind_method(D_METHOD("add_transition", "from_clip", "to_clip", "from_time", "to_time", "fade_mode", "fade_beats", "use_filler_clip", "filler_clip", "hold_previous"), &AudioStreamInteractive::add_transition, DEFVAL(false), DEFVAL(-1), DEFVAL(false));
	ClassDB::bind_method(D_METHOD("has_transition", "from_clip", "to_clip"), &AudioStreamInteractive::has_transition);
	ClassDB::bind_method(D_METHOD("erase_transition", "from_clip", "to_clip"), &AudioStreamInteractive::erase_transition);
	ClassDB::bind_method(D_METHOD("get_transition_list"), &AudioStreamInteractive::get_transition_list);

	ClassDB::bind_method(D_METHOD("get_transition_from_time", "from_clip", "to_clip"), &AudioStreamInteractive::get_transition_from_time);
	ClassDB::bind_method(D_METHOD("get_transition_to_time", "from_clip", "to_clip"), &AudioStreamInteractive::get_transition_to_time);
	ClassDB::bind_method(D_METHOD("get_transition_fade_mode", "from_clip", "to_clip"), &AudioStreamInteractive::get_transition_fade_mode);
	ClassDB::bind_method(D_METHOD("get_transition_fade_beats", "from_clip", "to_clip"), &AudioStreamInteractive::get_transition_fade_beats);
	ClassDB::bind_method(D_METHOD("is_transition_using_filler_clip", "from_clip", "to_clip"), &AudioStreamInteractive::is_transition_using_filler_clip);
	ClassDB::bind_method(D_METHOD("get_transition_filler_clip", "from_clip", "to_clip"), &AudioStreamInteractive::get_transition_filler_clip);
	ClassDB::bind_method(D_METHOD("is_transition_holding_previous", "from_clip", "to_clip"), &AudioStreamInteractive::is_transition_holding_previous);

	ClassDB::bind_method(D_METHOD("_set_transitions", "transitions"), &AudioStreamInteractive::_set_transitions);
	ClassDB::bind_method(D_METHOD("_get_transitions"), &AudioStreamInteractive::_get_transitions);

	ADD_PROPERTY(PropertyInfo(Variant::DICTIONARY, "_transitions", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NO_EDITOR | PROPERTY_USAGE_INTERNAL), "_set_transitions", "_get_transitions");

	BIND_ENUM_CONSTANT(TRANSITION_FROM_TIME_IMMEDIATE);
	BIND_ENUM_CONSTANT(TRANSITION_FROM_TIME_NEXT_BEAT);
	BIND_ENUM_CONSTANT(TRANSITION_FROM_TIME_NEXT_BAR);
	BIND_ENUM_CONSTANT(TRANSITION_FROM_TIME_END);

	BIND_ENUM_CONSTANT(TRANSITION_TO_TIME_SAME_POSITION);
	BIND_ENUM_CONSTANT(TRANSITION_TO_TIME_START);

	BIND_ENUM_CONSTANT(FADE_DISABLED);
	BIND_ENUM_CONSTANT(FADE_IN);
	BIND_ENUM_CONSTANT(FADE_OUT);
	BIND_ENUM_CONSTANT(FADE_CROSS);
	BIND_ENUM_CONSTANT(FADE_AUTOMATIC);

	BIND_ENUM_CONSTANT(AUTO_ADVANCE_DISABLED);
	BIND_ENUM_CONSTANT(AUTO_ADVANCE_ENABLED);
	BIND_ENUM_CONSTANT(AUTO_ADVANCE_RETURN_TO_HOLD);

	BIND_CONSTANT(CLIP_ANY);
}

///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
AudioStreamPlaybackInteractive::AudioStreamPlaybackInteractive() {
}

AudioStreamPlaybackInteractive::~AudioStreamPlaybackInteractive() {
}

void AudioStreamPlaybackInteractive::stop() {
	if (!active) {
		return;
	}

	active = false;

	for (int i = 0; i < AudioStreamInteractive::MAX_CLIPS; i++) {
		if (states[i].playback.is_valid()) {
			states[i].playback->stop();
		}
		states[i].fade_speed = 0.0;
		states[i].fade_volume = 0.0;
		states[i].fade_wait = 0.0;
		states[i].reset_fade();
		states[i].active = false;
		states[i].auto_advance = -1;
		states[i].first_mix = true;
	}
}

void AudioStreamPlaybackInteractive::start(double p_from_pos) {
	if (active) {
		stop();
	}

	if (version != stream->version) {
		for (int i = 0; i < AudioStreamInteractive::MAX_CLIPS; i++) {
			Ref<AudioStream> src_stream;
			if (i < stream->clip_count) {
				src_stream = stream->clips[i].stream;
			}
			if (states[i].stream != src_stream) {
				states[i].stream.unref();
				states[i].playback.unref();

				states[i].stream = src_stream;
				states[i].playback = src_stream->instantiate_playback();
			}
		}

		version = stream->version;
	}

	int current = stream->initial_clip;
	if (current < 0 || current >= stream->clip_count) {
		return; // No playback possible.
	}
	if (states[current].playback.is_null()) {
		return; //no playback possible
	}
	active = true;

	_queue(current, false);
}

void AudioStreamPlaybackInteractive::_queue(int p_to_clip_index, bool p_is_auto_advance) {
	ERR_FAIL_INDEX(p_to_clip_index, stream->clip_count);
	ERR_FAIL_COND(states[p_to_clip_index].playback.is_null());

	if (playback_current == -1) {
		// Nothing to do, start.
		int current = p_to_clip_index;
		State &state = states[current];
		state.active = true;
		state.fade_wait = 0;
		state.fade_volume = 1.0;
		state.fade_speed = 0;
		state.first_mix = true;

		state.playback->start(0);

		playback_current = current;

		if (stream->clips[current].auto_advance == AudioStreamInteractive::AUTO_ADVANCE_ENABLED && stream->clips[current].auto_advance_next_clip >= 0 && stream->clips[current].auto_advance_next_clip < stream->clip_count && stream->clips[current].auto_advance_next_clip != current) {
			//prepare auto advance
			state.auto_advance = stream->clips[current].auto_advance_next_clip;
		}
		return;
	}

	for (int i = 0; i < stream->clip_count; i++) {
		if (i == playback_current || i == p_to_clip_index) {
			continue;
		}
		if (states[i].active && states[i].fade_wait > 0) { // Waiting to kick in, terminate because change of plans.
			states[i].playback->stop();
			states[i].reset_fade();
			states[i].active = false;
		}
	}

	State &from_state = states[playback_current];
	State &to_state = states[p_to_clip_index];

	AudioStreamInteractive::Transition transition; // Use an empty transition by default

	AudioStreamInteractive::TransitionKey tkeys[4] = {
		AudioStreamInteractive::TransitionKey(playback_current, p_to_clip_index),
		AudioStreamInteractive::TransitionKey(playback_current, AudioStreamInteractive::CLIP_ANY),
		AudioStreamInteractive::TransitionKey(AudioStreamInteractive::CLIP_ANY, p_to_clip_index),
		AudioStreamInteractive::TransitionKey(AudioStreamInteractive::CLIP_ANY, AudioStreamInteractive::CLIP_ANY)
	};

	for (int i = 0; i < 4; i++) {
		if (stream->transition_map.has(tkeys[i])) {
			transition = stream->transition_map[tkeys[i]];
			break;
		}
	}

	if (transition.fade_mode == AudioStreamInteractive::FADE_AUTOMATIC) {
		// Adjust automatic mode based on context.
		if (transition.to_time == AudioStreamInteractive::TRANSITION_TO_TIME_START) {
			transition.fade_mode = AudioStreamInteractive::FADE_OUT;
		} else {
			transition.fade_mode = AudioStreamInteractive::FADE_CROSS;
		}
	}

	if (p_is_auto_advance) {
		transition.from_time = AudioStreamInteractive::TRANSITION_FROM_TIME_END;
		if (transition.to_time == AudioStreamInteractive::TRANSITION_TO_TIME_SAME_POSITION) {
			transition.to_time = AudioStreamInteractive::TRANSITION_TO_TIME_START;
		}
	}

	// Prepare the fadeout
	float current_pos = from_state.playback->get_playback_position();

	float src_fade_wait = 0;
	float dst_seek_to = 0;
	float fade_speed = 0;
	bool src_no_loop = false;

	if (from_state.stream->get_bpm()) {
		// Check if source speed has BPM, if so, transition syncs to BPM
		float beat_sec = 60 / float(from_state.stream->get_bpm());
		switch (transition.from_time) {
			case AudioStreamInteractive::TRANSITION_FROM_TIME_IMMEDIATE: {
				src_fade_wait = 0;
			} break;
			case AudioStreamInteractive::TRANSITION_FROM_TIME_NEXT_BEAT: {
				float remainder = Math::fmod(current_pos, beat_sec);
				src_fade_wait = beat_sec - remainder;
			} break;
			case AudioStreamInteractive::TRANSITION_FROM_TIME_NEXT_BAR: {
				if (from_state.stream->get_bar_beats() > 0) {
					float bar_sec = beat_sec * from_state.stream->get_bar_beats();
					float remainder = Math::fmod(current_pos, bar_sec);
					src_fade_wait = bar_sec - remainder;
				} else {
					// Stream does not have a number of beats per bar - avoid NaN, and play immediately.
					src_fade_wait = 0;
				}
			} break;
			case AudioStreamInteractive::TRANSITION_FROM_TIME_END: {
				float end = from_state.stream->get_beat_count() > 0 ? float(from_state.stream->get_beat_count() * beat_sec) : from_state.stream->get_length();
				if (end == 0) {
					// Stream does not have a length.
					src_fade_wait = 0;
				} else {
					src_fade_wait = end - current_pos;
				}

				if (!from_state.stream->has_loop()) {
					src_no_loop = true;
				}

			} break;
			default: {
			}
		}
		// Fade speed also aligned to BPM
		fade_speed = 1.0 / (transition.fade_beats * beat_sec);
	} else {
		// Source has no BPM, so just simple transition.
		if (transition.from_time == AudioStreamInteractive::TRANSITION_FROM_TIME_END && from_state.stream->get_length() > 0) {
			float end = from_state.stream->get_length();
			src_fade_wait = end - current_pos;
			if (!from_state.stream->has_loop()) {
				src_no_loop = true;
			}
		} else {
			src_fade_wait = 0;
		}
		fade_speed = 1.0 / transition.fade_beats;
	}

	if (transition.to_time == AudioStreamInteractive::TRANSITION_TO_TIME_PREVIOUS_POSITION && to_state.stream->get_length() > 0.0) {
		dst_seek_to = to_state.previous_position;
	} else if (transition.to_time == AudioStreamInteractive::TRANSITION_TO_TIME_SAME_POSITION && transition.from_time != AudioStreamInteractive::TRANSITION_FROM_TIME_END && to_state.stream->get_length() > 0.0) {
		// Seeking to basically same position as when we start fading.
		dst_seek_to = current_pos + src_fade_wait;
		float end;
		if (to_state.stream->get_bpm() > 0 && to_state.stream->get_beat_count()) {
			float beat_sec = 60 / float(to_state.stream->get_bpm());
			end = to_state.stream->get_beat_count() * beat_sec;
		} else {
			end = to_state.stream->get_length();
		}

		if (dst_seek_to > end) {
			// Seeking too far away.
			dst_seek_to = 0; //past end, loop to beginning.
		}

	} else {
		// Seek to Start
		dst_seek_to = 0.0;
	}

	if (transition.fade_mode == AudioStreamInteractive::FADE_DISABLED || transition.fade_mode == AudioStreamInteractive::FADE_IN) {
		if (src_no_loop) {
			// If there is no fade in the source stream, then let it continue until it ends.
			from_state.fade_wait = 0;
			from_state.fade_speed = 0;
		} else {
			// Otherwise force a very quick fade to avoid clicks
			from_state.fade_wait = src_fade_wait;
			from_state.fade_speed = 1.0 / -0.001;
		}
	} else {
		// Regular fade.
		from_state.fade_wait = src_fade_wait;
		from_state.fade_speed = -fade_speed;
	}
	// keep volume, since it may have been fading in from something else.

	to_state.playback->start(dst_seek_to);
	to_state.active = true;
	to_state.fade_volume = 0.0;
	to_state.first_mix = true;

	int auto_advance_to = -1;

	if (stream->clips[p_to_clip_index].auto_advance == AudioStreamInteractive::AUTO_ADVANCE_ENABLED) {
		int next_clip = stream->clips[p_to_clip_index].auto_advance_next_clip;
		if (next_clip >= 0 && next_clip < (int)stream->clip_count && states[next_clip].playback.is_valid() && next_clip != p_to_clip_index && (!transition.use_filler_clip || next_clip != transition.filler_clip)) {
			auto_advance_to = next_clip;
		}
	}

	if (return_memory != -1 && stream->clips[p_to_clip_index].auto_advance == AudioStreamInteractive::AUTO_ADVANCE_RETURN_TO_HOLD) {
		auto_advance_to = return_memory;
		return_memory = -1;
	}

	if (transition.hold_previous) {
		return_memory = playback_current;
	}

	if (transition.use_filler_clip && transition.filler_clip >= 0 && transition.filler_clip < (int)stream->clip_count && states[transition.filler_clip].playback.is_valid() && playback_current != transition.filler_clip && p_to_clip_index != transition.filler_clip) {
		State &filler_state = states[transition.filler_clip];

		filler_state.playback->start(0);
		filler_state.active = true;

		// Filler state does not fade (bake fade in the audio clip if you want fading.
		filler_state.fade_volume = 1.0;
		filler_state.fade_speed = 0.0;

		filler_state.fade_wait = src_fade_wait;
		filler_state.first_mix = true;

		float filler_end;
		if (filler_state.stream->get_bpm() > 0 && filler_state.stream->get_beat_count() > 0) {
			float filler_beat_sec = 60 / float(filler_state.stream->get_bpm());
			filler_end = filler_beat_sec * filler_state.stream->get_beat_count();
		} else {
			filler_end = filler_state.stream->get_length();
		}

		if (!filler_state.stream->has_loop()) {
			src_no_loop = true;
		}

		if (transition.fade_mode == AudioStreamInteractive::FADE_DISABLED || transition.fade_mode == AudioStreamInteractive::FADE_OUT) {
			// No fading, immediately start at full volume.
			to_state.fade_volume = 0.0;
			to_state.fade_speed = 1.0; //start at full volume, as filler is meant as a transition.
		} else {
			// Fade enable, prepare fade.
			to_state.fade_volume = 0.0;
			to_state.fade_speed = fade_speed;
		}

		to_state.fade_wait = src_fade_wait + filler_end;

	} else {
		to_state.fade_wait = src_fade_wait;

		if (transition.fade_mode == AudioStreamInteractive::FADE_DISABLED || transition.fade_mode == AudioStreamInteractive::FADE_OUT) {
			to_state.fade_volume = 1.0;
			to_state.fade_speed = 0.0;
		} else {
			to_state.fade_volume = 0.0;
			to_state.fade_speed = fade_speed;
		}

		to_state.auto_advance = auto_advance_to;
	}
}

void AudioStreamPlaybackInteractive::seek(double p_time) {
	// Seek not supported
}

int AudioStreamPlaybackInteractive::mix(AudioFrame *p_buffer, float p_rate_scale, int p_frames) {
	if (active && version != stream->version) {
		stop();
	}

	if (switch_request != -1) {
		_queue(switch_request, false);
		switch_request = -1;
	}

	if (!active) {
		return 0;
	}

	int todo = p_frames;

	while (todo) {
		int to_mix = MIN(todo, BUFFER_SIZE);
		_mix_internal(to_mix);
		for (int i = 0; i < to_mix; i++) {
			p_buffer[i] = mix_buffer[i];
		}
		p_buffer += to_mix;
		todo -= to_mix;
	}

	return p_frames;
}

void AudioStreamPlaybackInteractive::_mix_internal(int p_frames) {
	for (int i = 0; i < p_frames; i++) {
		mix_buffer[i] = AudioFrame(0, 0);
	}

	for (int i = 0; i < stream->clip_count; i++) {
		if (!states[i].active) {
			continue;
		}

		_mix_internal_state(i, p_frames);
	}
}

void AudioStreamPlaybackInteractive::_mix_internal_state(int p_state_idx, int p_frames) {
	State &state = states[p_state_idx];
	double mix_rate = double(AudioServer::get_singleton()->get_mix_rate());
	double frame_inc = 1.0 / mix_rate;

	int from_frame = 0;
	int queue_next = -1;

	if (state.first_mix) {
		// Did not start mixing yet, wait.
		double mix_time = p_frames * frame_inc;
		if (state.fade_wait < mix_time) {
			// time to start!
			from_frame = state.fade_wait * mix_rate;
			state.fade_wait = 0;
			if (state.fade_speed == 0.0) {
				queue_next = state.auto_advance;
			}
			playback_current = p_state_idx;
			state.first_mix = false;
		} else {
			// This is for fade in of new stream.
			state.fade_wait -= mix_time;
			return; // Nothing to do
		}
	}

	state.previous_position = state.playback->get_playback_position();
	state.playback->mix(temp_buffer + from_frame, 1.0, p_frames - from_frame);

	double frame_fade_inc = state.fade_speed * frame_inc;
	for (int i = from_frame; i < p_frames; i++) {
		if (state.fade_wait) {
			// This is for fade out of existing stream;
			state.fade_wait -= frame_inc;
			if (state.fade_wait < 0.0) {
				state.fade_wait = 0.0;
			}
		} else if (frame_fade_inc > 0) {
			state.fade_volume += frame_fade_inc;
			if (state.fade_volume >= 1.0) {
				state.fade_speed = 0.0;
				frame_fade_inc = 0.0;
				state.fade_volume = 1.0;
				queue_next = state.auto_advance;
			}
		} else if (frame_fade_inc < 0.0) {
			state.fade_volume += frame_fade_inc;
			if (state.fade_volume <= 0.0) {
				state.fade_speed = 0.0;
				frame_fade_inc = 0.0;
				state.fade_volume = 0.0;
				state.playback->stop(); // Stop playback and break, no point to continue mixing
				break;
			}
		}

		mix_buffer[i] += temp_buffer[i] * state.fade_volume;
		state.previous_position += frame_inc;
	}

	if (!state.playback->is_playing()) {
		// It finished because it either reached end or faded out, so deactivate and continue.
		state.active = false;
	}
	if (queue_next != -1) {
		_queue(queue_next, true);
	}
}

void AudioStreamPlaybackInteractive::tag_used_streams() {
	for (int i = 0; i < stream->clip_count; i++) {
		if (states[i].active && !states[i].first_mix && states[i].playback->is_playing()) {
			states[i].stream->tag_used(states[i].playback->get_playback_position());
		}
	}
	stream->tag_used(0);
}

void AudioStreamPlaybackInteractive::switch_to_clip_by_name(const StringName &p_name) {
	if (p_name == StringName()) {
		switch_request = -1;
		return;
	}

	ERR_FAIL_COND_MSG(stream.is_null(), "Attempted to switch while not playing back any stream.");

	for (int i = 0; i < stream->get_clip_count(); i++) {
		if (stream->get_clip_name(i) == p_name) {
			switch_request = i;
			return;
		}
	}
	ERR_FAIL_MSG("Clip not found: " + String(p_name));
}

void AudioStreamPlaybackInteractive::set_parameter(const StringName &p_name, const Variant &p_value) {
	if (p_name == SNAME("switch_to_clip")) {
		switch_to_clip_by_name(p_value);
	}
}

Variant AudioStreamPlaybackInteractive::get_parameter(const StringName &p_name) const {
	if (p_name == SNAME("switch_to_clip")) {
		for (int i = 0; i < stream->get_clip_count(); i++) {
			if (switch_request != -1) {
				if (switch_request == i) {
					return String(stream->get_clip_name(i));
				}
			} else if (playback_current == i) {
				return String(stream->get_clip_name(i));
			}
		}
		return "";
	}

	return Variant();
}

void AudioStreamPlaybackInteractive::switch_to_clip(int p_index) {
	switch_request = p_index;
}

int AudioStreamPlaybackInteractive::get_current_clip_index() const {
	return playback_current;
}

int AudioStreamPlaybackInteractive::get_loop_count() const {
	return 0; // Looping not supported
}

double AudioStreamPlaybackInteractive::get_playback_position() const {
	return 0.0;
}

bool AudioStreamPlaybackInteractive::is_playing() const {
	return active;
}

void AudioStreamPlaybackInteractive::_bind_methods() {
	ClassDB::bind_method(D_METHOD("switch_to_clip_by_name", "clip_name"), &AudioStreamPlaybackInteractive::switch_to_clip_by_name);
	ClassDB::bind_method(D_METHOD("switch_to_clip", "clip_index"), &AudioStreamPlaybackInteractive::switch_to_clip);
	ClassDB::bind_method(D_METHOD("get_current_clip_index"), &AudioStreamPlaybackInteractive::get_current_clip_index);
}

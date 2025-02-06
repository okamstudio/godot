/**************************************************************************/
/*  audio_stream_graph_nodes.cpp                                          */
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

#include "audio_stream_graph_nodes.h"

String AudioStreamGraphInputNode::get_caption() const {
	return "AudioInput";
}

String AudioStreamGraphInputNode::get_description() const {
	return "Lorem ipsum";
}

int AudioStreamGraphInputNode::get_input_port_count() const {
	return 0;
}

AudioStreamGraphNode::PortType AudioStreamGraphInputNode::get_input_port_type(int p_port) const {
	return PORT_TYPE_STREAM;
}

String AudioStreamGraphInputNode::get_input_port_name(int p_port) const {
	return "";
};

int AudioStreamGraphInputNode::get_output_port_count() const {
	return 1;
}

AudioStreamGraphNode::PortType AudioStreamGraphInputNode::get_output_port_type(int p_port) const {
	return PORT_TYPE_STREAM;
}

String AudioStreamGraphInputNode::get_output_port_name(int p_port) const {
	return "out";
};

void AudioStreamGraphInputNode::update_default_values() {
}

void AudioStreamGraphInputNode::start(double p_from_pos) {
	if (is_playing()) {
		stop();
	}
	if (stream.is_valid()) {
		playback = stream->instantiate_playback();
		playback->start(p_from_pos);
	}
}

int AudioStreamGraphInputNode::mix(AudioFrame *p_buffer, float p_rate_scale, int p_frames) {
	if (!playback.is_valid()) {
		return 0;
	}

	if (playback->is_playing()) {
		return playback->mix(p_buffer, p_rate_scale, p_frames);
	}

	return 0;
}

void AudioStreamGraphInputNode::stop() {
	if (playback.is_valid()) {
		playback->stop();
	}
}

bool AudioStreamGraphInputNode::is_playing() const {
	return playback.is_valid() && playback->is_playing();
}

int AudioStreamGraphInputNode::get_loop_count() const {
	if (playback.is_valid()) {
		return playback->get_loop_count();
	}

	return 0;
}

double AudioStreamGraphInputNode::get_playback_position() const {
	if (playback.is_valid()) {
		return playback->get_playback_position();
	}

	return 0.0;
}

bool AudioStreamGraphInputNode::is_show_prop_names() const {
	return true;
}

Vector<StringName> AudioStreamGraphInputNode::get_editable_properties() const {
	Vector<StringName> props;
	props.push_back("stream");

	return props;
}

void AudioStreamGraphInputNode::set_stream(Ref<AudioStream> p_stream) {
	stream = p_stream;
}
Ref<AudioStream> AudioStreamGraphInputNode::get_stream() const {
	return stream;
}

void AudioStreamGraphInputNode::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_stream", "stream"), &AudioStreamGraphInputNode::set_stream);
	ClassDB::bind_method(D_METHOD("get_stream"), &AudioStreamGraphInputNode::get_stream);
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "stream", PROPERTY_HINT_RESOURCE_TYPE, "AudioStream"), "set_stream", "get_stream");
}

String AudioStreamGraphOutputNode::get_caption() const {
	return "Output";
}

String AudioStreamGraphOutputNode::get_description() const {
	return "";
}

int AudioStreamGraphOutputNode::get_input_port_count() const {
	return 1;
}

AudioStreamGraphNode::PortType AudioStreamGraphOutputNode::get_input_port_type(int p_port) const {
	return PORT_TYPE_STREAM;
}

void AudioStreamGraphOutputNode::update_default_values() {
}

String AudioStreamGraphOutputNode::get_input_port_name(int p_port) const {
	return "input";
};

int AudioStreamGraphOutputNode::get_output_port_count() const {
	return 0;
}

AudioStreamGraphNode::PortType AudioStreamGraphOutputNode::get_output_port_type(int p_port) const {
	return PORT_TYPE_STREAM;
}

String AudioStreamGraphOutputNode::get_output_port_name(int p_port) const {
	return "";
};

void AudioStreamGraphOutputNode::start(double p_from_pos) {
	HashMap<int, Ref<AudioStreamGraphNodePlayback>> nodes = get_connected_input_playback_nodes();

	if (nodes.size() > 0) {
		node = nodes[0];
		if (is_playing()) {
			node->stop();
		}
		node->start(p_from_pos);
	}
}

int AudioStreamGraphOutputNode::mix(AudioFrame *p_buffer, float p_rate_scale, int p_frames) {
	if (!is_playing()) {
		return 0;
	}

	return node->mix(p_buffer, p_rate_scale, p_frames);
}

void AudioStreamGraphOutputNode::stop() {
	if (node.is_valid()) {
		return node->stop();
	}
}

bool AudioStreamGraphOutputNode::is_playing() const {
	return node.is_valid() && node->is_playing();
}

int AudioStreamGraphOutputNode::get_loop_count() const {
	if (node.is_valid()) {
		return node->get_loop_count();
	}

	return 0;
}

double AudioStreamGraphOutputNode::get_playback_position() const {
	if (node.is_valid()) {
		return node->get_playback_position();
	}

	return 0.0;
}

String AudioStreamGraphRandomizerNode::get_caption() const {
	return "Randomizer";
}

String AudioStreamGraphRandomizerNode::get_description() const {
	return "Lorem Ipsum Randomizer";
}

int AudioStreamGraphRandomizerNode::get_input_port_count() const {
	return 2 * get_port_group_count();
}

AudioStreamGraphNode::PortType AudioStreamGraphRandomizerNode::get_input_port_type(int p_port) const {
	switch (p_port % 2) {
		case 0:
			return PORT_TYPE_SCALAR;
		case 1:
			return PORT_TYPE_STREAM;
		default:
			return PORT_TYPE_SCALAR;
	}
}

String AudioStreamGraphRandomizerNode::get_input_port_name(int p_port) const {
	switch (p_port % 2) {
		case 0:
			return "weight";
		case 1:
			return "input";
		default:
			return "weight";
	}
};

int AudioStreamGraphRandomizerNode::get_output_port_count() const {
	return 1;
}

AudioStreamGraphNode::PortType AudioStreamGraphRandomizerNode::get_output_port_type(int p_port) const {
	return PORT_TYPE_STREAM;
}

String AudioStreamGraphRandomizerNode::get_output_port_name(int p_port) const {
	return "out";
};

void AudioStreamGraphRandomizerNode::update_default_values() {
	for (int i = 0; i < get_port_group_count(); i++) {
		set_input_port_default_value(i * 2, 1.0, get_input_port_default_value(i * 2));
	}
}

void AudioStreamGraphRandomizerNode::start(double p_from_pos) {
	HashMap<int, Ref<AudioStreamGraphNodePlayback>> nodes = get_connected_input_playback_nodes();
	HashMap<int, Ref<AudioStreamGraphNodeParameter>> parameter_nodes = get_connected_input_parameter_nodes();
	int port_count = get_input_port_count();

	double total_weight = 0.0;
	for (int i = 0; i < port_count; i++) {
		if (parameter_nodes.has(i)) {
			Ref<AudioStreamGraphNodeFloatParameter> float_parameter = parameter_nodes[i];
			if (float_parameter.is_valid()) {
				total_weight += (double)float_parameter->get_value();
				continue;
			}
		}

		Variant result = get_input_port_default_value(i);
		if (result.get_type() == Variant::FLOAT) {
			total_weight += (double)result;
		}
	}

	double random_value = Math::random(0.0, total_weight);

	double cumulative_weight = 0.0;
	for (int i = 0; i < port_count; i++) {
		if (parameter_nodes.has(i)) {
			Ref<AudioStreamGraphNodeFloatParameter> float_parameter = parameter_nodes[i];
			if (float_parameter.is_valid()) {
				cumulative_weight += (double)float_parameter->get_value();
			}
		} else {
			Variant result = get_input_port_default_value(i);
			if (result.get_type() == Variant::FLOAT) {
				cumulative_weight += (double)result;
			}
		}
		if (random_value < cumulative_weight) {
			node = nodes[i + 1];
			ERR_BREAK(!node.is_valid());
			if (node->is_playing()) {
				node->stop();
			}

			node->start(p_from_pos);
			break;
		}
	}
}

int AudioStreamGraphRandomizerNode::mix(AudioFrame *p_buffer, float p_rate_scale, int p_frames) {
	if (!is_playing()) {
		return 0;
	}

	if (node->is_playing()) {
		return node->mix(p_buffer, p_rate_scale, p_frames);
	}

	return p_frames;
}

void AudioStreamGraphRandomizerNode::stop() {
	if (node.is_valid()) {
		node->stop();
	}
}

bool AudioStreamGraphRandomizerNode::is_playing() const {
	return node.is_valid() && node->is_playing();
}

int AudioStreamGraphRandomizerNode::get_loop_count() const {
	if (node.is_valid()) {
		return node->get_loop_count();
	}

	return 0;
}

double AudioStreamGraphRandomizerNode::get_playback_position() const {
	if (node.is_valid()) {
		return node->get_playback_position();
	}

	return 0.0;
}

AudioStreamGraphRandomizerNode::AudioStreamGraphRandomizerNode() {
	set_port_group_count(2);

	update_default_values();
}

String AudioStreamGraphMixerNode::get_caption() const {
	return "Mixer";
}

String AudioStreamGraphMixerNode::get_description() const {
	return "Lorem Ipsum Mixer";
}

int AudioStreamGraphMixerNode::get_input_port_count() const {
	return 2 * get_port_group_count();
}

AudioStreamGraphNode::PortType AudioStreamGraphMixerNode::get_input_port_type(int p_port) const {
	switch (p_port % 2) {
		case 0:
			return PORT_TYPE_SCALAR;
		case 1:
			return PORT_TYPE_STREAM;
		default:
			return PORT_TYPE_SCALAR;
	}
}

String AudioStreamGraphMixerNode::get_input_port_name(int p_port) const {
	switch (p_port % 2) {
		case 0:
			return "volume";
		case 1:
			return "input";
		default:
			return "volume";
	}
};

int AudioStreamGraphMixerNode::get_output_port_count() const {
	return 1;
}

AudioStreamGraphNode::PortType AudioStreamGraphMixerNode::get_output_port_type(int p_port) const {
	return PORT_TYPE_STREAM;
}

String AudioStreamGraphMixerNode::get_output_port_name(int p_port) const {
	return "out";
};

void AudioStreamGraphMixerNode::update_default_values() {
	for (int i = 0; i < get_port_group_count(); i++) {
		set_input_port_default_value(i * 2, 1.0, get_input_port_default_value(i * 2));
	}
}

void AudioStreamGraphMixerNode::start(double p_from_pos) {
	nodes = get_connected_input_playback_nodes();

	if (is_playing()) {
		stop();
	}

	for (const KeyValue<int, Ref<AudioStreamGraphNodePlayback>> &E : nodes) {
		E.value->start(p_from_pos);
	}
}

int AudioStreamGraphMixerNode::mix(AudioFrame *p_buffer, float p_rate_scale, int p_frames) {
	if (!is_playing()) {
		return 0;
	}

	int todo = p_frames;

	while (todo) {
		int to_mix = MIN(todo, MIX_BUFFER_SIZE);

		bool first = true;
		for (const KeyValue<int, Ref<AudioStreamGraphNodePlayback>> &E : nodes) {
			Ref<AudioStreamGraphNodePlayback> current_node = E.value;
			if (current_node.is_valid() && current_node->is_playing()) {
				float volume = get_input_port_default_value(E.key - 1);
				if (first) {
					current_node->mix(p_buffer, p_rate_scale, to_mix);
					for (int j = 0; j < to_mix; j++) {
						p_buffer[j] *= volume;
					}
					first = false;
				} else {
					current_node->mix(mix_buffer, p_rate_scale, to_mix);
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

	return p_frames;
}

void AudioStreamGraphMixerNode::stop() {
	for (const KeyValue<int, Ref<AudioStreamGraphNodePlayback>> &E : nodes) {
		Ref<AudioStreamGraphNodePlayback> current_node = E.value;
		if (current_node.is_valid()) {
			current_node->stop();
		}
	}
}

bool AudioStreamGraphMixerNode::is_playing() const {
	bool any_active = false;
	for (const KeyValue<int, Ref<AudioStreamGraphNodePlayback>> &E : nodes) {
		const Ref<AudioStreamGraphNodePlayback> node = E.value;
		if (node->is_playing()) {
			any_active = true;
			break;
		}
	}
	return any_active;
}

int AudioStreamGraphMixerNode::get_loop_count() const {
	int min_loops = 0;
	bool min_loops_found = false;
	for (const KeyValue<int, Ref<AudioStreamGraphNodePlayback>> &E : nodes) {
		Ref<AudioStreamGraphNodePlayback> current_node = E.value;
		if (current_node.is_valid() && current_node->is_playing()) {
			int loops = current_node->get_loop_count();
			if (!min_loops_found || loops < min_loops) {
				min_loops = loops;
				min_loops_found = true;
			}
		}
	}
	return min_loops;
}

double AudioStreamGraphMixerNode::get_playback_position() const {
	float max_pos = 0;
	bool pos_found = false;
	for (const KeyValue<int, Ref<AudioStreamGraphNodePlayback>> &E : nodes) {
		Ref<AudioStreamGraphNodePlayback> current_node = E.value;
		if (current_node.is_valid() && current_node->is_playing()) {
			float pos = current_node->get_playback_position();
			if (!pos_found || pos > max_pos) {
				max_pos = pos;
				pos_found = true;
			}
		}
	}
	return max_pos;
}

AudioStreamGraphMixerNode::AudioStreamGraphMixerNode() {
	set_port_group_count(2);
	update_default_values();
}

String AudioStreamGraphModulatorNode::get_caption() const {
	return "Modulator";
}

String AudioStreamGraphModulatorNode::get_description() const {
	return "Lorem Ipsum Modulator";
}

int AudioStreamGraphModulatorNode::get_input_port_count() const {
	return 5;
}

AudioStreamGraphNode::PortType AudioStreamGraphModulatorNode::get_input_port_type(int p_port) const {
	switch (p_port) {
		case 0:
		case 1:
		case 2:
		case 3:
			return PORT_TYPE_SCALAR;
		case 4:
			return PORT_TYPE_STREAM;
		default:
			return PORT_TYPE_STREAM;
	}
}

String AudioStreamGraphModulatorNode::get_input_port_name(int p_port) const {
	switch (p_port) {
		case 0:
			return "min volume";
		case 1:
			return "max volume";
		case 2:
			return "min pitch";
		case 3:
			return "max pitch";
		case 4:
			return "input";
		default:
			return "input";
	}
};

int AudioStreamGraphModulatorNode::get_output_port_count() const {
	return 1;
}

AudioStreamGraphNode::PortType AudioStreamGraphModulatorNode::get_output_port_type(int p_port) const {
	return PORT_TYPE_STREAM;
}

String AudioStreamGraphModulatorNode::get_output_port_name(int p_port) const {
	return "out";
};

void AudioStreamGraphModulatorNode::update_default_values() {
	set_input_port_default_value(0, 0.2, get_input_port_default_value(0));
	set_input_port_default_value(1, 1.0, get_input_port_default_value(1));
	set_input_port_default_value(2, 0.2, get_input_port_default_value(2));
	set_input_port_default_value(3, 1.0, get_input_port_default_value(3));
}

void AudioStreamGraphModulatorNode::start(double p_from_pos) {
	HashMap<int, Ref<AudioStreamGraphNodePlayback>> nodes = get_connected_input_playback_nodes();

	if (is_playing()) {
		stop();
	}

	if (nodes.size() == 1) {
		float min_volume = get_input_port_default_value(0);
		float max_volume = get_input_port_default_value(1);
		float min_pitch = get_input_port_default_value(2);
		float max_pitch = get_input_port_default_value(3);
		current_volume = Math::random(min_volume, max_volume);
		current_pitch = Math::random(min_pitch, max_pitch);

		node = nodes[4];
		ERR_FAIL_COND_MSG(!node.is_valid(), "Connected node is of type parameter");
		node->start(p_from_pos);
	}
}

int AudioStreamGraphModulatorNode::mix(AudioFrame *p_buffer, float p_rate_scale, int p_frames) {
	if (!is_playing()) {
		return 0;
	}

	if (node->is_playing()) {
		int mixed_samples = node->mix(p_buffer, p_rate_scale * current_pitch, p_frames);
		for (int samp = 0; samp < mixed_samples; samp++) {
			p_buffer[samp] *= current_volume;
		}
		return mixed_samples;
	} else {
		for (int i = 0; i < p_frames; i++) {
			p_buffer[i] = AudioFrame(0, 0);
		}
		return p_frames;
	}
}

void AudioStreamGraphModulatorNode::stop() {
	if (node.is_valid()) {
		node->stop();
	}
}

bool AudioStreamGraphModulatorNode::is_playing() const {
	return node.is_valid() && node->is_playing();
}

int AudioStreamGraphModulatorNode::get_loop_count() const {
	return node->get_loop_count();
}

double AudioStreamGraphModulatorNode::get_playback_position() const {
	return node->get_playback_position();
}

AudioStreamGraphModulatorNode::AudioStreamGraphModulatorNode() {
	update_default_values();
}

String AudioStreamGraphNodeFloatParameter::get_caption() const {
	return "FloatParameter";
}

String AudioStreamGraphNodeFloatParameter::get_description() const {
	return "Lorem ipsum";
}

void AudioStreamGraphNodeFloatParameter::update_default_values() {
}

int AudioStreamGraphNodeFloatParameter::get_input_port_count() const {
	return 0;
}

AudioStreamGraphNodeFloatParameter::PortType AudioStreamGraphNodeFloatParameter::get_input_port_type(int p_port) const {
	return PORT_TYPE_SCALAR;
}

String AudioStreamGraphNodeFloatParameter::get_input_port_name(int p_port) const {
	return String();
}

int AudioStreamGraphNodeFloatParameter::get_output_port_count() const {
	return 1;
}

AudioStreamGraphNodeFloatParameter::PortType AudioStreamGraphNodeFloatParameter::get_output_port_type(int p_port) const {
	return PORT_TYPE_SCALAR;
}

String AudioStreamGraphNodeFloatParameter::get_output_port_name(int p_port) const {
	return ""; //no output port means the editor will be used as port
}

bool AudioStreamGraphNodeFloatParameter::is_show_prop_names() const {
	return true;
}

// bool AudioStreamGraphNodeFloatParameter::is_use_prop_slots() const {
// 	return true;
// }

void AudioStreamGraphNodeFloatParameter::set_hint(Hint p_hint) {
	ERR_FAIL_INDEX(int(p_hint), int(HINT_MAX));
	if (hint == p_hint) {
		return;
	}
	hint = p_hint;
	emit_changed();
}

AudioStreamGraphNodeFloatParameter::Hint AudioStreamGraphNodeFloatParameter::get_hint() const {
	return hint;
}

void AudioStreamGraphNodeFloatParameter::set_min(float p_value) {
	if (Math::is_equal_approx(hint_range_min, p_value)) {
		return;
	}
	hint_range_min = p_value;
	emit_changed();
}

float AudioStreamGraphNodeFloatParameter::get_min() const {
	return hint_range_min;
}

void AudioStreamGraphNodeFloatParameter::set_max(float p_value) {
	if (Math::is_equal_approx(hint_range_max, p_value)) {
		return;
	}
	hint_range_max = p_value;
	emit_changed();
}

float AudioStreamGraphNodeFloatParameter::get_max() const {
	return hint_range_max;
}

void AudioStreamGraphNodeFloatParameter::set_step(float p_value) {
	if (Math::is_equal_approx(hint_range_step, p_value)) {
		return;
	}
	hint_range_step = p_value;
	emit_changed();
}

float AudioStreamGraphNodeFloatParameter::get_step() const {
	return hint_range_step;
}

void AudioStreamGraphNodeFloatParameter::set_default_value_enabled(bool p_enabled) {
	if (default_value_enabled == p_enabled) {
		return;
	}
	default_value_enabled = p_enabled;
	emit_changed();
}

bool AudioStreamGraphNodeFloatParameter::is_default_value_enabled() const {
	return default_value_enabled;
}

void AudioStreamGraphNodeFloatParameter::set_default_value(float p_value) {
	if (Math::is_equal_approx(default_value, p_value)) {
		return;
	}
	default_value = p_value;
	emit_changed();
}

float AudioStreamGraphNodeFloatParameter::get_default_value() const {
	return default_value;
}

void AudioStreamGraphNodeFloatParameter::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_hint", "hint"), &AudioStreamGraphNodeFloatParameter::set_hint);
	ClassDB::bind_method(D_METHOD("get_hint"), &AudioStreamGraphNodeFloatParameter::get_hint);

	ClassDB::bind_method(D_METHOD("set_min", "value"), &AudioStreamGraphNodeFloatParameter::set_min);
	ClassDB::bind_method(D_METHOD("get_min"), &AudioStreamGraphNodeFloatParameter::get_min);

	ClassDB::bind_method(D_METHOD("set_max", "value"), &AudioStreamGraphNodeFloatParameter::set_max);
	ClassDB::bind_method(D_METHOD("get_max"), &AudioStreamGraphNodeFloatParameter::get_max);

	ClassDB::bind_method(D_METHOD("set_step", "value"), &AudioStreamGraphNodeFloatParameter::set_step);
	ClassDB::bind_method(D_METHOD("get_step"), &AudioStreamGraphNodeFloatParameter::get_step);

	ClassDB::bind_method(D_METHOD("set_default_value_enabled", "enabled"), &AudioStreamGraphNodeFloatParameter::set_default_value_enabled);
	ClassDB::bind_method(D_METHOD("is_default_value_enabled"), &AudioStreamGraphNodeFloatParameter::is_default_value_enabled);

	ClassDB::bind_method(D_METHOD("set_default_value", "value"), &AudioStreamGraphNodeFloatParameter::set_default_value);
	ClassDB::bind_method(D_METHOD("get_default_value"), &AudioStreamGraphNodeFloatParameter::get_default_value);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "hint", PROPERTY_HINT_ENUM, "None,Range,Range+Step"), "set_hint", "get_hint");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "min"), "set_min", "get_min");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "max"), "set_max", "get_max");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "step"), "set_step", "get_step");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "default_value_enabled"), "set_default_value_enabled", "is_default_value_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "default_value"), "set_default_value", "get_default_value");

	BIND_ENUM_CONSTANT(HINT_NONE);
	BIND_ENUM_CONSTANT(HINT_RANGE);
	BIND_ENUM_CONSTANT(HINT_RANGE_STEP);
	BIND_ENUM_CONSTANT(HINT_MAX);
}

// bool AudioStreamGraphNodeFloatParameter::is_convertible_to_constant() const {
// 	return true; // conversion is allowed
// }

Vector<StringName> AudioStreamGraphNodeFloatParameter::get_editable_properties() const {
	Vector<StringName> props = {};
	props.push_back("hint");
	if (hint == HINT_RANGE || hint == HINT_RANGE_STEP) {
		props.push_back("min");
		props.push_back("max");
	}
	if (hint == HINT_RANGE_STEP) {
		props.push_back("step");
	}
	props.push_back("default_value_enabled");
	if (default_value_enabled) {
		props.push_back("default_value");
	}
	return props;
}

AudioStreamGraphNodeFloatParameter::AudioStreamGraphNodeFloatParameter() {
	set_value(0.0);
}

/**************************************************************************/
/*  audio_stream_graph_nodes.h                                            */
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

#ifndef AUDIO_STREAM_GRAPH_NODES_H
#define AUDIO_STREAM_GRAPH_NODES_H

#include "audio_stream_graph.h"

class AudioStreamGraphInputNode : public AudioStreamGraphNodePlayback {
	GDCLASS(AudioStreamGraphInputNode, AudioStreamGraphNodePlayback);

	virtual String get_caption() const override;
	virtual String get_description() const override;

	virtual int get_input_port_count() const override;
	virtual PortType get_input_port_type(int p_port) const override;
	virtual String get_input_port_name(int p_port) const override;

	virtual int get_output_port_count() const override;
	virtual PortType get_output_port_type(int p_port) const override;
	virtual String get_output_port_name(int p_port) const override;

	virtual bool is_show_prop_names() const override;
	virtual Vector<StringName> get_editable_properties() const override;

	virtual void update_default_values() override;

	virtual void start(double p_from_pos) override;
	virtual int mix(AudioFrame *p_buffer, float p_rate_scale, int p_frames) override;
	virtual void stop() override;
	virtual bool is_playing() const override;
	virtual int get_loop_count() const override; // times it looped
	virtual double get_playback_position() const override;

private:
	Ref<AudioStream> stream;
	Ref<AudioStreamPlayback> playback;

public:
	void set_stream(Ref<AudioStream> p_stream);
	Ref<AudioStream> get_stream() const;

protected:
	static void _bind_methods();
};

class AudioStreamGraphOutputNode : public AudioStreamGraphNodePlayback {
	GDCLASS(AudioStreamGraphOutputNode, AudioStreamGraphNodePlayback);

	virtual String get_caption() const override;
	virtual String get_description() const override;

	virtual int get_input_port_count() const override;
	virtual PortType get_input_port_type(int p_port) const override;
	virtual String get_input_port_name(int p_port) const override;

	virtual int get_output_port_count() const override;
	virtual PortType get_output_port_type(int p_port) const override;
	virtual String get_output_port_name(int p_port) const override;

	virtual void update_default_values() override;

	virtual void start(double p_from_pos) override;
	virtual int mix(AudioFrame *p_buffer, float p_rate_scale, int p_frames) override;
	virtual void stop() override;
	virtual bool is_playing() const override;
	virtual int get_loop_count() const override; // times it looped
	virtual double get_playback_position() const override;

private:
	Ref<AudioStreamGraphNodePlayback> node;
};

class AudioStreamGraphRandomizerNode : public AudioStreamGraphNodePlayback {
	GDCLASS(AudioStreamGraphRandomizerNode, AudioStreamGraphNodePlayback);

	virtual String get_caption() const override;
	virtual String get_description() const override;

	virtual int get_input_port_count() const override;
	virtual PortType get_input_port_type(int p_port) const override;
	virtual String get_input_port_name(int p_port) const override;

	virtual int get_output_port_count() const override;
	virtual PortType get_output_port_type(int p_port) const override;
	virtual String get_output_port_name(int p_port) const override;

	virtual void update_default_values() override;

	virtual void start(double p_from_pos) override;
	virtual int mix(AudioFrame *p_buffer, float p_rate_scale, int p_frames) override;
	virtual void stop() override;
	virtual bool is_playing() const override;
	virtual int get_loop_count() const override; // times it looped
	virtual double get_playback_position() const override;

private:
	Ref<AudioStreamGraphNodePlayback> node;
	AudioStreamGraphRandomizerNode();
};

class AudioStreamGraphMixerNode : public AudioStreamGraphNodePlayback {
	GDCLASS(AudioStreamGraphMixerNode, AudioStreamGraphNodePlayback);

	virtual String get_caption() const override;
	virtual String get_description() const override;

	virtual int get_input_port_count() const override;
	virtual PortType get_input_port_type(int p_port) const override;
	virtual String get_input_port_name(int p_port) const override;

	virtual int get_output_port_count() const override;
	virtual PortType get_output_port_type(int p_port) const override;
	virtual String get_output_port_name(int p_port) const override;

	virtual void update_default_values() override;

	virtual void start(double p_from_pos) override;
	virtual int mix(AudioFrame *p_buffer, float p_rate_scale, int p_frames) override;
	virtual void stop() override;
	virtual bool is_playing() const override;
	virtual int get_loop_count() const override; // times it looped
	virtual double get_playback_position() const override;

private:
	HashMap<int, Ref<AudioStreamGraphNodePlayback>> nodes;
	enum {
		MIX_BUFFER_SIZE = 1024
	};
	AudioFrame mix_buffer[MIX_BUFFER_SIZE];

	AudioStreamGraphMixerNode();
};

class AudioStreamGraphModulatorNode : public AudioStreamGraphNodePlayback {
	GDCLASS(AudioStreamGraphModulatorNode, AudioStreamGraphNodePlayback);

	virtual String get_caption() const override;
	virtual String get_description() const override;

	virtual int get_input_port_count() const override;
	virtual PortType get_input_port_type(int p_port) const override;
	virtual String get_input_port_name(int p_port) const override;

	virtual int get_output_port_count() const override;
	virtual PortType get_output_port_type(int p_port) const override;
	virtual String get_output_port_name(int p_port) const override;

	virtual void update_default_values() override;

	virtual void start(double p_from_pos) override;
	virtual int mix(AudioFrame *p_buffer, float p_rate_scale, int p_frames) override;
	virtual void stop() override;
	virtual bool is_playing() const override;
	virtual int get_loop_count() const override; // times it looped
	virtual double get_playback_position() const override;

	AudioStreamGraphModulatorNode();

private:
	Ref<AudioStreamGraphNodePlayback> node;
	float current_volume = 0.0;
	float current_pitch = 0.0;
};

class AudioStreamGraphNodeFloatParameter : public AudioStreamGraphNodeParameter {
	GDCLASS(AudioStreamGraphNodeFloatParameter, AudioStreamGraphNodeParameter);

public:
	enum Hint {
		HINT_NONE,
		HINT_RANGE,
		HINT_RANGE_STEP,
		HINT_MAX,
	};

private:
	Hint hint = HINT_NONE;
	float hint_range_min = 0.0f;
	float hint_range_max = 1.0f;
	float hint_range_step = 0.1f;
	bool default_value_enabled = false;
	float default_value = 0.0f;

protected:
	static void _bind_methods();

public:
	virtual String get_caption() const override;
	virtual String get_description() const override;

	virtual int get_input_port_count() const override;
	virtual PortType get_input_port_type(int p_port) const override;
	virtual String get_input_port_name(int p_port) const override;

	virtual int get_output_port_count() const override;
	virtual PortType get_output_port_type(int p_port) const override;
	virtual String get_output_port_name(int p_port) const override;

	virtual void update_default_values() override;

	virtual bool is_show_prop_names() const override;
	//virtual bool is_use_prop_slots() const override;

	void set_hint(Hint p_hint);
	Hint get_hint() const;

	void set_min(float p_value);
	float get_min() const;

	void set_max(float p_value);
	float get_max() const;

	void set_step(float p_value);
	float get_step() const;

	void set_default_value_enabled(bool p_enabled);
	bool is_default_value_enabled() const;

	void set_default_value(float p_value);
	float get_default_value() const;

	//bool is_convertible_to_constant() const override;

	virtual Vector<StringName> get_editable_properties() const override;

	AudioStreamGraphNodeFloatParameter();
};

VARIANT_ENUM_CAST(AudioStreamGraphNodeFloatParameter::Hint)
#endif // AUDIO_STREAM_GRAPH_NODES_H
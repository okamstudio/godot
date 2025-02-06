/**************************************************************************/
/*  gltf_audio_player.h                                                   */
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

#ifndef GLTF_AUDIO_PLAYER_H
#define GLTF_AUDIO_PLAYER_H

#include "../../gltf_defines.h"
#include "core/io/resource.h"

class AudioStream;
class AudioStreamPlayer;
class AudioStreamPlayer3D;

using GLTFAudioSourceIndex = int;

// GLTFAudioPlayer is an intermediary between GLTF audio and Godot's audio player nodes.
// https://github.com/omigroup/gltf-extensions/tree/main/extensions/2.0/KHR_audio

class GLTFAudioPlayer : public Resource {
	GDCLASS(GLTFAudioPlayer, Resource)

public:
	enum EmitterType {
		EMITTER_TYPE_GLOBAL,
		EMITTER_TYPE_POSITIONAL,
	};

protected:
	static void _bind_methods();

private:
	// General audio properties.
	EmitterType emitter_type = EmitterType::EMITTER_TYPE_POSITIONAL;
	Vector<GLTFAudioSourceIndex> audio_sources;
	Ref<AudioStream> audio_stream;
	real_t pitch_speed = 1.0;
	real_t volume_gain = 1.0;
	bool auto_play = false;
	// Distance attenuation.
	String distance_model = "inverse";
	real_t max_distance = 0.0;
	real_t unit_distance = 1.0;
	real_t rolloff_factor = 1.0;
	// Cone attenuation. All angles are in radians.
	String shape_type = "omnidirectional";
	real_t cone_inner_angle = Math_TAU;
	real_t cone_outer_angle = Math_TAU;
	real_t cone_outer_gain = 0.0;

public:
	// General audio properties.
	EmitterType get_emitter_type() const;
	void set_emitter_type(EmitterType p_emitter_type);

	Vector<GLTFAudioSourceIndex> get_audio_sources() const;
	void set_audio_sources(const Vector<GLTFAudioSourceIndex> &p_audio_sources);

	Ref<AudioStream> get_audio_stream() const;
	void set_audio_stream(const Ref<AudioStream> p_audio_stream);

	bool get_auto_play() const;
	void set_auto_play(bool p_auto_play);

	real_t get_pitch_speed() const;
	void set_pitch_speed(real_t p_pitch_speed);

	real_t get_volume_gain() const;
	void set_volume_gain(real_t p_volume_gain);

	// Distance attenuation.
	String get_distance_model() const;
	void set_distance_model(const String &p_distance_model);

	real_t get_max_distance() const;
	void set_max_distance(real_t p_max_distance);

	real_t get_unit_distance() const;
	void set_unit_distance(real_t p_unit_distance);

	real_t get_rolloff_factor() const;
	void set_rolloff_factor(real_t p_rolloff_factor);

	// Cone attenuation. All angles are in radians.
	String get_shape_type() const;
	void set_shape_type(const String &p_shape_type);

	real_t get_cone_inner_angle() const;
	void set_cone_inner_angle(real_t p_cone_inner_angle);

	real_t get_cone_outer_angle() const;
	void set_cone_outer_angle(real_t p_cone_outer_angle);

	real_t get_cone_outer_gain() const;
	void set_cone_outer_gain(real_t p_cone_outer_gain);

	static Ref<GLTFAudioPlayer> from_node_0d(const AudioStreamPlayer *p_audio_node);
	static Ref<GLTFAudioPlayer> from_node_3d(const AudioStreamPlayer3D *p_audio_node);
	static Ref<GLTFAudioPlayer> from_node(const Node *p_audio_node);

	AudioStreamPlayer *to_node_0d();
	AudioStreamPlayer3D *to_node_3d();
	Node *to_node();

	static Ref<GLTFAudioPlayer> from_dictionary(const Dictionary &p_dictionary);
	Dictionary to_dictionary() const;
};

VARIANT_ENUM_CAST(GLTFAudioPlayer::EmitterType);

#endif // GLTF_AUDIO_PLAYER_H

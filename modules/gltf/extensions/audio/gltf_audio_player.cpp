/**************************************************************************/
/*  gltf_audio_player.cpp                                                 */
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

#include "gltf_audio_player.h"

#include "../../gltf_state.h"
#include "../../gltf_template_convert.h"
#include "scene/3d/audio_stream_player_3d.h"
#include "scene/audio/audio_stream_player.h"

void GLTFAudioPlayer::_bind_methods() {
	ClassDB::bind_static_method("GLTFAudioPlayer", D_METHOD("from_node_0d", "audio_node"), &GLTFAudioPlayer::from_node_0d);
	ClassDB::bind_static_method("GLTFAudioPlayer", D_METHOD("from_node_3d", "audio_node"), &GLTFAudioPlayer::from_node_3d);
	ClassDB::bind_static_method("GLTFAudioPlayer", D_METHOD("from_node", "audio_node"), &GLTFAudioPlayer::from_node);

	ClassDB::bind_method(D_METHOD("to_node_0d"), &GLTFAudioPlayer::to_node_0d);
	ClassDB::bind_method(D_METHOD("to_node_3d"), &GLTFAudioPlayer::to_node_3d);
	ClassDB::bind_method(D_METHOD("to_node"), &GLTFAudioPlayer::to_node, DEFVAL(false));

	ClassDB::bind_static_method("GLTFAudioPlayer", D_METHOD("from_dictionary", "dictionary"), &GLTFAudioPlayer::from_dictionary);
	ClassDB::bind_method(D_METHOD("to_dictionary"), &GLTFAudioPlayer::to_dictionary);

	// General audio properties.
	ClassDB::bind_method(D_METHOD("get_emitter_type"), &GLTFAudioPlayer::get_emitter_type);
	ClassDB::bind_method(D_METHOD("set_emitter_type", "emitter_type"), &GLTFAudioPlayer::set_emitter_type);

	ClassDB::bind_method(D_METHOD("get_audio_sources"), &GLTFAudioPlayer::get_audio_sources);
	ClassDB::bind_method(D_METHOD("set_audio_sources", "audio_sources"), &GLTFAudioPlayer::set_audio_sources);

	ClassDB::bind_method(D_METHOD("get_audio_stream"), &GLTFAudioPlayer::get_audio_stream);
	ClassDB::bind_method(D_METHOD("set_audio_stream", "audio_stream"), &GLTFAudioPlayer::set_audio_stream);

	ClassDB::bind_method(D_METHOD("get_pitch_speed"), &GLTFAudioPlayer::get_pitch_speed);
	ClassDB::bind_method(D_METHOD("set_pitch_speed", "pitch_speed"), &GLTFAudioPlayer::set_pitch_speed);

	ClassDB::bind_method(D_METHOD("get_volume_gain"), &GLTFAudioPlayer::get_volume_gain);
	ClassDB::bind_method(D_METHOD("set_volume_gain", "volume_gain"), &GLTFAudioPlayer::set_volume_gain);

	ClassDB::bind_method(D_METHOD("get_auto_play"), &GLTFAudioPlayer::get_auto_play);
	ClassDB::bind_method(D_METHOD("set_auto_play", "auto_play"), &GLTFAudioPlayer::set_auto_play);

	// Distance attenuation.
	ClassDB::bind_method(D_METHOD("get_distance_model"), &GLTFAudioPlayer::get_distance_model);
	ClassDB::bind_method(D_METHOD("set_distance_model", "distance_model"), &GLTFAudioPlayer::set_distance_model);

	ClassDB::bind_method(D_METHOD("get_max_distance"), &GLTFAudioPlayer::get_max_distance);
	ClassDB::bind_method(D_METHOD("set_max_distance", "max_distance"), &GLTFAudioPlayer::set_max_distance);

	ClassDB::bind_method(D_METHOD("get_unit_distance"), &GLTFAudioPlayer::get_unit_distance);
	ClassDB::bind_method(D_METHOD("set_unit_distance", "unit_distance"), &GLTFAudioPlayer::set_unit_distance);

	ClassDB::bind_method(D_METHOD("get_rolloff_factor"), &GLTFAudioPlayer::get_rolloff_factor);
	ClassDB::bind_method(D_METHOD("set_rolloff_factor", "rolloff_factor"), &GLTFAudioPlayer::set_rolloff_factor);

	// Cone attenuation. All angles are in radians.
	ClassDB::bind_method(D_METHOD("get_shape_type"), &GLTFAudioPlayer::get_shape_type);
	ClassDB::bind_method(D_METHOD("set_shape_type", "shape_type"), &GLTFAudioPlayer::set_shape_type);

	ClassDB::bind_method(D_METHOD("get_cone_inner_angle"), &GLTFAudioPlayer::get_cone_inner_angle);
	ClassDB::bind_method(D_METHOD("set_cone_inner_angle", "cone_inner_angle"), &GLTFAudioPlayer::set_cone_inner_angle);

	ClassDB::bind_method(D_METHOD("get_cone_outer_angle"), &GLTFAudioPlayer::get_cone_outer_angle);
	ClassDB::bind_method(D_METHOD("set_cone_outer_angle", "cone_outer_angle"), &GLTFAudioPlayer::set_cone_outer_angle);

	ClassDB::bind_method(D_METHOD("get_cone_outer_gain"), &GLTFAudioPlayer::get_cone_outer_gain);
	ClassDB::bind_method(D_METHOD("set_cone_outer_gain", "cone_outer_gain"), &GLTFAudioPlayer::set_cone_outer_gain);

	// General audio properties.
	ADD_PROPERTY(PropertyInfo(Variant::INT, "emitter_type"), "set_emitter_type", "get_emitter_type");
	ADD_PROPERTY(PropertyInfo(Variant::PACKED_INT32_ARRAY, "audio_sources"), "set_audio_sources", "get_audio_sources");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "audio_stream", PROPERTY_HINT_RESOURCE_TYPE, "AudioStream"), "set_audio_stream", "get_audio_stream");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "auto_play"), "set_auto_play", "get_auto_play");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "pitch_speed"), "set_pitch_speed", "get_pitch_speed");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "volume_gain"), "set_volume_gain", "get_volume_gain");

	// Distance attenuation.
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "distance_model"), "set_distance_model", "get_distance_model");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "max_distance"), "set_max_distance", "get_max_distance");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "unit_distance"), "set_unit_distance", "get_unit_distance");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "rolloff_factor"), "set_rolloff_factor", "get_rolloff_factor");

	// Cone attenuation. All angles are in radians.
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "shape_type"), "set_shape_type", "get_shape_type");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "cone_inner_angle"), "set_cone_inner_angle", "get_cone_inner_angle");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "cone_outer_angle"), "set_cone_outer_angle", "get_cone_outer_angle");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "cone_outer_gain"), "set_cone_outer_gain", "get_cone_outer_gain");

	// Emitter type enum.
	BIND_ENUM_CONSTANT(EMITTER_TYPE_GLOBAL);
	BIND_ENUM_CONSTANT(EMITTER_TYPE_POSITIONAL);
}

// General audio properties.

GLTFAudioPlayer::EmitterType GLTFAudioPlayer::get_emitter_type() const {
	return emitter_type;
}

void GLTFAudioPlayer::set_emitter_type(EmitterType p_emitter_type) {
	emitter_type = p_emitter_type;
}

Vector<GLTFAudioSourceIndex> GLTFAudioPlayer::get_audio_sources() const {
	return audio_sources;
}

void GLTFAudioPlayer::set_audio_sources(const Vector<GLTFAudioSourceIndex> &p_audio_sources) {
	audio_sources = p_audio_sources;
}

Ref<AudioStream> GLTFAudioPlayer::get_audio_stream() const {
	return audio_stream;
}

void GLTFAudioPlayer::set_audio_stream(const Ref<AudioStream> p_audio_stream) {
	audio_stream = p_audio_stream;
}

bool GLTFAudioPlayer::get_auto_play() const {
	return auto_play;
}

void GLTFAudioPlayer::set_auto_play(bool p_auto_play) {
	auto_play = p_auto_play;
}

real_t GLTFAudioPlayer::get_pitch_speed() const {
	return pitch_speed;
}

void GLTFAudioPlayer::set_pitch_speed(real_t p_pitch_speed) {
	pitch_speed = p_pitch_speed;
}

real_t GLTFAudioPlayer::get_volume_gain() const {
	return volume_gain;
}

void GLTFAudioPlayer::set_volume_gain(real_t p_volume_gain) {
	volume_gain = p_volume_gain;
}

// Distance attenuation.

String GLTFAudioPlayer::get_distance_model() const {
	return distance_model;
}

void GLTFAudioPlayer::set_distance_model(const String &p_distance_model) {
	distance_model = p_distance_model;
}

real_t GLTFAudioPlayer::get_max_distance() const {
	return max_distance;
}

void GLTFAudioPlayer::set_max_distance(real_t p_max_distance) {
	max_distance = p_max_distance;
}

real_t GLTFAudioPlayer::get_unit_distance() const {
	return unit_distance;
}

void GLTFAudioPlayer::set_unit_distance(real_t p_unit_distance) {
	unit_distance = p_unit_distance;
}

real_t GLTFAudioPlayer::get_rolloff_factor() const {
	return rolloff_factor;
}

void GLTFAudioPlayer::set_rolloff_factor(real_t p_rolloff_factor) {
	rolloff_factor = p_rolloff_factor;
}

// Cone attenuation. All angles are in radians.

String GLTFAudioPlayer::get_shape_type() const {
	return shape_type;
}

void GLTFAudioPlayer::set_shape_type(const String &p_shape_type) {
	shape_type = p_shape_type;
}

real_t GLTFAudioPlayer::get_cone_inner_angle() const {
	return cone_inner_angle;
}

void GLTFAudioPlayer::set_cone_inner_angle(real_t p_cone_inner_angle) {
	cone_inner_angle = p_cone_inner_angle;
}

real_t GLTFAudioPlayer::get_cone_outer_angle() const {
	return cone_outer_angle;
}

void GLTFAudioPlayer::set_cone_outer_angle(real_t p_cone_outer_angle) {
	cone_outer_angle = p_cone_outer_angle;
}

real_t GLTFAudioPlayer::get_cone_outer_gain() const {
	return cone_outer_gain;
}

void GLTFAudioPlayer::set_cone_outer_gain(real_t p_cone_outer_gain) {
	cone_outer_gain = p_cone_outer_gain;
}

Ref<GLTFAudioPlayer> GLTFAudioPlayer::from_node_0d(const AudioStreamPlayer *p_audio_player_node) {
	Ref<GLTFAudioPlayer> audio_player;
	audio_player.instantiate();
	audio_player->set_emitter_type(GLTFAudioPlayer::EmitterType::EMITTER_TYPE_GLOBAL);
	audio_player->set_audio_stream(p_audio_player_node->get_stream());
	audio_player->set_pitch_speed(p_audio_player_node->get_pitch_scale());
	audio_player->set_volume_gain(Math::db_to_linear(p_audio_player_node->get_volume_db()));
	audio_player->set_auto_play(p_audio_player_node->is_autoplay_enabled());
	audio_player->set_name(p_audio_player_node->get_name());
	return audio_player;
}

Ref<GLTFAudioPlayer> GLTFAudioPlayer::from_node_3d(const AudioStreamPlayer3D *p_audio_player_node) {
	Ref<GLTFAudioPlayer> audio_player;
	audio_player.instantiate();
	audio_player->set_emitter_type(GLTFAudioPlayer::EmitterType::EMITTER_TYPE_POSITIONAL);
	audio_player->set_audio_stream(p_audio_player_node->get_stream());
	audio_player->set_pitch_speed(p_audio_player_node->get_pitch_scale());
	audio_player->set_volume_gain(Math::db_to_linear(p_audio_player_node->get_volume_db()));
	audio_player->set_auto_play(p_audio_player_node->is_autoplay_enabled());
	audio_player->set_name(p_audio_player_node->get_name());
	// Distance attenuation.
	audio_player->set_max_distance(p_audio_player_node->get_max_distance());
	audio_player->set_unit_distance(p_audio_player_node->get_unit_size());
	audio_player->set_distance_model("inverse");
	const AudioStreamPlayer3D::AttenuationModel attenuation_model = p_audio_player_node->get_attenuation_model();
	switch (attenuation_model) {
		case AudioStreamPlayer3D::ATTENUATION_INVERSE_DISTANCE:
			audio_player->set_rolloff_factor(1.0);
			break;
		case AudioStreamPlayer3D::ATTENUATION_INVERSE_SQUARE_DISTANCE:
			audio_player->set_rolloff_factor(2.0);
			break;
		case AudioStreamPlayer3D::ATTENUATION_LOGARITHMIC:
			ERR_PRINT("GLTF audio: Logarithmic attenuation is not supported by GLTF audio. Falling back to inverse.");
			audio_player->set_rolloff_factor(1.0);
			break;
		case AudioStreamPlayer3D::ATTENUATION_DISABLED:
			audio_player->set_rolloff_factor(0.0);
			break;
	}
	// Cone attenuation. Godot only has one cone angle.
	if (p_audio_player_node->is_emission_angle_enabled()) {
		audio_player->set_shape_type("cone");
		const real_t cone_angle = Math::deg_to_rad(p_audio_player_node->get_emission_angle()) * 2.0f;
		audio_player->set_cone_inner_angle(cone_angle);
		audio_player->set_cone_outer_angle(cone_angle);
		audio_player->set_cone_outer_gain(Math::db_to_linear(p_audio_player_node->get_emission_angle_filter_attenuation_db()));
	}
	return audio_player;
}

Ref<GLTFAudioPlayer> GLTFAudioPlayer::from_node(const Node *p_audio_player_node) {
	Ref<GLTFAudioPlayer> audio_player;
	if (cast_to<const AudioStreamPlayer>(p_audio_player_node)) {
		audio_player = from_node_0d(cast_to<const AudioStreamPlayer>(p_audio_player_node));
	} else if (cast_to<const AudioStreamPlayer3D>(p_audio_player_node)) {
		audio_player = from_node_3d(cast_to<const AudioStreamPlayer3D>(p_audio_player_node));
	} else {
		ERR_PRINT("Tried to create a GLTFAudioPlayer from a node, but the given node was not an AudioStreamPlayer or AudioStreamPlayer3D.");
	}
	return audio_player;
}

AudioStreamPlayer *GLTFAudioPlayer::to_node_0d() {
	AudioStreamPlayer *audio_node = memnew(AudioStreamPlayer);
	audio_node->set_stream(audio_stream);
	audio_node->set_pitch_scale(pitch_speed);
	audio_node->set_volume_db(Math::linear_to_db(volume_gain));
	audio_node->set_autoplay(auto_play);
	if (get_name().is_empty()) {
		audio_node->set_name("GlobalAudioPlayer");
	} else {
		audio_node->set_name(get_name());
	}
	return audio_node;
}

AudioStreamPlayer3D *GLTFAudioPlayer::to_node_3d() {
	AudioStreamPlayer3D *audio_node = memnew(AudioStreamPlayer3D);
	audio_node->set_stream(audio_stream);
	audio_node->set_pitch_scale(pitch_speed);
	audio_node->set_volume_db(Math::linear_to_db(volume_gain));
	audio_node->set_max_db(Math::linear_to_db(volume_gain));
	audio_node->set_autoplay(auto_play);
	if (get_name().is_empty()) {
		audio_node->set_name("PositionalAudioPlayer");
	} else {
		audio_node->set_name(get_name());
	}
	// Distance attenuation.
	audio_node->set_max_distance(max_distance);
	audio_node->set_unit_size(unit_distance);
	if (distance_model != "inverse") {
		WARN_PRINT("GLTF audio: A distance model of " + distance_model + " was specified in the GLTF data, but Godot only supports 'inverse'. Falling back to 'inverse'.");
	}
	if (rolloff_factor < 0.25f) {
		audio_node->set_attenuation_model(AudioStreamPlayer3D::ATTENUATION_DISABLED);
		if (!Math::is_zero_approx(rolloff_factor)) {
			WARN_PRINT("GLTF audio: A rolloff factor of " + rtos(rolloff_factor) + " was specified in the GLTF data, but Godot only supports 0, 1, and 2. Falling back to 0 (no attenuation).");
		}
	} else if (rolloff_factor < 1.5f) {
		audio_node->set_attenuation_model(AudioStreamPlayer3D::ATTENUATION_INVERSE_DISTANCE);
		if (!Math::is_equal_approx(rolloff_factor, 1)) {
			WARN_PRINT("GLTF audio: A rolloff factor of " + rtos(rolloff_factor) + " was specified in the GLTF data, but Godot only supports 0, 1, and 2. Falling back to 1 (inverse attenuation).");
		}
	} else {
		audio_node->set_attenuation_model(AudioStreamPlayer3D::ATTENUATION_INVERSE_SQUARE_DISTANCE);
		if (!Math::is_equal_approx(rolloff_factor, 2)) {
			WARN_PRINT("GLTF audio: A rolloff factor of " + rtos(rolloff_factor) + " was specified in the GLTF data, but Godot only supports 0, 1, and 2. Falling back to 2 (inverse squared attenuation).");
		}
	}
	// Cone attenuation. Godot only has one cone angle.
	real_t emission_angle_radians = (cone_inner_angle + cone_outer_angle) / 4.0f;
	// Note: Don't use TAU or PI in checks to account for floating point errors.
	if (emission_angle_radians < 3.14f) {
		if (emission_angle_radians > 1.58f) {
			WARN_PRINT("GLTF audio: An emission angular radius of " + rtos(emission_angle_radians) + " radians was determined from the GLTF data (half the average of cone inner angular diameter " + rtos(cone_inner_angle) + " and cone outer angular diameter " + rtos(cone_outer_angle) + "), but Godot only supports 0 to 90 degrees (1.570796... radians). Falling back to 90 degrees.");
		}
		audio_node->set_emission_angle(MIN(Math::rad_to_deg(emission_angle_radians), 90.0f));
		if (shape_type == "cone") {
			audio_node->set_emission_angle_enabled(true);
		}
	}
	audio_node->set_emission_angle_filter_attenuation_db(Math::linear_to_db(cone_outer_gain));
	return audio_node;
}

Node *GLTFAudioPlayer::to_node() {
	if (emitter_type == EmitterType::EMITTER_TYPE_GLOBAL) {
		return to_node_0d();
	}
	AudioStreamPlayer3D *audio_node_3d = to_node_3d();
	return audio_node_3d;
}

Ref<GLTFAudioPlayer> GLTFAudioPlayer::from_dictionary(const Dictionary &p_dictionary) {
	ERR_FAIL_COND_V_MSG(!p_dictionary.has("type"), Ref<GLTFAudioPlayer>(), "Failed to parse GLTF audio, missing required field 'type'.");
	Ref<GLTFAudioPlayer> audio;
	audio.instantiate();
	const String &emitter_type_string = p_dictionary["type"];
	// KHR_audio_emitter has "global" and "positional".
	if (emitter_type_string == "global") {
		audio->set_emitter_type(EmitterType::EMITTER_TYPE_GLOBAL);
	} else if (emitter_type_string == "positional") {
		audio->set_emitter_type(EmitterType::EMITTER_TYPE_POSITIONAL);
	} else {
		ERR_PRINT("Error parsing GLTF audio: Player type '" + emitter_type_string + "' is unknown.");
	}
	Vector<int> audio_sources;
	Array sources = p_dictionary["sources"];
	GLTFTemplateConvert::set_from_array(audio_sources, sources);
	audio->set_audio_sources(audio_sources);
	audio->set_volume_gain(p_dictionary.get("gain", 1.0));
	audio->set_name(p_dictionary.get("name", ""));
	if (p_dictionary.has("positional")) {
		const Dictionary &positional = p_dictionary["positional"];
		// Distance attenuation.
		audio->set_distance_model(positional.get("distanceModel", "inverse"));
		audio->set_max_distance(positional.get("maxDistance", 0.0));
		audio->set_unit_distance(positional.get("refDistance", 1.0));
		audio->set_rolloff_factor(positional.get("rolloffFactor", 1.0));
		// Cone attenuation.
		audio->set_cone_inner_angle(positional.get("coneInnerAngle", Math_TAU));
		audio->set_cone_outer_angle(positional.get("coneOuterAngle", Math_TAU));
		audio->set_cone_outer_gain(positional.get("coneOuterGain", 0.0));
		if (positional.has("shapeType")) {
			audio->set_shape_type(positional["shapeType"]);
		} else if (audio->get_cone_outer_angle() < 6.28) {
			audio->set_shape_type("cone");
		}
	}
	return audio;
}

Dictionary GLTFAudioPlayer::to_dictionary() const {
	Dictionary dict;
	dict["sources"] = GLTFTemplateConvert::to_array(audio_sources);
	dict["gain"] = volume_gain;
	if (!get_name().is_empty()) {
		dict["name"] = get_name();
	}
	if (emitter_type == EmitterType::EMITTER_TYPE_POSITIONAL) {
		Dictionary positional;
		if (cone_inner_angle < Math_TAU) {
			positional["coneInnerAngle"] = cone_inner_angle;
		}
		if (cone_outer_angle < Math_TAU) {
			positional["coneOuterAngle"] = cone_outer_angle;
		}
		if (cone_outer_gain != 0.0) {
			positional["coneOuterGain"] = cone_outer_gain;
		}
		if (distance_model != "inverse") {
			positional["distanceModel"] = distance_model;
		}
		if (max_distance > 0.0) {
			positional["maxDistance"] = max_distance;
		}
		if (unit_distance != 1.0) {
			positional["refDistance"] = unit_distance;
		}
		if (rolloff_factor != 1.0) {
			positional["rolloffFactor"] = rolloff_factor;
		}
		if (shape_type != "omnidirectional") {
			positional["shapeType"] = shape_type;
		}
		dict["positional"] = positional;
		dict["type"] = "positional";
	} else {
		dict["type"] = "global";
	}
	return dict;
}
